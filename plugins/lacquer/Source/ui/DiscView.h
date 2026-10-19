#pragma once
// El disco de laca: el surco es la canción. Cada vuelta de la espiral es un tramo de tiempo y la excursión del surco
// es su nivel, así que se ve la dinámica de un vistazo (un master aplastado deja un surco parejo y ancho). Durante
// LEARN el cabezal corta lo que va escuchando; en modo archivo el surco es el master entero y un clic lleva la
// reproducción a ese punto.

#include "LookLq.h"

namespace dh::lq::ui {

class DiscView : public juce::Component
{
public:
    std::function<void (double frac)> onSeek;            // modo archivo: 0..1

    // groove: nivel en dB cada 50 ms. progress: fracción cortada (0..1). head: posición del cabezal (0..1, −1 = fuera).
    void setGroove (std::vector<float> g, std::vector<float> ghost = {})
    {
        groove = std::move (g); ghostGroove = std::move (ghost);
        cacheValid = false; repaint();
    }
    void setState (double cutFrac, double headFrac, bool cutting, bool seekable)
    {
        if (std::abs (cutFrac - cut) < 1e-4 && std::abs (headFrac - head) < 1e-4 && cutting == isCutting && seekable == canSeek) return;
        if (std::abs (cutFrac - cut) > 1e-4) cacheValid = false;
        cut = cutFrac; head = headFrac; isCutting = cutting; canSeek = seekable; repaint();
    }
    void setLabel (juce::String top, juce::String mid, juce::String bottom) { l1 = std::move (top); l2 = std::move (mid); l3 = std::move (bottom); repaint(); }
    void setSpin (float a) { spin = a; repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const float D = std::min (b.getWidth(), b.getHeight() - 10.0f);
        const auto c = juce::Point<float> (b.getCentreX(), b.getY() + D * 0.5f + 2.0f);
        const float R = D * 0.5f - 6.0f;
        // Plato de aluminio bajo el disco.
        g.setColour (juce::Colours::black.withAlpha (0.22f));
        g.fillEllipse (c.x - R - 6, c.y - R - 2, (R + 6) * 2, (R + 6) * 2 + 6);
        juce::ColourGradient plat (juce::Colour (0xffd9dbd6), c.x - R, c.y - R, juce::Colour (0xff8d908a), c.x + R, c.y + R, false);
        g.setGradientFill (plat);
        g.fillEllipse (c.x - R - 5, c.y - R - 5, (R + 5) * 2, (R + 5) * 2);
        // Laca.
        juce::ColourGradient lac (col::sheen.withAlpha (0.9f), c.x - R * 0.7f, c.y - R * 0.8f, col::lacquer, c.x + R * 0.2f, c.y + R * 0.3f, true);
        g.setGradientFill (lac);
        g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        // Surcos de fondo (laca virgen: finos y regulares).
        g.setColour (juce::Colours::white.withAlpha (0.025f));
        for (float r = R * 0.36f; r < R * 0.98f; r += 3.0f) g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.6f);

        if (! cacheValid || cacheSize != juce::Point<int> (getWidth(), getHeight())) rebuildPaths (c, R);
        // El surco cortado: base oscura + brillo (el corte de una laca refleja la luz).
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.strokePath (groovePath, juce::PathStrokeType (1.6f));
        g.setColour (juce::Colour (0xff8f8a99).withAlpha (0.55f));
        g.strokePath (groovePath, juce::PathStrokeType (0.7f));
        if (! ghostPath.isEmpty())
        {
            g.setColour (col::amber.withAlpha (0.55f));
            g.strokePath (ghostPath, juce::PathStrokeType (0.8f));
        }
        // Reflejo de la lámpara: dos sectores claros que giran con el disco.
        for (int k = 0; k < 2; ++k)
        {
            const float a = spin + (float) k * juce::MathConstants<float>::pi - 0.35f;
            juce::Path sector;
            sector.addPieSegment (c.x - R, c.y - R, R * 2, R * 2, a, a + 0.5f, 0.36f);
            juce::ColourGradient sh (juce::Colours::white.withAlpha (0.07f), c.x, c.y, juce::Colours::white.withAlpha (0.0f), c.x + R * std::sin (a + 0.25f), c.y - R * std::cos (a + 0.25f), true);
            g.setGradientFill (sh);
            g.fillPath (sector);
        }
        // Etiqueta central (papel crema).
        const float lr = R * 0.34f;
        g.setColour (col::label);
        g.fillEllipse (c.x - lr, c.y - lr, lr * 2, lr * 2);
        g.setColour (col::cut.withAlpha (0.85f));
        g.drawEllipse (c.x - lr + 5, c.y - lr + 5, (lr - 5) * 2, (lr - 5) * 2, 1.2f);
        g.setColour (col::ink);
        g.fillEllipse (c.x - 4, c.y - 4, 8, 8);
        g.setColour (col::inkSoft); g.setFont (regular (lr * 0.20f));
        g.drawText (l1, juce::Rectangle<float> (c.x - lr, c.y - lr * 0.80f, lr * 2, lr * 0.26f), juce::Justification::centred);
        g.setColour (col::ink); g.setFont (bold (lr * 0.34f));
        g.drawText (l2, juce::Rectangle<float> (c.x - lr, c.y - lr * 0.56f, lr * 2, lr * 0.40f), juce::Justification::centred);
        g.setColour (col::inkSoft); g.setFont (regular (lr * 0.19f));
        g.drawText (l3, juce::Rectangle<float> (c.x - lr * 0.9f, c.y + lr * 0.16f, lr * 1.8f, lr * 0.5f), juce::Justification::centredTop);

        // Cabezal de corte sobre un brazo que entra desde la derecha.
        if (head >= 0.0)
        {
            const float rr = radiusAt (head, R);
            const float ang = -0.55f;                       // el brazo entra en diagonal desde arriba a la derecha
            const auto tip = juce::Point<float> (c.x + rr * std::sin (ang + juce::MathConstants<float>::pi * 0.5f), c.y - rr * std::cos (ang + juce::MathConstants<float>::pi * 0.5f));
            const auto pivot = juce::Point<float> (b.getRight() - 16.0f, b.getY() + 22.0f);
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.drawLine (pivot.x + 3, pivot.y + 5, tip.x + 3, tip.y + 5, 7.0f);
            juce::ColourGradient arm (juce::Colour (0xffe4e5e0), pivot.x, pivot.y - 4, juce::Colour (0xff8e918a), pivot.x, pivot.y + 4, false);
            g.setGradientFill (arm);
            g.drawLine (pivot.x, pivot.y, tip.x, tip.y, 6.0f);
            g.setColour (col::ink);
            g.fillEllipse (pivot.x - 9, pivot.y - 9, 18, 18);
            // Cabeza.
            juce::Path hd; hd.addRoundedRectangle (-11.0f, -8.0f, 22.0f, 16.0f, 3.0f);
            const float aa = std::atan2 (tip.y - pivot.y, tip.x - pivot.x);
            g.setColour (col::ink);
            g.fillPath (hd, juce::AffineTransform::rotation (aa).translated (tip.x, tip.y));
            g.setColour (isCutting ? col::cut : col::amberDim);
            g.fillEllipse (tip.x - 3.5f, tip.y - 3.5f, 7.0f, 7.0f);
            if (isCutting)
            {
                g.setColour (col::cut.withAlpha (0.35f));
                g.fillEllipse (tip.x - 8, tip.y - 8, 16, 16);
            }
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { seekTo (e.position); }
    void mouseDrag (const juce::MouseEvent& e) override { seekTo (e.position); }
    void resized() override { cacheValid = false; }

private:
    std::vector<float> groove, ghostGroove;
    double cut = 0.0, head = -1.0;
    bool isCutting = false, canSeek = false, cacheValid = false;
    juce::Point<int> cacheSize;
    juce::Path groovePath, ghostPath;
    juce::String l1, l2, l3;
    float spin = 0.0f;
    static constexpr float kTurns = 26.0f;                // vueltas de la espiral para toda la duración

    static float radiusAt (double frac, float R) { return R * (0.96f - 0.56f * (float) juce::jlimit (0.0, 1.0, frac)); }

    void buildPath (juce::Path& p, const std::vector<float>& g, juce::Point<float> c, float R, double upTo)
    {
        p.clear();
        if (g.size() < 2) return;
        const size_t n = (size_t) ((double) g.size() * juce::jlimit (0.0, 1.0, upTo));
        if (n < 2) return;
        // Escala de nivel: el máximo del surco toca la vuelta vecina; −40 dB por debajo es una línea recta.
        float mx = -100.0f; for (float v : g) mx = std::max (mx, v);
        const float pitch = R * 0.56f / kTurns;
        const int stepsPerTurn = 360;
        const double total = (double) kTurns * stepsPerTurn;
        bool first = true;
        for (int s = 0; s <= (int) (total * (double) n / (double) g.size()); ++s)
        {
            const double frac = (double) s / total;
            const size_t gi = std::min (g.size() - 1, (size_t) (frac * (double) g.size()));
            const float lev = juce::jlimit (0.0f, 1.0f, (g[gi] - (mx - 40.0f)) / 40.0f);
            // Excursión: una oscilación rápida (el "audio") cuyo ancho es el nivel.
            const float wig = 0.48f * pitch * lev * std::sin ((float) s * 2.4f);
            const float ang = (float) (frac * kTurns * juce::MathConstants<double>::twoPi);
            const float r = radiusAt (frac, R) + wig;
            const auto pt = juce::Point<float> (c.x + r * std::sin (ang), c.y - r * std::cos (ang));
            if (first) { p.startNewSubPath (pt); first = false; } else p.lineTo (pt);
        }
    }
    void rebuildPaths (juce::Point<float> c, float R)
    {
        buildPath (groovePath, groove, c, R, cut);
        buildPath (ghostPath, ghostGroove, c, R, ghostGroove.empty() ? 0.0 : cut);
        cacheValid = true; cacheSize = { getWidth(), getHeight() };
    }
    void seekTo (juce::Point<float> p)
    {
        if (! canSeek || ! onSeek) return;
        const auto b = getLocalBounds().toFloat();
        const float D = std::min (b.getWidth(), b.getHeight() - 10.0f);
        const auto c = juce::Point<float> (b.getCentreX(), b.getY() + D * 0.5f + 2.0f);
        const float R = D * 0.5f - 6.0f;
        const float r = p.getDistanceFrom (c);
        if (r < R * 0.36f || r > R) return;
        // Radio → vuelta; el ángulo afina dentro de la vuelta.
        const float ang = std::atan2 (p.x - c.x, -(p.y - c.y));
        const double a01 = (ang < 0 ? ang + juce::MathConstants<float>::twoPi : ang) / juce::MathConstants<double>::twoPi;
        const double fracR = (0.96 - r / R) / 0.56;
        const double turn = std::floor (fracR * kTurns - a01 + 0.5);
        onSeek (juce::jlimit (0.0, 1.0, (turn + a01) / kTurns));
    }
};

} // namespace dh::lq::ui
