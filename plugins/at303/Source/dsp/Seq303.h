#pragma once
// El secuenciador del AT303 (docs/research.md §2). Como en el original, la altura y el tiempo son dos listas:
//   - la lista de alturas guarda notas (0-12 sobre C, octava arriba/abajo, acento, slide), en orden de ejecución;
//   - la lista de tiempos guarda, por paso de semicorchea, nota (N), ligadura (T) o silencio (R).
// Cada paso N consume la siguiente altura. Por eso editar una lista "corre" la otra y los acentos se mudan de
// lugar: el desorden que hizo famosas a las máquinas viejas. La grilla moderna edita los mismos datos.
//
// Tiempo (Whittle, medido): reloj de 24 ppq, 6 pulsos por paso (8 en tresillos). La compuerta sube en el pulso 0 y
// baja a mitad del pulso 3 del último paso de la nota (3,5 de 6). Una nota con slide no baja la compuerta: la
// siguiente entra sin flanco (las envolventes no se redisparan) y el slide queda conectado durante toda esa nota.
// Un slide hacia un silencio no funciona. Todo se calcula en pulsos de reloj, así que el resultado es exacto a la
// muestra con cualquier tempo y tamaño de bloque.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dh::tb {

struct PitchSlot { int8_t note = 0; bool up = false, down = false, accent = false, slide = false; };
enum StepType : uint8_t { kNote = 0, kTie = 1, kRest = 2 };

struct Pattern
{
    static constexpr int kSteps = 16;
    std::array<PitchSlot, kSteps> pitch {};
    std::array<StepType, kSteps> time {};
    int length = 16;
    bool triplet = false;

    Pattern() { time.fill (kNote); }
    int clocksPerStep() const { return triplet ? 8 : 6; }
};

// Una nota ya resuelta (lo que suena): dónde empieza, cuántos pasos dura, su altura y sus banderas.
struct CompiledNote { int start = 0, len = 1; int semis = 0; bool accent = false, slide = false, legatoIn = false; };

inline std::vector<CompiledNote> compile (const Pattern& p)
{
    std::vector<CompiledNote> out;
    int ptr = 0;
    const int L = std::clamp (p.length, 1, Pattern::kSteps);
    for (int i = 0; i < L; ++i)
    {
        StepType t = p.time[(size_t) i];
        if (i == 0 && t == kTie) t = kNote;                       // una ligadura al principio se escribe como nota
        if (t == kNote)
        {
            const PitchSlot& s = p.pitch[(size_t) (ptr % Pattern::kSteps)];
            ++ptr;
            CompiledNote n; n.start = i; n.len = 1;
            n.semis = s.note + (s.up ? 12 : 0) - (s.down ? 12 : 0);
            n.accent = s.accent; n.slide = s.slide;
            out.push_back (n);
        }
        else if (t == kTie && ! out.empty() && out.back().start + out.back().len == i) out.back().len++;
    }
    // ¿Entra ligada desde la anterior? (slide de la anterior y sin silencio en el medio; también al dar la vuelta)
    for (size_t k = 0; k < out.size(); ++k)
    {
        const CompiledNote& prev = out[(k + out.size() - 1) % out.size()];
        const bool contiguous = k > 0 ? prev.start + prev.len == out[k].start : (prev.start + prev.len == L && out[k].start == 0);
        out[k].legatoIn = prev.slide && contiguous;
    }
    return out;
}

// Lo que el secuenciador le pide a la voz en este instante.
struct SeqEvent { enum Kind { kNoteOn, kGateOff, kSlide } kind; float cv; bool accent, legato, slide; };

class Sequencer
{
public:
    // Reinicio al principio del patrón (o al arrancar el transporte).
    void restart() { lastStep = -1; noteIdx = -1; gateOffAt = -1; lastC1 = -1; patPos = 0; shownStep = -1; }
    void setPattern (const Pattern& p) { pending = p; pendingNotes = compile (p); havePending = true; }
    void setTranspose (int semis) { transpose = semis; }

    // Avanza de `c0` a `c1` (pulsos de reloj absolutos desde el inicio de la canción) y emite los eventos que caen
    // en ese tramo, con su posición fraccionaria `frac` (0..1) dentro de él.
    template <typename F> void advance (double c0, double c1, F&& emit)
    {
        if (havePending && lastStep < 0) adopt();
        // ¿El reloj sigue de corrido? Entre bloques puede quedar una rendija de redondeo; más que eso es un salto del
        // host (loop, otra posición): la nota que sonaba se suelta (un fin de compuerta pendiente quedaría en la
        // línea de tiempo vieja y la nota colgada) y se recalcula como al arrancar, incluido el paso justo en c0.
        const bool continuous = lastC1 >= 0 && c0 >= lastC1 - 0.5 && c0 <= lastC1 + 0.5;
        if (! continuous && lastStep >= 0)
        {
            emit (SeqEvent { SeqEvent::kSlide, 0, false, false, false }, 0.0);
            emit (SeqEvent { SeqEvent::kGateOff, 0, false, false, false }, 0.0);
            lastStep = -1; gateOffAt = -1; noteIdx = -1;
            if (havePending) adopt();
        }
        const int cps = cur.clocksPerStep();
        // Fin de compuerta pendiente.
        auto checkGate = [&] (double a, double b) {
            if (gateOffAt >= 0 && gateOffAt > a && gateOffAt <= b) { emit (SeqEvent { SeqEvent::kGateOff, 0, false, false, false }, (gateOffAt - c0) / std::max (1e-12, c1 - c0)); gateOffAt = -1; }
        };
        // Si el reloj sigue de corrido se retoma desde donde quedó: un paso o un fin de compuerta que cae en la
        // rendija entre bloques no se pierde.
        double from = continuous ? std::min (c0, lastC1) : c0;
        // Pasos que empiezan en (from, c1]; al arrancar, también el que cae justo en c0.
        int64_t sFirst;
        if (lastStep < 0) sFirst = (int64_t) std::ceil (c0 / cps - 1e-9);
        else sFirst = std::max (lastStep + 1, (int64_t) std::floor (from / cps) + 1);
        const int64_t sLast = (int64_t) std::floor (c1 / cps);
        lastC1 = c1;
        for (int64_t s = sFirst; s <= sLast; ++s)
        {
            const double at = (double) s * cps;
            checkGate (from, at - 1e-9);
            from = at - 1e-9;
            if (s < 0) continue;                                    // pre-roll o cuenta previa del host (posición negativa)
            stepStart (s, at, c0, c1, emit);
        }
        checkGate (from, c1);
    }

    int currentStep() const { return lastStep < 0 ? -1 : shownStep; }
    const Pattern& pattern() const { return cur; }

private:
    Pattern cur, pending; std::vector<CompiledNote> notes, pendingNotes; bool havePending = false;
    int64_t lastStep = -1; int noteIdx = -1; double gateOffAt = -1, lastC1 = -1; int transpose = 0;
    int patPos = 0, shownStep = -1;                             // el paso dentro del patrón (contador propio)

    void adopt() { cur = pending; notes = pendingNotes; havePending = false; }

    template <typename F> void stepStart (int64_t sAbs, double at, double c0, double c1, F&& emit)
    {
        int L = std::max (1, cur.length);
        // El paso dentro del patrón lleva su propio contador: con la posición absoluta módulo el largo, un patrón de
        // otro largo (o cambiar el largo tocando) entraba en cualquier paso y salteaba los del medio. Al arrancar se
        // alinea con la grilla del host.
        if (lastStep < 0) patPos = (int) (((sAbs % L) + L) % L);
        if (patPos >= L) patPos = 0;
        int s = patPos;
        lastStep = sAbs;
        if (s == 0 && havePending)                                  // el patrón nuevo entra en el compás siguiente
        {
            const int oldCps = cur.clocksPerStep();
            adopt(); L = std::max (1, cur.length);
            // Tresillos ↔ corcheas: el paso cambia de 6 a 8 pulsos (o al revés); la cuenta de pasos pasa a las
            // unidades nuevas (si no, el paso siguiente caía cinco pasos más tarde).
            if (cur.clocksPerStep() != oldCps) lastStep = (int64_t) std::llround (at / cur.clocksPerStep());
        }
        patPos = s + 1; shownStep = s;
        const int cps = cur.clocksPerStep();
        const double frac = (at - c0) / std::max (1e-12, c1 - c0);
        for (size_t k = 0; k < notes.size(); ++k)
        {
            const CompiledNote& n = notes[k];
            if (n.start != s) continue;
            noteIdx = (int) k;
            const float cv = 2.0f + (float) (n.semis + transpose) / 12.0f;
            emit (SeqEvent { SeqEvent::kSlide, 0, false, false, n.legatoIn }, frac);
            emit (SeqEvent { SeqEvent::kNoteOn, cv, n.accent, n.legatoIn, n.legatoIn }, frac);
            // ¿Baja la compuerta? Sólo si no se liga a la siguiente.
            const CompiledNote& next = notes[(k + 1) % notes.size()];
            const bool contiguous = (k + 1 < notes.size()) ? n.start + n.len == next.start : (n.start + n.len == L && next.start == 0);
            if (n.slide && contiguous && notes.size() > 1) gateOffAt = -1;
            else if (n.slide && contiguous && notes.size() == 1) gateOffAt = -1;
            else gateOffAt = at + (double) (n.len - 1) * cps + 3.5 * cps / 6.0;
            return;
        }
        // Silencio o ligadura: nada que disparar; el slide se suelta al empezar un silencio.
        if (cur.time[(size_t) s] == kRest) emit (SeqEvent { SeqEvent::kSlide, 0, false, false, false }, frac);
    }
};

// ── Generador ácido (docs/research.md §5.4): densidad con pesos métricos, escala y raíz, acentos y slides por
// probabilidad, semilla reproducible y bloqueo por carril. Determinista: la misma semilla da el mismo patrón. ──
struct GenParams { int root = 0, scale = 0, octaves = 1; float density = 0.7f, accent = 0.3f, slide = 0.2f, ties = 0.15f; uint32_t seed = 1;
                   bool lockNotes = false, lockRhythm = false, lockAccents = false, lockSlides = false; };

inline const std::array<std::vector<int>, 7>& scales()
{
    static const std::array<std::vector<int>, 7> s {{
        { 0, 2, 3, 5, 7, 8, 10 },          // menor natural
        { 0, 1, 3, 5, 7, 8, 10 },          // frigio
        { 0, 2, 3, 5, 7, 9, 10 },          // dórico
        { 0, 3, 5, 7, 10 },                // pentatónica menor
        { 0, 2, 3, 5, 7, 8, 11 },          // menor armónica
        { 0, 2, 4, 5, 7, 9, 10 },          // mixolidio
        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 },
    }};
    return s;
}
inline const char* scaleName (int i) { static const char* n[] = { "Minor", "Phrygian", "Dorian", "Minor pentatonic", "Harmonic minor", "Mixolydian", "Chromatic" }; return n[std::clamp (i, 0, 6)]; }

inline Pattern generate (const Pattern& base, const GenParams& g)
{
    Pattern p = base;
    uint32_t s = g.seed * 2654435761u + 0x9E3779B9u;
    auto rnd = [&] { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) * (1.0f / 16777216.0f); };
    const auto& sc = scales()[(size_t) std::clamp (g.scale, 0, 6)];
    if (! g.lockRhythm)
        for (int i = 0; i < Pattern::kSteps; ++i)
        {
            const float w = i % 4 == 0 ? 0.95f : (i % 2 == 0 ? 0.7f : 0.45f);         // el pulso pesa más
            const float r = rnd();
            if (i > 0 && p.time[(size_t) (i - 1)] != kRest && rnd() < g.ties * 0.6f) p.time[(size_t) i] = kTie;
            else p.time[(size_t) i] = r < g.density * w + 0.05f ? kNote : kRest;
        }
    for (int i = 0; i < Pattern::kSteps; ++i)
    {
        PitchSlot& ps = p.pitch[(size_t) i];
        if (! g.lockNotes)
        {
            const float r = rnd();
            const int deg = r < 0.35f ? 0 : (int) (rnd() * (float) sc.size()) % (int) sc.size();   // la raíz pesa más
            ps.note = (int8_t) ((g.root + sc[(size_t) deg]) % 12);
            const float o = rnd();
            ps.up = g.octaves >= 1 && o < 0.18f * (float) g.octaves;
            ps.down = g.octaves >= 2 && ! ps.up && o > 1.0f - 0.12f * (float) g.octaves;
        }
        if (! g.lockAccents) ps.accent = rnd() < g.accent;
        if (! g.lockSlides) ps.slide = rnd() < g.slide;
    }
    return p;
}

} // namespace dh::tb
