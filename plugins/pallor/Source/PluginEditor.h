#pragma once
// Editor de Pallor: el instrumento renderizado (tools/blender/pallor.py) con todo lo que se mueve dibujado encima, y la
// pantalla grande con sus páginas (PATTERN, VOICE, MIXER, FX, SONG, SAMPLES, PRESETS, SETUP). Ver ui/*.h.
// Todo vive en un componente de 1440 × 940 que se escala entero (relación fija) al tamaño de la ventana.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ManualView.h"
#include "ui/Ctx.h"
#include "ui/ThemePl.h"
#include "ui/Hardware.h"
#include "ui/PatternPage.h"
#include "ui/Pages.h"

class PallorEditor : public juce::AudioProcessorEditor, public juce::DragAndDropContainer, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit PallorEditor (PallorProcessor&);
    ~PallorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray& f) override { return f.size() == 1; }
    void filesDropped (const juce::StringArray& f, int x, int y) override;

    static constexpr int baseW = 1440, baseH = 940;

private:
    class Screen;
    PallorProcessor& proc;
    dh::pal::ui::Look look;
    dh::pal::ui::Ctx ctx;
    juce::Component content;
    dh::pal::ui::StaticLayer staticLayer;
    dh::pal::ui::Overlay overlay;
    std::unique_ptr<Screen> screen;
    std::unique_ptr<juce::Component> manual;
    juce::TooltipWindow tips { this, 800 };
    std::array<uint32_t, dh::pal::kNumSlots> lastTrig {};

    void timerCallback() override;
    void showManual (bool on);
    void loadSample (int voice, const juce::File& f);
};
