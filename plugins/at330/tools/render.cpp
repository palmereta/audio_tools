// at330render: el procesador completo del AT330 tocando un preset a WAV, con un .mid o con acordes, y la voz del
// vocoder (el modulador, lo que en un DAW entra por sidechain) desde un WAV. Sirve para las demos, los clips de la web
// y la CPU.
//
//   at330render out.wav --preset "Nombre" [--midi f.mid | --chords "Am F C G"] [--beats-per-chord 4] [--bpm 96]
//               [--voice voz.wav] [--carrier portador.wav] [--seconds s] [--tail s] [--sr 48000] [--block 512]
//               [--set id=valor ...] [--at segundos id=valor ...]
//   at330render --list                      los presets
//   at330render --bench                     CPU de cada preset: un acorde de 8 notas 10 s a 48 kHz (con voz si es vocoder)
//
// --chords: un acorde por compás (o por --beats-per-chord tiempos), con nombres como C, Am, F#m7, Bbmaj7, Dsus4, G/B.
// --at mueve un parámetro en ese momento (por ejemplo bajar el ensemble a mitad del clip: --at 8 ens_on=0).
// Sin --midi ni --chords toca un acorde de prueba (A menor de tres octavas) 6 s.
#include "PluginProcessor.h"
#include "../../../shared/tools/MidiEvents.h"
#include "Oversampling.h"

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

static bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, double sr)
{
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (os == nullptr) return false;
    juce::WavAudioFormat wav;
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (b.getNumChannels()).withBitsPerSample (24));
    return w != nullptr && w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

// Un WAV a estéreo a la frecuencia del render (sin remuestrear: tiene que venir a la misma).
static bool readWav (const juce::File& f, double sr, juce::AudioBuffer<float>& out)
{
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr) { std::printf ("no se puede leer %s\n", f.getFullPathName().toRawUTF8()); return false; }
    if (std::abs (r->sampleRate - sr) > 0.5) { std::printf ("%s está a %.0f Hz, el render a %.0f\n", f.getFileName().toRawUTF8(), r->sampleRate, sr); return false; }
    out.setSize (2, (int) r->lengthInSamples); out.clear();
    r->read (&out, 0, (int) r->lengthInSamples, 0, true, true);
    if (r->numChannels == 1) out.copyFrom (1, 0, out, 0, 0, out.getNumSamples());
    return true;
}

// Un acorde por nombre → notas MIDI (en la octava 3-4, voces cerradas, y el bajo una octava abajo).
static std::vector<int> chordNotes (const juce::String& nameIn)
{
    juce::String name = nameIn.trim(), bass;
    if (name.containsChar ('/')) { bass = name.fromFirstOccurrenceOf ("/", false, false); name = name.upToFirstOccurrenceOf ("/", false, false); }
    auto pcOf = [] (const juce::String& s, int& len) {
        static const char* n[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B", "Db", "D#", "Gb", "G#", "A#" };
        static const int pc[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 1, 3, 6, 8, 10 };
        int best = -1; len = 0;
        for (int i = 0; i < 17; ++i) { const juce::String k (n[i]); if (s.startsWith (k) && k.length() > len) { best = pc[i]; len = k.length(); } }
        return best;
    };
    int len = 0; const int root = pcOf (name, len);
    if (root < 0) return {};
    const juce::String q = name.substring (len);
    std::vector<int> iv { 0, 4, 7 };
    if (q.startsWith ("m") && ! q.startsWith ("maj")) iv = { 0, 3, 7 };
    if (q.contains ("sus4")) iv = { 0, 5, 7 };
    if (q.contains ("sus2")) iv = { 0, 2, 7 };
    if (q.contains ("dim")) iv = { 0, 3, 6 };
    if (q.contains ("maj7")) iv.push_back (11);
    else if (q.contains ("7")) iv.push_back (10);
    if (q.contains ("add9") || q.endsWith ("9")) iv.push_back (14);
    std::vector<int> out;
    for (int i : iv) out.push_back (57 + (root + 3) % 12 + i);            // raíz entre A3 (57) y G#4 (68)
    const int b = bass.isNotEmpty() ? [&] { int bl = 0; return pcOf (bass, bl); }() : root;
    out.push_back (45 + (b + 3) % 12);
    return out;
}

static int presetIndex (At330Processor& p, const juce::String& name)
{
    for (int i = 0; i < (int) p.getPresets().size(); ++i) if (p.getPresets()[(size_t) i].name.equalsIgnoreCase (name)) return i;
    return -1;
}

struct RenderResult { double cpu = 0; float peak = 0; };

static RenderResult render (At330Processor& p, HostClock& clock, juce::AudioBuffer<float>& out, const juce::AudioBuffer<float>* voice,
                      const juce::AudioBuffer<float>* carrier, int block, const std::vector<dh::MidiEv>& ev,
                      const std::vector<std::tuple<int, juce::String, float>>& ats)
{
    const int frames = out.getNumSamples();
    // La entrada comparte los canales 0-1 con la salida: la voz en los dos o, con portador externo, la voz a la
    // izquierda y el portador a la derecha (como lo espera el procesador).
    juce::AudioBuffer<float> blk (2, block);
    juce::MidiBuffer midi; size_t mi = 0;
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < frames; pos += block)
    {
        const int n = std::min (block, frames - pos);
        clock.pos = pos;
        for (auto& a : ats) if (std::get<0> (a) >= pos && std::get<0> (a) < pos + n) p.setValue (std::get<1> (a), std::get<2> (a));
        midi.clear();
        while (mi < ev.size() && ev[mi].sample < pos + n) { midi.addEvent (ev[mi].msg, std::max (0, ev[mi].sample - pos)); ++mi; }
        blk.setSize (2, n, false, false, true); blk.clear();
        const int vn = voice != nullptr && pos < voice->getNumSamples() ? std::min (n, voice->getNumSamples() - pos) : 0;
        const int cn = carrier != nullptr && pos < carrier->getNumSamples() ? std::min (n, carrier->getNumSamples() - pos) : 0;
        if (carrier != nullptr)
        {
            for (int i = 0; i < vn; ++i) blk.setSample (0, i, 0.5f * (voice->getSample (0, pos + i) + voice->getSample (1, pos + i)));
            for (int i = 0; i < cn; ++i) blk.setSample (1, i, 0.5f * (carrier->getSample (0, pos + i) + carrier->getSample (1, pos + i)));
        }
        else if (vn > 0) for (int c = 0; c < 2; ++c) blk.copyFrom (c, 0, *voice, c, pos, vn);
        p.processBlock (blk, midi);
        for (int c = 0; c < 2; ++c) out.copyFrom (c, pos, blk, c, 0, n);
    }
    RenderResult r;
    r.cpu = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0 / ((double) frames / clock.sr) * 100.0;
    r.peak = out.getMagnitude (0, 0, frames);
    return r;
}

static void prepare (At330Processor& p, HostClock& clock, double sr, int block, bool withCarrier)
{
    p.setPlayHead (&clock);
    // La entrada (la voz, y el portador a la derecha) estéreo.
    auto layout = p.getBusesLayout();
    layout.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    juce::ignoreUnused (withCarrier);
    p.setBusesLayout (layout);
    p.setRateAndBufferSizeDetails (sr, block);
    p.prepareToPlay (sr, block);
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (juce::String::fromUTF8 (argv[i]));
    auto opt = [&] (const char* n, juce::String d) { const int i = args.indexOf (n); return i >= 0 && i + 1 < args.size() ? args[i + 1] : d; };
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const double sr = opt ("--sr", "48000").getDoubleValue();
    const int block = std::max (1, opt ("--block", "512").getIntValue());

    if (args.contains ("--list"))
    {
        At330Processor p;
        for (auto& e : p.getPresets()) std::printf ("%-28s %-10s %s\n", e.name.toRawUTF8(), e.category.toRawUTF8(), e.description.toRawUTF8());
        return 0;
    }
    if (args.contains ("--bench"))
    {
        // La voz de prueba para los presets de vocoder: un pulso de 110 Hz con dos formantes que cambian (sin depender de
        // archivos).
        const int frames = (int) (10.0 * sr);
        juce::AudioBuffer<float> voice (2, frames);
        for (int i = 0; i < frames; ++i)
        {
            const double t = i / sr, ph = std::fmod (t * 110.0, 1.0);
            const float x = (float) (0.3 * (ph < 0.4 ? std::sin (dh::kPiD * ph / 0.4) : 0.0) * (0.6 + 0.4 * std::sin (2 * dh::kPiD * 1.7 * t)));
            voice.setSample (0, i, x); voice.setSample (1, i, x);
        }
        std::vector<dh::MidiEv> ev;
        for (int n : { 45, 52, 57, 60, 64, 67, 69, 72 }) { ev.push_back ({ 0, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) }); ev.push_back ({ frames - 4800, juce::MidiMessage::noteOff (1, n) }); }
        At330Processor probe;
        double worst = 0; juce::String worstName;
        for (int i = 0; i < (int) probe.getPresets().size(); ++i)
        {
            At330Processor p; HostClock clock; clock.sr = sr;
            prepare (p, clock, sr, block, false);
            p.loadPreset (i);
            juce::AudioBuffer<float> out (2, frames);
            const auto r = render (p, clock, out, &voice, nullptr, block, ev, {});
            std::printf ("%-28s CPU %5.2f %%  pico %.2f\n", p.getPresets()[(size_t) i].name.toRawUTF8(), r.cpu, r.peak);
            if (r.cpu > worst) { worst = r.cpu; worstName = p.getPresets()[(size_t) i].name; }
        }
        std::printf ("peor: %s %.2f %% de un núcleo\n", worstName.toRawUTF8(), worst);
        return 0;
    }
    if (args.isEmpty() || args[0].startsWith ("--"))
    {
        std::printf ("uso: at330render out.wav --preset nombre [--midi f.mid | --chords \"Am F C G\"] [--voice v.wav] [--carrier c.wav] [--bpm n] [--seconds s] [--tail s] [--set id=valor] [--at s id=valor]\n");
        return 1;
    }

    At330Processor p; HostClock clock; clock.sr = sr;
    juce::AudioBuffer<float> voice, carrier;
    const bool hasVoice = opt ("--voice", "").isNotEmpty(), hasCarrier = opt ("--carrier", "").isNotEmpty();
    if (hasVoice && ! readWav (cwd.getChildFile (opt ("--voice", "")), sr, voice)) return 2;
    if (hasCarrier && ! readWav (cwd.getChildFile (opt ("--carrier", "")), sr, carrier)) return 2;
    prepare (p, clock, sr, block, hasCarrier);
    if (const auto name = opt ("--preset", ""); name.isNotEmpty())
    {
        const int idx = presetIndex (p, name);
        if (idx < 0) { std::printf ("preset desconocido: %s\n", name.toRawUTF8()); return 3; }
        p.loadPreset (idx);
    }
    for (int i = 0; i < args.size(); ++i)
        if (args[i] == "--set" && i + 1 < args.size())
            p.setValue (args[i + 1].upToFirstOccurrenceOf ("=", false, false), args[i + 1].fromFirstOccurrenceOf ("=", false, false).getFloatValue());
    clock.bpm = opt ("--bpm", "96").getDoubleValue();

    // Notas: el .mid, los acordes o el acorde de prueba.
    std::vector<dh::MidiEv> ev;
    double playSecs = 6.0;
    if (const auto mid = opt ("--midi", ""); mid.isNotEmpty())
        ev = dh::loadMidiFile (cwd.getChildFile (mid), sr, playSecs);
    else if (const auto ch = opt ("--chords", ""); ch.isNotEmpty())
    {
        juce::StringArray chords; chords.addTokens (ch, " ", ""); chords.removeEmptyStrings();
        const double beats = opt ("--beats-per-chord", "4").getDoubleValue(), len = beats * 60.0 / clock.bpm * sr;
        for (int c = 0; c < chords.size(); ++c)
            for (int n : chordNotes (chords[c]))
            {
                ev.push_back ({ (int) (c * len), juce::MidiMessage::noteOn (1, n, (juce::uint8) 96) });
                ev.push_back ({ (int) ((c + 1) * len) - 32, juce::MidiMessage::noteOff (1, n) });
            }
        playSecs = chords.size() * len / sr;
    }
    else
        for (int n : { 45, 57, 60, 64, 69, 72, 76 }) { ev.push_back ({ 0, juce::MidiMessage::noteOn (1, n, (juce::uint8) 96) }); ev.push_back ({ (int) (6.0 * sr), juce::MidiMessage::noteOff (1, n) }); }
    std::stable_sort (ev.begin(), ev.end(), [] (const dh::MidiEv& a, const dh::MidiEv& b) { return a.sample < b.sample; });
    if (hasVoice) playSecs = std::max (playSecs, voice.getNumSamples() / sr);
    const double secs = opt ("--seconds", juce::String (playSecs + opt ("--tail", "3").getDoubleValue())).getDoubleValue();

    std::vector<std::tuple<int, juce::String, float>> ats;
    for (int i = 0; i + 2 < args.size(); ++i)
        if (args[i] == "--at")
            ats.push_back ({ (int) (args[i + 1].getDoubleValue() * sr), args[i + 2].upToFirstOccurrenceOf ("=", false, false),
                             args[i + 2].fromFirstOccurrenceOf ("=", false, false).getFloatValue() });

    juce::AudioBuffer<float> out (2, (int) (secs * sr)); out.clear();
    const auto r = render (p, clock, out, hasVoice ? &voice : nullptr, hasCarrier ? &carrier : nullptr, block, ev, ats);
    const auto file = cwd.getChildFile (args[0]);
    if (! writeWav (file, out, sr)) { std::printf ("no se puede escribir %s\n", file.getFullPathName().toRawUTF8()); return 4; }
    std::printf ("%s: %.1f s, pico %.2f, CPU %.2f %% de un núcleo\n", file.getFileName().toRawUTF8(), secs, r.peak, r.cpu);
    return 0;
}
