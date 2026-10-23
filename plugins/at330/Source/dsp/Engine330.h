#pragma once
// Motor del AT330 (docs/research.md): un oscilador maestro con divisores (las 12 notas de la octava de arriba y las
// demás por mitades, todas en fase), compuertas por tecla con un release común, y tres secciones como en el original:
//   - cuerdas: la forma de 4' por un filtro de tono fijo, el VCA con su ataque y el control TONE;
//   - coro: tres voces (masculina 8', masculina 4', femenina 4') que comparten un banco de 7 filtros formantes fijos
//     (200, 230, 600, 900, 1300, 2800 y 3300 Hz), cada voz entra a 4 de ellos;
//   - vocoder: 10 bandas de análisis sobre la voz (con preénfasis) que gobiernan 10 bandas de síntesis sobre el
//     portador (el 8' de las teclas o una entrada externa), el paso directo de las consonantes agudas, Hold.
// El coro y el vocoder pasan por un BBD de vibrato; después el ensemble: cuatro BBD con dos LFO compuestos (triángulo
// lento + seno rápido) y dos salidas. Lo que el original no tenía (polifonía de envolventes, velocity, freeze, corrimiento
// de formantes, reverb) está marcado como "extra".
//
// Sin JUCE y determinista: todo corre muestra a muestra, los eventos entran en su muestra y el resultado no depende del
// tamaño de bloque. Sin std::exp/std::tan por muestra (fastExp2/fastTan de Common.h).

#include "Common.h"
#include "Blep.h"
#include "Fx.h"
#include <array>
#include <vector>

namespace dh::vp {

// ── Constantes del hardware (docs/research.md §2-§6; las que llevan "inferido" no están publicadas) ──
// Divisores del generador de la octava de arriba (familia AY-3-0214 / MK50240, reloj 2,00024 MHz): C8..B8. El
// redondeo a enteros deja cada nota a menos de 2 cents del temperamento igual; el AT330 lo conserva.
constexpr int kTopDivisor[12] = { 478, 451, 426, 402, 379, 358, 338, 319, 301, 284, 268, 253 };
constexpr double kTopClock = 2000240.0;
// Niveles medidos en las salidas de 8' y 4' por octava (hoja de ajuste del AGH17: C2..C6), en V.
constexpr float kLevel8[5] = { 2.9f, 2.4f, 2.0f, 1.8f, 1.7f };
constexpr float kLevel4[5] = { 1.5f, 1.6f, 1.6f, 1.6f, 1.6f };
// Curvatura de la carga exponencial de los dientes de sierra (inferido de la forma en la hoja de ajuste).
constexpr float kCurve = 1.8f;
// Banco formante del coro.
constexpr float kChoirHz[7] = { 200, 230, 600, 900, 1300, 2800, 3300 };
// Qué filtros usa cada voz y con qué peso (inferido: el reparto está sólo en el esquema; ver research.md §3).
//                                     200   230   600   900   1300  2800  3300
constexpr float kChoirMix[3][7] = { { 1.0f, 0.0f, 1.0f, 0.8f, 0.0f, 0.35f, 0.0f },     // masculina 8'
                                    { 0.0f, 0.8f, 1.0f, 0.0f, 0.7f, 0.35f, 0.0f },     // masculina 4'
                                    { 0.0f, 0.5f, 0.0f, 1.0f, 0.8f, 0.0f, 0.4f } };   // femenina 4'
constexpr float kChoirQ[7] = { 2.5f, 2.5f, 5.0f, 5.0f, 5.0f, 6.0f, 6.0f };
// Vocoder: centros de la tabla de ajuste del FLH16.
constexpr int kBands = 10;
constexpr float kVocHz[kBands] = { 200, 280, 400, 600, 900, 1300, 2000, 2800, 4000, 6000 };
constexpr float kVocQ = 3.2f;                  // "high Q" en el texto; el número es inferido
constexpr float kHoldSeconds = 2.5f;           // Hold: la descarga de los detectores se alarga a 2-3 s
// Ensemble (ETH09): períodos medidos de los cuatro LFO.
constexpr float kEnsSlowT[2] = { 3.9f, 4.6f }, kEnsFastT[2] = { 0.150f, 0.175f };
constexpr float kEnsBaseMs = 5.12f;            // 512 etapas a ~50 kHz de reloj
constexpr float kEnsSlowMs = 1.2f, kEnsFastMs = 0.15f;   // excursión (inferida, a oído contra grabaciones)
constexpr float kVibBaseMs = 2.56f, kVibMaxMs = 0.6f;    // BBD de vibrato (256 etapas) y su excursión máxima
constexpr float kDumpSeconds = 0.05f;          // modo clásico: cada tecla nueva descarga la envolvente 50 ms

enum Carrier { kCarrierKeys, kCarrierExternal, kCarrierBoth };
enum HoldMode { kHoldOff, kHoldOn, kHoldFreeze };
enum PitchMode { kPitchOff, kPitchAuto, kPitchManual };
enum Keying { kKeyingClassic, kKeyingPoly };

struct Params
{
    // Llaves (tablets): cuerdas 4' arriba/abajo; coro: arriba femenina 4' y masculina 8', abajo masculina 4' y 8';
    // vocoder 8' arriba/abajo; ensemble del coro y del vocoder (las cuerdas siempre pasan por él en el original).
    bool strUp = true, strLo = true;
    bool chFemUp = false, chM8Up = false, chM4Lo = false, chM8Lo = false;
    bool vocUp = false, vocLo = false;
    bool chEns = true, vocEns = true, strEns = true;          // strEns: extra
    float strAttack = 0.25f, strTone = 0.55f, strLevel = 0.8f;
    float chAttack = 0.15f, chLevel = 0.8f;
    float release = 1.2f;
    float micGain = 1.0f, vocLevel = 0.8f, vocTone = 0.6f, directMic = 0.0f, consonants = 0.5f;
    float vocResponse = 0.04f, formantShift = 0.0f, extGain = 1.0f;   // extra: tiempo del detector, corrimiento (oct)
    int carrier = kCarrierKeys, hold = kHoldOff;
    float vibRate = 6.0f, vibDepth = 0.3f, vibDelay = 0.9f, modVib = 0.5f;
    int pitchMode = kPitchOff;
    float pitchSet = 7.0f, pitchTime = 0.15f, pitchAmount = 0.0f, bendRange = 2.0f;
    float ensDepth = 1.0f, velocity = 0.0f;
    int keying = kKeyingPoly, split = 60;
    float tuneCents = 0.0f, spaceMix = 0.0f, spaceSize = 0.6f, volume = 1.0f;
};

// ── Formas de onda del divisor ──
// 8': carga exponencial durante todo el período y descarga brusca. 4': plana medio período y la misma carga en el
// otro medio. Las dos son unipolares (0..1); los saltos se corrigen con PolyBLEP y los quiebres con PolyBLAMP.
struct Wave
{
    float k = kCurve, norm = 1.0f, d0 = 0.0f, d1 = 0.0f;
    Wave() { norm = 1.0f / (1.0f - std::exp (-k)); d0 = k * norm; d1 = k * std::exp (-k) * norm; }
    inline float charge (float x) const { return (1.0f - fastExp (-k * x)) * norm; }
    inline float eight (float ph, float dt) const
    {
        float y = charge (ph);
        y -= 0.5f * polyBlep (ph, dt);
        y += (d0 - d1) * dt * polyBlamp (ph, dt);
        return y;
    }
    inline float four (float ph, float dt) const
    {
        float y = ph < 0.5f ? 0.0f : charge (2.0f * ph - 1.0f);
        float h = ph + 0.5f; if (h >= 1.0f) h -= 1.0f;               // fase relativa al quiebre de la mitad
        y += 2.0f * d0 * dt * polyBlamp (h, dt);
        y -= 0.5f * polyBlep (ph, dt);
        y -= 2.0f * d1 * dt * polyBlamp (ph, dt);
        return y;
    }
};

// Nivel de cada forma según la octava de la nota (la tabla medida, interpolada en octavas; C2 = MIDI 36).
inline float levelOf (const float* table, int note8)
{
    const float o = clampf ((float) (note8 - 36) / 12.0f, 0.0f, 4.0f);
    const int i = std::min (3, (int) o);
    return table[i] + (table[i + 1] - table[i]) * (o - (float) i);
}

// SVF TPT con las tres salidas.
struct Svf
{
    float g = 0, k = 1, a1 = 0, a2 = 0, a3 = 0, ic1 = 0, ic2 = 0, lp = 0, bp = 0, hp = 0;
    void set (float fc, float q, float fs)
    {
        g = prewarp (fc, fs); k = 1.0f / q;
        a1 = 1.0f / (1.0f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    inline void process (float v0)
    {
        const float v3 = v0 - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1; ic2 = 2.0f * v2 - ic2;
        lp = v2; bp = v1; hp = v0 - k * v1 - v2;
    }
    inline float bandNorm (float x) { process (x); return bp * k; }      // ganancia 1 en fc
    void reset() { ic1 = ic2 = lp = bp = hp = 0; }
};

// Una línea de BBD: filtro antialias a la entrada y de reconstrucción a la salida, una saturación suave y un piso de
// ruido muy bajo (el original los tenía; el compansor los escondía casi del todo).
struct Bbd
{
    DelayLine line; Svf pre, post; Rng rng { 0x5EED0001u };
    float fs = 48000.0f;
    void prepare (float sampleRate, float maxMs, uint32_t seed)
    {
        fs = sampleRate; line.prepare ((int) (fs * maxMs * 0.001f) + 8);
        pre.set (9000.0f, 0.7f, fs); post.set (8500.0f, 0.7f, fs); rng = Rng (seed);
    }
    inline void push (float x) { pre.process (x); line.push (fastTanh (pre.lp * 0.7f) * 1.4286f + 1.5e-5f * rng.bipolar()); }
    inline float read (float ms) { post.process (line.tap (std::max (1.0f, ms * 0.001f * fs))); return post.lp; }
    void restart (uint32_t seed) { line.restart(); pre.reset(); post.reset(); rng = Rng (seed); }
};

// ── Una tecla ──
struct Key
{
    bool down = false, held = false, active = false;   // held: suelta pero sostenida por el pedal
    float vel = 1.0f;
    // Compuerta por tecla: en clásico una sola (g[0]); en poly una por sección: cuerdas, coro, vocoder.
    std::array<float, 3> g {};
};

class Engine
{
public:
    void prepare (double sampleRate, int /*maxBlock*/)
    {
        fs = (float) sampleRate;
        computeTopInc();
        for (int s = 0; s < 4; ++s) ens[(size_t) s].prepare (fs, kEnsBaseMs + kEnsSlowMs * 1.6f + kEnsFastMs * 1.6f + 2.0f, 0x51u + (uint32_t) s);
        vibCh.prepare (fs, kVibBaseMs + kVibMaxMs * 2.5f + 2.0f, 0x71u); vibVoc.prepare (fs, kVibBaseMs + kVibMaxMs * 2.5f + 2.0f, 0x72u);
        reverb.prepare (fs);
        for (int i = 0; i < 7; ++i) choir[(size_t) i].set (kChoirHz[i], kChoirQ[i], fs);
        strHp.set (140.0f, 0.6f, fs); strPeak.set (1500.0f, 1.0f, fs);
        for (int i = 0; i < 6; ++i) consHp[(size_t) i].set (4500.0f, 0.707f, fs);
        configuredShift = -100.0f;
        restart();
    }

    // Como recién preparado: el estado de todo vuelve a cero (un reset del host suena igual que una instancia nueva).
    void restart()
    {
        for (auto& k : keys) k = Key();
        nActive = 0;
        rest(); idle = 0;
        for (int s = 0; s < 4; ++s) ens[(size_t) s].restart (0x51u + (uint32_t) s);
        vibCh.restart (0x71u); vibVoc.restart (0x72u);
        reverb.restart(); reverbTail = 0;
        for (auto& f : choir) f.reset();
        strHp.reset(); strPeak.reset(); strTone.reset(); vocToneF.reset();
        for (auto& f : consHp) f.reset();
        for (auto& b : ana) { b.a.reset(); b.b.reset(); b.env = 0.0f; }
        for (auto& b : syn) { b.a.reset(); b.b.reset(); }
        for (auto& d : dc) d.reset();
        for (auto& d : dc) d.setCutoff (18.0f, fs);
        dc[5].setCutoff (8.0f, fs); dc[6].setCutoff (8.0f, fs);
        preX = deY = 0.0f; carrierEnv = 0.0f; carrierGate = 0.0f;
        envStr = envCh = 0.0f; dump = 0;
        vibFade = 0.0f;
        pitchOffset = 0.0f; bend = 0.0f; modWheel = 0.0f; sustain = false;
        smoothReady = false;
        micPeak = 0.0f;
    }

    void set (const Params& np)
    {
        p = np;
        if (std::fabs (p.formantShift - configuredShift) > 1e-4f)
        {
            configuredShift = p.formantShift;
            for (int b = 0; b < kBands; ++b)
            {
                ana[(size_t) b].a.set (kVocHz[b], kVocQ, fs); ana[(size_t) b].b.set (kVocHz[b], kVocQ, fs);
                const float fc = clampf (kVocHz[b] * std::exp2 (p.formantShift), 60.0f, 0.42f * fs);
                syn[(size_t) b].a.set (fc, kVocQ, fs); syn[(size_t) b].b.set (fc, kVocQ, fs);
            }
        }
        if (! smoothReady)
        {
            for (int i = 0; i < kNumSmooth; ++i) { sm[(size_t) i].setTime (0.02f, fs); sm[(size_t) i].reset (target (i)); }
            smoothReady = true;
        }
    }
    const Params& params() const { return p; }

    // ── MIDI ──
    void noteOn (int note, int velocity)
    {
        if (note < 0 || note > 127) return;
        auto& k = keys[(size_t) note];
        const bool anyBefore = anyDown();
        k.down = true; k.held = false; k.vel = clampf ((float) velocity / 127.0f, 0.0f, 1.0f);
        if (! k.active) { k.active = true; k.g = { 0.0f, 0.0f, 0.0f }; addActive (note); }
        // Modo clásico: la envolvente de cada sección se descarga 50 ms y vuelve a atacar (todo el acorde).
        if (p.keying == kKeyingClassic) dump = (int) (kDumpSeconds * fs);
        // Pitch Shift en Auto: cada tecla arranca abajo y sube a su altura.
        if (p.pitchMode == kPitchAuto) pitchOffset = -p.pitchSet;
        // El vibrato vuelve a esperar su retardo sólo si no había ninguna tecla (legato lo conserva).
        if (! anyBefore) vibFade = 0.0f;
    }
    void noteOff (int note)
    {
        if (note < 0 || note > 127) return;
        auto& k = keys[(size_t) note];
        if (! k.down) return;
        k.down = false; k.held = sustain;
    }
    void setSustain (bool on)
    {
        sustain = on;
        if (! on) for (auto& k : keys) k.held = false;
    }
    void allNotesOff() { for (auto& k : keys) { k.down = false; k.held = false; } sustain = false; }
    // Todo en silencio ya (All Sound Off, hueco del host).
    void allSoundOff() { allNotesOff(); for (auto& k : keys) { k.g = { 0, 0, 0 }; } }
    void pitchBend (float v) { bend = clampf (v, -1.0f, 1.0f); }
    void setModWheel (float v) { modWheel = clampf (v, 0.0f, 1.0f); }

    // mic: la voz (modulador), carrier: el portador externo; los dos pueden ser nullptr.
    void process (float* L, float* R, const float* mic, const float* carrier, int n)
    {
        const float aFast = 1.0f - fastExp (-1.0f / (0.002f * fs));
        const float kRel = 1.0f - fastExp (-4.6f / (std::max (0.01f, p.release) * fs));
        const float kAtS = 1.0f - fastExp (-3.0f / (std::max (0.002f, p.strAttack) * fs));
        const float kAtC = 1.0f - fastExp (-3.0f / (std::max (0.002f, p.chAttack) * fs));
        const float kDump = 1.0f - fastExp (-1.0f / (0.003f * fs));
        const float detA = 1.0f - fastExp (-1.0f / (0.0015f * fs));
        // Freeze (extra): las bandas suben con la voz pero no bajan nunca (quedan con lo más fuerte que oyeron).
        const float detR = p.hold == kHoldFreeze ? 0.0f
                         : 1.0f - fastExp (-1.0f / ((p.hold == kHoldOn ? kHoldSeconds : std::max (0.003f, p.vocResponse)) * fs));
        const float kPitch = 1.0f - fastExp (-3.0f / (std::max (0.005f, p.pitchTime) * fs));
        const float vibInc = p.vibRate / fs, fadeInc = p.vibDelay > 0.01f ? 1.0f / (p.vibDelay * fs) : 1.0f;
        const float slowInc[2] = { 1.0f / (kEnsSlowT[0] * fs), 1.0f / (kEnsSlowT[1] * fs) };
        const float fastInc[2] = { 1.0f / (kEnsFastT[0] * fs), 1.0f / (kEnsFastT[1] * fs) };
        const bool classic = p.keying == kKeyingClassic;
        const float velAmt = classic ? 0.0f : p.velocity;
        const float aCarrier = 1.0f - fastExp (-1.0f / (0.02f * fs));

        for (int i = 0; i < n; ++i)
        {
            // Parámetros suavizados (por muestra: el resultado no depende del tamaño de bloque).
            float v[kNumSmooth];
            for (int s = 0; s < kNumSmooth; ++s) v[s] = sm[(size_t) s].process (target (s));

            // ── Altura: afinación, Pitch Shift (sólo hacia abajo, como el original), pitch bend ──
            float semis = v[kSmTune] / 100.0f + bend * p.bendRange;
            if (p.pitchMode == kPitchAuto) { pitchOffset += (0.0f - pitchOffset) * kPitch; semis += pitchOffset; }
            else if (p.pitchMode == kPitchManual) semis -= v[kSmPitchAmt] * p.pitchSet;
            const float ratio = fastExp2 (semis / 12.0f);
            // En reposo (ver más abajo) el divisor y los LFO quedan quietos en su fase inicial.
            const bool resting = idle >= kIdleSamples;
            for (int pc = 0; pc < 12 && ! resting; ++pc)
            {
                cnt[(size_t) pc] += topInc[(size_t) pc] * ratio;
                if (cnt[(size_t) pc] >= 65536.0) cnt[(size_t) pc] -= 65536.0;
            }

            // ── Envolventes de sección (modo clásico) ──
            const bool any = anyDown();
            if (classic)
            {
                if (dump > 0) { --dump; envStr += (0.0f - envStr) * kDump; envCh += (0.0f - envCh) * kDump; }
                else if (any) { envStr += (1.0f - envStr) * kAtS; envCh += (1.0f - envCh) * kAtC; }
            }
            else { envStr = 1.0f; envCh = 1.0f; }

            // ── Teclas: compuertas y formas de onda ──
            float busStr = 0, busM8 = 0, busM4 = 0, busF4 = 0, busVoc = 0;
            for (int a = 0; a < nActive;)
            {
                const int note = active[(size_t) a];
                auto& k = keys[(size_t) note];
                const bool on = k.down || k.held;
                const float gainVel = 1.0f - velAmt * (1.0f - k.vel);
                if (classic)
                {
                    k.g[0] += ((on ? 1.0f : 0.0f) - k.g[0]) * (on ? aFast : kRel);
                    k.g[1] = k.g[2] = k.g[0];
                }
                else
                {
                    k.g[0] += on ? (1.0f - k.g[0]) * kAtS : -k.g[0] * kRel;
                    k.g[1] += on ? (1.0f - k.g[1]) * kAtC : -k.g[1] * kRel;
                    k.g[2] += on ? (1.0f - k.g[2]) * aFast : -k.g[2] * kRel;
                }
                if (! on && k.g[0] < 1e-5f && k.g[1] < 1e-5f && k.g[2] < 1e-5f) { k.active = false; k.g = { 0, 0, 0 }; removeActiveAt (a); continue; }
                ++a;
                const int pc = note % 12, oct = note / 12 - 1;
                const double sc = octaveScale (oct);
                double ph8 = cnt[(size_t) pc] * sc; ph8 -= std::floor (ph8);
                double ph4 = ph8 * 2.0; ph4 -= std::floor (ph4);
                const float dt8 = (float) (topInc[(size_t) pc] * sc) * ratio, dt4 = 2.0f * dt8;
                const bool upper = note >= p.split;
                const float w8 = wave.eight ((float) ph8, std::min (0.5f, dt8)) * levelOf (kLevel8, note) * (1.0f / 2.9f);
                const float w4 = wave.four ((float) ph4, std::min (0.5f, dt4)) * levelOf (kLevel4, note) * (1.0f / 2.9f);
                if (upper ? p.strUp : p.strLo) busStr += w4 * k.g[0] * gainVel;
                const float gc = k.g[1] * gainVel;
                if (upper ? p.chM8Up : p.chM8Lo) busM8 += w8 * gc;
                if (! upper && p.chM4Lo) busM4 += w4 * gc;
                if (upper && p.chFemUp) busF4 += w4 * gc;
                if (upper ? p.vocUp : p.vocLo) busVoc += w8 * k.g[2] * gainVel;
            }
            // Acople AC de cada bus (las formas son unipolares).
            busStr = dc[0].process (busStr); busM8 = dc[1].process (busM8); busM4 = dc[2].process (busM4);
            busF4 = dc[3].process (busF4); busVoc = dc[4].process (busVoc);

            // ── Cuerdas: filtro de tono fijo, VCA, TONE ──
            strHp.process (busStr);
            float s = strHp.hp + 0.5f * strPeak.bandNorm (strHp.hp);
            s *= envStr;
            strTone.set (1200.0f * fastExp2 (v[kSmStrTone] * 3.3f), 0.6f, fs); strTone.process (s);
            s = strTone.lp * v[kSmStrLevel] * 0.4f;

            // ── Coro: banco de 7 formantes compartido por las tres voces, VCA ──
            float c = 0.0f;
            for (int f = 0; f < 7; ++f)
            {
                const float in = kChoirMix[0][f] * busM8 + kChoirMix[1][f] * busM4 + kChoirMix[2][f] * busF4;
                c += choir[(size_t) f].bandNorm (in);
            }
            c *= envCh * v[kSmChLevel] * 0.9f;

            // ── Vocoder ──
            const float m = mic != nullptr ? mic[i] * v[kSmMic] : 0.0f;
            micPeak = std::max (micPeak, std::fabs (m));
            float car = 0.0f;
            if (p.carrier != kCarrierExternal) car += busVoc;
            if (p.carrier != kCarrierKeys && carrier != nullptr) car += carrier[i] * v[kSmExt];
            // Detector de "sonido musical": hay portador.
            carrierEnv += (std::fabs (car) - carrierEnv) * aCarrier;
            carrierGate += ((carrierEnv > 2e-4f ? 1.0f : 0.0f) - carrierGate) * aCarrier;
            const float pe = m - 0.6f * preX; preX = m;                   // preénfasis
            float y = 0.0f;
            for (int b = 0; b < kBands; ++b)
            {
                auto& A = ana[(size_t) b];
                const float r = std::fabs (A.b.bandNorm (A.a.bandNorm (pe)));
                A.env += (r - A.env) * (r > A.env ? detA : detR);
                y += syn[(size_t) b].b.bandNorm (syn[(size_t) b].a.bandNorm (car)) * A.env;
            }
            deY = y + 0.6f * deY;                                          // desénfasis
            vocToneF.set (1500.0f * fastExp2 (v[kSmVocTone] * 3.3f), 0.6f, fs); vocToneF.process (deY * 45.0f);
            float hf = m;                                                  // consonantes: tres pasa altos en serie
            for (int h = 0; h < 3; ++h) { consHp[(size_t) h].process (hf); hf = consHp[(size_t) h].hp; }
            const float voc = vocToneF.lp * v[kSmVocLevel] + hf * carrierGate * v[kSmCons] * 0.8f + m * v[kSmDirect] * 0.5f;

            // ── Vibrato (BBD) en el coro y el vocoder, con su retardo de entrada ──
            if (any) vibFade = std::min (1.0f, vibFade + fadeInc);
            if (! resting) { vibPhase += vibInc; if (vibPhase >= 1.0f) vibPhase -= 1.0f; }
            const float depth = clampf (v[kSmVibDepth] + p.modVib * modWheel, 0.0f, 1.5f) * vibFade;
            const float vibMs = kVibBaseMs + kVibMaxMs * depth * fastSin (vibPhase);
            vibCh.push (c); const float cv = vibCh.read (vibMs);
            vibVoc.push (voc); const float vv = vibVoc.read (vibMs);

            // ── Ensemble: dos LFO compuestos por canal, cada canal con dos BBD en contrafase ──
            const float ensIn = (p.strEns ? s : 0.0f) + (p.chEns ? cv : 0.0f) + (p.vocEns ? vv : 0.0f);
            const float dry = (p.strEns ? 0.0f : s) + (p.chEns ? 0.0f : cv) + (p.vocEns ? 0.0f : vv);
            for (int ch = 0; ch < 2 && ! resting; ++ch)
            {
                ensPhase[(size_t) ch] += slowInc[ch]; if (ensPhase[(size_t) ch] >= 1.0f) ensPhase[(size_t) ch] -= 1.0f;
                ensPhase[(size_t) (2 + ch)] += fastInc[ch]; if (ensPhase[(size_t) (2 + ch)] >= 1.0f) ensPhase[(size_t) (2 + ch)] -= 1.0f;
            }
            const float d = v[kSmEns];
            float mod[2];
            for (int ch = 0; ch < 2; ++ch) mod[ch] = d * (kEnsSlowMs * tri (ensPhase[(size_t) ch]) + kEnsFastMs * fastSin (ensPhase[(size_t) (2 + ch)]));
            for (auto& e : ens) e.push (ensIn);
            const float l = 0.5f * (ens[0].read (kEnsBaseMs + mod[0]) + ens[1].read (kEnsBaseMs - mod[1]));
            const float r = 0.5f * (ens[2].read (kEnsBaseMs - mod[0]) + ens[3].read (kEnsBaseMs + mod[1]));
            // Acople AC de la salida (la saturación de los BBD con señales asimétricas deja un poco de continua).
            L[i] = dc[5].process (l + dry) * v[kSmVolume];
            R[i] = dc[6].process (r + dry) * v[kSmVolume];
            lastEnsMs = { kEnsBaseMs + mod[0], kEnsBaseMs + mod[1] };
            lastVibMs = vibMs;
            // Reposo: sin teclas y sin nada entrando a los BBD (el piso de ruido de las líneas no cuenta) durante más de lo que
            // tarda en vaciarse la línea más larga, el divisor y los LFO vuelven a su fase inicial. Así
            // lo que suena después de un silencio no depende de lo que sonó antes (un bounce da los mismos bits cada vez,
            // y guardar y abrir el proyecto suena igual); las líneas ya están en silencio, así que no se oye.
            if (nActive == 0 && std::fabs (s) + std::fabs (c) + std::fabs (voc) < 1e-7f) { if (idle < kIdleSamples && ++idle == kIdleSamples) rest(); }
            else idle = 0;
        }
        // Reverb (extra), sobre el estéreo.
        if (p.spaceMix > 0.0005f || reverbTail > 0)
        {
            reverb.process (L, R, n, 0.35f + 0.6f * p.spaceSize, 0.45f, p.spaceMix);
            reverbTail = p.spaceMix > 0.0005f ? (int) (fs * 12.0f) : std::max (0, reverbTail - n);
        }
    }

    // ── Para las pruebas y la vista ──
    bool anyDown() const { for (int a = 0; a < nActive; ++a) { const auto& k = keys[(size_t) active[(size_t) a]]; if (k.down || k.held) return true; } return false; }
    int activeKeys() const { return nActive; }
    double noteHz (int note) const
    {
        const int pc = note % 12, oct = note / 12 - 1;
        return topInc[(size_t) pc] * octaveScale (oct) * fs * std::exp2 (p.tuneCents / 1200.0);
    }
    float bandEnv (int b) const { return ana[(size_t) b].env; }
    float takeMicPeak() { const float x = micPeak; micPeak = 0.0f; return x; }
    std::array<float, 2> lastEnsMs {};
    float lastVibMs = 0.0f;
    float currentPitchOffset() const { return pitchOffset; }
    float sectionEnv (int s) const { return s == 0 ? envStr : envCh; }

    // Calibración del divisor: la A de arriba (A8, divisor 284) cae en 7040 Hz exactos, A4 = 440 Hz.
    void computeTopInc()
    {
        const double scale = 7040.0 * kTopDivisor[9] / kTopClock;
        for (int pc = 0; pc < 12; ++pc) topInc[(size_t) pc] = kTopClock / kTopDivisor[pc] * scale / (double) fs;
    }

    Engine() { computeTopInc(); }

private:
    enum Sm { kSmTune, kSmStrTone, kSmStrLevel, kSmChLevel, kSmMic, kSmExt, kSmVocTone, kSmVocLevel, kSmCons, kSmDirect,
              kSmVibDepth, kSmEns, kSmVolume, kSmPitchAmt, kNumSmooth };
    float target (int s) const
    {
        switch (s)
        {
            case kSmTune: return p.tuneCents;          case kSmStrTone: return p.strTone;   case kSmStrLevel: return p.strLevel;
            case kSmChLevel: return p.chLevel;         case kSmMic: return p.micGain;       case kSmExt: return p.extGain;
            case kSmVocTone: return p.vocTone;         case kSmVocLevel: return p.vocLevel; case kSmCons: return p.consonants;
            case kSmDirect: return p.directMic;        case kSmVibDepth: return p.vibDepth; case kSmEns: return p.ensDepth;
            case kSmVolume: return p.volume;           case kSmPitchAmt: return p.pitchAmount;
            default: return 0.0f;
        }
    }
    static inline double octaveScale (int oct) { return std::ldexp (1.0, oct - 8); }     // C8 = la octava del generador
    static inline float fastSin (float ph)   // -cos(2π·ph) por serie de Taylor sobre un triángulo (error < 2e-4)
    {
        const float x = ph < 0.5f ? ph * 4.0f - 1.0f : 3.0f - ph * 4.0f;   // triángulo -1..1, pico en 0,25
        const float t = x * 1.5707963f;                                  // seno de t con t en ±π/2
        const float t2 = t * t;
        return t * (1.0f - t2 * (0.16666667f - t2 * (0.0083333310f - t2 * 0.00019840874f)));
    }
    static inline float tri (float ph) { return ph < 0.5f ? 4.0f * ph - 1.0f : 3.0f - 4.0f * ph; }
    // Fases de reposo: el divisor con un desfase fijo por nota (no todas en fase: un acorde no arranca con todos los
    // flancos juntos) y los LFO al principio.
    void rest()
    {
        for (int pc = 0; pc < 12; ++pc) cnt[(size_t) pc] = (double) ((pc * 7919) % 1000) * 0.001 * 4096.0;
        vibPhase = 0.0f; ensPhase = { 0.0f, 0.0f, 0.0f, 0.0f };
        envStr = envCh = 0.0f; dump = 0;                    // las envolventes clásicas también, como recién preparado
    }
    void addActive (int note) { active[(size_t) nActive++] = (int16_t) note; }
    void removeActiveAt (int a) { active[(size_t) a] = active[(size_t) (nActive - 1)]; --nActive; }

    Params p;
    float fs = 48000.0f;
    std::array<double, 12> topInc {}, cnt {};
    std::array<Key, 128> keys {};
    std::array<int16_t, 128> active {}; int nActive = 0;
    Wave wave;
    std::array<Svf, 7> choir {};
    Svf strHp, strPeak, strTone, vocToneF;
    std::array<Svf, 6> consHp {};
    struct Band { Svf a, b; float env = 0.0f; };
    std::array<Band, kBands> ana {};
    struct SBand { Svf a, b; };
    std::array<SBand, kBands> syn {};
    std::array<DcBlocker, 7> dc {};
    std::array<Bbd, 4> ens {};
    Bbd vibCh, vibVoc;
    PlateReverb reverb; int reverbTail = 0;
    std::array<Smoother, kNumSmooth> sm {}; bool smoothReady = false;
    float preX = 0, deY = 0, carrierEnv = 0, carrierGate = 0, envStr = 0, envCh = 0, micPeak = 0;
    int dump = 0, idle = 0;
    static constexpr int kIdleSamples = 1024;   // ~21 ms a 48 kHz: más que la línea más larga (ensemble, ~8 ms)
    float vibPhase = 0, vibFade = 0; std::array<float, 4> ensPhase {};
    float pitchOffset = 0, bend = 0, modWheel = 0; bool sustain = false;
    float configuredShift = -100.0f;
};

} // namespace dh::vp
