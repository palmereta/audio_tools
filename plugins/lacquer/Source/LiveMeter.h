#pragma once
// Medidor en vivo, apto para el hilo de audio: ponderación K, bloques de 100 ms, momentánea (400 ms) y corta (3 s)
// como atómicos, pico verdadero máximo, y una cola sin bloqueos con la energía de cada bloque para que el hilo de la
// interfaz calcule la integrada y el LRA (LoudnessMeter lo haría con memoria que crece; acá crece del otro lado).

#include <JuceHeader.h>
#include "dsp/Loudness.h"

namespace dh::lq {

class LiveMeter
{
public:
    void prepare (double fs)
    {
        for (int c = 0; c < 2; ++c) { kWeighting (fs, shelf[c], hp[c]); tp[c].prepare (fs); }
        blockLen = (int) std::lround (fs * 0.1);
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c) { shelf[c].reset(); hp[c].reset(); tp[c].reset(); }
        acc = 0.0; inBlock = 0; ring.fill (0.0); ringPos = 0; filled = 0;
        momentary.store (-70.0f); shortTerm.store (-70.0f); truePeak.store (-100.0f); resetFlag.store (true);
    }
    // Audio: L y R (R puede ser L).
    void process (const float* L, const float* R, int n)
    {
        double tpMax = 0.0;
        for (int i = 0; i < n; ++i)
        {
            double e = 0.0;
            for (int c = 0; c < 2; ++c)
            {
                const double x = c == 0 ? L[i] : R[i];
                tpMax = std::max (tpMax, tp[c].process (x));
                const double y = hp[c].process (shelf[c].process (x));
                e += y * y;
            }
            acc += e;
            if (++inBlock == blockLen)
            {
                const double b = acc / blockLen; acc = 0.0; inBlock = 0;
                ring[(size_t) ringPos] = b; ringPos = (ringPos + 1) % 30; filled = std::min (30, filled + 1);
                double m = 0.0, s = 0.0;
                for (int k = 0; k < filled; ++k) { const double v = ring[(size_t) ((ringPos - 1 - k + 30) % 30)]; s += v; if (k < 4) m += v; }
                momentary.store ((float) powToLufs (m / std::min (4, filled)));
                shortTerm.store ((float) powToLufs (s / filled));
                int s1, n1, s2, n2; fifo.prepareToWrite (1, s1, n1, s2, n2);
                if (n1 > 0) blocks[(size_t) s1] = b;
                fifo.finishedWrite (n1);
            }
        }
        const float t = (float) toDb (tpMax);
        if (t > truePeak.load()) truePeak.store (t);
    }

    // Interfaz: vacía la cola y actualiza la integrada y el LRA (llamar desde un único hilo, el de la interfaz).
    void updateUi()
    {
        if (resetFlag.exchange (false)) { uiBlocks.clear(); uiSt.clear(); integrated = -70.0; lra = 0.0; }
        int s1, n1, s2, n2; const int avail = fifo.getNumReady();
        fifo.prepareToRead (avail, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) push (blocks[(size_t) (s1 + i)]);
        for (int i = 0; i < n2; ++i) push (blocks[(size_t) (s2 + i)]);
        fifo.finishedRead (n1 + n2);
        if (n1 + n2 > 0) recompute();
    }
    std::atomic<float> momentary { -70.0f }, shortTerm { -70.0f }, truePeak { -100.0f };
    double integrated = -70.0, lra = 0.0;                  // del lado de la interfaz
    void requestReset() { resetFlag.store (true); truePeak.store (-100.0f); }

private:
    BiquadD shelf[2], hp[2];
    TruePeak tp[2];
    double acc = 0.0; int inBlock = 0, blockLen = 4800;
    std::array<double, 30> ring {}; int ringPos = 0, filled = 0;
    juce::AbstractFifo fifo { 1024 };
    std::array<double, 1024> blocks {};
    std::atomic<bool> resetFlag { true };
    std::vector<double> uiBlocks, uiSt;

    void push (double b)
    {
        uiBlocks.push_back (b);
        if (uiBlocks.size() >= 30) { double s = 0; for (size_t k = uiBlocks.size() - 30; k < uiBlocks.size(); ++k) s += uiBlocks[k]; uiSt.push_back (powToLufs (s / 30.0)); }
    }
    void recompute()
    {
        std::vector<double> g;
        for (size_t j = 3; j < uiBlocks.size(); ++j) { const double p = 0.25 * (uiBlocks[j] + uiBlocks[j - 1] + uiBlocks[j - 2] + uiBlocks[j - 3]); if (powToLufs (p) > -70.0) g.push_back (p); }
        if (g.empty()) { integrated = -70.0; } else
        {
            double s = 0; for (double p : g) s += p;
            const double rel = powToLufs (s / g.size()) - 10.0;
            double s2 = 0; size_t n2 = 0; for (double p : g) if (powToLufs (p) > rel) { s2 += p; ++n2; }
            integrated = n2 ? powToLufs (s2 / n2) : -70.0;
        }
        std::vector<double> v; for (double l : uiSt) if (l >= -70.0) v.push_back (l);
        if (v.size() < 2) { lra = 0.0; return; }
        double s = 0; for (double l : v) s += lufsToPow (l);
        const double rel = powToLufs (s / v.size()) - 20.0;
        std::vector<double> w; for (double l : v) if (l >= rel) w.push_back (l);
        if (w.size() < 2) { lra = 0.0; return; }
        std::sort (w.begin(), w.end());
        auto prc = [&] (double p) { return w[std::min (w.size() - 1, (size_t) std::lround ((w.size() - 1) * p / 100.0))]; };
        lra = prc (95) - prc (10);
    }
};

} // namespace dh::lq
