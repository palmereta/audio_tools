#pragma once
// Lo que el átomo necesita saber, en datos planos: una instantánea del motor (acorde, capas, cursores, fusión) y los
// eventos de cada nota. Dos fuentes producen lo mismo: el procesador en vivo (AtomLive) y un motor aparte que corre
// una demo fuera de tiempo real (AtomFeed, para VALENCE_RECORD). Las tres direcciones visuales sólo leen esto.

#include <JuceHeader.h>
#include "../ParamsVal.h"

namespace dh::val::ui {

struct AtomLayer
{
    bool on = false;
    int length = 16, step = 0, octave = 0, octaves = 1, pattern = 0;
    float phase = 0.0f, gate = 0.5f;                 // phase: 0..1 dentro del ciclo de la capa
    double cycleBeats = 4.0;
    std::array<float, kMaxSteps> hit {};             // 0 = silencio; si no, la probabilidad (0..1)
    std::array<uint8_t, kMaxSteps> flags {};         // 1 acento, 2 ligadura, 4 ratchet
};

struct AtomSnap
{
    bool running = false;
    double beat = 0.0, bpm = 120.0;
    std::array<AtomLayer, kLayers> L;
    int protons = 0, neutrons = 0;
    std::array<uint8_t, kMaxPool> protonNotes {}, neutronNotes {};
    double fusionBeats = -1.0;
    int64_t fusionTick = -1;
    uint32_t chordSerial = 0;
    char chord[16] {};
};

// Una nota que sonó: capa, nota, velocidad, nucleón de origen (0 protón, 1 neutrón), nota del acorde de la que sale,
// paso, banderas del motor (acento, ligadura, ratchet) y hace cuántos segundos de sesión sonó.
struct SparkEv { int layer = 0, note = 60, vel = 100, nucleon = 0, src = -1, step = 0, flags = 0; float age = 0.0f; };

// Ajustes de las capas (lo que cambia poco): del motor o de los parámetros.
inline void fillAtomConfig (AtomSnap& s, const std::array<LayerParams, kLayers>& lp)
{
    for (int l = 0; l < kLayers; ++l)
    {
        auto& A = s.L[(size_t) l]; const auto& P = lp[(size_t) l];
        A.on = P.on; A.length = std::clamp (P.length, 1, kMaxSteps); A.octave = P.octave; A.octaves = P.octaves; A.pattern = P.pattern;
        A.gate = P.gate;
        A.cycleBeats = (double) A.length * (double) figureTicks (P.figure) * (double) std::max (1, P.mult) / (double) kTPB;
        for (int k = 0; k < kMaxSteps; ++k)
        {
            const auto& st = P.steps[(size_t) k];
            const bool h = k < A.length && (P.euPulses > 0 ? Engine::euclidHit (k, A.length, P.euPulses, P.euRotate) : st.on);
            A.hit[(size_t) k] = h ? std::max (0.15f, (float) st.prob / 100.0f) : 0.0f;
            A.flags[(size_t) k] = (uint8_t) ((st.accent ? 1 : 0) | (st.tie ? 2 : 0) | (st.ratchet > 1 ? 4 : 0));
        }
    }
}

// Lo que cambia en cada bloque.
inline void fillAtomView (AtomSnap& s, const Engine::View& v)
{
    s.running = v.running; s.beat = v.beat; s.bpm = v.bpm;
    for (int l = 0; l < kLayers; ++l) { s.L[(size_t) l].step = v.step[(size_t) l]; s.L[(size_t) l].phase = v.phase[(size_t) l]; }
    s.protons = v.protons; s.neutrons = v.neutrons; s.protonNotes = v.protonNotes; s.neutronNotes = v.neutronNotes;
    s.fusionBeats = v.fusionBeats; s.fusionTick = v.fusionTick; s.chordSerial = v.chordSerial;
    std::memcpy (s.chord, v.chord, sizeof (s.chord));
}

// Una demo corrida fuera de tiempo real con el mismo motor y el mismo procedimiento que valencerecord: la misma música
// que el audio de la demo. advanceTo(t) deja el motor en el segundo t de sesión (cuadros de 1/30 s: bloques de sr/30
// muestras, el motor es exacto a la muestra entre bloques).
class AtomFeed
{
public:
    bool load (const juce::File& dir, juce::String& err)
    {
        const auto pv = juce::JSON::parse (dir.getChildFile ("preset.json"));
        if (! pv.isObject()) { err = "no preset.json in " + dir.getFullPathName(); return false; }
        const auto& ps = params();
        std::vector<float> vals (ps.size());
        const auto* obj = pv.getProperty ("params", {}).getDynamicObject();
        for (size_t k = 0; k < ps.size(); ++k) vals[k] = obj != nullptr && obj->hasProperty (ps[k].id) ? (float) obj->getProperty (ps[k].id) : ps[k].def;
        vals[(size_t) paramIndex ("clock")] = 1.0f;
        bpm = 120.0;
        const auto ev = juce::JSON::parse (dir.getChildFile ("events.json"));    // el tempo con el que se renderizó (--bpm)
        bpm = ev.isObject() && (double) ev.getProperty ("bpm", 0.0) > 0.0 ? (double) ev.getProperty ("bpm", 0.0) : (double) vals[(size_t) paramIndex ("tempo")];
        eng = std::make_unique<Engine>(); eng->prepare (sr);
        applyParams (vals.data(), eng->g, eng->layers);
        const auto rows = rowsFromVar (pv.getProperty ("rows", {}));
        for (int l = 0; l < kLayers; ++l) { eng->layers[(size_t) l].steps = rows[(size_t) l]; eng->layers[(size_t) l].rowsVersion = 1; }
        autoMode = ! dir.getChildFile ("input.mid").existsAsFile();
        eng->g.run = autoMode || eng->g.source == SrcAuto;
        spb = 60.0 / bpm * sr;
        input.clear(); ii = 0; pos = 0;
        if (! autoMode)
        {
            juce::FileInputStream is (dir.getChildFile ("input.mid"));
            juce::MidiFile mf;
            if (! is.openedOk() || ! mf.readFrom (is)) { err = "cannot read input.mid"; return false; }
            const double tpq = mf.getTimeFormat() > 0 ? (double) mf.getTimeFormat() : 480.0;
            for (int t = 0; t < mf.getNumTracks(); ++t)
                for (auto* m : *mf.getTrack (t))
                {
                    const auto& msg = m->message;
                    if (msg.isMetaEvent() || msg.isSysEx() || msg.getRawDataSize() > 3 || msg.getRawDataSize() < 2) continue;
                    const int64_t at = (int64_t) std::floor (msg.getTimeStamp() / tpq * spb + 0.5 + 1.0e-9);
                    if (at < 0) continue;
                    In x { at, { msg.getRawData()[0], msg.getRawData()[1], (uint8_t) (msg.getRawDataSize() > 2 ? msg.getRawData()[2] : 0) } };
                    input.push_back (x);
                }
            std::stable_sort (input.begin(), input.end(), [] (const In& a, const In& b) { return a.at < b.at; });
        }
        name = pv.getProperty ("name", dir.getFileName()).toString();
        return true;
    }
    juce::String name;
    double tempo() const { return bpm; }

    // Corre hasta el segundo t. Las notas de los bloques recorridos van a sparks con su edad respecto de t.
    void advanceTo (double t, AtomSnap& snap, std::vector<SparkEv>& sparks)
    {
        sparks.clear();
        const int block = (int) std::lround (sr / 30.0);
        const int64_t target = (int64_t) std::llround (t * sr);
        while (pos < target)
        {
            Transport tr; tr.playing = true; tr.bpm = bpm; tr.ppq = (double) pos / spb;
            bi.clear();
            while (ii < input.size() && input[ii].at < pos + block)
            {
                InEvent x; x.offset = (int) std::max<int64_t> (0, input[ii].at - pos); x.size = 3; std::memcpy (x.data, input[ii].d, 3);
                bi.push_back (x); ++ii;
            }
            eng->process (block, tr, bi.data(), (int) bi.size(), ev);
            for (auto& e : ev)
                if ((e.data[0] & 0xF0) == 0x90 && e.data[2] > 0 && e.layer >= 0)
                {
                    SparkEv s; s.layer = e.layer; s.note = e.data[1]; s.vel = e.data[2]; s.nucleon = e.nucleon; s.src = e.src; s.step = e.step; s.flags = e.flags;
                    s.age = (float) ((double) (pos + block - (pos + e.offset)) / sr);
                    sparks.push_back (s);
                }
            pos += block;
        }
        // las que ya son viejas (avance silencioso hasta el inicio del clip) no se dibujan
        sparks.erase (std::remove_if (sparks.begin(), sparks.end(), [] (const SparkEv& s) { return s.age > 3.0f; }), sparks.end());
        fillAtomConfig (snap, eng->layers);
        fillAtomView (snap, eng->view);
    }

private:
    struct In { int64_t at; uint8_t d[3]; };
    std::unique_ptr<Engine> eng;
    std::vector<In> input; std::vector<InEvent> bi; std::vector<OutEvent> ev;
    size_t ii = 0; int64_t pos = 0;
    double sr = 48000.0, bpm = 120.0, spb = 24000.0;
    bool autoMode = false;
};

} // namespace dh::val::ui
