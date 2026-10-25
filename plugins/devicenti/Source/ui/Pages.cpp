#include "Pages.h"
#include "UiModel.h"

namespace dh::dv::ui {

juce::Font Theme::font (float h, bool bold)
{
    static juce::Typeface::Ptr tfB, tfM;
    if (tfB == nullptr)
    {
        int sz = 0;
        if (const char* d = resource ("ArchivoCondensed-Bold.ttf", sz)) tfB = juce::Typeface::createSystemTypefaceFor (d, (size_t) sz);
        if (const char* d = resource ("ArchivoCondensed-Medium.ttf", sz)) tfM = juce::Typeface::createSystemTypefaceFor (d, (size_t) sz);
    }
    auto tf = bold ? tfB : tfM;
    if (tf != nullptr) return juce::Font (juce::FontOptions (tf).withHeight (h));
    return juce::Font (juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain));
}

// ───────────── FieldPage ─────────────
void FieldPage::layoutRows()
{
    rows.clear(); groups.clear();
    const auto b = getLocalBounds().toFloat().reduced (18, 14);
    const float scale = std::max (0.7f, (float) getWidth() / 1120.0f);
    const float rowH = 34.0f * scale, headH = 28.0f * scale, gap = 10.0f * scale;
    // Groups in columns, each group a block, each into the shortest column (in order), so no column stays empty.
    const int cols = page == Page::Ops ? 4 : 3;              // the window keeps its proportions, so three always fit
    const float colW = (b.getWidth() - gap * (float) (cols - 1)) / (float) cols;
    std::vector<float> colY ((size_t) cols, b.getY());
    for (int i = 0; i < (int) fields.size();)
    {
        const juce::String g = fields[(size_t) i].group;
        int n = 0; while (i + n < (int) fields.size() && fields[(size_t) (i + n)].group == g) ++n;
        int col = 0;
        if (page == Page::Ops) col = juce::jmin (cols - 1, (int) groups.size());
        else for (int c = 1; c < cols; ++c) if (colY[(size_t) c] < colY[(size_t) col] - 1.0f) col = c;
        const float x = b.getX() + (float) col * (colW + gap);
        float y = colY[(size_t) col];
        groups.push_back ({ g, { x, y, colW, headH } });
        float ry = y + headH;
        for (int k = 0; k < n; ++k) { rows.push_back ({ i + k, { x, ry, colW, rowH } }); ry += rowH; }
        colY[(size_t) col] = ry + gap;
        i += n;
    }
}

void FieldPage::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg());
    const float scale = std::max (0.7f, (float) getWidth() / 1120.0f);
    for (const auto& gr : groups)
    {
        g.setColour (Theme::accent()); g.setFont (Theme::font (15.0f * scale));
        g.drawText (gr.name.toUpperCase(), gr.r.withTrimmedBottom (4), juce::Justification::bottomLeft);
        g.setColour (Theme::accent().withAlpha (0.4f)); g.fillRect (gr.r.withTop (gr.r.getBottom() - 2).withHeight (1.5f));
    }
    for (int k = 0; k < (int) rows.size(); ++k)
    {
        const auto& row = rows[(size_t) k];
        const auto& f = fields[(size_t) row.field];
        auto r = row.r.reduced (2, 3);
        const bool active = k == dragRow;
        g.setColour (active ? Theme::panel().brighter (0.15f) : Theme::panel()); g.fillRoundedRectangle (r, 4.0f);
        const float u = f.get ? juce::jlimit (0.0f, 1.0f, f.get()) : 0.0f;
        // Value bar along the bottom.
        auto bar = r.removeFromBottom (4.0f * scale).reduced (6, 0);
        g.setColour (Theme::dim().withAlpha (0.25f)); g.fillRoundedRectangle (bar, 1.5f);
        if (f.enabled) { g.setColour (f.steps > 1 ? Theme::accent2() : Theme::accent()); g.fillRoundedRectangle (bar.withWidth (std::max (3.0f, bar.getWidth() * u)), 1.5f); }
        r = r.reduced (8, 0);
        g.setColour (f.enabled ? Theme::ink() : Theme::dim()); g.setFont (Theme::font (14.5f * scale, false));
        g.drawText (f.name, r, juce::Justification::centredLeft, true);
        g.setColour (f.enabled ? Theme::accent2() : Theme::dim()); g.setFont (Theme::font (15.0f * scale));
        g.drawText (f.text ? f.text() : juce::String(), r, juce::Justification::centredRight, true);
    }
    if (fields.empty())
    {
        g.setColour (Theme::dim()); g.setFont (Theme::font (18.0f));
        g.drawText ("Nothing to edit here", getLocalBounds(), juce::Justification::centred);
    }
}

int FieldPage::rowAt (juce::Point<float> p) const { for (int k = 0; k < (int) rows.size(); ++k) if (rows[(size_t) k].r.contains (p)) return k; return -1; }

void FieldPage::mouseDown (const juce::MouseEvent& e)
{
    dragRow = rowAt (e.position); moved = false; downPos = e.position;
    if (dragRow < 0) return;
    auto& f = fields[(size_t) rows[(size_t) dragRow].field];
    if (! f.enabled) { dragRow = -1; return; }
    dragNorm = f.get ? f.get() : 0.0f; dragStart = 0;
    if (f.gesture) f.gesture (true);
    repaint();
}

void FieldPage::mouseDrag (const juce::MouseEvent& e)
{
    if (dragRow < 0) return;
    auto& f = fields[(size_t) rows[(size_t) dragRow].field];
    const auto d = e.position - downPos;
    const float amount = (d.x - d.y) / (e.mods.isShiftDown() ? 1200.0f : 240.0f);
    if (std::abs (d.x) + std::abs (d.y) > 3) moved = true;
    if (! moved) return;
    if (f.steps > 1)
    {
        const int n = f.steps; const float per = n > 40 ? 3.0f : 14.0f;
        const int k0 = juce::roundToInt (dragNorm * (float) (n - 1));
        const int k = juce::jlimit (0, n - 1, k0 + (int) ((d.x - d.y) / per));
        f.set ((float) k / (float) (n - 1));
    }
    else f.set (juce::jlimit (0.0f, 1.0f, dragNorm + amount));
    repaint();
}

void FieldPage::mouseUp (const juce::MouseEvent& e)
{
    if (dragRow < 0) return;
    auto& f = fields[(size_t) rows[(size_t) dragRow].field];
    if (! moved && f.steps > 1)
    {
        // A click steps a choice: right half forward, left half back.
        const int n = f.steps; const int k0 = juce::roundToInt ((f.get ? f.get() : 0.0f) * (float) (n - 1));
        const bool fwd = e.position.x > rows[(size_t) dragRow].r.getCentreX();
        f.set ((float) ((k0 + (fwd ? 1 : n - 1)) % n) / (float) (n - 1));
    }
    if (f.gesture) f.gesture (false);
    dragRow = -1;
    if (page == Page::Voice || page == Page::Midi || page == Page::Mix) rebuild();     // machine / lane / focus change what the page shows
    repaint();
}

void FieldPage::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int k = rowAt (e.position);
    if (k < 0) return;
    auto& f = fields[(size_t) rows[(size_t) k].field];
    if (! f.enabled) return;
    const float d = (w.isReversed ? -1.0f : 1.0f) * w.deltaY;
    if (f.steps > 1) { const int n = f.steps; const int k0 = juce::roundToInt (f.get() * (float) (n - 1)); f.set ((float) juce::jlimit (0, n - 1, k0 + (d > 0 ? 1 : -1)) / (float) (n - 1)); }
    else { if (f.gesture) f.gesture (true); f.set (juce::jlimit (0.0f, 1.0f, f.get() + d * 0.08f)); if (f.gesture) f.gesture (false); }
    repaint();
}

void FieldPage::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Double-click a continuous row: type a value (0-100 % of the range).
    const int k = rowAt (e.position);
    if (k < 0) return;
    auto& f = fields[(size_t) rows[(size_t) k].field];
    if (! f.enabled || f.steps > 1) return;
    auto* aw = new juce::AlertWindow (f.name, "Position in the range, 0 to 100", juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("v", juce::String (juce::roundToInt (f.get() * 100.0f)));
    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<FieldPage> safe (this);
    const int fi = rows[(size_t) k].field;
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw, fi] (int r) {
        if (safe != nullptr && r == 1 && fi < (int) safe->fields.size()) { auto& fd = safe->fields[(size_t) fi]; if (fd.gesture) fd.gesture (true); fd.set (juce::jlimit (0.0f, 1.0f, aw->getTextEditorContents ("v").getFloatValue() / 100.0f)); if (fd.gesture) fd.gesture (false); safe->repaint(); }
    }), true);
}

// ───────────── SeqPage ─────────────
void SeqPage::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg());
    const float scale = std::max (0.7f, (float) getWidth() / 1120.0f);
    const int pat = currentPattern (proc);
    auto b = getLocalBounds().toFloat().reduced (18, 14);
    g.setColour (Theme::accent()); g.setFont (Theme::font (16.0f * scale));
    g.drawText ("PATTERN " + juce::String (pat + 1) + "   (click a cell to toggle a step; drag to paint)", b.removeFromTop (26 * scale), juce::Justification::centredLeft);
    lanes.clear();
    struct Info { int machine; int length; std::array<uint8_t, kLanes> laneLen; };
    std::array<Info, kTracks> inf {};
    proc.withProject ([&] (const Project& pr) { for (int t = 0; t < kTracks; ++t) inf[(size_t) t] = { pr.track[(size_t) t].machine, pr.bank.p[(size_t) pat].t[(size_t) t].length, pr.bank.p[(size_t) pat].t[(size_t) t].laneLen }; return 0; });
    int nRows = 0; for (int t = 0; t < kTracks; ++t) nRows += inf[(size_t) t].machine == kKit ? 8 : 1;
    const float labelW = 90.0f * scale;
    const float rowH = std::min (30.0f * scale, (b.getHeight() - 4 * 8) / (float) nRows);
    x0 = b.getX() + labelW; cellW = (b.getWidth() - labelW) / 64.0f;
    float y = b.getY();
    for (int t = 0; t < kTracks; ++t)
    {
        const bool kit = inf[(size_t) t].machine == kKit;
        for (int l = kit ? 1 : 0; l <= (kit ? 8 : 0); ++l) { lanes.push_back ({ t, l, { b.getX(), y, b.getWidth(), rowH } }); y += rowH; }
        y += 8;
    }
    const auto steps = proc.withProject ([&] (const Project& pr) {
        std::vector<std::array<uint8_t, 64>> v;
        for (const auto& ln : lanes) { std::array<uint8_t, 64> a {}; for (int i = 0; i < 64; ++i) { const Step& s = pr.bank.p[(size_t) pat].t[(size_t) ln.track].lane[(size_t) ln.lane][(size_t) i]; a[(size_t) i] = (uint8_t) (s.on() ? (s.lockCount > 0 ? 3 : (s.cond != cNone || s.prob < 100 ? 2 : 1)) : 0); } v.push_back (a); }
        return v; });
    for (int li = 0; li < (int) lanes.size(); ++li)
    {
        const auto& ln = lanes[(size_t) li];
        const auto& I = inf[(size_t) ln.track];
        const int len = ln.lane > 0 && I.laneLen[(size_t) ln.lane] > 0 ? I.laneLen[(size_t) ln.lane] : I.length;
        g.setColour (ln.track == st.track ? Theme::accent2() : Theme::ink()); g.setFont (Theme::font (13.0f * scale, ln.lane == 0 || ln.lane == 1));
        g.drawText ((ln.lane <= 1 ? "T" + juce::String (ln.track + 1) + " " : juce::String ("   ")) + (ln.lane > 0 ? "L" + juce::String (ln.lane) : juce::String (I.machine == kMidiMachine ? "MIDI" : "TONE")),
                    ln.r.withWidth (labelW - 6), juce::Justification::centredLeft);
        const int cur = proc.viewPattern.load() == pat ? proc.viewStep[(size_t) ln.track].load() : -1;
        for (int i = 0; i < 64; ++i)
        {
            auto c = juce::Rectangle<float> (x0 + cellW * (float) i, ln.r.getY(), cellW, ln.r.getHeight()).reduced (1.0f, 2.0f);
            const int v = steps[(size_t) li][(size_t) i];
            const bool inLen = i < len;
            juce::Colour col = (i / 4) % 2 == 0 ? Theme::panel() : Theme::panel().brighter (0.08f);
            if (! inLen) col = col.darker (0.5f);
            if (v == 1) col = Theme::accent(); else if (v == 2) col = Theme::accent().withAlpha (0.55f); else if (v == 3) col = Theme::accent2();
            if (i == cur) col = col.interpolatedWith (juce::Colours::red, 0.6f);
            g.setColour (col); g.fillRoundedRectangle (c, 2.0f);
        }
    }
}

bool SeqPage::cellAt (juce::Point<float> p, int& li, int& step) const
{
    for (int k = 0; k < (int) lanes.size(); ++k)
        if (lanes[(size_t) k].r.contains (p) && p.x >= x0) { li = k; step = juce::jlimit (0, 63, (int) ((p.x - x0) / cellW)); return true; }
    return false;
}

void SeqPage::mouseDown (const juce::MouseEvent& e)
{
    int li, step; paintValue = -1;
    if (! cellAt (e.position, li, step)) return;
    const auto& ln = lanes[(size_t) li];
    const int pat = currentPattern (proc);
    const bool on = proc.withProject ([&] (const Project& pr) { return pr.bank.p[(size_t) pat].t[(size_t) ln.track].lane[(size_t) ln.lane][(size_t) step].on(); });
    paintValue = on ? 0 : 1;
    proc.editProject ([&] (Project& pr) { stepedit::toggle (pr.bank.p[(size_t) pat], ln.track, ln.lane, step); }, 0, 1u << pat);
    repaint();
}

void SeqPage::mouseDrag (const juce::MouseEvent& e)
{
    int li, step;
    if (paintValue < 0 || ! cellAt (e.position, li, step)) return;
    const auto& ln = lanes[(size_t) li];
    const int pat = currentPattern (proc);
    const bool on = proc.withProject ([&] (const Project& pr) { return pr.bank.p[(size_t) pat].t[(size_t) ln.track].lane[(size_t) ln.lane][(size_t) step].on(); });
    if (on != (paintValue == 1)) { proc.editProject ([&] (Project& pr) { stepedit::toggle (pr.bank.p[(size_t) pat], ln.track, ln.lane, step); }, 0, 1u << pat); repaint(); }
}

// ───────────── SettingsPage ─────────────
void SettingsPage::paint (juce::Graphics& g)
{
    g.fillAll (Theme::bg());
    const float scale = std::max (0.7f, (float) getWidth() / 1120.0f);
    auto b = getLocalBounds().toFloat().reduced (26, 20);
    g.setColour (Theme::accent()); g.setFont (Theme::font (17.0f * scale));
    g.drawText ("WINDOW SIZE", b.removeFromTop (28 * scale), juce::Justification::centredLeft);
    sizeButtons.clear();
    auto row = b.removeFromTop (40 * scale);
    for (float s : { 0.75f, 0.9f, 1.0f, 1.25f, 1.5f })
    {
        auto r = row.removeFromLeft (90 * scale).reduced (4);
        sizeButtons.push_back ({ s, r });
        g.setColour (Theme::panel()); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (Theme::ink()); g.setFont (Theme::font (15.0f * scale));
        g.drawText (juce::String (juce::roundToInt (s * 100)) + " %", r, juce::Justification::centred);
    }
    b.removeFromTop (16 * scale);
    g.setColour (Theme::accent()); g.setFont (Theme::font (17.0f * scale));
    g.drawText ("USING THE PANEL WITH A MOUSE", b.removeFromTop (28 * scale), juce::Justification::centredLeft);
    const char* lines[] = {
        "Knobs: drag up and down (Shift for fine), scroll, double-click for the default. Right-click: MIDI learn, CC number.",
        "FUNC: click to latch the second layer (the small rust labels), click again to release. Holding Shift does the same while held.",
        "TRACK 1-4 select the track the operators, the bottom row and the STEP keys edit. FUNC + TRACK mutes; FUNC + Alt-click solos.",
        "REC: grid record. Click a STEP to toggle it. Hold a STEP for half a second (or Alt-click it) to hold it: then every knob you turn",
        "    becomes a lock on that step, ALGORITHM picks a trig field and DATA edits it. Click the step again (or Escape) to let go.",
        "REC off: the STEP keys play the track: 16 notes of the pattern's scale (TONE), the eight lanes and lane select (KIT).",
        "FUNC + STEP: 1-4 page of steps, 5 copy, 6 paste, 7 clear, 8 fill while held, 9-16 pattern (Alt-click: jump at once).",
        "The small display: FUNC + DATA picks the page, click its left or right half to step through the fields, DATA edits.",
        "Chord memory: hold CHORD and play notes (MIDI or the STEP keys), let go to store; or Alt-click CHORD, play, click CHORD.",
        "Every page is also here at full size: the PAGES menu at the top, or right-click the display.",
    };
    g.setColour (Theme::ink()); g.setFont (Theme::font (15.0f * scale, false));
    for (auto* l : lines) g.drawText (l, b.removeFromTop (25 * scale), juce::Justification::centredLeft, true);
    b.removeFromTop (16 * scale);
    g.setColour (Theme::dim()); g.setFont (Theme::font (14.0f * scale, false));
    g.drawText ("Audio Tools DeVicenti " DV_VERSION "   |   latency " + juce::String (proc.engineLatency()) + " samples", b.removeFromTop (24 * scale), juce::Justification::centredLeft);
}

void SettingsPage::mouseDown (const juce::MouseEvent& e)
{
    for (const auto& [s, r] : sizeButtons) if (r.contains (e.position) && onScale) onScale (s);
}

} // namespace dh::dv::ui
