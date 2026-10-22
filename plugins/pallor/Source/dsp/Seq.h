#pragma once
// El secuenciador de Pallor (docs/research.md §4, §7).
//
// Datos: 16 grupos de patrones, cada uno con las variaciones A, B y Fill. Un patrón tiene 16 pistas de hasta 64 pasos,
// un largo maestro (cuántos pasos dura antes de pasar al siguiente) y una escala (1/32, 1/16, 1/8, 1/16T, 1/8T). Cada
// pista puede tener su propio largo (polimetría: una pista de 12 pasos sobre un compás de 16 se corre un paso cada vuelta).
// Cada paso: velocidad (0 = apagado), acento, probabilidad, repeticiones (ratchet), flam, corrimiento (±50 % del paso) y
// una condición (sólo en el fill, nunca en el fill, la 1.ª de cada 2, …).
//
// Tiempo: todo en negras (ppq) de la línea de tiempo del host. La reproducción es una sucesión de segmentos: un segmento
// es una pasada del patrón (su largo maestro × su paso). Qué segmento suena en una posición es una función pura de la
// posición y de los ajustes (modo A, B, A>B alternado o canción; fill automático cada N): al arrancar o después de un
// salto del host se recalcula desde ahí. Tocando, el segmento siguiente se decide medio pulso antes de que empiece (así
// un flam o un corrimiento negativo del primer paso pueden sonar antes de la barra) con los ajustes de ese momento: un
// cambio de patrón entra en la barra siguiente.
//
// Swing (como en las cajas de 1979-88): cada par de semicorcheas (o de corcheas) se deforma en el tiempo, la segunda
// cae al `swing` del par (50 % derecho, 66,7 % tresillo, 75 % puntillo). Se aplica a la posición absoluta, así queda
// atado a los tiempos del compás.
//
// Exactitud a la muestra: el motor llama una vez por bloque con la posición del host al empezar el bloque. Las
// ventanas de bloques seguidos se encadenan (la ventana nueva empieza donde terminó la anterior, aunque el host deje
// una rendija de redondeo), así ningún golpe se pierde ni se duplica (la lección T2b del AT303). Cada golpe se ubica en
// la muestra más cercana a su posición.

#include "Base.h"

namespace dh::pal {

constexpr int kTracks = 16, kMaxSteps = 64, kGroups = 16, kPatterns = kGroups * 3, kMaxChain = 64;

enum StepFlag : uint8_t { kAccent = 1 };
enum Cond : uint8_t { cAlways, cFill, cNotFill, c1of2, c2of2, c1of4, c4of4, cFirst, cNotFirst, kNumConds };
inline const char* condName (int c) { static const char* n[] = { "Always", "Fill", "Not fill", "1:2", "2:2", "1:4", "4:4", "First", "Not first" }; return n[std::clamp (c, 0, (int) kNumConds - 1)]; }

struct Step
{
    uint8_t vel = 0, flags = 0, prob = 100, ratchet = 1, flam = 0, cond = 0;   // flam en ms (0 = sin flam)
    int8_t nudge = 0;                                                           // % del paso, −50..50
    uint8_t pad = 0;
    bool on() const { return vel > 0; }
};
struct Track { std::array<Step, kMaxSteps> s {}; uint8_t len = 0; };          // len 0 = el largo del patrón
struct Pattern
{
    std::array<Track, kTracks> t {};
    uint8_t length = 16, scale = 1;
    uint8_t swing = 0;          // 0 = el Swing general; 50..75 = el de este patrón
    bool isEmpty() const { for (const auto& tr : t) for (const auto& s : tr.s) if (s.vel) return false; return true; }
};
inline double scaleBeats (int sc) { static const double b[5] = { 0.125, 0.25, 0.5, 1.0 / 6.0, 1.0 / 3.0 }; return b[std::clamp (sc, 0, 4)]; }
inline const char* scaleName (int sc) { static const char* n[5] = { "1/32", "1/16", "1/8", "1/16T", "1/8T" }; return n[std::clamp (sc, 0, 4)]; }
inline bool scaleSwings (int sc) { return sc <= 2; }

struct ChainEntry { uint8_t group = 0, var = 0, repeats = 1; bool fill = false; };
struct Bank
{
    std::array<Pattern, kPatterns> p {};
    std::array<ChainEntry, kMaxChain> chain {};
    int chainLen = 0;
};
inline int patIndex (int group, int var) { return std::clamp (group, 0, kGroups - 1) * 3 + std::clamp (var, 0, 2); }

struct SeqSettings
{
    int mode = 0;               // 0 A, 1 B, 2 A>B (alterna), 3 canción
    int group = 0;
    bool fill = false;          // fill a mano: el segmento siguiente es un fill (mientras esté prendido)
    int autofill = 0;           // 0 apagado; si no, el último de cada N segmentos es fill
    float swing = 0.5f;         // 0.5 … 0.75
    int swingRes = 1;           // 0 corcheas, 1 semicorcheas
    uint32_t seed = 1;          // probabilidad
    int forcePattern = -1;      // ≥ 0: sólo ese patrón en bucle (exportar, escuchar uno)
};

struct Segment
{
    double start = 0, end = 0, entryStart = 0, sb = 0.25;
    int pat = 0, basePat = 0, var = 0, group = 0, iter = 0, entry = -1, rep = 0, mode = 0, len = 16;
    int64_t k = 0; bool fill = false;
};

// Deformación del swing sobre la posición absoluta.
inline double swingWarp (double ppq, float swing, int res)
{
    if (swing <= 0.5001f) return ppq;
    const double pl = res == 0 ? 1.0 : 0.5;
    const double u = ppq / pl, fl = std::floor (u), f = u - fl, s = (double) clampf (swing, 0.5f, 0.75f);
    const double w = f < 0.5 ? f * 2.0 * s : s + (f - 0.5) * 2.0 * (1.0 - s);
    return (fl + w) * pl;
}

class Sequencer
{
public:
    void setBank (const Bank* b) { bank = b; }
    void reset() { nSeg = 0; haveLast = false; }

    // Bloque del host: posición c0 (ppq) de la primera muestra, n muestras, rate = ppq por muestra. emit (ppq, pista,
    // velocidad 0..1, acento) para cada golpe que cae en el bloque, en orden de pista y de paso (no de tiempo).
    template <typename F> void advance (double c0, int n, double rate, double bpm, const SeqSettings& st, F&& emit)
    {
        const double tol = std::max (2.0 * rate, 1e-7);
        double w0;
        if (haveLast && std::fabs (c0 - lastC1) <= tol) w0 = lastW1;
        else { nSeg = 0; jumped = true; w0 = c0 - 0.5 * rate; }
        const double w1 = c0 + ((double) n - 0.5) * rate;
        haveLast = true; lastC1 = c0 + (double) n * rate; lastW1 = w1;
        window (w0, w1, bpm, st, emit);
    }
    // Una ventana suelta [w0, w1) desde cero (exportar a .mid, pruebas).
    template <typename F> void collect (double w0, double w1, double bpm, const SeqSettings& st, F&& emit)
    {
        nSeg = 0; window (w0, w1, bpm, st, emit); nSeg = 0; haveLast = false;
    }
    bool consumeJump() { const bool j = jumped; jumped = false; return j; }

    // Para la interfaz: el segmento que suena en `ppq` (o nullptr).
    const Segment* segmentAt (double ppq) const
    {
        for (int i = 0; i < nSeg; ++i) { const auto& s = seg[(size_t) ((head + i) % kRing)]; if (ppq >= s.start && ppq < s.end) return &s; }
        return nullptr;
    }
    // Paso de una pista en `ppq` dentro del segmento (−1 si no hay).
    int stepOf (const Segment& s, int track, double ppq) const
    {
        if (bank == nullptr) return -1;
        const auto& pt = bank->p[(size_t) s.pat];
        const int i = (int) std::floor ((ppq - s.start) / s.sb);
        const int Lt = trackLen (pt, track);
        const int64_t g = std::llround ((s.start - s.entryStart) / s.sb) + i;
        return (int) (((g % Lt) + Lt) % Lt);
    }
    static int trackLen (const Pattern& p, int track) { const int l = p.t[(size_t) track].len ? p.t[(size_t) track].len : p.length; return std::clamp (l, 1, kMaxSteps); }
    double patternBeats (int pat) const { const auto& p = bank->p[(size_t) pat]; return std::max (1, (int) p.length) * scaleBeats (p.scale); }

private:
    static constexpr int kRing = 64;
    const Bank* bank = nullptr;
    std::array<Segment, kRing> seg {}; int head = 0, nSeg = 0;
    bool haveLast = false, jumped = false; double lastC1 = 0, lastW1 = 0;

    Segment& at (int i) { return seg[(size_t) ((head + i) % kRing)]; }

    bool autofill (int64_t k, int N) const { return N > 0 && (((k % N) + N) % N) == N - 1; }
    bool fillEmpty (int group) const { return bank->p[(size_t) patIndex (group, 2)].isEmpty(); }

    void finalize (Segment& s, bool fillWanted) const
    {
        s.fill = fillWanted;
        s.pat = s.basePat;
        if (fillWanted && s.mode != 99 && ! fillEmpty (s.group)) s.pat = patIndex (s.group, 2);
        s.sb = scaleBeats (bank->p[(size_t) s.pat].scale);
        s.len = std::max (1, (int) std::ceil ((s.end - s.start) / s.sb - 1e-9));
    }

    int effMode (const SeqSettings& st) const
    {
        if (st.forcePattern >= 0) return 99;
        if (st.mode == 3 && bank->chainLen <= 0) return 0;
        return std::clamp (st.mode, 0, 3);
    }

    // El segmento que suena en x, desde cero (función pura de x y de los ajustes).
    Segment pure (double x, const SeqSettings& st) const
    {
        Segment s; s.mode = effMode (st);
        bool fill = false;
        if (s.mode == 99 || s.mode == 0 || s.mode == 1)
        {
            s.basePat = s.mode == 99 ? std::clamp (st.forcePattern, 0, kPatterns - 1) : patIndex (st.group, s.mode);
            s.group = s.basePat / 3; s.var = s.basePat % 3;
            const double d = patternBeats (s.basePat);
            s.k = (int64_t) std::floor (x / d);
            s.start = (double) s.k * d; s.end = s.start + d; s.entryStart = 0; s.iter = (int) s.k;
            fill = s.mode != 99 && (st.fill || autofill (s.k, st.autofill));
        }
        else if (s.mode == 2)
        {
            const int a = patIndex (st.group, 0), b = patIndex (st.group, 1);
            const double dA = patternBeats (a), dB = patternBeats (b), cyc = dA + dB;
            const int64_t c = (int64_t) std::floor (x / cyc);
            const double r = x - (double) c * cyc;
            s.group = st.group;
            if (r < dA) { s.var = 0; s.basePat = a; s.start = (double) c * cyc; s.end = s.start + dA; s.k = 2 * c; }
            else { s.var = 1; s.basePat = b; s.start = (double) c * cyc + dA; s.end = s.start + dB; s.k = 2 * c + 1; }
            s.entryStart = s.start; s.iter = (int) c;
            fill = st.fill || autofill (s.k, st.autofill);
        }
        else
        {
            double total = 0; int reps = 0;
            for (int e = 0; e < bank->chainLen; ++e) { const auto& ce = bank->chain[(size_t) e]; total += std::max (1, (int) ce.repeats) * patternBeats (patIndex (ce.group, ce.var)); reps += std::max (1, (int) ce.repeats); }
            const int64_t c = (int64_t) std::floor (x / total);
            double r = x - (double) c * total, acc = (double) c * total; int64_t kk = c * reps;
            for (int e = 0; e < bank->chainLen; ++e)
            {
                const auto& ce = bank->chain[(size_t) e];
                const int rp = std::max (1, (int) ce.repeats);
                const double d = patternBeats (patIndex (ce.group, ce.var));
                if (r < d * rp || e == bank->chainLen - 1)
                {
                    const int rep = std::clamp ((int) std::floor (r / d), 0, rp - 1);
                    s.entry = e; s.rep = rep; s.group = ce.group; s.var = ce.var; s.basePat = patIndex (ce.group, ce.var);
                    s.entryStart = acc; s.start = acc + rep * d; s.end = s.start + d; s.iter = rep; s.k = kk + rep;
                    fill = (ce.fill && rep == rp - 1) || st.fill;
                    break;
                }
                r -= d * rp; acc += d * rp; kk += rp;
            }
        }
        finalize (s, fill);
        return s;
    }

    // El segmento que sigue a `p`, con los ajustes de ahora.
    Segment next (const Segment& p, const SeqSettings& st) const
    {
        Segment s; s.mode = effMode (st); s.start = p.end; s.k = p.k + 1;
        bool fill = false;
        if (s.mode == 99 || s.mode == 0 || s.mode == 1)
        {
            s.basePat = s.mode == 99 ? std::clamp (st.forcePattern, 0, kPatterns - 1) : patIndex (st.group, s.mode);
            s.group = s.basePat / 3; s.var = s.basePat % 3;
            const bool same = p.basePat == s.basePat && p.mode == s.mode;
            s.entryStart = same ? p.entryStart : s.start; s.iter = same ? p.iter + 1 : 0;
            s.end = s.start + patternBeats (s.basePat);
            fill = s.mode != 99 && (st.fill || autofill (s.k, st.autofill));
        }
        else if (s.mode == 2)
        {
            const bool cont = p.mode == 2 && p.group == st.group;
            s.var = cont ? 1 - p.var : 0; s.group = st.group; s.basePat = patIndex (st.group, s.var);
            s.entryStart = s.start; s.iter = s.var == 0 ? (cont ? p.iter + 1 : 0) : (cont ? p.iter : 0);
            s.end = s.start + patternBeats (s.basePat);
            fill = st.fill || autofill (s.k, st.autofill);
        }
        else
        {
            int e = 0, rep = 0;
            if (p.mode == 3 && p.entry >= 0 && p.entry < bank->chainLen)
            {
                e = p.entry; rep = p.rep + 1;
                if (rep >= std::max (1, (int) bank->chain[(size_t) e].repeats)) { e = (e + 1) % bank->chainLen; rep = 0; }
            }
            const auto& ce = bank->chain[(size_t) e];
            s.entry = e; s.rep = rep; s.group = ce.group; s.var = ce.var; s.basePat = patIndex (ce.group, ce.var);
            s.entryStart = rep == 0 ? s.start : p.entryStart; s.iter = rep;
            s.end = s.start + patternBeats (s.basePat);
            fill = (ce.fill && rep == std::max (1, (int) ce.repeats) - 1) || st.fill;
        }
        finalize (s, fill);
        return s;
    }

    template <typename F> void window (double w0, double w1, double bpm, const SeqSettings& st, F&& emit)
    {
        if (bank == nullptr || w1 <= w0) return;
        const double ahead = 0.5;
        if (nSeg == 0) { head = 0; seg[0] = pure (w0, st); nSeg = 1; }
        while (at (nSeg - 1).end < w1 + ahead)
        {
            const Segment nx = next (at (nSeg - 1), st);
            if (nSeg == kRing) { head = (head + 1) % kRing; --nSeg; }
            at (nSeg) = nx; ++nSeg;
        }
        for (int i = 0; i < nSeg; ++i) emitSegment (at (i), w0, w1, bpm, st, emit);
        // Los segmentos que ya no pueden aportar golpes (lo más tardío de un paso: ratchet + corrimiento + swing).
        while (nSeg > 1 && at (0).end < w0 - (2.0 * at (0).sb + 0.4)) { head = (head + 1) % kRing; --nSeg; }
    }

    template <typename F> void emitSegment (const Segment& s, double w0, double w1, double bpm, const SeqSettings& st, F&& emit)
    {
        const double late = 2.0 * s.sb + 0.3, early = 0.5 * s.sb + 0.35;
        const double lo = std::max (s.start, w0 - late), hi = std::min (s.end, w1 + early);
        if (hi <= lo) return;
        const int i0 = std::max (0, (int) std::ceil ((lo - s.start) / s.sb - 1e-9));
        const int i1 = std::min (s.len, (int) std::ceil ((hi - s.start) / s.sb - 1e-9));
        if (i1 <= i0) return;
        const Pattern& pt = bank->p[(size_t) s.pat];
        const bool swings = scaleSwings (pt.scale);
        const float sw = pt.swing >= 50 ? clampf ((float) pt.swing * 0.01f, 0.5f, 0.75f) : st.swing;
        const int64_t off = std::llround ((s.start - s.entryStart) / s.sb);
        const double flamBeats = bpm / 60.0e3;      // por ms
        auto put = [&] (double te, int tr, float v, bool acc) { if (te >= w0 && te < w1 && te >= 0.0) emit (te, tr, v, acc); };
        for (int tr = 0; tr < kTracks; ++tr)
        {
            const Track& T = pt.t[(size_t) tr];
            const int Lt = trackLen (pt, tr);
            for (int i = i0; i < i1; ++i)
            {
                const int64_t g = off + i;
                const Step& sp = T.s[(size_t) (((g % Lt) + Lt) % Lt)];
                if (! sp.vel) continue;
                if (! condOk (sp.cond, s)) continue;
                const double nominal = s.start + (double) i * s.sb;
                if (sp.prob < 100 && hashUni (st.seed, (uint64_t) std::llround (nominal * 960.0 + 1.0e9), (uint64_t) tr) * 100.0 >= (double) sp.prob) continue;
                double t0 = swings ? swingWarp (nominal, sw, st.swingRes) : nominal;
                t0 += (double) sp.nudge * 0.01 * s.sb;
                const float v = (float) sp.vel / 127.0f;
                const bool acc = (sp.flags & kAccent) != 0;
                const int r = std::clamp ((int) sp.ratchet, 1, 8);
                for (int j = 0; j < r; ++j) put (t0 + (double) j * s.sb / r, tr, v, acc);
                if (sp.flam) put (t0 - (double) sp.flam * flamBeats, tr, v * 0.55f, false);
            }
        }
    }

    static bool condOk (uint8_t c, const Segment& s)
    {
        switch (c)
        {
            case cFill: return s.fill;
            case cNotFill: return ! s.fill;
            case c1of2: return s.iter % 2 == 0;
            case c2of2: return s.iter % 2 == 1;
            case c1of4: return s.iter % 4 == 0;
            case c4of4: return s.iter % 4 == 3;
            case cFirst: return s.iter == 0;
            case cNotFirst: return s.iter != 0;
            default: return true;
        }
    }
};

} // namespace dh::pal
