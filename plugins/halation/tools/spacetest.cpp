// Pruebas del motor de espacio (sin JUCE). Se agregan a medida que entra cada módulo.
//   clang++ -std=c++20 -O2 -ffp-contract=off -I plugins/space/Source/dsp plugins/space/tools/spacetest.cpp -o /tmp/sp_test && /tmp/sp_test
#include "Engine.h"
#include <chrono>
#include <cstring>
#include <cstdio>
#include <string>

using namespace dh::sp;
static int fails = 0;
static void check (bool ok, const std::string& what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what.c_str()); if (! ok) ++fails; }

// Respuesta al impulso (L) de la red con los parámetros dados.
static std::vector<float> ir (const NetParams& p, float fs, float secs)
{
    Network n; n.prepare (fs); n.set (p);
    std::vector<float> out ((size_t) (secs * fs));
    for (size_t i = 0; i < out.size(); ++i) { float l, r; const float x = i == 0 ? 0.5f : 0.0f; n.process (x, x, l, r); out[i] = 0.5f * (l + r); }
    return out;
}
// T60 por integración de Schroeder (pendiente entre −5 y −25 dB, extrapolada a −60).
static double t60 (const std::vector<float>& h, float fs)
{
    std::vector<double> e (h.size()); double acc = 0;
    for (size_t i = h.size(); i-- > 0;) { acc += (double) h[i] * h[i]; e[i] = acc; }
    const double e0 = e[0]; size_t i5 = 0, i25 = 0;
    for (size_t i = 0; i < e.size(); ++i) { const double db = 10 * std::log10 (e[i] / e0 + 1e-30); if (! i5 && db < -5) i5 = i; if (! i25 && db < -25) { i25 = i; break; } }
    return i25 > i5 ? 3.0 * (double) (i25 - i5) / fs : 0.0;
}

int main()
{
    const float fs = 48000.0f;
    std::printf ("Red: delay\n");
    {
        NetParams p; p.space = 0.0f; p.timeMs = 250.0f; p.decayS = 3.0f * 0.25f / std::log10 (2.0f); p.hfRatio = 1.0f; p.pingPong = false; p.modDepth = 0.0f; p.lowCutHz = 5.0f;   // T60 con el que cada vuelta es la mitad
        const auto h = ir (p, fs, 1.2f);
        // Primer eco a 250 ms (+2 muestras de lectura), el segundo a 500 ms con la mitad de amplitud.
        auto peakNear = [&] (float ms) { size_t c = (size_t) (ms * 1e-3f * fs); float m = 0; size_t at = 0; for (size_t i = c - 200; i < c + 200; ++i) if (std::fabs (h[i]) > m) { m = std::fabs (h[i]); at = i; } return std::make_pair (at, m); };
        const auto e1 = peakNear (250.0f), e2 = peakNear (500.0f);
        std::printf ("  eco 1 en %.2f ms (%.3f), eco 2 en %.2f ms (%.3f)\n", e1.first * 1e3 / fs, e1.second, e2.first * 1e3 / fs, e2.second);
        check (std::fabs ((double) e1.first * 1e3 / fs - 250.0) < 0.1, "el primer eco cae en el tiempo pedido");
        check (std::fabs (e2.second / e1.second - 0.5f) < 0.03f, "cada vuelta pierde lo que dice el T60 (la mitad)");
    }
    std::printf ("\nRed: reverb, T60 medido contra el pedido\n");
    for (float T : { 0.8f, 2.0f, 6.0f })
    {
        NetParams p; p.space = 1.0f; p.decayS = T; p.hfRatio = 1.0f; p.lowCutHz = 5.0f; p.modDepth = 0.0f; p.size = 1.0f;
        auto h = ir (p, fs, T * 1.5f + 1.0f);
        // Banda media (500 Hz-2 kHz, dos pasa altos y dos pasa bajos de un polo), como se mide el T60 en acústica.
        float ah = std::exp (-2 * kPi * 500 / fs), al = std::exp (-2 * kPi * 2000 / fs), h1 = 0, h2 = 0, x1 = 0, x2 = 0, l1 = 0, l2 = 0;
        for (auto& v : h) { const float a1 = v - x1 + ah * h1; x1 = v; h1 = a1; const float a2 = a1 - x2 + ah * h2; x2 = a1; h2 = a2; l1 = a2 + al * (l1 - a2); l2 = l1 + al * (l2 - l1); v = l2; }
        const double m = t60 (h, fs);
        std::printf ("  T60 pedido %.1f s, medido %.2f s (banda media)\n", T, m);
        check (std::fabs (m - T) / T < 0.10, "T60 dentro de ±10 %");
    }
    std::printf ("\nCongelado: no crece ni se apaga\n");
    {
        NetParams p; p.space = 1.0f; p.decayS = 2.0f; p.modDepth = 0.2f;
        Network n; n.prepare (fs); n.set (p);
        float l, r; uint32_t s = 1;
        for (int i = 0; i < (int) fs; ++i) { s ^= s << 13; s ^= s >> 17; s ^= s << 5; const float x = 0.3f * ((float) (int32_t) s / 2147483648.0f); n.process (x, x, l, r); }
        p.freezeMask = 7; n.set (p);
        auto rms = [&] (int samples) { double a = 0; for (int i = 0; i < samples; ++i) { n.process (0.5f, -0.5f, l, r); a += l * l + r * r; } return std::sqrt (a / (2.0 * samples)); };
        rms ((int) (0.5f * fs));
        const double r1 = rms ((int) fs), r2 = rms ((int) (10 * fs));
        std::printf ("  nivel congelado: %.4f después de 1 s, %.4f después de 11 s (%.2f dB)\n", r1, r2, 20 * std::log10 (r2 / r1));
        check (std::fabs (20 * std::log10 (r2 / r1)) < 1.5, "el congelado sostiene el nivel (±1.5 dB en 10 s, con la entrada sonando)");
    }
    std::printf ("\nRealimentación excesiva: el limitador la contiene\n");
    {
        NetParams p; p.space = 0.3f; p.decayS = 1000.0f; p.timeMs = 120.0f;
        Network n; n.prepare (fs); n.set (p);
        float l, r, mx = 0; bool finite = true;
        for (int i = 0; i < (int) (20 * fs); ++i) { const float x = i < 4800 ? 0.9f * std::sin (0.05f * i) : 0.0f; n.process (x, x, l, r); mx = std::max (mx, std::max (std::fabs (l), std::fabs (r))); finite = finite && std::isfinite (l) && std::isfinite (r); }
        std::printf ("  pico de salida en 20 s: %.3f, reducción del limitador %.1f dB\n", mx, n.limiterGrDb());
        check (finite && mx < 3.0f, "sin NaN y acotado");
    }

    // ── Motor completo ──
    auto noiseSig = [&] (size_t n, uint32_t seed, std::vector<float>& L, std::vector<float>& R) {
        L.resize (n); R.resize (n); Rand r (seed); float lp = 0;
        for (size_t i = 0; i < n; ++i) { lp += 0.2f * (r.bi() - lp); const float env = (i / 12000) % 2 ? 0.0f : 0.5f; L[i] = env * lp; R[i] = env * (lp * 0.8f + 0.1f * r.bi()); }
    };
    auto allOn = [] { EngineParams p; p.mix = 0.5f; p.net.space = 0.6f; p.net.character = Character::tape; p.net.wear = 0.5f; p.shimmer = 0.5f; p.interval = 4;
                      p.grains = 0.5f; p.reverse = 0.4f; p.spectral = 0.5f; p.resonance = 0.4f; p.nNotes = 3; p.duck = 0.3f; p.gate = 0.3f; p.stutter = 0.3f;
                      p.impossible = 2; p.net.decayS = 8.0f; return p; };
    std::printf ("\nMotor: ruta seca sin latencia\n");
    {
        Engine e; e.prepare (fs, 512); EngineParams p; p.mix = 0.0f; e.set (p);
        std::vector<float> L, R; noiseSig (48000, 3, L, R); auto L0 = L, R0 = R;
        for (size_t pos = 0; pos < L.size(); pos += 512) e.process (L.data() + pos, R.data() + pos, (int) std::min ((size_t) 512, L.size() - pos));
        double err = 0; for (size_t i = 0; i < L.size(); ++i) err = std::max (err, (double) std::fabs (L[i] - L0[i]));
        std::printf ("  mezcla 0: diferencia máxima %.2e\n", err);
        check (err < 1e-6, "con mezcla 0 la salida es la entrada, sin retardo");
    }
    std::printf ("\nMotor: determinismo con distintos bloques (todo encendido)\n");
    {
        std::vector<float> L, R; noiseSig ((size_t) (6 * fs), 5, L, R);
        auto run = [&] (int block) { Engine e; e.prepare (fs, 4096); e.set (allOn()); auto a = L, b = R; uint32_t s = 99;
            for (size_t pos = 0; pos < a.size();) { size_t k = block > 0 ? (size_t) block : 1 + ((s ^= s << 13, s ^= s >> 17, s ^= s << 5) % 2000u); k = std::min (k, a.size() - pos);
                e.process (a.data() + pos, b.data() + pos, (int) k); pos += k; } return std::make_pair (a, b); };
        const auto ref = run (512);
        bool same = true;
        for (int b : { 64, 4096, -1 }) { const auto o = run (b); const bool eq = std::memcmp (o.first.data(), ref.first.data(), ref.first.size() * 4) == 0; std::printf ("  bloque %s: %s\n", b < 0 ? "aleatorio" : std::to_string (b).c_str(), eq ? "idéntico" : "DISTINTO"); same = same && eq; }
        check (same, "salida idéntica bit a bit");
    }
    std::printf ("\nMotor: todo encendido, 30 s de ruido y silencios\n");
    {
        Engine e; e.prepare (fs, 512); auto p = allOn(); e.set (p);
        std::vector<float> L, R; noiseSig ((size_t) (30 * fs), 7, L, R);
        bool fin = true; float mx = 0;
        for (size_t pos = 0; pos < L.size(); pos += 512)
        {
            if (pos == (size_t) (12 * fs)) { p.net.freezeMask = 7; p.spectralFreeze = 7; e.set (p); }
            if (pos == (size_t) (20 * fs)) { p.net.freezeMask = 0; p.spectralFreeze = 0; p.net.space = 1.0f; p.net.decayS = 200.0f; e.set (p); }
            e.process (L.data() + pos, R.data() + pos, 512);
        }
        for (size_t i = 0; i < L.size(); ++i) { fin = fin && std::isfinite (L[i]) && std::isfinite (R[i]); mx = std::max (mx, std::max (std::fabs (L[i]), std::fabs (R[i]))); }
        std::printf ("  pico %.3f, finito %s\n", mx, fin ? "sí" : "NO");
        check (fin && mx <= 0.8913f, "sin NaN y bajo −1 dBFS (techo de salida)");
    }
    std::printf ("\nShimmer: el transpositor da el intervalo\n");
    {
        for (float ratio : { 2.0f, 1.5f })
        {
            PitchShifter ps; ps.prepare (fs); ps.setRatio (ratio);
            const int n = 1 << 15; std::vector<std::complex<float>> X (n); float ph = 0;
            for (int i = 0; i < n + 8000; ++i) { ph += 2 * kPi * 440 / fs; const float y = ps.process (0.5f * std::sin (ph)); if (i >= 8000) X[(size_t) (i - 8000)] = { y * (0.5f - 0.5f * std::cos (2 * kPi * (i - 8000) / n)), 0 }; }
            // DFT por bins cerca del esperado (barata): el pico debe estar en 440·ratio.
            float best = 0, bestHz = 0;
            for (float hz = 300; hz < 1200; hz += 1.0f) { std::complex<double> acc = 0; for (int i = 0; i < n; i += 4) { const double th = -2 * kPiD * hz * i / fs; acc += (double) X[(size_t) i].real() * std::complex<double> (std::cos (th), std::sin (th)); } if (std::abs (acc) > best) { best = (float) std::abs (acc); bestHz = hz; } }
            std::printf ("  razón %.2f: pico en %.0f Hz (esperado %.0f)\n", ratio, bestHz, 440 * ratio);
            check (std::fabs (bestHz - 440 * ratio) < 0.01f * 440 * ratio, "intervalo afinado (±1 %, la ventana del grano agrega bandas laterales)");
        }
    }
    std::printf ("\nEspectral: el congelado sostiene\n");
    {
        Spectral sp; sp.prepare (fs, 3);
        Rand r (5); double a1 = 0, a2 = 0;
        for (int i = 0; i < (int) fs; ++i) sp.process (0.3f * r.bi(), 0.5f, 0);
        for (int i = 0; i < (int) (4 * fs); ++i) { const float y = sp.process (0.0f, 0.5f, 7); if (i > fs && i < 2 * fs) a1 += y * y; if (i > 3 * fs) a2 += y * y; }
        std::printf ("  nivel congelado: %.2f dB entre el segundo 1 y el 3\n", 10 * std::log10 (a2 / a1));
        check (a1 > 0 && std::fabs (10 * std::log10 (a2 / a1)) < 1.0, "sostiene ±1 dB sin entrada");
    }

    std::printf ("\nMatriz: ortogonal para cualquier ángulo\n");
    {
        double worst = 0;
        for (float th : { 0.0f, 0.2f, 0.5f, 0.7853982f })
        {
            ButterflyMatrix<8> m; m.setAngle (th);
            double M[8][8];
            for (int j = 0; j < 8; ++j) { std::array<float, 8> e {}; e[(size_t) j] = 1.0f; m.apply (e); for (int i = 0; i < 8; ++i) M[i][j] = e[(size_t) i]; }
            for (int a = 0; a < 8; ++a) for (int b = 0; b < 8; ++b) { double d = 0; for (int i = 0; i < 8; ++i) d += M[i][a] * M[i][b]; worst = std::max (worst, std::fabs (d - (a == b ? 1.0 : 0.0))); }
        }
        std::printf ("  desvío máximo de MᵀM respecto de la identidad: %.2e\n", worst);
        check (worst < 1e-6, "MᵀM = I (±1e-6)");
    }
    std::printf ("\nCongelado por banda: los graves se sostienen, los agudos siguen su caída\n");
    {
        Engine e; e.prepare (fs, 512); EngineParams p; p.mix = 1.0f; p.net.space = 1.0f; p.net.decayS = 1.5f; p.net.modDepth = 0.0f; e.set (p);
        std::vector<float> L (512), R (512); Rand r (21);
        for (int b = 0; b < (int) fs / 512; ++b) { for (int i = 0; i < 512; ++i) L[(size_t) i] = R[(size_t) i] = 0.3f * r.bi(); e.process (L.data(), R.data(), 512); }
        p.spectralFreeze = 1; e.set (p);
        auto bands = [&] (int secs, double& lo, double& hi) {
            float a = std::exp (-2 * kPi * 120 / fs), b2 = std::exp (-2 * kPi * 4000 / fs), zl = 0, zh = 0; lo = hi = 0;
            for (int blk = 0; blk < secs * (int) fs / 512; ++blk)
            {
                std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); e.process (L.data(), R.data(), 512);
                if (blk >= (secs - 1) * (int) fs / 512) for (float v : L) { zl = v + a * (zl - v); zh = v + b2 * (zh - v); lo += zl * zl; hi += (v - zh) * (v - zh); }
            }
        };
        double lo1, hi1, lo2, hi2; bands (1, lo1, hi1); bands (3, lo2, hi2);
        const double dLo = 10 * std::log10 (lo2 / lo1), dHi = 10 * std::log10 (hi2 / hi1 + 1e-30);
        std::printf ("  graves %.1f dB, agudos %.1f dB en 3 s\n", dLo, dHi);
        check (std::fabs (dLo) < 2.0 && dHi < -20.0, "graves sostenidos (±2 dB), agudos caídos (< −20 dB)");
    }
    std::printf ("\nCola afinada: la red suena en la nota\n");
    {
        NetParams p; p.space = 1.0f; p.decayS = 6.0f; p.modDepth = 0.0f; p.nTune = 1; p.tuneHz[0] = 220.0f; p.tuneAmount = 1.0f; p.hfRatio = 0.3f;
        const auto h = ir (p, fs, 3.0f);
        // Autocorrelación de la cola (de 0.5 a 2.5 s): el pico debe estar en el período de 220 Hz (o un múltiplo).
        const int a0 = (int) (0.5f * fs), n = (int) (2.0f * fs);
        double best = -1; int lag = 0;
        for (int L = (int) (fs / 500); L < (int) (fs / 100); ++L) { double c = 0; for (int i = a0; i < a0 + n; i += 2) c += h[(size_t) i] * h[(size_t) (i + L)]; if (c > best) { best = c; lag = L; } }
        const double hz = fs / lag;
        double cents = std::fmod (1200 * std::log2 (hz / 220.0), 1200.0); if (cents > 600) cents -= 1200; if (cents < -600) cents += 1200;
        std::printf ("  período dominante %.2f Hz (%.1f cents de 220 Hz, módulo octava)\n", hz, cents);
        check (std::fabs (cents) < 15.0, "la cola suena en la nota pedida (±15 cents, módulo octava)");
    }
    std::printf ("\n200 configuraciones extremas al azar\n");
    {
        Rand r (31); bool ok = true; float worst = 0;
        for (int k = 0; k < 200 && ok; ++k)
        {
            EngineParams p;
            p.net.space = r.uni(); p.net.timeMs = 1 + 3999 * r.uni(); p.net.size = 0.25f + 3.75f * r.uni(); p.net.decayS = r.uni() < 0.2f ? 1000.0f : 0.2f + 60 * r.uni();
            p.net.hfRatio = r.uni(); p.net.lfRatio = 0.3f + 1.7f * r.uni(); p.net.lowCutHz = 20 + 780 * r.uni(); p.net.diffusion = r.uni(); p.net.modDepth = r.uni(); p.net.modRate = 6 * r.uni();
            p.net.character = (Character) (int) (4 * r.uni()); p.net.room = (RoomType) (int) (4 * r.uni()); p.net.pattern = (int) (8 * r.uni()); p.net.wear = r.uni();
            p.net.freezeMask = r.uni() < 0.2f ? (int) (8 * r.uni()) : 0; p.net.width = 1.5f * r.uni(); p.net.elastic = r.uni() < 0.5f ? 1.0f : 0.0f;
            p.mix = r.uni(); p.predelayMs = 250 * r.uni(); p.shimmer = r.uni() < 0.3f ? r.uni() : 0; p.interval = (int) (7 * r.uni());
            p.grains = r.uni() < 0.3f ? r.uni() : 0; p.reverse = r.uni() < 0.3f ? r.uni() : 0; p.spectral = r.uni() < 0.2f ? r.uni() : 0;
            p.resonance = r.uni() < 0.3f ? r.uni() : 0; p.nNotes = (int) (5 * r.uni()); p.duck = r.uni() < 0.3f ? r.uni() : 0; p.gate = r.uni() < 0.3f ? r.uni() : 0;
            p.stutter = r.uni() < 0.3f ? r.uni() : 0; p.impossible = (int) (3 * r.uni()); p.impDepth = r.uni(); p.impRate = r.uni(); p.outGainDb = -24 + 36 * r.uni();
            Engine e; e.prepare (fs, 512); e.set (p);
            std::vector<float> L ((size_t) (fs / 2)), R ((size_t) (fs / 2)); Rand x (k + 1);
            for (int rep = 0; rep < 4; ++rep)
            {
                for (size_t i = 0; i < L.size(); ++i) { const float v = rep < 2 ? (r.uni() < 0.5f ? x.bi() : (i % 200 == 0 ? 1.0f : 0.0f)) : 0.0f; L[i] = v; R[i] = -v; }
                for (size_t pos = 0; pos < L.size(); pos += 512) e.process (L.data() + pos, R.data() + pos, (int) std::min ((size_t) 512, L.size() - pos));
                for (size_t i = 0; i < L.size(); ++i) { if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) ok = false; worst = std::max (worst, std::max (std::fabs (L[i]), std::fabs (R[i]))); }
            }
        }
        // Con la ganancia de salida en hasta +12 dB la salida puede superar 1: lo que importa es que sea finita y acotada.
        std::printf ("  pico máximo %.2f (con salida hasta +12 dB y entrada a escala plena)\n", worst);
        check (ok && worst < 8.0f, "sin NaN/Inf, salida acotada");
    }
    std::printf ("\nCPU (48 kHz, bloques de 512)\n");
    {
        std::vector<float> L, R; noiseSig ((size_t) (20 * fs), 9, L, R);
        for (int mode = 0; mode < 3; ++mode)
        {
            Engine e; e.prepare (fs, 512);
            EngineParams p; if (mode == 1) { p.net.space = 1.0f; p.net.character = Character::tape; } if (mode == 2) p = allOn();
            e.set (p);
            auto a = L, b = R;
            const auto t0 = std::chrono::steady_clock::now();
            for (size_t pos = 0; pos + 512 <= a.size(); pos += 512) e.process (a.data() + pos, b.data() + pos, 512);
            const double s = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  %-30s %.2f %% de un núcleo\n", mode == 0 ? "delay digital" : mode == 1 ? "reverb con cinta" : "todo encendido", 100 * s / 20.0);
        }
    }
    std::printf (fails ? "\n%d FALLAS\n" : "\nTODO OK (0 fallas)\n", fails);
    return fails ? 1 : 0;
}
