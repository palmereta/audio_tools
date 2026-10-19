#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ManualView.h"
#include "ui/Bars42.h"
#include "ui/ExtView42.h"
#include "ui/PanelView42.h"

class AT42Editor : public juce::AudioProcessorEditor
{
public:
    explicit AT42Editor (AT42Processor&);
    ~AT42Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kBarH = 46, kLowerH = 30;

    AT42Processor& proc;
    juce::Component content;
    dh::TopBar42 bar;
    dh::PanelView42 panel;
    dh::ExtView42 ext;
    dh::LowerBar42 lower;
    dh::manual::ManualHost manualHost;     // el manual encima del editor (botón MANUAL)
    int baseW = 0, baseH = 0;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AT42Editor)
};
