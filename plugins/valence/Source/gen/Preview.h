#pragma once
// Sonido de prueba de Valence: un sinte chico con cuatro timbres de este mundo (pluck corto, bajo con filtro, stab de
// acorde, sub), uno por capa, paneo por capa, un delay y una reverb corta. El CC 74 de una capa (su fila de CC) abre y
// cierra el filtro de su voz. Sirve para escuchar sin rutear nada; el MIDI sale igual. Determinista: la misma entrada
// suena igual en cualquier instancia (athosttest repro).

#include "Common.h"
#include "Blep.h"
#include "Fx.h"
#include "Engine.h"
#include <array>
#include <cmath>
#include <vector>

namespace dh::val {

enum Voice : int { VoicePluck = 0, VoiceBass, VoiceStab, VoiceSub, VoiceCount };
inline const char* voiceName (int v) { static const char* N[VoiceCount] = { "Pluck", "Bass", "Stab", "Sub" }; return N[std::clamp (v, 0, VoiceCount - 1)]; }

class Preview
{
public:
    void prepare (float sampleRate, int maxBlock)
    {
        fs = sampleRate;
        rev.prepare (fs); dly.prepare (fs);
        volS.setTime (0.03f, fs); spaceS.setTime (0.03f, fs);
        L.assign ((size_t) maxBlock, 0.0f); R.assign ((size_t) maxBlock, 0.0f);
        sL.assign ((size_t) maxBlock, 0.0f); sR.assign ((size_t) maxBlock, 0.0f);
        reset();
    }
    // Como recién preparado (un render tiene que sonar igual que una instancia nueva).
    void reset()
    {
        for (auto& v : voices) v = VoiceState {};
        counter = 0; bright.fill (0.5f);
        rev.restart(); dly.restart();
        primed = false;
    }

    // Eventos del motor; el timbre de cada capa.
    void handle (const OutEvent& e, const std::array<int, kLayers>& voiceOf)
    {
        const int st = e.data[0] & 0xF0;
        if (e.layer < 0 || e.layer >= kLayers) { if (st == 0xB0 && (e.data[1] == 123 || e.data[1] == 120)) for (auto& v : voices) v.gate = false; return; }
        if (st == 0x90 && e.data[2] > 0) { noteOn (e.layer, voiceOf[(size_t) e.layer], e.data[1], (float) e.data[2] / 127.0f, (e.flags & FlagAccent) != 0); lastVoice->ch = e.data[0] & 15; }
        else if (st == 0x80 || (st == 0x90 && e.data[2] == 0)) { for (auto& v : voices) if (v.active && v.gate && v.ch == (e.data[0] & 15) && v.note == e.data[1]) v.gate = false; }   // como un sinte: nota y canal
        else if (st == 0xB0 && e.data[1] == 74) bright[(size_t) e.layer] = (float) e.data[2] / 127.0f;
    }

    // Suma el sonido en out (estéreo), entre las muestras [from, to).
    void render (float* outL, float* outR, int from, int to, float volume, float space)
    {
        if ((size_t) to > L.size()) { L.resize ((size_t) to); R.resize ((size_t) to); sL.resize ((size_t) to); sR.resize ((size_t) to); }
        for (int i = from; i < to; ++i) { L[(size_t) i] = R[(size_t) i] = sL[(size_t) i] = sR[(size_t) i] = 0.0f; }
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const float b = bright[(size_t) v.layer];
            for (int i = from; i < to && v.active; ++i)
            {
                const float y = tick (v, b);
                L[(size_t) i] += y * v.panL; R[(size_t) i] += y * v.panR;
                sL[(size_t) i] += y * v.panL * v.send; sR[(size_t) i] += y * v.panR * v.send;
            }
        }
        const int n = to - from;
        if (n <= 0) return;
        dly.process (sL.data() + from, sR.data() + from, n, 0.375f, 0.42f, 0.4f, 0.45f);
        rev.process (sL.data() + from, sR.data() + from, n, 0.55f, 0.6f, 1.0f);
        // Volumen y espacio suavizados por muestra (sin escalones a la frecuencia de bloque: athosttest zipper). El
        // volumen va después de la saturación, que así no cambia de color con él.
        if (! primed) { volS.reset (volume); spaceS.reset (space); primed = true; }
        for (int i = from; i < to; ++i)
        {
            const float sp = spaceS.process (space), vo = volS.process (volume);
            const float l = L[(size_t) i] + sL[(size_t) i] * sp, r = R[(size_t) i] + sR[(size_t) i] * sp;
            outL[i] += fastTanh (l * 0.9f) / 0.9f * vo; outR[i] += fastTanh (r * 0.9f) / 0.9f * vo;
        }
    }

private:
    struct VoiceState
    {
        bool active = false, gate = false;
        int layer = 0, kind = 0, note = 60, ch = 0;
        float vel = 0.5f, env = 0.0f, fenv = 0.0f, att = 0.01f, dec = 0.001f, sus = 0.0f, rel = 0.01f, fdec = 0.001f;
        float ph1 = 0, ph2 = 0, ph3 = 0, inc1 = 0, inc2 = 0, inc3 = 0, lp = 0, lp2 = 0, bp = 0, click = 0;
        float cutBase = 1000.0f, cutEnv = 2000.0f, res = 0.0f;
        float panL = 0.7f, panR = 0.7f, send = 0.3f;
        bool attacking = true;
        uint32_t order = 0;
    };
    float fs = 48000.0f;
    std::array<VoiceState, 48> voices {};
    std::array<float, kLayers> bright {};
    uint32_t counter = 0;
    VoiceState* lastVoice = nullptr;
    std::vector<float> L, R, sL, sR;
    PlateReverb rev; StereoDelay dly;
    Smoother volS, spaceS; bool primed = false;

    static float hz (int n) { return 440.0f * fastExp2 (((float) n - 69.0f) / 12.0f); }
    float coefFor (float seconds) const { return 1.0f - fastExp (-1.0f / std::max (1.0f, seconds * fs)); }

    void noteOn (int layer, int kind, int note, float vel, bool accent)
    {
        VoiceState* v = nullptr;
        for (auto& x : voices) if (! x.active) { v = &x; break; }
        if (v == nullptr) { v = &voices[0]; for (auto& x : voices) if (x.order < v->order) v = &x; }   // roba la más vieja
        *v = VoiceState {};
        lastVoice = v;
        v->active = v->gate = true; v->layer = layer; v->kind = std::clamp (kind, 0, VoiceCount - 1); v->note = note; v->vel = vel; v->order = ++counter;
        const float f = hz (note);
        const float pan = std::clamp (((float) layer - 1.5f) / 2.0f, -1.0f, 1.0f) * 0.6f;
        v->panL = std::sqrt (0.5f * (1.0f - pan)); v->panR = std::sqrt (0.5f * (1.0f + pan));
        v->inc1 = f / fs; v->inc2 = f * 1.006f / fs; v->inc3 = f * 0.5f / fs;
        const float acc = accent ? 1.35f : 1.0f;
        switch (v->kind)
        {
            case VoiceBass:   // sierra + cuadrada a la octava de abajo, pasa bajos resonante con su envolvente
                v->att = coefFor (0.002f); v->dec = coefFor (0.35f); v->sus = 0.55f; v->rel = coefFor (0.06f); v->fdec = coefFor (0.18f / acc);
                v->cutBase = 140.0f; v->cutEnv = 1800.0f * acc; v->res = 0.55f; v->send = 0.08f; v->panL = v->panR = 0.7071f; break;
            case VoiceStab:   // dos sierras desafinadas por un pasa banda, caída corta: el acorde de dub
                v->att = coefFor (0.003f); v->dec = coefFor (0.22f); v->sus = 0.0f; v->rel = coefFor (0.12f); v->fdec = coefFor (0.12f);
                v->cutBase = 500.0f; v->cutEnv = 2200.0f * acc; v->res = 0.3f; v->send = 0.55f; break;
            case VoiceSub:    // seno con un clic de ataque
                v->att = coefFor (0.004f); v->dec = coefFor (1.2f); v->sus = 0.8f; v->rel = coefFor (0.09f); v->fdec = coefFor (0.01f);
                v->click = 1.0f; v->send = 0.0f; v->panL = v->panR = 0.7071f; break;
            default:          // pluck: triángulo y una sierra suave, el filtro se cierra rápido
                v->att = coefFor (0.0015f); v->dec = coefFor (0.16f * acc); v->sus = 0.0f; v->rel = coefFor (0.08f); v->fdec = coefFor (0.07f);
                v->cutBase = 700.0f; v->cutEnv = 5200.0f * acc; v->res = 0.2f; v->send = 0.4f; break;
        }
        v->fenv = 1.0f;
    }

    inline float tick (VoiceState& v, float brightness)
    {
        // Envolventes: ataque al tope, caída al sostén, release al soltar.
        if (v.gate)
        {
            if (v.attacking) { v.env += (1.02f - v.env) * v.att; if (v.env >= 1.0f) { v.env = 1.0f; v.attacking = false; } }
            else v.env += (v.sus - v.env) * v.dec;
        }
        else v.env -= v.env * v.rel;
        if (! v.gate && v.env < 1.0e-4f) { v.active = false; return 0.0f; }
        if (! v.gate && v.sus <= 0.0f && v.env < 2.0e-4f) { v.active = false; return 0.0f; }
        v.fenv -= v.fenv * v.fdec;
        float y = 0.0f;
        if (v.kind == VoiceSub)
        {
            v.ph1 += v.inc1; if (v.ph1 >= 1.0f) v.ph1 -= 1.0f;
            v.click *= 0.995f;
            y = std::sin (kTwoPi * v.ph1) * 0.85f + v.click * 0.15f * std::sin (kTwoPi * v.ph1 * 4.0f);
            return y * v.env * (0.35f + 0.65f * v.vel);
        }
        v.ph1 += v.inc1; if (v.ph1 >= 1.0f) v.ph1 -= 1.0f;
        v.ph2 += v.inc2; if (v.ph2 >= 1.0f) v.ph2 -= 1.0f;
        v.ph3 += v.inc3; if (v.ph3 >= 1.0f) v.ph3 -= 1.0f;
        const float s1 = 2.0f * v.ph1 - 1.0f - polyBlep (v.ph1, v.inc1);
        const float s2 = 2.0f * v.ph2 - 1.0f - polyBlep (v.ph2, v.inc2);
        float x;
        switch (v.kind)
        {
            case VoiceBass: { const float sq = v.ph3 < 0.5f ? 1.0f : -1.0f; x = 0.7f * s1 + 0.45f * (sq - polyBlep (v.ph3, v.inc3) + polyBlep (std::fmod (v.ph3 + 0.5f, 1.0f), v.inc3)); break; }
            case VoiceStab: x = 0.5f * (s1 + s2); break;
            default: x = 0.6f * (4.0f * std::abs (v.ph1 - 0.5f) - 1.0f) + 0.35f * s1; break;
        }
        // Filtro de dos polos con resonancia (estado variable); el CC 74 lo abre (0,5 = el del preset).
        const float cut = std::clamp ((v.cutBase + v.cutEnv * v.fenv * (0.4f + 0.6f * v.vel)) * fastExp2 ((brightness - 0.5f) * 3.0f), 30.0f, 0.42f * fs);
        const float gcoef = std::min (0.98f, 2.0f * std::sin (kPi * cut / fs));
        const float q = 1.0f - 0.85f * v.res;
        v.lp += gcoef * v.bp;
        const float hp = x - v.lp - q * v.bp;
        v.bp += gcoef * hp;
        y = v.kind == VoiceStab ? v.bp * 1.2f : v.lp;
        y = fastTanh (y * 1.2f) * 0.6f;
        return y * v.env * (0.3f + 0.7f * v.vel);
    }
};

} // namespace dh::val
