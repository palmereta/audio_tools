#pragma once
// Parámetros del host: los de la parte del FS1R (los desvíos que mueven las perillas del frente),
// el Fseq, los efectos y el sistema. La voz completa (8+8 operadores) vive en el estado del plugin y se
// edita en las páginas del editor. Una sola tabla para el plugin, la interfaz, los presets y atfs1render.

#include <JuceHeader.h>
#include "dsp/FsEngine.h"
#include "dsp/FsEffects.h"

namespace dh::fs {

struct HostParam
{
    const char* id;
    const char* name;
    int steps;              // > 0: elección (o entero si choices == nullptr)
    float lo, hi, def;
    const char* choices;
    const char* unit = "";
    bool global = false;    // no lo pisan los presets
};

inline const char* ctlDestNames() { return "Off|LFO1 Pitch Mod|LFO1 Amp Mod|LFO1 Freq Mod|LFO1 Filter Mod|Cutoff|Formant|FM|V/N Balance|Fseq Speed|Formant Scratch|Freq Bias|Amp EG Bias|Voiced Bandwidth"; }

inline const std::vector<HostParam>& hostParams()
{
    static const std::vector<HostParam> p = {
        // Frente
        { "volume",       "Volume",              0, 0, 1, 0.75f, nullptr },
        { "attack",       "Attack",              128, -64, 63, 0, nullptr },
        { "release",      "Release",             128, -64, 63, 0, nullptr },
        { "formant",      "Formant",             128, -64, 63, 0, nullptr },
        { "fm",           "FM",                  128, -64, 63, 0, nullptr },
        // Parte
        { "vn_balance",   "V/N Balance",         128, -64, 63, 0, nullptr },
        { "decay",        "Decay",               128, -64, 63, 0, nullptr },
        { "cutoff",       "Cutoff",              128, -64, 63, 0, nullptr },
        { "resonance",    "Resonance",           128, -64, 63, 0, nullptr },
        { "feg_depth",    "Filter EG Depth",     128, -64, 63, 0, nullptr },
        { "lfo1_rate",    "LFO1 Rate",           128, -64, 63, 0, nullptr },
        { "lfo1_pmd",     "LFO1 Pitch Depth",    128, -64, 63, 0, nullptr },
        { "lfo1_delay",   "LFO1 Delay",          128, -64, 63, 0, nullptr },
        { "lfo2_rate",    "LFO2 Rate",           128, -64, 63, 0, nullptr },
        { "lfo2_depth",   "LFO2 Depth",          128, -64, 63, 0, nullptr },
        { "peg_init",     "PEG Initial Level",   128, -64, 63, 0, nullptr },
        { "peg_attack",   "PEG Attack Time",     128, -64, 63, 0, nullptr },
        { "peg_rel_level","PEG Release Level",   128, -64, 63, 0, nullptr },
        { "peg_rel_time", "PEG Release Time",    128, -64, 63, 0, nullptr },
        { "pan",          "Pan",                 0, -1, 1, 0, nullptr },
        { "note_shift",   "Note Shift",          49, -24, 24, 0, nullptr, " st" },
        { "detune",       "Detune",              128, -64, 63, 0, nullptr },
        { "filter",       "Filter",              2, 0, 1, 1, "Off|On" },
        { "mono",         "Mono/Poly",           2, 0, 1, 0, "Poly|Mono" },
        { "voices",       "Voices",              32, 1, 32, 16, nullptr },
        { "porta",        "Portamento",          2, 0, 1, 0, "Off|On" },
        { "porta_mode",   "Portamento Mode",     2, 0, 1, 0, "Fingered|Fulltime" },
        { "porta_time",   "Portamento Time",     128, 0, 127, 40, nullptr },
        { "bend_up",      "Bend Up",             25, 0, 24, 2, nullptr, " st" },
        { "bend_down",    "Bend Down",           25, 0, 24, 2, nullptr, " st" },
        { "vel_depth",    "Velocity Depth",      128, 0, 127, 64, nullptr },
        { "vel_offset",   "Velocity Offset",     128, 0, 127, 64, nullptr },
        { "mw_dest",      "Mod Wheel Dest",      CdCount, 0, CdCount - 1, CdLfo1Pm, ctlDestNames() },
        { "mw_depth",     "Mod Wheel Depth",     127, -63, 63, 40, nullptr },
        { "at_dest",      "Aftertouch Dest",     CdCount, 0, CdCount - 1, CdFormant, ctlDestNames() },
        { "at_depth",     "Aftertouch Depth",    127, -63, 63, 0, nullptr },
        // Fseq
        { "fseq",         "Fseq",                2, 0, 1, 0, "Off|On" },
        { "fseq_speed",   "Fseq Speed",          0, 10, 1000, 100, nullptr, " %" },
        { "fseq_smooth",  "Fseq Interpolation",  2, 0, 1, 0, "Off (original)|On" },
        { "fseq_pitch",   "Fseq Pitch Mode",     2, 0, 1, 0, "Fseq pitch|Key pitch" },
        { "fseq_loop",    "Fseq Loop",           3, 0, 2, 0, "From Fseq|One-way|Round" },
        { "fseq_start",   "Fseq Start",          512, 0, 511, 0, nullptr },
        { "fseq_lp_start","Fseq Loop Start",     512, 0, 511, 0, nullptr },
        { "fseq_lp_end",  "Fseq Loop End",       512, 0, 511, 0, nullptr },
        { "fseq_vel_speed","Fseq Speed Velocity",8, 0, 7, 0, nullptr },
        { "fseq_vel_level","Fseq Level Velocity",128, -64, 63, 0, nullptr },
        { "fseq_delay",   "Fseq Delay",          100, 0, 99, 0, nullptr },
        // Efectos
        { "ins_type",     "Insertion",           InsCount, 0, InsCount - 1, 0, "Thru|Distortion|Overdrive|Amp Sim|Auto Wah|Phaser|Rotary Sp|Lo-Fi|Compressor" },
        { "ins_amount",   "Insertion Amount",    0, 0, 1, 0.5f, nullptr },
        { "ins_mix",      "Insertion Mix",       0, 0, 1, 1.0f, nullptr },
        { "var_type",     "Variation",           VarCount, 0, VarCount - 1, VarChorus, "No Effect|Chorus|Celeste|Flanger|Symphonic|Ens Detune|Tremolo|Auto Pan|Delay L,R|Echo|Cross Delay" },
        { "var_rate",     "Variation Rate",      0, 0, 1, 0.3f, nullptr },
        { "var_depth",    "Variation Depth",     0, 0, 1, 0.5f, nullptr },
        { "var_send",     "Variation Send",      0, 0, 1, 0.2f, nullptr },
        { "var_to_rev",   "Variation to Reverb", 0, 0, 1, 0.2f, nullptr },
        { "rev_type",     "Reverb",              RevCount, 0, RevCount - 1, RevHall1, "No Effect|Hall1|Hall2|Room1|Room2|Stage|Plate|Canyon" },
        { "rev_time",     "Reverb Time",         0, 0, 1, 0.5f, nullptr },
        { "rev_send",     "Reverb Send",         0, 0, 1, 0.25f, nullptr },
        { "eq_low",       "EQ Low",              0, -12, 12, 0, nullptr, " dB" },
        { "eq_high",      "EQ High",             0, -12, 12, 0, nullptr, " dB" },
        // Sistema
        { "authentic",    "Authentic Polyphony", 2, 0, 1, 0, "Off|On (filter halves voices)", "", true },
        { "output",       "Output",              0, -24, 6, 0, nullptr, " dB", true },
    };
    return p;
}

// Un parámetro entero (perillas -64..+63, Voices, Note Shift…) para el motor y la pantalla, que el host ve continuo
// y que guarda el valor tal como llegó. Con AudioParameterInt el host escribía 0.6739 en "Cutoff", el plugin lo
// guardaba redondeado a +22 y al reabrir la sesión el host leía 0.6772 (QA en REAPER 2026-10-22, D2). El motor
// redondea al leerlo (applyHost) y la interfaz lo mueve de a enteros (snapReal / snapNorm).
class SteppedParam : public juce::AudioParameterFloat
{
public:
    SteppedParam (const juce::ParameterID& pid, const juce::String& name, float lo, float hi, float def, int numSteps,
                  const juce::AudioParameterFloatAttributes& attributes)
        : juce::AudioParameterFloat (pid, name, juce::NormalisableRange<float> (lo, hi), def, attributes), steps (numSteps) {}
    // Para la interfaz (pasos de la rueda, arrastre); los wrappers sólo lo leen en parámetros discretos, y este no lo es.
    int getNumSteps() const override { return steps; }
private:
    int steps;
};

// Valor real → el entero más cercano si el parámetro es entero (lo que escribe la interfaz).
inline float snapReal (const juce::RangedAudioParameter& p, float v)
{
    return dynamic_cast<const SteppedParam*> (&p) != nullptr ? std::round (v) : v;
}
inline float snapNorm (const juce::RangedAudioParameter& p, float norm)
{
    return dynamic_cast<const SteppedParam*> (&p) != nullptr ? p.convertTo0to1 (std::round (p.convertFrom0to1 (norm))) : norm;
}

inline int hostIndex (const juce::String& id)
{
    const auto& ps = hostParams();
    for (size_t i = 0; i < ps.size(); ++i) if (id == ps[i].id) return (int) i;
    return -1;
}

inline juce::StringArray hostChoices (const HostParam& e)
{
    return e.choices != nullptr ? juce::StringArray::fromTokens (e.choices, "|", "") : juce::StringArray();
}

inline juce::String formatHost (const HostParam& e, float v)
{
    if (e.choices != nullptr) return hostChoices (e)[juce::jlimit (0, e.steps - 1, (int) std::lround (v))];
    const juce::String id (e.id);
    if (id == "pan") { const int p = (int) std::lround (v * 63.0f); return p == 0 ? juce::String ("C") : (p < 0 ? "L" + juce::String (-p) : "R" + juce::String (p)); }
    if (id == "output" || id.startsWith ("eq_")) return (v >= 0 ? "+" : "") + juce::String (v, 1) + e.unit;
    if (id == "fseq_speed") return juce::String (v, 0) + " %";
    if (e.steps > 0)
    {
        const int iv = (int) std::lround (v);
        return (e.lo < 0 && iv > 0 ? "+" : "") + juce::String (iv) + e.unit;
    }
    return juce::String ((int) std::lround (v * 127.0f));
}

// Lee los parámetros (get (id) → valor real) y los vuelca en la parte y los efectos.
template <typename Get>
inline void applyHost (Get&& get, PartParams& pp, FxParams& fx, float& outputGain)
{
    auto i = [&] (const char* id) { return (int) std::lround (get (id)); };
    pp.volume = get ("volume"); pp.pan = get ("pan");
    pp.attack = i ("attack"); pp.release = i ("release"); pp.formant = i ("formant"); pp.fm = i ("fm");
    pp.vnBalance = i ("vn_balance"); pp.decay = i ("decay"); pp.cutoff = i ("cutoff"); pp.resonance = i ("resonance");
    pp.fegDepth = i ("feg_depth");
    pp.lfo1Rate = i ("lfo1_rate"); pp.lfo1Pmd = i ("lfo1_pmd"); pp.lfo1Delay = i ("lfo1_delay");
    pp.lfo2Rate = i ("lfo2_rate"); pp.lfo2Depth = i ("lfo2_depth");
    pp.pegInit = i ("peg_init"); pp.pegAttack = i ("peg_attack"); pp.pegRelLevel = i ("peg_rel_level"); pp.pegRelTime = i ("peg_rel_time");
    pp.noteShift = i ("note_shift"); pp.detune = i ("detune");
    pp.filterOn = i ("filter") != 0; pp.mono = i ("mono") != 0; pp.maxVoices = juce::jlimit (1, kMaxVoices, i ("voices"));
    pp.porta = i ("porta") != 0; pp.portaFulltime = i ("porta_mode") != 0; pp.portaTime = i ("porta_time");
    pp.bendUp = i ("bend_up"); pp.bendDown = i ("bend_down");
    pp.velDepth = i ("vel_depth"); pp.velOffset = i ("vel_offset");
    pp.mwDest = i ("mw_dest"); pp.mwDepth = i ("mw_depth"); pp.atDest = i ("at_dest"); pp.atDepth = i ("at_depth");
    pp.fseqOn = i ("fseq") != 0; pp.fseqSpeed = get ("fseq_speed") / 100.0f; pp.fseqSmooth = i ("fseq_smooth") != 0;
    pp.fseqPitchMode = i ("fseq_pitch");
    const int loop = i ("fseq_loop");
    pp.fseqLoopMode = loop == 2 ? 1 : 0;
    // Con One-way o Round elegidos pero sin puntos propios (0/0, los de fábrica) se usan los del Fseq: antes el
    // lazo quedaba en los cuadros 0-1 (un aleteo de 16 ms).
    pp.fseqLoopOverride = loop != 0;
    pp.fseqLoopStart = i ("fseq_lp_start"); pp.fseqLoopEnd = loop == 0 ? 0 : i ("fseq_lp_end");
    pp.fseqStart = i ("fseq_start"); pp.fseqSpeedVel = i ("fseq_vel_speed"); pp.fseqLevelVel = i ("fseq_vel_level"); pp.fseqDelay = i ("fseq_delay");
    pp.authenticPoly = i ("authentic") != 0;
    fx.insType = i ("ins_type"); fx.insAmount = get ("ins_amount"); fx.insMix = get ("ins_mix");
    fx.varType = i ("var_type"); fx.varRate = get ("var_rate"); fx.varDepth = get ("var_depth"); fx.varSend = get ("var_send"); fx.varToRev = get ("var_to_rev");
    fx.revType = i ("rev_type"); fx.revTime = get ("rev_time"); fx.revSend = get ("rev_send");
    fx.eqLow = get ("eq_low"); fx.eqHigh = get ("eq_high");
    outputGain = juce::Decibels::decibelsToGain (get ("output"));
}

} // namespace dh::fs
