#pragma once
// EQ tonal automática (docs/research.md §3.4): de la diferencia entre la curva objetivo y el espectro medido sale
// una curva deseada en las 31 bandas de tercio, con zona muerta, pesos de confianza, tope y pendiente máxima; esa
// curva se realiza con una topología fija (estante grave, seis campanas a una octava, estante agudo) cuyas
// ganancias se ajustan por mínimos cuadrados sobre la respuesta en dB (Gauss-Newton, 4 iteraciones). Fase mínima,
// sin latencia: lo mismo en el plugin y en el render.

#include "Targets.h"

namespace dh::lq {

constexpr int kEqBands = 8;
struct EqBand { int type = 0; double hz = 1000, q = 1.0, gainDb = 0.0; };        // 0 campana, 1 estante grave, 2 estante agudo

inline Coefs eqCoefs (const EqBand& b, double fs)
{
    if (b.type == 1) return lowShelf (b.hz, b.gainDb, fs);
    if (b.type == 2) return highShelf (b.hz, b.gainDb, fs);
    return peaking (b.hz, b.q, b.gainDb, fs);
}

inline std::array<EqBand, kEqBands> eqTopology()
{
    std::array<EqBand, kEqBands> t {};
    t[0] = { 1, 90.0, 0.7071, 0.0 };
    const double bells[6] = { 160, 315, 630, 1250, 2500, 5000 };
    for (int i = 0; i < 6; ++i) t[(size_t) i + 1] = { 0, bells[i], 1.0, 0.0 };
    t[7] = { 2, 9000.0, 0.7071, 0.0 };
    return t;
}

using BandCurve = std::array<double, kBands>;

inline BandCurve eqResponse (const std::array<EqBand, kEqBands>& eq, double fs)
{
    BandCurve r {};
    for (int b = 0; b < kBands; ++b)
    {
        const double f = std::min (bandCentres()[(size_t) b], 0.45 * fs);
        for (auto& e : eq) if (e.gainDb != 0.0) r[(size_t) b] += responseDb (eqCoefs (e, fs), f, fs);
    }
    return r;
}

// Mínimos cuadrados ponderados con regularización: ganancias de la topología que mejor siguen `want`.
inline std::array<EqBand, kEqBands> fitEq (const BandCurve& want, const BandCurve& w, double fs, double maxGainDb)
{
    auto eq = eqTopology();
    for (int it = 0; it < 4; ++it)
    {
        const BandCurve cur = eqResponse (eq, fs);
        // Jacobiano por diferencias: respuesta de cada banda con +0.5 dB respecto de su valor actual.
        double J[kBands][kEqBands];
        for (int j = 0; j < kEqBands; ++j)
        {
            auto e2 = eq; e2[(size_t) j].gainDb += 0.5;
            for (int b = 0; b < kBands; ++b)
            {
                const double f = std::min (bandCentres()[(size_t) b], 0.45 * fs);
                J[b][j] = (responseDb (eqCoefs (e2[(size_t) j], fs), f, fs) - responseDb (eqCoefs (eq[(size_t) j], fs), f, fs)) / 0.5;
            }
        }
        // (JᵀWJ + λI) Δ = JᵀW (want − cur) − λ g
        double A[kEqBands][kEqBands + 1] {};
        const double lambda = 0.02;
        for (int i = 0; i < kEqBands; ++i)
        {
            for (int j = 0; j < kEqBands; ++j)
            {
                double s = 0.0; for (int b = 0; b < kBands; ++b) s += J[b][i] * w[(size_t) b] * J[b][j];
                A[i][j] = s + (i == j ? lambda : 0.0);
            }
            double s = 0.0; for (int b = 0; b < kBands; ++b) s += J[b][i] * w[(size_t) b] * (want[(size_t) b] - cur[(size_t) b]);
            A[i][kEqBands] = s - lambda * eq[(size_t) i].gainDb;
        }
        // Gauss con pivoteo parcial.
        for (int c = 0; c < kEqBands; ++c)
        {
            int p = c; for (int r = c + 1; r < kEqBands; ++r) if (std::fabs (A[r][c]) > std::fabs (A[p][c])) p = r;
            for (int k = 0; k <= kEqBands; ++k) std::swap (A[c][k], A[p][k]);
            if (std::fabs (A[c][c]) < 1e-12) continue;
            for (int r = 0; r < kEqBands; ++r)
            {
                if (r == c) continue;
                const double f = A[r][c] / A[c][c];
                for (int k = c; k <= kEqBands; ++k) A[r][k] -= f * A[c][k];
            }
        }
        for (int i = 0; i < kEqBands; ++i)
        {
            const double d = std::fabs (A[i][i]) > 1e-12 ? A[i][kEqBands] / A[i][i] : 0.0;
            eq[(size_t) i].gainDb = std::clamp (eq[(size_t) i].gainDb + d, -maxGainDb, maxGainDb);
        }
    }
    for (auto& e : eq) e.gainDb = std::round (e.gainDb * 100.0) / 100.0;       // centésimas: el informe y el plugin dicen lo mismo
    return eq;
}

// La curva deseada a partir del análisis (§3.4, pasos 1-6). Devuelve dB por banda; `conf` recibe los pesos.
struct MatchInputs { double amount = 0.6, clampDb = 3.0; };
inline BandCurve matchCurve (const Report& rep, const TargetShape& shape, const MatchInputs& mi, BandCurve* confOut = nullptr)
{
    const auto& fc = bandCentres();
    BandCurve D {}, conf {};
    for (int b = 0; b < kBands; ++b) D[(size_t) b] = targetAt (shape, fc[(size_t) b]) - rep.mid[(size_t) b];
    // Alineación: la mediana de la diferencia entre 100 Hz y 8 kHz (la forma, no el nivel).
    {
        std::vector<double> v;
        for (int b = 0; b < kBands; ++b) if (fc[(size_t) b] >= 100.0 && fc[(size_t) b] <= 8000.0) v.push_back (D[(size_t) b]);
        std::sort (v.begin(), v.end());
        const double med = v.size() % 2 ? v[v.size() / 2] : 0.5 * (v[v.size() / 2 - 1] + v[v.size() / 2]);
        for (auto& d : D) d -= med;
    }
    // Suavizado: una octava debajo de 100 Hz, [¼ ½ ¼] en el resto.
    BandCurve S {};
    for (int b = 0; b < kBands; ++b)
    {
        const int lo = std::max (0, b - 1), hi = std::min (kBands - 1, b + 1);
        if (fc[(size_t) b] < 100.0) S[(size_t) b] = (D[(size_t) lo] + D[(size_t) b] + D[(size_t) hi]) / 3.0;
        else S[(size_t) b] = 0.25 * D[(size_t) lo] + 0.5 * D[(size_t) b] + 0.25 * D[(size_t) hi];
    }
    // Zona muerta: dentro de la tolerancia se corrige el 30 %; afuera, el 30 % de la tolerancia más todo el exceso.
    for (int b = 0; b < kBands; ++b)
    {
        const double tol = toleranceAt (fc[(size_t) b]), a = std::fabs (S[(size_t) b]);
        const double y = a <= tol ? 0.3 * a : 0.3 * tol + (a - tol);
        S[(size_t) b] = S[(size_t) b] < 0 ? -y : y;
    }
    // Confianza: nada debajo de 30 Hz ni arriba de 16 kHz (ni arriba de un corte de banda), la mitad en los bordes.
    for (int b = 0; b < kBands; ++b)
    {
        const double f = fc[(size_t) b];
        double c = 1.0;
        if (f < 30.0 || f > 16000.0) c = 0.0;
        else if (f < 50.0 || f > 12000.0) c = 0.5;
        if (rep.bandLimitHz > 0.0 && f >= rep.bandLimitHz / std::pow (2.0, 1.0 / 3.0)) c = 0.0;
        conf[(size_t) b] = c;
        S[(size_t) b] *= c * mi.amount;
    }
    // Topes: general, ±2 dB arriba de 12 kHz, ±3 dB debajo de 50 Hz; y no más de 1.5 dB entre bandas vecinas.
    for (int b = 0; b < kBands; ++b)
    {
        const double f = fc[(size_t) b];
        double cl = mi.clampDb;
        if (f > 12000.0) cl = std::min (cl, 2.0);
        if (f < 50.0) cl = std::min (cl, 3.0);
        S[(size_t) b] = std::clamp (S[(size_t) b], -cl, cl);
    }
    for (int pass = 0; pass < 4; ++pass)
    {
        for (int b = 1; b < kBands; ++b) S[(size_t) b] = std::clamp (S[(size_t) b], S[(size_t) b - 1] - 1.5, S[(size_t) b - 1] + 1.5);
        for (int b = kBands - 2; b >= 0; --b) S[(size_t) b] = std::clamp (S[(size_t) b], S[(size_t) b + 1] - 1.5, S[(size_t) b + 1] + 1.5);
    }
    // Tope de energía de los realces: la suma ponderada por la contribución de cada banda a la sonoridad ≤ +2 dB.
    {
        double pTot = 0.0, pBoost = 0.0;
        for (int b = 0; b < kBands; ++b)
        {
            const double p = std::pow (10.0, rep.mid[(size_t) b] / 10.0);
            pTot += p; pBoost += p * std::pow (10.0, std::max (0.0, S[(size_t) b]) / 10.0);
        }
        const double boostDb = pTot > 0 ? 10.0 * std::log10 (pBoost / pTot) : 0.0;
        if (boostDb > 2.0) { const double k = 2.0 / boostDb; for (auto& s : S) if (s > 0) s *= k; }
    }
    if (confOut) *confOut = conf;
    return S;
}

// Distancia espectral (§6.10 T7): RMS entre 40 Hz y 12 kHz de (espectro − objetivo), alineados por la mediana.
inline double spectralDistance (const BandCurve& mid, const TargetShape& shape)
{
    const auto& fc = bandCentres();
    std::vector<double> d;
    for (int b = 0; b < kBands; ++b) if (fc[(size_t) b] >= 40.0 && fc[(size_t) b] <= 12500.0) d.push_back (mid[(size_t) b] - targetAt (shape, fc[(size_t) b]));
    std::vector<double> s = d; std::sort (s.begin(), s.end());
    const double med = s[s.size() / 2];
    double acc = 0.0; for (double v : d) acc += (v - med) * (v - med);
    return std::sqrt (acc / (double) d.size());
}

// El color del estilo como curva por bandas (respuesta de sus tres filtros).
inline BandCurve styleCurve (const StyleDef& s, double scale, double fs)
{
    std::array<EqBand, kEqBands> e {};
    e[0] = { 1, s.lowShelfHz, 0.7071, s.lowShelfDb * scale };
    e[1] = { 0, s.bellHz, s.bellQ, s.bellDb * scale };
    e[2] = { 2, s.highShelfHz, 0.7071, s.highShelfDb * scale };
    return eqResponse (e, fs);
}

// Ocho biquads en cascada por canal.
class ToneEq
{
public:
    void prepare (double sampleRate, const std::array<EqBand, kEqBands>& eq, double trimDb)
    {
        fs = sampleRate; active = 0;
        for (auto& e : eq)
            if (std::fabs (e.gainDb) >= 0.01) { const auto k = eqCoefs (e, fs); fl[(size_t) active].set (k); fr[(size_t) active].set (k); ++active; }
        trim = std::pow (10.0, trimDb / 20.0);
        reset();
    }
    void reset() { for (auto& f : fl) f.reset(); for (auto& f : fr) f.reset(); }
    bool isActive() const { return active > 0 || trim != 1.0; }
    inline void process (double& l, double& r)
    {
        for (int i = 0; i < active; ++i) { l = fl[(size_t) i].process (l); r = fr[(size_t) i].process (r); }
        l *= trim; r *= trim;
    }
private:
    double fs = 48000.0, trim = 1.0;
    int active = 0;
    std::array<Biquad, kEqBands> fl {}, fr {};
};

} // namespace dh::lq
