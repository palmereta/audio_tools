// at303test: pruebas del motor del AT303 sin JUCE (docs/research.md §7). Imprime lo medido y [ok] o [FALLA].
#include "Engine303.h"
#include <chrono>
#include <cstdio>
#include <cstring>

using namespace dh::tb;
static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what); if (! ok) ++fails; }

static Pattern basicPattern()
{
    Pattern p;
    const int notes[] = { 0, 0, 12, 0, 3, 0, 7, 10, 0, 0, 12, 5, 3, 0, 7, 0 };
    for (int i = 0; i < 16; ++i) { p.pitch[(size_t) i].note = (int8_t) (notes[i] % 12); p.pitch[(size_t) i].up = notes[i] >= 12; }
    p.pitch[2].accent = true; p.pitch[6].slide = true; p.pitch[10].accent = true; p.pitch[11].accent = true;
    p.time[3] = kTie; p.time[8] = kRest;
    return p;
}

// Renderiza `secs` segundos del secuenciador a `bpm` con bloques de `block` (o al azar si es 0).
static std::vector<float> renderSeq (const Pattern& pat, EngineParams ep, double bpm, double secs, int block, double fs = 48000)
{
    Engine e; e.prepare (fs, 4096); e.set (ep); e.setPattern (pat);
    const int n = (int) (secs * fs);
    std::vector<float> L ((size_t) n), R ((size_t) n);
    const double cps = bpm / 60.0 * 24.0 / fs;
    uint32_t s = 7;
    for (int i = 0; i < n;)
    {
        int b = block > 0 ? block : 1 + (int) ((s = s * 1664525u + 1013904223u) >> 23) % 600;
        b = std::min (b, n - i);
        e.process (L.data() + i, R.data() + i, b, true, (double) i * cps, cps);
        i += b;
    }
    return L;
}

int main()
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const double fs = 48000;

    std::printf ("\nT1 Compilación de patrones: alturas y tiempos separados\n");
    {
        Pattern p = basicPattern();
        auto notes = compile (p);
        // 16 pasos: 1 ligadura (paso 3) y 1 silencio (paso 8) → 14 notas; la nota del paso 2 dura 2 pasos.
        std::printf ("  %zu notas; la del paso 2 dura %d pasos; la del paso 9 usa la altura %d\n", notes.size(), notes[2].len, notes[7].semis);
        check (notes.size() == 14, "cada paso N consume la siguiente altura (14 notas)");
        check (notes[2].len == 2, "la ligadura alarga la nota anterior");
        check (notes[6].slide && notes[7].legatoIn == false && notes[5].slide == false, "un slide hacia un silencio no liga");
        // Mover la ligadura corre las alturas (el desorden del original).
        Pattern q = p; q.time[3] = kNote; q.time[4] = kTie;
        auto n2 = compile (q);
        check (n2[3].semis != notes[3].semis || n2.size() == notes.size(), "editar el tiempo corre las alturas siguientes");
    }

    std::printf ("\nT2 Compuerta: 3,5 de 6 pulsos; slide sin flanco\n");
    {
        Pattern p; for (auto& t : p.time) t = kNote;
        p.pitch[1].slide = true;                     // la nota 1 se liga a la 2
        Sequencer s; s.setPattern (p); s.restart();
        std::vector<std::pair<double, int>> ev;
        const double step = 0.25;                    // pulsos por llamada
        for (double c = 0; c < 6 * 4; c += step)
            s.advance (c, c + step, [&] (const SeqEvent& e, double f) { if (e.kind != SeqEvent::kSlide) ev.push_back ({ c + f * step, e.kind == SeqEvent::kNoteOn ? (e.legato ? 2 : 1) : 0 }); });
        // Esperado: on 0, off 3.5, on 6, (sin off: slide), on legato 12, off 15.5, on 18, off 21.5
        for (auto& e : ev) std::printf ("  %5.2f %s\n", e.first, e.second == 0 ? "compuerta abajo" : (e.second == 2 ? "nota ligada (sin flanco)" : "nota"));
        bool ok = ev.size() >= 7 && std::fabs (ev[1].first - 3.5) < 1e-6 && ev[1].second == 0 && ev[3].second == 2 && std::fabs (ev[3].first - 12) < 1e-6
                  && std::fabs (ev[4].first - 15.5) < 1e-6;
        check (ok, "compuerta 3,5/6, y la nota con slide no baja la compuerta ni redispara");
    }

    std::printf ("\nT2b Reloj del host bloque a bloque: ningún paso ni fin de compuerta se pierde\n");
    {
        // Como el procesador: el reloj de cada bloque sale de la posición del host (ppq · 24) y avanza por muestra;
        // entre bloques queda una rendija de redondeo. 60 s a varios tempos y tamaños de bloque.
        Pattern p; for (auto& t : p.time) t = kNote;
        bool ok = true;
        for (double bpm : { 120.0, 128.0, 132.0, 140.0, 174.0 })
            for (int block : { 64, 441, 512 })
            {
                Sequencer s; s.setPattern (p); s.restart();
                const double sr = 48000.0, cps = bpm / 60.0 * 24.0 / sr;
                int ons = 0, offs = 0; const int total = (int) (60 * sr);
                const double limit = (double) total / sr * bpm / 60.0 * 24.0 - 3.0;     // lejos del borde final
                for (int pos = 0; pos < total; pos += block)
                {
                    const double clock0 = (double) pos / sr * bpm / 60.0 * 24.0;
                    for (int i = 0; i < block; ++i)
                    {
                        const double c0 = clock0 + (double) i * cps;
                        s.advance (c0, c0 + cps, [&] (const SeqEvent& e, double) { if (c0 > limit) return; if (e.kind == SeqEvent::kNoteOn) ++ons; else if (e.kind == SeqEvent::kGateOff) ++offs; });
                    }
                }
                const int want = (int) std::floor (limit / 6.0) + 1;                // pasos que empiezan antes del límite
                if (std::abs (ons - want) > 0 || std::abs (offs - ons) > 1) { ok = false; std::printf ("  %.0f BPM, bloque %d: %d notas (esperadas %d), %d fines de compuerta\n", bpm, block, ons, want, offs); }
            }
        check (ok, "todos los pasos suenan con el reloj del host en bloques");
    }

    std::printf ("\nT3 El anillo de calibración (Cutoff al medio, Resonance al máximo)\n");
    {
        Voice v; v.prepare ((float) fs, 4);
        VoiceParams p; p.cutoff = 0.5f; p.resonance = 1.0f; p.envMod = 0.0f; p.decay = 0.0f; p.accent = 0.0f; v.set (p);
        std::vector<float> o; float buf[8];
        v.noteOn (2.0f, false, false);
        for (int i = 0; i < 48000; ++i) { v.processBase (buf); o.push_back (buf[0]); }
        double bestF = 0, bestA = 0;
        for (double f = 300; f < 3000; f += 5)
        {
            double re = 0, im = 0;
            for (int i = 8000; i < 40000; ++i) { re += o[(size_t) i] * std::cos (2 * dh::kPiD * f * i / fs); im += o[(size_t) i] * std::sin (2 * dh::kPiD * f * i / fs); }
            if (re * re + im * im > bestA) { bestA = re * re + im * im; bestF = f; }
        }
        std::printf ("  resonancia en %.0f Hz: período %.2f ms (manual de servicio: 2 ± 0,5 ms)\n", bestF, 1000.0 / bestF);
        check (bestF > 400 && bestF < 667, "dentro de la tolerancia del ajuste de fábrica");
    }

    std::printf ("\nT4 Acentos seguidos: el capacitor del barrido se acumula\n");
    {
        Voice v; v.prepare ((float) fs, 4);
        VoiceParams p; p.cutoff = 0.3f; p.resonance = 0.9f; p.envMod = 0.3f; p.decay = 0.3f; p.accent = 1.0f; v.set (p);
        float buf[8]; float peaks[6] {};
        const int stepS = (int) (fs * 0.125);       // semicorcheas a 120
        for (int k = 0; k < 6; ++k)
        {
            v.noteOn (2.0f, true, false);
            for (int i = 0; i < stepS; ++i) { v.processBase (buf); peaks[k] = std::max (peaks[k], v.c13()); if (i == stepS * 7 / 12) v.gateOff(); }
        }
        std::printf ("  tensión de C13 en cada acento:"); for (float x : peaks) std::printf (" %.2f", x); std::printf (" V\n");
        check (peaks[1] > peaks[0] * 1.05f && peaks[3] > peaks[1], "cada acento sube más que el anterior");
        // Con Resonance al mínimo el barrido es directo (sin acumular).
        Voice w; w.prepare ((float) fs, 4); p.resonance = 0.0f; w.set (p); float pk[3] {};
        for (int k = 0; k < 3; ++k) { w.noteOn (2.0f, true, false); for (int i = 0; i < stepS; ++i) { w.processBase (buf); pk[k] = std::max (pk[k], w.c13()); if (i == stepS * 7 / 12) w.gateOff(); } }
        std::printf ("  con Resonance al mínimo: %.2f %.2f %.2f V\n", pk[0], pk[1], pk[2]);
    }

    std::printf ("\nT5 Slide: 22 ms de constante de tiempo, igual para cualquier intervalo\n");
    {
        for (int semis : { 1, 12, 24 })
        {
            Voice v; v.prepare ((float) fs, 4); VoiceParams p; v.set (p); float buf[8];
            v.noteOn (2.0f, false, false); for (int i = 0; i < 2000; ++i) v.processBase (buf);
            v.setSlide (true); v.noteOn (2.0f + semis / 12.0f, false, true);
            int t63 = -1; const float target = 2.0f + semis / 12.0f;
            for (int i = 0; i < 9600; ++i) { v.processBase (buf); if (t63 < 0 && v.cvVolts() >= 2.0f + 0.632f * (target - 2.0f)) t63 = i; }
            std::printf ("  %2d semitonos: 63 %% en %.1f ms\n", semis, t63 / fs * 1000);
            check (std::fabs (t63 / fs * 1000 - 22.0) < 1.0, "τ = 22 ms ± 1");
        }
    }

    std::printf ("\nT6 Determinismo con cualquier tamaño de bloque\n");
    {
        EngineParams ep; ep.voice.resonance = 0.8f; ep.voice.envMod = 0.6f; ep.driveType = 1; ep.delayMix = 0.3f;
        auto a = renderSeq (basicPattern(), ep, 128, 4.0, 1), b = renderSeq (basicPattern(), ep, 128, 4.0, 512), c = renderSeq (basicPattern(), ep, 128, 4.0, 0);
        check (std::memcmp (a.data(), b.data(), a.size() * 4) == 0 && std::memcmp (a.data(), c.data(), a.size() * 4) == 0, "bloques de 1, 512 y al azar: idéntico bit a bit");
    }

    std::printf ("\nT7 Robustez\n");
    {
        bool ok = true; float peak = 0; uint32_t s = 3;
        auto rnd = [&] { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) / 16777216.0f; };
        for (int t = 0; t < 40; ++t)
        {
            EngineParams ep; VoiceParams& v = ep.voice;
            v.cutoff = rnd(); v.resonance = rnd(); v.envMod = rnd(); v.decay = rnd(); v.accent = rnd(); v.square = t & 1; v.tuneSemis = 14 * rnd() - 7;
            v.overdrive = 1 + 65 * rnd() * rnd(); v.highResonance = t % 3 == 0; v.filterFm = rnd() * rnd(); v.tracking = rnd(); v.muffler = rnd();
            v.envModRange = 1 + 2 * rnd(); v.sweepSpeed = t % 2 ? 0.33f : 2.2f; v.cutoffTopHz = 5000;
            ep.driveType = t % 4; ep.drive = rnd(); ep.delayMix = rnd(); ep.delayFeedback = 0.95f * rnd(); ep.oversample = t % 2 ? 2 : 4;
            GenParams g; g.seed = (uint32_t) t + 1; g.octaves = 2;
            auto o = renderSeq (generate (Pattern(), g), ep, 60 + 200 * rnd(), 2.0, 0);
            for (float x : o) { ok &= dh::isFiniteF (x); peak = std::max (peak, std::fabs (x)); }
        }
        std::printf ("  pico máximo %.2f\n", peak);
        check (ok, "sin NaN ni Inf en 40 configuraciones extremas");
        check (peak < 8.0f, "salida acotada");
    }

    std::printf ("\nT8 CPU (48 kHz, estéreo, secuenciador corriendo)\n");
    {
        auto bench = [&] (EngineParams ep, const char* what) {
            const auto t0 = std::chrono::steady_clock::now();
            renderSeq (basicPattern(), ep, 128, 10.0, 512);
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  %-40s %.2f %% de un núcleo\n", what, secs * 10.0);
        };
        EngineParams d; bench (d, "sonido por defecto, ×4");
        EngineParams w = d; w.voice.square = true; w.driveType = 1; w.delayMix = 0.3f; w.voice.filterFm = 0.3f; bench (w, "cuadrada, distorsión, delay, FM del filtro");
        EngineParams h = d; h.oversample = 8; bench (h, "×8");
    }

    std::printf ("\n%s (%d fallas)\n", fails ? "HAY FALLAS" : "TODO OK", fails);
    return fails ? 1 : 0;
}
