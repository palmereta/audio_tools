#pragma once
// Motor del AT700: consola principal del System-700 (fila de 280 mm: 3 VCO, ruido/ring mod, 2 VCF,
// 2 VCA y módulo de salida; fila de 140 mm: monitor/procesador de tensión, 2 LFO, S&H,
// amplificador/seguidor/integrador, doble ADSR, mezclador y múltiples), teclado 701A y
// secuenciador 717A. Las conexiones internas de fábrica (los "buses") están cableadas como en el
// original; sólo los jacks se patchean.
//
// Polifonía (extensión): los módulos de cada voz (VCO, ring mod, VCF, VCA, ADSR, 707A, 716A,
// múltiples y las salidas del teclado) se duplican por voz; LFO, S&H, ruido, salida, procesador de
// tensión y secuenciador son globales. Un cable de una salida global a una entrada de voz llega a
// todas las voces; uno de una salida de voz a una entrada global suma las voces (si la señal difiere
// por voz) o toma la de la primera.

#include "Common.h"
#include "Jacks700.generated.h"
#include "Keyboard701.h"
#include "Modules700.h"
#include "Oversampling.h"
#include "Params700.generated.h"
#include "Seq717.h"
#include <array>
#include <vector>

namespace dh {

using Panel700 = std::array<float, kNumP700>;

inline Panel700 panelDefaults700()
{
    Panel700 v {};
    for (int i = 0; i < kNumP700; ++i) v[(size_t) i] = panelParam700 (i).def;
    return v;
}

struct Settings700
{
    int vcf1Model = 0;                 // 0: 703E 24 dB (consolas conocidas), 1: 703A 12 dB (manual de 1976)
    int vcf1Mode = 2;                  // modo del 703A: HP, BP, LP
    float drift = 0.15f;               // deriva lenta de los VCO
    float outputGain = 1.0f;
    bool seqHost = false;              // el secuenciador sigue el reloj y el transporte del DAW
    bool seqFollowTransport = true;    // con el host: arranca y para con el transporte
    double bpm = 120.0, ppq = 0.0;
    bool hostPlaying = false;
};

struct PatchState700 { std::array<int16_t, jk700::kNumJacks> src; PatchState700() { src.fill (-1); } };


class Engine700
{
public:
    Panel700 p = panelDefaults700();
    Settings700 st;
    PatchState700 patch;
    Keyboard701 kb;
    Seq717 seq;
    bool manualGate = false;
    float bender = 0.0f;               // −1..1 (palanca + pitch bend MIDI)
    float leverNow = 0.0f;                       // la palanca que suena (parámetro + pitch bend): la dibuja la interfaz

    struct View
    {
        std::array<float, 2> vcfOver {}, vcfSig {}, vcaOver {}, vcaSig {};
        float rmSig = 0, ef = 0, lfo1 = 0, lfo2 = 0, vuL = 0, vuR = 0, pshaper = 0, clock = 0;
        int step = 0, chan = 0;
        bool running = false;
        std::array<bool, 4> master {};
        bool power = true;
    } view;

    void prepare (double sampleRate, int maxBlock, int oversampling)
    {
        fsBase = (float) sampleRate;
        os = std::max (1, oversampling);
        fs = fsBase * (float) os;
        // Los módulos vuelven a nuevos (fases, envolventes, filtros, ruido, LFO, resorte, deriva) y el teclado suelta lo
        // que tenía: una instancia preparada de nuevo después de tocar suena igual que una recién creada (QA en DAWs
        // 2026-10-22; athosttest repro +4,6 dB). Los ajustes, el patch y el 717A (su marcha y sus pasos) quedan.
        for (auto& V : vs) V = VoiceState {};
        noise = Noise708 {}; lfo1 = lfo2 = Lfo706 {}; sh = Sh709 {}; spring = Spring711 {}; pshaper = PulseShaper717 {};
        decL = Decimator {}; decR = Decimator {};
        stdPhase = psLfoPhase = 0.0f; lfo1Delay = 1.0f; vuL = vuR = 0.0f; lastAnyGate = false;
        prevSeqHost = false; expectedPpq = -1.0e9;
        kb.allOff(); for (auto& vv : kb.v) vv = Keyboard701::Voice {};
        decL.prepare (os); decR.prepare (os);
        osL.assign ((size_t) (maxBlock + 64) * (size_t) os, 0.0f);
        osR.assign ((size_t) (maxBlock + 64) * (size_t) os, 0.0f);
        kb.prepare (fsBase);
        spring.prepare (fs);
        for (int v = 0; v < kMaxVoices700; ++v)
        {
            auto& V = vs[(size_t) v];
            for (int n = 0; n < 3; ++n) V.vco[(size_t) n].seed (0.137f * (float) (v * 3 + n));
            V.vcf1o.reset(); V.vcf1s.reset(); V.vcf2.reset();
            V.driftRng = Rng (0x700u + 31u * (uint32_t) v);
        }
        for (auto& r : jv) r.fill (0.0f);
        dcL.reset(); dcR.reset(); phaser = Phaser711 {};     // si quedaron con NaN, no se recuperaban nunca
        lastMaxBlock = maxBlock;
        computePoly();
    }

    int oversampling() const { return os; }

    // Llamar cuando cambia el patch (desde el hilo de audio).
    void computePoly()
    {
        using namespace jk700;
        auto src = [&] (int j) { return (int) patch.src[(size_t) j]; };
        for (int j = 0; j < kNumJacks; ++j)
        {
            const juce_jack& d = jacks[j];
            polyJack[(size_t) j] = d.voice && d.io == Out && ! transparent (j);
        }
        for (int it = 0; it < 6; ++it)
        {
            auto pin = [&] (int j) { const int s = src (j); return s >= 0 && polyJack[(size_t) s]; };
            const bool amp = pin (amp_in), ef = pin (ef_in) || (src (ef_in) < 0 && amp), integ = pin (int_in) || (src (int_in) < 0 && ef);
            polyJack[amp_out] = amp; polyJack[ef_out] = ef; polyJack[int_out] = integ;
            const bool mix = pin (mix_in1) || pin (mix_in2) || pin (mix_in3);
            polyJack[mix_out] = mix; polyJack[mix_inv] = mix;
            for (int c = 0; c < 2; ++c)
            {
                bool any = false;
                for (int r = 0; r < 5; ++r) any = any || pin (multJack (c, r));
                for (int r = 0; r < 5; ++r) polyJack[(size_t) multJack (c, r)] = any;
            }
        }
    }

    void render (float* L, float* R, int n)
    {
        if ((int) osL.size() < n * os) { osL.resize ((size_t) (n * os)); osR.resize ((size_t) (n * os)); }
        updateControls();
        const float dtBase = 1.0f / fsBase;
        const double ppqPerSample = st.bpm / 60.0 / fsBase;
        // Secuenciador con el reloj del host: si el host salta (loop, otra posición) vuelve al paso 1 alineado; si se
        // elige el reloj del host con el transporte ya andando, arranca.
        if (st.seqHost && st.seqFollowTransport && st.hostPlaying)
        {
            if ((hostWasPlaying && expectedPpq > -1.0e8 && std::abs (st.ppq - expectedPpq) > 0.05) || ! prevSeqHost) seq.start();
        }
        prevSeqHost = st.seqHost;
        // El procesador fija la posición del host una vez por bloque y renderiza en trozos: la posición avanza acá
        // con cada trozo, así el trozo siguiente (y el bloque siguiente) no parece un salto del host.
        if (st.hostPlaying) st.ppq += n * ppqPerSample;
        expectedPpq = st.hostPlaying ? st.ppq : -1.0e9;
        for (int i = 0; i < n; ++i)
        {
            baseTick (dtBase, st.seqHost ? ppqPerSample : -1.0);
            for (int k = 0; k < os; ++k)
            {
                float l, r;
                tick (k == 0, l, r);
                osL[(size_t) (i * os + k)] = l;
                osR[(size_t) (i * os + k)] = r;
            }
        }
        std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
        decL.process (osL.data(), L, n, 1.0f);
        decR.process (osR.data(), R, n, 1.0f);
        // Guardia: si algo llegó a NaN/Inf, silencio y estados reiniciados (softLimit lo convertía en +0,7 fijos).
        {
            float sum = 0.0f;
            for (int i = 0; i < n; ++i) sum += L[i] + R[i];
            if (! isFiniteF (sum))
            {
                std::fill (L, L + n, 0.0f); std::fill (R, R + n, 0.0f);
                prepare (fsBase, lastMaxBlock, os);
                return;
            }
        }
        const float mon = monGain * st.outputGain;
        const float vuC = 1.0f - fastExp (-(float) n / (0.3f * fsBase));
        float pkL = 0.0f, pkR = 0.0f;
        for (int i = 0; i < n; ++i)
        {
            powerFade += ((powered ? 1.0f : 0.0f) - powerFade) * 0.002f;
            pkL = std::max (pkL, std::fabs (L[i])); pkR = std::max (pkR, std::fabs (R[i]));
            L[i] = softLimit (dcL.process (L[i]) * 0.11f * mon * powerFade);
            R[i] = softLimit (dcR.process (R[i]) * 0.11f * mon * powerFade);
        }
        vuL += (pkL * 0.25f - vuL) * vuC; vuR += (pkR * 0.25f - vuR) * vuC;
        view.vuL = powered ? vuL : 0.0f; view.vuR = powered ? vuR : 0.0f;
        view.step = seq.step; view.chan = seq.serMode == 3 ? seq.manualChan : seq.chan; view.running = seq.running;
        view.power = powered;
    }

    // Comandos del panel (los llama el procesador entre bloques).
    void seqStart() { seq.start(); }
    void seqContinue() { seq.cont(); }
    void seqStop() { seq.stop(); }
    void seqEnd() { seq.end(); }
    void seqManual (int s) { seq.manual (s); }
    void seqManualChan (int c) { seq.manualChan = juce_clamp (c, 0, 2); if (seq.serMode != 3) seq.chan = seq.manualChan; }

    float jackValue (int j, int v = 0) const { return jv[(size_t) j][(size_t) (jk700::jacks[j].voice ? v : 0)]; }

private:
    using juce_jack = jk700::JackDef;
    static int juce_clamp (int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

    struct VoiceState
    {
        std::array<Vco702, 3> vco;
        Vcf703Ota vcf1o; Vcf703Svf vcf1s, vcf2;
        std::array<Adsr705, 2> adsr;
        EnvFollower707 ef;
        float intY = 0.0f;
        std::array<float, 3> busVco {}, drift {}, driftT {};
        std::array<float, 2> busVcf {}, busVca {}, busAdsr {};
        float busRm = 0.0f;
        Rng driftRng { 1 };
        int driftCount = 0;
        bool trigEdge = false, lastTrig = false;
    };

    float fsBase = 48000.0f, fs = 96000.0f;
    int os = 2;
    std::array<VoiceState, kMaxVoices700> vs;
    std::array<std::array<float, kMaxVoices700>, jk700::kNumJacks> jv {};
    std::array<bool, jk700::kNumJacks> polyJack {};
    Noise708 noise;
    Lfo706 lfo1, lfo2;
    Sh709 sh;
    Spring711 spring;
    Phaser711 phaser;
    int lastMaxBlock = 512;
    bool prevSeqHost = false;
    double expectedPpq = -1.0e9;
    PulseShaper717 pshaper;
    Decimator decL, decR;
    DcBlocker dcL, dcR;
    std::vector<float> osL, osR;
    float stdPhase = 0.0f, psLfoPhase = 0.0f, lfo1Delay = 1.0f, vuL = 0.0f, vuR = 0.0f, powerFade = 1.0f;
    bool lastAnyGate = false, powered = true;
    int nv = 1;

    // ── controles precalculados por bloque ──
    std::array<float, 3> vcoOct {}, vcoPw {}, vcoKey {};
    std::array<std::array<float, 3>, 3> vcoMod {};
    std::array<int, 3> vcoWave {};
    std::array<bool, 3> vcoStrong {};
    float vcf1Oct = 0, vcf2Oct = 0, vcf1K = 0, vcf1Res = 0, vcf2Res = 0, pwmAmt = 0;
    std::array<float, 5> vcf1Lvl {}, vcf2Lvl {};
    std::array<float, 4> vcf1Mod {}, vcf2Mod {};
    int vcf1Adsr = 0, vcf2Adsr = 0, vcf2Mode = 2, rmX = 0, rmY = 1;
    std::array<std::array<float, 3>, 2> vcaLvl {};
    std::array<std::array<float, 2>, 2> vcaMod {};
    std::array<float, 2> vcaGainV {};
    std::array<bool, 2> vcaExp {};
    std::array<int, 2> vcaAdsr {};
    struct AdsrC { int mode = 0; float a = 0.01f, d = 0.01f, s = 5.0f, r = 0.01f; };
    std::array<AdsrC, 2> adsrC {};
    float lfo1Hz = 1, lfo2Hz = 1, lfo2Pw = 0.5f, lfo1DelayC = 1.0f;
    int lfo1Wave = 2, lfo2Wave = 2;
    bool lfo1Key = true, lfo2Key = true;
    int shMode = 1; bool shInt = true; float shHz = 2.0f, shSlew = 0.1f;
    float ampGain = 1.0f, efAtk = 0.1f, efRel = 0.01f, intC = 0.01f;
    std::array<float, 3> mixLvl {}, revLvl {}, panLvl {}, panPosL {}, panPosR {};
    std::array<float, 2> psLvl {};
    float psRes = 0, psSpeed = 0.5f, psInt = 0.5f; bool psAuto = true;
    float stdLevel = 0, stdHz = 440; bool stdSine = false; int stdDest = 1;
    float vpLevel = 0, monGain = 1.0f;
    float pshThr = 5.0f, pshTime = 0.3f;

    inline float P (P700 id) const { return p[(size_t) id]; }
    inline int I (P700 id) const { return (int) std::lround (p[(size_t) id]); }

    static bool transparent (int j)
    {
        using namespace jk700;
        return j == amp_out || j == ef_out || j == int_out || j == mix_out || j == mix_inv || jacks[j].io == Mult;
    }
    static int multJack (int c, int r) { return (int) jk700::mult1_1 + c * 5 + r; }

    // Valor de una entrada para un módulo de voz / global.
    // Rieles de ±15 V y NaN → 0: un lazo de la patchera (OUT → PAN IN, fase en realimentación, múltiples en lazo)
    // satura como el hardware en vez de crecer hasta infinito.
    inline float srcVal (int s, int v) const { return railV (jv[(size_t) s][(size_t) (jk700::jacks[s].voice ? v : 0)]); }
    inline bool patched (int j) const { return patch.src[(size_t) j] >= 0; }
    inline float inV (int j, int v) const { const int s = patch.src[(size_t) j]; return s < 0 ? 0.0f : srcVal (s, v); }
    inline float inG (int j) const
    {
        const int s = patch.src[(size_t) j];
        if (s < 0) return 0.0f;
        if (! jk700::jacks[s].voice || ! polyJack[(size_t) s]) return railV (jv[(size_t) s][0]);
        float sum = 0.0f;
        for (int v = 0; v < nv; ++v) sum += jv[(size_t) s][(size_t) v];
        return railV (sum);
    }
    inline void setG (int j, float x) { jv[(size_t) j][0] = x; }
    inline void setV (int j, int v, float x) { jv[(size_t) j][(size_t) v] = x; }

    static float lagTime (float v)
    {
        // Escala del panel: 0, 0.1, 0.2, 0.5, 1, 2, 3 s en siete marcas iguales.
        static const float pts[7] = { 0.003f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, 3.0f };
        const float x = clampf (v, 0.0f, 1.0f) * 6.0f;
        const int i = std::min (5, (int) x);
        return pts[i] + (pts[i + 1] - pts[i]) * (x - (float) i);
    }

    void updateControls()
    {
        static const P700 RANGE[3] = { P700::vco1_range, P700::vco2_range, P700::vco3_range };
        static const P700 PITCH[3] = { P700::vco1_pitch, P700::vco2_pitch, P700::vco3_pitch };
        static const P700 FINE[3] = { P700::vco1_fine, P700::vco2_fine, P700::vco3_fine };
        static const P700 PW[3] = { P700::vco1_pw, P700::vco2_pw, P700::vco3_pw };
        static const P700 WAVE[3] = { P700::vco1_wave, P700::vco2_wave, P700::vco3_wave };
        static const P700 KEY[3] = { P700::vco1_keycv, P700::vco2_keycv, P700::vco3_keycv };
        static const float RANGE_OCT[6] = { -5.0f, -2.0f, -1.0f, 0.0f, 1.0f, 2.0f };
        for (int n = 0; n < 3; ++n)
        {
            const int rg = I (RANGE[n]);
            const float pitchSpan = rg == 0 ? 24.0f : 12.0f;            // en LO la perilla de PITCH abarca más
            vcoOct[(size_t) n] = RANGE_OCT[rg] + ((P (PITCH[n]) - 0.5f) * 2.0f * pitchSpan + (P (FINE[n]) - 0.5f) * 2.0f) / 12.0f;
            vcoPw[(size_t) n] = 0.1f + 0.8f * P (PW[n]);
            vcoWave[(size_t) n] = I (WAVE[n]);
            vcoKey[(size_t) n] = I (KEY[n]) == 0 ? 1.0f : 0.0f;
        }
        auto att = [&] (P700 a) { return I (a) == 0 ? 0.1f : 1.0f; };
        vcoMod[0] = { P (P700::vco1_mod1) * att (P700::vco1_att1), P (P700::vco1_mod2) * att (P700::vco1_att2), 0.0f };
        vcoMod[1] = { P (P700::vco2_mod1) * att (P700::vco2_att1), P (P700::vco2_mod2) * att (P700::vco2_att2), P (P700::vco2_mod3) * att (P700::vco2_att3) };
        vcoMod[2] = { P (P700::vco3_mod1) * att (P700::vco3_att1), P (P700::vco3_mod2) * att (P700::vco3_att2), P (P700::vco3_mod3) * att (P700::vco3_att3) };
        vcoStrong = { true, I (P700::vco2_syncmode) == 0, I (P700::vco3_syncmode) == 0 };
        pwmAmt = P (P700::vco1_pwm) * 0.1f;
        rmX = I (P700::rm_x); rmY = I (P700::rm_y);
        // Filtros: el fader de corte va de 20 Hz a 30 kHz (escala 100 / 1K / 10K del panel).
        vcf1Oct = std::log2 (20.0f) + P (P700::vcf1_cutoff) * std::log2 (1500.0f);
        vcf2Oct = std::log2 (20.0f) + P (P700::vcf2_cutoff) * std::log2 (1500.0f);
        vcf1Res = P (P700::vcf1_res); vcf2Res = P (P700::vcf2_res);
        vcf1K = vcf1Res * 5.3f;                                   // autooscila desde RES ≈ 7,5
        vcf1Lvl = { P (P700::vcf1_lvl1), P (P700::vcf1_lvl2), P (P700::vcf1_lvl3), P (P700::vcf1_lvl4), P (P700::vcf1_lvl5) };
        vcf2Lvl = { P (P700::vcf2_lvl1), P (P700::vcf2_lvl2), P (P700::vcf2_lvl3), P (P700::vcf2_lvl4), P (P700::vcf2_lvl5) };
        vcf1Mod = { P (P700::vcf1_mod1), P (P700::vcf1_mod2), P (P700::vcf1_mod3), P (P700::vcf1_mod4) };
        vcf2Mod = { P (P700::vcf2_mod1), P (P700::vcf2_mod2), P (P700::vcf2_mod3), P (P700::vcf2_mod4) };
        vcf1Adsr = I (P700::vcf1_adsr); vcf2Adsr = I (P700::vcf2_adsr); vcf2Mode = I (P700::vcf2_mode);
        vcaLvl[0] = { P (P700::vca1_lvl1), P (P700::vca1_lvl2), P (P700::vca1_lvl3) };
        vcaLvl[1] = { P (P700::vca2_lvl1), P (P700::vca2_lvl2), P (P700::vca2_lvl3) };
        vcaMod[0] = { P (P700::vca1_mod1), P (P700::vca1_mod2) };
        vcaMod[1] = { P (P700::vca2_mod1), P (P700::vca2_mod2) };
        vcaGainV = { P (P700::vca1_gain) * 10.0f, P (P700::vca2_gain) * 10.0f };
        vcaExp = { I (P700::vca1_exp) == 1, I (P700::vca2_exp) == 1 };
        vcaAdsr = { I (P700::vca1_adsr), I (P700::vca2_adsr) };
        static const P700 AM[2] = { P700::adsr1_mode, P700::adsr2_mode }, AT[2] = { P700::adsr1_time, P700::adsr2_time };
        static const P700 AA[2] = { P700::adsr1_a, P700::adsr2_a }, AD[2] = { P700::adsr1_d, P700::adsr2_d };
        static const P700 AS[2] = { P700::adsr1_s, P700::adsr2_s }, AR[2] = { P700::adsr1_r, P700::adsr2_r };
        for (int a = 0; a < 2; ++a)
        {
            const float sc = I (AT[a]) == 1 ? 0.1f : 1.0f;
            auto t = [&] (float v) { return expMap700 (v, 0.002f, 10.0f) * sc; };
            adsrC[(size_t) a].mode = I (AM[a]);
            adsrC[(size_t) a].a = coef700 (t (P (AA[a])) / 1.466f, fs);
            adsrC[(size_t) a].d = coef700 (t (P (AD[a])) / 2.3f, fs);
            adsrC[(size_t) a].s = P (AS[a]) * 10.0f;
            adsrC[(size_t) a].r = coef700 (t (P (AR[a])) / 2.3f, fs);
        }
        lfo1Hz = expMap700 (P (P700::lfo1_freq), 0.03f, 30.0f);
        lfo2Hz = expMap700 (P (P700::lfo2_freq), 0.03f, 30.0f);
        lfo1Wave = I (P700::lfo1_wave); lfo2Wave = I (P700::lfo2_wave);
        lfo2Pw = 0.1f + 0.8f * P (P700::lfo2_pw);
        lfo1Key = I (P700::lfo1_keytrig) == 0; lfo2Key = I (P700::lfo2_keytrig) == 0;
        lfo1DelayC = coef700 (expMap700 (P (P700::lfo1_delay), 0.002f, 10.0f) / 2.3f, fs);
        shMode = I (P700::sh_mode); shInt = I (P700::sh_clock) == 1;
        shHz = expMap700 (P (P700::sh_rate), 0.3f, 30.0f);
        shSlew = coef700 (0.0003f, fs);
        static const float AMP_R[3] = { 1000.0f, 100.0f, 10.0f };
        ampGain = AMP_R[juce_clamp (I (P700::amp_range), 0, 2)] * P (P700::amp_gain);
        efAtk = coef700 (0.004f, fs); efRel = coef700 (0.06f, fs);
        intC = coef700 (lagTime (P (P700::int_lag)) / 2.3f, fs);
        mixLvl = { P (P700::mix_lvl1), P (P700::mix_lvl2), P (P700::mix_lvl3) };
        revLvl = { P (P700::rev_lvl1), P (P700::rev_lvl2), P (P700::rev_lvl3) };
        panLvl = { P (P700::pan_lvl1), P (P700::pan_lvl2), P (P700::pan_lvl3) };
        static const P700 POS[3] = { P700::pan_pos1, P700::pan_pos2, P700::pan_pos3 };
        for (int c = 0; c < 3; ++c)
        {
            const float pp = P (POS[c]);
            panPosL[(size_t) c] = std::cos (pp * 1.5707963f) * 1.4142f;
            panPosR[(size_t) c] = std::sin (pp * 1.5707963f) * 1.4142f;
        }
        psLvl = { P (P700::ps_lvl1), P (P700::ps_lvl2) };
        psRes = P (P700::ps_res); psInt = P (P700::ps_int); psAuto = I (P700::ps_auto) == 0;
        psSpeed = expMap700 (P (P700::ps_speed), 0.03f, 10.0f);
        stdLevel = P (P700::std_level) < 0.02f ? 0.0f : P (P700::std_level) * 5.0f;
        { const int f = I (P700::std_freq); stdSine = f == 5; stdHz = f == 5 ? 1000.0f : 110.0f * (float) (1 << juce_clamp (f, 0, 4)); }
        stdDest = I (P700::std_dest);
        vpLevel = P (P700::vp_level) * 10.0f;
        { const float m = P (P700::monitor) / 0.7f; monGain = m * m * m; }          // +9 dB al máximo, −∞ en 0
        powered = I (P700::power) == 0;
        pshThr = P (P700::pshaper_thr) * 10.0f;
        { static const float pts[6] = { 0.0f, 0.1f, 0.2f, 1.0f, 2.0f, 3.0f }; static const float at_[6] = { 0.0f, 0.25f, 0.4f, 0.75f, 0.9f, 1.0f };
          const float v = P (P700::pshaper_time); int i = 0; while (i < 4 && v > at_[i + 1]) ++i;
          pshTime = pts[i] + (pts[i + 1] - pts[i]) * (v - at_[i]) / (at_[i + 1] - at_[i]); }
        // Teclado.
        kb.pitchSemis = (P (P700::kb_pitch) - 0.5f) * 12.0f;
        kb.porta = I (P700::kb_portaon) == 1;
        kb.portaTime = expMap700 (P (P700::kb_porta), 0.1f, 10.0f) * 0.5f;
        static const float BR[4] = { 5.0f, 1.0f, 0.1f, 0.0f };
        const float lever = clampf ((P (P700::kb_bender) - 0.5f) * 2.0f + bender, -1.0f, 1.0f);
        kb.bendV = lever * BR[juce_clamp (I (P700::kb_bendrange), 0, 3)];
        leverNow = lever;
        nv = kb.voices();
        // Secuenciador.
        static const float SRANGE[3] = { 10.0f, 4.0f, 2.0f };
        static const float DIV[6] = { 1.0f / 32.0f, 1.0f / 16.0f, 1.0f / 8.0f, 1.0f / 4.0f, 1.0f / 2.0f, 1.0f };
        for (int s = 0; s < 12; ++s)
        {
            const int b = (int) P700::step1_a + s * 5;                  // a, b, c, div, fine por paso (orden de panel700.py)
            seq.reg[0][(size_t) s] = p[(size_t) b]; seq.reg[1][(size_t) s] = p[(size_t) b + 1]; seq.reg[2][(size_t) s] = p[(size_t) b + 2];
            seq.divide[(size_t) s] = DIV[juce_clamp ((int) std::lround (p[(size_t) b + 3]), 0, 5)];
            seq.fine[(size_t) s] = 1.0f + p[(size_t) b + 4];
            seq.hold[(size_t) s] = (int) std::lround (p[(size_t) ((int) P700::step1_hold + s)]) == 1;
        }
        seq.range = { SRANGE[juce_clamp (I (P700::par_range_a), 0, 2)], SRANGE[juce_clamp (I (P700::par_range_b), 0, 2)], SRANGE[juce_clamp (I (P700::par_range_c), 0, 2)] };
        seq.rate = expMap700 (P (P700::clk_rate), 2.0f, 20.0f) * (I (P700::clk_range) == 0 ? 10.0f : 1.0f);
        seq.pw = 0.1f + 0.8f * P (P700::clk_pw);
        seq.serMode = I (P700::ser_mode); seq.repeat = I (P700::ser_repeat) == 1; seq.steps = 1 + I (P700::ser_steps);
        if (seq.step >= seq.steps) seq.step = 0;
    }


    // Una vez por muestra base: teclado, secuenciador, pulsos de compuerta.
    void baseTick (float dt, double ppqPerSample)
    {
        using namespace jk700;
        kb.process();
        for (int v = 0; v < nv; ++v)
        {
            auto& V = vs[(size_t) v];
            const bool tr = kb.v[(size_t) v].trig > 0;
            V.trigEdge = tr && ! V.lastTrig;
            V.lastTrig = tr;
            const float c1 = kb.cv1 (v), c2 = kb.cv2 (v);
            const bool g = kb.v[(size_t) v].gate;
            setV (kb_cv1, v, c1); setV (kb_cv2, v, c2); setV (kb_cvout, v, c1);
            setV (kb_gate, v, g ? 15.0f : 0.0f); setV (kb_gateout, v, g ? 15.0f : 0.0f);
            setV (kb_trig, v, tr ? 15.0f : 0.0f); setV (kb_vel, v, kb.v[(size_t) v].vel * 10.0f);
        }
        setG (kb_bend, leverNow * 5.0f); setG (kb_mod, kb.modWheel * 10.0f); setG (kb_at, kb.aftertouch * 10.0f);
        // Compuerta global del teclado (primera nota de una frase): KEY TRIG de los LFO y retardo del LFO-1.
        const bool anyGate = kb.anyGate();
        if (anyGate && ! lastAnyGate)
        {
            if (lfo1Key) lfo1.keyTrig (lfo1Wave);
            if (lfo2Key) lfo2.keyTrig (lfo2Wave);
            lfo1Delay = 0.0f;
        }
        lastAnyGate = anyGate;
        // Secuenciador.
        const std::array<float, 4> cps { inG (seq_cp_start), inG (seq_cp_continue), inG (seq_cp_stop), inG (seq_cp_end) };
        // Follow Transport: el Play/Stop del DAW hace START/STOP con los dos relojes (con el interno, el tempo
        // sigue siendo el del panel). Antes sólo actuaba con el reloj del host y los presets de secuencia no
        // arrancaban con el Play del DAW.
        if (st.seqFollowTransport)
        {
            if (st.hostPlaying && ! hostWasPlaying) seq.start();
            if (! st.hostPlaying && hostWasPlaying) seq.stop();
        }
        hostWasPlaying = st.hostPlaying;
        seq.process (inG (clk_ext), patched (clk_ext), inG (clk_cv), cps, ppqPerSample, st.hostPlaying, dt);
        setG (clk_out1, seq.clockOut); setG (clk_out2, seq.clockOut);
        setG (seq_startout, seq.startOut); setG (seq_endout, seq.endOut);
        for (int s = 0; s < 12; ++s) setG ((int) step1_gate + 2 * s, seq.gateOut[(size_t) s]);
        view.clock = seq.clockOut > 1.0f ? 1.0f : 0.0f;
        view.master = { seq.running, false, ! seq.running, seq.endOut > 1.0f && seq.step == 0 };
    }
    bool hostWasPlaying = false;

    // Una muestra sobremuestreada.
    inline void tick (bool firstOfBase, float& outL, float& outR)
    {
        using namespace jk700;
        // ── fuentes globales ──
        noise.process();
        setG (noise_white, noise.white); setG (noise_pink, noise.pink);
        const float l1 = lfo1.process (lfo1Hz * fastExp2 (clampf (inG (lfo1_cv), -10.0f, 10.0f)), lfo1Wave, 0.5f, fs);
        lfo1Delay += (1.0f - lfo1Delay) * lfo1DelayC;
        const float lfo1v = l1 * lfo1Delay;
        setG (lfo1_out, lfo1v);
        const float l2 = lfo2.process (lfo2Hz * fastExp2 (clampf (inG (lfo2_cv), -10.0f, 10.0f)), lfo2Wave, lfo2Pw, fs);
        setG (lfo2_out, l2);
        {
            const bool edge = sh.clock (! shInt, inG (sh_clkin), shHz, fs);
            const float src = shMode == 0 ? lfo1v : (shMode == 1 ? noise.white : inG (sh_in));
            setG (sh_out, sh.process (edge, src, shSlew));
            setG (sh_clkout, sh.clockHigh ? 15.0f : 0.0f);
        }
        setG (vp_pos, vpLevel); setG (vp_neg, -vpLevel);
        // Salidas del secuenciador con ANALOG IN (puede ser audio: va a la frecuencia alta).
        {
            const int s = seq.step;
            float par[3] = { seq.par[0], seq.par[1], seq.par[2] };
            const int aj = (int) step1_in + 2 * s;
            if (patched (aj) && ! seq.hold[(size_t) s])
                for (int c = 0; c < 3; ++c) par[c] = inG (aj) * seq.reg[(size_t) c][(size_t) s];
            setG (par_a1, par[0]); setG (par_a2, par[0]); setG (par_b1, par[1]); setG (par_b2, par[1]); setG (par_c1, par[2]); setG (par_c2, par[2]);
            const float so = par[seq.serMode == 3 ? seq.manualChan : seq.chan];
            setG (ser_out1, so); setG (ser_out2, so);
            const float ps = pshaper.process (inG (ps1_in), pshThr, pshTime, 1.0f / fs);
            setG (ps1_out1, ps); setG (ps1_out2, ps);
            view.pshaper = ps > 1.0f ? 1.0f : 0.0f;
        }

        // ── voces ──
        float bus1 = 0.0f, bus2 = 0.0f;
        for (int v = 0; v < nv; ++v)
        {
            voiceTick (v, firstOfBase, lfo1v);
            bus1 += vs[(size_t) v].busVca[0]; bus2 += vs[(size_t) v].busVca[1];
        }
        // Múltiples: cada columna suma lo que entra por cualquiera de sus jacks.
        for (int c = 0; c < 2; ++c)
            for (int v = 0; v < nv; ++v)
            {
                float sum = 0.0f;
                for (int r = 0; r < 5; ++r) { const int j = multJack (c, r); const int s = patch.src[(size_t) j]; if (s >= 0 && s != j) sum += srcVal (s, v); }
                for (int r = 0; r < 5; ++r) setV (multJack (c, r), v, sum);
            }

        // ── 711A: reverb, paneo, phaser, oscilador patrón ──
        const float revIn = bus1 * revLvl[0] + bus2 * revLvl[1] + inG (rev_in) * revLvl[2];
        const float rev = spring.process (revIn * 0.6f);
        setG (rev_out, rev);
        const float p1 = patched (pan_in1) ? inG (pan_in1) : bus1;
        const float p2 = patched (pan_in2) ? inG (pan_in2) : bus2;
        const float p3 = patched (pan_in3) ? inG (pan_in3) : rev;
        float L = p1 * panLvl[0] * panPosL[0] + p2 * panLvl[1] * panPosL[1] + p3 * panLvl[2] * panPosL[2];
        float R = p1 * panLvl[0] * panPosR[0] + p2 * panLvl[1] * panPosR[1] + p3 * panLvl[2] * panPosR[2];
        if (stdLevel > 0.0f)
        {
            stdPhase += stdHz / fs; if (stdPhase >= 1.0f) stdPhase -= 1.0f;
            const float so = stdLevel * (stdSine ? sinHalfPi (stdPhase < 0.5f ? 4.0f * stdPhase - 1.0f : 3.0f - 4.0f * stdPhase) : (stdPhase < 0.5f ? 1.0f : -1.0f));
            if (stdDest != 2) L += so;
            if (stdDest != 0) R += so;
        }
        {
            const float psIn = (patched (ps_in1) ? inG (ps_in1) : bus1) * psLvl[0] + (patched (ps_in2) ? inG (ps_in2) : bus2) * psLvl[1];
            float sweep;
            if (psAuto)
            {
                psLfoPhase += psSpeed / fs; if (psLfoPhase >= 1.0f) psLfoPhase -= 1.0f;
                sweep = 0.5f + 0.5f * psInt * sinHalfPi (psLfoPhase < 0.5f ? 4.0f * psLfoPhase - 1.0f : 3.0f - 4.0f * psLfoPhase);
            }
            else sweep = clampf (0.5f + psInt * inG (ps_ext) * 0.1f, 0.0f, 1.0f);
            setG (ps_out, phaser.process (psIn, sweep, psRes, fs));
        }
        setG (out_l, L); setG (out_r, R);
        outL = L; outR = R;
    }

    inline void voiceTick (int v, bool firstOfBase, float lfo1v)
    {
        using namespace jk700;
        auto& V = vs[(size_t) v];
        const float kcv = jv[kb_cv1][(size_t) v];
        const bool kgate = kb.v[(size_t) v].gate || manualGate;
        const bool ktrig = firstOfBase && V.trigEdge;
        // ADSR 1 y 2.
        static const int AG[2] = { adsr1_gate, adsr2_gate }, AO[2] = { adsr1_out, adsr2_out };
        for (int a = 0; a < 2; ++a)
        {
            const auto& c = adsrC[(size_t) a];
            bool g, t = false;
            if (c.mode == 2) g = inV (AG[a], v) >= 5.0f || manualGate;
            else { g = kgate; t = c.mode == 0 && ktrig; }
            const float y = V.adsr[(size_t) a].process (g, t, c.a, c.d, c.s, c.r);
            V.busAdsr[(size_t) a] = y;
            setV (AO[a], v, y);
        }
        // Deriva lenta (el µA726 es estable: unos pocos cents).
        if (++V.driftCount >= 4096)
        {
            V.driftCount = 0;
            for (auto& d : V.driftT) d = V.driftRng.bipolar() * st.drift * 0.006f;
        }
        // VCO 1–3.
        const float shv = jv[sh_out][0];
        static const int OUT0[3] = { vco1_sine, vco2_sine, vco3_sine };
        for (int n = 0; n < 3; ++n)
        {
            V.drift[(size_t) n] += (V.driftT[(size_t) n] - V.drift[(size_t) n]) * 0.0005f;
            const auto& m = vcoMod[(size_t) n];
            float oct = vcoOct[(size_t) n] + V.drift[(size_t) n] + vcoKey[(size_t) n] * kcv + m[0] * lfo1v;
            float pw = vcoPw[(size_t) n];
            bool syncP = false; float syncIn = 0.0f;
            if (n == 0) { oct += m[1] * inV (vco1_ext, v); pw += pwmAmt * inV (vco1_pwm, v); }
            else
            {
                oct += m[1] * (n == 1 ? V.busAdsr[0] : shv) + m[2] * inV (n == 1 ? vco2_ext : vco3_ext, v);
                const int sj = n == 1 ? vco2_sync : vco3_sync;
                syncP = patched (sj); syncIn = syncP ? inV (sj, v) : 0.0f;
            }
            const float hz = clampf (65.406f * fastExp2 (clampf (oct, -14.0f, 11.0f)), 0.02f, fs * 0.45f);
            auto& o = V.vco[(size_t) n];
            o.process (hz, pw, syncIn, syncP, vcoStrong[(size_t) n], fs);
            const float w[4] = { o.sine, o.tri, o.saw, o.pulse };
            for (int k = 0; k < 4; ++k) setV (OUT0[n] + k, v, w[k]);
            // Bus del selector de onda: sierra y pulso unipolares van centrados (acople de los VCF).
            const int wi = vcoWave[(size_t) n];
            V.busVco[(size_t) n] = wi >= 2 ? w[wi] - 5.0f : w[wi];
        }
        // Ring mod (X·Y, sólo suma y diferencia; algo de portadora por la calibración).
        {
            const float x = rmX < 3 ? V.busVco[(size_t) rmX] : inV (rm_x, v);
            const float y = rmY == 0 ? V.busVco[0] : rmY == 1 ? V.busVco[1] : rmY == 2 ? noise.white : inV (rm_y, v);
            V.busRm = softClip700 (x * y * 0.2f + 0.008f * x, 10.0f);
            setV (rm_out, v, V.busRm);
            if (v == 0) view.rmSig = std::max (view.rmSig * 0.9995f, std::fabs (V.busRm) * 0.2f);
        }
        // VCF-1 (703E, o 703A según el ajuste) y VCF-2 (703B).
        {
            const float in = V.busVco[0] * vcf1Lvl[0] + V.busVco[1] * vcf1Lvl[1] + V.busVco[2] * vcf1Lvl[2]
                           + noise.white * vcf1Lvl[3] + inV (vcf1_in, v) * vcf1Lvl[4] + noise.white * 2.0e-4f;   // piso de ruido: arranca la autooscilación
            const float oct = vcf1Oct + kcv * vcf1Mod[0] + V.busAdsr[(size_t) vcf1Adsr] * vcf1Mod[1] + lfo1v * vcf1Mod[2] + inV (vcf1_cv, v) * vcf1Mod[3];
            const float fc = fastExp2 (clampf (oct, 2.0f, 15.2f));
            float y;
            if (st.vcf1Model == 0) y = V.vcf1o.process (in, fc, clampf (vcf1K + inV (vcf1_rescv, v) * 0.47f, 0.0f, 5.5f), fs);
            else y = V.vcf1s.process (in, fc, vcf1Res, st.vcf1Mode, fs);
            V.busVcf[0] = y; setV (vcf1_out, v, y);
            if (v == 0) { view.vcfOver[0] = std::max (view.vcfOver[0] * 0.9995f, std::fabs (in) > 6.5f ? 1.0f : 0.0f); view.vcfSig[0] = std::max (view.vcfSig[0] * 0.9995f, std::fabs (y) * 0.25f); }
        }
        {
            const float in = V.busVco[0] * vcf2Lvl[0] + V.busVco[1] * vcf2Lvl[1] + V.busVco[2] * vcf2Lvl[2]
                           + V.busRm * vcf2Lvl[3] + inV (vcf2_in, v) * vcf2Lvl[4] + noise.white * 2.0e-4f;
            const float oct = vcf2Oct + kcv * vcf2Mod[0] + V.busAdsr[(size_t) vcf2Adsr] * vcf2Mod[1] + lfo1v * vcf2Mod[2] + inV (vcf2_cv, v) * vcf2Mod[3];
            const float fc = fastExp2 (clampf (oct, 2.0f, 15.2f));
            const float y = V.vcf2.process (in, fc, vcf2Res, vcf2Mode, fs);
            V.busVcf[1] = y; setV (vcf2_out, v, y);
            if (v == 0) { view.vcfOver[1] = std::max (view.vcfOver[1] * 0.9995f, std::fabs (in) > 6.5f ? 1.0f : 0.0f); view.vcfSig[1] = std::max (view.vcfSig[1] * 0.9995f, std::fabs (y) * 0.25f); }
        }
        // VCA-1 y VCA-2.
        static const int VIN[2] = { vca1_in, vca2_in }, VCV[2] = { vca1_cv, vca2_cv }, VOUT[2] = { vca1_out, vca2_out };
        for (int a = 0; a < 2; ++a)
        {
            const auto& lv = vcaLvl[(size_t) a];
            const float in = V.busVcf[0] * lv[0] + V.busVcf[1] * lv[1] + inV (VIN[a], v) * lv[2];
            const float vctl = vcaGainV[(size_t) a] + V.busAdsr[(size_t) vcaAdsr[(size_t) a]] * vcaMod[(size_t) a][0] + inV (VCV[a], v) * vcaMod[(size_t) a][1];
            const float y = softClip700 (in * vcaGain704 (vctl, vcaExp[(size_t) a]), 11.0f);
            V.busVca[(size_t) a] = y; setV (VOUT[a], v, y);
            if (v == 0) { view.vcaOver[(size_t) a] = std::max (view.vcaOver[(size_t) a] * 0.9995f, std::fabs (in) > 6.5f ? 1.0f : 0.0f); view.vcaSig[(size_t) a] = std::max (view.vcaSig[(size_t) a] * 0.9995f, std::fabs (y) * 0.25f); }
        }
        // 707A: amplificador → seguidor de envolvente → integrador (jacks de corte).
        {
            const float amp = softClip700 (inV (amp_in, v) * ampGain, 10.0f);
            setV (amp_out, v, amp);
            const float efIn = patched (ef_in) ? inV (ef_in, v) : amp;
            const float e = V.ef.process (efIn, efAtk, efRel);
            setV (ef_out, v, e);
            if (v == 0) view.ef = clampf (e * 0.2f, 0.0f, 1.0f);
            const float iIn = patched (int_in) ? inV (int_in, v) : e;
            // El integrador del original no es lineal: baja un poco más lento de lo que sube.
            V.intY += (iIn - V.intY) * (iIn > V.intY ? intC : intC * 0.87f);
            setV (int_out, v, V.intY);
        }
        // 716A mezclador.
        {
            const float s = inV (mix_in1, v) * mixLvl[0] + inV (mix_in2, v) * mixLvl[1] + inV (mix_in3, v) * mixLvl[2];
            setV (mix_out, v, softClip700 (s, 13.0f)); setV (mix_inv, v, softClip700 (-s, 13.0f));
        }
        if (v == 0) { view.lfo1 = clampf (std::fabs (jv[lfo1_out][0]) * 0.15f, 0.0f, 1.0f); view.lfo2 = clampf (std::fabs (jv[lfo2_out][0]) * 0.15f, 0.0f, 1.0f); }
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
