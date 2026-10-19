#pragma once
// El frente de 1U, vivo: las perillas ATTACK/RELEASE/FORMANT/FM y VOLUME mueven sus parámetros; los
// botones cambian de página como en el original (PLAY, EDIT PERFORM/EFFECT/VOICE, UTIL, SEARCH) y la
// botonera de datos (PART, CURSOR, VALUE, ENTER, EXIT) recorre el editor. El botón entre los dos LED
// pasa las perillas a la fila de abajo: 1 operador, 2 página, 3 cursor, 4 valor. MUTE/SOLO alterna
// todo / sólo con voz / sólo sin voz. El LCD muestra el preset y lo último que se tocó.

#include <JuceHeader.h>
#include "AssetsFs.h"
#include "Widgets.h"

namespace dh::fs::ui {

class RackView : public juce::Component, private juce::Timer
{
public:
    std::function<void (const juce::String&)> onButton;       // id del botón
    std::function<void (int, int)> onDataKnob;                // perilla 0..3, pasos
    std::function<juce::String()> line1, line2;               // contenido normal del LCD
    bool dataMode = false;

    explicit RackView (ATFS1Processor& p) : proc (p)
    {
        const auto& A = Assets::get();
        if (auto* ks = A.layout.getProperty ("knobs", {}).getArray())
            for (auto& k : *ks) knobs.push_back ({ k["id"].toString(), k["param"].toString(), (float) k["x"], (float) k["y"], k["kind"].toString() == "big" });
        if (auto* bs = A.layout.getProperty ("buttons", {}).getArray())
            for (auto& b : *bs) buttons.push_back ({ b["id"].toString(), (float) b["x"], (float) b["y"], (float) b["d"], b["kind"].toString() });
        if (auto* ls = A.layout.getProperty ("leds", {}).getArray())
            for (auto& l : *ls) leds.push_back ({ (float) l["x"], (float) l["y"] });
        const auto g = A.layout.getProperty ("lcd", {}).getProperty ("glass", {});
        glass = juce::Rectangle<float>::leftTopRightBottom ((float) g["x0"], (float) g["y0"], (float) g["x1"], (float) g["y1"]);
        if (auto* ll = A.layout.getProperty ("lcdLabels", {}).getArray())
            for (auto& l : *ll) lcdMarks.push_back ({ l["text"].toString(), (float) l["x"] });
        const auto pw = A.layout.getProperty ("power", {});
        power = juce::Rectangle<float>::leftTopRightBottom ((float) pw["x0"], (float) pw["y0"], (float) pw["x1"], (float) pw["y1"]);
        Bus::get().lcd = [this] (const juce::String& n, const juce::String& v) { flash (n, v); };
        startTimerHz (30);
        setRepaintsOnMouseActivity (false);
    }
    ~RackView() override { Bus::get().lcd = nullptr; }

    void flash (const juce::String& name, const juce::String& value)
    {
        flashText = name.toUpperCase().substring (0, 14).paddedRight (' ', 14) + value.substring (0, 10).paddedLeft (' ', 10);
        flashUntil = juce::Time::getMillisecondCounter() + 1600;
        repaint (lcdBounds().toNearestInt().expanded (4));
    }
    bool powerOn = true;

    void paint (juce::Graphics& g) override
    {
        const auto& A = Assets::get();
        const float s = scale();
        g.addTransform (juce::AffineTransform::scale (s));
        if (A.background.isValid()) g.drawImage (A.background, juce::Rectangle<float> (0, 0, A.viewWidth(), A.viewHeight()));
        else { g.setColour (juce::Colour (0xff8796a6)); g.fillRect (0.0f, 0.0f, A.viewWidth(), A.viewHeight()); }
        // Perillas.
        for (auto& k : knobs)
        {
            float v = 0.5f;
            if (auto* prm = proc.apvts.getParameter (k.param)) v = prm->getValue();
            if (dataMode && k.big) v = 0.5f + 0.5f * std::sin ((float) dataAngle[(size_t) (k.id.getLastCharacter() - '1')] * 0.05f);
            const float deg = -150.0f + 300.0f * v;
            const int frame = ((int) std::lround (deg / 360.0f * 128.0f) % 128 + 128) % 128;
            A.draw (g, k.big ? A.knobBig : A.knobSmall, A.toView (k.x, k.y), frame);
        }
        // Botones apretados (el fondo ya los tiene arriba).
        for (auto& b : buttons)
        {
            if (b.kind == "hidden") continue;
            const bool down = b.id == pressed;
            const bool page = b.id == litPage;
            if (down) A.draw (g, b.kind == "grey" ? A.btnGreyDown : A.btnBlackDown, A.toView (b.x, b.y));
            if (page)   // página actual: una marca verde al costado, como un LED de estado
            {
                const auto p = A.toView (b.x - b.d / 2 - 1.6f, b.y);
                g.setColour (col::green.withAlpha (0.9f)); g.fillEllipse (p.x - 3.0f, p.y - 3.0f, 6.0f, 6.0f);
            }
        }
        // LED del modo de las perillas.
        if (! leds.empty())
        {
            const auto& l = leds[(size_t) (dataMode ? 1 : 0)];
            A.draw (g, A.ledOn, A.toView (l.x, l.y));
        }
        paintLcd (g);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = toMm (e.position);
        for (auto& k : knobs)
            if (p.getDistanceFrom ({ k.x, k.y }) < (k.big ? 8.5f : 5.5f))
            {
                dragKnob = &k; dragStart = 0.0f; lastSteps = 0;
                if (auto* prm = proc.apvts.getParameter (k.param)) { dragStart = prm->getValue(); if (! (dataMode && k.big)) prm->beginChangeGesture(); }
                return;
            }
        for (auto& b : buttons)
            if (p.getDistanceFrom ({ b.x, b.y }) < std::max (3.2f, b.d / 2 + 0.8f))
            {
                pressed = b.id; repaint();
                return;
            }
        if (power.contains (p)) { powerOn = ! powerOn; if (onButton) onButton (powerOn ? "power_on" : "power_off"); repaint(); }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragKnob == nullptr) return;
        const float dy = -(float) e.getDistanceFromDragStartY();
        if (dataMode && dragKnob->big)
        {
            const int idx = dragKnob->id.getLastCharacter() - '1';
            const int steps = (int) (dy / 10.0f);
            if (steps != lastSteps) { if (onDataKnob) onDataKnob (idx, steps - lastSteps); dataAngle[(size_t) idx] += steps - lastSteps; lastSteps = steps; repaint(); }
            return;
        }
        if (auto* prm = proc.apvts.getParameter (dragKnob->param))
        {
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, dragStart + dy / (e.mods.isShiftDown() ? 900.0f : 220.0f)));
            flash (prm->getName (20), prm->getCurrentValueAsText());
            repaint();
        }
    }
    void mouseUp (const juce::MouseEvent&) override
    {
        if (dragKnob != nullptr && ! (dataMode && dragKnob->big))
            if (auto* prm = proc.apvts.getParameter (dragKnob->param)) prm->endChangeGesture();
        dragKnob = nullptr;
        if (pressed.isNotEmpty())
        {
            const auto id = pressed; pressed.clear(); repaint();
            if (id == "knob_mode") { dataMode = ! dataMode; flash ("Knobs", dataMode ? "DATA" : "PERFORM"); }
            if (onButton) onButton (id);
        }
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto p = toMm (e.position);
        for (auto& k : knobs)
            if (p.getDistanceFrom ({ k.x, k.y }) < (k.big ? 8.5f : 5.5f) && ! (dataMode && k.big))
                if (auto* prm = proc.apvts.getParameter (k.param)) { prm->setValueNotifyingHost (prm->getDefaultValue()); flash (prm->getName (20), prm->getCurrentValueAsText()); }
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto p = toMm (e.position);
        juce::String h;
        for (auto& k : knobs) if (p.getDistanceFrom ({ k.x, k.y }) < (k.big ? 8.5f : 5.5f)) h = dataMode && k.big ? juce::String ("Data knob: ") + (k.id == "k1" ? "operator" : k.id == "k2" ? "page" : k.id == "k3" ? "cursor" : "value") : "Knob: " + k.param + " (drag; double-click resets)";
        for (auto& b : buttons) if (p.getDistanceFrom ({ b.x, b.y }) < std::max (3.2f, b.d / 2 + 0.8f)) h = buttonHelp (b.id);
        if (power.contains (p)) h = "Power: mutes the output and resets all voices";
        if (h != lastHelp) { lastHelp = h; showHelp (h); }
        setMouseCursor (h.isNotEmpty() ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit (const juce::MouseEvent&) override { lastHelp.clear(); showHelp ({}); }
    juce::String litPage;

private:
    struct Knob { juce::String id, param; float x, y; bool big; };
    struct Button { juce::String id; float x, y, d; juce::String kind; };
    struct Led { float x, y; };
    ATFS1Processor& proc;
    std::vector<Knob> knobs;
    std::vector<Button> buttons;
    std::vector<Led> leds;
    std::vector<std::pair<juce::String, float>> lcdMarks;
    juce::Rectangle<float> glass, power;
    Knob* dragKnob = nullptr;
    float dragStart = 0.0f;
    int lastSteps = 0;
    std::array<int, 4> dataAngle {};
    juce::String pressed, flashText, lastHelp, shown1, shown2;
    juce::uint32 flashUntil = 0;
    bool midiLit = false;

    float scale() const { return (float) getWidth() / Assets::get().viewWidth(); }
    juce::Point<float> toMm (juce::Point<float> v) const
    {
        const auto& A = Assets::get();
        const float s = scale() * A.pxmm * 0.5f;
        return { v.x / s + A.x0, v.y / s / A.cosT + A.y0 };
    }
    juce::Rectangle<float> lcdBounds() const
    {
        const auto& A = Assets::get();
        const auto a = A.toView (glass.getX(), glass.getY(), -1.4f), b = A.toView (glass.getRight(), glass.getBottom(), -1.4f);
        return juce::Rectangle<float> (a, b).transformedBy (juce::AffineTransform::scale (scale()));
    }
    static juce::String buttonHelp (const juce::String& id)
    {
        if (id == "play") return "PLAY: presets and the live view";
        if (id == "perform") return "EDIT PERFORM: part offsets, voice mode, controllers";
        if (id == "effect") return "EDIT EFFECT: insertion, variation, reverb and EQ";
        if (id == "voice") return "EDIT VOICE: algorithm and operators (press again for common, then operator detail)";
        if (id == "util") return "UTIL: import and export FS1R sysex, polyphony, output";
        if (id == "search") return "SEARCH: the Formant Sequence page";
        if (id == "mute") return "MUTE/SOLO: all operators, voiced only, unvoiced only";
        if (id == "part_dn" || id == "part_up") return "PART -/+: previous/next preset (operator on operator pages)";
        if (id == "cur_dn" || id == "cur_up") return "CURSOR -/+: move the cursor over the fields of the page";
        if (id == "val_dn" || id == "val_up") return "VALUE -/+: change the field under the cursor";
        if (id == "enter") return "ENTER: open the field under the cursor";
        if (id == "exit") return "EXIT: back to PLAY";
        if (id == "knob_mode") return "Knob mode: perform (attack, release, formant, FM) or data entry (operator, page, cursor, value)";
        return {};
    }
    void paintLcd (juce::Graphics& g)
    {
        const auto& A = Assets::get();
        const auto a = A.toView (glass.getX(), glass.getY(), -1.4f), b = A.toView (glass.getRight(), glass.getBottom(), -1.4f);
        const juce::Rectangle<float> r (a, b);
        if (! powerOn) { g.setColour (juce::Colour (0xff1a2208).withAlpha (0.85f)); g.fillRect (r); return; }
        const float dot = (r.getWidth() - 10.0f) / (24.0f * 6.0f);
        const float x = r.getX() + 6.0f, y1 = r.getY() + r.getHeight() * 0.16f, y2 = y1 + dot * 10.0f;
        const juce::String l1 = (line1 ? line1() : juce::String()).paddedRight (' ', 24);
        const juce::String l2 = juce::Time::getMillisecondCounter() < flashUntil ? flashText : (line2 ? line2() : juce::String()).paddedRight (' ', 24);
        dotText (g, l1, x, y1, dot, col::ink, true, 24);
        dotText (g, l2, x, y2, dot, col::ink, true, 24);
        // Indicadores sobre los rótulos impresos debajo del cristal.
        const float ty = r.getBottom() - dot * 3.2f;
        for (auto& [label, mx] : lcdMarks)
        {
            bool on = false;
            if (label == "PART") on = true;
            else if (label == "MIDI") on = midiLit;
            else if (label == "FLT") on = proc.value ("filter") > 0.5f;
            else if (label == "REV") on = proc.value ("rev_type") > 0.5f;
            else if (label == "VAR") on = proc.value ("var_type") > 0.5f;
            else if (label == "INS") on = proc.value ("ins_type") > 0.5f;
            else if (label == "KEY") on = proc.value ("mono") > 0.5f;
            else if (label == "VOL") on = proc.value ("volume") > 0.01f;
            else if (label == "PAN") on = std::abs (proc.value ("pan")) > 0.01f;
            else if (label == "BANK/PGM#") on = true;
            const float px = A.toView (mx, 0).x;
            juce::Path t; t.addTriangle (px - dot * 2.0f, ty, px + dot * 2.0f, ty, px, ty + dot * 2.2f);
            g.setColour (on ? col::ink : col::ink.withAlpha (0.07f)); g.fillPath (t);
        }
    }
    void timerCallback() override
    {
        const bool m = proc.midiInFlash.exchange (false);
        bool need = m != midiLit;
        midiLit = m;
        const auto l1 = line1 ? line1() : juce::String(), l2 = line2 ? line2() : juce::String();
        if (l1 != shown1 || l2 != shown2) { shown1 = l1; shown2 = l2; need = true; }
        if (flashUntil != 0 && juce::Time::getMillisecondCounter() > flashUntil) { flashUntil = 0; need = true; }
        if (need) repaint (lcdBounds().toNearestInt().expanded (6));
        // Perillas movidas por el host o el editor: se repinta sólo la que cambió.
        for (size_t i = 0; i < knobs.size(); ++i)
        {
            float v = 0.0f;
            if (auto* prm = proc.apvts.getParameter (knobs[i].param)) v = prm->getValue();
            if (i >= lastKnob.size()) lastKnob.resize (knobs.size(), -1.0f);
            if (std::abs (v - lastKnob[i]) > 1e-4f)
            {
                lastKnob[i] = v;
                const auto& A = Assets::get();
                const auto c = A.toView (knobs[i].x, knobs[i].y);
                const float r = A.mm (knobs[i].big ? 14.0f : 8.0f), s = scale();
                repaint (juce::Rectangle<float> (c.x - r, c.y - r, 2 * r, 2 * r).transformedBy (juce::AffineTransform::scale (s)).toNearestInt().expanded (2));
            }
        }
    }
    std::vector<float> lastKnob;
};

} // namespace dh::fs::ui
