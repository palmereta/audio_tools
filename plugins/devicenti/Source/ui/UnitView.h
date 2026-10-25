#pragma once
// DeviceNTI V: the unit in 3/4 perspective (plan §8, phase 2). The background is the Blender render; on it the view
// draws what changes: knob pointers and the screens through each plane's homography, pressed and lit keys and lit
// LEDs as sprites rendered at the same pixels. Every control is hit-tested through the inverse homography of its top
// plane (and of its base, for knobs), so the click lands where the control is seen.
//
// The panel's behaviour (FUNC layer, track keys, REC and the STEP keys, step locks, the small display) lives here
// too. With a mouse two things cannot be held at once, so: FUNC latches on a click (Shift holds it), a STEP key held
// for half a second (or Alt-clicked, or right-clicked) stays held for locks and trig fields until it is clicked
// again or Escape is pressed, and TRACK + TRACK (solo) is FUNC-click on a track key while another is held, or the
// track key's right-click menu.
//
// The paint loop runs only while the view is showing (a hidden editor costs nothing).

#include <JuceHeader.h>
#include "UiModel.h"
#include "Fields.h"
#include <set>

namespace dh::dv::ui {

class UnitView : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    UnitView (DeVicentiProcessor& p, PanelState& s);
    ~UnitView() override;

    void paint (juce::Graphics&) override;
    void resized() override {}
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; }
    bool keyPressed (const juce::KeyPress&) override;
    void modifierKeysChanged (const juce::ModifierKeys&) override;
    void visibilityChanged() override { updateTimer(); }
    void parentHierarchyChanged() override { updateTimer(); }

    std::function<void (Page)> onOpenPage;       // a back page from the display's context menu
    bool learnMode = false;                      // MIDI learn: the next knob clicked is armed
    std::function<void()> onLearnArmed;

    // Tests and snapshots: press a control by id (keys) or set a knob (0..1).
    void pressKey (const juce::String& id) { const int i = indexOf (id); if (i >= 0) { keyDown (i, {}); keyUp (i, false); } }
    void holdStep (int absStep) { st.heldStep = absStep; st.heldLocked = false; st.page = Page::Trig; repaint(); }
    int indexOf (const juce::String& id) const;
    int controlAt (juce::Point<float> p) const { return hitControl (p); }
    float pxScale() const { return model.width > 0 ? (float) getWidth() / (float) model.width : 1.0f; }

private:
    DeVicentiProcessor& proc;
    PanelState& st;
    const Model& model;
    juce::Image displayImg, opImg[4];
    int hover = -1, dragCtl = -1, pressedKey = -1;
    float dragStartNorm = 0.0f, dragAccum = 0.0f; int dragLastY = 0; bool dragMoved = false;
    juce::uint32 keyDownAt = 0;
    std::set<int> keysHeld;                      // control indexes of keys held with the mouse (or latched steps)
    bool fillHeld = false;
    int blinkPhase = 0; juce::uint32 lastFrame = 0;
    std::array<int, 16> ledShown {}; std::array<bool, 64> keyLitShown {};
    std::array<float, 64> knobShown {};

    // geometry
    int hitControl (juce::Point<float> pos) const;
    juce::Point<float> toBg (juce::Point<float> p) const { const float s = pxScale(); return { p.x / s, p.y / s }; }

    // panel behaviour
    const panel::Control& pc (int idx) const { return panel::controls()[(size_t) model.ctls[(size_t) idx].panelIndex]; }
    bool isKit() const;
    int laneForSound() const { return isKit() ? juce::jlimit (1, 8, st.lane) : 0; }
    float knobNorm (int idx) const;
    void knobTurn (int idx, float delta, bool fine);
    int knobSteps (int idx) const;
    void knobGesture (int idx, bool begin);
    void keyDown (int idx, const juce::ModifierKeys& mods);
    void keyUp (int idx, bool cancelled);
    void stepKey (int i, bool down, const juce::ModifierKeys& mods);
    void flash (const juce::String& name, const juce::String& value) { st.flashName = name; st.flashValue = value; st.flashUntil = juce::Time::getMillisecondCounter() + 1400; }
    int lockIdFor (const panel::Target& t, int& lane) const;
    void setTrack (int t);
    void showContextMenu (int idx);
    void armLearnFor (int idx);
    void finishChordCapture (bool storeOnly);
    bool chordLatched = false; int lastChordSlot = -1;
    juce::String knobValueText (int idx) const;
    int currentStepOf (int track) const { return fakePlayhead >= 0 ? fakePlayhead : proc.viewStep[(size_t) juce::jlimit (0, kTracks - 1, track)].load(); }
    bool playing() const { return fakePlayhead >= 0 || proc.viewPattern.load() >= 0; }
    int fakePlayhead = juce::SystemStats::getEnvironmentVariable ("DV_PLAYHEAD", "0").getIntValue() - 1;     // snapshots: a playhead without audio
    int playNoteForKey (int i) const;

    // drawing
    void drawPointer (juce::Graphics&, const Ctl&, float norm) const;
    void drawSprite (juce::Graphics&, const Ctl&, const char* which, float alpha) const;
    void drawWarped (juce::Graphics&, const juce::Image&, const Ctl&) const;
    bool renderDisplay();                         // true when the picture changed
    bool renderOpScreen (int op);
    juce::String displaySig, opSig[4];
    int ledState (int i) const;                  // 0 off, 1 dim, 2 on, 3 playhead
    bool keyLit (int idx) const;

    void timerCallback() override;
    void updateTimer() { if (isShowing()) { if (! isTimerRunning()) startTimerHz (30); } else stopTimer(); }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UnitView)
};

} // namespace dh::dv::ui
