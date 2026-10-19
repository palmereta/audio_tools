#pragma once
// Estilo de Tarnish, "óxido y crema": una grabadora vista desde arriba sobre una mesa de trabajo. Marrón óxido de
// fondo, letras crema y un solo color de acento (verde cardenillo); rojo sólo para la sobrecarga. Tipografía IBM
// Plex Sans Condensed (OFL), con el tono de un manual de servicio.

#include <JuceHeader.h>

namespace dh::tn::ui {

namespace col {
    const juce::Colour bg (0xff241912), deck (0xff33241c), deck2 (0xff2b1f18), panel (0xff2e211a), panel2 (0xff3a2a21);
    const juce::Colour line (0xff4f3b2f), lineHi (0xff6a5242);
    const juce::Colour cream (0xffefe6d2), creamDim (0xffc2b59d), creamFaint (0xff8d7f6a);
    const juce::Colour teal (0xff3e8c84), tealHi (0xff6cbab0), red (0xffd2452f), amber (0xffe0a24a);
    const juce::Colour tape (0xff1d1511), tapeSheen (0xff4a3a30), metal (0xffb7b1a6), metalDark (0xff6f6a62);
}

inline juce::Typeface::Ptr typeface (bool bold)
{
    static juce::Typeface::Ptr reg = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexSansCondensedMedium_ttf, BinaryData::IBMPlexSansCondensedMedium_ttfSize);
    static juce::Typeface::Ptr bld = juce::Typeface::createSystemTypefaceFor (BinaryData::IBMPlexSansCondensedSemiBold_ttf, BinaryData::IBMPlexSansCondensedSemiBold_ttfSize);
    return bold ? bld : reg;
}
inline juce::Font regular (float h) { return juce::Font (juce::FontOptions (typeface (false)).withHeight (h)); }
inline juce::Font bold (float h) { return juce::Font (juce::FontOptions (typeface (true)).withHeight (h)); }

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::panel2);
        setColour (juce::PopupMenu::textColourId, col::cream);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::teal);
        setColour (juce::PopupMenu::highlightedTextColourId, col::cream);
        setColour (juce::PopupMenu::headerTextColourId, col::creamDim);
        setColour (juce::ComboBox::backgroundColourId, col::panel2);
        setColour (juce::ComboBox::textColourId, col::cream);
        setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::ComboBox::arrowColourId, col::creamDim);
        setColour (juce::TooltipWindow::backgroundColourId, col::cream);
        setColour (juce::TooltipWindow::textColourId, col::bg);
        setColour (juce::TextButton::buttonColourId, col::panel2);
        setColour (juce::TextButton::buttonOnColourId, col::cream);
        setColour (juce::TextButton::textColourOffId, col::cream);
        setColour (juce::TextButton::textColourOnId, col::bg);
    }
    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override { return typeface (false); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return bold (juce::jmin (15.0f, (float) h * 0.48f)); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return regular (15.0f); }
    juce::Font getPopupMenuFont() override { return regular (15.0f); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        const bool stop = b.getProperties().contains ("stop");
        juce::Colour fill = on ? (stop ? col::red : col::cream) : col::panel2;
        if (over && ! on) fill = fill.brighter (0.08f);
        if (down) fill = fill.darker (0.1f);
        g.setColour (fill); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? fill.darker (0.25f) : col::line); g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState(), stop = b.getProperties().contains ("stop");
        g.setColour (on ? (stop ? col::cream : col::bg) : col::cream);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (4, 0), juce::Justification::centred, 1, 0.85f);
    }
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::panel2); g.fillRoundedRectangle (r, 4.0f);
        g.setColour (col::line); g.drawRoundedRectangle (r, 4.0f, 1.0f);
        juce::Path p; const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
        p.addTriangle (cx - 4, cy - 2, cx + 4, cy - 2, cx, cy + 3);
        g.setColour (col::creamDim); g.fillPath (p);
    }
    void positionComboBoxText (juce::ComboBox& b, juce::Label& l) override { l.setBounds (8, 1, b.getWidth() - 26, b.getHeight() - 2); l.setFont (getComboBoxFont (b)); }

    // Perilla de baquelita crema sobre un arco de escala; el arco de valor en cardenillo (centrado si es bipolar).
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (3.0f);
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f, cx = r.getCentreX(), cy = r.getCentreY();
        const float ang = a0 + pos * (a1 - a0);
        const bool bip = s.getProperties().contains ("bipolar");
        juce::Path track; track.addCentredArc (cx, cy, rad - 2, rad - 2, 0, a0, a1, true);
        g.setColour (col::line); g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        juce::Path val; const float from = bip ? (a0 + a1) * 0.5f : a0;
        val.addCentredArc (cx, cy, rad - 2, rad - 2, 0, juce::jmin (from, ang), juce::jmax (from, ang), true);
        g.setColour (col::teal); g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float kr = rad - 9;
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillEllipse (cx - kr + 1.5f, cy - kr + 3.0f, kr * 2, kr * 2);
        juce::ColourGradient cg (col::cream, cx - kr * 0.4f, cy - kr * 0.6f, col::creamDim.darker (0.15f), cx + kr * 0.5f, cy + kr, true);
        g.setGradientFill (cg); g.fillEllipse (cx - kr, cy - kr, kr * 2, kr * 2);
        g.setColour (col::creamDim.darker (0.4f)); g.drawEllipse (cx - kr, cy - kr, kr * 2, kr * 2, 1.0f);
        // Estrías sutiles del borde.
        g.setColour (col::creamDim.darker (0.25f).withAlpha (0.5f));
        for (int i = 0; i < 24; ++i)
        {
            const float a = juce::MathConstants<float>::twoPi * (float) i / 24.0f;
            g.drawLine (cx + std::sin (a) * (kr - 3), cy - std::cos (a) * (kr - 3), cx + std::sin (a) * kr, cy - std::cos (a) * kr, 1.0f);
        }
        g.setColour (col::bg);
        g.drawLine (cx + std::sin (ang) * kr * 0.25f, cy - std::cos (ang) * kr * 0.25f, cx + std::sin (ang) * (kr - 4), cy - std::cos (ang) * (kr - 4), 3.0f);
    }
};

} // namespace dh::tn::ui
