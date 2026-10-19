// Render offline de un preset a WAV, sin host: para escuchar, nivelar y analizar el motor.
//   at42render preset.json out.wav [--secs 10] [--keys 1,2] [--notes "0:2,7:2"] [--os 2] [--sr 48000]
// --keys: teclas drone que quedan apretadas (por defecto 1 y 2, o las del preset); --notes: notas
// del teclado táctil (semitonos desde C3 : segundos).
#include <JuceHeader.h>
#include "../../../shared/tools/MidiEvents.h"
#include "ExtParams42.h"
#include "Preset42.h"

using namespace dh;

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("uso: at42render preset.json out.wav [--secs N] [--keys 1,2] [--notes ...]\n"); return 1; }
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);
    auto opt = [&] (const char* name, juce::String def) { const int i = args.indexOf (name); return i >= 0 && i + 1 < args.size() ? args[i + 1] : def; };
    const juce::File presetFile (juce::File::getCurrentWorkingDirectory().getChildFile (args[0]));
    const juce::File outFile (juce::File::getCurrentWorkingDirectory().getChildFile (args[1]));
    const double sr = opt ("--sr", "48000").getDoubleValue();
    const double secs = opt ("--secs", "10").getDoubleValue();
    const auto pv = juce::JSON::parse (presetFile);
    const Preset42 p = presetFromJson42 (pv);

    Engine42 eng;
    eng.prepare (sr, 512, opt ("--os", "2").getIntValue());
    eng.p = p.panel;
    eng.patch = patchFromCables42 (p.cables);
    auto get = [&] (const char* id) {
        if (p.settings.contains (id)) return (float) p.settings[id];
        const auto* e = findExt42 (id); return e != nullptr ? e->def : 0.0f;
    };
    applyExt42 (get, eng.st, eng.keys);
    eng.keys.seqCount = p.keySeqCount;
    for (int i = 0; i < 16; ++i) eng.keys.seqNotes[(size_t) i] = p.keySeq[(size_t) i];
    eng.joyX = p.panel[(size_t) P42::joy_x] * 2.0f - 1.0f; eng.joyY = 1.0f - p.panel[(size_t) P42::joy_y] * 2.0f;
    for (auto& k : juce::StringArray::fromTokens (opt ("--keys", pv.getProperty ("demoKeys", "1,2").toString()), ",", ""))
        if (const int n = k.getIntValue(); n >= 1 && n <= 6) eng.droneKey[(size_t) n] = true;
    struct Ev { int sample; int note; bool on; };
    std::vector<Ev> evs;
    double t = 0.05;
    for (auto& tok : juce::StringArray::fromTokens (opt ("--notes", pv.getProperty ("demoNotes", "").toString()), ",", ""))
    {
        const double dur = tok.fromFirstOccurrenceOf (":", false, false).getDoubleValue();
        for (auto& ns : juce::StringArray::fromTokens (tok.upToFirstOccurrenceOf (":", false, false), "+", ""))
            if (ns.trim() != "-") { evs.push_back ({ (int) (t * sr), ns.getIntValue(), true }); evs.push_back ({ (int) ((t + dur * 0.9) * sr), ns.getIntValue(), false }); }
        t += dur;
    }
    std::sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.sample < b.sample; });
    int totalOverride = 0;
    int total = (int) (secs * sr);
    std::vector<MidiEv> midiEvs;
    size_t me = 0;
    if (const auto mp = opt ("--midi", ""); mp.isNotEmpty())
    {
        double len = 0.0;
        midiEvs = loadMidiFile (juce::File::getCurrentWorkingDirectory().getChildFile (mp), sr, len);
        if (! args.contains ("--secs")) totalOverride = (int) ((len + 3.0) * sr);
        if (! args.contains ("--seq") && ! args.contains ("--notes")) evs.clear();     // sólo lo que trae el MIDI
        if (! args.contains ("--keys")) for (auto& k : eng.droneKey) k = false;
    }
    if (totalOverride > 0) total = totalOverride;
    juce::AudioBuffer<float> out (2, total); out.clear();
    size_t e = 0;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < total; pos += 64)
    {
        const int n = std::min (64, total - pos);
        while (e < evs.size() && evs[e].sample < pos + n) { if (evs[e].on) eng.keys.press (evs[e].note, 0.85f, false); else eng.keys.release (evs[e].note); ++e; }
        while (me < midiEvs.size() && midiEvs[me].sample < pos + n)
        {
            const auto& m = midiEvs[me++].msg;
            const int nn = m.isNoteOnOrOff() ? m.getNoteNumber() : 0;
            if (m.isNoteOnOrOff() && nn >= 24 && nn <= 29) eng.droneKey[(size_t) (nn - 23)] = m.isNoteOn();     // C1–F1: teclas drone
            else if (m.isNoteOn()) eng.keys.press (nn - 48, m.getFloatVelocity(), nn >= 60);
            else if (m.isNoteOff()) eng.keys.release (nn - 48);
            else if (m.isPitchWheel()) eng.keys.bend = (float) (m.getPitchWheelValue() - 8192) / 8192.0f * 2.0f / 12.0f;
            else if (m.isController() && m.getControllerNumber() == 1) eng.joyY = 1.0f - 2.0f * (float) m.getControllerValue() / 127.0f;
        }
        eng.st.ppq = pos / sr * eng.st.bpm / 60.0;
        eng.render (out.getWritePointer (0) + pos, out.getWritePointer (1) + pos, nullptr, nullptr, n);
    }
    const double cpu = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    const float peak = out.getMagnitude (0, total);
    std::printf ("%s: %.1f s, pico %.3f (%.1f dBFS), CPU %.1f %% de un núcleo\n", p.name.toRawUTF8(), secs, peak, 20.0 * std::log10 (peak + 1e-9), 100.0 * cpu / secs);
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (outFile), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, total);
    return 0;
}
