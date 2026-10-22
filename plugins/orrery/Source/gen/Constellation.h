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
//   - Pistas que no van a la par (REAPER con "anticipative FX", sobre todo en render offline: cada pista se procesa
//     en su hilo, varios bloques adelante o atrás de las otras; QA en REAPER, H1). Con el transporte en marcha el
//     satélite se ubica por la posición del host, no por el orden de proceso: pide una copia del estado del Main en
//     un borde de bloque que todavía no alcanzó (más adelante si hace falta) y empieza al llegar a ese borde. Si va
//     adelantado y el Main todavía no publicó los controles de un bloque, usa los últimos que vio y los confirma
//     cuando aparecen; si no coinciden, se resincroniza. Entre una pérdida y el próximo ataque espera cada vez
//     más (si no, una sincronización que falla en cada bloque inundaba de notas de largo cero).

#include "Generator.h"
#include <algorithm>
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
   #if defined (__clang__)
    #pragma clang diagnostic push
    #pragma clang diagnostic ignored "-Wfloat-equal"      // igualdad exacta a propósito (¿son los mismos ajustes?)
   #endif
    bool operator== (const Controls&) const = default;
   #if defined (__clang__)
    #pragma clang diagnostic pop
   #endif
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
    static constexpr int kRing = 1024;
    std::atomic<uint32_t> generation { 0 };
    std::atomic<bool> live { false };
    std::atomic<int64_t> lastBlock { -1 };      // último bloque terminado
    std::atomic<int64_t> lastCtrl { -1 };       // último bloque con controles publicados
    std::atomic<double> lastWallMs { -1.0 };    // hora de la máquina del último bloque (en tiempo real; -1 offline)
    int owner = -1;                              // con el lock del registro

    struct DoneInfo { int64_t clk = 0; int n = 0; Stamp st; uint64_t hash = 0; };

    void writeCtrl (int64_t block, const Controls& c)
    {
        auto& s = ctrl[(size_t) (block % kRing)];
        seqWrite (s.seq, [&] { s.block = block; s.c = c; });
        if (block > lastCtrl.load (std::memory_order_relaxed)) lastCtrl.store (block, std::memory_order_release);
    }
    // 1 = encontrado, 0 = todavía no (el Main no llegó), -1 = ya se pisó
    int readCtrl (int64_t block, Controls& c) const
    {
        if (block > lastCtrl.load (std::memory_order_acquire)) return 0;
        const auto& s = ctrl[(size_t) (block % kRing)];
        bool ok = false;
        if (! seqRead (s.seq, [&] { ok = s.block == block; if (ok) c = s.c; })) return 0;
        return ok ? 1 : -1;
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

    void attach (MainBus* b) { bus = b; generation = b != nullptr ? b->generation.load() : 0; block = 0; clk = 0; havePending = false; served = b != nullptr ? b->cpRequest.load() : 0; if (b) b->lastCtrl.store (-1); }
    bool attachedTo (const MainBus* b) const { return b == bus && (b == nullptr || b->generation.load() == generation); }

    // raw: los controles leídos ahora. Devuelve los que valen en este bloque (los del bloque anterior).
    const Controls& begin (const Controls& raw)
    {
        if (! havePending) { cur = raw; if (bus) bus->writeCtrl (block, cur); pending = raw; pending.reset = pending.panic = false; pending.g.runCmd = 0; havePending = true; }
        else { cur = pending; pending = raw; }
        if (bus) bus->writeCtrl (block + 1, pending);
        return cur;
    }
    // nowMs: la hora de la máquina en tiempo real, o -1 en un render offline.
    void end (int n, const Stamp& st, const std::vector<OutEvent>& ev, const Generator& gen, double nowMs = -1.0)
    {
        if (bus)
        {
            bus->lastWallMs.store (nowMs, std::memory_order_relaxed);
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
    static constexpr double kMaxLeadSec = 1.0;      // lo más que espera los controles o la huella de un Main atrasado

    SatelliteCore() { rep.prepare (48000.0); ev.reserve (8192); ver.reserve (1024); }

    // Un bloque. bus: el del Main a seguir (o nada). newMask: los carriles del Main que toca. out: lo que sale.
    // nowMs: la hora de la máquina en tiempo real, o -1 en un render offline.
    void process (MainBus* bus, int n, double fs, const Stamp& st, bool playing, double ppq, double bpm, uint32_t newMask, std::vector<OutEvent>& out,
                  double nowMs = -1.0)
    {
        out.clear();
        curN = n; curFs = fs;
        hist[(size_t) (own % kHist)] = { own, ownClk, n, playing, ppq, bpm, st };
        if (bus != curBus || (bus != nullptr && bus->generation.load() != busGen))
        {
            if (synced) release (out, mask);
            synced = false; staged = false; curBus = bus; busGen = bus != nullptr ? bus->generation.load() : 0; hint = kNoHint;
            holdOff = 0; backoff = 2;
        }
        if (bus == nullptr || ! bus->live.load()) { if (synced) { release (out, mask); synced = false; } staged = false; state.store (Waiting); finish (n, newMask, out); return; }
        if (! synced) trySync (bus, n, fs, st, newMask, out);
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
            const int r = bus->readCtrl (mb, c);
            bool predicted = false, ok = true;
            if (r == 1)
            {
                // Controles nuevos: antes de cambiar la base de las predicciones, se confirman las pendientes.
                if (! confirmPredictions (bus)) ok = false;
                else { base = c; base.reset = base.panic = false; base.g.runCmd = 0; haveBase = true; }
            }
            else if (r == 0 && haveBase && st.sampleValid && (int) ver.size() < maxLead (n, fs) && ! mainStalled (bus, n, fs, nowMs))
            {
                c = base; predicted = true;           // el Main va atrasado (con el transporte en marcha: render, anticipative FX)
            }
            else ok = false;              // el Main dejó de procesar (o se pisaron los controles)
            if (! ok) lose (out);
            else
            {
                step (mb, hist[(size_t) (own % kHist)], c, predicted);
                for (auto& e : ev) if (passes (e, mask)) out.push_back (e);
                state.store (InSync);
            }
        }
        if (synced) verify (bus, n, fs, out);
        finish (n, newMask, out);
    }

    // Note-off de todo lo que suena en sus carriles (bypass, cambio de rol) y vuelve a esperar.
    void releaseAll (std::vector<OutEvent>& out) { if (synced) release (out, mask); synced = false; staged = false; state.store (Waiting); }
    void desync() { synced = false; staged = false; state.store (Waiting); }
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
    struct Ver { int64_t mb, mclk; int n; uint64_t hash; Stamp st; bool predicted; };
    std::vector<Ver> ver;                         // bloques de la réplica todavía sin comparar con el Main
    // Por posición del host: la copia del Main ya está en la réplica y empieza en su borde stagedClk (muestras del Main).
    bool staged = false; int64_t stagedBlock = 0, stagedClk = 0;
    // Lo que recibió cada bloque propio (para ponerse al día desde una copia del Main que quedó atrás).
    struct In { int64_t own = -1, clk = 0; int n = 0; bool playing = false; double ppq = 0.0, bpm = 120.0; Stamp st; };
    static constexpr int kHist = 1024;
    std::vector<In> hist = std::vector<In> (kHist);
    Controls base; bool haveBase = false;         // los últimos controles leídos: la predicción si el Main va atrasado
    int holdOff = 0, backoff = 2; int64_t syncedAt = 0;   // espera entre una pérdida y el próximo intento (bloques)
    int curN = 512; double curFs = 48000.0;

    // Un bloque de la réplica: el bloque mb del Main con estos controles.
    void step (int64_t mb, const In& in, const Controls& c, bool predicted)
    {
        rep.g = c.g; rep.lanes = c.lanes;
        if (c.reset) rep.resetRequest.store (true);
        if (c.panic) rep.panic (ev);
        else rep.process (in.n, in.playing, in.ppq, in.bpm, ev);
        ver.push_back ({ mb, in.clk + coff, in.n, hashBlock (in.n, ev), in.st, predicted });
    }
    // En tiempo real, un Main que no procesa hace más de 3 bloques (y 60 ms) no va atrasado: está en bypass o congelado.
    static bool mainStalled (MainBus* bus, int n, double fs, double nowMs)
    {
        const double last = bus->lastWallMs.load (std::memory_order_relaxed);
        return nowMs >= 0.0 && last >= 0.0 && nowMs - last > std::max (60.0, 3000.0 * n / std::max (1.0, fs));
    }
    static int maxLead (int n, double fs) { return std::clamp ((int) (kMaxLeadSec * fs / std::max (1, n)), 8, MainBus::kRing / 2); }

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
    void lose (std::vector<OutEvent>& out)
    {
        release (out, mask); synced = false; staged = false; state.store (Syncing); resyncs.fetch_add (1);
        ver.clear(); haveBase = false;
        // Se perdió poco después de sincronizarse (menos de 1 s): la próxima vez espera el doble, hasta 0,5 s.
        const int perSec = std::max (1, (int) (curFs / std::max (1, curN)));
        backoff = own - syncedAt < perSec ? std::min (backoff * 2, std::max (2, perSec / 2)) : 2;
        holdOff = backoff;
    }
    bool confirmPredictions (MainBus* bus)
    {
        for (auto& v : ver)
        {
            if (! v.predicted) continue;
            Controls real;
            if (bus->readCtrl (v.mb, real) != 1 || ! (real == base)) return false;
            v.predicted = false;
        }
        return true;
    }

    void trySync (MainBus* bus, int n, double fs, const Stamp& st, uint32_t newMask, std::vector<OutEvent>& out)
    {
        state.store (Syncing);
        ++sinceRequest;
        if (staged)
        {
            const int64_t m = ownClk + coff;
            if (m == stagedClk) { boff = stagedBlock - own; begin (newMask, out); }
            else if (m > stagedClk) staged = false;       // se pasó (bloques de otro tamaño que los del Main): otra vez
            return;
        }
        if (holdOff > 0) { --holdOff; return; }
        const int64_t lb = bus->lastBlock.load (std::memory_order_acquire);
        MainBus::DoneInfo last;
        if (lb < 0 || bus->readDone (lb, last) != 1) { request (bus); return; }
        if (st.sampleValid && last.st.sampleValid) { syncByPosition (bus, n, st, last, newMask, out); return; }
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
            boff = tb - own; coff = tclk - ownClk;
            begin (newMask, out);
        }
        else if (bus->cpGeneration != busGen || bus->cpBlock < tb) { lk.unlock(); request (bus); }
    }
    // Con el transporte en marcha: la muestra del Main que corresponde al comienzo de este bloque sale de las
    // posiciones del host (la del último bloque del Main y la propia), así que no importa cuántos bloques vaya
    // adelante o atrás. Una copia en un borde todavía no alcanzado se guarda hasta llegar a él; una que quedó atrás
    // (el satélite va adelantado) se pone al día con lo que recibió cada bloque propio desde ese borde.
    void syncByPosition (MainBus* bus, int n, const Stamp& st, const MainBus::DoneInfo& last, uint32_t newMask, std::vector<OutEvent>& out)
    {
        const int64_t mine = last.clk + (st.hostSample - last.st.hostSample);
        std::unique_lock<std::mutex> lk (bus->cpMutex, std::try_to_lock);
        if (! lk.owns_lock()) return;
        const bool have = bus->cpGeneration == busGen && bus->cpBlock >= 0;
        const int64_t span = (int64_t) maxLead (n, curFs) * std::max (1, n);
        if (have && bus->cpClk >= mine && bus->cpClk - mine <= span)
        {
            rep.copyStateFrom (*bus->cp);
            stagedBlock = bus->cpBlock; stagedClk = bus->cpClk;
            lk.unlock();
            coff = mine - ownClk; staged = true;
            if (stagedClk == mine) { boff = stagedBlock - own; begin (newMask, out); }
            return;
        }
        if (have && bus->cpClk < mine && mine - bus->cpClk <= span)
        {
            // Los bloques propios desde el borde de la copia hasta éste, en orden y sin huecos.
            const int64_t c0 = bus->cpClk - (mine - ownClk);        // el borde, en muestras propias
            int64_t first = -1;
            for (int64_t k = own - 1; k >= 0 && k > own - kHist; --k)
            {
                const auto& h = hist[(size_t) (k % kHist)];
                if (h.own != k || h.clk < c0) break;
                if (h.clk == c0) { first = k; break; }
            }
            Controls cc;
            if (first >= 0 && bus->readCtrl (bus->cpBlock, cc) == 1)
            {
                rep.copyStateFrom (*bus->cp);
                const int64_t cb = bus->cpBlock;
                lk.unlock();
                coff = mine - ownClk; ver.clear();
                base = cc; base.reset = base.panic = false; base.g.runCmd = 0; haveBase = true;
                for (int64_t k = first; k < own; ++k)
                {
                    const int64_t mb = cb + (k - first);
                    Controls c; bool predicted = false;
                    const int r = bus->readCtrl (mb, c);
                    if (r == 1 && confirmPredictions (bus)) { base = c; base.reset = base.panic = false; base.g.runCmd = 0; }
                    else if (r == 0) { c = base; predicted = true; }
                    else { ver.clear(); haveBase = false; request (bus); return; }
                    step (mb, hist[(size_t) (k % kHist)], c, predicted);
                }
                boff = cb + (own - first) - own;
                begin (newMask, out, true);
                return;
            }
        }
        lk.unlock();
        request (bus);
    }
    void begin (uint32_t newMask, std::vector<OutEvent>& out, bool keepVer = false)
    {
        synced = true; staged = false; hint = kNoHint; dtSum = 0.0; dtN = 0; syncedAt = own;
        if (! keepVer) { ver.clear(); haveBase = false; }
        mask = newMask;
        attack (out, mask);      // las notas que ya suenan en sus carriles entran ahora
    }
    void request (MainBus* bus)
    {
        if (sinceRequest < 2) return;
        sinceRequest = 0;
        bus->cpRequest.fetch_add (1, std::memory_order_acq_rel);
    }

    void verify (MainBus* bus, int n, double fs, std::vector<OutEvent>& out)
    {
        size_t k = 0;
        for (; k < ver.size(); ++k)
        {
            const auto& v = ver[k];
            MainBus::DoneInfo d;
            const int r = bus->readDone (v.mb, d);
            if (r == 0) break;              // el Main todavía no llegó (procesa después en este ciclo, o va atrasado)
            if (r < 0) { if (v.predicted) { lose (out); return; } continue; }
            if (d.clk != v.mclk || d.n != v.n || d.hash != v.hash) { lose (out); return; }     // algo se perdió
            if (v.predicted)        // los controles que se supusieron tienen que ser los que el Main usó
            {
                Controls real;
                if (bus->readCtrl (v.mb, real) != 1 || ! (real == base)) { lose (out); return; }
            }
            // Alineación: si la réplica copia el bloque del Main del ciclo anterior (o del siguiente), genera lo mismo
            // pero corrido un bloque, y la huella no lo ve. Lo ven las marcas: la posición del host tiene que ser la
            // misma; sin ella, en promedio, la hora de este bloque y la del bloque del Main tienen que coincidir.
            if (v.st.sampleValid && d.st.sampleValid)
            {
                if (v.st.hostSample != d.st.hostSample) { hint = boff + (v.st.hostSample > d.st.hostSample ? 1 : -1); lose (out); return; }
            }
            else if (! v.st.sampleValid && ! d.st.sampleValid)
            {
                dtSum += (v.st.ns - d.st.ns) / ((double) std::max (1, v.n) / std::max (1.0, fs) * 1.0e9); ++dtN;
                if (dtN >= 32)
                {
                    const double mean = dtSum / dtN; dtSum = 0.0; dtN = 0;
                    if (std::abs (mean) > 0.5) { hint = boff + (mean > 0.0 ? 1 : -1); lose (out); return; }
                }
            }
        }
        ver.erase (ver.begin(), ver.begin() + (long) k);
        if ((int) ver.size() > maxLead (n, fs)) lose (out);      // el Main dejó de procesar
    }

    // Una nota que se ataca y se suelta en el mismo instante (una sincronización que se pierde en el mismo bloque)
    // no sale: un sinte la tocaría como un clic.
    static void dropZeroLength (std::vector<OutEvent>& out)
    {
        for (size_t i = 0; i < out.size(); ++i)
        {
            auto& a = out[i];
            if ((a.data[0] & 0xF0) != 0x90 || a.data[2] == 0 || a.size == 0) continue;
            for (size_t j = i + 1; j < out.size(); ++j)
            {
                auto& b = out[j];
                if (b.size == 0 || (b.data[0] & 0x0F) != (a.data[0] & 0x0F) || b.data[1] != a.data[1]) continue;
                const int bs = b.data[0] & 0xF0;
                if (bs == 0x90 && b.data[2] > 0) break;                 // otra nota-on de la misma tecla: la primera vale
                if (bs == 0x80 || bs == 0x90)
                {
                    if (b.offset == a.offset) { a.size = 0; b.size = 0; }
                    break;
                }
            }
        }
        out.erase (std::remove_if (out.begin(), out.end(), [] (const OutEvent& e) { return e.size == 0; }), out.end());
    }

    void finish (int n, uint32_t newMask, std::vector<OutEvent>& out)
    {
        if (! synced) mask = newMask;
        for (auto& e : out) e.offset = std::clamp (e.offset, 0, std::max (0, n - 1));
        dropZeroLength (out);
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
            b->owner = i; b->generation.fetch_add (1); b->lastBlock.store (-1); b->lastCtrl.store (-1);
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
        if (auto* b = members[(size_t) i].bus.load()) { b->generation.fetch_add (1); b->lastBlock.store (-1); b->lastCtrl.store (-1); std::lock_guard<std::mutex> cl (b->cpMutex); b->cpBlock = -1; }
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
