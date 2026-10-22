#pragma once
// Editor de Thrum (estilo "Stage"): barra de arriba con los presets, las páginas, Inspect y el manual; el diapasón en
// vivo a todo el ancho; y la página elegida (Guitar, Play, Pedals, Amp / Cab, Settings). Todo se dibuja en un lienzo
// lógico de 1360 × 850 que se escala con la ventana (renders a 2x). Los controles van atados a los parámetros por id.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/Pages.h"
#include "ManualView.h"

class ThrumEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ThrumEditor (ThrumProcessor&);
    ~ThrumEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int baseW = 1360, baseH = 850;
    void showPage (int i);
    void showManual (bool on);

private:
    ThrumProcessor& proc;
    thrumui::Look look;
    juce::TooltipWindow tips { this, 600 };
    struct Content;
    std::unique_ptr<Content> content;
    std::unique_ptr<juce::Component> manual;
    void timerCallback() override;
    // Notas de demostración para las capturas (THRUM_DEMO): se tocan por el teclado del procesador.
    juce::Array<int> demoNotes; double demoPeriod = 0; double demoStart = 0; bool demoDown = false;
};
