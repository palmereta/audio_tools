#pragma once
// Lienzo del AT700: los tres gabinetes renderizados y, encima, todo lo que se mueve o se ilumina:
// perillas (la de PITCH es concéntrica: anillo de semitonos y centro FINE), faders verticales y
// horizontales, llaves deslizantes, selectores de onda, palancas, botones, LEDs, VU, las 61 teclas
// y la patchera con cables de 1/4" (fichas apilables en las salidas y en los múltiples).
//
// El lienzo mide 900 × 1090 mm; el editor lo muestra por una ventana (PanelPort700) que se mueve
// entre el secuenciador, la consola y el teclado o lo muestra entero.

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Assets700.h"
#include "Rope.h"

namespace dh {

class PanelView700 : public juce::Component, private juce::Timer
{
public:
    std::function<void (float)> onScrollRequest;        // la rueda sobre una zona vacía mueve la vista

    explicit PanelView700 (AT700Processor& p) : proc (p)
    {
        setSize ((int) A.viewWidth(), (int) A.viewHeight());
        build();
        rebuildCables();
        lastTime = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (30);
    }

    void rebuildCables()
    {
        std::vector<CableView> fresh;
        for (auto& c : proc.getCables())
        {
            CableView cv { c.src, c.dst, c.color, {} };
            bool kept = false;
            for (auto& old : cables) if (old.src == c.src && old.dst == c.dst) { cv.rope = old.rope; kept = true; break; }
            if (! kept) cv.rope.init (jacks[(size_t) c.src].plugTop, jacks[(size_t) c.dst].plugTop, 1.16f, 46.0f);
            fresh.push_back (cv);
        }
        cables = std::move (fresh);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        if (! A.background.isValid()) { g.fillAll (col700::editorBg); return; }
        // Sólo la parte visible del fondo (el lienzo es grande).
        const auto clip = g.getClipBounds();
        const auto src = juce::Rectangle<int> (clip.getX() * 2, clip.getY() * 2, clip.getWidth() * 2, clip.getHeight() * 2).getIntersection (A.background.getBounds());
        g.drawImage (A.background, clip.getX(), clip.getY(), src.getWidth() / 2, src.getHeight() / 2, src.getX(), src.getY(), src.getWidth(), src.getHeight());

        // Sólo lo que toca la zona a repintar: el lienzo tiene cientos de controles.
        const auto vis = clip.toFloat().expanded (A.mm (10.0f), A.mm (14.0f));
        for (auto& l : leds) if (vis.contains (l.c)) drawLed (g, l);
        drawMeters (g);
        drawKeys (g);
        for (auto& c : ctls)
        {
            if (! vis.intersects (c.box)) continue;
            switch (c.kind)
            {
                case Kind::Knob: drawKnob (g, c); break;
                case Kind::Fader: drawFader (g, c); break;
                case Kind::Switch: drawSwitch (g, c); break;
                case Kind::Lever: drawLever (g, c); break;
                case Kind::Toggle: drawToggle (g, c); break;
                case Kind::Button: drawButton (g, c); break;
            }
        }
        // Cables: sombras, fichas, cables, y el que está en la mano.
        for (const auto& c : cables) drawCable (g, c.rope.path(), c.color, true);
        if (drag.active) drawCable (g, drag.rope.path(), drag.color, true);
        for (const auto& c : cables)
        {
            if (vis.contains (jacks[(size_t) c.src].base)) drawPlug (g, c.src, c.color);
            if (vis.contains (jacks[(size_t) c.dst].base)) drawPlug (g, c.dst, c.color);
        }
        if (drag.active) drawPlug (g, drag.fixedJack, drag.color);
        for (const auto& c : cables) drawCable (g, c.rope.path(), c.color, false);
        if (drag.active)
        {
            drawCable (g, drag.rope.path(), drag.color, false);
            A.draw (g, A.plugs[drag.color], drag.free + juce::Point<float> (0.0f, A.mm (A.plugTopZ) * A.sinT), 0, 1.05f);
        }
        if (hoverJack >= 0)
        {
            g.setColour (juce::Colour (0xffffc040).withAlpha (0.85f));
            g.drawEllipse (juce::Rectangle<float> (A.mm (12.0f), A.mm (12.0f)).withCentre (jacks[(size_t) hoverJack].base), 2.0f);
        }
        drawBubble (g);
    }

    // ───────── ratón ─────────
    void mouseDown (const juce::MouseEvent& e) override
    {
        lastMouse = e.position;
        const int j = jackAt (e.position);
        if (j >= 0) { jackDown (j, e); return; }
        if (auto* c = ctlAt (e.position))
        {
            if (e.mods.isPopupMenu() && c->param >= 0) { learnMenu (c->paramId()); return; }
            active = c;
            switch (c->kind)
            {
                case Kind::Knob: case Kind::Fader:
                    if (auto* prm = c->prm()) { prm->beginChangeGesture(); gestureOpen = true; }
                    stepAccum = 0.0f;
                    if (c->kind == Kind::Fader && ! capBox (*c).expanded (A.mm (1.5f)).contains (e.position)) setParam (*c, faderValueAt (*c, e.position));
                    break;
                case Kind::Switch: case Kind::Lever: setParam (*c, (float) nearestPosition (*c, e.position)); break;
                case Kind::Toggle: flipToggle (*c, e.position); break;
                case Kind::Button: pressButton (*c, true); break;
            }
            repaint();
            return;
        }
        if (const int k = keyAt (e.position); k >= 0) { mouseKey = k; proc.sendCommand (AT700Processor::CmdNoteOn, 36 + k); return; }
        draggingCanvas = true; lastScreen = e.getScreenPosition().toFloat();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const auto d = e.position - lastMouse;
        lastMouse = e.position;
        if (drag.active)
        {
            drag.free = e.position;
            const int j = jackAt (e.position);
            hoverJack = canConnect (drag.fixedJack, j) ? j : -1;
            return;
        }
        if (mouseKey >= 0)
        {
            const int k = keyAt (e.position);
            if (k != mouseKey) { proc.sendCommand (AT700Processor::CmdNoteOff, 36 + mouseKey); mouseKey = k; if (k >= 0) proc.sendCommand (AT700Processor::CmdNoteOn, 36 + k); }
            return;
        }
        // Arrastrar el panel lo mueve con la mano. La cuenta va en coordenadas de pantalla: las locales cambian
        // justamente porque el panel se mueve, y la diferencia se agrandaba en cada paso.
        if (draggingCanvas)
        {
            const auto sp = e.getScreenPosition().toFloat();
            if (onScrollRequest) onScrollRequest (sp.y - lastScreen.y);
            lastScreen = sp;
            return;
        }
        if (active == nullptr) return;
        auto& c = *active;
        const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
        if (c.kind == Kind::Knob)
        {
            if (c.steps > 0)
            {
                stepAccum += -d.y;
                if (std::abs (stepAccum) > 14.0f)
                {
                    setParam (c, (float) juce::jlimit (0, c.steps - 1, (int) std::lround (c.value()) + (stepAccum > 0 ? 1 : -1)));
                    stepAccum = 0.0f;
                }
            }
            else setParam (c, juce::jlimit (0.0f, 1.0f, c.value() - d.y / 220.0f * fine));
            showBubble = true;
        }
        else if (c.kind == Kind::Fader)
        {
            const float travel = c.horiz ? A.mm (c.x1 - c.x0) : A.mm (c.y1 - c.y0) * A.cosT;
            const float dv = (c.horiz ? d.x : -d.y) / std::max (1.0f, travel) * fine;
            setParam (c, juce::jlimit (0.0f, 1.0f, c.value() + dv));
            showBubble = true;
        }
        else if (c.kind == Kind::Lever || c.kind == Kind::Switch) setParam (c, (float) nearestPosition (c, e.position));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        if (drag.active) { finishCable (lastMouse); return; }
        if (active != nullptr)
        {
            if (active->id == "kb_bender") setParam (*active, 0.5f);          // la palanca vuelve al centro, dentro del gesto
            if (active->kind == Kind::Knob || active->kind == Kind::Fader) if (auto* prm = active->prm()) prm->endChangeGesture();
            gestureOpen = false;
            if (active->kind == Kind::Button) pressButton (*active, false);
        }
        if (mouseKey >= 0) { proc.sendCommand (AT700Processor::CmdNoteOff, 36 + mouseKey); mouseKey = -1; }
        active = nullptr; showBubble = false; draggingCanvas = false;
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto* c = ctlAt (e.position);
        if (c != hover) { hover = c; repaint(); }
        const int j = jackAt (e.position);
        setMouseCursor (c != nullptr || j >= 0 || keyAt (e.position) >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override { hover = nullptr; repaint(); }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (auto* c = ctlAt (e.position); c != nullptr && (c->kind == Kind::Knob || c->kind == Kind::Fader) && c->param >= 0)
            setParam (*c, panelParam700 (c->param).def);
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto* c = ctlAt (e.position);
        const float d = w.isReversed ? -w.deltaY : w.deltaY;
        if (c == nullptr || ! (c->kind == Kind::Knob || c->kind == Kind::Fader))
        {
            if (onScrollRequest) onScrollRequest (-d * 160.0f);
            return;
        }
        if (c->steps > 0)
        {
            // Las perillas con posiciones avanzan un paso por muesca: el trackpad manda muchos deltas chicos.
            wheelAcc += d;
            if (std::abs (wheelAcc) >= 0.1f)
            {
                setParam (*c, (float) juce::jlimit (0, c->steps - 1, (int) std::lround (c->value()) + (wheelAcc > 0 ? 1 : -1)));
                wheelAcc = 0.0f;
            }
        }
        else setParam (*c, juce::jlimit (0.0f, 1.0f, c->value() + d * 0.05f));
        hover = c; bubbleTimer = 25;
        repaint();
    }

    // Prueba sin pantalla (AT700_SELFTEST=1): arrastra un cable, gira una perilla, mueve un fader,
    // cambia una llave y toca una tecla con eventos de ratón sintéticos, como lo haría la mano.
    void selfTest()
    {
        auto msrc = juce::Desktop::getInstance().getMainMouseSource();
        auto ev = [&] (juce::Point<float> p, bool dragged) {
            return juce::MouseEvent (msrc, p, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, this, this, juce::Time::getCurrentTime(),
                                     p, juce::Time::getCurrentTime(), 1, dragged);
        };
        auto gesture = [&] (juce::Point<float> a, juce::Point<float> b) {
            mouseDown (ev (a, false));
            for (int i = 1; i <= 10; ++i) mouseDrag (ev (a + (b - a) * ((float) i / 10.0f), true));
            mouseUp (ev (b, true));
        };
        auto jackPos = [&] (int j) { return jacks[(size_t) j].base; };
        gesture (jackPos (jk700::vco1_saw), jackPos (jk700::vco2_sync));                     // cable de sync
        gesture (jackPos (jk700::lfo2_out), jackPos (jk700::mult2_1));                       // a un múltiple
        gesture (jackPos (jk700::mult2_3), jackPos (jk700::vco1_pwm));                       // y del múltiple al PWM
        for (auto& c : ctls)
        {
            if (c.id == "vcf1_cutoff") gesture (capBox (c).getCentre(), capBox (c).getCentre() + juce::Point<float> (0.0f, -40.0f));
            if (c.id == "vco1_pw") gesture (c.box.getCentre(), c.box.getCentre() + juce::Point<float> (0.0f, -60.0f));
            if (c.id == "vco1_wave") { mouseDown (ev (switchPos (c, 0), false)); mouseUp (ev (switchPos (c, 0), false)); }
            if (c.id == "vcf2_mode") { mouseDown (ev (switchPos (c, 1), false)); mouseUp (ev (switchPos (c, 1), false)); }
        }
        for (auto& k : keys) if (k.i == 24) { mouseDown (ev (k.box.getCentre(), false)); break; }
    }
    void releaseKeys() { mouseUp (juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), {}, juce::ModifierKeys(), 1.0f, 0, 0, 0, 0, this, this, juce::Time::getCurrentTime(), {}, juce::Time::getCurrentTime(), 1, false)); }

private:
    enum class Kind { Knob, Fader, Switch, Lever, Toggle, Button };
    struct Ctl
    {
        Kind kind = Kind::Knob; juce::String id; int param = -1;
        float x = 0, y = 0, d = 8, a0 = -150, a1 = 150;
        float x0 = 0, x1 = 0, y0 = 0, y1 = 0, length = 10;     // faders, llaves, palancas
        bool horiz = false, vertical = true, sideways = false;
        int steps = 0, positions = 2;
        juce::String sprite, shape, color;
        juce::Rectangle<float> box;
        AT700Processor* proc = nullptr;
        juce::RangedAudioParameter* prm() const { return param >= 0 ? proc->apvts.getParameter (panelParam700 (param).id) : nullptr; }
        juce::String paramId() const { return param >= 0 ? juce::String (panelParam700 (param).id) : juce::String(); }
        float value() const { return param >= 0 ? proc->value (param) : 0.0f; }
    };
    struct LedV { juce::String id; juce::Point<float> c; juce::Colour col; };
    struct MeterV { juce::String id; float x, y, w, h; };
    struct KeyV { int i; bool black; juce::Path shape; juce::Rectangle<float> box; };
    struct JackV { int index; juce::Point<float> base, plugTop; };
    struct CableView { int src, dst; juce::String color; Rope rope; };
    struct DragCable { bool active = false; int fixedJack = -1; juce::Point<float> free; juce::String color; Rope rope; };

    AT700Processor& proc;
    Assets700& A = Assets700::get();
    std::vector<Ctl> ctls;
    std::vector<LedV> leds;
    std::vector<MeterV> meters;
    std::vector<KeyV> keys;
    std::vector<JackV> jacks;
    std::vector<CableView> cables;
    DragCable drag;
    Ctl* active = nullptr; Ctl* hover = nullptr;
    juce::Point<float> lastMouse, lastScreen;
    float stepAccum = 0.0f;
    bool showBubble = false, draggingCanvas = false;
    int bubbleTimer = 0, mouseKey = -1, hoverJack = -1, colorCycle = 0;
    float wheelAcc = 0.0f;
    bool gestureOpen = false;
    double lastTime = 0.0;

    static int pidx (const juce::String& id) { for (int i = 0; i < kNumP700; ++i) if (id == panelParam700 (i).id) return i; return -1; }
    static Ctl makeCtl (Kind k, const juce::String& id, int param) { Ctl c; c.kind = k; c.id = id; c.param = param; return c; }

    void build()
    {
        const auto& L = A.layout;
        if (auto* arr = L.getProperty ("knobs", {}).getArray())
            for (auto& k : *arr)
            {
                Ctl c = makeCtl (Kind::Knob, k["id"].toString(), pidx (k["id"].toString()));
                c.x = k["x"]; c.y = k["y"]; c.d = k["d"]; c.steps = k["steps"]; c.a0 = k["a0"]; c.a1 = k["a1"];
                c.sprite = k["size"].toString();
                const float r = A.mm (c.d) * 0.5f + (c.sprite == "KF" ? 1.0f : 3.0f);
                c.box = juce::Rectangle<float> (r * 2, r * 2).withCentre (A.toView (c.x, c.y, 6.0f));
                c.proc = &proc; ctls.push_back (c);
            }
        // El centro FINE va antes que el anillo de PITCH para el clic.
        std::stable_sort (ctls.begin(), ctls.end(), [] (const Ctl& a, const Ctl& b) { return (a.sprite == "KF") > (b.sprite == "KF"); });
        if (auto* arr = L.getProperty ("sliders", {}).getArray())
            for (auto& s : *arr)
            {
                Ctl c = makeCtl (Kind::Fader, s["id"].toString(), pidx (s["id"].toString()));
                c.horiz = (bool) s["horiz"];
                if (c.horiz) { c.x0 = s["x0"]; c.x1 = s["x1"]; c.y = s["y"]; const auto a = A.toView (c.x0 - 5.0f, c.y - 4.0f, 6.0f), b = A.toView (c.x1 + 5.0f, c.y + 4.0f); c.box = juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y); }
                else { c.x = s["x"]; c.y0 = s["y0"]; c.y1 = s["y1"]; const auto a = A.toView (c.x - 4.6f, c.y0 - 4.0f, 6.0f), b = A.toView (c.x + 4.6f, c.y1 + 4.0f); c.box = juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y); }
                c.proc = &proc; ctls.push_back (c);
            }
        if (auto* arr = L.getProperty ("switches", {}).getArray())
            for (auto& s : *arr)
            {
                Ctl c = makeCtl (Kind::Switch, s["id"].toString(), pidx (s["id"].toString()));
                c.x = s["x"]; c.y = s["y"]; c.positions = s["positions"]; c.steps = c.positions; c.length = s["length"];
                c.vertical = s["orient"].toString() == "v";
                const float hw = c.vertical ? 3.4f : c.length / 2 + 1.5f, hh = c.vertical ? c.length / 2 + 1.5f : 3.4f;
                const auto a = A.toView (c.x - hw, c.y - hh, 3.0f), b = A.toView (c.x + hw, c.y + hh);
                c.box = juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y);
                c.proc = &proc; ctls.push_back (c);
            }
        if (auto* arr = L.getProperty ("levers", {}).getArray())
            for (auto& s : *arr)
            {
                Ctl c = makeCtl (Kind::Lever, s["id"].toString(), pidx (s["id"].toString()));
                c.x = s["x"]; c.y0 = s["y0"]; c.y1 = s["y1"]; c.positions = s["positions"]; c.steps = c.positions;
                const auto a = A.toView (c.x - 3.6f, c.y0 - 4.0f, 6.0f), b = A.toView (c.x + 3.6f, c.y1 + 4.0f);
                c.box = juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y);
                c.proc = &proc; ctls.push_back (c);
            }
        if (auto* arr = L.getProperty ("toggles", {}).getArray())
            for (auto& t : *arr)
            {
                Ctl c = makeCtl (Kind::Toggle, t["id"].toString(), pidx (t["id"].toString()));
                c.x = t["x"]; c.y = t["y"]; c.positions = t["positions"]; c.steps = c.positions;
                c.sideways = t["lpos"].toString() == "horizontal";
                c.box = juce::Rectangle<float> (A.mm (8.0f), A.mm (9.0f)).withCentre (A.toView (c.x, c.y, 4.0f));
                c.proc = &proc; ctls.push_back (c);
            }
        if (auto* arr = L.getProperty ("buttons", {}).getArray())
            for (auto& b : *arr)
            {
                Ctl c = makeCtl (Kind::Button, b["id"].toString(), -1);
                c.x = b["x"]; c.y = b["y"]; c.shape = b["shape"].toString(); c.color = b["color"].toString();
                const float w = b["w"], h = b["h"];
                c.sprite = c.shape + "_" + c.color + "_" + juce::String ((int) std::lround (w * 10.0f));
                c.box = juce::Rectangle<float> (A.mm (w + 2.0f), A.mm (h + 2.0f)).withCentre (A.toView (c.x, c.y, 3.0f));
                c.proc = &proc; ctls.push_back (c);
            }
        if (auto* arr = L.getProperty ("leds", {}).getArray())
            for (auto& l : *arr) leds.push_back ({ l["id"].toString(), A.toView (l["x"], l["y"], 1.4f), col700::led (l["color"].toString()) });
        if (auto* arr = L.getProperty ("meters", {}).getArray())
            for (auto& m : *arr) meters.push_back ({ m["id"].toString(), m["x"], m["y"], m["w"], m["h"] });
        // Teclas: blancas primero, negras encima (se buscan al revés para el clic).
        const auto kr = L.getProperty ("cabinets", {}).getProperty ("keys", {});
        const float ky0 = kr["y0"], ky1 = kr["y1"];
        if (auto* arr = L.getProperty ("keylist", {}).getArray())
        {
            for (int pass = 0; pass < 2; ++pass)
                for (auto& k : *arr)
                {
                    const bool black = (bool) k["black"];
                    if (black != (pass == 1)) continue;
                    const float x0 = k["x0"], x1 = k["x1"];
                    const float yb = black ? ky0 + (ky1 - ky0) * 0.62f : ky1, z = black ? 21.0f : 11.0f;
                    juce::Path p;
                    const auto a = A.toView (x0 + 0.4f, ky0, z), b = A.toView (x1 - 0.4f, ky0, z), c = A.toView (x1 - 0.4f, yb, z), d = A.toView (x0 + 0.4f, yb, z);
                    p.startNewSubPath (a); p.lineTo (b); p.lineTo (c); p.lineTo (d); p.closeSubPath();
                    keys.push_back ({ (int) k["i"], black, p, p.getBounds() });
                }
        }
        for (int j = 0; j < jk700::kNumJacks; ++j)
        {
            const auto& d = jk700::jacks[j];
            jacks.push_back ({ j, A.toView (d.x, d.y, 3.0f), A.toView (d.x, d.y, A.plugTopZ) });
        }
    }

    Ctl* ctlAt (juce::Point<float> p)
    {
        for (auto& c : ctls)
        {
            if (! c.box.contains (p)) continue;
            if (c.kind == Kind::Knob && p.getDistanceFrom (c.box.getCentre()) > c.box.getWidth() * 0.5f) continue;
            return &c;
        }
        return nullptr;
    }

    int jackAt (juce::Point<float> p) const
    {
        for (const auto& j : jacks)
            if (p.getDistanceFrom (j.base) < A.mm (5.4f) || p.getDistanceFrom (j.plugTop) < A.mm (3.0f)) return j.index;
        return -1;
    }

    int keyAt (juce::Point<float> p) const
    {
        for (int i = (int) keys.size() - 1; i >= 0; --i) if (keys[(size_t) i].shape.contains (p)) return keys[(size_t) i].i;
        return -1;
    }

    // Un cambio suelto (rueda, doble clic, llaves, la palanca al volver) va envuelto en su propio gesto para que el
    // host lo grabe en automatización Touch/Latch; durante un arrastre el gesto ya está abierto desde mouseDown.
    void setParam (Ctl& c, float v)
    {
        auto* prm = c.prm();
        if (prm == nullptr) return;
        const bool inDrag = gestureOpen && active == &c;
        if (! inDrag) prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        if (! inDrag) prm->endChangeGesture();
    }

    // Posición del fader (mm) para un valor, y su recuadro en pantalla.
    juce::Point<float> faderPos (const Ctl& c, float v) const
    {
        return c.horiz ? A.toView (c.x0 + (c.x1 - c.x0) * v, c.y) : A.toView (c.x, c.y1 - (c.y1 - c.y0) * v);
    }
    juce::Rectangle<float> capBox (const Ctl& c) const
    {
        const auto p = faderPos (c, c.value());
        return juce::Rectangle<float> (A.mm (c.horiz ? 5.0f : 9.0f), A.mm (c.horiz ? 9.0f : 5.0f)).withCentre (p + juce::Point<float> (0.0f, -A.mm (8.0f) * A.sinT));
    }
    float faderValueAt (const Ctl& c, juce::Point<float> p) const
    {
        if (c.horiz) { const auto a = A.toView (c.x0, c.y), b = A.toView (c.x1, c.y); return juce::jlimit (0.0f, 1.0f, (p.x - a.x) / (b.x - a.x)); }
        const auto a = A.toView (c.x, c.y1), b = A.toView (c.x, c.y0);
        return juce::jlimit (0.0f, 1.0f, (p.y - a.y) / (b.y - a.y));
    }

    juce::Point<float> switchPos (const Ctl& c, int i) const
    {
        if (c.kind == Kind::Lever) return A.toView (c.x, c.y0 + (c.y1 - c.y0) * (float) i / (float) (c.positions - 1));
        const float l = c.length - 4.0f, f = c.positions > 1 ? (float) i / (float) (c.positions - 1) : 0.0f;
        return c.vertical ? A.toView (c.x, c.y - l / 2 + l * f) : A.toView (c.x - l / 2 + l * f, c.y);
    }
    int nearestPosition (const Ctl& c, juce::Point<float> p) const
    {
        int best = 0; float bd = 1.0e9f;
        for (int i = 0; i < c.positions; ++i) { const float d = p.getDistanceFrom (switchPos (c, i)); if (d < bd) { bd = d; best = i; } }
        return best;
    }

    void flipToggle (Ctl& c, juce::Point<float> p)
    {
        int v = (int) std::lround (c.value());
        const auto ctr = A.toView (c.x, c.y, 4.0f);
        if (c.positions == 3) v = p.y < ctr.y - 3.0f ? 0 : (p.y > ctr.y + 3.0f ? 2 : 1);
        else v = 1 - v;
        setParam (c, (float) v);
    }

    void pressButton (Ctl& c, bool down)
    {
        pressed = down ? &c : nullptr;
        if (c.id == "adsr_manual") { proc.manualGate.store (down); return; }
        if (! down) return;
        if (c.id == "seq_start") proc.sendCommand (AT700Processor::CmdSeqStart);
        else if (c.id == "seq_continue") proc.sendCommand (AT700Processor::CmdSeqContinue);
        else if (c.id == "seq_stop") proc.sendCommand (AT700Processor::CmdSeqStop);
        else if (c.id == "seq_end") proc.sendCommand (AT700Processor::CmdSeqEnd);
        else if (c.id.startsWith ("step") && c.id.endsWith ("_manual")) proc.sendCommand (AT700Processor::CmdSeqStep, c.id.fromFirstOccurrenceOf ("step", false, false).getIntValue() - 1);
        else if (c.id.startsWith ("ser_manual_")) proc.sendCommand (AT700Processor::CmdSeqChan, (int) (c.id.getLastCharacter() - 'a'));
    }
    Ctl* pressed = nullptr;

    void learnMenu (const juce::String& id)
    {
        juce::PopupMenu m;
        const int cc = proc.ccForParam (id);
        m.addItem (1, proc.isLearning() ? "Waiting for a MIDI CC..." : "MIDI Learn", ! proc.isLearning());
        m.addItem (2, cc >= 0 ? "Clear MIDI CC " + juce::String (cc) : "No MIDI CC assigned", cc >= 0);
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, id] (int r) {
            if (r == 1) proc.startMidiLearn (id); else if (r == 2) proc.clearMidiMapping (id);
        });
    }

    // ── cables ──
    juce::String nextColor()
    {
        static const char* c[] = { "red", "yellow", "blue", "green", "white", "black" };
        return c[(colorCycle++) % 6];
    }

    static int multColumn (int j) { const int m = (int) jk700::mult1_1; return (j >= m && j < m + 10) ? (j - m) / 5 : -1; }

    // Dirección del cable si se puede conectar (src → dst); false si no.
    bool direction (int a, int b, int& src, int& dst) const
    {
        if (a < 0 || b < 0 || a == b) return false;
        const auto ia = jk700::jacks[a].io, ib = jk700::jacks[b].io;
        if ((ia == jk700::Out && ib == jk700::Out) || (ia == jk700::In && ib == jk700::In)) return false;
        // Entre dos múltiples manda el arrastre: el jack de donde sale el cable es la fuente.
        if (ia == jk700::Out || (ia == jk700::Mult && (ib == jk700::In || ib == jk700::Mult))) { src = a; dst = b; }
        else { src = b; dst = a; }
        if (multColumn (src) >= 0 && multColumn (src) == multColumn (dst)) return false;   // una columna no se alimenta a sí misma
        if (multColumn (src) >= 0 && multColumn (dst) >= 0)
            for (auto& c : proc.getCables())                                                 // ni en lazo con la otra
                if (multColumn (c.src) == multColumn (dst) && multColumn (c.dst) == multColumn (src)) return false;
        return true;
    }
    bool canConnect (int a, int b) const { int s, d; return direction (a, b, s, d); }

    void jackDown (int j, const juce::MouseEvent& e)
    {
        auto cs = proc.getCables();
        if (e.mods.isAltDown() || e.mods.isPopupMenu())
        {
            cs.erase (std::remove_if (cs.begin(), cs.end(), [j] (auto& c) { return c.src == j || c.dst == j; }), cs.end());
            proc.setCables (cs);
            return;
        }
        for (int i = (int) cs.size() - 1; i >= 0; --i)
        {
            auto c = cs[(size_t) i];
            if (c.src != j && c.dst != j) continue;
            if (e.mods.isShiftDown() && jk700::jacks[j].io != jk700::In) break;     // ficha apilada desde una salida
            drag.active = true; drag.fixedJack = c.src == j ? c.dst : c.src; drag.color = c.color; drag.free = e.position;
            for (auto& cv : cables)
                if (cv.src == c.src && cv.dst == c.dst)
                {
                    drag.rope = cv.rope;
                    if (c.src == j) { std::reverse (drag.rope.p.begin(), drag.rope.p.end()); std::reverse (drag.rope.prev.begin(), drag.rope.prev.end()); }
                }
            cs.erase (cs.begin() + i);
            proc.setCables (cs);
            return;
        }
        drag.active = true; drag.fixedJack = j; drag.color = nextColor(); drag.free = e.position;
        drag.rope.init (jacks[(size_t) j].plugTop, e.position, 1.0f, 40.0f);
    }

    void finishCable (juce::Point<float> p)
    {
        const int target = jackAt (p);
        auto cs = proc.getCables();
        int src = -1, dst = -1;
        if (direction (drag.fixedJack, target, src, dst))
        {
            Cable700 c; c.src = src; c.dst = dst; c.color = drag.color;
            cs.erase (std::remove_if (cs.begin(), cs.end(), [&] (auto& x) { return x.dst == c.dst; }), cs.end());
            cs.push_back (c);
            Rope r = drag.rope;
            r.length = jacks[(size_t) c.src].plugTop.getDistanceFrom (jacks[(size_t) c.dst].plugTop) * 1.16f + 46.0f;
            if (drag.fixedJack != src) { std::reverse (r.p.begin(), r.p.end()); std::reverse (r.prev.begin(), r.prev.end()); }
            drag.active = false; hoverJack = -1;
            proc.setCables (cs);
            for (auto& cv : cables) if (cv.src == c.src && cv.dst == c.dst) { cv.rope = r; cv.rope.energy = 1.0f; }
        }
        else { drag.active = false; hoverJack = -1; }
        repaint();
    }

    void drawPlug (juce::Graphics& g, int jack, const juce::String& color) const
    {
        auto it = A.plugs.find (color);
        if (it == A.plugs.end()) return;
        const auto& d = jk700::jacks[jack];
        A.draw (g, it->second, A.toView (d.x, d.y));
    }

    void drawCable (juce::Graphics& g, const juce::Path& path, const juce::String& color, bool shadow) const
    {
        const float w = A.mm (2.6f);
        if (shadow)
        {
            const auto t = juce::AffineTransform::translation (6.0f, 11.0f);
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.strokePath (path, juce::PathStrokeType (w * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
            g.setColour (juce::Colours::black.withAlpha (0.22f));
            g.strokePath (path, juce::PathStrokeType (w * 1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
            return;
        }
        const auto base = col700::cable (color);
        g.setColour (base.darker (0.8f));
        g.strokePath (path, juce::PathStrokeType (w * 1.25f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (base);
        g.strokePath (path, juce::PathStrokeType (w, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (color == "white" ? 0.5f : 0.28f));
        g.strokePath (path, juce::PathStrokeType (w * 0.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (-1.0f, -1.4f));
    }

    // ── dibujo de controles ──
    void drawKnob (juce::Graphics& g, const Ctl& c) const
    {
        auto it = A.knobs.find (c.sprite);
        if (it == A.knobs.end()) return;
        float ang;
        if (c.steps > 0) ang = c.a0 + (c.a1 - c.a0) * juce::jlimit (0.0f, 1.0f, c.value() / (float) (c.steps - 1));
        else ang = c.a0 + (c.a1 - c.a0) * c.value();
        if (c.sprite == "KF") ang = -150.0f + 300.0f * c.value();
        const int frames = it->second.frames;
        const int f = ((int) std::lround (ang / 360.0f * (float) frames) % frames + frames) % frames;
        A.draw (g, it->second, A.toView (c.x, c.y, c.sprite == "KF" ? 7.2f : 0.0f), f);
    }

    void drawFader (juce::Graphics& g, const Ctl& c) const
    {
        float v = c.value();
        if (c.id == "kb_bender") v = juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * proc.live.bender.load());
        A.draw (g, c.horiz ? A.faderH : A.faderV, faderPos (c, v));
    }

    void drawSwitch (juce::Graphics& g, const Ctl& c) const
    {
        const int v = juce::jlimit (0, c.positions - 1, (int) std::lround (c.value()));
        A.draw (g, c.vertical ? A.nubV : A.nubH, switchPos (c, v));
    }

    void drawLever (juce::Graphics& g, const Ctl& c) const
    {
        const int v = juce::jlimit (0, c.positions - 1, (int) std::lround (c.value()));
        A.draw (g, A.lever, switchPos (c, v));
    }

    void drawToggle (juce::Graphics& g, const Ctl& c) const
    {
        const int v = (int) std::lround (c.value());
        const Sprite700* s;
        if (c.positions == 3) s = v == 0 ? &A.batUp : (v == 2 ? &A.batDown : &A.batMid);
        else if (c.sideways) s = v == 0 ? &A.batLeft : &A.batRight;
        else s = v == 0 ? &A.batUp : &A.batDown;
        A.draw (g, *s, A.toView (c.x, c.y));
    }

    void drawButton (juce::Graphics& g, const Ctl& c) const
    {
        if (pressed != &c) return;
        auto it = A.buttons.find (c.sprite);
        if (it != A.buttons.end()) A.draw (g, it->second, A.toView (c.x, c.y));
    }

    static void glow (juce::Graphics& g, juce::Point<float> c, juce::Colour col, float amount, float radius)
    {
        if (amount <= 0.01f) return;
        juce::ColourGradient gr (col.withAlpha (0.55f * amount), c.x, c.y, col.withAlpha (0.0f), c.x + radius, c.y, true);
        g.setGradientFill (gr);
        g.fillEllipse (juce::Rectangle<float> (radius * 2, radius * 2).withCentre (c));
        g.setColour (col.brighter (0.6f).withAlpha (std::min (1.0f, amount)));
        g.fillEllipse (juce::Rectangle<float> (radius * 0.4f, radius * 0.4f).withCentre (c));
    }

    float ledAmount (const juce::String& id) const
    {
        auto& L = proc.live;
        if (! L.power.load()) return 0.0f;
        if (id == "vcf1_over") return L.vcfOver[0].load();
        if (id == "vcf1_sig") return std::min (1.0f, L.vcfSig[0].load()) * (L.vcfOver[0].load() > 0.5f ? 0.0f : 1.0f);
        if (id == "vcf2_over") return L.vcfOver[1].load();
        if (id == "vcf2_sig") return std::min (1.0f, L.vcfSig[1].load()) * (L.vcfOver[1].load() > 0.5f ? 0.0f : 1.0f);
        if (id == "vca1_over") return L.vcaOver[0].load();
        if (id == "vca1_sig") return std::min (1.0f, L.vcaSig[0].load()) * (L.vcaOver[0].load() > 0.5f ? 0.0f : 1.0f);
        if (id == "vca2_over") return L.vcaOver[1].load();
        if (id == "vca2_sig") return std::min (1.0f, L.vcaSig[1].load()) * (L.vcaOver[1].load() > 0.5f ? 0.0f : 1.0f);
        if (id == "rm_sig") return std::min (1.0f, L.rmSig.load());
        if (id == "ef") return L.ef.load();
        if (id == "lfo1") return L.lfo1.load();
        if (id == "lfo2") return L.lfo2.load();
        if (id == "power") return 1.0f;
        if (id == "pshaper") return L.pshaper.load();
        if (id == "seq_clock") return L.clock.load();
        if (id == "seq_start") return L.running.load() ? 1.0f : 0.0f;
        if (id == "seq_stop") return L.running.load() ? 0.0f : 1.0f;
        if (id.startsWith ("step")) return L.step.load() == id.substring (4).getIntValue() - 1 ? 1.0f : 0.0f;
        if (id.startsWith ("ser_")) return L.chan.load() == (int) (id.getLastCharacter() - 'a') ? 1.0f : 0.0f;
        return 0.0f;
    }

    void drawLed (juce::Graphics& g, const LedV& l) const { glow (g, l.c, l.col, ledAmount (l.id), A.mm (3.6f)); }

    void drawMeters (juce::Graphics& g) const
    {
        for (auto& m : meters)
        {
            // Esfera de 800 × 520 con el pivote en (0.5, 1.18) y radio 0.98·alto (ver panel700.py).
            const float fx0 = m.x - m.w / 2 + 0.5f, fy0 = m.y - m.h / 2 + 0.5f, fw = m.w - 1.0f, fh = m.h - 1.0f;
            const auto pivot = A.toView (fx0 + fw * 0.5f, fy0 + fh * 1.18f, 0.6f);
            const float lvl = m.id == "vu_l" ? proc.live.vuL.load() : proc.live.vuR.load();
            const float db = juce::Decibels::gainToDecibels (std::max (1.0e-4f, lvl), -60.0f);
            const float vu = std::pow (10.0f, db / 20.0f);
            const float ang = juce::degreesToRadians (-48.0f + 96.0f * juce::jlimit (0.0f, 1.0f, vu / 1.4125f));
            const float len = A.mm (fh * 0.98f * 0.86f);
            const auto tip = pivot + juce::Point<float> (std::sin (ang), -std::cos (ang) * A.cosT) * len;
            const auto from = pivot + (tip - pivot) * 0.36f;
            g.saveState();
            const auto a = A.toView (fx0, fy0, 0.6f), b = A.toView (fx0 + fw, fy0 + fh, 0.6f);
            g.reduceClipRegion (juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y).toNearestInt());
            g.setColour (juce::Colours::black.withAlpha (0.25f));
            g.drawLine (juce::Line<float> (from + juce::Point<float> (2.0f, 2.0f), tip + juce::Point<float> (2.0f, 2.0f)), 1.6f);
            g.setColour (juce::Colour (0xff141414));
            g.drawLine (juce::Line<float> (from, tip), 1.3f);
            g.restoreState();
        }
    }

    void drawKeys (juce::Graphics& g) const
    {
        for (auto& k : keys)
        {
            if (! proc.live.keyDown[(size_t) (36 + k.i)].load()) continue;
            g.setColour (k.black ? juce::Colours::white.withAlpha (0.12f) : juce::Colours::black.withAlpha (0.18f));
            g.fillPath (k.shape);
            g.setColour (juce::Colour (0xffff8a3c).withAlpha (0.55f));
            g.strokePath (k.shape, juce::PathStrokeType (1.4f));
        }
    }

    // El valor como suena: el texto del host (formatPanel700) no puede ver los selectores que lo modifican (CLOCK
    // RANGE x10, TIME x1/10 de los ADSR, RANGE LO del VCO), así que la burbuja los tiene en cuenta acá.
    juce::String panelText (int param, float v) const
    {
        const juce::String id (panelParam700 (param).id);
        auto sel = [this] (const char* sid) { auto* r = proc.apvts.getRawParameterValue (sid); return r != nullptr ? (int) std::lround (r->load()) : 0; };
        if (id == "clk_rate")
            return juce::String (2.0f * std::pow (10.0f, v) * (sel ("clk_range") == 0 ? 10.0f : 1.0f), 1) + " steps/min";
        if (id.startsWith ("adsr") && ! id.endsWith ("_s") && ! id.endsWith ("_mode") && ! id.endsWith ("_time"))
        {
            const bool tenth = sel (("adsr" + id.substring (4, 5) + "_time").toRawUTF8()) == 1;
            return secText700 (0.002f * std::pow (5000.0f, v) * (tenth ? 0.1f : 1.0f));
        }
        if (id.startsWith ("vco") && id.endsWith ("_pitch"))
        {
            const float span = sel (("vco" + id.substring (3, 4) + "_range").toRawUTF8()) == 0 ? 24.0f : 12.0f;
            const float st = (v - 0.5f) * 2.0f * span;
            return (st >= 0 ? "+" : "") + juce::String (st, 1) + " st";
        }
        return formatPanel700 (param, v);
    }

    void drawBubble (juce::Graphics& g) const
    {
        const Ctl* c = active != nullptr && showBubble ? active : (bubbleTimer > 0 ? hover : nullptr);
        if (c == nullptr || c->param < 0 || ! (c->kind == Kind::Knob || c->kind == Kind::Fader)) return;
        const auto title = juce::String (panelParam700 (c->param).name);
        const auto value = panelText (c->param, c->value());
        const auto f1 = col700::font (11.0f), f2 = col700::font (13.0f);
        const float w = std::max (juce::GlyphArrangement::getStringWidth (f1, title), juce::GlyphArrangement::getStringWidth (f2, value)) + 22.0f;
        auto r = juce::Rectangle<float> (w, 40.0f).withCentre ({ c->box.getCentreX(), c->box.getY() - 24.0f }).constrainedWithin (getLocalBounds().toFloat().reduced (4.0f));
        g.setColour (juce::Colour (0xf0161617)); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colour (0xff3a3a3d)); g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (juce::Colour (0xffb9b6ad)); g.setFont (f1); g.drawText (title.toUpperCase(), r.withHeight (20.0f).translated (0, 2), juce::Justification::centred);
        g.setColour (juce::Colours::white); g.setFont (f2); g.drawText (value, r.withTrimmedTop (18.0f), juce::Justification::centred);
    }

    // Sólo se redibuja lo que cambió.
    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const float dt = (float) juce::jlimit (0.001, 0.05, (now - lastTime) * 0.001);
        lastTime = now;
        juce::RectangleList<int> dirty;
        auto mark = [&] (std::vector<int>& last, size_t i, int sig, juce::Rectangle<float> r) {
            if (last.size() <= i) last.resize (i + 1, INT_MIN);
            if (last[i] != sig) { last[i] = sig; dirty.add (r.toNearestInt());
                if (&last == &sigCtl) ++dbgCtl; else if (&last == &sigLed) ++dbgLed; else if (&last == &sigMeter) ++dbgMeter; else ++dbgKey; }
        };
        for (size_t i = 0; i < ctls.size(); ++i)
        {
            const auto& c = ctls[i];
            int sig = (int) std::lround (c.value() * 4000.0f);
            if (c.kind == Kind::Button) sig = pressed == &c ? 1 : 0;
            if (c.id == "kb_bender") sig = (int) std::lround (proc.live.bender.load() * 1000.0f);
            auto r = c.box.expanded (A.mm (4.0f));
            if (c.kind == Kind::Knob) r = r.withTop (r.getY() - A.mm (6.0f));
            mark (sigCtl, i, sig, r);
        }
        for (size_t i = 0; i < leds.size(); ++i)
            mark (sigLed, i, (int) (ledAmount (leds[i].id) * 8.0f), juce::Rectangle<float> (A.mm (8.0f), A.mm (8.0f)).withCentre (leds[i].c));
        for (size_t i = 0; i < meters.size(); ++i)
        {
            const auto& m = meters[i];
            const float lvl = m.id == "vu_l" ? proc.live.vuL.load() : proc.live.vuR.load();
            const auto a = A.toView (m.x - m.w / 2, m.y - m.h / 2), b = A.toView (m.x + m.w / 2, m.y + m.h / 2);
            mark (sigMeter, i, (int) (juce::Decibels::gainToDecibels (std::max (1.0e-4f, lvl), -60.0f) * 1.5f), juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y).expanded (2.0f));
        }
        for (size_t i = 0; i < keys.size(); ++i)
            mark (sigKey, i, proc.live.keyDown[(size_t) (36 + keys[i].i)].load() ? 1 : 0, keys[i].box.expanded (3.0f));

        auto ropeArea = [] (const Rope& r) { return r.path().getBounds().expanded (20.0f).withTrimmedTop (-50.0f).toNearestInt(); };
        for (auto& c : cables)
        {
            if (c.rope.settled() && ! drag.active) continue;
            dirty.add (ropeArea (c.rope));
            c.rope.step (jacks[(size_t) c.src].plugTop, jacks[(size_t) c.dst].plugTop, dt);
            dirty.add (ropeArea (c.rope));
        }
        if (drag.active)
        {
            dirty.add (ropeArea (drag.rope));
            drag.rope.step (jacks[(size_t) drag.fixedJack].plugTop, drag.free, dt);
            dirty.add (ropeArea (drag.rope));
            dirty.add (juce::Rectangle<float> (A.mm (14.0f), A.mm (34.0f)).withCentre (drag.free).toNearestInt());
        }
        if (bubbleTimer > 0 && --bubbleTimer == 0) repaint();
        dirty.consolidate();
        for (const auto& r : dirty) repaint (r);
        if (debugPaint)
        {
            for (const auto& r : dirty) { dbgArea += (double) r.getWidth() * r.getHeight(); ++dbgRects; }
            if (++dbgFrames >= 80)
            {
                std::printf ("paint: %d rects, %.0f kpx/s; ctl %d led %d meter %d key %d\n", dbgRects / 2, dbgArea / 2000.0, dbgCtl, dbgLed, dbgMeter, dbgKey);
                std::fflush (stdout);
                dbgFrames = dbgRects = dbgCtl = dbgLed = dbgMeter = dbgKey = 0; dbgArea = 0;
            }
        }
    }
    bool debugPaint = juce::SystemStats::getEnvironmentVariable ("AT700_DEBUGPAINT", "0") == "1";
    int dbgFrames = 0, dbgRects = 0, dbgCtl = 0, dbgLed = 0, dbgMeter = 0, dbgKey = 0; double dbgArea = 0;

    std::vector<int> sigCtl, sigLed, sigMeter, sigKey;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelView700)
};

// Ventana sobre el lienzo: escala y desplazamiento con transición suave entre vistas.
class PanelPort700 : public juce::Component, private juce::Timer
{
public:
    explicit PanelPort700 (PanelView700& v) : view (v)
    {
        addAndMakeVisible (view);
        view.onScrollRequest = [this] (float dy) { scrollBy (dy); };
    }

    // Vista en mm del lienzo [x0, y0, x1, y1].
    void showRegion (juce::Rectangle<float> mmRect, bool animate = true)
    {
        auto& A = Assets700::get();
        const float pxW = A.mm (mmRect.getWidth()), pxH = mmRect.getHeight() * A.cosT * A.pxmm * 0.5f;
        targetScale = std::min ((float) getWidth() / std::max (1.0f, pxW), (float) getHeight() / std::max (1.0f, pxH));
        targetScale = juce::jlimit (0.2f, 1.6f, targetScale);
        const float cx = A.mm (mmRect.getCentreX()), cy = A.yOf (mmRect.getCentreY());
        targetX = (float) getWidth() * 0.5f - cx * targetScale;
        targetY = (float) getHeight() * 0.5f - cy * targetScale;
        clampTarget();
        if (! animate) { scale = targetScale; tx = targetX; ty = targetY; apply(); }
        else startTimerHz (60);
    }

    void scrollBy (float dy)
    {
        targetY += dy; clampTarget();
        scale = targetScale; tx = targetX; ty = targetY;
        apply();
    }

    void resized() override { clampTarget(); scale = targetScale; tx = targetX; ty = targetY; apply(); }
    void paint (juce::Graphics& g) override { g.fillAll (col700::editorBg); }

private:
    void clampTarget()
    {
        const float w = view.getWidth() * targetScale, h = view.getHeight() * targetScale;
        targetX = w <= getWidth() ? ((float) getWidth() - w) * 0.5f : juce::jlimit ((float) getWidth() - w, 0.0f, targetX);
        targetY = h <= getHeight() ? ((float) getHeight() - h) * 0.5f : juce::jlimit ((float) getHeight() - h, 0.0f, targetY);
    }
    void apply() { view.setTransform (juce::AffineTransform::scale (scale).translated (tx, ty)); }
    void timerCallback() override
    {
        const float k = 0.22f;
        scale += (targetScale - scale) * k; tx += (targetX - tx) * k; ty += (targetY - ty) * k;
        if (std::abs (targetScale - scale) < 0.001f && std::abs (targetX - tx) < 0.5f && std::abs (targetY - ty) < 0.5f)
        { scale = targetScale; tx = targetX; ty = targetY; stopTimer(); }
        apply();
    }
    PanelView700& view;
    float scale = 1.0f, tx = 0.0f, ty = 0.0f, targetScale = 1.0f, targetX = 0.0f, targetY = 0.0f;
};

} // namespace dh
