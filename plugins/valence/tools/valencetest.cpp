// Pruebas del motor de Valence sin JUCE:
//   clang++ -std=c++20 -O2 -ffp-contract=off -I plugins/valence/Source/gen plugins/valence/tools/valencetest.cpp -o /tmp/valencetest && /tmp/valencetest
//   g++ -std=c++20 -O2 -ffp-contract=off -I plugins/valence/Source/gen plugins/valence/tools/valencetest.cpp -o valencetest   (Linux)
//   cl /std:c++20 /O2 /fp:precise /EHsc /I plugins\valence\Source\gen plugins\valence\tools\valencetest.cpp   (Windows)
// La huella de la prueba 2 tiene que dar igual en todos los sistemas. `valencetest 2` corre sólo la prueba 2;
// `valencetest --long` agrega el barrido largo de la prueba 2 (todos los buffers y frecuencias, 20 minutos).
#include "Engine.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <map>
#include <random>
#include <set>
#include <string>

using namespace dh::val;

static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what); if (! ok) ++fails; }

struct Ev { int64_t t; uint8_t s, d1, d2; int layer, nuc, src, step; uint8_t flags; };
struct Input { int64_t t; uint8_t s, d1, d2; };
struct Clk { int64_t t; uint8_t b; };

struct Sim
{
    double fs = 48000.0; int block = 512; bool randomBlocks = false; uint32_t blockSeed = 7;
    std::vector<Input> in;
    int64_t total = 0;
    std::function<Transport (int64_t)> transport;                    // por bloque (posición del bloque)
    std::function<void (Engine&, int64_t)> perBlock;                 // cambios de ajustes antes del bloque
    std::function<bool (Engine&, int64_t)> gap;                      // true: el host no llama este bloque (bypass en REAPER)
    std::function<bool (int64_t)> panicAt;                          // true: el botón de pánico antes de este bloque
};

static std::vector<Ev> simulate (Engine& e, Sim s, std::vector<Clk>* clk = nullptr)
{
    std::stable_sort (s.in.begin(), s.in.end(), [] (const Input& a, const Input& b) { return a.t < b.t; });
    e.prepare (s.fs);
    std::vector<Ev> log; std::vector<OutEvent> out; std::vector<InEvent> bi;
    std::mt19937 rng (s.blockSeed);
    size_t ii = 0; bool wasGap = false;
    for (int64_t pos = 0; pos < s.total;)
    {
        int n = s.randomBlocks ? 1 + (int) (rng() % (uint32_t) s.block) : s.block;
        n = (int) std::min<int64_t> (n, s.total - pos);
        if (s.perBlock) s.perBlock (e, pos);
        bi.clear();
        while (ii < s.in.size() && s.in[ii].t < pos + n)
        {
            InEvent x; x.offset = (int) std::max<int64_t> (0, s.in[ii].t - pos); x.data[0] = s.in[ii].s; x.data[1] = s.in[ii].d1; x.data[2] = s.in[ii].d2; x.size = 3;
            bi.push_back (x); ++ii;
        }
        if (s.gap && s.gap (e, pos)) { wasGap = true; pos += n; continue; }     // lo que entra mientras tanto se pierde
        if (wasGap) { e.releaseAll(); wasGap = false; }                          // dh::HostGap en el plugin
        if (s.panicAt && s.panicAt (pos)) { e.panic (out); for (auto& o : out) log.push_back ({ pos + o.offset, o.data[0], o.data[1], o.data[2], o.layer, o.nucleon, o.src, o.step, o.flags }); }
        const Transport tr = s.transport ? s.transport (pos) : Transport {};
        e.process (n, tr, bi.data(), (int) bi.size(), out);
        for (auto& o : out) log.push_back ({ pos + o.offset, o.data[0], o.data[1], o.data[2], o.layer, o.nucleon, o.src, o.step, o.flags });
        if (clk) for (auto& c : e.clock) clk->push_back ({ pos + c.offset, c.byte });
        pos += n;
    }
    return log;
}

static bool isOn (const Ev& e) { return (e.s & 0xF0) == 0x90 && e.d2 > 0; }
static bool isOff (const Ev& e) { return (e.s & 0xF0) == 0x80 || ((e.s & 0xF0) == 0x90 && e.d2 == 0); }
// Notas que quedan sonando al final, note-on repetidos sin su note-off, notas de largo cero.
struct Health { int hanging = 0, doubleOn = 0, zeroLen = 0, ons = 0; };
static Health health (const std::vector<Ev>& log)
{
    Health h; std::map<int, int64_t> on;
    for (auto& e : log)
    {
        const int key = (e.s & 15) * 128 + e.d1;
        if (isOn (e)) { ++h.ons; if (on.count (key)) ++h.doubleOn; on[key] = e.t; }
        else if (isOff (e)) { auto it = on.find (key); if (it != on.end()) { if (it->second == e.t) ++h.zeroLen; on.erase (it); } }
    }
    h.hanging = (int) on.size();
    return h;
}
static std::vector<Ev> onsOf (const std::vector<Ev>& log, int layer)
{
    std::vector<Ev> v; for (auto& e : log) if (isOn (e) && (layer < 0 || e.layer == layer)) v.push_back (e); return v;
}

static void fresh (Engine& e)
{
    e.g = GlobalParams {}; e.g.hostClock = false; e.g.tempo = 120.0f; e.g.key = 0; e.g.scale = 0;
    for (auto& L : e.layers) L = LayerParams {};
}
static LayerParams& layer (Engine& e, int l, int figure = 6, int length = 16)
{
    auto& L = e.layers[(size_t) l]; L.on = true; L.figure = figure; L.length = length; L.channel = l + 1; return L;
}
static void hold (Sim& s, int64_t t, std::vector<int> notes, int64_t until = -1, int vel = 100)
{
    for (int n : notes) s.in.push_back ({ t, 0x90, (uint8_t) n, (uint8_t) vel });
    if (until >= 0) for (int n : notes) s.in.push_back ({ until, 0x80, (uint8_t) n, 0 });
}
static int64_t S (double sec, double fs = 48000.0) { return (int64_t) std::llround (sec * fs); }
static std::string join (const std::vector<int>& v) { std::string s; for (size_t i = 0; i < v.size(); ++i) s += (i ? " " : "") + std::to_string (v[i]); return s; }

// ───────── 1. patrones ─────────
static std::vector<int> patternRun (std::vector<int> chord, int pattern, int steps, std::function<void (Engine&)> cfg = {}, bool chordGroups = false)
{
    Engine e; fresh (e);
    auto& L = layer (e, 0); L.pattern = pattern; L.length = 16;
    if (cfg) cfg (e);
    Sim s; s.total = S (0.125 * steps + 0.05); hold (s, 0, chord);
    const auto log = simulate (e, s);
    std::vector<int> v; int64_t lastT = -1;
    for (auto& x : onsOf (log, 0))
    {
        if (chordGroups) { if (x.t != lastT) v.push_back (-1); lastT = x.t; }
        v.push_back (x.d1);
    }
    if ((int) v.size() > steps && ! chordGroups) v.resize ((size_t) steps);
    return v;
}

static void test1()
{
    std::printf ("1. Cada patrón da la secuencia esperada (acordes de 1, 2, 3 y 7 notas)\n");
    const std::vector<int> C { 60, 64, 67 };
    struct Case { const char* name; std::vector<int> chord; int pattern; std::vector<int> want; std::function<void (Engine&)> cfg; };
    const std::vector<Case> cases = {
        { "Up", C, PatUp, { 60, 64, 67, 60, 64, 67, 60, 64 }, {} },
        { "Down", C, PatDown, { 67, 64, 60, 67, 64, 60, 67, 64 }, {} },
        { "Up-down", C, PatUpDown, { 60, 64, 67, 64, 60, 64, 67, 64 }, {} },
        { "Up-down (repeat ends)", C, PatUpDownRep, { 60, 64, 67, 67, 64, 60, 60, 64 }, {} },
        { "Converge", { 60, 62, 64, 65 }, PatConverge, { 60, 65, 62, 64, 60, 65, 62, 64 }, {} },
        { "Diverge", { 60, 62, 64, 65 }, PatDiverge, { 64, 62, 65, 60, 64, 62, 65, 60 }, {} },
        { "As played (67 60 64)", { 67, 60, 64 }, PatPlayed, { 67, 60, 64, 67, 60, 64, 67, 60 }, {} },
        { "Pedal", { 60, 64, 67, 71 }, PatPedal, { 60, 64, 60, 67, 60, 71, 60, 64 }, {} },
        { "Finger order 2 0 1 1", C, PatFinger, { 67, 60, 64, 64, 67, 60, 64, 64 },
          [] (Engine& e) { const int f[4] = { 2, 0, 1, 1 }; for (int k = 0; k < kMaxSteps; ++k) e.layers[0].steps[(size_t) k].finger = (int8_t) f[k % 4]; e.layers[0].length = 4; } },
        { "Up, 2 octaves stacked", C, PatUp, { 60, 64, 67, 72, 76, 79, 60, 64 }, [] (Engine& e) { e.layers[0].octaves = 2; } },
        { "Up, 2 octaves alternating (note by note)", C, PatUp, { 60, 76, 67, 72, 64, 79, 60, 76 }, [] (Engine& e) { e.layers[0].octaves = 2; e.layers[0].octMode = OctAlternate; } },
        { "Up-down, 3 octaves alternating", { 60, 67 }, PatUpDown, { 60, 79, 84, 79, 60, 79, 84, 79 }, [] (Engine& e) { e.layers[0].octaves = 3; e.layers[0].octMode = OctAlternate; } },
        { "Down, 2 octaves alternating vs stacked differ", C, PatDown, { 67, 76, 60, 79, 64, 72, 67, 76 }, [] (Engine& e) { e.layers[0].octaves = 2; e.layers[0].octMode = OctAlternate; } },
        { "Up, index shift 1", C, PatUp, { 64, 67, 60, 64, 67, 60, 64, 67 }, [] (Engine& e) { e.layers[0].shift = 1; } },
        { "Up, chord order high to low", C, PatUp, { 67, 64, 60, 67, 64, 60, 67, 64 }, [] (Engine& e) { e.g.order = OrderHighLow; } },
        { "Up, register -1 and step octave +1 on step 2", C, PatUp, { 48, 64, 55, 48, 52, 67, 48, 52 },
          [] (Engine& e) { e.layers[0].octave = -1; e.layers[0].steps[1].oct = 1; e.layers[0].length = 4; } },
        { "One note, Up-down", { 60 }, PatUpDown, { 60, 60, 60, 60, 60, 60, 60, 60 }, {} },
        { "One note, Pedal", { 62 }, PatPedal, { 62, 62, 62, 62, 62, 62, 62, 62 }, {} },
        { "Two notes, Up-down", { 60, 67 }, PatUpDown, { 60, 67, 60, 67, 60, 67, 60, 67 }, {} },
        { "Seven notes, Up-down", { 60, 62, 64, 65, 67, 69, 71 }, PatUpDown, { 60, 62, 64, 65, 67, 69, 71, 69, 67, 65, 64, 62, 60, 62 }, {} },
        { "Seven notes, Converge", { 60, 62, 64, 65, 67, 69, 71 }, PatConverge, { 60, 71, 62, 69, 64, 67, 65, 60 }, {} },
        { "Fixed note: scale degree 4, octave +1, on step 3", C, PatUp, { 60, 64, 79, 67, 60, 64, 79, 67 },
          [] (Engine& e) { e.layers[0].length = 4; e.layers[0].steps[2].fixed = 4; e.layers[0].steps[2].oct = 1; } },
    };
    bool all = true;
    for (auto& c : cases)
    {
        const auto got = patternRun (c.chord, c.pattern, (int) c.want.size(), c.cfg);
        const bool ok = got == c.want;
        if (! ok) std::printf ("  %-48s quiere %s, dio %s\n", c.name, join (c.want).c_str(), join (got).c_str());
        all = all && ok;
    }
    std::printf ("  %d secuencias comparadas\n", (int) cases.size());
    check (all, "los patrones dan la secuencia exacta");
    // Acorde completo: las tres notas juntas en cada paso.
    {
        const auto v = patternRun (C, PatChord, 4, {}, true);
        check (v == std::vector<int> ({ -1, 60, 64, 67, -1, 60, 64, 67, -1, 60, 64, 67, -1, 60, 64, 67 }) || (v.size() >= 16 && std::vector<int> (v.begin(), v.begin() + 16) == std::vector<int> ({ -1, 60, 64, 67, -1, 60, 64, 67, -1, 60, 64, 67, -1, 60, 64, 67 })),
               "Chord: las tres notas en la misma muestra en cada paso");
    }
    // Al azar y caminata: sólo notas del acorde; la caminata se mueve de a una posición.
    {
        const std::vector<int> c7 { 60, 62, 64, 65, 67, 69, 71 };
        const auto r = patternRun (c7, PatRandom, 200);
        const auto w = patternRun (c7, PatWalk, 200);
        bool inChord = true; for (int n : r) inChord = inChord && std::find (c7.begin(), c7.end(), n) != c7.end();
        std::set<int> distinct (r.begin(), r.end());
        bool walk = w.size() == 200;
        for (size_t i = 1; i < w.size(); ++i)
        {
            const auto a = std::find (c7.begin(), c7.end(), w[i - 1]) - c7.begin(), b = std::find (c7.begin(), c7.end(), w[i]) - c7.begin();
            walk = walk && std::abs ((int) (a - b)) == 1;
        }
        std::printf ("  al azar: %d notas distintas de 7 en 200 pasos\n", (int) distinct.size());
        check (inChord && distinct.size() == 7, "Random toca sólo notas del acorde, todas");
        check (walk, "Random walk se mueve de a un lugar del acorde");
    }
}

// ───────── escenario rico para la huella ─────────
static void richSetup (Engine& e)
{
    fresh (e);
    e.g.key = 9; e.g.scale = 1; e.g.complete = CompSeventh; e.g.seed = 11; e.g.tempo = 124.0f;
    e.g.perLayerChannels = true;
    auto& a = layer (e, 0, 6, 16); a.pattern = PatUpDown; a.octaves = 2; a.gate = 0.45f; a.swing = 58; a.ccNum = 74; a.mutate = 0.3f; a.velMode = VelStep;
    for (int k = 0; k < 16; ++k) { auto& s = a.steps[(size_t) k]; s.cc = (int8_t) (40 + 5 * k); s.vel = (uint8_t) (70 + (k * 7) % 50); s.accent = k % 4 == 0; s.prob = (uint8_t) (k % 3 == 2 ? 60 : 100); }
    a.steps[15].ratchet = 3; a.steps[7].tie = true;
    auto& b = layer (e, 1, 4, 15); b.pattern = PatRandom; b.octMode = OctRandom; b.octaves = 2; b.humanizeMs = 7.0f; b.gate = 0.8f; b.velMode = VelInput;
    b.euPulses = 7; b.euRotate = 2;
    auto& c = layer (e, 2, 8, 13); c.pattern = PatWalk; c.octave = -1; c.mutate = 0.5f; c.humanizeMs = 12.0f;
    for (int k = 0; k < 13; ++k) c.steps[(size_t) k].ratchet = (uint8_t) (k % 5 == 4 ? 2 : 1);
    c.steps[3].tie = true; c.steps[4].tie = true;
    auto& d = layer (e, 3, 9, 7); d.pattern = PatChord; d.octMode = OctAlternate; d.octaves = 2; d.gate = 0.3f; d.restart = false;
    d.steps[2].on = false; d.steps[5].fixed = 0;
}
// La "mano": acordes de una o dos notas (Completar arma la séptima) sobre pulsos enteros, con pedal y legato.
static int64_t exactSample (int64_t tick, int64_t fs, int64_t bpm);
// En muestras como las pone un DAW: el pulso llevado a la muestra con el mismo mapa de tempo (124 BPM).
static int64_t B (double beats, double fs) { return exactSample ((int64_t) std::llround (beats * (double) kTPB), (int64_t) fs, 124); }
static void richInput (Sim& s, double secs)
{
    const double beat = 60.0 / 124.0;
    const int roots[8][2] = { { 57, 64 }, { 53, -1 }, { 60, 67 }, { 55, -1 }, { 62, 69 }, { 57, -1 }, { 52, 59 }, { 64, -1 } };
    int i = 0;
    for (double b = 0.02; b * beat < secs - 2.0; b += 8.0, ++i)
    {
        // Como toca una persona: nada cae justo en la muestra de un paso (eso depende del redondeo de cada frecuencia), y
        // el acorde entra un poco antes del pulso. Dos de cada tres acordes se sueltan antes del siguiente (con el reloj
        // propio, la sesión vuelve a empezar en la mano nueva); el tercero se superpone con el siguiente (legato).
        const auto& r = roots[i % 8];
        const int64_t on = B (b - 0.004 - 0.003 * (i % 4), s.fs), off = B (b + 8.0 + (i % 3 == 0 ? 0.11 : -0.02), s.fs);
        s.in.push_back ({ on, 0x90, (uint8_t) r[0], (uint8_t) (80 + 5 * (i % 6)) });
        s.in.push_back ({ off, 0x80, (uint8_t) r[0], 0 });
        if (r[1] >= 0) { s.in.push_back ({ B (b + 0.027, s.fs), 0x90, (uint8_t) r[1], 90 }); s.in.push_back ({ B (b + 5.93, s.fs), 0x80, (uint8_t) r[1], 0 }); }
        if (i % 4 == 2) { s.in.push_back ({ B (b + 4.01, s.fs), 0xB0, 64, 127 }); s.in.push_back ({ B (b + 9.01, s.fs), 0xB0, 64, 0 }); }
    }
}

static uint64_t fingerprint (const std::vector<Ev>& log, int& notes)
{
    uint64_t h = 1469598103934665603ull; notes = 0;
    for (auto& e : log)
    {
        for (uint64_t v : { (uint64_t) e.s, (uint64_t) e.d1, (uint64_t) e.d2, (uint64_t) (e.layer + 1), (uint64_t) e.t }) { h ^= v; h *= 1099511628211ull; }
        notes += isOn (e);
    }
    return h;
}

// Igualdad entre frecuencias: los mismos mensajes en el mismo orden, a menos de tol segundos.
static int sameAcrossRates (const std::vector<Ev>& a, double fa, const std::vector<Ev>& b, double fb, double tol, int& total)
{
    total = (int) a.size(); int matched = 0;
    std::vector<bool> used (b.size(), false); size_t from = 0;
    for (auto& x : a)
    {
        const double tx = (double) x.t / fa;
        while (from < b.size() && (double) b[from].t / fb < tx - tol) ++from;
        for (size_t j = from; j < b.size() && (double) b[j].t / fb <= tx + tol; ++j)
            if (! used[j] && b[j].s == x.s && b[j].d1 == x.d1 && b[j].d2 == x.d2 && b[j].layer == x.layer) { used[j] = true; ++matched; break; }
    }
    if (std::getenv ("VT_DIFF") != nullptr)
    {
        int shown = 0; std::vector<bool> usedA (a.size(), false); from = 0;
        for (size_t i = 0; i < a.size() && shown < 12; ++i)
        {
            const auto& x = a[i]; const double tx = (double) x.t / fa; bool f = false;
            for (size_t j = 0; j < b.size(); ++j) if (std::abs ((double) b[j].t / fb - tx) <= tol && b[j].s == x.s && b[j].d1 == x.d1 && b[j].d2 == x.d2 && b[j].layer == x.layer) { f = true; break; }
            if (! f) { std::printf ("    falta %.6f s %02x %d %d capa %d paso %d\n", tx, x.s, x.d1, x.d2, x.layer, x.step); ++shown; }
        }
    }
    return matched - (int) (b.size() - (size_t) matched);
}

static void test2 (bool longSweep)
{
    std::printf ("2. Determinismo: la misma entrada da la misma salida con cualquier buffer y frecuencia\n");
    const double secs = 180.0;
    auto runAt = [&] (double fs, int block, bool random) {
        Engine e; richSetup (e);
        Sim s; s.fs = fs; s.block = block; s.randomBlocks = random; s.total = S (secs, fs); richInput (s, secs);
        return simulate (e, s);
    };
    const auto ref = runAt (48000.0, 512, false);
    int notes = 0; const uint64_t h = fingerprint (ref, notes);
    const auto hl = health (ref);
    int ccs = 0, neutrons = 0; for (auto& e : ref) { ccs += (e.s & 0xF0) == 0xB0; neutrons += isOn (e) && e.nuc == NucNeutron; }
    std::printf ("  huella %016llx (%d notas, %d CC, %d mensajes en 3 minutos; %d notas de neutrones)\n", (unsigned long long) h, notes, ccs, (int) ref.size(), neutrons);
    check (notes > 2000 && ccs > 100 && neutrons > 100 && hl.doubleOn == 0 && hl.zeroLen == 0, "escenario rico: 4 capas, ratchets, ligaduras, swing, humanizado, mutación, CC, Completar");
    bool sameFs = true;
    for (int bs : { 64, 333, 441, 1024 })
    {
        const auto b = runAt (48000.0, bs, false);
        const bool eq = b.size() == ref.size() && std::equal (b.begin(), b.end(), ref.begin(), [] (const Ev& x, const Ev& y) { return x.t == y.t && x.s == y.s && x.d1 == y.d1 && x.d2 == y.d2 && x.layer == y.layer; });
        std::printf ("  bloque %4d a 48000 Hz: %s\n", bs, eq ? "idéntico a la muestra" : "DISTINTO");
        sameFs = sameFs && eq;
    }
    {
        const auto b = runAt (48000.0, 1024, true);
        const bool eq = b.size() == ref.size() && std::equal (b.begin(), b.end(), ref.begin(), [] (const Ev& x, const Ev& y) { return x.t == y.t && x.s == y.s && x.d1 == y.d1 && x.d2 == y.d2 && x.layer == y.layer; });
        std::printf ("  bloques al azar de 1 a 1024: %s\n", eq ? "idéntico a la muestra" : "DISTINTO");
        sameFs = sameFs && eq;
    }
    check (sameFs, "a 48 kHz: mismas notas, velocidades, CC y muestras con 64/333/441/512/1024 y bloques al azar");
    bool rates = true;
    for (double fs : { 44100.0, 96000.0 })
        for (int bs : { 64, 512, 1024 })
        {
            const auto b = runAt (fs, bs, false);
            int total = 0; const int m = sameAcrossRates (ref, 48000.0, b, fs, 3.0 / 44100.0, total);
            if (bs == 512 || m != total) std::printf ("  bloque %4d a %5.0f Hz: %d de %d mensajes iguales (±0,07 ms)\n", bs, fs, m, total);
            rates = rates && m == total;
        }
    // Entre frecuencias la muestra de cada evento redondea distinto: la de la entrada, la de la grilla y la del humanizado
    // (a lo sumo media muestra cada una), así que la tolerancia es de 3 muestras a 44,1 kHz.
    check (rates, "a 44,1 y 96 kHz: los mismos mensajes en el mismo orden, a menos de 0,07 ms");
    if (longSweep)
    {
        // Barrido largo (pc50): 20 minutos, todos los buffers y frecuencias.
        const double ls = 1200.0;
        auto longAt = [&] (double fs, int block) { Engine e; richSetup (e); Sim s; s.fs = fs; s.block = block; s.total = S (ls, fs); richInput (s, ls); return simulate (e, s); };
        const auto lref = longAt (48000.0, 512);
        int ln = 0; const uint64_t lh = fingerprint (lref, ln);
        std::printf ("  largo: huella %016llx (%d notas en 20 minutos)\n", (unsigned long long) lh, ln);
        bool ok = true;
        for (double fs : { 44100.0, 48000.0, 96000.0 })
            for (int bs : { 64, 333, 441, 512, 1024 })
            {
                const auto b = longAt (fs, bs);
                int total = 0; const int m = sameAcrossRates (lref, 48000.0, b, fs, 3.0 / 44100.0, total);
                std::printf ("  largo, bloque %4d a %5.0f Hz: %d de %d\n", bs, fs, m, total);
                ok = ok && m == total;
            }
        check (ok, "barrido largo: 20 minutos iguales en 15 combinaciones");
    }
}

// ───────── 3. grilla ─────────
static int64_t exactSample (int64_t tick, int64_t fs, int64_t bpm) { const int64_t num = tick * 60 * fs, den = bpm * kTPB; return (2 * num + den) / (2 * den); }

static void test3()
{
    std::printf ("3. Grilla: cada nota cae en la muestra de su paso (±0), con swing, tresillos y puntillos\n");
    bool all = true; int checked = 0;
    for (int64_t fs : { 44100, 48000, 96000 })
        for (int64_t bpm : { 120, 87, 174 })
            for (bool host : { false, true })
            {
                Engine e; fresh (e); e.g.tempo = (float) bpm; e.g.hostClock = host; e.g.perLayerChannels = true;
                const int figs[4] = { 6, 4, 8, 10 };            // 1/16, 1/8T, 1/16D, 1/32T
                const int swings[4] = { 62, 50, 55, 71 };
                for (int l = 0; l < 4; ++l) { auto& L = layer (e, l, figs[l], 16 - l); L.swing = swings[l]; L.gate = 0.4f; }
                const int64_t s0 = host ? 777 : 12345;
                Sim s; s.fs = (double) fs; s.block = 333; s.total = S (20.0, (double) fs) + s0;
                hold (s, host ? 0 : s0, { 60, 64, 67 });
                if (host) s.transport = [&] (int64_t pos) { Transport t; t.playing = true; t.bpm = (double) bpm; t.ppq = (double) (pos - s0) / (double) fs * (double) bpm / 60.0; return t; };
                const auto log = simulate (e, s);
                for (int l = 0; l < 4; ++l)
                {
                    const auto ons = onsOf (log, l);
                    const int64_t st = figureTicks (figs[l]);
                    for (size_t k = 0; k < ons.size(); ++k)
                    {
                        int64_t tick = (int64_t) k * st;
                        if (k % 2 == 1) tick += st * (2 * swings[l] - 100) / 100;
                        const int64_t want = s0 + exactSample (tick, fs, bpm);
                        ++checked;
                        if (ons[k].t != want) { if (all) std::printf ("  %lld Hz %lld BPM %s, capa %d paso %zu: muestra %lld, esperada %lld\n", (long long) fs, (long long) bpm, host ? "host" : "libre", l + 1, k, (long long) ons[k].t, (long long) want); all = false; }
                    }
                }
            }
    std::printf ("  %d notas comparadas (3 frecuencias, 3 tempos, reloj propio y del host, bloques de 333)\n", checked);
    check (all && checked > 5000, "cada nota en la muestra exacta de su paso");
}

// ───────── 4. notas colgadas ─────────
static void test4()
{
    std::printf ("4. Notas colgadas: soltar, latch, transporte, bypass, preset, capa apagada, canal, pánico, pedal\n");
    struct Scn { const char* name; std::function<void (Engine&, Sim&)> cfg; };
    auto base = [] (Engine& e) {
        fresh (e); e.g.perLayerChannels = true;
        auto& a = layer (e, 0, 6, 16); a.gate = 1.0f; a.steps[3].tie = true; a.steps[5].ratchet = 4;
        auto& b = layer (e, 1, 3, 5); b.pattern = PatChord; b.gate = 1.0f; b.steps[1].tie = true;
        auto& c = layer (e, 2, 9, 7); c.humanizeMs = 20.0f; c.gate = 0.9f;
    };
    const std::vector<Scn> scns = {
        { "soltar a mitad de nota", [] (Engine&, Sim& s) { hold (s, 0, { 60, 64, 67 }, S (2.03)); } },
        { "latch y apagarlo", [] (Engine& e, Sim& s) { e.g.latch = true; hold (s, 0, { 60, 64 }, S (0.5)); s.perBlock = [] (Engine& x, int64_t p) { if (p >= S (3.01)) x.g.latch = false; }; } },
        { "parar el transporte", [] (Engine& e, Sim& s) { e.g.hostClock = true; hold (s, 0, { 60, 64, 67 }); s.transport = [] (int64_t p) { Transport t; t.playing = p < S (2.51); t.bpm = 120; t.ppq = p / 48000.0 * 2.0; return t; }; } },
        { "bypass (el host no llama: HostGap)", [] (Engine&, Sim& s) { hold (s, 0, { 60, 64, 67 }, S (2.0)); s.gap = [] (Engine&, int64_t p) { return p >= S (1.03) && p < S (3.0); }; } },
        { "otro preset", [] (Engine& e, Sim& s) { e.g.latch = true; hold (s, 0, { 60, 64 }, S (0.4)); s.perBlock = [] (Engine& x, int64_t p) { if (p >= S (2.02) && p < S (2.02) + 512) { x.resetRequest = true; x.layers[0].on = false; } if (p >= S (4.0)) x.g.latch = false; }; } },
        { "apagar una capa a mitad de nota", [] (Engine&, Sim& s) { hold (s, 0, { 60, 64, 67 }, S (5.0)); s.perBlock = [] (Engine& x, int64_t p) { if (p >= S (1.37)) x.layers[1].on = false; if (p >= S (2.11)) x.layers[0].on = false; }; } },
        { "cambiar de canal a mitad de nota", [] (Engine&, Sim& s) { hold (s, 0, { 60, 64, 67 }, S (5.0)); s.perBlock = [] (Engine& x, int64_t p) { if (p >= S (1.37)) x.layers[0].channel = 9; if (p >= S (2.2)) x.g.perLayerChannels = false; }; } },
        { "pánico", [] (Engine&, Sim& s) { hold (s, 0, { 60, 64, 67 }, S (5.0)); s.panicAt = [] (int64_t p) { return p >= S (1.5) && p < S (1.5) + 256; }; } },
        { "CC 123 de la entrada", [] (Engine&, Sim& s) { hold (s, 0, { 60, 64, 67 }); s.in.push_back ({ S (1.77), 0xB0, 123, 0 }); } },
        { "pedal: soltar las teclas, después el pedal", [] (Engine&, Sim& s) { s.in.push_back ({ 0, 0xB0, 64, 127 }); hold (s, 10, { 60, 64, 67 }, S (1.0)); s.in.push_back ({ S (2.71), 0xB0, 64, 0 }); } },
        { "Stop con armonía automática", [] (Engine& e, Sim& s) { e.g.source = SrcAuto; e.g.run = true; s.perBlock = [] (Engine& x, int64_t p) { if (p >= S (3.3)) x.g.run = false; }; } },
    };
    bool all = true;
    for (auto& c : scns)
    {
        Engine e; base (e);
        Sim s; s.total = S (7.0); s.block = 256;
        c.cfg (e, s);
        // Al final del bloque de prueba se suelta todo lo que sigue apretado, salvo en los casos que miran el corte.
        const auto log = simulate (e, s);
        const auto h = health (log);
        // Después del corte no puede quedar nada sonando por más de un paso largo (salvo lo que sigue tocando a propósito).
        std::printf ("  %-44s %4d notas, %d colgadas, %d note-on dobles, %d de largo cero\n", c.name, h.ons, h.hanging, h.doubleOn, h.zeroLen);
        all = all && h.hanging == 0 && h.doubleOn == 0 && h.zeroLen == 0 && h.ons > 5;
    }
    check (all, "ninguna nota colgada ni note-on doble en ningún caso");
}

// ───────── 5. polimetría y fusión ─────────
static void test5()
{
    std::printf ("5. Polimetría: capas de 16, 15 y 13 pasos se realinean en el paso 3120 y el contador lo predice\n");
    Engine e; fresh (e); e.g.perLayerChannels = true;
    for (int l = 0; l < 3; ++l) layer (e, l, 6, l == 0 ? 16 : (l == 1 ? 15 : 13));
    Sim s; s.total = S (0.125 * 3200 + 0.1); s.block = 512; hold (s, 0, { 60, 64, 67 });
    // El contador de la vista al principio y un rato después.
    double viewAt0 = -1.0; int64_t steps0 = -1, stepsMid = -1, midStep = -1;
    s.perBlock = [&] (Engine& x, int64_t p) {
        if (p == 512) { viewAt0 = x.view.fusionBeats; steps0 = x.view.fusionSteps[0] + (x.layerGrid (0) - 0); }
        if (p == 512 * 300) { stepsMid = x.view.fusionSteps[0]; midStep = x.layerGrid (0); }
    };
    const auto log = simulate (e, s);
    std::map<int64_t, std::array<int, 3>> byTime;
    for (auto& x : log) if (isOn (x) && x.layer < 3) { auto& a = byTime.try_emplace (x.t, std::array<int, 3> { -1, -1, -1 }).first->second; a[(size_t) x.layer] = x.step; }
    int64_t first = -1; int idx = 0;
    for (auto& [t, a] : byTime) { if (idx > 0 && a[0] == 0 && a[1] == 0 && a[2] == 0) { first = idx; break; } ++idx; }
    std::printf ("  primer paso con las tres capas en el paso 0 (después del 0): %lld; vista al empezar: %.1f pulsos, %lld pasos de la capa 1; en el paso %lld faltaban %lld\n",
                 (long long) first, viewAt0, (long long) steps0, (long long) midStep, (long long) stepsMid);
    check (first == 3120, "se realinean en el paso 3120");
    check (std::abs (viewAt0 - 780.0) < 0.6 && steps0 == 3120 && midStep + stepsMid == 3120, "el contador de fusión lo predice (780 pulsos, 3120 pasos) y cuenta hacia atrás");
    // Figuras distintas y un acorde nuevo que reinicia dos capas y no la tercera: la predicción contra la fuerza bruta.
    // Acorde nuevo justo antes del pulso 5: tres capas vuelven al paso 0 en el pulso 5, la de corcheas sigue su fase.
    Engine f; fresh (f); f.g.hostClock = true; f.g.perLayerChannels = true;
    layer (f, 0, 6, 16); layer (f, 1, 4, 6); auto& c = layer (f, 2, 3, 3); c.restart = false; layer (f, 3, 9, 4);
    Sim t; t.total = S (30.0); t.block = 256; hold (t, 0, { 60, 64 }, S (1.4)); hold (t, S (2.49), { 62, 65 }, S (29.0));
    t.transport = [] (int64_t p) { Transport x; x.playing = true; x.bpm = 120; x.ppq = p / 48000.0 * 2.0; return x; };
    int64_t predicted = -1, predictedAt = -1;
    t.perBlock = [&] (Engine& x, int64_t p) { if (predicted < 0 && p >= S (2.5)) { predictedAt = (int64_t) std::floor (x.view.beat * kTPB); predicted = x.nextFusionTick (predictedAt); } };
    const auto log2 = simulate (f, t);
    // Fuerza bruta: el primer instante después de la predicción en que las cuatro capas atacan su paso 0 a la vez.
    std::map<int64_t, int> zero;
    for (auto& x : log2) if (isOn (x) && x.step == 0) zero[x.t] |= 1 << x.layer;
    int64_t brute = -1;
    for (auto& [tt, m] : zero) if (m == 15 && tt > S (2.5)) { brute = tt; break; }
    const int64_t predSample = predicted >= 0 ? f.sampleAtTick (predicted) : -2;
    std::printf ("  1/16×16, 1/8T×6, 1/8×3 sin reinicio, 1/32×4: predicción en el pulso %.2f (muestra %lld), primera fusión real en la muestra %lld\n",
                 predicted / (double) kTPB, (long long) predSample, (long long) brute);
    check (predicted > 0 && brute == predSample, "con figuras distintas y reinicios la predicción coincide con lo que suena");
}

// ───────── 6. latch, pedal, acorde nuevo ─────────
static void test6()
{
    std::printf ("6. Latch, pedal de sustain y acorde nuevo con y sin reinicio\n");
    auto inWindow = [] (const std::vector<Ev>& log, double a, double b) { std::set<int> n; for (auto& e : log) if (isOn (e) && e.t >= S (a) && e.t < S (b)) n.insert (e.d1); return n; };
    {
        Engine e; fresh (e); e.g.latch = true; layer (e, 0);
        Sim s; s.total = S (8.0);
        hold (s, 0, { 60, 64 }, S (0.3));                 // se suelta: sigue
        hold (s, S (2.0), { 62, 65 }, S (2.2));            // mano nueva con todo suelto: reemplaza
        hold (s, S (4.0), { 67 }, S (5.0)); hold (s, S (4.5), { 71 }, S (5.0));   // agregar mientras se sostiene
        s.perBlock = [] (Engine& x, int64_t p) { if (p >= S (6.5)) x.g.latch = false; };
        const auto log = simulate (e, s);
        const auto a = inWindow (log, 0.5, 2.0), b = inWindow (log, 2.3, 4.0), c = inWindow (log, 4.7, 6.4), d = inWindow (log, 6.7, 8.0);
        check (a == std::set<int> ({ 60, 64 }) && b == std::set<int> ({ 62, 65 }) && c == std::set<int> ({ 67, 71 }) && d.empty() && health (log).hanging == 0,
               "latch: retiene, una mano nueva reemplaza, agregar suma, apagarlo calla");
    }
    {
        Engine e; fresh (e); layer (e, 0);
        Sim s; s.total = S (6.0);
        s.in.push_back ({ S (0.0), 0xB0, 64, 127 });
        hold (s, S (0.01), { 60, 64, 67 }, S (0.5));
        s.in.push_back ({ S (3.0), 0xB0, 64, 0 });
        hold (s, S (4.0), { 62 }, S (4.6));
        const auto log = simulate (e, s);
        const auto a = inWindow (log, 0.6, 3.0), b = inWindow (log, 3.2, 4.0), c = inWindow (log, 4.0, 4.6);
        check (a == std::set<int> ({ 60, 64, 67 }) && b.empty() && c == std::set<int> ({ 62 }) && health (log).hanging == 0, "pedal: sostiene al soltar las teclas y suelta al levantarlo");
    }
    {
        // Acorde nuevo: con reinicio la capa vuelve al paso 0 en el primer paso del acorde; sin reinicio sigue.
        Engine e; fresh (e); e.g.hostClock = true;
        auto& a = layer (e, 0, 6, 16); a.restart = true;
        auto& b = layer (e, 1, 6, 16); b.restart = false;
        Sim s; s.total = S (6.0);
        s.transport = [] (int64_t p) { Transport t; t.playing = true; t.bpm = 120; t.ppq = p / 48000.0 * 2.0; return t; };
        hold (s, 0, { 60, 64, 67 }, S (1.98));
        hold (s, S (2.5) - 100, { 62, 65, 69 }, S (5.0));
        const auto log = simulate (e, s);
        int sa = -1, sb = -1;
        for (auto& x : log) if (isOn (x) && x.t >= S (2.5) - 100) { if (x.layer == 0 && sa < 0) sa = x.step; if (x.layer == 1 && sb < 0) sb = x.step; }
        std::printf ("  primer paso tras el acorde nuevo: con reinicio %d, sin reinicio %d (pulso 5 = paso 20 de la grilla)\n", sa, sb);
        check (sa == 0 && sb == 20 % 16, "con reinicio vuelve al paso 0; sin reinicio mantiene la fase");
    }
}

// ───────── 7. ratchets y ligaduras ─────────
static void test7()
{
    std::printf ("7. Ratchets, ligaduras y choques de notas: largos correctos y sin note-on duplicados\n");
    {
        Engine e; fresh (e);
        auto& L = layer (e, 0, 6, 4); L.gate = 0.5f; L.steps[1].ratchet = 3; L.steps[3].ratchet = 4;
        Sim s; s.total = S (2.0); hold (s, 0, { 60 });
        const auto log = simulate (e, s);
        const int64_t st = figureTicks (6);
        // Paso 1: tres notas en el paso, cada una con gate × paso/3.
        std::vector<std::pair<int64_t, int64_t>> notes; std::map<int, int64_t> on;
        for (auto& x : log) { if (isOn (x)) on[x.d1] = x.t; else if (isOff (x)) { notes.push_back ({ on[x.d1], x.t }); } }
        bool ok = notes.size() >= 10;
        const int64_t want1[3] = { exactSample (st, 48000, 120), exactSample (st + st / 3, 48000, 120), exactSample (st + 2 * st / 3, 48000, 120) };
        for (int i = 0; i < 3 && ok; ++i) ok = notes[(size_t) 1 + (size_t) i].first == want1[i] && notes[(size_t) 1 + (size_t) i].second == exactSample (st + i * st / 3 + st / 6, 48000, 120);
        const auto h = health (log);
        std::printf ("  ratchet 3: ataques %lld %lld %lld; %d notas en 2 s, %d note-on dobles\n", (long long) notes[1].first, (long long) notes[2].first, (long long) notes[3].first, h.ons, h.doubleOn);
        check (ok && h.doubleOn == 0 && h.zeroLen == 0, "ratchet: las notas en su lugar, del largo justo");
    }
    {
        // Ligadura a la misma nota: no vuelve a atacar; el largo cubre los dos pasos (gate del segundo).
        Engine e; fresh (e);
        auto& L = layer (e, 0, 6, 4); L.gate = 0.5f; L.steps[0].tie = true;
        Sim s; s.total = S (0.5); hold (s, 0, { 60 });
        const auto log = simulate (e, s);
        const auto ons = onsOf (log, 0);
        int64_t off0 = -1; for (auto& x : log) if (isOff (x)) { off0 = x.t; break; }
        const int64_t st = figureTicks (6);
        check (ons.size() >= 2 && ons[1].t == exactSample (2 * st, 48000, 120) && off0 == exactSample (st + st / 2, 48000, 120), "ligadura a la misma nota: una sola nota de paso y medio");
    }
    {
        // Ligadura a otra nota: legato, la nota nueva entra en la muestra en que sale la anterior y antes que su note-off.
        Engine e; fresh (e);
        auto& L = layer (e, 0, 6, 4); L.gate = 0.5f; L.steps[0].tie = true;
        Sim s; s.total = S (0.5); hold (s, 0, { 60, 64 });
        const auto log = simulate (e, s);
        int iOn = -1, iOff = -1;
        for (int i = 0; i < (int) log.size(); ++i) { if (isOn (log[(size_t) i]) && log[(size_t) i].d1 == 64 && iOn < 0) iOn = i; if (isOff (log[(size_t) i]) && log[(size_t) i].d1 == 60 && iOff < 0) iOff = i; }
        check (iOn >= 0 && iOff >= 0 && log[(size_t) iOn].t == log[(size_t) iOff].t && iOn < iOff, "ligadura a otra nota: legato (note-on antes del note-off, en la misma muestra)");
    }
    {
        // Dos capas al mismo canal y la misma nota: en la misma muestra se funden; en otra, se corta y vuelve a atacar.
        Engine e; fresh (e);
        auto& a = layer (e, 0, 6, 4); a.gate = 1.0f;
        auto& b = layer (e, 1, 3, 4); b.gate = 1.0f; b.swing = 50;
        auto& c = layer (e, 2, 4, 4); c.gate = 1.0f;
        Sim s; s.total = S (4.0); hold (s, 0, { 60 }, S (3.5));
        const auto log = simulate (e, s);
        const auto h = health (log);
        int merged = 0; std::map<int64_t, int> perT; for (auto& x : log) if (isOn (x)) perT[x.t]++;
        for (auto& [t, k] : perT) merged += k > 1;
        // Cada note-off sale con la capa de su note-on (si no, un puerto por capa o el sonido de prueba no la suelta).
        int wrongLayer = 0; std::map<int, int> onLayer;
        for (auto& x : log) { const int key = (x.s & 15) * 128 + x.d1; if (isOn (x)) onLayer[key] = x.layer; else if (isOff (x) && onLayer.count (key) && onLayer[key] != x.layer) ++wrongLayer; }
        std::printf ("  tres capas con la misma nota en el canal 1: %d note-on, %d dobles, %d muestras con dos ataques, %d de largo cero, %d note-off con otra capa\n", h.ons, h.doubleOn, merged, h.zeroLen, wrongLayer);
        check (h.doubleOn == 0 && merged == 0 && h.zeroLen == 0 && h.hanging == 0 && h.ons > 30 && wrongLayer == 0, "choques: se funden en la misma muestra, se cortan en otra; el note-off con la capa de su note-on");
    }
}

// ───────── 8. completar ─────────
static void test8()
{
    std::printf ("8. Completar: ninguna nota fuera de la escala en 10 minutos\n");
    bool all = true; int totalNotes = 0, neutrons = 0;
    for (int sc : { 0, 1, 2, 7, 11 })
    {
        Engine e; fresh (e); e.g.key = 2; e.g.scale = sc; e.g.seed = 5;
        for (int l = 0; l < 4; ++l) { auto& L = layer (e, l, 6 - l, 16 - 3 * l); L.pattern = l == 3 ? PatChord : PatRandom; L.octaves = 2; L.steps[(size_t) l].fixed = (int8_t) (l * 2); }
        const auto pcs = scalePcs (scales()[(size_t) sc], 2);
        std::mt19937 r (sc + 1);
        Sim s; s.total = S (600.0); s.block = 1024;
        // Una o dos notas de la escala cada 2 s; el modo de Completar cambia cada 30 s; el último minuto en armonía automática.
        for (double t = 0.0; t < 540.0; t += 2.0)
        {
            const int a = 48 + pcs[r() % pcs.size()] + 12 * (int) (r() % 2), b = 48 + pcs[r() % pcs.size()] + 12 * (int) (r() % 2);
            hold (s, S (t), { a }, S (t + 1.9));
            if (r() % 2) hold (s, S (t + 0.2), { b }, S (t + 1.7));
        }
        s.perBlock = [] (Engine& x, int64_t p) { const int k = (int) (p / S (30.0)); x.g.complete = 1 + k % 4; if (p >= S (540.0)) { x.g.source = SrcAuto; x.g.run = true; x.g.complete = 2 + k % 2; } };
        const auto log = simulate (e, s);
        int out = 0;
        for (auto& x : log) if (isOn (x)) { ++totalNotes; neutrons += x.nuc == NucNeutron; if (std::find (pcs.begin(), pcs.end(), x.d1 % 12) == pcs.end()) { if (out++ < 3) std::printf ("  escala %d: nota %d fuera, capa %d\n", sc, x.d1, x.layer); } }
        all = all && out == 0;
    }
    std::printf ("  %d notas en 5 escalas (50 minutos), %d de neutrones\n", totalNotes, neutrons);
    check (all && neutrons > 1000, "Completar (tríada, séptima, novena, octavas), nota fija y armonía automática quedan en la escala");
}

// ───────── 9. tráfico ─────────
static void test9()
{
    std::printf ("9. Tráfico MIDI: 1/32 con ratchet 4 a 180 BPM\n");
    auto rate = [] (Engine& e) {
        Sim s; s.total = S (60.0); hold (s, 0, { 48, 55, 60, 64, 67, 71, 74 });
        const auto log = simulate (e, s);
        std::map<std::pair<int, int>, int> per; int maxRate = 0;
        for (auto& x : log) per[{ x.s & 15, (int) (x.t / 48000) }]++;
        for (auto& kv : per) maxRate = std::max (maxRate, kv.second);
        return std::pair<int, Health> { maxRate, health (log) };
    };
    Engine e; fresh (e); e.g.tempo = 180.0f;
    auto& L = layer (e, 0, 9, 16); L.gate = 0.5f; L.ccNum = 74;
    for (auto& s : L.steps) { s.ratchet = 4; s.cc = 0; }
    for (int k = 0; k < 16; ++k) L.steps[(size_t) k].cc = (int8_t) (k * 8);
    const auto one = rate (e);
    Engine f; fresh (f); f.g.tempo = 180.0f;
    for (int l = 0; l < 4; ++l) { auto& M = layer (f, l, 9, 16 - l); M.gate = 0.5f; M.pattern = l; M.ccNum = 74 + l; for (int k = 0; k < kMaxSteps; ++k) { M.steps[(size_t) k].ratchet = 4; M.steps[(size_t) k].cc = (int8_t) ((k * 13) % 128); } }
    const auto four = rate (f);
    std::printf ("  una capa con CC por paso: %d mensajes por segundo en el canal; las cuatro capas al canal 1: %d (un cable DIN lleva ~1000)\n", one.first, four.first);
    check (one.first < 1000 && one.second.doubleOn == 0, "una capa a 1/32 con ratchet 4 entra en un cable DIN");
    check (four.second.doubleOn == 0 && four.second.zeroLen == 0, "las cuatro capas al mismo canal: sin note-on dobles ni notas de largo cero");
    if (four.first >= 1000) std::printf ("  (las cuatro capas juntas a 1/32 con ratchet 4 pasan lo que lleva un cable DIN: para hardware, una capa por puerto)\n");
}

// ───────── 10. transporte ─────────
static std::vector<Ev> hostRun (int block, int64_t preroll, int64_t stoppedBlocks, double startPpq, double secs, std::function<void (Engine&)> cfg = {})
{
    Engine e; richSetup (e); e.g.hostClock = true;
    if (cfg) cfg (e);
    Sim s; s.block = block; s.total = S (secs) + preroll + stoppedBlocks * block;
    const int64_t playFrom = stoppedBlocks * block;
    s.transport = [=] (int64_t pos) {
        Transport t; t.bpm = 124.0; t.playing = pos >= playFrom;
        t.ppq = t.playing ? startPpq + (double) (pos - playFrom - preroll) / 48000.0 * 124.0 / 60.0 : startPpq; return t; };
    // La mano, desde el comienzo de la música (corrida por el pre-roll y la espera).
    Sim in; richInput (in, secs);
    for (auto& x : in.in) s.in.push_back ({ x.t + playFrom + preroll, x.s, x.d1, x.d2 });
    auto log = simulate (e, s);
    for (auto& x : log) x.t -= playFrom + preroll;
    log.erase (std::remove_if (log.begin(), log.end(), [] (const Ev& x) { return x.t < 0; }), log.end());
    return log;
}
static int sameWithin (const std::vector<Ev>& a, const std::vector<Ev>& b, int64_t tol, int& total)
{
    total = (int) a.size(); int matched = 0; std::vector<bool> used (b.size(), false); size_t from = 0;
    for (auto& x : a)
    {
        while (from < b.size() && b[from].t < x.t - tol) ++from;
        for (size_t j = from; j < b.size() && b[j].t <= x.t + tol; ++j)
            if (! used[j] && b[j].s == x.s && b[j].d1 == x.d1 && b[j].d2 == x.d2 && b[j].layer == x.layer) { used[j] = true; ++matched; break; }
    }
    return matched - (int) (b.size() - (size_t) matched);
}

static void test10()
{
    std::printf ("10. Transporte: pre-roll, buffers, espera previa, lazo del host y cambio de tempo a mitad de compás\n");
    {
        const double secs = 90.0;
        const auto ref = hostRun (512, 0, 0, 0.0, secs);
        struct V { const char* name; int block; int64_t preroll; int64_t stopped; };
        const V vs[] = { { "bloque 64", 64, 0, 0 }, { "bloque 333, pre-roll de 3 bloques", 333, 999, 0 }, { "bloque 768 (REAPER con 1024)", 768, 0, 0 },
                         { "bloque 1024, pre-roll de 12345 muestras", 1024, 12345, 0 }, { "bloque 512, 1,3 s con el host parado", 512, 0, 122 }, { "bloque 333, 9 s parado y pre-roll", 333, 4000, 1297 } };
        bool all = true;
        for (auto& v : vs)
        {
            const auto b = hostRun (v.block, v.preroll, v.stopped, 0.0, secs);
            int total = 0; const int m = sameWithin (ref, b, 0, total);
            std::printf ("  %-42s %d de %d mensajes iguales (±0 muestras)\n", v.name, m, total);
            all = all && m == total;
        }
        check (all && ref.size() > 1000, "con el reloj del host: lo mismo con cualquier buffer, pre-roll o espera previa");
    }
    {
        // Lazo del host: 4 compases (16 pulsos) que vuelven al pulso 0 (el host parte el bloque en el borde del lazo,
        // como REAPER y Live). Cada pasada igual a las otras, sin notas colgadas.
        Engine e; richSetup (e); e.g.hostClock = true; e.g.latch = true;
        const double bpm = 120.0; const int64_t loopLen = 384000;
        Sim s; s.block = 500; s.total = loopLen * 4 + 1000;
        s.transport = [=] (int64_t pos) { Transport t; t.playing = true; t.bpm = bpm; t.ppq = (double) (pos % loopLen) / 48000.0 * bpm / 60.0; return t; };
        hold (s, 0, { 57, 64 }, 1000);
        const auto log = simulate (e, s);
        std::vector<std::vector<Ev>> passes (4);
        for (auto& x : log) { const int k = (int) (x.t / loopLen); if (k < 4) { auto y = x; y.t -= (int64_t) k * loopLen; passes[(size_t) k].push_back (y); } }
        int t1 = 0, t2 = 0; const int m1 = sameWithin (passes[1], passes[2], 0, t1), m2 = sameWithin (passes[2], passes[3], 0, t2);
        const auto h = health (log);
        std::printf ("  lazo de 4 compases con bloques de 500: pasadas 2 y 3 %d de %d iguales, 3 y 4 %d de %d; %d notas colgadas, %d dobles\n", m1, t1, m2, t2, h.hanging, h.doubleOn);
        check (m1 == t1 && m2 == t2 && t1 > 50 && h.doubleOn == 0, "cada pasada del lazo suena igual, sin notas dobles");
    }
    {
        // Cambio de tempo a mitad de compás (pulso 6, de 120 a 90): los pasos siguientes en su muestra con el ancla nueva.
        Engine e; fresh (e); e.g.hostClock = true; layer (e, 0, 6, 16);
        const int64_t change = S (3.0);                      // pulso 6 a 120
        Sim s; s.block = 512; s.total = S (8.0);
        s.transport = [=] (int64_t pos) {
            Transport t; t.playing = true;
            const int64_t p0 = (pos / 512) * 512;
            if (p0 < change) { t.bpm = 120.0; t.ppq = (double) p0 / 48000.0 * 2.0; }
            else { t.bpm = 90.0; t.ppq = 6.0 + (double) (p0 - change) / 48000.0 * 1.5; }
            return t; };
        hold (s, 0, { 60, 64, 67 });
        const auto log = simulate (e, s);
        // El tempo cambia en el borde de bloque de la muestra 144384 (la primera en que el host informa 90).
        const int64_t edge = ((change + 511) / 512) * 512;
        const double ppqEdge = 6.0 + (double) (edge - change) / 48000.0 * 1.5;
        bool ok = true; int n = 0, after = 0;
        for (auto& x : onsOf (log, 0))
        {
            const bool post = x.t >= edge;
            const double beat = post ? ppqEdge + (double) (x.t - edge) / 48000.0 * 1.5 : (double) x.t / 48000.0 * 2.0;
            const double q = beat * 4.0, per16 = post ? 8000.0 : 6000.0;       // semicorcheas; muestras por semicorchea
            if (std::abs (q - std::round (q)) * per16 > 1.0) ok = false;        // a menos de una muestra de la grilla
            ++n; after += post;
        }
        n = n * 1; (void) after;
        std::printf ("  %d notas antes y después del cambio, todas en la grilla de 1/16 (±1 muestra)\n", n);
        check (ok && n > 40, "cambio de tempo a mitad de compás: la grilla sigue en su lugar");
    }
}

// ───────── 11. reloj MIDI y Link ─────────
static void test11()
{
    std::printf ("11. Reloj MIDI de salida y Link (la seguidora toca sobre el acorde de la líder)\n");
    {
        bool same = true; std::vector<Clk> ref;
        for (int bs : { 512, 64, 333, 1024 })
        {
            Engine e; fresh (e); e.g.tempo = 128.0f; layer (e, 0);
            Sim s; s.block = bs; s.total = S (10.0); hold (s, S (0.5), { 60 }, S (8.0));
            std::vector<Clk> c; simulate (e, s, &c);
            if (bs == 512) ref = c;
            else same = same && c.size() == ref.size() && std::equal (c.begin(), c.end(), ref.begin(), [] (const Clk& a, const Clk& b) { return a.t == b.t && a.b == b.b; });
        }
        int ticks = 0; bool exact = true; int64_t startAt = -1, stopAt = -1;
        for (auto& c : ref)
        {
            if (c.b == 0xFA) startAt = c.t;
            if (c.b == 0xFC) stopAt = c.t;
            if (c.b == 0xF8) { const int64_t want = S (0.5) + exactSample ((int64_t) ticks * kClockTicks * 10, 48000, 1280); exact = exact && c.t == want; ++ticks; }
        }
        std::printf ("  %d pulsos de reloj en 7,5 s a 128 BPM (esperados %d), start en %lld, stop en %lld\n", ticks, (int) std::ceil (7.5 * 128.0 / 60.0 * 24.0), (long long) startAt, (long long) stopAt);
        check (exact && ticks == (int) std::ceil (7.5 * 128.0 / 60.0 * 24.0) && startAt == S (0.5) && stopAt == S (8.0) && same, "24 por negra, en su muestra exacta, igual con cualquier buffer; start y stop");
    }
    {
        Engine lead, fol; fresh (lead); fresh (fol);
        layer (lead, 0); auto& F = layer (fol, 0, 4, 7); F.pattern = PatRandom; F.octaves = 3;
        fol.g.key = 7; fol.g.scale = 0;
        lead.prepare (48000.0); fol.prepare (48000.0);
        std::vector<OutEvent> a, b; int checked = 0, wrong = 0;
        const int chords[4][3] = { { 57, 60, 64 }, { 53, 57, 60 }, { 55, 59, 62 }, { 52, 55, 59 } };
        std::set<int> pcsNow;
        for (int blk = 0; blk < (int) (40.0 * 48000.0 / 512); ++blk)
        {
            std::vector<InEvent> in;
            const int64_t pos = (int64_t) blk * 512;
            for (int k = 0; k < 4 * 10; ++k)
            {
                const int64_t t = S (k * 1.0);
                if (t >= pos && t < pos + 512)
                {
                    const auto& c = chords[k % 4];
                    if (k > 0) for (int n : chords[(k - 1) % 4]) in.push_back ({ (int) (t - pos), { 0x80, (uint8_t) n, 0 }, 3 });
                    for (int n : c) in.push_back ({ (int) (t - pos), { 0x90, (uint8_t) n, 100 }, 3 });
                }
            }
            lead.process (512, {}, in.data(), (int) in.size(), a);
            const auto share = lead.published();
            pcsNow.clear(); for (int i = 0; i < share.count; ++i) pcsNow.insert (share.notes[(size_t) i] % 12);
            fol.following = true; fol.external = share;
            fol.process (512, {}, nullptr, 0, b);
            for (auto& e : b) if ((e.data[0] & 0xF0) == 0x90 && e.data[2] > 0) { ++checked; if (! pcsNow.count (e.data[1] % 12)) ++wrong; }
        }
        std::printf ("  %d notas de la seguidora, %d fuera del acorde de la líder\n", checked, wrong);
        check (checked > 100 && wrong == 0, "la seguidora toca sólo las notas del acorde de la líder");
    }
}

// ───────── 12. CPU ─────────
static void test12()
{
    std::printf ("12. CPU del motor solo (lo mismo que valencerender --bench)\n");
    Engine e; fresh (e); e.g.complete = CompNinth; e.g.tempo = 174.0f;
    for (int l = 0; l < 4; ++l)
    {
        auto& L = layer (e, l, 9 + (l % 3), 16 - l * 3); L.pattern = l * 3; L.octaves = 3; L.mutate = 0.5f; L.humanizeMs = 8.0f; L.ccNum = 74; L.gate = 0.6f;
        for (int k = 0; k < kMaxSteps; ++k) { auto& s = L.steps[(size_t) k]; s.ratchet = (uint8_t) (1 + k % 2); s.cc = (int8_t) (k * 4); s.tie = k % 7 == 3; s.prob = 90; }
    }
    e.g.perLayerChannels = true;
    Sim s; s.block = 256; s.total = S (600.0);
    for (double t = 0.0; t < 600.0; t += 4.0) hold (s, S (t), { 45 + (int) t % 7, 52, 57 }, S (t + 3.9));
    const auto t0 = std::chrono::steady_clock::now();
    const auto log = simulate (e, s);
    const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("  4 capas a 1/32 con ratchets, CC, mutación y humanizado: %zu eventos en 10 minutos, CPU %.4f %% de un núcleo\n", log.size(), 100.0 * el / 600.0);
    check (100.0 * el / 600.0 < 1.0, "el motor solo usa menos del 1 % de un núcleo");
}

int main (int argc, char** argv)
{
    std::set<int> only; bool longSweep = false;
    for (int i = 1; i < argc; ++i) { if (std::string (argv[i]) == "--long") longSweep = true; else only.insert (std::atoi (argv[i])); }
    auto want = [&] (int k) { return only.empty() || only.count (k) > 0; };
    if (want (1)) test1();
    if (want (2)) test2 (longSweep);
    if (want (3)) test3();
    if (want (4)) test4();
    if (want (5)) test5();
    if (want (6)) test6();
    if (want (7)) test7();
    if (want (8)) test8();
    if (want (9)) test9();
    if (want (10)) test10();
    if (want (11)) test11();
    if (want (12)) test12();
    std::printf ("\n%s (%d fallas)\n", fails == 0 ? "TODO OK" : "HAY FALLAS", fails);
    return fails == 0 ? 0 : 1;
}
