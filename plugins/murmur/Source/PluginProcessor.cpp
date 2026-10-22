#include <set>
#include "PluginProcessor.h"
#if ! MURMUR_NO_EDITOR
 #include "PluginEditor.h"
#endif

using namespace dh::mm;

MurmurProcessor::MurmurProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Murmur", createLayout())
{
    for (auto& p : params()) raw.push_back (apvts.getRawParameterValue (p.id));
    vals.assign (raw.size(), 0.0f);
    loadFactory();
    rescanUserPresets();
    const auto demo = juce::SystemStats::getEnvironmentVariable ("MURMUR_DEMO_INPUT", {});
    if (demo.isNotEmpty())
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        if (std::unique_ptr<juce::AudioFormatReader> rd { fm.createReaderFor (juce::File (demo)) })
        {
            demoIn.setSize (2, (int) rd->lengthInSamples);
            rd->read (&demoIn, 0, (int) rd->lengthInSamples, 0, true, true);
        }
    }
    { juce::StringArray g; for (auto& e : params()) if (e.global) g.add (e.id); tracker.ignore (g); }   // ajustes de sesión: no "modifican" el preset
    // Una instancia nueva es el programa 0 que informa al host (QA en REAPER 2026-10-22, X2: el VST3 decía "First
    // Murmur" pero sonaba con los valores por defecto). Así la foto del preset tampoco queda vacía hasta reabrir (QA
    // en Live, H4: el estado guardado cambiaba sin que nadie tocara nada).
    if (! presets.empty()) loadPreset (0);
    else tracker.capture (*this);
}

juce::AudioProcessorValueTreeState::ParameterLayout MurmurProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (auto& e : params())
    {
        const juce::ParameterID pid { e.id, 1 };
        const auto* ep = &e;
        if (e.choices.isNotEmpty())
            layout.add (std::make_unique<juce::AudioParameterChoice> (pid, e.name, juce::StringArray::fromTokens (e.choices, "|", ""), (int) (e.def - e.lo)));
        else if (e.steps > 0)
            layout.add (std::make_unique<juce::AudioParameterInt> (pid, e.name, (int) e.lo, (int) e.hi, (int) e.def,
                juce::AudioParameterIntAttributes().withStringFromValueFunction ([ep] (int v, int) { return formatParam (*ep, (float) v); })));
        else
        {
            juce::NormalisableRange<float> range (e.lo, e.hi);
            if (e.skew != 1.0f) range.setSkewForCentre (e.lo + (e.hi - e.lo) * std::pow (0.5f, 1.0f / e.skew));
            layout.add (std::make_unique<juce::AudioParameterFloat> (pid, e.name, range, e.def,
                juce::AudioParameterFloatAttributes().withStringFromValueFunction ([ep] (float v, int) { return formatParam (*ep, v); })));
        }
    }
    return layout;
}

void MurmurProcessor::setValue (const juce::String& id, float v)
{
    if (auto* prm = apvts.getParameter (id)) prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

bool MurmurProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono()) return false;
    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

void MurmurProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    eng.prepare (sampleRate, std::max (64, samplesPerBlock));
    tmpL.assign ((size_t) std::max (64, samplesPerBlock) + 64, 0.0f); tmpR = tmpL;
}

void MurmurProcessor::handleMidi (const juce::MidiMessage& m)
{
    if (m.isNoteOn()) eng.noteOn (m.getNoteNumber(), m.getFloatVelocity());
    else if (m.isNoteOff()) eng.noteOff (m.getNoteNumber());
    else if (m.isController())
    {
        const int c = m.getControllerNumber();
        if (c == 64) eng.setFreezeHold (m.getControllerValue() >= 64);           // pedal: freeze mientras está apretado
        else if (c == 120 || c == 123) eng.allNotesOff();
        else if (c == 121) eng.setFreezeHold (false);
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff()) eng.allNotesOff();
}

void MurmurProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    if (n <= 0) return;
    wasBypassed = false;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    for (size_t k = 0; k < raw.size(); ++k) vals[k] = raw[k]->load();
    applyParams (vals.data(), eng);

    HostInfo host;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            host.playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) host.ppq = *q;
            if (auto b = pos->getBpm()) host.bpm = *b;
        }

    // Entrada mono → los dos canales; salida mono → promedio.
    const int ins = getTotalNumInputChannels(), outs = getTotalNumOutputChannels();
    if ((int) tmpL.size() < n) { tmpL.resize ((size_t) n); tmpR.resize ((size_t) n); }
    float* L = buffer.getWritePointer (0);
    float* R = outs > 1 && buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : tmpR.data();
    if (ins < 2) juce::FloatVectorOperations::copy (R, L, n);
    else if (R == tmpR.data()) juce::FloatVectorOperations::copy (R, buffer.getReadPointer (1), n);   // estéreo → mono: la R real
    if (memPending.exchange (false)) eng.setMemory (memData.data(), memCount, memRate);   // memoria de la sesión
    if (demoIn.getNumSamples() > 0)
        for (int i = 0; i < n; ++i, demoPos = (demoPos + 1) % demoIn.getNumSamples())
        { L[i] = demoIn.getSample (0, demoPos); R[i] = demoIn.getSample (1, demoPos); }
    float inPk = 0.0f; for (int i = 0; i < n; ++i) inPk = std::max (inPk, std::max (std::abs (L[i]), std::abs (R[i])));
    if (! midi.isEmpty()) midiFlash.store (6);

    auto it = midi.cbegin();
    int pos = 0;
    while (pos < n)
    {
        while (it != midi.cend() && (*it).samplePosition <= pos) { handleMidi ((*it).getMessage()); ++it; }
        int end = n;
        if (it != midi.cend()) end = std::min (end, std::max (pos + 1, (*it).samplePosition));
        HostInfo h = host; h.ppq = host.ppq + pos * host.bpm / 60.0 / sr;
        eng.process (L + pos, R + pos, L + pos, R + pos, end - pos, h);
        pos = end;
    }
    for (; it != midi.cend(); ++it) handleMidi ((*it).getMessage());
    midi.clear();
    if (outs < 2) for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);

    float outPk = 0.0f; for (int i = 0; i < n; ++i) outPk = std::max (outPk, std::abs (L[i]));
    inLevel.store (std::max (inPk, inLevel.load() * 0.9f)); outLevel.store (std::max (outPk, outLevel.load() * 0.9f));
    const double used = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    cpuLoad.store (cpuLoad.load() * 0.9f + (float) (used / (n / sr)) * 0.1f);
}

// En bypass la señal pasa seca (lo que hace JUCE por defecto); se sueltan las notas MIDI retenidas.
void MurmurProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    if (! wasBypassed) { eng.allNotesOff(); eng.setFreezeHold (false); wasBypassed = true; }
    for (int c = getTotalNumInputChannels(); c < buffer.getNumChannels(); ++c) buffer.clear (c, 0, buffer.getNumSamples());
    midi.clear();
}

// ───────────── presets ─────────────
int MurmurProcessor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void MurmurProcessor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String MurmurProcessor::getProgramName (int index)
{
    return juce::isPositiveAndBelow (index, (int) presets.size()) ? presets[(size_t) index].name : juce::String ("Init");
}

void MurmurProcessor::loadPreset (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) presets.size())) return;
    currentProgram = index;
    const auto& v = presets[(size_t) index].data;
    const auto* obj = v.getProperty ("params", {}).getDynamicObject();
    for (auto& e : params())
    {
        if (e.global) continue;
        setValue (e.id, obj != nullptr && obj->hasProperty (e.id) ? (float) obj->getProperty (e.id) : e.def);
    }
    currentPresetName = presets[(size_t) index].name;
    tracker.capture (*this);
    notifyUi (onPresetChanged);
}

void MurmurProcessor::loadFactory()
{
   #if MURMUR_HAS_ASSETS
    juce::StringArray names;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String nm (BinaryData::namedResourceList[i]);
        if (nm.endsWith ("_json") && nm.startsWith ("p") && nm.substring (1, 3).containsOnly ("0123456789")) names.add (nm);
    }
    names.sort (true);
    for (auto& nm : names)
    {
        int size = 0;
        const char* data = BinaryData::getNamedResource (nm.toRawUTF8(), size);
        const auto v = juce::JSON::parse (juce::String::fromUTF8 (data, size));
        presets.push_back ({ v.getProperty ("name", nm).toString(), v.getProperty ("category", "").toString(), v, true, {} });
    }
   #endif
}

juce::File MurmurProcessor::userPresetDir()
{
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/Audio Tools/Murmur/Presets");
   #else
    return base.getChildFile ("Audio Tools/Murmur/Presets");
   #endif
}

void MurmurProcessor::rescanUserPresets()
{
    presets.erase (std::remove_if (presets.begin(), presets.end(), [] (auto& p) { return ! p.factory; }), presets.end());
    const auto dir = userPresetDir();
    if (! dir.isDirectory()) return;
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();
    for (auto& f : files)
    {
        const auto v = juce::JSON::parse (f);
        if (v.isObject()) presets.push_back ({ v.getProperty ("name", f.getFileNameWithoutExtension()).toString(), "User", v, false, f });
    }
}

bool MurmurProcessor::saveUserPreset (const juce::String& name)
{
    const auto dir = userPresetDir();
    dir.createDirectory();
    auto* o = new juce::DynamicObject(); auto* prm = new juce::DynamicObject();
    o->setProperty ("name", name); o->setProperty ("category", "User");
    const auto& ps = params();
    for (size_t k = 0; k < ps.size(); ++k) if (! ps[k].global && std::abs (raw[k]->load() - ps[k].def) > 1e-4f) prm->setProperty (ps[k].id, raw[k]->load());
    o->setProperty ("params", juce::var (prm));
    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".json");
    const bool ok = file.replaceWithText (juce::JSON::toString (juce::var (o)));
    rescanUserPresets();
    currentPresetName = name;
    tracker.capture (*this);
    for (int i = 0; i < (int) presets.size(); ++i) if (! presets[(size_t) i].factory && presets[(size_t) i].file == file) currentProgram = i;
    notifyUi (onPresetChanged);
    return ok;
}

void MurmurProcessor::notifyUi (std::function<void()>& cb)
{
    if (! cb) return;
    if (juce::MessageManager::existsAndIsCurrentThread()) { cb(); return; }
    juce::MessageManager::callAsync ([safe = juce::WeakReference<MurmurProcessor> (this), &cb] { if (safe != nullptr && cb) cb(); });
}

void MurmurProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("presetName", currentPresetName.get(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    // Con Freeze, lo congelado viaja con la sesión (16 bits; hasta 16 s, unos 4 MB): al reabrir suena lo mismo.
    if (eng.frozenNow() && eng.memorySamples() > 0)
    {
        const int cnt = eng.memorySamples();
        std::vector<float> l ((size_t) cnt), r ((size_t) cnt);
        eng.copyMemory (l.data(), r.data(), cnt);
        juce::MemoryBlock mb ((size_t) cnt * 4);
        auto* d = static_cast<int16_t*> (mb.getData());
        for (int i = 0; i < cnt; ++i)
        {
            d[2 * i] = (int16_t) juce::jlimit (-32767, 32767, (int) std::lround (l[(size_t) i] * 32767.0f));
            d[2 * i + 1] = (int16_t) juce::jlimit (-32767, 32767, (int) std::lround (r[(size_t) i] * 32767.0f));
        }
        state.setProperty ("frozenAudio", mb.toBase64Encoding(), nullptr);
        state.setProperty ("frozenRate", sr, nullptr);
        state.setProperty ("frozenCount", cnt, nullptr);
    }
    state.setProperty ("presetSnapshot", tracker.toString(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void MurmurProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || ! state.hasType (apvts.state.getType())) return;
    currentPresetName = state.getProperty ("presetName", "Init").toString();
    currentProgram = state.getProperty ("program", 0);
    if (state.hasProperty ("frozenAudio"))
    {
        juce::MemoryBlock mb; mb.fromBase64Encoding (state.getProperty ("frozenAudio").toString());
        const int cnt = std::min ((int) state.getProperty ("frozenCount", 0), (int) (mb.getSize() / 4));
        if (cnt > 0 && ! memPending.load())
        {
            memData.assign (static_cast<const int16_t*> (mb.getData()), static_cast<const int16_t*> (mb.getData()) + 2 * cnt);
            memCount = cnt; memRate = (double) state.getProperty ("frozenRate", 48000.0);
            memPending.store (true);                       // el hilo de audio la carga al principio del bloque
        }
    }
    for (auto id : { "presetName", "program", "frozenAudio", "frozenRate", "frozenCount" }) state.removeProperty (id, nullptr);
    const auto snapshot = state.getProperty ("presetSnapshot").toString(); state.removeProperty ("presetSnapshot", nullptr);
    std::set<juce::String> present;
    for (const auto& c : state) if (c.hasProperty ("id")) present.insert (c.getProperty ("id").toString());
    apvts.replaceState (state);
    // Parámetros que faltan (sesiones viejas) o llegaron como NaN: a su valor por defecto.
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! dh::isFiniteF (rp->getValue()) || present.count (rp->paramID) == 0) rp->setValueNotifyingHost (rp->getDefaultValue());
    if (snapshot.isNotEmpty()) tracker.fromString (*this, snapshot);
    else tracker.capture (*this);
    notifyUi (onPresetChanged);
}

#if ! MURMUR_NO_EDITOR
juce::AudioProcessorEditor* MurmurProcessor::createEditor() { return new MurmurEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MurmurProcessor(); }
#else
juce::AudioProcessorEditor* MurmurProcessor::createEditor() { return nullptr; }
#endif
