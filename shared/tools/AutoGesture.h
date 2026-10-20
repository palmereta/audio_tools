#pragma once
// Gestos de automatización para los cambios que hace la interfaz: el gesto empieza con el primer cambio de un
// parámetro y termina 300 ms después del último (o al pasar a otro parámetro). Sin gestos, Logic, Cubase o Pro Tools
// en modo Touch/Latch no graban lo que se mueve en el panel, o lo devuelven a la automatización existente.
// Sólo actúa en el hilo de mensajes (un cambio del host o del MIDI no abre gestos).

#include <JuceHeader.h>

namespace dh {

class AutoGesture : private juce::Timer
{
public:
    ~AutoGesture() override { end(); }
    void touch (juce::AudioProcessorParameter* p)
    {
        if (p == nullptr || ! juce::MessageManager::existsAndIsCurrentThread()) return;
        if (p != current) { end(); current = p; p->beginChangeGesture(); }
        startTimer (300);
    }
    void end()
    {
        stopTimer();
        if (current != nullptr) { current->endChangeGesture(); current = nullptr; }
    }

private:
    void timerCallback() override { end(); }
    juce::AudioProcessorParameter* current = nullptr;
};

} // namespace dh
