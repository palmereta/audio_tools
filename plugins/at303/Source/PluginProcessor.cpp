#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace dh::tb;

At303Processor::At303Processor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "AT303", params::layout())
{
    loadFactory();
    // Banco inicial: el patrón 1 con una línea simple; el resto vacío (una nota por paso en C).
    for (auto& p : bank) p = Pattern();
}

bool At303Processor::isBusesLayoutSupported (const BusesLayout& l) const { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }

void At303Processor::setValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void At303Processor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sr, samplesPerBlock);
    engine.set (buildParams());
    patDirty = true; lastPatIdx = -1; freeClock = 0;
}

EngineParams At303Processor::buildParams() const
{
    EngineParams p; VoiceParams& v = p.voice;
    v.tuneSemis = value ("tuning");
    v.cutoff = value ("cutoff") / 100.0f; v.resonance = value ("resonance") / 100.0f; v.envMod = value ("envmod") / 100.0f;
    v.decay = value ("decay") / 100.0f; v.accent = value ("accent") / 100.0f;
    v.square = (int) value ("waveform") == 1;
    const float od = value ("overdrive") / 100.0f;
    v.overdrive = 1.0f + 65.6f * od * od;
    v.normalDecayMs = value ("normal_decay") > 1.0f ? value ("normal_decay") : -1.0f;
    v.accentDecayMs = value ("accent_decay");
    v.softAttackMs = value ("soft_attack");
    v.noteDelay = value ("note_delay") > 0.5f;
    v.slideTauMs = value ("slide_time") / 2.7f;
    static const float speeds[] = { 0.33f, 1.0f, 2.2f };
    v.sweepSpeed = speeds[juce::jlimit (0, 2, (int) value ("sweep_speed"))];
    v.sweepOn = value ("sweep_on") > 0.5f;
    v.tracking = value ("tracking") / 100.0f * 1.2f;
    v.filterFm = value ("filter_fm") / 100.0f;
    static const float muffs[] = { 0.0f, 0.5f, 1.0f };
    v.muffler = muffs[juce::jlimit (0, 2, (int) value ("muffler"))];
    v.highResonance = value ("high_res") > 0.5f;
    v.envModRange = value ("env_range") / 100.0f;
    v.envModZero = value ("env_zero") > 0.5f;
    v.bassFix = value ("bass_fix") > 0.5f;
    v.cutoffTopHz = value ("cutoff_top");
    v.vcaClick = value ("vca_click") / 100.0f;
    static const int osf[] = { 2, 4, 8 };
    p.oversample = osf[juce::jlimit (0, 2, (int) value ("quality"))];
    p.seqMode = (int) value ("mode") == 0;
    p.transpose = (int) std::round (value ("transpose"));
    p.accentVelocity = (int) value ("accent_vel");
    p.volume = value ("volume") / 100.0f;
    p.driveType = (int) value ("drive_type"); p.drive = value ("drive") / 100.0f; p.driveTone = value ("drive_tone") / 100.0f;
    p.delayMix = value ("delay_mix") / 100.0f; p.delayFeedback = value ("delay_fb") / 100.0f; p.delayTone = value ("delay_tone") / 100.0f;
    p.delayBeats = (float) params::delayBeats ((int) value ("delay_time"));
    return p;
}

void At303Processor::setPattern (int i, const Pattern& p)
{
    { const juce::SpinLock::ScopedLockType l (patLock); bank[(size_t) juce::jlimit (0, 63, i)] = p; }
    patDirty = true;
    if (onPatternsChanged) juce::MessageManager::callAsync ([this] { if (onPatternsChanged) onPatternsChanged(); });
}

void At303Processor::generate (bool mutate)
{
    GenParams g;
    g.root = (int) value ("gen_root"); g.scale = (int) value ("gen_scale"); g.octaves = (int) value ("gen_octaves");
    g.density = value ("gen_density") / 100.0f; g.accent = value ("gen_accent") / 100.0f; g.slide = value ("gen_slide") / 100.0f; g.ties = value ("gen_ties") / 100.0f;
    if (! mutate) genSeed = genSeed * 1103515245u + 12345u;
    g.seed = genSeed;
    Pattern base = getPattern (patternIndex());
    if (mutate) { g.seed ^= 0x5bd1e995u; g.lockRhythm = true; }     // mutar: mismo ritmo, alturas y banderas nuevas
    setPattern (patternIndex(), dh::tb::generate (base, g));
}

void At303Processor::clearPattern() { setPattern (patternIndex(), Pattern()); }

void At303Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0) return;
    engine.set (buildParams());
    // Patrón: si cambió (edición o selección), se pasa la copia al motor; entra en el compás siguiente.
    const int pi = patternIndex();
    if (pi != lastPatIdx || patDirty.load())
    {
        const juce::SpinLock::ScopedTryLockType l (patLock);
        if (l.isLocked()) { engine.setPattern (bank[(size_t) pi]); lastPatIdx = pi; patDirty = false; }
    }
    // Reloj: del host si sigue al host y está reproduciendo; si no, el propio con Tempo cuando Run está prendido.
    double bpm = value ("tempo"), ppq = 0; bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (value ("sync") > 0.5f) { if (auto b = pos->getBpm()) bpm = *b; }
            if (auto q = pos->getPpqPosition()) ppq = *q;
            hostPlaying = pos->getIsPlaying();
        }
    const bool useHost = value ("sync") > 0.5f && hostPlaying;
    const bool running = useHost || value ("run") > 0.5f;
    const double cps = bpm / 60.0 * 24.0 / sr;
    double clock0;
    if (useHost) clock0 = ppq * 24.0;
    else { clock0 = freeClock; if (running) freeClock += cps * n; else freeClock = 0; }
    // MIDI: notas para el modo MIDI y para dibujar el teclado; los mensajes se procesan en su muestra.
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    int pos = 0;
    float* L = buffer.getWritePointer (0); float* R = buffer.getWritePointer (1);
    for (const auto m : midi)
    {
        const int at = juce::jlimit (0, n, m.samplePosition);
        if (at > pos) { engine.process (L + pos, R + pos, at - pos, running, clock0 + pos * cps, cps); pos = at; }
        const auto msg = m.getMessage();
        if (msg.isNoteOn()) engine.midiNoteOn (msg.getNoteNumber(), msg.getVelocity());
        else if (msg.isNoteOff()) engine.midiNoteOff (msg.getNoteNumber());
        else if (msg.isAllNotesOff() || msg.isAllSoundOff()) engine.allNotesOff();
    }
    if (pos < n) engine.process (L + pos, R + pos, n - pos, running, clock0 + pos * cps, cps);
    // Vista.
    const auto& v = engine.voiceView();
    view.step.store (running ? engine.currentStep() : -1); view.running.store (running);
    view.c13.store (v.c13()); view.meg.store (v.megLevel()); view.veg.store (v.vegLevel());
    view.cutoffHz.store (v.cutoffHz()); view.cv.store (v.cvVolts()); view.k.store (v.kNow()); view.gate.store (v.gateHigh());
    view.level.store (buffer.getMagnitude (0, 0, n));
    for (int i = 0; i < 4; ++i) view.ladder[(size_t) i].store (v.ladderState (i));
    view.vca.store (v.vcaControl());
}

// ── Estado: parámetros + los 64 patrones ──
juce::ValueTree At303Processor::patternsToTree() const
{
    juce::ValueTree t ("PATTERNS");
    const juce::SpinLock::ScopedLockType l (patLock);
    for (int i = 0; i < 64; ++i)
    {
        const auto& p = bank[(size_t) i];
        juce::String pitch, time;
        for (int s = 0; s < Pattern::kSteps; ++s)
        {
            const auto& ps = p.pitch[(size_t) s];
            pitch << juce::String (ps.note) << (ps.up ? "u" : "") << (ps.down ? "d" : "") << (ps.accent ? "a" : "") << (ps.slide ? "s" : "") << " ";
            time << (p.time[(size_t) s] == kNote ? "N" : p.time[(size_t) s] == kTie ? "T" : "R");
        }
        juce::ValueTree c ("P"); c.setProperty ("i", i, nullptr); c.setProperty ("pitch", pitch.trim(), nullptr); c.setProperty ("time", time, nullptr);
        c.setProperty ("len", p.length, nullptr); c.setProperty ("trip", p.triplet, nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

static Pattern patternFromStrings (const juce::String& pitch, const juce::String& time, int len, bool trip)
{
    Pattern p;
    auto toks = juce::StringArray::fromTokens (pitch, " ", "");
    for (int s = 0; s < Pattern::kSteps && s < toks.size(); ++s)
    {
        auto& ps = p.pitch[(size_t) s]; const auto& tk = toks[s];
        ps.note = (int8_t) juce::jlimit (0, 12, tk.getIntValue());
        ps.up = tk.containsChar ('u'); ps.down = tk.containsChar ('d'); ps.accent = tk.containsChar ('a'); ps.slide = tk.containsChar ('s');
    }
    for (int s = 0; s < Pattern::kSteps && s < time.length(); ++s)
        p.time[(size_t) s] = time[s] == 'T' ? kTie : time[s] == 'R' ? kRest : kNote;
    p.length = juce::jlimit (1, 16, len); p.triplet = trip;
    if (p.triplet) p.length = std::min (p.length, 15);
    return p;
}

void At303Processor::patternsFromTree (const juce::ValueTree& t)
{
    if (! t.isValid()) return;
    const juce::SpinLock::ScopedLockType l (patLock);
    for (const auto& c : t)
    {
        const int i = c.getProperty ("i", -1);
        if (i < 0 || i >= 64) continue;
        bank[(size_t) i] = patternFromStrings (c.getProperty ("pitch").toString(), c.getProperty ("time").toString(), c.getProperty ("len", 16), c.getProperty ("trip", false));
    }
    patDirty = true;
}

void At303Processor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPresetName, nullptr);
    state.removeChild (state.getChildWithName ("PATTERNS"), nullptr);
    state.appendChild (patternsToTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void At303Processor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || state.getType() != apvts.state.getType()) return;
    const auto pats = state.getChildWithName ("PATTERNS").createCopy();
    apvts.replaceState (state);
    for (auto* prm : getParameters())
        if (auto* pb = dynamic_cast<juce::AudioParameterBool*> (prm)) pb->setValueNotifyingHost (pb->get() ? 1.0f : 0.0f);
    currentPresetName = state.getProperty ("preset", "Init").toString();
    patternsFromTree (pats);
    if (onPatternsChanged) juce::MessageManager::callAsync ([this] { if (onPatternsChanged) onPatternsChanged(); });
}

// ── Presets: sonido y patrón (se escribe en el patrón elegido por el preset) ──
void At303Processor::loadFactory()
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

int At303Processor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void At303Processor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String At303Processor::getProgramName (int index) { return index >= 0 && index < (int) presets.size() ? presets[(size_t) index].name : juce::String ("Init"); }

void At303Processor::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets.size()) return;
    const auto& pr = presets[(size_t) index];
    const auto pv = pr.data.getProperty ("params", {});
    // Sonido: todo a su valor por defecto salvo el transporte, el modo y el patrón elegido; después lo del preset.
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto id = rp->getParameterID();
            if ((id == "run" || id == "sync" || id == "mode" || id == "pattern" || id == "quality") && ! pv.hasProperty (id)) continue;
            rp->setValueNotifyingHost (rp->getDefaultValue());
        }
    if (auto* obj = pv.getDynamicObject())
        for (auto& kv : obj->getProperties()) setValue (kv.name.toString(), (float) kv.value);
    // Patrón del preset: se escribe en la posición elegida.
    const auto pt = pr.data.getProperty ("pattern", {});
    if (pt.isObject())
        setPattern (patternIndex(), patternFromStrings (pt.getProperty ("pitch", "").toString(), pt.getProperty ("time", "").toString(), pt.getProperty ("length", 16), pt.getProperty ("triplet", false)));
    currentProgram = index; currentPresetName = pr.name;
    if (onPresetChanged) juce::MessageManager::callAsync ([this] { if (onPresetChanged) onPresetChanged(); });
}

juce::AudioProcessorEditor* At303Processor::createEditor() { return new At303Editor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new At303Processor(); }
