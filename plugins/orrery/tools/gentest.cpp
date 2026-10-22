// Pruebas del generador de Orrery sin JUCE:
//   clang++ -std=c++17 -O2 -ffp-contract=off -I plugins/orrery/Source/gen plugins/orrery/tools/gentest.cpp -o /tmp/orr_gentest && /tmp/orr_gentest
//   cl /std:c++17 /O2 /fp:precise /EHsc /I plugins\orrery\Source\gen plugins\orrery\tools\gentest.cpp   (Windows)
// La "huella" de la prueba 8 tiene que dar igual en macOS y en Windows.
// Pruebas 9 a 15: Main y satélites (Constellation.h). En Linux, compilar con -pthread.
#include "Generator.h"
#include "Constellation.h"
#include <condition_variable>
#include <cstdio>
#include <map>
#include <random>
#include <set>
#include <thread>

using namespace dh::orr;

static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what); if (! ok) ++fails; }

struct Log { double sec; uint8_t s, d1, d2; int lane; };

static std::vector<Log> run (Generator& g, double seconds, int block = 512, double fs = 48000.0)
{
    std::vector<Log> log; std::vector<OutEvent> ev;
    g.prepare (fs);
    const int blocks = (int) (seconds * fs / block);
    for (int b = 0; b < blocks; ++b)
    {
        g.process (block, false, 0.0, 120.0, ev);
        for (auto& e : ev) log.push_back ({ (b * block + e.offset) / fs, e.data[0], e.data[1], e.data[2], e.lane });
    }
    g.g.run = false;
    g.process (block, false, 0.0, 120.0, ev);
    for (auto& e : ev) log.push_back ({ (blocks * block + e.offset) / fs, e.data[0], e.data[1], e.data[2], e.lane });
    return log;
}

static void setupAmbient (Generator& g, uint32_t seed)
{
    g.g = GlobalParams{}; g.g.run = true; g.g.hostClock = false; g.g.tempo = 60.0f; g.g.seed = seed; g.g.tension = 0.0f; g.g.fadeIn = 0.0f;
    const int roles[7] = { Drone, Pad, Melody, Arp, Cloud, Bass, Pulse };
    for (int l = 0; l < kLanes; ++l) { g.lanes[(size_t) l] = LaneParams{}; }
    for (int l = 0; l < 7; ++l)
    {
        auto& L = g.lanes[(size_t) l]; L.on = true; L.role = roles[l]; L.channel = l + 1; L.follow = l == 2 ? FollowScale : FollowChord;
        L.euclidPulses = l == 6 ? 3 : 0; L.euclidSteps = 8;
    }
    g.lanes[4].echoes = 3;
    g.lanes[3].loop = true; g.lanes[3].loopBeats = 17.0f;
    g.lanes[2].ccNum = 74; g.lanes[2].ccShape = CcDrift;
    g.lanes[1].driftCents = 8.0f;
}

// ───────── Main y satélites ─────────
namespace sat {

struct E { int64_t t; uint8_t s, d1, d2; int lane; };

// Los ajustes que el Main va leyendo: arranca parado, toca, cambia ajustes, carga otro "preset", para, vuelve, pánico.
struct Schedule
{
    Controls base, changed, preset2;
    bool hostClock = false;
    Schedule (bool host)
    {
        hostClock = host;
        Generator a; setupAmbient (a, 11);
        a.g.breathRate = 6.0f; a.g.fadeIn = 8.0f; a.g.humanizeMs = 12.0f; a.g.hostClock = host; a.g.runWhenStopped = true;
        a.lanes[4].loop = true; a.lanes[4].loopBeats = 23.0f; a.lanes[5].ccNum = 1; a.lanes[6].ccNum = 11; a.lanes[6].ccShape = CcBreath;
        base.g = a.g; base.lanes = a.lanes;
        changed = base; changed.g.density = 0.8f; changed.g.motion = 0.7f; changed.lanes[7].on = true; changed.lanes[7].role = Arp; changed.lanes[7].channel = 8; changed.lanes[0].driftCents = 20.0f;
        Generator b; setupAmbient (b, 4321); b.g.key = 7; b.g.scale = 0; b.g.tempo = 72.0f; b.g.hostClock = host; b.g.fadeIn = 3.0f;
        b.lanes[1].ccNum = 74; b.lanes[1].ccShape = CcSwell; b.lanes[3].echoes = 4;
        preset2.g = b.g; preset2.lanes = b.lanes;
    }
    Controls at (double sec, double prevSec, double total) const
    {
        Controls c = sec < 20.0 ? base : (sec < 40.0 ? changed : preset2);
        c.g.run = ((sec >= 1.0 && sec < 55.0) || sec >= 58.0) && sec < total - 3.0;
        c.reset = prevSec < 40.0 && sec >= 40.0;
        c.panic = prevSec < 70.0 && sec >= 70.0;
        return c;
    }
};

struct Inst
{
    bool isMain = false;
    std::unique_ptr<SatelliteCore> core;
    uint32_t mask = 0;
    double joinSec = 0.0;
    int64_t syncT = -1;
    std::vector<E> log;
    std::vector<OutEvent> out;
};

struct Config
{
    int block = 512; bool hostClock = false; bool keyed = true; double jitter = 0.0; bool threads = false; uint32_t seed = 1;
    double secs = 90.0, fs = 48000.0;
    double stallFrom = -1.0, stallTo = -1.0;      // el Main no procesa (bypass, pista congelada)
    std::vector<std::pair<uint32_t, double>> sats;   // carriles y hora de alta de cada satélite
    double laneChangeAt = -1.0;                      // el primer satélite pasa a otro carril
    bool fixedOrder = false;                         // siempre el Main primero (un solo hilo, siempre el mismo orden)
};

struct Result { std::vector<Inst> inst; int resyncs = 0; };

// Un ciclo del host para una instancia: el mismo playhead para todas, la marca de hora según el orden de proceso.
struct Cycle { int64_t start; int n; bool playing; double ppq, bpm; double ns; double sec, prevSec; };

class Barrier
{
public:
    explicit Barrier (int n) : count (n), waiting (0), phase (0) {}
    void wait()
    {
        std::unique_lock<std::mutex> lk (m);
        const int ph = phase;
        if (++waiting == count) { waiting = 0; ++phase; cv.notify_all(); return; }
        cv.wait (lk, [&] { return phase != ph; });
    }
private:
    std::mutex m; std::condition_variable cv; int count, waiting, phase;
};

static Result simulate (const Config& cfg)
{
    Schedule sch (cfg.hostClock);
    auto& reg = Constellation::get();
    const int mainSlot = reg.join();
    { std::lock_guard<std::mutex> lk (reg.lock); reg.setRoleLocked (mainSlot, RoleMain, 0); }
    MainBus* bus = reg.members[(size_t) mainSlot].bus.load();
    Generator gen; gen.prepare (cfg.fs);
    MainCore mc; mc.attach (bus);
    Result res;
    res.inst.resize (1 + cfg.sats.size());
    res.inst[0].isMain = true;
    for (size_t i = 0; i < cfg.sats.size(); ++i)
    {
        auto& s = res.inst[i + 1];
        s.core = std::make_unique<SatelliteCore>(); s.mask = cfg.sats[i].first; s.joinSec = cfg.sats[i].second;
        s.out.reserve (8192);
    }
    std::vector<OutEvent> mev; mev.reserve (8192);
    const int64_t total = (int64_t) (cfg.secs * cfg.fs);
    std::mt19937 rng (cfg.seed);
    const double bpm = 90.0;

    auto stepMain = [&] (const Cycle& c) {
        if (c.sec >= cfg.stallFrom && c.sec < cfg.stallTo) return;
        Controls raw = sch.at (c.sec, c.prevSec, cfg.secs);
        const Controls& cur = mc.begin (raw);
        gen.g = cur.g; gen.lanes = cur.lanes;
        if (cur.reset) gen.resetRequest.store (true);
        if (cur.panic) gen.panic (mev); else gen.process (c.n, c.playing, c.ppq, c.bpm, mev);
        Stamp st; st.sampleValid = cfg.keyed && c.playing; st.hostSample = c.start; st.ns = c.ns;
        mc.end (c.n, st, mev, gen);
        for (auto& e : mev) res.inst[0].log.push_back ({ c.start + e.offset, e.data[0], e.data[1], e.data[2], e.lane });
    };
    auto stepSat = [&] (Inst& s, const Cycle& c) {
        if (c.sec < s.joinSec) return;
        uint32_t m = s.mask;
        if (cfg.laneChangeAt > 0.0 && &s == &res.inst[1] && c.sec >= cfg.laneChangeAt) m = (s.mask << 1) & 0xFFu;
        Stamp st; st.sampleValid = cfg.keyed && c.playing; st.hostSample = c.start; st.ns = c.ns;
        s.core->process (bus, c.n, cfg.fs, st, c.playing, c.ppq, c.bpm, m, s.out);
        if (s.syncT < 0 && s.core->isSynced()) s.syncT = c.start;
        for (auto& e : s.out) s.log.push_back ({ c.start + e.offset, e.data[0], e.data[1], e.data[2], e.lane });
    };

    const int N = (int) res.inst.size();
    auto cycleAt = [&] (int64_t start, int n) {
        Cycle c; c.start = start; c.n = n;
        c.sec = (double) start / cfg.fs; c.prevSec = (double) (start - n) / cfg.fs;
        c.playing = cfg.hostClock;
        double beat = (double) start / cfg.fs * bpm / 60.0;
        if (c.sec >= 50.0) beat += 16.0;       // salto del transporte
        c.ppq = beat; c.bpm = bpm;
        c.ns = 1.0e9 * (double) start / cfg.fs + 123456789.0;
        return c;
    };
    if (! cfg.threads)
    {
        std::vector<int> order (N);
        for (int64_t pos = 0; pos < total; pos += cfg.block)
        {
            const int n = (int) std::min<int64_t> (cfg.block, total - pos);
            const Cycle c0 = cycleAt (pos, n);
            for (int i = 0; i < N; ++i) order[(size_t) i] = i;
            if (! cfg.fixedOrder) std::shuffle (order.begin(), order.end(), rng);
            for (int r = 0; r < N; ++r)
            {
                Cycle c = c0;
                // La hora de cada pista dentro del ciclo: más tarde cuanto más tarde se procesa.
                c.ns += cfg.jitter * (double) n / cfg.fs * 1.0e9 * (double) r / (double) std::max (1, N - 1);
                const int i = order[(size_t) r];
                if (res.inst[(size_t) i].isMain) stepMain (c); else stepSat (res.inst[(size_t) i], c);
            }
        }
    }
    else
    {
        Barrier start (N), done (N);
        std::vector<std::thread> th;
        for (int i = 0; i < N; ++i)
            th.emplace_back ([&, i] {
                std::mt19937 r2 (cfg.seed * 977u + (uint32_t) i);
                for (int64_t pos = 0; pos < total; pos += cfg.block)
                {
                    const int n = (int) std::min<int64_t> (cfg.block, total - pos);
                    const Cycle c = cycleAt (pos, n);
                    start.wait();
                    // Un poco de espera al azar para mezclar el orden (sin sleep: en Windows duerme 15 ms).
                    if (r2() % 3 == 0) { volatile uint32_t spin = r2() % 20000; while (spin > 0) spin = spin - 1; }
                    if (res.inst[(size_t) i].isMain) stepMain (c); else stepSat (res.inst[(size_t) i], c);
                    done.wait();
                }
            });
        for (auto& t : th) t.join();
    }
    for (auto& s : res.inst) if (s.core) res.resyncs += s.core->resyncs.load();
    reg.leave (mainSlot);
    return res;
}

// Lo del Main para esos carriles (y los mensajes de todos los canales en sus canales), desde "from".
static std::vector<E> laneEvents (const std::vector<E>& log, uint32_t mask, int64_t from, int64_t to = INT64_MAX)
{
    std::vector<E> v;
    for (auto& e : log)
    {
        if (e.t < from || e.t >= to) continue;
        bool ok = e.lane >= 0 ? ((mask >> e.lane) & 1u) != 0 : false;
        if (e.lane < 0) for (int l = 0; l < kLanes; ++l) if (((mask >> l) & 1u) && (e.s & 15) == l) ok = true;     // canales 1..8 = carriles
        if (ok) v.push_back (e);
    }
    return v;
}
static bool sameEvents (const std::vector<E>& a, const std::vector<E>& b, int& matched)
{
    matched = 0;
    for (size_t i = 0; i < std::min (a.size(), b.size()); ++i)
        if (a[i].t == b[i].t && a[i].s == b[i].s && a[i].d1 == b[i].d1 && a[i].d2 == b[i].d2 && a[i].lane == b[i].lane) ++matched;
    return a.size() == b.size() && matched == (int) a.size();
}
static int hanging (const std::vector<E>& log)
{
    std::map<int, int> on;
    for (auto& e : log)
    {
        const int st = e.s & 0xF0, k = (e.s & 15) * 128 + e.d1;
        if (st == 0x90 && e.d2 > 0) on[k] = 1;
        else if (st == 0x80 || (st == 0x90 && e.d2 == 0)) on[k] = 0;
        else if (st == 0xB0 && (e.d1 == 123 || e.d1 == 120)) for (int n = 0; n < 128; ++n) on[(e.s & 15) * 128 + n] = 0;
    }
    int h = 0; for (auto& kv : on) h += kv.second;
    return h;
}

} // namespace sat

static void constellationTests()
{
    using namespace sat;
    std::printf ("9. Copia del estado: una generadora copiada a mitad de camino sigue igual\n");
    {
        Generator a, b; setupAmbient (a, 11); a.g.breathRate = 6.0f; a.g.humanizeMs = 12.0f; a.g.fadeIn = 10.0f;
        a.lanes[4].loop = true; a.lanes[4].loopBeats = 23.0f; a.lanes[2].ccNum = 74; a.lanes[2].ccShape = CcSwell;
        a.prepare (48000.0); b.prepare (48000.0);
        std::vector<OutEvent> ea, eb; int same = 0, total = 0;
        for (int k = 0; k < (int) (120.0 * 48000.0 / 512); ++k)
        {
            if (k == (int) (37.0 * 48000.0 / 512)) b.copyStateFrom (a);
            a.process (512, false, 0.0, 120.0, ea);
            if (k < (int) (37.0 * 48000.0 / 512)) continue;
            b.process (512, false, 0.0, 120.0, eb);
            total += (int) ea.size();
            if (ea.size() == eb.size()) for (size_t i = 0; i < ea.size(); ++i) same += ea[i].offset == eb[i].offset && std::memcmp (ea[i].data, eb[i].data, 3) == 0 && ea[i].lane == eb[i].lane;
        }
        std::printf ("  %d de %d mensajes iguales en 83 s\n", same, total);
        check (total > 500 && same == total, "la copia genera exactamente lo mismo");
    }
    std::printf ("10. Roles en Auto y carriles tomados\n");
    {
        auto& reg = Constellation::get();
        int ids[4]; int roles[4]; uint32_t lanes[4];
        for (int i = 0; i < 4; ++i)
        {
            ids[i] = reg.join();
            std::lock_guard<std::mutex> lk (reg.lock);
            roles[i] = reg.resolveAutoLocked (ids[i]);
            lanes[i] = roles[i] == RoleSatellite ? reg.pickLanesLocked (ids[i]) : 0u;
            reg.setRoleLocked (ids[i], roles[i], lanes[i]);
            if (roles[i] == RoleMain) reg.members[(size_t) ids[i]].laneOn.store (0x37u);    // carriles 1, 2, 3, 5 y 6 encendidos
        }
        std::printf ("  roles %d %d %d %d, carriles %x %x %x\n", roles[0], roles[1], roles[2], roles[3], lanes[1], lanes[2], lanes[3]);
        check (roles[0] == RoleMain && roles[1] == RoleSatellite && roles[2] == RoleSatellite && roles[3] == RoleSatellite, "la primera es Main y las siguientes satélites");
        check (lanes[1] == 0x1u && lanes[2] == 0x2u && lanes[3] == 0x4u, "cada satélite toma el próximo carril libre");
        // Un satélite que se va libera su carril; el que llega lo toma. Los carriles encendidos van primero.
        reg.leave (ids[2]);
        int e; uint32_t el;
        { e = reg.join(); std::lock_guard<std::mutex> lk (reg.lock); el = reg.pickLanesLocked (e); reg.setRoleLocked (e, RoleSatellite, el); }
        check (el == 0x2u, "un carril liberado se vuelve a tomar");
        int f; uint32_t fl;
        { f = reg.join(); std::lock_guard<std::mutex> lk (reg.lock); fl = reg.pickLanesLocked (f); reg.setRoleLocked (f, RoleSatellite, fl); }
        check (fl == 0x10u, "salta los carriles apagados del Main (el 4) y toma el 5");
        MainBus* b = reg.members[(size_t) ids[0]].bus.load();
        for (int i : { ids[1], ids[3], e, f }) { reg.members[(size_t) i].follows.store (b); reg.members[(size_t) i].seenMs.store (1000.0); }
        reg.members[(size_t) f].seenMs.store (-5000.0);
        check (reg.takenLanes (b, 1100.0) == 0x7u, "el Main ve los carriles de los satélites vivos (no los del que dejó de procesar)");
        check (reg.mainBus() == b, "los satélites siguen al Main");
        { std::lock_guard<std::mutex> lk (reg.lock); check (reg.resolveAutoLocked (ids[0]) == RoleMain, "sin otro Main, una nueva en Auto sería Main"); }
        for (int i : { ids[0], ids[1], ids[3], e, f }) reg.leave (i);
        check (reg.mainBus() == nullptr, "sin instancias no queda Main");
    }
    std::printf ("11. Satélites iguales al Main, un hilo, orden de proceso al azar en cada ciclo\n");
    {
        bool all = true, heavy = true;
        static const char* modeName[4] = { "host en marcha", "propio, hora del host", "propio, hora de la máquina", "propio, máquina muy cargada" };
        for (int mode = 0; mode < 4; ++mode)
            for (int block : { 64, 333, 512, 1024 })
            {
                Config cfg; cfg.block = block; cfg.seed = (uint32_t) (block * 7 + mode);
                cfg.hostClock = mode == 0;          // 0: reloj del host en marcha; 1 a 3: reloj propio con el host parado
                // 1: el host da la hora de cada ciclo (la misma para todas las pistas); 2 y 3: no la da y cada pista
                // anota la de la máquina al procesar, hasta 35 % o 100 % de un bloque después que la primera.
                cfg.jitter = mode == 2 ? 0.35 : (mode == 3 ? 1.0 : 0.0);
                cfg.sats = { { 0x1u, 0.0 }, { 0x4u, 0.0 }, { 0x30u, 0.0 } };
                const auto r = simulate (cfg);
                int okSats = 0, okLater = 0;
                std::printf ("  %-27s bloque %4d:", modeName[mode], block);
                for (size_t i = 1; i < r.inst.size(); ++i)
                {
                    const auto& s = r.inst[i];
                    const auto want = laneEvents (r.inst[0].log, s.mask, 0);
                    int m = 0, m2 = 0; const bool same = sameEvents (want, s.log, m);
                    okSats += same && s.syncT >= 0 && s.syncT < (int64_t) (0.2 * cfg.fs) && hanging (s.log) == 0;
                    okLater += sameEvents (laneEvents (r.inst[0].log, s.mask, (int64_t) (5.0 * cfg.fs)), laneEvents (s.log, 0xFFu, (int64_t) (5.0 * cfg.fs)), m2) && hanging (s.log) == 0;
                    std::printf (" %d/%d", m, (int) want.size());
                }
                std::printf (", resincronizaciones %d\n", r.resyncs);
                if (mode < 3) all = all && okSats == (int) cfg.sats.size() && r.resyncs == 0;
                else heavy = heavy && okLater == (int) cfg.sats.size();
            }
        check (all, "cada satélite saca exactamente los eventos del Main para su carril (notas, velocidades, muestras, CC, bend)");
        check (heavy, "sin hora del host y con la máquina muy cargada se corrige solo (iguales desde los 5 s, sin notas colgadas)");
    }
    std::printf ("12. Satélites en hilos propios (como Live con varios núcleos)\n");
    {
        bool all = true;
        for (int mode = 0; mode < 2; ++mode)
            for (int block : { 128, 512 })
            {
                Config cfg; cfg.block = block; cfg.threads = true; cfg.hostClock = mode == 0; cfg.secs = 80.0; cfg.seed = 99u + (uint32_t) block;
                cfg.sats = { { 0x1u, 0.0 }, { 0x2u, 0.0 }, { 0x4u, 0.0 }, { 0x30u, 0.0 } };
                const auto r = simulate (cfg);
                int ok = 0, total = 0;
                for (size_t i = 1; i < r.inst.size(); ++i)
                {
                    const auto want = laneEvents (r.inst[0].log, r.inst[i].mask, 0);
                    int m = 0; ok += sameEvents (want, r.inst[i].log, m) && hanging (r.inst[i].log) == 0; total += (int) want.size();
                }
                std::printf ("  %s, bloque %d: %d de %d satélites iguales (%d eventos), resincronizaciones %d\n",
                             mode == 0 ? "host en marcha" : "reloj propio", block, ok, (int) cfg.sats.size(), total, r.resyncs);
                all = all && ok == (int) cfg.sats.size() && r.resyncs == 0;
            }
        check (all, "iguales con cada instancia en su hilo");
    }
    std::printf ("13. Un satélite que se suma tocando, y otro que cambia de carril\n");
    {
        Config cfg; cfg.block = 256; cfg.secs = 60.0;
        cfg.sats = { { 0x2u, 0.0 }, { 0x1u, 30.0 } };
        cfg.laneChangeAt = 25.0;            // el primero pasa del carril 2 al 3
        const auto r = simulate (cfg);
        const auto& late = r.inst[2];
        const int64_t from = late.syncT + cfg.block;
        int m = 0; const bool same = from > 0 && sameEvents (laneEvents (r.inst[0].log, 0x1u, from), laneEvents (late.log, 0xFFu, from), m);
        int catchUp = 0; for (auto& e : late.log) if (e.t == late.syncT && (e.s & 0xF0) == 0x90) ++catchUp;
        std::printf ("  se sumó a los %.2f s, atacó %d notas que sonaban, después %d eventos iguales\n", (double) late.syncT / cfg.fs, catchUp, m);
        check (same && late.syncT < (int64_t) (30.2 * cfg.fs) && hanging (late.log) == 0, "se sincroniza en marcha y desde ahí es igual");
        const auto& mover = r.inst[1];
        const int64_t at = (int64_t) (25.0 * cfg.fs);
        int m1 = 0, m2 = 0;
        const bool before = sameEvents (laneEvents (r.inst[0].log, 0x2u, 0, at - cfg.block), laneEvents (mover.log, 0xFFu, 0, at - cfg.block), m1);
        const bool after = sameEvents (laneEvents (r.inst[0].log, 0x4u, at + cfg.block), laneEvents (mover.log, 0xFFu, at + cfg.block), m2);
        std::printf ("  cambio de carril: %d eventos antes, %d después, %d notas colgadas\n", m1, m2, hanging (mover.log));
        check (before && after && hanging (mover.log) == 0, "al cambiar de carril suelta el viejo y sigue igual en el nuevo");
    }
    std::printf ("14. Sin hora del host ni transporte, el satélite siempre 0,7 bloques después del Main\n");
    {
        // La hora de la máquina no distingue "0,7 bloques después en este ciclo" de "0,3 antes del próximo":
        // el satélite elige lo más cercano. Tiene que quedar estable (sin resincronizar en bucle ni colgar notas).
        Config cfg; cfg.block = 512; cfg.fixedOrder = true; cfg.jitter = 0.7; cfg.secs = 60.0;
        cfg.sats = { { 0x1u, 0.0 } };
        const auto r = simulate (cfg);
        const auto want = laneEvents (r.inst[0].log, 0x1u, 0);
        auto later = r.inst[1].log; for (auto& e : later) e.t -= cfg.block;
        auto sooner = r.inst[1].log; for (auto& e : sooner) e.t += cfg.block;
        int m = 0, ms = 0, ms2 = 0; const bool exact = sameEvents (want, r.inst[1].log, m);
        const bool early = sameEvents (want, later, ms) || sameEvents (want, sooner, ms2);
        std::printf ("  %s, resincronizaciones %d, notas colgadas %d\n", exact ? "exacto" : (early ? "un bloque corrido" : "distinto"), r.resyncs, hanging (r.inst[1].log));
        check ((exact || early) && r.resyncs <= 2 && hanging (r.inst[1].log) == 0, "estable: exacto o un bloque corrido, sin bucles");
        cfg.hostClock = true; cfg.keyed = true;
        const auto r2 = simulate (cfg);
        int m2 = 0; const bool exact2 = sameEvents (laneEvents (r2.inst[0].log, 0x1u, 0), r2.inst[1].log, m2);
        check (exact2, "con el transporte del host en marcha, exacto aunque el orden y la hora engañen");
    }
    std::printf ("15. El Main deja de procesar 3 s (bypass, pista congelada)\n");
    {
        Config cfg; cfg.block = 512; cfg.secs = 50.0; cfg.stallFrom = 20.0; cfg.stallTo = 23.0;
        cfg.sats = { { 0x1u, 0.0 }, { 0x2u, 0.0 } };
        const auto r = simulate (cfg);
        int okAfter = 0, h = 0;
        for (size_t i = 1; i < r.inst.size(); ++i)
        {
            h += hanging (r.inst[i].log);
            // Desde el primer bloque después de volver: las notas que el Main ataca a partir de ahí, iguales.
            const int64_t from = (int64_t) (24.0 * cfg.fs);
            int m = 0; okAfter += sameEvents (laneEvents (r.inst[0].log, r.inst[i].mask, from), laneEvents (r.inst[i].log, 0xFFu, from), m);
        }
        std::printf ("  resincronizaciones %d, notas colgadas %d, %d de 2 iguales al volver\n", r.resyncs, h, okAfter);
        check (r.resyncs >= 2 && h == 0 && okAfter == 2, "sueltan las notas, esperan y se vuelven a sincronizar");
    }
}

int main()
{
    std::printf ("1. Diez minutos de ambient (7 carriles, Re dórico)\n");
    {
        Generator g; setupAmbient (g, 42);
        const auto log = run (g, 600.0);
        const auto sc = scalePcs (scales()[2], 2);
        int ons = 0, outOfScale = 0, maxPoly = 0, maxRate = 0;
        std::map<int, std::set<int>> on;          // canal → notas sonando
        std::map<std::pair<int, int>, int> perSec; // (canal, segundo) → mensajes
        std::map<int, int> perLane;
        for (auto& e : log)
        {
            const int ch = e.s & 15, st = e.s & 0xF0;
            perSec[{ ch, (int) e.sec }]++;
            if (st == 0x90 && e.d2 > 0) { ++ons; perLane[e.lane]++; if (std::find (sc.begin(), sc.end(), e.d1 % 12) == sc.end()) { if (outOfScale++ < 5) std::printf ("  fuera: carril %d nota %d en %.2f s\n", e.lane, e.d1, e.sec); } on[ch].insert (e.d1); maxPoly = std::max (maxPoly, (int) on[ch].size()); }
            else if (st == 0x80 || (st == 0x90 && e.d2 == 0)) on[ch].erase (e.d1);
        }
        for (auto& kv : perSec) maxRate = std::max (maxRate, kv.second);
        int stuck = 0; for (auto& kv : on) stuck += (int) kv.second.size();
        std::printf ("  %d notas; por carril:", ons); for (auto& kv : perLane) std::printf (" %s %d", roleName (g.lanes[(size_t) kv.first].role), kv.second); std::printf ("\n");
        std::printf ("  polifonía máxima por canal %d, mensajes por segundo por canal (máx) %d\n", maxPoly, maxRate);
        check (outOfScale == 0, "con tensión 0 ninguna nota sale de la escala");
        check (stuck == 0, "al parar no queda ninguna nota colgada");
        check (maxRate < 120, "tráfico apto para MIDI DIN (menos de 120 mensajes por segundo por canal)");
        check (perLane.size() == 7, "los 7 roles suenan");
    }
    std::printf ("2. Semilla\n");
    {
        Generator a, b, c; setupAmbient (a, 7); setupAmbient (b, 7); setupAmbient (c, 8);
        const auto la = run (a, 60.0), lb = run (b, 60.0), lc = run (c, 60.0);
        bool same = la.size() == lb.size();
        for (size_t i = 0; same && i < la.size(); ++i) same = la[i].s == lb[i].s && la[i].d1 == lb[i].d1 && std::abs (la[i].sec - lb[i].sec) < 1e-9;
        check (same, "la misma semilla da exactamente el mismo resultado");
        check (la.size() != lc.size() || la[10].d1 != lc[10].d1 || la[20].d1 != lc[20].d1, "otra semilla da otro resultado");
    }
    std::printf ("3. Lazo de 17 pulsos (Eno)\n");
    {
        Generator g; setupAmbient (g, 3);
        for (int l = 0; l < kLanes; ++l) if (l != 3) g.lanes[(size_t) l].on = false;
        g.lanes[3].mutate = 0.0f; g.lanes[3].probability = 1.0f; g.g.humanizeMs = 0.0f; g.g.progression = 1; g.g.color = 0.0f; g.lanes[3].follow = FollowScale;
        const auto log = run (g, 17.0 * 6);
        std::vector<std::pair<double, int>> ons;
        for (auto& e : log) if ((e.s & 0xF0) == 0x90 && e.d2 > 0) ons.push_back ({ e.sec, e.d1 });
        int matched = 0, total = 0;
        for (auto& [t, n] : ons)
        {
            if (t < 17.0 || t >= 17.0 * 5) continue;
            ++total;
            for (auto& [t2, n2] : ons) if (std::abs (t2 - (t + 17.0)) < 0.02 && n2 == n) { ++matched; break; }
        }
        std::printf ("  %d de %d notas se repiten 17 s después\n", matched, total);
        check (total > 10 && matched >= total * 9 / 10, "el lazo se repite con su período");
    }
    std::printf ("4. Sesión de 1 minuto con fades\n");
    {
        Generator g; setupAmbient (g, 5); g.g.sessionMinutes = 1.0f; g.g.fadeIn = 10.0f; g.g.fadeOut = 15.0f;
        const auto log = run (g, 80.0);
        double lastOn = 0; int earlyLoud = 0, cc7Min = 127;
        std::map<int, std::set<int>> on;
        for (auto& e : log)
        {
            const int st = e.s & 0xF0;
            if (st == 0x90 && e.d2 > 0) { lastOn = e.sec; if (e.sec < 2.0 && e.d2 > 70) ++earlyLoud; on[e.s & 15].insert (e.d1); }
            else if (st == 0x80) on[e.s & 15].erase (e.d1);
            else if (st == 0xB0 && e.d1 == 7 && e.sec > 55.0) cc7Min = std::min (cc7Min, (int) e.d2);
        }
        int stuck = 0; for (auto& kv : on) stuck += (int) kv.second.size();
        std::printf ("  última nota a los %.1f s; CC7 al final %d\n", lastOn, cc7Min);
        check (lastOn <= 60.5 && stuck == 0, "termina al minuto y suelta todo");
        check (earlyLoud == 0 && cc7Min < 20, "entra y sale en fade");
    }
    std::printf ("5. Modo suave\n");
    {
        Generator g; setupAmbient (g, 9); g.g.gentle = true; g.g.brightness = 1.0f; g.g.density = 1.0f;
        for (auto& L : g.lanes) L.velocity = 1.0f;
        const auto log = run (g, 120.0);
        int maxV = 0, maxN = 0;
        for (auto& e : log) if ((e.s & 0xF0) == 0x90 && e.d2 > 0 && e.lane != 4) { maxV = std::max (maxV, (int) e.d2); maxN = std::max (maxN, (int) e.d1); }
        std::printf ("  velocidad máxima %d, nota más aguda %d\n", maxV, maxN);
        check (maxV <= 92 && maxN <= 88, "velocidades y registro acotados");
    }
    std::printf ("6. Respiración: los acordes cambian en respiraciones enteras\n");
    {
        Generator g; setupAmbient (g, 11); g.g.breathRate = 6.0f;   // 10 s por respiración
        for (int l = 0; l < kLanes; ++l) if (l != 1) g.lanes[(size_t) l].on = false;
        g.g.humanizeMs = 0.0f; g.lanes[1].length = 0.0f;
        const auto log = run (g, 120.0);
        std::set<int> starts;
        for (auto& e : log) if ((e.s & 0xF0) == 0x90 && e.d2 > 0) starts.insert ((int) std::lround (e.sec * 10.0));
        int off = 0, groups = 0; int prev = -1000;
        for (int t : starts) { if (t - prev > 20) { ++groups; const double s = t / 10.0; if (std::abs (s / 10.0 - std::round (s / 10.0)) > 0.05) ++off; } prev = t; }
        std::printf ("  %d entradas de pad, %d fuera de la respiración\n", groups, off);
        check (groups >= 3 && off == 0, "el pad entra al comienzo de cada ciclo de respiración");
    }
    std::printf ("7. Vínculo: una seguidora toca sobre los acordes de la líder\n");
    {
        Generator lead, fol; setupAmbient (lead, 21); setupAmbient (fol, 99);
        for (int l = 0; l < kLanes; ++l) { lead.lanes[(size_t) l].on = l == 1; fol.lanes[(size_t) l].on = l == 1; }
        fol.lanes[1].channel = 9; fol.g.key = 7; fol.g.scale = 0;      // otra tonalidad propia: debe ignorarla
        lead.g.humanizeMs = fol.g.humanizeMs = 0.0f;
        lead.prepare (48000.0); fol.prepare (48000.0);
        std::vector<OutEvent> ea, eb; int checked = 0, wrong = 0;
        for (int b = 0; b < (int) (240.0 * 48000.0 / 512); ++b)
        {
            lead.process (512, false, 0.0, 120.0, ea);
            const auto h = lead.published();
            fol.following = true; fol.external = h; fol.g.key = h.key; fol.g.scale = h.scale;
            fol.process (512, false, 0.0, 120.0, eb);
            for (auto& e : eb) if ((e.data[0] & 0xF0) == 0x90 && e.data[2] > 0) { ++checked; if (! ((h.mask >> (e.data[1] % 12)) & 1)) ++wrong; }
        }
        std::printf ("  %d notas de la seguidora, %d fuera del acorde de la líder\n", checked, wrong);
        check (checked > 20 && wrong == 0, "la seguidora usa los acordes de la líder");
    }
    std::printf ("8. La misma variación con otro host: tamaño de bloque y frecuencia de muestreo\n");
    {
        auto setup = [] (Generator& g) {
            setupAmbient (g, 11);
            g.g.breathRate = 6.0f; g.g.fadeIn = 20.0f; g.g.humanizeMs = 12.0f;
            g.lanes[4].loop = true; g.lanes[4].loopBeats = 23.0f; g.lanes[2].loop = true; g.lanes[2].loopBeats = 19.0f;
            g.lanes[5].ccNum = 1; g.lanes[5].ccShape = CcSine; g.lanes[6].ccNum = 11; g.lanes[6].ccShape = CcBreath;
        };
        const double secs = 180.0;
        Generator ref; setup (ref); auto a = run (ref, secs, 512, 48000.0);
        uint64_t h = 1469598103934665603ull; int notes = 0;
        for (auto& e : a) if (e.sec < secs - 1.0)
        {
            for (uint64_t v : { (uint64_t) e.s, (uint64_t) e.d1, (uint64_t) e.d2, (uint64_t) (e.lane + 1), (uint64_t) std::llround (e.sec * 48000.0) }) { h ^= v; h *= 1099511628211ull; }
            notes += (e.s & 0xF0) == 0x90;
        }
        std::printf ("  huella %016llx (%d notas en 3 minutos)\n", (unsigned long long) h, notes);
        bool allSame = true;
        for (auto cfg : { std::pair<int, double> { 64, 48000.0 }, { 441, 48000.0 }, { 1024, 48000.0 }, { 512, 44100.0 }, { 333, 96000.0 } })
        {
            Generator g; setup (g); auto b = run (g, secs, cfg.first, cfg.second);
            std::vector<bool> used (b.size(), false); int matched = 0, total = 0;
            size_t from = 0;
            for (auto& x : a)
            {
                if (x.sec >= secs - 1.0) continue;
                ++total;
                while (from < b.size() && b[from].sec < x.sec - 0.002) ++from;
                for (size_t j = from; j < b.size() && b[j].sec <= x.sec + 0.002; ++j)
                    if (! used[j] && b[j].s == x.s && b[j].d1 == x.d1 && b[j].d2 == x.d2 && b[j].lane == x.lane) { used[j] = true; ++matched; break; }
            }
            std::printf ("  bloque %4d a %5.0f Hz: %d de %d mensajes iguales (±2 ms)\n", cfg.first, cfg.second, matched, total);
            allSame = allSame && matched == total;
        }
        check (allSame, "mismas notas y controles con cualquier bloque y frecuencia de muestreo");
    }

    constellationTests();
    std::printf ("\n%s (%d fallas)\n", fails == 0 ? "TODO OK" : "HAY FALLAS", fails);
    return fails == 0 ? 0 : 1;
}
