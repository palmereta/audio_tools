#pragma once
// Editor del AT330, dirección "Studio 1979" (tools/blender/at330.py). La ventana es de 1600 x 960 (escalable con la
// proporción fija): arriba la barra de presets y páginas; en el medio el instrumento renderizado; abajo una barra de
// estado. El render (assets/ui) trae el panel con las llaves apagadas, el mismo con todas prendidas, las teclas
// apretadas y las tapas sueltas; la interfaz arma encima lo que cambia y dibuja la serigrafía como texto vectorial.
// Los controles son sliders y botones de JUCE invisibles sobre cada pieza (automatización, doble clic al valor por
// defecto, gestos del host); la pieza se pinta con el valor del parámetro.
// Páginas: Panel (el instrumento), Extended (los extras que el original no tenía) y Manual.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "../../../shared/ui/Showing.h"

class At330Editor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit At330Editor (At330Processor&);
    ~At330Editor() override;
    void paint (juce::Graphics&) override {}
    void resized() override;

    static constexpr int baseW = 1600, baseH = 960;

private:
    class Content; class Keys; class Extended; struct Lnf;
    void timerCallback() override;
    void showPage (int page);
    void showPresetMenu();

    At330Processor& proc;
    std::unique_ptr<Lnf> lnf;
    std::unique_ptr<Content> content;
    std::unique_ptr<juce::Component> manual;
    int page = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (At330Editor)
};
