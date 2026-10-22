#pragma once
// Los kits de fábrica del motor digital (docs/research.md §7). No hay ROM de ninguna máquina: cada muestra se sintetiza
// acá, una sola vez al cargar el plugin, con modelos propios:
//   - parches de membrana con los modos de Bessel de una membrana circular (1, 1.594, 2.136, 2.296…), la caída de
//     altura por tensión al golpear, el mazo o el palo como un pulso corto y el bordonero como ruido filtrado que
//     sigue a la energía del parche;
//   - platillos y charles como muchos modos inarmónicos más ruido por bandas, cada banda con su envolvente;
//   - metales (barras libres: 1, 2.756, 5.404, 8.933; chapas: modos densos al azar), madera (dos o tres modos),
//     FM de dos operadores, y las voces analógicas de Pallor "muestreadas";
//   - una sala propia (red de cuatro líneas con difusión) y la compuerta sobre la sala para las cajas de 1984.
// Cada sonido se genera al doble de la frecuencia del kit y se baja con el diezmador de media banda compartido (el
// filtro antialias del muestreador), se recorta, se le da un fundido final y se normaliza a su nivel de slot.
// Todo es determinista (semillas fijas) y no depende de la frecuencia del host.

#include "Digital.h"
#include "Oversampling.h"
#include <cmath>
#include <functional>

namespace dh::pal::kitgen {

struct KitInfo { const char* name; double rate; int chain; const char* desc; };
inline const std::vector<KitInfo>& kits()
{
    static const std::vector<KitInfo> k = {
        { "Studio 82", 28000.0, kChain8, "Acoustic kit, dry and tight, 8-bit companded at 28 kHz" },
        { "Pop 84", 25000.0, kChain8, "Punchy acoustic kit with gated rooms on snare, claps and toms, 8-bit at 25 kHz" },
        { "Electro 81", 26040.0, kChain26, "Pallor's own analog voices sampled into a 12-bit 26 kHz sampler" },
        { "Hexa 84", 31250.0, kChain8, "The hexagonal electronic kit sampled at 8 bits: bends, noise and clicks" },
        { "Crunch 88", 26040.0, kChain26, "Big, hot acoustic hits through a 12-bit 26 kHz sampler" },
        { "Studio 91", 40000.0, kChain40, "Roomier acoustic kit, 12-bit at 40 kHz" },
        { "Foundry", 32000.0, kChain8, "Industrial: struck sheets, pipes, barrels and chains, 8-bit at 32 kHz" },
        { "FM 85", 31250.0, kChain8, "Two-operator FM drums: thin, glassy, cold" },
    };
    return k;
}
inline int numKits() { return (int) kits().size(); }

// ── Herramientas de síntesis (double, sin apuro: corre una vez) ──
struct Rng64
{
    uint64_t s;
    explicit Rng64 (uint64_t seed) : s (seed * 0x9E3779B97F4A7C15ull + 1) {}
    double uni() { s ^= s >> 12; s ^= s << 25; s ^= s >> 27; return (double) ((s * 0x2545F4914F6CDD1Dull) >> 11) * (1.0 / 9007199254740992.0); }
    double bi() { return 2.0 * uni() - 1.0; }
};

struct Bq
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
    void set (double B0, double B1, double B2, double A0, double A1, double A2) { b0 = B0 / A0; b1 = B1 / A0; b2 = B2 / A0; a1 = A1 / A0; a2 = A2 / A0; }
    static Bq lp (double f, double q, double fs) { Bq b; const double w = 2 * kPiD * std::min (f, 0.49 * fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q); b.set ((1 - c) / 2, 1 - c, (1 - c) / 2, 1 + al, -2 * c, 1 - al); return b; }
    static Bq hp (double f, double q, double fs) { Bq b; const double w = 2 * kPiD * std::min (f, 0.49 * fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q); b.set ((1 + c) / 2, -(1 + c), (1 + c) / 2, 1 + al, -2 * c, 1 - al); return b; }
    static Bq bp (double f, double q, double fs) { Bq b; const double w = 2 * kPiD * std::min (f, 0.49 * fs) / fs, c = std::cos (w), al = std::sin (w) / (2 * q); b.set (al, 0, -al, 1 + al, -2 * c, 1 - al); return b; }
};

struct Gen
{
    double fs; std::vector<double> x; Rng64 rng;
    Gen (double seconds, double rate, uint64_t seed) : fs (rate), x ((size_t) (seconds * rate), 0.0), rng (seed) {}
    size_t n() const { return x.size(); }
    double t60c (double t60) const { return std::exp (-6.907755 / std::max (1e-4, t60 * fs)); }
};

struct M { double f, amp, t60; };

// Modos a frecuencia fija (fasor complejo por modo).
inline void modes (Gen& g, const std::vector<M>& ms, double at = 0.0, double attackMs = 0.0)
{
    const size_t i0 = (size_t) (at * g.fs);
    const double atk = attackMs * 1e-3 * g.fs;
    for (const auto& m : ms)
    {
        if (m.f <= 0 || m.f >= 0.47 * g.fs || std::fabs (m.amp) < 1e-12) continue;
        const double w = 2 * kPiD * m.f / g.fs, r = g.t60c (m.t60), c = std::cos (w) * r, s = std::sin (w) * r;
        double re = 0, im = m.amp;                      // fase de coseno: el golpe arranca en su máximo
        // Fase inicial al azar (los modos de un metal no arrancan juntos).
        const double ph = 2 * kPiD * g.rng.uni();
        re = m.amp * std::sin (ph); im = m.amp * std::cos (ph);
        for (size_t i = i0; i < g.n(); ++i)
        {
            const double k = (double) i - (double) i0;
            const double e = atk > 0 && k < atk ? k / atk : 1.0;
            g.x[i] += im * e;
            const double nr = re * c - im * s, ni = re * s + im * c; re = nr; im = ni;
            if (std::fabs (re) + std::fabs (im) < 1e-7) break;
        }
    }
}

// Parche: modos de membrana con caída de altura al golpear (la tensión sube con la amplitud). Arrancan en seno.
inline void membrane (Gen& g, double f0, const std::vector<M>& rel, double drop, double dropTau, double at = 0.0)
{
    const size_t i0 = (size_t) (at * g.fs);
    for (const auto& m : rel)
    {
        const double f = f0 * m.f;
        if (f >= 0.47 * g.fs) continue;
        const double r = g.t60c (m.t60);
        double ph = 0, a = m.amp;
        const double dc = std::exp (-1.0 / (dropTau * g.fs));
        double d = drop;
        for (size_t i = i0; i < g.n(); ++i)
        {
            g.x[i] += a * std::sin (ph);
            ph += 2 * kPiD * f * (1.0 + d) / g.fs;
            d *= dc; a *= r;
            if (a < 1e-7) break;
        }
    }
}

struct NoiseSpec { int type = 0; double f = 3000, q = 0.7; double f2 = 0; double atkMs = 0.5, holdMs = 0, t60 = 0.2, amp = 1; double at = 0; double t60b = 0, splitB = 0; };
// Ruido filtrado (0 pasa bajos, 1 pasa altos, 2 pasa banda; f2 > 0 agrega un pasa bajos) con ataque, meseta y caída
// (con t60b > 0 la caída tiene dos tramos: el segundo con `splitB` del nivel).
inline void noise (Gen& g, const NoiseSpec& s)
{
    Bq f = s.type == 0 ? Bq::lp (s.f, s.q, g.fs) : s.type == 1 ? Bq::hp (s.f, s.q, g.fs) : Bq::bp (s.f, s.q, g.fs);
    Bq f2 = Bq::lp (s.f2 > 0 ? s.f2 : 0.45 * g.fs, 0.707, g.fs);
    const size_t i0 = (size_t) (s.at * g.fs);
    const double atk = std::max (1.0, s.atkMs * 1e-3 * g.fs), hold = s.holdMs * 1e-3 * g.fs;
    const double r = g.t60c (s.t60), rb = s.t60b > 0 ? g.t60c (s.t60b) : 0;
    double e = 1, eb = s.splitB;
    for (size_t i = i0; i < g.n(); ++i)
    {
        const double k = (double) (i - i0);
        double env = k < atk ? k / atk : 1.0;
        if (k >= atk + hold) { env *= (1.0 - s.splitB) * e + eb; e *= r; eb *= rb; }
        else env *= 1.0;
        double v = f.run (g.rng.bi());
        if (s.f2 > 0) v = f2.run (v);
        g.x[i] += s.amp * env * v;
        if (k > atk + hold && e < 1e-6 && eb < 1e-6) break;
    }
}

// Pulso del palo o del mazo: medio coseno de ancho `ms`, por un pasa bajos.
inline void click (Gen& g, double ms, double amp, double lpHz, double at = 0.0)
{
    Bq f = Bq::lp (lpHz, 0.707, g.fs);
    const size_t i0 = (size_t) (at * g.fs), w = std::max<size_t> (2, (size_t) (ms * 1e-3 * g.fs));
    for (size_t i = i0; i < std::min (g.n(), i0 + w + (size_t) (0.01 * g.fs)); ++i)
    {
        const double k = (double) (i - i0);
        const double p = k < (double) w ? std::sin (kPiD * k / (double) w) : 0.0;
        g.x[i] += amp * f.run (p);
    }
}

// FM de dos operadores con envolventes exponenciales, altura con caída.
struct FmSpec { double fc, ratio, index, indexTau, t60, pitch = 0, pitchTau = 0.02, amp = 1, fb = 0; };
inline void fm (Gen& g, const FmSpec& s)
{
    double pc = 0, pm = 0, prev = 0;
    const double r = g.t60c (s.t60), ri = std::exp (-1.0 / (s.indexTau * g.fs)), rp = std::exp (-1.0 / (s.pitchTau * g.fs));
    double a = s.amp, idx = s.index, pe = s.pitch;
    for (size_t i = 0; i < g.n(); ++i)
    {
        const double f = s.fc * (1.0 + pe);
        const double mod = std::sin (pm + s.fb * prev);
        prev = mod;
        g.x[i] += a * std::sin (pc + idx * mod);
        pc += 2 * kPiD * f / g.fs; pm += 2 * kPiD * f * s.ratio / g.fs;
        a *= r; idx *= ri; pe *= rp;
        if (a < 1e-7) break;
    }
}

// Sala: cuatro líneas con matriz de Hadamard, pasa bajos en el lazo y difusión de entrada. gateMs > 0 cierra la
// sala de golpe a ese tiempo (en 15 ms): la sala comprimida y con compuerta de las cajas de 1984.
inline void room (Gen& g, double t60, double mix, double preMs, double lpHz, double gateMs = 0.0, double squash = 0.0)
{
    const double ms[4] = { 23.3, 29.1, 35.9, 41.7 }, ap[3] = { 4.7, 7.3, 11.3 };
    std::vector<double> line[4], apb[3]; int lw[4] {}, aw[3] {}, len[4] {}, alen[3] {};
    for (int k = 0; k < 4; ++k) { len[k] = (int) (ms[k] * 1e-3 * g.fs); line[k].assign ((size_t) len[k], 0.0); }
    for (int k = 0; k < 3; ++k) { alen[k] = (int) (ap[k] * 1e-3 * g.fs); apb[k].assign ((size_t) alen[k], 0.0); }
    double gain[4]; for (int k = 0; k < 4; ++k) gain[k] = std::pow (10.0, -3.0 * ms[k] * 1e-3 / t60);
    Bq lpf[4]; for (auto& b : lpf) b = Bq::lp (lpHz, 0.6, g.fs);
    const int pre = (int) (preMs * 1e-3 * g.fs);
    std::vector<double> wet (g.n(), 0.0);
    double env = 0;
    for (size_t i = 0; i < g.n(); ++i)
    {
        double in = i >= (size_t) pre ? g.x[i - (size_t) pre] : 0.0;
        for (int k = 0; k < 3; ++k) { double& d = apb[k][(size_t) aw[k]]; const double v = in + 0.6 * d; const double o = d - 0.6 * v; d = v; aw[k] = (aw[k] + 1) % alen[k]; in = o; }
        double y[4]; for (int k = 0; k < 4; ++k) y[k] = line[k][(size_t) lw[k]];
        const double h0 = y[0] + y[1] + y[2] + y[3], h1 = y[0] - y[1] + y[2] - y[3], h2 = y[0] + y[1] - y[2] - y[3], h3 = y[0] - y[1] - y[2] + y[3];
        const double fb[4] = { 0.5 * h0, 0.5 * h1, 0.5 * h2, 0.5 * h3 };
        for (int k = 0; k < 4; ++k) { line[k][(size_t) lw[k]] = lpf[k].run (fb[k] * gain[k] + in * 0.5); lw[k] = (lw[k] + 1) % len[k]; }
        double o = 0.6 * (y[0] - y[1] + y[2] - y[3] * 0.8);
        // Compresión de la sala (los micrófonos de ambiente aplastados): sube la cola.
        if (squash > 0) { const double a = std::fabs (o); env = a > env ? a : env * 0.9995; o /= std::pow (1.0 + env * 6.0, squash); }
        wet[i] = o;
    }
    if (gateMs > 0)
    {
        const double g0 = gateMs * 1e-3 * g.fs, fade = 0.015 * g.fs;
        for (size_t i = 0; i < g.n(); ++i) { const double k = (double) i - g0; if (k > 0) wet[i] *= k < fade ? 0.5 * (1 + std::cos (kPiD * k / fade)) : 0.0; }
    }
    double pk = 1e-9, pd = 1e-9; for (size_t i = 0; i < g.n(); ++i) { pk = std::max (pk, std::fabs (wet[i])); pd = std::max (pd, std::fabs (g.x[i])); }
    const double s = mix * pd / pk;
    for (size_t i = 0; i < g.n(); ++i) g.x[i] = g.x[i] * (1.0 - 0.3 * mix) + wet[i] * s;
}

inline void drive (Gen& g, double amount)
{
    double pk = 1e-9; for (double v : g.x) pk = std::max (pk, std::fabs (v));
    for (auto& v : g.x) v = std::tanh (amount * v / pk) / std::tanh (amount);
}

// Una voz analógica de Pallor tocada a la frecuencia de generación.
inline void analog (Gen& g, int slot, int model, Shape sh, float vel = 1.0f, double at = 0.0)
{
    AnalogVoice v; v.prepare ((float) g.fs, slot);
    v.trigger (model, sh, vel, 0.0f, 12345u + (uint32_t) slot);
    for (size_t i = (size_t) (at * g.fs); i < g.n() && v.isActive(); ++i) g.x[i] += v.process();
}

// Bajada a la frecuencia del kit (×2 → ×1), recorte del silencio final, fundido de salida y normalización.
inline SampleData finish (Gen& g, double kitRate, int chain, double peak, const char* name)
{
    SampleData s; s.rate = kitRate; s.chain = chain; s.name = name;
    dh::HalfbandFir hb; hb.design (95, 9.0);
    std::vector<float> y; y.reserve (g.n() / 2 + 64);
    for (size_t i = 0; i + 1 < g.n(); i += 2) { hb.push ((float) g.x[i]); const float o = hb.output(); hb.push ((float) g.x[i + 1]); y.push_back (o); }
    for (int i = 0; i < 24; ++i) { hb.push (0.0f); const float o = hb.output(); hb.push (0.0f); y.push_back (o); }
    y.erase (y.begin(), y.begin() + (std::ptrdiff_t) std::min<size_t> (y.size(), 23));          // retardo del diezmador (47 / 2)
    float pk = 1e-9f; for (float v : y) pk = std::max (pk, std::fabs (v));
    // Recorte: lo que queda por debajo de −66 dB del pico al final.
    size_t end = y.size();
    while (end > 64 && std::fabs (y[end - 1]) < pk * 5e-4f) --end;
    end = std::min (y.size(), end + (size_t) (0.005 * kitRate));
    y.resize (end);
    const size_t fade = std::min (y.size(), (size_t) (0.01 * kitRate));
    for (size_t i = 0; i < fade; ++i) y[y.size() - 1 - i] *= (float) i / (float) fade;
    const float k = (float) peak / pk;
    for (auto& v : y) v *= k;
    s.data = std::move (y);
    return s;
}

// Niveles de pico por slot: la mezcla por defecto de un kit queda equilibrada sin tocar los niveles.
inline double slotPeak (int slot)
{
    static const double p[kNumSlots] = { 0.95, 0.9, 0.85, 0.75, 0.8, 0.8, 0.75, 0.7, 0.85, 0.85, 0.85, 0.85, 0.7, 0.7, 0.75, 0.7 };
    return p[std::clamp (slot, 0, kNumSlots - 1)];
}

inline std::vector<M> besselModes (double t60, double tilt, int n, double ampDecay = 0.6)
{
    static const double r[12] = { 1.0, 1.594, 2.136, 2.296, 2.653, 2.918, 3.156, 3.501, 3.600, 3.652, 4.060, 4.154 };
    std::vector<M> v;
    for (int i = 0; i < std::min (n, 12); ++i) v.push_back ({ r[i], std::pow (ampDecay, i * 0.8) * (i == 0 ? 1.0 : tilt), t60 / (1.0 + 0.9 * i) });
    return v;
}

// Modos de metal: `n` frecuencias log-uniformes en [lo, hi] (con semilla), decaimientos que bajan con la frecuencia.
inline std::vector<M> metalModes (Rng64& r, int n, double lo, double hi, double t60, double ampSpread = 0.7)
{
    std::vector<M> v;
    for (int i = 0; i < n; ++i)
    {
        const double u = r.uni(), f = lo * std::pow (hi / lo, u);
        v.push_back ({ f, (1.0 - ampSpread + ampSpread * r.uni()) / std::sqrt ((double) n) * 2.0, t60 * (0.6 + 0.8 * r.uni()) * std::pow (lo / f, 0.35) });
    }
    return v;
}
inline std::vector<M> barModes (double f0, double t60, int n, double amp = 0.6)
{
    static const double r[6] = { 1.0, 2.756, 5.404, 8.933, 13.34, 18.64 };
    std::vector<M> v;
    for (int i = 0; i < std::min (n, 6); ++i) v.push_back ({ f0 * r[i], amp / (1.0 + 0.7 * i), t60 / (1.0 + 1.2 * i) });
    return v;
}

// ── Recetas acústicas con parámetros por kit ──
struct Acoustic { double kickF = 56, kickT = 0.45, kickDrop = 0.35, kickClick = 0.6, snareF = 188, wireT = 0.25, wireAmp = 0.9, tomScale = 1.0, tomT = 0.7,
                  roomT = 0.0, roomMix = 0.0, gateMs = 0.0, squash = 0.0, hot = 0.0, hatT = 0.07, ohT = 0.9, crashT = 2.0, rideT = 2.4, bright = 1.0; };

inline void acKick (Gen& g, const Acoustic& a)
{
    membrane (g, a.kickF, { { 1.0, 1.0, a.kickT }, { 1.594, 0.32, a.kickT * 0.4 }, { 2.136, 0.18, a.kickT * 0.27 }, { 2.296, 0.1, a.kickT * 0.22 }, { 2.653, 0.07, a.kickT * 0.18 } }, a.kickDrop, 0.018);
    click (g, 0.7, a.kickClick, 5500 * a.bright);
    noise (g, { 1, 2200, 0.7, 9000, 0.1, 0, 0.012, 0.18 * a.kickClick });
    if (a.roomMix > 0) room (g, a.roomT * 0.6, a.roomMix * 0.5, 2, 4000, a.gateMs > 0 ? a.gateMs * 0.8 : 0);
    if (a.hot > 0) drive (g, 1.0 + 2.5 * a.hot);
}
inline void acSnare (Gen& g, const Acoustic& a)
{
    membrane (g, a.snareF, { { 1.0, 0.8, 0.22 }, { 1.594, 0.55, 0.16 }, { 2.136, 0.45, 0.13 }, { 2.296, 0.35, 0.12 }, { 2.653, 0.3, 0.1 }, { 2.918, 0.22, 0.09 }, { 3.156, 0.18, 0.08 } }, 0.08, 0.015);
    noise (g, { 2, 4500 * a.bright, 0.6, 11000 * a.bright, 0.6, 0, a.wireT, a.wireAmp, 0.0005, a.wireT * 2.2, 0.25 });
    noise (g, { 1, 2500, 0.7, 0, 0.1, 0, 0.012, 0.5 });
    click (g, 0.35, 0.5, 8000);
    if (a.roomMix > 0) room (g, a.roomT, a.roomMix, 3, 6000 * a.bright, a.gateMs, a.squash);
    if (a.hot > 0) drive (g, 1.0 + 2.0 * a.hot);
}
inline void acClap (Gen& g, const Acoustic& a)
{
    const double times[5] = { 0.0, 0.0075, 0.0148, 0.0215, 0.031 };
    for (int i = 0; i < 5; ++i)
    {
        const double jt = times[i] + 0.002 * g.rng.bi() * (i > 0);
        noise (g, { 2, 1250 + 300 * g.rng.uni(), 1.3, 0, 0.25, 0, 0.035, 0.9, jt });
        noise (g, { 2, 2400 + 500 * g.rng.uni(), 1.6, 0, 0.25, 0, 0.025, 0.5, jt });
    }
    noise (g, { 2, 1500, 0.8, 5000, 2, 0, 0.18, 0.35, 0.031 });
    room (g, std::max (0.25, a.roomT), std::max (0.3, a.roomMix), 2, 5000, a.gateMs, a.squash);
    if (a.hot > 0) drive (g, 1.0 + 1.5 * a.hot);
}
inline void acRim (Gen& g, const Acoustic& a)
{
    modes (g, { { 1720, 0.5, 0.05 }, { 2930, 0.3, 0.035 }, { 4150, 0.2, 0.025 }, { 6100, 0.1, 0.015 } });
    membrane (g, a.snareF * 2.1, { { 1.0, 0.25, 0.07 }, { 1.594, 0.1, 0.05 } }, 0.0, 0.01);
    click (g, 0.2, 0.5, 9000);
    if (a.roomMix > 0) room (g, a.roomT * 0.6, a.roomMix * 0.5, 2, 5000);
}
inline void acHat (Gen& g, const Acoustic& a, bool open, uint64_t seed)
{
    Rng64 r (seed);
    const double t = open ? a.ohT : a.hatT;
    modes (g, metalModes (r, 36, 3200, 15500, t, 0.8));
    noise (g, { 1, 7000 * a.bright, 0.7, 16000, 0.3, 0, t * 1.1, 0.75 });
    noise (g, { 2, 1700, 1.2, 0, 0.1, 0, 0.012, 0.18 });
    click (g, 0.15, 0.25, 12000);
}
inline void acCrash (Gen& g, const Acoustic& a, uint64_t seed)
{
    Rng64 r (seed);
    modes (g, metalModes (r, 44, 380, 9500, a.crashT * 0.9, 0.8), 0.0, 2.0);
    const double bands[5] = { 2500, 4000, 6000, 9000, 12500 };
    for (int i = 0; i < 5; ++i) noise (g, { 2, bands[i] * a.bright, 0.9, 0, 2.0 + 6.0 * i, 0, a.crashT * (0.75 + 0.1 * i), 0.4 });
    noise (g, { 1, 3000, 0.7, 0, 0.1, 0, 0.012, 0.6 });
}
inline void acRide (Gen& g, const Acoustic& a, uint64_t seed)
{
    Rng64 r (seed);
    modes (g, metalModes (r, 40, 320, 8000, a.rideT, 0.8));
    modes (g, { { 752, 0.35, a.rideT * 0.8 }, { 1734, 0.28, a.rideT * 0.6 }, { 2533, 0.22, a.rideT * 0.5 }, { 3611, 0.16, a.rideT * 0.4 } });
    noise (g, { 1, 5200 * a.bright, 0.7, 14000, 0.3, 0, a.rideT * 0.6, 0.18 });
    click (g, 0.15, 0.35, 11000);
}
inline void acTom (Gen& g, const Acoustic& a, int slot)
{
    const double f = tomBase (slot) * 1.08 * a.tomScale;
    membrane (g, f, { { 1.0, 1.0, a.tomT }, { 1.594, 0.42, a.tomT * 0.4 }, { 2.136, 0.26, a.tomT * 0.3 }, { 2.296, 0.18, a.tomT * 0.25 }, { 2.653, 0.12, a.tomT * 0.18 } }, 0.13, 0.05);
    click (g, 0.5, 0.35, 6000);
    noise (g, { 0, 2000, 0.7, 0, 0.3, 0, 0.05, 0.12 });
    if (a.roomMix > 0) room (g, a.roomT, a.roomMix * 0.9, 4, 5000, a.gateMs > 0 ? a.gateMs * 1.25 : 0, a.squash);
    if (a.hot > 0) drive (g, 1.0 + 1.8 * a.hot);
}
inline void acCowbell (Gen& g, const Acoustic& a)
{
    modes (g, { { 587, 1.0, 0.36 }, { 845, 0.8, 0.3 }, { 1352, 0.35, 0.2 }, { 1896, 0.25, 0.14 }, { 2580, 0.15, 0.1 } });
    click (g, 0.25, 0.4, 7000);
    if (a.roomMix > 0) room (g, a.roomT * 0.6, a.roomMix * 0.4, 3, 6000);
}
inline void acClave (Gen& g, const Acoustic&) { modes (g, { { 2510, 1.0, 0.09 }, { 6280, 0.28, 0.035 } }); click (g, 0.12, 0.2, 9000); }
inline void acTamb (Gen& g, const Acoustic& a, uint64_t seed)
{
    Rng64 r (seed);
    const double at[4] = { 0.0, 0.012, 0.026, 0.041 };
    for (int s = 0; s < 4; ++s)
    {
        std::vector<M> jm;
        for (int i = 0; i < 10; ++i) jm.push_back ({ 5000 + 8500 * r.uni(), (s == 0 ? 0.32 : 0.2) * (0.5 + r.uni()), 0.12 + 0.2 * r.uni() });
        modes (g, jm, at[s] + 0.003 * r.bi() * (s > 0));
    }
    noise (g, { 1, 8000 * a.bright, 0.7, 0, 3, 0, 0.12, 0.3 });
}

// Sonoridad de referencia de un slot: el RMS de los primeros 120 ms de su voz analógica (modelo 0, controles al medio).
// Las muestras de los kits se llevan a ese RMS (sin pasar el fondo de escala): cambiar de motor no cambia el nivel y
// una caja acústica (muy picuda) no queda 8 dB debajo de su bombo.
inline double refRms (int slot)
{
    static const std::array<double, kNumSlots> r = [] {
        std::array<double, kNumSlots> a {};
        for (int s = 0; s < kNumSlots; ++s)
        {
            AnalogVoice v; v.prepare (48000.0f, s); v.trigger (0, Shape(), 1.0f, 0.0f, 1u);
            double e = 0; const int n = (int) (0.12 * 48000);
            for (int i = 0; i < n; ++i) { const double x = v.process(); e += x * x; }
            a[(size_t) s] = std::sqrt (e / n);
        }
        return a;
    }();
    return r[(size_t) std::clamp (slot, 0, kNumSlots - 1)];
}
inline void matchLoudness (SampleData& s, int slot)
{
    const size_t n = std::min (s.data.size(), (size_t) (0.12 * s.rate));
    double e = 0, pk = 1e-9;
    for (size_t i = 0; i < n; ++i) e += (double) s.data[i] * s.data[i];
    for (float x : s.data) pk = std::max (pk, (double) std::fabs (x));
    const double rms = std::sqrt (e / std::max<size_t> (1, n));
    if (rms < 1e-9) return;
    const float g = (float) std::min (refRms (slot) / rms, 0.98 / pk);
    for (auto& x : s.data) x *= g;
}

// ── Un kit: los 16 sonidos ──
inline SampleData makeSound (int kit, int slot)
{
    const auto& K = kits()[(size_t) kit];
    const double gfs = K.rate * 2.0;
    const uint64_t seed = (uint64_t) (kit * 100 + slot + 1);
    const double len[kNumSlots] = { 0.9, 0.8, 0.8, 0.3, 0.25, 1.4, 2.6, 2.8, 1.2, 1.1, 1.0, 0.9, 0.6, 0.25, 0.5, 1.0 };
    double secs = len[slot];
    Acoustic A;
    std::function<void (Gen&)> fn;
    auto ac = [&] (Gen& g) {
        switch (slot)
        {
            case kKick: acKick (g, A); break;
            case kSnare: acSnare (g, A); break;
            case kClap: acClap (g, A); break;
            case kRim: acRim (g, A); break;
            case kClosedHat: acHat (g, A, false, seed); break;
            case kOpenHat: acHat (g, A, true, seed); break;
            case kCrash: acCrash (g, A, seed); break;
            case kRide: acRide (g, A, seed); break;
            case kCowbell: acCowbell (g, A); break;
            case kClave: acClave (g, A); break;
            case kTamb: acTamb (g, A, seed); break;
            default: if (slot >= kTom1 && slot <= kTom4) acTom (g, A, slot); break;
        }
    };
    const char* nm = "";
    switch (kit)
    {
        case 0:   // Studio 82: seco y apretado
            A = {}; nm = "Studio 82";
            if (slot == kFx) { secs = 0.4; fn = [] (Gen& g) { noise (g, { 1, 6000, 0.7, 13000, 9, 0, 0.12, 0.8 }); noise (g, { 2, 9500, 3, 0, 4, 0, 0.08, 0.3, 0.015 }); }; }
            else fn = ac;
            break;
        case 1:   // Pop 84: salas con compuerta, golpes comprimidos
            A.kickF = 52; A.kickT = 0.35; A.kickClick = 0.9; A.roomT = 1.6; A.roomMix = 0.6; A.gateMs = 240; A.squash = 0.6; A.hot = 0.4; A.wireT = 0.2; A.tomT = 0.6; A.crashT = 1.8;
            nm = "Pop 84";
            if (slot == kFx) { secs = 0.5; fn = [] (Gen& g) { noise (g, { 0, 7000, 0.7, 0, 0.3, 160, 0.03, 0.9 }); room (g, 1.2, 0.5, 2, 5000, 200, 0.6); }; }
            else if (slot == kClosedHat || slot == kOpenHat || slot == kCrash || slot == kRide || slot == kTamb || slot == kClave || slot == kCowbell || slot == kRim) { A.roomMix = 0; fn = ac; }
            else fn = ac;
            break;
        case 2:   // Electro 81: las voces analógicas (modelo 0 de cada familia) muestreadas
        {
            nm = "Electro 81";
            Shape sh;
            if (slot == kKick) { sh.decay = 0.55f; sh.a = 0.35f; sh.b = 0.25f; }
            if (slot == kSnare) { sh.a = 0.55f; sh.b = 0.35f; }
            if (slot == kOpenHat) sh.decay = 0.45f;
            if (slot == kCrash) sh.decay = 0.55f;
            if (slot >= kTom1 && slot <= kTom4) { sh.b = 0.25f; sh.decay = 0.45f; }
            fn = [slot, sh] (Gen& g) { analog (g, slot, 0, sh); };
            break;
        }
        case 3:   // Hexa 84: los modelos hexagonales
        {
            nm = "Hexa 84";
            Shape sh; int model = 0, s = slot;
            switch (slot)
            {
                case kKick: model = 3; sh.b = 0.55f; sh.a = 0.6f; sh.decay = 0.45f; break;
                case kSnare: model = 2; sh.a = 0.62f; sh.b = 0.45f; sh.decay = 0.4f; sh.tone = 0.6f; break;
                case kClap: model = 2; break;
                case kRim: model = 2; sh.b = 0.6f; break;
                case kClosedHat: case kOpenHat: model = 1; sh.a = 0.4f; sh.b = 0.3f; break;
                case kCrash: model = 1; sh.a = 0.4f; break;
                case kRide: model = 2; break;
                case kCowbell: model = 1; break;
                case kClave: model = 2; break;
                case kTamb: model = 2; break;
                case kFx: model = 1; s = kTom2; sh.b = 1.0f; sh.decay = 0.7f; sh.a = 0.25f; break;
                default: model = 1; sh.b = 0.62f; sh.decay = 0.55f; sh.a = 0.3f; break;
            }
            fn = [s, model, sh] (Gen& g) { analog (g, s, model, sh); };
            break;
        }
        case 4:   // Crunch 88: grande y caliente
            A.kickF = 49; A.kickT = 0.75; A.kickDrop = 0.5; A.snareF = 176; A.wireT = 0.3; A.roomT = 0.45; A.roomMix = 0.28; A.hot = 0.8; A.tomScale = 0.92; A.bright = 0.85;
            nm = "Crunch 88";
            if (slot == kFx) { secs = 0.7; fn = [] (Gen& g) { Shape sh; sh.b = 0.9f; sh.decay = 0.3f; sh.a = 0.6f; sh.tone = 0.55f; analog (g, kFx, 1, sh); drive (g, 2.0); }; }
            else fn = ac;
            break;
        case 5:   // Studio 91: más sala, más aire
            A.kickF = 55; A.kickT = 0.55; A.roomT = 0.9; A.roomMix = 0.32; A.wireT = 0.27; A.tomT = 0.85; A.bright = 1.1; A.crashT = 2.4; A.ohT = 1.1;
            nm = "Studio 91";
            if (slot == kFx) { secs = 1.6; fn = [seed] (Gen& g) { Acoustic a; a.crashT = 2.2; acCrash (g, a, seed); std::reverse (g.x.begin(), g.x.end()); }; }
            else fn = ac;
            break;
        case 6:   // Foundry: industrial
            nm = "Foundry";
            fn = [slot, seed] (Gen& g) {
                Rng64 r (seed);
                switch (slot)
                {
                    case kKick: membrane (g, 47, { { 1.0, 1.0, 0.5 }, { 1.594, 0.3, 0.2 } }, 0.6, 0.02); modes (g, metalModes (r, 20, 150, 2500, 0.15, 0.8)); click (g, 0.6, 0.8, 4000); drive (g, 4.0); room (g, 0.4, 0.2, 2, 3000); break;
                    case kSnare: modes (g, metalModes (r, 50, 300, 7000, 0.45, 0.9)); noise (g, { 2, 3000, 0.7, 0, 0.3, 0, 0.2, 0.7 }); click (g, 0.4, 0.7, 6000); drive (g, 2.0); break;
                    case kClap: for (int i = 0; i < 6; ++i) { const double at = 0.06 * r.uni(); modes (g, metalModes (r, 6, 2000, 9000, 0.05, 0.5), at); noise (g, { 2, 3500, 1.5, 0, 0.1, 0, 0.02, 0.3, at }); } room (g, 0.5, 0.3, 2, 5000); break;
                    case kRim: modes (g, barModes (1900, 0.3, 4)); click (g, 0.15, 0.4, 10000); break;
                    case kClosedHat: modes (g, metalModes (r, 24, 2500, 9500, 0.05, 0.8)); noise (g, { 1, 6000, 0.7, 0, 0.1, 0, 0.05, 0.4 }); break;
                    case kOpenHat: modes (g, metalModes (r, 24, 2500, 9500, 0.55, 0.8)); noise (g, { 1, 6000, 0.7, 0, 0.1, 0, 0.5, 0.35 }); break;
                    case kCrash: modes (g, metalModes (r, 80, 200, 10000, 2.0, 0.9)); noise (g, { 1, 4000, 0.7, 0, 5, 0, 1.6, 0.4 }); click (g, 0.5, 0.5, 6000); break;
                    case kRide: modes (g, barModes (410, 2.6, 5, 0.8)); noise (g, { 1, 5000, 0.7, 0, 0.3, 0, 0.6, 0.1 }); break;
                    case kCowbell: modes (g, barModes (980, 1.2, 3, 0.9)); click (g, 0.2, 0.5, 9000); break;
                    case kClave: modes (g, barModes (2200, 0.2, 3)); break;
                    case kTamb: for (int i = 0; i < 14; ++i) modes (g, metalModes (r, 3, 3000, 11000, 0.04, 0.5), 0.12 * r.uni()); break;
                    case kFx: { Gen h (g.n() / g.fs, g.fs, seed); noise (h, { 2, 3000, 2.0, 0, 20, 0, 0.9, 1.0 }); Bq sweep; double f = 6000; for (size_t i = 0; i < g.n(); ++i) { if (i % 64 == 0) { f = 800 + 5200 * std::exp (-(double) i / (0.25 * g.fs)); sweep = Bq::bp (f, 3.0, g.fs); } g.x[i] += sweep.run (r.bi()) * std::exp (-(double) i / (0.3 * g.fs)) + 0.3 * h.x[i]; } break; }
                    default:
                    {
                        const double f = tomBase (slot) * 0.72;
                        membrane (g, f, { { 1.0, 1.0, 0.6 }, { 1.594, 0.35, 0.25 } }, 0.2, 0.04);
                        modes (g, { { f * 3.7, 0.3, 0.3 }, { f * 5.9, 0.2, 0.2 }, { f * 8.1, 0.12, 0.12 } });
                        click (g, 0.5, 0.4, 5000); drive (g, 1.6);
                        break;
                    }
                }
            };
            break;
        default:  // FM 85
            nm = "FM 85";
            fn = [slot] (Gen& g) {
                switch (slot)
                {
                    case kKick: fm (g, { 62, 1.0, 3.0, 0.02, 0.45, 2.2, 0.014 }); break;
                    case kSnare: fm (g, { 205, 1.47, 6.0, 0.05, 0.22 }); fm (g, { 1800, 7.31, 12.0, 0.3, 0.25, 0, 0.02, 0.5 }); break;
                    case kClap: for (int i = 0; i < 4; ++i) { Gen h (g.n() / g.fs, g.fs, 9u + (uint64_t) i); fm (h, { 1200, 3.31, 14.0, 1.0, 0.03 + (i == 3 ? 0.2 : 0.0), 0, 0.02, 0.8 }); const size_t off = (size_t) (i * 0.009 * g.fs); for (size_t k = off; k < g.n(); ++k) g.x[k] += h.x[k - off]; } break;
                    case kRim: fm (g, { 1400, 2.4, 2.0, 0.01, 0.045 }); break;
                    case kClosedHat: fm (g, { 6300, 1.414, 4.0, 0.2, 0.06, 0, 0.02, 0.8, 0.6 }); break;
                    case kOpenHat: fm (g, { 6300, 1.414, 4.0, 1.0, 0.7, 0, 0.02, 0.8, 0.6 }); break;
                    case kCrash: fm (g, { 3100, 1.73, 8.0, 3.0, 2.0, 0, 0.02, 0.6, 0.7 }); fm (g, { 5200, 1.41, 6.0, 2.0, 1.5, 0, 0.02, 0.4, 0.8 }); break;
                    case kRide: fm (g, { 1900, 2.41, 3.0, 0.8, 2.2 }); break;
                    case kCowbell: fm (g, { 800, 1.48, 2.5, 0.12, 0.4 }); break;
                    case kClave: fm (g, { 2400, 3.0, 1.0, 0.02, 0.06 }); break;
                    case kTamb: for (int i = 0; i < 4; ++i) { Gen h (g.n() / g.fs, g.fs, 30u + (uint64_t) i); fm (h, { 7500, 1.21, 6.0, 0.1, 0.18, 0, 0.02, 0.4, 0.5 }); const size_t off = (size_t) (i * 0.011 * g.fs); for (size_t k = off; k < g.n(); ++k) g.x[k] += h.x[k - off]; } break;
                    case kFx:   // láser: portadora que cae de 3 kHz a 100 Hz con una moduladora a media octava
                    {
                        double pc = 0, pm = 0;
                        for (size_t i = 0; i < g.n(); ++i)
                        {
                            const double k = (double) i / g.fs, f = 100 + 2900 * std::exp (-k / 0.08);
                            g.x[i] += std::sin (pc + 2.0 * std::exp (-k / 0.3) * std::sin (pm)) * std::exp (-k / 0.25);
                            pc += 2 * kPiD * f / g.fs; pm += 2 * kPiD * f * 0.5 / g.fs;
                        }
                        break;
                    }
                    default: { const double f = tomBase (slot) * 1.1; fm (g, { f, 1.0, 1.5, 0.05, 0.6, 0.5, 0.04 }); break; }
                }
            };
            break;
    }
    Gen g (secs, gfs, seed * 7919u);
    fn (g);
    SampleData sd = finish (g, K.rate, K.chain, slotPeak (slot), nm);
    matchLoudness (sd, slot);
    return sd;
}

// Todas las muestras de fábrica, generadas una vez por proceso (compartidas por todas las instancias).
class Library
{
public:
    static const Library& get() { static const Library lib; return lib; }
    const SampleData* sample (int kit, int slot) const
    {
        kit = std::clamp (kit, 0, numKits() - 1); slot = std::clamp (slot, 0, kNumSlots - 1);
        return &smp[(size_t) (kit * kNumSlots + slot)];
    }
    size_t bytes() const { size_t b = 0; for (const auto& s : smp) b += s.data.size() * sizeof (float); return b; }
private:
    Library()
    {
        smp.resize ((size_t) (numKits() * kNumSlots));
        for (int k = 0; k < numKits(); ++k)
            for (int s = 0; s < kNumSlots; ++s) smp[(size_t) (k * kNumSlots + s)] = makeSound (k, s);
    }
    std::vector<SampleData> smp;
};

} // namespace dh::pal::kitgen
