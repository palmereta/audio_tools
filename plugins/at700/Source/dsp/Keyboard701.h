#pragma once
// 701A KEYBOARD CONTROLLER: 61 teclas (C1–C6 = 0–5 V, 1 V/oct). Como el original es duofónico:
// KEY CV1 = la tecla más grave apretada, KEY CV2 = la más aguda; la tensión queda retenida al soltar
// (S/H, con una caída lenta por fuga si se activa). GATE +15 V mientras haya una tecla; TRIG +15 V,
// 2 ms, cada vez que cambia la tecla más grave. Portamento RC después del S/H; PITCH ±6 semitonos y
// la palanca BENDER por el selector de rango (5 V, 1 V, 0,1 V, OFF).
//
// Extensiones: modo mono (última nota) y polifonía de hasta 8 voces (cada voz tiene su CV, su gate
// y su trigger; la consola se duplica por voz en el motor).

#include "Common.h"
#include <array>
#include <vector>

namespace dh {

constexpr int kMaxVoices700 = 8;

class Keyboard701
{
public:
    enum Mode { Duo = 0, Mono = 1, Poly = 2 };

    struct Voice
    {
        int note = -1;
        float t1 = 0.0f, t2 = 0.0f;       // tensiones objetivo (antes del portamento)
        float cv1 = 0.0f, cv2 = 0.0f;     // después del portamento
        float vel = 0.0f;
        bool gate = false;
        int trig = 0;                     // muestras restantes del pulso de trigger
        uint32_t age = 0;
    };

    int mode = Duo, polyVoices = 6;
    float portaTime = 0.3f;               // s
    bool porta = false, droop = false;
    float pitchSemis = 0.0f, bendV = 0.0f;
    float modWheel = 0.0f, aftertouch = 0.0f;
    std::array<Voice, kMaxVoices700> v {};

    int voices() const { return mode == Poly ? polyVoices : 1; }

    void prepare (float sampleRate) { fs = sampleRate; trigLen = (int) std::lround (0.002f * fs); held.reserve (128); }

    static float noteToV (int n) { return (float) (n - 36) / 12.0f; }

    void noteOn (int note, float velocity)
    {
        if (std::find (held.begin(), held.end(), note) == held.end()) held.push_back (note);
        lastVel = velocity;
        if (mode == Poly)
        {
            // Una voz libre (la que se soltó hace más tiempo) o se roba la más vieja.
            int best = -1; uint32_t bestAge = UINT32_MAX;
            for (int i = 0; i < polyVoices; ++i)
                if (v[(size_t) i].note == note) { best = i; break; }
            if (best < 0)
                for (int i = 0; i < polyVoices; ++i)
                    if (! v[(size_t) i].gate && v[(size_t) i].age < bestAge) { bestAge = v[(size_t) i].age; best = i; }
            if (best < 0)
            {
                bestAge = UINT32_MAX;
                for (int i = 0; i < polyVoices; ++i) if (v[(size_t) i].age < bestAge) { bestAge = v[(size_t) i].age; best = i; }
            }
            auto& vv = v[(size_t) best];
            vv.note = note; vv.t1 = vv.t2 = noteToV (note); vv.vel = velocity; vv.gate = true; vv.trig = trigLen; vv.age = ++ageCounter;
            if (! porta) { vv.cv1 = vv.t1; vv.cv2 = vv.t2; }
            return;
        }
        updateMono (true);
    }

    void noteOff (int note)
    {
        held.erase (std::remove (held.begin(), held.end(), note), held.end());
        if (mode == Poly)
        {
            for (int i = 0; i < kMaxVoices700; ++i)
                if (v[(size_t) i].note == note && v[(size_t) i].gate) { v[(size_t) i].gate = false; v[(size_t) i].age = ++ageCounter; }
            return;
        }
        updateMono (false);
    }

    void allOff()
    {
        held.clear();
        for (auto& vv : v) vv.gate = false;
    }

    bool anyGate() const
    {
        if (mode != Poly) return v[0].gate;
        for (int i = 0; i < polyVoices; ++i) if (v[(size_t) i].gate) return true;
        return false;
    }

    // Una vez por muestra base.
    inline void process()
    {
        const float c = porta ? 1.0f - fastExp (-1.0f / std::max (1.0f, portaTime * 0.5f * fs)) : 1.0f;
        const float dr = droop ? 0.004f / fs : 0.0f;         // fuga del S/H: −4 mV/s
        for (int i = 0; i < voices(); ++i)
        {
            auto& vv = v[(size_t) i];
            if (dr > 0.0f && ! vv.gate)      // la fuga lleva el CV hacia 0 V y se detiene ahí
            {
                vv.t1 = vv.t1 > 0.0f ? std::max (0.0f, vv.t1 - dr) : std::min (0.0f, vv.t1 + dr);
                vv.t2 = vv.t2 > 0.0f ? std::max (0.0f, vv.t2 - dr) : std::min (0.0f, vv.t2 + dr);
            }
            vv.cv1 += (vv.t1 - vv.cv1) * c;
            vv.cv2 += (vv.t2 - vv.cv2) * c;
            if (vv.trig > 0) --vv.trig;
        }
    }

    float cv1 (int i) const { return v[(size_t) i].cv1 + offset(); }
    float cv2 (int i) const { return v[(size_t) i].cv2 + offset(); }

private:
    float offset() const { return pitchSemis / 12.0f + bendV; }

    void updateMono (bool press)
    {
        auto& vv = v[0];
        if (held.empty()) { vv.gate = false; return; }
        const int lo = *std::min_element (held.begin(), held.end());
        const int hi = *std::max_element (held.begin(), held.end());
        const int last = held.back();
        const bool wasGate = vv.gate;
        if (mode == Duo)
        {
            const float newT1 = noteToV (lo);
            if (! wasGate || std::abs (newT1 - vv.t1) > 1.0e-4f) vv.trig = trigLen;    // cambia la tecla más grave
            vv.t1 = newT1; vv.t2 = noteToV (hi); vv.note = lo;
        }
        else
        {
            if (press || last != vv.note) vv.trig = trigLen;
            vv.t1 = vv.t2 = noteToV (last); vv.note = last;
        }
        vv.vel = lastVel;
        vv.gate = true;
        if (! porta) { vv.cv1 = vv.t1; vv.cv2 = vv.t2; }
    }

    std::vector<int> held;
    float fs = 48000.0f, lastVel = 0.8f;
    int trigLen = 96;
    uint32_t ageCounter = 0;
};

} // namespace dh
