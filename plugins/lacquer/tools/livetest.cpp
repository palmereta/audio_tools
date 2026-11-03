// Prueba del flujo en vivo del plugin, sin host: el procesador recibe una canción por bloques, LEARN escucha 30 s,
// el trabajador analiza y entrega la cadena nueva; después se mide la salida, la comparación A/B igualada y que el
// estado guardado reproduzca lo mismo en otra instancia.
//   lacquerlivetest mezcla.wav
#include "PluginProcessor.h"

static int fails = 0;
static void check (bool ok, const char* whatUtf8) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", whatUtf8); if (! ok) ++fails; }

static double lufsOf (const std::vector<float>& L, const std::vector<float>& R, double fs)
{
    dh::lq::LoudnessMeter m; m.prepare (fs); m.process (L.data(), R.data(), (int) L.size()); return m.integrated();
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { std::printf ("uso: lacquerlivetest mezcla.wav\n"); return 1; }
    juce::AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> r (fm.createReaderFor (juce::File (juce::String::fromUTF8 (argv[1]))));
    if (! r) { std::printf ("no se puede leer\n"); return 1; }
    const double fs = r->sampleRate; const int len = (int) r->lengthInSamples;
    juce::AudioBuffer<float> song (2, len); r->read (&song, 0, len, 0, true, true);

    auto run = [&] (LacquerProcessor& p, int from, int to, std::vector<float>* oL, std::vector<float>* oR, int block = 512) {
        juce::AudioBuffer<float> b (2, block); juce::MidiBuffer mb;
        for (int pos = from; pos < to; pos += block)
        {
            const int n = std::min (block, to - pos);
            b.setSize (2, n, false, false, true);
            b.copyFrom (0, 0, song, 0, pos, n); b.copyFrom (1, 0, song, 1, pos, n);
            p.processBlock (b, mb);
            if (oL) { oL->insert (oL->end(), b.getReadPointer (0), b.getReadPointer (0) + n); oR->insert (oR->end(), b.getReadPointer (1), b.getReadPointer (1) + n); }
        }
    };

    LacquerProcessor p;
    p.setPlayConfigDetails (2, 2, fs, 512);
    p.prepareToPlay (fs, 512);
    std::printf ("latencia informada: %d muestras (%.2f ms)\n", p.getLatencySamples(), 1000.0 * p.getLatencySamples() / fs);
    // Canción entera: LEARN termina solo cuando la música para (2 s de silencio después del final).
    int pos = 0;
    p.startLearning();
    juce::AudioBuffer<float> sil (2, 512); sil.clear(); juce::MidiBuffer mb0;
    while (p.learnState.load() == LacquerProcessor::LearnState::listening && pos < len + (int) (3 * fs))
    {
        if (pos < len) run (p, pos, std::min (len, pos + 512), nullptr, nullptr);
        else { sil.clear(); p.processBlock (sil, mb0); }
        pos += 512;
    }
    std::printf ("escuchó %.1f s\n", p.learnedSeconds());
    check (p.learnState.load() == LacquerProcessor::LearnState::analysing, "LEARN se detiene solo cuando la música para");
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    while (p.learnState.load() != LacquerProcessor::LearnState::ready && juce::Time::getMillisecondCounterHiRes() - t0 < 30000) juce::Thread::sleep (20);
    std::printf ("análisis en %.2f s\n", (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0);
    check (p.learnState.load() == LacquerProcessor::LearnState::ready && p.hasPlan(), "el trabajador entregó un plan");
    const auto plan = p.planCopy();
    std::printf ("plan: %s, ganancia %+.2f dB, salida prevista %.2f LUFS\n", dh::lq::styleDef (plan.opt.style).name, plan.gainDb, plan.outLufs);

    // Toda la canción de nuevo con la cadena aprendida (el fundido de 50 ms ocurre al principio).
    std::vector<float> L, R; run (p, 0, len, &L, &R);
    const int lat = p.getLatencySamples();
    std::vector<float> L2 (L.begin() + lat, L.end()), R2 (R.begin() + lat, R.end());
    const double out = lufsOf (L2, R2, fs);
    dh::lq::LoudnessMeter tm; tm.prepare (fs); tm.process (L2.data(), R2.data(), (int) L2.size());
    std::printf ("salida en vivo: %.2f LUFS, %.2f dBTP (objetivo %.1f, techo %.1f)\n", out, tm.truePeakDb(), plan.targetLufs, plan.ceilingDb);
    check (std::fabs (out - plan.targetLufs) <= 0.5, "T3: la canción entera queda a ±0.5 LU del objetivo");
    check (tm.truePeakDb() <= plan.ceilingDb + 0.1, "T4: pico verdadero bajo el techo en vivo");

    // A/B: el original igualado debe sonar a la misma sonoridad que el master.
    p.setValue (dh::lq::params::compare, 1.0f);
    std::vector<float> A, B; run (p, 0, len, &A, &B);
    const double ab = lufsOf (std::vector<float> (A.begin() + lat, A.end()), std::vector<float> (B.begin() + lat, B.end()), fs);
    std::printf ("original igualado: %.2f LUFS (master %.2f)\n", ab, out);
    check (std::fabs (ab - out) <= 1.0, "A/B igualado en sonoridad (±1 LU)");
    p.setValue (dh::lq::params::compare, 0.0f);

    // Estado: otra instancia restaura el plan y suena igual.
    juce::MemoryBlock st; p.getStateInformation (st);
    LacquerProcessor q; q.setPlayConfigDetails (2, 2, fs, 512); q.prepareToPlay (fs, 512);
    q.setStateInformation (st.getData(), (int) st.getSize());
    std::vector<float> QL, QR; run (q, 0, len, &QL, &QR); QL.clear(); QR.clear(); run (q, 0, len, &QL, &QR);
    std::vector<float> PL, PR; run (p, 0, len, &PL, &PR);
    double e = 0, d = 0; for (size_t i = (size_t) lat + 4800; i < PL.size(); ++i) { e += std::pow (PL[i] - QL[i], 2); d += PL[i] * PL[i]; }
    std::printf ("estado: %zu bytes, diferencia %.1f dB\n", st.getSize(), 10 * std::log10 (e / d + 1e-30));
    check (10 * std::log10 (e / d + 1e-30) < -60.0, "el estado guardado reproduce el master");

    // Modo archivo: la primera pasada procesa todo; volver a un ajuste anterior no procesa nada, y el master que sale es el mismo.
    {
        LacquerProcessor f; f.setPlayConfigDetails (2, 2, fs, 512); f.prepareToPlay (fs, 512);
        auto nowMs = [] { return juce::Time::getMillisecondCounterHiRes(); };
        auto waitReady = [&] (double since) {
            for (;;) { juce::Thread::sleep (20); if (f.fileState.load() == LacquerProcessor::FileState::ready && f.fileRenderSeconds() == 0.0 && nowMs() - since > 500.0) return nowMs() - since; }
        };
        auto exportTo = [&] (const char* name) { juce::File d = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile (name); juce::String err; f.exportMaster (d, 24, err); return d; };
        auto same = [&] (const juce::File& a, const juce::File& b) { juce::MemoryBlock x, y; a.loadFileAsData (x); b.loadFileAsData (y); return x == y; };
        const juce::File src (juce::String::fromUTF8 (argv[1]));
        double t0 = nowMs(); f.openFile (src);
        if (const char* shot = std::getenv ("LQ_SHOT"))      // captura de la barra de progreso a mitad del proceso
        {
            std::unique_ptr<juce::AudioProcessorEditor> ed (f.createEditor());
            juce::Thread::sleep (4500);
            for (auto* ch : ed->getChildComponent (0)->getChildren())     // lo que el temporizador del editor esconde mientras masteriza
                if (auto* tb = dynamic_cast<juce::TextButton*> (ch)) if (tb->getButtonText() == "Play" || tb->getButtonText() == "Export master" || tb->getButtonText() == "Back to live") tb->setVisible (false);
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            juce::PNGImageFormat png; juce::File o (shot); o.deleteFile(); juce::FileOutputStream os (o); png.writeImageToStream (img, os);
        }
        while (f.fileState.load() != LacquerProcessor::FileState::ready) { juce::Thread::sleep (20); if (f.fileState.load() == LacquerProcessor::FileState::failed) break; }
        std::printf ("archivo: primera pasada %.1f s\n", (nowMs() - t0) / 1000.0);
        check (f.fileState.load() == LacquerProcessor::FileState::ready, "el archivo se masteriza");
        const auto m1 = exportTo ("lq_a.wav");
        const float st0 = f.value (dh::lq::params::style);
        t0 = nowMs(); f.setValue (dh::lq::params::style, 3.0f);
        const double tStyle = waitReady (t0);
        const auto m2 = exportTo ("lq_b.wav");
        std::printf ("archivo: otro estilo %.1f s (incluye 0,3 s de espera por si se sigue moviendo)\n", tStyle / 1000.0);
        check (! same (m1, m2), "otro estilo da otro master");
        t0 = nowMs(); f.setValue (dh::lq::params::style, st0);
        const double tBack = waitReady (t0);
        const auto m3 = exportTo ("lq_c.wav");
        std::printf ("archivo: volver al estilo anterior %.1f s\n", tBack / 1000.0);
        check (same (m1, m3), "volver al ajuste anterior da exactamente el mismo master");
        check (tBack < tStyle * 0.35 || tBack < 1500.0, "volver al ajuste anterior no vuelve a procesar");
        m1.deleteFile(); m2.deleteFile(); m3.deleteFile();
    }
    std::printf (fails ? "\n%d FALLAS\n" : "\nTODO OK\n", fails);
    return fails ? 1 : 0;
}
