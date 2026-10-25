// devicentirender: the complete DeVicenti processor playing a preset or a project file (its patterns with a simulated
// host clock, or a .mid file played into it), to WAV. Used for the demos, the CPU figures and the processor tests.
//
//   devicentirender out.wav [--preset "Name" | --project file.json] [--bars 8 | --seconds 16] [--bpm 88] [--sr 48000]
//                   [--block 512] [--tail 4] [--set id=value ...] [--at seconds id=value ...] [--song] [--pattern n]
//                   [--midi file.mid [--with-pattern] [--midi-channels]] [--stems dir] [--focus n]
//   devicentirender --list            the factory presets
//   devicentirender --bench           CPU of every preset (8 bars, 48 kHz, blocks of 512)
//   devicentirender --state-test      state save/restore: same bytes, same sound
//   devicentirender --midi-test       MIDI in and out through the processor (CC map, relative CC, learn, feedback,
//                                     program change, MIDI out latency, standalone clock in/out, bypass)
//
// --tail stops the transport at the end and lets the tails ring. --at moves a parameter at that time (seconds).
// --midi plays the file into the processor (transport stopped unless --with-pattern). The output starts at time 0
// (the plugin latency is removed).
#include "PluginProcessor.h"
#include "../../../shared/tools/MidiEvents.h"

class HostClock : public juce::AudioPlayHead
{
public:
    double bpm = 120.0, sr = 48000.0; int64_t pos = 0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setIsPlaying (playing); p.setTimeInSamples (pos); p.setPpqPosition ((double) pos / sr * bpm / 60.0);
        p.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        return p;
    }
};

static bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, int ch0, double sr, int frames)
{
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (os == nullptr) return false;
    juce::WavAudioFormat wav;
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
    if (w == nullptr) return false;
    const float* chans[2] = { b.getReadPointer (ch0), b.getReadPointer (ch0 + 1) };
    return w->writeFromFloatArrays (chans, 2, frames);
}

static int presetIndex (DeVicentiProcessor& p, const juce::String& name)
{
    for (int i = 0; i < (int) p.getPresets().size(); ++i) if (p.getPresets()[(size_t) i].name.equalsIgnoreCase (name)) return i;
    return -1;
}

struct RenderResult { double cpu = 0; float peak = 0; double voices = 0; };

static RenderResult render (DeVicentiProcessor& p, HostClock& clock, juce::AudioBuffer<float>& out, int frames, int playFrames, int block,
                      const std::vector<dh::MidiEv>& midiEvents, const std::vector<std::tuple<int, juce::String, float>>& ats, std::vector<std::pair<int, juce::MidiMessage>>* midiOut = nullptr)
{
    juce::MidiBuffer midi;
    size_t mi = 0;
    juce::AudioBuffer<float> blk (out.getNumChannels(), block);
    double voiceSum = 0; int voiceBlocks = 0;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < frames; pos += block)
    {
        const int n = std::min (block, frames - pos);
        clock.pos = pos; clock.playing = pos < playFrames;
        for (auto& a : ats) if (std::get<0> (a) >= pos && std::get<0> (a) < pos + n) p.setValue (std::get<1> (a), std::get<2> (a));
        midi.clear();
        while (mi < midiEvents.size() && midiEvents[mi].sample < pos + n) { midi.addEvent (midiEvents[mi].msg, std::max (0, midiEvents[mi].sample - pos)); ++mi; }
        blk.setSize (out.getNumChannels(), n, false, false, true);
        blk.clear();
        p.processBlock (blk, midi);
        voiceSum += p.engineForTests().activeVoices(); ++voiceBlocks;
        if (midiOut) for (const auto m : midi) midiOut->push_back ({ pos + m.samplePosition, m.getMessage() });
        for (int c = 0; c < out.getNumChannels(); ++c) out.copyFrom (c, pos, blk, c, 0, n);
    }
    RenderResult r;
    r.cpu = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0 / ((double) frames / clock.sr) * 100.0;
    r.peak = out.getMagnitude (0, 0, frames);
    r.voices = voiceSum / std::max (1, voiceBlocks);
    return r;
}

static int fails = 0;
static void check (const char* what, bool ok, const juce::String& detail = {})
{
    std::printf ("  %-64s %s  %s\n", what, ok ? "PASS" : "FAIL", detail.toRawUTF8());
    if (! ok) ++fails;
}

static int midiTest()
{
    const double sr = 48000.0; const int blk = 256;
    auto make = [&] () {
        auto p = std::make_unique<DeVicentiProcessor>(); p->setPlayConfigDetails (0, 2, sr, blk); p->prepareToPlay (sr, blk);
        p->editProject ([] (dh::dv::Project& pr) { pr.midi.focus = 0; }, DeVicentiProcessor::dMidi);     // the tests address track 1
        return p; };
    auto runBlocks = [&] (DeVicentiProcessor& p, HostClock* clk, int blocks, std::vector<std::pair<int, juce::MidiMessage>> in, std::vector<std::pair<int, juce::MidiMessage>>* out = nullptr, int64_t startPos = 0) {
        juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m;
        for (int k = 0; k < blocks; ++k)
        {
            const int64_t pos = startPos + (int64_t) k * blk;
            if (clk) { clk->pos = pos; p.setPlayHead (clk); }
            m.clear(); b.clear();
            for (auto& e : in) if (e.first >= pos && e.first < pos + blk) m.addEvent (e.second, (int) (e.first - pos));
            p.processBlock (b, m);
            if (out) for (const auto x : m) out->push_back ({ (int) (pos + x.samplePosition), x.getMessage() });
        }
    };
    using MM = juce::MidiMessage;
    // 1. Fixed CC map: CC 74 on channel 1 (OMNI → focused track 1) sets T1 Cutoff.
    {
        auto p = make();
        runBlocks (*p, nullptr, 2, { { 10, MM::controllerEvent (1, 74, 64) } });
        const float v = p->apvts.getParameter ("t1_cutoff")->getValue();
        check ("CC 74 → T1 Cutoff (absolute)", std::abs (v - 64.0f / 127.0f) < 1e-4f, juce::String (v, 4));
        // 2. Relative two's complement: +3 then −1.
        p->editProject ([] (dh::dv::Project& pr) { pr.midi.ccMode = 1; }, DeVicentiProcessor::dMidi);
        runBlocks (*p, nullptr, 2, { { 10, MM::controllerEvent (1, 74, 3) }, { 300, MM::controllerEvent (1, 74, 127) } });
        const float v2 = p->apvts.getParameter ("t1_cutoff")->getValue();
        check ("relative CC (two's complement): +3 −1 steps", std::abs (v2 - (64.0f + 2.0f) / 127.0f) < 1e-3f, juce::String (v2 * 127.0f, 2));
    }
    // 3. CC to a sound parameter (CC 15 = algorithm) and to an operator ratio step (CC 40 = A ratio).
    {
        auto p = make();
        runBlocks (*p, nullptr, 2, { { 0, MM::controllerEvent (1, 15, 127) }, { 5, MM::controllerEvent (1, 40, 127) } });
        const auto& s = p->engineForTests().project().sounds[0][0];
        check ("CC 15 → algorithm 8, CC 40 → A ratio 16 (stepped table)", std::abs (s[dh::dv::ALGO] - 7.0f) < 0.01f && std::abs (s[dh::dv::A_RATIO] - 16.0f) < 0.01f,
               juce::String (s[dh::dv::ALGO]) + " / " + juce::String (s[dh::dv::A_RATIO]));
    }
    // 4. Per-channel mode: channel 3 → track 3's cutoff.
    {
        auto p = make();
        p->editProject ([] (dh::dv::Project& pr) { pr.midi.inMode = 1; }, DeVicentiProcessor::dMidi);
        runBlocks (*p, nullptr, 2, { { 10, MM::controllerEvent (3, 74, 100) } });
        check ("per-channel mode: CC 74 on channel 3 → T3 Cutoff", std::abs (p->apvts.getParameter ("t3_cutoff")->getValue() - 100.0f / 127.0f) < 1e-4f);
    }
    // 5. Learn.
    {
        auto p = make();
        p->armLearn ("t2_drive");
        runBlocks (*p, nullptr, 2, { { 10, MM::controllerEvent (1, 20, 90) }, { 300, MM::controllerEvent (1, 20, 30) } });
        check ("MIDI learn: CC 20 → T2 Drive", std::abs (p->apvts.getParameter ("t2_drive")->getValue() - 30.0f / 127.0f) < 1e-4f && p->learnedTarget (1, 20) == "t2_drive");
    }
    // 6. Program change selects the pattern.
    {
        auto p = make();
        runBlocks (*p, nullptr, 1, { { 0, MM::programChange (1, 5) } });
        check ("program change 5 → pattern 6", (int) std::lround (p->value ("pattern")) == 6, juce::String (p->value ("pattern")));
    }
    // 7. CC feedback: a parameter moved by the host goes out as its CC.
    {
        auto p = make();
        p->editProject ([] (dh::dv::Project& pr) { pr.midi.ccFeedback = true; }, DeVicentiProcessor::dMidi);
        std::vector<std::pair<int, juce::MidiMessage>> out;
        runBlocks (*p, nullptr, 2, {}, &out);
        const size_t before = out.size();
        p->setValue ("t1_reso", 0.75f);
        runBlocks (*p, nullptr, 40, {}, &out, 512);
        int cc71 = 0, val = -1; for (size_t i = before; i < out.size(); ++i) if (out[i].second.isController() && out[i].second.getControllerNumber() == 71) { ++cc71; val = out[i].second.getControllerValue(); }
        check ("CC feedback: T1 Resonance 0.75 → CC 71 = 95 (once)", cc71 == 1 && val == 95 && before == 0, juce::String (cc71) + " msgs, value " + juce::String (val) + ", primed " + juce::String ((int) before));
    }
    // 8. MIDI out is delayed by the latency: a doubled TONE step at time 0 leaves at sample = latency.
    {
        auto p = make();
        p->editProject ([] (dh::dv::Project& pr) {
            pr.track[0].machine = dh::dv::kTone; pr.track[0].doubleMidi = true; pr.track[0].outChannel = 2;
            for (auto& pt : pr.bank.p) pt = dh::dv::Pattern();
            auto& s = pr.bank.p[0].t[0].lane[0][0]; s.trig = 1; s.nNotes = 1; s.notes[0] = 60; }, DeVicentiProcessor::dAll, 0xFFFF);
        p->setValue ("pattern", 1);
        HostClock c; c.sr = sr; c.bpm = 120; c.playing = true;
        std::vector<std::pair<int, juce::MidiMessage>> out;
        p->prepareToPlay (sr, blk);
        runBlocks (*p, &c, 8, {}, &out);
        int at = -1; for (auto& e : out) if (e.second.isNoteOn() && e.second.getChannel() == 2) { at = e.first; break; }
        check ("MIDI out of a step at 0 leaves after the latency", at == p->engineLatency(), "at " + juce::String (at) + ", latency " + juce::String (p->engineLatency()));
    }
    // 9. MIDI-machine track: notes on its channel, balanced; MIDI in on it goes thru.
    {
        auto p = make();
        p->editProject ([] (dh::dv::Project& pr) {
            pr.track[1].machine = dh::dv::kMidiMachine; pr.track[1].outChannel = 7; pr.midi.focus = 1;
            for (auto& pt : pr.bank.p) pt = dh::dv::Pattern();
            for (int i = 0; i < 16; i += 4) { auto& s = pr.bank.p[0].t[1].lane[0][(size_t) i]; s.trig = 1; s.nNotes = 1; s.notes[0] = (int8_t) (48 + i); s.len = 6; } }, DeVicentiProcessor::dAll, 0xFFFF);
        p->setValue ("pattern", 1);
        HostClock c; c.sr = sr; c.bpm = 120; c.playing = true;
        std::vector<std::pair<int, juce::MidiMessage>> out;
        runBlocks (*p, &c, (int) (2.2 * sr / blk), { { 1000, MM::noteOn (1, 70, (juce::uint8) 100) }, { 5000, MM::noteOff (1, 70) } }, &out);
        int on = 0, off = 0, thru = 0, other = 0;
        for (auto& e : out) { if (e.second.getChannel() != 7 && ! e.second.isMidiClock()) ++other; if (e.second.isNoteOn()) { ++on; if (e.second.getNoteNumber() == 70) ++thru; } else if (e.second.isNoteOff()) ++off; }
        check ("MIDI machine: 5 steps in 1.1 bars + 1 note thru on channel 7, balanced", on == 6 && off == 6 && thru == 1 && other == 0,
               juce::String (on) + " on, " + juce::String (off) + " off, " + juce::String (other) + " on other channels");
    }
    // 10. Standalone: internal clock sends start + 24 clocks per beat; MIDI clock in drives the sequencer.
    {
        auto p = make(); p->forceStandalone = true;
        p->editProject ([] (dh::dv::Project& pr) { pr.midi.clockOut = true; }, DeVicentiProcessor::dMidi);
        p->setValue ("tempo", 120.0f);
        p->standalonePlay = true;
        std::vector<std::pair<int, juce::MidiMessage>> out;
        runBlocks (*p, nullptr, (int) std::ceil (sr * 0.5 / blk), {}, &out);     // one beat at 120 BPM
        int clocks = 0, starts = 0; for (auto& e : out) { clocks += e.second.isMidiClock(); starts += e.second.isMidiStart(); }
        check ("standalone clock out: start + 24 clocks in one beat (±1)", starts == 1 && std::abs (clocks - 24) <= 1, juce::String (starts) + " start, " + juce::String (clocks) + " clocks");
    }
    {
        auto p = make(); p->forceStandalone = true;
        p->editProject ([] (dh::dv::Project& pr) {
            pr.midi.clockIn = true; pr.track[0].machine = dh::dv::kMidiMachine; pr.track[0].outChannel = 1;
            for (auto& pt : pr.bank.p) pt = dh::dv::Pattern();
            for (int i = 0; i < 16; ++i) { auto& s = pr.bank.p[0].t[0].lane[0][(size_t) i]; s.trig = 1; s.nNotes = 1; s.notes[0] = 60; s.len = 6; } }, DeVicentiProcessor::dAll, 0xFFFF);
        p->setValue ("pattern", 1);
        std::vector<std::pair<int, juce::MidiMessage>> in;
        in.push_back ({ 100, MM::midiStart() });
        const double tick = sr * 60.0 / 120.0 / 24.0;
        for (int k = 0; k < 24 * 8; ++k) in.push_back ({ 100 + (int) std::lround (k * tick), MM::midiClock() });
        std::vector<std::pair<int, juce::MidiMessage>> out;
        runBlocks (*p, nullptr, (int) (sr * 4.0 / blk), in, &out);
        int on = 0; for (auto& e : out) on += e.second.isNoteOn();
        check ("standalone MIDI clock in at 120 BPM: 16ths play (≈ 32 in 2 bars)", on >= 28 && on <= 34, juce::String (on) + " notes");
    }
    // 11. Bypass releases the MIDI notes that are on.
    {
        auto p = make();
        p->editProject ([] (dh::dv::Project& pr) {
            pr.track[0].machine = dh::dv::kMidiMachine; pr.track[0].outChannel = 1;
            for (auto& pt : pr.bank.p) pt = dh::dv::Pattern();
            auto& s = pr.bank.p[0].t[0].lane[0][0]; s.trig = 1; s.nNotes = 1; s.notes[0] = 60; s.len = dh::dv::Step::kTie; }, DeVicentiProcessor::dAll, 0xFFFF);
        p->setValue ("pattern", 1);
        HostClock c; c.sr = sr; c.bpm = 120; c.playing = true;
        std::vector<std::pair<int, juce::MidiMessage>> out;
        runBlocks (*p, &c, 20, {}, &out);
        juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m;
        p->processBlockBypassed (b, m);
        for (const auto x : m) out.push_back ({ 0, x.getMessage() });
        int on = 0, off = 0; for (auto& e : out) { on += e.second.isNoteOn(); off += e.second.isNoteOff(); }
        check ("bypass sends the pending note-offs", on == 1 && off == 1, juce::String (on) + " on, " + juce::String (off) + " off");
    }
    // 12b. Direct jump: pattern 2 starts at once, mid-bar, instead of at the end of pattern 1.
    {
        auto p = make();
        p->editProject ([] (dh::dv::Project& pr) {
            pr.track[0].machine = dh::dv::kMidiMachine; pr.track[0].outChannel = 1;
            for (auto& pt : pr.bank.p) pt = dh::dv::Pattern();
            auto& a = pr.bank.p[0].t[0].lane[0][0]; a.trig = 1; a.nNotes = 1; a.notes[0] = 60; a.len = 3;
            auto& b = pr.bank.p[1].t[0].lane[0][0]; b.trig = 1; b.nNotes = 1; b.notes[0] = 72; b.len = 3; }, DeVicentiProcessor::dAll, 0xFFFF);
        p->setValue ("pattern", 1);
        HostClock c; c.sr = sr; c.bpm = 120; c.playing = true;
        std::vector<std::pair<int, juce::MidiMessage>> out;
        runBlocks (*p, &c, 40, {}, &out);                       // 40 × 256 = 0.21 bar
        p->jumpToPattern (1);
        runBlocks (*p, &c, 40, {}, &out, 40 * blk);
        int at72 = -1; for (auto& e : out) if (e.second.isNoteOn() && e.second.getNoteNumber() == 72) { at72 = e.first; break; }
        const int want = 40 * blk + p->engineLatency();
        check ("direct jump: the new pattern starts in the next block (not at the bar)", at72 >= 0 && std::abs (at72 - want) <= 2,
               "note 72 at " + juce::String (at72) + ", expected ≈ " + juce::String (want) + ", pattern param " + juce::String (p->value ("pattern")));
    }
    // 12. Notes in: a controller plays the focused track (OMNI) with sound; KIT notes 36/38 play lanes.
    {
        auto p = make();
        p->editProject ([] (dh::dv::Project& pr) { pr.midi.focus = 1; pr.track[1].machine = dh::dv::kTone; }, DeVicentiProcessor::dMidi | DeVicentiProcessor::dTracks);
        juce::AudioBuffer<float> b (2, blk); juce::MidiBuffer m; float pk = 0;
        for (int k = 0; k < 40; ++k) { m.clear(); b.clear(); if (k == 0) m.addEvent (MM::noteOn (5, 57, (juce::uint8) 110), 0); if (k == 30) m.addEvent (MM::noteOff (5, 57), 0); p->processBlock (b, m); pk = std::max (pk, b.getMagnitude (0, 0, blk)); }
        check ("a note from a controller (channel 5, OMNI → focused TONE track) sounds", pk > 0.01f, juce::String (pk, 3));
    }
    std::printf ("\n%s: %d failure(s)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}

static int stateTest()
{
    const double sr = 48000.0; const int blk = 512;
    auto play = [&] (DeVicentiProcessor& p, int frames) {
        HostClock c; c.sr = sr; c.bpm = p.value ("tempo"); c.playing = true; p.setPlayHead (&c);
        juce::AudioBuffer<float> out (2, frames); out.clear(); juce::MidiBuffer m;
        for (int pos = 0; pos < frames; pos += blk) { const int n = std::min (blk, frames - pos); c.pos = pos; juce::AudioBuffer<float> b (out.getArrayOfWritePointers(), 2, pos, n); m.clear(); p.processBlock (b, m); }
        p.setPlayHead (nullptr);
        return out;
    };
    int bad = 0;
    // Fresh instance saved twice: same bytes; saved, loaded elsewhere, saved: same bytes; fresh = program 0.
    {
        DeVicentiProcessor a; juce::MemoryBlock s1, s2; a.getStateInformation (s1); a.getStateInformation (s2);
        DeVicentiProcessor b; b.setStateInformation (s1.getData(), (int) s1.getSize()); juce::MemoryBlock s3; b.getStateInformation (s3);
        DeVicentiProcessor c; c.setCurrentProgram (0); juce::MemoryBlock s4; c.getStateInformation (s4);
        check ("state: saved twice = same bytes", s1 == s2, juce::String ((int) s1.getSize()) + " bytes"); bad += s1 != s2;
        check ("state: save → load → save = same bytes", s1 == s3); bad += s1 != s3;
        check ("new instance = program 0", s1 == s4 && a.getCurrentProgram() == 0); bad += ! (s1 == s4);
    }
    for (int k : { 0, 7, 21, 40 })
    {
        DeVicentiProcessor a; a.setPlayConfigDetails (0, 2, sr, blk); a.prepareToPlay (sr, blk);
        if (k >= (int) a.getPresets().size()) continue;
        a.setCurrentProgram (k);
        juce::MemoryBlock st; a.getStateInformation (st);
        DeVicentiProcessor b; b.setPlayConfigDetails (0, 2, sr, blk); b.prepareToPlay (sr, blk);
        b.setStateInformation (st.getData(), (int) st.getSize());
        juce::MemoryBlock st2; b.getStateInformation (st2);
        a.prepareToPlay (sr, blk); b.prepareToPlay (sr, blk);
        const auto ra = play (a, (int) (3.0 * sr)), rb = play (b, (int) (3.0 * sr));
        double d = 0; for (int i = 0; i < ra.getNumSamples(); ++i) d = std::max (d, (double) std::abs (ra.getSample (0, i) - rb.getSample (0, i)));
        const bool ok = d < 1e-5 && st == st2;
        check (("program " + juce::String (k) + " (" + a.getProgramName (k) + "): restored = same sound and bytes").toRawUTF8(), ok, juce::String ((int) st.getSize()) + " bytes, max diff " + juce::String (d, 8));
        bad += ok ? 0 : 1;
    }
    std::printf ("\n%s\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (juce::String::fromUTF8 (argv[i]));
    auto opt = [&] (const char* n, juce::String d) { const int i = args.indexOf (n); return i >= 0 && i + 1 < args.size() ? args[i + 1] : d; };
    const auto cwd = juce::File::getCurrentWorkingDirectory();

    if (args.contains ("--list"))
    {
        DeVicentiProcessor p; juce::String cat; std::map<juce::String, int> count;
        for (const auto& pr : p.getPresets())
        {
            if (pr.category != cat) { std::printf ("\n[%s]\n", pr.category.toRawUTF8()); cat = pr.category; }
            std::printf ("  %-28s %s BPM  %s\n", pr.name.toRawUTF8(), pr.data.getProperty ("params", {}).getProperty ("tempo", 88).toString().toRawUTF8(), pr.description.toRawUTF8());
            count[pr.category]++;
        }
        std::printf ("\n%d presets:", (int) p.getPresets().size());
        for (auto& kv : count) std::printf (" %s %d,", kv.first.toRawUTF8(), kv.second);
        std::printf ("\n");
        return 0;
    }
    if (args.contains ("--panel-json"))
    {
        // The front panel table for the Blender script (tools/blender/devicenti.py).
        const auto out = juce::File::getCurrentWorkingDirectory().getChildFile (args[args.indexOf ("--panel-json") + 1]);
        out.replaceWithText (dh::dv::panel::controlsJson());
        std::printf ("%s\n", out.getFullPathName().toRawUTF8());
        return 0;
    }
    if (args.contains ("--midi-test")) return midiTest();
    if (args.contains ("--state-test")) return stateTest();

    if (args.contains ("--no-cull")) dh::dv::Voice::cullInaudible = false;     // null tests against renders made before the culling
    const double sr = opt ("--sr", "48000").getDoubleValue();
    const int block = std::max (1, opt ("--block", "512").getIntValue());

    if (args.contains ("--bench"))
    {
        DeVicentiProcessor p; HostClock clock; clock.sr = sr;
        p.setPlayHead (&clock); p.setPlayConfigDetails (0, 2, sr, block); p.prepareToPlay (sr, block);
        double sum = 0, worst = 0, vsum = 0; juce::String worstName;
        for (int i = 0; i < (int) p.getPresets().size(); ++i)
        {
            p.setCurrentProgram (i);
            clock.bpm = p.value ("tempo");
            const int frames = (int) (sr * 8 * 4 * 60.0 / clock.bpm);
            juce::AudioBuffer<float> out (2, frames);
            p.prepareToPlay (sr, block);
            const auto r = render (p, clock, out, frames, frames, block, {}, {});
            std::printf ("  %-28s %6.2f %% of one core, peak %.2f, %.1f voices on average\n", p.getPresets()[(size_t) i].name.toRawUTF8(), r.cpu, r.peak, r.voices);
            vsum += r.voices;
            sum += r.cpu; if (r.cpu > worst) { worst = r.cpu; worstName = p.getPresets()[(size_t) i].name; }
        }
        std::printf ("average %.2f %%, max %.2f %% (%s), %.1f voices on average\n", sum / std::max (1, (int) p.getPresets().size()), worst, worstName.toRawUTF8(), vsum / std::max (1, (int) p.getPresets().size()));
        return 0;
    }

    if (args.isEmpty() || args[0].startsWith ("--")) { std::printf ("usage: devicentirender out.wav [--preset name | --project f.json] [--bars n | --seconds s] [--bpm n] [--tail s] [--set id=v] [--at s id=v] [--midi f.mid] [--song] [--stems dir]\n       devicentirender --list | --bench | --state-test | --midi-test\n"); return 1; }

    DeVicentiProcessor p; HostClock clock; clock.sr = sr;
    const bool stems = args.contains ("--stems");
    if (stems)
    {
        auto layout = p.getBusesLayout();
        for (int b = 1; b < layout.outputBuses.size(); ++b) layout.outputBuses.getReference (b) = juce::AudioChannelSet::stereo();
        if (! p.setBusesLayout (layout)) std::printf ("could not enable the track outputs\n");
    }
    const int nCh = p.getTotalNumOutputChannels();
    p.setPlayHead (&clock); p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block);
    if (const auto name = opt ("--preset", ""); name.isNotEmpty())
    {
        const int idx = presetIndex (p, name);
        if (idx < 0) { std::printf ("unknown preset: %s\n", name.toRawUTF8()); return 3; }
        p.setCurrentProgram (idx);
    }
    if (const auto pf = opt ("--project", ""); pf.isNotEmpty())
    {
        const auto v = juce::JSON::parse (cwd.getChildFile (pf));
        if (! p.loadPresetVar (v, v.getProperty ("name", "project").toString())) { std::printf ("cannot read %s\n", pf.toRawUTF8()); return 3; }
    }
    if (stems) p.editProject ([] (dh::dv::Project& pr) { for (auto& t : pr.track) t.outBus = 1; }, DeVicentiProcessor::dTracks);
    if (args.contains ("--song")) p.songMode = true;
    if (args.contains ("--midi-channels")) p.editProject ([] (dh::dv::Project& pr) { pr.midi.inMode = 1; }, DeVicentiProcessor::dMidi);
    if (const auto f = opt ("--focus", ""); f.isNotEmpty()) { const int fo = juce::jlimit (1, 4, f.getIntValue()) - 1; p.editProject ([fo] (dh::dv::Project& pr) { pr.midi.focus = fo; }, DeVicentiProcessor::dMidi); }
    for (int i = 0; i < args.size(); ++i)
        if (args[i] == "--set" && i + 1 < args.size()) p.setValue (args[i + 1].upToFirstOccurrenceOf ("=", false, false), args[i + 1].fromFirstOccurrenceOf ("=", false, false).getFloatValue());
    if (const auto pt = opt ("--pattern", ""); pt.isNotEmpty()) p.setValue ("pattern", (float) pt.getIntValue());
    clock.bpm = opt ("--bpm", juce::String (p.value ("tempo"))).getDoubleValue();
    p.setValue ("tempo", (float) clock.bpm);
    std::vector<std::tuple<int, juce::String, float>> ats;
    for (int i = 0; i + 2 < args.size(); ++i)
        if (args[i] == "--at") ats.push_back ({ (int) (args[i + 1].getDoubleValue() * sr), args[i + 2].upToFirstOccurrenceOf ("=", false, false), args[i + 2].fromFirstOccurrenceOf ("=", false, false).getFloatValue() });
    std::vector<dh::MidiEv> midiEvents; double midiLen = 0;
    const bool withMidi = opt ("--midi", "").isNotEmpty();
    if (withMidi) midiEvents = dh::loadMidiFile (cwd.getChildFile (opt ("--midi", "")), sr, midiLen);
    double secs = opt ("--seconds", "0").getDoubleValue();
    if (secs <= 0) secs = midiLen > 0 ? midiLen + 0.5 : opt ("--bars", "8").getDoubleValue() * 4.0 * 60.0 / clock.bpm;
    const double tail = opt ("--tail", "0").getDoubleValue();
    const int playFrames = (withMidi && ! args.contains ("--with-pattern")) ? 0 : (int) (secs * sr);
    const int frames = (int) ((secs + tail) * sr);
    const int lat = p.engineLatency();
    juce::AudioBuffer<float> out (nCh, frames + lat); out.clear();
    std::vector<std::pair<int, juce::MidiMessage>> mo;
    const auto r = render (p, clock, out, frames + lat, playFrames, block, midiEvents, ats, &mo);
    juce::AudioBuffer<float> trimmed (nCh, frames);
    for (int c = 0; c < nCh; ++c) trimmed.copyFrom (c, 0, out, c, lat, frames);
    juce::File outFile = cwd.getChildFile (args[0]);
    if (! writeWav (outFile, trimmed, 0, sr, frames)) { std::printf ("cannot write %s\n", outFile.getFullPathName().toRawUTF8()); return 4; }
    if (stems)
    {
        juce::File dir = cwd.getChildFile (opt ("--stems", "stems")); dir.createDirectory();
        for (int b = 1; b <= 4 && 2 * b + 1 < nCh; ++b) writeWav (dir.getChildFile ("track" + juce::String (b) + ".wav"), trimmed, 2 * b, sr, frames);
    }
    std::printf ("%s: %.1f s, peak %.3f, CPU %.2f %% of one core, latency %d samples, %d MIDI out events\n", outFile.getFileName().toRawUTF8(), frames / sr, trimmed.getMagnitude (0, 0, frames), r.cpu, lat, (int) mo.size());
    return 0;
}
