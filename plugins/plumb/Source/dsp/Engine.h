#pragma once
// Motor de Plumb (docs/research.md §3.6). Una sola cadena para los tres modos; cambia sólo cuánto adelanto tiene
// el análisis sobre el audio:
//
//   entrada ─┬─ detector (Detector.h) ─ corrector (Correct.h) ─ historial con tiempo ─────────┐ Δ(t), período(t)
//            ├─ LPC cada 128 muestras ─ instantáneas de coeficientes ────────────────┐          │
//            └─ celosía de análisis ─ residuo ─ empalmador ─ celosía de síntesis ─ salida
//
// El empalmador lee el residuo con un cabezal que avanza a r = 2^(Δ/12) muestras por muestra. Cuando el retardo
// de contenido d = escritura − lectura se acerca al mínimo D (corrección hacia arriba) el cabezal salta un
// período atrás (repite un ciclo); cuando pasa D + T (hacia abajo) salta uno adelante (descarta un ciclo). El
// salto se ajusta ±4 muestras por correlación, se hace entre pulsos glóticos y con un fundido de 0,5 ms. La
// latencia informada es D (más el adelanto del modo); mientras corrige, el contenido se demora hasta un período
// más (§3.4). La síntesis usa los coeficientes del instante que se está leyendo: con Δ = 0 la salida es la
// entrada demorada, exacta.

#include "Correct.h"
#include "Lpc.h"
#include <atomic>
#include <functional>
#include <vector>

namespace dh::pl {

enum class Mode { live = 0, tight = 1, studio = 2 };

struct EngineParams
{
    CorrectParams correct;
    int voice = 1;                 // 0 grave, 1 media, 2 aguda, 3 instrumento
    float refA = 440.0f;
    float gateDb = -55.0f;
    bool formants = true;          // preservar formantes (la excitación pasa por la envolvente original)
    float formantAmount = 1.0f;    // 1 = quedan fijos; 0 = se mueven con la altura
    float throat = 0.0f;           // semitonos de corrimiento de formantes (largo del tracto vocal)
    float mix = 1.0f, outGainDb = 0.0f;
};

class Engine
{
public:
    static constexpr int kBuf = 1 << 15, kSnaps = 128, kHist = 1024, kView = 4096;

    void prepare (double sampleRate, Mode m)
    {
        fs = sampleRate; mode = m;
        mult = fs > 140000 ? 4 : (fs > 70000 ? 2 : 1);
        D0 = 32 * mult;
        L = (m == Mode::live ? 0 : (m == Mode::tight ? 96 : 992)) * mult;
        De = D0 + L;
        Nx = 24 * mult;
        lpcHop = 128 * mult; lpcWin = 1024 * mult;
        order = 48;                                                 // orden 48: resuelve F1 y F2 (prueba T7)
        det.prepare (fs);
        hopRate = det.estimateRate();
        cor.prepare (hopRate);
        res.assign (kBuf, 0.0f); wav.assign (kBuf, 0.0f); dry.assign (kBuf, 0.0f); anaRing.assign ((size_t) lpcWin * 2, 0.0f);
        snaps.assign (kSnaps, Snap());
        window.resize ((size_t) lpcWin);
        for (int i = 0; i < lpcWin; ++i) window[(size_t) i] = 0.5 - 0.5 * std::cos (2 * kPi * (i + 0.5) / lpcWin);
        hist.assign (kHist, HistEntry());
        reset();
        set (P);
    }
    void reset()
    {
        det.reset(); cor.reset(); ana.reset(); syn.reset();
        std::fill (res.begin(), res.end(), 0.0f); std::fill (wav.begin(), wav.end(), 0.0f); std::fill (dry.begin(), dry.end(), 0.0f); std::fill (anaRing.begin(), anaRing.end(), 0.0f);
        for (auto& s : snaps) s = Snap();
        for (auto& h : hist) h = HistEntry();
        n = 0; rp = -(double) De; xf = 0; rpOld = 0; histW = 0; peak = 0; unvoicedFor = 1; preX = 0; deY = 0;
        lastF0 = 0; splices = 0; minD = 1e9; maxD = 0; envIn = envOut = 0; guard = 1;
        envRel = std::exp (-1.0 / (0.05 * fs)); guardRel = 1.0 - std::exp (-1.0 / (0.02 * fs));
    }
    void set (const EngineParams& p)
    {
        P = p;
        cor.P = p.correct;
        det.setRange (voicePreset (p.voice)); det.setThreshold (p.gateDb); det.setReference (p.refA);
        if (! std::equal_to<double>() (det.estimateRate(), hopRate)) { hopRate = det.estimateRate(); cor.setRate (hopRate); }
        wetGain = std::pow (10.0f, p.outGainDb / 20.0f);
    }
    // Cambio de modo en marcha (desde el hilo de audio): sólo cambia el adelanto, sin reservar memoria. El cabezal
    // vuelve a la latencia nueva con un fundido.
    void setMode (Mode m)
    {
        if (m == mode) return;
        mode = m;
        L = (m == Mode::live ? 0 : (m == Mode::tight ? 96 : 992)) * mult;
        De = D0 + L;
        rpOld = rp; rp = std::max (0.0, (double) n - De); xf = Nx;
    }
    int latencySamples() const { return De; }
    // La latencia que tendrá un modo (para informarla antes de que el audio cambie de modo, p. ej. al restaurar).
    int latencyForMode (Mode m) const { return D0 + (m == Mode::live ? 0 : (m == Mode::tight ? 96 : 992)) * mult; }
    Mode currentMode() const { return mode; }

    // MIDI al corrector.
    void noteOn (int note) { cor.noteOn (note); }
    void noteOff (int note) { cor.noteOff (note); }
    void pitchBend (float v) { cor.pitchBend (v); }
    void keyEstimate (int& tonic, bool& minor, float& conf) const { cor.keys.best (tonic, minor, conf); }
    void resetKeys() { cor.keys.reset(); }

    // Una muestra mono.
    inline float process (float x)
    {
        // 1. Detector y corrector (a ~1,5 kHz).
        PitchEstimate est;
        if (det.push (x, est))
        {
            // La readquisición (cada 16 saltos) analiza una ventana más larga y su centro cae ~11 ms antes que el de
            // la estimación anterior: el tiempo se mantiene creciente para el corrector, el historial y la vista.
            if (histW > 0) est.time = std::max (est.time, hist[(size_t) ((histW - 1) & (kHist - 1))].time);
            const CorrectOut o = cor.update (est);
            HistEntry& h = hist[(size_t) (histW & (kHist - 1))];
            h.time = o.time; h.f0 = o.voiced ? o.hz : 0.0f; h.delta = o.delta; ++histW;
            lastOut = o;
            const int64_t vw = viewW.load (std::memory_order_relaxed);
            view[(size_t) (vw & (kView - 1))] = o; viewW.store (vw + 1, std::memory_order_release);
        }
        // 2. LPC sobre la señal con preénfasis, que es también la que recorre la cadena: el residuo queda blanco
        //    (sin la inclinación del preénfasis, que al estirarse con la altura corría el balance de los formantes)
        //    y el desénfasis exacto al final deshace el preénfasis.
        const double xpe = x - 0.9 * preX; preX = x;
        anaRing[(size_t) (n % (lpcWin * 2))] = (float) xpe;
        dry[(size_t) (n & (kBuf - 1))] = x;
        if ((n + 1) % lpcHop == 0) analyseLpc();
        // 3. Residuo con los coeficientes del instante n.
        KVec kn, cn; kAt ((double) n, kn, cn, false);
        const double e = ana.process (xpe, kn, cn, order);
        res[(size_t) (n & (kBuf - 1))] = (float) e;
        wav[(size_t) (n & (kBuf - 1))] = (float) xpe;
        peak = std::max (std::abs (e), peak * peakDecay);
        // 4. Empalmador.
        double ratio = 1.0, T = 0.01 * fs; bool voiced = false;
        lookup (rp, ratio, T, voiced);
        if (voiced) unvoicedFor = 0; else unvoicedFor += 1.0 / fs;
        schedule (ratio, T, voiced);
        // Con preservación se lee el residuo y se vuelve a pasar por la envolvente; sin preservación se lee la
        // forma de onda (con preénfasis) y los formantes se mueven con la altura, exacto y sin LPC.
        const bool preserve = P.formants;
        const std::vector<float>& src = preserve ? res : wav;
        double y = interp (src, rp);
        if (xf > 0)
        {
            const double g = 0.5 - 0.5 * std::cos (kPi * (double) (Nx - xf) / Nx);
            y = interp (src, rpOld) * (1.0 - g) + y * g;
            rpOld += ratio; --xf;
        }
        if (preserve != wasPreserving) { syn.reset(); wasPreserving = preserve; }
        double ype = y;
        if (preserve)
        {
            // 5. Síntesis con los coeficientes del instante leído (modificados si se mueven los formantes).
            KVec ks, cs; float g = 1.0f; kAt (rp, ks, cs, true, &g);
            ype = syn.process (y * g, ks, cs, order);
        }
        const double out = ype + 0.9 * deY; deY = out;
        const double dd = (double) n - rp;
        minD = std::min (minD, dd); maxD = std::max (maxD, dd);
        rp += ratio;
        if (rp > (double) n - 2) rp = (double) n - 2;                       // nunca delante de lo escrito
        // 6. Guardián de nivel: la salida no pasa de la entrada alineada más 12 dB. En uso normal no actúa; con
        //    señales patológicas (un seno puro corrido dos octavas) evita que un empalme haga sonar la envolvente.
        const float dryS = dry[(size_t) ((n - De) & (kBuf - 1))];
        envIn = std::max ((double) std::abs (dryS), envIn * envRel);
        envOut = std::max (std::abs (out), envOut * envRel);
        const double allowed = 4.0 * envIn + 1e-4;
        const double gTarget = envOut > allowed ? allowed / envOut : 1.0;
        guard = gTarget < guard ? gTarget : guard + (gTarget - guard) * guardRel;
        ++n;
        const float wet = (float) (out * guard) * wetGain;
        return dryS + (wet - dryS) * P.mix;
    }

    // Para la vista: lee las decisiones nuevas desde `from`.
    int64_t viewWrite() const { return viewW.load (std::memory_order_acquire); }
    const CorrectOut& viewAt (int64_t i) const { return view[(size_t) (i & (kView - 1))]; }
    CorrectOut last() const { return lastOut; }
    int64_t samplePos() const { return n; }
    // Para las pruebas.
    double contentDelayMin() const { return minD; }
    double contentDelayMax() const { return maxD; }
    void resetDelayStats() { minD = 1e9; maxD = 0; }
    int spliceCount() const { return splices; }
    int baseDelay() const { return De; }
    // Envolvente LPC (dB) de la instantánea más reciente en `hz` (para las pruebas).
    double envelopeDb (double hz) const
    {
        const Snap* s = nullptr; int64_t best = -1;
        for (auto& x : snaps) if (x.index > best) { best = x.index; s = &x; }
        if (s == nullptr) return 0;
        double a[kMaxOrder + 1]; stepUp (s->k, order, a);
        std::complex<double> A = 0; for (int i = 0; i <= order; ++i) A += a[i] * std::polar (1.0, -2 * kPi * hz * i / fs);
        double c = 1; for (int i = 0; i < order; ++i) c *= std::sqrt (1.0 - (double) s->k[(size_t) i] * s->k[(size_t) i]);
        return 20 * std::log10 (c / std::abs (A));
    }

private:
    struct Snap { KVec k {}, c {}; int64_t index = -1; double factor = 1.0; KVec km {}, cm {}; float gm = 1.0f; std::vector<std::complex<double>> roots; bool warm = false; int64_t computedAt = -1; };
    struct HistEntry { double time = -1; float f0 = 0, delta = 0; };

    EngineParams P;
    Detector det; Corrector cor; LatticeAnalysis ana; LatticeSynthesis syn;
    double fs = 48000, hopRate = 1500; Mode mode = Mode::tight;
    int mult = 1, D0 = 32, L = 96, De = 128, Nx = 24, lpcHop = 128, lpcWin = 1024, order = 24;
    std::vector<float> res, wav, dry, anaRing; bool wasPreserving = true; std::vector<double> window;
    std::vector<Snap> snaps; std::vector<HistEntry> hist; int64_t histW = 0;
    int64_t n = 0; double rp = 0, rpOld = 0; int xf = 0; double peak = 0, peakDecay = 0.999, unvoicedFor = 1, preX = 0, deY = 0;
    float lastF0 = 0; float wetGain = 1; double envIn = 0, envOut = 0, guard = 1, envRel = 0.9995, guardRel = 0.001; int splices = 0; double minD = 1e9, maxD = 0;
    CorrectOut lastOut;
public:
    double lastGainDb = 0;          // ganancia de predicción de la última instantánea (para las pruebas)
private:
    std::vector<CorrectOut> view = std::vector<CorrectOut> (kView); std::atomic<int64_t> viewW { 0 };

    inline float resAt (int64_t i) const { return i < 0 ? 0.0f : res[(size_t) (i & (kBuf - 1))]; }
    static inline float bufAt (const std::vector<float>& b, int64_t i) { return i < 0 ? 0.0f : b[(size_t) (i & (kBuf - 1))]; }
    inline double interp (const std::vector<float>& b, double pos) const
    {
        const double fl = std::floor (pos); const int64_t i = (int64_t) fl; const double t = pos - fl;
        const double xm1 = bufAt (b, i - 1), x0 = bufAt (b, i), x1 = bufAt (b, i + 1), x2 = bufAt (b, i + 2);
        if (t == 0.0) return x0;
        const double c1 = 0.5 * (x1 - xm1), c2 = xm1 - 2.5 * x0 + 2 * x1 - 0.5 * x2, c3 = 0.5 * (x2 - xm1) + 1.5 * (x0 - x1);
        return ((c3 * t + c2) * t + c1) * t + x0;
    }

    void analyseLpc()
    {
        // Instantánea j: coeficientes de las últimas `lpcWin` muestras, válidos desde la muestra n + 1.
        const int64_t j = (n + 1) / lpcHop;
        double r[kMaxOrder + 1] {};
        const int W = lpcWin, ring = lpcWin * 2;
        static thread_local std::vector<double> buf; buf.resize ((size_t) W);
        for (int i = 0; i < W; ++i) buf[(size_t) i] = anaRing[(size_t) (((n - W + 1 + i) % ring + ring) % ring)] * window[(size_t) i];
        for (int lag = 0; lag <= order; ++lag)
        {
            double acc = 0;
            for (int i = lag; i < W; ++i) acc += buf[(size_t) i] * buf[(size_t) (i - lag)];
            r[lag] = acc;
        }
        // Piso de ruido blanco a −40 dB y expansión de ancho de banda de 10 Hz: la envolvente conserva los formantes
        // (con más piso o más expansión quedan formantes en el residuo y se mueven con la altura, prueba T7).
        r[0] *= 1.0001; r[0] += 1e-12;
        const double lagHz = 60.0;
        for (int lag = 1; lag <= order; ++lag) r[lag] *= std::exp (-0.5 * std::pow (2 * kPi * lagHz * lag / fs, 2));   // lag window
        Snap& s = snaps[(size_t) (j % kSnaps)];
        s.index = j;
        levinson (r, order, s.k);
        // Tope de ganancia de predicción: una voz real queda por debajo; un seno puro o un tono muy limpio no,
        // y ahí un empalme haría sonar la resonancia. Se sube el piso de ruido blanco hasta que entra.
        {
            const double cap = 60.0;
            const double r0 = r[0];
            for (int it = 0; it < 8; ++it)
            {
                double e = 1; for (int i = 0; i < order; ++i) e *= 1.0 - (double) s.k[(size_t) i] * s.k[(size_t) i];
                lastGainDb = -10 * std::log10 (std::max (1e-30, e));
                if (lastGainDb <= cap) break;
                r[0] = r0 * (1.0 + 1e-3 * std::pow (4.0, it));
                levinson (r, order, s.k);
            }
        }
        {
            double a[kMaxOrder + 1]; stepUp (s.k, order, a);
            const double g = std::exp (-kPi * 10.0 / fs); double gi = 1;
            for (int i = 1; i <= order; ++i) { gi *= g; a[i] *= gi; }
            KVec kk; if (stepDown (a, order, kk)) s.k = kk;
        }
        for (int i = 0; i < kMaxOrder; ++i) s.c[(size_t) i] = (float) std::sqrt (1.0 - (double) s.k[(size_t) i] * s.k[(size_t) i]);
        s.factor = 1.0; s.km = s.k; s.cm = s.c; s.gm = 1.0f; s.warm = false; s.computedAt = -1;
    }

    // Coeficientes interpolados en el instante `t` (de la muestra t, posiblemente fraccionaria).
    void kAt (double t, KVec& k, KVec& c, bool synth, float* gain = nullptr)
    {
        const double u = t / lpcHop;
        const int64_t j = (int64_t) std::floor (u);
        const double f = u - (double) j;
        const Snap* a = snapFor (j - 1, synth);
        const Snap* b = snapFor (j, synth);
        if (a == nullptr && b == nullptr) { k.fill (0.0f); c.fill (1.0f); return; }
        if (a == nullptr) a = b;
        if (b == nullptr) b = a;
        const KVec& ka = synth ? a->km : a->k;
        const KVec& kb = synth ? b->km : b->k;
        const KVec& ca = synth ? a->cm : a->c;
        const KVec& cb = synth ? b->cm : b->c;
        for (int i = 0; i < order; ++i)
        {
            k[(size_t) i] = (float) (ka[(size_t) i] + (kb[(size_t) i] - ka[(size_t) i]) * f);
            c[(size_t) i] = (float) (ca[(size_t) i] + (cb[(size_t) i] - ca[(size_t) i]) * f);
        }
        if (gain != nullptr) *gain = synth ? (float) (a->gm + (b->gm - a->gm) * f) : 1.0f;
    }

    const Snap* snapFor (int64_t j, bool synth)
    {
        if (j < 0) return nullptr;
        Snap& s = snaps[(size_t) (j % kSnaps)];
        if (s.index != j) return nullptr;
        if (synth)
        {
            // Formantes: el factor es 2^(throat/12) · r^(1 − amount) (amount 1 = quedan fijos). Sin preservación no
            // se usa: se lee la forma de onda y los formantes se mueven con la altura.
            const double r = std::pow (std::exp2 (lastOut.delta / 12.0), 1.0 - P.formantAmount);
            const double factor = r * std::exp2 (P.throat / 12.0);
            // Como mucho una búsqueda de raíces por instantánea y por intervalo LPC (el factor cambia despacio).
            if (std::abs (factor - s.factor) > 0.002 && (s.computedAt < 0 || n - s.computedAt >= lpcHop || std::abs (factor - s.factor) > 0.03))
            {
                s.computedAt = n;
                if (std::abs (factor - 1.0) < 0.002) { s.km = s.k; s.cm = s.c; s.gm = 1.0f; }
                else
                {
                    // Arranque en caliente desde las raíces de la instantánea anterior: cambian muy poco en 2,7 ms.
                    if (! s.warm)
                    {
                        const Snap& prev = snaps[(size_t) ((j + kSnaps - 1) % kSnaps)];
                        if (prev.index == j - 1 && (int) prev.roots.size() == order) { s.roots = prev.roots; s.warm = true; }
                    }
                    scaleFormants (s.k, order, factor, s.km, s.roots, s.warm, std::exp (-3.14159265358979 * 150.0 / fs)); s.warm = true;
                    for (int i = 0; i < kMaxOrder; ++i) s.cm[(size_t) i] = (float) std::sqrt (1.0 - (double) s.km[(size_t) i] * s.km[(size_t) i]);
                    // La potencia para ruido blanco se conserva, pero la voz es armónica: se iguala la energía en los
                    // armónicos de la nota que suena (si no, mover los formantes bajaba la voz ~5 dB).
                    s.gm = harmonicMatchGain (s.k, s.km, order, lastOut.voiced ? (double) lastOut.hz : 200.0, fs, 0.9);
                }
                s.factor = factor;
            }
        }
        return &s;
    }

    // Δ y período en el instante leído, desde el historial (si es más nuevo que todo el historial, el último).
    void lookup (double t, double& ratio, double& T, bool& voiced) const
    {
        if (histW == 0) { ratio = 1; T = 0.01 * fs; voiced = false; return; }
        const HistEntry& newest = hist[(size_t) ((histW - 1) & (kHist - 1))];
        const HistEntry* e = &newest;
        if (t < newest.time)
        {
            // Búsqueda hacia atrás: las entradas están ordenadas por tiempo.
            int64_t i = histW - 1;
            while (i > 0 && i > histW - kHist + 1 && hist[(size_t) ((i - 1) & (kHist - 1))].time > t) --i;
            const HistEntry& b = hist[(size_t) (i & (kHist - 1))];
            const HistEntry& a = hist[(size_t) ((i > 0 ? i - 1 : 0) & (kHist - 1))];
            if (a.time >= 0 && b.time > a.time && t >= a.time)
            {
                const double f = (t - a.time) / (b.time - a.time);
                const double dlt = a.delta + (b.delta - a.delta) * f;
                ratio = std::exp2 (dlt / 12.0);
                const float f0 = (a.f0 > 0 && b.f0 > 0) ? (float) (a.f0 + (b.f0 - a.f0) * f) : std::max (a.f0, b.f0);
                voiced = f0 > 0; T = voiced ? fs / f0 : 0.01 * fs;
                return;
            }
            e = &b;
        }
        ratio = std::exp2 (e->delta / 12.0);
        voiced = e->f0 > 0; T = voiced ? fs / e->f0 : 0.01 * fs;
    }

    // Decide si hay que repetir o descartar un ciclo, y cuándo.
    void schedule (double ratio, double T, bool voiced)
    {
        if (xf > 0) return;
        const double d = (double) n - rp;
        const int h = 3;
        const double quiet = 0.3 * peak;
        auto quietHere = [&] { for (int i = 0; i < 4; ++i) if (std::abs (resAt ((int64_t) rp + i)) > quiet) return false; return true; };
        double jump = 0;
        if (ratio > 1.0 + 1e-6)
        {
            const double lo = De + Nx * (ratio - 1.0) + h;
            if (d < lo || (d < lo + 0.5 * T && quietHere())) jump = -T;
        }
        else if (ratio < 1.0 - 1e-6)
        {
            const double ok = De + T + Nx + h + 6;
            if (d > ok + 0.5 * T || (d > ok && quietHere())) jump = T;
        }
        else if (! voiced && unvoicedFor > 0.02 && std::abs (d - De) > 0.5)
        {
            // Sin voz: de vuelta a la latencia base, exacta (sin el ajuste por correlación, que la corría ±4
            // muestras). Antes sólo volvía si el retardo pasaba De + 35: un resto menor quedaba de una nota a la
            // siguiente y se acumulaba (QA en REAPER 2026-10-22, P2: ráfagas de 20 ms a 44,1 kHz, 0 → 33 muestras).
            const double target = (double) (n - De);
            if (target > (double) n - 2 - Nx || target < 0) return;
            rpOld = rp; rp = target; xf = Nx; ++splices;
            return;
        }
        if (std::equal_to<double>() (jump, 0.0)) return;
        // Ajuste fino por correlación (±4 muestras a 48 kHz) para que no haya salto de fase.
        const int R = 4 * mult;
        double best = -1e18; int bd = 0;
        const int64_t base = (int64_t) std::floor (rp);
        const int win = Nx;
        for (int dlt = -R; dlt <= R; ++dlt)
        {
            const int64_t j0 = base + (int64_t) std::lround (jump) + dlt;
            if (j0 + win + 2 > n || j0 - win < 0) continue;
            double num = 0, ea = 0, eb = 0;
            for (int i = -win; i < win; ++i)
            {
                const double a = resAt (base + i), b = resAt (j0 + i);
                num += a * b; ea += a * a; eb += b * b;
            }
            const double c = num / std::sqrt (ea * eb + 1e-24);
            if (c > best) { best = c; bd = dlt; }
        }
        const double target = rp + std::lround (jump) + bd;
        if (target > (double) n - 2 - Nx * ratio || target < 0) return;   // sin datos todavía: se espera
        rpOld = rp; rp = target; xf = Nx; ++splices;
    }
};

} // namespace dh::pl
