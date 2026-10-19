#pragma once
// Perfiles de las máquinas (docs/research.md §3.4, con los valores de §2.4-§2.19). Cada máquina tiene variantes
// (velocidad, formulación, revoluciones, ancho de banda) que cambian la ecualización, el golpe de cabeza, las
// pérdidas, el ruido y el wow/flutter de fábrica. Son genéricas: ninguna copia un aparato de una marca.

#include "Core.h"

namespace dh::tn {

enum Machine { kStudio = 0, kCassette, kVhs, kVinyl, kRadio, kPhone, kNumMachines };

// Una línea del espectro de velocidad del transporte: frecuencia (Hz; 0 = la de los carretes, que depende del
// radio de la bobina) y peso relativo dentro del grupo (wow o flutter).
struct SpeedLine { double hz, weight; int kind; };      // kind: 0 fija, 1 carrete que entrega, 2 carrete que recibe

struct Profile
{
    const char* name;
    // Cinta: velocidad (m/s), ecualización (constantes de tiempo en s; 0 = no), golpe de cabeza.
    double speed = 0.381, tauHF = 35e-6, tauLF = 0.0, bumpHz = 55, bumpDb = 2.0, lfHz = 25;
    double hfHz = 20000;              // −3 dB de las pérdidas de reproducción a bajo nivel
    double formulation = 1.0;         // ancho del lazo (coercitividad relativa)
    double satOffset = 0.0;           // corrimiento de la saturación (las casetes saturan antes)
    double selfErase = 0.0;           // cuánto borra el bias los agudos fuertes (0-1)
    double hissDb = -68;              // ruido propio, dB respecto del nivel de referencia
    double wowPct = 0.03, flutterPct = 0.02;    // wow y flutter de fábrica (ponderados, %)
    double hubR = 0.05, fullR = 0.13, thick = 50e-6, tapeLenS = 2000;   // geometría de las bobinas
    double capstanD = 0.0254, rollerD = 0.025;
    bool magnetic = true;
};

// Variantes por máquina. Los nombres son los de la interfaz.
inline const std::array<std::vector<const char*>, kNumMachines>& variantNames()
{
    static const std::array<std::vector<const char*>, kNumMachines> v {{
        { "30 ips", "15 ips", "7.5 ips", "3.75 ips" },
        { "Ferric", "Chrome", "Metal", "Half speed" },
        { "SP", "EP" },
        { "33", "45", "78" },
        { "Narrow", "Wide" },
        { "Landline", "Mobile" },
    }};
    return v;
}
inline const std::array<const char*, kNumMachines>& machineNames()
{
    static const std::array<const char*, kNumMachines> n { "Studio", "Cassette", "Video", "Vinyl", "Radio", "Phone" };
    return n;
}

inline Profile profile (int machine, int variant)
{
    Profile p;
    switch (machine)
    {
        case kStudio:
        {
            static const double ips[] = { 30, 15, 7.5, 3.75 };
            const int v = std::clamp (variant, 0, 3);
            p.name = "Studio";
            p.speed = ips[v] * 0.0254;
            p.tauHF = v == 0 ? 17.5e-6 : (v == 1 ? 35e-6 : (v == 2 ? 70e-6 : 90e-6));
            p.bumpHz = 55.0 * ips[v] / 15.0 * (v == 0 ? 0.95 : 1.0);
            p.bumpDb = v == 0 ? 1.8 : (v == 1 ? 2.0 : 2.5);
            p.lfHz = v <= 1 ? 25 : 22;
            p.hfHz = v == 0 ? 21000 : (v == 1 ? 20000 : (v == 2 ? 14000 : 9000));
            p.hissDb = v == 0 ? -72 : (v == 1 ? -68 : (v == 2 ? -63 : -58));
            p.wowPct = v <= 1 ? 0.03 : (v == 2 ? 0.06 : 0.1); p.flutterPct = p.wowPct * 0.6;
            p.hubR = 0.05; p.fullR = 0.13; p.thick = 50e-6; p.capstanD = 0.0254; p.rollerD = 0.025;
            break;
        }
        case kCassette:
        {
            const int v = std::clamp (variant, 0, 3);
            p.name = "Cassette";
            p.speed = v == 3 ? 0.0238 : 0.0476;
            p.tauHF = v == 0 || v == 3 ? 120e-6 : 70e-6; p.tauLF = 3180e-6;
            p.bumpHz = v == 3 ? 140 : 55; p.bumpDb = 1.5; p.lfHz = v == 3 ? 120 : 30;
            p.hfHz = v == 3 ? 5000 : (v == 0 ? 13000 : (v == 1 ? 14500 : 15500));
            p.formulation = v == 2 ? 1.6 : (v == 1 ? 1.25 : 1.0);
            p.satOffset = v == 2 ? -0.05 : (v == 1 ? 0.05 : 0.12);
            p.selfErase = v == 2 ? 0.3 : 0.55;
            p.hissDb = v == 0 ? -52 : (v == 1 ? -56 : (v == 2 ? -57 : -46));
            p.wowPct = v == 3 ? 0.3 : 0.12; p.flutterPct = v == 3 ? 0.15 : 0.08;
            p.hubR = 0.011; p.fullR = 0.025; p.thick = 15e-6; p.capstanD = 0.002; p.rollerD = 0.010;
            break;
        }
        case kVhs:
        {
            const int v = std::clamp (variant, 0, 1);
            p.name = "Video";
            p.speed = v == 0 ? 0.03335 : 0.01112;
            p.tauHF = 120e-6; p.tauLF = 3180e-6;
            p.bumpHz = 130; p.bumpDb = 1.0; p.lfHz = 100;
            p.hfHz = v == 0 ? 9000 : 4500;
            p.formulation = 1.1; p.satOffset = 0.1; p.selfErase = 0.5;
            p.hissDb = v == 0 ? -44 : -40;
            p.wowPct = v == 0 ? 0.25 : 0.35; p.flutterPct = 0.12;
            p.hubR = 0.013; p.fullR = 0.04; p.thick = 20e-6; p.capstanD = 0.0035; p.rollerD = 0.012;
            break;
        }
        case kVinyl:
        {
            const int v = std::clamp (variant, 0, 2);
            p.name = "Vinyl"; p.magnetic = false;
            p.speed = v == 0 ? 33.333 : (v == 1 ? 45.0 : 78.0);          // rpm
            p.bumpHz = 0; p.bumpDb = 0; p.lfHz = v == 2 ? 120 : 20;
            p.hfHz = v == 2 ? 5000 : 18000;
            p.hissDb = v == 2 ? -38 : -60;
            p.wowPct = 0.03; p.flutterPct = 0.02;
            break;
        }
        case kRadio:
        {
            const int v = std::clamp (variant, 0, 1);
            p.name = "Radio"; p.magnetic = false;
            p.hfHz = v == 0 ? 3500 : 7000; p.lfHz = 150;
            p.hissDb = -50; p.wowPct = 0; p.flutterPct = 0;
            break;
        }
        default:
        {
            const int v = std::clamp (variant, 0, 1);
            p.name = "Phone"; p.magnetic = false;
            p.hfHz = 3400; p.lfHz = 300;
            p.hissDb = v == 0 ? -50 : -56; p.wowPct = 0; p.flutterPct = 0;
            break;
        }
    }
    return p;
}

// Líneas del transporte por máquina (grupo wow: lo lento; grupo flutter: cabrestante, rodillo, motor).
// Frecuencias fijas en Hz se calculan de la velocidad y los diámetros (docs/research.md §2.9).
inline void speedLines (const Profile& p, int machine, std::vector<SpeedLine>& wow, std::vector<SpeedLine>& flutter)
{
    wow.clear(); flutter.clear();
    if (machine == kVinyl)
    {
        wow.push_back ({ p.speed / 60.0, 1.0, 0 });               // excentricidad, a la rotación del plato
        wow.push_back ({ 2.0 * p.speed / 60.0, 0.35, 0 });        // alabeo, dos veces por vuelta
        flutter.push_back ({ 8.0, 0.6, 0 });                      // motor y correa
        flutter.push_back ({ 16.3, 0.3, 0 });
        return;
    }
    if (! p.magnetic) return;
    const double capstan = p.speed / (kPi * p.capstanD), roller = p.speed / (kPi * p.rollerD);
    if (machine != kVhs)                                          // en el video los carretes giran a 0,1-0,4 Hz: es deriva
    {
        wow.push_back ({ 0.0, 0.25, 1 });                         // carrete que entrega (se acelera al vaciarse)
        wow.push_back ({ 0.0, 0.18, 2 });                         // carrete que recibe
    }
    wow.push_back ({ roller, 0.75, 0 });                          // rodillo de presión
    if (machine == kVhs) wow.push_back ({ 2.1, 0.9, 0 });         // el servo del cabrestante buscando
    flutter.push_back ({ capstan, 1.0, 0 });
    flutter.push_back ({ 2.0 * capstan, 0.35, 0 });
    flutter.push_back ({ machine == kStudio ? 50.0 / 4.0 : 23.0, 0.25, 0 });   // motor
}

} // namespace dh::tn
