#pragma once
// Análisis de una mezcla para decidir el master (docs/research.md §6.4). Se corre una vez: sobre el archivo
// entero (render) o sobre lo que se escuchó con LEARN (plugin). Todo en double, determinista.
//
// Mide sonoridad (integrada, serie corta, LRA), pico verdadero y muestra, PLR, la máscara de partes fuertes
// (bloques de 3 s con sonoridad corta ≥ integrada − 3 LU), y sólo sobre esa máscara: el espectro promedio de
// mid y side (Welch, Hann, 50 %, potencia promediada) en 31 bandas de tercio de octava (ISO 266, 20 Hz–20 kHz)
// y en sextos para detectar resonancias, la correlación global y en graves, el factor de cresta, la densidad de
// transitorios (flujo espectral), la aspereza (2–5 kHz) y la sibilancia (5–10 kHz), el corte de banda en agudos y
// el recorte en la entrada.

#include "Filters.h"

namespace dh::lq {

// FFT radix-2 iterativa, en el lugar.
inline void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i)
    {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if (i < j) std::swap (a[i], a[j]);
    }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * kPiD / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0, 0.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

constexpr int kBands = 31;
inline const std::array<double, kBands>& bandCentres()
{
    static const std::array<double, kBands> c { 20, 25, 31.5, 40, 50, 63, 80, 100, 125, 160, 200, 250, 315, 400, 500, 630, 800,
                                                1000, 1250, 1600, 2000, 2500, 3150, 4000, 5000, 6300, 8000, 10000, 12500, 16000, 20000 };
    return c;
}
inline int bandIndex (double hz) { int best = 0; for (int i = 0; i < kBands; ++i) if (std::fabs (std::log2 (bandCentres()[(size_t) i] / hz)) < std::fabs (std::log2 (bandCentres()[(size_t) best] / hz))) best = i; return best; }

struct Report
{
    double sampleRate = 48000.0, seconds = 0.0;
    // Sonoridad y picos
    double lufsI = -70, lra = 0, truePeakDb = -100, samplePeakDb = -100, plr = 0, loudestShortTerm = -70;
    double minPsr = 0;                              // PSR mínimo sobre la máscara (pico verdadero − corta)
    std::vector<double> shortTermHistory;           // sonoridad corta cada 100 ms (desde los 3 s), para la máscara
    // Espectro (dB de potencia por banda, sonoridad normalizada a −14 LUFS, sobre la máscara)
    std::array<double, kBands> mid {}, side {}, sideMinusMid {};
    std::vector<double> fineHz, fineMidDb;           // sextos de octava, para resonancias
    // Estéreo
    double correlation = 1, lowCorrelation = 1;      // global y debajo de 150 Hz
    // Dinámica
    double crestDb = 0, onsetsPerSec = 0;
    // Problemas
    double harshHz = 0, harshExcessDb = 0;           // resonancia más marcada entre 2 y 5 kHz
    double sibilanceEventsPerSec = 0;
    double bandLimitHz = 0;                          // 0 = sin corte; si no, frecuencia donde el espectro se desploma
    int clippedRuns = 0;                             // tramos de ≥ 3 muestras en |x| ≥ 0.999
    bool alreadyMastered = false;
    bool silent = false;
};

class Analyzer
{
public:
    // Analiza un estéreo (R puede ser nullptr: mono). La máscara de partes fuertes sale de la sonoridad corta.
    static Report run (const float* L, const float* R, size_t n, double fs)
    {
        Report rep; rep.sampleRate = fs; rep.seconds = (double) n / fs;
        if (R == nullptr) R = L;
        // 1) Sonoridad, picos y serie corta (cada 100 ms).
        LoudnessMeter m; m.prepare (fs);
        for (size_t p = 0; p < n; p += 4096) m.process (L + p, R + p, (int) std::min ((size_t) 4096, n - p));
        rep.lufsI = m.integrated(); rep.lra = m.lra(); rep.truePeakDb = m.truePeakDb(); rep.samplePeakDb = m.samplePeakDb();
        if (rep.lufsI < -69.0) { rep.silent = true; return rep; }
        rep.plr = rep.truePeakDb - rep.lufsI;
        rep.shortTermHistory = m.shortTermHistory();
        const auto& st = rep.shortTermHistory;
        double maxSt = -70; for (double v : st) maxSt = std::max (maxSt, v);
        rep.loudestShortTerm = maxSt;
        // Máscara: por cada muestra, ¿pertenece a un tramo fuerte? (serie corta con salto de 100 ms desde los 3 s)
        const double gate = std::max (rep.lufsI - 3.0, -45.0);
        auto loudAt = [&] (size_t i) -> bool {
            if (st.empty()) return true;
            // El valor j de la serie corta cubre [j·0.1, j·0.1 + 3] s: la muestra i usa la ventana que la centra.
            const double t = (double) i / fs;
            long j = (long) std::lround ((t - 1.5) / 0.1);
            j = std::clamp (j, 0L, (long) st.size() - 1);
            return st[(size_t) j] >= gate;
        };

        // 2) Espectro mid / side por Welch sobre la máscara. Tamaño de ventana: ~0.17 s (8192 a 48 kHz).
        size_t N = 1; while ((double) N < fs * 0.17) N <<= 1;
        const size_t hop = N / 2;
        std::vector<double> win (N);
        for (size_t i = 0; i < N; ++i) win[i] = 0.5 - 0.5 * std::cos (2.0 * kPiD * (double) i / (double) N);
        std::vector<double> pm (N / 2 + 1, 0.0), ps (N / 2 + 1, 0.0), prevMag (N / 2 + 1, 0.0);
        std::vector<std::complex<double>> bm (N), bs (N);
        int frames = 0; std::vector<double> flux;
        double corrLR = 0, corrLL = 0, corrRR = 0, lowLR = 0, lowLL = 0, lowRR = 0;
        Biquad lpL, lpR; lpL.set (lowPass (150.0, 0.7071, fs)); lpR.set (lowPass (150.0, 0.7071, fs));
        // Correlación (global y en graves) muestra a muestra sobre la máscara.
        for (size_t i = 0; i < n; ++i)
        {
            const double l = L[i], r = R[i];
            const double ll = lpL.process (l), rr = lpR.process (r);
            if (! loudAt (i)) continue;
            corrLR += l * r; corrLL += l * l; corrRR += r * r;
            lowLR += ll * rr; lowLL += ll * ll; lowRR += rr * rr;
        }
        rep.correlation = corrLL > 0 && corrRR > 0 ? corrLR / std::sqrt (corrLL * corrRR) : 1.0;
        rep.lowCorrelation = lowLL > 0 && lowRR > 0 ? lowLR / std::sqrt (lowLL * lowRR) : 1.0;
        for (size_t p = 0; p + N <= n; p += hop)
        {
            if (! loudAt (p + N / 2)) continue;
            for (size_t i = 0; i < N; ++i)
            {
                const double l = L[p + i], r = R[p + i];
                bm[i] = { 0.5 * (l + r) * win[i], 0.0 }; bs[i] = { 0.5 * (l - r) * win[i], 0.0 };
            }
            fft (bm); fft (bs);
            double fx = 0.0;
            for (size_t k = 0; k <= N / 2; ++k)
            {
                const double a = std::norm (bm[k]), b = std::norm (bs[k]);
                pm[k] += a; ps[k] += b;
                const double mag = std::sqrt (a);
                if (frames > 0) fx += std::max (0.0, mag - prevMag[k]);
                prevMag[k] = mag;
            }
            if (frames > 0) flux.push_back (fx);
            ++frames;
        }
        if (frames == 0) { rep.silent = true; return rep; }
        const double df = fs / (double) N;
        // Bandas de tercio: potencia sumada sobre los bins de [fc·2^(−1/6), fc·2^(1/6)] (con al menos un bin).
        auto bandPow = [&] (const std::vector<double>& P, double fc, double width) {
            const double lo = fc * std::pow (2.0, -width / 2.0), hi = fc * std::pow (2.0, width / 2.0);
            size_t k0 = (size_t) std::max (1.0, std::floor (lo / df)), k1 = (size_t) std::min ((double) N / 2, std::ceil (hi / df));
            if (k1 < k0) k1 = k0;
            double s = 0.0; for (size_t k = k0; k <= k1; ++k) s += P[k];
            return s / (double) frames;
        };
        // Normalización: el espectro se lleva a −14 LUFS (así se compara con las curvas objetivo).
        const double norm = 10.0 * std::log10 (lufsToPow (-14.0) / lufsToPow (rep.lufsI));
        for (int b = 0; b < kBands; ++b)
        {
            const double fc = bandCentres()[(size_t) b];
            const double pmb = bandPow (pm, fc, 1.0 / 3.0), psb = bandPow (ps, fc, 1.0 / 3.0);
            rep.mid[(size_t) b] = 10.0 * std::log10 (pmb + 1e-30) + norm;
            rep.side[(size_t) b] = 10.0 * std::log10 (psb + 1e-30) + norm;
            rep.sideMinusMid[(size_t) b] = rep.side[(size_t) b] - rep.mid[(size_t) b];
        }
        for (double f = 1000.0 * std::pow (2.0, -5.0); f <= 20000.0; f *= std::pow (2.0, 1.0 / 6.0))
        {
            rep.fineHz.push_back (f);
            rep.fineMidDb.push_back (10.0 * std::log10 (bandPow (pm, f, 1.0 / 6.0) + 1e-30) + norm);
        }
        // Aspereza: el sexto de octava que más sobresale de su vecindad de tercio (2–5 kHz).
        double best = 0.0, bestHz = 0.0;
        for (size_t i = 1; i + 1 < rep.fineHz.size(); ++i)
        {
            const double f = rep.fineHz[i];
            if (f < 2000.0 || f > 5000.0) continue;
            const double trend = 0.5 * (rep.fineMidDb[i - 1] + rep.fineMidDb[i + 1]);
            const double ex = rep.fineMidDb[i] - trend;
            if (ex > best) { best = ex; bestHz = f; }
        }
        rep.harshHz = bestHz; rep.harshExcessDb = best;
        // Corte de banda: la primera banda desde 10 kHz que cae > 20 dB respecto de la de 8 kHz.
        const double ref8 = rep.mid[(size_t) bandIndex (8000.0)];
        for (int b = bandIndex (10000.0); b < kBands; ++b)
            if (rep.mid[(size_t) b] < ref8 - 20.0) { rep.bandLimitHz = bandCentres()[(size_t) b]; break; }
        // Transitorios: picos del flujo espectral por encima de su mediana × 1.5, por segundo de parte fuerte.
        if (flux.size() > 4)
        {
            std::vector<double> sorted = flux; std::sort (sorted.begin(), sorted.end());
            const double med = sorted[sorted.size() / 2];
            int onsets = 0;
            for (size_t i = 1; i + 1 < flux.size(); ++i) if (flux[i] > 1.5 * med && flux[i] >= flux[i - 1] && flux[i] > flux[i + 1]) ++onsets;
            rep.onsetsPerSec = (double) onsets / ((double) flux.size() * (double) hop / fs);
        }
        // 3) Cresta (pico / RMS sobre la máscara), PSR mínimo, sibilancia y recortes.
        double sq = 0; size_t nn = 0; double pk = 0;
        Biquad sibL, harshL; sibL.set (bandPass (7000.0, 1.0, fs)); harshL.set (bandPass (3200.0, 1.0, fs));
        double eS = 0, eH = 0; int win20 = 0, events = 0; const int w20 = (int) (0.02 * fs);
        int run = 0;
        for (size_t i = 0; i < n; ++i)
        {
            const double mono = 0.5 * ((double) L[i] + (double) R[i]);
            const double s = sibL.process (mono), h = harshL.process (mono);
            const double a = std::max (std::fabs ((double) L[i]), std::fabs ((double) R[i]));
            if (a >= 0.999) { if (++run == 3) ++rep.clippedRuns; } else run = 0;
            if (! loudAt (i)) continue;
            sq += mono * mono; ++nn; pk = std::max (pk, a);
            eS += s * s; eH += h * h;
            if (++win20 == w20) { if (eS > 4.0 * eH && eS > 1e-6 * w20) ++events; eS = eH = 0; win20 = 0; }     // 5-10 kHz > 2-5 kHz + 6 dB
        }
        rep.crestDb = nn ? toDb (pk) - 10.0 * std::log10 (sq / (double) nn + 1e-30) : 0.0;
        double loudSecs = (double) nn / fs;
        rep.sibilanceEventsPerSec = loudSecs > 0 ? (double) events / loudSecs : 0.0;
        double minPsr = 100.0;
        for (double v : st) if (v >= gate) minPsr = std::min (minPsr, rep.truePeakDb - v);
        rep.minPsr = minPsr < 100.0 ? minPsr : rep.plr;
        // Ya masterizada: PSR mínimo < 8 dB y sonoridad de master (≥ −12 LUFS).
        rep.alreadyMastered = rep.minPsr < 8.0 && rep.lufsI > -12.0;
        return rep;
    }
};

} // namespace dh::lq
