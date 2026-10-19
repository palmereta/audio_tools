#pragma once
// Datos de una voz del ATFS1, con los mismos rangos que el FS1R (1998) para poder importar sus
// bulks de sysex: común + 8 operadores con voz + 8 sin voz (ver docs/hardware-spec.md §2-§6).
// La voz es la "partitura" del sonido; la parte (performance) agrega desvíos, controladores y efectos.

#include <array>
#include <cstdint>
#include <string>

namespace dh::fs {

constexpr int kOps = 8;

enum Form : int { Sine = 0, All1, All2, Odd1, Odd2, Res1, Res2, Frmt };

struct AmpEgData
{
    std::array<int, 4> level { 99, 99, 99, 0 };      // L1..L4, 0..99
    std::array<int, 4> time { 0, 20, 20, 30 };       // T1..T4, 0..99 (más alto = más lento)
    int hold = 0, timeScale = 0;                      // 0..99, 0..7
};

struct FreqEgData { int init = 0, attack = 0, attackTime = 0, decayTime = 0; };   // niveles −50..+50

struct VoicedOp
{
    int form = Sine;
    bool fixed = false;
    int coarse = 1, fine = 0;          // ratio: coarse 0..31, fine 0..99; fijo: coarse 0..21, fine 0..127
    int noteScale = 0;                 // seguimiento de teclado del modo fijo, 0..99
    int transpose = 0;                 // −24..+24 (sólo frmt)
    bool keySync = false;
    int detune = 0;                    // −15..+15
    int skirt = 0;                     // 0..7
    int bwRes = 0;                     // ancho de banda (frmt) o resonancia (res1/res2), 0..99
    int bwBias = 0;                    // −7..+7
    AmpEgData eg;
    FreqEgData feg;
    int level = 0;                     // 0..99
    int breakPoint = 39, leftDepth = 0, rightDepth = 0, leftCurve = 0, rightCurve = 0;
    int freqBias = 0, pitchMod = 0, freqMod = 0, freqVel = 0, ampMod = 0, ampVel = 0, egBias = 0;
    bool fseq = false;
    int fseqTrack = 0;                 // 0..7
};

struct UnvoicedOp
{
    int transpose = 0;                 // −24..+24
    int mode = 0;                      // 0 normal, 1 linkFO (sigue la fundamental), 2 linkFF (sigue al formante con voz)
    int coarse = 19, fine = 0;         // 0..21, 0..127: 440·2^(coarse + fine/128 − 16) Hz (19 = 3,5 kHz)
    int noteScale = 0;
    int bandwidth = 30, bwBias = 0;    // 0..99, −7..+7
    int resonance = 0, skirt = 0;      // 0..7, 0..7
    FreqEgData feg;
    int level = 0, levelKeyScale = 0;  // 0..99, −7..+7
    AmpEgData eg;
    int freqBias = 0, freqMod = 0, freqVel = 0, ampMod = 0, ampVel = 0, egBias = 0;
    bool fseq = false;
};

struct KnobSlot { int dest = 0, unvoiced = 0, op = 0, depth = 0; };   // dest: 0 off, 1 out, 2 freq, 3 width

struct VoiceData
{
    std::string name = "Init Voice";
    int category = 0;
    int algorithm = 0;                 // 0..87
    int feedback = 0;                  // 0..7
    int noteShift = 0;                 // −24..+24
    std::array<int, kOps> carrierAtt {};   // 0..15 (−1,5 dB por paso)
    // Envolvente de altura: L0..L4 −50..+50, T1..T4 0..99.
    std::array<int, 5> pegLevel {};
    std::array<int, 4> pegTime { 0, 0, 0, 0 };
    int pegVel = 0, pegRange = 1, pegTimeScale = 0;   // rango 0: 8 oct, 1: 2 oct, 2: 1 oct, 3: 1/2 oct
    // LFO1 (altura, amplitud, frecuencia de los operadores, filtro) y LFO2 (filtro).
    int lfo1Wave = 0, lfo1Speed = 35, lfo1Delay = 0, lfo1Pmd = 0, lfo1Amd = 0, lfo1Fmd = 0, lfo1FilterDepth = 0;
    bool lfo1KeySync = false;
    int lfo2Wave = 0, lfo2Speed = 40, lfo2Phase = 0, lfo2FilterDepth = 0;
    bool lfo2KeySync = false;
    // Filtro (en el FS1R lo enciende la parte).
    int filterType = 0;                // 0 LPF24, 1 LPF18, 2 LPF12, 3 HPF, 4 BPF, 5 BEF
    int cutoff = 127, resonance = 0;   // 0..127, −16..+100
    int resVel = 0, inputGain = 0;     // −7..+7, −12..+12 dB
    int keyScaleDepth = 0, keyScaleBreak = 60;
    std::array<int, 4> fegLevel {};    // L1..L4, −50..+50 (desvíos del corte)
    std::array<int, 4> fegTime { 0, 30, 30, 30 };
    int fegDepth = 0, fegDepthVel = 0, fegAttackVel = 0, fegTimeScale = 0;
    // Matrices de las perillas FORMANT y FM.
    std::array<KnobSlot, 5> formantKnob {}, fmKnob {};
    std::array<VoicedOp, kOps> op {};
    std::array<UnvoicedOp, kOps> nop {};

    VoiceData()
    {
        op[0].level = 99;              // un seno: el patch de partida de cualquier síntesis FM
    }
};

// Algoritmos: las 88 topologías de la hoja de algoritmos pública (una fila por algoritmo):
// conexiones de modulación (src → dst, de la 1 a la 8), portadoras y realimentación (src → dst).
struct AlgorithmDef
{
    std::array<uint8_t, kOps> modMask;   // modMask[d]: bit s = el operador s modula al d
    uint8_t carriers;                    // bit d = portadora
    int8_t fbSrc, fbDst;                 // −1 = sin realimentación
};

const AlgorithmDef& algorithm (int index);   // FsAlgorithms.h

} // namespace dh::fs
