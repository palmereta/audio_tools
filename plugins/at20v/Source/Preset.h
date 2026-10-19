#pragma once
// Presets en JSON: { name, category, knobs: {id: valor}, cables: [{src, dst, color}], settings: {} }.
// Valores de perilla normalizados (0..1; selectores como índice), igual que en el motor.

#include <JuceHeader.h>
#include "dsp/Engine.h"

namespace dh {

struct Cable
{
    int src = -1, dst = -1;          // índices de JackId (salida → entrada)
    juce::String color = "red";
};

struct Preset
{
    juce::String name, category;
    KnobValues knobs = Engine::normalSetting();
    std::vector<Cable> cables;
    juce::NamedValueSet settings;
};

inline int knobIndex (const juce::String& id)
{
    for (int i = 0; i < kNumKnobs; ++i) if (id == layout::knobs[i].id) return i;
    return -1;
}

inline int jackIndex (const juce::String& id)
{
    for (int i = 0; i < kNumJacks; ++i) if (id == layout::jacks[i].id) return i;
    return -1;
}

inline Preset presetFromJson (const juce::var& v)
{
    Preset p;
    p.name = v.getProperty ("name", "Init").toString();
    p.category = v.getProperty ("category", "").toString();
    if (auto* obj = v.getProperty ("knobs", {}).getDynamicObject())
        for (auto& kv : obj->getProperties())
            if (const int i = knobIndex (kv.name.toString()); i >= 0) p.knobs[(size_t) i] = (float) kv.value;
    if (auto* arr = v.getProperty ("cables", {}).getArray())
        for (auto& c : *arr)
        {
            Cable cb;
            cb.src = jackIndex (c.getProperty ("src", "").toString());
            cb.dst = jackIndex (c.getProperty ("dst", "").toString());
            cb.color = c.getProperty ("color", "red").toString();
            if (cb.src >= 0 && cb.dst >= 0 && layout::jacks[cb.src].isOutput && ! layout::jacks[cb.dst].isOutput)
                p.cables.push_back (cb);
        }
    if (auto* obj = v.getProperty ("settings", {}).getDynamicObject())
        p.settings = obj->getProperties();
    return p;
}

inline juce::var presetToJson (const Preset& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", p.name);
    o->setProperty ("category", p.category);
    auto* k = new juce::DynamicObject();
    for (int i = 0; i < kNumKnobs; ++i) k->setProperty (layout::knobs[i].id, p.knobs[(size_t) i]);
    o->setProperty ("knobs", juce::var (k));
    juce::Array<juce::var> cables;
    for (auto& c : p.cables)
    {
        auto* co = new juce::DynamicObject();
        co->setProperty ("src", layout::jacks[c.src].id);
        co->setProperty ("dst", layout::jacks[c.dst].id);
        co->setProperty ("color", c.color);
        cables.add (juce::var (co));
    }
    o->setProperty ("cables", cables);
    auto* s = new juce::DynamicObject();
    for (auto& kv : p.settings) s->setProperty (kv.name, kv.value);
    o->setProperty ("settings", juce::var (s));
    return juce::var (o);
}

inline PatchState patchFromCables (const std::vector<Cable>& cables)
{
    PatchState ps;
    for (auto& c : cables) ps.src[(size_t) c.dst] = (int8_t) c.src;
    return ps;
}

} // namespace dh
