#pragma once
// El masterizador completo (docs/research.md §6.2-6.7): la cadena, su latencia y el "aprendizaje" que decide
// todos los parámetros a partir de una porción de audio (el archivo entero en el render, lo que se escuchó con
// LEARN en el plugin).
//
// Cadena: pasa altos → EQ tonal → EQ dinámica → multibanda → glue → estéreo → saturación (×4) → ganancia de
// sonoridad → recortador (×4) → limitador de pico verdadero.
//
// El aprendizaje es por etapas: decide la EQ con el análisis de la entrada, la aplica a una copia, mide lo que
// necesita la etapa siguiente sobre esa copia, y así hasta el final; la ganancia de sonoridad se itera sobre las
// dos últimas etapas hasta que la salida mide el objetivo. Como la cadena del render es exactamente la misma (y
// arranca del mismo estado), lo que el aprendizaje midió es lo que sale.

#include "Analysis.h"
#include "Targets.h"
#include "ToneEq.h"
#include "DynEq.h"
#include "Dynamics.h"
#include "Stereo.h"
#include "Saturation.h"
#include <cstdio>
#include <functional>
#include <string>

namespace dh::lq {

struct Options
{
    Style style = Style::Balanced;
    double intensity = 0.5;           // 0..1
    double targetLufs = -14.0, ceilingDb = -1.0;
    bool matchInput = false;
    bool constantLatency = false;     // el plugin: las regiones sobremuestreadas corren siempre
};

struct Plan
{
    Options opt;
    double fs = 48000.0;
    double targetLufs = -14.0, ceilingDb = -1.0;
    bool alreadyMastered = false, silent = false;
    bool dcBlock = false; double hpfHz = 0.0;
    std::array<EqBand, kEqBands> eq = eqTopology();
    double eqTrimDb = 0.0;
    BandCurve eqWant {}, eqFit {};
    DynBandParams harsh, sib;
    bool mbOn = false; double xo1 = 120.0, xo2 = 2500.0; std::array<CompParams, 3> mb {};
    bool glueOn = false; CompParams glue;
    StereoParams stereo;
    SatParams sat;
    bool clipOn = false; double clipThresholdDb = 0.0;
    double gainDb = 0.0;
    LimiterParams lim;
    // Lo que el aprendizaje midió a la salida.
    double outLufs = -70.0, outTruePeakDb = -100.0, outLra = 0.0, limiterAvgGrDb = 0.0, limiterMaxGrDb = 0.0;
    std::vector<std::string> notes;
};

class Chain
{
public:
    enum Stage { kHpf, kEq, kDyn, kMb, kGlue, kStereo, kSat, kGain, kClip, kLim, kStages };

    void prepare (double sampleRate, const Plan& p)
    {
        fs = sampleRate; P = p;
        const bool cl = p.opt.constantLatency;
        for (int c = 0; c < 2; ++c) hpf[c].set (highPass (std::max (p.hpfHz, 20.0), 0.7071, fs));
        dcK = 1.0 - 2.0 * kPiD * 5.0 / fs;
        eq.prepare (fs, p.eq, p.eqTrimDb);
        harsh.prepare (fs, p.harsh); sib.prepare (fs, p.sib);
        for (int c = 0; c < 2; ++c) xo[c].set (p.xo1, p.xo2, fs);
        for (int b = 0; b < 3; ++b) { mb[b].prepare (fs); mb[b].set (p.mb[(size_t) b]); }
        glue.prepare (fs); glue.set (p.glue);
        for (int c = 0; c < 2; ++c) glueHp[c].set (highPass (90.0, 0.7071, fs));
        stereo.prepare (fs, p.stereo);
        sat.prepare (fs, p.sat);
        runSat = p.sat.on || cl;
        gain = std::pow (10.0, p.gainDb / 20.0);
        clip.prepare (std::pow (10.0, p.clipThresholdDb / 20.0), p.clipOn);
        runClip = p.clipOn || cl;
        lim.prepare (fs, p.lim);
        reset();
    }
    void reset()
    {
        for (auto& b : hpf) b.reset();
        dcX[0] = dcX[1] = dcY[0] = dcY[1] = 0.0;
        eq.reset(); harsh.reset(); sib.reset(); for (auto& x : xo) x.reset(); for (auto& m : mb) m.reset();
        glue.reset(); for (auto& h : glueHp) h.reset(); stereo.reset(); sat.reset(); clip.reset(); lim.reset();
    }
    // Latencia de las etapas [from, to).
    int latency (int from = 0, int to = kStages) const
    {
        int l = 0;
        if (from <= kSat && kSat < to && runSat) l += Saturation::latency();
        if (from <= kClip && kClip < to && runClip) l += Clipper::latency();
        if (from <= kLim && kLim < to) l += lim.latency();
        return l;
    }

    inline void processSample (double& l, double& r, int from = 0, int to = kStages)
    {
        for (int s = from; s < to; ++s) stage (s, l, r);
    }

    void process (float* L, float* R, int n)
    {
        for (int i = 0; i < n; ++i)
        {
            double l = L[i], r = R[i];
            processSample (l, r);
            L[i] = (float) l; R[i] = (float) r;
        }
    }

    // Medidores
    double grHarsh() const { return harsh.currentGrDb(); }
    double grSib() const { return sib.currentGrDb(); }
    double grMb (int b) const { return mb[b].currentGrDb(); }
    double grGlue() const { return glue.currentGrDb(); }
    double grLimiter() const { return lim.currentGrDb(); }
    double limiterAverageGr() const { return lim.averageGrDb(); }
    double limiterMaxGr() const { return lim.maxGrDb(); }
    double mbAverageGr (int b) const { return mb[b].averageGrDb(); }
    double glueAverageGr() const { return glue.averageGrDb(); }
    long long clippedSubSamples() const { return clip.clippedSubSamples(); }
    const Plan& plan() const { return P; }
    double sampleRate() const { return fs; }

private:
    inline void stage (int s, double& l, double& r)
    {
        switch (s)
        {
            case kHpf:
                // Bloqueo de continua (1.er orden, 5 Hz: casi no gira la fase de los graves, que en material ya limitado
                // subiría los picos) y, sólo si hay retumbe, un pasa altos de 2.º orden.
                if (P.dcBlock)
                {
                    dcY[0] = l - dcX[0] + dcK * dcY[0]; dcX[0] = l; l = dcY[0];
                    dcY[1] = r - dcX[1] + dcK * dcY[1]; dcX[1] = r; r = dcY[1];
                }
                if (P.hpfHz > 0.0) { l = hpf[0].process (l); r = hpf[1].process (r); }
                break;
            case kEq: eq.process (l, r); break;
            case kDyn: harsh.process (l, r); sib.process (l, r); break;
            case kMb:
                if (P.mbOn)
                {
                    // Forma paralela: salida = entrada + Σ (g − 1)·banda. Sin reducción la señal sale intacta (el
                    // divisor LR4 suma un pasa todo, y girar la fase de un master ya limitado le sube los picos varios dB).
                    double bl[3], br[3];
                    xo[0].process (l, bl[0], bl[1], bl[2]); xo[1].process (r, br[0], br[1], br[2]);
                    for (int b = 0; b < 3; ++b)
                    {
                        const double g = mb[b].gainFor (std::max (std::fabs (bl[b]), std::fabs (br[b]))) - 1.0;
                        l += bl[b] * g; r += br[b] * g;
                    }
                }
                break;
            case kGlue:
                if (P.glueOn)
                {
                    const double d = std::max (std::fabs (glueHp[0].process (l)), std::fabs (glueHp[1].process (r)));
                    const double g = glue.gainFor (d);
                    l *= g; r *= g;
                }
                break;
            case kStereo: stereo.process (l, r); break;
            case kSat: sat.process (l, r, runSat); break;
            case kGain: l *= gain; r *= gain; break;
            case kClip: clip.process (l, r, runClip); break;
            case kLim: lim.process (l, r); break;
            default: break;
        }
    }

    double fs = 48000.0, gain = 1.0;
    bool runSat = false, runClip = false;
    Plan P;
    Biquad hpf[2];
    double dcK = 0.9993, dcX[2] {}, dcY[2] {};
    ToneEq eq;
    DynBand harsh, sib;
    Crossover3 xo[2];
    Compressor mb[3], glue;
    Biquad glueHp[2];
    Stereo stereo;
    Saturation sat;
    Clipper clip;
    TruePeakLimiter lim;
};

// ---- Aprendizaje ------------------------------------------------------------------------------------------------

namespace detail {

inline double percentile (std::vector<double> v, double p)
{
    if (v.empty()) return 0.0;
    std::sort (v.begin(), v.end());
    const size_t k = (size_t) std::lround ((double) (v.size() - 1) * std::clamp (p, 0.0, 100.0) / 100.0);
    return v[k];
}

struct Work
{
    std::vector<float> L, R;
    size_t n = 0;                      // muestras válidas (el resto es cola de ceros para absorber latencias)
    double fs = 48000.0;
};

inline double lufsOf (const Work& w, double* tp = nullptr, double* lra = nullptr)
{
    LoudnessMeter m; m.prepare (w.fs);
    for (size_t p = 0; p < w.n; p += 4096) m.process (w.L.data() + p, w.R.data() + p, (int) std::min ((size_t) 4096, w.n - p));
    if (tp) *tp = m.truePeakDb();
    if (lra) *lra = m.lra();
    return m.integrated();
}

// Corre las etapas [from, to) de `chain` sobre `w` (desde su estado inicial) y compensa la latencia.
inline void runStages (Chain& chain, Work& w, int from, int to)
{
    chain.reset();
    const int lat = chain.latency (from, to);
    const size_t total = w.L.size();
    for (size_t i = 0; i < total; ++i)
    {
        double l = w.L[i], r = w.R[i];
        chain.processSample (l, r, from, to);
        if ((long long) i - lat >= 0) { w.L[i - (size_t) lat] = (float) l; w.R[i - (size_t) lat] = (float) r; }
    }
    for (size_t i = total - (size_t) lat; i < total; ++i) { w.L[i] = 0.0f; w.R[i] = 0.0f; }
}

} // namespace detail

class Learner
{
public:
    // Decide el plan a partir de `n` muestras estéreo. `report` (opcional) recibe el análisis de la entrada.
    static Plan learn (const float* L, const float* R, size_t n, double fs, const Options& opt, Report* report = nullptr,
                       std::vector<float>* outL = nullptr, std::vector<float>* outR = nullptr,
                       const std::function<void (double)>& progress = {})
    {
        auto prog = [&] (double v) { if (progress) progress (v); };
        using namespace detail;
        Plan p; p.opt = opt; p.fs = fs;
        if (R == nullptr) R = L;
        const Report rep = Analyzer::run (L, R, n, fs);
        if (report) *report = rep;
        prog (0.15);
        p.ceilingDb = opt.ceilingDb;
        p.targetLufs = opt.matchInput ? rep.lufsI : opt.targetLufs;
        p.lim.ceilingDb = opt.ceilingDb;
        const StyleDef& S = styleDef (opt.style);
        p.lim.releaseFastMs = S.relFastMs; p.lim.releaseSlowMs = S.relSlowMs;
        if (rep.silent || n < (size_t) (fs * 0.5))
        {
            p.silent = true; p.notes.push_back ("Input is silent or too short: only the safety limiter runs.");
            return p;
        }
        const double x = std::clamp (opt.intensity, 0.0, 1.0);
        const IntensityMap im = intensityMap (x);
        const double styleScale = std::min (1.0, x / 0.5);

        // Cuánta reducción de dinámica pide el objetivo (§6.4-2).
        const double plrTarget = p.ceilingDb - p.targetLufs;
        const double need = rep.plr - plrTarget;
        double dynScale = std::clamp ((need - 2.0) / 6.0, 0.25, 1.0);
        // Ya masterizada: densa (PSR mínimo < 8 dB) y ya cerca del objetivo o más fuerte (§6.4-2).
        p.alreadyMastered = rep.minPsr < 8.0 && (rep.lufsI >= p.targetLufs - 1.0 || rep.lufsI > -12.0);
        if (p.alreadyMastered)
        {
            dynScale = 0.0;
            p.notes.push_back ("The input already sounds mastered (dense and loud): compression and clipping stay off, the EQ works at half strength and the limiter only catches peaks.");
        }
        if (rep.clippedRuns > 10) p.notes.push_back ("The input has clipped passages (" + std::to_string (rep.clippedRuns) + "). Mastering cannot undo them: export the mix with more headroom if you can.");
        if (rep.lra > 15.0) p.notes.push_back ("Very wide dynamic range (LRA " + fmt1 (rep.lra) + " LU): quiet passages will stay quiet; that is intended.");
        if (rep.bandLimitHz > 0.0) p.notes.push_back ("The input has almost nothing above " + fmt0 (rep.bandLimitHz / 1000.0) + " kHz (lossy source?): the EQ leaves that region alone.");

        Work w; w.fs = fs; w.n = n;
        const size_t pad = 8192;
        w.L.assign (n + pad, 0.0f); w.R.assign (n + pad, 0.0f);
        std::copy (L, L + n, w.L.begin()); std::copy (R, R + n, w.R.begin());

        // Máscara de partes fuertes (la del análisis; las etapas compensan su latencia, el tiempo no se corre).
        const auto& st = rep.shortTermHistory;
        const double gate = std::max (rep.lufsI - 3.0, -45.0);
        auto loudAt = [&] (size_t i) {
            if (st.empty()) return true;
            long j = (long) std::lround (((double) i / fs - 1.5) / 0.1);
            j = std::clamp (j, 0L, (long) st.size() - 1);
            return st[(size_t) j] >= gate;
        };

        Chain chain;
        auto apply = [&] (int from, int to) { chain.prepare (fs, p); runStages (chain, w, from, to); };

        // 0) Pasa altos: 20 Hz; 30 Hz si hay retumbe (lo de 20-25 Hz a menos de 6 dB de lo de 40-80 Hz).
        if (x > 0.0)
        {
            p.dcBlock = true;
            double ref = 0.0; for (int b = 3; b <= 6; ++b) ref += rep.mid[(size_t) b] / 4.0;
            // Retumbe: lo de 20 Hz cerca de los graves musicales y sin caer hacia abajo (un sub de 808 sí cae).
            if (rep.mid[0] > ref - 3.0 && rep.mid[0] >= rep.mid[1] - 1.0) { p.hpfHz = 25.0; p.notes.push_back ("Rumble below 25 Hz: a 25 Hz high-pass cleans it."); }
        }
        // 1) EQ tonal: corrección hacia la curva del estilo + color del estilo.
        {
            MatchInputs mi; mi.amount = im.eqAmount * S.eqAmount * (p.alreadyMastered ? 0.5 : 1.0); mi.clampDb = im.eqClamp;
            BandCurve conf {};
            const BandCurve match = matchCurve (rep, S.target, mi, &conf);
            const BandCurve col = styleCurve (S, styleScale, fs);
            BandCurve want {}, wts {};
            for (int b = 0; b < kBands; ++b)
            {
                want[(size_t) b] = match[(size_t) b] + col[(size_t) b];
                wts[(size_t) b] = std::max (conf[(size_t) b], 0.3);
            }
            p.eqWant = want;
            if (x > 0.0) p.eq = fitEq (want, wts, fs, im.eqClamp + 2.0);
            p.eqFit = eqResponse (p.eq, fs);
            // Neutral en sonoridad: el recorte compensa lo que la EQ cambió (medido, no estimado).
            const double before = lufsOf (w);
            apply (Chain::kHpf, Chain::kEq + 1);
            const double after = lufsOf (w);
            p.eqTrimDb = std::round ((before - after) * 1000.0) / 1000.0;
            for (size_t i = 0; i < w.L.size(); ++i) { w.L[i] *= (float) std::pow (10.0, p.eqTrimDb / 20.0); w.R[i] *= (float) std::pow (10.0, p.eqTrimDb / 20.0); }
            // (la cadena aplica el recorte dentro de la EQ; acá se aplicó aparte, mismo resultado salvo redondeo de float)
        }
        prog (0.3);
        // 2) EQ dinámica: umbrales por percentil del nivel relativo de cada banda en las partes fuertes.
        if (x > 0.0 && ! p.alreadyMastered)
        {
            // Aspereza: una resonancia marcada (≥ 3 dB sobre sus vecinas) en una zona que la EQ no está realzando
            // (en una mezcla oscura un pico en 3 kHz es una nota, no estridencia).
            const int hb = bandIndex (rep.harshHz > 0.0 ? rep.harshHz : 3000.0);
            if (rep.harshExcessDb >= 3.0 && rep.harshHz > 0.0 && p.eqFit[(size_t) hb] < 0.5)
            {
                p.harsh.on = true; p.harsh.hz = rep.harshHz; p.harsh.q = 2.0;
                p.harsh.maxCutDb = std::clamp (rep.harshExcessDb - 1.0, 1.0, 4.0) * styleScale;
                p.harsh.thresholdDb = bandPercentile (w, fs, p.harsh, loudAt, 95.0 - 15.0 * x);
                p.notes.push_back ("Harshness around " + fmt1 (rep.harshHz / 1000.0) + " kHz: a dynamic band tames it only when it sticks out.");
            }
            if (rep.sibilanceEventsPerSec >= 0.3)
            {
                double hz = 7000.0, best = -1e9;
                for (size_t i = 1; i + 1 < rep.fineHz.size(); ++i)
                {
                    if (rep.fineHz[i] < 5000.0 || rep.fineHz[i] > 10000.0) continue;
                    const double ex = rep.fineMidDb[i] - 0.5 * (rep.fineMidDb[i - 1] + rep.fineMidDb[i + 1]);
                    if (ex > best) { best = ex; hz = rep.fineHz[i]; }
                }
                p.sib.on = true; p.sib.hz = hz; p.sib.q = 1.4; p.sib.attackMs = 2.0; p.sib.releaseMs = 70.0;
                p.sib.maxCutDb = std::clamp (2.0 + rep.sibilanceEventsPerSec, 2.0, 4.0) * styleScale;
                p.sib.thresholdDb = bandPercentile (w, fs, p.sib, loudAt, 97.0 - 10.0 * x);
            }
            if (p.harsh.on || p.sib.on) apply (Chain::kDyn, Chain::kDyn + 1);
        }
        prog (0.4);
        // 3) Multibanda: umbral por percentil del nivel de cada banda, con tope de reducción; compensación = GR media.
        {
            const double maxGr = im.mbMaxGr * S.mbScale * dynScale;
            if (x > 0.0 && maxGr >= 0.3)
            {
                p.mbOn = true;
                const double ratio = std::clamp (1.3 + 1.0 * x * S.mbScale, 1.2, 2.5);
                const double att[3] = { 30.0 * (opt.style == Style::Punchy ? 1.5 : 1.0), 15.0, 5.0 };
                double rel[3] = { 200.0, 150.0, 100.0 };
                if (rep.onsetsPerSec > 4.0) for (auto& v : rel) v *= 0.75;
                std::array<std::vector<double>, 3> lev;
                {
                    Crossover3 xl, xr; xl.set (p.xo1, p.xo2, fs); xr.set (p.xo1, p.xo2, fs);
                    std::array<double, 3> rms {}; const double aR = std::exp (-1.0 / (0.005 * fs));
                    for (size_t i = 0; i < w.n; ++i)
                    {
                        double bl[3], br[3];
                        xl.process (w.L[i], bl[0], bl[1], bl[2]); xr.process (w.R[i], br[0], br[1], br[2]);
                        for (int b = 0; b < 3; ++b)
                        {
                            const double d = std::max (std::fabs (bl[b]), std::fabs (br[b]));
                            rms[(size_t) b] = aR * rms[(size_t) b] + (1.0 - aR) * d * d;
                            if ((i & 31) == 0 && loudAt (i)) lev[(size_t) b].push_back (toDb (std::sqrt (2.0 * rms[(size_t) b])));
                        }
                    }
                }
                for (int b = 0; b < 3; ++b)
                {
                    const double T0 = percentile (lev[(size_t) b], 90.0 - 20.0 * x), p99 = percentile (lev[(size_t) b], 99.0);
                    const double T = std::max (T0, p99 - maxGr / (1.0 - 1.0 / ratio));
                    p.mb[(size_t) b] = { std::round (T * 100.0) / 100.0, ratio, 6.0, att[b], rel[b], 5.0, 0.0 };
                }
                // GR media real en las partes fuertes → compensación por banda (mantiene el balance tonal).
                std::array<double, 3> grSum {}; size_t cnt = 0;
                {
                    Chain c3; c3.prepare (fs, p);
                    for (size_t i = 0; i < w.n; ++i)
                    {
                        double l = w.L[i], r = w.R[i];
                        c3.processSample (l, r, Chain::kMb, Chain::kMb + 1);
                        if (loudAt (i)) { for (int b = 0; b < 3; ++b) grSum[(size_t) b] += c3.grMb (b); ++cnt; }
                    }
                }
                for (int b = 0; b < 3; ++b) p.mb[(size_t) b].makeupDb = cnt ? std::round (grSum[(size_t) b] / (double) cnt * 100.0) / 100.0 : 0.0;
                apply (Chain::kMb, Chain::kMb + 1);
            }
        }
        prog (0.5);
        // 4) Glue: umbral tal que la GR media estática en las partes fuertes sea la buscada; compensación = GR media.
        {
            const double grWant = S.glueGrDb * im.glueGr * dynScale;
            if (x > 0.0 && grWant >= 0.2)
            {
                p.glueOn = true;
                const double ratio = 1.0 + (S.glueRatio - 1.0) * im.glueRatioFrac;
                double attack = S.glueAttackMs, release = S.glueReleaseMs;
                if (rep.onsetsPerSec > 4.0) release *= 0.8;
                std::vector<double> lev;
                {
                    Biquad hl, hr; hl.set (highPass (90.0, 0.7071, fs)); hr.set (highPass (90.0, 0.7071, fs));
                    double rms = 0.0; const double aR = std::exp (-1.0 / (0.010 * fs));
                    for (size_t i = 0; i < w.n; ++i)
                    {
                        const double d = std::max (std::fabs (hl.process (w.L[i])), std::fabs (hr.process (w.R[i])));
                        rms = aR * rms + (1.0 - aR) * d * d;
                        if ((i & 31) == 0 && loudAt (i)) lev.push_back (toDb (std::sqrt (2.0 * rms)));
                    }
                }
                auto avgGr = [&] (double T) { double s = 0.0; for (double v : lev) s += v - Compressor::staticCurve (v, T, ratio, 8.0); return lev.empty() ? 0.0 : s / (double) lev.size(); };
                double lo = -60.0, hi = 0.0;
                for (int it = 0; it < 40; ++it) { const double mid = 0.5 * (lo + hi); (avgGr (mid) > grWant ? lo : hi) = mid; }
                p.glue = { std::round (0.5 * (lo + hi) * 100.0) / 100.0, ratio, 8.0, attack, release, 10.0, 0.0 };
                double grSum = 0.0; size_t cnt = 0;
                {
                    Chain c3; c3.prepare (fs, p);
                    for (size_t i = 0; i < w.n; ++i)
                    {
                        double l = w.L[i], r = w.R[i];
                        c3.processSample (l, r, Chain::kGlue, Chain::kGlue + 1);
                        if (loudAt (i)) { grSum += c3.grGlue(); ++cnt; }
                    }
                }
                p.glue.makeupDb = cnt ? std::round (grSum / (double) cnt * 100.0) / 100.0 : 0.0;
                apply (Chain::kGlue, Chain::kGlue + 1);
            }
        }
        prog (0.6);
        // 5) Estéreo.
        if (x > 0.0)
        {
            StereoParams sp; sp.on = true;
            if (x >= 0.25 || rep.lowCorrelation < 0.8) sp.monoHz = S.monoHz;
            double smr = 0.0; int nb = 0;
            for (int b = 0; b < kBands; ++b) if (bandCentres()[(size_t) b] >= 200.0 && bandCentres()[(size_t) b] <= 8000.0) { smr += rep.sideMinusMid[(size_t) b]; ++nb; }
            smr /= std::max (1, nb);
            const double target = -9.0 + S.widthDb;
            double d = target - smr;
            d = std::fabs (d) < 2.0 ? 0.0 : (d > 0 ? d - 2.0 : d + 2.0);
            d = std::clamp (d, -3.0, 1.5);
            if (d > 0.0 && rep.correlation < 0.3) d = 0.0;          // no abrir lo que ya está flojo en mono
            sp.sideGainDb = std::round (d * im.widthFrac * 100.0) / 100.0;
            sp.sideHighShelfDb = S.sideHighShelfDb * styleScale;
            if (rep.correlation < 0.0) p.notes.push_back ("The input is out of phase between left and right (correlation " + fmt1 (rep.correlation) + "): check the mix in mono.");
            p.stereo = sp;
            apply (Chain::kStereo, Chain::kStereo + 1);
        }
        prog (0.65);
        // 6) Saturación.
        {
            const double drive = S.satDriveDb * im.satFrac;
            if (x > 0.0 && drive >= 0.05 && ! p.alreadyMastered)
            {
                p.sat.on = true; p.sat.flavour = S.satFlavour; p.sat.driveDb = std::round (drive * 100.0) / 100.0;
                p.sat.inputLufs = std::round (lufsOf (w) * 100.0) / 100.0;
                apply (Chain::kSat, Chain::kSat + 1);
            }
            else if (opt.constantLatency) apply (Chain::kSat, Chain::kSat + 1);     // la región corre igual (latencia fija)
        }
        prog (0.7);
        // 7-9) Sonoridad: ganancia + recortador + limitador, iterado sobre la salida real.
        const double pre = lufsOf (w);
        p.gainDb = p.targetLufs - pre;
        // Picos por bloques de 50 ms (en dBFS, antes de la ganancia), para el recortador.
        std::vector<double> blockPk;
        {
            const size_t B = (size_t) (0.05 * fs);
            for (size_t s = 0; s + B <= w.n; s += B)
            {
                if (! loudAt (s + B / 2)) continue;
                double m = 0.0; for (size_t i = s; i < s + B; ++i) m = std::max (m, (double) std::max (std::fabs (w.L[i]), std::fabs (w.R[i])));
                blockPk.push_back (toDb (m));
            }
        }
        const double pk95 = percentile (blockPk, 95.0);
        // Reparto del pico que hay que sacar: el limitador solo hasta 4 dB (6 en objetivos de club); del resto, hasta
        // 3 dB van al recortador (más en Punchy y Loud, menos en Gentle), que en transitorios suena mejor que hundir
        // la ganancia. Se mide sobre la señal ya comprimida, no sobre la entrada.
        double tpNow = 0.0; lufsOf (w, &tpNow);
        const double needNow = (tpNow - pre) - plrTarget;
        const double limBudget = p.targetLufs > -10.0 ? 6.0 : 4.0;
        const double clipFactor = opt.style == Style::Gentle ? 0.4 : (opt.style == Style::Punchy || opt.style == Style::Loud) ? 1.0 : 0.7;
        double clipDb = x > 0.0 && ! p.alreadyMastered
            ? S.clipDb * styleScale + clipFactor * std::clamp (needNow - limBudget, 0.0, 3.0) * std::min (1.0, x / 0.25) : 0.0;
        // Si el limitador no alcanza (un limitador sólo baja la cresta de ventanas del orden de su liberación), el
        // recortador sube de a 1.5 dB: hasta 6 dB en objetivos fuertes (más de −12 LUFS), 3 dB en el resto.
        const double clipCap = x > 0.0 && ! p.alreadyMastered ? (p.targetLufs > -12.0 ? 6.0 : 3.0) : 0.0;
        Work base = w;
        // El recortador se decide una vez por nivel de recorte (con la ganancia inicial) y no cambia durante la
        // búsqueda: si se prendiera y apagara según la ganancia, la sonoridad saltaría y la búsqueda oscilaría.
        bool clipDecided = false, clipUse = false;
        auto evalGain = [&] (double g, Work& t) {
            p.gainDb = std::round (g * 1000.0) / 1000.0;
            const double excess = pk95 + p.gainDb - p.ceilingDb;
            if (! clipDecided) { clipUse = clipDb >= 0.3 && excess > 0.5; clipDecided = true; }
            p.clipOn = clipUse;
            p.clipThresholdDb = p.clipOn ? std::round ((p.ceilingDb + std::max (0.5, excess - clipDb)) * 100.0) / 100.0 : 0.0;
            t = base;
            chain.prepare (fs, p);
            runStages (chain, t, Chain::kGain, Chain::kStages);
            double tp = 0.0, lra = 0.0;
            const double got = lufsOf (t, &tp, &lra);
            p.outLufs = got; p.outTruePeakDb = tp; p.outLra = lra;
            p.limiterAvgGrDb = chain.limiterAverageGr(); p.limiterMaxGrDb = chain.limiterMaxGr();
            return got;
        };
        // Búsqueda de la ganancia: secante con la pendiente medida (la limitación la vuelve sublineal), y regula falsi
        // en cuanto hay un punto de cada lado del objetivo.
        {
            Work t;
            double g = p.targetLufs - pre;
            for (;;)
            {
                double gPrev = 0.0, fPrev = 0.0, gLo = 0.0, fLo = 0.0, gHi = 0.0, fHi = 0.0, f = 0.0;
                bool hasLo = false, hasHi = false, stuck = false;
                for (int it = 0; it < 10; ++it)
                {
                    prog (std::min (0.98, 0.7 + 0.03 * (double) it));
                    f = evalGain (g, t);
                    if (std::getenv ("LQ_DEBUG"))          // traza de la búsqueda (stderr), para depurar
                        std::fprintf (stderr, "  it %d: gain %.3f clip %.2f (on %d thr %.2f) -> %.3f LUFS, lim %.2f dB\n", it, p.gainDb, clipDb, (int) p.clipOn, p.clipThresholdDb, f, p.limiterAvgGrDb);
                    const double err = p.targetLufs - f;
                    if (std::fabs (err) <= 0.05 || it == 9) break;
                    if (f < p.targetLufs) { gLo = g; fLo = f; hasLo = true; } else { gHi = g; fHi = f; hasHi = true; }
                    if (p.limiterAvgGrDb > 8.0 && err > 0.0) { stuck = true; break; }
                    double gn;
                    if (hasLo && hasHi) gn = gLo + (p.targetLufs - fLo) * (gHi - gLo) / std::max (1e-6, fHi - fLo);
                    else if (it > 0) { const double slope = std::clamp ((f - fPrev) / (g - gPrev), 0.1, 1.0); gn = g + err / slope; }
                    else gn = g + err / 0.8;
                    gPrev = g; fPrev = f;
                    g = std::clamp (gn, g - 12.0, g + 12.0);
                }
                if (! stuck) break;
                if (clipDb < clipCap - 1e-9) { clipDb = std::min (clipCap, clipDb + 1.5); clipDecided = false; g = std::max (p.targetLufs - pre, g - 6.0); continue; }
                // Sin más recorte posible: se queda en la ganancia con ~8 dB de limitación.
                p.notes.push_back ("This mix cannot reach " + fmt1 (p.targetLufs) + " LUFS without wrecking it: it stops at " + fmt1 (f) + " LUFS.");
                break;
            }
            if (outL) { *outL = std::move (t.L); outL->resize (n); }
            if (outR) { *outR = std::move (t.R); outR->resize (n); }
        }
        if (p.limiterAvgGrDb > 4.0) p.notes.push_back ("Reaching " + fmt1 (p.targetLufs) + " LUFS takes heavy limiting (" + fmt1 (p.limiterAvgGrDb) + " dB on average): a quieter target will sound more open.");
        return p;
    }

private:
    static std::string fmt1 (double v) { char b[32]; std::snprintf (b, sizeof b, "%.1f", v); return b; }
    static std::string fmt0 (double v) { char b[32]; std::snprintf (b, sizeof b, "%.0f", v); return b; }

    template <typename Mask>
    static double bandPercentile (const detail::Work& w, double fs, const DynBandParams& bp, Mask&& loudAt, double pct)
    {
        DynBand d; DynBandParams q = bp; q.on = true; d.prepare (fs, q);
        std::vector<double> v;
        for (size_t i = 0; i < w.n; ++i)
        {
            const double rel = d.relLevel (0.5 * ((double) w.L[i] + (double) w.R[i]));
            if ((i & 15) == 0 && loudAt (i)) v.push_back (rel);
        }
        return std::round (detail::percentile (v, pct) * 100.0) / 100.0;
    }
};

} // namespace dh::lq
