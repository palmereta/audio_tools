#pragma once
// Presets en JSON: { name, category, panel: {id: valor}, settings: {id: valor},
//                    sequence: [ "C4", "D#4 acc", "-", "G3 tie", "A3 slide 50%", ... ] }
// Panel 0..1 (selectores como índice). Cada paso de la secuencia es texto legible: nota o "-"
// (silencio) y banderas opcionales (tie, acc, slide, NN%).

#include <JuceHeader.h>
#include "dsp/Engine101.h"
#include "dsp/Modules101.h"

namespace dh {

struct Preset
{
    juce::String name { "Init" }, category;
    PanelValues panel = panelDefaults();
    juce::NamedValueSet settings;
    Sequence seq;
    bool hasSeq = false;
};

inline int panelIndex (const juce::String& id)
{
    for (int i = 0; i < kNumP; ++i) if (id == panelParam (i).id) return i;
    return -1;
}

inline juce::String noteName (int n)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return juce::String (names[((n % 12) + 12) % 12]) + juce::String (n / 12 - 1);
}

inline int parseNote (const juce::String& t)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int len : { 2, 1 })
        for (int i = 0; i < 12; ++i)
            if (t.substring (0, len).equalsIgnoreCase (names[i]) && (len == 2) == (juce::String (names[i]).length() == 2))
            {
                const auto oct = t.substring (len);
                if (oct.isNotEmpty() && oct.containsOnly ("-0123456789")) return juce::jlimit (0, 127, (oct.getIntValue() + 1) * 12 + i);
            }
    return -1;
}

inline juce::String stepToText (const SeqStep& s)
{
    juce::String t = s.rest ? juce::String ("-") : noteName (s.note);
    if (s.tie) t << " tie";
    if (s.accent) t << " acc";
    if (s.slide) t << " slide";
    if (s.prob < 100) t << " " << (int) s.prob << "%";
    return t;
}

inline SeqStep stepFromText (const juce::String& text, int prevNote)
{
    SeqStep s;
    auto toks = juce::StringArray::fromTokens (text.trim(), " ", "");
    toks.removeEmptyStrings();
    if (toks.isEmpty() || toks[0] == "-") { s.rest = true; s.note = (int8_t) prevNote; }
    else { const int n = parseNote (toks[0]); s.note = (int8_t) (n >= 0 ? n : prevNote); }
    for (int i = 1; i < toks.size(); ++i)
    {
        const auto& f = toks[i];
        if (f == "tie") s.tie = true;
        else if (f == "acc") s.accent = true;
        else if (f == "slide") s.slide = true;
        else if (f.endsWith ("%")) s.prob = (uint8_t) juce::jlimit (0, 100, f.getIntValue());
    }
    return s;
}

inline juce::var sequenceToVar (const Sequence& q)
{
    juce::Array<juce::var> a;
    for (int i = 0; i < q.length; ++i) a.add (stepToText (q.steps[(size_t) i]));
    return a;
}

inline Sequence sequenceFromVar (const juce::var& v)
{
    Sequence q;
    if (auto* arr = v.getArray())
    {
        int prev = 60;
        for (auto& s : *arr)
        {
            if (q.length >= Sequence::kMaxSteps) break;
            const auto st = stepFromText (s.toString(), prev);
            prev = st.note;
            q.steps[(size_t) q.length++] = st;
        }
    }
    return q;
}

inline Preset presetFromJson (const juce::var& v)
{
    Preset p;
    p.name = v.getProperty ("name", "Init").toString();
    p.category = v.getProperty ("category", "").toString();
    if (auto* obj = v.getProperty ("panel", {}).getDynamicObject())
        for (auto& kv : obj->getProperties())
            if (const int i = panelIndex (kv.name.toString()); i >= 0) p.panel[(size_t) i] = (float) kv.value;
    if (auto* obj = v.getProperty ("settings", {}).getDynamicObject())
        p.settings = obj->getProperties();
    if (v.hasProperty ("sequence")) { p.seq = sequenceFromVar (v.getProperty ("sequence", {})); p.hasSeq = true; }
    return p;
}

inline juce::var presetToJson (const Preset& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", p.name);
    o->setProperty ("category", p.category);
    auto* k = new juce::DynamicObject();
    for (int i = 0; i < kNumP; ++i) k->setProperty (panelParam (i).id, p.panel[(size_t) i]);
    o->setProperty ("panel", juce::var (k));
    auto* s = new juce::DynamicObject();
    for (auto& kv : p.settings) s->setProperty (kv.name, kv.value);
    o->setProperty ("settings", juce::var (s));
    if (p.hasSeq) o->setProperty ("sequence", sequenceToVar (p.seq));
    return juce::var (o);
}

// ── texto de los valores (host y globo del editor) ──
inline juce::String fmtTime (float s)
{
    if (s <= 0.0f) return "0 ms";
    return s < 1.0f ? juce::String (s * 1000.0f, s < 0.01f ? 1 : 0) + " ms" : juce::String (s, 2) + " s";
}

inline juce::String fmtHz (float hz)
{
    return hz < 1000.0f ? juce::String (hz, hz < 10.0f ? 2 : (hz < 100.0f ? 1 : 0)) + " Hz" : juce::String (hz / 1000.0f, 2) + " kHz";
}

inline juce::String panelName (int i)
{
    const auto& pp = panelParam (i);
    return juce::String (pp.section).isEmpty() ? juce::String (pp.name) : juce::String (pp.section) + " " + pp.name;
}

inline juce::StringArray panelChoices (int i)
{
    const auto& pp = panelParam (i);
    return pp.choices != nullptr ? juce::StringArray::fromTokens (pp.choices, "|", "") : juce::StringArray();
}

inline juce::String formatPanel (int i, float v)
{
    const auto c = panelChoices (i);
    if (! c.isEmpty()) return c[juce::jlimit (0, c.size() - 1, (int) std::lround (v))];
    const juce::String dial = juce::String (v * 10.0f, 1);
    switch ((P) i)
    {
        case P::lfo_rate:   return dial + "  (" + fmtHz (cal101::lfoHz (v)) + ")";
        case P::vcf_freq:   return dial + "  (" + fmtHz (cal101::cutoffHz (v)) + ")";
        case P::env_a:      return dial + "  (" + fmtTime (cal101::attackTime (v)) + ")";
        case P::env_d:      return dial + "  (" + fmtTime (cal101::decayTime (v)) + ")";
        case P::env_r:      return dial + "  (" + fmtTime (cal101::releaseTime (v)) + ")";
        case P::portamento: return dial + "  (" + fmtTime (cal101::portaTau (v) * 3.0f) + ")";
        case P::tune:       return (v >= 0.5f ? "+" : "") + juce::String ((v - 0.5f) * 100.0f, 0) + " ct";
        case P::vco_mod:    return dial + "  (" + juce::String (cal101::vcoModOct (v) * 12.0f, 1) + " st)";
        case P::pw:         return dial + "  (" + juce::String ((0.5f - 0.47f * v) * 100.0f, 0) + " %)";
        default:            return dial;
    }
}

} // namespace dh
