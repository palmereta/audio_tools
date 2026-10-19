#pragma once
// Voces, Fseqs y presets en JSON.
//   preset: { name, category, voice: {…}, params: {id: valor}, fseq: "nombre de la biblioteca" | fseqData: "base64" }
//   voice:  campos de VoiceData con los mismos nombres; ops: [ {…} × 8 ], nops: [ {…} × 8 ]. Lo que falta
//           queda en su valor por defecto, así los presets de fábrica se escriben cortos (tools/presetsfs1.py).
//   Fseq:   el bulk del FS1R (32 + 50·N bytes) en base64: el mismo formato que el original.

#include <JuceHeader.h>
#include "dsp/FsData.h"
#include "dsp/Fseq.h"
#include "dsp/Syx.h"

namespace dh::fs {

namespace json {
inline juce::var to (int x) { return x; }
inline juce::var to (bool x) { return x; }
template <size_t N> juce::var to (const std::array<int, N>& a) { juce::Array<juce::var> r; for (int x : a) r.add (x); return r; }
inline juce::var to (const AmpEgData& e)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("level", to (e.level)); o->setProperty ("time", to (e.time)); o->setProperty ("hold", e.hold); o->setProperty ("timeScale", e.timeScale);
    return juce::var (o);
}
inline juce::var to (const FreqEgData& e) { juce::Array<juce::var> r { e.init, e.attack, e.attackTime, e.decayTime }; return r; }
template <size_t N> juce::var to (const std::array<KnobSlot, N>& a)
{
    juce::Array<juce::var> r;
    for (auto& s : a) r.add (juce::Array<juce::var> { s.dest, s.unvoiced, s.op, s.depth });
    return r;
}

inline void from (const juce::var& v, int& x) { x = (int) v; }
inline void from (const juce::var& v, bool& x) { x = (bool) v; }
template <size_t N> void from (const juce::var& v, std::array<int, N>& a) { if (auto* r = v.getArray()) for (int i = 0; i < (int) N && i < r->size(); ++i) a[(size_t) i] = (int) (*r)[i]; }
inline void from (const juce::var& v, AmpEgData& e)
{
    if (v.hasProperty ("level")) from (v["level"], e.level);
    if (v.hasProperty ("time")) from (v["time"], e.time);
    if (v.hasProperty ("hold")) e.hold = (int) v["hold"];
    if (v.hasProperty ("timeScale")) e.timeScale = (int) v["timeScale"];
}
inline void from (const juce::var& v, FreqEgData& e) { if (auto* r = v.getArray(); r != nullptr && r->size() >= 4) { e.init = (*r)[0]; e.attack = (*r)[1]; e.attackTime = (*r)[2]; e.decayTime = (*r)[3]; } }
template <size_t N> void from (const juce::var& v, std::array<KnobSlot, N>& a)
{
    if (auto* r = v.getArray())
        for (int i = 0; i < (int) N && i < r->size(); ++i)
            if (auto* s = (*r)[i].getArray(); s != nullptr && s->size() >= 4) a[(size_t) i] = { (*s)[0], (*s)[1], (*s)[2], (*s)[3] };
}
} // namespace json

template <typename F> void visitCommon (VoiceData& d, F&& f)
{
    f ("category", d.category); f ("algorithm", d.algorithm); f ("feedback", d.feedback); f ("noteShift", d.noteShift);
    f ("carrierAtt", d.carrierAtt);
    f ("pegLevel", d.pegLevel); f ("pegTime", d.pegTime); f ("pegVel", d.pegVel); f ("pegRange", d.pegRange); f ("pegTimeScale", d.pegTimeScale);
    f ("lfo1Wave", d.lfo1Wave); f ("lfo1Speed", d.lfo1Speed); f ("lfo1Delay", d.lfo1Delay); f ("lfo1Pmd", d.lfo1Pmd); f ("lfo1Amd", d.lfo1Amd);
    f ("lfo1Fmd", d.lfo1Fmd); f ("lfo1FilterDepth", d.lfo1FilterDepth); f ("lfo1KeySync", d.lfo1KeySync);
    f ("lfo2Wave", d.lfo2Wave); f ("lfo2Speed", d.lfo2Speed); f ("lfo2Phase", d.lfo2Phase); f ("lfo2FilterDepth", d.lfo2FilterDepth); f ("lfo2KeySync", d.lfo2KeySync);
    f ("filterType", d.filterType); f ("cutoff", d.cutoff); f ("resonance", d.resonance); f ("resVel", d.resVel); f ("inputGain", d.inputGain);
    f ("keyScaleDepth", d.keyScaleDepth); f ("keyScaleBreak", d.keyScaleBreak);
    f ("fegLevel", d.fegLevel); f ("fegTime", d.fegTime); f ("fegDepth", d.fegDepth); f ("fegDepthVel", d.fegDepthVel);
    f ("fegAttackVel", d.fegAttackVel); f ("fegTimeScale", d.fegTimeScale);
    f ("formantKnob", d.formantKnob); f ("fmKnob", d.fmKnob);
}
template <typename F> void visitOp (VoicedOp& o, F&& f)
{
    f ("form", o.form); f ("fixed", o.fixed); f ("coarse", o.coarse); f ("fine", o.fine); f ("noteScale", o.noteScale);
    f ("transpose", o.transpose); f ("keySync", o.keySync); f ("detune", o.detune); f ("skirt", o.skirt); f ("bwRes", o.bwRes); f ("bwBias", o.bwBias);
    f ("eg", o.eg); f ("feg", o.feg); f ("level", o.level);
    f ("breakPoint", o.breakPoint); f ("leftDepth", o.leftDepth); f ("rightDepth", o.rightDepth); f ("leftCurve", o.leftCurve); f ("rightCurve", o.rightCurve);
    f ("freqBias", o.freqBias); f ("pitchMod", o.pitchMod); f ("freqMod", o.freqMod); f ("freqVel", o.freqVel); f ("ampMod", o.ampMod);
    f ("ampVel", o.ampVel); f ("egBias", o.egBias); f ("fseq", o.fseq); f ("fseqTrack", o.fseqTrack);
}
template <typename F> void visitNop (UnvoicedOp& u, F&& f)
{
    f ("transpose", u.transpose); f ("mode", u.mode); f ("coarse", u.coarse); f ("fine", u.fine); f ("noteScale", u.noteScale);
    f ("bandwidth", u.bandwidth); f ("bwBias", u.bwBias); f ("resonance", u.resonance); f ("skirt", u.skirt); f ("feg", u.feg);
    f ("level", u.level); f ("levelKeyScale", u.levelKeyScale); f ("eg", u.eg);
    f ("freqBias", u.freqBias); f ("freqMod", u.freqMod); f ("freqVel", u.freqVel); f ("ampMod", u.ampMod); f ("ampVel", u.ampVel);
    f ("egBias", u.egBias); f ("fseq", u.fseq);
}

inline juce::var voiceToJson (VoiceData v)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", juce::String (v.name));
    visitCommon (v, [o] (const char* k, auto& x) { o->setProperty (k, json::to (x)); });
    juce::Array<juce::var> ops, nops;
    for (int k = 0; k < kOps; ++k)
    {
        auto* a = new juce::DynamicObject(); visitOp (v.op[(size_t) k], [a] (const char* key, auto& x) { a->setProperty (key, json::to (x)); }); ops.add (juce::var (a));
        auto* b = new juce::DynamicObject(); visitNop (v.nop[(size_t) k], [b] (const char* key, auto& x) { b->setProperty (key, json::to (x)); }); nops.add (juce::var (b));
    }
    o->setProperty ("ops", ops); o->setProperty ("nops", nops);
    return juce::var (o);
}

inline VoiceData voiceFromJson (const juce::var& j)
{
    VoiceData v;
    if (! j.isObject()) return v;
    v.name = j.getProperty ("name", "Init Voice").toString().substring (0, 10).toStdString();
    visitCommon (v, [&j] (const char* k, auto& x) { if (j.hasProperty (k)) json::from (j[k], x); });
    if (auto* ops = j.getProperty ("ops", {}).getArray())
        for (int k = 0; k < kOps && k < ops->size(); ++k)
        {
            const auto& oj = (*ops)[k];
            VoicedOp o; if (k == 0) o.level = v.op[0].level;
            visitOp (o, [&oj] (const char* key, auto& x) { if (oj.hasProperty (key)) json::from (oj[key], x); });
            v.op[(size_t) k] = o;
        }
    if (auto* nops = j.getProperty ("nops", {}).getArray())
        for (int k = 0; k < kOps && k < nops->size(); ++k)
        {
            const auto& nj = (*nops)[k];
            UnvoicedOp u;
            visitNop (u, [&nj] (const char* key, auto& x) { if (nj.hasProperty (key)) json::from (nj[key], x); });
            v.nop[(size_t) k] = u;
        }
    // Rango.
    v.algorithm = juce::jlimit (0, 87, v.algorithm); v.feedback = juce::jlimit (0, 7, v.feedback);
    for (auto& o : v.op) { o.form = juce::jlimit (0, 7, o.form); o.skirt = juce::jlimit (0, 7, o.skirt); o.fseqTrack = juce::jlimit (0, 7, o.fseqTrack); }
    return v;
}

inline juce::String fseqToBase64 (const FseqData& f)
{
    const auto d = syx::writeFseqData (f);
    juce::String nm (f.name);
    return nm.replaceCharacter ('|', ' ') + "|" + juce::Base64::toBase64 (d.data(), d.size());
}
inline FseqData fseqFromBase64 (const juce::String& s)
{
    juce::MemoryOutputStream mo;
    const auto b64 = s.fromFirstOccurrenceOf ("|", false, false);
    if (! juce::Base64::convertFromBase64 (mo, b64.isEmpty() ? s : b64) || mo.getDataSize() < 32 + 50) return {};
    std::vector<uint8_t> d ((const uint8_t*) mo.getData(), (const uint8_t*) mo.getData() + mo.getDataSize());
    auto f = syx::readFseq (d);
    if (s.containsChar ('|')) f.name = s.upToFirstOccurrenceOf ("|", false, false).toStdString();
    return f;
}

struct Preset
{
    juce::String name { "Init" }, category;
    VoiceData voice;
    juce::NamedValueSet params;
    juce::String fseqRef;            // nombre en la biblioteca de fábrica
    FseqData fseqInline;             // o el Fseq propio (importado o analizado)
};

inline Preset presetFromJson (const juce::var& v)
{
    Preset p;
    p.name = v.getProperty ("name", "Init").toString();
    p.category = v.getProperty ("category", "").toString();
    p.voice = voiceFromJson (v.getProperty ("voice", {}));
    if (p.voice.name == "Init Voice") p.voice.name = p.name.substring (0, 10).toStdString();
    if (auto* obj = v.getProperty ("params", {}).getDynamicObject()) p.params = obj->getProperties();
    p.fseqRef = v.getProperty ("fseq", "").toString();
    if (v.hasProperty ("fseqData")) p.fseqInline = fseqFromBase64 (v["fseqData"].toString());
    return p;
}

inline juce::var presetToJson (const Preset& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("name", p.name);
    o->setProperty ("category", p.category);
    o->setProperty ("voice", voiceToJson (p.voice));
    auto* prm = new juce::DynamicObject();
    for (auto& kv : p.params) prm->setProperty (kv.name, kv.value);
    o->setProperty ("params", juce::var (prm));
    if (p.fseqRef.isNotEmpty()) o->setProperty ("fseq", p.fseqRef);
    else if (! p.fseqInline.empty()) o->setProperty ("fseqData", fseqToBase64 (p.fseqInline));
    return juce::var (o);
}

} // namespace dh::fs
