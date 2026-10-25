#pragma once
// DeviceNTI V: the front panel as data (plan §8). One table lists every control of the single unit: its id, the
// silkscreen label and the small FUNC label, what it does turned and with FUNC held, its fixed MIDI CC (main layer
// and FUNC layer), and where it sits on the panel: millimetres from the top-left corner of the case (300 × 426 mm,
// the proportions of art/single-front.png). `devicentirender --panel-json` exports this table; the Blender script
// (tools/blender/devicenti.py) builds the unit from it and writes assets/ui/ui.json with every control projected
// through the camera, which is what the editor draws and hit-tests.
//
// The MIDI CC map is fixed and the same on every track channel (channels 1-4 in per-channel mode; OMNI sends them
// to the focused track). It is documented in the manual. Learned CCs (right-click a control) are kept in the state.

#include "dsp/SoundParams.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <string>
#include <vector>

namespace dh::dv::panel {

// Stepped ratio table of the RATIO knob (FUNC + RATIO edits the continuous value).
inline const std::array<float, 24>& ratioTable()
{
    static const std::array<float, 24> r { 0.25f, 0.5f, 0.75f, 1.0f, 1.41f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f, 7.0f, 7.07f, 8.0f, 9.0f, 10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f };
    return r;
}
inline float ratioFromKnob (float u) { const auto& r = ratioTable(); const int i = std::clamp ((int) std::lround (u * (float) (r.size() - 1)), 0, (int) r.size() - 1); return r[(size_t) i]; }
inline float knobFromRatio (float ratio)
{
    const auto& r = ratioTable(); int best = 0; float bd = 1e9f;
    for (int i = 0; i < (int) r.size(); ++i) { const float d = std::fabs (std::log (r[(size_t) i] / std::max (ratio, 0.01f))); if (d < bd) { bd = d; best = i; } }
    return (float) best / (float) (r.size() - 1);
}

// The one-knob ENV macro: a curated path click → pluck → piano → organ → swell → pad (A, D, S, R in seconds).
struct Adsr { float a, d, s, r; };
inline Adsr envMacro (float u)
{
    static const Adsr pts[6] = {
        { 0.0005f, 0.012f, 0.0f, 0.010f },    // click
        { 0.001f,  0.25f,  0.0f, 0.12f },     // pluck
        { 0.002f,  1.6f,   0.0f, 0.45f },     // piano
        { 0.004f,  0.3f,   1.0f, 0.06f },     // organ (gate)
        { 0.6f,    1.5f,   0.85f, 1.2f },     // swell
        { 2.0f,    3.0f,   0.8f, 4.0f } };    // pad
    u = std::clamp (u, 0.0f, 1.0f) * 5.0f;
    const int i = std::min (4, (int) u); const float t = u - (float) i;
    auto lg = [] (float a, float b, float t) { return std::exp (std::log (a) + (std::log (b) - std::log (a)) * t); };
    return { lg (pts[i].a, pts[i + 1].a, t), lg (pts[i].d, pts[i + 1].d, t), pts[i].s + (pts[i + 1].s - pts[i].s) * t, lg (pts[i].r, pts[i + 1].r, t) };
}

// What a control (or its FUNC layer) drives.
enum class TK : uint8_t { None, Auto, AutoGlobal, Sound, OpRatioStep, OpEnvMacro, Action, Display };
struct Target
{
    TK kind = TK::None;
    const char* id = "";         // Auto: the per-track suffix ("cutoff" → "t1_cutoff"); AutoGlobal: the parameter id; Action: the action name
    int sound = -1;              // Sound: the sound parameter
    int op = -1;                 // operator controls (0..3)
};
enum class Kind : uint8_t { Knob, Key, Display, Led };
// Case and plate (mm): the cream plate is inset; the orange rim below it carries the wordmark.
constexpr float kCaseW = 300.0f, kCaseH = 426.0f, kCaseZ = 50.0f;
struct Control
{
    const char* id; const char* label; const char* funcLabel; Kind kind;
    Target main, func;
    int cc = -1, ccFunc = -1;
    float x = 0, y = 0, size = 0;    // centre and diameter / width (mm); displays: width (the height is fixed per display)
};

inline Target autoT (const char* suffix) { Target t; t.kind = TK::Auto; t.id = suffix; return t; }
inline Target globT (const char* id) { Target t; t.kind = TK::AutoGlobal; t.id = id; return t; }
inline Target sndT (int p, int op = -1) { Target t; t.kind = TK::Sound; t.sound = p; t.op = op; return t; }
inline Target actT (const char* a) { Target t; t.kind = TK::Action; t.id = a; return t; }

inline const std::vector<Control>& controls()
{
    static const std::vector<Control> c = [] {
        std::vector<Control> v;
        // 1. Top row.
        v.push_back ({ "tempo", "TEMPO", "SPEED", Kind::Knob, globT ("tempo"), actT ("track_speed"), -1, -1, 30.7f, 45.6f, 24 });
        v.push_back ({ "swing", "SWING", "TRK SWING", Kind::Knob, globT ("swing"), actT ("track_swing"), 9, -1, 69.3f, 45.6f, 24 });
        v.push_back ({ "algorithm", "ALGORITHM", "MACHINE", Kind::Knob, sndT (ALGO), actT ("machine"), 15, -1, 125.0f, 33.0f, 28 });
        v.push_back ({ "display", "", "", Kind::Display, {}, {}, -1, -1, 156.5f, 68.5f, 56 });
        v.push_back ({ "data", "DATA", "PAGE", Kind::Knob, actT ("data"), actT ("page"), -1, -1, 188.0f, 33.0f, 28 });
        v.push_back ({ "lfo", "LFO", "LFO 2", Kind::Knob, sndT (LFO1_DEPTH), sndT (LFO2_DEPTH), 12, 13, 238.6f, 45.6f, 24 });
        v.push_back ({ "master", "MASTER", "TRK LEVEL", Kind::Knob, globT ("master"), autoT ("level"), 16, 7, 272.8f, 45.6f, 24 });
        // 2. Key row.
        const char* trk[4] = { "track1", "track2", "track3", "track4" };
        const char* trkL[4] = { "TRACK 1", "TRACK 2", "TRACK 3", "TRACK 4" };
        const float trkX[4] = { 35.1f, 66.8f, 97.8f, 128.8f };
        for (int i = 0; i < 4; ++i) v.push_back ({ trk[i], trkL[i], "MUTE", Kind::Key, actT ("track"), actT ("mute"), -1, -1, trkX[i], 107.3f, 26 });
        v.push_back ({ "play", "PLAY", "RESTART", Kind::Key, actT ("play"), actT ("restart"), -1, -1, 156.8f, 107.3f, 23 });
        v.push_back ({ "stop", "STOP", "PANIC", Kind::Key, actT ("stop"), actT ("panic"), -1, -1, 185.2f, 107.3f, 23 });
        v.push_back ({ "rec", "REC", "STEP REC", Kind::Key, actT ("rec"), actT ("step_rec"), -1, -1, 213.8f, 107.3f, 23 });
        v.push_back ({ "chord", "CHORD", "SCALE", Kind::Key, actT ("chord"), actT ("scale"), -1, -1, 242.3f, 107.3f, 23 });
        v.push_back ({ "func", "FUNC", "", Kind::Key, actT ("func"), {}, -1, -1, 270.8f, 107.3f, 23 });
        // 3. Operator grid: A B on the upper row, C D on the lower row; same five knobs each, a small screen below.
        const char* opn[4] = { "a", "b", "c", "d" };
        const char* opsfx[4] = { "opa", "opb", "opc", "opd" };
        static const char* opDisp[4] = { "opdisp_a", "opdisp_b", "opdisp_c", "opdisp_d" };
        for (int o = 0; o < 4; ++o)
        {
            const float ox = o % 2 == 0 ? 0.0f : 136.1f, oy = o < 2 ? 161.3f : 249.0f;
            const int ccBase = 40 + 5 * o, fBase = 102 + 5 * o;
            auto id = [&] (const char* what) { static std::deque<std::string> keep; keep.push_back (std::string ("op_") + opn[o] + "_" + what); return keep.back().c_str(); };
            Target ratio; ratio.kind = TK::OpRatioStep; ratio.op = o;
            Target env; env.kind = TK::OpEnvMacro; env.op = o;
            v.push_back ({ id ("ratio"), "RATIO", "FINE", Kind::Knob, ratio, sndT (opParam (o, oRatio), o), ccBase + 0, fBase + 0, ox + 30.7f, oy, 19 });
            v.push_back ({ id ("level"), "LEVEL", "VELOCITY", Kind::Knob, autoT (opsfx[o]), sndT (opParam (o, oVel), o), ccBase + 1, fBase + 1, ox + 57.6f, oy, 19 });
            v.push_back ({ id ("feedback"), "FEEDBACK", "HARMONICS", Kind::Knob, sndT (opParam (o, oFb), o), sndT (opParam (o, oHAmt), o), ccBase + 2, fBase + 2, ox + 84.5f, oy, 19 });
            v.push_back ({ id ("detune"), "DETUNE", "KEY SCALE", Kind::Knob, sndT (opParam (o, oDetune), o), sndT (opParam (o, oKs), o), ccBase + 3, fBase + 3, ox + 111.1f, oy, 19 });
            v.push_back ({ id ("env"), "ENV", "TIME", Kind::Knob, env, sndT (opParam (o, oTScale), o), ccBase + 4, fBase + 4, ox + 136.4f, oy, 19 });
            v.push_back ({ opDisp[o], "", "", Kind::Display, {}, {}, -1, -1, ox + 83.5f, oy + 33.5f, 74 });
        }
        // 4. Bottom row: FILTER, then DRIVE / AGE (olive), then the sends.
        v.push_back ({ "cutoff", "CUTOFF", "KEYTRACK", Kind::Knob, autoT ("cutoff"), sndT (KTRK), 74, 80, 30.5f, 326.0f, 20 });
        v.push_back ({ "reso", "RESO", "BASE", Kind::Knob, autoT ("reso"), sndT (BASE), 71, 81, 58.5f, 326.0f, 20 });
        v.push_back ({ "fenv", "ENV", "DECAY", Kind::Knob, autoT ("fenv"), sndT (F_DEC), 75, 82, 86.5f, 326.0f, 20 });
        v.push_back ({ "type", "TYPE", "WIDTH", Kind::Knob, sndT (FTYPE), sndT (WIDTH), 76, 83, 114.5f, 326.0f, 20 });
        v.push_back ({ "drive", "DRIVE", "FOLD", Kind::Knob, autoT ("drive"), sndT (FOLD), 77, 84, 169.3f, 326.0f, 20 });
        v.push_back ({ "age", "AGE", "VINTAGE", Kind::Knob, autoT ("age"), sndT (VINTAGE), 78, 85, 200.3f, 326.0f, 20 });
        v.push_back ({ "delay", "DELAY", "TIME/FDBK", Kind::Knob, autoT ("delay"), globT ("delay_fb"), 92, 86, 235.8f, 326.0f, 20 });
        v.push_back ({ "reverb", "REVERB", "SIZE/DECAY", Kind::Knob, autoT ("reverb"), globT ("reverb_decay"), 91, 87, 266.8f, 326.0f, 20 });
        // 5. STEP keys along the front edge, an LED above each.
        static const char* stepIds[16] = { "step1", "step2", "step3", "step4", "step5", "step6", "step7", "step8", "step9", "step10", "step11", "step12", "step13", "step14", "step15", "step16" };
        static const char* ledIds[16] = { "led1", "led2", "led3", "led4", "led5", "led6", "led7", "led8", "led9", "led10", "led11", "led12", "led13", "led14", "led15", "led16" };
        static const char* stepL[16] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12", "13", "14", "15", "16" };
        static const char* stepF[16] = { "PAGE 1", "PAGE 2", "PAGE 3", "PAGE 4", "COPY", "PASTE", "CLEAR", "FILL", "PTN 1", "PTN 2", "PTN 3", "PTN 4", "PTN 5", "PTN 6", "PTN 7", "PTN 8" };
        for (int i = 0; i < 16; ++i)
        {
            const float x = 22.8f + 16.94f * (float) i;
            v.push_back ({ stepIds[i], stepL[i], stepF[i], Kind::Key, actT ("step"), actT ("step_func"), -1, -1, x, 384.0f, 14.2f });
            v.push_back ({ ledIds[i], "", "", Kind::Led, {}, {}, -1, -1, x, 370.0f, 2.6f });
        }
        return v;
    }();
    return c;
}

// Extra CCs that are not knobs of the panel (all fixed): master effects and the insert amount.
struct ExtraCc { int cc; Target t; };
inline const std::vector<ExtraCc>& extraCcs()
{
    static const std::vector<ExtraCc> e { { 93, autoT ("insert") }, { 10, sndT (PAN) }, { 88, globT ("tape") }, { 89, globT ("glue") }, { 90, globT ("duck") }, { 3, globT ("limit") },
                                          { 94, globT ("delay_fb") }, { 95, globT ("reverb_size") } };
    return e;
}

// The table as JSON (for the Blender script): id, label, FUNC label, kind and position.
inline std::string controlsJson()
{
    static const char* kinds[4] = { "knob", "key", "display", "led" };
    std::string o = "{\n \"case\": [" + std::to_string (kCaseW) + ", " + std::to_string (kCaseH) + ", " + std::to_string (kCaseZ) + "],\n \"controls\": [\n";
    const auto& c = controls();
    for (size_t i = 0; i < c.size(); ++i)
    {
        const auto& k = c[i];
        o += "  {\"id\": \"" + std::string (k.id) + "\", \"label\": \"" + k.label + "\", \"func\": \"" + k.funcLabel + "\", \"kind\": \"" + kinds[(int) k.kind]
           + "\", \"x\": " + std::to_string (k.x) + ", \"y\": " + std::to_string (k.y) + ", \"size\": " + std::to_string (k.size) + "}" + (i + 1 < c.size() ? ",\n" : "\n");
    }
    return o + " ]\n}\n";
}

} // namespace dh::dv::panel
