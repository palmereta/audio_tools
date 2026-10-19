#pragma once
// Controles del panel del SH-101 (los que guarda un preset y automatiza el host). Una sola tabla
// para el motor, el plugin, la interfaz, los presets y at101vrender.
//
// Valores: sliders y perillas continuas 0..1 (0..10 en el panel); selectores como índice.

#include <array>

namespace dh {

enum class P : int
{
    lfo_rate, lfo_wave, vco_mod, vco_range, pw, pwm_src,
    mix_pulse, mix_saw, mix_sub, sub_wave, mix_noise,
    vcf_freq, vcf_res, vcf_env, vcf_mod, vcf_kybd,
    vca_mode, env_trig, env_a, env_d, env_s, env_r,
    tune, volume, portamento, porta_mode, transpose,
    bend_vco, bend_vcf, bend_lfo,
    arp_mode, hold, seq_play,
    count
};
constexpr int kNumP = (int) P::count;
using PanelValues = std::array<float, kNumP>;

struct PanelParam
{
    const char* id;
    const char* name;
    const char* section;
    int steps;            // 0 = continuo; >0 = opciones
    float def;
    const char* choices;  // '|'
};

inline const PanelParam& panelParam (int i)
{
    static const PanelParam t[kNumP] = {
        { "lfo_rate",   "Rate",            "Modulator", 0, 0.55f, nullptr },
        { "lfo_wave",   "Wave Form",       "Modulator", 4, 0,     "Triangle|Square|Random|Noise" },
        { "vco_mod",    "Mod",             "VCO",       0, 0.0f,  nullptr },
        { "vco_range",  "Range",           "VCO",       4, 1,     "16'|8'|4'|2'" },
        { "pw",         "Pulse Width",     "VCO",       0, 0.0f,  nullptr },
        { "pwm_src",    "PW Mod Source",   "VCO",       3, 1,     "LFO|Man|Env" },
        { "mix_pulse",  "Pulse",           "Mixer",     0, 0.0f,  nullptr },
        { "mix_saw",    "Saw",             "Mixer",     0, 1.0f,  nullptr },
        { "mix_sub",    "Sub Osc",         "Mixer",     0, 0.0f,  nullptr },
        { "sub_wave",   "Sub Wave",        "Mixer",     3, 0,     "1 Oct Down|2 Oct Down|2 Oct Pulse" },
        { "mix_noise",  "Noise",           "Mixer",     0, 0.0f,  nullptr },
        { "vcf_freq",   "Freq",            "VCF",       0, 0.62f, nullptr },
        { "vcf_res",    "Res",             "VCF",       0, 0.0f,  nullptr },
        { "vcf_env",    "Env",             "VCF",       0, 0.3f,  nullptr },
        { "vcf_mod",    "Mod",             "VCF",       0, 0.0f,  nullptr },
        { "vcf_kybd",   "Kybd",            "VCF",       0, 1.0f,  nullptr },
        { "vca_mode",   "Mode",            "VCA",       2, 0,     "Env|Gate" },
        { "env_trig",   "Trigger",         "Env",       3, 0,     "Gate+Trig|Gate|LFO" },
        { "env_a",      "Attack",          "Env",       0, 0.0f,  nullptr },
        { "env_d",      "Decay",           "Env",       0, 0.45f, nullptr },
        { "env_s",      "Sustain",         "Env",       0, 0.6f,  nullptr },
        { "env_r",      "Release",         "Env",       0, 0.3f,  nullptr },
        { "tune",       "Tune",            "",          0, 0.5f,  nullptr },
        { "volume",     "Volume",          "",          0, 0.75f, nullptr },
        { "portamento", "Portamento",      "",          0, 0.0f,  nullptr },
        { "porta_mode", "Portamento Mode", "",          3, 1,     "Auto|Off|On" },
        { "transpose",  "Transpose",       "",          3, 1,     "L|M|H" },
        { "bend_vco",   "Bender VCO",      "Bender",    0, 0.2f,  nullptr },
        { "bend_vcf",   "Bender VCF",      "Bender",    0, 0.0f,  nullptr },
        { "bend_lfo",   "LFO Mod",         "Bender",    0, 0.5f,  nullptr },
        { "arp_mode",   "Arpeggio",        "",          4, 0,     "Off|Down|U&D|Up" },
        { "hold",       "Hold",            "",          2, 0,     "Off|On" },
        { "seq_play",   "Sequencer Play",  "",          2, 0,     "Off|On" },
    };
    return t[i];
}

inline PanelValues panelDefaults()
{
    PanelValues v {};
    for (int i = 0; i < kNumP; ++i) v[(size_t) i] = panelParam (i).def;
    return v;
}

} // namespace dh
