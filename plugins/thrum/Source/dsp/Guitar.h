#pragma once
// Los seis tipos de guitarra de Thrum y la física de sus cuerdas (docs/research.md §1, §2). Descripciones genéricas:
// ninguna marca ni forma de cuerpo registrada. Cada tipo fija el diapasón (escala, trastes), el juego de cuerdas
// (calibre, entorchada o lisa), el cuerpo (sustain, modos del puente y de la caja) y las pastillas (tipo, posición
// medida desde el puente, selector). La afinación y el calibre se eligen aparte.
//
// Cuerdas: densidad lineal μ = ρ·π·d²/4 (acero, ρ = 7850 kg/m³; las entorchadas pesan ~0,8 de una barra maciza del
// mismo diámetro, que es lo que dan las tablas de peso por unidad de largo de los fabricantes), tensión
// T = μ·(2·L·f)², inarmonicidad B = π³·E·d_alma⁴/(64·T·L²) con E = 200 GPa y el alma de las entorchadas en 0,38 del
// diámetro exterior. En un traste n el largo vibrante es L·2^(−n/12) y la tensión no cambia: B crece 4^(n/12).
// Los parciales caen en f_k = k·f0·√(1 + B·k²)/√(1 + B) (afinada la fundamental).

#include "Base.h"

namespace dh::gt {

constexpr int kMaxStrings = 7;
constexpr int kMaxFrets = 24;

enum PickupKind { kSingle, kHumbucker };

struct PickupSpec
{
    float posMm = 100.0f;          // centro de la pastilla, medido desde el puente (lado de los agudos en las inclinadas)
    float slantMm = 0.0f;          // diferencia de posición entre la cuerda más grave y la más aguda (inclinada)
    int kind = kSingle;
    float henry = 2.4f, ohm = 6000.0f, selfPf = 120.0f;   // bobina: inductancia, resistencia, capacidad propia
    float output = 1.0f;           // sensibilidad relativa (imán, vueltas)
};

struct BodyMode { float hz, q, gain; };

struct GuitarSpec
{
    const char* name;
    const char* shortName;
    int nStrings;
    float scaleMm;
    int frets;
    std::array<int, kMaxStrings> standard;        // afinación estándar, MIDI, índice 0 = la cuerda más grave
    std::array<float, kMaxStrings> gauge;         // calibre "regular" en pulgadas (índice 0 = la más grave)
    std::array<bool, kMaxStrings> wound;
    int nPickups;
    std::array<PickupSpec, 3> pickup;             // 0 = puente, luego medio/mástil
    int selector;                                 // 3 o 5 posiciones
    float potVolOhm, potToneOhm, toneCapNf;
    float sustainS;                               // T60 de la fundamental de la 6.ª al aire (s)
    float brightS;                                // T60 a 3 kHz (s)
    std::array<BodyMode, 3> body;                 // modos del puente y la caja (acople entre cuerdas)
    float microphonic;                            // cuánto de la caja llega a la pastilla (huecas)
    float hollow;                                 // 0 maciza … 1 hueca (realimentación acústica más fácil)
    float neckHz = 300.0f;                        // modo del mástil: las notas cerca de él se apagan antes (punto muerto)
};

inline const std::array<GuitarSpec, 6>& guitars()
{
    // Posiciones de pastillas típicas medidas desde el puente (mm). Valores de bobina de Lemme y Zollner
    // (docs/research.md §2): simple ~2,2-2,6 H / 5,5-6,5 kΩ, doble ~4-5 H / 8-15 kΩ.
    static const std::array<GuitarSpec, 6> g {{
        { "Solid body, three single coils", "Solid 3S", 6, 648.0f, 22,
          { 40, 45, 50, 55, 59, 64, 0 }, { 0.046f, 0.036f, 0.026f, 0.017f, 0.013f, 0.010f, 0 }, { true, true, true, false, false, false, false },
          3, {{ { 41.0f, 14.0f, kSingle, 2.6f, 6400.0f, 120.0f, 1.08f }, { 99.0f, 0.0f, kSingle, 2.3f, 5900.0f, 110.0f, 1.0f }, { 160.0f, 0.0f, kSingle, 2.25f, 5700.0f, 110.0f, 0.96f } }},
          5, 250e3f, 250e3f, 47.0f, 13.0f, 2.6f, {{ { 118.0f, 3.0f, 1.0f }, { 265.0f, 5.0f, 0.6f }, { 1450.0f, 8.0f, 0.3f } }}, 0.0f, 0.0f, 290.0f },
        { "Solid body, two humbuckers", "Solid 2H", 6, 628.0f, 22,
          { 40, 45, 50, 55, 59, 64, 0 }, { 0.046f, 0.036f, 0.026f, 0.017f, 0.013f, 0.010f, 0 }, { true, true, true, false, false, false, false },
          2, {{ { 38.0f, 0.0f, kHumbucker, 4.6f, 9200.0f, 150.0f, 1.55f }, { 157.0f, 0.0f, kHumbucker, 4.1f, 8000.0f, 140.0f, 1.4f }, {} }},
          3, 500e3f, 500e3f, 22.0f, 17.0f, 2.8f, {{ { 105.0f, 3.0f, 1.0f }, { 240.0f, 5.0f, 0.5f }, { 1200.0f, 8.0f, 0.3f } }}, 0.0f, 0.0f, 270.0f },
        { "Semi-hollow body, two humbuckers", "Semi-hollow", 6, 628.0f, 22,
          { 40, 45, 50, 55, 59, 64, 0 }, { 0.046f, 0.036f, 0.026f, 0.017f, 0.013f, 0.010f, 0 }, { true, true, true, false, false, false, false },
          2, {{ { 40.0f, 0.0f, kHumbucker, 4.3f, 8400.0f, 145.0f, 1.45f }, { 155.0f, 0.0f, kHumbucker, 4.0f, 7800.0f, 140.0f, 1.35f }, {} }},
          3, 500e3f, 500e3f, 22.0f, 11.0f, 2.2f, {{ { 98.0f, 6.0f, 1.0f }, { 205.0f, 8.0f, 0.9f }, { 520.0f, 9.0f, 0.4f } }}, 0.012f, 0.5f, 250.0f },
        { "Hollow jazz body", "Hollow jazz", 6, 628.0f, 20,
          { 40, 45, 50, 55, 59, 64, 0 }, { 0.052f, 0.042f, 0.032f, 0.024f, 0.016f, 0.012f, 0 }, { true, true, true, true, false, false, false },
          2, {{ { 45.0f, 0.0f, kHumbucker, 4.4f, 8600.0f, 160.0f, 1.3f }, { 150.0f, 0.0f, kHumbucker, 4.2f, 8200.0f, 160.0f, 1.25f }, {} }},
          3, 500e3f, 500e3f, 22.0f, 7.0f, 1.1f, {{ { 92.0f, 8.0f, 1.0f }, { 190.0f, 10.0f, 1.0f }, { 410.0f, 10.0f, 0.5f } }}, 0.03f, 1.0f, 220.0f },
        { "Baritone", "Baritone", 6, 686.0f, 24,
          { 35, 40, 45, 50, 54, 59, 0 }, { 0.062f, 0.046f, 0.036f, 0.026f, 0.017f, 0.013f, 0 }, { true, true, true, true, false, false, false },
          2, {{ { 40.0f, 0.0f, kHumbucker, 4.5f, 9000.0f, 150.0f, 1.5f }, { 170.0f, 0.0f, kSingle, 2.4f, 6000.0f, 115.0f, 1.0f }, {} }},
          3, 500e3f, 500e3f, 22.0f, 16.0f, 2.6f, {{ { 100.0f, 3.0f, 1.0f }, { 230.0f, 5.0f, 0.5f }, { 1150.0f, 8.0f, 0.3f } }}, 0.0f, 0.0f, 240.0f },
        { "Seven-string", "Seven-string", 7, 648.0f, 24,
          { 35, 40, 45, 50, 55, 59, 64 }, { 0.059f, 0.046f, 0.036f, 0.026f, 0.017f, 0.013f, 0.010f }, { true, true, true, true, false, false, false },
          2, {{ { 39.0f, 0.0f, kHumbucker, 4.8f, 10500.0f, 155.0f, 1.6f }, { 160.0f, 0.0f, kHumbucker, 4.2f, 8300.0f, 145.0f, 1.45f }, {} }},
          3, 500e3f, 500e3f, 22.0f, 16.0f, 2.6f, {{ { 102.0f, 3.0f, 1.0f }, { 236.0f, 5.0f, 0.5f }, { 1180.0f, 8.0f, 0.3f } }}, 0.0f, 0.0f, 280.0f },
    }};
    return g;
}
inline const GuitarSpec& guitar (int i) { return guitars()[(size_t) std::clamp (i, 0, 5)]; }

// Selector: qué pastillas suenan en cada posición (bit 0 = puente, 1 = medio o mástil, 2 = mástil), en paralelo.
inline int selectorMask (const GuitarSpec& g, int pos)
{
    if (g.selector == 5)
    {
        static const int m5[5] = { 1, 3, 2, 6, 4 };                  // puente, puente+medio, medio, medio+mástil, mástil
        return m5[std::clamp (pos, 0, 4)];
    }
    static const int m3[3] = { 1, 3, 2 };                            // puente, las dos, mástil
    return m3[std::clamp (pos, 0, 2)];
}
inline int selectorPositions (const GuitarSpec& g) { return g.selector; }

// ── Afinaciones ──
// Desplazamientos en semitonos respecto de la estándar del tipo, para las seis cuerdas más agudas (índice 0 = la 6.ª);
// en la de siete, la 7.ª acompaña a la 6.ª en las caídas y queda igual en las abiertas.
enum Tuning { kStandard, kDropD, kHalfDown, kOpenG, kOpenD, kOpenE, kDadgad, kDropC, kCustom, kNumTunings };
inline const char* tuningName (int t)
{
    static const char* n[] = { "Standard", "Drop (lowest string down a tone)", "Half step down", "Open G", "Open D", "Open E", "DADGAD", "Drop, a tone lower", "Custom" };
    return n[std::clamp (t, 0, (int) kNumTunings - 1)];
}
inline std::array<int, kMaxStrings> tuningNotes (const GuitarSpec& g, int t, const std::array<int, kMaxStrings>& custom)
{
    static const int off6[kNumTunings][6] = {
        { 0, 0, 0, 0, 0, 0 }, { -2, 0, 0, 0, 0, 0 }, { -1, -1, -1, -1, -1, -1 },
        { -2, -2, 0, 0, 0, -2 }, { -2, 0, 0, -1, -2, -2 }, { 0, 2, 2, 1, 0, 0 }, { -2, 0, 0, 0, -2, -2 }, { -4, -2, -2, -2, -2, -2 }, { 0, 0, 0, 0, 0, 0 } };
    std::array<int, kMaxStrings> n {};
    const int extra = g.nStrings - 6;                                // la 7.ª (índice 0) en la de siete
    for (int s = 0; s < g.nStrings; ++s)
    {
        int o = 0;
        const int k = s - extra;                                     // índice dentro de las seis agudas
        if (t == kCustom) o = custom[(size_t) s];
        else if (k >= 0) o = off6[std::clamp (t, 0, (int) kNumTunings - 1)][k];
        else o = (t == kDropD) ? -2 : (t == kHalfDown) ? -1 : (t == kDropC) ? -4 : 0;
        n[(size_t) s] = g.standard[(size_t) s] + o;
    }
    return n;
}

// ── Calibres ──
enum Gauge { kLight, kRegular, kMedium, kHeavy, kNumGauges };
inline const char* gaugeName (int i) { static const char* n[] = { "Light", "Regular", "Medium", "Heavy" }; return n[std::clamp (i, 0, 3)]; }
inline float gaugeScale (int i) { static const float s[] = { 0.9f, 1.0f, 1.1f, 1.2f }; return s[std::clamp (i, 0, 3)]; }

// ── Física de una cuerda ──
struct StringPhysics
{
    float openHz = 82.41f;       // al aire, con la afinación y la cejilla (capo) aplicadas
    float gaugeIn = 0.046f;
    bool wound = true;
    float muKgM = 0.0f, tensionN = 0.0f, B = 0.0f;   // densidad, tensión, inarmonicidad al aire
    float lengthMm = 648.0f;     // largo vibrante al aire (desde la cejilla o el capo)
    float scaleMm = 648.0f;      // escala completa (para ubicar púa y pastillas)
    int capo = 0;
};

inline float midiHz (float note) { return 440.0f * std::exp2 ((note - 69.0f) / 12.0f); }

inline StringPhysics stringPhysics (const GuitarSpec& g, int s, int midiNote, int gaugeOpt, int capo)
{
    StringPhysics p;
    p.scaleMm = g.scaleMm;
    p.capo = std::clamp (capo, 0, 12);
    p.openHz = midiHz ((float) (midiNote + p.capo));
    p.gaugeIn = g.gauge[(size_t) s] * gaugeScale (gaugeOpt);
    p.wound = g.wound[(size_t) s];
    const double d = (double) p.gaugeIn * 0.0254;                 // m
    const double rho = 7850.0, E = 2.0e11;
    const double mu = rho * kPiD * d * d / 4.0 * (p.wound ? 0.8 : 1.0);
    const double L = g.scaleMm * 1e-3;
    const double fOpen = midiHz ((float) midiNote);               // la tensión es la de la cuerda al aire sin capo
    const double T = mu * (2.0 * L * fOpen) * (2.0 * L * fOpen);
    const double dc = p.wound ? 0.38 * d : d;
    p.lengthMm = (float) (g.scaleMm * std::pow (2.0, -p.capo / 12.0));
    const double Lv = p.lengthMm * 1e-3;
    p.muKgM = (float) mu; p.tensionN = (float) T;
    p.B = (float) (kPiD * kPiD * kPiD * E * dc * dc * dc * dc / (64.0 * T * Lv * Lv));
    return p;
}
// Inarmonicidad en un traste (contado desde el capo).
inline float fretB (const StringPhysics& p, float fret) { return p.B * std::exp2 (fret / 6.0f); }
// Frecuencia esperada del parcial k (fundamental afinada).
inline double partialHz (double f0, double B, int k) { return k * f0 * std::sqrt (1.0 + B * k * k) / std::sqrt (1.0 + B); }

} // namespace dh::gt
