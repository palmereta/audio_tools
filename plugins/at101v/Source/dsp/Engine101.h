#pragma once
// Motor del AT101V: la voz del SH-101 (CEM3340 + sub de flip-flops + ruido → IR3109 → BA662) con
// su lógica de teclado (prioridad según el modo de disparo, portamento AUTO/ON, HOLD), el modulador
// que también es el reloj, el arpegiador y el secuenciador de 100 pasos.
//
// Extensiones: polifonía y unísono, velocidad, acento/slide/probabilidad por paso, reloj del host
// con swing y largo de gate, octavas de arpegio, drive, chorus, delay y reverb.
//
// Audio sobremuestreado (VCO + VCF + VCA) y control a la frecuencia base.

#include "ArpSeq.h"
#include "Common.h"
#include "Fx.h"
#include "Ir3109.h"
#include "Modules101.h"
#include "Oversampling.h"
#include "Panel101.h"
#include "Vco101.h"

namespace dh {

struct Settings101
{
    int voices = 1, unison = 1;
    float detuneCents = 10.0f, spread = 0.5f, analog = 0.3f;
    float velVcf = 0.0f, velVca = 0.0f, accent = 0.5f, slideTime = 0.06f;
    int clockSrc = 0;              // 0 = modulador (como el 101), 1 = tempo del host
    float clockBeats = 0.25f;      // largo de paso con el host (en negras)
    float swing = 0.0f, gateLen = 0.5f;
    int arpOctaves = 1;
    bool seqKeyTranspose = true;
    float drive = 0.0f;
    float outputGain = 1.0f;
    // Transporte (lo escribe el procesador en cada bloque).
    double bpm = 120.0, ppq = 0.0;
    bool hostPlaying = false;
};

struct Fx101
{
    int chorus = 0; float chorusMix = 0.5f;
    bool dlyOn = false; float dlyBeats = 0.75f, dlyFeedback = 0.35f, dlyTone = 0.6f, dlyMix = 0.25f;
    bool revOn = false; float revSize = 0.6f, revDamp = 0.4f, revMix = 0.2f;
};

struct MidiOutEvent { int pos; bool on; int note; int vel; };

class Engine101
{
public:
    static constexpr int kMaxVoices = 16;

    PanelValues p = panelDefaults();
    Settings101 st;
    Fx101 fx;
    Sequence seq;                         // dueño: el hilo de audio (el procesador la publica)
    bool seqChanged = false;              // la carga desde el teclado modificó la secuencia

    // Performance (la mueven el procesador o el MIDI).
    float bender = 0.0f;                  // palanca −1..+1
    float lfoPush = 0.0f;                 // empuje de la palanca / rueda de modulación 0..1

    // Estado visible.
    bool rateLed = false, loading = false, keyTrArmed = false;
    int keyTranspose = 0, seqPos = -1, soundingNote = -1;

    std::array<MidiOutEvent, 64> midiEvents {};
    int numMidiEvents = 0;
    bool flushPending = false;            // prepare() dejó notas abiertas en el MIDI out: el procesador llama a flushOut()
    int curSample = 0;                    // posición (en el bloque) de los eventos que se generan

    void prepare (double sampleRate, int maxBlock, int oversampling)
    {
        fs = (float) sampleRate;
        os = oversampling;
        fsOs = fs * (float) os;
        interpolatorsUnused = maxBlock;
        decL.prepare (os); decR.prepare (os);
        osL.assign ((size_t) (kSub * os), 0.0f);
        osR.assign ((size_t) (kSub * os), 0.0f);
        lfo.prepare (fs);
        for (int v = 0; v < kMaxVoices; ++v)
        {
            auto& vc = voices[(size_t) v];
            vc.vco.seed (0x9e3779b9u * (uint32_t) (v + 1), std::fmod (0.13f * (float) v, 1.0f));   // fase en [0,1): con 16 voces 0.13·v pasaba de 1 y la voz arrancaba con un golpe de DC
            vc.vcf.reset(); vc.env.reset();
            vc.dc.r = 1.0f - 2.0f * 3.14159265f * 8.0f / fsOs;
            vc.driftRng = Rng (0x1234u + (uint32_t) v * 77u);
        }
        chorus.prepare (fs);
        delay.prepare (fs);
        reverb.prepare (fs);
        held.reserve (64); latched.reserve (64); arpKeys.reserve (64);
        resetNotes();
        if (std::any_of (outOn.begin(), outOn.end(), [] (bool b) { return b; })) flushPending = true;     // los note-off salen en el próximo bloque (los eventos de acá se pierden)
    }

    int oversampling() const { return os; }

    // ───────── teclado ─────────
    void keyDown (int note, float vel)
    {
        note = std::clamp (note, 0, 127);
        if (keyTrArmed)
        {
            keyTranspose = std::clamp (note - 60, -12, 24);
            keyTrArmed = false;
            return;
        }
        if (seqPlaying() && st.seqKeyTranspose && ! loading)
        {
            seqTranspose = note - 60;     // la tecla transpone la secuencia que corre
            return;
        }
        if (loading) loadNote (note);

        checkLayout();
        removeHeld (note);
        held.push_back (note);
        heldVel[(size_t) note] = vel;
        lastVel = vel;
        sustained[(size_t) note] = false;     // vuelve a estar apretada: el pedal ya no la suelta

        if (arpActive() && ! loading)
        {
            if (hold && latchReleased) { latched.clear(); latchReleased = false; }
            if (std::find (latched.begin(), latched.end(), note) == latched.end()) latched.push_back (note);
            if (held.size() == 1 && (! hold || latched.size() == 1))
            {
                // CLK RST: el reloj arranca con la tecla y la primera nota sale ya. Con el reloj del host en
                // marcha, la primera nota espera a la grilla (si no, sale corrida y corta).
                arp.restart();
                if (st.clockSrc == 0) { lfo.reset(); clockStep(); }
                else if (! st.hostPlaying) clockStep();
            }
            return;
        }
        if (isPoly()) { polyOn (note, vel, false, false); return; }
        monoResolve (true);
    }

    void keyUp (int note)
    {
        note = std::clamp (note, 0, 127);
        checkLayout();
        const bool was = removeHeld (note);
        if (! was) return;
        if (arpActive() && ! loading)
        {
            if (! hold) latched.erase (std::remove (latched.begin(), latched.end(), note), latched.end());
            if (held.empty())
            {
                latchReleased = true;
                if (! hold) stopArpNote();
            }
            return;
        }
        if (isPoly())
        {
            if (hold) sustained[(size_t) note] = true;
            else polyOff (note);
            return;
        }
        monoResolve (false);
    }

    // Panic / CC123: suelta todo y cierra las notas abiertas del MIDI out.
    void allNotesOff()
    {
        resetNotes();
        flushOut();
    }
    void flushMidiIfPending() { if (flushPending) flushOut(); }
    void resetNotes()
    {
        held.clear(); latched.clear();
        std::fill (sustained.begin(), sustained.end(), false);
        for (auto& v : voices) { v.gate = false; v.env.gateOff(); }
        for (auto& s : slots) s = {};
        monoNote = -1; monoGate = false;
        arpNote = -1; seqNote = -1; seqGateOpen = false;
        soundingNote = -1;
    }

    // ───────── comandos de los botones ─────────
    void setLoading (bool on)
    {
        if (on == loading) return;
        loading = on;
        if (on) { seq.length = 0; tieNext = false; seqChanged = true; }
    }
    void addRest()
    {
        if (! loading || seq.length >= Sequence::kMaxSteps) return;
        SeqStep s; s.rest = true; s.note = (int8_t) (seq.length > 0 ? seq.steps[(size_t) seq.length - 1].note : 60);
        seq.steps[(size_t) seq.length++] = s;
        seqChanged = true;
        if (seq.length >= Sequence::kMaxSteps) loading = false;
    }
    void markTie() { if (loading) tieNext = true; }
    void armKeyTranspose() { keyTrArmed = ! keyTrArmed; }

    // ───────── render ─────────
    // L, R: n muestras a la frecuencia base (se escriben, no se suman).
    // Los eventos MIDI de salida se acumulan (también los de keyDown/keyUp); el procesador los lee
    // y pone numMidiEvents en 0. eventPos es la posición de los que se generan fuera de render().
    void render (float* L, float* R, int n)
    {
        int done = 0;
        while (done < n)
        {
            const int len = std::min (kSub, n - done);
            renderSub (L + done, R + done, len, done);
            done += len;
        }
        // Efectos a la frecuencia base.
        if (fx.chorus > 0) chorus.process (L, R, n, fx.chorus, fx.chorusMix);
        if (fx.dlyOn)
        {
            const float sec = (float) (fx.dlyBeats * 60.0 / std::max (20.0, st.bpm));
            delay.process (L, R, n, sec, fx.dlyFeedback, fx.dlyTone, fx.dlyMix);
        }
        if (fx.revOn) reverb.process (L, R, n, fx.revSize, fx.revDamp, fx.revMix);
        for (int i = 0; i < n; ++i)
        {
            L[i] = softLimit (L[i] * st.outputGain);
            R[i] = softLimit (R[i] * st.outputGain);
        }
    }

    int activeVoiceCount() const
    {
        int c = 0;
        for (auto& v : voices) if (v.active) ++c;
        return c;
    }

private:
    static constexpr int kSub = 32;

    struct Voice
    {
        Vco101 vco; Ir3109 vcf; Adsr env; DcBlocker dc;
        bool gate = false, active = false, accent = false;
        int note = 60, slot = 0;
        float cv = 60.0f, target = 60.0f, vel = 0.8f, vca = 0.0f, prevVca = 0.0f;
        float glideTau = 0.0f, glideTauCached = -1.0f, glideCoef = 1.0f, detune = 0.0f, pan = 0.0f;
        float drift = 0.0f, driftTarget = 0.0f; Rng driftRng { 1 };
        float cutPrev = 1000.0f, pwPrev = 0.5f, hzPrev = 100.0f;
    };
    struct Slot { int note = -1; bool gate = false; uint32_t age = 0; int outNote = -1; };

    float fs = 48000.0f, fsOs = 192000.0f;
    int os = 4, interpolatorsUnused = 0;
    std::array<Voice, kMaxVoices> voices {};
    std::array<Slot, kMaxVoices> slots {};
    uint32_t ageCounter = 0;
    Lfo101 lfo;
    float sh = 0.0f;                    // valor del RANDOM (S&H del CPU)
    Decimator decL, decR;
    std::vector<float> osL, osR;
    Chorus101 chorus; StereoDelay delay; PlateReverb reverb;
    Arp arp;

    std::vector<int> held, latched, arpKeys;
    std::array<float, 128> heldVel {};
    std::array<bool, 128> sustained {};
    float lastVel = 0.8f;
    bool hold = false, latchReleased = false, tieNext = false;
    int monoNote = -1, monoOutNote = -1; bool monoGate = false;
    std::array<bool, 128> outOn {};       // notas abiertas en el MIDI out (lo que efectivamente salió)
    int layoutKey = -1;
    long long sampleCount = 0;            // muestras desde el arranque (la deriva no depende del bloque)
    int arpNote = -1, seqNote = -1, seqTranspose = 0;
    bool seqGateOpen = false, seqTiePending = false;
    bool prevSeqPlay = false; int prevArp = 0;
    // Reloj del host.
    long long lastHostStep = -1; bool hostGateOffPending = false; double hostGateOffAt = 0.0;

    // ── modos ──
    int arpMode() const { return (int) std::lround (p[(size_t) P::arp_mode]); }
    bool arpActive() const { return arpMode() != 0 && ! seqPlaying(); }
    bool seqPlaying() const { return p[(size_t) P::seq_play] > 0.5f && seq.length > 0; }
    int trigMode() const { return (int) std::lround (p[(size_t) P::env_trig]); }    // 0 G+T, 1 GATE, 2 LFO
    int portaMode() const { return (int) std::lround (p[(size_t) P::porta_mode]); } // 0 AUTO, 1 OFF, 2 ON
    bool isPoly() const { return st.voices > 1; }
    int numSlots() const { return isPoly() ? std::min (st.voices, kMaxVoices / std::max (1, st.unison)) : 1; }

    bool removeHeld (int note)
    {
        auto it = std::find (held.begin(), held.end(), note);
        if (it == held.end()) return false;
        held.erase (it);
        return true;
    }

    // note ya es la nota que sale (con el transpose aplicado al momento del note-on).
    void midiOut (bool on, int note, int vel)
    {
        note = std::clamp (note, 0, 127);
        if (! on && ! outOn[(size_t) note]) return;
        outOn[(size_t) note] = on;
        if (numMidiEvents < (int) midiEvents.size())
            midiEvents[(size_t) numMidiEvents++] = { curSample, on, note, vel };
    }
    static int outNoteOf (int note, int transpose) { return std::clamp (note + transpose, 0, 127); }

    // Al cambiar voces, unísono o mono/poly con teclas apretadas: se suelta lo que sonaba con la distribución
    // anterior (con sus note-off) y las teclas apretadas vuelven a sonar con la nueva.
    void checkLayout()
    {
        const int key = isPoly() ? numSlots() * 16 + st.unison : st.unison;
        if (key == layoutKey) return;
        const bool first = layoutKey < 0;
        layoutKey = key;
        if (first) return;
        if (monoNote >= 0 && monoGate) midiOut (false, monoOutNote, 0);
        for (auto& sl : slots) { if (sl.gate && sl.outNote >= 0) midiOut (false, sl.outNote, 0); sl = {}; }
        for (auto& v : voices) if (v.gate) { v.gate = false; v.env.gateOff(); }
        monoNote = -1; monoGate = false; arpNote = -1; soundingNote = -1;
        std::fill (sustained.begin(), sustained.end(), false);
        if (arpActive() || seqPlaying() || held.empty()) return;
        if (isPoly()) for (int k : held) polyOn (k, heldVel[(size_t) k], false, false);
        else monoResolve (true);
    }

    void flushOut()
    {
        for (int k = 0; k < 128; ++k) if (outOn[(size_t) k]) midiOut (false, k, 0);
        flushPending = false;
    }

    // ── carga de la secuencia desde el teclado ──
    void loadNote (int note)
    {
        if (seq.length >= Sequence::kMaxSteps) { loading = false; return; }
        SeqStep s; s.note = (int8_t) std::clamp (note, 0, 127);
        if (tieNext && seq.length > 0) seq.steps[(size_t) seq.length - 1].tie = true;
        tieNext = false;
        seq.steps[(size_t) seq.length++] = s;
        seqChanged = true;
        if (seq.length >= Sequence::kMaxSteps) loading = false;
    }

    // ── mono: prioridad, portamento y redisparo como el 101 ──
    int monoSelect() const
    {
        if (held.empty()) return -1;
        if (trigMode() == 0) return held.back();                         // última nota
        return *std::min_element (held.begin(), held.end());              // nota más grave
    }

    void monoResolve (bool pressed)
    {
        int n = monoSelect();
        const bool legato = monoGate && n >= 0;
        if (n < 0)
        {
            if (hold && monoNote >= 0) return;                            // HOLD sostiene la última
            monoRelease();
            return;
        }
        if (n == monoNote && monoGate && ! pressed) return;
        const bool glide = portaMode() == 2 || (portaMode() == 0 && legato);
        monoPlay (n, heldVel[(size_t) n], legato, glide, false);
    }

    void monoPlay (int note, float vel, bool legato, bool glide, bool accent, bool slide = false)
    {
        const bool retrig = ! legato || (trigMode() == 0 && ! slide);
        if (! legato && trigMode() == 2 && st.clockSrc == 0) lfo.reset();
        if (monoNote >= 0 && monoGate) midiOut (false, monoOutNote, 0);
        monoOutNote = outNoteOf (note, keyTranspose);
        midiOut (true, monoOutNote, (int) std::lround (std::clamp (vel, 0.05f, 1.0f) * 127.0f));
        monoNote = note; monoGate = true; soundingNote = note;
        for (int u = 0; u < st.unison; ++u)
        {
            auto& v = voices[(size_t) u];
            startVoice (v, note, vel, retrig, glide, accent, slide, u);
        }
        for (int u = st.unison; u < kMaxVoices; ++u) if (voices[(size_t) u].gate) { voices[(size_t) u].gate = false; voices[(size_t) u].env.gateOff(); }
    }

    void monoRelease()
    {
        if (monoNote >= 0 && monoGate) midiOut (false, monoOutNote, 0);
        monoGate = false; soundingNote = -1;
        for (auto& v : voices) if (v.gate) { v.gate = false; v.env.gateOff(); }
        monoNote = -1;
    }

    void startVoice (Voice& v, int note, float vel, bool retrig, bool glide, bool accent, bool slide, int unisonIndex)
    {
        const bool fresh = ! v.active;
        v.note = note; v.target = (float) note; v.vel = vel; v.accent = accent;
        if (fresh || (! glide && ! slide)) v.cv = (float) note;
        v.glideTau = slide ? std::max (st.slideTime, cal101::portaTau (p[(size_t) P::portamento]))
                           : (glide ? cal101::portaTau (p[(size_t) P::portamento]) : 0.0f);
        const int u = unisonIndex, nu = st.unison;
        v.detune = nu > 1 ? (st.detuneCents / 100.0f) * ((float) u / (float) (nu - 1) - 0.5f) * 2.0f * 0.5f : 0.0f;
        v.pan = nu > 1 ? st.spread * ((float) u / (float) (nu - 1) * 2.0f - 1.0f) : 0.0f;
        if (fresh) { v.vcf.reset(); v.vca = v.prevVca = 0.0f; }
        v.active = true;
        v.env.gateOn (retrig || ! v.gate);
        v.gate = true;
    }

    // ── poly ──
    void polyOn (int note, float vel, bool accent, bool slide)
    {
        const int ns = numSlots();
        int s = -1;
        for (int i = 0; i < ns; ++i) if (slots[(size_t) i].note == note) { s = i; break; }
        if (s < 0) for (int i = 0; i < ns; ++i) if (! slots[(size_t) i].gate && ! voices[(size_t) (i * st.unison)].active) { s = i; break; }
        if (s < 0) for (int i = 0; i < ns; ++i) if (! slots[(size_t) i].gate) { s = i; break; }
        if (s < 0)
        {
            uint32_t best = UINT32_MAX;
            for (int i = 0; i < ns; ++i) if (slots[(size_t) i].age < best) { best = slots[(size_t) i].age; s = i; }
            if (slots[(size_t) s].note >= 0) midiOut (false, slots[(size_t) s].outNote, 0);
        }
        else if (slots[(size_t) s].gate && slots[(size_t) s].outNote >= 0) midiOut (false, slots[(size_t) s].outNote, 0);     // re-ataque de la misma nota
        const int prevNote = slots[(size_t) s].note;
        const bool anyHeld = std::any_of (slots.begin(), slots.begin() + ns, [] (const Slot& x) { return x.gate; });
        const bool glide = portaMode() == 2 || (portaMode() == 0 && anyHeld);
        slots[(size_t) s] = { note, true, ++ageCounter, outNoteOf (note, keyTranspose) };
        midiOut (true, slots[(size_t) s].outNote, (int) std::lround (std::clamp (vel, 0.05f, 1.0f) * 127.0f));
        for (int u = 0; u < st.unison; ++u)
        {
            auto& v = voices[(size_t) (s * st.unison + u)];
            v.slot = s;
            if (glide && prevNote >= 0) v.cv = (float) prevNote;
            startVoice (v, note, vel, true, glide && prevNote >= 0, accent, slide, u);
        }
        soundingNote = note;
    }

    void polyOff (int note)
    {
        sustained[(size_t) note] = false;
        for (int i = 0; i < numSlots(); ++i)
            if (slots[(size_t) i].note == note && slots[(size_t) i].gate)
            {
                slots[(size_t) i].gate = false;
                midiOut (false, slots[(size_t) i].outNote, 0);
                for (int u = 0; u < st.unison; ++u)
                {
                    auto& v = voices[(size_t) (i * st.unison + u)];
                    v.gate = false; v.env.gateOff();
                }
            }
    }

    // ── notas generadas (arpegio / secuencia) ──
    void genOn (int note, float vel, bool legato, bool accent, bool slide)
    {
        if (isPoly())
        {
            if (legato && slide) { polyMove (note, vel, accent); return; }
            polyOn (note, vel, accent, slide);
            return;
        }
        const bool glide = portaMode() == 2 || slide;
        monoPlay (note, vel, legato, glide, accent, slide);
    }

    void genOff (int note)
    {
        if (isPoly()) polyOff (note);
        else if (monoNote == note) monoRelease();
    }

    void polyMove (int note, float vel, bool accent)
    {
        // Slide en poly: la última voz se mueve a la nota nueva sin redisparo.
        int s = -1; uint32_t best = 0;
        for (int i = 0; i < numSlots(); ++i) if (slots[(size_t) i].gate && slots[(size_t) i].age >= best) { best = slots[(size_t) i].age; s = i; }
        if (s < 0) { polyOn (note, vel, accent, true); return; }
        midiOut (false, slots[(size_t) s].outNote, 0);
        slots[(size_t) s].outNote = outNoteOf (note, keyTranspose);
        midiOut (true, slots[(size_t) s].outNote, (int) std::clamp (vel * 127.0f, 1.0f, 127.0f));
        slots[(size_t) s].note = note;
        for (int u = 0; u < st.unison; ++u) startVoice (voices[(size_t) (s * st.unison + u)], note, vel, false, true, accent, true, u);
    }

    void stopArpNote()
    {
        if (arpNote >= 0) { genOff (arpNote); arpNote = -1; }
    }

    // ── reloj ──
    void clockStep()
    {
        sh = lfo.rng.bipolar();                       // RANDOM: valor nuevo en cada flanco
        if (trigMode() == 2)
            for (auto& v : voices) if (v.gate) v.env.gateOn (true);

        if (seqPlaying())
        {
            stepSequence();
            return;
        }
        if (arpActive())
        {
            const auto& src = hold ? latched : held;
            if (src.empty()) { stopArpNote(); return; }
            // Una sola tecla sin HOLD (y sin octavas extra) suena normal: no se redispara.
            if (src.size() == 1 && st.arpOctaves <= 1 && arpNote == src[0] && ! hold) return;
            const int n = arp.next (src, arpMode(), st.arpOctaves);
            if (n < 0) return;
            const float vel = heldVel[(size_t) std::clamp (src.back(), 0, 127)];
            // En el 101 el gate queda alto durante el arpegio: cada nota es legato y el ENV se
            // redispara según el modo (GATE+TRIG sí, GATE no).
            const bool legato = arpNote >= 0 && ! isPoly();
            if (isPoly() && arpNote >= 0) genOff (arpNote);
            genOn (n, vel, legato, false, false);
            arpNote = n;
        }
    }

    void stepSequence()
    {
        seqPos = (seqPos + 1) % std::max (1, seq.length);
        const auto& s = seq.steps[(size_t) seqPos];
        bool play = ! s.rest;
        if (play && s.prob < 100) play = (int) (lfo.rng.unipolar() * 100.0f) < (int) s.prob;
        const int note = std::clamp ((int) s.note + seqTranspose, 0, 127);
        if (! play)
        {
            if (seqNote >= 0) { genOff (seqNote); seqNote = -1; }
            seqTiePending = false; seqGateOpen = false;
            return;
        }
        const bool legato = seqTiePending && seqNote >= 0;
        const float vel = s.accent ? 1.0f : 0.75f;
        if (legato && isPoly()) { if (s.slide) genOn (note, vel, true, s.accent, true); else { genOff (seqNote); genOn (note, vel, false, s.accent, false); } }
        else if (legato) genOn (note, vel, true, s.accent, s.slide);
        else
        {
            if (seqNote >= 0) genOff (seqNote);
            genOn (note, vel, false, s.accent, false);
        }
        seqNote = note;
        // El gate sigue alto si el paso está ligado o si el siguiente entra con slide.
        seqTiePending = s.tie || (seqPos + 1 < seq.length ? seq.steps[(size_t) seqPos + 1].slide : seq.steps[0].slide);
        seqGateOpen = true;
    }

    void clockGateOff()
    {
        // Fin del gate de un paso de secuencia (salvo ligado). El arpegio deja el gate alto.
        if (seqPlaying() && seqGateOpen && ! seqTiePending && seqNote >= 0)
        {
            genOff (seqNote); seqNote = -1; seqGateOpen = false;
        }
    }

    void updateModes()
    {
        const bool sp = p[(size_t) P::seq_play] > 0.5f;
        if (sp != prevSeqPlay)
        {
            prevSeqPlay = sp;
            if (sp)
            {
                if (! isPoly()) { monoRelease(); }
                stopArpNote();
                seqPos = -1; seqTranspose = 0; seqTiePending = false;
                if (st.clockSrc == 0) lfo.reset();
                lastHostStep = -1;
                if (seq.length > 0 && st.clockSrc == 0) clockStep();
            }
            else
            {
                if (seqNote >= 0) { genOff (seqNote); seqNote = -1; }
                seqPos = -1;
            }
        }
        const int am = arpMode();
        if (am != prevArp)
        {
            if (am == 0 || prevArp == 0)
            {
                stopArpNote();
                if (am == 0)
                {
                    // Al apagar el arpegio, las teclas apretadas suenan normal.
                    if (! isPoly()) monoResolve (true);
                    else for (int k : held) polyOn (k, heldVel[(size_t) k], false, false);
                }
                else
                {
                    if (! isPoly()) monoRelease();
                    else
                    {
                        for (auto& sl : slots) { if (sl.gate && sl.outNote >= 0) midiOut (false, sl.outNote, 0); sl.gate = false; }
                        for (auto& v : voices) { v.gate = false; v.env.gateOff(); }
                    }
                    latched = held;
                    arp.restart();
                    if (! held.empty()) clockStep();
                }
            }
            prevArp = am;
        }
        const bool h = p[(size_t) P::hold] > 0.5f;
        if (h != hold)
        {
            hold = h;
            if (hold) { latched = held; latchReleased = held.empty(); }
            else
            {
                latched = held;
                if (arpActive() && held.empty()) stopArpNote();
                if (isPoly()) { for (int n = 0; n < 128; ++n) if (sustained[(size_t) n]) polyOff (n); }
                else if (! arpActive() && ! seqPlaying()) monoResolve (false);
            }
        }
    }

    // ── un sub-bloque ──
    void renderSub (float* L, float* R, int n, int offset)
    {
        checkLayout();
        updateModes();
        const float lfoHz = cal101::lfoHz (p[(size_t) P::lfo_rate]);
        const int lfoWave = (int) std::lround (p[(size_t) P::lfo_wave]);
        const int range = (int) std::lround (p[(size_t) P::vco_range]);
        const int trn = (int) std::lround (p[(size_t) P::transpose]);
        const int pwmSrc = (int) std::lround (p[(size_t) P::pwm_src]);
        const int subWave = (int) std::lround (p[(size_t) P::sub_wave]);
        const bool vcaGate = p[(size_t) P::vca_mode] > 0.5f;
        const float pwAmt = p[(size_t) P::pw];
        const float mPulse = sq (p[(size_t) P::mix_pulse]), mSaw = sq (p[(size_t) P::mix_saw]);
        const float mSub = sq (p[(size_t) P::mix_sub]), mNoise = sq (p[(size_t) P::mix_noise]) * 1.6f;
        const float res = cal101::resK (p[(size_t) P::vcf_res]);
        const float envOct = 10.0f * p[(size_t) P::vcf_env];
        const float modOct = 3.0f * p[(size_t) P::vcf_mod];
        const float kybd = p[(size_t) P::vcf_kybd];
        const float vcoMod = 12.0f * cal101::vcoModOct (p[(size_t) P::vco_mod]);
        const float tuneSt = (p[(size_t) P::tune] - 0.5f);
        const float vol = sq (p[(size_t) P::volume]) * 1.8f;
        const float baseCut = std::log2 (cal101::cutoffHz (p[(size_t) P::vcf_freq]));
        const float aC = rcCoef (cal101::attackTau (p[(size_t) P::env_a]), fs);
        const float dC = rcCoef (cal101::decayTau (p[(size_t) P::env_d]), fs);
        const float rC = rcCoef (cal101::releaseTau (p[(size_t) P::env_r]), fs);
        const float sLvl = p[(size_t) P::env_s];
        const float bendSt = bender * 12.0f * p[(size_t) P::bend_vco];
        const float bendOct = bender * 2.0f * p[(size_t) P::bend_vcf];
        const float pushAmt = lfoPush * p[(size_t) P::bend_lfo];
        const float pitchOffset = (float) ((range - 1) * 12 + (trn - 1) * 12 + keyTranspose) + tuneSt + bendSt;
        const float drive = 0.55f * (1.0f + 3.0f * st.drive);
        const float gateSlew = rcCoef (0.0015f, fs);
        const float driftCoef = rcCoef (1.5f, fs);
        const double ppqPerSample = st.bpm / 60.0 / fs;

        std::fill (osL.begin(), osL.begin() + n * os, 0.0f);
        std::fill (osR.begin(), osR.begin() + n * os, 0.0f);

        for (int i = 0; i < n; ++i)
        {
            curSample = offset + i;
            // Modulador y reloj.
            const float prevPhase = lfo.phase;
            lfo.process (lfoHz, fs);
            rateLed = lfo.sq > 0.0f;
            if (st.clockSrc == 0)
            {
                if (lfo.rise) clockStep();
                if (prevPhase < st.gateLen && lfo.phase >= st.gateLen) clockGateOff();
            }
            else
            {
                const double q = st.ppq + (double) curSample * ppqPerSample;
                const double stepLen = std::max (1.0e-3, (double) st.clockBeats);
                const long long idx = (long long) std::floor (q / stepLen);
                const double frac = q / stepLen - (double) idx;
                const double start = (idx & 1) ? st.swing * 0.5 : 0.0;
                if (idx != lastHostStep && frac >= start)
                {
                    if (lastHostStep >= 0 || frac < 0.5)
                    {
                        if (seqPlaying() && st.hostPlaying) seqPos = (int) ((idx - 1) % std::max (1, seq.length) + std::max (1, seq.length)) % std::max (1, seq.length);
                        clockStep();
                        hostGateOffPending = true;
                        hostGateOffAt = start + (1.0 - start) * st.gateLen;
                    }
                    lastHostStep = idx;
                }
                if (hostGateOffPending && idx == lastHostStep && frac >= hostGateOffAt) { hostGateOffPending = false; clockGateOff(); }
            }
            float lfoOut;
            switch (lfoWave)
            {
                case 0:  lfoOut = lfo.tri; break;
                case 1:  lfoOut = lfo.sq; break;
                case 2:  lfoOut = sh; break;
                default: lfoOut = lfo.noise; break;
            }
            const float pushTri = lfo.tri * pushAmt;

            for (int vi = 0; vi < kMaxVoices; ++vi)
            {
                auto& v = voices[(size_t) vi];
                if (! v.active) continue;
                // Portamento: lag exponencial sobre el CV (en semitonos).
                if (v.glideTau > 0.0f) { if (v.glideTau != v.glideTauCached) { v.glideTauCached = v.glideTau; v.glideCoef = rcCoef (v.glideTau, fs); } v.cv += (v.target - v.cv) * v.glideCoef; }
                else v.cv = v.target;
                // Deriva analógica lenta.
                if (((sampleCount + i) & 255) == 0) v.driftTarget = v.driftRng.bipolar() * st.analog * 0.04f;
                v.drift += (v.driftTarget - v.drift) * driftCoef;

                const float e = v.env.process (aC, dC, sLvl, rC);
                const float accentBoost = v.accent ? st.accent : 0.0f;
                const float velF = 1.0f - st.velVcf + st.velVcf * v.vel;

                const float semis = v.cv + pitchOffset + vcoMod * lfoOut + 0.75f * pushTri + v.detune + v.drift;
                const float hz = std::min (20000.0f, 440.0f * fastExp2 ((semis - 69.0f) / 12.0f));

                float pwv;
                if (pwmSrc == 1) pwv = 0.5f - 0.47f * pwAmt;
                else
                {
                    const float m = pwmSrc == 0 ? 0.5f * (lfo.tri + 1.0f) : e;
                    pwv = 0.5f - 0.5f * pwAmt * m;
                }
                const float keyOct = ((float) v.note + (float) ((trn - 1) * 12) - 60.0f) / 12.0f;
                float cutOct = baseCut + envOct * e * velF * (1.0f + accentBoost) + modOct * lfoOut + kybd * keyOct
                             + bendOct + pushTri + v.drift * 0.5f;
                const float cut = clampf (fastExp2 (cutOct), 8.0f, std::min (20000.0f, 0.45f * fsOs));

                float vcaT = vcaGate ? (v.gate ? 1.0f : 0.0f) : e;
                vcaT *= (1.0f - st.velVca + st.velVca * v.vel) * (1.0f + 0.6f * accentBoost);
                const float vPrev = v.vca;
                v.vca = vcaGate ? v.vca + (vcaT - v.vca) * gateSlew : vcaT;

                v.vco.setFreq (hz, fsOs);
                const float gl = 0.5f - 0.5f * v.pan, gr = 0.5f + 0.5f * v.pan;
                float* ol = osL.data() + i * os;
                float* orr = osR.data() + i * os;
                const float invOs = 1.0f / (float) os;
                for (int j = 0; j < os; ++j)
                {
                    const float t = (float) (j + 1) * invOs;
                    v.vco.process (v.pwPrev + (pwv - v.pwPrev) * t, subWave);
                    const float mix = mPulse * v.vco.pulse + mSaw * v.vco.saw + mSub * v.vco.sub + (mNoise + 2.0e-4f) * v.vco.noise;   // piso de ruido: arranca la autooscilación
                    const float fc = v.cutPrev + (cut - v.cutPrev) * t;
                    float y = v.vcf.process (mix * drive, fc, res, fsOs, 1.0f, 0.5f) / 0.55f;
                    y = v.dc.process (y) * (vPrev + (v.vca - vPrev) * t) * vol;
                    ol[j] += y * gl * 1.41f;
                    orr[j] += y * gr * 1.41f;
                }
                v.cutPrev = cut; v.pwPrev = pwv;
                if (! v.gate && v.env.stage == Adsr::Idle && v.vca < 1.0e-5f) { v.active = false; v.vca = 0.0f; }
            }
        }
        std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
        const float uni = 1.0f / std::sqrt ((float) std::max (1, st.unison));
        const float polyG = isPoly() ? 0.7f : 1.0f;
        decL.process (osL.data(), L, n, uni * polyG);
        decR.process (osR.data(), R, n, uni * polyG);
        sampleCount += n;
    }

    static inline float sq (float x) { return x * x; }
    static inline float softLimit (float x)
    {
        const float a = std::fabs (x);
        if (a < 0.8f) return x;
        const float y = 0.8f + 0.2f * fastTanh ((a - 0.8f) * 5.0f);
        return x < 0 ? -y : y;
    }
};

} // namespace dh
