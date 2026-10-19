#pragma once
// Piezas dibujadas para las páginas SEQUENCER y SETTINGS, con el mismo vocabulario del panel:
// fondo del gabinete, marcos en relieve, botones crema con LED rojo, ventanas negras y faders con
// los capuchones renderizados.

#include <JuceHeader.h>
#include "Assets101.h"

namespace dh::w101 {

// Fondo: el panel renderizado detrás, tapado por el plástico (queda la textura y la luz).
inline void backdrop (juce::Graphics& g, juce::Rectangle<float> r, int body)
{
    const auto& A = Assets101::get();
    const auto& bg = A.backgrounds[(size_t) juce::jlimit (0, 2, body)];
    g.fillAll (col101::editorBg);
    if (bg.isValid()) { g.setOpacity (1.0f); g.drawImage (bg, r); }
    g.setColour (col101::body (body).withAlpha (0.975f));
    g.fillRoundedRectangle (r.reduced (14.0f, 12.0f), 6.0f);
    juce::ColourGradient light (juce::Colours::white.withAlpha (0.05f), r.getX(), r.getY(),
                                juce::Colours::black.withAlpha (0.12f), r.getX(), r.getBottom(), false);
    g.setGradientFill (light);
    g.fillRoundedRectangle (r.reduced (14.0f, 12.0f), 6.0f);
}

// Marco moldeado (como los que separan las secciones del 101), con título opcional arriba.
inline void frame (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title = {}, int body = 0)
{
    const auto base = col101::body (body);
    g.setColour (base.darker (0.55f)); g.drawRoundedRectangle (r.translated (0.0f, 1.2f), 2.0f, 2.0f);
    g.setColour (base.brighter (0.28f)); g.drawRoundedRectangle (r, 2.0f, 1.4f);
    if (title.isNotEmpty())
    {
        auto head = r.removeFromTop (30.0f);
        g.setColour (base.brighter (0.28f)); g.drawLine (r.getX(), head.getBottom(), r.getRight(), head.getBottom(), 1.4f);
        g.setColour (base.darker (0.55f)); g.drawLine (r.getX(), head.getBottom() + 1.4f, r.getRight(), head.getBottom() + 1.4f, 1.0f);
        g.setColour (col101::silk); g.setFont (col101::font (17.0f));
        g.drawText (title, head, juce::Justification::centred);
    }
}

inline void silk (juce::Graphics& g, const juce::String& t, juce::Rectangle<float> r, float size = 10.5f,
                  juce::Justification j = juce::Justification::centred, juce::Colour c = col101::silk)
{
    g.setColour (c); g.setFont (col101::font (size)); g.drawFittedText (t, r.toNearestInt(), j, 2, 0.9f);
}

inline void led (juce::Graphics& g, juce::Point<float> c, bool on)
{
    const auto r = juce::Rectangle<float> (9.0f, 4.6f).withCentre (c);
    if (on)
    {
        g.setColour (col101::ledRed.withAlpha (0.22f)); g.fillEllipse (r.expanded (6.0f, 5.0f));
        juce::ColourGradient gr (juce::Colour (0xffffb090), r.getCentreX(), r.getY(), col101::ledRed, r.getCentreX(), r.getBottom(), false);
        g.setGradientFill (gr);
    }
    else g.setColour (col101::ledOff);
    g.fillRoundedRectangle (r, 1.2f);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (r, 1.2f, 0.6f);
}

// Botón crema del 101 (arriba o apretado), con la leyenda serigrafiada debajo.
inline void creamButton (juce::Graphics& g, juce::Rectangle<float> r, bool down, const juce::String& label = {}, bool dark = false)
{
    const auto cap = dark ? juce::Colour (0xff202022) : col101::cream;
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (r.translated (0.0f, down ? 1.0f : 2.6f).expanded (0.6f), 2.5f);
    auto face = r.translated (0.0f, down ? 1.2f : 0.0f);
    juce::ColourGradient gr (cap.brighter (0.12f), face.getX(), face.getY(), cap.darker (down ? 0.25f : 0.12f), face.getX(), face.getBottom(), false);
    g.setGradientFill (gr); g.fillRoundedRectangle (face, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (down ? 0.05f : 0.35f));
    g.drawLine (face.getX() + 3, face.getY() + 1.0f, face.getRight() - 3, face.getY() + 1.0f, 1.0f);
    if (label.isNotEmpty()) silk (g, label, { r.getX() - 20.0f, r.getBottom() + 4.0f, r.getWidth() + 40.0f, 14.0f }, 9.5f);
}

// Ventana negra con dígitos rojos (para el largo de la secuencia, la transposición, etc.).
inline void display (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text)
{
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (r.expanded (1.5f).translated (0, 1.0f), 3.0f);
    g.setColour (juce::Colour (0xff0b0a0a)); g.fillRoundedRectangle (r, 3.0f);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), r.getHeight() * 0.72f, juce::Font::bold));
    g.setColour (col101::ledRed.withAlpha (0.10f));
    g.drawText (juce::String::repeatedString ("8", std::max (3, text.length())), r.reduced (6.0f, 0), juce::Justification::centredRight);
    g.setColour (juce::Colour (0xffff5a3c));
    g.drawText (text, r.reduced (6.0f, 0), juce::Justification::centredRight);
}

// Fader con ranura, escala 0–10 a los dos lados y el capuchón renderizado.
inline void fader (juce::Graphics& g, juce::Rectangle<float> r, float v, bool green)
{
    const auto& A = Assets101::get();
    const float cx = r.getCentreX(), top = r.getY() + 10.0f, bot = r.getBottom() - 10.0f;
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (juce::Rectangle<float> (cx - 3.2f, top - 6.0f, 6.4f, bot - top + 12.0f).translated (0, 1), 2.0f);
    g.setColour (col101::slot); g.fillRoundedRectangle (cx - 3.0f, top - 6.0f, 6.0f, bot - top + 12.0f, 2.0f);
    g.setColour (col101::silk.withAlpha (0.8f));
    for (int i = 0; i <= 10; ++i)
    {
        const float y = bot - (bot - top) * (float) i / 10.0f;
        const float len = i % 5 == 0 ? 7.0f : 4.5f;
        g.fillRect (cx - 14.0f - len + 7.0f, y - 0.5f, len, 1.0f);
        g.fillRect (cx + 14.0f - 7.0f, y - 0.5f, len, 1.0f);
    }
    const float y = bot - (bot - top) * juce::jlimit (0.0f, 1.0f, v);
    A.draw (g, green ? A.capGreen : A.capOrange, { cx, y + 3.0f }, 0, 0.92f);
}

} // namespace dh::w101
