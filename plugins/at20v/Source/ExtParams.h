#pragma once
// Parámetros que no son perillas del MS-20 (página EXTENSIONS): polifonía, sistema, vocoder,
// delay, reverb y MIDI. Una sola tabla para el plugin, la interfaz, los presets y at20vrender.

#include <JuceHeader.h>
#include "dsp/Engine.h"

namespace dh {

struct ExtParam
{
    const char* id;
    const char* name;
    int steps;                   // 0 = continuo; >0 = opciones (choices)
    float lo, hi, def;           // continuo: rango y valor por defecto; choice: def = índice
    const char* choices;         // opciones separadas por '|'
};

inline const std::vector<ExtParam>& extParams()
{
    static const std::vector<ExtParam> p = {
        // Polifonía
        { "voices",     "Voices",          6, 0, 5, 0,    "Mono|2|4|6|8|12" },
        { "unison",     "Unison",          4, 0, 3, 0,    "1|2|3|4" },
        { "detune",     "Unison Detune",   0, 0, 50, 12,  nullptr },
        { "spread",     "Stereo Spread",   0, 0, 1, 0.6f, nullptr },
        { "priority",   "Note Priority",   3, 0, 2, 1,    "Last|High|Low" },
        { "retrigger",  "Retrigger",       2, 0, 1, 0,    "Off|On" },
        // Sistema
        { "filter_rev", "Filter Revision", 2, 0, 1, 0,    "Rev 1 (Korg35)|Rev 2 (OTA)" },
        { "analog",     "Analog Drift",    0, 0, 1, 0.35f, nullptr },
        { "mg_sync",    "MG Tempo Sync",   2, 0, 1, 0,    "Free|Sync" },
        { "bend_range", "Bend Range",      13, 0, 12, 0,  "0|1|2|3|4|5|6|7|8|9|10|11|12" },
        { "quality",    "Oversampling",    2, 0, 1, 0,    "4x|8x" },
        { "midi_out",   "MIDI Out",        4, 0, 3, 1,    "Off|Thru|Patch|Thru + Patch" },
        { "output",     "Output",          0, -24, 6, 0,  nullptr },
        // Vocoder
        { "voc_on",     "Vocoder",         2, 0, 1, 0,    "Off|On" },
        { "voc_bands",  "Vocoder Bands",   4, 0, 3, 2,    "8|12|16|20" },
        { "voc_gain",   "Vocoder Input",   0, -12, 24, 6, nullptr },
        { "voc_shift",  "Formant Shift",   0, -1, 1, 0,   nullptr },
        { "voc_q",      "Vocoder Q",       0, 0, 1, 0.5f, nullptr },
        { "voc_attack", "Vocoder Attack",  0, 0, 1, 0.2f, nullptr },
        { "voc_release","Vocoder Release", 0, 0, 1, 0.3f, nullptr },
        { "voc_hiss",   "Vocoder Sibilance", 0, 0, 1, 0.4f, nullptr },
        { "voc_mix",    "Vocoder Mix",     0, 0, 1, 1.0f, nullptr },
        // Delay
        { "dly_on",     "Delay",           2, 0, 1, 0,    "Off|On" },
        { "dly_time",   "Delay Time",      12, 0, 11, 6,  "1/32|1/16 T|1/16|1/16 D|1/8 T|1/8|1/8 D|1/4 T|1/4|1/4 D|1/2|1 bar" },
        { "dly_fb",     "Delay Feedback",  0, 0, 0.95f, 0.35f, nullptr },
        { "dly_tone",   "Delay Tone",      0, 0, 1, 0.6f, nullptr },
        { "dly_mix",    "Delay Mix",       0, 0, 1, 0.25f, nullptr },
        // Reverb
        { "rev_on",     "Reverb",          2, 0, 1, 0,    "Off|On" },
        { "rev_size",   "Reverb Size",     0, 0, 1, 0.6f, nullptr },
        { "rev_damp",   "Reverb Damping",  0, 0, 1, 0.4f, nullptr },
        { "rev_mix",    "Reverb Mix",      0, 0, 1, 0.25f, nullptr },
    };
    return p;
}

inline const ExtParam* findExt (const juce::String& id)
{
    for (auto& e : extParams()) if (id == e.id) return &e;
    return nullptr;
}

inline juce::StringArray extChoices (const ExtParam& e)
{
    return e.choices != nullptr ? juce::StringArray::fromTokens (e.choices, "|", "") : juce::StringArray();
}

constexpr float kDelayBeats[12] = { 0.125f, 1.0f / 6.0f, 0.25f, 0.375f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 4.0f };
constexpr int kVoiceChoices[6] = { 1, 2, 4, 6, 8, 12 };
constexpr int kVocBands[4] = { 8, 12, 16, 20 };

inline juce::String formatExt (const ExtParam& e, float v)
{
    if (e.steps > 0) return extChoices (e)[juce::jlimit (0, e.steps - 1, (int) std::lround (v))];
    const juce::String id (e.id);
    if (id == "detune") return juce::String (v, 1) + " ct";
    if (id == "output" || id == "voc_gain") return (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB";
    if (id == "voc_shift") return (v >= 0 ? "+" : "") + juce::String (v * 12.0f, 1) + " st";
    if (id == "voc_attack") return juce::String (1.0f + 99.0f * v * v, 1) + " ms";
    if (id == "voc_release") return juce::String (10.0f + 990.0f * v * v, 0) + " ms";
    return juce::String (v * 10.0f, 1);
}

// Valores reales (los del host o los del preset) → ajustes del motor.
template <typename Get>
inline void applyExtParams (Get&& get, Settings& st, FxSettings& fx)
{
    st.voices = kVoiceChoices[juce::jlimit (0, 5, (int) std::lround (get ("voices")))];
    st.unison = 1 + juce::jlimit (0, 3, (int) std::lround (get ("unison")));
    st.detuneCents = get ("detune");
    st.spread = get ("spread");
    st.priority = juce::jlimit (0, 2, (int) std::lround (get ("priority")));
    st.retrigger = get ("retrigger") > 0.5f;
    st.filterRev = get ("filter_rev") > 0.5f ? 2 : 1;
    st.analog = get ("analog");
    st.mgSync = get ("mg_sync") > 0.5f;
    st.bendRange = get ("bend_range");
    st.outputGain = juce::Decibels::decibelsToGain (get ("output"));
    fx.vocOn = get ("voc_on") > 0.5f;
    fx.vocBands = kVocBands[juce::jlimit (0, 3, (int) std::lround (get ("voc_bands")))];
    fx.vocInGain = juce::Decibels::decibelsToGain (get ("voc_gain"));
    fx.vocShift = get ("voc_shift");
    fx.vocQ = get ("voc_q");
    const float a = get ("voc_attack"), r = get ("voc_release");
    fx.vocAttack = (1.0f + 99.0f * a * a) * 0.001f;
    fx.vocRelease = (10.0f + 990.0f * r * r) * 0.001f;
    fx.vocHiss = get ("voc_hiss");
    fx.vocMix = get ("voc_mix");
    fx.dlyOn = get ("dly_on") > 0.5f;
    fx.dlyBeats = kDelayBeats[juce::jlimit (0, 11, (int) std::lround (get ("dly_time")))];
    fx.dlyFeedback = get ("dly_fb");
    fx.dlyTone = get ("dly_tone");
    fx.dlyMix = get ("dly_mix");
    fx.revOn = get ("rev_on") > 0.5f;
    fx.revSize = get ("rev_size");
    fx.revDamp = get ("rev_damp");
    fx.revMix = get ("rev_mix");
}

} // namespace dh
