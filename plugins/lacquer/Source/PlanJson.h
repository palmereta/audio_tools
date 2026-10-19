#pragma once
// Plan ⇄ JSON (estado del plugin y archivo de informe). Guarda todo lo que la cadena necesita para sonar igual al
// reabrir el proyecto, más lo que la interfaz muestra del análisis.

#include <JuceHeader.h>
#include "dsp/EngineMaster.h"

namespace dh::lq::json {

inline juce::var arr (const BandCurve& c) { juce::Array<juce::var> a; for (double v : c) a.add (v); return a; }
inline BandCurve curve (const juce::var& v) { BandCurve c {}; if (auto* a = v.getArray()) for (int i = 0; i < kBands && i < a->size(); ++i) c[(size_t) i] = (double) (*a)[i]; return c; }

inline juce::var comp (const CompParams& c)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("threshold", c.thresholdDb); o->setProperty ("ratio", c.ratio); o->setProperty ("knee", c.kneeDb);
    o->setProperty ("attack", c.attackMs); o->setProperty ("release", c.releaseMs); o->setProperty ("rms", c.rmsMs); o->setProperty ("makeup", c.makeupDb);
    return o;
}
inline CompParams comp (const juce::var& v)
{
    CompParams c;
    c.thresholdDb = v.getProperty ("threshold", c.thresholdDb); c.ratio = v.getProperty ("ratio", c.ratio); c.kneeDb = v.getProperty ("knee", c.kneeDb);
    c.attackMs = v.getProperty ("attack", c.attackMs); c.releaseMs = v.getProperty ("release", c.releaseMs); c.rmsMs = v.getProperty ("rms", c.rmsMs);
    c.makeupDb = v.getProperty ("makeup", c.makeupDb);
    return c;
}
inline juce::var dyn (const DynBandParams& d)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("on", d.on); o->setProperty ("hz", d.hz); o->setProperty ("q", d.q); o->setProperty ("threshold", d.thresholdDb);
    o->setProperty ("ratio", d.ratio); o->setProperty ("maxCut", d.maxCutDb); o->setProperty ("attack", d.attackMs); o->setProperty ("release", d.releaseMs);
    return o;
}
inline DynBandParams dyn (const juce::var& v)
{
    DynBandParams d;
    d.on = v.getProperty ("on", false); d.hz = v.getProperty ("hz", d.hz); d.q = v.getProperty ("q", d.q); d.thresholdDb = v.getProperty ("threshold", d.thresholdDb);
    d.ratio = v.getProperty ("ratio", d.ratio); d.maxCutDb = v.getProperty ("maxCut", d.maxCutDb); d.attackMs = v.getProperty ("attack", d.attackMs);
    d.releaseMs = v.getProperty ("release", d.releaseMs);
    return d;
}

inline juce::var toVar (const Plan& p)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("version", 1);
    o->setProperty ("style", (int) p.opt.style); o->setProperty ("intensity", p.opt.intensity);
    o->setProperty ("targetLufs", p.targetLufs); o->setProperty ("ceiling", p.ceilingDb); o->setProperty ("matchInput", p.opt.matchInput);
    o->setProperty ("alreadyMastered", p.alreadyMastered); o->setProperty ("silent", p.silent);
    o->setProperty ("dcBlock", p.dcBlock); o->setProperty ("hpf", p.hpfHz);
    juce::Array<juce::var> eq;
    for (auto& e : p.eq) { auto* b = new juce::DynamicObject(); b->setProperty ("type", e.type); b->setProperty ("hz", e.hz); b->setProperty ("q", e.q); b->setProperty ("gain", e.gainDb); eq.add (b); }
    o->setProperty ("eq", eq); o->setProperty ("eqTrim", p.eqTrimDb); o->setProperty ("eqWant", arr (p.eqWant)); o->setProperty ("eqFit", arr (p.eqFit));
    o->setProperty ("harsh", dyn (p.harsh)); o->setProperty ("sib", dyn (p.sib));
    o->setProperty ("mbOn", p.mbOn); o->setProperty ("xo1", p.xo1); o->setProperty ("xo2", p.xo2);
    juce::Array<juce::var> mb; for (auto& c : p.mb) mb.add (comp (c)); o->setProperty ("mb", mb);
    o->setProperty ("glueOn", p.glueOn); o->setProperty ("glue", comp (p.glue));
    auto* st = new juce::DynamicObject();
    st->setProperty ("on", p.stereo.on); st->setProperty ("mono", p.stereo.monoHz); st->setProperty ("side", p.stereo.sideGainDb); st->setProperty ("sideShelf", p.stereo.sideHighShelfDb);
    o->setProperty ("stereo", st);
    auto* sa = new juce::DynamicObject();
    sa->setProperty ("on", p.sat.on); sa->setProperty ("flavour", p.sat.flavour); sa->setProperty ("drive", p.sat.driveDb); sa->setProperty ("inputLufs", p.sat.inputLufs);
    o->setProperty ("sat", sa);
    o->setProperty ("clipOn", p.clipOn); o->setProperty ("clipThreshold", p.clipThresholdDb); o->setProperty ("gain", p.gainDb);
    auto* li = new juce::DynamicObject();
    li->setProperty ("ceiling", p.lim.ceilingDb); li->setProperty ("look", p.lim.lookaheadMs); li->setProperty ("fast", p.lim.releaseFastMs); li->setProperty ("slow", p.lim.releaseSlowMs);
    o->setProperty ("limiter", li);
    o->setProperty ("outLufs", p.outLufs); o->setProperty ("outTp", p.outTruePeakDb); o->setProperty ("outLra", p.outLra);
    o->setProperty ("limAvg", p.limiterAvgGrDb); o->setProperty ("limMax", p.limiterMaxGrDb);
    juce::Array<juce::var> notes; for (auto& n : p.notes) notes.add (juce::String (n)); o->setProperty ("notes", notes);
    return o;
}

inline Plan fromVar (const juce::var& v)
{
    Plan p;
    p.opt.style = (Style) juce::jlimit (0, kStyles - 1, (int) v.getProperty ("style", 0));
    p.opt.intensity = v.getProperty ("intensity", 0.5); p.opt.matchInput = v.getProperty ("matchInput", false);
    p.targetLufs = v.getProperty ("targetLufs", -14.0); p.ceilingDb = v.getProperty ("ceiling", -1.0);
    p.opt.targetLufs = p.targetLufs; p.opt.ceilingDb = p.ceilingDb; p.opt.constantLatency = true;
    p.alreadyMastered = v.getProperty ("alreadyMastered", false); p.silent = v.getProperty ("silent", false);
    p.dcBlock = v.getProperty ("dcBlock", false); p.hpfHz = v.getProperty ("hpf", 0.0);
    if (auto* eq = v.getProperty ("eq", {}).getArray())
        for (int i = 0; i < kEqBands && i < eq->size(); ++i)
        {
            const auto& b = (*eq)[i];
            p.eq[(size_t) i] = { (int) b.getProperty ("type", 0), (double) b.getProperty ("hz", 1000.0), (double) b.getProperty ("q", 1.0), (double) b.getProperty ("gain", 0.0) };
        }
    p.eqTrimDb = v.getProperty ("eqTrim", 0.0); p.eqWant = curve (v.getProperty ("eqWant", {})); p.eqFit = curve (v.getProperty ("eqFit", {}));
    p.harsh = dyn (v.getProperty ("harsh", {})); p.sib = dyn (v.getProperty ("sib", {}));
    p.mbOn = v.getProperty ("mbOn", false); p.xo1 = v.getProperty ("xo1", 120.0); p.xo2 = v.getProperty ("xo2", 2500.0);
    if (auto* mb = v.getProperty ("mb", {}).getArray()) for (int i = 0; i < 3 && i < mb->size(); ++i) p.mb[(size_t) i] = comp ((*mb)[i]);
    p.glueOn = v.getProperty ("glueOn", false); p.glue = comp (v.getProperty ("glue", {}));
    const auto st = v.getProperty ("stereo", {});
    p.stereo.on = st.getProperty ("on", false); p.stereo.monoHz = st.getProperty ("mono", 0.0); p.stereo.sideGainDb = st.getProperty ("side", 0.0); p.stereo.sideHighShelfDb = st.getProperty ("sideShelf", 0.0);
    const auto sa = v.getProperty ("sat", {});
    p.sat.on = sa.getProperty ("on", false); p.sat.flavour = sa.getProperty ("flavour", 0); p.sat.driveDb = sa.getProperty ("drive", 0.0); p.sat.inputLufs = sa.getProperty ("inputLufs", -14.0);
    p.clipOn = v.getProperty ("clipOn", false); p.clipThresholdDb = v.getProperty ("clipThreshold", 0.0); p.gainDb = v.getProperty ("gain", 0.0);
    const auto li = v.getProperty ("limiter", {});
    p.lim.ceilingDb = li.getProperty ("ceiling", -1.0); p.lim.lookaheadMs = li.getProperty ("look", 3.0);
    p.lim.releaseFastMs = li.getProperty ("fast", 30.0); p.lim.releaseSlowMs = li.getProperty ("slow", 300.0);
    p.outLufs = v.getProperty ("outLufs", -70.0); p.outTruePeakDb = v.getProperty ("outTp", -100.0); p.outLra = v.getProperty ("outLra", 0.0);
    p.limiterAvgGrDb = v.getProperty ("limAvg", 0.0); p.limiterMaxGrDb = v.getProperty ("limMax", 0.0);
    if (auto* n = v.getProperty ("notes", {}).getArray()) for (auto& s : *n) p.notes.push_back (s.toString().toStdString());
    return p;
}

// Lo que la interfaz necesita del análisis de la entrada (se guarda con el estado para redibujar sin el audio).
struct InputSummary
{
    bool valid = false;
    double lufs = -70, tp = -100, lra = 0, plr = 0, seconds = 0, correlation = 1;
    BandCurve mid {};
    std::vector<float> groove;                       // nivel (dB) cada 50 ms de lo aprendido, para el disco
};
inline juce::var toVar (const InputSummary& s)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("valid", s.valid); o->setProperty ("lufs", s.lufs); o->setProperty ("tp", s.tp); o->setProperty ("lra", s.lra);
    o->setProperty ("plr", s.plr); o->setProperty ("seconds", s.seconds); o->setProperty ("corr", s.correlation); o->setProperty ("mid", arr (s.mid));
    juce::MemoryBlock mb (s.groove.data(), s.groove.size() * sizeof (float));
    o->setProperty ("groove", mb.toBase64Encoding());
    return o;
}
inline InputSummary summaryFromVar (const juce::var& v)
{
    InputSummary s;
    s.valid = v.getProperty ("valid", false); s.lufs = v.getProperty ("lufs", -70.0); s.tp = v.getProperty ("tp", -100.0); s.lra = v.getProperty ("lra", 0.0);
    s.plr = v.getProperty ("plr", 0.0); s.seconds = v.getProperty ("seconds", 0.0); s.correlation = v.getProperty ("corr", 1.0); s.mid = curve (v.getProperty ("mid", {}));
    juce::MemoryBlock mb; mb.fromBase64Encoding (v.getProperty ("groove", "").toString());
    s.groove.resize (mb.getSize() / sizeof (float));
    if (! s.groove.empty()) std::memcpy (s.groove.data(), mb.getData(), s.groove.size() * sizeof (float));
    return s;
}

} // namespace dh::lq::json
