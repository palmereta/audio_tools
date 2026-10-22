#pragma once
// Estilo de Pallor: aluminio negro, cerámica color hueso y una sola luz rosa fría. La pantalla es vidrio oscuro: todo
// lo que se dibuja en ella va en tinta hueso, con el rosa para lo que está activo o sonando y un azul acero para lo
// secundario (probabilidades, condiciones). Barlow Condensed (OFL) para todo el texto.
// Los controles de pantalla (perilla, chips, interruptor) se atan a un parámetro con juce::ParameterAttachment: el
// gesto llega al host (automatización, deshacer) y el control se repinta solo cuando el valor cambia.

#include <JuceHeader.h>

namespace dh::pal::ui {

namespace col {
    const juce::Colour glass (0xff0b0c0e), glass2 (0xff121317), cell (0xff1b1c21), cell2 (0xff22232a), line (0xff2c2d34), lineHi (0xff3d3e47);
    const juce::Colour bone (0xffe2dacb), boneDim (0xff9d968a), boneFaint (0xff5d5952), ink (0xff121212);
    const juce::Colour rose (0xffe0709c), roseHi (0xffff9cc0), roseDim (0xff6b2e46), steel (0xff7fa6c6), steelDim (0xff3a4c5c);
    const juce::Colour amber (0xffe7b45a), red (0xffe0503c);
    const juce::Colour silk (0xffd8d1c3), silkDim (0xff86817a), padInk (0xff1d1b19);
}

inline juce::Typeface::Ptr typeface (bool semi)
{
    static juce::Typeface::Ptr med = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedMedium_ttf, BinaryData::BarlowCondensedMedium_ttfSize);
    static juce::Typeface::Ptr sb = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    return semi ? sb : med;
}
inline juce::Font font (float h, bool semi = false) { return juce::Font (juce::FontOptions (typeface (semi)).withHeight (h)); }
inline juce::Font tracked (float h, float track, bool semi = false) { auto f = font (h, semi); f.setExtraKerningFactor (track); return f; }

// Texto con espaciado (kerning extra) donde haga falta, centrado en un rectángulo.
inline void text (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Justification j, float h, bool semi = false, float track = 0.0f)
{
    g.setFont (track != 0.0f ? tracked (h, track, semi) : font (h, semi));
    g.drawText (s, r, j, false);
}

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::cell);
        setColour (juce::PopupMenu::textColourId, col::bone);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::rose);
        setColour (juce::PopupMenu::highlightedTextColourId, col::ink);
        setColour (juce::PopupMenu::headerTextColourId, col::boneDim);
        setColour (juce::TooltipWindow::backgroundColourId, col::bone);
        setColour (juce::TooltipWindow::textColourId, col::ink);
        setColour (juce::TooltipWindow::outlineColourId, col::bone);
        setColour (juce::AlertWindow::backgroundColourId, col::cell);
        setColour (juce::AlertWindow::textColourId, col::bone);
        setColour (juce::AlertWindow::outlineColourId, col::line);
        setColour (juce::TextEditor::backgroundColourId, col::glass);
        setColour (juce::TextEditor::textColourId, col::bone);
        setColour (juce::TextEditor::outlineColourId, col::line);
        setColour (juce::TextEditor::focusedOutlineColourId, col::rose);
        setColour (juce::TextEditor::highlightColourId, col::roseDim);
        setColour (juce::TextButton::buttonColourId, col::cell2);
        setColour (juce::TextButton::textColourOffId, col::bone);
        setColour (juce::ScrollBar::thumbColourId, col::lineHi);
        setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    }
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override { return typeface (false); }
    juce::Font getPopupMenuFont() override { return font (16.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (16.0f, (float) h * 0.55f), true); }
    juce::Font getAlertWindowMessageFont() override { return font (16.0f); }
    juce::Font getAlertWindowTitleFont() override { return font (19.0f, true); }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour ((down ? col::rose.darker (0.3f) : (over ? col::lineHi : col::cell2))); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (col::lineHi); g.drawRoundedRectangle (r, 3.0f, 1.0f);
    }
};

// ── Un control de pantalla atado a un parámetro (o a una función, si no hay parámetro) ──
class Bound : public juce::Component, public juce::SettableTooltipClient
{
public:
    void bind (juce::RangedAudioParameter* p)
    {
        att.reset(); param = p;
        if (p != nullptr) { att = std::make_unique<juce::ParameterAttachment> (*p, [this] (float) { repaint(); }); att->sendInitialUpdate(); }
        repaint();
    }
    juce::RangedAudioParameter* param = nullptr;
    std::function<void (const juce::String&)> onHover;          // texto para el display mientras se mueve
protected:
    std::unique_ptr<juce::ParameterAttachment> att;
    float norm() const { return param != nullptr ? param->getValue() : 0.0f; }
    float real() const { return param != nullptr ? param->convertFrom0to1 (param->getValue()) : 0.0f; }
    juce::String valueText() const { return param != nullptr ? param->getCurrentValueAsText() : juce::String(); }
    void setNorm (float v, bool gesture)
    {
        if (param == nullptr || att == nullptr) return;
        const float r = param->convertFrom0to1 (juce::jlimit (0.0f, 1.0f, v));
        if (gesture) att->setValueAsPartOfGesture (r); else att->setValueAsCompleteGesture (r);
        if (onHover) onHover (param->getName (40) + "  " + valueText());
    }
};

// Perilla de pantalla: anillo con el arco del valor en rosa, el punto, el nombre arriba y el valor abajo.
class Knob : public Bound
{
public:
    juce::String label;
    bool bipolar = false;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::boneDim); text (g, label.toUpperCase(), r.removeFromTop (15.0f), juce::Justification::centred, 13.5f, false, 0.06f);
        auto vt = r.removeFromBottom (15.0f);
        const float d = std::min (r.getWidth(), r.getHeight()) - 6.0f;
        auto c = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
        const float a0 = juce::MathConstants<float>::pi * 1.25f, a1 = juce::MathConstants<float>::pi * 2.75f, v = norm();
        const float ang = a0 + v * (a1 - a0), mid = a0 + 0.5f * (a1 - a0);
        juce::Path tr; tr.addCentredArc (c.getCentreX(), c.getCentreY(), d * 0.5f, d * 0.5f, 0, a0, a1, true);
        g.setColour (col::line); g.strokePath (tr, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path va; va.addCentredArc (c.getCentreX(), c.getCentreY(), d * 0.5f, d * 0.5f, 0, bipolar ? mid : a0, ang, true);
        g.setColour (active ? col::roseHi : col::rose); g.strokePath (va, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (col::cell2); g.fillEllipse (c.reduced (d * 0.2f));
        g.setColour (col::bone);
        const float rr = d * 0.3f;
        g.drawLine (c.getCentreX() + std::sin (ang) * rr * 0.25f, c.getCentreY() - std::cos (ang) * rr * 0.25f,
                    c.getCentreX() + std::sin (ang) * rr, c.getCentreY() - std::cos (ang) * rr, 2.0f);
        g.setColour (active ? col::bone : col::boneDim); text (g, valueText(), vt, juce::Justification::centred, 13.5f);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        if (param == nullptr) return;
        if (e.mods.isPopupMenu()) return;
        att->beginGesture(); start = norm(); active = true; repaint();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (param == nullptr || e.mods.isPopupMenu()) return;
        const float px = e.mods.isShiftDown() ? 900.0f : 220.0f;
        setNorm (start + ((float) e.getDistanceFromDragStartX() - (float) e.getDistanceFromDragStartY()) / px, true);
    }
    void mouseUp (const juce::MouseEvent& e) override { if (param != nullptr && ! e.mods.isPopupMenu()) att->endGesture(); active = false; repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override { if (param != nullptr) setNorm (param->getDefaultValue(), false); }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { if (param != nullptr) setNorm (norm() + w.deltaY * 0.06f, false); }
private:
    float start = 0; bool active = false;
};

// Chips: una fila (o grilla) de opciones; la elegida va llena. Atada a un parámetro de elección o a funciones.
class Chips : public Bound
{
public:
    juce::StringArray items;
    int columns = 0;                                   // 0 = todas en una fila
    float fontH = 14.5f;
    std::function<int()> getIndex;                     // sin parámetro: de dónde sale la elegida
    std::function<void (int)> onPick;                  // sin parámetro: qué hacer al elegir
    juce::Colour onColour = col::bone;
    int selected() const
    {
        if (getIndex) return getIndex();
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (param)) return c->getIndex();
        if (dynamic_cast<juce::AudioParameterBool*> (param)) return norm() > 0.5f ? 1 : 0;
        return -1;
    }
    juce::Rectangle<float> cellRect (int i) const
    {
        const int n = std::max (1, items.size()), cols = columns > 0 ? columns : n, rows = (n + cols - 1) / cols;
        const float w = (float) getWidth() / (float) cols, h = (float) getHeight() / (float) rows;
        return { w * (float) (i % cols), h * (float) (i / cols), w, h };
    }
    void paint (juce::Graphics& g) override
    {
        const int sel = selected();
        for (int i = 0; i < items.size(); ++i)
        {
            auto r = cellRect (i).reduced (2.0f, 2.0f);
            const bool on = i == sel;
            g.setColour (on ? onColour : (i == hover ? col::cell2.brighter (0.06f) : col::cell));
            g.fillRoundedRectangle (r, 3.0f);
            if (! on) { g.setColour (col::line); g.drawRoundedRectangle (r, 3.0f, 1.0f); }
            g.setColour (on ? col::ink : col::bone);
            g.setFont (font (fontH, on));
            g.drawFittedText (items[i], r.reduced (3.0f, 0).toNearestInt(), juce::Justification::centred, 1, 0.8f);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = hit (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int i = hit (e.position);
        if (i < 0) return;
        if (onPick) { onPick (i); repaint(); return; }
        if (param == nullptr) return;
        if (dynamic_cast<juce::AudioParameterBool*> (param)) { setNorm (i == 1 ? 1.0f : 0.0f, false); return; }
        if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (param)) setNorm (c->convertTo0to1 ((float) i), false);
    }
private:
    int hover = -1;
    int hit (juce::Point<float> p) const { for (int i = 0; i < items.size(); ++i) if (cellRect (i).contains (p)) return i; return -1; }
};

// Interruptor: una lámpara y su nombre (parámetro booleano o función).
class Toggle : public Bound
{
public:
    juce::String label;
    std::function<bool()> getState; std::function<void (bool)> setState;
    bool state() const { return getState ? getState() : norm() > 0.5f; }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        const bool on = state();
        g.setColour (on ? col::roseDim.withAlpha (0.55f) : col::cell); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (on ? col::rose : col::line); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        auto lamp = r.removeFromLeft (r.getHeight()).reduced (r.getHeight() * 0.32f);
        g.setColour (on ? col::roseHi : col::boneFaint); g.fillEllipse (lamp);
        g.setColour (on ? col::bone : col::boneDim); text (g, label, r.withTrimmedRight (4), juce::Justification::centredLeft, 14.5f, on);
    }
    void mouseDown (const juce::MouseEvent&) override
    {
        if (setState) { setState (! state()); repaint(); return; }
        setNorm (state() ? 0.0f : 1.0f, false);
    }
};

// Botón de pantalla (acción), con texto.
class Action : public juce::Component, public juce::SettableTooltipClient
{
public:
    juce::String label; std::function<void()> onClick; bool emphasis = false; float fontH = 14.5f;
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (down ? col::rose : (over ? col::cell2.brighter (0.08f) : col::cell2)); g.fillRoundedRectangle (r, 3.0f);
        g.setColour (emphasis ? col::rose : col::lineHi); g.drawRoundedRectangle (r, 3.0f, 1.0f);
        g.setColour (down ? col::ink : col::bone); g.setFont (font (fontH, true));
        g.drawFittedText (label, r.reduced (4, 0).toNearestInt(), juce::Justification::centred, 1, 0.8f);
    }
    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
    void mouseDown (const juce::MouseEvent&) override { down = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override { down = false; repaint(); if (getLocalBounds().contains (e.getPosition()) && onClick) onClick(); }
private:
    bool over = false, down = false;
};

// Rótulo de sección: texto chico con espaciado y una regla.
inline void section (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t)
{
    g.setColour (col::boneDim);
    g.setFont (tracked (13.0f, 0.16f, true));
    const float w = juce::GlyphArrangement::getStringWidth (g.getCurrentFont(), t.toUpperCase());
    g.drawText (t.toUpperCase(), r, juce::Justification::centredLeft, false);
    g.setColour (col::line);
    g.drawHorizontalLine (juce::roundToInt (r.getCentreY()), r.getX() + w + 10.0f, r.getRight());
}

} // namespace dh::pal::ui
