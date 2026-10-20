#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace dh::pl;

PlumbProcessor::PlumbProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true).withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PLUMB", params::layout())
{
    loadFactory();
    tracker.capture (*this);
    for (int i = 0; i < 12; ++i) degPtr[(size_t) i] = apvts.getRawParameterValue ("deg" + juce::String (i));
}

bool PlumbProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo()) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void PlumbProcessor::setValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void PlumbProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    lastMode = juce::jlimit (0, 2, (int) value ("mode"));
    engine.prepare (sr, (Mode) lastMode);
    engine.set (buildParams());
    mono.assign ((size_t) std::max (1, samplesPerBlock), 0.0f);
    setLatencySamples (engine.latencySamples());
    const auto demoPath = juce::SystemStats::getEnvironmentVariable ("PLUMB_DEMO_INPUT", {});
    if (demoPath.isNotEmpty() && demo.getNumSamples() == 0)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        if (std::unique_ptr<juce::AudioFormatReader> r { fm.createReaderFor (juce::File (demoPath)) })
        {
            // A la tasa del dispositivo (lineal: es sólo para capturas).
            juce::AudioBuffer<float> raw (1, (int) r->lengthInSamples);
            r->read (&raw, 0, (int) r->lengthInSamples, 0, true, false);
            const double ratio = r->sampleRate / sr;
            const int len = (int) ((double) raw.getNumSamples() / ratio);
            demo.setSize (1, std::max (1, len));
            for (int i = 0; i < len; ++i)
            {
                const double pos = i * ratio; const int i0 = (int) pos; const float f = (float) (pos - i0);
                const float a = raw.getSample (0, i0), b = raw.getSample (0, std::min (i0 + 1, raw.getNumSamples() - 1));
                demo.setSample (0, i, a + (b - a) * f);
            }
        }
        demoPos = 0;
    }
}

uint16_t PlumbProcessor::allowedMask() const
{
    const int key = juce::jlimit (0, 11, (int) value ("key")), sc = juce::jlimit (0, params::customScale(), (int) value ("scale"));
    uint16_t rel;
    if (sc == params::customScale())
    {
        // La escala propia está en clases de altura absolutas (C = 0): no se rota con la tonalidad.
        uint16_t m = 0;
        for (int i = 0; i < 12; ++i) if ((int) degPtr[(size_t) i]->load() == 0) m |= (uint16_t) (1u << i);
        return m ? m : 0x0FFF;
    }
    rel = params::scales()[(size_t) sc].mask;
    uint16_t m = 0;
    for (int i = 0; i < 12; ++i) if ((rel >> i) & 1) m |= (uint16_t) (1u << ((i + key) % 12));
    return m;
}

uint16_t PlumbProcessor::bypassMask() const
{
    if ((int) value ("scale") != params::customScale()) return 0;
    uint16_t m = 0;
    for (int i = 0; i < 12; ++i) if ((int) degPtr[(size_t) i]->load() == 2) m |= (uint16_t) (1u << i);
    return m;
}

EngineParams PlumbProcessor::buildParams() const
{
    EngineParams p; CorrectParams& c = p.correct;
    c.allowed = allowedMask(); c.bypass = bypassMask();
    c.amount = value ("amount") / 100.0f;
    c.speedMs = value ("speed");
    const float style = value ("style") / 100.0f;
    c.direct = style >= 0.995f;
    if (value ("custom_character") > 0.5f)
    {
        c.transitionMs = value ("transition"); c.humanize = value ("humanize") / 100.0f;
        c.flexIn = value ("flex"); c.flexOut = std::min (600.0f, c.flexIn * 2.0f);
        if (c.flexIn >= 99.5f) c.flexIn = c.flexOut = 600.0f;            // al tope: siempre
        c.hysteresis = value ("hysteresis") / 100.0f;
        c.keepVibrato = value ("keep_vibrato") / 100.0f; c.keepDrift = value ("keep_drift") / 100.0f;
    }
    else
    {
        const auto ch = params::characterFor (style);
        c.transitionMs = ch.transitionMs; c.humanize = ch.humanize; c.flexIn = ch.flexIn; c.flexOut = ch.flexOut;
        c.hysteresis = ch.hysteresis; c.keepVibrato = ch.keepV; c.keepDrift = ch.keepD;
    }
    c.transpose = value ("transpose") + value ("fine") / 100.0f;
    c.vibRate = value ("vib_rate"); c.vibDepth = value ("vib_depth"); c.vibDelayMs = value ("vib_delay");
    c.midiMode = juce::jlimit (0, 3, (int) value ("midi_mode"));
    c.bendRange = value ("bend_range");
    p.voice = juce::jlimit (0, 3, (int) value ("voice"));
    p.refA = value ("ref_a"); p.gateDb = value ("gate");
    p.formants = value ("formants") > 0.5f; p.formantAmount = value ("formant_amount") / 100.0f; p.throat = value ("throat");
    p.mix = compare.load() ? 0.0f : value ("mix") / 100.0f; p.outGainDb = value ("output");
    return p;
}

void PlumbProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples(), inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    if (n == 0) return;
    if ((int) mono.size() < n) mono.resize ((size_t) n);                   // sólo si el host pasa un bloque mayor al anunciado
    // Modo: cambia en marcha; la latencia nueva se informa desde el hilo de mensajes.
    const int m = juce::jlimit (0, 2, (int) value ("mode"));
    if (m != lastMode) { lastMode = m; engine.setMode ((Mode) m); triggerAsyncUpdate(); }
    engine.set (buildParams());
    if (listenRequested.exchange (false)) engine.resetKeys();
    // Mono: la suma de los canales de entrada (o la entrada de prueba en loop).
    float inPk = 0;
    for (int i = 0; i < n; ++i)
    {
        float s = 0; for (int c = 0; c < inCh; ++c) s += buffer.getReadPointer (c)[i];
        mono[(size_t) i] = inCh > 0 ? s / (float) inCh : 0.0f;
        if (demo.getNumSamples() > 0) { mono[(size_t) i] = demo.getSample (0, demoPos); demoPos = (demoPos + 1) % demo.getNumSamples(); }
        inPk = std::max (inPk, std::abs (mono[(size_t) i]));
    }
    // MIDI en su muestra.
    int pos = 0; float outPk = 0;
    auto run = [&] (int to) { for (; pos < to; ++pos) { const float y = engine.process (mono[(size_t) pos]); mono[(size_t) pos] = y; outPk = std::max (outPk, std::abs (y)); } };
    for (const auto meta : midi)
    {
        run (juce::jlimit (0, n, meta.samplePosition));
        const auto msg = meta.getMessage();
        if (msg.isNoteOn()) engine.noteOn (msg.getNoteNumber());
        else if (msg.isNoteOff()) engine.noteOff (msg.getNoteNumber());
        else if (msg.isPitchWheel()) engine.pitchBend ((float) (msg.getPitchWheelValue() - 8192) / 8192.0f);
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) for (int k = 0; k < 128; ++k) engine.noteOff (k);
    }
    run (n);
    for (int c = 0; c < outCh; ++c) buffer.copyFrom (c, 0, mono.data(), n);
    samplesDone += n;
    inLevel.store (std::max (inPk, inLevel.load() * 0.8f)); outLevel.store (std::max (outPk, outLevel.load() * 0.8f));
    if (listening.load())
    {
        int t; bool mi; float cf; engine.keyEstimate (t, mi, cf);
        sugTonic.store (t); sugMinor.store (mi); sugConf.store (cf);
    }
}

// En bypass el motor sigue escuchando (la vista y las notas MIDI no se cortan) y la salida es la entrada demorada
// por la misma latencia: el host compensa igual con o sin bypass.
void PlumbProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    auto p = buildParams(); p.mix = 0.0f; engine.set (p);
    const int n = buffer.getNumSamples(), inCh = getTotalNumInputChannels(), outCh = getTotalNumOutputChannels();
    if ((int) mono.size() < n) mono.resize ((size_t) n);
    for (int i = 0; i < n; ++i) { float s = 0; for (int c = 0; c < inCh; ++c) s += buffer.getReadPointer (c)[i]; mono[(size_t) i] = inCh > 0 ? s / (float) inCh : 0.0f; }
    for (const auto meta : midi) { const auto msg = meta.getMessage(); if (msg.isNoteOn()) engine.noteOn (msg.getNoteNumber()); else if (msg.isNoteOff()) engine.noteOff (msg.getNoteNumber()); }
    for (int i = 0; i < n; ++i) mono[(size_t) i] = engine.process (mono[(size_t) i]);
    for (int c = 0; c < outCh; ++c) buffer.copyFrom (c, 0, mono.data(), n);
    samplesDone += n;
}

// Hilo de mensajes: la latencia del modo elegido (la del parámetro, que es la que el audio adopta en el bloque
// siguiente) y el aviso a la interfaz de un preset o estado nuevo.
void PlumbProcessor::handleAsyncUpdate()
{
    setLatencySamples (engine.latencyForMode ((Mode) juce::jlimit (0, 2, (int) value ("mode"))));
    if (presetDirty.exchange (false) && onPresetChanged) onPresetChanged();
}

// ── Estado ──
void PlumbProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPresetName.get(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetSnapshot", tracker.toString(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void PlumbProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || state.getType() != apvts.state.getType()) return;
    apvts.replaceState (state);
    // replaceState no avisa a los parámetros booleanos que no cambiaron de forma; se reafirman (como en AT303).
    for (auto* prm : getParameters())
        if (auto* pb = dynamic_cast<juce::AudioParameterBool*> (prm)) pb->setValueNotifyingHost (pb->get() ? 1.0f : 0.0f);
    currentPresetName = state.getProperty ("preset", "Init").toString();
    currentProgram = juce::jlimit (0, std::max (0, (int) presets.size() - 1), (int) state.getProperty ("program", 0));
    if (state.hasProperty ("presetSnapshot")) tracker.fromString (*this, state.getProperty ("presetSnapshot").toString());
    else tracker.capture (*this);
    // La latencia del modo restaurado se informa ya: el host la lee al cargar la sesión (Logic restaura el estado
    // después de prepareToPlay) y una exportación que empiece antes del primer bloque compensaría mal.
    setLatencySamples (engine.latencyForMode ((Mode) juce::jlimit (0, 2, (int) value ("mode"))));
    presetDirty = true; triggerAsyncUpdate();
}

// ── Presets: el carácter de la corrección; nunca tocan la tonalidad ni la escala ──
void PlumbProcessor::loadFactory()
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

int PlumbProcessor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void PlumbProcessor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String PlumbProcessor::getProgramName (int index) { return index >= 0 && index < (int) presets.size() ? presets[(size_t) index].name : juce::String ("Init"); }

void PlumbProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets.size()) return;
    const auto& pr = presets[(size_t) index];
    const auto pv = pr.data.getProperty ("params", {});
    static const juce::StringArray keep { "key", "scale", "mode", "ref_a", "gate", "midi_mode", "voice", "mix", "output",
                                          "deg0", "deg1", "deg2", "deg3", "deg4", "deg5", "deg6", "deg7", "deg8", "deg9", "deg10", "deg11" };
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto id = rp->getParameterID();
            if (keep.contains (id) && ! pv.hasProperty (id)) continue;
            rp->setValueNotifyingHost (rp->getDefaultValue());
        }
    if (auto* obj = pv.getDynamicObject())
        for (auto& kv : obj->getProperties()) setValue (kv.name.toString(), (float) kv.value);
    currentProgram = index; currentPresetName = pr.name;
    tracker.capture (*this);
    presetDirty = true; triggerAsyncUpdate();
}

juce::AudioProcessorEditor* PlumbProcessor::createEditor() { return new PlumbEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PlumbProcessor(); }
