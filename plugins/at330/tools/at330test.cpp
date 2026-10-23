// at330test: pruebas del motor del AT330 (docs/research.md §7; T1-T11 sólo el motor) y del procesador completo sin
// host (T12-T16). Imprime lo medido y [ok] o [FALLA]. Sale con 1 si algo falla.
#include "PluginProcessor.h"
#include "Engine330.h"
#include "Oversampling.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

using namespace dh::vp;
static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what); if (! ok) ++fails; }
static constexpr double fs = 48000.0;

static double rmsDb (const std::vector<float>& x, size_t a, size_t b)
{
    double e = 0; b = std::min (b, x.size());
    for (size_t i = a; i < b; ++i) e += (double) x[i] * x[i];
    return 10.0 * std::log10 (e / (double) std::max<size_t> (1, b - a) + 1e-30);
}

// La frecuencia de una señal por cruces por cero ascendentes interpolados (para un tono con un armónico dominante).
static double zeroCrossHz (const std::vector<float>& x, size_t a, size_t b)
{
    double first = -1, last = -1; int count = 0;
    for (size_t i = a + 1; i < b; ++i)
        if (x[i - 1] < 0.0f && x[i] >= 0.0f)
        {
            const double t = (double) (i - 1) + x[i - 1] / (x[i - 1] - x[i]);
            if (first < 0) first = t; else ++count;
            last = t;
        }
    return count > 0 ? count * fs / (last - first) : 0.0;
}

// Energía de x en una banda (DFT directa a una frecuencia).
static double tone (const std::vector<float>& x, double f, size_t a, size_t b)
{
    double re = 0, im = 0;
    for (size_t i = a; i < b; ++i) { const double w = 2 * dh::kPiD * f * (double) i / fs; re += x[i] * std::cos (w); im += x[i] * std::sin (w); }
    return (re * re + im * im) / (double) ((b - a) * (b - a));
}

struct Ev { int at; int note; int vel; };   // vel 0 = note-off
// Renderiza el motor con eventos, una voz opcional y bloques fijos (o al azar si block = 0). Devuelve L y R.
static void render (Engine& e, const Params& p, const std::vector<Ev>& ev, int total, int block, std::vector<float>& L, std::vector<float>& R,
                    const std::vector<float>* mic = nullptr)
{
    e.set (p);
    L.assign ((size_t) total, 0.0f); R.assign ((size_t) total, 0.0f);
    uint32_t s = 11; size_t k = 0;
    for (int i = 0; i < total;)
    {
        int b = block > 0 ? block : 1 + (int) ((s = s * 1664525u + 1013904223u) >> 23) % 700;
        b = std::min (b, total - i);
        // Los eventos caen en su muestra: se corta el bloque ahí.
        while (k < ev.size() && ev[k].at <= i) { if (ev[k].vel > 0) e.noteOn (ev[k].note, ev[k].vel); else e.noteOff (ev[k].note); ++k; }
        if (k < ev.size() && ev[k].at < i + b) b = ev[k].at - i;
        e.process (L.data() + i, R.data() + i, mic ? mic->data() + i : nullptr, nullptr, b);
        i += b;
    }
}
static std::vector<float> renderL (const Params& p, const std::vector<Ev>& ev, int total, int block = 512, const std::vector<float>* mic = nullptr)
{
    Engine e; e.prepare (fs, 512); std::vector<float> L, R; render (e, p, ev, total, block, L, R, mic); return L;
}
static Params stringsOnly() { Params p; p.strEns = false; p.strAttack = 0.005f; p.release = 0.05f; p.keying = kKeyingPoly; p.strTone = 1.0f; return p; }

// Una voz de prueba: pulso glótico a 110 Hz por dos formantes, con una /s/ (ruido agudo) en el medio.
static std::vector<float> testVoice (int total, double sFrom = -1, double sTo = -1)
{
    std::vector<float> v ((size_t) total);
    dh::Rng rng (5); float s1 = 0, s2 = 0, h = 0;
    for (int i = 0; i < total; ++i)
    {
        const double t = i / fs, ph = std::fmod (t * 110.0, 1.0);
        float x = (float) (ph < 0.4 ? std::sin (dh::kPiD * ph / 0.4) : 0.0) * 0.5f;
        s1 += 0.08f * (x - s1); s2 += 0.3f * (s1 - s2);
        float y = x - s2;
        if (t >= sFrom && t < sTo) { const float nz = rng.bipolar(); h = nz - 0.6f * h; y = 0.3f * (nz - h); }
        v[(size_t) i] = y;
    }
    return v;
}

// Un host de mentira para el procesador.
struct TestHead : public juce::AudioPlayHead
{
    bool playing = false; juce::int64 pos = 0; double bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setIsPlaying (playing); p.setTimeInSamples (pos); p.setPpqPosition ((double) pos / fs * bpm / 60.0);
        return p;
    }
};

// El procesador tocando un acorde de 3 s y soltándolo (canal izquierdo), con bloques de 512.
static std::vector<float> renderProc (At330Processor& p, double secs = 4.5, bool withVoice = false)
{
    TestHead h; p.setPlayHead (&h); p.setPlayConfigDetails (2, 2, fs, 512); p.prepareToPlay (fs, 512);
    const int total = (int) (secs * fs);
    const auto voice = testVoice (total, 1.0, 1.3);
    std::vector<float> out; out.reserve ((size_t) total);
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer mb;
    for (int pos = 0; pos < total; pos += 512)
    {
        buf.clear(); mb.clear();
        if (pos == 0) for (int n : { 45, 57, 60, 64, 69, 72 }) mb.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
        if (pos <= (int) (3.0 * fs) && (int) (3.0 * fs) < pos + 512) for (int n : { 45, 57, 60, 64, 69, 72 }) mb.addEvent (juce::MidiMessage::noteOff (1, n), (int) (3.0 * fs) - pos);
        // La voz en los dos canales; con portador externo, la voz a la izquierda y una sierra de 110 Hz a la derecha.
        const bool ext = p.value ("carrier") > 0.5f;
        if (withVoice)
            for (int i = 0; i < 512 && pos + i < total; ++i)
            {
                buf.setSample (0, i, voice[(size_t) (pos + i)]);
                buf.setSample (1, i, ext ? 1.6f * (float) (std::fmod ((pos + i) * 110.0 / fs, 1.0) - 0.5) : voice[(size_t) (pos + i)]);
            }
        h.pos = pos;
        p.processBlock (buf, mb);
        out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + std::min (512, total - pos));
    }
    p.setPlayHead (nullptr);
    return out;
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    std::setvbuf (stdout, nullptr, _IONBF, 0);

    std::printf ("\nT1 Divisor: afinación de cada nota y las octavas en fase\n");
    {
        Engine e; e.prepare (fs, 512); Params p; e.set (p);
        double worst = 0; int worstNote = 0;
        for (int n = 36; n <= 84; ++n)
        {
            const double want = 440.0 * std::pow (2.0, (n - 69) / 12.0), c = 1200.0 * std::log2 (e.noteHz (n) / want);
            if (std::fabs (c) > std::fabs (worst)) { worst = c; worstNote = n; }
        }
        std::printf ("  el peor desvío del temperamento igual: %+.2f cents (nota %d); A4 = %.3f Hz\n", worst, worstNote, e.noteHz (69));
        check (std::fabs (worst) < 2.0 && std::fabs (e.noteHz (69) - 440.0) < 1e-6, "los divisores enteros dejan cada nota a menos de 2 cents; A4 = 440");
        // Medido en la salida: A3 en cuerdas (4') suena a 440 Hz.
        auto o = renderL (stringsOnly(), { { 0, 57, 100 } }, (int) fs);
        dh::OnePoleLP lp; lp.setCutoff (700.0f, (float) fs); for (auto& x : o) x = lp.process (x);
        for (int k = 0; k < 3; ++k) { lp.reset(); for (auto& x : o) x = lp.process (x); }
        const double hz = zeroCrossHz (o, 9600, o.size());
        std::printf ("  A3 en las cuerdas (4'): %.2f Hz\n", hz);
        check (std::fabs (hz - 440.0) < 0.5, "la tecla A3 da 440 Hz por la salida de 4' (el original se ajusta así, a 442)");
        // Tune ±50 cents.
        Engine t; t.prepare (fs, 512); p.tuneCents = 50.0f; t.set (p);
        check (std::fabs (1200.0 * std::log2 (t.noteHz (69) / 440.0) - 50.0) < 1e-3, "Tune mueve el maestro (+50 cents)");
    }

    std::printf ("\nT2 Formas de onda: 8' con carga exponencial, 4' truncada (medio período plana)\n");
    {
        Wave w;
        int flat = 0, N = 1000; float maxStep = 0, prev = w.four (0.0f, 1e-6f);
        for (int i = 0; i < N; ++i) { const float x = w.four ((i + 0.5f) / N, 1e-6f); if (std::fabs (x) < 1e-6f) ++flat; if (i) maxStep = std::max (maxStep, std::fabs (x - prev)); prev = x; }
        const float mid8 = w.eight (0.5f, 1e-6f);
        std::printf ("  4': %.0f %% del período en cero; 8': a mitad del período vale %.3f (rampa lineal: 0,5)\n", 100.0 * flat / N, mid8);
        check (flat == N / 2 && mid8 > 0.6f && mid8 < 0.8f, "formas del AGH17: 4' plana la mitad, 8' cóncava");
        const float l2 = levelOf (kLevel8, 36), l5 = levelOf (kLevel8, 72), f2 = levelOf (kLevel4, 36), f5 = levelOf (kLevel4, 72);
        std::printf ("  niveles medidos: 8' C2 %.1f V, C5 %.1f V; 4' C2 %.1f V, C5 %.1f V\n", l2, l5, f2, f5);
        check (l2 > l5 && std::fabs (f5 - 1.6f) < 1e-6f, "el 8' baja con la octava, el 4' queda parejo");
        // Aliasing: una C6 a 4' (2 kHz) con BLEP; lo que cae entre armónicos queda muy abajo.
        Params p = stringsOnly(); p.strTone = 1.0f;
        auto o = renderL (p, { { 0, 84, 100 } }, (int) fs);
        const double f0 = 2.0 * 440.0 * std::pow (2.0, (84 - 69) / 12.0);
        double h = 0, a = 0;
        for (int k = 1; k <= 8; ++k) h = std::max (h, tone (o, f0 * k, 12000, 36000));
        for (double f : { f0 * 1.5, f0 * 2.5, f0 * 3.37 }) a = std::max (a, tone (o, f, 12000, 36000));
        std::printf ("  C6 a 4': entre armónicos %.0f dB por debajo del armónico más fuerte\n", 10 * std::log10 (h / (a + 1e-30)));
        check (10 * std::log10 (h / (a + 1e-30)) > 50.0, "PolyBLEP/BLAMP: aliasing > 50 dB abajo");
    }

    std::printf ("\nT3 Envolventes: clásico (una sola, se redispara) y poly (extra); release\n");
    {
        auto level = [] (const std::vector<float>& x, double t) { return rmsDb (x, (size_t) (t * fs), (size_t) ((t + 0.01) * fs)); };
        Params p; p.strEns = false; p.strAttack = 0.01f; p.release = 1.0f;
        p.keying = kKeyingClassic;
        const std::vector<Ev> ev { { 0, 57, 100 }, { (int) (0.5 * fs), 64, 100 } };
        const auto c = renderL (p, ev, (int) fs);
        p.keying = kKeyingPoly;
        const auto q = renderL (p, ev, (int) fs);
        std::printf ("  segunda tecla a 0,5 s: clásico %.1f dB a 0,52 s (antes %.1f); poly %.1f dB (antes %.1f)\n", level (c, 0.52), level (c, 0.45), level (q, 0.52), level (q, 0.45));
        check (level (c, 0.52) < level (c, 0.45) - 20.0, "clásico: la tecla nueva descarga la envolvente común 50 ms (todo el acorde vuelve a atacar)");
        check (level (q, 0.52) > level (q, 0.45) - 1.0, "poly: la tecla nueva no toca a las que suenan");
        // Primera nota en clásico: 50 ms de espera (la descarga de la hoja de ajuste, TP7A).
        check (level (c, 0.02) < -80.0 && level (c, 0.09) > -40.0, "clásico: la primera nota entra después de la descarga de 50 ms");
        // Release: a -40 dB en el tiempo pedido (±15 %).
        Params r = stringsOnly(); r.release = 2.0f;
        const auto o = renderL (r, { { 0, 60, 100 }, { (int) (0.5 * fs), 60, 0 } }, (int) (3.5 * fs));
        const double ref = level (o, 0.4); double t40 = -1;
        for (double t = 0.5; t < 3.4; t += 0.005) if (level (o, t) < ref - 40.0) { t40 = t - 0.5; break; }
        std::printf ("  Release 2,0 s: -40 dB a los %.2f s\n", t40);
        check (t40 > 1.6 && t40 < 2.3, "el release llega a -40 dB en el tiempo de la perilla");
    }

    std::printf ("\nT4 Coro: el banco de 7 formantes y las tres voces\n");
    {
        auto spectrum = [] (Params p, int note) {
            auto o = renderL (p, { { 0, note, 100 } }, (int) (1.2 * fs));
            double lowE = 0, midE = 0, f0 = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            for (int k = 1; k * f0 < 4000; ++k) { const double f = k * f0, e = tone (o, f, 24000, 57600); if (f < 750) lowE += e; else if (f < 1600) midE += e; }
            return 10 * std::log10 (midE / (lowE + 1e-30));
        };
        Params m8; m8.strUp = m8.strLo = false; m8.chM8Up = m8.chM8Lo = true; m8.chEns = false; m8.vibDepth = 0; m8.keying = kKeyingPoly; m8.chAttack = 0.005f;
        Params f4 = m8; f4.chM8Up = f4.chM8Lo = false; f4.chFemUp = true;
        const double a = spectrum (m8, 60), b = spectrum (f4, 60);
        std::printf ("  energía 750-1600 Hz respecto de < 750 Hz: masculina 8' %.1f dB, femenina 4' %.1f dB\n", a, b);
        check (b > a + 6.0, "la voz femenina tiene sus formantes más arriba que la masculina");
        // Las llaves respetan la división: la femenina 4' sólo arriba.
        const auto lo = renderL (f4, { { 0, 48, 100 } }, (int) (0.5 * fs));
        check (rmsDb (lo, 4800, lo.size()) < -100.0, "femenina 4' no suena en la mitad de abajo");
    }

    std::printf ("\nT5 Vocoder: diez bandas en sus centros, Hold, Freeze y las consonantes\n");
    {
        // Un seno en el centro de cada banda como voz y un acorde como portador: la salida se concentra ahí.
        Params p; p.strUp = p.strLo = false; p.vocUp = p.vocLo = true; p.vocEns = false; p.vibDepth = 0; p.consonants = 0; p.keying = kKeyingPoly; p.vocTone = 1.0f;
        int good = 0;
        for (int b = 0; b < kBands; ++b)
        {
            const int total = (int) (0.6 * fs);
            std::vector<float> mic ((size_t) total);
            for (int i = 0; i < total; ++i) mic[(size_t) i] = 0.3f * (float) std::sin (2 * dh::kPiD * kVocHz[b] * i / fs);
            Engine e; e.prepare (fs, 512); std::vector<float> L, R;
            render (e, p, { { 0, 36, 100 }, { 0, 43, 100 }, { 0, 48, 100 } }, total, 512, L, R, &mic);
            int best = 0; double bestE = 0;
            for (int k = 0; k < kBands; ++k) if (e.bandEnv (k) > bestE) { bestE = e.bandEnv (k); best = k; }
            if (best == b) ++good;
        }
        std::printf ("  %d de 10 bandas responden a un seno en su centro (200 ... 6000 Hz)\n", good);
        check (good == 10, "los centros de la tabla de ajuste");
        // Hold: la voz se corta a 0,5 s; sin Hold la banda cae en décimas, con Hold dura segundos, con Freeze queda.
        auto afterCut = [&] (int hold) {
            const int total = (int) (3.0 * fs);
            auto mic = testVoice (total); for (int i = (int) (0.5 * fs); i < total; ++i) mic[(size_t) i] = 0.0f;
            Params q = p; q.hold = hold;
            Engine e; e.prepare (fs, 512); std::vector<float> L, R;
            render (e, q, { { 0, 48, 100 } }, (int) (0.5 * fs), 512, L, R, &mic);
            const float at = e.bandEnv (2);
            std::vector<float> L2, R2; std::vector<float> rest (mic.begin() + (int) (0.5 * fs), mic.end());
            render (e, q, {}, (int) (2.0 * fs), 512, L2, R2, &rest);
            return 20 * std::log10 ((e.bandEnv (2) + 1e-12) / (at + 1e-12));
        };
        const double off = afterCut (kHoldOff), on = afterCut (kHoldOn), fr = afterCut (kHoldFreeze);
        std::printf ("  banda 3, 2 s después de cortar la voz: sin Hold %.0f dB, Hold %.1f dB, Freeze %.1f dB\n", off, on, fr);
        check (off < -60.0 && on > -12.0 && on < -2.0 && std::fabs (fr) < 0.01, "Hold alarga la descarga a ~2,5 s; Freeze no la deja bajar");
        // Consonantes: la /s/ pasa sólo con una tecla apretada (detector de sonido musical).
        auto sib = [&] (bool key) {
            const int total = (int) (1.0 * fs);
            std::vector<float> mic ((size_t) total, 0.0f); dh::Rng r (3);
            for (int i = (int) (0.3 * fs); i < (int) (0.7 * fs); ++i) mic[(size_t) i] = 0.2f * r.bipolar();
            Params q = p; q.consonants = 1.0f; q.vocLevel = 0.0f;
            std::vector<Ev> ev; if (key) ev.push_back ({ 0, 60, 100 });
            return rmsDb (renderL (q, ev, total, 512, &mic), (size_t) (0.4 * fs), (size_t) (0.6 * fs));
        };
        const double withKey = sib (true), without = sib (false);
        std::printf ("  ruido agudo en la voz: %.1f dB con una tecla, %.1f dB sin teclas\n", withKey, without);
        check (withKey > -45.0 && without < withKey - 40.0, "el paso de las consonantes sólo se abre con portador");
    }

    std::printf ("\nT6 Ensemble: los cuatro LFO medidos (3,9 s y 4,6 s triangulares, 150 y 175 ms senoidales)\n");
    {
        Params p; p.release = 0.05f;
        Engine e; e.prepare (fs, 512); e.set (p); e.noteOn (60, 100);     // con una nota: en reposo los LFO no corren
        const int total = (int) (20.0 * fs);
        std::vector<float> d0 ((size_t) total), d1 ((size_t) total); float l, r;
        for (int i = 0; i < total; ++i) { e.process (&l, &r, nullptr, nullptr, 1); d0[(size_t) i] = e.lastEnsMs[0]; d1[(size_t) i] = e.lastEnsMs[1]; }
        // El rápido: cruces por cero del retardo menos su media móvil de 50 ms; el lento: cruces de la media móvil de 0,5 s.
        // Media móvil centrada (sin el retraso que dejaría un escalón sobre las rampas del triángulo).
        auto split = [] (const std::vector<float>& d, int win) {
            std::vector<double> cs (d.size() + 1, 0.0); for (size_t i = 0; i < d.size(); ++i) cs[i + 1] = cs[i] + d[i];
            std::vector<float> m (d.size());
            for (size_t i = 0; i < d.size(); ++i) { const size_t a = i >= (size_t) (win / 2) ? i - (size_t) (win / 2) : 0, b = std::min (d.size(), i + (size_t) (win / 2) + 1); m[i] = (float) ((cs[b] - cs[a]) / (double) (b - a)); }
            return m; };
        double fast[2], slow[2];
        for (int ch = 0; ch < 2; ++ch)
        {
            const auto& d = ch ? d1 : d0;
            auto lp = split (d, (int) (0.5 * fs));
            std::vector<float> hi (d.size()), lo (d.size());
            for (size_t i = 0; i < d.size(); ++i) { hi[i] = d[i] - lp[i]; lo[i] = lp[i] - kEnsBaseMs; }
            fast[ch] = 1.0 / zeroCrossHz (hi, (size_t) fs, d.size());
            slow[ch] = 1.0 / zeroCrossHz (lo, (size_t) (2 * fs), d.size());
        }
        std::printf ("  canal 1: lento %.2f s, rápido %.0f ms; canal 2: lento %.2f s, rápido %.0f ms\n", slow[0], fast[0] * 1000, slow[1], fast[1] * 1000);
        check (std::fabs (fast[0] - 0.150) < 0.01 && std::fabs (fast[1] - 0.175) < 0.01 && std::fabs (slow[0] - 3.9) < 0.3 && std::fabs (slow[1] - 4.6) < 0.3,
               "los períodos de la hoja de ajuste del ETH09");
        // Estéreo: con el ensemble las dos salidas son distintas; sin él, iguales.
        std::vector<float> L, R; Engine s; s.prepare (fs, 512);
        render (s, p, { { 0, 57, 100 }, { 0, 64, 100 } }, (int) (2 * fs), 512, L, R);
        double corr = 0, eL = 0, eR = 0; for (size_t i = 48000; i < L.size(); ++i) { corr += L[i] * R[i]; eL += L[i] * L[i]; eR += R[i] * R[i]; }
        std::printf ("  correlación L/R de las cuerdas con ensemble: %.2f\n", corr / std::sqrt (eL * eR));
        check (corr / std::sqrt (eL * eR) < 0.95, "el ensemble abre el estéreo");
    }

    std::printf ("\nT7 Vibrato (BBD): velocidad 4,5-11 Hz y su retardo de entrada\n");
    {
        for (float rate : { 4.5f, 11.0f })
        {
            Params p; p.vibRate = rate; p.vibDepth = 1.0f; p.vibDelay = 0.0f;
            Engine e; e.prepare (fs, 512); e.set (p); e.noteOn (60, 100);
            const int total = (int) (3 * fs); std::vector<float> d ((size_t) total); float l, r;
            for (int i = 0; i < total; ++i) { e.process (&l, &r, nullptr, nullptr, 1); d[(size_t) i] = e.lastVibMs - kVibBaseMs; }
            const double hz = zeroCrossHz (d, (size_t) (0.5 * fs), d.size());
            std::printf ("  Rate %.1f Hz: medido %.2f Hz\n", rate, hz);
            check (std::fabs (hz - rate) < 0.05, "la velocidad pedida");
        }
        Params p; p.vibDepth = 1.0f; p.vibDelay = 2.0f;
        Engine e; e.prepare (fs, 512); e.set (p); e.noteOn (60, 100);
        float l, r, a1 = 0, a2 = 0;
        for (int i = 0; i < (int) (3 * fs); ++i) { e.process (&l, &r, nullptr, nullptr, 1); const float x = std::fabs (e.lastVibMs - kVibBaseMs); if (i < (int) (0.5 * fs)) a1 = std::max (a1, x); else if (i > (int) (2.2 * fs)) a2 = std::max (a2, x); }
        std::printf ("  Delay 2 s: excursión en el primer medio segundo %.3f ms, después de 2,2 s %.3f ms\n", a1, a2);
        check (a1 < 0.3f * a2, "el vibrato entra de a poco en el tiempo de Delay");
    }

    std::printf ("\nT8 Pitch Shift: Auto arranca abajo y sube; Manual sólo baja\n");
    {
        Params p; p.pitchMode = kPitchAuto; p.pitchSet = 12.0f; p.pitchTime = 0.3f;
        Engine e; e.prepare (fs, 512); e.set (p); e.noteOn (60, 100);
        float l, r; e.process (&l, &r, nullptr, nullptr, 1);
        const float start = e.currentPitchOffset();
        for (int i = 0; i < (int) (0.3 * fs); ++i) e.process (&l, &r, nullptr, nullptr, 1);
        const float after = e.currentPitchOffset();
        std::printf ("  Auto, Set 12: %.2f semitonos al tocar, %.2f a los 300 ms\n", start, after);
        check (start < -11.9f && after > -0.7f && after < 0.0f, "una octava abajo y de vuelta en el tiempo de Time");
    }

    std::printf ("\nT9 Determinismo: el mismo resultado con cualquier tamaño de bloque, y después de restart()\n");
    {
        Params p; p.chM8Up = p.chFemUp = p.chM4Lo = true; p.vocUp = p.vocLo = true; p.spaceMix = 0.3f; p.vibDelay = 0.1f;
        const int total = (int) (3 * fs);
        const auto mic = testVoice (total, 1.0, 1.4);
        const std::vector<Ev> ev { { 100, 45, 90 }, { 2000, 57, 100 }, { 24000, 64, 80 }, { 70000, 57, 0 }, { 90000, 72, 100 }, { 120000, 45, 0 } };
        Engine e1, e2, e3; e1.prepare (fs, 512); e2.prepare (fs, 512); e3.prepare (fs, 512);
        std::vector<float> a, ar, b, br, c, cr;
        render (e1, p, ev, total, 1, a, ar, &mic); render (e2, p, ev, total, 512, b, br, &mic); render (e3, p, ev, total, 0, c, cr, &mic);
        check (std::memcmp (a.data(), b.data(), a.size() * 4) == 0 && std::memcmp (a.data(), c.data(), a.size() * 4) == 0 && std::memcmp (ar.data(), cr.data(), a.size() * 4) == 0,
               "bloques de 1, 512 y al azar: idéntico bit a bit");
        e3.restart(); std::vector<float> d, dr; render (e3, p, ev, total, 512, d, dr, &mic);
        check (std::memcmp (b.data(), d.data(), b.size() * 4) == 0, "restart() deja el motor como recién preparado");
    }

    std::printf ("\nT10 Robustez: parámetros al azar y extremos, sin NaN, acotado, sin DC\n");
    {
        bool ok = true; float peak = 0; double worstDc = 0; uint32_t s = 3;
        auto rnd = [&] { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) / 16777216.0f; };
        for (int t = 0; t < 30; ++t)
        {
            Params p;
            p.strUp = rnd() > 0.3f; p.strLo = rnd() > 0.3f; p.chFemUp = rnd() > 0.5f; p.chM8Up = rnd() > 0.5f; p.chM4Lo = rnd() > 0.5f; p.chM8Lo = rnd() > 0.5f;
            p.vocUp = rnd() > 0.5f; p.vocLo = rnd() > 0.5f; p.strEns = rnd() > 0.2f;
            p.strAttack = 0.005f + 4 * rnd() * rnd(); p.chAttack = 0.005f + rnd(); p.release = 0.02f + 8 * rnd() * rnd();
            p.strTone = rnd(); p.vocTone = rnd(); p.micGain = 0.06f + 15 * rnd(); p.consonants = rnd(); p.directMic = rnd();
            p.vocResponse = 0.003f + 0.3f * rnd(); p.formantShift = 2 * rnd() - 1; p.hold = t % 3;
            p.vibRate = 4.5f + 6.5f * rnd(); p.vibDepth = rnd(); p.vibDelay = 3 * rnd(); p.pitchMode = t % 3; p.pitchSet = 15 * rnd(); p.pitchTime = 0.01f + 2 * rnd();
            p.pitchAmount = rnd(); p.ensDepth = 1.5f * rnd(); p.keying = t % 2; p.velocity = rnd(); p.spaceMix = rnd(); p.spaceSize = rnd(); p.tuneCents = 100 * rnd() - 50;
            std::vector<Ev> ev;
            for (int k = 0; k < 40; ++k) ev.push_back ({ (int) (rnd() * 1.8f * (float) fs), 24 + (int) (rnd() * 80), rnd() > 0.4f ? 1 + (int) (rnd() * 126) : 0 });
            std::sort (ev.begin(), ev.end(), [] (const Ev& a, const Ev& b) { return a.at < b.at; });
            const int total = (int) (2.5 * fs);
            auto mic = testVoice (total, 0.5, 0.9);
            for (auto& x : mic) x *= 3.0f;
            const auto o = renderL (p, ev, total, 0, &mic);
            double dc = 0;
            for (float x : o) { ok &= dh::isFiniteF (x); peak = std::max (peak, std::fabs (x)); dc += x; }
            worstDc = std::max (worstDc, std::fabs (dc / (double) o.size()));
        }
        std::printf ("  pico máximo %.2f, peor DC %.4f\n", peak, worstDc);
        check (ok, "sin NaN ni Inf en 30 configuraciones al azar");
        check (peak < 8.0f && worstDc < 0.01, "salida acotada y sin DC");
    }

    std::printf ("\nT11 CPU (48 kHz, estéreo)\n");
    {
        auto bench = [&] (Params p, int notes, const char* what, bool voice) {
            std::vector<Ev> ev; for (int k = 0; k < notes; ++k) ev.push_back ({ 0, 36 + (k * 5) % 48, 100 });
            const int total = (int) (10 * fs);
            const auto mic = testVoice (total);
            const auto t0 = std::chrono::steady_clock::now();
            renderL (p, ev, total, 512, voice ? &mic : nullptr);
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  %-52s %.2f %% de un núcleo\n", what, secs * 10.0);
            return secs * 10.0;
        };
        Params a; bench (a, 8, "cuerdas, acorde de 8 notas", false);
        Params b; b.chFemUp = b.chM8Up = b.chM4Lo = b.chM8Lo = true; b.vocUp = b.vocLo = true; b.spaceMix = 0.3f;
        const double worst = bench (b, 16, "todo prendido, 16 notas, vocoder con voz, reverb", true);
        bench (b, 49, "todo prendido, las 49 teclas", true);
        check (worst < 25.0, "16 notas con todo prendido: menos de un cuarto de núcleo");
    }

    // ── El procesador completo ──
    std::printf ("\nT12 Hueco del host: el note-off perdido mientras el host no llamaba no deja la nota colgada\n");
    {
        auto run = [&] (bool gap, bool playing) {
            At330Processor p; TestHead h; h.playing = playing;
            p.setPlayHead (&h); p.setPlayConfigDetails (2, 2, fs, 512); p.prepareToPlay (fs, 512);
            p.setValue ("release", 0.1f);
            const juce::int64 n = 512, tOn = 24000, tOff = 72000, g0 = 48000, g1 = 96000, end = 192000;
            std::vector<float> o; juce::AudioBuffer<float> buf (2, (int) n); juce::MidiBuffer mb; bool slept = false;
            for (juce::int64 pos = 0; pos < end; pos += n)
            {
                buf.clear();
                if (! (gap && pos >= g0 && pos < g1))
                {
                    if (gap && ! playing && pos >= g1 && ! slept) { juce::Thread::sleep (600); slept = true; }
                    mb.clear();
                    if (tOn >= pos && tOn < pos + n) mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), (int) (tOn - pos));
                    if (tOff >= pos && tOff < pos + n) mb.addEvent (juce::MidiMessage::noteOff (1, 60), (int) (tOff - pos));
                    h.pos = playing ? pos : 0;
                    p.processBlock (buf, mb);
                }
                o.insert (o.end(), buf.getReadPointer (0), buf.getReadPointer (0) + n);
            }
            return std::pair<double, double> { rmsDb (o, 30000, 46000), rmsDb (o, 168000, 192000) };
        };
        for (bool playing : { true, false })
        {
            const auto ctl = run (false, playing), gp = run (true, playing);
            std::printf ("  transporte %s: nota %.0f dB; al final %.0f dB con el note-off perdido\n", playing ? "andando" : "parado", ctl.first, gp.second);
            check (ctl.first > -40.0 && gp.second < -90.0, "la nota se suelta al volver el host");
        }
    }

    std::printf ("\nT13 Programa 0: un plugin recién insertado es el preset 0, byte a byte\n");
    {
        At330Processor fresh, viaProgram; viaProgram.setCurrentProgram (0);
        juce::MemoryBlock sf, sp; fresh.getStateInformation (sf); viaProgram.getStateInformation (sp);
        const auto& p0 = fresh.getPresets().front();
        std::printf ("  %d presets; programa %d \"%s\"; estado %d bytes\n", (int) fresh.getPresets().size(), fresh.getCurrentProgram(),
                     fresh.getProgramName (0).toRawUTF8(), (int) sf.getSize());
        check (fresh.getProgramName (fresh.getCurrentProgram()) == p0.name && fresh.presetDisplayName() == p0.name, "el host y el editor dicen el nombre del preset 0");
        check (sf == sp, "estado recién insertado == estado después de setCurrentProgram (0)");
        At330Processor r1, r2; r2.setCurrentProgram (0);
        const auto a = renderProc (r1), b = renderProc (r2);
        check (std::memcmp (a.data(), b.data(), a.size() * 4) == 0, "suena igual que después de elegir el programa 0 (bit a bit)");
        // Guardar, restaurar y volver a guardar: los mismos bytes.
        At330Processor saved; saved.setCurrentProgram (7); saved.setValue ("str_tone", 33.0f);
        juce::MemoryBlock ss; saved.getStateInformation (ss);
        At330Processor restored; restored.setStateInformation (ss.getData(), (int) ss.getSize());
        juce::MemoryBlock s2; restored.getStateInformation (s2);
        check (s2 == ss && restored.getCurrentProgram() == 7 && std::fabs (restored.value ("str_tone") - 33.0f) < 1e-3f && restored.presetDisplayName().endsWith ("*"),
               "una sesión se restaura con su programa, sus valores y la marca de modificado, byte a byte");
    }

    std::printf ("\nT14 reset() del host: suena igual que una instancia nueva\n");
    {
        At330Processor a, b;
        renderProc (b, 2.0, true);           // b queda con el ensemble, la reverb y las bandas en otro estado
        b.reset();
        // renderProc vuelve a preparar; para probar reset() solo, se procesa sin preparar de nuevo.
        auto play = [] (At330Processor& p, bool prepare) {
            TestHead h; p.setPlayHead (&h);
            if (prepare) { p.setPlayConfigDetails (2, 2, fs, 512); p.prepareToPlay (fs, 512); }
            std::vector<float> out; juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer mb;
            for (int pos = 0; pos < (int) (2 * fs); pos += 512)
            {
                buf.clear(); mb.clear();
                if (pos == 0) mb.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0);
                p.processBlock (buf, mb);
                out.insert (out.end(), buf.getReadPointer (0), buf.getReadPointer (0) + 512);
            }
            p.setPlayHead (nullptr);
            return out;
        };
        const auto x = play (a, true), y = play (b, false);
        check (x.size() == y.size() && std::memcmp (x.data(), y.data(), x.size() * 4) == 0, "después de reset() el mismo acorde da los mismos bits que una instancia nueva");
    }

    std::printf ("\nT15 Parámetros suavizados: mover una perilla de golpe no hace clic\n");
    {
        At330Processor p; TestHead h; p.setPlayHead (&h); p.setPlayConfigDetails (2, 2, fs, 512); p.prepareToPlay (fs, 512);
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer mb; float worst = 0, base = 0; float prev = 0;
        for (int pos = 0; pos < (int) (2 * fs); pos += 512)
        {
            buf.clear(); mb.clear();
            if (pos == 0) mb.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 100), 0);
            if (pos == 512 * 60) { p.setValue ("str_level", 0.0f); p.setValue ("volume", -36.0f); p.setValue ("str_tone", 0.0f); }
            p.processBlock (buf, mb);
            for (int i = 0; i < 512; ++i)
            {
                const float x = buf.getSample (0, i), dlt = std::fabs (x - prev); prev = x;
                if (pos >= 512 * 40 && pos < 512 * 60) base = std::max (base, dlt);
                if (pos >= 512 * 60 && pos < 512 * 62) worst = std::max (worst, dlt);
            }
        }
        std::printf ("  mayor salto entre muestras: %.4f sonando, %.4f al bajar nivel, tono y volumen de golpe\n", base, worst);
        check (worst <= base * 1.05f, "el cambio no agrega saltos mayores que los de la forma de onda");
    }

    std::printf ("\nT16 Presets: niveles, DC y picos (acorde de 6 notas 3 s; los de vocoder con voz)\n");
    {
        At330Processor probe; const int np = (int) probe.getPresets().size();
        std::vector<double> lv; double worstDc = 0; float worstPk = 0; juce::String worstPkName, worstDcName;
        for (int i = 0; i < np; ++i)
        {
            At330Processor p; p.setCurrentProgram (i);
            const bool voice = (bool) p.getPresets()[(size_t) i].data.getProperty ("voice", false);
            const auto x = renderProc (p, 4.5, voice);
            double m = 0; float pk = 0; for (size_t k = 0; k < x.size(); ++k) { m += x[k]; pk = std::max (pk, std::fabs (x[k])); }
            m /= (double) x.size();
            if (std::fabs (m) > std::fabs (worstDc)) { worstDc = m; worstDcName = p.getPresets()[(size_t) i].name; }
            if (pk > worstPk) { worstPk = pk; worstPkName = p.getPresets()[(size_t) i].name; }
            lv.push_back (rmsDb (x, (size_t) (0.5 * fs), (size_t) (3 * fs)));
        }
        auto sorted = lv; std::sort (sorted.begin(), sorted.end());
        const double med = sorted[sorted.size() / 2];
        const int loud = (int) (std::max_element (lv.begin(), lv.end()) - lv.begin()), quiet = (int) (std::min_element (lv.begin(), lv.end()) - lv.begin());
        std::printf ("  %d presets: mediana %.1f dBFS RMS; el más fuerte %s %+.1f, el más bajo %s %+.1f; pico %.2f dBFS (%s); DC %.5f (%s)\n", np, med,
                     probe.getPresets()[(size_t) loud].name.toRawUTF8(), lv[(size_t) loud] - med, probe.getPresets()[(size_t) quiet].name.toRawUTF8(), lv[(size_t) quiet] - med,
                     20 * std::log10 (worstPk + 1e-9), worstPkName.toRawUTF8(), worstDc, worstDcName.toRawUTF8());
        check (np >= 40, "al menos 40 presets");
        check (std::fabs (worstDc) < 0.002 && worstPk <= std::pow (10.0f, -1.0f / 20.0f), "ninguno con DC ni pasado de -1 dBFS");
        check (lv[(size_t) loud] - med <= 8.0 && med - lv[(size_t) quiet] <= 12.0, "ninguno a más de 8 dB arriba ni 12 dB abajo de la mediana");
    }

    std::printf ("\n%s (%d fallas)\n", fails ? "HAY FALLAS" : "TODO OK", fails);
    return fails ? 1 : 0;
}
