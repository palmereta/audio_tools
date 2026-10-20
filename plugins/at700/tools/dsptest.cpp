// Banco de pruebas del DSP del AT700: clang++ -std=c++20 -O2 -ISource/dsp -I../../shared/dsp tools/dsptest.cpp
#include "Engine700.h"
#include <chrono>
#include <cstdio>

using namespace dh;

static float freqOf (const std::vector<float>& x, float fs)
{
    int n = 0; float first = -1, last = -1;
    for (size_t i = 1; i < x.size(); ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f) { const float t = (float) i / fs; if (first < 0) first = t; last = t; ++n; }
    return n > 1 ? (float) (n - 1) / (last - first) : 0.0f;
}

struct Run
{
    Engine700 e;
    explicit Run (int os = 2) { e.prepare (48000.0, 512, os); }
    std::vector<float> play (float secs, std::vector<float>* right = nullptr)
    {
        const int n = (int) (secs * 48000.0f);
        std::vector<float> L ((size_t) n), R ((size_t) n);
        for (int pos = 0; pos < n; pos += 256)
        {
            const int k = std::min (256, n - pos);
            e.render (L.data() + pos, R.data() + pos, k);
        }
        if (right) *right = R;
        return L;
    }
    void set (P700 id, float v) { e.p[(size_t) id] = v; }
    void cable (int src, int dst) { e.patch.src[(size_t) dst] = (int16_t) src; e.computePoly(); }
};

static float rms (const std::vector<float>& x, size_t from = 0)
{
    double s = 0; for (size_t i = from; i < x.size(); ++i) s += x[i] * x[i];
    return (float) std::sqrt (s / std::max<size_t> (1, x.size() - from));
}
static float peak (const std::vector<float>& x) { float m = 0; for (float v : x) m = std::max (m, std::fabs (v)); return m; }

int main()
{
    // 1) Patch de fábrica: VCO-1 (sierra) → VCF-1 → VCA-1 (ADSR-1) → PAN 1.
    {
        Run r;
        r.e.kb.noteOn (69, 0.8f);                                       // A3 del manual = 440 Hz en 8'
        auto x = r.play (1.0f);
        std::printf ("Patch de fábrica, A3: rms %.3f pico %.3f\n", rms (x, 4800), peak (x));
        // Frecuencia: VCO-1 directo al VCA por el VCF abierto.
        Run f; f.set (P700::vcf1_cutoff, 1.0f); f.set (P700::vcf1_mod2, 0.0f); f.set (P700::vco1_wave, 0);
        f.e.kb.noteOn (69, 0.8f); auto y = f.play (1.0f);
        std::vector<float> tail (y.begin() + 12000, y.end());
        std::printf ("  VCO-1 seno, A3 en 8': %.1f Hz (440)\n", freqOf (tail, 48000.0f));
        for (int rg : { 1, 2, 4, 5 })
        {
            Run g; g.set (P700::vcf1_cutoff, 1.0f); g.set (P700::vcf1_mod2, 0.0f); g.set (P700::vco1_wave, 0); g.set (P700::vco1_range, (float) rg);
            g.e.kb.noteOn (69, 0.8f); auto z = g.play (0.8f);
            std::vector<float> t2 (z.begin() + 12000, z.end());
            std::printf ("  rango %d: %.1f Hz\n", rg, freqOf (t2, 48000.0f));
        }
        // Seguimiento de teclado C1..C6.
        std::printf ("  teclas C1–C6:");
        for (int nn = 36; nn <= 96; nn += 12)
        {
            Run g; g.set (P700::vcf1_cutoff, 1.0f); g.set (P700::vcf1_mod2, 0.0f); g.set (P700::vco1_wave, 0);
            g.e.kb.noteOn (nn, 0.8f); auto z = g.play (0.6f);
            std::vector<float> t2 (z.begin() + 9600, z.end());
            std::printf (" %.1f", freqOf (t2, 48000.0f));
        }
        std::printf (" (65.4 130.8 261.6 523.3 1046.5 2093)\n");
    }
    // 2) Autooscilación de los VCF (sin entrada).
    for (int model = 0; model < 2; ++model)
    {
        std::printf ("VCF-1 %s autooscila:", model == 0 ? "703E 24 dB" : "703A 12 dB");
        for (float res : { 0.7f, 0.8f, 0.9f, 1.0f })
        {
            Run g; g.e.st.vcf1Model = model;
            g.set (P700::vcf1_lvl1, 0.0f); g.set (P700::vcf1_res, res); g.set (P700::vcf1_cutoff, 0.55f); g.set (P700::vcf1_mod2, 0.0f);
            g.set (P700::vca1_gain, 1.0f);
            g.e.kb.noteOn (60, 0.8f);
            auto z = g.play (1.0f);
            std::vector<float> t2 (z.begin() + 24000, z.end());
            std::printf ("  res %.1f: rms %.3f f %.0f", res, rms (t2), freqOf (t2, 48000.0f));
        }
        std::printf ("\n");
    }
    // Seguimiento de la autooscilación del 703E (KEY 10).
    {
        std::printf ("703E autooscilando, KEY 10:");
        for (int nn : { 48, 60, 72 })
        {
            Run g; g.set (P700::vcf1_lvl1, 0.0f); g.set (P700::vcf1_res, 0.95f); g.set (P700::vcf1_cutoff, 0.3f); g.set (P700::vcf1_mod2, 0.0f);
            g.set (P700::vca1_gain, 1.0f); g.e.kb.noteOn (nn, 0.8f);
            auto z = g.play (1.0f); std::vector<float> t2 (z.begin() + 24000, z.end());
            std::printf ("  n%d %.0f Hz", nn, freqOf (t2, 48000.0f));
        }
        std::printf ("\n");
    }
    // 3) Pérdida de banda pasante del 703E con la resonancia.
    {
        std::printf ("703E banda pasante (sierra 110 Hz, corte alto):");
        for (float res : { 0.0f, 0.4f, 0.7f })
        {
            Run g; g.set (P700::vcf1_res, res); g.set (P700::vcf1_cutoff, 0.95f); g.set (P700::vcf1_mod2, 0.0f);
            g.e.kb.noteOn (45, 0.8f); auto z = g.play (0.8f);
            std::printf ("  res %.1f: %.3f", res, rms (z, 12000));
        }
        std::printf ("\n");
    }
    // 4) Secuenciador: 4 pasos a 120 pasos/min (x10, ~1/4), salida A al VCO-1 EXT.
    {
        Run g;
        g.set (P700::clk_rate, 0.86f); g.set (P700::clk_range, 0); g.set (P700::ser_steps, 3);
        for (int s = 0; s < 4; ++s) { g.e.p[(size_t) ((int) P700::step1_a + s * 5)] = 0.1f * (float) s; g.e.p[(size_t) ((int) P700::step1_div + s * 5)] = 3; }
        g.cable (jk700::par_a1, jk700::vco1_ext);
        g.set (P700::vco1_mod2, 1.0f);
        g.cable (jk700::clk_out1, jk700::adsr1_gate); g.set (P700::adsr1_mode, 2);
        g.e.seqStart();
        int steps[8]; float t = 0;
        for (int k = 0; k < 8; ++k) { g.play (0.125f); steps[k] = g.e.view.step; t += 0.125f; }
        std::printf ("Secuenciador (paso cada 1/8 s):"); for (int k : steps) std::printf (" %d", k); std::printf ("\n");
    }
    // 4b) Secuenciador con el reloj del host y buffers grandes: el host da la posición una vez por bloque y el
    //     procesador renderiza en trozos de 32; no debe confundir cada bloque con un salto (volvía al paso 1).
    for (int block : { 64, 2048 })
    {
        Run g;
        g.set (P700::ser_steps, 3);
        for (int s = 0; s < 4; ++s) g.e.p[(size_t) ((int) P700::step1_div + s * 5)] = 3;
        g.e.st.seqHost = true; g.e.st.seqFollowTransport = true; g.e.st.hostPlaying = true; g.e.st.bpm = 120.0;
        std::vector<float> L (2048), R (2048);
        double ppq = 0; int maxStep = 0, changes = 0, last = -1;
        for (int b = 0; b < (int) (4.0 * 48000 / block); ++b)
        {
            g.e.st.ppq = ppq;
            for (int pos = 0; pos < block; pos += 32) g.e.render (L.data(), R.data(), std::min (32, block - pos));
            ppq += block * 120.0 / 60.0 / 48000.0;
            if (g.e.view.step != last) { ++changes; last = g.e.view.step; }
            maxStep = std::max (maxStep, g.e.view.step);
        }
        std::printf ("Secuenciador con el host, bloques de %d: %d cambios de paso, paso máximo %d %s\n", block, changes, maxStep,
                     maxStep >= 3 && changes >= 8 ? "OK" : "FALLA");
    }
    // 4c) Reloj externo con un pulso por paso: CLOCK OUT tiene que bajar entre pulsos (quedaba siempre alto).
    {
        Seq717 q; q.steps = 4; q.pw = 0.5f; q.start();
        for (int s = 0; s < 12; ++s) { q.divide[(size_t) s] = 1.0f / 32.0f; q.fine[(size_t) s] = 1.0f; }
        int rises = 0; bool last = false;
        for (int i = 0; i < 48000; ++i)
        {
            const float ext = (i % 4800) < 2400 ? 10.0f : 0.0f;             // 10 Hz, 50 %
            q.process (ext, true, 0.0f, { 0, 0, 0, 0 }, -1.0, false, 1.0f / 48000.0f);
            const bool h = q.clockOut > 1.0f;
            if (h && ! last) ++rises;
            last = h;
        }
        std::printf ("Reloj externo 1/32: CLOCK OUT subió %d veces en 1 s (10 esperadas) %s\n", rises, rises >= 9 ? "OK" : "FALLA");
    }
    // 5) Duofónico: dos teclas → CV1 la grave, CV2 la aguda.
    {
        Run g; g.e.kb.noteOn (60, 0.8f); g.e.kb.noteOn (67, 0.8f); g.play (0.05f);
        std::printf ("Duo: CV1 %.3f V  CV2 %.3f V  (2.000 / 2.583)\n", g.e.jackValue (jk700::kb_cv1), g.e.jackValue (jk700::kb_cv2));
    }
    // 6) Poly 8 voces: un acorde, CPU.
    for (int os : { 1, 2, 4 })
    {
        Run g (os); g.e.kb.mode = Keyboard701::Poly; g.e.kb.polyVoices = 8;
        g.set (P700::vcf1_lvl2, 0.6f); g.set (P700::vcf1_lvl3, 0.5f); g.set (P700::rev_lvl1, 0.4f);
        for (int nn : { 48, 55, 60, 64, 67, 71, 74, 76 }) g.e.kb.noteOn (nn, 0.8f);
        const auto t0 = std::chrono::steady_clock::now();
        auto z = g.play (4.0f);
        const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        bool bad = false; for (float v : z) if (! std::isfinite (v)) bad = true;
        std::printf ("Poly 8 voces, %dx: CPU %.1f %% de un núcleo, pico %.2f%s\n", os, 100.0 * el / 4.0, peak (z), bad ? " NaN!" : "");
    }
    {
        Run g (2);
        g.e.kb.noteOn (60, 0.8f);
        const auto t0 = std::chrono::steady_clock::now();
        g.play (4.0f);
        const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("Duo (consola completa), 2x: CPU %.1f %%\n", 100.0 * el / 4.0);
    }
    // 7) Fuzz: perillas al azar y cables al azar.
    {
        Run g; Rng rng (77); float pk = 0; bool bad = false;
        g.e.kb.noteOn (60, 0.8f);
        for (int b = 0; b < 300; ++b)
        {
            for (int k = 0; k < 6; ++k)
            {
                const int i = (int) (rng.unipolar() * kNumP700) % kNumP700;
                const auto& pp = panelParam700 (i);
                g.e.p[(size_t) i] = pp.steps > 0 ? (float) ((int) (rng.unipolar() * pp.steps) % pp.steps) : rng.unipolar();
            }
            if (b % 20 == 0)
            {
                g.e.patch = PatchState700();
                for (int c = 0; c < 8; ++c)
                {
                    const int s = (int) (rng.unipolar() * jk700::kNumJacks) % jk700::kNumJacks, d = (int) (rng.unipolar() * jk700::kNumJacks) % jk700::kNumJacks;
                    if (jk700::jacks[s].io != jk700::In && jk700::jacks[d].io != jk700::Out && s != d) g.e.patch.src[(size_t) d] = (int16_t) s;
                }
                g.e.computePoly();
            }
            if (b % 37 == 0) g.e.kb.mode = (b / 37) % 3, g.e.kb.polyVoices = 4;
            auto z = g.play (0.02f);
            for (float v : z) { if (! std::isfinite (v)) bad = true; pk = std::max (pk, std::fabs (v)); }
        }
        std::printf ("Fuzz 300 bloques: pico %.2f%s\n", pk, bad ? ", NaN!" : ", sin NaN");
    }
    return 0;
}
