#pragma once
// Presets en JSON: { name, category, panel: {id: valor}, settings: {id: valor},
//                    cables: [{src, dst, color}], keyseq: [semitonos o -1] }

#include <JuceHeader.h>
#include "dsp/Engine42.h"

namespace dh {

struct Cable42 { int src = -1, dst = -1; juce::String color = "red"; };

struct Preset42
{
    juce::String name { "Init" }, category;
    Panel42 panel = panelDefaults42();
    juce::NamedValueSet settings;
    std::vector<Cable42> cables;
    std::array<int, 16> keySeq {};
    int keySeqCount = 0;
    Preset42() { keySeq.fill (-1); }
};

inline int panelIndex42 (const juce::String& id)
{
    for (int i = 0; i < kNumP42; ++i) if (id == panelParam42 (i).id) return i;
    return -1;
}

inline int jackIndex42 (const juce::String& id)
{
    for (int i = 0; i < jk42::kNumJacks; ++i) if (id == jk42::jacks[i].id) return i;
    return -1;
}

inline Preset42 presetFromJson42 (const juce::var& v)
{
    Preset42 p;
    p.name = v.getProperty ("name", "Init").toString();
    p.category = v.getProperty ("category", "").toString();
    if (auto* obj = v.getProperty ("panel", {}).getDynamicObject())
        for (auto& kv : obj->getProperties())
            if (const int i = panelIndex42 (kv.name.toString()); i >= 0) p.panel[(size_t) i] = (float) kv.value;
    if (auto* obj = v.getProperty ("settings", {}).getDynamicObject()) p.settings = obj->getProperties();
    if (auto* arr = v.getProperty ("cables", {}).getArray())
        for (auto& c : *arr)
        {
            Cable42 cb;
            cb.src = jackIndex42 (c.getProperty ("src", "").toString());
            cb.dst = jackIndex42 (c.getProperty ("dst", "").toString());
            cb.color = c.getProperty ("color", "red").toString();
            if (cb.src >= 0 && cb.dst >= 0 && jk42::jacks[cb.src].isOutput && ! jk42::jacks[cb.dst].isOutput) p.cables.push_back (cb);
        }
    if (auto* arr = v.getProperty ("keyseq", {}).getArray())
        for (auto& n : *arr) if (p.keySeqCount < 16) p.keySeq[(size_t) p.keySeqCount++] = (int) n;
    return p;
}

inline juce::var presetToJson42 (const Preset42& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", p.name);
    o->setProperty ("category", p.category);
    auto* k = new juce::DynamicObject();
    for (int i = 0; i < kNumP42; ++i) k->setProperty (panelParam42 (i).id, p.panel[(size_t) i]);
    o->setProperty ("panel", juce::var (k));
    auto* s = new juce::DynamicObject();
    for (auto& kv : p.settings) s->setProperty (kv.name, kv.value);
    o->setProperty ("settings", juce::var (s));
    juce::Array<juce::var> cables;
    for (auto& c : p.cables)
    {
        auto* co = new juce::DynamicObject();
        co->setProperty ("src", jk42::jacks[c.src].id);
        co->setProperty ("dst", jk42::jacks[c.dst].id);
        co->setProperty ("color", c.color);
        cables.add (juce::var (co));
    }
    o->setProperty ("cables", cables);
    juce::Array<juce::var> ks;
    for (int i = 0; i < p.keySeqCount; ++i) ks.add (p.keySeq[(size_t) i]);
    o->setProperty ("keyseq", ks);
    return juce::var (o);
}

inline PatchState42 patchFromCables42 (const std::vector<Cable42>& cables)
{
    PatchState42 ps;
    for (auto& c : cables) ps.src[(size_t) c.dst] = (int8_t) c.src;
    return ps;
}

inline juce::StringArray panelChoices42 (int i)
{
    const auto& pp = panelParam42 (i);
    return juce::String (pp.choices).isNotEmpty() ? juce::StringArray::fromTokens (pp.choices, "|", "") : juce::StringArray();
}

inline juce::String formatPanel42 (int i, float v)
{
    const auto c = panelChoices42 (i);
    if (! c.isEmpty()) return c[juce::jlimit (0, c.size() - 1, (int) std::lround (v))];
    const juce::String id (panelParam42 (i).id);
    if (id.contains ("_pan")) { const int pc = (int) std::lround ((v - 0.5f) * 200.0f); return pc == 0 ? juce::String ("C") : (pc < 0 ? "L" + juce::String (-pc) : "R" + juce::String (pc)); }
    if (id.startsWith ("seq_step")) return juce::String (v * 5.0f, 2) + " V";
    if (id.startsWith ("flt_freq")) { const float hz = 20.0f * std::pow (1000.0f, v); return hz < 1000 ? juce::String (hz, 0) + " Hz" : juce::String (hz / 1000.0f, 2) + " kHz"; }
    if (id.contains ("_tune") && id.startsWith ("d"))
    {
        const int osc = id.getLastCharacters (1).getIntValue() - 1;
        if (osc >= 0 && osc < 5) return juce::String (ClassicVoice::tuneHz (osc, v), 1) + " Hz";
    }
    return juce::String (v * 10.0f, 1);
}

} // namespace dh
