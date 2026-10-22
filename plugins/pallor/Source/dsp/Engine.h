#pragma once
// El motor de Pallor (docs/research.md §7), sin JUCE: lo usan el plugin, pallorrender y pallortest.
//
// Por bloque: el secuenciador da los golpes del bloque (con su muestra exacta), se suman las notas MIDI de entrada,
// cada golpe se vuelve un evento del slot (y un choke para los otros slots de su grupo), cada slot se renderiza
// cortando en sus eventos, y su salida mono se panea al bus que le toca (principal o uno de los seis auxiliares) y a
// los dos envíos. Los envíos vuelven al principal; el principal pasa por el maestro. Si el maestro agrega latencia (la
// cinta sobremuestreada) los auxiliares se atrasan lo mismo para que todo siga alineado.
//
// MIDI: entrada con el mapa GM (36 bombo, 38 caja, 42 charles cerrado…) o cromático (36-51, los 16 pads); salida de lo
// que toca el secuenciador con el mismo mapa, note-off a los 30 ms (ajustable) (o antes si la misma nota vuelve), y todos los
// note-off pendientes al parar o saltar.

#include "Voice.h"
#include "KitGen.h"
#include "Seq.h"
#include "Effects.h"

namespace dh::pal {

constexpr int kBuses = 7;
enum Bus { kBusMain, kBusKick, kBusSnare, kBusHats, kBusToms, kBusPerc, kBusFx };
inline const char* busName (int b) { static const char* n[kBuses] = { "Main", "Kick", "Snare/Clap", "Hats/Cymbals", "Toms", "Perc", "FX" }; return n[std::clamp (b, 0, kBuses - 1)]; }
inline int groupBus (int slot)
{
    static const int b[kNumSlots] = { kBusKick, kBusSnare, kBusSnare, kBusSnare, kBusHats, kBusHats, kBusHats, kBusHats, kBusToms, kBusToms, kBusToms, kBusToms, kBusPerc, kBusPerc, kBusPerc, kBusFx };
    return b[std::clamp (slot, 0, kNumSlots - 1)];
}

// Mapas de notas. GM: los números del estándar de percusión del canal 10 (con alias); cromático: 36-51 por slot.
inline int gmNote (int slot) { static const int n[kNumSlots] = { 36, 38, 39, 37, 42, 46, 49, 51, 41, 45, 48, 50, 56, 75, 54, 52 }; return n[std::clamp (slot, 0, kNumSlots - 1)]; }
inline int noteToSlot (int note, int map)
{
    if (map == 1) return note >= 36 && note < 36 + kNumSlots ? note - 36 : -1;
    switch (note)
    {
        case 35: case 36: return kKick;
        case 38: case 40: return kSnare;
        case 39: return kClap;
        case 37: return kRim;
        case 42: case 44: return kClosedHat;
        case 46: return kOpenHat;
        case 49: case 55: case 57: return kCrash;
        case 51: case 53: case 59: return kRide;
        case 41: case 43: return kTom1;
        case 45: return kTom2;
        case 47: case 48: return kTom3;
        case 50: return kTom4;
        case 56: return kCowbell;
        case 75: case 76: case 77: return kClave;
        case 54: case 69: case 70: case 82: return kTamb;
        case 52: return kFx;
        default: return -1;
    }
}
inline int slotToNote (int slot, int map) { return map == 1 ? 36 + slot : gmNote (slot); }

struct GlobalParams
{
    SeqSettings seq;
    float accent = 0.8f;          // nivel general de acento
    int noteMap = 0;              // 0 GM, 1 cromático
    bool midiOut = true; int midiChannel = 10;
    int routing = 0;              // 0 todo al principal, 1 por grupo, 2 el bus de cada slot
    int accentVelocity = 120;     // MIDI: desde esta velocidad la nota es acento
    float midiGateMs = 30.0f;     // largo de las notas de la salida MIDI
};

struct EngineParams
{
    std::array<VoiceParams, kNumSlots> v {};
    GlobalParams g;
    SpaceParams a; float aReturnDb = 0.0f;
    EchoParams b; float bReturnDb = 0.0f;
    MasterParams m;
};

struct HostTime { bool playing = false; double ppq = 0.0, bpm = 120.0; };
struct MidiIn { int offset = 0, note = 0, vel = 0; bool off = false; };
struct MidiOut { int offset = 0, channel = 10, note = 0, vel = 0; };      // vel 0 = note-off
struct HitView { int offset; int slot; float vel; bool accent; };

class Engine
{
public:
    static constexpr int kSub = 1024;      // el bloque interno más largo
    static constexpr float kMixGain = 0.5f;

    void prepare (double sampleRate, int /*maxBlock*/)
    {
        fs = (float) sampleRate;
        lib = &kitgen::Library::get();
        for (int s = 0; s < kNumSlots; ++s) slots[(size_t) s].prepare (fs, s);
        space.prepare (fs); echo.prepare (fs); master.prepare (fs);
        vb.assign (kSub, 0.0f);
        for (auto& b : bus) b.assign (2 * kSub, 0.0f);
        for (auto& b : sendBuf) b.assign (2 * kSub, 0.0f);
        for (auto& d : auxDelay) d.prepare (Master::kTapeLatency + 4);
        hits.reserve (8192); ev.reserve (8192); midiPending.fill ({});
        seq.setBank (&bank);
        smoothK = 1.0f - fastExp (-1.0f / (0.004f * fs));
        reset();
    }
    void reset()
    {
        for (auto& s : slots) s.reset();
        space.reset(); echo.reset(); master.reset(); seq.reset();
        for (auto& d : auxDelay) d.clear();
        wasPlaying = false; samplePos = 0; for (auto& g : gains) g = {};
        firstSet = true;
    }

    void set (const EngineParams& p)
    {
        P = p;
        for (int s = 0; s < kNumSlots; ++s) slots[(size_t) s].set (P.v[(size_t) s]);
        space.set (P.a); echo.set (P.b); master.set (P.m);
        retA = dbToGain (P.aReturnDb); retB = dbToGain (P.bReturnDb);
        bool anySolo = false; for (const auto& v : P.v) anySolo |= v.solo;
        for (int s = 0; s < kNumSlots; ++s)
        {
            const auto& v = P.v[(size_t) s];
            const float lvl = v.mute || (anySolo && ! v.solo) || v.levelDb <= -59.9f ? 0.0f : dbToGain (v.levelDb);
            const float pan = clampf (v.pan, -1.0f, 1.0f);
            const float pl = sin2pi (0.125f * (1.0f - pan)), pr = sin2pi (0.125f * (1.0f + pan));   // potencia constante
            // −6 dB de margen: un kit con los niveles a 0 dB queda con los picos cerca de −3 dBFS antes del maestro.
            target[(size_t) s] = { kMixGain * lvl * pl * 1.41421356f, kMixGain * lvl * pr * 1.41421356f, v.sendA, v.sendB };
            for (auto& x : target[(size_t) s]) x = isFiniteF (x) ? x : 0.0f;
        }
        if (firstSet) { gains = target; firstSet = false; }
    }

    // La muestra propia de un slot (nullptr = ninguna). El objeto vive en el procesador; un reemplazo corta lo que sonaba.
    void setUserSample (int slot, const SampleData* s)
    {
        if (slot < 0 || slot >= kNumSlots || user[(size_t) slot] == s) return;
        if (user[(size_t) slot] != nullptr) slots[(size_t) slot].dropSample (user[(size_t) slot]);
        user[(size_t) slot] = s;
    }
    const SampleData* sampleFor (int slot) const
    {
        const auto& v = P.v[(size_t) slot];
        if (v.kit >= kitgen::numKits()) return user[(size_t) slot] != nullptr ? user[(size_t) slot] : lib->sample (0, slot);
        return lib->sample (v.kit, slot);
    }

    Bank& bankRef() { return bank; }
    const Bank& bankRef() const { return bank; }
    Sequencer& sequencer() { return seq; }
    int latency() const { return kVoiceLatency + master.latency(); }

    // outs: kBuses × 2 punteros (L, R de cada bus; nullptr si el host no activó ese bus: se suma al principal).
    void process (float* const* outs, int n, const HostTime& ht, const MidiIn* in, int nIn, std::vector<MidiOut>* midiOut)
    {
        int inIdx = 0;
        for (int off = 0; off < n; off += kSub)
        {
            const int m = std::min (kSub, n - off);
            HostTime h = ht; h.ppq = ht.ppq + (double) off * ht.bpm / 60.0 / fs;
            // Las notas de entrada de este tramo.
            int inEnd = inIdx; while (inEnd < nIn && in[inEnd].offset < off + m) ++inEnd;
            processSub (outs, off, m, h, in + inIdx, inEnd - inIdx, off, midiOut);
            inIdx = inEnd;
        }
    }

    // Todos los note-off pendientes ya (bypass, stop, CC 123).
    void flushMidi (std::vector<MidiOut>* midiOut, int offset = 0)
    {
        for (auto& p : midiPending)
            if (p.first >= 0) { if (midiOut) midiOut->push_back ({ offset, p.channel, p.first, 0 }); p = {}; }
    }
    void allSoundOff() { for (auto& s : slots) s.reset(); }

    // Vista: el último golpe de cada slot (contador) y los golpes del último bloque.
    std::array<uint32_t, kNumSlots> trigCount {};
    std::array<float, kNumSlots> slotPeak {};     // pico de cada voz desde la última lectura (medidores)
    float masterGrDb() const { return master.gainReductionDb(); }

private:
    float fs = 48000;
    EngineParams P;
    const kitgen::Library* lib = nullptr;
    std::array<Slot, kNumSlots> slots;
    std::array<const SampleData*, kNumSlots> user {};
    Space space; Echo echo; Master master;
    Bank bank; Sequencer seq;
    std::vector<float> vb;
    std::array<std::vector<float>, kBuses> bus;
    std::array<std::vector<float>, 2> sendBuf;
    std::array<Line, 2 * kBuses> auxDelay;
    struct Hit { int offset; int slot; float vel; bool accent; bool fromSeq; };
    struct Ev { int offset; int slot; int kind; float vel; bool accent; };   // kind 0 golpe, 1 choke
    std::vector<Hit> hits; std::vector<Ev> ev;
    struct Pending { int first = -1, second = 0, channel = 10; };             // nota, muestras que faltan, canal
    std::array<Pending, kNumSlots> midiPending {};
    using G4 = std::array<float, 4>;
    std::array<G4, kNumSlots> gains {}, target {};
    float smoothK = 0.01f, retA = 1, retB = 1;
    bool wasPlaying = false, firstSet = true;
    int64_t samplePos = 0;

    void processSub (float* const* outs, int outOff, int n, const HostTime& ht, const MidiIn* in, int nIn, int inBase, std::vector<MidiOut>* midiOut)
    {
        hits.clear(); ev.clear();
        const double rate = ht.bpm / 60.0 / fs;
        // ── Secuenciador ──
        if (ht.playing)
        {
            seq.advance (ht.ppq, n, rate, ht.bpm, P.g.seq, [&] (double t, int tr, float v, bool acc) {
                const int o = std::clamp ((int) std::floor ((t - ht.ppq) / rate + 0.5), 0, n - 1);
                hits.push_back ({ o, tr, v, acc, true });
            });
            if (seq.consumeJump() && wasPlaying) flushMidi (midiOut, outOff);
        }
        else if (wasPlaying) { seq.reset(); flushMidi (midiOut, outOff); }
        wasPlaying = ht.playing;
        // ── MIDI de entrada ──
        for (int i = 0; i < nIn; ++i)
        {
            if (in[i].off || in[i].vel <= 0) continue;
            const int s = noteToSlot (in[i].note, P.g.noteMap);
            if (s < 0) continue;
            hits.push_back ({ std::clamp (in[i].offset - inBase, 0, n - 1), s, (float) in[i].vel / 127.0f, in[i].vel >= P.g.accentVelocity, false });
        }
        std::stable_sort (hits.begin(), hits.end(), [] (const Hit& a, const Hit& b) { return a.offset < b.offset; });
        // ── Eventos por slot (y los choke) ──
        for (const auto& h : hits)
        {
            ev.push_back ({ h.offset, h.slot, 0, h.vel, h.accent });
            const int g = P.v[(size_t) h.slot].choke;
            if (g > 0)
                for (int s = 0; s < kNumSlots; ++s)
                    if (s != h.slot && P.v[(size_t) s].choke == g) ev.push_back ({ h.offset, s, 1, 0.0f, false });
            ++trigCount[(size_t) h.slot];
        }
        // Mismo instante: primero los choke, después los golpes (dos golpes juntos del mismo grupo suenan los dos).
        std::stable_sort (ev.begin(), ev.end(), [] (const Ev& a, const Ev& b) { return a.slot != b.slot ? a.slot < b.slot : (a.offset != b.offset ? a.offset < b.offset : a.kind > b.kind); });
        // ── MIDI de salida ──
        if (midiOut != nullptr) midiOutFor (*midiOut, outOff, n);
        else for (auto& p : midiPending) if (p.first >= 0) { p.second -= n; if (p.second <= 0) p = {}; }
        // ── Slots ──
        for (auto& b : bus) std::fill (b.begin(), b.begin() + 2 * n, 0.0f);
        for (auto& b : sendBuf) std::fill (b.begin(), b.begin() + 2 * n, 0.0f);
        size_t e = 0;
        for (int s = 0; s < kNumSlots; ++s)
        {
            Slot& sl = slots[(size_t) s];
            int pos = 0; bool any = false;
            while (e < ev.size() && ev[e].slot == s)
            {
                const Ev& x = ev[e++];
                if (x.offset > pos) { any |= sl.render (vb.data() + pos, x.offset - pos); pos = x.offset; }
                if (x.kind == 0) sl.trigger (x.vel, x.accent, P.g.accent, P.v[(size_t) s].engine == 1 ? sampleFor (s) : nullptr);
                else sl.choke();
            }
            if (pos < n) any |= sl.render (vb.data() + pos, n - pos);
            if (! any) { gains[(size_t) s] = target[(size_t) s]; continue; }
            int b = P.g.routing == 0 ? kBusMain : (P.g.routing == 1 ? groupBus (s) : std::clamp (P.v[(size_t) s].out, 0, kBuses - 1));
            if (b != kBusMain && (outs[2 * b] == nullptr || outs[2 * b + 1] == nullptr)) b = kBusMain;
            float* bl = bus[(size_t) b].data(); float* br = bl + n;
            float* al = sendBuf[0].data(); float* ar = al + n; float* cl = sendBuf[1].data(); float* cr = cl + n;
            auto& g = gains[(size_t) s]; const auto& tg = target[(size_t) s];
            float pk = slotPeak[(size_t) s];
            for (int i = 0; i < n; ++i)
            {
                for (int k = 0; k < 4; ++k) g[(size_t) k] += (tg[(size_t) k] - g[(size_t) k]) * smoothK;
                const float x = vb[(size_t) i];
                const float l = x * g[0], r = x * g[1];
                pk = std::max (pk, std::max (std::fabs (l), std::fabs (r)));
                bl[i] += l; br[i] += r;
                al[i] += l * g[2]; ar[i] += r * g[2];
                cl[i] += l * g[3]; cr[i] += r * g[3];
            }
            slotPeak[(size_t) s] = pk;
        }
        // ── Envíos ──
        float* ml = bus[0].data(); float* mr = ml + n;
        {
            float* al = sendBuf[0].data(); float* ar = al + n;
            space.process (al, ar, n);
            float* cl = sendBuf[1].data(); float* cr = cl + n;
            echo.process (cl, cr, n, ht.bpm);
            for (int i = 0; i < n; ++i) { ml[i] += al[i] * retA + cl[i] * retB; mr[i] += ar[i] * retA + cr[i] * retB; }
        }
        // ── Maestro ──
        master.process (ml, mr, n, samplePos);
        const int lat = master.latency();
        // ── Salidas ──
        for (int b = 0; b < kBuses; ++b)
        {
            float* oL = outs[2 * b]; float* oR = outs[2 * b + 1];
            if (oL == nullptr || oR == nullptr) continue;
            const float* sl = bus[(size_t) b].data(); const float* sr = sl + n;
            if (b == kBusMain || lat == 0) { std::copy (sl, sl + n, oL + outOff); std::copy (sr, sr + n, oR + outOff); }
            else
                for (int i = 0; i < n; ++i)
                {
                    auxDelay[(size_t) (2 * b)].push (sl[i]); auxDelay[(size_t) (2 * b + 1)].push (sr[i]);
                    oL[outOff + i] = auxDelay[(size_t) (2 * b)].at (lat + 1); oR[outOff + i] = auxDelay[(size_t) (2 * b + 1)].at (lat + 1);
                }
        }
        samplePos += n;
    }

    void midiOutFor (std::vector<MidiOut>& out, int outOff, int n)
    {
        const int ch = std::clamp (P.g.midiChannel, 1, 16);
        const int gate = std::max (1, (int) (clampf (P.g.midiGateMs, 1.0f, 2000.0f) * 0.001f * fs));
        std::vector<MidiOut>& o = out;
        const size_t first = o.size();
        // Los note-off que vencen en este tramo. Si el mismo slot vuelve a sonar antes, el note-off va justo antes de
        // ese note-on (lo pone el lazo de abajo).
        for (int s = 0; s < kNumSlots; ++s)
        {
            auto& p = midiPending[(size_t) s];
            if (p.first < 0) continue;
            int hs = 1 << 30;
            if (P.g.midiOut) for (const auto& h : hits) if (h.fromSeq && h.slot == s) { hs = h.offset; break; }
            if (p.second < n && p.second <= hs) { o.push_back ({ outOff + std::max (0, p.second), p.channel, p.first, 0 }); p = {}; }
            else if (hs == (1 << 30)) p.second -= n;
        }
        if (P.g.midiOut)
            for (size_t k = 0; k < hits.size(); ++k)
            {
                const auto& h = hits[k];
                if (! h.fromSeq) continue;
                const int note = slotToNote (h.slot, P.g.noteMap);
                auto& p = midiPending[(size_t) h.slot];
                if (p.first >= 0) { o.push_back ({ outOff + h.offset, p.channel, p.first, 0 }); p = {}; }
                const int vel = h.accent ? 127 : std::clamp ((int) std::lround (h.vel * 127.0f), 1, 127);
                o.push_back ({ outOff + h.offset, ch, note, vel });
                // La compuerta: 30 ms (Gate), o hasta el golpe siguiente del mismo slot (ratchets, flams).
                int offAt = h.offset + gate;
                for (size_t j = k + 1; j < hits.size(); ++j) if (hits[j].fromSeq && hits[j].slot == h.slot) { offAt = std::min (offAt, hits[j].offset); break; }
                if (offAt < n) o.push_back ({ outOff + offAt, ch, note, 0 });
                else p = { note, offAt - n, ch };
            }
        std::stable_sort (o.begin() + (std::ptrdiff_t) first, o.end(), [] (const MidiOut& a, const MidiOut& b) { return a.offset < b.offset; });
    }
};

} // namespace dh::pal
