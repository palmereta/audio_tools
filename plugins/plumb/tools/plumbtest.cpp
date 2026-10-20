// plumbtest: pruebas del motor de Plumb, sin JUCE (docs/research.md §7). Las voces son sintéticas: un pulso
// glótico (Rosenberg) con jitter y shimmer por formantes de vocal, así la altura de cada instante se conoce
// exactamente. Ninguna voz grabada.
#include "Engine.h"
#include <chrono>
#include <cstdio>
#include <functional>
#include <random>
#include <vector>

using namespace dh::pl;

static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what); if (! ok) ++fails; }
static double midiToHz (double m) { return 440.0 * std::exp2 ((m - 69.0) / 12.0); }

// Gaussiana portable: la misma secuencia que std::normal_distribution de libc++ (macOS), con la que se ajustaron
// los umbrales. libstdc++ (Linux) y MSVC implementan la distribución distinto y la voz de prueba cambiaba: con su
// secuencia, 82,4 Hz con jitter daba errores de octava y una nota de T5 no quedaba enganchada.
struct Gauss
{
    std::mt19937& rng;
    bool hot = false; double saved = 0;
    double uniform()    // generate_canonical<double, 53> con un motor de 32 bits, como libc++: dos salidas
    {
        const double lo = rng(), hi = rng();
        return (lo + hi * 4294967296.0) / 18446744073709551616.0;
    }
    double operator() (std::mt19937&)
    {
        if (hot) { hot = false; return saved; }
        double u, v, q;     // método polar de Marsaglia
        do { u = 2 * uniform() - 1; v = 2 * uniform() - 1; q = u * u + v * v; } while (q > 1 || q == 0);
        const double f = std::sqrt (-2 * std::log (q) / q);
        saved = v * f; hot = true;
        return u * f;
    }
};

// ── Voz sintética: f0(t) en Hz → muestras ──
struct Vowel { double f[4], bw[4]; };
static const Vowel kA { { 730, 1090, 2440, 3400 }, { 80, 90, 120, 150 } };
static const Vowel kI { { 270, 2290, 3010, 3600 }, { 60, 100, 120, 150 } };

static std::vector<float> voice (double fs, double secs, std::function<double (double)> f0, const Vowel& v = kA, double jitter = 0.003, double shimmer = 0.02, double noise = 0.002, uint32_t seed = 1)
{
    const int N = (int) (secs * fs);
    std::vector<float> out ((size_t) N);
    std::mt19937 rng (seed); Gauss g { rng };
    // Resonadores de dos polos en cascada.
    struct Res { double a1, a2, g, y1 = 0, y2 = 0; };
    std::vector<Res> rs;
    for (int i = 0; i < 4; ++i)
    {
        const double r = std::exp (-kPi * v.bw[i] / fs), th = 2 * kPi * v.f[i] / fs;
        rs.push_back ({ -2 * r * std::cos (th), r * r, (1 - r) * std::sqrt (1 - 2 * r * std::cos (2 * th) + r * r) });
    }
    double phase = 0, perJit = 0, amp = 1; double prev = 0;
    for (int n = 0; n < N; ++n)
    {
        const double t = n / fs;
        phase += f0 (t) * (1 + perJit) / fs;
        if (phase >= 1) { phase -= 1; perJit = jitter * g (rng); amp = 1 + shimmer * g (rng); }
        // Pulso de Rosenberg: subida 40 %, bajada 16 % del período; se usa su derivada (flujo → presión).
        double u;
        const double tp = 0.4, tn = 0.16;
        if (phase < tp) u = 0.5 * (1 - std::cos (kPi * phase / tp));
        else if (phase < tp + tn) u = std::cos (0.5 * kPi * (phase - tp) / tn);
        else u = 0;
        double x = (u - prev) * amp; prev = u;
        x += noise * g (rng);
        for (auto& r : rs) { const double y = r.g * x - r.a1 * r.y1 - r.a2 * r.y2; r.y2 = r.y1; r.y1 = y; x = y; }
        out[(size_t) n] = (float) x;
    }
    // Normalizado a −12 dBFS de pico.
    float pk = 1e-9f; for (float s : out) pk = std::max (pk, std::abs (s));
    for (auto& s : out) s *= 0.25f / pk;
    return out;
}

// ── Altura de referencia fuera de línea: YIN a tasa completa, ventana de 40 ms, cada 5 ms ──
static std::vector<std::pair<double, double>> offlinePitch (const std::vector<float>& x, double fs, double fmin = 60, double fmax = 1200)
{
    std::vector<std::pair<double, double>> out;
    const int W = (int) (0.04 * fs), tmax = (int) (fs / fmin), tmin = (int) (fs / fmax), hop = (int) (0.005 * fs);
    std::vector<double> d ((size_t) tmax + 2);
    for (int s = 0; s + W + tmax + 2 < (int) x.size(); s += hop)
    {
        double sum = 0; int bi = -1;
        for (int t = 1; t <= tmax + 1; ++t)
        {
            double acc = 0; for (int j = 0; j < W; ++j) { const double v = x[(size_t) (s + j)] - x[(size_t) (s + j + t)]; acc += v * v; }
            sum += acc; d[(size_t) t] = sum > 0 ? acc * t / sum : 1;
        }
        for (int t = tmin; t <= tmax; ++t) if (d[(size_t) t] < 0.1) { while (t + 1 <= tmax && d[(size_t) t + 1] < d[(size_t) t]) ++t; bi = t; break; }
        if (bi < 0) { out.push_back ({ (s + W / 2) / fs, 0 }); continue; }
        const double a = d[(size_t) bi - 1], b = d[(size_t) bi], c = d[(size_t) bi + 1], den = a - 2 * b + c;
        const double P = bi + (std::abs (den) > 1e-12 ? 0.5 * (a - c) / den : 0);
        out.push_back ({ (s + W / 2 + P / 2) / fs, fs / P });
    }
    return out;
}
static double cents (double hz, double ref) { return 1200.0 * std::log2 (hz / ref); }

static std::vector<float> run (Engine& e, const std::vector<float>& x) { std::vector<float> y (x.size()); for (size_t i = 0; i < x.size(); ++i) y[i] = e.process (x[i]); return y; }

static EngineParams hardParams() { EngineParams p; p.correct.direct = true; p.correct.speedMs = 0; p.correct.keepVibrato = 0; p.correct.keepDrift = 0; p.correct.hysteresis = 0; return p; }
static EngineParams naturalParams() { EngineParams p; p.correct.speedMs = 40; p.correct.keepVibrato = 1; p.correct.keepDrift = 0.3f; p.correct.flexIn = 60; p.correct.flexOut = 120; p.correct.hysteresis = 0.15f; p.correct.transitionMs = 60; return p; }

// Formantes del tramo, a la manera estándar: pasa bajos y diezmado a 8 kHz, LPC de orden 10, polos con ancho de
// banda menor a 400 Hz.
static std::vector<double> formants (const std::vector<float>& x, double fs, size_t from, size_t len)
{
    const int M = (int) std::lround (fs / 8000.0), p = 10;
    const double fd = fs / M;
    Biquad a, b; a.lowpass (3600, fs, 0.54); b.lowpass (3600, fs, 1.31);
    std::vector<double> d;
    for (size_t i = from >= 2000 ? from - 2000 : 0; i < from + len; ++i) { const double v = b.process (a.process (x[i])); if (i >= from && (i - from) % (size_t) M == 0) d.push_back (v); }
    const size_t L = d.size();
    std::vector<double> w (L);
    for (size_t i = 0; i < L; ++i) w[i] = (d[i] - (i ? 0.9 * d[i - 1] : 0)) * (0.5 - 0.5 * std::cos (2 * kPi * (i + 0.5) / L));
    double r[kMaxOrder + 1] {};
    for (int l = 0; l <= p; ++l) { double acc = 0; for (size_t i = (size_t) l; i < L; ++i) acc += w[i] * w[i - (size_t) l]; r[l] = acc; }
    KVec k; levinson (r, p, k);
    double aa[kMaxOrder + 1]; stepUp (k, p, aa);
    std::vector<std::complex<double>> z; polyRoots (aa, p, z, false);
    std::vector<double> f;
    for (auto& c : z) if (c.imag() > 0) { const double hz = std::arg (c) * fd / (2 * kPi), bw = -std::log (std::abs (c)) * fd / kPi; if (hz > 200 && bw < 400) f.push_back (hz); }
    std::sort (f.begin(), f.end());
    return f;
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const double fs = 48000;

    std::printf ("T1 Latencia informada y medida (identidad con Amount 0)\n");
    {
        bool ok = true;
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            for (Mode m : { Mode::live, Mode::tight, Mode::studio })
            {
                Engine e; e.prepare (sr, m); EngineParams p; p.correct.amount = 0; e.set (p);
                auto x = voice (sr, 1.0, [] (double) { return 220.0; });
                x[(size_t) (0.5 * sr)] += 0.5f;                              // un impulso para medir el retardo
                auto y = run (e, x);
                // Retardo medido: el desplazamiento que anula la diferencia.
                int best = -1; double bestErr = 1e18;
                for (int dly = 0; dly < 4096; ++dly)
                {
                    double err = 0; for (size_t i = (size_t) (0.4 * sr); i < (size_t) (0.6 * sr); ++i) { const double v = y[i] - x[i - (size_t) dly]; err += v * v; }
                    if (err < bestErr) { bestErr = err; best = dly; }
                }
                double sig = 0, err = 0;
                for (size_t i = (size_t) (0.2 * sr); i < x.size(); ++i) { const double v = y[i] - x[i - (size_t) e.latencySamples()]; err += v * v; sig += x[i] * x[i]; }
                const double db = 10 * std::log10 (err / sig + 1e-30);
                std::printf ("  %5.1f kHz %-6s informada %4d (%.2f ms), medida %4d, error de identidad %6.1f dB\n", sr / 1000, m == Mode::live ? "Live" : m == Mode::tight ? "Tight" : "Studio",
                             e.latencySamples(), 1000.0 * e.latencySamples() / sr, best, db);
                ok &= best == e.latencySamples() && db < -90;
            }
        check (ok, "latencia medida = informada en los 9 casos, identidad por debajo de −90 dB");
    }

    std::printf ("\nT2 Detección sobre vocales sintéticas (sin jitter: precisión; con jitter 0,3 %% y shimmer 2 %%: robustez)\n");
    {
        bool ok = true;
        struct C { double hz; int voice; } cases[] { { 82.4, 0 }, { 110, 0 }, { 147, 1 }, { 220, 1 }, { 330, 1 }, { 440, 2 }, { 660, 2 }, { 880, 2 } };
        for (auto& c : cases)
        {
          for (int jit = 0; jit < 2; ++jit)
          {
            Detector d2; d2.prepare (fs); d2.setRange (voicePreset (c.voice));
            auto x = voice (fs, 0.8, [&] (double) { return c.hz; }, c.hz > 500 ? kI : kA, jit ? 0.003 : 0.0, jit ? 0.02 : 0.0, 0.002, (uint32_t) c.hz);
            std::vector<double> errs; int octave = 0, unvoiced = 0; double firstLock = -1;
            for (size_t i = 0; i < x.size(); ++i)
            {
                PitchEstimate e;
                if (! d2.push (x[i], e)) continue;
                const double t = i / fs;
                if (! e.voiced) { if (t > 0.1) ++unvoiced; continue; }
                const double ce = cents (e.hz, c.hz);
                if (firstLock < 0 && std::abs (ce) < 20) firstLock = t;
                if (t < 0.1) continue;
                if (std::abs (ce) > 600) ++octave; else errs.push_back (std::abs (ce));
            }
            std::sort (errs.begin(), errs.end());
            const double med = errs.empty() ? 999 : errs[errs.size() / 2], p95 = errs.empty() ? 999 : errs[errs.size() * 95 / 100];
            std::printf ("  %6.1f Hz %s: mediana %.2f cents, p95 %.2f, errores de octava %d, sin voz %d, engancha a los %.1f ms\n", c.hz, jit ? "con jitter" : "limpia    ", med, p95, octave, unvoiced, firstLock * 1000);
            ok &= med < (jit ? 4.0 : 1.5) && p95 < (jit ? 10.0 : 4.0) && octave == 0 && unvoiced < 3 && firstLock > 0 && firstLock < 0.035;
          }
        }
        check (ok, "sin jitter: mediana < 1,5 cents y p95 < 4; con jitter: < 4 y < 10; sin errores de octava; engancha en < 35 ms");
    }

    std::printf ("\nT3 Afinación dura: A3 + 35 cents → A3\n");
    {
        bool ok = true;
        for (Mode m : { Mode::live, Mode::tight, Mode::studio })
        {
            Engine e; e.prepare (fs, m); e.set (hardParams());
            const double f = midiToHz (57.35);
            auto x = voice (fs, 1.5, [&] (double) { return f; });
            auto y = run (e, x);
            auto pt = offlinePitch (y, fs);
            std::vector<double> errs;
            for (auto& q : pt) if (q.first > 0.3 && q.first < 1.4 && q.second > 0) errs.push_back (cents (q.second, 220.0));
            double mean = 0; for (double v : errs) mean += v; mean /= std::max<size_t> (1, errs.size());
            double worst = 0; for (double v : errs) worst = std::max (worst, std::abs (v));
            std::printf ("  %-6s salida %+.2f cents de A3 (peor %+.2f), %d empalmes, retardo de contenido %.0f-%.0f\n", m == Mode::live ? "Live" : m == Mode::tight ? "Tight" : "Studio",
                         mean, worst, e.spliceCount(), e.contentDelayMin(), e.contentDelayMax());
            ok &= std::abs (mean) < 2 && worst < 6;
        }
        check (ok, "la salida queda a menos de 2 cents de A3 en promedio y 6 en el peor caso");
    }

    std::printf ("\nT4 Natural: vibrato de ±50 cents a 5,5 Hz alrededor de A3 + 30 cents\n");
    {
        Engine e; e.prepare (fs, Mode::tight); e.set (naturalParams());
        auto x = voice (fs, 3.0, [] (double t) { return midiToHz (57.3 + 0.5 * std::sin (2 * kPi * 5.5 * t)); });
        auto y = run (e, x);
        auto pin = offlinePitch (x, fs), pout = offlinePitch (y, fs);
        auto stats = [&] (const std::vector<std::pair<double, double>>& v, double& mean, double& ext) {
            std::vector<double> c; for (auto& q : v) if (q.first > 1.5 && q.first < 2.8 && q.second > 0) c.push_back (cents (q.second, 220.0));
            mean = 0; for (double a : c) mean += a; mean /= std::max<size_t> (1, c.size());
            double lo = 1e9, hi = -1e9; for (double a : c) { lo = std::min (lo, a); hi = std::max (hi, a); } ext = (hi - lo) / 2; };
        double mi, ei, mo, eo; stats (pin, mi, ei); stats (pout, mo, eo);
        std::printf ("  entrada: centro %+.1f cents, vibrato ±%.1f; salida: centro %+.1f, vibrato ±%.1f\n", mi, ei, mo, eo);
        check (std::abs (mo) < 6 && eo > 0.75 * ei && eo < 1.25 * ei, "el centro va a A3 (±6 cents) y el vibrato se conserva (±25 %)");
        // Afinación dura: con vibrato alrededor del centro de la nota (si cruzara el punto medio saltaría a la
        // nota vecina: el "warble" clásico) la salida queda plana.
        auto xc = voice (fs, 3.0, [] (double t) { return midiToHz (57.0 + 0.4 * std::sin (2 * kPi * 5.5 * t)); });
        Engine h; h.prepare (fs, Mode::tight); h.set (hardParams());
        auto yh = run (h, xc); auto ph = offlinePitch (yh, fs);
        double mh, eh; stats (ph, mh, eh);
        std::printf ("  afinación dura con vibrato de ±40 cents sobre A3: centro %+.1f, vibrato ±%.1f\n", mh, eh);
        check (std::abs (mh) < 4 && eh < 12, "en afinación dura el vibrato desaparece (±12 cents como mucho)");
    }

    std::printf ("\nT5 Escalones: notas desafinadas que cambian cada 400 ms (afinación dura, Live)\n");
    {
        Engine e; e.prepare (fs, Mode::live); e.set (hardParams());
        const double notes[] = { 57.3, 59.75, 62.2, 60.8, 64.35, 57.7 };
        auto x = voice (fs, 2.4, [&] (double t) { return midiToHz (notes[std::min (5, (int) (t / 0.4))]); });
        auto y = run (e, x);
        auto pt = offlinePitch (y, fs);
        bool ok = true; double worstLock = 0;
        for (int k = 0; k < 6; ++k)
        {
            const double target = std::round (notes[k]), t0 = k * 0.4;
            double lock = -1;
            for (auto& q : pt) if (q.first > t0 + 0.005 && q.first < t0 + 0.39 && q.second > 0)
            { const double c = cents (q.second, midiToHz (target)); if (std::abs (c) < 8) { if (lock < 0) lock = q.first - t0; } else if (lock >= 0 && q.first - t0 > lock + 0.03) lock = -2; }
            std::printf ("  nota %.2f → %d: engancha a los %.0f ms\n", notes[k], (int) target, lock * 1000);
            ok &= lock > 0 && lock < 0.06; worstLock = std::max (worstLock, lock);
        }
        check (ok, "cada nota llega a su objetivo (±8 cents) en menos de 60 ms y se queda");
    }

    std::printf ("\nT6 Retardo de contenido dentro de lo previsto mientras corrige\n");
    {
        bool ok = true;
        for (double off : { +0.45, -0.45, +2.0, -2.0, +12.0, -12.0 })
        {
            Engine e; e.prepare (fs, Mode::live);
            EngineParams p = hardParams(); p.correct.transpose = std::abs (off) > 1 ? (float) off : 0.0f; e.set (p);
            const double f = midiToHz (57 + (std::abs (off) > 1 ? 0 : off));
            auto x = voice (fs, 1.0, [&] (double) { return f; });
            run (e, std::vector<float> (x.begin(), x.begin() + (long) (0.2 * fs)));
            e.resetDelayStats();
            run (e, std::vector<float> (x.begin() + (long) (0.2 * fs), x.end()));
            const double T = fs / f, lim = e.baseDelay() + 1.5 * T + 24 + 16;
            std::printf ("  %+5.2f st: retardo de contenido %.0f-%.0f muestras (base %d, límite %.0f), %d empalmes\n", off, e.contentDelayMin(), e.contentDelayMax(), e.baseDelay(), lim, e.spliceCount());
            ok &= e.contentDelayMin() >= e.baseDelay() - 1 && e.contentDelayMax() <= lim;
        }
        check (ok, "el retardo nunca baja de la latencia informada ni pasa de un período y medio más");
    }

    std::printf ("\nT7 Formantes: +4 semitonos con y sin preservación; Throat +3 (amplitud de los armónicos 2-14 contra la voz ideal)\n");
    {
        // La referencia es la misma vocal sintetizada directamente a la altura de salida (con los formantes en su
        // lugar, movidos con la altura o movidos por Throat). Se compara la amplitud de cada armónico relativa al
        // primero: error RMS en dB.
        const double f0 = 150.0, fo = f0 * std::exp2 (4 / 12.0);
        auto harm = [&] (const std::vector<float>& y, double f, size_t a, size_t L) {
            double re = 0, im = 0;
            for (size_t i = 0; i < L; ++i) { const double w = 0.5 - 0.5 * std::cos (2 * kPi * (i + 0.5) / L); re += y[a + i] * w * std::cos (2 * kPi * f * i / fs); im += y[a + i] * w * std::sin (2 * kPi * f * i / fs); }
            return 20 * std::log10 (std::sqrt (re * re + im * im) + 1e-12); };
        const size_t a0 = (size_t) (0.5 * fs), L = (size_t) (0.4 * fs);
        auto err = [&] (const std::vector<float>& y, const std::vector<float>& ref) {
            const double y0 = harm (y, fo, a0, L), r0 = harm (ref, fo, a0, L); double s2 = 0;
            for (int h = 2; h <= 14; ++h) { const double d = (harm (y, h * fo, a0, L) - y0) - (harm (ref, h * fo, a0, L) - r0); s2 += d * d; }
            return std::sqrt (s2 / 13); };
        auto x = voice (fs, 1.2, [&] (double) { return f0; }, kA, 0, 0, 0);
        auto ideal = voice (fs, 1.2, [&] (double) { return fo; }, kA, 0, 0, 0);
        Vowel mv = kA, th = kA; for (int i = 0; i < 4; ++i) { mv.f[i] *= std::exp2 (4 / 12.0); th.f[i] *= std::exp2 (3 / 12.0); }
        auto idealMoved = voice (fs, 1.2, [&] (double) { return fo; }, mv, 0, 0, 0), idealThroat = voice (fs, 1.2, [&] (double) { return fo; }, th, 0, 0, 0);
        auto shifted = [&] (bool preserve, float throat) {
            Engine e; e.prepare (fs, Mode::tight); EngineParams p; p.correct.amount = 0; p.correct.transpose = 4; p.formants = preserve; p.throat = throat; e.set (p);
            return run (e, x); };
        const auto yp = shifted (true, 0), yn = shifted (false, 0), yt = shifted (true, 3);
        const double ep = err (yp, ideal), en = err (yn, idealMoved), et = err (yt, idealThroat), eWrong = err (yp, idealMoved);
        std::printf ("  error RMS: preservados %.1f dB (contra formantes movidos: %.1f), sin preservar %.1f, throat +3 %.1f\n", ep, eWrong, en, et);
        check (ep < 4 && eWrong > 6, "con preservación los formantes quedan (error < 4 dB; contra los movidos > 6)");
        check (en < 2.5, "sin preservación se mueven con la altura (error < 2,5 dB)");
        check (et < 4, "Throat +3 los mueve un 19 % (error < 4 dB)");
    }

    std::printf ("\nT8 Tonalidad automática: una melodía en Re mayor\n");
    {
        Engine e; e.prepare (fs, Mode::tight); EngineParams p; p.correct.amount = 0; e.set (p);
        const int mel[] = { 62, 64, 66, 67, 69, 67, 66, 64, 62, 69, 71, 69, 66, 62, 64, 61, 62, 66, 69, 74, 69, 66, 62, 62 };
        auto x = voice (fs, 24 * 0.35, [&] (double t) { return midiToHz (mel[std::min (23, (int) (t / 0.35))]); });
        run (e, x);
        int tonic; bool minor; float conf; e.keyEstimate (tonic, minor, conf);
        static const char* nm[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        std::printf ("  estimada: %s %s (confianza %.2f)\n", nm[tonic], minor ? "menor" : "mayor", conf);
        check ((tonic == 2 && ! minor) || (tonic == 11 && minor), "Re mayor (o su relativa, Si menor)");
    }

    std::printf ("\nT9 Sin NaN ni explosiones: parámetros al azar, ruido, silencio, impulsos, saturación\n");
    {
        Engine e; e.prepare (fs, Mode::tight);
        std::mt19937 rng (7); std::uniform_real_distribution<float> u (0, 1);
        bool ok = true; float peak = 0;
        auto v = voice (fs, 1.0, [] (double t) { return 180 + 60 * std::sin (t * 3); });
        for (int blk = 0; blk < 400; ++blk)
        {
            EngineParams p; p.voice = (int) (u (rng) * 4); p.correct.amount = u (rng); p.correct.speedMs = u (rng) * 400; p.correct.direct = u (rng) > 0.5f;
            p.correct.transpose = (u (rng) - 0.5f) * 48; p.throat = (u (rng) - 0.5f) * 24; p.formantAmount = u (rng); p.formants = u (rng) > 0.3f;
            p.correct.allowed = (uint16_t) (rng() & 0xFFF); p.correct.vibDepth = u (rng) * 100; p.mix = u (rng);
            e.set (p);
            for (int i = 0; i < 512; ++i)
            {
                float x;
                const int kind = blk % 5;
                if (kind == 0) x = v[(size_t) ((blk * 512 + i) % (int) v.size())];
                else if (kind == 1) x = (u (rng) - 0.5f) * 2;
                else if (kind == 2) x = 0;
                else if (kind == 3) x = i == 0 ? 1.0f : 0.0f;
                else x = v[(size_t) ((blk * 512 + i) % (int) v.size())] * 8;
                const float y = e.process (x);
                ok &= dh::isFiniteF (y); peak = std::max (peak, std::abs (y));
            }
        }
        std::printf ("  pico de salida %.2f\n", peak);
        check (ok && peak < 64, "salida finita y acotada");
    }

    std::printf ("\nT10 Determinismo: misma salida con bloques de 64, 333 y 512 muestras\n");
    {
        auto x = voice (fs, 2.0, [] (double t) { return midiToHz (57.3 + 0.4 * std::sin (2 * kPi * 5 * t)); });
        std::vector<std::vector<float>> outs;
        for (int b : { 64, 333, 512 })
        {
            Engine e; e.prepare (fs, Mode::tight); e.set (naturalParams());
            std::vector<float> y (x.size());
            for (size_t i = 0; i < x.size(); i += (size_t) b) { e.set (naturalParams()); for (size_t j = i; j < std::min (x.size(), i + (size_t) b); ++j) y[j] = e.process (x[j]); }
            outs.push_back (y);
        }
        check (outs[0] == outs[1] && outs[1] == outs[2], "bit a bit iguales");
    }

    std::printf ("\nT11 Empalmes sin clics: el salto entre muestras no supera al de la entrada\n");
    {
        Engine e; e.prepare (fs, Mode::live); e.set (hardParams());
        auto x = voice (fs, 2.0, [] (double t) { return midiToHz (57.4 + 0.3 * std::sin (2 * kPi * 0.7 * t)); });
        auto y = run (e, x);
        double dx = 0, dy = 0;
        for (size_t i = 1; i < x.size(); ++i) { dx = std::max (dx, (double) std::abs (x[i] - x[i - 1])); }
        for (size_t i = (size_t) (0.2 * fs); i < y.size(); ++i) dy = std::max (dy, (double) std::abs (y[i] - y[i - 1]));
        std::printf ("  máximo salto: entrada %.4f, salida %.4f (%d empalmes)\n", dx, dy, e.spliceCount());
        check (dy < 1.5 * dx, "la salida no tiene saltos mayores que 1,5 veces los de la entrada");
    }

    std::printf ("\nT12b Sin picos de corrección en saltos de nota grandes (Natural, notas a menos de medio tono del objetivo)\n");
    {
        Engine e; e.prepare (fs, Mode::tight); e.set (naturalParams());
        const double notes[] = { 57.3, 64.2, 52.8, 69.35, 57.1, 45.4, 57.2 };
        auto x = voice (fs, 2.8, [&] (double t) { return midiToHz (notes[std::min (6, (int) (t / 0.4))]); });
        // Un error de octava al arranque de cada nota: la entrada empieza con un período a la mitad durante 3 ms.
        int64_t rd = 0; double worst = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            e.process (x[i]);
            for (; rd < e.viewWrite(); ++rd) { const auto o = e.viewAt (rd); if (o.voiced) worst = std::max (worst, (double) std::abs (o.delta)); }
        }
        std::printf ("  corrección máxima %.2f semitonos\n", worst);
        check (worst < 1.0, "la corrección nunca pasa de un semitono");
    }

    std::printf ("\nT13 Afinación dura: un vibrato sobre el punto medio entre dos notas no salta de una a otra\n");
    {
        // Do mayor; la voz en 64,6 (entre Mi y Fa, el medio es 64,5) con vibrato de ±35 cents a 5,5 Hz.
        auto run = [&] (float hyst) {
            Engine e; e.prepare (fs, Mode::tight); EngineParams p = hardParams(); p.correct.hysteresis = hyst; p.correct.allowed = 0x0AB5; e.set (p);
            auto x = voice (fs, 2.0, [] (double t) { return midiToHz (64.6 + 0.35 * std::sin (2 * kPi * 5.5 * t)); });
            int64_t rd = 0; int changes = 0; float last = -1;
            for (size_t i = 0; i < x.size(); ++i)
            {
                e.process (x[i]);
                for (; rd < e.viewWrite(); ++rd)
                {
                    const auto o = e.viewAt (rd);
                    if (! o.voiced || o.time < 0.25 * fs) continue;
                    if (last >= 0 && std::abs (o.target - last) > 0.5f) ++changes;
                    last = o.target;
                }
            }
            return changes;
        };
        const int hard = run (0.06f), warble = run (0.0f);
        std::printf ("  cambios de nota: Hard %d, histéresis 0 (warble) %d\n", hard, warble);
        check (hard == 0, "con histéresis la nota no cambia");
        check (warble > 10, "con histéresis 0 salta con el vibrato (el efecto buscado)");
    }

    std::printf ("\nT14 MIDI: escala desde las notas apretadas (queda al soltar), Auto (vuelve a la escala), nota fija\n");
    {
        // La voz en Mi + 30 cents, cromática; se aprieta y se suelta un Sol a los 0,3 s.
        auto run = [&] (int mode, int note) {
            Engine e; e.prepare (fs, Mode::tight); EngineParams p = hardParams(); p.correct.hysteresis = 0.06f; p.correct.midiMode = mode; e.set (p);
            auto x = voice (fs, 1.2, [] (double) { return midiToHz (64.3); });
            int64_t rd = 0; float held = -1, after = -1;
            for (size_t i = 0; i < x.size(); ++i)
            {
                if (i == (size_t) (0.3 * fs)) e.noteOn (note);
                if (i == (size_t) (0.7 * fs) && mode != 2) e.noteOff (note);
                e.process (x[i]);
                for (; rd < e.viewWrite(); ++rd)
                {
                    const auto o = e.viewAt (rd);
                    if (! o.voiced) continue;
                    if (o.time > 0.5 * fs && o.time < 0.65 * fs) held = o.target;   // o.time es la del centro del análisis
                    if (o.time > 1.0 * fs) after = o.target;
                }
            }
            return std::make_pair (held, after);
        };
        const auto latch = run (1, 67), autoM = run (3, 67), lock = run (2, 60);
        std::printf ("  escala desde notas: %.1f / %.1f; Auto: %.1f / %.1f; nota fija (Do): %.1f\n", latch.first, latch.second, autoM.first, autoM.second, lock.second);
        check (std::abs (latch.first - 67) < 0.01f && std::abs (latch.second - 67) < 0.01f, "escala desde notas: Sol mientras se aprieta y después de soltarlo");
        check (std::abs (autoM.first - 67) < 0.01f && std::abs (autoM.second - 64) < 0.01f, "Auto: Sol mientras se aprieta, Mi (la escala) al soltarlo");
        check (std::abs (lock.second - 60) < 0.01f, "nota fija: el objetivo es la nota apretada, lejos de lo cantado");
    }

    std::printf ("\nT15 Natural con entradas desde abajo: notas de 0,5 s que arrancan 0,9 semitonos abajo, desafinadas 25-38 cents\n");
    {
        // Como canta una voz: cada nota entra desde abajo (scoop de 70 ms) y queda corrida del centro. El centro de la
        // salida en la mitad de cada nota tiene que caer en la nota objetivo.
        const double notes[] = { 64.28, 66.65, 69.18, 66.78, 64.38, 61.70 }, expect[] = { 64, 67, 69, 67, 64, 62 };
        Engine e; e.prepare (fs, Mode::tight); EngineParams p = naturalParams(); p.correct.allowed = 0x0AB5 << 0; e.set (p);
        // La menor = Do mayor en clases de altura (0x0AB5 con tónica Do).
        auto x = voice (fs, 3.3, [&] (double t) {
            const int k = std::min (5, (int) (t / 0.55)); const double age = t - k * 0.55;
            return midiToHz (notes[k] - 0.9 * std::exp (-age / 0.07)); });
        for (size_t i = 0; i < x.size(); ++i) if (std::fmod (i / fs, 0.55) > 0.5) x[i] = 0;          // un respiro entre notas
        std::vector<std::vector<float>> outs (6);
        int64_t rd = 0;
        for (size_t i = 0; i < x.size(); ++i)
        {
            e.process (x[i]);
            for (; rd < e.viewWrite(); ++rd)
            {
                const auto o = e.viewAt (rd); if (! o.voiced) continue;
                const double t = o.time / fs; const int k = std::min (5, (int) (t / 0.55)); const double age = t - k * 0.55;
                if (age > 0.2 && age < 0.45) outs[(size_t) k].push_back (o.output);
            }
        }
        double worst = 0;
        for (int k = 0; k < 6; ++k)
        {
            auto& v = outs[(size_t) k]; std::nth_element (v.begin(), v.begin() + (long) v.size() / 2, v.end());
            const double c = v[v.size() / 2], err = (c - expect[k]) * 100;
            std::printf ("  nota %.2f → %.2f (%+.1f cents de %g)\n", notes[k], c, err, expect[k]);
            worst = std::max (worst, std::abs (err));
        }
        check (worst < 10, "el centro de cada nota queda a menos de 10 cents del objetivo");
    }

    std::printf ("\nT12 CPU (48 kHz, mono)\n");
    {
        auto x = voice (fs, 10.0, [] (double t) { return midiToHz (57.3 + 0.5 * std::sin (2 * kPi * 5.5 * t)); });
        struct Case { const char* name; Mode m; bool throat; };
        for (Case c : { Case { "Live", Mode::live, false }, Case { "Tight", Mode::tight, false }, Case { "Studio", Mode::studio, false }, Case { "Tight con Throat +3", Mode::tight, true } })
        {
            Engine e; e.prepare (fs, c.m); EngineParams p = naturalParams(); if (c.throat) p.throat = 3; e.set (p);
            const auto t0 = std::chrono::steady_clock::now();
            float sink = 0; for (float v : x) sink += e.process (v);
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  %-22s %.2f %% de un núcleo%s\n", c.name, 100 * secs / 10.0, sink == 12345.f ? " " : "");
        }
    }

    std::printf ("\n%s (%d fallas)\n", fails ? "HAY FALLAS" : "TODO OK", fails);
    return fails ? 1 : 0;
}
