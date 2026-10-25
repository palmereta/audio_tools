#pragma once
// DeVicenti: every value the panel does not show on a knob, as "fields" (name, value text, normalised get/set,
// number of steps). One list per page serves both the small display (ALGORITHM picks the field with a step held,
// clicks on the screen step through the fields, DATA edits) and the back pages (all fields of a page at once).

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "../StepEdit.h"

namespace dh::dv::ui {

enum class Page : int { Voice, Ops, FilterAmp, Lfo, Mod, Trig, ChordScale, Mix, Fx, Midi, Song, Seq, Settings, kCount };
inline const char* pageName (Page p)
{
    static const char* n[] = { "VOICE", "OPS", "FILTER/AMP", "LFO", "MOD", "TRIG", "CHORD/SCALE", "MIX", "FX", "MIDI", "SONG", "SEQ", "SETTINGS" };
    return n[(int) p];
}
constexpr int kDisplayPages = 11;      // VOICE … SONG are reachable from FUNC + DATA; SEQ and SETTINGS only as back pages

// What the panel is pointing at: the current track (= the MIDI focus), the operator last touched, the KIT lane, the
// step held for locks and trig fields, the page and field on the display.
struct PanelState
{
    int track = 1, op = 0, lane = 1;
    bool func = false, funcLatched = false, gridRec = false, stepRec = false;
    int heldStep = -1;                  // absolute step index (0..63) or −1
    bool heldLocked = false;            // a lock was made while holding (the step is not toggled on release)
    int stepPage = 0;                   // which 16 steps the STEP keys show (0..3)
    Page page = Page::Voice;
    std::array<int, (size_t) Page::kCount> field {};
    int trigField = 0;
    stepedit::PageClip clip;
    juce::uint32 flashUntil = 0;        // the display shows the last edit until then
    juce::String flashName, flashValue;
    bool isFunc() const { return func || funcLatched; }
};

struct Field
{
    juce::String group, name;
    std::function<float()> get;                 // 0..1
    std::function<void (float)> set;
    std::function<juce::String()> text;
    int steps = 0;                              // > 1: stepped (choices)
    std::function<void (bool)> gesture;         // host parameters: begin / end of an edit
    bool enabled = true;
};
using FieldList = std::vector<Field>;

// Value text of a sound parameter (units, choice names).
juce::String formatSound (int param, float value);

// The fields of a page for the current panel state (the TRIG page needs a held step, or uses the first step on).
FieldList fieldsFor (DeVicentiProcessor& proc, PanelState& st, Page page);

// The current pattern (0-based) from the host parameter.
inline int currentPattern (const DeVicentiProcessor& p) { return juce::jlimit (0, kPatterns - 1, (int) std::lround (p.value ("pattern")) - 1); }

} // namespace dh::dv::ui
