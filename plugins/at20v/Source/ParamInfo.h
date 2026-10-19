#pragma once
// Nombres visibles y formato de valores de las perillas (para el host y el globo del editor).

#include <JuceHeader.h>
#include "dsp/Engine.h"

namespace dh {

struct KnobInfo { const char* name; const char* section; };

inline const KnobInfo& knobInfo (int i)
{
    static const KnobInfo info[kNumKnobs] = {
        { "Wave Form", "VCO 1" }, { "PW", "VCO 1" }, { "Scale", "VCO 1" },
        { "Wave Form", "VCO 2" }, { "Pitch", "VCO 2" }, { "Scale", "VCO 2" },
        { "Portamento", "" }, { "Master Tune", "" },
        { "VCO 1 Level", "Mixer" }, { "VCO 2 Level", "Mixer" },
        { "MG/T.Ext", "Freq Mod" }, { "EG1/Ext", "Freq Mod" },
        { "Cutoff", "HPF" }, { "Peak", "HPF" }, { "MG/T.Ext", "HPF" }, { "EG2/Ext", "HPF" },
        { "Cutoff", "LPF" }, { "Peak", "LPF" }, { "MG/T.Ext", "LPF" }, { "EG2/Ext", "LPF" },
        { "Wave Form", "MG" }, { "Frequency", "MG" },
        { "Delay", "EG1" }, { "Attack", "EG1" }, { "Release", "EG1" },
        { "Hold", "EG2" }, { "Attack", "EG2" }, { "Decay", "EG2" }, { "Sustain", "EG2" }, { "Release", "EG2" },
        { "Volume", "" },
        { "Signal Level", "ESP" }, { "Low Cut", "ESP" }, { "High Cut", "ESP" }, { "CV Adjust", "ESP" }, { "Threshold", "ESP" },
    };
    return info[i];
}

inline juce::String paramId (int i) { return layout::knobs[i].id; }

inline juce::String paramName (int i)
{
    const auto& k = knobInfo (i);
    return juce::String (k.section).isEmpty() ? juce::String (k.name) : juce::String (k.section) + " " + k.name;
}

inline juce::StringArray choicesFor (int i)
{
    switch ((KnobId) i)
    {
        case KnobId::vco1_wave:  return { "Triangle", "Sawtooth", "Pulse", "Noise" };
        case KnobId::vco1_scale: return { "32'", "16'", "8'", "4'" };
        case KnobId::vco2_wave:  return { "Sawtooth", "Square", "Narrow Pulse", "Ring" };
        case KnobId::vco2_scale: return { "16'", "8'", "4'", "2'" };
        default: return {};
    }
}

inline juce::String fmtTime (float s)
{
    if (s <= 0.0f) return "0 ms";
    return s < 1.0f ? juce::String (s * 1000.0f, s < 0.01f ? 1 : 0) + " ms" : juce::String (s, 2) + " s";
}

inline juce::String fmtHz (float hz)
{
    return hz < 1000.0f ? juce::String (hz, hz < 10.0f ? 2 : (hz < 100.0f ? 1 : 0)) + " Hz" : juce::String (hz / 1000.0f, 2) + " kHz";
}

// Texto del valor: el dial (como en el panel) y la magnitud física cuando aporta.
inline juce::String formatKnob (int i, float v)
{
    const auto c = choicesFor (i);
    if (! c.isEmpty()) return c[juce::jlimit (0, c.size() - 1, (int) std::lround (v))];
    const juce::String dial = juce::String (v * 10.0f, 1);
    const juce::String bip = (v >= 0.5f ? "+" : "") + juce::String ((v - 0.5f) * 10.0f, 1);
    switch ((KnobId) i)
    {
        case KnobId::vco2_pitch:  return bip + "  (" + juce::String (cal::vco2PitchOct (v) * 12.0f, 1) + " st)";
        case KnobId::master_tune: return bip + "  (" + juce::String (cal::masterTuneOct (v) * 1200.0f, 0) + " ct)";
        case KnobId::portamento:  return dial + "  (" + fmtTime (cal::portamento (v)) + ")";
        case KnobId::hpf_cutoff: case KnobId::lpf_cutoff: return dial + "  (" + fmtHz (std::exp2 (cal::cutoffOct (v))) + ")";
        case KnobId::mg_freq:     return dial + "  (" + fmtHz (cal::mgHz (v)) + ")";
        case KnobId::mg_wave:     return v < 0.47f ? juce::String::fromUTF8 ("Ramp ↘ ") + dial : (v > 0.53f ? juce::String::fromUTF8 ("Ramp ↗ ") + dial : "Triangle");
        case KnobId::eg1_delay:   return dial + "  (" + fmtTime (cal::delay (v)) + ")";
        case KnobId::eg2_hold:    return dial + "  (" + fmtTime (cal::hold (v)) + ")";
        case KnobId::eg1_attack: case KnobId::eg2_attack: return dial + "  (" + fmtTime (cal::attack (v)) + ")";
        case KnobId::eg1_release: case KnobId::eg2_decay: case KnobId::eg2_release: return dial + "  (" + fmtTime (cal::decay (v)) + ")";
        case KnobId::esp_lowcut:  return dial + "  (" + fmtHz (cal::espLowCut (v)) + ")";
        case KnobId::esp_highcut: return dial + "  (" + fmtHz (cal::espHighCut (v)) + ")";
        default: return dial;
    }
}

} // namespace dh
