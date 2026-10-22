#pragma once
// Efectos del ATFS1 en la topología del FS1R: inserción sobre la parte → envíos a Variación y a Reverb
// (con Variación → Reverb) → EQ de dos bandas → limitador suave. Los algoritmos son propios (los del
// FS1R son programas del DSP de Yamaha); los nombres de los tipos siguen la lista del original.

#include "Common.h"
#include "Fx.h"
#include "FsDsp.h"
#include <array>
#include <cmath>
#include <vector>

namespace dh::fs {

enum InsType : int { InsThru = 0, InsDistortion, InsOverdrive, InsAmpSim, InsAutoWah, InsPhaser, InsRotary, InsLoFi, InsCompressor, InsCount };
enum VarType : int { VarOff = 0, VarChorus, VarCeleste, VarFlanger, VarSymphonic, VarEnsDetune, VarTremolo, VarAutoPan, VarDelayLR, VarEcho, VarCrossDelay, VarCount };
enum RevType : int { RevOff = 0, RevHall1, RevHall2, RevRoom1, RevRoom2, RevStage, RevPlate, RevCanyon, RevCount };

struct FxParams
{
    int insType = InsThru; float insAmount = 0.5f, insMix = 1.0f;
    int varType = VarChorus; float varRate = 0.3f, varDepth = 0.5f, varSend = 0.2f, varToRev = 0.2f;
    int revType = RevHall1; float revTime = 0.5f, revSend = 0.25f;
    float eqLow = 0.0f, eqHigh = 0.0f;    // dB
};

// Línea con varias lecturas moduladas (coro, flanger, rotativo).
struct ModDelay
{
    DelayLine d;
    void prepare (float fs, float maxSec) { d.prepare ((int) (fs * maxSec) + 8); }
};

class Effects
{
public:
    void prepare (float sampleRate, int maxBlock)
    {
        fs = sampleRate;
        // Las constantes del auto-wah, el crossover del rotativo y el compresor se afinaron por muestra a 48 kHz:
        // se pasan a esta frecuencia para que suenen igual a 44,1, 96 o 192 kHz.
        auto at48 = [this] (float c) { return 1.0f - std::pow (1.0f - c, 48000.0f / fs); };
        kWahAtk = at48 (0.01f); kWahRel = at48 (0.0006f); kRotLp = at48 (0.06f); kCompAtk = at48 (0.02f); kCompRel = at48 (0.0004f);
        // Las líneas, la reverb (su LFO no vuelve a cero en reset()) y los filtros se rehacen, y los LFO de los efectos
        // arrancan en su fase inicial: el estado no depende de cuánto procesó la instancia antes (D1).
        mdL = mdR = rotL = rotR = ModDelay {};
        dlyL = dlyR = DelayLine {};
        reverb = PlateReverb {};
        lowL = lowR = highL = highR = OnePoleLP {};
        dcL = dcR = DcBlocker {};
        lfoA = 0.0f; lfoB = 0.25f; rotPhase = 0.0f; rotLow = 0.0f; toneL = toneR = 0.0f;
        for (auto* m : { &mdL, &mdR }) m->prepare (fs, 0.06f);
        for (auto* m : { &rotL, &rotR }) m->prepare (fs, 0.02f);
        dlyL.prepare ((int) (fs * 2.1f)); dlyR.prepare ((int) (fs * 2.1f));
        reverb.prepare (fs);
        varL.assign ((size_t) maxBlock, 0.0f); varR.assign ((size_t) maxBlock, 0.0f);
        revL.assign ((size_t) maxBlock, 0.0f); revR.assign ((size_t) maxBlock, 0.0f);
        tmpL.assign ((size_t) maxBlock, 0.0f); tmpR.assign ((size_t) maxBlock, 0.0f);
        lowL.setCutoff (250.0f, fs); lowR.setCutoff (250.0f, fs); highL.setCutoff (4000.0f, fs); highR.setCutoff (4000.0f, fs);
        dcL.setCutoff (8.0f, fs); dcR.setCutoff (8.0f, fs);
        reset();
    }
    void reset()
    {
        mdL.d.clear(); mdR.d.clear(); rotL.d.clear(); rotR.d.clear(); dlyL.clear(); dlyR.clear(); reverb.reset();
        for (auto& a : ap) a = 0.0f;
        wahA = wahB = env = compEnv = 0.0f; holdL = holdR = 0.0f; holdCount = 0;
        lowL.reset(); lowR.reset(); highL.reset(); highR.reset(); dcL.reset(); dcR.reset();
    }

    void process (float* L, float* R, int n, const FxParams& p)
    {
        if ((size_t) n > varL.size()) return;
        insertion (L, R, n, p);
        // Envíos.
        for (int i = 0; i < n; ++i) { varL[(size_t) i] = L[i] * p.varSend; varR[(size_t) i] = R[i] * p.varSend; revL[(size_t) i] = L[i] * p.revSend; revR[(size_t) i] = R[i] * p.revSend; }
        if (p.varType != VarOff)
        {
            variation (varL.data(), varR.data(), n, p);
            for (int i = 0; i < n; ++i) { revL[(size_t) i] += varL[(size_t) i] * p.varToRev; revR[(size_t) i] += varR[(size_t) i] * p.varToRev; L[i] += varL[(size_t) i]; R[i] += varR[(size_t) i]; }
        }
        if (p.revType != RevOff)
        {
            static const float SIZE[RevCount] = { 0, 0.78f, 0.86f, 0.5f, 0.62f, 0.72f, 0.7f, 0.97f };
            static const float DAMP[RevCount] = { 0, 0.45f, 0.35f, 0.6f, 0.5f, 0.4f, 0.15f, 0.3f };
            const float size = clampf (SIZE[p.revType] * (0.55f + 0.55f * p.revTime), 0.0f, 1.0f);
            for (int i = 0; i < n; ++i) { tmpL[(size_t) i] = revL[(size_t) i]; tmpR[(size_t) i] = revR[(size_t) i]; }
            reverb.process (tmpL.data(), tmpR.data(), n, size, DAMP[p.revType], 1.0f);
            for (int i = 0; i < n; ++i) { L[i] += tmpL[(size_t) i] - revL[(size_t) i]; R[i] += tmpR[(size_t) i] - revR[(size_t) i]; }
        }
        // EQ en estantes y limitador suave (la salida del original satura en el DAC de 18 bits).
        const float gl = dbToGain (p.eqLow) - 1.0f, gh = dbToGain (p.eqHigh) - 1.0f;
        for (int i = 0; i < n; ++i)
        {
            float l = dcL.process (L[i]), r = dcR.process (R[i]);
            const float ll = lowL.process (l), lr = lowR.process (r);
            l += gl * ll + gh * (l - highL.process (l)); r += gl * lr + gh * (r - highR.process (r));
            L[i] = softLimit (l); R[i] = softLimit (r);
        }
    }

private:
    float fs = 48000.0f;
    float kWahAtk = 0.01f, kWahRel = 0.0006f, kRotLp = 0.06f, kCompAtk = 0.02f, kCompRel = 0.0004f;
    ModDelay mdL, mdR, rotL, rotR;
    DelayLine dlyL, dlyR;
    PlateReverb reverb;
    std::vector<float> varL, varR, revL, revR, tmpL, tmpR;
    OnePoleLP lowL, lowR, highL, highR;
    DcBlocker dcL, dcR;
    float lfoA = 0.0f, lfoB = 0.25f, rotPhase = 0.0f, rotLow = 0.0f;
    std::array<float, 12> ap {};
    float wahA = 0, wahB = 0, env = 0, compEnv = 0, holdL = 0, holdR = 0; int holdCount = 0;
    float toneL = 0, toneR = 0;

    static float dbToGain (float db) { return fastExp2 (db * 0.16609640474f); }
    static float softLimit (float x) { const float a = std::fabs (x); return a < 0.8f ? x : std::copysign (0.8f + 0.2f * fastTanh ((a - 0.8f) * 5.0f), x); }
    float lfoStep (float& ph, float hz) { ph += hz / fs; if (ph >= 1.0f) ph -= 1.0f; return ph; }
    static float tri (float ph) { return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; }

    void insertion (float* L, float* R, int n, const FxParams& p)
    {
        const float a = clampf (p.insAmount, 0.0f, 1.0f), mix = clampf (p.insMix, 0.0f, 1.0f);
        switch (p.insType)
        {
            case InsDistortion: case InsOverdrive: case InsAmpSim:
            {
                const float drive = p.insType == InsDistortion ? 1.0f + 60.0f * a * a : (p.insType == InsOverdrive ? 1.0f + 14.0f * a : 1.0f + 30.0f * a);
                const float toneHz = p.insType == InsAmpSim ? 3800.0f : 7000.0f - 3000.0f * a;
                const float g = 1.0f - fastExp (-kTwoPi * toneHz / fs);
                const float comp = 1.0f / std::sqrt (drive) * (p.insType == InsDistortion ? 1.6f : 1.3f);
                for (int i = 0; i < n; ++i)
                {
                    auto shape = [&] (float x) {
                        x *= drive;
                        if (p.insType == InsOverdrive) return x > 0.0f ? fastTanh (x) : fastTanh (0.7f * x) / 0.7f * 0.85f;   // asimétrico, a válvula
                        if (p.insType == InsAmpSim) return fastTanh (x + 0.15f) - 0.1489f;
                        x = clampf (x, -1.0f, 1.0f);
                        return 1.5f * x - 0.5f * x * x * x;   // cúbica dura
                    };
                    float l = shape (clampf (L[i], -4.0f, 4.0f)), r = shape (clampf (R[i], -4.0f, 4.0f));
                    toneL += (l - toneL) * g; toneR += (r - toneR) * g;
                    L[i] += (toneL * comp - L[i]) * mix; R[i] += (toneR * comp - R[i]) * mix;
                }
                break;
            }
            case InsAutoWah:
            {
                for (int i = 0; i < n; ++i)
                {
                    const float x = 0.5f * (L[i] + R[i]);
                    const float ax = std::fabs (x);
                    env += (ax - env) * (ax > env ? kWahAtk : kWahRel);
                    const float fc = 300.0f * fastExp2 (clampf (env * 6.0f * (0.3f + a), 0.0f, 4.5f));
                    const float f = 2.0f * std::sin (kPi * std::min (fc, 0.2f * fs) / fs), q = 0.25f;
                    const float hp = x - wahA - q * wahB; wahB += f * hp; wahA += f * wahB;
                    const float y = wahB * 1.6f;
                    L[i] += (y - L[i]) * mix; R[i] += (y - R[i]) * mix;
                }
                break;
            }
            case InsPhaser:
            {
                for (int i = 0; i < n; ++i)
                {
                    const float m = 0.5f + 0.5f * tri (lfoStep (lfoA, 0.15f + 2.5f * a * a));
                    const float fc = 200.0f * fastExp2 (m * 4.0f);
                    const float t = fastTan (kPi * fc / fs), c = (t - 1.0f) / (t + 1.0f);
                    float x[2] = { L[i] + 0.55f * ap[10], R[i] + 0.55f * ap[11] };
                    for (int ch = 0; ch < 2; ++ch)
                    {
                        float v = x[ch];
                        for (int s = 0; s < 5; ++s) { const size_t k = (size_t) (ch * 5 + s); const float y = c * v + ap[k]; ap[k] = v - c * y; v = y; }
                        ap[(size_t) 10 + (size_t) ch] = v;
                        x[ch] = v;
                    }
                    L[i] = 0.7f * (L[i] + x[0] * mix); R[i] = 0.7f * (R[i] + x[1] * mix);
                }
                break;
            }
            case InsRotary:
            {
                const float speed = a < 0.5f ? 0.8f : 6.7f;
                for (int i = 0; i < n; ++i)
                {
                    const float ph = lfoStep (rotPhase, speed);
                    const float x = 0.5f * (L[i] + R[i]);
                    rotLow += (x - rotLow) * kRotLp;
                    const float hi = x - rotLow;
                    rotL.d.push (hi); rotR.d.push (hi);
                    const float s = Tables::get().sin2pi (ph), c = Tables::get().sin2pi (ph + 0.25f);
                    const float yl = rotL.d.tap (fs * (0.0025f + 0.0012f * s)) * (0.75f + 0.25f * c);
                    const float yr = rotR.d.tap (fs * (0.0025f - 0.0012f * s)) * (0.75f - 0.25f * c);
                    L[i] += (rotLow + yl - L[i]) * mix; R[i] += (rotLow + yr - R[i]) * mix;
                }
                break;
            }
            case InsLoFi:
            {
                const int hold = 1 + (int) (a * 10.0f);
                const float steps = 8.0f + (1.0f - a) * 248.0f;
                for (int i = 0; i < n; ++i)
                {
                    if (++holdCount >= hold) { holdCount = 0; holdL = std::round (L[i] * steps) / steps; holdR = std::round (R[i] * steps) / steps; }
                    L[i] += (holdL - L[i]) * mix; R[i] += (holdR - R[i]) * mix;
                }
                break;
            }
            case InsCompressor:
            {
                const float thr = 0.5f - 0.45f * a, ratio = 2.0f + 8.0f * a;
                for (int i = 0; i < n; ++i)
                {
                    const float lv = std::max (std::fabs (L[i]), std::fabs (R[i]));
                    compEnv += (lv - compEnv) * (lv > compEnv ? kCompAtk : kCompRel);
                    const float g = compEnv > thr ? std::pow (compEnv / thr, 1.0f / ratio - 1.0f) : 1.0f;
                    const float makeup = 1.0f + a * 1.5f;
                    L[i] += (L[i] * g * makeup - L[i]) * mix; R[i] += (R[i] * g * makeup - R[i]) * mix;
                }
                break;
            }
            default: break;
        }
    }

    void variation (float* L, float* R, int n, const FxParams& p)
    {
        const float rate = 0.05f + 6.0f * p.varRate * p.varRate, depth = clampf (p.varDepth, 0.0f, 1.0f);
        switch (p.varType)
        {
            case VarChorus: case VarCeleste: case VarSymphonic: case VarEnsDetune: case VarFlanger:
            {
                const bool fl = p.varType == VarFlanger;
                const float base = fl ? 0.0015f : (p.varType == VarEnsDetune ? 0.012f : 0.009f);
                const float dep = fl ? 0.0014f * depth : 0.0045f * depth;
                const float fb = fl ? 0.6f * depth : 0.0f;
                const float r = p.varType == VarEnsDetune ? 0.0f : (p.varType == VarCeleste ? rate * 0.6f : rate);
                for (int i = 0; i < n; ++i)
                {
                    const float pa = lfoStep (lfoA, r), pb = lfoStep (lfoB, r * 1.07f);
                    float ml = p.varType == VarCeleste ? Tables::get().sin2pi (pa) : tri (pa);
                    float mr = p.varType == VarCeleste ? Tables::get().sin2pi (pb + 0.25f) : tri (std::fmod (pb + 0.5f, 1.0f));
                    float dl = base + dep * ml, dr = base + dep * mr;
                    if (p.varType == VarEnsDetune) { dl = base + 0.002f * depth; dr = base + 0.0035f * depth; }
                    float yl = mdL.d.tap (std::max (2.0f, dl * fs)), yr = mdR.d.tap (std::max (2.0f, dr * fs));
                    if (p.varType == VarSymphonic) { yl = 0.6f * (yl + mdL.d.tap ((base * 1.7f + dep * mr) * fs)); yr = 0.6f * (yr + mdR.d.tap ((base * 1.7f + dep * ml) * fs)); }
                    mdL.d.push (L[i] + yl * fb); mdR.d.push (R[i] + yr * fb);
                    L[i] = yl; R[i] = yr;
                }
                break;
            }
            case VarTremolo: case VarAutoPan:
            {
                for (int i = 0; i < n; ++i)
                {
                    const float m = Tables::get().sin2pi (lfoStep (lfoA, rate));
                    if (p.varType == VarTremolo) { const float g = 1.0f - depth * 0.5f * (1.0f + m); L[i] *= g; R[i] *= g; }
                    else { const float x = 0.5f * (L[i] + R[i]); L[i] = x * (1.0f - depth * m); R[i] = x * (1.0f + depth * m); }
                }
                break;
            }
            default:   // retardos: tiempo con la velocidad, realimentación con la profundidad
            {
                const float t = 0.08f + 0.9f * (1.0f - p.varRate);
                const float fb = 0.75f * depth;
                for (int i = 0; i < n; ++i)
                {
                    const float yl = dlyL.tap (t * fs);
                    const float yr = dlyR.tap (t * fs * (p.varType == VarDelayLR ? 0.75f : 1.0f));
                    if (p.varType == VarCrossDelay) { dlyL.push (L[i] + yr * fb); dlyR.push (R[i] + yl * fb); }
                    else if (p.varType == VarEcho) { dlyL.push (0.5f * (L[i] + R[i]) + yr * fb); dlyR.push (yl); }
                    else { dlyL.push (L[i] + yl * fb); dlyR.push (R[i] + yr * fb); }
                    L[i] = yl; R[i] = yr;
                }
                break;
            }
        }
    }
};

} // namespace dh::fs
