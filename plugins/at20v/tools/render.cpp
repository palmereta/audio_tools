// Render offline de un preset a WAV, sin host: para escuchar y analizar el motor.
//   at20vrender preset.json out.wav [--seq "nota:dur,nota:dur,..."] [--rev 1|2] [--os 4] [--sr 48000]
//   [--esp in.wav]   (alimenta la ESP con un archivo, para los presets de guitarra)
// Nota 0 = silencio. Duración en segundos (gate = 85 % de la duración).
#include <JuceHeader.h>
#include "../../../shared/tools/MidiEvents.h"
#include "Preset.h"
#include "ExtParams.h"

using namespace dh;

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("uso: at20vrender preset.json out.wav [--seq ...] [--rev N] [--os N]\n"); return 1; }
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);
    auto opt = [&] (const char* name, juce::String def) { const int i = args.indexOf (name); return i >= 0 && i + 1 < args.size() ? args[i + 1] : def; };

    const juce::File presetFile (juce::File::getCurrentWorkingDirectory().getChildFile (args[0]));
    const juce::File outFile (juce::File::getCurrentWorkingDirectory().getChildFile (args[1]));
    const double sr = opt ("--sr", "48000").getDoubleValue();
    const int os = opt ("--os", "4").getIntValue();
    const juce::String seq = opt ("--seq", "36:0.4,36:0.4,48:0.4,36:0.4,39:0.4,36:0.4,43:0.4,46:0.4,60:1.2,0:0.3,67:1.2,0:0.6");

    Preset p = presetFromJson (juce::JSON::parse (presetFile));
    Engine eng;
    eng.prepare (sr, 512, os);
    eng.knobs = p.knobs;
    eng.patch = patchFromCables (p.cables);
    // Ajustes de extensiones: los del preset, o el valor por defecto.
    auto get = [&] (const char* id) {
        if (p.settings.contains (id)) return (float) p.settings[id];
        const auto* e = findExt (id); return e != nullptr ? e->def : 0.0f;
    };
    applyExtParams (get, eng.settings, eng.fx);
    if (args.contains ("--rev")) eng.settings.filterRev = opt ("--rev", "1").getIntValue();
    if (args.contains ("--voices")) eng.settings.voices = opt ("--voices", "1").getIntValue();

    std::vector<float> espData;
    if (const juce::String espPath = opt ("--esp", ""); espPath.isNotEmpty())
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File::getCurrentWorkingDirectory().getChildFile (espPath)));
        if (r)
        {
            juce::AudioBuffer<float> b (1, (int) r->lengthInSamples);
            r->read (&b, 0, (int) r->lengthInSamples, 0, true, false);
            espData.assign (b.getReadPointer (0), b.getReadPointer (0) + b.getNumSamples());
        }
    }

    struct Ev { int sample; int note; bool on; };
    std::vector<Ev> evs;
    double t = 0.1;
    for (auto& tok : juce::StringArray::fromTokens (seq, ",", ""))
    {
        const double dur = tok.fromFirstOccurrenceOf (":", false, false).getDoubleValue();
        for (auto& ns : juce::StringArray::fromTokens (tok.upToFirstOccurrenceOf (":", false, false), "+", ""))
            if (const int note = ns.getIntValue(); note > 0)
            { evs.push_back ({ (int) (t * sr), note, true }); evs.push_back ({ (int) ((t + dur * 0.85) * sr), note, false }); }
        t += dur;
    }
    int totalOverride = 0;
    int total = (int) ((t + 2.0) * sr);
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
    if (args.contains ("--secs")) total = (int) (opt ("--secs", "10").getDoubleValue() * sr);
    juce::AudioBuffer<float> out (2, total);
    out.clear();
    size_t e = 0;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    const int block = 64;
    std::vector<float> espBlock (block);
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        while (e < evs.size() && evs[e].sample < pos + n)
        {
            if (evs[e].on) eng.noteOn (evs[e].note, 1.0f); else eng.noteOff (evs[e].note);
            ++e;
        }
        while (me < midiEvs.size() && midiEvs[me].sample < pos + n)
        {
            const auto& m = midiEvs[me++].msg;
            if (m.isNoteOn()) eng.noteOn (m.getNoteNumber(), m.getFloatVelocity());
            else if (m.isNoteOff()) eng.noteOff (m.getNoteNumber());
            else if (m.isPitchWheel()) eng.pitchBend ((float) (m.getPitchWheelValue() - 8192) / 8192.0f);
        }
        const float* espPtr = nullptr;
        if (! espData.empty())
        {
            for (int i = 0; i < n; ++i) espBlock[(size_t) i] = pos + i < (int) espData.size() ? espData[(size_t) (pos + i)] : 0.0f;
            espPtr = espBlock.data();
        }
        eng.render (out.getWritePointer (0) + pos, out.getWritePointer (1) + pos, espPtr, n);
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
