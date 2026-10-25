#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "StepEdit.h"

using namespace dh::dv;

DeVicentiProcessor::DeVicentiProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Main", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Track 1", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Track 2", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Track 3", juce::AudioChannelSet::stereo(), false)
                          .withOutput ("Track 4", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "DEVICENTI", params::layout())
{
    project = std::make_unique<Project>();
    for (int t = 0; t < kTracks; ++t)
        for (int k = 0; k < params::kTrackParams; ++k) trackPtr.push_back (apvts.getRawParameterValue (params::tid (t, params::trackSuffix (k))));
    const char* g[12] = { "swing", "master", "delay_time", "delay_fb", "reverb_size", "reverb_decay", "tape", "glue", "duck", "limit", "tempo", "pattern" };
    for (int i = 0; i < 12; ++i) gPtr[i] = apvts.getRawParameterValue (g[i]);
    // Fixed CC map: main layer and FUNC layer of every panel control, plus the extras.
    for (const auto& c : panel::controls())
    {
        if (c.cc >= 0 && c.cc < 128) ccTarget[(size_t) c.cc] = c.main;
        if (c.ccFunc >= 0 && c.ccFunc < 128) ccTarget[(size_t) c.ccFunc] = c.func;
    }
    for (const auto& e : panel::extraCcs()) ccTarget[(size_t) e.cc] = e.t;
    for (auto& v : viewStep) v.store (-1);
    midiIn.reserve (1024); midiOut.reserve (4096);
    loadFactory();
    // A new instance is program 0: its patterns loaded and the transport stopped.
    if (! presets.empty()) loadPreset (0);
    tracker.capture (*this);
}

bool DeVicentiProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    if (l.getMainOutputChannelSet() != juce::AudioChannelSet::stereo()) return false;
    for (const auto& b : l.inputBuses) if (! b.isDisabled()) return false;
    for (int i = 1; i < l.outputBuses.size(); ++i) if (! l.outputBuses[i].isDisabled() && l.outputBuses[i] != juce::AudioChannelSet::stereo()) return false;
    return true;
}

void DeVicentiProcessor::setValue (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void DeVicentiProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    engine.prepare (sr, samplesPerBlock);
    dirtyBits = dAll; dirtyPatterns = 0xFFFF;
    copyDirtyToEngine();
    latency.setNow (engine.latency());
    midiDelay.prepare (4096); midiDelay.setDelay (engine.latency());
    hostGap.prepare (sr);
    freeClock = 0.0; wasStandalonePlaying = false; extPlaying = false; extTicks = 0; lastTickAt = -1; sampleCount = 0; clockRunning = false;
    ccPrimed = false;
}

EngineParams DeVicentiProcessor::buildParams() const
{
    EngineParams P;
    for (int t = 0; t < kTracks; ++t)
    {
        auto g = [&] (int k) { const auto* p = trackPtr[(size_t) (t * params::kTrackParams + k)]; return p ? p->load() : 0.0f; };
        auto& a = P.t[(size_t) t];
        a.cutoff = g (0); a.reso = g (1); a.fenv = g (2); a.drive = g (3); a.age = g (4); a.dly = g (5); a.rev = g (6); a.levelDb = g (7) <= -59.9f ? -200.0f : g (7);
        for (int o = 0; o < 4; ++o) a.op[o] = g (8 + o);
        a.ins = g (12);
    }
    P.swing = gPtr[0]->load() * 0.01f; P.masterDb = gPtr[1]->load() <= -59.9f ? -200.0f : gPtr[1]->load();
    P.delayDiv = (int) gPtr[2]->load(); P.delayFb = gPtr[3]->load(); P.revSize = gPtr[4]->load(); P.revDecay = gPtr[5]->load();
    P.tape = gPtr[6]->load(); P.glue = gPtr[7]->load(); P.duck = gPtr[8]->load(); P.limit = gPtr[9]->load();
    P.tempo = gPtr[10]->load(); P.pattern = juce::jlimit (0, kPatterns - 1, (int) std::lround (gPtr[11]->load()) - 1);
    P.fill = fill.load(); P.song = songMode.load(); P.rec = rec.load();
    return P;
}

void DeVicentiProcessor::copyDirtyToEngine()
{
    if (dirtyBits.load() == 0 && dirtyPatterns.load() == 0) return;
    const juce::SpinLock::ScopedTryLockType l (projLock);
    if (! l.isLocked()) return;
    const uint32_t b = dirtyBits.exchange (0), pm = dirtyPatterns.exchange (0);
    auto& E = engine.project();
    for (int t = 0; t < kTracks; ++t) if (b & (dTrack0 << t)) E.sounds[(size_t) t] = project->sounds[(size_t) t];
    if (b & dTracks) E.track = project->track;
    if (b & dPatches) E.patches = project->patches;
    if (b & dChordMem) E.chordMem = project->chordMem;
    if (b & dFx) E.fx = project->fx;
    if (b & dMidi) E.midi = project->midi;
    if (b & dChain) { E.bank.chain = project->bank.chain; E.bank.chainLen = project->bank.chainLen; E.seed = project->seed; }
    for (int i = 0; i < kPatterns; ++i) if (pm & (1u << i)) E.bank.p[(size_t) i] = project->bank.p[(size_t) i];
    if (pm) engine.refreshFreezeLocks();
}

std::unique_ptr<Project> DeVicentiProcessor::projectCopy() const
{
    auto p = std::make_unique<Project>();
    const juce::SpinLock::ScopedLockType l (projLock);
    *p = *project;
    return p;
}

juce::String DeVicentiProcessor::projectJson (bool withMidi) const
{
    auto p = projectCopy();
    return juce::JSON::toString (pio::projectToVar (*p, withMidi), true);
}

// ── Audio ──
void DeVicentiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    buffer.clear();
    if (n == 0) return;
    // A host that stopped calling us (REAPER in bypass) and now comes back: release the notes first.
    if (hostGap.check (getPlayHead(), n, isNonRealtime())) engine.allNotesOff();
    copyDirtyToEngine();
    const EngineParams P = buildParams();
    const auto& M = engine.project().midi;
    const bool standalone = isStandalone();

    midiIn.clear(); midiOut.clear();
    // The panel: panic and all-notes-off requests, notes played on the STEP keys.
    if (panicRequest.exchange (false)) engine.allSoundOff();
    if (notesOffRequest.exchange (false)) engine.allNotesOff();
    {
        int s1, z1, s2, z2; const int k = uiMidiFifo.getNumReady();
        uiMidiFifo.prepareToRead (k, s1, z1, s2, z2);
        for (int i = 0; i < z1 + z2; ++i) { dh::dv::MidiIn e; e.offset = 0; const auto& m = uiMidiBuf[(size_t) (i < z1 ? s1 + i : s2 + i - z1)]; e.d[0] = m[0]; e.d[1] = m[1]; e.d[2] = m[2]; midiIn.push_back (e); }
        uiMidiFifo.finishedRead (z1 + z2);
    }
    juce::MidiBuffer extra;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        const int at = juce::jlimit (0, n - 1, meta.samplePosition);
        const auto* raw = msg.getRawData();
        if (msg.isMidiClock() || msg.isMidiStart() || msg.isMidiStop() || msg.isMidiContinue() || msg.isSongPositionPointer())
        {
            if (! (standalone && M.clockIn)) continue;
            if (msg.isMidiStart()) { extPlaying = true; extTicks = 0; lastTickAt = -1; }
            else if (msg.isMidiContinue()) extPlaying = true;
            else if (msg.isMidiStop()) extPlaying = false;
            else if (msg.isSongPositionPointer()) extTicks = (int64_t) msg.getSongPositionPointerMidiBeat() * 6;
            else
            {
                const int64_t now = sampleCount + at;
                if (lastTickAt >= 0) { const double d = (double) (now - lastTickAt); extTickSamples = extTickSamples <= 0 ? d : extTickSamples + 0.1 * (d - extTickSamples); extBpm = juce::jlimit (20.0, 400.0, 60.0 * sr / (24.0 * extTickSamples)); }
                lastTickAt = now; if (extPlaying) ++extTicks;
            }
            continue;
        }
        if (msg.isProgramChange())
        {
            const int pc = msg.getProgramChangeNumber();
            if (M.pcMode == 0) { if (auto* p = apvts.getParameter ("pattern")) p->setValueNotifyingHost (p->convertTo0to1 ((float) juce::jlimit (1, kPatterns, pc + 1))); }
            else { pendingProgram.store (pc); triggerAsyncUpdate(); }
            continue;
        }
        if (msg.isController())
        {
            const int cc = msg.getControllerNumber();
            if (cc == 120) { engine.allSoundOff(); continue; }
            if (cc == 123) { engine.allNotesOff(); continue; }
            if (cc == 1 || cc == 2 || cc == 11 || cc == 64) { dh::dv::MidiIn e; e.offset = at; e.d[0] = raw[0]; e.d[1] = raw[1]; e.d[2] = raw[2]; midiIn.push_back (e); continue; }
            handleCc (msg.getChannel(), cc, msg.getControllerValue(), at);
            continue;
        }
        if (msg.isNoteOnOrOff() || msg.isPitchWheel() || msg.isChannelPressure() || msg.isAftertouch())
        {
            dh::dv::MidiIn e; e.offset = at; e.d[0] = raw[0]; e.d[1] = msg.getRawDataSize() > 1 ? raw[1] : 0; e.d[2] = msg.getRawDataSize() > 2 ? raw[2] : 0;
            midiIn.push_back (e);
        }
    }

    // Clock: the host's in a DAW; in the standalone the internal one (PLAY) or MIDI clock in.
    HostTime ht; ht.bpm = P.tempo;
    if (standalone)
    {
        if (M.clockIn && lastTickAt >= 0)
        {
            ht.bpm = extBpm; ht.playing = extPlaying;
            const double est = ((double) extTicks + (extTickSamples > 0 ? std::min (0.99, (double) (sampleCount - lastTickAt) / extTickSamples) : 0.0)) / 24.0;
            const double rate = ht.bpm / 60.0 / sr;
            if (! extPlaying) freeClock = est;
            else if (std::fabs (est - freeClock) > 1.0) freeClock = est;
            else freeClock += juce::jlimit (-0.5 * rate, 0.5 * rate, 0.05 * (est - freeClock));
            ht.ppq = freeClock;
            if (extPlaying) freeClock += n * rate;
        }
        else
        {
            ht.playing = standalonePlay.load();
            if (ht.playing && ! wasStandalonePlaying) freeClock = 0.0;
            ht.ppq = freeClock;
            if (ht.playing) freeClock += (double) n * ht.bpm / 60.0 / sr;
        }
    }
    else if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            ht.playing = pos->getIsPlaying();
            if (auto q = pos->getPpqPosition()) ht.ppq = *q;
            if (auto b = pos->getBpm()) ht.bpm = juce::jlimit (20.0, 999.0, *b);
        }

    // Outputs: main and the track buses the host enabled.
    for (int b = 0; b < 5; ++b)
    {
        auto bb = getBusBuffer (buffer, false, b);
        const bool ok = bb.getNumChannels() >= 2;
        outPtr[(size_t) (2 * b)] = ok ? bb.getWritePointer (0) : nullptr;
        outPtr[(size_t) (2 * b + 1)] = ok ? bb.getWritePointer (1) : nullptr;
    }
    EngineParams Pj = P;
    if (const int j = jumpRequest.exchange (-1); j >= 0)
    {
        engine.sequencer().jumpTo (j);
        Pj.pattern = j;
        if (auto* prm = apvts.getParameter ("pattern")) prm->setValueNotifyingHost (prm->convertTo0to1 ((float) (j + 1)));
    }
    if (chordCapture.load (std::memory_order_relaxed))
        for (const auto& e : midiIn) if ((e.d[0] & 0xF0) == 0x90 && e.d[2] > 0) captured[(size_t) (e.d[1] & 127)].store (1, std::memory_order_relaxed);
    engine.process (outPtr.data(), n, ht, Pj, midiIn.data(), (int) midiIn.size(), &midiOut);

    // Audio-thread edits and recording → message thread.
    for (const auto& e : engine.soundEdits) pushEdit (e);
    engine.soundEdits.clear();
    for (const auto& r : engine.recordEvents())
    {
        int s1, z1, s2, z2; recFifo.prepareToWrite (1, s1, z1, s2, z2);
        if (z1 > 0) { recBuf[(size_t) s1] = r; recFifo.finishedWrite (1); }
    }
    // Knob moves on the focused track while recording become locks on the current step.
    if (P.rec && ht.playing)
    {
        const int t = juce::jlimit (0, kTracks - 1, M.focus);
        static const int soundOf[13] = { CUTOFF, RESO, FENV, DRIVE, AGE, SEND_DELAY, SEND_REVERB, -1, A_LEVEL, B_LEVEL, C_LEVEL, D_LEVEL, INS_AMT };
        const float vals[13] = { P.t[(size_t) t].cutoff, P.t[(size_t) t].reso, P.t[(size_t) t].fenv, P.t[(size_t) t].drive, P.t[(size_t) t].age, P.t[(size_t) t].dly, P.t[(size_t) t].rev, 0,
                                 P.t[(size_t) t].op[0], P.t[(size_t) t].op[1], P.t[(size_t) t].op[2], P.t[(size_t) t].op[3], P.t[(size_t) t].ins };
        for (int k = 0; k < 13; ++k)
        {
            if (soundOf[k] < 0) continue;
            if (std::fabs (vals[k] - lastAuto[t][k]) > 1e-4f * std::max (1.0f, std::fabs (vals[k])) && engine.view.pattern >= 0)
            {
                RecEvent r; r.track = t; r.pattern = engine.view.pattern; r.step = engine.view.step[(size_t) (t * kLanes)]; r.lockId = soundOf[k]; r.lockValue = vals[k];
                int s1, z1, s2, z2; recFifo.prepareToWrite (1, s1, z1, s2, z2);
                if (z1 > 0) { recBuf[(size_t) s1] = r; recFifo.finishedWrite (1); }
            }
        }
    }
    for (int t = 0; t < kTracks; ++t) { lastAuto[t][0] = P.t[(size_t) t].cutoff; lastAuto[t][1] = P.t[(size_t) t].reso; lastAuto[t][2] = P.t[(size_t) t].fenv; lastAuto[t][3] = P.t[(size_t) t].drive;
                                        lastAuto[t][4] = P.t[(size_t) t].age; lastAuto[t][5] = P.t[(size_t) t].dly; lastAuto[t][6] = P.t[(size_t) t].rev;
                                        for (int o = 0; o < 4; ++o) lastAuto[t][8 + o] = P.t[(size_t) t].op[o]; lastAuto[t][12] = P.t[(size_t) t].ins; }
    if (editFifo.getNumReady() > 0 || recFifo.getNumReady() > 0) triggerAsyncUpdate();

    // MIDI out: the engine's, CC feedback and (standalone) clock; all delayed by the latency.
    midi.clear();
    for (const auto& e : midiOut)
        midi.addEvent (juce::MidiMessage (e.d[0], e.d[1], e.d[2]), juce::jlimit (0, n - 1, e.offset));
    if (M.ccFeedback) ccFeedback (midi);
    if (standalone && M.clockOut && ! M.clockIn)
    {
        if (ht.playing && ! clockRunning) { midi.addEvent (juce::MidiMessage::midiStart(), 0); clockRunning = true; clockOutNext = std::ceil (ht.ppq * 24.0 - 1e-9) / 24.0; }
        if (! ht.playing && clockRunning) { midi.addEvent (juce::MidiMessage::midiStop(), 0); clockRunning = false; }
        if (clockRunning)
        {
            const double rate = ht.bpm / 60.0 / sr, end = ht.ppq + n * rate;
            while (clockOutNext < end) { midi.addEvent (juce::MidiMessage::midiClock(), juce::jlimit (0, n - 1, (int) std::lround ((clockOutNext - ht.ppq) / rate))); clockOutNext += 1.0 / 24.0; }
        }
    }
    midiDelay.setDelay (getLatencySamples() > 0 ? getLatencySamples() : engine.latency());
    midiDelay.process (midi, n);
    latency.request (engine.latency());
    wasStandalonePlaying = ht.playing;
    sampleCount += n;
    viewPattern.store (engine.view.playing ? engine.view.pattern : -1);
    for (int t = 0; t < kTracks; ++t) viewStep[(size_t) t].store (engine.view.playing ? engine.view.step[(size_t) (t * kLanes)] : -1);
}

void DeVicentiProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    buffer.clear();
    midi.clear();
    midiDelay.flushNoteOffs (midi);
    std::vector<dh::dv::MidiOut> offs; offs.reserve (256);
    engine.flushMidiOut (offs);
    for (const auto& e : offs) midi.addEvent (juce::MidiMessage (e.d[0], e.d[1], 0), 0);
    engine.allNotesOff();
}

// ── CC ──
void DeVicentiProcessor::handleCc (int channel, int cc, int value, int offset)
{
    const auto& M = engine.project().midi;
    int track;
    if (M.inMode == 0) track = M.focus;
    else { track = -1; for (int t = 0; t < kTracks; ++t) if (engine.project().track[(size_t) t].inChannel == channel) track = t; }
    if (track < 0) return;
    // MIDI-machine track: its CCs go out to the external synth.
    if (engine.project().track[(size_t) track].machine == kMidiMachine)
    {
        midiOut.push_back ({ offset, { (uint8_t) (0xB0 | ((engine.project().track[(size_t) track].outChannel - 1) & 15)), (uint8_t) cc, (uint8_t) value }, 3 });
        return;
    }
    ccSent[(size_t) track][(size_t) cc] = (int16_t) value;     // no echo of what the controller just sent
    // Learn.
    {
        const juce::SpinLock::ScopedTryLockType l (learnLock);
        if (l.isLocked())
        {
            if (learnArmed.isNotEmpty()) { learned[(channel - 1) * 128 + cc] = learnArmed; learnArmed.clear(); }
            auto it = learned.find ((channel - 1) * 128 + cc);
            if (it != learned.end()) { applyTargetId (it->second, track, (float) value / 127.0f); return; }
        }
    }
    const auto& T = ccTarget[(size_t) cc];
    if (T.kind == panel::TK::None) return;
    if (M.ccMode == 0) applyTarget (T, track, (float) value / 127.0f, false, 0.0f);
    else
    {
        // Relative: two's complement (1..63 up, 127..65 down) or offset 64 (65.. up, ..63 down). One step = 1/127.
        int d = M.ccMode == 1 ? (value < 64 ? value : value - 128) : value - 64;
        applyTarget (T, track, 0.0f, true, (float) d / 127.0f);
    }
}

float DeVicentiProcessor::currentNorm (const panel::Target& t, int track)
{
    using panel::TK;
    switch (t.kind)
    {
        case TK::Auto: if (auto* p = apvts.getParameter (params::tid (track, t.id))) return p->getValue(); return 0.0f;
        case TK::AutoGlobal: if (auto* p = apvts.getParameter (t.id)) return p->getValue(); return 0.0f;
        case TK::Sound: return toNorm (t.sound, engine.project().sounds[(size_t) track][0][t.sound]);
        case TK::OpRatioStep: return panel::knobFromRatio (engine.project().sounds[(size_t) track][0][opParam (t.op, oRatio)]);
        case TK::OpEnvMacro:
        {
            // Nearest point of the path to the current decay (the macro's main axis).
            const float d = engine.project().sounds[(size_t) track][0][opParam (t.op, oDec)];
            float best = 0.0f, bd = 1e9f;
            for (int i = 0; i <= 50; ++i) { const float u = (float) i / 50.0f; const float e = std::fabs (std::log (panel::envMacro (u).d / std::max (d, 1e-4f))); if (e < bd) { bd = e; best = u; } }
            return best;
        }
        default: return 0.0f;
    }
}

void DeVicentiProcessor::applyTarget (const panel::Target& t, int track, float u, bool relative, float delta)
{
    using panel::TK;
    if (relative) u = juce::jlimit (0.0f, 1.0f, currentNorm (t, track) + delta);
    switch (t.kind)
    {
        case TK::Auto: if (auto* p = apvts.getParameter (params::tid (track, t.id))) p->setValueNotifyingHost (u); break;
        case TK::AutoGlobal: if (auto* p = apvts.getParameter (t.id)) p->setValueNotifyingHost (u); break;
        case TK::Sound:
        {
            const float v = clampParam (t.sound, fromNorm (t.sound, u));
            engine.applySoundEdit (track, 0, t.sound, v); pushEdit ({ track, 0, t.sound, v });
            break;
        }
        case TK::OpRatioStep:
        {
            const float v = panel::ratioFromKnob (u);
            engine.applySoundEdit (track, 0, opParam (t.op, oRatio), v); pushEdit ({ track, 0, opParam (t.op, oRatio), v });
            break;
        }
        case TK::OpEnvMacro:
        {
            const auto e = panel::envMacro (u);
            const int ids[4] = { opParam (t.op, oAtt), opParam (t.op, oDec), opParam (t.op, oSus), opParam (t.op, oRel) };
            const float vs[4] = { e.a, e.d, e.s, e.r };
            for (int k = 0; k < 4; ++k) { engine.applySoundEdit (track, 0, ids[k], vs[k]); pushEdit ({ track, 0, ids[k], vs[k] }); }
            break;
        }
        default: break;
    }
}

void DeVicentiProcessor::applyTargetId (const juce::String& id, int track, float u)
{
    panel::Target t;
    if (id.startsWith ("s:")) { t.kind = panel::TK::Sound; t.sound = paramIndex (id.substring (2).toRawUTF8()); if (t.sound < 0) return; applyTarget (t, track, u, false, 0.0f); return; }
    if (auto* p = apvts.getParameter (id)) { p->setValueNotifyingHost (u); return; }
    if (auto* p = apvts.getParameter (params::tid (track, id.toRawUTF8()))) p->setValueNotifyingHost (u);
}

juce::String DeVicentiProcessor::learnedTarget (int channel, int cc) const
{
    const juce::SpinLock::ScopedLockType l (learnLock);
    auto it = learned.find ((channel - 1) * 128 + cc);
    return it != learned.end() ? it->second : juce::String();
}

void DeVicentiProcessor::ccFeedback (juce::MidiBuffer& out)
{
    // Every mapped CC of every track whose value changed (from the panel, automation, a preset) goes out, so a
    // motorised or LED controller follows. Primed on the first block without sending.
    const auto& M = engine.project().midi;
    for (int t = 0; t < kTracks; ++t)
    {
        if (M.inMode == 0 && t != M.focus) continue;
        const int ch = M.inMode == 0 ? 1 : engine.project().track[(size_t) t].inChannel;
        for (int cc = 0; cc < 128; ++cc)
        {
            const auto& T = ccTarget[(size_t) cc];
            if (T.kind == panel::TK::None || T.kind == panel::TK::Action) continue;
            const int v = juce::jlimit (0, 127, (int) std::lround (currentNorm (T, t) * 127.0f));
            if (ccPrimed && v != ccSent[(size_t) t][(size_t) cc]) out.addEvent (juce::MidiMessage::controllerEvent (ch, cc, v), 0);
            ccSent[(size_t) t][(size_t) cc] = (int16_t) v;
        }
    }
    ccPrimed = true;
}

void DeVicentiProcessor::pushEdit (const SoundEdit& e)
{
    int s1, z1, s2, z2; editFifo.prepareToWrite (1, s1, z1, s2, z2);
    if (z1 > 0) { editBuf[(size_t) s1] = e; editFifo.finishedWrite (1); }
}

// ── message thread ──
void DeVicentiProcessor::applyRecord (const RecEvent& r)
{
    const int pat = juce::jlimit (0, kPatterns - 1, r.pattern);
    editProject ([&] (Project& pr) {
        Pattern& p = pr.bank.p[(size_t) pat];
        auto& tp = p.t[(size_t) juce::jlimit (0, kTracks - 1, r.track)];
        const int L = Sequencer::laneLength (tp, r.lane);
        const int stepI = ((r.step % L) + L) % L;
        Step& s = tp.lane[(size_t) juce::jlimit (0, kLanes - 1, r.lane)][(size_t) stepI];
        if (r.lockId >= 0)
        {
            if (! s.on()) { s = Step(); s.trig = 2; }
            stepedit::setLock (p, r.track, r.lane, stepI, r.lockId, r.lockValue);
            return;
        }
        const int key = r.track * 128 + r.note;
        if (r.isNoteOff)
        {
            auto it = recOpen.find (key);
            if (it == recOpen.end()) return;
            auto& st = p.t[(size_t) r.track].lane[(size_t) r.lane][(size_t) it->second.second];
            int d = ((r.step - it->second.second) % L + L) % L; d = std::max (1, d);
            st.len = (uint16_t) (d * 12);
            recOpen.erase (it);
            return;
        }
        // A second note on the same step in the same pass adds to the chord.
        bool sameStep = false; for (auto& kv : recOpen) if (kv.second.first == pat && kv.second.second == stepI && kv.first / 128 == r.track) sameStep = true;
        const bool kit = pr.track[(size_t) r.track].machine == kKit;
        if (! sameStep || ! s.on()) { s = Step(); s.trig = 1; s.nNotes = 0; }
        if (! kit && s.nNotes < kMaxNotes) s.notes[s.nNotes++] = (int8_t) r.note;
        s.vel = (uint8_t) juce::jlimit (1, 127, r.vel); s.micro = (int8_t) r.micro;
        recOpen[key] = { pat, stepI };
    }, 0, 1u << pat);
}

void DeVicentiProcessor::handleAsyncUpdate()
{
    // Sound edits made on the audio thread (CC): into the project (the engine already has them).
    {
        int s1, z1, s2, z2; const int n = editFifo.getNumReady();
        editFifo.prepareToRead (n, s1, z1, s2, z2);
        const juce::SpinLock::ScopedLockType l (projLock);
        for (int i = 0; i < z1; ++i) { const auto& e = editBuf[(size_t) (s1 + i)]; project->sounds[(size_t) e.track][(size_t) e.lane][e.param] = e.value; }
        for (int i = 0; i < z2; ++i) { const auto& e = editBuf[(size_t) (s2 + i)]; project->sounds[(size_t) e.track][(size_t) e.lane][e.param] = e.value; }
        editFifo.finishedRead (z1 + z2);
    }
    {
        int s1, z1, s2, z2; const int n = recFifo.getNumReady();
        recFifo.prepareToRead (n, s1, z1, s2, z2);
        std::vector<RecEvent> evs;
        for (int i = 0; i < z1; ++i) evs.push_back (recBuf[(size_t) (s1 + i)]);
        for (int i = 0; i < z2; ++i) evs.push_back (recBuf[(size_t) (s2 + i)]);
        recFifo.finishedRead (z1 + z2);
        for (auto& r : evs) applyRecord (r);
    }
    if (const int pc = pendingProgram.exchange (-1); pc >= 0 && pc < (int) presets.size()) loadPreset (pc);
    if (onPresetChanged) onPresetChanged();
}

// ── Interface (message thread) ──
void DeVicentiProcessor::uiMidi (uint8_t status, uint8_t d1, uint8_t d2)
{
    int s1, z1, s2, z2; uiMidiFifo.prepareToWrite (1, s1, z1, s2, z2);
    if (z1 > 0) { uiMidiBuf[(size_t) s1] = { status, d1, d2 }; uiMidiFifo.finishedWrite (1); }
}

float DeVicentiProcessor::uiNorm (const panel::Target& t, int track, int lane) const
{
    lane = juce::jlimit (0, kSoundsPerTrack - 1, lane);
    using panel::TK;
    track = juce::jlimit (0, kTracks - 1, track);
    switch (t.kind)
    {
        case TK::Auto: if (auto* p = apvts.getParameter (params::tid (track, t.id))) return p->getValue(); return 0.0f;
        case TK::AutoGlobal: if (auto* p = apvts.getParameter (t.id)) return p->getValue(); return 0.0f;
        case TK::Sound: return withProject ([&] (const Project& pr) { return toNorm (t.sound, pr.sounds[(size_t) track][(size_t) lane][t.sound]); });
        case TK::OpRatioStep: return withProject ([&] (const Project& pr) { return panel::knobFromRatio (pr.sounds[(size_t) track][(size_t) lane][opParam (t.op, oRatio)]); });
        case TK::OpEnvMacro:
        {
            const float d = withProject ([&] (const Project& pr) { return pr.sounds[(size_t) track][(size_t) lane][opParam (t.op, oDec)]; });
            float best = 0.0f, bd = 1e9f;
            for (int i = 0; i <= 50; ++i) { const float u = (float) i / 50.0f; const float e = std::fabs (std::log (panel::envMacro (u).d / std::max (d, 1e-4f))); if (e < bd) { bd = e; best = u; } }
            return best;
        }
        default: return 0.0f;
    }
}

void DeVicentiProcessor::uiSet (const panel::Target& t, int track, float u, int lane)
{
    lane = juce::jlimit (0, kSoundsPerTrack - 1, lane);
    using panel::TK;
    track = juce::jlimit (0, kTracks - 1, track); u = juce::jlimit (0.0f, 1.0f, u);
    switch (t.kind)
    {
        case TK::Auto: if (auto* p = apvts.getParameter (params::tid (track, t.id))) p->setValueNotifyingHost (u); break;
        case TK::AutoGlobal: if (auto* p = apvts.getParameter (t.id)) p->setValueNotifyingHost (u); break;
        case TK::Sound: setSound (track, lane, t.sound, fromNorm (t.sound, u)); break;
        case TK::OpRatioStep: setSound (track, lane, opParam (t.op, oRatio), panel::ratioFromKnob (u)); break;
        case TK::OpEnvMacro:
        {
            const auto e = panel::envMacro (u);
            editProject ([&] (Project& pr) {
                auto& s = pr.sounds[(size_t) track][(size_t) lane];
                s[opParam (t.op, oAtt)] = e.a; s[opParam (t.op, oDec)] = e.d; s[opParam (t.op, oSus)] = e.s; s[opParam (t.op, oRel)] = e.r;
            }, dTrack0 << track);
            break;
        }
        default: break;
    }
}

void DeVicentiProcessor::uiGesture (const panel::Target& t, int track, bool begin)
{
    juce::RangedAudioParameter* p = nullptr;
    if (t.kind == panel::TK::Auto) p = apvts.getParameter (params::tid (juce::jlimit (0, kTracks - 1, track), t.id));
    else if (t.kind == panel::TK::AutoGlobal) p = apvts.getParameter (t.id);
    if (p == nullptr) return;
    if (begin) p->beginChangeGesture(); else p->endChangeGesture();
}

float DeVicentiProcessor::soundValue (int track, int lane, int param) const
{
    return withProject ([&] (const Project& pr) { return pr.sounds[(size_t) juce::jlimit (0, kTracks - 1, track)][(size_t) juce::jlimit (0, kSoundsPerTrack - 1, lane)][param]; });
}

void DeVicentiProcessor::setSound (int track, int lane, int param, float value)
{
    if (param < 0 || param >= kNumSoundParams) return;
    track = juce::jlimit (0, kTracks - 1, track); lane = juce::jlimit (0, kSoundsPerTrack - 1, lane);
    const float v = clampParam (param, value);
    editProject ([&] (Project& pr) { pr.sounds[(size_t) track][(size_t) lane][param] = v; }, dTrack0 << track);
}

// ── State ──
void DeVicentiProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("preset", currentPresetName.get(), nullptr);
    state.setProperty ("program", currentProgram, nullptr);
    state.setProperty ("presetSnapshot", tracker.toString(), nullptr);
    state.setProperty ("project", projectJson (true), nullptr);
    juce::String lr;
    { const juce::SpinLock::ScopedLockType l (learnLock); for (auto& kv : learned) lr << kv.first << "=" << kv.second << ";"; }
    state.setProperty ("learn", lr, nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary (*xml, destData);
}

void DeVicentiProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid() || state.getType() != apvts.state.getType()) return;
    const juce::String pj = state.getProperty ("project").toString(), lr = state.getProperty ("learn").toString();
    apvts.replaceState (state);
    currentPresetName = state.getProperty ("preset", "Init").toString();
    currentProgram = juce::jlimit (0, std::max (0, (int) presets.size() - 1), (int) state.getProperty ("program", 0));
    if (pj.isNotEmpty()) { const auto v = juce::JSON::parse (pj); editProject ([&] (Project& pr) { pio::projectFromVar (v, pr, true); }, dAll, 0xFFFF); }
    {
        const juce::SpinLock::ScopedLockType l (learnLock); learned.clear();
        juce::StringArray es; es.addTokens (lr, ";", ""); es.removeEmptyStrings();
        for (auto& e : es) learned[e.upToFirstOccurrenceOf ("=", false, false).getIntValue()] = e.fromFirstOccurrenceOf ("=", false, false);
    }
    if (state.hasProperty ("presetSnapshot")) tracker.fromString (*this, state.getProperty ("presetSnapshot").toString());
    else tracker.capture (*this);
    triggerAsyncUpdate();
}

// ── Presets ──
void DeVicentiProcessor::loadFactory()
{
    presets.clear();
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
    {
        const juce::String name (BinaryData::namedResourceList[i]);
        if (! name.endsWith ("_json")) continue;
        int size = 0;
        const char* d = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
        auto v = juce::JSON::parse (juce::String::fromUTF8 (d, size));
        if (v.isObject() && v.hasProperty ("tracks")) presets.push_back ({ v.getProperty ("name", name).toString(), v.getProperty ("category", "").toString(), v.getProperty ("description", "").toString(), v, true, {} });
    }
    std::sort (presets.begin(), presets.end(), [] (const PresetEntry& a, const PresetEntry& b) {
        const int oa = a.data.getProperty ("order", 9999), ob = b.data.getProperty ("order", 9999);
        return oa != ob ? oa < ob : a.name < b.name; });
    rescanUserPresets();
}

juce::File DeVicentiProcessor::userPresetDir()
{
    const auto base = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
   #if JUCE_MAC
    return base.getChildFile ("Application Support/Audio Tools/DeVicenti/Presets");
   #else
    return base.getChildFile ("Audio Tools/DeVicenti/Presets");
   #endif
}

void DeVicentiProcessor::rescanUserPresets()
{
    presets.erase (std::remove_if (presets.begin(), presets.end(), [] (const PresetEntry& p) { return ! p.factory; }), presets.end());
    const auto dir = userPresetDir();
    if (! dir.isDirectory()) return;
    auto files = dir.findChildFiles (juce::File::findFiles, false, "*.json");
    files.sort();
    for (auto& f : files)
    {
        const auto v = juce::JSON::parse (f);
        if (v.isObject() && v.hasProperty ("tracks")) presets.push_back ({ v.getProperty ("name", f.getFileNameWithoutExtension()).toString(), "User", v.getProperty ("description", "").toString(), v, false, f });
    }
}

bool DeVicentiProcessor::saveUserPreset (const juce::String& name)
{
    const auto dir = userPresetDir(); dir.createDirectory();
    auto v = juce::JSON::parse (projectJson (false));
    auto* o = v.getDynamicObject(); if (o == nullptr) return false;
    o->setProperty ("name", name); o->setProperty ("category", "User");
    auto* prm = new juce::DynamicObject();
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param))
            if (std::abs (rp->getValue() - rp->getDefaultValue()) > 1.0e-5f) prm->setProperty (rp->getParameterID(), pio::r6 (rp->convertFrom0to1 (rp->getValue())));
    o->setProperty ("params", juce::var (prm));
    const auto file = dir.getChildFile (juce::File::createLegalFileName (name) + ".json");
    const bool ok = file.replaceWithText (juce::JSON::toString (v));
    rescanUserPresets();
    currentPresetName = name; tracker.capture (*this);
    return ok;
}

bool DeVicentiProcessor::loadPresetVar (const juce::var& v, const juce::String& name)
{
    if (! v.isObject()) return false;
    editProject ([&] (Project& pr) { pio::projectFromVar (v, pr, false); }, dAll, 0xFFFF);
    // Parameters: defaults, then the preset's; the per-track ones follow the project's track sounds.
    const auto pv = v.getProperty ("params", {});
    for (auto* param : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (param)) rp->setValueNotifyingHost (rp->getDefaultValue());
    if (auto* obj = pv.getDynamicObject()) for (auto& kv : obj->getProperties()) setValue (kv.name.toString(), (float) (double) kv.value);
    auto pr = projectCopy();
    for (int t = 0; t < kTracks; ++t)
    {
        const Sound& s = pr->sounds[(size_t) t][0];
        setValue (params::tid (t, "cutoff"), s[CUTOFF]); setValue (params::tid (t, "reso"), s[RESO]); setValue (params::tid (t, "fenv"), s[FENV]);
        setValue (params::tid (t, "drive"), s[DRIVE]); setValue (params::tid (t, "age"), s[AGE]); setValue (params::tid (t, "delay"), s[SEND_DELAY]);
        setValue (params::tid (t, "reverb"), s[SEND_REVERB]); setValue (params::tid (t, "insert"), s[INS_AMT]);
        const char* ops[4] = { "opa", "opb", "opc", "opd" };
        for (int o = 0; o < 4; ++o) setValue (params::tid (t, ops[o]), s[opParam (o, oLevel)]);
    }
    currentPresetName = name;
    tracker.capture (*this);
    return true;
}

void DeVicentiProcessor::loadPreset (int index)
{
    if (index < 0 || index >= (int) presets.size()) return;
    loadPresetVar (presets[(size_t) index].data, presets[(size_t) index].name);
    currentProgram = index;
    triggerAsyncUpdate();
}

int DeVicentiProcessor::getNumPrograms() { return std::max (1, (int) presets.size()); }
void DeVicentiProcessor::setCurrentProgram (int index) { loadPreset (index); }
const juce::String DeVicentiProcessor::getProgramName (int index) { return index >= 0 && index < (int) presets.size() ? presets[(size_t) index].name : juce::String ("Init"); }

juce::AudioProcessorEditor* DeVicentiProcessor::createEditor() { return new DeVicentiEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new DeVicentiProcessor(); }
