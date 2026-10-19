#pragma once
// Filtros del masterizador, en double: biquads del "Audio EQ Cookbook" de R. Bristow-Johnson (campana,
// estantes, Butterworth, pasa todo), su respuesta en dB (para ajustar la EQ automática) y el divisor de tres
// bandas Linkwitz-Riley de 4.º orden con compensación de fase (las tres bandas suman un pasa todo).

#include "Loudness.h"
#include <complex>

namespace dh::lq {

struct Coefs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };

inline Coefs peaking (double f, double q, double gainDb, double fs)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPiD * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w);
    const double a0 = 1.0 + al / A;
    return { (1.0 + al * A) / a0, -2.0 * c / a0, (1.0 - al * A) / a0, -2.0 * c / a0, (1.0 - al / A) / a0 };
}
// Estantes con pendiente S = 1 (la más empinada sin sobrepico).
inline Coefs lowShelf (double f, double gainDb, double fs, double S = 1.0)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPiD * f / fs, c = std::cos (w), s = std::sin (w);
    const double al = s / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0), sa = 2.0 * std::sqrt (A) * al;
    const double a0 = (A + 1) + (A - 1) * c + sa;
    return { A * ((A + 1) - (A - 1) * c + sa) / a0, 2 * A * ((A - 1) - (A + 1) * c) / a0, A * ((A + 1) - (A - 1) * c - sa) / a0,
             -2 * ((A - 1) + (A + 1) * c) / a0, ((A + 1) + (A - 1) * c - sa) / a0 };
}
inline Coefs highShelf (double f, double gainDb, double fs, double S = 1.0)
{
    const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * kPiD * f / fs, c = std::cos (w), s = std::sin (w);
    const double al = s / 2.0 * std::sqrt ((A + 1.0 / A) * (1.0 / S - 1.0) + 2.0), sa = 2.0 * std::sqrt (A) * al;
    const double a0 = (A + 1) - (A - 1) * c + sa;
    return { A * ((A + 1) + (A - 1) * c + sa) / a0, -2 * A * ((A - 1) + (A + 1) * c) / a0, A * ((A + 1) + (A - 1) * c - sa) / a0,
             2 * ((A - 1) - (A + 1) * c) / a0, ((A + 1) - (A - 1) * c - sa) / a0 };
}
inline Coefs lowPass (double f, double q, double fs)
{
    const double w = 2.0 * kPiD * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + al;
    return { (1 - c) / 2 / a0, (1 - c) / a0, (1 - c) / 2 / a0, -2 * c / a0, (1 - al) / a0 };
}
inline Coefs highPass (double f, double q, double fs)
{
    const double w = 2.0 * kPiD * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + al;
    return { (1 + c) / 2 / a0, -(1 + c) / a0, (1 + c) / 2 / a0, -2 * c / a0, (1 - al) / a0 };
}
inline Coefs allPass (double f, double q, double fs)
{
    const double w = 2.0 * kPiD * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + al;
    return { (1 - al) / a0, -2 * c / a0, (1 + al) / a0, -2 * c / a0, (1 - al) / a0 };
}
inline Coefs bandPass (double f, double q, double fs)          // ganancia 0 dB en el centro
{
    const double w = 2.0 * kPiD * f / fs, al = std::sin (w) / (2.0 * q), c = std::cos (w), a0 = 1.0 + al;
    return { al / a0, 0.0, -al / a0, -2 * c / a0, (1 - al) / a0 };
}

// Respuesta en dB de un biquad a la frecuencia f.
inline double responseDb (const Coefs& k, double f, double fs)
{
    const std::complex<double> z1 = std::polar (1.0, -2.0 * kPiD * f / fs), z2 = z1 * z1;
    const auto num = k.b0 + k.b1 * z1 + k.b2 * z2, den = 1.0 + k.a1 * z1 + k.a2 * z2;
    return 20.0 * std::log10 (std::max (1e-12, std::abs (num / den)));
}

struct Biquad
{
    Coefs k;
    double z1 = 0, z2 = 0;
    void set (const Coefs& c) { k = c; }
    void reset() { z1 = z2 = 0; }
    inline double process (double x)
    {
        const double y = k.b0 * x + z1;
        z1 = k.b1 * x - k.a1 * y + z2; z2 = k.b2 * x - k.a2 * y;
        return y;
    }
};

// Linkwitz-Riley de 4.º orden: dos Butterworth de 2.º orden en cascada (−6 dB en el corte, salidas en fase).
struct LR4
{
    Biquad lp1, lp2, hp1, hp2;
    void set (double f, double fs)
    {
        const double q = 0.70710678118654752;
        lp1.set (lowPass (f, q, fs)); lp2.set (lowPass (f, q, fs)); hp1.set (highPass (f, q, fs)); hp2.set (highPass (f, q, fs));
    }
    void reset() { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }
    inline void process (double x, double& lo, double& hi) { lo = lp2.process (lp1.process (x)); hi = hp2.process (hp1.process (x)); }
};

// Tres bandas: corte en f1 (graves | resto) y en f2 (medios | agudos). La banda grave pasa por un pasa todo de
// 2.º orden en f2 (Q 0.7071): así las tres tienen la misma fase y su suma es un pasa todo puro (plana en magnitud).
struct Crossover3
{
    LR4 a, b;
    Biquad apLow;
    void set (double f1, double f2, double fs) { a.set (f1, fs); b.set (f2, fs); apLow.set (allPass (f2, 0.70710678118654752, fs)); }
    void reset() { a.reset(); b.reset(); apLow.reset(); }
    inline void process (double x, double& lo, double& mid, double& hi)
    {
        double rest;
        a.process (x, lo, rest);
        b.process (rest, mid, hi);
        lo = apLow.process (lo);
    }
};

} // namespace dh::lq
