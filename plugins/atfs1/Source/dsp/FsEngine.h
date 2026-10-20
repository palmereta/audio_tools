#pragma once
// Motor del ATFS1: hasta 32 notas de una voz del FS1R (8 operadores con voz enrutados por uno de los 88
// algoritmos + 8 operadores sin voz sumados directo), envolvente de altura, LFO1/LFO2, filtro con su EG,
// reproducción del Fseq y los desvíos de la parte (las perillas ATTACK, RELEASE, FORMANT y FM del frente).
//
// Control cada kCtl muestras (3 kHz a 48 kHz; el original corre la altura y los LFO a 192 Hz y los
// registros del chip a la frecuencia de muestreo): altura, envolventes, LFO y Fseq se recalculan por
// bloque y la amplitud de cada operador se interpola linealmente dentro del bloque.

#include "Common.h"
#include "FsData.h"
#include "FsAlgorithms.h"
#include "FsDsp.h"
#include "Fseq.h"
#include <array>
#include <cmath>
#include <vector>

namespace dh::fs {

constexpr int kMaxVoices = 32;
constexpr int kCtl = 16;

// Destinos de la rueda y del aftertouch (un subconjunto de la matriz de controladores del FS1R, §5.7).
enum CtlDest : int { CdOff = 0, CdLfo1Pm, CdLfo1Am, CdLfo1Fm, CdLfo1Filter, CdCutoff, CdFormant, CdFm, CdVnBalance,
                     CdFseqSpeed, CdScratch, CdFreqBias, CdEgBias, CdVoicedBw, CdCount };

// Valores de la parte (performance): los que expone el host. Desvíos −64..+63 salvo indicación.
struct PartParams
{
    float volume = 0.8f, pan = 0.0f;         // 0..1, −1..+1
    int vnBalance = 0;
    int attack = 0, decay = 0, release = 0, formant = 0, fm = 0;
    int cutoff = 0, resonance = 0, fegDepth = 0;
    int lfo1Rate = 0, lfo1Pmd = 0, lfo1Delay = 0, lfo2Rate = 0, lfo2Depth = 0;
    int pegInit = 0, pegAttack = 0, pegRelLevel = 0, pegRelTime = 0;
    bool filterOn = true, authenticPoly = false;   // auténtico: el filtro reduce la polifonía a 16
    bool mono = false, porta = false, portaFulltime = false;
    int portaTime = 0;                        // 0..127
    int bendUp = 2, bendDown = 2;             // semitonos
    int maxVoices = 16;
    int velDepth = 64, velOffset = 64;        // 0..127
    int noteShift = 0, detune = 0;            // semitonos, −64..+63 (décimos de cent ×10)
    int mwDest = CdLfo1Pm, mwDepth = 40, atDest = CdOff, atDepth = 0;
    // Fseq
    bool fseqOn = false, fseqSmooth = false, fseqScratch = false;
    float fseqSpeed = 1.0f;                   // 0,1..5 (100 % = 1)
    int fseqLoopMode = 0, fseqStart = 0, fseqLoopStart = 0, fseqLoopEnd = 0;   // loop end 0 = el del Fseq
    bool fseqLoopOverride = false;                                                 // el modo elegido en la parte (no el del Fseq)
    int fseqPitchMode = 0;                    // 0 la altura del Fseq, 1 la de la tecla
    int fseqSpeedVel = 0, fseqLevelVel = 0, fseqDelay = 0;
    int muteMode = 0;                         // 0 todo, 1 sólo con voz, 2 sólo sin voz (MUTE/SOLO)
};

// ───────── EG del filtro: 0 → L1 → L2 → L3 (sostiene) → L4 al soltar, niveles −50..+50 ─────────
struct FilterEg
{
    float val = 0.0f; int stage = 4; std::array<float, 4> lv {}; std::array<float, 4> rate {};
    void setup (const VoiceData& v, int note, float velocity, int attackOfs)
    {
        const int add = keyRateAdd (v.fegTimeScale, note);
        for (int i = 0; i < 4; ++i) lv[(size_t) i] = (float) v.fegLevel[(size_t) i];
        for (int i = 0; i < 4; ++i)
        {
            int t = v.fegTime[(size_t) i];
            if (i == 0) t = std::clamp (t - attackOfs - (int) std::lround ((float) v.fegAttackVel * 2.0f * (velocity - 0.5f) * 7.0f), 0, 99);
            rate[(size_t) i] = 100.0f / traverseSeconds (rateOf (t) + add);
        }
    }
    void keyOn() { val = 0.0f; stage = 0; }
    void keyOff() { stage = 3; }
    inline float process (float dt)
    {
        if (stage >= 4) return val;
        const float tg = lv[(size_t) stage];
        const float r = rate[(size_t) stage] * dt;
        if (std::fabs (tg - val) <= r) { val = tg; if (stage < 2) ++stage; else if (stage == 3) stage = 4; else stage = 2; }
        else val += tg > val ? r : -r;
        return val;
    }
};

// ───────── escalas por tecla y sensibilidades ─────────
// Escala de nivel al estilo DX7: distancia en grupos de 3 semitonos al punto de quiebre (valor + 21 = nota MIDI).
inline float levelScaleDb (const VoicedOp& o, int note)
{
    const int bp = o.breakPoint + 21;
    if (note == bp) return 0.0f;
    const bool left = note < bp;
    const float groups = (float) std::abs (note - bp) / 3.0f;
    const int depth = left ? o.leftDepth : o.rightDepth, curve = left ? o.leftCurve : o.rightCurve;
    if (depth <= 0) return 0.0f;
    const float d = (float) depth / 99.0f;
    const float mag = (curve == 0 || curve == 3) ? d * groups * 1.6f : d * 48.0f * (1.0f - fastExp (-groups / 9.0f));
    return std::min (24.0f, (curve >= 2 ? 1.0f : -1.0f) * mag);
}
// Velocidad −7..+7: 0 dB a velocidad máxima para las positivas, a velocidad mínima para las negativas.
inline float velDb (int sens, float vel) { return sens >= 0 ? (float) sens / 7.0f * 30.0f * (vel - 1.0f) : (float) -sens / 7.0f * 30.0f * -vel; }
inline float ampModScale (int sens) { static const float S[8] = { 0, 1, 2, 4, 5, 6, 7, 8 }; return S[sens & 7] / 8.0f; }
// En modo ratio Fine va de 0 a 99 (como muestra el campo); un valor mayor que quedó del modo fijo se limita.
inline float ratioOf (int coarse, int fine) { return (coarse <= 0 ? 0.5f : (float) coarse) * (1.0f + (float) std::min (fine, 99) / 100.0f); }
inline float fixedHz (int coarse, int fine) { return 440.0f * fastExp2 ((float) coarse + (float) fine / 128.0f - 16.0f); }
// Detune: un paso es una fracción de cent que se achica con la nota (la tabla del original disminuye arriba).
inline float detuneRatio (int detune, int note) { return fastExp2 ((float) detune * 0.6f * fastExp2 (-(float) (note - 60) / 36.0f) / 1200.0f); }

// ───────── una nota ─────────
struct Voice
{
    bool active = false, gate = false, sustained = false;
    int note = 60, voiceNote = 60; float velocity = 1.0f; uint32_t age = 0;
    float pitchSemis = 60.0f, glideFrom = 60.0f, glideRate = 0.0f;
    std::array<FormOperator, kOps> fo {};
    std::array<NoiseOperator, kOps> no {};
    std::array<AmpEg, kOps> egV {}, egN {};
    std::array<FreqEg, kOps> fegV {}, fegN {};
    std::array<float, kOps> ampV {}, ampN {};        // amplitud al comienzo del bloque
    std::array<float, kOps> stepV {}, stepN {};
    std::array<float, kOps> fseqLvV {}, fseqLvN {};  // nivel del Fseq suavizado (~1,6 ms, como el chip)
    PitchEg peg; FilterEg feg; Lfo lfo1, lfo2;
    VoiceFilter filter;
    FseqPlayer fseq; float fseqDelayLeft = 0.0f; float fseqSpeedVelK = 1.0f;
    float fb0 = 0.0f, fb1 = 0.0f;
    // Parámetros calculados por bloque.
    std::array<float, kOps> grainHz {}, carrierHz {}, winSec {};
    std::array<int, kOps> pIdx {};
    std::array<float, kOps> noiseHz {};
    std::array<int, kOps> noiseBw {};
    float cutoffHz = 20000.0f, gainV = 1.0f, gainN = 1.0f, panL = 0.707f, panR = 0.707f;
    int filterRes = 0;
    uint8_t quietV = 0, quietN = 0;          // operadores callados en este bloque

    void seed (int idx)
    {
        for (int k = 0; k < kOps; ++k) no[(size_t) k].rng = Rng (0xA11CEu + 7919u * (uint32_t) (idx * kOps + k + 1));
        lfo1.rng = Rng (0x1F51u + 104729u * (uint32_t) (idx + 1));
        lfo2.rng = Rng (0x2F52u + 15485863u * (uint32_t) (idx + 1));
    }
};

class Engine
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (int i = 0; i < kMaxVoices; ++i) voices[(size_t) i].seed (i);
        mixL.assign (8192, 0.0f); mixR.assign (8192, 0.0f);
        reset();
    }
    void reset()
    {
        for (auto& v : voices) { v.active = false; v.gate = false; v.filter.reset(); }
        heldCount = 0; sustain = false;
    }

    // ── MIDI ──
    // Si se pasa de poly a mono (o al revés) con teclas apretadas, las voces del modo anterior se sueltan
    // (en mono el note-off sólo mira la voz 0 y quedaban voces colgadas).
    void checkMode (const PartParams& pp)
    {
        if (pp.mono == lastMono) return;
        lastMono = pp.mono;
        for (auto& vc : voices) if (vc.active && vc.gate) release (vc);
        heldCount = 0;
    }

    void noteOn (int note, int vel, const VoiceData& vd, const PartParams& pp)
    {
        checkMode (pp);
        if (vel <= 0) { noteOff (note, pp); return; }
        pushHeld (note);
        const float v = velocityCurve (vel, pp);
        if (pp.mono)
        {
            Voice& vc = voices[0];
            const bool legato = vc.active && vc.gate && heldCount > 1;
            for (int i = 1; i < kMaxVoices; ++i) voices[(size_t) i].active = false;
            startVoice (vc, note, legato ? vc.velocity : v, vd, pp, legato);
            return;
        }
        startVoice (allocate (note, pp), note, v, vd, pp, false);
    }
    void noteOff (int note, const PartParams& pp)
    {
        checkMode (pp);
        popHeld (note);
        if (pp.mono)
        {
            Voice& vc = voices[0];
            if (vc.active && vc.note == note)
            {
                if (heldCount > 0) { glideTo (vc, held[(size_t) heldCount - 1], pp, true); vc.note = held[(size_t) heldCount - 1]; }
                else if (sustain) vc.sustained = true;
                else release (vc);
            }
            return;
        }
        for (auto& vc : voices)
            if (vc.active && vc.gate && vc.note == note) { if (sustain) vc.sustained = true; else release (vc); }
    }
    void setSustain (bool on)
    {
        sustain = on;
        if (! on) for (auto& vc : voices) if (vc.active && vc.sustained) { vc.sustained = false; release (vc); }
    }
    // También suelta el pedal: si el sustain se levantó durante un bypass (o el host mandó sólo All Notes Off), las
    // notas siguientes quedaban sostenidas hasta volver a pisarlo.
    void allNotesOff() { sustain = false; for (auto& vc : voices) { vc.sustained = false; if (vc.active) release (vc); } heldCount = 0; }
    void resetControllers() { bend = 0.0f; modWheel = 0.0f; aftertouch = 0.0f; setSustain (false); }     // CC121
    void panic() { reset(); }
    void setBend (float b) { bend = b; }            // −1..+1
    void setModWheel (float m) { modWheel = m; }    // 0..1
    void setAftertouch (float a) { aftertouch = a; }

    int activeVoices() const { int n = 0; for (auto& v : voices) n += v.active ? 1 : 0; return n; }
    float fseqPosition() const { for (auto& v : voices) if (v.active && v.fseq.active) return v.fseq.pos; return -1.0f; }
    // Nivel por operador de la última nota (para los LED del editor), 0..1.
    std::array<float, 16> opMeter {};

    // ── audio ──
    void process (float* L, float* R, int n, const VoiceData& vd, const FseqData* fsq, const PartParams& pp)
    {
        std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
        const AlgorithmDef& alg = algorithm (std::clamp (vd.algorithm, 0, 87));
        prepareAlgorithm (alg, vd);
        const bool useFseq = pp.fseqOn && fsq != nullptr && ! fsq->empty();
        std::array<float, 16> meter {};
        for (auto& vc : voices)
        {
            if (! vc.active) continue;
            for (int s0 = 0; s0 < n; s0 += kCtl)
            {
                const int m = std::min (kCtl, n - s0);
                control (vc, vd, useFseq ? fsq : nullptr, pp, (float) m / fs);
                render (vc, alg, vd, pp, L + s0, R + s0, m);
                if (! vc.active) break;
            }
            if (vc.active) for (int k = 0; k < kOps; ++k) { meter[(size_t) k] = std::max (meter[(size_t) k], vc.ampV[(size_t) k]); meter[(size_t) k + 8] = std::max (meter[(size_t) k + 8], vc.ampN[(size_t) k]); }
        }
        opMeter = meter;
        const float vol = pp.volume * pp.volume;
        for (int i = 0; i < n; ++i) { L[i] *= vol; R[i] *= vol; }
    }

private:
    float fs = 48000.0f;
    std::array<Voice, kMaxVoices> voices {};
    std::array<int, 16> held {}; int heldCount = 0;
    bool sustain = false;
    bool lastMono = false;
    float bend = 0.0f, modWheel = 0.0f, aftertouch = 0.0f;
    uint32_t ageCounter = 0;
    std::vector<float> mixL, mixR;
    // Algoritmo preparado: lista de moduladores por destino, portadoras y operadores en uso.
    std::array<std::array<int8_t, kOps>, kOps> mods {};
    std::array<int, kOps> modCount {};
    uint8_t carriers = 0, usedV = 0, usedN = 0;
    int fbSrc = -1, fbDst = -1;
    float fbGain = 0.0f;

    void pushHeld (int note) { popHeld (note); if (heldCount < 16) held[(size_t) heldCount++] = note; }
    void popHeld (int note)
    {
        for (int i = 0; i < heldCount; ++i)
            if (held[(size_t) i] == note) { for (int j = i; j < heldCount - 1; ++j) held[(size_t) j] = held[(size_t) j + 1]; --heldCount; return; }
    }
    static float velocityCurve (int vel, const PartParams& pp)
    {
        const float v = (float) vel * (float) pp.velDepth / 64.0f + (float) (pp.velOffset - 64);
        return clampf (v, 1.0f, 127.0f) / 127.0f;
    }

    Voice& allocate (int note, const PartParams& pp)
    {
        const int limit = std::clamp (pp.filterOn && pp.authenticPoly ? std::min (pp.maxVoices, 16) : pp.maxVoices, 1, kMaxVoices);
        for (int i = 0; i < limit; ++i) { auto& v = voices[(size_t) i]; if (v.active && v.note == note) return v; }   // misma tecla: se reusa
        for (int i = 0; i < limit; ++i) if (! voices[(size_t) i].active) return voices[(size_t) i];
        Voice* best = nullptr;
        for (int i = 0; i < limit; ++i) { auto& v = voices[(size_t) i]; if (! v.gate && (best == nullptr || v.age < best->age)) best = &v; }
        if (best == nullptr) for (int i = 0; i < limit; ++i) { auto& v = voices[(size_t) i]; if (best == nullptr || v.age < best->age) best = &v; }
        for (int i = limit; i < kMaxVoices; ++i) voices[(size_t) i].active = false;
        return *best;
    }

    void glideTo (Voice& vc, int note, const PartParams& pp, bool legato)
    {
        const float target = (float) note;
        if (pp.porta && pp.portaTime > 0 && (legato || pp.portaFulltime))
        {
            vc.glideFrom = vc.pitchSemis;
            const float secs = 0.0015f * fastExp ((float) pp.portaTime * 0.058f);   // 1,5 ms .. 2,4 s por octava
            vc.glideRate = 12.0f / secs;
        }
        else { vc.pitchSemis = target; vc.glideRate = 0.0f; }
        vc.voiceNote = note;
    }

    void startVoice (Voice& vc, int note, float vel, const VoiceData& vd, const PartParams& pp, bool legato)
    {
        const bool wasActive = vc.active;
        if (! wasActive) vc.pitchSemis = (float) note;
        glideTo (vc, note, pp, legato || (pp.porta && pp.portaFulltime && lastNote >= 0));
        if (! wasActive && pp.porta && pp.portaFulltime && lastNote >= 0) vc.pitchSemis = (float) lastNote;
        lastNote = note;
        vc.note = note; vc.velocity = vel; vc.gate = true; vc.sustained = false; vc.age = ++ageCounter;
        const int attackOfs = (int) std::lround ((float) pp.attack * 0.78f);
        const int decayOfs = (int) std::lround ((float) pp.decay * 0.78f);
        const int releaseOfs = (int) std::lround ((float) pp.release * 0.78f);
        for (int k = 0; k < kOps; ++k)
        {
            const auto& o = vd.op[(size_t) k];
            // Una voz reutilizada (o después de un pánico) puede tener envolventes de moduladores a mitad del
            // release: sin esto el operador saltaba a ese nivel y la nota perdía el ataque.
            if (! wasActive) { vc.egV[(size_t) k].reset(); vc.egN[(size_t) k].reset(); }
            vc.egV[(size_t) k].setup (o.eg, note, -attackOfs, -decayOfs, -releaseOfs, 0.0f);
            vc.egV[(size_t) k].keyOn (legato);
            if (! legato) { vc.fegV[(size_t) k].keyOn (o.feg); vc.fo[(size_t) k].reset (o.keySync && o.form != Frmt); }
            const auto& u = vd.nop[(size_t) k];
            vc.egN[(size_t) k].setup (u.eg, note, -attackOfs, -decayOfs, -releaseOfs, 0.0f);
            vc.egN[(size_t) k].keyOn (legato);
            if (! legato) vc.fegN[(size_t) k].keyOn (u.feg);
            if (! wasActive) { vc.ampV[(size_t) k] = vc.ampN[(size_t) k] = 0.0f; vc.fseqLvV[(size_t) k] = vc.fseqLvN[(size_t) k] = 0.0f; }
        }
        if (! legato)
        {
            VoiceData pv = vd;   // desvíos de la parte sobre la envolvente de altura (L0, L4, T1, T4)
            pv.pegLevel[0] = std::clamp (vd.pegLevel[0] + pp.pegInit * 50 / 64, -50, 50);
            pv.pegLevel[4] = std::clamp (vd.pegLevel[4] + pp.pegRelLevel * 50 / 64, -50, 50);
            pv.pegTime[0] = std::clamp (vd.pegTime[0] + pp.pegAttack * 99 / 127, 0, 99);
            pv.pegTime[3] = std::clamp (vd.pegTime[3] + pp.pegRelTime * 99 / 127, 0, 99);
            vc.peg.setup (pv, vel); vc.peg.keyOn();
            vc.feg.setup (vd, note, vel, attackOfs); vc.feg.keyOn();
            if (vd.lfo1KeySync) vc.lfo1.keySync();
            vc.lfo1.startDelay (std::clamp (vd.lfo1Delay + pp.lfo1Delay * 99 / 127, 0, 99));
            if (vd.lfo2KeySync) vc.lfo2.keySync ((float) vd.lfo2Phase * 0.25f);
            vc.fseq.start (pp.fseqStart);
            vc.fseq.active = false;
            vc.fseqDelayLeft = (float) pp.fseqDelay * 0.008f * 2.5f;
            vc.fseqSpeedVelK = 1.0f - (float) pp.fseqSpeedVel / 7.0f * (1.0f - vel) * 0.9f;
            if (! wasActive) { vc.filter.reset(); vc.fb0 = vc.fb1 = 0.0f; }
        }
        vc.active = true;
    }
    int lastNote = -1;

    void release (Voice& vc)
    {
        vc.gate = false;
        for (int k = 0; k < kOps; ++k) { vc.egV[(size_t) k].keyOff(); vc.egN[(size_t) k].keyOff(); }
        vc.peg.keyOff(); vc.feg.keyOff(); vc.fseq.release();
    }

    void prepareAlgorithm (const AlgorithmDef& alg, const VoiceData& vd)
    {
        carriers = alg.carriers;
        for (int d = 0; d < kOps; ++d)
        {
            modCount[(size_t) d] = 0;
            for (int s = 0; s < kOps; ++s) if (alg.modMask[(size_t) d] & (1 << s)) mods[(size_t) d][(size_t) modCount[(size_t) d]++] = (int8_t) s;
        }
        fbSrc = alg.fbSrc; fbDst = alg.fbDst;
        fbGain = vd.feedback <= 0 || fbSrc < 0 ? 0.0f : 0.5f * fastExp2 ((float) (vd.feedback - 7));
        // Operadores en uso: los que tienen nivel (o Fseq), y sus moduladores.
        usedV = 0; usedN = 0;
        for (int k = 0; k < kOps; ++k)
        {
            if (vd.op[(size_t) k].level > 0 || vd.op[(size_t) k].fseq) usedV |= (uint8_t) (1 << k);
            if (vd.nop[(size_t) k].level > 0 || vd.nop[(size_t) k].fseq) usedN |= (uint8_t) (1 << k);
        }
    }

    float ctlAmount (int dest, const PartParams& pp) const
    {
        float a = 0.0f;
        if (pp.mwDest == dest) a += modWheel * (float) pp.mwDepth / 63.0f;
        if (pp.atDest == dest) a += aftertouch * (float) pp.atDepth / 63.0f;
        return a;
    }

    // Una vuelta de control: altura, envolventes, LFO, Fseq, frecuencias y amplitudes objetivo del bloque.
    void control (Voice& vc, const VoiceData& vd, const FseqData* fsq, const PartParams& pp, float dt)
    {
        const float invN = 1.0f / (dt * fs);
        // Portamento.
        if (vc.glideRate > 0.0f)
        {
            const float tg = (float) vc.voiceNote, st = vc.glideRate * dt;
            if (std::fabs (tg - vc.pitchSemis) <= st) { vc.pitchSemis = tg; vc.glideRate = 0.0f; }
            else vc.pitchSemis += tg > vc.pitchSemis ? st : -st;
        }
        // LFO.
        const float l1hz = lfoHz (std::clamp (vd.lfo1Speed + pp.lfo1Rate * 99 / 127, 0, 99));
        const float lfo1 = vc.lfo1.process (l1hz, vd.lfo1Wave, dt);
        const float lfo2 = vc.lfo2.process (lfoHz (std::clamp (vd.lfo2Speed + pp.lfo2Rate, 0, 127), 127), vd.lfo2Wave, dt);
        const float pmd = clampf (((float) vd.lfo1Pmd + (float) pp.lfo1Pmd * 99.0f / 127.0f) / 99.0f + ctlAmount (CdLfo1Pm, pp), 0.0f, 1.0f);
        const float amd = clampf ((float) vd.lfo1Amd / 99.0f + ctlAmount (CdLfo1Am, pp), 0.0f, 1.0f);
        const float fmd = clampf ((float) vd.lfo1Fmd / 99.0f + ctlAmount (CdLfo1Fm, pp), 0.0f, 1.0f);
        const float pegSemis = vc.peg.process (dt);
        const float bendSemis = bend >= 0.0f ? bend * (float) pp.bendUp : bend * (float) pp.bendDown;
        const float detuneSemis = (float) pp.detune * 0.0078f;   // ±50 cents
        float baseSemis = vc.pitchSemis + (float) (vd.noteShift + pp.noteShift) + pegSemis + bendSemis + detuneSemis;

        // Fseq.
        FseqFrame frame; bool fseqActive = false;
        if (fsq != nullptr)
        {
            if (vc.fseqDelayLeft > 0.0f) vc.fseqDelayLeft -= dt;
            else if (! vc.fseq.active && vc.gate && vc.fseq.pos <= (float) pp.fseqStart + 0.001f) vc.fseq.active = true;
            const float speed = clampf (pp.fseqSpeed * vc.fseqSpeedVelK * (1.0f + ctlAmount (CdFseqSpeed, pp)), 0.02f, 10.0f);     // hasta 1000 %
            if (pp.fseqScratch || pp.mwDest == CdScratch || pp.atDest == CdScratch)
            {
                const float c = clampf (ctlAmount (CdScratch, pp), 0.0f, 1.0f);
                vc.fseq.pos = c * (float) (fsq->frames.size() - 1);
                vc.fseq.active = true;
            }
            else
            {
                const int nf = (int) fsq->frames.size();
                if (pp.fseqLoopOverride && pp.fseqLoopEnd > pp.fseqLoopStart) vc.fseq.advance (*fsq, std::clamp (pp.fseqLoopStart, 0, nf - 1), std::clamp (pp.fseqLoopEnd, 0, nf - 1), pp.fseqLoopMode, dt, speed);
                else vc.fseq.advance (*fsq, fsq->loopStart, fsq->loopEnd, pp.fseqLoopOverride ? pp.fseqLoopMode : fsq->loopMode, dt, speed);
            }
            frame = vc.fseq.frame (*fsq, pp.fseqSmooth);
            fseqActive = true;
            if (pp.fseqPitchMode == 0)
            {
                const float fseqSemis = 69.0f + 12.0f * fastLog2 (std::max (20.0f, frame.pitch) / 440.0f);
                baseSemis += fseqSemis - (float) fsq->noteAssign;
            }
        }
        const int keyNote = vc.note;
        const float noteHz = 440.0f * fastExp2 ((baseSemis - 69.0f) / 12.0f);
        const float knobFormant = clampf ((float) pp.formant / 64.0f + ctlAmount (CdFormant, pp), -1.0f, 1.0f);
        const float knobFm = clampf ((float) pp.fm / 64.0f + ctlAmount (CdFm, pp), -1.0f, 1.0f);
        std::array<float, kOps> knobOutDbV {}, knobFreqV {}, knobBwV {}, knobOutDbN {}, knobFreqN {}, knobBwN {};
        auto applyKnob = [&] (const std::array<KnobSlot, 5>& slots, float k) {
            if (std::fabs (k) < 1.0e-4f) return;
            for (const auto& s : slots)
            {
                if (s.dest <= 0 || s.depth == 0) continue;
                const float e = clampf (k * (float) s.depth / 32.0f, -2.0f, 2.0f);   // +63: 100 % a media perilla
                const size_t o = (size_t) std::clamp (s.op, 0, 7);
                if (s.dest == 1) (s.unvoiced ? knobOutDbN : knobOutDbV)[o] += e * 24.0f;
                else if (s.dest == 2) (s.unvoiced ? knobFreqN : knobFreqV)[o] += e * 12.0f;
                else (s.unvoiced ? knobBwN : knobBwV)[o] += e * 24.0f;
            }
        };
        applyKnob (vd.formantKnob, knobFormant);
        applyKnob (vd.fmKnob, knobFm);
        const float freqBias = ctlAmount (CdFreqBias, pp), egBias = ctlAmount (CdEgBias, pp), bwBias = ctlAmount (CdVoicedBw, pp);

        for (int k = 0; k < kOps; ++k)
        {
            const size_t K = (size_t) k;
            const auto& o = vd.op[K];
            const float fegS = vc.fegV[K].process (dt);
            const float egDb = vc.egV[K].process (dt);
            float hz, grain = noteHz;
            const float pmSemis = lfo1 * pmd * pmd * 12.0f * (float) o.pitchMod / 7.0f;
            const bool fseqOp = fseqActive && o.fseq;
            if (fseqOp)
            {
                hz = frame.vFreq[(size_t) (o.fseqTrack & 7)] * fastExp2 (knobFreqV[K] / 12.0f);
                grain = noteHz * fastExp2 ((pmSemis + (float) o.transpose) / 12.0f);
            }
            else if (o.fixed)
            {
                hz = fixedHz (o.coarse, o.fine) * fastExp2 (((float) (keyNote - 60) * (float) o.noteScale / 99.0f
                                                             + knobFreqV[K]
                                                             + lfo1 * fmd * 12.0f * (float) o.freqMod / 7.0f
                                                             + (float) o.freqVel / 7.0f * 12.0f * (vc.velocity - 1.0f) * 0.5f
                                                             + freqBias * (float) o.freqBias / 7.0f * 12.0f) / 12.0f);
                grain = noteHz * fastExp2 (((o.form == Frmt ? pmSemis : 0.0f) + (float) o.transpose) / 12.0f);
            }
            else
            {
                hz = noteHz * ratioOf (o.coarse, o.fine) * detuneRatio (o.detune, keyNote)
                     * fastExp2 ((fegS + pmSemis + knobFreqV[K]) / 12.0f);
                grain = noteHz * fastExp2 ((pmSemis + (float) o.transpose) / 12.0f);
            }
            vc.carrierHz[K] = std::min (hz, 0.49f * fs);
            vc.grainHz[K] = std::max (8.0f, grain);
            const float bw = (float) o.bwRes + bwBias * (float) o.bwBias * 4.0f + knobBwV[K];
            vc.winSec[K] = 1.0f / (3.1f * fastExp2 (clampf (bw, 0.0f, 99.0f) / 8.0f));
            vc.pIdx[K] = Tables::pIndex (o.form == Frmt, o.skirt);
            vc.fo[K].setup (o.form, vc.grainHz[K], vc.carrierHz[K], vc.winSec[K], vc.pIdx[K], o.bwRes, fs);
            // Amplitud.
            float db = egDb + knobOutDbV[K] + egBias * (float) o.egBias / 7.0f * 24.0f;
            if (fseqOp)
            {
                const float tgt = fseqLevelGain (frame.vLevel[(size_t) (o.fseqTrack & 7)]);
                vc.fseqLvV[K] += (tgt - vc.fseqLvV[K]) * (1.0f - fastExp (-dt / 0.0016f));
                db += (float) pp.fseqLevelVel / 64.0f * 24.0f * (vc.velocity - 1.0f);
            }
            else db += levelDb (o.level) + levelScaleDb (o, keyNote) + velDb (o.ampVel, vc.velocity);
            db -= amd * ampModScale (o.ampMod) * 24.0f * 0.5f * (lfo1 + 1.0f);
            if (carriers & (1 << k)) db -= 1.5f * (float) vd.carrierAtt[K];
            float a = ((usedV >> k) & 1) ? dbGain (std::min (db, 24.0f)) : 0.0f;     // tope: datos importados fuera de rango
            if (fseqOp) a *= vc.fseqLvV[K];
            // Un operador callado todo el bloque (al principio y al final, menos de −90 dB) no se calcula.
            if (a < 3.0e-5f && vc.ampV[K] < 3.0e-5f) { vc.ampV[K] = a; vc.stepV[K] = 0.0f; vc.quietV |= (uint8_t) (1 << k); }
            else { vc.quietV &= (uint8_t) ~(1 << k); vc.stepV[K] = (a - vc.ampV[K]) * invN; }

            // Sin voz.
            const auto& u = vd.nop[K];
            const float fegN = vc.fegN[K].process (dt);
            const float egNDb = vc.egN[K].process (dt);
            const bool fseqN = fseqActive && u.fseq;
            float nhz;
            if (fseqN) nhz = frame.nFreq[(size_t) (o.fseqTrack & 7)];
            else if (u.mode == 1) nhz = noteHz;
            else if (u.mode == 2 && o.form == Frmt) nhz = vc.carrierHz[K];
            else nhz = fixedHz (u.coarse, u.fine) * fastExp2 (((float) (keyNote - 60) * (float) u.noteScale / 99.0f + fegN
                                                               + lfo1 * fmd * 12.0f * (float) u.freqMod / 7.0f
                                                               + freqBias * (float) u.freqBias / 7.0f * 12.0f) / 12.0f);
            nhz *= fastExp2 (((float) u.transpose + knobFreqN[K]) / 12.0f);
            vc.noiseHz[K] = std::min (nhz, 0.45f * fs);
            vc.noiseBw[K] = std::clamp ((int) ((float) u.bandwidth + bwBias * (float) u.bwBias * 4.0f + knobBwN[K] + 0.5f), 0, 99);
            vc.no[K].setup (vc.noiseHz[K], vc.noiseBw[K], u.skirt, u.resonance, fs);
            float ndb = egNDb + knobOutDbN[K] + egBias * (float) u.egBias / 7.0f * 24.0f;
            if (fseqN)
            {
                const float tgt = fseqLevelGain (frame.nLevel[(size_t) (o.fseqTrack & 7)]);
                vc.fseqLvN[K] += (tgt - vc.fseqLvN[K]) * (1.0f - fastExp (-dt / 0.0016f));
                ndb += (float) pp.fseqLevelVel / 64.0f * 24.0f * (vc.velocity - 1.0f);
            }
            else ndb += levelDb (u.level) + (float) u.levelKeyScale * 1.5f * (float) (keyNote - 60) / 12.0f + velDb (u.ampVel, vc.velocity);
            ndb -= amd * ampModScale (u.ampMod) * 24.0f * 0.5f * (lfo1 + 1.0f);
            float an = ((usedN >> k) & 1) ? dbGain (ndb) : 0.0f;
            if (fseqN) an *= vc.fseqLvN[K];
            if (an < 3.0e-5f && vc.ampN[K] < 3.0e-5f) { vc.ampN[K] = an; vc.stepN[K] = 0.0f; vc.quietN |= (uint8_t) (1 << k); }
            else { vc.quietN &= (uint8_t) ~(1 << k); vc.stepN[K] = (an - vc.ampN[K]) * invN; }
        }
        // Balance con voz / sin voz.
        const float bal = clampf ((float) pp.vnBalance / 64.0f + ctlAmount (CdVnBalance, pp), -1.0f, 1.0f);
        vc.gainV = bal > 0.0f ? 1.0f - bal : 1.0f;
        vc.gainN = bal < 0.0f ? 1.0f + bal : 1.0f;
        if (pp.muteMode == 1) vc.gainN = 0.0f; else if (pp.muteMode == 2) vc.gainV = 0.0f;
        // Filtro.
        const float fegV = vc.feg.process (dt);
        const float fegDepth = clampf (((float) vd.fegDepth + (float) pp.fegDepth) / 64.0f, -1.0f, 1.0f)
                               * (1.0f + (float) vd.fegDepthVel / 7.0f * (vc.velocity - 1.0f));
        const float l1f = clampf ((float) vd.lfo1FilterDepth / 99.0f + ctlAmount (CdLfo1Filter, pp), 0.0f, 1.0f);
        const float l2f = clampf (((float) vd.lfo2FilterDepth + (float) pp.lfo2Depth * 99.0f / 127.0f) / 99.0f, 0.0f, 1.0f);
        const float cutSemis = (float) vd.cutoff + (float) pp.cutoff + fegV / 50.0f * fegDepth * 96.0f
                               + (float) vd.keyScaleDepth / 64.0f * (float) (keyNote - vd.keyScaleBreak)
                               + lfo1 * l1f * 48.0f + lfo2 * l2f * 48.0f + ctlAmount (CdCutoff, pp) * 64.0f;
        vc.cutoffHz = 17.4f * fastExp2 (clampf (cutSemis, 0.0f, 127.0f) / 12.0f);
        vc.filterRes = std::clamp (vd.resonance + pp.resonance + (int) std::floor ((float) vd.resVel / 7.0f * 30.0f * (vc.velocity - 0.5f) + 0.5f), -16, 100);
        const float p = clampf (pp.pan, -1.0f, 1.0f);
        vc.panL = std::sqrt (0.5f * (1.0f - p)); vc.panR = std::sqrt (0.5f * (1.0f + p));
        // Fin de la nota: todas las envolventes en uso apagadas.
        if (! vc.gate)
        {
            bool alive = false;
            for (int k = 0; k < kOps && ! alive; ++k)
                alive = (((carriers & usedV) >> k) & 1 && ! vc.egV[(size_t) k].idle()) || (((usedN >> k) & 1) && ! vc.egN[(size_t) k].idle());
            if (! alive) vc.active = false;
        }
    }
    void render (Voice& vc, const AlgorithmDef&, const VoiceData& vd, const PartParams& pp, float* L, float* R, int n)
    {
        std::array<float, kOps> out {};
        const float fsl = fs;
        float* buf = mixL.data();
        const uint8_t liveV = (uint8_t) (usedV & ~vc.quietV), liveN = (uint8_t) (usedN & ~vc.quietN);
        for (int i = 0; i < n; ++i)
        {
            float carrier = 0.0f;
            for (int d = 0; d < kOps; ++d)
            {
                const size_t D = (size_t) d;
                if (! ((liveV >> d) & 1)) { out[D] = 0.0f; continue; }
                float m = 0.0f;
                for (int j = 0; j < modCount[D]; ++j) m += out[(size_t) mods[D][(size_t) j]];
                m *= kFmIndex;
                if (d == fbDst) m += fbGain * 0.5f * (vc.fb0 + vc.fb1);
                const float y = vc.fo[D].tick (m);
                const float a = vc.ampV[D];
                vc.ampV[D] = a + vc.stepV[D];
                out[D] = y * a;
                if ((carriers >> d) & 1) carrier += out[D];
            }
            if (fbSrc >= 0) { vc.fb1 = vc.fb0; vc.fb0 = out[(size_t) fbSrc]; }
            float noise = 0.0f;
            if (liveN)
                for (int k = 0; k < kOps; ++k)
                {
                    if (! ((liveN >> k) & 1)) continue;
                    const size_t K = (size_t) k;
                    noise += vc.no[K].tick() * vc.ampN[K];
                    vc.ampN[K] += vc.stepN[K];
                }
            // Acumulador del canal: satura como el del chip.
            float x = carrier * vc.gainV + noise * vc.gainN;
            x = 2.0f * fastTanh (0.5f * x);
            buf[i] = x;
        }
        if (pp.filterOn)
        {
            const float gIn = dbGain ((float) vd.inputGain), gOut = 1.0f / std::max (1.0f, gIn);
            vc.filter.setup (vd.filterType, vc.cutoffHz, vc.filterRes, fsl);
            for (int i = 0; i < n; ++i)
            {
                float x = buf[i] * gIn;
                if (vd.inputGain > 0) x = fastTanh (x);
                buf[i] = vc.filter.tick (x) * gOut;
            }
            vc.filter.sanitize();
        }
        for (int i = 0; i < n; ++i) { L[i] += buf[i] * vc.panL; R[i] += buf[i] * vc.panR; }
    }
};

} // namespace dh::fs
