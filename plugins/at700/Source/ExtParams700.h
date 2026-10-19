#pragma once
// Parámetros que no son del panel (página SETTINGS): voces, variante del VCF-1, carácter, reloj del
// secuenciador, MIDI y sistema. Una sola tabla para el plugin, la interfaz, los presets y at700render.

#include <JuceHeader.h>
#include "dsp/Engine700.h"

namespace dh {

struct ExtParam700
{
    const char* id;
    const char* name;
    int steps;
    float lo, hi, def;
    const char* choices;
    bool global = false;
};

inline const std::vector<ExtParam700>& extParams700()
{
    static const std::vector<ExtParam700> p = {
        // Voces
        { "voices",      "Keyboard Voices",   5, 0, 4, 0,     "Duo (original)|Mono|Poly 4|Poly 6|Poly 8" },
        { "droop",       "Key CV Droop",      2, 0, 1, 0,     "Off|On" },
        // Carácter
        { "vcf1_model",  "VCF-1 Model",       2, 0, 1, 0,     "703E 24 dB LP|703A 12 dB multimode" },
        { "vcf1_mode",   "703A Filter Mode",  3, 0, 2, 2,     "HP|BP|LP" },
        { "drift",       "VCO Drift",         0, 0, 1, 0.15f, nullptr },
        // Secuenciador
        { "seq_clock",   "Sequencer Clock",   2, 0, 1, 0,     "Internal|Host" },
        { "seq_follow",  "Follow Transport",  2, 0, 1, 1,     "Off|On" },
        // MIDI
        { "midi_bend",   "MIDI Pitch Bend",   2, 0, 1, 1,     "Off|Bender" },
        // Sistema
        { "quality",     "Oversampling",      3, 0, 2, 1,     "1x|2x|4x", true },
        { "output",      "Output",            0, -24, 6, 0,   nullptr, true },
    };
    return p;
}

inline const ExtParam700* findExt700 (const juce::String& id)
{
    for (auto& e : extParams700()) if (id == e.id) return &e;
    return nullptr;
}

inline juce::StringArray extChoices700 (const ExtParam700& e)
{
    return e.choices != nullptr ? juce::StringArray::fromTokens (e.choices, "|", "") : juce::StringArray();
}

inline juce::String formatExt700 (const ExtParam700& e, float v)
{
    if (e.steps > 0) return extChoices700 (e)[juce::jlimit (0, e.steps - 1, (int) std::lround (v))];
    const juce::String id (e.id);
    if (id == "output") return (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB";
    return juce::String (v * 10.0f, 1);
}

constexpr int kOversampling700[3] = { 1, 2, 4 };

template <typename Get>
inline void applyExt700 (Get&& get, Engine700& e)
{
    const int vm = (int) std::lround (get ("voices"));
    e.kb.mode = vm == 0 ? Keyboard701::Duo : (vm == 1 ? Keyboard701::Mono : Keyboard701::Poly);
    e.kb.polyVoices = vm == 2 ? 4 : (vm == 3 ? 6 : 8);
    e.kb.droop = get ("droop") > 0.5f;
    e.st.vcf1Model = (int) std::lround (get ("vcf1_model"));
    e.st.vcf1Mode = (int) std::lround (get ("vcf1_mode"));
    e.st.drift = get ("drift");
    e.st.seqHost = get ("seq_clock") > 0.5f;
    e.st.seqFollowTransport = get ("seq_follow") > 0.5f;
    e.st.outputGain = juce::Decibels::decibelsToGain (get ("output"));
}

} // namespace dh
