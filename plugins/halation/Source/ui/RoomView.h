#pragma once
// La sala: una maqueta de yeso vista en corte desde adelante y arriba, con la pared del frente quitada. Las
// partículas de luz (RoomSim) rebotan adentro; su luz tiñe las paredes. Encima: qué está haciendo el espacio en
// palabras, la grilla de tempo con los ecos, la curva de caída y el aviso antes de que la realimentación se dispare.

#include "LookHa.h"
#include "RoomSim.h"
#include "../PluginProcessor.h"

namespace dh::sp::ui {

using view::V3;

class RoomView : public juce::Component, private juce::Timer
{
public:
    explicit RoomView (SpaceProcessor& p) : proc (p)
    {
        // Brillo de partícula: un degradé radial blanco, teñido al dibujarlo.
        glow = juce::Image (juce::Image::ARGB, 64, 64, true);
        juce::Graphics g (glow);
        juce::ColourGradient cg (juce::Colours::white, 32, 32, juce::Colours::white.withAlpha (0.0f), 32, 0, true);
        cg.addColour (0.18, juce::Colours::white.withAlpha (0.85f));
        cg.addColour (0.45, juce::Colours::white.withAlpha (0.25f));
        g.setGradientFill (cg); g.fillEllipse (0, 0, 64, 64);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        // Sala de exposición a oscuras, con la maqueta apoyada en una mesa.
        juce::ColourGradient bg (col::bg.brighter (0.04f), b.getCentreX(), b.getY(), col::bg.darker (0.35f), b.getCentreX(), b.getBottom(), false);
        g.setGradientFill (bg); g.fillRect (b);
        const V3 h = sim.walls();
        const float H = 2.0f * h.z;
        const float frz = proc.view.freeze.load();
        // Luz de las partículas sobre cada pared (tibia y fría por separado).
        float wl[4][2] {};                                      // piso, fondo, izquierda, derecha × (tibio, frío)
        for (int i = 0; i < sim.size(); ++i)
        {
            const auto& q = sim.data()[i];
            const float e = q.size * q.life;
            auto acc = [&] (int w, float d) { const float k = e / (1.0f + d * d * 0.35f); wl[w][0] += k * q.warm; wl[w][1] += k * (1.0f - q.warm); };
            acc (0, q.p.z); acc (1, h.y - q.p.y); acc (2, q.p.x + h.x); acc (3, h.x - q.p.x);
        }
        auto wallColour = [&] (int w, float shade) {
            const float a = 0.25f / std::sqrt ((float) std::max (40, sim.size()));
            auto c = col::plaster.withMultipliedBrightness (shade);
            auto lw = juce::jlimit (0.0f, 1.0f, wl[w][0] * a), lc = juce::jlimit (0.0f, 1.0f, wl[w][1] * a);
            c = c.withMultipliedBrightness (0.44f + 0.5f * (lw + lc));
            c = c.interpolatedWith (col::warm, 0.35f * lw).interpolatedWith (col::cold, 0.35f * lc);
            return c.interpolatedWith (col::frozen.withMultipliedBrightness (0.7f), 0.35f * frz);
        };
        auto quad = [&] (V3 a, V3 bb, V3 c, V3 d) { juce::Path p; p.startNewSubPath (proj (a)); p.lineTo (proj (bb)); p.lineTo (proj (c)); p.lineTo (proj (d)); p.closeSubPath(); return p; };
        // Mesa.
        const float tx = 13.0f, ty = 16.0f;
        g.setColour (col::panel.darker (0.2f));
        g.fillPath (quad ({ -tx, -ty, -0.4f }, { tx, -ty, -0.4f }, { tx, ty, -0.4f }, { -tx, ty, -0.4f }));
        // Piso, fondo, laterales (con espesor: el corte de yeso se ve como un borde claro).
        const float t = 0.35f;
        g.setColour (wallColour (0, 0.9f)); g.fillPath (quad ({ -h.x - t, -h.y, 0 }, { h.x + t, -h.y, 0 }, { h.x + t, h.y + t, 0 }, { -h.x - t, h.y + t, 0 }));
        g.setColour (wallColour (1, 0.82f)); g.fillPath (quad ({ -h.x, h.y, 0 }, { h.x, h.y, 0 }, { h.x, h.y, H }, { -h.x, h.y, H }));
        g.setColour (wallColour (2, 0.7f)); g.fillPath (quad ({ -h.x, -h.y, 0 }, { -h.x, h.y, 0 }, { -h.x, h.y, H }, { -h.x, -h.y, H }));
        g.setColour (wallColour (3, 0.62f)); g.fillPath (quad ({ h.x, -h.y, 0 }, { h.x, h.y, 0 }, { h.x, h.y, H }, { h.x, -h.y, H }));
        // Fuente y oyente: un parlante y un oído de maqueta (cilindros chicos).
        drawMarker (g, sim.source, col::plaster, "Source");
        drawMarker (g, sim.listener, col::plasterMid, "You");
        // Partículas (de atrás hacia adelante).
        order.resize ((size_t) sim.size());
        for (int i = 0; i < sim.size(); ++i) order[(size_t) i] = i;
        std::sort (order.begin(), order.end(), [&] (int a, int c) { return depth (sim.data()[a].p) > depth (sim.data()[c].p); });
        for (int i : order)
        {
            const auto& q = sim.data()[i];
            const float z = depth (q.p); if (z <= 0.5f) continue;
            const auto sp = proj (q.p);
            const float r = juce::jlimit (2.0f, 80.0f, q.size * focal * 0.55f / z * (q.fragment ? 0.75f : 1.3f));
            auto c = lightColour (q.warm).interpolatedWith (juce::Colours::white, 0.35f * q.lift);
            if (frz > 0.01f) c = c.interpolatedWith (col::frozen, 0.5f * frz);
            if (! q.fragment)
            {
                const auto tail = proj (q.p - q.v * 0.045f);
                juce::ColourGradient tg (c.withAlpha (0.0f), tail.x, tail.y, c.withAlpha (0.55f * q.life), sp.x, sp.y, false);
                g.setGradientFill (tg); g.drawLine (juce::Line<float> (tail, sp), std::max (1.5f, r * 0.35f));
            }
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, 0.9f * q.life)));
            g.drawImage (glow, juce::Rectangle<float> (sp.x - r, sp.y - r, 2 * r, 2 * r), juce::RectanglePlacement::stretchToFit, true);
        }
        // Bordes cortados de las paredes (yeso claro) encima de todo.
        g.setColour (col::plaster.withAlpha (0.9f));
        auto edge = [&] (V3 a, V3 c) { g.drawLine (juce::Line<float> (proj (a), proj (c)), 2.2f); };
        edge ({ -h.x, -h.y, H }, { -h.x, h.y, H }); edge ({ h.x, -h.y, H }, { h.x, h.y, H }); edge ({ -h.x, h.y, H }, { h.x, h.y, H });
        edge ({ -h.x, -h.y, 0 }, { -h.x, -h.y, H }); edge ({ h.x, -h.y, 0 }, { h.x, -h.y, H });
        g.setColour (col::plaster.withAlpha (0.5f));
        edge ({ -h.x - t, -h.y, 0 }, { h.x + t, -h.y, 0 });
        if (frz > 0.01f) { g.setColour (col::frozen.withAlpha (0.10f * frz)); g.fillRect (b); }
        paintOverlays (g, b);
    }

    void resized() override
    {
        const auto b = getLocalBounds().toFloat();
        centre = { b.getCentreX(), b.getY() + b.getHeight() * 0.50f };
        focal = std::min (b.getWidth(), b.getHeight() * 1.9f) * 1.3f;
    }

private:
    SpaceProcessor& proc;
    view::RoomSim sim;
    juce::Image glow;
    std::vector<int> order;
    juce::Point<float> centre;
    float focal = 600.0f;
    // Cámara: adelante y arriba de la maqueta, mirando un poco hacia abajo.
    V3 eye { 0.0f, -46.0f, 13.0f }, look { 0.0f, 1.5f, 4.2f };
    std::array<float, 120> wetHist {}; int histPos = 0;
    float pulse = 0.0f;

    V3 fwd() const { V3 f = look - eye; return f * (1.0f / view::len (f)); }
    float depth (V3 p) const { const V3 f = fwd(); const V3 d = p - eye; return d.x * f.x + d.y * f.y + d.z * f.z; }
    juce::Point<float> proj (V3 p) const
    {
        const V3 f = fwd();
        V3 r { f.y, -f.x, 0.0f }; const float rl = view::len (r); r = r * (1.0f / rl);              // derecha = f × arriba
        const V3 u { r.y * f.z - r.z * f.y, r.z * f.x - r.x * f.z, r.x * f.y - r.y * f.x };     // arriba = derecha × f
        const V3 d = p - eye;
        const float x = d.x * r.x + d.y * r.y + d.z * r.z, y = d.x * u.x + d.y * u.y + d.z * u.z, z = std::max (0.1f, d.x * f.x + d.y * f.y + d.z * f.z);
        return { centre.x + x / z * focal, centre.y - y / z * focal };
    }
    void drawMarker (juce::Graphics& g, V3 p, juce::Colour c, const char* label)
    {
        const auto a = proj (p), base = proj ({ p.x, p.y, 0.0f });
        const float r = focal * 0.32f / depth (p);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillEllipse (base.x - r, base.y - r * 0.35f, 2 * r, 0.7f * r);
        g.setColour (c.withMultipliedBrightness (0.8f)); g.fillRect (juce::Rectangle<float> (base.x - r * 0.6f, a.y, r * 1.2f, base.y - a.y));
        g.setColour (c); g.fillEllipse (a.x - r * 0.6f, a.y - r * 0.25f, r * 1.2f, r * 0.5f);
        g.setColour (col::inkDim); g.setFont (regular (12.5f));
        g.drawText (label, juce::Rectangle<float> (base.x - 40, base.y + 4, 80, 16), juce::Justification::centred);
    }

    void timerCallback() override
    {
        auto& v = proc.view;
        view::SimInputs in;
        in.space = v.space.load(); in.sizeScale = v.sizeNow.load(); in.feedback = v.feedback.load(); in.decayS = v.decayS.load();
        in.freeze = v.freeze.load(); in.shimmerLayers = v.shimmerLayers.load(); in.grains = v.grains.load() > 0 ? 0.6f : 0.0f;
        const auto ep = proc.currentEngineParams();
        in.reverse = ep.reverse;
        in.activeLines = v.activeLines.load();
        for (int i = 0; i < 8; ++i) { in.lineMs[i] = v.lineMs[i].load(); in.lineEnergy[i] = v.lineEnergy[i].load(); }
        // Ataques nuevos → partículas.
        int s1, n1, s2, n2; const int ready = v.onsetFifo.getNumReady();
        v.onsetFifo.prepareToRead (ready, s1, n1, s2, n2);
        for (int i = 0; i < n1; ++i) sim.onset (v.onsets[(size_t) (s1 + i)].level, v.onsets[(size_t) (s1 + i)].bright, in);
        for (int i = 0; i < n2; ++i) sim.onset (v.onsets[(size_t) (s2 + i)].level, v.onsets[(size_t) (s2 + i)].bright, in);
        v.onsetFifo.finishedRead (n1 + n2);
        float e = 0.0f; for (int i = 0; i < 8; ++i) e += in.lineEnergy[i];
        sim.mist (e, 0.25f, in, 1.0f / 30.0f);
        for (int k = 0; k < 2; ++k) sim.step (1.0f / 60.0f, in);
        wetHist[(size_t) histPos] = juce::Decibels::gainToDecibels (v.wetPeak.load(), -90.0f); histPos = (histPos + 1) % (int) wetHist.size();
        pulse += 1.0f / 30.0f;
        repaint();
    }

    static juce::String secs (float s) { return s < 10.0f ? juce::String (s, 1) + " s" : juce::String (juce::roundToInt (s)) + " s"; }
    // ── Indicadores ──
    void paintOverlays (juce::Graphics& g, juce::Rectangle<float> b)
    {
        auto& v = proc.view;
        const float space = v.space.load(), decay = v.decayS.load(), fb = v.feedback.load();
        // Qué está haciendo, en palabras.
        g.setColour (col::ink); g.setFont (bold (17.0f));
        juce::String what, detail;
        const float ms = v.lineMs[0].load();
        if (space < 0.25f)
        {
            what = "Delay";
            const int repeats = decay >= 100.0f ? -1 : (int) std::ceil (decay * 1000.0f / std::max (1.0f, ms));
            detail = "An echo every " + juce::String (juce::roundToInt (ms)) + " ms, " + (repeats < 0 ? juce::String ("repeating forever") : juce::String (repeats) + " repeats until silence");
        }
        else if (space > 0.75f)
        {
            what = "Space";
            detail = (decay >= 100.0f ? juce::String ("An endless tail") : "A tail of " + secs (decay)) + ", room " + juce::String (juce::roundToInt (v.sizeNow.load() * 100)) + " %";
        }
        else { what = "Echoes melting into space"; detail = "Echo every " + juce::String (juce::roundToInt (ms)) + " ms, tail " + (decay >= 100.0f ? juce::String ("endless") : secs (decay)); }
        if (v.freeze.load() > 0.5f) { what = "Frozen"; detail = "The space holds what it had. Turn Freeze off to let it go."; }
        g.drawText (what, b.reduced (18, 12).withHeight (22), juce::Justification::topLeft);
        g.setColour (col::inkDim); g.setFont (regular (14.5f));
        g.drawText (detail, b.reduced (18, 12).withTrimmedTop (24).withHeight (20), juce::Justification::topLeft);

        // Curva de caída (arriba a la derecha): la teórica y lo que mide la salida.
        auto dg = juce::Rectangle<float> (b.getRight() - 196, b.getY() + 14, 178, 74);
        g.setColour (col::bg.withAlpha (0.6f)); g.fillRoundedRectangle (dg, 6.0f);
        auto plot = dg.reduced (10, 8).withTrimmedTop (12);
        const float tMax = decay >= 100.0f ? 10.0f : juce::jlimit (1.0f, 30.0f, decay * 1.25f);
        juce::Path curve;
        for (int i = 0; i <= 40; ++i)
        {
            const float tt = tMax * i / 40.0f, db = decay >= 100.0f ? 0.0f : -60.0f * tt / decay;
            const auto pt = juce::Point<float> (plot.getX() + plot.getWidth() * i / 40.0f, plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, -db / 60.0f));
            if (i == 0) curve.startNewSubPath (pt); else curve.lineTo (pt);
        }
        g.setColour (col::plaster.withAlpha (0.8f)); g.strokePath (curve, juce::PathStrokeType (1.5f));
        juce::Path meas;
        for (int i = 0; i < (int) wetHist.size(); ++i)
        {
            const float db = wetHist[(size_t) ((histPos + i) % (int) wetHist.size())];
            const auto pt = juce::Point<float> (plot.getX() + plot.getWidth() * i / (float) (wetHist.size() - 1), plot.getY() + plot.getHeight() * juce::jlimit (0.0f, 1.0f, -db / 60.0f));
            if (i == 0) meas.startNewSubPath (pt); else meas.lineTo (pt);
        }
        g.setColour (col::warm.withAlpha (0.8f)); g.strokePath (meas, juce::PathStrokeType (1.2f));
        g.setColour (col::inkDim); g.setFont (regular (12.0f));
        g.drawText ("Decay to -60 dB", dg.reduced (10, 4).withHeight (14), juce::Justification::topLeft);
        g.drawText (decay >= 100.0f ? juce::String ("endless") : secs (decay), dg.reduced (10, 4).withHeight (14), juce::Justification::topRight);

        // Aviso de realimentación: ámbar cerca de autooscilar, rojo latiendo (≤ 1 Hz) si el limitador ya actúa.
        const float lim = v.limiterDb.load();
        const bool near = fb > 0.97f && v.freeze.load() < 0.5f, active = lim > 0.5f;
        if (near || active)
        {
            const float a = active ? 0.55f + 0.45f * std::sin (pulse * 2.0f * juce::MathConstants<float>::pi) : 1.0f;
            auto wr = juce::Rectangle<float> (b.getRight() - 196, dg.getBottom() + 8, 178, 24);
            g.setColour ((active ? col::alarm : col::amber).withAlpha (0.18f * a)); g.fillRoundedRectangle (wr, 6.0f);
            g.setColour ((active ? col::alarm : col::amber).withAlpha (a)); g.setFont (bold (13.0f));
            g.drawText (active ? "Self-oscillating: limiter holding" : "Near self-oscillation", wr.reduced (8, 0), juce::Justification::centredLeft);
        }

        // Grilla de tempo (abajo): dos compases, las líneas activas y sus repeticiones.
        auto grid = juce::Rectangle<float> (b.getX() + 18, b.getBottom() - 34, b.getWidth() - 36, 20);
        const float bpm = std::max (20.0f, v.bpm.load()), beatMs = 60000.0f / bpm, spanMs = 8.0f * beatMs;
        g.setColour (col::bg.withAlpha (0.55f)); g.fillRoundedRectangle (grid.expanded (0, 4), 5.0f);
        for (int k = 0; k <= 8; ++k)
        {
            const float x = grid.getX() + grid.getWidth() * k / 8.0f;
            g.setColour (k % 4 == 0 ? col::inkDim : col::line);
            g.drawVerticalLine ((int) x, grid.getY(), grid.getBottom());
        }
        if (space < 0.8f)
        {
            const int lines = v.activeLines.load();
            for (int i = 0; i < std::min (8, lines); ++i)
            {
                const float lm = v.lineMs[i].load(); if (lm <= 1.0f) continue;
                float amp = 1.0f;
                for (int rep = 1; rep <= 16; ++rep)
                {
                    const float t = lm * rep; if (t > spanMs) break;
                    const float x = grid.getX() + grid.getWidth() * t / spanMs;
                    g.setColour (col::mist.withAlpha (juce::jlimit (0.05f, 1.0f, amp) * (1.0f - space)));
                    g.fillEllipse (x - 3.5f, grid.getCentreY() - 3.5f + (i & 1 ? 4.0f : -4.0f), 7.0f, 7.0f);
                    amp *= fb;
                }
            }
        }
        g.setColour (col::inkFaint); g.setFont (regular (11.5f));
        g.drawText ("2 bars at " + juce::String (juce::roundToInt (bpm)) + " BPM", grid.withY (grid.getY() - 16).withHeight (14), juce::Justification::topRight);
    }
};

} // namespace dh::sp::ui
