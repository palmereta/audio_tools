#pragma once
// El host dejó de llamar a processBlock y ahora vuelve. REAPER no llama a un plugin en bypass (ni processBlock ni
// processBlockBypassed), así que los note-off que llegan mientras tanto se pierden y la nota vuelve colgada al sacar
// el bypass (QA en DAWs, 2026-10-22, X1). Se detecta de dos maneras:
//   - con el transporte andando, la posición salta hacia adelante más de lo que dura un bloque (en tiempo real y en
//     render offline, donde el reloj de pared no sirve);
//   - con el transporte parado y en tiempo real, pasó mucho más tiempo de reloj de lo que duran unos bloques.
// Un salto hacia atrás (loop, relocalización) no cuenta: ahí el host ya manda sus note-off.
// Uso, al principio de processBlock: if (hostGap.check (getPlayHead(), n, isNonRealtime())) engine.allNotesOff();

#include <JuceHeader.h>

namespace dh {

class HostGap
{
public:
    void prepare (double sampleRate) { sr = sampleRate > 0.0 ? sampleRate : 48000.0; reset(); }
    void reset() { expected = -1; lastWallMs = -1.0; }

    // true si hubo un hueco entre el bloque anterior y este: hay que soltar las notas antes de procesar.
    bool check (juce::AudioPlayHead* playHead, int numSamples, bool offline)
    {
        bool gap = false, playing = false;
        juce::int64 pos = -1;
        if (playHead != nullptr)
            if (auto info = playHead->getPosition())
            {
                playing = info->getIsPlaying();
                if (auto t = info->getTimeInSamples()) pos = *t;
            }

        if (playing && pos >= 0)
        {
            // Hasta un bloque de tolerancia: algunos hosts redondean la posición entre bloques.
            if (expected >= 0 && pos > expected + std::max (numSamples, 64)) gap = true;
            expected = pos + numSamples;
        }
        else expected = -1;

        const double now = juce::Time::getMillisecondCounterHiRes();
        if (! offline && ! playing && lastWallMs >= 0.0)
        {
            // Parado: un hueco de reloj de 250 ms o de 8 bloques (lo que sea mayor) es un host que dejó de llamar,
            // no un tirón de CPU.
            const double blockMs = 1000.0 * numSamples / sr;
            if (now - lastWallMs > std::max (250.0, 8.0 * blockMs)) gap = true;
        }
        lastWallMs = offline ? -1.0 : now;
        return gap;
    }

private:
    double sr = 48000.0;
    juce::int64 expected = -1;
    double lastWallMs = -1.0;
};

} // namespace dh
