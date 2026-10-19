#pragma once
// Lo que pasa de golpe: caídas de la cinta, crujidos y rayas del vinilo, estática de la radio. Cada evento se
// decide con un hash de (semilla, posición absoluta): la probabilidad por muestra es tasa/fs, así la tasa en
// eventos por segundo es la misma a cualquier frecuencia de muestreo, el resultado no depende del tamaño de bloque
// y, con la posición del host, se repite exacto en cada reproducción (docs/research.md §3.5).
//
// Caídas (§2.19): un polvo o un grumo de óxido separa la cinta de la cabeza un instante. La pérdida por
// separación es 54,6·d/λ dB: se apagan primero los agudos y los graves sobreviven. Por eso una caída devuelve una
// separación d(t) en metros, que el motor convierte en el corte del filtro de pérdidas. Las cintas tienen tramos
// dañados: cada segundo de cinta puede estar dañado (más probable con desgaste) y ahí la tasa se multiplica.

#include "Core.h"

namespace dh::tn {

struct UiEvent { int64_t pos = 0; float depth = 0, length = 0; int kind = 0; };   // kind 0 caída, 1 pop, 2 raya, 3 estática

class Dropouts
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { n = 0; }
    // rate: caídas por segundo en cinta sana; damage: probabilidad de que un segundo de cinta esté dañado.
    void set (double rate, double damage, double depthScale, uint64_t s) { rateS = rate; dmg = damage; depth = depthScale; seed = s; }

    // ¿Empieza una caída en `pos`? Función pura (sin estado): la usa también la interfaz para dibujar las manchas
    // que vienen por la cinta antes de que lleguen a la cabeza.
    static bool fires (uint64_t seed, int64_t pos, double rate, double damage, double fs)
    {
        if (rate <= 0) return false;
        const bool damaged = hashUni (seed, (uint64_t) (pos / (int64_t) fs), 101) < damage;
        return hashUni (seed, (uint64_t) pos, 1) < rate * (damaged ? 15.0 : 1.0) / fs;
    }
    struct Shape { double rise, hold, fall, depth; };
    // Forma: subida 1-5 ms, meseta log-normal (mediana 25 ms), bajada 5-30 ms; profundidad log-normal (m).
    static Shape shape (uint64_t seed, int64_t pos, double fs, double depthScale)
    {
        auto ln = [&] (double med, double sig, int k) { return med * std::exp (sig * (hashUni (seed, (uint64_t) pos, (uint64_t) k) + hashUni (seed, (uint64_t) pos, (uint64_t) k + 1) - 1.0) * 1.7); };
        Shape s;
        s.rise = (0.001 + 0.004 * hashUni (seed, (uint64_t) pos, 3)) * fs;
        s.hold = clampd (ln (0.025, 1.0, 4), 0.004, 0.4) * fs;
        s.fall = (0.005 + 0.025 * hashUni (seed, (uint64_t) pos, 6)) * fs;
        s.depth = clampd (ln (1.2e-6, 0.8, 7), 0.2e-6, 6e-6) * depthScale;
        return s;
    }

    // Separación extra (m) en esta muestra; `ev` recibe la caída nueva si empieza acá.
    inline double process (int64_t pos, UiEvent* ev)
    {
        if (fires (seed, pos, rateS, dmg, fs) && n < kMax)
        {
            const Shape sh = shape (seed, pos, fs, depth);
            D d; d.start = pos; d.rise = sh.rise; d.hold = sh.hold; d.fall = sh.fall; d.depth = sh.depth;
            act[(size_t) n++] = d;
            if (ev) { ev->pos = pos; ev->depth = (float) (d.depth * 1e6); ev->length = (float) ((d.rise + d.hold + d.fall) / fs); ev->kind = 0; }
        }
        double dmax = 0;
        for (int i = 0; i < n;)
        {
            const D& d = act[(size_t) i];
            const double x = (double) (pos - d.start);
            double e;
            if (x < d.rise) e = x / d.rise;
            else if (x < d.rise + d.hold) e = 1.0;
            else if (x < d.rise + d.hold + d.fall) e = 1.0 - (x - d.rise - d.hold) / d.fall;
            else { act[(size_t) i] = act[(size_t) --n]; continue; }
            dmax = std::max (dmax, d.depth * e * e * (3 - 2 * e));
            ++i;
        }
        return dmax;
    }

private:
    struct D { int64_t start = 0; double rise = 1, hold = 1, fall = 1, depth = 0; };
    static constexpr int kMax = 16;
    std::array<D, kMax> act {};
    int n = 0;
    double fs = 48000, rateS = 0, dmg = 0, depth = 1;
    uint64_t seed = 1;
};

// Impulsos con forma (oscilación amortiguada): el crujido del vinilo en tres clases y la estática de la radio.
class Impulses
{
public:
    void prepare (double sampleRate) { fs = sampleRate; reset(); }
    void reset() { n = 0; }

    struct Class { double rate, ampDb, ampSpread, decayMs, hzLo, hzHi; int kind; };

    // Dispara los eventos de esta muestra según las clases; suma las colas activas. `corr`: correlación L/R.
    inline void process (int64_t pos, const Class* cls, int ncls, double corr, uint64_t seed, double& l, double& r, UiEvent* ev)
    {
        for (int c = 0; c < ncls; ++c)
        {
            const Class& k = cls[c];
            if (k.rate <= 0) continue;
            if (hashUni (seed, (uint64_t) pos, 20 + (uint64_t) c) < k.rate / fs) add (pos, k, corr, seed, 40 + 10 * (uint64_t) c, ev);
        }
        tail (l, r);
    }
    // Un pop en un lugar fijo del disco (raya): lo dispara el motor cuando el ángulo pasa por ella.
    inline void scratch (int64_t pos, double ampDb, uint64_t seed, int which, UiEvent* ev)
    {
        Class k { 0, ampDb, 3.0, 2.5, 150, 700, 2 };
        add (pos, k, 0.6, seed ^ (uint64_t) (which * 7919), 90, ev);
    }
    inline void tail (double& l, double& r)
    {
        for (int i = 0; i < n;)
        {
            E& e = act[(size_t) i];
            const double v = e.a * std::sin (e.ph);
            l += v * e.gl; r += v * e.gr;
            e.ph += e.w; e.a *= e.dec;
            if (std::fabs (e.a) < 1e-7) { act[(size_t) i] = act[(size_t) --n]; continue; }
            ++i;
        }
    }

private:
    struct E { double a = 0, dec = 0, ph = 0, w = 0, gl = 1, gr = 1; };
    static constexpr int kMax = 48;
    std::array<E, kMax> act {};
    int n = 0;
    double fs = 48000;

    inline void add (int64_t pos, const Class& k, double corr, uint64_t seed, uint64_t lane, UiEvent* ev)
    {
        if (n >= kMax) return;
        const double u1 = hashUni (seed, (uint64_t) pos, lane), u2 = hashUni (seed, (uint64_t) pos, lane + 1);
        const double u3 = hashUni (seed, (uint64_t) pos, lane + 2), u4 = hashUni (seed, (uint64_t) pos, lane + 3);
        // Amplitud con cola pesada (Pareto, exponente 2): muchos chicos, pocos grandes.
        const double par = std::pow (std::max (1e-3, 1.0 - u1), -0.5) - 1.0;
        const double db = k.ampDb - k.ampSpread * 4.0 + std::min (k.ampSpread * 4.0 + 6.0, 6.0 * std::log2 (1.0 + 3.0 * par));
        E e;
        e.a = dbToGain (db) * (u2 < 0.5 ? -1.0 : 1.0);
        e.dec = std::exp (-1.0 / (k.decayMs * (0.5 + u3) * 0.001 * fs));
        e.w = kTwoPi * (k.hzLo + (k.hzHi - k.hzLo) * u4) / fs;
        e.ph = 0.5 * kPi;                                // empieza en el máximo: un golpe, no un fundido
        const double side = hashBi (seed, (uint64_t) pos, lane + 4) * (1.0 - corr);
        e.gl = 1.0 + side; e.gr = 1.0 - side;
        act[(size_t) n++] = e;
        if (ev && k.kind >= 1) { ev->pos = pos; ev->depth = (float) db; ev->length = (float) (k.decayMs * 0.001); ev->kind = k.kind; }
    }
};

} // namespace dh::tn
