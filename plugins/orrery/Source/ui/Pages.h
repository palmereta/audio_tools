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

// ───────── simple ─────────
class SimplePage : public Page, private juce::ListBoxModel
{
public:
    std::function<void (int)> onLane;
    explicit SimplePage (OrreryProcessor& p) : Page (p)
    {
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
        for (int l = 0; l < kLanes; ++l)
        {
            const auto r = chipLabels[(size_t) l];
            g.setColour (col::lane (l)); g.fillEllipse (r.getX() + 2.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
            g.setColour (col::dim); g.setFont (font (11.5f));
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
                 { "bend_range", "Synth bend range", "set it to your synth's pitch bend range, in semitones" } })
            opts.push_back (make<ParamBox> (p, id, lab, tip));
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
        g.drawFittedText ("Every lane always goes to the DAW on its own MIDI channel. To play several synths from one Orrery, pick ports here:\n"
                          "- hardware synths: their USB or MIDI interface port;\n"
                          "- other tracks in your DAW: a virtual port (IAC Driver in macOS Audio MIDI Setup, loopMIDI on Windows); then set each track's input to that port and the lane's channel.\n"
                          "In Ableton Live, routing MIDI between tracks merges all channels, so use a virtual port, or put one Orrery per track with Link set to Lead and Follow.",
                          note, juce::Justification::topLeft, 12);
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
        gridLayout (o.removeFromTop (4 * 52), { opts.begin(), opts.end() }, 2, 44, 8);
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
        for (int l = 0; l < kLanes; ++l)
        {
            auto* cb = ports[(size_t) l];
            cb->clear (juce::dontSendNotification);
            cb->addItem ("DAW only", 1);
            for (int i = 0; i < devices.size(); ++i) cb->addItem (devices[i].name, i + 2);
            int sel = 0;
            for (int i = 0; i < devices.size(); ++i) if (devices[i].identifier == proc.direct.lanePort (l)) sel = i + 1;
            cb->setSelectedItemIndex (sel, juce::dontSendNotification);
        }
    }
    void timerCallback() override
    {
        const auto now = juce::MidiOutput::getAvailableDevices();
        if (now.size() != devices.size()) fillPorts();
    }
};

} // namespace dh::orr::ui
