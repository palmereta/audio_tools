// Render offline de un preset a WAV, sin host: para escuchar, nivelar y analizar el motor.
//   at700render preset.json out.wav [--secs 10] [--notes "60+64:1,62:0.5"] [--midi file.mid] [--run] [--os 2] [--sr 48000]
// --notes: notas MIDI (acordes con +, "-" es silencio) : segundos; --run arranca el secuenciador
// (también lo arranca el preset si trae "run": true).
#include <JuceHeader.h>
#include "../../../shared/tools/MidiEvents.h"
#include "ExtParams700.h"
#include "Preset700.h"

using namespace dh;

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("uso: at700render preset.json out.wav [--secs N] [--notes ...] [--midi file.mid] [--run]\n"); return 1; }
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);
    auto opt = [&] (const char* name, juce::String def) { const int i = args.indexOf (name); return i >= 0 && i + 1 < args.size() ? args[i + 1] : def; };
    const juce::File presetFile (juce::File::getCurrentWorkingDirectory().getChildFile (args[0]));
    const juce::File outFile (juce::File::getCurrentWorkingDirectory().getChildFile (args[1]));
    const double sr = opt ("--sr", "48000").getDoubleValue();
    const double secs = opt ("--secs", "10").getDoubleValue();
    const auto pv = juce::JSON::parse (presetFile);
    const Preset700 p = presetFromJson700 (pv);

    Engine700 eng;
    eng.prepare (sr, 512, opt ("--os", "2").getIntValue());
    eng.p = p.panel;
    eng.patch = patchFromCables700 (p.cables);
    auto get = [&] (const char* id) {
        if (p.settings.contains (id)) return (float) p.settings[id];
        const auto* e = findExt700 (id); return e != nullptr ? e->def : 0.0f;
    };
    applyExt700 (get, eng);
    eng.computePoly();
    if (args.contains ("--run") || (bool) pv.getProperty ("run", false)) eng.seqStart();

    struct Ev { int sample; int note; bool on; };
    std::vector<Ev> evs;
    double t = 0.05;
    for (auto& tok : juce::StringArray::fromTokens (opt ("--notes", pv.getProperty ("demoNotes", "").toString()), ",", ""))
    {
        const double dur = tok.fromFirstOccurrenceOf (":", false, false).getDoubleValue();
        for (auto& ns : juce::StringArray::fromTokens (tok.upToFirstOccurrenceOf (":", false, false), "+", ""))
            if (ns.trim() != "-") { evs.push_back ({ (int) (t * sr), ns.getIntValue(), true }); evs.push_back ({ (int) ((t + dur * 0.92) * sr), ns.getIntValue(), false }); }
        t += dur;
    }
    std::sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.sample < b.sample; });
    int total = (int) (secs * sr);
    std::vector<MidiEv> midiEvs;
    size_t me = 0;
    if (const auto mp = opt ("--midi", ""); mp.isNotEmpty())
    {
        double len = 0.0;
        midiEvs = loadMidiFile (juce::File::getCurrentWorkingDirectory().getChildFile (mp), sr, len);
        if (! args.contains ("--secs")) total = (int) ((len + 3.0) * sr);
        if (! args.contains ("--notes")) evs.clear();
    }
    juce::AudioBuffer<float> out (2, total); out.clear();
    size_t e = 0;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < total; pos += 64)
    {
        const int n = std::min (64, total - pos);
        while (e < evs.size() && evs[e].sample < pos + n) { if (evs[e].on) eng.kb.noteOn (evs[e].note, 0.85f); else eng.kb.noteOff (evs[e].note); ++e; }
        while (me < midiEvs.size() && midiEvs[me].sample < pos + n)
        {
            const auto& m = midiEvs[me++].msg;
            if (m.isNoteOn()) eng.kb.noteOn (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff()) eng.kb.noteOff (m.getNoteNumber());
            else if (m.isPitchWheel()) eng.bender = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
            else if (m.isController() && m.getControllerNumber() == 1) eng.kb.modWheel = (float) m.getControllerValue() / 127.0f;
        }
        eng.st.hostPlaying = true;
        eng.st.ppq = pos / sr * eng.st.bpm / 60.0;
        eng.render (out.getWritePointer (0) + pos, out.getWritePointer (1) + pos, n);
    }
    const double cpu = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    const float peak = out.getMagnitude (0, total);
    std::printf ("%s: %.1f s, pico %.3f (%.1f dBFS), CPU %.1f %% de un núcleo\n", p.name.toRawUTF8(), total / sr, peak, 20.0 * std::log10 (peak + 1e-9), 100.0 * cpu / (total / sr));
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (outFile), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, total);
    return 0;
}
