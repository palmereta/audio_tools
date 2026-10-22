#pragma once
// Convolución particionada sin latencia para el gabinete (IR del modelo, de fábrica y del usuario hasta kMaxIrSeconds).
//
// La IR se parte en una cabeza de kB muestras, que se convoluciona directo (muestra a muestra, como un FIR), y una cola en
// particiones de kB muestras que se convolucionan en frecuencia (overlap-save con FFT real de 2·kB y una línea de
// demora de espectros, "uniformly partitioned convolution": Wefers, "Partitioned convolution algorithms for real-time
// auralization", 2015, cap. 5; Gardner, JAES 1995, para la cabeza directa que evita la latencia). El bloque de la cola
// que termina en la muestra t aporta a las salidas desde t + 1 (la cola empieza en el retardo kB): la suma es la
// convolución completa sin latencia agregada.
//
// Todo es por muestra con un contador propio: el resultado no depende del tamaño de bloque del host (determinista).
// La FFT es propia (radix 2 iterativa, real por media longitud compleja): sin JUCE, igual en los tres sistemas.

#include "Base.h"
#include <complex>
#include <vector>

namespace dh::gt {

constexpr double kMaxIrSeconds = 3.0;

// FFT real de n puntos (potencia de dos, n ≥ 8): n reales ↔ n/2 + 1 bins. Sin normalizar a la ida; la vuelta divide por n.
class RealFft
{
public:
    void init (int n)
    {
        if (n == N) return;
        N = n; M = n / 2;
        rev.assign ((size_t) M, 0);
        int bits = 0; while ((1 << bits) < M) ++bits;
        for (int i = 0; i < M; ++i) { int r = 0; for (int b = 0; b < bits; ++b) if (i & (1 << b)) r |= 1 << (bits - 1 - b); rev[(size_t) i] = r; }
        twr.assign ((size_t) (M / 2 + 1), 0.0f); twi.assign ((size_t) (M / 2 + 1), 0.0f);
        for (int k = 0; k <= M / 2; ++k) { twr[(size_t) k] = (float) std::cos (2.0 * kPiD * k / M); twi[(size_t) k] = (float) -std::sin (2.0 * kPiD * k / M); }
        wr.assign ((size_t) (M + 1), 0.0f); wi.assign ((size_t) (M + 1), 0.0f);
        for (int k = 0; k <= M; ++k) { wr[(size_t) k] = (float) std::cos (2.0 * kPiD * k / N); wi[(size_t) k] = (float) -std::sin (2.0 * kPiD * k / N); }
        zr.assign ((size_t) M, 0.0f); zi.assign ((size_t) M, 0.0f);
    }
    int size() const { return N; }

    void forward (const float* x, float* re, float* im)
    {
        for (int n = 0; n < M; ++n) { const int r = rev[(size_t) n]; zr[(size_t) r] = x[2 * n]; zi[(size_t) r] = x[2 * n + 1]; }
        butterflies (false);
        // X[k] = Fe[k] + W^k·Fo[k], con Fe = (Z[k] + Z*[M−k]) / 2 y Fo = (Z[k] − Z*[M−k]) / 2i.
        for (int k = 0; k <= M; ++k)
        {
            const int a = k % M, b = (M - k) % M;
            const float ar = zr[(size_t) a], ai = zi[(size_t) a], br = zr[(size_t) b], bi = -zi[(size_t) b];
            const float fer = 0.5f * (ar + br), fei = 0.5f * (ai + bi);
            const float dr = ar - br, di = ai - bi;
            const float For = 0.5f * di, Foi = -0.5f * dr;
            const float c = wr[(size_t) k], s = wi[(size_t) k];
            re[k] = fer + c * For - s * Foi;
            im[k] = fei + c * Foi + s * For;
        }
    }
    void inverse (const float* re, const float* im, float* x)
    {
        // Fe = (X[k] + X*[M−k]) / 2, Fo = (X[k] − X*[M−k]) / 2 · W^−k; Z = Fe + i·Fo.
        for (int k = 0; k < M; ++k)
        {
            const float ar = re[k], ai = im[k], br = re[M - k], bi = -im[M - k];
            const float fer = 0.5f * (ar + br), fei = 0.5f * (ai + bi);
            const float dr = 0.5f * (ar - br), di = 0.5f * (ai - bi);
            const float c = wr[(size_t) k], s = -wi[(size_t) k];                  // W^−k
            const float For = dr * c - di * s, Foi = dr * s + di * c;
            const int r = rev[(size_t) k];
            zr[(size_t) r] = fer - Foi; zi[(size_t) r] = fei + For;
        }
        butterflies (true);
        const float g = 1.0f / (float) M;
        for (int n = 0; n < M; ++n) { x[2 * n] = zr[(size_t) n] * g; x[2 * n + 1] = zi[(size_t) n] * g; }
    }

private:
    int N = 0, M = 0;
    std::vector<int> rev;
    std::vector<float> twr, twi, wr, wi, zr, zi;
    // Mariposas sobre zr/zi ya permutados (en orden de bits invertidos).
    void butterflies (bool inv)
    {
        float* R = zr.data(); float* I = zi.data();
        for (int len = 2; len <= M; len <<= 1)
        {
            const int half = len >> 1, step = M / len;
            for (int i = 0; i < M; i += len)
                for (int j = 0; j < half; ++j)
                {
                    // e^(∓2πi·j/len) = tw[j·step] (tabla de M/2 + 1 valores: j·step ≤ M/2).
                    const float c = twr[(size_t) (j * step)], s = inv ? -twi[(size_t) (j * step)] : twi[(size_t) (j * step)];
                    const int a = i + j, b = a + half;
                    const float tr = R[b] * c - I[b] * s, ti = R[b] * s + I[b] * c;
                    R[b] = R[a] - tr; I[b] = I[a] - ti;
                    R[a] += tr; I[a] += ti;
                }
        }
    }
};

// Una IR lista para convolucionar: la cabeza en el tiempo y las particiones de la cola en frecuencia.
struct ConvKernel
{
    static constexpr int kB = 256;              // cabeza y partición (5,3 ms a 48 kHz)
    int len = 1, parts = 0; double fs = 0.0;
    std::vector<float> head = std::vector<float> ((size_t) kB, 0.0f);
    std::vector<float> hr, hi;                   // parts × (kB + 1)

    static int maxLength (double fs) { return (int) std::ceil (kMaxIrSeconds * fs); }
    static int partsFor (int n) { return n <= kB ? 0 : (n - kB + kB - 1) / kB; }
    void reserve (int maxLen)
    {
        const size_t m = (size_t) partsFor (maxLen) * (kB + 1);
        hr.reserve (m); hi.reserve (m);
    }
    // Sin reservar memoria si reserve() alcanzó (sirve en el hilo de audio, fuera de tiempo real).
    void build (const float* h, int n, double sampleRate, RealFft& fft, std::vector<float>& scratch)
    {
        fs = sampleRate;
        len = std::max (1, std::min (n, maxLength (sampleRate)));
        std::fill (head.begin(), head.end(), 0.0f);
        for (int i = 0; i < std::min (len, kB); ++i) head[(size_t) i] = h[i];
        parts = partsFor (len);
        hr.resize ((size_t) parts * (kB + 1)); hi.resize ((size_t) parts * (kB + 1));
        fft.init (2 * kB);
        scratch.resize ((size_t) (2 * kB));
        for (int p = 0; p < parts; ++p)
        {
            std::fill (scratch.begin(), scratch.end(), 0.0f);
            const int o = kB + p * kB;
            for (int k = 0; k < kB && o + k < len; ++k) scratch[(size_t) k] = h[o + k];
            fft.forward (scratch.data(), hr.data() + (size_t) p * (kB + 1), hi.data() + (size_t) p * (kB + 1));
        }
    }
    void swap (ConvKernel& o) noexcept
    {
        std::swap (len, o.len); std::swap (parts, o.parts); std::swap (fs, o.fs);
        head.swap (o.head); hr.swap (o.hr); hi.swap (o.hi);
    }
};

// El convolucionador (un canal). El núcleo es de otro (Engine): sólo se lee.
class PartConv
{
public:
    static constexpr int kB = ConvKernel::kB;
    void prepare (double sampleRate)
    {
        maxParts = std::max (1, ConvKernel::partsFor (ConvKernel::maxLength (sampleRate)));
        fft.init (2 * kB);
        hist.assign ((size_t) (2 * kB + 8), 0.0f);
        inBuf.assign ((size_t) (2 * kB), 0.0f); outTail.assign ((size_t) kB, 0.0f); tmp.assign ((size_t) (2 * kB), 0.0f);
        fdlR.assign ((size_t) maxParts * (kB + 1), 0.0f); fdlI.assign ((size_t) maxParts * (kB + 1), 0.0f);
        accR.assign ((size_t) (kB + 1), 0.0f); accI.assign ((size_t) (kB + 1), 0.0f);
        reset();
    }
    void reset()
    {
        std::fill (hist.begin(), hist.end(), 0.0f); std::fill (inBuf.begin(), inBuf.end(), 0.0f); std::fill (outTail.begin(), outTail.end(), 0.0f);
        // (La línea de espectros sólo hasta las particiones en uso: las de más se escriben antes de leerse.)
        const size_t used = (size_t) std::min (maxParts, K ? K->parts : 0) * (kB + 1);
        std::fill (fdlR.begin(), fdlR.begin() + (ptrdiff_t) used, 0.0f); std::fill (fdlI.begin(), fdlI.begin() + (ptrdiff_t) used, 0.0f);
        hpos = 0; idx = 0; slot = 0;
    }
    void setKernel (const ConvKernel* k) { K = k; }

    inline float process (float x)
    {
        // Cabeza: directa, con la historia duplicada (lectura contigua).
        hpos = (hpos == 0 ? kB : hpos) - 1;
        hist[(size_t) hpos] = hist[(size_t) (hpos + kB)] = x;
        const float* xp = hist.data() + hpos;
        const float* c = K ? K->head.data() : nullptr;
        float y = 0.0f;
        if (c)
        {
            float a0 = 0, a1 = 0, a2 = 0, a3 = 0;
            for (int k = 0; k < kB; k += 4) { a0 += c[k] * xp[k]; a1 += c[k + 1] * xp[k + 1]; a2 += c[k + 2] * xp[k + 2]; a3 += c[k + 3] * xp[k + 3]; }
            y = (a0 + a1) + (a2 + a3);
        }
        // Cola.
        const int P = K ? std::min (K->parts, maxParts) : 0;
        if (P == 0) return y;
        y += outTail[(size_t) idx];
        inBuf[(size_t) (kB + idx)] = x;
        if (++idx == kB) { idx = 0; block (P); }
        return y;
    }

private:
    const ConvKernel* K = nullptr;
    RealFft fft;
    std::vector<float> hist, inBuf, outTail, tmp, fdlR, fdlI, accR, accI;
    int maxParts = 1, hpos = 0, idx = 0, slot = 0;

    void block (int P)
    {
        constexpr int nb = kB + 1;
        // Espectro de [bloque anterior, bloque actual] en la ranura nueva de la línea de espectros.
        slot = (slot + 1) % P;
        fft.forward (inBuf.data(), fdlR.data() + (size_t) slot * nb, fdlI.data() + (size_t) slot * nb);
        std::fill (accR.begin(), accR.end(), 0.0f); std::fill (accI.begin(), accI.end(), 0.0f);
        float* ar = accR.data(); float* ai = accI.data();
        for (int p = 0; p < P; ++p)
        {
            const int sl = (slot - p + P) % P;
            const float* xr = fdlR.data() + (size_t) sl * nb; const float* xi = fdlI.data() + (size_t) sl * nb;
            const float* hr = K->hr.data() + (size_t) p * nb; const float* hi = K->hi.data() + (size_t) p * nb;
            for (int k = 0; k < nb; ++k) { ar[k] += xr[k] * hr[k] - xi[k] * hi[k]; ai[k] += xr[k] * hi[k] + xi[k] * hr[k]; }
        }
        fft.inverse (ar, ai, tmp.data());
        // Overlap-save: la segunda mitad es la convolución lineal del bloque actual con la cola.
        std::copy (tmp.begin() + kB, tmp.end(), outTail.begin());
        std::copy (inBuf.begin() + kB, inBuf.end(), inBuf.begin());
    }
};

} // namespace dh::gt
