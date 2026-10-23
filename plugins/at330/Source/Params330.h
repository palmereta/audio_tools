#pragma once
// Parámetros del AT330. Primero los del panel original (las llaves de cada sección, ataques, release común, tono,
// niveles, vibrato, Pitch Shift, afinación); después lo que el original no tenía, marcado "extra" en
// docs/research.md: el ensemble de las cuerdas apagable y su profundidad, envolventes polifónicas, velocity, el modo
// Freeze, el corrimiento de formantes, el portador externo por la entrada derecha, el pitch bend hacia arriba, la
// rueda de modulación y la reverb. 44 parámetros, todos automatizables (Live llena solo su panel hasta 64).
// Los ids son estables: los presets y el estado guardado dependen de ellos.

#include <JuceHeader.h>
#include "Engine330.h"

namespace dh::vp::params {

struct Def { const char* id; const char* name; float min, max, def, skew; const char* label; };

inline const std::vector<Def>& floats()
{
    static const std::vector<Def> d {
        { "tune", "Tune", -50, 50, 0, 1, "ct" },
        // Cuerdas
        { "str_attack", "Strings Attack", 0.005f, 4.0f, 0.25f, 0.3f, "s" },
        { "str_tone", "Strings Tone", 0, 100, 55, 1, "%" },
        { "str_level", "Strings Level", 0, 100, 80, 1, "%" },
        // Coro
        { "ch_attack", "Choir Attack", 0.005f, 1.5f, 0.15f, 0.35f, "s" },
        { "ch_level", "Choir Level", 0, 100, 80, 1, "%" },
        { "release", "Release", 0.02f, 8.0f, 1.2f, 0.3f, "s" },
        // Vocoder
        { "mic_level", "Mic Level", -24, 24, 0, 1, "dB" },
        { "voc_level", "Vocoder Level", 0, 100, 80, 1, "%" },
        { "voc_tone", "Vocoder Tone", 0, 100, 60, 1, "%" },
        { "direct_mic", "Direct Mic", 0, 100, 0, 1, "%" },
        { "consonants", "Consonants", 0, 100, 50, 1, "%" },
        { "voc_response", "Vocoder Response", 3, 300, 40, 0.4f, "ms" },
        { "formant_shift", "Formant Shift", -12, 12, 0, 1, "st" },
        { "ext_level", "External Level", -24, 24, 0, 1, "dB" },
        // Vibrato (coro y vocoder)
        { "vib_rate", "Vibrato Rate", 4.5f, 11.0f, 6.0f, 1, "Hz" },
        { "vib_depth", "Vibrato Depth", 0, 100, 30, 1, "%" },
        { "vib_delay", "Vibrato Delay", 0, 3, 0.9f, 1, "s" },
        { "mod_vib", "Mod Wheel Vibrato", 0, 100, 50, 1, "%" },
        // Pitch Shift
        { "pitch_set", "Pitch Set", 0, 15, 7, 1, "st" },
        { "pitch_time", "Pitch Time", 0.01f, 2.0f, 0.15f, 0.4f, "s" },
        { "pitch_amount", "Pitch Shift", 0, 100, 0, 1, "%" },
        { "bend_range", "Bend Range", 0, 12, 2, 1, "st" },
        // Ensemble y salida
        { "ens_depth", "Ensemble Depth", 0, 150, 100, 1, "%" },
        { "velocity", "Velocity", 0, 100, 0, 1, "%" },
        { "space_mix", "Space", 0, 100, 0, 1, "%" },
        { "space_size", "Space Size", 0, 100, 60, 1, "%" },
        { "volume", "Volume", -36, 6, 0, 1, "dB" },
    };
    return d;
}

struct Switch { const char* id; const char* name; bool def; };
inline const std::vector<Switch>& switches()
{
    static const std::vector<Switch> d {
        { "str_upper", "Strings Upper 4'", true }, { "str_lower", "Strings Lower 4'", true },
        { "ch_female_up", "Choir Female 4' (Upper)", false }, { "ch_male8_up", "Choir Male 8' (Upper)", false },
        { "ch_male4_lo", "Choir Male 4' (Lower)", false }, { "ch_male8_lo", "Choir Male 8' (Lower)", false },
        { "voc_upper", "Vocoder Upper 8'", false }, { "voc_lower", "Vocoder Lower 8'", false },
        { "ch_ens", "Choir Ensemble", true }, { "voc_ens", "Vocoder Ensemble", true }, { "str_ens", "Strings Ensemble", true },
    };
    return d;
}

inline juce::StringArray holdNames() { return { "Off", "Hold", "Freeze" }; }
inline juce::StringArray carrierNames() { return { "Keyboard", "External (input R)", "Keyboard + External" }; }
inline juce::StringArray pitchModeNames() { return { "Off", "Auto", "Manual" }; }
inline juce::StringArray keyingNames() { return { "Classic", "Poly" }; }
inline juce::String noteName (int n) { static const char* nm[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; return juce::String (nm[n % 12]) + juce::String (n / 12 - 1); }
inline juce::StringArray splitNames() { juce::StringArray a; for (int n = 36; n <= 84; ++n) a.add (noteName (n)); return a; }

// El texto de un valor: sin la cola de decimales de juce::String (x, 0), con la unidad.
inline juce::AudioParameterFloatAttributes textFor (const char* label)
{
    const juce::String lab (label);
    return juce::AudioParameterFloatAttributes().withLabel (label).withStringFromValueFunction ([lab] (float v, int) -> juce::String {
        if (lab == "s") return v < 1.0f ? juce::String (juce::roundToInt (v * 1000.0f)) + " ms" : juce::String (v, 2) + " s";
        if (lab == "dB" || lab == "st") return (v > 0.04f ? "+" : "") + juce::String (v, 1) + " " + lab;
        if (lab == "ct") return (v > 0.04f ? "+" : "") + juce::String (juce::roundToInt (v)) + " ct";
        if (lab == "Hz") return juce::String (v, 1) + " Hz";
        if (lab == "ms") return juce::String (juce::roundToInt (v)) + " ms";
        if (lab == "%") return juce::String (juce::roundToInt (v)) + " %";
        return juce::String (v, 2);
    });
}

inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    for (const auto& d : floats())
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, NormalisableRange<float> (d.min, d.max, 0.0f, d.skew), d.def, textFor (d.label)));
    for (const auto& s : switches())
        l.add (std::make_unique<AudioParameterBool> (ParameterID { s.id, 1 }, s.name, s.def));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "hold", 1 }, "Vocoder Hold", holdNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "carrier", 1 }, "Carrier", carrierNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "pitch_mode", 1 }, "Pitch Shift Mode", pitchModeNames(), 0));
    // Poly por defecto (extra): con las envolventes del original cada tecla nueva corta 50 ms todo lo que suena.
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "keying", 1 }, "Envelopes", keyingNames(), 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "split", 1 }, "Split", splitNames(), 60 - 36));
    return l;
}

} // namespace dh::vp::params
