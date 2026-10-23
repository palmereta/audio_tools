// Pruebas del motor de Murmur sin JUCE:
//   clang++ -std=c++20 -O2 -I plugins/murmur/Source/dsp -I shared/dsp plugins/murmur/tools/dsptest.cpp -o /tmp/mm_dsptest && /tmp/mm_dsptest
#include "MurmurEngine.h"
#include <chrono>
#include <cstdio>

using namespace dh::mm;
static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what); if (! ok) ++fails; }

struct Run { std::vector<float> L, R; double cpu = 0; };

// Entrada: un seno de 220 Hz durante "toneSec" y después silencio.
static Run run (Engine& e, double secs, double toneSec, double sr = 48000.0, int block = 512, HostInfo host = {}, float freq = 220.0f)
{
    Run r; std::vector<float> il (block), ir (block), ol (block), orr (block);
    const int total = (int) (secs * sr);
    const auto t0 = std::chrono::steady_clock::now();
    for (int pos = 0; pos < total; pos += block)
    {
        const int n = std::min (block, total - pos);
        for (int i = 0; i < n; ++i)
        {
            const double t = (pos + i) / sr;
            const float x = t < toneSec ? 0.5f * (float) std::sin (2.0 * 3.14159265 * freq * t) : 0.0f;
            il[(size_t) i] = x; ir[(size_t) i] = x;
        }
        e.process (il.data(), ir.data(), ol.data(), orr.data(), n, host);
        r.L.insert (r.L.end(), ol.begin(), ol.begin() + n); r.R.insert (r.R.end(), orr.begin(), orr.begin() + n);
        if (host.playing) host.ppq += n * host.bpm / 60.0 / sr;
    }
    r.cpu = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count() / secs * 100.0;
    return r;
}
static float rms (const std::vector<float>& v, double a, double b, double sr = 48000.0)
{
    double s = 0; size_t n = 0;
    for (size_t i = (size_t) (a * sr); i < (size_t) (b * sr) && i < v.size(); ++i) { s += (double) v[i] * v[i]; ++n; }
    return n ? (float) std::sqrt (s / n) : 0.0f;
}
static bool finite (const Run& r) { for (float x : r.L) if (! dh::isFiniteF (x)) return false; for (float x : r.R) if (! dh::isFiniteF (x)) return false; return true; }
// Frecuencia por cruces por cero (señal tonal).
static float zcFreq (const std::vector<float>& v, double a, double b, double sr = 48000.0)
{
    int z = 0; for (size_t i = (size_t) (a * sr) + 1; i < (size_t) (b * sr); ++i) if ((v[i - 1] < 0) != (v[i] < 0)) ++z;
    return (float) (z / 2.0 / (b - a));
}

int main()
{
    std::printf ("1. Nube por defecto sobre un seno\n");
    {
        Engine e; e.prepare (48000.0, 512); e.p.mix = 1.0f;
        auto r = run (e, 6.0, 6.0);
        std::printf ("  nivel wet %.3f, CPU %.2f %%\n", rms (r.L, 2, 6), r.cpu);
        check (finite (r), "salida finita");
        check (rms (r.L, 2, 6) > 0.05f && rms (r.L, 2, 6) < 0.8f, "nivel razonable (compensado por superposición)");
    }
    std::printf ("2. Pitch +12 sin azar: la nube suena una octava arriba\n");
    {
        Engine e; e.prepare (48000.0, 512); e.p.mix = 1.0f; e.p.pitch = 12.0f; e.p.spray = 0.0f; e.p.sizeMs = 200.0f; e.p.density = 10.0f; e.p.shape = 1.0f;
        auto r = run (e, 4.0, 4.0);
        const float f = zcFreq (r.L, 1.5, 3.5);
        std::printf ("  frecuencia %.1f Hz (esperado 440)\n", f);
        check (std::fabs (f - 440.0f) < 15.0f, "una octava arriba");
    }
    std::printf ("3. Freeze: con la entrada en silencio la nube sigue sonando con lo grabado\n");
    {
        Engine e; e.prepare (48000.0, 512); e.p.mix = 1.0f; e.p.bufferSec = 4.0f; e.p.position = 1.0f; e.p.spray = 0.5f;
        auto r1 = run (e, 3.0, 3.0);
        e.p.freeze = true;
        auto r2 = run (e, 6.0, 0.0);
        std::printf ("  nivel con freeze y entrada muda %.3f\n", rms (r2.L, 2, 6));
        check (rms (r2.L, 2, 6) > 0.05f, "el búfer congelado sigue sonando");
        e.p.freeze = false;
        auto r3 = run (e, 6.0, 0.0);
        check (rms (r3.L, 5, 6) < 1.0e-3f, "sin freeze, al vaciarse el búfer queda en silencio");
    }
    std::printf ("3b. Freeze con el búfer a medio llenar: Scan y los granos largos sólo leen lo grabado\n");
    {
        Engine e; e.prepare (48000.0, 512); e.p.mix = 1.0f; e.p.bufferSec = 16.0f; e.p.position = 0.3f; e.p.spray = 0.2f;
        e.p.sizeMs = 600.0f; e.p.density = 25.0f; e.p.scan = 0.5f; e.p.pitchRand = 7.0f; e.p.revMix = 0.0f; e.p.delayMix = 0.0f;
        run (e, 2.0, 2.0);                                       // 2 s grabados de 16
        e.p.freeze = true;
        auto r = run (e, 12.0, 0.0);
        float lo = 1.0f;
        for (double t = 1.0; t < 12.0; t += 0.1) lo = std::min (lo, rms (r.L, t, t + 0.1));
        std::printf ("  nivel mínimo en ventanas de 100 ms: %.3f\n", lo);
        check (lo > 0.02f, "ningún hueco de silencio en 12 s de Scan sobre 2 s congelados");
    }
    std::printf ("4. Feedback al máximo con shimmer: acotado y se apaga al cortar la entrada\n");
    {
        Engine e; e.prepare (48000.0, 512); e.p.mix = 1.0f; e.p.feedback = 0.97f; e.p.shimmer = 0.5f; e.p.density = 60.0f; e.p.revMix = 0.5f;
        auto r = run (e, 20.0, 5.0);
        float pk = 0; for (float x : r.L) pk = std::max (pk, std::fabs (x));
        std::printf ("  pico %.2f, nivel a los 19 s %.4f\n", pk, rms (r.L, 19, 20));
        check (finite (r) && pk < 4.0f, "acotado");
    }
    std::printf ("5. Sincronía: un grano por corchea a 120 BPM\n");
    {
        Engine e; e.prepare (48000.0, 512); e.p.mix = 1.0f; e.p.sync = 4; e.p.chance = 1.0f; e.p.sizeMs = 40.0f; e.p.shape = 0.2f; e.p.spray = 0.0f;
        HostInfo h; h.bpm = 120.0; h.playing = true;
        auto r = run (e, 4.0, 4.0, 48000.0, 512, h);
        int onsets = 0; bool on = false;
        for (size_t i = 0; i + 240 < r.L.size(); i += 240)       // envolvente: pico por ventana de 5 ms
        {
            float pk = 0; for (size_t j = i; j < i + 240; ++j) pk = std::max (pk, std::fabs (r.L[j]));
            const bool loud = pk > 0.02f; if (loud && ! on) ++onsets; on = loud;
        }
        std::printf ("  %d ataques en 4 s (esperado 16: 8 negras en 4 s a 120 BPM)\n", onsets);
        check (onsets >= 15 && onsets <= 17, "granos en la grilla");
    }
    std::printf ("6. MIDI Gate: sin notas no hay granos, con una nota sí, a su altura\n");
    {
        Engine e; e.prepare (48000.0, 512); e.p.mix = 1.0f; e.p.midiMode = MidiGate; e.p.spray = 0.0f; e.p.sizeMs = 200.0f; e.p.density = 10.0f;
        auto r1 = run (e, 2.0, 2.0);
        check (rms (r1.L, 0.5, 2.0) < 1.0e-4f, "mudo sin notas");
        e.noteOn (72, 1.0f);
        auto r2 = run (e, 3.0, 3.0);
        const float f = zcFreq (r2.L, 1.0, 3.0);
        std::printf ("  con C4 (+12): %.1f Hz\n", f);
        check (rms (r2.L, 1.0, 3.0) > 0.05f && std::fabs (f - 440.0f) < 15.0f, "suena con la nota, una octava arriba");
    }
    std::printf ("7. Frecuencias de muestreo y bloques\n");
    {
        bool ok = true;
        for (double sr : { 44100.0, 96000.0, 192000.0 })
            for (int b : { 1, 64, 4096 })
            {
                if (b == 1 && sr > 48000.0) continue;
                Engine e; e.prepare (sr, b); e.p.mix = 1.0f; e.p.pitch = 7.0f;
                auto r = run (e, 2.0, 2.0, sr, b);
                ok = ok && finite (r) && rms (r.L, 1, 2, sr) > 0.02f;
            }
        check (ok, "finito y con nivel a 44,1/96/192 kHz, bloques de 1 a 4096");
    }
    std::printf ("8. Parámetros extremos al azar\n");
    {
        dh::Rng rng (7); bool ok = true; float pk = 0;
        for (int t = 0; t < 60; ++t)
        {
            Engine e; e.prepare (48000.0, 256);
            auto& p = e.p;
            p.mix = rng.unipolar(); p.sizeMs = 5 + 1995 * rng.unipolar(); p.sizeRand = rng.unipolar(); p.density = 0.5f + 199.5f * rng.unipolar();
            p.densityRand = rng.unipolar(); p.position = 16 * rng.unipolar(); p.spray = 16 * rng.unipolar(); p.scan = 4 * rng.bipolar();
            p.pitch = 24 * rng.bipolar(); p.pitchRand = 24 * rng.unipolar(); p.quant = (int) (rng.unipolar() * QuantCount); p.shimmer = rng.unipolar();
            p.subOctave = rng.unipolar(); p.reverse = rng.unipolar(); p.shape = rng.unipolar(); p.skew = rng.bipolar(); p.pan = rng.unipolar();
            p.width = 2 * rng.unipolar(); p.lowCut = 20 + 2000 * rng.unipolar(); p.highCut = 200 + 20000 * rng.unipolar(); p.feedback = rng.unipolar();
            p.freeze = rng.unipolar() < 0.3f; p.bufferSec = 0.5f + 15.5f * rng.unipolar(); p.voices = 1 + (int) (127 * rng.unipolar());
            p.lfo[0] = { 20 * rng.unipolar(), rng.bipolar(), (int) (rng.unipolar() * 11), (int) (rng.unipolar() * LfoShapeCount), (int) (rng.unipolar() * TargetCount) };
            p.lfo[1] = { 20 * rng.unipolar(), rng.bipolar(), (int) (rng.unipolar() * 11), (int) (rng.unipolar() * LfoShapeCount), (int) (rng.unipolar() * TargetCount) };
            p.delayMix = rng.unipolar(); p.delayFb = rng.unipolar(); p.delaySync = (int) (rng.unipolar() * 11); p.revMix = rng.unipolar(); p.duck = rng.unipolar();
            p.sync = rng.unipolar() < 0.3f ? 1 + (int) (rng.unipolar() * 10) : 0; p.midiMode = (int) (rng.unipolar() * 3);
            if (p.midiMode) e.noteOn (40 + (int) (rng.unipolar() * 50), rng.unipolar());
            HostInfo h; h.playing = rng.unipolar() < 0.5f; h.bpm = 40 + 200 * rng.unipolar();
            auto r = run (e, 3.0, 3.0, 48000.0, 256, h, 110.0f + 800.0f * rng.unipolar());
            ok = ok && finite (r);
            for (float x : r.L) pk = std::max (pk, std::fabs (x));
        }
        std::printf ("  pico máximo %.2f\n", pk);
        check (ok, "60 combinaciones al azar: siempre finito");
        check (pk < 8.0f, "sin explosiones de nivel");
    }
    std::printf ("9. CPU con 128 granos simultáneos\n");
    {
        Engine e; e.prepare (48000.0, 512); e.p.mix = 1.0f; e.p.voices = 128; e.p.density = 200.0f; e.p.sizeMs = 1000.0f; e.p.revMix = 0.3f; e.p.delayMix = 0.3f;
        auto r = run (e, 10.0, 10.0);
        std::printf ("  %.2f %% de un núcleo, %d granos activos al final\n", r.cpu, e.activeGrains.load());
        check (e.activeGrains.load() >= 120, "llega a 128 granos");
    }
    std::printf ("10. Misma salida con bloques de 64, 333, 512 y 1024 muestras (dos LFO, ducking, mezcla modulada)\n");
    {
        // QA en REAPER 2026-10-22: con "First Murmur" el AU a 64 muestras no nulaba contra 512 (−2,2 dB): los tramos
        // de modulación empezaban de nuevo en cada bloque y la mezcla y el ducking tomaban el valor del final del bloque.
        auto make = [] (Engine& e) {
            e.prepare (48000.0, 1024); e.p.mix = 0.6f; e.p.density = 25.0f; e.p.duck = 0.5f; e.p.delayMix = 0.2f; e.p.revMix = 0.2f;
            e.p.lfo[0] = { 0.7f, 0.5f, 0, LStep, TPitch }; e.p.lfo[1] = { 0.3f, 0.6f, 0, LDrift, TMix };
        };
        Engine a; make (a); const auto ra = run (a, 4.0, 2.5, 48000.0, 512);
        float worst = 0.0f;
        for (int bs : { 64, 333, 1024 })
        {
            Engine b; make (b); const auto rb = run (b, 4.0, 2.5, 48000.0, bs);
            float d = 0.0f; for (size_t i = 0; i < ra.L.size(); ++i) d = std::max (d, std::max (std::fabs (ra.L[i] - rb.L[i]), std::fabs (ra.R[i] - rb.R[i])));
            std::printf ("  bloque %4d contra 512: diferencia máxima %.3g\n", bs, d);
            worst = std::max (worst, d);
        }
        check (worst == 0.0f, "bit a bit iguales");
    }
    std::printf ("11. Bloques al azar, y una instancia que ya sonó y el host volvió a preparar o reiniciar, contra una nueva\n");
    {
        // QA en REAPER 2026-10-22 (re-test): dos renders con "First Murmur" (reverb 0,25) no nulaban entre corridas ni entre
        // tamaños de búfer: prepare conservaba la memoria, el azar, la fase de la reverb y el estado de sus filtros, y el
        // host hace correr el plugin en silencio un tiempo que varía antes del render.
        auto make = [] (Engine& e) {
            e.p.mix = 0.5f; e.p.sizeMs = 180.0f; e.p.density = 18.0f; e.p.spray = 0.35f; e.p.pan = 0.6f; e.p.revMix = 0.25f; e.p.delayMix = 0.15f;
            e.p.lfo[0] = { 0.7f, 0.5f, 0, LDrift, TPitch };
            e.prepare (48000.0, 1024);
        };
        auto diff = [] (const Run& a, const Run& b) {
            float d = 0.0f; for (size_t i = 0; i < std::min (a.L.size(), b.L.size()); ++i) d = std::max (d, std::max (std::fabs (a.L[i] - b.L[i]), std::fabs (a.R[i] - b.R[i])));
            return d;
        };
        Engine a; make (a); const auto ra = run (a, 4.0, 2.5, 48000.0, 512);
        // Bloques de 1 a 1024 al azar.
        Engine b; make (b);
        Run rb; { std::vector<float> il (1024), ir (1024), ol (1024), orr (1024); dh::Rng br (77); HostInfo h;
            for (int pos = 0, total = 4 * 48000; pos < total;)
            {
                const int n = std::min (1 + (int) (br.unipolar() * 1024.0f), total - pos);
                for (int i = 0; i < n; ++i) { const double t = (pos + i) / 48000.0; il[(size_t) i] = ir[(size_t) i] = t < 2.5 ? 0.5f * (float) std::sin (2.0 * 3.14159265 * 220.0 * t) : 0.0f; }
                b.process (il.data(), ir.data(), ol.data(), orr.data(), n, h);
                rb.L.insert (rb.L.end(), ol.begin(), ol.begin() + n); rb.R.insert (rb.R.end(), orr.begin(), orr.begin() + n);
                pos += n;
            } }
        const float d1 = diff (ra, rb);
        std::printf ("  bloques al azar contra 512: diferencia máxima %.3g\n", d1);
        check (d1 == 0.0f, "bloques al azar: bit a bit iguales");
        // Sonó, siguió en silencio un rato, y el host lo preparó de nuevo (con otro tamaño de bloque).
        Engine c; make (c); run (c, 3.0, 3.0, 48000.0, 256); run (c, 1.37, 0.0, 48000.0, 64);
        c.prepare (48000.0, 333); const auto rc = run (c, 4.0, 2.5, 48000.0, 512);
        const float d2 = diff (ra, rc);
        std::printf ("  preparada de nuevo después de sonar: diferencia máxima %.3g\n", d2);
        check (d2 == 0.0f, "preparada de nuevo = nueva");
        // Lo mismo con reset() del host (AudioUnitReset): restart() sin volver a preparar.
        Engine d; make (d); run (d, 2.0, 2.0, 48000.0, 128); run (d, 0.81, 0.0, 48000.0, 128);
        d.restart(); const auto rd = run (d, 4.0, 2.5, 48000.0, 512);
        const float d3 = diff (ra, rd);
        std::printf ("  reiniciada después de sonar: diferencia máxima %.3g\n", d3);
        check (d3 == 0.0f, "reiniciada = nueva");
        // Congelada: la memoria se conserva al preparar de nuevo.
        Engine f; make (f); run (f, 2.0, 2.0); f.p.freeze = true; run (f, 0.2, 0.0); f.prepare (48000.0, 512);
        const auto rf = run (f, 1.0, 0.0);
        check (rms (rf.L, 0.2, 1.0) > 0.02f, "congelada: la memoria sigue después de preparar de nuevo");
    }
    std::printf ("\n%s (%d fallas)\n", fails == 0 ? "TODO OK" : "HAY FALLAS", fails);
    return fails == 0 ? 0 : 1;
}
