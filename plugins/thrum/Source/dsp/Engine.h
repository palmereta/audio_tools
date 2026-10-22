#pragma once
// El motor completo de Thrum, sin JUCE (docs/research.md §8): la guitarra (Instrument), el guitarrista (Performer) y la
// cadena de quince unidades reordenables (trece pedales, el amplificador y el gabinete), con la salida procesada
// (estéreo) y la directa (DI, mono, alineada con la procesada: se le aplica la misma latencia del amplificador).
//
// Un bloque: el MIDI se aplica en su muestra, los eventos de la cola en la suya, el control (vibrato, bends, palanca)
// cada 32 muestras absolutas; entre esas fronteras las cuerdas corren muestra a muestra. El resultado no depende del
// tamaño de bloque (prueba T4 de thrumtest).
//
// Latencia: la del sobremuestreo del amplificador (47, 55 o 57 muestras con 2x, 4x u 8x), fija aunque el amplificador
// esté apagado (se reemplaza por un retardo igual), para que el host no tenga que recompensar y el DI siga alineado.

#include "Performer.h"
#include "Pedals.h"
#include "Amp.h"
#include "Cab.h"
#include "Conv.h"

namespace dh::gt {

struct EngineParams
{
    InstrumentParams ins;
    PerformParams perf;
    std::array<PedalParams, kNumPedals> pedal {};
    AmpParams amp; bool ampOn = true;
    CabParams cab; bool cabOn = true; int cabSource = 0;    // 0 modelo, 1 IR de fábrica, 2 IR del usuario
    int factoryIr = 0;
    std::array<int, kNumUnits> order {};                  // permutación de las unidades
    float outDb = 0.0f;
    float diDb = 0.0f;
    bool diOnMain = false;                                // la salida principal lleva la DI (para hosts sin buses)
    EngineParams() { for (int i = 0; i < kNumUnits; ++i) order[(size_t) i] = defaultOrder()[(size_t) i]; }
    static const std::array<int, kNumUnits>& defaultOrder()
    {
        // Lo habitual: dinámica, wah y saturación antes del amplificador; modulación y tiempo después del gabinete
        // (como en un estudio, para que salgan en estéreo).
        static const std::array<int, kNumUnits> o { kGate, kComp, kWah, kOd, kDist, kFuzz, kUnitAmp, kUnitCab, kChorus, kPhaser, kFlanger, kTremolo, kVibrato, kDelay, kReverb };
        return o;
    }
};

class Engine
{
public:
    void prepare (double sampleRate, int maxBlock)
    {
        fs = (float) sampleRate; maxB = std::max (16, maxBlock);
        ins.prepare (fs);
        perf.prepare (fs, ins); perf.reserve();
        gate.prepare (fs); comp.prepare (fs); wah.prepare (fs); od.prepare (fs); dist.prepare (fs); fuzz.prepare (fs);
        chorus.prepare (fs); phaser.prepare (fs); flanger.prepare (fs); trem.prepare (fs); vib.prepare (fs); delay.prepare (fs); verb.prepare (fs);
        amp.prepare (fs, maxB);
        convA.prepare (fs); convB.prepare (fs); convAR.prepare (fs); convBR.prepare (fs);
        kA.reserve (ConvKernel::maxLength (fs)); kB.reserve (ConvKernel::maxLength (fs));
        { const float one = 1.0f; kA.build (&one, 1, fs, kfft, kscratch); kB.build (&one, 1, fs, kfft, kscratch); }
        convA.setKernel (&kA); convAR.setKernel (&kA); convB.setKernel (&kB); convBR.setKernel (&kB);
        di.assign ((size_t) maxB, 0.0f); bufL.assign ((size_t) maxB, 0.0f); bufR.assign ((size_t) maxB, 0.0f);
        irLen = (int) std::lround (0.045 * fs);
        configured = false;
        set (P);
        reset();
    }
    void reset()
    {
        ins.reset(); perf.reset();
        gate.reset(); comp.reset(); wah.reset(); od.reset(); dist.reset(); fuzz.reset();
        chorus.reset(); phaser.reset(); flanger.reset(); trem.reset(); vib.reset(); delay.reset(); verb.reset();
        amp.reset(); convA.reset(); convB.reset(); convAR.reset(); convBR.reset();
        diDelay.clear(); ampBypL.clear(); ampBypR.clear();
        t = 0; xf = 1.0f;
        perf.refreshTab();
    }

    void set (const EngineParams& p)
    {
        const bool qualityChanged = ! configured || p.amp.quality != P.amp.quality;
        const bool cabChanged = ! configured || ! (p.cab == P.cab) || p.cabSource != P.cabSource || p.factoryIr != P.factoryIr;
        P = p;
        ins.set (P.ins);
        perf.set (P.perf);
        perf.refreshTab();
        gate.set (P.pedal[kGate]); comp.set (P.pedal[kComp]); wah.set (P.pedal[kWah]); od.set (P.pedal[kOd]); dist.set (P.pedal[kDist]);
        fuzz.set (P.pedal[kFuzz]); chorus.set (P.pedal[kChorus]); phaser.set (P.pedal[kPhaser]); flanger.set (P.pedal[kFlanger]);
        trem.set (P.pedal[kTremolo]); vib.set (P.pedal[kVibrato]); delay.set (P.pedal[kDelay]); verb.set (P.pedal[kReverb]);
        amp.set (P.amp);
        if (qualityChanged) { const int lat = amp.latency(); diDelay.prepare (lat); ampBypL.prepare (lat); ampBypR.prepare (lat); }
        // Gabinete: en modo síncrono (pruebas, render) la IR se diseña acá; en el plugin la diseña otro hilo.
        if (cabChanged && syncCab && P.cabSource != 2) { const auto ir = currentModelIr(); loadIr (ir.data(), (int) ir.size(), ! configured); }
        outGain = dbToGain (P.outDb); diGain = dbToGain (P.diDb);
        configured = true;
    }
    const EngineParams& params() const { return P; }
    // La IR que corresponde a los ajustes del gabinete (modelo o de fábrica); la IR del usuario la pasa el procesador.
    std::vector<float> currentModelIr() const
    {
        const CabParams& c = P.cabSource == 1 ? factoryIrs()[(size_t) std::clamp (P.factoryIr, 0, (int) factoryIrs().size() - 1)].p : P.cab;
        return designCabIr (c, fs, irLen);
    }
    void setSyncCab (bool s) { syncCab = s; }
    int irLength() const { return irLen; }
    // Cargar una IR con un fundido de 20 ms desde la anterior. Arma el núcleo acá (FFT de cada partición): sin reservar
    // memoria hasta kMaxIrSeconds, pero con trabajo; en el plugin en tiempo real, el núcleo lo arma otro hilo (swapIn).
    void loadIr (const float* h, int n, bool immediate = false)
    {
        ConvKernel& dst = useB ? kA : kB;
        dst.build (h, n, fs, kfft, kscratch);
        startFade (immediate);
    }
    // Un núcleo armado en otro hilo: se intercambia con el libre (sin copiar ni reservar) y queda el viejo en "staged".
    // false si se armó para otra frecuencia de muestreo (hay que pedirlo de nuevo).
    bool swapIn (ConvKernel& staged, bool immediate = false)
    {
        if (std::fabs (staged.fs - (double) fs) > 0.5) return false;
        (useB ? kA : kB).swap (staged);
        startFade (immediate);
        return true;
    }
    int irLengthMax() const { return ConvKernel::maxLength (fs); }

    int latency() const { return amp.latency(); }
    static int latencyFor (int quality) { return Amp::latencyFor (quality); }

    // Un bloque. outDI puede ser nullptr. midi: los mensajes de este bloque (offset dentro del bloque, ordenados).
    void process (float* outL, float* outR, float* outDI, int n, const HostTime& host, const MidiMsg* midi, int nMidi, std::vector<MidiOutEv>* midiOut)
    {
        for (int i0 = 0; i0 < n; i0 += maxB)
        {
            const int m = std::min (maxB, n - i0);
            // El MIDI de este trozo.
            int a = 0; while (a < nMidi && midi[a].offset < i0) ++a;
            int b = a; while (b < nMidi && midi[b].offset < i0 + m) ++b;
            HostTime h = host;
            if (h.playing) h.ppq += (double) i0 / fs * h.bpm / 60.0;
            processChunk (outL + i0, outR + i0, outDI ? outDI + i0 : nullptr, m, h, midi + a, b - a, i0, midiOut);
        }
    }

    // Soltar todo (bypass del host, all notes off).
    void allNotesOff (std::vector<MidiOutEv>* midiOut)
    {
        perf.allNotesOff (t, true);
        if (midiOut) { for (auto e : perf.outEvents) { e.t = 0; midiOut->push_back (e); } }
        perf.outEvents.clear();
    }

    Instrument& instrument() { return ins; }
    Performer& performer() { return perf; }
    const Performer& performer() const { return perf; }
    int64_t samplePosition() const { return t; }
    void setWahPosition (float x) { wah.setExternal (x); }

private:
    float fs = 48000.0f; int maxB = 512; bool configured = false, syncCab = true;
    EngineParams P;
    Instrument ins; Performer perf;
    Gate gate; Comp comp; Wah wah; Overdrive od; Distortion dist; Fuzz fuzz; Chorus chorus; Phaser phaser; Flanger flanger;
    Tremolo trem; Vibrato vib; Delay delay; Reverb verb; Amp amp;
    ConvKernel kA, kB; PartConv convA, convB, convAR, convBR; RealFft kfft; std::vector<float> kscratch;
    bool useB = false; float xf = 1.0f; int irLen = 2048;
    void startFade (bool immediate)
    {
        // El núcleo nuevo está en el lado libre: ese lado arranca de cero y pasa a ser el actual.
        useB = ! useB;
        PartConv& cur = useB ? convB : convA; PartConv& curR = useB ? convBR : convAR;
        cur.reset(); curR.reset();
        if (immediate)
        {
            // Los dos lados con el mismo núcleo no hacen falta: sin fundido, el viejo no se lee.
            xf = 1.0f; return;
        }
        xf = 0.0f;
    }
    FixedDelay diDelay, ampBypL, ampBypR;
    std::vector<float> di, bufL, bufR;
    float outGain = 1.0f, diGain = 1.0f;
    int64_t t = 0;

    void processChunk (float* outL, float* outR, float* outDI, int n, const HostTime& host, const MidiMsg* midi, int nMidi, int blockOffset, std::vector<MidiOutEv>* midiOut)
    {
        perf.refreshTab();
        perf.beginBlock (host, n, t);
        const int64_t t0 = t;
        int pos = 0, mi = 0;
        while (pos < n)
        {
            const int64_t now = t0 + pos;
            while (mi < nMidi && midi[mi].offset - blockOffset <= pos) { perf.midi (midi[mi], now); ++mi; }
            if ((now % Performer::kCtl) == 0) perf.control (now);
            perf.dispatch (now);
            int next = n;
            if (mi < nMidi) next = std::min (next, std::max (pos + 1, midi[mi].offset - blockOffset));
            const int64_t ne = perf.nextEventTime();
            if (ne != INT64_MAX) next = (int) std::min<int64_t> (next, std::max<int64_t> (pos + 1, ne - t0));
            const int64_t nextCtl = (now / Performer::kCtl + 1) * Performer::kCtl;
            next = (int) std::min<int64_t> (next, nextCtl - t0);
            ins.process (di.data() + pos, next - pos);
            pos = next;
        }
        t = t0 + n;
        // Salida MIDI de este trozo.
        if (midiOut)
            for (auto e : perf.outEvents) { e.t = std::clamp<int64_t> (e.t - t0, 0, n - 1) + blockOffset; midiOut->push_back (e); }   // (reservado por el procesador)
        perf.outEvents.clear();
        // Cadena.
        float* L = bufL.data(); float* R = bufR.data();
        for (int i = 0; i < n; ++i) { L[i] = di[(size_t) i]; R[i] = di[(size_t) i]; }
        bool stereo = false;
        const double bpm = host.playing ? host.bpm : (double) P.perf.tempo;
        for (int k = 0; k < kNumUnits; ++k)
        {
            const int u = P.order[(size_t) k];
            if (u == kUnitAmp)
            {
                if (stereo) for (int i = 0; i < n; ++i) L[i] = 0.5f * (L[i] + R[i]);
                if (P.ampOn) amp.process (L, n);
                else for (int i = 0; i < n; ++i) L[i] = ampBypL.process (L[i]);
                std::copy (L, L + n, R); stereo = false;
                continue;
            }
            if (u == kUnitCab)
            {
                if (! P.cabOn) continue;
                for (int i = 0; i < n; ++i)
                {
                    // Fundido entre la IR anterior y la nueva (20 ms).
                    PartConv& cur = useB ? convB : convA; PartConv& old = useB ? convA : convB;
                    float y = cur.process (L[i]);
                    if (xf < 1.0f) { y = old.process (L[i]) * (1.0f - xf) + y * xf; xf = std::min (1.0f, xf + 1.0f / (0.02f * fs)); }
                    if (stereo)
                    {
                        PartConv& curR = useB ? convBR : convAR; PartConv& oldR = useB ? convAR : convBR;
                        float yr = curR.process (R[i]);
                        if (xf < 1.0f) yr = oldR.process (R[i]) * (1.0f - xf) + yr * xf;
                        R[i] = yr;
                    }
                    L[i] = y;
                }
                if (! stereo) std::copy (L, L + n, R);
                continue;
            }
            const auto& pp = P.pedal[(size_t) u];
            if (! pp.on) continue;
            switch (u)
            {
                case kGate: gate.process (L, R, n, stereo); break;
                case kComp: comp.process (L, R, n, stereo); break;
                case kWah: wah.process (L, R, n, stereo); break;
                case kOd: od.process (L, R, n, stereo); break;
                case kDist: dist.process (L, R, n, stereo); break;
                case kFuzz: fuzz.process (L, R, n, stereo); break;
                case kChorus: chorus.process (L, R, n, stereo); break;
                case kPhaser: phaser.process (L, R, n, stereo); break;
                case kFlanger: flanger.process (L, R, n, stereo); break;
                case kTremolo: trem.process (L, R, n, stereo, bpm); break;
                case kVibrato: vib.process (L, R, n, stereo); break;
                case kDelay: delay.process (L, R, n, stereo, bpm); break;
                case kReverb: verb.process (L, R, n, stereo); break;
                default: break;
            }
            if (! stereo) std::copy (L, L + n, R);
        }
        // Salidas: techo suave en la procesada; la DI con la misma latencia.
        for (int i = 0; i < n; ++i)
        {
            const float d = diDelay.process (di[(size_t) i]) * diGain;
            if (outDI) outDI[i] = d;
            if (P.diOnMain) { outL[i] = d; outR[i] = d; continue; }
            outL[i] = ceiling (L[i] * outGain);
            outR[i] = ceiling (R[i] * outGain);
        }
    }
    static inline float ceiling (float x)
    {
        if (! isFiniteF (x)) return 0.0f;
        const float m = std::fabs (x);
        return m < 0.9f ? x : std::copysign (0.9f + 0.1f * fastTanh ((m - 0.9f) * 10.0f), x);
    }
};

} // namespace dh::gt
