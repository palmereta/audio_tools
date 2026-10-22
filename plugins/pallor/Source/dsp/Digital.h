#pragma once
// El motor digital de Pallor: una muestra tocada por la cadena de las cajas de 1980-90 (docs/research.md §2.2, §7):
//   - "8-bit": códigos de 8 bits con ley μ segmentada (el convertidor companding), y la altura cambiando el reloj de
//     lectura: cada tic del reloj lee la muestra siguiente, sin interpolación. Subir la altura sube el reloj, así que
//     las imágenes del convertidor suben con ella y el filtro de salida (fijo, por canal) deja pasar más.
//   - "12-bit 26k": reloj fijo de 26,04 kHz, 12 bits lineales y la altura por salto de muestras (se toma la muestra
//     más cercana hacia atrás: el "drop-sample" de los muestreadores de 1987), sin filtro de salida.
//   - "12-bit 40k": reloj fijo de 40 kHz, 12 bits, salto de muestras y un filtro de salida a 15 kHz.
//   - "Clean": interpolación cúbica a la frecuencia del host, sin cuantizar (para muestras propias en alta fidelidad).
// La salida del convertidor es una escalera (retención de orden cero). Se dibuja en la frecuencia del host con
// escalones de banda limitada (PolyBLEP de dos muestras) en el instante exacto de cada tic: las imágenes que
// produce el convertidor quedan (son el sonido de estas máquinas), pero la grilla del host no agrega aliasing ni el
// temblor de ±1 muestra en cada escalón. La corrección necesita ver el escalón siguiente: la voz atrasa una muestra
// en todos los modos (la latencia no cambia con el modo).
// Después: el filtro de salida del modo (research §2.2: 8 bits, cuatro polos a 10 kHz en bombo y toms y a 16 kHz en el
// resto, y en bombo y toms el pasa bajos dinámico que se abre en cada golpe y se cierra a 1,4 kHz / 650 Hz; 12 bits
// a 40 kHz, cuatro polos a 16 kHz; 12 bits a 26 kHz, nada: la salida cruda del convertidor), el tono (pasa bajos por
// debajo del centro, pasa altos por encima) y el VCA de decaimiento por canal, después del convertidor (el VCA es su
// referencia: el ruido de cuantización baja con el sonido).
// El reloj de 8 bits corre libre: cada golpe cae en otra fase del reloj (un poco distinto cada vez, como en el aparato).
// La voz corre al doble de la frecuencia del host (Voice.h la baja con el diezmador de media banda): las imágenes del
// convertidor hasta 1,5 veces el reloj caen dentro de la banda representable y el diezmador saca lo que doblaría.

#include "Analog.h"
#include <memory>
#include <string>

namespace dh::pal {

enum Chain { kChainKit = 0, kChain8 = 1, kChain26 = 2, kChain40 = 3, kChainClean = 4 };

struct SampleData
{
    std::vector<float> data;     // mono, ±1
    double rate = 30000.0;       // frecuencia propia (la del reloj con tune = 0)
    int chain = kChain8;         // el modo con el que vino la muestra (el de su kit)
    std::string name;
};

class DigitalPlayer
{
public:
    void prepare (float sampleRate, int slotId = 0) { fs = sampleRate; slot = slotId; active = false; }
    bool isActive() const { return active; }
    const SampleData* sample() const { return smp; }
    void stop() { active = false; smp = nullptr; }

    // Shape: tune (semitonos), decay (≥ 0.98 sin VCA), tone (0.5 abierto), a = arranque (debajo de 0.5 un ataque
    // suave, encima un punto de inicio más adelante), b = envolvente de altura (0.5 nada, ±12 semitonos).
    void trigger (const SampleData* s, int chainMode, const Shape& sh, bool reverse, float vel, float acc, uint32_t seed = 0)
    {
        smp = s;
        if (smp == nullptr || smp->data.empty()) { active = false; return; }
        mode = chainMode == kChainKit ? smp->chain : chainMode;
        rev = reverse; amp = vel * (1.0f + 0.25f * acc);
        const int n = (int) smp->data.size();
        const float startU = clampf ((sh.a - 0.5f) * 2.0f, 0.0f, 1.0f) * 0.6f;
        attackN = sh.a < 0.5f ? (0.5f - sh.a) * 2.0f * 0.04f * fs : 0.0f;
        const double start = (double) startU * n;
        pos = rev ? (double) (n - 1) - start : start;
        dir = rev ? -1.0 : 1.0;
        baseRatio = (double) semis (sh.tune);
        pitchEnv = (sh.b - 0.5f) * 24.0f;
        penv = 1.0f; penvC = tauCoef (0.06f, fs);
        // VCA: decay ≥ 0.98 deja sonar la muestra entera.
        useVca = sh.decay < 0.98f;
        vca = 1.0f; vcaC = useVca ? t60Coef (expMap (sh.decay / 0.98f, 0.04f, 8.0f), fs) : 1.0f;
        // Tono: pasa bajos debajo de 0.5, pasa altos encima.
        toneMode = sh.tone < 0.49f ? 1 : (sh.tone > 0.51f ? 2 : 0);
        if (toneMode == 1) tone.set (expMap (sh.tone / 0.49f, 250.0f, 20000.0f), 0.707f, fs);
        if (toneMode == 2) tone.set (expMap ((sh.tone - 0.51f) / 0.49f, 20.0f, 1500.0f), 0.707f, fs);
        // Filtro de salida del modo (fijo: no sigue a la altura, como en el aparato).
        const bool lowVoice = slot == kKick || (slot >= kTom1 && slot <= kTom4);
        reconOn = mode == kChain8 || mode == kChain40;
        const float rc = mode == kChain8 ? (lowVoice ? 10000.0f : 16000.0f) : 16000.0f;
        recon.set (std::min (rc, 0.45f * fs), 0.541f, fs); recon2.set (std::min (rc, 0.45f * fs), 1.307f, fs);
        dynOn = mode == kChain8 && lowVoice;
        dynEnd = slot == kKick ? 1400.0f : 650.0f; dynTop = std::min (20000.0f, 0.45f * fs) - dynEnd; dynE = 1.0f; dynC = tauCoef (0.05f, fs);
        tone.reset(); recon.reset(); recon2.reset(); dyn.reset();
        // Reloj. En 8 bits la muestra es su propio reloj; en los muestreadores el reloj es fijo.
        clockHz = mode == kChain26 ? 26040.0 : (mode == kChain40 ? 40000.0 : smp->rate);
        // El primer tic cae en algún lugar de la primera muestra (el reloj libre no espera al golpe).
        clockPh = 1.0 - (mode == kChain8 ? hashUni (seed, (uint64_t) slot, 17) * 0.999 * std::min (1.0, smp->rate / fs) : 0.0);
        level = 0.0f; pending = 0.0f; ending = 0; t = 0;
        active = true;
    }

    inline float process()
    {
        if (! active) return 0.0f;
        // Altura del momento: la de la perilla y la envolvente.
        double ratio = baseRatio;
        if (pitchEnv != 0.0f) { ratio *= (double) semis (pitchEnv * penv); penv *= penvC; }
        float out;
        if (mode == kChainClean)
        {
            const float v = ending ? 0.0f : readCubic();
            pos += dir * smp->rate * ratio / fs;
            checkEnd();
            out = pending; pending = v;
        }
        else
        {
            const double inc = mode == kChain8 ? smp->rate * ratio / fs : clockHz / fs;
            clockPh += inc;
            float corrPrev = 0.0f, corrCur = 0.0f;
            int guard = 0;
            while (clockPh >= 1.0 && guard++ < 16)
            {
                clockPh -= 1.0;
                const float since = (float) (clockPh / inc);                        // muestras desde el tic, [0, 1)
                const float nl = ending ? 0.0f : fetch (ratio);
                const float J = nl - level; level = nl;
                corrPrev += 0.5f * J * since * since;
                corrCur -= 0.5f * J * (1.0f - since) * (1.0f - since);
            }
            out = pending + corrPrev; pending = level + corrCur;
        }
        if (reconOn) out = recon2.lowpass (recon.lowpass (out));
        if (dynOn) { dyn.set (dynEnd + dynTop * dynE, 0.707f, fs); dynE *= dynC; out = dyn.lowpass (out); }
        if (toneMode == 1) out = tone.lowpass (out); else if (toneMode == 2) out = tone.highpass (out);
        float g = amp;
        if (useVca) { g *= vca; vca *= vcaC; }
        if (attackN > 0.0f && (float) t < attackN) g *= (float) t / attackN;
        ++t;
        if (ending) { if (++ending > (int) (0.02f * fs)) active = false; }
        else if (useVca && vca < 3e-5f) active = false;
        return out * g;
    }

private:
    float fs = 48000;
    const SampleData* smp = nullptr;
    int mode = kChain8; bool rev = false, active = false, useVca = false, reconOn = false;
    double pos = 0, dir = 1, baseRatio = 1, clockHz = 30000, clockPh = 0;
    float amp = 1, pitchEnv = 0, penv = 0, penvC = 0, vca = 1, vcaC = 1, attackN = 0;
    float level = 0, pending = 0;
    int toneMode = 0, ending = 0, t = 0, slot = 0;
    Svf tone, recon, recon2, dyn;
    bool dynOn = false; float dynEnd = 1400, dynTop = 18000, dynE = 1, dynC = 0.999f;

    inline void checkEnd() { if (pos < 0.0 || pos >= (double) smp->data.size() - 1.0) ending = std::max (ending, 1); }

    // Un tic del reloj: la muestra que toca y su cuantización.
    inline float fetch (double ratio)
    {
        const int n = (int) smp->data.size();
        const int i = (int) std::floor (pos);
        if (i < 0 || i >= n) { ending = std::max (ending, 1); return 0.0f; }
        const float x = smp->data[(size_t) i];
        if (mode == kChain8) { pos += dir; return dac76::quantize (x); }
        pos += dir * smp->rate * ratio / clockHz;                                      // salto de muestras
        return quantLinear (x, 2047.0f);
    }
    inline float readCubic() const
    {
        const int n = (int) smp->data.size();
        const int i = (int) std::floor (pos); const float f = (float) (pos - i);
        auto at = [&] (int k) { return k < 0 || k >= n ? 0.0f : smp->data[(size_t) k]; };     // fuera de la muestra, silencio
        const float y0 = at (i - 1), y1 = at (i), y2 = at (i + 1), y3 = at (i + 2);
        const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * f + c2) * f + c1) * f + y1;
    }
};

} // namespace dh::pal
