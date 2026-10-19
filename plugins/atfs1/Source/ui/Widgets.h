#pragma once
// Controles del editor: perillas, elecciones y llaves para los parámetros del host; campos numéricos
// para la voz (los 8+8 operadores no son parámetros del host: viven en el estado y se editan acá).
// Todo control avisa al pasar el mouse (línea de ayuda) y al cambiar (renglón 2 del LCD del frente).

#include <JuceHeader.h>
#include "LookFs.h"
#include "../PluginProcessor.h"

namespace dh::fs::ui {

// Avisos globales del editor (un solo editor abierto por instancia; se reenganchan al abrir otro).
struct Bus
{
    std::function<void (const juce::String&)> help;                       // línea de ayuda
    std::function<void (const juce::String&, const juce::String&)> lcd;   // nombre, valor → LCD del frente
    static Bus& get() { static Bus b; return b; }
};

inline void showHelp (const juce::String& s) { if (Bus::get().help) Bus::get().help (s); }
inline void showLcd (const juce::String& n, const juce::String& v) { if (Bus::get().lcd) Bus::get().lcd (n, v); }

// ───────── perilla de un parámetro del host ─────────
class ParamKnob : public juce::Component, private juce::Timer
{
public:
    ParamKnob (ATFS1Processor& p, const juce::String& id, const juce::String& label, const juce::String& helpText = {})
        : proc (p), paramId (id), text (label), help (helpText)
    {
        param = p.apvts.getParameter (id);
        if (param != nullptr) attach = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        if (attach) attach->sendInitialUpdate();
        setRepaintsOnMouseActivity (true);
        startTimerHz (15);
    }
    void paint (juce::Graphics& g) override
    {
        if (param == nullptr) return;
        const auto b = getLocalBounds().toFloat();
        const float d = std::min (b.getWidth(), b.getHeight() - 30.0f);
        const auto c = juce::Rectangle<float> (d, d).withCentre ({ b.getCentreX(), 2.0f + d / 2 });
        const float v = param->getValue();
        const float a0 = -2.35f, a1 = 2.35f;
        const bool bip = proc.apvts.getParameterRange (paramId).start < 0.0f;
        const float ang = a0 + (a1 - a0) * v;
        // Pista y arco.
        juce::Path track; track.addCentredArc (c.getCentreX(), c.getCentreY(), d / 2 - 2, d / 2 - 2, 0, a0, a1, true);
        g.setColour (col::border); g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path arc; arc.addCentredArc (c.getCentreX(), c.getCentreY(), d / 2 - 2, d / 2 - 2, 0, bip ? 0.0f : a0, ang, true);
        g.setColour (col::green); g.strokePath (arc, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        // Cuerpo.
        const auto k = c.reduced (d * 0.2f);
        juce::ColourGradient gr (juce::Colour (0xff3a4552), k.getX(), k.getY(), juce::Colour (0xff161c23), k.getRight(), k.getBottom(), false);
        g.setGradientFill (gr); g.fillEllipse (k);
        g.setColour (isMouseOverOrDragging() ? col::dim : col::border); g.drawEllipse (k, 1.0f);
        const auto tip = k.getCentre().getPointOnCircumference (k.getWidth() * 0.36f, ang);
        g.setColour (col::text); g.drawLine (juce::Line<float> (k.getCentre().getPointOnCircumference (k.getWidth() * 0.12f, ang), tip), 2.0f);
        // Rótulo y valor.
        g.setFont (font (12.0f));
        g.setColour (col::dim);
        g.drawText (text, b.withTop (b.getBottom() - 28).withHeight (14).toNearestInt(), juce::Justification::centred);
        g.setColour (isMouseOverOrDragging() ? col::green : col::text);
        g.drawText (param->getCurrentValueAsText(), b.withTop (b.getBottom() - 14).toNearestInt(), juce::Justification::centred);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (param == nullptr) return;
        if (e.mods.isPopupMenu()) { menu(); return; }
        startValue = param->getValue();
        attach->beginGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (param == nullptr || e.mods.isPopupMenu()) return;
        const float sens = e.mods.isShiftDown() ? 1200.0f : 240.0f;
        const float nv = juce::jlimit (0.0f, 1.0f, startValue - (float) e.getDistanceFromDragStartY() / sens + (float) e.getDistanceFromDragStartX() / (sens * 2.0f));
        attach->setValueAsPartOfGesture (param->convertFrom0to1 (nv));
        showLcd (text, param->getCurrentValueAsText());
    }
    void mouseUp (const juce::MouseEvent& e) override { if (param != nullptr && ! e.mods.isPopupMenu()) attach->endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (param != nullptr) attach->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue())); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (param == nullptr) return;
        const int steps = param->getNumSteps();
        const float step = steps > 1 && steps < 400 ? 1.0f / (float) (steps - 1) : 0.01f;
        const float nv = juce::jlimit (0.0f, 1.0f, param->getValue() + (w.deltaY > 0 ? step : -step) * (w.isReversed ? -1.0f : 1.0f));
        attach->setValueAsCompleteGesture (param->convertFrom0to1 (nv));
        showLcd (text, param->getCurrentValueAsText());
    }
    void mouseEnter (const juce::MouseEvent&) override { showHelp (text + (help.isNotEmpty() ? ": " + help : juce::String()) + "  (drag; double-click resets)"); }
    void mouseExit (const juce::MouseEvent&) override { showHelp ({}); }

private:
    ATFS1Processor& proc;
    juce::String paramId, text, help;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attach;
    float startValue = 0.0f, last = -1.0f;
    void timerCallback() override { if (param != nullptr && std::abs (param->getValue() - last) > 1e-5f) { last = param->getValue(); repaint(); } }
    void menu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Reset to default");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r) {
            if (r == 1) attach->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue()));
        });
    }
};

// ───────── elección / entero de un parámetro del host: caja que se arrastra o abre un menú ─────────
class ParamBox : public juce::Component, private juce::Timer
{
public:
    ParamBox (ATFS1Processor& p, const juce::String& id, const juce::String& label, const juce::String& helpText = {})
        : text (label), help (helpText)
    {
        param = p.apvts.getParameter (id);
        if (param != nullptr) attach = std::make_unique<juce::ParameterAttachment> (*param, [this] (float) { repaint(); });
        if (attach) attach->sendInitialUpdate();
        choice = dynamic_cast<juce::AudioParameterChoice*> (param) != nullptr;
        setRepaintsOnMouseActivity (true);
        startTimerHz (10);
    }
    void paint (juce::Graphics& g) override
    {
        if (param == nullptr) return;
        auto b = getLocalBounds();
        g.setFont (font (12.0f)); g.setColour (col::dim);
        g.drawText (text, b.removeFromTop (16), juce::Justification::centredLeft);
        const auto r = b.reduced (0, 1).toFloat();
        g.setColour (juce::Colour (0xff0d1116)); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (isMouseOverOrDragging() ? col::dim : col::border); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (isToggle() ? (param->getValue() > 0.5f ? col::green : col::dim) : col::green);
        g.setFont (font (14.0f, true));
        g.drawText (param->getCurrentValueAsText(), r.reduced (8, 0).toNearestInt(), juce::Justification::centredLeft);
        if (choice && ! isToggle())
        {
            juce::Path tri; const float cx = r.getRight() - 12.0f, cy = r.getCentreY();
            tri.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
            g.setColour (col::dim); g.fillPath (tri);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (param == nullptr) return;
        if (isToggle()) { attach->setValueAsCompleteGesture (param->getValue() > 0.5f ? 0.0f : 1.0f); showLcd (text, param->getCurrentValueAsText()); return; }
        if (choice)
        {
            juce::PopupMenu m;
            const auto* c = dynamic_cast<juce::AudioParameterChoice*> (param);
            for (int i = 0; i < c->choices.size(); ++i) m.addItem (i + 1, c->choices[i], true, i == c->getIndex());
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r) {
                if (r > 0) { attach->setValueAsCompleteGesture ((float) (r - 1)); showLcd (text, param->getCurrentValueAsText()); }
            });
            return;
        }
        juce::ignoreUnused (e);
        startValue = param->getValue();
        attach->beginGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (param == nullptr || choice) return;
        const int steps = std::max (2, param->getNumSteps());
        const float px = e.mods.isShiftDown() ? 16.0f : 4.0f;
        const float nv = juce::jlimit (0.0f, 1.0f, startValue - (float) e.getDistanceFromDragStartY() / px / (float) (steps - 1));
        attach->setValueAsPartOfGesture (param->convertFrom0to1 (nv));
        showLcd (text, param->getCurrentValueAsText());
    }
    void mouseUp (const juce::MouseEvent&) override { if (param != nullptr && ! choice && ! isToggle()) attach->endGesture(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (param != nullptr && ! choice) attach->setValueAsCompleteGesture (param->convertFrom0to1 (param->getDefaultValue())); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (param == nullptr) return;
        const int steps = std::max (2, param->getNumSteps());
        const float nv = juce::jlimit (0.0f, 1.0f, param->getValue() + (w.deltaY > 0 ? 1.0f : -1.0f) / (float) (steps - 1));
        attach->setValueAsCompleteGesture (param->convertFrom0to1 (nv));
        showLcd (text, param->getCurrentValueAsText());
    }
    void mouseEnter (const juce::MouseEvent&) override { showHelp (text + (help.isNotEmpty() ? ": " + help : juce::String())); }
    void mouseExit (const juce::MouseEvent&) override { showHelp ({}); }

private:
    juce::String text, help;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::ParameterAttachment> attach;
    bool choice = false;
    float startValue = 0.0f, last = -1.0f;
    bool isToggle() const { auto* c = dynamic_cast<juce::AudioParameterChoice*> (param); return c != nullptr && c->choices.size() == 2; }
    void timerCallback() override { if (param != nullptr && std::abs (param->getValue() - last) > 1e-5f) { last = param->getValue(); repaint(); } }
};

// ───────── campo de la voz ─────────
struct VField
{
    juce::String label;
    int lo = 0, hi = 99, def = 0;
    std::function<int (const VoiceData&)> get;
    std::function<void (VoiceData&, int)> set;
    juce::StringArray names;                         // si no está vacío, el valor es un índice
    std::function<juce::String (int, const VoiceData&)> fmt;
    juce::String help;
    std::function<bool (const VoiceData&)> enabled;  // lo que la forma o el modo no leen se apaga
};

class NumField : public juce::Component
{
public:
    NumField (ATFS1Processor& p, VField f, bool compactLayout = false) : proc (p), field (std::move (f)), compact (compactLayout)
    {
        setRepaintsOnMouseActivity (true);
        setWantsKeyboardFocus (false);
    }
    void setField (VField f) { field = std::move (f); repaint(); }
    const VField& getField() const { return field; }
    bool isActive() const { return ! field.enabled || field.enabled (proc.voice()); }
    juce::String valueText() const
    {
        const auto& v = proc.voice();
        const int x = field.get (v);
        if (field.fmt) return field.fmt (x, v);
        if (! field.names.isEmpty()) return field.names[juce::jlimit (0, field.names.size() - 1, x - field.lo)];
        return (field.lo < 0 && x > 0 ? "+" : "") + juce::String (x);
    }
    void step (int d) { apply (field.get (proc.voice()) + d); }
    bool highlighted = false;

    void paint (juce::Graphics& g) override
    {
        const bool on = isActive();
        auto b = getLocalBounds();
        juce::Rectangle<int> lab, box;
        if (compact) { box = b; }
        else { lab = b.removeFromTop (15); box = b; }
        if (! compact)
        {
            g.setFont (font (11.5f)); g.setColour (on ? col::dim : col::faint);
            g.drawText (field.label, lab, juce::Justification::centredLeft);
        }
        const auto r = box.toFloat().reduced (0.5f, 1.0f);
        g.setColour (juce::Colour (0xff0d1116).withAlpha (on ? 1.0f : 0.5f)); g.fillRoundedRectangle (r, 3.5f);
        g.setColour (highlighted ? col::green : (isMouseOverOrDragging() && on ? col::dim : col::border));
        g.drawRoundedRectangle (r, 3.5f, highlighted ? 1.5f : 1.0f);
        g.setColour (on ? col::green : col::faint);
        g.setFont (font (compact ? 13.0f : 14.0f, true));
        g.drawText (on ? valueText() : juce::String ("--"), r.reduced (6, 0).toNearestInt(), compact ? juce::Justification::centred : juce::Justification::centredLeft);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! isActive()) return;
        if (e.mods.isPopupMenu() || (! field.names.isEmpty() && field.names.size() <= 24 && ! e.mods.isAltDown()))
        {
            if (! field.names.isEmpty()) { namesMenu(); return; }
            juce::PopupMenu m; m.addItem (1, "Reset (" + juce::String (field.def) + ")");
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r) { if (r == 1) apply (field.def); });
            return;
        }
        startValue = field.get (proc.voice());
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! isActive() || e.mods.isPopupMenu() || (! field.names.isEmpty() && field.names.size() <= 24)) return;
        const float span = (float) (field.hi - field.lo);
        const float px = e.mods.isShiftDown() ? 10.0f : std::max (1.5f, 300.0f / std::max (1.0f, span));
        apply (startValue - (int) std::lround ((float) e.getDistanceFromDragStartY() / px));
    }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (! isActive() || ! field.names.isEmpty()) return;
        auto* ed = new juce::TextEditor();
        ed->setText (juce::String (field.get (proc.voice())), false);
        ed->setJustification (juce::Justification::centred);
        ed->setBounds (getLocalBounds().removeFromBottom (compact ? getHeight() : getHeight() - 15));
        ed->onReturnKey = [this, ed] { apply (ed->getText().getIntValue()); removeEditor(); };
        ed->onEscapeKey = [this] { removeEditor(); };
        ed->onFocusLost = [this, ed] { apply (ed->getText().getIntValue()); removeEditor(); };
        editor.reset (ed);
        addAndMakeVisible (ed);
        ed->grabKeyboardFocus(); ed->selectAll();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (! isActive()) return;
        wheelAcc += w.deltaY * (w.isReversed ? -1.0f : 1.0f) * 8.0f;
        while (wheelAcc >= 1.0f) { step (1); wheelAcc -= 1.0f; }
        while (wheelAcc <= -1.0f) { step (-1); wheelAcc += 1.0f; }
    }
    void mouseEnter (const juce::MouseEvent&) override { showHelp (field.label + (field.help.isNotEmpty() ? ": " + field.help : juce::String()) + (isActive() ? juce::String() : "  (not read by the current form or mode)")); }
    void mouseExit (const juce::MouseEvent&) override { showHelp ({}); }

private:
    ATFS1Processor& proc;
    VField field;
    bool compact;
    int startValue = 0;
    float wheelAcc = 0.0f;
    std::unique_ptr<juce::TextEditor> editor;
    void removeEditor() { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<NumField> (this)] { if (safe != nullptr) safe->editor.reset(); }); }
    void apply (int v)
    {
        v = juce::jlimit (field.lo, field.hi, v);
        if (v == field.get (proc.voice())) return;
        proc.editVoiceWith ([&] (VoiceData& d) { field.set (d, v); });
        showLcd (field.label, valueText());
        repaint();
    }
    void namesMenu()
    {
        juce::PopupMenu m;
        const int cur = field.get (proc.voice());
        for (int i = 0; i < field.names.size(); ++i) m.addItem (i + 1, field.names[i], true, field.lo + i == cur);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r) { if (r > 0) apply (field.lo + r - 1); });
    }
};

// Grilla simple de controles: los coloca en filas de `cols` columnas.
inline void gridLayout (juce::Rectangle<int> r, const std::vector<juce::Component*>& items, int cols, int rowH, int gap = 8)
{
    if (items.empty()) return;
    const int w = (r.getWidth() - gap * (cols - 1)) / cols;
    for (size_t i = 0; i < items.size(); ++i)
    {
        const int c = (int) i % cols, row = (int) i / cols;
        if (items[i] != nullptr) items[i]->setBounds (r.getX() + c * (w + gap), r.getY() + row * (rowH + gap), w, rowH);
    }
}

} // namespace dh::fs::ui
