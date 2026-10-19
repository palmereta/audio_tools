#pragma once
// Parámetros que no son del panel del SH-101 (página SETTINGS): voces, expresión, reloj, carácter,
// efectos y sistema. Una sola tabla para el plugin, la interfaz, los presets y at101vrender.

#include <JuceHeader.h>
#include "dsp/Engine101.h"

namespace dh {

struct ExtParam
{
    const char* id;
    const char* name;
    int steps;                   // 0 = continuo; >0 = opciones
    float lo, hi, def;
    const char* choices;
    bool global = false;         // ajuste del usuario: un preset no lo pisa
};

inline const std::vector<ExtParam>& extParams()
{
    static const std::vector<ExtParam> p = {
        // Voces
        { "voices",     "Voices",          5, 0, 4, 0,     "Mono|2|4|6|8" },
        { "unison",     "Unison",          4, 0, 3, 0,     "1|2|3|4" },
        { "detune",     "Unison Detune",   0, 0, 50, 14,   nullptr },
        { "spread",     "Stereo Spread",   0, 0, 1, 0.6f,  nullptr },
        { "analog",     "Analog Drift",    0, 0, 1, 0.3f,  nullptr },
        // Expresión
        { "vel_vcf",    "Velocity > VCF",  0, 0, 1, 0.0f,  nullptr },
        { "vel_vca",    "Velocity > VCA",  0, 0, 1, 0.0f,  nullptr },
        { "accent",     "Accent",          0, 0, 1, 0.5f,  nullptr },
        { "slide_time", "Slide Time",      0, 0.02f, 0.3f, 0.06f, nullptr },
        // Reloj, arpegio y secuencia
        { "clock_src",  "Clock",           2, 0, 1, 0,     "Modulator|Host Tempo" },
        { "clock_div",  "Clock Division",  6, 0, 5, 3,     "1/4|1/8|1/8 T|1/16|1/16 T|1/32" },
        { "swing",      "Swing",           0, 0, 0.75f, 0, nullptr },
        { "gate_len",   "Gate Length",     0, 0.05f, 1.0f, 0.5f, nullptr },
        { "arp_oct",    "Arp Octaves",     4, 0, 3, 0,     "1|2|3|4" },
        { "seq_keytr",  "Keys Transpose Seq", 2, 0, 1, 1,  "Off|On" },
        // Carácter
        { "drive",      "Drive",           0, 0, 1, 0.0f,  nullptr },
        { "quality",    "Oversampling",    3, 0, 2, 1,     "2x|4x|8x", true },
        // Chorus
        { "chorus",     "Chorus",          4, 0, 3, 0,     "Off|I|II|I+II" },
        { "chorus_mix", "Chorus Mix",      0, 0, 1, 0.5f,  nullptr },
        // Delay
        { "dly_on",     "Delay",           2, 0, 1, 0,     "Off|On" },
        { "dly_time",   "Delay Time",      12, 0, 11, 6,   "1/32|1/16 T|1/16|1/16 D|1/8 T|1/8|1/8 D|1/4 T|1/4|1/4 D|1/2|1 bar" },
        { "dly_fb",     "Delay Feedback",  0, 0, 0.95f, 0.35f, nullptr },
        { "dly_tone",   "Delay Tone",      0, 0, 1, 0.6f,  nullptr },
        { "dly_mix",    "Delay Mix",       0, 0, 1, 0.25f, nullptr },
        // Reverb
        { "rev_on",     "Reverb",          2, 0, 1, 0,     "Off|On" },
        { "rev_size",   "Reverb Size",     0, 0, 1, 0.6f,  nullptr },
        { "rev_damp",   "Reverb Damping",  0, 0, 1, 0.4f,  nullptr },
        { "rev_mix",    "Reverb Mix",      0, 0, 1, 0.2f,  nullptr },
        // Sistema
        { "midi_out",   "MIDI Out",        3, 0, 2, 2,     "Off|Thru|Synth Notes", true },
        { "output",     "Output",          0, -24, 6, 0,   nullptr, true },
        { "body",       "Body Colour",     3, 0, 2, 0,     "Grey|Blue|Red", true },
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
constexpr float kClockBeats[6] = { 1.0f, 0.5f, 1.0f / 3.0f, 0.25f, 1.0f / 6.0f, 0.125f };
constexpr int kVoiceChoices[5] = { 1, 2, 4, 6, 8 };
constexpr int kOversampling[3] = { 2, 4, 8 };

inline juce::String formatExt (const ExtParam& e, float v)
{
    if (e.steps > 0) return extChoices (e)[juce::jlimit (0, e.steps - 1, (int) std::lround (v))];
    const juce::String id (e.id);
    if (id == "detune") return juce::String (v, 1) + " ct";
    if (id == "output") return (v >= 0 ? "+" : "") + juce::String (v, 1) + " dB";
    if (id == "slide_time") return juce::String (v * 1000.0f, 0) + " ms";
    if (id == "swing" || id == "gate_len") return juce::String (v * 100.0f, 0) + " %";
    return juce::String (v * 10.0f, 1);
}

template <typename Get>
inline void applyExtParams (Get&& get, Settings101& st, Fx101& fx)
{
    st.voices = kVoiceChoices[juce::jlimit (0, 4, (int) std::lround (get ("voices")))];
    st.unison = 1 + juce::jlimit (0, 3, (int) std::lround (get ("unison")));
    st.detuneCents = get ("detune");
    st.spread = get ("spread");
    st.analog = get ("analog");
    st.velVcf = get ("vel_vcf");
    st.velVca = get ("vel_vca");
    st.accent = get ("accent");
    st.slideTime = get ("slide_time");
    st.clockSrc = get ("clock_src") > 0.5f ? 1 : 0;
    st.clockBeats = kClockBeats[juce::jlimit (0, 5, (int) std::lround (get ("clock_div")))];
    st.swing = get ("swing");
    st.gateLen = get ("gate_len");
    st.arpOctaves = 1 + juce::jlimit (0, 3, (int) std::lround (get ("arp_oct")));
    st.seqKeyTranspose = get ("seq_keytr") > 0.5f;
    st.drive = get ("drive");
    st.outputGain = juce::Decibels::decibelsToGain (get ("output"));
    fx.chorus = juce::jlimit (0, 3, (int) std::lround (get ("chorus")));
    fx.chorusMix = get ("chorus_mix");
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
