// Render offline de un preset a WAV, sin host: para escuchar y analizar el motor.
//   at101vrender preset.json out.wav [--seq "nota:dur,nota:dur,..."] [--secs N] [--os 4] [--sr 48000] [--bpm 120]
// Si el preset trae arpegio o secuencia, --seq son las teclas que se mantienen (p. ej. "48+55+60:8").
// Nota 0 = silencio. Duración en segundos (gate = 85 % de la duración).
#include <JuceHeader.h>
#include "../../../shared/tools/MidiEvents.h"
#include "ExtParams101.h"
#include "Preset101.h"

using namespace dh;

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("uso: at101vrender preset.json out.wav [--seq ...] [--secs N] [--os N]\n"); return 1; }
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);
    auto opt = [&] (const char* name, juce::String def) { const int i = args.indexOf (name); return i >= 0 && i + 1 < args.size() ? args[i + 1] : def; };

    const juce::File presetFile (juce::File::getCurrentWorkingDirectory().getChildFile (args[0]));
    const juce::File outFile (juce::File::getCurrentWorkingDirectory().getChildFile (args[1]));
    const double sr = opt ("--sr", "48000").getDoubleValue();
    const int os = opt ("--os", "4").getIntValue();
    const double bpm = opt ("--bpm", "120").getDoubleValue();

    const Preset p = presetFromJson (juce::JSON::parse (presetFile));
    const bool generative = p.panel[(size_t) P::arp_mode] > 0.5f || p.panel[(size_t) P::seq_play] > 0.5f;
    const juce::String seq = opt ("--seq", generative ? "48+55+60+63:8" : "36:0.4,36:0.4,48:0.4,36:0.4,39:0.4,36:0.4,43:0.4,46:0.4,60:1.2,0:0.3,67:1.2,0:0.6");

    Engine101 eng;
    eng.prepare (sr, 512, os);
    eng.p = p.panel;
    eng.seq = p.seq;
    auto get = [&] (const char* id) {
        if (p.settings.contains (id)) return (float) p.settings[id];
        const auto* e = findExt (id); return e != nullptr ? e->def : 0.0f;
    };
    applyExtParams (get, eng.st, eng.fx);
    eng.st.bpm = bpm;

    struct Ev { int sample; int note; bool on; };
    std::vector<Ev> evs;
    double t = 0.05;
    for (auto& tok : juce::StringArray::fromTokens (seq, ",", ""))
    {
        const double dur = tok.fromFirstOccurrenceOf (":", false, false).getDoubleValue();
        for (auto& ns : juce::StringArray::fromTokens (tok.upToFirstOccurrenceOf (":", false, false), "+", ""))
            if (const int note = ns.getIntValue(); note > 0)
            { evs.push_back ({ (int) (t * sr), note, true }); evs.push_back ({ (int) ((t + dur * 0.85) * sr), note, false }); }
        t += dur;
    }
    // Con secuencia sola no hacen falta teclas: corre sola.
    if (p.panel[(size_t) P::seq_play] > 0.5f && ! args.contains ("--seq")) evs.clear();
    const double secs = opt ("--secs", juce::String (t + 2.0)).getDoubleValue();
    int totalOverride = 0;
    int total = (int) (secs * sr);
    std::sort (evs.begin(), evs.end(), [] (auto& a, auto& b) { return a.sample < b.sample; });

    std::vector<MidiEv> midiEvs;
    size_t me = 0;
    if (const auto mp = opt ("--midi", ""); mp.isNotEmpty())
    {
        double len = 0.0;
        midiEvs = loadMidiFile (juce::File::getCurrentWorkingDirectory().getChildFile (mp), sr, len);
        if (! args.contains ("--secs")) totalOverride = (int) ((len + 3.0) * sr);
        if (! args.contains ("--seq") && ! args.contains ("--notes")) evs.clear();     // sólo lo que trae el MIDI
    }
    if (totalOverride > 0) total = totalOverride;
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    size_t e = 0;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int block = 64;
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        while (e < evs.size() && evs[e].sample < pos + n)
        {
            if (evs[e].on) eng.keyDown (evs[e].note, 0.85f); else eng.keyUp (evs[e].note);
            ++e;
        }
        while (me < midiEvs.size() && midiEvs[me].sample < pos + n)
        {
            const auto& m = midiEvs[me++].msg;
            if (m.isNoteOn()) eng.keyDown (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff()) eng.keyUp (m.getNoteNumber());
            else if (m.isPitchWheel()) eng.bender = (float) (m.getPitchWheelValue() - 8192) / 8192.0f;
            else if (m.isController() && m.getControllerNumber() == 1) eng.lfoPush = (float) m.getControllerValue() / 127.0f;
        }
        eng.st.ppq = pos / sr * bpm / 60.0;
        eng.numMidiEvents = 0;
        eng.render (out.getWritePointer (0) + pos, out.getWritePointer (1) + pos, n);
    }

    const double cpuSec = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
    const float peak = out.getMagnitude (0, total);
    std::printf ("%s: %.2f s, pico %.3f (%.1f dBFS), CPU %.1f %% de un núcleo\n", p.name.toRawUTF8(), total / sr, peak,
                 20.0 * std::log10 (peak + 1e-9), 100.0 * cpuSec / (total / sr));
    outFile.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (outFile), sr, 2, 24, {}, 0));
    w->writeFromAudioSampleBuffer (out, 0, total);
    return 0;
}
