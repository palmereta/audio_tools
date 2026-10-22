#pragma once
// El gabinete de Thrum (docs/research.md §3, §8): un modelo propio de parlante y micrófono que se vuelve una respuesta
// al impulso (IR) cada vez que se mueve un control, y la carga de IR del usuario. La IR se convoluciona en forma
// directa (Fir en Base.h).
//
// Diseño de la IR (en frecuencia, 8192 puntos):
//   parlante  resonancia de la caja (pasa altos de segundo orden; la caja abierta además pierde graves por la
//             cancelación entre frente y fondo), picos de la ruptura del cono (1-4 kHz), caída fuerte arriba de 5 kHz
//             con las muescas de la ruptura;
//   caja      resonancia de la caja cerrada (~100 Hz) y ondulación chica de las ondas estacionarias;
//   micrófono tipo (dinámico: pico de presencia y caída arriba de 12 kHz; condensador: plano y extendido; cinta:
//             oscuro y con mucho efecto de proximidad), posición (del centro del cono, brillante, al borde, oscuro),
//             distancia (proximidad: refuerzo de graves de cerca; de lejos, el reflejo del piso hace un peine);
// a fase mínima por el cepstro (la IR arranca en la muestra 0), más el retardo relativo entre los dos micrófonos (que
// suma el peine real de dos micrófonos a distinta distancia) y el reflejo del piso. Se normaliza la energía entre 100 Hz
// y 5 kHz para que cambiar de micrófono no cambie tanto el volumen.

#include "Base.h"

namespace dh::gt {

enum SpeakerType { kSpkVintage, kSpkBritish, kSpkAmerican, kSpkModern, kSpkSmall, kNumSpeakers };
inline const char* speakerName (int i) { static const char* n[] = { "Vintage 12 (alnico)", "British 12 (ceramic, mid bump)", "American 12 (clean, wide)", "Modern 12 (aggressive)", "Small 10" }; return n[std::clamp (i, 0, 4)]; }
enum CabType { kCab1x12Open, kCab2x12Open, kCab4x12Closed, kCab1x10Combo, kNumCabs };
inline const char* cabName (int i) { static const char* n[] = { "1x12 open back", "2x12 open back", "4x12 closed back", "1x10 small combo" }; return n[std::clamp (i, 0, 3)]; }
enum MicType { kMicDynamic, kMicCondenser, kMicRibbon, kNumMics };
inline const char* micName (int i) { static const char* n[] = { "Dynamic", "Condenser", "Ribbon" }; return n[std::clamp (i, 0, 2)]; }

struct MicParams { int type = kMicDynamic; float position = 0.3f, distanceCm = 3.0f; };
struct CabParams
{
    int speaker = kSpkBritish, cab = kCab4x12Closed;
    MicParams a, b { kMicRibbon, 0.5f, 25.0f };
    float mix = 0.0f;               // 0 sólo A … 1 sólo B
    float room = 0.15f;             // reflejo del piso
    bool operator== (const CabParams& o) const
    {
        auto eq = [] (const MicParams& x, const MicParams& y) { return x.type == y.type && std::fabs (x.position - y.position) < 1e-4f && std::fabs (x.distanceCm - y.distanceCm) < 1e-3f; };
        return speaker == o.speaker && cab == o.cab && eq (a, o.a) && eq (b, o.b) && std::fabs (mix - o.mix) < 1e-4f && std::fabs (room - o.room) < 1e-4f;
    }
};

// FFT compleja de base 2 (en el lugar). Sólo para diseñar IR (fuera del lazo de audio por muestra).
inline void fftInPlace (std::vector<std::complex<double>>& a, bool inverse)
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
        const double ang = (inverse ? 2.0 : -2.0) * kPiD / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1.0);
            for (size_t j = 0; j < len / 2; ++j)
            {
                const auto u = a[i + j], v = a[i + j + len / 2] * w;
                a[i + j] = u + v; a[i + j + len / 2] = u - v;
                w *= wl;
            }
        }
    }
    if (inverse) for (auto& x : a) x /= (double) n;
}

// Magnitud (lineal) de un micrófono ubicado según `m` frente al parlante y la caja dados, en f (Hz).
inline double cabMagnitude (const CabParams& c, const MicParams& m, double f)
{
    auto hp2 = [] (double x, double fc, double q) { const double r = x / fc; return r * r / std::sqrt ((1 - r * r) * (1 - r * r) + (r / q) * (r / q)); };
    auto lpN = [] (double x, double fc, int order) { return 1.0 / std::sqrt (1.0 + std::pow (x / fc, 2.0 * order)); };
    auto peak = [] (double x, double fc, double q, double db) {
        const double A = std::pow (10.0, db / 40.0), r = x / fc;
        const std::complex<double> s (0.0, r);
        return std::abs ((s * s + s * (A / q) + 1.0) / (s * s + s / (A * q) + 1.0));
    };
    double g = 1.0;
    // Parlante.
    struct Spk { double fres, q, brk1, brk1Db, brk2, brk2Db, lpHz, notch, notchDb; int lpOrd; };
    static const Spk spk[kNumSpeakers] = {
        { 95, 1.0, 2400, 4.5, 3800, 2.0, 5200, 6800, -9.0, 4 },     // vintage: suave, campana a 2,4 kHz
        { 85, 1.1, 1600, 4.0, 2900, 5.0, 4800, 6200, -10.0, 4 },    // británico: medios adelante
        { 80, 0.9, 2900, 2.0, 4600, 2.5, 6200, 8200, -7.0, 3 },     // americano: ancho y limpio
        { 90, 1.2, 2300, 6.0, 4200, 5.5, 5400, 7000, -11.0, 5 },    // moderno: agresivo
        { 125, 0.9, 2100, 5.0, 3400, 3.0, 4600, 6000, -9.0, 4 },    // de 10": menos graves, rompe antes
    };
    const Spk& s = spk[std::clamp (c.speaker, 0, kNumSpeakers - 1)];
    double fres = s.fres, q = s.q;
    if (c.cab == kCab4x12Closed) { fres *= 0.95; q *= 1.35; }
    if (c.cab == kCab1x10Combo) { fres *= 1.15; }
    g *= hp2 (f, fres, q);
    if (c.cab == kCab1x12Open || c.cab == kCab2x12Open || c.cab == kCab1x10Combo)
        g *= (f / 140.0) / std::sqrt (1.0 + (f / 140.0) * (f / 140.0)) * 0.6 + 0.4;   // cancelación frente-fondo (parcial)
    g *= peak (f, s.brk1, 2.5, s.brk1Db) * peak (f, s.brk2, 3.0, s.brk2Db) * peak (f, s.notch, 4.0, s.notchDb);
    if (c.cab == kCab4x12Closed) g *= peak (f, 420.0, 6.0, 1.5) * peak (f, 760.0, 6.0, -1.5);
    if (c.cab == kCab2x12Open) g *= peak (f, 650.0, 5.0, 1.2);
    // Posición: del centro (cúpula, brillante) al borde (oscuro): el corte baja de ~1,3× a ~0,45× y el pico de la cúpula
    // se apaga.
    const double pos = clampf (m.position, 0.0f, 1.0f);
    const double lpHz = s.lpHz * (1.35 - 0.9 * pos);
    g *= lpN (f, lpHz, s.lpOrd);
    g *= peak (f, 4200.0, 2.0, 3.5 * (1.0 - pos));
    // Micrófono.
    const double dist = std::max (0.5, (double) m.distanceCm);
    switch (m.type)
    {
        case kMicDynamic: g *= peak (f, 5200.0, 1.4, 4.5) * lpN (f, 13000.0, 2) * hp2 (f, 45.0, 0.7); break;
        case kMicCondenser: g *= peak (f, 10000.0, 1.0, 2.0) * hp2 (f, 25.0, 0.7); break;
        default: g *= lpN (f, 7500.0, 1) * peak (f, 2500.0, 0.8, -1.5) * hp2 (f, 25.0, 0.7); break;
    }
    // Proximidad (cardioide y figura 8; el condensador cardioide la tiene más suave): +dB en graves de cerca.
    const double proxMax = m.type == kMicRibbon ? 14.0 : m.type == kMicDynamic ? 10.0 : 6.0;
    const double prox = proxMax * std::min (1.0, 4.0 / (dist + 2.0));
    g *= std::sqrt (1.0 + std::pow (10.0, prox / 10.0) * 1.0 / (1.0 + std::pow (f / 180.0, 2.0)));
    return g;
}

// La IR de un ajuste completo, `len` muestras a `fs`.
inline std::vector<float> designCabIr (const CabParams& c, double fs, int len)
{
    const int N = 8192;
    std::vector<std::complex<double>> spec ((size_t) N);
    auto micIr = [&] (const MicParams& m, double delaySamples, std::vector<std::complex<double>>& out) {
        // Log-magnitud → cepstro → fase mínima.
        std::vector<std::complex<double>> a ((size_t) N);
        for (int k = 0; k <= N / 2; ++k)
        {
            const double f = std::max (1.0, (double) k * fs / N);
            const double mag = std::max (1e-6, cabMagnitude (c, m, std::min (f, 0.49 * fs)) * (f > 0.45 * fs ? 0.1 : 1.0));
            a[(size_t) k] = std::log (mag);
            if (k > 0 && k < N / 2) a[(size_t) (N - k)] = a[(size_t) k];
        }
        fftInPlace (a, true);
        for (int n = 1; n < N / 2; ++n) a[(size_t) n] *= 2.0;
        for (int n = N / 2 + 1; n < N; ++n) a[(size_t) n] = 0.0;
        fftInPlace (a, false);
        // Reflejo del piso: un camino más largo, más oscuro, más débil cuanto más cerca está el micrófono.
        const double cm = std::max (0.5, (double) m.distanceCm);
        const double mh = 45.0;                                         // altura del centro del parlante (cm)
        const double extra = (std::sqrt (cm * cm + 4.0 * mh * mh) - cm) / 34300.0 * fs;
        const double rg = c.room * std::min (1.0, cm / 30.0) * 0.6;
        out.assign ((size_t) N, 0.0);
        for (int k = 0; k <= N / 2; ++k)
        {
            const double w = 2.0 * kPiD * (double) k / N;
            const auto h = std::exp (a[(size_t) k]);
            const double f = (double) k * fs / N;
            const double refl = rg / std::sqrt (1.0 + (f / 3000.0) * (f / 3000.0));
            out[(size_t) k] = h * (std::polar (1.0, -w * delaySamples) + refl * std::polar (1.0, -w * (delaySamples + extra)));
        }
    };
    // Retardo relativo entre los dos micrófonos (la distancia más corta se toma como cero).
    const double da = c.a.distanceCm, db = c.b.distanceCm, d0 = std::min (da, db);
    std::vector<std::complex<double>> ha, hb;
    const double mixB = clampf (c.mix, 0.0f, 1.0f);
    if (mixB < 0.999) micIr (c.a, (da - d0) / 34300.0 * fs, ha);
    if (mixB > 0.001) micIr (c.b, (db - d0) / 34300.0 * fs, hb);
    for (int k = 0; k <= N / 2; ++k)
    {
        std::complex<double> v = 0.0;
        if (! ha.empty()) v += (1.0 - mixB) * ha[(size_t) k];
        if (! hb.empty()) v += mixB * hb[(size_t) k];
        spec[(size_t) k] = v;
        if (k > 0 && k < N / 2) spec[(size_t) (N - k)] = std::conj (v);
    }
    // Normalizar: energía media entre 100 Hz y 5 kHz igual a 1.
    double e = 0; int cnt = 0;
    for (int k = 0; k <= N / 2; ++k) { const double f = (double) k * fs / N; if (f >= 100.0 && f <= 5000.0) { e += std::norm (spec[(size_t) k]); ++cnt; } }
    const double norm = 1.0 / std::sqrt (std::max (1e-12, e / std::max (1, cnt)));
    fftInPlace (spec, true);
    std::vector<float> ir ((size_t) len, 0.0f);
    const int fade = len / 4;
    for (int n = 0; n < len && n < N; ++n)
    {
        double w = 1.0;
        if (n > len - fade) w = 0.5 + 0.5 * std::cos (kPiD * (double) (n - (len - fade)) / fade);
        ir[(size_t) n] = (float) (spec[(size_t) n].real() * norm * w);
    }
    return ir;
}

// Las IR de fábrica: el mismo modelo con ajustes elegidos (las genera thrumrender --export-irs; el plugin las calcula).
struct FactoryIr { const char* name; CabParams p; };
inline const std::array<FactoryIr, 8>& factoryIrs()
{
    static const std::array<FactoryIr, 8> f {{
        { "4x12 British, dynamic on the cap edge", { kSpkBritish, kCab4x12Closed, { kMicDynamic, 0.3f, 2.0f }, { kMicRibbon, 0.5f, 25.0f }, 0.0f, 0.1f } },
        { "4x12 Modern, dynamic + ribbon", { kSpkModern, kCab4x12Closed, { kMicDynamic, 0.25f, 2.0f }, { kMicRibbon, 0.4f, 15.0f }, 0.4f, 0.1f } },
        { "2x12 American, condenser at 30 cm", { kSpkAmerican, kCab2x12Open, { kMicCondenser, 0.4f, 30.0f }, { kMicDynamic, 0.3f, 3.0f }, 0.0f, 0.3f } },
        { "1x12 Vintage, ribbon", { kSpkVintage, kCab1x12Open, { kMicRibbon, 0.3f, 10.0f }, { kMicDynamic, 0.3f, 3.0f }, 0.0f, 0.2f } },
        { "1x12 Vintage, dynamic center", { kSpkVintage, kCab1x12Open, { kMicDynamic, 0.05f, 2.0f }, { kMicRibbon, 0.5f, 20.0f }, 0.0f, 0.1f } },
        { "1x10 Small combo, dynamic", { kSpkSmall, kCab1x10Combo, { kMicDynamic, 0.35f, 3.0f }, { kMicCondenser, 0.5f, 40.0f }, 0.0f, 0.2f } },
        { "1x10 Small combo, room", { kSpkSmall, kCab1x10Combo, { kMicCondenser, 0.5f, 60.0f }, { kMicDynamic, 0.3f, 3.0f }, 0.0f, 0.6f } },
        { "4x12 British, edge and far", { kSpkBritish, kCab4x12Closed, { kMicDynamic, 0.8f, 4.0f }, { kMicCondenser, 0.5f, 60.0f }, 0.35f, 0.3f } },
    }};
    return f;
}

// Normalizar una IR: energía media del espectro entre 100 Hz y 5 kHz igual a 1 (la de las IR del modelo).
inline void normalizeIr (std::vector<float>& y, double fs)
{
    const int N = 8192;
    std::vector<std::complex<double>> a ((size_t) N);
    for (int i = 0; i < N && i < (int) y.size(); ++i) a[(size_t) i] = y[(size_t) i];
    fftInPlace (a, false);
    double e = 0; int cnt = 0;
    for (int k = 0; k <= N / 2; ++k) { const double f = (double) k * fs / N; if (f >= 100.0 && f <= 5000.0) { e += std::norm (a[(size_t) k]); ++cnt; } }
    const float g = (float) (1.0 / std::sqrt (std::max (1e-12, e / std::max (1, cnt))));
    for (auto& v : y) v *= g;
}

// IR del usuario: se lleva a la frecuencia del motor por interpolación de sinc con ventana (fuera del audio) y se recorta.
inline std::vector<float> resampleIr (const float* x, int n, double fromHz, double toHz, int maxLen)
{
    if (n <= 0) return { 1.0f };
    const double ratio = toHz / fromHz;
    const int outLen = std::clamp ((int) std::ceil (n * ratio), 1, maxLen);
    std::vector<float> y ((size_t) outLen, 0.0f);
    const double cutoff = std::min (1.0, ratio);
    const int taps = 16;
    for (int i = 0; i < outLen; ++i)
    {
        const double t = i / ratio;
        const int c = (int) std::floor (t);
        double acc = 0;
        for (int k = c - taps + 1; k <= c + taps; ++k)
        {
            if (k < 0 || k >= n) continue;
            const double d = t - k;
            const double sx = std::fabs (d * cutoff) < 1e-9 ? 1.0 : std::sin (kPiD * d * cutoff) / (kPiD * d * cutoff);
            const double w = 0.5 + 0.5 * std::cos (kPiD * d / (taps + 1));
            acc += x[k] * sx * w * cutoff;
        }
        y[(size_t) i] = (float) acc;
    }
    // Fundido al final y el mismo nivel que las IR del modelo (energía media de 100 Hz a 5 kHz igual a 1).
    const int fade = std::min (outLen / 8, (int) (0.005 * toHz));
    for (int i = 0; i < fade; ++i) y[(size_t) (outLen - 1 - i)] *= (float) i / (float) std::max (1, fade);
    normalizeIr (y, toHz);
    return y;
}

} // namespace dh::gt
