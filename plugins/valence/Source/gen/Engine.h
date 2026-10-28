#pragma once
// Motor de Valence: un arpegiador MIDI de cuatro capas sobre el acorde que se toca. Sin JUCE: lo usan el plugin,
// valencerender y valencetest.
//
// Cada capa es un electrón que orbita el acorde: su figura, su largo en pasos (1-32), su patrón, sus octavas, su
// gate, su swing y su canal. Con largos distintos las capas entran y salen de fase y vuelven a coincidir en el paso
// 0 cada mínimo común múltiplo de sus períodos (la "fusión", que el motor predice). Cada paso tiene sus filas:
// encendido, velocidad y acento, ligadura, ratchet, probabilidad, octava, nota fija y un valor de CC.
//
// El acorde son los protones (las notas que se tocan, con latch o pedal) y los neutrones (las que agrega el
// plugin al completarlo). Sin entrada, una caminata de acordes como la de Orrery hace de manos.
//
// Tiempo: ticks enteros (kTPB por negra) sobre una grilla absoluta, convertidos a muestras desde un ancla que sólo
// se mueve con el tempo o un salto del transporte. La misma entrada y la misma variación dan la misma salida con
// cualquier tamaño de bloque (las cinco reglas de la tabla "Determinism" del README de Orrery).

#include "../../../../shared/gen/Theory.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace dh::val {

using namespace dh::theory;   // escalas, acordes, dm:: y Rng viven en shared/gen/Theory.h

constexpr int kLayers = 4;
constexpr int kMaxSteps = 32;
constexpr int kMaxPool = 16;                  // notas del acorde (protones + neutrones)
// Ticks por negra: divisible por todo lo que se usa (figuras con tresillo y puntillo hasta 1/32, ratchets de 2, 3
// y 4, swing en porcentaje entero), así cada paso cae en un tick entero.
constexpr int64_t kTPB = 100800;
constexpr int64_t kClockTicks = kTPB / 24;    // reloj MIDI: 24 por negra
constexpr float kMaxHumanizeMs = 30.0f;

enum Pattern : int { PatUp = 0, PatDown, PatUpDown, PatUpDownRep, PatConverge, PatDiverge, PatPlayed, PatRandom, PatWalk, PatChord, PatPedal, PatFinger, PatternCount };
inline const char* patternName (int p)
{
    static const char* N[PatternCount] = { "Up", "Down", "Up-down", "Up-down (repeat ends)", "Converge", "Diverge", "As played", "Random", "Random walk", "Chord", "Pedal", "Finger order" };
    return N[std::clamp (p, 0, PatternCount - 1)];
}

constexpr int kFigures = 12;
inline const char* figureName (int f)
{
    static const char* N[kFigures] = { "1/4", "1/4T", "1/4D", "1/8", "1/8T", "1/8D", "1/16", "1/16T", "1/16D", "1/32", "1/32T", "1/32D" };
    return N[std::clamp (f, 0, kFigures - 1)];
}
inline int64_t figureTicks (int f)
{
    f = std::clamp (f, 0, kFigures - 1);
    const int64_t base = kTPB >> (f / 3);                          // 1/4, 1/8, 1/16, 1/32
    switch (f % 3) { case 1: return base * 2 / 3; case 2: return base * 3 / 2; default: return base; }
}

// Octavas: Stack recorre el patrón sobre todas las octavas (sube una por vuelta); Alternate cambia de octava nota por nota
// (ida y vuelta: 0, 1, 2, 1, 0...); Random sortea la octava de cada nota.
enum OctMode : int { OctStack = 0, OctAlternate, OctRandom };
enum VelMode : int { VelFixed = 0, VelInput, VelStep };
enum Complete : int { CompOff = 0, CompTriad, CompSeventh, CompNinth, CompOctaves };
enum Order : int { OrderPlayed = 0, OrderLowHigh, OrderHighLow, OrderRandom };
enum Source : int { SrcInput = 0, SrcAuto };
enum Nucleon : int { NucNone = -1, NucProton = 0, NucNeutron = 1 };
enum OutFlags : uint8_t { FlagAccent = 1, FlagTie = 2, FlagRatchet = 4, FlagFixed = 8 };

// Un paso de una capa: sus filas.
struct Step
{
    bool on = true;
    uint8_t vel = 100;        // con la velocidad "por paso"
    bool accent = false, tie = false;
    uint8_t ratchet = 1;      // 1-4 notas en el paso
    uint8_t prob = 100;       // probabilidad, %
    int8_t oct = 0;           // octava, -2..+2
    int8_t fixed = -1;        // nota fija: grado de la escala (0-13) sobre la tónica; -1 = la del patrón
    int8_t cc = -1;           // valor de CC del paso (0-127); -1 = no manda
    int8_t finger = 0;        // índice del acorde para el patrón "orden de dedos"
    bool operator== (const Step&) const = default;
};

struct LayerParams
{
    bool on = false;
    int channel = 1;
    int figure = 6, mult = 1;                 // 1/16 × 1
    int length = 16;
    int pattern = PatUp;
    int octaves = 1, octMode = OctStack, octave = 0, shift = 0;
    float gate = 0.5f;                        // fracción del paso
    int swing = 50;                           // 50-75 %
    int velMode = VelFixed, velocity = 100;
    float humanizeMs = 0.0f;
    int euPulses = 0, euRotate = 0;           // 0 pulsos = las filas de encendido
    bool restart = true;                      // un acorde nuevo vuelve la capa al paso 0
    float mutate = 0.0f;
    int ccNum = -1;
    std::array<Step, kMaxSteps> steps {};
    uint32_t rowsVersion = 0;                 // cambia cuando cambian las filas (el motor rehace su copia)
    LayerParams() { for (int k = 0; k < kMaxSteps; ++k) steps[(size_t) k].finger = (int8_t) (k % 8); }
   #if defined (__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wfloat-equal"
   #endif
    bool operator== (const LayerParams&) const = default;
   #if defined (__clang__)
    #pragma clang diagnostic pop
   #endif
};

struct GlobalParams
{
    bool run = false, hostClock = true;
    float tempo = 120.0f;
    int key = 9, scale = 1;                   // La menor natural
    uint32_t seed = 1;
    int phraseBars = 0;                       // 0 = polimetría libre
    int source = SrcInput, complete = CompOff, order = OrderLowHigh;
    bool latch = false;
    int transpose = 0;
    int chordBars = 2, progression = 0;       // armonía automática: compases por acorde; 0 caminata, 1 dos acordes, 2 drone
    int accent = 24;                          // lo que suma un acento a la velocidad
    bool perLayerChannels = false;            // false: todas las capas al canal 1
    // Play y Stop como en Orrery: sin cmdMode cuentan los cambios de run; con cmdMode, runCmd 1 = Play, 2 = Stop.
    bool cmdMode = false; int runCmd = 0;
};

struct Transport { bool playing = false; double ppq = 0.0, bpm = 0.0; };     // bpm 0: el host no lo dice (el tempo propio)
struct InEvent { int offset; uint8_t data[3]; uint8_t size; };

// Lo que sale. layer -1: pánico, reenvío de la entrada. nucleon y src: de qué nota del acorde salió (la vista).
struct OutEvent
{
    int offset; uint8_t data[3]; uint8_t size;
    int8_t layer = -1, nucleon = NucNone, src = -1, step = -1; uint8_t flags = 0;
};

// Acorde compartido (Link: la líder publica, las seguidoras tocan sobre él).
struct ChordShare
{
    uint32_t serial = 0, chordSerial = 0;
    int key = 0, scale = 0, count = 0;
    std::array<uint8_t, kMaxPool> notes {}, vels {};
};

class Engine
{
public:
    std::array<LayerParams, kLayers> layers {};
    GlobalParams g;

    // Lo que ve la interfaz (el átomo).
    struct View
    {
        bool running = false;
        double beat = 0.0, bpm = 120.0;
        std::array<int, kLayers> step {}, length {};
        std::array<float, kLayers> phase {};          // fase dentro del ciclo de la capa (0..1)
        int protons = 0, neutrons = 0;
        std::array<uint8_t, kMaxPool> protonNotes {}, neutronNotes {};
        double fusionBeats = -1.0;                    // pulsos hasta la próxima fusión (todas en el paso 0); -1 = nunca
        int64_t fusionTick = -1;                      // el tick de esa fusión (pulso × kTPB, exacto)
        std::array<int64_t, kLayers> fusionSteps {};  // pasos de cada capa hasta la fusión
        uint32_t chordSerial = 0;
        char chord[16] {};
    } view;

    // Lo último que sonó, para capturar como archivo MIDI (anillo).
    struct Captured { double sec; uint8_t ch, note, vel; int8_t layer; float durSec; };
    std::vector<Captured> capture = std::vector<Captured> (8192);
    std::atomic<int> captureHead { 0 };
    double captureBpm = 120.0;

    // Reloj MIDI de salida del bloque (0xF8, 0xFA, 0xFC) en su muestra; lo manda el plugin a los puertos directos.
    struct ClockEvent { int offset; uint8_t byte; };
    std::vector<ClockEvent> clock;

    // Cada cambio del acorde en su muestra (para valencerender; el plugin no lo prende: pide memoria).
    struct ChordChange { int64_t at; std::vector<int> protons, neutrons; };
    bool logChords = false;
    std::vector<ChordChange> chordLog;

    // Vínculo.
    bool following = false;
    ChordShare external;

    void prepare (double sampleRate)
    {
        fs = sampleRate > 0.0 ? sampleRate : 48000.0;
        pending.reserve (8192); out.reserve (8192); clock.reserve (512); flushOffs.reserve (2048);
        for (auto* v : { &protonList, &base, &played, &scratch }) v->reserve (64);
        tieKeep.reserve (kMaxPool * 2);
        for (auto& S : st) S.ties.reserve (kMaxPool * 2);
        if (session) setBpm (bpm);
    }

    // Como recién creado (reset() del host, prepareToPlay, otro preset): lo que suena sale con note-off en el
    // próximo bloque; la entrada retenida, el latch y la sesión se olvidan.
    void reset()
    {
        for (int ch = 0; ch < 16; ++ch)
            for (int n = 0; n < 128; ++n)
                if (activeId[(size_t) ch][(size_t) n] != 0) { flushOffs.push_back ({ (uint8_t) ch, (uint8_t) n, activeLayer[(size_t) ch][(size_t) n] }); activeId[(size_t) ch][(size_t) n] = 0; }
        pending.clear();
        held = {}; pedal = false; seqCounter = 0; base.clear(); played.clear(); protonList.clear();
        session = false; armed = false; prevRun = g.run; prevHost = false; hostEdge = 0; autoWasOn = false; latchWas = false; followWas = false;
        abs = 0; cursor = 0; chordSerial = 0; seenExternal = 0; clockStartPending = false;
        lastComplete = lastOrder = lastKey = lastScale = lastSource = -1;
        for (auto& S : st) { std::vector<TieNote> v; v.swap (S.ties); v.clear(); S = LayerState {}; S.ties.swap (v); }
        view = View {};
    }

    void process (int n, const Transport& tr, const InEvent* in, int nin, std::vector<OutEvent>& outEvents)
    {
        out.clear(); clock.clear();
        blockStart = abs; blockN = std::max (1, n);
        cursor = abs;
        for (auto& f : flushOffs) push (abs, 0x80 | f.ch, f.note, 0, f.layer);
        flushOffs.clear();
        if (n <= 0) { outEvents = out; return; }

        // Play / Stop.
        if (g.cmdMode) { if (g.runCmd == 1) armed = true; else if (g.runCmd == 2) armed = false; }
        else if (g.run != prevRun) armed = g.run;
        prevRun = g.run;

        // Capas que se apagan o cambian de canal: sus notas se sueltan ya.
        for (int l = 0; l < kLayers; ++l)
        {
            auto& S = st[(size_t) l]; const auto& L = layers[(size_t) l];
            const int ch = channelOf (l);
            if (S.wasOn && (! L.on || ch != S.lastCh)) { releaseLayer (l, abs); if (! L.on) S.wasOn = false; }
            S.lastCh = ch;
        }

        // Transporte.
        hostEdge = 0;
        const bool hostPlaying = g.hostClock && tr.playing;
        const double freeBpm = std::clamp ((double) g.tempo, 20.0, 400.0);
        const double hostBpm = tr.bpm > 0.0 ? std::clamp (tr.bpm, 20.0, 400.0) : freeBpm;
        if (g.hostClock && tr.playing != prevHost)
        {
            hostEdge = tr.playing ? 1 : -1;
            armed = tr.playing;             // con el reloj del host, arrancar el transporte es Play y pararlo es Stop
            if (session) { endSession (abs); allNotesOffAt (abs); }
            if (tr.playing) startHostSession (tr.ppq, hostBpm);
        }
        else if (hostPlaying)
        {
            if (! session) startHostSession (tr.ppq, hostBpm);
            else
            {
                const double hostTick = tr.ppq * (double) kTPB, predicted = tickAt (abs);
                if (std::abs (hostTick - predicted) > (double) kTPB / 64.0)
                {
                    // Salto (lazo del host, relocalización): se suelta todo y se empieza de nuevo desde esa posición,
                    // así cada pasada de un lazo suena igual.
                    endSession (abs); allNotesOffAt (abs);
                    startHostSession (tr.ppq, hostBpm);
                }
                else if (std::abs (hostBpm - bpm) > 1.0e-9) { aT = predicted; aS = abs; setBpm (hostBpm); }
                else if (std::abs (hostTick - predicted) * spt > 0.5) { aT = hostTick; aS = abs; }   // deriva de más de media muestra
            }
        }
        prevHost = tr.playing;
        if (! hostPlaying)
        {
            const double b = g.hostClock ? hostBpm : freeBpm;
            if (session && std::abs (b - bpm) > 1.0e-9) { aT = tickAt (abs); aS = abs; setBpm (b); }
            if (! session) bpm = b;
        }
        if (resetRequest.exchange (false))
        {
            // Otro preset: lo que suena se suelta y la música vuelve a empezar (con el acorde que se esté tocando).
            if (session) endSession (abs);
            allNotesOffAt (abs);
            if (hostPlaying) startHostSession (tr.ppq, hostBpm);
            else if (autoActive()) startFreeSession (abs);
            rebuildChord (abs, ! base.empty());
        }

        // Mientras el host no llamaba se perdieron los note-off de la entrada: las teclas y el pedal se sueltan (lo
        // retenido con latch sigue).
        if (inputLost)
        {
            inputLost = false;
            for (auto& h : held) { h.down = false; h.sustained = false; }
            pedal = false;
            rebuildChord (abs, false);
        }
        // La entrada que define el acorde: la líder (Link), el latch o la armonía automática pueden cambiarlo sin
        // notas de entrada.
        if (! g.latch && latchWas) { for (auto& h : held) h.latched = false; rebuildChord (abs, false); }
        latchWas = g.latch;
        if (following != followWas) { followWas = following; seenExternal = external.chordSerial; rebuildChord (abs, following); }
        else if (following && external.chordSerial != seenExternal) { seenExternal = external.chordSerial; rebuildChord (abs, true); }
        const bool wantAuto = autoActive();
        if (wantAuto != autoWasOn)
        {
            autoWasOn = wantAuto;
            if (wantAuto && ! session && ! hostPlaying) startFreeSession (abs);         // arranca la armonía adentro
            else if (wantAuto && session) { startAuto ((int64_t) std::ceil (tickAt (abs) - 1.0e-9)); rebuildChord (abs, true); }
            else rebuildChord (abs, false);
        }
        else if (configChanged()) rebuildChord (abs, false);

        // Capas que se encienden en marcha: entran en la grilla, en el paso que les toca por posición.
        if (session)
            for (int l = 0; l < kLayers; ++l)
            {
                auto& S = st[(size_t) l]; const auto& L = layers[(size_t) l];
                if (! L.on) continue;
                if (! S.wasOn) { positionLayer (l, std::max<int64_t> (startTick, (int64_t) std::ceil (tickAt (abs) - 1.0e-9))); continue; }
                const int64_t want = stepTicks (L);
                if (want != S.st) { S.grid = ceilDiv ((int64_t) std::ceil (tickAt (abs) - 1.0e-9), want); S.st = want; }
            }

        // Entrada, cambios de acorde y pasos, en orden de tiempo (a igual muestra: entrada, acorde, capas 1-4).
        const int64_t end = abs + n;
        int ii = 0;
        for (int guard = 0; guard < 200000; ++guard)
        {
            int64_t t = INT64_MAX; int who = -3;
            if (ii < nin) { t = abs + std::clamp (in[ii].offset, 0, n - 1); who = -2; }
            if (session)
            {
                if (autoActive()) { const int64_t cs = sampleOf (nextChordTick); if (cs < t) { t = cs; who = -1; } }
                for (int l = 0; l < kLayers; ++l)
                {
                    if (! layers[(size_t) l].on || ! st[(size_t) l].wasOn) continue;
                    const int64_t gs = genSample (l);
                    if (gs < t) { t = gs; who = l; }
                }
            }
            if (t >= end || who == -3) break;
            t = std::max (t, cursor);
            emitPending (t);
            cursor = t;
            if (who == -2) applyInput (in[ii++], t);
            else if (who == -1) autoChordChange (t);
            else generateStep (who, t);
        }
        emitPending (end);
        if (session) clockUpTo (end);
        std::stable_sort (out.begin(), out.end(), [] (const OutEvent& a, const OutEvent& b) { return a.offset < b.offset; });
        abs = end;
        updateView();
        captureBpm = bpm;
        outEvents = out;
    }

    // Todo apagado, también en el sinte: note-off de lo que suena, CC 123 y 120 en los 16 canales. La entrada
    // retenida (latch, pedal) se suelta.
    void panic (std::vector<OutEvent>& outEvents)
    {
        out.clear(); blockStart = abs; blockN = 1;
        for (auto& f : flushOffs) push (abs, 0x80 | f.ch, f.note, 0, f.layer);
        flushOffs.clear();
        allNotesOffAt (abs);
        for (int ch = 0; ch < 16; ++ch) { push (abs, 0xB0 | ch, 123, 0, -1); push (abs, 0xB0 | ch, 120, 0, -1); }
        held = {}; pedal = false; if (session) endSession (abs); rebuildChord (abs, false);
        outEvents = out;
    }
    // El host dejó de llamar (HostGap, bypass): note-off de lo que suena al principio del bloque que viene.
    void releaseAll()
    {
        for (int ch = 0; ch < 16; ++ch)
            for (int n = 0; n < 128; ++n)
                if (activeId[(size_t) ch][(size_t) n] != 0) { flushOffs.push_back ({ (uint8_t) ch, (uint8_t) n, activeLayer[(size_t) ch][(size_t) n] }); activeId[(size_t) ch][(size_t) n] = 0; }
        pending.clear();
        for (auto& S : st) S.ties.clear();
        inputLost = true;
    }

    // Bypass: note-off ya de lo que suena (sin CC 123 ni soltar el latch); las teclas apretadas se olvidan, porque sus
    // note-off no van a llegar mientras el plugin esté en bypass.
    void releaseNow (std::vector<OutEvent>& outEvents)
    {
        out.clear(); blockStart = abs; blockN = 1;
        for (auto& f : flushOffs) push (abs, 0x80 | f.ch, f.note, 0, f.layer);
        flushOffs.clear();
        allNotesOffAt (abs);
        inputLost = true;
        outEvents = out;
    }

    std::atomic<bool> resetRequest { false };      // otro preset: lo que suena se suelta y la sesión vuelve a empezar
    bool isRunning() const { return session; }
    bool isArmed() const { return armed; }
    int lastHostEdge() const { return hostEdge; }
    int64_t samplePosition() const { return abs; }
    double bpmNow() const { return bpm; }

    ChordShare published() const
    {
        ChordShare c; c.serial = ++pubSerial; c.chordSerial = chordSerial; c.key = g.key; c.scale = g.scale;
        c.count = std::min ((int) protonList.size(), kMaxPool);
        for (int i = 0; i < c.count; ++i) { c.notes[(size_t) i] = (uint8_t) protonList[(size_t) i].note; c.vels[(size_t) i] = protonList[(size_t) i].vel; }
        return c;
    }

    // Dónde empieza la música cuando el transporte arranca o salta a p (pulsos): en p llevado a la grilla de 1/16
    // (lo que cae a menos de 1/256 de pulso de una línea es esa línea) y nunca antes de 0 (un pre-roll espera).
    static double musicalStart (double p)
    {
        if (p <= 0.0) return 0.0;
        const double q = 0.0625, k = std::round (p / q);
        if (std::abs (p - k * q) < 1.0 / 256.0) return k * q;
        return std::ceil (p / q) * q;
    }

    static int64_t stepTicks (const LayerParams& L) { return figureTicks (L.figure) * std::clamp (L.mult, 1, 16); }
    static bool euclidHit (int k, int len, int pulses, int rotate)
    {
        len = std::max (1, len); pulses = std::clamp (pulses, 0, len);
        const int idx = ((k + rotate) % len + len) % len;
        return (idx * pulses) % len < pulses;
    }
    // Índice del patrón p en la posición c de un ciclo sobre m notas (sin los sorteos: Random y Random walk aparte).
    static int patternIndex (int p, int64_t c, int m, int finger)
    {
        if (m <= 1) return 0;
        c = std::max<int64_t> (0, c);
        switch (p)
        {
            case PatDown: return m - 1 - (int) (c % m);
            case PatUpDown: { const int cyc = 2 * m - 2; const int k = (int) (c % cyc); return k < m ? k : cyc - k; }
            case PatUpDownRep: { const int cyc = 2 * m; const int k = (int) (c % cyc); return k < m ? k : cyc - 1 - k; }
            case PatConverge: { const int k = (int) (c % m); return k % 2 == 0 ? k / 2 : m - 1 - k / 2; }
            case PatDiverge: { const int k = m - 1 - (int) (c % m); return k % 2 == 0 ? k / 2 : m - 1 - k / 2; }
            case PatPedal: { const int cyc = 2 * (m - 1); const int k = (int) (c % cyc); return k % 2 == 0 ? 0 : 1 + k / 2; }
            case PatFinger: return ((finger % m) + m) % m;
            default: return (int) (c % m);
        }
    }
    static int patternCycle (int p, int m)
    {
        if (m <= 1) return 1;
        switch (p) { case PatUpDown: return 2 * m - 2; case PatUpDownRep: return 2 * m; case PatPedal: return 2 * (m - 1); default: return m; }
    }
    static int pingPong (int64_t x, int r)
    {
        if (r <= 1) return 0;
        const int cyc = 2 * r - 2; const int k = (int) (x % cyc);
        return k < r ? k : cyc - k;
    }

private:
    // ───────── estado ─────────
    struct PoolNote { int note; int8_t kind; int8_t src; uint8_t vel; uint32_t seq; };
    struct Held { bool down = false, latched = false, sustained = false; uint8_t vel = 100; uint32_t seq = 0; };
    struct TieNote { uint8_t ch, note; uint32_t id; };
    struct Pick { int note; int8_t kind, src; uint8_t vel; bool used; };
    struct LayerState
    {
        bool wasOn = false, restartPending = false;
        int64_t st = kTPB / 4, grid = 0, cycleTick = 0, nextPhrase = INT64_MAX;
        int counter = 0, walk = 0, cycles = 0, lastCh = -1, lastCc = -1, curStep = 0;
        int64_t pat = 0, curOn = 0, curDur = 1;
        Rng rng { 1 }, hrng { 1 }, mrng { 1 };     // notas / humanizado / mutación (cada uno su secuencia)
        std::array<Step, kMaxSteps> work {};
        uint32_t rowsVersion = 0xFFFFFFFFu;
        std::vector<TieNote> ties;
    };
    struct Pending { int64_t at; uint8_t prio; uint8_t type, ch, d1, d2; int8_t layer, nucleon, src, step; uint8_t flags; uint32_t id, seq; int64_t end; };
    struct Flush { uint8_t ch, note; int8_t layer; };

    double fs = 48000.0;
    int64_t abs = 0, blockStart = 0, cursor = 0; int blockN = 1;
    // Sesión y ancla de tiempo.
    bool session = false, armed = false, prevRun = false, prevHost = false, autoWasOn = false, latchWas = false, followWas = false;
    bool clockStartPending = false, inputLost = false; int64_t clockStartAt = 0;
    int hostEdge = 0;
    int64_t aS = 0, startTick = 0, nextClock = 0; double aT = 0.0, bpm = 120.0, spt = 1.0;
    // Entrada y acorde.
    std::array<Held, 128> held {};
    bool pedal = false; uint32_t seqCounter = 0, chordSerial = 0, seenExternal = 0;
    mutable uint32_t pubSerial = 0;
    std::vector<PoolNote> protonList, base, played, scratch;
    int lastComplete = -1, lastOrder = -1, lastTranspose = 0, lastKey = -1, lastScale = -1, lastSource = -1; uint32_t lastSeed = 0;
    // Armonía automática.
    Rng arng { 1 }; int degree = 0; int64_t nextChordTick = 0; std::vector<int> autoTriad, autoExtra; Chord autoChord;
    std::array<LayerState, kLayers> st {};
    std::vector<Pending> pending; uint32_t pendSeq = 0; bool pendDirty = false;
    std::vector<OutEvent> out;
    std::vector<Flush> flushOffs;
    std::vector<TieNote> tieKeep;
    // Notas sonando por canal: id de la última nota-on (0 = apagada), capa, muestra de ataque y de fin previsto.
    std::array<std::array<uint32_t, 128>, 16> activeId {};
    std::array<std::array<int8_t, 128>, 16> activeLayer {};
    std::array<std::array<int64_t, 128>, 16> activeStart {}, activeEnd {};
    uint32_t nextId = 1;

    static int64_t floorDiv (int64_t a, int64_t b) { int64_t q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) --q; return q; }
    static int64_t ceilDiv (int64_t a, int64_t b) { return -floorDiv (-a, b); }
    const Scale& scale() const { return scales()[(size_t) std::clamp (g.scale, 0, (int) scales().size() - 1)]; }
    int channelOf (int l) const { return g.perLayerChannels ? std::clamp (layers[(size_t) l].channel, 1, 16) - 1 : 0; }
    bool autoActive() const { return g.source == SrcAuto && ! following && armed; }

    // ───────── tiempo ─────────
    void setBpm (double b) { bpm = std::clamp (b, 20.0, 400.0); spt = 60.0 * fs / (bpm * (double) kTPB); }
    int64_t sampleOf (int64_t tick) const { return aS + (int64_t) std::floor (((double) tick - aT) * spt + 0.5 + 1.0e-9); }
    double tickAt (int64_t sample) const { return aT + (double) (sample - aS) / spt; }
    int64_t onsetTick (int l, int64_t grid) const
    {
        const auto& S = st[(size_t) l]; const auto& L = layers[(size_t) l];
        int64_t t = grid * S.st;
        const int sw = std::clamp (L.swing, 50, 75);
        if ((grid & 1) != 0 && sw > 50) t += S.st * (2 * sw - 100) / 100;      // swing en las corcheas (o semicorcheas) impares de la grilla
        return t;
    }
    // Humanizado: de 0 a +ms de atraso por paso, reproducible. Sólo hacia atrás: cada paso lee el acorde en su propia
    // muestra de la grilla (adelantarlo obligaría a elegir sus notas antes de que se toque un acorde que cae en el pulso).
    int humanizeSamples (int l) const
    {
        const float ms = std::clamp (layers[(size_t) l].humanizeMs, 0.0f, kMaxHumanizeMs);
        return ms > 0.0f ? (int) std::lround ((double) ms * 0.001 * fs) : 0;
    }
    int64_t genSample (int l) const { return sampleOf (onsetTick (l, st[(size_t) l].grid)); }
    int64_t phraseTicks() const { return (int64_t) std::clamp (g.phraseBars, 1, 64) * 4 * kTPB; }

    void startHostSession (double ppq, double b)
    {
        setBpm (b);
        startSession (abs, ppq * (double) kTPB, (int64_t) std::llround (musicalStart (ppq) * (double) kTPB));
    }
    void startFreeSession (int64_t at)
    {
        if (! g.hostClock) setBpm (std::clamp ((double) g.tempo, 20.0, 400.0)); else setBpm (bpm);
        startSession (at, 0.0, 0);
    }
    void startSession (int64_t at, double anchorTick, int64_t fromTick)
    {
        session = true; aS = at; aT = anchorTick; startTick = fromTick;
        for (int l = 0; l < kLayers; ++l)
        {
            auto& S = st[(size_t) l];
            S.rng.reseed (g.seed * 7919u + (uint32_t) l * 104729u + 17u);
            S.hrng.reseed (g.seed * 6271u + (uint32_t) l * 15485863u + 91u);
            S.mrng.reseed (g.seed * 92821u + (uint32_t) l * 31337u + 5u);
            S.rowsVersion = 0xFFFFFFFFu;
            S.wasOn = false;
            if (layers[(size_t) l].on) positionLayer (l, fromTick);
        }
        nextClock = ceilDiv (fromTick, kClockTicks) * kClockTicks;
        clockStartPending = true; clockStartAt = std::max (at, sampleOf (fromTick));    // un pre-roll espera al comienzo
        autoWasOn = autoActive();
        if (autoWasOn) { startAuto (fromTick); rebuildChord (at, true); }
    }
    void endSession (int64_t at)
    {
        clockUpTo (at);
        if (! clockStartPending) clockEvent (at, 0xFC);
        clockStartPending = false;
        for (int l = 0; l < kLayers; ++l) closeTies (l, at, 0);
        session = false;
    }
    // La capa entra en la grilla en el primer paso desde fromTick, con el contador que le toca por posición (o por
    // la frase): la misma posición del transporte da el mismo paso.
    void positionLayer (int l, int64_t fromTick)
    {
        auto& S = st[(size_t) l]; const auto& L = layers[(size_t) l];
        const int len = std::clamp (L.length, 1, kMaxSteps);
        S.st = stepTicks (L);
        S.grid = ceilDiv (fromTick, S.st);
        int64_t first = 0;
        if (g.phraseBars > 0)
        {
            const int64_t ph = phraseTicks(), p0 = floorDiv (S.grid * S.st, ph) * ph;
            first = ceilDiv (p0, S.st);
            S.nextPhrase = p0 + ph;
        }
        else S.nextPhrase = INT64_MAX;
        S.counter = (int) (((S.grid - first) % len + len) % len);
        S.pat = S.counter; S.walk = 0; S.cycles = S.counter == 0 ? 0 : 1;
        S.cycleTick = S.grid * S.st - (int64_t) S.counter * S.st;
        S.restartPending = false; S.ties.clear(); S.lastCc = -1;
        S.curStep = S.counter; S.curOn = S.grid * S.st; S.curDur = S.st;
        S.wasOn = true;
        syncRows (l, true);
    }
    void syncRows (int l, bool force = false)
    {
        auto& S = st[(size_t) l]; const auto& L = layers[(size_t) l];
        if (force || S.rowsVersion != L.rowsVersion) { S.work = L.steps; S.rowsVersion = L.rowsVersion; }
    }

    // ───────── reloj MIDI ─────────
    void clockEvent (int64_t at, uint8_t b) { clock.push_back ({ (int) std::clamp<int64_t> (at - blockStart, 0, blockN - 1), b }); }
    void clockUpTo (int64_t until)
    {
        if (clockStartPending)
        {
            if (clockStartAt >= until) return;
            clockEvent (clockStartAt, 0xFA); clockStartPending = false;
        }
        for (int guard = 0; guard < 4096; ++guard)
        {
            const int64_t s = sampleOf (nextClock);
            if (s >= until) break;
            if (s >= blockStart) clockEvent (s, 0xF8);
            nextClock += kClockTicks;
        }
    }

    // ───────── entrada ─────────
    void applyInput (const InEvent& e, int64_t t)
    {
        const int status = e.data[0] & 0xF0;
        const int d1 = e.data[1] & 0x7F, d2 = e.data[2] & 0x7F;
        if (status == 0x90 && d2 > 0)
        {
            bool anyDown = false; for (auto& h : held) anyDown = anyDown || h.down;
            bool newChord = protonCount() == 0;
            if (g.latch && ! anyDown && ! pedal)       // latch: una mano nueva reemplaza al acorde retenido
            {
                for (auto& h : held) h.latched = false;
                newChord = true;
            }
            auto& h = held[(size_t) d1];
            h.down = true; h.vel = (uint8_t) d2; h.seq = ++seqCounter; h.latched = g.latch; h.sustained = false;
            if (! following) rebuildChord (t, newChord);
            return;
        }
        if (status == 0x80 || status == 0x90)
        {
            auto& h = held[(size_t) d1];
            if (! h.down) return;
            h.down = false;
            if (pedal) h.sustained = true;
            if (! following) rebuildChord (t, false);
            return;
        }
        if (status == 0xB0 && d1 == 64)
        {
            const bool down = d2 >= 64;
            if (down == pedal) return;
            pedal = down;
            if (! down) { for (auto& h : held) h.sustained = false; if (! following) rebuildChord (t, false); }
            return;
        }
        if (status == 0xB0 && (d1 == 123 || d1 == 120))
        {
            held = {}; pedal = false;
            allNotesOffAt (t);
            rebuildChord (t, false);
            return;
        }
        // Lo demás (CC, pitch bend, aftertouch) pasa tal cual.
        push (t, e.data[0], d1, d2, -1, std::clamp<int> (e.size, 1, 3));
    }
    int protonCount() const { return (int) protonList.size(); }

    bool configChanged() const
    {
        return g.complete != lastComplete || g.order != lastOrder || g.transpose != lastTranspose || g.key != lastKey || g.scale != lastScale
            || g.source != lastSource || g.seed != lastSeed;
    }

    // El acorde: protones (lo que se toca, la líder o la armonía automática) y neutrones (lo que agrega Completar).
    void rebuildChord (int64_t t, bool newChord)
    {
        lastComplete = g.complete; lastOrder = g.order; lastTranspose = g.transpose; lastKey = g.key; lastScale = g.scale; lastSource = g.source; lastSeed = g.seed;
        const bool wasEmpty = base.empty();
        protonList.clear();
        const bool isAuto = autoActive();
        if (following)
            for (int i = 0; i < std::min (external.count, kMaxPool); ++i) protonList.push_back ({ external.notes[(size_t) i], NucProton, (int8_t) external.notes[(size_t) i], external.vels[(size_t) i], (uint32_t) i });
        else if (isAuto)
            for (size_t i = 0; i < autoTriad.size(); ++i) protonList.push_back ({ autoTriad[i], NucProton, (int8_t) autoTriad[i], 100, (uint32_t) i });
        else if (g.source == SrcInput)
            for (int n = 0; n < 128; ++n)
            {
                const auto& h = held[(size_t) n];
                if (! (h.down || h.latched || h.sustained)) continue;
                const int note = std::clamp (n + g.transpose, 0, 127);
                bool dup = false; for (auto& p : protonList) dup = dup || p.note == note;
                if (! dup && (int) protonList.size() < kMaxPool) protonList.push_back ({ note, NucProton, (int8_t) note, h.vel, h.seq });
            }
        std::stable_sort (protonList.begin(), protonList.end(), [] (const PoolNote& a, const PoolNote& b) { return a.seq < b.seq; });

        // Neutrones.
        auto& neutrons = scratch; neutrons.clear();
        auto has = [&] (int note) { for (auto& p : protonList) if (p.note == note) return true; for (auto& p : neutrons) if (p.note == note) return true; return false; };
        auto hasPc = [&] (int pc) { for (auto& p : protonList) if (p.note % 12 == pc) return true; for (auto& p : neutrons) if (p.note % 12 == pc) return true; return false; };
        const int np = (int) protonList.size();
        if (isAuto && ! autoExtra.empty() && (g.complete == CompSeventh || g.complete == CompNinth))
        {
            const size_t want = g.complete == CompSeventh ? 1u : 2u;
            for (size_t i = 0; i < std::min (want, autoExtra.size()); ++i)
                if (! has (autoExtra[i]) && np + (int) neutrons.size() < kMaxPool) neutrons.push_back ({ autoExtra[i], NucNeutron, (int8_t) autoTriad[0], 100, 1000u + (uint32_t) i });
        }
        else if (! isAuto && np >= 1 && np <= 2 && (g.complete == CompTriad || g.complete == CompSeventh || g.complete == CompNinth))
        {
            // Con una o dos notas: el acorde de la escala sobre la más grave (llevada a la escala si no está).
            int root = 127; uint8_t rv = 100; for (auto& p : protonList) if (p.note < root) { root = p.note; rv = p.vel; }
            const auto pcs = scalePcs (scale(), g.key);
            const int rs = snap (root, pcs);
            int deg = 0;
            for (size_t d = 0; d < scale().steps.size(); ++d) if ((g.key + scale().steps[d]) % 12 == ((rs % 12) + 12) % 12) deg = (int) d;
            Rng none (1);
            const float color = g.complete == CompTriad ? 0.0f : (g.complete == CompSeventh ? 0.4f : 0.6f);
            const Chord c = buildChord (scale(), g.key, deg, color, 0.0f, none);
            for (int pc : c.pcs)
            {
                if (hasPc (pc)) continue;
                int iv = ((pc - rs) % 12 + 12) % 12;
                if (iv < 3) iv += 12;                     // segundas arriba, como novenas
                const int note = rs + iv;
                if (note <= 127 && ! has (note) && np + (int) neutrons.size() < kMaxPool) neutrons.push_back ({ note, NucNeutron, (int8_t) root, rv, 2000u + (uint32_t) note });
            }
        }
        if (g.complete == CompOctaves)
            for (int i = 0; i < np; ++i)
            {
                const int note = protonList[(size_t) i].note + 12;
                if (note <= 127 && ! has (note) && np + (int) neutrons.size() < kMaxPool) neutrons.push_back ({ note, NucNeutron, (int8_t) protonList[(size_t) i].note, protonList[(size_t) i].vel, 3000u + (uint32_t) note });
            }

        const bool starting = ! protonList.empty() && (wasEmpty || newChord);
        if (starting) ++chordSerial;           // antes de ordenar: el orden al azar es el del acorde nuevo
        played = protonList;
        std::sort (neutrons.begin(), neutrons.end(), [] (const PoolNote& a, const PoolNote& b) { return a.note < b.note; });
        played.insert (played.end(), neutrons.begin(), neutrons.end());
        base = played;
        switch (g.order)
        {
            case OrderPlayed: break;
            case OrderHighLow: std::stable_sort (base.begin(), base.end(), [] (const PoolNote& a, const PoolNote& b) { return a.note > b.note; }); break;
            case OrderRandom:
            {
                // Al azar reproducible: un orden fijo por acorde (agregar una nota no reordena las demás).
                auto key = [this] (int note) { uint32_t h = (uint32_t) (note + 1) * 2654435761u ^ (g.seed * 40503u + chordSerial * 97u); h ^= h << 13; h ^= h >> 17; h ^= h << 5; return h; };
                std::stable_sort (base.begin(), base.end(), [&] (const PoolNote& a, const PoolNote& b) { return key (a.note) < key (b.note); });
                break;
            }
            default: std::stable_sort (base.begin(), base.end(), [] (const PoolNote& a, const PoolNote& b) { return a.note < b.note; }); break;
        }

        if (logChords)
        {
            ChordChange c; c.at = t;
            for (auto& p : base) (p.kind == NucProton ? c.protons : c.neutrons).push_back (p.note);
            std::sort (c.protons.begin(), c.protons.end()); std::sort (c.neutrons.begin(), c.neutrons.end());
            if (chordLog.empty() || chordLog.back().protons != c.protons || chordLog.back().neutrons != c.neutrons) chordLog.push_back (std::move (c));
        }
        if (starting) chordStart (t);
        else if (base.empty() && ! wasEmpty) chordEnd (t);
    }
    void chordStart (int64_t t)
    {
        for (int l = 0; l < kLayers; ++l) if (layers[(size_t) l].restart) st[(size_t) l].restartPending = true;
        if (! session && ! (g.hostClock && prevHost)) startFreeSession (t);
    }
    void chordEnd (int64_t t)
    {
        // Sin el transporte del host, soltar todo termina la sesión: la próxima mano empieza en su muestra.
        if (session && ! (g.hostClock && prevHost) && ! autoActive()) endSession (t);
    }

    // ───────── armonía automática (la caminata de Orrery) ─────────
    int64_t chordTicks() const { return (int64_t) std::clamp (g.chordBars, 1, 16) * 4 * kTPB; }
    void startAuto (int64_t fromTick)
    {
        arng.reseed (g.seed * 2246822519u + 3u);
        degree = 0; buildAuto();
        nextChordTick = (floorDiv (fromTick, chordTicks()) + 1) * chordTicks();
    }
    void buildAuto()
    {
        const auto c = buildChord (scale(), g.key, degree, 0.6f, 0.0f, arng);     // tensión 0: no sortea
        autoChord = c;
        autoTriad.clear(); autoExtra.clear();
        int n = 48 + ((c.rootPc - 48) % 12 + 12) % 12;
        n = std::clamp (n + g.transpose, 0, 115);
        for (size_t i = 0; i < c.pcs.size(); ++i)
        {
            if (i > 0) { int m = (i < 3 ? autoTriad.back() : (autoExtra.empty() ? autoTriad.back() : autoExtra.back())) + 1; while (((m - g.transpose) % 12 + 12) % 12 != c.pcs[i]) ++m; n = m; }
            if (n > 127) break;
            (i < 3 ? autoTriad : autoExtra).push_back (n);
        }
    }
    void autoChordChange (int64_t t)
    {
        const int n = (int) scale().steps.size();
        if (g.progression == 1) degree = degree == 0 ? (n >= 6 ? 5 : 3) : 0;
        else if (g.progression == 2) { if (! arng.chance (0.8f)) degree = degree != 0 ? 0 : (n >= 6 ? 3 : 2); }
        else
        {
            int d = degree;
            for (int tries = 0; tries < 8; ++tries)
            {
                d = nextDegree (degree, n, 0.5f, 0.0f, arng);
                const int fifth = (degreeSemis (scale(), d + (n > 5 ? 4 : 3)) - degreeSemis (scale(), d) + 12) % 12;
                if (n <= 5 || fifth == 7) break;           // sin acordes disminuidos
            }
            degree = d;
        }
        buildAuto();
        nextChordTick += chordTicks();
        rebuildChord (t, true);
    }

    // ───────── pasos ─────────
    void generateStep (int l, int64_t now)
    {
        auto& L = layers[(size_t) l]; auto& S = st[(size_t) l];
        syncRows (l);
        const int len = std::clamp (L.length, 1, kMaxSteps);
        const int64_t gridTick = S.grid * S.st;
        if (g.phraseBars <= 0) S.nextPhrase = INT64_MAX;
        else if (S.nextPhrase == INT64_MAX) S.nextPhrase = (floorDiv (gridTick, phraseTicks()) + 1) * phraseTicks();
        else if (gridTick >= S.nextPhrase)
        {
            S.counter = 0; S.pat = 0; S.walk = 0;
            S.nextPhrase = (floorDiv (gridTick, phraseTicks()) + 1) * phraseTicks();
        }
        if (S.restartPending) { S.counter = 0; S.pat = 0; S.walk = 0; S.restartPending = false; }
        if (S.counter >= len) S.counter %= len;
        const int k = S.counter;
        if (k == 0) { if (S.cycles > 0) mutate (l, len); ++S.cycles; }
        S.cycleTick = gridTick - (int64_t) k * S.st;
        const int64_t onTick = onsetTick (l, S.grid), dur = std::max<int64_t> (1, onsetTick (l, S.grid + 1) - onTick);
        const Step s = S.work[(size_t) k];

        // Sorteos fijos por paso (uno por sentencia): el humanizado y la probabilidad, suene o no.
        const int hs = humanizeSamples (l);
        const float jr = S.hrng.uni();
        const int64_t jitter = hs > 0 ? (int64_t) std::llround ((double) jr * (double) hs) : 0;
        const float pr = S.rng.uni();
        const bool on = L.euPulses > 0 ? euclidHit (k, len, L.euPulses, L.euRotate) : s.on;
        std::array<Pick, kMaxPool> picks {};
        int np = 0;
        if (on && ! base.empty()) np = pick (l, s, picks);
        const bool plays = np > 0 && pr * 100.0f < (float) s.prob;
        const int ch = channelOf (l);
        const int64_t onS = std::max (now, sampleOf (onTick) + jitter);

        // Fila de CC: el valor del paso sale en su ataque (antes que la nota).
        if (L.ccNum >= 0 && L.ccNum <= 119 && s.cc >= 0 && s.cc != S.lastCc)
        {
            addPending (onS, 1, 2, (uint8_t) ch, (uint8_t) L.ccNum, (uint8_t) std::min<int> (127, s.cc), (int8_t) l, NucNone, -1, (int8_t) k, 0, 0, onS);
            S.lastCc = s.cc;
        }

        // Ligaduras abiertas del paso anterior: la misma nota sigue sin volver a atacar; otra, legato.
        const int r = std::clamp<int> (s.ratchet, 1, 4);
        auto& keep = tieKeep; keep.clear();
        for (auto& tn : S.ties)
        {
            bool cont = false;
            if (plays && r == 1)
                for (int i = 0; i < np; ++i)
                    if (! picks[(size_t) i].used && picks[(size_t) i].note == tn.note && ch == tn.ch) { picks[(size_t) i].used = true; cont = true; break; }
            if (cont)
            {
                if (s.tie) keep.push_back (tn);
                else addOff (std::max (onS + 1, sampleOf (onTick + gateTicks (L, dur)) + jitter), tn.ch, tn.note, l, tn.id, 0);
            }
            else addOff (onS, tn.ch, tn.note, l, tn.id, plays ? 3 : 0);      // legato: la nota nueva entra antes del note-off
        }
        S.ties.swap (keep);

        if (plays)
        {
            for (int i = 0; i < np; ++i)
            {
                const auto& p = picks[(size_t) i];
                if (p.used) continue;
                int vel = L.velMode == VelInput ? p.vel : (L.velMode == VelStep ? s.vel : L.velocity);
                if (s.accent) vel += g.accent;
                vel = std::clamp (vel, 1, 127);
                for (int sub = 0; sub < r; ++sub)
                {
                    const int64_t subOn = onTick + dur * sub / r, subDur = std::max<int64_t> (1, dur / r);
                    const int64_t a = std::max (now, sampleOf (subOn) + jitter);
                    const bool tie = s.tie && sub == r - 1;
                    uint8_t flags = (uint8_t) ((s.accent ? FlagAccent : 0) | (tie ? FlagTie : 0) | (sub > 0 ? FlagRatchet : 0) | (s.fixed >= 0 ? FlagFixed : 0));
                    const uint32_t id = nextId++;
                    if (tie)
                    {
                        addPending (a, 2, 1, (uint8_t) ch, (uint8_t) p.note, (uint8_t) vel, (int8_t) l, p.kind, p.src, (int8_t) k, flags, id, INT64_MAX);
                        S.ties.push_back ({ (uint8_t) ch, (uint8_t) p.note, id });
                        captureNote (a, ch, p.note, vel, l, (double) (sampleOf (onTick + dur) - a) / fs);
                    }
                    else
                    {
                        const int64_t off = std::max (a + 1, sampleOf (subOn + gateTicks (L, subDur)) + jitter);
                        addPending (a, 2, 1, (uint8_t) ch, (uint8_t) p.note, (uint8_t) vel, (int8_t) l, p.kind, p.src, (int8_t) k, flags, id, off);
                        addOff (off, ch, p.note, l, id, 0);
                        captureNote (a, ch, p.note, vel, l, (double) (off - a) / fs);
                    }
                }
            }
        }
        S.curStep = k; S.curOn = onTick; S.curDur = dur;
        ++S.grid; ++S.counter;
    }
    static int64_t gateTicks (const LayerParams& L, int64_t d) { return std::max<int64_t> (1, (int64_t) std::floor ((double) std::clamp (L.gate, 0.01f, 1.0f) * (double) d)); }

    // Las notas del paso según el patrón (avanza el patrón; la nota fija no).
    int pick (int l, const Step& s, std::array<Pick, kMaxPool>& outPicks)
    {
        auto& L = layers[(size_t) l]; auto& S = st[(size_t) l];
        auto fold = [] (int n) { while (n > 127) n -= 12; while (n < 0) n += 12; return n; };
        const int shiftOct = 12 * (std::clamp (L.octave, -3, 3) + std::clamp<int> (s.oct, -3, 3));
        if (s.fixed >= 0)
        {
            // Grado de la escala sobre la tónica de la octava de la mano (la tónica en o debajo de la nota más grave).
            int lo = 127; for (auto& p : base) lo = std::min (lo, p.note);
            int tonic = 12 * (lo / 12) + g.key; if (tonic > lo) tonic -= 12;
            const int note = fold (tonic + degreeSemis (scale(), std::clamp<int> (s.fixed, 0, 21)) + shiftOct);
            outPicks[0] = { note, NucNeutron, -1, base.empty() ? (uint8_t) 100 : base[0].vel, false };
            return 1;
        }
        const auto& B = L.pattern == PatPlayed ? played : base;
        const int m = std::min ((int) B.size(), kMaxPool);
        if (m == 0) return 0;
        const int R = std::clamp (L.octaves, 1, 4);
        const int64_t c = S.pat + std::max (0, L.shift);
        ++S.pat;
        if (L.pattern == PatChord)
        {
            int oct;
            if (L.octMode == OctRandom) oct = R > 1 ? S.rng.irange (0, R - 1) : 0;
            else if (L.octMode == OctAlternate) oct = pingPong (c, R);
            else oct = (int) (c % R);
            for (int i = 0; i < m; ++i) outPicks[(size_t) i] = { fold (B[(size_t) i].note + 12 * oct + shiftOct), B[(size_t) i].kind, B[(size_t) i].src, B[(size_t) i].vel, false };
            // Sin repetidas (una octava que se pliega puede caer sobre otra nota del acorde).
            int w = 0;
            for (int i = 0; i < m; ++i) { bool dup = false; for (int j = 0; j < w; ++j) dup = dup || outPicks[(size_t) j].note == outPicks[(size_t) i].note; if (! dup) outPicks[(size_t) w++] = outPicks[(size_t) i]; }
            return w;
        }
        const int M = L.octMode == OctStack ? m * R : m;
        int i;
        if (L.pattern == PatRandom) i = S.rng.irange (0, M - 1);
        else if (L.pattern == PatWalk)
        {
            i = std::clamp (S.walk, 0, M - 1);
            const int dir = S.rng.chance (0.5f) ? 1 : -1;
            int nw = i + dir;
            if (nw < 0) nw = std::min (1, M - 1); else if (nw >= M) nw = std::max (0, M - 2);
            S.walk = nw;
        }
        else i = patternIndex (L.pattern, c, M, s.finger);
        int oct = 0;
        if (L.octMode == OctStack) { oct = i / m; i = i % m; }
        else if (L.octMode == OctAlternate) oct = pingPong (c, R);     // nota por nota: 0, 1, 0, 1 (o 0, 1, 2, 1...)
        else if (R > 1) oct = S.rng.irange (0, R - 1);
        const auto& p = B[(size_t) i];
        outPicks[0] = { fold (p.note + 12 * oct + shiftOct), p.kind, p.src, p.vel, false };
        return 1;
    }

    // Mutación al empezar cada ciclo (desde el segundo): la capa cambia despacio, con su propia semilla.
    void mutate (int l, int len)
    {
        const float m = std::clamp (layers[(size_t) l].mutate, 0.0f, 1.0f);
        if (m <= 0.0f) return;
        auto& S = st[(size_t) l];
        const int tries = 1 + (int) (m * 3.0f);
        for (int t = 0; t < tries; ++t)
        {
            if (! S.mrng.chance (m)) continue;
            const int k = S.mrng.irange (0, len - 1);
            const int op = S.mrng.irange (0, 4);
            const int dir = S.mrng.chance (0.5f) ? 1 : -1;
            auto& s = S.work[(size_t) k];
            switch (op)
            {
                case 0: s.prob = (uint8_t) std::clamp<int> (s.prob + 25 * dir, 15, 100); break;
                case 1: s.oct = (int8_t) std::clamp<int> (s.oct + dir, -2, 2); break;
                case 2: if (layers[(size_t) l].euPulses == 0 && k != 0) s.on = ! s.on; else s.prob = (uint8_t) std::clamp<int> (s.prob - 25, 15, 100); break;
                case 3: s.vel = (uint8_t) std::clamp<int> (s.vel + 16 * dir, 20, 127); s.accent = dir > 0 && s.accent ? false : s.accent; break;
                default: s.ratchet = (uint8_t) (s.ratchet == 1 ? 2 : 1); break;
            }
        }
    }

    // ───────── salida ─────────
    void addPending (int64_t at, uint8_t prio, uint8_t type, uint8_t ch, uint8_t d1, uint8_t d2, int8_t layer, int8_t nuc, int8_t src, int8_t step, uint8_t flags, uint32_t id, int64_t end)
    {
        pending.push_back ({ at, prio, type, ch, d1, d2, layer, nuc, src, step, flags, id, pendSeq++, end });
        pendDirty = true;
    }
    // prio: 0 note-off, 1 CC, 2 note-on, 3 note-off después del ataque (legato).
    void addOff (int64_t at, int ch, int note, int layer, uint32_t id, uint8_t prio) { addPending (at, prio, 0, (uint8_t) ch, (uint8_t) note, 0, (int8_t) layer, NucNone, -1, -1, 0, id, at); }

    void push (int64_t at, int s, int d1, int d2, int layer, int size = 3, int8_t nuc = NucNone, int8_t src = -1, int8_t step = -1, uint8_t flags = 0)
    {
        OutEvent e; e.offset = (int) std::clamp<int64_t> (at - blockStart, 0, blockN - 1);
        e.data[0] = (uint8_t) s; e.data[1] = (uint8_t) d1; e.data[2] = (uint8_t) d2; e.size = (uint8_t) size;
        e.layer = (int8_t) layer; e.nucleon = nuc; e.src = src; e.step = step; e.flags = flags;
        out.push_back (e);
    }

    // Choques de la misma nota en el mismo canal (dos capas, un ratchet, una ligadura): si ya suena y atacó en la
    // misma muestra, se funden (un solo note-on, dura lo que la más larga); si atacó antes, se corta (note-off y
    // note-on en esa muestra). Nunca dos note-on seguidos sin su note-off.
    void emitPending (int64_t upTo)
    {
        if (pending.empty()) return;
        if (pendDirty)
        {
            std::sort (pending.begin(), pending.end(), [] (const Pending& a, const Pending& b)
                       { return a.at < b.at || (a.at == b.at && (a.prio < b.prio || (a.prio == b.prio && a.seq < b.seq))); });
            pendDirty = false;
        }
        size_t k = 0;
        for (; k < pending.size() && pending[k].at < upTo; ++k)
        {
            const auto& p = pending[k];
            auto& id = activeId[p.ch][p.d1];
            if (p.type == 0)
            {
                if (id == p.id) { id = 0; push (p.at, 0x80 | p.ch, p.d1, 0, activeLayer[p.ch][p.d1]); }     // con la capa que mandó el note-on
            }
            else if (p.type == 2) push (p.at, 0xB0 | p.ch, p.d1, p.d2, p.layer, 3, NucNone, -1, p.step);
            else
            {
                if (id != 0)
                {
                    if (activeStart[p.ch][p.d1] == p.at)
                    {
                        if (p.end > activeEnd[p.ch][p.d1]) { id = p.id; activeEnd[p.ch][p.d1] = p.end; }
                        continue;
                    }
                    push (p.at, 0x80 | p.ch, p.d1, 0, activeLayer[p.ch][p.d1]);
                }
                id = p.id; activeLayer[p.ch][p.d1] = p.layer; activeStart[p.ch][p.d1] = p.at; activeEnd[p.ch][p.d1] = p.end;
                push (p.at, 0x90 | p.ch, p.d1, p.d2, p.layer, 3, p.nucleon, p.src, p.step, p.flags);
            }
        }
        pending.erase (pending.begin(), pending.begin() + (long) k);
    }

    void closeTies (int l, int64_t at, uint8_t prio)
    {
        auto& S = st[(size_t) l];
        for (auto& tn : S.ties) addOff (at, tn.ch, tn.note, l, tn.id, prio);
        S.ties.clear();
    }
    void releaseLayer (int l, int64_t at)
    {
        emitPending (at);
        for (int ch = 0; ch < 16; ++ch)
            for (int n = 0; n < 128; ++n)
                if (activeId[(size_t) ch][(size_t) n] != 0 && activeLayer[(size_t) ch][(size_t) n] == l) { activeId[(size_t) ch][(size_t) n] = 0; push (at, 0x80 | ch, n, 0, l); }
        pending.erase (std::remove_if (pending.begin(), pending.end(), [l] (const Pending& p) { return p.layer == l; }), pending.end());
        st[(size_t) l].ties.clear();
        st[(size_t) l].lastCc = -1;
    }
    void allNotesOffAt (int64_t at)
    {
        emitPending (at);
        for (int ch = 0; ch < 16; ++ch)
            for (int n = 0; n < 128; ++n)
                if (activeId[(size_t) ch][(size_t) n] != 0) { activeId[(size_t) ch][(size_t) n] = 0; push (at, 0x80 | ch, n, 0, activeLayer[(size_t) ch][(size_t) n]); }
        pending.clear();
        for (auto& S : st) { S.ties.clear(); S.lastCc = -1; }
    }

    void captureNote (int64_t at, int ch, int note, int vel, int layer, double dur)
    {
        const int h = captureHead.load (std::memory_order_relaxed);
        capture[(size_t) (h % (int) capture.size())] = { (double) at / fs, (uint8_t) (ch + 1), (uint8_t) note, (uint8_t) vel, (int8_t) layer, (float) dur };
        captureHead.store (h + 1, std::memory_order_release);
    }

    // ───────── vista y fusión ─────────
    static int64_t gcd64 (int64_t a, int64_t b) { while (b != 0) { const int64_t t = a % b; a = b; b = t; } return a < 0 ? -a : a; }
    static int64_t mulmod (int64_t a, int64_t b, int64_t m)
    {
        a = ((a % m) + m) % m; b = ((b % m) + m) % m;
        int64_t r = 0;
        while (b > 0) { if (b & 1) { r += a; if (r >= m) r -= m; } a += a; if (a >= m) a -= m; b >>= 1; }
        return r;
    }
    static int64_t invmod (int64_t a, int64_t m)
    {
        int64_t g0 = m, g1 = ((a % m) + m) % m, x0 = 0, x1 = 1;
        while (g1 != 0) { const int64_t q = g0 / g1; int64_t t = g0 - q * g1; g0 = g1; g1 = t; t = x0 - q * x1; x0 = x1; x1 = t; }
        return ((x0 % m) + m) % m;
    }
public:
    // Próximo tick (> after) en que todas las capas encendidas están en su paso 0, o -1 si nunca (o no entra en 63
    // bits). Cada capa vuelve al paso 0 en cycleTick + k·período; se resuelve como congruencias (teorema chino).
    int64_t nextFusionTick (int64_t after) const
    {
        int64_t a = 0, m = 1; bool any = false;
        for (int l = 0; l < kLayers; ++l)
        {
            const auto& L = layers[(size_t) l]; const auto& S = st[(size_t) l];
            if (! L.on || ! S.wasOn) continue;
            const int64_t P = (int64_t) std::clamp (L.length, 1, kMaxSteps) * S.st;
            const int64_t c = S.restartPending ? S.grid * S.st : S.cycleTick;
            const int64_t ci = ((c % P) + P) % P;
            if (! any) { a = ci; m = P; any = true; continue; }
            const int64_t gg = gcd64 (m, P);
            if (((ci - a) % gg) != 0) return -1;
            const int64_t m2 = P / gg;
            if (m > ((int64_t) 1 << 62) / m2) return -1;
            const int64_t k = mulmod ((ci - a) / gg, invmod ((m / gg) % m2, m2), m2);
            const int64_t lcm = m * m2;
            a = ((a + mulmod (m, k, lcm)) % lcm + lcm) % lcm;
            m = lcm;
        }
        if (! any) return -1;
        int64_t T = a;
        if (T <= after) T += ((after - T) / m + 1) * m;
        if (g.phraseBars > 0) { const int64_t ph = phraseTicks(); const int64_t nb = (floorDiv (after, ph) + 1) * ph; if (nb < T) T = nb; }
        return T;
    }
    // Para las pruebas y la vista: paso actual de cada capa, su próxima grilla, su largo de paso en ticks.
    int layerCounter (int l) const { return st[(size_t) l].counter; }
    int64_t layerGrid (int l) const { return st[(size_t) l].grid; }
    int64_t layerStepTicks (int l) const { return st[(size_t) l].st; }
    int64_t sampleAtTick (int64_t tick) const { return sampleOf (tick); }
    int chordSize() const { return (int) base.size(); }
private:
    void updateView()
    {
        auto& v = view;
        v.running = session; v.bpm = bpm;
        const double nowTick = session ? tickAt (abs) : 0.0;
        v.beat = nowTick / (double) kTPB;
        for (int l = 0; l < kLayers; ++l)
        {
            const auto& S = st[(size_t) l]; const auto& L = layers[(size_t) l];
            const int len = std::clamp (L.length, 1, kMaxSteps);
            v.length[(size_t) l] = len; v.step[(size_t) l] = S.curStep;
            const double frac = std::clamp ((nowTick - (double) S.curOn) / (double) std::max<int64_t> (1, S.curDur), 0.0, 1.0);
            v.phase[(size_t) l] = session && L.on ? (float) std::fmod (((double) S.curStep + frac) / (double) len, 1.0) : 0.0f;
        }
        v.protons = v.neutrons = 0;
        for (auto& p : base)
        {
            if (p.kind == NucProton && v.protons < kMaxPool) v.protonNotes[(size_t) v.protons++] = (uint8_t) p.note;
            else if (p.kind == NucNeutron && v.neutrons < kMaxPool) v.neutronNotes[(size_t) v.neutrons++] = (uint8_t) p.note;
        }
        v.chordSerial = chordSerial;
        std::string name;
        if (autoActive()) name = autoChord.name;
        else if (! protonList.empty()) { int lo = 127; for (auto& p : protonList) lo = std::min (lo, p.note); name = noteName (lo % 12); }
        std::memset (v.chord, 0, sizeof (v.chord)); std::strncpy (v.chord, name.c_str(), sizeof (v.chord) - 1);
        v.fusionBeats = -1.0; v.fusionTick = -1; v.fusionSteps = {};
        if (session)
        {
            const int64_t now = (int64_t) std::floor (nowTick);
            const int64_t T = nextFusionTick (now);
            if (T >= 0)
            {
                v.fusionBeats = (double) (T - now) / (double) kTPB; v.fusionTick = T;
                for (int l = 0; l < kLayers; ++l) if (layers[(size_t) l].on) v.fusionSteps[(size_t) l] = std::max<int64_t> (0, ceilDiv (T - st[(size_t) l].grid * st[(size_t) l].st, st[(size_t) l].st));
            }
        }
    }
};

} // namespace dh::val
