// Banco de pruebas del DSP, sin JUCE: clang++ -std=c++20 -O2 -ISource -I../../shared/dsp tools/dsptest.cpp
#include "dsp/Korg35.h"
#include "dsp/Oscillators.h"
#include <complex>
#include <cstdio>
#include <vector>

using namespace dh;

// Ganancia (dB) de una senoidal chica a través del filtro, medida por correlación.
template <typename F>
static float gainDb (F& filt, float f, float fc, float k, float fs)
{
    filt.reset();
    const int n = (int) (fs * 0.5f);
    double re = 0, im = 0;
    for (int i = 0; i < n; ++i)
    {
        const float x = 0.01f * std::sin (kTwoPi * f * i / fs);
        const float y = filt.process (x, fc, k, fs);
        if (i > n / 2) { re += y * std::sin (kTwoPi * f * i / fs); im += y * std::cos (kTwoPi * f * i / fs); }
    }
    const double amp = 2.0 * std::sqrt (re * re + im * im) / (n - n / 2 - 1);
    return (float) (20.0 * std::log10 (amp / 0.01 + 1e-12));
}

// Amplitud RMS de la salida sin entrada (tras un impulso): mide autooscilación.
template <typename F>
static float selfOsc (F& filt, float fc, float k, float fs, float& freqOut)
{
    filt.reset();
    const int n = (int) fs;
    double sum = 0; int cross = 0; float prev = 0;
    for (int i = 0; i < n; ++i)
    {
        const float y = filt.process (i == 0 ? 0.1f : 0.0f, fc, k, fs);
        if (i > n / 2) { sum += y * y; if (prev < 0 && y >= 0) ++cross; }
        prev = y;
    }
    freqOut = cross / 0.5f;
    return (float) std::sqrt (sum / (n / 2));
}

// Energía fuera de los armónicos (aliasing) de un oscilador, por DFT de armónicos.
template <typename G>
static float aliasDb (G gen, float f0, float fs)
{
    const int n = 1 << 16;
    std::vector<float> x (n);
    for (int i = 0; i < n; ++i) x[i] = gen();
    double total = 0; for (float v : x) total += v * v;
    double harm = 0;
    for (int h = 1; h * f0 < fs / 2; ++h)
    {
        double re = 0, im = 0;
        for (int i = 0; i < n; ++i)
        {
            const double w = 0.5 - 0.5 * std::cos (2 * M_PI * i / n);
            re += w * x[i] * std::cos (2 * M_PI * h * f0 * i / fs);
            im += w * x[i] * std::sin (2 * M_PI * h * f0 * i / fs);
        }
        harm += 8.0 * (re * re + im * im) / ((double) n * n);   // Hann: amplitud coherente n/2
    }
    const double resid = std::max (1e-15, total / n - harm);
    return (float) (10 * std::log10 (resid / (total / n)));
}

// THD de una senoidal de 100 Hz a través del filtro abierto (fc 15 kHz).
static float thd (Ms20Filter& f, float amp, float fs)
{
    f.reset();
    const int n = (int) fs; const float f0 = 100.0f;
    std::vector<float> y (n);
    for (int i = 0; i < n; ++i) y[i] = f.process (amp * std::sin (kTwoPi * f0 * i / fs), 15000.0f, 0.0f, fs);
    double h[6] = {};
    for (int k = 1; k <= 5; ++k)
    {
        double re = 0, im = 0;
        for (int i = n / 2; i < n; ++i) { re += y[i] * std::cos (kTwoPi * k * f0 * i / fs); im += y[i] * std::sin (kTwoPi * k * f0 * i / fs); }
        h[k] = std::sqrt (re * re + im * im);
    }
    return (float) (100.0 * std::sqrt (h[2] * h[2] + h[3] * h[3] + h[4] * h[4] + h[5] * h[5]) / h[1]);
}

static float cutoffHz (float k) { return 500.0f * std::exp2 (0.823f * (k - 5) + 0.0318f * (k - 5) * (k - 5)); }

int main()
{
    const float fs = 48000.0f * 4.0f;
    for (int rev = 1; rev <= 2; ++rev)
    {
        Ms20Filter lpf; lpf.mode = Ms20Filter::Lowpass; lpf.revision = rev;
        Ms20Filter hpf; hpf.mode = Ms20Filter::Highpass; hpf.revision = rev;
        std::printf ("=== REV%d ===\nLPF fc=1k (K=0 / 1 / 1.8):\n", rev);
        for (float f : { 100.f, 300.f, 1000.f, 3000.f, 10000.f })
            std::printf ("  %6.0f Hz: %6.1f %6.1f %6.1f dB\n", f, gainDb (lpf, f, 1000, 0.f, fs), gainDb (lpf, f, 1000, 1.f, fs), gainDb (lpf, f, 1000, 1.8f, fs));
        std::printf ("HPF fc=1k (K=0 / 1 / 1.8):\n");
        for (float f : { 62.5f, 125.f, 250.f, 1000.f, 4000.f, 10000.f })
            std::printf ("  %6.0f Hz: %6.1f %6.1f %6.1f dB\n", f, gainDb (hpf, f, 1000, 0.f, fs), gainDb (hpf, f, 1000, 1.f, fs), gainDb (hpf, f, 1000, 1.8f, fs));
        std::printf ("Autooscilación con corte en 5 (%.0f Hz):\n", cutoffHz (5));
        for (float K : { 1.95f, 2.02f, 2.1f, 2.25f })
        {
            float fl, fh; float al = selfOsc (lpf, cutoffHz (5), K, fs, fl); float ah = selfOsc (hpf, cutoffHz (5), K, fs, fh);
            std::printf ("  K=%.2f  LPF rms=%.3f f=%.0f | HPF rms=%.3f f=%.0f\n", K, al, fl, ah, fh);
        }
        std::printf ("THD LPF abierto (100 Hz) según amplitud:\n");
        for (float a : { 0.25f, 0.5f, 1.0f, 1.5f, 2.0f })
            std::printf ("  amp %.2f → %.1f %%\n", a, thd (lpf, a, fs));
    }
    return 0;
}
