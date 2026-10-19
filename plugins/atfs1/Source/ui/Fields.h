#pragma once
// Definición de los campos de la voz para el editor: rango, nombre, formato, ayuda (lo que el
// parámetro hace de verdad en el DSP) y cuándo está activo (sólo lo que la forma o el modo leen).

#include "Widgets.h"

namespace dh::fs::ui {

inline juce::StringArray noteNames (int lo, int hi)
{
    static const char* N[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    juce::StringArray s;
    for (int n = lo; n <= hi; ++n) s.add (juce::String (N[((n % 12) + 12) % 12]) + juce::String (n / 12 - 2));
    return s;
}

inline juce::String ratioText (const VoicedOp& o)
{
    if (o.fixed) { const float hz = fixedHz (o.coarse, o.fine); return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz" : (hz < 10.0f ? juce::String (hz, 2) : juce::String (juce::roundToInt (hz))) + " Hz"; }
    return "x" + juce::String (ratioOf (o.coarse, o.fine), 2);
}

#define OPF(lbl, lo, hi, def, member, helpTxt) \
    VField { lbl, lo, hi, def, [k] (const VoiceData& v) { return (int) v.op[(size_t) k].member; }, [k] (VoiceData& v, int x) { v.op[(size_t) k].member = (decltype (v.op[0].member)) x; }, {}, nullptr, helpTxt, nullptr }
#define NOPF(lbl, lo, hi, def, member, helpTxt) \
    VField { lbl, lo, hi, def, [k] (const VoiceData& v) { return (int) v.nop[(size_t) k].member; }, [k] (VoiceData& v, int x) { v.nop[(size_t) k].member = (decltype (v.nop[0].member)) x; }, {}, nullptr, helpTxt, nullptr }
#define COMF(lbl, lo, hi, def, member, helpTxt) \
    VField { lbl, lo, hi, def, [] (const VoiceData& v) { return (int) v.member; }, [] (VoiceData& v, int x) { v.member = (decltype (v.member)) x; }, {}, nullptr, helpTxt, nullptr }

inline VField withNames (VField f, juce::StringArray n) { f.names = std::move (n); return f; }
inline VField withEnabled (VField f, std::function<bool (const VoiceData&)> e) { f.enabled = std::move (e); return f; }
inline VField withFmt (VField f, std::function<juce::String (int, const VoiceData&)> fm) { f.fmt = std::move (fm); return f; }

// ── operador con voz ──
inline std::vector<VField> voicedOscFields (int k)
{
    auto isFrmt = [k] (const VoiceData& v) { return v.op[(size_t) k].form == Frmt; };
    auto isRes = [k] (const VoiceData& v) { return v.op[(size_t) k].form == Res1 || v.op[(size_t) k].form == Res2; };
    auto notSine = [k] (const VoiceData& v) { return v.op[(size_t) k].form != Sine; };
    return {
        withNames (OPF ("Form", 0, 7, 0, form, "spectral form. sine; all1/all2 a group of harmonics around the frequency; odd1/odd2 odd harmonics only; res1/res2 a 3-line group at harmonic (resonance + 1); frmt a formant: grains at the note pitch with the carrier at the operator frequency"),
                   { "sine", "all1", "all2", "odd1", "odd2", "res1", "res2", "frmt" }),
        withNames (OPF ("Mode", 0, 1, 0, fixed, "ratio follows the key; fixed is an absolute frequency (a formant centre that does not move with the note)"), { "ratio", "fixed" }),
        withFmt (OPF ("Coarse", 0, 31, 1, coarse, "ratio: 0.5, 1, 2 ... 31. Fixed: octaves, 440 Hz x 2^(coarse + fine/128 - 16)"), [k] (int, const VoiceData& v) { return ratioText (v.op[(size_t) k]); }),
        withFmt (OPF ("Fine", 0, 127, 0, fine, "ratio: +1 % per step (0..99). Fixed: 1/128 octave per step"), [k] (int x, const VoiceData& v) { return v.op[(size_t) k].fixed ? juce::String (x) : "+" + juce::String (std::min (x, 99)) + " %"; }),
        withEnabled (OPF ("Detune", -15, 15, 0, detune, "a few cents, smaller in the upper octaves"), [k] (const VoiceData& v) { return ! v.op[(size_t) k].fixed; }),
        withEnabled (OPF ("Transpose", -24, 24, 0, transpose, "shifts the formant's grain pitch (the fundamental) in semitones, frmt only"), isFrmt),
        withEnabled (withNames (OPF ("Key sync", 0, 1, 0, keySync, "restart the oscillator phase at every note (not on frmt)"), { "off", "on" }), [k] (const VoiceData& v) { return v.op[(size_t) k].form != Frmt; }),
        withEnabled (OPF ("Note scale", 0, 99, 0, noteScale, "key tracking of a fixed frequency, around C3 (99 = follows the keyboard)"), [k] (const VoiceData& v) { return v.op[(size_t) k].fixed; }),
        withEnabled (OPF ("Skirt", 0, 7, 0, skirt, "window steepness: opens the harmonic group (all/odd/res) or the formant skirts (frmt). About -1.5 dB per step"), notSine),
        withEnabled (withFmt (OPF ("BW / Reso", 0, 99, 0, bwRes, "frmt: bandwidth, the width in Hz doubles every 8 steps whatever the note. res1/res2: resonance, the group sits on harmonic (value + 1)"),
                              [k] (int x, const VoiceData& v) { return v.op[(size_t) k].form == Frmt ? juce::String (x) + "  " + juce::String (std::lround (3.1f * std::exp2 ((float) x / 8.0f))) + " Hz" : "h" + juce::String (x + 1); }),
                     [isFrmt, isRes] (const VoiceData& v) { return isFrmt (v) || isRes (v); }),
        withEnabled (OPF ("BW bias", -7, 7, 0, bwBias, "controller-driven bandwidth (frmt)"), isFrmt),
        withNames (OPF ("Fseq", 0, 1, 0, fseq, "this operator takes frequency and level from the Formant Sequence track"), { "off", "on" }),
        withEnabled (withFmt (OPF ("Fseq track", 0, 7, 0, fseqTrack, "which of the 8 formant tracks drives this pair of operators"), [] (int x, const VoiceData&) { return "Tr" + juce::String (x + 1); }),
                     [k] (const VoiceData& v) { return v.op[(size_t) k].fseq || v.nop[(size_t) k].fseq; }),
    };
}

inline std::vector<VField> voicedLevelFields (int k)
{
    auto curve = juce::StringArray { "-lin", "-exp", "+exp", "+lin" };
    return {
        OPF ("Level", 0, 99, 0, level, "output level, 0.75 dB per step; on a modulator, the FM depth (99 = 3.37 cycles of phase)"),
        withNames (OPF ("Break point", 0, 99, 39, breakPoint, "level scaling pivot key"), noteNames (21, 120)),
        OPF ("Left depth", 0, 99, 0, leftDepth, "level change below the break point"),
        OPF ("Right depth", 0, 99, 0, rightDepth, "level change above the break point"),
        withNames (OPF ("Left curve", 0, 3, 0, leftCurve, "linear or exponential, down or up"), curve),
        withNames (OPF ("Right curve", 0, 3, 0, rightCurve, "linear or exponential, down or up"), curve),
        OPF ("Amp velocity", -7, 7, 0, ampVel, "velocity to level"),
        OPF ("Amp mod", 0, 7, 0, ampMod, "LFO1 amplitude modulation sensitivity"),
        OPF ("Pitch mod", 0, 7, 0, pitchMod, "LFO1 vibrato sensitivity"),
        withEnabled (OPF ("Freq mod", 0, 7, 0, freqMod, "LFO1 modulation of a fixed frequency (formant wobble)"), [k] (const VoiceData& v) { return v.op[(size_t) k].fixed; }),
        withEnabled (OPF ("Freq velocity", -7, 7, 0, freqVel, "velocity moves a fixed frequency"), [k] (const VoiceData& v) { return v.op[(size_t) k].fixed; }),
        withEnabled (OPF ("Freq bias", -7, 7, 0, freqBias, "controller moves a fixed frequency"), [k] (const VoiceData& v) { return v.op[(size_t) k].fixed; }),
        OPF ("EG bias", -7, 7, 0, egBias, "controller-driven level (Amp EG Bias destination)"),
        VField { "Hold", 0, 99, 0, [k] (const VoiceData& v) { return v.op[(size_t) k].eg.hold; }, [k] (VoiceData& v, int x) { v.op[(size_t) k].eg.hold = x; }, {}, nullptr, "time before the attack starts", nullptr },
        VField { "Time scale", 0, 7, 0, [k] (const VoiceData& v) { return v.op[(size_t) k].eg.timeScale; }, [k] (VoiceData& v, int x) { v.op[(size_t) k].eg.timeScale = x; }, {}, nullptr, "higher notes run the envelope faster", nullptr },
    };
}

// ── operador sin voz ──
inline std::vector<VField> unvoicedFields (int k)
{
    auto normal = [k] (const VoiceData& v) { return v.nop[(size_t) k].mode == 0; };
    return {
        withNames (NOPF ("Mode", 0, 2, 0, mode, "normal: its own frequency. linkFO: follows the note pitch. linkFF: follows the paired voiced formant (frmt)"), { "normal", "linkFO", "linkFF" }),
        withEnabled (withFmt (NOPF ("Coarse", 0, 21, 19, coarse, "centre frequency in octaves: 440 Hz x 2^(coarse + fine/128 - 16)"),
                              [k] (int, const VoiceData& v) { const float hz = fixedHz (v.nop[(size_t) k].coarse, v.nop[(size_t) k].fine); return hz >= 1000.0f ? juce::String (hz / 1000.0f, 2) + " kHz" : juce::String ((int) hz) + " Hz"; }), normal),
        withEnabled (NOPF ("Fine", 0, 127, 0, fine, "1/128 octave per step"), normal),
        NOPF ("Transpose", -24, 24, 0, transpose, "semitones"),
        withEnabled (NOPF ("Note scale", 0, 99, 0, noteScale, "key tracking of the centre, around C3"), normal),
        NOPF ("Bandwidth", 0, 99, 30, bandwidth, "width of the noise band, about 17 Hz per step up to 60; narrow settings ring like a sine"),
        NOPF ("BW bias", -7, 7, 0, bwBias, "controller-driven bandwidth"),
        NOPF ("Resonance", 0, 7, 0, resonance, "0-3 nothing; 4 narrows the band; 5-7 add a pure tone at the centre"),
        NOPF ("Skirt", 0, 7, 0, skirt, "each step widens the band by about 1.35x"),
        NOPF ("Level", 0, 99, 0, level, "output level, 0.75 dB per step"),
        NOPF ("Key scale", -7, 7, 0, levelKeyScale, "level tilt across the keyboard, pivot C3"),
        NOPF ("Amp velocity", -7, 7, 0, ampVel, "velocity to level"),
        NOPF ("Amp mod", 0, 7, 0, ampMod, "LFO1 amplitude modulation sensitivity"),
        withEnabled (NOPF ("Freq mod", 0, 7, 0, freqMod, "LFO1 moves the centre"), normal),
        withEnabled (NOPF ("Freq bias", -7, 7, 0, freqBias, "controller moves the centre"), normal),
        NOPF ("EG bias", -7, 7, 0, egBias, "controller-driven level"),
        withNames (NOPF ("Fseq", 0, 1, 0, fseq, "this operator takes centre and level from the Formant Sequence (the track of the voiced pair)"), { "off", "on" }),
        VField { "Hold", 0, 99, 0, [k] (const VoiceData& v) { return v.nop[(size_t) k].eg.hold; }, [k] (VoiceData& v, int x) { v.nop[(size_t) k].eg.hold = x; }, {}, nullptr, "time before the attack starts", nullptr },
        VField { "Time scale", 0, 7, 0, [k] (const VoiceData& v) { return v.nop[(size_t) k].eg.timeScale; }, [k] (VoiceData& v, int x) { v.nop[(size_t) k].eg.timeScale = x; }, {}, nullptr, "higher notes run the envelope faster", nullptr },
    };
}

// ── común ──
inline const juce::StringArray& lfoWaves() { static juce::StringArray s { "triangle", "saw down", "saw up", "square", "sine", "S&H" }; return s; }
inline const juce::StringArray& categories() { static juce::StringArray s { "--", "Pf", "Cp", "Or", "Gt", "Ba", "St", "En", "Br", "Rd", "Pi", "Ld", "Pd", "Fx", "Et", "Pc", "Se", "Dr", "Sc", "Vo", "Co", "Wv", "Sq" }; return s; }

inline std::vector<VField> pitchEgFields()
{
    return {
        withNames (COMF ("Range", 0, 3, 1, pegRange, "how far +/-50 goes: 8 octaves, 2, 1 or half an octave"), { "8 oct", "2 oct", "1 oct", "1/2 oct" }),
        COMF ("Velocity", 0, 7, 0, pegVel, "softer notes get less pitch envelope"),
        COMF ("Time scale", 0, 7, 0, pegTimeScale, "higher notes run faster"),
    };
}
inline std::vector<VField> lfo1Fields()
{
    return {
        withNames (COMF ("Wave", 0, 5, 0, lfo1Wave, ""), lfoWaves()),
        withFmt (COMF ("Speed", 0, 99, 35, lfo1Speed, "rate"), [] (int x, const VoiceData&) { return juce::String (lfoHz (x), 2) + " Hz"; }),
        COMF ("Delay", 0, 99, 0, lfo1Delay, "fade-in after the note starts"),
        withNames (COMF ("Key sync", 0, 1, 0, lfo1KeySync, "restart at every note"), { "off", "on" }),
        COMF ("Pitch depth", 0, 99, 0, lfo1Pmd, "vibrato depth (each operator scales it with its Pitch mod)"),
        COMF ("Amp depth", 0, 99, 0, lfo1Amd, "tremolo depth (each operator scales it with its Amp mod)"),
        COMF ("Freq depth", 0, 99, 0, lfo1Fmd, "moves fixed frequencies and noise centres (per-operator Freq mod)"),
        COMF ("Filter depth", 0, 99, 0, lfo1FilterDepth, "cutoff modulation"),
    };
}
inline std::vector<VField> lfo2Fields()
{
    return {
        withNames (COMF ("Wave", 0, 5, 0, lfo2Wave, ""), lfoWaves()),
        withFmt (COMF ("Speed", 0, 127, 40, lfo2Speed, "rate"), [] (int x, const VoiceData&) { return juce::String (lfoHz (x, 127), 2) + " Hz"; }),
        withNames (COMF ("Phase", 0, 3, 0, lfo2Phase, "start phase with key sync"), { "0", "90", "180", "270" }),
        withNames (COMF ("Key sync", 0, 1, 0, lfo2KeySync, "restart at every note"), { "off", "on" }),
        COMF ("Filter depth", 0, 99, 0, lfo2FilterDepth, "cutoff modulation"),
    };
}
inline std::vector<VField> filterFields()
{
    return {
        withNames (COMF ("Type", 0, 5, 0, filterType, "LPF 24/18/12 dB, HPF, BPF, BEF"), { "LPF24", "LPF18", "LPF12", "HPF", "BPF", "BEF" }),
        withFmt (COMF ("Cutoff", 0, 127, 127, cutoff, "17.4 Hz x 2^(value/12)"), [] (int x, const VoiceData&) { const float hz = 17.4f * std::exp2 ((float) x / 12.0f); return hz >= 1000.0f ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String ((int) hz) + " Hz"; }),
        COMF ("Resonance", -16, 100, 0, resonance, "negative values are damper than flat"),
        COMF ("Reso velocity", -7, 7, 0, resVel, ""),
        COMF ("Input gain", -12, 12, 0, inputGain, "dB; positive values drive the filter into distortion"),
        COMF ("Key scale", -64, 63, 0, keyScaleDepth, "cutoff tracking (63 = full tracking)"),
        withNames (COMF ("Key point", 0, 127, 60, keyScaleBreak, "pivot of the cutoff tracking"), noteNames (0, 127)),
        COMF ("EG depth", -64, 63, 0, fegDepth, "filter envelope amount"),
        COMF ("EG velocity", -7, 7, 0, fegDepthVel, "velocity scales the envelope depth"),
        COMF ("EG attack vel", 0, 7, 0, fegAttackVel, "harder notes start faster"),
        COMF ("EG time scale", 0, 7, 0, fegTimeScale, "higher notes run faster"),
    };
}
inline std::vector<VField> knobSlotFields (bool fm, int s)
{
    auto slot = [fm, s] (VoiceData& v) -> KnobSlot& { return fm ? v.fmKnob[(size_t) s] : v.formantKnob[(size_t) s]; };
    auto slotc = [fm, s] (const VoiceData& v) -> const KnobSlot& { return fm ? v.fmKnob[(size_t) s] : v.formantKnob[(size_t) s]; };
    auto on = [slotc] (const VoiceData& v) { return slotc (v).dest != 0; };
    return {
        VField { "Dest", 0, 3, 0, [slotc] (const VoiceData& v) { return slotc (v).dest; }, [slot] (VoiceData& v, int x) { slot (v).dest = x; }, { "off", "level", "freq", "width" }, nullptr, "what the knob moves on that operator", nullptr },
        VField { "V/N", 0, 1, 0, [slotc] (const VoiceData& v) { return slotc (v).unvoiced; }, [slot] (VoiceData& v, int x) { slot (v).unvoiced = x; }, { "V", "N" }, nullptr, "voiced or unvoiced operator", on },
        VField { "Op", 0, 7, 0, [slotc] (const VoiceData& v) { return slotc (v).op; }, [slot] (VoiceData& v, int x) { slot (v).op = x; }, { "1", "2", "3", "4", "5", "6", "7", "8" }, nullptr, "operator", on },
        VField { "Depth", -64, 63, 0, [slotc] (const VoiceData& v) { return slotc (v).depth; }, [slot] (VoiceData& v, int x) { slot (v).depth = x; }, {}, nullptr, "+63 reaches 100 % at half the knob's travel, +31 at full travel", on },
    };
}

#undef OPF
#undef NOPF
#undef COMF

} // namespace dh::fs::ui
