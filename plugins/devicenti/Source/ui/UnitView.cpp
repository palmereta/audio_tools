#include "UnitView.h"

namespace dh::dv::ui {

using panel::Kind; using panel::TK;

static const juce::Colour kPointer (0xfff1ede2), kLcdBg (0xff0a1512), kLcdInk (0xffd8efe6), kLcdDim (0xff6f8f86), kLcdAmber (0xffffc06a);

UnitView::UnitView (DeVicentiProcessor& p, PanelState& s) : proc (p), st (s), model (Model::get())
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
    setMouseClickGrabsKeyboardFocus (true);
    displayImg = juce::Image (juce::Image::ARGB, 448, 120, true);
    for (auto& im : opImg) im = juce::Image (juce::Image::ARGB, 592, 88, true);
    knobShown.fill (-1.0f); ledShown.fill (-1); keyLitShown.fill (false);
    st.track = proc.withProject ([] (const Project& pr) { return pr.midi.focus; });
    renderDisplay(); for (int o = 0; o < 4; ++o) renderOpScreen (o);
}

UnitView::~UnitView() { stopTimer(); }

int UnitView::indexOf (const juce::String& id) const
{
    for (int i = 0; i < (int) model.ctls.size(); ++i) if (model.ctls[(size_t) i].id == id) return i;
    return -1;
}

bool UnitView::isKit() const { return proc.withProject ([this] (const Project& pr) { return pr.track[(size_t) st.track].machine == kKit; }); }

// ───────────── hit testing ─────────────
int UnitView::hitControl (juce::Point<float> pos) const
{
    const auto bp = toBg (pos);
    int best = -1; float bestD = 1e9f;
    for (int i = 0; i < (int) model.ctls.size(); ++i)
    {
        const auto& c = model.ctls[(size_t) i];
        if (c.kind == Kind::Led || ! c.boundsPx.contains (bp)) continue;
        float d = 1e9f;
        if (c.kind == Kind::Knob)
        {
            const float R = c.size * 0.5f;
            for (const auto* H : { &c.topInv, &c.baseInv })
            {
                const auto l = H->map (bp);
                d = std::min (d, std::sqrt (l.x * l.x + l.y * l.y) / (R * 1.08f));
            }
        }
        else
        {
            const auto l = c.topInv.map (bp);
            const float hw = c.w * 0.5f + 1.2f, hh = c.h * 0.5f + 1.2f;
            d = std::max (std::abs (l.x) / hw, std::abs (l.y) / hh);
        }
        if (d <= 1.0f && d < bestD) { bestD = d; best = i; }
    }
    return best;
}

// ───────────── values ─────────────
int UnitView::lockIdFor (const panel::Target& t, int& lane) const
{
    lane = isKit() ? juce::jlimit (1, 8, st.lane) : 0;
    switch (t.kind)
    {
        case TK::Sound: return t.sound;
        case TK::OpRatioStep: return opParam (t.op, oRatio);
        case TK::OpEnvMacro: return opParam (t.op, oDec);
        case TK::Auto:
        {
            const juce::String id (t.id);
            static const std::pair<const char*, int> m[] = { { "cutoff", CUTOFF }, { "reso", RESO }, { "fenv", FENV }, { "drive", DRIVE }, { "age", AGE }, { "delay", SEND_DELAY },
                                                             { "reverb", SEND_REVERB }, { "level", LEVEL }, { "opa", A_LEVEL }, { "opb", B_LEVEL }, { "opc", C_LEVEL }, { "opd", D_LEVEL }, { "insert", INS_AMT } };
            for (const auto& kv : m) if (id == kv.first) return kv.second;
            return -1;
        }
        case TK::AutoGlobal:
        {
            const juce::String id (t.id);
            if (id == "delay_fb") return gDelayFeedback;
            if (id == "reverb_decay") return gReverbDecay;
            return -1;
        }
        default: return -1;
    }
}

// Normalised value of a lock id (sound parameters by their table; the two global locks by their host ranges).
static float lockToNorm (int id, float v) { if (id < kNumSoundParams) return toNorm (id, v); if (id == gReverbDecay) return std::log (v / 0.3f) / std::log (100.0f); return v / 1.1f; }
static float lockFromNorm (int id, float u) { u = juce::jlimit (0.0f, 1.0f, u); if (id < kNumSoundParams) return clampParam (id, fromNorm (id, u)); if (id == gReverbDecay) return 0.3f * std::pow (100.0f, u); return u * 1.1f; }

float UnitView::knobNorm (int idx) const
{
    const auto& c = pc (idx);
    const bool f = st.isFunc() && c.func.kind != TK::None;
    const auto& t = f ? c.func : c.main;
    if (st.heldStep >= 0)
    {
        if (juce::String (c.id) == "algorithm") { auto fl = fieldsFor (proc, st, Page::Trig); return fl.empty() ? 0.0f : (float) st.trigField / (float) std::max (1, (int) fl.size() - 1); }
        if (juce::String (c.id) == "data") { auto fl = fieldsFor (proc, st, Page::Trig); return fl.empty() ? 0.0f : fl[(size_t) juce::jlimit (0, (int) fl.size() - 1, st.trigField)].get(); }
        int lane = 0; const int id = lockIdFor (t, lane);
        if (id >= 0)
        {
            const int pat = currentPattern (proc), tr = st.track, step = st.heldStep;
            const float base = id < kNumSoundParams ? proc.soundValue (tr, lane, id) : (id == gReverbDecay ? proc.value ("reverb_decay") : proc.value ("delay_fb"));
            const float v = proc.withProject ([&] (const Project& pr) { return stepedit::lockValue (pr.bank.p[(size_t) pat], tr, lane, step, id, base); });
            if (t.kind == TK::Auto || t.kind == TK::AutoGlobal || t.kind == TK::Sound) return lockToNorm (id, v);
        }
    }
    if (t.kind == TK::Action)
    {
        const juce::String a (t.id);
        if (a == "data") { auto fl = fieldsFor (proc, st, st.page); return fl.empty() ? 0.0f : fl[(size_t) juce::jlimit (0, (int) fl.size() - 1, st.field[(size_t) st.page])].get(); }
        if (a == "page") return (float) juce::jlimit (0, kDisplayPages - 1, (int) st.page) / (float) (kDisplayPages - 1);
        const int pat = currentPattern (proc), tr = st.track;
        if (a == "track_speed") return proc.withProject ([&] (const Project& pr) { return (float) pr.bank.p[(size_t) pat].t[(size_t) tr].speed / 6.0f; });
        if (a == "track_swing") return proc.withProject ([&] (const Project& pr) { return (float) pr.bank.p[(size_t) pat].t[(size_t) tr].swing / 100.0f; });
        if (a == "machine") return proc.withProject ([&] (const Project& pr) { return (float) pr.track[(size_t) tr].machine / 2.0f; });
        return 0.0f;
    }
    return proc.uiNorm (t, st.track, laneForSound());
}

int UnitView::knobSteps (int idx) const
{
    const auto& c = pc (idx);
    const bool f = st.isFunc() && c.func.kind != TK::None;
    const auto& t = f ? c.func : c.main;
    if (st.heldStep >= 0 && (juce::String (c.id) == "algorithm")) return std::max (2, (int) fieldsFor (proc, st, Page::Trig).size());
    if (st.heldStep >= 0 && juce::String (c.id) == "data") { auto fl = fieldsFor (proc, st, Page::Trig); return fl.empty() ? 0 : fl[(size_t) juce::jlimit (0, (int) fl.size() - 1, st.trigField)].steps; }
    switch (t.kind)
    {
        case TK::Sound: { const auto& I = paramInfo()[(size_t) t.sound]; return (I.flags & pfInt) ? (int) (I.max - I.min) + 1 : 0; }
        case TK::OpRatioStep: return (int) panel::ratioTable().size();
        case TK::AutoGlobal: if (auto* prm = proc.apvts.getParameter (t.id)) { if (auto* c2 = dynamic_cast<juce::AudioParameterChoice*> (prm)) return c2->choices.size(); } return 0;
        case TK::Action:
        {
            const juce::String a (t.id);
            if (a == "page") return kDisplayPages;
            if (a == "track_speed") return 7;
            if (a == "machine") return 3;
            if (a == "track_swing") return 101;
            if (a == "data") { auto fl = fieldsFor (proc, st, st.page); return fl.empty() ? 0 : fl[(size_t) juce::jlimit (0, (int) fl.size() - 1, st.field[(size_t) st.page])].steps; }
            return 0;
        }
        default: return 0;
    }
}

juce::String UnitView::knobValueText (int idx) const
{
    const auto& c = pc (idx);
    const bool f = st.isFunc() && c.func.kind != TK::None;
    const auto& t = f ? c.func : c.main;
    const juce::String nm = f ? juce::String (c.funcLabel) : juce::String (c.label);
    if (st.heldStep >= 0 && (juce::String (c.id) == "algorithm" || juce::String (c.id) == "data"))
    {
        auto fl = fieldsFor (proc, st, Page::Trig);
        if (fl.empty()) return {};
        const auto& fd = fl[(size_t) juce::jlimit (0, (int) fl.size() - 1, st.trigField)];
        return fd.name + ": " + fd.text();
    }
    int lane = 0; const int lid = st.heldStep >= 0 ? lockIdFor (t, lane) : -1;
    if (lid >= 0)
    {
        const float u = knobNorm (idx);
        const float v = lockFromNorm (lid, u);
        return "LOCK " + nm + ": " + (lid < kNumSoundParams ? formatSound (lid, v) : juce::String (v, 2));
    }
    switch (t.kind)
    {
        case TK::Auto: if (auto* prm = proc.apvts.getParameter (params::tid (st.track, t.id))) return nm + ": " + prm->getCurrentValueAsText(); break;
        case TK::AutoGlobal: if (auto* prm = proc.apvts.getParameter (t.id)) return nm + ": " + prm->getCurrentValueAsText(); break;
        case TK::Sound: return nm + ": " + formatSound (t.sound, proc.soundValue (st.track, laneForSound(), t.sound));
        case TK::OpRatioStep: return nm + ": " + formatSound (opParam (t.op, oRatio), proc.soundValue (st.track, laneForSound(), opParam (t.op, oRatio)));
        case TK::OpEnvMacro:
        {
            const int L = laneForSound();
            return nm + ": A " + formatSound (opParam (t.op, oAtt), proc.soundValue (st.track, L, opParam (t.op, oAtt))) + "  D " + formatSound (opParam (t.op, oDec), proc.soundValue (st.track, L, opParam (t.op, oDec)));
        }
        case TK::Action:
        {
            const juce::String a (t.id);
            const int pat = currentPattern (proc), tr = st.track;
            if (a == "data") { auto fl = fieldsFor (proc, st, st.page); if (fl.empty()) return {}; const auto& fd = fl[(size_t) juce::jlimit (0, (int) fl.size() - 1, st.field[(size_t) st.page])]; return fd.name + ": " + fd.text(); }
            if (a == "page") return juce::String ("PAGE: ") + pageName (st.page);
            if (a == "track_speed") return "SPEED: " + juce::String (speedName (proc.withProject ([&] (const Project& pr) { return (int) pr.bank.p[(size_t) pat].t[(size_t) tr].speed; })));
            if (a == "track_swing") return "TRK SWING: " + juce::String (proc.withProject ([&] (const Project& pr) { return (int) pr.bank.p[(size_t) pat].t[(size_t) tr].swing; })) + " %";
            if (a == "machine") { static const char* n[3] = { "TONE", "KIT", "MIDI" }; return juce::String ("MACHINE: ") + n[juce::jlimit (0, 2, proc.withProject ([&] (const Project& pr) { return pr.track[(size_t) tr].machine; }))]; }
            break;
        }
        default: break;
    }
    return nm;
}

void UnitView::knobGesture (int idx, bool begin)
{
    const auto& c = pc (idx);
    const bool f = st.isFunc() && c.func.kind != TK::None;
    if (st.heldStep >= 0) return;
    const auto& t = f ? c.func : c.main;
    if (t.kind == TK::Action && juce::String (t.id) == "data")
    {
        auto fl = fieldsFor (proc, st, st.page);
        if (! fl.empty()) if (auto& g = fl[(size_t) juce::jlimit (0, (int) fl.size() - 1, st.field[(size_t) st.page])].gesture) g (begin);
        return;
    }
    proc.uiGesture (t, st.track, begin);
}

// delta: normalised change (continuous) or ±steps (stepped).
void UnitView::knobTurn (int idx, float delta, bool)
{
    const auto& c = pc (idx);
    const bool f = st.isFunc() && c.func.kind != TK::None;
    const auto& t = f ? c.func : c.main;
    const int steps = knobSteps (idx);
    auto stepped = [steps] (float u, float d) { const int n = std::max (2, steps); const int k = juce::jlimit (0, n - 1, juce::roundToInt (u * (float) (n - 1)) + (int) d); return (float) k / (float) (n - 1); };
    const float u0 = knobNorm (idx);
    const float u1 = steps > 1 ? stepped (u0, delta) : juce::jlimit (0.0f, 1.0f, u0 + delta);
    if (st.heldStep >= 0)
    {
        if (juce::String (c.id) == "algorithm")
        {
            auto fl = fieldsFor (proc, st, Page::Trig);
            st.trigField = juce::jlimit (0, std::max (0, (int) fl.size() - 1), st.trigField + (int) delta);
            st.page = Page::Trig; st.field[(size_t) Page::Trig] = st.trigField;
            if (! fl.empty()) flash (fl[(size_t) st.trigField].name, fl[(size_t) st.trigField].text());
            return;
        }
        if (juce::String (c.id) == "data")
        {
            auto fl = fieldsFor (proc, st, Page::Trig);
            if (fl.empty()) return;
            auto& fd = fl[(size_t) juce::jlimit (0, (int) fl.size() - 1, st.trigField)];
            fd.set (u1); st.heldLocked = true;
            flash (fd.name, fd.text());
            return;
        }
        int lane = 0; const int id = lockIdFor (t, lane);
        if (id >= 0)
        {
            const int pat = currentPattern (proc), tr = st.track, step = st.heldStep;
            float v;
            if (t.kind == TK::OpRatioStep) v = panel::ratioFromKnob (u1);
            else if (t.kind == TK::OpEnvMacro) v = panel::envMacro (u1).d;
            else v = lockFromNorm (id, u1);
            proc.editProject ([&] (Project& pr) {
                Pattern& p = pr.bank.p[(size_t) pat];
                Step& s = p.t[(size_t) tr].lane[(size_t) lane][(size_t) step];
                if (! s.on()) { s = Step(); s.trig = 2; }
                stepedit::setLock (p, tr, lane, step, id, v);
            }, 0, 1u << pat);
            st.heldLocked = true;
            flash ("LOCK " + juce::String (f ? c.funcLabel : c.label), id < kNumSoundParams ? formatSound (id, v) : juce::String (v, 2));
            return;
        }
    }
    if (t.kind == TK::Action)
    {
        const juce::String a (t.id);
        const int pat = currentPattern (proc), tr = st.track;
        if (a == "data")
        {
            auto fl = fieldsFor (proc, st, st.page);
            if (fl.empty()) return;
            auto& fd = fl[(size_t) juce::jlimit (0, (int) fl.size() - 1, st.field[(size_t) st.page])];
            if (! fd.enabled) return;
            fd.set (fd.steps > 1 ? stepped (fd.get(), delta) : u1);
            flash (fd.name, fd.text());
        }
        else if (a == "page")
        {
            st.page = (Page) juce::jlimit (0, kDisplayPages - 1, (int) st.page + (int) delta);
            auto fl = fieldsFor (proc, st, st.page);
            flash (pageName (st.page), fl.empty() ? juce::String() : fl[(size_t) juce::jlimit (0, (int) fl.size() - 1, st.field[(size_t) st.page])].name);
        }
        else if (a == "track_speed") proc.editProject ([&] (Project& pr) { pr.bank.p[(size_t) pat].t[(size_t) tr].speed = (uint8_t) juce::roundToInt (u1 * 6.0f); }, 0, 1u << pat);
        else if (a == "track_swing") proc.editProject ([&] (Project& pr) { pr.bank.p[(size_t) pat].t[(size_t) tr].swing = (uint8_t) juce::roundToInt (u1 * 100.0f); }, 0, 1u << pat);
        else if (a == "machine") proc.editProject ([&] (Project& pr) { pr.track[(size_t) tr].machine = juce::roundToInt (u1 * 2.0f); }, DeVicentiProcessor::dTracks);
        if (a != "data" && a != "page") flash (knobValueText (idx).upToFirstOccurrenceOf (":", false, false), knobValueText (idx).fromFirstOccurrenceOf (": ", false, false));
        return;
    }
    if (t.kind == TK::None) return;
    proc.uiSet (t, st.track, u1, laneForSound());
    if (t.op >= 0) st.op = t.op;
    const auto txt = knobValueText (idx);
    flash (txt.upToFirstOccurrenceOf (":", false, false), txt.fromFirstOccurrenceOf (": ", false, false));
}

// ───────────── keys ─────────────
void UnitView::setTrack (int t)
{
    st.track = juce::jlimit (0, kTracks - 1, t);
    proc.editProject ([this] (Project& pr) { pr.midi.focus = st.track; }, DeVicentiProcessor::dMidi);
    st.heldStep = -1;
}

int UnitView::playNoteForKey (int i) const
{
    const int pat = currentPattern (proc);
    return proc.withProject ([&] (const Project& pr) {
        const auto& p = pr.bank.p[(size_t) pat];
        if (pr.track[(size_t) st.track].machine == kKit)
            return juce::jlimit (0, 127, (int) std::lround (pr.sounds[(size_t) st.track][(size_t) (i + 1)][LANE_NOTE]));
        return juce::jlimit (0, 127, degreeNote (48 + p.root, scaleOf (p.scale), i));
    });
}

void UnitView::keyDown (int idx, const juce::ModifierKeys& mods)
{
    const juce::String id = model.ctls[(size_t) idx].id;
    const bool f = st.isFunc();
    keysHeld.insert (idx);
    keyDownAt = juce::Time::getMillisecondCounter();
    if (id.startsWith ("track"))
    {
        const int t = id.getTrailingIntValue() - 1;
        bool otherTrackHeld = false;
        for (int k : keysHeld) if (k != idx && model.ctls[(size_t) k].id.startsWith ("track")) otherTrackHeld = true;
        if (otherTrackHeld || (f && mods.isAltDown()))
        {
            proc.editProject ([t] (Project& pr) { pr.track[(size_t) t].solo = ! pr.track[(size_t) t].solo; }, DeVicentiProcessor::dTracks);
            flash ("SOLO T" + juce::String (t + 1), proc.withProject ([t] (const Project& pr) { return pr.track[(size_t) t].solo; }) ? "ON" : "OFF");
        }
        else if (f)
        {
            proc.editProject ([t] (Project& pr) { pr.track[(size_t) t].mute = ! pr.track[(size_t) t].mute; }, DeVicentiProcessor::dTracks);
            flash ("MUTE T" + juce::String (t + 1), proc.withProject ([t] (const Project& pr) { return pr.track[(size_t) t].mute; }) ? "ON" : "OFF");
        }
        else { setTrack (t); static const char* n[3] = { "TONE", "KIT", "MIDI" }; flash ("TRACK " + juce::String (t + 1), n[juce::jlimit (0, 2, proc.withProject ([t] (const Project& pr) { return pr.track[(size_t) t].machine; }))]); }
    }
    else if (id == "play")
    {
        bool recHeld = false; for (int k : keysHeld) if (model.ctls[(size_t) k].id == "rec") recHeld = true;
        if (recHeld) { proc.rec = true; flash ("LIVE REC", "ON"); }
        if (! proc.isStandalone()) { flash ("PLAY", "The host's transport runs the sequencer"); }
        else if (f) { proc.standalonePlay = false; juce::Timer::callAfterDelay (30, [p = &proc] { p->standalonePlay = true; }); flash ("RESTART", "from step 1"); }
        else { proc.standalonePlay = ! proc.standalonePlay.load(); flash (proc.standalonePlay ? "PLAY" : "PAUSE", ""); }
    }
    else if (id == "stop")
    {
        if (f) { proc.panicRequest = true; flash ("PANIC", "all voices and MIDI off"); }
        else
        {
            if (proc.isStandalone() && proc.standalonePlay) proc.standalonePlay = false;
            else proc.notesOffRequest = true;
            proc.rec = false;
            flash ("STOP", "");
        }
    }
    else if (id == "rec")
    {
        if (f) { st.stepRec = ! st.stepRec; flash ("STEP REC", st.stepRec ? "ON" : "OFF"); }
        else { st.gridRec = ! st.gridRec; if (! st.gridRec) { proc.rec = false; st.heldStep = -1; } flash ("GRID REC", st.gridRec ? "ON" : "OFF"); }
    }
    else if (id == "chord")
    {
        if (f) { st.page = Page::ChordScale; flash ("CHORD/SCALE", "page"); }
        else if (chordLatched) { chordLatched = false; finishChordCapture (true); }
        else
        {
            // Hold CHORD and play (MIDI or the STEP keys): the notes become a chord memory when CHORD is let go.
            // A short click without notes toggles chord mode; Alt-click keeps the capture open until the next click.
            for (auto& c : proc.captured) c.store (0);
            proc.chordCapture = true;
            chordLatched = mods.isAltDown();
            if (chordLatched) flash ("CHORD MEMORY", "play the notes, then click CHORD");
        }
    }
    else if (id == "func") { st.funcLatched = ! st.funcLatched; }
    else if (id.startsWith ("step")) stepKey (id.getTrailingIntValue() - 1, true, mods);
    repaint();
}

void UnitView::finishChordCapture (bool storeOnly)
{
    proc.chordCapture = false;
    std::vector<int> notes;
    for (int n = 0; n < 128; ++n) if (proc.captured[(size_t) n].load()) notes.push_back (n);
    if (notes.size() >= 2)
    {
        // Intervals from the lowest note, into the first empty slot (or the one after the last stored).
        const int slot = proc.withProject ([this] (const Project& pr) {
            for (int k = 0; k < kChordMems; ++k) if (pr.chordMem[(size_t) k].n == 0) return k;
            return (lastChordSlot + 1) % kChordMems; });
        lastChordSlot = slot;
        juce::String txt;
        proc.editProject ([&] (Project& pr) {
            auto& m = pr.chordMem[(size_t) slot]; m = ChordMem();
            for (size_t i = 0; i < notes.size() && m.n < kMaxNotes; ++i) { m.iv[m.n++] = (int8_t) (notes[i] - notes[0]); txt << (i ? " " : "") << (notes[i] - notes[0]); }
        }, DeVicentiProcessor::dChordMem);
        flash ("CHORD MEMORY " + juce::String (slot + 1), txt);
        return;
    }
    if (storeOnly) { flash ("CHORD MEMORY", "no chord (play two notes or more)"); return; }
    const int t = st.track;
    proc.editProject ([t] (Project& pr) { pr.track[(size_t) t].chordPlay = ! pr.track[(size_t) t].chordPlay; }, DeVicentiProcessor::dTracks);
    flash ("CHORD MODE", proc.withProject ([t] (const Project& pr) { return pr.track[(size_t) t].chordPlay; }) ? "ON" : "OFF");
}

void UnitView::keyUp (int idx, bool cancelled)
{
    const juce::String id = model.ctls[(size_t) idx].id;
    keysHeld.erase (idx);
    if (id == "chord" && ! cancelled && proc.chordCapture.load() && ! chordLatched) finishChordCapture (false);
    if (id.startsWith ("step") && ! cancelled) stepKey (id.getTrailingIntValue() - 1, false, {});
    if (st.funcLatched && id != "func" && ! id.startsWith ("step") && ! id.startsWith ("track")) {}      // stays latched until FUNC is clicked again
    repaint();
}

void UnitView::stepKey (int i, bool down, const juce::ModifierKeys& mods)
{
    const int pat = currentPattern (proc);
    const int abs = st.stepPage * 16 + i;
    const bool kit = isKit();
    const int lane = kit ? juce::jlimit (1, 8, st.lane) : 0;
    const int tr = st.track;
    const int ch = proc.withProject ([tr] (const Project& pr) { return pr.midi.inMode == 0 ? 1 : pr.track[(size_t) tr].inChannel; });
    if (down)
    {
        if (st.isFunc())
        {
            bool stopHeld = false; for (int k : keysHeld) if (model.ctls[(size_t) k].id == "stop") stopHeld = true;
            if (i >= 8 && (stopHeld || mods.isAltDown()))
            {
                const int target = (pat / 8) * 8 + (i - 8);
                proc.jumpToPattern (target); flash ("JUMP", "pattern " + juce::String (target + 1));
            }
            else if (i < 4) { st.stepPage = i; flash ("STEPS", juce::String (i * 16 + 1) + "-" + juce::String (i * 16 + 16)); }
            else if (i == 4) { st.clip = proc.withProject ([&] (const Project& pr) { return stepedit::copyPage (pr.bank.p[(size_t) pat], tr, lane, st.stepPage); }); flash ("COPY", "steps " + juce::String (st.stepPage * 16 + 1) + "-" + juce::String (st.stepPage * 16 + 16)); }
            else if (i == 5) { proc.editProject ([&] (Project& pr) { stepedit::pastePage (pr.bank.p[(size_t) pat], tr, lane, st.stepPage, st.clip); }, 0, 1u << pat); flash ("PASTE", st.clip.valid ? "done" : "nothing copied"); }
            else if (i == 6) { proc.editProject ([&] (Project& pr) { stepedit::clearPage (pr.bank.p[(size_t) pat], tr, lane, st.stepPage); }, 0, 1u << pat); flash ("CLEAR", "steps " + juce::String (st.stepPage * 16 + 1) + "-" + juce::String (st.stepPage * 16 + 16)); }
            else if (i == 7) { proc.fill = true; fillHeld = true; flash ("FILL", "while held"); }
            else
            {
                const int target = (pat / 8) * 8 + (i - 8);
                if (target == pat) { const int other = (pat + 8) % 16; proc.setValue ("pattern", (float) (other + 1)); flash ("PATTERN", juce::String (other + 1) + " (bank)"); }
                else { proc.setValue ("pattern", (float) (target + 1)); flash ("PATTERN", juce::String (target + 1) + (playing() ? " (next)" : "")); }
            }
            return;
        }
        if (st.gridRec)
        {
            if (mods.isAltDown() || mods.isPopupMenu())
            {
                st.heldStep = st.heldStep == abs ? -1 : abs; st.heldLocked = false;
                if (st.heldStep >= 0) { st.page = Page::Trig; flash ("STEP " + juce::String (abs + 1), "held: turn knobs to lock"); }
                pressedKey = -2;           // consumed
            }
            return;
        }
        // Play mode: the keyboard (TONE), the lanes (KIT 1-8) and lane select (KIT 9-16).
        if (kit && i >= 8) { st.lane = i - 7; flash ("LANE", juce::String (st.lane)); return; }
        const int note = playNoteForKey (i);
        proc.uiMidi ((uint8_t) (0x90 | ((ch - 1) & 15)), (uint8_t) note, 100);
        if (proc.chordCapture.load()) proc.captured[(size_t) (note & 127)].store (1);     // also without a running audio device
        if (st.stepRec && ! kit)
        {
            // Step record: the note goes to the step under the cursor (the held step, else the first free one).
            const int at = st.heldStep >= 0 ? st.heldStep : abs;
            proc.editProject ([&] (Project& pr) { Step& s = pr.bank.p[(size_t) pat].t[(size_t) tr].lane[0][(size_t) at]; s = Step(); s.trig = 1; s.nNotes = 1; s.notes[0] = (int8_t) note; }, 0, 1u << pat);
            st.heldStep = (at + 1) % std::max (1, proc.withProject ([&] (const Project& pr) { return (int) pr.bank.p[(size_t) pat].t[(size_t) tr].length; }));
        }
        return;
    }
    // Release.
    if (fillHeld) { proc.fill = false; fillHeld = false; }
    if (st.isFunc()) return;
    if (st.gridRec)
    {
        if (pressedKey == -2) { pressedKey = -1; return; }
        const bool longPress = juce::Time::getMillisecondCounter() - keyDownAt > 450;
        if (st.heldStep == abs && ! longPress) { st.heldStep = -1; return; }            // a second click releases a held step
        if (longPress) { st.heldStep = abs; st.heldLocked = false; st.page = Page::Trig; return; }
        proc.editProject ([&] (Project& pr) { stepedit::toggle (pr.bank.p[(size_t) pat], tr, lane, abs); }, 0, 1u << pat);
        return;
    }
    if (! (kit && i >= 8))
        proc.uiMidi ((uint8_t) (0x80 | ((ch - 1) & 15)), (uint8_t) playNoteForKey (i), 0);
}

// ───────────── mouse ─────────────
void UnitView::mouseDown (const juce::MouseEvent& e)
{
    const int idx = hitControl (e.position);
    dragCtl = -1; pressedKey = -1; dragMoved = false;
    if (idx < 0) return;
    const auto kind = model.ctls[(size_t) idx].kind;
    if (e.mods.isPopupMenu() && ! (kind == Kind::Key && model.ctls[(size_t) idx].id.startsWith ("step") && st.gridRec)) { showContextMenu (idx); return; }
    if (kind == Kind::Knob)
    {
        if (learnMode) { armLearnFor (idx); return; }
        dragCtl = idx; dragLastY = e.getPosition().y; dragAccum = 0.0f; dragStartNorm = knobNorm (idx);
        knobGesture (idx, true);
        flash (knobValueText (idx).upToFirstOccurrenceOf (":", false, false), knobValueText (idx).fromFirstOccurrenceOf (": ", false, false));
    }
    else if (kind == Kind::Key) { pressedKey = idx; keyDown (idx, e.mods); }
    else if (kind == Kind::Display)
    {
        // The screen: the left half steps back through the fields of the page, the right half forward.
        const auto l = model.ctls[(size_t) idx].topInv.map (toBg (e.position));
        if (model.ctls[(size_t) idx].id == "display")
        {
            const Page pg = st.heldStep >= 0 ? Page::Trig : st.page;
            auto fl = fieldsFor (proc, st, pg);
            int& fi = pg == Page::Trig ? st.trigField : st.field[(size_t) pg];
            fi = juce::jlimit (0, std::max (0, (int) fl.size() - 1), fi + (l.x < 0 ? -1 : 1));
            if (pg == Page::Trig) st.field[(size_t) Page::Trig] = fi;
            if (! fl.empty()) flash (fl[(size_t) fi].name, fl[(size_t) fi].text());
        }
        else { st.op = model.ctls[(size_t) idx].id.getLastCharacter() - 'a'; st.page = Page::Ops; flash ("OPERATOR " + juce::String::charToString ((juce::juce_wchar) ('A' + st.op)), "OPS page"); }
    }
    repaint();
}

void UnitView::mouseDrag (const juce::MouseEvent& e)
{
    if (dragCtl < 0) return;
    const int y = e.getPosition().y; const int dy = dragLastY - y; dragLastY = y;
    if (dy == 0) return;
    dragMoved = true;
    const int steps = knobSteps (dragCtl);
    if (steps > 1)
    {
        dragAccum += (float) dy;
        const float per = steps > 40 ? 4.0f : 12.0f;
        while (std::abs (dragAccum) >= per) { const float s = dragAccum > 0 ? 1.0f : -1.0f; knobTurn (dragCtl, s, false); dragAccum -= s * per; }
    }
    else knobTurn (dragCtl, (float) dy / (e.mods.isShiftDown() ? 1500.0f : 260.0f), e.mods.isShiftDown());
    repaint();
}

void UnitView::mouseUp (const juce::MouseEvent&)
{
    if (dragCtl >= 0) { knobGesture (dragCtl, false); dragCtl = -1; }
    if (pressedKey >= 0 || pressedKey == -2)
    {
        if (pressedKey >= 0) keyUp (pressedKey, false);
        else { for (auto it = keysHeld.begin(); it != keysHeld.end();) { if (model.ctls[(size_t) *it].id.startsWith ("step")) it = keysHeld.erase (it); else ++it; } pressedKey = -1; }
    }
    pressedKey = -1;
    repaint();
}

void UnitView::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Double-click a knob: back to its default (host parameters and sound parameters).
    const int idx = hitControl (e.position);
    if (idx < 0 || model.ctls[(size_t) idx].kind != Kind::Knob || st.heldStep >= 0) return;
    const auto& c = pc (idx);
    const auto& t = st.isFunc() && c.func.kind != TK::None ? c.func : c.main;
    if (t.kind == TK::Auto || t.kind == TK::AutoGlobal)
    {
        if (auto* prm = proc.apvts.getParameter (t.kind == TK::Auto ? params::tid (st.track, t.id) : juce::String (t.id))) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture(); }
    }
    else if (t.kind == TK::Sound) proc.setSound (st.track, laneForSound(), t.sound, paramInfo()[(size_t) t.sound].def);
    repaint();
}

void UnitView::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int idx = hitControl (e.position);
    if (idx < 0 || model.ctls[(size_t) idx].kind != Kind::Knob) return;
    const int steps = knobSteps (idx);
    const float d = (w.isReversed ? -1.0f : 1.0f) * (std::abs (w.deltaY) > std::abs (w.deltaX) ? w.deltaY : -w.deltaX);
    if (steps > 1) { static float acc = 0; acc += d * 8.0f; while (std::abs (acc) >= 1.0f) { const float s = acc > 0 ? 1.0f : -1.0f; knobTurn (idx, s, false); acc -= s; } }
    else { knobGesture (idx, true); knobTurn (idx, d * (e.mods.isShiftDown() ? 0.02f : 0.12f), false); knobGesture (idx, false); }
    repaint();
}

void UnitView::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitControl (e.position);
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h >= 0 && model.ctls[(size_t) h].kind != Kind::Led ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        // The tooltip names both layers (the small FUNC labels are the hardest thing to read on a 1x screen).
        juce::String tip;
        if (h >= 0)
        {
            const auto& c = pc (h);
            if (c.kind == Kind::Knob || c.kind == Kind::Key)
            {
                tip = juce::String (c.label).isNotEmpty() ? juce::String (c.label) : juce::String (c.id);
                if (juce::String (c.id) == "chord") tip << " (hold and play notes: chord memory)";
                if (juce::String (c.funcLabel).isNotEmpty()) tip << "   |   FUNC: " << c.funcLabel;
                if (c.cc >= 0) tip << "   |   CC " << c.cc << (c.ccFunc >= 0 ? " / " + juce::String (c.ccFunc) : juce::String());
                if (c.kind == Kind::Knob) tip << "\n" << knobValueText (h);
            }
            else if (c.kind == Kind::Display) tip = "Click the left or right half to step through the fields; right-click for the pages";
        }
        setTooltip (tip);
    }
}

bool UnitView::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { st.heldStep = -1; st.funcLatched = false; repaint(); return true; }
    return false;
}

void UnitView::modifierKeysChanged (const juce::ModifierKeys& m)
{
    if (st.func != m.isShiftDown() && dragCtl < 0) { st.func = m.isShiftDown(); repaint(); }
}

void UnitView::armLearnFor (int idx)
{
    const auto& c = pc (idx);
    const auto& t = st.isFunc() && c.func.kind != TK::None ? c.func : c.main;
    juce::String tid;
    if (t.kind == TK::Auto || t.kind == TK::AutoGlobal) tid = t.id; else if (t.kind == TK::Sound) tid = "s:" + juce::String (paramInfo()[(size_t) t.sound].key);
    if (tid.isNotEmpty()) { proc.armLearn (tid); flash ("MIDI LEARN", juce::String (st.isFunc() ? c.funcLabel : c.label) + ": move a controller"); }
    learnMode = false; if (onLearnArmed) onLearnArmed();
}

void UnitView::showContextMenu (int idx)
{
    juce::PopupMenu m;
    const auto& c = model.ctls[(size_t) idx];
    const auto& p = pc (idx);
    if (c.kind == Kind::Knob)
    {
        const auto& t = st.isFunc() && p.func.kind != TK::None ? p.func : p.main;
        m.addSectionHeader (juce::String (st.isFunc() ? p.funcLabel : p.label) + (p.cc >= 0 ? "  (CC " + juce::String (st.isFunc() ? p.ccFunc : p.cc) + ")" : juce::String()));
        m.addItem (1, "MIDI learn", t.kind == TK::Auto || t.kind == TK::AutoGlobal || t.kind == TK::Sound);
        m.addItem (2, "Clear learned CCs");
        m.addItem (3, "Default value", t.kind == TK::Auto || t.kind == TK::AutoGlobal || t.kind == TK::Sound);
        if (st.heldStep >= 0) m.addItem (4, "Remove this lock from the held step");
    }
    else if (c.id.startsWith ("track"))
    {
        const int t = c.id.getTrailingIntValue() - 1;
        const bool mu = proc.withProject ([t] (const Project& pr) { return pr.track[(size_t) t].mute; }), so = proc.withProject ([t] (const Project& pr) { return pr.track[(size_t) t].solo; });
        m.addItem (10, "Mute", true, mu); m.addItem (11, "Solo", true, so);
    }
    else if (c.id.startsWith ("step"))
    {
        const int abs = st.stepPage * 16 + c.id.getTrailingIntValue() - 1;
        m.addItem (20, st.heldStep == abs ? "Release the held step" : "Hold this step (locks, trig fields)");
        m.addItem (21, "Clear its locks");
    }
    else if (c.kind == Kind::Display)
        for (int pg = 0; pg < (int) Page::kCount; ++pg) m.addItem (100 + pg, juce::String ("Open the ") + pageName ((Page) pg) + " page");
    juce::Component::SafePointer<UnitView> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(), [safe, idx] (int r) {
        if (safe == nullptr || r == 0) return;
        auto& v = *safe;
        const auto& c2 = v.model.ctls[(size_t) idx];
        const auto& p2 = v.pc (idx);
        const auto& t = v.st.isFunc() && p2.func.kind != TK::None ? p2.func : p2.main;
        if (r == 1) v.armLearnFor (idx);
        else if (r == 2) { v.proc.clearLearn(); v.flash ("MIDI LEARN", "cleared"); }
        else if (r == 3) { if (t.kind == TK::Sound) v.proc.setSound (v.st.track, v.laneForSound(), t.sound, paramInfo()[(size_t) t.sound].def); else if (auto* prm = v.proc.apvts.getParameter (t.kind == TK::Auto ? params::tid (v.st.track, t.id) : juce::String (t.id))) prm->setValueNotifyingHost (prm->getDefaultValue()); }
        else if (r == 4) { int lane = 0; const int id = v.lockIdFor (t, lane); const int pat = currentPattern (v.proc), tr = v.st.track, step = v.st.heldStep; if (id >= 0) v.proc.editProject ([&] (Project& pr) { stepedit::clearLocks (pr.bank.p[(size_t) pat], tr, lane, step, id); }, 0, 1u << pat); }
        else if (r == 10 || r == 11)
        {
            const int tk = c2.id.getTrailingIntValue() - 1;
            v.proc.editProject ([tk, r] (Project& pr) { if (r == 10) pr.track[(size_t) tk].mute = ! pr.track[(size_t) tk].mute; else pr.track[(size_t) tk].solo = ! pr.track[(size_t) tk].solo; }, DeVicentiProcessor::dTracks);
        }
        else if (r == 20) { const int abs = v.st.stepPage * 16 + c2.id.getTrailingIntValue() - 1; v.st.heldStep = v.st.heldStep == abs ? -1 : abs; v.st.heldLocked = false; if (v.st.heldStep >= 0) v.st.page = Page::Trig; }
        else if (r == 21) { const int abs = v.st.stepPage * 16 + c2.id.getTrailingIntValue() - 1; const int pat = currentPattern (v.proc), tr = v.st.track; const int lane = v.isKit() ? juce::jlimit (1, 8, v.st.lane) : 0; v.proc.editProject ([&] (Project& pr) { stepedit::clearLocks (pr.bank.p[(size_t) pat], tr, lane, abs); }, 0, 1u << pat); }
        else if (r >= 100 && v.onOpenPage) v.onOpenPage ((Page) (r - 100));
        v.repaint();
    });
}

// ───────────── state for drawing ─────────────
int UnitView::ledState (int i) const
{
    const int pat = currentPattern (proc);
    const int abs = st.stepPage * 16 + i;
    const int tr = st.track;
    const bool kit = isKit();
    const int lane = kit ? juce::jlimit (1, 8, st.lane) : 0;
    const int cur = currentStepOf (tr);
    if (playing() && cur == abs && (fakePlayhead >= 0 || proc.viewPattern.load() == pat)) return 3;
    const auto info = proc.withProject ([&] (const Project& pr) {
        const auto& tp = pr.bank.p[(size_t) pat].t[(size_t) tr];
        const int len = kit && tp.laneLen[(size_t) lane] > 0 ? tp.laneLen[(size_t) lane] : tp.length;
        const Step& s = tp.lane[(size_t) lane][(size_t) abs];
        return std::make_tuple (s.on(), s.cond != cNone || s.prob < 100, s.lockCount > 0, abs < len);
    });
    if (st.heldStep == abs) return blinkPhase % 6 < 3 ? 2 : 0;
    if (! std::get<0> (info)) return std::get<3> (info) ? 0 : 0;
    if (std::get<2> (info) && blinkPhase % 16 >= 12) return 1;          // locked steps blink
    return std::get<1> (info) ? 1 : 2;
}

bool UnitView::keyLit (int idx) const
{
    const juce::String& id = model.ctls[(size_t) idx].id;
    if (id.startsWith ("track")) return id.getTrailingIntValue() - 1 == st.track;
    if (id == "play") return playing() || (proc.isStandalone() && proc.standalonePlay.load());
    if (id == "rec") return st.gridRec || (proc.rec.load() && blinkPhase % 10 < 5);
    if (id == "chord" && proc.chordCapture.load()) return blinkPhase % 8 < 4;
    if (id == "chord") return proc.withProject ([this] (const Project& pr) { return pr.track[(size_t) st.track].chordPlay; });
    if (id == "func") return st.isFunc();
    if (id.startsWith ("step")) return st.heldStep == st.stepPage * 16 + id.getTrailingIntValue() - 1 || (st.isFunc() && id.getTrailingIntValue() - 1 == st.stepPage);
    return false;
}

// ───────────── drawing ─────────────
void UnitView::drawSprite (juce::Graphics& g, const Ctl& c, const char* which, float alpha) const
{
    auto it = c.sprites.find (which);
    if (it == c.sprites.end() || ! model.atlas.isValid()) return;
    const float s = pxScale();
    const auto& a = it->second;
    // Exactly over its crop of the background (same scale as the background, no integer rounding).
    juce::Graphics::ScopedSaveState ss (g);
    g.reduceClipRegion ((c.crop.toFloat() * s).getSmallestIntegerContainer());
    g.setOpacity (alpha);
    g.drawImageTransformed (model.atlas.getClippedImage (a), juce::AffineTransform::scale (s).translated ((float) c.crop.getX() * s, (float) c.crop.getY() * s));
}

void UnitView::drawPointer (juce::Graphics& g, const Ctl& c, float norm) const
{
    const float s = pxScale();
    const float R = c.r;
    const float a = juce::degreesToRadians (-135.0f + 270.0f * juce::jlimit (0.0f, 1.0f, norm));
    const float sx = std::sin (a), cy = -std::cos (a), nx = std::cos (a), ny = std::sin (a);
    const float r0 = 0.32f * R, r1 = 0.96f * R, w = 0.045f * c.size * 0.5f + 0.18f;
    juce::Path p;
    auto P = [&] (float rho, float off) { return c.top.map (sx * rho + nx * off, cy * rho + ny * off) * s; };
    p.startNewSubPath (P (r0, -w)); p.lineTo (P (r1, -w)); p.lineTo (P (r1, w)); p.lineTo (P (r0, w)); p.closeSubPath();
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.strokePath (p, juce::PathStrokeType (1.0f));
    g.setColour (kPointer);
    g.fillPath (p);
}

void UnitView::drawWarped (juce::Graphics& g, const juce::Image& img, const Ctl& c) const
{
    const float s = pxScale();
    const int NX = 6, NY = 2;
    const float W = (float) img.getWidth(), H = (float) img.getHeight();
    auto scr = [&] (float ix, float iy) { return c.top.map ((ix / W - 0.5f) * c.w, (iy / H - 0.5f) * c.h) * s; };
    for (int yi = 0; yi < NY; ++yi)
        for (int xi = 0; xi < NX; ++xi)
        {
            const float x0 = W * (float) xi / NX, x1 = W * (float) (xi + 1) / NX, y0 = H * (float) yi / NY, y1 = H * (float) (yi + 1) / NY;
            const juce::Point<float> I[4] = { { x0, y0 }, { x1, y0 }, { x1, y1 }, { x0, y1 } };
            juce::Point<float> S[4]; for (int k = 0; k < 4; ++k) S[k] = scr (I[k].x, I[k].y);
            for (int tri = 0; tri < 2; ++tri)
            {
                const int a = 0, b = tri == 0 ? 1 : 2, cc = tri == 0 ? 2 : 3;
                const auto A = juce::AffineTransform::fromTargetPoints (I[a].x, I[a].y, S[a].x, S[a].y, I[b].x, I[b].y, S[b].x, S[b].y, I[cc].x, I[cc].y, S[cc].x, S[cc].y);
                juce::Path clip; const auto ctr = (S[a] + S[b] + S[cc]) / 3.0f;
                auto grow = [&] (juce::Point<float> q) { const auto d = q - ctr; const float l = std::max (0.001f, d.getDistanceFromOrigin()); return q + d * (0.7f / l); };
                clip.addTriangle (grow (S[a]), grow (S[b]), grow (S[cc]));
                juce::Graphics::ScopedSaveState ss (g);
                g.reduceClipRegion (clip);
                g.drawImageTransformed (img, A);
            }
        }
}

void UnitView::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff17100c));
    if (! model.ok) { g.setColour (juce::Colours::white); g.drawText ("assets/ui missing", getLocalBounds(), juce::Justification::centred); return; }
    const float s = pxScale();
    g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
    g.drawImageTransformed (model.bg, juce::AffineTransform::scale (s));
    const auto clip = g.getClipBounds().toFloat();
    for (int i = 0; i < (int) model.ctls.size(); ++i)
    {
        const auto& c = model.ctls[(size_t) i];
        const auto area = c.boundsPx * s;
        if (! area.intersects (clip)) continue;
        switch (c.kind)
        {
            case Kind::Key:
            {
                const bool down = keysHeld.count (i) > 0 || (c.id.startsWith ("step") && st.heldStep == st.stepPage * 16 + c.id.getTrailingIntValue() - 1);
                const bool lit = keyLit (i);
                if (down) drawSprite (g, c, "down", 1.0f);          // a pressed key shows pressed (lit or not)
                else if (lit) drawSprite (g, c, "lit", 1.0f);
                break;
            }
            case Kind::Led:
            {
                const int k = c.id.getTrailingIntValue() - 1;
                const int stt = ledState (k);
                if (stt == 3) drawSprite (g, c, "red", 1.0f);
                else if (stt == 2) drawSprite (g, c, "green", 1.0f);
                else if (stt == 1) drawSprite (g, c, "green", 0.42f);
                break;
            }
            case Kind::Knob: drawPointer (g, c, knobNorm (i)); break;
            case Kind::Display:
                if (c.id == "display") drawWarped (g, displayImg, c);
                else { const int op = c.id.getLastCharacter() - 'a'; if (op >= 0 && op < 4) drawWarped (g, opImg[op], c); }
                break;
            default: break;
        }
    }
    // The value of the knob being turned, next to it.
    if (dragCtl >= 0)
    {
        const auto& c = model.ctls[(size_t) dragCtl];
        const auto txt = knobValueText (dragCtl);
        const auto p = c.base.map (0, c.size * 0.5f + 2.0f) * s;
        juce::Font f (juce::FontOptions (13.0f, juce::Font::bold));
        const float tw = juce::GlyphArrangement::getStringWidth (f, txt) + 16.0f;
        juce::Rectangle<float> r (p.x - tw / 2, p.y + 4.0f, tw, 22.0f);
        r = r.constrainedWithin (getLocalBounds().toFloat().reduced (4));
        g.setColour (juce::Colour (0xee101315)); g.fillRoundedRectangle (r, 5.0f);
        g.setColour (juce::Colour (0xffffc06a)); g.setFont (f); g.drawText (txt, r, juce::Justification::centred);
    }
}

// ───────────── screens ─────────────
static void drawAlgo (juce::Graphics& g, juce::Rectangle<float> r, int algo, juce::Colour ink, juce::Colour dim)
{
    const auto& A = algos()[(size_t) (algo & 7)];
    // Rows by depth: carriers at the bottom, their modulators above.
    int depth[4] {};
    for (int it = 0; it < 4; ++it)
        for (int a = 0; a < 4; ++a) for (int b = 0; b < 4; ++b) if (A.mod[b][a]) depth[b] = std::max (depth[b], depth[a] + 1);
    int maxD = 0; for (int d : depth) maxD = std::max (maxD, d);
    juce::Point<float> pos[4]; int perRow[5] {}, idxRow[4] {};
    for (int o = 0; o < 4; ++o) idxRow[o] = perRow[depth[o]]++;
    const float bw = std::min (r.getWidth() / 4.6f, 30.0f), bh = std::min (r.getHeight() / (float) (maxD + 1) * 0.62f, 24.0f);
    for (int o = 0; o < 4; ++o)
    {
        const float rowY = r.getBottom() - bh * 0.5f - (float) depth[o] * (r.getHeight() - bh) / (float) std::max (1, maxD);
        const float n = (float) perRow[depth[o]];
        pos[o] = { r.getX() + r.getWidth() * ((float) idxRow[o] + 0.5f) / n, maxD == 0 ? r.getCentreY() : rowY };
    }
    g.setColour (dim);
    for (int a = 0; a < 4; ++a) for (int b = 0; b < 4; ++b) if (A.mod[b][a]) g.drawLine (juce::Line<float> (pos[b], pos[a]), 2.0f);
    for (int o = 0; o < 4; ++o)
    {
        juce::Rectangle<float> box (pos[o].x - bw / 2, pos[o].y - bh / 2, bw, bh);
        if (A.car[o]) { g.setColour (ink); g.fillRoundedRectangle (box, 3.0f); g.setColour (kLcdBg); }
        else { g.setColour (kLcdBg); g.fillRoundedRectangle (box, 3.0f); g.setColour (ink); g.drawRoundedRectangle (box, 3.0f, 2.0f); }
        g.setFont (juce::FontOptions (bh * 0.7f, juce::Font::bold));
        g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + o)), box, juce::Justification::centred);
    }
}

bool UnitView::renderDisplay()
{
    const int tr = st.track, pat = currentPattern (proc);
    const auto info = proc.withProject ([&] (const Project& pr) { return std::make_tuple (pr.track[(size_t) tr].machine, (int) std::lround (pr.sounds[(size_t) tr][0][ALGO])); });
    const int machine = std::get<0> (info), algo = std::get<1> (info);
    static const char* mn[3] = { "TONE", "KIT", "MIDI" };
    // Texts first: the screen is drawn again only when something on it changed.
    juce::String l1 = "T" + juce::String (tr + 1) + " " + mn[juce::jlimit (0, 2, machine)] + "  PTN " + juce::String (pat + 1).paddedLeft ('0', 2)
                    + "  " + juce::String (st.stepPage * 16 + 1) + "-" + juce::String (st.stepPage * 16 + 16);
    if (st.gridRec) l1 << "  REC"; if (st.stepRec) l1 << "  STEP"; if (st.isFunc()) l1 << "  FUNC";
    juce::String l2, l3;
    if (juce::Time::getMillisecondCounter() < st.flashUntil) { l2 = st.flashName; l3 = st.flashValue; }
    else
    {
        const Page pg = st.heldStep >= 0 ? Page::Trig : st.page;
        auto fl = fieldsFor (proc, st, pg);
        if (! fl.empty())
        {
            const auto& fd = fl[(size_t) juce::jlimit (0, (int) fl.size() - 1, pg == Page::Trig ? st.trigField : st.field[(size_t) pg])];
            l2 = juce::String (pageName (pg)) + " > " + fd.name; l3 = fd.text();
        }
        if (pg == Page::Voice && st.field[(size_t) Page::Voice] == 0 && st.heldStep < 0) { l2 = "PRESET"; l3 = proc.currentPresetName.get(); }
    }
    const juce::String sig = l1 + "|" + l2 + "|" + l3 + "|" + juce::String (machine * 100 + algo * 10 + st.lane);
    if (sig == displaySig) return false;
    displaySig = sig;
    displayImg.clear (displayImg.getBounds());
    juce::Graphics g (displayImg);
    const auto R = displayImg.getBounds().toFloat();
    g.setColour (kLcdBg); g.fillRect (R);
    auto left = R.withWidth (R.getWidth() * 0.30f).reduced (10, 12);
    if (machine == kTone) drawAlgo (g, left, algo, kLcdInk, kLcdDim);
    else
    {
        g.setColour (kLcdInk); g.setFont (juce::FontOptions (40.0f, juce::Font::bold));
        g.drawText (machine == kKit ? "L" + juce::String (juce::jlimit (1, 8, st.lane)) : juce::String ("MIDI"), left, juce::Justification::centred);
    }
    auto right = R.withTrimmedLeft (R.getWidth() * 0.33f).reduced (6, 6);
    // Line 1: where the panel is.
    g.setColour (kLcdDim); g.setFont (juce::FontOptions (22.0f, juce::Font::bold));
    g.drawText (l1, right.removeFromTop (28), juce::Justification::centredLeft);
    // Lines 2-3: the last edit, or the page's current field.
    g.setColour (kLcdInk); g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText (l2.toUpperCase(), right.removeFromTop (30), juce::Justification::centredLeft, true);
    g.setColour (kLcdAmber); g.setFont (juce::FontOptions (34.0f, juce::Font::bold));
    g.drawFittedText (l3, right.toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    return true;
}

bool UnitView::renderOpScreen (int op)
{
    auto& img = opImg[op];
    const int tr = st.track, L = laneForSound();
    struct OpInfo { float ratio, fixed, level, a, d, s, r; bool car, on; int algo; };
    const auto oi = proc.withProject ([&] (const Project& pr) {
        const auto& s = pr.sounds[(size_t) tr][(size_t) L];
        const int algo = s.i (ALGO);
        return OpInfo { s[opParam (op, oRatio)], s[opParam (op, oFixed)], s[opParam (op, oLevel)], s[opParam (op, oAtt)], s[opParam (op, oDec)], s[opParam (op, oSus)], s[opParam (op, oRel)],
                        algos()[(size_t) (algo & 7)].car[op], true, algo };
    });
    const float lvl = op < 4 ? proc.value (params::tid (tr, op == 0 ? "opa" : op == 1 ? "opb" : op == 2 ? "opc" : "opd")) : oi.level;
    const bool focused = st.op == op && (st.page == Page::Ops);
    const juce::String sig = juce::String (oi.ratio) + "," + juce::String (oi.fixed) + "," + juce::String (oi.a) + "," + juce::String (oi.d) + "," + juce::String (oi.s) + ","
                           + juce::String (oi.r) + "," + juce::String (oi.car ? 1 : 0) + "," + juce::String (lvl, 3) + "," + juce::String (focused ? 1 : 0);
    if (sig == opSig[op]) return false;
    opSig[op] = sig;
    img.clear (img.getBounds());
    juce::Graphics g (img);
    const auto R = img.getBounds().toFloat();
    g.setColour (kLcdBg); g.fillRect (R);
    auto r = R.reduced (8, 6);
    // Left: name, role and ratio.
    auto left = r.removeFromLeft (150);
    g.setColour (focused ? kLcdAmber : kLcdInk); g.setFont (juce::FontOptions (40.0f, juce::Font::bold));
    g.drawText (juce::String::charToString ((juce::juce_wchar) ('A' + op)), left.removeFromLeft (34), juce::Justification::centred);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold)); g.setColour (oi.car ? kLcdAmber : kLcdDim);
    g.drawText (oi.car ? "CARRIER" : "MOD", left.removeFromTop (left.getHeight() / 2), juce::Justification::centredLeft);
    g.setColour (kLcdInk); g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText (oi.fixed >= 1.0f ? juce::String (juce::roundToInt (oi.fixed)) + " Hz" : "x" + juce::String (oi.ratio, oi.ratio < 10 ? 2 : 1), left, juce::Justification::centredLeft);
    // Right: level bar.
    auto bar = r.removeFromRight (16).reduced (2, 4);
    g.setColour (kLcdDim.withAlpha (0.5f)); g.drawRect (bar, 1.5f);
    g.setColour (kLcdAmber); g.fillRect (bar.withTrimmedTop (bar.getHeight() * (1.0f - juce::jlimit (0.0f, 1.0f, lvl))).reduced (2.5f));
    r.removeFromRight (10);
    // Middle: the envelope.
    const float tot = std::max (0.05f, oi.a + oi.d + 0.25f * (oi.a + oi.d + 0.2f) + oi.r);
    auto X = [&] (float t) { return r.getX() + r.getWidth() * std::sqrt (juce::jlimit (0.0f, 1.0f, t / tot)); };
    auto Y = [&] (float v) { return r.getBottom() - 4 - (r.getHeight() - 8) * v; };
    juce::Path env;
    const float tS = oi.a + oi.d, tR = tS + 0.25f * (oi.a + oi.d + 0.2f);
    env.startNewSubPath (X (0), Y (0));
    env.lineTo (X (oi.a), Y (1));
    for (int k = 1; k <= 12; ++k) { const float u = (float) k / 12.0f; env.lineTo (X (oi.a + u * oi.d), Y (oi.s + (1 - oi.s) * std::exp (-5.0f * u))); }
    env.lineTo (X (tR), Y (oi.s));
    for (int k = 1; k <= 12; ++k) { const float u = (float) k / 12.0f; env.lineTo (X (tR + u * oi.r), Y (oi.s * std::exp (-5.0f * u))); }
    juce::ignoreUnused (tS);
    g.setColour (kLcdDim.withAlpha (0.35f)); g.drawHorizontalLine ((int) Y (0), r.getX(), r.getRight());
    g.setColour (oi.car ? kLcdAmber : kLcdInk); g.strokePath (env, juce::PathStrokeType (2.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    return true;
}

// ───────────── timer ─────────────
void UnitView::timerCallback()
{
    ++blinkPhase;
    const float s = pxScale();
    // A STEP key held past half a second (grid record) becomes the held step.
    if (st.gridRec && pressedKey >= 0 && model.ctls[(size_t) pressedKey].id.startsWith ("step") && juce::Time::getMillisecondCounter() - keyDownAt > 450)
    {
        const int abs = st.stepPage * 16 + model.ctls[(size_t) pressedKey].id.getTrailingIntValue() - 1;
        if (st.heldStep != abs) { st.heldStep = abs; st.heldLocked = false; st.page = Page::Trig; flash ("STEP " + juce::String (abs + 1), "held: turn knobs to lock"); }
    }
    // Repaint only what changed: LEDs, lit keys, knob pointers, screens.
    for (int i = 0; i < (int) model.ctls.size(); ++i)
    {
        const auto& c = model.ctls[(size_t) i];
        bool changed = false;
        if (c.kind == Kind::Led) { const int k = c.id.getTrailingIntValue() - 1; const int v = ledState (k); if (k >= 0 && k < 16 && ledShown[(size_t) k] != v) { ledShown[(size_t) k] = v; changed = true; } }
        else if (c.kind == Kind::Key && i < 64) { const bool v = keyLit (i); if (keyLitShown[(size_t) i] != v) { keyLitShown[(size_t) i] = v; changed = true; } }
        else if (c.kind == Kind::Knob && i < 64 && (i == dragCtl || blinkPhase % 3 == 1)) { const float v = knobNorm (i); if (std::abs (knobShown[(size_t) i] - v) > 1e-4f) { knobShown[(size_t) i] = v; changed = true; } }
        if (changed) repaint ((c.boundsPx * s).getSmallestIntegerContainer().expanded (2));
    }
    if (blinkPhase % 3 == 0)
    {
        if (renderDisplay()) if (const auto* d = model.find ("display")) repaint ((d->boundsPx * s).getSmallestIntegerContainer().expanded (2));
        for (int o = 0; o < 4; ++o)
            if (renderOpScreen (o)) if (const auto* d = model.find (juce::String ("opdisp_") + (char) ('a' + o))) repaint ((d->boundsPx * s).getSmallestIntegerContainer().expanded (2));
    }
}

} // namespace dh::dv::ui
