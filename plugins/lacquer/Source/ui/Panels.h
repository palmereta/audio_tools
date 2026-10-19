#pragma once
// Paneles informativos: lectores de sonoridad (entrada → salida), el balance tonal (espectro contra la curva del
// estilo) y "qué hizo Lacquer" (cada módulo con su valor y su reducción en vivo).

#include "LookLq.h"
#include "../dsp/ToneEq.h"

namespace dh::lq::ui {

inline juce::String fmtDb (double v, int dec = 1) { return v <= -69.0 ? juce::String ("--") : juce::String (v, dec); }

class Readouts : public juce::Component
{
public:
    struct Values { double inI = -70, inTp = -100, inLra = 0, outI = -70, outTp = -100, outLra = 0, target = -14, ceiling = -1; double liveM = -70, liveS = -70, liveTp = -100; bool have = false; };
    void set (const Values& v) { V = v; repaint(); }
    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        displayWell (g, b);
        auto r = b.reduced (14.0f, 10.0f);
        const float colW = r.getWidth() / 4.0f;
        const char* heads[] = { "", "Integrated", "True peak", "Range" };
        g.setFont (regular (14.0f));
        for (int k = 1; k < 4; ++k) { g.setColour (col::amberDim.brighter (0.4f)); g.drawText (heads[k], juce::Rectangle<float> (r.getX() + colW * k, r.getY(), colW, 18), juce::Justification::centredRight); }
        auto row = [&] (int i, const char* name, double I, double tp, double lra, bool bright) {
            const float y = r.getY() + 22.0f + 36.0f * (float) i;
            g.setColour (col::amberDim.brighter (0.4f)); g.setFont (regular (15.0f));
            g.drawText (name, juce::Rectangle<float> (r.getX(), y, colW, 30), juce::Justification::centredLeft);
            g.setColour (bright ? col::amber : col::amber.withAlpha (0.62f)); g.setFont (bold (27.0f));
            g.drawText (fmtDb (I), juce::Rectangle<float> (r.getX() + colW, y, colW - 34, 30), juce::Justification::centredRight);
            g.drawText (fmtDb (tp), juce::Rectangle<float> (r.getX() + colW * 2, y, colW - 38, 30), juce::Justification::centredRight);
            g.drawText (V.have || bright ? juce::String (lra, 1) : juce::String ("--"), juce::Rectangle<float> (r.getX() + colW * 3, y, colW - 22, 30), juce::Justification::centredRight);
            g.setFont (regular (13.0f)); g.setColour (col::amberDim.brighter (0.3f));
            g.drawText ("LUFS", juce::Rectangle<float> (r.getX() + colW * 2 - 32, y + 9, 30, 18), juce::Justification::centredLeft);
            g.drawText ("dBTP", juce::Rectangle<float> (r.getX() + colW * 3 - 36, y + 9, 34, 18), juce::Justification::centredLeft);
            g.drawText ("LU", juce::Rectangle<float> (r.getRight() - 20, y + 9, 20, 18), juce::Justification::centredLeft);
        };
        row (0, "Mix in", V.have ? V.inI : -70, V.have ? V.inTp : -100, V.inLra, false);
        row (1, "Master", V.have ? V.outI : -70, V.have ? V.outTp : -100, V.outLra, true);
        // Medidor en vivo de la salida: momentánea y corta sobre una escala −30..0 LUFS con el objetivo marcado.
        const float y = r.getY() + 22.0f + 36.0f * 2 + 8.0f;
        auto bar = juce::Rectangle<float> (r.getX() + colW, y, r.getWidth() - colW - 6, 8.0f);
        auto xOf = [&] (double l) { return bar.getX() + bar.getWidth() * (float) juce::jlimit (0.0, 1.0, (l + 30.0) / 30.0); };
        g.setColour (col::amberDim.withAlpha (0.35f)); g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (col::amber.withAlpha (0.45f)); g.fillRoundedRectangle (bar.withRight (xOf (V.liveS)), 2.0f);
        g.setColour (col::amber); g.fillRect (juce::Rectangle<float> (xOf (V.liveM) - 1.5f, bar.getY() - 2, 3, bar.getHeight() + 4));
        g.setColour (col::label.withAlpha (0.8f)); g.drawLine (xOf (V.target), bar.getY() - 5, xOf (V.target), bar.getBottom() + 5, 1.5f);
        g.setColour (col::amberDim.brighter (0.4f)); g.setFont (regular (14.0f));
        g.drawText ("Live out", juce::Rectangle<float> (r.getX(), y - 6, colW, 20), juce::Justification::centredLeft);
        g.setFont (regular (12.5f));
        for (int l = -30; l <= 0; l += 6) g.drawText (juce::String (l), juce::Rectangle<float> (xOf (l) - 14, bar.getBottom() + 3, 28, 14), juce::Justification::centred);
    }
private:
    Values V;
};

// Balance tonal: banda de tolerancia de la curva del estilo, el espectro de la mezcla (gris) y el del master (ámbar),
// alineados como los compara la EQ automática; abajo, la curva de EQ que se aplicó.
class BalanceView : public juce::Component
{
public:
    void set (const BandCurve& in, const BandCurve& out, bool haveOut, const TargetShape& shape, const BandCurve& eq, bool have)
    {
        mixIn = in; master = out; hasOut = haveOut; target = shape; eqFit = eq; valid = have; repaint();
    }
    void setLive (const std::vector<float>& bands, bool on) { live = bands; liveOn = on; repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        plate (g, b, 6.0f, true);
        auto r = b.reduced (40.0f, 12.0f).withTrimmedBottom (18.0f).withTrimmedTop (18.0f);
        g.setColour (col::ink); g.setFont (bold (17.0f));
        g.drawText ("Tonal balance", b.reduced (14.0f, 6.0f).withHeight (20), juce::Justification::topLeft);
        g.setColour (col::inkSoft); g.setFont (regular (14.0f));
        g.drawText (valid ? "band: style target   grey: your mix   amber: master   line below: EQ applied" : "Learn or open a file to see the balance",
                    b.reduced (14.0f, 6.0f).withHeight (20), juce::Justification::topRight);
        const auto& fc = bandCentres();
        auto xOf = [&] (double f) { return r.getX() + r.getWidth() * (float) (std::log (f / 25.0) / std::log (16000.0 / 25.0)); };
        // Rejilla de frecuencias.
        g.setFont (regular (13.0f));
        for (double f : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
        {
            g.setColour (col::seam.withAlpha (0.6f)); g.drawVerticalLine ((int) xOf (f), r.getY(), r.getBottom());
            g.setColour (col::inkSoft); g.drawText (f >= 1000 ? juce::String (juce::roundToInt (f / 1000)) + "k" : juce::String (juce::roundToInt (f)), juce::Rectangle<float> (xOf (f) - 18, r.getBottom() + 2, 36, 14), juce::Justification::centred);
        }
        if (! valid) return;
        // Alineación: como en matchCurve (mediana entre 100 Hz y 8 kHz de la diferencia).
        auto align = [&] (const BandCurve& c) {
            std::vector<double> d; for (int k = 0; k < kBands; ++k) if (fc[(size_t) k] >= 100 && fc[(size_t) k] <= 8000) d.push_back (c[(size_t) k] - targetAt (target, fc[(size_t) k]));
            std::sort (d.begin(), d.end()); return d.empty() ? 0.0 : d[d.size() / 2];
        };
        const float top = r.getY(), h = r.getHeight() * 0.72f;
        auto yOf = [&] (double db) { return top + h * 0.5f - (float) db * h / 48.0f; };       // ±24 dB alrededor de la curva
        // Banda de tolerancia.
        juce::Path band; bool first = true;
        for (int k = 0; k < kBands; ++k) { const double f = fc[(size_t) k]; if (f < 25 || f > 16000) continue; const auto p = juce::Point<float> (xOf (f), yOf (toleranceAt (f))); if (first) { band.startNewSubPath (p); first = false; } else band.lineTo (p); }
        for (int k = kBands - 1; k >= 0; --k) { const double f = fc[(size_t) k]; if (f < 25 || f > 16000) continue; band.lineTo (xOf (f), yOf (-toleranceAt (f))); }
        band.closeSubPath();
        g.setColour (col::sage.withAlpha (0.35f)); g.fillPath (band);
        g.setColour (col::sage); g.drawLine (xOf (25), yOf (0), xOf (16000), yOf (0), 1.0f);
        auto curvePath = [&] (const BandCurve& c) {
            const double off = align (c);
            juce::Path p; bool f0 = true;
            for (int k = 0; k < kBands; ++k)
            {
                const double f = fc[(size_t) k]; if (f < 25 || f > 16000) continue;
                const auto pt = juce::Point<float> (xOf (f), juce::jlimit (top, top + h, yOf (c[(size_t) k] - off - targetAt (target, f))));
                if (f0) { p.startNewSubPath (pt); f0 = false; } else p.lineTo (pt);
            }
            return p;
        };
        g.setColour (col::inkSoft); g.strokePath (curvePath (mixIn), juce::PathStrokeType (1.6f, juce::PathStrokeType::curved));
        if (hasOut) { g.setColour (col::ink.withAlpha (0.4f)); g.strokePath (curvePath (master), juce::PathStrokeType (4.0f, juce::PathStrokeType::curved)); g.setColour (col::amber); g.strokePath (curvePath (master), juce::PathStrokeType (2.2f, juce::PathStrokeType::curved)); }
        // Curva de EQ aplicada (escala ±6 dB) en la franja de abajo.
        const float eqTop = top + h + 6, eqH = r.getBottom() - eqTop;
        g.setColour (col::seam); g.drawLine (xOf (25), eqTop + eqH * 0.5f, xOf (16000), eqTop + eqH * 0.5f, 1.0f);
        juce::Path eq; bool f1 = true;
        for (int k = 0; k < kBands; ++k)
        {
            const double f = fc[(size_t) k]; if (f < 25 || f > 16000) continue;
            const auto pt = juce::Point<float> (xOf (f), eqTop + eqH * 0.5f - (float) juce::jlimit (-6.0, 6.0, eqFit[(size_t) k]) * eqH / 12.0f);
            if (f1) { eq.startNewSubPath (pt); f1 = false; } else eq.lineTo (pt);
        }
        g.setColour (col::ink); g.strokePath (eq, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved));
        g.setColour (col::inkSoft); g.setFont (regular (12.5f));
        g.drawText ("EQ", juce::Rectangle<float> (b.getX() + 8, eqTop, 28, eqH), juce::Justification::centredLeft);
    }
private:
    BandCurve mixIn {}, master {}, eqFit {};
    TargetShape target { -2.6, 32, 3, 70, 7000, 1.5 };
    bool valid = false, hasOut = false, liveOn = false;
    std::vector<float> live;
};

// Qué hizo Lacquer: una línea por módulo con su ajuste y un medidor de reducción en vivo.
class ModulesView : public juce::Component
{
public:
    struct Row { juce::String name, detail; bool on = false; float gr = 0.0f, grMax = 6.0f; };
    void set (std::vector<Row> r) { rows = std::move (r); repaint(); }
    void setGr (int i, float gr) { if (i >= 0 && i < (int) rows.size()) rows[(size_t) i].gr = gr; }
    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        plate (g, b, 6.0f, true);
        g.setColour (col::ink); g.setFont (bold (17.0f));
        g.drawText ("What Lacquer did", b.reduced (14.0f, 6.0f).withHeight (20), juce::Justification::topLeft);
        auto r = b.reduced (14.0f, 8.0f).withTrimmedTop (26.0f);
        const float rh = rows.empty() ? 20.0f : std::min (24.0f, r.getHeight() / (float) rows.size());
        for (size_t i = 0; i < rows.size(); ++i)
        {
            const auto& w = rows[i];
            auto line = juce::Rectangle<float> (r.getX(), r.getY() + rh * (float) i, r.getWidth(), rh);
            g.setColour (w.on ? col::amber : col::seam);
            g.fillEllipse (line.getX(), line.getCentreY() - 3.5f, 7, 7);
            g.setColour (w.on ? col::ink : col::inkFaint); g.setFont (bold (16.0f));
            g.drawText (w.name, line.withTrimmedLeft (14).withWidth (110), juce::Justification::centredLeft);
            g.setColour (w.on ? col::inkSoft : col::inkFaint); g.setFont (regular (15.0f));
            g.drawFittedText (w.detail, line.withTrimmedLeft (124).withTrimmedRight (70).toNearestInt(), juce::Justification::centredLeft, 1, 0.85f);
            if (w.on && w.grMax > 0.0f)
            {
                auto m = juce::Rectangle<float> (line.getRight() - 62, line.getCentreY() - 3, 62, 6);
                g.setColour (col::seam.withAlpha (0.7f)); g.fillRoundedRectangle (m, 2.0f);
                g.setColour (col::ink); g.fillRoundedRectangle (m.withWidth (m.getWidth() * juce::jlimit (0.0f, 1.0f, w.gr / w.grMax)), 2.0f);
            }
        }
    }
private:
    std::vector<Row> rows;
};

} // namespace dh::lq::ui
