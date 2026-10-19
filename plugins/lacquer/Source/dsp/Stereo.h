#pragma once
// Etapa estéreo (docs/research.md §4.5, §6.3-6): matriz medio/lado, graves en mono por debajo de Fc (pasa altos
// LR4 en el lado; el medio no se toca), ganancia del lado arriba de
// Fc y un estante agudo en el lado. Sin retardos ni trucos de Haas: lo que sale suma bien en mono.

#include "Filters.h"

namespace dh::lq {

struct StereoParams { bool on = false; double monoHz = 0.0, sideGainDb = 0.0, sideHighShelfDb = 0.0, sideHighShelfHz = 5000.0; };

class Stereo
{
public:
    void prepare (double sampleRate, const StereoParams& p)
    {
        fs = sampleRate; P = p;
        if (p.monoHz > 0.0) xs.set (p.monoHz, fs);
        hs.set (highShelf (p.sideHighShelfHz, p.sideHighShelfDb, fs));
        g = std::pow (10.0, p.sideGainDb / 20.0);
        reset();
    }
    void reset() { xs.reset(); hs.reset(); }
    inline void process (double& l, double& r)
    {
        if (! P.on) return;
        double m = 0.5 * (l + r), s = 0.5 * (l - r);
        if (P.monoHz > 0.0)
        {
            // Sólo el lado pasa por el pasa altos: el medio queda intacto (girarle la fase subiría los picos de un
            // master ya limitado). El desfasaje del lado arriba de 5·Fc es de menos de 20°.
            double lo, hi;
            xs.process (s, lo, hi); s = hi;
        }
        if (P.sideHighShelfDb != 0.0) s = hs.process (s);
        s *= g;
        l = m + s; r = m - s;
    }
private:
    double fs = 48000.0, g = 1.0;
    StereoParams P;
    LR4 xs;
    Biquad hs;
};

} // namespace dh::lq
