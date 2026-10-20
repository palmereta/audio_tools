#pragma once
// Parámetros del AT303. Al frente, los del original: Tuning, Cutoff, Resonance, Env Mod, Decay, Accent, la forma
// de onda, Tempo y Volume. El secuenciador (patrón, transposición, modo), la página extendida (las modificaciones
// clásicas del circuito, con un botón "de fábrica"), la salida (distorsión y delay) y el generador.

#include <JuceHeader.h>
#include "Engine303.h"

namespace dh::tb::params {

struct Def { const char* id; const char* name; float min, max, def, skew; const char* label; };

inline const std::vector<Def>& floats()
{
    static const std::vector<Def> d {
        { "tuning", "Tuning", -7, 7, 0, 1, "st" },
        { "cutoff", "Cutoff", 0, 100, 45, 1, "%" },
        { "resonance", "Resonance", 0, 100, 60, 1, "%" },
        { "envmod", "Env Mod", 0, 100, 50, 1, "%" },
        { "decay", "Decay", 0, 100, 40, 1, "%" },
        { "accent", "Accent", 0, 100, 60, 1, "%" },
        { "volume", "Volume", 0, 100, 80, 1, "%" },
        { "tempo", "Tempo", 40, 300, 125, 1, "BPM" },
        { "transpose", "Transpose", -12, 12, 0, 1, "st" },
        { "accent_vel", "Accent velocity", 1, 127, 100, 1, "" },
        // Página extendida
        { "overdrive", "Overdrive", 0, 100, 0, 0.5f, "%" },
        { "normal_decay", "Normal decay", 0, 3000, 0, 0.5f, "ms" },       // 0 = la perilla Decay
        { "accent_decay", "Accent decay", 30, 3000, 200, 0.4f, "ms" },
        { "soft_attack", "Soft attack", 0.3f, 30, 3, 0.4f, "ms" },
        { "slide_time", "Slide time", 60, 360, 60, 0.6f, "ms" },
        { "tracking", "Filter tracking", 0, 100, 0, 1, "%" },
        { "filter_fm", "Filter FM", 0, 100, 0, 0.6f, "%" },
        { "env_range", "Env Mod range", 100, 300, 100, 1, "%" },
        { "cutoff_top", "Cutoff top", 2400, 5000, 2400, 1, "Hz" },
        { "vca_click", "VCA click", 0, 200, 100, 1, "%" },
        // Salida
        { "drive", "Drive", 0, 100, 40, 1, "%" },
        { "drive_tone", "Drive tone", 0, 100, 60, 1, "%" },
        { "delay_mix", "Delay", 0, 100, 0, 1, "%" },
        { "delay_fb", "Delay feedback", 0, 95, 35, 1, "%" },
        { "delay_tone", "Delay tone", 0, 100, 50, 1, "%" },
        // Generador
        { "gen_density", "Density", 0, 100, 70, 1, "%" },
        { "gen_accent", "Accents", 0, 100, 30, 1, "%" },
        { "gen_slide", "Slides", 0, 100, 20, 1, "%" },
        { "gen_ties", "Ties", 0, 100, 15, 1, "%" },
    };
    return d;
}

inline const juce::StringArray waveforms() { return { "Saw", "Square" }; }
inline const juce::StringArray modes() { return { "Sequencer", "MIDI" }; }
inline const juce::StringArray sweepSpeeds() { return { "Fast", "Normal", "Slow" }; }
inline const juce::StringArray mufflers() { return { "Off", "Soft", "Hard" }; }
inline const juce::StringArray driveTypes() { return { "Off", "Soft clip", "Hard clip", "Fuzz" }; }
inline const juce::StringArray delayTimes() { return { "1/16", "1/8", "1/8 dotted", "1/4", "1/4 dotted", "1/2" }; }
inline double delayBeats (int i) { static const double b[] = { 0.25, 0.5, 0.75, 1.0, 1.5, 2.0 }; return b[juce::jlimit (0, 5, i)]; }
inline const juce::StringArray qualities() { return { "Eco (2x)", "Normal (4x)", "High (8x)" }; }
inline const juce::StringArray roots() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
inline juce::StringArray scaleNames() { juce::StringArray a; for (int i = 0; i < 7; ++i) a.add (dh::tb::scaleName (i)); return a; }
inline juce::String patternName (int i) { static const char* g[] = { "I", "II", "III", "IV" }; return juce::String (g[(i / 16) & 3]) + "-" + (((i / 8) & 1) ? "B" : "A") + juce::String (i % 8 + 1); }
inline juce::StringArray patternNames() { juce::StringArray a; for (int i = 0; i < 64; ++i) a.add (patternName (i)); return a; }

inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& d : floats())
    {
        // Transpose y Accent velocity son enteros en el motor: con paso 1 la perilla muestra lo que suena.
        const bool whole = juce::String (d.id) == "transpose" || juce::String (d.id) == "accent_vel";
        NormalisableRange<float> r (d.min, d.max, whole ? 1.0f : 0.0f, d.skew);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, r, d.def, AudioParameterFloatAttributes().withLabel (d.label)));
    }
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "waveform", 1 }, "Waveform", waveforms(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "pattern", 1 }, "Pattern", patternNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Mode", modes(), 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "run", 1 }, "Run", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Follow host", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "sweep_speed", 1 }, "Accent sweep speed", sweepSpeeds(), 1));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "sweep_on", 1 }, "Accent sweep", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "muffler", 1 }, "Muffler", mufflers(), 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "high_res", 1 }, "High resonance", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "env_zero", 1 }, "Env Mod to zero", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "bass_fix", 1 }, "Full bass", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "note_delay", 1 }, "Note start delay", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "drive_type", 1 }, "Drive type", driveTypes(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "delay_time", 1 }, "Delay time", delayTimes(), 2));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "quality", 1 }, "Quality", qualities(), 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "gen_root", 1 }, "Root", roots(), 9));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "gen_scale", 1 }, "Scale", scaleNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "gen_octaves", 1 }, "Octaves", juce::StringArray { "1", "2", "3" }, 1));
    return l;
}

} // namespace dh::tb::params
