#pragma once
// Las cinco páginas de Thrum (Guitar, Play, Pedals, Amp / Cab, Settings) sobre un área de 1328 × 584 (coordenadas
// lógicas del editor de 1360 × 850). Los renders (guitarra, pedales, amplificador y gabinete) salen de
// tools/blender/thrum.py; todo lo demás lo dibuja la interfaz. Inspect agrega los rótulos técnicos en cian.

#include "Controls.h"
#include "Fretboard.h"

namespace thrumui {

inline void panelBox (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (col::panel); g.fillRoundedRectangle (r, 8.0f);
    g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, 1.0f);
}

class Page : public juce::Component
{
public:
    explicit Page (ThrumProcessor& p) : proc (p) {}
    virtual void tick() {}
    virtual void setInspect (bool on) { inspect = on; repaint(); }
    bool inspect = false;
protected:
    ThrumProcessor& proc;
    std::vector<std::unique_ptr<juce::Component>> owned;
    template <typename T, typename... A> T& add (A&&... a)
    {
        auto c = std::make_unique<T> (std::forward<A> (a)...);
        auto& r = *c; addAndMakeVisible (r); owned.push_back (std::move (c)); return r;
    }
    Knob& knob (const char* id, juce::String name = {}, bool small = true) { auto& k = add<Knob> (proc, id, name); k.small = small; return k; }
    // Una fila de perillas de ancho w cada una.
    static void row (std::initializer_list<juce::Component*> cs, int x, int y, int w, int h) { for (auto* c : cs) { if (c) c->setBounds (x, y, w, h); x += w; } }
};

// ───────── Guitar ─────────
// El selector de pastillas con los rótulos de cada guitarra: cinco posiciones (tres single-coil) o tres (dos pastillas).
class PickupSelector : public ParamControl
{
public:
    PickupSelector (ThrumProcessor& p) : ParamControl (p, "g_selector") {}
    int positions() const { return dh::gt::selectorPositions (dh::gt::guitar ((int) proc.value ("g_type"))); }
    juce::StringArray names() const
    {
        return positions() == 5 ? juce::StringArray { "Bridge", "Bridge + middle", "Middle", "Middle + neck", "Neck" } : juce::StringArray { "Bridge", "Both", "Neck" };
    }
    juce::Point<float> dot (int k) const
    {
        const int n = positions();
        return { 14.0f + ((float) getWidth() - 28.0f) * (float) k / (float) (n - 1), 46.0f };
    }
    void paint (juce::Graphics& g) override
    {
        const int n = positions(), cur = std::min (n - 1, juce::roundToInt (value()));
        g.setColour (col::line); g.fillRoundedRectangle (14.0f, 44.0f, (float) getWidth() - 28.0f, 4.0f, 2.0f);
        for (int k = 0; k < n; ++k)
        {
            auto d = dot (k);
            if (k == cur) { g.setColour (col::amber.withAlpha (0.3f)); g.fillEllipse (d.x - 13, d.y - 13, 26, 26); g.setColour (col::amber); g.fillEllipse (d.x - 8, d.y - 8, 16, 16); }
            else { g.setColour (col::dim); g.fillEllipse (d.x - 5, d.y - 5, 10, 10); }
        }
        g.setColour (col::text); text (g, names()[cur].toUpperCase(), { 0.0f, 4.0f, (float) getWidth(), 22.0f }, juce::Justification::centred, 16.0f, true, 0.04f);
        label (g, names()[0], { 0.0f, 62.0f, 120.0f, 16.0f }, juce::Justification::centredLeft, 12.0f);
        label (g, names()[n - 1], { (float) getWidth() - 120.0f, 62.0f, 120.0f, 16.0f }, juce::Justification::centredRight, 12.0f);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        int best = 0; float bd = 1e9f;
        for (int k = 0; k < positions(); ++k) { const float d = std::fabs (dot (k).x - e.position.x); if (d < bd) { bd = d; best = k; } }
        set ((float) best);
    }
};

class GuitarPage : public Page
{
public:
    explicit GuitarPage (ThrumProcessor& p) : Page (p)
    {
        type = &add<Chips> (proc, "g_type", juce::StringArray { "Solid 3S", "Solid 2H", "Semi-hollow", "Hollow jazz", "Baritone", "Seven-string" }, 3);
        tuning = &add<Menu> (proc, "g_tuning", "Tuning");
        gauge = &add<Chips> (proc, "g_gauge", juce::StringArray { "Light", "Regular", "Medium", "Heavy" });
        gauge->fontH = 12.5f;
        for (auto id : { "g_capo", "g_fine", "g_age", "g_sustain", "g_action", "g_buzz", "g_sym" }) body.push_back (&knob (id));
        body[0]->title = "Capo"; body[1]->title = "Fine tune"; body[2]->title = "String age"; body[3]->title = "Sustain"; body[4]->title = "Action"; body[5]->title = "Fret buzz"; body[6]->title = "Sympathetic";
        for (int i = 0; i < 7; ++i) { auto& k = knob (("g_t" + juce::String (i + 1)).toRawUTF8(), "String " + juce::String (i + 1)); custom.push_back (&k); }
        muteOpen = &add<Toggle> (proc, "g_mute_open", "Mute unplayed strings");
        selector = &add<PickupSelector> (proc);
        vol = &knob ("g_volume", "Volume", false); tone = &knob ("g_tone", "Tone", false); cable = &knob ("g_cable", "Cable", false);
        squeak = &knob ("g_squeak", "Finger squeak"); release = &knob ("g_release", "Release"); attack = &knob ("g_attack", "Pick attack"); hum = &knob ("g_noise", "Pickup hum");
        mains = &add<Chips> (proc, "g_mains", juce::StringArray { "50 Hz", "60 Hz" });
        type->setTooltip ("The guitar: scale, frets, strings, body and pickups change with it.");
        tick();
    }
    void resized() override
    {
        const int x = 812;
        type->setBounds (x, 36, 496, 64);
        tuning->setBounds (x, 126, 264, 50); gauge->setBounds (x + 280, 145, 216, 31);
        row ({ body[0], body[1], body[2], body[3], body[4], body[5], body[6] }, x - 6, 186, 72, 82);
        for (size_t i = 0; i < custom.size(); ++i) custom[i]->setBounds (x - 6 + (int) i * 72, 270, 72, 76);
        muteOpen->setBounds (x, 282, 250, 28);
        selector->setBounds (x, 372, 230, 80);
        row ({ vol, tone, cable }, x + 262, 364, 78, 88);
        row ({ squeak, release, attack, hum }, x - 6, 492, 76, 84);
        mains->setBounds (x + 312, 510, 180, 28);
    }
    void tick() override
    {
        const int t = (int) proc.value ("g_type");
        const bool isCustom = (int) proc.value ("g_tuning") == dh::gt::kNumTunings - 1;
        const int n = dh::gt::guitar (t).nStrings;
        for (size_t i = 0; i < custom.size(); ++i) custom[i]->setVisible (isCustom && (int) i < n);
        muteOpen->setVisible (! isCustom);
        if (t != shownType) { shownType = t; selector->repaint(); repaint(0, 0, 790, 584); }
    }
    void paint (juce::Graphics& g) override
    {
        const int t = juce::jlimit (0, 5, (int) proc.value ("g_type"));
        auto im = asset ("guitar_" + juce::String (t) + ".jpg");
        auto rr = juce::Rectangle<float> (0, 0, 780, 584);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip; clip.addRoundedRectangle (rr, 8.0f); g.reduceClipRegion (clip);
            if (im.isValid()) g.drawImage (im, rr, juce::RectanglePlacement::fillDestination | juce::RectanglePlacement::xMid | juce::RectanglePlacement::yTop);
        }
        const auto& G = dh::gt::guitar (t);
        g.setColour (col::text); text (g, juce::String (G.name).toUpperCase(), { 22, 14, 600, 22 }, juce::Justification::centredLeft, 18.0f, true, 0.05f);
        g.setColour (col::dim); text (g, juce::String (juce::roundToInt (G.scaleMm)) + " mm scale  /  " + juce::String (G.frets) + " frets  /  " + juce::String (G.nStrings) + " strings",
                                      { 22, 36, 600, 18 }, juce::Justification::centredLeft, 14.0f);
        if (inspect) paintInspect (g, t, rr);
        panelBox (g, { 792, 0, 536, 584 });
        section (g, "Guitar", 812, 10); section (g, "Strings", 812, 104); section (g, "Pickups", 812, 346); section (g, "Hand noises", 812, 464);
        label (g, "Gauge", { 1092, 126, 120, 17 }, juce::Justification::centredLeft, 12.5f);
    }
private:
    Chips* type; Menu* tuning; Chips* gauge; std::vector<Knob*> body, custom; Toggle* muteOpen; PickupSelector* selector;
    Knob *vol, *tone, *cable, *squeak, *release, *attack, *hum; Chips* mains;
    int shownType = -1;

    // Inspect: dónde escucha cada pastilla (medido desde el puente), cuáles suenan con el selector, y la escala.
    void paintInspect (juce::Graphics& g, int t, juce::Rectangle<float> rr)
    {
        const auto& L = layout()["guitar"][t];
        if (! L.isObject()) return;
        auto at = [&] (const juce::var& v) { auto p = lpt (v); return juce::Point<float> (rr.getX() + p.x * rr.getWidth(), rr.getY() + p.y * (rr.getWidth() * 1188.0f / 1560.0f)); };
        const auto& G = dh::gt::guitar (t);
        const int mask = dh::gt::selectorMask (G, (int) proc.value ("g_selector"));
        const auto* pk = L["pickups"].getArray();
        for (int i = 0; pk && i < pk->size(); ++i)
        {
            auto p = at ((*pk)[i]);
            const bool on = (mask >> i) & 1;
            const float ly = 404.0f + 32.0f * (float) i;
            const float lx = 330.0f;
            g.setColour (on ? col::cyan : col::cyanDim);
            g.drawEllipse (p.x - 5, p.y - 5, 10, 10, 1.5f);
            juce::Path pa; pa.startNewSubPath (p); pa.lineTo (lx - 10, ly); pa.lineTo (lx + 210, ly);
            g.strokePath (pa, juce::PathStrokeType (1.0f));
            const juce::String kind = L["kinds"][i].toString() == "s" ? "single coil" : "humbucker";
            const char* where = pk->size() == 3 ? (i == 0 ? "Bridge" : i == 1 ? "Middle" : "Neck") : (i == 0 ? "Bridge" : "Neck");
            text (g, juce::String (where).toUpperCase() + "  " + juce::String ((int) L["pickup_mm"][i]) + " mm  " + kind.toUpperCase() + (on ? "  ON" : ""),
                  { lx, ly - 17.0f, 260.0f, 15.0f }, juce::Justification::centredLeft, 13.0f, on);
        }
        auto b = at (L["bridge"]), n = at (L["nut"]);
        auto d = n - b; const float len = std::max (1.0f, d.getDistanceFromOrigin());
        juce::Point<float> o (-d.y / len * 40.0f, d.x / len * 40.0f);
        g.setColour (col::cyan.withAlpha (0.8f));
        g.drawLine ({ b + o, n + o }, 1.0f);
        g.drawLine ({ b + o * 0.7f, b + o * 1.3f }, 1.0f); g.drawLine ({ n + o * 0.7f, n + o * 1.3f }, 1.0f);
        auto m = (b + n) * 0.5f + o * 1.5f;
        g.setColour (col::cyan);
        text (g, "SCALE " + juce::String (juce::roundToInt (G.scaleMm)) + " mm", { m.x - 80, m.y - 9, 160, 18 }, juce::Justification::centred, 13.0f, true);
    }
};

// ───────── Play ─────────
// El patrón de rasgueo: un casillero por paso (clic: el golpe siguiente; Mayúsculas+clic: el anterior; clic derecho: el
// acento). Las flechas cambian el largo.
class StrumGrid : public juce::Component
{
public:
    explicit StrumGrid (ThrumProcessor& p) : proc (p) { refresh(); }
    void refresh() { pat = proc.getStrum(); repaint(); }
    static constexpr int kPerRow = 16;
    int count() const { return std::max (1, pat.steps.removeCharacters (" ").length()); }
    juce::Rectangle<float> cell (int i) const
    {
        const int per = std::min (kPerRow, std::max (8, count() > kPerRow ? kPerRow : count()));
        const float w = ((float) getWidth() - 50.0f) / (float) per;
        if (count() <= kPerRow) return { (float) i * w, 0.0f, w - 3.0f, 84.0f };
        return { (float) (i % per) * w, (float) (i / per) * 44.0f, w - 3.0f, 40.0f };
    }
    void paint (juce::Graphics& g) override
    {
        const auto s = pat.steps.removeCharacters (" ");
        const auto acc = pat.accents.removeCharacters (" ");
        const int n = count();
        for (int i = 0; i < n && i < 2 * kPerRow; ++i)
        {
            auto r = cell (i);
            const juce::juce_wchar c = s[i];
            const bool rest = c == '.' || c == '-';
            const int a = i < acc.length() ? (int) (acc[i] - '0') : 0;
            g.setColour (rest ? col::bg : a == 2 ? col::amber : a == 1 ? col::amberDim : col::cell2); g.fillRoundedRectangle (r, 3.0f);
            g.setColour ((i / 4) % 2 ? col::line : col::lineHi); g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);
            g.setColour (a == 2 && ! rest ? col::ink : col::text);
            text (g, juce::String::charToString (c == '|' ? (juce::juce_wchar) '|' : c), r.withTrimmedBottom (10.0f), juce::Justification::centred, 17.0f, true);
            label (g, stepWord (c), r.withTrimmedTop (24.0f), juce::Justification::centred, 9.5f, a == 2 && ! rest ? col::ink : col::dim);
        }
        // Largo.
        auto lr = juce::Rectangle<float> ((float) getWidth() - 44.0f, 0.0f, 44.0f, 84.0f);
        g.setColour (col::cell2); g.fillRoundedRectangle (lr.withHeight (26.0f), 3.0f); g.fillRoundedRectangle (lr.withY (58.0f).withHeight (26.0f), 3.0f);
        g.setColour (col::text); text (g, "+", lr.withHeight (26.0f), juce::Justification::centred, 18.0f, true); text (g, "-", lr.withY (58.0f).withHeight (26.0f), juce::Justification::centred, 18.0f, true);
        text (g, juce::String (n), lr.withY (26.0f).withHeight (32.0f), juce::Justification::centred, 16.0f, true);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        auto s = pat.steps.removeCharacters (" "); auto acc = pat.accents.removeCharacters (" ");
        while (acc.length() < s.length()) acc << "0";
        const int n = count();
        if (e.position.x > (float) getWidth() - 44.0f)
        {
            const int len = juce::jlimit (1, 32, n + (e.position.y < 29.0f ? 1 : e.position.y > 55.0f ? -1 : 0));
            while (s.length() < len) { s << "."; acc << "0"; }
            s = s.substring (0, len); acc = acc.substring (0, len);
        }
        else
            for (int i = 0; i < n; ++i)
                if (cell (i).contains (e.position))
                {
                    if (e.mods.isPopupMenu())
                        acc = acc.replaceSection (i, 1, juce::String ((acc[i] - '0' + 1) % 3));
                    else
                    {
                        static const juce::String cycle = "DUduMmXxBAS|.";
                        int k = std::max (0, cycle.indexOfChar (s[i]));
                        k = (k + (e.mods.isShiftDown() ? cycle.length() - 1 : 1)) % cycle.length();
                        s = s.replaceSection (i, 1, juce::String::charToString (cycle[k]));
                    }
                }
        pat.steps = s; pat.accents = acc; pat.name = "Edited";
        proc.setStrum (pat);
        repaint();
    }
    static juce::String stepWord (juce::juce_wchar c)
    {
        switch (c)
        {
            case 'D': return "down"; case 'U': return "up"; case 'd': return "high dn"; case 'u': return "high up"; case 'M': return "palm dn"; case 'm': return "palm up";
            case 'X': return "dead dn"; case 'x': return "dead up"; case 'B': return "bass"; case 'A': return "alt bass"; case 'S': return "stab"; case '|': return "stop";
            default: return "";
        }
    }
private:
    ThrumProcessor& proc; ThrumProcessor::PatternEntry pat;
};

// La división y el swing del patrón (son parte del patrón, no parámetros).
class PatternDiv : public juce::Component
{
public:
    PatternDiv (ThrumProcessor& p, bool strum) : proc (p), isStrum (strum) {}
    void paint (juce::Graphics& g) override
    {
        const auto e = isStrum ? proc.getStrum() : proc.getArp();
        for (int i = 0; i < dh::gt::kNumDivs; ++i)
        {
            auto r = cellR (i); const bool on = i == e.div;
            g.setColour (on ? col::amber : col::panel); g.fillRoundedRectangle (r, 4.0f);
            if (! on) { g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f); }
            g.setColour (on ? col::ink : col::text); text (g, dh::gt::divisionName (i), r, juce::Justification::centred, 13.5f, on);
        }
        auto sr = swingR();
        g.setColour (col::bg); g.fillRoundedRectangle (sr, 4.0f); g.setColour (col::line); g.drawRoundedRectangle (sr.reduced (0.5f), 4.0f, 1.0f);
        g.setColour (col::text); text (g, "SWING " + juce::String (juce::roundToInt (e.swing)) + " %", sr, juce::Justification::centred, 13.5f, false, 0.04f);
    }
    float cw() const { return std::min (56.0f, ((float) getWidth() - 120.0f) / 5.0f); }
    juce::Rectangle<float> cellR (int i) const { return { (float) i * cw(), 0.0f, cw() - 5.0f, (float) getHeight() }; }
    juce::Rectangle<float> swingR() const { return { 5.0f * cw() + 4.0f, 0.0f, (float) getWidth() - 5.0f * cw() - 4.0f, (float) getHeight() }; }
    void mouseDown (const juce::MouseEvent& e) override
    {
        auto p = isStrum ? proc.getStrum() : proc.getArp();
        for (int i = 0; i < dh::gt::kNumDivs; ++i) if (cellR (i).contains (e.position)) { p.div = i; store (p); return; }
        if (swingR().contains (e.position)) { dragStart = p.swing; dragY = e.position.y; }
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! swingR().contains (e.mouseDownPosition)) return;
        auto p = isStrum ? proc.getStrum() : proc.getArp();
        p.swing = juce::jlimit (50.0f, 75.0f, (float) juce::roundToInt (dragStart + (e.position.x - e.mouseDownPosition.x) * 0.1f - (e.position.y - dragY) * 0.1f));
        store (p);
    }
    void mouseDoubleClick (const juce::MouseEvent& e) override { if (swingR().contains (e.position)) { auto p = isStrum ? proc.getStrum() : proc.getArp(); p.swing = 50; store (p); } }
private:
    ThrumProcessor& proc; bool isStrum; float dragStart = 50, dragY = 0;
    void store (const ThrumProcessor::PatternEntry& p) { if (isStrum) proc.setStrum (p); else proc.setArp (p); repaint(); }
};

class PlayPage : public Page
{
public:
    explicit PlayPage (ThrumProcessor& p) : Page (p), grid (add<StrumGrid> (p)), strumDiv (add<PatternDiv> (p, true)), arpDiv (add<PatternDiv> (p, false))
    {
        mode = &add<Chips> (proc, "p_mode", juce::StringArray { "Melody poly", "Melody mono", "Chords", "Strum", "Fingerpick" }); mode->fontH = 12.5f;
        art = &add<Chips> (proc, "p_art", juce::StringArray { "Sustain", "Palm mute", "Staccato", "Dead", "Harmonics", "Pinch", "Tremolo", "Bend in", "Slide in", "Slide out" }, 5); art->fontH = 12.5f;
        legato = &add<Chips> (proc, "p_legato", juce::StringArray { "Off", "Hammer / pull", "Slides", "Auto" }); legato->fontH = 12.5f;
        palm = &knob ("p_palm", "Palm mute"); stac = &knob ("p_staccato", "Staccato"); reach = &knob ("p_legato_frets", "Legato reach"); slide = &knob ("p_slide_time", "Slide time");
        tremDiv = &add<Menu> (proc, "p_trem_div", "Tremolo");
        dead = &knob ("p_dead_below", "Dead below vel"); pinch = &knob ("p_pinch_above", "Pinch above vel"); idle = &knob ("p_idle", "Idle mute");
        whammy = &knob ("p_whammy", "Whammy range"); feedback = &knob ("p_feedback", "Feedback"); fbPitch = &add<Menu> (proc, "p_fb_harm", "Feedback pitch");
        style = &add<Chips> (proc, "c_style"); style->fontH = 12.5f;
        single = &add<Chips> (proc, "c_single", juce::StringArray { "Major", "Power", "Note" }); single->fontH = 12.5f;
        anchor = &add<Chips> (proc, "c_anchor", juce::StringArray { "Centred", "On the beat" }); anchor->fontH = 12.5f;
        anchor->setTooltip ("Centred: the middle strings land on the beat, as guitarists keep time. On the beat: the first string lands on it.");
        region = &knob ("c_region", "Region"); speed = &knob ("c_strum", "Strum speed"); ups = &knob ("c_up", "Upstrokes"); window = &knob ("c_window", "Chord window");
        latch = &add<Toggle> (proc, "c_latch", "Latch"); ring = &add<Toggle> (proc, "c_letring", "Let ring");
        strumMenu = &add<Menu> (proc, "s_pattern"); arpMenu = &add<Menu> (proc, "a_pattern");
        strumMenu->build = [this] (juce::PopupMenu& m) { libraryMenu (m, ThrumProcessor::strumLibrary(), (int) proc.value ("s_pattern")); };
        arpMenu->build = [this] (juce::PopupMenu& m) { libraryMenu (m, ThrumProcessor::arpLibrary(), (int) proc.value ("a_pattern")); };
        addAndMakeVisible (arpText);
        arpText.setFont (font (16.0f)); arpText.setJustification (juce::Justification::centredLeft); arpText.setIndents (8, 4);
        arpText.setTooltip ("T bass, A alternate bass, 1-4 strings from the top, T1 together, . rest, > accent; one group per step.");
        arpText.onReturnKey = [this] { applyArp(); };
        arpText.onFocusLost = [this] { applyArp(); };
        expPat = &add<Button> ("Export pattern"); expPlayed = &add<Button> ("Export played"); clearRec = &add<Button> ("Clear");
        expPat->setTooltip ("Saves the current pattern as a .mid file, played over the chord you are holding (or C major).");
        expPlayed->setTooltip ("Saves everything Thrum played since the last Clear as a .mid file.");
        expPat->onClick = [this] { exportTo (true); };
        expPlayed->onClick = [this] { exportTo (false); };
        clearRec->onClick = [this] { proc.clearRecording(); };
        autoVib = &add<Toggle> (proc, "p_vib_auto", "Auto vibrato"); bendAll = &add<Toggle> (proc, "p_bend_all", "Bend every string");
        vd = &knob ("p_vib_delay", "Vib delay"); vr = &knob ("p_vib_rate", "Vib rate"); vdep = &knob ("p_vib_depth", "Vib depth");
        br = &knob ("p_bend_range", "Bend range"); bi = &knob ("p_bendin", "Bend-in"); bit = &knob ("p_bendin_time", "Bend-in time");
        hand = &knob ("f_position", "Hand position"); pref = &knob ("f_string", "Preferred string"); open = &knob ("f_open", "Open strings"); low = &knob ("f_low", "Low positions");
        fingers = &add<Toggle> (proc, "p_fingers", "Fingers instead of pick");
        ppos = &knob ("p_pick_pos", "Pick position"); phard = &knob ("p_pick_hard", "Hardness"); pang = &knob ("p_pick_angle", "Angle");
        ht = &knob ("h_tune", "Tuning"); htm = &knob ("h_time", "Timing"); hv = &knob ("h_vel", "Velocity"); hp = &knob ("h_pick", "Pick");
        refreshPatterns();
    }
    void refreshPatterns() { grid.refresh(); strumDiv.repaint(); arpDiv.repaint(); if (! arpText.hasKeyboardFocus (true)) arpText.setText (proc.getArp().steps, false); }
    void tick() override
    {
        const int rc = proc.recordedEvents();
        if (rc != shownRec) { shownRec = rc; repaint (446, 514, 436, 26); }
    }
    void resized() override
    {
        // Columna 1: cómo se toca.
        int x = 16;
        mode->setBounds (x, 40, 404, 28);
        art->setBounds (x, 108, 404, 62);
        legato->setBounds (x, 210, 404, 28);
        row ({ palm, stac, reach, slide }, x - 6, 254, 82, 78); tremDiv->setBounds (x + 328, 268, 82, 46);
        row ({ dead, pinch, idle }, x - 6, 374, 82, 78);
        row ({ whammy, feedback }, x - 6, 494, 82, 78); fbPitch->setBounds (x + 168, 508, 180, 46);
        // Columna 2: acordes y patrones.
        x = 462;
        style->setBounds (x, 40, 404, 28);
        single->setBounds (x, 78, 196, 26); anchor->setBounds (x + 208, 78, 196, 26);
        row ({ region, speed, ups, window }, x - 6, 110, 76, 82);
        latch->setBounds (x + 302, 116, 102, 30); ring->setBounds (x + 302, 154, 102, 30);
        strumMenu->setBounds (x, 226, 404, 30);
        strumDiv.setBounds (x, 262, 404, 26);
        grid.setBounds (x, 294, 404, 84);
        arpMenu->setBounds (x, 424, 196, 30); arpText.setBounds (x + 204, 424, 200, 30);
        arpDiv.setBounds (x, 460, 404, 26);
        expPat->setBounds (x, 544, 140, 30); expPlayed->setBounds (x + 146, 544, 140, 30); clearRec->setBounds (x + 292, 544, 112, 30);
        // Columna 3: expresión y manos.
        x = 908;
        autoVib->setBounds (x, 40, 190, 28); bendAll->setBounds (x + 200, 40, 204, 28);
        row ({ vd, vr, vdep, br, bi, bit }, x - 6, 76, 69, 78);
        row ({ hand, pref, open, low }, x - 6, 194, 82, 78);
        fingers->setBounds (x, 310, 240, 28);
        row ({ ppos, phard, pang }, x - 6, 344, 82, 78);
        row ({ ht, htm, hv, hp }, x - 6, 464, 82, 78);
    }
    void paint (juce::Graphics& g) override
    {
        panelBox (g, { 0, 0, 436, 584 }); panelBox (g, { 446, 0, 436, 584 }); panelBox (g, { 892, 0, 436, 584 });
        section (g, "Play mode", 16, 12); section (g, "Articulation", 16, 80); section (g, "Legato", 16, 182);
        section (g, "Dynamics", 16, 346); section (g, "Whammy and feedback", 16, 466);
        section (g, "Chords", 462, 12); section (g, "Strum pattern", 462, 200); section (g, "Picking pattern", 462, 398); section (g, "MIDI files", 462, 494);
        label (g, juce::String (proc.recordedEvents()) + " notes recorded since the last clear", { 462, 518, 404, 18 }, juce::Justification::centredLeft, 12.5f);
        section (g, "Vibrato and bends", 908, 12); section (g, "Fingering", 908, 166); section (g, "Picking hand", 908, 284); section (g, "Human variation", 908, 436);
        label (g, "Click: next stroke, shift-click: previous, right-click: accent. + and - change the length.", { 462, 380, 404, 16 }, juce::Justification::centredLeft, 11.5f, col::faint);
        if (inspect)
        {
            // Inspect: la anotación del patrón en texto, tal como la guarda el preset.
            const auto s = proc.getStrum();
            g.setColour (col::cyan);
            g.setColour (col::panel); g.fillRect (462, 378, 404, 20);
            g.setColour (col::cyan);
            text (g, "STEPS " + s.steps + "   ACCENTS " + s.accents + "   DIV " + dh::gt::divisionName (s.div), { 462, 380, 404, 16 }, juce::Justification::centredLeft, 12.0f);
        }
    }
private:
    StrumGrid& grid; PatternDiv& strumDiv; PatternDiv& arpDiv;
    Chips *mode, *art, *legato, *style, *single, *anchor; Knob *palm, *stac, *reach, *slide, *dead, *pinch, *idle, *whammy, *feedback;
    Menu *tremDiv, *fbPitch, *strumMenu, *arpMenu;
    Knob *region, *speed, *ups, *window; Toggle *latch, *ring;
    juce::TextEditor arpText; Button *expPat, *expPlayed, *clearRec;
    Toggle *autoVib, *bendAll, *fingers; Knob *vd, *vr, *vdep, *br, *bi, *bit, *hand, *pref, *open, *low, *ppos, *phard, *pang, *ht, *htm, *hv, *hp;
    std::unique_ptr<juce::FileChooser> chooser; int shownRec = -1;

    static void libraryMenu (juce::PopupMenu& m, const std::vector<ThrumProcessor::PatternEntry>& lib, int cur)
    {
        // Agrupados por estilo.
        juce::StringArray genres; for (auto& e : lib) genres.addIfNotAlreadyThere (e.genre);
        for (auto& gname : genres)
        {
            juce::PopupMenu sub;
            for (size_t i = 0; i < lib.size(); ++i) if (lib[i].genre == gname) sub.addItem ((int) i + 1, lib[i].name, true, (int) i == cur);
            m.addSubMenu (gname, sub);
        }
    }
    void applyArp()
    {
        auto a = proc.getArp();
        if (arpText.getText().trim() == a.steps.trim()) return;
        a.steps = arpText.getText().trim(); a.name = "Edited"; proc.setArp (a);
    }
    void exportTo (bool pattern)
    {
        chooser = std::make_unique<juce::FileChooser> (pattern ? "Export the pattern as MIDI" : "Export what was played as MIDI",
                                                       juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile (pattern ? "thrum pattern.mid" : "thrum played.mid"), "*.mid");
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting,
                              [this, pattern] (const juce::FileChooser& fc) {
                                  const auto f = fc.getResult(); if (f == juce::File()) return;
                                  if (pattern) proc.exportPattern (f.withFileExtension (".mid"), 4); else proc.exportPlayed (f.withFileExtension (".mid"));
                              });
    }
};

// ───────── Pedals ─────────
// La pedalera: las quince unidades en el orden de la cadena, en dos filas (de izquierda a derecha, la de arriba primero).
// Clic en el pulsador (el tercio de abajo): encender o apagar. Clic en el resto: elegirlo (sus controles van abajo).
// Arrastrar: moverlo en la cadena. El amplificador y el gabinete son placas propias.
class Board : public juce::Component
{
public:
    explicit Board (ThrumProcessor& p) : proc (p) {}
    std::function<void (int)> onSelect;
    int selected = dh::gt::kOd;
    bool inspect = false;
    static constexpr int kRow1 = 8;
    juce::Rectangle<float> slot (int k) const
    {
        const float w = (float) getWidth() / 8.0f;
        return k < kRow1 ? juce::Rectangle<float> ((float) k * w, 8.0f, w, 172.0f) : juce::Rectangle<float> ((float) (k - kRow1) * w + w * 0.5f, 186.0f, w, 172.0f);
    }
    static juce::String pedalFile (int u) { static const char* f[] = { "gate", "comp", "wah", "od", "dist", "fuzz", "chorus", "phaser", "flanger", "trem", "vib", "delay", "reverb" }; return "pedal_" + juce::String (f[u]) + ".png"; }
    static juce::String shortName (int u)
    {
        static const char* n[] = { "Gate", "Comp", "Wah", "Green drive", "Distortion", "Fuzz", "Chorus", "Phaser", "Flanger", "Tremolo", "Vibrato", "Delay", "Reverb", "Amp", "Cabinet" };
        return n[juce::jlimit (0, (int) dh::gt::kNumUnits - 1, u)];
    }
    bool isOn (int u) const
    {
        if (u == dh::gt::kUnitAmp) return proc.value ("amp_on") > 0.5f;
        if (u == dh::gt::kUnitCab) return proc.value ("cab_on") > 0.5f;
        return proc.value (dh::gt::params::pedalIds (u)[0]) > 0.5f;
    }
    juce::String onId (int u) const { return u == dh::gt::kUnitAmp ? "amp_on" : u == dh::gt::kUnitCab ? "cab_on" : dh::gt::params::pedalIds (u)[0]; }

    void paint (juce::Graphics& g) override
    {
        auto bg = asset ("pedalboard.jpg");
        auto rr = getLocalBounds().toFloat();
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip; clip.addRoundedRectangle (rr, 8.0f); g.reduceClipRegion (clip);
            if (bg.isValid()) g.drawImage (bg, rr, juce::RectanglePlacement::fillDestination);
            else g.fillAll (col::panel);
        }
        const auto order = proc.getOrder();
        for (int k = 0; k < dh::gt::kNumUnits; ++k)
        {
            const int u = order[(size_t) k];
            if (dragging && u == dragUnit) continue;
            drawUnit (g, u, slot (displayIndex (k)), false);
        }
        if (dragging)
        {
            auto r = slot (0).withCentre (dragPos);
            drawUnit (g, dragUnit, r, true);
            auto ins = slot (insertAt);
            g.setColour (col::amber); g.fillRoundedRectangle (ins.getX() - 2.0f, ins.getY() + 10.0f, 4.0f, ins.getHeight() - 20.0f, 2.0f);
        }
        if (inspect)
        {
            // Inspect: el camino de la señal y dónde se vuelve estéreo.
            juce::String s = "IN";
            for (int k = 0; k < dh::gt::kNumUnits; ++k) { const int u = order[(size_t) k]; if (isOn (u)) s << "  >  " << shortName (u).toUpperCase(); }
            s << "  >  OUT";
            g.setColour (col::ink.withAlpha (0.7f)); g.fillRoundedRectangle (8.0f, rr.getBottom() - 30.0f, rr.getWidth() - 16.0f, 22.0f, 4.0f);
            g.setColour (col::cyan); text (g, s, { 16.0f, rr.getBottom() - 30.0f, rr.getWidth() - 32.0f, 22.0f }, juce::Justification::centredLeft, 13.0f, true);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        downUnit = unitAt (e.position, downSlot);
        dragging = false;
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (downUnit < 0) return;
        if (! dragging && e.getDistanceFromDragStart() > 6) { dragging = true; dragUnit = downUnit; }
        if (! dragging) return;
        dragPos = e.position;
        // El lugar donde caería: el casillero más cercano.
        int best = 0; float bd = 1e9f;
        for (int k = 0; k < dh::gt::kNumUnits; ++k) { const float d = slot (k).getCentre().getDistanceFrom (e.position); if (d < bd) { bd = d; best = k; } }
        insertAt = best;
        repaint();
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragging)
        {
            auto o = proc.getOrder();
            std::vector<int> v (o.begin(), o.end());
            v.erase (std::find (v.begin(), v.end(), dragUnit));
            v.insert (v.begin() + std::min ((int) v.size(), insertAt), dragUnit);
            for (int k = 0; k < dh::gt::kNumUnits; ++k) o[(size_t) k] = v[(size_t) k];
            proc.setOrder (o);
            dragging = false; selected = dragUnit; if (onSelect) onSelect (selected);
            repaint(); return;
        }
        if (downUnit < 0) return;
        auto r = slot (downSlot);
        const bool footswitch = e.position.y > r.getY() + r.getHeight() * 0.48f && e.position.y < r.getBottom() - 22.0f;
        if (footswitch || e.getNumberOfClicks() > 1)
        {
            if (auto* p = proc.apvts.getParameter (onId (downUnit))) { p->beginChangeGesture(); p->setValueNotifyingHost (isOn (downUnit) ? 0.0f : 1.0f); p->endChangeGesture(); }
        }
        selected = downUnit; if (onSelect) onSelect (selected);
        repaint();
    }
private:
    ThrumProcessor& proc;
    int downUnit = -1, downSlot = -1, dragUnit = -1, insertAt = 0; bool dragging = false; juce::Point<float> dragPos;
    int displayIndex (int k) const { return k; }
    int unitAt (juce::Point<float> p, int& slotIdx) const
    {
        const auto order = proc.getOrder();
        for (int k = 0; k < dh::gt::kNumUnits; ++k) if (slot (k).contains (p)) { slotIdx = k; return order[(size_t) k]; }
        slotIdx = -1; return -1;
    }
    void drawUnit (juce::Graphics& g, int u, juce::Rectangle<float> r, bool lifted)
    {
        const bool on = isOn (u), sel = u == selected;
        auto body = r.withTrimmedBottom (24.0f).reduced (6.0f, 2.0f);
        if (sel && ! lifted) { g.setColour (col::amber.withAlpha (0.10f)); g.fillRoundedRectangle (r.reduced (3.0f), 8.0f); g.setColour (col::amber.withAlpha (0.55f)); g.drawRoundedRectangle (r.reduced (3.0f), 8.0f, 1.2f); }
        if (u < dh::gt::kNumPedals)
        {
            auto im = asset (pedalFile (u));
            if (im.isValid())
            {
                auto ir = body.withSizeKeepingCentre (body.getHeight() * (float) im.getWidth() / (float) im.getHeight(), body.getHeight());
                if (lifted) ir = ir.translated (0.0f, -6.0f);
                g.setOpacity (lifted ? 0.92f : 1.0f);
                g.drawImage (im, ir, juce::RectanglePlacement::stretchToFit);
                g.setOpacity (1.0f);
                // El LED: proyección del render.
                const auto& L = layout()["pedal"][u];
                if (L.isObject() && on)
                {
                    auto lp = lpt (L["led"]);
                    juce::Point<float> c (ir.getX() + lp.x * ir.getWidth(), ir.getY() + lp.y * ir.getHeight());
                    g.setGradientFill (juce::ColourGradient (col::red.withAlpha (0.85f), c, col::red.withAlpha (0.0f), c.translated (12.0f, 0.0f), true));
                    g.fillEllipse (c.x - 12.0f, c.y - 12.0f, 24.0f, 24.0f);
                    g.setColour (juce::Colour (0xffffd0b0)); g.fillEllipse (c.x - 2.2f, c.y - 2.2f, 4.4f, 4.4f);
                }
            }
        }
        else
        {
            // El amplificador y el gabinete: placas negras con su luz.
            auto pl = body.withSizeKeepingCentre (std::min (body.getWidth(), 112.0f), 92.0f);
            g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRoundedRectangle (pl.translated (2.0f, 4.0f), 8.0f);
            g.setColour (juce::Colour (0xff181614)); g.fillRoundedRectangle (pl, 8.0f);
            g.setColour (u == dh::gt::kUnitAmp ? juce::Colour (0xff8a6a35) : col::lineHi); g.drawRoundedRectangle (pl.reduced (4.0f), 6.0f, 1.5f);
            if (u == dh::gt::kUnitCab)
            {
                g.setColour (col::cell2); g.fillEllipse (pl.getCentreX() - 26.0f, pl.getCentreY() - 30.0f, 52.0f, 52.0f);
                g.setColour (col::bg); g.fillEllipse (pl.getCentreX() - 9.0f, pl.getCentreY() - 13.0f, 18.0f, 18.0f);
            }
            else
            {
                for (int i = 0; i < 5; ++i) { g.setColour (col::dim); g.fillEllipse (pl.getX() + 16.0f + (float) i * 17.0f, pl.getY() + 20.0f, 9.0f, 9.0f); }
            }
            g.setColour (on ? col::red : col::faint); g.fillEllipse (pl.getRight() - 18.0f, pl.getBottom() - 18.0f, 8.0f, 8.0f);
            g.setColour (col::text); text (g, u == dh::gt::kUnitAmp ? "AMP" : "", pl.withTrimmedTop (44.0f), juce::Justification::centred, 18.0f, true, 0.1f);
        }
        // El rótulo, como una cinta debajo.
        auto tape = juce::Rectangle<float> (r.getX() + 14.0f, r.getBottom() - 22.0f, r.getWidth() - 28.0f, 18.0f);
        g.setColour (on ? col::amber : juce::Colour (0xffd9cfbf).withAlpha (0.85f)); g.fillRoundedRectangle (tape, 2.0f);
        g.setColour (col::ink); text (g, shortName (u).toUpperCase(), tape, juce::Justification::centred, 13.0f, true, 0.06f);
    }
};

class PedalsPage : public Page
{
public:
    explicit PedalsPage (ThrumProcessor& p) : Page (p), board (add<Board> (p))
    {
        board.onSelect = [this] (int u) { show (u); };
        show (board.selected);
    }
    void setInspect (bool on) override { Page::setInspect (on); board.inspect = on; board.repaint(); }
    void resized() override { board.setBounds (0, 0, 1328, 392); layoutControls(); }
    void tick() override
    {
        // El orden o los interruptores pueden cambiar desde el host o un preset.
        const auto o = proc.getOrder(); uint64_t sig = 0;
        for (int u : o) sig = sig * 17 + (uint64_t) u;
        for (int u = 0; u < dh::gt::kNumUnits; ++u) sig = sig * 3 + (board.isOn (u) ? 1u : 0u);
        if (sig != lastSig) { lastSig = sig; board.repaint(); }
        if (shown == dh::gt::kDelay && inspect) repaint (0, 400, 1328, 184);
    }
    void paint (juce::Graphics& g) override
    {
        panelBox (g, { 0, 402, 1328, 182 });
        section (g, dh::gt::unitName (shown), 20, 412, 400);
        if (shown >= dh::gt::kNumPedals)
        {
            label (g, "The " + juce::String (shown == dh::gt::kUnitAmp ? "amplifier" : "cabinet") + " is set on the Amp / Cab page. Drag it here to change where it sits in the chain.",
                   { 20, 450, 900, 20 }, juce::Justification::centredLeft, 14.0f, col::text);
        }
        if (inspect && shown == dh::gt::kDelay) paintEchoes (g, { 1010, 416, 300, 156 });
    }
    std::function<void()> gotoAmp;
    void select (int u) { board.selected = juce::jlimit (0, (int) dh::gt::kNumUnits - 1, u); board.repaint(); show (board.selected); }
private:
    Board& board;
    int shown = -1; uint64_t lastSig = 0;
    std::vector<std::unique_ptr<juce::Component>> ctrls;
    void show (int u)
    {
        shown = u;
        for (auto& c : ctrls) removeChildComponent (c.get());
        ctrls.clear();
        if (u < dh::gt::kNumPedals)
        {
            const auto ids = dh::gt::params::pedalIds (u);
            for (auto& id : ids)
            {
                auto* prm = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id));
                if (prm == nullptr) continue;
                std::unique_ptr<juce::Component> c;
                if (dynamic_cast<juce::AudioParameterChoice*> (prm)) c = std::make_unique<Chips> (proc, id);
                else if (dynamic_cast<juce::AudioParameterBool*> (prm))
                {
                    auto name = prm->getName (40);
                    if (id.endsWith ("_on")) name = "On";
                    else name = name.fromFirstOccurrenceOf (" ", false, false).isNotEmpty() ? name.fromFirstOccurrenceOf (" ", false, false) : name;
                    c = std::make_unique<Toggle> (proc, id, name);
                }
                else
                {
                    // El nombre sin el del pedal ("Delay time" → "Time").
                    auto name = prm->getName (40);
                    for (auto pre : { "Overdrive ", "Distortion ", "Fuzz ", "Chorus ", "Phaser ", "Flanger ", "Tremolo ", "Vibrato pedal ", "Delay ", "Reverb ", "Wah ", "Comp ", "Gate " })
                        if (name.startsWith (pre)) name = name.fromFirstOccurrenceOf (pre, false, false);
                    if (id == "dly_div") name = "Sync division";
                    auto k = std::make_unique<Knob> (proc, id, name.substring (0, 1).toUpperCase() + name.substring (1));
                    // El tiempo del delay en ms y la división por su nombre (las mismas cuentas que Pedals.h).
                    if (id == "dly_time") k->format = [] (float val) { const float q = val * 0.01f; return juce::String (juce::roundToInt (40.0f + 1460.0f * q * q)) + " ms"; };
                    if (id == "dly_div") k->format = [] (float v) { return juce::String (dh::gt::delayDivName (v * 0.01f)); };
                    c = std::move (k);
                }
                addAndMakeVisible (*c); ctrls.push_back (std::move (c));
            }
        }
        else
        {
            auto b = std::make_unique<Button> ("Open Amp / Cab");
            b->onClick = [this] { if (gotoAmp) gotoAmp(); };
            addAndMakeVisible (*b); ctrls.push_back (std::move (b));
            auto t = std::make_unique<Toggle> (proc, shown == dh::gt::kUnitAmp ? "amp_on" : "cab_on", "On");
            addAndMakeVisible (*t); ctrls.push_back (std::move (t));
        }
        layoutControls();
        repaint();
    }
    void layoutControls()
    {
        int xk = 300, xc = 20, yc = 444;
        for (auto& c : ctrls)
        {
            if (auto* ch = dynamic_cast<Chips*> (c.get())) { const int w = std::max (180, ch->labels.size() * 92); ch->setBounds (xc, yc, std::min (260, w), 30); yc += 40; }
            else if (dynamic_cast<Toggle*> (c.get())) { c->setBounds (xc, yc, 120, 30); yc += 40; }
            else if (dynamic_cast<Button*> (c.get())) { c->setBounds (xc, 486, 180, 32); }
            else { c->setBounds (xk, 440, 100, 104); xk += 106; }
        }
    }
    void paintEchoes (juce::Graphics& g, juce::Rectangle<float> r)
    {
        // Las repeticiones del delay (realimentación y tiempo del motor: Pedals.h).
        const float fb = std::min (0.95f, proc.value ("dly_fb") * 0.01f * 1.05f), mix = proc.value ("dly_mix") * 0.01f;
        const bool sync = proc.value ("dly_sync") > 0.5f;
        const double bpm = proc.value ("s_tempo");
        const double ms = sync ? dh::gt::delayDivBeats (proc.value ("dly_div") * 0.01f) * 60000.0 / bpm : 40.0 + 1460.0 * std::pow (proc.value ("dly_time") * 0.01, 2.0);
        g.setColour (col::cyanDim); g.drawRect (r, 1.0f);
        const int n = 10; float a = 1.0f;
        for (int k = 0; k < n; ++k)
        {
            const float x = r.getX() + 10.0f + (r.getWidth() - 20.0f) * (float) k / (float) (n - 1);
            const float h = (r.getHeight() - 36.0f) * (k == 0 ? 1.0f : a * mix * 1.6f);
            g.setColour (k == 0 ? col::text : col::cyan); g.fillRect (x - 2.0f, r.getBottom() - 22.0f - std::min (h, r.getHeight() - 36.0f), 4.0f, std::min (h, r.getHeight() - 36.0f));
            if (k > 0) a *= fb;
        }
        g.setColour (col::cyan);
        text (g, "ECHOES  " + juce::String (juce::roundToInt (ms)) + " ms APART" + (sync ? " AT " + juce::String (juce::roundToInt (bpm)) + " BPM" : "") + "  /  FEEDBACK " + juce::String (fb, 2),
              r.removeFromBottom (18.0f), juce::Justification::centredLeft, 12.0f);
    }
};

// ───────── Amp / Cab ─────────
// La vista de arriba del parlante para mover los micrófonos: el ancho es la posición (centro del cono a la izquierda,
// borde a la derecha) y la altura la distancia (de 1 a 60 cm).
class MicPad : public juce::Component
{
public:
    explicit MicPad (ThrumProcessor& p) : proc (p) {}
    juce::Rectangle<float> area() const { return getLocalBounds().toFloat().reduced (14.0f, 10.0f).withTrimmedTop (34.0f); }
    juce::Point<float> where (bool b) const
    {
        auto a = area();
        const float pos = proc.value (b ? "cab_pos_b" : "cab_pos_a") * 0.01f, d = proc.value (b ? "cab_dist_b" : "cab_dist_a");
        return { a.getX() + a.getWidth() * pos, a.getY() + a.getHeight() * std::sqrt ((d - 1.0f) / 59.0f) };
    }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (col::bg); g.fillRoundedRectangle (r, 6.0f); g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
        auto a = area();
        // El cono visto de arriba: el domo a la izquierda, el borde a la derecha.
        juce::Path cone; cone.startNewSubPath (a.getX(), r.getY() + 20.0f);
        cone.quadraticTo (a.getX() + a.getWidth() * 0.5f, r.getY() + 34.0f, a.getRight(), r.getY() + 36.0f);
        g.setColour (col::dim); g.strokePath (cone, juce::PathStrokeType (2.0f));
        g.fillEllipse (a.getX() - 10.0f, r.getY() + 12.0f, 20.0f, 14.0f);
        label (g, "Cap", { a.getX() + 10.0f, r.getY() + 2.0f, 60.0f, 14.0f }, juce::Justification::centredLeft, 11.0f);
        label (g, "Edge", { a.getRight() - 60.0f, r.getY() + 2.0f, 60.0f, 14.0f }, juce::Justification::centredRight, 11.0f);
        for (int cm : { 5, 15, 30, 60 })
        {
            const float y = a.getY() + a.getHeight() * std::sqrt ((float) (cm - 1) / 59.0f);
            g.setColour (col::line); g.drawHorizontalLine ((int) y, a.getX(), a.getRight());
            label (g, juce::String (cm) + " cm", { a.getRight() - 50.0f, y - 14.0f, 50.0f, 13.0f }, juce::Justification::centredRight, 10.5f, col::faint);
        }
        const bool showB = proc.value ("cab_mix") > 0.5f;
        for (int k = showB ? 1 : 0; k >= 0; --k)
        {
            auto p = where (k == 1);
            g.setColour ((k == 1 ? col::red : col::amber)); g.fillEllipse (p.x - 12.0f, p.y - 12.0f, 24.0f, 24.0f);
            g.setColour (col::ink); text (g, k == 1 ? "B" : "A", { p.x - 12.0f, p.y - 12.0f, 24.0f, 24.0f }, juce::Justification::centred, 15.0f, true);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const bool showB = proc.value ("cab_mix") > 0.5f;
        dragB = showB && where (true).getDistanceFrom (e.position) < where (false).getDistanceFrom (e.position);
        for (auto id : ids()) if (auto* p = proc.apvts.getParameter (id)) p->beginChangeGesture();
        mouseDrag (e);
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        auto a = area();
        const float pos = juce::jlimit (0.0f, 1.0f, (e.position.x - a.getX()) / a.getWidth());
        const float yy = juce::jlimit (0.0f, 1.0f, (e.position.y - a.getY()) / a.getHeight());
        set (dragB ? "cab_pos_b" : "cab_pos_a", pos * 100.0f);
        set (dragB ? "cab_dist_b" : "cab_dist_a", 1.0f + 59.0f * yy * yy);
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { for (auto id : ids()) if (auto* p = proc.apvts.getParameter (id)) p->endChangeGesture(); }
private:
    ThrumProcessor& proc; bool dragB = false;
    juce::StringArray ids() const { return dragB ? juce::StringArray { "cab_pos_b", "cab_dist_b" } : juce::StringArray { "cab_pos_a", "cab_dist_a" }; }
    void set (const char* id, float v) { if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (proc.apvts.getParameter (id))) p->setValueNotifyingHost (p->convertTo0to1 (v)); }
};

class AmpPage : public Page
{
public:
    explicit AmpPage (ThrumProcessor& p) : Page (p), pad (add<MicPad> (p))
    {
        model = &add<Chips> (proc, "amp_model", juce::StringArray { "American clean", "British crunch", "Modern high gain", "Small combo" }, 2); model->fontH = 13.0f;
        for (auto id : { "amp_gain", "amp_bass", "amp_mid", "amp_treble", "amp_presence", "amp_depth", "amp_master", "amp_sag" }) amp.push_back (&knob (id, {}, false));
        level = &knob ("amp_level", "Level");
        ampOn = &add<Toggle> (proc, "amp_on", "Amp"); bright = &add<Toggle> (proc, "amp_bright", "Bright"); autoLvl = &add<Toggle> (proc, "amp_auto", "Auto level");
        autoLvl->setTooltip ("On: Gain changes the character but not much the volume. Off: Gain also raises the volume, like a real amplifier.");
        quality = &add<Chips> (proc, "amp_quality");
        cabOn = &add<Toggle> (proc, "cab_on", "Cabinet"); source = &add<Chips> (proc, "cab_source"); source->fontH = 13.0f;
        speaker = &add<Menu> (proc, "cab_speaker", "Speaker"); box = &add<Menu> (proc, "cab_box", "Cabinet");
        micA = &add<Chips> (proc, "cab_mic_a"); micB = &add<Chips> (proc, "cab_mic_b"); micA->fontH = micB->fontH = 12.5f;
        mix = &add<HSlider> (proc, "cab_mix", "Mic mix A / B"); room = &add<HSlider> (proc, "cab_room", "Room");
        factory = &add<Menu> (proc, "cab_ir", "Factory IR");
        load = &add<Button> ("Load IR file...");
        load->onClick = [this] {
            chooser = std::make_unique<juce::FileChooser> ("Load an impulse response (up to 3 s)", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
            chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc) {
                const auto f = fc.getResult(); if (! f.existsAsFile()) return;
                juce::String err; if (! proc.loadUserIr (f, err)) juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Thrum", err);
                repaint();
            });
        };
        tick();
    }
    void resized() override
    {
        const int x = 732;
        model->setBounds (x, 36, 400, 62);
        ampOn->setBounds (x + 412, 36, 160, 28); autoLvl->setBounds (x + 412, 70, 160, 28);
        for (size_t i = 0; i < amp.size(); ++i) amp[i]->setBounds (x - 8 + (int) (i % 4) * 82 + (i >= 4 ? 0 : 0), 108 + (int) (i / 4) * 92, 82, 90);
        level->setBounds (x + 324, 108, 82, 90);
        bright->setBounds (x + 412, 120, 160, 28); quality->setBounds (x + 412, 162, 160, 28);
        cabOn->setBounds (x + 412, 326, 160, 28);
        source->setBounds (x, 326, 400, 28);
        speaker->setBounds (x, 362, 250, 48); box->setBounds (x + 262, 362, 310, 48);
        factory->setBounds (x, 362, 572, 48); load->setBounds (x, 372, 180, 32);
        pad.setBounds (x, 420, 270, 156);
        micA->setBounds (x + 286, 440, 286, 28); micB->setBounds (x + 286, 494, 286, 28);
        mix->setBounds (x + 286, 532, 136, 40); room->setBounds (x + 436, 532, 136, 40);
    }
    void tick() override
    {
        const int src = (int) proc.value ("cab_source");
        speaker->setVisible (src == 0); box->setVisible (src == 0); factory->setVisible (src == 1); load->setVisible (src == 2);
        const bool model0 = src == 0;
        pad.setVisible (model0); micA->setVisible (model0); micB->setVisible (model0); mix->setVisible (model0); room->setVisible (model0);
        // La foto y los micrófonos: repintar si cambiaron el amplificador, el gabinete o los micrófonos.
        uint64_t sig = (uint64_t) proc.value ("amp_model") * 7 + (uint64_t) proc.value ("cab_box") * 131 + (uint64_t) src * 1009;
        for (auto id : { "cab_pos_a", "cab_dist_a", "cab_pos_b", "cab_dist_b", "cab_mix", "cab_mic_a", "cab_mic_b", "cab_speaker", "cab_room", "cab_ir", "amp_on", "cab_on" })
            sig = sig * 1000003u + (uint64_t) juce::roundToInt (proc.value (id) * 10.0f);
        sig = sig * 31 + (inspect ? 1u : 0u);
        if (sig != lastSig) { lastSig = sig; if (inspect) computeResponse(); repaint (0, 0, 720, 584); pad.repaint(); repaint (720, 380, 608, 204); }
    }
    void setInspect (bool on) override { Page::setInspect (on); lastSig = 0; tick(); }
    void paint (juce::Graphics& g) override
    {
        const int a = juce::jlimit (0, 3, (int) proc.value ("amp_model")), c = juce::jlimit (0, 3, (int) proc.value ("cab_box"));
        auto im = asset ("amp_" + juce::String (a) + "_" + juce::String (c) + ".jpg");
        auto rr = juce::Rectangle<float> (0, 0, 712, 584);
        {
            juce::Graphics::ScopedSaveState ss (g);
            juce::Path clip; clip.addRoundedRectangle (rr, 8.0f); g.reduceClipRegion (clip);
            if (im.isValid()) g.drawImage (im, rr, juce::RectanglePlacement::fillDestination | juce::RectanglePlacement::xMid | juce::RectanglePlacement::yMid);
            if ((int) proc.value ("cab_source") == 0) paintMics (g, rr, im, c);
        }
        g.setColour (col::text); text (g, juce::String (dh::gt::ampName (a)).toUpperCase(), { 22, 14, 600, 22 }, juce::Justification::centredLeft, 18.0f, true, 0.05f);
        g.setColour (col::dim); text (g, juce::String (dh::gt::cabName (c)) + "  /  " + dh::gt::speakerName ((int) proc.value ("cab_speaker")), { 22, 36, 600, 18 }, juce::Justification::centredLeft, 14.0f);
        if (inspect) paintResponse (g, { 16, 446, 680, 124 });
        panelBox (g, { 720, 0, 608, 584 });
        section (g, "Amplifier", 732, 10); section (g, "Cabinet", 732, 298);
        if ((int) proc.value ("cab_source") == 2)
        {
            const auto n = proc.userIrName();
            label (g, n.isNotEmpty() ? n : "No file loaded: WAV, AIFF or FLAC, up to 3 seconds", { 924, 372, 400, 32 }, juce::Justification::centredLeft, 14.0f, col::text);
        }
        if ((int) proc.value ("cab_source") == 0) { label (g, "Mic A", { 1018, 422, 120, 15 }, juce::Justification::centredLeft, 12.5f, col::amber); label (g, "Mic B", { 1018, 476, 120, 15 }, juce::Justification::centredLeft, 12.5f, col::red); }
    }

private:
    MicPad& pad;
    Chips *model, *quality, *source, *micA, *micB; std::vector<Knob*> amp; Knob* level; Toggle *ampOn, *bright, *autoLvl, *cabOn; Menu *speaker, *box, *factory; HSlider *mix, *room;
    Button* load; std::unique_ptr<juce::FileChooser> chooser; uint64_t lastSig = 0;
    std::vector<float> resp; juce::String respNote;

    void paintMics (juce::Graphics& g, juce::Rectangle<float> rr, const juce::Image& im, int c)
    {
        const auto& L = layout()["amp"][c];
        if (! L.isObject() || ! im.isValid()) return;
        // La foto se dibuja llenando el área, centrada: la misma cuenta para pasar de fracciones a píxeles.
        const float s = std::max (rr.getWidth() / (float) im.getWidth(), rr.getHeight() / (float) im.getHeight());
        const float ox = rr.getCentreX() - (float) im.getWidth() * s * 0.5f, oy = rr.getCentreY() - (float) im.getHeight() * s * 0.5f;
        auto P = [&] (const juce::var& v) { auto p = lpt (v); return juce::Point<float> (ox + p.x * (float) im.getWidth() * s, oy + p.y * (float) im.getHeight() * s); };
        const auto cen = P (L["center"]), ex = P (L["edgeX"]), fr = P (L["front"]);
        const bool showB = proc.value ("cab_mix") > 0.5f, showA = proc.value ("cab_mix") < 99.5f;
        for (int k = 1; k >= 0; --k)
        {
            if ((k == 1 && ! showB) || (k == 0 && ! showA)) continue;
            const float pos = proc.value (k ? "cab_pos_b" : "cab_pos_a") * 0.01f, d = proc.value (k ? "cab_dist_b" : "cab_dist_a");
            const int type = juce::jlimit (0, 2, (int) proc.value (k ? "cab_mic_b" : "cab_mic_a"));
            auto mic = asset ("mic_" + juce::String (type) + ".png");
            const auto& ML = layout()["mic"][type];
            if (! mic.isValid() || ! ML.isObject()) continue;
            const auto tip = cen + (ex - cen) * (pos * (k ? -1.0f : 1.0f)) + (fr - cen) * (0.15f + 0.85f * (d / 60.0f));
            const float sc = 0.42f * (1.0f + 0.5f * (d / 60.0f)) * rr.getWidth() / 712.0f;
            const auto tp = lpt (ML["tip"]);
            const float w = (float) mic.getWidth() * sc, h = (float) mic.getHeight() * sc;
            g.drawImage (mic, { tip.x - tp.x * w, tip.y - tp.y * h, w, h });
            g.setColour (k ? col::red : col::amber); g.fillEllipse (tip.x - 9.0f, tip.y - 22.0f - 9.0f, 18.0f, 18.0f);
            g.setColour (col::ink); text (g, k ? "B" : "A", { tip.x - 9.0f, tip.y - 31.0f, 18.0f, 18.0f }, juce::Justification::centred, 12.5f, true);
        }
    }
    // Inspect: la respuesta del gabinete (la IR del modelo o de fábrica, la misma que usa el motor).
    void computeResponse()
    {
        resp.clear(); respNote.clear();
        const auto P = proc.buildParams();
        if (P.cabSource == 2) { respNote = "USER IR"; return; }
        const auto& cp = P.cabSource == 1 ? dh::gt::factoryIrs()[(size_t) juce::jlimit (0, (int) dh::gt::factoryIrs().size() - 1, P.factoryIr)].p : P.cab;
        const double fs = 48000.0;
        const auto ir = dh::gt::designCabIr (cp, fs, (int) (0.045 * fs));
        for (int k = 0; k < 160; ++k)
        {
            const double f = 50.0 * std::pow (2.0, (double) k / 159.0 * std::log2 (16000.0 / 50.0));
            double re = 0, im = 0; const double w = 2.0 * juce::MathConstants<double>::pi * f / fs;
            for (size_t n = 0; n < ir.size(); ++n) { re += ir[n] * std::cos (w * (double) n); im -= ir[n] * std::sin (w * (double) n); }
            resp.push_back ((float) (10.0 * std::log10 (re * re + im * im + 1e-12)));
        }
        const float mx = *std::max_element (resp.begin(), resp.end());
        for (auto& v : resp) v -= mx;
    }
    void paintResponse (juce::Graphics& g, juce::Rectangle<float> r)
    {
        g.setColour (col::ink.withAlpha (0.78f)); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::cyanDim); g.drawRoundedRectangle (r, 6.0f, 1.0f);
        auto pr = r.reduced (12.0f, 22.0f);
        g.setColour (col::cyan); text (g, "CABINET RESPONSE  (dB, 50 Hz - 16 kHz)" + (respNote.isNotEmpty() ? "  /  " + respNote : juce::String()), { r.getX() + 12.0f, r.getY() + 3.0f, 500.0f, 16.0f }, juce::Justification::centredLeft, 12.0f, true);
        for (double f : { 100.0, 1000.0, 10000.0 })
        {
            const float x = pr.getX() + pr.getWidth() * (float) (std::log2 (f / 50.0) / std::log2 (16000.0 / 50.0));
            g.setColour (col::cyanDim); g.drawVerticalLine ((int) x, pr.getY(), pr.getBottom());
            g.setColour (col::cyan.withAlpha (0.7f)); text (g, f >= 1000 ? juce::String ((int) (f / 1000)) + " kHz" : juce::String ((int) f) + " Hz", { x + 3.0f, pr.getBottom() + 2.0f, 60.0f, 14.0f }, juce::Justification::centredLeft, 11.0f);
        }
        if (resp.empty()) return;
        juce::Path p;
        for (size_t k = 0; k < resp.size(); ++k)
        {
            const float x = pr.getX() + pr.getWidth() * (float) k / (float) (resp.size() - 1);
            const float y = pr.getY() + pr.getHeight() * juce::jlimit (0.0f, 1.0f, -resp[k] / 40.0f);
            if (k == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        g.setColour (col::cyan); g.strokePath (p, juce::PathStrokeType (1.6f));
    }
};

// ───────── Settings ─────────
class SettingsPage : public Page
{
public:
    explicit SettingsPage (ThrumProcessor& p) : Page (p)
    {
        midiIn = &add<Chips> (proc, "m_in", juce::StringArray { "Normal", "Guitar (per string)", "MPE" });
        strBend = &knob ("m_str_bend", "String bend range", false); mpeBend = &knob ("m_mpe_bend", "MPE bend range", false); ks = &knob ("m_ks", "Keyswitch base", false);
        out = &add<Toggle> (proc, "m_out", "MIDI out"); outStr = &add<Toggle> (proc, "m_out_str", "Channel per string"); outCh = &knob ("m_out_ch", "Out channel", false);
        level = &knob ("out_level", "Output", false); di = &knob ("out_di", "DI level", false); diMain = &add<Toggle> (proc, "out_di_main", "Main output is DI");
        tempo = &knob ("s_tempo", "Tempo (host stopped)", false); seed = &knob ("m_seed", "Variation seed", false);
    }
    void resized() override
    {
        midiIn->setBounds (20, 40, 400, 30);
        row ({ strBend, mpeBend, ks }, 14, 86, 110, 92);
        out->setBounds (466, 40, 180, 30); outStr->setBounds (466, 80, 220, 30); outCh->setBounds (720, 34, 110, 92);
        row ({ level, di }, 908, 30, 110, 92); diMain->setBounds (1140, 46, 170, 30);
        row ({ tempo, seed }, 908, 296, 150, 92);
    }
    void paint (juce::Graphics& g) override
    {
        panelBox (g, { 0, 0, 436, 250 }); panelBox (g, { 446, 0, 436, 250 }); panelBox (g, { 892, 0, 436, 250 }); panelBox (g, { 0, 260, 882, 324 }); panelBox (g, { 892, 260, 436, 324 });
        section (g, "MIDI input", 20, 12); section (g, "MIDI output", 466, 12); section (g, "Outputs", 912, 12); section (g, "Keyswitches and controllers", 20, 272, 500); section (g, "Performance", 912, 272);
        const int base = (int) proc.value ("m_ks");
        static const char* nn[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
        auto nm = [&] (int n) { return juce::String (nn[n % 12]) + juce::String (n / 12 - 1); };
        para (g, "Keyswitches start at MIDI note " + juce::String (base) + " (" + nm (base) + " when middle C is C4). Only keys below the guitar's lowest string switch; they never sound. "
                 "Guitar mode: channel 1 is the highest string. MPE: each note keeps its channel, bend, pressure and CC 74.", { 20, 184, 400, 60 }, 13.5f);
        para (g, "With MIDI out on, Thrum sends what the guitar played: every strummed string and every fingered note, at the moment it sounds. Channel per string: channel 1 is the highest string.",
              { 466, 134, 400, 90 }, 13.5f);
        para (g, "DI: the guitar alone before the pedals, on the second output, delayed exactly like the amplifier so a re-amp lines up. Main output is DI: for hosts with one output.",
              { 912, 134, 400, 90 }, 13.5f);
        // La tabla de keyswitches con las notas reales de la base elegida.
        static const char* arts[] = { "Sustain", "Palm mute", "Staccato", "Dead notes", "Harmonics", "Pinch harmonics", "Tremolo picking", "Bend in", "Slide in", "Slide out" };
        static const char* holds[] = { "Up-strokes", "Feedback", "Vibrato", "Mute all strings", "Fingers" };
        float y = 306;
        for (int i = 0; i < 10; ++i)
        {
            g.setColour (col::amber); text (g, nm (base + i), { 20, y, 50, 18 }, juce::Justification::centredLeft, 14.5f, true);
            g.setColour (col::text); text (g, arts[i], { 70, y, 170, 18 }, juce::Justification::centredLeft, 14.5f);
            y += 26;
        }
        y = 306;
        for (int i = 0; i < 5; ++i)
        {
            g.setColour (col::amber); text (g, nm (base + 12 + i), { 250, y, 50, 18 }, juce::Justification::centredLeft, 14.5f, true);
            g.setColour (col::text); text (g, juce::String (holds[i]) + " (held)", { 300, y, 200, 18 }, juce::Justification::centredLeft, 14.5f);
            y += 26;
        }
        static const char* ccs[][2] = { { "Pitch bend", "bend the last string (or all)" }, { "CC 1", "vibrato depth" }, { "CC 2", "palm mute amount" }, { "CC 4", "whammy (push down)" },
                                        { "CC 11", "wah (MIDI CC 11 mode)" }, { "CC 64", "let the strings ring" }, { "CC 68", "legato on / off" }, { "CC 74", "pick hardness" },
                                        { "Pressure", "vibrato" }, { "CC 120 / 123", "all sound / notes off" } };
        y = 306;
        for (auto& c : ccs)
        {
            g.setColour (col::amber); text (g, c[0], { 520, y, 100, 18 }, juce::Justification::centredLeft, 14.5f, true);
            g.setColour (col::text); text (g, c[1], { 620, y, 250, 18 }, juce::Justification::centredLeft, 14.5f);
            y += 26;
        }
        g.setColour (col::text);
        text (g, juce::String ("Thrum ") + THRUM_VERSION, { 912, 410, 400, 22 }, juce::Justification::centredLeft, 18.0f, true);
        para (g, "Electric guitar by physical modelling. No samples.\nLatency " + juce::String (proc.getLatencySamples()) + " samples at the current amplifier quality.\n"
                 "Free and open source (GNU AGPL v3). audiotools.com.ar", { 912, 438, 400, 80 }, 14.0f);
        para (g, "The variation seed gives a different, equally natural performance of the same MIDI.", { 912, 524, 400, 44 }, 13.0f, col::faint);
    }
private:
    Chips* midiIn; Knob *strBend, *mpeBend, *ks, *outCh, *level, *di, *tempo, *seed; Toggle *out, *outStr, *diMain;
};

} // namespace thrumui
