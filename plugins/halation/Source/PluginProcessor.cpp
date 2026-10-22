#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace dh::sp;

SpaceProcessor::SpaceProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "HALATION", params::layout())
{
    for (auto& a : view.lineMs) a.store (0.0f);
    for (auto& a : view.lineEnergy) a.store (0.0f);
    loadFactory();
    tracker.capture (*this);
    // Una instancia nueva es el programa 0 que informa al host (QA en REAPER 2026-10-22, X2: el VST3 decía "Tape
    // Echo 1974" pero sonaba con los valores por defecto, y elegir el programa 0 no cambiaba nada).
    if (! presets.empty()) loadPreset (0);
}

bool SpaceProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto in = l.getMainInputChannelSet(), out = l.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() && (in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono());
}

void SpaceProcessor::setValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void SpaceProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare ((float) sr, samplesPerBlock);
    engine.set (buildParams (120.0));
    setLatencySamples (0);
    nHeld = 0; envFollow = 0.0f; lastPpq = -1.0;
    const auto demoPath = juce::SystemStats::getEnvironmentVariable ("HALATION_DEMO_INPUT", {});
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

// Parámetros → motor. `bpm` para lo sincronizado. El morph A/B y la envolvente se aplican sobre los valores.
EngineParams SpaceProcessor::buildParams (double bpm) const
{
    auto v = [&] (const char* id) {
        float x = value (id);
        if (sceneBValid.load())
        {
            const float m = value ("morph") / 100.0f;
            if (m > 0.0f)
            {
                const juce::ScopedLock sl (sceneLock);
                auto it = sceneB.find (id);
                if (it != sceneB.end()) x += (it->second - x) * m;
            }
        }
        return x;
    };
    auto on = [&] (const char* m) { return value (juce::String ("on_") + m) > 0.5f; };
    auto beatsMs = [&] (int divIdx) { const auto& d = params::divisions()[(size_t) juce::jlimit (0, (int) params::divisions().size() - 1, divIdx)]; return (float) (d.beats * 60000.0 / std::max (20.0, bpm)); };

    EngineParams p;
    // Envolvente → un destino (−1..1 de profundidad).
    const float env = juce::jlimit (0.0f, 1.0f, envFollow) * value ("env_amount") / 100.0f;
    const int envT = (int) value ("env_target");
    auto withEnv = [&] (float x, float lo, float hi, int target) { return target == envT ? juce::jlimit (lo, hi, x + env * (hi - lo)) : x; };

    p.net.space = withEnv (v ("space") / 100.0f, 0.0f, 1.0f, 0);
    p.net.timeMs = juce::jlimit (1.0f, 8000.0f, value ("sync") > 0.5f ? beatsMs ((int) value ("division")) : v ("time_ms"));
    p.net.size = withEnv (v ("size") / 100.0f, 0.25f, 4.0f, 1);
    // Length → T60 (de 0.2 s a 60 s; al 100 % infinito). Tone → caída de los agudos; Bass → la de los graves.
    const float L = v ("length") / 100.0f;
    p.net.decayS = L >= 0.995f ? 1000.0f : 0.2f * std::pow (300.0f, L);
    const float tone = withEnv (v ("tone") / 100.0f, 0.0f, 1.0f, 3);
    p.net.hfRatio = 0.05f + 0.95f * tone * tone;
    p.net.lfRatio = v ("bass") / 100.0f;
    p.net.lowCutHz = v ("lowcut");
    p.net.room = (RoomType) juce::jlimit (0, 3, (int) value ("room"));
    p.net.diffusion = v ("diffusion") / 100.0f;
    p.net.modDepth = v ("mod_depth") / 100.0f;
    p.net.modRate = v ("mod_rate");
    p.net.width = v ("width") / 100.0f;
    p.net.pingPong = value ("pingpong") > 0.5f;
    p.net.pattern = (int) value ("pattern");
    p.net.character = (Character) juce::jlimit (0, 3, (int) value ("character"));
    p.net.wear = v ("wear") / 100.0f;
    p.net.elastic = value ("elastic") > 0.5f ? 1.0f : 0.0f;
    p.mix = withEnv (v ("mix") / 100.0f, 0.0f, 1.0f, 4);
    p.predelayMs = v ("predelay");
    p.outGainDb = v ("out_gain");
    // Módulos.
    if (on ("freeze"))
    {
        // All: la red entera se sostiene. Una banda: el espectral sostiene esos bins y deja pasar el resto.
        static const int masks[5] = { 7, 1, 2, 4, 3 };                 // All, Lows, Mids, Highs, Lows + mids
        const int mode = juce::jlimit (0, 4, (int) value ("freeze_mode"));
        if (mode == 0) p.net.freezeMask = 7; else p.spectralFreeze = masks[mode];
    }
    if (on ("shimmer")) { p.shimmer = withEnv (v ("shimmer") / 100.0f, 0.0f, 1.0f, 2); p.interval = (int) value ("interval"); }
    if (on ("reverse")) { p.reverse = v ("reverse") / 100.0f; p.reverseMs = std::min (4000.0f, beatsMs ((int) value ("reverse_div"))); }
    if (on ("grains")) { p.grains = v ("grains") / 100.0f; p.grainMs = v ("grain_ms"); }
    if (on ("spectral")) { p.spectral = 0.7f; p.smear = v ("spectral") / 100.0f; }
    if (on ("resonator"))
    {
        p.resonance = v ("resonance") / 100.0f; p.resFeedback = 0.9f + 0.09f * p.resonance;
        if (value ("res_midi") > 0.5f && nHeld > 0) { p.nNotes = std::min (4, nHeld); for (int k = 0; k < p.nNotes; ++k) p.notes[k] = (float) heldNotes[(size_t) (nHeld - p.nNotes + k)]; }
        else
        {
            static const int chordSteps[6][4] = { { 0, 0, 0, 0 }, { 0, 7, 0, 0 }, { 0, 4, 7, 0 }, { 0, 3, 7, 0 }, { 0, 5, 7, 0 }, { 0, 12, 24, 0 } };
            static const int chordN[6] = { 1, 2, 3, 3, 3, 3 };
            const int c = juce::jlimit (0, 5, (int) value ("chord"));
            p.nNotes = chordN[c];
            for (int k = 0; k < p.nNotes; ++k) p.notes[k] = std::round (value ("res_root")) + (float) chordSteps[c][k];
        }
    }
    if (on ("stutter")) { p.stutter = v ("stutter") / 100.0f; p.stutterMs = std::min (4000.0f, beatsMs ((int) value ("stutter_div"))); p.stutterRepeats = 4; }
    if (on ("breathe")) { p.impossible = (int) value ("breathe_mode") + 1; p.impDepth = v ("breathe_depth") / 100.0f; p.impRate = v ("breathe_rate"); }
    if (on ("duck")) p.duck = v ("duck") / 100.0f;
    if (on ("gate")) { p.gate = v ("gate") / 100.0f; p.gateHoldMs = v ("gate_hold"); }
    return p;
}

// La cola: infinita con Length al máximo o con Freeze; si no, el T60 más el pre-delay (y un poco de margen).
double SpaceProcessor::getTailLengthSeconds() const
{
    const auto p = buildParams (120.0);
    if (p.net.decayS >= 100.0f || p.net.freezeMask != 0) return std::numeric_limits<double>::infinity();
    return std::min (120.0, p.net.decayS * 1.1 + p.predelayMs * 1e-3 + (p.reverse > 0 ? p.reverseMs * 1e-3 : 0.0));
}

EngineParams SpaceProcessor::currentEngineParams() const { return buildParams (view.bpm.load()); }

void SpaceProcessor::storeSceneB()
{
    const juce::ScopedLock sl (sceneLock);
    sceneB.clear();
    for (auto& d : params::floats()) if (juce::String (d.id) != "morph" && juce::String (d.id) != "out_gain") sceneB[d.id] = value (d.id);
    sceneBValid.store (true);
}

void SpaceProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    if (n == 0) return;
    // MIDI: notas sostenidas para el resonador (las 8 últimas).
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        if (msg.isNoteOn())
        {
            bool already = false;                    // la misma nota dos veces: una sola entrada (si no, quedaba sonando)
            for (int k = 0; k < nHeld; ++k) already = already || heldNotes[(size_t) k] == msg.getNoteNumber();
            if (already) continue;
            if (nHeld == 8) { std::move (heldNotes.begin() + 1, heldNotes.end(), heldNotes.begin()); --nHeld; }
            heldNotes[(size_t) nHeld++] = msg.getNoteNumber();
        }
        else if (msg.isNoteOff())
        {
            for (int k = 0; k < nHeld; ++k) if (heldNotes[(size_t) k] == msg.getNoteNumber()) { std::move (heldNotes.begin() + k + 1, heldNotes.begin() + nHeld, heldNotes.begin() + k); --nHeld; break; }
        }
        else if (msg.isAllNotesOff()) nHeld = 0;
    }
    // Tempo y posición del host (para lo sincronizado y para que el stutter caiga en la grilla).
    double bpm = 120.0, ppq = -1.0; bool playing = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto q = pos->getPpqPosition()) ppq = *q;
            playing = pos->getIsPlaying();
        }
    view.bpm.store ((float) bpm);
    // Envolvente de la entrada (para la modulación).
    float pk = 0.0f;
    for (int c = 0; c < buffer.getNumChannels(); ++c) pk = std::max (pk, buffer.getMagnitude (c, 0, n));
    const float target = juce::jlimit (0.0f, 1.0f, (juce::Decibels::gainToDecibels (pk, -60.0f) + 48.0f) / 42.0f);
    // Seguidor por bloque con constantes de tiempo (15 ms de ataque, 130 ms de caída): igual con bloques de 64 o de
    // 1024 muestras (antes los coeficientes eran por bloque y una exportación sonaba distinta que la reproducción).
    {
        const float secs = (float) n / (float) sr;
        const float a = 1.0f - std::exp (-secs / (target > envFollow ? 0.015f : 0.13f));
        envFollow += (target - envFollow) * a;
    }
    view.inLevel.store (pk);

    const auto p = buildParams (bpm);
    engine.set (p);
    // Inicio de trozo del stutter en este bloque, según la grilla del host.
    int beatStart = -1;
    if (playing && ppq >= 0.0 && p.stutter > 0.0f)
    {
        const double divBeats = params::divisions()[(size_t) juce::jlimit (0, 15, (int) value ("stutter_div"))].beats;
        const double samplesPerBeat = sr * 60.0 / bpm;
        const double next = std::ceil (ppq / divBeats) * divBeats;
        const int at = (int) std::lround ((next - ppq) * samplesPerBeat);
        if (at >= 0 && at < n) beatStart = at;
    }
    if (buffer.getNumChannels() == 1 || getTotalNumInputChannels() == 1) buffer.copyFrom (1, 0, buffer, 0, 0, n);
    if (demo.getNumSamples() > 0)
        for (int i = 0; i < n; ++i, demoPos = (demoPos + 1) % demo.getNumSamples())
            for (int c = 0; c < 2; ++c) buffer.setSample (c, i, demo.getSample (c, demoPos));
    engine.process (buffer.getWritePointer (0), buffer.getWritePointer (1), n, beatStart);

    // Para la vista.
    const auto& net = engine.network();
    for (int i = 0; i < 8; ++i) { view.lineMs[i].store (net.lineMs (i)); view.lineEnergy[i].store (net.lineEnergy (i)); }
    view.freeze.store (net.freezeAmount()); view.limiterDb.store (net.limiterGrDb()); view.loopGain.store (net.loopGainMax());
    view.sizeNow.store (engine.sizeNow()); view.wetPeak.store (engine.wetPeakLevel());
    view.space.store (p.net.space); view.decayS.store (p.net.decayS); view.feedback.store (p.net.decayS >= 100.0f ? 1.0f : std::pow (10.0f, -3.0f * p.net.timeMs * 1e-3f / p.net.decayS)); view.length.store (value ("length") / 100.0f);
    view.shimmerLayers.store (engine.shimmerLayers()); view.grains.store (engine.grainsActive()); view.stutter.store (engine.stutterActive());
    {
        int lines = 0; const auto& pat = patterns()[(size_t) juce::jlimit (0, 7, p.net.pattern)];
        for (float g : pat.gain) lines += g > 0.0f ? 2 : 0;
        view.activeLines.store (p.net.space > 0.5f ? 8 : std::max (2, lines));
    }
    Onset o[8]; const int k = engine.takeOnsets (o, 8);
    if (k > 0)
    {
        int s1, n1, s2, n2; view.onsetFifo.prepareToWrite (k, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) view.onsets[(size_t) (s1 + i)] = o[i];
        for (int i = 0; i < n2; ++i) view.onsets[(size_t) (s2 + i)] = o[n1 + i];
        view.onsetFifo.finishedWrite (n1 + n2);
    }
}

// ── Presets ──────────────────────────────────────────────────────────────────────────────────────────────────
void SpaceProcessor::loadFactory()
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

int SpaceProcessor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void SpaceProcessor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String SpaceProcessor::getProgramName (int index) { return index >= 0 && index < (int) presets.size() ? presets[(size_t) index].name : juce::String ("Init"); }

void SpaceProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets.size()) return;
    const auto& pr = presets[(size_t) index];
    // Primero todo a su valor por defecto, después lo que el preset fija.
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (rp->getParameterID() != "mix" || pr.data.getProperty ("params", {}).hasProperty ("mix")) rp->setValueNotifyingHost (rp->getDefaultValue());
    if (auto* obj = pr.data.getProperty ("params", {}).getDynamicObject())
        for (auto& kv : obj->getProperties()) setValue (kv.name.toString(), (float) kv.value);
    sceneBValid.store (false);
    if (auto* b = pr.data.getProperty ("scene_b", {}).getDynamicObject())
    {
        storeSceneB();
        const juce::ScopedLock sl (sceneLock);
        for (auto& kv : b->getProperties()) sceneB[kv.name.toString()] = (float) kv.value;
    }
    currentProgram = index; currentPresetName = pr.name;
    tracker.capture (*this);
    triggerAsyncUpdate();
}

void SpaceProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPresetName.get(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetSnapshot", tracker.toString(), nullptr);
    juce::ValueTree b ("SCENE_B");
    {
        const juce::ScopedLock sl (sceneLock);
        b.setProperty ("valid", sceneBValid.load(), nullptr);
        for (auto& kv : sceneB) b.setProperty (juce::Identifier (kv.first), kv.second, nullptr);
    }
    state.removeChild (state.getChildWithName ("SCENE_B"), nullptr);
    state.appendChild (b, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void SpaceProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || state.getType() != apvts.state.getType()) return;
    const auto b = state.getChildWithName ("SCENE_B");
    apvts.replaceState (state);
    // El APVTS no reescribe un parámetro cuyo valor ya coincide con el del estado; un interruptor que quedó en un
    // valor crudo intermedio (0.49 = "apagado") se quedaría así. Se fuerzan a 0 o 1 exactos.
    for (auto* prm : getParameters())
        if (auto* pb = dynamic_cast<juce::AudioParameterBool*> (prm)) pb->setValueNotifyingHost (pb->get() ? 1.0f : 0.0f);
    currentPresetName = state.getProperty ("preset", "Init").toString();
    currentProgram = juce::jlimit (0, std::max (0, (int) presets.size() - 1), (int) state.getProperty ("program", 0));
    if (state.hasProperty ("presetSnapshot")) tracker.fromString (*this, state.getProperty ("presetSnapshot").toString());
    else tracker.capture (*this);
    triggerAsyncUpdate();
    if (b.isValid())
    {
        const juce::ScopedLock sl (sceneLock);
        sceneB.clear();
        for (int i = 0; i < b.getNumProperties(); ++i)
        {
            const auto nm = b.getPropertyName (i);
            if (nm.toString() != "valid") sceneB[nm.toString()] = (float) b.getProperty (nm);
        }
        sceneBValid.store ((bool) b.getProperty ("valid", false));
    }
}

juce::AudioProcessorEditor* SpaceProcessor::createEditor() { return new SpaceEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SpaceProcessor(); }
