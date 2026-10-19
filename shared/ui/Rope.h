#pragma once
// Cable de patcheo como cuerda de Verlet: los extremos quedan fijos en la punta de las fichas y
// el resto cae con la gravedad. El largo se fija al enchufar (como un cable real) y, si se
// estira más allá, la cuerda se tensa en lugar de crecer.

#include <JuceHeader.h>

namespace dh {

struct Rope
{
    static constexpr int kPoints = 22;
    std::array<juce::Point<float>, kPoints> p, prev;
    float length = 100.0f;
    float energy = 1.0f;

    void init (juce::Point<float> a, juce::Point<float> b, float slack = 1.18f, float extra = 34.0f)
    {
        length = a.getDistanceFrom (b) * slack + extra;
        for (int i = 0; i < kPoints; ++i)
        {
            const float t = (float) i / (kPoints - 1);
            // Arranque con una panza parabólica, para que no "caiga" de golpe al aparecer.
            const float sag = 4.0f * t * (1.0f - t) * length * 0.18f;
            p[(size_t) i] = a + (b - a) * t + juce::Point<float> (0.0f, sag);
            prev[(size_t) i] = p[(size_t) i];
        }
        energy = 1.0f;
    }

    // dt en segundos; a y b son los extremos (fijos).
    void step (juce::Point<float> a, juce::Point<float> b, float dt)
    {
        const float span = a.getDistanceFrom (b);
        const float L = std::max (length, span * 1.01f);
        const float seg = L / (kPoints - 1);
        const juce::Point<float> gravity (0.0f, 2400.0f);
        const float damping = 0.965f;
        float e = 0.0f;
        for (int i = 1; i < kPoints - 1; ++i)
        {
            auto& x = p[(size_t) i];
            const auto v = (x - prev[(size_t) i]) * damping;
            prev[(size_t) i] = x;
            x += v + gravity * (dt * dt);
            e += v.getDistanceSquaredFromOrigin();
        }
        p.front() = a; p.back() = b;
        for (int it = 0; it < 24; ++it)
        {
            for (int i = 0; i < kPoints - 1; ++i)
            {
                auto& x1 = p[(size_t) i];
                auto& x2 = p[(size_t) i + 1];
                const auto d = x2 - x1;
                const float dist = std::max (0.001f, d.getDistanceFromOrigin());
                // Sólo resiste el estiramiento (un cable se dobla libre, no se comprime).
                if (dist <= seg) continue;
                const auto delta = d * ((dist - seg) / dist);
                const bool fix1 = i == 0, fix2 = i + 1 == kPoints - 1;
                if (fix1 && fix2) continue;
                if (fix1) x2 -= delta;
                else if (fix2) x1 += delta;
                else { x1 += delta * 0.5f; x2 -= delta * 0.5f; }
            }
            p.front() = a; p.back() = b;
        }
        energy = e;
    }

    bool settled() const { return energy < 0.02f; }

    juce::Path path() const
    {
        juce::Path path;
        path.startNewSubPath (p[0]);
        for (int i = 1; i < kPoints - 1; ++i)
        {
            const auto mid = (p[(size_t) i] + p[(size_t) i + 1]) * 0.5f;
            path.quadraticTo (p[(size_t) i], mid);
        }
        path.lineTo (p.back());
        return path;
    }
};

} // namespace dh
