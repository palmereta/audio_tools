#pragma once
// Parámetros que no son del panel (página SETTINGS): carácter, sincronía con el host, teclado táctil,
// MIDI y sistema. Una sola tabla para el plugin, la interfaz, los presets y at42render.

#include <JuceHeader.h>
#include "dsp/Engine42.h"

namespace dh {

struct ExtParam
{
    const char* id;
    const char* name;
    int steps;
    float lo, hi, def;
    const char* choices;
    bool global = false;
};

inline const std::vector<ExtParam>& extParams42()
{
    static const std::vector<ExtParam> p = {
        // Carácter
        { "ambient",     "Ambient Light",     0, 0, 1, 0.18f, nullptr },
        { "drift",       "Drone Drift",       0, 0, 1, 0.3f,  nullptr },
        { "mismatch",    "L/R Mismatch",      0, 0, 1, 0.5f,  nullptr },
        { "bus_drive",   "Mixer Drive",       0, 0, 1, 0.35f, nullptr },
        { "vintage",     "Effector Noise",    0, 0, 1, 0.0f,  nullptr },
        { "chromatic",   "Drone Tuning",      2, 0, 1, 0,     "Analog|Chromatic" },
        { "long_rel",    "Drone Release",     2, 0, 1, 0,     "8 s|30 s" },
        // Sincronía
        { "sync_lfoa",   "LFO A Sync",        2, 0, 1, 0,     "Free|Host" },
        { "lfoa_beats",  "LFO A Cycle",       6, 0, 5, 3,     "1/4|1/2|1 bar|2 bars|4 bars|8 bars" },
        { "sync_lfob",   "LFO B Sync",        2, 0, 1, 0,     "Free|Host" },
        { "lfob_beats",  "LFO B Cycle",       6, 0, 5, 2,     "1/16|1/8|1/4|1/2|1 bar|2 bars" },
        { "sync_pulser", "Pulser Sync",       2, 0, 1, 0,     "Free|Host" },
        { "pulser_div",  "Pulser Division",   6, 0, 5, 3,     "1/4|1/8|1/8 T|1/16|1/16 T|1/32" },
        { "kb_clock",    "Keys Clock",        2, 0, 1, 0,     "Internal|Host" },
        // Teclado táctil
        { "kb_mode",     "Keys Mode",         3, 0, 2, 0,     "Keyboard|Arpeggiator|Sequencer" },
        { "kb_scale",    "Keys Scale",        19, 0, 18, 0,   "Semitones|Ionian|Dorian|Phrygian|Lydian|Mixolydian|Aeolian|Locrian|Blues major|Blues minor|Pentatonic major|Pentatonic minor|Folk|Japanese|Gamelan|Gypsy|Arabian|Flamenco|Whole tone" },
        { "kb_root",     "Keys Root",         12, 0, 11, 0,   "C|C#|D|D#|E|F|F#|G|G#|A|A#|B" },
        { "kb_octave",   "Keys Octave",       7, 0, 6, 2,     "-2|-1|0|+1|+2|+3|+4" },
        { "kb_dir",      "Arp Direction",     5, 0, 4, 0,     "Forward|Backward|Ping-pong|Random|Order" },
        { "kb_hold",     "Arp Hold",          2, 0, 1, 0,     "Off|On" },
        { "kb_div",      "Keys Clock Div",    4, 0, 3, 2,     "1/4|1/8|1/16|1/32" },
        { "kb_bpm",      "Keys Tempo",        0, 10, 300, 120, nullptr },
        { "kb_porta",    "Portamento",        0, 0, 1, 0.0f,  nullptr },
        { "kb_legato",   "Legato",            2, 0, 1, 0,     "Off|On" },
        { "kb_vib",      "Vibrato Depth",     0, 0, 1, 0.0f,  nullptr },
        { "kb_vibspd",   "Vibrato Speed",     0, 0, 1, 0.4f,  nullptr },
        { "kb_press",    "Pressure Mode",     5, 0, 4, 0,     "Pressure|ASR|AD|Loop|Random" },
        { "kb_rise",     "Pressure Rise",     0, 0, 1, 0.3f,  nullptr },
        { "kb_fall",     "Pressure Fall",     0, 0, 1, 0.4f,  nullptr },
        { "kb_seqlen",   "Keys Seq Length",   15, 0, 14, 14,  "2|3|4|5|6|7|8|9|10|11|12|13|14|15|16" },
        // MIDI
        { "midi_drone",  "MIDI Drone Keys",   2, 0, 1, 1,     "Off|C1-F1", true },                              // globales: un preset no los cambia
        { "bend_range",  "Pitch Bend Range",  13, 0, 12, 2,   "0|1|2|3|4|5|6|7|8|9|10|11|12", true },
        // Sistema
        { "quality",     "Oversampling",      3, 0, 2, 1,     "1x|2x|4x", true },
        { "output",      "Output",            0, -24, 6, 0,   nullptr, true },
        { "panel",       "Panel Colour",      4, 0, 3, 0,     "Cream|Black|Maize|Blue", true },
    };
    return p;
}

inline const ExtParam* findExt42 (const juce::String& id)
{
    for (auto& e : extParams42()) if (id == e.id) return &e;
    return nullptr;
}

inline juce::StringArray extChoices42 (const ExtParam& e)
{
    return e.choices != nullptr ? juce::StringArray::fromTokens (e.choices, "|", "") : juce::StringArray();
}

inline juce::String formatExt42 (const ExtParam& e, float v)
{
    if (e.steps > 0) return extChoices42 (e)[juce::jlimit (0, e.steps - 1, (int) std::lround (v))];
    const juce::String id (e.id);
    if (id == "output") return (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB";
    if (id == "kb_bpm") return juce::String (v, 0) + " BPM";
    return juce::String (v * 10.0f, 1);
}

constexpr float kLfoABeats[6] = { 1, 2, 4, 8, 16, 32 };
constexpr float kLfoBBeats[6] = { 0.25f, 0.5f, 1, 2, 4, 8 };
constexpr float kPulserBeats[6] = { 1, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };
constexpr int kOversampling42[3] = { 1, 2, 4 };

template <typename Get>
inline void applyExt42 (Get&& get, Settings42& st, TouchKeys& k)
{
    st.ambient = get ("ambient"); st.drift = get ("drift"); st.mismatch = get ("mismatch");
    st.busDrive = get ("bus_drive"); st.vintageNoise = get ("vintage");
    st.chromatic = get ("chromatic") > 0.5f; st.longRelease = get ("long_rel") > 0.5f;
    st.syncLfoA = get ("sync_lfoa") > 0.5f; st.lfoABeats = kLfoABeats[juce::jlimit (0, 5, (int) std::lround (get ("lfoa_beats")))];
    st.syncLfoB = get ("sync_lfob") > 0.5f; st.lfoBBeats = kLfoBBeats[juce::jlimit (0, 5, (int) std::lround (get ("lfob_beats")))];
    st.syncPulser = get ("sync_pulser") > 0.5f; st.pulserBeats = kPulserBeats[juce::jlimit (0, 5, (int) std::lround (get ("pulser_div")))];
    st.kbHostClock = get ("kb_clock") > 0.5f;
    st.outputGain = juce::Decibels::decibelsToGain (get ("output"));
    k.mode = (int) std::lround (get ("kb_mode")); k.scale = (int) std::lround (get ("kb_scale"));
    k.root = (int) std::lround (get ("kb_root")); k.octave = (int) std::lround (get ("kb_octave"));
    k.dir = (int) std::lround (get ("kb_dir")); k.arpHold = get ("kb_hold") > 0.5f;
    k.clockDiv = (int) std::lround (get ("kb_div")); k.bpm = get ("kb_bpm");
    k.porta = get ("kb_porta"); k.legato = get ("kb_legato") > 0.5f;
    k.vibDepth = get ("kb_vib"); k.vibSpeed = get ("kb_vibspd");
    k.pressMode = (int) std::lround (get ("kb_press")); k.rise = get ("kb_rise"); k.fall = get ("kb_fall");
    k.seqLen = 2 + (int) std::lround (get ("kb_seqlen"));
}

} // namespace dh
