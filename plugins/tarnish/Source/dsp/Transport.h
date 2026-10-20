#pragma once
// El transporte: la cinta pasa por la cabeza de reproducción con la velocidad que le dan el cabrestante, el
// rodillo y los carretes. Un error de velocidad e(t) se oye como un retardo modulado: si la cinta va lenta, la
// lectura se atrasa. Para una línea senoidal de desvío p a f Hz, el retardo es τ(t) = p/(2πf)·cos(φ(t))
// (docs/research.md §2.9). Todo es función de la posición en la línea de tiempo (sin estado): con el host
// marcando la posición, el compás 17 suena igual empiece donde empiece la reproducción.
//
// Las bobinas: con radio de cubo r_h, espesor h y largo bobinado ℓ, el radio es √(r_h² + ℓ·h/π) y la cantidad de
// vueltas desde el cubo es (r − r_h)/h. Así la frecuencia de cada carrete (v / 2πr) cambia a lo largo de la cinta,
// como en una máquina real, y la fase es una fórmula cerrada.
//
// También acá: el tape stop (la velocidad baja de 1 a 0 con una curva; la salida de una cabeza magnética es
// proporcional a la velocidad, así que el nivel cae con ella), el arranque (vuelve a la posición actual con un
// fundido) y el eco de copia (la capa de cinta de una vuelta antes, en el carrete que recibe).

#include "Machines.h"

namespace dh::tn {

struct TransportParams
{
    double wowPct = 0.03, flutterPct = 0.02;     // ponderados (aprox. IEC 386)
    double driftShare = 0.35;                    // parte del wow que es deriva aleatoria (no líneas)
    bool stereoWobble = false;
    double printDb = -200;                       // eco de copia (−200 = apagado)
    double grooveScale = 1.0;                    // vinilo: el wow de excentricidad crece hacia el centro (e/r)
    double stopSeconds = 0.6, startSeconds = 0.35, stopCurve = 0.5;
    uint64_t seed = 0x5EED;
};

class Transport
{
public:
    static constexpr double kBaseDelayS = 0.025;          // retardo fijo: de acá salen el wow y el tape stop
    static constexpr double kMaxStopS = 10.5, kMaxPrintS = 3.6;     // stop sincronizado: hasta 10 s (4 compases a 96 BPM)

    void prepare (double sampleRate)
    {
        fs = sampleRate;
        d0 = std::round (kBaseDelayS * fs);
        const int n = (int) (d0 + (kMaxStopS + 0.6 + kMaxPrintS) * fs) + 64;
        for (auto& b : buf) b.prepare (n);
        reset();
    }
    void reset()
    {
        for (auto& b : buf) b.clear();
        state = kRunning; s = 1; u = 0; lag = 0; xfade = 0; oldLag = 0;
        for (auto& f : printLp) f.reset();
    }
    int latency() const { return (int) d0; }

    void set (const Profile& prof, int machine, const TransportParams& tp)
    {
        P = tp; pr = prof; mach = machine;
        speedLines (prof, machine, wowLines, flLines);
        // Largo total de la cinta (s) a partir de la geometría: ℓ = π (r_full² − r_h²) / h.
        tapeLen = prof.magnetic ? kPi * (prof.fullR * prof.fullR - prof.hubR * prof.hubR) / prof.thick : 1.0;
        tapeSecs = prof.magnetic ? tapeLen / prof.speed : 1200.0;
        // Los pesos se normalizan con lo que la ponderación deja de cada línea: una línea muy lenta (los carretes
        // de una casetera de video giran a 0,1-0,4 Hz) aporta poco a la lectura aunque mueva mucho la cinta, y su
        // amplitud se limita (piso de 0,2 en la ponderación), así que las otras completan el valor pedido.
        const double rMid = std::sqrt (0.5 * (prof.hubR * prof.hubR + prof.fullR * prof.fullR));
        auto lineHz = [&] (const SpeedLine& l) { return l.kind == 0 ? l.hz : prof.speed / (kTwoPi * rMid); };
        auto norm = [&] (std::vector<SpeedLine>& v, double share) {
            double s2 = 0;
            for (auto& l : v) { const double w = flutterWeight (lineHz (l)), eff = w / std::max (0.2, w); s2 += l.weight * l.weight * eff * eff; }
            const double k = s2 > 0 ? std::sqrt (share / s2) : 0.0;
            for (auto& l : v) l.weight *= k;
        };
        norm (wowLines, 1.0 - tp.driftShare);
        norm (flLines, 1.0);
        for (size_t i = 0; i < wowLines.size(); ++i) wowLines[i].hz += 0.0, phaseW[i] = hashUni (tp.seed, i + 7, 0), phaseW2[i] = 0.137 + hashUni (tp.seed, i + 7, 1);
        for (size_t i = 0; i < flLines.size(); ++i) phaseF[i] = hashUni (tp.seed, i + 17, 0), phaseF2[i] = 0.137 + hashUni (tp.seed, i + 17, 1);
        pDrift = tp.wowPct / 100.0 * std::sqrt (tp.driftShare);
        printGain = tp.printDb > -150 ? dbToGain (tp.printDb) : 0.0;
        for (auto& f : printLp) f.setHz (1800.0, fs);
    }

    // Tape stop: on = parar, off = arrancar. Las transiciones siguen sus tiempos.
    void setStopped (bool on)
    {
        if (on && (state == kRunning || state == kStarting)) { state = kStopping; u = 0; s0 = s; }
        if (! on && (state == kStopping || state == kStopped)) { state = kStarting; u = 0; s0 = s; }
    }
    bool stopping() const { return state != kRunning; }

    // Una muestra. `pos` es la posición absoluta (muestras) en la línea de tiempo.
    inline void process (double& l, double& r, double pos)
    {
        buf[0].push (l); buf[1].push (r);
        advanceStop();
        const double t = pos / fs;
        double tauL = 0, tauR = 0;
        delays (t, pos, tauL, tauR);
        const double dL = d0 + tauL * fs + lag, dR = d0 + tauR * fs + lag;
        double yl = buf[0].read (dL), yr = buf[1].read (dR);
        if (xfade > 0)                                 // después de arrancar: de la lectura atrasada a la actual
        {
            const double k = (double) xfade / (double) xfadeLen;           // 1 → 0
            const double ol = buf[0].read (d0 + tauL * fs + oldLag), orr = buf[1].read (d0 + tauR * fs + oldLag);
            const double a = std::sin (0.5 * kPi * k), b = std::cos (0.5 * kPi * k);
            yl = ol * a + yl * b; yr = orr * a + yr * b;
            --xfade;
        }
        const double amp = state == kRunning ? 1.0 : s;    // la cabeza entrega tensión proporcional a la velocidad
        yl *= amp; yr *= amp;
        if (printGain > 0)                              // eco de copia: una vuelta del carrete que recibe
        {
            const double rev = revolutionSeconds (t) * fs;
            l = yl + printGain * printLp[0].lp (buf[0].read (dL + rev));
            r = yr + printGain * printLp[1].lp (buf[1].read (dR + rev));
        }
        else { l = yl; r = yr; }
        lastSpeed = (state == kRunning ? 1.0 : s) * (1.0 + speedDev);
    }

    // Lo que la interfaz dibuja y mide.
    double speedNow() const { return lastSpeed; }          // velocidad relativa (1 = nominal)
    double deviation() const { return speedDev; }          // desvío de velocidad (sin el stop), para medir W&F
    // Radio de los carretes (m) y vueltas acumuladas, en la posición `pos`.
    void reels (double pos, double& rSupply, double& rTake, double& turnsSupply, double& turnsTake) const
    {
        const double l = tapeLength (pos / fs);
        rTake = std::sqrt (pr.hubR * pr.hubR + l * pr.thick / kPi);
        rSupply = std::sqrt (std::max (pr.hubR * pr.hubR, pr.fullR * pr.fullR - l * pr.thick / kPi));
        turnsTake = (rTake - pr.hubR) / pr.thick;
        turnsSupply = (pr.fullR - rSupply) / pr.thick;
    }
    double tapeSeconds() const { return tapeSecs; }

private:
    enum State { kRunning, kStopping, kStopped, kStarting };
    TransportParams P; Profile pr; int mach = 0;
    std::vector<SpeedLine> wowLines, flLines;
    std::array<double, 8> phaseW {}, phaseW2 {}, phaseF {}, phaseF2 {};
    DelayLine buf[2];
    OnePole printLp[2];
    double fs = 48000, d0 = 960, tapeLen = 100, tapeSecs = 2000, pDrift = 0, printGain = 0;
    State state = kRunning; double s = 1, s0 = 1, u = 0, lag = 0, oldLag = 0;
    int xfade = 0, xfadeLen = 1;
    double speedDev = 0, lastSpeed = 1;

    double tapeLength (double t) const { return pr.speed * std::fmod (std::max (0.0, t), tapeSecs); }
    double revolutionSeconds (double t) const
    {
        const double rT = std::sqrt (pr.hubR * pr.hubR + tapeLength (t) * pr.thick / kPi);
        return clampd (kTwoPi * rT / std::max (pr.speed, 1e-3), 0.3, kMaxPrintS);
    }

    // Retardos de wow y flutter (s) para los dos canales y el desvío de velocidad para medir.
    inline void delays (double t, double pos, double& tauL, double& tauR)
    {
        double dev = 0;
        const double wowP = P.wowPct / 100.0 * P.grooveScale, flP = P.flutterPct / 100.0;
        double rS = 0, rT = 0, nS = 0, nT = 0;
        if (pr.magnetic) reels (pos, rS, rT, nS, nT);
        auto line = [&] (const SpeedLine& L, double pct, int lane, double phase0) {
            double f = L.hz, ph = 0;
            if (L.kind == 1) { f = pr.speed / (kTwoPi * rS); ph = nS; }
            else if (L.kind == 2) { f = pr.speed / (kTwoPi * rT); ph = nT; }
            else ph = f * t;
            ph += phase0;                                  // fase propia de cada línea (y de cada canal con wobble)
            const double p = pct * L.weight / std::max (0.2, flutterWeight (f));      // así la lectura ponderada da `pct`
            const double ang = kTwoPi * ph;
            if (lane == 0) dev += p * std::sin (ang);
            return p / (kTwoPi * f) * std::cos (ang);
        };
        for (size_t i = 0; i < wowLines.size(); ++i) { tauL += line (wowLines[i], wowP, 0, phaseW[i]); if (P.stereoWobble) tauR += line (wowLines[i], wowP, 1, phaseW2[i]); }
        for (size_t i = 0; i < flLines.size(); ++i)  { tauL += line (flLines[i], flP, 0, phaseF[i]);  if (P.stereoWobble) tauR += line (flLines[i], flP, 1, phaseF2[i]); }
        if (pDrift > 0)
        {
            // Deriva: ruido suave de retardo con puntos cada 0,45 s; su pendiente es el desvío de velocidad.
            const double period = 0.45 * fs, amp = pDrift * 0.45 / (1.25 * 0.55);     // pendiente RMS ≈ 1,25 por período; ponderación media ≈ 0,55
            tauL += amp * valueNoise (P.seed, pos, period, 11);
            dev -= amp * valueNoiseSlope (P.seed, pos, period, 11) * fs;
            if (P.stereoWobble) tauR += amp * valueNoise (P.seed, pos, period, 12);
        }
        if (! P.stereoWobble) tauR = tauL;
        const double lim = 0.9 * kBaseDelayS;          // nunca leer más adelante que lo escrito
        tauL = clampd (tauL, -lim, lim); tauR = clampd (tauR, -lim, lim);
        speedDev = dev;
    }

    // La curva del stop: de lineal (curve 0, frenado en seco por fricción) a exponencial (curve 1, por inercia).
    static double stopShape (double x, double curve)
    {
        x = clampd (x, 0.0, 1.0);
        const double lin = 1.0 - x;
        const double a = 1.0 + 6.0 * curve, ex = (std::exp (-a * x) - std::exp (-a)) / (1.0 - std::exp (-a));
        return lin + (ex - lin) * curve;
    }
    inline void advanceStop()
    {
        switch (state)
        {
            case kRunning: s = 1; break;
            case kStopping:
                u += 1.0 / (std::max (0.01, P.stopSeconds) * fs);
                s = s0 * stopShape (u, P.stopCurve);
                if (u >= 1.0) { s = 0; state = kStopped; }
                break;
            case kStopped: s = 0; break;
            case kStarting:
                u += 1.0 / (std::max (0.01, P.startSeconds) * fs);
                s = s0 + (1.0 - s0) * (1.0 - stopShape (u, 0.3));
                if (u >= 1.0)
                {
                    s = 1; state = kRunning;
                    if (lag > 0.5) { oldLag = lag; xfadeLen = xfade = (int) (0.04 * fs); }
                    lag = 0;
                }
                break;
        }
        if (state != kRunning) lag = std::min (lag + (1.0 - s), (kMaxStopS + 0.5) * fs);
    }
};

} // namespace dh::tn
