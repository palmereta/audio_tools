#pragma once
// Teoría compartida por los generadores de MIDI (Orrery, Valence): escalas, acordes por terceras sobre los grados de la escala, colores
// (séptimas, novenas, sus, clusters), una progresión lenta por cadena de Markov y la conducción de
// voces (cada voz va a la nota del acorde nuevo más cercana).

#include <array>
#include <cstdint>
#include <cmath>
#include <string>
#include <vector>
#include <algorithm>

namespace dh::theory {

// Matemática determinista: la misma variación tiene que dar la misma música en macOS y Windows, y las
// funciones de la biblioteca de cada plataforma (log, sin, exp2...) no coinciden al bit. Estas usan sólo
// + − × ÷ y frexp/ldexp, que IEEE 754 define exactos; el target se compila sin contracción a FMA
// (-ffp-contract=off) y con /fp:precise en MSVC. Precisión ~1e-15, de sobra para música.
namespace dm {
constexpr double kPi = 3.14159265358979323846, kLn2 = 0.69314718055994530942;
inline double exp2 (double x)
{
    if (x > 1000.0) x = 1000.0; else if (x < -1000.0) x = -1000.0;
    const double i = std::floor (x + 0.5), f = (x - i) * kLn2;      // f en [−0,35, 0,35]
    double t = 1.0, s = 1.0;
    for (int k = 1; k <= 17; ++k) { t = t * f / (double) k; s = s + t; }
    return std::ldexp (s, (int) i);
}
inline double log (double x)
{
    if (! (x > 0.0)) return -1.0e300;
    int e = 0; double m = std::frexp (x, &e);                       // m en [0,5, 1)
    if (m < 0.70710678118654752440) { m = m * 2.0; e = e - 1; }     // m en [0,707, 1,414)
    const double z = (m - 1.0) / (m + 1.0), z2 = z * z;             // |z| < 0,1716
    double t = z, s = z;
    for (int k = 3; k <= 31; k += 2) { t = t * z2; s = s + t / (double) k; }
    return 2.0 * s + (double) e * kLn2;
}
inline double log2 (double x) { return log (x) / kLn2; }
inline double pow (double b, double x) { return b <= 0.0 ? 0.0 : exp2 (x * log2 (b)); }
inline double sin (double x)
{
    const double q = std::floor (x / (2.0 * kPi) + 0.5);
    double r = x - q * (2.0 * kPi);                                 // r en [−π, π]
    if (r > 0.5 * kPi) r = kPi - r; else if (r < -0.5 * kPi) r = -kPi - r;   // r en [−π/2, π/2]
    const double r2 = r * r;
    double t = r, s = r;
    for (int k = 1; k <= 12; ++k) { t = -t * r2 / (double) ((2 * k) * (2 * k + 1)); s = s + t; }
    return s;
}
inline double cos (double x) { return sin (x + 0.5 * kPi); }
} // namespace dm

struct Rng
{
    uint32_t s = 0x9E3779B9u;
    explicit Rng (uint32_t seed = 1) { reseed (seed); }
    void reseed (uint32_t seed) { s = seed * 2654435761u + 0x6D2B79F5u; if (s == 0) s = 1; for (int i = 0; i < 4; ++i) next(); }
    inline uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    inline float uni() { return (float) (next() >> 8) * (1.0f / 16777216.0f); }            // [0, 1)
    inline float range (float a, float b) { return a + (b - a) * uni(); }
    inline int irange (int a, int b) { return a + (int) (uni() * (float) (b - a + 1)); }       // [a, b]
    inline bool chance (float p) { return uni() < p; }
    inline float gauss() { const double u = std::max (1.0e-7f, uni()), v = uni(); return (float) (std::sqrt (-2.0 * dm::log (u)) * dm::cos (2.0 * dm::kPi * v)); }
};

struct Scale { const char* name; std::vector<int> steps; };

inline const std::vector<Scale>& scales()
{
    static const std::vector<Scale> s = {
        { "Major (Ionian)",     { 0, 2, 4, 5, 7, 9, 11 } },
        { "Natural minor",      { 0, 2, 3, 5, 7, 8, 10 } },
        { "Dorian",             { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",           { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",             { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",         { 0, 2, 4, 5, 7, 9, 10 } },
        { "Locrian",            { 0, 1, 3, 5, 6, 8, 10 } },
        { "Harmonic minor",     { 0, 2, 3, 5, 7, 8, 11 } },
        { "Phrygian dominant",  { 0, 1, 4, 5, 7, 8, 10 } },
        { "Double harmonic",    { 0, 1, 4, 5, 7, 8, 11 } },
        { "Major pentatonic",   { 0, 2, 4, 7, 9 } },
        { "Minor pentatonic",   { 0, 3, 5, 7, 10 } },
        { "Hirajoshi",          { 0, 2, 3, 7, 8 } },
        { "In-sen",             { 0, 1, 5, 7, 10 } },
        { "Whole tone",         { 0, 2, 4, 6, 8, 10 } },
        { "Lydian augmented",   { 0, 2, 4, 6, 8, 9, 11 } },
    };
    return s;
}

inline const char* noteName (int pc) { static const char* N[12] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }; return N[((pc % 12) + 12) % 12]; }

// Un acorde: raíz (grado de la escala) y las clases de altura que lo forman.
struct Chord
{
    int degree = 0, rootPc = 0;
    std::vector<int> pcs;           // clases de altura, la raíz primero
    std::string name;
};

// Grado d de la escala (puede pasar de la octava) → semitonos desde la tónica.
inline int degreeSemis (const Scale& sc, int d)
{
    const int n = (int) sc.steps.size();
    const int oct = (int) std::floor ((double) d / n);
    return sc.steps[(size_t) (((d % n) + n) % n)] + 12 * oct;
}

// color 0..1: tríada → séptima → novena → oncena; tension 0..1: sus, segundas menores y tritonos.
inline Chord buildChord (const Scale& sc, int key, int degree, float color, float tension, Rng& rng)
{
    Chord c; c.degree = degree;
    const int n = (int) sc.steps.size();
    const int rootSemis = degreeSemis (sc, degree);
    c.rootPc = ((key + rootSemis) % 12 + 12) % 12;
    std::vector<int> rel;   // semitonos sobre la raíz
    if (n <= 5)             // pentatónicas y similares: la propia escala como color (cuartas y quintas)
    {
        rel = { 0, degreeSemis (sc, degree + 2) - rootSemis, degreeSemis (sc, degree + 3) - rootSemis };
        if (color > 0.4f) rel.push_back (degreeSemis (sc, degree + 1) - rootSemis);
        if (color > 0.75f) rel.push_back (degreeSemis (sc, degree + 4) - rootSemis);
    }
    else
    {
        for (int k : { 0, 2, 4 }) rel.push_back (degreeSemis (sc, degree + k) - rootSemis);
        if (color > 0.3f) rel.push_back (degreeSemis (sc, degree + 6) - rootSemis);     // séptima
        if (color > 0.55f) rel.push_back (degreeSemis (sc, degree + 8) - rootSemis);    // novena
        if (color > 0.85f) rel.push_back (degreeSemis (sc, degree + 10) - rootSemis);   // oncena
        if (tension > 0.25f && rng.chance (tension)) rel[1] = degreeSemis (sc, degree + (rng.chance (0.5f) ? 1 : 3)) - rootSemis;   // sus2 / sus4
    }
    if (tension > 0.55f && rng.chance (tension - 0.4f)) rel.push_back (1);               // cluster: segunda menor
    if (tension > 0.75f && rng.chance (tension - 0.6f)) rel.push_back (6);               // tritono
    for (int r : rel) { const int pc = ((c.rootPc + r) % 12 + 12) % 12; if (std::find (c.pcs.begin(), c.pcs.end(), pc) == c.pcs.end()) c.pcs.push_back (pc); }
    // Nombre corto para la pantalla.
    const int third = (degreeSemis (sc, degree + 2) - rootSemis + 12) % 12;
    const int fifth = (degreeSemis (sc, degree + 4) - rootSemis + 12) % 12;
    c.name = noteName (c.rootPc);
    if (n > 5)
    {
        const bool dim = third == 3 && fifth == 6, aug = third == 4 && fifth == 8;
        if (dim) c.name += color > 0.3f ? "m7b5" : "dim";
        else
        {
            c.name += third == 3 ? "m" : (aug ? "+" : "");
            if (color > 0.3f) c.name += color > 0.55f ? (color > 0.85f ? "11" : "9") : "7";
        }
    }
    if (tension > 0.55f) c.name += "*";
    return c;
}

// Próximo grado: movimientos por cuarta/quinta y tercera preferidos; "motion" sube la probabilidad
// de saltos; "stasis" (modo drone) queda casi siempre en el mismo grado.
inline int nextDegree (int current, int scaleSize, float motion, float stasis, Rng& rng)
{
    if (rng.chance (stasis)) return current;
    struct Move { int step; float w; };
    const Move moves[] = { { 3, 3.0f }, { -3, 2.5f }, { 4, 2.0f }, { -4, 2.0f }, { 2, 1.6f }, { -2, 1.8f }, { 1, 0.9f + motion }, { -1, 1.0f + motion }, { 5, 0.4f + motion }, { 0, 0.6f } };
    float total = 0.0f; for (auto& m : moves) total += m.w;
    float r = rng.uni() * total;
    int step = 0;
    for (auto& m : moves) { if ((r -= m.w) <= 0.0f) { step = m.step; break; } }
    int d = ((current + step) % scaleSize + scaleSize) % scaleSize;
    // Volver a la tónica de vez en cuando: el centro tonal no se pierde.
    if (rng.chance (0.18f * (1.0f - motion))) d = 0;
    return d;
}

// Clases de altura → notas MIDI dentro de [lo, hi].
inline std::vector<int> notesInRange (const std::vector<int>& pcs, int lo, int hi)
{
    std::vector<int> v;
    for (int n = lo; n <= hi; ++n) if (std::find (pcs.begin(), pcs.end(), n % 12) != pcs.end()) v.push_back (n);
    return v;
}

inline std::vector<int> scalePcs (const Scale& sc, int key)
{
    std::vector<int> v;
    for (int s : sc.steps) v.push_back ((key + s) % 12);
    return v;
}

// Nota permitida más cercana a n.
inline int snap (int n, const std::vector<int>& pcs)
{
    for (int d = 0; d < 12; ++d)
    {
        if (std::find (pcs.begin(), pcs.end(), ((n - d) % 12 + 12) % 12) != pcs.end()) return n - d;
        if (std::find (pcs.begin(), pcs.end(), ((n + d) % 12 + 12) % 12) != pcs.end()) return n + d;
    }
    return n;
}

// Voicing de "voices" notas del acorde en [lo, hi], cerca del voicing anterior (conducción de voces).
inline std::vector<int> voiceLead (const std::vector<int>& prev, const Chord& c, int voices, int lo, int hi, Rng& rng)
{
    auto pool = notesInRange (c.pcs, lo, hi);
    std::vector<int> out;
    if (pool.empty()) return out;
    if (prev.empty())
    {
        // Distribución abierta desde abajo: raíz, luego cada pc del acorde en orden, saltando una octava si hace falta.
        int n = lo + ((c.rootPc - lo) % 12 + 12) % 12;
        out.push_back (n);
        for (size_t i = 1; (int) out.size() < voices && i < 32; ++i)
        {
            const int pc = c.pcs[i % c.pcs.size()];
            int m = out.back() + 1;
            while (m % 12 != pc) ++m;
            if (rng.chance (0.3f) && m + 12 <= hi) m += 12;       // voicing abierto
            if (m > hi) break;
            out.push_back (m);
        }
        return out;
    }
    std::vector<bool> used (pool.size(), false);
    for (int p : prev)
    {
        if ((int) out.size() >= voices) break;
        int best = -1, bd = 999;
        for (size_t i = 0; i < pool.size(); ++i)
            if (! used[i]) { const int d = std::abs (pool[i] - p); if (d < bd) { bd = d; best = (int) i; } }
        if (best >= 0) { used[(size_t) best] = true; out.push_back (pool[(size_t) best]); }
    }
    // Que la raíz esté siempre (abajo si se puede).
    bool hasRoot = false; for (int n : out) hasRoot |= n % 12 == c.rootPc;
    if (! hasRoot && ! out.empty())
    {
        auto lowest = std::min_element (out.begin(), out.end());
        int r = *lowest; while (r % 12 != c.rootPc) --r;
        if (r < lo) r += 12;
        *lowest = r;
    }
    while ((int) out.size() < voices)
    {
        int best = -1;
        for (size_t i = 0; i < pool.size(); ++i) if (! used[i]) { best = (int) i; break; }
        if (best < 0) break;
        used[(size_t) best] = true; out.push_back (pool[(size_t) best]);
    }
    std::sort (out.begin(), out.end());
    out.erase (std::unique (out.begin(), out.end()), out.end());
    return out;
}

} // namespace dh::theory
