#pragma once
// La red de Halation: 8 líneas de retardo realimentadas por una matriz ortogonal. Un solo parámetro, `space`
// (0..1), la lleva de delay a reverb (docs/research.md §2.5, §3.2):
//   - largo de cada línea: del patrón rítmico (tiempo × multiplicadores) a un conjunto de largos de sala sin
//     factores comunes (escalados por `size` y el tipo de sala), interpolados en escala logarítmica;
//   - matriz: rotación θ de 0 (identidad, o la permutación ping-pong) a π/4 (Hadamard: mezcla total), siempre
//     ortogonal (Schlecht y Habets, 2015);
//   - difusión de entrada: de nada a la cadena de pasa todos.
// La caída es siempre un T60 (Jot): cada línea pierde 10^(−3·largo/T60) por vuelta, separada en graves, medios y
// agudos (tres bandas complementarias de un polo), así mover Space no cambia cuánto dura el sonido. Congelar una
// banda es poner su ganancia en 1 y cortarle la entrada. En el lazo, por línea: carácter (digital, cinta, BBD,
// resorte), modulación de lectura, bloqueo de continua y un limitador suave. Con notas para afinar, los largos de
// sala se redondean a múltiplos del período de cada nota: toda la cola suena en esa altura.

#include "Core.h"

namespace dh::sp {

enum class Character { digital, tape, bbd, spring };
enum class RoomType { room, hall, plate, chamber };

struct NetParams
{
    float space = 0.0f;              // 0 delay … 1 reverb
    float timeMs = 375.0f;           // tiempo de delay (ya convertido si va sincronizado)
    float size = 1.0f;               // escala de la sala (0.25..4)
    RoomType room = RoomType::hall;
    float decayS = 3.0f;             // T60 de los medios (s); ≥ 100 = infinito
    float hfRatio = 0.5f;            // T60 de los agudos / T60 de los medios (0.05..1)
    float lfRatio = 1.0f;            // T60 de los graves / T60 de los medios (0.3..2)
    float lowCutHz = 40.0f;          // pasa altos del lazo (continua y retumbe)
    float diffusion = 0.7f;          // difusión de entrada (a space = 1)
    float modDepth = 0.25f;          // 0..1
    float modRate = 0.4f;            // Hz
    bool pingPong = true;
    int pattern = 0;
    Character character = Character::digital;
    float wear = 0.0f;               // desgaste de la cinta (0..1)
    float elastic = 1.0f;            // 1 = el tiempo se desliza como cinta; 0 = salto limpio con fundido
    int freezeMask = 0;              // bandas congeladas: 1 graves, 2 medios, 4 agudos (7 = todo)
    float width = 1.0f;
    int nTune = 0; float tuneHz[4] {}; float tuneAmount = 0.0f;   // cola afinada (0..1)
};

struct Pattern { const char* name; float mult[4]; float gain[4]; };
inline const std::array<Pattern, 8>& patterns()
{
    static const std::array<Pattern, 8> p {{
        { "Single",      { 1.0f,  1.0f,   1.0f,  1.0f },  { 1.0f, 0.0f, 0.0f, 0.0f } },
        { "Dotted",      { 0.75f, 1.0f,   1.0f,  1.0f },  { 1.0f, 0.0f, 0.0f, 0.0f } },
        { "Triplet",     { 0.6667f, 1.0f, 1.0f,  1.0f },  { 1.0f, 0.0f, 0.0f, 0.0f } },
        { "Dotted + 1/4",{ 0.75f, 1.0f,   1.0f,  1.0f },  { 0.8f, 0.6f, 0.0f, 0.0f } },
        { "Rain",        { 0.75f, 0.5f,   1.25f, 1.5f },  { 0.8f, 0.5f, 0.4f, 0.3f } },
        { "Gallop",      { 0.25f, 0.75f,  1.0f,  1.0f },  { 0.7f, 0.7f, 0.0f, 0.0f } },
        { "Cascade",     { 0.5f, 0.75f,   1.0f,  1.5f },  { 0.6f, 0.5f, 0.45f, 0.4f } },
        { "Swing",       { 0.6667f, 1.3333f, 1.0f, 1.0f },{ 0.8f, 0.6f, 0.0f, 0.0f } },
    }};
    return p;
}

// Largos de sala (ms a size = 1) por tipo, y la difusión y modulación que les van.
struct RoomDef { const char* name; float ms[8]; float diffusion, mod; };
inline const std::array<RoomDef, 4>& rooms()
{
    static const std::array<RoomDef, 4> r {{
        { "Room",    { 11.3f, 13.7f, 16.1f, 19.3f, 22.9f, 26.3f, 30.7f, 35.9f }, 0.75f, 0.6f },
        { "Hall",    { 29.7f, 37.1f, 41.9f, 50.3f, 56.9f, 67.1f, 79.3f, 89.9f }, 0.70f, 1.0f },
        { "Plate",   { 7.9f, 9.7f, 12.1f, 14.3f, 17.9f, 21.1f, 25.3f, 29.9f }, 0.90f, 0.8f },
        { "Chamber", { 17.3f, 21.9f, 24.7f, 29.3f, 33.1f, 38.9f, 44.3f, 51.7f }, 0.55f, 0.5f },
    }};
    return r;
}

class Network
{
public:
    static constexpr int N = 8;
    static constexpr float kMaxMs = 12000.0f;      // 8 s de retardo sincronizado (2 compases a 60 BPM) por 1,5 del patrón
    static constexpr float kF1 = 250.0f, kF2 = 2500.0f;     // cortes de las tres bandas de absorción

    void prepare (float sampleRate, uint32_t seed = 1)
    {
        fs = sampleRate;
        for (auto& l : line) l.prepare ((int) (kMaxMs * 1e-3f * fs * 1.05f));
        diffL.prepare (fs); diffR.prepare (fs);
        seed0 = seed;
        b1 = std::exp (-2.0f * kPi * kF1 / fs); b2 = std::exp (-2.0f * kPi * kF2 / fs);
        bumpK = 1.0f - std::exp (-2.0f * kPi * 90.0f / fs);
        frzK = 1.0f - std::exp (-1.0f / (0.06f * fs));
        reset();
    }
    void reset()
    {
        for (auto& l : line) l.clear();
        diffL.clear(); diffR.clear();
        for (int i = 0; i < N; ++i)
        {
            lp1[i] = lp2[i] = 0.0f; dc[i].reset(); bbd[i].reset(); bbd2[i].reset(); bump1[i] = bump2[i] = 0.0f;
            curLen[i] = -1.0f; xf[i] = 1.0f; oldLen[i] = -1.0f; energy[i] = 0.0f; hiEnergy[i] = 0.0f; drop[i] = 0.0f; dropT[i] = 0;
            lfoPh[i] = (float) i / N;
            for (int k = 0; k < kSpringAp; ++k) spX[i][k][0] = spX[i][k][1] = spY[i][k][0] = spY[i][k][1] = 0.0f;
        }
        inLp1[0] = inLp1[1] = inLp2[0] = inLp2[1] = 0.0f;
        for (int b = 0; b < 3; ++b) frz[b] = 0.0f;
        wowPh = 0.0f; flutPh = 0.0f; drift = 0.0f; limGr = 1.0f; rng = Rand (seed0);
    }

    void set (const NetParams& p)
    {
        P = p;
        const float a = clampf (p.space, 0.0f, 1.0f);
        const float w = a * a * (3.0f - 2.0f * a);
        mat.setAngle (0.25f * kPi * w);
        spaceW = w;
        const auto& rd = rooms()[(size_t) std::clamp ((int) p.room, 0, 3)];
        const auto& pat = patterns()[(size_t) std::clamp (p.pattern, 0, (int) patterns().size() - 1)];
        for (int i = 0; i < N; ++i)
        {
            const int pair = i / 2;
            const float tDelay = std::max (1.0f, p.timeMs * pat.mult[pair]);
            float tRoom = rd.ms[i] * p.size;
            // Cola afinada: el largo de sala más cercano que sea múltiplo del período de la nota de esta línea.
            if (p.nTune > 0 && p.tuneAmount > 0.0f)
            {
                const float period = 1000.0f / std::max (20.0f, p.tuneHz[i % p.nTune]);
                const float k = std::max (1.0f, std::round (tRoom / period));
                tRoom += (k * period - tRoom) * clampf (p.tuneAmount, 0.0f, 1.0f);
            }
            target[i] = std::exp ((1.0f - w) * std::log (tDelay) + w * std::log (tRoom)) * 1e-3f * fs;
            target[i] = std::min (target[i], (float) line[i].capacity() - 64.0f);
            // Ganancias por banda (Jot): las tres a partir del largo de esta línea.
            const float sec = target[i] / fs;
            auto g = [&] (float t60) { return p.decayS >= 100.0f ? 1.0f : std::pow (10.0f, -3.0f * sec / std::max (0.03f, t60)); };
            gLo[i] = g (p.decayS * clampf (p.lfRatio, 0.2f, 3.0f));
            gMid[i] = g (p.decayS);
            gHi[i] = g (p.decayS * clampf (p.hfRatio, 0.02f, 1.0f));
            const float bDelay = pat.gain[pair], bSpace = 0.5f * ((i / 2) % 2 ? -1.0f : 1.0f);
            inGain[i] = (1.0f - w) * bDelay + w * bSpace;
            if (p.character == Character::bbd)
            {
                // El reloj del BBD baja con el tiempo: más largo, más oscuro (Raffel y Smith 2010: corte ≈ reloj/2).
                const float ms = sec * 1e3f;
                const float hz = clampf (6000.0f * std::pow (100.0f / std::max (20.0f, ms), 0.56f), 1500.0f, 9000.0f);
                bbd[i].setHz (hz, fs); bbd2[i].setHz (hz, fs);
            }
            dc[i].setHz (std::max (10.0f, p.lowCutHz), fs);
        }
        diffAmt = 0.62f * p.diffusion * rd.diffusion / 0.7f;
        roomMod = rd.mod;
        springA = 0.55f + 0.1f * clampf (p.size, 0.25f, 2.0f) / 2.0f;
        glideK = 1.0f - std::exp (-1.0f / (0.12f * fs));
        xfStep = 1.0f / (0.04f * fs);
    }

    // Una muestra estéreo de entrada → salida húmeda estéreo.
    inline void process (float inL, float inR, float& outL, float& outR)
    {
        // Congelado por banda: la ganancia de la banda va a 1 y su entrada se corta, en ~60 ms.
        for (int b = 0; b < 3; ++b) frz[b] += ((P.freezeMask & (1 << b) ? 1.0f : 0.0f) - frz[b]) * frzK;
        // Difusión de entrada (crece con space) y las inyecciones del motor (shimmer, granos).
        const float g = diffAmt * spaceW;
        float dl = inL, dr = inR;
        if (g > 1e-4f) { dl = diffL.process (inL, g); dr = diffR.process (inR, g); }
        dl = inL + (dl - inL) * spaceW + injectL; dr = inR + (dr - inR) * spaceW + injectR;
        // La entrada se reparte en las tres bandas para poder cortársela a las congeladas.
        float in[2] = { dl, dr };
        if (frz[0] + frz[1] + frz[2] > 1e-4f)
            for (int c = 0; c < 2; ++c)
            {
                inLp1[c] = in[c] + b1 * (inLp1[c] - in[c]); inLp2[c] = in[c] + b2 * (inLp2[c] - in[c]);
                const float lo = inLp1[c], mid = inLp2[c] - inLp1[c], hi = in[c] - inLp2[c];
                in[c] = lo * (1.0f - frz[0]) + mid * (1.0f - frz[1]) + hi * (1.0f - frz[2]);
            }

        // Cinta: wow lento (0.5 Hz con deriva al azar) y flutter (≈ 7 Hz), cada uno con su fase.
        float tapeMod = 0.0f;
        if (P.character == Character::tape)
        {
            wowPh += (0.45f + 0.15f * drift) / fs; wowPh -= std::floor (wowPh);
            flutPh += 6.8f / fs; flutPh -= std::floor (flutPh);
            drift += (rng.bi() - drift) * (1.0f / (2.0f * fs));
            const float wowDepth = (0.0008f + 0.003f * P.wear) * (0.3f + P.modDepth);      // fracción del largo
            const float flutDepth = (0.00012f + 0.0006f * P.wear) * (0.3f + P.modDepth);
            tapeMod = wowDepth * std::sin (2.0f * kPi * wowPh) + flutDepth * std::sin (2.0f * kPi * flutPh);
        }

        std::array<float, N> y {};
        const float depthSamples = P.modDepth * roomMod * 0.0012f * fs * (0.3f + 0.7f * spaceW) + P.modDepth * 0.0006f * fs * (1.0f - spaceW);
        for (int i = 0; i < N; ++i)
        {
            if (curLen[i] < 0.0f) curLen[i] = target[i];
            if (P.elastic >= 0.5f) curLen[i] += (target[i] - curLen[i]) * glideK;
            else if (std::fabs (target[i] - curLen[i]) > 1.0f && xf[i] >= 1.0f) { oldLen[i] = curLen[i]; curLen[i] = target[i]; xf[i] = 0.0f; }
            lfoPh[i] += P.modRate * (0.8f + 0.06f * (float) i) / fs; lfoPh[i] -= std::floor (lfoPh[i]);
            const float mod = depthSamples * std::sin (2.0f * kPi * lfoPh[i]) + tapeMod * curLen[i];
            float v = line[i].read (curLen[i] + mod + 2.0f);
            if (xf[i] < 1.0f) { v = v * xf[i] + line[i].read (oldLen[i] + mod + 2.0f) * (1.0f - xf[i]); xf[i] = std::min (1.0f, xf[i] + xfStep); }
            y[(size_t) i] = v;
        }
        // Salida: pares → L, impares → R; con signos alternados en modo reverb para descorrelacionar.
        float oL = 0.0f, oR = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            const float s = (i / 2) % 2 ? -1.0f : 1.0f;
            const float sw = (1.0f - spaceW) + spaceW * s;
            if (i & 1) oR += y[(size_t) i] * sw; else oL += y[(size_t) i] * sw;
        }
        const float outScale = (1.0f - spaceW) + 0.75f * spaceW;          // en delay suena un par: el eco sale con ganancia 1
        oL *= outScale; oR *= outScale;
        const float m = 0.5f * (oL + oR), sd = 0.5f * (oL - oR) * P.width;
        outL = m + sd; outR = m - sd;

        // Realimentación: carácter, absorción por bandas, matriz, ping-pong, entrada, limitador.
        float energySum = 0.0f;
        for (int i = 0; i < N; ++i)
        {
            float v = y[(size_t) i];
            if (P.character == Character::tape)
            {
                // Saturación suave con un poco de asimetría y head bump (realce de ~+2 dB cerca de 90 Hz: un pasa
                // banda de dos polos sumado).
                v = (std::tanh (1.2f * v + 0.05f) - std::tanh (0.05f)) / 1.2f;
                bump1[i] += (v - bump1[i]) * bumpK; bump2[i] += (bump1[i] - bump2[i]) * bumpK;
                v += 0.5f * (bump1[i] - bump2[i]);
                // Dropouts: unos por segundo según el desgaste; oscurecen y bajan apenas (no cortan).
                if (P.wear > 0.0f && dropT[i] == 0 && rng.uni() < 0.6f * P.wear * P.wear / fs) dropT[i] = (int) (fs * (0.03f + 0.08f * rng.uni()));
                const float dTarget = dropT[i] > 0 ? 1.0f : 0.0f; if (dropT[i] > 0) --dropT[i];
                drop[i] += (dTarget - drop[i]) * 0.002f;
                hiEnergy[i] += (v - hiEnergy[i]) * (1.0f - 0.85f * drop[i]);            // un pasa bajos que se cierra
                v = hiEnergy[i] * (1.0f - 0.3f * drop[i]);
                v += P.wear * 2e-4f * rng.bi();
            }
            else if (P.character == Character::bbd)
            {
                v = bbd2[i].lp (bbd[i].lp (v));
                v = std::tanh (1.6f * v) / 1.6f;
                v += 6e-5f * rng.bi();
            }
            else if (P.character == Character::spring)
            {
                for (int k = 0; k < kSpringAp; ++k)
                {
                    float* xs = spX[i][k]; float* ys = spY[i][k];
                    const float o = -springA * v + xs[1] + springA * ys[1];
                    xs[1] = xs[0]; xs[0] = v; ys[1] = ys[0]; ys[0] = o;
                    v = o;
                }
            }
            // Pasa altos del lazo (continua y retumbe); si los graves están congelados, se desactiva en proporción
            // (si no, cada vuelta les sacaría energía y el congelado de graves se apagaría).
            { const float hp = dc[i].process (v); v = hp + (v - hp) * frz[0]; }
            // Tres bandas complementarias (suman la señal exacta) con su ganancia de caída cada una.
            lp1[i] = v + b1 * (lp1[i] - v); lp2[i] = v + b2 * (lp2[i] - v);
            const float lo = lp1[i], mid = lp2[i] - lp1[i], hi = v - lp2[i];
            const float gl = gLo[i] + (1.0f - gLo[i]) * frz[0], gm = gMid[i] + (1.0f - gMid[i]) * frz[1], gh = gHi[i] + (1.0f - gHi[i]) * frz[2];
            v = lo * gl + mid * gm + hi * gh;
            y[(size_t) i] = v;
            energy[i] += (v * v - energy[i]) * 0.001f;
            energySum += v * v;
        }
        if (P.pingPong) for (int i = 0; i < N; i += 2) std::swap (y[(size_t) i], y[(size_t) i + 1]);
        mat.apply (y);
        // Limitador de energía sobre todas las líneas (conserva el color) y recorte suave por línea como último recurso.
        const float rms = std::sqrt (energySum / N);
        const float target = rms > 0.5f ? 0.5f / rms : 1.0f;
        limGr = target < limGr ? target : limGr + (1.0f - limGr) * 0.0005f;
        for (int i = 0; i < N; ++i)
        {
            float v = y[(size_t) i] * limGr + inGain[i] * ((i & 1) ? in[1] : in[0]);
            const float a = std::fabs (v);
            if (a > 0.9f) { const float lim = 0.9f + 0.5f * std::tanh ((a - 0.9f) / 0.5f); v = v > 0 ? lim : -lim; }
            line[i].push (v);
        }
    }

    float lineMs (int i) const { return (curLen[i] < 0 ? target[i] : curLen[i]) / fs * 1e3f; }
    float lineEnergy (int i) const { return energy[i]; }
    float lineGain (int i) const { return gMid[i]; }
    float loopGainMax() const { float g = 0.0f; for (int i = 0; i < N; ++i) g = std::max (g, std::max (gLo[i], std::max (gMid[i], gHi[i]))); return std::max (g, std::max (frz[0], std::max (frz[1], frz[2]))); }
    float limiterGrDb() const { return -gainToDb (limGr); }
    float freezeAmount() const { return std::max (frz[0], std::max (frz[1], frz[2])); }
    float injectL = 0.0f, injectR = 0.0f;

private:
    static constexpr int kSpringAp = 24;
    float fs = 48000.0f;
    NetParams P;
    std::array<DelayLine, N> line;
    Diffuser diffL, diffR;
    ButterflyMatrix<N> mat;
    std::array<OnePole, N> bbd, bbd2;
    std::array<DcBlock, N> dc;
    float target[N] {}, curLen[N] {}, oldLen[N] {}, xf[N] {}, gLo[N] {}, gMid[N] {}, gHi[N] {}, inGain[N] {}, lfoPh[N] {};
    float lp1[N] {}, lp2[N] {}, bump1[N] {}, bump2[N] {}, energy[N] {}, hiEnergy[N] {}, drop[N] {};
    int dropT[N] {};
    float spX[N][kSpringAp][2] {}, spY[N][kSpringAp][2] {};
    float inLp1[2] {}, inLp2[2] {}, frz[3] {};
    float b1 = 0.97f, b2 = 0.72f, spaceW = 0.0f, diffAmt = 0.6f, roomMod = 1.0f, springA = 0.6f, limGr = 1.0f;
    float bumpK = 0.01f, glideK = 0.001f, xfStep = 0.001f, frzK = 0.001f, wowPh = 0.0f, flutPh = 0.0f, drift = 0.0f;
    Rand rng; uint32_t seed0 = 1;
};

} // namespace dh::sp
