#pragma once
// Main y satélites: varias Orrery del mismo proceso tocan como una sola. Sin JUCE (lo prueba gentest).
//
// Un satélite no recibe notas del Main: corre su propia réplica de la generadora, con todo el estado musical
// del Main, y saca sólo sus carriles. Como la generadora es determinista, la réplica genera lo mismo, a la
// misma muestra, siempre que reciba lo mismo en los mismos bloques:
//
//   - El transporte del host (reproduciendo, ppq, tempo) lo lee cada instancia de su propio playhead: en un
//     ciclo del host todas las pistas ven el mismo.
//   - Los controles (ajustes, Play, preset nuevo, pánico) los aplica el Main un bloque después de leerlos y
//     los publica para ese bloque. En un ciclo k el Main ya terminó el ciclo k-1, procese antes o después que
//     el satélite y en el hilo que sea: el satélite siempre encuentra los controles del bloque k.
//   - Base de tiempo común: cada instancia cuenta sus bloques y sus muestras. Al sincronizarse, el satélite
//     copia el estado del Main en un borde de bloque y fija, una vez, qué bloque suyo es qué bloque del Main.
//     Para saber si el Main ya procesó el ciclo actual compara marcas del ciclo: la posición del host en
//     muestras (con el transporte en marcha), o la hora del sistema que el host da por ciclo (VST3 systemTime,
//     AU host time), o el reloj de la máquina. Desde ahí avanzan a la par.
//   - Comprobación: por cada bloque el Main publica una huella de todo lo que generó; el satélite la compara
//     con la de su réplica. Si difieren (o el Main dejó de procesar) suelta sus notas y se vuelve a sincronizar,
//     atacando las notas de sus carriles que estén sonando.

#include "Generator.h"
#include <climits>
#include <memory>
#include <mutex>

namespace dh::orr {

enum RoleSetting : int { RoleAuto = 0, RoleIndependent = 1, RoleMain = 2, RoleSatellite = 3 };

struct Controls
{
    GlobalParams g;
    std::array<LaneParams, kLanes> lanes {};
    bool reset = false, panic = false;
};

// Marca de un bloque tal como la ve cada instancia.
struct Stamp
{
    int64_t hostSample = 0; bool sampleValid = false;    // posición del host en muestras, sólo con el transporte en marcha
    double ns = 0.0;                                     // hora del ciclo: la del host si la da, si no la de la máquina
};

// ¿El bloque "mine" es el mismo ciclo del host que el último bloque que procesó el Main ("last", de nLast muestras)?
inline bool sameCycle (const Stamp& mine, const Stamp& last, int nLast, double fs)
{
    if (mine.sampleValid && last.sampleValid)
    {
        if (mine.hostSample == last.hostSample) return true;
        if (mine.hostSample == last.hostSample + nLast) return false;
    }
    const double dur = (double) std::max (1, nLast) / std::max (1.0, fs) * 1.0e9;
    return mine.ns - last.ns < 0.5 * dur;
}

inline uint64_t hashBlock (int n, const std::vector<OutEvent>& ev)
{
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h] (uint64_t v) { h ^= v; h *= 1099511628211ull; };
    mix ((uint64_t) n); mix ((uint64_t) ev.size());
    for (auto& e : ev)
    {
        mix ((uint64_t) (uint32_t) e.offset); mix ((uint64_t) e.size); mix ((uint64_t) (uint8_t) (e.lane + 1));
        for (int i = 0; i < e.size && i < 3; ++i) mix (e.data[i]);
    }
    return h;
}

// Lo que publica un Main. No se libera nunca mientras viva el proceso (un satélite puede estar leyéndolo):
// cuando un Main deja de serlo, el bus queda libre para el próximo y cambia de generación.
class MainBus
{
public:
    static constexpr int kRing = 256;
    std::atomic<uint32_t> generation { 0 };
    std::atomic<bool> live { false };
    std::atomic<int64_t> lastBlock { -1 };      // último bloque terminado
    int owner = -1;                              // con el lock del registro

    struct DoneInfo { int64_t clk = 0; int n = 0; Stamp st; uint64_t hash = 0; };

    void writeCtrl (int64_t block, const Controls& c) { auto& s = ctrl[(size_t) (block % kRing)]; seqWrite (s.seq, [&] { s.block = block; s.c = c; }); }
    bool readCtrl (int64_t block, Controls& c) const
    {
        const auto& s = ctrl[(size_t) (block % kRing)];
        bool ok = false;
        return seqRead (s.seq, [&] { ok = s.block == block; if (ok) c = s.c; }) && ok;
    }
    void writeDone (int64_t block, const DoneInfo& d)
    {
        auto& s = done[(size_t) (block % kRing)];
        seqWrite (s.seq, [&] { s.block = block; s.d = d; });
        lastBlock.store (block, std::memory_order_release);
    }
    // 1 = encontrado, 0 = todavía no, -1 = ya se pisó (o nunca va a estar)
    int readDone (int64_t block, DoneInfo& d) const
    {
        const int64_t last = lastBlock.load (std::memory_order_acquire);
        if (block > last) return 0;
        if (last - block >= kRing - 4) return -1;
        const auto& s = done[(size_t) (block % kRing)];
        bool ok = false;
        if (! seqRead (s.seq, [&] { ok = s.block == block; if (ok) d = s.d; })) return 0;
        return ok ? 1 : -1;
    }

    // Copia de la generadora en un borde de bloque, a pedido de un satélite.
    std::mutex cpMutex;
    std::unique_ptr<Generator> cp = std::make_unique<Generator>();
    int64_t cpBlock = -1, cpClk = 0; uint32_t cpGeneration = 0;     // con cpMutex
    std::atomic<uint32_t> cpRequest { 0 };

private:
    struct CtrlSlot { std::atomic<uint32_t> seq { 0 }; int64_t block = -1; Controls c; };
    struct DoneSlot { std::atomic<uint32_t> seq { 0 }; int64_t block = -1; DoneInfo d; };
    std::array<CtrlSlot, kRing> ctrl {};
    std::array<DoneSlot, kRing> done {};

    template <typename F> static void seqWrite (std::atomic<uint32_t>& seq, F&& f)
    {
        seq.fetch_add (1, std::memory_order_relaxed);
        std::atomic_thread_fence (std::memory_order_release);
        f();
        seq.fetch_add (1, std::memory_order_release);
    }
    template <typename F> static bool seqRead (const std::atomic<uint32_t>& seq, F&& f)
    {
        for (int tries = 0; tries < 8; ++tries)
        {
            const uint32_t a = seq.load (std::memory_order_acquire);
            if (a & 1) continue;
            f();
            std::atomic_thread_fence (std::memory_order_acquire);
            if (seq.load (std::memory_order_relaxed) == a) return true;
        }
        return false;
    }
};

// El lado del Main: aplica los controles un bloque tarde, publica controles, huellas y copias.
class MainCore
{
public:
    MainBus* bus = nullptr;
    uint32_t generation = 0;
    int64_t block = 0, clk = 0;

    void attach (MainBus* b) { bus = b; generation = b != nullptr ? b->generation.load() : 0; block = 0; clk = 0; havePending = false; served = b != nullptr ? b->cpRequest.load() : 0; }
    bool attachedTo (const MainBus* b) const { return b == bus && (b == nullptr || b->generation.load() == generation); }

    // raw: los controles leídos ahora. Devuelve los que valen en este bloque (los del bloque anterior).
    const Controls& begin (const Controls& raw)
    {
        if (! havePending) { cur = raw; if (bus) bus->writeCtrl (block, cur); pending = raw; pending.reset = pending.panic = false; havePending = true; }
        else { cur = pending; pending = raw; }
        if (bus) bus->writeCtrl (block + 1, pending);
        return cur;
    }
    void end (int n, const Stamp& st, const std::vector<OutEvent>& ev, const Generator& gen)
    {
        if (bus)
        {
            MainBus::DoneInfo d; d.clk = clk; d.n = n; d.st = st; d.hash = hashBlock (n, ev);
            bus->writeDone (block, d);
        }
        clk += n; ++block;
        if (bus != nullptr)
        {
            const uint32_t req = bus->cpRequest.load (std::memory_order_acquire);
            if (req != served)
            {
                std::unique_lock<std::mutex> lk (bus->cpMutex, std::try_to_lock);
                if (lk.owns_lock())
                {
                    bus->cp->copyStateFrom (gen);
                    bus->cpBlock = block; bus->cpClk = clk; bus->cpGeneration = generation;
                    served = req;
                }
            }
        }
    }
private:
    Controls cur, pending;
    bool havePending = false;
    uint32_t served = 0;
};

// El lado del satélite: la réplica y la salida de sus carriles.
class SatelliteCore
{
public:
    enum State { Waiting = 0, Syncing = 1, InSync = 2 };
    Generator rep;
    std::atomic<int> state { Waiting };
    std::atomic<int> resyncs { 0 };
    uint32_t mask = 0;

    SatelliteCore() { rep.prepare (48000.0); ev.reserve (8192); ver.reserve (256); }

    // Un bloque. bus: el del Main a seguir (o nada). newMask: los carriles del Main que toca. out: lo que sale.
    void process (MainBus* bus, int n, double fs, const Stamp& st, bool playing, double ppq, double bpm, uint32_t newMask, std::vector<OutEvent>& out)
    {
        out.clear();
        if (bus != curBus || (bus != nullptr && bus->generation.load() != busGen))
        {
            if (synced) release (out, mask);
            synced = false; curBus = bus; busGen = bus != nullptr ? bus->generation.load() : 0; hint = kNoHint;
        }
        if (bus == nullptr || ! bus->live.load()) { if (synced) { release (out, mask); synced = false; } state.store (Waiting); finish (n, newMask, out); return; }
        if (! synced) trySync (bus, fs, st, newMask, out);
        else if (newMask != mask)
        {
            // Otro carril: lo que sonaba en el que deja se suelta y lo que suena en el nuevo entra ya.
            release (out, mask & ~newMask); attack (out, newMask & ~mask);
            mask = newMask;
        }
        if (synced)
        {
            const int64_t mb = own + boff;
            Controls c;
            if (! bus->readCtrl (mb, c)) { lose (out); }      // el Main dejó de procesar (o la réplica iba un bloque adelantada)
            else
            {
                rep.g = c.g; rep.lanes = c.lanes;
                if (c.reset) rep.resetRequest.store (true);
                if (c.panic) rep.panic (ev);
                else rep.process (n, playing, ppq, bpm, ev);
                if (ver.size() >= 200) ver.erase (ver.begin(), ver.begin() + 100);
                ver.push_back ({ mb, ownClk + coff, n, hashBlock (n, ev), st });
                for (auto& e : ev) if (passes (e, mask)) out.push_back (e);
                state.store (InSync);
            }
        }
        if (synced) verify (bus, fs, out);
        finish (n, newMask, out);
    }

    // Note-off de todo lo que suena en sus carriles (bypass, cambio de rol) y vuelve a esperar.
    void releaseAll (std::vector<OutEvent>& out) { if (synced) release (out, mask); synced = false; state.store (Waiting); }
    void desync() { synced = false; state.store (Waiting); }
    bool isSynced() const { return synced; }

private:
    MainBus* curBus = nullptr; uint32_t busGen = 0;
    bool synced = false;
    static constexpr int64_t kNoHint = INT64_MIN;
    int64_t hint = kNoHint;                       // el desfase (bloques) a probar en la próxima sincronización
    double dtSum = 0.0; int dtN = 0;
    int64_t own = 0, ownClk = 0, boff = 0, coff = 0;
    int sinceRequest = 1000;
    std::vector<OutEvent> ev;
    struct Ver { int64_t mb, mclk; int n; uint64_t hash; Stamp st; };
    std::vector<Ver> ver;

    bool passes (const OutEvent& e, uint32_t m) const
    {
        if (e.lane >= 0) return ((m >> e.lane) & 1u) != 0;
        const int ch = e.data[0] & 0x0F;      // mensajes de todos los canales (pánico): sólo los de sus carriles
        for (int l = 0; l < kLanes; ++l) if (((m >> l) & 1u) && std::clamp (rep.lanes[(size_t) l].channel, 1, 16) - 1 == ch) return true;
        return false;
    }
    static void put (std::vector<OutEvent>& out, int off, uint8_t s, uint8_t d1, uint8_t d2, int lane)
    {
        OutEvent e; e.offset = off; e.data[0] = s; e.data[1] = d1; e.data[2] = d2; e.size = 3; e.lane = (int8_t) lane;
        out.push_back (e);
    }
    void release (std::vector<OutEvent>& out, uint32_t m)
    {
        rep.forEachSounding ([&] (int ch, int note, int lane, int) { if (lane >= 0 && ((m >> lane) & 1u)) put (out, 0, (uint8_t) (0x80 | ch), (uint8_t) note, 0, lane); });
    }
    void attack (std::vector<OutEvent>& out, uint32_t m)
    {
        rep.forEachSounding ([&] (int ch, int note, int lane, int vel) { if (lane >= 0 && ((m >> lane) & 1u)) put (out, 0, (uint8_t) (0x90 | ch), (uint8_t) note, (uint8_t) vel, lane); });
    }
    void lose (std::vector<OutEvent>& out) { release (out, mask); synced = false; state.store (Syncing); resyncs.fetch_add (1); }

    void trySync (MainBus* bus, double fs, const Stamp& st, uint32_t newMask, std::vector<OutEvent>& out)
    {
        state.store (Syncing);
        ++sinceRequest;
        const int64_t lb = bus->lastBlock.load (std::memory_order_acquire);
        MainBus::DoneInfo last;
        if (lb < 0 || bus->readDone (lb, last) != 1) { request (bus); return; }
        bool same = sameCycle (st, last.st, last.n, fs);
        if (hint != kNoHint)        // la comprobación anterior dijo hacia dónde estaba corrida
        {
            if (own + hint == lb) same = true;
            else if (own + hint == lb + 1) same = false;
        }
        const int64_t tb = same ? lb : lb + 1, tclk = same ? last.clk : last.clk + last.n;
        std::unique_lock<std::mutex> lk (bus->cpMutex, std::try_to_lock);
        if (! lk.owns_lock()) return;
        if (bus->cpGeneration == busGen && bus->cpBlock == tb && bus->cpClk == tclk)
        {
            rep.copyStateFrom (*bus->cp);
            lk.unlock();
            boff = tb - own; coff = tclk - ownClk; synced = true; ver.clear(); hint = kNoHint; dtSum = 0.0; dtN = 0;
            mask = newMask;
            attack (out, mask);      // las notas que ya suenan en sus carriles entran ahora
        }
        else if (bus->cpGeneration != busGen || bus->cpBlock < tb) { lk.unlock(); request (bus); }
    }
    void request (MainBus* bus)
    {
        if (sinceRequest < 2) return;
        sinceRequest = 0;
        bus->cpRequest.fetch_add (1, std::memory_order_acq_rel);
    }

    void verify (MainBus* bus, double fs, std::vector<OutEvent>& out)
    {
        size_t k = 0;
        for (; k < ver.size(); ++k)
        {
            const auto& v = ver[k];
            MainBus::DoneInfo d;
            const int r = bus->readDone (v.mb, d);
            if (r == 0) break;              // el Main todavía no llegó (procesa después en este ciclo)
            if (r < 0) continue;
            if (d.clk != v.mclk || d.n != v.n || d.hash != v.hash) { ver.clear(); lose (out); return; }     // algo se perdió
            // Alineación: si la réplica copia el bloque del Main del ciclo anterior (o del siguiente), genera lo mismo
            // pero corrido un bloque, y la huella no lo ve. Lo ven las marcas: la posición del host tiene que ser la
            // misma; sin ella, en promedio, la hora de este bloque y la del bloque del Main tienen que coincidir.
            if (v.st.sampleValid && d.st.sampleValid)
            {
                if (v.st.hostSample != d.st.hostSample) { hint = boff + (v.st.hostSample > d.st.hostSample ? 1 : -1); ver.clear(); lose (out); return; }
            }
            else if (! v.st.sampleValid && ! d.st.sampleValid)
            {
                dtSum += (v.st.ns - d.st.ns) / ((double) std::max (1, v.n) / std::max (1.0, fs) * 1.0e9); ++dtN;
                if (dtN >= 32)
                {
                    const double mean = dtSum / dtN; dtSum = 0.0; dtN = 0;
                    if (std::abs (mean) > 0.5) { hint = boff + (mean > 0.0 ? 1 : -1); ver.clear(); lose (out); return; }
                }
            }
        }
        ver.erase (ver.begin(), ver.begin() + (long) k);
    }

    void finish (int n, uint32_t newMask, std::vector<OutEvent>& out)
    {
        if (! synced) mask = newMask;
        for (auto& e : out) e.offset = std::clamp (e.offset, 0, std::max (0, n - 1));
        ++own; ownClk += n;
    }
};

// Registro de las Orrery del proceso: roles, carriles de los satélites, el Main a seguir.
class Constellation
{
public:
    static constexpr int kMax = 64;
    struct Member
    {
        std::atomic<bool> used { false };
        std::atomic<int> order { 0 };
        std::atomic<int> role { 0 };                  // efectivo: 0 sin resolver, 1 independiente, 2 Main, 3 satélite
        std::atomic<uint32_t> lanes { 0 };            // satélite: carriles del Main que toca
        std::atomic<uint32_t> laneOn { 0 };           // Main: carriles encendidos
        std::atomic<MainBus*> bus { nullptr };        // Main: su bus
        std::atomic<MainBus*> follows { nullptr };    // satélite: el bus que sigue
        std::atomic<int> sync { 0 };                  // satélite: SatelliteCore::State
        std::atomic<double> seenMs { 0.0 };           // último bloque procesado
        char name[64] {};                             // con el lock
    };

    static Constellation& get() { static Constellation c; return c; }
    std::mutex lock;
    std::array<Member, kMax> members;

    int join()
    {
        std::lock_guard<std::mutex> lk (lock);
        for (int i = 0; i < kMax; ++i)
            if (! members[(size_t) i].used.load())
            {
                auto& m = members[(size_t) i];
                m.role.store (0); m.lanes.store (0); m.laneOn.store (0); m.bus.store (nullptr); m.follows.store (nullptr);
                m.sync.store (0); m.seenMs.store (0.0); m.name[0] = 0;
                m.order.store (nextOrder++); m.used.store (true);
                return i;
            }
        return -1;
    }
    void leave (int i)
    {
        if (i < 0) return;
        std::lock_guard<std::mutex> lk (lock);
        setRoleLocked (i, 0, 0);
        members[(size_t) i].used.store (false);
    }
    // Con el lock tomado (lock o try_lock del que llama).
    void setRoleLocked (int i, int role, uint32_t lanes)
    {
        auto& m = members[(size_t) i];
        if (role == RoleMain && m.bus.load() == nullptr)
        {
            MainBus* b = nullptr;
            for (auto& p : pool) if (p->owner < 0) { b = p.get(); break; }
            if (b == nullptr) { pool.push_back (std::make_unique<MainBus>()); b = pool.back().get(); }
            b->owner = i; b->generation.fetch_add (1); b->lastBlock.store (-1);
            { std::lock_guard<std::mutex> cl (b->cpMutex); b->cpBlock = -1; }
            b->live.store (true);
            m.bus.store (b);
        }
        else if (role != RoleMain && m.bus.load() != nullptr)
        {
            auto* b = m.bus.load();
            b->live.store (false); b->generation.fetch_add (1); b->owner = -1;
            m.bus.store (nullptr);
        }
        m.lanes.store (role == RoleSatellite ? lanes : 0u);
        if (role != RoleSatellite) { m.follows.store (nullptr); m.sync.store (0); }
        m.role.store (role);
    }
    // El Main nuevo de un bus (cambió la frecuencia de muestreo, se reinició): los satélites se resincronizan.
    void bumpGeneration (int i)
    {
        std::lock_guard<std::mutex> lk (lock);
        if (auto* b = members[(size_t) i].bus.load()) { b->generation.fetch_add (1); b->lastBlock.store (-1); std::lock_guard<std::mutex> cl (b->cpMutex); b->cpBlock = -1; }
    }

    // El Main a seguir: el más antiguo. Sin lock.
    int mainIndex (int except = -1) const
    {
        int best = -1, bestOrder = 0;
        for (int i = 0; i < kMax; ++i)
        {
            const auto& m = members[(size_t) i];
            if (i == except || ! m.used.load() || m.role.load() != RoleMain || m.bus.load() == nullptr) continue;
            const int o = m.order.load();
            if (best < 0 || o < bestOrder) { best = i; bestOrder = o; }
        }
        return best;
    }
    MainBus* mainBus() const { const int i = mainIndex(); return i >= 0 ? members[(size_t) i].bus.load() : nullptr; }

    // Carriles que tocan los satélites vivos de un bus (para que el Main no los duplique).
    uint32_t takenLanes (const MainBus* b, double nowMs, int except = -1) const
    {
        uint32_t t = 0;
        for (int i = 0; i < kMax; ++i)
        {
            const auto& m = members[(size_t) i];
            if (i == except || ! m.used.load() || m.role.load() != RoleSatellite) continue;
            if (b != nullptr && m.follows.load() != b) continue;
            if (nowMs >= 0.0 && nowMs - m.seenMs.load() > 1500.0) continue;
            t |= m.lanes.load();
        }
        return t;
    }

    // Auto: con un Main vivo, satélite en el próximo carril libre; si no, Main. Con el lock tomado.
    int resolveAutoLocked (int self) const { return mainIndex (self) >= 0 ? RoleSatellite : RoleMain; }
    uint32_t pickLanesLocked (int self) const
    {
        const int mi = mainIndex (self);
        const uint32_t on = mi >= 0 ? members[(size_t) mi].laneOn.load() : 0xFFu;
        uint32_t taken = 0;
        for (int i = 0; i < kMax; ++i)
        {
            const auto& m = members[(size_t) i];
            if (i != self && m.used.load() && m.role.load() == RoleSatellite) taken |= m.lanes.load();
        }
        for (int l = 0; l < kLanes; ++l) if (((on >> l) & 1u) && ! ((taken >> l) & 1u)) return 1u << l;
        for (int l = 0; l < kLanes; ++l) if (! ((taken >> l) & 1u)) return 1u << l;
        return 1u;
    }

private:
    std::vector<std::unique_ptr<MainBus>> pool;
    int nextOrder = 1;
};

} // namespace dh::orr
