#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace dh::vp;

At330Processor::At330Processor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Voice In (Vocoder)", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AT330", params::layout())
{
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm)) rawIdStore.push_back (rp->getParameterID());
    for (auto& id : rawIdStore) rawIds.push_back ({ id.toRawUTF8(), apvts.getRawParameterValue (id) });
    loadFactory();
    // Una instancia nueva es el programa 0 que informa al host (QA en DAWs 2026-10-22, X2). Una sesión guardada lo
    // pisa todo en setStateInformation.
    if (! presets.empty()) loadPreset (0);
    else tracker.capture (*this);
}

bool At330Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    const auto in = l.getMainInputChannelSet();
    return in.isDisabled() || in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void At330Processor::setValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void At330Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    maxBlock = std::max (samplesPerBlock, 64);
    micBuf.assign ((size_t) std::max (maxBlock, 4096), 0.0f);
    carBuf.assign ((size_t) std::max (maxBlock, 4096), 0.0f);
    hostGap.prepare (sr);
    engine.prepare (sr, maxBlock);
    engine.set (buildParams());
    keyboardState.reset();
}

Params At330Processor::buildParams() const
{
    Params p;
    auto on = [this] (const char* id) { return value (id) > 0.5f; };
    auto db = [] (float x) { return std::pow (10.0f, x / 20.0f); };
    p.strUp = on ("str_upper"); p.strLo = on ("str_lower");
    p.chFemUp = on ("ch_female_up"); p.chM8Up = on ("ch_male8_up"); p.chM4Lo = on ("ch_male4_lo"); p.chM8Lo = on ("ch_male8_lo");
    p.vocUp = on ("voc_upper"); p.vocLo = on ("voc_lower");
    p.chEns = on ("ch_ens"); p.vocEns = on ("voc_ens"); p.strEns = on ("str_ens");
    p.strAttack = value ("str_attack"); p.strTone = value ("str_tone") / 100.0f; p.strLevel = value ("str_level") / 100.0f;
    p.chAttack = value ("ch_attack"); p.chLevel = value ("ch_level") / 100.0f;
    p.release = value ("release");
    p.micGain = db (value ("mic_level")); p.vocLevel = value ("voc_level") / 100.0f; p.vocTone = value ("voc_tone") / 100.0f;
    p.directMic = value ("direct_mic") / 100.0f; p.consonants = value ("consonants") / 100.0f;
    p.vocResponse = value ("voc_response") / 1000.0f; p.formantShift = value ("formant_shift") / 12.0f; p.extGain = db (value ("ext_level"));
    p.carrier = (int) value ("carrier"); p.hold = (int) value ("hold");
    p.vibRate = value ("vib_rate"); p.vibDepth = value ("vib_depth") / 100.0f; p.vibDelay = value ("vib_delay"); p.modVib = value ("mod_vib") / 100.0f;
    p.pitchMode = (int) value ("pitch_mode"); p.pitchSet = value ("pitch_set"); p.pitchTime = value ("pitch_time");
    p.pitchAmount = value ("pitch_amount") / 100.0f; p.bendRange = value ("bend_range");
    p.ensDepth = value ("ens_depth") / 100.0f; p.velocity = value ("velocity") / 100.0f;
    p.keying = (int) value ("keying"); p.split = 36 + (int) value ("split");
    p.tuneCents = value ("tune"); p.spaceMix = value ("space_mix") / 100.0f; p.spaceSize = value ("space_size") / 100.0f;
    p.volume = db (value ("volume"));
    return p;
}

void At330Processor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) engine.noteOn (m.getNoteNumber(), m.getVelocity());
    else if (m.isNoteOff()) engine.noteOff (m.getNoteNumber());
    else if (m.isAllSoundOff()) engine.allSoundOff();
    else if (m.isAllNotesOff() || m.isResetAllControllers()) engine.allNotesOff();
    else if (m.isSustainPedalOn()) engine.setSustain (true);
    else if (m.isSustainPedalOff()) engine.setSustain (false);
    else if (m.isPitchWheel()) engine.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
    else if (m.isControllerOfType (1)) engine.setModWheel ((float) m.getControllerValue() / 127.0f);
}

void At330Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    // La voz (y el portador externo) antes de limpiar el buffer: la entrada y la salida comparten canales.
    if ((int) micBuf.size() < n) { micBuf.resize ((size_t) n); carBuf.resize ((size_t) n); }      // host que no avisó el tamaño
    const auto p = buildParams();
    const bool extCarrier = p.carrier != kCarrierKeys;
    bool hasMic = false, hasCar = false;
    if (getBus (true, 0) != nullptr && getBus (true, 0)->isEnabled())
    {
        auto in = getBusBuffer (buffer, true, 0);
        const int ch = in.getNumChannels();
        if (ch >= 2 && extCarrier)
        {
            std::copy (in.getReadPointer (0), in.getReadPointer (0) + n, micBuf.begin());
            std::copy (in.getReadPointer (1), in.getReadPointer (1) + n, carBuf.begin());
            hasMic = hasCar = true;
        }
        else if (ch >= 2)
        {
            const float* a = in.getReadPointer (0); const float* b = in.getReadPointer (1);
            for (int i = 0; i < n; ++i) micBuf[(size_t) i] = 0.5f * (a[i] + b[i]);
            hasMic = true;
        }
        else if (ch == 1) { std::copy (in.getReadPointer (0), in.getReadPointer (0) + n, micBuf.begin()); hasMic = true; }
    }
    buffer.clear();
    // El host dejó de llamar un rato (REAPER no llama a un plugin en bypass) y los note-off de ese rato se perdieron:
    // se sueltan las notas antes del MIDI de este bloque (QA en DAWs 2026-10-22, X1).
    if (hostGap.check (getPlayHead(), n, isNonRealtime())) { engine.allNotesOff(); keyboardState.reset(); }
    engine.set (p);
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    float* L = buffer.getWritePointer (0); float* R = buffer.getWritePointer (1);
    const float* mic = hasMic ? micBuf.data() : nullptr; const float* car = hasCar ? carBuf.data() : nullptr;
    int pos = 0;
    for (const auto m : midi)
    {
        const int at = juce::jlimit (0, n, m.samplePosition);
        if (at > pos) { engine.process (L + pos, R + pos, mic ? mic + pos : nullptr, car ? car + pos : nullptr, at - pos); pos = at; }
        handleMidi (m.getMessage());
    }
    if (pos < n) engine.process (L + pos, R + pos, mic ? mic + pos : nullptr, car ? car + pos : nullptr, n - pos);
    // Vista.
    view.mic.store (std::max (view.mic.load(), engine.takeMicPeak()));
    view.out.store (std::max (view.out.load(), buffer.getMagnitude (0, 0, n)));
    for (int b = 0; b < kBands; ++b) view.bands[(size_t) b].store (engine.bandEnv (b));
    view.keys.store (engine.activeKeys());
}

void At330Processor::processBlockBypassed (juce::AudioBuffer<float>& b, juce::MidiBuffer& m)
{
    // El motor sigue recibiendo el MIDI (si no, un note-off que llega en bypass deja la nota colgada al volver).
    processBlock (b, m);
    b.clear();
}

// ── Estado: los parámetros, el preset y su foto ──
void At330Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPresetName.get(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetSnapshot", tracker.toString(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void At330Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || state.getType() != apvts.state.getType()) return;
    apvts.replaceState (state);
    for (auto* prm : getParameters())
        if (auto* pb = dynamic_cast<juce::AudioParameterBool*> (prm)) pb->setValueNotifyingHost (pb->get() ? 1.0f : 0.0f);
    currentPresetName = state.getProperty ("preset", "Init").toString();
    currentProgram = juce::jlimit (0, std::max (0, (int) presets.size() - 1), (int) state.getProperty ("program", 0));
    if (state.hasProperty ("presetSnapshot")) tracker.fromString (*this, state.getProperty ("presetSnapshot").toString());
    else tracker.capture (*this);
    presetDirty = true; triggerAsyncUpdate();
}

// ── Presets de fábrica (assets/presets/*.json, generados por tools/presets.py) ──
void At330Processor::loadFactory()
{
    presets.clear();
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String name (BinaryData::namedResourceList[i]);
        if (! name.endsWith ("_json")) continue;
        int size = 0;
        const char* d = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        auto v = juce::JSON::parse (juce::String::fromUTF8 (d, size));
        if (v.isObject() && v.hasProperty ("params")) presets.push_back ({ v.getProperty ("name", name).toString(), v.getProperty ("category", "").toString(), v.getProperty ("description", "").toString(), v });
    }
    std::sort (presets.begin(), presets.end(), [] (const PresetEntry& a, const PresetEntry& b) { return (int) a.data.getProperty ("order", 999) < (int) b.data.getProperty ("order", 999); });
}

int At330Processor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void At330Processor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String At330Processor::getProgramName (int index) { return index >= 0 && index < (int) presets.size() ? presets[(size_t) index].name : juce::String ("Init"); }

void At330Processor::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets.size()) return;
    const auto& pr = presets[(size_t) index];
    const auto pv = pr.data.getProperty ("params", {});
    // Todo a su valor por defecto salvo el punto de división del teclado (de la sesión); después lo del preset.
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto id = rp->getParameterID();
            if (id == "split" && ! pv.hasProperty (id)) continue;
            rp->setValueNotifyingHost (rp->getDefaultValue());
        }
    if (auto* obj = pv.getDynamicObject())
        for (auto& kv : obj->getProperties()) setValue (kv.name.toString(), (float) kv.value);
    currentProgram = index; currentPresetName = pr.name;
    tracker.capture (*this);
    presetDirty = true; triggerAsyncUpdate();
}

juce::AudioProcessorEditor* At330Processor::createEditor() { return new At330Editor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new At330Processor(); }
