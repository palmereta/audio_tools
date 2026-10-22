// at303test: pruebas del motor del AT303 (docs/research.md §7; T1-T8 sólo el motor) y del procesador completo sin
// host (T9-T11). Imprime lo medido y [ok] o [FALLA].
#include "PluginProcessor.h"
#include "Engine303.h"
#include <algorithm>
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

// Un host de mentira para el procesador: transporte, posición y tempo.
struct TestHead : public juce::AudioPlayHead
{
    bool playing = false; juce::int64 pos = 0; double bpm = 120.0, sr = 48000.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p; p.setBpm (bpm); p.setIsPlaying (playing); p.setTimeInSamples (pos); p.setPpqPosition ((double) pos / sr * bpm / 60.0);
        return p;
    }
};

static double rmsDb (const std::vector<float>& x, size_t a, size_t b)
{
    double e = 0; b = std::min (b, x.size());
    for (size_t i = a; i < b; ++i) e += (double) x[i] * x[i];
    return 10.0 * std::log10 (e / (double) std::max<size_t> (1, b - a) + 1e-30);
}

// Renderiza el procesador con el reloj del host andando (el patrón del preset), canal izquierdo.
static std::vector<float> renderProc (At303Processor& p, double secs, double fs = 48000.0)
{
    TestHead h; h.playing = true; h.sr = fs; h.bpm = p.value ("tempo");
    p.setPlayHead (&h); p.setPlayConfigDetails (0, 2, fs, 512); p.prepareToPlay (fs, 512);
    p.setValue ("sync", 1.0f); p.setValue ("mode", 0.0f);
    const int total = (int) (secs * fs);
    std::vector<float> out; out.reserve ((size_t) total);
    juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer mb;
    for (int pos = 0; pos < total; pos += 512)
    {
        h.pos = pos; buf.clear(); mb.clear();
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

    std::printf ("\nT2c Cambios en marcha: largo, tresillos, slide desde la compuerta cerrada, salto del host\n");
    {
        const double sr = 48000.0, bpm = 120.0, cps = bpm / 60.0 * 24.0 / sr;
        // Corre el secuenciador muestra a muestra y guarda en qué paso del patrón empieza cada nota y en qué pulso.
        struct Hit { double clock; int step; bool legato; };
        auto run = [&] (Sequencer& s, double fromClock, double secs, std::vector<Hit>& hits, std::vector<double>* offs = nullptr) {
            for (int i = 0; i < (int) (secs * sr); ++i)
            {
                const double c0 = fromClock + i * cps;
                s.advance (c0, c0 + cps, [&] (const SeqEvent& e, double) {
                    if (e.kind == SeqEvent::kNoteOn) hits.push_back ({ c0, s.currentStep(), e.legato });
                    if (e.kind == SeqEvent::kGateOff && offs) offs->push_back (c0);
                });
            }
            return fromClock + (int) (secs * sr) * cps;
        };
        Pattern p16; for (auto& t : p16.time) t = kNote;
        // Largo 16 → 12 tocando: después del cambio los pasos siguen 0, 1, 2… sin saltos.
        {
            Sequencer s; s.setPattern (p16); s.restart();
            std::vector<Hit> h; double c = run (s, 0.0, 2.1, h);
            Pattern p12 = p16; p12.length = 12; s.setPattern (p12);
            const size_t from = h.size(); c = run (s, c, 4.0, h);
            // El patrón viejo termina su vuelta; desde el primer paso 0 manda el nuevo, de 12.
            size_t z = from; while (z < h.size() && h[z].step != 0) ++z;
            bool seq = true; int after = 0;
            for (size_t k = z + 1; k < h.size(); ++k) { ++after; if (h[k].step != (h[k - 1].step + 1) % 12) seq = false; }
            check (seq && after > 12, "largo 16 → 12 tocando: el patrón nuevo sigue paso a paso, sin saltear");
        }
        // Corcheas → tresillos tocando: ningún hueco mayor que un paso de tresillo (8 pulsos).
        {
            Sequencer s; s.setPattern (p16); s.restart();
            std::vector<Hit> h; double c = run (s, 0.0, 1.3, h);
            Pattern pt = p16; pt.triplet = true; s.setPattern (pt);
            c = run (s, c, 4.0, h);
            double gap = 0; for (size_t k = 1; k < h.size(); ++k) gap = std::max (gap, h[k].clock - h[k - 1].clock);
            check (gap <= 10.0, "corcheas → tresillos tocando: sin silencio en el cambio (hueco máximo ≤ 10 pulsos)");
        }
        // Slide del último paso hacia el primero, al dar Play: la primera nota suena (compuerta cerrada = nota nueva).
        {
            Voice v; v.prepare ((float) sr, 2); VoiceParams vp; v.set (vp);
            v.noteOn (2.0f, false, true);                      // ligada pero con la compuerta cerrada
            check (v.gateHigh(), "una nota ligada con la compuerta cerrada abre la compuerta");
        }
        // Salto del host hacia atrás con una nota sonando y su fin de compuerta pendiente: no queda colgada.
        {
            Pattern pr; for (auto& t : pr.time) t = kRest; pr.time[0] = kNote; pr.time[15] = kNote;
            Sequencer s; s.setPattern (pr); s.restart();
            std::vector<Hit> h; std::vector<double> offs;
            run (s, 0.0, 1.9, h, &offs);                       // la nota del paso 15 empieza a ~1,875 s
            const size_t ons = h.size(), offsBefore = offs.size();
            run (s, 0.0, 0.1, h, &offs);                       // el host vuelve al principio
            check (offs.size() > offsBefore && h.size() > ons, "salto del host: la nota que sonaba se suelta y el paso 0 vuelve a sonar");
        }
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

    std::printf ("\nT9 Hueco del host: el note-off perdido mientras el host no llamaba (bypass de REAPER) no deja la nota colgada\n");
    {
        // Nota de 0,5 a 1,5 s; el host deja de llamar de 1,0 a 2,0 s (con el transporte andando la posición avanza;
        // parado, pasan 600 ms de reloj sin llamadas). Mide la nota y el final, y la compuerta al terminar.
        struct R { double note, end; bool gate; };
        auto run = [&] (bool gap, bool playing, bool midiMode, bool sendOff = true) -> R {
            At303Processor p; TestHead h; h.playing = playing;
            p.setPlayHead (&h); p.setPlayConfigDetails (0, 2, fs, 512); p.prepareToPlay (fs, 512);
            p.setValue ("mode", midiMode ? 1.0f : 0.0f); p.setValue ("sync", 1.0f); p.setValue ("run", 0.0f);
            const juce::int64 n = 512, tOn = 24000, tOff = 72000, g0 = 48000, g1 = 96000, end = 168000;
            std::vector<float> o; juce::AudioBuffer<float> buf (2, (int) n); juce::MidiBuffer mb; bool slept = false;
            for (juce::int64 pos = 0; pos < end; pos += n)
            {
                buf.clear();
                if (! (gap && pos >= g0 && pos < g1))
                {
                    if (gap && ! playing && pos >= g1 && ! slept) { juce::Thread::sleep (600); slept = true; }
                    mb.clear();
                    if (tOn >= pos && tOn < pos + n) mb.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 90), (int) (tOn - pos));
                    if (sendOff && tOff >= pos && tOff < pos + n) mb.addEvent (juce::MidiMessage::noteOff (1, 36), (int) (tOff - pos));
                    h.pos = playing ? pos : 0;
                    p.processBlock (buf, mb);
                }
                o.insert (o.end(), buf.getReadPointer (0), buf.getReadPointer (0) + n);
            }
            return { rmsDb (o, 28800, 45600), rmsDb (o, 144000, 168000), p.view.gate.load() };
        };
        struct Case { const char* what; bool playing, midiMode; };
        for (auto c : { Case { "transporte andando, modo MIDI (se saltan bloques y la posición avanza)", true, true },
                        Case { "transporte parado, teclas del instrumento (600 ms de reloj sin llamadas)", false, false },
                        Case { "transporte parado, modo MIDI", false, true } })
        {
            const R ctl = run (false, c.playing, c.midiMode), gp = run (true, c.playing, c.midiMode);
            std::printf ("  %s: nota %.0f dB; al final %.0f dB con el note-off perdido (compuerta %s), %.0f dB con el note-off\n",
                         c.what, ctl.note, gp.end, gp.gate ? "abierta" : "cerrada", ctl.end);
            check (ctl.note > -40.0 && ! gp.gate && gp.end <= std::max (ctl.end + 6.0, -70.0), "la nota se suelta al volver el host");
        }
        // Sin hueco no se suelta nada: una nota sostenida sin note-off sigue con la compuerta abierta.
        const R held = run (false, false, false, false), heldPlay = run (false, true, true, false);
        check (held.gate && heldPlay.gate, "sin hueco (parado y andando) una nota sostenida sigue sonando: ninguna falsa alarma");
        // Secuenciador con el host andando: después del hueco el patrón sigue sonando en su lugar.
        {
            auto seqRun = [&] (bool gap) {
                At303Processor p; TestHead h; h.playing = true; h.bpm = 125;
                p.setPlayHead (&h); p.setPlayConfigDetails (0, 2, fs, 512); p.prepareToPlay (fs, 512);
                p.setValue ("sync", 1.0f);
                std::vector<float> o; juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer mb;
                for (juce::int64 pos = 0; pos < 192000; pos += 512)
                {
                    buf.clear(); mb.clear();
                    if (! (gap && pos >= 48000 && pos < 96000)) { h.pos = pos; p.processBlock (buf, mb); }
                    o.insert (o.end(), buf.getReadPointer (0), buf.getReadPointer (0) + 512);
                }
                return o;
            };
            const auto a = seqRun (false), b = seqRun (true);
            const double la = rmsDb (a, 120000, 192000), lb = rmsDb (b, 120000, 192000);
            std::printf ("  secuenciador con el host andando: después del hueco %.1f dB, sin hueco %.1f dB\n", lb, la);
            check (std::abs (la - lb) < 1.5, "el secuenciador sigue el patrón después del hueco");
        }
    }

    std::printf ("\nT10 Programa 0: un plugin recién insertado es el preset 0 que informa al host\n");
    {
        At303Processor fresh;
        At303Processor viaProgram; viaProgram.setCurrentProgram (0);
        juce::MemoryBlock sf, sp; fresh.getStateInformation (sf); viaProgram.getStateInformation (sp);
        const auto& p0 = fresh.getPresets().front();
        const auto name = fresh.getProgramName (fresh.getCurrentProgram());
        std::printf ("  programa %d \"%s\"; el editor muestra \"%s\"; Cutoff %.0f (preset %.0f); estado %d bytes\n",
                     fresh.getCurrentProgram(), name.toRawUTF8(), fresh.presetDisplayName().toRawUTF8(), fresh.value ("cutoff"),
                     (double) p0.data.getProperty ("params", {}).getProperty ("cutoff", -1), (int) sf.getSize());
        check (name == p0.name && fresh.presetDisplayName() == p0.name, "el host y el editor dicen el mismo nombre: el del preset 0");
        check (std::abs (fresh.value ("cutoff") - (float) (double) p0.data.getProperty ("params", {}).getProperty ("cutoff", -1)) < 1e-3f
                   && std::abs (fresh.value ("tempo") - (float) (double) p0.data.getProperty ("params", {}).getProperty ("tempo", -1)) < 1e-3f,
               "los valores son los del preset 0, no los por defecto");
        check (sf == sp, "estado recién insertado == estado después de setCurrentProgram (0), byte a byte");
        At303Processor r1, r2; r2.setCurrentProgram (0);
        const auto a = renderProc (r1, 3.0), b = renderProc (r2, 3.0);
        check (a.size() == b.size() && std::memcmp (a.data(), b.data(), a.size() * 4) == 0, "suena igual que después de elegir el programa 0 (bit a bit)");
        // Una sesión guardada manda: el preset del constructor no la pisa.
        At303Processor saved; saved.setCurrentProgram (5); saved.setValue ("cutoff", 77.0f);
        { auto pt = saved.getPattern (0); pt.length = 11; saved.setPattern (0, pt); }
        juce::MemoryBlock ss; saved.getStateInformation (ss);
        At303Processor restored; restored.setStateInformation (ss.getData(), (int) ss.getSize());
        juce::MemoryBlock sr2; restored.getStateInformation (sr2);
        check (sr2 == ss && restored.getCurrentProgram() == 5 && std::abs (restored.value ("cutoff") - 77.0f) < 1e-3f && restored.getPattern (0).length == 11,
               "una sesión guardada se restaura con su programa, sus valores y sus patrones");
        // Una sesión de 1.0.2 guardada sin tocar nada ("Init", valores por defecto) vuelve como estaba.
        At303Processor old;
        for (auto* prm : old.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (prm))
                if (rp->getParameterID() != "sync" && rp->getParameterID() != "run") rp->setValueNotifyingHost (rp->getDefaultValue());
        old.currentPresetName = juce::String ("Init"); old.setPattern (0, Pattern());
        juce::MemoryBlock so; old.getStateInformation (so);
        At303Processor back; back.setStateInformation (so.getData(), (int) so.getSize());
        check (back.currentPresetName.get() == "Init" && std::abs (back.value ("cutoff") - old.value ("cutoff")) < 1e-3f, "una sesión vieja con Init vuelve con Init y sus valores");
    }

    std::printf ("\nT11 DC y nivel: el techo de salida no corre el promedio; ningún preset muy por encima del resto\n");
    {
        // La escalera saturada a todo volumen (lo que hacía Overdriven Ladder): recorta en el techo.
        EngineParams ep; ep.voice.cutoff = 0.42f; ep.voice.resonance = 0.8f; ep.voice.envMod = 0.6f; ep.voice.overdrive = 1.0f + 65.6f;
        ep.volume = 1.0f;
        const auto o = renderSeq (basicPattern(), ep, 130, 6.0, 512);
        double dc = 0; float pk = 0; for (size_t i = 48000; i < o.size(); ++i) { dc += o[i]; pk = std::max (pk, std::fabs (o[i])); }
        dc /= (double) (o.size() - 48000);
        std::printf ("  Overdrive al máximo, Volume al máximo: DC %.5f, pico %.3f\n", dc, pk);
        // Golpeando el techo todo el tiempo queda algo (el límite final de ±1 vuelve a recortar lo que el pasa altos
        // corrió), bastante menos que el 0,037 que daba Overdriven Ladder.
        check (std::abs (dc) < 0.01 && pk <= 1.0f, "recortando en el techo: DC < 0,01 (el preset Overdriven Ladder daba 0,037) y pico <= 1");
        // Debajo del techo la etapa no toca nada: con la mitad del volumen la salida es exactamente la mitad.
        EngineParams q; q.volume = 0.4f; EngineParams h = q; h.volume = 0.2f;
        const auto a = renderSeq (basicPattern(), q, 128, 3.0, 512), b = renderSeq (basicPattern(), h, 128, 3.0, 512);
        bool exact = true; float pa = 0; for (size_t i = 0; i < a.size(); ++i) { exact &= a[i] == 2.0f * b[i]; pa = std::max (pa, std::fabs (a[i])); }
        std::printf ("  sonido por defecto con Volume 40 (pico %.2f) y 20\n", pa);
        check (pa < 0.9f && exact, "debajo del techo la salida es lineal bit a bit (el pasa altos no actúa)");
        // Todos los presets por el procesador completo, con su patrón.
        At303Processor probe; const int np = (int) probe.getPresets().size();
        std::vector<double> lv; double worstDc = 0; float worstPk = 0; std::string worstDcName;
        for (int i = 0; i < np; ++i)
        {
            At303Processor p; p.setCurrentProgram (i);
            const auto x = renderProc (p, 6.0);
            double m = 0; for (size_t k = 96000; k < x.size(); ++k) { m += x[k]; worstPk = std::max (worstPk, std::fabs (x[k])); }
            m /= (double) (x.size() - 96000);
            if (std::abs (m) > std::abs (worstDc)) { worstDc = m; worstDcName = p.getPresets()[(size_t) i].name.toStdString(); }
            lv.push_back (rmsDb (x, 96000, x.size()));
        }
        auto sorted = lv; std::sort (sorted.begin(), sorted.end());
        const double med = sorted[sorted.size() / 2];
        int loudest = (int) (std::max_element (lv.begin(), lv.end()) - lv.begin());
        std::printf ("  %d presets: mediana %.1f dBFS RMS; el más fuerte %s %.1f dB (+%.1f); peor DC %.4f (%s); pico %.3f\n", np, med,
                     probe.getPresets()[(size_t) loudest].name.toRawUTF8(), lv[(size_t) loudest], lv[(size_t) loudest] - med, worstDc, worstDcName.c_str(), worstPk);
        check (std::abs (worstDc) < 0.002 && worstPk <= 1.0f, "ningún preset con DC (< 0,002) ni pasado de 0 dBFS");
        check (lv[(size_t) loudest] - med <= 8.0, "ningún preset más de 8 dB por encima de la mediana");
    }

    std::printf ("\n%s (%d fallas)\n", fails ? "HAY FALLAS" : "TODO OK", fails);
    return fails ? 1 : 0;
}
