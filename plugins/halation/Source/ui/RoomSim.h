#pragma once
// La sala que se ve: una simulación de partículas de luz alimentada por el motor (sin JUCE, la dibuja la vista).
//
// Cada ataque de la entrada lanza un pulso por línea activa. El pulso viaja de la fuente a una pared y vuelve al
// oyente en exactamente el tiempo de esa línea: la luz llega cuando suena el eco. En cada vuelta pierde lo que
// pierde la realimentación (tamaño y brillo). Con `space` alto, cada rebote se parte en muchas partículas chicas
// en direcciones al azar: la niebla de la reverb. Congelado: todo queda suspendido y tiembla apenas, como polvo en
// un haz. Shimmer: las partículas suben y se aclaran, una capa por intervalo. Reverse: convergen a la fuente.
// Granos: se rompen en fragmentos. Las paredes siguen al tamaño real de la sala (respiran o se cierran).

#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace dh::sp::view {

struct V3 { float x = 0, y = 0, z = 0; };
inline V3 operator+ (V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator- (V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator* (V3 a, float k) { return { a.x * k, a.y * k, a.z * k }; }
inline float len (V3 a) { return std::sqrt (a.x * a.x + a.y * a.y + a.z * a.z); }

struct Particle
{
    V3 p, v;              // posición (m) y velocidad (m/s)
    float life = 0;       // 0..1, se apaga al llegar a 0
    float decay = 0.5f;   // pérdida de vida por segundo
    float size = 1;       // tamaño relativo (sigue al nivel)
    float warm = 0.5f;    // 0 frío (agudo) … 1 tibio (grave)
    float lift = 0;       // capa de shimmer (sube)
    int bounces = 0;
    bool fragment = false;
};

struct SimInputs
{
    float space = 0, sizeScale = 1, feedback = 0.4f, decayS = 2, freeze = 0;
    int shimmerLayers = 0; float reverse = 0, grains = 0;
    float lineMs[8] {}; float lineEnergy[8] {};
    int activeLines = 2;
};

class RoomSim
{
public:
    static constexpr int kMax = 1400;
    // Sala base (m): ancho × profundidad × alto; la fuente adelante a la izquierda, el oyente adelante a la derecha.
    V3 room { 12.0f, 16.0f, 7.0f };
    V3 source { -2.5f, -5.5f, 1.4f }, listener { 2.5f, -5.5f, 1.4f };     // se recalculan con el tamaño de la sala

    void onset (float level, float bright, const SimInputs& in)
    {
        if (count >= kMax - 16) return;
        const V3 half = halfRoom (in);
        placeMarkers (half);
        const int lines = std::max (1, std::min (8, in.activeLines));
        for (int k = 0; k < lines; ++k)
        {
            // Rumbo: hacia una pared distinta por línea; la distancia ida y vuelta = c_vis × tiempo de la línea.
            const float ang = 6.2831853f * ((float) k / (float) lines) + 0.6f;
            V3 dir { std::cos (ang), std::sin (ang) * 0.9f + 0.35f, 0.15f * std::sin (1.7f * ang) };
            const float d = len (dir); dir = dir * (1.0f / d);
            const float ms = std::max (5.0f, in.lineMs[k]);
            // La partícula debe ir y volver en `ms`: la "velocidad de la luz" de la vista se ajusta a la sala.
            const float path = 2.0f * distToWall (source, dir, half);
            Particle q;
            // En modo reverb los rebotes son tan rápidos que no se verían: la vista los frena (son niebla).
            const float slow = 1.0f - 0.85f * in.space;
            q.p = source; q.v = dir * (path / (ms * 1e-3f) * slow);
            q.life = 1.0f; q.size = 0.7f + 2.0f * std::sqrt (std::min (1.0f, level * 6.0f));
            q.warm = 1.0f - std::min (1.0f, bright * 2.5f);
            q.decay = 0.9f / std::max (0.3f, std::min (in.decayS, 20.0f));     // se apaga con la cola (≈ T60)
            add (q);
        }
    }

    void step (float dt, const SimInputs& in)
    {
        const V3 half = halfRoom (in);
        const float frozen = in.freeze;
        const float loss = std::pow (std::max (0.001f, in.feedback), 1.0f);
        // Sólo las que existían al empezar el paso: las que nacen ahora se mueven en el próximo.
        int live = count;
        for (int i = 0; i < live;)
        {
            Particle& q = parts[(size_t) i];
            // Congelado: la velocidad se apaga y queda un temblor de polvo.
            const float k = 1.0f - frozen;
            V3 vel = q.v * k;
            if (frozen > 0.01f) vel = vel + V3 { jitter() * 0.15f, jitter() * 0.15f, jitter() * 0.1f } * frozen;
            // Shimmer: sube por capas; reverse: va hacia la fuente.
            if (q.lift > 0) vel.z += 1.2f * q.lift * k;
            if (in.reverse > 0.01f) vel = vel * (1.0f - in.reverse) + (source - q.p) * (1.6f * in.reverse);
            q.p = q.p + vel * dt;
            // Rebotes contra las paredes (que se mueven con el tamaño de la sala).
            bool hit = false;
            auto wall = [&] (float& pos, float& v, float h) { if (pos > h) { pos = 2 * h - pos; v = -std::fabs (v); hit = true; } else if (pos < -h) { pos = -2 * h - pos; v = std::fabs (v); hit = true; } };
            wall (q.p.x, q.v.x, half.x); wall (q.p.y, q.v.y, half.y);
            if (q.p.z > 2 * half.z) { q.p.z = 4 * half.z - q.p.z; q.v.z = -std::fabs (q.v.z); hit = true; } else if (q.p.z < 0) { q.p.z = -q.p.z; q.v.z = std::fabs (q.v.z); hit = true; }
            if (hit && frozen < 0.5f)
            {
                ++q.bounces;
                q.size *= std::sqrt (loss); q.life *= loss;
                // Con space alto, el rebote se parte en una nube de partículas chicas (difusión).
                // Sólo los pulsos originales se dividen (nunca un fragmento), y sólo mientras tienen cuerpo.
                const int split = q.fragment || q.size < 0.2f || q.life < 0.1f ? 0 : (int) (in.space * in.space * 6.0f);
                for (int s = 0; s < split && count < kMax - 1; ++s)
                {
                    Particle f = q; f.size *= 0.45f; f.life *= 0.7f;
                    const float sp = len (q.v);
                    V3 d { jitter(), jitter(), 0.5f * jitter() }; const float l = std::max (0.1f, len (d));
                    f.v = d * (sp * (0.4f + 0.6f * uni()) / l); f.decay = 0.6f / std::max (0.3f, in.decayS); f.fragment = true;
                    if (in.shimmerLayers > 0) { f.lift = (float) (1 + (int) (uni() * (float) in.shimmerLayers)) / (float) in.shimmerLayers; f.warm *= 0.5f; }
                    add (f);
                }
                // Granos: fragmentos que se desprenden y se frenan.
                if (! q.fragment && in.grains > 0.01f && uni() < in.grains)
                    for (int s = 0; s < 3 && count < kMax - 1; ++s) { Particle f = q; f.size *= 0.25f; f.v = q.v * 0.15f + V3 { jitter(), jitter(), jitter() } * 1.5f; f.decay = 1.2f; f.fragment = true; add (f); }
                if (in.space > 0.6f) q.decay = std::max (q.decay, 0.4f / std::max (0.3f, in.decayS));
            }
            q.life -= q.decay * dt * (1.0f - frozen);
            if (q.life < 0.02f || q.size < 0.03f)
            {
                // Se borra: si hay nacidas en este paso, la última de las viejas toma su lugar y una nueva ocupa ese.
                parts[(size_t) i] = parts[(size_t) (live - 1)];
                parts[(size_t) (live - 1)] = parts[(size_t) (count - 1)];
                --live; --count;
                continue;
            }
            ++i;
        }
        wallScale += (in.sizeScale - wallScale) * std::min (1.0f, dt * 3.0f);
        placeMarkers (halfRoomScaled (wallScale));
    }

    // Niebla: en modo reverb la cola llena la sala aunque no haya ataques nuevos (cantidad según su energía).
    void mist (float energy, float bright, const SimInputs& in, float dt)
    {
        const float rate = in.space * in.space * std::min (60.0f, energy * 4000.0f);
        acc += rate * dt;
        const V3 h = halfRoom (in);
        while (acc >= 1.0f && count < kMax - 1)
        {
            acc -= 1.0f;
            Particle q;
            q.p = { jitter() * h.x * 0.9f, jitter() * h.y * 0.9f, (0.1f + 0.8f * uni()) * 2.0f * h.z };
            q.v = { jitter() * 0.6f, jitter() * 0.6f, jitter() * 0.3f };
            q.life = 0.5f + 0.4f * uni(); q.decay = 0.35f / std::max (0.4f, std::min (in.decayS, 8.0f)) + 0.08f;
            q.size = 0.5f + 1.2f * uni(); q.warm = 1.0f - std::min (1.0f, bright * 2.5f); q.fragment = true;
            if (in.shimmerLayers > 0) q.lift = (float) (1 + (int) (uni() * (float) in.shimmerLayers)) / (float) in.shimmerLayers;
            add (q);
        }
    }
    const Particle* data() const { return parts.data(); }
    int size() const { return count; }
    V3 walls() const { return halfRoomScaled (wallScale); }       // medio ancho, media profundidad, alto/2 actuales

private:
    std::array<Particle, kMax> parts {};
    int count = 0;
    uint32_t seed = 12345u;
    float wallScale = 1.0f, acc = 0.0f;
    float uni() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return (float) (seed >> 8) / 16777216.0f; }
    float jitter() { return 2.0f * uni() - 1.0f; }
    void placeMarkers (V3 h) { source = { -0.24f * h.x, -0.62f * h.y, 1.3f }; listener = { 0.24f * h.x, -0.62f * h.y, 1.3f }; }
    void add (const Particle& q) { if (count < kMax) parts[(size_t) count++] = q; }
    V3 halfRoomScaled (float s) const { const float k = std::cbrt (std::max (0.1f, s)); return { room.x * 0.5f * k, room.y * 0.5f * k, room.z * 0.5f * k }; }
    V3 halfRoom (const SimInputs& in) const { return halfRoomScaled (in.sizeScale); }
    static float distToWall (V3 p, V3 d, V3 h)
    {
        float t = 1e9f;
        if (d.x > 1e-4f) t = std::min (t, (h.x - p.x) / d.x); if (d.x < -1e-4f) t = std::min (t, (-h.x - p.x) / d.x);
        if (d.y > 1e-4f) t = std::min (t, (h.y - p.y) / d.y); if (d.y < -1e-4f) t = std::min (t, (-h.y - p.y) / d.y);
        if (d.z > 1e-4f) t = std::min (t, (2 * h.z - p.z) / d.z); if (d.z < -1e-4f) t = std::min (t, (0 - p.z) / d.z);
        return std::max (0.5f, t);
    }
};

} // namespace dh::sp::view
