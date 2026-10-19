#pragma once
// Instrumento completo: fondo renderizado, faders, selectores, perillas, botones con LED, palanca
// del bender y teclado de 32 teclas. Todo lo que se mueve son sprites de Blender sobre el fondo.

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Assets101.h"

namespace dh {

class PanelView101 : public juce::Component, private juce::Timer
{
public:
    std::function<int()> keyboardOctave;          // lo lleva la barra inferior

    explicit PanelView101 (AT101VProcessor& p) : proc (p)
    {
        setSize ((int) A.viewWidth(), (int) A.viewHeight());
        buildControls();
        setRepaintsOnMouseActivity (false);
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const int body = bodyColour();
        const auto& bg = A.backgrounds[(size_t) body];
        if (! bg.isValid()) { g.fillAll (col101::body (body)); return; }
        g.drawImage (bg, getLocalBounds().toFloat());
        drawKeys (g, bg);

        // Faders.
        for (auto& c : controls)
        {
            switch (c.kind)
            {
                case Kind::Slider:
                {
                    const float v = proc.panelValue (c.param);
                    const auto pt = A.toView (c.x, c.y0 + (c.y1 - c.y0) * v);
                    A.draw (g, c.green ? A.capGreen : A.capOrange, pt);
                    break;
                }
                case Kind::Switch:
                {
                    const int i = juce::jlimit (0, (int) c.positions.size() - 1, (int) std::lround (proc.panelValue (c.param)));
                    A.draw (g, A.swcap, A.toView (c.x, c.positions[(size_t) i]));
                    break;
                }
                case Kind::Mini:
                {
                    const int i = juce::jlimit (0, (int) c.positions.size() - 1, (int) std::lround (proc.panelValue (c.param)));
                    A.draw (g, A.nub, A.toView (c.positions[(size_t) i], c.y0, 0.9f));
                    break;
                }
                case Kind::Knob:
                {
                    const float ang = knobAngle (c, proc.panelValue (c.param));
                    const int f = ((int) std::lround (ang / 360.0f * (float) A.knob.frames) % A.knob.frames + A.knob.frames) % A.knob.frames;
                    A.draw (g, A.knob, A.toView (c.x, c.y0), f, c.r / 10.0f);
                    break;
                }
                case Kind::Button: break;
            }
        }

        // Botones, LEDs y POWER.
        for (auto& c : controls)
        {
            if (c.kind != Kind::Button) continue;
            if (c.id == "power")
            {
                if (! proc.powerOn.load()) A.draw (g, A.powerDown, A.toView (c.x, c.y0));
            }
            else if (pressedButton == &c) A.draw (g, A.buttonDown, A.toView (c.x, c.y0));
            if (buttonLed (c)) drawLed (g, A.toView (c.ledX, c.ledY, 0.6f));
        }
        if (proc.powerOn.load())
        {
            const auto& rl = A.layout.getProperty ("rateLed", {});
            if (proc.rateLed.load() && rl.isArray()) drawLed (g, A.toView ((float) rl[0], (float) rl[1], 0.6f), 1.15f);
        }

        // Palanca del bender.
        {
            const auto& bd = A.layout.getProperty ("bender", {});
            const float b = proc.bender.load(), push = proc.lfoPush.load();
            const int f = juce::jlimit (0, A.lever.frames - 1, (int) std::lround ((b + 1.0f) * 0.5f * (float) (A.lever.frames - 1)));
            A.draw (g, A.lever, A.toView ((float) bd["cx"], (float) bd["cy"] - 2.2f * push), f);
        }

        drawBubble (g);
    }

    // ───────── ratón ─────────
    void mouseDown (const juce::MouseEvent& e) override
    {
        lastMouse = e.position;
        active = hit (e.position);
        if (e.mods.isPopupMenu() && active != nullptr && active->kind != Kind::Button) { showContextMenu (*active); active = nullptr; return; }
        if (active == nullptr)
        {
            if (inBender (e.position)) { draggingBender = true; benderStart = e.position; return; }
            playAt (e.position);
            return;
        }
        auto& c = *active;
        if (c.kind == Kind::Button) { pressButton (c, e.mods); return; }
        dragStart = proc.panelValue (c.param);
        if (c.kind == Kind::Switch || c.kind == Kind::Mini) setPosition (c, e.position);
        if (c.kind == Kind::Knob && c.steps > 0 && ! e.mods.isShiftDown())
        {
            stepAccum = 0.0f;
        }
        showBubble = true;
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (draggingBender)
        {
            const float w = A.mmToView ((float) A.layout.getProperty ("bender", {})["w"]) * 0.45f;
            proc.bender.store (juce::jlimit (-1.0f, 1.0f, (e.position.x - benderStart.x) / w));
            proc.lfoPush.store (juce::jlimit (0.0f, 1.0f, (benderStart.y - e.position.y) / 24.0f));
            repaint();
            return;
        }
        if (active == nullptr) { if (mouseNote >= 0) playAt (e.position); return; }
        auto& c = *active;
        const float dy = lastMouse.y - e.position.y;
        lastMouse = e.position;
        if (c.kind == Kind::Slider)
        {
            const float travel = std::abs (A.mmToView (c.y1 - c.y0) * A.cosT);
            const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
            setValue (c, juce::jlimit (0.0f, 1.0f, proc.panelValue (c.param) + dy / travel * fine));
        }
        else if (c.kind == Kind::Knob)
        {
            if (c.steps > 0)
            {
                stepAccum += dy;
                if (std::abs (stepAccum) > 14.0f)
                {
                    const int cur = (int) std::lround (proc.panelValue (c.param));
                    setValue (c, (float) juce::jlimit (0, c.steps - 1, cur + (stepAccum > 0 ? 1 : -1)));
                    stepAccum = 0.0f;
                }
            }
            else setValue (c, juce::jlimit (0.0f, 1.0f, proc.panelValue (c.param) + dy / (e.mods.isShiftDown() ? 900.0f : 180.0f)));
        }
        else if (c.kind == Kind::Switch || c.kind == Kind::Mini) setPosition (c, e.position);
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (draggingBender) { draggingBender = false; proc.bender.store (0.0f); proc.lfoPush.store (0.0f); }
        if (pressedButton != nullptr) { pressedButton = nullptr; }
        active = nullptr; showBubble = false;
        releaseMouseNote();
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (auto* c = hit (e.position); c != nullptr && c->kind != Kind::Button)
            setValue (*c, panelParam ((int) c->param).def);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto* c = hit (e.position);
        if (c == nullptr || c->kind == Kind::Button) return;
        const float d = (w.isReversed ? -w.deltaY : w.deltaY);
        const int steps = c->kind == Kind::Knob ? c->steps : (int) c->positions.size();
        if (steps > 0)
        {
            if (std::abs (d) > 0.02f) setValue (*c, (float) juce::jlimit (0, steps - 1, (int) std::lround (proc.panelValue (c->param)) + (d > 0 ? 1 : -1)));
        }
        else setValue (*c, juce::jlimit (0.0f, 1.0f, proc.panelValue (c->param) + d * 0.08f));
        hover = c; showBubble = true; bubbleTimer = 25;
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto* c = hit (e.position);
        setMouseCursor (c != nullptr || inBender (e.position) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        if (c != hover) { hover = c; repaint(); }
    }

private:
    enum class Kind { Slider, Switch, Mini, Knob, Button };
    struct Control
    {
        Kind kind;
        juce::String id;
        P param = P::count;
        float x = 0, y0 = 0, y1 = 0, r = 10;
        bool green = false;
        int steps = 0;
        float a0 = -150, a1 = 150;
        std::vector<float> positions;
        float ledX = 0, ledY = 0;
        juce::Rectangle<float> box;      // zona de clic (vista)
    };

    AT101VProcessor& proc;
    Assets101& A = Assets101::get();
    std::vector<Control> controls;
    Control* active = nullptr;
    Control* hover = nullptr;
    Control* pressedButton = nullptr;
    float dragStart = 0.0f, stepAccum = 0.0f;
    juce::Point<float> lastMouse, benderStart;
    bool draggingBender = false, showBubble = false;
    int bubbleTimer = 0, mouseNote = -1;
    uint64_t shownState = 0;

    int bodyColour() const { return juce::jlimit (0, 2, (int) proc.apvts.getRawParameterValue ("body")->load()); }

    void buildControls()
    {
        const auto& L = A.layout;
        auto pid = [] (const juce::String& id) { const int i = panelIndex (id); return i >= 0 ? (P) i : P::count; };
        if (auto* arr = L.getProperty ("sliders", {}).getArray())
            for (auto& s : *arr)
            {
                Control c { Kind::Slider, s["id"].toString(), pid (s["id"].toString()) };
                c.x = s["x"]; c.y0 = s["y0"]; c.y1 = s["y10"]; c.green = s["color"].toString() == "green";
                const auto a = A.toView (c.x, c.y1, 9.0f), b = A.toView (c.x, c.y0);
                c.box = juce::Rectangle<float> (a.x - 13.0f, a.y - 10.0f, 26.0f, b.y - a.y + 22.0f);
                controls.push_back (c);
            }
        if (auto* arr = L.getProperty ("switches", {}).getArray())
            for (auto& s : *arr)
            {
                Control c { Kind::Switch, s["id"].toString(), pid (s["id"].toString()) };
                c.x = s["x"];
                for (auto& y : *s["ys"].getArray()) c.positions.push_back ((float) y);
                const auto a = A.toView (c.x, c.positions.front() - 5.0f, 6.0f), b = A.toView (c.x, c.positions.back() + 5.0f);
                c.box = juce::Rectangle<float> (a.x - 13.0f, a.y, 26.0f, b.y - a.y);
                controls.push_back (c);
            }
        if (auto* arr = L.getProperty ("mini", {}).getArray())
            for (auto& s : *arr)
            {
                Control c { Kind::Mini, s["id"].toString(), pid (s["id"].toString()) };
                c.y0 = s["y"];
                for (auto& x : *s["xs"].getArray()) c.positions.push_back ((float) x);
                const auto a = A.toView (c.positions.front() - 5.0f, c.y0 - 4.0f), b = A.toView (c.positions.back() + 5.0f, c.y0 + 4.0f);
                c.box = juce::Rectangle<float>::leftTopRightBottom (a.x, a.y - 4.0f, b.x, b.y);
                controls.push_back (c);
            }
        if (auto* arr = L.getProperty ("knobs", {}).getArray())
            for (auto& s : *arr)
            {
                Control c { Kind::Knob, s["id"].toString(), pid (s["id"].toString()) };
                c.x = s["x"]; c.y0 = s["y"]; c.r = s["r"]; c.steps = s["steps"];
                const auto kind = s["kind"].toString();
                if (kind == "wave") { c.a0 = -55; c.a1 = 55; }
                else if (kind == "range") { c.a0 = -50; c.a1 = 50; }
                const auto ctr = A.toView (c.x, c.y0, 6.0f);
                const float rr = A.mmToView (c.r) + 4.0f;
                c.box = juce::Rectangle<float> (ctr.x - rr, ctr.y - rr, rr * 2, rr * 2);
                controls.push_back (c);
            }
        if (auto* arr = L.getProperty ("buttons", {}).getArray())
            for (auto& s : *arr)
            {
                Control c { Kind::Button, s["id"].toString() };
                c.x = s["x"]; c.y0 = s["y"]; c.ledX = s["led"][0]; c.ledY = s["led"][1];
                const auto a = A.toView (c.x - 7.0f, c.y0 - 4.0f, 3.0f), b = A.toView (c.x + 7.0f, c.y0 + 4.0f);
                c.box = juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y);
                controls.push_back (c);
            }
        {
            const auto& pw = L.getProperty ("power", {});
            Control c { Kind::Button, "power" };
            c.x = pw["x"]; c.y0 = pw["y"]; c.ledX = pw["led"][0]; c.ledY = pw["led"][1];
            const auto a = A.toView (c.x - 8.5f, c.y0 - 5.0f, 3.0f), b = A.toView (c.x + 8.5f, c.y0 + 5.0f);
            c.box = juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y);
            controls.push_back (c);
        }
    }

    Control* hit (juce::Point<float> p)
    {
        for (auto& c : controls) if (c.box.contains (p)) return &c;
        return nullptr;
    }

    bool inBender (juce::Point<float> p) const
    {
        const auto& bd = A.layout.getProperty ("bender", {});
        const auto a = A.toView ((float) bd["cx"] - (float) bd["w"] * 0.5f, (float) bd["cy"] - 9.0f, 6.0f);
        const auto b = A.toView ((float) bd["cx"] + (float) bd["w"] * 0.5f, (float) bd["cy"] + (float) bd["h"] * 0.5f);
        return juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y).contains (p);
    }

    float knobAngle (const Control& c, float v) const
    {
        if (c.steps > 0) return c.a0 + (c.a1 - c.a0) * juce::jlimit (0.0f, 1.0f, v / (float) (c.steps - 1));
        return c.a0 + (c.a1 - c.a0) * v;
    }

    void setValue (Control& c, float v)
    {
        if (c.param != P::count) proc.setPanelValue (c.param, v);
        repaint();
    }

    void setPosition (Control& c, juce::Point<float> p)
    {
        int best = 0; float bd = 1e9f;
        for (int i = 0; i < (int) c.positions.size(); ++i)
        {
            const auto q = c.kind == Kind::Switch ? A.toView (c.x, c.positions[(size_t) i]) : A.toView (c.positions[(size_t) i], c.y0);
            const float d = c.kind == Kind::Switch ? std::abs (q.y - p.y) : std::abs (q.x - p.x);
            if (d < bd) { bd = d; best = i; }
        }
        setValue (c, (float) best);
    }

    bool buttonLed (const Control& c) const
    {
        if (! proc.powerOn.load()) return false;
        const int arp = (int) std::lround (proc.panelValue (P::arp_mode));
        if (c.id == "power") return true;
        if (c.id == "seq_load") return proc.loadingLed.load();
        if (c.id == "seq_play") return proc.panelValue (P::seq_play) > 0.5f;
        if (c.id == "arp_down") return arp == 1;
        if (c.id == "arp_ud") return arp == 2;
        if (c.id == "arp_up") return arp == 3;
        if (c.id == "hold") return proc.panelValue (P::hold) > 0.5f || proc.holdPedal.load();
        if (c.id == "key_transpose")
            return proc.keyTrArmed.load() ? (juce::Time::getMillisecondCounter() / 180) % 2 == 0 : proc.keyTranspose.load() != 0;
        return false;
    }

    void pressButton (Control& c, const juce::ModifierKeys& mods)
    {
        pressedButton = &c;
        const bool loading = proc.loadingLed.load();
        if (c.id == "power") { proc.powerOn.store (! proc.powerOn.load()); if (! proc.powerOn.load()) proc.sendCommand (AT101VProcessor::CmdPanic); }
        else if (c.id == "seq_load")
        {
            if (proc.panelValue (P::seq_play) > 0.5f) proc.setPanelValue (P::seq_play, 0.0f);
            proc.sendCommand (AT101VProcessor::CmdLoadToggle);
        }
        else if (c.id == "seq_play")
        {
            const bool on = proc.panelValue (P::seq_play) < 0.5f;
            if (on && loading) proc.sendCommand (AT101VProcessor::CmdLoadToggle);
            if (on) proc.setPanelValue (P::arp_mode, 0.0f);
            proc.setPanelValue (P::seq_play, on ? 1.0f : 0.0f);
        }
        else if (c.id.startsWith ("arp_"))
        {
            const int m = c.id == "arp_down" ? 1 : (c.id == "arp_ud" ? 2 : 3);
            const int cur = (int) std::lround (proc.panelValue (P::arp_mode));
            proc.setPanelValue (P::seq_play, 0.0f);
            proc.setPanelValue (P::arp_mode, cur == m ? 0.0f : (float) m);
        }
        else if (c.id == "hold")
        {
            if (loading) proc.sendCommand (AT101VProcessor::CmdTie);          // LEGATO
            else proc.setPanelValue (P::hold, proc.panelValue (P::hold) > 0.5f ? 0.0f : 1.0f);
        }
        else if (c.id == "key_transpose")
        {
            if (loading) proc.sendCommand (AT101VProcessor::CmdRest);         // REST
            else if (mods.isPopupMenu() || mods.isAltDown()) proc.sendCommand (AT101VProcessor::CmdKeyTransposeReset);
            else proc.sendCommand (AT101VProcessor::CmdKeyTranspose);
        }
        repaint();
    }

    void showContextMenu (Control& c)
    {
        const juce::String id = panelParam ((int) c.param).id;
        juce::PopupMenu m;
        m.addSectionHeader (panelName ((int) c.param));
        const int cc = proc.ccForParam (id);
        m.addItem (1, proc.isLearning() ? "Waiting for a MIDI CC..." : "MIDI Learn", ! proc.isLearning());
        m.addItem (2, cc >= 0 ? "Clear MIDI CC " + juce::String (cc) : "No MIDI CC assigned", cc >= 0);
        m.addSeparator();
        m.addItem (3, "Reset to default");
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, id, &c] (int r) {
            if (r == 1) proc.startMidiLearn (id);
            else if (r == 2) proc.clearMidiMapping (id);
            else if (r == 3) setValue (c, panelParam ((int) c.param).def);
        });
    }

    void drawLed (juce::Graphics& g, juce::Point<float> p, float scale = 1.0f) const
    {
        A.draw (g, A.ledOn, p, 0, scale);
        g.setColour (col101::ledRed.withAlpha (0.16f));
        g.fillEllipse (juce::Rectangle<float> (16.0f * scale, 10.0f * scale).withCentre (p));
    }

    // Valor del control que se toca, en un globo como el del AT20V.
    void drawBubble (juce::Graphics& g)
    {
        Control* c = active != nullptr && showBubble ? active : (bubbleTimer > 0 ? hover : nullptr);
        if (c == nullptr || c->kind == Kind::Button || c->param == P::count) return;
        const auto title = panelName ((int) c->param);
        const auto value = formatPanel ((int) c->param, proc.panelValue (c->param));
        const auto f1 = col101::font (11.0f), f2 = col101::font (13.0f);
        const float w = std::max (juce::GlyphArrangement::getStringWidth (f1, title), juce::GlyphArrangement::getStringWidth (f2, value)) + 22.0f;
        auto r = juce::Rectangle<float> (w, 40.0f).withCentre ({ c->box.getCentreX(), c->box.getY() - 26.0f });
        r = r.constrainedWithin (getLocalBounds().toFloat().reduced (4.0f));
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (r.translated (0, 2), 5.0f);
        g.setColour (juce::Colour (0xf0161617)); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colour (0xff3a3a3d)); g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (col101::silkDim); g.setFont (f1); g.drawText (title.toUpperCase(), r.withHeight (20.0f).translated (0, 2), juce::Justification::centred);
        g.setColour (juce::Colours::white); g.setFont (f2); g.drawText (value, r.withTrimmedTop (18.0f), juce::Justification::centred);
    }

    // ───────── teclado ─────────
    int baseNote() const { return keyboardOctave ? 12 * keyboardOctave() : 0; }

    bool isDown (int keyNote) const
    {
        const int n = keyNote + baseNote();
        return n >= 0 && n < 128 && proc.keyboardState.isNoteOnForChannels (0xffff, n);
    }

    void drawKeys (juce::Graphics& g, const juce::Image& bg)
    {
        if (! A.keysDown.isValid()) return;
        const auto crop = A.keysCrop.toFloat() * 0.5f;
        auto drawKey = [&] (const Assets101::KeyShape& k, bool pressed) {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (k.hull);
            g.setOpacity (1.0f);
            if (pressed) g.drawImage (A.keysDown, crop);
            else g.drawImage (bg, getLocalBounds().toFloat());
            if (pressed)
            {
                const auto b = k.hull.getBounds();
                juce::ColourGradient sh (juce::Colours::black.withAlpha (k.black ? 0.10f : 0.12f), b.getX(), b.getY(),
                                         juce::Colours::black.withAlpha (k.black ? 0.40f : 0.34f), b.getX(), b.getBottom(), false);
                g.setGradientFill (sh); g.fillRect (b);
            }
        };
        bool any = false;
        for (const auto& k : A.keys) if (! k.black && isDown (k.note)) { drawKey (k, true); any = true; }
        if (! any) { for (const auto& k : A.keys) if (k.black && isDown (k.note)) drawKey (k, true); return; }
        for (const auto& k : A.keys) if (k.black) drawKey (k, isDown (k.note));
    }

    int keyAt (juce::Point<float> p, float& vel) const
    {
        for (int pass = 0; pass < 2; ++pass)
            for (const auto& k : A.keys)
                if (k.black == (pass == 0) && k.hull.contains (p))
                {
                    const auto b = k.hull.getBounds();
                    vel = juce::jlimit (0.25f, 1.0f, 0.3f + 0.8f * (p.y - b.getY()) / b.getHeight());
                    return k.note;
                }
        return -1;
    }

    void playAt (juce::Point<float> p)
    {
        float vel = 0.8f;
        const int k = keyAt (p, vel);
        const int n = k >= 0 ? juce::jlimit (0, 127, k + baseNote()) : -1;
        if (n == mouseNote) return;
        releaseMouseNote();
        if (n < 0) return;
        mouseNote = n;
        proc.keyboardState.noteOn (1, n, vel);
        repaint();
    }

    void releaseMouseNote()
    {
        if (mouseNote >= 0) proc.keyboardState.noteOff (1, mouseNote, 0.0f);
        mouseNote = -1;
    }

    void timerCallback() override
    {
        if (bubbleTimer > 0 && --bubbleTimer == 0) repaint();
        // Redibuja sólo si cambió algo visible que no movió el ratón (MIDI, automatización, LEDs).
        uint64_t h = 1469598103934665603ull;
        auto mix = [&] (uint64_t v) { h = (h ^ v) * 1099511628211ull; };
        for (int i = 0; i < kNumP; ++i) mix ((uint64_t) (proc.panelValue ((P) i) * 1000.0f));
        for (const auto& k : A.keys) mix (isDown (k.note) ? (uint64_t) k.note : 0);
        for (auto& c : controls) if (c.kind == Kind::Button) mix (buttonLed (c) ? 7 : 3);
        mix (proc.rateLed.load() ? 11 : 13);
        mix ((uint64_t) (proc.bender.load() * 100.0f + 200.0f)); mix ((uint64_t) (proc.lfoPush.load() * 100.0f));
        mix ((uint64_t) bodyColour()); mix (proc.powerOn.load() ? 1 : 2);
        if (h != shownState) { shownState = h; repaint(); }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelView101)
};

} // namespace dh
