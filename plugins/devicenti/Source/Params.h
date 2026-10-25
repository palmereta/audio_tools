#pragma once
// DeVicenti: the 64 host parameters (plan §5.7; Live shows at most 64). Per track (×4): cutoff, resonance, filter
// envelope, drive, AGE, delay send, reverb send, level, the four operator levels and the insert amount (13 × 4 = 52);
// global: swing, master, delay time, delay feedback, reverb size, reverb decay, TAPE, GLUE, DUCK, LIMIT, tempo and
// pattern (12). Everything else (the rest of the sounds, the patterns, the effect modes, MIDI settings) is state:
// lockable from the sequencer, editable on the display pages, not host-automatable. The ids are stable.
//
// On a KIT track the per-track values act on the kit bus: cutoff and resonance are a bus low-pass (open at 20 kHz),
// ENV scales every lane's decay, drive is a bus drive, the four operator levels are the lane-group levels
// (A kick + low tom, B snare + clap, C hats, D high tom + perc; 0.75 = unity).

#include <JuceHeader.h>
#include "dsp/Engine.h"

namespace dh::dv::params {

inline const char* trackSuffix (int k)
{
    static const char* s[13] = { "cutoff", "reso", "fenv", "drive", "age", "delay", "reverb", "level", "opa", "opb", "opc", "opd", "insert" };
    return s[std::clamp (k, 0, 12)];
}
constexpr int kTrackParams = 13;
inline juce::String tid (int t, const char* suffix) { return "t" + juce::String (t + 1) + "_" + suffix; }

inline juce::StringArray delayDivNames() { juce::StringArray a; for (int i = 0; i < 11; ++i) a.add (delayDivName (i)); return a; }

inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    // Value text with units (the host's parameter list, the panel and the back pages show it). The unit is part
    // of the text, so the label stays empty.
    auto text = [] (const String& id, const String& label) -> std::function<String (float, int)> {
        if (label == "Hz") return [] (float v, int) { return v < 1000.0f ? String (v, v < 100.0f ? 1 : 0) + " Hz" : String (v / 1000.0f, v < 10000.0f ? 2 : 1) + " kHz"; };
        if (label == "dB") return [] (float v, int) { return v <= -59.95f ? String ("-inf dB") : (v > 0.05f ? "+" : "") + String (v, 1) + " dB"; };
        if (label == "%") return [] (float v, int) { return String (v, 1) + " %"; };
        if (label == "s") return [] (float v, int) { return v < 1.0f ? String (juce::roundToInt (v * 1000.0f)) + " ms" : String (v, v < 10.0f ? 2 : 1) + " s"; };
        if (label == "BPM") return [] (float v, int) { return String (v, 1) + " BPM"; };
        if (id.endsWith ("fenv")) return [] (float v, int) { return (v > 0.005f ? "+" : "") + String (juce::roundToInt (v * 100.0f)) + " %"; };
        if (id == "delay_fb") return [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %" + (v > 1.0f ? " (runaway)" : ""); };
        return [] (float v, int) { return String (juce::roundToInt (v * 100.0f)) + " %"; };      // 0..1 amounts
    };
    auto f = [&] (const String& id, const String& name, float mn, float mx, float def, float skewCentre, const String& label) {
        NormalisableRange<float> r (mn, mx);
        if (skewCentre > 0.0f) r.setSkewForCentre (skewCentre);
        // Typed values back (the host's text entry): "2.4 kHz", "350 ms", "45 %", "-6 dB".
        auto parse = [label] (const String& t) {
            const float x = t.retainCharacters ("0123456789.-+").getFloatValue();
            if (label == "Hz") return t.containsIgnoreCase ("k") ? x * 1000.0f : x;
            if (label == "s") return t.containsIgnoreCase ("ms") ? x / 1000.0f : x;
            if (label.isEmpty()) return t.contains ("%") || std::abs (x) > 1.5f ? x / 100.0f : x;
            return x;
        };
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, r, def,
                   AudioParameterFloatAttributes().withStringFromValueFunction (text (id, label)).withValueFromStringFunction (parse)));
    };
    for (int t = 0; t < kTracks; ++t)
    {
        const String T = "T" + String (t + 1) + " ";
        f (tid (t, "cutoff"), T + "Cutoff", 20.0f, 20000.0f, 8000.0f, 1000.0f, "Hz");
        f (tid (t, "reso"), T + "Resonance", 0.0f, 1.0f, 0.1f, 0.0f, "");
        f (tid (t, "fenv"), T + "Filter env", -1.0f, 1.0f, 0.0f, 0.0f, "");
        f (tid (t, "drive"), T + "Drive", 0.0f, 1.0f, 0.0f, 0.0f, "");
        f (tid (t, "age"), T + "AGE", 0.0f, 1.0f, 0.0f, 0.0f, "");
        f (tid (t, "delay"), T + "Delay send", 0.0f, 1.0f, 0.0f, 0.0f, "");
        f (tid (t, "reverb"), T + "Reverb send", 0.0f, 1.0f, 0.0f, 0.0f, "");
        f (tid (t, "level"), T + "Level", -60.0f, 6.0f, 0.0f, -12.0f, "dB");
        f (tid (t, "opa"), T + "Op A level", 0.0f, 1.0f, 1.0f, 0.0f, "");
        f (tid (t, "opb"), T + "Op B level", 0.0f, 1.0f, 0.0f, 0.0f, "");
        f (tid (t, "opc"), T + "Op C level", 0.0f, 1.0f, 0.0f, 0.0f, "");
        f (tid (t, "opd"), T + "Op D level", 0.0f, 1.0f, 0.0f, 0.0f, "");
        f (tid (t, "insert"), T + "Insert amount", 0.0f, 1.0f, 0.5f, 0.0f, "");
    }
    f ("swing", "Swing", 50.0f, 80.0f, 56.0f, 0.0f, "%");
    f ("master", "Master", -60.0f, 6.0f, 0.0f, -12.0f, "dB");
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "delay_time", 1 }, "Delay time", delayDivNames(), 7));
    f ("delay_fb", "Delay feedback", 0.0f, 1.1f, 0.4f, 0.0f, "");
    f ("reverb_size", "Reverb size", 0.0f, 1.0f, 0.5f, 0.0f, "");
    f ("reverb_decay", "Reverb decay", 0.3f, 30.0f, 3.0f, 4.0f, "s");
    f ("tape", "Tape", 0.0f, 1.0f, 0.0f, 0.0f, "");
    f ("glue", "Glue", 0.0f, 1.0f, 0.0f, 0.0f, "");
    f ("duck", "Duck", 0.0f, 1.0f, 0.0f, 0.0f, "");
    f ("limit", "Limit", 0.0f, 1.0f, 0.0f, 0.0f, "");
    f ("tempo", "Tempo", 40.0f, 240.0f, 88.0f, 0.0f, "BPM");
    l.add (std::make_unique<AudioParameterInt> (ParameterID { "pattern", 1 }, "Pattern", 1, kPatterns, 1));
    return l;
}

} // namespace dh::dv::params
