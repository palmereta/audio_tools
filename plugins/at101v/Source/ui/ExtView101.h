#pragma once
// Página SETTINGS: todo lo que el SH-101 no trae (voces, expresión, reloj del host, carácter,
// chorus, delay, reverb y sistema), con faders y selectores de LED como los del panel.

#include <JuceHeader.h>
#include "../../../../shared/ui/Showing.h"
#include "../PluginProcessor.h"
#include "Widgets101.h"

namespace dh {

class ExtView101 : public juce::Component, private juce::Timer
{
public:
    explicit ExtView101 (AT101VProcessor& p) : proc (p)
    {
        setSize ((int) Assets101::get().viewWidth(), (int) Assets101::get().viewHeight());
        build();
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        const int body = bodyColour();
        w101::backdrop (g, getLocalBounds().toFloat(), body);
        for (auto& s : sections) w101::frame (g, s.r, s.title, body);
        for (auto& c : items)
        {
            const auto* e = findExt (c.id);
            if (e == nullptr) continue;
            const float v = value (c.id);
            const auto labelR = juce::Rectangle<float> (c.r.getX() - 16.0f, c.r.getBottom() + 4.0f, c.r.getWidth() + 32.0f, 14.0f);
            switch (c.kind)
            {
                case Kind::Fader:
                {
                    const float norm = (v - e->lo) / (e->hi - e->lo);
                    w101::fader (g, c.r, norm, c.green);
                    w101::silk (g, c.label, labelR, 10.0f);
                    w101::silk (g, formatExt (*e, v), labelR.translated (0, 14.0f), 9.5f, juce::Justification::centred, col101::silkDim);
                    break;
                }
                case Kind::List:
                {
                    const auto ch = extChoices (*e);
                    const int sel = (int) std::lround (v);
                    const float rowH = c.r.getHeight() / (float) ch.size();
                    for (int i = 0; i < ch.size(); ++i)
                    {
                        const auto row = c.r.withHeight (rowH).translated (0, rowH * (float) i);
                        w101::led (g, { row.getX() + 8.0f, row.getCentreY() }, i == sel);
                        w101::silk (g, ch[i].toUpperCase(), row.withTrimmedLeft (20.0f), 10.0f, juce::Justification::centredLeft,
                                    i == sel ? col101::silk : col101::silkDim);
                    }
                    w101::silk (g, c.label, labelR.withY (c.r.getY() - 22.0f), 10.0f);
                    break;
                }
                case Kind::Toggle:
                {
                    const bool on = v > 0.5f;
                    const auto br = juce::Rectangle<float> (40.0f, 18.0f).withCentre (c.r.getCentre());
                    w101::creamButton (g, br, pressed == &c);
                    w101::led (g, { br.getCentreX(), br.getY() - 10.0f }, on);
                    w101::silk (g, c.label, br.translated (0, 24.0f).expanded (30.0f, 0).withHeight (14.0f), 10.0f);
                    break;
                }
                case Kind::Stepper:
                {
                    const auto disp = c.r.withHeight (30.0f);
                    w101::display (g, disp, formatExt (*e, v));
                    const auto bl = juce::Rectangle<float> (28.0f, 18.0f).withPosition (disp.getX(), disp.getBottom() + 10.0f);
                    const auto br = bl.withX (disp.getRight() - 28.0f);
                    w101::creamButton (g, bl, pressed == &c && pressedSide < 0);
                    w101::creamButton (g, br, pressed == &c && pressedSide > 0);
                    w101::silk (g, "-", bl.translated (0, pressed == &c && pressedSide < 0 ? 1.2f : 0.0f), 13.0f, juce::Justification::centred, juce::Colour (0xff333333));
                    w101::silk (g, "+", br.translated (0, pressed == &c && pressedSide > 0 ? 1.2f : 0.0f), 13.0f, juce::Justification::centred, juce::Colour (0xff333333));
                    w101::silk (g, c.label, labelR.withY (disp.getY() - 20.0f), 10.0f);
                    break;
                }
            }
            if (const int cc = proc.ccForParam (c.id); cc >= 0)
                w101::silk (g, "CC" + juce::String (cc), labelR.translated (0, c.kind == Kind::Fader ? 28.0f : 14.0f), 8.5f, juce::Justification::centred, col101::orange);
        }

        w101::silk (g, "Right-click any control, here or on the panel, for MIDI Learn. The mod wheel pushes the bender (LFO MOD), "
                       "the sustain pedal is HOLD and pitch bend moves the lever.",
                    { 60.0f, 812.0f, (float) getWidth() - 120.0f, 16.0f }, 10.0f, juce::Justification::centred, col101::silkDim);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto* c = hit (e.position);
        if (c == nullptr) return;
        if (e.mods.isPopupMenu()) { showContextMenu (*c); return; }
        pressed = c;
        const auto* ex = findExt (c->id);
        switch (c->kind)
        {
            case Kind::Fader: dragStartY = e.position.y; dragStartV = value (c->id); break;
            case Kind::List:
            {
                const int n = ex->steps;
                const int i = juce::jlimit (0, n - 1, (int) ((e.position.y - c->r.getY()) / (c->r.getHeight() / (float) n)));
                proc.setExtValue (c->id, (float) i);
                break;
            }
            case Kind::Toggle: proc.setExtValue (c->id, value (c->id) > 0.5f ? 0.0f : 1.0f); break;
            case Kind::Stepper:
            {
                pressedSide = e.position.x < c->r.getCentreX() ? -1 : 1;
                proc.setExtValue (c->id, (float) juce::jlimit (0, ex->steps - 1, (int) std::lround (value (c->id)) + pressedSide));
                break;
            }
        }
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (pressed == nullptr || pressed->kind != Kind::Fader) return;
        const auto* ex = findExt (pressed->id);
        const float travel = pressed->r.getHeight() - 20.0f;
        const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
        const float norm = (dragStartV - ex->lo) / (ex->hi - ex->lo) + (dragStartY - e.position.y) / travel * fine;
        proc.setExtValue (pressed->id, ex->lo + (ex->hi - ex->lo) * juce::jlimit (0.0f, 1.0f, norm));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { pressed = nullptr; pressedSide = 0; repaint(); }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (auto* c = hit (e.position); c != nullptr && c->kind == Kind::Fader) proc.setExtValue (c->id, findExt (c->id)->def);
    }

private:
    enum class Kind { Fader, List, Toggle, Stepper };
    struct Item { juce::String id, label; Kind kind; juce::Rectangle<float> r; bool green = false; };
    struct Section { juce::String title; juce::Rectangle<float> r; };

    AT101VProcessor& proc;
    std::vector<Item> items;
    std::vector<Section> sections;
    Item* pressed = nullptr;
    int pressedSide = 0;
    float dragStartY = 0, dragStartV = 0;
    uint64_t shown = 0;

    int bodyColour() const { return juce::jlimit (0, 2, (int) proc.apvts.getRawParameterValue ("body")->load()); }
    float value (const juce::String& id) const { return proc.apvts.getRawParameterValue (id)->load(); }

    void build()
    {
        // Dos filas de secciones; cada fader ocupa una columna de 66 px y cada lista 96.
        const float top1 = 34.0f, top2 = 436.0f, h = 372.0f, col = 66.0f, listW = 96.0f;
        float x = 34.0f;
        auto section = [&] (const char* title, float top, std::initializer_list<std::tuple<const char*, const char*, Kind, bool>> list) {
            float width = 24.0f;
            for (auto& it : list) width += std::get<2> (it) == Kind::List ? listW : (std::get<2> (it) == Kind::Stepper ? 116.0f : col);
            sections.push_back ({ title, { x, top, width, h } });
            float cx = x + 12.0f;
            for (auto& [id, label, kind, green] : list)
            {
                const float w = kind == Kind::List ? listW : (kind == Kind::Stepper ? 116.0f : col);
                juce::Rectangle<float> r;
                if (kind == Kind::Fader) r = { cx + w * 0.5f - 22.0f, top + 70.0f, 44.0f, 220.0f };
                else if (kind == Kind::List) r = { cx + 8.0f, top + 78.0f, w - 12.0f, std::min (210.0f, 30.0f * (float) findExt (id)->steps) };
                else if (kind == Kind::Toggle) r = { cx + w * 0.5f - 26.0f, top + 90.0f, 52.0f, 40.0f };
                else r = { cx + 8.0f, top + 92.0f, w - 16.0f, 70.0f };
                items.push_back ({ id, label, kind, r, green });
                cx += w;
            }
            x += width + 18.0f;
        };
        section ("VOICES", top1, { { "voices", "VOICES", Kind::List, false }, { "unison", "UNISON", Kind::List, false },
                                   { "detune", "DETUNE", Kind::Fader, false }, { "spread", "SPREAD", Kind::Fader, true },
                                   { "analog", "DRIFT", Kind::Fader, true } });
        section ("EXPRESSION", top1, { { "vel_vcf", "VEL VCF", Kind::Fader, false }, { "vel_vca", "VEL VCA", Kind::Fader, false },
                                       { "accent", "ACCENT", Kind::Fader, true }, { "slide_time", "SLIDE", Kind::Fader, true } });
        section ("CLOCK", top1, { { "clock_src", "SOURCE", Kind::List, false }, { "clock_div", "DIVISION", Kind::List, false },
                                  { "swing", "SWING", Kind::Fader, false }, { "gate_len", "GATE", Kind::Fader, false },
                                  { "arp_oct", "ARP OCT", Kind::List, false }, { "seq_keytr", "KEY TRANSP", Kind::Toggle, false } });
        sections.back().r.setRight ((float) getWidth() - 34.0f);
        x = 34.0f;
        section ("CHARACTER", top2, { { "drive", "DRIVE", Kind::Fader, false }, { "quality", "OVERSAMPLING", Kind::List, false } });
        section ("CHORUS", top2, { { "chorus", "MODE", Kind::List, false }, { "chorus_mix", "MIX", Kind::Fader, true } });
        section ("DELAY", top2, { { "dly_on", "ON", Kind::Toggle, false }, { "dly_time", "TIME", Kind::Stepper, false },
                                  { "dly_fb", "FEEDBACK", Kind::Fader, false }, { "dly_tone", "TONE", Kind::Fader, false }, { "dly_mix", "MIX", Kind::Fader, true } });
        section ("REVERB", top2, { { "rev_on", "ON", Kind::Toggle, false }, { "rev_size", "SIZE", Kind::Fader, false },
                                   { "rev_damp", "DAMP", Kind::Fader, false }, { "rev_mix", "MIX", Kind::Fader, true } });
        section ("SYSTEM", top2, { { "midi_out", "MIDI OUT", Kind::List, false }, { "output", "OUTPUT", Kind::Fader, true },
                                   { "body", "BODY", Kind::List, false } });
        // La última sección se estira hasta el borde.
        auto& last = sections.back().r;
        last.setRight ((float) getWidth() - 34.0f);
    }

    Item* hit (juce::Point<float> p)
    {
        for (auto& c : items)
        {
            auto r = c.r;
            if (c.kind == Kind::Stepper) r = r.withHeight (70.0f);
            if (r.expanded (6.0f).contains (p)) return &c;
        }
        return nullptr;
    }

    void showContextMenu (Item& c)
    {
        juce::PopupMenu m;
        m.addSectionHeader (findExt (c.id)->name);
        const int cc = proc.ccForParam (c.id);
        m.addItem (1, proc.isLearning() ? "Waiting for a MIDI CC..." : "MIDI Learn", ! proc.isLearning());
        m.addItem (2, cc >= 0 ? "Clear MIDI CC " + juce::String (cc) : "No MIDI CC assigned", cc >= 0);
        m.addItem (3, "Reset to default");
        const auto id = c.id;
        m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, id] (int r) {
            if (r == 1) proc.startMidiLearn (id);
            else if (r == 2) proc.clearMidiMapping (id);
            else if (r == 3) proc.setExtValue (id, findExt (id)->def);
        });
    }

    void timerCallback() override
    {
        if (! dh::reallyShowing (*this)) return;      // ventana escondida (Live la esconde sin cerrarla): sin trabajo (H3)
        uint64_t h = 1469598103934665603ull;
        for (auto& c : items) h = (h ^ (uint64_t) (value (c.id) * 1000.0f + 5000.0f) ^ (uint64_t) (proc.ccForParam (c.id) + 2)) * 1099511628211ull;
        if (h != shown) { shown = h; repaint(); }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExtView101)
};

} // namespace dh
