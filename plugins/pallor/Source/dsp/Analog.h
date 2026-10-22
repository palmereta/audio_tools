#pragma once
// El motor analógico de Pallor: modelos de circuito de las cajas de ritmo de 1978-84 y de los tambores electrónicos
// hexagonales, más dos voces de hoy (docs/research.md §3 y §7). Ningún modelo copia una máquina: son los circuitos
// genéricos que esas máquinas usaban, con sus números publicados donde los hay:
//   - Puente en T (bombo, tom, conga, rim, clave): un resonador de dos polos pateado por el pulso de disparo de ~1 ms.
//     El pulso también pasa directo a la salida (el clic) y la altura arranca más arriba y cae unos ms (el deslizamiento
//     que Werner, Abel y Smith atribuyen al pulso que cambia el punto de trabajo del puente; DAFx 2014).
//   - VCO con envolvente de altura y conformador triángulo → seno (el bombo y los toms de 1983-84).
//   - Seis cuadradas de frecuencias sin relación armónica en pasa bandas (platillos y charles; Werner, ICMC 2014), dos
//     cuadradas en un pasa banda con envolvente de dos tramos (el cencerro; Werner, AES 2014).
//   - Ruido filtrado con envolvente de diente de sierra que se redispara (el aplauso), ruido de caja barata.
//   - Los tambores hexagonales: oscilador con barrido de altura empinado (el "pew"), ruido y clic, cada uno con su
//     nivel; el barrido baja y su tiempo sigue al decay.
// Cada modelo deja su salida normalizada (pico ≈ 1 con velocidad 1, sin acento); el nivel, el paneo y los insertos
// van en el slot (Voice.h). Todo se calcula por muestra con funciones rápidas (sin std::sin/exp por muestra).

#include "Base.h"

namespace dh::pal {

// Los 16 instrumentos y su familia de modelos.
enum SlotId { kKick, kSnare, kClap, kRim, kClosedHat, kOpenHat, kCrash, kRide, kTom1, kTom2, kTom3, kTom4, kCowbell, kClave, kTamb, kFx, kNumSlots };
enum Family { fKick, fSnare, fClap, fRim, fHat, fCrash, fRide, fTom, fCowbell, fClave, fTamb, fFx };

inline Family familyOf (int slot)
{
    static const Family f[kNumSlots] = { fKick, fSnare, fClap, fRim, fHat, fHat, fCrash, fRide, fTom, fTom, fTom, fTom, fCowbell, fClave, fTamb, fFx };
    return f[std::clamp (slot, 0, kNumSlots - 1)];
}

// Nombres de los modelos por familia (lo que muestra la interfaz) y de los dos controles de carácter de cada uno.
struct ModelInfo { const char* name; const char* charA; const char* charB; };
inline const std::vector<ModelInfo>& analogModels (Family f)
{
    static const std::vector<ModelInfo> m[12] = {
        { { "Bridged-T", "Click", "Glide" }, { "VCO Punch", "Attack", "Sweep" }, { "Compact", "Click", "Thump" }, { "Hexagonal", "Click", "Bend" }, { "Dark", "Click", "Sweep time" } },
        { { "Bridged-T", "Snappy", "Body" }, { "VCO Punch", "Snappy", "Sweep" }, { "Hexagonal", "Noise", "Bend" }, { "Noise Box", "Snappy", "Ring" }, { "Crack", "Snap", "Body" } },
        { { "Analog", "Spread", "Tail" }, { "Big", "Spread", "Tail" }, { "Tight", "Spread", "Tail" } },
        { { "Bridged-T", "Click", "Ring" }, { "Woody", "Click", "Ratio" }, { "Click", "Noise", "Bend" } },
        { { "Six-Square", "Color", "Spread" }, { "Noise Box", "Ring", "Grit" }, { "Twin Tank", "Metal", "Ratio" }, { "FM", "Index", "Ratio" } },
        { { "Six-Square", "Color", "Spread" }, { "Noise Wash", "Shimmer", "Swell" }, { "FM", "Index", "Ratio" } },
        { { "Six-Square", "Ping", "Spread" }, { "Bell", "Wash", "Stretch" }, { "FM", "Index", "Ratio" } },
        { { "Bridged-T", "Noise", "Glide" }, { "Hexagonal", "Noise", "Bend" }, { "VCO Punch", "Noise", "Sweep" }, { "Conga", "Slap", "Glide" } },
        { { "Two-Square", "Body", "Detune" }, { "Clank", "Index", "Ratio" }, { "Bell", "Strike", "Stretch" } },
        { { "Bridged-T", "Click", "Ring" }, { "Woodblock", "Strike", "Stretch" }, { "Blip", "Noise", "Sweep" } },
        { { "Maracas", "Attack", "Grain" }, { "Tambourine", "Spread", "Shake" }, { "Shaker", "Attack", "Grain" } },
        { { "Zap", "Noise", "Depth" }, { "Sweep", "Resonance", "Direction" }, { "Metal", "Index", "Ratio" }, { "Drop", "Drive", "Depth" }, { "Static", "Density", "Hiss" } },
    };
    return m[(int) f];
}
inline int numAnalogModels (int slot) { return (int) analogModels (familyOf (slot)).size(); }

// Ganancia de cada modelo para que su pico con los controles al medio quede en el nivel de su slot (kitgen::slotPeak:
// bombo 0.95, caja 0.9, charles 0.8…): un kit nuevo suena equilibrado sin tocar los niveles. Medido con
// pallortest --calibrate (8 golpes por modelo, 48 kHz).
inline float modelGain (Family f, int m)
{
    static const std::vector<float> g[12] = {
        { 1.127f, 1.176f, 1.170f, 1.334f, 0.938f },
        { 1.089f, 1.097f, 1.187f, 1.005f, 1.016f },
        { 1.572f, 1.256f, 1.424f },
        { 0.887f, 0.870f, 0.893f },
        { 2.223f, 0.472f, 0.735f, 0.576f },
        { 0.595f, 0.267f, 0.494f },
        { 0.494f, 0.608f, 0.697f },
        { 1.087f, 1.243f, 1.138f, 1.058f },
        { 2.877f, 0.703f, 0.833f },
        { 0.919f, 0.904f, 0.943f },
        { 0.275f, 0.828f, 0.602f },
        { 0.969f, 0.593f, 0.771f, 0.700f, 1.772f },
    };
    const auto& v = g[(int) f];
    return v[(size_t) std::clamp (m, 0, (int) v.size() - 1)];
}

// Frecuencias de las seis cuadradas del generador metálico (Werner, Abel y Smith, ICMC 2014, medidas del circuito).
inline const float* metalFreqs() { static const float f[6] = { 205.3f, 304.4f, 369.6f, 522.7f, 540.0f, 800.0f }; return f; }
// Frecuencia base (tune = 0) de cada tom: de grave a agudo.
inline float tomBase (int slot) { static const float f[4] = { 82.0f, 110.0f, 147.0f, 196.0f }; return f[std::clamp (slot - kTom1, 0, 3)]; }

// Los controles de un golpe, en unidades del panel: tune en semitonos, el resto 0..1.
struct Shape { float tune = 0, decay = 0.5f, tone = 0.5f, a = 0.5f, b = 0.5f; };

class AnalogVoice
{
public:
    void prepare (float sampleRate, int slotId) { fs = sampleRate; slot = slotId; fam = familyOf (slotId); active = false; }
    bool isActive() const { return active; }
    void stop() { active = false; }
    bool calibrated = true;          // false: la salida cruda del modelo (para medir la tabla de modelGain)

    // Un golpe. vel 0..1 (la velocidad ya pasada por la sensibilidad), acc 0..1 (cuánto acento: más nivel y más
    // saturación en la etapa de salida, como en las cajas con línea de acento). seed: el número de golpe (las fases
    // libres de los osciladores y el ruido salen de él, así un golpe se repite igual en cada render).
    void trigger (int modelIndex, const Shape& sh, float vel, float acc, uint32_t seed)
    {
        model = std::clamp (modelIndex, 0, numAnalogModels (slot) - 1);
        S = sh; amp = vel * (calibrated ? modelGain (fam, model) : 1.0f); accent = clampf (acc, 0.0f, 1.0f);
        t = 0; active = true; checkCount = 0;
        nz.seed (seed * 2654435761u + (uint32_t) slot * 40503u + 1u);
        for (auto& m : md) m.reset();
        for (auto& f : flt) f.reset();
        for (auto& o : osc) o.p = 0;
        lpA.reset(); lpB.reset(); hpA.reset(); hpB.reset();
        for (auto& e : env) e.v = 0;
        held = 0; holdPh = 0; pe = 0; pe2 = 0; aux = 0;
        tr = semis (S.tune);
        hitSeed = seed;
        switch (fam)
        {
            case fKick: setupKick(); break;
            case fSnare: setupSnare(); break;
            case fClap: setupClap(); break;
            case fRim: setupRim(); break;
            case fHat: setupHat(); break;
            case fCrash: setupCrash(); break;
            case fRide: setupRide(); break;
            case fTom: setupTom(); break;
            case fCowbell: setupCowbell(); break;
            case fClave: setupClave(); break;
            case fTamb: setupTamb(); break;
            case fFx: setupFx(); break;
        }
    }

    inline float process()
    {
        if (! active) return 0.0f;
        float y = 0.0f;
        switch (fam)
        {
            case fKick: y = kick(); break;
            case fSnare: y = snare(); break;
            case fClap: y = clap(); break;
            case fRim: y = rim(); break;
            case fHat: y = hat(); break;
            case fCrash: y = crash(); break;
            case fRide: y = ride(); break;
            case fTom: y = tom(); break;
            case fCowbell: y = cowbell(); break;
            case fClave: y = clave(); break;
            case fTamb: y = tamb(); break;
            case fFx: y = fx(); break;
        }
        ++t;
        // ¿Terminó? Cada 64 muestras se mira la envolvente principal (por debajo de −90 dB) y un largo máximo.
        if (++checkCount >= 64)
        {
            checkCount = 0;
            if ((t > minLen && level() < 3e-5f) || t > maxLen) active = false;
        }
        return amp * y;
    }

private:
    float fs = 48000; int slot = 0; Family fam = fKick; int model = 0;
    Shape S; float amp = 1, accent = 0, tr = 1;
    int t = 0, minLen = 0, maxLen = 1 << 30, checkCount = 0; bool active = false; uint32_t hitSeed = 0;
    Noise nz;
    std::array<Mode, 6> md {};
    std::array<float, 6> mdGain {};
    std::array<Svf, 4> flt {};
    std::array<Phasor, 3> osc {};
    std::array<Decay, 5> env {};
    Lp1 lpA, lpB; Hp1 hpA, hpB;
    Metal6 metal;
    // Cantidades fijadas en el disparo.
    float f0 = 50, f1 = 0, glide = 0, glideC = 0, glide2C = 0, pe = 0, pe2 = 0;
    float pulseGain = 0; int pulseN = 0;
    float k0 = 0, k1 = 0, k2 = 0, aux = 0, held = 0, holdPh = 0, holdInc = 0;
    int nBursts = 0; std::array<int, 6> burstAt {}; int burstLen = 0, tailAt = 0;

    // Nivel de la envolvente principal (para saber cuándo apagar).
    float level() const { float m = 0; for (const auto& e : env) m = std::max (m, e.v); for (const auto& d : md) m = std::max (m, std::fabs (d.re) + std::fabs (d.im)); return m; }

    void setLen (float minSeconds, float maxSeconds) { minLen = (int) (minSeconds * fs); maxLen = (int) (maxSeconds * fs); }

    // El pulso de disparo de ancho `ms` sobre un modo de frecuencia `hz`: la ganancia que deja la amplitud en 1.
    // La suma de N muestras de un fasor que gira ω da |sen(ωN/2)/sen(ω/2)| (para pulsos largos frente al período los
    // dos flancos se cancelan en parte: el resonador se golpea con los bordes, como en el circuito).
    float pulseNorm (float hz, int n) const
    {
        const float w = kPi * hz / fs;
        const float num = std::fabs (sin2pi (w * (float) n / kTwoPi)), den = std::max (1e-6f, std::fabs (sin2pi (w / kTwoPi)));
        return 1.0f / std::max (0.35f * (float) n, num / den);
    }

    // ── Bombo ──
    void setupKick()
    {
        switch (model)
        {
            case 0:   // Puente en T (research §3.1): resonador a 49,5 Hz que arranca ~15 % más arriba y se asienta en
                      // 0,2-0,3 s; durante los primeros 4 ms resuena casi al doble (la constante de tiempo se parte en dos
                      // con el acento: un ataque más seco); pulso de 1 ms, el clic que pasa directo y el sobrepico.
                f0 = 49.5f * tr; glide = 0.3f * S.b; glideC = tauCoef (0.09f, fs); pe = glide;
                k0 = 0.004f * fs; k1 = accent > 0.01f ? 0.9f : 0.5f * S.a;
                md[0].setDecay (expMap (S.decay, 0.15f, 4.0f), fs);
                pulseN = std::max (1, (int) (0.001f * fs)); pulseGain = pulseNorm (f0 * (1 + glide) * (1 + k1), pulseN) * 1.05f;
                env[1].setTau (0.00035f, fs); env[1].fire (S.a * 1.3f);           // clic: el pulso que pasa directo
                env[2].setTau (0.018f, fs); env[2].fire (0.15f + 0.35f * S.a);    // sobrepico del primer ciclo
                lpA.set (expMap (S.tone, 220.0f, 9000.0f), fs);
                setLen (0.02f, 6.0f);
                break;
            case 1:   // VCO: seno con dos tramos de envolvente de altura (≈ 270 Hz → 49,5 Hz, τ 3 y 31 ms), clic de ruido y pulso
                f0 = 49.5f * tr; glide = 0.4f + 9.0f * S.b; pe = 1.0f; pe2 = 1.0f;
                glideC = tauCoef (0.003f, fs); glide2C = tauCoef (0.031f, fs);
                env[0].set (expMap (S.decay, 0.12f, 1.8f), fs); env[0].fire();
                env[1].setTau (0.0016f, fs); env[1].fire (S.a * 0.9f);
                env[2].setTau (0.00025f, fs); env[2].fire (S.a * 0.8f);
                hpA.set (1800.0f, fs);
                lpA.set (expMap (S.tone, 400.0f, 14000.0f), fs);
                setLen (0.02f, 6.0f);
                break;
            case 2:   // Compacto (la caja barata de 1978, research §3.9): oscilador de un transistor a 62,5 Hz que suena
                      // sin caída de altura, ~100 ms, oscuro, con el "bof" de ruido grave
                f0 = 62.5f * tr; glide = 0.02f; glideC = tauCoef (0.007f, fs); pe = glide;
                md[0].setDecay (expMap (S.decay, 0.08f, 1.0f), fs);
                pulseN = std::max (1, (int) (0.002f * fs)); pulseGain = pulseNorm (f0 * 1.02f, pulseN);
                env[1].setTau (0.0003f, fs); env[1].fire (S.a * 1.1f);
                env[3].setTau (0.022f, fs); env[3].fire (S.b * 1.6f);
                lpB.set (170.0f, fs);
                lpA.set (expMap (S.tone, 150.0f, 4500.0f), fs);
                setLen (0.02f, 4.0f);
                break;
            case 3:   // Hexagonal: seno con barrido en octavas, ruido, clic
                f0 = 56.0f * tr; glide = 2.6f * S.b; pe = 1.0f; glideC = tauCoef (0.018f + 0.09f * S.decay, fs);
                env[0].set (expMap (S.decay, 0.12f, 2.2f), fs); env[0].fire();
                env[1].setTau (0.0006f, fs); env[1].fire (S.a * 0.8f);
                hpA.set (500.0f, fs);
                lpA.set (expMap (S.tone, 250.0f, 9000.0f), fs);
                setLen (0.02f, 6.0f);
                break;
            default:  // Oscuro (de hoy): barrido largo de varias octavas, meseta, saturación
                f0 = 44.0f * tr; glide = 2.7f; pe = 1.0f; glideC = tauCoef (0.006f + 0.05f * S.b, fs);
                env[0].set (expMap (S.decay, 0.2f, 3.2f), fs); env[0].fire();
                k0 = 0.008f * fs;                                                // meseta antes de caer
                env[1].setTau (0.0025f, fs); env[1].fire (S.a * 1.2f);
                env[2].setTau (0.0004f, fs); env[2].fire (S.a * 0.7f);
                flt[0].set (3600.0f, 1.2f, fs);
                lpA.set (expMap (S.tone, 200.0f, 14000.0f), fs);
                setLen (0.02f, 7.0f);
                break;
        }
    }
    inline float kick()
    {
        switch (model)
        {
            case 0:
            {
                pe *= glideC;
                md[0].setFreq (f0 * (1.0f + pe) * ((float) t < k0 ? 1.0f + k1 : 1.0f), fs);
                const float x = t < pulseN ? pulseGain : 0.0f;
                float y = md[0].process (x) * (1.0f + env[2].tick()) + env[1].tick();
                y = lpA.process (y);
                return fastTanh (y * (1.1f + 1.4f * accent)) * 0.95f;
            }
            case 1:
            {
                pe *= glideC; pe2 *= glide2C;
                const float f = f0 * (1.0f + glide * (0.25f * pe + 0.75f * pe2));
                const float p = osc[0].tick (f, fs);
                float body = 0.86f * sin2pi (p) + 0.14f * triFromPhase (p);
                body *= env[0].tick();
                const float click = hpA.process (nz.next()) * env[1].tick() + env[2].tick();
                const float y = lpA.process (body + click);
                return fastTanh (y * (1.2f + 1.2f * accent)) * 0.98f;
            }
            case 2:
            {
                pe *= glideC;
                md[0].setFreq (f0 * (1.0f + pe), fs);
                const float x = t < pulseN ? pulseGain : 0.0f;
                const float thump = lpB.process (nz.next()) * env[3].tick() * 3.0f;
                const float y = lpA.process (md[0].process (x) + thump) + env[1].tick();
                return fastTanh (y * (1.1f + accent)) * 0.95f;
            }
            case 3:
            {
                pe *= glideC;
                const float f = f0 * fastExp2 (glide * pe);
                const float p = osc[0].tick (f, fs);
                const float e = env[0].tick() * std::min (1.0f, (float) t / (0.0005f * fs));
                const float body = (0.9f * sin2pi (p) + 0.1f * triFromPhase (p)) * e;
                const float n = nz.next() * e * 0.05f;
                const float click = hpA.process (env[1].tick() * (1.0f + 0.3f * nz.next()));
                const float y = lpA.process (body + n) + click;
                return fastTanh (y * (1.1f + accent)) * 0.95f;
            }
            default:
            {
                pe *= glideC;
                const float f = f0 * fastExp2 (glide * pe);
                const float p = osc[0].tick (f, fs);
                const float e = (float) t < k0 ? env[0].v : env[0].tick();
                const float body = sin2pi (p) * e;
                const float click = flt[0].bandpass (nz.next()) * env[1].tick() + env[2].tick();
                float y = fastTanh ((body + click) * (2.2f + 1.5f * accent)) * 1.03f;
                return lpA.process (y);
            }
        }
    }

    // ── Caja ──
    void setupSnare()
    {
        switch (model)
        {
            case 0:   // Puente en T (research §3.2): resonadores a 238 y 476 Hz (Q ≈ 17 y 11) en la proporción de Body, más
                      // el ruido "snappy" por un pasa altos de 2,75 kHz (Q 0,73) con su envolvente
            {
                f0 = 238.0f * tr; f1 = 476.0f * tr;
                md[0].setFreq (f0, fs); md[1].setFreq (f1, fs);
                md[0].setDecay (0.08f + 0.28f * S.decay, fs); md[1].setDecay (0.03f + 0.1f * S.decay, fs);
                pulseN = std::max (1, (int) (0.001f * fs));
                mdGain[0] = pulseNorm (f0, pulseN) * (1.0f - 0.7f * S.b) * 0.8f; mdGain[1] = pulseNorm (f1, pulseN) * (0.3f + 0.7f * S.b) * 0.8f;
                env[1].set (expMap (S.decay, 0.08f, 0.6f), fs); env[1].fire (S.a * 1.1f);
                flt[0].set (expMap (S.tone, 1400.0f, 5500.0f), 0.73f, fs);
                lpA.set (expMap (S.tone, 3500.0f, 12000.0f), fs);
                setLen (0.02f, 3.0f);
                break;
            }
            case 1:   // VCO: dos osciladores con caída de altura + ruido
                f0 = 185.0f * tr; f1 = 330.0f * tr; glide = 0.05f + 0.7f * S.b; pe = 1.0f; glideC = tauCoef (0.011f, fs);
                env[0].set (0.12f + 0.3f * S.decay, fs); env[0].fire();
                env[1].set (expMap (S.decay, 0.09f, 0.75f), fs); env[1].fire (S.a * 1.1f);
                flt[0].set (expMap (S.tone, 2500.0f, 15000.0f), 0.6f, fs);
                hpA.set (700.0f, fs);
                setLen (0.02f, 3.0f);
                break;
            case 2:   // Hexagonal: oscilador con barrido + ruido mezclado + clic
                f0 = 190.0f * tr; glide = 1.6f * S.b; pe = 1.0f; glideC = tauCoef (0.035f, fs);
                env[0].set (expMap (S.decay, 0.1f, 1.3f), fs); env[0].fire();
                env[1].setTau (0.0004f, fs); env[1].fire (0.6f);
                flt[0].set (expMap (S.tone, 1200.0f, 12000.0f), 0.8f, fs);
                k0 = S.a;
                setLen (0.02f, 4.0f);
                break;
            case 3:   // Caja de ruido barata: un resonador corto + ruido en pasa banda
                f0 = 225.0f * tr; md[0].setFreq (f0, fs); md[0].setDecay (0.05f + 0.2f * S.b, fs);
                md[0].strike (0.7f);
                env[1].set (expMap (S.decay, 0.04f, 0.45f), fs); env[1].fire (0.5f + S.a);
                flt[0].set (expMap (S.tone, 1500.0f, 6500.0f), 0.9f, fs);
                setLen (0.02f, 2.0f);
                break;
            default:  // Crack (de hoy): cuerpo con caída, chasquido en pasa banda alto y cola de ruido
                f0 = 200.0f * tr; glide = 0.45f; pe = 1.0f; glideC = tauCoef (0.008f, fs);
                env[0].set (0.1f + 0.35f * S.b, fs); env[0].fire();
                env[1].setTau (0.006f, fs); env[1].fire (S.a * 1.6f);
                env[2].set (expMap (S.decay, 0.1f, 0.95f), fs); env[2].fire (0.75f);
                flt[0].set (4200.0f, 1.5f, fs);
                flt[1].set (expMap (S.tone, 3000.0f, 16000.0f), 0.7f, fs);
                hpA.set (1400.0f, fs);
                setLen (0.02f, 3.0f);
                break;
        }
    }
    inline float snare()
    {
        switch (model)
        {
            case 0:
            {
                const float x = t < pulseN ? 1.0f : 0.0f;
                const float body = md[0].process (x * mdGain[0]) + md[1].process (x * mdGain[1]);
                flt[0].process (nz.next());
                const float noise = lpA.process (flt[0].hp) * env[1].tick();
                return fastTanh ((body + noise) * (1.15f + accent)) * 0.95f;
            }
            case 1:
            {
                pe *= glideC;
                const float g = 1.0f + glide * pe;
                const float a = sin2pi (osc[0].tick (f0 * g, fs)), b = triFromPhase (osc[1].tick (f1 * g, fs));
                const float body = (0.62f * a + 0.38f * b) * env[0].tick();
                const float noise = hpA.process (flt[0].lowpass (nz.next())) * env[1].tick();
                return fastTanh ((body + noise) * (1.1f + accent)) * 0.95f;
            }
            case 2:
            {
                pe *= glideC;
                const float f = f0 * fastExp2 (glide * pe);
                const float p = osc[0].tick (f, fs);
                const float e = env[0].tick();
                const float tone = 0.8f * sin2pi (p) + 0.2f * triFromPhase (p);
                const float noise = flt[0].lowpass (nz.next()) * 1.4f;
                const float y = (tone * (1.0f - 0.6f * k0) + noise * k0) * e + env[1].tick();
                return fastTanh (y * (1.1f + accent)) * 0.95f;
            }
            case 3:
            {
                const float body = md[0].process();
                const float noise = flt[0].bandpass (nz.next()) * env[1].tick() * 1.3f;
                return fastTanh ((body + noise) * (1.2f + accent)) * 0.95f;
            }
            default:
            {
                pe *= glideC;
                const float body = sin2pi (osc[0].tick (f0 * (1.0f + glide * pe), fs)) * env[0].tick() * (0.4f + 0.6f * S.b);
                const float w = nz.next();
                const float crack = flt[0].bandpass (w) * env[1].tick();
                const float tail = flt[1].lowpass (hpA.process (w)) * env[2].tick();
                return fastTanh ((body + crack + tail) * (1.5f + accent)) * 0.9f;
            }
        }
    }

    // ── Aplauso: ráfagas de ruido en pasa banda con envolvente de diente de sierra, y la cola ──
    void setupClap()
    {
        const float spacing = (model == 2 ? 0.004f + 0.006f * S.a : 0.006f + 0.010f * S.a) * fs;
        nBursts = model == 0 ? 4 : (model == 1 ? 5 : 3);
        int at = 0;
        for (int i = 0; i < nBursts; ++i)
        {
            burstAt[(size_t) i] = at;
            const float jit = model == 1 ? 0.7f + 0.6f * nz.uni() : 1.0f;
            at += std::max (8, (int) (spacing * jit));
        }
        burstLen = (int) spacing;
        tailAt = burstAt[(size_t) nBursts - 1];
        const float c = expMap (S.tone, 550.0f, 2100.0f);                  // 1,07 kHz con Tone al medio (research §3.5)
        if (model == 0) { flt[0].set (c, 1.6f, fs); flt[1].set (c * 0.85f, 1.0f, fs); env[1].set (expMap (S.decay, 0.08f, 0.9f), fs); }
        else if (model == 1) { flt[0].set (c * 0.85f, 1.1f, fs); flt[2].set (c * 2.1f, 1.2f, fs); flt[1].set (c * 0.8f, 0.8f, fs); env[1].set (expMap (S.decay, 0.15f, 1.6f), fs); lpA.set (expMap (S.tone, 3000.0f, 12000.0f), fs); }
        else { flt[0].set (c * 1.5f, 1.4f, fs); flt[1].set (c * 1.3f, 1.0f, fs); env[1].set (expMap (S.decay, 0.05f, 0.45f), fs); }
        k1 = 0.25f + 0.75f * S.b;
        setLen ((float) tailAt / fs + 0.01f, 5.0f);
    }
    inline float clap()
    {
        // Envolvente de ráfagas: cada una salta a 1 y baja en rampa hasta la siguiente (el diente de sierra que se redispara).
        float e = 0.0f;
        if (t < tailAt) { int i = 0; while (i + 1 < nBursts && t >= burstAt[(size_t) i + 1]) ++i; const float u = (float) (t - burstAt[(size_t) i]) / (float) burstLen; e = std::max (0.0f, 1.0f - u); e *= e; }
        if (t == tailAt) env[1].fire (k1);
        const float w = nz.next();
        float burst = flt[0].bandpass (w);
        if (model == 1) burst = 0.75f * burst + 0.45f * flt[2].bandpass (w);
        float tail = flt[1].bandpass (w) * env[1].tick();
        if (model == 1) tail = lpA.process (tail);
        const float y = burst * e + tail;
        return fastTanh (y * (2.2f + accent)) * 0.9f;
    }

    // ── Rim ──
    void setupRim()
    {
        const float dk = 0.5f + 1.5f * S.decay;
        if (model == 0)
        {
            f0 = 455.0f * tr; f1 = 1667.0f * tr;                                 // research §3.6: 455 + 1.667 Hz, 10 ms a −20 dB
            md[0].setFreq (f0, fs); md[1].setFreq (f1, fs);
            md[0].setDecay ((0.02f + 0.04f * S.b) * dk, fs); md[1].setDecay ((0.012f + 0.025f * S.b) * dk, fs);
            md[0].strike (1.0f); md[1].strike (0.8f);
            env[1].setTau (0.0006f, fs); env[1].fire (S.a);
            hpA.set (1500.0f, fs); hpB.set (320.0f, fs);
            lpA.set (expMap (S.tone, 2500.0f, 16000.0f), fs);
        }
        else if (model == 1)
        {
            f0 = 690.0f * tr;
            const float st = 0.8f + 0.4f * S.b;
            const float r[3] = { 1.0f, 2.27f * st, 3.93f * st * st };
            for (int i = 0; i < 3; ++i) { md[(size_t) i].setFreq (f0 * r[i], fs); md[(size_t) i].setDecay (0.06f * dk / (1.0f + 0.8f * (float) i), fs); md[(size_t) i].strike (1.0f / (1.0f + 0.9f * (float) i)); }
            env[1].setTau (0.001f, fs); env[1].fire (S.a * 0.8f);
            hpA.set (1500.0f, fs);
            lpA.set (expMap (S.tone, 2500.0f, 16000.0f), fs);
        }
        else
        {
            f0 = 1300.0f * tr; glide = 1.0f * S.b + 0.2f; pe = 1.0f; glideC = tauCoef (0.005f, fs);
            env[0].set (expMap (S.decay, 0.02f, 0.25f), fs); env[0].fire();
            env[1].setTau (0.0015f, fs); env[1].fire (0.3f + S.a);
            hpA.set (1000.0f, fs);
            lpA.set (expMap (S.tone, 3000.0f, 16000.0f), fs);
        }
        setLen (0.005f, 2.0f);
    }
    inline float rim()
    {
        if (model == 0)
        {
            const float y = md[0].process() + md[1].process() + hpA.process (nz.next()) * env[1].tick();
            return lpA.process (fastTanh (hpB.process (y) * (2.6f + 2.0f * accent)) * 0.85f);
        }
        if (model == 1)
        {
            const float y = md[0].process() + md[1].process() + md[2].process() + hpA.process (nz.next()) * env[1].tick();
            return lpA.process (fastTanh (y * (1.3f + accent)) * 0.9f);
        }
        pe *= glideC;
        const float s = sin2pi (osc[0].tick (f0 * fastExp2 (glide * pe), fs)) * env[0].tick();
        const float n = hpA.process (nz.next()) * env[1].tick();
        return lpA.process (fastTanh ((s + n) * (1.2f + accent)) * 0.9f);
    }

    // ── Charles (cerrado y abierto) ──
    void setupHat()
    {
        const bool open = slot == kOpenHat;
        // Decaimientos (research §3.3): cerrado 50 ms a −20 dB con Decay al medio (T60 ≈ 0,15 s), abierto 90-600 ms.
        const float t60 = open ? expMap (S.decay, 0.25f, 4.0f) : expMap (S.decay, 0.04f, 0.6f);
        env[0].set (t60, fs); env[0].fire();
        const float tone = expMap (S.tone, 0.7f, 1.4f);
        switch (model)
        {
            case 0:   // Seis cuadradas → pasa banda de 7,1 kHz → pasa altos resonante cerca de 10,5 kHz; Color suma la banda de 3,44 kHz
                metal.set (metalFreqs(), 6, tr, 0.6f + 0.8f * S.b, fs); metal.resetPhases (hitSeed);
                flt[0].set (7100.0f * tone, 1.4f, fs);
                flt[1].set (std::min (10500.0f * tone, 0.42f * fs), 1.4f, fs);
                flt[2].set (3440.0f * tone, 1.8f, fs);
                break;
            case 1:   // Caja barata: ruido (con el grano de un generador digital lento) en un tanque resonante cerca de 9 kHz
                holdInc = expMap (1.0f - S.b, 5000.0f, fs) / fs;
                flt[0].set (std::min (7000.0f * tone, 0.42f * fs), 0.7f, fs);
                flt[1].set (std::min (9100.0f * tone, 0.42f * fs), 3.0f, fs);
                break;
            case 2:   // Twin Tank (research §3.9): ruido en un tanque de 9,1 kHz + tres cuadradas a 6.170, 5.620 y 4.080 Hz en un
                      // tanque de 5,6 kHz con su propia envolvente de 50 ms; Metal las mezcla, Ratio estira sus frecuencias
            {
                const float hz[3] = { 6170.0f, 5620.0f, 4080.0f };
                metal.set (hz, 3, tr, 0.6f + 0.8f * S.b, fs); metal.resetPhases (hitSeed);
                flt[0].set (std::min (9100.0f * tone, 0.42f * fs), 3.0f, fs);
                flt[1].set (std::min (5600.0f * tone, 0.42f * fs), 3.0f, fs);
                flt[2].set (std::min (5000.0f * tone, 0.42f * fs), 0.7f, fs);
                env[1].set (0.15f, fs); env[1].fire();
                break;
            }
            default:
                f0 = 1210.0f * tr; f1 = f0 * (1.41f + 1.2f * S.b);
                k0 = 1.5f + 6.0f * S.a;
                flt[0].set (expMap (S.tone, 2000.0f, 9000.0f), 0.7f, fs);
                break;
        }
        setLen (0.004f, open ? 8.0f : 2.0f);
    }
    inline float hat()
    {
        const float e = env[0].tick() * std::min (1.0f, (float) t / (0.0002f * fs + 1.0f));
        switch (model)
        {
            case 0:
            {
                const float m = metal.process();
                const float hi = flt[1].highpass (flt[0].bandpass (m));
                const float lo = flt[2].bandpass (m) * S.a * 0.6f;
                return (hi * 3.4f + lo * 1.6f) * e * (1.0f + 0.3f * accent);
            }
            case 1:
            {
                holdPh += holdInc; if (holdPh >= 1.0f) { holdPh -= 1.0f; held = nz.next(); }
                const float hp = flt[0].highpass (held);
                const float ring = flt[1].bandpass (held) * S.a * 1.5f;
                return (hp * 1.1f + ring) * e * (1.0f + 0.3f * accent);
            }
            case 2:
            {
                const float body = flt[2].highpass (flt[0].bandpass (nz.next())) * 1.6f;
                const float beat = flt[1].bandpass (metal.process()) * 2.5f * env[1].tick();
                return (body * (1.0f - 0.5f * S.a) * e + beat * S.a) * (1.0f + 0.3f * accent);
            }
            default:
            {
                const float pm = osc[1].tick (f1, fs);
                const float idx = k0 * (0.3f + 0.7f * e);
                const float c = sin2pi (osc[0].tick (f0, fs) + idx * sin2pi (pm) * 0.159f);
                const float x = 0.8f * c + 0.25f * nz.next();
                return flt[0].highpass (x) * 1.2f * e * (1.0f + 0.3f * accent);
            }
        }
    }

    // ── Crash ──
    void setupCrash()
    {
        const float t60 = expMap (S.decay, 0.9f, 7.0f);                    // 808: 350-1.200 ms a −20 dB (T60 ≈ 1-3,6 s)
        env[0].set (t60, fs); env[0].fire(); env[1].set (t60 * 0.35f, fs); env[1].fire();
        const float tone = expMap (S.tone, 0.7f, 1.4f);
        switch (model)
        {
            case 0:
                metal.set (metalFreqs(), 6, tr, 0.6f + 0.8f * S.b, fs); metal.resetPhases (hitSeed);
                flt[0].set (3440.0f * tone, 1.5f, fs); flt[1].set (7100.0f * tone, 1.5f, fs); flt[2].set (9000.0f * tone, 0.7f, fs);
                break;
            case 1:
                flt[0].set (2800.0f * tone, 0.8f, fs); flt[1].set (5600.0f * tone, 0.8f, fs); flt[2].set (9200.0f * tone, 0.8f, fs); flt[3].set (4000.0f, 0.7f, fs);
                k0 = 6.0f + 5.0f * (float) hashUni (hitSeed, 1, 9); k1 = expMap (S.b, 0.001f, 0.6f) * fs;
                break;
            default:
                f0 = 1430.0f * tr * tone; f1 = f0 * (1.37f + 1.4f * S.b); k0 = 2.0f + 7.0f * S.a;
                flt[0].set (3000.0f * tone, 0.7f, fs);
                break;
        }
        setLen (0.01f, 12.0f);
    }
    inline float crash()
    {
        const float atk = std::min (1.0f, (float) t / (0.001f * fs));
        const float eh = env[0].tick() * atk, el = env[1].tick() * atk;
        switch (model)
        {
            case 0:
            {
                const float m = metal.process();
                const float lo = flt[0].bandpass (m) * el * (0.3f + 0.9f * S.a);
                const float mid = flt[1].bandpass (m) * eh;
                const float hi = flt[2].highpass (m) * eh;
                const float n = nz.next() * 0.12f * eh;
                return (lo * 1.4f + mid * 2.2f + hi * 2.0f + n) * (1.0f + 0.3f * accent);
            }
            case 1:
            {
                const float w = nz.next();
                const float sw = k1 > 1.0f ? std::min (1.0f, (float) t / k1) : 1.0f;
                const float sh = 1.0f + 0.35f * S.a * sin2pi (k0 * (float) t / fs);
                const float y = flt[0].bandpass (w) * 0.8f + flt[1].bandpass (w) + flt[2].bandpass (w) * 0.9f + flt[3].highpass (w) * 0.3f;
                return y * eh * sw * sh * 1.1f * (1.0f + 0.3f * accent);
            }
            default:
            {
                const float idx = k0 * (0.4f + 0.6f * eh);
                const float c = sin2pi (osc[0].tick (f0, fs) + idx * 0.159f * sin2pi (osc[1].tick (f1, fs)));
                const float x = 0.7f * c + 0.35f * nz.next();
                return flt[0].highpass (x) * eh * 1.1f * (1.0f + 0.3f * accent);
            }
        }
    }

    // ── Ride ──
    void setupRide()
    {
        const float t60 = expMap (S.decay, 0.8f, 6.0f);
        env[0].set (t60, fs); env[0].fire();
        const float tone = expMap (S.tone, 0.7f, 1.4f);
        switch (model)
        {
            case 0:
                metal.set (metalFreqs(), 6, tr, 0.6f + 0.8f * S.b, fs); metal.resetPhases (hitSeed);
                flt[0].set (3200.0f * tone, 1.4f, fs); flt[1].set (5600.0f * tone, 1.8f, fs);
                md[0].setFreq (2350.0f * tr, fs); md[0].setDecay (t60 * 0.5f, fs); md[0].strike (S.a * 0.6f);
                md[1].setFreq (3410.0f * tr, fs); md[1].setDecay (t60 * 0.35f, fs); md[1].strike (S.a * 0.35f);
                break;
            case 1:
            {
                const float st = 0.85f + 0.3f * S.b;
                const float r[6] = { 1.0f, 1.54f, 2.08f, 2.67f, 3.24f, 4.13f };
                for (int i = 0; i < 6; ++i)
                {
                    const float ri = 1.0f + (r[i] - 1.0f) * st;
                    md[(size_t) i].setFreq (640.0f * tr * tone * ri, fs);
                    md[(size_t) i].setDecay (t60 * (1.0f - 0.12f * (float) i), fs);
                    md[(size_t) i].strike (0.32f / (1.0f + 0.25f * (float) i));
                }
                metal.set (metalFreqs(), 6, tr * 1.6f, 1.0f, fs); metal.resetPhases (hitSeed);
                flt[0].set (6500.0f * tone, 0.7f, fs);
                break;
            }
            default:
                f0 = 940.0f * tr * tone; f1 = f0 * (1.48f + 1.2f * S.b); k0 = 1.0f + 5.0f * S.a;
                flt[0].set (2500.0f * tone, 0.7f, fs);
                break;
        }
        setLen (0.01f, 12.0f);
    }
    inline float ride()
    {
        const float e = env[0].tick() * std::min (1.0f, (float) t / (0.0005f * fs));
        switch (model)
        {
            case 0:
            {
                const float m = metal.process();
                const float wash = (flt[0].bandpass (m) * 1.6f + flt[1].bandpass (m) * 1.8f) * e;
                return (wash + md[0].process() + md[1].process()) * (1.0f + 0.3f * accent);
            }
            case 1:
            {
                float y = 0; for (int i = 0; i < 6; ++i) y += md[(size_t) i].process();
                return (y + flt[0].highpass (metal.process()) * S.a * 1.5f * e) * (1.0f + 0.3f * accent);
            }
            default:
            {
                const float idx = k0 * (0.5f + 0.5f * e);
                const float c = sin2pi (osc[0].tick (f0, fs) + idx * 0.159f * sin2pi (osc[1].tick (f1, fs)));
                return flt[0].highpass (0.8f * c + 0.15f * nz.next()) * e * 0.9f * (1.0f + 0.3f * accent);
            }
        }
    }

    // ── Toms ──
    void setupTom()
    {
        const float base = tomBase (slot) * tr;
        switch (model)
        {
            case 0:   // research §3.6: más agudo mientras suena fuerte (los diodos acortan la constante del puente): la
                      // altura baja con la amplitud; ruido rosado grave con una caída un poco más larga (τ 86 ms, 400 Hz)
            {
                const float t60 = expMap (S.decay, 0.15f, 1.6f);
                f0 = base; glide = 0.05f + 0.5f * S.b; pe = glide; glideC = t60Coef (t60 * 0.5f, fs);
                md[0].setDecay (t60, fs);
                pulseN = std::max (1, (int) (0.001f * fs)); pulseGain = pulseNorm (f0 * (1.0f + glide), pulseN);
                env[1].setTau (0.086f, fs); env[1].fire (S.a * 0.5f);
                lpB.set (400.0f, fs);
                lpA.set (expMap (S.tone, 300.0f, 9000.0f), fs);
                break;
            }
            case 1:
                f0 = base; glide = 2.2f * S.b; pe = 1.0f; glideC = tauCoef (0.04f + 0.22f * S.decay, fs);
                env[0].set (expMap (S.decay, 0.18f, 2.6f), fs); env[0].fire();
                env[1].setTau (0.0005f, fs); env[1].fire (0.5f);
                lpA.set (expMap (S.tone, 400.0f, 12000.0f), fs);
                k0 = S.a;
                break;
            case 2:
                f0 = base; glide = 0.3f + 2.5f * S.b; pe = 1.0f; pe2 = 1.0f; glideC = tauCoef (0.006f, fs); glide2C = tauCoef (0.06f, fs);
                env[0].set (expMap (S.decay, 0.15f, 1.3f), fs); env[0].fire();
                env[1].setTau (0.04f, fs); env[1].fire (S.a * 0.5f);
                lpB.set (2500.0f, fs);
                lpA.set (expMap (S.tone, 400.0f, 12000.0f), fs);
                break;
            default:
                f0 = base * 2.4f; glide = 0.02f + 0.4f * S.b; pe = glide; glideC = tauCoef (0.012f, fs);
                md[0].setDecay (expMap (S.decay, 0.08f, 0.7f), fs);
                pulseN = std::max (1, (int) (0.0006f * fs)); pulseGain = pulseNorm (f0 * (1.0f + glide), pulseN);
                env[1].setTau (0.0018f, fs); env[1].fire (S.a * 1.2f);
                hpA.set (1500.0f, fs);
                lpA.set (expMap (S.tone, 1000.0f, 14000.0f), fs);
                break;
        }
        setLen (0.02f, 8.0f);
    }
    inline float tom()
    {
        switch (model)
        {
            case 0:
            {
                pe *= glideC;
                md[0].setFreq (f0 * (1.0f + pe), fs);
                const float x = t < pulseN ? pulseGain : 0.0f;
                const float y = md[0].process (x) + lpB.process (nz.next()) * env[1].tick() * 2.0f;
                return fastTanh (lpA.process (y) * (1.1f + accent)) * 0.95f;
            }
            case 1:
            {
                pe *= glideC;
                const float p = osc[0].tick (f0 * fastExp2 (glide * pe), fs);
                const float e = env[0].tick() * std::min (1.0f, (float) t / (0.001f * fs));
                const float tone = 0.85f * sin2pi (p) + 0.15f * triFromPhase (p);
                const float y = (tone * (1.0f - 0.5f * k0) + nz.next() * k0 * 0.8f) * e + env[1].tick() * nz.next();
                return fastTanh (lpA.process (y) * (1.1f + accent)) * 0.95f;
            }
            case 2:
            {
                pe *= glideC; pe2 *= glide2C;
                const float p = osc[0].tick (f0 * (1.0f + glide * (0.4f * pe + 0.6f * pe2) * 0.5f), fs);
                const float tone = (0.8f * sin2pi (p) + 0.2f * triFromPhase (p)) * env[0].tick();
                const float y = tone + lpB.process (nz.next()) * env[1].tick() * 2.0f;
                return fastTanh (lpA.process (y) * (1.1f + accent)) * 0.95f;
            }
            default:
            {
                pe *= glideC;
                md[0].setFreq (f0 * (1.0f + pe), fs);
                const float x = t < pulseN ? pulseGain : 0.0f;
                const float y = md[0].process (x) + hpA.process (nz.next()) * env[1].tick();
                return fastTanh (lpA.process (y) * (1.2f + accent)) * 0.92f;
            }
        }
    }

    // ── Cencerro ──
    void setupCowbell()
    {
        const float t60 = expMap (S.decay, 0.08f, 1.4f);
        switch (model)
        {
            case 0:   // research §3.4: 540 + 800 Hz por un pasa banda (≈ 2,6 kHz, Q 3, sin verificar) y un pasa altos suave;
                      // envolvente de dos tramos: una caída brusca de ~10 dB y la cola (50 ms a −20 dB con Decay al medio)
            {
                const float hz[2] = { 540.0f, 800.0f };
                metal.set (hz, 2, tr, 0.6f + 0.8f * S.b, fs); metal.resetPhases (hitSeed);
                flt[0].set (expMap (S.tone, 1500.0f, 4500.0f), 3.0f, fs);
                hpA.set (500.0f, fs);
                lpA.set (900.0f, fs);
                env[0].setTau (0.003f, fs); env[0].fire (0.7f); env[1].set (expMap (S.decay, 0.06f, 0.8f), fs); env[1].fire (0.26f);
                break;
            }
            case 1:
                f0 = 562.0f * tr; f1 = f0 * (1.41f + 0.6f * S.b); k0 = 1.0f + 7.0f * S.a;
                env[0].setTau (0.006f, fs); env[0].fire (0.5f); env[1].set (t60, fs); env[1].fire (0.55f);
                flt[0].set (expMap (S.tone, 900.0f, 4000.0f), 0.8f, fs);
                break;
            default:
            {
                const float st = 0.85f + 0.3f * S.b;
                const float r[3] = { 1.0f, 2.5f * st, 4.3f * st };
                for (int i = 0; i < 3; ++i) { md[(size_t) i].setFreq (820.0f * tr * r[i] * expMap (S.tone, 0.8f, 1.25f), fs); md[(size_t) i].setDecay (t60 / (1.0f + 0.7f * (float) i), fs); md[(size_t) i].strike (0.55f / (1.0f + 0.5f * (float) i)); }
                env[2].setTau (0.0015f, fs); env[2].fire (S.a * 0.7f);
                hpA.set (2000.0f, fs);
                break;
            }
        }
        setLen (0.005f, 4.0f);
    }
    inline float cowbell()
    {
        switch (model)
        {
            case 0:
            {
                const float m = metal.process();
                const float e = env[0].tick() + env[1].tick();
                const float y = hpA.process (flt[0].bandpass (m)) * 1.8f + lpA.process (m) * S.a * 0.6f;
                return fastTanh (y * e * (1.3f + accent)) * 1.0f;
            }
            case 1:
            {
                const float e = env[0].tick() + env[1].tick();
                const float c = sin2pi (osc[0].tick (f0, fs) + k0 * e * 0.159f * sin2pi (osc[1].tick (f1, fs)));
                return fastTanh (flt[0].bandpass (c) * 1.4f * e * (1.2f + accent)) * 1.05f;
            }
            default:
            {
                const float y = md[0].process() + md[1].process() + md[2].process() + hpA.process (nz.next()) * env[2].tick();
                return fastTanh (y * (1.1f + accent)) * 0.95f;
            }
        }
    }

    // ── Clave ──
    void setupClave()
    {
        switch (model)
        {
            case 0:
                f0 = 2500.0f * tr; md[0].setFreq (f0, fs); md[0].setDecay (expMap (S.decay, 0.02f, 0.2f) * (0.7f + 0.6f * S.b), fs); md[0].strike (1.0f);
                md[1].setFreq (f0 * 2.61f, fs); md[1].setDecay (0.015f, fs); md[1].strike (0.25f * S.b);
                env[1].setTau (0.0005f, fs); env[1].fire (S.a * 0.6f);
                hpA.set (1200.0f, fs);
                break;
            case 1:
            {
                const float st = 0.85f + 0.3f * S.b;
                const float r[3] = { 1.0f, 2.71f * st, 4.79f * st };
                for (int i = 0; i < 3; ++i) { md[(size_t) i].setFreq (1050.0f * tr * r[i], fs); md[(size_t) i].setDecay (expMap (S.decay, 0.04f, 0.3f) / (1.0f + 1.2f * (float) i), fs); md[(size_t) i].strike (0.8f / (1.0f + 0.8f * (float) i)); }
                env[1].setTau (0.0012f, fs); env[1].fire (S.a * 0.8f);
                hpA.set (2500.0f, fs);
                break;
            }
            default:
                f0 = 1600.0f * tr; glide = 0.5f + 1.5f * S.b; pe = 1.0f; glideC = tauCoef (0.004f + 0.012f * S.b, fs);
                env[0].set (expMap (S.decay, 0.03f, 0.35f), fs); env[0].fire();
                env[1].setTau (0.001f, fs); env[1].fire (S.a * 0.6f);
                hpA.set (2000.0f, fs);
                break;
        }
        lpA.set (expMap (S.tone, 3000.0f, 18000.0f), fs);
        setLen (0.003f, 2.0f);
    }
    inline float clave()
    {
        if (model == 0) return lpA.process (fastTanh ((md[0].process() + md[1].process() + hpA.process (nz.next()) * env[1].tick()) * (1.2f + accent)) * 0.9f);
        if (model == 1) return lpA.process (fastTanh ((md[0].process() + md[1].process() + md[2].process() + hpA.process (nz.next()) * env[1].tick()) * (1.1f + accent)) * 0.92f);
        pe *= glideC;
        const float s = sin2pi (osc[0].tick (f0 * fastExp2 (glide * pe), fs)) * env[0].tick();
        return lpA.process (fastTanh ((s + hpA.process (nz.next()) * env[1].tick()) * (1.1f + accent)) * 0.92f);
    }

    // ── Pandereta, maracas, shaker ──
    void setupTamb()
    {
        switch (model)
        {
            case 0:   // research §3.5: ruido por un pasa altos de ≈ 10,6 kHz (Q 2,3), 25-35 ms
                env[0].set (expMap (S.decay, 0.03f, 0.4f), fs); env[0].fire();
                k0 = (0.001f + 0.014f * S.a) * fs;
                flt[0].set (std::min (expMap (S.tone, 6000.0f, 16000.0f), 0.42f * fs), 2.3f, fs);
                break;
            case 1:
            {
                // Sonajas: seis modos agudos golpeados en ráfagas desfasadas (el aro sacudido).
                const float tone = expMap (S.tone, 0.8f, 1.25f);
                const float hz[6] = { 5100.0f, 6830.0f, 7920.0f, 9350.0f, 10400.0f, 12100.0f };
                for (int i = 0; i < 6; ++i)
                {
                    md[(size_t) i].setFreq (std::min (hz[i] * tone * tr, 0.45f * fs), fs);
                    md[(size_t) i].setDecay (expMap (S.decay, 0.06f, 0.7f) * (1.0f - 0.08f * (float) i), fs);
                }
                nBursts = 4;
                for (int i = 0; i < nBursts; ++i) burstAt[(size_t) i] = (int) ((float) i * (0.002f + 0.009f * S.a) * fs * (0.7f + 0.6f * (float) hashUni (hitSeed, (uint64_t) i, 11)));
                env[0].set (expMap (S.decay, 0.05f, 0.5f), fs); env[0].fire (S.b);
                flt[0].set (7000.0f * tone, 0.7f, fs);
                break;
            }
            default:
                env[0].set (expMap (S.decay, 0.05f, 0.5f), fs); env[0].fire();
                k0 = (0.008f + 0.05f * S.a) * fs;
                flt[0].set (expMap (S.tone, 4000.0f, 10000.0f), 1.2f, fs);
                break;
        }
        setLen (0.003f, 3.0f);
    }
    inline float tamb()
    {
        if (model == 1)
        {
            float y = 0;
            for (int b = 0; b < nBursts; ++b)
                if (t == burstAt[(size_t) b])
                    for (int i = 0; i < 6; ++i) md[(size_t) i].strike ((0.18f + 0.12f * (float) hashUni (hitSeed, (uint64_t) (b * 8 + i), 13)) * (b == 0 ? 1.0f : 0.7f));
            for (int i = 0; i < 6; ++i) y += md[(size_t) i].process();
            y += flt[0].highpass (nz.next()) * env[0].tick() * 0.5f;
            return fastTanh (y * (1.0f + 0.5f * accent)) * 1.0f;
        }
        // Maracas y shaker: ruido con ataque en rampa; el grano son las bolitas (ruido cortado al azar).
        const float atk = std::min (1.0f, (float) t / std::max (1.0f, k0));
        const float e = (float) t < k0 ? atk * env[0].v : env[0].tick();
        float w = nz.next();
        if (S.b > 0.01f) { const float g = nz.uni(); w *= g < 1.0f - 0.85f * S.b ? 1.0f : 3.0f * S.b; }
        const float y = model == 0 ? flt[0].highpass (w) : flt[0].bandpass (w) * 1.2f;
        return y * e * 0.9f * (1.0f + 0.3f * accent);
    }

    // ── FX ──
    void setupFx()
    {
        switch (model)
        {
            case 0:   // Zap: barrido empinado desde muy arriba (el láser de los tambores electrónicos)
                f0 = 90.0f * tr; glide = 2.0f + 5.0f * S.b;
                env[0].set (expMap (S.decay, 0.05f, 1.2f), fs); env[0].fire();
                glideC = tauCoef (expMap (S.decay, 0.05f, 1.2f) * 0.12f, fs); pe = 1.0f;
                break;
            case 1:   // Barrido de ruido resonante, hacia arriba o abajo
                env[0].set (expMap (S.decay, 0.2f, 4.0f), fs); env[0].fire();
                k0 = 4.0f * (S.b - 0.5f) * 2.0f;                                   // octavas de recorrido (con signo)
                k1 = expMap (S.tone, 200.0f, 4000.0f);
                k2 = 1.0f + 12.0f * S.a;
                pe = 0.0f; glideC = 1.0f / std::max (1.0f, expMap (S.decay, 0.2f, 4.0f) * 0.5f * fs);
                break;
            case 2:   // Metal: yunque (FM inarmónica) + modos
                f0 = 330.0f * tr; f1 = f0 * (1.37f + S.b); k0 = 3.0f + 8.0f * S.a;
                env[0].set (expMap (S.decay, 0.2f, 3.0f), fs); env[0].fire();
                {
                    const float r[3] = { 1.0f, 2.76f, 5.4f };
                    for (int i = 0; i < 3; ++i) { md[(size_t) i].setFreq (f0 * 1.9f * r[i], fs); md[(size_t) i].setDecay (expMap (S.decay, 0.2f, 3.0f) * 0.6f / (1.0f + (float) i), fs); md[(size_t) i].strike (0.25f); }
                }
                lpA.set (expMap (S.tone, 2000.0f, 16000.0f), fs);
                break;
            case 3:   // Caída sub: seno que baja varias octavas en el largo del golpe
                f0 = 30.0f * tr; glide = 1.0f + 3.0f * S.b; pe = 1.0f;
                env[0].set (expMap (S.decay, 0.3f, 4.0f), fs); env[0].fire();
                glideC = tauCoef (expMap (S.decay, 0.3f, 4.0f) * 0.25f, fs);
                k0 = 1.0f + 5.0f * S.a;
                break;
            default:  // Estática: crujidos al azar en pasa banda más soplido
                env[0].set (expMap (S.decay, 0.1f, 3.0f), fs); env[0].fire();
                k0 = expMap (S.a, 150.0f, 8000.0f) / fs;
                flt[0].set (expMap (S.tone, 700.0f, 8000.0f), 2.0f, fs);
                k1 = S.b;
                break;
        }
        lpB.set (expMap (S.tone, 800.0f, 16000.0f), fs);
        setLen (0.01f, 10.0f);
    }
    inline float fx()
    {
        const float e = env[0].tick() * std::min (1.0f, (float) t / (0.0008f * fs));
        switch (model)
        {
            case 0:
            {
                pe *= glideC;
                const float p = osc[0].tick (f0 * fastExp2 (glide * pe), fs);
                const float y = (0.8f * sin2pi (p) + 0.2f * triFromPhase (p)) * (1.0f - 0.5f * S.a) + nz.next() * S.a * 0.6f;
                return lpB.process (y * e) * (1.0f + 0.3f * accent);
            }
            case 1:
            {
                pe = std::min (1.0f, pe + glideC);
                const float fc = clampf (k1 * fastExp2 (k0 * pe), 30.0f, 0.42f * fs);
                flt[0].set (fc, k2, fs);
                flt[0].process (nz.next());
                return (flt[0].lp * 0.6f + flt[0].bp * 0.9f) * e * 0.9f * (1.0f + 0.3f * accent);
            }
            case 2:
            {
                const float c = sin2pi (osc[0].tick (f0, fs) + k0 * e * 0.159f * sin2pi (osc[1].tick (f1, fs)));
                const float y = c * e * 0.7f + md[0].process() + md[1].process() + md[2].process();
                return lpA.process (fastTanh (y * (1.2f + accent)));
            }
            case 3:
            {
                pe *= glideC;
                const float s = sin2pi (osc[0].tick (f0 * fastExp2 (glide * pe), fs));
                return fastTanh (s * e * k0) / fastTanh (k0) * (1.0f + 0.2f * accent);
            }
            default:
            {
                float imp = 0.0f;
                if (nz.uni() < k0) imp = nz.next() * 3.0f;
                const float y = flt[0].bandpass (imp) + nz.next() * 0.25f * k1;
                return lpB.process (y) * e * (1.0f + 0.3f * accent);
            }
        }
    }
};

} // namespace dh::pal
