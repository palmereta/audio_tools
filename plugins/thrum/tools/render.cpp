// thrumrender: el procesador completo de Thrum tocando un preset a WAV, con un .mid o con una progresión de acordes,
// y el host simulado con su transporte (los patrones de rasgueo siguen el tempo). Sirve para las demos, la CPU, las IR
// de fábrica y para exportar patrones a .mid.
//
//   thrumrender out.wav --preset "Nombre" [--midi f.mid | --chords "C G Am F"] [--bars n | --seconds s] [--bpm n]
//               [--tail s] [--set id=valor ...] [--di di.wav] [--sr 48000] [--block 512] [--order "gate comp ... cab"]
//   thrumrender --list                      los presets y los patrones de la biblioteca
//   thrumrender --bench                     CPU de cada preset (8 compases a 48 kHz, bloques de 512)
//   thrumrender --export-irs carpeta        las IR de fábrica (las que genera nuestro modelo de gabinete) a WAV
//   thrumrender --export-pattern f.mid [--preset n] [--bars 4]
//   thrumrender --state-test                guardar y restaurar el estado da el mismo sonido
//   thrumrender --snapshot ui.png           la interfaz pintada a un PNG
//
// --chords: un acorde por compás (o --beats-per-chord n), con nombres como C, Am, F#m7, Bb, G/B, E5.
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "../../../shared/tools/MidiEvents.h"

using namespace dh::gt;

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

static bool writeWav (const juce::File& f, const float* const* ch, int nCh, double sr, int frames)
{
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (os == nullptr) return false;
    juce::WavAudioFormat wav;
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (nCh).withBitsPerSample (24));
    if (w == nullptr) return false;
    return w->writeFromFloatArrays (ch, nCh, frames);
}

static int presetIndex (ThrumProcessor& p, const juce::String& name)
{
    for (int i = 0; i < (int) p.getPresets().size(); ++i) if (p.getPresets()[(size_t) i].name.equalsIgnoreCase (name)) return i;
    return -1;
}

// Un acorde por nombre → notas MIDI (raíz en la octava 3, el bajo abajo).
static std::vector<int> chordNotes (const juce::String& nameIn)
{
    juce::String name = nameIn.trim();
    juce::String bass;
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
    const juce::String suffix = name.substring (len);
    int type = 0;
    for (int t = (int) chordTypes().size() - 1; t >= 0; --t) if (suffix == chordTypes()[(size_t) t].suffix) { type = t; break; }
    std::vector<int> out;
    const uint16_t m = chordTypes()[(size_t) type].mask;
    for (int iv = 0; iv < 12; ++iv) if (m & (1u << iv)) out.push_back (48 + root + iv);
    if (bass.isNotEmpty()) { int bl = 0; const int b = pcOf (bass, bl); if (b >= 0) out.push_back (36 + b); }
    return out;
}

struct RenderResult { double cpu = 0; float peak = 0; };

static RenderResult render (ThrumProcessor& p, HostClock& clock, juce::AudioBuffer<float>& out, int frames, int playFrames, int block, const std::vector<dh::MidiEv>& ev,
                      const std::vector<std::tuple<int, juce::String, float>>& ats)
{
    juce::MidiBuffer midi; size_t mi = 0;
    juce::AudioBuffer<float> blk (out.getNumChannels(), block);
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    for (int pos = 0; pos < frames; pos += block)
    {
        const int n = std::min (block, frames - pos);
        clock.pos = pos; clock.playing = pos < playFrames;
        for (auto& a : ats) if (std::get<0> (a) >= pos && std::get<0> (a) < pos + n) p.setValue (std::get<1> (a), std::get<2> (a));
        midi.clear();
        while (mi < ev.size() && ev[mi].sample < pos + n) { midi.addEvent (ev[mi].msg, std::max (0, ev[mi].sample - pos)); ++mi; }
        blk.setSize (out.getNumChannels(), n, false, false, true);
        blk.clear();
        p.processBlock (blk, midi);
        for (int c = 0; c < out.getNumChannels(); ++c) out.copyFrom (c, pos, blk, c, 0, n);
    }
    RenderResult r;
    r.cpu = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0 / ((double) frames / clock.sr) * 100.0;
    r.peak = out.getMagnitude (0, 0, frames);
    return r;
}

// La frase de prueba de un preset: su progresión ("demo_chords") o una melodía corta con ligados y un bend.
static std::vector<dh::MidiEv> presetPhrase (ThrumProcessor& p, int idx, double sr, double bpm, int bars)
{
    std::vector<dh::MidiEv> ev;
    const auto& d = p.getPresets()[(size_t) idx].data;
    const int mode = (int) p.value ("p_mode");
    const double beat = 60.0 / bpm * sr;
    if (mode >= kModeChords)
    {
        juce::StringArray chords; chords.addTokens (d.getProperty ("demo_chords", "C G Am F").toString(), " ", ""); chords.removeEmptyStrings();
        for (int b = 0; b < bars; ++b)
        {
            const auto notes = chordNotes (chords[b % chords.size()]);
            const int on = (int) (b * 4 * beat), off = (int) ((b + 1) * 4 * beat) - 64;
            for (int n : notes) { ev.push_back ({ on, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) }); ev.push_back ({ off, juce::MidiMessage::noteOff (1, n) }); }
        }
    }
    else
    {
        const int mel[] = { 64, 67, 69, 67, 64, 62, 64, 69, 71, 72, 71, 69, 67, 64, 62, 59 };
        for (int b = 0; b < bars * 4; ++b)
        {
            const int n = mel[b % 16] - (mode == kModeMono ? 0 : 0);
            const int on = (int) (b * beat), off = (int) ((b + 1) * beat + (b % 4 == 1 ? 0.2 * beat : -64));
            ev.push_back ({ on, juce::MidiMessage::noteOn (1, n, (juce::uint8) (80 + (b * 7) % 40)) });
            ev.push_back ({ off, juce::MidiMessage::noteOff (1, n) });
        }
    }
    std::stable_sort (ev.begin(), ev.end(), [] (const dh::MidiEv& a, const dh::MidiEv& b) { return a.sample < b.sample; });
    return ev;
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
        ThrumProcessor p;
        juce::String cat;
        for (const auto& pr : p.getPresets()) { if (pr.category != cat) { std::printf ("\n[%s]\n", pr.category.toRawUTF8()); cat = pr.category; } std::printf ("  %s\n", pr.name.toRawUTF8()); }
        std::printf ("\n%d presets; biblioteca: %d patrones de rasgueo, %d de dedos\n", (int) p.getPresets().size(), (int) ThrumProcessor::strumLibrary().size(), (int) ThrumProcessor::arpLibrary().size());
        return 0;
    }

    // Cada preset: los ids existen, las elecciones por nombre existen, los patrones existen.
    if (args.contains ("--check-presets"))
    {
        ThrumProcessor p; int bad = 0;
        for (const auto& pr : p.getPresets())
        {
            const auto pv = pr.data.getProperty ("params", {});
            if (auto* obj = pv.getDynamicObject())
                for (auto& kv : obj->getProperties())
                {
                    auto* prm = p.apvts.getParameter (kv.name.toString());
                    if (prm == nullptr) { std::printf ("%s: parámetro desconocido %s\n", pr.name.toRawUTF8(), kv.name.toString().toRawUTF8()); ++bad; continue; }
                    if (kv.value.isString())
                    {
                        auto* c = dynamic_cast<juce::AudioParameterChoice*> (prm);
                        if (c == nullptr || c->choices.indexOf (kv.value.toString()) < 0) { std::printf ("%s: %s = '%s' no es una opción\n", pr.name.toRawUTF8(), kv.name.toString().toRawUTF8(), kv.value.toString().toRawUTF8()); ++bad; }
                    }
                    else if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                    {
                        const float v = (float) kv.value; const auto r = rp->getNormalisableRange();
                        if (v < r.start - 1e-4f || v > r.end + 1e-4f) { std::printf ("%s: %s = %g fuera de rango [%g, %g]\n", pr.name.toRawUTF8(), kv.name.toString().toRawUTF8(), v, r.start, r.end); ++bad; }
                    }
                }
        }
        std::printf ("%d presets revisados, %d problemas\n", (int) p.getPresets().size(), bad);
        return bad ? 1 : 0;
    }

    if (const auto dir = opt ("--export-irs", ""); dir.isNotEmpty())
    {
        juce::File d = cwd.getChildFile (dir); d.createDirectory();
        int k = 0;
        for (const auto& f : factoryIrs())
        {
            const auto ir = designCabIr (f.p, sr, (int) (0.045 * sr));
            const float* ch[1] = { ir.data() };
            const auto name = juce::String (++k).paddedLeft ('0', 2) + "_" + juce::String (f.name).replaceCharacters (" ,+", "___").removeCharacters ("()") + ".wav";
            const bool ok = writeWav (d.getChildFile (name), ch, 1, sr, (int) ir.size());
            std::printf ("%s: %s\n", name.toRawUTF8(), ok ? "ok" : "no se pudo escribir");
        }
        return 0;
    }

    if (args.contains ("--snapshot"))
    {
        ThrumProcessor p;
        if (const auto name = opt ("--preset", ""); name.isNotEmpty()) { const int idx = presetIndex (p, name); if (idx >= 0) p.loadPreset (idx); }
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (opt ("--width", "1180").getIntValue(), opt ("--height", "780").getIntValue());
        if (auto* te = dynamic_cast<ThrumEditor*> (ed.get())) te->showPage (opt ("--tab", "0").getIntValue());
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out = cwd.getChildFile (opt ("--snapshot", "ui.png")); out.deleteFile();
        juce::FileOutputStream os (out); juce::PNGImageFormat png;
        const bool ok = os.openedOk() && png.writeImageToStream (img, os);
        std::printf ("%s: %s\n", out.getFileName().toRawUTF8(), ok ? "ok" : "no se pudo escribir");
        return ok ? 0 : 4;
    }

    if (args.contains ("--state-test"))
    {
        int fails = 0;
        for (int k : { 0, 20, 40 })
        {
            auto play = [&] (ThrumProcessor& p, int frames) {
                HostClock c; c.sr = sr; c.playing = true; c.bpm = 100; p.setPlayHead (&c);
                juce::AudioBuffer<float> out (2, frames); out.clear();
                std::vector<dh::MidiEv> ev = { { 100, juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100) }, { 100, juce::MidiMessage::noteOn (1, 52, (juce::uint8) 100) }, { 100, juce::MidiMessage::noteOn (1, 55, (juce::uint8) 100) },
                                               { frames - 1000, juce::MidiMessage::noteOff (1, 48) }, { frames - 1000, juce::MidiMessage::noteOff (1, 52) }, { frames - 1000, juce::MidiMessage::noteOff (1, 55) } };
                render (p, c, out, frames, frames, 512, ev, {});
                p.setPlayHead (nullptr);
                return out;
            };
            ThrumProcessor a; a.setNonRealtime (true); a.setPlayConfigDetails (0, 2, sr, 512); a.prepareToPlay (sr, 512);
            if (k < (int) a.getPresets().size()) a.loadPreset (k);
            a.setValue ("g_capo", 2.0f);
            juce::MemoryBlock st; a.getStateInformation (st);
            ThrumProcessor b; b.setNonRealtime (true); b.setPlayConfigDetails (0, 2, sr, 512); b.prepareToPlay (sr, 512);
            b.setStateInformation (st.getData(), (int) st.getSize());
            a.prepareToPlay (sr, 512); b.prepareToPlay (sr, 512);
            const auto ra = play (a, (int) (3.0 * sr)), rb = play (b, (int) (3.0 * sr));
            const float la = ra.getRMSLevel (0, 0, ra.getNumSamples()), lb = rb.getRMSLevel (0, 0, rb.getNumSamples());
            float maxDiff = 0; for (int i = 0; i < ra.getNumSamples(); ++i) maxDiff = std::max (maxDiff, std::fabs (ra.getSample (0, i) - rb.getSample (0, i)));
            const bool ok = maxDiff < 1e-5f && a.value ("g_capo") == b.value ("g_capo");
            std::printf ("  programa %d (%s): %d bytes de estado, nivel %.1f / %.1f dB, diferencia máxima %.1e  [%s]\n", k, a.getProgramName (k).toRawUTF8(), (int) st.getSize(),
                         juce::Decibels::gainToDecibels (la), juce::Decibels::gainToDecibels (lb), maxDiff, ok ? "ok" : "FALLA");
            fails += ok ? 0 : 1;
        }
        return fails ? 1 : 0;
    }

    if (args.contains ("--bench"))
    {
        ThrumProcessor p; HostClock clock; clock.sr = sr;
        p.setNonRealtime (true);
        p.setPlayHead (&clock); p.setPlayConfigDetails (0, 2, sr, block); p.prepareToPlay (sr, block);
        double sum = 0, worst = 0; juce::String worstName;
        for (int i = 0; i < (int) p.getPresets().size(); ++i)
        {
            p.loadPreset (i);
            clock.bpm = (double) p.getPresets()[(size_t) i].data.getProperty ("demo_bpm", 110);
            p.prepareToPlay (sr, block);
            const int frames = (int) (sr * 8 * 4 * 60.0 / clock.bpm);
            const auto ev = presetPhrase (p, i, sr, clock.bpm, 8);
            p.setNonRealtime (false);                          // el gabinete en su hilo, como en un host
            juce::AudioBuffer<float> out (2, frames);
            const auto r = render (p, clock, out, frames, frames, block, ev, {});
            p.setNonRealtime (true);
            std::printf ("  %-30s %6.2f %% de un núcleo, pico %.2f\n", p.getPresets()[(size_t) i].name.toRawUTF8(), r.cpu, r.peak);
            sum += r.cpu; if (r.cpu > worst) { worst = r.cpu; worstName = p.getPresets()[(size_t) i].name; }
        }
        std::printf ("promedio %.2f %%, máximo %.2f %% (%s)\n", sum / std::max (1, (int) p.getPresets().size()), worst, worstName.toRawUTF8());
        return 0;
    }

    if (const auto mid = opt ("--export-pattern", ""); mid.isNotEmpty())
    {
        ThrumProcessor p; p.setPlayConfigDetails (0, 2, sr, block); p.prepareToPlay (sr, block);
        if (const auto name = opt ("--preset", ""); name.isNotEmpty()) { const int idx = presetIndex (p, name); if (idx >= 0) p.loadPreset (idx); }
        const bool ok = p.exportPattern (cwd.getChildFile (mid), std::max (1, opt ("--bars", "4").getIntValue()));
        std::printf ("%s: %s\n", mid.toRawUTF8(), ok ? "ok" : "no se pudo escribir");
        return ok ? 0 : 4;
    }

    if (args.isEmpty() || args[0].startsWith ("--"))
    {
        std::printf ("uso: thrumrender out.wav [--preset nombre] [--midi f.mid | --chords \"C G Am F\"] [--bars n | --seconds s] [--bpm n] [--tail s] [--set id=v] [--at s id=v] [--di di.wav]\n"
                     "     thrumrender --list | --bench | --export-irs dir | --export-pattern f.mid | --state-test | --snapshot ui.png\n");
        return 1;
    }

    ThrumProcessor p; HostClock clock; clock.sr = sr;
    p.setNonRealtime (true);
    auto layout = p.getBusesLayout();
    const bool wantDi = opt ("--di", "").isNotEmpty();
    if (wantDi) { layout.outputBuses.getReference (1) = juce::AudioChannelSet::stereo(); p.setBusesLayout (layout); }
    p.setPlayHead (&clock); p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block);
    int presetIdx = -1;
    if (const auto name = opt ("--preset", ""); name.isNotEmpty())
    {
        presetIdx = presetIndex (p, name);
        if (presetIdx < 0) { std::printf ("preset desconocido: %s\n", name.toRawUTF8()); return 3; }
        p.loadPreset (presetIdx);
    }
    for (int i = 0; i < args.size(); ++i)
        if (args[i] == "--set" && i + 1 < args.size()) p.setValue (args[i + 1].upToFirstOccurrenceOf ("=", false, false), args[i + 1].fromFirstOccurrenceOf ("=", false, false).getFloatValue());
    if (const auto o = opt ("--order", ""); o.isNotEmpty()) p.setOrder (ThrumProcessor::orderFromString (o));
    const double defBpm = presetIdx >= 0 ? (double) p.getPresets()[(size_t) presetIdx].data.getProperty ("demo_bpm", 110) : 110.0;
    clock.bpm = opt ("--bpm", juce::String (defBpm)).getDoubleValue();
    p.prepareToPlay (sr, block);
    std::vector<std::tuple<int, juce::String, float>> ats;
    for (int i = 0; i + 2 < args.size(); ++i)
        if (args[i] == "--at")
            ats.push_back ({ (int) (args[i + 1].getDoubleValue() * sr), args[i + 2].upToFirstOccurrenceOf ("=", false, false), args[i + 2].fromFirstOccurrenceOf ("=", false, false).getFloatValue() });
    std::vector<dh::MidiEv> ev; double midiLen = 0;
    if (const auto mf = opt ("--midi", ""); mf.isNotEmpty()) ev = dh::loadMidiFile (cwd.getChildFile (mf), sr, midiLen);
    else if (const auto ch = opt ("--chords", ""); ch.isNotEmpty())
    {
        juce::StringArray chords; chords.addTokens (ch, " ", ""); chords.removeEmptyStrings();
        const double beats = opt ("--beats-per-chord", "4").getDoubleValue();
        const double beat = 60.0 / clock.bpm * sr;
        const int reps = std::max (1, opt ("--repeat", "1").getIntValue());
        int k = 0;
        for (int r = 0; r < reps; ++r)
            for (const auto& c : chords)
            {
                const auto notes = chordNotes (c);
                const int on = (int) (k * beats * beat), off = (int) ((k + 1) * beats * beat) - 32;
                for (int n : notes) { ev.push_back ({ on, juce::MidiMessage::noteOn (1, n, (juce::uint8) 100) }); ev.push_back ({ off, juce::MidiMessage::noteOff (1, n) }); }
                ++k;
            }
        midiLen = k * beats * beat / sr;
        std::stable_sort (ev.begin(), ev.end(), [] (const dh::MidiEv& a, const dh::MidiEv& b) { return a.sample < b.sample; });
    }
    else if (presetIdx >= 0)
    {
        ev = presetPhrase (p, presetIdx, sr, clock.bpm, std::max (1, opt ("--bars", "4").getIntValue()));
        midiLen = std::max (1, opt ("--bars", "4").getIntValue()) * 4 * 60.0 / clock.bpm;
    }
    double secs = opt ("--seconds", "0").getDoubleValue();
    if (secs <= 0) secs = midiLen > 0 ? midiLen + 0.25 : opt ("--bars", "4").getDoubleValue() * 4.0 * 60.0 / clock.bpm;
    const double tail = opt ("--tail", "2").getDoubleValue();
    const int playFrames = (int) (secs * sr), frames = (int) ((secs + tail) * sr);
    const int lat = p.getLatencySamples() > 0 ? p.getLatencySamples() : p.engineForTests().latency();
    const int nCh = p.getTotalNumOutputChannels();
    juce::AudioBuffer<float> out (nCh, frames + lat); out.clear();
    const auto r = render (p, clock, out, frames + lat, playFrames, block, ev, ats);
    // La latencia se descuenta: el archivo empieza en el tiempo 0.
    juce::File outFile = cwd.getChildFile (args[0]);
    const float* chs[2] = { out.getReadPointer (0) + lat, out.getReadPointer (1) + lat };
    if (! writeWav (outFile, chs, 2, sr, frames)) { std::printf ("no se puede escribir %s\n", outFile.getFullPathName().toRawUTF8()); return 4; }
    if (wantDi && nCh >= 4)
    {
        const float* d[1] = { out.getReadPointer (2) + lat };
        writeWav (cwd.getChildFile (opt ("--di", "di.wav")), d, 1, sr, frames);
    }
    float pk = 0; for (int c = 0; c < 2; ++c) pk = std::max (pk, out.getMagnitude (c, lat, frames));
    std::printf ("%s: %.1f s, pico %.3f, CPU %.2f %% de un núcleo, latencia %d muestras\n", outFile.getFileName().toRawUTF8(), frames / sr, pk, r.cpu, lat);
    return 0;
}
