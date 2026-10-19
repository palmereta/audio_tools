// Verificación numérica del motor del ATFS1 contra las mediciones públicas del FS1R
// (docs/hardware-spec.md). Compilar sin JUCE:
//   clang++ -std=c++17 -O2 -I shared/dsp -I plugins/atfs1/Source/dsp plugins/atfs1/tools/dsptest.cpp -o /tmp/atfs1_dsptest

#include "FsEngine.h"
#include "FsEffects.h"
#include "Syx.h"
#include <chrono>
#include <cstdio>
#include <vector>

using namespace dh;
using namespace dh::fs;

static const float FS = 48000.0f;

// Magnitud (dB relativos a un seno de amplitud 1) en la frecuencia f, ventana de Hann.
static float magDb (const std::vector<float>& x, float f, size_t start = 0, size_t len = 0)
{
    if (len == 0) len = x.size() - start;
    double re = 0, im = 0, wsum = 0;
    for (size_t i = 0; i < len; ++i)
    {
        const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * (double) i / (double) (len - 1));
        const double ph = 2.0 * M_PI * f * (double) i / FS;
        re += w * x[start + i] * std::cos (ph); im += w * x[start + i] * std::sin (ph);
        wsum += w;
    }
    return (float) (20.0 * std::log10 (2.0 * std::sqrt (re * re + im * im) / wsum + 1e-12));
}

static std::vector<float> renderOp (int form, float f0, float carrier, int skirt, int bwRes, float secs = 1.0f)
{
    FormOperator op;
    const float win = 1.0f / (3.1f * std::exp2 ((float) bwRes / 8.0f));
    std::vector<float> y ((size_t) (secs * FS));
    for (auto& s : y) s = op.process (form, f0, carrier, win, Tables::pIndex (form == Frmt, skirt), bwRes, 0.0f, FS);
    return y;
}

static void partials (const char* label, const std::vector<float>& y, float f0, int n)
{
    std::printf ("  %-26s", label);
    const float ref = magDb (y, f0, 4800, 38400);
    for (int h = 1; h <= n; ++h) std::printf (" %6.1f", magDb (y, f0 * (float) h, 4800, 38400) - (h == 1 ? 0.0f : ref));
    std::printf ("   (h1 %.1f dB)\n", ref);
}

static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what); if (! ok) ++fails; }

int main()
{
    std::printf ("1. Formas espectrales (parciales 1..8 en dB relativos a la 1, f = 220 Hz)\n");
    {
        auto a1 = renderOp (All1, 220, 220, 0, 0);
        partials ("all1 falda 0", a1, 220, 8);
        check (magDb (a1, 440, 4800, 38400) - magDb (a1, 220, 4800, 38400) < -40.0f, "all1 a falda 0 = una sola parcial");
        auto a2 = renderOp (All2, 220, 220, 3, 0); partials ("all2 falda 3", a2, 220, 8);
        auto a17 = renderOp (All1, 220, 220, 7, 0); partials ("all1 falda 7", a17, 220, 8);
        auto a27 = renderOp (All2, 220, 220, 7, 0); partials ("all2 falda 7", a27, 220, 8);
        // Ancho equivalente del grupo (suma de potencias / la mayor, de un lado): FSVR mide 1,34 y 6,5 parciales
        // contando las dos mitades del grupo; de un lado, ~1,2 y ~3,3.
        auto eqw = [] (const std::vector<float>& y) { double mx = 0, sum = 0; for (int h = 1; h <= 16; ++h) { const double p = std::pow (10.0, magDb (y, 220.0f * (float) h, 4800, 38400) / 10.0); mx = std::max (mx, p); sum += p; } return sum / mx; };
        const double w1 = eqw (a17), w2 = eqw (a27);
        std::printf ("    ancho equivalente a falda 7: all1 %.2f, all2 %.2f parciales\n", w1, w2);
        check (w1 > 1.05 && w1 < 1.6 && w2 > 2.5 && w2 < 4.5, "all1 angosto y all2 ancho a falda 7");
        auto o1 = renderOp (Odd1, 220, 220, 0, 0); partials ("odd1 falda 0", o1, 220, 8);
        check (magDb (o1, 440, 4800, 38400) - magDb (o1, 220, 4800, 38400) < -40.0f && magDb (o1, 660, 4800, 38400) - magDb (o1, 220, 4800, 38400) > -20.0f,
               "odd = parciales impares (1 y 3 a falda 0)");
        auto o2 = renderOp (Odd2, 220, 220, 4, 0); partials ("odd2 falda 4", o2, 220, 8);
        auto r1 = renderOp (Res1, 220, 220, 0, 4); partials ("res1 res 4 falda 0", r1, 220, 8);
        const float c = magDb (r1, 220 * 5, 4800, 38400), s1 = magDb (r1, 220 * 4, 4800, 38400), s2 = magDb (r1, 220 * 6, 4800, 38400), out = magDb (r1, 220 * 2, 4800, 38400);
        std::printf ("    res1: centro %.1f, lados %.1f / %.1f, parcial 2 %.1f dB\n", c, s1 - c, s2 - c, out - c);
        check (std::fabs (s1 - c + 6.0f) < 1.5f && std::fabs (s2 - c + 6.0f) < 1.5f && out - c < -30.0f, "res = 3 líneas centradas en la parcial res+1, lados −6 dB");
    }
    std::printf ("2. frmt: f0 110 Hz, centro 1000 Hz\n");
    for (int bw : { 20, 40, 60 })
    {
        auto y = renderOp (Frmt, 110, 1000, 0, bw);
        float best = -200; int bh = 0;
        std::vector<float> m (40);
        for (int h = 1; h < 40; ++h) { m[(size_t) h] = magDb (y, 110.0f * (float) h, 4800, 38400); if (m[(size_t) h] > best) { best = m[(size_t) h]; bh = h; } }
        int lo = bh, hi = bh;
        while (lo > 1 && m[(size_t) lo - 1] > best - 6.0f) --lo;
        while (hi < 39 && m[(size_t) hi + 1] > best - 6.0f) ++hi;
        const float expectHz = 3.1f * std::exp2 ((float) bw / 8.0f);
        std::printf ("  bw %2d: pico en %4.0f Hz, ancho −6 dB ≈ %4.0f Hz (ventana 1/(3,1·2^(bw/8)) → escala %.0f Hz)\n",
                     bw, 110.0f * (float) bh, 110.0f * (float) (hi - lo + 1), expectHz);
        check (std::fabs (110.0f * (float) bh - 1000.0f) < 120.0f, "el pico del formante cae en el centro");
    }
    std::printf ("3. Pérdida de nivel por falda (all2, energía total)\n  ");
    for (int s = 0; s < 8; ++s)
    {
        auto y = renderOp (All2, 220, 220, s, 0);
        double e = 0; for (size_t i = 4800; i < y.size(); ++i) e += (double) y[i] * y[i];
        std::printf ("s%d %.1f  ", s, 10.0 * std::log10 (e / (double) (y.size() - 4800) * 2.0));
    }
    std::printf ("dB\n");

    std::printf ("4. Operador sin voz: centro 2 kHz\n");
    for (int bw : { 5, 30, 60 })
    {
        NoiseOperator no;
        std::vector<float> y (48000);
        for (auto& s : y) s = no.process (2000.0f, bw, 0, 0, FS);
        double num = 0, den = 0;
        for (int f = 200; f < 8000; f += 50) { const double p = std::pow (10.0, magDb (y, (float) f) / 10.0); num += p * f; den += p; }
        double rms = 0; for (auto s : y) rms += (double) s * s;
        std::printf ("  bw %2d: centroide %.0f Hz, rms %.1f dB\n", bw, num / den, 10.0 * std::log10 (rms / y.size()));
        check (std::fabs (num / den - 2000.0) < 600.0, "el ruido queda centrado en el formante");
    }

    std::printf ("5. Envolventes (ley del chip)\n");
    for (int t : { 0, 30, 50, 70, 90, 99 })
        std::printf ("  T=%2d: recorrido de 96 dB = %.4f s\n", t, traverseSeconds (rateOf (t)));
    check (std::fabs (traverseSeconds (63) - 0.0061f) < 0.0002f && std::fabs (traverseSeconds (0) - 349.5f) < 1.0f, "2^(26 − q/4)/(4 + q mod 4)/48000: 6,1 ms a 350 s");
    {
        AmpEgData d; d.level = { 99, 70, 70, 0 }; d.time = { 40, 50, 50, 50 };
        AmpEg eg; eg.setup (d, 60, 0, 0, 0, 0.0f); eg.keyOn (false);
        float t = 0, tAtk = -1, tDec = -1; const float dt = 16.0f / FS;
        for (int i = 0; i < 100000; ++i)
        {
            const float db = eg.process (dt); t += dt;
            if (tAtk < 0 && db >= 0.0f) tAtk = t;
            if (tDec < 0 && tAtk > 0 && db <= levelDb (70) + 0.01f) tDec = t - tAtk;
            if (tDec > 0) break;
        }
        std::printf ("  ataque T=40: %.3f s, decaimiento de %.1f dB con T=50: %.3f s (esperado %.3f)\n", tAtk, -levelDb (70), tDec,
                     -levelDb (70) / 96.0f * traverseSeconds (rateOf (50)));
        check (std::fabs (tDec - (-levelDb (70) / 96.0f * traverseSeconds (rateOf (50)))) < 0.01f, "decaimiento lineal en dB");
    }

    std::printf ("6. Algoritmos\n");
    {
        int bad = 0, fb = 0;
        for (int a = 0; a < 88; ++a)
        {
            const auto& A = algorithm (a);
            if (A.carriers == 0) ++bad;
            for (int d = 0; d < 8; ++d) if (A.modMask[(size_t) d] >> d) ++bad;   // sólo hacia adelante
            for (int d = 0; d < 8; ++d) if ((A.carriers >> d & 1) == 0) { bool feeds = false; for (int e = 0; e < 8; ++e) feeds |= (A.modMask[(size_t) e] >> d & 1) != 0; if (! feeds) ++bad; }
            if (A.fbSrc >= 0) ++fb;
        }
        std::printf ("  88 algoritmos, %d con realimentación\n", fb);
        check (bad == 0, "toda portadora existe, toda modulación va hacia adelante y todo modulador llega a algún lado");
    }

    std::printf ("7. FM: dos senos 1:1 (alg 2: 1>2>3>4), modulador a nivel máximo\n");
    {
        VoiceData v; v.algorithm = 1;
        for (auto& o : v.op) o.level = 0;
        v.op[2].level = 99; v.op[3].level = 99;
        PartParams pp; pp.filterOn = false; pp.volume = 1.0f;
        Engine e; e.prepare (FS); e.noteOn (57, 127, v, pp);
        std::vector<float> L (48000), R (48000);
        e.process (L.data(), R.data(), 48000, v, nullptr, pp);
        std::printf ("  parciales: ");
        const float ref = magDb (L, 220, 9600, 38400);
        for (int h = 1; h <= 8; ++h) std::printf ("%.1f ", magDb (L, 220.0f * (float) h, 9600, 38400) - ref);
        std::printf ("dB\n");
        // Índice 2π·3,369 = 21,2 rad → bandas laterales hasta ~22 parciales.
        check (magDb (L, 220.0f * 10.0f, 9600, 38400) - ref > -20.0f, "índice de FM alto (3,369 ciclos): bandas laterales hasta la 10.ª parcial");
    }

    std::printf ("8. Motor: Fseq de vocales y CPU\n");
    {
        VoiceData v; v.algorithm = 0;    // 8 portadoras
        for (int k = 0; k < 8; ++k)
        {
            auto& o = v.op[(size_t) k]; o.form = Frmt; o.fixed = true; o.coarse = 16; o.level = 99 - k * 6; o.bwRes = 46; o.skirt = 1; o.fseq = true; o.fseqTrack = k;
            o.eg.time = { 30, 40, 40, 45 };
            auto& u = v.nop[(size_t) k]; u.level = 60; u.fseq = true; u.bandwidth = 40; u.eg.time = { 30, 40, 40, 45 };
        }
        const FseqData f = vowelSequence ("Test", "aeiou", 0.35f, 0.4f, 130.0f, 1.0f, 0.3f, 0.1f);
        std::printf ("  Fseq: %d cuadros, nota %d, lazo %d-%d\n", (int) f.frames.size(), f.noteAssign, f.loopStart, f.loopEnd);
        PartParams pp; pp.fseqOn = true; pp.maxVoices = 16;
        Engine e; e.prepare (FS);
        const int N = 48000 * 2;
        std::vector<float> L ((size_t) N), R ((size_t) N);
        for (int n : { 48, 52, 55, 60, 64, 67, 72, 76, 79, 84, 50, 57, 62, 69, 74, 81 }) e.noteOn (n, 100, v, pp);
        const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < N; b += 256) e.process (L.data() + b, R.data() + b, 256, v, &f, pp);
        const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        float peak = 0; bool finite = true;
        for (auto s : L) { peak = std::max (peak, std::fabs (s)); finite &= std::isfinite (s); }
        std::printf ("  16 notas × 16 operadores con Fseq: %.1f %% de un núcleo (pico %.2f)\n", secs / 2.0 * 100.0, peak);
        check (finite && peak > 0.01f, "salida finita y audible");

        // Análisis de vuelta: una "a" sintetizada se analiza y el F1/F2 deben caer cerca de 730/1090 Hz.
        const FseqData a = vowelSequence ("A", "a", 1.0f, 0.0f, 120.0f);
        PartParams p1; p1.fseqOn = true; p1.filterOn = false;
        Engine e1; e1.prepare (FS); e1.noteOn (a.noteAssign, 127, v, p1);
        std::vector<float> M (48000), M2 (48000);
        e1.process (M.data(), M2.data(), 48000, v, &a, p1);
        const FseqData an = analyze (M, FS, "an");
        const auto& fr = an.frames[an.frames.size() / 2];
        std::printf ("  análisis de una 'a' sintética: altura %.0f Hz, formantes %.0f %.0f %.0f Hz\n", fr.pitch, fr.vFreq[0], fr.vFreq[1], fr.vFreq[2]);
        check (std::fabs (fr.pitch - 120.0f) < 8.0f, "altura recuperada");
        check (std::fabs (fr.vFreq[0] - 730.0f) < 150.0f && std::fabs (fr.vFreq[1] - 1090.0f) < 200.0f, "F1 y F2 recuperados");
    }
    std::printf ("9. Sysex: ida y vuelta\n");
    {
        VoiceData v; v.name = "RoundTrip"; v.algorithm = 37; v.feedback = 5; v.pegLevel = { -10, 20, 0, 5, -3 }; v.fegLevel = { 10, -20, 30, -5 };
        v.resonance = -9; v.inputGain = 4; v.formantKnob[2] = { 2, 1, 5, -20 }; v.fmKnob[0] = { 1, 0, 3, 40 };
        for (int k = 0; k < 8; ++k)
        {
            auto& o = v.op[(size_t) k]; o.form = k; o.fixed = (k & 1) != 0; o.coarse = 3 + k; o.fine = 10 * k; o.skirt = k; o.bwRes = 11 * k; o.fseq = (k % 3) == 0; o.fseqTrack = 7 - k;
            o.freqBias = k - 4; o.pitchMod = k; o.ampVel = 3 - k; o.eg.level = { 99, 80 - k, 60, 0 }; o.level = 50 + k; o.transpose = k - 3; o.detune = k - 7;
            auto& u = v.nop[(size_t) k]; u.mode = k % 3; u.coarse = 15 + k % 6; u.fine = 9 * k; u.resonance = k; u.skirt = 7 - k; u.level = 20 + k; u.fseq = (k & 2) != 0; u.levelKeyScale = k - 4;
        }
        const auto syx = syx::voiceSyx (v);
        const auto bulks = syx::parse (syx);
        bool ok = syx.size() == 619 && bulks.size() == 1 && syx::isVoice (bulks[0]);
        int sum = 0; for (size_t i = 4; i < syx.size() - 1; ++i) sum += syx[i];
        ok = ok && (sum & 0x7F) == 0;
        if (ok)
        {
            const VoiceData w = syx::readVoice (bulks[0].data.data());
            ok = syx::writeVoiceData (w) == syx::writeVoiceData (v) && w.name == "RoundTrip" && w.op[5].form == 5 && w.nop[4].mode == 1 && w.op[0].fseq && ! w.op[1].fseq && w.op[3].fseq && w.pegLevel[3] == 5;
        }
        std::printf ("  voz: %zu bytes\n", syx.size());
        check (ok, "voz de 619 bytes, suma de verificación y todos los campos ida y vuelta");
        const FseqData f = vowelSequence ("Vowels", "aeiou", 0.2f, 0.3f, 110.0f);
        const auto fs = syx::fseqSyx (f);
        const auto fb = syx::parse (fs);
        bool okf = fb.size() == 1 && syx::isFseq (fb[0]);
        if (okf)
        {
            const FseqData g = syx::readFseq (fb[0].data);
            float err = 0.0f; int lerr = 0;
            for (size_t i = 0; i < f.frames.size(); ++i)
                for (int k = 0; k < 8; ++k)
                {
                    err = std::max (err, std::fabs (std::log2 (g.frames[i].vFreq[(size_t) k] / f.frames[i].vFreq[(size_t) k])) * 1200.0f);
                    lerr = std::max (lerr, std::abs ((int) g.frames[i].vLevel[(size_t) k] - (int) f.frames[i].vLevel[(size_t) k]));
                }
            std::printf ("  Fseq: %zu bytes, %zu cuadros, error de frecuencia %.2f cents, de nivel %d\n", fs.size(), g.frames.size(), err, lerr);
            okf = g.frames.size() == f.frames.size() && err < 2.5f && lerr == 0 && g.loopEnd == f.loopEnd;
        }
        check (okf, "Fseq ida y vuelta (palabras de altura de 1/1024 de octava)");
    }
    std::printf ("10. Efectos: todos los tipos sin NaN\n");
    {
        Effects fx; fx.prepare (FS, 512);
        bool ok = true;
        for (int it = 0; it < InsCount; ++it)
            for (int vt = 0; vt < VarCount; ++vt)
            {
                FxParams p; p.insType = it; p.varType = vt; p.revType = (it + vt) % RevCount; p.insAmount = 0.8f; p.varDepth = 0.9f; p.eqLow = 6; p.eqHigh = -6;
                std::vector<float> l (512), r (512);
                for (int b = 0; b < 40; ++b)
                {
                    for (int i = 0; i < 512; ++i) { l[(size_t) i] = 0.7f * std::sin ((float) (b * 512 + i) * 0.05f); r[(size_t) i] = l[(size_t) i]; }
                    fx.process (l.data(), r.data(), 512, p);
                    for (int i = 0; i < 512; ++i) ok &= std::isfinite (l[(size_t) i]) && std::fabs (l[(size_t) i]) <= 1.01f;
                }
            }
        check (ok, "9 inserciones × 11 variaciones × 8 reverbs: salida finita y limitada");
    }
    std::printf ("\n%s (%d fallas)\n", fails == 0 ? "TODO OK" : "HAY FALLAS", fails);
    return fails == 0 ? 0 : 1;
}
