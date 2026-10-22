#pragma once
// Editor de Lacquer: barra superior (LEARN, Master a file…, estado, MANUAL), el disco con el surco a la izquierda,
// los controles (estilo, intensidad, objetivo, techo) y los lectores a la derecha, y abajo el balance tonal y lo
// que hizo cada módulo.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/LookLq.h"
#include "ui/DiscView.h"
#include "ui/Controls.h"
#include "ui/Panels.h"
#include "ManualView.h"

class LacquerEditor : public juce::AudioProcessorEditor, public juce::FileDragAndDropTarget, private juce::Timer
{
public:
    explicit LacquerEditor (LacquerProcessor&);
    ~LacquerEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int, int) override;
    void fileDragEnter (const juce::StringArray&, int, int) override { dragOver = true; content.repaint(); }
    void fileDragExit (const juce::StringArray&) override { dragOver = false; content.repaint(); }

    static constexpr int baseW = 1180, baseH = 880, barH = 52;

private:
    struct Content : public juce::Component
    {
        std::function<void (juce::Graphics&)> painter;
        void paint (juce::Graphics& g) override { if (painter) painter (g); }
    };

    LacquerProcessor& proc;
    dh::lq::ui::Look lnf;
    Content content;
    dh::lq::ui::DiscView disc;
    dh::lq::ui::StylePicker stylePicker;
    dh::lq::ui::RotarySwitch targetSwitch;
    juce::Slider intensity, ceiling;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> intensityAtt, ceilingAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> compareAtt;
    dh::lq::ui::Readouts readouts;
    dh::lq::ui::BalanceView balance;
    dh::lq::ui::ModulesView modules;
    juce::TextButton learnBtn { "Learn" }, fileBtn { "Master a file" }, manualBtn { "Manual" },
                     playBtn { "Play" }, exportBtn { "Export master" }, closeBtn { "Back to live" },
                     compareBtn { "A/B with the original" };
    juce::TextButton previewBtns[4];
    std::unique_ptr<juce::Component> manual;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::TooltipWindow tips { this, 600 };
    juce::String hoverHelp;
    float spin = 0.0f;
    int hiddenTicks = 0;                      // ticks con la ventana escondida (ver timerCallback)
    bool dragOver = false;
    juce::Rectangle<int> statusR, underDiscR, turnDownR;

    void refreshPlan();
    void paintBackdrop (juce::Graphics& g);
    void timerCallback() override;
    void chooseFile();
    void chooseExport();
    void showManual (bool on);
    void syncFromParams();
};
