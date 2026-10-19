#pragma once
// Aspecto de Lacquer: el panel esmaltado de un torno de corte, el disco de laca, lectores ámbar y una única luz
// roja de grabación. Barlow Condensed (OFL, assets/fonts) para todo el texto.

#include <JuceHeader.h>

namespace dh::lq::ui {

namespace col {
    const juce::Colour enamel (0xffbdbfb2), enamelHi (0xffcfd1c5), enamelDeep (0xffa7aa9b), seam (0xff8f9284);
    const juce::Colour ink (0xff24261f), inkSoft (0xff55594d), inkFaint (0xff7d8174);
    const juce::Colour lacquer (0xff17151a), sheen (0xff4a4552);
    const juce::Colour amber (0xffeaa43c), amberDim (0xff6f5428), display (0xff1d1e1a);
    const juce::Colour cut (0xffc8453a), sage (0xff7f8c6c), label (0xffe9e3d2);
}

class Fonts
{
public:
    static Fonts& get() { static Fonts f; return f; }
    juce::Font regular (float h) const { return make (medium, h); }
    juce::Font bold (float h) const { return make (semi, h); }
private:
    Fonts()
    {
       #if LACQUER_HAS_ASSETS
        medium = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedMedium_ttf, BinaryData::BarlowCondensedMedium_ttfSize);
        semi = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
       #endif
    }
    static juce::Font make (const juce::Typeface::Ptr& t, float h)
    {
        if (t != nullptr) return juce::Font (juce::FontOptions (t).withHeight (h));
        return juce::Font (juce::FontOptions ("Arial Narrow", h, juce::Font::plain));
    }
    juce::Typeface::Ptr medium, semi;
};
inline juce::Font regular (float h) { return Fonts::get().regular (h); }
inline juce::Font bold (float h) { return Fonts::get().bold (h); }

// Placa esmaltada con un borde de chapa (para agrupar controles).
inline void plate (juce::Graphics& g, juce::Rectangle<float> r, float radius = 6.0f, bool recessed = false)
{
    g.setColour (recessed ? col::enamelDeep : col::enamel);
    g.fillRoundedRectangle (r, radius);
    g.setColour (recessed ? col::seam.withAlpha (0.9f) : col::enamelHi);
    g.drawRoundedRectangle (r.reduced (0.5f), radius, 1.0f);
}
// Ventana oscura de lectores.
inline void displayWell (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (col::display);
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, 1.0f);
    g.setColour (col::enamelHi.withAlpha (0.6f));
    g.drawLine (r.getX() + 5, r.getBottom() + 0.5f, r.getRight() - 5, r.getBottom() + 0.5f, 1.0f);
}

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::enamelHi);
        setColour (juce::PopupMenu::textColourId, col::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::ink);
        setColour (juce::PopupMenu::highlightedTextColourId, col::label);
        setColour (juce::TooltipWindow::backgroundColourId, col::ink);
        setColour (juce::TooltipWindow::textColourId, col::label);
    }
    juce::Font getPopupMenuFont() override { return regular (17.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return bold (16.0f); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        const bool primary = b.getProperties().getWithDefault ("primary", false);
        // Botones de chapa: relieve suave; encendidos, hundidos con la tinta.
        juce::Colour base = on ? col::ink : (primary ? col::ink.withAlpha (0.92f) : col::enamelHi);
        if (! on && ! primary && (over || down)) base = base.brighter (0.08f);
        if (! on && primary && (over || down)) base = col::ink.brighter (0.15f);
        g.setColour (juce::Colours::black.withAlpha (on || down ? 0.0f : 0.18f));
        g.fillRoundedRectangle (r.translated (0, 1.5f), 4.0f);
        g.setColour (base);
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? juce::Colours::black.withAlpha (0.4f) : col::seam);
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        const bool on = b.getToggleState(), primary = b.getProperties().getWithDefault ("primary", false);
        g.setColour (on || primary ? col::label : col::ink);
        if (! b.isEnabled()) g.setColour (col::inkFaint);
        g.setFont (bold (16.5f));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, 1, 0.85f);
    }

    // Perilla de baquelita negra con índice crema y escala grabada en el panel.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float R = std::min (area.getWidth(), area.getHeight()) * 0.5f;
        const auto c = area.getCentre();
        const float knobR = R * 0.70f;
        // Escala: marcas cada 10 %, la del valor actual en ámbar hasta el índice.
        const int ticks = (int) s.getProperties().getWithDefault ("ticks", 11);
        for (int i = 0; i < ticks; ++i)
        {
            const float t = (float) i / (float) (ticks - 1), a = a0 + t * (a1 - a0);
            const bool major = i == 0 || i == ticks - 1 || i == (ticks - 1) / 2;
            const float r0 = R * (major ? 0.80f : 0.84f), r1 = R * 0.95f;
            g.setColour (t <= pos + 1e-4f ? col::ink : col::inkFaint);
            g.drawLine (c.x + r0 * std::sin (a), c.y - r0 * std::cos (a), c.x + r1 * std::sin (a), c.y - r1 * std::cos (a), major ? 2.0f : 1.2f);
        }
        // Sombra, cuerpo y bisel.
        g.setColour (juce::Colours::black.withAlpha (0.28f));
        g.fillEllipse (c.x - knobR, c.y - knobR + 3.0f, knobR * 2, knobR * 2);
        juce::ColourGradient body (juce::Colour (0xff3b3a3f), c.x - knobR * 0.5f, c.y - knobR, juce::Colour (0xff121114), c.x + knobR * 0.6f, c.y + knobR, true);
        g.setGradientFill (body);
        g.fillEllipse (c.x - knobR, c.y - knobR, knobR * 2, knobR * 2);
        // Estrías del borde.
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (int i = 0; i < 48; ++i)
        {
            const float a = (float) i / 48.0f * juce::MathConstants<float>::twoPi;
            g.drawLine (c.x + knobR * 0.86f * std::sin (a), c.y - knobR * 0.86f * std::cos (a), c.x + knobR * std::sin (a), c.y - knobR * std::cos (a), 1.0f);
        }
        const float capR = knobR * 0.78f;
        juce::ColourGradient cap (juce::Colour (0xff2c2b30), c.x, c.y - capR, juce::Colour (0xff1a191d), c.x, c.y + capR, false);
        g.setGradientFill (cap);
        g.fillEllipse (c.x - capR, c.y - capR, capR * 2, capR * 2);
        // Índice.
        const float a = a0 + pos * (a1 - a0);
        juce::Path p;
        p.addRoundedRectangle (-2.2f, -capR * 0.98f, 4.4f, capR * 0.55f, 2.0f);
        g.setColour (col::label);
        g.fillPath (p, juce::AffineTransform::rotation (a).translated (c.x, c.y));
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.fillAll (col::ink);
        g.setColour (col::label); g.setFont (regular (15.0f));
        g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (8, 4), juce::Justification::centredLeft, 4);
    }
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent) override
    {
        const int w = juce::jmin (360, (int) juce::GlyphArrangement::getStringWidth (regular (15.0f), text) + 18), h = text.length() > 50 ? 48 : 28;
        return juce::Rectangle<int> (pos.x + 12, pos.y + 16, w, h).constrainedWithin (parent);
    }
};

} // namespace dh::lq::ui
