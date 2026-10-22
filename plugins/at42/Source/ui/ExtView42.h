#pragma once
// Página SETTINGS del AT42: lo que el hardware no trae (o trae escondido en el firmware del teclado),
// dibujado como el panel: cajas con pestaña negra, perillas renderizadas y selectores con LED.

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Assets42.h"
#include "../../../../shared/ui/Showing.h"

namespace dh {

class ExtView42 : public juce::Component, private juce::Timer
{
public:
    explicit ExtView42 (AT42Processor& p) : proc (p)
    {
        setSize ((int) Assets42::get().viewWidth(), (int) Assets42::get().viewHeight());
        build();
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        auto& A = Assets42::get();
        const int pc = juce::jlimit (0, 3, (int) proc.extValue ("panel"));
        const auto panel = col42::panel (pc), ink = col42::ink (pc);
        g.fillAll (col42::editorBg);
        if (A.backgrounds[(size_t) pc].isValid()) g.drawImage (A.backgrounds[(size_t) pc], getLocalBounds().toFloat());
        g.setColour (panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (12.0f, 10.0f), 4.0f);

        for (auto& s : sections)
        {
            g.setColour (ink); g.drawRoundedRectangle (s.r, 4.0f, 1.2f);
            auto tab = juce::Rectangle<float> (s.r.getX(), s.r.getY(), std::max (90.0f, (float) s.title.length() * 8.2f + 24.0f), 18.0f);
            g.fillRoundedRectangle (tab, 3.0f);
            g.setColour (panel); g.setFont (col42::font (12.5f));
            g.drawText (s.title, tab, juce::Justification::centred);
        }
        for (auto& c : items)
        {
            const auto* e = findExt42 (c.id);
            const float v = proc.extValue (c.id);
            g.setColour (ink);
            if (c.list)
            {
                const auto ch = extChoices42 (*e);
                const int sel = (int) std::lround (v);
                const int n = ch.size(), cols = n > 8 ? 2 : 1, rows = (n + cols - 1) / cols;
                const float rh = std::min (17.0f, c.r.getHeight() / (float) rows), cw = c.r.getWidth() / (float) cols;
                for (int i = 0; i < n; ++i)
                {
                    const auto row = juce::Rectangle<float> (c.r.getX() + cw * (float) (i / rows), c.r.getY() + rh * (float) (i % rows), cw, rh);
                    ledAt (g, { row.getX() + 6.0f, row.getCentreY() }, i == sel);
                    g.setColour (i == sel ? ink : ink.withAlpha (0.55f)); g.setFont (col42::font (10.5f, i == sel));
                    g.drawText (ch[i], row.withTrimmedLeft (15.0f), juce::Justification::centredLeft);
                }
                g.setColour (ink); g.setFont (col42::font (11.0f));
                g.drawText (c.label, c.r.getX(), c.r.getY() - 18.0f, c.r.getWidth(), 14.0f, juce::Justification::centredLeft);
            }
            else
            {
                auto it = A.knobs.find (c.sprite);
                const float norm = (v - e->lo) / (e->hi - e->lo);
                if (it != A.knobs.end())
                {
                    const float ang = -150.0f + 300.0f * norm;
                    const int f = ((int) std::lround (ang / 360.0f * 128.0f) % 128 + 128) % 128;
                    const auto ctr = c.r.getCentre();
                    // Arco de escala.
                    juce::Path arc; arc.addCentredArc (ctr.x, ctr.y - 2.0f, 25.0f, 25.0f, 0.0f, juce::degreesToRadians (-150.0f), juce::degreesToRadians (150.0f), true);
                    g.strokePath (arc, juce::PathStrokeType (1.1f));
                    A.draw (g, it->second, { ctr.x, ctr.y + 3.0f }, f, 1.25f);
                }
                g.setFont (col42::font (11.0f));
                g.drawText (c.label, c.r.getX() - 10.0f, c.r.getBottom() + 2.0f, c.r.getWidth() + 20.0f, 14.0f, juce::Justification::centred);
                g.setColour (ink.withAlpha (0.6f)); g.setFont (col42::font (10.0f, false));
                g.drawText (formatExt42 (*e, v), c.r.getX() - 10.0f, c.r.getBottom() + 15.0f, c.r.getWidth() + 20.0f, 13.0f, juce::Justification::centred);
            }
            if (const int cc = proc.ccForParam (c.id); cc >= 0)
            {
                g.setColour (juce::Colour (0xffd8352b)); g.setFont (col42::font (9.0f));
                g.drawText ("CC" + juce::String (cc), c.r.getRight() - 30.0f, c.r.getY() - 16.0f, 30.0f, 12.0f, juce::Justification::centredRight);
            }
        }
        g.setColour (ink.withAlpha (0.65f)); g.setFont (col42::font (11.0f, false));
        g.drawText ("Right-click a control for MIDI Learn. MIDI notes play the touch keyboard; C1 to F1 play the drone keys; the mod wheel moves the joystick Y.",
                    getLocalBounds().toFloat().removeFromBottom (34.0f).reduced (30.0f, 8.0f), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto& c : items)
        {
            if (! c.r.expanded (6.0f, 18.0f).contains (e.position)) continue;
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu m; const auto id = c.id; const int cc = proc.ccForParam (id);
                m.addItem (1, "MIDI Learn"); m.addItem (2, cc >= 0 ? "Clear MIDI CC " + juce::String (cc) : "No MIDI CC assigned", cc >= 0);
                m.addItem (3, "Reset to default");
                m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, id] (int r) {
                    if (r == 1) proc.startMidiLearn (id); else if (r == 2) proc.clearMidiMapping (id); else if (r == 3) proc.setExtValue (id, findExt42 (id)->def);
                });
                return;
            }
            const auto* ex = findExt42 (c.id);
            if (c.list)
            {
                const int n = ex->steps, cols = n > 8 ? 2 : 1, rows = (n + cols - 1) / cols;
                const float rh = std::min (17.0f, c.r.getHeight() / (float) rows), cw = c.r.getWidth() / (float) cols;
                const int col = juce::jlimit (0, cols - 1, (int) ((e.position.x - c.r.getX()) / cw));
                const int row = juce::jlimit (0, rows - 1, (int) ((e.position.y - c.r.getY()) / rh));
                const int i = col * rows + row;
                if (i < n) proc.setExtValue (c.id, (float) i);
            }
            else { dragItem = &c; dragStartY = e.position.y; dragStartV = proc.extValue (c.id); }
            repaint();
            return;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragItem == nullptr) return;
        const auto* ex = findExt42 (dragItem->id);
        const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
        const float norm = (dragStartV - ex->lo) / (ex->hi - ex->lo) + (dragStartY - e.position.y) / 200.0f * fine;
        proc.setExtValue (dragItem->id, ex->lo + (ex->hi - ex->lo) * juce::jlimit (0.0f, 1.0f, norm));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { dragItem = nullptr; }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        for (auto& c : items) if (! c.list && c.r.contains (e.position)) proc.setExtValue (c.id, findExt42 (c.id)->def);
    }

private:
    struct Item { juce::String id, label, sprite; bool list; juce::Rectangle<float> r; };
    struct Section { juce::String title; juce::Rectangle<float> r; };
    AT42Processor& proc;
    std::vector<Item> items;
    std::vector<Section> sections;
    Item* dragItem = nullptr;
    float dragStartY = 0, dragStartV = 0;
    uint64_t shown = 0;

    static void ledAt (juce::Graphics& g, juce::Point<float> c, bool on)
    {
        if (on) { g.setColour (juce::Colour (0x55ff3020)); g.fillEllipse (juce::Rectangle<float> (12.0f, 12.0f).withCentre (c)); }
        g.setColour (on ? juce::Colour (0xffff3a24) : juce::Colour (0xff4a2020));
        g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (c));
    }

    void build()
    {
        // Columnas: cada perilla 74 px, cada lista 120 px (o 220 si es de dos columnas).
        float x = 30.0f;
        auto section = [&] (const char* title, float top, float h, std::initializer_list<std::tuple<const char*, const char*, int, const char*>> list) {
            float w = 20.0f;
            for (auto& [id, label, kind, sprite] : list) w += kind == 0 ? 74.0f : (kind == 2 ? 236.0f : 124.0f);
            sections.push_back ({ title, { x, top, w, h } });
            float cx = x + 12.0f;
            for (auto& [id, label, kind, sprite] : list)
            {
                if (kind == 0) items.push_back ({ id, label, sprite, false, { cx + 13.0f, top + 62.0f, 48.0f, 48.0f } });
                else items.push_back ({ id, label, "", true, { cx + 4.0f, top + 48.0f, kind == 2 ? 226.0f : 116.0f, h - 64.0f } });
                cx += kind == 0 ? 74.0f : (kind == 2 ? 236.0f : 124.0f);
            }
            x += w + 14.0f;
        };
        const float r1 = 34.0f, r2 = 290.0f, r3 = 600.0f, h1 = 230.0f, h2 = 280.0f;
        section ("CHARACTER", r1, h1, { { "ambient", "AMBIENT", 0, "KM_teal" }, { "drift", "DRIFT", 0, "KM_teal" }, { "mismatch", "L/R", 0, "KM_orange" },
                                       { "bus_drive", "DRIVE", 0, "KM_orange" }, { "vintage", "NOISE", 0, "KM_orange" },
                                       { "chromatic", "TUNING", 1, "" }, { "long_rel", "RELEASE", 1, "" } });
        section ("SYNC", r1, h1, { { "sync_lfoa", "LFO A", 1, "" }, { "lfoa_beats", "CYCLE", 1, "" }, { "sync_lfob", "LFO B", 1, "" },
                                  { "lfob_beats", "CYCLE", 1, "" }, { "sync_pulser", "PULSER", 1, "" }, { "pulser_div", "DIV", 1, "" } });
        x = 30.0f;
        section ("TOUCH KEYBOARD", r2, h2, { { "kb_mode", "MODE", 1, "" }, { "kb_scale", "SCALE", 2, "" }, { "kb_root", "ROOT", 2, "" },
                                            { "kb_dir", "ARP DIRECTION", 1, "" }, { "kb_div", "CLOCK", 1, "" }, { "kb_clock", "SOURCE", 1, "" },
                                            { "kb_bpm", "TEMPO", 0, "KL_red" } });
        x = 30.0f;
        section ("EXPRESSION", r3, h1 + 50.0f, { { "kb_porta", "PORTA", 0, "KM_green" }, { "kb_vib", "VIBRATO", 0, "KM_green" }, { "kb_vibspd", "SPEED", 0, "KM_green" },
                                        { "kb_rise", "RISE", 0, "KM_green" }, { "kb_fall", "FALL", 0, "KM_green" },
                                        { "kb_press", "PRESSURE", 1, "" }, { "kb_hold", "ARP HOLD", 1, "" }, { "kb_legato", "LEGATO", 1, "" } });
        section ("MIDI AND SYSTEM", r3, h1 + 50.0f, { { "midi_drone", "DRONE KEYS", 1, "" }, { "bend_range", "BEND", 2, "" }, { "quality", "OVERSAMPLING", 1, "" },
                                             { "output", "OUTPUT", 0, "KM_orange" }, { "panel", "PANEL", 1, "" } });
    }

    void timerCallback() override
    {
        if (! dh::reallyShowing (*this)) return;      // ventana escondida (Live la esconde sin cerrarla): sin trabajo (H3)
        uint64_t h = 1469598103934665603ull;
        for (auto& c : items) h = (h ^ (uint64_t) (proc.extValue (c.id) * 1000.0f + 50000.0f) ^ (uint64_t) (proc.ccForParam (c.id) + 2)) * 1099511628211ull;
        if (h != shown) { shown = h; repaint(); }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExtView42)
};

} // namespace dh
