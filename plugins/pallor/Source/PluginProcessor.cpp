#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace dh::pal;

namespace
{
    const char* const kVoiceIds[] = { "engine", "model", "kit", "chain", "tune", "decay", "tone", "char1", "char2", "level", "pan", "accent", "velsens",
                                      "drive", "bits", "rate", "cutoff", "reso", "senda", "sendb", "filter", "choke", "out", "reverse", "mute", "solo" };
    constexpr int kNumVoiceIds = (int) (sizeof (kVoiceIds) / sizeof (kVoiceIds[0]));
    enum VI { vEngine, vModel, vKit, vChain, vTune, vDecay, vTone, vChar1, vChar2, vLevel, vPan, vAccent, vVelsens, vDrive, vBits, vRate, vCutoff, vReso, vSendA, vSendB, vFilter, vChoke, vOut, vReverse, vMute, vSolo };
}

PallorProcessor::PallorProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Kick", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Snare-Clap", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Hats-Cymbals", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Toms", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Perc", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("FX", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "PALLOR", params::layout())
{
    for (auto* prm : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm)) rawIdStore.push_back (rp->getParameterID());
    for (auto& id : rawIdStore) rawIds.push_back ({ id.toRawUTF8(), apvts.getRawParameterValue (id) });
    // Punteros a los valores crudos de cada instrumento, armados una vez: buildParams lee ~430 parámetros por bloque
    // sin buscar por nombre.
    for (int v = 0; v < kNumSlots; ++v)
        for (int i = 0; i < kNumVoiceIds && i < (int) voicePtr[0].size(); ++i) voicePtr[(size_t) v][(size_t) i] = apvts.getRawParameterValue (params::vid (v, kVoiceIds[i]));
    for (auto& p : userPtr) p.store (nullptr);
    juce::StringArray session { "routing", "midi_out", "midi_ch", "note_map", "accent_vel", "midi_gate" };
    for (int s = 0; s < kNumSlots; ++s) { session.add (params::vid (s, "out")); session.add (params::vid (s, "solo")); }
    tracker.ignore (session);
    loadFactory();
    // Un instrumento recién abierto ya tiene un kit y sus patrones (el primer preset): con el host en marcha, suena.
    if (! presets.empty()) loadPreset (0);
    tracker.capture (*this);
    midiIn.reserve (1024); midiOut.reserve (4096);
}

float PallorProcessor::value (const char* id) const
{
    for (auto& r : rawIds) if (std::strcmp (r.first, id) == 0) return r.second->load();
    return 0.0f;
}

bool PallorProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    if (! l.inputBuses.isEmpty()) for (const auto& b : l.inputBuses) if (! b.isDisabled()) return false;
    for (int i = 1; i < l.outputBuses.size(); ++i)
        if (! l.outputBuses[i].isDisabled() && l.outputBuses[i] != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void PallorProcessor::setValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void PallorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sr, samplesPerBlock);
    engine.set (buildParams());
    patDirty = ~0ull; chainDirty = true; freeClock = 0;
    latency.setNow (engine.latency());
}

EngineParams PallorProcessor::buildParams() const
{
    EngineParams p;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto& c = voicePtr[(size_t) s];
        auto g = [&] (int i) { return c[(size_t) i] != nullptr ? c[(size_t) i]->load() : 0.0f; };
        VoiceParams& v = p.v[(size_t) s];
        v.engine = (int) g (vEngine); v.model = (int) g (vModel); v.kit = (int) g (vKit); v.chain = (int) g (vChain);
        v.shape.tune = g (vTune); v.shape.decay = g (vDecay) * 0.01f; v.shape.tone = g (vTone) * 0.01f;
        v.shape.a = g (vChar1) * 0.01f; v.shape.b = g (vChar2) * 0.01f;
        v.levelDb = g (vLevel); v.pan = g (vPan) * 0.01f; v.accent = g (vAccent) * 0.01f; v.velSense = g (vVelsens) * 0.01f;
        v.drive = g (vDrive) * 0.01f; v.bits = g (vBits); v.rateHz = g (vRate) >= 47999.0f ? 0.0f : g (vRate);
        v.filter = (int) g (vFilter); v.cutoff = g (vCutoff); v.reso = g (vReso) * 0.01f;
        v.choke = (int) g (vChoke); v.sendA = g (vSendA) * 0.01f; v.sendB = g (vSendB) * 0.01f; v.out = (int) g (vOut);
        v.reverse = g (vReverse) > 0.5f; v.mute = g (vMute) > 0.5f; v.solo = g (vSolo) > 0.5f;
    }
    auto& G = p.g;
    G.seq.mode = (int) value ("mode"); G.seq.group = (int) value ("group"); G.seq.fill = value ("fill") > 0.5f;
    G.seq.autofill = params::autofillBars ((int) value ("autofill"));
    G.seq.swing = value ("swing") * 0.01f; G.seq.swingRes = (int) value ("swing_res"); G.seq.seed = (uint32_t) value ("seed");
    G.accent = value ("accent") * 0.01f; G.noteMap = (int) value ("note_map");
    G.midiOut = value ("midi_out") > 0.5f; G.midiChannel = (int) value ("midi_ch");
    G.routing = (int) value ("routing") == 0 ? 0 : 2; G.accentVelocity = (int) value ("accent_vel"); G.midiGateMs = value ("midi_gate");
    auto& A = p.a;
    A.type = (int) value ("a_type"); A.size = value ("a_size") * 0.01f; A.predelayMs = value ("a_predelay"); A.gateMs = value ("a_gate");
    A.shape = value ("a_shape") * 0.01f; A.tone = value ("a_tone") * 0.01f; A.lowcutHz = value ("a_lowcut"); A.width = value ("a_width") * 0.01f;
    p.aReturnDb = value ("a_return") <= -59.9f ? -200.0f : value ("a_return");
    auto& B = p.b;
    B.type = (int) value ("b_type"); B.timeBeats = params::divisionBeats ((int) value ("b_time")); B.feedback = value ("b_fb") * 0.01f;
    B.tone = value ("b_tone") * 0.01f; B.rate = value ("b_rate") * 0.01f; B.depth = value ("b_depth") * 0.01f; B.width = value ("b_width") * 0.01f;
    p.bReturnDb = value ("b_return") <= -59.9f ? -200.0f : value ("b_return");
    auto& M = p.m;
    M.lofi = (int) value ("m_lofi"); M.lofiMix = value ("m_lofi_mix") * 0.01f; M.tape = value ("m_tape") > 0.5f; M.tapeDriveDb = value ("m_tape_drive");
    M.comp = value ("m_comp") * 0.01f; M.ratio = (int) value ("m_ratio"); M.attack = (int) value ("m_attack"); M.release = (int) value ("m_release");
    M.compMix = value ("m_comp_mix") * 0.01f; M.exciter = value ("m_exciter") * 0.01f; M.exciterHz = value ("m_exciter_freq"); M.volumeDb = value ("m_volume");
    return p;
}

// ── Patrones ──
Pattern PallorProcessor::getPattern (int i) const { const juce::SpinLock::ScopedLockType l (bankLock); return bank.p[(size_t) juce::jlimit (0, kPatterns - 1, i)]; }
void PallorProcessor::setPattern (int i, const Pattern& p)
{
    i = juce::jlimit (0, kPatterns - 1, i);
    { const juce::SpinLock::ScopedLockType l (bankLock); bank.p[(size_t) i] = p; }
    patDirty.fetch_or (1ull << i);
    patternsDirty = true; triggerAsyncUpdate();
}
std::vector<ChainEntry> PallorProcessor::getChain() const
{
    const juce::SpinLock::ScopedLockType l (bankLock);
    return std::vector<ChainEntry> (bank.chain.begin(), bank.chain.begin() + bank.chainLen);
}
void PallorProcessor::setChain (const std::vector<ChainEntry>& c)
{
    {
        const juce::SpinLock::ScopedLockType l (bankLock);
        bank.chainLen = std::min ((int) c.size(), kMaxChain);
        for (int i = 0; i < bank.chainLen; ++i) bank.chain[(size_t) i] = c[(size_t) i];
    }
    chainDirty = true; patternsDirty = true; triggerAsyncUpdate();
}
void PallorProcessor::clearBank()
{
    { const juce::SpinLock::ScopedLockType l (bankLock); for (auto& p : bank.p) p = Pattern(); bank.chainLen = 0; }
    patDirty = ~0ull; chainDirty = true; patternsDirty = true; triggerAsyncUpdate();
}

void PallorProcessor::loadLibrarySet (int index, int group)
{
    if (index < 0 || index >= (int) library.size()) return;
    const auto& s = library[(size_t) index];
    // El swing del juego viaja con sus patrones (si no, siguen al Swing general).
    auto withSwing = [&] (Pattern p) { if (s.swing > 50.5f) p.swing = (uint8_t) juce::jlimit (50, 75, juce::roundToInt (s.swing)); return p; };
    setPattern (patIndex (group, 0), withSwing (s.a)); setPattern (patIndex (group, 1), withSwing (s.b)); setPattern (patIndex (group, 2), withSwing (s.fill));
}

// ── Audio ──
void PallorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0) return;
    engine.set (buildParams());
    for (int s = 0; s < kNumSlots; ++s) engine.setUserSample (s, userPtr[(size_t) s].load());
    // Patrones editados: sólo los que cambiaron.
    if (patDirty.load() != 0 || chainDirty.load())
    {
        const juce::SpinLock::ScopedTryLockType l (bankLock);
        if (l.isLocked())
        {
            const uint64_t d = patDirty.exchange (0);
            for (int i = 0; i < kPatterns; ++i) if (d & (1ull << i)) engine.bankRef().p[(size_t) i] = bank.p[(size_t) i];
            if (chainDirty.exchange (false)) { engine.bankRef().chain = bank.chain; engine.bankRef().chainLen = bank.chainLen; }
        }
    }
    // Reloj: del host si sigue al host y el host reproduce; si no, el propio con Tempo mientras Run está prendido.
    double bpm = value ("tempo"), ppq = 0; bool hostPlaying = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto q = pos->getPpqPosition()) ppq = *q;
            hostPlaying = pos->getIsPlaying();
            if (value ("sync") > 0.5f && hostPlaying) if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
        }
    const bool useHost = value ("sync") > 0.5f && hostPlaying;
    const bool running = useHost || value ("run") > 0.5f;
    if (wasBypassed) { engine.sequencer().reset(); wasBypassed = false; }
    if (! useHost) { ppq = freeClock; if (running) freeClock += (double) n * bpm / 60.0 / sr; else freeClock = 0; }
    // MIDI de entrada.
    midiIn.clear(); midiOut.clear();
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        const int at = juce::jlimit (0, n - 1, m.samplePosition);
        if (msg.isNoteOn()) midiIn.push_back ({ at, msg.getNoteNumber(), msg.getVelocity(), false });
        else if (msg.isAllSoundOff()) { engine.allSoundOff(); engine.flushMidi (&midiOut, at); }
        else if (msg.isAllNotesOff()) engine.flushMidi (&midiOut, at);
    }
    for (int s = 0; s < kNumSlots; ++s)
        if (const int v = auditionVel[(size_t) s].exchange (0); v > 0) midiIn.insert (midiIn.begin(), { 0, slotToNote (s, (int) value ("note_map")), v, false });
    // Buses: los que el host no activó quedan en nullptr (el motor los suma al principal).
    for (int b = 0; b < kBuses; ++b)
    {
        auto bb = getBusBuffer (buffer, false, b);
        const bool ok = bb.getNumChannels() >= 2;
        outPtr[(size_t) (2 * b)] = ok ? bb.getWritePointer (0) : nullptr;
        outPtr[(size_t) (2 * b + 1)] = ok ? bb.getWritePointer (1) : nullptr;
    }
    HostTime ht; ht.playing = running; ht.ppq = ppq; ht.bpm = bpm;
    engine.process (outPtr.data(), n, ht, midiIn.data(), (int) midiIn.size(), &midiOut);
    midi.clear();
    for (const auto& e : midiOut)
    {
        const int at = juce::jlimit (0, n - 1, e.offset);
        const int ch = juce::jlimit (1, 16, e.channel);
        midi.addEvent (e.vel > 0 ? juce::MidiMessage::noteOn (ch, e.note, (juce::uint8) e.vel) : juce::MidiMessage::noteOff (ch, e.note), at);
    }
    latency.request (engine.latency());
    // Vista.
    view.running.store (running); view.ppq.store (ppq); view.bpm.store (bpm);
    if (const auto* sg = running ? engine.sequencer().segmentAt (ppq) : nullptr)
    {
        view.pattern.store (sg->pat); view.iter.store (sg->iter); view.fill.store (sg->fill);
        view.entry.store (sg->mode == 3 ? sg->entry : -1); view.rep.store (sg->rep); view.segStart.store (sg->start); view.segEnd.store (sg->end);
        for (int t = 0; t < kNumSlots; ++t) view.step[(size_t) t].store (engine.sequencer().stepOf (*sg, t, ppq));
    }
    else { view.pattern.store (-1); view.entry.store (-1); for (auto& s : view.step) s.store (-1); }
    for (int t = 0; t < kNumSlots; ++t)
    {
        const float pk = engine.slotPeak[(size_t) t]; engine.slotPeak[(size_t) t] = 0.0f;
        float cur = view.peak[(size_t) t].load();
        while (pk > cur && ! view.peak[(size_t) t].compare_exchange_weak (cur, pk)) {}
    }
    for (int t = 0; t < kNumSlots; ++t) view.trig[(size_t) t].store (engine.trigCount[(size_t) t]);
    view.gr.store (engine.masterGrDb());
    blockCount.fetch_add (1);
}

void PallorProcessor::audition (int slot, float velocity)
{
    if (slot >= 0 && slot < kNumSlots) auditionVel[(size_t) slot].store (juce::jlimit (1, 127, (int) std::lround (velocity * 127.0f)));
}

void PallorProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear(); midiOut.clear();
    engine.flushMidi (&midiOut, 0);
    for (const auto& e : midiOut) midi.addEvent (juce::MidiMessage::noteOff (juce::jlimit (1, 16, e.channel), e.note), 0);
    wasBypassed = true;
    blockCount.fetch_add (1);
}

// ── Muestras propias ──
void PallorProcessor::publishSample (int slot, std::shared_ptr<SampleData> s, const juce::String& path)
{
    releaseRetired();
    auto old = userSamples[(size_t) slot].data;
    userSamples[(size_t) slot] = { s, path };
    userPtr[(size_t) slot].store (s.get());
    if (old) retired.push_back ({ old, blockCount.load() });
}
void PallorProcessor::releaseRetired()
{
    // Se libera cuando el audio ya procesó dos bloques desde el reemplazo (el motor ya soltó el puntero viejo).
    const uint64_t now = blockCount.load();
    retired.erase (std::remove_if (retired.begin(), retired.end(), [now] (const auto& r) { return now >= r.second + 2; }), retired.end());
}

static std::shared_ptr<SampleData> readSample (juce::AudioFormatReader& r, const juce::String& name, int maxSeconds)
{
    const auto frames = (int) std::min<juce::int64> (r.lengthInSamples, (juce::int64) (r.sampleRate * maxSeconds));
    if (frames <= 0 || r.sampleRate < 1000.0) return nullptr;
    juce::AudioBuffer<float> buf ((int) std::max (1u, r.numChannels), frames);
    r.read (&buf, 0, frames, 0, true, true);
    auto s = std::make_shared<SampleData>();
    s->data.resize ((size_t) frames);
    for (int i = 0; i < frames; ++i)
    {
        float v = 0.0f; for (int c = 0; c < buf.getNumChannels(); ++c) v += buf.getSample (c, i);
        v /= (float) buf.getNumChannels();
        s->data[(size_t) i] = dh::isFiniteF (v) ? juce::jlimit (-1.0f, 1.0f, v) : 0.0f;
    }
    s->rate = r.sampleRate; s->chain = kChainClean; s->name = name.toStdString();
    return s;
}

bool PallorProcessor::loadUserSample (int slot, const juce::File& file, juce::String& error)
{
    if (slot < 0 || slot >= kNumSlots) return false;
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (file));
    if (r == nullptr) { error = "Not a readable audio file: " + file.getFileName(); return false; }
    auto s = readSample (*r, file.getFileNameWithoutExtension(), kMaxSampleSeconds);
    if (s == nullptr) { error = "The file is empty or its sample rate is not supported."; return false; }
    publishSample (slot, s, file.getFullPathName());
    setValue (params::vid (slot, "engine"), 1.0f);
    setValue (params::vid (slot, "kit"), (float) kitgen::numKits());
    return true;
}
void PallorProcessor::clearUserSample (int slot)
{
    if (slot < 0 || slot >= kNumSlots) return;
    publishSample (slot, nullptr, {});
    if ((int) value (params::vid (slot, "kit")) >= kitgen::numKits()) setValue (params::vid (slot, "kit"), 0.0f);
}
juce::String PallorProcessor::userSampleName (int slot) const
{
    const auto& u = userSamples[(size_t) juce::jlimit (0, kNumSlots - 1, slot)];
    return u.data ? juce::String (u.data->name) : juce::String();
}

juce::ValueTree PallorProcessor::samplesToTree() const
{
    juce::ValueTree t ("SAMPLES");
    int budget = kEmbedBudgetBytes;
    for (int s = 0; s < kNumSlots; ++s)
    {
        const auto& u = userSamples[(size_t) s];
        if (! u.data) continue;
        juce::ValueTree c ("SAMPLE");
        c.setProperty ("slot", s, nullptr); c.setProperty ("name", juce::String (u.data->name), nullptr);
        c.setProperty ("path", u.path, nullptr); c.setProperty ("rate", u.data->rate, nullptr);
        // FLAC de 16 bits en memoria: la muestra viaja con la sesión (otra máquina la abre igual).
        juce::MemoryBlock mb;
        {
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::MemoryOutputStream> (mb, false);
            juce::FlacAudioFormat flac;
            const double rate = juce::jlimit (8000.0, 192000.0, u.data->rate);
            if (auto w = flac.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (rate).withNumChannels (1).withBitsPerSample (16)))
            {
                const float* ch[1] = { u.data->data.data() };
                w->writeFromFloatArrays (ch, 1, (int) u.data->data.size());
            }
        }
        if ((int) mb.getSize() <= budget && mb.getSize() > 0) { c.setProperty ("flac", mb.toBase64Encoding(), nullptr); budget -= (int) mb.getSize(); }
        else c.setProperty ("embedded", false, nullptr);
        t.appendChild (c, nullptr);
    }
    return t;
}

void PallorProcessor::samplesFromTree (const juce::ValueTree& t)
{
    for (int s = 0; s < kNumSlots; ++s) if (userSamples[(size_t) s].data) publishSample (s, nullptr, {});
    if (! t.isValid()) return;
    for (const auto& c : t)
    {
        const int s = c.getProperty ("slot", -1);
        if (s < 0 || s >= kNumSlots) continue;
        std::shared_ptr<SampleData> data;
        const juce::String path = c.getProperty ("path").toString();
        if (c.hasProperty ("flac"))
        {
            juce::MemoryBlock mb;
            if (mb.fromBase64Encoding (c.getProperty ("flac").toString()))
            {
                juce::FlacAudioFormat flac;
                std::unique_ptr<juce::AudioFormatReader> r (flac.createReaderFor (new juce::MemoryInputStream (mb, true), true));
                if (r != nullptr) data = readSample (*r, c.getProperty ("name").toString(), kMaxSampleSeconds);
            }
        }
        if (data == nullptr && path.isNotEmpty() && juce::File (path).existsAsFile())
        {
            juce::AudioFormatManager fm; fm.registerBasicFormats();
            std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (path)));
            if (r != nullptr) data = readSample (*r, c.getProperty ("name").toString(), kMaxSampleSeconds);
        }
        if (data != nullptr) publishSample (s, data, path);
    }
}

// ── Estado: parámetros, patrones, cadena, muestras ──
juce::ValueTree PallorProcessor::bankToTree() const
{
    juce::ValueTree t ("BANK");
    const juce::SpinLock::ScopedLockType l (bankLock);
    for (int i = 0; i < kPatterns; ++i)
    {
        const auto& p = bank.p[(size_t) i];
        if (p.isEmpty() && p.length == 16 && p.scale == 1 && p.swing == 0) continue;
        juce::ValueTree c ("PAT");
        c.setProperty ("i", i, nullptr); c.setProperty ("length", (int) p.length, nullptr); c.setProperty ("scale", (int) p.scale, nullptr);
        if (p.swing >= 50) c.setProperty ("swing", (int) p.swing, nullptr);
        for (int k = 0; k < kTracks; ++k)
        {
            const auto& tr = p.t[(size_t) k];
            bool any = false; for (const auto& s : tr.s) any |= s.vel > 0;
            if (! any && ! tr.len) continue;
            juce::ValueTree tt ("T");
            tt.setProperty ("k", params::voiceKey (k), nullptr);
            if (tr.len) tt.setProperty ("len", (int) tr.len, nullptr);
            tt.setProperty ("s", pio::trackToText (tr, 0), nullptr);
            c.appendChild (tt, nullptr);
        }
        t.appendChild (c, nullptr);
    }
    juce::String chain;
    for (int i = 0; i < bank.chainLen; ++i) { const auto& e = bank.chain[(size_t) i]; chain << (int) e.group << "," << (int) e.var << "," << (int) e.repeats << "," << (e.fill ? 1 : 0) << ";"; }
    t.setProperty ("chain", chain, nullptr);
    return t;
}

void PallorProcessor::bankFromTree (const juce::ValueTree& t)
{
    // En el heap: un banco son ~400 KB y esto puede correr en un hilo del host con pila chica (512 KB en macOS).
    auto bp = std::make_unique<Bank>(); Bank& b = *bp;
    if (t.isValid())
    {
        for (const auto& c : t)
        {
            if (! c.hasType ("PAT")) continue;
            const int i = c.getProperty ("i", -1);
            if (i < 0 || i >= kPatterns) continue;
            Pattern p; p.length = (uint8_t) juce::jlimit (1, kMaxSteps, (int) c.getProperty ("length", 16)); p.scale = (uint8_t) juce::jlimit (0, 4, (int) c.getProperty ("scale", 1));
            { const int sw = (int) c.getProperty ("swing", 0); p.swing = (uint8_t) (sw >= 50 ? juce::jmin (75, sw) : 0); }
            for (const auto& tt : c)
            {
                const juce::String key = tt.getProperty ("k").toString();
                for (int k = 0; k < kTracks; ++k)
                    if (key == params::voiceKey (k)) { pio::trackFromText (p.t[(size_t) k], tt.getProperty ("s").toString()); p.t[(size_t) k].len = (uint8_t) juce::jlimit (0, kMaxSteps, (int) tt.getProperty ("len", 0)); }
            }
            b.p[(size_t) i] = p;
        }
        juce::StringArray es; es.addTokens (t.getProperty ("chain").toString(), ";", ""); es.removeEmptyStrings();
        for (const auto& e : es)
        {
            if (b.chainLen >= kMaxChain) break;
            juce::StringArray f; f.addTokens (e, ",", "");
            if (f.size() < 3) continue;
            b.chain[(size_t) b.chainLen++] = { (uint8_t) juce::jlimit (0, kGroups - 1, f[0].getIntValue()), (uint8_t) juce::jlimit (0, 1, f[1].getIntValue()),
                                               (uint8_t) juce::jlimit (1, 64, f[2].getIntValue()), f.size() > 3 && f[3].getIntValue() != 0 };
        }
    }
    { const juce::SpinLock::ScopedLockType l (bankLock); bank = b; }
    patDirty = ~0ull; chainDirty = true;
}

void PallorProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPresetName.get(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetSnapshot", tracker.toString(), nullptr);
    state.removeChild (state.getChildWithName ("BANK"), nullptr);
    state.removeChild (state.getChildWithName ("SAMPLES"), nullptr);
    state.appendChild (bankToTree(), nullptr);
    state.appendChild (samplesToTree(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void PallorProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || state.getType() != apvts.state.getType()) return;
    const auto bk = state.getChildWithName ("BANK").createCopy();
    const auto sm = state.getChildWithName ("SAMPLES").createCopy();
    state.removeChild (state.getChildWithName ("BANK"), nullptr);
    state.removeChild (state.getChildWithName ("SAMPLES"), nullptr);
    apvts.replaceState (state);
    for (auto* prm : getParameters())
        if (auto* pb = dynamic_cast<juce::AudioParameterBool*> (prm)) pb->setValueNotifyingHost (pb->get() ? 1.0f : 0.0f);
    currentPresetName = state.getProperty ("preset", "Init").toString();
    currentProgram = juce::jlimit (0, std::max (0, (int) presets.size() - 1), (int) state.getProperty ("program", 0));
    if (state.hasProperty ("presetSnapshot")) tracker.fromString (*this, state.getProperty ("presetSnapshot").toString());
    else tracker.capture (*this);
    bankFromTree (bk);
    samplesFromTree (sm);
    patternsDirty = true; presetDirty = true; triggerAsyncUpdate();
}

void PallorProcessor::handleAsyncUpdate()
{
    if (presetDirty.exchange (false) && onPresetChanged) onPresetChanged();
    if (patternsDirty.exchange (false) && onPatternsChanged) onPatternsChanged();
}

// ── Presets y biblioteca ──
static Pattern patternFrom (const juce::var& v) { return pio::patternFromVar (v); }

void PallorProcessor::loadFactory()
{
    presets.clear(); library.clear();
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String name (BinaryData::namedResourceList[i]);
        if (! name.endsWith ("_json")) continue;
        int size = 0;
        const char* d = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        auto v = juce::JSON::parse (juce::String::fromUTF8 (d, size));
        if (name == "library_json" && v.isArray())
        {
            for (const auto& s : *v.getArray())
            {
                PatternSet ps;
                ps.name = s.getProperty ("name", "").toString(); ps.genre = s.getProperty ("genre", "").toString();
                ps.tempo = (float) s.getProperty ("tempo", 120.0); ps.swing = (float) s.getProperty ("swing", 50.0); ps.swingRes = (int) s.getProperty ("swing_res", 1);
                ps.a = patternFrom (s.getProperty ("A", {})); ps.b = patternFrom (s.getProperty ("B", {})); ps.fill = patternFrom (s.getProperty ("Fill", {}));
                library.push_back (ps);
            }
            continue;
        }
        if (v.isObject() && v.hasProperty ("params")) presets.push_back ({ v.getProperty ("name", name).toString(), v.getProperty ("category", "").toString(), v.getProperty ("description", "").toString(), v, true, {} });
    }
    std::sort (presets.begin(), presets.end(), [] (const PresetEntry& a, const PresetEntry& b) { return (int) a.data.getProperty ("order", 999) < (int) b.data.getProperty ("order", 999); });
    rescanUserPresets();
}

juce::File PallorProcessor::userPresetDir()
{
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/Audio Tools/Pallor/Presets");
   #else
    return base.getChildFile ("Audio Tools/Pallor/Presets");
   #endif
}

void PallorProcessor::rescanUserPresets()
{
    presets.erase (std::remove_if (presets.begin(), presets.end(), [] (const PresetEntry& p) { return ! p.factory; }), presets.end());
    const auto dir = userPresetDir();
    if (! dir.isDirectory()) return;
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();
    for (auto& f : files)
    {
        const auto v = juce::JSON::parse (f);
        if (v.isObject() && v.hasProperty ("params"))
            presets.push_back ({ v.getProperty ("name", f.getFileNameWithoutExtension()).toString(), "User", v.getProperty ("description", "").toString(), v, false, f });
    }
}

bool PallorProcessor::saveUserPreset (const juce::String& name)
{
    const auto dir = userPresetDir();
    dir.createDirectory();
    auto* o = new juce::DynamicObject(); auto* prm = new juce::DynamicObject();
    o->setProperty ("name", name); o->setProperty ("category", "User");
    static const juce::StringArray session { "run", "sync", "routing", "midi_out", "midi_ch", "note_map", "accent_vel", "midi_gate", "group", "fill" };
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            const auto id = rp->getParameterID();
            if (session.contains (id) || id.endsWith ("_out") || id.endsWith ("_solo")) continue;
            if (std::abs (rp->getValue() - rp->getDefaultValue()) > 1.0e-5f) prm->setProperty (id, rp->convertFrom0to1 (rp->getValue()));
        }
    o->setProperty ("params", juce::var (prm));
    juce::Array<juce::var> pats;
    auto copyP = std::make_unique<Bank>(); Bank& copy = *copyP; { const juce::SpinLock::ScopedLockType l (bankLock); copy = bank; }
    for (int i = 0; i < kPatterns; ++i)
    {
        const auto& p = copy.p[(size_t) i];
        if (p.isEmpty() && p.length == 16 && p.scale == 1 && p.swing == 0) continue;
        auto v = pio::patternToVar (p);
        v.getDynamicObject()->setProperty ("group", i / 3);
        v.getDynamicObject()->setProperty ("var", i % 3 == 0 ? "A" : (i % 3 == 1 ? "B" : "Fill"));
        pats.add (v);
    }
    o->setProperty ("patterns", pats);
    juce::Array<juce::var> chain;
    for (int i = 0; i < copy.chainLen; ++i)
    {
        const auto& e = copy.chain[(size_t) i];
        chain.add (juce::Array<juce::var> { (int) e.group, (int) e.var, (int) e.repeats, e.fill ? 1 : 0 });
    }
    o->setProperty ("chain", chain);
    auto* smp = new juce::DynamicObject();
    for (int s = 0; s < kNumSlots; ++s) if (userSamples[(size_t) s].path.isNotEmpty()) smp->setProperty (params::voiceKey (s), userSamples[(size_t) s].path);
    o->setProperty ("samples", juce::var (smp));
    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".json");
    const bool ok = file.replaceWithText (juce::JSON::toString (juce::var (o)));
    rescanUserPresets();
    currentPresetName = name;
    for (int i = 0; i < (int) presets.size(); ++i) if (! presets[(size_t) i].factory && presets[(size_t) i].file == file) currentProgram = i;
    tracker.capture (*this);
    presetDirty = true; triggerAsyncUpdate();
    return ok;
}

bool PallorProcessor::deleteUserPreset (int index)
{
    if (index < 0 || index >= (int) presets.size() || presets[(size_t) index].factory) return false;
    const bool ok = presets[(size_t) index].file.deleteFile();
    rescanUserPresets();
    currentProgram = juce::jlimit (0, std::max (0, (int) presets.size() - 1), currentProgram);
    presetDirty = true; triggerAsyncUpdate();
    return ok;
}

int PallorProcessor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void PallorProcessor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String PallorProcessor::getProgramName (int index) { return index >= 0 && index < (int) presets.size() ? presets[(size_t) index].name : juce::String ("Init"); }

void PallorProcessor::loadPreset (int index, bool sounds, bool patterns)
{
    if (index < 0 || index >= (int) presets.size()) return;
    const auto& pr = presets[(size_t) index];
    const auto pv = pr.data.getProperty ("params", {});
    if (sounds)
    {
        // Todo a su valor por defecto salvo los ajustes de sesión (transporte, salidas, MIDI) que el preset no nombra.
        static const juce::StringArray keep { "run", "sync", "routing", "midi_out", "midi_ch", "note_map", "accent_vel", "midi_gate", "group", "fill" };
        for (auto* param : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            {
                const auto id = rp->getParameterID();
                if ((keep.contains (id) || id.endsWith ("_out")) && ! pv.hasProperty (id)) continue;
                rp->setValueNotifyingHost (rp->getDefaultValue());
            }
        if (auto* obj = pv.getDynamicObject())
            for (auto& kv : obj->getProperties()) setValue (kv.name.toString(), (float) kv.value);
        // Presets propios: las muestras por su ruta (si el archivo sigue ahí).
        if (auto* so = pr.data.getProperty ("samples", {}).getDynamicObject())
            for (int s = 0; s < kNumSlots; ++s)
                if (so->hasProperty (params::voiceKey (s)))
                {
                    const juce::File f (so->getProperty (params::voiceKey (s)).toString());
                    juce::String err;
                    if (f.existsAsFile()) loadUserSample (s, f, err);
                }
    }
    if (patterns)
    {
        auto bp = std::make_unique<Bank>(); Bank& b = *bp;
        const auto pats = pr.data.getProperty ("patterns", {});
        if (auto* arr = pats.getArray())
            for (const auto& e : *arr)
            {
                const int g = juce::jlimit (0, kGroups - 1, (int) e.getProperty ("group", 0));
                const juce::String var = e.getProperty ("var", "A").toString();
                const int vi = var == "B" ? 1 : (var == "Fill" ? 2 : 0);
                b.p[(size_t) patIndex (g, vi)] = patternFrom (e);
            }
        const auto ch = pr.data.getProperty ("chain", {});
        if (auto* arr = ch.getArray())
            for (const auto& e : *arr)
                if (auto* f = e.getArray(); f != nullptr && f->size() >= 3 && b.chainLen < kMaxChain)
                    b.chain[(size_t) b.chainLen++] = { (uint8_t) juce::jlimit (0, kGroups - 1, (int) (*f)[0]), (uint8_t) juce::jlimit (0, 1, (int) (*f)[1]),
                                                       (uint8_t) juce::jlimit (1, 64, (int) (*f)[2]), f->size() > 3 && (bool) (*f)[3] };
        { const juce::SpinLock::ScopedLockType l (bankLock); bank = b; }
        patDirty = ~0ull; chainDirty = true;
        setValue ("group", 0.0f);
        patternsDirty = true;
    }
    currentProgram = index; currentPresetName = pr.name;
    tracker.capture (*this);
    presetDirty = true; triggerAsyncUpdate();
}

// ── Exportar un patrón a .mid ──
juce::MidiFile PallorProcessor::patternToMidi (const Bank& bk, int patternIndex, int repeats, float swing, int swingRes, int noteMap, uint32_t seed, double bpm)
{
    Sequencer sq; sq.setBank (&bk);
    SeqSettings st; st.forcePattern = juce::jlimit (0, kPatterns - 1, patternIndex); st.swing = swing; st.swingRes = swingRes; st.seed = seed;
    const double beats = sq.patternBeats (st.forcePattern) * std::max (1, repeats);
    constexpr int ppqn = 960;
    struct E { double t; int note, vel; };
    std::vector<E> evs;
    sq.collect (0.0, beats, bpm, st, [&] (double t, int tr, float v, bool acc) { evs.push_back ({ t, slotToNote (tr, noteMap), acc ? 127 : juce::jlimit (1, 127, (int) std::lround (v * 127.0f)) }); });
    std::stable_sort (evs.begin(), evs.end(), [] (const E& a, const E& b) { return a.t < b.t; });
    juce::MidiMessageSequence seq;
    auto tempo = juce::MidiMessage::tempoMetaEvent ((int) std::lround (60.0e6 / std::max (20.0, bpm))); tempo.setTimeStamp (0); seq.addEvent (tempo);
    auto ts = juce::MidiMessage::timeSignatureMetaEvent (4, 4); ts.setTimeStamp (0); seq.addEvent (ts);
    for (size_t i = 0; i < evs.size(); ++i)
    {
        const double on = evs[i].t * ppqn;
        double off = on + ppqn / 16.0;                                     // fusa: lo justo para una percusión
        for (size_t j = i + 1; j < evs.size(); ++j) if (evs[j].note == evs[i].note) { off = std::min (off, evs[j].t * ppqn); break; }
        auto a = juce::MidiMessage::noteOn (10, evs[i].note, (juce::uint8) evs[i].vel); a.setTimeStamp (on); seq.addEvent (a);
        auto b = juce::MidiMessage::noteOff (10, evs[i].note); b.setTimeStamp (std::max (on + 1.0, off)); seq.addEvent (b);
    }
    seq.updateMatchedPairs();
    juce::MidiFile mf; mf.setTicksPerQuarterNote (ppqn); mf.addTrack (seq);
    return mf;
}

bool PallorProcessor::exportMidi (const juce::File& file, int patternIndex, int repeats)
{
    auto copyP = std::make_unique<Bank>(); Bank& copy = *copyP; { const juce::SpinLock::ScopedLockType l (bankLock); copy = bank; }
    auto mf = patternToMidi (copy, patternIndex, repeats, value ("swing") * 0.01f, (int) value ("swing_res"), (int) value ("note_map"), (uint32_t) value ("seed"), value ("tempo"));
    file.deleteFile();
    juce::FileOutputStream os (file);
    return os.openedOk() && mf.writeTo (os);
}

juce::File PallorProcessor::writeDragMidi (int patternIndex, int repeats)
{
    patternIndex = juce::jlimit (0, kPatterns - 1, patternIndex);
    const juce::String nm = "Pallor " + juce::String (patternIndex / 3 + 1) + (patternIndex % 3 == 0 ? "A" : (patternIndex % 3 == 1 ? "B" : " Fill"));
    const auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Pallor MIDI");
    dir.createDirectory();
    const auto f = dir.getChildFile (nm + ".mid");
    return exportMidi (f, patternIndex, repeats) ? f : juce::File();
}

juce::AudioProcessorEditor* PallorProcessor::createEditor() { return new PallorEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PallorProcessor(); }
