#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace dh::gt;

// ───────── Hilo de la IR ─────────
// Espera un pedido, diseña la IR del modelo (o toma la del usuario), arma su núcleo particionado en irStage y avisa con
// irReady = true. Si el audio todavía no tomó el anterior, espera: nunca escribe mientras el audio puede estar leyendo.
struct ThrumProcessor::IrWorker : public juce::Thread
{
    explicit IrWorker (ThrumProcessor& p) : juce::Thread ("Thrum IR"), proc (p) {}
    ThrumProcessor& proc;
    juce::WaitableEvent wake;
    juce::SpinLock lock; EngineParams want; bool pending = false; double fs = 48000; int len = 2048;
    RealFft fft; std::vector<float> scratch;
    void request (const EngineParams& p, double sampleRate, int irLength)
    {
        { const juce::SpinLock::ScopedLockType l (lock); want = p; pending = true; fs = sampleRate; len = irLength; }
        wake.signal();
    }
    void run() override
    {
        while (! threadShouldExit())
        {
            wake.wait (200);
            EngineParams p; bool go = false; double f = 48000; int n = 2048;
            { const juce::SpinLock::ScopedLockType l (lock); if (pending) { p = want; pending = false; go = true; f = fs; n = len; } }
            if (! go) continue;
            std::vector<float> ir;
            if (p.cabSource == 2) { const juce::ScopedLock sl (proc.userIrLock); ir = proc.userIr; }
            else
            {
                const CabParams& c = p.cabSource == 1 ? factoryIrs()[(size_t) std::clamp (p.factoryIr, 0, (int) factoryIrs().size() - 1)].p : p.cab;
                ir = designCabIr (c, f, n);
            }
            if (ir.empty()) ir = { 1.0f };
            while (proc.irReady.load() && ! threadShouldExit()) wait (2);
            if (threadShouldExit()) break;
            proc.irStage.build (ir.data(), (int) ir.size(), f, fft, scratch);
            proc.irReady.store (true);
        }
    }
};

// ───────── Biblioteca de patrones ─────────
static juce::var libraryJson()
{
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (juce::String (BinaryData::namedResourceList[i]).startsWith ("library"))
        {
            int size = 0;
            const char* d = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
            return juce::JSON::parse (juce::String::fromUTF8 (d, size));
        }
    return {};
}
static int divIndex (const juce::String& s)
{
    for (int i = 0; i < kNumDivs; ++i) if (s == divisionName (i)) return i;
    return kDiv8;
}
static std::vector<ThrumProcessor::PatternEntry> parseLib (const char* key)
{
    std::vector<ThrumProcessor::PatternEntry> out;
    const auto arr = libraryJson().getProperty (key, {});
    if (auto* a = arr.getArray())
        for (const auto& v : *a)
        {
            ThrumProcessor::PatternEntry e;
            e.name = v.getProperty ("name", "").toString(); e.genre = v.getProperty ("genre", "").toString();
            e.steps = v.getProperty ("steps", "D").toString(); e.accents = v.getProperty ("accents", "").toString();
            e.div = divIndex (v.getProperty ("div", "1/8").toString()); e.swing = (float) v.getProperty ("swing", 50);
            out.push_back (e);
        }
    if (out.empty()) { ThrumProcessor::PatternEntry e; e.name = "Default"; e.steps = key[0] == 's' ? "D.DU.UDU" : "T 3 2 3 1 3 2 3"; out.push_back (e); }
    return out;
}
const std::vector<ThrumProcessor::PatternEntry>& ThrumProcessor::strumLibrary() { static const auto l = parseLib ("strum"); return l; }
const std::vector<ThrumProcessor::PatternEntry>& ThrumProcessor::arpLibrary() { static const auto l = parseLib ("arp"); return l; }
static juce::StringArray libNames (const std::vector<ThrumProcessor::PatternEntry>& l)
{
    juce::StringArray a; for (const auto& e : l) a.add (e.genre.isNotEmpty() ? e.genre + ": " + e.name : e.name);
    return a;
}

// ───────── Procesador ─────────
ThrumProcessor::ThrumProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("DI", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "THRUM", params::layout (libNames (strumLibrary()), libNames (arpLibrary())))
{
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm)) rawIdStore.push_back (rp->getParameterID());
    for (auto& id : rawIdStore) rawIds.push_back ({ id.toRawUTF8(), apvts.getRawParameterValue (id) });
    for (auto& n : view.note) n.store (-1);
    orderPacked = packOrder (EngineParams::defaultOrder());
    strum = strumLibrary()[0]; arp = arpLibrary()[0];
    midiIn.reserve (2048); midiOut.reserve (4096); rec.reserve (1 << 16);
    loadFactory();
    if (! presets.empty()) loadPreset (0);
    tracker.capture (*this);
    worker = std::make_unique<IrWorker> (*this);
    worker->startThread();
}

ThrumProcessor::~ThrumProcessor()
{
    if (worker) worker->stopThread (2000);
}

float ThrumProcessor::value (const char* id) const
{
    for (auto& r : rawIds) if (std::strcmp (r.first, id) == 0) return r.second->load();
    return 0.0f;
}

void ThrumProcessor::setValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

bool ThrumProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (const auto& b : l.inputBuses) if (! b.isDisabled()) return false;
    for (int i = 1; i < l.outputBuses.size(); ++i)
        if (! l.outputBuses[i].isDisabled() && l.outputBuses[i] != juce::AudioChannelSet::stereo() && l.outputBuses[i] != juce::AudioChannelSet::mono()) return false;
    return true;
}

void ThrumProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate; maxBlock = std::max (32, samplesPerBlock);
    {
        // La IR del usuario está a la frecuencia en que se cargó: si el host cambió de frecuencia, se remuestrea.
        const juce::ScopedLock sl (userIrLock);
        if (! userIr.empty() && std::fabs (userIrRate - sr) > 0.5)
        {
            userIr = resampleIr (userIr.data(), (int) userIr.size(), userIrRate, sr, ConvKernel::maxLength (sr));
            userIrRate = sr;
        }
    }
    engine.setSyncCab (isNonRealtime());
    engine.prepare (sr, std::min (maxBlock, 2048));
    const auto p = buildParams();
    engine.set (p);
    // La IR de arranque, ya: el primer bloque suena con su gabinete.
    if (p.cabSource == 2) { const juce::ScopedLock sl (userIrLock); if (! userIr.empty()) engine.loadIr (userIr.data(), (int) userIr.size(), true); }
    else { const auto ir = engine.currentModelIr(); engine.loadIr (ir.data(), (int) ir.size(), true); }
    lastCab = p.cab; lastCabSource = p.cabSource; lastFactory = p.factoryIr; cabInit = true; irReady = false;
    engine.reset();
    patDirty = true;
    diScratch.assign ((size_t) std::max (maxBlock, 8192), 0.0f);
    latency.setNow (engine.latency());
}

EngineParams ThrumProcessor::buildParams() const
{
    EngineParams p;
    auto v = [this] (const char* id) { return value (id); };
    auto& I = p.ins;
    I.type = (int) v ("g_type"); I.tuning = (int) v ("g_tuning"); I.gauge = (int) v ("g_gauge"); I.capo = (int) v ("g_capo");
    static const char* tid[] = { "g_t1", "g_t2", "g_t3", "g_t4", "g_t5", "g_t6", "g_t7" };
    // Los controles numeran las cuerdas como un guitarrista (1 = la más aguda); el motor, de la más grave.
    const int nS = guitar (I.type).nStrings;
    for (int k = 0; k < kMaxStrings; ++k) { const int s = nS - 1 - k; if (s >= 0) I.custom[(size_t) s] = (int) v (tid[k]); }
    I.fineTune = v ("g_fine"); I.stringAge = 1.0f - v ("g_age") * 0.01f; I.sustain = v ("g_sustain") * 0.01f;
    {
        // Selector: con tres posiciones, los tres del medio son "las dos".
        const int sel = (int) v ("g_selector");
        I.selector = guitar (I.type).selector == 5 ? sel : (sel == 0 ? 0 : sel == 4 ? 2 : 1);
    }
    I.volume = v ("g_volume") * 0.1f; I.tone = v ("g_tone") * 0.1f; I.cableM = v ("g_cable");
    I.sympathetic = v ("g_sym") * 0.01f; I.unplayedMuted = v ("g_mute_open") > 0.5f;
    I.buzz = v ("g_buzz") * 0.01f; I.action = v ("g_action") * 0.01f;
    I.pickupNoise = v ("g_noise") * 0.01f; I.mains60 = (int) v ("g_mains") == 1; I.squeak = v ("g_squeak") * 0.01f;
    auto& F = p.perf;
    F.mode = (int) v ("p_mode"); F.art = (int) v ("p_art"); F.palm = v ("p_palm") * 0.01f;
    F.legato = (int) v ("p_legato"); F.legatoFrets = v ("p_legato_frets"); F.slideTime = v ("p_slide_time") * 1e-3f;
    F.pickPosMm = v ("p_pick_pos"); F.pickHard = v ("p_pick_hard") * 0.01f; F.pickAngle = v ("p_pick_angle") * 0.01f;
    F.attackNoise = v ("g_attack") * 0.01f; F.fingers = v ("p_fingers") > 0.5f;
    F.autoVibrato = v ("p_vib_auto") > 0.5f; F.vibDelay = v ("p_vib_delay") * 1e-3f; F.vibRate = v ("p_vib_rate"); F.vibDepth = v ("p_vib_depth") * 0.01f;
    F.bendRange = v ("p_bend_range"); F.bendAll = v ("p_bend_all") > 0.5f;
    F.bendInAmount = v ("p_bendin"); F.bendInTime = v ("p_bendin_time") * 1e-3f;
    F.tremoloDiv = (int) v ("p_trem_div"); F.feedback = v ("p_feedback") * 0.01f; F.feedbackHarm = 1 + (int) v ("p_fb_harm");   // armónico 1..4 (octava = 2, octava y quinta = 3)
    F.whammyRange = v ("p_whammy"); F.deadBelow = (int) v ("p_dead_below"); F.pinchAbove = (int) v ("p_pinch_above");
    F.staccatoMs = v ("p_staccato"); F.idleMute = v ("p_idle");
    F.humanTune = v ("h_tune") * 0.01f; F.humanTime = v ("h_time") * 0.01f; F.humanVel = v ("h_vel") * 0.01f; F.humanPick = v ("h_pick") * 0.01f;
    F.releaseNoise = v ("g_release") * 0.01f; F.squeak = v ("g_squeak") * 0.01f;
    F.style = (int) v ("c_style"); F.region = (int) v ("c_region"); F.singleKey = (int) v ("c_single");
    F.strumMs = v ("c_strum"); F.upStrings = (int) v ("c_up"); F.chordWindowMs = v ("c_window"); F.strumCentred = (int) v ("c_anchor") == 0;
    F.latch = v ("c_latch") > 0.5f; F.letRing = v ("c_letring") > 0.5f; F.tempo = v ("s_tempo");
    F.prefs.fixedPosition = (int) v ("f_position");
    { const int ps = (int) v ("f_string"); F.prefs.preferredString = ps > 0 && ps <= nS ? nS - ps : -1; }
    F.prefs.openStrings = v ("f_open") * 0.01f; F.prefs.lowPositions = v ("f_low") * 0.01f;
    F.midiIn = (int) v ("m_in"); F.stringBendRange = v ("m_str_bend"); F.mpeBendRange = v ("m_mpe_bend");
    F.keyswitchBase = (int) v ("m_ks"); F.midiOut = v ("m_out") > 0.5f; F.midiOutPerString = v ("m_out_str") > 0.5f; F.midiOutChannel = (int) v ("m_out_ch");
    F.seed = (uint32_t) v ("m_seed");
    // Pedales.
    auto ped = [&] (int u, const char* on, std::initializer_list<const char*> ids, const char* type = nullptr) {
        auto& q = p.pedal[(size_t) u];
        q.on = v (on) > 0.5f; int k = 0;
        for (auto* id : ids) { if (k < 6) q.p[k++] = v (id) * 0.01f; }
        q.type = type ? (int) v (type) : 0;
    };
    ped (kGate, "gate_on", { "gate_thr", "gate_rel" });
    ped (kComp, "comp_on", { "comp_sus", "comp_att", "comp_lvl", "comp_mix" });
    ped (kWah, "wah_on", { "wah_pos", "wah_sens", "wah_speed", "wah_range", "wah_q" }, "wah_mode");
    ped (kOd, "od_on", { "od_drive", "od_tone", "od_lvl" });
    ped (kDist, "dist_on", { "dist_drive", "dist_filter", "dist_lvl" });
    ped (kFuzz, "fuzz_on", { "fuzz_fuzz", "fuzz_bias", "fuzz_tone", "fuzz_lvl" }, "fuzz_type");
    ped (kChorus, "cho_on", { "cho_rate", "cho_depth", "cho_mix", "cho_tone" });
    ped (kPhaser, "pha_on", { "pha_rate", "pha_depth", "pha_fb", "pha_mix" });
    ped (kFlanger, "fla_on", { "fla_rate", "fla_depth", "fla_fb", "fla_mix", "fla_manual" });
    ped (kTremolo, "trem_on", { "trem_rate", "trem_depth" }, "trem_shape");
    p.pedal[kTremolo].p[2] = v ("trem_sync") > 0.5f ? 1.0f : 0.0f;
    ped (kVibrato, "vib_on", { "vib_rate", "vib_depth" });
    ped (kDelay, "dly_on", { "dly_time", "dly_fb", "dly_mix", "dly_tone", "dly_mod" }, "dly_type");
    p.pedal[kDelay].p[5] = v ("dly_sync") > 0.5f ? 1.0f : 0.0f;
    if (p.pedal[kDelay].p[5] > 0.5f) p.pedal[kDelay].p[0] = v ("dly_div") * 0.01f;
    ped (kReverb, "rev_on", { "rev_decay", "rev_tone", "rev_mix", "rev_pre", "rev_size" }, "rev_type");
    p.pedal[kReverb].p[5] = v ("rev_freeze") > 0.5f ? 1.0f : 0.0f;
    // Amplificador y gabinete.
    auto& A = p.amp;
    p.ampOn = v ("amp_on") > 0.5f;
    A.model = (int) v ("amp_model"); A.gain = v ("amp_gain") * 0.1f; A.bass = v ("amp_bass") * 0.1f; A.mid = v ("amp_mid") * 0.1f; A.treble = v ("amp_treble") * 0.1f;
    A.presence = v ("amp_presence") * 0.1f; A.depth = v ("amp_depth") * 0.1f; A.master = v ("amp_master") * 0.1f; A.sag = v ("amp_sag") * 0.1f;
    A.bright = v ("amp_bright") > 0.5f; A.autoLevel = v ("amp_auto") > 0.5f; A.levelDb = v ("amp_level");
    { static const int q[] = { 2, 4, 8 }; A.quality = q[juce::jlimit (0, 2, (int) v ("amp_quality"))]; }
    p.cabOn = v ("cab_on") > 0.5f; p.cabSource = (int) v ("cab_source"); p.factoryIr = (int) v ("cab_ir");
    auto& C = p.cab;
    C.speaker = (int) v ("cab_speaker"); C.cab = (int) v ("cab_box");
    C.a.type = (int) v ("cab_mic_a"); C.a.position = v ("cab_pos_a") * 0.01f; C.a.distanceCm = v ("cab_dist_a");
    C.b.type = (int) v ("cab_mic_b"); C.b.position = v ("cab_pos_b") * 0.01f; C.b.distanceCm = v ("cab_dist_b");
    C.mix = v ("cab_mix") * 0.01f; C.room = v ("cab_room") * 0.01f;
    p.order = unpackOrder (orderPacked.load());
    p.outDb = v ("out_level"); p.diDb = v ("out_di"); p.diOnMain = v ("out_di_main") > 0.5f;
    return p;
}

void ThrumProcessor::requestIr (const EngineParams& p)
{
    if (! worker) return;
    worker->request (p, sr, engine.irLength());
}

void ThrumProcessor::applyPatternToEngine()
{
    PatternEntry s, a;
    { const juce::SpinLock::ScopedTryLockType l (patLock); if (! l.isLocked()) return; s = strum; a = arp; }
    // (juce::String en el hilo de audio: copias por referencia, sólo cuando el patrón cambió.)
    engine.performer().setStrumPattern (parseStrum (s.steps.toRawUTF8(), s.accents.toRawUTF8(), s.div, s.swing * 0.01f));
    engine.performer().setArpPattern (parseArp (a.steps.toRawUTF8(), a.div, a.swing * 0.01f));
    patDirty = false;
}

void ThrumProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0) { midi.clear(); return; }
    const bool offline = isNonRealtime();
    engine.setSyncCab (offline);
    // Patrón elegido en la biblioteca (parámetro): se carga en el patrón editable.
    {
        const int si = (int) value ("s_pattern"), ai = (int) value ("a_pattern");
        if (si != lastStrumIdx) { lastStrumIdx = si; if (si >= 0 && si < (int) strumLibrary().size()) { const juce::SpinLock::ScopedTryLockType l (patLock); if (l.isLocked()) { strum = strumLibrary()[(size_t) si]; patDirty = true; patternDirtyUi = true; triggerAsyncUpdate(); } } }
        if (ai != lastArpIdx) { lastArpIdx = ai; if (ai >= 0 && ai < (int) arpLibrary().size()) { const juce::SpinLock::ScopedTryLockType l (patLock); if (l.isLocked()) { arp = arpLibrary()[(size_t) ai]; patDirty = true; patternDirtyUi = true; triggerAsyncUpdate(); } } }
    }
    if (patDirty) applyPatternToEngine();
    const auto p = buildParams();
    engine.set (p);
    // Gabinete: si cambió, se pide la IR nueva (o se diseña ya, fuera de tiempo real).
    const bool cabChanged = ! cabInit || ! (p.cab == lastCab) || p.cabSource != lastCabSource || p.factoryIr != lastFactory || userIrDirty.exchange (false);
    if (cabChanged)
    {
        lastCab = p.cab; lastCabSource = p.cabSource; lastFactory = p.factoryIr; cabInit = true;
        if (offline)
        {
            if (p.cabSource == 2) { const juce::ScopedLock sl (userIrLock); if (! userIr.empty()) engine.loadIr (userIr.data(), (int) userIr.size()); }
            else { const auto ir = engine.currentModelIr(); engine.loadIr (ir.data(), (int) ir.size()); }
        }
        else requestIr (p);
    }
    if (irReady.load())
    {
        // (Armado para otra frecuencia de muestreo: se pide de nuevo.)
        if (! engine.swapIn (irStage)) requestIr (p);
        irReady = false;
    }
    // Transporte.
    HostTime h; h.bpm = p.perf.tempo;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            h.playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) h.ppq = *q; else h.playing = false;
            if (auto b = pos->getBpm()) h.bpm = juce::jlimit (20.0, 999.0, *b);
        }
    recBpm = h.bpm;
    // MIDI de entrada (y el wah por CC 11).
    keyboardState.processNextMidiBuffer (midi, 0, n, true);
    midiIn.clear(); midiOut.clear();
    const bool wahCc = (int) value ("wah_mode") == 2;
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        const auto* raw = msg.getRawData();
        if (msg.getRawDataSize() < 1 || raw[0] < 0x80 || raw[0] >= 0xF0) continue;
        if (wahCc && msg.isController() && msg.getControllerNumber() == 11) { engine.setWahPosition (msg.getControllerValue() / 127.0f); continue; }
        MidiMsg mm; mm.offset = juce::jlimit (0, n - 1, m.samplePosition); mm.status = raw[0];
        mm.d1 = msg.getRawDataSize() > 1 ? raw[1] : 0; mm.d2 = msg.getRawDataSize() > 2 ? raw[2] : 0;
        if (midiIn.size() < midiIn.capacity()) midiIn.push_back (mm);
    }
    // Salidas: principal y DI (si el host activó el bus).
    float* L = buffer.getWritePointer (0); float* R = buffer.getWritePointer (1);
    float* di = nullptr;
    auto diBus = getBusBuffer (buffer, false, 1);
    if (diBus.getNumChannels() > 0) di = diBus.getWritePointer (0);
    else { if ((int) diScratch.size() < n) diScratch.resize ((size_t) n); di = diScratch.data(); }
    engine.process (L, R, di, n, h, midiIn.data(), (int) midiIn.size(), &midiOut);
    if (diBus.getNumChannels() > 1) diBus.copyFrom (1, 0, diBus, 0, 0, n);
    // MIDI de salida y grabación.
    midi.clear();
    const bool out = p.perf.midiOut;
    // Salida apagada a mitad de una nota: se cierran las que el host tenía abiertas.
    if (lastMidiOut && ! out)
        for (int c = 0; c < 16; ++c) for (int k = 0; k < 128; ++k) if (hostNotes[(size_t) c][(size_t) k]) { midi.addEvent (juce::MidiMessage::noteOff (c + 1, k), 0); hostNotes[(size_t) c][(size_t) k] = false; }
    lastMidiOut = out;
    {
        const juce::SpinLock::ScopedTryLockType l (recLock);
        for (const auto& e : midiOut)
        {
            // Los note-on salen si la salida está activa; los note-off, siempre que el host tenga esa nota abierta.
            const int ch = e.status & 15, type = e.status & 0xF0;
            const bool on = type == 0x90 && e.d2 > 0;
            const bool send = on ? out : hostNotes[(size_t) ch][e.d1];
            if (send)
            {
                midi.addEvent (juce::MidiMessage (e.status, e.d1, e.d2), (int) juce::jlimit<int64_t> (0, n - 1, e.t));
                hostNotes[(size_t) ch][e.d1] = on;
            }
            if (l.isLocked() && rec.size() < rec.capacity())
            {
                const int64_t abs = engine.samplePosition() - n + e.t;
                if (recOrigin < 0) recOrigin = abs;
                rec.push_back ({ abs - recOrigin, e.status, e.d1, e.d2 });
            }
        }
        if (l.isLocked()) recCount = (int) rec.size();
    }
    latency.request (engine.latency());
    // Vista.
    const auto& perf = engine.performer();
    const int nS = engine.instrument().strings();
    view.strings.store (nS);
    for (int s = 0; s < kMaxStrings; ++s)
    {
        const auto sv = perf.view (s);
        view.note[(size_t) s].store (s < nS && sv.sounding ? sv.note : -1);
        view.fret[(size_t) s].store (sv.fret);
        view.level[(size_t) s].store (s < nS ? engine.instrument().string (s).envelope() : 0.0f);
    }
    view.art.store (perf.currentArt()); view.hand.store (perf.handPosition());
    {
        const auto& c = perf.chord();
        view.chordRoot.store (c.root); view.chordType.store (c.type); view.chordBass.store (c.bass);
    }
    view.outPeak.store (std::max (buffer.getMagnitude (0, 0, n), view.outPeak.load() * 0.9f));
}

void ThrumProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear(); midiOut.clear();
    engine.allNotesOff (&midiOut);
    for (int c = 0; c < 16; ++c) for (int k = 0; k < 128; ++k) if (hostNotes[(size_t) c][(size_t) k]) { midi.addEvent (juce::MidiMessage::noteOff (c + 1, k), 0); hostNotes[(size_t) c][(size_t) k] = false; }
}

// ───────── Patrones y cadena ─────────
void ThrumProcessor::setStrum (const PatternEntry& p) { { const juce::SpinLock::ScopedLockType l (patLock); strum = p; } patDirty = true; patternDirtyUi = true; triggerAsyncUpdate(); }
void ThrumProcessor::setArp (const PatternEntry& p) { { const juce::SpinLock::ScopedLockType l (patLock); arp = p; } patDirty = true; patternDirtyUi = true; triggerAsyncUpdate(); }
void ThrumProcessor::loadStrumFromLibrary (int i) { if (i >= 0 && i < (int) strumLibrary().size()) { setValue ("s_pattern", (float) i); setStrum (strumLibrary()[(size_t) i]); lastStrumIdx = i; } }
void ThrumProcessor::loadArpFromLibrary (int i) { if (i >= 0 && i < (int) arpLibrary().size()) { setValue ("a_pattern", (float) i); setArp (arpLibrary()[(size_t) i]); lastArpIdx = i; } }

void ThrumProcessor::setOrder (const std::array<int, kNumUnits>& o)
{
    // Sólo permutaciones válidas.
    std::array<bool, kNumUnits> seen {};
    for (int u : o) { if (u < 0 || u >= kNumUnits || seen[(size_t) u]) return; seen[(size_t) u] = true; }
    orderPacked = packOrder (o);
}
juce::String ThrumProcessor::orderToString (const std::array<int, kNumUnits>& o)
{
    juce::StringArray a; for (int u : o) a.add (unitKey (u));
    return a.joinIntoString (" ");
}
std::array<int, kNumUnits> ThrumProcessor::orderFromString (const juce::String& s)
{
    auto o = EngineParams::defaultOrder();
    juce::StringArray t; t.addTokens (s, " ,", ""); t.removeEmptyStrings();
    std::array<int, kNumUnits> r {}; std::array<bool, kNumUnits> seen {}; int k = 0;
    for (const auto& tok : t)
        for (int u = 0; u < kNumUnits; ++u)
            if (tok == unitKey (u) && ! seen[(size_t) u] && k < kNumUnits) { r[(size_t) k++] = u; seen[(size_t) u] = true; }
    // Lo que falte va al final en el orden de fábrica.
    for (int u : o) if (! seen[(size_t) u] && k < kNumUnits) { r[(size_t) k++] = u; seen[(size_t) u] = true; }
    return r;
}

// ───────── IR del usuario ─────────
bool ThrumProcessor::loadUserIr (const juce::File& f, juce::String& error)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) { error = "Not an audio file this host can read"; return false; }
    const int frames = (int) std::min<juce::int64> (r->lengthInSamples, (juce::int64) (r->sampleRate * kMaxIrSeconds));
    if (frames <= 0) { error = "Empty file"; return false; }
    juce::AudioBuffer<float> b ((int) std::max (1u, r->numChannels), frames);
    r->read (&b, 0, frames, 0, true, true);
    std::vector<float> mono ((size_t) frames, 0.0f);
    for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < frames; ++i) mono[(size_t) i] += b.getSample (c, i) / (float) b.getNumChannels();
    auto ir = resampleIr (mono.data(), frames, r->sampleRate, sr, ConvKernel::maxLength (sr));
    { const juce::ScopedLock sl (userIrLock); userIr = std::move (ir); userIrRate = sr; }
    userIrNameStr = f.getFileName();
    userIrDirty = true;
    setValue ("cab_source", 2.0f);
    return true;
}

// ───────── MIDI: exportar ─────────
int ThrumProcessor::recordedEvents() const { return recCount.load(); }
void ThrumProcessor::clearRecording() { const juce::SpinLock::ScopedLockType l (recLock); rec.clear(); recCount = 0; recOrigin = -1; }

static bool writeMidi (const juce::File& f, juce::MidiMessageSequence& seq, double bpm)
{
    auto tempo = juce::MidiMessage::tempoMetaEvent ((int) std::lround (60.0e6 / std::max (20.0, bpm))); tempo.setTimeStamp (0); seq.addEvent (tempo);
    auto ts = juce::MidiMessage::timeSignatureMetaEvent (4, 4); ts.setTimeStamp (0); seq.addEvent (ts);
    seq.sort(); seq.updateMatchedPairs();
    juce::MidiFile mf; mf.setTicksPerQuarterNote (960); mf.addTrack (seq);
    f.deleteFile();
    juce::FileOutputStream os (f);
    return os.openedOk() && mf.writeTo (os);
}

bool ThrumProcessor::exportPlayed (const juce::File& f)
{
    std::vector<Rec> copy;
    { const juce::SpinLock::ScopedLockType l (recLock); copy = rec; }
    if (copy.empty()) return false;
    const double bpm = recBpm, ticksPerSample = 960.0 * bpm / 60.0 / sr;
    juce::MidiMessageSequence seq;
    for (const auto& e : copy) { juce::MidiMessage m (e.s, e.d1, e.d2); m.setTimeStamp ((double) e.t * ticksPerSample); seq.addEvent (m); }
    return writeMidi (f, seq, bpm);
}

bool ThrumProcessor::exportPattern (const juce::File& f, int bars)
{
    // Un motor aparte toca el patrón sobre el acorde que suena (o C mayor) y se guarda lo que tocó.
    Engine e; e.setSyncCab (true);
    const double fsr = 48000.0; const int blk = 512;
    e.prepare (fsr, blk);
    auto p = buildParams(); p.ampOn = false; p.cabOn = false; for (auto& q : p.pedal) q.on = false;
    p.perf.humanTime = 0.0f; p.perf.humanVel = 0.0f;
    if (p.perf.mode != kModeArp) p.perf.mode = kModeStrum;
    e.set (p); e.reset();
    {
        const auto s = getStrum(); const auto a = getArp();
        e.performer().setStrumPattern (parseStrum (s.steps.toRawUTF8(), s.accents.toRawUTF8(), s.div, s.swing * 0.01f));
        e.performer().setArpPattern (parseArp (a.steps.toRawUTF8(), a.div, a.swing * 0.01f));
    }
    std::vector<int> chord;
    for (int nn = 0; nn < 128; ++nn) if (keyboardState.isNoteOnForChannels (0xffff, nn) && nn >= (int) value ("m_ks") + 20) chord.push_back (nn);
    if (chord.empty()) chord = { 48, 52, 55 };
    const double bpm = recBpm > 0 ? recBpm : 120.0;
    const int frames = (int) (bars * 4 * 60.0 / bpm * fsr);
    std::vector<MidiMsg> in; for (int nn : chord) in.push_back ({ 0, 0x90, (uint8_t) nn, 100 });
    std::vector<float> L (blk), R (blk); std::vector<MidiOutEv> out;
    juce::MidiMessageSequence seq;
    const double tps = 960.0 * bpm / 60.0 / fsr;
    for (int pos = 0; pos < frames; pos += blk)
    {
        HostTime h; h.playing = true; h.bpm = bpm; h.ppq = pos / fsr * bpm / 60.0;
        out.clear();
        e.process (L.data(), R.data(), nullptr, blk, h, pos == 0 ? in.data() : nullptr, pos == 0 ? (int) in.size() : 0, &out);
        for (const auto& m : out) { juce::MidiMessage mm (m.status, m.d1, m.d2); mm.setTimeStamp ((double) (pos + m.t) * tps); seq.addEvent (mm); }
    }
    // Lo que quedó sonando se cierra al final.
    out.clear(); e.allNotesOff (&out);
    for (const auto& m : out) { juce::MidiMessage mm (m.status, m.d1, m.d2); mm.setTimeStamp ((double) frames * tps); seq.addEvent (mm); }
    return writeMidi (f, seq, bpm);
}

// ───────── Estado ─────────
void ThrumProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPresetName.get(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetSnapshot", tracker.toString(), nullptr);
    state.setProperty ("order", orderToString (getOrder()), nullptr);
    const auto s = getStrum(), a = getArp();
    state.setProperty ("strumSteps", s.steps, nullptr); state.setProperty ("strumAccents", s.accents, nullptr); state.setProperty ("strumName", s.name, nullptr);
    state.setProperty ("strumDiv", s.div, nullptr); state.setProperty ("strumSwing", s.swing, nullptr);
    state.setProperty ("arpSteps", a.steps, nullptr); state.setProperty ("arpName", a.name, nullptr);
    state.setProperty ("arpDiv", a.div, nullptr); state.setProperty ("arpSwing", a.swing, nullptr);
    {
        const juce::ScopedLock sl (userIrLock);
        if (! userIr.empty())
        {
            juce::MemoryBlock mb (userIr.data(), userIr.size() * sizeof (float));
            state.setProperty ("userIr", mb.toBase64Encoding(), nullptr);
            state.setProperty ("userIrRate", userIrRate, nullptr);
            state.setProperty ("userIrName", userIrNameStr.get(), nullptr);
        }
    }
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void ThrumProcessor::setStateInformation (const void* data, int sizeInBytes)
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
    setOrder (orderFromString (state.getProperty ("order", "").toString()));
    {
        PatternEntry s = strumLibrary()[(size_t) juce::jlimit (0, (int) strumLibrary().size() - 1, (int) value ("s_pattern"))];
        if (state.hasProperty ("strumSteps"))
        {
            s.steps = state.getProperty ("strumSteps").toString(); s.accents = state.getProperty ("strumAccents").toString(); s.name = state.getProperty ("strumName", s.name).toString();
            s.div = juce::jlimit (0, kNumDivs - 1, (int) state.getProperty ("strumDiv", s.div)); s.swing = (float) state.getProperty ("strumSwing", s.swing);
        }
        PatternEntry a = arpLibrary()[(size_t) juce::jlimit (0, (int) arpLibrary().size() - 1, (int) value ("a_pattern"))];
        if (state.hasProperty ("arpSteps"))
        {
            a.steps = state.getProperty ("arpSteps").toString(); a.name = state.getProperty ("arpName", a.name).toString();
            a.div = juce::jlimit (0, kNumDivs - 1, (int) state.getProperty ("arpDiv", a.div)); a.swing = (float) state.getProperty ("arpSwing", a.swing);
        }
        { const juce::SpinLock::ScopedLockType l (patLock); strum = s; arp = a; }
        lastStrumIdx = (int) value ("s_pattern"); lastArpIdx = (int) value ("a_pattern");
        patDirty = true;
    }
    if (state.hasProperty ("userIr"))
    {
        juce::MemoryBlock mb; mb.fromBase64Encoding (state.getProperty ("userIr").toString());
        const int n = (int) (mb.getSize() / sizeof (float));
        const double from = (double) state.getProperty ("userIrRate", 48000.0);
        if (n > 0 && n <= ConvKernel::maxLength (from) + 1)
        {
            auto ir = std::fabs (from - sr) > 1.0 ? resampleIr ((const float*) mb.getData(), n, from, sr, ConvKernel::maxLength (sr)) : std::vector<float> ((const float*) mb.getData(), (const float*) mb.getData() + n);
            { const juce::ScopedLock sl (userIrLock); userIr = std::move (ir); userIrRate = sr; }
            userIrNameStr = state.getProperty ("userIrName", "").toString();
            userIrDirty = true;
        }
    }
    presetDirty = true; patternDirtyUi = true; triggerAsyncUpdate();
}

void ThrumProcessor::handleAsyncUpdate()
{
    if (presetDirty.exchange (false) && onPresetChanged) onPresetChanged();
    if (patternDirtyUi.exchange (false) && onPatternChanged) onPatternChanged();
}

// ───────── Presets ─────────
void ThrumProcessor::loadFactory()
{
    presets.clear();
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String name (BinaryData::namedResourceList[i]);
        if (! name.endsWith ("_json") || name.startsWith ("library") || name.startsWith ("layout")) continue;
        int size = 0;
        const char* d = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        auto v = juce::JSON::parse (juce::String::fromUTF8 (d, size));
        if (v.isObject() && v.hasProperty ("params")) presets.push_back ({ v.getProperty ("name", name).toString(), v.getProperty ("category", "").toString(), v.getProperty ("description", "").toString(), v });
    }
    std::sort (presets.begin(), presets.end(), [] (const PresetEntry& a, const PresetEntry& b) { return (int) a.data.getProperty ("order", 999) < (int) b.data.getProperty ("order", 999); });
}

int ThrumProcessor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void ThrumProcessor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String ThrumProcessor::getProgramName (int index) { return index >= 0 && index < (int) presets.size() ? presets[(size_t) index].name : juce::String ("Init"); }

void ThrumProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets.size()) return;
    const auto& pr = presets[(size_t) index];
    const auto pv = pr.data.getProperty ("params", {});
    // Todo a su valor por defecto salvo los ajustes de la sesión (MIDI, tempo propio, salidas); después lo del preset.
    static const juce::StringArray session { "m_in", "m_out", "m_out_str", "m_out_ch", "m_ks", "s_tempo", "out_di_main", "out_di", "amp_quality", "m_str_bend", "m_mpe_bend" };
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto id = rp->getParameterID();
            if (session.contains (id) && ! pv.hasProperty (id)) continue;
            rp->setValueNotifyingHost (rp->getDefaultValue());
        }
    if (auto* obj = pv.getDynamicObject())
        for (auto& kv : obj->getProperties())
        {
            if (kv.value.isString())
            {
                // Las elecciones pueden venir por nombre.
                if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (apvts.getParameter (kv.name.toString())))
                {
                    const int i = c->choices.indexOf (kv.value.toString());
                    if (i >= 0) setValue (kv.name.toString(), (float) i);
                }
                continue;
            }
            setValue (kv.name.toString(), (float) kv.value);
        }
    // Orden de la cadena y patrones del preset.
    setOrder (orderFromString (pr.data.getProperty ("chain", "").toString()));
    const int si = (int) value ("s_pattern"), ai = (int) value ("a_pattern");
    PatternEntry s = strumLibrary()[(size_t) juce::jlimit (0, (int) strumLibrary().size() - 1, si)];
    PatternEntry a = arpLibrary()[(size_t) juce::jlimit (0, (int) arpLibrary().size() - 1, ai)];
    if (pr.data.hasProperty ("strum"))
    {
        s.steps = pr.data.getProperty ("strum", s.steps).toString(); s.accents = pr.data.getProperty ("accents", "").toString(); s.name = "Preset";
        s.div = divIndex (pr.data.getProperty ("strum_div", divisionName (s.div)).toString()); s.swing = (float) pr.data.getProperty ("strum_swing", s.swing);
    }
    if (pr.data.hasProperty ("arp"))
    {
        a.steps = pr.data.getProperty ("arp", a.steps).toString(); a.name = "Preset";
        a.div = divIndex (pr.data.getProperty ("arp_div", divisionName (a.div)).toString()); a.swing = (float) pr.data.getProperty ("arp_swing", a.swing);
    }
    { const juce::SpinLock::ScopedLockType l (patLock); strum = s; arp = a; }
    lastStrumIdx = si; lastArpIdx = ai;
    patDirty = true;
    currentProgram = index; currentPresetName = pr.name;
    tracker.capture (*this);
    presetDirty = true; patternDirtyUi = true; triggerAsyncUpdate();
}

juce::AudioProcessorEditor* ThrumProcessor::createEditor() { return new ThrumEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ThrumProcessor(); }
