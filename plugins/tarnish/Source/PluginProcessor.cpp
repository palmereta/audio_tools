#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace dh::tn;

TarnishProcessor::TarnishProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "TARNISH", params::layout())
{
    for (auto& a : view.scopeH) a.store (0.0f);
    for (auto& a : view.scopeM) a.store (0.0f);
    loadFactory();
}

bool TarnishProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void TarnishProcessor::setValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void TarnishProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sr, samplesPerBlock);
    engine.set (buildParams (bpmNow.load()), true);
    setLatencySamples (engine.latency());
    lastValues.clear(); counter = 0; heldNotes = 0; midiStop = transportStop = false; lastStop = false;
    const auto demoPath = juce::SystemStats::getEnvironmentVariable ("TARNISH_DEMO_INPUT", {});
    if (demoPath.isNotEmpty() && demo.getNumSamples() == 0)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        if (std::unique_ptr<juce::AudioFormatReader> r { fm.createReaderFor (juce::File (demoPath)) })
        {
            demo.setSize (2, (int) r->lengthInSamples);
            r->read (&demo, 0, (int) r->lengthInSamples, 0, true, true);
        }
        demoPos = 0;
    }
}

// Parámetros → motor. El tape stop sincronizado convierte compases en segundos con el tempo del host.
EngineParams TarnishProcessor::buildParams (double bpm) const
{
    EngineParams p;
    p.machine = juce::jlimit (0, 5, (int) value ("machine"));
    p.variant = (int) value (params::variantId (p.machine));
    p.nab = (int) value ("eq") == 1;
    p.driveDb = value ("drive"); p.saturation = value ("saturation") / 100.0; p.bias = value ("bias") / 100.0;
    p.wowPct = value ("wow"); p.flutterPct = value ("flutter"); p.stereoWobble = value ("wobble") > 0.5f;
    p.scrape = value ("scrape") / 100.0; p.wear = value ("wear") / 100.0; p.dropouts = value ("dropouts") / 100.0;
    p.noise = value ("noise") / 100.0; p.crackle = value ("crackle") / 100.0; p.scratches = (int) value ("scratches");
    p.hum = value ("hum") / 100.0;
    p.lowCut = value ("lowcut"); p.highCut = value ("highcut");
    p.azimuth = value ("azimuth") / 100.0;
    const float pr = value ("print");
    p.printDb = pr > 0.5f ? -70.0 + 30.0 * pr / 100.0 : -200.0;
    p.nr = (int) value ("nr"); p.mistrackDb = value ("mistrack");
    p.bits = value ("bits"); p.rateHz = value ("rate") >= 47999.0f ? 1e9 : value ("rate");
    p.crushRaw = (int) value ("crush_mode") == 1; p.jitter = value ("jitter") / 100.0; p.dither = value ("dither") > 0.5f;
    p.varispeed = value ("varispeed");
    const double beats = params::stopSyncBeats ((int) value ("stop_sync"));
    p.stopS = beats > 0 ? beats * 60.0 / std::max (20.0, bpm) : value ("stop_time");
    p.startS = value ("start_time"); p.stopCurve = value ("stop_curve") / 100.0;
    p.groove = value ("groove") / 100.0; p.grooveFollow = value ("groove_follow") > 0.5f;
    p.compress = value ("compress") / 100.0; p.fading = value ("fading") / 100.0; p.tuning = value ("tuning") / 100.0;
    p.whistle = value ("whistle") / 100.0; p.detector = value ("detector") / 100.0;
    p.frameLoss = value ("frameloss") / 100.0; p.codec = value ("codec") / 100.0;
    p.autoGain = value ("auto_gain") > 0.5f; p.outDb = value ("out"); p.mix = value ("mix") / 100.0;
    p.eco = (int) value ("quality") == 1;
    p.stop = value ("stop") > 0.5f;
    return p;
}

bool TarnishProcessor::paramsChanged()
{
    const auto& ps = getParameters();
    bool changed = lastValues.size() != (size_t) ps.size();
    lastValues.resize ((size_t) ps.size());
    for (int i = 0; i < ps.size(); ++i)
    {
        const float v = ps[i]->getValue();
        if (! juce::exactlyEqual (v, lastValues[(size_t) i])) { lastValues[(size_t) i] = v; changed = true; }
    }
    return changed;
}

void TarnishProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    double bpm = 120.0; bool playing = true; int64_t hostPos = -1;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto t = pos->getTimeInSamples()) hostPos = *t;
            playing = pos->getIsPlaying();
        }
    bpmNow.store (bpm);
    // Notas MIDI: mientras haya una sostenida, la cinta está parada.
    if (value ("stop_midi") > 0.5f)
        for (const auto m : midi)
        {
            const auto msg = m.getMessage();
            if (msg.isNoteOn()) ++heldNotes;
            else if (msg.isNoteOff()) heldNotes = std::max (0, heldNotes - 1);
            else if (msg.isAllNotesOff() || msg.isAllSoundOff()) heldNotes = 0;
        }
    midiStop = heldNotes > 0;
    // Con el transporte del host: al parar la reproducción, la cinta frena (en los hosts que siguen llamando al
    // proceso con el transporte detenido).
    transportStop = value ("stop_transport") > 0.5f && ! playing;

    const bool stopNow = value ("stop") > 0.5f || midiStop || transportStop;
    if (paramsChanged()) { auto p = buildParams (bpm); p.stop = stopNow; engine.set (p); lastStop = stopNow; }
    else if (stopNow != lastStop) { engine.setStop (stopNow); lastStop = stopNow; }

    if (buffer.getNumChannels() == 1 || getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    if (demo.getNumSamples() > 0)
        for (int i = 0; i < n; ++i, demoPos = (demoPos + 1) % demo.getNumSamples())
            for (int c = 0; c < 2; ++c) buffer.setSample (c, i, demo.getSample (c, demoPos));
    float inPk = 0.0f;
    for (int c = 0; c < 2; ++c) inPk = std::max (inPk, buffer.getMagnitude (c, 0, n));

    // Posición: la del host si está atado a la línea de tiempo y el host la da; si no, un contador propio.
    const bool lock = value ("timeline") > 0.5f && hostPos >= 0;
    const int64_t pos0 = lock ? hostPos : counter;
    // De a 32 muestras para tomar el osciloscopio B-H a lo largo del bloque.
    for (int i = 0; i < n; i += 32)
    {
        const int k = std::min (32, n - i);
        engine.process (buffer.getWritePointer (0) + i, buffer.getWritePointer (1) + i, k, pos0 + i);
        const int w = view.scopeWrite.load();
        view.scopeH[(size_t) w].store ((float) engine.scopeH (0)); view.scopeM[(size_t) w].store ((float) engine.scopeM (0));
        view.scopeWrite.store ((w + 1) % ViewState::kScope);
    }
    counter = pos0 + n;

    // Para la vista: lo que se oye ahora es la posición menos la latencia.
    const int64_t heard = pos0 + n - engine.latency();
    view.pos.store ((double) (pos0 + n));                   // índice de salida: las caídas se oyen en su posición
    view.vu.store ((float) engine.vuLevel()); view.speed.store ((float) engine.speed()); view.wf.store ((float) engine.flutterReading());
    view.spacing.store ((float) engine.spacingNow()); view.machine.store ((int) value ("machine")); view.stopped.store (engine.stopped());
    double rs, rt, ns, nt; engine.reels (heard, rs, rt, ns, nt);
    view.reelS.store ((float) rs); view.reelT.store ((float) rt); view.turnsS.store ((float) std::fmod (ns, 1.0)); view.turnsT.store ((float) std::fmod (nt, 1.0));
    view.groove.store ((float) engine.groovePos (heard)); view.rotation.store ((float) std::fmod (engine.rotation (heard), 1.0));
    view.inPeak.store (inPk);
    float outPk = 0.0f; for (int c = 0; c < 2; ++c) outPk = std::max (outPk, buffer.getMagnitude (c, 0, n));
    view.outPeak.store (outPk);
    UiEvent ev[32]; const int ne = engine.takeEvents (ev, 32);
    if (ne > 0)
    {
        int s1, n1, s2, n2; view.eventFifo.prepareToWrite (ne, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) view.events[(size_t) (s1 + i)] = ev[i];
        for (int i = 0; i < n2; ++i) view.events[(size_t) (s2 + i)] = ev[n1 + i];
        view.eventFifo.finishedWrite (n1 + n2);
    }
}

// ── Presets ──────────────────────────────────────────────────────────────────────────────────────────────────
void TarnishProcessor::loadFactory()
{
    presets.clear();
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String name (BinaryData::namedResourceList[i]);
        if (! name.endsWith ("_json")) continue;
        int size = 0;
        const char* data = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        auto v = juce::JSON::parse (juce::String::fromUTF8 (data, size));
        if (! v.isObject()) continue;
        presets.push_back ({ v.getProperty ("name", name).toString(), v.getProperty ("category", "").toString(), v.getProperty ("description", "").toString(), v });
    }
    std::sort (presets.begin(), presets.end(), [] (const PresetEntry& a, const PresetEntry& b) { return a.data.getProperty ("order", 999).operator int() < b.data.getProperty ("order", 999).operator int(); });
}

int TarnishProcessor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void TarnishProcessor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String TarnishProcessor::getProgramName (int index) { return index >= 0 && index < (int) presets.size() ? presets[(size_t) index].name : juce::String ("Init"); }

void TarnishProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets.size()) return;
    const auto& pr = presets[(size_t) index];
    // Primero todo a su valor por defecto (menos Mix y el stop, si el preset no los nombra), después lo del preset.
    const auto pv = pr.data.getProperty ("params", {});
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto id = rp->getParameterID();
            if ((id == "mix" || id == "stop") && ! pv.hasProperty (id)) continue;
            rp->setValueNotifyingHost (rp->getDefaultValue());
        }
    if (auto* obj = pv.getDynamicObject())
        for (auto& kv : obj->getProperties()) setValue (kv.name.toString(), (float) kv.value);
    currentProgram = index; currentPresetName = pr.name;
    if (onPresetChanged) juce::MessageManager::callAsync ([this] { if (onPresetChanged) onPresetChanged(); });
}

void TarnishProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPresetName, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void TarnishProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || state.getType() != apvts.state.getType()) return;
    apvts.replaceState (state);
    // El APVTS no reescribe un parámetro cuyo valor ya coincide con el del estado; un interruptor que quedó en un
    // valor crudo intermedio se quedaría así. Se fuerzan a 0 o 1 exactos (pluginval lo prueba).
    for (auto* prm : getParameters())
        if (auto* pb = dynamic_cast<juce::AudioParameterBool*> (prm)) pb->setValueNotifyingHost (pb->get() ? 1.0f : 0.0f);
    currentPresetName = state.getProperty ("preset", "Init").toString();
}

juce::AudioProcessorEditor* TarnishProcessor::createEditor() { return new TarnishEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TarnishProcessor(); }
