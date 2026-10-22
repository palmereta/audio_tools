#pragma once
// Página SETTINGS del AT700: lo que el System-700 no trae, dibujado como sus paneles: aluminio negro,
// serigrafía blanca, perillas renderizadas y selectores con LED.

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Assets700.h"
#include "../../../../shared/ui/Showing.h"

namespace dh {

class ExtView700 : public juce::Component, private juce::Timer
{
public:
    explicit ExtView700 (AT700Processor& p) : proc (p) { build(); startTimerHz (20); }

    void resized() override { layoutItems(); }

    void paint (juce::Graphics& g) override
    {
        auto& A = Assets700::get();
        const auto ink = juce::Colour (0xffe9e7e0), panel = juce::Colour (0xff141416);
        g.fillAll (col700::editorBg);
        g.setColour (panel);
        g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (12.0f, 10.0f), 4.0f);
        g.setColour (ink); g.setFont (col700::font (22.0f));
        g.drawText ("SETTINGS", juce::Rectangle<float> (40.0f, 16.0f, 400.0f, 24.0f), juce::Justification::centredLeft);
        g.setColour (ink.withAlpha (0.5f)); g.setFont (col700::font (12.0f, false));
        g.drawText ("What the System-700 does not have. Saved with each preset, except oversampling and output.", juce::Rectangle<float> (180.0f, 16.0f, 900.0f, 24.0f), juce::Justification::centredLeft);
        for (auto& s : sections)
        {
            g.setColour (ink); g.drawRoundedRectangle (s.r, 4.0f, 1.0f);
            g.setFont (col700::font (15.0f)); g.drawText (s.title, s.r.withHeight (30.0f).reduced (12.0f, 0), juce::Justification::centredLeft);
        }
        for (auto& c : items)
        {
            const auto* e = findExt700 (c.id);
            const float v = proc.extValue (c.id);
            g.setColour (ink);
            if (c.list)
            {
                const auto ch = extChoices700 (*e);
                const int sel = (int) std::lround (v);
                const float rh = std::min (20.0f, c.r.getHeight() / (float) ch.size());
                for (int i = 0; i < ch.size(); ++i)
                {
                    const auto row = juce::Rectangle<float> (c.r.getX(), c.r.getY() + rh * (float) i, c.r.getWidth(), rh);
                    ledAt (g, { row.getX() + 6.0f, row.getCentreY() }, i == sel);
                    g.setColour (i == sel ? ink : ink.withAlpha (0.5f)); g.setFont (col700::font (12.0f, i == sel));
                    g.drawText (ch[i], row.withTrimmedLeft (16.0f), juce::Justification::centredLeft);
                }
                g.setColour (ink); g.setFont (col700::font (11.5f));
                g.drawText (c.label, juce::Rectangle<float> (c.r.getX(), c.r.getY() - 20.0f, c.r.getWidth(), 14.0f), juce::Justification::centredLeft);
            }
            else
            {
                auto it = A.knobs.find ("KL");
                const float norm = (v - e->lo) / (e->hi - e->lo);
                const auto ctr = c.r.getCentre();
                juce::Path arc; arc.addCentredArc (ctr.x, ctr.y, 30.0f, 30.0f, 0.0f, juce::degreesToRadians (-150.0f), juce::degreesToRadians (150.0f), true);
                g.strokePath (arc, juce::PathStrokeType (1.1f));
                if (it != A.knobs.end())
                {
                    const float ang = -150.0f + 300.0f * norm;
                    const int f = ((int) std::lround (ang / 360.0f * 128.0f) % 128 + 128) % 128;
                    A.draw (g, it->second, ctr, f, 1.1f);
                }
                g.setFont (col700::font (11.5f));
                g.drawText (c.label, juce::Rectangle<float> (c.r.getX() - 20.0f, c.r.getBottom() + 6.0f, c.r.getWidth() + 40.0f, 14.0f), juce::Justification::centred);
                g.setColour (ink.withAlpha (0.6f)); g.setFont (col700::font (10.5f, false));
                g.drawText (formatExt700 (*e, v), juce::Rectangle<float> (c.r.getX() - 20.0f, c.r.getBottom() + 20.0f, c.r.getWidth() + 40.0f, 13.0f), juce::Justification::centred);
            }
            {
                g.setColour (ink.withAlpha (0.5f)); g.setFont (col700::font (11.0f, false));
                const auto hr = c.list ? juce::Rectangle<float> (c.r.getX(), c.r.getBottom() + 6.0f, c.r.getWidth(), 56.0f)
                                       : juce::Rectangle<float> (c.r.getCentreX() - 140.0f, c.r.getBottom() + 38.0f, 280.0f, 56.0f);
                g.drawFittedText (c.help, hr.toNearestInt(), c.list ? juce::Justification::topLeft : juce::Justification::centredTop, 4, 1.0f);
            }
            if (const int cc = proc.ccForParam (c.id); cc >= 0)
            {
                g.setColour (juce::Colour (0xffd8352b)); g.setFont (col700::font (9.0f));
                g.drawText ("CC" + juce::String (cc), juce::Rectangle<float> (c.r.getRight() - 30.0f, c.r.getY() - 18.0f, 30.0f, 12.0f), juce::Justification::centredRight);
            }
        }
        g.setColour (ink.withAlpha (0.55f)); g.setFont (col700::font (12.0f, false));
        g.drawText ("Right-click a control for MIDI Learn. MIDI notes play the 701A keyboard (C2 = the lowest key, 0 V). "
                    "Velocity, mod wheel and aftertouch come out of the jacks on the keyboard panel.",
                    getLocalBounds().toFloat().removeFromBottom (40.0f).reduced (30.0f, 8.0f), juce::Justification::centred);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        for (auto& c : items)
        {
            // Las listas sólo responden sobre sus renglones: con el margen de 20 px, un clic en el título o en la
            // ayuda de abajo elegía la primera o la última opción.
            if (! c.r.expanded (8.0f, c.list ? 0.0f : 20.0f).contains (e.position)) continue;
            if (e.mods.isPopupMenu())
            {
                juce::PopupMenu m; const auto id = c.id; const int cc = proc.ccForParam (id);
                m.addItem (1, "MIDI Learn"); m.addItem (2, cc >= 0 ? "Clear MIDI CC " + juce::String (cc) : "No MIDI CC assigned", cc >= 0);
                m.addItem (3, "Reset to default");
                m.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this, id] (int r) {
                    if (r == 1) proc.startMidiLearn (id); else if (r == 2) proc.clearMidiMapping (id); else if (r == 3) proc.setExtValue (id, findExt700 (id)->def);
                });
                return;
            }
            const auto* ex = findExt700 (c.id);
            if (c.list)
            {
                const float rh = std::min (20.0f, c.r.getHeight() / (float) ex->steps);
                const int i = juce::jlimit (0, ex->steps - 1, (int) ((e.position.y - c.r.getY()) / rh));
                proc.setExtValue (c.id, (float) i);
            }
            else { dragItem = &c; dragStartY = e.position.y; dragStartV = proc.extValue (c.id); }
            repaint();
            return;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragItem == nullptr) return;
        const auto* ex = findExt700 (dragItem->id);
        const float fine = e.mods.isShiftDown() ? 0.15f : 1.0f;
        const float norm = (dragStartV - ex->lo) / (ex->hi - ex->lo) + (dragStartY - e.position.y) / 200.0f * fine;
        proc.setExtValue (dragItem->id, ex->lo + (ex->hi - ex->lo) * juce::jlimit (0.0f, 1.0f, norm));
        repaint();
    }

    void mouseUp (const juce::MouseEvent&) override { dragItem = nullptr; }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        for (auto& c : items) if (! c.list && c.r.contains (e.position)) proc.setExtValue (c.id, findExt700 (c.id)->def);
    }

private:
    struct Item { juce::String id, label; bool list; juce::Rectangle<float> r; int section; juce::String help; };
    struct Section { juce::String title; juce::Rectangle<float> r; };
    AT700Processor& proc;
    std::vector<Item> items;
    std::vector<Section> sections;
    Item* dragItem = nullptr;
    float dragStartY = 0, dragStartV = 0;
    uint64_t shown = 0;

    static void ledAt (juce::Graphics& g, juce::Point<float> c, bool on)
    {
        if (on) { g.setColour (juce::Colour (0x55ff3020)); g.fillEllipse (juce::Rectangle<float> (13.0f, 13.0f).withCentre (c)); }
        g.setColour (on ? juce::Colour (0xffff3a24) : juce::Colour (0xff4a2020));
        g.fillEllipse (juce::Rectangle<float> (6.5f, 6.5f).withCentre (c));
    }

    void build()
    {
        sections = { { "VOICES", {} }, { "CHARACTER", {} }, { "SEQUENCER", {} }, { "MIDI AND SYSTEM", {} } };
        items = { { "voices", "KEYBOARD (701A)", true, {}, 0, "Duo is the original: KEY CV1 follows the lowest key, KEY CV2 the highest. Poly duplicates the voice modules of the console for every key." },
                  { "droop", "KEY CV DROOP", true, {}, 0, "The held key voltage slowly drifts down after release, as the 701A sample and hold leaks." },
                  { "vcf1_model", "VCF-1", true, {}, 1, "Known consoles carry the 703E 24 dB low-pass; the 1976 manual shows the 703A 12 dB multimode." },
                  { "vcf1_mode", "703A MODE", true, {}, 1, "Filter mode of VCF-1 when it is the 703A." },
                  { "drift", "VCO DRIFT", false, {}, 1, "Slow random pitch drift. The heated uA726 kept the original within a few cents." },
                  { "seq_clock", "CLOCK", true, {}, 2, "Host: one whole note of your DAW is DIVIDE 1, so 1/16 steps land on sixteenths." },
                  { "seq_follow", "TRANSPORT", true, {}, 2, "With the host clock, START and STOP follow the DAW transport." },
                  { "midi_bend", "PITCH BEND", true, {}, 3, "MIDI pitch bend moves the BENDER lever; its range is the 5V / 1V / 0.1V switch." },
                  { "quality", "OVERSAMPLING", true, {}, 3, "2x is the default; 4x for heavy sync and ring modulation." },
                  { "output", "OUTPUT", false, {}, 3, "Final level after the MONITOR knob." } };
    }

    void layoutItems()
    {
        const auto area = getLocalBounds().toFloat().reduced (40.0f, 40.0f).withTrimmedTop (24.0f).withTrimmedBottom (40.0f);
        const float colW = (area.getWidth() - 3 * 24.0f) / 4.0f;
        for (int s = 0; s < 4; ++s)
        {
            float y = area.getY() + 70.0f;
            for (auto& c : items)
            {
                if (c.section != s) continue;
                const auto* e = findExt700 (c.id);
                if (c.list) { const float h = 20.0f * (float) e->steps; c.r = { area.getX() + (float) s * (colW + 24.0f) + 24.0f, y, colW - 48.0f, h }; y += h + 92.0f; }
                else { c.r = juce::Rectangle<float> (64.0f, 64.0f).withCentre ({ area.getX() + (float) s * (colW + 24.0f) + colW * 0.5f, y + 36.0f }); y += 176.0f; }
            }
            sections[(size_t) s].r = { area.getX() + (float) s * (colW + 24.0f), area.getY(), colW, std::min (area.getHeight(), y - area.getY() - 20.0f) };
        }
    }

    void timerCallback() override
    {
        if (! dh::reallyShowing (*this)) return;      // ventana escondida (Live la esconde sin cerrarla): sin trabajo (H3)
        uint64_t h = 1469598103934665603ull;
        for (auto& c : items) h = (h ^ (uint64_t) (proc.extValue (c.id) * 1000.0f + 50000.0f) ^ (uint64_t) (proc.ccForParam (c.id) + 2)) * 1099511628211ull;
        if (h != shown) { shown = h; repaint(); }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExtView700)
};

} // namespace dh
