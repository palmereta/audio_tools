#pragma once
// Panel del AT20V: fondo renderizado, perillas por sprites, patchera con cables físicos y LEDs.

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Assets.h"
#include "Rope.h"

namespace dh {

class PanelView : public juce::Component, private juce::Timer
{
public:
    explicit PanelView (AT20VProcessor&);
    ~PanelView() override;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseExit (const juce::MouseEvent&) override;

    void rebuildCables();

private:
    void timerCallback() override;

    struct KnobView
    {
        int index;
        juce::Point<float> base;      // pie de la perilla (coordenadas de vista)
        float radius;
        bool big;
        int steps;
        float startDeg, endDeg;
        float shown = -1.0f;
        juce::RangedAudioParameter* param = nullptr;
    };
    struct JackView
    {
        int index;
        juce::Point<float> base, plugTop;
        bool isOutput;
    };
    struct CableView
    {
        int src, dst;
        juce::String color;
        Rope rope;
    };
    struct DragCable
    {
        bool active = false;
        int fixedJack = -1;          // el extremo que queda enchufado
        juce::Point<float> free;     // el extremo en la mano
        juce::String color;
        Rope rope;
    };

    int knobAt (juce::Point<float>) const;
    int jackAt (juce::Point<float>) const;
    float knobValue (const KnobView&) const;
    float knobAngle (const KnobView&, float value) const;
    void setKnobValue (KnobView&, float value);
    juce::String nextColor();
    bool canConnect (int fixedJack, int target) const;
    void commitCables (std::vector<Cable> cs);
    void drawCable (juce::Graphics&, const juce::Path&, const juce::String& color, bool shadowOnly) const;
    void drawPlug (juce::Graphics&, juce::Point<float> base, const juce::String& color, float lift = 0.0f) const;
    void showJackMenu (int jack);

    AT20VProcessor& proc;
    Assets& A = Assets::get();
    std::vector<KnobView> knobs;
    std::vector<JackView> jacks;
    std::vector<CableView> cables;
    DragCable drag;

    int dragKnob = -1, hoverKnob = -1, hoverJack = -1;
    float dragStartValue = 0.0f, dragAccum = 0.0f;
    juce::Point<float> lastMouse;
    int colorCycle = 0;

    std::array<float, 4> ledShown { -1, -1, -1, -1 };
    double lastTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelView)
};

} // namespace dh
