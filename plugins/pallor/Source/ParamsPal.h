#pragma once
// Parámetros de Pallor. Cada uno de los 16 instrumentos tiene los mismos 24 controles con el prefijo de su slot
// ("bd_tune", "sd_decay"…); después el transporte y el secuenciador, el envío A (Space), el envío B (Echo) y el maestro.
// Los ids son estables: la interfaz de la fase 2 se arma sobre ellos sin tocar el motor.

#include <JuceHeader.h>
#include "Engine.h"

namespace dh::pal::params {

inline const char* voiceKey (int s) { static const char* k[kNumSlots] = { "bd", "sd", "cp", "rs", "ch", "oh", "cy", "rd", "t1", "t2", "t3", "t4", "cb", "cl", "tb", "fx" }; return k[std::clamp (s, 0, kNumSlots - 1)]; }
inline const char* voiceName (int s)
{
    static const char* n[kNumSlots] = { "Kick", "Snare", "Clap", "Rim", "Closed Hat", "Open Hat", "Crash", "Ride", "Tom 1", "Tom 2", "Tom 3", "Tom 4", "Cowbell", "Clave", "Tambourine", "FX" };
    return n[std::clamp (s, 0, kNumSlots - 1)];
}
inline juce::String vid (int s, const char* suffix) { return juce::String (voiceKey (s)) + "_" + suffix; }

inline juce::StringArray modelNames (int s) { juce::StringArray a; for (const auto& m : analogModels (familyOf (s))) a.add (m.name); return a; }
inline juce::StringArray kitNames() { juce::StringArray a; for (const auto& k : kitgen::kits()) a.add (k.name); a.add ("User sample"); return a; }
inline juce::StringArray chainNames() { return { "Kit default", "8-bit", "12-bit 26k", "12-bit 40k", "Clean" }; }
inline juce::StringArray filterNames() { return { "Off", "Low-pass", "High-pass", "Band-pass" }; }
inline juce::StringArray chokeNames() { return { "None", "1", "2", "3", "4" }; }
inline juce::StringArray busNames() { juce::StringArray a; for (int b = 0; b < kBuses; ++b) a.add (busName (b)); return a; }
inline juce::StringArray modeNames() { return { "A", "B", "A>B", "Song" }; }
inline juce::StringArray groupNames() { juce::StringArray a; for (int g = 0; g < kGroups; ++g) a.add (juce::String (g + 1)); return a; }
inline juce::StringArray autofillNames() { return { "Off", "Every 2", "Every 4", "Every 8", "Every 16" }; }
inline int autofillBars (int i) { static const int b[5] = { 0, 2, 4, 8, 16 }; return b[std::clamp (i, 0, 4)]; }
inline juce::StringArray divisionNames() { return { "1/32", "1/16T", "1/16", "1/8T", "1/16D", "1/8", "1/4T", "1/8D", "1/4", "1/4D", "1/2", "1/2D", "1 bar" }; }
inline float divisionBeats (int i) { static const float b[13] = { 0.125f, 1.0f / 6.0f, 0.25f, 1.0f / 3.0f, 0.375f, 0.5f, 2.0f / 3.0f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f }; return b[std::clamp (i, 0, 12)]; }
inline juce::StringArray namesOf (const char* (*f) (int), int n) { juce::StringArray a; for (int i = 0; i < n; ++i) a.add (f (i)); return a; }

// Los controles de cada instrumento (sufijo, nombre, rango, por defecto).
struct VDef { const char* suffix; const char* name; float min, max, def, skew; const char* label; };
inline const std::vector<VDef>& voiceFloats()
{
    static const std::vector<VDef> d {
        { "tune", "Tune", -24, 24, 0, 1, "st" },
        { "decay", "Decay", 0, 100, 50, 1, "%" },
        { "tone", "Tone", 0, 100, 50, 1, "%" },
        { "char1", "Character 1", 0, 100, 50, 1, "%" },
        { "char2", "Character 2", 0, 100, 50, 1, "%" },
        { "level", "Level", -60, 6, 0, 2.2f, "dB" },
        { "pan", "Pan", -100, 100, 0, 1, "%" },
        { "accent", "Accent", 0, 100, 50, 1, "%" },
        { "velsens", "Velocity", 0, 100, 70, 1, "%" },
        { "drive", "Drive", 0, 100, 0, 1, "%" },
        { "bits", "Bits", 2, 16, 16, 1, "bit" },
        { "rate", "Rate", 1000, 48000, 48000, 0.4f, "Hz" },
        { "cutoff", "Cutoff", 20, 20000, 1000, 0.25f, "Hz" },
        { "reso", "Resonance", 0, 100, 10, 1, "%" },
        { "senda", "Send A", 0, 100, 0, 1, "%" },
        { "sendb", "Send B", 0, 100, 0, 1, "%" },
    };
    return d;
}

struct Def { const char* id; const char* name; float min, max, def, skew; const char* label; };
inline const std::vector<Def>& globalFloats()
{
    static const std::vector<Def> d {
        { "tempo", "Tempo", 40, 300, 120, 1, "BPM" },
        { "swing", "Swing", 50, 75, 50, 1, "%" },
        { "accent", "Accent", 0, 100, 80, 1, "%" },
        { "seed", "Chance seed", 1, 999, 1, 1, "" },
        { "midi_ch", "MIDI out channel", 1, 16, 10, 1, "" },
        { "accent_vel", "Accent velocity", 1, 127, 120, 1, "" },
        { "midi_gate", "MIDI out gate", 2, 1000, 30, 0.4f, "ms" },
        // Envío A
        { "a_size", "Space Size", 0, 100, 50, 1, "%" },
        { "a_predelay", "Space Pre-delay", 0, 250, 0, 0.5f, "ms" },
        { "a_gate", "Space Gate", 40, 1200, 250, 0.5f, "ms" },
        { "a_shape", "Space Shape", -100, 100, 0, 1, "%" },
        { "a_tone", "Space Tone", 0, 100, 60, 1, "%" },
        { "a_lowcut", "Space Low cut", 20, 1000, 150, 0.4f, "Hz" },
        { "a_width", "Space Width", 0, 100, 100, 1, "%" },
        { "a_return", "Space Return", -60, 6, 0, 2.2f, "dB" },
        // Envío B
        { "b_fb", "Echo Feedback", 0, 95, 35, 1, "%" },
        { "b_tone", "Echo Tone", 0, 100, 50, 1, "%" },
        { "b_rate", "Echo Rate", 0, 100, 30, 1, "%" },
        { "b_depth", "Echo Depth", 0, 100, 50, 1, "%" },
        { "b_width", "Echo Width", 0, 100, 100, 1, "%" },
        { "b_return", "Echo Return", -60, 6, 0, 2.2f, "dB" },
        // Maestro
        { "m_lofi_mix", "Lo-fi Mix", 0, 100, 100, 1, "%" },
        { "m_tape_drive", "Tape Drive", -12, 18, 0, 1, "dB" },
        { "m_comp", "Glue", 0, 100, 0, 1, "%" },
        { "m_comp_mix", "Glue Mix", 0, 100, 100, 1, "%" },
        { "m_exciter", "Exciter", 0, 100, 0, 1, "%" },
        { "m_exciter_freq", "Exciter Freq", 1000, 8000, 3500, 0.5f, "Hz" },
        { "m_volume", "Volume", -24, 6, 0, 1, "dB" },
    };
    return d;
}

inline int defaultChoke (int s) { return s == kClosedHat || s == kOpenHat ? 1 : 0; }

// El texto de un valor: sin la cola de decimales de juce::String (x, 0), con la unidad y "Off" donde corresponde.
inline juce::AudioParameterFloatAttributes textFor (const char* id, const char* label)
{
    const juce::String sid (id), lab (label);
    return juce::AudioParameterFloatAttributes().withLabel (label).withStringFromValueFunction ([sid, lab] (float v, int) -> juce::String {
        if (sid.endsWith ("rate") && ! sid.startsWith ("b_") && v >= 47999.0f) return "Off";
        if ((sid.endsWith ("level") || sid.endsWith ("return")) && v <= -59.9f) return "Off";
        if (sid.endsWith ("bits") && v >= 15.99f) return "Off";
        if (lab == "Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " kHz" : juce::String (juce::roundToInt (v)) + " Hz";
        if (lab == "dB" || lab == "st") return (v > 0.04f ? "+" : "") + juce::String (v, 1) + " " + lab;
        if (lab == "BPM") return juce::String (v, 1) + " BPM";
        if (lab == "ms") return juce::String (juce::roundToInt (v)) + " ms";
        if (lab == "bit") return juce::String (juce::roundToInt (v)) + " bit";
        if (lab == "%") return juce::String (juce::roundToInt (v)) + " %";
        return juce::String (juce::roundToInt (v));
    });
}

inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto whole = [] (const char* id) { const String s (id); return s == "seed" || s == "midi_ch" || s == "accent_vel" || s.endsWith ("_bits"); };
    for (int s = 0; s < kNumSlots; ++s)
    {
        const String vn (voiceName (s));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { vid (s, "engine"), 1 }, vn + " Engine", StringArray { "Analog", "Digital" }, 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { vid (s, "model"), 1 }, vn + " Model", modelNames (s), 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { vid (s, "kit"), 1 }, vn + " Kit", kitNames(), 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { vid (s, "chain"), 1 }, vn + " Chain", chainNames(), 0));
        for (const auto& d : voiceFloats())
        {
            const String id = vid (s, d.suffix);
            NormalisableRange<float> r (d.min, d.max, String (d.suffix) == "bits" || String (d.suffix) == "tune" ? 0.0f : 0.0f, d.skew);
            if (String (d.suffix) == "bits") r.interval = 1.0f;
            l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, vn + " " + d.name, r, d.def, textFor (d.suffix, d.label)));
        }
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { vid (s, "filter"), 1 }, vn + " Filter", filterNames(), 0));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { vid (s, "choke"), 1 }, vn + " Choke", chokeNames(), defaultChoke (s)));
        l.add (std::make_unique<AudioParameterChoice> (ParameterID { vid (s, "out"), 1 }, vn + " Output", busNames(), groupBus (s)));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { vid (s, "reverse"), 1 }, vn + " Reverse", false));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { vid (s, "mute"), 1 }, vn + " Mute", false));
        l.add (std::make_unique<AudioParameterBool> (ParameterID { vid (s, "solo"), 1 }, vn + " Solo", false));
    }
    for (const auto& d : globalFloats())
    {
        NormalisableRange<float> r (d.min, d.max, whole (d.id) ? 1.0f : 0.0f, d.skew);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, r, d.def, textFor (d.id, d.label)));
    }
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "run", 1 }, "Run", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "sync", 1 }, "Follow host", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "mode", 1 }, "Play mode", modeNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "group", 1 }, "Pattern", groupNames(), 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "fill", 1 }, "Fill", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "autofill", 1 }, "Auto fill", autofillNames(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "swing_res", 1 }, "Swing on", StringArray { "8ths", "16ths" }, 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "note_map", 1 }, "Note map", StringArray { "GM", "Chromatic 36-51" }, 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "midi_out", 1 }, "MIDI out", true));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "routing", 1 }, "Outputs", StringArray { "Main only", "Per voice" }, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "a_type", 1 }, "Space Type", namesOf (spaceTypeName, kNumSpaceTypes), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "b_type", 1 }, "Echo Type", namesOf (echoTypeName, kNumEchoTypes), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "b_time", 1 }, "Echo Time", divisionNames(), 7));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "m_lofi", 1 }, "Lo-fi", namesOf (lofiName, kNumLofi), 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "m_tape", 1 }, "Tape", false));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "m_ratio", 1 }, "Glue Ratio", namesOf (ratioName, 3), 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "m_attack", 1 }, "Glue Attack", namesOf (attackName, 6), 3));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "m_release", 1 }, "Glue Release", namesOf (releaseName, 5), 4));
    return l;
}

} // namespace dh::pal::params
