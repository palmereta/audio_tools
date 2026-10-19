#pragma once
// EQ dinámica de dos bandas (docs/research.md §4.4, §6.3-2): una para la aspereza (2-5 kHz) y otra para la
// sibilancia (5-10 kHz). Cada banda mide su nivel relativo al de toda la señal (así no depende del volumen de la
// mezcla) y, cuando supera el umbral, baja una campana en esa frecuencia con relación 3:1 hasta un recorte máximo.
// Los coeficientes se recalculan cada 16 muestras con un contador interno: el resultado no depende del tamaño de
// bloque del host.

#include "Filters.h"

namespace dh::lq {

struct DynBandParams
{
    bool on = false;
    double hz = 3000, q = 1.4, thresholdDb = 0, ratio = 3, maxCutDb = 3, attackMs = 3, releaseMs = 90;
};

class DynBand
{
public:
    static constexpr int kUpdate = 16;
    void prepare (double sampleRate, const DynBandParams& p)
    {
        fs = sampleRate; P = p;
        det.set (bandPass (p.hz, p.q * 2.0, fs));
        aA = std::exp (-1.0 / (p.attackMs * 1e-3 * fs)); aR = std::exp (-1.0 / (p.releaseMs * 1e-3 * fs));
        aW = std::exp (-1.0 / (0.050 * fs));                       // el nivel general, más lento (50 ms)
        reset();
    }
    void reset() { det.reset(); fl.reset(); fr.reset(); eb = ew = 1e-12; cnt = 0; gr = 0.0; fl.set (Coefs{}); fr.set (Coefs{}); grMax = grSum = 0.0; grN = 0; }

    // Nivel relativo de la banda (dB) para la muestra media m; lo usa también el aprendizaje para fijar el umbral.
    inline double relLevel (double m)
    {
        const double b = det.process (m), pb = b * b, pw = m * m;
        eb = pb > eb ? aA * eb + (1.0 - aA) * pb : aR * eb + (1.0 - aR) * pb;
        ew = aW * ew + (1.0 - aW) * pw;
        return 10.0 * std::log10 ((eb + 1e-15) / (ew + 1e-12));
    }

    inline void process (double& l, double& r)
    {
        if (! P.on) return;
        const double rel = relLevel (0.5 * (l + r));
        if (++cnt >= kUpdate)
        {
            cnt = 0;
            const double over = rel - P.thresholdDb;
            gr = over > 0.0 ? std::min (P.maxCutDb, over * (1.0 - 1.0 / P.ratio)) : 0.0;
            const auto k = peaking (P.hz, P.q, -gr, fs);
            fl.set (k); fr.set (k);
            grMax = std::max (grMax, gr); grSum += gr; ++grN;
        }
        l = fl.process (l); r = fr.process (r);
    }
    double currentGrDb() const { return gr; }
    double averageGrDb() const { return grN ? grSum / (double) grN : 0.0; }
    double maxGrDb() const { return grMax; }
    const DynBandParams& params() const { return P; }

private:
    double fs = 48000.0;
    DynBandParams P;
    Biquad det, fl, fr;
    double aA = 0.9, aR = 0.999, aW = 0.9999, eb = 1e-12, ew = 1e-12, gr = 0.0;
    int cnt = 0;
    double grMax = 0.0, grSum = 0.0; long long grN = 0;
};

} // namespace dh::lq
