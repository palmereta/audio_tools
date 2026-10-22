#pragma once
// Páginas de Orrery: la simple (climas, el orrery y cinco macros) y las avanzadas (carriles, armonía y
// sesión, salidas).

#include "OrreryView.h"

namespace dh::orr::ui {

inline const juce::StringArray& moods() { static juce::StringArray m { "Ambient", "Dark Ambient", "Drone", "Sleep", "Meditation", "Focus", "Celestial" }; return m; }
inline juce::String moodLine (const juce::String& m)
{
    if (m == "Ambient") return "Slow chords, a voice, glass in the distance";
    if (m == "Dark Ambient") return "Low, minor, unresolved";
    if (m == "Drone") return "One centre that barely moves";
    if (m == "Sleep") return "Very sparse, fading out over the session";
    if (m == "Meditation") return "Phrases paced by your breathing";
    if (m == "Focus") return "Steady, quiet, no surprises";
    if (m == "Celestial") return "Bright, high, wide";
    return {};
}

class Page : public juce::Component
{
public:
    explicit Page (OrreryProcessor& p) : proc (p) {}
    virtual void refresh() { repaint(); }
protected:
    OrreryProcessor& proc;
    std::vector<std::unique_ptr<juce::Component>> owned;
    template <typename T, typename... A> T* make (A&&... a) { auto c = std::make_unique<T> (std::forward<A> (a)...); auto* p = c.get(); owned.push_back (std::move (c)); addAndMakeVisible (p); return p; }
};

// ───────── rol: Auto, Independent, Main, Satellite ─────────
inline juce::String roleWord (int r) { static const char* N[4] = { "Auto", "Independent", "Main", "Satellite" }; return N[juce::jlimit (0, 3, r)]; }
inline juce::String laneList (uint32_t m, const std::function<int (int)>& roleOf)
{
    juce::StringArray s;
    for (int l = 0; l < kLanes; ++l) if ((m >> l) & 1u) s.add ("lane " + juce::String (l + 1) + " " + roleName (roleOf (l)));
    return s.joinIntoString (", ");
}

class RoleButton : public juce::TextButton, private juce::Timer
{
public:
    explicit RoleButton (OrreryProcessor& p) : proc (p)
    {
        onClick = [this] {
            juce::PopupMenu m;
            const int cur = (int) proc.value ("role");
            m.addSectionHeader ("Role of this Orrery");
            m.addItem (1, "Auto (the first is Main, the next ones satellites)", true, cur == RoleAuto);
            m.addItem (2, "Independent (plays every lane here; Link works)", true, cur == RoleIndependent);
            m.addItem (3, "Main (satellites play its lanes)", true, cur == RoleMain);
            m.addItem (4, "Satellite (plays one lane of the Main)", true, cur == RoleSatellite);
            m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [this] (int r) { if (r > 0) proc.setRoleFromUi (r - 1); });
        };
        refresh(); startTimerHz (4);
    }
    void mouseEnter (const juce::MouseEvent& e) override { juce::TextButton::mouseEnter (e); showHelp ("Role: Main and its satellites play as one Orrery, each satellite one lane on its own track. Independent works like before 1.0.5."); }
    void mouseExit (const juce::MouseEvent& e) override { juce::TextButton::mouseExit (e); showHelp ({}); }
private:
    OrreryProcessor& proc;
    void refresh()
    {
        const int set = (int) proc.value ("role"), eff = proc.role();
        juce::String t = "Role: " + (eff > 0 ? roleWord (eff) : roleWord (set));
        if (set == RoleAuto && eff > 0) t << " (Auto)";
        if (t != getButtonText()) setButtonText (t);
    }
    void timerCallback() override { if (! dh::reallyShowing (*this)) return; refresh(); }
};

// ───────── simple ─────────
class SimplePage : public Page, private juce::ListBoxModel, private juce::Timer
{
public:
    std::function<void (int)> onLane;
    explicit SimplePage (OrreryProcessor& p) : Page (p)
    {
        roleBtn = make<RoleButton> (p);
        skipT = make<PillToggle> (p, "skip_sat", "Skip satellite lanes", "the Main does not also send the lanes its satellites play (no doubled notes)");
        if (OrreryProcessor::virtualPortsSupported())
            portsT = make<PillToggle> (p, "midi_ports", "Publish lanes as MIDI ports", "one virtual MIDI port per lane (Orrery Drone, Orrery Pad...): pick them as MIDI From on each synth track");
        view = make<OrreryView> (p);
        view->onLane = [this] (int l) { if (onLane) onLane (l); };
        for (auto& m : moods())
        {
            auto* b = make<juce::TextButton> (m);
            b->setClickingTogglesState (false);
            b->onClick = [this, m] { selectMood (m, true); };
            moodBtns.push_back (b);
        }
        addAndMakeVisible (list); list.setModel (this); list.setRowHeight (26);
        static const char* macro[5][3] = { { "density", "Density", "how many notes: from a few to a lot" }, { "motion", "Motion", "how fast chords and loops change" },
                                           { "space", "Space", "longer notes, wider gaps, longer echoes" }, { "brightness", "Brightness", "register and velocity" },
                                           { "tension", "Tension", "from consonant to suspended, clusters and tritones" } };
        for (auto& m : macro) knobs.push_back (make<OrbitKnob> (p, m[0], m[1], m[2], true));
        for (auto [id, lab, tip] : std::initializer_list<std::tuple<const char*, const char*, const char*>> {
                 { "key", "Key", "" }, { "scale", "Scale", "" }, { "tempo", "Tempo", "only when the clock is Free or the host is stopped" },
                 { "session", "Session", "plays for this long, with fades, then stops" }, { "breath", "Breathing", "chords change on whole breaths; phrases start as you breathe out" },
                 { "gentle", "Gentle", "caps loud notes, high notes and leaps" } })
            boxes.push_back (make<ParamBox> (p, id, lab, tip));
        dice = make<juce::TextButton> ("New variation");
        dice->onClick = [this] { proc.newVariation(); };
        seed = make<ParamBox> (p, "seed", "Variation", "the same number always plays the same music with the same settings");
        for (int l = 0; l < kLanes; ++l)
        {
            auto* c = make<ParamBox> (p, "l" + juce::String (l + 1) + "_on", "Lane " + juce::String (l + 1), "switch the lane on or off", false);
            c->tint = col::lane (l);
            chips.push_back (c);
        }
        refresh();
        startTimerHz (4);
    }
    void refresh() override
    {
        const auto cur = proc.getCurrentProgram();
        if (juce::isPositiveAndBelow (cur, (int) proc.getPresets().size())) mood = proc.getPresets()[(size_t) cur].category;
        selectMood (mood, false);
        repaint();
    }
    void paint (juce::Graphics& g) override
    {
        panel (g, leftR, "Moods");
        panel (g, rightR, "Shape");
        g.setColour (col::dim); g.setFont (font (12.5f, true));
        g.drawText ("Key and session", keyTitle, juce::Justification::centredLeft);
        g.setColour (col::faint); g.setFont (font (12.0f));
        g.drawText (moodLine (mood), moodLineR, juce::Justification::centredLeft);
        // La línea del rol (y quién toca qué) y el aviso del sonido de prueba.
        g.setFont (font (12.5f));
        auto info = infoR;
        if (hearingPreview)
        {
            const auto hr = info.removeFromRight (290);
            g.setColour (col::sun);
            g.drawFittedText ("You're hearing Orrery's preview sound", hr, juce::Justification::centredRight, 1, 0.9f);
        }
        g.setColour (warn ? col::sun : col::dim);
        g.drawFittedText (roleLine, info, juce::Justification::centredLeft, 1, 0.85f);
        for (int l = 0; l < kLanes; ++l)
        {
            const auto r = chipLabels[(size_t) l];
            g.setColour (col::lane (l)); g.fillEllipse (r.getX() + 2.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
            const bool bySat = ((satMask >> l) & 1u) != 0;
            if (bySat) { g.setColour (col::sun); g.drawEllipse (r.getX() - 0.5f, r.getCentreY() - 6.5f, 13.0f, 13.0f, 1.2f); }
            g.setColour (col::dim); g.setFont (font (11.5f));
            if (bySat) g.setColour (col::sun);
            g.drawText (roleName ((int) proc.value ("l" + juce::String (l + 1) + "_role")), r.withTrimmedLeft (14), juce::Justification::centredLeft);
            g.setColour (col::faint);
            g.drawText (juce::String ((int) proc.value ("l" + juce::String (l + 1) + "_channel")), r, juce::Justification::centredRight);
        }
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        leftR = r.removeFromLeft (270); r.removeFromLeft (14);
        rightR = r.removeFromRight (360); r.removeFromRight (14);
        auto c = r;
        auto strip = c.removeFromTop (30);
        roleBtn->setBounds (strip.removeFromLeft (190)); strip.removeFromLeft (8);
        if (portsT != nullptr) { portsT->setBounds (strip.removeFromRight (250)); strip.removeFromRight (8); }
        skipT->setBounds (strip.removeFromLeft (std::min (strip.getWidth(), 210)));
        c.removeFromTop (2);
        infoR = c.removeFromTop (22);
        auto chipRow = c.removeFromBottom (56); c.removeFromBottom (8);
        view->setBounds (c);
        const int cw = chipRow.getWidth() / kLanes;
        for (int l = 0; l < kLanes; ++l)
        {
            auto cr = juce::Rectangle<int> (chipRow.getX() + l * cw, chipRow.getY(), cw - 6, chipRow.getHeight());
            chipLabels[(size_t) l] = cr.removeFromTop (20);
            chips[(size_t) l]->setBounds (cr.withHeight (30));
        }
        auto l = leftR.withTrimmedTop (32).reduced (12, 6);
        for (auto* b : moodBtns) { b->setBounds (l.removeFromTop (32)); l.removeFromTop (6); }
        moodLineR = l.removeFromTop (22); l.removeFromTop (4);
        list.setBounds (l);
        auto rr = rightR.withTrimmedTop (32).reduced (12, 6);
        auto top = rr.removeFromTop (140);
        gridLayout (top, { knobs[0], knobs[1], knobs[2] }, 3, 140, 6);
        auto mid = rr.removeFromTop (140);
        gridLayout (mid.reduced (54, 0), { knobs[3], knobs[4] }, 2, 140, 6);
        rr.removeFromTop (10);
        keyTitle = rr.removeFromTop (18);
        gridLayout (rr.removeFromTop (2 * 44 + 8), { boxes[0], boxes[1], boxes[2], boxes[3], boxes[4], boxes[5] }, 3, 44, 8);
        rr.removeFromTop (12);
        auto bottom = rr.removeFromTop (44);
        seed->setBounds (bottom.removeFromLeft (110)); bottom.removeFromLeft (10);
        dice->setBounds (bottom.withTrimmedTop (16));
    }
private:
    OrreryView* view = nullptr;
    RoleButton* roleBtn = nullptr;
    PillToggle *skipT = nullptr, *portsT = nullptr;
    juce::Rectangle<int> infoR;
    juce::String roleLine; bool warn = false, hearingPreview = false; uint32_t satMask = 0;
    void timerCallback() override
    {
        if (! dh::reallyShowing (*this)) return;      // editor escondido (Live lo esconde sin cerrarlo): nada que repintar
        const int r = proc.role();
        skipT->setVisible (r == RoleMain);
        juce::String line; bool w = false; uint32_t mask = 0;
        auto roleOf = [this] (int l) { return (int) proc.value ("l" + juce::String (l + 1) + "_role"); };
        if (r == RoleMain)
        {
            const auto sats = proc.satellites();
            juce::StringArray parts;
            // Sin el nombre de la pista (Live sólo lo dice al renombrarla), el carril basta: "1 Drone".
            for (auto& s : sats) if (s.alive) { parts.add (laneList (s.lanes, roleOf) + (s.name.isNotEmpty() ? " (" + s.name + ")" : juce::String())); mask |= s.lanes; }
            if (proc.otherMainActive()) { line = "Another Orrery is already the Main: satellites follow that one. Set this one to Independent."; w = true; }
            else if (parts.isEmpty()) line = "Main: each Orrery you add on another track becomes a satellite.";
            else line = "Satellites: " + parts.joinIntoString (", ");
        }
        else if (r == RoleIndependent) line = "Independent: every lane plays from this track" + juce::String (proc.value ("link") > 0.5f ? " (Link is on)." : ".");
        if (proc.value ("midi_ports") > 0.5f && OrreryProcessor::virtualPortsSupported())
        {
            if (! proc.direct.virtualPortsOpen()) { line = "Could not create the MIDI ports (is another program using the same names?)."; w = true; }
            else line << "  Ports: " << proc.direct.virtualPortNames().joinIntoString (", ").upToFirstOccurrenceOf (",", false, false) << " and 7 more.";
        }
        const bool hp = proc.value ("preview") > 0.5f && proc.view().running;
        if (line != roleLine || w != warn || hp != hearingPreview || mask != satMask) { roleLine = line; warn = w; hearingPreview = hp; satMask = mask; repaint(); }
    }
    std::vector<juce::TextButton*> moodBtns;
    juce::ListBox list;
    std::vector<OrbitKnob*> knobs;
    std::vector<ParamBox*> boxes, chips;
    juce::TextButton* dice = nullptr;
    ParamBox* seed = nullptr;
    juce::Rectangle<int> leftR, rightR, keyTitle, moodLineR;
    std::array<juce::Rectangle<int>, kLanes> chipLabels {};
    juce::String mood { "Ambient" };
    std::vector<int> rows;

    void selectMood (const juce::String& m, bool load)
    {
        mood = m;
        for (auto* b : moodBtns) b->setToggleState (b->getButtonText() == mood, juce::dontSendNotification);
        rows.clear();
        for (int i = 0; i < (int) proc.getPresets().size(); ++i) if (proc.getPresets()[(size_t) i].category == mood) rows.push_back (i);
        list.updateContent(); list.repaint();
        if (load && ! rows.empty())
        {
            const bool playing = proc.value ("run") > 0.5f;
            proc.loadPreset (rows[0]);
            if (playing) proc.setValue ("run", 1.0f);
        }
        repaint (moodLineR);
    }
    int getNumRows() override { return (int) rows.size(); }
    void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool) override
    {
        if (row < 0 || row >= (int) rows.size()) return;
        const auto& e = proc.getPresets()[(size_t) rows[(size_t) row]];
        const bool cur = rows[(size_t) row] == proc.getCurrentProgram();
        if (cur) { g.setColour (col::select.withAlpha (0.18f)); g.fillRoundedRectangle (0.0f, 1.0f, (float) w, (float) h - 2.0f, 6.0f); }
        g.setColour (cur ? col::text : col::dim); g.setFont (font (14.0f, cur));
        g.drawText (e.name, 10, 0, w - 20, h, juce::Justification::centredLeft);
    }
    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (row < 0 || row >= (int) rows.size()) return;
        const bool playing = proc.value ("run") > 0.5f;
        proc.loadPreset (rows[(size_t) row]);
        if (playing) proc.setValue ("run", 1.0f);
        list.repaint();
    }
};

// ───────── carriles (avanzado) ─────────
class LanesPage : public Page
{
public:
    explicit LanesPage (OrreryProcessor& p) : Page (p)
    {
        for (auto& r : rowDefs())
            for (int l = 0; l < kLanes; ++l)
            {
                auto* b = make<ParamBox> (p, "l" + juce::String (l + 1) + "_" + r.id, "Lane " + juce::String (l + 1) + " " + r.label, r.help, false);
                b->tint = col::lane (l);
                cells.push_back (b);
            }
    }
    void highlight (int lane) { sel = lane; repaint(); }
    void paint (juce::Graphics& g) override
    {
        panel (g, getLocalBounds().reduced (14), {});
        const auto& R = rowDefs();
        g.setFont (font (12.0f));
        for (size_t i = 0; i < R.size(); ++i)
        {
            g.setColour ((i % 2) ? col::faint.withAlpha (0.05f) : juce::Colours::transparentBlack);
            g.fillRect (rowRect ((int) i).withX (labelX - 6).withRight (getWidth() - 26));
            g.setColour (col::dim);
            g.drawText (R[i].label, labelX, rowRect ((int) i).getY(), labelW - 8, rowH, juce::Justification::centredLeft);
        }
        for (int l = 0; l < kLanes; ++l)
        {
            const auto hr = juce::Rectangle<int> (colX (l), headerY, colW, 22);
            g.setColour (col::lane (l).withAlpha (sel == l ? 1.0f : 0.8f)); g.fillEllipse ((float) hr.getX(), (float) hr.getCentreY() - 5.0f, 10.0f, 10.0f);
            g.setColour (sel == l ? col::text : col::dim); g.setFont (font (13.0f, true));
            g.drawText ("Lane " + juce::String (l + 1), hr.withTrimmedLeft (16), juce::Justification::centredLeft);
            if (sel == l) { g.setColour (col::lane (l).withAlpha (0.08f)); g.fillRoundedRectangle (juce::Rectangle<int> (colX (l) - 3, headerY - 4, colW + 6, rowRect ((int) R.size() - 1).getBottom() - headerY + 8).toFloat(), 6.0f); }
        }
    }
    void resized() override
    {
        const auto& R = rowDefs();
        const auto area = getLocalBounds().reduced (26, 22);
        labelX = area.getX(); labelW = 150; headerY = area.getY();
        colW = (area.getWidth() - labelW - 7 * 6) / kLanes;
        rowH = std::min (22, (area.getHeight() - 30) / (int) R.size() - 2);
        for (size_t i = 0; i < R.size(); ++i)
            for (int l = 0; l < kLanes; ++l)
                cells[i * kLanes + (size_t) l]->setBounds (juce::Rectangle<int> (colX (l), rowRect ((int) i).getY(), colW, rowH));
    }
private:
    struct RowDef { const char* id; const char* label; const char* help; };
    static const std::vector<RowDef>& rowDefs()
    {
        static const std::vector<RowDef> r = {
            { "on", "On", "" }, { "role", "Role", "drone, pad, melody, arpeggio, cloud, bass or pulse" }, { "channel", "MIDI channel", "" },
            { "octave", "Octave", "" }, { "range", "Range", "how far notes may spread, in semitones" }, { "density", "Density", "" },
            { "length", "Length", "note length" }, { "velocity", "Velocity", "" }, { "vel_rand", "Velocity random", "" }, { "prob", "Probability", "chance that each note plays" },
            { "loop", "Loop", "record a phrase and repeat it (Eno's tape loops); different lengths per lane rarely line up" },
            { "loop_beats", "Loop length", "beats; prime numbers (13, 17, 23...) keep lanes out of sync for a long time" },
            { "mutate", "Mutate", "chance that a looped note changes each pass" }, { "rate", "Rate", "step length for arpeggio and pulse" },
            { "eu_steps", "Euclid steps", "" }, { "eu_pulses", "Euclid pulses", "0 = random hits by density" }, { "eu_rot", "Euclid rotate", "" },
            { "arp", "Arp pattern", "" }, { "echoes", "Echoes", "MIDI repeats with falling velocity" }, { "echo_time", "Echo time", "" }, { "echo_decay", "Echo decay", "" },
            { "follow", "Follow", "chord tones, any scale tone, or root and fifth" }, { "voices", "Voices", "notes per pad chord" },
            { "cc", "CC number", "a slow curve on this controller (74 is often the filter)" }, { "cc_shape", "CC shape", "sine, random drift, breathing, or swell with the notes" },
            { "cc_beats", "CC period", "" }, { "cc_depth", "CC depth", "" }, { "cc_center", "CC centre", "" },
            { "drift", "Pitch drift", "slow pitch bend wander, like an analog oscillator" },
        };
        return r;
    }
    std::vector<ParamBox*> cells;
    int labelX = 0, labelW = 150, headerY = 0, colW = 100, rowH = 20, sel = -1;
    int colX (int l) const { return labelX + labelW + l * (colW + 6); }
    juce::Rectangle<int> rowRect (int i) const { return { labelX, headerY + 30 + i * (rowH + 2), getWidth(), rowH }; }
};

// ───────── armonía y sesión ─────────
class HarmonyPage : public Page
{
public:
    explicit HarmonyPage (OrreryProcessor& p) : Page (p)
    {
        for (auto [id, lab, tip] : std::initializer_list<std::tuple<const char*, const char*, const char*>> {
                 { "density", "Density", "" }, { "motion", "Motion", "" }, { "space", "Space", "" }, { "brightness", "Brightness", "" },
                 { "tension", "Tension", "" }, { "color", "Chord colour", "triads, sevenths, ninths, elevenths" } })
            knobs.push_back (make<OrbitKnob> (p, id, lab, tip));
        for (auto [id, lab, tip] : std::initializer_list<std::tuple<const char*, const char*, const char*>> {
                 { "key", "Key", "" }, { "scale", "Scale", "" }, { "chord_beats", "Chord length", "beats per chord at Motion 50 %" },
                 { "progression", "Progression", "slow walk, drone (almost static) or two chords" }, { "tempo", "Tempo", "" },
                 { "clock", "Clock", "Host follows the DAW transport; Free uses the tempo here" }, { "run_stopped", "Play with host stopped", "" },
                 { "seed", "Variation", "" }, { "humanize", "Humanize", "timing wander in milliseconds" } })
            harm.push_back (make<ParamBox> (p, id, lab, tip));
        for (auto [id, lab, tip] : std::initializer_list<std::tuple<const char*, const char*, const char*>> {
                 { "session", "Session", "plays for this long and stops; Endless never stops" }, { "fade_in", "Fade in", "" }, { "fade_out", "Fade out", "before the session ends" },
                 { "breath", "Breathing", "breaths per minute; 6 is a common slow-breathing pace" }, { "gentle", "Gentle", "no loud or high notes, small leaps, low tension" },
                 { "fade_cc7", "Fade with CC7", "also moves each channel's volume, so held notes fade too" }, { "level", "Level", "overall velocity" } })
            sess.push_back (make<ParamBox> (p, id, lab, tip));
    }
    void paint (juce::Graphics& g) override { panel (g, a, "Shape"); panel (g, b, "Harmony and time"); panel (g, c, "Session"); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        a = r.removeFromTop (190); r.removeFromTop (14);
        b = r.removeFromLeft (r.getWidth() / 2 - 7); r.removeFromLeft (14); c = r;
        gridLayout (a.withTrimmedTop (32).reduced (12, 6), { knobs.begin(), knobs.end() }, 6, 140, 8);
        gridLayout (b.withTrimmedTop (32).reduced (12, 6), { harm.begin(), harm.end() }, 3, 44, 12);
        gridLayout (c.withTrimmedTop (32).reduced (12, 6), { sess.begin(), sess.end() }, 3, 44, 12);
    }
private:
    std::vector<OrbitKnob*> knobs; std::vector<ParamBox*> harm, sess;
    juce::Rectangle<int> a, b, c;
};

// ───────── salidas ─────────
class OutputsPage : public Page, private juce::Timer
{
public:
    explicit OutputsPage (OrreryProcessor& p) : Page (p)
    {
        for (int l = 0; l < kLanes; ++l)
        {
            auto* cb = make<juce::ComboBox>();
            cb->onChange = [this, l, cb] {
                if (cb->getSelectedId() == 1000) return;          // "Not connected": se conserva el puerto elegido
                const int i = cb->getSelectedItemIndex();
                proc.direct.setLanePort (l, i <= 0 || i - 1 >= devices.size() ? juce::String() : devices[i - 1].identifier);
            };
            ports.push_back (cb);
            auto* ch = make<ParamBox> (p, "l" + juce::String (l + 1) + "_channel", "Channel", "", false);
            ch->tint = col::lane (l);
            chans.push_back (ch);
        }
        allBtn = make<juce::TextButton> ("Send every lane to the first port");
        allBtn->onClick = [this] { if (ports[0]->getSelectedItemIndex() > 0) for (int l = 1; l < kLanes; ++l) ports[(size_t) l]->setSelectedItemIndex (ports[0]->getSelectedItemIndex()); };
        rescan = make<juce::TextButton> ("Rescan ports");
        rescan->onClick = [this] { fillPorts(); };
        for (auto [id, lab, tip] : std::initializer_list<std::tuple<const char*, const char*, const char*>> {
                 { "clock_out", "MIDI clock out", "24 pulses per beat plus start and stop, to the direct ports" },
                 { "link", "Link", "Lead shares its key and chords; Follow instances in other tracks play over them" },
                 { "preview", "Preview sound", "the built-in sound; turn it off when your synths play" }, { "preview_vol", "Preview volume", "" }, { "preview_rev", "Preview reverb", "" },
                 { "ref_pitch", "Reference pitch", "A4 in Hz, applied by pitch bend" }, { "tune_bend", "Tune by pitch bend", "" },
                 { "bend_range", "Synth bend range", "set it to your synth's pitch bend range, in semitones" },
                 { "skip_sat", "Skip satellite lanes", "a Main does not also send the lanes its satellites play" },
                 { "midi_ports", "Lanes as MIDI ports", "one virtual MIDI port per lane, for Ableton Live (macOS and Linux)" } })
        {
            if (juce::String (id) == "midi_ports" && ! OrreryProcessor::virtualPortsSupported()) continue;
            opts.push_back (make<ParamBox> (p, id, lab, tip));
        }
        fillPorts();
        startTimer (3000);
    }
    void paint (juce::Graphics& g) override
    {
        panel (g, a, "Direct MIDI outputs");
        panel (g, b, "Options");
        for (int l = 0; l < kLanes; ++l)
        {
            const auto r = laneLab[(size_t) l];
            g.setColour (col::lane (l)); g.fillEllipse ((float) r.getX(), (float) r.getCentreY() - 5.0f, 10.0f, 10.0f);
            g.setColour (col::text); g.setFont (font (13.5f, true));
            g.drawText ("Lane " + juce::String (l + 1) + "  " + roleName ((int) proc.value ("l" + juce::String (l + 1) + "_role")), r.withTrimmedLeft (16), juce::Justification::centredLeft);
        }
        g.setColour (col::dim); g.setFont (font (13.0f));
        g.drawFittedText (juce::String ("Every lane always goes to the DAW on its own MIDI channel. To play several synths from one Orrery:\n"
                          "- Ableton Live: one Orrery per synth, each on its own track. The first is the Main and each new one a satellite that plays one lane (Simple page, Role).\n")
                          + (OrreryProcessor::virtualPortsSupported()
                                ? "- Lanes as MIDI ports: one virtual port per lane (Orrery Drone, Orrery Pad...) to pick as each track's MIDI input.\n"
                                : "- Windows has no virtual MIDI ports of its own: install loopMIDI, create a port and pick it on the left.\n")
                          + "- Hardware synths: their USB or MIDI interface port, on the left.\n"
                          "- Other tracks: a virtual port (IAC Driver on macOS, loopMIDI on Windows) with each track's input on that port and the lane's channel.",
                          note, juce::Justification::topLeft, 14);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        a = r.removeFromLeft (r.getWidth() * 58 / 100); r.removeFromLeft (14); b = r;
        auto in = a.withTrimmedTop (32).reduced (12, 6);
        auto top = in.removeFromTop (30);
        rescan->setBounds (top.removeFromRight (130)); top.removeFromRight (8); allBtn->setBounds (top.removeFromRight (250));
        in.removeFromTop (10);
        for (int l = 0; l < kLanes; ++l)
        {
            auto row = in.removeFromTop (36); in.removeFromTop (6);
            laneLab[(size_t) l] = row.removeFromLeft (170);
            chans[(size_t) l]->setBounds (row.removeFromRight (80).reduced (0, 3)); row.removeFromRight (10);
            ports[(size_t) l]->setBounds (row.reduced (0, 3));
        }
        auto o = b.withTrimmedTop (32).reduced (12, 6);
        gridLayout (o.removeFromTop (5 * 52), { opts.begin(), opts.end() }, 2, 44, 8);
        o.removeFromTop (16);
        note = o;
    }
private:
    std::vector<juce::ComboBox*> ports; std::vector<ParamBox*> chans, opts;
    juce::TextButton *allBtn = nullptr, *rescan = nullptr;
    juce::Array<juce::MidiDeviceInfo> devices;
    std::array<juce::Rectangle<int>, kLanes> laneLab {};
    juce::Rectangle<int> a, b, note;
    void fillPorts()
    {
        devices = juce::MidiOutput::getAvailableDevices();
        proc.direct.reopenMissing();                 // un puerto que apareció después de cargar el proyecto se abre ahora
        for (int l = 0; l < kLanes; ++l)
        {
            auto* cb = ports[(size_t) l];
            cb->clear (juce::dontSendNotification);
            cb->addItem ("DAW only", 1);
            for (int i = 0; i < devices.size(); ++i) cb->addItem (devices[i].name, i + 2);
            int sel = 0;
            for (int i = 0; i < devices.size(); ++i) if (devices[i].identifier == proc.direct.lanePort (l)) sel = i + 1;
            // Elegido pero sin conectar: se muestra como tal, no como si saliera por ahí.
            if (! proc.direct.laneConnected (l)) { cb->addItem ("Not connected (" + proc.direct.lanePort (l) + ")", 1000); cb->setSelectedId (1000, juce::dontSendNotification); continue; }
            cb->setSelectedItemIndex (sel, juce::dontSendNotification);
        }
    }
    void timerCallback() override
    {
        if (! dh::reallyShowing (*this)) return;      // editor escondido (Live lo esconde sin cerrarlo): nada que repintar
        const auto now = juce::MidiOutput::getAvailableDevices();
        if (now.size() != devices.size()) fillPorts();
    }
};


// ───────── satélite: lo único que importa es qué carril del Main toca ─────────
class SatellitePage : public Page, private juce::Timer
{
public:
    explicit SatellitePage (OrreryProcessor& p) : Page (p)
    {
        view = make<OrreryView> (p);
        roleBtn = make<RoleButton> (p);
        indep = make<juce::TextButton> ("Make independent");
        indep->onClick = [this] { proc.makeIndependentFromMain(); };
        mainBtn = make<juce::TextButton> ("Make this the Main");
        mainBtn->onClick = [this] { proc.setRoleFromUi (RoleMain); };
        startTimerHz (8);
    }
    void paint (juce::Graphics& g) override
    {
        panel (g, leftR, {});
        const uint32_t m = proc.satelliteLanes();
        const int sync = proc.satelliteSync();
        const auto mn = proc.mainName();
        auto r = leftR.reduced (26, 22);
        g.setColour (col::text); g.setFont (font (26.0f, true));
        g.drawFittedText (mn.isNotEmpty() ? "Satellite of " + mn : juce::String ("Satellite"), r.removeFromTop (36), juce::Justification::centredLeft, 1, 0.7f);
        int first = -1; for (int l = 0; l < kLanes; ++l) if ((m >> l) & 1u) { first = l; break; }
        juce::StringArray lanes;
        for (int l = 0; l < kLanes; ++l) if ((m >> l) & 1u) lanes.add (juce::String (l + 1) + (sync == 2 ? " " + juce::String (roleName (proc.satelliteLane (l).role)) : juce::String()));
        g.setColour (first >= 0 ? col::lane (first) : col::dim); g.setFont (font (18.0f, true));
        g.drawFittedText (lanes.isEmpty() ? juce::String ("no lane chosen")
                                          : (lanes.size() == 1 ? "playing lane " : "playing lanes ") + lanes.joinIntoString (", ")
                                            + (first >= 0 && sync == 2 ? ",  channel " + juce::String (proc.satelliteLane (first).channel) : juce::String()),
                          r.removeFromTop (30), juce::Justification::centredLeft, 1, 0.7f);
        r.removeFromTop (6);
        g.setFont (font (13.5f));
        juce::String st;
        if (mn.isEmpty()) { g.setColour (col::sun); st = "Waiting for a Main Orrery. Add one on another track, or make this one the Main."; }
        else if (sync == 2) { g.setColour (col::dim); st = proc.view().running ? "In sync: it plays its lane of the Main, note for note. Play, presets and settings follow the Main."
                                                                                 : "In sync, the Main is stopped. Press Play on the Main (or start the transport)."; }
        else { g.setColour (col::sun); st = "Syncing with the Main..."; }
        g.drawFittedText (st, r.removeFromTop (40), juce::Justification::topLeft, 2);
        r.removeFromTop (10);
        g.setColour (col::dim); g.setFont (font (12.5f, true));
        g.drawText ("Lane", r.removeFromTop (18), juce::Justification::centredLeft);
        // Las llaves de los carriles: se dibujan acá (chips); el clic se toma en mouseDown.
        for (int l = 0; l < kLanes; ++l)
        {
            const auto c = chips[(size_t) l].toFloat();
            const bool sel = (m >> l) & 1u, on = sync != 2 || proc.satelliteLane (l).on;
            g.setColour (sel ? col::lane (l).withAlpha (0.25f) : col::bg); g.fillRoundedRectangle (c, 8.0f);
            g.setColour (sel ? col::lane (l) : col::border); g.drawRoundedRectangle (c.reduced (0.5f), 8.0f, sel ? 1.6f : 1.0f);
            g.setColour (on ? col::text : col::faint); g.setFont (font (20.0f, true));
            g.drawText (juce::String (l + 1), c.withTrimmedBottom (c.getHeight() * 0.42f).toNearestInt(), juce::Justification::centredBottom);
            g.setColour (on ? col::lane (l) : col::faint); g.setFont (font (11.5f));
            g.drawText (sync != 2 ? juce::String() : (on ? juce::String (roleName (proc.satelliteLane (l).role)) : "off"), c.withTrimmedTop (c.getHeight() * 0.58f).toNearestInt(), juce::Justification::centredTop);
        }
        g.setColour (col::faint); g.setFont (font (12.0f));
        g.drawFittedText ("Click a lane to play it. Shift-click adds or removes a lane. Lanes that are off in the Main stay silent.", hintR, juce::Justification::topLeft, 2);
        // Actividad: cuánto suena cada carril y el acorde del Main.
        const auto& v = proc.view();
        auto a = actR;
        g.setColour (col::dim); g.setFont (font (12.5f, true));
        g.drawText ("Activity", a.removeFromTop (18), juce::Justification::centredLeft);
        for (int l = 0; l < kLanes; ++l)
        {
            if (! ((m >> l) & 1u)) continue;
            auto row = a.removeFromTop (20); a.removeFromTop (4);
            g.setColour (col::dim); g.setFont (font (12.0f));
            g.drawText ("Lane " + juce::String (l + 1), row.removeFromLeft (64), juce::Justification::centredLeft);
            const auto bar = row.withTrimmedRight (60).toFloat().reduced (0, 5);
            g.setColour (col::raised); g.fillRoundedRectangle (bar, 4.0f);
            g.setColour (col::lane (l)); g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * juce::jlimit (0.0f, 1.0f, v.activity[(size_t) l])), 4.0f);
        }
        g.setColour (col::faint); g.setFont (font (12.0f));
        if (v.running) g.drawText ("Chord " + juce::String (v.chord), a.removeFromTop (18), juce::Justification::centredLeft);
        g.drawFittedText ("Ableton Live: on the synth track set MIDI From to this track and \"Audio Tools Orrery\", and Monitor to In.",
                          liveR, juce::Justification::topLeft, 2);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        leftR = r.removeFromLeft (640); r.removeFromLeft (14);
        view->setBounds (r);
        auto in = leftR.reduced (26, 22);
        in.removeFromTop (36 + 30 + 6 + 40 + 10 + 18);
        auto chipRow = in.removeFromTop (60);
        const int cw = (chipRow.getWidth() - 7 * 8) / kLanes;
        for (int l = 0; l < kLanes; ++l) chips[(size_t) l] = { chipRow.getX() + l * (cw + 8), chipRow.getY(), cw, 60 };
        in.removeFromTop (6);
        hintR = in.removeFromTop (36);
        in.removeFromTop (8);
        actR = in.removeFromTop (150);
        auto bottom = in.removeFromBottom (36);
        liveR = in.removeFromBottom (40);
        roleBtn->setBounds (bottom.removeFromLeft (190)); bottom.removeFromLeft (8);
        indep->setBounds (bottom.removeFromLeft (150)); bottom.removeFromLeft (8);
        mainBtn->setBounds (indep->getBounds());
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int l = 0; l < kLanes; ++l)
            if (chips[(size_t) l].contains (e.getPosition()))
            {
                uint32_t m = proc.satelliteLanes();
                if (e.mods.isShiftDown() || e.mods.isCommandDown()) { m ^= 1u << l; if (m == 0) m = 1u << l; }
                else m = 1u << l;
                proc.setValue ("sat_lanes", (float) m);
                repaint();
            }
    }
private:
    OrreryView* view = nullptr;
    RoleButton* roleBtn = nullptr;
    juce::TextButton *indep = nullptr, *mainBtn = nullptr;
    juce::Rectangle<int> leftR, hintR, actR, liveR;
    std::array<juce::Rectangle<int>, kLanes> chips {};
    void timerCallback() override
    {
        if (! dh::reallyShowing (*this)) return;      // editor escondido (Live lo esconde sin cerrarlo): nada que repintar
        const bool noMain = proc.mainName().isEmpty();
        mainBtn->setVisible (noMain); indep->setVisible (! noMain);
        repaint (leftR);
    }
};

} // namespace dh::orr::ui
