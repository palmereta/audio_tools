#pragma once
// Digitación (docs/research.md §5): cada nota a una cuerda y un traste reales, los acordes que entran por MIDI
// reconocidos por plantillas y llevados a voicings de guitarra buscados sobre la afinación real, dentro de una región
// del diapasón. Todo determinista: la misma entrada da la misma digitación.
//
// Nota suelta: costo por posición = distancia a la mano (fuera de la ventana de cuatro trastes), cambio de cuerda,
// preferencia leve por los trastes bajos, cuerdas al aire según el gusto, cuerda preferida y posición fija del
// usuario, y una penalidad grande si la cuerda ya está sonando una nota sostenida (la cortaría).
// Notas simultáneas: búsqueda en profundidad de cuerdas distintas con el menor costo total y un estiramiento de la
// mano posible (cinco trastes como mucho).
// Acordes: plantillas de intervalos (tríada, séptimas, sus, add9, novenas, power) con la raíz y el bajo; el voicing es
// la combinación de cuerdas (apagada, al aire o pisada en la ventana) que da las notas del acorde con el bajo pedido,
// cuerdas contiguas, cuatro dedos (la cejilla cuenta como uno) y el estilo elegido (abierto, con cejilla, power,
// tríadas, jazz), con la ventana más cercana a la región.

#include "Guitar.h"

namespace dh::gt {

struct FretPos { int string = -1, fret = -1; bool valid() const { return string >= 0; } };

struct Tab
{
    int n = 6;
    std::array<int, kMaxStrings> open {};      // MIDI al aire (con capo)
    int frets = 22;
};

// ── Notas sueltas ──
struct FingerPrefs
{
    int fixedPosition = -1;        // traste del índice fijo (−1 = la mano se mueve sola)
    int preferredString = -1;      // cuerda preferida (índice interno, −1 = cualquiera)
    float openStrings = 0.5f;      // 0 las evita … 1 las busca
    float lowPositions = 0.5f;     // preferencia por las posiciones bajas
};

class Fingerer
{
public:
    void setTab (const Tab& t) { tab = t; }
    const Tab& getTab() const { return tab; }
    void reset() { hand = 1.0f; lastString = -1; }
    float handPosition() const { return hand; }
    void setHand (float h) { hand = h; }

    float cost (int s, int f, uint32_t busy, const FingerPrefs& p) const
    {
        if (f < 0 || f > tab.frets) return 1e9f;
        float c = 0.0f;
        if (busy & (1u << s)) c += 50.0f;
        // Con la mano fija, la ventana es la del usuario (y las cuerdas al aire sólo valen en las posiciones bajas).
        const float h = p.fixedPosition >= 0 ? (float) p.fixedPosition : hand;
        if (f == 0)
        {
            c += 1.2f * (1.0f - p.openStrings) - 0.4f * p.openStrings;
            if (p.fixedPosition > 2) c += 6.0f * ((float) p.fixedPosition - 2.0f);
        }
        else
        {
            const float lo = h, hi = h + 3.0f;
            if (f < lo) c += 1.4f * (lo - (float) f);
            else if (f > hi) c += 1.4f * ((float) f - hi);
            c += 0.06f * (float) f * p.lowPositions;
            if (p.fixedPosition >= 0)
            {
                const float flo = (float) p.fixedPosition - 0.5f, fhi = (float) p.fixedPosition + 3.5f;
                if ((float) f < flo) c += 6.0f * (flo - (float) f); else if ((float) f > fhi) c += 6.0f * ((float) f - fhi);
            }
        }
        if (lastString >= 0) c += 0.25f * (float) std::abs (s - lastString);
        if (p.preferredString >= 0 && s == p.preferredString) c -= 40.0f;      // la cuerda pedida manda si la nota entra
        return c;
    }

    // La mejor posición para una nota (busy: cuerdas con notas sostenidas que no conviene cortar).
    FretPos place (int note, uint32_t busy, const FingerPrefs& p) const
    {
        FretPos best; float bc = 1e8f;
        for (int s = 0; s < tab.n; ++s)
        {
            const int f = note - tab.open[(size_t) s];
            if (f < 0 || f > tab.frets) continue;
            const float c = cost (s, f, busy, p);
            if (c < bc) { bc = c; best = { s, f }; }
        }
        return best;
    }
    // Mover la mano a una nota tocada (la ventana sigue al dedo que salió de ella).
    void commit (const FretPos& fp)
    {
        if (! fp.valid()) return;
        lastString = fp.string;
        if (fp.fret > 0)
        {
            if ((float) fp.fret < hand) hand = (float) fp.fret;
            else if ((float) fp.fret > hand + 3.0f) hand = (float) fp.fret - 3.0f;
        }
    }

    // Varias notas a la vez en cuerdas distintas (las más graves en las cuerdas más graves cuando se puede).
    int placeChord (const int* notes, int count, uint32_t busy, const FingerPrefs& p, FretPos* out) const
    {
        count = std::min (count, tab.n);
        std::array<int, kMaxStrings> order {};
        for (int i = 0; i < count; ++i) order[(size_t) i] = i;
        std::sort (order.begin(), order.begin() + count, [&] (int a, int b) { return notes[a] < notes[b]; });
        std::array<int, kMaxStrings> cur {}, best {}; cur.fill (-1); best.fill (-1);
        float bestC = 1e8f;
        auto dfs = [&] (auto& self, int i, uint32_t used, float c, int fmin, int fmax) -> void {
            if (c >= bestC) return;
            if (i == count) { bestC = c; best = cur; return; }
            const int note = notes[order[(size_t) i]];
            for (int s = 0; s < tab.n; ++s)
            {
                if (used & (1u << s)) continue;
                const int f = note - tab.open[(size_t) s];
                if (f < 0 || f > tab.frets) continue;
                int nmin = fmin, nmax = fmax;
                if (f > 0) { nmin = std::min (nmin, f); nmax = std::max (nmax, f); }
                if (nmax - nmin > 4) continue;
                float cc = cost (s, f, busy, p) * 0.5f;
                // Cruzar cuerdas (una nota más aguda en una cuerda más grave) es raro en una mano real.
                for (int j = 0; j < i; ++j) { const int sj = cur[(size_t) order[(size_t) j]]; if (sj > s) cc += 2.5f; }
                if (nmax - nmin == 4) cc += 1.5f;
                cur[(size_t) order[(size_t) i]] = s;
                self (self, i + 1, used | (1u << s), c + cc, nmin, nmax);
                cur[(size_t) order[(size_t) i]] = -1;
            }
        };
        dfs (dfs, 0, 0u, 0.0f, 99, -1);
        int placed = 0;
        for (int i = 0; i < count; ++i)
        {
            const int s = best[(size_t) i];
            out[i] = s >= 0 ? FretPos { s, notes[i] - tab.open[(size_t) s] } : FretPos {};
            placed += s >= 0;
        }
        // Lo que no entró (más notas que cuerdas, o fuera de alcance) va solo, en la mejor cuerda libre.
        uint32_t used = 0; for (int i = 0; i < count; ++i) if (out[i].valid()) used |= 1u << out[i].string;
        for (int i = 0; i < count; ++i)
            if (! out[i].valid()) { out[i] = place (notes[i], busy | used, p); if (out[i].valid()) { used |= 1u << out[i].string; ++placed; } }
        return placed;
    }

private:
    Tab tab;
    float hand = 1.0f;
    int lastString = -1;
};

// ── Acordes ──
struct ChordType { const char* suffix; uint16_t mask; uint16_t essential; };   // máscaras de intervalos desde la raíz
inline const std::array<ChordType, 22>& chordTypes()
{
    // essential: los intervalos que un voicing no puede omitir (la quinta justa se puede omitir casi siempre).
    static const std::array<ChordType, 22> t {{
        { "",      0b000010010001, 0b000000010001 },   // mayor 0 4 7
        { "m",     0b000010001001, 0b000000001001 },   // menor 0 3 7
        { "5",     0b000010000001, 0b000010000001 },   // power 0 7
        { "7",     0b010010010001, 0b010000010001 },
        { "maj7",  0b100010010001, 0b100000010001 },
        { "m7",    0b010010001001, 0b010000001001 },
        { "sus4",  0b000010100001, 0b000000100001 },
        { "sus2",  0b000010000101, 0b000000000101 },
        { "dim",   0b000001001001, 0b000001001001 },
        { "aug",   0b000100010001, 0b000100010001 },
        { "6",     0b001010010001, 0b001000010001 },
        { "m6",    0b001010001001, 0b001000001001 },
        { "add9",  0b000010010101, 0b000000010101 },
        { "madd9", 0b000010001101, 0b000000001101 },
        { "9",     0b010010010101, 0b010000010101 },
        { "maj9",  0b100010010101, 0b100000010101 },
        { "m9",    0b010010001101, 0b010000001101 },
        { "m7b5",  0b010001001001, 0b010001001001 },
        { "dim7",  0b001001001001, 0b001001001001 },
        { "7sus4", 0b010010100001, 0b010000100001 },
        { "mMaj7", 0b100010001001, 0b100000001001 },
        { "7#9",   0b010010011001, 0b010000011001 },
    }};
    return t;
}

struct Chord
{
    int root = -1, type = 0, bass = -1;          // clases de altura 0..11 (C = 0)
    uint16_t pcs = 0;                            // clases de altura del acorde (máscara absoluta)
    bool valid() const { return root >= 0; }
    bool operator== (const Chord& o) const { return root == o.root && type == o.type && bass == o.bass; }
};
inline const char* pcName (int pc) { static const char* n[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }; return n[((pc % 12) + 12) % 12]; }
inline uint16_t rotate12 (uint16_t m, int r) { r = ((r % 12) + 12) % 12; return (uint16_t) (((m << r) | (m >> (12 - r))) & 0xFFF); }

// Reconocer un acorde por las notas que suenan (single: qué hacer con una sola nota: 0 mayor, 1 power, 2 la nota).
inline Chord detectChord (const int* notes, int count, int single = 0)
{
    Chord c;
    if (count <= 0) return c;
    uint16_t in = 0; int lowest = 999;
    for (int i = 0; i < count; ++i) { in |= (uint16_t) (1u << (notes[i] % 12)); lowest = std::min (lowest, notes[i]); }
    const int bass = lowest % 12;
    const int nPc = popc (in);
    if (nPc == 1)
    {
        c.root = bass; c.bass = bass;
        c.type = single == 1 ? 2 : 0;
        c.pcs = single == 2 ? in : rotate12 (chordTypes()[(size_t) c.type].mask, bass);
        return c;
    }
    float best = -1e9f;
    for (int r = 0; r < 12; ++r)
    {
        if (! (in & (1u << r))) continue;
        for (int t = 0; t < (int) chordTypes().size(); ++t)
        {
            const uint16_t tm = rotate12 (chordTypes()[(size_t) t].mask, r);
            const int matched = popc (in & tm), extra = popc (in & ~tm), missing = popc (tm & ~in);
            const bool fifthMissing = (tm & ~in) == rotate12 (0b000010000000, r);
            float sc = 2.0f * matched - 3.0f * extra - (fifthMissing ? 0.4f : 1.2f) * missing - 0.05f * t;
            if (r == bass) sc += 0.8f;
            if (sc > best) { best = sc; c.root = r; c.type = t; c.pcs = tm; }
        }
    }
    c.bass = bass;
    return c;
}
inline std::string chordName (const Chord& c)
{
    if (! c.valid()) return "-";
    std::string s = std::string (pcName (c.root)) + chordTypes()[(size_t) c.type].suffix;
    if (c.bass >= 0 && c.bass != c.root) s += std::string ("/") + pcName (c.bass);
    return s;
}

enum VoicingStyle { kAuto, kOpen, kBarre, kPower, kTriad, kJazz, kNumStyles };
inline const char* styleName (int i) { static const char* n[] = { "Auto", "Open", "Barre", "Power", "Triads", "Jazz" }; return n[std::clamp (i, 0, 5)]; }

struct Voicing
{
    std::array<int, kMaxStrings> fret {};        // −1 = apagada
    bool valid = false;
    int lowestString() const { for (int s = 0; s < kMaxStrings; ++s) if (fret[(size_t) s] >= 0) return s; return -1; }
    int highestString() const { for (int s = kMaxStrings - 1; s >= 0; --s) if (fret[(size_t) s] >= 0) return s; return -1; }
    int count() const { int n = 0; for (int f : fret) n += f >= 0; return n; }
};

// Buscar el voicing: cada cuerda apagada, al aire o pisada en la ventana [lo, lo + 3] (+1 con estiramiento).
inline Voicing findVoicing (const Tab& tab, const Chord& chIn, int style, int region)
{
    Voicing best; best.fret.fill (-1);
    if (! chIn.valid()) return best;
    if (style == kAuto) style = region <= 3 ? kOpen : kBarre;
    // Power: raíz y quinta (y su octava) sea cual sea el acorde; el bajo, la raíz.
    Chord ch = chIn;
    if (style == kPower) { ch.type = 2; ch.pcs = (uint16_t) ((1u << ch.root) | (1u << ((ch.root + 7) % 12))); ch.bass = ch.root; }
    const auto& ct = chordTypes()[(size_t) ch.type];
    const uint16_t essential = rotate12 (ct.essential, ch.root);
    const bool allowOpen = style == kOpen || region <= 3;
    const int third = (ch.pcs & rotate12 (0b10000, ch.root)) ? (ch.root + 4) % 12 : (ch.pcs & rotate12 (0b1000, ch.root)) ? (ch.root + 3) % 12 : -1;
    float bestC = 1e9f;
    const int nS = tab.n;
    std::array<int, kMaxStrings> cur {}; cur.fill (-1);
    const int loMin = style == kOpen ? 1 : std::max (1, region - 3), loMax = std::min (tab.frets - 3, (style == kOpen ? 4 : region + 4));
    for (int lo = loMin; lo <= std::max (loMin, loMax); ++lo)
    {
        const int hiF = lo + 3;
        if (style == kPower && lo > region + 4) break;
        // Opciones por cuerda.
        std::array<std::array<int, 6>, kMaxStrings> opt {}; std::array<int, kMaxStrings> nOpt {};
        for (int s = 0; s < nS; ++s)
        {
            int k = 0; opt[(size_t) s][(size_t) k++] = -1;
            if (allowOpen && (ch.pcs & (1u << (tab.open[(size_t) s] % 12)))) opt[(size_t) s][(size_t) k++] = 0;
            for (int f = lo; f <= std::min (hiF, tab.frets) && k < 6; ++f)
                if (ch.pcs & (1u << ((tab.open[(size_t) s] + f) % 12))) opt[(size_t) s][(size_t) k++] = f;
            nOpt[(size_t) s] = k;
        }
        const int maxGaps = style == kJazz ? 1 : 0;
        // Recorrido de la cuerda más grave a la más aguda con poda: la primera que suena tiene que dar el bajo, los
        // huecos están limitados por el estilo y la mano abarca cuatro trastes.
        auto rec = [&] (auto& self, int s, int lowS, int gaps, bool pendingGap, int fmin, int fmax) -> void {
            if (s < nS)
            {
                for (int k = 0; k < nOpt[(size_t) s]; ++k)
                {
                    const int f = opt[(size_t) s][(size_t) k];
                    if (f < 0)
                    {
                        cur[(size_t) s] = -1;
                        self (self, s + 1, lowS, gaps, lowS >= 0 ? true : false, fmin, fmax);
                        continue;
                    }
                    int ng = gaps;
                    if (pendingGap) { if (++ng > maxGaps) continue; }
                    if (lowS < 0 && (tab.open[(size_t) s] + f) % 12 != (ch.bass >= 0 ? ch.bass : ch.root)) continue;
                    int nmin = fmin, nmax = fmax;
                    if (f > 0) { nmin = std::min (nmin, f); nmax = std::max (nmax, f); if (nmax - nmin > 3) continue; }
                    cur[(size_t) s] = f;
                    self (self, s + 1, lowS < 0 ? s : lowS, ng, false, nmin, nmax);
                }
                cur[(size_t) s] = -1;
                return;
            }
            // Evaluar.
            int highS = -1, count = 0, fretted = 0, opens = 0; fmin = 99; fmax = 0;
            uint16_t got = 0; int thirds = 0;
            for (int t = 0; t < nS; ++t)
            {
                const int f = cur[(size_t) t];
                if (f < 0) continue;
                highS = t; ++count;
                const int pc = (tab.open[(size_t) t] + f) % 12;
                got |= (uint16_t) (1u << pc);
                if (pc == third) ++thirds;
                if (f > 0) { fmin = std::min (fmin, f); fmax = std::max (fmax, f); ++fretted; } else ++opens;
            }
            if (count < 2 || lowS < 0) return;
            if ((got & essential) != essential) return;
            // Dedos: la cejilla en el traste más bajo cubre todas las cuerdas pisadas en ese traste.
            int atMin = 0; for (int t = lowS; t <= highS; ++t) atMin += cur[(size_t) t] == fmin;
            const int fingers = fretted == 0 ? 0 : fretted - (atMin > 1 ? atMin - 1 : 0);
            if (fingers > 4) return;
            float c = 0.0f;
            const int span = highS - lowS + 1;
            switch (style)
            {
                case kOpen:
                    c += 0.9f * (float) (6 - std::min (6, count)) - 0.6f * (float) opens + (fretted ? 0.5f * (float) fmin : 0.0f);
                    if (fmax > 4) c += 3.0f;
                    break;
                case kBarre:
                    if (opens > 0) return;
                    c += 1.2f * (float) (std::min (5, nS) - std::min (count, 5)) + 0.6f * (float) std::abs (fmin - region);
                    if (atMin < 2) c += 2.0f;
                    break;
                case kPower:
                    if (count > 3 || (got & ~(uint16_t) ((1u << ch.root) | (1u << ((ch.root + 7) % 12)))) != 0) return;
                    c += 0.5f * (float) std::abs ((fretted ? fmin : 0) - region) + 1.2f * (float) lowS - 0.3f * (float) count;
                    if (opens > 0 && fretted > 0 && cur[(size_t) lowS] != 0) c += 2.0f;    // al aire mezclada arriba: raro
                    break;
                case kTriad:
                    if (count != 3) return;
                    c += 0.5f * (float) std::abs ((fretted ? fmin : 0) - region) + 0.25f * (float) std::abs (lowS - (nS - 4));
                    break;
                case kJazz:
                    if (count < 3 || count > 5 || opens > 0) return;
                    c += 0.5f * (float) std::abs (fmin - region) + (count == 4 ? 0.0f : 1.0f) + 0.6f * (float) gaps;
                    if (popc (got) < count) c += 2.0f;            // sin notas dobladas
                    break;
                default: break;
            }
            c += 0.8f * (float) std::max (0, thirds - 1);                      // la tercera doblada suena gruesa
            const int missing = popc (ch.pcs) - popc (got);
            if (missing > 0) c += 0.7f * (float) missing;
            c += 0.05f * (float) span * (style == kTriad ? 1.0f : -1.0f);
            if (c < bestC) { bestC = c; best.fret = cur; best.valid = true; }
        };
        rec (rec, 0, -1, 0, false, 99, 0);
    }
    // Sin resultado con el estilo pedido: probar con el más permisivo.
    if (! best.valid && style != kOpen && style != kBarre) return findVoicing (tab, chIn, kBarre, region);
    if (! best.valid && style == kBarre) return findVoicing (tab, chIn, kOpen, region);
    return best;
}

} // namespace dh::gt
