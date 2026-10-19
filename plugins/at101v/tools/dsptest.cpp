// Banco de pruebas del DSP del AT101V: clang++ -std=c++20 -O2 -ISource/dsp -I../../shared/dsp tools/dsptest.cpp
#include "Ir3109.h"
#include "Engine101.h"
#include <string>
#include <cstdio>
#include <vector>
using namespace dh;

static float gainDb (Ir3109& f, float freq, float fc, float k, float fs, float amp = 0.01f)
{
    f.reset(); const int n = (int) (fs * 0.5f); double re = 0, im = 0;
    for (int i = 0; i < n; ++i)
    {
        const float x = amp * std::sin (kTwoPi * freq * i / fs);
        const float y = f.process (x, fc, k, fs, 1.0f, 0.0f);
        if (i > n / 2) { re += y * std::sin (kTwoPi * freq * i / fs); im += y * std::cos (kTwoPi * freq * i / fs); }
    }
    return (float) (20 * std::log10 (2 * std::sqrt (re * re + im * im) / (n - n / 2 - 1) / amp + 1e-12));
}
static float selfOsc (Ir3109& f, float fc, float k, float fs, float& fo)
{
    f.reset(); const int n = (int) fs; double sum = 0; int cross = 0; float prev = 0;
    for (int i = 0; i < n; ++i)
    {
        const float y = f.process (i == 0 ? 0.1f : 0.0f, fc, k, fs, 1.0f, 0.0f);
        if (i > n / 2) { sum += y * y; if (prev < 0 && y >= 0) ++cross; } prev = y;
    }
    fo = cross / 0.5f; return (float) std::sqrt (sum / (n / 2));
}
static float thd (Ir3109& f, float amp, float fs)
{
    f.reset(); const int n = (int) fs; const float f0 = 100; std::vector<float> y (n);
    for (int i = 0; i < n; ++i) y[i] = f.process (amp * std::sin (kTwoPi * f0 * i / fs), 18000, 0, fs, 1.0f, 0.0f);
    double h[6] = {};
    for (int k = 1; k <= 5; ++k) { double re = 0, im = 0; for (int i = n / 2; i < n; ++i) { re += y[i] * std::cos (kTwoPi * k * f0 * i / fs); im += y[i] * std::sin (kTwoPi * k * f0 * i / fs); } h[k] = std::sqrt (re * re + im * im); }
    return (float) (100 * std::sqrt (h[2] * h[2] + h[3] * h[3] + h[4] * h[4] + h[5] * h[5]) / h[1]);
}
int main()
{
    const float fs = 192000; Ir3109 f;
    std::printf ("Respuesta fc=1 kHz (k=0 / 2 / 3.5):\n");
    for (float fr : { 100.f, 500.f, 1000.f, 2000.f, 4000.f, 8000.f })
        std::printf ("  %5.0f Hz: %6.1f %6.1f %6.1f dB\n", fr, gainDb (f, fr, 1000, 0, fs), gainDb (f, fr, 1000, 2, fs), gainDb (f, fr, 1000, 3.5f, fs));
    std::printf ("Autooscilación fc=1 kHz:\n");
    for (float k : { 3.8f, 3.95f, 4.05f, 4.2f, 4.5f }) { float fo; float r = selfOsc (f, 1000, k, fs, fo); std::printf ("  k=%.2f rms=%.3f f=%.0f\n", k, r, fo); }
    for (float fc : { 220.f, 2000.f, 8000.f }) { float fo; float r = selfOsc (f, fc, 4.3f, fs, fo); std::printf ("  fc=%5.0f k=4.3 rms=%.3f f=%.0f\n", fc, r, fo); }
    std::printf ("THD abierto 100 Hz: ");
    for (float a : { 0.25f, 0.5f, 1.0f, 2.0f }) std::printf ("%.2f→%.1f%%  ", a, thd (f, a, fs));
    std::printf ("\n");

    // ── Motor completo ──
    auto run = [] (Engine101& e, int n, float* pk = nullptr, double* rms = nullptr, std::string* notes = nullptr) {
        std::vector<float> L (512), R (512); float peak = 0; double s2 = 0; long cnt = 0;
        for (int done = 0; done < n; done += 512)
        {
            const int m = std::min (512, n - done);
            e.numMidiEvents = 0; e.render (L.data(), R.data(), m);
            for (int i = 0; i < m; ++i) { peak = std::max (peak, std::fabs (L[i])); s2 += L[i] * L[i]; ++cnt; if (! std::isfinite (L[i])) { std::printf ("NaN!\n"); std::exit (1); } }
            if (notes) for (int k = 0; k < e.numMidiEvents; ++k) if (e.midiEvents[k].on) *notes += std::to_string (e.midiEvents[k].note) + " ";
        }
        if (pk) *pk = peak; if (rms) *rms = std::sqrt (s2 / std::max (1L, cnt));
    };
    const double sr = 48000;
    auto fresh = [&] (Engine101& e) { e.prepare (sr, 512, 4); e.p = panelDefaults(); };
    {
        Engine101 e; fresh (e); e.p[(int) P::vcf_freq] = 1; e.p[(int) P::vcf_env] = 0; e.p[(int) P::env_s] = 1;
        e.keyDown (60, 0.8f); float pk; double r; run (e, 24000, &pk, &r);
        std::printf ("Sierra 10, filtro abierto: pico %.2f rms %.2f\n", pk, r);
        for (int m : { (int) P::mix_pulse, (int) P::mix_sub, (int) P::mix_noise }) e.p[m] = 1;
        run (e, 24000, &pk, &r); std::printf ("Las cuatro en 10: pico %.2f rms %.2f\n", pk, r);
    }
    {
        std::printf ("Autooscilación (RES 10, mezcla 0, KYBD 10, FREQ 5):");
        for (int note : { 48, 60, 72 })
        {
            Engine101 e; fresh (e); e.p[(int) P::mix_saw] = 0; e.p[(int) P::vcf_res] = 1; e.p[(int) P::vcf_freq] = 0.5f;
            e.p[(int) P::vcf_env] = 0; e.p[(int) P::env_s] = 1; e.keyDown (note, 0.8f);
            std::vector<float> L (48000), R (48000); e.render (L.data(), R.data(), 48000);
            int cross = 0; double s2 = 0; for (int i = 24000; i < 48000; ++i) { if (L[i - 1] < 0 && L[i] >= 0) ++cross; s2 += L[i] * L[i]; }
            std::printf ("  n%d: %d Hz rms %.2f", note, cross * 2, std::sqrt (s2 / 24000));
        }
        std::printf ("\n");
    }
    {
        Engine101 e; fresh (e); e.p[(int) P::arp_mode] = 2; e.p[(int) P::lfo_rate] = 0.6f;
        std::string notes; run (e, 512);
        e.keyDown (60, 0.8f); e.keyDown (64, 0.8f); e.keyDown (67, 0.8f); run (e, 48000 * 2, nullptr, nullptr, &notes);
        std::printf ("Arpegio U&D (%.1f Hz): %s\n", cal101::lfoHz (0.6f), notes.c_str());
    }
    {
        Engine101 e; fresh (e); e.setLoading (true);
        for (int n : { 48, 60, 63 }) { e.keyDown (n, 0.8f); e.keyUp (n); }
        e.addRest(); e.markTie(); e.keyDown (55, 0.8f); e.keyUp (55); e.keyDown (55, 0.8f); e.keyUp (55);
        e.setLoading (false); run (e, 512);
        e.p[(int) P::seq_play] = 1; e.p[(int) P::lfo_rate] = 0.6f; std::string notes; run (e, 48000 * 2, nullptr, nullptr, &notes);
        std::printf ("Secuencia (%d pasos): %s\n", e.seq.length, notes.c_str());
        e.keyDown (65, 0.8f); notes.clear(); run (e, 48000, nullptr, nullptr, &notes);
        std::printf ("  transpuesta +5: %s\n", notes.c_str());
    }
    {
        Engine101 e; fresh (e); e.st.voices = 8; e.st.unison = 2; float pk; double r;
        for (int n : { 48, 55, 60, 64, 67 }) e.keyDown (n, 0.8f);
        run (e, 24000, &pk, &r); std::printf ("Poly 8 x unison 2, 5 notas: voces %d pico %.2f rms %.2f\n", e.activeVoiceCount(), pk, r);
    }
    {
        // Fuzz: parámetros al azar cada bloque, notas al azar.
        Engine101 e; fresh (e); Rng rg (7); float pk = 0;
        for (int b = 0; b < 400; ++b)
        {
            for (int i = 0; i < kNumP; ++i) { const auto& pp = panelParam (i); e.p[i] = pp.steps > 0 ? (float) (rg.next() % pp.steps) : rg.unipolar(); }
            e.st.voices = (b / 50) % 2 ? 6 : 1; e.st.clockSrc = (b / 30) % 2; e.st.drive = rg.unipolar(); e.bender = rg.bipolar(); e.lfoPush = rg.unipolar();
            e.st.ppq += 0.1;
            if (rg.next() % 3 == 0) e.keyDown (36 + rg.next() % 48, rg.unipolar()); else e.keyUp (36 + rg.next() % 48);
            float p2; run (e, 512, &p2); pk = std::max (pk, p2);
        }
        std::printf ("Fuzz 400 bloques: pico %.2f, sin NaN\n", pk);
    }
}
