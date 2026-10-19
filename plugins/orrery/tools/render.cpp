// Render offline de un preset de Orrery, sin host:
//   orreryrender preset.json outdir [--secs 120] [--seed n] [--sr 48000]
//   orreryrender --bench      (CPU del generador solo: 8 carriles densos con ecos, curvas de CC y deriva, 10 minutos)
// Escribe outdir/all.mid (un canal por carril), outdir/laneN.mid (cada carril en canal 1, para tocarlo con
// otro render) y outdir/preview.wav (el sonido de prueba).
#include <JuceHeader.h>
#include "ParamsOr.h"
#include "Preview.h"

using namespace dh::orr;

int main (int argc, char** argv)
{
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);
    if (args.contains ("--bench"))
    {
        Generator g; g.g.run = true; g.g.hostClock = false; g.g.tempo = 64; g.g.seed = 3; g.g.fadeIn = 0;
        const int roles[kLanes] = { Drone, Pad, Melody, Arp, Cloud, Bass, Pulse, Cloud };
        for (int l = 0; l < kLanes; ++l)
        {
            auto& L = g.lanes[(size_t) l];
            L.on = true; L.role = roles[l]; L.channel = l + 1; L.density = 0.8f; L.echoes = l == 4 ? 4 : 0;
            L.ccNum = 74; L.ccShape = CcDrift; L.driftCents = 10; L.loop = l % 2 == 0; L.loopBeats = 13.0f + 4 * l;
        }
        g.prepare (48000.0); std::vector<OutEvent> ev;
        const double secs = 600.0; const int blocks = (int) (secs * 48000 / 256);
        size_t events = 0;
        const double t0 = juce::Time::getMillisecondCounterHiRes();
        for (int b = 0; b < blocks; ++b) { g.process (256, false, 0.0, 120.0, ev); events += ev.size(); }
        const double el = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
        std::printf ("generador, 8 carriles densos, %zu eventos en 10 min: CPU %.4f %% de un núcleo\n", events, 100.0 * el / secs);
        return 0;
    }
    if (args.size() < 2) { std::printf ("uso: orreryrender preset.json outdir [--secs 120] [--seed n]\n"); return 1; }
    auto opt = [&] (const char* name, juce::String def) { const int i = args.indexOf (name); return i >= 0 && i + 1 < args.size() ? args[i + 1] : def; };
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto pv = juce::JSON::parse (cwd.getChildFile (args[0]));
    const auto outDir = cwd.getChildFile (args[1]); outDir.createDirectory();
    const double sr = opt ("--sr", "48000").getDoubleValue(), secs = opt ("--secs", "120").getDoubleValue();

    const auto& ps = params();
    std::vector<float> vals (ps.size());
    const auto* obj = pv.getProperty ("params", {}).getDynamicObject();
    for (size_t k = 0; k < ps.size(); ++k) vals[k] = obj != nullptr && obj->hasProperty (ps[k].id) ? (float) obj->getProperty (ps[k].id) : ps[k].def;
    if (args.contains ("--seed")) vals[(size_t) paramIndex ("seed")] = opt ("--seed", "1").getFloatValue();
    vals[(size_t) paramIndex ("run")] = 1.0f;
    vals[(size_t) paramIndex ("clock")] = 0.0f;

    Generator gen; gen.prepare (sr); applyParams (vals.data(), gen);
    Preview prev; const int block = 256; prev.prepare ((float) sr, block);
    std::vector<OutEvent> ev;
    const int total = (int) (secs * sr);
    juce::AudioBuffer<float> audio (2, total + block); audio.clear();
    struct Rec { double sec; juce::MidiMessage m; int lane; };
    std::vector<Rec> recs;
    std::array<int, kLanes> roles {}; for (int l = 0; l < kLanes; ++l) roles[(size_t) l] = gen.lanes[(size_t) l].role;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < total; pos += block)
    {
        if (pos + block >= total) { gen.g.run = false; }
        gen.process (block, false, 0.0, 120.0, ev);
        int done = 0;
        for (auto& e : ev)
        {
            recs.push_back ({ (pos + e.offset) / sr, juce::MidiMessage (e.data, e.size), e.lane });
            if (e.offset > done) { prev.render (audio.getWritePointer (0) + pos, audio.getWritePointer (1) + pos, done, e.offset, 0.6f, 0.6f); done = e.offset; }
            prev.handle (e, roles);
        }
        prev.render (audio.getWritePointer (0) + pos, audio.getWritePointer (1) + pos, done, block, 0.6f, 0.6f);
    }
    const double cpu = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    const double bpm = gen.g.tempo;
    auto write = [&] (const juce::File& f, int laneFilter, bool toCh1) {
        juce::MidiFile mf; mf.setTicksPerQuarterNote (480);
        juce::MidiMessageSequence tempo; tempo.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60000000.0 / bpm)), 0.0); mf.addTrack (tempo);
        juce::MidiMessageSequence seq;
        for (auto& r : recs)
        {
            if (laneFilter >= 0 && r.lane != laneFilter) continue;
            if (! (r.m.isNoteOnOrOff() || r.m.isController() || r.m.isPitchWheel())) continue;
            auto m = r.m;
            if (toCh1) m.setChannel (1);
            m.setTimeStamp (r.sec * bpm / 60.0 * 480.0);
            seq.addEvent (m);
        }
        seq.updateMatchedPairs(); mf.addTrack (seq);
        f.deleteFile(); juce::FileOutputStream os (f); mf.writeTo (os);
        return seq.getNumEvents();
    };
    std::printf ("%s: %.0f s, %d eventos MIDI, CPU %.2f %% de un núcleo (generador + sonido de prueba)\n",
                 pv.getProperty ("name", "?").toString().toRawUTF8(), secs, (int) recs.size(), 100.0 * cpu / secs);
    write (outDir.getChildFile ("all.mid"), -1, false);
    for (int l = 0; l < kLanes; ++l)
        if (gen.lanes[(size_t) l].on) std::printf ("  carril %d (%s): %d eventos\n", l + 1, roleName (gen.lanes[(size_t) l].role), write (outDir.getChildFile ("lane" + juce::String (l + 1) + ".mid"), l, true));
    const auto wavF = outDir.getChildFile ("preview.wav"); wavF.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (wavF), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (audio, 0, total);
    return 0;
}
