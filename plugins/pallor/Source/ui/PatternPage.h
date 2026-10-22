#pragma once
// La página PATTERN: a la izquierda el banco (16 patrones × A, B y Fill), largo, escala, swing del patrón, biblioteca,
// copiar y pegar, y el patrón como MIDI para arrastrar al DAW. A la derecha la grilla (16 pistas, 16/32/64 pasos según
// el largo) y abajo las líneas de edición de la pista elegida: velocidad, probabilidad, repeticiones, condición, flam,
// corrimiento y acento, que se dibujan arrastrando.

#include "Ctx.h"
#include "ThemePl.h"

namespace dh::pal::ui {

class PatternGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PatternGrid (Ctx& c) : ctx (c)
    {
        setTooltip ("Click: step on/off (drag to paint). Shift: accent. Alt: ratchet. Cmd/Ctrl: probability. Right-click: everything about the step. "
                    "Click a name to select the voice; right-click it for its own length, shift and clear.");
    }
    static constexpr float kNameW = 112.0f, kHeadH = 18.0f;
    std::function<void()> onEdited;
    void refresh() { pat = ctx.proc.getPattern (ctx.editPattern()); repaint(); }

    int columns() const { const int L = std::max (1, (int) pat.length); return L <= 16 ? 16 : (L <= 32 ? 32 : 64); }
    juce::Rectangle<float> cell (int t, int s) const
    {
        const float x0 = kNameW, w = ((float) getWidth() - x0) / (float) columns(), h = ((float) getHeight() - kHeadH) / (float) kTracks;
        return { x0 + w * (float) s, kHeadH + h * (float) t, w, h };
    }
    juce::Rectangle<float> column (int s) const { auto a = cell (0, s); return a.withHeight ((float) getHeight()).withY (0); }

    void paint (juce::Graphics& g) override
    {
        const int cols = columns(), L = pat.length;
        const auto clip = g.getClipBounds().toFloat();
        const bool showPlay = ctx.proc.view.pattern.load() == ctx.editPattern() && ctx.proc.view.running.load();
        // Números de paso.
        for (int s = 0; s < cols; ++s)
        {
            auto r = cell (0, s).withY (0).withHeight (kHeadH);
            if (! clip.intersects (r)) continue;
            const bool beat = s % 4 == 0;
            if (cols == 64 && ! beat) continue;
            g.setColour (s < L ? (beat ? col::boneDim : col::boneFaint) : col::boneFaint.withAlpha (0.4f));
            text (g, juce::String (s + 1), r, juce::Justification::centredLeft, 12.0f);
        }
        for (int t = 0; t < kTracks; ++t)
        {
            const auto& tr = pat.t[(size_t) t];
            const int Lt = tr.len ? tr.len : L;
            if (! clip.intersects (cell (t, 0).withX (0).withWidth ((float) getWidth()))) continue;
            auto row = cell (t, 0).withX (0).withWidth (kNameW - 6);
            if (clip.intersects (row))
            {
            const bool sel = t == ctx.voice;
            if (sel) { g.setColour (col::cell2); g.fillRoundedRectangle (row.reduced (0, 1), 2.5f); g.setColour (col::rose); g.fillRect (row.getX(), row.getY() + 2, 2.5f, row.getHeight() - 4); }
            const float fl = ctx.flash[(size_t) t];
            if (fl > 0.02f) { g.setColour (col::rose.withAlpha (juce::jmin (1.0f, fl))); g.fillEllipse (row.getRight() - 9, row.getCentreY() - 3, 6, 6); }
            const bool muted = ctx.vvalue ("mute", t) > 0.5f, solo = ctx.vvalue ("solo", t) > 0.5f;
            g.setColour (muted ? col::boneFaint : (sel ? col::bone : col::boneDim));
            juce::String nm (params::voiceName (t)); if (tr.len) nm << "  /" << (int) tr.len;
            text (g, nm, row.withTrimmedLeft (8).withTrimmedRight (12), juce::Justification::centredLeft, 14.0f, sel);
            if (solo) { g.setColour (col::amber); text (g, "S", row.withTrimmedRight (14), juce::Justification::centredRight, 12.0f, true); }
            }
            for (int s = 0; s < cols; ++s)
            {
                auto r = cell (t, s).reduced (cols == 64 ? 0.8f : 1.5f, 1.5f);
                if (! clip.intersects (r)) continue;
                const bool in = s < L && s < Lt;
                juce::Colour base = (s / 4) % 2 ? col::cell : col::cell2;
                if (! in) base = base.withAlpha (0.35f);
                if (showPlay && ctx.proc.view.step[(size_t) t].load() == s) base = col::lineHi.brighter (0.15f);
                g.setColour (base); g.fillRoundedRectangle (r, 2.0f);
                const auto& st = tr.s[(size_t) s];
                if (! st.vel || ! in) continue;
                const bool acc = (st.flags & kAccent) != 0;
                const float a = 0.45f + 0.55f * (float) st.vel / 127.0f;
                auto fill = r.reduced (1.5f);
                juce::Colour c = (acc ? col::rose : col::bone).withAlpha (a);
                if (st.prob < 100)
                {
                    // Probabilidad: el relleno sube desde abajo hasta su porcentaje; el resto, contorno acero.
                    g.setColour (col::steel.withAlpha (0.8f)); g.drawRoundedRectangle (fill, 1.5f, 1.0f);
                    fill = fill.withTop (fill.getBottom() - fill.getHeight() * (float) st.prob / 100.0f);
                }
                g.setColour (c); g.fillRoundedRectangle (fill, 1.5f);
                g.setColour (col::ink.withAlpha (0.8f));
                if (st.ratchet > 1 && r.getWidth() > 10)
                    for (int k = 0; k < st.ratchet; ++k) g.fillRect (r.getX() + 3.0f + (float) k * (r.getWidth() - 6.0f) / (float) st.ratchet, r.getBottom() - 5.0f, std::max (1.5f, (r.getWidth() - 6.0f) / (float) st.ratchet - 1.5f), 2.0f);
                if (st.flam) g.fillEllipse (r.getX() + 2.5f, r.getY() + 2.5f, 3.5f, 3.5f);
                if (st.cond && r.getWidth() > 14) { g.setColour (col::ink); text (g, condShort (st.cond), r.reduced (2, 1), juce::Justification::topRight, 10.5f, true); }
                if (st.nudge && r.getWidth() > 14) { g.setColour (col::ink.withAlpha (0.8f)); const float nx = r.getCentreX() + (float) st.nudge / 50.0f * r.getWidth() * 0.4f; g.drawVerticalLine ((int) nx, r.getY() + 3, r.getBottom() - 6); }
            }
        }
        if (dropHi >= 0) { g.setColour (col::rose); g.drawRect (cell (dropHi, 0).withX (0).withWidth ((float) getWidth()), 1.0f); }
    }

    static juce::String condShort (int c) { static const char* n[] = { "", "F", "!F", "1:2", "2:2", "1:4", "4:4", "1st", "!1st" }; return n[juce::jlimit (0, 8, c)]; }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto p = e.position;
        const int t = juce::jlimit (0, kTracks - 1, (int) ((p.y - kHeadH) / (((float) getHeight() - kHeadH) / kTracks)));
        if (p.y < kHeadH) return;
        if (p.x < kNameW)
        {
            if (e.mods.isPopupMenu()) { trackMenu (t); return; }
            ctx.selectVoice (t); return;
        }
        const int s = juce::jlimit (0, columns() - 1, (int) ((p.x - kNameW) / (((float) getWidth() - kNameW) / (float) columns())));
        if (s >= pat.length) return;
        if (t != ctx.voice) ctx.selectVoice (t);
        if (e.mods.isPopupMenu()) { stepMenu (t, s); return; }
        auto& st = pat.t[(size_t) t].s[(size_t) s];
        if (e.mods.isShiftDown()) { if (! st.vel) st.vel = 100; st.flags ^= kAccent; }
        else if (e.mods.isAltDown()) { if (! st.vel) st.vel = 100; st.ratchet = (uint8_t) (st.ratchet >= 4 ? 1 : st.ratchet + 1); }
        else if (e.mods.isCommandDown()) { if (! st.vel) st.vel = 100; st.prob = (uint8_t) (st.prob <= 25 ? 100 : st.prob - 25); }
        else { paintOn = ! st.vel; paintTrack = t; st = paintOn ? Step { 100 } : Step(); lastPainted = s; }
        commit();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (paintTrack < 0) return;
        const int s = juce::jlimit (0, columns() - 1, (int) ((e.position.x - kNameW) / (((float) getWidth() - kNameW) / (float) columns())));
        if (s == lastPainted || s >= pat.length) return;
        auto& st = pat.t[(size_t) paintTrack].s[(size_t) s];
        if (paintOn && ! st.vel) st = Step { 100 };
        else if (! paintOn) st = Step();
        lastPainted = s; commit();
    }
    void mouseUp (const juce::MouseEvent&) override { paintTrack = -1; lastPainted = -1; }

    void commit() { ctx.proc.setPattern (ctx.editPattern(), pat); repaint(); if (onEdited) onEdited(); }

    // Una vez por cuadro: el cabezal y los destellos.
    void tick()
    {
        const bool showPlay = ctx.proc.view.pattern.load() == ctx.editPattern() && ctx.proc.view.running.load();
        for (int t = 0; t < kTracks; ++t)
        {
            const int s = showPlay ? ctx.proc.view.step[(size_t) t].load() : -1;
            if (s != shownStep[(size_t) t])
            {
                for (int x : { shownStep[(size_t) t], s }) if (x >= 0 && x < columns()) repaint (cell (t, x).expanded (1).getSmallestIntegerContainer());
                shownStep[(size_t) t] = s;
            }
            const bool lit = ctx.flash[(size_t) t] > 0.02f;
            if (lit || shownLit[(size_t) t]) { repaint (cell (t, 0).withX (kNameW - 14).withWidth (12).getSmallestIntegerContainer()); shownLit[(size_t) t] = lit; }
        }
    }
    dh::pal::Pattern pat;
    int dropHi = -1;

private:
    Ctx& ctx;
    int paintTrack = -1, lastPainted = -1; bool paintOn = true;
    std::array<int, kTracks> shownStep {};
    std::array<bool, kTracks> shownLit {};

    void stepMenu (int t, int s)
    {
        juce::PopupMenu m, vel, flam, nudge, cond, rat, prob;
        const auto cur = pat.t[(size_t) t].s[(size_t) s];
        for (int v : { 127, 110, 100, 80, 60, 40, 20 }) vel.addItem (100 + v, juce::String (v), true, cur.vel == v);
        for (int pr : { 100, 90, 75, 50, 33, 25, 10 }) prob.addItem (900 + pr, juce::String (pr) + " %", true, cur.vel && cur.prob == pr);
        for (int r = 1; r <= 8; ++r) rat.addItem (800 + r, r == 1 ? juce::String ("Off") : juce::String (r) + "x", true, cur.vel && cur.ratchet == r);
        for (int c = 0; c < kNumConds; ++c) cond.addItem (700 + c, condName (c), true, cur.vel && cur.cond == c);
        for (int f : { 0, 8, 15, 25, 40 }) flam.addItem (300 + f, f == 0 ? juce::String ("Off") : juce::String (f) + " ms", true, cur.vel && cur.flam == f);
        for (int nd : { -40, -25, -10, 0, 10, 25, 40 }) nudge.addItem (500 + nd + 50, (nd > 0 ? "+" : "") + juce::String (nd) + " %", true, cur.vel && cur.nudge == nd);
        m.addSectionHeader (juce::String (params::voiceName (t)) + ", step " + juce::String (s + 1));
        m.addItem (2, "Accent", true, cur.vel && (cur.flags & kAccent));
        m.addSubMenu ("Velocity", vel); m.addSubMenu ("Probability", prob); m.addSubMenu ("Ratchet", rat);
        m.addSubMenu ("Condition", cond); m.addSubMenu ("Flam", flam); m.addSubMenu ("Nudge", nudge);
        m.addSeparator(); m.addItem (1, "Clear step");
        juce::Component::SafePointer<PatternGrid> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, t, s] (int r) {
            if (safe == nullptr || r == 0) return;
            auto& st = safe->pat.t[(size_t) t].s[(size_t) s];
            if (r == 1) st = Step();
            else
            {
                if (! st.vel) st.vel = 100;
                if (r == 2) st.flags ^= kAccent;
                else if (r >= 100 && r < 300) st.vel = (uint8_t) (r - 100);
                else if (r >= 300 && r < 500) st.flam = (uint8_t) (r - 300);
                else if (r >= 500 && r < 700) st.nudge = (int8_t) (r - 550);
                else if (r >= 700 && r < 800) st.cond = (uint8_t) (r - 700);
                else if (r >= 800 && r < 900) st.ratchet = (uint8_t) (r - 800);
                else if (r >= 900) st.prob = (uint8_t) (r - 900);
            }
            safe->commit();
        });
    }

    void trackMenu (int t)
    {
        juce::PopupMenu m, len;
        const auto& tr = pat.t[(size_t) t];
        len.addItem (1000, "Same as pattern", true, tr.len == 0);
        for (int l : { 3, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 20, 24, 32, 48, 64 }) len.addItem (1000 + l, juce::String (l) + " steps", true, tr.len == l);
        m.addSectionHeader (juce::String (params::voiceName (t)));
        m.addSubMenu ("Track length (polymeter)", len);
        m.addItem (3, "Shift right"); m.addItem (4, "Shift left");
        m.addItem (5, "Copy track"); m.addItem (6, "Paste track", clipTrack != nullptr);
        m.addItem (2, "Clear track");
        juce::Component::SafePointer<PatternGrid> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, t] (int r) {
            if (safe == nullptr || r == 0) return;
            auto& trk = safe->pat.t[(size_t) t];
            const int L = std::max (1, trk.len ? (int) trk.len : (int) safe->pat.length);
            if (r >= 1000) trk.len = (uint8_t) (r - 1000);
            else if (r == 2) trk.s = {};
            else if (r == 3) std::rotate (trk.s.begin(), trk.s.begin() + (L - 1), trk.s.begin() + L);
            else if (r == 4) std::rotate (trk.s.begin(), trk.s.begin() + 1, trk.s.begin() + L);
            else if (r == 5) { clipTrack = std::make_unique<Track> (trk); return; }
            else if (r == 6 && clipTrack != nullptr) trk = *clipTrack;
            safe->commit();
        });
    }
    static inline std::unique_ptr<Track> clipTrack;
};

// Las líneas de edición de la pista elegida: barras por paso que se dibujan arrastrando.
class StepLanes : public juce::Component, public juce::SettableTooltipClient
{
public:
    StepLanes (Ctx& c, PatternGrid& g) : ctx (c), grid (g)
    {
        setTooltip ("Draw across the steps to set the selected lane for the selected voice. Double-click a step to reset it.");
    }
    enum Lane { kVel, kProb, kRatchet, kCond, kFlam, kNudge, kAcc, kNumLanes };
    int lane = kVel;
    static juce::String laneName (int l) { static const char* n[] = { "Velocity", "Probability", "Ratchet", "Condition", "Flam", "Nudge", "Accent" }; return n[l]; }

    juce::Rectangle<float> tabsR() const { return getLocalBounds().toFloat().removeFromTop (22.0f); }
    juce::Rectangle<float> tabR (int i) const { auto r = tabsR().withTrimmedLeft (PatternGrid::kNameW); const float w = std::min (110.0f, r.getWidth() / (float) kNumLanes); return { r.getX() + w * (float) i, r.getY(), w, r.getHeight() }; }
    juce::Rectangle<float> barsR() const { return getLocalBounds().toFloat().withTrimmedTop (26.0f).withTrimmedLeft (PatternGrid::kNameW); }
    juce::Rectangle<float> barR (int s) const { const int cols = grid.columns(); auto b = barsR(); const float w = b.getWidth() / (float) cols; return { b.getX() + w * (float) s, b.getY(), w, b.getHeight() }; }

    void paint (juce::Graphics& g) override
    {
        const auto& P = grid.pat; const auto& tr = P.t[(size_t) ctx.voice];
        const int cols = grid.columns(), L = P.length, Lt = tr.len ? tr.len : L;
        g.setColour (col::bone); text (g, juce::String (params::voiceName (ctx.voice)).toUpperCase(), getLocalBounds().toFloat().removeFromLeft (PatternGrid::kNameW).removeFromTop (22), juce::Justification::centredLeft, 14.0f, true, 0.1f);
        g.setColour (col::boneDim); text (g, laneName (lane).toUpperCase(), getLocalBounds().toFloat().removeFromLeft (PatternGrid::kNameW).withTrimmedTop (26).removeFromTop (16), juce::Justification::centredLeft, 12.0f, false, 0.1f);
        g.setColour (col::boneFaint); g.setFont (font (11.5f));
        g.drawFittedText (laneHint(), getLocalBounds().removeFromLeft ((int) PatternGrid::kNameW - 8).withTrimmedTop (44), juce::Justification::topLeft, 5, 1.0f);
        for (int i = 0; i < kNumLanes; ++i)
        {
            auto r = tabR (i).reduced (2, 2);
            g.setColour (i == lane ? col::rose : col::cell); g.fillRoundedRectangle (r, 3.0f);
            g.setColour (i == lane ? col::ink : col::boneDim); text (g, laneName (i).toUpperCase(), r, juce::Justification::centred, 12.0f, i == lane, 0.06f);
        }
        const bool bip = lane == kNudge;
        for (int s = 0; s < cols; ++s)
        {
            auto r = barR (s).reduced (cols == 64 ? 0.8f : 1.5f, 0);
            const bool in = s < L && s < Lt;
            g.setColour (in ? ((s / 4) % 2 ? col::cell : col::cell2) : col::cell.withAlpha (0.3f)); g.fillRoundedRectangle (r, 1.5f);
            const auto& st = tr.s[(size_t) s];
            if (! in || ! st.vel) continue;
            const float v = value (st);
            g.setColour (lane == kProb || lane == kCond ? col::steel : ((st.flags & kAccent) ? col::rose : col::bone));
            if (bip)
            {
                const float cy = r.getCentreY(), h = r.getHeight() * 0.5f * v;
                g.fillRect (r.reduced (r.getWidth() * 0.18f, 0).withY (h >= 0 ? cy - h : cy).withHeight (std::max (1.5f, std::abs (h))));
                g.setColour (col::lineHi); g.drawHorizontalLine ((int) cy, r.getX(), r.getRight());
            }
            else if (lane == kCond)
            {
                g.setColour (st.cond ? col::steel : col::boneFaint);
                text (g, st.cond ? PatternGrid::condShort (st.cond) : juce::String ("all"), r, juce::Justification::centred, cols == 64 ? 9.5f : 12.5f, st.cond != 0);
            }
            else
            {
                auto b = r.reduced (r.getWidth() * 0.18f, 0); b = b.withTop (b.getBottom() - std::max (2.0f, b.getHeight() * v));
                g.fillRoundedRectangle (b, 1.5f);
                if (lane == kRatchet && st.ratchet > 1 && r.getWidth() > 12) { g.setColour (col::ink); text (g, juce::String ((int) st.ratchet), b.withHeight (14), juce::Justification::centred, 11.5f, true); }
            }
        }
    }
    juce::String laneHint() const
    {
        switch (lane)
        {
            case kVel: return "How hard each step plays.";
            case kProb: return "Chance a step plays (same every time with the same seed).";
            case kRatchet: return "Repeats inside the step, 1-8.";
            case kCond: return "Click: cycle Always, Fill, Not fill, 1:2, 2:2, 1:4, 4:4, First, Not first.";
            case kFlam: return "A softer grace note before the beat, 0-40 ms.";
            case kNudge: return "Early or late, up to half a step.";
            default: return "Accent: louder and brighter, as set on each voice.";
        }
    }
    float value (const Step& st) const
    {
        switch (lane)
        {
            case kVel: return (float) st.vel / 127.0f;
            case kProb: return (float) st.prob / 100.0f;
            case kRatchet: return (float) st.ratchet / 8.0f;
            case kFlam: return (float) st.flam / 40.0f;
            case kNudge: return (float) st.nudge / 50.0f;
            case kAcc: return (st.flags & kAccent) ? 1.0f : 0.15f;
            default: return 0;
        }
    }
    void setValue (Step& st, float v, bool first)
    {
        v = juce::jlimit (0.0f, 1.0f, v);
        switch (lane)
        {
            case kVel: st.vel = (uint8_t) juce::jlimit (1, 127, juce::roundToInt (v * 127.0f)); break;
            case kProb: st.prob = (uint8_t) juce::jlimit (0, 100, juce::roundToInt (v * 20.0f) * 5); break;
            case kRatchet: st.ratchet = (uint8_t) juce::jlimit (1, 8, 1 + (int) (v * 8.0f)); break;
            case kFlam: st.flam = (uint8_t) juce::jlimit (0, 40, juce::roundToInt (v * 8.0f) * 5); break;
            case kNudge: st.nudge = (int8_t) juce::jlimit (-50, 50, juce::roundToInt ((v * 2.0f - 1.0f) * 10.0f) * 5); break;
            case kAcc: if (first) accSet = ! (st.flags & kAccent); if (accSet) st.flags |= kAccent; else st.flags &= (uint8_t) ~kAccent; break;
            case kCond: if (first) st.cond = (uint8_t) ((st.cond + 1) % kNumConds); break;
            default: break;
        }
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < kNumLanes; ++i) if (tabR (i).contains (e.position)) { lane = i; repaint(); return; }
        if (e.mods.isPopupMenu() && lane == kCond) { condMenu (e); return; }
        last = -1; edit (e, true);
    }
    void mouseDrag (const juce::MouseEvent& e) override { if (lane != kCond) edit (e, false); }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int s = stepAt (e.position.x); if (s < 0) return;
        auto& st = grid.pat.t[(size_t) ctx.voice].s[(size_t) s];
        if (! st.vel) return;
        const auto keep = st.vel; const bool acc = st.flags & kAccent;
        switch (lane) { case kVel: st.vel = 100; break; case kProb: st.prob = 100; break; case kRatchet: st.ratchet = 1; break; case kCond: st.cond = 0; break; case kFlam: st.flam = 0; break; case kNudge: st.nudge = 0; break; default: break; }
        juce::ignoreUnused (keep, acc);
        grid.commit(); repaint();
    }
    int stepAt (float x) const { for (int s = 0; s < grid.columns(); ++s) if (barR (s).getHorizontalRange().contains (x)) return s < grid.pat.length ? s : -1; return -1; }
private:
    Ctx& ctx; PatternGrid& grid; int last = -1; bool accSet = true;
    void edit (const juce::MouseEvent& e, bool first)
    {
        const int s = stepAt (e.position.x);
        if (s < 0) return;
        auto& st = grid.pat.t[(size_t) ctx.voice].s[(size_t) s];
        if (! st.vel) { if (lane == kVel && first) st = Step { 100 }; else return; }
        auto b = barR (s);
        const float v = 1.0f - (e.position.y - b.getY()) / b.getHeight();
        if (lane == kCond || lane == kAcc) { if (s == last) return; setValue (st, v, first || s != last); }
        else setValue (st, v, first);
        last = s;
        grid.commit(); repaint();
    }
    void condMenu (const juce::MouseEvent& e)
    {
        const int s = stepAt (e.position.x); if (s < 0) return;
        juce::PopupMenu m;
        for (int c = 0; c < kNumConds; ++c) m.addItem (1 + c, condName (c), true, grid.pat.t[(size_t) ctx.voice].s[(size_t) s].cond == c);
        juce::Component::SafePointer<StepLanes> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, s] (int r) {
            if (safe == nullptr || r == 0) return;
            auto& st = safe->grid.pat.t[(size_t) safe->ctx.voice].s[(size_t) s];
            if (! st.vel) st.vel = 100;
            st.cond = (uint8_t) (r - 1); safe->grid.commit(); safe->repaint();
        });
    }
};

// El patrón como MIDI: se arrastra al DAW (un archivo .mid en la carpeta temporal).
class MidiDrag : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MidiDrag (Ctx& c) : ctx (c) { setTooltip ("Drag this onto a MIDI track of your DAW: the pattern being edited, with its swing, ratchets and flams, on the selected note map. Click to save a .mid file."); setMouseCursor (juce::MouseCursor::DraggingHandCursor); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.5f);
        g.setColour (over ? col::cell2.brighter (0.06f) : col::cell2); g.fillRoundedRectangle (r, 4.0f);
        juce::Path dash; dash.addRoundedRectangle (r, 4.0f);
        juce::Path dd; const float dl[] = { 4.0f, 3.0f }; juce::PathStrokeType (1.2f).createDashedStroke (dd, dash, dl, 2);
        g.setColour (col::rose); g.fillPath (dd);
        // Un pequeño rollo de piano con las notas del patrón.
        auto ic = r.removeFromLeft (r.getHeight()).reduced (9.0f);
        const auto p = ctx.proc.getPattern (ctx.editPattern());
        g.setColour (col::cell); g.fillRect (ic);
        const int L = std::max (1, (int) p.length);
        for (int t = 0; t < 8; ++t)
            for (int s = 0; s < L; ++s)
                if (p.t[(size_t) t].s[(size_t) s].vel) { g.setColour (col::rose.withAlpha (0.9f)); g.fillRect (ic.getX() + ic.getWidth() * (float) s / (float) L, ic.getBottom() - ic.getHeight() * (float) (t + 1) / 8.0f, std::max (1.5f, ic.getWidth() / (float) L - 0.6f), ic.getHeight() / 8.0f - 0.8f); }
        g.setColour (col::bone); text (g, "DRAG MIDI", r.removeFromTop (r.getHeight() * 0.55f), juce::Justification::bottomLeft, 15.5f, true, 0.1f);
        g.setColour (col::boneDim); text (g, "pattern " + Ctx::patternName (ctx.editPattern()) + " to the DAW", r, juce::Justification::topLeft, 12.5f);
    }
    void mouseEnter (const juce::MouseEvent&) override { over = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { over = false; repaint(); }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragging || e.getDistanceFromDragStart() < 6) return;
        const auto f = ctx.proc.writeDragMidi (ctx.editPattern(), 1);
        if (f == juce::File()) return;
        dragging = true;
        juce::Component::SafePointer<MidiDrag> safe (this);
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ f.getFullPathName() }, false, this, [safe] { if (safe != nullptr) safe->dragging = false; });
    }
    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.getDistanceFromDragStart() >= 6) { dragging = false; return; }
        chooser = std::make_unique<juce::FileChooser> ("Save the pattern as MIDI", juce::File::getSpecialLocation (juce::File::userDesktopDirectory).getChildFile ("Pallor " + Ctx::patternName (ctx.editPattern()).removeCharacters (" ") + ".mid"), "*.mid");
        juce::Component::SafePointer<MidiDrag> safe (this);
        chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::warnAboutOverwriting, [safe] (const juce::FileChooser& fc) {
            if (safe == nullptr) return;
            const auto f = fc.getResult();
            if (f != juce::File()) { safe->ctx.proc.exportMidi (f.withFileExtension ("mid"), safe->ctx.editPattern(), 1); safe->ctx.say ("Saved " + f.withFileExtension ("mid").getFileName()); }
        });
    }
private:
    Ctx& ctx; bool over = false, dragging = false;
    std::unique_ptr<juce::FileChooser> chooser;
};

class PatternPage : public juce::Component
{
public:
    explicit PatternPage (Ctx& c) : ctx (c), grid (c), lanes (c, grid), drag (c)
    {
        addAndMakeVisible (grid); addAndMakeVisible (lanes); addAndMakeVisible (drag);
        grid.onEdited = [this] { lanes.repaint(); drag.repaint(); repaintLeft(); };
        for (int i = 0; i < 16; ++i) groups.items.add (juce::String (i + 1));
        groups.columns = 4; groups.fontH = 16.0f;
        groups.getIndex = [this] { return ctx.group; };
        groups.onPick = [this] (int i) { ctx.setTarget (i, ctx.var); };
        groups.setTooltip ("The 16 patterns. Click to edit one; the pattern that plays is set by Pattern in SONG (or here with Play).");
        addAndMakeVisible (groups);
        vars.items = { "A", "B", "FILL" }; vars.fontH = 15.0f;
        vars.getIndex = [this] { return ctx.var; };
        vars.onPick = [this] (int i) { ctx.setTarget (ctx.group, i); };
        vars.setTooltip ("A and B: two variations of the pattern. Fill: what plays on fill passes.");
        addAndMakeVisible (vars);
        lenChips.items = { "16", "32", "64" };
        lenChips.getIndex = [this] { const int l = grid.pat.length; return l == 16 ? 0 : (l == 32 ? 1 : (l == 64 ? 2 : -1)); };
        lenChips.onPick = [this] (int i) { grid.pat.length = (uint8_t) (16 << i); grid.commit(); };
        addAndMakeVisible (lenChips);
        for (auto* a : { &lenDown, &lenUp, &playBtn, &libBtn, &copyBtn, &pasteBtn, &clearBtn }) addAndMakeVisible (*a);
        lenDown.label = "-"; lenUp.label = "+";
        lenDown.onClick = [this] { grid.pat.length = (uint8_t) std::max (1, grid.pat.length - 1); grid.commit(); };
        lenUp.onClick = [this] { grid.pat.length = (uint8_t) std::min (kMaxSteps, grid.pat.length + 1); grid.commit(); };
        for (int s = 0; s < 5; ++s) scale.items.add (scaleName (s));
        scale.getIndex = [this] { return (int) grid.pat.scale; };
        scale.onPick = [this] (int i) { grid.pat.scale = (uint8_t) i; grid.commit(); };
        scale.setTooltip ("Step length: 1/32, 1/16, 1/8, or triplets.");
        addAndMakeVisible (scale);
        swing.items = { "GLOBAL", "54", "58", "62", "66", "71", "75", "50" }; swing.columns = 4;
        swing.getIndex = [this] { static const int v[] = { 0, 54, 58, 62, 66, 71, 75, 50 }; for (int i = 0; i < 8; ++i) if (v[i] == grid.pat.swing) return i; return -1; };
        swing.onPick = [this] (int i) { static const int v[] = { 0, 54, 58, 62, 66, 71, 75, 50 }; grid.pat.swing = (uint8_t) v[i]; grid.commit(); };
        swing.setTooltip ("This pattern's own swing (percent of each pair; 50 = straight). Global: follow the Swing knob.");
        addAndMakeVisible (swing);
        playBtn.label = "PLAY THIS"; playBtn.setTooltip ("Make this the pattern that plays (Pattern, and Play mode A or B).");
        playBtn.onClick = [this] { ctx.setParam ("group", (float) ctx.group); if (ctx.var < 2) ctx.setParam ("mode", (float) ctx.var); };
        libBtn.label = "LIBRARY..."; libBtn.setTooltip ("Load a pattern set (A, B and Fill) from the library, by genre, into this pattern.");
        libBtn.onClick = [this] { libraryMenu(); };
        copyBtn.label = "COPY"; pasteBtn.label = "PASTE"; clearBtn.label = "CLEAR";
        copyBtn.onClick = [this] { clip = std::make_unique<Pattern> (grid.pat); ctx.say ("Copied " + Ctx::patternName (ctx.editPattern())); };
        pasteBtn.onClick = [this] { if (clip) { grid.pat = *clip; grid.commit(); ctx.say ("Pasted into " + Ctx::patternName (ctx.editPattern())); } };
        clearBtn.onClick = [this] { for (auto& t : grid.pat.t) t = Track(); grid.commit(); };
        follow.label = "Follow"; follow.getState = [this] { return ctx.follow; }; follow.setState = [this] (bool b) { ctx.follow = b; };
        follow.setTooltip ("Follow: the grid shows whatever pattern is playing (song chains, A>B, fills).");
        addAndMakeVisible (follow);
        ctx.onTarget.push_back ([this] { refresh(); });
        ctx.onPatterns.push_back ([this] { refresh(); });
        ctx.onVoice.push_back ([this] { grid.repaint(); lanes.repaint(); });
        refresh();
    }
    void setLane (int l) { lanes.lane = juce::jlimit (0, StepLanes::kNumLanes - 1, l); lanes.repaint(); }
    void refresh() { grid.refresh(); lanes.repaint(); drag.repaint(); repaint(); for (auto* c : std::initializer_list<juce::Component*> { &groups, &vars, &lenChips, &scale, &swing }) c->repaint(); }
    void repaintLeft() { repaint (0, 0, kLeftW, getHeight()); for (auto* c : std::initializer_list<juce::Component*> { &lenChips, &scale, &swing }) c->repaint(); }
    void tick()
    {
        if (ctx.follow && ctx.proc.view.running.load())
        {
            const int p = ctx.proc.view.pattern.load();
            if (p >= 0 && p != ctx.editPattern()) ctx.setTarget (p / 3, p % 3);
        }
        grid.tick();
        const int pp = ctx.proc.view.running.load() ? ctx.proc.view.pattern.load() : -1;
        if (pp != shownPlaying) { shownPlaying = pp; groups.repaint(); repaint (0, 0, kLeftW, getHeight()); }
    }
    static constexpr int kLeftW = 236;
    void resized() override
    {
        auto r = getLocalBounds();
        auto left = r.removeFromLeft (kLeftW).reduced (10, 4);
        left.removeFromTop (18);
        groups.setBounds (left.removeFromTop (108));
        left.removeFromTop (4);
        vars.setBounds (left.removeFromTop (28));
        left.removeFromTop (18);
        auto lr = left.removeFromTop (26);
        lenDown.setBounds (lr.removeFromLeft (26)); lr.removeFromLeft (46); lenUp.setBounds (lr.removeFromLeft (26)); lr.removeFromLeft (6); lenChips.setBounds (lr);
        left.removeFromTop (18);
        scale.setBounds (left.removeFromTop (26));
        left.removeFromTop (18);
        swing.setBounds (left.removeFromTop (48));
        left.removeFromTop (8);
        auto br = left.removeFromTop (26);
        libBtn.setBounds (br.removeFromLeft (br.getWidth() / 2).reduced (1, 0)); playBtn.setBounds (br.reduced (1, 0));
        left.removeFromTop (4);
        auto cr = left.removeFromTop (26); const int w3 = cr.getWidth() / 3;
        copyBtn.setBounds (cr.removeFromLeft (w3).reduced (1, 0)); pasteBtn.setBounds (cr.removeFromLeft (w3).reduced (1, 0)); clearBtn.setBounds (cr.reduced (1, 0));
        left.removeFromTop (8);
        drag.setBounds (left.removeFromTop (44));
        r.removeFromLeft (6);
        auto top = r.removeFromTop (26);
        follow.setBounds (top.removeFromRight (100).reduced (0, 2));
        lanes.setBounds (r.removeFromBottom (122).reduced (0, 2).withTrimmedRight (8));
        grid.setBounds (r.withTrimmedBottom (4).withTrimmedRight (8));
    }
    void paint (juce::Graphics& g) override
    {
        auto left = getLocalBounds().removeFromLeft (kLeftW).reduced (10, 6).toFloat();
        section (g, left.removeFromTop (18), "Pattern " + Ctx::patternName (ctx.editPattern()));
        // Patrón que suena: un punto en su número.
        const int pp = ctx.proc.view.running.load() ? ctx.proc.view.pattern.load() : -1;
        if (pp >= 0)
        {
            auto cr = groups.getBounds().toFloat(); const float cw = cr.getWidth() / 4, chh = cr.getHeight() / 4;
            const int gi = pp / 3;
            g.setColour (col::rose); g.fillEllipse (cr.getX() + cw * (float) (gi % 4) + cw - 11, cr.getY() + chh * (float) (gi / 4) + 6, 5, 5);
        }
        auto y = [&] (const juce::Component& c) { return (float) c.getY() - 17.0f; };
        section (g, { left.getX(), y (lenDown), left.getWidth(), 16 }, "Length");
        g.setColour (col::bone); text (g, juce::String ((int) grid.pat.length), { (float) lenDown.getRight(), (float) lenDown.getY(), 46, 26 }, juce::Justification::centred, 19.0f, true);
        section (g, { left.getX(), y (scale), left.getWidth(), 16 }, "Step");
        section (g, { left.getX(), y (swing), left.getWidth(), 16 }, "Swing");
        g.setColour (col::boneDim);
        text (g, "STEPS", { (float) grid.getX(), 0, 200, 26 }, juce::Justification::centredLeft, 13.0f, true, 0.16f);
        g.setColour (col::boneFaint);
        text (g, "PLAYING " + (pp >= 0 ? Ctx::patternName (pp) : juce::String ("-")), { (float) grid.getX() + 70, 0, 200, 26 }, juce::Justification::centredLeft, 13.0f, false, 0.1f);
    }
private:
    Ctx& ctx;
    PatternGrid grid; StepLanes lanes; MidiDrag drag;
    Chips groups, vars, lenChips, scale, swing;
    Action lenDown, lenUp, playBtn, libBtn, copyBtn, pasteBtn, clearBtn;
    Toggle follow;
    std::unique_ptr<Pattern> clip;
    int shownPlaying = -2;

    void libraryMenu()
    {
        juce::PopupMenu m; std::map<juce::String, juce::PopupMenu> byGenre; juce::StringArray order;
        const auto& lib = ctx.proc.getLibrary();
        for (int i = 0; i < (int) lib.size(); ++i)
        {
            if (! order.contains (lib[(size_t) i].genre)) order.add (lib[(size_t) i].genre);
            byGenre[lib[(size_t) i].genre].addItem (1 + i, lib[(size_t) i].name + "   (" + juce::String (juce::roundToInt (lib[(size_t) i].tempo)) + " BPM)");
        }
        m.addSectionHeader ("Load A, B and Fill into pattern " + juce::String (ctx.group + 1));
        for (auto& gname : order) m.addSubMenu (gname, byGenre[gname]);
        juce::Component::SafePointer<PatternPage> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&libBtn), [safe] (int r) {
            if (safe == nullptr || r <= 0) return;
            safe->ctx.proc.loadLibrarySet (r - 1, safe->ctx.group);
            safe->ctx.say ("Library: " + safe->ctx.proc.getLibrary()[(size_t) (r - 1)].name);
            safe->refresh();
        });
    }
};

} // namespace dh::pal::ui
