#pragma once
// Parámetros del host del ATMD: una sola tabla para el plugin, la interfaz, los presets y atmdrender.
// Los ids del panel son los de docs/hardware-spec.md §10 y assets/layoutmd.json; las extensiones van aparte.
// Las ruedas (pitch y mod) no son parámetros: son controles de ejecución, como en el teclado del original,
// y los mueven el MIDI (pitch bend, CC 1) o el mouse.

#include <JuceHeader.h>
#include "dsp/EngineMd.h"

namespace dh::md {

struct PDef
{
    juce::String id, name;
    int steps;              // > 0 con choices: elección; > 0 sin choices: entero; 0: continuo
    float lo, hi, def;
    juce::String choices;   // separadas por |
    juce::String unit;
    float skew = 1.0f;
    bool global = false;    // no lo pisan los presets (ajustes del sistema)
    juce::String help;
};

inline const char* kRanges = "LO|32'|16'|8'|4'|2'";
inline const char* kWaves12 = "Triangle|Tri-saw|Sawtooth|Square|Wide pulse|Narrow pulse";
inline const char* kWaves3 = "Triangle|Reverse saw|Sawtooth|Square|Wide pulse|Narrow pulse";
inline const char* kOffOn = "Off|On";
// Divisiones del delay, en negras.
inline const std::array<float, 9> kDelayBeats { 0.25f, 1.0f / 3.0f, 0.5f, 0.75f, 2.0f / 3.0f, 1.0f, 1.5f, 2.0f, 4.0f };
inline const char* kDelayNames = "1/16|1/8 T|1/8|1/8 D|1/4 T|1/4|1/4 D|1/2|1 bar";

inline const std::vector<PDef>& params()
{
    static const std::vector<PDef> all = [] {
        std::vector<PDef> v;
        auto add = [&] (PDef d) { v.push_back (d); };
        auto knob = [&] (const char* id, const char* name, float lo, float hi, float def, const char* help) { add ({ id, name, 0, lo, hi, def, "", "", 1.0f, false, help }); };
        auto sw = [&] (const char* id, const char* name, bool def, const char* help) { add ({ id, name, 2, 0, 1, def ? 1.0f : 0.0f, kOffOn, "", 1.0f, false, help }); };
        // CONTROLLERS
        knob ("tune", "Tune", -2.5f, 2.5f, 0.0f, "master pitch of the three oscillators, about +/-2.5 semitones");
        knob ("glide", "Glide", 0, 10, 0, "glide time when GLIDE is on: about 1 ms to 1 s per octave");
        knob ("mod_mix", "Modulation Mix", 0, 10, 0, "what the MOD wheel sends: oscillator 3 (0) or noise (10)");
        sw ("osc_mod_on", "Oscillator Modulation", false, "the MOD wheel modulates the pitch of oscillators 1 and 2 (and 3 with OSC 3 CONTROL)");
        sw ("osc3_ctrl", "Osc 3 Control", true, "off: oscillator 3 ignores the keyboard and the wheels and becomes a modulator or drone; FREQUENCY then covers +/-3 octaves");
        // OSCILLATOR BANK
        for (int o = 1; o <= 3; ++o)
        {
            const juce::String n = "Osc " + juce::String (o) + " ";
            add ({ "osc" + juce::String (o) + "_range", n + "Range", 6, 0, 5, 3, kRanges, "", 1.0f, false, "octave: LO (a sub-audio range for modulation), 32' to 2'" });
            if (o > 1) add ({ "osc" + juce::String (o) + "_freq", n + "Frequency", 0, -7.5f, 7.5f, 0, "", " st", 1.0f, false,
                              o == 2 ? "tuning of oscillator 2 against oscillator 1, +/-7.5 semitones" : "tuning of oscillator 3, +/-7.5 semitones (+/-3 octaves with OSC 3 CONTROL off)" });
            add ({ "osc" + juce::String (o) + "_wave", n + "Waveform", 6, 0, 5, 2, o == 3 ? kWaves3 : kWaves12, "", 1.0f, false,
                   o == 3 ? "triangle, reverse sawtooth, sawtooth, square, wide and narrow pulse" : "triangle, triangle-sawtooth, sawtooth, square, wide and narrow pulse" });
        }
        // MIXER
        knob ("mix_osc1", "Osc 1 Volume", 0, 10, 7, "oscillator 1 into the mixer; several sources high drive the filter into saturation");
        sw ("mix_osc1_on", "Osc 1 On", true, "oscillator 1 into the mixer");
        knob ("ext_level", "External Input Volume", 0, 10, 0, "the external input (the plugin's side input) into the mixer, through a high-gain preamp: the OVERLOAD lamp lights when it clips");
        sw ("mix_ext_on", "External Input On", false, "external input into the mixer");
        knob ("mix_osc2", "Osc 2 Volume", 0, 10, 0, "oscillator 2 into the mixer");
        sw ("mix_osc2_on", "Osc 2 On", false, "oscillator 2 into the mixer");
        knob ("mix_noise", "Noise Volume", 0, 10, 0, "noise into the mixer");
        sw ("mix_noise_on", "Noise On", false, "noise into the mixer");
        knob ("mix_osc3", "Osc 3 Volume", 0, 10, 0, "oscillator 3 into the mixer (its modulation is not affected)");
        sw ("mix_osc3_on", "Osc 3 On", false, "oscillator 3 into the mixer");
        add ({ "noise_color", "Noise", 2, 0, 1, 0, "White|Pink", "", 1.0f, false, "white or pink noise into the mixer; the modulation noise is pink or red" });
        // MODIFIERS
        sw ("filter_mod_on", "Filter Modulation", false, "the MOD wheel modulates the filter cutoff");
        sw ("kbd_ctrl1", "Keyboard Control 1", false, "the filter follows the keyboard by 1/3 (both switches: fully)");
        sw ("kbd_ctrl2", "Keyboard Control 2", false, "the filter follows the keyboard by 2/3 (both switches: fully)");
        knob ("filter_cutoff", "Cutoff Frequency", -5, 5, 0, "filter cutoff, one octave per division (-1.1 = 440 Hz)");
        knob ("filter_emphasis", "Emphasis", 0, 10, 0, "resonance; the filter sings on its own from about 8");
        knob ("filter_contour", "Amount of Contour", 0, 10, 5, "how far the filter contour opens the filter, up to about 6 octaves");
        knob ("f_attack", "Filter Attack", 0, 10, 0, "filter contour attack, 1 ms to 10 s");
        knob ("f_decay", "Filter Decay", 0, 10, 4, "filter contour decay (and release with DECAY on), 1 ms to 10 s");
        knob ("f_sustain", "Filter Sustain", 0, 10, 5, "filter contour level while the key is held");
        knob ("l_attack", "Loudness Attack", 0, 10, 0, "loudness contour attack, 1 ms to 10 s");
        knob ("l_decay", "Loudness Decay", 0, 10, 4, "loudness contour decay (and release with DECAY on), 1 ms to 10 s");
        knob ("l_sustain", "Loudness Sustain", 0, 10, 8, "loudness while the key is held");
        // OUTPUT
        knob ("volume", "Volume", 0, 10, 6, "main output level");
        sw ("main_on", "Main Output", true, "main output on or off");
        sw ("a440", "A-440", false, "a 440 Hz reference tone for tuning, on top of the output");
        knob ("phones_volume", "Phones Volume", 0, 10, 6, "headphone level of the original; here it sets the level of the A-440 tone");
        // Panel izquierdo
        sw ("glide_on", "Glide", false, "glide between notes, at the GLIDE time");
        sw ("decay_on", "Decay", true, "on: the note dies away with the DECAY times when you release the key; off: it stops in about 15 ms");
        // Extensiones
        auto ext = [&] (PDef d) { add (d); };
        ext ({ "voices", "Voices", 8, 1, 8, 1, "", "", 1.0f, false, "voices: 1 is the monophonic original; more play chords" });
        ext ({ "unison", "Unison", 4, 1, 4, 1, "", "", 1.0f, false, "voices stacked on every note" });
        ext ({ "detune", "Unison Detune", 0, 0, 30, 8, "", " cents", 1.0f, false, "spread of the unison voices" });
        ext ({ "spread", "Stereo Spread", 0, 0, 1, 0.6f, "", "", 1.0f, false, "stereo width of unison and poly voices" });
        ext ({ "analog", "Analog Drift", 0, 0, 1, 0.35f, "", "", 1.0f, false, "slow pitch drift of the oscillators and differences between voices" });
        ext ({ "vel_filter", "Velocity to Filter", 0, 0, 1, 0, "", "", 1.0f, false, "harder playing opens the filter (the original had no velocity)" });
        ext ({ "vel_amp", "Velocity to Loudness", 0, 0, 1, 0, "", "", 1.0f, false, "harder playing is louder" });
        ext ({ "at_filter", "Aftertouch to Filter", 0, 0, 1, 0, "", "", 1.0f, false, "pressure opens the filter" });
        ext ({ "at_mod", "Aftertouch to Modulation", 0, 0, 1, 0, "", "", 1.0f, false, "pressure adds to the MOD wheel" });
        ext ({ "priority", "Note Priority", 3, 0, 2, 0, "Low|Last|High", "", 1.0f, false, "which note sounds when several are held (the original: low)" });
        ext ({ "trigger", "Trigger", 2, 0, 1, 0, "Single|Multi", "", 1.0f, false, "single: legato notes don't restart the contours, as on the original; multi: every note does" });
        ext ({ "bend_range", "Pitch Wheel Range", 24, 1, 24, 7, "", " st", 1.0f, false, "range of the PITCH wheel in semitones (the original: about 7)" });
        ext ({ "dly_on", "Delay", 2, 0, 1, 0, kOffOn, "", 1.0f, false, "tempo-synced stereo delay" });
        ext ({ "dly_time", "Delay Time", 9, 0, 8, 3, kDelayNames, "", 1.0f, false, "delay time in beats of the host tempo" });
        ext ({ "dly_fb", "Delay Feedback", 0, 0, 0.95f, 0.35f, "", "", 1.0f, false, "repeats" });
        ext ({ "dly_tone", "Delay Tone", 0, 0, 1, 0.6f, "", "", 1.0f, false, "darker or brighter repeats" });
        ext ({ "dly_mix", "Delay Mix", 0, 0, 1, 0.25f, "", "", 1.0f, false, "level of the repeats" });
        ext ({ "rev_on", "Reverb", 2, 0, 1, 0, kOffOn, "", 1.0f, false, "plate reverb" });
        ext ({ "rev_size", "Reverb Size", 0, 0, 1, 0.6f, "", "", 1.0f, false, "length of the reverb" });
        ext ({ "rev_damp", "Reverb Damping", 0, 0, 1, 0.4f, "", "", 1.0f, false, "darker reverb tail" });
        ext ({ "rev_mix", "Reverb Mix", 0, 0, 1, 0.2f, "", "", 1.0f, false, "reverb level" });
        ext ({ "oversampling", "Oversampling", 4, 0, 3, 2, "1x|2x|4x|8x", "", 1.0f, true, "internal sample rate of the voice: higher is cleaner with fast modulation and more CPU" });
        ext ({ "output", "Output", 0, -24, 6, 0, "", " dB", 1.0f, true, "output level after the effects" });
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
    if (d.unit == " st") return (v > 0.005f ? "+" : "") + juce::String (v, std::abs (v - std::round (v)) < 0.01f ? 0 : 1) + " st";
    if (d.unit == " dB") return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB";
    if (d.unit == " cents") return juce::String ((int) std::lround (v)) + " cents";
    if (d.steps > 0) return juce::String ((int) std::lround (v)) + d.unit;
    if (d.hi > 1.0f || d.lo < 0.0f) return (v > 0.005f && d.lo < 0.0f ? "+" : "") + juce::String (v, 1);     // perillas del panel: número del dial
    return juce::String ((int) std::lround (v * 100.0f)) + " %";
}

// Índices calculados una vez: el hilo de audio no busca por texto.
struct Ix
{
    int tune, glide, modMix, oscMod, osc3Ctrl, range[3], wave[3], freq2, freq3, level[3], on[3], ext, extOn, noise, noiseOn, noiseColor,
        filterMod, kbd1, kbd2, cutoff, emphasis, contour, fA, fD, fS, lA, lD, lS, volume, mainOn, a440, phones, glideOn, decayOn,
        voices, unison, detune, spread, analog, velFilter, velAmp, atFilter, atMod, priority, trigger, bendRange,
        dlyOn, dlyTime, dlyFb, dlyTone, dlyMix, revOn, revSize, revDamp, revMix, oversampling, output;
    static const Ix& get() { static Ix x; return x; }
private:
    Ix()
    {
        auto f = [] (const juce::String& id) { const int i = paramIndex (id); jassert (i >= 0); return i; };
        tune = f ("tune"); glide = f ("glide"); modMix = f ("mod_mix"); oscMod = f ("osc_mod_on"); osc3Ctrl = f ("osc3_ctrl");
        for (int o = 0; o < 3; ++o)
        {
            const juce::String n = juce::String (o + 1);
            range[o] = f ("osc" + n + "_range"); wave[o] = f ("osc" + n + "_wave");
            level[o] = f ("mix_osc" + n); on[o] = f ("mix_osc" + n + "_on");
        }
        freq2 = f ("osc2_freq"); freq3 = f ("osc3_freq");
        ext = f ("ext_level"); extOn = f ("mix_ext_on"); noise = f ("mix_noise"); noiseOn = f ("mix_noise_on"); noiseColor = f ("noise_color");
        filterMod = f ("filter_mod_on"); kbd1 = f ("kbd_ctrl1"); kbd2 = f ("kbd_ctrl2"); cutoff = f ("filter_cutoff"); emphasis = f ("filter_emphasis");
        contour = f ("filter_contour"); fA = f ("f_attack"); fD = f ("f_decay"); fS = f ("f_sustain"); lA = f ("l_attack"); lD = f ("l_decay"); lS = f ("l_sustain");
        volume = f ("volume"); mainOn = f ("main_on"); a440 = f ("a440"); phones = f ("phones_volume"); glideOn = f ("glide_on"); decayOn = f ("decay_on");
        voices = f ("voices"); unison = f ("unison"); detune = f ("detune"); spread = f ("spread"); analog = f ("analog");
        velFilter = f ("vel_filter"); velAmp = f ("vel_amp"); atFilter = f ("at_filter"); atMod = f ("at_mod");
        priority = f ("priority"); trigger = f ("trigger"); bendRange = f ("bend_range");
        dlyOn = f ("dly_on"); dlyTime = f ("dly_time"); dlyFb = f ("dly_fb"); dlyTone = f ("dly_tone"); dlyMix = f ("dly_mix");
        revOn = f ("rev_on"); revSize = f ("rev_size"); revDamp = f ("rev_damp"); revMix = f ("rev_mix");
        oversampling = f ("oversampling"); output = f ("output");
    }
};

inline int oversamplingFactor (float v) { return 1 << juce::jlimit (0, 3, (int) std::lround (v)); }

// Valores (en unidades reales, en el orden de params()) → motor.
inline void applyParams (const float* v, Engine& e)
{
    const auto& X = Ix::get();
    auto i = [v] (int k) { return (int) std::lround (v[k]); };
    auto b = [v] (int k) { return v[k] > 0.5f; };
    auto& p = e.p;
    p.tune = v[X.tune]; p.glide = v[X.glide]; p.modMix = v[X.modMix]; p.oscMod = b (X.oscMod); p.osc3Ctrl = b (X.osc3Ctrl);
    for (int o = 0; o < 3; ++o) { p.range[o] = i (X.range[o]); p.wave[o] = i (X.wave[o]); p.level[o] = v[X.level[o]]; p.on[o] = b (X.on[o]); }
    p.freq2 = v[X.freq2]; p.freq3 = v[X.freq3];
    p.extLevel = v[X.ext]; p.extOn = b (X.extOn); p.noiseLevel = v[X.noise]; p.noiseOn = b (X.noiseOn); p.noisePink = b (X.noiseColor);
    p.filterMod = b (X.filterMod); p.kbd1 = b (X.kbd1); p.kbd2 = b (X.kbd2);
    p.cutoff = v[X.cutoff]; p.emphasis = v[X.emphasis]; p.contourAmt = v[X.contour];
    p.fA = v[X.fA]; p.fD = v[X.fD]; p.fS = v[X.fS]; p.lA = v[X.lA]; p.lD = v[X.lD]; p.lS = v[X.lS];
    p.volume = v[X.volume]; p.mainOn = b (X.mainOn); p.a440 = b (X.a440); p.a440Level = v[X.phones];
    p.glideOn = b (X.glideOn); p.decayOn = b (X.decayOn);
    auto& s = e.st;
    s.voices = i (X.voices); s.unison = i (X.unison); s.detuneCents = v[X.detune]; s.spread = v[X.spread]; s.analog = v[X.analog];
    s.velFilter = v[X.velFilter]; s.velAmp = v[X.velAmp]; s.atFilter = v[X.atFilter]; s.atMod = v[X.atMod];
    s.priority = i (X.priority); s.multiTrigger = i (X.trigger) == 1; s.bendSemis = v[X.bendRange];
    s.outputGain = juce::Decibels::decibelsToGain (v[X.output]);
    auto& fx = e.fx;
    fx.dlyOn = b (X.dlyOn); fx.dlyBeats = kDelayBeats[(size_t) juce::jlimit (0, 8, i (X.dlyTime))]; fx.dlyFeedback = v[X.dlyFb]; fx.dlyTone = v[X.dlyTone]; fx.dlyMix = v[X.dlyMix];
    fx.revOn = b (X.revOn); fx.revSize = v[X.revSize]; fx.revDamp = v[X.revDamp]; fx.revMix = v[X.revMix];
}

} // namespace dh::md
