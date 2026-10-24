#pragma once
// Motor del AT42: ocho voces fijas (cuatro drones clásicos, dos Papa Srapa, dos VCO con su ADSR),
// mezclador de 10 canales con paneo, ancho estéreo, doble filtro Polivoks con distorsión, efector
// doble de cartuchos y master. Todas las fuentes de CV (LFO A/B, joystick, secuenciador de 5 pasos,
// seguidor de envolvente, teclado táctil, S&H, envolventes) salen por jacks que se patchean con
// las mismas conexiones internas (normales) del hardware.
//
// Voces y filtros corren sobremuestreados (1x/2x/4x); el efector, a la frecuencia base.

#include "Common.h"
#include "Drone42.h"
#include "Effector42.h"
#include "Jacks42.generated.h"
#include "Modules42.h"
#include "Oversampling.h"
#include "Params42.generated.h"
#include "Vco42.h"
#include <array>

namespace dh {

using Panel42 = std::array<float, kNumP42>;

inline Panel42 panelDefaults42()
{
    Panel42 v {};
    for (int i = 0; i < kNumP42; ++i) v[(size_t) i] = panelParam42 (i).def;
    return v;
}

struct Settings42
{
    float ambient = 0.18f, drift = 0.3f, mismatch = 0.5f, busDrive = 0.35f, vintageNoise = 0.0f, outputGain = 1.0f;
    bool chromatic = false, longRelease = false;
    bool syncLfoA = false, syncLfoB = false, syncPulser = false, kbHostClock = false;
    float lfoABeats = 4.0f, lfoBBeats = 1.0f, pulserBeats = 0.25f;
    // Transporte.
    double bpm = 120.0, ppq = 0.0;
};

struct PatchState42 { std::array<int8_t, jk42::kNumJacks> src; PatchState42() { src.fill (-1); } };

class Engine42
{
public:
    Panel42 p = panelDefaults42();
    Settings42 st;
    PatchState42 patch;
    TouchKeys keys;
    std::array<bool, 7> droneKey {};                   // 1..6 (teclas drone, pantalla o MIDI)
    std::array<float, 4> shade {};                      // sombra de la mano sobre cada orbe (drones 1, 2, 4, 5): d*_hand × 0,95
    float joyX = 0.0f, joyY = 0.0f;                     // posición del joystick, −1..1

    // Estado visible (LEDs, orbes, barras).
    struct View
    {
        std::array<float, 7> droneEnv {}, orbLight {}, orbLed {};
        std::array<bool, 7> shFlash {};
        float egA = 0, egB = 0, lfoA = 0, lfoB = 0, efEnv = 0;
        bool efGate = false, preClip = false;
        int seqStep = 0;
        float seqGate = 0;
    } view;

    void prepare (double sampleRate, int maxBlock, int oversampling)
    {
        fsBase = (float) sampleRate;
        os = std::max (1, oversampling);
        fs = fsBase * (float) os;
        maxBlockBase = maxBlock;
        // Todos los módulos vuelven a nuevos (fases, envolventes, filtros, efectos, azar): una instancia preparada de
        // nuevo después de tocar suena igual que una recién creada (QA en DAWs 2026-10-22; athosttest repro +4,1 dB).
        // Los ajustes (p, st, patch, la secuencia grabada) quedan.
        d1 = d2 = d4 = d5 = ClassicVoice {}; papa3 = papa6 = PapaVoice {};
        vcoA = vcoB = Vco42 {}; egA = egB = Adsr42 {}; fltL = fltR = Polivoks {};
        fxL = EffectEngine42 {}; fxR = EffectEngine42 {};
        lfoA = lfoB = pulser = Lfo42 {}; seq = Seq5 {}; ef = EnvFollower {};
        decL = Decimator {}; decR = Decimator {};
        fxCv = {}; noise = Rng { 5 }; preSig = 0.0f; view = {};
        keys.resetRuntime();
        int seed = 11;
        for (auto* v : classicPtrs()) v->prepare (fs, (uint32_t) (seed += 101));
        papa3.prepare (fs, 0x3333u); papa6.prepare (fs, 0x6666u);
        decL.prepare (os); decR.prepare (os);
        // Lugar para el sobremuestreo más alto: cambiarlo después (restart) no reserva memoria en el hilo de audio.
        osL.assign ((size_t) (maxBlock + 64) * 4, 0.0f);
        osR.assign ((size_t) (maxBlock + 64) * 4, 0.0f);
        fxL.prepare (fsBase, 1.0f); fxR.prepare (fsBase, 1.0f + 0.03f * st.mismatch);
        fxL.setReconstruction (14000.0f); fxR.setReconstruction (13600.0f);
        fltL.reset(); fltR.reset();
        for (auto& d : dcDrone) { d.reset(); d.setCutoff (6.0f, fs); }
        dcL.reset(); dcR.reset(); dcL.setCutoff (4.0f, fsBase); dcR.setCutoff (4.0f, fsBase);
        jv.fill (0.0f);
        gainSnap = gainSnapMaster = true;
        gainCoef = 1.0f - std::exp (-1.0f / (0.004f * fs));          // 4 ms: sin escalones de 32 muestras al automatizar
        masterCoef = 1.0f - std::exp (-1.0f / (0.004f * fsBase));
    }

    // Vuelve a empezar con otro sobremuestreo (o después de un NaN) sin reservar memoria: lo puede llamar el hilo de
    // audio. Las voces y los filtros vuelven a nuevos; el efector se vacía. Las notas apretadas y los ajustes quedan.
    void restart (int oversampling)
    {
        os = std::clamp (oversampling, 1, 4);
        fs = fsBase * (float) os;
        d1 = d2 = d4 = d5 = ClassicVoice {}; papa3 = papa6 = PapaVoice {};
        vcoA = vcoB = Vco42 {}; egA = egB = Adsr42 {}; fltL = fltR = Polivoks {};
        fxL.reset(); fxR.reset();
        lfoA = lfoB = pulser = Lfo42 {}; seq = Seq5 {}; ef = EnvFollower {};
        fxCv = {}; preSig = 0.0f; view = {};
        int seed = 11;
        for (auto* v : classicPtrs()) v->prepare (fs, (uint32_t) (seed += 101));
        papa3.prepare (fs, 0x3333u); papa6.prepare (fs, 0x6666u);
        decL.prepare (os); decR.prepare (os);
        for (auto& d : dcDrone) { d.reset(); d.setCutoff (6.0f, fs); }
        dcL.reset(); dcR.reset();
        jv.fill (0.0f);
        gainSnap = true;
        gainCoef = 1.0f - std::exp (-1.0f / (0.004f * fs));
    }

    int oversampling() const { return os; }

    // L, R: salida; inL/inR: entrada de audio del host (puede ser nullptr). n muestras base.
    void render (float* L, float* R, const float* inL, const float* inR, int n)
    {
        fxR.setMismatch (1.0f + 0.03f * st.mismatch);      // el ajuste actúa en vivo (antes sólo al preparar)
        if ((int) osL.size() < n * os) { osL.resize ((size_t) (n * os)); osR.resize ((size_t) (n * os)); }     // sólo si n > maxBlock
        updateControls();
        const double ppqPerBase = st.bpm / 60.0 / fsBase;
        for (int i = 0; i < n; ++i)
        {
            const float audioIn = (inL != nullptr ? inL[i] : 0.0f) * 0.5f + (inR != nullptr ? inR[i] : (inL != nullptr ? inL[i] : 0.0f)) * 0.5f;
            const double ppq = st.ppq + (double) i * ppqPerBase;
            for (int k = 0; k < os; ++k)
            {
                float l, r;
                tick (audioIn, ppq, l, r);
                osL[(size_t) (i * os + k)] = l;
                osR[(size_t) (i * os + k)] = r;
            }
        }
        std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
        decL.process (osL.data(), L, n, 1.0f);
        decR.process (osR.data(), R, n, 1.0f);

        // Efector doble, BLEND y MASTER.
        const int cartL = (int) p[(size_t) P42::fx_cart_l], cartR = (int) p[(size_t) P42::fx_cart_r];
        fxL.load (cartL, (int) p[(size_t) P42::fx_prog_l]);
        fxR.load (cartR, (int) p[(size_t) P42::fx_prog_r]);
        const float blend = p[(size_t) P42::fx_blend];
        const float master = p[(size_t) P42::master] * p[(size_t) P42::master] * 1.6f;
        if (gainSnapMaster) { masterS = master; gainSnapMaster = false; }
        for (int i = 0; i < n; ++i)
        {
            masterS += (master - masterS) * masterCoef;              // por muestra (antes, escalones cada 32 muestras)
            const float x = clampf (p[(size_t) P42::fx_x] + fxCv[0] * 0.1f, 0.0f, 1.0f);
            const float y = clampf (p[(size_t) P42::fx_y] + fxCv[1] * 0.1f, 0.0f, 1.0f);
            const float z = clampf (p[(size_t) P42::fx_z] + fxCv[2] * 0.1f, 0.0f, 1.0f);
            const float wl = fxL.process (L[i], x, y, z), wr = fxR.process (R[i], x, y * 1.01f, z);
            float l = L[i] * (1.0f - 0.5f * blend) + wl * blend * 1.1f;
            float r = R[i] * (1.0f - 0.5f * blend) + wr * blend * 1.1f;
            if (st.vintageNoise > 0.0f) { l += noise.bipolar() * 0.004f * st.vintageNoise * masterS; r += noise.bipolar() * 0.004f * st.vintageNoise * masterS; }
            // Capacitor de salida: la distorsión asimétrica y los filtros saturados también dejan DC.
            L[i] = softLimit (dcL.process (l) * masterS * st.outputGain);
            R[i] = softLimit (dcR.process (r) * masterS * st.outputGain);
        }
    }

private:
    float fsBase = 48000.0f, fs = 96000.0f;
    int os = 2;
    std::array<float, jk42::kNumJacks> jv {};
    ClassicVoice d1, d2, d4, d5;
    PapaVoice papa3, papa6;
    Vco42 vcoA, vcoB;
    Adsr42 egA, egB;
    Polivoks fltL, fltR;
    EffectEngine42 fxL, fxR;
    Lfo42 lfoA, lfoB, pulser;
    Seq5 seq;
    EnvFollower ef;
    Decimator decL, decR;
    std::vector<float> osL, osR;
    std::array<DcBlocker, 6> dcDrone {};
    DcBlocker dcL, dcR;
    std::array<float, 3> fxCv {};
    Rng noise { 5 };
    float preSig = 0.0f;
    // Coeficientes de control (se recalculan por bloque) y su versión suavizada muestra a muestra.
    std::array<float, 10> panL {}, panR {}, vol {}, panLS {}, panRS {}, volS {};
    float masterS = 0.0f, gainCoef = 0.01f, masterCoef = 0.01f;
    bool gainSnap = true, gainSnapMaster = true;
    int maxBlockBase = 512;

    std::array<ClassicVoice*, 4> classicPtrs() { return { &d1, &d2, &d4, &d5 }; }

    inline float P (P42 id) const { return p[(size_t) id]; }
    inline bool B (P42 id) const { return p[(size_t) id] > 0.5f; }
    inline int I (P42 id) const { return (int) std::lround (p[(size_t) id]); }

    // Valor de una entrada: el cable, o la conexión interna del hardware.
    inline float in (int j, float audioIn) const
    {
        const int s = patch.src[(size_t) j];
        if (s >= 0) return railV (jv[(size_t) s]);      // rieles de ±15 V: los lazos de la patchera saturan
        const int nm = jk42::jacks[j].normal;
        if (nm == jk42::None) return 0.0f;
        if (nm >= jk42::KEY1 && nm <= jk42::KEY6) return droneKey[(size_t) (nm - jk42::KEY1 + 1)] ? 10.0f : 0.0f;
        if (nm == jk42::NOISE3) return papa3.noiseOut * 5.0f;
        if (nm == jk42::NOISE6) return papa6.noiseOut * 5.0f;
        if (nm == jk42::AUDIO_IN) return audioIn * 5.0f;
        // Normal hacia otra entrada (p. ej. CV R ← CV L): sigue al cable que tenga esa entrada.
        if (patch.src[(size_t) nm] >= 0) return railV (jv[(size_t) patch.src[(size_t) nm]]);
        return jv[(size_t) nm];
    }

    void updateControls()
    {
        auto setClassic = [&] (ClassicVoice& v, int base) {
            // base: índice del primer parámetro de la voz (tune1); el orden lo fija panel42.py.
            for (int i = 0; i < 5; ++i)
            {
                v.tune[(size_t) i] = p[(size_t) (base + i)];
                v.mute[(size_t) i] = p[(size_t) (mutes (base) + 2 * i)] > 0.5f;      // MUTE y MOD intercalados por columna
                v.mod[(size_t) i] = p[(size_t) (mutes (base) + 2 * i + 1)] > 0.5f;
            }
            v.chromatic = st.chromatic; v.drift = st.drift; v.longRelease = st.longRelease ? 1.0f : 0.0f;
        };
        // La mano sobre cada orbe: parámetros d*_hand (los escribe el panel o la automatización).
        shade = { 0.95f * P (P42::d1_hand), 0.95f * P (P42::d2_hand), 0.95f * P (P42::d4_hand), 0.95f * P (P42::d5_hand) };
        setClassic (d1, (int) P42::d1_tune1); setClassic (d2, (int) P42::d2_tune1);
        setClassic (d4, (int) P42::d4_tune1); setClassic (d5, (int) P42::d5_tune1);
        d1.volt = P (P42::d1_volt); d1.att = P (P42::d1_att); d1.rls = P (P42::d1_rls); d1.cvAmt = P (P42::d1_cv); d1.hold = B (P42::d1_hold);
        d2.volt = P (P42::d2_volt); d2.att = P (P42::d2_att); d2.rls = P (P42::d2_rls); d2.cvAmt = P (P42::d2_cv); d2.hold = B (P42::d2_hold);
        d4.volt = P (P42::d4_volt); d4.att = P (P42::d4_att); d4.rls = P (P42::d4_rls); d4.cvAmt = P (P42::d4_cv); d4.hold = B (P42::d4_hold);
        d5.volt = P (P42::d5_volt); d5.att = P (P42::d5_att); d5.rls = P (P42::d5_rls); d5.cvAmt = P (P42::d5_cv); d5.hold = B (P42::d5_hold);
        auto setPapa = [&] (PapaVoice& v, P42 rate, P42 fmamt, P42 div, P42 pitch, P42 noise_, P42 att, P42 rls, P42 x10, P42 fm, P42 am, P42 hi, P42 hold) {
            v.rate = P (rate); v.fmAmt = P (fmamt); v.divider = P (div); v.pitch = P (pitch); v.noise = P (noise_);
            v.att = P (att); v.rls = P (rls); v.x10 = B (x10); v.fmOn = B (fm); v.amOn = B (am); v.hi = B (hi); v.hold = B (hold);
            v.longRelease = st.longRelease ? 1.0f : 0.0f;
        };
        setPapa (papa3, P42::d3_rate, P42::d3_fmamt, P42::d3_div, P42::d3_pitch, P42::d3_noise, P42::d3_att, P42::d3_rls, P42::d3_x10, P42::d3_fm, P42::d3_am, P42::d3_hi, P42::d3_hold);
        setPapa (papa6, P42::d6_rate, P42::d6_fmamt, P42::d6_div, P42::d6_pitch, P42::d6_noise, P42::d6_att, P42::d6_rls, P42::d6_x10, P42::d6_fm, P42::d6_am, P42::d6_hi, P42::d6_hold);
        for (auto* v : classicPtrs()) v->updateBlock();
        papa3.updateBlock(); papa6.updateBlock();
        for (int c = 0; c < 10; ++c)
        {
            const float pan = p[(size_t) ((int) P42::mix_pan1 + 2 * c)];
            const float v = p[(size_t) ((int) P42::mix_vol1 + 2 * c)];
            panL[(size_t) c] = std::cos (pan * 1.5707963f);
            panR[(size_t) c] = std::sin (pan * 1.5707963f);
            vol[(size_t) c] = v * v * 1.4f;
        }
        if (gainSnap) { volS = vol; panLS = panL; panRS = panR; gainSnap = false; gainSnapMaster = true; }
    }

    static int mutes (int tuneBase)
    {
        // Botones MUTE y MOD de cada voz, en el orden de panel42.py (botones después de perillas).
        if (tuneBase == (int) P42::d1_tune1) return (int) P42::d1_mute1;
        if (tuneBase == (int) P42::d2_tune1) return (int) P42::d2_mute1;
        if (tuneBase == (int) P42::d4_tune1) return (int) P42::d4_mute1;
        return (int) P42::d5_mute1;
    }

    inline void tick (float audioIn, double ppq, float& outL, float& outR)
    {
        using namespace jk42;
        // ── fuentes de CV ──
        const float speedA = I (P42::lfoa_speed) == 0 ? 1.0f / 6.0f : (I (P42::lfoa_speed) == 2 ? 10.0f : 1.0f);
        const float speedB = I (P42::lfob_speed) == 0 ? 1.0f / 6.0f : (I (P42::lfob_speed) == 2 ? 10.0f : 1.0f);
        jv[lfoa_out] = st.syncLfoA ? lfoA.processSynced (ppq, st.lfoABeats, P (P42::lfoa_wave))
                                   : lfoA.process (expMap (P (P42::lfoa_rate), 0.02f, 5.0f) * speedA, P (P42::lfoa_wave), fs);
        jv[lfob_out] = st.syncLfoB ? lfoB.processSynced (ppq, st.lfoBBeats, P (P42::lfob_wave))
                                   : lfoB.process (expMap (P (P42::lfob_rate), 0.2f, 40.0f) * speedB, P (P42::lfob_wave), fs);
        jv[joy_xout] = 5.0f * joyX + (P (P42::joy_offx) - 0.5f) * 10.0f;
        jv[joy_yout] = 5.0f * joyY + (P (P42::joy_offy) - 0.5f) * 10.0f;
        jv[seq_clkout] = st.syncPulser ? (pulser.processSynced (ppq, st.pulserBeats, 1.0f) > 5.0f ? 10.0f : -10.0f)
                                       : (pulser.process (expMap (P (P42::seq_pulser), 0.3f, 30.0f), 1.0f, fs) > 5.0f ? 10.0f : -10.0f);
        {
            const std::array<float, 5> steps { P (P42::seq_step1), P (P42::seq_step2), P (P42::seq_step3), P (P42::seq_step4), P (P42::seq_step5) };
            const std::array<bool, 5> gates { B (P42::seq_gate1), B (P42::seq_gate2), B (P42::seq_gate3), B (P42::seq_gate4), B (P42::seq_gate5) };
            seq.process (in (seq_clkin, audioIn), 3 + I (P42::seq_stages), steps, gates);
            jv[seq_cvout] = seq.cv; jv[seq_gateout] = seq.gate;
        }
        // Preamp (la entrada del host o lo que llegue al jack) y seguidor de envolvente.
        const float pre = in (pre_in, audioIn) * 0.2f * std::pow (10.0f, P (P42::pre_gain) * 2.0f);
        preSig = pre;
        ef.process (pre, P (P42::ef_att), P (P42::ef_rls), fs);
        jv[ef_env] = clampf (ef.env * 10.0f, 0.0f, 10.0f);
        jv[ef_gate] = ef.gate ? 8.0f : 0.0f;
        // Teclado táctil.
        keys.process (in (kb_clkin, audioIn), in (kb_reset, audioIn), ppq, st.kbHostClock, (float) st.bpm, fs);
        jv[kb_voct] = keys.voct; jv[kb_gatel] = keys.gateL; jv[kb_gater] = keys.gateR; jv[kb_press] = keys.pressureVolts();
        // Efector y filtros: entradas de CV.
        fxCv = { in (fx_cvx, audioIn), in (fx_cvy, audioIn), in (fx_cvz, audioIn) };

        // ── voces drone ──
        const float s1 = d1.process (in (d1_gate, audioIn), in (d1_cvin, audioIn), st.ambient, shade[0]);
        const float s2 = d2.process (in (d2_gate, audioIn), in (d2_cvin, audioIn), st.ambient, shade[1]);
        const float s4 = d4.process (in (d4_gate, audioIn), in (d4_cvin, audioIn), st.ambient, shade[2]);
        const float s5 = d5.process (in (d5_gate, audioIn), in (d5_cvin, audioIn), st.ambient, shade[3]);
        jv[d1_env] = d1.env * 10.0f; jv[d2_env] = d2.env * 10.0f; jv[d4_env] = d4.env * 10.0f; jv[d5_env] = d5.env * 10.0f;
        const float s3 = papa3.process (in (d3_gate, audioIn), in (d3_fmin, audioIn), in (d3_shin, audioIn), in (d3_shclk, audioIn));
        const float s6 = papa6.process (in (d6_gate, audioIn), in (d6_fmin, audioIn), in (d6_shin, audioIn), in (d6_shclk, audioIn));
        jv[d3_modout] = papa3.modOut; jv[d3_env] = papa3.env * 10.0f; jv[d3_shout] = papa3.shOut;
        jv[d6_modout] = papa6.modOut; jv[d6_env] = papa6.env * 10.0f; jv[d6_shout] = papa6.shOut;

        // ── VCO A / B con su ADSR y VCA ──
        const float a = vcoA.process (I (P42::vcoa_oct), P (P42::vcoa_tune), in (vcoa_voct, audioIn), in (vcoa_fm, audioIn), P (P42::vcoa_fm),
                                      I (P42::vcoa_wave), clampf (P (P42::vcoa_pw) + P (P42::vcoa_shape) * in (vcoa_shape, audioIn) * 0.1f, 0.0f, 1.0f), 0.0f, fs);
        jv[vcoa_out] = a * 5.0f;
        const float b = vcoB.process (I (P42::vcob_oct), P (P42::vcob_tune), in (vcob_voct, audioIn), in (vcob_fm, audioIn), P (P42::vcob_fm),
                                      I (P42::vcob_wave), clampf (P (P42::vcob_pw) + P (P42::vcob_shape) * in (vcob_shape, audioIn) * 0.1f, 0.0f, 1.0f),
                                      patch.src[vcob_sync] >= 0 ? in (vcob_sync, audioIn) : 0.0f, fs);
        jv[ega_out] = 8.0f * egA.process (in (ega_gate, audioIn) > 1.0f, B (P42::ega_loop), P (P42::ega_a), P (P42::ega_d), P (P42::ega_s), P (P42::ega_r), fs);
        jv[egb_out] = 8.0f * egB.process (in (egb_gate, audioIn) > 1.0f, B (P42::egb_loop), P (P42::egb_a), P (P42::egb_d), P (P42::egb_s), P (P42::egb_r), fs);
        const float vcaA = B (P42::ega_hold) ? 1.0f : clampf (in (ega_vca, audioIn) / 8.0f, 0.0f, 1.2f);
        const float vcaB = B (P42::egb_hold) ? 1.0f : clampf (in (egb_vca, audioIn) / 8.0f, 0.0f, 1.2f);

        // ── mezclador de 10 canales ──
        // Las entradas del mezclador van acopladas en alterna, como en el original: los negistores
        // dan una onda de relajación unipolar y sin el capacitor llegaba un DC constante al bus.
        const float ch[10] = { dcDrone[0].process (s1), dcDrone[1].process (s2), dcDrone[2].process (s3), audioIn,
                               a * vcaA * 0.9f, b * vcaB * 0.9f, clampf (preSig, -1.5f, 1.5f),
                               dcDrone[3].process (s4), dcDrone[4].process (s5), dcDrone[5].process (s6) };
        float l = 0.0f, r = 0.0f;
        for (int c = 0; c < 10; ++c)
        {
            // Volumen y paneo siguen al bloque muestra a muestra (con el valor quieto dan exactamente lo mismo que antes).
            volS[(size_t) c] += (vol[(size_t) c] - volS[(size_t) c]) * gainCoef;
            panLS[(size_t) c] += (panL[(size_t) c] - panLS[(size_t) c]) * gainCoef;
            panRS[(size_t) c] += (panR[(size_t) c] - panRS[(size_t) c]) * gainCoef;
            const float v = ch[c] * volS[(size_t) c]; l += v * panLS[(size_t) c]; r += v * panRS[(size_t) c];
        }
        // El bus suma con algo de saturación (lo que los dueños elogian del original).
        const float dr = 0.4f + 2.5f * st.busDrive;
        l = fastTanh (l * dr) / dr * 1.2f; r = fastTanh (r * dr) / dr * 1.2f;
        // Ancho estéreo (extensión): 0 = mono, 0.5 = original, 1 = doble.
        const float w = P (P42::width) * 2.0f;
        const float m = 0.5f * (l + r), sd = 0.5f * (l - r) * w;
        l = m + sd; r = m - sd;

        // ── filtros Polivoks L/R y distorsión ──
        const float cvL = in (flt_cvl, audioIn), cvR = in (flt_cvr, audioIn);
        const bool link = B (P42::flt_link);
        const float fL = 20.0f * fastExp2 (9.9657843f * P (P42::flt_freq_l)) * fastExp2 (cvL * P (P42::flt_mod_l));
        const float fR = 20.0f * fastExp2 (9.9657843f * P (link ? P42::flt_freq_l : P42::flt_freq_r)) * fastExp2 (cvR * P (P42::flt_mod_r))
                       * (1.0f + 0.03f * st.mismatch);
        const float resL = P (P42::flt_res_l), resR = P (link ? P42::flt_res_l : P42::flt_res_r) * (1.0f - 0.04f * st.mismatch);
        l = fltL.process (l, fL, resL, B (P42::flt_bp_l), fs);
        r = fltR.process (r, fR, resR, link ? B (P42::flt_bp_l) : B (P42::flt_bp_r), fs);
        l = distort42 (l, P (P42::flt_dist), P (P42::flt_gain));
        r = distort42 (r, P (P42::flt_dist), P (P42::flt_gain));
        outL = l; outR = r;

        // ── estado visible ──
        view.droneEnv = { 0, d1.env, d2.env, papa3.env, d4.env, d5.env, papa6.env };
        view.orbLight = { 0, d1.light, d2.light, 0, d4.light, d5.light, 0 };
        view.orbLed = { 0, d1.ledLevel, d2.ledLevel, 0, d4.ledLevel, d5.ledLevel, 0 };
        view.shFlash[3] = view.shFlash[3] || papa3.shFlash; view.shFlash[6] = view.shFlash[6] || papa6.shFlash;
        view.egA = jv[ega_out] / 8.0f; view.egB = jv[egb_out] / 8.0f;
        view.lfoA = jv[lfoa_out] / 10.0f; view.lfoB = jv[lfob_out] / 10.0f;
        view.efEnv = ef.env; view.efGate = ef.gate; view.preClip = view.preClip || std::fabs (pre) > 1.0f;
        view.seqStep = seq.step; view.seqGate = seq.gate;
    }

    static inline float softLimit (float x)
    {
        const float a = std::fabs (x);
        if (a < 0.85f) return x;
        const float y = 0.85f + 0.15f * fastTanh ((a - 0.85f) / 0.15f);
        return x < 0.0f ? -y : y;
    }
};

} // namespace dh
