#pragma once
// Motor de Orrery: genera MIDI para hasta 8 carriles, cada uno en su canal, sobre una armonía común
// que cambia despacio. Sin JUCE: lo usan el plugin y orreryrender.
//
// Tiempo en pulsos (negras). Cada carril tiene un rol (drone, pad, melodía, arpegio, nube, bajo, pulso),
// puede grabar lo que genera en un lazo de largo propio y repetirlo con mutaciones (la técnica de Eno
// en Music for Airports: lazos de largos distintos que casi nunca coinciden), repetir sus notas como eco,
// mandar una curva de CC y una deriva de afinación por pitch bend. Encima: sesión con fades, respiración
// y un modo suave para escucha tranquila.

#include "Theory.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstring>
#include <vector>

namespace dh::orr {

constexpr int kLanes = 8;
enum Role : int { Drone = 0, Pad, Melody, Arp, Cloud, Bass, Pulse, RoleCount };
inline const char* roleName (int r) { static const char* N[RoleCount] = { "Drone", "Pad", "Melody", "Arp", "Cloud", "Bass", "Pulse" }; return N[(r % RoleCount + RoleCount) % RoleCount]; }
// Registro central de cada rol (nota MIDI) y rango típico.
inline int roleCenter (int r) { static const int C[RoleCount] = { 43, 60, 69, 72, 79, 36, 60 }; return C[(r % RoleCount + RoleCount) % RoleCount]; }

enum Follow : int { FollowChord = 0, FollowScale, FollowRoot };
enum CcShape : int { CcSine = 0, CcDrift, CcBreath, CcSwell };
enum ArpPattern : int { ArpUp = 0, ArpDown, ArpUpDown, ArpRandom };

struct LaneParams
{
    bool on = false;
    int role = Pad, channel = 1, octave = 0, range = 19;   // octava sobre el centro del rol; rango en semitonos
    float density = 0.5f, length = 0.5f, velocity = 0.6f, velRand = 0.2f, probability = 1.0f;
    bool loop = false; float loopBeats = 16.0f, mutate = 0.15f;
    int rate = 2;                                    // 0 = 2 pulsos, 1 = 1, 2 = 1/2, 3 = 1/4, 4 = 1/8 (en negras: 2, 1, .5, .25, .125)
    int euclidSteps = 8, euclidPulses = 0, euclidRotate = 0;
    int arpPattern = ArpUpDown;
    int echoes = 0; float echoBeats = 0.75f, echoDecay = 0.55f;
    int follow = FollowChord, voices = 4;
    int ccNum = -1, ccShape = CcSine; float ccBeats = 16.0f, ccDepth = 0.4f, ccCenter = 0.5f;
    float driftCents = 0.0f;
};

struct GlobalParams
{
    bool run = false, hostClock = true, runWhenStopped = true;
    float tempo = 60.0f;
    int key = 2, scale = 2;                          // Re dórico
    float density = 0.5f, motion = 0.5f, space = 0.5f, brightness = 0.5f, tension = 0.2f;
    float chordBeats = 16.0f, color = 0.5f;
    int progression = 0;                             // 0 caminata, 1 drone (casi fija), 2 dos acordes
    uint32_t seed = 1;
    float breathRate = 0.0f;                         // respiraciones por minuto, 0 = apagado
    float sessionMinutes = 0.0f, fadeIn = 20.0f, fadeOut = 40.0f;   // 0 = sin fin; fades en segundos
    bool gentle = false, fadeCC7 = true;
    float refPitch = 440.0f; int bendRange = 2; bool tuneByBend = false;
    float humanizeMs = 8.0f, level = 1.0f;
};

struct OutEvent { int offset; uint8_t data[3]; uint8_t size; int8_t lane; };

// Armonía compartida entre instancias (vínculo líder / seguidoras).
struct Harmony
{
    int key = 0, scale = 0, root = 0; uint16_t mask = 0; uint32_t serial = 0;
    double start = 0.0, len = 16.0; char name[16] {};
    double now = 0.0;       // pulso de la líder al publicar: la seguidora traduce start a su propia línea de tiempo
};

class Generator
{
public:
    std::array<LaneParams, kLanes> lanes {};
    GlobalParams g;

    // Lo que ve el editor.
    struct View
    {
        std::array<float, kLanes> phase {};          // fase del lazo o del ciclo de cada carril (0..1)
        std::array<float, kLanes> activity {};       // notas sonando (con decaimiento)
        std::array<float, kLanes> ccValue {};
        float breath = 0.0f, sessionSeconds = 0.0f, fade = 1.0f, chordPhase = 0.0f;
        bool running = false, ended = false;
        char chord[16] {};
    } view;

    // Lo último que sonó, para capturar como clip MIDI (anillo).
    struct Captured { double beat; uint8_t ch, note, vel; float dur; };
    std::vector<Captured> capture = std::vector<Captured> (8192);
    std::atomic<int> captureHead { 0 };
    double captureBpm = 60.0;

    void prepare (double sampleRate)
    {
        // Cambio de frecuencia en marcha: el tiempo de sesión se conserva y el pulso se re-ancla (si no, saltaba).
        if (fs > 0.0 && sampleRate != fs) { sessionSamples = (int64_t) std::llround ((double) sessionSamples * sampleRate / fs); anchorBpm = -1.0; }
        fs = sampleRate; out.reserve (8192); pending.reserve (32768);
    }

    // Avanza un bloque. playing/ppq/bpm del host (si g.hostClock). Llena out (ordenado por offset).
    void process (int n, bool hostPlaying, double hostPpq, double hostBpm, std::vector<OutEvent>& outEvents)
    {
        out.clear();
        const bool useHost = g.hostClock && hostPlaying;
        const double bpm = useHost ? std::max (20.0, hostBpm) : (double) std::max (20.0f, g.tempo);
        if (g.run && ! prevRun && ended) resetSession();      // Play después de una sesión terminada: arranca otra
        prevRun = g.run;
        if (resetRequest.exchange (false)) resetSession();
        const bool shouldRun = g.run && (! g.hostClock || hostPlaying || g.runWhenStopped) && ! ended;
        if (shouldRun && ! running) start (useHost ? hostPpq : 0.0);
        if (! shouldRun && running) { stop (0); outEvents = out; return; }
        if (! running) { outEvents = out; view.running = false; return; }

        // El tiempo sale de contar muestras desde un ancla (que sólo se mueve si cambia el tempo), no de
        // sumar bloques: así la música no depende del tamaño de bloque del host.
        double b0, b1;
        if (useHost)
        {
            if (std::abs (hostPpq - beat) > 0.25) { allNotesOff (0); resetLanes (hostPpq); }   // salto del transporte
            b0 = hostPpq; b1 = b0 + bpm / 60.0 / fs * n;
            anchorBeat = b0; anchorSession = sessionSamples; anchorBpm = bpm;
        }
        else
        {
            if (std::abs (bpm - anchorBpm) > 1.0e-9) { anchorBeat = beat; anchorSession = sessionSamples; anchorBpm = bpm; }
            b0 = anchorBeat + (double) (sessionSamples - anchorSession) * bpm / 60.0 / fs;
            b1 = anchorBeat + (double) (sessionSamples + n - anchorSession) * bpm / 60.0 / fs;
        }
        blockB0 = b0; blockLen = b1 - b0; blockN = n; curBpm = bpm;
        captureBpm = bpm;

        // Sesión y fades.
        const int64_t session0 = sessionSamples;
        sessionSamples += n;
        sessionSec = (double) sessionSamples / fs;
        fade = gainAt (sessionSec);
        if (g.sessionMinutes > 0.0f && sessionSec >= g.sessionMinutes * 60.0)
        {
            ended = true;
            stop (n - 1);
            outEvents = out; return;
        }

        // Armonía: propia, o la de la líder.
        if (following)
        {
            if (external.serial != 0 && external.serial != seenSerial) adopt (external, b0);
            if (nextChordBeat < b1) nextChordBeat = b1 + chordLen;     // la líder se calló: se sostiene el acorde
        }

        // Carriles y cambios de acorde, en orden de tiempo (a igual tiempo, primero el acorde).
        for (int l = 0; l < kLanes; ++l)
        {
            auto& L = lanes[(size_t) l]; auto& S = st[(size_t) l];
            if (! L.on) { if (S.wasOn) { releaseLane (l, 0); S.wasOn = false; } continue; }
            if (! S.wasOn) { S.wasOn = true; S.next = b0; S.voicing.clear(); S.loopEvents.clear(); S.recording = L.loop; S.prevLoop = L.loop; S.cycleStart = b0; S.orbitStart = b0; }
        }
        for (int guard = 0; guard < 64 * kLanes; ++guard)
        {
            double t = following ? 1.0e300 : nextChordBeat; int who = -1;
            for (int l = 0; l < kLanes; ++l)
                if (lanes[(size_t) l].on && st[(size_t) l].next < t) { t = st[(size_t) l].next; who = l; }
            if (t >= b1) break;
            if (who < 0) changeChord (nextChordBeat);
            else generate (who, t);
        }

        // Salida: notas y controles intercalados en el tiempo.
        controlOutputs (session0, n);
        emitPending (b1);
        std::stable_sort (out.begin(), out.end(), [] (const OutEvent& a, const OutEvent& e) { return a.offset < e.offset; });
        beat = b1;
        playedBeats += blockLen;

        // Vista.
        for (int l = 0; l < kLanes; ++l)
        {
            auto& S = st[(size_t) l]; const auto& L = lanes[(size_t) l];
            // Cada carril orbita con su propio período (el largo de su lazo): los planetas casi nunca se alinean.
            view.phase[(size_t) l] = (float) std::fmod ((beat - (L.loop ? S.cycleStart : S.orbitStart)) / effLoopBeats (L) + 10.0, 1.0);
            view.activity[(size_t) l] = std::max (view.activity[(size_t) l] * 0.92f, (float) S.sounding / 3.0f);
        }
        view.breath = breathAt (sessionSec); view.sessionSeconds = (float) sessionSec; view.fade = fade;
        view.chordPhase = (float) std::clamp ((beat - chordStart) / std::max (1.0, chordLen), 0.0, 1.0);
        view.running = true; view.ended = ended;
        std::strncpy (view.chord, chord.name.c_str(), sizeof (view.chord) - 1);
        outEvents = out;
    }

    // Vínculo: la líder publica su armonía; una seguidora la adopta (en vez de elegir sus acordes).
    bool following = false;
    Harmony external;
    Harmony published() const
    {
        Harmony h; h.key = g.key; h.scale = g.scale; h.root = chord.rootPc; h.serial = serial; h.start = chordStart; h.len = chordLen; h.now = beat;
        for (int pc : chord.pcs) h.mask = (uint16_t) (h.mask | (1u << pc));
        std::strncpy (h.name, chord.name.c_str(), sizeof (h.name) - 1);
        return h;
    }

    void panic (std::vector<OutEvent>& outEvents) { out.clear(); allNotesOff (0); for (int ch = 1; ch <= 16; ++ch) { push (0, 0xB0 | (ch - 1), 123, 0, -1); push (0, 0xB0 | (ch - 1), 120, 0, -1); } outEvents = out; }
    std::atomic<bool> resetRequest { false };       // el hilo de mensajes pide una sesión nueva; la hace el audio
    void resetSession() { ended = false; sessionSec = 0.0; sessionSamples = 0; anchorSession = 0; anchorBeat = beat; }
    bool isRunning() const { return running; }

    // Satélites: una réplica toma todo el estado musical de la generadora del Main (no la captura) y desde
    // ahí avanza igual, bloque a bloque. Los vectores reusan su capacidad (sin pedir memoria si alcanza).
    void copyStateFrom (const Generator& o)
    {
        lanes = o.lanes; g = o.g; view = o.view; captureBpm = o.captureBpm;
        following = o.following; external = o.external; resetRequest.store (o.resetRequest.load());
        fs = o.fs; beat = o.beat; blockB0 = o.blockB0; blockLen = o.blockLen; curBpm = o.curBpm; blockN = o.blockN;
        running = o.running; ended = o.ended; prevRun = o.prevRun; playedBeats = o.playedBeats;
        sessionSec = o.sessionSec; fade = o.fade; sessionSamples = o.sessionSamples; anchorSession = o.anchorSession;
        anchorBeat = o.anchorBeat; anchorBpm = o.anchorBpm; genT = o.genT;
        for (int l = 0; l < kLanes; ++l)
        {
            auto& d = st[(size_t) l]; const auto& s = o.st[(size_t) l];
            d.wasOn = s.wasOn; d.recording = s.recording; d.next = s.next; d.cycleStart = s.cycleStart; d.restUntil = s.restUntil;
            d.orbitStart = s.orbitStart; d.lastNote = s.lastNote; d.phraseLeft = s.phraseLeft; d.step = s.step; d.arpIdx = s.arpIdx;
            d.sounding = s.sounding; d.loopIdx = s.loopIdx; d.voicing.assign (s.voicing.begin(), s.voicing.end());
            d.loopEvents.assign (s.loopEvents.begin(), s.loopEvents.end()); d.rng = s.rng; d.crng = s.crng;
            d.ccPhase = s.ccPhase; d.ccWalk = s.ccWalk; d.drift = s.drift; d.driftTarget = s.driftTarget; d.lastCc = s.lastCc;
            d.lastBend = s.lastBend; d.lastCc7 = s.lastCc7; d.lastCh = s.lastCh; d.prevLoop = s.prevLoop; d.swell = s.swell;
        }
        hrng = o.hrng; chord.degree = o.chord.degree; chord.rootPc = o.chord.rootPc; chord.pcs.assign (o.chord.pcs.begin(), o.chord.pcs.end()); chord.name = o.chord.name;
        degree = o.degree; chordStart = o.chordStart; nextChordBeat = o.nextChordBeat; chordLen = o.chordLen;
        pending.assign (o.pending.begin(), o.pending.end()); out.assign (o.out.begin(), o.out.end());
        activeId = o.activeId; activeLane = o.activeLane; activeVel = o.activeVel; nextId = o.nextId;
        serial = o.serial; seenSerial = o.seenSerial;
    }
    // Notas que suenan ahora (canal 0..15, nota, carril, velocidad): un satélite que se suma en marcha las ataca.
    template <typename F> void forEachSounding (F&& f) const
    {
        for (int ch = 0; ch < 16; ++ch)
            for (int n = 0; n < 128; ++n)
                if (activeId[(size_t) ch][(size_t) n] != 0) f (ch, n, (int) activeLane[(size_t) ch][(size_t) n], (int) activeVel[(size_t) ch][(size_t) n]);
    }

private:
    struct Pending { double beat; uint8_t type; uint8_t ch, note, vel; int8_t lane; uint32_t id; };   // type 0 off, 1 on
    struct LoopEv { double off; int note; int vel; double dur; };
    struct LaneState
    {
        bool wasOn = false, recording = false;
        double next = 0.0, cycleStart = 0.0, restUntil = 0.0, orbitStart = 0.0;
        int lastNote = -1, phraseLeft = 0, step = 0, arpIdx = 0, sounding = 0, loopIdx = 0;
        std::vector<int> voicing;
        std::vector<LoopEv> loopEvents;
        Rng rng { 1 }, crng { 1 };      // notas / controles (separados: los controles no corren la música)
        float ccPhase = 0.0f, ccWalk = 0.5f, drift = 0.0f, driftTarget = 0.0f; int lastCc = -1, lastBend = -1;
        int lastCc7 = -1, lastCh = -1;
        bool prevLoop = false;
        float swell = 0.0f;
    };

    double fs = 48000.0, beat = 0.0, blockB0 = 0.0, blockLen = 1.0, curBpm = 60.0;
    int blockN = 1;
    bool running = false, ended = false, prevRun = false;
    double playedBeats = 0.0;     // pulsos tocados en total (la captura no se confunde con stops ni saltos)
    double sessionSec = 0.0; float fade = 1.0f;
    int64_t sessionSamples = 0, anchorSession = 0; double anchorBeat = 0.0, anchorBpm = -1.0;
    double genT = 0.0;     // instante que se está generando
    std::array<LaneState, kLanes> st {};
    Rng hrng { 1 };
    Chord chord; int degree = 0; double chordStart = 0.0, nextChordBeat = 0.0, chordLen = 16.0;
    std::vector<Pending> pending;
    std::vector<OutEvent> out;
    // Notas sonando por canal: id de la última nota-on (0 = apagada).
    std::array<std::array<uint32_t, 128>, 16> activeId {};
    std::array<std::array<int8_t, 128>, 16> activeLane {};
    std::array<std::array<uint8_t, 128>, 16> activeVel {};      // sólo para los satélites (no cambia lo que sale)
    uint32_t nextId = 1;

    static float mfac (float macro) { return (float) dm::exp2 (2.0 * (double) (macro - 0.5f)); }      // 0..1 → ×0,5..×2
    const Scale& scale() const { return scales()[(size_t) std::clamp (g.scale, 0, (int) scales().size() - 1)]; }
    double effLoopBeats (const LaneParams& L) const { return std::max (2.0, (double) L.loopBeats); }
    float tensionEff() const { return g.gentle ? std::min (g.tension, 0.3f) : g.tension; }
    float densityEff (const LaneParams& L) const { float d = std::clamp (L.density * mfac (g.density), 0.0f, 1.0f); return g.gentle ? std::min (d, 0.7f) : d; }

    void start (double atBeat)
    {
        running = true; beat = atBeat; anchorBpm = -1.0;
        hrng.reseed (g.seed);
        for (int l = 0; l < kLanes; ++l)
        {
            st[(size_t) l] = LaneState{};
            st[(size_t) l].rng.reseed (g.seed * 7919u + (uint32_t) l * 104729u + 17u);
            st[(size_t) l].crng.reseed (g.seed * 6271u + (uint32_t) l * 15485863u + 91u);
        }
        degree = 0; chord = buildChord (scale(), g.key, 0, g.color, tensionEff(), hrng); ++serial; seenSerial = 0;
        chordStart = atBeat; chordLen = chordLength(); nextChordBeat = atBeat + chordLen;
        for (auto& ch : activeId) ch.fill (0);
        pending.clear();
        if (sessionSec <= 0.0) fade = gainAt (0.0);
    }
    void stop (int offset)
    {
        allNotesOff (offset);
        centerControls (offset);
        running = false; view.running = false; for (auto& S : st) S.wasOn = false;
    }
    // Pitch bend al centro y CC7 a 100 en los canales donde Orrery los había movido (si no, el sinte quedaba
    // desafinado o bajo después de parar).
    void centerControls (int offset)
    {
        for (int l = 0; l < kLanes; ++l)
        {
            auto& S = st[(size_t) l];
            const int ch = S.lastCh >= 0 ? S.lastCh : std::clamp (lanes[(size_t) l].channel, 1, 16) - 1;
            if (S.lastBend >= 0 && S.lastBend != 8192) push (offset, 0xE0 | ch, 0, 64, l);
            if (S.lastCc7 >= 0 && S.lastCc7 != 100) push (offset, 0xB0 | ch, 7, 100, l);
            S.lastBend = S.lastCc7 = S.lastCc = -1;
        }
    }
    void resetLanes (double atBeat)
    {
        pending.clear();
        for (auto& S : st) { S.wasOn = false; S.next = atBeat; }
        chordStart = atBeat; nextChordBeat = atBeat + chordLength();
    }

    double chordLength() const
    {
        double len = (double) g.chordBeats / mfac (g.motion);
        if (g.breathRate > 0.0f)
        {
            // Que los cambios de acorde caigan en un número entero de respiraciones.
            const double breathBeats = 60.0 / g.breathRate * curBpm / 60.0;
            len = std::max (1.0, std::round (len / breathBeats)) * breathBeats;
        }
        return std::max (2.0, len);
    }

    uint32_t serial = 0, seenSerial = 0;
    void adopt (const Harmony& h, double now)
    {
        seenSerial = h.serial;
        Chord c; c.rootPc = h.root; c.pcs.push_back (h.root);
        for (int pc = 0; pc < 12; ++pc) if ((h.mask >> pc) & 1 && pc != h.root) c.pcs.push_back (pc);
        c.name = h.name;
        chord = c;
        // Si las dos arrancaron en otro momento (reloj propio), los pulsos de la líder se traducen a los de la
        // seguidora; si comparten la línea de tiempo (diferencia menor a un pulso: el orden de proceso), no.
        const double offset = std::abs (h.now - now) > 1.0 ? now - h.now : 0.0;
        chordStart = h.start + offset;
        chordLen = std::max (1.0, h.len); nextChordBeat = std::max (now + 0.5, chordStart + chordLen);
        for (int l = 0; l < kLanes; ++l)
        {
            const int r = lanes[(size_t) l].role;
            if (lanes[(size_t) l].on && ! lanes[(size_t) l].loop && (r == Pad || r == Drone || r == Bass)) st[(size_t) l].next = now;
        }
    }

    void changeChord (double at)
    {
        ++serial;
        const int n = (int) scale().steps.size();
        const float stasis = g.progression == 1 ? 0.85f : 0.0f;
        if (g.progression == 2) degree = degree == 0 ? (n >= 6 ? 5 : 3) : 0;
        else if (g.progression == 1)
        {
            // Drone: el centro casi no se mueve; cuando lo hace, va a la cuarta o a la quinta y vuelve.
            if (! hrng.chance (stasis)) degree = degree != 0 ? 0 : (n >= 6 ? (hrng.chance (0.5f) ? 3 : 4) : 2);
        }
        else
        {
            // Caminata: con poca tensión se evitan los acordes disminuidos (la quinta de la escala sobre la raíz no es justa).
            int d = degree;
            for (int tries = 0; tries < 8; ++tries)
            {
                d = nextDegree (degree, n, g.motion, stasis, hrng);
                const int fifth = (degreeSemis (scale(), d + (n > 5 ? 4 : 3)) - degreeSemis (scale(), d) + 12) % 12;
                if (n <= 5 || fifth == 7 || tensionEff() >= 0.5f) break;
            }
            degree = d;
        }
        chord = buildChord (scale(), g.key, degree, g.color, tensionEff(), hrng);
        chordStart = at; chordLen = chordLength(); nextChordBeat = at + chordLen;
        // Los roles que siguen al acorde vuelven a tocar ahora (con su escalonamiento propio).
        for (int l = 0; l < kLanes; ++l)
        {
            const int r = lanes[(size_t) l].role;
            if (lanes[(size_t) l].on && ! lanes[(size_t) l].loop && (r == Pad || r == Drone || r == Bass)) st[(size_t) l].next = at;
        }
    }

    // La quinta de la escala sobre una nota (justa, o la que la escala tenga: disminuida en el grado vii).
    int fifthOf (int n) const
    {
        const auto sc = scalePcs (scale(), g.key);
        for (int d : { 7, 6, 8 }) if (std::find (sc.begin(), sc.end(), (n + d) % 12) != sc.end()) return n + d;
        return n + 7;
    }

    std::vector<int> allowed (const LaneParams& L) const
    {
        if (L.follow == FollowRoot) { std::vector<int> v { chord.rootPc }; if (chord.pcs.size() > 2) v.push_back (chord.pcs[2 % chord.pcs.size()]); return v; }
        if (L.follow == FollowScale) return scalePcs (scale(), g.key);
        return chord.pcs;
    }
    int lo (const LaneParams& L) const
    {
        const int shift = (int) std::lround ((g.brightness - 0.5f) * 14.0f);
        return std::clamp (roleCenter (L.role) + 12 * L.octave + shift - L.range / 2, 12, 115);
    }
    int hi (const LaneParams& L) const { return std::clamp (lo (L) + std::max (4, L.range), 16, g.gentle ? 88 : 120); }

    // Segundos de sesión en el pulso b (con el ancla del tempo: no depende del bloque).
    double secAt (double b) const { return (double) anchorSession / fs + (b - anchorBeat) * 60.0 / std::max (1.0, anchorBpm); }
    double breathPhaseAt (double sec) const { return g.breathRate > 0.0f ? std::fmod (std::max (0.0, sec) * g.breathRate / 60.0, 1.0) : 0.0; }
    float breathAt (double sec) const
    {
        if (g.breathRate <= 0.0f) return 0.5f;
        const double p = breathPhaseAt (sec);
        return p < 0.4 ? (float) (0.5 - 0.5 * dm::cos (p / 0.4 * dm::kPi)) : (float) (0.5 + 0.5 * dm::cos ((p - 0.4) / 0.6 * dm::kPi));
    }
    float gainAt (double sec) const
    {
        float gIn = g.fadeIn > 0.0f ? (float) std::clamp (sec / g.fadeIn, 0.0, 1.0) : 1.0f;
        float gOut = 1.0f;
        if (g.sessionMinutes > 0.0f && g.fadeOut > 0.0f)
        {
            const double left = g.sessionMinutes * 60.0 - sec;
            gOut = (float) std::clamp (left / g.fadeOut, 0.0, 1.0);
        }
        return gIn * gOut;
    }

    int velocityFor (LaneParams& L, LaneState& S, float accent = 1.0f)
    {
        const double sec = secAt (genT);
        float v = L.velocity * accent * (1.0f + L.velRand * 0.6f * (S.rng.uni() * 2.0f - 1.0f));
        v *= 0.75f + 0.5f * g.brightness;
        v *= g.level * (0.35f + 0.65f * gainAt (sec));
        if (g.breathRate > 0.0f && (L.role == Pad || L.role == Drone)) v *= 0.85f + 0.3f * breathAt (sec);
        int iv = (int) std::lround (v * 127.0f);
        if (g.gentle) iv = std::min (iv, 92);
        return std::clamp (iv, 1, 127);
    }

    // ───────── programación de notas ─────────
    void note (int lane, double at, int pitch, int vel, double dur, bool withEcho = true)
    {
        auto& L = lanes[(size_t) lane];
        if (pitch < 0 || pitch > 127 || dur <= 0.0) return;
        const double human = g.humanizeMs > 0.0f ? (st[(size_t) lane].rng.gauss() * g.humanizeMs * 0.001 * curBpm / 60.0 * 0.5) : 0.0;
        at = std::max (genT, at + human);      // nunca antes de lo que ya se generó
        const uint8_t ch = (uint8_t) (std::clamp (L.channel, 1, 16) - 1);
        const uint32_t id = nextId++;
        pending.push_back ({ at, 1, ch, (uint8_t) pitch, (uint8_t) vel, (int8_t) lane, id });
        pending.push_back ({ at + dur, 0, ch, (uint8_t) pitch, 0, (int8_t) lane, id });
        // Captura.
        const int h = captureHead.load (std::memory_order_relaxed);
        capture[(size_t) (h % (int) capture.size())] = { playedBeats + (at - blockB0), (uint8_t) (ch + 1), (uint8_t) pitch, (uint8_t) vel, (float) dur };
        captureHead.store (h + 1, std::memory_order_release);
        if (withEcho && L.echoes > 0)
        {
            float decay = 1.0f;
            for (int k = 1; k <= L.echoes; ++k)
            {
                decay *= L.echoDecay;
                const int ev = (int) std::lround ((float) vel * decay);
                if (ev < 4) break;
                const double t = at + k * L.echoBeats * mfac (g.space);
                pending.push_back ({ t, 1, ch, (uint8_t) pitch, (uint8_t) ev, (int8_t) lane, nextId });
                pending.push_back ({ t + std::min (dur, (double) L.echoBeats), 0, ch, (uint8_t) pitch, 0, (int8_t) lane, nextId++ });
                // El clip capturado lleva los ecos: es lo que se oyó.
                const int hc = captureHead.load (std::memory_order_relaxed);
                capture[(size_t) (hc % (int) capture.size())] = { playedBeats + (t - blockB0), (uint8_t) (ch + 1), (uint8_t) pitch, (uint8_t) ev, (float) std::min (dur, (double) L.echoBeats) };
                captureHead.store (hc + 1, std::memory_order_release);
            }
        }
    }

    // Una nota del rol (con lazo: graba o reproduce).
    void playOrRecord (int lane, double at, int pitch, int vel, double dur)
    {
        auto& L = lanes[(size_t) lane]; auto& S = st[(size_t) lane];
        if (L.loop && S.recording) S.loopEvents.push_back ({ at - S.cycleStart, pitch, vel, dur });
        note (lane, at, pitch, vel, dur);
    }

    void generate (int l, double t)
    {
        auto& L = lanes[(size_t) l]; auto& S = st[(size_t) l];
        genT = t;
        // Lazo encendido en marcha: empieza a grabar acá (si no, recorría todos los ciclos pasados de golpe:
        // cientos de notas en un bloque).
        if (L.loop != S.prevLoop)
        {
            S.prevLoop = L.loop;
            if (L.loop) { S.recording = true; S.loopEvents.clear(); S.loopIdx = 0; S.cycleStart = t; S.orbitStart = t; }
        }
        // Lazo en reproducción.
        if (L.loop && ! S.recording)
        {
            const double len = effLoopBeats (L);
            if (S.loopEvents.empty() || S.loopIdx >= (int) S.loopEvents.size())
            {
                S.cycleStart += len; S.loopIdx = 0;
                if (S.loopEvents.empty()) { S.recording = true; S.next = S.cycleStart; return; }
                S.next = S.cycleStart + S.loopEvents[0].off;
                return;
            }
            auto& e = S.loopEvents[(size_t) S.loopIdx++];
            if (S.rng.chance (L.mutate * mfac (g.motion) * 0.5f))
            {
                // Mutación: otra nota cercana de la escala, o silencio, o un desplazamiento chico.
                const float r = S.rng.uni();
                if (r < 0.6f) e.note = snap (e.note + S.rng.irange (-3, 3), allowed (L));
                else if (r < 0.8f) e.vel = std::max (1, e.vel / 2);
                else e.off = std::clamp (e.off + (S.rng.uni() - 0.5) * 0.5, 0.0, len - 0.01);
            }
            int pitch = snap (e.note, allowed (L));
            while (pitch > hi (L) && pitch - 12 >= 0) pitch -= 12;     // por octavas: un clamp sacaría la nota de la escala
            while (pitch < lo (L) && pitch + 12 <= 127) pitch += 12;
            if (S.rng.chance (L.probability)) note (l, S.cycleStart + e.off, pitch, std::clamp ((int) std::lround (e.vel * (0.35f + 0.65f * gainAt (secAt (t)))), 1, 127), e.dur);
            S.next = S.loopIdx < (int) S.loopEvents.size() ? S.cycleStart + S.loopEvents[(size_t) S.loopIdx].off : S.cycleStart + len;
            return;
        }
        if (L.loop && S.recording && t >= S.cycleStart + effLoopBeats (L))
        {
            S.recording = false; S.loopIdx = 0; S.cycleStart += effLoopBeats (L);
            std::stable_sort (S.loopEvents.begin(), S.loopEvents.end(), [] (const LoopEv& a, const LoopEv& b) { return a.off < b.off || (! (b.off < a.off) && a.note < b.note); });
            S.next = S.loopEvents.empty() ? S.cycleStart + effLoopBeats (L) : S.cycleStart + S.loopEvents[0].off;
            if (S.loopEvents.empty()) S.recording = true;
            return;
        }

        const float d = densityEff (L);
        const float len = L.length * mfac (g.space);
        const auto al = allowed (L);
        const int nlo = lo (L), nhi = hi (L);
        switch (L.role)
        {
            case Drone:
            {
                const double until = nextChordBeat - t + 1.0 + 3.0 * len;
                std::vector<int> ps { snap (nlo + ((chord.rootPc - nlo) % 12 + 12) % 12, { chord.rootPc }) };
                if (d > 0.35f) ps.push_back (fifthOf (ps[0]));
                if (d > 0.7f) ps.push_back (ps[0] + 12);
                for (size_t i = 0; i < ps.size(); ++i)
                    if (ps[i] <= nhi + 12 && S.rng.chance (L.probability)) playOrRecord (l, t + 0.05 * (double) i, ps[i], velocityFor (L, S, 0.9f), until);
                S.next = std::min (nextChordBeat, t + std::max (8.0, (double) L.loopBeats));
                break;
            }
            case Pad:
            {
                const int voices = std::clamp ((int) std::lround ((float) L.voices * (0.6f + 0.6f * d)), 1, 8);
                S.voicing = voiceLead (S.voicing, chord, voices, nlo, nhi, S.rng);
                const double spread = 2.5 * len;
                const double until = nextChordBeat - t + 0.5 + 2.0 * len;
                for (int p : S.voicing)
                {
                    if (! S.rng.chance (L.probability)) continue;
                    const double off = S.rng.uni() * spread;
                    playOrRecord (l, t + off, p, velocityFor (L, S, 0.8f), std::max (0.5, until - off));
                }
                S.next = nextChordBeat;
                break;
            }
            case Melody:
            {
                if (t < S.restUntil) { S.next = S.restUntil; break; }
                if (S.phraseLeft <= 0)
                {
                    S.phraseLeft = S.rng.irange (3, 3 + (int) (d * 6.0f));
                    if (g.breathRate > 0.0f)   // frases que empiezan con la exhalación
                    {
                        const double breathBeats = 60.0 / g.breathRate * curBpm / 60.0;
                        const double ph = breathPhaseAt (secAt (t));
                        const double toExhale = (ph < 0.4 ? 0.4 - ph : 1.4 - ph) * breathBeats;
                        if (toExhale > 0.25) { S.next = t + toExhale; break; }
                    }
                }
                static const double DUR[] = { 0.5, 1.0, 1.0, 1.5, 2.0, 2.0, 3.0, 4.0 };
                const int di = std::clamp ((int) ((1.0f - d) * 5.0f) + S.rng.irange (0, 2), 0, 7);
                const double step = DUR[di] * (0.6 + 0.8 * len);
                int p;
                if (S.lastNote < 0) p = snap ((nlo + nhi) / 2, al);
                else
                {
                    const float leapP = g.gentle ? 0.05f : 0.12f + 0.3f * g.motion;
                    // Un sorteo por sentencia: el orden de evaluación de los operandos de C++ no está fijado
                    // (clang y MSVC lo hacen al revés) y la misma variación tiene que sonar igual en los dos.
                    int moveSteps;
                    if (S.rng.chance (leapP)) moveSteps = S.rng.irange (-4, 4);
                    else { const int sign = S.rng.irange (-1, 1); moveSteps = sign * S.rng.irange (1, 2); }
                    const auto sc = scalePcs (scale(), g.key);
                    p = S.lastNote;
                    const int dir = moveSteps >= 0 ? 1 : -1;
                    for (int k = 0; k < std::abs (moveSteps); ++k) { p += dir; while (std::find (sc.begin(), sc.end(), ((p % 12) + 12) % 12) == sc.end()) p += dir; }
                    if (DUR[di] >= 2.0 && L.follow == FollowChord) p = snap (p, chord.pcs);   // notas largas en el acorde
                    if (p < nlo) p += 12;
                    if (p > nhi) p -= 12;
                }
                S.lastNote = p;
                const float arch = 0.85f + 0.3f * (float) dm::sin (dm::kPi * S.phraseLeft / 6.0);
                if (S.rng.chance (L.probability)) playOrRecord (l, t, p, velocityFor (L, S, arch), step * (0.7 + 0.6 * len));
                S.next = t + step;
                if (--S.phraseLeft <= 0) S.restUntil = S.next + (double) (1.0f - d) * 8.0 * mfac (g.space) + S.rng.uni() * 4.0;
                break;
            }
            case Arp:
            case Pulse:
            {
                static const double RATE[] = { 2.0, 1.0, 0.5, 0.25, 0.125 };
                const double r = RATE[std::clamp (L.rate, 0, 4)];
                const int steps = std::max (1, L.euclidSteps);
                const int idx = ((S.step + L.euclidRotate) % steps + steps) % steps;
                bool hit = true;
                if (L.euclidPulses > 0) hit = ((idx * std::min (L.euclidPulses, steps)) % steps) < std::min (L.euclidPulses, steps);
                else hit = S.rng.chance (0.25f + 0.75f * d);
                if (hit && S.rng.chance (L.probability))
                {
                    int p;
                    if (L.role == Pulse) p = snap ((nlo + nhi) / 2, { chord.rootPc, fifthOf (chord.rootPc) % 12 });
                    else
                    {
                        auto pool = notesInRange (al, nlo, nhi);
                        if (pool.empty()) pool.push_back (snap ((nlo + nhi) / 2, al));
                        const int m = (int) pool.size();
                        int i;
                        switch (L.arpPattern)
                        {
                            case ArpUp: i = S.arpIdx % m; break;
                            case ArpDown: i = m - 1 - S.arpIdx % m; break;
                            case ArpRandom: i = S.rng.irange (0, m - 1); break;
                            default: { const int c = std::max (1, 2 * m - 2); const int k = S.arpIdx % c; i = k < m ? k : c - k; break; }
                        }
                        ++S.arpIdx;
                        p = pool[(size_t) std::clamp (i, 0, m - 1)];
                    }
                    const float acc = idx == 0 ? 1.1f : 0.9f;
                    playOrRecord (l, t, p, velocityFor (L, S, acc), r * (0.3 + 1.4 * len));
                }
                ++S.step;
                S.next = t + r;
                break;
            }
            case Cloud:
            {
                const double mean = (double) (16.0f - 15.0f * d) * (g.breathRate > 0.0f ? (1.5 - breathAt (secAt (t))) : 1.0);
                const auto pool = notesInRange (al, nlo, nhi);
                if (! pool.empty() && S.rng.chance (L.probability))
                {
                    const int p = pool[(size_t) S.rng.irange (0, (int) pool.size() - 1)];
                    const float accent = 0.7f + 0.5f * S.rng.uni();       // un sorteo por sentencia (ver Melody)
                    const int vel = velocityFor (L, S, accent);
                    const double dur = 0.4 + 2.5 * len * S.rng.uni();
                    playOrRecord (l, t, p, vel, dur);
                }
                S.next = t + std::max (0.25, -dm::log (std::max (1.0e-4f, S.rng.uni())) * mean);
                break;
            }
            case Bass:
            default:
            {
                int root = nlo + ((chord.rootPc - nlo) % 12 + 12) % 12;
                if (root > nhi) root -= 12;
                const double until = nextChordBeat - t;
                const bool firstHalf = t < chordStart + 0.5;
                const int p = firstHalf || ! S.rng.chance (d - 0.3f) ? root : fifthOf (root);   // segunda mitad: a veces la quinta
                const double split = d > 0.5f && firstHalf && until > 4.0 ? until * 0.5 : until;
                if (S.rng.chance (L.probability)) playOrRecord (l, t, p, velocityFor (L, S), split * (0.5 + 0.5 * len));
                S.next = split < until ? t + split : nextChordBeat;
                break;
            }
        }
        // Grabando un lazo: el ciclo cierra a tiempo aunque la próxima nota caiga después (si no, la
        // reproducción arrancaría tarde, con un racimo de notas atrasadas).
        if (L.loop && S.recording) S.next = std::min (S.next, S.cycleStart + effLoopBeats (L));
        if (S.next <= t) S.next = t + 0.0625;   // nunca dos veces en el mismo instante
    }

    // ───────── salida ─────────
    void push (int offset, int s, int d1, int d2, int lane, int size = 3)
    {
        OutEvent e; e.offset = std::clamp (offset, 0, std::max (0, blockN - 1)); e.data[0] = (uint8_t) s; e.data[1] = (uint8_t) d1; e.data[2] = (uint8_t) d2; e.size = (uint8_t) size; e.lane = (int8_t) lane;
        out.push_back (e);
    }
    int offsetOf (double b) const { return (int) std::floor ((b - blockB0) / std::max (1e-12, blockLen) * blockN); }

    void emitPending (double b1)
    {
        // Ordena por tiempo; a igual tiempo, los note-off primero.
        std::sort (pending.begin(), pending.end(), [] (const Pending& a, const Pending& b)
                   { return a.beat < b.beat || (! (b.beat < a.beat) && (a.type < b.type || (a.type == b.type && a.id < b.id))); });
        size_t k = 0;
        for (; k < pending.size() && pending[k].beat < b1; ++k)
        {
            const auto& p = pending[k];
            const int off = offsetOf (p.beat);
            auto& id = activeId[p.ch][p.note];
            if (p.type == 1)
            {
                if (id != 0)      // re-articula: la nota anterior se corta acá (y deja de contar en su carril)
                {
                    push (off, 0x80 | p.ch, p.note, 0, p.lane);
                    const int pl = activeLane[p.ch][p.note];
                    if (pl >= 0) st[(size_t) pl].sounding = std::max (0, st[(size_t) pl].sounding - 1);
                }
                if (p.lane >= 0) st[(size_t) p.lane].sounding++;
                id = p.id; activeLane[p.ch][p.note] = p.lane; activeVel[p.ch][p.note] = (uint8_t) std::max<int> (1, p.vel);
                push (off, 0x90 | p.ch, p.note, std::max<int> (1, p.vel), p.lane);
            }
            else if (id == p.id)
            {
                id = 0;
                if (p.lane >= 0) st[(size_t) p.lane].sounding = std::max (0, st[(size_t) p.lane].sounding - 1);
                push (off, 0x80 | p.ch, p.note, 0, p.lane);
            }
        }
        pending.erase (pending.begin(), pending.begin() + (long) k);
    }

    void releaseLane (int lane, int offset)
    {
        for (int ch = 0; ch < 16; ++ch)
            for (int n = 0; n < 128; ++n)
                if (activeId[(size_t) ch][(size_t) n] != 0 && activeLane[(size_t) ch][(size_t) n] == lane) { activeId[(size_t) ch][(size_t) n] = 0; push (offset, 0x80 | ch, n, 0, lane); }
        pending.erase (std::remove_if (pending.begin(), pending.end(), [lane] (const Pending& p) { return p.lane == lane; }), pending.end());
        st[(size_t) lane].sounding = 0;
    }
    void allNotesOff (int offset)
    {
        for (int ch = 0; ch < 16; ++ch)
            for (int n = 0; n < 128; ++n)
                if (activeId[(size_t) ch][(size_t) n] != 0) { activeId[(size_t) ch][(size_t) n] = 0; push (offset, 0x80 | ch, n, 0, activeLane[(size_t) ch][(size_t) n]); }
        pending.clear();
        for (auto& S : st) S.sounding = 0;
    }

    // CC por carril, deriva y afinación por pitch bend, volumen CC7 de los fades. En una grilla fija de
    // 30 pasos por segundo de sesión (CC hasta 30/s, bend 15/s, CC7 10/s), con su propio aleatorio:
    // los controles no dependen del tamaño de bloque ni corren la secuencia de las notas.
    void controlOutputs (int64_t s0, int n)
    {
        const int64_t F = std::max<int64_t> (1, (int64_t) std::llround (fs));
        auto tickSample = [F] (int64_t k) { return k * F / 30; };
        int64_t k = s0 * 30 / F;
        while (tickSample (k) < s0) ++k;
        while (k > 0 && tickSample (k - 1) >= s0) --k;
        const double tuneCents = g.tuneByBend ? 1200.0 * dm::log2 (g.refPitch / 440.0) : 0.0;
        for (; tickSample (k) < s0 + n; ++k)
        {
            const int off = (int) (tickSample (k) - s0);
            const double b = blockB0 + blockLen * off / std::max (1, blockN);
            const double sec = (double) tickSample (k) / fs;
            emitPending (b);      // las notas anteriores salen primero (el CC "swell" mira cuántas suenan)
            for (int l = 0; l < kLanes; ++l)
            {
                auto& L = lanes[(size_t) l]; auto& S = st[(size_t) l];
                if (! L.on) continue;
                const int ch = std::clamp (L.channel, 1, 16) - 1;
                if (S.lastCh >= 0 && S.lastCh != ch)      // cambió el canal: el viejo vuelve al centro, el nuevo recibe todo
                {
                    if (S.lastBend >= 0 && S.lastBend != 8192) push (off, 0xE0 | S.lastCh, 0, 64, l);
                    if (S.lastCc7 >= 0 && S.lastCc7 != 100) push (off, 0xB0 | S.lastCh, 7, 100, l);
                    S.lastBend = S.lastCc = S.lastCc7 = -1;
                }
                S.lastCh = ch;
                if (L.ccNum >= 0 && L.ccNum <= 119)      // 120-127 son mensajes de modo de canal (All Notes Off, Mono...)
                {
                    float v;
                    switch (L.ccShape)
                    {
                        case CcDrift: S.ccWalk = std::clamp (S.ccWalk + S.crng.gauss() * 0.02f * mfac (g.motion), 0.0f, 1.0f); v = S.ccWalk * 2.0f - 1.0f; break;
                        case CcBreath: v = breathAt (sec) * 2.0f - 1.0f; break;
                        case CcSwell: S.swell += ((S.sounding > 0 ? 1.0f : 0.0f) - S.swell) * 0.05f; v = S.swell * 2.0f - 1.0f; break;
                        default: { const double ph = std::fmod (b / std::max (1.0f, L.ccBeats), 1.0); v = (float) dm::sin (ph * 2.0 * dm::kPi); break; }
                    }
                    const int iv = std::clamp ((int) std::lround ((L.ccCenter + 0.5f * L.ccDepth * v) * 127.0f), 0, 127);
                    view.ccValue[(size_t) l] = (float) iv / 127.0f;
                    if (iv != S.lastCc) { push (off, 0xB0 | ch, L.ccNum, iv, l); S.lastCc = iv; }
                }
                // Pitch bend: afinación de referencia + deriva lenta (sintes analógicos).
                if ((L.driftCents > 0.0f || g.tuneByBend) && k % 2 == 0)
                {
                    if (S.crng.chance (0.027f)) S.driftTarget = S.crng.gauss() * 0.5f;
                    S.drift += (S.driftTarget - S.drift) * 0.027f;
                    const double cents = tuneCents + (double) L.driftCents * S.drift;
                    const int bend = std::clamp ((int) std::lround (8192.0 + cents / (100.0 * std::max (1, g.bendRange)) * 8191.0), 0, 16383);
                    if (std::abs (bend - S.lastBend) >= 2) { push (off, 0xE0 | ch, bend & 0x7F, (bend >> 7) & 0x7F, l); S.lastBend = bend; }
                }
                else if (! (L.driftCents > 0.0f || g.tuneByBend) && S.lastBend >= 0 && S.lastBend != 8192)
                {
                    push (off, 0xE0 | ch, 0, 64, l); S.lastBend = 8192;     // deriva y afinación apagadas: el bend vuelve al centro
                }
                // Volumen del canal con el fade de la sesión.
                if (g.fadeCC7 && (g.fadeIn > 0.0f || g.sessionMinutes > 0.0f) && k % 3 == 0)
                {
                    const int v = std::clamp ((int) std::lround (100.0f * gainAt (sec)), 0, 127);
                    if (v != S.lastCc7) { push (off, 0xB0 | ch, 7, v, l); S.lastCc7 = v; }
                }
            }
        }
    }
};

} // namespace dh::orr
