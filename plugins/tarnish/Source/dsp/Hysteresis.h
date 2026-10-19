#pragma once
// Histéresis magnética de Jiles-Atherton (D. C. Jiles y D. L. Atherton, J. Magn. Magn. Mater. 61, 1986), en
// unidades normalizadas por la magnetización de saturación (M_s = 1): m = M/M_s, h = H/M_s. Se integra sobre el
// camino de h, no del tiempo: entre dos submuestras h va en línea recta, así dM/dt = (dM/dH)·(dH/dt) se vuelve
// dm = χ(h, m, δ)·dh y el modelo es independiente de la frecuencia de muestreo (la histéresis real lo es: el lazo
// no depende de la velocidad). RK4 de paso fijo: CPU constante y salida determinista.
//
// Controles → parámetros físicos (docs/research.md §2.2):
//   drive       escala h (cuánta corriente entra a la cabeza)
//   saturation  qué tan temprano llega el techo (escala de h respecto de `a`)
//   bias        −1 sub-bias (lazo abierto: c bajo, k alto, distorsión de cruce) … 0 nominal … +1 sobre-bias
//               (casi anhisterético: c alto, lazo angosto, más limpio)

#include "Core.h"

namespace dh::tn {

struct JaParams { double driveDb = 0.0, saturation = 0.5, bias = 0.0, formulation = 1.0; bool rk4 = true; };

class Hysteresis
{
public:
    // Valores de cinta de Chowdhury (DAFx 2019) llevados a unidades normalizadas: a = 2.2e4/3.5e5, k = 2.7e4/3.5e5.
    static constexpr double kA = 0.0629, kK = 0.0771, kAlpha = 1.6e-3, kC = 0.17;

    void set (const JaParams& p)
    {
        P = p; rk4 = p.rk4;
        a = kA * p.formulation;                       // formulaciones con más coercitividad: lazo más ancho
        const double b = clampd (p.bias, -1.0, 1.0);
        // La cinta polarizada es casi anhisterética: reversibilidad alta, lineal en nivel bajo y con techo suave.
        // Sin bias el lazo se abre (c baja) y aparece la distorsión de cruce en nivel bajo.
        c = clampd (b < 0 ? 0.62 + 0.40 * b : 0.62 + 0.28 * b, 0.03, 0.9);
        k = kK * std::pow (2.0, -0.8 * b) * p.formulation;
        alpha = kAlpha;
        // 0 dBFS de pico con drive 0 y saturation 0.5 lleva h a ≈ 2.2·a: ya se nota la curva, sin aplastar.
        hScale = 2.2 * a * std::pow (2.0, (p.saturation - 0.5) * 4.0);
        calibrate();                                   // con drive 0: el drive no cambia la calibración
        hScale *= dbToGain (p.driveDb);
    }
    void reset() { m = 0; hPrev = 0; dPrev = 1; }

    // Una submuestra: entra audio, sale audio (normalizado para que la ganancia de señal chica sea 1).
    inline double process (double x)
    {
        const double h1 = clampd (x * hScale, -40.0 * a, 40.0 * a);
        m = step (hPrev, h1, m);
        hPrev = h1;
        return m * outGain;
    }
    // Estado para el osciloscopio B-H de la interfaz (h en unidades de `a`, m en [−1, 1]).
    double lastH() const { return hPrev / a; }
    double lastM() const { return m; }
    double smallSignalGain() const { return outGain; }

private:
    JaParams P;
    double a = kA, k = kK, c = kC, alpha = kAlpha, hScale = 1, outGain = 1;
    double m = 0, hPrev = 0; int dPrev = 1; bool rk4 = true;

    // Langevin L(x) = coth x − 1/x y su derivada L'(x) = 1/x² − 1/sinh² x, con una sola exponencial. Cerca de 0
    // las dos pierden precisión (la resta de números grandes): ahí van sus series, que es donde la cinta en
    // silencio pasa todo el tiempo (sin esto el modelo da NaN en silencio, el error más común del modelo).
    static inline void langevin (double x, double& L, double& dL)
    {
        const double ax = std::fabs (x);
        if (ax < 0.02)
        {
            const double x2 = x * x;
            L = x * (1.0 / 3.0 - x2 * (1.0 / 45.0 - x2 * (2.0 / 945.0)));
            dL = 1.0 / 3.0 - x2 * (1.0 / 15.0 - x2 * (2.0 / 189.0));
            return;
        }
        const double inv = 1.0 / ax;
        if (ax > 18.0) { L = (x > 0 ? 1.0 : -1.0) * (1.0 - inv); dL = inv * inv; return; }
        const double e = std::exp (-2.0 * ax), om = 1.0 - e;
        L = (x > 0 ? 1.0 : -1.0) * ((1.0 + e) / om - inv);
        dL = inv * inv - 4.0 * e / (om * om);
    }
    // dm/dh en el punto (h, m) con la dirección δ.
    inline double chi (double h, double mm, int d) const
    {
        const double q = (h + alpha * mm) / a;
        double man, dman; langevin (q, man, dman); dman /= a;
        const double diff = man - mm;
        const double dM = (diff * d > 0) ? 1.0 : 0.0;
        double den = (1 - c) * d * k - alpha * diff;
        if (std::fabs (den) < 1e-9) den = den < 0 ? -1e-9 : 1e-9;
        const double num = (1 - c) * dM * diff / den + c * dman;
        double den2 = 1 - c * alpha * dman;
        if (std::fabs (den2) < 1e-9) den2 = 1e-9;
        return num / den2;
    }
    inline double step (double h0, double h1, double mm)
    {
        const double dh = h1 - h0;
        int d = dh > 0 ? 1 : (dh < 0 ? -1 : dPrev);       // en silencio se queda con el último sentido (sin ruido de δ)
        dPrev = d;
        if (dh == 0.0) return mm;
        const double k1 = chi (h0, mm, d) * dh;
        if (! rk4)                                         // RK2 (Heun): la mitad de evaluaciones
        {
            const double k2 = chi (h1, mm + k1, d) * dh;
            return clampd (mm + 0.5 * (k1 + k2), -1.0, 1.0);
        }
        const double k2 = chi (h0 + 0.5 * dh, mm + 0.5 * k1, d) * dh;
        const double k3 = chi (h0 + 0.5 * dh, mm + 0.5 * k2, d) * dh;
        const double k4 = chi (h1, mm + k3, d) * dh;
        const double out = mm + (k1 + 2 * k2 + 2 * k3 + k4) / 6.0;
        return clampd (out, -1.0, 1.0);
    }
    // Ganancia en el nivel de referencia: un seno de −18 dBFS de pico, después de dos ciclos de asentamiento, mide la
    // amplitud de m y fija outGain para que salga a −18 dBFS. Se calibra sin drive: el drive sube el nivel como en
    // la cinta (la compensación automática lo baja después) y el sub-bias, que casi no responde en nivel bajo, no
    // puede disparar la ganancia. Determinista y barato (se hace al cambiar ajustes).
    void calibrate()
    {
        const double saveM = m, saveH = hPrev; const int saveD = dPrev;
        m = 0; hPrev = 0; dPrev = 1;
        const int n = 256; const double amp = 0.125;     // nivel de referencia de la cinta: −18 dBFS de pico
        double peak = 0;
        for (int cyc = 0; cyc < 3; ++cyc)
            for (int i = 0; i < n; ++i)
            {
                const double h1 = amp * hScale * std::sin (kTwoPi * (i + 1) / n);
                m = step (hPrev, h1, m); hPrev = h1;
                if (cyc == 2) peak = std::max (peak, std::fabs (m));
            }
        outGain = peak > 1e-15 ? amp / peak : 1.0;
        m = saveM; hPrev = saveH; dPrev = saveD;
    }
};

} // namespace dh::tn
