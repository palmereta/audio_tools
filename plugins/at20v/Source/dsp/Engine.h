#pragma once
// Motor del AT20V: una voz es un MS-20 entero (todos los módulos + la patchera). El modo mono
// es el del instrumento original; el modo poli replica el MS-20 completo por voz.
//
// Convenciones eléctricas (docs/hardware-spec.md §2.1):
//   - Niveles: 1.0 = 5 V. Audio de los VCO ±1 (10 Vpp).
//   - Teclado y VCO CV IN en Hz/V (lineal): 1 V = 32,7 Hz en 8' (C1 = 1 V, C4 = 8 V). El
//     portamento actúa sobre esa tensión, así que el glissando se curva como en el original.
//   - FREQ y TOTAL a 1 V/oct; los CUTOFF FREQ a 2 oct/V.
//   - Disparos S-trig: salidas en +5 V en reposo que van a masa al activarse; entradas que se
//     activan al llevarlas a masa.
// Los cables se evalúan con una muestra de retardo (a 4x de sobremuestreo son 5 µs), lo que
// permite cualquier lazo de realimentación sin ordenar módulos.

#include "Common.h"
#include "Fx.h"
#include "Korg35.h"
#include "Modules.h"
#include "Oscillators.h"
#include "Oversampling.h"
#include "ui/Layout.generated.h"

#include <array>
#include <cstring>
#include <vector>

namespace dh {

using KnobId = layout::KnobId;
using JackId = layout::JackId;
constexpr int kNumKnobs = (int) KnobId::Count;
constexpr int kNumJacks = (int) JackId::Count;
constexpr int kMaxVoices = 12;
constexpr float kHzPerVolt = 32.7032f;       // C1

// Posiciones de perilla: continuas en 0..1 (dial 0..10 / 10); selectores 0..steps-1.
using KnobValues = std::array<float, kNumKnobs>;

// Para cada jack de entrada, el índice del jack de salida conectado o -1 (queda la normalización).
struct PatchState
{
    std::array<int8_t, kNumJacks> src;
    PatchState() { src.fill (-1); }
};

struct Settings
{
    int voices = 1;            // 1 = mono (MS-20)
    int priority = 1;          // 0 última, 1 aguda (MS-20), 2 grave
    bool retrigger = false;    // el MS-20 dispara una sola vez mientras haya teclas apretadas
    int filterRev = 1;         // 1 = Korg35 (original temprano y mini), 2 = LM13600
    float analog = 0.35f;      // deriva de afinación, dispersión entre voces y soplido
    float bendRange = 2.0f;    // semitonos (el MS-20 no tiene pitch bend: es una comodidad)
    float wheel = 0.0f;        // rueda de control −1..+1 (sólo sale por su jack)
    bool momentary = false;    // pulsador momentáneo (sólo sale por su jack)
    float outputGain = 1.0f;
    int unison = 1;            // voces apiladas por nota
    float detuneCents = 12.0f; // desafinación total del unísono
    float spread = 0.6f;       // apertura estéreo (unísono y poli)
    bool mgSync = false;       // MG sincronizado al tempo del host (FREQUENCY elige la división)
    double bpm = 120.0, ppq = 0.0;
    bool playing = false;
};

// Etapas posteriores (vocoder, delay, reverb): no son del MS-20.
struct FxSettings
{
    bool vocOn = false; int vocBands = 16; float vocShift = 0, vocQ = 0.5f, vocAttack = 0.005f, vocRelease = 0.06f;
    float vocHiss = 0.4f, vocMix = 1.0f, vocInGain = 1.0f;
    bool dlyOn = false; float dlyBeats = 0.75f, dlyFeedback = 0.35f, dlyTone = 0.6f, dlyMix = 0.25f;
    bool revOn = false; float revSize = 0.6f, revDamp = 0.4f, revMix = 0.25f;
};

// Disparo del bus de teclado registrado por la voz (para MIDI OUT "Patch").
struct TrigEvent { int pos; bool on; float hz; };

// ───────────── Calibración: perilla (0..1) → magnitud física ─────────────
namespace cal {
inline float expRange (float t, float lo, float hi) { return lo * std::pow (hi / lo, clampf (t, 0.0f, 1.0f)); }
// Tiempos: manual de servicio, hasta 10 s (hold 20 s). Ley cúbica (pote de audio): con ella las
// cartas de fábrica dan tiempos creíbles (DECAY 2 ≈ 80 ms en el Synthe Bass, ATTACK 1,5 ≈ 35 ms
// en el Violin). La ley exacta del original no está medida (spec §3.4).
inline float cubic (float t, float lo, float hi) { t = clampf (t, 0.0f, 1.0f); return lo + (hi - lo) * t * t * t; }
inline float attack (float t)  { return cubic (t, 0.0008f, 10.0f); }
inline float decay (float t)   { return cubic (t, 0.002f, 10.0f); }
inline float delay (float t)   { return t < 0.005f ? 0.0f : cubic (t, 0.0f, 10.0f); }
inline float hold (float t)    { return t < 0.005f ? 0.0f : cubic (t, 0.0f, 20.0f); }
// MG: 0,1 Hz abajo; ley cuadrática para que 4–5 caiga en 4–6 Hz (vibrato de las cartas).
inline float mgHz (float t)    { t = clampf (t, 0.0f, 1.0f); return 0.1f + 25.0f * t * t; }
inline float portamento (float t) { return t < 0.01f ? 0.0f : expRange (t, 0.002f, 2.0f); }
// Corte: ajuste log por 50 Hz @0, 500 Hz @5 (calibración de fábrica), 15 kHz @10.
inline float cutoffOct (float t)
{
    const float k = clampf (t, 0.0f, 1.0f) * 10.0f - 5.0f;
    return std::log2 (500.0f) + 0.823f * k + 0.0318f * k * k;
}
inline float peakK (float t)   { return 2.3f * std::pow (clampf (t, 0.0f, 1.0f), 1.15f); }
inline float level (float t)   { return std::pow (t, 1.4f); }
inline float volume (float t)  { return 2.0f * t * t; }
inline float fmDepth (float t)  { return 5.0f * t * t; }     // 1 V/oct → 5 oct por unidad a fondo
inline float cutDepth (float t) { return 10.0f * t * t; }    // 2 oct/V → 10 oct por unidad a fondo
inline float vco2PitchOct (float t) { return (t - 0.5f) * 2.0f; }       // ±1 octava
inline float masterTuneOct (float t) { return (t - 0.5f) * (2.0f / 12.0f); } // ±100 cents
inline float espGain (float t) { return expRange (t, 0.1f, 20.0f); }   // nivel de línea → ±2 (10 V) con PEAK al límite
inline float espLowCut (float t)  { return expRange (t, 50.0f, 2500.0f); }
inline float espHighCut (float t) { return expRange (t, 100.0f, 5000.0f); }
// Divisiones de tempo (en negras) para el MG sincronizado y el delay.
constexpr int kNumDivs = 11;
constexpr float syncBeatsTable[kNumDivs] = { 16.0f, 8.0f, 4.0f, 2.0f, 1.0f, 2.0f / 3.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };
inline const char* syncName (int i)
{
    static const char* n[kNumDivs] = { "4 bars", "2 bars", "1 bar", "1/2", "1/4", "1/4 T", "1/8", "1/8 T", "1/16", "1/16 T", "1/32" };
    return n[std::max (0, std::min (kNumDivs - 1, i))];
}
inline int syncIndex (float t) { return std::max (0, std::min (kNumDivs - 1, (int) std::lround (t * (kNumDivs - 1)))); }
constexpr float scale1Oct[4] = { -2.0f, -1.0f, 0.0f, 1.0f };   // 32' 16' 8' 4'
constexpr float scale2Oct[4] = { -1.0f, 0.0f, 1.0f, 2.0f };    // 16' 8' 4' 2'
} // namespace cal

class Voice
{
public:
    void prepare (float osRate, uint32_t seed)
    {
        fs = osRate;
        rng = Rng (seed);
        noise.rng = Rng (seed * 7919u + 13u);
        vco1.rng = Rng (seed * 104729u + 7u);
        esp.prepare (fs);
        dcOut.setCutoff (4.0f, fs);
        driftLp.setCutoff (0.3f, fs);
        driftLp2.setCutoff (0.23f, fs);
        staticDetune1 = rng.bipolar(); staticDetune2 = rng.bipolar();
        mg.phase = rng.unipolar();
        vco1.core.phase = rng.unipolar(); vco2.core.phase = rng.unipolar();
        hpf.mode = Ms20Filter::Highpass; lpf.mode = Ms20Filter::Lowpass;
        hpf.reset(); lpf.reset();
        outs.fill (0.0f);
        outs[(size_t) JackId::kbd_trig_out] = 1.0f;
        outs[(size_t) JackId::esp_trig_out] = 1.0f;
        outs[(size_t) JackId::msw_out] = 1.0f;
        targetV = currentV = noteVolts (60);
    }

    static float noteVolts (int note)
    {
        return 440.0f * std::exp2 ((float) (note - 69) / 12.0f) / kHzPerVolt;
    }

    void noteOn (int note, bool retrig)
    {
        targetV = noteVolts (note);
        if (! active) currentV = targetV;
        gate = true;
        if (retrig) gateRetrig = true;
        active = true;
        lastNote = note;
    }
    void glideTo (int note) { targetV = noteVolts (note); lastNote = note; }
    void noteOff() { gate = false; }
    void hardStop() { gate = gateRetrig = pendingNoteOn = active = false; }     // corta en seco (prepare, NaN)
    bool isActive() const { return active; }
    bool gateOn() const { return gate; }
    int note() const { return lastNote; }

    // Procesa n muestras sobremuestreadas y las suma en out. espIn puede ser nullptr.
    void process (float* outL, float* outR, const float* espIn, int n, const KnobValues& kv, const PatchState& patch,
                  const Settings& st, float bendSemis)
    {
        numEvents = 0;
        auto K = [&] (KnobId id) { return kv[(size_t) id]; };
        auto patched = [&] (JackId j) { return patch.src[(size_t) j] >= 0; };
        auto in = [&] (JackId j, float normal) {
            const int s = patch.src[(size_t) j];
                if (s < 0) return normal;
            // Rieles de ±15 V como el hardware: una realimentación por la patchera satura en vez de crecer sin
            // límite, y un NaN que entre por un cable no se propaga.
            return railV (outs[(size_t) s]);
        };

        hpf.revision = lpf.revision = st.filterRev;

        const int w1 = (int) K (KnobId::vco1_wave), w2 = (int) K (KnobId::vco2_wave);
        const float pw = K (KnobId::vco1_pw);
        const float sc1 = cal::scale1Oct[std::min (3, (int) K (KnobId::vco1_scale))];
        const float sc2 = cal::scale2Oct[std::min (3, (int) K (KnobId::vco2_scale))];
        const float tune = cal::masterTuneOct (K (KnobId::master_tune)) + bendSemis / 12.0f;
        const float p2 = cal::vco2PitchOct (K (KnobId::vco2_pitch));
        const float lv1 = cal::level (K (KnobId::vco1_level)), lv2 = cal::level (K (KnobId::vco2_level));
        const float fmMg = cal::fmDepth (K (KnobId::fm_mg)), fmEg = cal::fmDepth (K (KnobId::fm_eg1));
        const float hpBase = cal::cutoffOct (K (KnobId::hpf_cutoff));
        const float lpBase = cal::cutoffOct (K (KnobId::lpf_cutoff));
        const float hpK = cal::peakK (K (KnobId::hpf_peak)), lpK = cal::peakK (K (KnobId::lpf_peak));
        const float hpMg = cal::cutDepth (K (KnobId::hpf_mod_mg)), hpEg = cal::cutDepth (K (KnobId::hpf_mod_eg2));
        const float lpMg = cal::cutDepth (K (KnobId::lpf_mod_mg)), lpEg = cal::cutDepth (K (KnobId::lpf_mod_eg2));
        float mgHz = cal::mgHz (K (KnobId::mg_freq));
        if (st.mgSync)
        {
            // MG sincronizado: FREQUENCY elige la división; con el transporte andando la fase sigue
            // la posición del host (cada sub-bloque se realinea).
            const float beats = cal::syncBeatsTable[cal::syncIndex (K (KnobId::mg_freq))];
            mgHz = (float) (st.bpm / 60.0) / beats;
            if (st.playing) { const double ph = st.ppq / beats; mg.phase = (float) (ph - std::floor (ph)); }
        }
        const float mgSym = K (KnobId::mg_wave);
        const float e1d = cal::delay (K (KnobId::eg1_delay)), e1a = cal::attack (K (KnobId::eg1_attack));
        const float e1r = cal::decay (K (KnobId::eg1_release));
        const float e2h = cal::hold (K (KnobId::eg2_hold)), e2a = cal::attack (K (KnobId::eg2_attack));
        const float e2d = cal::decay (K (KnobId::eg2_decay)), e2s = K (KnobId::eg2_sustain);
        const float e2r = cal::decay (K (KnobId::eg2_release));
        const float vol = cal::volume (K (KnobId::volume));
        const float porta = cal::portamento (K (KnobId::portamento));
        const float portaCoef = porta > 0.0f ? 1.0f - std::exp (-1.0f / (porta * fs)) : 1.0f;
        const float espG = cal::espGain (K (KnobId::esp_level));
        const float espLo = cal::espLowCut (K (KnobId::esp_lowcut)), espHi = cal::espHighCut (K (KnobId::esp_highcut));
        const float espCv = K (KnobId::esp_cvadj), espTh = K (KnobId::esp_thresh);
        const float analog = st.analog;
        const float hiss = 0.0012f * analog;
        const float scale1 = std::exp2 (sc1 + tune), scale2 = std::exp2 (sc2 + p2 + tune);
        const float nyq = 0.45f * fs;

        const bool pTotal = patched (JackId::total_in), pFreq = patched (JackId::freq_in);
        esp.trackPitch = false;
        for (int j = 0; j < kNumJacks; ++j) esp.trackPitch = esp.trackPitch || patch.src[(size_t) j] == (int) JackId::esp_fv_out;
        const bool pKTrig = patched (JackId::kbd_trig_in), pE1Trig = patched (JackId::eg1_trig_in);

        for (int i = 0; i < n; ++i)
        {
            // ── Teclado (Hz/V) con portamento sobre la tensión ──
            currentV += (targetV - currentV) * portaCoef;
            const float kbdCv = currentV * 0.2f;
            outs[(size_t) JackId::kbd_cv_out] = kbdCv;
            outs[(size_t) JackId::kbd_trig_out] = gate ? 0.0f : 1.0f;
            outs[(size_t) JackId::wheel_out] = st.wheel;
            outs[(size_t) JackId::msw_out] = st.momentary ? 0.0f : 1.0f;

            // ── Disparos S-trig: TRIG IN reemplaza al teclado (EG1 + EG2); EG 1 TRIG IN sólo EG1 ──
            bool kbdGate = pKTrig ? trigIn.update (in (JackId::kbd_trig_in, 1.0f)) : gate;
            if (gateRetrig) { kbdGate = false; gateRetrig = false; }
            const bool e1Gate = pE1Trig ? e1TrigIn.update (in (JackId::eg1_trig_in, 1.0f)) : kbdGate;
            if (kbdGate && ! prevKbdGate) { eg2.trigger (e2h); pendingNoteOn = true; }
            if (! kbdGate && prevKbdGate) { eg2.release(); if (numEvents < (int) events.size()) events[(size_t) numEvents++] = { i, false, 0.0f }; }
            if (e1Gate && ! prevE1Gate) eg1.trigger (e1d);
            if (! e1Gate && prevE1Gate) eg1.release();
            prevKbdGate = kbdGate; prevE1Gate = e1Gate;

            // ── Moduladores ──
            mg.process (mgHz, mgSym, fs);
            const float e1 = eg1.process (e1a, e1r, fs);
            eg2.setHold (e2h);
            const float e2 = eg2.process (e2a, e2d, e2s, e2r, fs);
            outs[(size_t) JackId::mg_tri_out] = mg.tri * 0.5f;
            outs[(size_t) JackId::mg_sq_out] = mg.pulse > 0.0f ? 1.0f : 0.0f;
            outs[(size_t) JackId::eg1_out] = e1;
            outs[(size_t) JackId::eg1_rev_out] = 1.0f - e1;
            outs[(size_t) JackId::eg2_rev_out] = -e2;
            noise.process();
            outs[(size_t) JackId::white_out] = noise.white * 0.5f;
            outs[(size_t) JackId::pink_out] = noise.pink * 0.8f;

            // S/H y MVCA: sin normalizaciones salvo el control del MVCA (EG1).
            outs[(size_t) JackId::sh_out] = sh.process (in (JackId::sh_in, 0.0f), in (JackId::sh_clock_in, 1.0f));
            outs[(size_t) JackId::vca_out] = in (JackId::vca_in, 0.0f) * clampf (in (JackId::vca_ctrl_in, e1), 0.0f, 1.2f);

            // ── ESP ── (el detector de altura sólo corre si F∝V CV OUT tiene un cable)
            esp.process (in (JackId::esp_in, espIn != nullptr ? espIn[i] : 0.0f), espG, espLo, espHi, espCv, espTh, fs);
            outs[(size_t) JackId::esp_amp_out] = esp.ampOut;
            outs[(size_t) JackId::esp_bpf_out] = esp.bpfOut;
            outs[(size_t) JackId::esp_fv_out] = esp.fvOut;
            outs[(size_t) JackId::esp_env_out] = esp.envOut;
            outs[(size_t) JackId::esp_trig_out] = esp.trigOut;

            // ── VCO: CV lineal (Hz/V) × modulación exponencial (FREQ/TOTAL, 1 V/oct) ──
            const float cv12 = in (JackId::vco12_cv_in, kbdCv);
            const float cv2 = in (JackId::vco2_cv_in, cv12);
            const float total = pTotal ? in (JackId::total_in, 0.0f) : outs[(size_t) JackId::mg_tri_out];
            const float freq = pFreq ? in (JackId::freq_in, 0.0f) : e1;
            const float fmOct = total * fmMg + freq * fmEg;
            const float drift1 = analog * (0.010f * driftLp.process (rng.bipolar() * 6.0f) + 0.003f * staticDetune1);
            const float drift2 = analog * (0.010f * driftLp2.process (rng.bipolar() * 6.0f) + 0.004f * staticDetune2);
            const float expo1 = fastExp2 (fmOct + drift1 + detuneOct), expo2 = fastExp2 (fmOct + drift2 + detuneOct);
            const float f1 = std::max (0.0f, cv12 * 5.0f * kHzPerVolt) * scale1 * expo1;
            const float f2 = std::max (0.0f, cv2 * 5.0f * kHzPerVolt) * scale2 * expo2;
            vco1.core.setFreq (std::min (f1, nyq), fs);
            vco2.core.setFreq (std::min (f2, nyq), fs);
            if (pendingNoteOn)
            {
                pendingNoteOn = false;
                if (numEvents < (int) events.size()) events[(size_t) numEvents++] = { i, true, f1 };
            }
            const float a1 = vco1.process (w1, pw);
            const float a2 = vco2.process (w2, vco1.pulseOut);

            // ── Mezclador + EXT SIGNAL IN → HPF → LPF → VCA ──
            float x = a1 * lv1 + a2 * lv2 + in (JackId::ext_sig_in, 0.0f) + noise.white * hiss;
            const float hpCv = in (JackId::hpf_cut_in, e2);
            const float lpCv = in (JackId::lpf_cut_in, e2);
            const float hpHz = fastExp2 (clampf (hpBase + total * hpMg + hpCv * hpEg, 3.0f, 16.0f));
            const float lpHz = fastExp2 (clampf (lpBase + total * lpMg + lpCv * lpEg, 3.0f, 16.0f));
            x = hpf.process (x, hpHz, hpK, fs);
            x = lpf.process (x, lpHz, lpK, fs);
            // VCA: EG2 + INITIAL GAIN, satura al sumar 5 V.
            const float gain = clampf (e2 + in (JackId::gain_in, 0.0f), 0.0f, 1.0f);
            const float y = dcOut.process (x * gain) * vol;

            outs[(size_t) JackId::signal_out] = y;
            outs[(size_t) JackId::phones_out] = y;
            outL[i] += y * panL;
            outR[i] += y * panR;
        }

        if (! gate && eg2.stage == Eg2::Idle && eg1.stage == Eg1::Idle && ! pKTrig) active = false;
        ledMg = mg.pulse > 0.0f; ledEg = eg1.y;
        ledEspPeak = esp.peak > 1.6f; ledEspTrig = esp.trigOut < 0.5f;
    }

    std::array<float, kNumJacks> outs {};
    bool ledMg = false, ledEspPeak = false, ledEspTrig = false;
    float ledEg = 0.0f;
    float detuneOct = 0.0f, panL = 1.0f, panR = 1.0f;     // los fija el motor (unísono / apertura)
    std::array<TrigEvent, 64> events {};
    int numEvents = 0;
    void setPan (float pan) { panL = std::sqrt (clampf (1.0f - pan, 0.0f, 2.0f)); panR = std::sqrt (clampf (1.0f + pan, 0.0f, 2.0f)); }

private:
    float fs = 192000.0f;
    Rng rng;
    Vco1 vco1; Vco2 vco2; NoiseGen noise;
    Ms20Filter hpf, lpf;
    Eg1 eg1; Eg2 eg2; Mg mg; SampleHold sh; Esp esp;
    STrigInput trigIn, e1TrigIn;
    DcBlocker dcOut;
    OnePoleLP driftLp, driftLp2;
    float staticDetune1 = 0, staticDetune2 = 0;
    float targetV = 8.0f, currentV = 8.0f;
    bool gate = false, gateRetrig = false, active = false;
    bool prevKbdGate = false, prevE1Gate = false, pendingNoteOn = false;
    int lastNote = 60;
};

class Engine
{
public:
    // keepNotes: cambio de calidad en marcha; las teclas apretadas siguen sonando con el nuevo sobremuestreo.
    void prepare (double sampleRate, int maxBlock, int oversample = 4, bool keepNotes = false)
    {
        sr = (float) sampleRate;
        os = oversample;
        osL.assign ((size_t) (maxBlock * os + 8), 0.0f);
        osR.assign ((size_t) (maxBlock * os + 8), 0.0f);
        espOs.assign ((size_t) (maxBlock * os + 8), 0.0f);
        carrier.assign ((size_t) maxBlock + 8, 0.0f);
        modBuf.assign ((size_t) maxBlock + 8, 0.0f);
        vocOut.assign ((size_t) maxBlock + 8, 0.0f);
        for (int v = 0; v < kMaxVoices; ++v) voices[(size_t) v].prepare (sr * (float) os, 0x51F15EEDu + 7919u * (uint32_t) v);
        downL.prepare (os); downR.prepare (os); up.prepare (os);
        vocoder.prepare (sr); delay.prepare (sr); reverb.prepare (sr);
        for (auto& v : voices) v.hardStop();
        for (auto& g : groupNote) g = -1;
        rr = 0;
        if (! keepNotes) heldCount = 0;
        else retriggerHeld();
    }

    int oversampling() const { return os; }

    // ── Asignación: grupos de `unison` voces; mono = un solo grupo ──
    int unisonCount() const { return std::max (1, std::min (4, settings.unison)); }
    int groupCount() const
    {
        const int u = unisonCount();
        return settings.voices <= 1 ? 1 : std::max (1, std::min (settings.voices, kMaxVoices / u));
    }
    int activeVoiceCount() const { return groupCount() * unisonCount(); }

    void noteOn (int note, float /*vel*/)
    {
        checkLayout();
        removeHeld (note);
        if (heldCount < 128) held[heldCount++] = note;
        applyVoiceLayout();
        const int u = unisonCount();
        if (settings.voices <= 1)
        {
            const int target = monoTarget();
            for (int k = 0; k < u; ++k)
            {
                Voice& v = voices[(size_t) k];
                if (! v.gateOn()) v.noteOn (target, false);
                else { v.glideTo (target); if (settings.retrigger) v.noteOn (target, true); }
            }
        }
        else
        {
            const int g = pickGroup (note);
            groupNote[(size_t) g] = note;
            for (int k = 0; k < u; ++k) { Voice& v = voices[(size_t) (g * u + k)]; v.noteOn (note, v.gateOn()); }
        }
    }

    void noteOff (int note)
    {
        checkLayout();
        removeHeld (note);
        const int u = unisonCount();
        if (settings.voices <= 1)
        {
            for (int k = 0; k < u; ++k)
            {
                if (heldCount == 0) voices[(size_t) k].noteOff();
                else voices[(size_t) k].glideTo (monoTarget());
            }
        }
        else
            for (int g = 0; g < groupCount(); ++g)
                if (groupNote[(size_t) g] == note)
                {
                    groupNote[(size_t) g] = -1;
                    for (int k = 0; k < u; ++k) voices[(size_t) (g * u + k)].noteOff();
                }
    }

    void allNotesOff() { heldCount = 0; for (auto& v : voices) v.noteOff(); for (auto& g : groupNote) g = -1; }
    void pitchBend (float minus1to1) { bend = minus1to1; }

    // Renderiza n muestras a la frecuencia del host y las suma en outL/outR. espIn (opcional) es la
    // entrada de audio: alimenta la ESP y es el modulador del vocoder.
    void render (float* outL, float* outR, const float* espIn, int n)
    {
        const int nOs = n * os;
        std::fill (osL.begin(), osL.begin() + nOs, 0.0f);
        std::fill (osR.begin(), osR.begin() + nOs, 0.0f);
        const float* espPtr = nullptr;
        if (espIn != nullptr) { up.process (espIn, espOs.data(), n); espPtr = espOs.data(); }
        checkLayout();
        applyVoiceLayout();
        const int nv = activeVoiceCount();
        const float bendSemis = bend * settings.bendRange;
        const bool selfTrig = patch.src[(size_t) JackId::kbd_trig_in] >= 0;
        numMidiEvents = 0;
        for (int v = 0; v < nv; ++v)
        {
            Voice& voice = voices[(size_t) v];
            // En poli/unísono las voces calladas no se procesan (salvo que la patchera las dispare sola).
            if (v > 0 && ! voice.isActive() && ! (selfTrig && v < unisonCount())) continue;
            voice.process (osL.data(), osR.data(), espPtr, nOs, knobs, patch, settings, bendSemis);
        }
        // Disparos de la voz 0 → eventos MIDI (posición en muestras del host).
        for (int e = 0; e < voices[0].numEvents && numMidiEvents < (int) midiEvents.size(); ++e)
        {
            auto ev = voices[0].events[(size_t) e];
            ev.pos /= os;
            midiEvents[(size_t) numMidiEvents++] = ev;
        }

        // Poli/unísono: 1/√N para que un acorde lleno no sature; mono queda con ~3 dB de margen.
        const float g = settings.outputGain * 0.88f / std::sqrt ((float) nv);
        float* L = outL; float* R = outR;
        std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
        downL.process (osL.data(), L, n, g);
        downR.process (osR.data(), R, n, g);

        if (fx.vocOn)
        {
            vocoder.configure (fx.vocBands, fx.vocShift, fx.vocQ);
            for (int i = 0; i < n; ++i)
            {
                carrier[(size_t) i] = 0.5f * (L[i] + R[i]);
                modBuf[(size_t) i] = espIn != nullptr ? espIn[i] * fx.vocInGain : 0.0f;
            }
            vocoder.process (carrier.data(), modBuf.data(), vocOut.data(), n, fx.vocAttack, fx.vocRelease, fx.vocHiss);
            const float dry = 1.0f - fx.vocMix;
            for (int i = 0; i < n; ++i)
            {
                L[i] = L[i] * dry + vocOut[(size_t) i] * fx.vocMix;
                R[i] = R[i] * dry + vocOut[(size_t) i] * fx.vocMix;
            }
        }
        if (fx.dlyOn) delay.process (L, R, n, fx.dlyBeats * 60.0f / (float) settings.bpm, fx.dlyFeedback, fx.dlyTone, fx.dlyMix);
        if (fx.revOn) reverb.process (L, R, n, fx.revSize, fx.revDamp, fx.revMix);

        // Guardia: si algo se fue a NaN/Inf (una realimentación extrema), se corta en seco y se reinician los
        // estados, en vez de dejar el plugin mudo o trabado hasta recargarlo.
        float sum = 0.0f;
        for (int i = 0; i < n; ++i) sum += L[i] + R[i];
        if (! isFiniteF (sum))
        {
            std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
            resetFx();
            for (int v = 0; v < kMaxVoices; ++v) { voices[(size_t) v].prepare (sr * (float) os, 0x51F15EEDu + 7919u * (uint32_t) v); voices[(size_t) v].hardStop(); }
            for (auto& g : groupNote) g = -1;
            heldCount = 0;
            return;
        }

        // Limitador suave sólo por encima de 0,9 (no colorea el rango normal).
        for (float* ch : { L, R })
            for (int i = 0; i < n; ++i)
            {
                const float x = ch[i], ax = std::fabs (x);
                if (ax > 0.9f) ch[i] = std::copysign (0.9f + 0.1f * fastTanh ((ax - 0.9f) * 10.0f), x);
            }
    }

    // Variante mono (herramientas de prueba): promedia L y R.
    void render (float* out, const float* espIn, int n)
    {
        if ((int) tmpR.size() < n) tmpR.resize ((size_t) n);
        if ((int) tmpL.size() < n) tmpL.resize ((size_t) n);
        render (tmpL.data(), tmpR.data(), espIn, n);
        for (int i = 0; i < n; ++i) out[i] += 0.5f * (tmpL[(size_t) i] + tmpR[(size_t) i]);
    }

    void resetFx() { vocoder.reset(); delay.reset(); reverb.reset(); }

    KnobValues knobs {};
    PatchState patch;
    Settings settings;
    FxSettings fx;
    std::array<Voice, kMaxVoices> voices;
    std::array<TrigEvent, 64> midiEvents {};
    int numMidiEvents = 0;

    // Panel "Normal Setting" del manual (OM p.4), con volumen en 7.
    static KnobValues normalSetting()
    {
        KnobValues k {};
        auto set = [&] (KnobId id, float v) { k[(size_t) id] = v; };
        set (KnobId::vco1_wave, 1); set (KnobId::vco1_pw, 0); set (KnobId::vco1_scale, 2);
        set (KnobId::vco2_wave, 0); set (KnobId::vco2_pitch, 0.5f); set (KnobId::vco2_scale, 1);
        set (KnobId::portamento, 0); set (KnobId::master_tune, 0.5f);
        set (KnobId::vco1_level, 1); set (KnobId::vco2_level, 1);
        set (KnobId::hpf_cutoff, 0); set (KnobId::hpf_peak, 0);
        set (KnobId::lpf_cutoff, 1); set (KnobId::lpf_peak, 0);
        set (KnobId::mg_wave, 0.5f); set (KnobId::mg_freq, 0.5f);
        set (KnobId::eg2_sustain, 1);
        set (KnobId::volume, 0.7f);
        set (KnobId::esp_level, 0.5f); set (KnobId::esp_lowcut, 1.0f); set (KnobId::esp_highcut, 0.0f);
        set (KnobId::esp_cvadj, 1.0f); set (KnobId::esp_thresh, 0.5f);
        return k;
    }

private:
    // Si cambió la cantidad de voces o el unísono, las voces que sonaban con la distribución anterior se sueltan
    // y las teclas que siguen apretadas vuelven a sonar con la nueva (si no, quedaban voces colgadas).
    void checkLayout()
    {
        const int lv = settings.voices <= 1 ? 1 : settings.voices, lu = unisonCount();
        if (lv == layoutVoices && lu == layoutUnison) return;
        const bool first = layoutVoices < 0;
        layoutVoices = lv; layoutUnison = lu;
        if (first) return;
        for (auto& v : voices) v.noteOff();
        for (auto& g : groupNote) g = -1;
        rr = 0;
        retriggerHeld();
    }

    void retriggerHeld()
    {
        if (heldCount == 0) return;
        applyVoiceLayout();
        const int u = unisonCount();
        if (settings.voices <= 1)
        {
            const int target = monoTarget();
            for (int k = 0; k < u; ++k) voices[(size_t) k].noteOn (target, false);
            return;
        }
        const int gN = groupCount();
        for (int i = std::max (0, heldCount - gN); i < heldCount; ++i)
        {
            const int note = held[i], g = pickGroup (note);
            groupNote[(size_t) g] = note;
            for (int k = 0; k < u; ++k) { Voice& v = voices[(size_t) (g * u + k)]; v.noteOn (note, v.gateOn()); }
        }
    }

    // Desafinación y paneo de cada voz según unísono y apertura.
    void applyVoiceLayout()
    {
        const int u = unisonCount(), gN = groupCount();
        for (int g = 0; g < gN; ++g)
            for (int k = 0; k < u; ++k)
            {
                Voice& v = voices[(size_t) (g * u + k)];
                const float pos = u > 1 ? (float) k / (float) (u - 1) * 2.0f - 1.0f : 0.0f;
                v.detuneOct = pos * settings.detuneCents * 0.5f / 1200.0f;
                float pan = u > 1 ? pos * settings.spread : 0.0f;
                if (u == 1 && gN > 1) pan = ((g % 2) ? 1.0f : -1.0f) * settings.spread * (0.25f + 0.5f * (float) ((g / 2) % 2));
                v.setPan (pan);
            }
    }

    int monoTarget() const
    {
        if (heldCount == 0) return 60;
        if (settings.priority == 0) return held[heldCount - 1];
        int best = held[0];
        for (int i = 1; i < heldCount; ++i)
            best = settings.priority == 1 ? std::max (best, held[i]) : std::min (best, held[i]);
        return best;
    }

    void removeHeld (int note)
    {
        int w = 0;
        for (int i = 0; i < heldCount; ++i) if (held[i] != note) held[w++] = held[i];
        heldCount = w;
    }

    int pickGroup (int note)
    {
        const int gN = groupCount(), u = unisonCount();
        rr %= gN;      // el turno puede venir de una distribución con más grupos
        auto leader = [&] (int g) -> Voice& { return voices[(size_t) (g * u)]; };
        for (int g = 0; g < gN; ++g) if (groupNote[(size_t) g] == note) return g;
        for (int k = 0; k < gN; ++k) { const int g = (rr + k) % gN; if (! leader (g).isActive()) { rr = (g + 1) % gN; return g; } }
        for (int k = 0; k < gN; ++k) { const int g = (rr + k) % gN; if (! leader (g).gateOn()) { rr = (g + 1) % gN; return g; } }
        const int g = rr; rr = (rr + 1) % gN; return g;
    }

    float sr = 48000.0f;
    int os = 4;
    std::vector<float> osL, osR, espOs, carrier, modBuf, vocOut, tmpL, tmpR;
    Decimator downL, downR; Interpolator up;
    Vocoder vocoder; StereoDelay delay; PlateReverb reverb;
    int held[128] {}; int heldCount = 0;
    std::array<int, kMaxVoices> groupNote {};
    int rr = 0, layoutVoices = -1, layoutUnison = -1;
    float bend = 0.0f;
};

} // namespace dh
