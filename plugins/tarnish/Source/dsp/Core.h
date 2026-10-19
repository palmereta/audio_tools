#pragma once
// Piezas básicas del motor de Tarnish: ruido determinista (un generador por muestra y un hash por posición, para
// que lo aleatorio dependa de la línea de tiempo del host y no del tamaño de bloque), filtros en double y la línea
// de retardo del transporte. Sin JUCE.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dh::tn {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTwoPi = 2.0 * kPi;

inline double clampd (double x, double a, double b) { return std::min (b, std::max (a, x)); }
inline double dbToGain (double db) { return std::pow (10.0, db / 20.0); }
inline double gainToDb (double g) { return g > 1e-12 ? 20.0 * std::log10 (g) : -240.0; }
inline bool isFiniteD (double x) { return x - x == 0.0; }      // sobrevive a -ffast-math mejor que std::isfinite

// xorshift64*: ruido que avanza muestra a muestra (hiss, ruido de modulación).
struct Rand
{
    uint64_t s = 0x9E3779B97F4A7C15ull;
    explicit Rand (uint64_t seed = 0x9E3779B97F4A7C15ull) : s (seed ? seed : 1ull) {}
    inline uint64_t next() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return s * 0x2545F4914F6CDD1Dull; }
    inline double uni() { return (double) (next() >> 11) * (1.0 / 9007199254740992.0); }      // [0, 1)
    inline double bi() { return 2.0 * uni() - 1.0; }
    inline double gauss() { double a = 0; for (int i = 0; i < 4; ++i) a += uni(); return (a - 2.0) * 1.7320508075688772; }  // aprox., varianza 1
};

// Hash por contador (SplitMix64): el valor depende sólo de (semilla, índice). Con el índice = posición absoluta en
// la línea de tiempo, el compás 17 tiene las mismas caídas y el mismo crujido empiece donde empiece la reproducción.
inline uint64_t mix64 (uint64_t z)
{
    z += 0x9E3779B97F4A7C15ull;
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}
inline uint64_t hash3 (uint64_t seed, uint64_t a, uint64_t b = 0) { return mix64 (seed ^ mix64 (a ^ mix64 (b + 0x632BE59BD9B4E019ull))); }
inline double hashUni (uint64_t seed, uint64_t a, uint64_t b = 0) { return (double) (hash3 (seed, a, b) >> 11) * (1.0 / 9007199254740992.0); }
inline double hashBi (uint64_t seed, uint64_t a, uint64_t b = 0) { return 2.0 * hashUni (seed, a, b) - 1.0; }

// Ruido de valores suave sobre la línea de tiempo: puntos al azar cada `period` muestras, unidos por una cúbica
// (Catmull-Rom). Sin estado: f(posición). Lo usan la deriva lenta del transporte, el desvanecimiento de la radio,
// el alabeo del vinilo y los tramos dañados de la cinta. Devuelve aprox. [−1, 1].
inline double valueNoise (uint64_t seed, double pos, double period, uint64_t lane = 0)
{
    const double u = pos / period;
    const double fl = std::floor (u);
    const double t = u - fl;
    const int64_t i = (int64_t) fl;
    auto p = [&] (int64_t k) { return hashBi (seed, (uint64_t) k, lane); };
    const double y0 = p (i - 1), y1 = p (i), y2 = p (i + 1), y3 = p (i + 2);
    const double c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
    return ((c3 * t + c2) * t + c1) * t + y1;
}
// Su derivada respecto de `pos` (para pasar de un retardo a una velocidad sin integrar).
inline double valueNoiseSlope (uint64_t seed, double pos, double period, uint64_t lane = 0)
{
    const double u = pos / period;
    const double fl = std::floor (u);
    const double t = u - fl;
    const int64_t i = (int64_t) fl;
    auto p = [&] (int64_t k) { return hashBi (seed, (uint64_t) k, lane); };
    const double y0 = p (i - 1), y1 = p (i), y2 = p (i + 1), y3 = p (i + 2);
    const double c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
    return ((3.0 * c3 * t + 2.0 * c2) * t + c1) / period;
}

// Biquad (forma directa transpuesta II) con los diseños de la RBJ cookbook.
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    inline double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void reset() { z1 = z2 = 0; }
    void identity() { b0 = 1; b1 = b2 = a1 = a2 = 0; }
    void set (double B0, double B1, double B2, double A0, double A1, double A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
    void lowpass (double f, double q, double fs)
    {
        const double w = kTwoPi * clampd (f, 1.0, 0.4999 * fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void highpass (double f, double q, double fs)
    {
        const double w = kTwoPi * clampd (f, 1.0, 0.4999 * fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al);
    }
    void bandpass (double f, double q, double fs)          // ganancia 1 en el centro
    {
        const double w = kTwoPi * clampd (f, 1.0, 0.4999 * fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (al, 0, -al, 1 + al, -2 * c, 1 - al);
    }
    void peak (double f, double q, double db, double fs)
    {
        const double A = std::pow (10.0, db / 40.0), w = kTwoPi * clampd (f, 1.0, 0.4999 * fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q);
        set (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
    }
    void highshelf (double f, double db, double fs, double slope = 1.0)
    {
        const double A = std::pow (10.0, db / 40.0), w = kTwoPi * clampd (f, 1.0, 0.4999 * fs) / fs, c = std::cos (w);
        const double al = std::sin (w) / 2 * std::sqrt ((A + 1 / A) * (1 / slope - 1) + 2), sa = 2 * std::sqrt (A) * al;
        set (A * ((A + 1) + (A - 1) * c + sa), -2 * A * ((A - 1) + (A + 1) * c), A * ((A + 1) + (A - 1) * c - sa),
             (A + 1) - (A - 1) * c + sa, 2 * ((A - 1) - (A + 1) * c), (A + 1) - (A - 1) * c - sa);
    }
    // Magnitud en f (para calibrar sin simular).
    double mag (double f, double fs) const
    {
        const double w = kTwoPi * f / fs;
        const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
        const double nr = b0 + b1 * cr + b2 * c2r, ni = b1 * ci + b2 * c2i;
        const double dr = 1 + a1 * cr + a2 * c2r, di = a1 * ci + a2 * c2i;
        return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
    }
};

// Filtro de primer orden por transformación bilineal con prealabeo: H(s) = (1 + s·tz) / (1 + s·tp), con tz o tp = 0
// para quitar el cero o el polo. Las curvas de énfasis de la cinta (IEC, NAB, casete) y la RIAA salen de acá.
struct FirstOrder
{
    double b0 = 1, b1 = 0, a1 = 0, z = 0;
    void set (double tz, double tp, double fs, double fWarp = 1000.0)
    {
        // s → k·(1 − z⁻¹)/(1 + z⁻¹), con k elegido para que la frecuencia fWarp caiga exacta.
        const double wa = kTwoPi * fWarp, k = wa / std::tan (wa / (2 * fs));
        const double nb0 = 1 + tz * k, nb1 = 1 - tz * k, na0 = 1 + tp * k, na1 = 1 - tp * k;
        b0 = nb0 / na0; b1 = nb1 / na0; a1 = na1 / na0;
    }
    inline double process (double x) { const double y = b0 * x + z; z = b1 * x - a1 * y; return y; }
    void reset() { z = 0; }
    double mag (double f, double fs) const
    {
        const double w = kTwoPi * f / fs, cr = std::cos (w), ci = -std::sin (w);
        const double nr = b0 + b1 * cr, ni = b1 * ci, dr = 1 + a1 * cr, di = a1 * ci;
        return std::sqrt ((nr * nr + ni * ni) / (dr * dr + di * di));
    }
};

struct OnePole       // pasa bajos de un polo
{
    double a = 0, z = 0;
    void setHz (double hz, double fs) { a = std::exp (-kTwoPi * clampd (hz, 0.01, 0.49 * fs) / fs); }
    void setTime (double seconds, double fs) { a = seconds > 0 ? std::exp (-1.0 / (seconds * fs)) : 0.0; }
    inline double lp (double x) { z = x + a * (z - x); return z; }
    void reset (double v = 0) { z = v; }
};

struct DcBlock
{
    double r = 0.9995, x1 = 0, y1 = 0;
    void setHz (double hz, double fs) { r = std::exp (-kTwoPi * hz / fs); }
    inline double process (double x) { const double y = x - x1 + r * y1; x1 = x; y1 = y; return y; }
    void reset() { x1 = y1 = 0; }
};

// Filtro de variable de estado (TPT, Simper) con la frecuencia moduable por muestra: las caídas de la cinta mueven
// el corte del pasa bajos de las pérdidas en cada muestra.
struct Svf
{
    double ic1 = 0, ic2 = 0, g = 0.1, k = 1.41421356, a1 = 0, a2 = 0, a3 = 0;
    inline void setG (double gg, double q) { g = gg; k = 1.0 / q; a1 = 1.0 / (1.0 + g * (g + k)); a2 = g * a1; a3 = g * a2; }
    inline void setHz (double hz, double q, double fs) { setG (std::tan (kPi * clampd (hz, 5.0, 0.4999 * fs) / fs), q); }
    inline double lp (double v0)
    {
        const double v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2 * v1 - ic1; ic2 = 2 * v2 - ic2; return v2;
    }
    void reset() { ic1 = ic2 = 0; }
};

// tan(x) racional para 0 ≤ x < π/2 (las caídas mueven el filtro por muestra; std::tan pesa en Windows).
inline double fastTan (double x)
{
    const double x2 = x * x;
    return x * (135135.0 - 17325.0 * x2 + 378.0 * x2 * x2 - x2 * x2 * x2) / (135135.0 - 62370.0 * x2 + 3150.0 * x2 * x2 - 28.0 * x2 * x2 * x2);
}

// Línea de retardo circular en float con lectura de Hermite (el transporte, el eco de copia, el stop).
class DelayLine
{
public:
    void prepare (int maxSamples)
    {
        int n = 1; while (n < maxSamples + 8) n <<= 1;
        buf.assign ((size_t) n, 0.0f); mask = n - 1; w = 0;
    }
    void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }
    inline void push (double x) { buf[(size_t) w] = (float) x; w = (w + 1) & mask; }
    // `d` muestras atrás (d ≥ 1: la última escrita).
    inline double read (double d) const
    {
        d = clampd (d, 1.0, (double) (mask - 4));
        const double fd = std::floor (d);
        const int i = (int) fd; const double t = d - fd;
        const double y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const double c1 = 0.5 * (y2 - y0), c2 = y0 - 2.5 * y1 + 2.0 * y2 - 0.5 * y3, c3 = 0.5 * (y3 - y0) + 1.5 * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
    inline double at (int d) const { return buf[(size_t) ((w - d) & mask)]; }
    int capacity() const { return mask - 4; }
private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

// Ponderación aproximada de wow y flutter (IEC 386 / AES6): pasa banda con centro en 4 Hz. Es una aproximación
// derivada de las curvas publicadas, no la tabla de la norma (por eso la lectura de la interfaz dice "aprox.").
inline double flutterWeight (double f)
{
    const double r = f / 4.0 - 4.0 / std::max (f, 1e-6);
    return 1.0 / std::sqrt (1.0 + 0.25 * r * r);
}

} // namespace dh::tn
