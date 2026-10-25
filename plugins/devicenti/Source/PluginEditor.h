#pragma once
// DeVicenti editor (phase 2): a thin top bar (preset browser, MIDI learn, the back pages, MIDI settings, settings,
// save) over the unit in 3/4 perspective (ui/UnitView), or over a back page (ui/Pages). The window keeps the
// render's proportions and scales from 60 % to 160 %; the render is made at 2x, so it stays sharp on HiDPI screens.
//
// Self-test without screen capture: DV_SNAPSHOT=/path.png saves the editor (DV_SNAP_SCALE, default 2) after the
// setup below, then quits the standalone when DV_SNAP_QUIT=1. Setup: DV_PRESET=n, DV_TRACK=1..4, DV_PAGE=VOICE|OPS|…
// (a back page) or MANUAL, DV_REC=1 (grid record), DV_FUNC=1, DV_HOLD=n (hold step n), DV_LED=1 (a fake playhead is not
// possible: start the standalone transport with DV_PLAY=1), DV_W=width.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/UnitView.h"
#include "ui/Pages.h"

class DeVicentiEditor : public juce::AudioProcessorEditor
{
public:
    explicit DeVicentiEditor (DeVicentiProcessor&);
    ~DeVicentiEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int kBaseW = 1120, kViewH = 800, kBarH = 30, kBaseH = kViewH + kBarH;
    void openPage (int page);            // −1: the unit; otherwise a ui::Page

private:
    class TopBar : public juce::Component
    {
    public:
        explicit TopBar (DeVicentiEditor& e) : ed (e) {}
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
    private:
        DeVicentiEditor& ed;
        std::vector<std::pair<juce::String, juce::Rectangle<float>>> hits;
    };

    DeVicentiProcessor& proc;
    dh::dv::ui::PanelState state;
    TopBar bar { *this };
    dh::dv::ui::UnitView unit;
    dh::dv::ui::FieldPage fieldPage;
    dh::dv::ui::SeqPage seqPage;
    dh::dv::ui::SettingsPage settingsPage;
    juce::TooltipWindow tips { this, 700 };
    int page = -1;
    std::unique_ptr<juce::Component> manual;      // the embedded manual (docs/manual), over everything
    void toggleManual();

    void presetMenu();
    void pagesMenu();
    void savePreset();
    void setupSnapshot();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DeVicentiEditor)
};
