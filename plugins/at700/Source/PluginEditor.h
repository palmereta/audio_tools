#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ManualView.h"
#include "ui/Bars700.h"
#include "ui/ExtView700.h"
#include "ui/PanelView700.h"

class AT700Editor : public juce::AudioProcessorEditor
{
public:
    explicit AT700Editor (AT700Processor&);
    ~AT700Editor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kBarH = 46, kLowerH = 30, kBaseW = 1800, kPortH = 1080;

    void showView (int v, bool animate = true);

    AT700Processor& proc;
    juce::Component content;
    dh::TopBar700 bar;
    dh::PanelView700 panel;
    dh::PanelPort700 port;
    dh::ExtView700 ext;
    dh::LowerBar700 lower;
    dh::manual::ManualHost manualHost;     // el manual encima del editor (botón MANUAL)
    int baseW = kBaseW, baseH = kBarH + kPortH + kLowerH;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AT700Editor)
};
