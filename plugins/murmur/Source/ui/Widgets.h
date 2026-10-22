#pragma once
#include "../../../../shared/ui/Wheel.h"
#include "../../../../shared/ui/HelpRouter.h"
#include "../../../../shared/ui/Showing.h"
// Controles de Murmur atados a parámetros del host: la perilla-órbita, la caja (elección o entero que se
// arrastra) y la llave. Todos muestran ayuda al pasar el mouse.

#include "Look.h"
#include "../PluginProcessor.h"

namespace dh::mm::ui {

inline void showHelp (const juce::String& s) { dh::HelpRouter::help (s); }

class Bound : public juce::Component, private juce::Timer
{
public:
    Bound (MurmurProcessor& p, const juce::String& id, juce::String label, juce::String help)
        : text (std::move (label)), tip (std::move (help))
    {
        param = p.apvts.getParameter (id);
        if (tip.isEmpty()) tip = paramDef (id).help;
        if (param != nullptr) { attach = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); }); attach->sendInitialUpdate(); }
        setRepaintsOnMouseActivity (true);
        startTimerHz (12);
    }
    void mouseEnter (const juce::MouseEvent&) override { showHelp (text + (tip.isNotEmpty() ? ": " + tip : juce::String())); }
    void mouseExit (const juce::MouseEvent&) override { showHelp ({}); }
    juce::Colour tint = col::sun;
protected:
    juce::String text, tip;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attach;
    float last = -1.0f;
    void timerCallback() override { if (param != nullptr && dh::reallyShowing (*this) && std::abs (param->getValue() - last) > 1e-5f) { last = param->getValue(); repaint(); } }
    void setNorm (float v, bool gesture) { if (gesture) attach->setValueAsPartOfGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v))); else attach->setValueAsCompleteGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v))); }
};

// Perilla: un arco con un pájaro en el valor.
class BirdKnob : public Bound
{
public:
    BirdKnob (MurmurProcessor& p, const juce::String& id, juce::String label, juce::String help = {}, bool big = false)
        : Bound (p, id, std::move (label), std::move (help)), large (big) {}
    void paint (juce::Graphics& g) override
    {
        if (param == nullptr) return;
        const auto b = getLocalBounds().toFloat();
        const float labelH = large ? 36.0f : 30.0f;
        const float d = std::min (b.getWidth(), b.getHeight() - labelH) - 6.0f;
        const auto c = juce::Rectangle<float> (d, d).withCentre ({ b.getCentreX(), 3.0f + d / 2 });
        const float a0 = -2.4f, a1 = 2.4f, v = param->getValue(), ang = a0 + (a1 - a0) * v;
        const float rad = d / 2 - 4.0f;
        juce::Path track; track.addCentredArc (c.getCentreX(), c.getCentreY(), rad, rad, 0, a0, a1, true);
        g.setColour (col::border); g.strokePath (track, juce::PathStrokeType (1.4f));
        juce::Path arc; arc.addCentredArc (c.getCentreX(), c.getCentreY(), rad, rad, 0, a0, ang, true);
        g.setColour (tint.withAlpha (0.75f)); g.strokePath (arc, juce::PathStrokeType (large ? 2.6f : 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const auto bird = c.getCentre().getPointOnCircumference (rad, ang);
        const float ps = large ? 9.0f : 7.0f;
        g.setColour (tint.withAlpha (0.16f)); g.fillEllipse (juce::Rectangle<float> (ps * 2.6f, ps * 2.6f).withCentre (bird));
        juce::Path wing;      // un pájaro: dos alas en V, orientado según el arco
        wing.startNewSubPath (-ps, -ps * 0.35f); wing.quadraticTo (-ps * 0.4f, -ps * 0.55f, 0.0f, ps * 0.15f);
        wing.quadraticTo (ps * 0.4f, -ps * 0.55f, ps, -ps * 0.35f);
        g.setColour (tint);
        g.strokePath (wing, juce::PathStrokeType (large ? 2.2f : 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::rotation (ang).translated (bird.x, bird.y));
        g.setColour (isMouseOverOrDragging() ? col::text : col::dim);
        g.setFont (font (large ? 15.0f : 12.5f, large));
        g.drawFittedText (param->getCurrentValueAsText(), c.reduced (d * 0.12f).toNearestInt(), juce::Justification::centred, 1, 0.75f);
        g.setColour (col::dim); g.setFont (font (large ? 14.0f : 12.0f));
        g.drawText (text, b.withTop (b.getBottom() - labelH + 4).toNearestInt(), juce::Justification::centredTop);
    }
    void mouseDown (const juce::MouseEvent&) override { if (param != nullptr) { start = param->getValue(); attach->beginGesture(); } }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (param == nullptr) return;
        setNorm (start - (float) e.getDistanceFromDragStartY() / (e.mods.isShiftDown() ? 900.0f : 220.0f), true);
    }
    void mouseUp (const juce::MouseEvent&) override { if (param != nullptr) attach->endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (param != nullptr) setNorm (param->getDefaultValue(), false); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { if (param != nullptr && dh::wheelDelta (w) != 0.0f) setNorm (param->getValue() + dh::wheelAmount (w), false); }
private:
    bool large; float start = 0.0f;
};

// Caja: elección (menú), entero (arrastrar o rueda) o llave de dos estados (clic).
class ParamBox : public Bound
{
public:
    ParamBox (MurmurProcessor& p, const juce::String& id, juce::String label, juce::String help = {}, bool showLabel = true)
        : Bound (p, id, std::move (label), std::move (help)), labelled (showLabel)
    {
        choice = dynamic_cast<juce::AudioParameterChoice*> (param);
    }
    void paint (juce::Graphics& g) override
    {
        if (param == nullptr) return;
        auto b = getLocalBounds();
        if (labelled) { g.setFont (font (11.5f)); g.setColour (col::dim); g.drawText (text, b.removeFromTop (16), juce::Justification::centredLeft); }
        const auto r = b.toFloat().reduced (0.5f, 1.0f);
        const bool tog = isToggle(), on = tog && param->getValue() > 0.5f;
        g.setColour (on ? tint.withAlpha (0.22f) : col::bg); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? tint : (isMouseOverOrDragging() ? col::dim : col::border)); g.drawRoundedRectangle (r, 6.0f, 1.0f);
        g.setColour (tog ? (on ? col::text : col::dim) : col::text);
        g.setFont (font (13.5f, true));
        // Una llave sin rótulo arriba (Freeze) muestra su nombre adentro, no "On/Off".
        const auto shown = tog && ! labelled ? text : param->getCurrentValueAsText();
        g.drawText (shown, r.reduced (8, 0).toNearestInt(), tog ? juce::Justification::centred : juce::Justification::centredLeft);
        if (choice != nullptr && ! tog)
        {
            juce::Path tri; const float cx = r.getRight() - 11.0f, cy = r.getCentreY();
            tri.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
            g.setColour (col::dim); g.fillPath (tri);
        }
    }
    void mouseDown (const juce::MouseEvent&) override
    {
        if (param == nullptr) return;
        if (isToggle()) { setNorm (param->getValue() > 0.5f ? 0.0f : 1.0f, false); return; }
        if (choice != nullptr)
        {
            juce::PopupMenu m;
            for (int i = 0; i < choice->choices.size(); ++i) m.addItem (i + 1, choice->choices[i], true, i == choice->getIndex());
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r) { if (r > 0) attach->setValueAsCompleteGesture ((float) (r - 1)); });
            return;
        }
        start = param->getValue(); attach->beginGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (param == nullptr || choice != nullptr) return;
        const int steps = std::max (2, param->getNumSteps());
        const float px = e.mods.isShiftDown() ? 14.0f : (steps > 60 ? 2.0f : 5.0f);
        setNorm (start - (float) e.getDistanceFromDragStartY() / px / (float) (steps - 1), true);
    }
    void mouseUp (const juce::MouseEvent&) override { if (param != nullptr && choice == nullptr) attach->endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (param != nullptr && choice == nullptr) setNorm (param->getDefaultValue(), false); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (param == nullptr) return;
        const int steps = std::max (2, param->getNumSteps());
        const float stp = steps < 400 ? 1.0f / (float) (steps - 1) : 0.01f;
        if (const int k = wheelSteps.take (w); k != 0) setNorm (param->getValue() + (float) k * stp, false);
    }
private:
    dh::WheelSteps wheelSteps;
    juce::AudioParameterChoice* choice = nullptr;
    bool labelled;
    float start = 0.0f;
    bool isToggle() const { return choice != nullptr && choice->choices.size() == 2; }
};

inline void gridLayout (juce::Rectangle<int> r, const std::vector<juce::Component*>& items, int cols, int rowH, int gap = 8)
{
    if (items.empty()) return;
    const int w = (r.getWidth() - gap * (cols - 1)) / cols;
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i] != nullptr) items[i]->setBounds (r.getX() + ((int) i % cols) * (w + gap), r.getY() + ((int) i / cols) * (rowH + gap), w, rowH);
}

} // namespace dh::mm::ui
