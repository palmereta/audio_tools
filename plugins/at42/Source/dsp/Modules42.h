#pragma once
// Fuentes de CV del AT42: LFO A/B (triángulo ↔ cuadrada, unipolares), joystick, secuenciador de 5
// pasos con su PULSER, preamp con seguidor de envolvente, y el teclado táctil de 12 placas (escalas,
// arpegiador, secuenciador de 16 pasos, portamento, vibrato y salida de presión).

#include "Common.h"
#include "Drone42.h"
#include <array>
#include <vector>

namespace dh {

// LFO: triángulo con comparador; WAVE mezcla triángulo y cuadrada. Salida 0..10 V.
struct Lfo42
{
    float phase = 0.0f, out = 0.0f;
    inline float process (float hz, float wave, float fs)
    {
        phase += hz / fs;
        if (phase >= 1.0f) phase -= std::floor (phase);
        const float tri = phase < 0.5f ? 2.0f * phase : 2.0f - 2.0f * phase;
        const float sq = phase < 0.5f ? 1.0f : 0.0f;
        out = 10.0f * (tri + (sq - tri) * clampf (wave, 0.0f, 1.0f));
        return out;
    }
    // Sincronía con el host: fase derivada de la posición (ppq) y el largo del ciclo en negras.
    inline float processSynced (double ppq, double beats, float wave)
    {
        const double ph = ppq / std::max (1.0e-3, beats);
        phase = (float) (ph - std::floor (ph));
        return process (0.0f, wave, 1.0f);
    }
};

struct Seq5
{
    int step = 0;
    bool lastClock = false;
    float cv = 0.0f, gate = 0.0f;
    // clockV: reloj (≥ 2.5 V = alto). steps 0..1 → 0..5 V.
    inline void process (float clockV, int stages, const std::array<float, 5>& steps, const std::array<bool, 5>& gates)
    {
        const bool c = clockV > 2.5f;
        if (c && ! lastClock) step = (step + 1) % std::clamp (stages, 1, 5);
        lastClock = c;
        cv = 5.0f * clampf (steps[(size_t) step], 0.0f, 1.0f);
        gate = (c && gates[(size_t) step]) ? 10.0f : 0.0f;
    }
};

struct EnvFollower
{
    float env = 0.0f;
    bool gate = false;
    inline void process (float x, float att, float rls, float fs)
    {
        const float a = std::fabs (x);
        env += (a - env) * (a > env ? coefFor (expMap (att, 0.0005f, 0.5f), fs) : coefFor (expMap (rls, 0.005f, 3.0f), fs));
        if (! gate && env > 0.08f) gate = true;
        else if (gate && env < 0.04f) gate = false;
    }
};

// Escalas del cuantizador del teclado (semitonos dentro de la octava).
struct Scale42 { const char* name; std::vector<int> notes; };
inline const std::vector<Scale42>& scales42()
{
    static const std::vector<Scale42> s = {
        { "Semitones", { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } }, { "Ionian", { 0, 2, 4, 5, 7, 9, 11 } },
        { "Dorian", { 0, 2, 3, 5, 7, 9, 10 } }, { "Phrygian", { 0, 1, 3, 5, 7, 8, 10 } }, { "Lydian", { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian", { 0, 2, 4, 5, 7, 9, 10 } }, { "Aeolian", { 0, 2, 3, 5, 7, 8, 10 } }, { "Locrian", { 0, 1, 3, 5, 6, 8, 10 } },
        { "Blues major", { 0, 2, 3, 4, 7, 9 } }, { "Blues minor", { 0, 3, 5, 6, 7, 10 } }, { "Pentatonic major", { 0, 2, 4, 7, 9 } },
        { "Pentatonic minor", { 0, 3, 5, 7, 10 } }, { "Folk", { 0, 1, 3, 4, 5, 7, 8, 10 } }, { "Japanese", { 0, 1, 5, 7, 8 } },
        { "Gamelan", { 0, 1, 3, 7, 8 } }, { "Gypsy", { 0, 2, 3, 6, 7, 8, 11 } }, { "Arabian", { 0, 1, 4, 5, 7, 8, 11 } },
        { "Flamenco", { 0, 1, 4, 5, 7, 8, 10 } }, { "Whole tone", { 0, 2, 4, 6, 8, 10 } },
    };
    return s;
}

// Teclado táctil: 12 placas (o notas MIDI) → V/oct, gates y presión.
class TouchKeys
{
public:
    enum Mode { Keyboard = 0, Arpeggiator, Sequencer };
    enum Dir { Forward = 0, Backward, PingPong, Random, Order };
    enum Press { Pressure = 0, Asr, Ad, Loop, RandomPress };

    // Ajustes (los escribe el procesador).
    int mode = Keyboard, scale = 0, root = 0, octave = 2, dir = Forward, pressMode = Pressure, seqLen = 16;
    bool arpHold = false, legato = false;
    float porta = 0.0f, vibDepth = 0.0f, vibSpeed = 0.4f, rise = 0.3f, fall = 0.4f;
    float bpm = 120.0f; int clockDiv = 2;
    bool prevArpHold = false;       // 0 = 1/4, 1 = 1/8, 2 = 1/16, 3 = 1/32
    bool recording = false;
    float bend = 0.0f;                           // pitch bend MIDI, en voltios (1 V/oct)

    // Salidas.
    float voct = 0.0f, gateL = 0.0f, gateR = 0.0f, pressure = 0.0f;
    int activePlate = -1, seqPos = -1;
    std::array<int, 16> seqNotes {};             // notas grabadas (semitonos desde la base), −1 = silencio
    int seqCount = 0;

    TouchKeys() { held.reserve (32); order.reserve (32); sorted.reserve (32); seqNotes.fill (-1); }

    // Nota de una placa (0..11) según la escala: la placa 0 es la raíz.
    int plateNote (int plate) const
    {
        const auto& sc = scales42()[(size_t) std::clamp (scale, 0, (int) scales42().size() - 1)].notes;
        const int n = (int) sc.size();
        return root + 12 * (plate / n) + sc[(size_t) (plate % n)];
    }

    // noteSemis: semitonos desde la base (C del rango); vel 0..1; right: mitad derecha (gate R).
    void press (int noteSemis, float vel, bool right, int plate = -1)
    {
        held.erase (std::remove_if (held.begin(), held.end(), [&] (auto& h) { return h.note == noteSemis; }), held.end());
        held.push_back ({ noteSemis, vel, right });
        if (std::find (order.begin(), order.end(), noteSemis) == order.end()) order.push_back (noteSemis);
        if (arpHold && arpReleased) { order.clear(); order.push_back (noteSemis); arpReleased = false; }
        activePlate = plate;
        if (recording && seqCount < 16) seqNotes[(size_t) seqCount++] = noteSemis;
        if (mode == Keyboard) { target = noteSemis; trig = true; }
        if (mode != Keyboard && held.size() == 1 && clockFree) { clockPhase = 0.999; }
        pressVel = vel;
    }

    void release (int noteSemis)
    {
        held.erase (std::remove_if (held.begin(), held.end(), [&] (auto& h) { return h.note == noteSemis; }), held.end());
        if (! arpHold) order.erase (std::remove (order.begin(), order.end(), noteSemis), order.end());
        if (held.empty()) { arpReleased = true; activePlate = -1; }
        if (mode == Keyboard && ! held.empty()) { target = held.back().note; trig = legato ? false : true; }
    }

    void clear() { held.clear(); order.clear(); }

    // Vuelve al estado de recién creado lo que corre (reloj, arpegio, portamento, vibrato, presión, azar), sin tocar
    // los ajustes ni la secuencia grabada: prepare() del motor (QA en DAWs 2026-10-22: una instancia preparada de
    // nuevo después de tocar no sonaba igual que una nueva).
    void resetRuntime()
    {
        clear();
        voct = gateL = gateR = pressure = 0.0f; activePlate = -1; seqPos = -1; prevArpHold = arpHold;
        target = 0; arpPos = -1; arpDir = 1; stepGateTimer = 0; retrigGap = 0; extSeen = 1 << 30; lastIdx = -1;
        clockPhase = 0.0; glide = 0.0f; vibPhase = 0.0f; pressVel = 0.8f; randTarget = 0.5f;
        trig = lastExt = lastReset = arpReleased = adUp = lastGatePress = false; clockFree = true;
        rng = Rng { 42 };
    }

    // clockIn: entrada de reloj externa (V); resetIn (V); ppqClock: si ≥ 0, posición del host.
    inline void process (float clockIn, float resetIn, double ppq, bool hostClock, float hostBpm, float fs)
    {
        // HOLD apagado: las notas retenidas que ya no están apretadas salen del arpegio.
        if (prevArpHold && ! arpHold)
        {
            order.clear();
            for (auto& h : held) if (std::find (order.begin(), order.end(), h.note) == order.end()) order.push_back (h.note);
            if (held.empty()) arpReleased = true;
        }
        prevArpHold = arpHold;
        const float gateBpm = hostClock ? std::max (1.0f, hostBpm) : bpm;     // largo del gate al tempo que manda
        // Reloj: externo si hay flancos en la entrada; si no, el interno (o el del host).
        bool tick = false;
        const bool ext = clockIn > 1.5f;
        if (ext && ! lastExt) { tick = true; extSeen = 0; }
        lastExt = ext;
        if (extSeen < (int) (fs * 2.0f)) ++extSeen;
        const bool usingExt = extSeen < (int) (fs * 2.0f);
        const double beats = 1.0 / (double) (1 << std::clamp (clockDiv, 0, 3));
        if (! usingExt)
        {
            if (hostClock)
            {
                const long long idx = (long long) std::floor (ppq / beats);
                if (idx != lastIdx) { tick = lastIdx >= 0; lastIdx = idx; }
                clockFree = false;
            }
            else
            {
                clockPhase += (double) bpm / 60.0 / beats / fs;
                if (clockPhase >= 1.0) { clockPhase -= std::floor (clockPhase); tick = true; }
                clockFree = true;
            }
        }
        const bool rst = resetIn > 1.5f;
        if (rst && ! lastReset) { arpPos = -1; seqPos = -1; }
        lastReset = rst;

        stepGateTimer = std::max (0, stepGateTimer - 1);
        const bool any = ! held.empty() || (arpHold && ! order.empty());
        if (tick)
        {
            if (mode == Arpeggiator && any) { target = nextArp(); trig = true; stepGateTimer = (int) (fs * 0.5 * 60.0 / gateBpm * beats); }
            else if (mode == Sequencer && seqCount > 0 && (any || ! recording))
            {
                seqPos = (seqPos + 1) % std::clamp (seqCount, 1, std::max (2, seqLen));
                const int n = seqNotes[(size_t) seqPos];
                const int transpose = held.empty() ? 0 : held.back().note - seqNotes[0];
                if (n >= 0) { target = n + transpose; trig = true; stepGateTimer = (int) (fs * 0.5 * 60.0 / gateBpm * beats); }
            }
        }

        // Portamento (constante de tiempo) y vibrato con retardo.
        const float tgt = (float) (target + 12 * (octave - 2)) / 12.0f;
        const float pc = porta <= 0.0f ? 1.0f : coefFor (porta * porta * 1.2f, fs);
        glide += (tgt - glide) * pc;
        vibPhase += (0.5f + vibSpeed * 9.0f) / fs; if (vibPhase >= 1.0f) vibPhase -= 1.0f;
        const float vib = std::sin (6.2831853f * vibPhase) * vibDepth * 0.08f;
        voct = glide + vib + bend;                                // 0 V = C3 (VCO en 8'), octava 2 = sin transponer

        bool gate;
        if (mode == Keyboard) gate = ! held.empty();
        else gate = stepGateTimer > 0 && any;
        if (trig && mode == Keyboard && ! legato) retrigGap = (int) (fs * 0.002f);
        trig = false;
        if (retrigGap > 0) { --retrigGap; gate = false; }
        gateL = gate ? 10.0f : 0.0f;
        bool right = false;
        for (auto& h : held) right = right || h.right;
        gateR = right ? 10.0f : 0.0f;

        // Presión: directa (velocidad/aftertouch) o envolventes ASR, AD, AD en lazo, aleatoria.
        const float rc = coefFor (0.002f + rise * rise * 3.0f, fs), fc = coefFor (0.002f + fall * fall * 3.0f, fs);
        float pt = 0.0f;
        switch (pressMode)
        {
            case Pressure: pt = gate ? pressVel : 0.0f; break;
            case Asr: pt = gate ? 1.0f : 0.0f; break;
            case Ad: if (gate && ! lastGatePress) adUp = true; pt = adUp ? 1.0f : 0.0f; if (pressure > 0.98f) adUp = false; break;
            case Loop: if (gate) { if (pressure > 0.98f) adUp = false; else if (pressure < 0.02f) adUp = true; pt = adUp ? 1.0f : 0.0f; } break;
            default: if (gate && ! lastGatePress) randTarget = rng.unipolar(); pt = gate ? randTarget : 0.0f; break;
        }
        lastGatePress = gate;
        pressure += (pt - pressure) * (pt > pressure ? rc : fc);
    }

    float pressureVolts() const { return 8.0f * pressure; }

private:
    struct Held { int note; float vel; bool right; };
    std::vector<Held> held;
    std::vector<int> order, sorted;
    int target = 0, arpPos = -1, arpDir = 1, stepGateTimer = 0, retrigGap = 0, extSeen = 1 << 30;
    long long lastIdx = -1;
    double clockPhase = 0.0;
    float glide = 0.0f, vibPhase = 0.0f, pressVel = 0.8f, randTarget = 0.5f;
    bool trig = false, lastExt = false, lastReset = false, arpReleased = false, clockFree = true, adUp = false, lastGatePress = false;
    Rng rng { 42 };

    int nextArp()
    {
        if (order.empty()) return target;
        sorted.assign (order.begin(), order.end());
        if (dir != Order) std::sort (sorted.begin(), sorted.end());
        const int n = (int) sorted.size();
        switch (dir)
        {
            case Backward: arpPos = arpPos <= 0 ? n - 1 : arpPos - 1; break;
            case PingPong:
                if (n == 1) arpPos = 0;
                else { arpPos += arpDir; if (arpPos >= n) { arpPos = n - 2; arpDir = -1; } else if (arpPos < 0) { arpPos = 1; arpDir = 1; } }
                break;
            case Random: arpPos = (int) (rng.unipolar() * (float) n) % n; break;
            default: arpPos = (arpPos + 1) % n; break;
        }
        return sorted[(size_t) std::clamp (arpPos, 0, n - 1)];
    }
};

} // namespace dh
