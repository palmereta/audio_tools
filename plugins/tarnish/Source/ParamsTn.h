#pragma once
// Parámetros de Tarnish. Al frente: la máquina y su variante, y ocho perillas (Drive, Saturation, Bias, Wow,
// Flutter, Wear, Noise, Mix). Debajo, por pestañas: el daño (caídas, crujido, rayas, zumbido, azimut, copia), el
// tono (cortes, NR), el crush, la velocidad (varispeed y tape stop), lo propio de cada máquina y el resto.

#include <JuceHeader.h>
#include "Engine.h"

namespace dh::tn::params {

struct Def { const char* id; const char* name; float min, max, def, skew; const char* label; };

inline const std::vector<Def>& floats()
{
    static const std::vector<Def> d {
        { "drive", "Drive", -24, 24, 0, 1, "dB" },
        { "saturation", "Saturation", 0, 100, 50, 1, "%" },
        { "bias", "Bias", -100, 100, 0, 1, "%" },
        { "wow", "Wow", 0, 2, 0, 0.35f, "%" },
        { "flutter", "Flutter", 0, 1, 0, 0.35f, "%" },
        { "wear", "Wear", 0, 100, 0, 1, "%" },
        { "noise", "Noise", 0, 100, 0, 1, "%" },
        { "mix", "Mix", 0, 100, 100, 1, "%" },
        { "dropouts", "Dropouts", 0, 100, 0, 1, "%" },
        { "scrape", "Scrape", 0, 100, 0, 1, "%" },
        { "crackle", "Crackle", 0, 100, 0, 1, "%" },
        { "hum", "Hum", 0, 100, 0, 1, "%" },
        { "azimuth", "Azimuth", 0, 100, 0, 1, "%" },
        { "print", "Print-through", 0, 100, 0, 1, "%" },
        { "lowcut", "Low cut", 10, 2000, 10, 0.3f, "Hz" },
        { "highcut", "High cut", 500, 22000, 22000, 0.3f, "Hz" },
        { "mistrack", "NR mistrack", -6, 6, 0, 1, "dB" },
        { "bits", "Bits", 1, 24, 24, 0.7f, "" },
        { "rate", "Sample rate", 500, 48000, 48000, 0.3f, "Hz" },
        { "jitter", "Jitter", 0, 100, 0, 1, "%" },
        { "varispeed", "Varispeed", -12, 12, 0, 1, "st" },
        { "stop_time", "Stop time", 0.05f, 4, 0.6f, 0.5f, "s" },
        { "start_time", "Start time", 0.05f, 2, 0.35f, 0.5f, "s" },
        { "stop_curve", "Stop curve", 0, 100, 50, 1, "%" },
        { "groove", "Groove position", 0, 100, 0, 1, "%" },
        { "compress", "Station compressor", 0, 100, 60, 1, "%" },
        { "fading", "Fading", 0, 100, 20, 1, "%" },
        { "tuning", "Tuning", -100, 100, 0, 1, "%" },
        { "whistle", "Whistle", 0, 100, 0, 1, "%" },
        { "detector", "Detector", 0, 100, 30, 1, "%" },
        { "frameloss", "Frame loss", 0, 100, 0, 1, "%" },
        { "codec", "Codec", 0, 100, 0, 1, "%" },
        { "out", "Output", -24, 12, 0, 1, "dB" },
    };
    return d;
}

inline const juce::StringArray machines() { return { "Studio", "Cassette", "Video", "Vinyl", "Radio", "Phone" }; }
inline const juce::StringArray speeds() { return { "30 ips", "15 ips", "7.5 ips", "3.75 ips" }; }
inline const juce::StringArray formulations() { return { "Ferric", "Chrome", "Metal", "Half speed" }; }
inline const juce::StringArray videoModes() { return { "SP", "EP" }; }
inline const juce::StringArray rpms() { return { "33", "45", "78" }; }
inline const juce::StringArray bands() { return { "Narrow", "Wide" }; }
inline const juce::StringArray lines() { return { "Landline", "Mobile" }; }
inline const juce::StringArray nrModes() { return { "Off", "Mild", "Strong" }; }
inline const juce::StringArray scratchCounts() { return { "None", "1", "2", "3" }; }
inline const juce::StringArray crushModes() { return { "Clean", "Raw" }; }
inline const juce::StringArray stopSyncs() { return { "Free", "1/16", "1/8", "1/4", "1/2", "1 bar", "2 bars", "4 bars" }; }
inline double stopSyncBeats (int i) { static const double b[] = { 0, 0.25, 0.5, 1, 2, 4, 8, 16 }; return b[juce::jlimit (0, 7, i)]; }
inline const juce::StringArray qualities() { return { "Normal", "Eco" }; }
inline const juce::StringArray eqs() { return { "IEC", "NAB" }; }

// El id del parámetro de variante de cada máquina.
inline const char* variantId (int machine)
{
    static const char* ids[] = { "speed", "formulation", "video", "rpm", "band", "line" };
    return ids[juce::jlimit (0, 5, machine)];
}
inline juce::StringArray variantNames (int machine)
{
    switch (machine) { case 0: return speeds(); case 1: return formulations(); case 2: return videoModes(); case 3: return rpms(); case 4: return bands(); default: return lines(); }
}

inline juce::AudioProcessorValueTreeState::ParameterLayout layout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "machine", 1 }, "Machine", machines(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "speed", 1 }, "Studio speed", speeds(), 1));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "eq", 1 }, "Studio EQ", eqs(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "formulation", 1 }, "Cassette tape", formulations(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "video", 1 }, "Video speed", videoModes(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "rpm", 1 }, "Record speed", rpms(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "band", 1 }, "Radio band", bands(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "line", 1 }, "Phone line", lines(), 0));
    for (auto& d : floats())
    {
        NormalisableRange<float> r (d.min, d.max, 0.0f, d.skew);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, r, d.def, AudioParameterFloatAttributes().withLabel (d.label)));
    }
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "nr", 1 }, "Noise reduction", nrModes(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "scratches", 1 }, "Scratches", scratchCounts(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "crush_mode", 1 }, "Crush mode", crushModes(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "stop_sync", 1 }, "Stop sync", stopSyncs(), 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "quality", 1 }, "Quality", qualities(), 0));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "stop", 1 }, "Tape stop", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "stop_midi", 1 }, "Stop on MIDI notes", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "stop_transport", 1 }, "Stop with the host", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "wobble", 1 }, "Stereo wobble", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "timeline", 1 }, "Lock to timeline", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "auto_gain", 1 }, "Auto gain", true));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "dither", 1 }, "Dither", false));
    l.add (std::make_unique<AudioParameterBool> (ParameterID { "groove_follow", 1 }, "Groove follows the song", true));
    return l;
}

} // namespace dh::tn::params
