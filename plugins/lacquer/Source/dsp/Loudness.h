#pragma once
// Medición de sonoridad y pico verdadero según ITU-R BS.1770-5 y EBU Tech 3341 / 3342, para cualquier
// frecuencia de muestreo. Todo en double y sin hilos: el mismo número en el plugin, el render y las pruebas.
//
//   - Ponderación K: las dos etapas de la norma (estante "cabeza esférica" + pasa altos RLB) con los coeficientes
//     derivados para cualquier fs como libebur128 / FFmpeg (a 48 kHz dan la tabla de la norma con 14 dígitos).
//   - Bloques de 100 ms; momentánea = 4 bloques (400 ms), corta = 30 bloques (3 s), integrada con compuerta
//     absoluta −70 LUFS y relativa −10 LU sobre bloques de 400 ms con 75 % de solapamiento.
//   - LRA (Tech 3342): sonoridad corta cada 100 ms, compuerta −70 LUFS y −20 LU, P95 − P10 (rango más cercano).
//   - Pico verdadero (Anexo 2): sobremuestreo ×4 con el filtro de 48 coeficientes de la norma (×2 a 96 kHz,
//     ×1 desde 192 kHz, como sugiere la norma); se informa en dBTP.

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace dh::lq {

constexpr double kPiD = 3.14159265358979323846;

inline double toDb (double x) { return x > 1e-30 ? 20.0 * std::log10 (x) : -600.0; }
inline double powToLufs (double p) { return p > 1e-30 ? -0.691 + 10.0 * std::log10 (p) : -700.0; }
inline double lufsToPow (double l) { return std::pow (10.0, (l + 0.691) / 10.0); }

// Biquad en double, forma directa II transpuesta.
struct BiquadD
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
};

// Las dos etapas de la ponderación K para la frecuencia de muestreo fs.
inline void kWeighting (double fs, BiquadD& shelf, BiquadD& hp)
{
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (kPiD * f0 / fs), Vh = std::pow (10.0, G / 20.0), Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;
        shelf.b0 = (Vh + Vb * K / Q + K * K) / a0; shelf.b1 = 2.0 * (K * K - Vh) / a0; shelf.b2 = (Vh - Vb * K / Q + K * K) / a0;
        shelf.a1 = 2.0 * (K * K - 1.0) / a0; shelf.a2 = (1.0 - K / Q + K * K) / a0;
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (kPiD * f0 / fs), a0 = 1.0 + K / Q + K * K;
        hp.b0 = 1.0; hp.b1 = -2.0; hp.b2 = 1.0;            // sin normalizar, como la tabla de la norma
        hp.a1 = 2.0 * (K * K - 1.0) / a0; hp.a2 = (1.0 - K / Q + K * K) / a0;
    }
}

// Interpolador ×4 del Anexo 2 (48 coeficientes, 4 fases de 12). Las fases 2 y 3 son la 1 y la 0 invertidas.
inline const std::array<std::array<double, 12>, 4>& truePeakTaps()
{
    static const std::array<std::array<double, 12>, 4> t {{
        {{ 0.0017089843750, 0.0109863281250, -0.0196533203125, 0.0332031250000, -0.0594482421875, 0.1373291015625,
           0.9721679687500, -0.1022949218750, 0.0476074218750, -0.0266113281250, 0.0148925781250, -0.0083007812500 }},
        {{ -0.0291748046875, 0.0292968750000, -0.0517578125000, 0.0891113281250, -0.1665039062500, 0.4650878906250,
           0.7797851562500, -0.2003173828125, 0.1015625000000, -0.0582275390625, 0.0330810546875, -0.0189208984375 }},
        {{ -0.0189208984375, 0.0330810546875, -0.0582275390625, 0.1015625000000, -0.2003173828125, 0.7797851562500,
           0.4650878906250, -0.1665039062500, 0.0891113281250, -0.0517578125000, 0.0292968750000, -0.0291748046875 }},
        {{ -0.0083007812500, 0.0148925781250, -0.0266113281250, 0.0476074218750, -0.1022949218750, 0.9721679687500,
           0.1373291015625, -0.0594482421875, 0.0332031250000, -0.0196533203125, 0.0109863281250, 0.0017089843750 }},
    }};
    return t;
}

// Pico verdadero de un canal, muestra a muestra (estado de 12 muestras).
class TruePeak
{
public:
    void prepare (double fs) { factor = fs < 96000.0 ? 4 : (fs < 192000.0 ? 2 : 1); hist.fill (0.0); pos = 0; peak = 0.0; }
    void reset() { hist.fill (0.0); pos = 0; peak = 0.0; }
    // Devuelve el máximo absoluto de las sub-muestras de esta muestra.
    inline double process (double x)
    {
        hist[(size_t) pos] = x; pos = (pos + 1) % 12;
        double m = std::fabs (x), interp = 0.0;
        if (factor > 1)
        {
            const auto& T = truePeakTaps();
            const int step = 4 / factor;
            for (int ph = 0; ph < 4; ph += step)
            {
                double acc = 0.0;
                for (int k = 0; k < 12; ++k) acc += T[(size_t) ph][(size_t) k] * hist[(size_t) ((pos + k) % 12)];
                m = std::max (m, std::fabs (acc));
                interp = std::max (interp, std::fabs (acc));
            }
        }
        lastInterp = interp;
        peak = std::max (peak, m);
        return m;
    }
    // Para el limitador: el máximo del tramo entre las muestras n−6 y n−5 (las cuatro sub-muestras y las dos
    // muestras que las encierran). Todo lo que devuelve se refiere a ese instante, 5.5 muestras atrás.
    inline double processAligned (double x)
    {
        process (x);
        const double a = std::fabs (hist[(size_t) ((pos + 5) % 12)]), b = std::fabs (hist[(size_t) ((pos + 6) % 12)]);
        return std::max (lastInterp, std::max (a, b));
    }
    double maxPeak() const { return peak; }
private:
    std::array<double, 12> hist {};
    int pos = 0, factor = 4;
    double peak = 0.0, lastInterp = 0.0;
};

// Medidor completo de un programa estéreo (o mono). Alimentar con process(); leer en cualquier momento.
class LoudnessMeter
{
public:
    void prepare (double sampleRate, int channels = 2)
    {
        fs = sampleRate; nch = std::clamp (channels, 1, 2);
        for (int c = 0; c < 2; ++c) { kWeighting (fs, shelf[(size_t) c], hp[(size_t) c]); tp[(size_t) c].prepare (fs); }
        blockLen = (int) std::lround (fs * 0.1);
        reset();
    }
    void reset()
    {
        for (int c = 0; c < 2; ++c) { shelf[(size_t) c].reset(); hp[(size_t) c].reset(); tp[(size_t) c].reset(); }
        acc = 0.0; inBlock = 0; blocks.clear(); stHistory.clear(); samplePeak = 0.0;
    }

    void process (const float* L, const float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            double e = 0.0;
            for (int c = 0; c < nch; ++c)
            {
                const double x = (double) (c == 0 ? L[i] : (R != nullptr ? R[i] : L[i]));
                samplePeak = std::max (samplePeak, std::fabs (x));
                tp[(size_t) c].process (x);
                const double y = hp[(size_t) c].process (shelf[(size_t) c].process (x));
                e += y * y;                               // pesos de canal 1.0 para L y R
            }
            acc += e;
            if (++inBlock == blockLen)
            {
                blocks.push_back (acc / (double) blockLen);
                acc = 0.0; inBlock = 0;
                if (blocks.size() >= 30) stHistory.push_back (windowLufs (30));
            }
        }
    }

    double momentary() const { return blocks.size() >= 4 ? windowLufs (4) : -700.0; }
    double shortTerm() const { return blocks.size() >= 30 ? windowLufs (30) : (blocks.empty() ? -700.0 : windowLufs ((int) blocks.size())); }

    // Integrada (BS.1770, compuerta absoluta y relativa) sobre todo lo medido.
    double integrated() const
    {
        std::vector<double> g;
        for (size_t j = 3; j < blocks.size(); ++j)           // bloques de 400 ms con salto de 100 ms
        {
            const double p = (blocks[j] + blocks[j - 1] + blocks[j - 2] + blocks[j - 3]) * 0.25;
            if (powToLufs (p) > -70.0) g.push_back (p);
        }
        if (g.empty()) return -700.0;
        double s = 0.0; for (double p : g) s += p;
        const double rel = powToLufs (s / (double) g.size()) - 10.0;
        double s2 = 0.0; size_t n2 = 0;
        for (double p : g) if (powToLufs (p) > rel) { s2 += p; ++n2; }
        return n2 ? powToLufs (s2 / (double) n2) : -700.0;
    }

    // LRA según Tech 3342 (sonoridad corta cada 100 ms).
    double lra() const
    {
        std::vector<double> v;
        for (double l : stHistory) if (l >= -70.0) v.push_back (l);
        if (v.size() < 2) return 0.0;
        double s = 0.0; for (double l : v) s += lufsToPow (l);
        const double rel = powToLufs (s / (double) v.size()) - 20.0;
        std::vector<double> w;
        for (double l : v) if (l >= rel) w.push_back (l);
        if (w.size() < 2) return 0.0;
        std::sort (w.begin(), w.end());
        auto prc = [&] (double p) { const size_t k = (size_t) std::lround ((double) (w.size() - 1) * p / 100.0); return w[std::min (k, w.size() - 1)]; };
        return prc (95.0) - prc (10.0);
    }

    double truePeakDb() const { double m = 0.0; for (int c = 0; c < nch; ++c) m = std::max (m, tp[(size_t) c].maxPeak()); return toDb (m); }
    double samplePeakDb() const { return toDb (samplePeak); }
    const std::vector<double>& shortTermHistory() const { return stHistory; }      // un valor cada 100 ms (desde los 3 s)
    double seconds() const { return (double) blocks.size() * 0.1; }

private:
    double fs = 48000.0;
    int nch = 2, blockLen = 4800, inBlock = 0;
    std::array<BiquadD, 2> shelf {}, hp {};
    std::array<TruePeak, 2> tp {};
    double acc = 0.0, samplePeak = 0.0;
    std::vector<double> blocks, stHistory;

    double windowLufs (int n) const
    {
        const int k = std::min (n, (int) blocks.size());
        double s = 0.0;
        for (int j = 0; j < k; ++j) s += blocks[blocks.size() - 1 - (size_t) j];
        return powToLufs (s / (double) k);
    }
};

} // namespace dh::lq
