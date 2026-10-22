#pragma once
// El guitarrista (docs/research.md §5, §6, §7): convierte el MIDI que entra en lo que hacen las dos manos y lo deja en
// una cola de eventos con su muestra exacta; el motor los ejecuta sobre las cuerdas y los repite por la salida MIDI.
//
// Modos: melodía (polifónica o monofónica con ligado inteligente: un solapamiento en la misma cuerda es hammer-on,
// pull-off o slide), acordes (las teclas se juntan durante una ventana corta, se reconoce el acorde, se busca el voicing
// y se rasguea), rasgueo con patrón (sincronizado al host, o a un reloj propio que arranca con la primera tecla) y
// arpegiador de dedos (patrones por cuerda: bajo alternado, rolls, p-i-m-a).
// Articulaciones por keyswitch (latch), por velocity (muertas abajo, pinch arriba), por CC (palm mute, palanca, ligado)
// y automáticas (vibrato demorado, púa alternada, ligado por solapamiento). MPE: el canal de cada nota lleva su bend y
// su presión a la cuerda que la toca; en el modo de guitarra MIDI cada canal es una cuerda.
// Variación humana: microafinación, tiempo, fuerza y púa salen de un hash del número de evento (determinista).
//
// Exactitud entre bloques: el patrón se recorre por ventanas de ppq encadenadas (la ventana nueva empieza donde terminó
// la anterior aunque el host deje una rendija de redondeo) con 60 ms de anticipación, así cada paso se agenda una vez y
// en la muestra absoluta que le toca, con cualquier tamaño de bloque (la lección T2b del AT303).

#include "Instrument.h"
#include "Fingering.h"

namespace dh::gt {

// ── MIDI crudo ──
struct MidiMsg { int offset = 0; uint8_t status = 0, d1 = 0, d2 = 0; };
// looping, loopStart, loopEnd: el loop del host (ppq), si lo informa: el patrón sigue del final al principio sin saltear
// ni repetir golpes (QA en REAPER 2026-10-22, G1).
struct HostTime { bool playing = false; double ppq = 0.0, bpm = 120.0; bool looping = false; double loopStart = 0.0, loopEnd = 0.0; };

// ── Patrones ──
// Rasgueo, un carácter por paso: D/U abajo/arriba, d/u sólo las cuerdas agudas, M/m con la palma, X/x muerto (la mano
// izquierda apoyada: "chk"), B bajo, A bajo alternado, S abajo y cortado (staccato), | cortar todo, - o . nada.
// Acentos, un dígito por paso (0 normal, 1 acento, 2 fuerte).
struct StrumStep { char type = '.'; uint8_t accent = 0; };
enum Division { kDiv8, kDiv16, kDiv16T, kDiv8T, kDiv4, kNumDivs };
inline double divisionBeats (int d) { static const double b[] = { 0.5, 0.25, 1.0 / 6.0, 1.0 / 3.0, 1.0 }; return b[std::clamp (d, 0, (int) kNumDivs - 1)]; }
inline const char* divisionName (int d) { static const char* n[] = { "1/8", "1/16", "1/16T", "1/8T", "1/4" }; return n[std::clamp (d, 0, (int) kNumDivs - 1)]; }
inline bool divisionSwings (int d) { return d == kDiv8 || d == kDiv16; }

struct StrumPattern
{
    static constexpr int kMax = 64;
    std::array<StrumStep, kMax> steps {};
    int len = 8, div = kDiv8;
    float swing = 0.5f;              // 0,5 derecho … 0,75 puntillo
};
inline StrumPattern parseStrum (const char* steps, const char* accents, int div, float swing)
{
    StrumPattern p; p.div = div; p.swing = swing; p.len = 0;
    for (int i = 0; steps[i] && p.len < StrumPattern::kMax; ++i)
    {
        if (steps[i] == ' ') continue;
        p.steps[(size_t) p.len].type = steps[i];
        p.steps[(size_t) p.len].accent = 0;
        ++p.len;
    }
    int k = 0;
    for (int i = 0; accents && accents[i] && k < p.len; ++i) { if (accents[i] == ' ') continue; p.steps[(size_t) k++].accent = (uint8_t) (accents[i] >= '0' && accents[i] <= '2' ? accents[i] - '0' : 0); }
    if (p.len == 0) { p.len = 1; p.steps[0].type = 'D'; }
    return p;
}

// Arpegio, un grupo por paso separado por espacios: T bajo, A bajo alternado, 1 la cuerda más aguda del voicing, 2 la
// siguiente, 3, 4; "T1" pinza bajo y aguda juntos; "." silencio. Acentos con ">" delante del grupo.
struct ArpStep { uint8_t mask = 0, accent = 0; };
struct ArpPattern
{
    static constexpr int kMax = 64;
    std::array<ArpStep, kMax> steps {};
    int len = 8, div = kDiv8;
    float swing = 0.5f;
};
inline ArpPattern parseArp (const char* text, int div, float swing)
{
    ArpPattern p; p.div = div; p.swing = swing; p.len = 0;
    ArpStep cur; bool any = false;
    auto flush = [&] { if (any && p.len < ArpPattern::kMax) p.steps[(size_t) p.len++] = cur; cur = ArpStep(); any = false; };
    for (int i = 0; ; ++i)
    {
        const char c = text[i];
        if (c == 0 || c == ' ') { flush(); if (c == 0) break; continue; }
        any = true;
        switch (c)
        {
            case 'T': cur.mask |= 1; break;
            case 'A': cur.mask |= 2; break;
            case '1': cur.mask |= 4; break;
            case '2': cur.mask |= 8; break;
            case '3': cur.mask |= 16; break;
            case '4': cur.mask |= 32; break;
            case '>': cur.accent = 1; break;
            default: break;          // '.' = silencio
        }
    }
    if (p.len == 0) { p.len = 1; p.steps[0].mask = 1; }
    return p;
}

// ── Parámetros ──
enum PlayMode { kModePoly, kModeMono, kModeChords, kModeStrum, kModeArp, kNumModes };
inline const char* modeName (int m) { static const char* n[] = { "Melody (poly)", "Melody (mono)", "Chords", "Strum pattern", "Fingerpicking" }; return n[std::clamp (m, 0, (int) kNumModes - 1)]; }
enum Art { kArtSustain, kArtPalm, kArtStaccato, kArtDead, kArtHarmonic, kArtPinch, kArtTremolo, kArtBendIn, kArtSlideIn, kArtSlideOut, kNumArts };
inline const char* artName (int a) { static const char* n[] = { "Sustain", "Palm mute", "Staccato", "Dead notes", "Harmonics", "Pinch harmonics", "Tremolo picking", "Bend in", "Slide in", "Slide out" }; return n[std::clamp (a, 0, (int) kNumArts - 1)]; }
enum LegatoMode { kLegatoOff, kLegatoHammer, kLegatoSlide, kLegatoAuto };
enum MidiInMode { kMidiNormal, kMidiPerString, kMidiMpe };

struct PerformParams
{
    int mode = kModePoly;
    FingerPrefs prefs;
    // Mano derecha.
    float pickPosMm = 95.0f, pickHard = 0.6f, pickAngle = 0.35f, attackNoise = 0.35f;
    bool fingers = false;
    // Articulaciones.
    int art = kArtSustain;
    float palm = 0.65f;                      // grado del palm mute de la articulación
    int legato = kLegatoAuto; float legatoFrets = 4.0f;
    float slideTime = 0.09f;                 // s, slides de ligado y de entrada
    bool autoVibrato = true; float vibDelay = 0.45f, vibRate = 5.6f, vibDepth = 0.35f;   // semitonos (de un lado)
    float bendRange = 2.0f; bool bendAll = false;
    float bendInAmount = 2.0f, bendInTime = 0.18f;
    int tremoloDiv = kDiv16;
    float feedback = 0.0f; int feedbackHarm = 1;
    float whammyRange = 12.0f;
    int deadBelow = 0, pinchAbove = 0;       // velocity (0 = apagado)
    float staccatoMs = 110.0f;
    // Humano.
    float humanTune = 0.3f, humanTime = 0.3f, humanVel = 0.3f, humanPick = 0.5f;
    float releaseNoise = 0.4f, squeak = 0.5f;
    // Acordes y patrones.
    int style = kAuto, region = 0, singleKey = 0;
    float strumMs = 28.0f; int upStrings = 4; float chordWindowMs = 25.0f;
    bool strumCentred = true;                // el medio del rasgueo cae en el tiempo (si no, la primera cuerda)
    bool latch = false, letRing = false;
    float idleMute = 1.2f;                   // s sin teclas: la mano se apoya y apaga lo que quede sonando
    float tempo = 110.0f;
    // MIDI.
    int midiIn = kMidiNormal; float stringBendRange = 12.0f, mpeBendRange = 48.0f;
    int keyswitchBase = 12;
    bool midiOut = false, midiOutPerString = false; int midiOutChannel = 1;
    uint32_t seed = 1;
};

// ── Salida MIDI ──
struct MidiOutEv { int64_t t = 0; uint8_t status = 0, d1 = 0, d2 = 0; };

class Performer
{
public:
    static constexpr int kCtl = 32;               // control (vibrato, bends, trémolo) cada 32 muestras absolutas
    static constexpr int kQueue = 1024;

    void prepare (float sampleRate, Instrument& inst)
    {
        fs = sampleRate; ins = &inst;
        reset();
    }
    void reset()
    {
        nq = 0; evCount = 0; batchN = 0;
        for (auto& s : st) s = StrState();
        held.clear(); keysDown = 0;
        chordPending = -1; curChord = Chord(); curVoicing = Voicing(); prevVoicing = Voicing();
        win0 = -1.0; seqRunning = false; seqStartT = 0; lastPickT = -1000000; lastDir = 1;
        sustainPedal = false; wheel = 0; modWheel = 0; palmCC = -1; whammy = 0; legatoPedal = -1; momentary = 0;
        for (auto& c : chBend) c = 0; for (auto& c : chPress) c = 0; for (auto& c : chTimbre) c = -1;
        fing.reset(); tremT = 0;
        outEvents.clear();
    }
    // Cambiar de modo suelta todo (las teclas de un modo no significan lo mismo en otro).
    void set (const PerformParams& p)
    {
        const bool modeChanged = p.mode != P.mode;
        P = p;
        if (modeChanged && ins != nullptr) allNotesOff (lastNow, false);
    }
    const PerformParams& params() const { return P; }
    void refreshTab()
    {
        Tab t; t.n = ins->strings(); t.frets = ins->frets();
        for (int s = 0; s < t.n; ++s) t.open[(size_t) s] = ins->openNote (s);
        if (t.n != tab.n || t.open != tab.open || t.frets != tab.frets)
        {
            // Otra guitarra u otra afinación: lo que sonaba se apaga (y sus notas de salida se cierran).
            for (int s = 0; s < kMaxStrings; ++s)
            {
                auto& S = st[(size_t) s];
                if (S.outNote >= 0) emitOff (s, lastNow);
                S.note = -1; S.key = -1; S.tremolo = false; S.autoOffT = 0;
            }
            nq = 0; batchN = 0; held.clear(); keysDown = 0; chordPending = -1;
            tab = t; fing.setTab (t); voicingDirty = true;
        }
    }

    // ── Entrada ──
    void midi (const MidiMsg& m, int64_t now)
    {
        const int type = m.status & 0xF0, ch = (m.status & 0x0F) + 1;
        if (type == 0x90 && m.d2 > 0) noteOn (m.d1, m.d2, ch, now);
        else if (type == 0x80 || (type == 0x90 && m.d2 == 0)) noteOff (m.d1, ch, now);
        else if (type == 0xE0) pitchBend (ch, ((int) m.d2 << 7 | m.d1) - 8192, now);
        else if (type == 0xD0) { chPress[(size_t) (ch - 1)] = m.d1 / 127.0f; if (P.midiIn == kMidiNormal) chPress[0] = m.d1 / 127.0f; }
        else if (type == 0xA0) { const int s = stringOfKey (m.d1, ch); if (s >= 0) st[(size_t) s].pressure = m.d2 / 127.0f; }
        else if (type == 0xB0) cc (m.d1, m.d2, ch, now);
    }

    // Una vez por bloque, antes de procesarlo: agenda los pasos del patrón que caen en [ventana, fin + anticipación).
    void beginBlock (const HostTime& h, int n, int64_t absStart)
    {
        blockStart = absStart; blockLen = n;
        const bool pattern = P.mode == kModeStrum || P.mode == kModeArp;
        if (h.playing)
        {
            if (! hostWasPlaying) win0 = -1.0;
            hostWasPlaying = true; seqRunning = false;
            lastBpm = h.bpm;
            const bool loop = h.looping && h.loopEnd > h.loopStart + 1e-6 && h.ppq < h.loopEnd + 1e-9;
            if (pattern) schedule (h.ppq, h.bpm, absStart, n, loop ? h.loopStart : 0.0, loop ? h.loopEnd : -1.0);
            return;
        }
        // Sin transporte: un reloj propio que arranca en la muestra de la primera tecla (noteOn) y sigue mientras haya
        // teclas apretadas (o el latch).
        if (hostWasPlaying) { hostWasPlaying = false; win0 = -1.0; }
        lastBpm = P.tempo;
        if (pattern && seqRunning)
        {
            const double spb = fs * 60.0 / std::max (20.0, (double) P.tempo);
            schedule ((double) (absStart - seqStartT) / spb, P.tempo, absStart, n);
        }
    }

    // Agenda los pasos que caen en [ventana anterior, fin del bloque + anticipación) en ppq; la ventana nueva empieza
    // donde terminó la anterior si la posición es continua (la rendija de redondeo de un host no pierde ni repite pasos).
    // Con el loop del host (loopEnd > loopStart), lo que la ventana pasa del final del loop se agenda desde su principio,
    // así la vuelta siguiente sigue la ventana sin repetir ni perder el primer golpe (el rasgueo centrado se resuelve
    // antes del tiempo, todavía en la vuelta anterior). Un salto (otra posición, un loop que el host no informa): los
    // pasos agendados para la posición vieja que todavía no se resolvieron se descartan y la ventana arranca acá.
    void schedule (double ppq0, double bpm, int64_t absStart, int n, double loopStart = 0.0, double loopEnd = -1.0)
    {
        const double spb = fs * 60.0 / std::max (20.0, bpm);           // muestras por negra
        const double ppq1 = ppq0 + (double) n / spb;
        const double ahead = 0.06 * fs / spb;                           // 60 ms en negras
        double w0 = ppq0;
        const double expected = win0 - ahead;                           // donde tendría que empezar este bloque
        if (win0 >= 0.0 && std::fabs (ppq0 - expected) < 0.002 * bpm / 60.0) w0 = win0;      // continuo (±2 ms)
        else if (win0 >= 0.0) dropSteps (absStart);
        double w1 = ppq1 + ahead;
        if (loopEnd > loopStart && w1 > loopEnd && w0 < loopEnd)
        {
            // Hasta el final del loop acá; el resto, desde el principio, en las muestras que siguen al final.
            const int64_t wrapAt = absStart + (int64_t) std::llround ((loopEnd - ppq0) * spb);
            const double rest = std::min (w1 - loopEnd, loopEnd - loopStart);
            scheduleRange (w0, loopEnd, ppq0, absStart, spb);
            scheduleRange (loopStart, loopStart + rest, loopStart, wrapAt, spb);
            win0 = loopStart + rest;
            return;
        }
        if (loopEnd > loopStart && w0 >= loopEnd) w0 = w1;              // (ya agendado desde el principio del loop)
        win0 = w1;
        if (w1 > w0) scheduleRange (w0, w1, ppq0, absStart, spb);
    }
    // Saca de la cola los pasos del patrón que todavía no se resolvieron (de la posición vieja, antes de un salto).
    void dropSteps (int64_t from)
    {
        int w = 0;
        for (int r = 0; r < nq; ++r) if (! (q[(size_t) r].kind == Ev::step && q[(size_t) r].s == -1 && q[(size_t) r].t >= from)) q[(size_t) w++] = q[(size_t) r];
        nq = w;
        lastStepT = -1; replayDone = true;
    }
    // Los pasos con tiempo en [w0, w1) (ppq); ppqAt es el ppq de la muestra absAt.
    void scheduleRange (double w0, double w1, double ppqAt, int64_t absAt, double spb)
    {
        const double ppq0 = ppqAt; const int64_t absStart = absAt;
        const double stepB = P.mode == kModeStrum ? divisionBeats (strum.div) : divisionBeats (arp.div);
        const int len = P.mode == kModeStrum ? strum.len : arp.len;
        const float swing = P.mode == kModeStrum ? strum.swing : arp.swing;
        const bool swings = divisionSwings (P.mode == kModeStrum ? strum.div : arp.div);
        const int64_t k0 = (int64_t) std::ceil (w0 / stepB - 1e-9), k1 = (int64_t) std::ceil (w1 / stepB - 1e-9);
        // Cada paso entra a la cola como un evento propio y se resuelve (acorde, voicing, cuerdas) recién cuando le toca:
        // así lo que suena depende sólo de la línea de tiempo, no de dónde cayó el borde del bloque.
        // Rasgueo centrado: el paso se resuelve antes del tiempo, lo más que puede durar medio rasgueo (el rasgueo real se
        // calcula al resolverlo y empieza en tiempo − la mitad de su ancho). Menos de la anticipación de 60 ms.
        const int64_t lead = P.mode == kModeStrum && P.strumCentred ? (int64_t) (std::min (45.0f, 0.5f * 1.3f * P.strumMs * 1.15f) * 1e-3f * fs) : 0;
        for (int64_t k = k0; k < k1; ++k)
        {
            double tb = (double) k * stepB;
            if (swings && (k & 1)) tb += (swing - 0.5) * 2.0 * stepB;
            const int64_t t = absStart + (int64_t) std::llround ((tb - ppq0) * spb) - lead;
            const int idx = (int) (((k % len) + len) % len);
            Ev e; e.t = std::max (absStart, t); e.kind = Ev::step; e.s = -1; e.note = idx; e.a = (float) (spb * stepB);
            e.b = (float) (t + lead - e.t);                                 // muestras desde el evento hasta el tiempo
            push (e);
        }
    }

    // Control: vibrato, bends automáticos, trémolo, palanca, palm mute continuo. Cada kCtl muestras absolutas.
    void control (int64_t now)
    {
        const float dt = (float) kCtl / fs;
        // Mano en reposo: sin teclas ni pedal por un rato, se apagan las cuerdas que sigan sonando (las que se dejaron
        // sonar y las que resuenan por simpatía), como un guitarrista que deja de tocar.
        if (held.n + batchN == 0 && ! sustainPedal && ! (P.latch && curChord.valid()) && nq == 0)
        {
            idle += dt;
            if (idle > P.idleMute && ! idleMuted)
            {
                idleMuted = true;
                for (int s = 0; s < tab.n; ++s) if (ins->string (s).sounding()) ins->string (s).release (Release::mute);
            }
        }
        else { idle = 0.0f; idleMuted = false; }
        for (int s = 0; s < tab.n; ++s)
        {
            auto& S = st[(size_t) s];
            auto& str = ins->string (s);
            // Bend: rueda (a la última cuerda o a todas), MPE o cuerda por canal, y el bend automático.
            float bend = S.chBendSemis;
            if (P.midiIn == kMidiNormal && (P.bendAll || s == lastString)) bend += wheel * P.bendRange;
            if (S.autoBendLeft > 0.0f)
            {
                S.autoBendLeft = std::max (0.0f, S.autoBendLeft - dt);
                const float u = 1.0f - S.autoBendLeft / std::max (1e-3f, S.autoBendTotal);
                // Curva en S con un leve sobrepaso al llegar (el dedo pasa y vuelve).
                const float sm = u * u * (3.0f - 2.0f * u);
                const float over = 0.06f * std::sin (kPi * clampf ((u - 0.75f) * 4.0f, 0.0f, 1.0f));
                S.autoBend = S.autoBendFrom + (S.autoBendTo - S.autoBendFrom) * (sm + over);
            }
            bend += S.autoBend;
            str.setBend (bend);
            // Vibrato de mano: de un solo lado (la cuerda se empuja y vuelve), ~5-6 Hz con deriva.
            float depth = 0.0f;
            if (S.note >= 0 && str.gated())
            {
                const float age = (float) (now - S.onT) / fs;
                // Vibrato automático sólo en la melodía (una nota a la vez): nadie le hace vibrato a un acorde.
                const bool single = P.mode == kModeMono || (P.mode == kModePoly && held.n <= 1);
                if ((P.autoVibrato && single) || (momentary & kMomVibrato)) depth = P.vibDepth * clampf ((age - P.vibDelay) / 0.35f, 0.0f, 1.0f);
                depth = std::max (depth, P.vibDepth * 1.4f * modWheel);
                // Presión: la polifónica de la nota, o la de canal (en MPE y guitarra MIDI, la del canal de la nota; si no,
                // la del canal 1 para todas).
                const float chp = P.midiIn != kMidiNormal ? (S.srcCh > 0 ? chPress[(size_t) (S.srcCh - 1)] : 0.0f) : chPress[0];
                depth = std::max (depth, P.vibDepth * 1.4f * std::max (S.pressure, chp));
            }
            if (depth > 0.0f || S.vibAmp > 0.0f)
            {
                S.vibAmp += (depth - S.vibAmp) * 0.08f;
                const float drift = 1.0f + 0.07f * sin2pi (S.vibPhase * 0.13f + 0.3f * (float) s);
                S.vibPhase += P.vibRate * drift * dt;
                S.vibPhase -= std::floor (S.vibPhase);
                str.setVibrato (S.vibAmp * 0.5f * (1.0f - cos2pi (S.vibPhase)));
            }
            else str.setVibrato (0.0f);
            // Palanca: todas las cuerdas, las graves un poco más (el puente flotante gira).
            const float wk = 1.0f + 0.12f * (float) (tab.n - 1 - s) / (float) std::max (1, tab.n - 1);
            str.setWhammy (whammy * wk + armFlutter (now) * wk);
            // Palm mute continuo por CC.
            if (palmCC >= 0.0f) str.setPalmMute (palmCC);
            // Realimentación: mientras la nota está apretada (o la tecla momentánea).
            const float fb = (momentary & kMomFeedback) ? std::max (0.7f, P.feedback) : P.feedback;
            str.setFeedback (S.note >= 0 && str.gated() ? fb : 0.0f, P.feedbackHarm);
            // Trémolo picking: púas repetidas alternadas mientras la nota está apretada.
            if (S.tremolo && S.note >= 0 && now >= S.nextTrem)
            {
                const double spb = fs * 60.0 / std::max (20.0, lastBpm);
                const int64_t step = std::max<int64_t> (64, (int64_t) (divisionBeats (P.tremoloDiv) * spb));
                S.tremDir = -S.tremDir;
                // Cada púa con su propia variación (el índice de evento cambia): no hay dos iguales.
                PluckSpec ps = baseSpec (S.tremVel * (S.tremDir < 0 ? 0.8f : 0.88f), S.tremDir, evCount++);
                ps.fret = str.currentFret(); ps.mute = S.palm;
                Ev e = Ev::pluckAt (now, s, ps, S.note, S.outVel);
                e.b = (float) kArtTremolo; e.a = ps.fret;
                push (e);
                S.nextTrem = now + step;
            }
            // Staccato y slide out programados.
            if (S.autoOffT > 0 && now >= S.autoOffT) { S.autoOffT = 0; releaseString (s, now, false); }
        }
    }

    // Ejecutar los eventos que vencen hasta `now` (inclusive).
    void dispatch (int64_t now)
    {
        lastNow = now;
        while (nq > 0 && q[0].t <= now)
        {
            const Ev e = q[0];
            for (int i = 1; i < nq; ++i) q[(size_t) i - 1] = q[(size_t) i];
            --nq;
            run (e, now);
        }
    }
    int64_t nextEventTime() const { return nq > 0 ? q[0].t : INT64_MAX; }

    // Salida MIDI acumulada (tiempos absolutos); el motor la lee y la vacía por bloque.
    std::vector<MidiOutEv> outEvents;
    void reserve() { outEvents.reserve (4096); }

    // Para la vista y las pruebas.
    struct StringView { int note = -1; float fret = -1; bool sounding = false; };
    StringView view (int s) const { const auto& S = st[(size_t) std::clamp (s, 0, kMaxStrings - 1)]; return { S.note, ins->string (s).currentFret(), ins->string (s).sounding() }; }
    const Chord& chord() const { return curChord; }
    const Voicing& voicing() const { return curVoicing; }
    void setStrumPattern (const StrumPattern& p) { strum = p; }
    void setArpPattern (const ArpPattern& p) { arp = p; }
    const StrumPattern& strumPattern() const { return strum; }
    int currentArt() const { return P.art; }
    float handPosition() const { return fing.handPosition(); }
    int64_t eventsPlayed() const { return evCount; }
    // Soltar todo (bypass, all notes off): apaga las cuerdas y las notas de la salida.
    void allNotesOff (int64_t now, bool hard)
    {
        nq = 0; batchN = 0;
        held.clear(); keysDown = 0; chordPending = -1; seqRunning = false;
        for (int s = 0; s < kMaxStrings; ++s)
        {
            auto& S = st[(size_t) s];
            if (S.outNote >= 0) emitOff (s, now);
            if (s >= tab.n) continue;
            S.note = -1; S.tremolo = false; S.autoBend = 0; S.autoBendLeft = 0; S.chBendSemis = 0; S.autoOffT = 0;
            ins->string (s).release (hard ? Release::mute : Release::lift);
        }
    }

private:
    float fs = 48000.0f;
    Instrument* ins = nullptr;
    PerformParams P;
    Tab tab; Fingerer fing;
    StrumPattern strum = parseStrum ("D.DU.UDU", "10000000", kDiv8, 0.5f);
    ArpPattern arp = parseArp ("T 3 2 3 1 3 2 3", kDiv8, 0.5f);
    int64_t blockStart = 0; int blockLen = 0;
    int64_t evCount = 0;

    // ── Cola de eventos ──
    struct Ev
    {
        enum Kind : uint8_t { pluck, legatoHammer, legatoPull, slide, release, releaseOpen, muteAll, bendIn, step };
        int64_t t = 0; Kind kind = pluck; int8_t s = 0; PluckSpec ps; int note = -1; int vel = 100; float a = 0, b = 0;
        uint32_t tag = 0;                // el paso del patrón que la agendó (0 = ninguno): para anular lo que falta tocar
        static Ev pluckAt (int64_t t, int s, const PluckSpec& ps, int note, int vel) { Ev e; e.t = t; e.kind = pluck; e.s = (int8_t) s; e.ps = ps; e.note = note; e.vel = vel; return e; }
    };
    std::array<Ev, kQueue> q {}; int nq = 0;
    void push (const Ev& e)
    {
        if (nq >= kQueue) return;
        Ev ev = e; if (ev.tag == 0 && ev.kind == Ev::pluck) ev.tag = curTag;
        // Lo último que se agendó sobre cada cuerda: una suelta nunca se adelanta a una púa pendiente (una nota más
        // corta que el retraso humano quedaba sonando).
        if (e.s >= 0 && e.s < kMaxStrings && (e.kind == Ev::pluck || e.kind == Ev::legatoHammer || e.kind == Ev::legatoPull || e.kind == Ev::slide))
            st[(size_t) e.s].pendingT = std::max (st[(size_t) e.s].pendingT, e.t);
        int i = nq++;
        while (i > 0 && q[(size_t) i - 1].t > e.t) { q[(size_t) i] = q[(size_t) i - 1]; --i; }    // estable: mismo t, orden de llegada
        q[(size_t) i] = ev;
    }
    uint32_t curTag = 0, stepTags = 0, lastStepTag = 0;
    // Anula las púas de un paso que todavía no sonaron (el paso se repite con el acorde nuevo).
    void cancelTag (uint32_t tag)
    {
        if (tag == 0) return;
        int w = 0;
        for (int r = 0; r < nq; ++r) if (q[(size_t) r].tag != tag) q[(size_t) w++] = q[(size_t) r];
        nq = w;
        for (int s2 = 0; s2 < kMaxStrings; ++s2)
        {
            int64_t m = 0;
            for (int r = 0; r < nq; ++r)
                if (q[(size_t) r].s == s2 && (q[(size_t) r].kind == Ev::pluck || q[(size_t) r].kind == Ev::legatoHammer || q[(size_t) r].kind == Ev::legatoPull || q[(size_t) r].kind == Ev::slide))
                    m = std::max (m, q[(size_t) r].t);
            st[(size_t) s2].pendingT = std::min (st[(size_t) s2].pendingT, std::max (m, blockStart));
        }
    }

    // ── Estado por cuerda ──
    struct StrState
    {
        int note = -1;                   // nota MIDI que suena (la que se mandó por la salida)
        int key = -1, srcCh = 0;         // tecla y canal que la pidieron
        int64_t onT = 0;
        float chBendSemis = 0, pressure = 0, vibAmp = 0, vibPhase = 0;
        float autoBend = 0, autoBendFrom = 0, autoBendTo = 0, autoBendLeft = 0, autoBendTotal = 1;
        bool tremolo = false; int tremDir = 1; float tremVel = 0.8f; int64_t nextTrem = 0;
        float palm = 0; int64_t autoOffT = 0, pendingT = 0; bool slideOut = false;
        int outNote = -1, outCh = 1, outVel = 100, harmonic = 0;
    };
    std::array<StrState, kMaxStrings> st {};
    int lastString = -1;

    // Teclas apretadas (para acordes, mono y ligado).
    struct Key { int note, ch, vel; int64_t t; int string; };
    struct Held
    {
        std::array<Key, 32> k {}; int n = 0;
        void clear() { n = 0; }
        void add (const Key& x) { remove (x.note, x.ch); if (n < 32) k[(size_t) n++] = x; }
        int find (int note, int ch) const { for (int i = 0; i < n; ++i) if (k[(size_t) i].note == note && (ch == 0 || k[(size_t) i].ch == ch)) return i; return -1; }
        void remove (int note, int ch) { const int i = find (note, ch); if (i < 0) return; for (int j = i + 1; j < n; ++j) k[(size_t) j - 1] = k[(size_t) j]; --n; }
    } held;
    int keysDown = 0;

    // Acordes.
    int64_t chordPending = -1; Chord curChord; Voicing curVoicing, prevVoicing; bool voicingDirty = true;
    // Patrón.
    double win0 = -1.0, lastBpm = 120.0; int64_t seqStartT = 0; bool seqRunning = false, hostWasPlaying = false;
    // Mano derecha.
    int64_t lastPickT = -1000000; int lastDir = 1;
    // Controladores.
    bool sustainPedal = false; float wheel = 0, modWheel = 0, palmCC = -1, whammy = 0; int legatoPedal = -1;
    enum Mom { kMomUp = 1, kMomFeedback = 2, kMomVibrato = 4, kMomFingers = 8 };
    int momentary = 0;
    std::array<float, 16> chBend {}, chPress {}, chTimbre {};
    int64_t armReleaseT = -1; float armReleaseAmp = 0;
    int64_t tremT = 0, lastNow = 0;
    float idle = 0.0f; bool idleMuted = false;

    // ── Variación humana ──
    float hu (uint64_t ev, int what) const { return hashBi (P.seed, (uint64_t) ev, (uint64_t) what); }

    PluckSpec baseSpec (float vel01, int dir, int64_t ev)
    {
        PluckSpec ps;
        const float h = P.humanPick;
        ps.velocity = clampf (vel01 * (1.0f + 0.15f * P.humanVel * hu ((uint64_t) ev, 1)), 0.02f, 1.0f);
        ps.posMm = std::max (25.0f, P.pickPosMm + 7.0f * h * hu ((uint64_t) ev, 2) + (dir < 0 ? 4.0f : 0.0f));
        ps.hardness = clampf (P.pickHard + 0.1f * h * hu ((uint64_t) ev, 3) + (dir < 0 ? 0.05f : 0.0f), 0.0f, 1.0f);
        ps.angle = clampf (P.pickAngle + 0.1f * h * hu ((uint64_t) ev, 4), 0.0f, 1.0f);
        ps.dir = dir;
        ps.finger = P.fingers || (momentary & kMomFingers);
        ps.noise = P.attackNoise;
        ps.cents = 3.0f * P.humanTune * hu ((uint64_t) ev, 5);
        ps.seed = mix64 ((uint64_t) ev * 0x9E37u + P.seed);
        return ps;
    }
    int64_t humanDelay (int64_t ev) const { return (int64_t) (P.humanTime * 0.012f * fs * (0.5f + 0.5f * hu ((uint64_t) ev, 6))); }

    // ── Notas ──
    bool isKeyswitch (int note) const { return note >= P.keyswitchBase && note < P.keyswitchBase + 20 && note < (tab.n > 0 ? tab.open[0] : 40); }

    void noteOn (int note, int vel, int ch, int64_t now)
    {
        if (isKeyswitch (note))
        {
            const int k = note - P.keyswitchBase;
            if (k < kNumArts) P.art = k;
            else if (k == 12) momentary |= kMomUp;
            else if (k == 13) momentary |= kMomFeedback;
            else if (k == 14) momentary |= kMomVibrato;
            else if (k == 15) muteAll (now);
            else if (k == 16) momentary |= kMomFingers;
            artOverride = true;
            return;
        }
        ++keysDown;
        const int64_t ev = evCount++;
        switch (P.mode)
        {
            case kModeChords:
            case kModeStrum:
            case kModeArp:
                held.add ({ note, ch, vel, now, -1 });
                if (P.mode != kModeChords && ! hostWasPlaying && ! seqRunning)
                {
                    // El reloj propio arranca con esta tecla: el primer paso cae acá mismo.
                    seqRunning = true; seqStartT = now; win0 = -1.0;
                    schedule (0.0, P.tempo, now, (int) std::max<int64_t> (1, blockStart + blockLen - now));
                }
                if (P.mode != kModeChords && lastStepT >= 0 && now - lastStepT < (int64_t) (0.06f * fs) && ! replayDone)
                {
                    // Tecla tarde respecto del paso: se espera 8 ms a que llegue el resto del acorde y se repite el paso.
                    Ev e; e.t = now + (int64_t) (0.008f * fs); e.kind = Ev::bendIn; e.s = -1; push (e);
                }
                if (P.mode == kModeChords)
                {
                    if (chordPending < 0) chordPending = now + (int64_t) (P.chordWindowMs * 1e-3f * fs);
                    pendingVel = std::max (chordPending == now ? 0 : pendingVel, vel);
                    Ev e; e.t = chordPending; e.kind = Ev::pluck; e.s = -1; e.note = -2;            // marcador: "acorde listo"
                    push (e);
                }
                else
                {
                    updateChord (now);
                    // El acorde cambió durante un paso que todavía está sonando: lo que faltaba del acorde viejo ya no suena
                    // (la mano se fue); el paso se repite con el nuevo a los 8 ms.
                    if (lastStepT >= 0 && now - lastStepT < (int64_t) (0.06f * fs) && ! replayDone && lastStepPlayed && curChord.pcs != lastStepPcs)
                        cancelTag (lastStepTag);
                }
                return;
            default: break;
        }
        // Polifónica: las teclas de la misma muestra se juntan y se ubican juntas al despachar (después del MIDI).
        if (P.mode == kModePoly && P.midiIn == kMidiNormal)
        {
            if (batchN == 0) { Ev e; e.t = now; e.kind = Ev::step; e.s = -2; push (e); }
            if (batchN < (int) batch.size()) batch[(size_t) batchN++] = { note, vel, ch, ev };
            return;
        }
        melodyNoteOn (note, vel, ch, now, ev);
    }
    int pendingVel = 0;
    bool artOverride = false;

    void noteOff (int note, int ch, int64_t now)
    {
        if (isKeyswitch (note))
        {
            const int k = note - P.keyswitchBase;
            if (k == 12) momentary &= ~kMomUp;
            else if (k == 13) momentary &= ~kMomFeedback;
            else if (k == 14) momentary &= ~kMomVibrato;
            else if (k == 16) momentary &= ~kMomFingers;
            return;
        }
        keysDown = std::max (0, keysDown - 1);
        if (P.mode == kModeChords || P.mode == kModeStrum || P.mode == kModeArp)
        {
            held.remove (note, ch);
            if (held.n == 0)
            {
                chordPending = -1;
                // Un cambio de acorde suelta las teclas un instante antes de las nuevas: se espera 60 ms antes de apagar
                // las cuerdas (y de parar el reloj propio), y si llegan teclas nuevas, no se apaga nada.
                if (! P.latch && ! sustainPedal) { Ev e; e.t = now + (int64_t) (0.06f * fs); e.kind = Ev::muteAll; e.s = -1; push (e); }
            }
            else if (P.mode != kModeChords) updateChord (now);
            return;
        }
        // Melodía.
        const int i = held.find (note, P.midiIn == kMidiNormal ? 0 : ch);
        int s = i >= 0 ? held.k[(size_t) i].string : -1;
        held.remove (note, P.midiIn == kMidiNormal ? 0 : ch);
        if (s < 0 || st[(size_t) s].key != note)
        {
            // Si la tecla quedó en otra cuerda (no debería), se suelta igual: nunca una nota colgada.
            if (held.find (note, P.midiIn == kMidiNormal ? 0 : ch) < 0)
                for (int k = 0; k < tab.n; ++k)
                    if (st[(size_t) k].key == note && (P.midiIn == kMidiNormal || st[(size_t) k].srcCh == ch) && ! sustainPedal) releaseString (k, now, false);
            return;
        }
        // Mono con ligado: si queda otra tecla apretada, volver a ella (trinos con dos teclas).
        if (P.mode == kModeMono && held.n > 0 && legatoOn())
        {
            const Key& back = held.k[(size_t) held.n - 1];
            const int f = back.note - tab.open[(size_t) s];
            if (f >= 0 && f <= tab.frets && std::fabs ((float) f - ins->string (s).currentFret()) <= P.legatoFrets + 0.5f)
            {
                legatoTo (s, back.note, f, back.vel, now, evCount++, back.ch);
                held.k[(size_t) held.n - 1].string = s;
                return;
            }
        }
        if (sustainPedal) return;
        releaseString (s, now, false);
    }

    bool legatoOn() const { return legatoPedal >= 0 ? legatoPedal > 0 : P.legato != kLegatoOff; }

    void melodyNoteOn (int note, int vel, int ch, int64_t now, int64_t ev)
    {
        // Cuerdas ocupadas por notas sostenidas (en poly no se cortan si hay otra opción).
        uint32_t busy = 0;
        for (int s = 0; s < tab.n; ++s) if (st[(size_t) s].note >= 0 && ins->string (s).gated()) busy |= 1u << s;
        int art = P.art;
        if (P.deadBelow > 0 && vel < P.deadBelow) art = kArtDead;
        if (P.pinchAbove > 0 && vel >= P.pinchAbove && art == kArtSustain) art = kArtPinch;
        // Guitarra MIDI: el canal es la cuerda (1 = la más aguda).
        int forced = -1;
        if (P.midiIn == kMidiPerString && ch >= 1 && ch <= tab.n) forced = tab.n - ch;
        // La misma tecla otra vez sin soltarla: se vuelve a picar la misma cuerda (no se abre otra).
        if (forced < 0)
        {
            const int pi = held.find (note, P.midiIn == kMidiNormal ? 0 : ch);
            if (pi >= 0 && held.k[(size_t) pi].string >= 0 && st[(size_t) held.k[(size_t) pi].string].key == note) forced = held.k[(size_t) pi].string;
        }

        // Ligado: la tecla nueva se solapa con una nota que sigue apretada.
        if (legatoOn() && held.n > 0 && art != kArtHarmonic && art != kArtDead)
        {
            const Key& prev = held.k[(size_t) held.n - 1];
            const int ps = prev.string;
            if (ps >= 0 && st[(size_t) ps].key == prev.note && (forced < 0 || forced == ps))
            {
                const int f = note - tab.open[(size_t) ps];
                const float cf = ins->string (ps).currentFret();
                if (f >= 0 && f <= tab.frets && std::fabs ((float) f - cf) <= P.legatoFrets + 0.5f && (P.mode == kModeMono || popc (busy) <= 1))
                {
                    legatoTo (ps, note, f, vel, now, ev, ch);
                    held.add ({ note, ch, vel, now, ps });
                    return;
                }
            }
        }
        // Mono: la nota anterior se corta (si estaba en otra cuerda, se apaga).
        if (P.mode == kModeMono)
            for (int s = 0; s < tab.n; ++s) if (st[(size_t) s].note >= 0 && ins->string (s).gated()) { releaseString (s, now, false); busy &= ~(1u << s); }

        FretPos fp;
        int harm = 0;
        if (art == kArtHarmonic) fp = placeHarmonic (note, busy, harm);
        else if (forced >= 0) { const int f = note - tab.open[(size_t) forced]; fp = (f >= 0 && f <= tab.frets) ? FretPos { forced, f } : fing.place (note, busy, P.prefs); }
        else
        {
            // Notas que llegan juntas (mismo instante) se reparten como un acorde.
            fp = fing.place (note, busy | sameInstantMask (now), P.prefs);
        }
        if (! fp.valid()) return;
        playAt (note, vel, ch, now, ev, fp, art, harm, 0, true);
    }

    // Tocar una nota ya ubicada (cuerda y traste): chirrido si la mano saltó, púa alternada, la púa con su variación,
    // las articulaciones de ataque; extraDelay corre el ataque (las notas de un acorde se escalonan un poco).
    void playAt (int note, int vel, int ch, int64_t now, int64_t ev, const FretPos& fp, int art, int harm, int64_t extraDelay, bool alternate)
    {
        float harmMix = 1.0f;
        const int s = fp.string;
        // Cambio de posición grande en una cuerda entorchada: chirrido de los dedos.
        if (lastString >= 0 && now - lastPickT < (int64_t) (0.7f * fs) && fp.fret > 0)
        {
            const float from = fing.handPosition();
            if (std::fabs ((float) fp.fret - from) >= 3.0f && hu ((uint64_t) ev, 9) > -0.2f)
                ins->squeak (s, from, (float) fp.fret, 0.06f + 0.012f * std::fabs ((float) fp.fret - from), P.squeak * 0.6f);
        }
        fing.commit (fp);
        // Púa alternada en pasajes rápidos.
        int dir = 1;
        if (alternate)
        {
            if (now - lastPickT < (int64_t) (0.28f * fs)) dir = -lastDir;
            if (momentary & kMomUp) dir = -1;
            lastPickT = now; lastDir = dir;
        }
        PluckSpec ps = baseSpec (vel / 127.0f, dir, ev);
        ps.fret = (float) fp.fret;
        // CC 74 (el "timbre" del MPE) del canal de la nota: la dureza de la púa.
        if (ch >= 1 && ch <= 16 && chTimbre[(size_t) (ch - 1)] >= 0.0f) ps.hardness = 0.1f + 0.9f * chTimbre[(size_t) (ch - 1)];
        if (art == kArtPalm) ps.mute = P.palm;
        if (palmCC > 0.0f) ps.mute = palmCC;
        if (art == kArtDead) ps.dead = true;
        if (art == kArtHarmonic) { ps.harmonic = harm; ps.harmonicMix = 1.0f; ps.hardness = std::max (ps.hardness, 0.6f); }
        if (art == kArtPinch)
        {
            const float L = ins->string (s).physics().lengthMm * std::exp2 (-(float) fp.fret / 12.0f);
            const float kf = L / std::max (30.0f, ps.posMm);
            int k = (int) std::floor (kf) + (hu ((uint64_t) ev, 10) > 0.0f ? 1 : 0);
            ps.harmonic = std::clamp (k, 3, 9); ps.harmonicMix = 0.82f; harmMix = 0.82f;
            ps.hardness = 1.0f; ps.velocity = std::min (1.0f, ps.velocity * 1.15f);
        }
        auto& S = st[(size_t) s];
        const int64_t t = now + humanDelay (ev) + extraDelay;
        // Bend de entrada: el dedo pisa más abajo y empuja la cuerda hasta la nota.
        float startFret = ps.fret;
        if (art == kArtBendIn) { startFret = std::max (0.0f, ps.fret - std::round (P.bendInAmount)); ps.fret = startFret; }
        if (art == kArtSlideIn) { startFret = std::max (0.0f, (float) fp.fret - 4.0f); ps.fret = startFret; }
        Ev e = Ev::pluckAt (t, s, ps, note, vel);
        e.a = (float) fp.fret;                 // destino (bend in / slide in)
        e.b = (float) art;
        e.vel = vel;
        S.key = note; S.srcCh = ch; S.harmonic = harm; (void) harmMix;
        // MPE / guitarra MIDI: el bend que el canal ya traía antes del note-on vale desde el ataque.
        if (P.midiIn == kMidiMpe && ch > 1) S.chBendSemis = chBend[(size_t) (ch - 1)] * P.mpeBendRange;
        else if (P.midiIn == kMidiPerString && ch >= 1 && ch <= 16) S.chBendSemis = chBend[(size_t) (ch - 1)] * P.stringBendRange;
        push (e);
        held.add ({ note, ch, vel, now, s });
        lastString = s;
    }

    // Teclas que llegaron en la misma muestra (un acorde en modo melodía polifónica): se ubican juntas (cuerdas distintas,
    // una mano posible) y se pican de grave a aguda con un escalonamiento chico, como una púa que las barre rápido.
    struct BatchNote { int note, vel, ch; int64_t ev; };
    std::array<BatchNote, 12> batch {}; int batchN = 0;
    void flushBatch (int64_t now)
    {
        int n = batchN; batchN = 0;
        // Las teclas que ya estaban apretadas (repetidas) vuelven a su cuerda, una por una; el resto se ubica junto.
        for (int i = 0; i < n;)
        {
            const auto& b = batch[(size_t) i];
            const int pi = held.find (b.note, 0);
            if (pi >= 0 && held.k[(size_t) pi].string >= 0 && st[(size_t) held.k[(size_t) pi].string].key == b.note)
            {
                melodyNoteOn (b.note, b.vel, b.ch, now, b.ev);
                for (int j = i + 1; j < n; ++j) batch[(size_t) j - 1] = batch[(size_t) j];
                --n;
            }
            else ++i;
        }
        if (n == 0) return;
        if (n == 1 || P.art == kArtHarmonic) { for (int i = 0; i < n; ++i) melodyNoteOn (batch[(size_t) i].note, batch[(size_t) i].vel, batch[(size_t) i].ch, now, batch[(size_t) i].ev); return; }
        uint32_t busy = 0;
        for (int s = 0; s < tab.n; ++s) if (st[(size_t) s].note >= 0 && ins->string (s).gated()) busy |= 1u << s;
        std::array<int, 12> notes {}; std::array<FretPos, 12> pos {};
        for (int i = 0; i < n; ++i) notes[(size_t) i] = batch[(size_t) i].note;
        fing.placeChord (notes.data(), n, busy, P.prefs, pos.data());
        std::array<int, 12> order {}; for (int i = 0; i < n; ++i) order[(size_t) i] = i;
        std::sort (order.begin(), order.begin() + n, [&] (int a, int b) { return notes[(size_t) a] < notes[(size_t) b]; });
        const float spreadMs = n >= 3 ? 0.3f * P.strumMs : 0.0f;
        int k = 0;
        for (int j = 0; j < n; ++j)
        {
            const int i = order[(size_t) j];
            if (! pos[(size_t) i].valid()) continue;
            const auto& b = batch[(size_t) i];
            int art = P.art;
            if (P.deadBelow > 0 && b.vel < P.deadBelow) art = kArtDead;
            playAt (b.note, b.vel, b.ch, now, b.ev, pos[(size_t) i], art, 0, (int64_t) ((float) k * spreadMs * 1e-3f * fs), k == 0);
            ++k;
        }
    }

    uint32_t sameInstantMask (int64_t now) const
    {
        uint32_t m = 0;
        for (int i = 0; i < held.n; ++i) if (held.k[(size_t) i].t == now && held.k[(size_t) i].string >= 0) m |= 1u << held.k[(size_t) i].string;
        return m;
    }

    // Armónico: natural (cuerda al aire, nodo en 12, 7, 5, 4, 3,2) o, si no hay, artificial (traste + 12).
    FretPos placeHarmonic (int note, uint32_t busy, int& harm)
    {
        static const int ks[] = { 2, 3, 4, 5, 6 };
        static const int semis[] = { 12, 19, 24, 28, 31 };
        FretPos best; float bc = 1e9f;
        for (int s = 0; s < tab.n; ++s)
            for (int i = 0; i < 5; ++i)
                if (tab.open[(size_t) s] + semis[i] == note)
                {
                    const float c = (float) i + ((busy >> s) & 1u ? 20.0f : 0.0f);
                    if (c < bc) { bc = c; best = { s, 0 }; harm = ks[i]; }
                }
        if (best.valid()) return best;
        best = fing.place (note - 12, busy, P.prefs);
        harm = 2;
        return best;
    }

    void legatoTo (int s, int note, int fret, int vel, int64_t now, int64_t ev, int ch)
    {
        auto& S = st[(size_t) s];
        const float cf = ins->string (s).currentFret();
        int mode = legatoPedal > 0 ? kLegatoHammer : P.legato;
        if (mode == kLegatoAuto) mode = std::fabs ((float) fret - cf) <= 3.0f ? kLegatoHammer : kLegatoSlide;
        Ev e; e.t = now; e.s = (int8_t) s; e.note = note; e.vel = vel; e.a = (float) fret;
        e.kind = mode == kLegatoSlide ? Ev::slide : ((float) fret > cf ? Ev::legatoHammer : Ev::legatoPull);
        e.b = P.slideTime * (0.6f + 0.1f * std::fabs ((float) fret - cf));
        e.ps = baseSpec (vel / 127.0f, 1, ev);
        S.key = note; S.srcCh = ch;
        push (e);
        fing.commit ({ s, fret });
        lastString = s;
    }

    void releaseString (int s, int64_t now, bool handMute)
    {
        auto& S = st[(size_t) s];
        now = std::max (now, S.pendingT);
        Ev e; e.t = now; e.s = (int8_t) s;
        e.kind = handMute ? Ev::release : Ev::releaseOpen;
        if (P.art == kArtSlideOut && ! handMute && ! S.slideOut)
        {
            // Slide out: el dedo se va hacia la cejilla mientras afloja.
            S.slideOut = true;
            Ev sl; sl.t = now; sl.s = (int8_t) s; sl.kind = Ev::slide; sl.a = std::max (0.0f, ins->string (s).currentFret() - 7.0f); sl.b = 0.22f; sl.note = -1;
            push (sl);
            e.t = now + (int64_t) (0.2f * fs); e.kind = Ev::release;
        }
        push (e);
        S.tremolo = false; S.key = -1;
    }

    void muteAll (int64_t now)
    {
        for (int s = 0; s < tab.n; ++s) { Ev e; e.t = now; e.s = (int8_t) s; e.kind = Ev::release; push (e); }
    }

    // ── Acordes y patrones ──
    void updateChord (int64_t now)
    {
        (void) now;
        if (held.n == 0) return;
        std::array<int, 32> notes {};
        for (int i = 0; i < held.n; ++i) notes[(size_t) i] = held.k[(size_t) i].note;
        const Chord c = detectChord (notes.data(), held.n, P.singleKey);
        if (! (c == curChord) || voicingDirty || c.pcs != curChord.pcs)
        {
            curChord = c;
            prevVoicing = curVoicing;
            curVoicing = findVoicing (tab, curChord, P.style, P.region);
            voicingDirty = false;
        }
        int v = 0; for (int i = 0; i < held.n; ++i) v = std::max (v, held.k[(size_t) i].vel);
        chordVel = v;
    }
    int chordVel = 100;

    // La mano izquierda cambia de acorde: las cuerdas que cambian de traste o se apagan dejan de sonar un instante
    // antes (el dedo se levanta), lo que da el pequeño hueco real entre acordes.
    void applyVoicingChange (int64_t now)
    {
        // La mano se mueve a otra posición: chirrido en la cuerda entorchada más grave del acorde nuevo.
        auto minFret = [&] (const std::array<int, kMaxStrings>& f) { int m = 99; for (int s = 0; s < tab.n; ++s) if (f[(size_t) s] > 0) m = std::min (m, f[(size_t) s]); return m; };
        const int from = minFret (prevApplied), to = minFret (curVoicing.fret);
        if (from < 99 && to < 99 && std::abs (to - from) >= 3)
        {
            const int s = curVoicing.lowestString();
            if (s >= 0) ins->squeak (s, (float) from, (float) to, 0.05f + 0.01f * (float) std::abs (to - from), P.squeak * 0.5f);
        }
        for (int s = 0; s < tab.n; ++s)
        {
            const int nf = curVoicing.fret[(size_t) s];
            if (st[(size_t) s].note < 0) continue;
            if (nf < 0 || std::fabs (ins->string (s).currentFret() - (float) nf) > 0.1f)
            {
                Ev e; e.t = now; e.s = (int8_t) s; e.kind = Ev::release; push (e);
            }
        }
    }

    int minFretOf (const Voicing& v) const { int m = 99; for (int s = 0; s < tab.n; ++s) if (v.fret[(size_t) s] > 0) m = std::min (m, v.fret[(size_t) s]); return m == 99 ? 0 : m; }

    // Un rasgueo: las cuerdas del voicing, una tras otra, con su tiempo y su fuerza.
    // t: el tiempo del rasgueo. centredAt >= 0: el medio del rasgueo cae en t, sin empezar antes de centredAt (ahora).
    void strumChord (int64_t t, int dir, float vel01, int kindMask, float palm, bool dead, int64_t ev, double stepSamples, int64_t centredAt = -1)
    {
        if (! curVoicing.valid) return;
        const int lo = curVoicing.lowestString(), hi = curVoicing.highestString();
        if (lo < 0) return;
        std::array<int, kMaxStrings> order {}; int n = 0;
        // kindMask: 0 todas, 1 sólo agudas (arriba suelen sonar 3-4 cuerdas), 2 sólo el bajo, 3 bajo alternado.
        int first = lo, last = hi;
        if (kindMask == 1) first = std::max (lo, hi - std::max (1, P.upStrings) + 1);
        if (dir < 0 && kindMask == 0) first = std::max (lo, hi - std::max (2, P.upStrings) + 1);
        if (kindMask == 2) { last = lo; }
        if (kindMask == 3) { int a = lo + 1; while (a <= hi && curVoicing.fret[(size_t) a] < 0) ++a; first = last = std::min (a, hi); }
        // Las cuerdas apagadas entre las del acorde también las toca la púa: suenan muertas (la mano las tapa).
        if (dir > 0) for (int s = first; s <= last; ++s) order[(size_t) n++] = s;
        else for (int s = last; s >= first; --s) order[(size_t) n++] = s;
        if (n == 0) return;
        // Ancho del rasgueo: más rápido cuanto más fuerte; nunca más largo que la mitad del paso.
        float spreadMs = P.strumMs * (1.3f - 0.55f * vel01) * (1.0f + 0.15f * P.humanTime * hu ((uint64_t) ev, 11));
        if (stepSamples > 0.0) spreadMs = std::min (spreadMs, (float) (0.5 * stepSamples / fs * 1000.0));
        const float per = n > 1 ? spreadMs / (float) (n - 1) : 0.0f;
        // La mano no va pareja: cada intervalo entre cuerdas varía un poco.
        std::array<float, kMaxStrings> at {};
        { float acc = 0.0f; for (int i = 0; i < n; ++i) { at[(size_t) i] = acc; acc += per * (1.0f + 0.25f * P.humanTime * hu ((uint64_t) ev, 20 + i)); } }
        int64_t t0 = t;
        if (centredAt >= 0 && n > 1)
        {
            // El medio: la cuerda central (o el promedio de las dos centrales) cae en t.
            const float mid = (n & 1) ? at[(size_t) (n / 2)] : 0.5f * (at[(size_t) (n / 2 - 1)] + at[(size_t) (n / 2)]);
            t0 = std::max (centredAt, t - (int64_t) (mid * 1e-3f * fs));
        }
        for (int i = 0; i < n; ++i)
        {
            const int s = order[(size_t) i];
            const int64_t ts = t0 + (int64_t) (at[(size_t) i] * 1e-3f * fs);
            // Fuerza: la púa pierde algo al cruzar las cuerdas.
            const float v = clampf (vel01 * (1.0f - 0.04f * (float) i) * (1.0f + 0.08f * P.humanVel * hu ((uint64_t) ev, 40 + i)), 0.05f, 1.0f);
            PluckSpec ps = baseSpec (v, dir, ev * 8 + (int64_t) i);
            const int f = curVoicing.fret[(size_t) s];
            if (f < 0)
            {
                // Muerta y sin nota de salida (note = −3): un golpe apagado, más suave.
                ps.fret = (float) std::max (0, minFretOf (curVoicing)); ps.dead = true; ps.velocity *= 0.35f;
                push (Ev::pluckAt (ts, s, ps, -3, (int) std::lround (v * 0.35f * 127.0f)));
                continue;
            }
            ps.fret = (float) f;
            ps.mute = palm; ps.dead = dead;
            const int note = tab.open[(size_t) s] + f;
            push (Ev::pluckAt (ts, s, ps, note, (int) std::lround (v * 127.0f)));
        }
    }

    // t: ahora (cuando se resuelve el paso); beat: el tiempo del paso (>= t; mayor con el rasgueo centrado).
    void strumStep (const StrumStep& sp, int64_t t, int64_t beat, double stepSamples, bool centred)
    {
        if (sp.type == '.' || sp.type == '-' || sp.type == ' ') return;
        updateChord (t);
        if (sp.type == '|') { muteAll (t); return; }
        const int64_t ev = evCount++;
        float v = (chordVel / 127.0f) * (sp.accent == 2 ? 1.0f : sp.accent == 1 ? 0.85f : 0.66f);
        int dir = 1, mask = 0; float palm = 0.0f; bool dead = false;
        switch (sp.type)
        {
            case 'D': break;
            case 'U': dir = -1; break;
            case 'd': mask = 1; v *= 0.8f; break;
            case 'u': dir = -1; mask = 1; v *= 0.8f; break;
            case 'M': palm = P.palm; break;
            case 'm': dir = -1; palm = P.palm; break;
            case 'X': dead = true; break;
            case 'x': dir = -1; dead = true; break;
            case 'B': mask = 2; break;
            case 'A': mask = 3; break;
            case 'S': break;
            default: return;
        }
        if (P.art == kArtPalm) palm = std::max (palm, P.palm);
        if (palmCC > 0.0f) palm = std::max (palm, palmCC);
        const int64_t td = beat + humanDelay (ev) / 2;
        if (! (prevApplied == curVoicing.fret)) { applyVoicingChange (std::max (t, td - (int64_t) (0.012f * fs) - (centred ? (int64_t) (0.5f * P.strumMs * 1e-3f * fs) : 0))); prevApplied = curVoicing.fret; }
        strumChord (td, dir, v, mask, palm, dead, ev, stepSamples, centred ? t : -1);
        if (sp.type == 'S')
            for (int s = 0; s < tab.n; ++s) { Ev e; e.t = td + (int64_t) (stepSamples * 0.5); e.s = (int8_t) s; e.kind = Ev::release; push (e); }
    }
    std::array<int, kMaxStrings> prevApplied {};
    int64_t lastStepT = -1; int lastStepIdx = 0; double lastStepSamples = 0; bool lastStepPlayed = false, replayDone = true; uint16_t lastStepPcs = 0;
    void playStep (int idx, int64_t now, double stepSamples, int64_t beat, bool centred)
    {
        curTag = ++stepTags; if (curTag == 0) curTag = ++stepTags;
        lastStepTag = curTag;
        if (P.mode == kModeStrum) strumStep (strum.steps[(size_t) std::clamp (idx, 0, strum.len - 1)], now, std::max (now, beat), stepSamples, centred);
        else if (P.mode == kModeArp) arpStep (arp.steps[(size_t) std::clamp (idx, 0, arp.len - 1)], now, 0);
        curTag = 0;
    }

    void arpStep (const ArpStep& sp, int64_t t, int64_t k)
    {
        (void) k;
        if (sp.mask == 0) return;
        updateChord (t);
        if (! curVoicing.valid) return;
        const int64_t ev = evCount++;
        if (! (prevApplied == curVoicing.fret)) { applyVoicingChange (t - (int64_t) (0.010f * fs)); prevApplied = curVoicing.fret; }
        // Las cuerdas del voicing, de grave a aguda.
        std::array<int, kMaxStrings> vs {}; int n = 0;
        for (int s = 0; s < tab.n; ++s) if (curVoicing.fret[(size_t) s] >= 0) vs[(size_t) n++] = s;
        if (n == 0) return;
        auto pick = [&] (int s, float vel, int i) {
            PluckSpec ps = baseSpec (vel, 1, ev * 8 + i);
            ps.fret = (float) curVoicing.fret[(size_t) s];
            ps.finger = true; ps.hardness = std::min (ps.hardness, 0.45f);
            if (P.art == kArtPalm) ps.mute = P.palm;
            const int64_t ts = t + humanDelay (ev * 8 + i) / 2;
            push (Ev::pluckAt (ts, s, ps, tab.open[(size_t) s] + curVoicing.fret[(size_t) s], (int) (vel * 127)));
        };
        const float v = (chordVel / 127.0f) * (sp.accent ? 0.95f : 0.75f);
        int i = 0;
        if (sp.mask & 1) pick (vs[0], v * 1.05f, i++);
        if ((sp.mask & 2) && n > 1) pick (vs[1], v, i++);
        for (int r = 0; r < 4; ++r)
            if (sp.mask & (4 << r)) { const int idx = n - 1 - r; if (idx >= ((sp.mask & 3) ? 1 : 0) && idx >= 0) pick (vs[(size_t) idx], v * 0.92f, i++); }
    }

    // ── Controladores ──
    void pitchBend (int ch, int v, int64_t now)
    {
        (void) now;
        const float x = (float) v / 8192.0f;
        chBend[(size_t) (ch - 1)] = x;
        if (P.midiIn == kMidiNormal) { wheel = x; return; }
        const float range = P.midiIn == kMidiPerString ? P.stringBendRange : P.mpeBendRange;
        if (P.midiIn == kMidiPerString && ch >= 1 && ch <= tab.n) { st[(size_t) (tab.n - ch)].chBendSemis = x * range; return; }
        // MPE: el canal maestro (1) mueve todas; cada canal de nota, su cuerda.
        if (ch == 1) { wheel = x; for (int s = 0; s < tab.n; ++s) if (st[(size_t) s].srcCh == 1) st[(size_t) s].chBendSemis = x * 2.0f; return; }
        for (int s = 0; s < tab.n; ++s) if (st[(size_t) s].srcCh == ch && st[(size_t) s].note >= 0) st[(size_t) s].chBendSemis = x * range;
    }
    int stringOfKey (int note, int ch) const
    {
        for (int s = 0; s < tab.n; ++s) if (st[(size_t) s].key == note && (st[(size_t) s].srcCh == ch || P.midiIn == kMidiNormal)) return s;
        return -1;
    }
    void cc (int num, int val, int ch, int64_t now)
    {
        const float x = val / 127.0f;
        switch (num)
        {
            case 1: modWheel = x; break;
            case 2: palmCC = val == 0 ? -1.0f : x; if (val == 0) for (int s = 0; s < tab.n; ++s) ins->string (s).setPalmMute (0.0f); break;
            case 4: whammy = -x * P.whammyRange; if (val == 0 && armReleaseT < 0) { armReleaseT = now; armReleaseAmp = 0.35f; } else if (val > 0) armReleaseT = -1; break;
            case 64: sustainPedal = val >= 64;
                if (! sustainPedal && (P.mode == kModePoly || P.mode == kModeMono))
                    for (int s = 0; s < tab.n; ++s) if (st[(size_t) s].note >= 0 && held.find (st[(size_t) s].key, 0) < 0) releaseString (s, now, false);
                if (! sustainPedal && P.mode >= kModeChords && held.n == 0 && ! P.latch)
                    for (int s = 0; s < tab.n; ++s) if (st[(size_t) s].note >= 0) releaseString (s, now, true);
                break;
            case 68: legatoPedal = val >= 64 ? 1 : 0; break;
            case 74: chTimbre[(size_t) (ch - 1)] = x; break;
            case 120: case 123: allNotesOff (now, num == 120); break;
            default: break;
        }
    }
    // Al soltar la palanca, el puente flotante oscila unos instantes (amortiguado).
    float armFlutter (int64_t now)
    {
        if (armReleaseT < 0) return 0.0f;
        const float tt = (float) (now - armReleaseT) / fs;
        if (tt > 0.6f) { armReleaseT = -1; return 0.0f; }
        return armReleaseAmp * fastExp (-tt / 0.12f) * sin2pi (tt * 9.0f);
    }

    // ── Ejecución ──
    void emitOn (int s, int note, int vel, int64_t t)
    {
        auto& S = st[(size_t) s];
        if (S.outNote >= 0) emitOff (s, t);
        // Se emite siempre (el procesador lo graba para exportarlo); a la salida MIDI del host va sólo si está activa.
        const int ch = P.midiOutPerString ? std::clamp (tab.n - s, 1, 16) : std::clamp (P.midiOutChannel, 1, 16);
        S.outNote = std::clamp (note, 0, 127); S.outCh = ch;
        if (outEvents.size() < outEvents.capacity()) outEvents.push_back ({ t, (uint8_t) (0x90 | (ch - 1)), (uint8_t) S.outNote, (uint8_t) std::clamp (vel, 1, 127) });
    }
    void emitOff (int s, int64_t t)
    {
        auto& S = st[(size_t) s];
        if (S.outNote < 0) return;
        // (Un note-off nunca se pierde: si la reserva está llena, se agranda; nunca pasa con 4096 eventos por bloque.)
        outEvents.push_back ({ t, (uint8_t) (0x80 | (S.outCh - 1)), (uint8_t) S.outNote, 0 });
        S.outNote = -1;
    }

    void run (const Ev& e, int64_t now)
    {
        // Teclas juntas de la melodía polifónica.
        if (e.kind == Ev::step && e.s == -2) { flushBatch (now); return; }
        // Un paso del patrón: se resuelve ahora (acorde y voicing de este instante).
        if (e.kind == Ev::step)
        {
            // (Las teclas que suenan, no un contador: dos note-on de la misma tecla y un solo note-off no dejan el patrón
            // tocando para siempre.)
            const bool active = held.n > 0 || (P.latch && curChord.valid());
            // (lastStepT: el tiempo del paso, no cuando se resolvió.)
            lastStepT = now + (int64_t) e.b; lastStepIdx = e.note; lastStepSamples = (double) e.a; lastStepPlayed = active; replayDone = false;
            if (! active) return;
            playStep (e.note, now, (double) e.a, now + (int64_t) e.b, e.b > 0.0f);
            lastStepPcs = curChord.pcs;
            return;
        }
        // Repetir el paso: las teclas llegaron un poco tarde (tocando en vivo) y el paso sonó sin acorde o con el anterior.
        if (e.kind == Ev::bendIn && e.s < 0)
        {
            if (replayDone || lastStepT < 0) return;
            replayDone = true;
            updateChord (now);
            if (! curVoicing.valid || (lastStepPlayed && curChord.pcs == lastStepPcs)) return;
            // Lo que faltaba tocar del paso con el acorde anterior no suena; el paso se toca ya, con el nuevo.
            cancelTag (lastStepTag);
            playStep (lastStepIdx, now, lastStepSamples, lastStepT, lastStepT > now && P.strumCentred);
            lastStepPcs = curChord.pcs; lastStepPlayed = true;
            return;
        }
        // Las teclas se soltaron hace 60 ms y no volvieron: se apagan las cuerdas y se para el reloj propio.
        if (e.kind == Ev::muteAll && e.s < 0)
        {
            if (held.n > 0 || P.latch || sustainPedal) return;
            seqRunning = false;
            for (int s = 0; s < tab.n; ++s) if (st[(size_t) s].note >= 0) releaseString (s, now, true);
            return;
        }
        // Marcador del modo acordes: la ventana terminó, se toca el acorde.
        if (e.note == -2 && e.s < 0)
        {
            if (chordPending < 0 || now < chordPending) return;
            chordPending = -1;
            updateChord (now);
            if (! curVoicing.valid) return;
            applyVoicingChange (now);
            const int dir = (momentary & kMomUp) ? -1 : 1;
            float palm = P.art == kArtPalm ? P.palm : 0.0f;
            if (palmCC > 0.0f) palm = palmCC;
            strumChord (now, dir, std::max (pendingVel, chordVel) / 127.0f, 0, palm, P.art == kArtDead, evCount++, 0.0);
            pendingVel = 0;
            prevApplied = curVoicing.fret;
            return;
        }
        const int s = e.s;
        if (s < 0 || s >= tab.n) return;
        auto& S = st[(size_t) s];
        auto& str = ins->string (s);
        switch (e.kind)
        {
            case Ev::pluck:
            {
                str.pluck (e.ps);
                if (e.note == -3) { if (S.outNote >= 0) emitOff (s, now); S.note = -1; S.autoOffT = 0; break; }
                const int art = (int) e.b;
                // El sonido real: con armónico, la altura del parcial.
                int sounding = e.note;
                if (e.ps.harmonic >= 2 && e.ps.harmonicMix > 0.9f) sounding = tab.open[(size_t) s] + (int) std::lround (e.ps.fret) + (int) std::lround (12.0 * std::log2 ((double) e.ps.harmonic));
                emitOn (s, sounding, e.vel, now);
                S.note = sounding; S.onT = now; S.vibAmp = 0; S.vibPhase = 0; S.palm = e.ps.mute; S.slideOut = false;
                S.autoBend = 0; S.autoBendLeft = 0;
                if (art == kArtBendIn && e.a > e.ps.fret)
                {
                    S.autoBendFrom = 0; S.autoBendTo = e.a - e.ps.fret; S.autoBendTotal = P.bendInTime; S.autoBendLeft = P.bendInTime; S.autoBend = 0;
                }
                if (art == kArtSlideIn && e.a > e.ps.fret)
                {
                    str.slideTo (e.a, P.slideTime);
                    ins->squeak (s, e.ps.fret, e.a, P.slideTime, P.squeak);
                }
                if (art == kArtStaccato) S.autoOffT = now + (int64_t) (P.staccatoMs * 1e-3f * fs);
                if (art == kArtTremolo)
                {
                    // La primera púa fija la fuerza; las repetidas mantienen el trémolo andando.
                    if (! S.tremolo) S.tremVel = e.ps.velocity;
                    S.tremolo = true; S.tremDir = e.ps.dir;
                    S.nextTrem = now + (int64_t) (divisionBeats (P.tremoloDiv) * fs * 60.0 / std::max (20.0, lastBpm));
                }
                else S.tremolo = false;
                if (art == kArtDead) S.autoOffT = now + (int64_t) (0.06f * fs);
                break;
            }
            case Ev::legatoHammer:
            case Ev::legatoPull:
                str.legato (e.a, e.kind == Ev::legatoHammer ? 0 : 1, e.ps.velocity, e.ps.seed);
                emitOn (s, e.note, e.vel, now);
                S.note = e.note; S.onT = now; S.vibAmp *= 0.5f;
                break;
            case Ev::slide:
            {
                const float from = str.currentFret();
                str.slideTo (e.a, e.b);
                ins->squeak (s, from, e.a, e.b, P.squeak);
                if (e.note >= 0) { emitOn (s, e.note, e.vel, now); S.note = e.note; S.onT = now; }
                break;
            }
            case Ev::release:
                str.release (Release::mute);
                emitOff (s, now); S.note = -1; S.tremolo = false; S.autoBend = 0; S.autoBendLeft = 0; S.chBendSemis = 0;
                break;
            case Ev::releaseOpen:
            {
                const bool open = str.currentFret() < 0.5f;
                const bool ring = P.letRing && open;
                const float lvl = P.releaseNoise * 0.07f * (0.6f + 0.4f * (float) hashUni (P.seed, (uint64_t) now, 31));
                if (ring) str.release (Release::ring);
                else if (open) str.release (Release::mute);
                else str.releaseToOpen (lvl, mix64 ((uint64_t) now + 17));
                emitOff (s, now); S.note = -1; S.tremolo = false; S.autoBend = 0; S.autoBendLeft = 0; S.chBendSemis = 0;
                break;
            }
            case Ev::muteAll: case Ev::bendIn: case Ev::step: break;     // (marcadores, resueltos arriba)
            default: break;
        }
    }
};

} // namespace dh::gt
