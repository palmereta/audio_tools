#pragma once
// Un instrumento de Pallor (uno de los 16 slots): el motor elegido (analógico o digital), los insertos y el VCA final.
// Las voces digitales corren al doble de la frecuencia y bajan con el diezmador de media banda compartido (95
// coeficientes: 23,5 muestras de retardo, más la media muestra del escalón de banda limitada = 24 justas); las
// analógicas se atrasan esas mismas 24 muestras, así los dos motores quedan alineados y el plugin informa una latencia
// fija de kVoiceLatency.
//   golpe → fuente (analógica o muestra) → crush (bits y frecuencia) → drive → filtro → nivel
// Un golpe nuevo no corta el anterior de un tajo: el que sonaba se apaga en 2 ms en el otro carril mientras el nuevo
// arranca (dos carriles por slot). El choke apaga los dos carriles en 4 ms. El paneo, los envíos y el bus de salida los
// aplica el motor (Engine.h) sobre la salida mono de cada slot.

#include "Digital.h"
#include "Oversampling.h"

namespace dh::pal {

constexpr int kVoiceLatency = 24;

struct VoiceParams
{
    int engine = 0;              // 0 analógico, 1 digital
    int model = 0;               // modelo analógico de la familia
    int kit = 0;                 // kit de fábrica (numKits() = muestra propia)
    int chain = kChainKit;       // modo de la cadena digital
    Shape shape;
    bool reverse = false;
    float levelDb = 0.0f, pan = 0.0f, accent = 0.5f, velSense = 0.7f;
    float drive = 0.0f, bits = 16.0f, rateHz = 0.0f;     // rateHz 0 = sin reducción
    int filter = 0; float cutoff = 20000.0f, reso = 0.0f;  // 0 apagado, 1 LP, 2 HP, 3 BP
    int choke = 0; float sendA = 0.0f, sendB = 0.0f; int out = 0; bool mute = false, solo = false;
};

class Slot
{
public:
    void prepare (float sampleRate, int slotId)
    {
        fs = sampleRate; id = slotId;
        for (auto& a : an) a.prepare (fs, id);
        for (auto& d : dg) d.prepare (2.0f * fs, id);
        fir.design (95, 9.0);
        dc.set (8.0f, fs);
        fadeStep = 1.0f / (0.002f * fs); chokeStep = 1.0f / (0.004f * fs);
        reset();
    }
    void reset()
    {
        for (auto& a : an) a.stop();
        for (auto& d : dg) d.stop();
        gain[0] = gain[1] = 0; fading[0] = fading[1] = false; kind[0] = kind[1] = 0; cur = 0;
        flt.reset(); dc.reset(); held = 0; holdPh = 1.0f; tail = 0; hits = 0; active = false;
        fir.reset(); firTail = 0; adl.fill (0.0f); adw = 0;
    }
    void set (const VoiceParams& p) { P = p; lvl = P.mute ? 0.0f : (P.levelDb <= -59.9f ? 0.0f : dbToGain (P.levelDb)); updateInserts(); }
    const VoiceParams& params() const { return P; }

    // vel 0..1 (de la nota o del paso), accent: el paso está acentuado; globalAccent: el nivel de acento general.
    void trigger (float vel, bool accented, float globalAccent, const SampleData* sample)
    {
        const float v = clampf (vel, 0.0f, 1.0f);
        const float vg = 1.0f - P.velSense + P.velSense * std::pow (v, 1.3f);
        const float acc = accented ? clampf (P.accent * globalAccent, 0.0f, 1.0f) : 0.0f;
        const float amp = vg * (1.0f + 0.5f * acc);
        // El carril que sonaba se apaga; el otro toma el golpe nuevo.
        if (laneActive (cur)) fading[cur] = true;
        cur ^= 1;
        fading[cur] = false; gain[cur] = 1.0f;
        ++hits;
        if (P.engine == 0) { kind[cur] = 0; an[cur].trigger (P.model, P.shape, amp, acc, (uint32_t) hits); }
        else { kind[cur] = 1; dg[cur].trigger (sample, P.chain, P.shape, P.reverse, amp, acc, hits); }
        active = true; tail = (int) (0.05f * fs);
    }
    void choke() { for (int l = 0; l < 2; ++l) if (laneActive (l)) { fading[l] = true; fadeFast[l] = true; } }
    // Una muestra propia que se reemplaza: los carriles que la tocan se cortan (la memoria vieja se libera después).
    void dropSample (const SampleData* s) { for (int l = 0; l < 2; ++l) if (kind[l] == 1 && dg[l].sample() == s) dg[l].stop(); }

    bool isActive() const { return active; }

    // Renderiza n muestras mono (escribe, no suma). Devuelve false si el slot está en silencio (out queda en cero).
    bool render (float* out, int n)
    {
        if (! active) { std::fill (out, out + n, 0.0f); return false; }
        for (int i = 0; i < n; ++i)
        {
            float xa = 0.0f, xd0 = 0.0f, xd1 = 0.0f; bool dig = false;
            for (int l = 0; l < 2; ++l)
            {
                if (! laneActive (l)) continue;
                float s0, s1 = 0.0f;
                if (kind[l] == 0) s0 = an[l].process();
                else { s0 = dg[l].process(); s1 = dg[l].process(); dig = true; }
                if (fading[l])
                {
                    gain[l] -= fadeFast[l] ? chokeStep : fadeStep;
                    if (gain[l] <= 0.0f) { gain[l] = 0.0f; stopLane (l); continue; }
                }
                if (kind[l] == 0) xa += s0 * gain[l]; else { xd0 += s0 * gain[l]; xd1 += s1 * gain[l]; }
            }
            // Analógico: 24 muestras de retardo. Digital: de ×2 a ×1 por la media banda.
            adl[(size_t) adw] = xa; const float ya = adl[(size_t) ((adw - kVoiceLatency) & 31)]; adw = (adw + 1) & 31;
            float yd = 0.0f;
            if (dig || firTail > 0) { fir.push (xd0); yd = fir.output(); fir.push (xd1); firTail = dig ? 96 : firTail - 1; }
            out[i] = inserts (ya + yd);
            // La cola de los insertos se cuenta por muestra (no por bloque): el corte cae en la misma muestra con
            // cualquier tamaño de bloque.
            if (! dig && ! laneActive (0) && ! laneActive (1) && --tail <= 0)
            {
                active = false; flt.reset(); dc.reset(); held = 0; fir.reset(); firTail = 0; adl.fill (0.0f);
                std::fill (out + i + 1, out + n, 0.0f);
                break;
            }
        }
        return true;
    }

private:
    float fs = 48000; int id = 0;
    VoiceParams P; float lvl = 1.0f;
    AnalogVoice an[2]; DigitalPlayer dg[2];
    int kind[2] {}; float gain[2] {}; bool fading[2] {}, fadeFast[2] {};
    int cur = 0, tail = 0; uint32_t hits = 0; bool active = false;
    float fadeStep = 0.01f, chokeStep = 0.01f;
    dh::HalfbandFir fir; int firTail = 0;
    std::array<float, 32> adl {}; int adw = 0;
    // Insertos.
    Svf flt; Hp1 dc;
    float held = 0, holdPh = 1, holdInc = 1, levels = 32768, driveG = 1, driveMk = 1;
    bool crushBits = false, crushRate = false, driveOn = false;

    bool laneActive (int l) const { return kind[l] == 0 ? an[l].isActive() : dg[l].isActive(); }
    void stopLane (int l) { if (kind[l] == 0) an[l].stop(); else dg[l].stop(); fading[l] = false; fadeFast[l] = false; }

    void updateInserts()
    {
        crushBits = P.bits < 15.95f;
        levels = std::exp2 (clampf (P.bits, 2.0f, 16.0f) - 1.0f);
        crushRate = P.rateHz > 0.0f && P.rateHz < 0.999f * fs;
        holdInc = crushRate ? P.rateHz / fs : 1.0f;
        driveOn = P.drive > 0.001f;
        driveG = 1.0f + 24.0f * P.drive * P.drive;
        driveMk = 0.5f / fastTanh (0.5f * driveG);
        if (P.filter > 0)
        {
            const float q = 0.55f + 11.0f * P.reso * P.reso;
            flt.set (clampf (P.cutoff, 20.0f, 0.45f * fs), q, fs);
        }
    }
    inline float inserts (float x)
    {
        if (crushRate) { holdPh += holdInc; if (holdPh >= 1.0f) { holdPh -= 1.0f; held = x; } x = held; }
        if (crushBits) x = std::round (clampf (x, -1.0f, 1.0f) * levels) / levels;
        if (driveOn) x = fastTanh (driveG * x) * driveMk;
        if (P.filter == 1) x = flt.lowpass (x); else if (P.filter == 2) x = flt.highpass (x); else if (P.filter == 3) x = flt.bandpass (x);
        return dc.process (x) * lvl;
    }
};

} // namespace dh::pal
