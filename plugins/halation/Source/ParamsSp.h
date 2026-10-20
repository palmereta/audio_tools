#pragma once
// Parámetros de Halation. Al frente, seis: Space (delay ↔ reverb), Time, Size, Length, Tone y Mix. Debajo
// el carácter y los módulos especiales, cada uno con un interruptor y dos o tres ajustes. Todo lo demás (escenas,
// envolvente) se resuelve en el procesador antes de pasar los valores al motor.

#include <JuceHeader.h>
#include "Engine.h"

namespace dh::sp::params {

struct Def { const char* id; const char* name; float min, max, def, skew; const char* label; };

// Divisiones sincronizadas (en negras).
struct Division { const char* name; float beats; };
inline const std::array<Division, 16>& divisions()
{
    static const std::array<Division, 16> d {{
        { "1/32", 0.125f }, { "1/16T", 1.0f / 6.0f }, { "1/16", 0.25f }, { "1/16D", 0.375f }, { "1/8T", 1.0f / 3.0f }, { "1/8", 0.5f },
        { "1/8D", 0.75f }, { "1/4T", 2.0f / 3.0f }, { "1/4", 1.0f }, { "1/4D", 1.5f }, { "1/2T", 4.0f / 3.0f }, { "1/2", 2.0f },
        { "1/2D", 3.0f }, { "1 bar", 4.0f }, { "2 bars", 8.0f }, { "4 bars", 16.0f },
    }};
    return d;
}

inline const std::vector<Def>& floats()
{
    static const std::vector<Def> d {
        { "space", "Delay / Space", 0, 100, 35, 1, "%" },
        { "time_ms", "Time", 1, 4000, 375, 0.35f, "ms" },
        { "size", "Size", 25, 400, 100, 0.5f, "%" },
        { "length", "Length", 0, 100, 55, 1, "%" },
        { "tone", "Tone", 0, 100, 60, 1, "%" },
        { "mix", "Mix", 0, 100, 35, 1, "%" },
        { "predelay", "Pre-delay", 0, 250, 0, 0.5f, "ms" },
        { "lowcut", "Low cut", 20, 800, 40, 0.4f, "Hz" },
        { "bass", "Bass decay", 30, 200, 100, 1, "%" },
        { "diffusion", "Diffusion", 0, 100, 70, 1, "%" },
        { "mod_depth", "Modulation", 0, 100, 25, 1, "%" },
        { "mod_rate", "Mod rate", 0.05f, 6, 0.4f, 0.4f, "Hz" },
        { "width", "Width", 0, 150, 100, 1, "%" },
        { "wear", "Wear", 0, 100, 20, 1, "%" },
        { "shimmer", "Shimmer", 0, 100, 40, 1, "%" },
        { "grains", "Grains", 0, 100, 40, 1, "%" },
        { "grain_ms", "Grain size", 20, 500, 120, 0.5f, "ms" },
        { "reverse", "Reverse", 0, 100, 60, 1, "%" },
        { "spectral", "Smear", 0, 100, 50, 1, "%" },
        { "resonance", "Resonance", 0, 100, 50, 1, "%" },
        { "res_root", "Root note", 24, 96, 57, 1, "" },
        { "duck", "Duck", 0, 100, 50, 1, "%" },
        { "gate", "Gate", 0, 100, 80, 1, "%" },
        { "gate_hold", "Gate hold", 20, 1500, 250, 0.5f, "ms" },
        { "stutter", "Stutter chance", 0, 100, 35, 1, "%" },
        { "breathe_depth", "Breathe depth", 0, 100, 40, 1, "%" },
        { "breathe_rate", "Breathe rate", 0.01f, 1, 0.08f, 0.4f, "Hz" },
        { "env_amount", "Envelope", -100, 100, 0, 1, "%" },
        { "morph", "Morph A/B", 0, 100, 0, 1, "%" },
        { "out_gain", "Output", -24, 12, 0, 1, "dB" },
    };
    return d;
}

inline const juce::StringArray characters() { return { "Digital", "Tape", "BBD", "Spring" }; }
inline const juce::StringArray roomTypes() { return { "Room", "Hall", "Plate", "Chamber" }; }
inline const juce::StringArray chords() { return { "Single", "Fifth", "Major", "Minor", "Sus", "Octaves" }; }
inline const juce::StringArray freezeModes() { return { "All", "Lows", "Mids", "Highs", "Lows + mids" }; }
inline const juce::StringArray breatheModes() { return { "Breathe", "Collapse" }; }
inline const juce::StringArray envTargets() { return { "Space", "Size", "Shimmer", "Tone", "Mix" }; }
inline juce::StringArray divisionNames() { juce::StringArray a; for (auto& d : divisions()) a.add (d.name); return a; }
inline juce::StringArray patternNames() { juce::StringArray a; for (auto& p : patterns()) a.add (p.name); return a; }
inline juce::StringArray intervalNames() { juce::StringArray a; for (auto& i : intervals()) a.add (i.name); return a; }

// Interruptores de los módulos (en el orden de la fila de la interfaz).
inline const juce::StringArray& modules()
{
    static const juce::StringArray m { "freeze", "shimmer", "reverse", "grains", "spectral", "resonator", "stutter", "breathe", "duck", "gate" };
    return m;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& d : floats())
    {
        NormalisableRange<float> r (d.min, d.max, 0.0f, d.skew);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, r, d.def, AudioParameterFloatAttributes().withLabel (d.label)));
    }
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Sync to tempo", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "division", 1 }, "Division", divisionNames(), 6));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "pattern", 1 }, "Pattern", patternNames(), 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "pingpong", 1 }, "Ping-pong", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "character", 1 }, "Character", characters(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "room", 1 }, "Room", roomTypes(), 1));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "elastic", 1 }, "Tape-style time changes", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "interval", 1 }, "Shimmer interval", intervalNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "chord", 1 }, "Resonator chord", chords(), 2));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "res_midi", 1 }, "Resonator follows MIDI", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "freeze_mode", 1 }, "Freeze bands", freezeModes(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "reverse_div", 1 }, "Reverse length", divisionNames(), 8));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "stutter_div", 1 }, "Stutter length", divisionNames(), 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "breathe_mode", 1 }, "Impossible room", breatheModes(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "env_target", 1 }, "Envelope target", envTargets(), 1));
    for (auto& m : modules())
        l.add (std::make_unique<AudioParameterBool> (ParameterID { "on_" + m, 1 }, "On: " + m, false));
    return l;
}

} // namespace dh::sp::params
