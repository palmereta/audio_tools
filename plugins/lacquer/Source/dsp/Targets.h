#pragma once
// Objetivos del master: curvas tonales propias (paramétricas, docs/research.md §3.3), estilos (§6.5), mapa de
// intensidad (§6.6) y objetivos de sonoridad (§6.7). Son tablas: el análisis decide cuánto de cada cosa usar.

#include "Analysis.h"
#include <string>

namespace dh::lq {

// ---- Sonoridad ------------------------------------------------------------------------------------------------
struct LoudTarget { const char* id; const char* name; double lufs, ceilingDb; bool matchInput; };
inline const std::array<LoudTarget, 7>& loudTargets()
{
    static const std::array<LoudTarget, 7> t {{
        { "streaming", "Streaming",        -14.0, -1.0, false },
        { "apple",     "Apple / dynamic",  -16.0, -1.0, false },
        { "loud",      "Loud streaming",   -11.0, -2.0, false },
        { "club",      "Club",              -8.0, -2.0, false },
        { "cd",        "CD",                -9.0, -0.3, false },
        { "r128",      "Broadcast R128",   -23.0, -1.0, false },
        { "match",     "Match input",        0.0, -1.0, true  },
    }};
    return t;
}

// ---- Curva objetivo -------------------------------------------------------------------------------------------
// Potencia por banda de tercio (dB, relativa): inclinación + pasa altos de 2.º orden + joroba de graves + caída
// extra en agudos. La forma importa; el nivel absoluto se alinea con la mezcla al comparar.
struct TargetShape { double tiltDbOct, hpHz, bumpDb, bumpHz, hfHz, hfDbOct; };
inline double targetAt (const TargetShape& s, double f)
{
    double t = s.tiltDbOct * std::log2 (f / 1000.0);
    t += 10.0 * std::log10 (1.0 / (1.0 + std::pow (s.hpHz / f, 4.0)));
    const double u = std::log2 (f / s.bumpHz);
    t += s.bumpDb * std::exp (-u * u / (2.0 * 0.6 * 0.6));
    if (f > s.hfHz) t -= s.hfDbOct * std::log2 (f / s.hfHz);
    return t;
}
// Tolerancia de la curva (± dB): ±2 en medios, ±4 debajo de 60 Hz y arriba de 10 kHz.
inline double toleranceAt (double f)
{
    if (f <= 60.0) return 4.0;
    if (f < 120.0) return 4.0 - 2.0 * std::log2 (f / 60.0);
    if (f <= 5000.0) return 2.0;
    if (f < 10000.0) return 2.0 + 2.0 * std::log2 (f / 5000.0);
    return 4.0;
}

// ---- Estilos --------------------------------------------------------------------------------------------------
enum class Style { Balanced, Warm, Bright, Punchy, Loud, Gentle };
constexpr int kStyles = 6;

struct StyleDef
{
    const char* id; const char* name;
    TargetShape target;
    // Color propio del estilo (se suma a la EQ de corrección): estante grave, campana, estante agudo.
    double lowShelfHz, lowShelfDb, bellHz, bellDb, bellQ, highShelfHz, highShelfDb;
    double eqAmount;                  // escala de la parte de corrección (Gentle: la mitad)
    double mbScale;                   // escala del multibanda (0 = apagado)
    double glueRatio, glueAttackMs, glueReleaseMs, glueGrDb;   // a intensidad 100 %
    double satDriveDb; int satFlavour; // 0 cinta, 1 válvula
    double monoHz, widthDb, sideHighShelfDb;
    double clipDb;                    // cuánto pico le saca el recortador (0 = apagado)
    double relFastMs, relSlowMs;
};

inline const std::array<StyleDef, kStyles>& styles()
{
    //                                  tilt   hp   bump  bHz   hfHz  hfSl
    static const std::array<StyleDef, kStyles> s {{
        { "balanced", "Balanced", { -2.6, 32.0, 3.0, 70.0, 7000.0, 1.5 },
          120, 0.0,  300, 0.0, 1.0,  10000, 0.0,   1.0, 1.0,  1.6, 20, 250, 1.8,  0.5, 0,  120,  0.0,  0.0,  0.0, 30, 300 },
        { "warm",     "Warm",     { -2.9, 32.0, 3.5, 75.0, 6000.0, 2.0 },
          180, 1.0,  300, 0.0, 1.0,   6000, -1.5,  1.0, 0.8,  1.6, 30, 350, 1.8,  2.5, 0,  120,  0.0, -0.5,  0.0, 40, 450 },
        { "bright",   "Bright",   { -2.3, 32.0, 2.5, 70.0, 8000.0, 1.0 },
          120, 0.0,  300, -1.0, 1.0, 10000, 1.5,   1.0, 0.8,  1.6, 20, 250, 1.6,  0.0, 0,  120,  0.5,  1.0,  0.0, 30, 300 },
        { "punchy",   "Punchy",   { -2.6, 34.0, 3.5, 70.0, 7000.0, 1.5 },
           80, 1.0, 3000, 0.5, 0.8,  10000, 0.0,   1.0, 1.0,  1.8, 30, 150, 2.0,  1.0, 1,  150,  0.0,  0.0,  1.0, 20, 250 },
        { "loud",     "Loud",     { -2.4, 38.0, 2.5, 75.0, 7000.0, 1.5 },
           40, -1.0, 300, 0.0, 1.0,  10000, 0.0,   1.0, 1.3,  2.0, 15, 200, 2.5,  1.0, 0,  150,  0.0,  0.0,  2.0, 20, 200 },
        { "gentle",   "Gentle",   { -2.6, 30.0, 2.5, 70.0, 7000.0, 1.5 },
          120, 0.0,  300, 0.0, 1.0,  10000, 0.0,   0.5, 0.5,  1.4, 30, 400, 1.0,  0.0, 0,  100,  0.0,  0.0,  0.0, 50, 600 },
    }};
    return s;
}
inline const StyleDef& styleDef (Style s) { return styles()[(size_t) s]; }
inline int styleIndex (const std::string& id) { for (int i = 0; i < kStyles; ++i) if (id == styles()[(size_t) i].id) return i; return -1; }

// ---- Intensidad (0..1) ----------------------------------------------------------------------------------------
// 0 % es transparente (sólo sonoridad y limitador): así la prueba de nulo tiene sentido. 50 % es el valor por
// defecto. Los extremos del cuadro de §6.6 se respetan a partir de 25 %.
struct IntensityMap { double eqAmount, eqClamp, mbMaxGr, glueRatioFrac, glueGr, satFrac, widthFrac; };
inline IntensityMap intensityMap (double x)
{
    x = std::clamp (x, 0.0, 1.0);
    auto seg = [x] (double a0, double a25, double a50, double a100) {
        if (x < 0.25) return a0 + (a25 - a0) * x / 0.25;
        if (x < 0.5) return a25 + (a50 - a25) * (x - 0.25) / 0.25;
        return a50 + (a100 - a50) * (x - 0.5) / 0.5;
    };
    return { seg (0.0, 0.40, 0.60, 1.00),           // amount de la EQ de corrección
             seg (0.0, 2.0, 3.0, 4.0),              // tope de la EQ (dB)
             seg (0.0, 1.0, 2.0, 4.0),              // GR máxima por banda del multibanda (dB)
             seg (0.0, 0.5, 0.8, 1.0),              // fracción de (relación − 1) del glue
             seg (0.0, 0.3, 0.55, 1.0),             // fracción de la GR media del glue del estilo
             seg (0.0, 0.5, 1.0, 1.75),             // fracción de la saturación del estilo
             seg (0.0, 0.25, 0.5, 1.0) };           // fracción del ancho sugerido
}

} // namespace dh::lq
