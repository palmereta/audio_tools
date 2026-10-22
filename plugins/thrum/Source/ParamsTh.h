#pragma once
// Parámetros de Thrum, por página: Guitarra (g_), Tocar (p_, h_, c_, s_, f_), MIDI (m_), Pedales (un prefijo por
// pedal), Amplificador (amp_), Gabinete (cab_) y Salida (out_). Los ids no dependen del nombre del producto. El orden de
// la cadena, los patrones editados y la IR del usuario viajan en el estado, no son parámetros.

#include <JuceHeader.h>
#include "Engine.h"

namespace dh::gt::params {

struct Def { const char* id; const char* name; float min, max, def, step; const char* label; const char* page; };

inline const std::vector<Def>& floats()
{
    static const std::vector<Def> d {
        // ── Guitarra ──
        { "g_capo", "Capo", 0, 12, 0, 1, "", "Guitar" },
        { "g_fine", "Fine tune", -50, 50, 0, 0, "ct", "Guitar" },
        { "g_age", "String age", 0, 100, 30, 0, "%", "Guitar" },
        { "g_sustain", "Body sustain", 25, 200, 100, 0, "%", "Guitar" },
        { "g_volume", "Volume", 0, 10, 10, 0, "", "Guitar" },
        { "g_tone", "Tone", 0, 10, 10, 0, "", "Guitar" },
        { "g_cable", "Cable length", 1, 10, 3, 0, "m", "Guitar" },
        { "g_sym", "Sympathetic strings", 0, 100, 50, 0, "%", "Guitar" },
        { "g_buzz", "Fret buzz", 0, 100, 0, 0, "%", "Guitar" },
        { "g_action", "Action", 0, 100, 50, 0, "%", "Guitar" },
        { "g_noise", "Pickup hum", 0, 100, 0, 0, "%", "Guitar" },
        { "g_squeak", "Finger squeak", 0, 100, 50, 0, "%", "Guitar" },
        { "g_release", "Release noise", 0, 100, 40, 0, "%", "Guitar" },
        { "g_attack", "Pick attack noise", 0, 100, 35, 0, "%", "Guitar" },
        { "g_t1", "String 1 tuning", -12, 12, 0, 1, "st", "Guitar" },
        { "g_t2", "String 2 tuning", -12, 12, 0, 1, "st", "Guitar" },
        { "g_t3", "String 3 tuning", -12, 12, 0, 1, "st", "Guitar" },
        { "g_t4", "String 4 tuning", -12, 12, 0, 1, "st", "Guitar" },
        { "g_t5", "String 5 tuning", -12, 12, 0, 1, "st", "Guitar" },
        { "g_t6", "String 6 tuning", -12, 12, 0, 1, "st", "Guitar" },
        { "g_t7", "String 7 tuning", -12, 12, 0, 1, "st", "Guitar" },
        // ── Tocar ──
        { "p_palm", "Palm mute amount", 0, 100, 65, 0, "%", "Play" },
        { "p_legato_frets", "Legato reach", 1, 7, 4, 1, "frets", "Play" },
        { "p_slide_time", "Slide time", 20, 400, 90, 0, "ms", "Play" },
        { "p_pick_pos", "Pick position", 20, 160, 95, 0, "mm", "Play" },
        { "p_pick_hard", "Pick hardness", 0, 100, 60, 0, "%", "Play" },
        { "p_pick_angle", "Pick angle", 0, 100, 35, 0, "%", "Play" },
        { "p_vib_delay", "Vibrato delay", 0, 2000, 450, 0, "ms", "Play" },
        { "p_vib_rate", "Vibrato rate", 3, 8, 5.6f, 0, "Hz", "Play" },
        { "p_vib_depth", "Vibrato depth", 0, 100, 35, 0, "ct", "Play" },
        { "p_bend_range", "Pitch bend range", 1, 12, 2, 1, "st", "Play" },
        { "p_bendin", "Bend-in amount", 1, 4, 2, 1, "st", "Play" },
        { "p_bendin_time", "Bend-in time", 40, 600, 180, 0, "ms", "Play" },
        { "p_feedback", "Feedback", 0, 100, 0, 0, "%", "Play" },
        { "p_whammy", "Whammy range", 1, 24, 12, 1, "st", "Play" },
        { "p_dead_below", "Dead notes below velocity", 0, 60, 0, 1, "", "Play" },
        { "p_pinch_above", "Pinch above velocity", 0, 127, 0, 1, "", "Play" },
        { "p_staccato", "Staccato length", 40, 300, 110, 0, "ms", "Play" },
        { "p_idle", "Idle hand mute", 0.3f, 6, 1.2f, 0, "s", "Play" },
        { "h_tune", "Human tuning", 0, 100, 30, 0, "%", "Play" },
        { "h_time", "Human timing", 0, 100, 30, 0, "%", "Play" },
        { "h_vel", "Human velocity", 0, 100, 30, 0, "%", "Play" },
        { "h_pick", "Human pick", 0, 100, 50, 0, "%", "Play" },
        { "c_region", "Chord region", 0, 12, 0, 1, "fret", "Play" },
        { "c_strum", "Strum speed", 5, 120, 35, 0, "ms", "Play" },
        { "c_up", "Upstroke strings", 2, 7, 4, 1, "", "Play" },
        { "c_window", "Chord window", 0, 80, 25, 0, "ms", "Play" },
        { "s_tempo", "Tempo (host stopped)", 40, 240, 110, 0, "BPM", "Play" },
        { "f_position", "Hand position", -1, 19, -1, 1, "fret", "Play" },
        { "f_string", "Preferred string", 0, 7, 0, 1, "", "Play" },
        { "f_open", "Open strings", 0, 100, 50, 0, "%", "Play" },
        { "f_low", "Low positions", 0, 100, 50, 0, "%", "Play" },
        // ── MIDI ──
        { "m_str_bend", "Per-string bend range", 1, 48, 12, 1, "st", "Settings" },
        { "m_mpe_bend", "MPE bend range", 1, 96, 48, 1, "st", "Settings" },
        { "m_ks", "Keyswitch base note", 0, 24, 12, 1, "", "Settings" },
        { "m_out_ch", "MIDI out channel", 1, 16, 1, 1, "", "Settings" },
        { "m_seed", "Variation seed", 1, 999, 1, 1, "", "Settings" },
        // ── Pedales ──
        { "gate_thr", "Gate threshold", 0, 100, 30, 0, "%", "Pedals" },
        { "gate_rel", "Gate release", 0, 100, 30, 0, "%", "Pedals" },
        { "comp_sus", "Comp sustain", 0, 100, 50, 0, "%", "Pedals" },
        { "comp_att", "Comp attack", 0, 100, 30, 0, "%", "Pedals" },
        { "comp_lvl", "Comp level", 0, 100, 50, 0, "%", "Pedals" },
        { "comp_mix", "Comp blend", 0, 100, 100, 0, "%", "Pedals" },
        { "wah_pos", "Wah position", 0, 100, 50, 0, "%", "Pedals" },
        { "wah_sens", "Wah sensitivity", 0, 100, 50, 0, "%", "Pedals" },
        { "wah_speed", "Wah speed", 0, 100, 50, 0, "%", "Pedals" },
        { "wah_range", "Wah range", 0, 100, 50, 0, "%", "Pedals" },
        { "wah_q", "Wah resonance", 0, 100, 50, 0, "%", "Pedals" },
        { "od_drive", "Overdrive drive", 0, 100, 45, 0, "%", "Pedals" },
        { "od_tone", "Overdrive tone", 0, 100, 50, 0, "%", "Pedals" },
        { "od_lvl", "Overdrive level", 0, 100, 60, 0, "%", "Pedals" },
        { "dist_drive", "Distortion amount", 0, 100, 50, 0, "%", "Pedals" },
        { "dist_filter", "Distortion filter", 0, 100, 40, 0, "%", "Pedals" },
        { "dist_lvl", "Distortion level", 0, 100, 60, 0, "%", "Pedals" },
        { "fuzz_fuzz", "Fuzz amount", 0, 100, 70, 0, "%", "Pedals" },
        { "fuzz_bias", "Fuzz bias", 0, 100, 70, 0, "%", "Pedals" },
        { "fuzz_tone", "Fuzz tone", 0, 100, 50, 0, "%", "Pedals" },
        { "fuzz_lvl", "Fuzz level", 0, 100, 50, 0, "%", "Pedals" },
        { "cho_rate", "Chorus rate", 0, 100, 30, 0, "%", "Pedals" },
        { "cho_depth", "Chorus depth", 0, 100, 50, 0, "%", "Pedals" },
        { "cho_mix", "Chorus mix", 0, 100, 50, 0, "%", "Pedals" },
        { "cho_tone", "Chorus tone", 0, 100, 60, 0, "%", "Pedals" },
        { "pha_rate", "Phaser rate", 0, 100, 30, 0, "%", "Pedals" },
        { "pha_depth", "Phaser depth", 0, 100, 60, 0, "%", "Pedals" },
        { "pha_fb", "Phaser feedback", 0, 100, 40, 0, "%", "Pedals" },
        { "pha_mix", "Phaser mix", 0, 100, 100, 0, "%", "Pedals" },
        { "fla_rate", "Flanger rate", 0, 100, 25, 0, "%", "Pedals" },
        { "fla_depth", "Flanger depth", 0, 100, 60, 0, "%", "Pedals" },
        { "fla_fb", "Flanger feedback", 0, 100, 65, 0, "%", "Pedals" },
        { "fla_mix", "Flanger mix", 0, 100, 70, 0, "%", "Pedals" },
        { "fla_manual", "Flanger manual", 0, 100, 30, 0, "%", "Pedals" },
        { "trem_rate", "Tremolo rate", 0, 100, 40, 0, "%", "Pedals" },
        { "trem_depth", "Tremolo depth", 0, 100, 50, 0, "%", "Pedals" },
        { "vib_rate", "Vibrato pedal rate", 0, 100, 40, 0, "%", "Pedals" },
        { "vib_depth", "Vibrato pedal depth", 0, 100, 40, 0, "%", "Pedals" },
        { "dly_time", "Delay time", 0, 100, 45, 0, "%", "Pedals" },
        { "dly_fb", "Delay feedback", 0, 100, 35, 0, "%", "Pedals" },
        { "dly_mix", "Delay mix", 0, 100, 30, 0, "%", "Pedals" },
        { "dly_tone", "Delay tone", 0, 100, 50, 0, "%", "Pedals" },
        { "dly_mod", "Delay modulation", 0, 100, 20, 0, "%", "Pedals" },
        { "dly_div", "Delay division", 0, 100, 64, 0, "%", "Pedals" },
        { "rev_decay", "Reverb decay", 0, 100, 50, 0, "%", "Pedals" },
        { "rev_tone", "Reverb tone", 0, 100, 50, 0, "%", "Pedals" },
        { "rev_mix", "Reverb mix", 0, 100, 25, 0, "%", "Pedals" },
        { "rev_pre", "Reverb pre-delay", 0, 100, 10, 0, "%", "Pedals" },
        { "rev_size", "Reverb size / drip", 0, 100, 50, 0, "%", "Pedals" },
        // ── Amplificador ──
        { "amp_gain", "Gain", 0, 10, 4, 0, "", "Amp" },
        { "amp_bass", "Bass", 0, 10, 5, 0, "", "Amp" },
        { "amp_mid", "Middle", 0, 10, 5, 0, "", "Amp" },
        { "amp_treble", "Treble", 0, 10, 5, 0, "", "Amp" },
        { "amp_presence", "Presence", 0, 10, 4, 0, "", "Amp" },
        { "amp_depth", "Depth", 0, 10, 3, 0, "", "Amp" },
        { "amp_master", "Master", 0, 10, 5, 0, "", "Amp" },
        { "amp_sag", "Sag", 0, 10, 5, 0, "", "Amp" },
        { "amp_level", "Amp level", -24, 24, 0, 0, "dB", "Amp" },
        // ── Gabinete ──
        { "cab_pos_a", "Mic A position", 0, 100, 30, 0, "%", "Amp" },
        { "cab_dist_a", "Mic A distance", 1, 60, 3, 0, "cm", "Amp" },
        { "cab_pos_b", "Mic B position", 0, 100, 50, 0, "%", "Amp" },
        { "cab_dist_b", "Mic B distance", 1, 60, 25, 0, "cm", "Amp" },
        { "cab_mix", "Mic mix A/B", 0, 100, 0, 0, "%", "Amp" },
        { "cab_room", "Room reflection", 0, 100, 15, 0, "%", "Amp" },
        // ── Salida ──
        { "out_level", "Output", -36, 12, 0, 0, "dB", "Settings" },
        { "out_di", "DI level", -36, 12, 0, 0, "dB", "Settings" },
    };
    return d;
}

struct ChoiceDef { const char* id; const char* name; juce::StringArray items; int def; const char* page; };
inline const std::vector<ChoiceDef>& choices()
{
    static const std::vector<ChoiceDef> c = [] {
        std::vector<ChoiceDef> v;
        juce::StringArray types; for (int i = 0; i < 6; ++i) types.add (guitar (i).name);
        juce::StringArray tunings; for (int i = 0; i < kNumTunings; ++i) tunings.add (tuningName (i));
        juce::StringArray gauges; for (int i = 0; i < kNumGauges; ++i) gauges.add (gaugeName (i));
        juce::StringArray modes; for (int i = 0; i < kNumModes; ++i) modes.add (modeName (i));
        juce::StringArray arts; for (int i = 0; i < kNumArts; ++i) arts.add (artName (i));
        juce::StringArray styles; for (int i = 0; i < kNumStyles; ++i) styles.add (styleName (i));
        juce::StringArray divs; for (int i = 0; i < kNumDivs; ++i) divs.add (divisionName (i));
        juce::StringArray amps; for (int i = 0; i < kNumAmps; ++i) amps.add (ampName (i));
        juce::StringArray spk; for (int i = 0; i < kNumSpeakers; ++i) spk.add (speakerName (i));
        juce::StringArray cabs; for (int i = 0; i < kNumCabs; ++i) cabs.add (cabName (i));
        juce::StringArray mics; for (int i = 0; i < kNumMics; ++i) mics.add (micName (i));
        juce::StringArray irs; for (const auto& f : factoryIrs()) irs.add (f.name);
        v.push_back ({ "g_type", "Guitar", types, 0, "Guitar" });
        v.push_back ({ "g_tuning", "Tuning", tunings, 0, "Guitar" });
        v.push_back ({ "g_gauge", "String gauge", gauges, 1, "Guitar" });
        v.push_back ({ "g_selector", "Pickup selector", { "Bridge", "Bridge + middle", "Middle (both)", "Middle + neck", "Neck" }, 0, "Guitar" });
        v.push_back ({ "g_mains", "Mains hum", { "50 Hz", "60 Hz" }, 0, "Guitar" });
        v.push_back ({ "p_mode", "Play mode", modes, 0, "Play" });
        v.push_back ({ "p_art", "Articulation", arts, 0, "Play" });
        v.push_back ({ "p_legato", "Legato", { "Off", "Hammer-on / pull-off", "Slides", "Auto" }, 3, "Play" });
        v.push_back ({ "p_trem_div", "Tremolo picking", divs, 1, "Play" });
        v.push_back ({ "p_fb_harm", "Feedback pitch", { "Fundamental", "Octave", "Octave + fifth", "Two octaves" }, 1, "Play" });
        v.push_back ({ "c_style", "Voicing", styles, 0, "Play" });
        v.push_back ({ "c_single", "Single key plays", { "Major chord", "Power chord", "The note" }, 0, "Play" });
        v.push_back ({ "c_anchor", "Strum timing", { "Centred on the beat", "Starts on the beat" }, 0, "Play" });
        v.push_back ({ "m_in", "MIDI input", { "Normal", "Guitar (channel per string)", "MPE" }, 0, "Settings" });
        v.push_back ({ "wah_mode", "Wah mode", { "Manual", "Auto", "MIDI CC 11" }, 0, "Pedals" });
        v.push_back ({ "fuzz_type", "Fuzz transistor", { "Silicon", "Germanium" }, 0, "Pedals" });
        v.push_back ({ "trem_shape", "Tremolo shape", { "Sine", "Optical", "Harmonic" }, 0, "Pedals" });
        v.push_back ({ "dly_type", "Delay type", { "Digital", "Analog", "Tape" }, 0, "Pedals" });
        v.push_back ({ "rev_type", "Reverb type", { "Spring", "Room", "Hall" }, 0, "Pedals" });
        v.push_back ({ "amp_model", "Amp", amps, 0, "Amp" });
        v.push_back ({ "amp_quality", "Amp quality", { "2x", "4x", "8x" }, 1, "Amp" });
        v.push_back ({ "cab_source", "Cabinet source", { "Model", "Factory IR", "User IR" }, 0, "Amp" });
        v.push_back ({ "cab_ir", "Factory IR", irs, 0, "Amp" });
        v.push_back ({ "cab_speaker", "Speaker", spk, 1, "Amp" });
        v.push_back ({ "cab_box", "Cabinet", cabs, 2, "Amp" });
        v.push_back ({ "cab_mic_a", "Mic A", mics, 0, "Amp" });
        v.push_back ({ "cab_mic_b", "Mic B", mics, 2, "Amp" });
        return v;
    }();
    return c;
}

struct BoolDef { const char* id; const char* name; bool def; const char* page; };
inline const std::vector<BoolDef>& bools()
{
    static const std::vector<BoolDef> b {
        { "g_mute_open", "Mute unplayed strings", false, "Guitar" },
        { "p_fingers", "Fingers instead of pick", false, "Play" },
        { "p_vib_auto", "Auto vibrato", true, "Play" },
        { "p_bend_all", "Pitch bend bends every string", false, "Play" },
        { "c_latch", "Latch chord", false, "Play" },
        { "c_letring", "Let open strings ring", false, "Play" },
        { "m_out", "MIDI out", false, "Settings" },
        { "m_out_str", "MIDI out channel per string", false, "Settings" },
        { "gate_on", "Noise gate", false, "Pedals" }, { "comp_on", "Compressor", false, "Pedals" }, { "wah_on", "Wah", false, "Pedals" },
        { "od_on", "Green overdrive", false, "Pedals" }, { "dist_on", "Distortion", false, "Pedals" }, { "fuzz_on", "Fuzz", false, "Pedals" },
        { "cho_on", "Chorus", false, "Pedals" }, { "pha_on", "Phaser", false, "Pedals" }, { "fla_on", "Flanger", false, "Pedals" },
        { "trem_on", "Tremolo", false, "Pedals" }, { "vib_on", "Vibrato pedal", false, "Pedals" }, { "dly_on", "Delay", false, "Pedals" },
        { "rev_on", "Reverb", false, "Pedals" },
        { "trem_sync", "Tremolo sync", false, "Pedals" }, { "dly_sync", "Delay sync", true, "Pedals" }, { "rev_freeze", "Reverb freeze", false, "Pedals" },
        { "amp_on", "Amp", true, "Amp" }, { "amp_bright", "Bright", false, "Amp" }, { "amp_auto", "Auto level", true, "Amp" }, { "cab_on", "Cabinet", true, "Amp" },
        { "out_di_main", "Main output is DI", false, "Settings" },
    };
    return b;
}

// strumNames / arpNames: la biblioteca de patrones (la elección del patrón es un parámetro: se puede automatizar).
inline juce::AudioProcessorValueTreeState::ParameterLayout layout (const juce::StringArray& strumNames, const juce::StringArray& arpNames)
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "s_pattern", 1 }, "Strum pattern", strumNames.isEmpty() ? StringArray { "Default" } : strumNames, 0));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "a_pattern", 1 }, "Picking pattern", arpNames.isEmpty() ? StringArray { "Default" } : arpNames, 0));
    for (auto& d : floats())
    {
        NormalisableRange<float> r (d.min, d.max, d.step, 1.0f);
        // Texto: enteros con paso 1; si no, un decimal (dos en los rangos chicos). juce::String (x, 0) imprimiría todos.
        const int decimals = d.step >= 1.0f ? 0 : (d.max - d.min <= 2.0f ? 2 : 1);
        auto attrs = AudioParameterFloatAttributes().withLabel (d.label)
                         .withStringFromValueFunction ([decimals] (float v, int) { return decimals == 0 ? String (juce::roundToInt (v)) : String (v, decimals); });
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { d.id, 1 }, d.name, r, d.def, attrs));
    }
    for (auto& c : choices()) l.add (std::make_unique<AudioParameterChoice> (ParameterID { c.id, 1 }, c.name, c.items, c.def));
    for (auto& b : bools()) l.add (std::make_unique<AudioParameterBool> (ParameterID { b.id, 1 }, b.name, b.def));
    return l;
}

// Los ids de cada página, en el orden en que se muestran.
inline juce::StringArray idsOfPage (const char* page)
{
    juce::StringArray a;
    if (juce::String (page) == "Play") { a.add ("s_pattern"); a.add ("a_pattern"); }
    for (auto& c : choices()) if (juce::String (c.page) == page) a.add (c.id);
    for (auto& d : floats()) if (juce::String (d.page) == page) a.add (d.id);
    for (auto& b : bools()) if (juce::String (b.page) == page) a.add (b.id);
    return a;
}

// Los ids de cada pedal (el interruptor primero).
inline juce::StringArray pedalIds (int u)
{
    static const char* pre[] = { "gate_", "comp_", "wah_", "od_", "dist_", "fuzz_", "cho_", "pha_", "fla_", "trem_", "vib_", "dly_", "rev_" };
    juce::StringArray a;
    const juce::String p (pre[std::clamp (u, 0, (int) kNumPedals - 1)]);
    a.add (p + "on");
    for (auto& c : choices()) if (juce::String (c.id).startsWith (p)) a.add (c.id);
    for (auto& d : floats()) if (juce::String (d.id).startsWith (p)) a.add (d.id);
    for (auto& b : bools()) if (juce::String (b.id).startsWith (p) && ! juce::String (b.id).endsWith ("_on")) a.add (b.id);
    return a;
}

} // namespace dh::gt::params
