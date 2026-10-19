#pragma once
// Sonido de prueba de Orrery: un sinte polifónico chico con un timbre por rol (drone oscuro, pad tibio,
// voz de triángulo, campana FM, gota de cristal, bajo redondo, latido suave), paneo por carril, delay y
// una reverb larga. Sirve para escuchar sin otro instrumento; el MIDI sale igual por los canales.

#include "Common.h"
#include "Blep.h"
#include "Fx.h"
#include "Generator.h"
#include <array>
#include <cmath>
#include <vector>

namespace dh::orr {

class Preview
{
public:
    void prepare (float sampleRate, int maxBlock)
    {
        fs = sampleRate;
        rev.prepare (fs); dly.prepare (fs);
        L.assign ((size_t) maxBlock, 0.0f); R.assign ((size_t) maxBlock, 0.0f);
        sL.assign ((size_t) maxBlock, 0.0f); sR.assign ((size_t) maxBlock, 0.0f);
        for (auto& v : voices) v.active = false;
    }
    void reset() { for (auto& v : voices) v.active = false; rev.reset(); dly.reset(); }

    // Eventos del generador; roles por carril para elegir el timbre.
    void handle (const OutEvent& e, const std::array<int, kLanes>& roles)
    {
        const int st = e.data[0] & 0xF0;
        if (e.lane < 0) { if (st == 0xB0 && (e.data[1] == 123 || e.data[1] == 120)) for (auto& v : voices) v.gate = false; return; }
        if (st == 0x90 && e.data[2] > 0) noteOn (e.lane, roles[(size_t) e.lane], e.data[1], (float) e.data[2] / 127.0f);
        else if (st == 0x80 || (st == 0x90 && e.data[2] == 0)) for (auto& v : voices) if (v.active && v.gate && v.lane == e.lane && v.note == e.data[1]) v.gate = false;
    }

    // Suma el sonido en out (estéreo), entre las muestras [from, to).
    void render (float* outL, float* outR, int from, int to, float volume, float reverb)
    {
        if ((size_t) to > L.size()) { L.resize ((size_t) to); R.resize ((size_t) to); sL.resize ((size_t) to); sR.resize ((size_t) to); }     // bloque mayor que el preparado
        for (int i = from; i < to; ++i) { L[(size_t) i] = R[(size_t) i] = sL[(size_t) i] = sR[(size_t) i] = 0.0f; }
        for (auto& v : voices)
        {
            if (! v.active) continue;
            for (int i = from; i < to && v.active; ++i)
            {
                const float y = tick (v);
                L[(size_t) i] += y * v.panL; R[(size_t) i] += y * v.panR;
                sL[(size_t) i] += y * v.panL * v.send; sR[(size_t) i] += y * v.panR * v.send;
            }
        }
        const int n = to - from;
        if (n <= 0) return;
        dly.process (sL.data() + from, sR.data() + from, n, 0.62f, 0.38f, 0.45f, 0.35f);
        rev.process (sL.data() + from, sR.data() + from, n, 0.93f, 0.55f, 1.0f);
        for (int i = from; i < to; ++i)
        {
            const float l = (L[(size_t) i] + sL[(size_t) i] * reverb) * volume, r = (R[(size_t) i] + sR[(size_t) i] * reverb) * volume;
            outL[i] += fastTanh (l * 0.9f) / 0.9f; outR[i] += fastTanh (r * 0.9f) / 0.9f;
        }
    }

private:
    struct Voice
    {
        bool active = false, gate = false;
        int lane = 0, role = 0, note = 60;
        float vel = 0.5f, env = 0.0f, att = 0.01f, rel = 0.01f, sustain = 1.0f, decayRate = 0.0f;
        float ph1 = 0, ph2 = 0, ph3 = 0, inc1 = 0, inc2 = 0, inc3 = 0, lp = 0, lp2 = 0, lpCoef = 0.1f, fmIdx = 0, fmDecay = 0, pitchEnv = 0;
        float panL = 0.7f, panR = 0.7f, send = 0.4f, age = 0;
        uint32_t order = 0;
    };
    float fs = 48000.0f;
    std::array<Voice, 40> voices {};
    uint32_t counter = 0;
    std::vector<float> L, R, sL, sR;
    PlateReverb rev; StereoDelay dly;

    static float hz (int n) { return 440.0f * fastExp2 (((float) n - 69.0f) / 12.0f); }
    float coefFor (float seconds) const { return 1.0f - fastExp (-1.0f / std::max (1.0f, seconds * fs)); }

    void noteOn (int lane, int role, int note, float vel)
    {
        Voice* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr) { v = &voices[0]; for (auto& x : voices) if (x.order < v->order) v = &x; }   // roba la más vieja
        *v = Voice{};
        v->active = true; v->gate = true; v->lane = lane; v->role = role; v->note = note; v->vel = vel; v->order = ++counter;
        const float f = hz (note);
        const float pan = std::clamp (((float) lane - 3.5f) / 4.0f, -1.0f, 1.0f) * 0.7f;
        v->panL = std::sqrt (0.5f * (1.0f - pan)); v->panR = std::sqrt (0.5f * (1.0f + pan));
        v->inc1 = f / fs; v->inc2 = f * 1.0042f / fs; v->inc3 = f * 0.5f / fs;
        float a = 0.01f, r = 1.0f, cut = 3000.0f;
        switch (role)
        {
            case Drone:  a = 3.0f; r = 6.0f; cut = 520.0f; v->send = 0.55f; break;
            case Pad:    a = 1.6f; r = 4.0f; cut = 1500.0f; v->send = 0.6f; break;
            case Melody: a = 0.06f; r = 1.4f; cut = 2600.0f; v->send = 0.45f; break;
            case Arp:    a = 0.004f; r = 2.2f; v->fmIdx = 2.2f; v->fmDecay = coefFor (0.6f); v->inc2 = f * 3.5f / fs; v->sustain = 0.0f; v->decayRate = coefFor (1.8f); v->send = 0.5f; break;
            case Cloud:  a = 0.02f; r = 2.5f; v->fmIdx = 0.8f; v->fmDecay = coefFor (0.3f); v->inc2 = f * 2.0f / fs; v->sustain = 0.0f; v->decayRate = coefFor (2.5f); v->send = 0.75f; break;
            case Bass:   a = 0.25f; r = 2.0f; cut = 380.0f; v->send = 0.2f; break;
            default:     a = 0.003f; r = 0.5f; v->sustain = 0.0f; v->decayRate = coefFor (0.35f); v->pitchEnv = 1.0f; v->send = 0.3f; break;   // pulso
        }
        v->att = coefFor (a); v->rel = coefFor (r); v->lpCoef = 1.0f - fastExp (-kTwoPi * std::min (cut * (0.6f + 0.6f * vel), 0.4f * fs) / fs);
    }

    inline float tick (Voice& v)
    {
        // Envolvente: ataque exponencial al tope, caída al sostén (si tiene), release al soltar.
        if (v.gate && v.age >= 0.0f)
        {
            if (v.age < 1.0f) { v.env += (1.02f - v.env) * v.att; if (v.env >= 1.0f) { v.env = 1.0f; v.age = 1.0f; } }
            else if (v.decayRate > 0.0f) v.env += (v.sustain - v.env) * v.decayRate;
        }
        else v.env -= v.env * v.rel;
        if (! v.gate && v.env < 1.0e-4f) { v.active = false; return 0.0f; }
        if (! v.gate && v.decayRate > 0.0f) v.env -= v.env * v.decayRate;
        float y;
        switch (v.role)
        {
            case Drone: case Pad: case Bass:
            {
                // Dos sierras (la segunda desafinada) y una subarmónica en triángulo, por un pasa bajos de dos polos.
                v.ph1 += v.inc1; if (v.ph1 >= 1.0f) v.ph1 -= 1.0f;
                v.ph2 += v.inc2; if (v.ph2 >= 1.0f) v.ph2 -= 1.0f;
                v.ph3 += v.inc3; if (v.ph3 >= 1.0f) v.ph3 -= 1.0f;
                float s1 = 2.0f * v.ph1 - 1.0f - polyBlep (v.ph1, v.inc1);
                float s2 = 2.0f * v.ph2 - 1.0f - polyBlep (v.ph2, v.inc2);
                float tri = 4.0f * std::abs (v.ph3 - 0.5f) - 1.0f;
                const float x = v.role == Bass ? 0.4f * s1 + 0.9f * tri : 0.5f * (s1 + s2) + (v.role == Drone ? 0.4f * tri : 0.15f * tri);
                v.lp += (x - v.lp) * v.lpCoef; v.lp2 += (v.lp - v.lp2) * v.lpCoef;
                y = v.lp2 * 0.55f;
                break;
            }
            case Melody:
            {
                v.ph3 += 5.1f / fs; if (v.ph3 >= 1.0f) v.ph3 -= 1.0f;   // vibrato
                v.ph1 += v.inc1 * (1.0f + 0.003f * std::sin (kTwoPi * v.ph3) * std::min (1.0f, v.age)); if (v.ph1 >= 1.0f) v.ph1 -= 1.0f;
                float tri = 4.0f * std::abs (v.ph1 - 0.5f) - 1.0f;
                v.lp += (tri - v.lp) * v.lpCoef;
                y = v.lp * 0.7f;
                break;
            }
            case Arp: case Cloud:
            {
                v.fmIdx -= v.fmIdx * v.fmDecay;
                v.ph2 += v.inc2; if (v.ph2 >= 1.0f) v.ph2 -= 1.0f;
                v.ph1 += v.inc1; if (v.ph1 >= 1.0f) v.ph1 -= 1.0f;
                y = std::sin (kTwoPi * v.ph1 + v.fmIdx * std::sin (kTwoPi * v.ph2)) * 0.5f;
                break;
            }
            default:
            {
                v.pitchEnv *= 0.9992f;
                v.ph1 += v.inc1 * (1.0f + 1.5f * v.pitchEnv); if (v.ph1 >= 1.0f) v.ph1 -= 1.0f;
                y = std::sin (kTwoPi * v.ph1) * 0.6f;
                break;
            }
        }
        return y * v.env * (0.25f + 0.75f * v.vel);
    }
};

} // namespace dh::orr
