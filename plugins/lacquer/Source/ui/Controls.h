#pragma once
// Controles propios: el selector rotativo del objetivo de sonoridad (una llave de posiciones como la de un torno) y
// la fila de estilos.

#include "LookLq.h"

namespace dh::lq::ui {

// Llave rotativa de N posiciones: un clic sobre una leyenda o arrastrar la perilla.
class RotarySwitch : public juce::Component, public juce::SettableTooltipClient
{
public:
    std::function<void (int)> onChange;
    void setItems (juce::StringArray names, juce::StringArray sub) { items = std::move (names); subs = std::move (sub); repaint(); }
    void setIndex (int i) { i = juce::jlimit (0, juce::jmax (0, items.size() - 1), i); if (i != index) { index = i; repaint(); } }
    int getIndex() const { return index; }

    void paint (juce::Graphics& g) override
    {
        const auto c = centre(); const float R = knobR();
        for (int i = 0; i < items.size(); ++i)
        {
            const float a = angleOf (i);
            const bool on = i == index;
            const auto p0 = juce::Point<float> (c.x + (R + 6) * std::sin (a), c.y - (R + 6) * std::cos (a));
            const auto p1 = juce::Point<float> (c.x + (R + 14) * std::sin (a), c.y - (R + 14) * std::cos (a));
            g.setColour (on ? col::ink : col::inkFaint);
            g.drawLine ({ p0, p1 }, on ? 2.4f : 1.4f);
            const auto lr = labelRect (i);
            g.setColour (on ? col::ink : col::inkSoft);
            g.setFont (on ? bold (16.5f) : regular (16.0f));
            const bool left = std::sin (a) < -0.1f, right = std::sin (a) > 0.1f;
            const auto just = left ? juce::Justification::centredRight : right ? juce::Justification::centredLeft : juce::Justification::centred;
            g.drawText (items[i], lr.withHeight (lr.getHeight() * 0.55f), just);
            g.setColour (on ? col::inkSoft : col::inkFaint); g.setFont (regular (13.5f));
            g.drawText (subs[i], lr.withTrimmedTop (lr.getHeight() * 0.5f), just);
        }
        // Perilla tipo "pico de pato" de baquelita.
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillEllipse (c.x - R, c.y - R + 3, R * 2, R * 2);
        juce::ColourGradient body (juce::Colour (0xff3b3a3f), c.x - R * 0.5f, c.y - R, juce::Colour (0xff121114), c.x + R * 0.6f, c.y + R, true);
        g.setGradientFill (body);
        g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        const float a = angleOf (index);
        juce::Path beak;
        beak.addRoundedRectangle (-R * 0.30f, -R * 1.02f, R * 0.60f, R * 2.04f, R * 0.28f);
        g.setColour (juce::Colour (0xff27262b));
        g.fillPath (beak, juce::AffineTransform::rotation (a).translated (c.x, c.y));
        juce::Path tick; tick.addRoundedRectangle (-2.0f, -R * 0.98f, 4.0f, R * 0.5f, 2.0f);
        g.setColour (col::label);
        g.fillPath (tick, juce::AffineTransform::rotation (a).translated (c.x, c.y));
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < items.size(); ++i) if (labelRect (i).expanded (4).contains (e.position)) { choose (i); return; }
        dragStart = index; dragY = e.position.y;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragStart < 0) return;
        choose (juce::jlimit (0, items.size() - 1, dragStart + (int) std::round ((dragY - e.position.y) / 22.0f)));
    }
    void mouseUp (const juce::MouseEvent&) override { dragStart = -1; }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override { choose (juce::jlimit (0, items.size() - 1, index + (w.deltaY > 0 ? 1 : -1))); }

private:
    juce::StringArray items, subs;
    int index = 0, dragStart = -1; float dragY = 0;
    juce::Point<float> centre() const { return { getWidth() * 0.5f, getHeight() * 0.56f }; }
    float knobR() const { return std::min (getWidth() * 0.15f, getHeight() * 0.21f); }
    float angleOf (int i) const
    {
        const int n = juce::jmax (2, items.size());
        const float span = juce::MathConstants<float>::pi * 1.5f;           // 270°
        return -span * 0.5f + span * (float) i / (float) (n - 1);
    }
    juce::Rectangle<float> labelRect (int i) const
    {
        const auto c = centre(); const float R = knobR() + 22.0f, a = angleOf (i);
        const auto p = juce::Point<float> (c.x + R * std::sin (a), c.y - R * std::cos (a));
        const float w = 112.0f, h = 36.0f;
        const float s = std::sin (a);
        const float x = s < -0.1f ? p.x - w : s > 0.1f ? p.x : p.x - w * 0.5f;
        return { x, p.y - h * (std::cos (a) > 0.3f ? 0.9f : 0.5f), w, h };
    }
    void choose (int i) { if (i == index) return; index = i; repaint(); if (onChange) onChange (i); }
};

// Fila de estilos: seis teclas con nombre y una línea que dice qué hace cada una.
class StylePicker : public juce::Component
{
public:
    std::function<void (int)> onChange;
    std::function<void (const juce::String&)> onHover;
    void setIndex (int i) { if (i != index) { index = i; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        const auto& names = names_(); const auto& lines = lines_();
        for (int i = 0; i < 6; ++i)
        {
            auto r = cell (i).reduced (3.0f, 0.0f);
            const bool on = i == index, over = i == hover;
            g.setColour (juce::Colours::black.withAlpha (on ? 0.0f : 0.16f));
            g.fillRoundedRectangle (r.translated (0, 1.5f), 4.0f);
            g.setColour (on ? col::ink : (over ? col::enamelHi.brighter (0.05f) : col::enamelHi));
            g.fillRoundedRectangle (r, 4.0f);
            g.setColour (on ? juce::Colours::black.withAlpha (0.4f) : col::seam);
            g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
            // Lámpara.
            g.setColour (on ? col::amber : col::seam.withAlpha (0.7f));
            g.fillEllipse (r.getX() + 9, r.getY() + 11, 7, 7);
            g.setColour (on ? col::label : col::ink); g.setFont (bold (19.0f));
            g.drawText (names[i], r.withTrimmedLeft (22).withHeight (28).translated (0, 3), juce::Justification::centredLeft);
            g.setColour (on ? col::label.withAlpha (0.75f) : col::inkSoft); g.setFont (regular (14.0f));
            g.drawFittedText (lines[i], r.withTrimmedTop (30).reduced (9, 2).toNearestInt(), juce::Justification::topLeft, 2, 0.9f);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        int h = -1; for (int i = 0; i < 6; ++i) if (cell (i).contains (e.position)) h = i;
        if (h != hover) { hover = h; repaint(); if (onHover) onHover (h >= 0 ? tips_()[h] : juce::String()); }
    }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); if (onHover) onHover ({}); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 6; ++i) if (cell (i).contains (e.position)) { index = i; repaint(); if (onChange) onChange (i); }
    }

private:
    int index = 0, hover = -1;
    juce::Rectangle<float> cell (int i) const { const float w = getWidth() / 6.0f; return { w * i, 0.0f, w, (float) getHeight() }; }
    static const juce::StringArray& names_() { static juce::StringArray a { "Balanced", "Warm", "Bright", "Punchy", "Loud", "Gentle" }; return a; }
    static const juce::StringArray& lines_()
    {
        static juce::StringArray a { "Even and neutral", "Rounder lows, softer top", "More air, less mud",
                                     "Kick and snare up front", "Dense, for loud targets", "Light touch, keeps dynamics" };
        return a;
    }
    static const juce::StringArray& tips_()
    {
        static juce::StringArray a {
            "Balanced: moves the tone toward an even, neutral balance and glues gently. The default.",
            "Warm: a little more low-mid body, a softer top end and light tape saturation.",
            "Bright: more air above 10 kHz, less mud around 300 Hz, slightly wider highs.",
            "Punchy: slower glue attack so transients pass, a touch of valve saturation and a soft clipper.",
            "Loud: more compression and clipping, made for loud targets (-11 LUFS and up).",
            "Gentle: half the correction, no glue at low intensity: for mixes that are nearly there." };
        return a;
    }
};

} // namespace dh::lq::ui
