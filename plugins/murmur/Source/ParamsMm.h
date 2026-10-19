#pragma once
// Parámetros del host de Murmur: una sola tabla para el plugin, la interfaz, los presets y murmurrender.

#include <JuceHeader.h>
#include "dsp/MurmurEngine.h"

namespace dh::mm {

struct PDef
{
    juce::String id, name;
    int steps;              // > 0 con choices: elección; > 0 sin choices: entero; 0: continuo
    float lo, hi, def;
    juce::String choices;   // separadas por |
    juce::String unit;
    float skew = 1.0f;      // < 1: más resolución abajo (tiempos, frecuencias)
    bool global = false;    // no lo pisan los presets
    juce::String help;
};

inline juce::String quantChoices() { juce::StringArray s; for (int q = 0; q < QuantCount; ++q) s.add (quantName (q)); return s.joinIntoString ("|"); }
inline juce::String targetChoices() { juce::StringArray s; for (int t = 0; t < TargetCount; ++t) s.add (targetName (t)); return s.joinIntoString ("|"); }

inline const std::vector<PDef>& params()
{
    static const std::vector<PDef> all = [] {
        std::vector<PDef> v;
        auto add = [&] (PDef d) { v.push_back (d); };
        // Granos
        add ({ "size", "Size", 0, 5, 2000, 120, "", " ms", 0.3f, false, "length of each grain: short grains buzz and glitch, long ones blur into a cloud" });
        add ({ "size_rand", "Size Random", 0, 0, 1, 0.2f, "", "", 1, false, "each grain gets a random length around Size (up to an octave longer or shorter)" });
        add ({ "density", "Density", 0, 0.5f, 200, 16, "", " /s", 0.3f, false, "grains per second" });
        add ({ "density_rand", "Randomness", 0, 0, 1, 0.5f, "", "", 1, false, "0: grains evenly spaced, like a pulse; 1: random spacing, like rain" });
        add ({ "sync", "Sync", 11, 0, 10, 0, divisionChoices(), "", 1, false, "grains on the host's grid instead of Density (the free clock runs when the host is stopped)" });
        add ({ "chance", "Chance", 0, 0, 1, 1, "", "", 1, false, "with Sync on: the probability that each step plays a grain" });
        add ({ "voices", "Voices", 121, 8, 128, 64, "", "", 1, true, "maximum grains at once (lower it to save CPU)" });
        // Posición
        add ({ "buffer", "Buffer", 0, 0.5f, 16, 8, "", " s", 0.5f, false, "how much recent audio is kept to take grains from" });
        add ({ "freeze", "Freeze", 2, 0, 1, 0, "Off|On", "", 1, false, "stop recording: the grains keep playing what is in the buffer (sustain pedal too)" });
        add ({ "position", "Position", 0, 0, 16, 0.3f, "", " s", 0.5f, false, "how far back in the buffer grains start: 0 is what just came in" });
        add ({ "spray", "Spray", 0, 0, 8, 0.2f, "", " s", 0.5f, false, "random spread around Position: higher scatters grains across time" });
        add ({ "scan", "Scan", 0, -2, 2, 0, "", " x", 1, false, "moves Position on its own: with Freeze, 0.1 stretches the frozen audio ten times, negative plays it backwards" });
        // Altura
        add ({ "pitch", "Pitch", 0, -24, 24, 0, "", " st", 1, false, "transposition of every grain in semitones" });
        add ({ "fine", "Fine", 0, -100, 100, 0, "", " cents", 1, false, "fine tuning" });
        add ({ "pitch_rand", "Pitch Random", 0, 0, 24, 0, "", " st", 1, false, "random transposition per grain, up or down by up to this much" });
        add ({ "quant", "Pitch Scale", (int) QuantCount, 0, (float) QuantCount - 1, 0, quantChoices(), "", 1, false, "snaps Pitch Random to intervals: octaves, fifths, a major or minor chord, a scale" });
        add ({ "shimmer", "Shimmer", 0, 0, 1, 0, "", "", 1, false, "chance that a grain plays an octave higher (with Feedback: the classic rising shimmer)" });
        add ({ "sub", "Sub Octave", 0, 0, 1, 0, "", "", 1, false, "chance that a grain plays an octave lower" });
        add ({ "reverse", "Reverse", 0, 0, 1, 0, "", "", 1, false, "chance that a grain plays backwards" });
        // Envolvente y estéreo
        add ({ "shape", "Shape", 0, 0, 1, 0.6f, "", "", 1, false, "grain envelope: 0 almost square (clicky, rhythmic), 1 a soft bell (smooth clouds)" });
        add ({ "skew", "Skew", 0, -1, 1, 0, "", "", 1, false, "moves the envelope peak: negative for plucks, positive for reversed swells" });
        add ({ "pan", "Pan Spread", 0, 0, 1, 0.5f, "", "", 1, false, "how far grains spread across the stereo field" });
        add ({ "width", "Width", 0, 0, 2, 1, "", "", 1, false, "stereo width of what each grain reads: 0 mono, 1 as recorded, 2 wider" });
        // Tono y niveles
        add ({ "low_cut", "Low Cut", 0, 20, 2000, 20, "", " Hz", 0.3f, false, "removes lows from the grains" });
        add ({ "high_cut", "High Cut", 0, 500, 20000, 20000, "", " Hz", 0.3f, false, "removes highs from the grains" });
        add ({ "feedback", "Feedback", 0, 0, 0.97f, 0, "", "", 1, false, "sends the grains back into the buffer: textures that build on themselves" });
        add ({ "mix", "Mix", 0, 0, 1, 0.5f, "", "", 1, false, "balance between the dry input and the grains" });
        add ({ "in_gain", "Input", 0, -24, 24, 0, "", " dB", 1, false, "input level (both the dry signal and what goes into the buffer)" });
        add ({ "out_gain", "Output", 0, -24, 12, 0, "", " dB", 1, false, "output level" });
        add ({ "duck", "Ducking", 0, 0, 1, 0, "", "", 1, false, "lowers the grains while the input is loud, so the cloud blooms in the gaps" });
        // Modulación
        for (int l = 1; l <= 2; ++l)
        {
            const juce::String p = "lfo" + juce::String (l) + "_", n = "LFO " + juce::String (l) + " ";
            add ({ p + "rate", n + "Rate", 0, 0.01f, 20, l == 1 ? 0.15f : 0.4f, "", " Hz", 0.3f, false, "speed of the modulator" });
            add ({ p + "sync", n + "Sync", 11, 0, 10, 0, divisionChoices(), "", 1, false, "locks the modulator to the host tempo" });
            add ({ p + "shape", n + "Shape", (int) LfoShapeCount, 0, (float) LfoShapeCount - 1, 0, "Sine|Triangle|Ramp|Drift|Step", "", 1, false, "sine, triangle, falling ramp, smooth random drift or random steps" });
            add ({ p + "target", n + "Target", (int) TargetCount, 0, (float) TargetCount - 1, 0, targetChoices(), "", 1, false, "what the modulator moves" });
            add ({ p + "amount", n + "Amount", 0, -1, 1, 0, "", "", 1, false, "how much, and in which direction" });
        }
        // Efectos
        add ({ "delay_sync", "Delay Sync", 11, 0, 10, 0, divisionChoices(), "", 1, false, "delay time in notes of the host tempo (Off: Delay Time in seconds)" });
        add ({ "delay_time", "Delay Time", 0, 0.02f, 2, 0.35f, "", " s", 0.5f, false, "delay time when Delay Sync is off" });
        add ({ "delay_fb", "Delay Feedback", 0, 0, 0.95f, 0.35f, "", "", 1, false, "repeats" });
        add ({ "delay_mix", "Delay Mix", 0, 0, 1, 0, "", "", 1, false, "amount of delay on the grains" });
        add ({ "rev_size", "Reverb Size", 0, 0, 1, 0.6f, "", "", 1, false, "length of the reverb" });
        add ({ "rev_damp", "Reverb Damping", 0, 0, 1, 0.4f, "", "", 1, false, "darker reverb tail" });
        add ({ "rev_mix", "Reverb Mix", 0, 0, 1, 0, "", "", 1, false, "amount of reverb on the grains" });
        // MIDI
        add ({ "midi_mode", "MIDI Mode", 3, 0, 2, 0, "Off|Pitch|Gate", "", 1, false,
               "Pitch: the notes you play set the grains' pitch (chords of grains); Gate: grains only while a note is held" });
        return v;
    }();
    return all;
}

inline int paramIndex (const juce::String& id)
{
    const auto& ps = params();
    for (size_t i = 0; i < ps.size(); ++i) if (ps[i].id == id) return (int) i;
    return -1;
}
inline const PDef& paramDef (const juce::String& id) { return params()[(size_t) std::max (0, paramIndex (id))]; }

inline juce::String formatParam (const PDef& d, float v)
{
    if (d.choices.isNotEmpty()) return juce::StringArray::fromTokens (d.choices, "|", "")[juce::jlimit (0, d.steps - 1, (int) std::lround (v - d.lo))];
    if (d.unit == " ms") return v >= 1000.0f ? juce::String (v / 1000.0f, 2) + " s" : juce::String ((int) std::lround (v)) + " ms";
    if (d.unit == " Hz") return v >= 1000.0f ? juce::String (v / 1000.0f, v >= 9950.0f ? 0 : 1) + " kHz" : juce::String ((int) std::lround (v)) + " Hz";
    if (d.unit == " st") return (v > 0.005f ? "+" : "") + juce::String (v, std::abs (v - std::round (v)) < 0.01f ? 0 : 2) + " st";
    if (d.unit == " /s") return juce::String (v, v < 10.0f ? 1 : 0) + " /s";
    if (d.unit == " x") return (v > 0.0f ? "+" : "") + juce::String (v, 2) + " x";
    if (d.unit == " dB") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
    if (d.unit == " cents") return (v > 0.5f ? "+" : "") + juce::String ((int) std::lround (v)) + " cents";
    if (d.steps > 0) return juce::String ((int) std::lround (v)) + d.unit;
    if (d.unit.isNotEmpty()) return juce::String (v, v < 10.0f ? 2 : 1) + d.unit;
    if (d.lo < 0.0f) return (v > 0.005f ? "+" : "") + juce::String ((int) std::lround (v * 100.0f)) + " %";
    return juce::String ((int) std::lround (v * 100.0f)) + " %";
}

// Índices calculados una vez: el hilo de audio no busca por texto.
struct Ix
{
    int size, sizeRand, density, densityRand, sync, chance, voices, buffer, freeze, position, spray, scan, pitch, fine, pitchRand, quant,
        shimmer, sub, reverse, shape, skew, pan, width, lowCut, highCut, feedback, mix, inGain, outGain, duck,
        delaySync, delayTime, delayFb, delayMix, revSize, revDamp, revMix, midiMode;
    struct L { int rate, sync, shape, target, amount; };
    std::array<L, 2> lfo {};
    static const Ix& get() { static Ix x; return x; }
private:
    Ix()
    {
        auto f = [] (const char* id) { return paramIndex (id); };
        size = f ("size"); sizeRand = f ("size_rand"); density = f ("density"); densityRand = f ("density_rand"); sync = f ("sync"); chance = f ("chance");
        voices = f ("voices"); buffer = f ("buffer"); freeze = f ("freeze"); position = f ("position"); spray = f ("spray"); scan = f ("scan");
        pitch = f ("pitch"); fine = f ("fine"); pitchRand = f ("pitch_rand"); quant = f ("quant"); shimmer = f ("shimmer"); sub = f ("sub");
        reverse = f ("reverse"); shape = f ("shape"); skew = f ("skew"); pan = f ("pan"); width = f ("width"); lowCut = f ("low_cut");
        highCut = f ("high_cut"); feedback = f ("feedback"); mix = f ("mix"); inGain = f ("in_gain"); outGain = f ("out_gain"); duck = f ("duck");
        delaySync = f ("delay_sync"); delayTime = f ("delay_time"); delayFb = f ("delay_fb"); delayMix = f ("delay_mix");
        revSize = f ("rev_size"); revDamp = f ("rev_damp"); revMix = f ("rev_mix"); midiMode = f ("midi_mode");
        for (int l = 0; l < 2; ++l)
        {
            const juce::String p = "lfo" + juce::String (l + 1) + "_";
            lfo[(size_t) l] = { paramIndex (p + "rate"), paramIndex (p + "sync"), paramIndex (p + "shape"), paramIndex (p + "target"), paramIndex (p + "amount") };
        }
    }
};

// Valores (en unidades reales, en el orden de params()) → motor.
inline void applyParams (const float* v, Engine& e)
{
    const auto& X = Ix::get();
    auto i = [v] (int k) { return (int) std::lround (v[k]); };
    auto& p = e.p;
    p.sizeMs = v[X.size]; p.sizeRand = v[X.sizeRand]; p.density = v[X.density]; p.densityRand = v[X.densityRand];
    p.sync = i (X.sync); p.chance = v[X.chance]; p.voices = i (X.voices);
    p.bufferSec = v[X.buffer]; p.freeze = v[X.freeze] > 0.5f; p.position = v[X.position]; p.spray = v[X.spray]; p.scan = v[X.scan];
    p.pitch = v[X.pitch]; p.fine = v[X.fine]; p.pitchRand = v[X.pitchRand]; p.quant = i (X.quant);
    p.shimmer = v[X.shimmer]; p.subOctave = v[X.sub]; p.reverse = v[X.reverse];
    p.shape = v[X.shape]; p.skew = v[X.skew]; p.pan = v[X.pan]; p.width = v[X.width];
    p.lowCut = v[X.lowCut]; p.highCut = v[X.highCut]; p.feedback = v[X.feedback]; p.mix = v[X.mix];
    p.inGainDb = v[X.inGain]; p.outGainDb = v[X.outGain]; p.duck = v[X.duck];
    for (int l = 0; l < 2; ++l)
    {
        const auto& L = X.lfo[(size_t) l];
        p.lfo[(size_t) l] = { v[L.rate], v[L.amount], i (L.sync), i (L.shape), i (L.target) };
    }
    p.delaySync = i (X.delaySync); p.delaySec = v[X.delayTime]; p.delayFb = v[X.delayFb]; p.delayMix = v[X.delayMix];
    p.revSize = v[X.revSize]; p.revDamp = v[X.revDamp]; p.revMix = v[X.revMix];
    p.midiMode = i (X.midiMode);
}

} // namespace dh::mm
