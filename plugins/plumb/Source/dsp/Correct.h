#pragma once
// Lógica de corrección de Plumb (docs/research.md §4). Corre a la tasa del detector (~1,5 kHz) y entrega, para el
// instante de cada estimación, la corrección Δ en semitonos que el empalmador aplica al audio de ese instante.
//
// La altura detectada p se separa en centro c (constante por nota), deriva lenta (s − c, por debajo de ~0,6 Hz) y
// vibrato (p − s, lo que pasa de 2,5 Hz). La salida deseada es objetivo + keepDrift·deriva + keepVibrato·vibrato,
// así la corrección mueve el centro percibido y deja la vida de la voz (lo que hace un editor de notas, pero en
// vivo). En modo directo (Style al máximo) la salida es el objetivo exacto: el efecto de afinación dura.

#include "Detector.h"
#include <array>
#include <cmath>
#include <cstdint>

namespace dh::pl {

struct CorrectParams
{
    uint16_t allowed = 0x0FFF;      // clases de altura que son objetivo (bit 0 = C)
    uint16_t bypass = 0;            // clases sin corrección (notas de paso, blue notes)
    float amount = 1.0f;            // 0..1
    float speedMs = 20.0f;          // tiempo al 95 % de un salto de corrección
    float transitionMs = 0.0f;      // cómo se pasa de un objetivo al siguiente
    float humanize = 0.0f;          // 0..1: las notas largas se corrigen más despacio
    float flexIn = 600.0f, flexOut = 600.0f;   // cents: ventana de corrección alrededor del objetivo
    float hysteresis = 0.15f;       // semitonos más allá del punto medio para cambiar de objetivo
    float keepVibrato = 1.0f, keepDrift = 0.3f;
    bool direct = false;            // afinación dura: la salida es el objetivo, sin centro
    float transpose = 0.0f;         // semitonos (con cents)
    float vibRate = 5.5f, vibDepth = 0.0f, vibDelayMs = 200.0f;   // vibrato sintético (cents de profundidad)
    int midiMode = 0;               // 0 apagado, 1 escala con las notas apretadas, 2 nota fija, 3 automático
    float bendRange = 2.0f;
};

// Suavizado de segundo orden críticamente amortiguado (dos polos iguales): sin sobrepaso.
struct CritSmoother
{
    double y1 = 0, y2 = 0;
    void reset (double v) { y1 = y2 = v; }
    inline double process (double x, double a) { y1 = x + a * (y1 - x); y2 = y1 + a * (y2 - y1); return y2; }
    // Coeficiente para que un escalón llegue al 95 % en `t95` segundos a la tasa `rate` (t95 ≈ 4,74 τ).
    static double coef (double t95, double rate) { return t95 <= 1e-6 ? 0.0 : std::exp (-1.0 / (rate * t95 / 4.74)); }
};

// Tonalidad automática: histograma de clases de altura con olvido, contra los perfiles de Krumhansl y Kessler.
struct KeyFinder
{
    std::array<double, 12> hist {};
    void reset() { hist.fill (0); }
    void add (int pc, double w, double forget) { for (auto& h : hist) h *= forget; hist[(size_t) ((pc % 12 + 12) % 12)] += w; }
    // Devuelve la mejor tonalidad: tónica 0..11, menor, y la confianza (diferencia de correlación con la segunda).
    void best (int& tonic, bool& minor, float& conf) const
    {
        static const double maj[12] = { 6.35, 2.23, 3.48, 2.33, 4.38, 4.09, 2.52, 5.19, 2.39, 3.66, 2.29, 2.88 };
        static const double mnr[12] = { 6.33, 2.68, 3.52, 5.38, 2.60, 3.53, 2.54, 4.75, 3.98, 2.69, 3.34, 3.17 };
        double total = 0; for (double h : hist) total += h;
        tonic = 0; minor = false; conf = 0;
        if (total < 1e-6) return;
        double b1 = -2, b2 = -2;
        for (int m = 0; m < 2; ++m)
            for (int t = 0; t < 12; ++t)
            {
                const double* prof = m ? mnr : maj;
                double mx = 0, my = 0; for (int i = 0; i < 12; ++i) { mx += hist[(size_t) i]; my += prof[(i - t + 12) % 12]; }
                mx /= 12; my /= 12;
                double sxy = 0, sxx = 0, syy = 0;
                for (int i = 0; i < 12; ++i) { const double x = hist[(size_t) i] - mx, y = prof[(i - t + 12) % 12] - my; sxy += x * y; sxx += x * x; syy += y * y; }
                const double r = sxy / std::sqrt (std::max (1e-12, sxx * syy));
                if (r > b1) { b2 = b1; b1 = r; tonic = t; minor = m == 1; } else if (r > b2) b2 = r;
            }
        conf = (float) std::max (0.0, b1 - b2);
    }
};

// Lo que el corrector decide en cada estimación (también lo que dibuja la vista de notas).
struct CorrectOut
{
    double time = 0;
    float hz = 0, pitch = 0, target = 0, output = 0, delta = 0, centre = 0;
    bool voiced = false;
};

class Corrector
{
public:
    void prepare (double hopRate) { rate = hopRate; reset(); }
    void setRate (double hopRate) { rate = hopRate; }
    void reset()
    {
        voicedPrev = false; age = 0; target = -1; awayFor = 0; deltaS.reset (0); targetS.reset (0); sLP.reset (0); cLP.reset (0);
        heldCount = 0; chordMask = 0; bend = 0; unvoicedFor = 1; vibPhase = 0; keys.reset();
    }
    CorrectParams P;
    KeyFinder keys;

    void noteOn (int note) { for (int i = 0; i < heldCount; ++i) if (held[(size_t) i] == note) return; if (heldCount < 16) held[(size_t) heldCount++] = note; chordMask = 0; for (int i = 0; i < heldCount; ++i) chordMask |= (uint16_t) (1u << (held[(size_t) i] % 12)); }
    void noteOff (int note) { for (int i = 0; i < heldCount; ++i) if (held[(size_t) i] == note) { for (int j = i; j < heldCount - 1; ++j) held[(size_t) j] = held[(size_t) j + 1]; --heldCount; break; } }
    void pitchBend (float v) { bend = v; }    // −1..1

    CorrectOut update (const PitchEstimate& e)
    {
        CorrectOut o; o.time = e.time; o.hz = e.hz; o.voiced = e.voiced;
        const double dt = 1.0 / rate;
        if (! e.voiced)
        {
            // Sin voz: la corrección vuelve a cero rápido (20 ms) y se olvida la nota.
            unvoicedFor += dt; voicedPrev = false;
            const double d = deltaS.process (P.transpose, CritSmoother::coef (0.02, rate));
            o.delta = (float) d; o.pitch = 0; o.target = 0; o.output = 0;
            return o;
        }
        const double p = e.midi;
        if (! voicedPrev)
        {
            sLP.reset (p); cLP.reset (p); age = 0; awayFor = 0; target = -1;
            if (unvoicedFor > 0.05 || std::abs (deltaS.y2) > 24) deltaS.reset (deltaS.y2);
        }
        voicedPrev = true; unvoicedFor = 0; age += dt;
        // Centro, deriva y vibrato.
        // Al arrancar la nota los suavizados son rápidos y se van haciendo lentos: así el centro no se queda en el fondo
        // de la entrada desde abajo (un scoop de casi un semitono) mientras la nota ya está en su altura.
        // En los primeros 120 ms el centro es la altura suavizada misma; después se va haciendo lento.
        double s = sLP.process (p, CritSmoother::coef (std::min (0.30, 0.05 + 0.5 * age), rate));   // ~2,5 Hz: quita el vibrato
        double c = age < 0.12 ? (cLP.reset (s), s)
                              : cLP.process (s, CritSmoother::coef (std::min (1.20, 0.2 + 2.0 * (age - 0.12)), rate));   // ~0,6 Hz: el centro
        // Cambio de nota en legato: lejos del centro por más de 30 ms → la nota nueva arranca acá.
        if (std::abs (p - c) > 0.75) awayFor += dt; else awayFor = 0;
        // Lejos del centro (más de 2,5 semitonos: otra nota o un error de octava en el arranque) la nota nueva
        // empieza ya; si no, conservar la "deriva" respecto de un centro equivocado dispararía la corrección. Los
        // scoops naturales (menos de un semitono) esperan los 30 ms.
        if (awayFor > 0.03 || std::abs (p - c) > 2.5) { sLP.reset (p); cLP.reset (p); c = s = p; age = 0; awayFor = 0; }
        const double sNow = s;
        // Objetivo.
        uint16_t allowed = P.allowed ? P.allowed : 0x0FFF;
        const bool midiHeld = heldCount > 0 && P.midiMode != 0;
        // Escala desde las notas apretadas: el acorde queda hasta que se aprieta otro. Auto: sólo mientras se apretan.
        if ((P.midiMode == 1 && chordMask) || (midiHeld && P.midiMode == 3 && chordMask)) allowed = chordMask;
        // En afinación dura la nota se elige sobre la altura sin vibrato (sNow): un vibrato cerca del punto medio entre
        // dos notas no salta de una a otra. Con histéresis 0 se elige sobre la altura cruda: el efecto "warble".
        const double ref = P.direct ? (P.hysteresis < 0.005f ? p : sNow) : c;
        double tgt;
        if (midiHeld && P.midiMode == 2) tgt = held[(size_t) heldCount - 1] + bend * P.bendRange;
        else
        {
            const double near = nearestAllowed (ref, allowed);
            if (target >= 0 && isAllowed ((int) std::lround (target), allowed) && std::abs (ref - target) < 0.5 + P.hysteresis) tgt = target;
            else tgt = near;
        }
        if (target < 0) targetS.reset (tgt);
        target = tgt;
        const double tS = P.transitionMs > 0.5 ? targetS.process (tgt, CritSmoother::coef (P.transitionMs * 0.001, rate)) : (targetS.reset (tgt), tgt);
        // Peso: Amount, ventana flex y clases sin corrección.
        double w = P.amount;
        const int pcNear = ((int) std::lround (ref) % 12 + 12) % 12;
        if ((P.bypass >> pcNear) & 1) w = 0;
        if (! P.direct)
        {
            const double dist = std::abs (c - tS) * 100.0;
            if (dist > P.flexIn) w *= dist >= P.flexOut ? 0.0 : 1.0 - (dist - P.flexIn) / std::max (1.0, (double) P.flexOut - P.flexIn);
        }
        // Vibrato sintético sobre el objetivo, con retardo y entrada suave.
        double synth = 0;
        if (P.vibDepth > 0.01f)
        {
            vibPhase += 2 * kPi * P.vibRate * dt; if (vibPhase > 2 * kPi) vibPhase -= 2 * kPi;
            const double fade = std::min (1.0, std::max (0.0, (age - P.vibDelayMs * 0.001) / 0.25));
            synth = P.vibDepth * 0.01 * fade * std::sin (vibPhase);
        }
        double corr;
        if (P.direct) corr = tS - p;
        else corr = (tS - c) + (P.keepDrift - 1.0) * (sNow - c) + (P.keepVibrato - 1.0) * (p - sNow);
        const double raw = w * (corr + synth) + P.transpose;
        // Velocidad, con humanize: las notas largas se corrigen más despacio.
        const double sp = P.speedMs * 0.001 * (1.0 + 4.0 * P.humanize * std::min (1.0, age / 0.3));
        const double d = sp <= 1e-4 ? (deltaS.reset (raw), raw) : deltaS.process (raw, CritSmoother::coef (sp, rate));
        // Tonalidad automática: notas estables de más de 120 ms.
        if (age > 0.12 && e.conf > 0.5) keys.add ((int) std::lround (c), dt * e.conf, std::exp (-dt / 45.0));
        o.pitch = (float) p; o.centre = (float) c; o.target = (float) tS; o.delta = (float) std::max (-36.0, std::min (36.0, d));
        o.output = (float) (p + o.delta);
        return o;
    }

    static bool isAllowed (int note, uint16_t mask) { return (mask >> ((note % 12 + 12) % 12)) & 1; }
    static double nearestAllowed (double x, uint16_t mask)
    {
        const int base = (int) std::lround (x);
        for (int k = 0; k <= 6; ++k)
        {
            const int a = base - k, b = base + k;
            const bool okA = isAllowed (a, mask), okB = isAllowed (b, mask);
            if (okA && okB) return std::abs (x - a) <= std::abs (x - b) ? a : b;
            if (okA) return a;
            if (okB) return b;
        }
        return base;
    }

private:
    double rate = 1500;
    bool voicedPrev = false; double age = 0, awayFor = 0, target = -1, unvoicedFor = 1, vibPhase = 0;
    CritSmoother deltaS, targetS, sLP, cLP;
    std::array<int, 16> held {}; int heldCount = 0; uint16_t chordMask = 0; float bend = 0;
};

} // namespace dh::pl
