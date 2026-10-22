// tarnishrender: un archivo por el procesador completo de Tarnish con un preset de fábrica.
//   tarnishrender in.wav out.wav --preset "Dusty Cassette" [--tail 8] [--bpm 100] [--set id=valor ...]
//                  [--at segundos id=valor ...]
// --at cambia un parámetro en ese momento (por ejemplo parar la cinta a mitad del clip: --at 6 stop=1).
// Imprime la CPU (porcentaje de un núcleo). Sin --preset, el preset 0 (como una instancia nueva). --float escribe en coma flotante
// de 32 bits (sin recortar lo que pase de 0 dBFS, para medir niveles).
#include "PluginProcessor.h"

class HostClock : public juce::AudioPlayHead
{
public:
    double bpm = 120.0; int64_t pos = 0; double sr = 48000.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setIsPlaying (true); p.setTimeInSamples (pos); p.setPpqPosition ((double) pos / sr * bpm / 60.0);
        return p;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 3) { std::printf ("uso: tarnishrender in.wav out.wav [--preset nombre] [--tail s] [--bpm n] [--set id=valor ...] [--at s id=valor ...] [--float]\n"); return 1; }
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (juce::String::fromUTF8 (argv[i]));
    auto opt = [&] (const char* n, juce::String d) { const int i = args.indexOf (n); return i >= 0 && i + 1 < args.size() ? args[i + 1] : d; };
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (cwd.getChildFile (args[0])));
    if (! r) { std::printf ("no se puede leer %s\n", argv[1]); return 2; }
    const double sr = r->sampleRate; const double tail = opt ("--tail", "6").getDoubleValue();
    const int len = (int) r->lengthInSamples, total = len + (int) (tail * sr);
    juce::AudioBuffer<float> buf (2, total); buf.clear(); r->read (&buf, 0, len, 0, true, true);

    TarnishProcessor p; HostClock clock; clock.bpm = opt ("--bpm", "120").getDoubleValue(); clock.sr = sr;
    p.setPlayHead (&clock); p.setPlayConfigDetails (2, 2, sr, 512); p.prepareToPlay (sr, 512);
    if (const auto name = opt ("--preset", ""); name.isNotEmpty())
    {
        int idx = -1; for (int i = 0; i < (int) p.getPresets().size(); ++i) if (p.getPresets()[(size_t) i].name.equalsIgnoreCase (name)) idx = i;
        if (idx < 0) { std::printf ("preset desconocido: %s\n", name.toRawUTF8()); return 3; }
        p.loadPreset (idx);
    }
    for (int i = 0; i < args.size(); ++i) if (args[i] == "--set" && i + 1 < args.size()) p.setValue (args[i + 1].upToFirstOccurrenceOf ("=", false, false), args[i + 1].fromFirstOccurrenceOf ("=", false, false).getFloatValue());
    struct At { int pos; juce::String id; float v; };
    std::vector<At> ats;
    for (int i = 0; i + 2 < args.size(); ++i)
        if (args[i] == "--at")
            ats.push_back ({ (int) (args[i + 1].getDoubleValue() * sr), args[i + 2].upToFirstOccurrenceOf ("=", false, false),
                             args[i + 2].fromFirstOccurrenceOf ("=", false, false).getFloatValue() });
    juce::MidiBuffer midi;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < total; pos += 512)
    {
        const int n = std::min (512, total - pos);
        juce::AudioBuffer<float> blk (buf.getArrayOfWritePointers(), 2, pos, n);
        clock.pos = pos;
        for (auto& a : ats) if (a.pos >= pos && a.pos < pos + n) p.setValue (a.id, a.v);
        p.processBlock (blk, midi);
    }
    const double secs = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
    juce::File out = cwd.getChildFile (args[1]); out.deleteFile();
    std::unique_ptr<juce::OutputStream> os (out.createOutputStream());
    juce::WavAudioFormat wav;
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (args.contains ("--float") ? 32 : 24));
    if (! w || ! w->writeFromAudioSampleBuffer (buf, 0, total)) { std::printf ("no se puede escribir\n"); return 4; }
    std::printf ("%s: %.1f s, CPU %.2f %% de un núcleo\n", out.getFileName().toRawUTF8(), total / sr, 100.0 * secs / (total / sr));
    return 0;
}
