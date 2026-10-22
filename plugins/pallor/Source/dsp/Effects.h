#pragma once
// Los efectos de Pallor (docs/research.md §5, §7): dos envíos y el bus maestro.
//
// Envío A, "Space": la sala de los años 80.
//   - Gated: una sala (la red de Halation en modo sala) aplastada por un compresor rápido y cortada por una compuerta
//     que abre con cada golpe del envío, se sostiene `gate` ms y cierra de golpe: la caja de 1980-85.
//   - Nonlinear: el programa "no lineal" de las reverbs digitales de 1981-83: reflexiones densas al azar (ruido
//     aterciopelado: taps esparcidos de signo al azar) durante `gate` ms con una envolvente plana, que cae o que sube,
//     y un corte seco. Reverse: la misma con la envolvente que sube.
//   - Plate (la de Dattorro de shared/dsp), Spring (el par de resortes del AT700), Hall y Room: la red de Halation con
//     el ancho de banda (12 kHz) y los 12 bits de las primeras reverbs digitales.
// Envío B, "Echo": delay digital de 12 bits, eco de cinta (cabezas múltiples, wow y flutter, la histéresis magnética de
//   Tarnish en el lazo), delay BBD (el filtro baja con el tiempo, como el reloj), chorus BBD, ensemble de tres líneas,
//   flanger "jet" y phaser de seis etapas. Los de modulación devuelven sólo la señal húmeda: sumada al seco en el bus
//   principal hace el peine (el flanger) o las muescas (el phaser), como con un aparato en un envío de la consola.
// Maestro: lo-fi (el muestreador de 12 bits a 26/40 kHz con el crusher de Tarnish, o 8 bits μ), cinta (histéresis de
//   Jiles-Atherton de Tarnish a ×4 con el sobremuestreo de Lacquer), compresor de bus (el de Lacquer, con los tiempos
//   de un compresor de bus de consola), excitador y volumen, con un techo suave.

#include "Base.h"
#include "Fx.h"                                              // shared/dsp: PlateReverb
#include "../../../halation/Source/dsp/Network.h"            // dh::sp::Network
#include "../../../at700/Source/dsp/Modules700.h"            // dh::Spring711
#include "../../../tarnish/Source/dsp/Hysteresis.h"          // dh::tn::Hysteresis
#include "../../../tarnish/Source/dsp/Lofi.h"                // dh::tn::Crusher
#include "../../../lacquer/Source/dsp/Dynamics.h"            // dh::lq::Compressor
#include "../../../lacquer/Source/dsp/Saturation.h"          // dh::lq::Os4

namespace dh::pal {

// Línea de retardo simple con lectura de Hermite (tamaño potencia de 2).
struct Line
{
    std::vector<float> b; int mask = 0, w = 0;
    void prepare (int maxSamples) { int n = 1; while (n < maxSamples + 8) n <<= 1; b.assign ((size_t) n, 0.0f); mask = n - 1; w = 0; }
    void clear() { std::fill (b.begin(), b.end(), 0.0f); w = 0; }
    inline void push (float x) { b[(size_t) w] = x; w = (w + 1) & mask; }
    inline float at (int d) const { return b[(size_t) ((w - d) & mask)]; }
    inline float read (float d) const
    {
        d = clampf (d, 1.0f, (float) (mask - 4));
        const float fd = std::floor (d); const int i = (int) fd; const float t = d - fd;
        const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + y1;
    }
};

// ───────────────────────── Envío A: Space ─────────────────────────
enum SpaceType { kGated, kNonlinear, kReverse, kPlate, kSpring, kHall, kRoom, kNumSpaceTypes };
inline const char* spaceTypeName (int t) { static const char* n[] = { "Gated", "Nonlinear", "Reverse", "Plate", "Spring", "Hall", "Room" }; return n[std::clamp (t, 0, 6)]; }

struct SpaceParams { int type = kGated; float size = 0.5f, predelayMs = 0.0f, gateMs = 250.0f, shape = 0.0f, tone = 0.6f, lowcutHz = 150.0f, width = 1.0f; };

class Space
{
public:
    static constexpr int kMaxTaps = 1024;
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        preL.prepare ((int) (0.26f * fs)); preR.prepare ((int) (0.26f * fs));
        net.prepare (fs, 7u, 420.0f);
        plate.prepare (fs);
        spring.prepare (fs);
        nlBuf.prepare ((int) (1.25f * fs));
        for (auto& a : diff) a.prepare ((int) (0.01f * fs));
        sprR.prepare ((int) (0.002f * fs));
        tmpL.assign (8192, 0.0f); tmpR.assign (8192, 0.0f);
        reset();
        set (P, true);
    }
    void reset()
    {
        preL.clear(); preR.clear(); net.reset(); plate.reset(); spring = dh::Spring711(); spring.prepare (fs);
        nlBuf.clear(); for (auto& a : diff) a.clear(); sprR.clear();
        hpL.reset(); hpR.reset(); lpL.reset(); lpR.reset(); lp2L.reset(); lp2R.reset();
        envF = envS = 0; gate = 0; hold = 0; refr = 0; sq = 0;
    }
    void set (const SpaceParams& p, bool force = false)
    {
        const bool typeChanged = p.type != P.type;
        const bool tapsChanged = force || typeChanged || std::fabs (p.gateMs - P.gateMs) > 0.5f || std::fabs (p.shape - P.shape) > 1e-3f;
        P = p;
        if (typeChanged && ! force) reset();
        hpL.set (std::max (20.0f, P.lowcutHz), fs); hpR.set (std::max (20.0f, P.lowcutHz), fs);
        const float lpHz = expMap (P.tone, 2500.0f, 16000.0f);
        lpL.set (lpHz, 0.707f, fs); lpR.set (lpHz, 0.707f, fs);
        lp2L.set (12000.0f, 0.707f, fs); lp2R.set (12000.0f, 0.707f, fs);
        preN = clampf (P.predelayMs, 0.0f, 250.0f) * 1e-3f * fs;
        if (P.type == kGated || P.type == kHall || P.type == kRoom)
        {
            dh::sp::NetParams np;
            np.space = 1.0f; np.diffusion = 0.75f; np.modDepth = 0.2f; np.modRate = 0.35f; np.width = P.width;
            np.lowCutHz = 40.0f; np.hfRatio = 0.15f + 0.7f * P.tone; np.lfRatio = 1.0f;
            if (P.type == kGated) { np.room = dh::sp::RoomType::chamber; np.size = 0.6f + 0.8f * P.size; np.decayS = 1.2f + 3.0f * P.size; }
            else if (P.type == kHall) { np.room = dh::sp::RoomType::hall; np.size = 0.6f + 1.2f * P.size; np.decayS = 1.0f + 5.0f * P.size; }
            else { np.room = dh::sp::RoomType::room; np.size = 0.5f + 1.0f * P.size; np.decayS = 0.3f + 1.7f * P.size; }
            net.set (np);
        }
        gateN = clampf (P.gateMs, 40.0f, 1200.0f) * 1e-3f * fs;
        gateRel = 1.0f / ((3.0f + 57.0f * (0.5f + 0.5f * clampf (P.shape, -1.0f, 1.0f))) * 1e-3f * fs);
        if (tapsChanged && (P.type == kNonlinear || P.type == kReverse)) makeTaps();
    }

    // En el lugar: entra el bus del envío, sale la señal húmeda.
    void process (float* L, float* R, int n)
    {
        // Las salas de los 80 vuelven fuertes: en la caja de 1980-85 la sala es tanto sonido como el parche.
        const float g = (P.type == kGated || P.type == kNonlinear || P.type == kReverse) ? 1.58f : 1.26f;
        for (int i0 = 0; i0 < n; i0 += 8192)
        {
            const int m = std::min (8192, n - i0);
            processChunk (L + i0, R + i0, m);
            for (int i = i0; i < i0 + m; ++i) { L[i] *= g; R[i] *= g; }
        }
    }

private:
    float fs = 48000;
    SpaceParams P;
    Line preL, preR, nlBuf, sprR; Line diff[4];
    dh::sp::Network net;
    dh::PlateReverb plate;
    dh::Spring711 spring;
    Hp1 hpL, hpR; Svf lpL, lpR, lp2L, lp2R;
    std::vector<float> tmpL, tmpR;
    float preN = 0, gateN = 12000, gateRel = 0.001f;
    float envF = 0, envS = 0, gate = 0, hold = 0, refr = 0, sq = 0;
    std::array<int, kMaxTaps> tapPosL {}, tapPosR {}; std::array<float, kMaxTaps> tapGL {}, tapGR {}; int nTaps = 0;

    // Ruido aterciopelado: un tap por celda de largo fijo, en un lugar al azar de la celda, con signo al azar.
    void makeTaps()
    {
        const float T = clampf (P.gateMs, 40.0f, 1200.0f) * 1e-3f;
        nTaps = std::clamp ((int) (T * 800.0f), 8, kMaxTaps);
        const float cell = T * fs / (float) nTaps;
        const float sh = P.type == kReverse ? 1.0f : clampf (P.shape, -1.0f, 1.0f);
        double eL = 0, eR = 0;
        for (int k = 0; k < nTaps; ++k)
        {
            const float u = ((float) k + 0.5f) / (float) nTaps;
            // Forma: −1 cae (exponencial), 0 plana, +1 sube (el programa al revés).
            float env = 1.0f;
            if (sh < 0) env = 1.0f + (-sh) * (std::exp (-4.0f * u) - 1.0f);
            else if (sh > 0) env = 1.0f + sh * (std::pow (0.04f + u, 1.6f) / std::pow (1.04f, 1.6f) - 1.0f);
            tapPosL[(size_t) k] = std::max (1, (int) (((float) k + (float) hashUni (91, (uint64_t) k, 1)) * cell));
            tapPosR[(size_t) k] = std::max (1, (int) (((float) k + (float) hashUni (92, (uint64_t) k, 1)) * cell));
            tapGL[(size_t) k] = env * (hashUni (93, (uint64_t) k, 2) < 0.5 ? -1.0f : 1.0f);
            tapGR[(size_t) k] = env * (hashUni (94, (uint64_t) k, 2) < 0.5 ? -1.0f : 1.0f);
            eL += (double) tapGL[(size_t) k] * tapGL[(size_t) k]; eR += (double) tapGR[(size_t) k] * tapGR[(size_t) k];
        }
        // Energía de la respuesta igual a 1.5: un golpe sale con la misma energía (más denso, más bajo en pico).
        const float gL = (float) std::sqrt (1.5 / std::max (1e-9, eL)), gR = (float) std::sqrt (1.5 / std::max (1e-9, eR));
        for (int k = 0; k < nTaps; ++k) { tapGL[(size_t) k] *= gL; tapGR[(size_t) k] *= gR; }
    }

    void processChunk (float* L, float* R, int n)
    {
        // Pre-retardo y pasa altos de entrada.
        for (int i = 0; i < n; ++i)
        {
            preL.push (hpL.process (L[i])); preR.push (hpR.process (R[i]));
            const float key = std::max (std::fabs (L[i]), std::fabs (R[i]));
            tmpL[(size_t) i] = preN >= 1.0f ? preL.read (preN) : preL.at (1);
            tmpR[(size_t) i] = preN >= 1.0f ? preR.read (preN) : preR.at (1);
            L[i] = key;                                                     // la llave de la compuerta, sin pre-retardo
        }
        switch (P.type)
        {
            case kGated:
                for (int i = 0; i < n; ++i)
                {
                    float oL, oR;
                    net.process (tmpL[(size_t) i], tmpR[(size_t) i], oL, oR);
                    // La sala aplastada: un compresor rápido sobre la cola la vuelve una pared.
                    const float a = std::max (std::fabs (oL), std::fabs (oR));
                    sq = a > sq ? a + 0.3f * (sq - a) : a + 0.9995f * (sq - a);
                    const float g = 1.0f / (1.0f + 5.0f * sq);
                    // Compuerta con la llave de los golpes: abre en cada ataque y sostiene `gate` ms.
                    const float k = L[i];
                    envF = k > envF ? k : envF * 0.9985f;
                    envS += (k - envS) * 0.0015f;
                    if (refr > 0) refr -= 1.0f;
                    if (envF > 0.008f && envF > 2.2f * envS + 1e-4f && refr <= 0) { hold = gateN; refr = 0.02f * fs; }
                    if (hold > 0) { hold -= 1.0f; gate = std::min (1.0f, gate + 1.0f / (0.0008f * fs)); }
                    else gate = std::max (0.0f, gate - gateRel);
                    const float gg = gate * gate * (3.0f - 2.0f * gate) * g * 2.2f;
                    L[i] = lpL.lowpass (oL * gg); R[i] = lpR.lowpass (oR * gg);
                }
                break;
            case kNonlinear:
            case kReverse:
            {
                const float wM = 0.5f * (1.0f - P.width), wS = 1.0f - wM;
                for (int i = 0; i < n; ++i)
                {
                    nlBuf.push (0.5f * (tmpL[(size_t) i] + tmpR[(size_t) i]));
                    float yl = 0, yr = 0;
                    for (int k = 0; k < nTaps; ++k) { yl += tapGL[(size_t) k] * nlBuf.at (tapPosL[(size_t) k]); yr += tapGR[(size_t) k] * nlBuf.at (tapPosR[(size_t) k]); }
                    yl = allpass (diff[0], 0.0031f, yl); yl = allpass (diff[1], 0.0047f, yl);
                    yr = allpass (diff[2], 0.0037f, yr); yr = allpass (diff[3], 0.0053f, yr);
                    yl = lpL.lowpass (yl); yr = lpR.lowpass (yr);
                    L[i] = yl * wS + yr * wM; R[i] = yr * wS + yl * wM;
                }
                break;
            }
            case kPlate:
            {
                std::vector<float>& a = tmpL; std::vector<float>& b = tmpR;
                for (int i = 0; i < n; ++i) { L[i] = a[(size_t) i]; R[i] = b[(size_t) i]; }
                plate.process (a.data(), b.data(), n, 0.2f + 0.8f * P.size, 1.0f - P.tone, 1.0f);
                for (int i = 0; i < n; ++i) { const float yl = a[(size_t) i] - L[i], yr = b[(size_t) i] - R[i]; L[i] = lpL.lowpass (yl) * 1.3f; R[i] = lpR.lowpass (yr) * 1.3f; }
                break;
            }
            case kSpring:
                for (int i = 0; i < n; ++i)
                {
                    const float y = spring.process (0.5f * (tmpL[(size_t) i] + tmpR[(size_t) i]) * (0.6f + 0.8f * P.size));
                    sprR.push (y);
                    const float yl = lpL.lowpass (y), yr = lpR.lowpass (sprR.at ((int) (0.0007f * fs) + 1));
                    L[i] = yl * 1.1f; R[i] = (yl + (yr - yl) * P.width) * 1.1f;
                }
                break;
            default:   // Hall y Room: la red con el ancho de banda y los 12 bits de las primeras digitales
                for (int i = 0; i < n; ++i)
                {
                    float oL, oR;
                    net.process (tmpL[(size_t) i], tmpR[(size_t) i], oL, oR);
                    oL = quantLinear (lp2L.lowpass (oL) * 0.5f, 2047.0f) * 2.0f;
                    oR = quantLinear (lp2R.lowpass (oR) * 0.5f, 2047.0f) * 2.0f;
                    L[i] = lpL.lowpass (oL); R[i] = lpR.lowpass (oR);
                }
                break;
        }
    }
    inline float allpass (Line& d, float sec, float x)
    {
        const int len = std::max (1, (int) (sec * fs));
        const float del = d.at (len);
        const float v = x + 0.55f * del;
        d.push (v);
        return del - 0.55f * v;
    }
};

// ───────────────────────── Envío B: Echo ─────────────────────────
enum EchoType { kDigital, kTape, kBbd, kChorus, kEnsemble, kFlanger, kPhaser, kNumEchoTypes };
inline const char* echoTypeName (int t) { static const char* n[] = { "Digital 12-bit", "Tape", "BBD", "Chorus", "Ensemble", "Jet Flanger", "Phaser" }; return n[std::clamp (t, 0, 6)]; }

struct EchoParams { int type = kDigital; float timeBeats = 0.75f, feedback = 0.35f, tone = 0.5f, rate = 0.3f, depth = 0.5f, width = 1.0f; };

class Echo
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        lineL.prepare ((int) (2.6f * fs)); lineR.prepare ((int) (2.6f * fs));
        hyst.set ({ 0.0, 0.5, 0.0, 1.0, false });
        reset();
        set (P, true);
    }
    void reset()
    {
        lineL.clear(); lineR.clear(); fbL = fbR = 0; lfo = 0; lfo2 = 0; lfo3 = 0; drift = 0; hyst.reset();
        for (auto* f : { &tl1, &tl2, &tr1, &tr2 }) f->reset(); hpL.reset(); hpR.reset();
        apL.fill (0.0f); apR.fill (0.0f); phFbL = phFbR = 0; t = 0; dSm = -1;
    }
    void set (const EchoParams& p, bool force = false)
    {
        if (p.type != P.type && ! force) reset();
        P = p;
        const float lpHz = expMap (P.tone, 1800.0f, 14000.0f);      // (BBD: el filtro sigue al tiempo, en process)
        tl1.set (lpHz, 0.707f, fs); tl2.set (lpHz, 0.707f, fs); tr1.set (lpHz, 0.707f, fs); tr2.set (lpHz, 0.707f, fs);
        hpL.set (P.type == kTape ? 110.0f : 70.0f, fs); hpR.set (P.type == kTape ? 110.0f : 70.0f, fs);
        // La histéresis sube el nivel con el drive (como la cinta): se compensa, así la ganancia del lazo en señal chica es
        // la realimentación y sólo los picos se aplastan (si no, con la realimentación alta el eco se dispara).
        if (P.type == kTape && (force || std::fabs (P.feedback - hystFb) > 1e-4f))
        {
            const double d = 1.0 + 9.0 * P.feedback;
            hyst.set ({ d, 0.55, 0.0, 1.0, false }); hystFb = P.feedback; hystMk = (float) std::pow (10.0, -d / 20.0);
        }
    }

    void process (float* L, float* R, int n, double bpm)
    {
        const float beatS = (float) (60.0 / std::max (20.0, bpm));
        float dTarget = clampf (P.timeBeats * beatS * fs, 2.0f, 2.5f * fs);
        if (P.type == kBbd)
        {
            dTarget = std::min (dTarget, 0.6f * fs);
            const float ms = dTarget / fs * 1000.0f;
            const float hz = clampf (6000.0f * std::pow (100.0f / std::max (20.0f, ms), 0.56f), 1500.0f, 9000.0f) * expMap (P.tone, 0.6f, 1.5f);
            tl1.set (hz, 0.6f, fs); tl2.set (hz, 0.6f, fs); tr1.set (hz, 0.6f, fs); tr2.set (hz, 0.6f, fs);
        }
        if (dSm < 0) dSm = dTarget;
        const float glide = 1.0f - fastExp (-1.0f / (0.06f * fs));
        for (int i = 0; i < n; ++i)
        {
            dSm += (dTarget - dSm) * glide;
            float l = L[i], r = R[i];
            switch (P.type)
            {
                case kDigital: digital (l, r); break;
                case kTape: tape (l, r); break;
                case kBbd: bbd (l, r); break;
                case kChorus: chorus (l, r); break;
                case kEnsemble: ensemble (l, r); break;
                case kFlanger: flanger (l, r); break;
                default: phaser (l, r); break;
            }
            L[i] = l; R[i] = r; ++t;
        }
    }

private:
    float fs = 48000; EchoParams P;
    Line lineL, lineR;
    Svf tl1, tl2, tr1, tr2; Hp1 hpL, hpR;
    dh::tn::Hysteresis hyst;
    float fbL = 0, fbR = 0, lfo = 0, lfo2 = 0, lfo3 = 0, drift = 0, dSm = -1, hystFb = -1, hystMk = 1;
    std::array<float, 6> apL {}, apR {}; float phFbL = 0, phFbR = 0;
    int64_t t = 0;

    // Delay digital de 12 bits: el convertidor cuantiza lo que entra a la memoria, y el ancho de banda corto.
    inline void digital (float& l, float& r)
    {
        const float yl = lineL.read (dSm), yr = lineR.read (dSm);
        const float cross = clampf (P.width, 0.0f, 1.0f);
        const float inL = 0.5f * (l + r);
        const float fl = tl1.lowpass (hpL.process (inL * (1.0f - 0.5f * cross) + P.feedback * (yl * (1.0f - cross) + yr * cross)));
        const float fr = tr1.lowpass (hpR.process (inL * (1.0f - cross) * 0.5f + P.feedback * (yr * (1.0f - cross) + yl * cross)));
        lineL.push (quantLinear (fastTanh (fl), 2047.0f)); lineR.push (quantLinear (fastTanh (fr), 2047.0f));
        l = yl; r = yr;
    }
    // Eco de cinta: tres cabezas (1/3, 2/3 y el tiempo), wow, flutter y deriva; histéresis en el lazo.
    inline void tape (float& l, float& r)
    {
        lfo += 0.55f / fs; lfo -= std::floor (lfo); lfo2 += 6.3f / fs; lfo2 -= std::floor (lfo2);
        drift += ((float) hashUni (5, (uint64_t) (t >> 11), 0) * 2.0f - 1.0f - drift) * (1.0f / (0.8f * fs));
        const float w = 1.0f + P.rate * (0.004f * sin2pi (lfo) + 0.0009f * sin2pi (lfo2) + 0.002f * drift);
        const float d = dSm * w;
        const float h3 = lineL.read (d), h2 = lineL.read (d * 0.6667f), h1 = lineL.read (d * 0.3333f);
        const float heads = (h3 + P.depth * 0.8f * (h2 + 0.7f * h1)) / (1.0f + P.depth * 1.36f);   // suma de las cabezas, normalizada
        const float in = 0.5f * (l + r);
        float v = in + std::min (P.feedback, 0.92f) * fbL;
        v = (float) hyst.process ((double) v) * hystMk;
        v = tl2.lowpass (tl1.lowpass (hpL.process (v)));
        lineL.push (v);
        fbL = heads;
        lineR.push (heads);
        l = heads; r = lineR.read (1.0f + P.width * 0.011f * fs);
    }
    inline void bbd (float& l, float& r)
    {
        const float yl = lineL.read (dSm), yr = lineR.read (dSm);
        const float in = 0.5f * (l + r);
        float fl = tl2.lowpass (tl1.lowpass (in + P.feedback * yr));
        float fr = tr2.lowpass (tr1.lowpass (P.feedback * yl + in * (1.0f - P.width)));
        fl = fastTanh (1.4f * fl) / 1.4f + 3e-5f * (float) (hashUni (8, (uint64_t) t, 1) - 0.5);
        fr = fastTanh (1.4f * fr) / 1.4f + 3e-5f * (float) (hashUni (8, (uint64_t) t, 2) - 0.5);
        lineL.push (hpL.process (fl)); lineR.push (hpR.process (fr));
        l = yl; r = yr;
    }
    inline void chorus (float& l, float& r)
    {
        lfo += (0.15f + 1.6f * P.rate) / fs; lfo -= std::floor (lfo);
        const float tri = triFromPhase (lfo);
        const float c = 0.0065f * fs, dep = (0.0005f + 0.0028f * P.depth) * fs;
        lineL.push (tl1.lowpass (0.5f * (l + r)));
        const float a = lineL.read (c + dep * tri), b = lineL.read (c - dep * tri);
        l = tl2.lowpass (a); r = tr2.lowpass (a + (b - a) * P.width);
    }
    inline void ensemble (float& l, float& r)
    {
        lfo += 0.6f * (0.4f + P.rate) / fs; lfo -= std::floor (lfo);
        lfo2 += 6.0f / fs; lfo2 -= std::floor (lfo2);
        const float c = 0.009f * fs, d1 = (0.0008f + 0.0018f * P.depth) * fs, d2 = 0.00025f * fs;
        lineL.push (tl1.lowpass (0.5f * (l + r)));
        float tap[3];
        for (int k = 0; k < 3; ++k)
        {
            const float ph = (float) k / 3.0f;
            tap[k] = lineL.read (c + d1 * sin2pi (lfo + ph) + d2 * sin2pi (lfo2 + ph));
        }
        l = tl2.lowpass (tap[0] + 0.7f * tap[1]) * 0.7f;
        r = tr2.lowpass (tap[2] + 0.7f * tap[1]) * 0.7f;
        r = l + (r - l) * P.width;
    }
    // Flanger "jet": un retardo de 0,25 a ~8 ms barrido lento, con realimentación.
    inline void flanger (float& l, float& r)
    {
        lfo += (0.02f + 1.0f * P.rate * P.rate) / fs; lfo -= std::floor (lfo);
        const float u0 = 0.5f + 0.5f * triFromPhase (lfo), u1 = 0.5f + 0.5f * triFromPhase (lfo + 0.25f * P.width - std::floor (lfo + 0.25f * P.width));
        const float span = (0.0004f + 0.0076f * P.depth) * fs, base = 0.00025f * fs;
        const float dl = base + span * u0 * u0, dr = base + span * u1 * u1;
        const float fb = clampf (P.feedback, 0.0f, 0.95f);
        const float yl = lineL.read (dl), yr = lineR.read (dr);
        lineL.push (fastTanh (0.5f * (l + r) + fb * yl)); lineR.push (fastTanh (0.5f * (l + r) + fb * yr));
        l = yl; r = yr;
    }
    // Phaser de seis pasa todos de primer orden, barridos en escala exponencial, con realimentación.
    inline void phaser (float& l, float& r)
    {
        lfo += (0.03f + 2.0f * P.rate * P.rate) / fs; lfo -= std::floor (lfo);
        auto run = [&] (std::array<float, 6>& z, float& fb, float x, float u) {
            const float fc = 180.0f * fastExp2 (4.6f * P.depth * u);
            const float tt = prewarp (fc, fs), a = (tt - 1.0f) / (tt + 1.0f);
            float v = x + fb * clampf (P.feedback, 0.0f, 0.85f);
            for (auto& s : z) { const float y = a * v + s; s = v - a * y; v = y; }
            fb = fastTanh (v);
            return v;
        };
        const float x = 0.5f * (l + r);
        const float u0 = 0.5f + 0.5f * sin2pi (lfo), u1 = 0.5f + 0.5f * sin2pi (lfo + 0.25f * P.width);
        l = run (apL, phFbL, x, u0); r = run (apR, phFbR, x, u1);
    }
};

// ───────────────────────── Bus maestro ─────────────────────────
enum LofiType { kLofiOff, kLofi26, kLofi40, kLofi8bit, kLofi11k, kNumLofi };
inline const char* lofiName (int t) { static const char* n[] = { "Off", "12-bit 26k", "12-bit 40k", "8-bit 22k", "8-bit 11k" }; return n[std::clamp (t, 0, 4)]; }

struct MasterParams
{
    int lofi = kLofiOff; float lofiMix = 1.0f;
    bool tape = false; float tapeDriveDb = 0.0f;
    float comp = 0.0f; int ratio = 1, attack = 3, release = 4; float compMix = 1.0f;
    float exciter = 0.0f, exciterHz = 3500.0f;
    float volumeDb = 0.0f;
};
inline const char* ratioName (int i) { static const char* n[] = { "2:1", "4:1", "10:1" }; return n[std::clamp (i, 0, 2)]; }
inline const char* attackName (int i) { static const char* n[] = { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }; return n[std::clamp (i, 0, 5)]; }
inline const char* releaseName (int i) { static const char* n[] = { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" }; return n[std::clamp (i, 0, 4)]; }

class Master
{
public:
    static constexpr int kTapeLatency = dh::lq::Os4::latency();
    void prepare (float sampleRate) { fs = sampleRate; crushL.prepare (fs); crushR.prepare (fs); comp.prepare (fs); compSlow.prepare (fs); tapeDcL.setHz (5.0, fs); tapeDcR.setHz (5.0, fs); reset(); set (P, true); }
    void reset()
    {
        crushL.reset(); crushR.reset(); osL.reset(); osR.reset(); hyL.reset(); hyR.reset(); comp.reset(); compSlow.reset(); tapeDcL.reset(); tapeDcR.reset();
        exL.reset(); exR.reset(); ex2L.reset(); ex2R.reset(); grDb = 0;
    }
    void set (const MasterParams& p, bool force = false)
    {
        const bool lofiChanged = p.lofi != P.lofi, tapeChanged = std::fabs (p.tapeDriveDb - P.tapeDriveDb) > 1e-3f || p.tape != P.tape;
        P = p;
        if (force || lofiChanged)
        {
            if (P.lofi == kLofi26) { crushL.set (12, 26040.0, true, 0.0, false); crushR.set (12, 26040.0, true, 0.0, false); }
            else if (P.lofi == kLofi40) { crushL.set (12, 40000.0, false, 0.0, false); crushR.set (12, 40000.0, false, 0.0, false); }
            else if (P.lofi == kLofi8bit) { crushL.set (24, 22050.0, false, 0.0, false); crushR.set (24, 22050.0, false, 0.0, false); }
            else if (P.lofi == kLofi11k) { crushL.set (24, 11025.0, true, 0.0, false); crushR.set (24, 11025.0, true, 0.0, false); }
        }
        if (force || tapeChanged) { hyL.set ({ (double) P.tapeDriveDb, 0.5, 0.0, 1.0, false }); hyR.set ({ (double) P.tapeDriveDb, 0.5, 0.0, 1.0, false }); tapeMk = (double) dbToGain (-P.tapeDriveDb); }
        static const double att[6] = { 0.1, 0.3, 1.0, 3.0, 10.0, 30.0 }, rel[5] = { 100, 300, 600, 1200, 400 }, rat[3] = { 2.0, 4.0, 10.0 };
        dh::lq::CompParams cp;
        cp.thresholdDb = -26.0 * P.comp; cp.ratio = rat[std::clamp (P.ratio, 0, 2)]; cp.kneeDb = 6.0;
        cp.attackMs = att[std::clamp (P.attack, 0, 5)]; cp.releaseMs = rel[std::clamp (P.release, 0, 4)];
        cp.makeupDb = 26.0 * P.comp * (1.0 - 1.0 / cp.ratio) * 0.45;
        autoRel = P.release == 4;
        makeupDb = cp.makeupDb;
        if (autoRel) { cp.releaseMs = 150.0; dh::lq::CompParams cs = cp; cs.releaseMs = 1500.0; compSlow.set (cs); }
        comp.set (cp);
        exL.set (P.exciterHz, 0.707f, fs); exR.set (P.exciterHz, 0.707f, fs);
        ex2L.set (P.exciterHz * 0.7f, fs); ex2R.set (P.exciterHz * 0.7f, fs);
        vol = dbToGain (P.volumeDb);
    }
    int latency() const { return P.tape ? kTapeLatency : 0; }
    float gainReductionDb() const { return grDb; }

    // pos: la posición absoluta de la primera muestra (el crusher ata su reloj a la posición).
    void process (float* L, float* R, int n, int64_t pos)
    {
        for (int i = 0; i < n; ++i)
        {
            double l = L[i], r = R[i];
            if (P.lofi != kLofiOff)
            {
                double cl = crushL.process (l, pos + i, 0), cr = crushR.process (r, pos + i, 1);
                if (P.lofi == kLofi8bit || P.lofi == kLofi11k) { cl = dac76::quantize ((float) cl); cr = dac76::quantize ((float) cr); }
                l += (cl - l) * P.lofiMix; r += (cr - r) * P.lofiMix;
            }
            if (P.tape)
            {
                // El drive satura sin subir el nivel de la señal chica (la compensación de la ganancia del drive).
                l = osL.process (l, [&] (double v) { return hyL.process (v); }) * tapeMk;
                r = osR.process (r, [&] (double v) { return hyR.process (v); }) * tapeMk;
                // La remanencia: después de un golpe fuerte la cinta queda magnetizada y la salida tiene continua (la
                // cabeza de reproducción real no la ve). Un pasa altos de 5 Hz.
                l = tapeDcL.process (l); r = tapeDcR.process (r);
            }
            if (P.comp > 0.001f)
            {
                const double det = std::max (std::fabs (l), std::fabs (r));
                double g = comp.gainFor (det);
                // Liberación automática (la de los compresores de bus de consola): dos detectores, uno de 150 ms y otro
                // de 1,5 s, promediados en dB: un pico suelto se recupera rápido y la compresión sostenida, lento.
                if (autoRel)
                {
                    compSlow.gainFor (det);
                    const double gr = 0.5 * (comp.currentGrDb() + compSlow.currentGrDb());
                    g = (double) fastExp2 ((float) ((makeupDb - gr) * 0.16609640474));
                    grDb = (float) gr;
                }
                else grDb = (float) comp.currentGrDb();
                l = l + (l * g - l) * P.compMix; r = r + (r * g - r) * P.compMix;
            }
            if (P.exciter > 0.001f)
            {
                const float hl = exL.highpass ((float) l), hr = exR.highpass ((float) r);
                const float gl = hl + 0.8f * hl * std::fabs (hl) * 4.0f, gr = hr + 0.8f * hr * std::fabs (hr) * 4.0f;   // armónicos pares e impares
                l += P.exciter * 0.8f * (gl - ex2L.process (gl));
                r += P.exciter * 0.8f * (gr - ex2R.process (gr));
            }
            L[i] = ceiling ((float) l * vol); R[i] = ceiling ((float) r * vol);
        }
    }

private:
    float fs = 48000; MasterParams P;
    dh::tn::Crusher crushL, crushR;
    dh::lq::Os4 osL, osR;
    dh::tn::Hysteresis hyL, hyR; double tapeMk = 1.0; dh::tn::DcBlock tapeDcL, tapeDcR;
    dh::lq::Compressor comp, compSlow; bool autoRel = true; double makeupDb = 0;
    Svf exL, exR; Lp1 ex2L, ex2R;
    float vol = 1, grDb = 0;
    static inline float ceiling (float x)
    {
        const float m = std::fabs (x);
        return m < 0.9f ? x : std::copysign (0.9f + 0.1f * fastTanh ((m - 0.9f) * 10.0f), x);
    }
};

} // namespace dh::pal
