// Pruebas del motor del ATMD sin JUCE:
//   clang++ -std=c++20 -O2 -I plugins/atmd/Source/dsp -I shared/dsp plugins/atmd/tools/dsptest.cpp -o /tmp/atmd_dsptest && /tmp/atmd_dsptest
#include "EngineMd.h"
#include <chrono>
#include <cstdio>

using namespace dh::md;

static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what); if (! ok) ++fails; }

struct Out { std::vector<float> L, R; double cpu = 0; };

static Out run (Engine& e, double secs, double sr = 48000.0, int block = 256)
{
    Out o; std::vector<float> l ((size_t) block), r ((size_t) block);
    const int total = (int) (secs * sr);
    const auto t0 = std::chrono::steady_clock::now();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        e.process (nullptr, l.data(), r.data(), n);
        o.L.insert (o.L.end(), l.begin(), l.begin() + n); o.R.insert (o.R.end(), r.begin(), r.begin() + n);
    }
    o.cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / secs * 100.0;
    return o;
}
static float rms (const std::vector<float>& v, double a, double b, double sr = 48000.0)
{
    double s = 0; size_t n = 0;
    for (size_t i = (size_t) (a * sr); i < (size_t) (b * sr) && i < v.size(); ++i) { s += (double) v[i] * v[i]; ++n; }
    return n ? (float) std::sqrt (s / (double) n) : 0.0f;
}
static float peak (const std::vector<float>& v) { float p = 0; for (float x : v) p = std::max (p, std::fabs (x)); return p; }
static bool finite (const Out& o) { for (float x : o.L) if (! dh::isFiniteF (x)) return false; for (float x : o.R) if (! dh::isFiniteF (x)) return false; return true; }
// Frecuencia fundamental por autocorrelación normalizada (robusta para formas con armónicos).
static float f0 (const std::vector<float>& v, double a, double b, double sr = 48000.0, float fmin = 20.0f, float fmax = 4000.0f)
{
    const size_t i0 = (size_t) (a * sr), n = (size_t) ((b - a) * sr);
    const int lagMin = (int) (sr / fmax), lagMax = (int) (sr / fmin);
    double best = -1; int bestLag = 0;
    std::vector<double> c ((size_t) lagMax + 2, 0.0);
    for (int lag = lagMin; lag <= lagMax; ++lag)
    {
        double s = 0, e0 = 0, e1 = 0;
        for (size_t i = 0; i + (size_t) lag < n; ++i) { const double x = v[i0 + i], y = v[i0 + i + (size_t) lag]; s += x * y; e0 += x * x; e1 += y * y; }
        c[(size_t) lag] = s / std::sqrt (e0 * e1 + 1e-12);
    }
    // El primer máximo que llega cerca del máximo global (evita elegir el doble del período).
    for (int lag = lagMin; lag <= lagMax; ++lag) best = std::max (best, c[(size_t) lag]);
    for (int lag = lagMin + 1; lag < lagMax; ++lag)
        if (c[(size_t) lag] > 0.9 * best && c[(size_t) lag] >= c[(size_t) lag - 1] && c[(size_t) lag] >= c[(size_t) lag + 1]) { bestLag = lag; break; }
    if (! bestLag) return 0.0f;
    // Interpolación parabólica del pico.
    const double y0 = c[(size_t) bestLag - 1], y1 = c[(size_t) bestLag], y2 = c[(size_t) bestLag + 1];
    const double d = 0.5 * (y0 - y2) / (y0 - 2 * y1 + y2 + 1e-12);
    return (float) (sr / (bestLag + d));
}
static float cents (float f, float ref) { return 1200.0f * std::log2 (f / ref); }

static Engine fresh (int os = 2, double sr = 48000.0)
{
    Engine e; e.prepare (sr, 512, os);
    e.p.cutoff = 5.0f; e.p.emphasis = 0.0f; e.p.contourAmt = 0.0f; e.p.lS = 10.0f; e.p.lA = 0.0f; e.p.volume = 7.0f;
    return e;
}

int main()
{
    std::printf ("1. Afinación: la sierra de 8' suena en la nota MIDI; 32' dos octavas abajo, 2' dos arriba\n");
    {
        for (auto [range, note, hz] : { std::tuple { 3, 69, 440.0f }, std::tuple { 1, 69, 110.0f }, std::tuple { 5, 57, 880.0f }, std::tuple { 3, 45, 110.0f } })
        {
            Engine e = fresh(); e.p.range[0] = range; e.noteOn (note, 0.8f);
            auto o = run (e, 0.6);
            const float f = f0 (o.L, 0.2, 0.5);
            std::printf ("  rango %d nota %d: %.2f Hz (esperado %.1f, %+.1f cents)\n", range, note, f, hz, cents (f, hz));
            check (std::fabs (cents (f, hz)) < 3.0f, "afinación dentro de 3 cents");
        }
    }

    std::printf ("2. Frecuencia del osc 2 (+7 semitonos) y tune\n");
    {
        Engine e = fresh(); e.p.on[0] = false; e.p.on[1] = true; e.p.level[1] = 10.0f; e.p.freq2 = 7.0f; e.noteOn (57, 0.8f);
        auto o = run (e, 0.6); const float f = f0 (o.L, 0.2, 0.5);
        std::printf ("  A3 + 7 st: %.2f Hz (esperado 329.6)\n", f);
        check (std::fabs (cents (f, 329.63f)) < 3.0f, "una quinta arriba");
    }

    std::printf ("3. Autooscilación del filtro: senoidal que sigue al teclado con KEYBOARD CONTROL 1 y 2\n");
    {
        float fr[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e = fresh(); e.p.on[0] = false; e.p.emphasis = 10.0f; e.p.cutoff = 0.0f; e.p.kbd1 = e.p.kbd2 = true;
            e.noteOn (k == 0 ? 60 : 72, 0.8f);
            auto o = run (e, 1.2); fr[k] = f0 (o.L, 0.6, 1.1, 48000.0, 50.0f, 8000.0f);
            std::printf ("  nota %d: %.1f Hz, nivel %.3f\n", k == 0 ? 60 : 72, fr[k], rms (o.L, 0.6, 1.1));
            check (rms (o.L, 0.6, 1.1) > 0.02f, "oscila sola con el mezclador cerrado");
        }
        std::printf ("  relación %.3f (esperado 2.0)\n", fr[1] / fr[0]);
        check (std::fabs (cents (fr[1] / fr[0], 2.0f)) < 25.0f, "una octava de teclado = una octava de filtro (±25 cents)");
    }

    std::printf ("4. La resonancia adelgaza los graves (pérdida en la banda de paso)\n");
    {
        float lv[2];
        for (int k = 0; k < 2; ++k)
        {
            Engine e = fresh(); e.p.cutoff = 2.0f; e.p.emphasis = k ? 8.0f : 0.0f; e.p.level[0] = 3.0f; e.p.wave[0] = Triangle; e.noteOn (36, 0.8f);
            auto o = run (e, 0.6); lv[k] = rms (o.L, 0.2, 0.5);
        }
        std::printf ("  triángulo grave: %.3f sin resonancia, %.3f con 8\n", lv[0], lv[1]);
        check (lv[1] < 0.8f * lv[0], "con resonancia alta baja el nivel de la fundamental");
    }

    std::printf ("5. Contornos: DECAY apagado cae con τ = 15 ms; encendido, con el tiempo de DECAY\n");
    {
        for (int k = 0; k < 2; ++k)
        {
            Engine e = fresh(); e.p.decayOn = k == 1; e.p.lD = 5.0f; e.p.lS = 10.0f; e.noteOn (60, 0.8f);
            run (e, 0.3); e.noteOff (60);
            auto o = run (e, 1.0);
            const float after = rms (o.L, 0.10, 0.15);
            std::printf ("  DECAY %s: nivel 100-150 ms después de soltar %.4f\n", k ? "on " : "off", after);
            check (k ? after > 0.01f : after < 1e-3f, k ? "sigue sonando con DECAY" : "corta sin DECAY");
        }
    }

    std::printf ("6. Disparo único y prioridad grave: el legato no redispara; la grave manda\n");
    {
        Engine e = fresh(); e.p.lA = 0.0f; e.p.lD = 2.0f; e.p.lS = 2.0f; e.noteOn (60, 0.8f);
        run (e, 0.8);
        const float before = e.lastEnvelope();
        e.noteOn (67, 0.8f); auto o = run (e, 0.4);
        const float after = e.lastEnvelope();
        std::printf ("  contorno antes %.3f, después de la segunda tecla %.3f\n", before, after);
        check (std::fabs (after - before) < 0.05f, "no redispara al tocar ligado");
        const float fa = f0 (o.L, 0.1, 0.35);
        std::printf ("  con 60 y 67 apretadas suena %.1f Hz (C4 = 261.6)\n", fa);
        check (std::fabs (cents (fa, 261.63f)) < 5.0f, "suena la grave");
        e.noteOff (60); auto o2 = run (e, 0.5); const float fb = f0 (o2.L, 0.2, 0.45);
        std::printf ("  al soltar la grave: %.1f Hz (G4 = 392)\n", fb);
        check (std::fabs (cents (fb, 392.0f)) < 5.0f, "pasa a la que queda");
    }

    std::printf ("7. Glide: con 5 tarda, sin GLIDE no\n");
    {
        Engine e = fresh(); e.p.glideOn = true; e.p.glide = 5.0f; e.noteOn (48, 0.8f); run (e, 0.5); e.noteOff (48);
        e.noteOn (72, 0.8f); auto o = run (e, 0.6);
        const float early = f0 (o.L, 0.02, 0.07, 48000.0, 50.0f, 2000.0f), late = f0 (o.L, 0.45, 0.58);
        std::printf ("  a 20-70 ms %.1f Hz, al final %.1f Hz (C5 = 523.3)\n", early, late);
        check (early < 450.0f && std::fabs (cents (late, 523.25f)) < 10.0f, "se desliza y llega");
    }

    std::printf ("8. Osc 3 como LFO (LO, sin control de teclado) modulando el filtro\n");
    {
        Engine e = fresh(); e.p.osc3Ctrl = false; e.p.range[2] = 0; e.p.wave[2] = Triangle; e.p.filterMod = true; e.modWheel = 1.0f;
        e.p.cutoff = -3.0f; e.p.emphasis = 5.0f; e.p.kbd1 = false; e.noteOn (48, 0.8f);
        auto o = run (e, 3.0);
        float lo = 1e9f, hi = 0;
        for (double t = 0.5; t < 2.9; t += 0.1) { const float r = rms (o.L, t, t + 0.1); lo = std::min (lo, r); hi = std::max (hi, r); }
        std::printf ("  nivel entre %.3f y %.3f a lo largo de 2.4 s\n", lo, hi);
        check (hi > 1.6f * lo, "el filtro se abre y se cierra");
    }

    std::printf ("9. Polifonía y unísono (extensión)\n");
    {
        Engine e = fresh(); e.st.voices = 6;
        for (int nn : { 48, 52, 55, 59 }) e.noteOn (nn, 0.8f);
        run (e, 0.2);
        std::printf ("  voces activas %d\n", e.activeVoices());
        check (e.activeVoices() == 4, "cuatro notas, cuatro voces");
        Engine u = fresh(); u.st.unison = 4; u.noteOn (60, 0.8f); run (u, 0.2);
        check (u.activeVoices() == 4, "unísono de 4 con una nota");
    }

    std::printf ("10. Frecuencias de muestreo y bloques: misma afinación y nivel\n");
    {
        float refL = 0;
        for (double sr : { 44100.0, 48000.0, 96000.0, 192000.0 })
            for (int os : { 1, 2, 4 })
            {
                Engine e = fresh (os, sr); e.p.cutoff = 1.0f; e.p.emphasis = 3.0f; e.noteOn (57, 0.8f);
                auto o = run (e, 0.5, sr, 333);
                const float f = f0 (o.L, 0.15, 0.45, sr), lv = rms (o.L, 0.15, 0.45, sr);
                if (refL == 0) refL = lv;
                const bool ok = std::fabs (cents (f, 220.0f)) < 4.0f && std::fabs (20.0f * std::log10 (lv / refL)) < 1.5f && finite (o);
                if (! ok) std::printf ("  %.0f Hz ×%d: %.2f Hz, nivel %+.2f dB\n", sr, os, f, 20.0f * std::log10 (lv / refL));
                check (ok, "afinación ±4 cents y nivel ±1.5 dB");
            }
    }

    std::printf ("11. 200 combinaciones extremas al azar: siempre finito y acotado\n");
    {
        dh::Rng rng (77); bool ok = true; float pk = 0;
        for (int it = 0; it < 200; ++it)
        {
            Engine e = fresh (1 + (it % 3)); auto& p = e.p;
            p.tune = rng.bipolar(); p.glide = 10 * rng.unipolar(); p.modMix = 10 * rng.unipolar(); p.oscMod = rng.unipolar() < 0.5f; p.osc3Ctrl = rng.unipolar() < 0.5f;
            for (int o = 0; o < 3; ++o) { p.range[o] = (int) (rng.unipolar() * 6); p.wave[o] = (int) (rng.unipolar() * 6); p.level[o] = 10 * rng.unipolar(); p.on[o] = rng.unipolar() < 0.7f; }
            p.freq2 = 14 * rng.bipolar(); p.freq3 = 14 * rng.bipolar(); p.noiseLevel = 10 * rng.unipolar(); p.noiseOn = rng.unipolar() < 0.4f; p.noisePink = rng.unipolar() < 0.5f;
            p.filterMod = rng.unipolar() < 0.5f; p.kbd1 = rng.unipolar() < 0.5f; p.kbd2 = rng.unipolar() < 0.5f;
            p.cutoff = 5 * rng.bipolar(); p.emphasis = 10 * rng.unipolar(); p.contourAmt = 10 * rng.unipolar();
            p.fA = 10 * rng.unipolar(); p.fD = 10 * rng.unipolar(); p.fS = 10 * rng.unipolar(); p.lA = 3 * rng.unipolar(); p.lD = 10 * rng.unipolar(); p.lS = 10 * rng.unipolar();
            p.decayOn = rng.unipolar() < 0.5f; p.glideOn = rng.unipolar() < 0.5f; p.volume = 10; e.modWheel = rng.unipolar(); e.bend = rng.bipolar();
            e.st.voices = rng.unipolar() < 0.3f ? 8 : 1; e.st.unison = rng.unipolar() < 0.3f ? 3 : 1;
            for (int k = 0; k < 4; ++k) e.noteOn (24 + (int) (rng.unipolar() * 90), rng.unipolar());
            auto o = run (e, 0.4, 48000.0, 1 + (int) (rng.unipolar() * 700));
            ok = ok && finite (o); pk = std::max (pk, peak (o.L));
        }
        std::printf ("  pico máximo %.2f\n", pk);
        check (ok, "sin NaN ni Inf"); check (pk <= 4.0f, "acotado por el limitador");
    }

    std::printf ("13. Leyes del panel (hardware-spec §3.4, §3.9, §5)\n");
    {
        // Corte −1.1 = 440 Hz sin seguimiento: con EMPHASIS 10 el filtro oscila ahí.
        Engine e = fresh(); e.p.on[0] = false; e.p.emphasis = 10.0f; e.p.cutoff = -1.1f; e.noteOn (60, 0.8f);
        auto o = run (e, 1.5); const float f = f0 (o.L, 0.8, 1.4, 48000.0, 100.0f, 2000.0f);
        std::printf ("  CUTOFF −1.1, EMPHASIS 10: oscila en %.1f Hz (%+.0f cents de 440)\n", f, cents (f, 440.0f));
        check (std::fabs (cents (f, 440.0f)) < 60.0f, "−1.1 en el dial ≈ 440 Hz (±60 cents: la dispersión entre voces es de ±0.06 oct)");
        // Umbral de autooscilación cerca de 8.
        float lv[3];
        const float es[3] = { 7.0f, 8.4f, 10.0f };
        for (int k = 0; k < 3; ++k)
        {
            Engine g = fresh(); g.p.on[0] = false; g.p.emphasis = es[k]; g.p.cutoff = 1.0f; g.noteOn (60, 0.8f);
            auto og = run (g, 2.0); lv[k] = rms (og.L, 1.5, 1.9);
        }
        std::printf ("  nivel autooscilando con EMPHASIS 7 / 8.4 / 10: %.4f / %.4f / %.4f\n", lv[0], lv[1], lv[2]);
        check (lv[0] < 1e-3f && lv[1] > 0.01f && lv[2] > 0.01f, "no oscila en 7, sí desde ~8");
        // Con el corte bajo la resonancia se apaga (el original dejaba de oscilar cerca de 150 Hz).
        Engine lo = fresh(); lo.p.on[0] = false; lo.p.emphasis = 10.0f; lo.p.cutoff = -3.0f; lo.noteOn (41, 0.8f);   // ≈ 118 Hz
        auto ol = run (lo, 3.0);
        std::printf ("  corte ≈ 118 Hz, EMPHASIS 10: nivel %.5f\n", rms (ol.L, 2.5, 2.9));
        check (rms (ol.L, 2.5, 2.9) < 1e-3f, "no oscila con el corte cerca de 120 Hz");
        // Glide: en 5 tarda ~0.09 s por octava (pote logarítmico, 1 ms .. 1 s/oct): dos octavas, ~0.18 s.
        Engine gg = fresh(); gg.p.glideOn = true; gg.p.glide = 5.0f; gg.noteOn (48, 0.8f); run (gg, 0.3); gg.noteOff (48); gg.noteOn (72, 0.8f);
        auto og2 = run (gg, 0.5);
        const float mid = f0 (og2.L, 0.07, 0.11, 48000.0, 50.0f, 2000.0f);
        std::printf ("  glide 5, C3→C5: a 70-110 ms %.1f Hz (a mitad de camino ≈ 261.6)\n", mid);
        check (std::fabs (cents (mid, 261.63f)) < 300.0f, "velocidad casi constante por octava");
    }

    std::printf ("12. CPU (porcentaje de un núcleo a 48 kHz)\n");
    {
        for (auto [voices, os] : { std::pair { 1, 2 }, std::pair { 1, 4 }, std::pair { 8, 2 }, std::pair { 8, 4 } })
        {
            Engine e = fresh (os); e.st.voices = voices; e.p.on[1] = e.p.on[2] = true; e.p.level[1] = e.p.level[2] = 7; e.p.emphasis = 6; e.p.decayOn = true;
            for (int k = 0; k < voices; ++k) e.noteOn (48 + 3 * k, 0.8f);
            auto o = run (e, 4.0);
            std::printf ("  %d voz/voces, %dx: %.2f %%\n", voices, os, o.cpu);
        }
    }

    std::printf ("Cambios con teclas apretadas: unísono, notas repetidas\n");
    {
        // UNISON 4, tecla apretada; UNISON 1, se suelta; UNISON 4 otra vez: no tiene que quedar un drone.
        Engine e = fresh(); e.p.decayOn = false; e.st.unison = 4; e.noteOn (60, 0.8f); run (e, 0.3);
        e.st.unison = 1; run (e, 0.05); e.noteOff (60); run (e, 0.05);
        e.st.unison = 4; auto o = run (e, 3.0);
        std::printf ("  después de soltar: %.0f dB a los 2-3 s\n", 20 * std::log10 (std::max (1e-9f, rms (o.L, 2.0, 3.0))));
        check (rms (o.L, 2.0, 3.0) < 1e-3f, "cambiar el unísono con una tecla apretada no deja voces sonando");
        // Dos note-on de la misma nota y un solo note-off (un host que repite el note-on): no queda colgada.
        Engine f = fresh(); f.p.decayOn = false; f.noteOn (60, 0.8f); run (f, 0.1); f.noteOn (60, 0.8f); run (f, 0.1); f.noteOff (60);
        auto q = run (f, 0.5);
        check (rms (q.L, 0.3, 0.5) < 1e-3f, "dos note-on de la misma nota y un note-off: se suelta (sin notas colgadas)");
    }

    std::printf (fails ? "\n%d FALLAS\n" : "\nTODO OK (0 fallas)\n", fails);
    return fails ? 1 : 0;
}
