#pragma once
// Motor del ATMD: la voz del Model D (tres VCO → mezclador que satura → escalera de transistores → VCA con su
// contorno), los controladores (tune, glide, mezcla de modulación del osc 3 y el ruido, rueda de modulación,
// rueda de pitch) y el teclado monofónico con prioridad a la nota más grave y disparo único.
//
// Extensiones que el original no tenía: polifonía y unísono, velocidad y aftertouch, prioridad y modo de
// disparo elegibles, deriva analógica ajustable, delay y reverb.
//
// El audio de la voz (VCO, mezclador, filtro y VCA) corre sobremuestreado: el osc 3 modula a los otros a
// frecuencia de audio, y la escalera y el mezclador saturan. Contornos y glide corren a la frecuencia base.

#include "Common.h"
#include "ContourMd.h"
#include "Fx.h"
#include "LadderMd.h"
#include "Oversampling.h"
#include "VcoMd.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace dh::md {

constexpr float kSqrt2 = 1.41421356f;

// Rangos del selector RANGE: LO, 32', 16', 8', 4', 2'. Semitonos respecto de 8' (8' = altura MIDI).
constexpr float kRangeSemis[6] = { -60.0f, -24.0f, -12.0f, 0.0f, 12.0f, 24.0f };    // LO ≈ 32' − 3 oct [spec §5]
// Nota de referencia del teclado: la F más grave (F2) es 0 V. El seguimiento del filtro y el osc 3 libre parten de ahí.
constexpr float kLowF = 41.0f;

// El panel, en las unidades de sus perillas (0..10, −5..+5) e interruptores.
struct Panel
{
    // Controladores
    float tune = 0.0f;           // ±2.5 semitonos
    float glide = 0.0f;          // 0..10
    float modMix = 0.0f;         // 0 = osc 3, 10 = ruido
    bool oscMod = false, osc3Ctrl = true;
    // Osciladores
    int range[3] = { 3, 3, 3 };  // índice en kRangeSemis
    int wave[3] = { 2, 2, 2 };   // Wave
    float freq2 = 0.0f, freq3 = 0.0f;   // ±7.5 semitonos (osc 3 sin control de teclado: ±3 octavas)
    // Mezclador
    float level[3] = { 7.0f, 0.0f, 0.0f };
    bool on[3] = { true, false, false };
    float extLevel = 0.0f, noiseLevel = 0.0f;
    bool extOn = false, noiseOn = false, noisePink = false;
    // Modificadores
    bool filterMod = false, kbd1 = false, kbd2 = false;
    float cutoff = 0.0f;         // −5..+5
    float emphasis = 0.0f;       // 0..10
    float contourAmt = 5.0f;     // 0..10
    float fA = 0.0f, fD = 4.0f, fS = 5.0f;
    float lA = 0.0f, lD = 4.0f, lS = 8.0f;
    bool decayOn = true, glideOn = false;
    // Salida
    float volume = 6.0f;         // 0..10
    bool mainOn = true, a440 = false;
    float a440Level = 6.0f;      // la perilla PHONES del original: acá fija el nivel del tono de referencia
};

struct Settings
{
    int voices = 1, unison = 1;
    float detuneCents = 8.0f, spread = 0.6f, analog = 0.35f;
    float velFilter = 0.0f, velAmp = 0.0f, atFilter = 0.0f, atMod = 0.0f;
    int priority = 0;            // 0 grave (como el original), 1 última, 2 aguda
    bool multiTrigger = false;   // el original redispara sólo al tocar separado
    float bendSemis = 7.0f;      // la rueda del original da ±7 [spec §2.6]
    float outputGain = 1.0f;
};

struct Fx
{
    bool dlyOn = false; float dlyBeats = 0.75f, dlyFeedback = 0.35f, dlyTone = 0.6f, dlyMix = 0.25f;
    bool revOn = false; float revSize = 0.6f, revDamp = 0.4f, revMix = 0.2f;
};

class Engine
{
public:
    static constexpr int kMaxVoices = 16;

    Panel p;
    Settings st;
    Fx fx;
    float bend = 0.0f;           // −1..+1
    float modWheel = 0.0f;       // 0..1
    float aftertouch = 0.0f;     // 0..1
    bool overload = false;       // la entrada externa satura el preamplificador (la lámpara OVERLOAD)
    double bpm = 120.0;

    void prepare (double sampleRate, int maxBlock, int oversampling)
    {
        fs = (float) sampleRate;
        os = std::max (1, oversampling);
        fsOs = fs * (float) os;
        decL.prepare (os); decR.prepare (os);
        osL.assign ((size_t) (kSub * os), 0.0f);
        osR.assign ((size_t) (kSub * os), 0.0f);
        extOs.assign ((size_t) (kSub * os), 0.0f);
        interp.prepare (os);
        (void) maxBlock;
        for (int v = 0; v < kMaxVoices; ++v)
        {
            auto& vc = voices[(size_t) v];
            for (int o = 0; o < 3; ++o) vc.osc[(size_t) o].seed (0x9e3779b9u * (uint32_t) (v * 3 + o + 1), std::fmod (0.137f * (float) (v * 3 + o), 1.0f));
            vc.noise.seed (0x51ed27u + (uint32_t) v * 131u);
            vc.drift = Rng (0x1234u + (uint32_t) v * 77u);
            vc.vcf.reset(); vc.fEnv.reset(); vc.lEnv.reset();
            vc.dc.setCutoff (6.0f, fsOs); vc.dc.reset();
            vc.cutoffOffset = 0.06f * vc.drift.bipolar();     // cada voz con su filtro un poco distinto
        }
        delay.prepare (fs); reverb.prepare (fs);
        outDc.setCutoff (8.0f, fs); outDcR.setCutoff (8.0f, fs);
        a440Phase = 0.0f;
        allNotesOff (true);
    }

    int oversampling() const { return os; }

    // ───────── teclado ─────────
    void noteOn (int note, float vel)
    {
        note = std::clamp (note, 0, 127);
        removeHeld (note);
        held.push_back (note);
        heldVel[(size_t) note] = clampf (vel, 0.0f, 1.0f);
        sustainedKey[(size_t) note] = false;
        if (isPoly()) { polyOn (note, heldVel[(size_t) note]); return; }
        monoUpdate (true);
    }

    void noteOff (int note)
    {
        note = std::clamp (note, 0, 127);
        if (sustain) { sustainedKey[(size_t) note] = true; return; }
        removeHeld (note);
        if (isPoly()) { polyOff (note); return; }
        monoUpdate (false);
    }

    void setSustain (bool on)
    {
        sustain = on;
        if (on) return;
        for (int n = 0; n < 128; ++n)
            if (sustainedKey[(size_t) n]) { sustainedKey[(size_t) n] = false; removeHeld (n); if (isPoly()) polyOff (n); }
        if (! isPoly()) monoUpdate (false);
    }

    void allNotesOff (bool hard)
    {
        held.clear();
        sustainedKey.fill (false);
        sustain = false;
        for (auto& v : voices)
        {
            v.fEnv.gateOff(); v.lEnv.gateOff(); v.note = -1;
            if (hard) { v.fEnv.reset(); v.lEnv.reset(); v.vcf.reset(); }
        }
        monoNote = -1;
    }

    int activeVoices() const { int n = 0; for (int v = 0; v < kMaxVoices; ++v) n += voices[(size_t) v].lEnv.active() ? 1 : 0; return n; }
    float lastEnvelope() const { return voices[0].lEnv.value(); }

    // ───────── audio ─────────
    // extIn: entrada externa (mono, puede ser nullptr). Escribe L y R (reemplaza).
    void process (const float* extIn, float* L, float* R, int n)
    {
        for (int done = 0; done < n; done += kSub)
        {
            const int len = std::min (kSub, n - done);
            renderChunk (extIn ? extIn + done : nullptr, L + done, R + done, len);
        }
        effects (L, R, n);
    }

private:
    static constexpr int kSub = 32;

    struct Voice
    {
        std::array<Vco, 3> osc;
        Noise noise;
        Ladder vcf;
        Contour fEnv, lEnv;
        DcBlocker dc;
        Rng drift;
        int note = -1;
        float vel = 1.0f, pitch = 60.0f, target = 60.0f;   // altura actual (glide) y destino, en semitonos MIDI
        float driftSemis[3] = { 0, 0, 0 }, driftTarget[3] = { 0, 0, 0 };
        float cutoffOffset = 0.0f, detune = 0.0f, pan = 0.0f;
        int driftCountdown = 0;
        uint32_t age = 0;
    };

    float fs = 48000.0f, fsOs = 96000.0f;
    int os = 2;
    Decimator decL, decR;
    Interpolator interp;
    std::vector<float> osL, osR, extOs;
    std::array<Voice, kMaxVoices> voices;
    std::vector<int> held;
    std::array<float, 128> heldVel {};
    std::array<bool, 128> sustainedKey {};
    bool sustain = false;
    int monoNote = -1;
    uint32_t ageCounter = 0;
    StereoDelay delay;
    PlateReverb reverb;
    DcBlocker outDc, outDcR;
    float a440Phase = 0.0f;
    float volSmooth = 0.0f;

    bool isPoly() const { return st.voices > 1; }
    int unisonCount() const { return std::clamp (st.unison, 1, std::max (1, kMaxVoices / std::max (1, st.voices))); }

    void removeHeld (int note) { held.erase (std::remove (held.begin(), held.end(), note), held.end()); }

    int priorityNote() const
    {
        if (held.empty()) return -1;
        if (st.priority == 1) return held.back();
        return st.priority == 2 ? *std::max_element (held.begin(), held.end()) : *std::min_element (held.begin(), held.end());
    }

    // Mono (y unísono): una nota suena; los contornos se redisparan sólo si no había tecla apretada
    // (disparo único del original) o si se eligió disparo múltiple.
    void monoUpdate (bool pressed)
    {
        const int want = priorityNote();
        const int uni = unisonCount();
        if (want < 0)
        {
            for (int u = 0; u < uni; ++u) { voices[(size_t) u].fEnv.gateOff(); voices[(size_t) u].lEnv.gateOff(); }
            monoNote = -1;
            return;
        }
        const bool wasSilent = monoNote < 0;
        const bool changed = want != monoNote;
        monoNote = want;
        for (int u = 0; u < uni; ++u)
        {
            auto& v = voices[(size_t) u];
            v.note = want;
            v.target = (float) want;
            if (wasSilent && ! p.glideOn) v.pitch = v.target;       // sin glide la primera nota no se desliza
            if (wasSilent || (st.multiTrigger && pressed && changed)) v.vel = heldVel[(size_t) want];
            v.detune = uni > 1 ? st.detuneCents * 0.01f * ((float) u / (float) (uni - 1) - 0.5f) * 2.0f : 0.0f;
            v.pan = uni > 1 ? st.spread * ((float) u / (float) (uni - 1) - 0.5f) * 2.0f : 0.0f;
            const bool retrig = wasSilent || (st.multiTrigger && pressed && changed);
            if (retrig) { v.fEnv.gateOn (true); v.lEnv.gateOn (true); }
        }
    }

    void polyOn (int note, float vel)
    {
        const int uni = unisonCount();
        const int groups = std::max (1, std::min (st.voices, kMaxVoices / uni));
        // Grupo libre, o el más viejo (robo de voz).
        int best = -1; uint32_t bestAge = 0xffffffffu;
        for (int g = 0; g < groups; ++g)
        {
            const auto& v = voices[(size_t) (g * uni)];
            if (v.note == note) { best = g; break; }
            const bool free = ! v.lEnv.active();
            const uint32_t score = free ? 0u : v.age + 1u;
            if (score < bestAge) { bestAge = score; best = g; }
        }
        ++ageCounter;
        for (int u = 0; u < uni; ++u)
        {
            auto& v = voices[(size_t) (best * uni + u)];
            const bool wasActive = v.lEnv.active();
            v.note = note; v.vel = vel; v.age = ageCounter;
            v.target = (float) note;
            if (! wasActive || ! p.glideOn) v.pitch = v.target;
            v.detune = uni > 1 ? st.detuneCents * 0.01f * ((float) u / (float) (uni - 1) - 0.5f) * 2.0f : 0.0f;
            v.pan = uni > 1 ? st.spread * ((float) u / (float) (uni - 1) - 0.5f) * 2.0f : st.spread * 0.3f * (float) ((best % 3) - 1);
            v.fEnv.gateOn (true); v.lEnv.gateOn (true);
        }
    }

    void polyOff (int note)
    {
        for (auto& v : voices)
            if (v.note == note) { v.fEnv.gateOff(); v.lEnv.gateOff(); }
    }

    // GLIDE: pote logarítmico de 5 MΩ en el lazo de seguimiento → de 1 ms a 1 s por octava [spec §3.9].
    static float glidePerOct (float knob) { return 0.001f + 1.0f * audioTaper (knob); }

    void renderChunk (const float* ext, float* L, float* R, int n)
    {
        const int N = n * os;
        // Entrada externa sobremuestreada (va al mezclador como un canal más).
        const bool useExt = ext != nullptr && p.extOn && p.extLevel > 0.0f;
        // El detector de OVERLOAD mira el preamplificador, antes del interruptor del mezclador [spec §2.3].
        bool over = false;
        if (ext != nullptr && p.extLevel > 0.0f)
        {
            const float g = audioTaper (p.extLevel) * 6.0f;
            for (int i = 0; i < n; ++i) if (std::fabs (ext[i] * g) > 0.5f) { over = true; break; }
        }
        if (useExt) interp.process (ext, extOs.data(), n);

        const int uni = unisonCount();
        const int count = isPoly() ? std::min (kMaxVoices, std::max (1, std::min (st.voices, kMaxVoices / uni)) * uni) : uni;

        // Contornos: tiempos del panel.
        const float fA = contourTau (p.fA), fD = contourTau (p.fD);
        const float lA = contourTau (p.lA), lD = contourTau (p.lD);
        // Glide: velocidad casi constante en saltos grandes (el amplificador de error satura) y el final exponencial.
        const bool gl = p.glideOn;
        const float glideRate = gl ? std::min (1.0f, 12.0f / (glidePerOct (p.glide) * fs)) : 1.0f;   // semitonos por muestra

        // Mezclador pasivo: 33 kΩ por oscilador y entrada externa, 12 kΩ el ruido, directo a los transistores de
        // entrada de la escalera. Un oscilador en 10 queda cerca de tanh(0.4) [spec §3.3, §5].
        const float oscW = 0.4f / Ladder::kDrive;
        float lvl[3];
        for (int o = 0; o < 3; ++o) lvl[o] = p.on[o] ? 0.1f * clampf (p.level[o], 0.0f, 10.0f) * oscW : 0.0f;
        const float noiseLvl = p.noiseOn ? 0.1f * clampf (p.noiseLevel, 0.0f, 10.0f) * oscW * (33.0f / 12.0f) * 0.5f : 0.0f;
        // Entrada externa: preamplificador de ganancia 200; con la perilla alta satura y prende OVERLOAD.
        const float extLvl = useExt ? audioTaper (p.extLevel) * 6.0f : 0.0f;
        const float mm = clampf (p.modMix / 10.0f, 0.0f, 1.0f);
        const float wOsc3 = std::cos (mm * 1.5707963f), wNoise = std::sin (mm * 1.5707963f);   // ~0.7 cada uno al medio
        // Rueda de modulación: pote logarítmico en derivación, profundidad casi cúbica con el recorrido [spec §5].
        const float wheel = clampf (modWheel, 0.0f, 1.0f);
        const float modAmt = clampf (wheel * wheel * wheel + st.atMod * aftertouch, 0.0f, 1.0f);
        const bool modOn = modAmt > 1.0e-5f;
        // EMPHASIS: autooscila desde ~8 (k = 4); hasta k ≈ 4.4 en 10 [spec §5].
        const float e = clampf (p.emphasis, 0.0f, 10.0f);
        const float res = e <= 8.0f ? e / 8.0f : 1.0f + 0.1f * (e - 8.0f) / 2.0f;
        const float kbdTrack = (p.kbd1 ? 1.0f / 3.0f : 0.0f) + (p.kbd2 ? 2.0f / 3.0f : 0.0f);
        const float contour = clampf (p.contourAmt / 10.0f, 0.0f, 1.0f) * 6.0f;     // hasta ~6 octavas
        const float bendSemis = bend * st.bendSemis;
        const float tuneSemis = clampf (p.tune, -2.5f, 2.5f);
        const float redCoef = 1.0f - std::exp (-kTwoPi * 100.0f / fsOs);

        std::fill (osL.begin(), osL.begin() + N, 0.0f);
        std::fill (osR.begin(), osR.begin() + N, 0.0f);

        for (int vi = 0; vi < count; ++vi)
        {
            auto& v = voices[(size_t) vi];
            if (! v.lEnv.active() && ! v.fEnv.active()) continue;
            v.fEnv.set (fA, fD, p.fS / 10.0f, p.decayOn, fs);
            v.lEnv.set (lA, lD, p.lS / 10.0f, p.decayOn, fs);
            const float velF = st.velFilter * (v.vel - 0.7f) * 2.0f;            // octavas
            const float velA = 1.0f - st.velAmp * (1.0f - v.vel);
            const float panL = std::sqrt (0.5f * (1.0f - v.pan)), panR = std::sqrt (0.5f * (1.0f + v.pan));

            for (int i = 0; i < n; ++i)
            {
                // Control a la frecuencia base.
                if (! gl) v.pitch = v.target;
                else
                {
                    // Lineal a glideRate semitonos por muestra; dentro del último semitono, exponencial.
                    const float d = v.target - v.pitch;
                    v.pitch += clampf (d * glideRate, -glideRate, glideRate);
                }
                if (--v.driftCountdown <= 0)
                {
                    v.driftCountdown = (int) (fs * 0.05f);
                    for (int o = 0; o < 3; ++o) v.driftTarget[o] = st.analog * 0.06f * v.drift.bipolar();   // ±6 cents con analog = 1
                }
                for (int o = 0; o < 3; ++o) v.driftSemis[o] += (v.driftTarget[o] - v.driftSemis[o]) * 0.0004f;
                const float fe = v.fEnv.process();
                const float le = v.lEnv.process();
                const float keyPitch = v.pitch + bendSemis + tuneSemis + v.detune;
                float semis[3];
                semis[0] = keyPitch + kRangeSemis[std::clamp (p.range[0], 0, 5)] + v.driftSemis[0];
                semis[1] = keyPitch + kRangeSemis[std::clamp (p.range[1], 0, 5)] + p.freq2 + v.driftSemis[1];
                // Osc 3 sin control de teclado: no sigue la nota ni el bend (queda como LFO o drone).
                // Osc 3 sin control de teclado: no sigue la nota, el bend ni la modulación; FREQUENCY abarca ±3 oct.
                semis[2] = p.osc3Ctrl ? keyPitch + kRangeSemis[std::clamp (p.range[2], 0, 5)] + p.freq3 + v.driftSemis[2]
                                      : kLowF + tuneSemis + kRangeSemis[std::clamp (p.range[2], 0, 5)] + p.freq3 * (36.0f / 7.5f) + v.driftSemis[2];
                // Corte: una octava por división, −1.1 = 440 Hz sin seguimiento [spec §3.4].
                const float baseCut = 8.7814f + 1.1f + p.cutoff + v.cutoffOffset * st.analog * (1.0f / 0.35f);
                const float cutOct = baseCut + contour * fe + kbdTrack * (v.pitch - kLowF) / 12.0f + velF
                                   + st.atFilter * aftertouch * 2.0f;
                const float amp = le * velA;

                float* bl = osL.data() + i * os;
                float* br = osR.data() + i * os;
                // La modulación a frecuencia de audio sólo pide recalcular afinación y corte en cada muestra
                // sobremuestreada si de verdad llega (rueda arriba y su interruptor encendido); si no, una vez por
                // muestra base. El ruido rosa y rojo, sólo si suena o modula.
                const bool modPitch = modOn && p.oscMod, modCut = modOn && p.filterMod;
                const bool needPink = (p.noiseOn && p.noisePink) || (modOn && wNoise > 0.0f);
                if (! modPitch)
                {
                    v.osc[0].setFreq (440.0f * fastExp2 ((semis[0] - 69.0f) / 12.0f), fsOs);
                    v.osc[1].setFreq (440.0f * fastExp2 ((semis[1] - 69.0f) / 12.0f), fsOs);
                }
                v.osc[2].setFreq (440.0f * fastExp2 ((semis[2] - 69.0f) / 12.0f), fsOs);
                auto setFilter = [&] (float octs) {
                    const float fc = fastExp2 (clampf (octs, 3.3f, 14.6f));
                    // La resonancia se apaga con el corte bajo: deja de oscilar cerca de 150 Hz [spec §3.4].
                    const float lowFade = fc >= 300.0f ? 1.0f : 0.86f + 0.14f * clampf ((fc - 100.0f) / 200.0f, 0.0f, 1.0f);
                    v.vcf.set (fc, res * lowFade, fsOs);
                };
                if (! modCut) setFilter (cutOct);
                for (int k = 0; k < os; ++k)
                {
                    // Modulación: osc 3 ↔ ruido, por la rueda. Va a los osc 1-2 (exponencial) y al filtro.
                    const float o3 = v.osc[2].process (p.wave[2], true);
                    // Ruido: blanco o rosa al audio; rosa o rojo a la modulación, según el mismo interruptor.
                    const float pk = needPink ? v.noise.pink() * 2.2f : 0.0f;
                    const float wh = v.noise.white();
                    const float nz = p.noisePink ? pk : wh;
                    if (modPitch || modCut)
                    {
                        const float nzMod = p.noisePink ? v.noise.red (pk, redCoef) : pk;
                        const float mod = (wOsc3 * o3 + wNoise * nzMod) * modAmt;
                        if (modPitch)
                        {
                            const float pm = mod * 9.0f;                          // rueda a fondo ≈ 1.5 oct pico a pico
                            v.osc[0].setFreq (440.0f * fastExp2 ((semis[0] + pm - 69.0f) / 12.0f), fsOs);
                            v.osc[1].setFreq (440.0f * fastExp2 ((semis[1] + pm - 69.0f) / 12.0f), fsOs);
                            // Con OSC 3 CONTROL encendido el osc 3 también recibe la modulación (se modula a sí mismo
                            // cuando la fuente es él): así lo cablea el original [spec §3.7].
                            if (p.osc3Ctrl) v.osc[2].setFreq (440.0f * fastExp2 ((semis[2] + pm - 69.0f) / 12.0f), fsOs);
                        }
                        if (modCut) setFilter (cutOct + mod * 1.25f);           // ≈ 2.5 oct pico a pico
                    }
                    const float o1 = v.osc[0].process (p.wave[0], false);
                    const float o2 = v.osc[1].process (p.wave[1], false);
                    float mix = lvl[0] * o1 + lvl[1] * o2 + lvl[2] * o3 + noiseLvl * nz;
                    if (useExt)
                    {
                        float ex = extLvl * extOs[(size_t) (i * os + k)];
                        ex = 0.8f * fastTanh (ex * 1.25f);               // el preamplificador recorta
                        mix += ex * oscW * 2.5f;
                    }
                    mix += 2.0e-5f * wh;       // piso de ruido térmico: arranca la autooscilación con el mezclador cerrado
                    float y = v.vcf.process (mix) * amp;
                    y = v.dc.process (y);
                    bl[k] += y * panL;
                    br[k] += y * panR;
                }
            }
        }

        std::fill (L, L + n, 0.0f);
        std::fill (R, R + n, 0.0f);
        // Paneo de potencia constante: una voz al centro sale con √½ por lado; se compensa.
        decL.process (osL.data(), L, n, kSqrt2);
        decR.process (osR.data(), R, n, kSqrt2);
        // A-440: referencia de afinación sumada a la salida.
        overload = over;
        const float vol = p.mainOn ? audioTaper (p.volume) * 12.0f * st.outputGain : 0.0f;
        for (int i = 0; i < n; ++i)
        {
            volSmooth += (vol - volSmooth) * 0.002f;
            float l = outDc.process (L[i]) * volSmooth, r = outDcR.process (R[i]) * volSmooth;
            if (p.a440)
            {
                const float t = 0.4f * audioTaper (p.a440Level) * std::sin (kTwoPi * a440Phase);
                l += t; r += t;
                a440Phase += 440.0f / fs; if (a440Phase >= 1.0f) a440Phase -= 1.0f;
            }
            L[i] = l; R[i] = r;
        }
    }

    void effects (float* L, float* R, int n)
    {
        if (fx.dlyOn) delay.process (L, R, n, (float) (fx.dlyBeats * 60.0 / std::max (20.0, bpm)), fx.dlyFeedback, fx.dlyTone, fx.dlyMix);
        if (fx.revOn) reverb.process (L, R, n, fx.revSize, fx.revDamp, fx.revMix);
        for (int i = 0; i < n; ++i)
        {
            L[i] = isFiniteF (L[i]) ? clampf (L[i], -4.0f, 4.0f) : 0.0f;
            R[i] = isFiniteF (R[i]) ? clampf (R[i], -4.0f, 4.0f) : 0.0f;
        }
    }
};

} // namespace dh::md
