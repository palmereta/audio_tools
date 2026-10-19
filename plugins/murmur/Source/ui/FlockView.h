#pragma once
// La bandada: el búfer es el horizonte (la forma de onda de los últimos segundos, lo recién grabado a la
// derecha) y cada grano es un pájaro. Su lugar horizontal es el momento del búfer que está leyendo, su altura
// es su transposición, el tamaño sigue a su envolvente y vuela en el sentido en que lee (los de reversa hacia
// la izquierda). La franja clara es donde nacen los granos: Position ± Spray.

#include "Widgets.h"

namespace dh::mm::ui {

class FlockView : public juce::Component, private juce::Timer
{
public:
    explicit FlockView (MurmurProcessor& p) : proc (p) { startTimerHz (40); setOpaque (true); }

    void paint (juce::Graphics& g) override
    {
        const auto b = getLocalBounds().toFloat();
        const auto& e = proc.engine();
        const float bufSec = proc.value ("buffer");
        const bool frozen = proc.value ("freeze") > 0.5f || e.frozenNow();

        // El fondo (cielo, grilla y escala de tiempo) no cambia cuadro a cuadro: se dibuja una vez por tamaño, escala de
        // pantalla y largo del búfer. Repintarlo 40 veces por segundo era la mayor parte de la CPU de la ventana.
        const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
        if (bg.isNull() || ! juce::approximatelyEqual (bgSec, bufSec) || ! juce::approximatelyEqual (bgScale, scale) || bgSize != getLocalBounds().getBottomRight())
        {
            bgSec = bufSec; bgScale = scale; bgSize = getLocalBounds().getBottomRight();
            bg = juce::Image (juce::Image::ARGB, std::max (1, juce::roundToInt ((float) getWidth() * scale)), std::max (1, juce::roundToInt ((float) getHeight() * scale)), true);
            juce::Graphics bgG (bg);
            bgG.addTransform (juce::AffineTransform::scale (scale));
            paintBackground (bgG, bufSec);
        }
        g.drawImageTransformed (bg, juce::AffineTransform::scale (1.0f / scale));

        const float x0 = b.getX() + 16.0f, x1 = b.getRight() - 16.0f, W = x1 - x0;
        const float horizon = b.getBottom() - 44.0f, top = b.getY() + 22.0f;
        const float mid = top + (horizon - 40.0f - top) * 0.5f;
        // Eje de tiempo en raíz cuadrada: los últimos segundos (donde suelen nacer los granos) tienen más lugar.
        auto xOfFrac = [&] (float f) { return x1 - W * std::sqrt (juce::jlimit (0.0f, 1.0f, f)); };
        auto xOfBack = [&] (float sec) { return xOfFrac (sec / std::max (0.5f, bufSec)); };

        // Donde nacen los granos.
        const float pos = proc.value ("position"), spray = proc.value ("spray");
        const float bx0 = xOfBack (pos + spray), bx1 = xOfBack (std::max (0.0f, pos - spray));
        g.setColour (col::select.withAlpha (0.07f)); g.fillRect (juce::Rectangle<float> (bx0, top, std::max (2.0f, bx1 - bx0), horizon - top));
        g.setColour (col::select.withAlpha (0.35f)); g.fillRect (xOfBack (pos) - 0.75f, top, 1.5f, horizon - top);

        // El horizonte: la forma de onda del búfer.
        const float wf = e.writeFraction(), bf = e.bufferFraction();
        juce::Path wave; wave.startNewSubPath (x0, horizon);
        // La altura se normaliza al pico visible (con un piso), para que la forma se lea a cualquier nivel de entrada.
        const int cols = (int) W / 2;
        std::vector<float> pks ((size_t) cols + 1);
        float pkTop = 0.02f;
        const int writeBin = std::clamp ((int) (wf * kPeakBins), 0, kPeakBins - 1);
        for (int c = 0; c <= cols; ++c)
        {
            const float u = 1.0f - (float) c / (float) cols, a = u * u;                      // 1 = lo más viejo
            float f = wf - a * bf; f -= std::floor (f);
            const int bin = std::clamp ((int) (f * kPeakBins), 0, kPeakBins - 1);
            float pk = e.peaks[(size_t) bin].load (std::memory_order_relaxed);
            if (bin == writeBin)          // el casillero que se está llenando: el anterior, para que el borde no caiga a cero
                pk = std::max (pk, e.peaks[(size_t) ((bin + kPeakBins - 1) % kPeakBins)].load (std::memory_order_relaxed));
            pks[(size_t) c] = pk;
            pkTop = std::max (pkTop, pks[(size_t) c]);
        }
        for (int c = 0; c <= cols; ++c)
            wave.lineTo (x0 + W * (float) c / (float) cols, horizon - (3.0f + 31.0f * std::sqrt (pks[(size_t) c] / pkTop)));
        wave.lineTo (x1, horizon); wave.closeSubPath();
        g.setColour ((frozen ? col::select : col::bird).withAlpha (0.16f)); g.fillPath (wave);
        g.setColour ((frozen ? col::select : col::bird).withAlpha (0.45f)); g.strokePath (wave, juce::PathStrokeType (1.0f));
        g.setColour (col::border); g.fillRect (x0, horizon, W, 1.0f);

        // Cabeza de grabación y escala de tiempo.
        g.setColour (frozen ? col::select : col::sun); g.fillRect (x1 - 1.0f, top, 2.0f, horizon - top);
        g.setFont (font (11.5f)); g.setColour (col::dim);
        g.drawText (frozen ? "frozen" : "now", juce::Rectangle<float> (x1 - 80.0f, horizon + 6.0f, 80.0f, 16.0f), juce::Justification::centredRight);

        // Los que ya terminaron siguen volando un momento y se desvanecen: la bandada se lee como bandada.
        for (auto& gh : ghosts)
        {
            if (gh.life <= 0.0f) continue;
            const float s = 3.0f + 4.0f * gh.life;
            juce::Path w;
            w.startNewSubPath (-s, -s * 0.45f); w.quadraticTo (-s * 0.35f, -s * 0.7f, 0.0f, 0.0f);
            w.quadraticTo (s * 0.35f, -s * 0.7f, s, -s * 0.45f);
            g.setColour ((gh.reverse ? col::select : col::bird).withAlpha (0.5f * gh.life * gh.life));
            g.strokePath (w, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                          juce::AffineTransform::scale (1.0f, 1.0f + std::sin (gh.phase) * 0.35f).rotated (gh.reverse ? -0.25f : 0.25f).translated (gh.x, gh.y));
        }

        // La bandada.
        for (size_t i = 0; i < birds.size(); ++i)
        {
            const auto& bd = birds[i];
            if (! bd.active) continue;
            const float y = mid - juce::jlimit (-30.0f, 30.0f, bd.pitch) / 24.0f * (mid - top - 6.0f) + bd.pan * 28.0f;
            const float x = xOfFrac (bd.pos);
            const float lv = juce::jlimit (0.0f, 1.0f, bd.level * 1.6f);
            // Estela.
            for (int k = 0; k + 1 < bd.trailN; ++k)
            {
                const auto& p0 = bd.trail[(size_t) k]; const auto& p1 = bd.trail[(size_t) k + 1];
                g.setColour (col::bird.withAlpha (0.08f * lv * (float) (k + 1) / (float) bd.trailN));
                g.drawLine (p0.x, p0.y, p1.x, p1.y, 1.2f);
            }
            const float s = 3.5f + 6.0f * lv;
            juce::Path w;
            w.startNewSubPath (-s, -s * 0.45f); w.quadraticTo (-s * 0.35f, -s * 0.7f, 0.0f, 0.0f);
            w.quadraticTo (s * 0.35f, -s * 0.7f, s, -s * 0.45f);
            const float flap = std::sin (bd.phase) * 0.35f;     // aleteo
            g.setColour ((bd.reverse ? col::select : col::bird).withAlpha (0.25f + 0.75f * lv));
            g.strokePath (w, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                          juce::AffineTransform::scale (1.0f, 1.0f + flap).rotated (bd.reverse ? -0.25f : 0.25f).translated (x, y));
        }
        g.setColour (col::dim); g.setFont (font (12.0f));
        g.drawText ((e.heldNotes() > 0 ? juce::String (e.heldNotes()) + " notes held   " : juce::String()) + juce::String (e.activeGrains.load()) + " grains",
                    juce::Rectangle<float> (x1 - 300.0f, b.getY() + 5.0f, 290.0f, 14.0f), juce::Justification::centredRight);
    }

private:
    juce::Image bg;
    float bgSec = -1.0f, bgScale = 0.0f;
    juce::Point<int> bgSize;

    void paintBackground (juce::Graphics& g, float bufSec)
    {
        const auto b = getLocalBounds().toFloat();
        // Cielo: más oscuro arriba, un resto de luz sobre el horizonte.
        juce::ColourGradient sky (col::bg, b.getX(), b.getY(), col::sky.interpolatedWith (col::sun, 0.06f), b.getX(), b.getBottom(), false);
        g.setGradientFill (sky); g.fillRoundedRectangle (b, 10.0f);
        g.setColour (col::border); g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);
        const float x0 = b.getX() + 16.0f, x1 = b.getRight() - 16.0f, W = x1 - x0;
        const float horizon = b.getBottom() - 44.0f, top = b.getY() + 22.0f;
        const float mid = top + (horizon - 40.0f - top) * 0.5f;
        auto xOfBack = [&] (float sec) { return x1 - W * std::sqrt (juce::jlimit (0.0f, 1.0f, sec / std::max (0.5f, bufSec))); };
        g.setFont (font (11.5f)); g.setColour (col::dim);
        g.drawText ("-" + juce::String (bufSec, bufSec < 10 ? 1 : 0) + " s", juce::Rectangle<float> (x0, horizon + 6.0f, 80.0f, 16.0f), juce::Justification::centredLeft);
        for (float t : { 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f })
            if (t < bufSec * 0.8f)
            {
                const float tx = xOfBack (t);
                g.setColour (col::border); g.fillRect (tx, horizon, 1.0f, 4.0f);
                g.setColour (col::faint);
                g.drawText ("-" + juce::String (t, t < 1.0f ? 2 : 0) + " s", juce::Rectangle<float> (tx - 30.0f, horizon + 6.0f, 60.0f, 16.0f), juce::Justification::centred);
            }
        for (int st : { -24, -12, 12, 24 })
        {
            const float y = mid - (float) st / 24.0f * (mid - top - 6.0f);
            g.setColour (col::border.withAlpha (0.6f)); g.fillRect (x0, y, W, 0.5f);
            g.setColour (col::faint); g.drawText ((st > 0 ? "+" : "") + juce::String (st), juce::Rectangle<float> (x0 + 2.0f, y - 14.0f, 40.0f, 13.0f), juce::Justification::centredLeft);
        }
    }

    struct Bird
    {
        bool active = false, reverse = false;
        float pos = 0, pitch = 0, level = 0, pan = 0, age = 0, phase = 0;
        std::array<juce::Point<float>, 10> trail {};
        int trailN = 0;
    };
    struct Ghost { float x = 0, y = 0, vx = 0, vy = 0, life = 0, phase = 0; bool reverse = false; };
    MurmurProcessor& proc;
    std::array<Bird, kMaxGrains> birds {};
    std::array<Ghost, 384> ghosts {};
    size_t nextGhost = 0;

    void timerCallback() override
    {
        const auto& e = proc.engine();
        const auto b = getLocalBounds().toFloat();
        const float x0 = b.getX() + 16.0f, x1 = b.getRight() - 16.0f, W = x1 - x0;
        const float horizon = b.getBottom() - 44.0f, top = b.getY() + 22.0f, mid = top + (horizon - 40.0f - top) * 0.5f;
        for (size_t i = 0; i < birds.size(); ++i)
        {
            const auto& v = e.view[i];
            auto& bd = birds[i];
            if (! v.active)
            {
                if (bd.active && bd.trailN > 0)       // acaba de terminar: sigue como fantasma
                {
                    auto& gh = ghosts[nextGhost]; nextGhost = (nextGhost + 1) % ghosts.size();
                    const auto last = bd.trail[(size_t) bd.trailN - 1];
                    gh = { last.x, last.y, (bd.reverse ? -1.0f : 1.0f) * (0.3f + 0.02f * (float) (i % 7)), -0.12f - 0.03f * (float) (i % 5), 1.0f, bd.phase, bd.reverse };
                }
                bd.active = false; bd.trailN = 0; continue;
            }
            if (! bd.active || v.age < bd.age) bd.trailN = 0;          // grano nuevo en este lugar
            bd.active = true; bd.reverse = v.reverse; bd.pos = v.pos; bd.pitch = v.pitch; bd.level = v.level; bd.pan = v.pan; bd.age = v.age;
            bd.phase += 0.9f + 0.2f * (float) (i % 5);
            const juce::Point<float> pt (x1 - W * std::sqrt (juce::jlimit (0.0f, 1.0f, bd.pos)), mid - juce::jlimit (-30.0f, 30.0f, bd.pitch) / 24.0f * (mid - top - 6.0f) + bd.pan * 28.0f);
            if (bd.trailN < (int) bd.trail.size()) bd.trail[(size_t) bd.trailN++] = pt;
            else { std::rotate (bd.trail.begin(), bd.trail.begin() + 1, bd.trail.end()); bd.trail.back() = pt; }
        }
        for (auto& gh : ghosts)
            if (gh.life > 0.0f) { gh.x += gh.vx; gh.y += gh.vy; gh.life -= 0.016f; gh.phase += 1.0f; }
        repaint();
    }
};

} // namespace dh::mm::ui
