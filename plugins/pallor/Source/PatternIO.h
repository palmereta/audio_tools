#pragma once
// Patrones de Pallor como texto (presets, biblioteca y estado del plugin). Una pista es una lista de pasos separados
// por espacios: "." apagado, o la velocidad seguida de sus banderas:
//   a acento · pNN probabilidad % · rN ratchet (2-8) · fNN flam en ms · n±NN corrimiento en % del paso · cN condición
// Ejemplo: "100a . . . 90 . 70p50 . 127r3 . . . 100f20 . . 60n-15". Un patrón en JSON:
//   { "length": 16, "scale": 1, "tracks": { "bd": "...", "ch": "..." }, "lens": { "ch": 12 } }

#include <JuceHeader.h>
#include "ParamsPal.h"

namespace dh::pal::pio {

inline juce::String stepToText (const Step& s)
{
    if (! s.vel) return ".";
    juce::String t (s.vel);
    if (s.flags & kAccent) t << "a";
    if (s.prob < 100) t << "p" << (int) s.prob;
    if (s.ratchet > 1) t << "r" << (int) s.ratchet;
    if (s.flam) t << "f" << (int) s.flam;
    if (s.nudge) t << "n" << (int) s.nudge;
    if (s.cond) t << "c" << (int) s.cond;
    return t;
}

inline Step stepFromText (const juce::String& tok)
{
    Step s;
    if (tok.isEmpty() || tok == "." || tok == "-") return s;
    auto p = tok.getCharPointer();
    auto num = [&] () { bool neg = false; if (*p == '-') { neg = true; ++p; } else if (*p == '+') ++p; int v = 0; while (*p >= '0' && *p <= '9') { v = v * 10 + (int) (*p - '0'); ++p; } return neg ? -v : v; };
    s.vel = (uint8_t) juce::jlimit (1, 127, num());
    while (! p.isEmpty())
    {
        const auto c = *p; ++p;
        if (c == 'a') s.flags |= kAccent;
        else if (c == 'p') s.prob = (uint8_t) juce::jlimit (0, 100, num());
        else if (c == 'r') s.ratchet = (uint8_t) juce::jlimit (1, 8, num());
        else if (c == 'f') s.flam = (uint8_t) juce::jlimit (0, 80, num());
        else if (c == 'n') s.nudge = (int8_t) juce::jlimit (-50, 50, num());
        else if (c == 'c') s.cond = (uint8_t) juce::jlimit (0, (int) kNumConds - 1, num());
    }
    return s;
}

inline juce::String trackToText (const Track& t, int upTo)
{
    // Hasta el último paso encendido o el largo pedido (lo que sea mayor), sin cola de puntos.
    int last = 0; for (int i = 0; i < kMaxSteps; ++i) if (t.s[(size_t) i].vel) last = i + 1;
    const int n = std::max (last, std::min (upTo, kMaxSteps));
    juce::String out;
    for (int i = 0; i < n; ++i) { if (i) out << ' '; out << stepToText (t.s[(size_t) i]); }
    return out;
}
inline void trackFromText (Track& t, const juce::String& text)
{
    t.s = {};
    juce::StringArray toks; toks.addTokens (text, " ", ""); toks.removeEmptyStrings();
    for (int i = 0; i < std::min (kMaxSteps, toks.size()); ++i) t.s[(size_t) i] = stepFromText (toks[i]);
}

inline juce::var patternToVar (const Pattern& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("length", (int) p.length); o->setProperty ("scale", (int) p.scale);
    if (p.swing >= 50) o->setProperty ("swing", (int) p.swing);
    auto* tr = new juce::DynamicObject(); auto* lens = new juce::DynamicObject();
    for (int k = 0; k < kTracks; ++k)
    {
        const auto& t = p.t[(size_t) k];
        bool any = false; for (const auto& s : t.s) any |= s.vel > 0;
        if (any) tr->setProperty (params::voiceKey (k), trackToText (t, t.len ? t.len : p.length));
        if (t.len) lens->setProperty (params::voiceKey (k), (int) t.len);
    }
    o->setProperty ("tracks", juce::var (tr)); o->setProperty ("lens", juce::var (lens));
    return juce::var (o);
}
inline Pattern patternFromVar (const juce::var& v)
{
    Pattern p;
    if (! v.isObject()) return p;
    p.length = (uint8_t) juce::jlimit (1, kMaxSteps, (int) v.getProperty ("length", 16));
    p.scale = (uint8_t) juce::jlimit (0, 4, (int) v.getProperty ("scale", 1));
    { const int sw = (int) v.getProperty ("swing", 0); p.swing = (uint8_t) (sw >= 50 ? juce::jmin (75, sw) : 0); }
    const auto tr = v.getProperty ("tracks", {}), lens = v.getProperty ("lens", {});
    for (int k = 0; k < kTracks; ++k)
    {
        const juce::Identifier key (params::voiceKey (k));
        if (tr.hasProperty (key)) trackFromText (p.t[(size_t) k], tr.getProperty (key, "").toString());
        if (lens.hasProperty (key)) p.t[(size_t) k].len = (uint8_t) juce::jlimit (0, kMaxSteps, (int) lens.getProperty (key, 0));
    }
    return p;
}

} // namespace dh::pal::pio
