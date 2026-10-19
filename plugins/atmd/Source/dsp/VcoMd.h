#pragma once
// VCO del Model D. Un núcleo de rampa por oscilador; de él salen las seis formas del selector:
//   osc 1 y 2: triángulo, "diente de tiburón" (82 % triángulo + 18 % sierra), sierra, cuadrada, rectangular ancha y angosta;
//   osc 3:     igual, pero la segunda forma es la sierra invertida (bajando), útil como LFO.
// Las formas con saltos se limitan en banda con PolyBLEP y las que tienen quiebres con PolyBLAMP, sobre la
// misma fase. Los niveles relativos siguen al VCO real: la cuadrada y los pulsos suenan más fuerte que el
// triángulo con la perilla del mezclador en el mismo número.

#include "Blep.h"
#include "Common.h"

namespace dh::md {

enum Wave { Triangle = 0, SharkTooth, Saw, Square, WidePulse, NarrowPulse, WaveCount };

// Ancho de los rectangulares (fracción del período) y nivel relativo de cada forma.
constexpr float kWideWidth = 0.29f, kNarrowWidth = 0.15f;      // 50 / 29 / 15 % [spec §5]
constexpr float kWaveGain[WaveCount] = { 0.85f, 0.90f, 1.0f, 1.0f, 0.95f, 0.85f };

class Vco
{
public:
    void setFreq (float hz, float fs) { dt = clampf (hz / fs, 1.0e-7f, 0.45f); }
    void seed (uint32_t s, float startPhase) { rng = Rng (s); phase = startPhase; }
    void resetPhase (float ph = 0.0f) { phase = ph; }
    float phaseNow() const { return phase; }

    // wave: Wave. reverseSaw: la segunda forma es la sierra invertida (osc 3). Devuelve −1..1 aprox.
    inline float process (int wave, bool reverseSaw)
    {
        float y = 0.0f;
        switch (wave)
        {
            case Triangle:   y = tri(); break;
            case SharkTooth: y = reverseSaw ? -saw() : 0.82f * tri() + 0.18f * saw(); break;    // resistencias del selector [spec §5]
            case Saw:        y = saw(); break;
            case Square:     y = pulse (0.5f); break;
            case WidePulse:  y = pulse (kWideWidth); break;
            default:         y = pulse (kNarrowWidth); break;
        }
        phase += dt;
        if (phase >= 1.0f) phase -= std::floor (phase);
        return y * kWaveGain[std::clamp (wave, 0, (int) WaveCount - 1)];
    }

    // Valor sin límite de banda (para usar el osc 3 como modulador en LO): la forma exacta, no la de audio.
    inline float shapeRaw (int wave, bool reverseSaw) const
    {
        const float p = phase;
        switch (wave)
        {
            case Triangle:   return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
            case SharkTooth: return reverseSaw ? 1.0f - 2.0f * p : 0.82f * (p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p) + 0.18f * (2.0f * p - 1.0f);
            case Saw:        return 2.0f * p - 1.0f;
            case Square:     return p < 0.5f ? 1.0f : -1.0f;
            case WidePulse:  return p < kWideWidth ? 1.0f : -1.0f;
            default:         return p < kNarrowWidth ? 1.0f : -1.0f;
        }
    }

private:
    float phase = 0.0f, dt = 0.01f;
    Rng rng;

    inline float saw() const { return 2.0f * phase - 1.0f - polyBlep (phase, dt); }

    inline float tri() const
    {
        // Quiebres en 0 y 0.5: cambia la pendiente en ±8 por período → PolyBLAMP con ese salto.
        float y = phase < 0.5f ? 4.0f * phase - 1.0f : 3.0f - 4.0f * phase;
        y += 4.0f * dt * polyBlamp (phase, dt);
        float t = phase - 0.5f; if (t < 0.0f) t += 1.0f;
        y -= 4.0f * dt * polyBlamp (t, dt);
        return y;
    }

    inline float pulse (float w) const
    {
        w = clampf (w, 2.0f * dt, 1.0f - 2.0f * dt);
        float y = phase < w ? 1.0f : -1.0f;
        y += polyBlep (phase, dt);
        float t = phase - w; if (t < 0.0f) t += 1.0f;
        y -= polyBlep (t, dt);
        return y - (2.0f * w - 1.0f);          // acople AC: el pulso angosto no lleva continua
    }
};

// Ruido blanco y rosa (filtro de Paul Kellet, −3 dB/oct). Rosa más bajo de nivel en agudos, como en el panel.
class Noise
{
public:
    void seed (uint32_t s) { rng = Rng (s); }
    inline float white() { return rng.bipolar(); }
    inline float pink()
    {
        const float w = rng.bipolar() * 0.5f;
        b0 = 0.99886f * b0 + w * 0.0555179f; b1 = 0.99332f * b1 + w * 0.0750759f; b2 = 0.96900f * b2 + w * 0.1538520f;
        b3 = 0.86650f * b3 + w * 0.3104856f; b4 = 0.55000f * b4 + w * 0.5329522f; b5 = -0.7616f * b5 - w * 0.0168980f;
        const float y = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
        b6 = w * 0.115926f;
        return y * 0.22f;
    }
    // Rojo: el rosa por un pasa bajos de 100 Hz (la fuente de modulación con el interruptor en PINK).
    inline float red (float pinkSample, float coef) { r += (pinkSample - r) * coef; return r * 4.0f; }
private:
    Rng rng;
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0, r = 0;
};

} // namespace dh::md
