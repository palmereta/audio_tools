#pragma once
// Motor de Tarnish (docs/research.md §3.4). Por canal:
//
//   cinta (estudio, casete, video):
//     NR (codifica) → ⟦×4: énfasis de grabación → histéresis⟧ → autoborrado → transporte (wow, flutter, stop, eco de
//     copia) → golpe de cabeza → pérdidas (separación + caídas, azimut) → hiss y ruido de modulación → énfasis de
//     reproducción → NR (decodifica)
//   vinilo:
//     corte (RIAA inversa) → distorsión de lectura → transporte (excentricidad, alabeo) → crujido, rayas, superficie
//     y rumble → RIAA → pérdida de agudos del surco interior
//   radio / teléfono: su cadena mono (Lofi.h) → transporte (sin wow; mantiene la latencia)
//   → corte de graves y agudos → varispeed → crush → ganancia (con compensación automática) → mezcla con la señal
//     seca atrasada la misma latencia
//
// La latencia es constante: 54 muestras del sobremuestreo (Lacquer, Saturation.h) + 20 ms del transporte + 1 del
// crusher. Todo avanza por muestra y lo aleatorio depende de la posición: la salida no depende del bloque.

#include "Hysteresis.h"
#include "Transport.h"
#include "Events.h"
#include "Lofi.h"
#include "Saturation.h"          // dh::lq::Os4 (plugins/lacquer)

namespace dh::tn {

struct EngineParams
{
    int machine = kStudio, variant = 1;
    bool nab = false;                         // estudio: ecualización NAB (graves) en vez de IEC
    double driveDb = 0, saturation = 0.5, bias = 0;
    double wowPct = 0.03, flutterPct = 0.02; bool stereoWobble = false;
    double scrape = 0.2;                      // ruido de modulación, 0-1
    double wear = 0, dropouts = 0;            // 0-1
    double noise = 0.5;                       // 0 apagado, 0,5 el ruido propio de la máquina, 1 = +20 dB
    double crackle = 0; int scratches = 0;    // vinilo: crujido y rayas; radio: estática
    double hum = 0;                           // 0-1 (−80 a −30 dB)
    double lowCut = 10, highCut = 22000;
    double azimuth = 0, printDb = -200;
    int nr = 0; double mistrackDb = 0;        // 0 apagado, 1 suave (10 dB), 2 fuerte (20 dB)
    double bits = 24, rateHz = 192000; bool crushRaw = false; double jitter = 0; bool dither = false;
    double varispeed = 0;                     // semitonos
    bool stop = false; double stopS = 0.6, startS = 0.35, stopCurve = 0.5;
    double groove = 0; bool grooveFollow = true;
    double compress = 0.6, fading = 0.2, tuning = 0, whistle = 0, detector = 0.3;   // radio
    double frameLoss = 0, codec = 0;          // teléfono
    bool autoGain = true; double outDb = 0, mix = 1;
    bool eco = false;                         // histéresis con RK2 en vez de RK4 (la mitad de CPU)
    uint64_t seed = 0x7A2;
};

class Engine
{
public:
    void prepare (double sampleRate, int /*maxBlock*/)
    {
        fs = sampleRate;
        tr.prepare (fs);
        eraseAtt = 1.0 - std::exp (-1.0 / (0.001 * fs)); eraseRel = 1.0 - std::exp (-1.0 / (0.010 * fs));
        for (int c = 0; c < 2; ++c) { drop.prepare (fs); imp[c].prepare (fs); crush[c].prepare (fs); vari[c].prepare (fs); nrE[c].prepare (fs); nrD[c].prepare (fs); azi[c].prepare (64); }
        radio.prepare (fs); phone.prepare (fs);
        lat = dh::lq::Os4::latency() + tr.latency() + 1;          // + la muestra que atrasa el crusher
        dry[0].prepare (lat + 8); dry[1].prepare (lat + 8);
        machDelay[0].prepare (dh::lq::Os4::latency() + 8); machDelay[1].prepare (dh::lq::Os4::latency() + 8);
        set (P, true);
        reset();
    }
    int latency() const { return lat; }
    // Tasa de caídas por segundo, probabilidad de tramo dañado, escala de profundidad y semilla (las usa también la
    // interfaz para dibujar las caídas que vienen).
    static void dropoutSettings (const EngineParams& p, const Profile& prof, double& rate, double& damage, double& depth, uint64_t& seed)
    {
        rate = prof.magnetic ? 4.0 * p.dropouts * p.dropouts + 0.3 * p.dropouts : 0.0;
        damage = 0.05 + 0.35 * p.wear; depth = 1.0 + p.wear; seed = p.seed ^ 0xD20;
    }
    // Tape stop sin rehacer el resto de los ajustes (lo dispara el MIDI o el transporte del host).
    void setStop (bool on) { P.stop = on; tr.setStopped (on); }
    bool stopped() const { return tr.stopping(); }

    void reset()
    {
        for (int c = 0; c < 2; ++c)
        {
            os[c].reset(); ja[c].reset(); recHF[c].reset(); recLF[c].reset(); repHF[c].reset(); repLF[c].reset();
            bump[c].reset(); bumpHp[c].reset(); loss[c].reset(); azLp[c].reset(); eraseLp[c].reset(); eraseEnv[c] = 0;
            modBp[c].reset(); cutA[c].reset(); cutB[c].reset(); playA[c].reset(); playB[c].reset(); traceHp[c].reset(); traceDc[c].reset();
            inner[c].reset(); lc[c].reset(); hc1[c].reset(); hc2[c].reset(); rumble[c].reset(); dry[c].clear(); machDelay[c].clear();
            crush[c].reset(); vari[c].reset(); nrE[c].reset(); nrD[c].reset(); azi[c].clear(); imp[c].reset(); mHp[c].reset(); mLp[c].reset();
        }
        tr.reset(); drop.reset(); radio.reset(); phone.reset();
        counter = 0; innerG = -1; vu = 0; wfAcc = 0; wfN = 0; wfRead = 0; wfBp.reset(); wfDec = 0; lastRot = -1;
    }

    void set (const EngineParams& p, bool force = false)
    {
        const bool machineChanged = force || p.machine != P.machine || p.variant != P.variant || p.nab != P.nab;
        P = p;
        prof = profile (p.machine, p.variant);
                JaParams jp; jp.driveDb = p.driveDb; jp.saturation = clampd (p.saturation + prof.satOffset, 0.0, 1.0);
        jp.bias = p.bias; jp.formulation = prof.formulation; jp.rk4 = ! p.eco;
        for (auto& j : ja) j.set (jp);
        if (machineChanged)
        {
            // Énfasis (docs/research.md §2.4): grabación (1+sτ)/(1+sτp), reproducción la inversa; graves NAB/casete
            // con el refuerzo de reproducción limitado a +20 dB.
            const double tp = 1.0 / (kTwoPi * 22000.0), fT = 1.0 / (kTwoPi * prof.tauHF);
            for (int c = 0; c < 2; ++c)
            {
                recHF[c].set (prof.tauHF, tp, fs, fT);
                repHF[c].set (tp, prof.tauHF, fs, fT);
            }
            tauLF = (p.machine == kStudio && p.nab) ? 3180e-6 : prof.tauLF;
            for (int c = 0; c < 2; ++c)
            {
                if (tauLF > 0) { recLF[c].set (10.0 * tauLF, tauLF, fs, 50.0); repLF[c].set (tauLF, 10.0 * tauLF, fs, 50.0); }
                bump[c].identity(); bumpHp[c].identity();
                if (prof.bumpHz > 0) bump[c].peak (prof.bumpHz, 2.0, prof.bumpDb, fs);
                if (prof.magnetic) bumpHp[c].highpass (prof.lfHz, 0.6, fs);
                // RIAA (vinilo): reproducción = (1+s·318µ)/(1+s·3180µ) · (1+s·τl)/(1+s·75µ), corte = la inversa.
                cutA[c].set (3180e-6, 318e-6, fs, 1000.0); cutB[c].set (75e-6, 3.18e-6, fs, 1000.0);
                playA[c].set (318e-6, 3180e-6, fs, 1000.0); playB[c].set (3.18e-6, 75e-6, fs, 1000.0);
                traceHp[c].highpass (2000.0, 0.707, fs); traceDc[c].setHz (10.0, fs);
                eraseLp[c].setHz (6000.0, fs);
                modBp[c].bandpass (2500.0, 0.7, fs);
                rumble[c].setHz (25.0, fs);
                mHp[c].identity(); mLp[c].identity();
                if (p.machine == kVinyl) { mHp[c].highpass (prof.lfHz, 0.6, fs); if (prof.hfHz < 15000) mLp[c].lowpass (prof.hfHz, 0.707, fs); }
            }
            riaaNorm = 1.0 / (playA[0].mag (1000.0, fs) * playB[0].mag (1000.0, fs));
            radio.reset(); phone.reset();
        }
        for (int c = 0; c < 2; ++c)
        {
            lc[c].highpass (clampd (p.lowCut, 10.0, 2000.0), 0.707, fs);
            hc1[c].lowpass (clampd (p.highCut, 500.0, 0.49 * fs), 0.54, fs); hc2[c].lowpass (clampd (p.highCut, 500.0, 0.49 * fs), 1.31, fs);
            nrE[c].set (p.nr == 0 ? 0.0 : (p.nr == 1 ? 10.0 : 20.0), true, 0.0);
            nrD[c].set (p.nr == 0 ? 0.0 : (p.nr == 1 ? 10.0 : 20.0), false, p.mistrackDb);
            crush[c].set (p.bits, p.rateHz, p.crushRaw, p.jitter, p.dither);
            vari[c].setSemis (p.varispeed);
        }
        lcOn = p.lowCut > 10.5; hcOn = p.highCut < std::min (21900.0, 0.49 * fs);

        TransportParams tp;
        tp.wowPct = p.wowPct; tp.flutterPct = p.flutterPct; tp.stereoWobble = p.stereoWobble; tp.printDb = prof.magnetic ? p.printDb : -200;
        tp.stopSeconds = p.stopS; tp.startSeconds = p.startS; tp.stopCurve = p.stopCurve; tp.seed = p.seed;
        tp.driftShare = p.machine == kVinyl ? 0.15 : 0.35;
        tr.set (prof, p.machine, tp);
        tr.setStopped (p.stop);

        // Desgaste → separación fija (m); caídas por segundo; tramos dañados.
        spacing0 = p.wear * (p.machine == kStudio ? 1.5e-6 : 1.2e-6);
        { double r, d, k; uint64_t sd; dropoutSettings (p, prof, r, d, k, sd); drop.set (r, d, k, sd); }
        // Azimut (§2.7): ancho de pista, separación entre pistas y ángulo máximo por máquina.
        const double w = p.machine == kStudio ? 2.0e-3 : (p.machine == kCassette ? 0.6e-3 : 1.0e-3);
        const double sep = p.machine == kStudio ? 2.5e-3 : 0.9e-3;
        const double th = (p.machine == kStudio ? 0.12 : 0.4) * kPi / 180.0 * p.azimuth;
        fAz = th > 1e-9 ? 0.443 * prof.speed / (w * std::tan (th)) : 1e9;
        azDelay = prof.magnetic ? sep * std::tan (th) / prof.speed * fs : 0.0;
        for (int c = 0; c < 2; ++c) azLp[c].lowpass (std::min (fAz, 0.45 * fs), 0.707, fs);

        Radio::Params rp; rp.variant = p.variant; rp.compress = p.compress; rp.fading = p.fading; rp.staticAmt = p.crackle;
        rp.whistle = p.whistle; rp.tuning = p.tuning; rp.detector = p.detector; rp.seed = p.seed;
        radio.set (rp);
        Phone::Params pp; pp.variant = p.variant; pp.drive = p.driveDb; pp.frameLoss = p.frameLoss; pp.codec = p.codec; pp.seed = p.seed;
        phone.set (pp);

        // Ruido: el nivel pedido después de toda la cadena de reproducción (se calibra con las magnitudes de los
        // filtros, sin simular), respecto del nivel de referencia (un seno de −18 dBFS de pico).
        const double ref = 0.125 / std::sqrt (2.0);
        noiseDb = p.noise <= 0.0005 ? -300.0 : prof.hissDb + (p.noise - 0.5) * 40.0;
        noiseGain = noiseDb < -250 ? 0.0 : ref * dbToGain (noiseDb) / std::max (1e-9, chainNoiseRms());
        humGain = p.hum > 0 ? dbToGain (-80.0 + 50.0 * p.hum) : 0.0;
        humHz = p.machine == kVhs ? 59.94 : (p.machine == kPhone || p.machine == kRadio ? 50.0 : 60.0);
        // Compensación automática: el drive sube el nivel como en la cinta; acá se baja lo mismo, más el ajuste
        // propio de cada máquina (medido con ruido rosa a −18 dBFS, tools/tarnishtest.cpp).
        const double trim = machineTrimDb (p.machine, p.variant);
        outGain = dbToGain (p.outDb + (p.autoGain ? -(prof.magnetic || p.machine == kPhone ? p.driveDb : 0.0) + trim : 0.0));
        mix = clampd (p.mix, 0.0, 1.0);
        sideLen = p.machine == kVinyl ? (p.variant == 0 ? 1200.0 : (p.variant == 1 ? 420.0 : 240.0)) : 1200.0;
    }

    // Procesa en el lugar. `pos0`: posición absoluta de la primera muestra (del host o un contador propio).
    void process (float* l, float* r, int n, int64_t pos0)
    {
        for (int i = 0; i < n; ++i)
        {
            const int64_t pos = pos0 + i;
            double x[2] = { l[i], r[i] };
            dry[0].push (x[0]); dry[1].push (x[1]);
            double y[2];
            run (x, y, pos);
            const double d0 = dry[0].at (lat + 1), d1 = dry[1].at (lat + 1);      // at(1) es la última escrita
            l[i] = (float) (d0 + (y[0] - d0) * mix);
            r[i] = (float) (d1 + (y[1] - d1) * mix);
        }
        counter = pos0 + n;
    }

    // Para la interfaz.
    double vuLevel() const { return vu; }                   // nivel de grabación (RMS, 300 ms) respecto de la referencia
    double scopeH (int c) const { return ja[c].lastH(); }
    double scopeM (int c) const { return ja[c].lastM(); }
    double speed() const { return tr.speedNow(); }
    double flutterReading() const { return wfRead; }        // % ponderado, medido del desvío de velocidad
    double spacingNow() const { return spacingLast; }
    void reels (int64_t pos, double& rs, double& rt, double& ns, double& nt) const { tr.reels ((double) pos, rs, rt, ns, nt); }
    double groovePos (int64_t pos) const { return P.grooveFollow ? std::fmod ((double) pos / fs, sideLen) / sideLen : P.groove; }
    double rotation (int64_t pos) const { return prof.speed / 60.0 * (double) pos / fs; }   // vueltas del plato
    double scratchAngle (int k) const { return hashUni (P.seed ^ 0x5C, (uint64_t) k, 3); }
    const Profile& currentProfile() const { return prof; }
    // Eventos nuevos desde la última lectura (caídas, pops, rayas, estática), con su posición absoluta.
    int takeEvents (UiEvent* out, int max) { const int k = std::min (max, nev); for (int i = 0; i < k; ++i) out[i] = evs[(size_t) i]; nev = 0; return k; }
    double measuredHissDb() const { return noiseDb; }

private:
    EngineParams P; Profile prof;
    double fs = 48000; int lat = 1;
    dh::lq::Os4 os[2];
    Hysteresis ja[2];
    FirstOrder recHF[2], recLF[2], repHF[2], repLF[2], cutA[2], cutB[2], playA[2], playB[2];
    Biquad bump[2], bumpHp[2], azLp[2], modBp[2], traceHp[2], inner[2], lc[2], hc1[2], hc2[2], mHp[2], mLp[2];
    OnePole eraseLp[2], rumble[2];
    DcBlock traceDc[2];
    double eraseEnv[2] {};
    Svf loss[2];
    DelayLine dry[2], machDelay[2], azi[2];
    Transport tr; Dropouts drop; Impulses imp[2];
    Compander nrE[2], nrD[2];
    Radio radio; Phone phone; Crusher crush[2]; Varispeed vari[2];
    double tauLF = 0, riaaNorm = 1, spacing0 = 0, fAz = 1e9, azDelay = 0, noiseGain = 0, noiseDb = -300, humGain = 0, humHz = 60;
    double outGain = 1, mix = 1, sideLen = 1200, spacingLast = 0, innerG = -1, eraseAtt = 0.02, eraseRel = 0.002;
    bool lcOn = false, hcOn = false;
    int64_t counter = 0, lastRot = -1;
    double vu = 0, wfAcc = 0, wfRead = 0; int wfN = 0, wfDec = 0; Biquad wfBp;
    std::array<UiEvent, 64> evs {}; int nev = 0;
    inline void pushEvent (const UiEvent& e) { if (nev < (int) evs.size()) evs[(size_t) nev++] = e; }

    static double machineTrimDb (int m, int v)
    {
        switch (m)
        {
            case kRadio: return v == 0 ? -2.0 : -1.0;
            case kPhone: return -1.0;
            default: return 0.0;
        }
    }

    // RMS de un ruido blanco uniforme (varianza 1/3) después de la cadena de reproducción de esta máquina.
    double chainNoiseRms() const
    {
        const int N = 512; double acc = 0;
        const double fc = lossCutoff (spacing0, 1.0);
        for (int k = 1; k <= N; ++k)
        {
            const double f = (k - 0.5) / N * 0.5 * fs;
            double h = 1;
            if (prof.magnetic)
            {
                h *= 0.65 / std::sqrt (1.0 + std::pow (f / fc, 4.0)) + 0.35;        // 35 % del ruido no pasa por las pérdidas
                h *= repHF[0].mag (f, fs);                                         // el ruido entra después del golpe de cabeza
                if (tauLF > 0) h *= repLF[0].mag (f, fs) * 10.0;
            }
            else if (P.machine == kVinyl) h *= playA[0].mag (f, fs) * playB[0].mag (f, fs) * riaaNorm * mLp[0].mag (f, fs) * mHp[0].mag (f, fs);
            else h *= (f > prof.lfHz && f < prof.hfHz) ? 1.0 : 0.05;
            acc += h * h;
        }
        return std::sqrt (acc / N / 3.0);
    }
    // Corte (Hz) de las pérdidas de reproducción con una separación d (m) y la velocidad relativa s (§2.6):
    // la pérdida por separación cae 3 dB donde 54,6·d/λ = 3, o sea f = 3·v/(54,6·d).
    inline double lossCutoff (double d, double s) const
    {
        const double v = prof.speed * std::max (0.03, s);
        double inv = 1.0 / (prof.hfHz * prof.hfHz * std::max (0.03, s) * std::max (0.03, s));
        if (d > 1e-12) { const double fsp = 3.0 * v / (54.6 * d); inv += 1.0 / (fsp * fsp); }
        if (fAz < 1e8) inv += 1.0 / (fAz * fAz);
        return 1.0 / std::sqrt (inv);
    }

    inline void run (const double* in, double* out, int64_t pos)
    {
        const int m = P.machine;
        double v[2] = { in[0], in[1] };
        if (prof.magnetic)
        {
            for (int c = 0; c < 2; ++c)
            {
                double x = nrE[c].process (v[c]);
                // Énfasis de grabación antes de la no linealidad (por eso los agudos saturan antes), a la frecuencia
                // base: así grabación y reproducción son inversas exactas.
                x = recHF[c].process (x);
                if (tauLF > 0) x = 0.1 * recLF[c].process (x);
                x = os[c].process (x, [&] (double s) { return ja[c].process (s); });
                // Autoborrado: el bias borra en parte los agudos fuertes al salir de la cabeza (§2.12).
                if (prof.selfErase > 0)
                {
                    const double hf = x - eraseLp[c].lp (x);
                    const double a = std::fabs (hf);
                    eraseEnv[c] += (a - eraseEnv[c]) * (a > eraseEnv[c] ? eraseAtt : eraseRel);
                    const double e = prof.selfErase * (0.7 + 0.3 * P.bias) * clampd (eraseEnv[c] * 8.0, 0.0, 0.8);
                    x -= e * hf;
                }
                v[c] = x;
            }
            vu += ((v[0] * v[0] + v[1] * v[1]) * 0.5 - vu) * (1.0 / (0.3 * fs));
        }
        else if (m == kVinyl)
        {
            const double g = groovePos (pos);
            for (int c = 0; c < 2; ++c)
            {
                double x = cutB[c].process (cutA[c].process (v[c])) / riaaNorm;
                const double h = traceHp[c].process (x);
                const double th = std::tanh (h);
                x += traceDc[c].process ((0.25 + 2.5 * P.saturation * P.saturation) * (0.3 + 0.7 * g) * th * th);   // distorsión de lectura (par, acotada)
                v[c] = x;
            }
            vu += ((v[0] * v[0] + v[1] * v[1]) * 0.5 - vu) * (1.0 / (0.3 * fs));
            if (P.variant == 2) v[0] = v[1] = 0.5 * (v[0] + v[1]);
        }
        else
        {
            UiEvent ev; ev.kind = -1;
            const double mono = 0.5 * (v[0] + v[1]);
            const double y = m == kRadio ? radio.process (mono, pos, &ev) : phone.process (mono, pos);
            if (ev.kind >= 0) pushEvent (ev);
            v[0] = v[1] = y;
            vu += (mono * mono - vu) * (1.0 / (0.3 * fs));
        }
        // La histéresis atrasa 54 muestras; las máquinas sin ella pasan por la misma demora.
        if (! prof.magnetic)
            for (int c = 0; c < 2; ++c) { machDelay[c].push (v[c]); v[c] = machDelay[c].at (dh::lq::Os4::latency() + 1); }

        // Vinilo: alabeo en el canal lateral (movimiento vertical), por debajo del audio.
        const double tpos = (double) pos;
        double grooveScale = 1.0;
        if (m == kVinyl)
        {
            const double g = groovePos (pos);
            grooveScale = 146.0 / (146.0 - 86.0 * g);           // e/r: la excentricidad pesa más cerca del centro
            const double warp = 0.004 * clampd (P.wowPct / 0.4, 0.0, 2.0) * std::sin (kTwoPi * 2.0 * rotation (pos));
            v[0] += warp; v[1] -= warp;
        }
        (void) grooveScale;
        tr.process (v[0], v[1], tpos);

        if (prof.magnetic)
        {
            UiEvent ev; ev.kind = -1;
            const double dd = drop.process (pos, &ev);
            if (ev.kind >= 0) pushEvent (ev);
            const double d = spacing0 + dd;
            spacingLast = d;
            const double fc = lossCutoff (d, tr.speedNow());
            const double g = fastTan (kPi * std::min (fc, 0.45 * fs) / fs);
            // Ruido de la cinta, del amplificador (no se va en la caída) y de modulación.
            const double nL = noiseGain > 0 ? hashBi (P.seed, (uint64_t) pos, 501) * noiseGain : 0.0;
            const double nR = noiseGain > 0 ? hashBi (P.seed, (uint64_t) pos, 502) * noiseGain : 0.0;
            const double hum = humGain > 0 ? humGain * (std::sin (kTwoPi * humHz * tpos / fs) + 0.3 * std::sin (kTwoPi * 3 * humHz * tpos / fs)) : 0.0;
            for (int c = 0; c < 2; ++c)
            {
                double x = bumpHp[c].process (bump[c].process (v[c]));
                if (P.scrape > 0) x += modBp[c].process (hashBi (P.seed, (uint64_t) pos, 503 + (uint64_t) c)) * x * 0.12 * P.scrape;
                const double n = c == 0 ? nL : nR;
                loss[c].setG (g, 0.707);
                x = loss[c].lp (x + 0.65 * n) + 0.35 * n;
                azi[c].push (x);                                  // una muestra fija; con azimut, el derecho se atrasa más
                x = c == 1 && azDelay > 1e-6 ? azi[c].read (1.0 + azDelay) : azi[c].at (1);
                x = repHF[c].process (x);
                if (tauLF > 0) x = 10.0 * repLF[c].process (x + 0.5 * hum);   // refuerzo de graves de reproducción (NAB, casete)
                else x += hum;
                v[c] = nrD[c].process (x);
            }
        }
        else if (m == kVinyl)
        {
            const double g = groovePos (pos);
            // Crujido en tres clases (§2.13): crackle, clicks y pops; más las rayas en ángulos fijos del disco.
            const double c = P.crackle;
            const Impulses::Class cls[3] { { 30.0 + 1500.0 * c * c, -54.0 + 18.0 * c, 2.0, 0.12, 2500.0, 9000.0, 0 },
                                           { 0.1 + 12.0 * c * c, -34.0 + 14.0 * c, 2.0, 0.5, 800.0, 3500.0, 1 },
                                           { 0.4 * c * c, -24.0 + 10.0 * c, 2.0, 2.5, 120.0, 600.0, 1 } };
            double cl = 0, cr = 0;
            UiEvent ev; ev.kind = -1;
            if (c > 0) imp[0].process (pos, cls, 3, 0.5, P.seed ^ 0xC4A, cl, cr, &ev);
            if (ev.kind >= 0) pushEvent (ev);
            const double rot = rotation (pos), prevRot = rotation (pos - 1);
            for (int k = 0; k < P.scratches; ++k)
            {
                const double a = scratchAngle (k);
                if (std::floor (rot - a) != std::floor (prevRot - a))
                {
                    UiEvent e2; e2.kind = -1;
                    imp[1].scratch (pos, -16.0 + 3.0 * k, P.seed, k, &e2);
                    if (e2.kind >= 0) { e2.kind = 2; e2.depth = (float) a; pushEvent (e2); }
                }
            }
            imp[1].tail (cl, cr);
            const double sn = noiseGain > 0 ? hashBi (P.seed, (uint64_t) pos, 601) * noiseGain : 0.0;
            const double sd = noiseGain > 0 ? hashBi (P.seed, (uint64_t) pos, 602) * noiseGain * 0.6 : 0.0;
            const double rb = rumble[0].lp (hashBi (P.seed, (uint64_t) pos, 603)) * noiseGain * 12.0;
            const double hum = humGain > 0 ? humGain * std::sin (kTwoPi * humHz * tpos / fs) : 0.0;
            const double ext[2] = { cl + sn + sd + rb, cr + sn - sd + rb };
            if ((pos & 255) == 0 || innerG < 0)                // el surco interior pierde agudos (se actualiza cada 256 muestras)
            {
                innerG = g;
                for (auto& b : inner) { const double z1 = b.z1, z2 = b.z2; b.lowpass (std::min (20000.0 - 9000.0 * g, 0.45 * fs), 0.707, fs); b.z1 = z1; b.z2 = z2; }
            }
            for (int k = 0; k < 2; ++k)
            {
                double x = playB[k].process (playA[k].process (v[k] * riaaNorm + ext[k])) ;
                x = mLp[k].process (mHp[k].process (inner[k].process (x))) + hum;
                v[k] = x;
            }
            if (P.variant == 2) v[0] = v[1] = 0.5 * (v[0] + v[1]);
            for (int k = 0; k < 2; ++k) { azi[k].push (v[k]); v[k] = azi[k].at (1); }      // la misma muestra que el azimut
        }
        else
        {
            for (int k = 0; k < 2; ++k) { azi[k].push (v[k]); v[k] = azi[k].at (1); }
            // Radio y teléfono: ruido de línea y zumbido.
            const double n = noiseGain > 0 ? hashBi (P.seed, (uint64_t) pos, 701) * noiseGain : 0.0;
            const double hum = humGain > 0 ? humGain * (std::sin (kTwoPi * humHz * tpos / fs) + 0.5 * std::sin (kTwoPi * 2 * humHz * tpos / fs)) : 0.0;
            v[0] += n + hum; v[1] += n + hum;
        }

        for (int c = 0; c < 2; ++c)
        {
            double x = v[c];
            if (lcOn) x = lc[c].process (x);
            if (hcOn) x = hc2[c].process (hc1[c].process (x));
            x = vari[c].process (x, pos);
            x = crush[c].process (x, pos, c) * outGain;
            // La etapa de salida: lineal hasta +12 dBFS, después se satura suave (como el amplificador de una máquina).
            const double ax = std::fabs (x);
            if (ax > 4.0) x = (x > 0 ? 1.0 : -1.0) * (4.0 + 4.0 * std::tanh ((ax - 4.0) / 4.0));
            out[c] = x;
        }
        // Lectura de wow y flutter: el desvío de velocidad ponderado (pasa banda en 4 Hz, cada 32 muestras),
        // valor cuadrático medio en 2 s, por √2 (para un seno da su pico).
        if (++wfDec >= 32)
        {
            wfDec = 0;
            if (wfN == 0) wfBp.bandpass (4.0, 0.5, fs / 32.0);
            const double w = wfBp.process (tr.deviation());
            wfAcc += w * w; ++wfN;
            if (wfN >= (int) (2.0 * fs / 32.0)) { wfRead = std::sqrt (2.0 * wfAcc / wfN) * 100.0; wfAcc = 0; wfN = 1; }
        }
    }
};

} // namespace dh::tn
