#pragma once
// El orrery: un sol con el acorde que suena, una órbita por carril activo y un planeta que la recorre al
// ritmo del lazo del carril (o del acorde, si no tiene lazo). Cada nota deja una chispa sobre su órbita que
// se apaga despacio. Con la respiración encendida, un halo alrededor del sol se abre y se cierra.

#include "Widgets.h"

namespace dh::orr::ui {

class OrreryView : public juce::Component, private juce::Timer
{
public:
    std::function<void (int)> onLane;
    explicit OrreryView (OrreryProcessor& p) : proc (p)
    {
        stars.setInterceptsMouseClicks (false, false); stars.setBufferedToImage (true);
        stars.draw = [this] (juce::Graphics& g) { paintStars (g); };
        addAndMakeVisible (stars);
        startTimerHz (30);
    }
    void resized() override { stars.setBounds (getLocalBounds()); }
    void paint (juce::Graphics&) override {}
    void paintOverChildren (juce::Graphics& g) override
    {
        const auto& v = proc.view();
        const auto c = centre(); const float R = radius();
        // Órbitas y planetas.
        int k = 0; const int n = activeCount();
        for (int l = 0; l < kLanes; ++l)
        {
            if (! laneOn (l)) continue;
            const float r = orbitRadius (k++, n);
            const auto colr = col::lane (l);
            g.setColour (colr.withAlpha (hover == l ? 0.5f : 0.18f));
            g.drawEllipse (juce::Rectangle<float> (2 * r, 2 * r).withCentre (c), hover == l ? 1.6f : 1.0f);
            const float ang = v.phase[(size_t) l] * juce::MathConstants<float>::twoPi;
            const auto p = c.getPointOnCircumference (r, ang);
            const float act = juce::jlimit (0.0f, 1.0f, v.activity[(size_t) l]);
            const float s = 6.0f + 7.0f * act;
            g.setColour (colr.withAlpha (0.12f + 0.2f * act)); g.fillEllipse (juce::Rectangle<float> (s * 3.0f, s * 3.0f).withCentre (p));
            g.setColour (colr); g.fillEllipse (juce::Rectangle<float> (s, s).withCentre (p));
            planetPos[(size_t) l] = p;
        }
        // Chispas.
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        for (auto& sp : sparksLive)
        {
            const float age = (float) (now - sp.born), t = age / sp.life;
            if (t >= 1.0f) continue;
            const int idx = orbitIndexOf (sp.lane);
            if (idx < 0) continue;
            const float r = orbitRadius (idx, n) + sp.dr;
            const auto p = c.getPointOnCircumference (r, sp.ang);
            const float a = (1.0f - t) * (1.0f - t);
            const float s = sp.size * (1.0f + 0.6f * t);
            g.setColour (col::lane (sp.lane).withAlpha (0.25f * a)); g.fillEllipse (juce::Rectangle<float> (s * 2.6f, s * 2.6f).withCentre (p));
            g.setColour (col::lane (sp.lane).withAlpha (0.9f * a)); g.fillEllipse (juce::Rectangle<float> (s, s).withCentre (p));
        }
        // Sol, respiración y avance del acorde.
        const float sunR = R * 0.15f;
        if (proc.value ("breath") > 0.5f)
        {
            const float br = sunR * (1.35f + 0.35f * v.breath);
            g.setColour (col::sun.withAlpha (0.10f + 0.1f * v.breath)); g.fillEllipse (juce::Rectangle<float> (2 * br, 2 * br).withCentre (c));
        }
        juce::ColourGradient gr (col::sun.brighter (0.25f), c.x, c.y, col::sun.withAlpha (0.0f), c.x + sunR * 1.9f, c.y, true);
        g.setGradientFill (gr); g.fillEllipse (juce::Rectangle<float> (sunR * 3.8f, sunR * 3.8f).withCentre (c));
        g.setColour (col::sun); g.fillEllipse (juce::Rectangle<float> (2 * sunR, 2 * sunR).withCentre (c));
        if (v.running)
        {
            juce::Path arc; arc.addCentredArc (c.x, c.y, sunR + 7, sunR + 7, 0, 0.0f, juce::MathConstants<float>::twoPi * v.chordPhase, true);
            g.setColour (col::sun.withAlpha (0.6f)); g.strokePath (arc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        g.setColour (col::bg);
        g.setFont (font (sunR * 0.55f, true));
        g.drawText (v.running ? juce::String (v.chord) : juce::String ("rest"), juce::Rectangle<float> (2 * sunR, sunR).withCentre (c.translated (0, -sunR * 0.12f)).toNearestInt(), juce::Justification::centred);
        g.setFont (font (std::max (10.0f, sunR * 0.22f)));
        const auto keyName = juce::String (noteName ((int) proc.value ("key"))) + " " + juce::String (scales()[(size_t) juce::jlimit (0, (int) scales().size() - 1, (int) proc.value ("scale"))].name).upToFirstOccurrenceOf (" (", false, false).toLowerCase();
        g.drawFittedText (keyName, juce::Rectangle<float> (1.6f * sunR, sunR * 0.5f).withCentre (c.translated (0, sunR * 0.42f)).toNearestInt(), juce::Justification::centred, 1, 0.6f);
        if (hover >= 0)
        {
            g.setColour (col::text); g.setFont (font (13.0f));
            g.drawText ("Lane " + juce::String (hover + 1) + "  " + roleName ((int) proc.value ("l" + juce::String (hover + 1) + "_role")) + "  ch " + juce::String ((int) proc.value ("l" + juce::String (hover + 1) + "_channel")),
                        getLocalBounds().removeFromBottom (24), juce::Justification::centred);
        }
    }
    void mouseMove (const juce::MouseEvent& e) override { const int h = laneAt (e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit (const juce::MouseEvent&) override { hover = -1; showHelp ({}); repaint(); }
    void mouseEnter (const juce::MouseEvent&) override { showHelp ("The orrery: one orbit per lane, each planet travels at its lane's loop length. Click an orbit to edit that lane."); }
    void mouseUp (const juce::MouseEvent& e) override { const int h = laneAt (e.position); if (h >= 0 && onLane) onLane (h); }

private:
    OrreryProcessor& proc;
    struct StarLayer : juce::Component { std::function<void (juce::Graphics&)> draw; void paint (juce::Graphics& g) override { if (draw) draw (g); } } stars;
    struct LiveSpark { int lane; float ang, dr, size, life; double born; };
    std::vector<LiveSpark> sparksLive;
    std::array<juce::Point<float>, kLanes> planetPos {};
    int hover = -1;
    juce::Random rnd { 7 };

    juce::Point<float> centre() const { return getLocalBounds().toFloat().getCentre(); }
    float radius() const { return std::min (getWidth(), getHeight()) * 0.5f - 18.0f; }
    bool laneOn (int l) const { return proc.value ("l" + juce::String (l + 1) + "_on") > 0.5f; }
    int activeCount() const { int n = 0; for (int l = 0; l < kLanes; ++l) n += laneOn (l) ? 1 : 0; return n; }
    int orbitIndexOf (int lane) const { int k = 0; for (int l = 0; l < kLanes; ++l) { if (! laneOn (l)) continue; if (l == lane) return k; ++k; } return -1; }
    float orbitRadius (int k, int n) const { const float R = radius(); return n <= 1 ? R * 0.6f : R * (0.34f + 0.64f * (float) k / (float) (n - 1)); }
    int laneAt (juce::Point<float> p) const
    {
        const float d = p.getDistanceFrom (centre());
        const int n = activeCount(); int k = 0;
        for (int l = 0; l < kLanes; ++l) { if (! laneOn (l)) continue; if (std::abs (d - orbitRadius (k, n)) < 10.0f) return l; ++k; }
        return -1;
    }
    void paintStars (juce::Graphics& g)
    {
        g.fillAll (col::bg);
        juce::Random r (42);
        const auto b = getLocalBounds().toFloat();
        for (int i = 0; i < 260; ++i)
        {
            const float x = r.nextFloat() * b.getWidth(), y = r.nextFloat() * b.getHeight(), s = 0.6f + r.nextFloat() * 1.4f;
            g.setColour (juce::Colours::white.withAlpha (0.05f + 0.25f * r.nextFloat() * r.nextFloat()));
            g.fillEllipse (x, y, s, s);
        }
        juce::ColourGradient vg (col::select.withAlpha (0.07f), b.getCentreX(), b.getCentreY(), col::bg.withAlpha (0.0f), b.getCentreX() + radius(), b.getCentreY(), true);
        g.setGradientFill (vg); g.fillEllipse (juce::Rectangle<float> (radius() * 2, radius() * 2).withCentre (b.getCentre()));
    }
    void timerCallback() override
    {
        // Chispas nuevas desde el procesador.
        const auto r = proc.sparkFifo.read (proc.sparkFifo.getNumReady());
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        auto take = [&] (int i) {
            const auto& s = proc.sparks[(size_t) i];
            if (s.lane < 0 || s.lane >= kLanes) return;
            const float ang = proc.view().phase[(size_t) s.lane] * juce::MathConstants<float>::twoPi;
            sparksLive.push_back ({ s.lane, ang + (rnd.nextFloat() - 0.5f) * 0.06f, ((float) (s.note % 24) - 12.0f) * 0.6f, 3.0f + 5.0f * (float) s.vel / 127.0f,
                                    2.0f + 3.0f * (float) s.vel / 127.0f, now });
        };
        for (int i = 0; i < r.blockSize1; ++i) take (r.startIndex1 + i);
        for (int i = 0; i < r.blockSize2; ++i) take (r.startIndex2 + i);
        sparksLive.erase (std::remove_if (sparksLive.begin(), sparksLive.end(), [now] (const LiveSpark& s) { return now - s.born > s.life; }), sparksLive.end());
        if (sparksLive.size() > 400) sparksLive.erase (sparksLive.begin(), sparksLive.begin() + (long) (sparksLive.size() - 400));
        repaint();
    }
};

} // namespace dh::orr::ui
