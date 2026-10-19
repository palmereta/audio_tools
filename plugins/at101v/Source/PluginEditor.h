#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ManualView.h"
#include "ui/Bars101.h"
#include "ui/ExtView101.h"
#include "ui/PanelView101.h"
#include "ui/SeqView101.h"

class AT101VEditor : public juce::AudioProcessorEditor
{
public:
    explicit AT101VEditor (AT101VProcessor&);
    ~AT101VEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int kBarH = 46, kLowerH = 30;

    AT101VProcessor& proc;
    juce::Component content;
    dh::TopBar101 bar;
    dh::PanelView101 panel;
    dh::SeqView101 seq;
    dh::ExtView101 ext;
    dh::LowerBar101 lower;
    dh::manual::ManualHost manualHost;     // el manual encima del editor (botón MANUAL)
    int baseW = 0, baseH = 0, octave = 0;
    juce::TooltipWindow tooltips { this, 600 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AT101VEditor)
};
