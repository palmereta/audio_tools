#pragma once
// DeviceNTI V: the editor's self-test (DV_UITEST=/path/report.txt in the standalone). Real mouse events go to the unit
// view at the projected positions of the controls, so it checks the hit areas against the render as well as the
// panel's behaviour: every control is found where it is drawn, knobs turn their targets (main layer, FUNC layer, a
// lock on a held step), the keys select, record, hold and change patterns, the display steps through fields.

#include <JuceHeader.h>
#include "UnitView.h"

namespace dh::dv::ui {

struct UiTest
{
    DeVicentiProcessor& proc; UnitView& unit; PanelState& st;
    juce::StringArray lines; int fails = 0;

    void check (bool ok, const juce::String& name, const juce::String& detail = {})
    {
        lines.add (juce::String (ok ? "PASS " : "FAIL ") + name + (detail.isNotEmpty() ? "  (" + detail + ")" : juce::String()));
        if (! ok) ++fails;
    }
    juce::Point<float> at (const juce::String& id, float u = 0, float v = 0) const
    {
        const auto* c = Model::get().find (id);
        return c != nullptr ? c->top.map (u, v) * unit.pxScale() : juce::Point<float>();
    }
    juce::MouseEvent ev (juce::Point<float> p, juce::ModifierKeys m = {}) const
    {
        const auto now = juce::Time::getCurrentTime();
        return juce::MouseEvent (juce::Desktop::getInstance().getMainMouseSource(), p, m, 1.0f, 0, 0, 0, 0, &unit, &unit, now, p, now, 1, false);
    }
    void click (const juce::String& id, juce::ModifierKeys m = {})
    {
        const auto p = at (id);
        unit.mouseDown (ev (p, m.withFlags (juce::ModifierKeys::leftButtonModifier))); unit.mouseUp (ev (p, m));
    }
    void drag (const juce::String& id, int dy, juce::ModifierKeys m = {})
    {
        auto p = at (id);
        unit.mouseDown (ev (p, m.withFlags (juce::ModifierKeys::leftButtonModifier)));
        for (int k = 1; k <= 10; ++k) unit.mouseDrag (ev (p - juce::Point<float> (0, (float) dy * (float) k / 10.0f), m.withFlags (juce::ModifierKeys::leftButtonModifier)));
        unit.mouseUp (ev (p - juce::Point<float> (0, (float) dy), m));
    }
    float hostParam (const juce::String& id) { return proc.value (id); }

    void run (const juce::File& report)
    {
        // A known starting point (the standalone restores its last session).
        proc.setCurrentProgram (2); st.heldStep = -1; st.funcLatched = false; st.gridRec = false; st.stepPage = 0;
        // 1. Every knob, key and screen is hit at the centre of its top (and knobs also at their base, nearer the viewer).
        int misses = 0; juce::String missed;
        for (int i = 0; i < (int) Model::get().ctls.size(); ++i)
        {
            const auto& c = Model::get().ctls[(size_t) i];
            if (c.kind == panel::Kind::Led) continue;
            const int got = unit.controlAt (c.top.map (0, 0) * unit.pxScale());
            const int got2 = c.kind == panel::Kind::Knob ? unit.controlAt (c.base.map (0, c.size * 0.4f) * unit.pxScale()) : got;
            if (got != i || got2 != i) { ++misses; missed << c.id << " "; }
        }
        check (misses == 0, "every control is hit where it is drawn", misses ? missed : juce::String (Model::get().ctls.size()) + " controls");

        // 2. TRACK 3 selects track 3 (the MIDI focus follows).
        click ("track3");
        check (st.track == 2 && proc.withProject ([] (const Project& pr) { return pr.midi.focus; }) == 2, "TRACK 3 selects the track");

        // 3. CUTOFF up: the track's host parameter rises.
        const float c0 = hostParam ("t3_cutoff");
        drag ("cutoff", 120);
        const float c1 = hostParam ("t3_cutoff");
        check (c1 > c0 * 1.5f, "CUTOFF drag turns t3_cutoff", juce::String (c0, 0) + " -> " + juce::String (c1, 0) + " Hz");

        // 4. FUNC latched: CUTOFF turns KEYTRACK (a sound parameter of track 3).
        click ("func");
        const float k0 = proc.soundValue (2, 0, KTRK);
        drag ("cutoff", 80);
        const float k1 = proc.soundValue (2, 0, KTRK);
        click ("func");
        check (k1 > k0 + 0.1f && ! st.isFunc(), "FUNC + CUTOFF turns KEYTRACK", juce::String (k0, 2) + " -> " + juce::String (k1, 2));

        // 5. ALGORITHM is stepped: a drag of 40 px moves it by about three algorithms.
        const int a0 = (int) proc.soundValue (2, 0, ALGO);
        drag ("algorithm", 40);
        const int a1 = (int) proc.soundValue (2, 0, ALGO);
        check (a1 != a0, "ALGORITHM steps the algorithm", juce::String (a0) + " -> " + juce::String (a1));

        // 6. REC, then STEP 5 toggles step 5 of track 3 in the current pattern (and back).
        click ("rec");
        const int pat = currentPattern (proc);
        auto stepOn = [&] (int i) { return proc.withProject ([&] (const Project& pr) { return pr.bank.p[(size_t) pat].t[2].lane[0][(size_t) i].on(); }); };
        const bool s0 = stepOn (4);
        click ("step5");
        const bool s1 = stepOn (4);
        click ("step5");
        check (st.gridRec && s1 != s0 && stepOn (4) == s0, "REC + STEP 5 toggles the step");

        // 7. Alt-click holds a step; RESO then makes a lock on it, not a change of the sound.
        click ("step6", juce::ModifierKeys (juce::ModifierKeys::altModifier));
        check (st.heldStep == 5, "Alt-click holds STEP 6");
        const float r0 = hostParam ("t3_reso");
        drag ("reso", 60);
        const bool locked = proc.withProject ([&] (const Project& pr) { return stepedit::hasLock (pr.bank.p[(size_t) pat], 2, 0, 5, RESO); });
        check (locked && std::abs (hostParam ("t3_reso") - r0) < 1e-6f, "held step + RESO makes a lock (the knob's value stays)");
        // The held step's trig fields: ALGORITHM picks the field, DATA edits it (velocity).
        drag ("algorithm", 12);
        check (st.trigField == 1, "held step: ALGORITHM picks the trig field", "field " + juce::String (st.trigField));
        const int v0 = proc.withProject ([&] (const Project& pr) { return (int) pr.bank.p[(size_t) pat].t[2].lane[0][5].vel; });
        drag ("data", -60);
        const int v1 = proc.withProject ([&] (const Project& pr) { return (int) pr.bank.p[(size_t) pat].t[2].lane[0][5].vel; });
        check (v1 < v0, "held step: DATA edits the velocity", juce::String (v0) + " -> " + juce::String (v1));
        click ("step6");
        check (st.heldStep < 0, "clicking the held step releases it");
        click ("rec");

        // 8. FUNC + STEP 10 selects pattern 2 (FUNC + STEP 2 shows steps 17-32).
        click ("func"); click ("step10"); click ("step2"); click ("func");
        check (currentPattern (proc) == 1 && st.stepPage == 1, "FUNC + STEP 10 selects pattern 2, FUNC + STEP 2 steps 17-32", "pattern " + juce::String (currentPattern (proc) + 1));
        click ("func"); click ("step9"); click ("step1"); click ("func");

        // 9. The display: a click on its right half moves to the next field of the page.
        st.page = Page::FilterAmp; st.field[(size_t) Page::FilterAmp] = 0;
        { const auto p = at ("display", 10, 0); unit.mouseDown (ev (p, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier))); unit.mouseUp (ev (p)); }
        check (st.field[(size_t) Page::FilterAmp] == 1, "display click steps the field");
        // FUNC + DATA changes the page.
        click ("func"); drag ("data", 30); click ("func");
        check (st.page != Page::FilterAmp, "FUNC + DATA changes the page", pageName (st.page));

        // 10. FUNC + TRACK 4 mutes track 4 (and again unmutes).
        click ("func"); click ("track4");
        const bool m1 = proc.withProject ([] (const Project& pr) { return pr.track[3].mute; });
        click ("track4"); click ("func");
        check (m1 && ! proc.withProject ([] (const Project& pr) { return pr.track[3].mute; }), "FUNC + TRACK 4 mutes and unmutes");

        // 11. Chord memory: Alt-click CHORD opens the capture, three STEP keys play, CHORD stores the shape.
        {
            const int before = proc.withProject ([] (const Project& pr) { int k = 0; for (const auto& m : pr.chordMem) k += m.n > 0; return k; });
            click ("chord", juce::ModifierKeys (juce::ModifierKeys::altModifier));
            for (const char* k : { "step1", "step3", "step5" }) { click (k); juce::Thread::sleep (80); }
            juce::Thread::sleep (120);
            click ("chord");
            const auto shape = proc.withProject ([] (const Project& pr) {
                juce::String r; for (const auto& m : pr.chordMem) if (m.n == 3) { r.clear(); for (int i = 0; i < m.n; ++i) r << (i ? " " : "") << (int) m.iv[i]; } return r; });
            const int after = proc.withProject ([] (const Project& pr) { int k = 0; for (const auto& m : pr.chordMem) k += m.n > 0; return k; });
            check ((after > before || shape.isNotEmpty()) && ! proc.chordCapture.load(), "CHORD + three keys stores a chord memory", shape.isEmpty() ? "none" : "intervals " + shape);
        }

        // 12. Values on the pages carry units (no raw floats).
        {
            juce::String bad;
            for (Page pg : { Page::Voice, Page::Ops, Page::FilterAmp, Page::Lfo, Page::Mod, Page::Mix, Page::Fx, Page::Midi, Page::ChordScale, Page::Song })
                for (auto& f : fieldsFor (proc, st, pg))
                {
                    const auto t = f.text ? f.text() : juce::String();
                    // A raw float: digits with four or more decimals.
                    const int dot = t.indexOfChar ('.');
                    if (dot >= 0 && t.substring (dot + 1).initialSectionContainingOnly ("0123456789").length() >= 4) bad << pageName (pg) << ":" << f.name << "=" << t << " ";
                }
            check (bad.isEmpty(), "every page value is formatted", bad.isEmpty() ? juce::String ("ok") : bad);
        }

        // 13. Double-click returns CUTOFF to its default.
        { const auto p = at ("cutoff"); unit.mouseDoubleClick (ev (p)); }
        check (std::abs (hostParam ("t3_cutoff") - 8000.0f) < 1.0f, "double-click: CUTOFF back to its default", juce::String (hostParam ("t3_cutoff"), 0) + " Hz");

        lines.add (fails == 0 ? "PASS: 0 failure(s)" : "FAIL: " + juce::String (fails) + " failure(s)");
        report.replaceWithText (lines.joinIntoString ("\n") + "\n");
    }
};

} // namespace dh::dv::ui
