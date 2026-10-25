#include "Fields.h"

namespace dh::dv::ui {

using P = DeVicentiProcessor;

static juce::String pct (float u) { return juce::String (juce::roundToInt (u * 100.0f)) + " %"; }
static juce::String secs (float s) { return s < 1.0f ? juce::String (s * 1000.0f, s < 0.01f ? 1 : 0) + " ms" : juce::String (s, 2) + " s"; }
static juce::String hz (float f) { return f < 1000.0f ? juce::String (f, f < 100.0f ? 1 : 0) + " Hz" : juce::String (f / 1000.0f, 2) + " kHz"; }
static juce::String signedStr (float v, int dec, const char* unit) { return (v > 0 ? "+" : "") + juce::String (v, dec) + unit; }
static const char* noteName (int n) { static const char* nm[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return nm[((n % 12) + 12) % 12]; }
static juce::String noteStr (int n) { return juce::String (noteName (n)) + juce::String (n / 12 - 1); }

static juce::String destName (int d)
{
    if (d == mdNone) return "None";
    if (d == mdPitch) return "Pitch"; if (d == mdAmp) return "Amp"; if (d == mdIndex) return "FM index"; if (d == mdPanSpread) return "Pan spread";
    const int p = d - kDestParamBase;
    return p >= 0 && p < kNumSoundParams ? juce::String (paramInfo()[(size_t) p].name) : juce::String ("-");
}

juce::String formatSound (int param, float v)
{
    const auto& I = paramInfo()[(size_t) param];
    const int iv = (int) std::lround (v);
    switch (param)
    {
        case ALGO: return juce::String (iv + 1) + " " + algoName (iv);
        case FTYPE: return filterName (iv);
        case INS_TYPE: return insertName (iv);
        case VMODE: { static const char* n[3] = { "Poly", "Mono", "Legato" }; return n[juce::jlimit (0, 2, iv)]; }
        case NOISE_MODE: { static const char* n[3] = { "Low-pass", "Band-pass", "High-pass" }; return n[juce::jlimit (0, 2, iv)]; }
        case SUB_OCT: return iv ? "-2 oct" : "-1 oct";
        case SUB_SHAPE: return iv ? "Triangle" : "Sine";
        case STRUM_DIR: { static const char* n[3] = { "Up", "Down", "Alternate" }; return n[juce::jlimit (0, 2, iv)]; }
        case ARP_MODE: { static const char* n[6] = { "Off", "Up", "Down", "Up-down", "Random", "Order" }; return n[juce::jlimit (0, 5, iv)]; }
        case ARP_RATE: case LFO1_SYNC: case LFO2_SYNC: return divisionName (iv);
        case LFO1_SHAPE: case LFO2_SHAPE: return lfoShapeName (iv);
        case LFO1_MODE: case LFO2_MODE: return lfoModeName (iv);
        case LFO1_DEST: case LFO2_DEST: return destName (iv);
        case CHOKE: return iv == 0 ? juce::String ("None") : juce::String (iv);
        case LANE_NOTE: return noteStr (iv) + " (" + juce::String (iv) + ")";
        case KIT_PITCH: return noteStr (iv);
        case LEVEL: return signedStr (v, 1, " dB");
        case TUNE: return signedStr ((float) iv, 0, " st");
        case FINE: return signedStr (v, 0, " c");
        case PAN: return std::abs (v) < 0.01f ? juce::String ("C") : (v < 0 ? "L" : "R") + juce::String (juce::roundToInt (std::abs (v) * 100.0f));
        case PENV_DEPTH: return signedStr (v, 1, " st");
        case UNI_DETUNE: return juce::String (v, 1) + " c";
        case BEND: return juce::String (iv) + " st";
        case STRUM: return juce::String (v * 1000.0f, 0) + " ms";
        case CUTOFF: case NOISE_FREQ: case BASE: case WIDTH: return hz (v);
        default: break;
    }
    if (param >= A_RATIO && param <= D_TSCALE)
    {
        switch ((param - A_RATIO) % kOpParams)
        {
            case oRatio: return "x" + juce::String (v, v < 10.0f ? 3 : 2);
            case oDetune: return signedStr (v, 1, " c");
            case oHShape: return harmShapeName (iv);
            case oAtt: case oDec: case oRel: case oDelay: return secs (v);
            case oLoop: return iv ? "On" : "Off";
            case oFixed: return v < 1.0f ? juce::String ("Off") : hz (v);
            case oTScale: return "x" + juce::String (v, 2);
            case oCurve: case oKs: return signedStr (v, 2, "");
            default: return pct (v);
        }
    }
    if (param >= MOD1_SRC && param <= MOD8_AMT)
    {
        switch ((param - MOD1_SRC) % 3)
        {
            case 0: return modSourceName (iv);
            case 1: return destName (iv);
            default: return signedStr (v * 100.0f, 0, " %");
        }
    }
    const juce::String nm (I.name);
    if (nm.containsIgnoreCase ("attack") || nm.containsIgnoreCase ("decay") || nm.containsIgnoreCase ("release") || nm.containsIgnoreCase ("time") || param == GLIDE || param == NOISE_ATT)
        return secs (v);
    if (param == LFO1_RATE || param == LFO2_RATE) return hz (v);
    if (param == LFO1_FADE || param == LFO2_FADE) return signedStr (v, 1, " s");
    if (I.flags & pfInt) return (I.max - I.min <= 1.0f) ? juce::String (iv ? "On" : "Off") : juce::String (iv);
    if (I.flags & pfBip) return signedStr (v * (I.max <= 1.0f ? 100.0f : 1.0f), 0, I.max <= 1.0f ? " %" : "");
    if (I.max <= 1.0f) return pct (v);
    if (param == NOISE_Q) return juce::String (v, 2);
    return juce::String (v, 2);
}

// ── field builders ──
static int stepsOf (int param) { const auto& I = paramInfo()[(size_t) param]; return (I.flags & pfInt) ? (int) (I.max - I.min) + 1 : 0; }

static Field sound (P& p, int track, int lane, int param, const juce::String& group, juce::String name = {})
{
    Field f; f.group = group; f.name = name.isEmpty() ? juce::String (paramInfo()[(size_t) param].name) : name;
    f.get = [&p, track, lane, param] { return toNorm (param, p.soundValue (track, lane, param)); };
    f.set = [&p, track, lane, param] (float u) { p.setSound (track, lane, param, fromNorm (param, u)); };
    f.text = [&p, track, lane, param] { return formatSound (param, p.soundValue (track, lane, param)); };
    f.steps = stepsOf (param);
    return f;
}

static Field host (P& p, const juce::String& id, const juce::String& group, const juce::String& name)
{
    Field f; f.group = group; f.name = name;
    auto* prm = p.apvts.getParameter (id);
    f.get = [prm] { return prm ? prm->getValue() : 0.0f; };
    f.set = [prm] (float u) { if (prm) prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, u)); };
    f.text = [prm] { return prm ? prm->getCurrentValueAsText() : juce::String(); };
    f.gesture = [prm] (bool b) { if (prm) { if (b) prm->beginChangeGesture(); else prm->endChangeGesture(); } };
    if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (prm)) f.steps = c->choices.size();
    if (auto* i = dynamic_cast<juce::AudioParameterInt*> (prm)) f.steps = i->getRange().getLength() + 1;
    return f;
}

// An integer value of the project (choices or counts): getter and setter on the project, a name function.
static Field proj (P& p, const juce::String& group, const juce::String& name, int mn, int mx, uint32_t dirty,
                   std::function<int (const Project&)> get, std::function<void (Project&, int)> set,
                   std::function<juce::String (int)> txt = {}, uint32_t patMask = 0)
{
    Field f; f.group = group; f.name = name; f.steps = mx - mn + 1;
    f.get = [&p, get, mn, mx] { return mx > mn ? (float) (p.withProject (get) - mn) / (float) (mx - mn) : 0.0f; };
    f.set = [&p, set, mn, mx, dirty, patMask] (float u) { const int v = mn + juce::roundToInt (juce::jlimit (0.0f, 1.0f, u) * (float) (mx - mn)); p.editProject ([&] (Project& pr) { set (pr, v); }, dirty, patMask); };
    f.text = [&p, get, txt] { const int v = p.withProject (get); return txt ? txt (v) : juce::String (v); };
    return f;
}
// A continuous 0..1 (or min..max) float of the project.
static Field projF (P& p, const juce::String& group, const juce::String& name, float mn, float mx, uint32_t dirty,
                    std::function<float (const Project&)> get, std::function<void (Project&, float)> set,
                    std::function<juce::String (float)> txt = {})
{
    Field f; f.group = group; f.name = name;
    f.get = [&p, get, mn, mx] { return (p.withProject (get) - mn) / (mx - mn); };
    f.set = [&p, set, mn, mx, dirty] (float u) { const float v = mn + juce::jlimit (0.0f, 1.0f, u) * (mx - mn); p.editProject ([&] (Project& pr) { set (pr, v); }, dirty); };
    f.text = [&p, get, txt] { const float v = p.withProject (get); return txt ? txt (v) : pct (v); };
    return f;
}
static Field onOff (P& p, const juce::String& group, const juce::String& name, uint32_t dirty, std::function<bool (const Project&)> get, std::function<void (Project&, bool)> set)
{
    return proj (p, group, name, 0, 1, dirty, [get] (const Project& pr) { return get (pr) ? 1 : 0; }, [set] (Project& pr, int v) { set (pr, v != 0); }, [] (int v) { return juce::String (v ? "On" : "Off"); });
}

static const char* machineName (int m) { static const char* n[3] = { "TONE", "KIT", "MIDI" }; return n[juce::jlimit (0, 2, m)]; }

// The sound a page edits: the TONE voice, or on a KIT track the selected lane.
static int voiceLane (P& p, const PanelState& st) { return p.withProject ([&] (const Project& pr) { return pr.track[(size_t) st.track].machine == kKit; }) ? juce::jlimit (1, 8, st.lane) : 0; }

FieldList fieldsFor (P& p, PanelState& st, Page page)
{
    FieldList L;
    const int t = juce::jlimit (0, kTracks - 1, st.track);
    const int lane = voiceLane (p, st);
    const uint32_t dT = P::dTrack0 << t;
    const juce::String T = "T" + juce::String (t + 1);
    switch (page)
    {
        case Page::Voice:
        {
            const juce::String g1 = T + (lane > 0 ? " lane " + juce::String (lane) : juce::String()) + " voice";
            L.push_back (proj (p, g1, "Machine", 0, 2, P::dTracks, [t] (const Project& pr) { return pr.track[(size_t) t].machine; }, [t] (Project& pr, int v) { pr.track[(size_t) t].machine = v; }, [] (int v) { return juce::String (machineName (v)); }));
            if (lane > 0)
            {
                L.push_back (proj (p, g1, "KIT lane", 1, 8, 0, [&st] (const Project&) { return st.lane; }, [&st] (Project&, int v) { st.lane = v; }));
                for (int q : { KIT_PITCH, LANE_NOTE, CHOKE }) L.push_back (sound (p, t, lane, q, g1));
            }
            for (int q : { ALGO, VMODE, VOICES, UNISON, UNI_DETUNE, UNI_SPREAD, GLIDE, GLIDE_MODE, TUNE, FINE, BEND, LEVEL, PAN, LOWMONO })
                L.push_back (sound (p, t, lane, q, g1));
            const juce::String g2 = "Pitch, sub and noise";
            for (int q : { PENV_DEPTH, PENV_TIME, SUB_LEVEL, SUB_OCT, SUB_SHAPE, NOISE_LEVEL, NOISE_DECAY, NOISE_FREQ, NOISE_Q, NOISE_MODE, NOISE_ATT, NOISE_ENV })
                L.push_back (sound (p, t, lane, q, g2));
            const juce::String g3 = "AGE, strum, arp";
            for (int q : { AGE, VINTAGE, STRUM, STRUM_DIR, HUM_VEL, HUM_TIME, CHORD_HOLD, ARP_MODE, ARP_RATE, ARP_RANGE })
                L.push_back (sound (p, t, lane, q, g3));
            break;
        }
        case Page::Ops:
        {
            for (int o = 0; o < 4; ++o)
            {
                const juce::String g = "Operator " + juce::String::charToString ((juce::juce_wchar) ('A' + o));
                for (int off : { oRatio, oFixed, oDetune, oLevel, oFb, oHShape, oHAmt, oAtt, oDec, oSus, oRel, oCurve, oDelay, oLoop, oVel, oKs, oTScale })
                {
                    if (off == oLevel && lane == 0)
                    {
                        // The LEVEL knob is the track's host parameter (automatable), not the sound's value.
                        static const char* sfx[4] = { "opa", "opb", "opc", "opd" };
                        L.push_back (host (p, params::tid (t, sfx[o]), g, "Level"));
                        continue;
                    }
                    auto f = sound (p, t, lane, opParam (o, off), g);
                    f.name = f.name.substring (2);       // "A Ratio" → "Ratio"
                    f.enabled = true;
                    L.push_back (f);
                }
            }
            break;
        }
        case Page::FilterAmp:
        {
            const juce::String g1 = T + " filter";
            L.push_back (sound (p, t, lane, FTYPE, g1));
            L.push_back (host (p, params::tid (t, "cutoff"), g1, "Cutoff"));
            L.push_back (host (p, params::tid (t, "reso"), g1, "Resonance"));
            L.push_back (host (p, params::tid (t, "fenv"), g1, "Env amount"));
            for (int q : { F_ATT, F_DEC, F_SUS, F_REL, KTRK, BASE, WIDTH, BASSCOMP }) L.push_back (sound (p, t, lane, q, g1));
            const juce::String g2 = "Drive and amp";
            L.push_back (host (p, params::tid (t, "drive"), g2, "Drive"));
            for (int q : { FOLD, CRUSH, AMP_ATT, AMP_DEC, AMP_SUS, AMP_REL, VEL_AMP }) L.push_back (sound (p, t, lane, q, g2));
            break;
        }
        case Page::Lfo:
            for (int n = 0; n < 2; ++n)
            {
                const juce::String g = "LFO " + juce::String (n + 1);
                const int b = n == 0 ? LFO1_SHAPE : LFO2_SHAPE;
                for (int k = 0; k < 8; ++k) { auto f = sound (p, t, lane, b + k, g); f.name = f.name.fromFirstOccurrenceOf (" ", false, false).fromFirstOccurrenceOf (" ", false, false); L.push_back (f); }
            }
            break;
        case Page::Mod:
            for (int n = 0; n < 8; ++n)
            {
                const juce::String g = "Modulation " + juce::String (n + 1);
                const int b = MOD1_SRC + 3 * n;
                L.push_back (sound (p, t, lane, b, g, "Source")); L.push_back (sound (p, t, lane, b + 1, g, "Destination")); L.push_back (sound (p, t, lane, b + 2, g, "Amount"));
            }
            break;
        case Page::Trig:
        {
            const int pat = currentPattern (p);
            const int ln = lane;
            const int step = st.heldStep;
            const juce::String g = step >= 0 ? T + " step " + juce::String (step + 1) : juce::String ("Hold a STEP key");
            const uint32_t pm = 1u << pat;
            auto S = [pat, t, ln, step] (const Project& pr) -> const Step& { return pr.bank.p[(size_t) pat].t[(size_t) t].lane[(size_t) ln][(size_t) juce::jlimit (0, kMaxSteps - 1, step)]; };
            auto W = [pat, t, ln, step] (Project& pr) -> Step& { Step& s = pr.bank.p[(size_t) pat].t[(size_t) t].lane[(size_t) ln][(size_t) juce::jlimit (0, kMaxSteps - 1, step)]; if (! s.on()) { s = Step(); s.trig = 1; } return s; };
            L.push_back (proj (p, g, "Note", 0, 127, 0, [S] (const Project& pr) { const Step& s = S (pr); return s.nNotes > 0 ? (int) s.notes[0] : 60; }, [W] (Project& pr, int v) { Step& s = W (pr); s.nNotes = std::max<uint8_t> (1, s.nNotes); s.notes[0] = (int8_t) v; }, [] (int v) { return noteStr (v); }, pm));
            L.push_back (proj (p, g, "Velocity", 1, 127, 0, [S] (const Project& pr) { return (int) S (pr).vel; }, [W] (Project& pr, int v) { W (pr).vel = (uint8_t) v; }, {}, pm));
            L.push_back (proj (p, g, "Length", 1, 97, 0, [S] (const Project& pr) { const auto l = S (pr).len; return l == Step::kTie ? 97 : juce::jlimit (1, 96, (int) l); },
                               [W] (Project& pr, int v) { W (pr).len = v >= 97 ? Step::kTie : (uint16_t) v; },
                               [] (int v) { return v >= 97 ? juce::String ("Tie") : juce::String ((float) v / 12.0f, 2) + " st"; }, pm));
            L.push_back (proj (p, g, "Condition", 0, kNumConds - 1, 0, [S] (const Project& pr) { return (int) S (pr).cond; }, [W] (Project& pr, int v) { W (pr).cond = (uint8_t) v; }, [] (int v) { return juce::String (condName (v)); }, pm));
            L.push_back (proj (p, g, "Cond A", 1, 8, 0, [S] (const Project& pr) { return (int) S (pr).condA; }, [W] (Project& pr, int v) { W (pr).condA = (uint8_t) v; }, {}, pm));
            L.push_back (proj (p, g, "Cond B", 1, 8, 0, [S] (const Project& pr) { return (int) S (pr).condB; }, [W] (Project& pr, int v) { W (pr).condB = (uint8_t) v; }, {}, pm));
            L.push_back (proj (p, g, "Probability", 0, 100, 0, [S] (const Project& pr) { return (int) S (pr).prob; }, [W] (Project& pr, int v) { W (pr).prob = (uint8_t) v; }, [] (int v) { return juce::String (v) + " %"; }, pm));
            L.push_back (proj (p, g, "Micro timing", -24, 24, 0, [S] (const Project& pr) { return (int) S (pr).micro; }, [W] (Project& pr, int v) { W (pr).micro = (int8_t) v; }, [] (int v) { return (v > 0 ? "+" : "") + juce::String (v) + "/48"; }, pm));
            L.push_back (proj (p, g, "Retrig", 0, 5, 0, [S] (const Project& pr) { return (int) S (pr).retrig; }, [W] (Project& pr, int v) { W (pr).retrig = (uint8_t) v; },
                               [] (int v) { static const char* n[6] = { "Off", "1/2", "1/3", "1/4", "1/6", "1/8" }; return juce::String (n[juce::jlimit (0, 5, v)]); }, pm));
            L.push_back (proj (p, g, "Retrig count", 2, 16, 0, [S] (const Project& pr) { return (int) S (pr).retrigCount; }, [W] (Project& pr, int v) { W (pr).retrigCount = (uint8_t) v; }, {}, pm));
            L.push_back (proj (p, g, "Retrig ramp", -127, 127, 0, [S] (const Project& pr) { return (int) S (pr).retrigRamp; }, [W] (Project& pr, int v) { W (pr).retrigRamp = (int8_t) v; }, {}, pm));
            L.push_back (proj (p, g, "Chord", 0, 2, 0, [S] (const Project& pr) { return (int) S (pr).chord; }, [W] (Project& pr, int v) { W (pr).chord = (uint8_t) v; },
                               [] (int v) { static const char* n[3] = { "Off", "Degree", "Memory" }; return juce::String (n[juce::jlimit (0, 2, v)]); }, pm));
            L.push_back (proj (p, g, "Chord type", 0, kNumChordTypes - 1, 0, [S] (const Project& pr) { return (int) S (pr).chordType; }, [W] (Project& pr, int v) { W (pr).chordType = (uint8_t) v; }, [] (int v) { return juce::String (chordTypeName (v)); }, pm));
            L.push_back (proj (p, g, "Chord size", 2, 6, 0, [S] (const Project& pr) { return (int) S (pr).chordSize; }, [W] (Project& pr, int v) { W (pr).chordSize = (uint8_t) v; }, {}, pm));
            L.push_back (proj (p, g, "Voicing", 0, kNumVoicings - 1, 0, [S] (const Project& pr) { return (int) S (pr).voicing; }, [W] (Project& pr, int v) { W (pr).voicing = (uint8_t) v; }, [] (int v) { return juce::String (voicingName (v)); }, pm));
            L.push_back (proj (p, g, "Inversion", 0, 3, 0, [S] (const Project& pr) { return (int) S (pr).inversion; }, [W] (Project& pr, int v) { W (pr).inversion = (uint8_t) v; }, {}, pm));
            L.push_back (proj (p, g, "Slide", 0, 1, 0, [S] (const Project& pr) { return (S (pr).flags & sfSlide) ? 1 : 0; }, [W] (Project& pr, int v) { Step& s = W (pr); s.flags = (uint8_t) (v ? (s.flags | sfSlide) : (s.flags & ~sfSlide)); }, [] (int v) { return juce::String (v ? "On" : "Off"); }, pm));
            L.push_back (proj (p, g, "Patch", -1, kPatches - 1, 0, [S] (const Project& pr) { return (int) S (pr).patch; }, [W] (Project& pr, int v) { W (pr).patch = (int8_t) v; }, [] (int v) { return v < 0 ? juce::String ("-") : juce::String (v + 1); }, pm));
            if (step < 0) for (auto& f : L) f.enabled = false;
            break;
        }
        case Page::ChordScale:
        {
            const int pat = currentPattern (p);
            const uint32_t pm = 1u << pat;
            const juce::String g1 = "Pattern " + juce::String (pat + 1);
            L.push_back (proj (p, g1, "Root", 0, 11, 0, [pat] (const Project& pr) { return (int) pr.bank.p[(size_t) pat].root; }, [pat] (Project& pr, int v) { pr.bank.p[(size_t) pat].root = (uint8_t) v; }, [] (int v) { return juce::String (noteName (v)); }, pm));
            L.push_back (proj (p, g1, "Scale", 0, (int) scales().size() - 1, 0, [pat] (const Project& pr) { return (int) pr.bank.p[(size_t) pat].scale; }, [pat] (Project& pr, int v) { pr.bank.p[(size_t) pat].scale = (uint8_t) v; }, [] (int v) { return juce::String (scaleOf (v).name); }, pm));
            L.push_back (proj (p, g1, "Length", 1, 64, 0, [pat] (const Project& pr) { return (int) pr.bank.p[(size_t) pat].length; }, [pat] (Project& pr, int v) { pr.bank.p[(size_t) pat].length = (uint8_t) v; }, {}, pm));
            L.push_back (proj (p, g1, "Swing", 50, 80, 0, [pat] (const Project& pr) { return (int) pr.bank.p[(size_t) pat].swing; }, [pat] (Project& pr, int v) { pr.bank.p[(size_t) pat].swing = (uint8_t) v; }, [] (int v) { return juce::String (v) + " %"; }, pm));
            const juce::String g2 = T + " in pattern " + juce::String (pat + 1);
            L.push_back (proj (p, g2, "Track length", 1, 64, 0, [pat, t] (const Project& pr) { return (int) pr.bank.p[(size_t) pat].t[(size_t) t].length; }, [pat, t] (Project& pr, int v) { pr.bank.p[(size_t) pat].t[(size_t) t].length = (uint8_t) v; }, {}, pm));
            L.push_back (proj (p, g2, "Track speed", 0, 6, 0, [pat, t] (const Project& pr) { return (int) pr.bank.p[(size_t) pat].t[(size_t) t].speed; }, [pat, t] (Project& pr, int v) { pr.bank.p[(size_t) pat].t[(size_t) t].speed = (uint8_t) v; }, [] (int v) { return juce::String (speedName (v)); }, pm));
            L.push_back (proj (p, g2, "Track swing", 0, 100, 0, [pat, t] (const Project& pr) { return (int) pr.bank.p[(size_t) pat].t[(size_t) t].swing; }, [pat, t] (Project& pr, int v) { pr.bank.p[(size_t) pat].t[(size_t) t].swing = (uint8_t) v; }, [] (int v) { return juce::String (v) + " %"; }, pm));
            const juce::String g3 = T + " chords";
            L.push_back (onOff (p, g3, "Chord mode", P::dTracks, [t] (const Project& pr) { return pr.track[(size_t) t].chordPlay; }, [t] (Project& pr, bool v) { pr.track[(size_t) t].chordPlay = v; }));
            L.push_back (proj (p, g3, "Chord size", 2, 6, P::dTracks, [t] (const Project& pr) { return pr.track[(size_t) t].chordPlaySize; }, [t] (Project& pr, int v) { pr.track[(size_t) t].chordPlaySize = v; }));
            L.push_back (proj (p, g3, "Harmony channel", 0, 16, P::dMidi, [] (const Project& pr) { return pr.midi.harmonyChannel; }, [] (Project& pr, int v) { pr.midi.harmonyChannel = v; }, [] (int v) { return v == 0 ? juce::String ("Off") : juce::String (v); }));
            for (int m = 0; m < kChordMems; ++m)
            {
                Field f; f.group = "Chord memory"; f.name = "Slot " + juce::String (m + 1); f.steps = 1;
                f.get = [] { return 0.0f; }; f.set = [] (float) {};
                f.text = [&p, m] { return p.withProject ([m] (const Project& pr) {
                    const auto& c = pr.chordMem[(size_t) m]; if (c.n == 0) return juce::String ("empty");
                    juce::String s; for (int i = 0; i < c.n; ++i) s << (i ? " " : "") << (int) c.iv[i]; return s; }); };
                L.push_back (f);
            }
            break;
        }
        case Page::Mix:
            for (int k = 0; k < kTracks; ++k)
            {
                const juce::String g = "Track " + juce::String (k + 1);
                L.push_back (host (p, params::tid (k, "level"), g, "Level"));
                L.push_back (sound (p, k, 0, PAN, g));
                L.push_back (onOff (p, g, "Mute", P::dTracks, [k] (const Project& pr) { return pr.track[(size_t) k].mute; }, [k] (Project& pr, bool v) { pr.track[(size_t) k].mute = v; }));
                L.push_back (onOff (p, g, "Solo", P::dTracks, [k] (const Project& pr) { return pr.track[(size_t) k].solo; }, [k] (Project& pr, bool v) { pr.track[(size_t) k].solo = v; }));
                L.push_back (proj (p, g, "Output", 0, 1, P::dTracks, [k] (const Project& pr) { return pr.track[(size_t) k].outBus; }, [k] (Project& pr, int v) { pr.track[(size_t) k].outBus = v; }, [k] (int v) { return v ? "Track " + juce::String (k + 1) + " out" : juce::String ("Main"); }));
                L.push_back (proj (p, g, "Machine", 0, 2, P::dTracks, [k] (const Project& pr) { return pr.track[(size_t) k].machine; }, [k] (Project& pr, int v) { pr.track[(size_t) k].machine = v; }, [] (int v) { return juce::String (machineName (v)); }));
            }
            L.push_back (host (p, "master", "Master", "Master"));
            L.push_back (host (p, "tape", "Master", "Tape"));
            L.push_back (host (p, "glue", "Master", "Glue"));
            L.push_back (host (p, "limit", "Master", "Limit"));
            break;
        case Page::Fx:
        {
            const juce::String gd = "Delay", gr = "Reverb", gm = "Master bus", gi = T + " insert";
            L.push_back (proj (p, gd, "Mode", 0, 2, P::dFx, [] (const Project& pr) { return pr.fx.delayMode; }, [] (Project& pr, int v) { pr.fx.delayMode = v; }, [] (int v) { static const char* n[3] = { "Tape", "Dub", "Ping-pong" }; return juce::String (n[juce::jlimit (0, 2, v)]); }));
            L.push_back (host (p, "delay_time", gd, "Time"));
            L.push_back (host (p, "delay_fb", gd, "Feedback"));
            L.push_back (projF (p, gd, "Tone", 0, 1, P::dFx, [] (const Project& pr) { return pr.fx.delayTone; }, [] (Project& pr, float v) { pr.fx.delayTone = v; }));
            L.push_back (projF (p, gd, "Wow", 0, 1, P::dFx, [] (const Project& pr) { return pr.fx.delayWow; }, [] (Project& pr, float v) { pr.fx.delayWow = v; }));
            L.push_back (projF (p, gd, "Return", 0, 1, P::dFx, [] (const Project& pr) { return pr.fx.delayReturn; }, [] (Project& pr, float v) { pr.fx.delayReturn = v; }));
            L.push_back (projF (p, gd, "Into reverb", 0, 1, P::dFx, [] (const Project& pr) { return pr.fx.delayToReverb; }, [] (Project& pr, float v) { pr.fx.delayToReverb = v; }));
            L.push_back (onOff (p, gd, "Freeze", P::dFx, [] (const Project& pr) { return pr.fx.delayFreeze; }, [] (Project& pr, bool v) { pr.fx.delayFreeze = v; }));
            L.push_back (proj (p, gr, "Mode", 0, 3, P::dFx, [] (const Project& pr) { return pr.fx.reverbMode; }, [] (Project& pr, int v) { pr.fx.reverbMode = v; }, [] (int v) { static const char* n[4] = { "Spring", "Plate", "Hall", "Void" }; return juce::String (n[juce::jlimit (0, 3, v)]); }));
            L.push_back (host (p, "reverb_size", gr, "Size"));
            L.push_back (host (p, "reverb_decay", gr, "Decay"));
            L.push_back (projF (p, gr, "Damping", 0, 1, P::dFx, [] (const Project& pr) { return pr.fx.reverbDamp; }, [] (Project& pr, float v) { pr.fx.reverbDamp = v; }));
            L.push_back (projF (p, gr, "Pre-delay", 0, 0.25f, P::dFx, [] (const Project& pr) { return pr.fx.reverbPredelay; }, [] (Project& pr, float v) { pr.fx.reverbPredelay = v; }, [] (float v) { return secs (v); }));
            L.push_back (projF (p, gr, "Modulation", 0, 1, P::dFx, [] (const Project& pr) { return pr.fx.reverbMod; }, [] (Project& pr, float v) { pr.fx.reverbMod = v; }));
            L.push_back (projF (p, gr, "Shimmer", 0, 1, P::dFx, [] (const Project& pr) { return pr.fx.reverbShimmer; }, [] (Project& pr, float v) { pr.fx.reverbShimmer = v; }));
            L.push_back (projF (p, gr, "Return", 0, 1, P::dFx, [] (const Project& pr) { return pr.fx.reverbReturn; }, [] (Project& pr, float v) { pr.fx.reverbReturn = v; }));
            L.push_back (onOff (p, gr, "Freeze", P::dFx, [] (const Project& pr) { return pr.fx.reverbFreeze; }, [] (Project& pr, bool v) { pr.fx.reverbFreeze = v; }));
            L.push_back (host (p, "tape", gm, "Tape"));
            L.push_back (projF (p, gm, "Tape noise", 0, 1, P::dFx, [] (const Project& pr) { return pr.fx.tapeNoise; }, [] (Project& pr, float v) { pr.fx.tapeNoise = v; }));
            L.push_back (host (p, "glue", gm, "Glue"));
            L.push_back (host (p, "duck", gm, "Duck"));
            L.push_back (projF (p, gm, "Duck release", 0.02f, 1.5f, P::dFx, [] (const Project& pr) { return pr.fx.duckRelease; }, [] (Project& pr, float v) { pr.fx.duckRelease = v; }, [] (float v) { return secs (v); }));
            L.push_back (proj (p, gm, "Duck source", -1, 3, P::dFx, [] (const Project& pr) { return pr.fx.duckTrack; }, [] (Project& pr, int v) { pr.fx.duckTrack = v; }, [] (int v) { return v < 0 ? juce::String ("First KIT") : "Track " + juce::String (v + 1); }));
            L.push_back (host (p, "limit", gm, "Limit"));
            L.push_back (sound (p, t, 0, INS_TYPE, gi, "Type"));
            L.push_back (host (p, params::tid (t, "insert"), gi, "Amount"));
            L.push_back (sound (p, t, 0, INS_P1, gi, "Tone"));
            L.push_back (sound (p, t, 0, INS_P2, gi, "Character"));
            L.push_back (sound (p, t, 0, INS_MIX, gi, "Mix"));
            L.push_back (host (p, params::tid (t, "delay"), gi, "Delay send"));
            L.push_back (host (p, params::tid (t, "reverb"), gi, "Reverb send"));
            break;
        }
        case Page::Midi:
        {
            const juce::String g = "MIDI";
            L.push_back (proj (p, g, "Input", 0, 1, P::dMidi, [] (const Project& pr) { return pr.midi.inMode; }, [] (Project& pr, int v) { pr.midi.inMode = v; }, [] (int v) { return juce::String (v ? "Channel per track" : "OMNI to current track"); }));
            L.push_back (proj (p, g, "Current track", 0, 3, P::dMidi, [] (const Project& pr) { return pr.midi.focus; }, [&st] (Project& pr, int v) { pr.midi.focus = v; st.track = v; }, [] (int v) { return "Track " + juce::String (v + 1); }));
            L.push_back (proj (p, g, "CC mode", 0, 2, P::dMidi, [] (const Project& pr) { return pr.midi.ccMode; }, [] (Project& pr, int v) { pr.midi.ccMode = v; }, [] (int v) { static const char* n[3] = { "Absolute", "Relative (2's compl.)", "Relative (offset 64)" }; return juce::String (n[juce::jlimit (0, 2, v)]); }));
            L.push_back (onOff (p, g, "CC feedback", P::dMidi, [] (const Project& pr) { return pr.midi.ccFeedback; }, [] (Project& pr, bool v) { pr.midi.ccFeedback = v; }));
            L.push_back (proj (p, g, "Program change", 0, 1, P::dMidi, [] (const Project& pr) { return pr.midi.pcMode; }, [] (Project& pr, int v) { pr.midi.pcMode = v; }, [] (int v) { return juce::String (v ? "Preset" : "Pattern"); }));
            L.push_back (proj (p, g, "Harmony channel", 0, 16, P::dMidi, [] (const Project& pr) { return pr.midi.harmonyChannel; }, [] (Project& pr, int v) { pr.midi.harmonyChannel = v; }, [] (int v) { return v == 0 ? juce::String ("Off") : juce::String (v); }));
            L.push_back (onOff (p, g, "Clock in (standalone)", P::dMidi, [] (const Project& pr) { return pr.midi.clockIn; }, [] (Project& pr, bool v) { pr.midi.clockIn = v; }));
            L.push_back (onOff (p, g, "Clock out (standalone)", P::dMidi, [] (const Project& pr) { return pr.midi.clockOut; }, [] (Project& pr, bool v) { pr.midi.clockOut = v; }));
            L.push_back (onOff (p, g, "Thru", P::dMidi, [] (const Project& pr) { return pr.midi.thru; }, [] (Project& pr, bool v) { pr.midi.thru = v; }));
            L.push_back (proj (p, g, "Record quantise", 0, 1, P::dMidi, [] (const Project& pr) { return pr.midi.recQuantize; }, [] (Project& pr, int v) { pr.midi.recQuantize = v; }, [] (int v) { return juce::String (v ? "To the step" : "Keep timing"); }));
            for (int k = 0; k < kTracks; ++k)
            {
                const juce::String gt = "Track " + juce::String (k + 1);
                L.push_back (proj (p, gt, "In channel", 1, 16, P::dTracks, [k] (const Project& pr) { return pr.track[(size_t) k].inChannel; }, [k] (Project& pr, int v) { pr.track[(size_t) k].inChannel = v; }));
                L.push_back (proj (p, gt, "Out channel", 1, 16, P::dTracks, [k] (const Project& pr) { return pr.track[(size_t) k].outChannel; }, [k] (Project& pr, int v) { pr.track[(size_t) k].outChannel = v; }));
                L.push_back (onOff (p, gt, "Notes to MIDI out", P::dTracks, [k] (const Project& pr) { return pr.track[(size_t) k].doubleMidi; }, [k] (Project& pr, bool v) { pr.track[(size_t) k].doubleMidi = v; }));
            }
            break;
        }
        case Page::Song:
        {
            Field sm; sm.group = "Song"; sm.name = "Song mode"; sm.steps = 2;
            sm.get = [&p] { return p.songMode.load() ? 1.0f : 0.0f; }; sm.set = [&p] (float u) { p.songMode.store (u > 0.5f); };
            sm.text = [&p] { return juce::String (p.songMode.load() ? "On (plays the chain)" : "Off (plays the pattern)"); };
            L.push_back (sm);
            L.push_back (proj (p, "Song", "Chain length", 0, kMaxChain, P::dChain, [] (const Project& pr) { return pr.bank.chainLen; }, [] (Project& pr, int v) { pr.bank.chainLen = v; }));
            for (int e = 0; e < 16; ++e)
            {
                const juce::String g = "Chain " + juce::String (e + 1);
                L.push_back (proj (p, g, "Pattern", 1, kPatterns, P::dChain, [e] (const Project& pr) { return (int) pr.bank.chain[(size_t) e].pattern + 1; }, [e] (Project& pr, int v) { pr.bank.chain[(size_t) e].pattern = (uint8_t) (v - 1); pr.bank.chainLen = std::max (pr.bank.chainLen, e + 1); }));
                L.push_back (proj (p, g, "Repeats", 1, 16, P::dChain, [e] (const Project& pr) { return (int) pr.bank.chain[(size_t) e].repeats; }, [e] (Project& pr, int v) { pr.bank.chain[(size_t) e].repeats = (uint8_t) v; }));
            }
            break;
        }
        default: break;
    }
    return L;
}

} // namespace dh::dv::ui
