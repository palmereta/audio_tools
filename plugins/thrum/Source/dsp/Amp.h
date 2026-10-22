#pragma once
// Los cuatro amplificadores de Thrum (docs/research.md §3, §8), genéricos: limpio americano, crunch británico, alta
// ganancia moderna y combo chico saturado. Corren sobremuestreados (2x, 4x u 8x con medias bandas de fase lineal:
// latencia fija y entera, la misma para la salida DI).
//
// Cada etapa de triodo: estante del capacitor de cátodo (cuánto grave se amplifica), ganancia, la curva del triodo
// (asimétrica: conducción de grilla arriba, corte abajo, invierte la fase como el real), corrimiento de polarización
// por la corriente de grilla (el bloqueo que comprime y "escupe" en alta ganancia), capacitor de acople (pasa altos) y
// la capacidad de Miller (pasa bajos). El tonestack es el FMV pasivo con su transferencia de tercer orden (Yeh y Smith,
// DAFx 2006; verificada contra el análisis nodal del circuito), discretizado por la bilineal cuando se mueve un control.
// Etapa de potencia push-pull: presencia y profundidad como estantes (la realimentación negativa que se les quita), una
// saturación simétrica con el "sag" de la fuente (la envolvente de la corriente baja la tensión: menos margen, más
// compresión, menos nivel) y el transformador de salida (pasa banda).

#include "Base.h"

namespace dh::gt {

enum AmpModel { kAmpClean, kAmpCrunch, kAmpHigh, kAmpCombo, kNumAmps };
inline const char* ampName (int m) { static const char* n[] = { "American clean", "British crunch", "Modern high gain", "Small combo" }; return n[std::clamp (m, 0, 3)]; }

struct AmpParams
{
    int model = kAmpClean;
    float gain = 0.5f, bass = 0.5f, mid = 0.5f, treble = 0.5f, presence = 0.5f, depth = 0.3f, master = 0.5f, sag = 0.5f;
    bool bright = false;
    bool autoLevel = true;           // compensar el nivel de Gain (apagado: Gain sube el volumen, como en un amplificador)
    float levelDb = 0.0f;
    int quality = 4;                 // factor de sobremuestreo
};

// Transferencia del tonestack FMV (Yeh y Smith 2006): t agudos, m medios, l graves (0..1), componentes en Ω y F.
// Coeficientes de s⁰..s³ del numerador y el denominador.
struct FmvParts { double R1, R2, R3, R4, C1, C2, C3; };
inline void fmvCoefficients (const FmvParts& c, double t, double m, double l, double* B, double* A)
{
    const double R1 = c.R1, R2 = c.R2, R3 = c.R3, R4 = c.R4, C1 = c.C1, C2 = c.C2, C3 = c.C3;
    B[0] = 0;
    B[1] = t * C1 * R1 + m * C3 * R3 + l * (C1 * R2 + C2 * R2) + (C1 * R3 + C2 * R3);
    B[2] = t * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4) - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3)
         + m * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + l * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4)
         + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);
    B[3] = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4) - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
         + m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + t * C1 * C2 * C3 * R1 * R3 * R4 - t * m * C1 * C2 * C3 * R1 * R3 * R4
         + t * l * C1 * C2 * C3 * R1 * R2 * R4;
    A[0] = 1;
    A[1] = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + m * C3 * R3 + l * (C1 * R2 + C2 * R2);
    A[2] = m * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + l * m * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3)
         - m * m * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + l * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4)
         + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);
    A[3] = l * m * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4) - m * m * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4)
         + m * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4) + l * C1 * C2 * C3 * R1 * R2 * R4
         + C1 * C2 * C3 * R1 * R3 * R4;
}

// Una etapa de triodo. Las tensiones van normalizadas a la polarización de la grilla de cada etapa (1 = la grilla llega
// a conducir): la curva es lineal hasta ~0,5 y satura suave arriba (conducción de grilla) y algo más tarde abajo (corte),
// invierte la fase como el real, y la ganancia de la etapa (la de un 12AX7 con su carga, 30-40 veces) se aplica a la
// salida en las mismas unidades, antes del divisor o el pote que lleva a la etapa siguiente.
struct Triode
{
    float gain = 35.0f, posSat = 1.0f, negSat = 1.6f, blockK = 0.0f;
    Biquad cathode; Hp1 couple; Lp1 miller;
    float blk = 0.0f, blkA = 0.0f, blkR = 0.0f;
    void setup (float fs, float g, float kHz, float shelfDb, float coupleHz, float millerHz, float block, float pos = 1.0f, float neg = 1.6f)
    {
        gain = g; posSat = pos; negSat = neg; blockK = block;
        cathode.lowShelf (kHz, 0.6, shelfDb, fs);                     // el capacitor de cátodo: cuánto grave se amplifica
        couple.set (coupleHz, fs); miller.set (millerHz, fs);
        blkA = 1.0f - fastExp (-1.0f / (0.0008f * fs)); blkR = 1.0f - fastExp (-1.0f / (0.045f * fs));
    }
    void reset() { cathode.reset(); couple.reset(); miller.reset(); blk = 0; }
    inline float process (float x)
    {
        float v = cathode.process (x);
        // Bloqueo: la corriente de grilla carga el capacitor de acople y corre el punto de trabajo hacia el corte.
        const float grid = std::max (0.0f, v - 0.7f * posSat);
        blk += (grid - blk) * (grid > blk ? blkA : blkR);
        v -= blockK * blk;
        const float y = v > 0.0f ? posSat * fastTanh (v / posSat) : negSat * fastTanh (v / negSat);
        return miller.process (couple.process (-y * gain));           // el triodo invierte
    }
};

class Amp
{
public:
    void prepare (float sampleRate, int maxBlock)
    {
        fs = sampleRate; maxB = std::max (16, maxBlock);
        os.prepare (P.quality, maxB);
        configured = false;
        set (P);
        reset();
    }
    void reset()
    {
        os.reset(); for (auto& t : tri) t.reset(); stack.reset(); inHp.reset(); bright.reset(); pres.reset(); deep.reset();
        otHp.reset(); otLp.reset(); preLp.reset(); sagEnv = 0; tight.reset();
    }
    int latency() const { return Oversampler::latencyFor (os.getFactor()); }
    // Para medir la tabla de nivel automático (thrumtest --amp-levels): sin la compensación.
    static inline bool rawLevel = false;
    static int latencyFor (int quality) { return Oversampler::latencyFor (quality); }

    void set (const AmpParams& p)
    {
        const bool qualityChanged = ! configured || p.quality != P.quality;
        const bool modelChanged = ! configured || p.model != P.model || qualityChanged;
        P = p;
        if (qualityChanged) { os.prepare (P.quality, maxB); os.reset(); }
        const float fo = fs * (float) os.getFactor();
        if (modelChanged || p.bright != lastBright || p.autoLevel != lastAuto || std::fabs (p.gain - lastGain) > 1e-4f) design (fo);
        designTone (fo);
        configured = true;
    }

    // En el lugar, mono.
    void process (float* x, int n)
    {
        for (int i0 = 0; i0 < n; i0 += maxB)
        {
            const int m = std::min (maxB, n - i0);
            float* up = os.upsample (x + i0, m);
            const int N = m * os.getFactor();
            for (int i = 0; i < N; ++i) up[i] = tick (up[i]);
            os.downsample (up, x + i0, m);
        }
    }

private:
    float fs = 48000.0f; int maxB = 512; bool configured = false;
    AmpParams P; float lastGain = -1.0f; bool lastBright = false, lastAuto = true;
    Oversampler os;
    std::array<Triode, 5> tri; std::array<float, 5> divider {};      // divisor fijo después de cada etapa
    int nTri = 2, stackAfter = 1, knobAfter = 0;
    Iir3 stack; Hp1 inHp, tight; Biquad bright, pres, deep; Hp1 otHp; Biquad otLp; Lp1 preLp;
    float inScale = 0.67f, knob = 1.0f, powerGain = 1.0f, outGain = 1.0f, sagEnv = 0.0f, sagA = 0.0f, sagR = 0.0f, sagAmt = 0.5f;
    float powerSat = 1.0f, masterGain = 1.0f, level = 1.0f, autoLevel = 1.0f; bool follower = false;

    // Nivel automático: lo que sale cada modelo con un rasgueo de referencia (DI de −26,5 dB RMS, master 5) según Gain de
    // 0 a 10, medido con el propio motor; se compensa hacia un objetivo que sube apenas con la ganancia (−24 dB + 0,3 dB
    // por punto), así Gain cambia el carácter y no tanto el volumen (un teclista no tiene un volumen de amplificador a mano).
    static float autoLevelDb (int model, float g)
    {
        static const float meas[4][11] = {          // thrumtest --amp-levels
            { -73.5f, -66.5f, -54.9f, -46.5f, -40.2f, -35.3f, -31.3f, -27.9f, -25.0f, -22.5f, -20.4f },
            { -58.5f, -51.4f, -40.0f, -32.1f, -27.3f, -24.3f, -22.4f, -21.1f, -20.2f, -19.6f, -19.1f },
            { -29.8f, -24.1f, -17.9f, -15.7f, -15.0f, -14.8f, -14.7f, -14.6f, -14.5f, -14.4f, -14.4f },
            { -60.5f, -53.5f, -42.0f, -33.6f, -27.6f, -23.1f, -19.8f, -17.3f, -15.5f, -14.1f, -13.1f } };
        const float x = clampf (g, 0.0f, 1.0f) * 10.0f;
        const int i = std::min (9, (int) x); const float t = x - (float) i;
        const float m = lerp (meas[std::clamp (model, 0, 3)][i], meas[std::clamp (model, 0, 3)][i + 1], t);
        return clampf (-24.0f + 0.3f * x - m, -18.0f, 30.0f);
    }

    static FmvParts parts (int m)
    {
        switch (m)
        {
            case kAmpClean: return { 250e3, 250e3, 10e3, 100e3, 250e-12, 100e-9, 47e-9 };      // tipo "blackface"
            case kAmpCrunch: return { 220e3, 1e6, 22e3, 33e3, 470e-12, 22e-9, 22e-9 };         // tipo británico de 1980
            case kAmpHigh: return { 250e3, 1e6, 25e3, 47e3, 470e-12, 22e-9, 22e-9 };           // moderno (inferido)
            default: return { 1e6, 1e6, 10e3, 100e3, 100e-12, 47e-9, 22e-9 };                  // combo chico (inferido)
        }
    }

    // La cadena de cada modelo (docs/research.md §3): ganancias de etapa de un 12AX7 (30-40), divisores entre etapas,
    // dónde está el pote de ganancia (curva logarítmica) y dónde el tonestack. Con la guitarra en ~0,1-0,4 V de pico:
    //   limpio      dos etapas; con Gain en 3-4 la segunda no llega a saturar (rompe arriba de 6 con un rasgueo fuerte);
    //   crunch      tres etapas y seguidor catódico; la tercera polarizada en frío (recorta antes de un lado);
    //   alta        cinco etapas con acoples chicos (graves apretados antes de saturar) y Miller bajo;
    //   combo       dos etapas sin master: la potencia clase A (cátodo) satura y se hunde.
    void design (float fo)
    {
        lastGain = P.gain; lastBright = P.bright; lastAuto = P.autoLevel;
        const float g = clampf (P.gain, 0.0f, 1.0f);
        knob = 0.002f + 0.998f * std::pow (g, 2.6f);                  // pote logarítmico ("10 % a la mitad")
        // Sin "Auto level", la compensación queda fija en la de Gain 5: los dos modos suenan igual de fuerte ahí, y lejos de 5
        // Gain cambia el volumen como en el amplificador real (el limpio, ~38 dB entre 0 y 5).
        autoLevel = rawLevel ? 1.0f : dbToGain (autoLevelDb (P.model, P.autoLevel ? g : 0.5f));
        inHp.set (25.0f, fo);
        bright.highShelf (2200.0, 0.6, P.bright ? 9.0 * (1.0 - g) : 0.0, fo);
        divider.fill (1.0f);
        switch (P.model)
        {
            case kAmpClean:
                nTri = 2; knobAfter = 0; stackAfter = 1;
                tri[0].setup (fo, 35.0f, 60.0f, -1.0f, 12.0f, 20000.0f, 0.05f, 1.0f, 1.7f);
                tri[1].setup (fo, 35.0f, 60.0f, -1.0f, 15.0f, 16000.0f, 0.10f, 1.0f, 1.7f);
                powerGain = 0.10f; powerSat = 1.4f; sagAmt = 0.3f; outGain = 0.55f; follower = false;
                tight.set (12.0f, fo);
                break;
            case kAmpCrunch:
                nTri = 3; knobAfter = 0; stackAfter = 3;
                tri[0].setup (fo, 35.0f, 250.0f, -4.0f, 20.0f, 18000.0f, 0.15f, 1.0f, 1.7f);
                tri[1].setup (fo, 35.0f, 500.0f, -6.0f, 60.0f, 12000.0f, 0.25f, 1.0f, 1.5f);
                tri[2].setup (fo, 30.0f, 200.0f, -2.0f, 30.0f, 9000.0f, 0.35f, 1.0f, 0.75f);   // polarizada en frío
                divider[1] = 0.15f;
                powerGain = 0.05f; powerSat = 1.0f; sagAmt = 0.5f; outGain = 0.75f; follower = true;
                tight.set (35.0f, fo);
                break;
            case kAmpHigh:
                nTri = 5; knobAfter = 0; stackAfter = 2;
                tri[0].setup (fo, 40.0f, 400.0f, -5.0f, 25.0f, 16000.0f, 0.2f, 1.0f, 1.7f);
                tri[1].setup (fo, 40.0f, 600.0f, -4.0f, 70.0f, 10000.0f, 0.35f, 1.0f, 1.4f);
                tri[2].setup (fo, 35.0f, 300.0f, -2.0f, 45.0f, 8000.0f, 0.35f, 1.0f, 1.3f);
                tri[3].setup (fo, 30.0f, 200.0f, -1.5f, 30.0f, 7000.0f, 0.3f, 1.0f, 1.2f);
                tri[4].setup (fo, 20.0f, 120.0f, -1.0f, 20.0f, 7500.0f, 0.2f, 1.0f, 1.2f);
                divider[1] = 0.22f; divider[2] = 0.2f; divider[3] = 0.15f;
                powerGain = 0.05f; powerSat = 1.0f; sagAmt = 0.4f; outGain = 0.7f; follower = true;
                tight.set (45.0f, fo);
                break;
            default:
                nTri = 2; knobAfter = 0; stackAfter = 2;
                tri[0].setup (fo, 40.0f, 150.0f, -2.0f, 25.0f, 16000.0f, 0.12f, 1.0f, 1.7f);
                tri[1].setup (fo, 30.0f, 200.0f, -2.0f, 40.0f, 11000.0f, 0.2f, 1.0f, 1.5f);
                powerGain = 0.22f; powerSat = 0.8f; sagAmt = 0.85f; outGain = 0.9f; follower = false;
                tight.set (40.0f, fo);
                break;
        }
        preLp.set (P.model == kAmpHigh ? 6500.0f : 12000.0f, fo);
        otHp.set (P.model == kAmpCombo ? 70.0f : 55.0f, fo);
        otLp.lowpass (P.model == kAmpClean ? 9500.0 : 8000.0, 0.65, fo);
        sagA = 1.0f - fastExp (-1.0f / (0.012f * fo)); sagR = 1.0f - fastExp (-1.0f / (0.18f * fo));
    }

    void designTone (float fo)
    {
        const double t = clampf (P.treble, 0.0f, 1.0f), m = clampf (P.mid, 0.0f, 1.0f);
        const double l = std::exp ((clampf (P.bass, 0.0f, 1.0f) - 1.0) * 3.4);       // pote de graves logarítmico
        double B[4], A[4];
        fmvCoefficients (parts (P.model), t, m, l, B, A);
        stack.fromAnalog (B, A, fo, 1000.0);
        pres.highShelf (3800.0, 0.7, 9.0 * (P.presence - 0.35), fo);
        deep.lowShelf (95.0, 0.8, 7.0 * P.depth, fo);
        // Master (y en el combo, el volumen de la potencia): de −24 a +6 dB.
        masterGain = dbToGain (-24.0f + 30.0f * clampf (P.master, 0.0f, 1.0f));
        level = dbToGain (P.levelDb);
    }

    inline float tick (float x)
    {
        float v = bright.process (inHp.process (x)) * inScale;
        for (int k = 0; k < nTri; ++k)
        {
            v = tri[(size_t) k].process (v) * divider[(size_t) k];
            if (k == knobAfter) v *= knob;
            if (k == stackAfter - 1) v = stack.process (v);
        }
        if (stackAfter > nTri) v = stack.process (v);
        v = preLp.process (v);
        // Seguidor catódico: recorta suave cuando la grilla pasa la tensión de la placa (sólo hacia arriba).
        if (follower) { const float c = 18.0f; v = v > 0.0f ? c * fastTanh (v / c) : v; }
        // Potencia: master, profundidad y presencia, saturación con sag.
        v = tight.process (v) * masterGain * powerGain;
        v = pres.process (deep.process (v));
        const float s = sagAmt * clampf (P.sag, 0.0f, 1.0f);
        const float headroom = 1.0f - 0.45f * s * std::min (1.0f, sagEnv);
        float y = powerSat * headroom * fastTanh (v / (powerSat * headroom));
        // Cruce del push-pull: una zona muerta suave en señal muy fuerte (más audible en el combo).
        y = y - 0.02f * powerSat * fastTanh (y * 40.0f) * (P.model == kAmpCombo ? 1.5f : 0.5f) * std::min (1.0f, std::fabs (v));
        const float a = std::fabs (y);
        sagEnv += (a - sagEnv) * (a > sagEnv ? sagA : sagR);
        y = otLp.process (otHp.process (y));
        return y * outGain * level * autoLevel * (1.0f - 0.25f * s * std::min (1.0f, sagEnv));
    }
};

} // namespace dh::gt
