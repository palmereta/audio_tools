#pragma once
// Panel del AT42: fondo renderizado y, encima, todo lo que se mueve o se ilumina: perillas, botones,
// llaves, LEDs, orbes (con la luz del LED interno y la sombra de la mano), barras OSC STATUS, placas
// del teclado táctil, teclas drone, joystick, pantalla con menú, ranura de cartuchos y la patchera
// con cables físicos (fichas apilables en las salidas, una sola por entrada).

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Assets42.h"
#include "Rope.h"

namespace dh {

class PanelView42 : public juce::Component, private juce::Timer
{
public:
    explicit PanelView42 (AT42Processor& p) : proc (p)
    {
        setSize ((int) A.viewWidth(), (int) A.viewHeight());
        build();
        rebuildCables();
        lastTime = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (40);
    }

    void rebuildCables()
    {
        std::vector<CableView> fresh;
        for (auto& c : proc.getCables())
        {
            CableView cv { c.src, c.dst, c.color, {} };
            bool kept = false;
            for (auto& old : cables) if (old.src == c.src && old.dst == c.dst) { cv.rope = old.rope; kept = true; break; }
            if (! kept) cv.rope.init (jacks[(size_t) c.src].plugTop, jacks[(size_t) c.dst].plugTop, 1.12f, 26.0f);
            fresh.push_back (cv);
        }
        cables = std::move (fresh);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const int pc = panelColour();
        const auto& bg = A.backgrounds[(size_t) pc];
        if (! bg.isValid()) { g.fillAll (col42::panel (pc)); return; }
        g.drawImage (bg, getLocalBounds().toFloat());

        drawBars (g);
        drawOrbs (g);
        drawPlates (g);
        drawDisplay (g);
        drawSlot (g);
        for (auto& l : leds) drawLed (g, l);

        for (auto& c : ctls)
        {
            if (c.kind == Kind::Knob) drawKnob (g, c);
            else if (c.kind == Kind::Button) drawButton (g, c);
            else drawToggle (g, c);
        }
        for (auto& k : dkeys)
            if (proc.live.droneKeyDown[(size_t) k.key].load())
            {
                A.draw (g, A.dkeyDown, A.toView (k.x, k.y));
                const auto w = A.toView (k.x, k.y - 7.6f, 4.3f);
                glow (g, w, juce::Colour (0xffff3020), 1.0f, 9.0f);
            }
        drawJoystick (g);

        // Cables: sombras, fichas, cables, y el que está en la mano.
        for (const auto& c : cables) drawCable (g, c.rope.path(), c.color, true);
        if (drag.active) drawCable (g, drag.rope.path(), drag.color, true);
        for (const auto& c : cables) { drawPlug (g, c.src, c.color); drawPlug (g, c.dst, c.color); }
        if (drag.active) drawPlug (g, drag.fixedJack, drag.color);
        for (const auto& c : cables) drawCable (g, c.rope.path(), c.color, false);
        if (drag.active)
        {
            drawCable (g, drag.rope.path(), drag.color, false);
            A.draw (g, A.plugs[drag.color], drag.free + juce::Point<float> (0.0f, A.mm (A.plugTopZ) * A.sinT), 0, 1.06f);
        }
        if (hoverJack >= 0)
        {
            g.setColour (juce::Colour (0xffffc040).withAlpha (0.8f));
            g.drawEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre (jacks[(size_t) hoverJack].base), 2.0f);
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
            if (c->kind == Kind::Knob)
            {
                if (c->id == "kb_enc") { encDrag = 0.0f; encMoved = false; return; }
                if (auto* prm = c->prm()) prm->beginChangeGesture();
                stepAccum = 0.0f;
            }
            else if (c->kind == Kind::Button) pressButton (*c);
            else cycleToggle (*c, e.position);
            repaint();
            return;
        }
        if (const int pl = plateAt (e.position); pl >= 0) { mousePlate = pl; proc.platePress (pl, platePressure (pl, e.position)); repaint(); return; }
        for (auto& k : dkeys)
            if (k.box.contains (e.position)) { mouseKey = k.key; proc.droneKeyUi[(size_t) k.key].store (true); repaint(); return; }
        if (joyBox.contains (e.position)) { draggingJoy = true; setJoy (e.position); return; }
        if (slotBox.contains (e.position)) { cartridgeMenu(); return; }
        if (displayBox.contains (e.position)) { menuPage = (menuPage + (e.mods.isPopupMenu() ? kPages - 1 : 1)) % kPages; repaint(); return; }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        const float dy = lastMouse.y - e.position.y;
        lastMouse = e.position;
        if (drag.active)
        {
            drag.free = e.position;
            const int j = jackAt (e.position);
            hoverJack = canConnect (drag.fixedJack, j) ? j : -1;
            return;
        }
        if (draggingJoy) { setJoy (e.position); return; }
        if (mousePlate >= 0)
        {
            const int pl = plateAt (e.position);
            if (pl != mousePlate) { proc.plateRelease (mousePlate); mousePlate = pl; if (pl >= 0) proc.platePress (pl, platePressure (pl, e.position)); }
            return;
        }
        if (active == nullptr || active->kind != Kind::Knob) return;
        auto& c = *active;
        if (c.id == "kb_enc")
        {
            encDrag += dy; encAngle += dy * 3.0f;
            if (std::abs (encDrag) > 10.0f) { encMoved = true; stepMenu (encDrag > 0 ? 1 : -1); encDrag = 0.0f; }
            repaint();
            return;
        }
        const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
        if (c.steps > 0)
        {
            stepAccum += dy;
            if (std::abs (stepAccum) > 16.0f)
            {
                setParam (c, (float) juce::jlimit (0, c.steps - 1, (int) std::lround (c.value()) + (stepAccum > 0 ? 1 : -1)));
                stepAccum = 0.0f;
            }
        }
        else setParam (c, juce::jlimit (0.0f, 1.0f, c.value() + dy / 200.0f * fine));
        showBubble = true;
        repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (drag.active) { finishCable (e.position); return; }
        if (active != nullptr && active->kind == Kind::Knob)
        {
            if (active->id == "kb_enc" && ! encMoved) menuPage = (menuPage + 1) % kPages;
            else if (auto* prm = active->prm()) prm->endChangeGesture();
        }
        if (active != nullptr && active->kind == Kind::Button && active->momentary) pressedMomentary = nullptr;
        active = nullptr; showBubble = false; draggingJoy = false;
        if (mousePlate >= 0) { proc.plateRelease (mousePlate); mousePlate = -1; }
        if (mouseKey >= 0) { proc.droneKeyUi[(size_t) mouseKey].store (false); mouseKey = -1; }
        repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        auto* c = ctlAt (e.position);
        if (c != hover) { hover = c; repaint(); }
        // La mano sobre un orbe le hace sombra (más cerca del centro, más oscuro).
        for (size_t i = 0; i < orbs.size(); ++i)
        {
            const float d = e.position.getDistanceFrom (orbs[i].c) / std::max (1.0f, orbs[i].r * 2.2f);
            proc.orbShade[i].store (juce::jlimit (0.0f, 0.95f, 1.0f - d));
        }
        const int j = jackAt (e.position);
        setMouseCursor (c != nullptr || j >= 0 || plateAt (e.position) >= 0 || joyBox.contains (e.position) || slotBox.contains (e.position)
                        ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void mouseExit (const juce::MouseEvent&) override { for (auto& s : proc.orbShade) s.store (0.0f); hover = nullptr; repaint(); }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (auto* c = ctlAt (e.position); c != nullptr && c->kind == Kind::Knob && c->param >= 0)
            setParam (*c, panelParam42 (c->param).def);
        if (joyBox.contains (e.position)) { proc.setValue (P42::joy_x, 0.5f); proc.setValue (P42::joy_y, 0.5f); }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        auto* c = ctlAt (e.position);
        if (c == nullptr || c->kind != Kind::Knob) return;
        const float d = w.isReversed ? -w.deltaY : w.deltaY;
        if (c->id == "kb_enc") { if (std::abs (d) > 0.01f) stepMenu (d > 0 ? 1 : -1); encAngle += d * 40.0f; repaint(); return; }
        if (c->steps > 0) { if (std::abs (d) > 0.01f) setParam (*c, (float) juce::jlimit (0, c->steps - 1, (int) std::lround (c->value()) + (d > 0 ? 1 : -1))); }
        else setParam (*c, juce::jlimit (0.0f, 1.0f, c->value() + d * 0.06f));
        hover = c; bubbleTimer = 25;
        repaint();
    }

private:
    enum class Kind { Knob, Button, Toggle };
    struct Ctl
    {
        Kind kind; juce::String id; int param = -1;
        float x = 0, y = 0, d = 8, a0 = -150, a1 = 150;
        int steps = 0, positions = 2;
        juce::String sprite, color;
        bool momentary = false;
        juce::Rectangle<float> box;
        AT42Processor* proc = nullptr;
        juce::RangedAudioParameter* prm() const { return param >= 0 ? proc->apvts.getParameter (panelParam42 (param).id) : nullptr; }
        juce::String paramId() const { return param >= 0 ? juce::String (panelParam42 (param).id) : juce::String(); }
        float value() const { return param >= 0 ? proc->value ((P42) param) : 0.0f; }
    };
    struct LedV { juce::String id; juce::Point<float> c; juce::Colour col; };
    struct Orb { int voice; juce::Point<float> c; float r; };
    struct Bar { int voice; float x0, y0, x1, y1; };
    struct Plate { int i; juce::Rectangle<float> box; };
    struct DKey { int key; float x, y; juce::Rectangle<float> box; };
    struct JackV { int index; juce::Point<float> base, plugTop; bool isOutput; };
    struct CableView { int src, dst; juce::String color; Rope rope; };
    struct DragCable { bool active = false; int fixedJack = -1; juce::Point<float> free; juce::String color; Rope rope; };

    AT42Processor& proc;
    Assets42& A = Assets42::get();
    std::vector<Ctl> ctls;
    std::vector<LedV> leds;
    std::vector<Orb> orbs;
    std::vector<Bar> bars;
    std::vector<Plate> plates;
    std::vector<DKey> dkeys;
    std::vector<JackV> jacks;
    std::vector<CableView> cables;
    DragCable drag;
    juce::Rectangle<float> joyBox, slotBox, displayBox;
    juce::Point<float> joyCentre; float joyTravel = 18.0f;
    Ctl* active = nullptr; Ctl* hover = nullptr; Ctl* pressedMomentary = nullptr;
    juce::Point<float> lastMouse;
    float stepAccum = 0.0f, encDrag = 0.0f, encAngle = 0.0f;
    bool encMoved = false, showBubble = false, draggingJoy = false;
    int bubbleTimer = 0, mousePlate = -1, mouseKey = -1, hoverJack = -1, colorCycle = 0, menuPage = 0;
    double lastTime = 0.0;
    static constexpr int kPages = 17;

    int panelColour() const { return juce::jlimit (0, 3, (int) proc.extValue ("panel")); }

    void build()
    {
        const auto& L = A.layout;
        auto pidx = [] (const juce::String& id) { for (int i = 0; i < kNumP42; ++i) if (id == panelParam42 (i).id) return i; return -1; };
        if (auto* arr = L.getProperty ("knobs", {}).getArray())
            for (auto& k : *arr)
            {
                Ctl c { Kind::Knob, k["id"].toString(), pidx (k["id"].toString()) };
                c.x = k["x"]; c.y = k["y"]; c.d = k["d"]; c.steps = k["steps"]; c.a0 = k["a0"]; c.a1 = k["a1"];
                c.sprite = k["size"].toString() + "_" + k["color"].toString();
                const auto ctr = A.toView (c.x, c.y, 5.0f); const float r = A.mm (c.d) * 0.55f + 2.0f;
                c.box = juce::Rectangle<float> (r * 2, r * 2).withCentre (ctr);
                c.proc = &proc; ctls.push_back (c);
            }
        if (auto* arr = L.getProperty ("buttons", {}).getArray())
            for (auto& b : *arr)
            {
                Ctl c { Kind::Button, b["id"].toString(), pidx (b["id"].toString()) };
                c.x = b["x"]; c.y = b["y"]; c.d = b["d"]; c.color = b["color"].toString(); c.momentary = b["kind"].toString() == "momentary";
                c.sprite = c.color + "_" + juce::String ((float) b["d"], 1);
                const auto ctr = A.toView (c.x, c.y, 3.0f); const float r = A.mm (c.d) * 0.5f + 3.0f;
                c.box = juce::Rectangle<float> (r * 2, r * 2).withCentre (ctr);
                c.proc = &proc; ctls.push_back (c);
            }
        if (auto* arr = L.getProperty ("toggles", {}).getArray())
            for (auto& t : *arr)
            {
                Ctl c { Kind::Toggle, t["id"].toString(), pidx (t["id"].toString()) };
                c.x = t["x"]; c.y = t["y"]; c.positions = t["positions"]; c.steps = c.positions;
                const auto ctr = A.toView (c.x, c.y, 4.0f);
                c.box = juce::Rectangle<float> (18.0f, 26.0f).withCentre (ctr);
                c.proc = &proc; ctls.push_back (c);
            }
        if (auto* arr = L.getProperty ("leds", {}).getArray())
            for (auto& l : *arr) leds.push_back ({ l["id"].toString(), A.toView (l["x"], l["y"], 1.2f), col42::led (l["color"].toString()) });
        if (auto* arr = L.getProperty ("orbs", {}).getArray())
            for (auto& o : *arr) orbs.push_back ({ (int) o["voice"], A.toView (o["x"], o["y"], 5.0f), A.mm (o["d"]) * 0.5f });
        if (auto* arr = L.getProperty ("bars", {}).getArray())
            for (auto& b : *arr) bars.push_back ({ b["id"].toString().substring (1, 2).getIntValue(), b["x0"], b["y0"], b["x1"], b["y1"] });
        if (auto* arr = L.getProperty ("plates", {}).getArray())
            for (auto& pl : *arr)
            {
                const float x = pl["x"], y = pl["y"], w = pl["w"], h = pl["h"];
                const auto a = A.toView (x - w / 2, y - h / 2, 1.4f), b = A.toView (x + w / 2, y + h / 2, 1.4f);
                plates.push_back ({ (int) pl["i"], juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y) });
            }
        if (auto* arr = L.getProperty ("dronekeys", {}).getArray())
            for (auto& k : *arr)
            {
                const float x = k["x"], y = k["y"], s = k["s"];
                const auto a = A.toView (x - s / 2, y - s / 2, 6.0f), b = A.toView (x + s / 2, y + s / 2);
                dkeys.push_back ({ (int) k["key"], x, y, juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y) });
            }
        for (int j = 0; j < jk42::kNumJacks; ++j)
        {
            const auto& d = jk42::jacks[j];
            jacks.push_back ({ j, A.toView (d.x, d.y, 3.4f), A.toView (d.x, d.y, A.plugTopZ), d.isOutput });
        }
        const auto js = L.getProperty ("joystick", {});
        joyCentre = A.toView (js["x"], js["y"], 9.0f);
        joyTravel = A.mm (js["travel"]);
        joyBox = juce::Rectangle<float> (A.mm (js["d"]) + 8.0f, A.mm (js["d"]) + 8.0f).withCentre (joyCentre);
        const auto s = L.getProperty ("slot", {});
        { const auto a = A.toView (s["x0"], s["y0"], 1.0f), b = A.toView (s["x1"], s["y1"], 1.0f); slotBox = juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y); }
        const auto dsp = L.getProperty ("display", {});
        { const auto a = A.toView (dsp["x0"], dsp["y0"], 1.25f), b = A.toView (dsp["x1"], dsp["y1"], 1.25f); displayBox = juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, b.x, b.y); }
    }

    Ctl* ctlAt (juce::Point<float> p)
    {
        for (auto& c : ctls) if (c.box.contains (p)) return &c;
        return nullptr;
    }

    int jackAt (juce::Point<float> p) const
    {
        for (const auto& j : jacks)
            if (p.getDistanceFrom (j.base) < 9.0f || p.getDistanceFrom (j.plugTop) < 6.0f) return j.index;
        return -1;
    }

    int plateAt (juce::Point<float> p) const
    {
        for (auto& pl : plates) if (pl.box.contains (p)) return pl.i;
        return -1;
    }

    float platePressure (int pl, juce::Point<float> p) const
    {
        for (auto& x : plates) if (x.i == pl) return juce::jlimit (0.2f, 1.0f, 0.25f + 0.9f * (p.y - x.box.getY()) / x.box.getHeight());
        return 0.8f;
    }

    void setParam (Ctl& c, float v) { if (auto* prm = c.prm()) prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }

    void pressButton (Ctl& c)
    {
        if (c.id == "kb_down" || c.id == "kb_up")
        {
            pressedMomentary = &c;
            const float o = proc.extValue ("kb_octave");
            proc.setExtValue ("kb_octave", juce::jlimit (0.0f, 6.0f, o + (c.id == "kb_up" ? 1.0f : -1.0f)));
            menuPage = 3;
            return;
        }
        setParam (c, c.value() > 0.5f ? 0.0f : 1.0f);
    }

    void cycleToggle (Ctl& c, juce::Point<float> p)
    {
        const auto ctr = A.toView (c.x, c.y, 4.0f);
        int v = (int) std::lround (c.value());
        if (c.positions == 3) v = p.y < ctr.y - 4.0f ? 0 : (p.y > ctr.y + 4.0f ? 2 : 1);
        else v = 1 - v;
        setParam (c, (float) v);
    }

    void setJoy (juce::Point<float> p)
    {
        const auto d = (p - joyCentre) / joyTravel;
        proc.setValue (P42::joy_x, juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * d.x));
        proc.setValue (P42::joy_y, juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * d.y));
        repaint();
    }

    // ── menú de la pantalla del teclado ──
    struct Page { const char* label; const char* id; };
    static const Page& page (int i)
    {
        static const Page pages[kPages] = {
            { "MODE", "kb_mode" }, { "SCALE", "kb_scale" }, { "ROOT", "kb_root" }, { "OCTAVE", "kb_octave" },
            { "ARP DIR", "kb_dir" }, { "ARP HOLD", "kb_hold" }, { "CLOCK", "kb_div" }, { "TEMPO", "kb_bpm" },
            { "PORTAMENTO", "kb_porta" }, { "LEGATO", "kb_legato" }, { "VIBRATO", "kb_vib" }, { "VIB SPEED", "kb_vibspd" },
            { "PRESSURE", "kb_press" }, { "RISE", "kb_rise" }, { "FALL", "kb_fall" }, { "SEQ LENGTH", "kb_seqlen" }, { "SEQ RECORD", nullptr },
        };
        return pages[i];
    }

    void stepMenu (int d)
    {
        const auto& pg = page (menuPage);
        if (pg.id == nullptr) { proc.sendCommand (AT42Processor::CmdSeqRec); return; }
        const auto* e = findExt42 (pg.id);
        const float v = proc.extValue (pg.id);
        if (e->steps > 0) proc.setExtValue (pg.id, (float) juce::jlimit (0, e->steps - 1, (int) std::lround (v) + d));
        else proc.setExtValue (pg.id, juce::jlimit (e->lo, e->hi, v + (e->hi - e->lo) * (juce::String (pg.id) == "kb_bpm" ? 0.0035f : 0.02f) * (float) d));
    }

    void cartridgeMenu()
    {
        juce::PopupMenu m, ml, mr, mb;
        const auto names = panelChoices42 ((int) P42::fx_cart_l);
        for (int i = 0; i < names.size(); ++i)
        {
            ml.addItem (100 + i, names[i], true, (int) proc.value (P42::fx_cart_l) == i);
            mr.addItem (200 + i, names[i], true, (int) proc.value (P42::fx_cart_r) == i);
            mb.addItem (300 + i, names[i]);
        }
        m.addSectionHeader ("Load a cartridge");
        m.addSubMenu ("Channel L", ml); m.addSubMenu ("Channel R", mr); m.addSubMenu ("Both channels", mb);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (slotBox.toNearestInt())), [this] (int r) {
            if (r >= 100 && r < 200) proc.setValue (P42::fx_cart_l, (float) (r - 100));
            else if (r >= 200 && r < 300) proc.setValue (P42::fx_cart_r, (float) (r - 200));
            else if (r >= 300) { proc.setValue (P42::fx_cart_l, (float) (r - 300)); proc.setValue (P42::fx_cart_r, (float) (r - 300)); }
            repaint();
        });
    }

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

    bool canConnect (int fixedJack, int target) const
    {
        if (fixedJack < 0 || target < 0 || fixedJack == target) return false;
        return jk42::jacks[fixedJack].isOutput != jk42::jacks[target].isOutput;
    }

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
            // Desde una salida con cables se puede sacar uno nuevo con Shift (fichas apiladas).
            if (e.mods.isShiftDown() && jk42::jacks[j].isOutput) break;
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
        if (canConnect (drag.fixedJack, target))
        {
            Cable42 c;
            const bool fixedOut = jk42::jacks[drag.fixedJack].isOutput;
            c.src = fixedOut ? drag.fixedJack : target; c.dst = fixedOut ? target : drag.fixedJack; c.color = drag.color;
            cs.erase (std::remove_if (cs.begin(), cs.end(), [&] (auto& x) { return x.dst == c.dst; }), cs.end());
            cs.push_back (c);
            Rope r = drag.rope;
            r.length = jacks[(size_t) c.src].plugTop.getDistanceFrom (jacks[(size_t) c.dst].plugTop) * 1.12f + 26.0f;
            if (! fixedOut) { std::reverse (r.p.begin(), r.p.end()); std::reverse (r.prev.begin(), r.prev.end()); }
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
        const auto& d = jk42::jacks[jack];
        A.draw (g, it->second, A.toView (d.x, d.y));
    }

    void drawCable (juce::Graphics& g, const juce::Path& path, const juce::String& color, bool shadow) const
    {
        if (shadow)
        {
            const auto t = juce::AffineTransform::translation (5.0f, 9.0f);
            g.setColour (juce::Colours::black.withAlpha (0.10f));
            g.strokePath (path, juce::PathStrokeType (11.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
            g.setColour (juce::Colours::black.withAlpha (0.2f));
            g.strokePath (path, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), t);
            return;
        }
        const auto base = col42::cable (color);
        g.setColour (base.darker (0.75f));
        g.strokePath (path, juce::PathStrokeType (5.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (base);
        g.strokePath (path, juce::PathStrokeType (4.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (color == "white" ? 0.55f : 0.3f));
        g.strokePath (path, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded),
                      juce::AffineTransform::translation (-0.9f, -1.2f));
    }

    // ── dibujo de controles ──
    void drawKnob (juce::Graphics& g, const Ctl& c) const
    {
        auto it = A.knobs.find (c.sprite);
        if (it == A.knobs.end()) return;
        float ang;
        if (c.id == "kb_enc") ang = encAngle;
        else if (c.steps > 0) ang = c.a0 + (c.a1 - c.a0) * juce::jlimit (0.0f, 1.0f, c.value() / (float) (c.steps - 1));
        else ang = c.a0 + (c.a1 - c.a0) * c.value();
        const int frames = it->second.frames;
        const int f = ((int) std::lround (ang / 360.0f * (float) frames) % frames + frames) % frames;
        A.draw (g, it->second, A.toView (c.x, c.y), f);
    }

    void drawButton (juce::Graphics& g, const Ctl& c) const
    {
        const bool down = c.momentary ? pressedMomentary == &c : c.value() > 0.5f;
        if (! down) return;
        auto it = A.buttons.find (c.sprite);
        if (it != A.buttons.end()) A.draw (g, it->second, A.toView (c.x, c.y, c.color == "red" ? 1.0f : 0.0f));
    }

    void drawToggle (juce::Graphics& g, const Ctl& c) const
    {
        const int v = (int) std::lround (c.value());
        const Sprite42* s = &A.batMid;
        if (c.positions == 3) s = v == 0 ? &A.batUp : (v == 2 ? &A.batDown : &A.batMid);
        else s = v == 0 ? &A.batUp : &A.batDown;
        A.draw (g, *s, A.toView (c.x, c.y));
    }

    static void glow (juce::Graphics& g, juce::Point<float> c, juce::Colour col, float amount, float radius)
    {
        if (amount <= 0.01f) return;
        juce::ColourGradient gr (col.withAlpha (0.55f * amount), c.x, c.y, col.withAlpha (0.0f), c.x + radius, c.y, true);
        g.setGradientFill (gr);
        g.fillEllipse (juce::Rectangle<float> (radius * 2, radius * 2).withCentre (c));
        g.setColour (col.brighter (0.6f).withAlpha (std::min (1.0f, amount)));
        g.fillEllipse (juce::Rectangle<float> (radius * 0.42f, radius * 0.42f).withCentre (c));
    }

    float ledAmount (const juce::String& id) const
    {
        auto& L = proc.live;
        if (id.endsWith ("_gate") && id.startsWith ("d")) return L.droneEnv[(size_t) id.substring (1, 2).getIntValue()].load();
        if (id.endsWith ("_sh")) return L.shFlash[(size_t) id.substring (1, 2).getIntValue()].load() ? 1.0f : 0.0f;
        if (id == "ega") return L.egA.load();
        if (id == "egb") return L.egB.load();
        if (id == "lfoa") return L.lfoA.load();
        if (id == "lfob") return L.lfoB.load();
        if (id == "joy_x") return std::abs (proc.value (P42::joy_x) - 0.5f) * 2.0f;
        if (id == "joy_y") return std::abs (proc.value (P42::joy_y) - 0.5f) * 2.0f;
        if (id.startsWith ("seq_step")) return (L.seqStep.load() == id.getLastCharacters (1).getIntValue() - 1) ? 1.0f : 0.0f;
        if (id == "pre_clip") return L.preClip.load() ? 1.0f : 0.0f;
        if (id == "ef_env") return juce::jlimit (0.0f, 1.0f, L.efEnv.load() * 4.0f);
        if (id == "ef_gate") return L.efGate.load() ? 1.0f : 0.0f;
        return 0.0f;
    }

    void drawLed (juce::Graphics& g, const LedV& l) const { glow (g, l.c, l.col, ledAmount (l.id), 9.0f); }

    void drawOrbs (juce::Graphics& g) const
    {
        for (auto& o : orbs)
        {
            const float led = proc.live.orbLed[(size_t) o.voice].load(), light = proc.live.orbLight[(size_t) o.voice].load();
            const float shade = proc.orbShade[(size_t) (o.voice <= 2 ? o.voice - 1 : o.voice - 2)].load();
            // LED rojo interno: el domo se tiñe desde adentro.
            if (led > 0.01f)
            {
                juce::ColourGradient gr (juce::Colour (0xffff4a2a).withAlpha (juce::jlimit (0.0f, 0.85f, led * 0.9f)), o.c.x, o.c.y + o.r * 0.1f,
                                         juce::Colour (0xffff2a10).withAlpha (0.0f), o.c.x + o.r * 1.15f, o.c.y, true);
                g.setGradientFill (gr);
                g.fillEllipse (juce::Rectangle<float> (o.r * 2.3f, o.r * 1.9f).withCentre (o.c));
            }
            // Sombra de la mano.
            if (shade > 0.02f)
            {
                g.setColour (juce::Colours::black.withAlpha (0.35f * shade));
                g.fillEllipse (juce::Rectangle<float> (o.r * 2.0f, o.r * 1.7f).withCentre (o.c));
            }
            juce::ignoreUnused (light);
        }
    }

    void drawBars (juce::Graphics& g) const
    {
        for (auto& b : bars)
        {
            const int v = b.voice;
            const int base = v == 1 ? (int) P42::d1_mute1 : v == 2 ? (int) P42::d2_mute1 : v == 4 ? (int) P42::d4_mute1 : (int) P42::d5_mute1;
            const float env = proc.live.droneEnv[(size_t) v].load();
            for (int i = 0; i < 5; ++i)
            {
                const bool on = proc.value ((P42) (base + 2 * i)) < 0.5f;
                const float w = (b.x1 - b.x0 - 2.4f) / 5.0f;
                const float x0 = b.x0 + 1.2f + (float) i * w;
                const auto a = A.toView (x0 + 0.35f, b.y0 + 1.3f, 0.3f), c = A.toView (x0 + w - 0.35f, b.y1 - 1.3f, 0.3f);
                const auto r = juce::Rectangle<float>::leftTopRightBottom (a.x, a.y, c.x, c.y);
                g.setColour (on ? juce::Colour (0xffff2f1a).withAlpha (0.55f + 0.45f * env) : juce::Colour (0xff3a3a3a));
                g.fillRect (r);
                if (on) { g.setColour (juce::Colour (0xffff2f1a).withAlpha (0.25f * (0.5f + env))); g.fillRect (r.expanded (1.5f)); }
            }
        }
    }

    void drawPlates (juce::Graphics& g) const
    {
        for (auto& pl : plates)
            if (proc.live.plateDown[(size_t) pl.i].load() || mousePlate == pl.i)
            {
                g.setColour (juce::Colour (0xffffd29a).withAlpha (0.22f));
                g.fillRoundedRectangle (pl.box.reduced (1.0f), 2.0f);
                g.setColour (juce::Colour (0xffffb040).withAlpha (0.8f));
                g.drawRoundedRectangle (pl.box.reduced (1.0f), 2.0f, 1.4f);
            }
    }

    void drawDisplay (juce::Graphics& g) const
    {
        const auto r = displayBox.reduced (2.0f, 1.5f);
        const auto& pg = page (menuPage);
        juce::String value;
        if (pg.id == nullptr) value = proc.live.recording.load() ? "REC " + juce::String (proc.live.kbSeqCount.load()) + "/16" : "press enc";
        else value = formatExt42 (*findExt42 (pg.id), proc.extValue (pg.id));
        g.setColour (juce::Colour (0xff9fe1ff).withAlpha (0.9f));
        g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), r.getHeight() * 0.36f, juce::Font::bold));
        g.drawText (pg.label, r.withHeight (r.getHeight() * 0.48f), juce::Justification::centredLeft);
        g.setColour (juce::Colours::white);
        g.drawText (value.toUpperCase().substring (0, 14), r.withTrimmedTop (r.getHeight() * 0.48f), juce::Justification::centredLeft);
    }

    void drawSlot (juce::Graphics& g) const
    {
        const auto names = panelChoices42 ((int) P42::fx_cart_l);
        const auto l = names[(int) proc.value (P42::fx_cart_l)], rr = names[(int) proc.value (P42::fx_cart_r)];
        const auto r = slotBox.reduced (A.mm (2.2f), A.mm (2.0f));
        g.setColour (juce::Colour (0xffe8e2d4));
        g.fillRoundedRectangle (r, 1.5f);
        g.setColour (juce::Colour (0xff1a1a1a));
        g.setFont (col42::font (r.getHeight() * 0.42f));
        g.drawText (l == rr ? l.toUpperCase() : (l.toUpperCase() + " / " + rr.toUpperCase()), r.reduced (2.0f, 0), juce::Justification::centred);
    }

    void drawJoystick (juce::Graphics& g) const
    {
        const float jx = proc.value (P42::joy_x) * 2.0f - 1.0f, jy = proc.value (P42::joy_y) * 2.0f - 1.0f;
        const auto c = joyCentre + juce::Point<float> (jx, jy) * joyTravel;
        A.draw (g, A.joyCap, c + juce::Point<float> (0.0f, A.mm (9.0f) * A.sinT));
    }

    void drawBubble (juce::Graphics& g) const
    {
        const Ctl* c = active != nullptr && showBubble ? active : (bubbleTimer > 0 ? hover : nullptr);
        if (c == nullptr || c->param < 0 || c->kind != Kind::Knob) return;
        const auto title = juce::String (panelParam42 (c->param).name);
        const auto value = formatPanel42 (c->param, c->value());
        const auto f1 = col42::font (11.0f), f2 = col42::font (13.0f);
        const float w = std::max (juce::GlyphArrangement::getStringWidth (f1, title), juce::GlyphArrangement::getStringWidth (f2, value)) + 22.0f;
        auto r = juce::Rectangle<float> (w, 40.0f).withCentre ({ c->box.getCentreX(), c->box.getY() - 24.0f }).constrainedWithin (getLocalBounds().toFloat().reduced (4.0f));
        g.setColour (juce::Colour (0xf0161617)); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colour (0xff3a3a3d)); g.drawRoundedRectangle (r, 5.0f, 1.0f);
        g.setColour (juce::Colour (0xffb9b6ad)); g.setFont (f1); g.drawText (title.toUpperCase(), r.withHeight (20.0f).translated (0, 2), juce::Justification::centred);
        g.setColour (juce::Colours::white); g.setFont (f2); g.drawText (value, r.withTrimmedTop (18.0f), juce::Justification::centred);
    }

    // Sólo se redibuja lo que cambió (cada elemento tiene su firma). Redibujar el panel completo
    // en cada parpadeo de un LED costaba ~20 % de un núcleo con la ventana abierta.
    void timerCallback() override
    {
        const double now = juce::Time::getMillisecondCounterHiRes();
        const float dt = (float) juce::jlimit (0.001, 0.05, (now - lastTime) * 0.001);
        lastTime = now;
        juce::RectangleList<int> dirty;
        auto mark = [&] (std::vector<int>& last, size_t i, int sig, juce::Rectangle<float> r) {
            if (last.size() <= i) last.resize (i + 1, INT_MIN);
            if (last[i] != sig) { last[i] = sig; dirty.add (r.toNearestInt()); }
        };
        auto& L = proc.live;
        const int pc = panelColour();
        if (pc != shownPanel) { shownPanel = pc; repaint(); return; }

        for (size_t i = 0; i < ctls.size(); ++i)
        {
            const auto& c = ctls[i];
            int sig = (int) std::lround (c.value() * 4000.0f);
            if (c.id == "kb_enc") sig = (int) encAngle;
            if (c.kind == Kind::Button && c.momentary) sig = pressedMomentary == &c ? 1 : 0;
            const float sz = A.mm (c.d + 12.0f);
            mark (sigCtl, i, sig, juce::Rectangle<float> (sz, sz + 8.0f).withCentre (A.toView (c.x, c.y, 3.0f)));
        }
        for (size_t i = 0; i < leds.size(); ++i)
            mark (sigLed, i, (int) (ledAmount (leds[i].id) * 24.0f), juce::Rectangle<float> (24.0f, 24.0f).withCentre (leds[i].c));
        for (size_t i = 0; i < orbs.size(); ++i)
        {
            const auto& o = orbs[i];
            const int sig = (int) (L.orbLed[(size_t) o.voice].load() * 40.0f) * 100 + (int) (proc.orbShade[i].load() * 40.0f);
            mark (sigOrb, i, sig, juce::Rectangle<float> (o.r * 2.6f, o.r * 2.4f).withCentre (o.c));
        }
        for (size_t i = 0; i < bars.size(); ++i)
        {
            const auto& b = bars[i];
            const int base = b.voice == 1 ? (int) P42::d1_mute1 : b.voice == 2 ? (int) P42::d2_mute1 : b.voice == 4 ? (int) P42::d4_mute1 : (int) P42::d5_mute1;
            int sig = (int) (L.droneEnv[(size_t) b.voice].load() * 20.0f);
            for (int k = 0; k < 5; ++k) sig = sig * 2 + (proc.value ((P42) (base + 2 * k)) < 0.5f ? 1 : 0);
            const auto a0 = A.toView (b.x0, b.y0), a1 = A.toView (b.x1, b.y1);
            mark (sigBar, i, sig, juce::Rectangle<float>::leftTopRightBottom (a0.x, a0.y, a1.x, a1.y).expanded (3.0f));
        }
        for (size_t i = 0; i < plates.size(); ++i)
            mark (sigPlate, i, (L.plateDown[(size_t) plates[i].i].load() || mousePlate == plates[i].i) ? 1 : 0, plates[i].box.expanded (3.0f));
        for (size_t i = 0; i < dkeys.size(); ++i)
            mark (sigKey, i, L.droneKeyDown[(size_t) dkeys[i].key].load() ? 1 : 0, dkeys[i].box.expanded (8.0f, 14.0f));
        mark (sigMisc, 0, (int) (proc.value (P42::joy_x) * 500.0f) * 1000 + (int) (proc.value (P42::joy_y) * 500.0f), joyBox.expanded (20.0f));
        {
            const auto& pg = page (menuPage);
            const int v = pg.id == nullptr ? (L.recording.load() ? 1000 + L.kbSeqCount.load() : 0) : (int) (proc.extValue (pg.id) * 100.0f);
            mark (sigMisc, 1, menuPage * 100000 + v, displayBox.expanded (2.0f));
        }
        mark (sigMisc, 2, (int) proc.value (P42::fx_cart_l) * 100 + (int) proc.value (P42::fx_cart_r), slotBox.expanded (2.0f));
        for (auto& f : L.shFlash) f.store (false);
        L.preClip.store (false);

        auto ropeArea = [] (const Rope& r) { return r.path().getBounds().expanded (16.0f).withTrimmedTop (-40.0f).toNearestInt(); };
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
            dirty.add (juce::Rectangle<float> (50.0f, 80.0f).withCentre (drag.free).toNearestInt());
        }
        if (bubbleTimer > 0 && --bubbleTimer == 0) repaint();
        dirty.consolidate();
        for (const auto& r : dirty) repaint (r);
    }

    std::vector<int> sigCtl, sigLed, sigOrb, sigBar, sigPlate, sigKey, sigMisc;
    int shownPanel = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PanelView42)
};

} // namespace dh
