// tarnishtest: pruebas del motor de Tarnish sin JUCE (docs/research.md §5). Cada prueba imprime lo que midió y
// [ok] o [FALLA]; al final, el total. Las señales se sintetizan acá: no hacen falta archivos.
#include "Engine.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <functional>

using namespace dh::tn;
static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what); if (! ok) ++fails; }

// Parámetros "a cero": estudio sin wow, flutter, ruido, caídas ni nada.
static EngineParams clean (int machine = kStudio, int variant = 1)
{
    EngineParams p; p.machine = machine; p.variant = variant;
    p.wowPct = 0; p.flutterPct = 0; p.scrape = 0; p.noise = 0; p.wear = 0; p.dropouts = 0; p.crackle = 0;
    p.driveDb = -12; p.autoGain = true; p.fading = 0; p.compress = 0; p.whistle = 0; p.detector = 0;
    return p;
}
struct Render { std::vector<float> l, r; };
static Render render (Engine& e, const std::vector<float>& inL, const std::vector<float>& inR, int block, int64_t pos0 = 0,
                      std::function<void (int64_t, Engine&)> hook = nullptr)
{
    Render o; o.l = inL; o.r = inR;
    const int n = (int) inL.size();
    for (int i = 0; i < n;)
    {
        int b = block > 0 ? block : 1 + (int) (hashUni (99, (uint64_t) i) * 700);
        b = std::min (b, n - i);
        if (hook) hook (pos0 + i, e);
        e.process (o.l.data() + i, o.r.data() + i, b, pos0 + i);
        i += b;
    }
    return o;
}
static std::vector<float> sine (double f, double amp, double secs, double fs)
{
    std::vector<float> x ((size_t) (secs * fs));
    for (size_t i = 0; i < x.size(); ++i) x[i] = (float) (amp * std::sin (kTwoPi * f * (double) i / fs));
    return x;
}
static std::vector<float> noise (double amp, double secs, double fs, uint64_t seed)
{
    Rand r (seed); std::vector<float> x ((size_t) (secs * fs));
    for (auto& v : x) v = (float) (amp * r.bi());
    return x;
}
static double rms (const std::vector<float>& x, size_t a, size_t b) { double s = 0; for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i]; return std::sqrt (s / std::max<size_t> (1, b - a)); }
// Amplitud de una frecuencia por correlación (DFT de un solo bin).
static double tone (const std::vector<float>& x, double f, double fs, size_t a, size_t b)
{
    double re = 0, im = 0;
    for (size_t i = a; i < b; ++i) { re += x[i] * std::cos (kTwoPi * f * (double) i / fs); im += x[i] * std::sin (kTwoPi * f * (double) i / fs); }
    return 2.0 * std::sqrt (re * re + im * im) / (double) (b - a);
}
static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -kTwoPi / (double) len; const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len) { std::complex<double> w (1); for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; } }
    }
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);       // sin búfer: si algo se cae, se ve hasta dónde llegó
    const double fs = 48000;
    auto start = std::chrono::steady_clock::now();

    std::printf ("\nT1 Robustez: todas las máquinas, señales extremas y ajustes al azar\n");
    {
        bool ok = true; double peak = 0;
        Rand rr (5);
        for (int m = 0; m < kNumMachines; ++m)
        {
            double mpeak = 0;
            for (int trial = 0; trial < 6; ++trial)
            {
                Engine e; EngineParams p; p.machine = m; p.variant = trial % 4; p.driveDb = -24 + 48 * rr.uni(); p.bias = rr.bi(); p.saturation = rr.uni();
                p.wowPct = 3 * rr.uni(); p.flutterPct = rr.uni(); p.wear = rr.uni(); p.dropouts = rr.uni(); p.noise = rr.uni(); p.crackle = rr.uni();
                p.scratches = trial % 4; p.hum = rr.uni(); p.azimuth = rr.uni(); p.printDb = -40; p.nr = trial % 3; p.mistrackDb = 6 * rr.bi();
                p.bits = 1 + 23 * rr.uni(); p.rateHz = 500 + 47000 * rr.uni(); p.crushRaw = trial & 1; p.jitter = rr.uni(); p.varispeed = 12 * rr.bi();
                p.fading = rr.uni(); p.tuning = rr.bi(); p.whistle = rr.uni(); p.frameLoss = rr.uni(); p.codec = rr.uni(); p.stop = trial == 3;
                e.prepare (fs, 512); e.set (p);
                std::vector<float> l ((size_t) (2 * fs)), r ((size_t) (2 * fs));
                for (size_t i = 0; i < l.size(); ++i)
                {
                    const int kind = (int) (i / (size_t) (fs / 2));
                    double x = kind == 0 ? 0.0 : (kind == 1 ? (i % 200 < 100 ? 1.0 : -1.0) : (kind == 2 ? 4.0 * rr.bi() : (i % 997 == 0 ? 1.0 : 0.0)));
                    l[i] = (float) x; r[i] = (float) (kind == 1 ? 1.0 : x);
                }
                for (size_t i = 0; i < l.size(); i += 256)
                {
                    const int b = (int) std::min<size_t> (256, l.size() - i);
                    if ((i / 256) % 7 == 0) { p.driveDb = -24 + 48 * rr.uni(); p.wowPct = 3 * rr.uni(); p.varispeed = 12 * rr.bi(); e.set (p); }
                    e.process (l.data() + i, r.data() + i, b, (int64_t) i);
                }
                for (size_t i = 0; i < l.size(); ++i) { ok &= isFiniteD (l[i]) && isFiniteD (r[i]); peak = std::max (peak, (double) std::fabs (l[i])); mpeak = std::max (mpeak, (double) std::fabs (l[i])); }
            }
            std::printf ("  %-8s pico %.1f dBFS\n", profile (m, 0).name, gainToDb (mpeak));
        }
        std::printf ("  pico máximo %.2f (%.1f dBFS)\n", peak, gainToDb (peak));
        check (ok, "sin NaN ni Inf");
        check (peak < dbToGain (24.0), "salida acotada (≤ +24 dBFS)");
        // Silencio largo: la histéresis no se desboca (serie de Langevin y sentido retenido).
        Engine e; e.prepare (fs, 512); e.set (clean());
        std::vector<float> z ((size_t) (30 * fs)), z2 = z;
        auto o = render (e, z, z2, 512);
        double mx = 0; for (float v : o.l) mx = std::max (mx, (double) std::fabs (v));
        check (mx < 1e-9 && isFiniteD (mx), "30 s de silencio digital dan silencio");
    }

    std::printf ("\nT2 Latencia exacta\n");
    {
        for (int m : { kStudio, kCassette, kVinyl, kRadio, kPhone })
        {
            Engine e; e.prepare (fs, 512); EngineParams p = clean (m, m == kStudio ? 0 : 0); p.highCut = 22000; e.set (p);
            auto x = noise (0.05, 4.0, fs, 11); auto y = x;
            auto o = render (e, x, y, 512);
            // Correlación cruzada en ±64 muestras alrededor de la latencia informada.
            int best = 0; double bv = -1e9;
            for (int lag = e.latency() - 64; lag <= e.latency() + 64; ++lag)
            {
                double s = 0; for (size_t i = 20000; i + (size_t) lag < x.size(); i += 3) s += (double) x[i] * o.l[i + (size_t) lag];
                if (s > bv) { bv = s; best = lag; }
            }
            std::printf ("  %-8s latencia informada %d, medida %d\n", profile (m, 0).name, e.latency(), best);
            check (std::abs (best - e.latency()) <= (m >= kRadio ? 16 : 1), m >= kRadio ? "la señal sale a la latencia informada (más el retardo de grupo de sus filtros de banda)" : "la señal sale a la latencia informada");
        }
        Engine e; e.prepare (fs, 512); EngineParams p; p.mix = 0; e.set (p);
        auto x = noise (0.3, 2.0, fs, 12); auto y = x;
        auto o = render (e, x, y, 333);
        double err = 0; for (size_t i = (size_t) e.latency(); i < x.size(); ++i) err = std::max (err, (double) std::fabs (o.l[i] - x[i - (size_t) e.latency()]));
        std::printf ("  con Mix 0: error %.2e\n", err);
        check (err < 1e-6, "Mix 0 devuelve la entrada atrasada la latencia, exacta");
    }

    std::printf ("\nT3 Determinismo: tamaño de bloque y línea de tiempo\n");
    {
        EngineParams p; p.machine = kCassette; p.variant = 0; p.wowPct = 0.4; p.flutterPct = 0.2; p.wear = 0.6; p.dropouts = 0.7; p.noise = 0.6; p.scrape = 0.5;
        p.printDb = -45; p.nr = 1; p.mistrackDb = 3; p.bits = 10; p.rateHz = 22050; p.azimuth = 0.4; p.varispeed = -3; p.stopS = 0.4;
        auto x = noise (0.2, 6.0, fs, 21); auto y = sine (330, 0.3, 6.0, fs);
        auto hook = [] (int64_t pos, Engine& e) { (void) pos; (void) e; };
        std::vector<Render> outs;
        for (int b : { 1, 64, 512, 4096, -1 })
        {
            Engine e; e.prepare (fs, 4096); e.set (p);
            // Tape stop en posiciones fijas: se dispara cuando la posición de bloque pasa por ellas (bloques de 1 muestra
            // en la frontera, así el momento exacto es el mismo para todos los tamaños).
            Render o; o.l = x; o.r = y;
            for (int64_t i = 0; i < (int64_t) x.size();)
            {
                int bb = b > 0 ? b : 1 + (int) (hashUni (99, (uint64_t) i) * 700);
                const int64_t ev[2] = { (int64_t) (2.0 * fs), (int64_t) (3.0 * fs) };
                for (auto evp : ev) if (i < evp && i + bb > evp) bb = (int) (evp - i);
                bb = (int) std::min<int64_t> (bb, (int64_t) x.size() - i);
                if (i == ev[0]) { p.stop = true; e.set (p); }
                if (i == ev[1]) { p.stop = false; e.set (p); }
                e.process (o.l.data() + i, o.r.data() + i, bb, i);
                i += bb;
            }
            p.stop = false;
            outs.push_back (o);
        }
        bool same = true;
        for (size_t k = 1; k < outs.size(); ++k) same &= std::memcmp (outs[0].l.data(), outs[k].l.data(), outs[0].l.size() * 4) == 0 && std::memcmp (outs[0].r.data(), outs[k].r.data(), outs[0].r.size() * 4) == 0;
        check (same, "bloques de 1, 64, 512, 4096 y al azar: salida idéntica bit a bit (con tape stop, caídas, crush)");
        // Línea de tiempo: arrancar en 0 o en el segundo 3 da lo mismo desde el segundo 5.
        (void) hook;
        Engine a, b2; a.prepare (fs, 512); b2.prepare (fs, 512); a.set (p); b2.set (p);
        auto oa = render (a, x, y, 512, 0);
        const size_t off = (size_t) (3 * fs);
        std::vector<float> xb (x.begin() + (long) off, x.end()), yb (y.begin() + (long) off, y.end());
        auto ob = render (b2, xb, yb, 512, (int64_t) off);
        double diff = 0;
        for (size_t i = (size_t) (5 * fs); i < x.size(); ++i) diff = std::max (diff, (double) std::fabs (oa.l[i] - ob.l[i - off]));
        std::printf ("  diferencia máxima desde el segundo 5: %.2e\n", diff);
        check (diff < 1e-5, "con la posición del host, la reproducción da lo mismo empiece donde empiece");
    }

    std::printf ("\nT4 Wow y flutter medidos contra lo pedido\n");
    {
        for (int m : { kStudio, kCassette, kVhs, kVinyl })
            for (double set : { 0.05, 0.3, 1.5 })
            {
                Engine e; e.prepare (fs, 512); EngineParams p = clean (m, m == kVinyl ? 0 : 1);
                p.wowPct = set; p.flutterPct = 0; e.set (p);
                const double f0 = 3150; auto x = sine (f0, 0.1, 60.0, fs); auto y = x;
                auto o = render (e, x, y, 512);
                // Frecuencia instantánea por cruces por cero (interpolados), una lectura por ciclo.
                std::vector<double> dev; double last = -1;
                for (size_t i = (size_t) fs; i + 1 < o.l.size(); ++i)
                    if (o.l[i] <= 0 && o.l[i + 1] > 0)
                    {
                        const double t = (double) i + o.l[i] / (o.l[i] - o.l[i + 1]);
                        if (last > 0) dev.push_back ((fs / (t - last)) / f0 - 1.0);
                        last = t;
                    }
                // Ponderación en frecuencia sobre la serie (muestreada a f0), valor cuadrático medio por √2.
                size_t n = 1; while (n * 2 <= dev.size()) n <<= 1;
                std::vector<std::complex<double>> a (n);
                double mean = 0; for (size_t i = 0; i < n; ++i) mean += dev[i]; mean /= (double) n;
                for (size_t i = 0; i < n; ++i) a[i] = dev[i] - mean;
                fft (a);
                double acc = 0;
                for (size_t k = 1; k < n / 2; ++k) { const double f = (double) k * f0 / (double) n; const double w = flutterWeight (f); acc += 2.0 * std::norm (a[k]) * w * w; }
                const double meas = std::sqrt (2.0 * acc / ((double) n * (double) n)) * 100.0;
                std::printf ("  %-8s pedido %.2f %%  medido %.3f %%  (lectura de la interfaz %.3f %%)\n", profile (m, 1).name, set, meas, e.flutterReading());
                check (std::fabs (meas / set - 1.0) < 0.15, "dentro de ±15 %");
            }
    }

    std::printf ("\nT5 Histéresis\n");
    {
        Hysteresis j; j.set ({ 0, 0.5, 0, 1 }); j.reset();
        const int N = 2048; std::vector<double> m1 (N), m2 (N);
        for (int cyc = 0; cyc < 4; ++cyc)
            for (int i = 0; i < N; ++i) { const double y = j.process (0.9 * std::sin (kTwoPi * i / N)); (cyc == 2 ? m1 : m2)[(size_t) i] = cyc >= 2 ? j.lastM() : 0; (void) y; }
        double per = 0, sym = 0, mx = 0;
        for (int i = 0; i < N; ++i) { per = std::max (per, std::fabs (m1[(size_t) i] - m2[(size_t) i])); sym = std::max (sym, std::fabs (m1[(size_t) i] + m1[(size_t) ((i + N / 2) % N)])); mx = std::max (mx, std::fabs (m1[(size_t) i])); }
        std::printf ("  ciclo a ciclo %.1e, simetría impar %.1e, |m| máx %.3f\n", per, sym, mx);
        check (per < 2e-5, "estado periódico estable después de dos ciclos (±2e-5)");
        check (sym < 1e-4, "lazo simétrico (m(−h) = −m(h))");
        check (mx <= 1.0, "|M| ≤ M_s");
        auto area = [] (double drive, double bias) {
            Hysteresis h; h.set ({ drive, 0.5, bias, 1 }); h.reset(); const int n = 2048; double a = 0, hp = 0, mp = 0;
            for (int c = 0; c < 3; ++c) for (int i = 0; i < n; ++i) { h.process (0.5 * std::sin (kTwoPi * i / n)); if (c == 2) a += 0.5 * (h.lastM() + mp) * (h.lastH() - hp); hp = h.lastH(); mp = h.lastM(); }
            return std::fabs (a);
        };
        std::printf ("  área del lazo: drive 0 %.4f, +12 dB %.4f; bias −1 %.4f, +1 %.4f\n", area (0, 0), area (12, 0), area (0, -1), area (0, 1));
        check (area (12, 0) > area (0, 0), "el lazo crece con el drive");
        check (area (0, -1) > area (0, 0) && area (0, 1) < area (0, 0), "sub-bias abre el lazo, sobre-bias lo cierra");
        auto thd = [] (double bias, double amp) {
            Hysteresis h; h.set ({ 0, 0.5, bias, 1 }); h.reset(); const int sub = 192000, f = 1000, n = sub / f * 40; std::vector<double> y ((size_t) n);
            for (int i = 0; i < n; ++i) y[(size_t) i] = h.process (amp * std::sin (kTwoPi * f * i / sub));
            auto hk = [&] (int k) { double re = 0, im = 0; for (int i = n / 2; i < n; ++i) { re += y[(size_t) i] * std::cos (kTwoPi * k * f * i / sub); im += y[(size_t) i] * std::sin (kTwoPi * k * f * i / sub); } return std::sqrt (re * re + im * im); };
            double s = 0; for (int k = 2; k <= 9; ++k) s += hk (k) * hk (k);
            return 100.0 * std::sqrt (s) / hk (1);
        };
        std::printf ("  THD a −40 dBFS: nominal %.3f %%, sub-bias %.3f %%\n", thd (0, 0.01), thd (-1, 0.01));
        check (thd (0, 0.01) < 0.1, "casi lineal en nivel bajo con bias nominal (THD < 0,1 %)");
        check (thd (-1, 0.01) > 2 * thd (0, 0.01), "sub-bias: la distorsión de nivel bajo sube (la firma)");
    }

    std::printf ("\nT6 Aliasing de la histéresis a ×4\n");
    {
        for (double f : { 5000.0, 9000.0, 15000.0 })
        {
            Engine e; e.prepare (fs, 512); EngineParams p = clean (kStudio, 0); p.driveDb = 12; e.set (p);
            auto x = sine (f, 0.5, 2.5, fs); auto y = x;          // 1 s de asentamiento + 65536 muestras de análisis
            auto o = render (e, x, y, 512);
            const size_t n = 65536, a0 = (size_t) fs;
            std::vector<std::complex<double>> s (n);
            for (size_t i = 0; i < n; ++i) { const double w = 0.5 - 0.5 * std::cos (kTwoPi * (double) i / (double) n); s[i] = o.l[a0 + i] * w; }
            fft (s);
            double worst = -300, fund = 0;
            for (size_t k = 1; k < n / 2; ++k)
            {
                const double fk = (double) k * fs / (double) n, db = gainToDb (std::abs (s[k]) * 4.0 / (double) n);
                bool harm = false;
                for (int h = 1; h * f < fs / 2 + 1; ++h) if (std::fabs (fk - h * f) < 8.0) harm = true;
                if (std::fabs (fk - f) < 8.0) fund = std::max (fund, db);
                if (! harm && fk > 30.0) worst = std::max (worst, db);
            }
            std::printf ("  %5.0f Hz a +12 dB: fundamental %.1f dBFS, peor componente no armónica %.1f dBFS\n", f, fund, worst);
            // Hasta 9 kHz el sobremuestreo ×4 alcanza; un tono de 15 kHz a escala plena con +12 dB (y el énfasis de
            // 30 ips encima) genera armónicos que vuelven a la banda: se informa, no se exige.
            if (f < 10000.0) check (worst < -78.0, "aliasing bajo −78 dBFS");
            else std::printf ("  (dato: 15 kHz a escala plena es el caso extremo; se informa en el README)\n");
        }
    }

    std::printf ("\nT7 Respuesta de cada máquina (a −20 dB, sin ruido)\n");
    {
        auto resp = [&] (EngineParams p, double f, double amp = 0.0125) {     // −38 dBFS: 20 dB bajo la referencia
            Engine e; e.prepare (fs, 512); e.set (p);
            auto x = sine (f, amp, 1.0, fs); auto y = x; auto o = render (e, x, y, 512);
            return gainToDb (tone (o.l, f, fs, (size_t) (0.5 * fs), o.l.size()) / amp);
        };
        for (int m : { kStudio, kCassette, kVhs, kVinyl })
        {
            EngineParams p = clean (m, 1); if (m == kVinyl) p.variant = 0;
            const Profile pr = profile (m, p.variant);
            const double r1 = resp (p, 1000), rh = resp (p, pr.hfHz);
            std::printf ("  %-8s 1 kHz %+.2f dB, en el corte de agudos (%.0f Hz) %+.2f dB", pr.name, r1, pr.hfHz, rh - r1);
            if (pr.bumpHz > 0) std::printf (", golpe (%.0f Hz) %+.2f dB", pr.bumpHz, resp (p, pr.bumpHz) - r1);
            std::printf ("\n");
            check (std::fabs (r1) < 1.0, "1 kHz dentro de ±1 dB");
            check (m == kVinyl || std::fabs (rh - r1 + 3.0) < 1.2, "agudos: −3 dB en el corte del perfil (±1,2 dB)");
        }
        EngineParams pc = clean (kCassette, 0); pc.driveDb = 0;
        const double lo = resp (pc, 10000, 0.01) - resp (pc, 1000, 0.01), hi = resp (pc, 10000, 0.5) - resp (pc, 1000, 0.5);
        std::printf ("  casete: 10 kHz relativo a 1 kHz, nivel bajo %+.1f dB, nivel alto %+.1f dB\n", lo, hi);
        check (lo - hi > 6.0, "los agudos de la casete se cierran con el nivel (≥ 6 dB, §2.12)");
    }

    std::printf ("\nT8 Ruido\n");
    {
        for (int m : { kStudio, kCassette, kVhs })
        {
            Engine e; e.prepare (fs, 512); EngineParams p = clean (m, m == kStudio ? 1 : 0); p.noise = 0.5; p.driveDb = 0; e.set (p);
            std::vector<float> z ((size_t) (4 * fs)), z2 = z; auto o = render (e, z, z2, 512);
            const double lv = gainToDb (rms (o.l, (size_t) fs, o.l.size()) / (0.125 / std::sqrt (2.0)));
            std::printf ("  %-8s hiss %.1f dB respecto de la referencia (perfil %.0f dB)\n", profile (m, 0).name, lv, profile (m, m == kStudio ? 1 : 0).hissDb);
            check (std::fabs (lv - profile (m, m == kStudio ? 1 : 0).hissDb) < 1.0, "dentro de ±1 dB");
        }
        Engine e; e.prepare (fs, 512); EngineParams p = clean (kPhone, 0); p.driveDb = 0; p.noise = 0; e.set (p);
        auto x = sine (1000, dbToGain (-10), 2.0, fs); auto y = x; auto o = render (e, x, y, 512);
        const double t = tone (o.l, 1000, fs, (size_t) fs, o.l.size());
        double resid = 0; for (size_t i = (size_t) fs; i < o.l.size(); ++i) { const double ref = t * std::sin (kTwoPi * 1000 * (double) (i) / fs); (void) ref; }
        (void) resid;
        std::printf ("  teléfono: 1 kHz a −10 dBFS sale a %.1f dBFS\n", gainToDb (t));
    }

    std::printf ("\nT9 Estadística de eventos\n");
    {
        for (double sr : { 44100.0, 96000.0 })
        {
            Engine e; e.prepare (sr, 512); EngineParams p = clean (kCassette, 0); p.dropouts = 0.5; p.wear = 0.0; e.set (p);
            const int secs = 300; std::vector<float> z ((size_t) (sr * 10)), z2 = z;
            int count = 0; UiEvent ev[64];
            for (int k = 0; k < secs / 10; ++k) { auto zz = z, zz2 = z2; e.process (zz.data(), zz2.data(), (int) zz.size(), (int64_t) k * (int64_t) zz.size()); count += e.takeEvents (ev, 64); }
            std::printf ("  %.1f kHz: %d caídas en %d s (%.2f por segundo)\n", sr / 1000, count, secs, count / (double) secs);
        }
        // Las rayas del vinilo se repiten exactamente una vez por vuelta.
        Engine e; e.prepare (fs, 512); EngineParams p = clean (kVinyl, 0); p.scratches = 1; e.set (p);
        std::vector<float> z ((size_t) (8 * fs)), z2 = z; e.process (z.data(), z2.data(), (int) z.size(), 0);
        UiEvent ev[64]; const int n = e.takeEvents (ev, 64);
        std::vector<int64_t> ps; for (int i = 0; i < n; ++i) if (ev[i].kind == 2) ps.push_back (ev[i].pos);
        bool periodic = ps.size() >= 3;
        const double period = 60.0 / 33.333 * fs;
        for (size_t i = 1; i < ps.size(); ++i) periodic &= std::fabs ((double) (ps[i] - ps[i - 1]) - period) <= 1.0;
        std::printf ("  raya: %zu pops en 8 s, cada %.1f muestras (una vuelta = %.1f)\n", ps.size(), ps.size() > 1 ? (double) (ps.back() - ps.front()) / (double) (ps.size() - 1) : 0.0, period);
        check (periodic, "la raya suena una vez por vuelta (±1 muestra)");
    }

    std::printf ("\nT10 Tape stop\n");
    {
        Engine e; e.prepare (fs, 512); EngineParams p = clean (kStudio, 1); p.stopS = 0.5; p.stopCurve = 0.0; e.set (p);
        auto x = sine (1000, 0.25, 4.0, fs); auto y = x; Render o; o.l = x; o.r = y;
        const int64_t t0 = (int64_t) fs, t1 = (int64_t) (2.0 * fs);
        int64_t zero = -1; double midHz = 0;
        for (int64_t i = 0; i < (int64_t) x.size(); ++i)
        {
            if (i == t0) { p.stop = true; e.set (p); }
            if (i == t1) { p.stop = false; e.set (p); }
            e.process (o.l.data() + i, o.r.data() + i, 1, i);
            if (zero < 0 && i > t0 && e.speed() <= 0.0) zero = i;
        }
        const double ms = zero > 0 ? (double) (zero - t0 + 1) / fs * 1000.0 : -1;
        std::printf ("  pedido 500 ms, la velocidad llega a cero en %.2f ms\n", ms);
        check (std::fabs (ms - 500.0) < 1.0, "el stop dura lo pedido (±1 ms)");
        // A mitad del stop lineal la velocidad es 0,5: el seno de 1 kHz sale a 500 Hz (cruces por cero).
        {
            const size_t a = (size_t) (t0 + (int64_t) (0.24 * fs)), b = a + (size_t) (0.02 * fs);     // la velocidad actúa en la cabeza: sin latencia
            int zc = 0; double first = -1, last = -1;
            for (size_t i = a; i < b; ++i) if (o.l[i] <= 0 && o.l[i + 1] > 0) { const double t = (double) i + o.l[i] / (o.l[i] - o.l[i + 1]); if (first < 0) first = t; last = t; ++zc; }
            midHz = zc > 1 ? (zc - 1) * fs / (last - first) : 0;
        }
        std::printf ("  a los 250 ms la frecuencia es %.0f Hz (esperado ≈ 500)\n", midHz);
        check (std::fabs (midHz - 500.0) < 30.0, "la afinación sigue a la velocidad");
        double jump = 0; for (size_t i = (size_t) (t1 + e.latency()); i < (size_t) (t1 + e.latency() + 0.5 * fs); ++i) jump = std::max (jump, (double) std::fabs (o.l[i] - o.l[i - 1]));
        std::printf ("  arranque: salto máximo entre muestras %.4f (un seno de 1 kHz a 0,25 salta hasta %.4f)\n", jump, 0.25 * kTwoPi * 1000 / fs);
        check (jump < 0.25 * kTwoPi * 1000 / fs * 1.5, "el arranque no hace clic");
        const double tl = tone (o.l, 1000, fs, (size_t) (3.2 * fs), (size_t) (3.9 * fs));
        std::printf ("  después del arranque: 1 kHz a %.2f dB\n", gainToDb (tl / 0.25));
        check (std::fabs (gainToDb (tl / 0.25)) < 1.5, "después del arranque, el nivel vuelve");
    }

    std::printf ("\nT12 Independencia de la frecuencia de muestreo\n");
    {
        double r1[3], r10[3], wf[3]; int k = 0;
        for (double sr : { 44100.0, 48000.0, 96000.0 })
        {
            auto meas = [&] (double f) {
                Engine e; e.prepare (sr, 512); EngineParams p = clean (kCassette, 1); e.set (p);
                auto x = sine (f, 0.0125, 1.0, sr); auto y = x; auto o = render (e, x, y, 512);
                return gainToDb (tone (o.l, f, sr, (size_t) (0.5 * sr), o.l.size()) / 0.0125);
            };
            r1[k] = meas (1000); r10[k] = meas (10000);
            Engine e; e.prepare (sr, 512); EngineParams p = clean (kCassette, 1); p.wowPct = 0.2; e.set (p);
            std::vector<float> z ((size_t) (6 * sr)), z2 = z; e.process (z.data(), z2.data(), (int) z.size(), 0);
            wf[k] = e.flutterReading();
            std::printf ("  %.1f kHz: 1 kHz %+.2f dB, 10 kHz %+.2f dB, lectura W&F %.3f %%\n", sr / 1000, r1[k], r10[k], wf[k]);
            ++k;
        }
        check (std::fabs (r10[0] - r10[2]) < 0.6 && std::fabs (r1[0] - r1[2]) < 0.3, "respuesta igual a 44,1 y 96 kHz (±0,3 dB a 1 kHz, ±0,6 dB a 10 kHz)");
        check (std::fabs (wf[0] / wf[2] - 1) < 0.08, "wow y flutter iguales (±8 %)");
    }

    std::printf ("\nT16 CPU (48 kHz, bloques de 512, estéreo)\n");
    {
        auto bench = [&] (EngineParams p, const char* what) {
            Engine e; e.prepare (fs, 512); e.set (p);
            auto x = noise (0.2, 10.0, fs, 31); auto y = x;
            const auto t0 = std::chrono::steady_clock::now();
            render (e, x, y, 512);
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  %-44s %.2f %% de un núcleo\n", what, 100.0 * secs / 10.0);
            return 100.0 * secs / 10.0;
        };
        EngineParams d; bench (d, "estudio, valores por defecto");
        EngineParams c; c.machine = kCassette; c.variant = 0; c.wowPct = 0.25; c.wear = 0.45; c.dropouts = 0.3; c.azimuth = 0.25; c.nr = 1; bench (c, "casete gastada con NR");
        EngineParams w = c; w.bits = 8; w.rateHz = 11025; w.varispeed = -5; w.printDb = -45; w.driveDb = 12; w.stereoWobble = true;
        bench (w, "todo prendido (casete, crush, varispeed, copia)");
        EngineParams v; v.machine = kVinyl; v.crackle = 0.6; v.scratches = 2; bench (v, "vinilo con crujido y rayas");
        EngineParams r; r.machine = kRadio; r.crackle = 0.5; r.fading = 0.5; bench (r, "radio con estática y desvanecimiento");
    }

    const double total = std::chrono::duration<double> (std::chrono::steady_clock::now() - start).count();
    std::printf ("\n%s (%d fallas) en %.0f s\n", fails ? "HAY FALLAS" : "TODO OK", fails, total);
    return fails ? 1 : 0;
}
