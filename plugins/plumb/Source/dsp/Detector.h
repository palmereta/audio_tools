#pragma once
// Detector de altura de Plumb (docs/research.md §2.6). Corre sobre una copia de la entrada diezmada a ~12 kHz:
//
//  - Adquisición: YIN (diferencia acumulada normalizada) sobre todos los retardos del rango de la voz, ventana de
//    medio período máximo, primer valle bajo el umbral e interpolación parabólica.
//  - Seguimiento: con la altura enganchada, sólo los retardos a ±25 % del último período, con la diferencia
//    normalizada por energía (vale 0 en una señal periódica) y una ventana de un período: barato y con poca
//    demora. Cada 16 saltos se vuelve a adquirir para atrapar errores de octava.
//  - Refinamiento a la tasa completa alrededor del período hallado, con interpolación parabólica.
//  - Voz: compuerta de energía, aperiodicidad y la proporción de energía por encima de 4 kHz (sibilantes).
//
// Cada estimación lleva la muestra de entrada del centro de su ventana, para que el corrector la ubique en el
// tiempo del audio que se está leyendo (§3.4).

#include "Common.h"
#include <array>
#include <cmath>
#include <functional>
#include <vector>

namespace dh::pl {

constexpr double kPi = 3.14159265358979323846;

// Biquad RBJ en forma directa transpuesta (doble precisión: el detector es barato).
struct Biquad
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void lowpass (double fc, double fs, double q = 0.7071) { design (fc, fs, q, true); }
    void highpass (double fc, double fs, double q = 0.7071) { design (fc, fs, q, false); }
    void reset() { z1 = z2 = 0; }
    inline double process (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
private:
    void design (double fc, double fs, double q, bool lp)
    {
        const double w = 2 * kPi * std::min (fc, 0.49 * fs) / fs, c = std::cos (w), s = std::sin (w), al = s / (2 * q);
        const double a0 = 1 + al;
        if (lp) { b0 = (1 - c) / 2 / a0; b1 = (1 - c) / a0; b2 = b0; }
        else { b0 = (1 + c) / 2 / a0; b1 = -(1 + c) / a0; b2 = b0; }
        a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
};

struct PitchEstimate
{
    double time = 0;          // muestra de entrada (tasa completa) del centro de la ventana
    float hz = 0;             // 0 = sin voz
    float midi = 0;           // número MIDI fraccionario (A = 69 a la referencia)
    float conf = 0;           // 0..1
    bool voiced = false;
};

struct VoiceRange { float fmin, fmax; };
inline VoiceRange voicePreset (int i)
{
    static const VoiceRange r[] = { { 65, 520 }, { 80, 880 }, { 175, 1400 }, { 40, 2000 } };   // Low, Mid, High, Instrument
    return r[std::max (0, std::min (3, i))];
}

class Detector
{
public:
    static constexpr int kRing = 4096, kHop = 8;

    void prepare (double sampleRate)
    {
        fs = sampleRate; D = 0;
        ring.assign (kRing, 0.0f); full.assign (kFull, 0.0f);
        configure();
        n = 0;
    }
    // La tasa del detector: ~12 kHz, o ~24 kHz si el rango sube de 1 kHz (a 12 kHz una voz de 880 Hz tiene un
    // período de 13,6 muestras y ningún retardo entero baja del umbral: el detector se enganchaba al triple).
    void configure()
    {
        const int newD = std::max (1, (int) std::lround (fs / (range.fmax > 1000 ? 24000.0 : 12000.0)));
        if (newD == D) return;
        D = newD; fsD = fs / D; hop = fsD > 18000 ? 16 : 8;       // un salto cada ~0,67 ms en los dos casos
        ePeakDecay = std::exp (-1.0 / (0.4 * fs));                   // el pico se olvida en ~0,4 s
        winPeakDecay = std::exp (-1.0 / (0.4 * fsD / hop));          // lo mismo a la tasa de las estimaciones
        hp.highpass (50.0, fs); lp1.lowpass (0.42 * fsD, fs, 0.54); lp2.lowpass (0.42 * fsD, fs, 1.31);
        sib.highpass (4000.0, fs);
        // Un cambio de rango en marcha (Voice) cambia la decimación, pero el reloj de entrada sigue: las estimaciones
        // llevan la muestra de entrada de su ventana y el motor las busca por ese tiempo. Si volviera a 0, el motor
        // tomaría siempre la última y la vista de notas no podría descartar las viejas.
        const auto keepN = n;
        reset();
        n = keepN;
    }
    void reset()
    {
        hp.reset(); lp1.reset(); lp2.reset(); sib.reset(); dcX = dcY = 0;
        std::fill (ring.begin(), ring.end(), 0.0f); std::fill (full.begin(), full.end(), 0.0f);
        n = 0; nd = 0; hopCount = 0; tracking = false; lastPeriod = 0; sinceAcq = 0; steadyHops = 0; lastMidi = rawPrev = rawPrev2 = 0; eTot = eSib = ePeak = winPeak = 0;
    }
    void setRange (VoiceRange r) { if (! std::equal_to<float>() (r.fmin, range.fmin) || ! std::equal_to<float>() (r.fmax, range.fmax)) { range = r; configure(); } }
    void setThreshold (float gateDb) { gate = std::pow (10.0f, gateDb / 20.0f); }
    void setReference (float aHz) { refA = aHz; }
    int decimation() const { return D; }
    double estimateRate() const { return fsD / hop; }

    // Una muestra de entrada; devuelve true cuando hay una estimación nueva en `out`.
    bool push (float x, PitchEstimate& out)
    {
        // DC y pasa altos a la tasa completa; la copia completa (para el refinamiento) sale de aquí.
        const double y0 = x - dcX + 0.995 * dcY; dcX = x; dcY = y0;
        const double h = hp.process (y0);
        full[(size_t) (n & (kFull - 1))] = (float) h;
        const double s = sib.process (h);
        eTot += (h * h - eTot) * eAlpha; eSib += (s * s - eSib) * eAlpha;
        ePeak = std::max (eTot, ePeak * ePeakDecay);
        const double l = lp2.process (lp1.process (h));
        ++n;
        if (n % D != 0) return false;
        ring[(size_t) (nd & (kRing - 1))] = (float) l;
        ++nd;
        if (++hopCount < hop) return false;
        hopCount = 0;
        analyse (out);
        return true;
    }

private:
    static constexpr int kFull = 16384;
    double fs = 48000, fsD = 12000; int D = 4;
    Biquad hp, lp1, lp2, sib; double dcX = 0, dcY = 0;
    std::vector<float> ring, full;
    int64_t n = 0, nd = 0; int hopCount = 0, hop = 8;
    VoiceRange range { 80, 880 };
    float gate = 0.0018f, refA = 440.0f;          // −55 dBFS
    bool tracking = false; double lastPeriod = 0; int sinceAcq = 0, steadyHops = 0; float lastMidi = 0, rawPrev = 0, rawPrev2 = 0;
    double eTot = 0, eSib = 0, ePeak = 0, ePeakDecay = 0.99995, winPeak = 0, winPeakDecay = 0.999; const double eAlpha = 0.002;
    std::array<double, 1024> d {}, dp {};
public:
    bool dbgTrack = false; double dbgAper = 0, dbgBest = 0;
private:

    inline float at (int64_t i) const { return ring[(size_t) (i & (kRing - 1))]; }
    inline float atFull (int64_t i) const { return full[(size_t) (i & (kFull - 1))]; }

    void analyse (PitchEstimate& out)
    {
        const int tmin = std::max (2, (int) std::floor (fsD / range.fmax)), tmax = std::min (1000, (int) std::ceil (fsD / range.fmin));
        out = PitchEstimate();
        double period = 0, aper = 1; int64_t start = 0; int W = 0;
        const bool tryTrack = tracking && lastPeriod > 0 && sinceAcq < 16;
        if (tryTrack)
        {
            // Seguimiento: retardos a ±25 % del último período, diferencia normalizada por energía.
            const int lo = std::max (tmin, (int) std::floor (lastPeriod * 0.8)), hi = std::min (tmax, (int) std::ceil (lastPeriod * 1.25));
            W = std::max ((int) std::ceil (lastPeriod), (int) (0.0015 * fsD));
            start = nd - W - hi - 1;
            double best = 1e9; int bi = -1;
            for (int t = lo - 1; t <= hi + 1; ++t)
            {
                double num = 0, den = 0;
                for (int j = 0; j < W; ++j) { const double a = at (start + j), b = at (start + j + t); num += (a - b) * (a - b); den += a * a + b * b; }
                dp[(size_t) t] = den > 1e-20 ? num / den : 1.0;
                if (t >= lo && t <= hi && dp[(size_t) t] < best) { best = dp[(size_t) t]; bi = t; }
            }
            if (bi > 0 && best < 0.25)
            {
                period = bi + parabola (dp[(size_t) bi - 1], dp[(size_t) bi], dp[(size_t) bi + 1]);
                aper = best; ++sinceAcq;
            }
            else tracking = false;
        }
        if (period <= 0)
        {
            // Adquisición: YIN completo sobre el rango.
            W = std::max (tmax / 2, (int) (0.0015 * fsD));
            start = nd - W - tmax - 1;
            double sum = 0;
            dp[0] = 1;
            for (int t = 1; t <= tmax + 1; ++t)
            {
                double acc = 0;
                for (int j = 0; j < W; ++j) { const double v = at (start + j) - at (start + j + t); acc += v * v; }
                d[(size_t) t] = acc; sum += acc;
                dp[(size_t) t] = sum > 1e-20 ? acc * t / sum : 1.0;
            }
            int bi = -1;
            for (int t = tmin; t <= tmax; ++t)
                if (dp[(size_t) t] < 0.15) { while (t + 1 <= tmax && dp[(size_t) t + 1] < dp[(size_t) t]) ++t; bi = t; break; }
            if (bi < 0)
            {
                double best = 1e9;
                for (int t = tmin; t <= tmax; ++t) if (dp[(size_t) t] < best) { best = dp[(size_t) t]; bi = t; }
                if (best > 0.35) bi = -1;
            }
            // Guarda de octava: si en la mitad del período hay un valle casi tan profundo como el hallado (lo que pasa
            // cuando se tomó el doble del período real, por ejemplo a 880 Hz), el período es la mitad. Que esto no
            // haga saltar a una voz bien seguida lo cuida la continuidad de más abajo.
            if (bi > 0 && bi / 2 >= tmin)
            {
                const int h = bi / 2; int hb = h;
                for (int t = std::max (tmin, h - 2); t <= std::min (tmax, h + 2); ++t) if (dp[(size_t) t] < dp[(size_t) hb]) hb = t;
                if (dp[(size_t) hb] < 0.15 && dp[(size_t) hb] < dp[(size_t) bi] + 0.05) bi = hb;
            }
            // Continuidad: si se venía siguiendo bien un período y la readquisición propone otro con un valle flojo
            // (medio período de una /a/ a 440 Hz da 0,149, apenas bajo el umbral), se queda el anterior. Un
            // candidato con un valle hondo es un período de verdad y gana (así se corrige un enganche al triple).
            if (bi > 0 && lastPeriod > 0 && steadyHops >= 8 && std::abs (bi - lastPeriod) > 0.25 * lastPeriod)
            {
                const int lp = (int) std::lround (lastPeriod);
                int lb = -1;
                for (int t = std::max (tmin, lp - 2); t <= std::min (tmax, lp + 2); ++t) if (lb < 0 || dp[(size_t) t] < dp[(size_t) lb]) lb = t;
                if (lb > 0 && dp[(size_t) lb] < 0.1 && dp[(size_t) bi] > 0.05 && dp[(size_t) lb] < dp[(size_t) bi]) bi = lb;
            }
            if (bi > 0)
            {
                period = bi + parabola (dp[(size_t) bi - 1], dp[(size_t) bi], dp[(size_t) std::min (bi + 1, tmax + 1)]);
                aper = dp[(size_t) bi];
                tracking = true; sinceAcq = 0;
            }
            else tracking = false;
        }
        // Tiempo de la estimación: centro de la ventana más el retardo, en muestras de entrada.
        const double centreD = (double) start + (W + period) * 0.5;
        out.time = centreD * D;
        // Compuerta sobre la misma ventana que se analizó (no sobre el instante actual: la estimación es del
        // centro de la ventana, en el pasado), absoluta y relativa: 30 dB bajo el pico reciente no cuenta como voz
        // (colas de formantes y de reverberación; y en el silencio YIN, que normaliza la amplitud, encuentra
        // "períodos" en el resto de los filtros).
        double winE = 0, e1 = 0, e2 = 0;
        {
            const int L = std::max (8, W + (int) period);
            for (int j = 0; j < L; ++j) { const double v = at (start + j); winE += v * v; (j < L / 2 ? e1 : e2) += v * v; }
            winE /= L;
        }
        winPeak = std::max (winE, winPeak * winPeakDecay);
        // Estable: las dos mitades de la ventana con energía parecida (en el arranque de una nota la ventana es
        // mitad silencio y la primera estimación sale una octava arriba).
        const bool steady = e1 > 0.15 * e2 && e2 > 0.15 * e1;
        const bool loud = winE > (double) gate * gate && winE > winPeak * 1e-3 && steady;
        const bool sibilant = eSib > 0.45 * eTot;
        if (period <= 0 || ! loud || sibilant) { tracking = tracking && loud && ! sibilant; steadyHops = 0; if (! loud) { lastPeriod = 0; lastMidi = 0; } return; }
        // Refinamiento a la tasa completa: ±D muestras alrededor del período, ventana de un período.
        const double P = period * D;
        const int pc = (int) std::lround (P), Wf = std::min (kFull / 4, std::max (2 * pc, (int) (0.003 * fs)));     // dos períodos: la mitad de ruido
        const int64_t sf = n - Wf - pc - D - 2;
        double best = 1e9; int bk = 0; double v[64] {};
        const int R = std::min (30, D + 1);
        for (int k = -R; k <= R; ++k)
        {
            double num = 0, den = 0;
            for (int j = 0; j < Wf; ++j) { const double a = atFull (sf + j), b = atFull (sf + j + pc + k); num += (a - b) * (a - b); den += a * a + b * b; }
            v[k + R] = den > 1e-20 ? num / den : 1.0;
            if (v[k + R] < best) { best = v[k + R]; bk = k; }
        }
        double Pf = P;
        if (bk > -R && bk < R) Pf = pc + bk + parabola (v[bk + R - 1], v[bk + R], v[bk + R + 1]);
        lastPeriod = Pf / D;
        out.hz = (float) (fs / Pf);
        out.midi = 69.0f + 12.0f * std::log2 (out.hz / refA);
        out.conf = (float) std::max (0.0, 1.0 - std::min (aper, best) * 3.0);
        out.voiced = out.conf > 0.25f;
        // Antirrebote: un salto de más de 5 semitonos se acepta cuando tres estimaciones seguidas coinciden (1,3 ms);
        // una o dos fuera de lugar (un valle espurio de la readquisición) se reemplazan por la anterior.
        if (out.voiced && lastMidi > 0 && std::abs (out.midi - lastMidi) > 5.0f && (std::abs (out.midi - rawPrev) > 1.0f || std::abs (out.midi - rawPrev2) > 1.0f))
        {
            rawPrev2 = rawPrev; rawPrev = out.midi;
            out.midi = lastMidi; out.hz = refA * std::exp2 ((lastMidi - 69.0f) / 12.0f); out.conf *= 0.5f;
        }
        else { rawPrev2 = rawPrev; rawPrev = out.midi; if (out.voiced) lastMidi = out.midi; }
        steadyHops = (out.voiced && aper < 0.05) ? steadyHops + 1 : 0;
        dbgTrack = tryTrack; dbgAper = aper; dbgBest = best;
    }

    static double parabola (double a, double b, double c)
    {
        const double den = a - 2 * b + c;
        if (std::abs (den) < 1e-12) return 0;
        return std::max (-0.5, std::min (0.5, 0.5 * (a - c) / den));
    }
};

} // namespace dh::pl
