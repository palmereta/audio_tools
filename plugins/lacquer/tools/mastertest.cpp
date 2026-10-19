// Pruebas de Lacquer sin JUCE (docs/research.md §6.10). Se agregan a medida que entra cada etapa.
//   clang++ -std=c++20 -O2 -ffp-contract=off -I plugins/lacquer/Source/dsp plugins/lacquer/tools/mastertest.cpp -o /tmp/lq_test && /tmp/lq_test
//   (con --quick se saltean las pruebas largas del motor completo)
#include "Dynamics.h"
#include "EngineMaster.h"
#include <cstdio>
#include <functional>
#include <string>
#include <cstring>

using namespace dh::lq;

static int fails = 0;
static void check (bool ok, const std::string& what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what.c_str()); if (! ok) ++fails; }
static bool near (double a, double b, double tol) { return std::fabs (a - b) <= tol; }

// Señal estéreo como lista de tramos (segundos, dBFS de cada canal; frecuencia).
struct Seg { double secs, dbL, dbR, hz = 1000.0; };
static void synth (const std::vector<Seg>& segs, double fs, std::vector<float>& L, std::vector<float>& R)
{
    L.clear(); R.clear();
    double ph = 0.0;
    for (auto& s : segs)
    {
        const int n = (int) std::lround (s.secs * fs);
        const double aL = std::pow (10.0, s.dbL / 20.0), aR = std::pow (10.0, s.dbR / 20.0);
        for (int i = 0; i < n; ++i)
        {
            const double v = std::sin (ph); ph += 2.0 * kPiD * s.hz / fs; if (ph > 2.0 * kPiD) ph -= 2.0 * kPiD;
            L.push_back ((float) (aL * v)); R.push_back ((float) (aR * v));
        }
    }
}
static LoudnessMeter measure (const std::vector<float>& L, const std::vector<float>& R, double fs, int block = 977)
{
    LoudnessMeter m; m.prepare (fs);
    for (size_t p = 0; p < L.size(); p += (size_t) block)
    {
        const int n = (int) std::min ((size_t) block, L.size() - p);
        m.process (L.data() + p, R.data() + p, n);
    }
    return m;
}


// Medidor de referencia ×8 independiente: sinc con ventana de Kaiser de ±32 muestras (para verificar el limitador
// con un interpolador distinto del suyo).
static double refTruePeakDb (const std::vector<float>& x)
{
    auto bessel0 = [] (double v) { double s = 1, t = 1; for (int k = 1; k < 30; ++k) { t *= (v / (2 * k)) * (v / (2 * k)); s += t; } return s; };
    const int H = 32; const double beta = 9.0, i0b = bessel0 (beta);
    double pk = 0;
    for (size_t n = (size_t) H; n + (size_t) H < x.size(); ++n)
        for (int ph = 0; ph < 8; ++ph)
        {
            const double frac = ph / 8.0; double acc = 0;
            for (int m = -H + 1; m <= H; ++m)
            {
                const double t = frac - m, w = std::fabs (t) >= H ? 0.0 : bessel0 (beta * std::sqrt (1.0 - (t / H) * (t / H))) / i0b;
                const double sinc = std::fabs (t) < 1e-12 ? 1.0 : std::sin (kPiD * t) / (kPiD * t);
                acc += x[(size_t) ((long) n + m)] * sinc * w;
            }
            pk = std::max (pk, std::fabs (acc));
        }
    return toDb (pk);
}
static std::vector<float> noise (int n, uint32_t seed, double gain)
{
    std::vector<float> v ((size_t) n); uint32_t s = seed;
    double b0 = 0, b1 = 0, b2 = 0;
    for (auto& x : v)
    {
        s ^= s << 13; s ^= s >> 17; s ^= s << 5;
        const double w = (double) (int32_t) s / 2147483648.0;
        b0 = 0.99765 * b0 + w * 0.0990460; b1 = 0.96300 * b1 + w * 0.2965164; b2 = 0.57000 * b2 + w * 1.0526913;
        x = (float) (gain * (b0 + b1 + b2 + w * 0.1848) * 0.25);
    }
    return v;
}
// Batería sintética: bombo (seno que cae de 120 a 45 Hz), redoblante (ruido + 180 Hz) y hi-hat (ruido agudo).
static std::vector<float> drums (double fs, double secs, double bpm, uint32_t seed)
{
    std::vector<float> v ((size_t) (fs * secs), 0.0f); uint32_t s = seed;
    auto rnd = [&] { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (double) (int32_t) s / 2147483648.0; };
    const double beat = 60.0 / bpm;
    for (double t = 0; t < secs; t += beat / 2)
    {
        const int i0 = (int) (t * fs); const int step = (int) std::lround (t / (beat / 2));
        const bool kick = step % 2 == 0, snare = step % 4 == 2;
        double ph = 0;
        for (int i = 0; i < (int) (0.35 * fs) && i0 + i < (int) v.size(); ++i)
        {
            const double tt = i / fs; double y = 0;
            if (kick) { const double f = 45 + 75 * std::exp (-tt * 30); ph += 2 * kPiD * f / fs; y += 0.9 * std::sin (ph) * std::exp (-tt * 9); }
            if (snare) y += (0.5 * rnd() + 0.3 * std::sin (2 * kPiD * 180 * tt)) * std::exp (-tt * 22);
            y += 0.12 * rnd() * std::exp (-tt * 60);            // hi-hat en cada corchea
            v[(size_t) (i0 + i)] += (float) y;
        }
    }
    return v;
}

// Canción sintética estéreo: batería, bajo, pad de acordes (sierras desafinadas) y aire. Variantes para las
// pruebas: oscura, brillante, ancha (graves fuera de fase), con intro suave. Normalizada a −20 LUFS.
struct SongOpts { double secs = 10.0; uint32_t seed = 1; bool dark = false, bright = false, wideBass = false, intro = false; double lufs = -20.0; };
static void song (double fs, const SongOpts& o, std::vector<float>& L, std::vector<float>& R)
{
    const size_t n = (size_t) (o.secs * fs);
    L.assign (n, 0.0f); R.assign (n, 0.0f);
    const auto dr = drums (fs, o.secs, 120.0, o.seed);
    const double bassHz[4] = { 55.0, 55.0, 65.41, 49.0 };
    const double chord[4][3] = { { 220.0, 261.63, 329.63 }, { 220.0, 261.63, 329.63 }, { 261.63, 329.63, 392.0 }, { 196.0, 246.94, 293.66 } };
    double phB = 0, lpB = 0, ph[3][2] {}, lp1[2] {}, lp2[2] {}, hpA[2] {}, xa[2] {};
    uint32_t s = o.seed * 7919u + 13u;
    auto rnd = [&] { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (double) (int32_t) s / 2147483648.0; };
    const double padFc = o.dark ? 900.0 : o.bright ? 7000.0 : 2500.0;
    const double kPad = 1.0 - std::exp (-2.0 * kPiD * padFc / fs), kB = 1.0 - std::exp (-2.0 * kPiD * 220.0 / fs);
    const double kA = std::exp (-2.0 * kPiD * 6000.0 / fs);
    for (size_t i = 0; i < n; ++i)
    {
        const double t = (double) i / fs, beat = t * 2.0;            // 120 bpm
        const int bar = ((int) (beat / 4.0)) % 4;
        const bool introPart = o.intro && t < o.secs * 0.3;
        // Bajo: sierra filtrada + seno, envolvente por negra.
        const double f = bassHz[bar], env = std::exp (-(beat - std::floor (beat)) * 1.5);
        phB += f / fs; if (phB >= 1.0) phB -= 1.0;
        const double saw = 2.0 * phB - 1.0;
        lpB += kB * (saw - lpB);
        const double bass = introPart ? 0.0 : (0.35 * lpB + 0.45 * std::sin (2.0 * kPiD * phB)) * (0.4 + 0.6 * env);
        // Pad: tres notas, sierras desafinadas distinto en cada canal.
        double pad[2] = { 0, 0 };
        for (int k = 0; k < 3; ++k)
            for (int c = 0; c < 2; ++c)
            {
                ph[k][c] += chord[bar][k] * (c ? 1.004 : 0.996) / fs; if (ph[k][c] >= 1.0) ph[k][c] -= 1.0;
                pad[c] += (2.0 * ph[k][c] - 1.0) / 3.0;
            }
        for (int c = 0; c < 2; ++c) { lp1[c] += kPad * (pad[c] - lp1[c]); lp2[c] += kPad * (lp1[c] - lp2[c]); }
        // Aire: ruido pasa altos suave.
        double air[2];
        for (int c = 0; c < 2; ++c) { const double w = rnd(); hpA[c] = kA * (hpA[c] + w - xa[c]); xa[c] = w; air[c] = hpA[c] * (o.bright ? 0.06 : 0.02); }
        const double d = introPart ? 0.0 : dr[i] * 0.55;
        const double padG = introPart ? 0.12 : 0.25;
        const double bl = bass, br = o.wideBass ? -0.6 * bass : bass;
        L[i] = (float) (d + bl + padG * lp2[0] + air[0]);
        R[i] = (float) (d + br + padG * lp2[1] + air[1]);
    }
    LoudnessMeter m; m.prepare (fs); m.process (L.data(), R.data(), (int) n);
    const float g = (float) std::pow (10.0, (o.lufs - m.integrated()) / 20.0);
    for (size_t i = 0; i < n; ++i) { L[i] *= g; R[i] *= g; }
}

// Render completo como lacquerrender: aprendizaje + cadena por bloques (tamaño fijo o aleatorio si block < 0).
struct Rendered { std::vector<float> L, R; Plan plan; Report in; };
static Rendered renderSong (const std::vector<float>& L, const std::vector<float>& R, double fs, const Options& o, int block = 512)
{
    Rendered r;
    const size_t n = L.size();
    r.plan = Learner::learn (L.data(), R.data(), n, fs, o, &r.in);
    Chain c; c.prepare (fs, r.plan);
    const int lat = c.latency();
    r.L.assign (n, 0.0f); r.R.assign (n, 0.0f);
    std::vector<float> bl (8192), br (8192);
    uint32_t s = 12345u;
    for (size_t pos = 0; pos < n + (size_t) lat;)
    {
        size_t k = block > 0 ? (size_t) block : (size_t) (1 + ((s ^= s << 13, s ^= s >> 17, s ^= s << 5) % 3000u));
        k = std::min (k, n + (size_t) lat - pos);
        for (size_t i = 0; i < k; ++i) { const size_t q = pos + i; bl[i] = q < n ? L[q] : 0.0f; br[i] = q < n ? R[q] : 0.0f; }
        c.process (bl.data(), br.data(), (int) k);
        for (size_t i = 0; i < k; ++i) { const long long q = (long long) (pos + i) - lat; if (q >= 0 && (size_t) q < n) { r.L[(size_t) q] = bl[i]; r.R[(size_t) q] = br[i]; } }
        pos += k;
    }
    return r;
}
static double correlation (const std::vector<float>& L, const std::vector<float>& R, double fs, double lpHz = 0.0)
{
    Biquad a, b; if (lpHz > 0) { a.set (lowPass (lpHz, 0.7071, fs)); b.set (lowPass (lpHz, 0.7071, fs)); }
    double lr = 0, ll = 0, rr = 0;
    for (size_t i = 0; i < L.size(); ++i)
    {
        double l = L[i], r = R[i];
        if (lpHz > 0) { l = a.process (l); r = b.process (r); }
        lr += l * r; ll += l * l; rr += r * r;
    }
    return ll > 0 && rr > 0 ? lr / std::sqrt (ll * rr) : 1.0;
}
// Remuestreo de calidad (sinc con ventana de Kaiser, ±48 cruces, corte al 90 % del Nyquist de salida).
static std::vector<float> resample (const std::vector<float>& x, double fsIn, double fsOut)
{
    const double ratio = fsIn / fsOut, fc = 0.9 * std::min (1.0, 1.0 / ratio);
    const int H = 48;
    auto bessel0 = [] (double v) { double s = 1, t = 1; for (int k = 1; k < 40; ++k) { t *= (v / (2 * k)) * (v / (2 * k)); s += t; } return s; };
    const double beta = 10.0, i0b = bessel0 (beta);
    const size_t n = (size_t) std::floor ((double) x.size() / ratio);
    std::vector<float> y (n);
    for (size_t j = 0; j < n; ++j)
    {
        const double t = (double) j * ratio; const long c = (long) std::floor (t);
        double acc = 0.0;
        for (long k = c - (long) (H / fc); k <= c + (long) (H / fc); ++k)
        {
            if (k < 0 || k >= (long) x.size()) continue;
            const double d = (t - (double) k) * fc;
            if (std::fabs (d) >= H) continue;
            const double w = bessel0 (beta * std::sqrt (1.0 - (d / H) * (d / H))) / i0b;
            acc += x[(size_t) k] * fc * (std::fabs (d) < 1e-12 ? 1.0 : std::sin (kPiD * d) / (kPiD * d)) * w;
        }
        y[j] = (float) acc;
    }
    return y;
}
static bool finiteAll (const std::vector<float>& v) { for (float x : v) if (! std::isfinite (x)) return false; return true; }
static double maxAbs (const std::vector<float>& v) { double m = 0; for (float x : v) m = std::max (m, (double) std::fabs (x)); return m; }

int main (int argc, char** argv)
{
    std::printf ("T1a. Ponderación K: coeficientes a 48 kHz = tabla de BS.1770-5\n");
    {
        BiquadD s, h; kWeighting (48000.0, s, h);
        const double tab[] = { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585, -1.99004745483398, 0.99007225036621 };
        const double got[] = { s.b0, s.b1, s.b2, s.a1, s.a2, h.a1, h.a2 };
        double worst = 0; for (int i = 0; i < 7; ++i) worst = std::max (worst, std::fabs (tab[i] - got[i]));
        std::printf ("  máxima diferencia %.2e\n", worst);
        check (worst < 1e-12, "coinciden con la tabla hasta 1e-12");
    }

    std::printf ("T1b. EBU Tech 3341, señales mínimas (I, M, S ±0.1 LU)\n");
    for (double fs : { 48000.0, 44100.0, 96000.0 })
    {
        std::vector<float> L, R;
        struct Case { const char* name; std::vector<Seg> segs; double expectI; };
        const std::vector<Case> cases {
            { "#1 1 kHz −23 dBFS 20 s", { { 20, -23, -23 } }, -23.0 },
            { "#2 1 kHz −33 dBFS 20 s", { { 20, -33, -33 } }, -33.0 },
            { "#3 −36 / −23 / −36", { { 10, -36, -36 }, { 60, -23, -23 }, { 10, -36, -36 } }, -23.0 },
            { "#4 −72 / −36 / −23 / −36 / −72", { { 10, -72, -72 }, { 10, -36, -36 }, { 60, -23, -23 }, { 10, -36, -36 }, { 10, -72, -72 } }, -23.0 },
            { "#5 −26 / −20 / −26", { { 20, -26, -26 }, { 20.1, -20, -20 }, { 20, -26, -26 } }, -23.0 },
        };
        for (auto& c : cases)
        {
            synth (c.segs, fs, L, R);
            auto m = measure (L, R, fs);
            const double I = m.integrated();
            check (near (I, c.expectI, 0.1), std::string (c.name) + " a " + std::to_string ((int) fs) + " Hz: I = " + std::to_string (I).substr (0, 7));
        }
        // Momentánea y corta con un tono constante: −23.
        synth ({ { 20, -23, -23 } }, fs, L, R);
        auto m = measure (L, R, fs);
        check (near (m.momentary(), -23.0, 0.1) && near (m.shortTerm(), -23.0, 0.1), "M y S de #1 = −23.0");
    }

    std::printf ("T1c. Tech 3341 #9 y #12: corta y momentánea constantes con tonos que alternan\n");
    {
        const double fs = 48000.0;
        std::vector<Seg> s9, s12;
        for (int k = 0; k < 5; ++k) { s9.push_back ({ 1.34, -20, -20 }); s9.push_back ({ 1.66, -30, -30 }); }
        for (int k = 0; k < 25; ++k) { s12.push_back ({ 0.18, -20, -20 }); s12.push_back ({ 0.22, -30, -30 }); }
        std::vector<float> L, R;
        synth (s9, fs, L, R);
        { auto m = measure (L, R, fs); const auto& h = m.shortTermHistory(); double lo = 1e9, hi = -1e9; for (double v : h) { lo = std::min (lo, v); hi = std::max (hi, v); }
          std::printf ("  #9: corta entre %.2f y %.2f\n", lo, hi); check (near (lo, -23.0, 0.1) && near (hi, -23.0, 0.1), "#9 S = −23.0 desde los 3 s"); }
        synth (s12, fs, L, R);
        { LoudnessMeter m; m.prepare (fs); double lo = 1e9, hi = -1e9;
          for (size_t p = 0; p < L.size(); p += 480) { m.process (L.data() + p, R.data() + p, 480); if (p >= (size_t) fs) { lo = std::min (lo, m.momentary()); hi = std::max (hi, m.momentary()); } }
          std::printf ("  #12: momentánea entre %.2f y %.2f\n", lo, hi); check (near (lo, -23.0, 0.1) && near (hi, -23.0, 0.1), "#12 M = −23.0 desde 1 s"); }
    }

    std::printf ("T1d. Pico verdadero (Tech 3341 #15-19, tolerancia +0.2 / −0.4 dB)\n");
    {
        const double fs = 48000.0;
        struct TP { const char* name; double frac, ampl, phaseDeg, expect; };
        for (auto c : { TP { "#15 fs/4 0°", 0.25, 0.5, 0, -6.0 }, TP { "#16 fs/4 45°", 0.25, 0.5, 45, -6.0 }, TP { "#17 fs/6 60°", 1.0 / 6.0, 0.5, 60, -6.0 },
                        TP { "#18 fs/8 67.5°", 0.125, 0.5, 67.5, -6.0 }, TP { "#19 fs/4 1.41 45°", 0.25, 1.41, 45, 3.0 } })
        {
            std::vector<float> L, R;
            const int n = (int) fs;
            // Entrada con 10 ms de fundido: un arranque abrupto suena (y mide) como un escalón que rebota.
            for (int i = 0; i < n; ++i)
            {
                const double fade = i < 480 ? 0.5 - 0.5 * std::cos (kPiD * i / 480.0) : 1.0;
                const float v = (float) (fade * c.ampl * std::sin (2.0 * kPiD * c.frac * i + c.phaseDeg * kPiD / 180.0)); L.push_back (v); R.push_back (v);
            }
            auto m = measure (L, R, fs);
            const double tp = m.truePeakDb();
            std::printf ("  %-18s %.2f dBTP (muestra %.2f)\n", c.name, tp, m.samplePeakDb());
            check (tp <= c.expect + 0.2 && tp >= c.expect - 0.4, std::string (c.name));
        }
    }

    std::printf ("T1e. LRA (EBU Tech 3342, ±1 LU)\n");
    {
        const double fs = 48000.0;
        struct C { const char* name; std::vector<Seg> segs; double expect; };
        for (auto& c : std::vector<C> { { "−20 / −30", { { 20, -20, -20 }, { 20, -30, -30 } }, 10 },
                                        { "−20 / −15", { { 20, -20, -20 }, { 20, -15, -15 } }, 5 },
                                        { "−40 / −20", { { 20, -40, -40 }, { 20, -20, -20 } }, 20 },
                                        { "−50 / −35 / −20 / −35 / −50", { { 20, -50, -50 }, { 20, -35, -35 }, { 20, -20, -20 }, { 20, -35, -35 }, { 20, -50, -50 } }, 15 } })
        {
            std::vector<float> L, R; synth (c.segs, fs, L, R);
            std::vector<float> pad ((size_t) (1.5 * fs), 0.0f);     // Tech 3342: el medidor de archivo agrega 1.5 s de silencio
            L.insert (L.end(), pad.begin(), pad.end()); R.insert (R.end(), pad.begin(), pad.end());
            auto m = measure (L, R, fs);
            std::printf ("  %-30s LRA %.2f LU\n", c.name, m.lra());
            check (near (m.lra(), c.expect, 1.0), std::string ("LRA ") + c.name);
        }
    }

    std::printf ("T1f. Seno de 997 Hz: −20 dBFS de pico en los dos canales = −20.0 LUFS; en uno = −23.0\n");
    {
        std::vector<float> L, R; synth ({ { 10, -20, -20, 997 } }, 48000.0, L, R);
        auto m2 = measure (L, R, 48000.0);
        std::vector<float> Z (L.size(), 0.0f);
        auto m1 = measure (L, Z, 48000.0);
        std::printf ("  estéreo %.3f, un canal %.3f\n", m2.integrated(), m1.integrated());
        check (near (m2.integrated(), -20.0, 0.05) && near (m1.integrated(), -23.01, 0.05), "997 Hz");
    }


    std::printf ("T4. Limitador: pico verdadero de salida ≤ techo (+0.1 dB), medido ×4 y con una referencia ×8 independiente\n");
    {
        const double fs = 48000.0;
        struct Sig { const char* name; std::vector<float> L, R; };
        std::vector<Sig> sigs;
        sigs.push_back ({ "ruido rosa +12 dB", noise ((int) fs * 4, 1, 4.0), noise ((int) fs * 4, 2, 4.0) });
        { std::vector<float> a ((size_t) fs * 3); for (size_t i = 0; i < a.size(); ++i) a[i] = (float) (((i / 60) % 2) ? 1.8 : -1.8); sigs.push_back ({ "cuadrada 400 Hz a +5 dBFS", a, a }); }
        { std::vector<float> a ((size_t) fs * 3, 0.0f); for (size_t i = 0; i < a.size(); i += 2400) a[i] = 3.0f; sigs.push_back ({ "impulsos a +9.5 dBFS", a, a }); }
        { std::vector<float> a ((size_t) fs * 3); for (size_t i = 0; i < a.size(); ++i) a[i] = (float) (1.6 * std::sin (2 * kPiD * 0.25 * (double) i + kPiD / 4)); sigs.push_back ({ "seno fs/4 a 45° (+4 dB de pico verdadero)", a, a }); }
        { auto d = drums (fs, 6, 124, 7); for (auto& x : d) x *= 2.5f; sigs.push_back ({ "batería +8 dB", d, d }); }
        { std::vector<float> a ((size_t) fs * 3); for (size_t i = 0; i < a.size(); ++i) a[i] = (float) (2.0 * std::sin (2 * kPiD * 50 * (double) i / fs)); sigs.push_back ({ "seno de 50 Hz a +6 dBFS", a, a }); }
        for (double ceil : { -1.0, -0.3, -2.0 })
            for (auto& sg : sigs)
            {
                TruePeakLimiter lim; LimiterParams lp; lp.ceilingDb = ceil; lim.prepare (fs, lp);
                std::vector<float> L = sg.L, R = sg.R;
                for (size_t i = 0; i < L.size(); ++i) { double l = L[i], r = R[i]; lim.process (l, r); L[i] = (float) l; R[i] = (float) r; }
                auto m = measure (L, R, fs);
                const double ref = std::max (refTruePeakDb (L), refTruePeakDb (R));
                char buf[200]; std::snprintf (buf, sizeof buf, "techo %.1f, %s: ×4 %.2f dBTP, ×8 ref %.2f dBTP, reducción máx %.1f dB", ceil, sg.name, m.truePeakDb(), ref, lim.maxGrDb());
                check (m.truePeakDb() <= ceil + 0.02 && ref <= ceil + 0.02, buf);
            }
        // Latencia exacta: un impulso pequeño (sin reducción) sale demorado latency() muestras.
        TruePeakLimiter lim; LimiterParams lp; lim.prepare (fs, lp);
        int at = -1;
        for (int i = 0; i < 2000; ++i) { double l = i == 100 ? 0.25 : 0.0, r = l; lim.process (l, r); if (l > 0.2 && at < 0) at = i; }
        std::printf ("  latencia declarada %d, medida %d\n", lim.latency(), at - 100);
        check (at - 100 == lim.latency(), "latencia exacta");
    }

    const bool quick = argc > 1 && std::string (argv[1]) == "--quick";
    // ---- Sobremuestreo ×4: latencia entera y respuesta plana ----
    std::printf ("\nOs4: latencia y respuesta\n");
    {
        Os4 os; int at = -1; double best = 0;
        for (int i = 0; i < 400; ++i) { const double y = os.process (i == 50 ? 1.0 : 0.0, [] (double v) { return v; }); if (std::fabs (y) > best) { best = std::fabs (y); at = i - 50; } }
        std::printf ("  impulso: pico a %d muestras (declarada %d), amplitud %.4f\n", at, Os4::latency(), best);
        check (at == Os4::latency(), "latencia del sobremuestreo = 54");
        const double fs = 48000.0;
        double worst = 0.0;
        for (double hz : { 50.0, 1000.0, 10000.0, 18000.0 })
        {
            Os4 o2; double pIn = 0, pOut = 0;
            for (int i = 0; i < 48000; ++i)
            {
                const double x = std::sin (2 * kPiD * hz * i / fs), y = o2.process (x, [] (double v) { return v; });
                if (i > 1000) { pIn += x * x; pOut += y * y; }
            }
            worst = std::max (worst, std::fabs (10 * std::log10 (pOut / pIn)));
        }
        std::printf ("  ganancia 50 Hz-18 kHz: desvío máximo %.4f dB\n", worst);
        check (worst < 0.01, "plano hasta 18 kHz (±0.01 dB)");
    }
    // ---- T9: aliasing de la saturación (seno de 13 kHz, saturación fuerte) ----
    std::printf ("\nT9: aliasing de la saturación\n");
    {
        const double fs = 48000.0, hz = 12937.5;            // cae justo en un bin de la FFT de 8192 a 48 kHz... casi
        Saturation sat; SatParams sp; sp.on = true; sp.flavour = 1; sp.driveDb = 12.0; sp.inputLufs = -26.0; sat.prepare (fs, sp);
        const size_t N = 16384; std::vector<std::complex<double>> X (N);
        std::vector<double> y (N + 4096);
        for (size_t i = 0; i < N + 4096; ++i) { double l = 0.5 * std::sin (2 * kPiD * hz * i / fs), r = l; sat.process (l, r, true); y[i] = l; }
        for (size_t i = 0; i < N; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * kPiD * i / N); X[i] = { y[i + 4096] * w, 0.0 }; }
        fft (X);
        double fund = 0, alias = 0; double aliasHz = 0;
        for (size_t k = 1; k < N / 2; ++k)
        {
            const double f = k * fs / N, mag = std::abs (X[k]) / (N / 4.0);
            bool harmonic = false;
            for (int h = 1; h * hz < fs / 2 + 100; ++h) if (std::fabs (f - h * hz) < 60.0) harmonic = true;
            if (std::fabs (f) < 60.0) harmonic = true;
            if (std::fabs (f - hz) < 60.0) fund = std::max (fund, mag);
            else if (! harmonic && f < 20000.0 && mag > alias) { alias = mag; aliasHz = f; }
        }
        std::printf ("  fundamental %.1f dBFS, mayor producto no armónico %.1f dBFS (%.0f Hz)\n", toDb (fund), toDb (alias), aliasHz);
        check (toDb (alias) < -90.0, "aliasing < −90 dBFS");
    }
    if (! quick)
    {
        const double fs = 48000.0;
        std::vector<SongOpts> songs (4);
        songs[0].seed = 1;
        songs[1].seed = 2; songs[1].dark = true; songs[1].intro = true; songs[1].lufs = -24.0;
        songs[2].seed = 3; songs[2].bright = true; songs[2].wideBass = true; songs[2].lufs = -18.0;
        songs[3].seed = 4; songs[3].lufs = -12.0;
        std::vector<std::vector<float>> SL (songs.size()), SR (songs.size());
        for (size_t k = 0; k < songs.size(); ++k) song (fs, songs[k], SL[k], SR[k]);

        // ---- T2 / T4 / T5: sonoridad y techo en cada objetivo, y en cada estilo ----
        std::printf ("\nT2/T4/T5: objetivo de sonoridad, pico verdadero, sin recorte\n");
        int t2ok = 0, t2n = 0, t2refused = 0; double t2worst = 0.0, t4worst = -100.0;
        bool t5 = true;
        auto runCase = [&] (size_t k, Options o, const char* label) {
            const Rendered r = renderSong (SL[k], SR[k], fs, o);
            const auto m = measure (r.L, r.R, fs);
            const double tgt = r.plan.targetLufs;
            const double err = std::fabs (m.integrated() - tgt);
            const double tpRef = std::max (refTruePeakDb (r.L), refTruePeakDb (r.R));
            const double over = std::max (m.truePeakDb(), tpRef) - o.ceilingDb;
            // En objetivos de club puede negarse a llegar (con una nota que lo dice) en vez de destrozar la mezcla.
            bool refused = false;
            for (auto& note : r.plan.notes) if (note.find ("cannot reach") != std::string::npos) refused = true;
            ++t2n;
            if (err <= 0.5) { ++t2ok; t2worst = std::max (t2worst, err); }
            else if (refused && tgt >= -9.0 && r.plan.limiterAvgGrDb <= 14.0) { ++t2ok; ++t2refused; }
            else t2worst = std::max (t2worst, err);
            t4worst = std::max (t4worst, over);
            if (maxAbs (r.L) >= 1.0 || maxAbs (r.R) >= 1.0 || ! finiteAll (r.L) || ! finiteAll (r.R)) t5 = false;
            std::printf ("  canción %zu %-9s %-10s: %.2f LUFS (objetivo %.1f), %.2f dBTP ×4 / %.2f ×8 (techo %.1f), limitador %.1f dB%s\n",
                         k + 1, styleDef (o.style).id, label, m.integrated(), tgt, m.truePeakDb(), tpRef, o.ceilingDb, r.plan.limiterAvgGrDb, refused ? " (avisa que no llega)" : "");
        };
        for (auto& t : loudTargets())
        {
            if (t.matchInput) continue;
            for (size_t k = 0; k < songs.size(); ++k)
            {
                if (std::string (t.id) != "streaming" && k != 0 && k != 3) continue;
                Options o; o.targetLufs = t.lufs; o.ceilingDb = t.ceilingDb; runCase (k, o, t.id);
            }
        }
        for (int st = 1; st < kStyles; ++st) { Options o; o.style = (Style) st; o.intensity = 0.75; runCase (1, o, "streaming"); }
        { Options o; o.intensity = 1.0; o.style = Style::Loud; o.targetLufs = -8.0; o.ceilingDb = -2.0; runCase (2, o, "club 100%"); }
        std::printf ("  peor error %.2f LU (sin contar %d casos de club que avisan que no llegan), peor exceso de pico %.2f dB\n", t2worst, t2refused, t4worst);
        check (t2ok == t2n, "T2: |LUFS − objetivo| ≤ 0.5 LU en todos los casos (" + std::to_string (t2ok) + "/" + std::to_string (t2n) + ")");
        check (t2worst <= 0.2, "T2: meta 0.2 LU");
        check (t4worst <= 0.1, "T4: pico verdadero ≤ techo + 0.1 dB (×4 y ×8)");
        check (t5, "T5: ninguna muestra ≥ 1.0, sin NaN/Inf");

        // ---- T7: distancia espectral a la curva del estilo ----
        std::printf ("\nT7: distancia espectral\n");
        {
            int better = 0; bool never = true;
            for (size_t k = 0; k < songs.size(); ++k)
            {
                Options o; const Rendered r = renderSong (SL[k], SR[k], fs, o);
                const Report ro = Analyzer::run (r.L.data(), r.R.data(), r.L.size(), fs);
                const double di = spectralDistance (r.in.mid, styleDef (o.style).target), dO = spectralDistance (ro.mid, styleDef (o.style).target);
                std::printf ("  canción %zu: %.2f → %.2f dB\n", k + 1, di, dO);
                if (dO < di) ++better;
                if (dO > di + 0.5) never = false;
            }
            check (better >= (int) songs.size() - (int) songs.size() / 10, "T7: la distancia baja en ≥ 90 % de los casos");
            check (never, "T7: nunca sube más de 0.5 dB");
        }
        // ---- T10: compatibilidad mono ----
        std::printf ("\nT10: mono\n");
        {
            Options o; const Rendered r = renderSong (SL[2], SR[2], fs, o);
            const double ci = correlation (SL[2], SR[2], fs), co = correlation (r.L, r.R, fs);
            const double li = correlation (SL[2], SR[2], fs, 100.0), lo = correlation (r.L, r.R, fs, 100.0);
            std::vector<float> M (r.L.size()); for (size_t i = 0; i < M.size(); ++i) M[i] = 0.5f * (r.L[i] + r.R[i]);
            const double drop = measure (r.L, r.R, fs).integrated() - measure (M, M, fs).integrated();
            std::printf ("  correlación %.2f → %.2f; graves (<100 Hz) %.2f → %.2f; caída al sumar en mono %.2f dB (mono-maker %.0f Hz)\n", ci, co, li, lo, drop, r.plan.stereo.monoHz);
            check (co >= std::min (ci, 0.2), "T10: correlación ≥ min(entrada, 0.2)");
            check (r.plan.stereo.monoHz > 0.0 && lo >= 0.95, "T10: graves en mono (correlación ≥ 0.95)");
            check (drop <= 3.0, "T10: la suma mono pierde ≤ 3 dB");
        }
        // ---- T11: transparencia a intensidad 0 ----
        std::printf ("\nT11: transparencia\n");
        {
            Options o; o.intensity = 0.0; o.matchInput = true; o.ceilingDb = 0.0;
            const Rendered r = renderSong (SL[1], SR[1], fs, o);
            double e = 0, d = 0;
            for (size_t i = 0; i < r.L.size(); ++i) { e += std::pow (r.L[i] - SL[1][i], 2) + std::pow (r.R[i] - SR[1][i], 2); d += std::pow (SL[1][i], 2) + std::pow (SR[1][i], 2); }
            const double res = 10 * std::log10 (e / d + 1e-30);
            std::printf ("  residuo %.1f dB (pico de entrada %.1f dBTP)\n", res, r.in.truePeakDb);
            check (res < -60.0, "T11: nulo < −60 dB");
        }
        // ---- T12: determinismo con distintos tamaños de bloque ----
        std::printf ("\nT12: determinismo\n");
        {
            Options o; o.style = Style::Punchy; o.intensity = 0.8;
            const Rendered a = renderSong (SL[0], SR[0], fs, o, 512);
            bool same = true;
            for (int b : { 64, 4096, 1, -1 })
            {
                const Rendered c = renderSong (SL[0], SR[0], fs, o, b);
                const bool eq = std::memcmp (a.L.data(), c.L.data(), a.L.size() * 4) == 0 && std::memcmp (a.R.data(), c.R.data(), a.R.size() * 4) == 0;
                std::printf ("  bloque %s: %s\n", b < 0 ? "aleatorio" : std::to_string (b).c_str(), eq ? "idéntico" : "DISTINTO");
                same = same && eq;
            }
            check (same, "T12: salida idéntica bit a bit");
            // El aprendizaje predice la salida: lo que midió es lo que sale.
            const auto m = measure (a.L, a.R, fs);
            std::printf ("  aprendizaje %.3f LUFS, render %.3f LUFS\n", a.plan.outLufs, m.integrated());
            check (near (a.plan.outLufs, m.integrated(), 0.02), "T12: el render mide lo que predijo el aprendizaje (±0.02 LU)");
        }
        // ---- T13: independencia de la frecuencia de muestreo ----
        std::printf ("\nT13: frecuencia de muestreo\n");
        {
            // La misma canción a 96 kHz, remuestreada a 48 y 44.1 kHz (sintetizarla a cada frecuencia daría otra señal:
            // las sierras sin limitar banda doblan distinto).
            std::vector<BandCurve> tf; std::vector<double> lu;
            std::vector<float> a96, b96; song (96000.0, songs[0], a96, b96);
            for (double f2 : { 44100.0, 48000.0, 96000.0 })
            {
                std::vector<float> a = f2 == 96000.0 ? a96 : resample (a96, 96000.0, f2), b = f2 == 96000.0 ? b96 : resample (b96, 96000.0, f2);
                Options o; const Rendered r = renderSong (a, b, f2, o);
                const Report ro = Analyzer::run (r.L.data(), r.R.data(), r.L.size(), f2);
                BandCurve d {}; for (int k = 0; k < kBands; ++k) d[(size_t) k] = ro.mid[(size_t) k] - r.in.mid[(size_t) k];
                tf.push_back (d); lu.push_back (measure (r.L, r.R, f2).integrated());
                std::printf ("  %.1f kHz: %.2f LUFS, EQ [", f2 / 1000.0, lu.back());
                for (auto& e : r.plan.eq) std::printf (" %.1f", e.gainDb);
                std::printf (" ]\n");
            }
            double worst = 0.0;
            for (int k = 0; k < kBands; ++k)
            {
                const double f = bandCentres()[(size_t) k];
                if (f < 40.0 || f > 12500.0) continue;
                for (size_t j = 1; j < tf.size(); ++j) worst = std::max (worst, std::fabs (tf[j][(size_t) k] - tf[0][(size_t) k]));
            }
            std::printf ("  transferencia por banda (40 Hz-12.5 kHz): desvío máximo %.2f dB\n", worst);
            check (std::fabs (lu[1] - lu[0]) <= 0.1 && std::fabs (lu[2] - lu[0]) <= 0.1, "T13: LUFS de salida dentro de 0.1 LU");
            check (worst <= 0.5, "T13: el proceso por bandas coincide (±0.5 dB)");
        }
        // ---- T8: bombeo ----
        std::printf ("\nT8: bombeo\n");
        {
            // Pad sostenido + bombo a 120 bpm; profundidad de modulación agregada en 2-8 kHz a la frecuencia del pulso.
            auto padKick = [&] (std::vector<float>& L, std::vector<float>& R) {
                SongOpts so; so.seed = 9; song (fs, so, L, R);
                const auto dr = drums (fs, so.secs, 120.0, 9);
                double ph[2] {};
                for (size_t i = 0; i < L.size(); ++i)
                {
                    double p = 0; for (int c = 0; c < 2; ++c) { ph[c] += (c ? 3003.0 : 4507.0) / fs; if (ph[c] >= 1) ph[c] -= 1; p += std::sin (2 * kPiD * ph[c]); }
                    L[i] = R[i] = (float) (0.05 * p + 0.6 * dr[i] * 0.3);
                }
            };
            auto modDepth = [&] (const std::vector<float>& x) {
                Biquad bp; bp.set (bandPass (4000.0, 0.7, fs));
                const size_t hop = (size_t) (0.01 * fs); std::vector<double> env;
                double acc = 0; size_t cnt = 0;
                for (size_t i = (size_t) (2 * fs); i < x.size(); ++i) { const double y = bp.process (x[i]); acc += y * y; if (++cnt == hop) { env.push_back (10 * std::log10 (acc / hop + 1e-20)); acc = 0; cnt = 0; } }
                // Componente a 2 Hz (negras a 120 bpm) del envolvente en dB: amplitud pico a pico.
                double re = 0, im = 0;
                for (size_t k = 0; k < env.size(); ++k) { const double a = 2 * kPiD * 2.0 * k * 0.01; re += env[k] * std::cos (a); im += env[k] * std::sin (a); }
                return 4.0 * std::sqrt (re * re + im * im) / env.size();
            };
            std::vector<float> L, R; padKick (L, R);
            const double din = modDepth (L);
            Options o; o.style = Style::Loud; o.intensity = 0.75; o.targetLufs = -9.0;
            const Rendered r = renderSong (L, R, fs, o);
            const double dout = modDepth (r.L);
            // Control positivo: un compresor rápido y fuerte.
            Compressor bad; bad.prepare (fs); bad.set ({ -30.0, 8.0, 0.0, 1.0, 60.0, 0.0, 12.0 });
            std::vector<float> B (L.size()); for (size_t i = 0; i < L.size(); ++i) B[i] = (float) (L[i] * bad.gainFor (L[i]));
            const double dbad = modDepth (B);
            std::printf ("  modulación a 2 Hz en 2-8 kHz: entrada %.2f dB, Lacquer (Loud 75 %%, −9 LUFS) %.2f dB, compresor mal ajustado %.2f dB\n", din, dout, dbad);
            check (dout - din < 1.5, "T8: agrega menos de 1.5 dB de bombeo");
            check (dbad - din > 3.0, "T8: el control positivo sí bombea (la medida sirve)");
        }
        // ---- T15: robustez ----
        std::printf ("\nT15: robustez\n");
        {
            bool ok = true;
            auto run = [&] (const char* name, std::vector<float> L, std::vector<float> R) {
                Options o; o.intensity = 1.0; o.style = Style::Loud;
                const Rendered r = renderSong (L, R, fs, o);
                const bool fin = finiteAll (r.L) && finiteAll (r.R);
                const auto m = measure (r.L, r.R, fs);
                const bool peak = m.truePeakDb() <= o.ceilingDb + 0.1;
                std::printf ("  %-22s %s, %.2f LUFS, %.2f dBTP%s\n", name, fin ? "finito" : "NaN/Inf", m.integrated(), m.truePeakDb(), r.plan.silent ? " (silencio)" : "");
                ok = ok && fin && peak;
            };
            const size_t n = (size_t) (8 * fs);
            run ("silencio", std::vector<float> (n, 0.0f), std::vector<float> (n, 0.0f));
            run ("continua 0.5", std::vector<float> (n, 0.5f), std::vector<float> (n, 0.5f));
            { std::vector<float> q (n); for (size_t i = 0; i < n; ++i) q[i] = (i / 120) % 2 ? 1.0f : -1.0f; run ("cuadrada a escala plena", q, q); }
            { auto w = noise ((int) n, 77, 4.0); auto w2 = noise ((int) n, 78, 4.0); run ("ruido fuerte", w, w2); }
            { auto a = SL[0], b = SL[0]; run ("mono", a, b); }
            { auto a = SL[0], b = SL[0]; for (auto& v : b) v = -v; run ("canales invertidos", a, b); }
            { std::vector<float> a (SL[0].begin(), SL[0].begin() + (long) fs), b (SR[0].begin(), SR[0].begin() + (long) fs); run ("un segundo", a, b); }
            { std::vector<float> a (100, 0.3f), b (100, -0.3f); run ("100 muestras", a, b); }
            check (ok, "T15: sin NaN/Inf ni picos sobre el techo");
        }
    }
    std::printf (fails ? "\n%d FALLAS\n" : "\nTODO OK (0 fallas)\n", fails);
    return fails ? 1 : 0;
}
