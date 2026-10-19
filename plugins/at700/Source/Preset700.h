#pragma once
// Presets en JSON: { name, category, panel: {id: valor}, settings: {id: valor},
//                    cables: [{src, dst, color}], demo: {notes, secs} }

#include <JuceHeader.h>
#include "dsp/Engine700.h"

namespace dh {

struct Cable700 { int src = -1, dst = -1; juce::String color = "red"; };

struct Preset700
{
    juce::String name { "Init" }, category;
    Panel700 panel = panelDefaults700();
    juce::NamedValueSet settings;
    std::vector<Cable700> cables;
    bool run = false;       // presets de secuencia: el 717A arranca al cargarlos (como en los demos)
};

inline int panelIndex700 (const juce::String& id)
{
    for (int i = 0; i < kNumP700; ++i) if (id == panelParam700 (i).id) return i;
    return -1;
}

inline int jackIndex700 (const juce::String& id)
{
    for (int i = 0; i < jk700::kNumJacks; ++i) if (id == jk700::jacks[i].id) return i;
    return -1;
}

inline bool canBeSource700 (int j) { return j >= 0 && jk700::jacks[j].io != jk700::In; }
inline bool canBeDest700 (int j) { return j >= 0 && jk700::jacks[j].io != jk700::Out; }

inline Preset700 presetFromJson700 (const juce::var& v)
{
    Preset700 p;
    p.name = v.getProperty ("name", "Init").toString();
    p.category = v.getProperty ("category", "").toString();
    p.run = (bool) v.getProperty ("run", false);
    if (auto* obj = v.getProperty ("panel", {}).getDynamicObject())
        for (auto& kv : obj->getProperties())
            if (const int i = panelIndex700 (kv.name.toString()); i >= 0) p.panel[(size_t) i] = (float) kv.value;
    if (auto* obj = v.getProperty ("settings", {}).getDynamicObject()) p.settings = obj->getProperties();
    if (auto* arr = v.getProperty ("cables", {}).getArray())
        for (auto& c : *arr)
        {
            Cable700 cb;
            cb.src = jackIndex700 (c.getProperty ("src", "").toString());
            cb.dst = jackIndex700 (c.getProperty ("dst", "").toString());
            cb.color = c.getProperty ("color", "red").toString();
            if (canBeSource700 (cb.src) && canBeDest700 (cb.dst) && cb.src != cb.dst) p.cables.push_back (cb);
        }
    return p;
}

inline juce::var presetToJson700 (const Preset700& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", p.name);
    o->setProperty ("category", p.category);
    auto* k = new juce::DynamicObject();
    for (int i = 0; i < kNumP700; ++i) k->setProperty (panelParam700 (i).id, p.panel[(size_t) i]);
    o->setProperty ("panel", juce::var (k));
    auto* s = new juce::DynamicObject();
    for (auto& kv : p.settings) s->setProperty (kv.name, kv.value);
    o->setProperty ("settings", juce::var (s));
    juce::Array<juce::var> cables;
    for (auto& c : p.cables)
    {
        auto* co = new juce::DynamicObject();
        co->setProperty ("src", jk700::jacks[c.src].id);
        co->setProperty ("dst", jk700::jacks[c.dst].id);
        co->setProperty ("color", c.color);
        cables.add (juce::var (co));
    }
    o->setProperty ("cables", cables);
    return juce::var (o);
}

inline PatchState700 patchFromCables700 (const std::vector<Cable700>& cables)
{
    PatchState700 ps;
    for (auto& c : cables) ps.src[(size_t) c.dst] = (int16_t) c.src;
    return ps;
}

inline juce::StringArray panelChoices700 (int i)
{
    const auto& pp = panelParam700 (i);
    return juce::String (pp.choices).isNotEmpty() ? juce::StringArray::fromTokens (pp.choices, "|", "") : juce::StringArray();
}

inline juce::String hzText700 (float hz) { return hz < 1000 ? juce::String (hz, hz < 10 ? 2 : (hz < 100 ? 1 : 0)) + " Hz" : juce::String (hz / 1000.0f, 2) + " kHz"; }
inline juce::String secText700 (float s) { return s < 1 ? juce::String (s * 1000.0f, s < 0.01f ? 1 : 0) + " ms" : juce::String (s, 2) + " s"; }

inline juce::String formatPanel700 (int i, float v)
{
    const auto c = panelChoices700 (i);
    if (! c.isEmpty()) return c[juce::jlimit (0, c.size() - 1, (int) std::lround (v))];
    const juce::String id (panelParam700 (i).id);
    if (id.endsWith ("_cutoff")) return hzText700 (20.0f * std::pow (1500.0f, v));
    if (id.endsWith ("_freq") && id.startsWith ("lfo")) return hzText700 (0.03f * std::pow (1000.0f, v));
    if (id == "sh_rate") return hzText700 (0.3f * std::pow (100.0f, v));
    if (id == "ps_speed") return hzText700 (0.03f * std::pow (10.0f / 0.03f, v));
    if (id.startsWith ("adsr") && ! id.endsWith ("_s")) return secText700 (0.002f * std::pow (5000.0f, v));
    if (id == "lfo1_delay") return secText700 (0.002f * std::pow (5000.0f, v));
    if (id == "kb_porta") return secText700 (0.1f * std::pow (100.0f, v));
    if (id.endsWith ("_pitch") && id.startsWith ("vco")) { const float st = (v - 0.5f) * 24.0f; return (st >= 0 ? "+" : "") + juce::String (st, 1) + " st"; }
    if (id.startsWith ("step") && id.endsWith ("_fine")) return juce::String (1.0f + v, 2) + "x";
    if (id.endsWith ("_fine")) { const float ct = (v - 0.5f) * 200.0f; return (ct >= 0 ? "+" : "") + juce::String (ct, 0) + " ct"; }
    if (id == "kb_pitch") { const float st = (v - 0.5f) * 12.0f; return (st >= 0 ? "+" : "") + juce::String (st, 1) + " st"; }
    if (id.endsWith ("_pw")) return juce::String (10.0f + 80.0f * v, 0) + " %";
    if (id.startsWith ("pan_pos") || id == "kb_bender") { const int pc = (int) std::lround ((v - 0.5f) * 200.0f); return pc == 0 ? juce::String ("C") : (pc < 0 ? "L" + juce::String (-pc) : "R" + juce::String (pc)); }
    if (id == "clk_rate") return juce::String (2.0f * std::pow (10.0f, v), 1) + " steps/min";
    if (id == "vp_level") return "+" + juce::String (v * 10.0f, 2) + " V";
    return juce::String (v * 10.0f, 1);
}

} // namespace dh
