#pragma once
// Controles de Thrum dibujados a mano (perilla, chips, interruptor, deslizador, menú, botón), cada uno atado a un
// parámetro por juce::ParameterAttachment: el gesto llega al host (automatización, deshacer) y el control se repinta
// solo cuando el valor cambia (sin temporizadores por control).

#include "Theme.h"
#include "../PluginProcessor.h"

namespace thrumui {

class ParamControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    ParamControl (ThrumProcessor& p, const juce::String& id) : proc (p)
    {
        param = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (id));
        jassert (param != nullptr);
        if (param != nullptr)
        {
            att = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); }, nullptr);
            att->sendInitialUpdate();
        }
    }
    float value() const { return param ? param->convertFrom0to1 (param->getValue()) : 0.0f; }
    float norm() const { return param ? param->getValue() : 0.0f; }
    juce::String valueText() const
    {
        if (! param) return {};
        auto t = param->getCurrentValueAsText();
        const auto l = param->getLabel();
        return l.isNotEmpty() && ! t.endsWith (l) ? t + " " + l : t;
    }
    void set (float v) { if (att) att->setValueAsCompleteGesture (v); }
    juce::RangedAudioParameter* param = nullptr;
protected:
    ThrumProcessor& proc;
    std::unique_ptr<juce::ParameterAttachment> att;
};

// Perilla: arrastre vertical u horizontal (Mayúsculas = fino), rueda, doble clic = valor de fábrica.
class Knob : public ParamControl
{
public:
    Knob (ThrumProcessor& p, const juce::String& id, juce::String name = {}) : ParamControl (p, id), title (name.isEmpty() && param ? param->getName (40) : name) {}
    juce::String title;
    bool small = false;
    std::function<juce::String (float)> format;      // opcional: el valor en unidades propias (ms, una división)
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float labH = 32.0f;
        const float d = std::min (r.getWidth() - 12.0f, r.getHeight() - labH - 6.0f);
        auto k = juce::Rectangle<float> (d, d).withCentre ({ r.getCentreX(), r.getY() + d * 0.5f + 3.0f });
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f;
        const float a = a0 + (a1 - a0) * norm();
        // Sombra, cuerpo y la escala de ámbar.
        g.setColour (juce::Colours::black.withAlpha (0.45f)); g.fillEllipse (k.translated (1.0f, 2.0f).reduced (4.0f));
        juce::ColourGradient grad (col::cell2.brighter (0.15f), k.getX(), k.getY(), col::bg, k.getRight(), k.getBottom(), false);
        g.setGradientFill (grad); g.fillEllipse (k.reduced (4.0f));
        g.setColour (col::lineHi); g.drawEllipse (k.reduced (4.0f), 1.0f);
        juce::Path track; track.addCentredArc (k.getCentreX(), k.getCentreY(), d * 0.5f, d * 0.5f, 0.0f, a0, a1, true);
        g.setColour (col::line); g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path arc; arc.addCentredArc (k.getCentreX(), k.getCentreY(), d * 0.5f, d * 0.5f, 0.0f, a0, a, true);
        g.setColour (col::amber); g.strokePath (arc, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float rr = d * 0.5f - 8.0f;
        g.setColour (col::text);
        g.drawLine (k.getCentreX(), k.getCentreY(), k.getCentreX() + rr * std::sin (a), k.getCentreY() - rr * std::cos (a), 2.0f);
        auto lr = r.withTop (k.getBottom() + 1.0f);
        label (g, title, lr.removeFromTop (15.0f), juce::Justification::centredTop, small ? 12.0f : 12.5f);
        g.setColour (hover || dragging ? col::amberHi : col::text);
        text (g, format ? format (value()) : valueText(), lr.removeFromTop (15.0f), juce::Justification::centredTop, small ? 12.5f : 13.5f);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { if (! att) return; dragging = true; start = norm(); last = e.position; att->beginGesture(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! att) return;
        const float fine = e.mods.isShiftDown() ? 0.2f : 1.0f;
        const auto d = e.position - last; last = e.position;
        start = juce::jlimit (0.0f, 1.0f, start + (d.x - d.y) * 0.005f * fine);
        att->setValueAsPartOfGesture (param->convertFrom0to1 (start));
    }
    void mouseUp (const juce::MouseEvent&) override { if (att) att->endGesture(); dragging = false; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (param) set (param->convertFrom0to1 (param->getDefaultValue())); }
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (! param) return;
        const float step = (e.mods.isShiftDown() ? 0.004f : 0.02f) * (w.deltaY + w.deltaX >= 0 ? 1.0f : -1.0f);
        set (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, norm() + step)));
    }
private:
    bool hover = false, dragging = false; float start = 0; juce::Point<float> last;
};

// Chips: una opción por casilla (parámetro de elección o interruptor). labels vacío = los nombres del parámetro.
class Chips : public ParamControl
{
public:
    Chips (ThrumProcessor& p, const juce::String& id, juce::StringArray labs = {}, int cols = 0) : ParamControl (p, id), labels (labs), columns (cols)
    {
        if (labels.isEmpty() && param) labels = param->getAllValueStrings();
        if (labels.isEmpty()) labels = { "Off", "On" };
    }
    juce::StringArray labels;
    int columns = 0;          // 0 = todas en una fila
    std::vector<int> map;     // opcional: casilla → índice del parámetro (para mostrar sólo algunas)
    float fontH = 13.5f;
    int index (int k) const { return map.empty() ? k : map[(size_t) k]; }
    int current() const { return juce::roundToInt (value()); }
    juce::Rectangle<float> cellR (int k) const
    {
        const int n = labels.size(), c = columns > 0 ? columns : n, rows = (n + c - 1) / c;
        const float gap = 6.0f, w = ((float) getWidth() - gap * (float) (c - 1)) / (float) c, h = ((float) getHeight() - gap * (float) (rows - 1)) / (float) rows;
        return { (float) (k % c) * (w + gap), (float) (k / c) * (h + gap), w, h };
    }
    void paint (juce::Graphics& g) override
    {
        for (int k = 0; k < labels.size(); ++k)
        {
            auto r = cellR (k);
            const bool on = index (k) == current();
            g.setColour (on ? col::amber : (hot == k ? col::cell2 : col::panel)); g.fillRoundedRectangle (r, 4.0f);
            if (! on) { g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f); }
            g.setColour (on ? col::ink : col::text);
            text (g, labels[k].toUpperCase(), r.reduced (4.0f, 0.0f), juce::Justification::centred, fontH, on, 0.04f);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { int h = hit (e.position); if (h != hot) { hot = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hot = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override { const int h = hit (e.position); if (h >= 0) set ((float) index (h)); }
private:
    int hot = -1;
    int hit (juce::Point<float> p) const { for (int k = 0; k < labels.size(); ++k) if (cellR (k).contains (p)) return k; return -1; }
};

// Interruptor: una píldora con su rótulo (un parámetro booleano).
class Toggle : public ParamControl
{
public:
    Toggle (ThrumProcessor& p, const juce::String& id, juce::String name = {}) : ParamControl (p, id), title (name.isEmpty() && param ? param->getName (40) : name) {}
    juce::String title;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = value() > 0.5f;
        g.setColour (on ? col::amber : col::panel); g.fillRoundedRectangle (r, 4.0f);
        if (! on) { g.setColour (col::line); g.drawRoundedRectangle (r, 4.0f, 1.0f); }
        // El LED a la izquierda.
        auto led = juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ r.getX() + 12.0f, r.getCentreY() });
        g.setColour (on ? col::ink : col::faint); g.fillEllipse (led);
        g.setColour (on ? col::ink : col::text);
        text (g, title.toUpperCase(), r.withTrimmedLeft (22.0f).withTrimmedRight (4.0f), juce::Justification::centredLeft, 13.5f, on, 0.04f);
    }
    void mouseDown (const juce::MouseEvent&) override { set (value() > 0.5f ? 0.0f : 1.0f); }
};

// Deslizador horizontal: rótulo a la izquierda, valor a la derecha, la pista debajo.
class HSlider : public ParamControl
{
public:
    HSlider (ThrumProcessor& p, const juce::String& id, juce::String name = {}) : ParamControl (p, id), title (name.isEmpty() && param ? param->getName (40) : name) {}
    juce::String title;
    juce::Rectangle<float> trackR() const { return { 6.0f, (float) getHeight() - 9.0f, (float) getWidth() - 12.0f, 4.0f }; }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        auto top = r.removeFromTop (18.0f);
        label (g, title, top, juce::Justification::centredLeft, 12.5f);
        g.setColour (drag ? col::amberHi : col::text); text (g, valueText(), top, juce::Justification::centredRight, 13.5f);
        auto t = trackR();
        g.setColour (col::line); g.fillRoundedRectangle (t, 2.0f);
        const float x = t.getX() + t.getWidth() * norm();
        g.setColour (col::amber); g.fillRoundedRectangle (t.withRight (x), 2.0f);
        g.setColour (col::text); g.fillEllipse (x - 6.0f, t.getCentreY() - 6.0f, 12.0f, 12.0f);
    }
    void mouseDown (const juce::MouseEvent& e) override { if (! att) return; drag = true; att->beginGesture(); mouseDrag (e); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! att) return;
        auto t = trackR();
        att->setValueAsPartOfGesture (param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, (e.position.x - t.getX()) / t.getWidth())));
    }
    void mouseUp (const juce::MouseEvent&) override { if (att) att->endGesture(); drag = false; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (param) set (param->convertFrom0to1 (param->getDefaultValue())); }
private:
    bool drag = false;
};

// Menú: la opción elegida en una caja; clic abre la lista (para las listas largas: afinaciones, patrones, IR).
class Menu : public ParamControl
{
public:
    Menu (ThrumProcessor& p, const juce::String& id, juce::String name = {}) : ParamControl (p, id), title (name) {}
    juce::String title;
    std::function<void (juce::PopupMenu&)> build;        // opcional: secciones propias (índices + 1)
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (title.isNotEmpty()) label (g, title, r.removeFromTop (17.0f), juce::Justification::centredLeft, 12.5f);
        r = r.reduced (0.5f);
        g.setColour (hover ? col::cell2 : col::bg); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (col::line); g.drawRoundedRectangle (r, 4.0f, 1.0f);
        g.setColour (col::text); text (g, param ? param->getCurrentValueAsText() : juce::String(), r.reduced (10.0f, 0.0f).withTrimmedRight (14.0f), juce::Justification::centredLeft, 15.0f);
        juce::Path tri; const float cx = r.getRight() - 13.0f, cy = r.getCentreY();
        tri.addTriangle (cx - 4.5f, cy - 2.0f, cx + 4.5f, cy - 2.0f, cx, cy + 3.5f);
        g.setColour (col::amber); g.fillPath (tri);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override
    {
        if (! param) return;
        juce::PopupMenu m;
        const int cur = juce::roundToInt (value());
        if (build) build (m);
        else { auto names = param->getAllValueStrings(); for (int i = 0; i < names.size(); ++i) m.addItem (i + 1, names[i], true, i == cur); }
        juce::Component::SafePointer<Menu> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (getWidth()), [safe] (int r) { if (safe != nullptr && r > 0) safe->set ((float) (r - 1)); });
    }
private:
    bool hover = false;
};

// Botón simple (acciones: exportar, cargar, limpiar).
class Button : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit Button (juce::String t) : title (std::move (t)) {}
    juce::String title; std::function<void()> onClick; bool lit = false;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (lit ? col::amber : hover ? col::cell2 : col::panel); g.fillRoundedRectangle (r, 4.0f);
        if (! lit) { g.setColour (col::lineHi); g.drawRoundedRectangle (r, 4.0f, 1.0f); }
        g.setColour (lit ? col::ink : col::text); text (g, title.toUpperCase(), r, juce::Justification::centred, 13.5f, lit, 0.05f);
    }
    void mouseEnter (const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { hover = false; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
private:
    bool hover = false;
};

} // namespace thrumui
