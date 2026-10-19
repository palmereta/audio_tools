#pragma once
// Parámetros de Plumb. Al frente: tonalidad, escala, voz, Amount, Speed, Style y el modo de latencia. Style es una
// macro de Natural a Hard que fija el carácter de la corrección (transición, humanize, flex, cuánto vibrato y deriva
// se conservan, histéresis); con "Custom character" esos valores se ajustan a mano en la página avanzada. Al 100 %
// Style es afinación dura: la salida es la nota exacta.

#include <JuceHeader.h>
#include "Engine.h"

namespace dh::pl::params {

struct Def { const char* id; const char* name; float min, max, def, skew; const char* label; };

inline const std::vector<Def>& floats()
{
    static const std::vector<Def> d {
        { "amount", "Amount", 0, 100, 100, 1, "%" },
        { "speed", "Speed", 0, 400, 20, 0.45f, "ms" },
        { "style", "Style", 0, 100, 25, 1, "%" },
        // Carácter (con Custom character)
        { "transition", "Transition", 0, 200, 45, 0.6f, "ms" },
        { "humanize", "Humanize", 0, 100, 30, 1, "%" },
        { "flex", "Flex", 0, 200, 62, 0.6f, "cents" },
        { "hysteresis", "Hysteresis", 0, 50, 11, 1, "cents" },
        { "keep_vibrato", "Vibrato kept", 0, 100, 75, 1, "%" },
        { "keep_drift", "Drift kept", 0, 100, 22, 1, "%" },
        // Altura y formantes
        { "transpose", "Transpose", -24, 24, 0, 1, "st" },
        { "fine", "Fine", -100, 100, 0, 1, "cents" },
        { "formant_amount", "Formant amount", 0, 100, 100, 1, "%" },
        { "throat", "Throat", -12, 12, 0, 1, "st" },
        { "vib_rate", "Vibrato rate", 3, 9, 5.5f, 1, "Hz" },
        { "vib_depth", "Vibrato depth", 0, 100, 0, 1, "cents" },
        { "vib_delay", "Vibrato delay", 0, 1000, 250, 0.6f, "ms" },
        // Detector, referencia y salida
        { "ref_a", "Reference A", 415, 466, 440, 1, "Hz" },
        { "gate", "Gate", -80, -20, -55, 1, "dB" },
        { "bend_range", "Bend range", 0, 12, 2, 1, "st" },
        { "mix", "Mix", 0, 100, 100, 1, "%" },
        { "output", "Output", -24, 12, 0, 1, "dB" },
    };
    return d;
}

inline const juce::StringArray keys() { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
// Escalas: nombre y máscara de grados (bit 0 = la tónica).
struct Scale { const char* name; uint16_t mask; };
inline const std::vector<Scale>& scales()
{
    static const std::vector<Scale> s {
        { "Chromatic", 0x0FFF }, { "Major", 0x0AB5 }, { "Minor", 0x05AD }, { "Harmonic minor", 0x09AD }, { "Melodic minor", 0x0AAD },
        { "Dorian", 0x06AD }, { "Phrygian", 0x05AB }, { "Lydian", 0x0AD5 }, { "Mixolydian", 0x06B5 }, { "Locrian", 0x056B },
        { "Major pentatonic", 0x0295 }, { "Minor pentatonic", 0x04A9 }, { "Blues", 0x04E9 }, { "Custom", 0x0000 },
    };
    return s;
}
inline juce::StringArray scaleNames() { juce::StringArray a; for (auto& s : scales()) a.add (s.name); return a; }
inline int customScale() { return (int) scales().size() - 1; }
inline const juce::StringArray voices() { return { "Low", "Mid", "High", "Instrument" }; }
inline const juce::StringArray modes() { return { "Live", "Tight", "Studio" }; }
inline const juce::StringArray midiModes() { return { "Off", "Scale from held notes", "Note lock", "Auto" }; }
inline const juce::StringArray degreeStates() { return { "Snap", "Skip", "Bypass" }; }

inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    for (auto& d : floats())
    {
        NormalisableRange<float> r (d.min, d.max, 0.0f, d.skew);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, r, d.def, AudioParameterFloatAttributes().withLabel (d.label)));
    }
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "key", 1 }, "Key", keys(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scale", 1 }, "Scale", scaleNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "voice", 1 }, "Voice", voices(), 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Latency mode", modes(), 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "midi_mode", 1 }, "MIDI", midiModes(), 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "custom_character", 1 }, "Custom character", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "formants", 1 }, "Formant preservation", true));
    // Escala propia: cada clase de altura (relativa a C) es objetivo, se saltea o no se corrige.
    for (int i = 0; i < 12; ++i)
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { "deg" + String (i), 1 }, "Custom " + keys()[i], degreeStates(), 0));
    return l;
}

// El carácter que fija Style (0 = Natural, 1 = Hard), docs/research.md §4.5.
struct Character { float speedMs, transitionMs, humanize, flexIn, flexOut, keepV, keepD, hysteresis; };
inline Character characterFor (float style)
{
    const float s = juce::jlimit (0.0f, 1.0f, style);
    auto mix = [s] (float a, float b) { return a + (b - a) * s; };
    // La ventana flex se abre hasta "siempre" (600 cents) recién hacia Hard: 60 cents · 10^(s³). En Natural corrige
    // cualquier nota a menos de 60 cents de su objetivo y deja pasar entero lo que se aleja más de 120 (bends, glissandi).
    const float flexIn = 60.0f * std::pow (600.0f / 60.0f, s * s * s);
    return { mix (40, 0), mix (60, 0), mix (0.4f, 0), flexIn, std::min (600.0f, flexIn * 2), mix (1, 0), mix (0.3f, 0), mix (0.15f, 0.06f) };
}

} // namespace dh::pl::params
