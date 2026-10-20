#pragma once

// The website's look (web/assets/site.css): charcoal panel, warm white ink, one accent per plugin,
// pill buttons. The site's typeface is Archivo from Google Fonts, which is not in the repository;
// the launcher uses the site's own fallback (Helvetica Neue on macOS, Arial on Windows).

#include <juce_gui_basics/juce_gui_basics.h>

namespace atl::theme
{
using juce::Colour;

inline const Colour bg       { 0xff1b1c1e };   // --bg
inline const Colour deep     { 0xff121314 };   // --deep
inline const Colour card     { 0xff26272a };   // --card
inline const Colour card2    { 0xff2e2f33 };   // --card-2
inline const Colour ink      { 0xfff2f0ea };   // --ink
inline const Colour ink2     { 0xffb8b5ad };   // --ink-2
inline const Colour ink3     { 0xff85827b };   // --ink-3
inline const Colour hair     { ink.withAlpha (0.14f) };
inline const Colour accent   { 0xffd24a35 };   // --accent (AT20V red), for buttons that belong to no plugin

inline Colour parse (const juce::String& hex, Colour fallback = accent)
{
    auto h = hex.trim().trimCharactersAtStart ("#");
    if (h.length() != 6 || ! h.containsOnly ("0123456789abcdefABCDEF")) return fallback;
    return Colour ((juce::uint32) (0xff000000u | (juce::uint32) h.getHexValue32()));
}

/** --accent-ink: dark text on the light accents (yellow, lime, lilac), white on the rest. */
inline Colour inkOn (Colour c)
{
    const auto lin = [] (float v) { return v <= 0.03928f ? v / 12.92f : std::pow ((v + 0.055f) / 1.055f, 2.4f); };
    const float l = 0.2126f * lin (c.getFloatRed()) + 0.7152f * lin (c.getFloatGreen()) + 0.0722f * lin (c.getFloatBlue());
    return l > 0.3f ? Colour (0xff17140a) : Colour (0xffffffff);
}

inline juce::String family()
{
   #if JUCE_MAC
    return "Helvetica Neue";
   #elif JUCE_LINUX
    // Linux no suele traer Arial, y con una familia que no está instalada JUCE no dibuja el texto: la primera de
    // estas que haya (Liberation Sans tiene las medidas de Arial) o, vacía, la sans que elige JUCE.
    static const juce::String name = []
    {
        const auto all = juce::Font::findAllTypefaceNames();
        for (auto* f : { "Liberation Sans", "Arial", "Noto Sans", "DejaVu Sans" })
            if (all.contains (f)) return juce::String (f);
        return juce::String();
    }();
    return name;
   #else
    return "Arial";
   #endif
}

inline juce::Font uiFont (float h, bool bold = false)
{
    return juce::Font (juce::FontOptions (family(), h, bold ? juce::Font::bold : juce::Font::plain));
}

/** The site's headings use Archivo at a narrow width (wdth 70); Helvetica Neue has a condensed cut. */
inline juce::Font heading (float h)
{
   #if JUCE_MAC
    return juce::Font (juce::FontOptions (family(), h, juce::Font::plain).withStyle ("Condensed Bold"));
   #elif JUCE_LINUX
    return juce::Font (juce::FontOptions (family(), h, juce::Font::bold));
   #else
    return juce::Font (juce::FontOptions ("Arial Narrow", h, juce::Font::bold));
   #endif
}

//==============================================================================
/** .pill: primary (filled with the accent), ghost (outlined) or quiet (text only). */
class Pill : public juce::Button
{
public:
    enum class Style { primary, ghost, quiet };

    explicit Pill (const juce::String& text = {}, Style s = Style::ghost) : juce::Button (text), style (s)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setStyle (Style s)            { style = s; repaint(); }
    void setAccent (Colour c)          { tint = c; repaint(); }
    void setDots (bool d)              { dots = d; repaint(); }   // the "more" button
    void setFontHeight (float h)       { fontH = h; }

    int idealWidth() const             { return dots ? 34 : (int) juce::GlyphArrangement::getStringWidth (uiFont (fontH, true), getButtonText()) + 34; }

    void paintButton (juce::Graphics& g, bool over, bool down) override
    {
        auto r = getLocalBounds().toFloat().reduced (0.75f);
        const float rad = r.getHeight() * 0.5f;
        const bool on = isEnabled();
        Colour fg = ink;

        if (style == Style::primary && on)
        {
            auto fill = tint;
            if (over) fill = fill.brighter (0.08f);
            if (down) fill = fill.darker (0.08f);
            g.setColour (fill);
            g.fillRoundedRectangle (r, rad);
            fg = inkOn (tint);
        }
        else if (style == Style::ghost || (style == Style::primary && ! on))
        {
            if (over && on) { g.setColour (ink.withAlpha (0.08f)); g.fillRoundedRectangle (r, rad); }
            g.setColour (ink.withAlpha (on ? 0.55f : 0.18f));
            g.drawRoundedRectangle (r, rad, 1.5f);
            fg = on ? ink : ink3;
        }
        else
        {
            if (over && on) { g.setColour (ink.withAlpha (0.08f)); g.fillRoundedRectangle (r, rad); }
            fg = on ? (over ? ink : ink2) : ink3;
        }

        g.setColour (fg);
        if (dots)
        {
            const auto c = r.getCentre();
            for (int i = -1; i <= 1; ++i) g.fillEllipse (c.x + (float) i * 6.0f - 1.8f, c.y - 1.8f, 3.6f, 3.6f);
            return;
        }
        g.setFont (uiFont (fontH, true));
        g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, false);
    }

private:
    Style style;
    Colour tint = accent;
    bool dots = false;
    float fontH = 14.5f;
};

/** A slim switch with a label, drawn like the site's controls. */
class Toggle : public juce::ToggleButton
{
public:
    using juce::ToggleButton::ToggleButton;

    void paintButton (juce::Graphics& g, bool over, bool) override
    {
        auto r = getLocalBounds().toFloat();
        auto sw = r.removeFromLeft (38.0f).withSizeKeepingCentre (38.0f, 22.0f);
        const bool on = getToggleState();
        g.setColour (on ? accent : card2.brighter (over ? 0.12f : 0.05f));
        g.fillRoundedRectangle (sw, 11.0f);
        g.setColour (on ? juce::Colours::white : ink2);
        const float x = on ? sw.getRight() - 19.0f : sw.getX() + 3.0f;
        g.fillEllipse (x, sw.getY() + 3.0f, 16.0f, 16.0f);
        g.setColour (isEnabled() ? ink : ink3);
        g.setFont (uiFont (15.0f));
        g.drawText (getButtonText(), r.withTrimmedLeft (12.0f), juce::Justification::centredLeft, true);
    }
};

} // namespace atl::theme
