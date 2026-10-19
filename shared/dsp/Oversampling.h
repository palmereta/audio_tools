#pragma once
// Sobremuestreo 2x/4x/8x con FIR half-band en cascada (ventana de Kaiser). La última etapa
// (2x→1x) es la exigente: transición 20–28 kHz a 96 kHz, ~100 dB de rechazo. Las anteriores
// sólo tienen que limpiar lo que doblaría sobre la banda de audio, así que son cortas.

#include <cmath>
#include <vector>

namespace dh {

constexpr double kPiD = 3.14159265358979323846;

class HalfbandFir
{
public:
    void design (int taps, double beta)
    {
        n = taps | 1;
        h.assign ((size_t) n, 0.0f);
        const int m = n / 2;
        const double i0b = besselI0 (beta);
        for (int k = 0; k < n; ++k)
        {
            const int t = k - m;
            const double sinc = t == 0 ? 0.5 : std::sin (kPiD * t * 0.5) / (kPiD * t);
            const double r = (double) t / m;
            const double w = besselI0 (beta * std::sqrt (std::max (0.0, 1.0 - r * r))) / i0b;
            h[(size_t) k] = (float) (sinc * w);
        }
        hist.assign ((size_t) (2 * n), 0.0f);
        pos = 0;
    }

    void reset() { std::fill (hist.begin(), hist.end(), 0.0f); pos = 0; }

    inline void push (float x)
    {
        pos = (pos == 0 ? n : pos) - 1;
        hist[(size_t) pos] = hist[(size_t) (pos + n)] = x;
    }

    inline float output() const
    {
        const float* x = hist.data() + pos;
        float acc = 0.0f;
        for (int k = 0; k < n; ++k) acc += h[(size_t) k] * x[k];
        return acc;
    }

private:
    static double besselI0 (double x)
    {
        double sum = 1.0, term = 1.0;
        for (int k = 1; k < 40; ++k) { term *= (x / (2.0 * k)) * (x / (2.0 * k)); sum += term; }
        return sum;
    }
    std::vector<float> h, hist;
    int n = 1, pos = 0;
};

inline void designStage (HalfbandFir& f, int stageFromBase)
{
    // stageFromBase 0 = la etapa que entrega la frecuencia base (la más exigente).
    if (stageFromBase == 0) f.design (95, 9.0);
    else f.design (31, 7.0);
}

class Decimator
{
public:
    void prepare (int factor)
    {
        stages = factor >= 8 ? 3 : factor >= 4 ? 2 : factor >= 2 ? 1 : 0;
        for (int s = 0; s < stages; ++s) designStage (fir[s], stages - 1 - s);
        work.assign (8192 * 8, 0.0f);
    }

    // in: n·factor muestras; out: n muestras (se suma gain·y a out).
    void process (const float* in, float* out, int n, float gain)
    {
        if (stages == 0) { for (int i = 0; i < n; ++i) out[i] += gain * in[i]; return; }
        int len = n << stages;
        const float* src = in;
        for (int s = 0; s < stages; ++s)
        {
            const int outLen = len / 2;
            float* dst = s == stages - 1 ? nullptr : work.data() + (s % 2) * 32768;
            for (int i = 0; i < outLen; ++i)
            {
                fir[s].push (src[2 * i]);
                const float y = fir[s].output();   // ganancia de continua 1 (el ×2 es sólo al interpolar)
                fir[s].push (src[2 * i + 1]);
                if (dst) dst[i] = y; else out[i] += gain * y;
            }
            src = dst; len = outLen;
        }
    }

private:
    HalfbandFir fir[3];
    int stages = 0;
    std::vector<float> work;
};

class Interpolator
{
public:
    void prepare (int factor)
    {
        stages = factor >= 8 ? 3 : factor >= 4 ? 2 : factor >= 2 ? 1 : 0;
        for (int s = 0; s < stages; ++s) designStage (fir[s], s);
        work.assign (8192 * 8 * 2, 0.0f);
    }

    // in: n muestras; out: n·factor muestras.
    void process (const float* in, float* out, int n)
    {
        if (stages == 0) { std::copy (in, in + n, out); return; }
        const float* src = in;
        int len = n;
        for (int s = 0; s < stages; ++s)
        {
            float* dst = s == stages - 1 ? out : work.data() + (s % 2) * 65536;
            for (int i = 0; i < len; ++i)
            {
                fir[s].push (src[i] * 2.0f); dst[2 * i] = fir[s].output();
                fir[s].push (0.0f);          dst[2 * i + 1] = fir[s].output();
            }
            src = dst; len *= 2;
        }
    }

private:
    HalfbandFir fir[3];
    int stages = 0;
    std::vector<float> work;
};

} // namespace dh
