#pragma once
// Sobremuestreo ×4 en double (dos medias bandas en cascada, Kaiser) y las dos etapas no lineales que lo usan:
// la saturación (cinta o válvula, docs/research.md §4.6) y el recortador suave previo al limitador (§4.7).
//
// Latencia entera por construcción: la etapa externa (×1↔×2, 95 coeficientes) aporta 47 muestras, la interna
// (×2↔×4, 31 coeficientes) 7.5 muestras de ×2, y la fase de decimación de la externa (1) completa: 54 muestras de
// la frecuencia base en total (lo verifica mastertest).

#include "Filters.h"

namespace dh::lq {

class HalfbandD
{
public:
    void design (int taps, double beta)
    {
        n = taps | 1;
        const int m = n / 2;
        auto bessel0 = [] (double v) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (v / (2 * k)) * (v / (2 * k)); s += t; } return s; };
        const double i0b = bessel0 (beta);
        idx.clear(); h.clear();
        for (int k = 0; k < n; ++k)
        {
            const int t = k - m;
            const double sinc = t == 0 ? 0.5 : std::sin (kPiD * t * 0.5) / (kPiD * t);
            const double r = (double) t / m;
            const double v = sinc * bessel0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
            if (t == 0 || (t % 2) != 0) { idx.push_back (k); h.push_back (v); }      // los pares (salvo el centro) son cero
        }
        hist.assign ((size_t) (2 * n), 0.0); pos = 0;
    }
    void reset() { std::fill (hist.begin(), hist.end(), 0.0); pos = 0; }
    inline void push (double x) { pos = (pos == 0 ? n : pos) - 1; hist[(size_t) pos] = hist[(size_t) (pos + n)] = x; }
    inline double output() const
    {
        const double* x = hist.data() + pos;
        double acc = 0.0;
        for (size_t i = 0; i < h.size(); ++i) acc += h[i] * x[idx[i]];
        return acc;
    }
    int taps() const { return n; }
private:
    std::vector<double> h, hist;
    std::vector<int> idx;
    int n = 1, pos = 0;
};

// Un canal: sube ×4, aplica `f` a cada sub-muestra y baja. `latency()` en muestras de la frecuencia base.
class Os4
{
public:
    Os4() { upA.design (95, 9.0); dnA.design (95, 9.0); upB.design (31, 7.0); dnB.design (31, 7.0); }
    void reset() { upA.reset(); dnA.reset(); upB.reset(); dnB.reset(); }
    static constexpr int latency() { return 54; }

    template <typename F> inline double process (double x, F&& f)
    {
        double a[2];
        upA.push (2.0 * x); a[0] = upA.output();
        upA.push (0.0);     a[1] = upA.output();
        for (int i = 0; i < 2; ++i)
        {
            upB.push (2.0 * a[i]); double b0 = f (upB.output());
            upB.push (0.0);        double b1 = f (upB.output());
            // Interna: fase de decimación 0 (la salida después del primer empuje).
            dnB.push (b0); a[i] = dnB.output(); dnB.push (b1);
        }
        // Externa: fase 1 (la salida después del segundo empuje).
        dnA.push (a[0]); dnA.push (a[1]);
        return dnA.output();
    }
private:
    HalfbandD upA, dnA, upB, dnB;
};

struct SatParams { bool on = false; int flavour = 0; double driveDb = 0.0, inputLufs = -14.0; };

class Saturation
{
public:
    void prepare (double sampleRate, const SatParams& p)
    {
        fs = sampleRate; P = p;
        bias = p.flavour == 0 ? 0.04 : 0.15;
        // Normaliza la entrada a −14 LUFS y deja 12 dB de margen: la curva no depende del volumen de la mezcla.
        gIn = std::pow (10.0, (-14.0 - p.inputLufs + p.driveDb - 12.0) / 20.0);
        const double t = std::tanh (bias);
        norm = 1.0 / (1.0 - t * t);                     // pendiente unitaria en el origen
        tb = t;
        for (int c = 0; c < 2; ++c)
        {
            pre[c].set (highShelf (3000.0, 3.0, 4.0 * fs)); post[c].set (highShelf (3000.0, -3.0, 4.0 * fs));
            dc[c].set (highPass (5.0, 0.7071, fs));
        }
        reset();
    }
    void reset() { for (int c = 0; c < 2; ++c) { os[c].reset(); pre[c].reset(); post[c].reset(); dc[c].reset(); } }
    static constexpr int latency() { return Os4::latency(); }

    inline void process (double& l, double& r, bool run)
    {
        if (! run) return;
        const bool on = P.on;
        l = proc (0, l, on); r = proc (1, r, on);
    }
private:
    inline double shape (int c, double x)
    {
        double u = x * gIn;
        if (P.flavour == 0) u = pre[c].process (u);
        double y = (std::tanh (u + bias) - tb) * norm;
        if (P.flavour == 0) y = post[c].process (y);
        return y / gIn;
    }
    inline double proc (int c, double x, bool on)
    {
        const double y = os[c].process (x, [&] (double v) { return on ? shape (c, v) : v; });
        return on ? dc[c].process (y) : y;
    }
    double fs = 48000.0, gIn = 1.0, bias = 0.04, norm = 1.0, tb = 0.0;
    SatParams P;
    Os4 os[2];
    Biquad pre[2], post[2], dc[2];
};

// Recortador suave sobremuestreado: lineal hasta `knee`·t, después tanh hasta t.
inline double clipCurve (double x, double t, double knee = 0.7)
{
    const double a = std::fabs (x), k = knee * t;
    if (a <= k) return x;
    const double y = k + (t - k) * std::tanh ((a - k) / (t - k));
    return x < 0 ? -y : y;
}

class Clipper
{
public:
    void prepare (double thresholdLin, bool enabled) { t = thresholdLin; on = enabled; reset(); }
    void reset() { os[0].reset(); os[1].reset(); clipped = 0; }
    static constexpr int latency() { return Os4::latency(); }
    inline void process (double& l, double& r, bool run)
    {
        if (! run) return;
        l = os[0].process (l, [&] (double v) { if (! on) return v; if (std::fabs (v) > knee () ) ++clipped; return clipCurve (v, t); });
        r = os[1].process (r, [&] (double v) { if (! on) return v; if (std::fabs (v) > knee () ) ++clipped; return clipCurve (v, t); });
    }
    long long clippedSubSamples() const { return clipped; }
private:
    double knee() const { return 0.7 * t; }
    double t = 1.0; bool on = false;
    Os4 os[2];
    long long clipped = 0;
};

} // namespace dh::lq
