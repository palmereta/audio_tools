#pragma once
// Estilo "Stage" de Thrum: un escenario oscuro con una sola luz cálida. Paneles negros, texto marfil, ámbar para lo que
// está encendido o suena, un rojo de LED para lo que se toca ahora, un cian de plano técnico sólo en Inspect.
// Barlow Condensed (OFL, la de Lacquer y Pallor) para todo el texto.

#include <JuceHeader.h>

namespace thrumui {

namespace col {
    const juce::Colour bg (0xff0b0a0a), bar (0xff100e0d), panel (0xff151312), cell (0xff1d1a18), cell2 (0xff262220);
    const juce::Colour line (0xff2a2522), lineHi (0xff3d3631);
    const juce::Colour text (0xffefe5d6), dim (0xff8c8072), faint (0xff5a5148), ink (0xff0b0a0a);
    const juce::Colour amber (0xffe8a33d), amberHi (0xffffc76b), amberDim (0xff6b4a1d), red (0xffff5a36);
    const juce::Colour cyan (0xff4fd1e8), cyanDim (0xff1f5866);          // Inspect
    const juce::Colour stringCol (0xffcdbea5);
}

inline juce::Typeface::Ptr typeface (bool semi)
{
    static juce::Typeface::Ptr med = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedMedium_ttf, BinaryData::BarlowCondensedMedium_ttfSize);
    static juce::Typeface::Ptr sb = juce::Typeface::createSystemTypefaceFor (BinaryData::BarlowCondensedSemiBold_ttf, BinaryData::BarlowCondensedSemiBold_ttfSize);
    return semi ? sb : med;
}
inline juce::Font font (float h, bool semi = false, float track = 0.0f)
{
    juce::Font f (juce::FontOptions (typeface (semi)).withHeight (h));
    if (track != 0.0f) f.setExtraKerningFactor (track);
    return f;
}
inline void text (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Justification j, float h, bool semi = false, float track = 0.0f)
{
    g.setFont (font (h, semi, track));
    g.drawText (s, r, j, false);
}
// Rótulos en mayúsculas con algo de aire, como en el resto de la serie.
inline void label (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, juce::Justification j, float h = 13.0f, juce::Colour c = col::dim, bool semi = false)
{
    g.setColour (c); text (g, s.toUpperCase(), r, j, h, semi, 0.06f);
}
// Un párrafo corto (varias líneas, sin mayúsculas forzadas).
inline void para (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r, float h = 14.0f, juce::Colour c = col::dim)
{
    g.setColour (c); g.setFont (font (h));
    g.drawFittedText (s, r.toNearestInt(), juce::Justification::topLeft, std::max (1, (int) (r.getHeight() / (h * 1.15f))), 1.0f);
}
inline void section (juce::Graphics& g, const juce::String& s, float x, float y, float w = 300.0f)
{
    g.setColour (col::amber); text (g, s.toUpperCase(), { x, y, w, 20.0f }, juce::Justification::centredLeft, 17.0f, true, 0.05f);
}

// Las imágenes del render (BinaryData), por nombre de archivo, una sola vez.
inline juce::Image asset (const juce::String& file)
{
    static std::map<juce::String, juce::Image> cache;
    if (auto it = cache.find (file); it != cache.end()) return it->second;
    juce::Image im;
    for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
        if (juce::String (BinaryData::originalFilenames[i]) == file)
        {
            int size = 0;
            if (const char* d = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size)) im = juce::ImageCache::getFromMemory (d, size);
            break;
        }
    cache[file] = im;
    return im;
}
// layout.json (escrito por tools/blender/thrum.py): proyecciones de los renders.
inline const juce::var& layout()
{
    static juce::var v = [] {
        for (int i = 0; i < BinaryData::namedResourceListSize; ++i)
            if (juce::String (BinaryData::originalFilenames[i]) == "layout.json")
            {
                int size = 0; const char* d = BinaryData::getNamedResource (BinaryData::namedResourceList[i], size);
                return juce::JSON::parse (juce::String::fromUTF8 (d, size));
            }
        return juce::var();
    }();
    return v;
}
inline juce::Point<float> lpt (const juce::var& a) { return { (float) (double) a[0], (float) (double) a[1] }; }

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::cell);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::amber);
        setColour (juce::PopupMenu::highlightedTextColourId, col::ink);
        setColour (juce::PopupMenu::headerTextColourId, col::amber);
        setColour (juce::TooltipWindow::backgroundColourId, col::text);
        setColour (juce::TooltipWindow::textColourId, col::ink);
        setColour (juce::TooltipWindow::outlineColourId, col::text);
        setColour (juce::AlertWindow::backgroundColourId, col::cell);
        setColour (juce::AlertWindow::textColourId, col::text);
        setColour (juce::AlertWindow::outlineColourId, col::line);
        setColour (juce::TextEditor::backgroundColourId, col::bg);
        setColour (juce::TextEditor::textColourId, col::text);
        setColour (juce::TextEditor::outlineColourId, col::line);
        setColour (juce::TextEditor::focusedOutlineColourId, col::amber);
        setColour (juce::TextEditor::highlightColourId, col::amberDim);
        setColour (juce::CaretComponent::caretColourId, col::amber);
        setColour (juce::TextButton::buttonColourId, col::cell2);
        setColour (juce::TextButton::textColourOffId, col::text);
        setColour (juce::ScrollBar::thumbColourId, col::lineHi);
    }
    juce::Font getPopupMenuFont() override { return font (16.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (juce::jmin (16.0f, (float) h * 0.6f)); }
    void drawPopupMenuBackground (juce::Graphics& g, int w, int h) override
    {
        g.fillAll (col::cell); g.setColour (col::line); g.drawRect (0, 0, w, h);
    }
};

} // namespace thrumui
