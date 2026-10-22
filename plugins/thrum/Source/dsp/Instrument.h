#pragma once
// La guitarra completa hasta el jack (docs/research.md §1, §2): las cuerdas, el puente que las acopla, las pastillas
// con su posición y su red pasiva, el selector, el volumen, el tono, el cable y los ruidos que no son de la cuerda
// (chirrido de los dedos en el entorchado, zumbido de red y soplido de la pastilla). Sale la señal directa (DI), mono.
//
// Pastilla (por bobina y por cuerda): peine de posición c[n] = y[n] − y[n − γ·N], con γ = distancia al puente / largo
// vibrante (sube al pisar trastes más altos: la misma pastilla suena más "al medio" de la cuerda). Una simple promedia
// dos lecturas a ±5 mm (la apertura del imán); una doble, sus dos bobinas a ±9 mm. La suma de las cuerdas se integra
// una vez (el lazo está en aceleración; la bobina siente la velocidad) y pasa por la red de la bobina:
//     fuente → R + sL → nodo caliente ‖ C (propia + cable) ‖ pote de volumen ‖ amplificador (1 MΩ) ‖ (R_tono + 1/sC_tono)
// que da una transferencia de tercer orden (resonancia de 2-5 kHz) llevada a digital por la bilineal cada vez que se
// mueve un control. Dos pastillas en paralelo se tratan como una equivalente (L y R en paralelo, fuentes promediadas
// por 1/L). Bajar el volumen deja la resistencia del pote contra la capacidad del cable: un pasa bajos de un polo.

#include "Waveguide.h"

namespace dh::gt {

struct InstrumentParams
{
    int type = 0, tuning = kStandard, gauge = kRegular, capo = 0;
    std::array<int, kMaxStrings> custom {};
    float fineTune = 0.0f;           // cents
    float stringAge = 0.7f;          // 1 nuevas … 0 viejas
    float sustain = 1.0f;            // multiplicador del sustain del cuerpo
    int selector = 0;
    float volume = 1.0f, tone = 1.0f;
    float cableM = 3.0f;
    float sympathetic = 0.5f;        // acople del puente (cuerdas que resuenan por simpatía)
    bool unplayedMuted = false;      // las cuerdas que no se tocan quedan apagadas por la mano
    float buzz = 0.0f, action = 0.5f;
    float pickupNoise = 0.0f;        // zumbido y soplido de la pastilla
    bool mains60 = false;
    float squeak = 0.5f;             // chirrido de los dedos
};

class Instrument
{
public:
    void prepare (float sampleRate)
    {
        fs = sampleRate;
        for (int s = 0; s < kMaxStrings; ++s) str[(size_t) s].prepare (fs, s);
        integK = kTwoPi * 1000.0f / fs;
        tapK = 1.0f - fastExp (-1.0f / (0.0007f * fs));
        integR = fastExp (-kTwoPi * 12.0f / fs);
        dcHp.set (18.0f, fs);
        reset();
        configured = false;
        set (P);
    }
    void reset()
    {
        for (auto& s : str) s.reset();
        for (auto& b : bridge) b.reset();
        bPrev = 0; integ = 0; net.reset(); volLp.reset(); dcHp.reset(); hum = 0; t = 0;
        for (auto& q : squeaks) q = Squeak();
        noise.seed (0x51A7u);
    }

    void set (const InstrumentParams& p)
    {
        const bool physChanged = ! configured || p.type != P.type || p.tuning != P.tuning || p.gauge != P.gauge || p.capo != P.capo
                                 || p.custom != P.custom || std::fabs (p.fineTune - P.fineTune) > 1e-3f || std::fabs (p.stringAge - P.stringAge) > 1e-3f
                                 || std::fabs (p.sustain - P.sustain) > 1e-3f;
        const bool netChanged = physChanged || p.selector != P.selector || std::fabs (p.volume - P.volume) > 1e-4f || std::fabs (p.tone - P.tone) > 1e-4f
                                || std::fabs (p.cableM - P.cableM) > 1e-3f;
        P = p; configured = true;
        G = &guitar (P.type);
        nStrings = G->nStrings;
        if (physChanged)
        {
            notes = tuningNotes (*G, P.tuning, P.custom);
            for (int s = 0; s < kMaxStrings; ++s)
            {
                if (s >= nStrings) { str[(size_t) s].reset(); continue; }
                auto ph = stringPhysics (*G, s, notes[(size_t) s], P.gauge, P.capo);
                ph.openHz *= std::exp2 (P.fineTune / 1200.0f);
                // La segunda polarización, desafinada unos décimos de cent (más alta o más baja) distintos en cada cuerda:
                // batido lento.
                const float u2 = (float) hashUni (77, (uint64_t) s, (uint64_t) P.type);
                const float c2 = (0.15f + 0.5f * u2) * (hashUni (78, (uint64_t) s, (uint64_t) P.type) < 0.5 ? -1.0f : 1.0f);
                str[(size_t) s].setPhysics (ph, G->sustainS * P.sustain, G->brightS, P.stringAge, c2);
                str[(size_t) s].setNeckMode (G->neckHz, 4.0f);      // un parcial justo en el modo dura ~1/3 (punto muerto; docs/research.md §1.7)
                // Impedancia relativa de la cuerda: las gruesas mueven más el puente y lo sienten menos.
                z[(size_t) s] = std::sqrt (ph.tensionN * ph.muKgM) / 0.7f;
            }
            for (int b = 0; b < 3; ++b) bridge[(size_t) b].bandpass (G->body[(size_t) b].hz, G->body[(size_t) b].q, fs);
        }
        for (int s = 0; s < nStrings; ++s)
        {
            str[(size_t) s].setUnplayedMuted (P.unplayedMuted);
            str[(size_t) s].setBuzz (P.buzz, P.action);
            str[(size_t) s].setBridgeCoupling (0.0035f * P.sympathetic / std::max (0.2f, z[(size_t) s]));
        }
        mask = selectorMask (*G, P.selector);
        if (G->nPickups < 3) mask &= 3;
        if (netChanged) designNetwork();
    }
    const InstrumentParams& params() const { return P; }
    const GuitarSpec& spec() const { return *G; }
    int strings() const { return nStrings; }
    int openNote (int s) const { return notes[(size_t) std::clamp (s, 0, kMaxStrings - 1)] + P.capo; }
    int frets() const { return G->frets - P.capo; }
    StringModel& string (int s) { return str[(size_t) std::clamp (s, 0, kMaxStrings - 1)]; }
    const StringModel& string (int s) const { return str[(size_t) std::clamp (s, 0, kMaxStrings - 1)]; }
    // Distancia de un traste a la cejilla (mm).
    float fretMm (float fret) const { return G->scaleMm * (1.0f - std::exp2 (-(fret + (float) P.capo) / 12.0f)); }

    // Chirrido: el dedo que se desliza sobre una cuerda entorchada (o que cambia de posición) deja un tren de pulsos a
    // la frecuencia con que pasa por las vueltas del entorchado (velocidad / paso de la vuelta).
    void squeak (int s, float fromFret, float toFret, float seconds, float level)
    {
        if (P.squeak <= 0.0f || level <= 0.0f || s < 0 || s >= nStrings) return;
        if (! str[(size_t) s].physics().wound) return;
        const float mm = std::fabs (fretMm (toFret) - fretMm (fromFret));
        if (mm < 2.0f) return;
        auto& q = squeaks[(size_t) sqNext++ % squeaks.size()];
        q.len = std::max (1, (int) (seconds * fs)); q.pos = 0;
        const float pitchMm = 0.30f + 0.18f * clampf ((str[(size_t) s].physics().gaugeIn - 0.024f) / 0.04f, 0.0f, 1.0f);   // paso del entorchado
        q.hz = clampf (mm / std::max (0.02f, seconds) / pitchMm, 120.0f, 6000.0f);
        q.level = level * P.squeak * (0.6f + 0.4f * clampf (mm / 60.0f, 0.0f, 1.0f));
        q.phase = 0; q.bp.set (2600.0f + 900.0f * (float) (s % 3), 2.5f, fs); q.bp.reset(); q.hp.set (700.0f, fs); q.hp.reset();
        q.seed = (uint32_t) (s * 7919 + sqNext * 104729);
    }

    // n muestras de la salida directa (DI) en out (se escribe, no se suma).
    void process (float* out, int n)
    {
        const bool single = (mask & 1 ? G->pickup[0].kind == kSingle : false) || (mask & 2 ? G->pickup[1].kind == kSingle : false) || (mask & 4 ? G->pickup[2].kind == kSingle : false);
        const float humLevel = P.pickupNoise * (single ? 0.0035f : 0.00025f);
        const float hissLevel = P.pickupNoise * 0.00035f;
        const float humHz = P.mains60 ? 60.0f : 50.0f;
        for (int i = 0; i < n; ++i)
        {
            if ((t & 31) == 0) updateGeometry();
            // Cuerdas y puente.
            float force = 0.0f;
            for (int s = 0; s < nStrings; ++s) force += str[(size_t) s].tick (bPrev) * z[(size_t) s];
            float b = 0.0f;
            for (int k = 0; k < 3; ++k) b += bridge[(size_t) k].process (force) * G->body[(size_t) k].gain;
            bPrev = b;
            // Pastillas.
            float e = 0.0f;
            for (int s = 0; s < nStrings; ++s)
            {
                const auto& S = str[(size_t) s];
                if (! S.sounding()) { awake[(size_t) s] = false; continue; }
                const float N = S.loopSamples();
                const bool snap = ! awake[(size_t) s]; awake[(size_t) s] = true;          // recién despierta: sin glide
                float acc = 0.0f;
                for (int p = 0; p < 3; ++p)
                {
                    if (! (mask & (1 << p))) continue;
                    auto& g = geo[(size_t) s][(size_t) p];
                    // La lectura del peine se mueve suave (~0,7 ms): un cambio de traste no la hace saltar sobre la onda.
                    if (snap) { g.d0 = g.gamma0 * N; g.d1 = g.gamma1 * N; }
                    g.d0 += (g.gamma0 * N - g.d0) * tapK; g.d1 += (g.gamma1 * N - g.d1) * tapK;
                    const float c0V = S.latest (0) - S.tap (0, g.d0), c1V = S.latest (0) - S.tap (0, g.d1);
                    const float c0H = S.latest (1) - S.tap (1, g.d0), c1H = S.latest (1) - S.tap (1, g.d1);
                    acc += pickW[(size_t) p] * g.level * (0.5f * (c0V + c1V) + 0.45f * 0.5f * (c0H + c1H));
                }
                e += acc * sens[(size_t) s];
            }
            // Integración (velocidad), ruidos de la mano, zumbido y soplido.
            integ = e + integR * integ;
            float v = integ * integK;
            for (auto& q : squeaks) if (q.pos < q.len) v += q.tick (fs);
            if (P.pickupNoise > 0.0f)
            {
                const float ph = (float) ((double) t * humHz / fs - std::floor ((double) t * humHz / fs));
                const float h = sin2pi (ph) + 0.35f * sin2pi (2.0f * ph) + 0.6f * sin2pi (3.0f * ph) + 0.3f * sin2pi (5.0f * ph) + 0.18f * sin2pi (7.0f * ph);
                v += h * humLevel + noise.next() * hissLevel;
            }
            v += b * G->microphonic;
            float y = net.process (v);
            if (lpOn) y = volLp.process (y);
            out[i] = dcHp.process (y * outGain);
            ++t;
        }
    }

private:
    float fs = 48000.0f;
    InstrumentParams P; const GuitarSpec* G = &guitar (0); bool configured = false;
    int nStrings = 6, mask = 1;
    std::array<int, kMaxStrings> notes {};
    std::array<StringModel, kMaxStrings> str;
    std::array<float, kMaxStrings> z {}, sens {};
    std::array<Biquad, 3> bridge; float bPrev = 0.0f;
    struct Geo { float gamma0 = 0.2f, gamma1 = 0.2f, level = 1.0f, d0 = 10.0f, d1 = 10.0f; };
    float tapK = 0.03f; std::array<bool, kMaxStrings> awake {};
    std::array<std::array<Geo, 3>, kMaxStrings> geo {};
    std::array<float, 3> pickW {};
    float integ = 0, integK = 0.13f, integR = 0.998f;
    Iir3 net; Lp1 volLp; bool lpOn = false; Hp1 dcHp;
    float outGain = 1.0f;
    Noise noise; int64_t t = 0;
    float hum = 0;

    struct Squeak
    {
        int len = 0, pos = 0; float hz = 800, level = 0, phase = 0; Svf bp; Hp1 hp; uint32_t seed = 1;
        inline float tick (float sampleRate)
        {
            const float u = (float) pos / (float) len;
            const float envl = std::sin (kPi * u) * (1.0f - 0.3f * u);
            // La velocidad del dedo no es pareja: la frecuencia tiembla un poco.
            seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5;
            const float jitter = 1.0f + 0.12f * ((float) (int32_t) seed * (1.0f / 2147483648.0f));
            phase += hz * jitter * (0.8f + 0.4f * u) / sampleRate;
            float pulse = 0.0f;
            if (phase >= 1.0f) { phase -= std::floor (phase); pulse = 1.0f; }
            ++pos;
            return hp.process (bp.bandpass (pulse)) * level * envl * 0.08f;
        }
    };
    std::array<Squeak, 3> squeaks {}; int sqNext = 0;

    // Geometría de las pastillas para cada cuerda (posición relativa al largo vibrante se recalcula por traste en
    // updateGeometry; acá la posición absoluta y la apertura) y la red eléctrica del selector.
    void designNetwork()
    {
        double invL = 0, invR = 0, cSelf = 0; int count = 0;
        for (int p = 0; p < 3; ++p)
        {
            if (! (mask & (1 << p))) { pickW[(size_t) p] = 0; continue; }
            const auto& pk = G->pickup[(size_t) p];
            invL += 1.0 / pk.henry; invR += 1.0 / pk.ohm; cSelf += pk.selfPf * 1e-12; ++count;
        }
        for (int p = 0; p < 3; ++p)
            if (mask & (1 << p)) pickW[(size_t) p] = (float) ((1.0 / G->pickup[(size_t) p].henry) / invL) * G->pickup[(size_t) p].output * (count > 1 ? 1.15f : 1.0f);
        const double L = 1.0 / std::max (1e-6, invL), R = 1.0 / std::max (1e-9, invR);
        // Volumen (audio) y tono (audio): la resistencia que ve la bobina y el divisor del pote.
        const double vol = std::pow (clampf (P.volume, 0.0f, 1.0f), 2.2);
        const double toneT = std::pow (clampf (P.tone, 0.0f, 1.0f), 2.2);
        const double cable = (P.cableM * 100.0 + 50.0) * 1e-12;
        const double C = cSelf + cable * vol * vol;
        const double Rv = G->potVolOhm, Ramp = 1e6;
        const double Gl = 1.0 / Rv + 1.0 / Ramp;
        const double Rt = G->potToneOhm * toneT + 220.0, Ct = G->toneCapNf * 1e-9;
        const double tt = Rt * Ct;
        // H(s) = (1 + s·τt) / [ (1 + sτt)(1 + (R + sL)(sC + G)) + (R + sL)·s·Ct ]
        const double a0 = 1.0 + R * Gl;
        const double a1 = R * C + L * Gl + a0 * tt + R * Ct;
        const double a2 = L * C + (R * C + L * Gl) * tt + L * Ct;
        const double a3 = L * C * tt;
        const double B[4] = { a0, a0 * tt, 0, 0 };               // ganancia de continua 1
        const double A[4] = { a0, a1, a2, a3 };
        const double fRes = 1.0 / (2.0 * kPiD * std::sqrt (L * C));
        net.fromAnalog (B, A, fs, std::min (fRes, 0.4 * fs));
        // El divisor del volumen y su resistencia de salida contra el cable.
        const double Rout = vol * (1.0 - vol) * Rv;
        lpOn = Rout > 100.0;
        if (lpOn) volLp.set ((float) std::min (0.45 * fs, 1.0 / (2.0 * kPiD * Rout * cable)), fs);
        outGain = (float) vol * 2.2f;
        // Geometría: posición de cada bobina para cada cuerda (las inclinadas cambian de cuerda a cuerda).
        for (int s = 0; s < nStrings; ++s)
        {
            const float across = nStrings > 1 ? (float) (nStrings - 1 - s) / (float) (nStrings - 1) : 0.0f;
            const float L0 = str[(size_t) s].physics().lengthMm;
            for (int p = 0; p < 3; ++p)
            {
                const auto& pk = G->pickup[(size_t) p];
                const float pos = pk.posMm + pk.slantMm * across;
                const float half = pk.kind == kHumbucker ? 9.0f : 5.0f;
                auto& g = geo[(size_t) s][(size_t) p];
                g.gamma0 = (pos - half) / L0; g.gamma1 = (pos + half) / L0; g.level = 1.0f;
            }
            // Sensibilidad por cuerda: más hierro en las entorchadas, compensado por su menor amplitud.
            sens[(size_t) s] = str[(size_t) s].physics().wound ? 1.0f : 1.12f;
        }
        geoFret.fill (-1.0f);
    }
    std::array<float, kMaxStrings> geoFret {};

public:
    // La posición relativa crece al pisar trastes (el largo vibrante se acorta): se ajusta al cambiar de traste.
    void updateGeometry()
    {
        for (int s = 0; s < nStrings; ++s)
        {
            const float f = str[(size_t) s].currentFret();
            if (std::fabs (f - geoFret[(size_t) s]) < 0.02f) continue;
            geoFret[(size_t) s] = f;
            const float Lf = str[(size_t) s].physics().lengthMm * std::exp2 (-f / 12.0f);
            const float across = nStrings > 1 ? (float) (nStrings - 1 - s) / (float) (nStrings - 1) : 0.0f;
            for (int p = 0; p < 3; ++p)
            {
                const auto& pk = G->pickup[(size_t) p];
                const float pos = pk.posMm + pk.slantMm * across;
                const float half = pk.kind == kHumbucker ? 9.0f : 5.0f;
                auto& g = geo[(size_t) s][(size_t) p];
                // Más allá del traste la cuerda no vibra: la bobina queda sobre un nodo (se apaga suavemente).
                g.gamma0 = clampf ((pos - half) / Lf, 0.01f, 0.98f);
                g.gamma1 = clampf ((pos + half) / Lf, 0.01f, 0.98f);
                g.level = clampf ((Lf - pos) / 15.0f, 0.0f, 1.0f);
            }
        }
    }
};

} // namespace dh::gt
