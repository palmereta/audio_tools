#pragma once
// El efecto completo. Seco y húmedo; el húmedo recorre:
//   entrada → pre-delay → red (con shimmer y granos inyectados en el lazo, y la sala que respira o colapsa)
//   → reverse → espectral (difuminado / congelado por bandas) → resonador afinado → gate → ducking → stutter
//   → limitador de seguridad → mezcla.
// La ruta seca no tiene latencia (el plugin informa 0). Determinista: semillas fijas, nada depende del bloque.
// También deja datos para la vista (energía por línea, brillo y nivel de cada ataque, estados de los módulos).

#include "Network.h"
#include "Modules.h"
#include "Dynamics.h"          // el compresor de Lacquer (plugins/lacquer/Source/dsp): detector del ducking

namespace dh::sp {

struct EngineParams
{
    NetParams net;
    float mix = 0.35f;               // 0 seco … 1 húmedo
    float predelayMs = 0.0f;
    float shimmer = 0.0f; int interval = 0;
    float grains = 0.0f; float grainMs = 120.0f;
    float reverse = 0.0f; float reverseMs = 500.0f;
    float spectral = 0.0f; float smear = 0.5f; int spectralFreeze = 0;   // máscara de bandas (1 graves, 2 medios, 4 agudos)
    float resonance = 0.0f; float resFeedback = 0.93f; float notes[4] = { 57, 64, 69, 72 }; int nNotes = 0;
    float duck = 0.0f;               // cuánto baja el húmedo mientras suena la entrada (0..1)
    float gate = 0.0f; float gateHoldMs = 250.0f;
    float stutter = 0.0f; float stutterMs = 125.0f; int stutterRepeats = 4;
    int impossible = 0; float impDepth = 0.4f; float impRate = 0.08f;      // 1 respira, 2 colapsa
    float outGainDb = 0.0f;
};

struct Onset { float level = 0.0f, bright = 0.0f; };   // un ataque de la entrada, para lanzar partículas

class Engine
{
public:
    void prepare (float sampleRate, int /*maxBlock*/)
    {
        fs = sampleRate;
        net.prepare (fs, 7);
        pre[0].prepare ((int) (0.5f * fs)); pre[1].prepare ((int) (0.5f * fs));
        shim.prepare (fs); grainsL.prepare (fs, 11); grainsR.prepare (fs, 13);
        revL.prepare (fs); revR.prepare (fs);
        specL.prepare (fs, 17); specR.prepare (fs, 19);
        resL.prepare (fs); resR.prepare (fs);
        stut.prepare (fs, 23);
        ducker.prepare (fs);
        duckEnv.set (8.0f, 260.0f, fs); gateEnv.set (2.0f, 40.0f, fs); onsetFast.set (1.0f, 30.0f, fs); onsetSlow.set (40.0f, 400.0f, fs);
        brightLp.setHz (1500.0f, fs);
        specK = 1.0f - std::exp (-1.0f / (0.05f * fs));
        reset();
    }
    void reset()
    {
        net.reset(); pre[0].clear(); pre[1].clear(); shim.reset(); grainsL.reset(); grainsR.reset(); revL.reset(); revR.reset();
        specL.reset(); specR.reset(); resL.reset(); resR.reset(); stut.reset(); ducker.reset();
        duckEnv.e = gateEnv.e = onsetFast.e = onsetSlow.e = 0.0f; gateOpen = 0.0f; gateHold = 0; lastWetL = lastWetR = 0.0f; impPh = 0.0f; collapse = 0.0f; impCount = 0;
        onsetArmed = true; brightLp.reset(); pendingOnsets = 0; specMix = 0.0f;
    }

    void set (const EngineParams& p)
    {
        P = p;
        NetParams np = p.net;
        if (p.resonance > 0.0f && p.nNotes > 0)
        {
            np.nTune = p.nNotes; np.tuneAmount = clampf (p.resonance * 1.5f, 0.0f, 1.0f);
            for (int k = 0; k < p.nNotes; ++k) np.tuneHz[k] = 440.0f * std::exp2 ((p.notes[k] - 69.0f) / 12.0f);
        }
        baseNet = np;
        net.set (np);
        shim.setInterval (p.interval);
        revL.setChunk (p.reverseMs); revR.setChunk (p.reverseMs);
        resL.setNotes (p.notes, p.nNotes, p.resFeedback); resR.setNotes (p.notes, p.nNotes, p.resFeedback);
        outGain = dbToGain (p.outGainDb);
        mixW = std::sin (0.5f * kPi * clampf (p.mix, 0.0f, 1.0f)); mixD = std::cos (0.5f * kPi * clampf (p.mix, 0.0f, 1.0f));
        holdN = (int) (p.gateHoldMs * 1e-3f * fs);
        dh::lq::CompParams cp; cp.thresholdDb = -38.0; cp.ratio = 1.0 + 9.0 * clampf (p.duck, 0.0f, 1.0f); cp.kneeDb = 10.0;
        cp.attackMs = 8.0; cp.releaseMs = 260.0; cp.rmsMs = 5.0; ducker.set (cp);
    }

    // `beatStart`: la muestra (dentro del bloque) donde empieza un trozo del stutter según el host (−1 = ninguna).
    void process (float* L, float* R, int n, int beatStart = -1)
    {
        const int pd = (int) (P.predelayMs * 1e-3f * fs);
        for (int i = 0; i < n; ++i)
        {
            // Salas imposibles: el tamaño se recalcula cada 32 muestras con un contador propio (no depende del bloque);
            // la red desliza los largos, así no hay saltos.
            if (P.impossible != 0 && ++impCount >= 32) { impCount = 0; updateImpossible (32); }
            const float dryL = L[i], dryR = R[i];
            // Ataques de la entrada (para la vista): envolvente rápida contra la lenta.
            const float mono = 0.5f * (dryL + dryR);
            const float ef = onsetFast.process (mono), es = onsetSlow.process (mono);
            const float hf = mono - brightLp.lp (mono);
            brightAcc = 0.995f * brightAcc + 0.005f * hf * hf; fullAcc = 0.995f * fullAcc + 0.005f * mono * mono;
            // Un ataque: la envolvente rápida supera 1.6 veces la lenta. Se rearma cuando baja, o a los 90 ms.
            if (onsetArmed && ef > 0.008f && ef > 1.6f * es) { onsetArmed = false; rearm = (int) (0.09f * fs); if (pendingOnsets < 8) onsets[pendingOnsets++] = { ef, std::sqrt (brightAcc / (fullAcc + 1e-12f)) }; }
            if (! onsetArmed && (ef < 1.1f * es || --rearm <= 0)) onsetArmed = true;

            // Pre-delay.
            float inL = dryL, inR = dryR;
            pre[0].push (dryL); pre[1].push (dryR);       // siempre: al subir el pre-delay desde 0 no suena audio viejo
            if (pd > 0) { inL = pre[0].at (pd); inR = pre[1].at (pd); }
            // Inyección en el lazo: shimmer (transpone lo que sale) y granos.
            float injL = 0.0f, injR = 0.0f;
            if (P.shimmer > 0.0f) { const float s = shim.process (0.5f * (lastWetL + lastWetR)) * P.shimmer * 0.7f; injL += s; injR += s; }
            if (P.grains > 0.0f) { injL += grainsL.process (lastWetL, P.grains, P.grainMs) * P.grains; injR += grainsR.process (lastWetR, P.grains, P.grainMs) * P.grains; }
            net.injectL = injL; net.injectR = injR;
            float wl, wr;
            net.process (inL, inR, wl, wr);
            lastWetL = wl; lastWetR = wr;
            // Reverse, espectral, resonador (mezclados sobre el húmedo).
            if (P.reverse > 0.0f) { wl += (revL.process (wl) - wl) * P.reverse; wr += (revR.process (wr) - wr) * P.reverse; }
            // Espectral: difuminado (módulo Spectral) o congelado de una banda (el resto pasa intacto, con su fase).
            // Se funde en 50 ms al entrar y al salir (el camino espectral llega ~40 ms más tarde).
            {
                const bool want = P.spectral > 0.0f || P.spectralFreeze != 0;
                specMix += ((want ? 1.0f : 0.0f) - specMix) * specK;
                if (specMix <= 1e-4f) { specL.feed (wl); specR.feed (wr); }
                else
                {
                    const bool live = P.spectral <= 0.0f;
                    const float a = specMix * (P.spectralFreeze ? 1.0f : P.spectral);
                    const float sl = specL.process (wl, P.smear, P.spectralFreeze, live), sr2 = specR.process (wr, P.smear, P.spectralFreeze, live);
                    wl += (sl - wl) * a; wr += (sr2 - wr) * a;
                }
            }
            if (P.resonance > 0.0f && P.nNotes > 0)
            {
                // En modo delay, peines afinados sobre el húmedo; en modo reverb la red misma está afinada (set()).
                const float k = P.resonance * (1.0f - baseNet.space);
                if (k > 0.0f) { wl += resL.process (wl) * k; wr += resR.process (wr) * k; }
            }
            // Gate (reverb cortada a la vieja usanza): abierta mientras suena la entrada y un rato después.
            if (P.gate > 0.0f)
            {
                const float e = gateEnv.process (mono);
                if (e > 0.02f) gateHold = holdN; else if (gateHold > 0) --gateHold;
                const float tgt = gateHold > 0 ? 1.0f : 0.0f;
                gateOpen += (tgt - gateOpen) * (tgt > gateOpen ? 0.05f : 0.004f);
                const float g = 1.0f - P.gate * (1.0f - gateOpen);
                wl *= g; wr *= g;
            }
            // Ducking: el húmedo baja mientras la entrada suena (compresor de Lacquer con la entrada como detector:
            // umbral relativo, rodilla suave, ataque 8 ms, liberación 260 ms; la profundidad la da la relación).
            if (P.duck > 0.0f)
            {
                const float g = (float) ducker.gainFor (std::max (std::fabs ((double) dryL), std::fabs ((double) dryR)));
                wl *= g; wr *= g;
            }
            if (P.stutter > 0.0f) stut.process (wl, wr, (int) (P.stutterMs * 1e-3f * fs), P.stutter, P.stutterRepeats, i == beatStart);
            // Limitador de seguridad del húmedo (suave desde −3 dBFS) y mezcla con potencia constante.
            wl = safety (wl); wr = safety (wr);
            L[i] = (dryL * mixD + wl * mixW) * outGain;
            R[i] = (dryR * mixD + wr * mixW) * outGain;
            wetPeak = std::max (wetPeak * 0.99995f, std::max (std::fabs (wl), std::fabs (wr)));
        }
    }

    // ── Para la vista ──
    const Network& network() const { return net; }
    int takeOnsets (Onset* out, int max) { const int k = std::min (max, pendingOnsets); for (int i = 0; i < k; ++i) out[i] = onsets[i]; pendingOnsets = 0; return k; }
    float wetPeakLevel() const { return wetPeak; }
    bool stutterActive() const { return stut.isActive(); }
    int shimmerLayers() const { return P.shimmer > 0.0f ? shim.layers() : 0; }
    int grainsActive() const { return grainsL.active() + grainsR.active(); }
    float sizeNow() const { return P.impossible == 0 ? baseNet.size : baseNet.size * (P.impossible == 1 ? (1.0f + P.impDepth * std::sin (2.0f * kPi * impPh)) : (1.0f - 0.85f * P.impDepth * collapse)); }

private:
    void updateImpossible (int n)
    {
        impPh += P.impRate * (float) n / fs; impPh -= std::floor (impPh);
        NetParams q = baseNet;
        if (P.impossible == 1) q.size = baseNet.size * (1.0f + P.impDepth * std::sin (2.0f * kPi * impPh));
        else
        {
            // Colapsa: mientras suena, las paredes se acercan y la realimentación sube; en silencio vuelven despacio.
            const float drive = onsetSlow.e > 0.01f ? 1.0f : -0.3f;
            collapse = clampf (collapse + drive * P.impRate * (float) n / fs, 0.0f, 1.0f);
            q.size = baseNet.size * (1.0f - 0.85f * P.impDepth * collapse);
            q.decayS = baseNet.decayS >= 100.0f ? baseNet.decayS : baseNet.decayS * (1.0f + 3.0f * collapse);
        }
        net.set (q);
    }
    int impCount = 0;
    static inline float safety (float x)
    {
        const float a = std::fabs (x), t = 0.708f;                      // −3 dBFS
        if (a <= t) return x;
        const float y = t + (1.0f - t) * std::tanh ((a - t) / (1.0f - t));
        return x < 0 ? -y : y;
    }

    float fs = 48000.0f, outGain = 1.0f, mixW = 0.5f, mixD = 0.8f;
    EngineParams P; NetParams baseNet;
    Network net;
    DelayLine pre[2];
    Shimmer shim; GrainCloud grainsL, grainsR; Reverser revL, revR; Spectral specL, specR; Resonator resL, resR; Stutter stut;
    Envelope duckEnv, gateEnv, onsetFast, onsetSlow;
    dh::lq::Compressor ducker;
    OnePole brightLp;
    float lastWetL = 0.0f, lastWetR = 0.0f, gateOpen = 0.0f, impPh = 0.0f, collapse = 0.0f, wetPeak = 0.0f, brightAcc = 0.0f, fullAcc = 1e-9f;
    int gateHold = 0, holdN = 12000;
    float specMix = 0.0f, specK = 0.001f;
    bool onsetArmed = true; int rearm = 0;
    Onset onsets[8]; int pendingOnsets = 0;
};

} // namespace dh::sp
