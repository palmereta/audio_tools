#pragma once
// El resto de las máquinas y los efectos de baja fidelidad:
//   - NR: un compansor genérico de estante deslizante (docs/research.md §2.11). Codifica antes de grabar (sube los
//     agudos bajos) y decodifica después (los baja). Si el decodificador no ve el mismo nivel (desgaste, pérdidas,
//     el ajuste Mistrack), los agudos respiran: el "sonido de casete" que la gente recuerda.
//   - Radio AM (§2.14): compresor de emisora, modulación de amplitud sobre la envolvente (con sobremodulación),
//     desvanecimiento selectivo en dos bandas, detector a diodo con su constante RC (recorte diagonal), estática,
//     silbido de heterodino, filtro de FI y parlante chico.
//   - Teléfono (§2.15): banda de 300-3400 Hz, la cápsula del tubo, muestreo a 8 kHz y ley μ de 8 bits exacta;
//     en el celular, pérdida de tramas de 20 ms en ráfagas (se repite la anterior, apagándose) y menos bits.
//   - Crush (§2.16): bits continuos con dither opcional, y retención de muestras a cualquier frecuencia con escalones
//     de banda limitada (PolyBLEP) en modo "clean" o crudo con su aliasing.
//   - Varispeed: la cinta más lenta o más rápida, con un desplazador de dos cabezas.

#include "Events.h"

namespace dh::tn {

class Compander
{
public:
    void prepare (double fs_) { fs = fs_; hp.highpass (1500.0, 0.707, fs); att = std::exp (-1.0 / (0.002 * fs)); rel = std::exp (-1.0 / (0.08 * fs)); reset(); }
    void reset() { hp.reset(); env = 0; sh.reset(); }
    // maxDb: 10 (suave) o 20 (fuerte); encode = true sube, false baja; offsetDb: lo que el decodificador ve de más.
    void set (double maxDb, bool encode, double offsetDb) { maxB = maxDb; enc = encode; off = dbToGain (offsetDb); }
    inline double process (double x)
    {
        if (maxB <= 0) return x;
        const double h = std::fabs (hp.process (x)) * off;
        env = h > env ? h + att * (env - h) : h + rel * (env - h);
        // Nivel de agudos alto → casi sin acción; bajo → hasta maxB. Umbral −40 dBFS, la acción se va entre −40 y −10.
        const double lv = clampd ((gainToDb (env + 1e-9) + 40.0) / 30.0, 0.0, 1.0);
        const double g = maxB * (1.0 - lv);
        // El estante se desliza hacia arriba cuando el agudo es fuerte (no sube lo que ya está alto).
        const double fc = 1500.0 * std::pow (2.0, 2.4 * lv);
        if (std::fabs (g - lastG) > 0.05 || std::fabs (fc - lastF) > 20.0) { sh.highshelf (fc, enc ? g : -g, fs, 0.8); lastG = g; lastF = fc; }
        return sh.process (x);
    }
private:
    Biquad hp, sh; double fs = 48000, env = 0, att = 0, rel = 0, maxB = 0, off = 1, lastG = -99, lastF = 0; bool enc = true;
};

// Ley μ (G.711, μ = 255) con `bits` de resolución en el dominio comprimido.
inline double muLaw (double x, double bits)
{
    const double mu = 255.0, a = clampd (x, -1.0, 1.0);
    const double y = (a < 0 ? -1.0 : 1.0) * std::log1p (mu * std::fabs (a)) / std::log1p (mu);
    const double q = std::pow (2.0, bits - 1.0);
    const double yq = std::round (y * q) / q;
    return (yq < 0 ? -1.0 : 1.0) * (std::pow (1.0 + mu, std::fabs (yq)) - 1.0) / mu;
}

class Radio
{
public:
    struct Params { int variant = 0; double compress = 0.6, fading = 0.2, staticAmt = 0.2, whistle = 0.0, tuning = 0.0, detector = 0.3; uint64_t seed = 7; };
    void prepare (double fs_) { fs = fs_; reset(); }
    void reset() { for (auto* b : { &pre1, &pre2, &if1, &if2, &spkHp, &spkLp, &spkPk, &split, &statBp }) b->reset(); env = 0; det = 1; whPh = 0; dc.reset(); imp.prepare (fs); }
    void set (const Params& p)
    {
        P = p;
        const double bw = p.variant == 0 ? 3500.0 : 7000.0;
        pre1.lowpass (bw * 1.25, 0.54, fs); pre2.lowpass (bw * 1.25, 1.31, fs);
        if1.lowpass (bw, 0.54, fs); if2.lowpass (bw, 1.31, fs);
        spkHp.highpass (150.0, 0.7, fs); spkLp.lowpass (p.variant == 0 ? 4000.0 : 7500.0, 0.7, fs); spkPk.peak (320.0, 1.2, 4.0, fs);
        split.lowpass (700.0, 0.707, fs); statBp.bandpass (1800.0, 0.6, fs);
        // El RC del detector: con 0 sigue a la portadora; con 1 se queda atrás en los descensos rápidos.
        detRel = std::exp (-1.0 / ((0.00005 + 0.0012 * p.detector) * fs));
        dc.setHz (8.0, fs);
        cAtt = std::exp (-1.0 / (0.003 * fs)); cRel = std::exp (-1.0 / (0.25 * fs));
    }
    inline double process (double x, int64_t pos, UiEvent* ev)
    {
        // Emisora: compresión pesada (relación 6:1 sobre −30 dBFS, con 0 a 1 de cuánto).
        const double a = std::fabs (x);
        env = a > env ? a + cAtt * (env - a) : a + cRel * (env - a);
        const double over = std::max (0.0, gainToDb (env + 1e-9) + 30.0);
        x *= dbToGain (-over * (1.0 - 1.0 / 6.0) * P.compress + 12.0 * P.compress);
        x = pre2.process (pre1.process (x));
        // Desvanecimiento selectivo: graves y agudos con su propio camino de onda de cielo.
        const double fL = 1.0 - P.fading * 0.5 * (1.0 + valueNoise (P.seed, (double) pos, 1.7 * fs, 31));
        const double fH = 1.0 - P.fading * 0.5 * (1.0 + valueNoise (P.seed, (double) pos, 0.9 * fs, 32));
        const double lo = split.process (x);
        const double am = 0.85 * (lo * fL + (x - lo) * fH);
        // Envolvente de la portadora y detector: la portadora no puede ir bajo cero (sobremodulación) y el
        // capacitor del detector no la sigue cuando cae rápido.
        const double e = std::max (0.0, 1.0 + am) * (0.35 + 0.65 * std::min (fL, fH));
        det = e > det ? e : e + detRel * (det - e);
        double y = dc.process (det);
        // Estática: ráfagas impulsivas filtradas.
        double sl = 0, sr = 0;
        const Impulses::Class cls[2] { { 0.4 + 18.0 * P.staticAmt * P.staticAmt, -30.0 + 14.0 * P.staticAmt, 2.0, 6.0, 300.0, 3000.0, 3 },
                                       { 200.0 * P.staticAmt, -48.0 + 10.0 * P.staticAmt, 1.0, 0.4, 2000.0, 6000.0, 0 } };
        if (P.staticAmt > 0) imp.process (pos, cls, 2, 1.0, P.seed ^ 0xA11, sl, sr, ev);
        y += statBp.process (sl) * 1.6 + sl * 0.3;
        // Silbido: dos emisoras cerca; con Tuning fuera de centro se acerca y desafina.
        const double whHz = 600.0 + 2400.0 * std::fabs (P.tuning) + 300.0 * valueNoise (P.seed, (double) pos, 2.3 * fs, 33);
        // Fase del silbido: la integral de su frecuencia, aproximada por la frecuencia media (sin estado).
        whPh = kTwoPi * (600.0 + 2400.0 * std::fabs (P.tuning)) * (double) pos / fs + 300.0 * kTwoPi * 2.3 * valueNoise (P.seed, (double) pos, 2.3 * fs, 34) / 6.0;
        (void) whHz;
        y += (P.whistle + 0.6 * std::fabs (P.tuning)) * 0.03 * std::sin (whPh);
        if (std::fabs (P.tuning) > 1e-6) y += 0.35 * std::fabs (P.tuning) * y * y * (P.tuning > 0 ? 1 : -1);    // banda lateral asimétrica
        y = if2.process (if1.process (y));
        y = spkPk.process (spkLp.process (spkHp.process (y)));
        return y;
    }
private:
    Params P; double fs = 48000, env = 0, det = 1, detRel = 0.99, whPh = 0, cAtt = 0, cRel = 0;
    Biquad pre1, pre2, if1, if2, spkHp, spkLp, spkPk, split, statBp; DcBlock dc; Impulses imp;
};

class Phone
{
public:
    struct Params { int variant = 0; double drive = 0.0, frameLoss = 0.0, codec = 0.0; uint64_t seed = 9; };
    void prepare (double fs_)
    {
        fs = fs_; frame = (int) std::round (0.020 * fs);
        hist.assign ((size_t) (2 * frame + 8), 0.0);
        reset();
    }
    void reset() { for (auto* b : { &h1, &h2, &l1, &l2, &pk, &l3, &l4 }) b->reset(); hold = 0; acc = 0; std::fill (hist.begin(), hist.end(), 0.0); hp = 0; lossGain = 1; }
    void set (const Params& p)
    {
        P = p;
        h1.highpass (300.0, 0.54, fs); h2.highpass (300.0, 1.31, fs);
        l1.lowpass (3400.0, 0.54, fs); l2.lowpass (3400.0, 1.31, fs);
        l3.lowpass (3600.0, 0.54, fs); l4.lowpass (3600.0, 1.31, fs);
        pk.peak (1500.0, 0.9, 6.0, fs);
    }
    inline double process (double x, int64_t pos)
    {
        x = pk.process (l2.process (l1.process (h2.process (h1.process (x)))));
        const double g = dbToGain (P.drive);
        x = std::tanh (x * g * 1.5) / 1.5;                       // la cápsula y la línea se saturan suave
        // Muestreo a 8 kHz con retención y ley μ.
        if (std::floor ((double) pos * 8000.0 / fs) != std::floor ((double) (pos - 1) * 8000.0 / fs)) hold = muLaw (x, P.variant == 1 ? 8.0 - 3.0 * P.codec : 8.0);
        double y = l4.process (l3.process (hold));
        // Pérdida de tramas (celular): ráfagas según un estado malo lento; la trama perdida repite la anterior apagándose.
        if (P.frameLoss > 0)
        {
            const int64_t fr = pos / frame;
            const bool bad = valueNoise (P.seed, (double) fr, 12.0, 41) > 0.55 - 0.9 * P.frameLoss;
            const bool lost = hashUni (P.seed, (uint64_t) fr, 42) < (bad ? 0.6 : 0.02) * std::min (1.0, 2.0 * P.frameLoss);
            const int k = (int) (pos % frame);
            if (k == 0) lossGain = lost ? lossGain * 0.6 : 1.0;
            const size_t w = (size_t) (hp % (int64_t) hist.size());
            if (lost) y = hist[(size_t) ((hp - frame + (int64_t) hist.size()) % (int64_t) hist.size())] * lossGain;
            hist[w] = y; ++hp;
        }
        return y;
    }
private:
    Params P; double fs = 48000, hold = 0, acc = 0, lossGain = 1; int frame = 960; int64_t hp = 0;
    std::vector<double> hist;
    Biquad h1, h2, l1, l2, pk, l3, l4;
};

// Bits y frecuencia de muestreo. `raw`: sin filtros (el aliasing es la idea); si no, pasa bajos a 0,45·R antes y
// después y escalones de banda limitada (PolyBLEP de 2 muestras) en los bordes de la retención, así una frecuencia
// que no divide a la del host no agrega el temblor de ±1 muestra en cada borde. La corrección necesita ver el
// borde que viene, por eso el crusher atrasa siempre una muestra (también apagado: la latencia no cambia).
class Crusher
{
public:
    void prepare (double fs_) { fs = fs_; reset(); }
    void reset() { for (auto& b : aa) b.reset(); for (auto& b : rc) b.reset(); ph = 0; held = 0; pending = 0; prevX = 0; }
    void set (double bits_, double rateHz, bool raw_, double jitter_, bool dither_)
    {
        bits = bits_; rate = clampd (rateHz, 200.0, fs); raw = raw_; jitter = jitter_; dither = dither_;
        const double fc = 0.45 * rate;
        on = rate < 0.999 * fs; quant = bits < 23.5;
        aa[0].lowpass (fc, 0.54, fs); aa[1].lowpass (fc, 1.31, fs); rc[0].lowpass (fc, 0.54, fs); rc[1].lowpass (fc, 1.31, fs);
    }
    inline double process (double x, int64_t pos, int ch)
    {
        if (quant)
        {
            const double step = 2.0 / std::pow (2.0, bits);
            const double d = dither ? (hashUni (77 + (uint64_t) ch, (uint64_t) pos, 1) - hashUni (77 + (uint64_t) ch, (uint64_t) pos, 2)) * step : 0.0;
            x = std::round ((x + d) / step) * step;
        }
        if (! on) { const double o = prevX; prevX = x; return o; }
        const double xin = raw ? x : aa[1].process (aa[0].process (x));
        // La fase del reloj de retención sale de la posición (sin estado): con el host marcando la posición, la
        // retención cae siempre en las mismas muestras. El jitter corre la fase un poco en cada borde.
        const double inc = rate / fs;
        const double cyc = (double) pos * inc;
        auto cell = [] (double c) { return (uint64_t) (int64_t) std::floor (c); };     // vía int64: negativo antes del 0
        const double jit = jitter * 0.45 * hashBi (55 + (uint64_t) ch, cell (cyc));
        const double prevCyc = (double) (pos - 1) * inc;
        const double phn = cyc + jit - std::floor (cyc + jit);
        const bool edge = std::floor (cyc + jit) > std::floor (prevCyc + jitter * 0.45 * hashBi (55 + (uint64_t) ch, cell (prevCyc)));
        double out = pending, cur = held, corr = 0;
        if (edge)
        {
            const double J = xin - held, u = clampd (phn / std::max (inc, 1e-9), 0.0, 1.0);   // el borde cayó u muestras antes
            if (! raw) { out += 0.5 * J * u * u; corr = 0.5 * J * (2 * u - u * u - 1); }
            held = cur = xin;
        }
        pending = cur + corr;
        return raw ? out : rc[1].process (rc[0].process (out));
    }
private:
    double fs = 48000, bits = 24, rate = 48000, jitter = 0, ph = 0, held = 0, pending = 0, prevX = 0;
    bool raw = false, on = false, quant = false, dither = false;
    Biquad aa[2], rc[2];
};

// Desplazador de dos cabezas (ventanas de 60 ms cruzadas): la cinta más lenta o más rápida, sin cambiar el largo.
class Varispeed
{
public:
    void prepare (double fs_) { fs = fs_; win = 0.06 * fs; line.prepare ((int) (win * 2 + 16)); reset(); }
    void reset() { line.clear(); ph = 0; }
    void setSemis (double s) { ratio = std::pow (2.0, s / 12.0); active = std::fabs (s) > 1e-3; }
    inline double process (double x, int64_t pos)
    {
        line.push (x);
        if (! active) return x;
        ph = (double) pos * (1.0 - ratio) / win;                // fase de la posición: sin estado
        ph -= std::floor (ph);
        const double p2 = ph + 0.5 - std::floor (ph + 0.5);
        const double a = line.read (1.0 + ph * win), b = line.read (1.0 + p2 * win);
        const double wa = std::sin (kPi * ph), wb = std::sin (kPi * p2);
        return (a * wa * wa + b * wb * wb);
    }
private:
    DelayLine line; double fs = 48000, win = 2880, ph = 0, ratio = 1; bool active = false;
};

} // namespace dh::tn
