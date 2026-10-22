#pragma once
// La pedalera de Thrum (docs/research.md §4, §8): trece pedales con nombres genéricos, reordenables junto con el
// amplificador y el gabinete. Las saturaciones usan ADAA de primer orden (sin sobremuestreo: latencia cero). Se reusa
// lo del repositorio: la red de Halation para la sala (y su congelado), la histéresis de Jiles-Atherton de Tarnish en el
// lazo del eco de cinta y el compresor de Lacquer.
//
//   Puerta de ruido, compresor, wah (manual, automático o por CC), overdrive "verde" (pasa altos de 720 Hz hacia los
//   diodos de la realimentación: el bajo pasa limpio y aparece la joroba de medios), distorsión (ganancia de
//   operacional con su ancho de banda finito, recorte duro a masa, filtro), fuzz de silicio o germanio (dos transistores:
//   recorte asimétrico, polarización, el "escupido" del germanio con poca batería), chorus, phaser, flanger, trémolo,
//   vibrato, delay (digital, analógico de BBD, cinta) y reverb (resorte, sala).
// Todos estéreo en el lugar: un pedal mono procesa los dos canales si la señal ya es estéreo.

#include "Base.h"
#include "../../../halation/Source/dsp/Network.h"            // dh::sp::Network (sala, congelado)
#include "../../../tarnish/Source/dsp/Hysteresis.h"          // dh::tn::Hysteresis (cinta)
#include "../../../lacquer/Source/dsp/Dynamics.h"            // dh::lq::Compressor

namespace dh::gt {

enum PedalId { kGate, kComp, kWah, kOd, kDist, kFuzz, kChorus, kPhaser, kFlanger, kTremolo, kVibrato, kDelay, kReverb, kNumPedals };
// Las unidades de la cadena: los pedales, el amplificador y el gabinete.
enum UnitId { kUnitAmp = kNumPedals, kUnitCab, kNumUnits };
inline const char* unitName (int u)
{
    static const char* n[] = { "Noise gate", "Compressor", "Wah", "Green overdrive", "Distortion", "Fuzz", "Chorus", "Phaser", "Flanger",
                               "Tremolo", "Vibrato", "Delay", "Reverb", "Amp", "Cabinet" };
    return n[std::clamp (u, 0, (int) kNumUnits - 1)];
}
inline const char* unitKey (int u)
{
    static const char* n[] = { "gate", "comp", "wah", "od", "dist", "fuzz", "chorus", "phaser", "flanger", "trem", "vib", "delay", "verb", "amp", "cab" };
    return n[std::clamp (u, 0, (int) kNumUnits - 1)];
}

// Parámetros genéricos de un pedal: p[0..5] en 0..1 salvo donde se indique, type para los selectores.
struct PedalParams { bool on = false; float p[6] {}; int type = 0; };

// ── Puerta de ruido ──
// p0 umbral (−80..−20 dB), p1 liberación (20..500 ms).
class Gate
{
public:
    void prepare (float f) { fs = f; reset(); }
    void reset() { env = 0; g = 0; hold = 0; }
    void set (const PedalParams& p)
    {
        thr = dbToGain (-80.0f + 60.0f * p.p[0]); thrClose = thr * 0.5f;
        rel = 1.0f - fastExp (-1.0f / ((0.02f + 0.48f * p.p[1]) * fs));
        att = 1.0f - fastExp (-1.0f / (0.0007f * fs));
        envR = fastExp (-1.0f / (0.01f * fs));
    }
    void process (float* L, float* R, int n, bool stereo)
    {
        for (int i = 0; i < n; ++i)
        {
            const float a = stereo ? std::max (std::fabs (L[i]), std::fabs (R[i])) : std::fabs (L[i]);
            env = a > env ? a : env * envR;
            if (env > thr) hold = (int) (0.03f * fs);
            const float target = (env > thrClose && hold > 0) ? 1.0f : 0.0f;
            if (hold > 0) --hold;
            g += (target - g) * (target > g ? att : rel);
            L[i] *= g; if (stereo) R[i] *= g;
        }
    }
private:
    float fs = 48000, env = 0, g = 0, thr = 0.001f, thrClose = 0.0005f, att = 0.1f, rel = 0.001f, envR = 0.999f; int hold = 0;
};

// ── Compresor (el de Lacquer, con los tiempos de un compresor de pedal) ──
// p0 sustain (umbral −10..−45 dB), p1 ataque (0,5..30 ms), p2 nivel (−12..+12 dB), p3 mezcla.
class Comp
{
public:
    void prepare (float f) { fs = f; c.prepare (fs); reset(); }
    void reset() { c.reset(); }
    void set (const PedalParams& p)
    {
        dh::lq::CompParams cp;
        cp.thresholdDb = -10.0 - 35.0 * p.p[0]; cp.ratio = 6.0; cp.kneeDb = 8.0;
        cp.attackMs = 0.5 + 29.5 * p.p[1] * p.p[1]; cp.releaseMs = 180.0;
        cp.makeupDb = -cp.thresholdDb * (1.0 - 1.0 / cp.ratio) * 0.7;
        c.set (cp);
        out = dbToGain (-12.0f + 24.0f * p.p[2]); mix = p.p[3];
    }
    void process (float* L, float* R, int n, bool stereo)
    {
        for (int i = 0; i < n; ++i)
        {
            const double det = stereo ? std::max (std::fabs (L[i]), std::fabs (R[i])) : std::fabs (L[i]);
            const float g = (float) c.gainFor (det);
            L[i] = (L[i] + (L[i] * g - L[i]) * mix) * out;
            if (stereo) R[i] = (R[i] + (R[i] * g - R[i]) * mix) * out;
        }
    }
private:
    float fs = 48000, out = 1, mix = 1; dh::lq::Compressor c;
};

// ── Wah ──
// type 0 manual (p0 posición), 1 automático (p1 sensibilidad, p2 velocidad), 2 por CC (el motor pone la posición).
// p3 rango (estrecho … amplio), p4 Q. El pedal del inductor: un pasa banda resonante de ~380 Hz a ~2,3 kHz con algo de
// pasa bajos sumado (el wah real no deja pasar sólo la banda).
class Wah
{
public:
    void prepare (float f) { fs = f; reset(); }
    void reset() { svfL.reset(); svfR.reset(); env = 0; pos = 0.5f; }
    void set (const PedalParams& p) { P = p; envA = 1.0f - fastExp (-1.0f / (0.004f * fs)); envR = 1.0f - fastExp (-1.0f / ((0.05f + 0.4f * (1.0f - p.p[2])) * fs)); }
    void setExternal (float x) { ext = clampf (x, 0.0f, 1.0f); }
    void process (float* L, float* R, int n, bool stereo)
    {
        const float lo = 380.0f * (1.0f - 0.25f * P.p[3]), hi = 2300.0f * (1.0f + 0.3f * P.p[3]);
        const float q = 2.5f + 6.0f * P.p[4];
        for (int i = 0; i < n; ++i)
        {
            float target = P.type == 0 ? P.p[0] : P.type == 2 ? ext : 0.0f;
            if (P.type == 1)
            {
                const float a = std::fabs (stereo ? 0.5f * (L[i] + R[i]) : L[i]) * (2.0f + 30.0f * P.p[1]);
                env += (a - env) * (a > env ? envA : envR);
                target = clampf (env, 0.0f, 1.0f);
            }
            pos += (target - pos) * 0.002f;
            if ((i & 7) == 0) { const float f = lo * fastExp2 (pos * std::log2 (hi / lo)); svfL.set (f, q, fs); svfR.set (f, q, fs); }
            svfL.process (L[i]); L[i] = (svfL.bp * svfL.k * 2.2f + svfL.lp * 0.25f);
            if (stereo) { svfR.process (R[i]); R[i] = (svfR.bp * svfR.k * 2.2f + svfR.lp * 0.25f); }
        }
    }
private:
    float fs = 48000, env = 0, pos = 0.5f, envA = 0.1f, envR = 0.01f, ext = 0.5f; PedalParams P; Svf svfL, svfR;
};

// ── Overdrive "verde" ──
// p0 drive, p1 tono, p2 nivel. y = x + recorte((G − 1)·HP720(x)), con el tono como pasa bajos variable.
class Overdrive
{
public:
    void prepare (float f) { fs = f; reset(); }
    void reset() { for (auto& c : ch) { c.hp.reset(); c.lp.reset(); c.pre.reset(); c.ad.reset(); } }
    void set (const PedalParams& p)
    {
        gain = 1.0f + 120.0f * p.p[0] * p.p[0];
        for (auto& c : ch) { c.hp.set (720.0f, fs); c.lp.set (expMap (p.p[1], 900.0f, 7000.0f), fs); c.pre.set (6000.0f, fs); }
        out = dbToGain (-18.0f + 24.0f * p.p[2]);
    }
    void process (float* L, float* R, int n, bool stereo)
    {
        for (int c = 0; c < (stereo ? 2 : 1); ++c)
        {
            float* x = c == 0 ? L : R; auto& C = ch[(size_t) c];
            for (int i = 0; i < n; ++i)
            {
                const float hpv = C.hp.process (x[i]);
                // Los diodos en la realimentación limitan sólo la parte amplificada; el operacional limita el ancho.
                const float d = C.ad.process (C.pre.process (hpv * (gain - 1.0f) * 2.0f)) * 0.5f;
                x[i] = C.lp.process (x[i] + d) * out;
            }
        }
    }
private:
    struct Ch { Hp1 hp; Lp1 lp, pre; Adaa1<CubicFn, CubicAnti> ad; };
    std::array<Ch, 2> ch; float fs = 48000, gain = 10, out = 1;
};

// ── Distorsión ──
// p0 distorsión, p1 filtro, p2 nivel. Operacional con producto ganancia-ancho de banda finito (a ganancia alta el
// ancho cae a ~1 kHz y la distorsión se vuelve áspera y oscura a la vez), diodos a masa (recorte duro), filtro.
class Distortion
{
public:
    void prepare (float f) { fs = f; reset(); }
    void reset() { for (auto& c : ch) { c.hp.reset(); c.gbw.reset(); c.lp.reset(); c.ad.reset(); c.post.reset(); } }
    void set (const PedalParams& p)
    {
        gain = 2.0f + 900.0f * p.p[0] * p.p[0] * p.p[0];
        const float bw = clampf (1.0e6f / gain, 900.0f, 0.4f * fs);
        for (auto& c : ch) { c.hp.set (80.0f, fs); c.gbw.set (bw, fs); c.lp.set (expMap (1.0f - p.p[1], 600.0f, 14000.0f), fs); c.post.set (40.0f, fs); }
        out = dbToGain (-24.0f + 24.0f * p.p[2]);
    }
    void process (float* L, float* R, int n, bool stereo)
    {
        for (int c = 0; c < (stereo ? 2 : 1); ++c)
        {
            float* x = c == 0 ? L : R; auto& C = ch[(size_t) c];
            for (int i = 0; i < n; ++i)
            {
                const float v = C.gbw.process (C.hp.process (x[i]) * gain);
                x[i] = C.post.process (C.lp.process (C.ad.process (v * 1.4f) * 0.7f)) * out;
            }
        }
    }
private:
    struct Ch { Hp1 hp, post; Lp1 gbw, lp; Adaa1<HardFn, HardAnti> ad; };
    std::array<Ch, 2> ch; float fs = 48000, gain = 10, out = 1;
};

// ── Fuzz ──
// type 0 silicio, 1 germanio. p0 fuzz, p1 polarización (germanio: con poca, se corta y escupe), p2 tono, p3 nivel.
class Fuzz
{
public:
    void prepare (float f) { fs = f; reset(); }
    void reset() { for (auto& c : ch) { c.in.reset(); c.lp.reset(); c.ad.reset(); c.dc.reset(); c.env = 0; } }
    void set (const PedalParams& p)
    {
        P = p;
        gain = 4.0f + (p.type == 1 ? 150.0f : 300.0f) * p.p[0] * p.p[0];
        bias = p.type == 1 ? 0.15f + 0.45f * (1.0f - p.p[1]) : 0.08f;
        for (auto& c : ch) { c.in.set (p.type == 1 ? 60.0f : 110.0f, fs); c.lp.set (expMap (p.p[2], 1200.0f, 9000.0f), fs); c.dc.set (20.0f, fs); }
        out = dbToGain (-26.0f + 22.0f * p.p[3]);
        envK = 1.0f - fastExp (-1.0f / (0.02f * fs));
    }
    void process (float* L, float* R, int n, bool stereo)
    {
        for (int c = 0; c < (stereo ? 2 : 1); ++c)
        {
            float* x = c == 0 ? L : R; auto& C = ch[(size_t) c];
            for (int i = 0; i < n; ++i)
            {
                float v = C.in.process (x[i]) * gain;
                // Germanio: el punto de trabajo se corre con la señal (la carga del acople): ataques que se "abren".
                C.env += (std::fabs (v) - C.env) * envK;
                const float b = P.type == 1 ? bias * (1.0f - 0.4f * std::min (1.0f, C.env * 0.1f)) : bias;
                float y = C.ad.process (v + b) - CubicFn() (b);
                if (P.type == 1) y = y > 0.0f ? y : y * 0.75f;                 // el germanio recorta distinto de cada lado
                // Compuerta del germanio con poca polarización: debajo de un umbral la señal se corta (escupe).
                if (P.type == 1 && P.p[1] < 0.3f) { const float th = (0.3f - P.p[1]) * 0.25f; const float a = std::fabs (y); y *= clampf ((a - th) / (th + 1e-4f) + 1.0f, 0.0f, 1.0f); }
                x[i] = C.dc.process (C.lp.process (y)) * out;
            }
        }
    }
private:
    struct Ch { Hp1 in, dc; Lp1 lp; Adaa1<CubicFn, CubicAnti> ad; float env = 0; };
    std::array<Ch, 2> ch; PedalParams P; float fs = 48000, gain = 10, bias = 0.1f, out = 1, envK = 0.01f;
};

// ── Chorus (BBD de dos voces) ──
// p0 velocidad (0,1..3 Hz), p1 profundidad, p2 mezcla, p3 tono. Estéreo: cada lado con su fase.
class Chorus
{
public:
    void prepare (float f) { fs = f; line.prepare ((int) (0.05f * fs)); reset(); }
    void reset() { line.clear(); ph = 0; lpL.reset(); lpR.reset(); }
    void set (const PedalParams& p) { P = p; lpL.set (expMap (p.p[3], 3000.0f, 12000.0f), fs); lpR.set (expMap (p.p[3], 3000.0f, 12000.0f), fs); }
    void process (float* L, float* R, int n, bool& stereo)
    {
        const float rate = 0.1f + 2.9f * P.p[0] * P.p[0], c = 0.0075f * fs, dep = (0.0004f + 0.0035f * P.p[1]) * fs;
        for (int i = 0; i < n; ++i)
        {
            const float in = stereo ? 0.5f * (L[i] + R[i]) : L[i];
            line.push (in);
            ph += rate / fs; ph -= std::floor (ph);
            const float a = line.hermite (c + dep * triFromPhase (ph)), b = line.hermite (c + dep * triFromPhase (ph + 0.5f));
            const float dl = stereo ? L[i] : in, dr = stereo ? R[i] : in;
            L[i] = dl + (lpL.process (a) - dl * 0.0f) * P.p[2];
            R[i] = dr + (lpR.process (b) - dr * 0.0f) * P.p[2];
            const float norm = 1.0f / (1.0f + 0.5f * P.p[2]);
            L[i] *= norm; R[i] *= norm;
        }
        stereo = true;
    }
private:
    float fs = 48000, ph = 0; PedalParams P; Line line; Lp1 lpL, lpR;
};

// ── Phaser (cuatro etapas de OTA con realimentación) ──
// p0 velocidad, p1 profundidad, p2 realimentación, p3 mezcla.
class Phaser
{
public:
    void prepare (float f) { fs = f; reset(); }
    void reset() { zl.fill (0); zr.fill (0); fbl = fbr = 0; ph = 0; }
    void set (const PedalParams& p) { P = p; }
    void process (float* L, float* R, int n, bool stereo)
    {
        const float rate = 0.05f + 4.0f * P.p[0] * P.p[0];
        for (int i = 0; i < n; ++i)
        {
            ph += rate / fs; ph -= std::floor (ph);
            const float u = 0.5f + 0.5f * sin2pi (ph);
            L[i] = run (zl, fbl, L[i], u);
            if (stereo) R[i] = run (zr, fbr, R[i], 0.5f + 0.5f * sin2pi (ph + 0.25f));
        }
    }
private:
    float fs = 48000, ph = 0, fbl = 0, fbr = 0; PedalParams P; std::array<float, 4> zl {}, zr {};
    inline float run (std::array<float, 4>& z, float& fb, float x, float u)
    {
        const float fc = 200.0f * fastExp2 ((1.0f + 3.5f * P.p[1]) * u);
        const float t = prewarp (fc, fs), a = (t - 1.0f) / (t + 1.0f);
        float v = x + fb * 0.8f * P.p[2];
        for (auto& s : z) { const float y = a * v + s; s = v - a * y; v = y; }
        fb = fastTanh (v);
        return x + (v - x) * (0.5f * P.p[3]) + (v * 0.5f * P.p[3]) * 0.0f;
    }
};

// ── Flanger ──
// p0 velocidad, p1 profundidad, p2 realimentación, p3 mezcla, p4 manual (retardo base).
class Flanger
{
public:
    void prepare (float f) { fs = f; l.prepare ((int) (0.02f * fs)); r.prepare ((int) (0.02f * fs)); reset(); }
    void reset() { l.clear(); r.clear(); ph = 0; yl = yr = 0; }
    void set (const PedalParams& p) { P = p; }
    void process (float* L, float* R, int n, bool& stereo)
    {
        const float rate = 0.03f + 1.5f * P.p[0] * P.p[0];
        const float base = (0.0002f + 0.003f * P.p[4]) * fs, span = (0.0003f + 0.006f * P.p[1]) * fs, fb = 0.92f * (2.0f * P.p[2] - 1.0f);
        for (int i = 0; i < n; ++i)
        {
            ph += rate / fs; ph -= std::floor (ph);
            const float u0 = 0.5f + 0.5f * triFromPhase (ph), u1 = 0.5f + 0.5f * triFromPhase (ph + 0.25f);
            const float inL = L[i], inR = stereo ? R[i] : L[i];
            l.push (fastTanh (inL + fb * yl)); r.push (fastTanh (inR + fb * yr));
            yl = l.hermite (base + span * u0 * u0); yr = r.hermite (base + span * u1 * u1);
            L[i] = (inL + yl * P.p[3]) / (1.0f + 0.5f * P.p[3]); R[i] = (inR + yr * P.p[3]) / (1.0f + 0.5f * P.p[3]);
        }
        stereo = true;
    }
private:
    float fs = 48000, ph = 0, yl = 0, yr = 0; PedalParams P; Line l, r;
};

// ── Trémolo ──
// type 0 seno, 1 óptico (más cuadrado y asimétrico), 2 armónico (graves y agudos en contrafase). p0 velocidad (o
// división del tempo si p2 > 0,5), p1 profundidad.
class Tremolo
{
public:
    void prepare (float f) { fs = f; reset(); }
    void reset() { ph = 0; for (auto* x : { &lpL, &lpR }) x->reset(); }
    void set (const PedalParams& p) { P = p; lpL.set (800.0f, fs); lpR.set (800.0f, fs); }
    void process (float* L, float* R, int n, bool stereo, double bpm)
    {
        const float rate = P.p[2] > 0.5f ? (float) (bpm / 60.0) * (P.p[0] < 0.33f ? 1.0f : P.p[0] < 0.66f ? 2.0f : 4.0f) : 1.0f + 11.0f * P.p[0];
        for (int i = 0; i < n; ++i)
        {
            ph += rate / fs; ph -= std::floor (ph);
            float m = 0.5f + 0.5f * sin2pi (ph);
            if (P.type == 1) m = clampf ((m - 0.5f) * 2.6f + 0.5f, 0.0f, 1.0f), m = m * m;
            if (P.type == 2)
            {
                const float lo = lpL.process (L[i]), hi = L[i] - lo;
                L[i] = lo * (1.0f - P.p[1] * m) + hi * (1.0f - P.p[1] * (1.0f - m));
                if (stereo) { const float lo2 = lpR.process (R[i]), hi2 = R[i] - lo2; R[i] = lo2 * (1.0f - P.p[1] * m) + hi2 * (1.0f - P.p[1] * (1.0f - m)); }
                continue;
            }
            const float g = 1.0f - P.p[1] * m;
            L[i] *= g; if (stereo) R[i] *= g;
        }
    }
private:
    float fs = 48000, ph = 0; PedalParams P; Lp1 lpL, lpR;
};

// ── Vibrato (sólo la señal modulada) ──
// p0 velocidad, p1 profundidad.
class Vibrato
{
public:
    void prepare (float f) { fs = f; l.prepare ((int) (0.02f * fs)); r.prepare ((int) (0.02f * fs)); reset(); }
    void reset() { l.clear(); r.clear(); ph = 0; }
    void set (const PedalParams& p) { P = p; }
    void process (float* L, float* R, int n, bool stereo)
    {
        const float rate = 0.5f + 8.0f * P.p[0] * P.p[0], c = 0.004f * fs, d = (0.0002f + 0.0028f * P.p[1]) * fs;
        for (int i = 0; i < n; ++i)
        {
            ph += rate / fs; ph -= std::floor (ph);
            const float m = sin2pi (ph);
            l.push (L[i]); L[i] = l.hermite (c + d * m);
            if (stereo) { r.push (R[i]); R[i] = r.hermite (c + d * m); }
        }
    }
private:
    float fs = 48000, ph = 0; PedalParams P; Line l, r;
};

// ── Delay ──
// type 0 digital, 1 analógico (BBD: el filtro baja con el tiempo, satura, ruido de reloj chico), 2 cinta (wow, flutter,
// histéresis en el lazo, cabezas). p0 tiempo (ms 40..1500, o división si p5 > 0,5), p1 realimentación, p2 mezcla,
// p3 tono, p4 modulación, p5 sincronizado. Ping-pong estéreo.
inline double delayDivBeats (float u) { static const double b[] = { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.375, 0.5, 2.0 / 3.0, 0.75, 1.0, 1.5, 2.0 }; return b[std::clamp ((int) (u * 10.999f), 0, 10)]; }
inline const char* delayDivName (float u) { static const char* n[] = { "1/32", "1/16T", "1/16", "1/8T", "1/16.", "1/8", "1/4T", "1/8.", "1/4", "1/4.", "1/2" }; return n[std::clamp ((int) (u * 10.999f), 0, 10)]; }
class Delay
{
public:
    void prepare (float f) { fs = f; l.prepare ((int) (2.2f * fs)); r.prepare ((int) (2.2f * fs)); reset(); }
    void reset() { l.clear(); r.clear(); fbl = fbr = 0; wow = flut = drift = 0; hyst.reset(); hystR.reset(); for (auto* x : { &tl, &tr, &tl2, &tr2 }) x->reset(); hpl.reset(); hpr.reset(); dSm = -1; rng.seed (0xD31A7u); }
    void set (const PedalParams& p)
    {
        if (p.type != P.type) { l.clear(); r.clear(); fbl = fbr = 0; }
        P = p;
        const float lp = expMap (p.p[3], 1500.0f, 14000.0f);
        tl.set (lp, 0.7f, fs); tr.set (lp, 0.7f, fs); tl2.set (lp, 0.7f, fs); tr2.set (lp, 0.7f, fs);
        hpl.set (p.type == 2 ? 120.0f : 70.0f, fs); hpr.set (p.type == 2 ? 120.0f : 70.0f, fs);
        if (p.type == 2 && std::fabs (p.p[1] - hystFb) > 1e-4f)
        {
            const double d = 2.0 + 8.0 * p.p[1];
            hyst.set ({ d, 0.55, 0.0, 1.0, false }); hystR.set ({ d, 0.55, 0.0, 1.0, false }); hystFb = p.p[1]; hystMk = (float) std::pow (10.0, -d / 20.0);
        }
    }
    void process (float* L, float* R, int n, bool& stereo, double bpm)
    {
        float ms = P.p[5] > 0.5f ? (float) (delayDivBeats (P.p[0]) * 60000.0 / std::max (20.0, bpm)) : 40.0f + 1460.0f * P.p[0] * P.p[0];
        if (P.type == 1) ms = std::min (ms, 600.0f);
        const float target = clampf (ms * 1e-3f * fs, 2.0f, 2.1f * fs);
        if (dSm < 0) dSm = target;
        if (P.type == 1)
        {
            const float hz = clampf (6000.0f * std::pow (100.0f / std::max (20.0f, ms), 0.56f), 1500.0f, 9000.0f) * expMap (P.p[3], 0.6f, 1.4f);
            tl.set (hz, 0.6f, fs); tr.set (hz, 0.6f, fs); tl2.set (hz, 0.6f, fs); tr2.set (hz, 0.6f, fs);
        }
        const float glide = 1.0f - fastExp (-1.0f / (0.08f * fs));
        const float fb = std::min (0.95f, P.p[1] * 1.05f);
        for (int i = 0; i < n; ++i)
        {
            dSm += (target - dSm) * glide;
            float mod = 1.0f;
            if (P.type == 2 || P.p[4] > 0.0f)
            {
                wow += 0.6f / fs; wow -= std::floor (wow); flut += 6.5f / fs; flut -= std::floor (flut);
                drift += (rng.next() - drift) * (1.0f / (0.7f * fs));
                const float depth = (P.type == 2 ? 1.0f : 0.3f) * (0.2f + P.p[4]);
                mod = 1.0f + depth * (0.003f * sin2pi (wow) + 0.0007f * sin2pi (flut) + 0.0015f * drift);
            }
            const float d = dSm * mod;
            const float yl = l.hermite (d), yr = r.hermite (d);
            const float inL = L[i], inR = stereo ? R[i] : L[i];
            const float in = 0.5f * (inL + inR);
            float wl, wr;
            if (P.type == 2)
            {
                wl = (float) hyst.process ((double) (in + fb * yr)) * hystMk;
                wr = (float) hystR.process ((double) (fb * yl)) * hystMk;
                wl = tl2.lowpass (tl.lowpass (hpl.process (wl))); wr = tr2.lowpass (tr.lowpass (hpr.process (wr)));
            }
            else if (P.type == 1)
            {
                wl = tl2.lowpass (tl.lowpass (in + fb * yr)); wr = tr2.lowpass (tr.lowpass (fb * yl));
                wl = fastTanh (1.3f * wl) / 1.3f + 2e-5f * rng.next(); wr = fastTanh (1.3f * wr) / 1.3f + 2e-5f * rng.next();
                wl = hpl.process (wl); wr = hpr.process (wr);
            }
            else
            {
                wl = tl.lowpass (hpl.process (in + fb * yr)); wr = tr.lowpass (hpr.process (fb * yl));
            }
            l.push (wl); r.push (wr);
            L[i] = inL + P.p[2] * yl; R[i] = inR + P.p[2] * yr;
        }
        stereo = true;
    }
private:
    float fs = 48000, fbl = 0, fbr = 0, wow = 0, flut = 0, drift = 0, dSm = -1, hystFb = -1, hystMk = 1; PedalParams P;
    Line l, r; Svf tl, tr, tl2, tr2; Hp1 hpl, hpr; dh::tn::Hysteresis hyst, hystR; Noise rng;
};

// ── Reverb ──
// type 0 resorte (dos tanques: cadena de pasa todos estirados que dan el "chirrido" y el goteo, línea y realimentación),
// 1 sala, 2 hall (la red de Halation). p0 decaimiento, p1 tono, p2 mezcla, p3 pre-retardo, p4 goteo / tamaño,
// p5 congelar (> 0,5: la cola queda sonando sin fin, como en Halation).
class SpringTank
{
public:
    void prepare (float fs_, float lenMs, uint32_t seed)
    {
        fs = fs_;
        line.prepare ((int) (lenMs * 1e-3f * fs) + 8); len = lenMs * 1e-3f * fs;
        ap.fill (0); apx.fill (0); lp = 0; hp.set (90.0f, fs); hp.reset(); rng.seed (seed);
    }
    void set (float decay, float drip, float tone)
    {
        fb = 0.55f + 0.42f * decay;
        a = 0.45f + 0.35f * drip;
        lpC = 1.0f - fastExp (-kTwoPi * expMap (tone, 2200.0f, 6500.0f) / fs);
    }
    inline float process (float x)
    {
        float v = x + line.linear (len) * fb;
        // Dispersión: pasa todos de segundo grado estirado (z⁻²): los graves llegan tarde (el chirrido del resorte).
        for (int k = 0; k < kAp; ++k)
        {
            const float y = -a * v + apx[(size_t) k * 2 + 1] + a * ap[(size_t) k * 2 + 1];
            apx[(size_t) k * 2 + 1] = apx[(size_t) k * 2]; apx[(size_t) k * 2] = v;
            ap[(size_t) k * 2 + 1] = ap[(size_t) k * 2]; ap[(size_t) k * 2] = y;
            v = y;
        }
        lp += (v - lp) * lpC;
        const float o = hp.process (lp);
        line.push (fastTanh (o * 1.2f) / 1.2f + 1e-6f * rng.next());
        return o;
    }
private:
    static constexpr int kAp = 18;
    Line line; float len = 1000, fb = 0.8f, a = 0.6f, lpC = 0.3f, lp = 0, fs = 48000; Hp1 hp; Noise rng;
    std::array<float, kAp * 2> ap {}, apx {};
};

class Reverb
{
public:
    void prepare (float f)
    {
        fs = f;
        net.prepare (fs, 11u, 400.0f);
        sp1.prepare (fs, 37.0f, 3u); sp2.prepare (fs, 43.0f, 5u);
        preL.prepare ((int) (0.2f * fs)); preR.prepare ((int) (0.2f * fs));
        reset();
    }
    void reset() { net.reset(); sp1.prepare (fs, 37.0f, 3u); sp2.prepare (fs, 43.0f, 5u); preL.clear(); preR.clear(); sp1.set (P.p[0], P.p[4], P.p[1]); sp2.set (P.p[0], P.p[4], P.p[1]); }
    void set (const PedalParams& p)
    {
        if (p.type != P.type) { net.reset(); }
        P = p;
        if (P.type == 0) { sp1.set (P.p[0], P.p[4], P.p[1]); sp2.set (P.p[0], P.p[4], P.p[1]); }
        else
        {
            dh::sp::NetParams np;
            np.space = 1.0f; np.diffusion = 0.75f; np.modDepth = 0.25f; np.modRate = 0.4f; np.width = 1.0f; np.lowCutHz = 70.0f;
            np.room = P.type == 1 ? dh::sp::RoomType::room : dh::sp::RoomType::hall;
            np.size = P.type == 1 ? 0.5f + 1.0f * P.p[4] : 0.7f + 1.3f * P.p[4];
            np.decayS = P.p[5] > 0.5f ? 1000.0f : (P.type == 1 ? 0.3f + 2.7f * P.p[0] : 1.0f + 9.0f * P.p[0] * P.p[0]);
            np.hfRatio = 0.15f + 0.7f * P.p[1]; np.lfRatio = 1.0f;
            np.freezeMask = P.p[5] > 0.5f ? 7 : 0;
            net.set (np);
        }
        pre = P.p[3] * 0.12f * fs;
    }
    void process (float* L, float* R, int n, bool& stereo)
    {
        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = stereo ? R[i] : L[i];
            preL.push (inL); preR.push (inR);
            const float xl = pre >= 1.0f ? preL.linear (pre) : inL, xr = pre >= 1.0f ? preR.linear (pre) : inR;
            float wl, wr;
            if (P.type == 0)
            {
                const float x = 0.5f * (xl + xr) * 0.6f;
                wl = sp1.process (x) * 1.6f; wr = sp2.process (x) * 1.6f;
            }
            else net.process (xl * 0.7f, xr * 0.7f, wl, wr);
            L[i] = inL + P.p[2] * wl; R[i] = inR + P.p[2] * wr;
        }
        stereo = true;
    }
private:
    float fs = 48000, pre = 0; PedalParams P; dh::sp::Network net; SpringTank sp1, sp2; Line preL, preR;
};

} // namespace dh::gt
