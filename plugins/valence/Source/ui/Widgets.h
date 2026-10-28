#pragma once
#include "../../../../shared/ui/Wheel.h"
#include "../../../../shared/ui/HelpRouter.h"
#include "../../../../shared/ui/Showing.h"
// Controles de Valence (los de Orrery) atados a parámetros del host: la perilla-órbita, la caja (elección o entero que se
// arrastra) y la llave. Todos muestran ayuda al pasar el mouse.

#include "Look.h"
#include "../PluginProcessor.h"

namespace dh::val::ui {

inline void showHelp (const juce::String& s) { dh::HelpRouter::help (s); }

// Se repinta cuando cambia el parámetro (la ParameterAttachment avisa de cualquier cambio: la interfaz, el host, un
// preset). Sin temporizador propio: en Linux cada isShowing() es una ida y vuelta al servidor X, y con un temporizador
// por control la página Layers pagaba cientos por segundo (README, Editor).
class Bound : public juce::Component
{
public:
    Bound (ValenceProcessor& p, const juce::String& id, juce::String label, juce::String help)
        : text (std::move (label)), tip (std::move (help))
    {
        param = p.apvts.getParameter (id);
        if (param != nullptr) { attach = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); }); attach->sendInitialUpdate(); }
        setRepaintsOnMouseActivity (true);
    }
    void mouseEnter (const juce::MouseEvent&) override { showHelp (text + (tip.isNotEmpty() ? ": " + tip : juce::String())); }
    void mouseExit (const juce::MouseEvent&) override { showHelp ({}); }
    juce::Colour tint = col::sun;
protected:
    juce::String text, tip;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attach;
    void setNorm (float v, bool gesture) { if (gesture) attach->setValueAsPartOfGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v))); else attach->setValueAsCompleteGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v))); }
};

// Perilla-órbita: el valor (0..1) es un planeta sobre su arco. La dibujan OrbitKnob y las macros (Macros.h).
inline void drawOrbit (juce::Graphics& g, juce::Rectangle<float> b, float v, const juce::String& value, const juce::String& label,
                       juce::Colour tint, bool large, bool over)
{
    const float labelH = large ? 36.0f : 30.0f;
    const float d = std::min (b.getWidth(), b.getHeight() - labelH) - 6.0f;
    const auto c = juce::Rectangle<float> (d, d).withCentre ({ b.getCentreX(), b.getY() + 3.0f + d / 2 });
    const float a0 = -2.4f, a1 = 2.4f, ang = a0 + (a1 - a0) * juce::jlimit (0.0f, 1.0f, v);
    const float rad = d / 2 - 4.0f;
    juce::Path track; track.addCentredArc (c.getCentreX(), c.getCentreY(), rad, rad, 0, a0, a1, true);
    g.setColour (col::border); g.strokePath (track, juce::PathStrokeType (1.4f));
    juce::Path arc; arc.addCentredArc (c.getCentreX(), c.getCentreY(), rad, rad, 0, a0, ang, true);
    g.setColour (tint.withAlpha (0.75f)); g.strokePath (arc, juce::PathStrokeType (large ? 2.6f : 2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    const auto planet = c.getCentre().getPointOnCircumference (rad, ang);
    const float ps = large ? 12.0f : 9.0f;
    g.setColour (tint.withAlpha (0.18f)); g.fillEllipse (juce::Rectangle<float> (ps * 2.4f, ps * 2.4f).withCentre (planet));
    g.setColour (tint); g.fillEllipse (juce::Rectangle<float> (ps, ps).withCentre (planet));
    g.setColour (over ? col::text : col::dim);
    g.setFont (font (large ? 15.0f : 12.5f, large));
    g.drawFittedText (value, c.reduced (d * 0.16f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    g.setColour (col::dim); g.setFont (font (large ? 14.0f : 12.0f));
    g.drawText (label, b.withTop (b.getBottom() - labelH + 4).toNearestInt(), juce::Justification::centredTop);
}

class OrbitKnob : public Bound
{
public:
    OrbitKnob (ValenceProcessor& p, const juce::String& id, juce::String label, juce::String help = {}, bool big = false)
        : Bound (p, id, std::move (label), std::move (help)), large (big) {}
    void paint (juce::Graphics& g) override
    {
        if (param != nullptr) drawOrbit (g, getLocalBounds().toFloat(), param->getValue(), param->getCurrentValueAsText(), text, tint, large, isMouseOverOrDragging());
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
    ParamBox (ValenceProcessor& p, const juce::String& id, juce::String label, juce::String help = {}, bool showLabel = true)
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
        g.drawText (param->getCurrentValueAsText(), r.reduced (8, 0).toNearestInt(), tog ? juce::Justification::centred : juce::Justification::centredLeft);
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

// Llave de dos estados con su nombre y el estado a la vista ("Preview sound: On"), atada a un parámetro.
class PillToggle : public juce::TextButton, private juce::Timer
{
public:
    PillToggle (ValenceProcessor& p, const juce::String& id, juce::String label, juce::String help = {})
        : text (std::move (label)), tip (std::move (help))
    {
        param = p.apvts.getParameter (id);
        if (param != nullptr) attach = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { refresh(); });
        onClick = [this] { if (param != nullptr) attach->setValueAsCompleteGesture (param->getValue() > 0.5f ? 0.0f : 1.0f); };
        refresh();
        startTimerHz (8);
    }
    bool isOn() const { return param != nullptr && param->getValue() > 0.5f; }
    void mouseEnter (const juce::MouseEvent& e) override { juce::TextButton::mouseEnter (e); if (tip.isNotEmpty()) showHelp (text + ": " + tip); }
    void mouseExit (const juce::MouseEvent& e) override { juce::TextButton::mouseExit (e); showHelp ({}); }
    juce::Colour tint = col::sun;
    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        const auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = isOn();
        auto bgc = on ? tint.withAlpha (0.20f) : col::raised;
        if (over) bgc = bgc.brighter (0.08f);
        if (down) bgc = bgc.darker (0.12f);
        g.setColour (bgc); g.fillRoundedRectangle (r, r.getHeight() / 2);
        g.setColour (on ? tint : col::border); g.drawRoundedRectangle (r, r.getHeight() / 2, on ? 1.4f : 1.0f);
        // Una luz: encendida, del color; apagada, hueca.
        const float d = r.getHeight() * 0.32f;
        const auto dot = juce::Rectangle<float> (d, d).withCentre ({ r.getX() + r.getHeight() * 0.55f, r.getCentreY() });
        if (on) { g.setColour (tint.withAlpha (0.35f)); g.fillEllipse (dot.expanded (d * 0.5f)); g.setColour (tint); g.fillEllipse (dot); }
        else { g.setColour (col::faint); g.drawEllipse (dot, 1.2f); }
        g.setFont (font (std::min (14.0f, r.getHeight() * 0.46f), true));
        juce::AttributedString a;
        a.append (text + ": ", font (std::min (14.0f, r.getHeight() * 0.46f)), col::dim);
        a.append (on ? "On" : "Off", font (std::min (14.0f, r.getHeight() * 0.46f), true), on ? col::text : col::dim);
        a.setJustification (juce::Justification::centredLeft);
        a.draw (g, r.withTrimmedLeft (r.getHeight() * 0.95f).withTrimmedRight (8.0f));
    }
private:
    juce::String text, tip;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attach;
    bool last = false;
    void refresh() { last = isOn(); setToggleState (last, juce::dontSendNotification); repaint(); }
    void timerCallback() override { if (isOn() != last) refresh(); }
};

// Lo quieto de un componente opaco en una imagen a la escala física de la pantalla: se rehace entero sólo si cambian los
// datos, el tamaño o la escala (dirty), o por zonas con update(); cada repintado copia del caché sólo el recorte y encima
// se dibuja lo que se mueve (el cursor). Con la ventana escalada JUCE dibuja texto y rectángulos redondeados como caminos,
// y eso era lo caro de la página Layers (README, Editor).
struct PaintCache
{
    juce::Image img;
    float scale = 0.0f;
    bool dirty = true;
    template <typename F> void draw (juce::Graphics& g, const juce::Component& c, F&& render)
    {
        const float pf = g.getInternalContext().getPhysicalPixelScaleFactor();
        const float s = pf > 0.0f ? pf : 1.0f;
        const int w = std::max (1, juce::roundToInt ((float) c.getWidth() * s)), h = std::max (1, juce::roundToInt ((float) c.getHeight() * s));
        if (dirty || img.isNull() || std::abs (s - scale) > 1.0e-4f || img.getWidth() != w || img.getHeight() != h)
        {
            img = juce::Image (juce::Image::RGB, w, h, false, juce::SoftwareImageType());
            juce::Graphics ig (img);
            ig.addTransform (juce::AffineTransform::scale (s));
            render (ig);
            scale = s; dirty = false;
        }
        g.setOpacity (1.0f);
        g.drawImageTransformed (img, juce::AffineTransform::scale (1.0f / scale));
    }
    // Rehace sólo una zona (coordenadas del componente) y la repinta.
    template <typename F> void update (juce::Component& c, juce::Rectangle<int> area, F&& render)
    {
        if (! dirty && img.isValid())
        {
            juce::Graphics ig (img);
            ig.addTransform (juce::AffineTransform::scale (scale));
            ig.reduceClipRegion (area);
            render (ig);
        }
        c.repaint (area);
    }
    void invalidate (juce::Component& c) { dirty = true; c.repaint(); }
};

inline void gridLayout (juce::Rectangle<int> r, const std::vector<juce::Component*>& items, int cols, int rowH, int gap = 8)
{
    if (items.empty()) return;
    const int w = (r.getWidth() - gap * (cols - 1)) / cols;
    for (size_t i = 0; i < items.size(); ++i)
        if (items[i] != nullptr) items[i]->setBounds (r.getX() + ((int) i % cols) * (w + gap), r.getY() + ((int) i / cols) * (rowH + gap), w, rowH);
}

} // namespace dh::val::ui
