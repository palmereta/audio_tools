#pragma once
// Compresión y limitación del masterizador.
//
//   Compressor: realimentación hacia adelante, detector en el dominio log después del computador de ganancia y
//     detector de pico "suave desacoplado" (Giannoulis, Massberg y Reiss, JAES 60(6), 2012, ec. 4 y 17), con un
//     pre-detector RMS opcional y enlace estéreo por el máximo de los dos canales. Lo usan el multibanda y el "glue".
//   Limiter: limitador de pico verdadero con anticipación: la ganancia necesaria se calcula sobre la señal
//     sobremuestreada ×4 (el interpolador del Anexo 2 de BS.1770), se toma el mínimo móvil sobre la ventana de
//     anticipación, se aplica una liberación doble (rápida hasta la reducción sostenida, lenta debajo) y se suaviza con dos promedios móviles que, juntos,
//     no superan la ventana (Signalsmith, "Designing a limiter", 2022): la ganancia llega justo a tiempo y nunca
//     deja pasar el pico. La señal se demora lo mismo. Una retención para graves (como x42-dpl) evita que la
//     liberación rápida module las frecuencias bajas.

#include "Filters.h"
#include <deque>

namespace dh::lq {

struct CompParams
{
    double thresholdDb = -20, ratio = 2, kneeDb = 6, attackMs = 20, releaseMs = 200, rmsMs = 0, makeupDb = 0;
};

class Compressor
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { y1 = yL = 0.0; rms = 0.0; grMax = 0.0; grSum = 0.0; grN = 0; }
    void set (const CompParams& p)
    {
        P = p;
        aA = std::exp (-1.0 / (std::max (0.05, p.attackMs) * 1e-3 * fs));
        aR = std::exp (-1.0 / (std::max (1.0, p.releaseMs) * 1e-3 * fs));
        aRms = p.rmsMs > 0.0 ? std::exp (-1.0 / (p.rmsMs * 1e-3 * fs)) : 0.0;
        makeup = std::pow (10.0, p.makeupDb / 20.0);
    }

    // Computador de ganancia con rodilla suave (ec. 4): nivel de entrada en dB → nivel de salida en dB.
    static double staticCurve (double xG, double T, double R, double W)
    {
        if (2.0 * (xG - T) < -W) return xG;
        if (2.0 * std::fabs (xG - T) <= W && W > 0.0) { const double d = xG - T + W / 2.0; return xG + (1.0 / R - 1.0) * d * d / (2.0 * W); }
        return T + (xG - T) / R;
    }

    // Ganancia lineal para la muestra de detección `det` (amplitud, ya enlazada entre canales).
    inline double gainFor (double det)
    {
        double lev = std::fabs (det);
        if (aRms > 0.0) { rms = aRms * rms + (1.0 - aRms) * lev * lev; lev = std::sqrt (rms * 2.0); }   // ×2: un seno lee su pico
        const double xG = lev > 1e-9 ? 20.0 * std::log10 (lev) : -180.0;
        const double xL = xG - staticCurve (xG, P.thresholdDb, P.ratio, P.kneeDb);                      // reducción deseada ≥ 0
        y1 = std::max (xL, aR * y1 + (1.0 - aR) * xL);
        yL = aA * yL + (1.0 - aA) * y1;
        grMax = std::max (grMax, yL); grSum += yL; ++grN;
        return std::pow (10.0, -yL / 20.0) * makeup;
    }
    double currentGrDb() const { return yL; }
    double averageGrDb() const { return grN ? grSum / (double) grN : 0.0; }
    double maxGrDb() const { return grMax; }
    void resetStats() { grMax = 0.0; grSum = 0.0; grN = 0; }

private:
    double fs = 48000.0;
    CompParams P;
    double aA = 0.99, aR = 0.999, aRms = 0.0, makeup = 1.0;
    double y1 = 0.0, yL = 0.0, rms = 0.0;
    double grMax = 0.0, grSum = 0.0; long long grN = 0;
};


// Detector de picos ×8 para el limitador: más fino que el medidor ×4 de la norma (que puede leer hasta 0.69 dB de
// menos cerca de Nyquist). Sinc con ventana de Kaiser (β = 9) de ±24 muestras por fase. Devuelve el máximo del
// tramo entre las muestras n − 24 y n − 23 (sus 8 sub-muestras y las dos muestras que lo encierran).
class PeakDetector8
{
public:
    static constexpr int H = 24, kDelay = H;
    PeakDetector8()
    {
        auto bessel0 = [] (double v) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (v / (2 * k)) * (v / (2 * k)); s += t; } return s; };
        const double beta = 9.0, i0b = bessel0 (beta);
        for (int ph = 0; ph < 8; ++ph)
            for (int m = 0; m < 2 * H; ++m)
            {
                // Sub-muestra en (n − H + ph/8): toma muestras n − 2H + 1 + m, m = 0..2H−1.
                const double t = (double) ph / 8.0 + (double) (H - 1 - m);
                const double w = std::fabs (t) >= H ? 0.0 : bessel0 (beta * std::sqrt (1.0 - (t / H) * (t / H))) / i0b;
                taps[(size_t) ph][(size_t) m] = (std::fabs (t) < 1e-12 ? 1.0 : std::sin (kPiD * t) / (kPiD * t)) * w;
            }
    }
    void reset() { hist.fill (0.0); pos = 0; }
    inline double process (double x)
    {
        // Historia duplicada (cada muestra se escribe en pos y pos + 2H): la ventana siempre es contigua y el
        // producto con cada fase se vectoriza.
        hist[(size_t) pos] = hist[(size_t) (pos + 2 * H)] = x;
        pos = (pos + 1) % (2 * H);
        const double* w = hist.data() + pos;                      // las 2H muestras más viejas → más nuevas
        double m = std::max (std::fabs (w[H - 1]), std::fabs (w[H]));
        for (int ph = 1; ph < 8; ++ph)
        {
            const double* t = taps[(size_t) ph].data();
            double a0 = 0.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;
            for (int k = 0; k < 2 * H; k += 4) { a0 += t[k] * w[k]; a1 += t[k + 1] * w[k + 1]; a2 += t[k + 2] * w[k + 2]; a3 += t[k + 3] * w[k + 3]; }
            m = std::max (m, std::fabs ((a0 + a1) + (a2 + a3)));
        }
        return m;
    }
private:
    std::array<std::array<double, 2 * H>, 8> taps {};
    std::array<double, 4 * H> hist {};
    int pos = 0;
};

struct LimiterParams
{
    double ceilingDb = -1.0;          // dBTP
    double lookaheadMs = 3.0;
    double releaseFastMs = 30.0, releaseSlowMs = 400.0;
    double slowAboveDb = 1.0;         // la liberación es rápida mientras la reducción supere la sostenida en más que esto
    double lfHoldMs = 15.0;           // retención de la liberación cuando el pico viene de los graves
};

class Limiter
{
public:
    void prepare (double sampleRate, const LimiterParams& p)
    {
        fs = sampleRate; P = p;
        look = std::max (8, (int) std::lround (p.lookaheadMs * 1e-3 * fs));
        look += look & 1;                                       // par: dos promedios de look/2
        half = look / 2;
        ceiling = std::pow (10.0, p.ceilingDb / 20.0);
        kFast = 1.0 - std::exp (-1.0 / (p.releaseFastMs * 1e-3 * fs));
        kSlow = 1.0 - std::exp (-1.0 / (p.releaseSlowMs * 1e-3 * fs));
        holdN = (int) std::lround (p.lfHoldMs * 1e-3 * fs);
        kSustUp = 1.0 - std::exp (-1.0 / (0.4 * fs));          // promedio de la reducción necesaria (~0.4 s)
        lfL.set (lowPass (500.0, 0.7071, fs)); lfR.set (lowPass (500.0, 0.7071, fs));
        detL.reset(); detR.reset();
        // Demora: la ventana de anticipación (look − 1, por los promedios) más las 6 muestras del interpolador.
        delay = look - 1 + kDetectorDelay;
        delayL.assign ((size_t) delay, 0.0); delayR.assign ((size_t) delay, 0.0);
        box1.assign ((size_t) half, 1.0); box2.assign ((size_t) half, 1.0);
        reset();
    }
    void reset()
    {
        std::fill (delayL.begin(), delayL.end(), 0.0); std::fill (delayR.begin(), delayR.end(), 0.0);
        std::fill (box1.begin(), box1.end(), 1.0); std::fill (box2.begin(), box2.end(), 1.0);
        sum1 = sum2 = (double) half; dpos = 0; bpos = 0; minq.clear(); n = 0; rel = 1.0; hold = 0; grSust = 0.0;
        detL.reset(); detR.reset(); lfL.reset(); lfR.reset(); grMax = 0.0; grSum = 0.0; grN = 0;
    }
    int latency() const { return delay; }

    // Procesa un par de muestras en su lugar (salida demorada `latency()` muestras).
    inline void process (double& l, double& r)
    {
        // 1) Ganancia permitida para esta muestra, mirando sus sub-muestras ×4.
        const double pk = std::max (detL.process (l), detR.process (r));
        const double need = pk > ceiling ? ceiling / pk : 1.0;
        // Retención para graves: si el pico está dominado por lo que hay debajo de 500 Hz, la liberación espera.
        const double lf = std::max (std::fabs (lfL.process (l)), std::fabs (lfR.process (r)));
        if (need < 1.0 && lf > 0.7 * pk) hold = holdN;
        // 2) Mínimo móvil sobre la ventana de anticipación (cola monótona, O(1) amortizado).
        while (! minq.empty() && minq.back().second >= need) minq.pop_back();
        minq.push_back ({ n, need });
        while (minq.front().first <= n - look - 1) minq.pop_front();      // look + 1: cubre las dos muestras del tramo
        const double gmin = minq.front().second;
        // 3) Liberación doble: rápida mientras la reducción esté por encima de la sostenida (los picos aislados, un
        //    golpe de bombo, se sueltan enseguida y no hunden lo que sigue), lenta por debajo (la reducción de fondo
        //    de un programa denso no se mueve con cada golpe, que es lo que suena a bombeo y a distorsión).
        // La sostenida es el promedio (~0.4 s) de la reducción *necesaria*, no de la aplicada: si se promediara la
        // aplicada, la liberación lenta la mantendría arriba y el limitador terminaría actuando como un compresor lento.
        const double grNow = -20.0 * std::log10 (std::max (1e-9, rel));
        grSust += (-20.0 * std::log10 (std::max (1e-9, need)) - grSust) * kSustUp;
        if (gmin < rel) rel = gmin;
        else if (hold > 0) --hold;
        else
        {
            const double k = grNow > grSust + P.slowAboveDb ? kFast : kSlow;
            rel = std::min (gmin, rel + (1.0 - rel) * k);
        }
        // 4) Dos promedios móviles de look/2: nunca superan la ganancia permitida dentro de la ventana.
        sum1 += rel - box1[(size_t) bpos]; box1[(size_t) bpos] = rel;
        const double s1 = sum1 / (double) half;
        sum2 += s1 - box2[(size_t) bpos]; box2[(size_t) bpos] = s1;
        bpos = (bpos + 1) % half;
        const double g = std::min (1.0, sum2 / (double) half);
        // 5) Señal demorada × ganancia.
        const double dl = delayL[(size_t) dpos], dr = delayR[(size_t) dpos];
        delayL[(size_t) dpos] = l; delayR[(size_t) dpos] = r;
        dpos = (dpos + 1) % delay;
        l = dl * g; r = dr * g;
        const double gr = -20.0 * std::log10 (g);
        grMax = std::max (grMax, gr); grSum += gr; ++grN; lastGr = gr;
        ++n;
    }
    double currentGrDb() const { return lastGr; }
    double averageGrDb() const { return grN ? grSum / (double) grN : 0.0; }
    double maxGrDb() const { return grMax; }
    void resetStats() { grMax = 0.0; grSum = 0.0; grN = 0; }

private:
    double fs = 48000.0;
    LimiterParams P;
    static constexpr int kDetectorDelay = PeakDetector8::kDelay;
    int look = 144, half = 72, delay = 149, dpos = 0, bpos = 0, hold = 0, holdN = 720;
    long long n = 0;
    double ceiling = 0.89, kFast = 0.001, kSlow = 0.0001, kSustUp = 0.0001, rel = 1.0, sum1 = 0, sum2 = 0, lastGr = 0, grSust = 0.0;
    std::vector<double> delayL, delayR, box1, box2;
    std::deque<std::pair<long long, double>> minq;
    PeakDetector8 detL, detR;
    Biquad lfL, lfR;
    double grMax = 0.0, grSum = 0.0; long long grN = 0;
};

// Recortador suave (antes del limitador, para los estilos fuertes): tanh mezclada con recorte duro, sobremuestreada
// afuera (el motor lo corre a ×4 con el interpolador/decimador compartido).
inline double softClip (double x, double t)
{
    if (t <= 0.0) return x;
    const double a = std::fabs (x);
    if (a <= 0.5 * t) return x;                                  // lineal hasta la mitad del umbral
    const double over = (a - 0.5 * t) / (0.5 * t);
    const double y = 0.5 * t + 0.5 * t * std::tanh (over);
    return x < 0 ? -y : y;
}

} // namespace dh::lq

namespace dh::lq {

// El limitador de salida. La modulación de ganancia, cuando la reducción es grande y rápida, crea picos entre
// muestras que la entrada no tenía: con el detector ×8 de ±24 muestras quedan en +0.07 dB como mucho (medido
// contra un interpolador ×8 independiente de ±32 muestras en ruido, batería, cuadradas e impulsos). Un margen
// interno de 0.08 dB deja el resultado en el techo nominal. Una segunda etapa en serie no actuaba nunca: su
// detector es el mismo.
class TruePeakLimiter
{
public:
    static constexpr double kMarginDb = 0.08;
    void prepare (double fs, const LimiterParams& p) { LimiterParams q = p; q.ceilingDb -= kMarginDb; a.prepare (fs, q); }
    void reset() { a.reset(); }
    int latency() const { return a.latency(); }
    inline void process (double& l, double& r) { a.process (l, r); }
    double currentGrDb() const { return a.currentGrDb(); }
    double averageGrDb() const { return a.averageGrDb(); }
    double maxGrDb() const { return a.maxGrDb(); }
    void resetStats() { a.resetStats(); }
private:
    Limiter a;
};

} // namespace dh::lq
