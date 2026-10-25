#pragma once
// DeVicenti: the back pages, the small display enlarged into full editors (plan §7 phase 2): every field of a page
// at once, grouped, as rows you drag (left-right or up-down), scroll or click (choices step on a click). SEQ is the
// pattern's step grid (all tracks, all KIT lanes, 64 steps); SETTINGS holds the window size and the help text.

#include <JuceHeader.h>
#include "Fields.h"

namespace dh::dv::ui {

struct Theme
{
    static juce::Colour bg() { return juce::Colour (0xff1b1410); }
    static juce::Colour panel() { return juce::Colour (0xff261c16); }
    static juce::Colour ink() { return juce::Colour (0xffefe2c4); }
    static juce::Colour dim() { return juce::Colour (0xff9c8a74); }
    static juce::Colour accent() { return juce::Colour (0xffe8792b); }
    static juce::Colour accent2() { return juce::Colour (0xffefaa2c); }
    static juce::Font font (float h, bool bold = true);
};

class FieldPage : public juce::Component, private juce::Timer
{
public:
    FieldPage (DeVicentiProcessor& p, PanelState& s) : proc (p), st (s) {}
    void setPage (Page pg) { page = pg; rebuild(); }
    Page getPage() const { return page; }
    void rebuild() { fields = fieldsFor (proc, st, page); layoutRows(); repaint(); }
    void paint (juce::Graphics&) override;
    void resized() override { layoutRows(); }
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void visibilityChanged() override { if (isShowing()) startTimerHz (10); else stopTimer(); }

private:
    DeVicentiProcessor& proc; PanelState& st;
    Page page = Page::Voice;
    FieldList fields;
    struct Row { int field; juce::Rectangle<float> r; };
    struct Group { juce::String name; juce::Rectangle<float> r; };
    std::vector<Row> rows; std::vector<Group> groups;
    int dragRow = -1; float dragStart = 0, dragNorm = 0; juce::Point<float> downPos; bool moved = false;
    void layoutRows();
    int rowAt (juce::Point<float>) const;
    void timerCallback() override { repaint(); }
};

class SeqPage : public juce::Component, private juce::Timer
{
public:
    SeqPage (DeVicentiProcessor& p, PanelState& s) : proc (p), st (s) {}
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void visibilityChanged() override { if (isShowing()) startTimerHz (20); else stopTimer(); }
private:
    DeVicentiProcessor& proc; PanelState& st;
    struct Lane { int track, lane; juce::Rectangle<float> r; };
    std::vector<Lane> lanes;
    float cellW = 10, x0 = 0;
    int paintValue = -1;                 // drag paints this state (0/1)
    bool cellAt (juce::Point<float>, int& li, int& step) const;
    void timerCallback() override { repaint(); }
};

class SettingsPage : public juce::Component
{
public:
    SettingsPage (DeVicentiProcessor& p) : proc (p) {}
    std::function<void (float)> onScale;
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
private:
    DeVicentiProcessor& proc;
    std::vector<std::pair<float, juce::Rectangle<float>>> sizeButtons;
};

} // namespace dh::dv::ui
