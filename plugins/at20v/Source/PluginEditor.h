#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ManualView.h"
#include "ui/ExtPanelView.h"
#include "ui/KeyboardView.h"
#include "ui/LowerBar.h"
#include "ui/PanelView.h"
#include "ui/TopBar.h"

class AT20VEditor : public juce::AudioProcessorEditor
{
public:
    explicit AT20VEditor (AT20VProcessor&);
    ~AT20VEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kBarH = 46, kLowerH = 30;

    AT20VProcessor& proc;
    juce::Component content;
    dh::TopBar bar;
    dh::PanelView panel;
    dh::ExtPanelView ext;
    dh::KeyboardView keyboard;
    dh::LowerBar lower;
    dh::manual::ManualHost manualHost;     // el manual encima del editor (botón MANUAL)
    int baseW = 0, baseH = 0;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AT20VEditor)
};
