#pragma once
// La máquina vista desde arriba (docs/research.md §4.2). Todo lo que se mueve sale de las mismas señales que el
// audio: los carretes giran con la velocidad del transporte y su radio sigue la bobina, la cinta corre con su
// desgaste, y cada caída es una mancha que llega a la cabeza de reproducción justo cuando se oye (las caídas son un
// hash de la posición, así que la vista las calcula antes de que lleguen). El vinilo muestra el brazo avanzando por
// el surco y las rayas en su ángulo; la radio, el dial y la señal; el teléfono, la línea. A la derecha, la ventana
// de servicio: el VU de grabación, el lazo B-H de la histéresis en vivo y la lectura de wow y flutter.

#include "LookTn.h"
#include "../PluginProcessor.h"

namespace dh::tn::ui {

class DeckView : public juce::Component
{
public:
    explicit DeckView (TarnishProcessor& p) : proc (p) {}

    // Con la ventana escondida no se anima: sólo se vacía la cola de eventos (si no, al volver destellaban todos los
    // viejos) y el reloj vuelve a empezar.
    void idle()
    {
        auto& v = proc.view;
        v.eventFifo.finishedRead (v.eventFifo.getNumReady());
        flashes.clear(); lastT = 0;
    }

    void tick()
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
        const double dt = lastT > 0 ? juce::jlimit (0.0, 0.1, now - lastT) : 0.0;
        lastT = now;
        auto& v = proc.view;
        const double fs = proc.sampleRate();
        // Posición extrapolada entre bloques de audio (para que la cinta corra suave).
        const double pAudio = v.pos.load();
        if (std::abs (pAudio - lastAudioPos) > 0.5) { lastAudioPos = pAudio; audioPosT = now; }
        nowPos = pAudio + (now - audioPosT) * fs * (v.stopped.load() ? 0.0 : 1.0);
        machine = v.machine.load();
        prof = profile (machine, (int) proc.value (params::variantId (machine)));
        speed = v.speed.load();
        // Carretes: ω = v / r, integrado con la velocidad real (así frenan con el tape stop).
        const double vT = prof.magnetic ? prof.speed : 0.0;
        if (prof.magnetic)
        {
            angS -= dt * speed * vT / std::max (1e-3, (double) v.reelS.load());
            angT -= dt * speed * vT / std::max (1e-3, (double) v.reelT.load());
            capAng -= dt * speed * vT / (0.5 * prof.capstanD);
            tapeM += dt * speed * vT;
        }
        else if (machine == kVinyl) platAng += dt * speed * prof.speed / 60.0 * juce::MathConstants<double>::twoPi;
        vuSm += ((double) v.vu.load() - vuSm) * juce::jlimit (0.0, 1.0, dt / 0.3);
        // Eventos (pops, rayas, estática) para los destellos.
        {
            int s1, n1, s2, n2; v.eventFifo.prepareToRead (v.eventFifo.getNumReady(), s1, n1, s2, n2);
            for (int i = 0; i < n1 + n2; ++i)
            {
                const auto& e = v.events[(size_t) (i < n1 ? s1 + i : s2 + i - n1)];
                if (e.kind >= 1) flashes.push_back ({ now, e.kind, e.depth });
            }
            v.eventFifo.finishedRead (n1 + n2);
            while (! flashes.empty() && now - flashes.front().t > 0.6) flashes.erase (flashes.begin());
        }
        predictDropouts (fs);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto b = getLocalBounds().toFloat();
        // Mesa de trabajo y la placa de la máquina.
        g.setColour (col::bg); g.fillRect (b);
        const auto deck = b.withTrimmedRight (270.0f).reduced (10.0f, 10.0f);
        juce::ColourGradient dg (col::deck, deck.getX(), deck.getY(), col::deck2, deck.getRight(), deck.getBottom(), false);
        g.setGradientFill (dg); g.fillRoundedRectangle (deck, 10.0f);
        g.setColour (col::line); g.drawRoundedRectangle (deck, 10.0f, 1.0f);
        // Textura del cepillado: líneas horizontales muy tenues.
        g.setColour (juce::Colours::white.withAlpha (0.018f));
        for (float y = deck.getY() + 3; y < deck.getBottom(); y += 3.0f) g.drawHorizontalLine ((int) y, deck.getX() + 4, deck.getRight() - 4);
        switch (machine)
        {
            case kStudio: paintStudio (g, deck); break;
            case kCassette: case kVhs: paintCassette (g, deck, machine == kVhs); break;
            case kVinyl: paintVinyl (g, deck); break;
            case kRadio: paintRadio (g, deck); break;
            default: paintPhone (g, deck); break;
        }
        paintService (g, b.withLeft (b.getRight() - 270.0f).reduced (6.0f, 10.0f));
    }

private:
    TarnishProcessor& proc;
    Profile prof = profile (0, 1);
    int machine = 0;
    double lastT = 0, lastAudioPos = -1, audioPosT = 0, nowPos = 0, speed = 1, vuSm = 0;
    double angS = 0, angT = 0, capAng = 0, platAng = 0, tapeM = 0;
    struct Flash { double t; int kind; float v; };
    std::vector<Flash> flashes;
    struct Drop { int64_t pos; double len, depth; };
    std::vector<Drop> drops; int64_t predictedTo = -1; uint64_t predictKey = 0;

    // Caídas de la ventana [ahora − 1 s, ahora + 2 s]: la misma función que usa el audio.
    void predictDropouts (double fs)
    {
        const auto ep = proc.currentEngineParams();
        double rate, dmg, depth; uint64_t seed;
        Engine::dropoutSettings (ep, prof, rate, dmg, depth, seed);
        const uint64_t key = (uint64_t) (rate * 1e6) ^ ((uint64_t) (dmg * 1e6) << 20) ^ ((uint64_t) machine << 50);
        const int64_t from = (int64_t) (nowPos - fs), to = (int64_t) (nowPos + 2.0 * fs);
        if (key != predictKey || predictedTo < from || predictedTo > to + (int64_t) fs) { drops.clear(); predictedTo = from; predictKey = key; }
        if (rate > 0)
            for (int64_t p = std::max (predictedTo, from); p < to; ++p)
                if (Dropouts::fires (seed, p, rate, dmg, fs))
                {
                    const auto sh = Dropouts::shape (seed, p, fs, depth);
                    drops.push_back ({ p, (sh.rise + sh.hold + sh.fall) / fs, sh.depth });
                }
        predictedTo = std::max (predictedTo, to);
        drops.erase (std::remove_if (drops.begin(), drops.end(), [&] (const Drop& d) { return (double) d.pos < nowPos - fs; }), drops.end());
    }

    // ── Carrete visto desde arriba: bobina oscura con anillos y una brida metálica con tres ventanas ──
    void paintReel (juce::Graphics& g, juce::Point<float> c, float flangeR, float hubR, float packR, double ang)
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillEllipse (c.x - flangeR + 4, c.y - flangeR + 7, flangeR * 2, flangeR * 2);
        g.setColour (juce::Colour (0xff140e0b)); g.fillEllipse (c.x - flangeR, c.y - flangeR, flangeR * 2, flangeR * 2);    // lo que se ve por las ventanas
        // Bobina: marrón oscuro satinado con anillos finos.
        juce::ColourGradient pg (juce::Colour (0xff4a3426), c.x - packR, c.y - packR, juce::Colour (0xff2c1f17), c.x + packR, c.y + packR, false);
        g.setGradientFill (pg); g.fillEllipse (c.x - packR, c.y - packR, packR * 2, packR * 2);
        g.setColour (juce::Colour (0xff6b4c38)); g.drawEllipse (c.x - packR, c.y - packR, packR * 2, packR * 2, 1.2f);
        for (float r = hubR + 3; r < packR; r += 2.6f) { g.setColour (col::tapeSheen.withAlpha (0.10f + 0.06f * std::sin (r * 0.7f))); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.7f); }
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.0f), c.x - packR, c.y - packR, juce::Colours::white.withAlpha (0.08f), c.x + packR * 0.3f, c.y - packR * 0.3f, false);
        g.setGradientFill (sheen); g.fillEllipse (c.x - packR, c.y - packR, packR * 2, packR * 2);
        // Brida con tres ventanas (relleno par-impar: las ventanas dejan ver la bobina).
        juce::Path fl; fl.setUsingNonZeroWinding (false);
        fl.addEllipse (c.x - flangeR, c.y - flangeR, flangeR * 2, flangeR * 2);
        for (int k = 0; k < 3; ++k)
        {
            const float a0 = (float) ang + juce::MathConstants<float>::twoPi * (float) k / 3.0f + 0.25f, a1 = a0 + 1.45f;
            juce::Path w; const float ri = hubR + 10, ro = flangeR - 12;
            w.addCentredArc (c.x, c.y, ro, ro, 0, a0, a1, true);
            w.addCentredArc (c.x, c.y, ri, ri, 0, a1, a0, false);
            w.closeSubPath();
            fl.addPath (w);
        }
        juce::ColourGradient mg (col::metal.withAlpha (0.92f), c.x - flangeR, c.y - flangeR, col::metalDark.withAlpha (0.92f), c.x + flangeR, c.y + flangeR, false);
        g.setGradientFill (mg); g.fillPath (fl);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.strokePath (fl, juce::PathStrokeType (1.0f));
        // Cubo con tres rayos.
        g.setColour (col::cream.darker (0.1f)); g.fillEllipse (c.x - hubR, c.y - hubR, hubR * 2, hubR * 2);
        g.setColour (col::metalDark);
        for (int k = 0; k < 3; ++k)
        {
            const float a = (float) ang + juce::MathConstants<float>::twoPi * (float) k / 3.0f;
            g.drawLine (c.x + std::cos (a) * hubR * 0.3f, c.y + std::sin (a) * hubR * 0.3f, c.x + std::cos (a) * hubR * 0.9f, c.y + std::sin (a) * hubR * 0.9f, 3.0f);
        }
        g.setColour (col::bg); g.fillEllipse (c.x - 5, c.y - 5, 10, 10);
    }

    // La franja de cinta que pasa por las cabezas: desgaste longitudinal, óxido desprendido y las caídas que vienen.
    void paintTapeBand (juce::Graphics& g, juce::Rectangle<float> band, float headX, double pxPerSec)
    {
        g.setColour (col::tape); g.fillRect (band);
        g.setColour (col::tapeSheen.withAlpha (0.5f)); g.drawHorizontalLine ((int) band.getY() + 1, band.getX(), band.getRight());
        const double wear = proc.value ("wear") / 100.0;
        // Rayas longitudinales: quedan quietas (la cinta corre a lo largo de ellas).
        for (int k = 0; k < (int) (wear * 9.0); ++k)
        {
            const float y = band.getY() + 2.0f + (band.getHeight() - 4.0f) * (float) hashUni (7, (uint64_t) k, 1);
            g.setColour (col::creamFaint.withAlpha (0.18f + 0.25f * (float) hashUni (7, (uint64_t) k, 2)));
            g.drawHorizontalLine ((int) y, band.getX(), band.getRight());
        }
        // Manchas de óxido que viajan con la cinta.
        const double pxPerM = pxPerSec / std::max (1e-4, prof.speed);
        const double off = tapeM * pxPerM;
        const int nSpots = (int) (wear * 40.0);
        for (int k = 0; k < nSpots; ++k)
        {
            const double period = band.getWidth() * 3.0;
            const double base = hashUni (11, (uint64_t) k, 1) * period;
            const double x = band.getX() + std::fmod (base + off, period) - period * 0.33;
            if (x < band.getX() || x > band.getRight()) continue;
            const float y = band.getY() + band.getHeight() * (float) hashUni (11, (uint64_t) k, 2);
            g.setColour (col::creamFaint.withAlpha (0.35f)); g.fillEllipse ((float) x, y - 1.0f, 2.5f + 3.0f * (float) hashUni (11, (uint64_t) k, 3), 1.8f);
        }
        // Caídas: una mancha clara que llega a la cabeza de reproducción cuando se oye.
        const double fs = proc.sampleRate();
        for (const auto& d : drops)
        {
            const double dtS = ((double) d.pos - nowPos) / fs;
            const float x0 = headX - (float) (dtS * pxPerSec * std::max (0.0, speed));
            const float len = std::max (4.0f, (float) (d.len * pxPerSec));
            const float a = juce::jlimit (0.25f, 0.95f, (float) (d.depth * 3e5));
            juce::Rectangle<float> r (x0 - len, band.getY() + 1.0f, len, band.getHeight() - 2.0f);
            if (r.getRight() < band.getX() || r.getX() > band.getRight()) continue;
            g.setColour (col::amber.withAlpha (a * 0.85f)); g.fillRoundedRectangle (r.getIntersection (band), 2.0f);
        }
    }

    void paintHeads (juce::Graphics& g, juce::Rectangle<float> area, float& playX)
    {
        const float w = area.getWidth() / 3.0f;
        const double ref = 0.0884;   // RMS de un seno de −18 dBFS
        const double lvDb = vuSm > 0 ? 10.0 * std::log10 (vuSm) - 20.0 * std::log10 (ref) : -60.0;
        const char* names[] = { "erase", "record", "play" };
        for (int i = 0; i < 3; ++i)
        {
            auto r = juce::Rectangle<float> (area.getX() + w * (float) i, area.getY(), w, area.getHeight()).reduced (6.0f, 0.0f);
            juce::ColourGradient mg (col::metal, r.getX(), r.getY(), col::metalDark, r.getRight(), r.getBottom(), false);
            g.setGradientFill (mg); g.fillRoundedRectangle (r, 4.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.drawRoundedRectangle (r, 4.0f, 1.0f);
            // La ranura: la de reproducción brilla con el nivel y se pone roja cuando la cinta satura.
            juce::Colour gap = col::bg;
            if (i == 2)
            {
                const float k = juce::jlimit (0.0f, 1.0f, (float) ((lvDb + 24.0) / 30.0));
                gap = col::teal.interpolatedWith (col::red, juce::jlimit (0.0f, 1.0f, (float) ((lvDb - 3.0) / 6.0))).withAlpha (0.3f + 0.7f * k);
                playX = r.getCentreX();
            }
            g.setColour (gap); g.fillRect (r.getCentreX() - 1.0f, r.getY() + 1.0f, 2.0f, r.getHeight() * 0.6f);
            g.setColour (col::creamFaint); g.setFont (regular (12.0f));
            g.drawText (names[i], r.withY (r.getBottom() + 2).withHeight (14), juce::Justification::centred);
        }
    }

    void paintStudio (juce::Graphics& g, juce::Rectangle<float> d)
    {
        const float R = juce::jmin (d.getHeight() * 0.40f, d.getWidth() * 0.21f);
        const juce::Point<float> cS (d.getX() + d.getWidth() * 0.24f, d.getY() + R + 14), cT (d.getX() + d.getWidth() * 0.76f, d.getY() + R + 14);
        const float hub = R * 0.36f;
        auto packPx = [&] (float r) { return juce::jmap (r, (float) prof.hubR, (float) prof.fullR, hub + 2.0f, R - 6.0f); };
        const float pS = packPx (proc.view.reelS.load()), pT = packPx (proc.view.reelT.load());
        // Recorrido de la cinta: bobina → rodillo de tensión → cabezas → cabrestante → rodillo → bobina.
        const float yH = d.getBottom() - 46.0f;
        const float wob = juce::jlimit (-7.0f, 7.0f, (float) ((speed - 1.0) * 2500.0));
        const juce::Point<float> g1 (d.getX() + d.getWidth() * 0.37f, yH - 2 + wob), g2 (d.getX() + d.getWidth() * 0.66f, yH - 2), cap (d.getX() + d.getWidth() * 0.60f, yH + 4);
        auto tangent = [] (juce::Point<float> c, float r, juce::Point<float> to, bool cw) {
            const auto dv = to - c; const float dist = dv.getDistanceFromOrigin();
            const float base = std::atan2 (dv.y, dv.x), off = std::acos (juce::jlimit (-1.0f, 1.0f, r / dist));
            const float a = cw ? base + off : base - off;
            return juce::Point<float> (c.x + r * std::cos (a), c.y + r * std::sin (a));
        };
        juce::Path tape;
        tape.startNewSubPath (tangent (cS, pS, g1, true)); tape.lineTo (g1); tape.lineTo (g2.x - (g2.x - g1.x) * 0.0f, yH);
        tape.lineTo (g2); tape.lineTo (tangent (cT, pT, g2, false));
        g.setColour (col::tape); g.strokePath (tape, juce::PathStrokeType (5.0f));
        paintReel (g, cS, R, hub, pS, angS);
        paintReel (g, cT, R, hub, pT, angT);
        // Rodillos guía (el de la izquierda es el brazo de tensión: se mueve con el wow).
        for (auto p : { g1, g2 })
        {
            g.setColour (col::metal); g.fillEllipse (p.x - 9, p.y - 9, 18, 18);
            g.setColour (col::metalDark); g.drawEllipse (p.x - 9, p.y - 9, 18, 18, 1.0f); g.fillEllipse (p.x - 3, p.y - 3, 6, 6);
        }
        g.setColour (col::metalDark); g.drawLine (g1.x, g1.y, g1.x - 40, g1.y - 34, 4.0f);
        // Franja de cinta ampliada sobre las cabezas.
        auto band = juce::Rectangle<float> (g1.x + 12, yH - 9, g2.x - g1.x - 24, 18);
        auto heads = juce::Rectangle<float> (band.getX() + 10, yH + 6, band.getWidth() * 0.62f, 30);
        float playX = heads.getRight();
        paintHeads (g, heads, playX);                       // las cabezas, debajo; la cinta pasa por su cara
        paintTapeBand (g, band, playX, 90.0 / 0.381 * prof.speed);
        // Cabrestante y rodillo de presión.
        g.setColour (col::metal); g.fillEllipse (cap.x - 7, cap.y - 7, 14, 14);
        g.setColour (col::bg); g.drawLine (cap.x, cap.y, cap.x + 6 * (float) std::cos (capAng), cap.y + 6 * (float) std::sin (capAng), 2.0f);
        g.setColour (juce::Colour (0xff141010)); g.fillEllipse (cap.x - 11, cap.y + 7, 22, 22);
        g.setColour (col::creamFaint); g.setFont (regular (12.0f));
        g.drawText ("supply", juce::Rectangle<float> (cS.x - 60, cS.y + R + 2, 120, 14), juce::Justification::centred);
        g.drawText ("take-up", juce::Rectangle<float> (cT.x - 60, cT.y + R + 2, 120, 14), juce::Justification::centred);
    }

    void paintCassette (juce::Graphics& g, juce::Rectangle<float> d, bool video)
    {
        auto shell = d.withSizeKeepingCentre (video ? d.getWidth() * 0.84f : d.getWidth() * 0.66f, d.getHeight() * 0.66f).translated (0, -34);
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillRoundedRectangle (shell.translated (4, 7), 16.0f);
        g.setColour (video ? juce::Colour (0xff141210) : juce::Colour (0xff2a2420)); g.fillRoundedRectangle (shell, 16.0f);
        g.setColour (col::line); g.drawRoundedRectangle (shell, 16.0f, 1.2f);
        // Etiqueta crema con renglones y un trazo a mano (sin texto ni marca).
        auto label = shell.reduced (shell.getWidth() * 0.06f, 0).withTrimmedTop (16).withHeight (shell.getHeight() * (video ? 0.34f : 0.6f));
        g.setColour (juce::Colour (0xffe8ddc4)); g.fillRoundedRectangle (label, 6.0f);
        g.setColour (col::teal.withAlpha (0.35f));
        for (int i = 1; i < 4; ++i) g.drawHorizontalLine ((int) (label.getY() + label.getHeight() * (0.22f + 0.17f * (float) i)), label.getX() + 14, label.getRight() - 14);
        juce::Path hand; const float hx = label.getX() + 26, hy = label.getY() + label.getHeight() * 0.3f;
        hand.startNewSubPath (hx, hy);
        for (int k = 1; k < 28; ++k) hand.lineTo (hx + (float) k * 9.0f, hy + 5.0f * std::sin ((float) k * 1.3f) + 2.0f * std::sin ((float) k * 0.37f));
        g.setColour (juce::Colour (0xff2b2a5a).withAlpha (0.75f)); g.strokePath (hand, juce::PathStrokeType (1.6f));
        // Ventana con los dos cubos y las bobinas.
        auto win = juce::Rectangle<float> (0, 0, shell.getWidth() * (video ? 0.5f : 0.46f), shell.getHeight() * 0.24f)
                       .withCentre ({ shell.getCentreX(), video ? shell.getCentreY() + 10 : label.getY() + label.getHeight() * 0.62f });
        g.setColour (juce::Colour (0xff3b322b)); g.fillRoundedRectangle (win, 8.0f);
        const float hubR = win.getHeight() * 0.22f, maxR = win.getHeight() * 0.9f;
        auto packPx = [&] (float r) { return juce::jmap (r, (float) prof.hubR, (float) prof.fullR, hubR + 1, maxR); };
        const juce::Point<float> cS (win.getX() + win.getWidth() * 0.22f, win.getCentreY()), cT (win.getRight() - win.getWidth() * 0.22f, win.getCentreY());
        g.saveState(); g.reduceClipRegion (win.toNearestInt());
        for (int k = 0; k < 2; ++k)
        {
            const auto c = k == 0 ? cS : cT; const float pr = packPx (k == 0 ? proc.view.reelS.load() : proc.view.reelT.load());
            g.setColour (juce::Colour (0xff4a3426)); g.fillEllipse (c.x - pr, c.y - pr, pr * 2, pr * 2);
            for (float r = hubR + 2; r < pr; r += 2.2f) { g.setColour (col::tapeSheen.withAlpha (0.14f)); g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.6f); }
        }
        g.restoreState();
        for (int k = 0; k < 2; ++k)
        {
            const auto c = k == 0 ? cS : cT; const double a = k == 0 ? angS : angT;
            g.setColour (juce::Colour (0xfff2ede2)); g.fillEllipse (c.x - hubR, c.y - hubR, hubR * 2, hubR * 2);
            g.setColour (col::bg);
            for (int t = 0; t < 6; ++t)
            {
                const float aa = (float) a + juce::MathConstants<float>::twoPi * (float) t / 6.0f;
                g.fillEllipse (c.x + std::cos (aa) * hubR * 0.62f - 2.0f, c.y + std::sin (aa) * hubR * 0.62f - 2.0f, 4.0f, 4.0f);
            }
        }
        g.setColour (col::line); g.drawRoundedRectangle (win, 8.0f, 1.0f);
        // Tornillos.
        for (auto p : { shell.getTopLeft() + juce::Point<float> (12, 12), shell.getTopRight() + juce::Point<float> (-12, 12),
                        shell.getBottomLeft() + juce::Point<float> (12, -12), shell.getBottomRight() + juce::Point<float> (-12, -12) })
        {
            g.setColour (col::metalDark); g.fillEllipse (p.x - 4, p.y - 4, 8, 8);
            g.setColour (col::bg); g.drawLine (p.x - 3, p.y, p.x + 3, p.y, 1.2f);
        }
        // Cinta a lo largo del frente, con la cabeza, el cabrestante y el rodillo.
        const float yT = shell.getBottom() + 26;
        auto band = juce::Rectangle<float> (shell.getX() + 20, yT - 9, shell.getWidth() - 40, 18);
        auto heads = juce::Rectangle<float> (band.getCentreX() - 70, yT + 12, 140, 30);
        g.setColour (col::tape); g.drawLine (shell.getX() + 26, shell.getBottom() - 4, band.getX(), yT, 3.0f); g.drawLine (shell.getRight() - 26, shell.getBottom() - 4, band.getRight(), yT, 3.0f);
        float playX = band.getCentreX();
        paintTapeBand (g, band, playX + 22, (video ? 70.0 : 60.0) / 0.0476 * prof.speed);
        paintHeads (g, heads, playX);
        const juce::Point<float> cap (heads.getRight() + 30, yT + 6);
        g.setColour (col::metal); g.fillEllipse (cap.x - 5, cap.y - 5, 10, 10);
        g.setColour (juce::Colour (0xff141010)); g.fillEllipse (cap.x + 6, cap.y - 9, 18, 18);
    }

    void paintVinyl (juce::Graphics& g, juce::Rectangle<float> d)
    {
        const float R = juce::jmin (d.getHeight() * 0.46f, d.getWidth() * 0.32f);
        const juce::Point<float> c (d.getX() + d.getWidth() * 0.40f, d.getCentreY());
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillEllipse (c.x - R - 6 + 4, c.y - R - 6 + 7, (R + 6) * 2, (R + 6) * 2);
        g.setColour (col::metalDark); g.fillEllipse (c.x - R - 6, c.y - R - 6, (R + 6) * 2, (R + 6) * 2);
        g.setColour (juce::Colour (0xff0e0c0b)); g.fillEllipse (c.x - R, c.y - R, R * 2, R * 2);
        // Surcos: anillos tenues; el tramo ya tocado (afuera del brazo) un poco más brillante.
        const float lab = R * 0.36f;
        const float g0 = (float) proc.view.groove.load();
        const float stylusR = juce::jmap (g0, R * 0.97f, lab + R * 0.08f);
        for (float r = lab + 4; r < R - 2; r += 1.8f)
        {
            g.setColour (juce::Colours::white.withAlpha (r > stylusR ? 0.055f : 0.03f));
            g.drawEllipse (c.x - r, c.y - r, r * 2, r * 2, 0.6f);
        }
        for (int k = 0; k < 2; ++k)            // dos reflejos finos de la luz sobre los surcos (no giran)
        {
            juce::Path sheen; const float a0 = k == 0 ? -1.0f : 2.15f;
            for (float r = lab + 10; r < R - 4; r += 3.0f) sheen.addCentredArc (c.x, c.y, r, r, 0, a0, a0 + 0.35f, true);
            g.setColour (juce::Colours::white.withAlpha (0.05f)); g.strokePath (sheen, juce::PathStrokeType (1.4f));
        }
        // Rayas en su ángulo (giran con el disco) y el polvo.
        const int nScr = (int) proc.value ("scratches");
        for (int k = 0; k < nScr; ++k)
        {
            const float a = (float) (platAng + juce::MathConstants<double>::twoPi * proc.scratchAngle (k)) - juce::MathConstants<float>::halfPi;
            g.setColour (juce::Colours::white.withAlpha (0.35f));
            g.drawLine (c.x + std::cos (a) * (lab + 8), c.y + std::sin (a) * (lab + 8), c.x + std::cos (a + 0.05f) * (R - 6), c.y + std::sin (a + 0.05f) * (R - 6), 1.2f);
        }
        const int dust = (int) (proc.value ("crackle") * 0.6f);
        for (int k = 0; k < dust; ++k)
        {
            const float a = (float) platAng + juce::MathConstants<float>::twoPi * (float) hashUni (3, (uint64_t) k, 1);
            const float r = lab + (R - lab) * (float) hashUni (3, (uint64_t) k, 2);
            g.setColour (col::cream.withAlpha (0.5f)); g.fillEllipse (c.x + std::cos (a) * r, c.y + std::sin (a) * r, 1.8f, 1.8f);
        }
        // Etiqueta crema sin texto, con una marca que deja ver el giro.
        g.setColour (juce::Colour (0xffe8ddc4)); g.fillEllipse (c.x - lab, c.y - lab, lab * 2, lab * 2);
        g.setColour (col::teal.withAlpha (0.7f));
        g.drawLine (c.x + std::cos ((float) platAng) * lab * 0.35f, c.y + std::sin ((float) platAng) * lab * 0.35f, c.x + std::cos ((float) platAng) * lab * 0.85f, c.y + std::sin ((float) platAng) * lab * 0.85f, 3.0f);
        g.setColour (col::metal); g.fillEllipse (c.x - 4, c.y - 4, 8, 8);
        // Brazo: pivote arriba a la derecha, la púa en el radio del surco.
        const juce::Point<float> piv (c.x + R * 1.15f, c.y - R * 0.75f);
        const float armLen = piv.getDistanceFrom (c) * 0.98f;
        // Ángulo del brazo para que la púa caiga a stylusR del centro (intersección de dos círculos).
        const float dd = piv.getDistanceFrom (c);
        const float aa = (armLen * armLen + dd * dd - stylusR * stylusR) / (2 * armLen * dd);
        const float base = std::atan2 (c.y - piv.y, c.x - piv.x);
        const float wob = juce::jlimit (-0.01f, 0.01f, (float) ((speed - 1.0) * 3.0));
        const float ang = base + std::acos (juce::jlimit (-1.0f, 1.0f, aa)) + wob;
        const juce::Point<float> tip (piv.x + std::cos (ang) * armLen, piv.y + std::sin (ang) * armLen);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.drawLine (piv.x + 4, piv.y + 7, tip.x + 4, tip.y + 7, 6.0f);
        g.setColour (col::metal); g.drawLine (piv.x, piv.y, tip.x, tip.y, 5.0f);
        g.setColour (col::metalDark); g.fillEllipse (piv.x - 16, piv.y - 16, 32, 32);
        g.setColour (col::metal); g.fillEllipse (piv.x - 9, piv.y - 9, 18, 18);
        juce::Path shell; shell.addRoundedRectangle (-10.0f, -6.0f, 22.0f, 12.0f, 2.0f);
        shell.applyTransform (juce::AffineTransform::rotation (ang).translated (tip.x, tip.y));
        g.setColour (col::cream.darker (0.2f)); g.fillPath (shell);
        // Destellos de los pops y las rayas en la púa.
        const double now = lastT;
        for (const auto& f : flashes)
        {
            const float k = 1.0f - (float) ((now - f.t) / 0.25);
            if (k <= 0) continue;
            g.setColour (col::cream.withAlpha (0.8f * k)); g.fillEllipse (tip.x - 6 * k, tip.y - 6 * k, 12 * k, 12 * k);
        }
    }

    void paintRadio (juce::Graphics& g, juce::Rectangle<float> d)
    {
        // Dial crema con la escala de onda media y la aguja que se corre con Tuning.
        auto dial = d.reduced (36, 0).withTrimmedTop (36).withHeight (110);
        g.setColour (juce::Colour (0xffe8ddc4)); g.fillRoundedRectangle (dial, 8.0f);
        g.setColour (juce::Colour (0xff5b4a3c)); g.setFont (regular (13.0f));
        const int marks[] = { 530, 600, 700, 800, 1000, 1200, 1400, 1600 };
        for (int i = 0; i < 8; ++i)
        {
            const float x = dial.getX() + 30 + (dial.getWidth() - 60) * (float) i / 7.0f;
            g.drawLine (x, dial.getY() + 46, x, dial.getY() + 62, 1.0f);
            g.drawText (juce::String (marks[i]), juce::Rectangle<float> (x - 30, dial.getY() + 66, 60, 16), juce::Justification::centred);
        }
        for (int i = 0; i < 57; ++i) { const float x = dial.getX() + 30 + (dial.getWidth() - 60) * (float) i / 56.0f; g.drawLine (x, dial.getY() + 52, x, dial.getY() + 58, 0.6f); }
        g.drawText ("kHz", dial.withTrimmedTop (86).reduced (12, 0), juce::Justification::centredRight);
        const double fs = proc.sampleRate();
        const float tune = proc.value ("tuning") / 100.0f;
        const float drift = 0.006f * (float) valueNoise (17, nowPos, 3.0 * fs, 1);
        const float nx = dial.getX() + 30 + (dial.getWidth() - 60) * (0.55f + 0.08f * tune + drift);
        g.setColour (col::red); g.fillRect (nx - 1.5f, dial.getY() + 10, 3.0f, dial.getHeight() - 20);
        // Señal: el desvanecimiento (la misma fórmula del audio).
        const double fading = proc.value ("fading") / 100.0;
        const double fL = 1.0 - fading * 0.5 * (1.0 + valueNoise (proc.currentEngineParams().seed, nowPos, 1.7 * fs, 31));
        auto meter = juce::Rectangle<float> (d.getX() + 36, dial.getBottom() + 28, 220, 120);
        paintGauge (g, meter, (float) juce::jlimit (0.0, 1.0, fL), "signal");
        // Parlante: rejilla de agujeros que se ilumina con la estática.
        auto grille = juce::Rectangle<float> (meter.getRight() + 40, dial.getBottom() + 20, d.getRight() - meter.getRight() - 76, d.getBottom() - dial.getBottom() - 34);
        g.setColour (juce::Colour (0xff1a1410)); g.fillRoundedRectangle (grille, 10.0f);
        float flash = 0; for (const auto& f : flashes) flash = std::max (flash, 1.0f - (float) ((lastT - f.t) / 0.15));
        const float lv = juce::jlimit (0.0f, 1.0f, (float) std::sqrt (vuSm) * 6.0f);
        for (float y = grille.getY() + 12; y < grille.getBottom() - 6; y += 11)
            for (float x = grille.getX() + 12; x < grille.getRight() - 6; x += 11)
            {
                g.setColour (col::creamFaint.withAlpha (0.25f + 0.35f * lv * (float) hashUni (5, (uint64_t) (x * 7 + y), 1) + 0.4f * juce::jmax (0.0f, flash)));
                g.fillEllipse (x - 2, y - 2, 4, 4);
            }
    }

    void paintPhone (juce::Graphics& g, juce::Rectangle<float> d)
    {
        // Tubo visto de arriba, con el cable enrulado hasta la base y la luz de línea.
        const auto c = d.getCentre().translated (-40, -10);
        juce::Path handset;
        handset.startNewSubPath (c.x - 190, c.y - 30);
        handset.cubicTo (c.x - 120, c.y - 70, c.x + 120, c.y - 70, c.x + 190, c.y - 30);
        handset.cubicTo (c.x + 215, c.y - 18, c.x + 215, c.y + 30, c.x + 185, c.y + 34);
        handset.cubicTo (c.x + 150, c.y + 38, c.x + 140, c.y + 6, c.x + 110, c.y - 6);
        handset.cubicTo (c.x + 40, c.y - 26, c.x - 40, c.y - 26, c.x - 110, c.y - 6);
        handset.cubicTo (c.x - 140, c.y + 6, c.x - 150, c.y + 38, c.x - 185, c.y + 34);
        handset.cubicTo (c.x - 215, c.y + 30, c.x - 215, c.y - 18, c.x - 190, c.y - 30);
        handset.closeSubPath();
        g.setColour (juce::Colours::black.withAlpha (0.4f)); g.fillPath (handset, juce::AffineTransform::translation (4, 7));
        g.setColour (juce::Colour (0xff3b2e26)); g.fillPath (handset);
        g.setColour (col::lineHi); g.strokePath (handset, juce::PathStrokeType (1.2f));
        for (int k = 0; k < 2; ++k)
        {
            const float x = k == 0 ? c.x - 178 : c.x + 178;
            for (int i = 0; i < 9; ++i) { const float a = juce::MathConstants<float>::twoPi * (float) i / 9.0f; g.setColour (col::bg); g.fillEllipse (x + 9 * std::cos (a) - 2, c.y + 10 + 9 * std::sin (a) - 2, 4, 4); }
        }
        juce::Path cord; cord.startNewSubPath (c.x - 185, c.y + 34);
        for (int i = 0; i < 160; ++i)
        {
            const float t = (float) i / 159.0f;
            const float x = c.x - 185 + 240 * t, y = c.y + 34 + 90 * t * t;
            cord.lineTo (x + 7 * std::cos (t * 70.0f), y + 7 * std::sin (t * 70.0f));
        }
        g.setColour (juce::Colour (0xff4a3a30)); g.strokePath (cord, juce::PathStrokeType (3.0f));
        // Luz de línea: se apaga en las tramas perdidas (la misma fórmula del audio).
        const double fs = proc.sampleRate();
        const auto ep = proc.currentEngineParams();
        bool lost = false;
        if (ep.frameLoss > 0)
        {
            const int64_t frame = (int64_t) std::round (0.02 * fs), fr = (int64_t) nowPos / frame;
            const bool bad = valueNoise (ep.seed, (double) fr, 12.0, 41) > 0.55 - 0.9 * ep.frameLoss;
            lost = hashUni (ep.seed, (uint64_t) fr, 42) < (bad ? 0.6 : 0.02) * std::min (1.0, 2.0 * ep.frameLoss);
        }
        const juce::Point<float> lamp (d.getRight() - 90, d.getBottom() - 60);
        g.setColour ((lost ? col::red : col::teal).withAlpha (0.9f)); g.fillEllipse (lamp.x - 10, lamp.y - 10, 20, 20);
        g.setColour (col::creamFaint); g.setFont (regular (12.0f));
        g.drawText ("line", juce::Rectangle<float> (lamp.x - 40, lamp.y + 14, 80, 14), juce::Justification::centred);
    }

    void paintGauge (juce::Graphics& g, juce::Rectangle<float> r, float v, const juce::String& label)
    {
        g.setColour (juce::Colour (0xffe8ddc4)); g.fillRoundedRectangle (r, 8.0f);
        const auto c = juce::Point<float> (r.getCentreX(), r.getBottom() - 14);
        const float R = r.getHeight() * 0.78f;
        juce::Path arc; arc.addCentredArc (c.x, c.y, R, R, 0, -1.0f, 1.0f, true);
        g.setColour (juce::Colour (0xff5b4a3c)); g.strokePath (arc, juce::PathStrokeType (1.2f));
        for (int i = 0; i <= 10; ++i)
        {
            const float a = -1.0f + 2.0f * (float) i / 10.0f;
            g.drawLine (c.x + std::sin (a) * (R - 6), c.y - std::cos (a) * (R - 6), c.x + std::sin (a) * R, c.y - std::cos (a) * R, i % 5 == 0 ? 1.6f : 0.8f);
        }
        const float a = -1.0f + 2.0f * juce::jlimit (0.0f, 1.0f, v);
        g.setColour (col::red); g.drawLine (c.x, c.y, c.x + std::sin (a) * (R - 3), c.y - std::cos (a) * (R - 3), 1.6f);
        g.setColour (juce::Colour (0xff5b4a3c)); g.setFont (regular (12.0f));
        g.drawText (label, r.withTrimmedBottom (r.getHeight() * 0.42f).withTrimmedTop (r.getHeight() * 0.36f), juce::Justification::centred);
    }

    // ── Ventana de servicio: VU, lazo B-H y lecturas ──
    void paintService (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (col::panel); g.fillRoundedRectangle (r, 8.0f);
        g.setColour (col::line); g.drawRoundedRectangle (r, 8.0f, 1.0f);
        auto rr = r.reduced (12.0f, 10.0f);
        // VU de grabación respecto del nivel de referencia de la cinta.
        const double ref = 0.0884;
        const double db = vuSm > 1e-12 ? 10.0 * std::log10 (vuSm) - 20.0 * std::log10 (ref) : -60.0;
        auto vu = rr.removeFromTop (110);
        paintGauge (g, vu, (float) juce::jlimit (0.0, 1.0, (db + 20.0) / 26.0), prof.magnetic ? "record level" : "level");
        g.setColour (col::creamDim); g.setFont (regular (12.0f));
        g.drawText ("0 = reference", vu.withY (vu.getBottom() + 1).withHeight (14), juce::Justification::centredRight);
        rr.removeFromTop (18);
        // Lazo B-H (sólo en las máquinas magnéticas).
        auto bh = rr.removeFromTop (128);
        g.setColour (col::bg); g.fillRoundedRectangle (bh, 6.0f);
        g.setColour (col::line); g.drawHorizontalLine ((int) bh.getCentreY(), bh.getX() + 4, bh.getRight() - 4); g.drawVerticalLine ((int) bh.getCentreX(), bh.getY() + 4, bh.getBottom() - 4);
        if (prof.magnetic)
        {
            const int w = proc.view.scopeWrite.load();
            juce::Path p; bool started = false;
            for (int i = 0; i < TarnishProcessor::ViewState::kScope; ++i)
            {
                const int k = (w + i) % TarnishProcessor::ViewState::kScope;
                const float h = proc.view.scopeH[(size_t) k].load(), m = proc.view.scopeM[(size_t) k].load();
                const float x = bh.getCentreX() + bh.getWidth() * 0.46f * std::tanh (h / 8.0f), y = bh.getCentreY() - bh.getHeight() * 0.44f * m;
                if (! started) { p.startNewSubPath (x, y); started = true; } else p.lineTo (x, y);
            }
            g.setColour (col::tealHi.withAlpha (0.85f)); g.strokePath (p, juce::PathStrokeType (1.3f));
            g.setColour (col::creamFaint); g.setFont (regular (11.5f));
            g.drawText ("H", bh.reduced (6, 2), juce::Justification::bottomRight); g.drawText ("M", bh.reduced (6, 2), juce::Justification::topLeft);
        }
        else { g.setColour (col::creamFaint); g.setFont (regular (13.0f)); g.drawText ("no magnetic tape", bh, juce::Justification::centred); }
        rr.removeFromTop (12);
        // Lecturas.
        auto row = [&] (const juce::String& k, const juce::String& v) {
            auto line = rr.removeFromTop (19);
            g.setColour (col::creamDim); g.setFont (regular (14.0f)); g.drawText (k, line, juce::Justification::centredLeft);
            g.setColour (col::cream); g.setFont (bold (14.0f)); g.drawText (v, line, juce::Justification::centredRight);
        };
        const float wf = proc.view.wf.load();
        if (machine <= kVinyl) row ("Wow and flutter (approx.)", juce::String (wf, wf < 0.1f ? 3 : 2) + " %");
        if (prof.magnetic) row ("Head-to-tape spacing", juce::String (proc.view.spacing.load() * 1e6f, 2) + " " + juce::String::fromUTF8 ("\xc2\xb5m"));
        if (machine == kVinyl) row ("Groove position", juce::String (juce::roundToInt (proc.view.groove.load() * 100)) + " %");
        row ("Speed", juce::String (juce::roundToInt (speed * 100.0)) + " %");
        if (machine == kRadio) row ("Station", std::abs (proc.value ("tuning")) < 0.5f ? juce::String ("on centre") : juce::String ("off centre"));
    }
};

} // namespace dh::tn::ui
