#pragma once
// Una cuerda de Thrum (docs/research.md §1, §8): guía de onda en la forma de un solo lazo (SDL, Karjalainen, Välimäki y
// Tolonen 1998), una por polarización (vertical y horizontal: dos decaimientos y batido). El lazo lleva la señal en el
// dominio de la aceleración: una púa ideal es un impulso, la pastilla magnética siente la velocidad (se integra una vez
// en Pickups.h). Por vuelta, en orden:
//   1. lectura del retardo con Lagrange de quinto orden (fraccional, modulable: bends, slides, vibrato, palanca);
//   2. filtro de pérdidas de un polo g·(1 − a)/(1 − a·z⁻¹), ajustado a dos T60 (fundamental y 3 kHz);
//   3. seis pasa todos de primer orden iguales para la inarmonicidad, con el coeficiente ajustado por nota para que
//      el parcial ~4 kHz caiga donde lo pone la rigidez (k·f0·√(1 + B·k²)); el retardo total se corrige con la fase de
//      los filtros en la fundamental, así la nota queda afinada;
//   4. contacto (la púa que vuelve a tocar una cuerda que suena la frena durante una vuelta) y zumbido de traste;
//   5. excitación (pulso de la púa o del dedo, filtrado por la dureza, con el peine de la posición de ataque y, en los
//      armónicos, el peine que deja sólo los múltiplos de k), acople del puente y realimentación controlada.
// Una nota nueva en la misma cuerda corta la anterior: la púa la frena al tocarla. El ligado (hammer-on, pull-off,
// slide) cambia el largo sin volver a excitar, salvo el golpe chico del dedo.

#include "Guitar.h"

namespace dh::gt {

struct PluckSpec
{
    float fret = 0.0f;           // traste (desde el capo; fraccional en slides)
    float velocity = 0.8f;       // 0..1
    float posMm = 95.0f;         // distancia de la púa al puente
    float hardness = 0.6f;       // 0 blanda … 1 dura
    float angle = 0.35f;         // 0 paralela al cuerpo … 1 perpendicular
    int dir = 1;                 // +1 hacia abajo, −1 hacia arriba
    bool finger = false;         // dedo (más ancho, más oscuro, más vertical)
    int harmonic = 0;            // k ≥ 2: armónico (natural o artificial: el dedo en 1/k del largo)
    float harmonicMix = 1.0f;    // 1 = armónico puro; el pinch deja pasar algo de la fundamental
    float mute = 0.0f;           // palm mute en el ataque (0..1)
    bool dead = false;           // nota muerta (la mano izquierda apoyada, sin presionar)
    float noise = 0.3f;          // ruido de la púa contra la cuerda
    float cents = 0.0f;          // microafinación de este ataque (variación humana)
    uint64_t seed = 1;           // ruido de este ataque (hash del evento)
};

enum class Release { lift, mute, ring };

class StringModel
{
public:
    static constexpr int kDisp = 6;               // pasa todos de dispersión
    static constexpr int kCtl = 8;                // cada cuántas muestras se recalcula la afinación
    static constexpr int kExcMax = 16384;

    void prepare (float sampleRate, int index)
    {
        fs = sampleRate; idx = index;
        velK = 48000.0f / fs; velR = fastExp (-0.005f * 48000.0f / fs);   // integración de la velocidad, igual a cualquier fs
        // La nota más grave posible: un B0 con la palanca a −1 octava a 192 kHz cabe en 2^14.
        for (auto& l : line) l.prepare ((int) (fs / 14.0f) + 64);
        exc.assign ((size_t) kExcMax, 0.0f); work.assign ((size_t) kExcMax, 0.0f); work2.assign ((size_t) kExcMax, 0.0f);
        fbBp.reset();
        reset();
    }
    void reset()
    {
        for (auto& l : line) l.clear();
        for (int p = 0; p < 2; ++p) { lz[p] = 0; for (int k = 0; k < kDisp; ++k) apx[p][k] = apy[p][k] = 0; out[p] = 0; neck[p].reset(); }
        excLen = excPos = 0; contactLeft = 0; contactPos = 0; contactGain = 1.0f; holdN = 0;
        vel = env = fbEnv = fbRamp = 0; quiet = 1 << 20; asleep = true;
        fret = fretTarget = fingerPos = 0; slideLeft = 0; bend = bendTarget = vib = whammy = 0; glideTau = 0;
        gate = false; palm = palmTarget = 0; loss = LossState::open; lossDirty = true; ctl = 0; dPrev[0] = dPrev[1] = -1;
        fbBp.reset(); fbOn = false;
    }

    // Física: frecuencia al aire, inarmonicidad, sustain del cuerpo. Invalida la tabla de dispersión.
    void setPhysics (const StringPhysics& p, float sustainS, float brightS, float ageNew, float pol2Cents)
    {
        phys = p;
        susS = sustainS; brS = brightS; age = ageNew;
        detune2 = std::exp2 (pol2Cents / 1200.0f);
        // Glissando de tensión: más en las cuerdas de menos tensión (Tolonen, Välimäki y Karjalainen 2000).
        glideK = 0.0046f * (75.0f / std::max (20.0f, p.tensionN));          // +8 cents con una púa fuerte a 75 N
        for (auto& d : dispValid) d = false;
        lossDirty = true;
    }
    // Modo del mástil (punto muerto): un parcial que cae cerca pierde energía en el mástil y dura menos. t60 es el
    // decaimiento extra que le agrega a un parcial justo en el modo (s); 0 = sin modo.
    void setNeckMode (float hz, float t60) { neckHz = hz; neckT60 = t60; lossDirty = true; }
    const StringPhysics& physics() const { return phys; }
    void setUnplayedMuted (bool m) { unplayedMuted = m; lossDirty = true; }
    void setBuzz (float amount, float action) { buzz = clampf (amount, 0.0f, 1.0f); buzzLevel = 0.02f + 0.18f * clampf (action, 0.0f, 1.0f); }

    // ── Interpretación ──
    void pluck (const PluckSpec& s)
    {
        asleep = false; quiet = 0;
        // Si la cuerda suena, la púa la frena al apoyarse: durante una vuelta del lazo todo lo que pasa por el punto de
        // contacto sale con ganancia baja (la nota anterior se corta); la excitación nueva entra después de ese punto y
        // vuelve a él justo cuando el contacto terminó (la ventana es el retardo de lectura menos dos muestras).
        const bool wasSounding = env > 1e-4f;
        openPending = 0;
        const float dOld0 = dTo[0], dOld1 = dTo[1], adOld = ad;
        fret = fretTarget = fingerPos = s.fret; slideLeft = 0; glideTau = 0;
        bend = bendTarget; dPrev[0] = dPrev[1] = -1; fine = s.cents / 100.0f;
        gate = true; dead = s.dead;
        palmTarget = s.mute; palm = s.mute;
        loss = s.dead ? LossState::dead : LossState::play;
        lossDirty = true;
        updatePitch (true);
        if (wasSounding)
        {
            contactLeft = std::max (2, (int) dTo[0] - 2); contactTarget = s.dead ? 0.0f : 0.05f; contactPos = 0;
            // El largo nuevo entra cuando la púa ya frenó la vibración vieja (16 muestras): si el retardo saltara antes,
            // el salto de la lectura sobre la onda vieja sería un clic.
            holdN = 16; dNew[0] = dTo[0]; dNew[1] = dTo[1]; adNew = ad;
            dFrom[0] = dTo[0] = dOld0; dFrom[1] = dTo[1] = dOld1; ad = adOld;
        }
        buildExcitation (s);
        fbRamp = 0;
    }
    // Ligado: hammer-on (kind 0) o pull-off (kind 1) a otro traste sin púa; el dedo da un golpe chico.
    void legato (float toFret, int kind, float velocity, uint64_t seed)
    {
        asleep = false; quiet = 0;
        fretTarget = fingerPos = toFret; slideLeft = 0; openPending = 0;
        glideTau = 0.0006f;
        gate = true;
        if (loss != LossState::dead) loss = LossState::play;
        lossDirty = true;
        PluckSpec s;
        s.fret = toFret; s.velocity = velocity * (kind == 0 ? 0.30f : 0.42f);
        // El golpe del dedo cae junto al traste (hammer) o el dedo tira la cuerda al soltarla (pull-off).
        s.posMm = 0.0f; s.hardness = kind == 0 ? 0.75f : 0.35f; s.angle = kind == 0 ? 0.9f : 0.3f; s.finger = true; s.noise = 0.05f; s.seed = seed;
        posNearFret = true;
        buildExcitation (s, true);
        posNearFret = false;
    }
    // Slide: el dedo recorre los trastes; la altura sigue a los trastes que pisa (escalones suavizados).
    void slideTo (float toFret, float seconds)
    {
        asleep = false; quiet = 0;
        slideFrom = fingerPos; slideTarget = toFret;
        slideLeft = slideTotal = std::max (1, (int) (seconds * fs));
        fretTarget = toFret;
        gate = true;
    }
    // Fin de la nota.
    void release (Release how)
    {
        gate = false;
        if (how == Release::ring) return;
        loss = how == Release::lift ? LossState::lifted : LossState::muted;
        lossDirty = true;
    }
    // Soltar el dedo de un traste deja sonar un instante la cuerda al aire (el "ruido de soltada").
    void releaseToOpen (float level, uint64_t seed)
    {
        gate = false;
        loss = LossState::lifted; lossDirty = true;
        openPending = 0;
        if (fret < 0.5f || level <= 0.0f) return;
        // El dedo que se levanta primero frena la cuerda (~35 ms apoyado) y recién después la deja libre: entonces la
        // cuerda pasa a sonar al aire, apenas, con lo que el dedo le dio al soltarla (el "ruido de soltada").
        openPending = std::max (1, (int) (0.035f * fs)); openLevel = level; openSeed = seed;
    }
    void setBend (float semitones) { bendTarget = semitones; }
    void setVibrato (float semitones) { vib = semitones; }
    void setWhammy (float semitones) { whammy = semitones; }
    void setPalmMute (float m) { if (std::fabs (m - palmTarget) > 1e-4f) { palmTarget = m; lossDirty = true; } }
    void setFeedback (float amount, int harmonic) { fbAmount = clampf (amount, 0.0f, 1.0f); fbHarm = std::clamp (harmonic, 1, 4); fbOn = fbAmount > 0.0f; }
    void setBridgeCoupling (float inGain) { couple = inGain; }

    bool sounding() const { return ! asleep; }
    bool gated() const { return gate; }
    float currentFret() const { return fret; }
    float dispersionCoef() const { return ad; }
    float lossCoef() const { return aL; }
    float envelope() const { return env; }
    float pitchHz() const { return curHz; }
    float loopSamples() const { return fs / std::max (10.0f, curHz); }

    // Una muestra. bridgeIn: el movimiento del puente (del paso anterior). Devuelve la fuerza sobre el puente.
    inline float tick (float bridgeIn)
    {
        if (asleep)
        {
            if (std::fabs (bridgeIn * couple) < 1e-9f) { out[0] = out[1] = 0.0f; line[0].push (0.0f); line[1].push (0.0f); return 0.0f; }
            asleep = false; quiet = 0;
        }
        if (openPending > 0 && --openPending == 0 && ! gate)
        {
            fretTarget = fingerPos = 0.0f; slideLeft = 0; glideTau = 0.0008f;
            PluckSpec s; s.fret = 0; s.velocity = openLevel; s.posMm = 0.0f; s.hardness = 0.05f; s.angle = 0.3f; s.finger = true; s.noise = 0.0f; s.seed = openSeed;
            posNearFret = true; buildExcitation (s, true); posNearFret = false;
        }
        if (holdN > 0) { if (--holdN == 0) { dFrom[0] = dTo[0] = dNew[0]; dFrom[1] = dTo[1] = dNew[1]; ad = adNew; } }
        else if (ctl == 0) updatePitch (false);
        const float t = (float) (kCtl - ctl) / (float) kCtl;          // interpolación del retardo dentro del tramo
        if (++ctl >= kCtl) ctl = 0;
        if (lossDirty) updateLoss();
        // Coeficientes de pérdida suavizados (palm mute, apagar): ~3 ms.
        g += (gT - g) * lossK; aL += (aLT - aL) * lossK; g2 += (g2T - g2) * lossK;
        float e = 0.0f;
        if (excPos < excLen) e = exc[(size_t) excPos++];
        if (contactLeft > 0)
        {
            // Entrada en ~16 muestras (coseno) para no cortar la forma de onda vieja de golpe; la salida es inmediata (lo
            // que pasa después ya es la nota nueva).
            const float u = std::min (1.0f, (float) ++contactPos / 16.0f);
            contactGain = 1.0f + (contactTarget - 1.0f) * (0.5f - 0.5f * cos2pi (0.5f * u));
            if (--contactLeft == 0) contactGain = 1.0f;
        }
        const float bin = bridgeIn * couple;
        float fbIn = 0.0f;
        if (fbOn) fbIn = feedbackSample();
        float maxAbs = 0.0f;
        for (int p = 0; p < 2; ++p)
        {
            const float d = dFrom[p] + (dTo[p] - dFrom[p]) * t;
            float y = line[p].lagrange5 (d);
            const float gg = p == 0 ? g : g2;
            lz[p] = gg * (1.0f - aL) * y + aL * lz[p];
            y = lz[p];
            if (neckOn) y = neck[p].process (y);
            for (int k = 0; k < kDisp; ++k)
            {
                const float o = ad * (y - apy[p][k]) + apx[p][k];
                apx[p][k] = y; apy[p][k] = o; y = o;
            }
            y *= contactGain;
            if (buzz > 0.0f && p == 0)
            {
                // Zumbido de traste: la cuerda choca con el traste de un solo lado cuando la amplitud lo alcanza.
                const float over = y - buzzLevel;
                if (over > 0.0f) y -= buzz * 0.85f * over;
            }
            y += (p == 0 ? wV : wH) * e + bin * (p == 0 ? 1.0f : 0.5f) + fbIn * (p == 0 ? 1.0f : 0.3f);
            line[p].push (y);
            out[p] = y;
            maxAbs = std::max (maxAbs, std::fabs (y));
        }
        // Velocidad (integrada) para la envolvente: guía el glissando de tensión y el control de la realimentación.
        vel = out[0] * velK + velR * vel;
        const float av = std::fabs (vel);
        env = av > env ? av : env * envRel;
        // Dormir: sin señal por más de una vuelta y sin nada pendiente.
        if (maxAbs < 1e-7f && excPos >= excLen && ! fbOn) { if (++quiet > (int) loopSamples() + 256) { asleep = true; vel = env = 0; } }
        else quiet = 0;
        return out[0] + 0.5f * out[1];
    }
    // Lectura del lazo `d` muestras atrás (para el peine de las pastillas), por polarización.
    inline float tap (int p, float d) const { return line[p].linear (d); }
    inline float latest (int p) const { return out[p]; }

private:
    enum class LossState { play, open, lifted, muted, dead };
    float fs = 48000.0f; int idx = 0;
    StringPhysics phys;
    Line line[2];
    float lz[2] {}, apx[2][kDisp] {}, apy[2][kDisp] {}, out[2] {};
    float ad = 0.0f;                                // coeficiente de dispersión del traste actual
    std::array<float, kMaxFrets + 13> dispTab {}; std::array<bool, kMaxFrets + 13> dispValid {};
    float dFrom[2] { 10, 10 }, dTo[2] { 10, 10 }, dPrev[2] { -1, -1 };
    int ctl = 0;
    // Excitación pendiente.
    std::vector<float> exc, work, work2; int excLen = 0, excPos = 0;
    float wV = 0.5f, wH = 0.8f;
    bool posNearFret = false;
    // Contacto de la púa.
    int contactLeft = 0, contactPos = 0; float contactGain = 1.0f, contactTarget = 0.05f;
    // Altura.
    float fret = 0, fretTarget = 0, fingerPos = 0, slideFrom = 0, slideTarget = 0; int slideLeft = 0, slideTotal = 1;
    float glideTau = 0.0f, bend = 0, bendTarget = 0, vib = 0, whammy = 0, curHz = 82.4f, detune2 = 1.0f;
    float glideK = 0.01f, fine = 0.0f;
    // Pérdidas.
    LossState loss = LossState::open; bool lossDirty = true, dead = false, unplayedMuted = true;
    float g = 0.99f, aL = 0.1f, g2 = 0.99f, gT = 0.99f, aLT = 0.1f, g2T = 0.99f, lossK = 0.01f;
    float susS = 12.0f, brS = 1.0f, age = 1.0f, palm = 0.0f, palmTarget = 0.0f;
    float lastLossHz = -1.0f;
    // Envolvente, sueño.
    float vel = 0, env = 0, envRel = 0.9995f, velK = 1.0f, velR = 0.995f; int quiet = 0; bool asleep = true, gate = false;
    // Zumbido, acople, realimentación.
    float buzz = 0.0f, buzzLevel = 0.1f, couple = 0.0f;
    int openPending = 0; float openLevel = 0.0f; uint64_t openSeed = 1;
    int holdN = 0; float dNew[2] {}, adNew = 0.0f;
    Biquad neck[2]; float neckHz = 0.0f, neckT60 = 0.0f; bool neckOn = false;
    bool fbOn = false; float fbAmount = 0, fbEnv = 0, fbRamp = 0; int fbHarm = 1; Svf fbBp;

    // ── Afinación ──
    // Fase (atraso, radianes) de un pasa todos de primer orden (a + z⁻¹)/(1 + a·z⁻¹) y del pasa bajos de pérdidas.
    static double apPhase (double a, double w) { return w - 2.0 * std::atan2 (a * std::sin (w), 1.0 + a * std::cos (w)); }
    static double lpPhase (double a, double w) { return std::atan2 (a * std::sin (w), 1.0 - a * std::cos (w)); }

    // Coeficiente de dispersión de un traste. Seis pasa todos iguales no siguen la curva de la cuerda rígida en todo el
    // espectro: se elige el coeficiente que minimiza el máximo error (en cents) de los parciales hasta 1,5 kHz (hasta
    // el 16.º, y mientras la cuerda rígida no se aparte más de 50 cents de la serie armónica), donde el oído juzga la
    // afinación y el batido; más arriba queda alrededor de la mitad de la desviación real (docs/research.md §8).
    // Búsqueda de sección áurea; cada parcial por tres pasos de Newton desde k·f0. Se calcula la primera vez que se
    // pisa el traste (~0,1 ms) y queda en la tabla.
    float dispersionFor (int fr)
    {
        fr = std::clamp (fr, 0, (int) dispTab.size() - 1);
        if (dispValid[(size_t) fr]) return dispTab[(size_t) fr];
        const double f0 = phys.openHz * std::exp2 (fr / 12.0), B = fretB (phys, (float) fr);
        int K = std::clamp ((int) (std::min (1500.0, 0.3 * fs) / f0), 2, 16);
        while (K > 3 && 600.0 * std::log2 (1.0 + B * K * K) > 50.0) --K;     // hasta ~50 cents de desviación
        const double w0 = 2.0 * kPiD * f0 / fs, aL0 = lossFor (LossState::play, (float) f0, 0.0f).a;   // la fase de las pérdidas también dispersa
        double a = 0.0;
        if (B * K * K > 2e-6)
        {
            auto maxErr = [&] (double aa) {
                const double d = (2.0 * kPiD - kDisp * apPhase (aa, w0) - lpPhase (aL0, w0)) / w0;
                auto phi = [&] (double w) { return w * d + kDisp * apPhase (aa, w) + lpPhase (aL0, w); };
                double worst = 0.0;
                for (int k = 2; k <= K; ++k)
                {
                    double w = k * w0;
                    for (int it = 0; it < 3; ++it)
                    {
                        const double h = 1e-4 * w0, p = phi (w), dp = (phi (w + h) - phi (w - h)) / (2.0 * h);
                        w -= (p - 2.0 * kPiD * k) / std::max (1e-6, dp);
                    }
                    const double target = partialHz (f0, B, k) * 2.0 * kPiD / fs;
                    worst = std::max (worst, std::fabs (std::log2 (w / target)));
                }
                return worst;
            };
            double lo = -0.95, hi = 0.0;
            const double gr = 0.6180339887498949;
            double x1 = hi - gr * (hi - lo), x2 = lo + gr * (hi - lo), e1 = maxErr (x1), e2 = maxErr (x2);
            for (int it = 0; it < 28; ++it)
            {
                if (e1 < e2) { hi = x2; x2 = x1; e2 = e1; x1 = hi - gr * (hi - lo); e1 = maxErr (x1); }
                else { lo = x1; x1 = x2; e1 = e2; x2 = lo + gr * (hi - lo); e2 = maxErr (x2); }
            }
            a = 0.5 * (lo + hi);
        }
        dispTab[(size_t) fr] = (float) a; dispValid[(size_t) fr] = true;
        return (float) a;
    }

    void updatePitch (bool snap)
    {
        // Slide: el dedo avanza en línea recta; la altura salta de traste en traste con un suavizado de ~3 ms.
        if (slideLeft > 0)
        {
            slideLeft = std::max (0, slideLeft - kCtl);
            const float u = 1.0f - (float) slideLeft / (float) slideTotal;
            fingerPos = slideFrom + (slideTarget - slideFrom) * u;
            const float stepped = std::round (fingerPos);
            fret += (stepped - fret) * (1.0f - fastExp (-(float) kCtl / (0.003f * fs)));
            if (slideLeft == 0) fret = slideTarget;
        }
        else if (glideTau > 0.0f && ! snap)
        {
            fret += (fretTarget - fret) * (1.0f - fastExp (-(float) kCtl / (glideTau * fs)));
            if (std::fabs (fretTarget - fret) < 1e-4f) { fret = fretTarget; glideTau = 0; }
        }
        else fret = fretTarget;
        bend += (bendTarget - bend) * (snap ? 1.0f : 1.0f - fastExp (-(float) kCtl / (0.004f * fs)));
        // Glissando de tensión: la amplitud estira la cuerda (sube la altura al principio de una nota fuerte).
        const float glide = 1.0f + glideK * std::min (env * env * 11.0f, 2.0f);      // (env/0,3)²
        const float semisTot = fret + bend + vib + whammy + fine;
        const float hz = phys.openHz * fastExp2 (semisTot * (1.0f / 12.0f)) * glide;
        curHz = clampf (hz, 15.0f, 0.3f * fs);
        const int fr = std::clamp ((int) std::lround (fret), 0, (int) dispTab.size() - 1);
        ad = dispersionFor (fr);
        if (std::fabs (curHz - lastLossHz) > 0.02f * lastLossHz) lossDirty = true;
        for (int p = 0; p < 2; ++p)
        {
            const double f = curHz * (p == 1 ? detune2 : 1.0f);
            const double w = 2.0 * kPiD * f / fs;
            const double N = fs / f;
            double tau = (kDisp * apPhase (ad, w) + lpPhase (aL, w)) / w;
            if (neckOn) tau += -std::arg (neck[p].response (f, fs)) / w;          // la fase del modo también corre la nota
            const float dn = (float) std::max (1.0, N - tau);
            dFrom[p] = (snap || dPrev[p] < 0) ? dn : dTo[p];
            dTo[p] = dn; dPrev[p] = dn;
        }
    }

    // Pérdidas: g y a del pasa bajos desde los T60 de la fundamental y de 3 kHz para un estado (función pura).
    struct LossCoef { double g, a; };
    LossCoef lossFor (LossState st, float f0, float palmAmt) const
    {
        // Cuerda abierta tocada: el sustain del cuerpo, más corto arriba en el mástil y en las notas agudas.
        float t0 = susS * std::pow (82.4f / std::max (40.0f, f0), 0.35f) * (0.75f + 0.25f * age);
        float th = brS * (0.35f + 0.65f * age);
        switch (st)
        {
            case LossState::play: break;
            case LossState::open: if (unplayedMuted) { t0 = 0.25f; th = 0.03f; } break;
            case LossState::lifted: t0 = 0.10f; th = 0.012f; break;
            case LossState::muted: t0 = 0.16f; th = 0.02f; break;
            case LossState::dead: t0 = 0.035f; th = 0.006f; break;
        }
        // Palm mute: la palma junto al puente acorta la nota y se come los agudos (más cuanto más presiona).
        // (Interpolación en escala logarítmica: con la mitad de la palma la nota ya dura una fracción, como en la guitarra.)
        if (palmAmt > 0.0f && st == LossState::play)
        {
            const float w0 = std::pow (palmAmt, 0.6f), wh = std::pow (palmAmt, 0.4f);
            t0 = std::exp (lerp (std::log (t0), std::log (0.09f), w0));
            th = std::exp (lerp (std::log (th), std::log (0.008f), wh));
        }
        th = std::min (th, t0 * 0.9f);
        const double w0 = 2.0 * kPiD * f0 / fs;
        const double fh = std::max (3000.0, 2.5 * (double) f0);
        const double wh = 2.0 * kPiD * std::min (fh, 0.45 * fs) / fs;
        const double G0 = std::pow (10.0, -3.0 / (t0 * f0)), Gh = std::pow (10.0, -3.0 / (th * f0));
        const double r = std::min (0.999999, (Gh / G0) * (Gh / G0));
        const double c = std::cos (wh);
        // (1 − a)² = r·(1 − 2a·c + a²)  →  (1 − r)a² − 2a(1 − r·c) + (1 − r) = 0
        const double A = 1.0 - r, Bq = -2.0 * (1.0 - r * c);
        double a = 0.0;
        if (A > 1e-12) { const double disc = std::max (0.0, Bq * Bq - 4.0 * A * A); a = (-Bq - std::sqrt (disc)) / (2.0 * A); }
        a = std::clamp (a, 0.0, 0.97);
        const double mag0 = std::abs (std::complex<double> (1.0 - a * std::cos (w0), a * std::sin (w0)));
        return { std::min (0.99995, G0 * mag0 / (1.0 - a)), a };
    }
    void updateLoss()
    {
        lossDirty = false;
        lastLossHz = curHz;
        palm = palmTarget;
        const auto lc = lossFor (loss, curHz, palm);
        gT = (float) lc.g; aLT = (float) lc.a;
        // La horizontal pierde menos en el puente: dura 1,6 veces más.
        g2T = (float) std::min (0.99997, std::pow (lc.g, 1.0 / 1.6));
        // Pérdida extra por vuelta en el modo del mástil: la tasa de decaimiento extra (60/T60 dB/s) repartida en las f0
        // vueltas por segundo. Un pico negativo angosto (Q 12) en el lazo.
        neckOn = neckT60 > 0.0f && neckHz > 0.0f && neckHz < 0.45f * fs;
        if (neckOn)
        {
            const double dbPerTrip = -60.0 / neckT60 / std::max (20.0f, curHz);
            for (int p = 0; p < 2; ++p) neck[p].peak (neckHz, 12.0, std::max (-3.0, dbPerTrip), fs);
        }
        const float k = loss == LossState::lifted ? 0.006f : loss == LossState::muted ? 0.008f : 0.003f;   // suavizado (s)
        lossK = 1.0f - fastExp (-1.0f / (k * fs));
        envRel = fastExp (-1.0f / (0.25f * fs));
    }

    // Una muestra de la realimentación: un pasa banda en el armónico elegido toma la cuerda y la vuelve a meter, con un
    // control de nivel que lleva la envolvente a un objetivo (sustain infinito sin desbocarse) y una entrada lenta.
    inline float feedbackSample()
    {
        if (! gate) { fbRamp = std::max (0.0f, fbRamp - 1.0f / (0.05f * fs)); if (fbRamp <= 0.0f) return 0.0f; }
        else fbRamp = std::min (1.0f, fbRamp + 1.0f / (1.2f * fs));
        if (fbCtl-- <= 0) { fbCtl = 64; fbBp.set (std::min (curHz * (float) fbHarm, 0.4f * fs), 14.0f, fs); }
        const float b = fbBp.bandpass (out[0]);
        const float ab = std::fabs (b);
        fbEnv = ab > fbEnv ? ab : fbEnv * 0.9995f;
        const float target = 0.004f + 0.02f * fbAmount;
        const float gain = fbAmount * fbRamp * clampf (1.0f - fbEnv / target, -0.5f, 1.0f) * 0.02f;
        return b * gain;
    }
    int fbCtl = 0;

    // ── Excitación ──
    // Pulso de Hann del ancho del contacto, pasa bajos por la dureza, ruido de la púa contra el entorchado, peine de la
    // posición de ataque y peine de armónicos; queda en `exc` para meterse en el lazo muestra a muestra.
    void buildExcitation (const PluckSpec& s, bool add = false)
    {
        const float N = loopSamples();
        const float vel01 = clampf (s.velocity, 0.0f, 1.0f);
        // Contacto: la púa se suelta en decenas de µs (casi un escalón de fuerza); el dedo, en unos cientos.
        float widthMs = s.finger ? lerp (0.45f, 0.12f, s.hardness) : lerp (0.30f, 0.025f, s.hardness);
        widthMs *= 1.15f - 0.3f * vel01;
        if (s.dead) widthMs *= 1.6f;
        const int W = std::max (2, (int) (widthMs * 1e-3f * fs));
        // Pasa bajos de la dureza y la fuerza: una púa dura tocada fuerte es brillante.
        const float bright = clampf ((s.finger ? 0.35f : 0.45f) * s.hardness + 0.45f * vel01 + (s.finger ? 0.08f : 0.15f), 0.0f, 1.0f) * (1.0f - 0.6f * s.mute);
        Lp1 lp; lp.set (expMap (bright, 1200.0f, 18000.0f), fs);
        const float D = posNearFret ? N * 0.94f : N * clampf (s.posMm / std::max (60.0f, phys.lengthMm * std::exp2 (-s.fret / 12.0f)), 0.02f, 0.5f);
        const int len = std::min (kExcMax - 1, W + (int) D + 2 + (s.harmonic >= 2 ? (int) N + 2 : 0) + (int) (0.004f * fs));
        // Pulso + ruido.
        Noise nz; nz.seed ((uint32_t) (mix64 (s.seed) | 1u));
        const float noiseAmt = s.noise * (phys.wound ? 1.0f : 0.45f) * (0.4f + 0.6f * vel01) * (s.finger ? 0.3f : 1.0f) + (s.dead ? 0.6f : 0.0f);
        const float nTau = 1.0f / ((s.dead ? 0.004f : 0.0012f) * fs);
        float nEnv = 1.0f, prevN = 0.0f;
        for (int i = 0; i < len; ++i)
        {
            float p = 0.0f;
            if (i < W) p = 0.5f - 0.5f * cos2pi ((float) i / (float) W);
            p *= 2.0f / (float) W;                          // área 1: el nivel no depende del ancho
            const float n = nz.next();
            const float scrape = (n - prevN) * 0.5f; prevN = n;  // ruido diferenciado: raspado agudo
            p += noiseAmt * scrape * nEnv * 0.08f;
            nEnv *= (1.0f - nTau);
            work[(size_t) i] = lp.process (p);
        }
        // Peine de la posición de ataque: e[n] = p[n] − p[n − D].
        for (int i = len - 1; i >= 0; --i)
        {
            const float x = (float) i - D;
            float dl = 0.0f;
            if (x >= 0.0f) { const int j = (int) x; const float t = x - (float) j; dl = work[(size_t) j] + ((j + 1 < len ? work[(size_t) j + 1] : 0.0f) - work[(size_t) j]) * t; }
            work[(size_t) i] -= dl;
        }
        // Armónicos: el promedio de k copias corridas N/k deja sólo los parciales múltiplos de k.
        if (s.harmonic >= 2)
        {
            const int k = s.harmonic;
            std::vector<float>& src = work;
            std::vector<float>& tmp = work2;
            for (int i = 0; i < len; ++i)
            {
                float acc = 0.0f;
                for (int j = 0; j < k; ++j)
                {
                    const float x = (float) i - (float) j * N / (float) k;
                    if (x < 0.0f) break;
                    const int a = (int) x; const float t = x - (float) a;
                    acc += src[(size_t) a] + ((a + 1 < len ? src[(size_t) a + 1] : 0.0f) - src[(size_t) a]) * t;
                }
                tmp[(size_t) i] = acc / (float) k * s.harmonicMix * 1.8f + src[(size_t) i] * (1.0f - s.harmonicMix);
            }
            std::copy (tmp.begin(), tmp.begin() + len, work.begin());
        }
        // Nivel por velocidad (curva de ~40 dB) y reparto entre polarizaciones por el ángulo. El pulso tiene área 1 en
        // muestras: para que la misma púa dé la misma velocidad de cuerda a cualquier frecuencia de muestreo, su altura
        // escala con fs (la integración de la pastilla multiplica por 1/fs).
        const float amp = 0.55f * dbToGain (-34.0f * (1.0f - vel01) * (1.0f - vel01) - 6.0f * (1.0f - vel01)) * (fs / 48000.0f);
        const float ang = clampf (s.finger ? std::max (s.angle, 0.55f) : s.angle, 0.0f, 1.0f);
        const float th = ang * 0.5f * kPi;
        wV = std::sin (th) * 0.85f + 0.15f; wH = std::cos (th) * (float) s.dir;
        // La excitación pendiente se suma (un ligado sobre una púa en curso) o reemplaza a la anterior.
        if (add && excPos < excLen)
        {
            const int rem = excLen - excPos;
            for (int i = 0; i < len; ++i) work[(size_t) i] = work[(size_t) i] * amp + (i < rem ? exc[(size_t) (excPos + i)] : 0.0f);
            const int nl = std::max (len, rem);
            for (int i = len; i < nl; ++i) work[(size_t) i] = exc[(size_t) (excPos + i)];
            std::copy (work.begin(), work.begin() + nl, exc.begin());
            excLen = nl;
        }
        else
        {
            for (int i = 0; i < len; ++i) exc[(size_t) i] = work[(size_t) i] * amp;
            excLen = len;
        }
        excPos = 0;
    }
};

} // namespace dh::gt
