#pragma once
// DeVicenti: the master bus (plan §13.2): TAPE → GLUE → LIMIT → volume, and the DUCK envelope.
//   TAPE   wow and flutter (a modulated read of a 1.5 ms base delay, always present: constant latency), soft tape
//          saturation, hiss and vinyl crackle (Poisson ticks and pops shaped by a short filter, plus surface
//          noise), everything a closed function of the absolute position.
//   GLUE   the Lacquer bus compressor: threshold and makeup follow one amount, ratio 2:1 → 4:1, 10 ms attack,
//          auto release (150 ms / 1.5 s detectors averaged).
//   DUCK   an envelope follower on the high-passed KIT kick lane (the sidechain high-pass keeps the sub from
//          pumping itself); the engine applies the gain to the TONE tracks only.
//   LIMIT  a soft limiter without look-ahead (instant attack on a smoothed peak, 80 ms release) driven by the amount,
//          ceiling −1 dBFS, then a soft clip: a safety ceiling is always on.

#include "Base.h"
#include "../../../lacquer/Source/dsp/Dynamics.h"      // dh::lq::Compressor

namespace dh::dv {

struct MasterParams { float tape = 0.0f, noise = 0.3f, glue = 0.0f, limit = 0.0f, volumeDb = 0.0f; };

class Master
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate; base = (int) std::lround (0.0015 * fs);
        line[0].prepare (base * 2 + 16); line[1].prepare (base * 2 + 16);
        comp.prepare (fs); compSlow.prepare (fs);
        dc[0].set (8.0f, 0.707f, (float) fs); dc[1].set (8.0f, 0.707f, (float) fs);
        relSig = std::exp (-1.0f / (2.0f * (float) fs));
        reset();
    }
    void reset()
    {
        line[0].clear(); line[1].clear(); comp.reset(); compSlow.reset(); dc[0].reset(); dc[1].reset(); rng.seed (0xC0FFEEu);
        tapeSm = 0; limEnv = 0; grDb = 0; sigEnv = 0; for (auto& c : click) c = 0; for (auto& c : clickLp) c = 0; surf[0] = surf[1] = 0; lastGlue = -1.0f;
    }
    int latency() const { return base; }
    float gainReductionDb() const { return grDb; }

    void process (float* L, float* R, int n, const MasterParams& p, int64_t pos)
    {
        if (p.glue != lastGlue)
        {
            lastGlue = p.glue;
            dh::lq::CompParams cp; cp.thresholdDb = -4.0 - 22.0 * p.glue; cp.ratio = 2.0 + 2.0 * p.glue; cp.kneeDb = 8.0;
            cp.attackMs = 10.0; cp.releaseMs = 150.0; cp.makeupDb = (22.0 * p.glue) * (1.0 - 1.0 / cp.ratio) * 0.5;
            makeup = cp.makeupDb;
            comp.set (cp); dh::lq::CompParams cs = cp; cs.releaseMs = 1500.0; compSlow.set (cs);
        }
        const float k = 1.0f - std::exp (-1.0f / (0.05f * (float) fs));
        const float vol = dbToGain (p.volumeDb);
        const float limDrive = dbToGain (12.0f * p.limit), ceiling = 0.891f;
        const float relC = std::exp (-1.0f / (0.08f * (float) fs));
        for (int i = 0; i < n; ++i)
        {
            tapeSm += (p.tape - tapeSm) * k;
            float l = L[i], r = R[i];
            line[0].push (l); line[1].push (r);
            const int64_t s = pos + i;
            if (tapeSm < 1e-4f) { l = line[0].at (base + 1); r = line[1].at (base + 1); }
            else
            {
                const double t = (double) s / fs;
                const float a = tapeSm;
                const double pw = 0.48 * t, pf = 7.1 * t + 0.37;
                const float wowS = (float) (0.003 / (2.0 * 3.14159265358979323846 * 0.48) * fs) * a, flS = (float) (0.0008 / (2.0 * 3.14159265358979323846 * 7.1) * fs) * a;
                const float m = wowS * sinT ((float) (pw - std::floor (pw))) + flS * sinT ((float) (pf - std::floor (pf)));
                const float d = (float) base + 1.0f - m;
                l = line[0].read (d); r = line[1].read (d);
                const float sd = 1.0f + 0.8f * a;
                l = fastTanh (l * sd) / sd; r = fastTanh (r * sd) / sd;
                // Hiss, surface noise and crackle, scaled by the noise amount and by the programme: the noise follows
                // the music in and fades out 2 s after it (a stopped plugin is silent, not hissing).
                const float lvl = std::max (std::fabs (L[i]), std::fabs (R[i]));
                sigEnv = lvl > sigEnv ? sigEnv + (lvl - sigEnv) * 0.01f : sigEnv * relSig;
                const float nAmt = a * p.noise * std::min (1.0f, sigEnv * 40.0f);
                if (nAmt > 1e-4f)
                {
                    const float h = 2.0e-4f * nAmt;
                    l += h * rng.bi(); r += h * rng.bi();
                    for (int c = 0; c < 2; ++c)
                    {
                        // Surface noise: low-passed white, very quiet.
                        surf[c] += 0.05f * (rng.bi() - surf[c]);
                        // Ticks: Poisson, up to ~35 per second; a few are louder pops.
                        if (rng.uni() < (float) (35.0 * nAmt / fs))
                        {
                            const float v = rng.uni();
                            const float amp = (v < 0.08f ? 0.25f + 0.4f * v : 0.05f + 0.25f * v * v) * nAmt;
                            click[c] += (rng.uni() < 0.5f ? -amp : amp);
                        }
                        clickLp[c] += 0.45f * (click[c] - clickLp[c]);
                        const float tick = click[c] - clickLp[c] * 0.6f;
                        click[c] *= 0.82f;
                        (c == 0 ? l : r) += tick + surf[c] * 1.2e-3f * nAmt;
                    }
                }
            }
            // DC and subsonic cleanup (two poles at 8 Hz) before the dynamics: saturation of asymmetric material
            // leaves a little DC, and after the limiter a filter could push peaks past the ceiling.
            l = dc[0].highpass (l); r = dc[1].highpass (r);
            if (p.glue > 0.001f)
            {
                const double det = std::max (std::fabs (l), std::fabs (r));
                comp.gainFor (det); compSlow.gainFor (det);
                const double gr = 0.5 * (comp.currentGrDb() + compSlow.currentGrDb());
                const float g = dbToGain ((float) (makeup - gr));
                l *= g; r *= g; grDb = (float) gr;
            }
            else grDb = 0.0f;
            l *= vol; r *= vol;
            if (p.limit > 0.001f)
            {
                l *= limDrive; r *= limDrive;
                const float pk = std::max (std::fabs (l), std::fabs (r));
                limEnv = pk > limEnv ? pk : pk + relC * (limEnv - pk);
                if (limEnv > ceiling) { const float g = ceiling / limEnv; l *= g; r *= g; }
            }
            L[i] = soft (l); R[i] = soft (r);
        }
    }

private:
    double fs = 48000.0; int base = 72;
    Line line[2];
    dh::lq::Compressor comp, compSlow; double makeup = 0.0;
    Svf dc[2]; Noise rng;
    float tapeSm = 0, limEnv = 0, grDb = 0, click[2] {}, clickLp[2] {}, surf[2] {}, lastGlue = -1.0f, sigEnv = 0.0f, relSig = 0.9999f;
    static inline float soft (float x) { const float m = std::fabs (x); return m < 0.94f ? x : std::copysign (0.94f + 0.06f * fastTanh ((m - 0.94f) / 0.06f), x); }
};

// Duck: follows the high-passed key and returns the gain for the ducked tracks.
class Ducker
{
public:
    void prepare (double sampleRate) { fs = (float) sampleRate; hp.set (90.0f, 0.707f, fs); reset(); }
    void reset() { env = 0; hp.reset(); }
    inline float process (float key, float amount, float releaseS)
    {
        const float x = std::fabs (hp.highpass (key));
        const float att = 1.0f - std::exp (-1.0f / (0.002f * fs)), rel = 1.0f - std::exp (-1.0f / (std::max (0.03f, releaseS) * fs));
        env += (x - env) * (x > env ? att : rel);
        // Up to −18 dB at amount 1, reached when the key passes ~ −12 dBFS.
        const float depth = clampf (env * 4.0f, 0.0f, 1.0f) * amount;
        return dbToGain (-18.0f * depth);
    }
private:
    float fs = 48000.0f, env = 0.0f;
    Svf hp;
};

} // namespace dh::dv
