#pragma once
// Parámetros de Lacquer. Pocos a propósito: estilo, intensidad, objetivo de sonoridad y techo. Todo lo demás lo
// decide el aprendizaje (Source/dsp/EngineMaster.h); el panel de detalle sólo lo muestra.

#include <JuceHeader.h>
#include "dsp/EngineMaster.h"

namespace dh::lq::params {

inline const juce::String style = "style", intensity = "intensity", target = "target", ceiling = "ceiling",
                          compare = "compare", preview = "preview";

inline juce::StringArray styleNames() { juce::StringArray a; for (auto& s : styles()) a.add (s.name); return a; }
inline juce::StringArray targetNames()
{
    juce::StringArray a;
    for (auto& t : loudTargets()) a.add (t.matchInput ? juce::String (t.name) : juce::String (t.name) + " " + juce::String (juce::roundToInt (t.lufs)));
    return a;
}
// Vista previa "como lo reproducen las plataformas": las dos señales (A y B) bajan a esa sonoridad si la superan.
struct Platform { const char* name; double lufs; };
inline const std::array<Platform, 4>& platforms()
{
    static const std::array<Platform, 4> p {{ { "Off", 0.0 }, { "Spotify / YouTube", -14.0 }, { "Apple Music", -16.0 }, { "Broadcast", -23.0 } }};
    return p;
}

inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { style, 1 }, "Style", styleNames(), 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { intensity, 1 }, "Intensity", NormalisableRange<float> (0.0f, 100.0f, 1.0f), 50.0f,
                                                  AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { target, 1 }, "Target", targetNames(), 0));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { ceiling, 1 }, "Ceiling", NormalisableRange<float> (-3.0f, -0.1f, 0.1f), -1.0f,
                                                  AudioParameterFloatAttributes().withLabel ("dBTP")));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { compare, 1 }, "Compare (original, same loudness)", false));
    juce::StringArray pv; for (auto& p : platforms()) pv.add (p.name);
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { preview, 1 }, "Hear it as", pv, 0));
    return l;
}

} // namespace dh::lq::params
