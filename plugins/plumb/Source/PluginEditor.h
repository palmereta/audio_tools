#pragma once
// Editor de Plumb: una pizarra de piedra con marco de bronce (render de tools/blender/plumb.py). A la izquierda cuelga
// la plomada, que oscila con el error de afinación de lo que se canta y se queda quieta cuando la nota está justa;
// en la pizarra, la vista de notas en vivo: lo cantado en tiza, lo corregido en bronce, las notas de la escala como
// renglones. Abajo, lo de todos los días; la página avanzada se abre sobre la pizarra.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ManualView.h"

class PlumbEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PlumbEditor (PlumbProcessor&);
    ~PlumbEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int baseW = 1200, baseH = 768;

private:
    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };
    class Bob; class NoteView; class Advanced;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using BA = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    PlumbProcessor& proc;
    struct Look; std::unique_ptr<Look> lnf;
    Content content;
    std::unique_ptr<Bob> bob;
    std::unique_ptr<NoteView> notes;
    std::unique_ptr<Advanced> advanced;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn { "Init" }, manualBtn { "Manual" }, advBtn { "Advanced" }, listenBtn { "Listen" }, applyBtn { "Apply" }, compareBtn { "Compare" };
    juce::OwnedArray<juce::TextButton> modeBtns, keyBtns;
    juce::ComboBox scaleBox, voiceBox;
    std::unique_ptr<CA> scaleAtt, voiceAtt;
    juce::Slider amount, speed, style;
    std::unique_ptr<SA> amountAtt, speedAtt, styleAtt;
    std::unique_ptr<juce::Component> manual;
    juce::TooltipWindow tips { this, 700 };
    juce::String suggestion;
    int shownLatency = -1; uint32_t keySignature = 0xffffffffu;

    void showPresetMenu();
    void showManual (bool on);
    void paintPanel (juce::Graphics& g);
    void timerCallback() override;
    void refreshKeys();
    juce::String valueText (const juce::String& id) const;
};
