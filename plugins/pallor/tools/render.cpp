// pallorrender: el procesador completo de Pallor tocando un preset (sus patrones con el reloj de un host simulado, o
// un .mid), a WAV. Sirve para los clips de demostración, las mezclas por bus, la CPU y para exportar patrones a .mid.
//
//   pallorrender out.wav --preset "Cold Room" [--bars 8 | --seconds 16] [--bpm 120] [--sr 48000] [--block 512]
//                [--tail 2] [--set id=valor ...] [--at segundos id=valor ...] [--midi notas.mid] [--sample slot=archivo]
//                [--stems carpeta] [--export-mid patron.mid --pattern 1A]
//   pallorrender --list                     los presets y los juegos de la biblioteca
//   pallorrender --bench                    CPU de cada preset (8 compases a 48 kHz, bloques de 512)
//
// --tail para el transporte al final y deja sonar las colas. --stems escribe además cada bus (Main, Kick, Snare/Clap,
// Hats/Cymbals, Toms, Perc, FX) con las salidas por voz activadas. --at mueve un parámetro en ese momento.
#include "PluginProcessor.h"
#include "../../../shared/tools/MidiEvents.h"

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

static bool writeWav (const juce::File& f, const juce::AudioBuffer<float>& b, int ch0, double sr, int frames)
{
    f.deleteFile();
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (os == nullptr) return false;
    juce::WavAudioFormat wav;
    auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
    if (w == nullptr) return false;
    const float* chans[2] = { b.getReadPointer (ch0), b.getReadPointer (ch0 + 1) };
    return w->writeFromFloatArrays (chans, 2, frames);
}

static int presetIndex (PallorProcessor& p, const juce::String& name)
{
    for (int i = 0; i < (int) p.getPresets().size(); ++i) if (p.getPresets()[(size_t) i].name.equalsIgnoreCase (name)) return i;
    return -1;
}

struct RenderResult { double cpu = 0; float peak = 0; };

static RenderResult render (PallorProcessor& p, HostClock& clock, juce::AudioBuffer<float>& out, int frames, int playFrames, int block,
                      const std::vector<dh::MidiEv>& midiEvents, const std::vector<std::tuple<int, juce::String, float>>& ats)
{
    juce::MidiBuffer midi;
    size_t mi = 0;
    juce::AudioBuffer<float> blk (out.getNumChannels(), block);
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    RenderResult r;
    for (int pos = 0; pos < frames; pos += block)
    {
        const int n = std::min (block, frames - pos);
        clock.pos = pos; clock.playing = pos < playFrames;
        for (auto& a : ats) if (std::get<0> (a) >= pos && std::get<0> (a) < pos + n) p.setValue (std::get<1> (a), std::get<2> (a));
        midi.clear();
        while (mi < midiEvents.size() && midiEvents[mi].sample < pos + n) { midi.addEvent (midiEvents[mi].msg, std::max (0, midiEvents[mi].sample - pos)); ++mi; }
        blk.setSize (out.getNumChannels(), n, false, false, true);
        blk.clear();
        p.processBlock (blk, midi);
        for (int c = 0; c < out.getNumChannels(); ++c) out.copyFrom (c, pos, blk, c, 0, n);
    }
    r.cpu = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0 / ((double) frames / clock.sr) * 100.0;
    r.peak = out.getMagnitude (0, 0, frames);
    return r;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    juce::StringArray args; for (int i = 1; i < argc; ++i) args.add (juce::String::fromUTF8 (argv[i]));
    auto opt = [&] (const char* n, juce::String d) { const int i = args.indexOf (n); return i >= 0 && i + 1 < args.size() ? args[i + 1] : d; };
    const auto cwd = juce::File::getCurrentWorkingDirectory();

    if (args.contains ("--list"))
    {
        PallorProcessor p;
        juce::String cat;
        for (const auto& pr : p.getPresets()) { if (pr.category != cat) { std::printf ("\n[%s]\n", pr.category.toRawUTF8()); cat = pr.category; } std::printf ("  %s  (%s BPM)\n", pr.name.toRawUTF8(), pr.data.getProperty ("params", {}).getProperty ("tempo", 120).toString().toRawUTF8()); }
        std::printf ("\n%d presets; biblioteca: %d juegos (A, B y Fill: %d patrones)\n", (int) p.getPresets().size(), (int) p.getLibrary().size(), (int) p.getLibrary().size() * 3);
        return 0;
    }

    // La interfaz pintada a un PNG (para revisarla sin abrir un host).
    if (const auto snap = opt ("--snapshot", ""); snap.isNotEmpty())
    {
        PallorProcessor p;
        if (const auto name = opt ("--preset", ""); name.isNotEmpty()) { const int idx = presetIndex (p, name); if (idx >= 0) p.loadPreset (idx); }
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (opt ("--width", "1180").getIntValue(), opt ("--height", "760").getIntValue());
        ed->resized();
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        juce::File out = cwd.getChildFile (snap); out.deleteFile();
        juce::FileOutputStream os (out); juce::PNGImageFormat png;
        const bool ok = os.openedOk() && png.writeImageToStream (img, os);
        std::printf ("%s: %s\n", out.getFileName().toRawUTF8(), ok ? "ok" : "no se pudo escribir");
        return ok ? 0 : 4;
    }

    // Prueba de estado: guardar y restaurar en otra instancia (con una muestra propia embebida) da el mismo sonido.
    if (args.contains ("--state-test"))
    {
        const double fsr = 48000.0; const int blk = 512;
        auto play = [&] (PallorProcessor& p, int frames) {
            HostClock c; c.sr = fsr; c.playing = false; p.setPlayHead (&c);
            juce::AudioBuffer<float> out (2, frames); out.clear();
            juce::MidiBuffer midi;
            const int notes[] = { 36, 38, 42, 46, 48, 55, 37, 41 };
            for (int pos = 0; pos < frames; pos += blk)
            {
                const int n = std::min (blk, frames - pos);
                midi.clear();
                for (int i = 0; i < 8; ++i) { const int at = (int) ((0.1 + 0.25 * i) * fsr); if (at >= pos && at < pos + n) midi.addEvent (juce::MidiMessage::noteOn (10, notes[i], (juce::uint8) 100), at - pos); }
                juce::AudioBuffer<float> b (out.getArrayOfWritePointers(), 2, pos, n);
                p.processBlock (b, midi);
            }
            p.setPlayHead (nullptr);
            return out;
        };
        int fails = 0;
        // Una muestra propia: un barrido sintético escrito a un WAV temporal.
        juce::File tmpWav = juce::File::createTempFile (".wav");
        {
            juce::AudioBuffer<float> sb (1, 22050);
            for (int i = 0; i < 22050; ++i) sb.setSample (0, i, 0.6f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * (200.0 + 2000.0 * i / 22050.0) * i / 44100.0) * (1.0f - (float) i / 22050.0f));
            std::unique_ptr<juce::OutputStream> os (tmpWav.createOutputStream());
            juce::WavAudioFormat wav;
            if (auto w = wav.createWriterFor (os, juce::AudioFormatWriterOptions().withSampleRate (44100).withNumChannels (1).withBitsPerSample (24))) w->writeFromAudioSampleBuffer (sb, 0, 22050);
        }
        for (int k : { 0, 32, 64 })
        {
            PallorProcessor a; a.setPlayConfigDetails (0, 2, fsr, blk); a.prepareToPlay (fsr, blk);
            if (k < (int) a.getPresets().size()) a.loadPreset (k);
            juce::String err;
            if (k == 32 && ! a.loadUserSample (dh::pal::kSnare, tmpWav, err)) { std::printf ("muestra: %s\n", err.toRawUTF8()); ++fails; }
            play (a, 2048);
            juce::MemoryBlock st; a.getStateInformation (st);
            a.setStateInformation (st.getData(), (int) st.getSize());
            PallorProcessor b; b.setPlayConfigDetails (0, 2, fsr, blk); b.prepareToPlay (fsr, blk);
            b.setStateInformation (st.getData(), (int) st.getSize());
            play (b, 2048);
            const auto ra = play (a, (int) (3.0 * fsr)), rb = play (b, (int) (3.0 * fsr));
            const float la = ra.getRMSLevel (0, 0, ra.getNumSamples()), lb = rb.getRMSLevel (0, 0, rb.getNumSamples());
            const bool ok = std::abs (juce::Decibels::gainToDecibels (la) - juce::Decibels::gainToDecibels (lb)) < 1.0f && b.userSampleName (dh::pal::kSnare) == a.userSampleName (dh::pal::kSnare);
            std::printf ("  programa %d: %d bytes de estado, nivel %.1f / %.1f dB, muestra propia '%s' / '%s'  [%s]\n", k, (int) st.getSize(),
                         juce::Decibels::gainToDecibels (la), juce::Decibels::gainToDecibels (lb), a.userSampleName (dh::pal::kSnare).toRawUTF8(), b.userSampleName (dh::pal::kSnare).toRawUTF8(), ok ? "ok" : "FALLA");
            fails += ok ? 0 : 1;
        }
        tmpWav.deleteFile();
        return fails ? 1 : 0;
    }

    const double sr = opt ("--sr", "48000").getDoubleValue();
    const int block = std::max (1, opt ("--block", "512").getIntValue());

    if (args.contains ("--bench"))
    {
        PallorProcessor p; HostClock clock; clock.sr = sr;
        p.setPlayHead (&clock); p.setPlayConfigDetails (0, 2, sr, block); p.prepareToPlay (sr, block);
        double sum = 0, worst = 0; juce::String worstName;
        for (int i = 0; i < (int) p.getPresets().size(); ++i)
        {
            p.loadPreset (i); p.setValue ("sync", 1.0f);
            clock.bpm = p.value ("tempo");
            const int frames = (int) (sr * 8 * 4 * 60.0 / clock.bpm);
            juce::AudioBuffer<float> out (2, frames);
            p.prepareToPlay (sr, block);
            const auto r = render (p, clock, out, frames, frames, block, {}, {});
            std::printf ("  %-28s %6.2f %% de un núcleo, pico %.2f\n", p.getPresets()[(size_t) i].name.toRawUTF8(), r.cpu, r.peak);
            sum += r.cpu; if (r.cpu > worst) { worst = r.cpu; worstName = p.getPresets()[(size_t) i].name; }
        }
        std::printf ("promedio %.2f %%, máximo %.2f %% (%s)\n", sum / std::max (1, (int) p.getPresets().size()), worst, worstName.toRawUTF8());
        return 0;
    }

    if (args.isEmpty() || args[0].startsWith ("--")) { std::printf ("uso: pallorrender out.wav [--preset nombre] [--bars n | --seconds s] [--bpm n] [--tail s] [--set id=v] [--at s id=v] [--midi f.mid] [--stems dir] [--export-mid f.mid --pattern 1A]\n       pallorrender --list | --bench\n"); return 1; }

    PallorProcessor p; HostClock clock; clock.sr = sr;
    const bool stems = args.contains ("--stems");
    if (stems)
    {
        auto layout = p.getBusesLayout();
        for (int b = 1; b < layout.outputBuses.size(); ++b) layout.outputBuses.getReference (b) = juce::AudioChannelSet::stereo();
        if (! p.setBusesLayout (layout)) std::printf ("el host de prueba no pudo activar los buses auxiliares\n");
    }
    const int nCh = p.getTotalNumOutputChannels();
    // setPlayConfigDetails volvería a un solo bus de 14 canales: con los auxiliares activos sólo se fija la frecuencia.
    p.setPlayHead (&clock); p.setRateAndBufferSizeDetails (sr, block); p.prepareToPlay (sr, block);
    if (const auto name = opt ("--preset", ""); name.isNotEmpty())
    {
        const int idx = presetIndex (p, name);
        if (idx < 0) { std::printf ("preset desconocido: %s\n", name.toRawUTF8()); return 3; }
        p.loadPreset (idx);
    }
    clock.bpm = opt ("--bpm", juce::String (p.value ("tempo"))).getDoubleValue();
    p.setValue ("sync", 1.0f);
    if (stems) p.setValue ("routing", 1.0f);
    for (int i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--set" && i + 1 < args.size()) p.setValue (args[i + 1].upToFirstOccurrenceOf ("=", false, false), args[i + 1].fromFirstOccurrenceOf ("=", false, false).getFloatValue());
        if (args[i] == "--sample" && i + 1 < args.size())
        {
            const int slot = args[i + 1].upToFirstOccurrenceOf ("=", false, false).getIntValue();
            juce::String err;
            if (! p.loadUserSample (slot, cwd.getChildFile (args[i + 1].fromFirstOccurrenceOf ("=", false, false)), err)) { std::printf ("%s\n", err.toRawUTF8()); return 5; }
        }
    }
    if (const auto mid = opt ("--export-mid", ""); mid.isNotEmpty())
    {
        const auto pat = opt ("--pattern", "1A").toUpperCase();
        const int g = juce::jlimit (1, dh::pal::kGroups, pat.getIntValue()) - 1;
        const int v = pat.endsWith ("FILL") || pat.endsWith ("F") ? 2 : (pat.endsWith ("B") ? 1 : 0);
        const bool ok = p.exportMidi (cwd.getChildFile (mid), dh::pal::patIndex (g, v), std::max (1, opt ("--repeats", "1").getIntValue()));
        std::printf ("%s: %s\n", mid.toRawUTF8(), ok ? "ok" : "no se pudo escribir");
        if (args[0].endsWith (".mid")) return ok ? 0 : 4;
    }
    std::vector<std::tuple<int, juce::String, float>> ats;
    for (int i = 0; i + 2 < args.size(); ++i)
        if (args[i] == "--at")
            ats.push_back ({ (int) (args[i + 1].getDoubleValue() * sr), args[i + 2].upToFirstOccurrenceOf ("=", false, false), args[i + 2].fromFirstOccurrenceOf ("=", false, false).getFloatValue() });
    std::vector<dh::MidiEv> midiEvents; double midiLen = 0;
    if (const auto mf = opt ("--midi", ""); mf.isNotEmpty()) { midiEvents = dh::loadMidiFile (cwd.getChildFile (mf), sr, midiLen); p.setValue ("sync", 0.0f); p.setValue ("run", 0.0f); }
    double secs = opt ("--seconds", "0").getDoubleValue();
    if (secs <= 0) secs = midiLen > 0 ? midiLen + 0.5 : opt ("--bars", "8").getDoubleValue() * 4.0 * 60.0 / clock.bpm;
    const double tail = opt ("--tail", "0").getDoubleValue();
    const int playFrames = (int) (secs * sr), frames = (int) ((secs + tail) * sr);
    // La latencia (las voces alineadas y la cinta) se descuenta: el archivo empieza en el tiempo 0. La del procesador
    // llega al host por un mensaje asincrónico (acá no hay bucle de mensajes): se toma la del motor después del render.
    const int maxLat = dh::pal::kVoiceLatency + dh::pal::Master::kTapeLatency;
    juce::AudioBuffer<float> out (nCh, frames + maxLat); out.clear();
    const auto r = render (p, clock, out, frames + maxLat, playFrames, block, midiEvents, ats);
    const int lat = p.engineForTests().latency();
    juce::AudioBuffer<float> trimmed (nCh, frames);
    for (int c = 0; c < nCh; ++c) trimmed.copyFrom (c, 0, out, c, lat, frames);
    juce::File outFile = cwd.getChildFile (args[0]);
    if (! writeWav (outFile, trimmed, 0, sr, frames)) { std::printf ("no se puede escribir %s\n", outFile.getFullPathName().toRawUTF8()); return 4; }
    if (stems)
    {
        juce::File dir = cwd.getChildFile (opt ("--stems", "stems")); dir.createDirectory();
        for (int b = 0; b < dh::pal::kBuses && 2 * b + 1 < nCh; ++b)
            writeWav (dir.getChildFile (juce::String (b) + "_" + juce::String (dh::pal::busName (b)).replaceCharacters ("/ ", "--") + ".wav"), trimmed, 2 * b, sr, frames);
    }
    std::printf ("%s: %.1f s, pico %.3f, CPU %.2f %% de un núcleo, latencia %d muestras\n", outFile.getFileName().toRawUTF8(), frames / sr, trimmed.getMagnitude (0, 0, frames), r.cpu, lat);
    return 0;
}
