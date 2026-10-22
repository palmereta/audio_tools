#pragma once
// El diapasón en vivo: el render del diapasón del tipo elegido (board_<k>.jpg, sin cuerdas) y encima las cuerdas, que
// dibuja la interfaz para poder hacerlas vibrar: la que suena tiembla entre el traste pisado y el puente con su nivel,
// un punto ámbar donde está el dedo y un aro en la cejuela para las cuerdas al aire. Con Inspect: el nombre de cada
// nota y su cuerda/traste en cian. La geometría sale de layout.json (la misma del render: escala, trastes, márgenes).

#include "Theme.h"
#include "../PluginProcessor.h"

namespace thrumui {

class Fretboard : public juce::Component
{
public:
    explicit Fretboard (ThrumProcessor& p) : proc (p) { setOpaque (true); }
    bool inspect = false;

    void resized() override { cacheType = -1; }
    void tick()
    {
        // Repinta sólo si algo suena o acaba de cambiar (con todo callado no gasta).
        bool any = false; uint64_t sig = 0;
        const int n = proc.view.strings.load();
        for (int s = 0; s < n; ++s)
        {
            const int note = proc.view.note[(size_t) s].load();
            const float lev = proc.view.level[(size_t) s].load();
            any |= note >= 0 && lev > 1e-4f;
            sig = sig * 131 + (uint64_t) (note + 1) * 97 + (uint64_t) juce::roundToInt (proc.view.fret[(size_t) s].load() * 4.0f);
        }
        sig = sig * 31 + (uint64_t) (int) proc.value ("g_type") * 7 + (uint64_t) proc.view.chordRoot.load() * 13 + (uint64_t) (inspect ? 1 : 0);
        if (any || sig != lastSig) { lastSig = sig; repaint(); }
    }

    void paint (juce::Graphics& g) override
    {
        const int type = juce::jlimit (0, 5, (int) proc.value ("g_type"));
        if (type != cacheType || cache.getWidth() != getWidth()) rebuild (type);
        g.drawImageAt (cache, 0, 0);
        const auto B = geo;
        const int n = proc.view.strings.load();
        const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;
        // Las cuerdas: de la cejuela al borde derecho (el puente queda afuera, a la derecha).
        for (int i = 0; i < n; ++i)
        {
            const int s = n - 1 - i;          // en el motor la 0 es la más grave
            const int note = proc.view.note[(size_t) s].load();
            const float fret = proc.view.fret[(size_t) s].load();
            const float lev = juce::jlimit (0.0f, 1.0f, proc.view.level[(size_t) s].load() * 5.0f);
            const float thick = 0.9f + 0.32f * (float) i * (6.0f / (float) std::max (6, n));
            const bool sounding = note >= 0;
            const float xs = sounding ? xOfFret (std::max (0.0f, fret)) : B.xNut;
            // La parte muerta (entre la cejuela y el dedo) queda quieta; la que suena vibra.
            g.setColour (col::stringCol.withAlpha (sounding ? 0.55f : 0.75f));
            g.drawLine (B.xNut, yOf (i, 0.0f), xs, yOf (i, dOf (xs)), thick);
            if (sounding && lev > 0.003f)
            {
                juce::Path a, bpath;
                const float amp = 3.6f * lev * (1.0f - 0.05f * (float) i);
                const float span = B.xBridge - xs;
                const float ph = (float) std::sin (t * (40.0 + 7.0 * i) + i);
                for (int k = 0; k <= 60; ++k)
                {
                    const float x = xs + (B.right - xs) * (float) k / 60.0f;
                    const float q = (x - xs) / span;
                    const float y = yOf (i, dOf (x));
                    const float e = amp * std::sin (juce::MathConstants<float>::pi * q);
                    if (k == 0) { a.startNewSubPath (x, y - e); bpath.startNewSubPath (x, y + e); }
                    else { a.lineTo (x, y - e); bpath.lineTo (x, y + e); }
                    juce::ignoreUnused (ph);
                }
                juce::Path fill (a);
                for (int k = 60; k >= 0; --k)
                {
                    const float x = xs + (B.right - xs) * (float) k / 60.0f;
                    const float q = (x - xs) / span;
                    fill.lineTo (x, yOf (i, dOf (x)) + amp * std::sin (juce::MathConstants<float>::pi * q));
                }
                fill.closeSubPath();
                g.setColour (col::amber.withAlpha (0.22f + 0.3f * lev)); g.fillPath (fill);
                g.setColour (col::amberHi.withAlpha (0.65f + 0.35f * lev));
                g.strokePath (a, juce::PathStrokeType (thick * 0.8f)); g.strokePath (bpath, juce::PathStrokeType (thick * 0.8f));
            }
            else
            {
                g.setColour (col::stringCol.withAlpha (0.8f));
                g.drawLine (xs, yOf (i, dOf (xs)), B.right, yOf (i, dOf (B.right)), thick);
            }
            if (! sounding) continue;
            // El dedo, o el aro de la cuerda al aire.
            const float y = yOf (i, fret < 0.5f ? 0.0f : dOfFret (fret - 0.5f));
            if (fret < 0.5f)
            {
                g.setColour (col::amber); g.drawEllipse (B.xNut - 15.0f, y - 6.5f, 13.0f, 13.0f, 2.0f);
            }
            else
            {
                const float xf = xOfFret (fret - 0.5f);      // entre los dos trastes (en un slide, donde vaya el dedo)
                g.setColour (col::amber.withAlpha (0.28f)); g.fillEllipse (xf - 12.0f, y - 12.0f, 24.0f, 24.0f);
                g.setColour (col::amber); g.fillEllipse (xf - 8.0f, y - 8.0f, 16.0f, 16.0f);
                if (inspect) { g.setColour (col::ink); text (g, juce::String (juce::roundToInt (fret)), { xf - 8.0f, y - 8.0f, 16.0f, 16.0f }, juce::Justification::centred, 11.5f, true); }
            }
            if (inspect)
            {
                static const char* names[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
                const float xl = fret < 0.5f ? B.xNut + 4.0f : xOfFret (fret) + 4.0f;
                g.setColour (col::cyan);
                text (g, juce::String (names[note % 12]) + juce::String (note / 12 - 1), { xl, y - 15.0f, 60.0f, 12.0f }, juce::Justification::centredLeft, 11.5f, true);
            }
        }
    }

    // Para la página Guitar (Inspect): dónde cae cada traste.
    float xOfFret (float f) const { return geo.xNut + geo.pxPerMm * (float) (geo.scale * (1.0 - std::exp2 (-f / 12.0))); }

private:
    ThrumProcessor& proc;
    juce::Image cache; int cacheType = -1; uint64_t lastSig = 1;
    struct Geo { float xNut = 0, pxPerMm = 1, cy = 0, pxPerMmY = 1, right = 0, xBridge = 0; double scale = 648; int frets = 22, n = 6; float nutW = 43, sb = 10.5f; } geo;

    float dOf (float x) const { return (x - geo.xNut) / geo.pxPerMm; }                       // mm desde la cejuela
    float dOfFret (float f) const { return (float) (geo.scale * (1.0 - std::exp2 (-std::max (0.0f, f) / 12.0))); }
    float yOf (int i, float d) const
    {
        const float sn = (geo.nutW - 7.0f) / (float) (geo.n - 1);
        const float u = 1.0f - d / (float) geo.scale;                 // x/escala desde el puente
        const float sp = geo.sb + (sn - geo.sb) * u;
        return geo.cy + (-(((float) geo.n - 1.0f) * 0.5f - (float) i) * sp) * geo.pxPerMmY;
    }

    void rebuild (int type)
    {
        cacheType = type;
        const auto& b = layout()["board"][type];
        auto img = asset ("board_" + juce::String (type) + ".jpg");
        const int W = std::max (1, getWidth()), H = std::max (1, getHeight());
        cache = juce::Image (juce::Image::RGB, W, H, true);
        juce::Graphics g (cache);
        g.fillAll (col::bg);
        const float padN = (float) (double) b["padNut"], padE = (float) (double) b["padEnd"];
        geo.scale = (double) b["scale"]; geo.frets = (int) b["frets"]; geo.n = (int) b["strings"];
        geo.nutW = (float) (double) b["nutW"]; geo.sb = (float) (double) b["spacingBridge"];
        const float half = (float) (double) b["halfMm"];
        const float dLast = (float) (geo.scale * (1.0 - std::exp2 (-(double) geo.frets / 12.0)));
        const float boardH = (float) H - 20.0f;                             // abajo: los números de traste
        const float iw = img.isValid() ? (float) img.getWidth() : 2400.0f, ih = img.isValid() ? (float) img.getHeight() : 300.0f;
        const float drawH = (float) W * ih / iw;
        auto dst = juce::Rectangle<float> (0.0f, (boardH - drawH) * 0.5f, (float) W, drawH);
        {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (0, 0, W, (int) boardH);
            if (img.isValid()) g.drawImage (img, dst, juce::RectanglePlacement::stretchToFit);
            // Bordes que se funden con el fondo.
            juce::ColourGradient top (col::bg, 0, 0, col::bg.withAlpha (0.0f), 0, 18, false); g.setGradientFill (top); g.fillRect (0, 0, W, 18);
            juce::ColourGradient bot (col::bg.withAlpha (0.0f), 0, boardH - 18, col::bg, 0, boardH, false); g.setGradientFill (bot); g.fillRect (0.0f, boardH - 18.0f, (float) W, 18.0f);
        }
        geo.pxPerMm = (float) W / (dLast + padN + padE);
        geo.xNut = padN * geo.pxPerMm;
        geo.cy = dst.getCentreY();
        geo.pxPerMmY = drawH / (2.0f * half);
        geo.right = (float) W;
        geo.xBridge = geo.xNut + (float) geo.scale * geo.pxPerMm;
        // Números de traste.
        g.setColour (col::dim);
        for (int f : { 3, 5, 7, 9, 12, 15, 17, 19, 21, 24 })
        {
            if (f > geo.frets) continue;
            const float x = 0.5f * (xOfFret ((float) f - 1.0f) + xOfFret ((float) f));
            text (g, juce::String (f), { x - 14.0f, boardH + 1.0f, 28.0f, 16.0f }, juce::Justification::centred, 13.0f);
        }
    }
};

} // namespace thrumui
