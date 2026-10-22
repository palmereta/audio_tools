#pragma once
// Motor de Murmur: granular en tiempo real. La entrada se graba en un búfer circular (hasta 16 s); cada grano
// lee un tramo con interpolación cúbica, a su altura, en un sentido, con su envolvente y su paneo. Se suman,
// pasan por el tono (pasa altos y pasa bajos), un delay y una reverb, y se mezclan con la señal seca.
// Freeze deja de grabar (el búfer queda fijo); feedback devuelve los granos al búfer (shimmer, nubes que
// crecen). Sin dependencias de JUCE: lo usan el plugin, murmurrender y las pruebas.

#include "Common.h"
#include "Fx.h"
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

namespace dh::mm {

constexpr int kMaxGrains = 128;
constexpr float kMaxBufferSec = 16.0f;
constexpr int kPeakBins = 512;           // forma de onda del búfer para la vista (picos por tramo)
constexpr int kSub = 32;                 // muestras por tramo de modulación

enum Quant { QOff, QSemitones, QOctaves, QFifths, QMajor, QMinor, QPentatonic, QWholeTone, QuantCount };
enum LfoShape { LSine, LTriangle, LRampDown, LDrift, LStep, LfoShapeCount };
enum Target { TNone, TPosition, TSpray, TSize, TDensity, TPitch, TPan, TTone, TMix, TFeedback, TargetCount };
enum MidiMode { MidiOff, MidiPitch, MidiGate };

inline const char* quantName (int q)
{
    static const char* n[] = { "Free", "Semitones", "Octaves", "Fifths", "Major", "Minor", "Pentatonic", "Whole tone" };
    return n[std::clamp (q, 0, (int) QuantCount - 1)];
}
inline const char* targetName (int t)
{
    static const char* n[] = { "Off", "Position", "Spray", "Size", "Density", "Pitch", "Pan", "Tone", "Mix", "Feedback" };
    return n[std::clamp (t, 0, (int) TargetCount - 1)];
}

// Divisiones de tiempo (en negras) para la sincronía de los granos, los LFO y el delay.
inline double divisionBeats (int d)
{
    static const double b[] = { 0.0, 4.0, 2.0, 1.0, 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0, 0.75, 0.375 };
    return b[std::clamp (d, 0, 10)];
}
inline const char* divisionChoices() { return "Off|1 bar|1/2|1/4|1/8|1/16|1/32|1/4 T|1/8 T|1/4 D|1/8 D"; }

struct Lfo
{
    float rateHz = 0.2f, amount = 0.0f; int sync = 0, shape = LSine, target = TNone;
};

struct Params
{
    float mix = 0.5f, inGainDb = 0.0f, outGainDb = 0.0f;
    float bufferSec = 8.0f, feedback = 0.0f; bool freeze = false;
    float sizeMs = 120.0f, sizeRand = 0.2f;
    float density = 16.0f, densityRand = 0.5f;           // granos por segundo; 0 = parejo, 1 = al azar (Poisson)
    int sync = 0; float chance = 1.0f;                   // sincronía a la grilla y probabilidad por paso
    float position = 0.3f, spray = 0.2f, scan = 0.0f;    // segundos detrás de la grabación; scan en "veces tiempo real"
    float pitch = 0.0f, fine = 0.0f, pitchRand = 0.0f; int quant = QOff;
    float shimmer = 0.0f, subOctave = 0.0f, reverse = 0.0f;
    float shape = 0.6f, skew = 0.0f;                     // forma: 0 casi rectangular … 1 campana; skew: −1 golpe … +1 crecida
    float pan = 0.5f, width = 1.0f;
    float lowCut = 20.0f, highCut = 20000.0f;
    std::array<Lfo, 2> lfo {};
    int delaySync = 0; float delaySec = 0.35f, delayFb = 0.35f, delayMix = 0.0f;
    float revSize = 0.6f, revDamp = 0.4f, revMix = 0.0f;
    float duck = 0.0f;
    int midiMode = MidiOff, voices = 64;
};

struct HostInfo { double bpm = 120.0, ppq = 0.0; bool playing = false; };

// Lo que dibuja la interfaz (se copia sin bloqueo; un cuadro con datos mezclados no importa).
struct GrainView { float age = 0, pos = 0, pitch = 0, level = 0, pan = 0; bool active = false, reverse = false; };

class Engine
{
public:
    Params p;

    void prepare (double sampleRate, int maxBlock)
    {
        // La memoria (16 s) se conserva si la frecuencia no cambió: muchos hosts vuelven a llamar a prepareToPlay
        // (otro tamaño de bloque, un render) y una textura congelada se perdía.
        const bool keep = std::abs ((float) sampleRate - sr) < 0.5f && (int) recL.size() == (int) (kMaxBufferSec * (float) sampleRate) + 8;
        sr = (float) sampleRate;
        bufN = (int) (kMaxBufferSec * sr) + 8;
        if (! keep) { recL.assign ((size_t) bufN, 0.0f); recR.assign ((size_t) bufN, 0.0f); w = 0; written = 0; }
        for (auto& g : grains) g.active = false;
        wetL.assign ((size_t) std::max (kSub, maxBlock) + kSub, 0.0f); wetR = wetL; dryL = wetL; dryR = wetL; mixT = wetL; duckT = wetL;
        subPos = 0;
        duckAtt = 1.0f - std::exp (-1.0f / (0.005f * sr)); duckRel = 1.0f - std::exp (-1.0f / (0.3f * sr));
        dly.prepare (sr); rev.prepare (sr);
        fbDcL.setCutoff (25.0f, sr); fbDcR.setCutoff (25.0f, sr);
        hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset();
        duckEnv = 0.0f; nextGrain = 0.0; scanSec = 0.0; freePpq = 0.0; lastDiv = -1;
        for (auto& l : lfoState) { l.phase = 0.0; l.held = 0.0f; l.from = 0.0f; l.to = rng.bipolar(); }
        mixS.setTime (0.03f, sr); mixS.reset (p.mix);
        inS.setTime (0.03f, sr); inS.reset (dbToGain (p.inGainDb));
        outS.setTime (0.03f, sr); outS.reset (dbToGain (p.outGainDb));
        for (auto& pk : peaks) pk.store (0.0f, std::memory_order_relaxed);
        for (int i = 0; i < kWin; ++i) win[(size_t) i] = 0.5f - 0.5f * std::cos (kPi * (float) i / (float) (kWin - 1));
        heldCount = 0;
    }

    // MIDI (lo llama el procesador entre tramos, en la muestra del evento).
    void noteOn (int note, float vel)
    {
        noteOff (note);
        if (heldCount < 16) { held[(size_t) heldCount] = { note, vel }; ++heldCount; }
        if (p.midiMode == MidiGate && p.sync == 0) nextGrain = 0.0;      // compuerta: el primer grano sale ya
    }
    void noteOff (int note)
    {
        int k = 0;
        for (int i = 0; i < heldCount; ++i) if (held[(size_t) i].note != note) held[(size_t) k++] = held[(size_t) i];
        heldCount = k;
    }
    void allNotesOff() { heldCount = 0; }
    void setFreezeHold (bool on) { freezeHold = on; }       // pedal CC64: freeze mientras está apretado

    // Procesa n muestras. inL/inR pueden ser los mismos buffers que outL/outR.
    void process (const float* inL, const float* inR, float* outL, float* outR, int n, const HostInfo& host)
    {
        if ((int) wetL.size() < n) { for (auto* v : { &wetL, &wetR, &dryL, &dryR, &mixT, &duckT }) v->resize ((size_t) n); }
        const bool frozen = p.freeze || freezeHold;
        const double ppqPerSample = std::max (1.0, host.bpm) / 60.0 / sr;
        double ppq = host.playing ? host.ppq : freePpq;
        if (! host.playing) freePpq += n * ppqPerSample; else freePpq = host.ppq + n * ppqPerSample;
        const float inT = dbToGain (p.inGainDb), outT = dbToGain (p.outGainDb);
        int active = 0;

        // Tramos de modulación de kSub muestras contados desde el principio, no desde cada llamada: un tramo puede
        // quedar repartido entre dos bloques (o entre dos eventos MIDI) y la salida no depende del tamaño de bloque
        // del host (QA en REAPER 2026-10-22: el AU a 64 muestras no nulaba contra 512).
        for (int s0 = 0; s0 < n;)
        {
            if (subPos == 0)
            {
                segMod = modulate (host.bpm, (float) kSub / sr);
                // Tono (se recalcula por tramo; los coeficientes son baratos).
                const float lo = clampf (segMod.lowCut, 10.0f, 0.45f * sr), hi = clampf (segMod.highCut, lo * 1.05f, 0.45f * sr);
                hpL.setHp (lo, sr); hpR.coef = hpL.coef; lpL.setLp (hi, sr); lpR.coef = lpL.coef;
            }
            const int m = std::min (kSub - subPos, n - s0);
            const double interval = sr / std::max (0.05f, segMod.density);
            const double bufSamples = clampf (p.bufferSec, 0.5f, kMaxBufferSec) * sr;

            for (int i = 0; i < m; ++i)
            {
                const int k = s0 + i;
                const float g = inS.process (inT);
                const float xl = sanitize (inL[k]) * g, xr = sanitize (inR[k]) * g;
                dryL[(size_t) k] = xl; dryR[(size_t) k] = xr;

                // ¿Nace un grano?
                bool spawn = false;
                if (p.sync > 0)
                {
                    const double div = divisionBeats (p.sync);
                    const long long d = (long long) std::floor ((ppq + i * ppqPerSample) / div);
                    if (d != lastDiv) { spawn = lastDiv >= 0 && rng.unipolar() < p.chance; lastDiv = d; }
                }
                else if ((nextGrain -= 1.0) <= 0.0)
                {
                    spawn = true;
                    // Entre parejo (0) y Poisson (1): el intervalo se mezcla con uno exponencial.
                    const double expo = -std::log (std::max (1.0e-6f, rng.unipolar()));
                    nextGrain += interval * ((1.0 - p.densityRand) + p.densityRand * expo);
                }
                if (spawn && (p.midiMode != MidiGate || heldCount > 0)) startGrain (segMod, frozen, bufSamples);

                // Granos.
                float sl = 0.0f, sr_ = 0.0f;
                for (auto& gr : grains)
                {
                    if (! gr.active) continue;
                    const double rp = gr.pos;
                    const float vl = readCubic (recL, rp), vr = readCubic (recR, rp);
                    const float x = (float) gr.age / (float) gr.len;
                    const float e = envelope (x, gr.peak, gr.taper) * gr.gain;
                    const float mid = 0.5f * (vl + vr), side = 0.5f * (vl - vr) * gr.width;
                    sl += (mid + side) * gr.gl * e; sr_ += (mid - side) * gr.gr * e;
                    gr.pos += gr.inc;
                    if (gr.pos >= bufN) gr.pos -= bufN; else if (gr.pos < 0.0) gr.pos += bufN;
                    gr.level = e;
                    if (++gr.age >= gr.len) gr.active = false;
                }
                // Tono.
                sl = lpL.process (hpL.process (sl)); sr_ = lpR.process (hpR.process (sr_));
                wetL[(size_t) k] = sl; wetR[(size_t) k] = sr_;

                // Grabación (con el feedback de los granos).
                if (! frozen)
                {
                    const float fb = clampf (segMod.feedback, 0.0f, 0.97f);
                    recL[(size_t) w] = xl + fb * fastTanh (fbDcL.process (sl));
                    recR[(size_t) w] = xr + fb * fastTanh (fbDcR.process (sr_));
                    peakAcc = std::max (peakAcc, std::max (std::fabs (recL[(size_t) w]), std::fabs (recR[(size_t) w])));
                    if (++peakCount >= bufN / kPeakBins) { peaks[(size_t) ((w / std::max (1, bufN / kPeakBins)) % kPeakBins)].store (peakAcc, std::memory_order_relaxed); peakAcc = 0.0f; peakCount = 0; }
                    if (++w >= bufN) w = 0;
                    ++written;
                }
                // Seguidor de la entrada para el ducking.
                const float lvl = std::max (std::fabs (xl), std::fabs (xr));
                duckEnv += (lvl - duckEnv) * (lvl > duckEnv ? duckAtt : duckRel);
                mixT[(size_t) k] = segMod.mix;
                duckT[(size_t) k] = 1.0f - clampf (p.duck, 0.0f, 1.0f) * clampf (duckEnv * 4.0f, 0.0f, 1.0f);
            }
            ppq += m * ppqPerSample;
            s0 += m; subPos += m;
            if (subPos >= kSub)
            {
                subPos = 0;
                scanSec += (double) p.scan * kSub / sr;
                if (scanSec > p.bufferSec || scanSec < -p.bufferSec) scanSec = std::fmod (scanSec, (double) p.bufferSec);
            }
        }

        // Delay y reverb sobre los granos.
        const double dSec = p.delaySync > 0 ? divisionBeats (p.delaySync) * 60.0 / std::max (1.0, host.bpm) : (double) p.delaySec;
        // Con la mezcla en 0 el efecto no corre; al volver a subirla arranca limpio (si no, sonaba la cola vieja).
        if (p.delayMix <= 0.001f) dlyIdle = true; else if (dlyIdle) { dly.reset(); dlyIdle = false; }
        if (p.revMix <= 0.001f) revIdle = true; else if (revIdle) { rev.reset(); revIdle = false; }
        if (p.delayMix > 0.001f) dly.process (wetL.data(), wetR.data(), n, (float) std::min (4.0, dSec), clampf (p.delayFb, 0.0f, 0.95f), 0.5f, p.delayMix);
        if (p.revMix > 0.001f) rev.process (wetL.data(), wetR.data(), n, p.revSize, p.revDamp, p.revMix);

        // Mezcla, ducking y salida.
        for (int k = 0; k < n; ++k)
        {
            const float mx = mixS.process (clampf (mixT[(size_t) k], 0.0f, 1.0f));
            const float duckG = duckT[(size_t) k];
            const float og = outS.process (outT);
            const float dl = dryL[(size_t) k], dr = dryR[(size_t) k];
            outL[k] = (dl * (1.0f - mx) + wetL[(size_t) k] * mx * duckG) * og;
            outR[k] = (dr * (1.0f - mx) + wetR[(size_t) k] * mx * duckG) * og;
        }

        // Guardia: si algo llegó a NaN/Inf (no debería), silencio y estados limpios.
        float sum = 0.0f;
        for (int k = 0; k < n; ++k) sum += outL[k] + outR[k];
        if (! isFiniteF (sum))
        {
            std::fill (outL, outL + n, 0.0f); std::fill (outR, outR + n, 0.0f);
            std::fill (recL.begin(), recL.end(), 0.0f); std::fill (recR.begin(), recR.end(), 0.0f);
            for (auto& gr : grains) gr.active = false;
            dly.reset(); rev.reset(); hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset(); fbDcL.reset(); fbDcR.reset();
        }
        for (auto& gr : grains) active += gr.active ? 1 : 0;
        activeGrains.store (active, std::memory_order_relaxed);
        publishView();
    }

    // Para la interfaz.
    std::array<GrainView, kMaxGrains> view {};
    std::atomic<int> activeGrains { 0 }, viewSerial { 0 };
    std::array<std::atomic<float>, kPeakBins> peaks {};
    float writeFraction() const { return (float) w / (float) bufN; }      // dónde está la cabeza de grabación (0..1 del búfer físico)
    float bufferFraction() const { return clampf (p.bufferSec, 0.5f, kMaxBufferSec) / kMaxBufferSec; }
    int heldNotes() const { return heldCount; }
    bool frozenNow() const { return p.freeze || freezeHold; }      // también con el pedal

    // La memoria en orden (de la muestra más vieja a la más nueva), para guardarla con la sesión.
    int memorySamples() const { return (int) std::min<long long> (written, (long long) bufN); }
    void copyMemory (float* l, float* r, int count) const
    {
        int s = w - count; while (s < 0) s += bufN;
        for (int i = 0; i < count; ++i) { l[i] = recL[(size_t) s]; r[i] = recR[(size_t) s]; if (++s >= bufN) s = 0; }
    }
    // Carga una memoria guardada (16 bits intercalados a la frecuencia `rate`), remuestreada a la actual.
    void setMemory (const int16_t* lr, int count, double rate)
    {
        const double step = rate / (double) sr;
        const int n = std::min (bufN, (int) ((double) count / step));
        for (int i = 0; i < n; ++i)
        {
            const double x = i * step; const int a = std::min (count - 1, (int) x), b = std::min (count - 1, a + 1); const float f = (float) (x - a);
            recL[(size_t) i] = ((float) lr[2 * a] + f * (float) (lr[2 * b] - lr[2 * a])) / 32767.0f;
            recR[(size_t) i] = ((float) lr[2 * a + 1] + f * (float) (lr[2 * b + 1] - lr[2 * a + 1])) / 32767.0f;
        }
        for (int i = n; i < bufN; ++i) { recL[(size_t) i] = 0.0f; recR[(size_t) i] = 0.0f; }
        w = n % bufN; written = n;
        const int bin = std::max (1, bufN / kPeakBins);
        for (int k = 0; k < kPeakBins; ++k)
        {
            float pk = 0.0f;
            for (int i = k * bin; i < std::min (bufN, (k + 1) * bin); ++i) pk = std::max (pk, std::max (std::fabs (recL[(size_t) i]), std::fabs (recR[(size_t) i])));
            peaks[(size_t) k].store (pk, std::memory_order_relaxed);
        }
    }

private:
    struct Grain
    {
        bool active = false, reverse = false;
        double pos = 0.0, inc = 1.0;
        int age = 0, len = 1;
        float gain = 1.0f, gl = 0.7f, gr = 0.7f, width = 1.0f, peak = 0.5f, taper = 1.0f, level = 0.0f, pitch = 0.0f, panV = 0.0f;
    };
    struct Mod { float position, spray, sizeMs, density, pitch, pan, lowCut, highCut, mix, feedback; };
    struct LfoState { double phase = 0.0; float held = 0.0f, from = 0.0f, to = 0.0f; };

    // Filtros de un polo TPT para el tono (pasa altos y pasa bajos, 6 dB/oct cada uno, suaves y estables).
    struct OnePole
    {
        float coef = 0.5f, s = 0.0f;
        void setLp (float fc, float fs) { const float g = prewarp (fc, fs); coef = g / (1.0f + g); }
        void setHp (float fc, float fs) { setLp (fc, fs); }
        inline float process (float x) { const float v = (x - s) * coef; const float y = v + s; s = y + v; return y; }
        void reset() { s = 0.0f; }
    };
    struct OnePoleHp : OnePole { inline float process (float x) { return x - OnePole::process (x); } };

    static float dbToGain (float db) { return std::pow (10.0f, db / 20.0f); }
    static float sanitize (float x) { return isFiniteF (x) ? x : 0.0f; }

    inline float readCubic (const std::vector<float>& b, double pos) const
    {
        const int i1 = (int) pos;
        const float t = (float) (pos - i1);
        const int i0 = i1 > 0 ? i1 - 1 : bufN - 1, i2 = i1 + 1 < bufN ? i1 + 1 : 0, i3 = i2 + 1 < bufN ? i2 + 1 : 0;
        const float y0 = b[(size_t) i0], y1 = b[(size_t) i1], y2 = b[(size_t) i2], y3 = b[(size_t) i3];
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }

    // Envolvente: sube hasta peak (0..1 del grano) y baja; taper es la fracción de cada lado que se redondea
    // con medio coseno (1 = campana completa, chico = casi rectangular con bordes cortos sin clic).
    inline float envelope (float x, float peak, float taper) const
    {
        const float f = x < peak ? x / std::max (1.0e-4f, peak) : (1.0f - x) / std::max (1.0e-4f, 1.0f - peak);
        if (f >= taper) return 1.0f;
        const float u = f / taper;
        return win[(size_t) std::clamp ((int) (u * (float) (kWin - 1)), 0, kWin - 1)];
    }

    Mod modulate (double bpm, float dt)
    {
        Mod md { p.position, p.spray, p.sizeMs, p.density, p.pitch, p.pan, p.lowCut, p.highCut, p.mix, p.feedback };
        for (int i = 0; i < 2; ++i)
        {
            const auto& L = p.lfo[(size_t) i];
            auto& S = lfoState[(size_t) i];
            const double hz = L.sync > 0 ? std::max (1.0, bpm) / 60.0 / divisionBeats (L.sync) : (double) L.rateHz;
            const double prev = S.phase;
            S.phase += hz * dt;
            if (S.phase >= 1.0)
            {
                S.phase -= std::floor (S.phase);
                S.held = rng.bipolar(); S.from = S.to; S.to = rng.bipolar();
            }
            (void) prev;
            const float ph = (float) S.phase;
            float v;
            switch (L.shape)
            {
                case LTriangle: v = 1.0f - 4.0f * std::fabs (ph - 0.5f); break;
                case LRampDown: v = 1.0f - 2.0f * ph; break;
                case LDrift: { const float u = 0.5f - 0.5f * std::cos (kPi * ph); v = S.from + (S.to - S.from) * u; break; }
                case LStep: v = S.held; break;
                default: v = std::sin (kTwoPi * ph); break;
            }
            v *= L.amount;
            switch (L.target)
            {
                case TPosition: md.position += v * p.bufferSec * 0.5f; break;
                case TSpray: md.spray *= std::max (0.0f, 1.0f + v); break;
                case TSize: md.sizeMs *= std::exp2 (2.0f * v); break;
                case TDensity: md.density *= std::exp2 (2.0f * v); break;
                case TPitch: md.pitch += 12.0f * v; break;
                case TPan: md.pan = clampf (md.pan + v, 0.0f, 1.0f); break;
                case TTone: md.highCut *= std::exp2 (3.0f * v); md.lowCut *= std::exp2 (1.5f * v); break;
                case TMix: md.mix = clampf (md.mix + 0.5f * v, 0.0f, 1.0f); break;
                case TFeedback: md.feedback = clampf (md.feedback + 0.5f * v, 0.0f, 0.97f); break;
                default: break;
            }
        }
        return md;
    }

    float quantize (float semis) const
    {
        static const std::array<std::vector<int>, QuantCount> sets { {
            {}, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 }, { 0 }, { 0, 7 }, { 0, 4, 7 }, { 0, 3, 7 }, { 0, 2, 4, 7, 9 }, { 0, 2, 4, 6, 8, 10 } } };
        if (p.quant <= QOff || p.quant >= QuantCount) return semis;
        const auto& set = sets[(size_t) p.quant];
        float best = 0.0f, bd = 1.0e9f;
        for (int oct = -3; oct <= 3; ++oct)
            for (int pc : set)
            {
                const float v = (float) (oct * 12 + pc);
                if (p.quant == QFifths && pc == 7 && oct * 12 + 7 > 24) continue;
                const float d = std::fabs (v - semis);
                if (d < bd) { bd = d; best = v; }
            }
        return best;
    }

    void startGrain (const Mod& md, bool frozen, double bufSamples)
    {
        int voiceLimit = std::clamp (p.voices, 1, kMaxGrains), activeNow = 0;
        Grain* slot = nullptr;
        for (auto& g : grains) { if (g.active) ++activeNow; else if (slot == nullptr) slot = &g; }
        if (slot == nullptr || activeNow >= voiceLimit) return;
        Grain& g = *slot;

        // Tamaño.
        const float sizeMs = clampf (md.sizeMs * std::exp2 (p.sizeRand * rng.bipolar()), 5.0f, 2000.0f);
        g.len = std::max (16, (int) (sizeMs * 0.001f * sr));

        // Altura: la base, la nota MIDI, el azar (cuantizado) y las octavas de shimmer y sub.
        float semis = md.pitch + p.fine * 0.01f;
        float vel = 1.0f;
        if (p.midiMode != MidiOff && heldCount > 0)
        {
            const auto& h = held[(size_t) std::min (heldCount - 1, (int) (rng.unipolar() * (float) heldCount))];
            semis += (float) (h.note - 60);
            vel = 0.35f + 0.65f * h.vel;
        }
        if (p.pitchRand > 0.0f) semis += quantize (p.pitchRand * rng.bipolar());
        else if (p.quant != QOff) semis = std::round (semis * 1000.0f) / 1000.0f;
        if (rng.unipolar() < p.shimmer) semis += 12.0f;
        if (rng.unipolar() < p.subOctave) semis -= 12.0f;
        semis = clampf (semis, -48.0f, 48.0f);
        const double ratio = std::exp2 (semis / 12.0);
        g.reverse = rng.unipolar() < p.reverse;
        g.inc = g.reverse ? -ratio : ratio;
        g.pitch = semis;

        // Posición: cuánto atrás de la cabeza de grabación arranca (con spray y el desplazamiento de scan).
        double back = ((double) md.position - scanSec + (double) md.spray * rng.bipolar()) * sr;
        const double span = ratio * g.len;                                   // muestras de fuente que recorre
        // Un grano hacia adelante no debe pasar la cabeza de grabación mientras suena (detrás está lo más viejo, o
        // silencio): sin congelar la cabeza avanza g.len muestras en ese tiempo, congelada no se mueve.
        const double minBack = g.reverse ? 4.0 : std::max (4.0, span - (frozen ? 0.0 : (double) g.len) + 4.0);
        // Sólo lo grabado: congelado antes de llenar el búfer, Scan da la vuelta sobre lo que hay y no sobre silencio.
        const double avail = std::min (bufSamples, (double) written);
        const double maxBack = std::max (minBack + 1.0, avail - (g.reverse ? span : 0.0) - 4.0);
        if (frozen && avail > 1.0) { back = std::fmod (back, avail); if (back < 0.0) back += avail; }
        // Lo que el spray pide antes del borde (audio que todavía no llegó) se refleja hacia atrás en vez de
        // recortarse: si no, con Spray mayor que Position la mitad de los granos se amontonaba en lo más nuevo.
        if (back < minBack) back = minBack + (minBack - back);
        back = std::clamp (back, minBack, maxBack);
        g.pos = (double) w - back;
        while (g.pos < 0.0) g.pos += bufN;

        // Envolvente, paneo y nivel (compensado por la superposición: la nube suena pareja a cualquier densidad).
        g.peak = clampf (0.5f + 0.45f * p.skew, 0.05f, 0.95f);
        const float minTaper = clampf (0.002f * sr / (float) g.len, 0.0f, 1.0f);      // al menos 2 ms de borde
        g.taper = clampf (std::max (p.shape, minTaper), 0.0f, 1.0f);
        const float panV = clampf (md.pan, 0.0f, 1.0f) * rng.bipolar();
        g.panV = panV;
        const float a = (panV + 1.0f) * 0.25f * kPi;
        g.gl = std::cos (a) * 1.41421356f; g.gr = std::sin (a) * 1.41421356f;
        g.width = p.width;
        const float overlap = std::max (1.0f, md.density * sizeMs * 0.001f);
        g.gain = vel / std::sqrt (overlap) * (0.9f + 0.2f * rng.unipolar());
        g.age = 0; g.level = 0.0f; g.active = true;
    }

    void publishView()
    {
        const float span = clampf (p.bufferSec, 0.5f, kMaxBufferSec) * sr;
        for (size_t i = 0; i < grains.size(); ++i)
        {
            const auto& g = grains[i];
            auto& v = view[i];
            v.active = g.active;
            if (! g.active) continue;
            double back = (double) w - g.pos; if (back < 0.0) back += bufN;
            v.pos = (float) (back / span);                 // 0 = recién grabado, 1 = el fondo del búfer
            v.pitch = g.pitch; v.level = g.level; v.pan = g.panV; v.reverse = g.reverse;
            v.age = (float) g.age / (float) g.len;
        }
        viewSerial.fetch_add (1, std::memory_order_release);
    }

    static constexpr int kWin = 1024;
    std::array<float, kWin> win {};
    float sr = 48000.0f;
    int bufN = 1, w = 0;
    long long written = 0;
    std::vector<float> recL, recR, wetL, wetR, dryL, dryR, mixT, duckT;   // mezcla y ducking de cada muestra
    std::array<Grain, kMaxGrains> grains {};
    std::array<LfoState, 2> lfoState {};
    struct Held { int note; float vel; };
    std::array<Held, 16> held {};
    int heldCount = 0;
    bool freezeHold = false, dlyIdle = true, revIdle = true;
    double nextGrain = 0.0, scanSec = 0.0, freePpq = 0.0;
    long long lastDiv = -1;
    Mod segMod {};                             // la modulación del tramo actual
    int subPos = 0;                            // muestras ya hechas del tramo actual
    float duckEnv = 0.0f, duckAtt = 0.01f, duckRel = 0.0005f, peakAcc = 0.0f;
    int peakCount = 0;
    Rng rng { 0x6d75726du };
    Smoother mixS, inS, outS;
    OnePoleHp hpL, hpR; OnePole lpL, lpR;
    DcBlocker fbDcL, fbDcR;
    StereoDelay dly; PlateReverb rev;
};

} // namespace dh::mm
