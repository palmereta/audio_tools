// Render offline de Murmur, sin host:
//   murmurrender preset.json in.wav out.wav [--secs n] [--tail s] [--bpm 120] [--notes "60+64:4,67:2"] [--freeze-at s]
// Pasa el WAV por el preset (los segundos que pida --secs, con silencio si el archivo es más corto, más la cola),
// e imprime la CPU. --notes manda notas MIDI (MIDI Mode Pitch o Gate); --freeze-at activa Freeze en ese segundo.
#include <JuceHeader.h>
#include "ParamsMm.h"

using namespace dh::mm;

int main (int argc, char** argv)
{
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);
    if (args.size() < 3) { std::printf ("uso: murmurrender preset.json in.wav out.wav [--secs n] [--tail s] [--bpm n] [--notes ...] [--freeze-at s]\n"); return 1; }
    auto opt = [&] (const char* name, juce::String def) { const int i = args.indexOf (name); return i >= 0 && i + 1 < args.size() ? args[i + 1] : def; };
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto pv = juce::JSON::parse (cwd.getChildFile (args[0]));
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (cwd.getChildFile (args[1])));
    if (rd == nullptr) { std::printf ("no se pudo leer %s\n", args[1].toRawUTF8()); return 1; }
    const double sr = rd->sampleRate;
    juce::AudioBuffer<float> in (2, (int) rd->lengthInSamples);
    rd->read (&in, 0, (int) rd->lengthInSamples, 0, true, true);
    const double secs = opt ("--secs", juce::String (rd->lengthInSamples / sr)).getDoubleValue(), tail = opt ("--tail", "4").getDoubleValue();
    const int total = (int) ((secs + tail) * sr);

    const auto& ps = params();
    std::vector<float> vals (ps.size());
    const auto* obj = pv.getProperty ("params", {}).getDynamicObject();
    for (size_t k = 0; k < ps.size(); ++k) vals[k] = obj != nullptr && obj->hasProperty (ps[k].id) ? (float) obj->getProperty (ps[k].id) : ps[k].def;
    Engine e; const int block = 256; e.prepare (sr, block); applyParams (vals.data(), e);

    struct Ev { int at; int note; bool on; };
    std::vector<Ev> evs;
    double t = 0.5;
    for (auto& chord : juce::StringArray::fromTokens (opt ("--notes", ""), ",", ""))
    {
        const double len = chord.fromFirstOccurrenceOf (":", false, false).getDoubleValue();
        for (auto& nn : juce::StringArray::fromTokens (chord.upToFirstOccurrenceOf (":", false, false), "+", ""))
        { evs.push_back ({ (int) (t * sr), nn.getIntValue(), true }); evs.push_back ({ (int) ((t + len) * sr), nn.getIntValue(), false }); }
        t += len;
    }
    const double freezeAt = opt ("--freeze-at", "-1").getDoubleValue();
    HostInfo host; host.bpm = opt ("--bpm", "120").getDoubleValue(); host.playing = true;

    juce::AudioBuffer<float> out (2, total);
    std::vector<float> L (block), R (block);
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        for (auto& ev : evs) if (ev.at >= pos && ev.at < pos + n) { if (ev.on) e.noteOn (ev.note, 0.8f); else e.noteOff (ev.note); }
        if (freezeAt >= 0.0 && pos / sr >= freezeAt) e.p.freeze = true;
        for (int i = 0; i < n; ++i)
        {
            const int s = pos + i;
            const bool have = s < in.getNumSamples() && s < (int) (secs * sr);
            L[(size_t) i] = have ? in.getSample (0, s) : 0.0f;
            R[(size_t) i] = have ? in.getSample (std::min (1, in.getNumChannels() - 1), s) : 0.0f;
        }
        e.process (L.data(), R.data(), L.data(), R.data(), n, host);
        out.copyFrom (0, pos, L.data(), n); out.copyFrom (1, pos, R.data(), n);
        host.ppq += n * host.bpm / 60.0 / sr;
    }
    const double cpu = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001 / ((double) total / sr) * 100.0;
    auto f = cwd.getChildFile (args[2]); f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), sr, 2, 24, {}, 0));
    if (w == nullptr) return 1;
    w->writeFromAudioSampleBuffer (out, 0, total);
    float pk = out.getMagnitude (0, total);
    std::printf ("%s: %.1f s, pico %.3f, CPU %.2f %% de un núcleo\n", pv.getProperty ("name", "?").toString().toRawUTF8(), (double) total / sr, pk, cpu);
    return 0;
}
