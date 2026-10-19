// Render offline de un preset a WAV, sin host, y herramientas para los Fseq de fábrica.
//   atfs1render preset.json out.wav [--secs 10] [--notes "60+64:1,62:0.5"] [--midi file.mid] [--fseqdir dir] [--sr 48000]
//   atfs1render --vowels out.json "Name" "aeiou" [--secs-per 0.4] [--pitch 130] [--female] [--vibrato 0.25] [--breath 0]
//   atfs1render --analyze in.wav out.json "Name" [--loop start end round|oneway]
// --notes: notas MIDI (acordes con +, "-" es silencio) : segundos.
#include <JuceHeader.h>
#include "../../../shared/tools/MidiEvents.h"
#include "SynthFs.h"
#include "PresetFs.h"

using namespace dh;
using namespace dh::fs;

static void writeFseqJson (const FseqData& f, const juce::File& out)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", juce::String (f.name));
    o->setProperty ("frames", (int) f.frames.size());
    o->setProperty ("data", fseqToBase64 (f).fromFirstOccurrenceOf ("|", false, false));
    out.replaceWithText (juce::JSON::toString (juce::var (o), true));
}

int main (int argc, char** argv)
{
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (argv[i]);
    auto opt = [&] (const char* name, juce::String def) { const int i = args.indexOf (name); return i >= 0 && i + 1 < args.size() ? args[i + 1] : def; };
    const auto cwd = juce::File::getCurrentWorkingDirectory();

    if (args[0] == "--vowels" && args.size() >= 4)
    {
        auto f = vowelSequence (args[2].toStdString(), args[3].toStdString(), (float) opt ("--secs-per", "0.4").getDoubleValue(), (float) opt ("--glide", "0.35").getDoubleValue(),
                                (float) opt ("--pitch", "130").getDoubleValue(), args.contains ("--female") ? 1.17f : 1.0f,
                                (float) opt ("--vibrato", "0.25").getDoubleValue(), (float) opt ("--breath", "0").getDoubleValue());
        if (f.frames.size() > 512) f.frames.resize (512);
        f.loopEnd = std::min (f.loopEnd, (int) f.frames.size() - 1); f.loopStart = std::min (f.loopStart, f.loopEnd);
        writeFseqJson (f, cwd.getChildFile (args[1]));
        std::printf ("%s: %d cuadros (%.2f s), lazo %d-%d\n", f.name.c_str(), (int) f.frames.size(), (double) f.frames.size() / kFseqFps, f.loopStart, f.loopEnd);
        return 0;
    }
    if (args[0] == "--analyze" && args.size() >= 4)
    {
        juce::AudioFormatManager fm; fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> rd (fm.createReaderFor (cwd.getChildFile (args[1])));
        if (rd == nullptr) { std::printf ("no se pudo leer %s\n", args[1].toRawUTF8()); return 1; }
        const int len = (int) std::min<juce::int64> (rd->lengthInSamples, (juce::int64) (rd->sampleRate * 4.2));
        juce::AudioBuffer<float> b ((int) rd->numChannels, len);
        rd->read (&b, 0, len, 0, true, true);
        std::vector<float> mono ((size_t) len);
        for (int c = 0; c < b.getNumChannels(); ++c) for (int i = 0; i < len; ++i) mono[(size_t) i] += b.getSample (c, i) / (float) b.getNumChannels();
        auto f = analyze (mono, (float) rd->sampleRate, args[3].toStdString());
        const int li = args.indexOf ("--loop");
        if (li >= 0 && li + 3 < args.size()) { f.loopStart = args[li + 1].getIntValue(); f.loopEnd = args[li + 2].getIntValue(); f.loopMode = args[li + 3] == "round" ? 1 : 0; }
        writeFseqJson (f, cwd.getChildFile (args[2]));
        std::printf ("%s: %d cuadros, nota %d\n", f.name.c_str(), (int) f.frames.size(), f.noteAssign);
        return 0;
    }
    if (argc < 3) { std::printf ("uso: atfs1render preset.json out.wav [--secs N] [--notes ...] [--midi file.mid]\n"); return 1; }

    const juce::File presetFile (cwd.getChildFile (args[0]));
    const juce::File outFile (cwd.getChildFile (args[1]));
    const double sr = opt ("--sr", "48000").getDoubleValue();
    const double secs = opt ("--secs", "10").getDoubleValue();
    const auto pv = juce::JSON::parse (presetFile);
    const Preset p = presetFromJson (pv);
    FseqData fseq = p.fseqInline;
    if (p.fseqRef.isNotEmpty())
    {
        const juce::File dir = cwd.getChildFile (opt ("--fseqdir", presetFile.getParentDirectory().getSiblingFile ("fseq").getFullPathName()));
        for (auto& f : dir.findChildFiles (juce::File::findFiles, false, "*.json"))
        {
            const auto v = juce::JSON::parse (f);
            if (v.getProperty ("name", "").toString() == p.fseqRef) { fseq = fseqFromBase64 (v["data"].toString()); fseq.name = p.fseqRef.toStdString(); }
        }
        if (fseq.empty()) std::printf ("aviso: Fseq \"%s\" no encontrado en %s\n", p.fseqRef.toRawUTF8(), dir.getFullPathName().toRawUTF8());
    }

    Synth synth;
    synth.prepare (sr, 512);
    auto get = [&] (const char* id) {
        if (p.params.contains (id)) return (float) p.params[id];
        const int i = hostIndex (id); return i >= 0 ? hostParams()[(size_t) i].def : 0.0f;
    };
    synth.setParams (get);
    // Los puntos de lazo del preset, como en el plugin: si no vienen, los del Fseq.
    if (! p.params.contains ("fseq_lp_end") && ! fseq.empty()) { synth.part.fseqLoopStart = fseq.loopStart; if (synth.part.fseqLoopEnd > 0) synth.part.fseqLoopEnd = fseq.loopEnd; }

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
        midiEvs = loadMidiFile (cwd.getChildFile (mp), sr, len);
        if (! args.contains ("--secs")) total = (int) ((len + 3.5) * sr);
        if (! args.contains ("--notes")) evs.clear();
    }
    juce::AudioBuffer<float> out (2, total); out.clear();
    size_t e = 0;
    const FseqData* fp = fseq.empty() ? nullptr : &fseq;
    const double t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < total; pos += 64)
    {
        const int n = std::min (64, total - pos);
        while (e < evs.size() && evs[e].sample < pos + n)
        {
            if (evs[e].on) synth.engine.noteOn (evs[e].note, 100, p.voice, synth.part); else synth.engine.noteOff (evs[e].note, synth.part);
            ++e;
        }
        while (me < midiEvs.size() && midiEvs[me].sample < pos + n)
        {
            const auto& m = midiEvs[me++].msg;
            if (m.isController())
                if (const char* id = Synth::ccToParam (m.getControllerNumber()))
                {
                    // Un CC que mueve un parámetro del host: se aplica sobre el rango del parámetro.
                    const int i = hostIndex (id);
                    if (i >= 0)
                    {
                        const auto& hp = hostParams()[(size_t) i];
                        const float v = hp.lo + (hp.hi - hp.lo) * (float) m.getControllerValue() / 127.0f;
                        synth.setParams ([&] (const char* q) { return juce::String (q) == id ? v : get (q); });
                    }
                    continue;
                }
            synth.handleMidi (m, p.voice);
        }
        synth.render (out.getWritePointer (0) + pos, out.getWritePointer (1) + pos, n, p.voice, fp);
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
