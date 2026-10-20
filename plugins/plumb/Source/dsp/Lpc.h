#pragma once
// Fuente y filtro (docs/research.md §3.5): la LPC separa la voz en su envolvente (los formantes, un filtro de
// polos) y su excitación (el tren de pulsos glóticos). Plumb desplaza la excitación y la vuelve a pasar por la
// envolvente original, así los formantes quedan donde estaban. El filtro se aplica en forma de celosía con los
// coeficientes de reflexión interpolados muestra a muestra: análisis y síntesis con la misma secuencia de
// coeficientes son inversas exactas (la prueba de identidad depende de eso).
//
// Para mover los formantes (Throat, y Formant amount < 100 %) se buscan las raíces del polinomio, se escalan sus
// ángulos y se vuelve a armar el filtro.

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

namespace dh::pl {

constexpr int kMaxOrder = 48;
using KVec = std::array<float, kMaxOrder>;

// Levinson-Durbin sobre la autocorrelación r[0..p]: devuelve los coeficientes de reflexión k[0..p-1]
// (convención A(z) = 1 + Σ a_j z^-j, a_i^(i) = k_i).
inline void levinson (const double* r, int p, KVec& k)
{
    std::fill (k.begin(), k.end(), 0.0f);
    if (r[0] <= 1e-18) return;
    double a[kMaxOrder + 1] {}, tmp[kMaxOrder + 1] {};
    a[0] = 1; double err = r[0];
    for (int i = 1; i <= p; ++i)
    {
        double acc = r[i];
        for (int j = 1; j < i; ++j) acc += a[j] * r[i - j];
        double ki = -acc / err;
        ki = std::max (-0.999, std::min (0.999, ki));
        for (int j = 1; j < i; ++j) tmp[j] = a[j] + ki * a[i - j];
        for (int j = 1; j < i; ++j) a[j] = tmp[j];
        a[i] = ki;
        err *= (1 - ki * ki);
        k[(size_t) i - 1] = (float) ki;
        if (err <= 1e-18) break;
    }
}

// Coeficientes de reflexión → polinomio a[0..p] (a[0] = 1).
inline void stepUp (const KVec& k, int p, double* a)
{
    double tmp[kMaxOrder + 1] {};
    for (int i = 0; i <= p; ++i) a[i] = 0;
    a[0] = 1;
    for (int i = 1; i <= p; ++i)
    {
        const double ki = k[(size_t) i - 1];
        for (int j = 1; j < i; ++j) tmp[j] = a[j] + ki * a[i - j];
        for (int j = 1; j < i; ++j) a[j] = tmp[j];
        a[i] = ki;
    }
}

// Polinomio → coeficientes de reflexión (Levinson hacia atrás). Devuelve false si el filtro no es estable.
inline bool stepDown (const double* aIn, int p, KVec& k)
{
    double a[kMaxOrder + 1], tmp[kMaxOrder + 1];
    for (int i = 0; i <= p; ++i) a[i] = aIn[i];
    std::fill (k.begin(), k.end(), 0.0f);
    for (int i = p; i >= 1; --i)
    {
        const double ki = a[i];
        if (std::abs (ki) >= 0.9995) return false;
        k[(size_t) i - 1] = (float) ki;
        const double den = 1 - ki * ki;
        for (int j = 1; j < i; ++j) tmp[j] = (a[j] - ki * a[i - j]) / den;
        for (int j = 1; j < i; ++j) a[j] = tmp[j];
    }
    return true;
}

// Raíces de z^p + a1 z^(p-1) + ... + ap (las raíces de A(z) en z) por el método de Aberth, con arranque en las
// raíces anteriores si las hay.
inline void polyRoots (const double* a, int p, std::vector<std::complex<double>>& z, bool warm)
{
    using C = std::complex<double>;
    const bool cold = (int) z.size() != p || ! warm;
    if (cold)
    {
        // Arranque en un círculo del radio medio de las raíces (|a_p|^(1/p)), con un giro para no caer en simetrías.
        const double rad = std::max (0.5, std::pow (std::max (1e-12, std::abs (a[p])), 1.0 / p));
        z.resize ((size_t) p);
        for (int i = 0; i < p; ++i) z[(size_t) i] = std::polar (rad, 2 * 3.14159265358979 * (i + 0.37) / p);
    }
    const int maxIt = cold ? 400 : 60;
    for (int it = 0; it < maxIt; ++it)
    {
        double moved = 0;
        for (int i = 0; i < p; ++i)
        {
            const C zi = z[(size_t) i];
            C f = 1, df = 0;
            for (int j = 1; j <= p; ++j) { df = df * zi + f; f = f * zi + a[j]; }
            if (std::abs (df) < 1e-300) continue;
            const C ratio = f / df;
            C sum = 0;
            for (int j = 0; j < p; ++j) if (j != i) { const C d = zi - z[(size_t) j]; if (std::abs (d) > 1e-14) sum += 1.0 / d; }
            const C w = ratio / (1.0 - ratio * sum);
            if (! std::isfinite (w.real()) || ! std::isfinite (w.imag())) continue;
            z[(size_t) i] = zi - w;
            moved = std::max (moved, std::abs (w) / std::max (1e-3, std::abs (zi)));
        }
        if (moved < 1e-12) break;
    }
}

// Escala los ángulos de los polos por `factor` (formantes arriba si > 1) y limita el radio; vuelve a armar k.
// rMax limita el radio (el ancho de banda mínimo de cada polo): con orden 48 sobre una voz aguda, varios polos se
// ajustan a armónicos sueltos con picos muy angostos; corridos, caen entre armónicos y la voz se vacía. Con un ancho
// mínimo de formante (~150 Hz) el modelo corrido sigue la envolvente y no los armónicos.
inline void scaleFormants (const KVec& kIn, int p, double factor, KVec& kOut, std::vector<std::complex<double>>& roots, bool warm, double rMax = 0.99)
{
    double a[kMaxOrder + 1];
    stepUp (kIn, p, a);
    polyRoots (a, p, roots, warm);
    using C = std::complex<double>;
    std::vector<C> zs (roots.size());
    for (size_t i = 0; i < roots.size(); ++i)
    {
        const double r = std::min (rMax, std::abs (roots[i])), th = std::arg (roots[i]);
        // Los polos reales (ángulo 0 o π, como el que modela la inclinación del preénfasis) quedan reales: girar uno en
        // π dejaba un polo complejo sin su conjugado, el polinomio reconstruido ya no era el modelo y la voz perdía
        // de 4 a 40 dB con cualquier Throat distinto de 0.
        const bool isReal = std::abs (roots[i].imag()) <= 1e-9 * std::max (1e-12, std::abs (roots[i]));
        double t2 = th, r2 = r;
        if (! isReal)
        {
            // Los polos que el corrimiento lleva por encima de 0,85π no se apilan en el límite: se amortiguan. Apilados
            // en el mismo ángulo formaban una resonancia enorme cerca de Nyquist que se llevaba la energía (la síntesis
            // normalizada conserva la potencia total) y la voz quedaba 20 a 60 dB más baja.
            const double knee = 3.14159265358979 * 0.85, lim = 3.14159265358979 * 0.97;
            double t = std::abs (th) * factor;
            if (t > knee)
            {
                r2 = r * std::exp (-6.0 * (t - knee));
                t = knee + (lim - knee) * std::tanh ((t - knee) / (lim - knee));
            }
            t2 = std::copysign (t, th);
        }
        zs[i] = std::polar (r2, t2);
    }
    // Reconstrucción del polinomio (coeficientes reales: se toma la parte real).
    std::vector<C> c ((size_t) p + 1, C (0));
    c[0] = 1;
    for (int i = 0; i < p; ++i)
        for (int j = i + 1; j >= 1; --j) c[(size_t) j] -= zs[(size_t) i] * c[(size_t) j - 1];
    double b[kMaxOrder + 1];
    for (int j = 0; j <= p; ++j) b[j] = c[(size_t) j].real();
    if (! stepDown (b, p, kOut))
    {
        // Si el redondeo dejó el filtro al borde, se acercan los polos al centro y se vuelve a intentar.
        for (double shrink : { 0.995, 0.98 })
        {
            double s2 = 1; for (int j = 0; j <= p; ++j) { b[j] = c[(size_t) j].real() * s2; s2 *= shrink; }
            if (stepDown (b, p, kOut)) return;
        }
        kOut = kIn;
    }
}

// Ganancia que iguala la energía de dos modelos (celosía normalizada, Πc/A) en los armónicos de f0: corridos, los
// polos que el análisis afinó sobre los armónicos dejan de realzarlos y la voz baja unos 5 dB aunque la potencia para
// ruido blanco sea la misma. Cada armónico pesa con el desénfasis (1/|1 − pre·z⁻¹|²): la energía de la voz está
// abajo. Se limita a ±12 dB.
inline float harmonicMatchGain (const KVec& kOrig, const KVec& kMod, int p, double f0, double fs, double pre)
{
    if (f0 < 40.0 || f0 > 2000.0) return 1.0f;
    auto energy = [&] (const KVec& kk) {
        double a[kMaxOrder + 1]; stepUp (kk, p, a);
        double c = 1; for (int i = 0; i < p; ++i) c *= std::sqrt (std::max (1e-12, 1.0 - (double) kk[(size_t) i] * kk[(size_t) i]));
        double e = 0;
        for (double fh = f0; fh < 0.45 * fs; fh += f0)
        {
            const std::complex<double> w = std::polar (1.0, -2.0 * 3.14159265358979 * fh / fs);
            std::complex<double> A = 0, zi = 1;
            for (int j = 0; j <= p; ++j) { A += a[j] * zi; zi *= w; }
            e += c * c / std::max (1e-30, std::norm (A)) / std::max (1e-6, std::norm (1.0 - pre * w));
        }
        return e;
    };
    const double eo = energy (kOrig), em = energy (kMod);
    if (em <= 0.0 || eo <= 0.0) return 1.0f;
    return (float) std::clamp (std::sqrt (eo / em), 0.25, 4.0);
}

// Celosías normalizadas. La síntesis es una cadena de rotaciones ortogonales (c = √(1 − k²)):
//   f_{i-1} = c·f_i − k·b_{i-1}[n−1],   b_i[n] = k·f_i + c·b_{i-1}[n−1]
// que no puede ganar energía aunque los coeficientes cambien muestra a muestra o el residuo salte (un empalme): el
// filtro queda pasivo (c se interpola junto con k entre instantáneas: queda apenas por debajo de √(1 − k²), lo
// que hace la rotación levemente contractiva, nunca expansiva). El análisis es su inversa exacta,
//   f_i = (f_{i-1} + k·b_{i-1}[n−1]) / c,   b_i[n] = k·f_i + c·b_{i-1}[n−1],
// y deja el residuo con la potencia de la entrada (la ganancia de la síntesis para ruido blanco es 1 con cualquier
// filtro: mover formantes no cambia el nivel).
struct LatticeAnalysis
{
    std::array<double, kMaxOrder + 1> bPrev {};
    void reset() { bPrev.fill (0); }
    inline double process (double x, const KVec& k, const KVec& cv, int p)
    {
        double f = x;
        std::array<double, kMaxOrder + 1> bNew;
        bNew[0] = x;
        for (int i = 0; i < p; ++i)
        {
            const double ki = k[(size_t) i], c = cv[(size_t) i];
            const double fi = (f + ki * bPrev[(size_t) i]) / c;
            bNew[(size_t) i + 1] = ki * fi + c * bPrev[(size_t) i];
            f = fi;
        }
        for (int i = 0; i <= p; ++i) bPrev[(size_t) i] = bNew[(size_t) i];
        return f;
    }
};

struct LatticeSynthesis
{
    std::array<double, kMaxOrder + 1> bPrev {};
    void reset() { bPrev.fill (0); }
    inline double process (double e, const KVec& k, const KVec& cv, int p)
    {
        double f = e;
        std::array<double, kMaxOrder + 1> bNew;
        for (int i = p - 1; i >= 0; --i)
        {
            const double ki = k[(size_t) i], c = cv[(size_t) i];
            const double fPrev = c * f - ki * bPrev[(size_t) i];
            bNew[(size_t) i + 1] = ki * f + c * bPrev[(size_t) i];
            f = fPrev;
        }
        bNew[0] = f;
        for (int i = 0; i <= p; ++i) bPrev[(size_t) i] = bNew[(size_t) i];
        if (! std::isfinite (f) || std::abs (f) > 1e4) { reset(); return 0.0; }     // guarda numérica
        return f;
    }
};

} // namespace dh::pl
