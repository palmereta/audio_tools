#pragma once
// Bloques DSP del ATFS1: tablas, envolventes con la ley del FS1R, el operador con voz (las 8 formas
// espectrales como síntesis granular sincrónica de la patente US 5,610,354: una ventana a la frecuencia
// del grano multiplica una portadora que se reinicia en cada grano) y el operador sin voz (ruido
// filtrado y llevado por modulación en anillo al centro del formante).
//
// Mediciones públicas usadas (docs/hardware-spec.md): ventana del frmt de largo 1/(3,1 Hz·2^(BW/8)),
// a lo sumo 2 períodos; falda p = 2·√2^skirt (frmt) o 2·2^skirt (all/odd/res); all a la falda 0 = una
// sola parcial; odd = parciales impares; res = grupo de 3 líneas en la parcial (res + 1); envolventes
// con 96 dB por recorrido completo de 6,6 ms a 380 s; FM de 3,369 ciclos a nivel máximo.

#include "Common.h"
#include "FsData.h"
#include <array>
#include <cmath>
#include <vector>

namespace dh::fs {

constexpr float kFmIndex = 3.369f;            // ciclos de desvío de fase con el modulador a nivel máximo

// ───────── tablas ─────────
struct Tables
{
    static constexpr int kSine = 2048, kWin = 512, kP = 16;     // 8 KB + 33 KB: entran juntas en la caché L1/L2 de cualquier CPU
    std::array<float, kSine + 1> sine {};
    std::array<std::array<float, kWin + 2>, kP> sinPow {};   // sin^p(πu), u ∈ [0, 1]
    std::array<float, kP> pval {};

    static const Tables& get() { static Tables t; return t; }

    // Índices de p: 0..7 → 2·√2^s (frmt), 8..15 → 2·2^s (all, odd, res).
    static int pIndex (bool frmt, int skirt) { return (frmt ? 0 : 8) + (skirt < 0 ? 0 : (skirt > 7 ? 7 : skirt)); }

    inline float sin2pi (float ph) const     // ph en ciclos
    {
        // Una fase enorme (FM con niveles fuera de rango) o no finita haría el (int) indefinido: se descarta.
        if (! isFiniteF (ph) || std::fabs (ph) > 1.0e6f) ph = 0.0f;
        ph -= (float) (int) ph;                    // más barato que floor; la fase llega en [−2, 3)
        if (ph < 0.0f) ph += 1.0f;
        const float x = ph * (float) kSine;
        int i = (int) x;
        float f = x - (float) i;
        if (i >= kSine) { i = 0; f = 0.0f; }      // una fase negativa diminuta redondea a 1,0
        return sine[(size_t) i] + (sine[(size_t) i + 1] - sine[(size_t) i]) * f;
    }
    inline float window (int pi, float u) const
    {
        const float x = clampf (u, 0.0f, 1.0f) * (float) kWin;
        const int i = (int) x;
        const float f = x - (float) i;
        const auto& t = sinPow[(size_t) pi];
        return t[(size_t) i] + (t[(size_t) i + 1] - t[(size_t) i]) * f;
    }

private:
    Tables()
    {
        for (int i = 0; i <= kSine; ++i) sine[(size_t) i] = (float) std::sin (2.0 * 3.14159265358979 * i / kSine);
        for (int k = 0; k < kP; ++k)
        {
            const int s = k % 8;
            const double p = k < 8 ? 2.0 * std::pow (std::sqrt (2.0), s) : 2.0 * std::pow (2.0, s);
            pval[(size_t) k] = (float) p;
            for (int i = 0; i <= kWin + 1; ++i)
            {
                const double u = std::min (1.0, (double) i / kWin);
                sinPow[(size_t) k][(size_t) i] = (float) std::pow (std::sin (3.14159265358979 * u), p);
            }
        }
    }
};

// log2 rápido: exponente del float + serie de atanh para la mantisa (error < 1e-5 octava).
inline float fastLog2 (float x)
{
    uint32_t bits; std::memcpy (&bits, &x, sizeof (bits));
    const float e = (float) ((int) ((bits >> 23) & 255) - 127);
    bits = (bits & 0x007FFFFFu) | 0x3F800000u;
    float m; std::memcpy (&m, &bits, sizeof (m));            // m en [1, 2)
    const float t = (m - 1.0f) / (m + 1.0f), t2 = t * t;
    return e + 2.8853900818f * t * (1.0f + t2 * (0.3333333f + t2 * (0.2f + t2 * (0.1428571f + t2 * 0.1111111f))));
}

// ───────── tiempos del FS1R ─────────
// TIME 0..99 → velocidad del chip q = ((99 − T)·164) >> 8 (0..63); un recorrido de 96 dB tarda
// 2^(26 − q/4) / (4 + q mod 4) / 48000 s: de 6,6 ms (q = 63) a 380 s (q = 0).
inline float traverseSeconds (int q)
{
    q = q < 0 ? 0 : (q > 63 ? 63 : q);
    return (float) std::ldexp (1.0, 26 - (q >> 2)) / (float) (4 + (q & 3)) / 48000.0f;
}
inline int rateOf (int time) { time = time < 0 ? 0 : (time > 99 ? 99 : time); return ((99 - time) * 164) >> 8; }
inline int keyRateAdd (int timeScale, int note) { const int x = (int) clampf ((float) (note - 55) / 4.0f, -13.0f, 13.0f); return timeScale * x / 8; }
inline float levelDb (int L) { return L <= 0 ? -96.0f : (float) (L - 99) * 0.75f; }
inline float dbGain (float db) { return db <= -95.0f ? 0.0f : fastExp2 (db * 0.16609640474f); }

// ───────── envolvente de amplitud (por operador) ─────────
// Retiene (HOLD), va a L1 en T1, a L2 en T2, a L3 en T3 (sostenido); al soltar, a L4 en T4. Parte de L4.
// Las caídas son lineales en dB; las subidas, una aproximación exponencial con constante 1/16 de recorrido
// desde un piso de −53,2 dB que se pasa unos 3,8 dB del objetivo (y ahí corta).
struct AmpEg
{
    int stage = 5;                     // 0 hold, 1..3 segmentos, 4 release, 5 apagada
    float db = -96.0f, holdLeft = 0.0f;
    std::array<float, 4> target {}, rateDb {}, tau {};

    void setup (const AmpEgData& d, int note, int attackOfs, int decayOfs, int releaseOfs, float egBiasDb)
    {
        const int add = keyRateAdd (d.timeScale, note);
        const int ofs[4] = { attackOfs, decayOfs, decayOfs, releaseOfs };
        for (int i = 0; i < 4; ++i)
        {
            const int q = rateOf (d.time[(size_t) i] - ofs[i]) + add;
            const float tr = traverseSeconds (q);
            rateDb[(size_t) i] = 96.0f / tr;
            tau[(size_t) i] = tr / 16.0f;
            target[(size_t) i] = levelDb (d.level[(size_t) i]) + egBiasDb;
        }
        holdTime = d.hold <= 0 ? 0.0f : traverseSeconds (rateOf (d.hold) + 4) * 0.25f + 0.002f;
        relTime = d.time[3]; relAdd = add;
    }
    // El desvío de RELEASE de la parte que vale al soltar la tecla (no el que valía al tocarla): una nota tocada
    // con RELEASE al máximo y soltada después de bajarlo quedaba sonando minutos.
    void setReleaseOfs (int releaseOfs)
    {
        const float tr = traverseSeconds (rateOf (relTime - releaseOfs) + relAdd);
        rateDb[3] = 96.0f / tr; tau[3] = tr / 16.0f;
    }
    void keyOn (bool legato)
    {
        if (! legato || stage >= 4) { db = std::max (db, target[3]); }
        holdLeft = holdTime;
        stage = holdTime > 0.0f ? 0 : 1;
    }
    void keyOff() { if (stage < 4) stage = 4; }
    void reset() { db = -96.0f; stage = 5; holdLeft = 0.0f; }   // voz nueva: el ataque arranca de cero
    bool idle() const { return stage == 5; }

    // dt: segundos de este paso de control.
    inline float process (float dt)
    {
        if (stage == 5) return -96.0f;
        if (stage == 0) { holdLeft -= dt; if (holdLeft <= 0.0f) stage = 1; return db; }
        const int seg = stage == 4 ? 3 : stage - 1;
        const float tg = target[(size_t) seg];
        if (tg > db)
        {
            db = std::max (db, -53.2f);
            db += (tg + 3.8f - db) * (1.0f - fastExp (-dt / std::max (1.0e-5f, tau[(size_t) seg])));
            if (db >= tg) { db = tg; advance(); }
        }
        else
        {
            db -= rateDb[(size_t) seg] * dt;
            if (db <= tg) { db = tg; advance(); }
        }
        if (stage == 4 && db <= -95.0f) { stage = 5; db = -96.0f; }
        return db;
    }

private:
    float holdTime = 0.0f;
    int relTime = 0, relAdd = 0;
    void advance() { if (stage == 1 || stage == 2) ++stage; else if (stage == 4 && db <= -95.0f) stage = 5; }   // en 3 sostiene
};

// ───────── envolvente de frecuencia del operador: INIT → ATTACK → 0, rampa lineal (24 semitonos por recorrido) ─────────
struct FreqEg
{
    float semis = 0.0f; int stage = 2; float target = 0.0f, rate = 0.0f, rate2 = 0.0f;
    void keyOn (const FreqEgData& d)
    {
        semis = (float) d.init * 0.48f; target = (float) d.attack * 0.48f;
        rate = 24.0f / traverseSeconds (rateOf (d.attackTime)); rate2 = 24.0f / traverseSeconds (rateOf (d.decayTime));
        stage = (d.init == 0 && d.attack == 0) ? 2 : 0;
    }
    inline float process (float dt)
    {
        if (stage == 0) { semis += (target > semis ? 1.0f : -1.0f) * rate * dt; if (std::fabs (semis - target) <= rate * dt) { semis = target; stage = 1; } }
        else if (stage == 1) { semis += (0.0f > semis ? 1.0f : -1.0f) * rate2 * dt; if (std::fabs (semis) <= rate2 * dt) { semis = 0.0f; stage = 2; } }
        return semis;
    }
};

// ───────── envolvente de altura (de la voz): L0 → L1 → L2 → L3 (sostiene) → L4 al soltar ─────────
struct PitchEg
{
    float val = 0.0f; int stage = 4; std::array<float, 5> lv {}; std::array<float, 4> rate {};
    void setup (const VoiceData& v, float velocity)
    {
        static const float RANGE[4] = { 48.0f, 12.0f, 6.0f, 3.0f };      // semitonos para ±50 (8, 2, 1, ½ octava)
        const float k = RANGE[v.pegRange & 3] / 50.0f * (1.0f - (float) v.pegVel / 7.0f * (1.0f - velocity));
        for (int i = 0; i < 5; ++i) lv[(size_t) i] = (float) v.pegLevel[(size_t) i] * k;
        for (int i = 0; i < 4; ++i) rate[(size_t) i] = 96.0f / traverseSeconds (rateOf (v.pegTime[(size_t) i])) * 0.5f;
    }
    void keyOn() { val = lv[0]; stage = 0; }
    void keyOff() { stage = 3; }
    inline float process (float dt)
    {
        if (stage >= 4) return val;
        const float tg = stage == 3 ? lv[4] : lv[(size_t) stage + 1];
        const float r = rate[(size_t) stage] * dt;
        if (std::fabs (tg - val) <= r) { val = tg; if (stage < 2) ++stage; else if (stage == 3) stage = 4; else stage = 2; }
        else val += tg > val ? r : -r;
        return val;
    }
};

// ───────── LFO: triángulo, sierra abajo, sierra arriba, cuadrada, seno, sample and hold ─────────
inline float lfoHz (int speed, int maxv = 99) { return 0.0645f * fastExp (0.0675f * 99.0f * (float) speed / (float) maxv); }
struct Lfo
{
    float phase = 0.0f, value = 0.0f, sh = 0.0f, delayAmt = 1.0f, delayRate = 0.0f;
    Rng rng { 0x1F51u };
    void keySync (float startPhase = 0.0f) { phase = startPhase; }
    void startDelay (int delay) { delayAmt = delay <= 0 ? 1.0f : 0.0f; delayRate = delay <= 0 ? 0.0f : 1.0f / (0.05f + 0.06f * (float) delay); }
    inline float process (float hz, int wave, float dt)
    {
        phase += hz * dt;
        if (phase >= 1.0f) { phase -= std::floor (phase); sh = rng.bipolar(); }
        const float t = phase;
        switch (wave)
        {
            case 0: value = t < 0.5f ? 4.0f * t - 1.0f : 3.0f - 4.0f * t; break;
            case 1: value = 1.0f - 2.0f * t; break;
            case 2: value = 2.0f * t - 1.0f; break;
            case 3: value = t < 0.5f ? 1.0f : -1.0f; break;
            case 4: value = Tables::get().sin2pi (t); break;
            default: value = sh; break;
        }
        if (delayAmt < 1.0f) delayAmt = std::min (1.0f, delayAmt + delayRate * dt);
        return value * delayAmt;
    }
};

// ───────── operador con voz ─────────
// Granos: dos ranuras que se alternan, así la ventana puede durar hasta dos períodos del grano.
// setup() fija la forma, los ritmos y la ventana una vez por bloque de control; tick() da una muestra.
// El largo de la ventana queda fijado al empezar cada grano (como el chip, que cambia la frecuencia en el
// grano siguiente).
struct FormOperator
{
    struct Slot { bool on = false; float age = 0.0f, invLen = 1.0f, carrier = 0.0f; };
    std::array<Slot, 2> slot {};
    int next = 0, parity = 0;
    float grainPhase = 0.0f, sinePhase = 0.0f, out = 0.0f;

    void reset (bool keySync) { if (keySync) { grainPhase = 0.0f; sinePhase = 0.0f; } }

    // grainHz: ritmo de los granos (frmt); carrierHz: frecuencia del operador; winSec: largo de la ventana (frmt).
    void setup (int form, float grainHz, float carrierHz, float winSec, int pIdx_, int resonance, float fs)
    {
        invFs = 1.0f / fs;
        sine = form == Sine;
        carInc = carrierHz * invFs;
        if (sine) return;
        // Ritmo y largo de grano, portadora y su fase al empezar cada grano, según la forma. En all y res
        // la portadora pasa por su cresta en el centro de la ventana (fase 1/4: así a falda alta el grupo
        // se abre alrededor de la fundamental); en odd los granos van al doble y la portadora sigue de
        // largo: granos alternados con signo opuesto, sólo parciales impares.
        float rate, len, car = carrierHz;
        startPh = 0.25f; asym = false; alternate = false;
        switch (form)
        {
            case All1: asym = true; [[fallthrough]];
            case All2: rate = carrierHz; len = 2.0f / std::max (1.0f, carrierHz); break;
            case Odd1: asym = true; [[fallthrough]];
            case Odd2: rate = 2.0f * carrierHz; len = 1.0f / std::max (1.0f, 2.0f * carrierHz); startPh = 0.0f; alternate = true; break;
            case Res1: asym = true; [[fallthrough]];
            case Res2: rate = carrierHz; len = 1.0f / std::max (1.0f, carrierHz); car = carrierHz * (float) (resonance + 1); break;
            default:   asym = true; rate = grainHz; len = std::min (2.0f / std::max (1.0f, grainHz), winSec); startPh = 0.0f; break;
        }
        rate = std::min (std::max (rate, 1.0f), 0.5f * fs);
        rateInc = rate * invFs; invRate = 1.0f / rate;
        invLen = 1.0f / std::max (len, 2.0f * invFs);
        carInc = std::min (car, 0.5f * fs) * invFs; carGrain = car;
        pIdx = pIdx_; fallIdx = Tables::pIndex (form == Frmt, 0);
    }

    inline float tick (float modCycles)
    {
        const auto& T = Tables::get();
        if (sine)
        {
            sinePhase += carInc;
            if (sinePhase >= 1.0f) sinePhase -= 1.0f;
            return out = T.sin2pi (sinePhase + modCycles);
        }
        grainPhase += rateInc;
        if (grainPhase >= 1.0f)
        {
            grainPhase -= (float) (int) grainPhase;
            auto& s = slot[(size_t) next];
            s.on = true; s.age = grainPhase * invRate; s.invLen = invLen;
            parity ^= 1;
            s.carrier = startPh + (alternate && parity ? 0.5f : 0.0f) + carGrain * s.age;
            s.carrier -= (float) (int) s.carrier;
            next ^= 1;
        }
        float y = 0.0f;
        for (auto& s : slot)
        {
            if (! s.on) continue;
            const float u = s.age * s.invLen;
            if (u >= 1.0f) { s.on = false; continue; }
            const float w = T.window (asym && u > 0.5f ? fallIdx : pIdx, u);     // "1": caída sin²
            y += w * T.sin2pi (s.carrier + modCycles);
            s.age += invFs;
            s.carrier += carInc; if (s.carrier >= 1.0f) s.carrier -= 1.0f;
        }
        return out = y;
    }

    // Comodidad para pruebas y para el editor (configura y da una muestra).
    inline float process (int form, float grainHz, float carrierHz, float winSec, int pIdx_, int resonance, float modCycles, float fs)
    {
        setup (form, grainHz, carrierHz, winSec, pIdx_, resonance, fs);
        return tick (modCycles);
    }

private:
    bool sine = true, asym = false, alternate = false;
    float invFs = 1.0f / 48000.0f, rateInc = 0.0f, invRate = 1.0f, invLen = 1.0f, carInc = 0.0f, carGrain = 0.0f, startPh = 0.25f;
    int pIdx = 0, fallIdx = 0;
};

// ───────── operador sin voz: ruido formante ─────────
// Dos pasa bajos de un polo sobre ruido blanco, llevado por anillo al centro: una banda de ±corte alrededor
// del formante. El corte es lineal en el ancho (≈17 Hz por paso del registro, satura en BW 60) y cada paso
// de falda lo multiplica por ≈1,35. Resonancia 4 estrecha la banda; 5 a 7 suman una portadora senoidal.
struct NoiseOperator
{
    Rng rng { 0xA11CEu };
    float lp1 = 0.0f, lp2 = 0.0f, phase = 0.0f, last = 0.0f, out = 0.0f;

    void setup (float centerHz, int bandwidth, int skirt, int resonance, float fs)
    {
        const float reg = (float) std::min (bandwidth, 60) * (77.0f / 60.0f);
        float corner = std::max (8.0f, 17.0f * reg) * fastExp2 ((float) skirt * 0.433f);   // 1,35^skirt
        if (resonance == 4) corner *= 0.6f;
        a = 1.0f - fastExp (-kTwoPi * std::min (corner, 0.45f * fs) / fs);
        // Ganancia para que la banda sea pareja en nivel (el ruido filtrado cae con el corte); ~−9 dB RMS a BW 30.
        const float norm = 1.0f / std::sqrt (std::max (1.0e-4f, a * 0.5f));
        // Nivel contra ancho (medido): −20 dB en 0, parejo de 5 a 20, ~15 dB de caída hasta el máximo.
        const float bwDb = reg < 5.0f ? -20.0f + reg * 4.0f : (reg <= 20.0f ? 0.0f : -15.0f * std::min (1.0f, (reg - 20.0f) / 40.0f));
        static const float SINE[8] = { 0, 0, 0, 0, 0.21f, 0.46f, 0.76f, 1.02f };
        const float g = dbGain (bwDb);
        noiseGain = norm * 2.5f * g;
        sineGain = SINE[resonance & 7] * 1.75f * g;
        phaseInc = std::min (centerHz, 0.5f * fs) / fs;
    }
    inline float tick()
    {
        const float n = rng.bipolar();
        lp1 += (n - lp1) * a; lp2 += (lp1 - lp2) * a;
        phase += phaseInc; if (phase >= 1.0f) phase -= 1.0f;
        const float c = Tables::get().sin2pi (phase);
        const float y = (lp2 * noiseGain + sineGain) * c;
        out = 0.5f * (y + last);                 // promedio de dos muestras: cae arriba de ~8 kHz
        last = y;
        return out;
    }
    inline float process (float centerHz, int bandwidth, int skirt, int resonance, float fs) { setup (centerHz, bandwidth, skirt, resonance, fs); return tick(); }

private:
    float a = 0.1f, noiseGain = 1.0f, sineGain = 0.0f, phaseInc = 0.0f;
};

// ───────── filtro de la voz: LPF24/18/12, HPF, BPF, BEF ─────────
struct VoiceFilter
{
    float s1 = 0, s2 = 0, s3 = 0, s4 = 0, s5 = 0;
    void reset() { s1 = s2 = s3 = s4 = s5 = 0.0f; }

    // type: 0 LPF24, 1 LPF18, 2 LPF12, 3 HPF, 4 BPF, 5 BEF. res: −16..+100 (el FS1R admite resonancia negativa:
    // más amortiguado que Butterworth). setup() una vez por bloque de control (coeficientes y divisiones);
    // tick() por muestra.
    void setup (int type_, float fc, int res, float fs)
    {
        type = type_;
        const float g = prewarp (fc, fs);
        // Factor de amortiguamiento sobre la etapa resonante: 1 en 0, ~0 en 100 (autooscila apenas), 1,5 en −16.
        const float k = res >= 0 ? std::max (0.008f, 1.0f - 0.992f * (float) res / 100.0f) : 1.0f + 0.5f * (float) -res / 16.0f;
        float R1, R2 = 0.383f * k;
        switch (type)
        {
            case 0: R1 = 0.924f; break;
            case 1: R1 = 0.5f * k; break;
            case 5: R1 = std::max (0.2f, 0.707f * k); break;
            default: R1 = 0.707f * k; break;
        }
        a1 = Stage::make (g, R1); a2 = Stage::make (g, R2);
        onePole = g / (1.0f + g);
    }
    inline float process (float x, int type_, float fc, int res, float fs) { setup (type_, fc, res, fs); return tick (x); }
    inline float tick (float x)
    {
        switch (type)
        {
            case 0: return a2.lp (a1.lp (x, s1, s2), s3, s4);
            case 1: { const float v = (a1.lp (x, s1, s2) - s5) * onePole; const float y = v + s5; s5 = y + v; return y; }
            case 2: return a1.lp (x, s1, s2);
            case 3: return a1.run (x, s1, s2, 2);
            case 4: return a1.run (x, s1, s2, 1);
            default: return a1.run (x, s1, s2, 3);
        }
    }
    // Una vez por bloque: si el estado se fue a infinito (resonancia extrema con mucha ganancia), se limpia.
    void sanitize() { if (! std::isfinite (s1 + s2 + s3 + s4 + s5)) reset(); }

private:
    // Etapa de estado variable TPT con el divisor precalculado.
    struct Stage
    {
        float g = 0.5f, R = 0.707f, h = 1.0f;
        static Stage make (float g, float R) { Stage st; st.g = g; st.R = R; st.h = 1.0f / (1.0f + 2.0f * R * g + g * g); return st; }
        inline float lp (float x, float& a, float& b) const
        {
            const float hp = (x - (2.0f * R + g) * a - b) * h;
            const float v1 = g * hp, bp = v1 + a; a = bp + v1;
            const float v2 = g * bp, lpo = v2 + b; b = lpo + v2;
            return lpo;
        }
        inline float run (float x, float& a, float& b, int mode) const
        {
            const float hp = (x - (2.0f * R + g) * a - b) * h;
            const float v1 = g * hp, bp = v1 + a; a = bp + v1;
            const float v2 = g * bp, lpo = v2 + b; b = lpo + v2;
            return mode == 1 ? bp : (mode == 2 ? hp : x - 2.0f * R * bp);
        }
    };
    int type = 0;
    Stage a1, a2;
    float onePole = 0.3f;
};

} // namespace dh::fs
