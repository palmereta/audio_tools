// pallortest: pruebas del motor de Pallor sin JUCE (docs/research.md §8). Cada prueba imprime lo que midió y [ok] o
// [FALLA]; al final, el total. Las señales se sintetizan acá: no hacen falta archivos.
//   pallortest              todas las pruebas
//   pallortest --calibrate  la tabla de ganancias de los modelos analógicos (Analog.h, modelGain)
#include "Engine.h"
#include <chrono>
#include <complex>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <set>

using namespace dh::pal;
static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what); if (! ok) ++fails; }
static double db (double x) { return 20.0 * std::log10 (std::max (1e-12, x)); }

// ── Utilidades ──
struct Render { std::vector<float> l, r; std::array<std::vector<float>, 2 * kBuses> bus; std::vector<MidiOut> midi; std::vector<int64_t> midiAbs; };

// Motor con parámetros por defecto y todas las voces apagadas salvo las que pida la prueba.
static EngineParams quietParams()
{
    EngineParams p;
    for (auto& v : p.v) v.levelDb = -60.0f;
    p.aReturnDb = -200.0f; p.bReturnDb = -200.0f;
    return p;
}

// Renderiza `secs` con el transporte del host corriendo desde ppq0, bloques de `block` (0 = al azar).
// hostJitter: la rendija de redondeo que deja un host entre bloques (en ppq).
static Render renderEngine (Engine& e, double secs, double bpm, int block, double fs = 48000.0, double ppq0 = 0.0, bool buses = false,
                            double hostJitter = 0.0, std::function<void (int64_t, Engine&)> hook = nullptr, const std::vector<MidiIn>* midiIn = nullptr)
{
    const int n = (int) (secs * fs);
    Render R; R.l.assign ((size_t) n, 0.0f); R.r.assign ((size_t) n, 0.0f);
    if (buses) for (auto& b : R.bus) b.assign ((size_t) n, 0.0f);
    uint32_t s = 12345;
    std::vector<MidiIn> blkIn;
    for (int pos = 0; pos < n;)
    {
        int b = block > 0 ? block : 1 + (int) ((s = s * 1664525u + 1013904223u) >> 22) % 700;
        b = std::min (b, n - pos);
        if (hook) hook (pos, e);
        std::array<float*, 2 * kBuses> outs {};
        outs[0] = R.l.data() + pos; outs[1] = R.r.data() + pos;
        if (buses) for (int k = 2; k < 2 * kBuses; ++k) outs[(size_t) k] = R.bus[(size_t) k].data() + pos;
        HostTime ht; ht.playing = true; ht.bpm = bpm;
        ht.ppq = ppq0 + (double) pos / fs * bpm / 60.0;
        if (hostJitter != 0.0 && pos > 0) ht.ppq += ((s >> 9) & 1 ? 1.0 : -1.0) * hostJitter;
        blkIn.clear();
        if (midiIn) for (const auto& m : *midiIn) if (m.offset >= pos && m.offset < pos + b) { auto x = m; x.offset -= pos; blkIn.push_back (x); }
        std::vector<MidiOut> mo;
        e.process (outs.data(), b, ht, blkIn.data(), (int) blkIn.size(), &mo);
        for (auto& m : mo) { R.midi.push_back (m); R.midiAbs.push_back (pos + m.offset); }
        pos += b;
    }
    return R;
}

static uint64_t fingerprint (const std::vector<float>& x)
{
    uint64_t h = 1469598103934665603ull;
    for (float v : x) { uint32_t u; std::memcpy (&u, &v, 4); h = (h ^ u) * 1099511628211ull; }
    return h;
}

// Amplitud de una frecuencia (DFT de un bin, ventana de Hann).
static double toneAmp (const std::vector<float>& x, double f, double fs, size_t a, size_t b)
{
    double re = 0, im = 0, w = 0;
    for (size_t i = a; i < b; ++i)
    {
        const double win = 0.5 - 0.5 * std::cos (2 * kPiD * (double) (i - a) / (double) (b - a));
        re += win * x[i] * std::cos (2 * kPiD * f * (double) i / fs); im += win * x[i] * std::sin (2 * kPiD * f * (double) i / fs); w += win;
    }
    return 2.0 * std::sqrt (re * re + im * im) / w;
}
// El pico espectral más fuerte entre lo y hi (paso `step` Hz).
static double peakFreq (const std::vector<float>& x, double lo, double hi, double step, double fs, size_t a, size_t b, double* amp = nullptr)
{
    double best = 0, bf = lo;
    for (double f = lo; f <= hi; f += step) { const double v = toneAmp (x, f, fs, a, b); if (v > best) { best = v; bf = f; } }
    if (amp) *amp = best;
    return bf;
}
// Frecuencia por cruces por cero (ascendentes, interpolados) en [a, b).
static double zcFreq (const std::vector<float>& x, double fs, size_t a, size_t b)
{
    std::vector<double> z;
    for (size_t i = a + 1; i < b; ++i) if (x[i - 1] < 0 && x[i] >= 0) z.push_back ((double) (i - 1) + x[i - 1] / (x[i - 1] - x[i]));
    if (z.size() < 2) return 0;
    return fs * (double) (z.size() - 1) / (z.back() - z.front());
}
// Frecuencia instantánea: el período entre los dos cruces ascendentes que rodean al instante t.
static double instFreq (const std::vector<float>& x, double fs, double t)
{
    const size_t c = (size_t) (t * fs);
    double before = -1, after = -1;
    for (size_t i = c; i > 1 && before < 0; --i) if (x[i - 1] < 0 && x[i] >= 0) before = (double) (i - 1) + x[i - 1] / (x[i - 1] - x[i]);
    for (size_t i = c + 1; i < x.size() && after < 0; ++i) if (x[i - 1] < 0 && x[i] >= 0) after = (double) (i - 1) + x[i - 1] / (x[i - 1] - x[i]);
    return before >= 0 && after > before ? fs / (after - before) : 0.0;
}
static double rms (const std::vector<float>& x, size_t a, size_t b) { double s = 0; b = std::min (b, x.size()); for (size_t i = a; i < b; ++i) s += (double) x[i] * x[i]; return std::sqrt (s / std::max<size_t> (1, b - a)); }

// Una voz analógica sola a 48 kHz.
static std::vector<float> voice (int slot, int model, Shape sh = {}, float vel = 1.0f, float acc = 0.0f, double secs = 1.5, uint32_t seed = 1)
{
    AnalogVoice v; v.prepare (48000.0f, slot); v.trigger (model, sh, vel, acc, seed);
    std::vector<float> o ((size_t) (secs * 48000));
    for (auto& x : o) x = v.process();
    return o;
}

static Pattern onePattern (std::initializer_list<std::pair<int, std::vector<int>>> hits, int length = 16, int vel = 100)
{
    Pattern p; p.length = (uint8_t) length;
    for (auto& [t, steps] : hits) for (int s : steps) p.t[(size_t) t].s[(size_t) s].vel = (uint8_t) vel;
    return p;
}

int main (int argc, char** argv)
{
    std::setvbuf (stdout, nullptr, _IONBF, 0);
    const double fs = 48000.0;

    if (argc > 1 && std::strcmp (argv[1], "--calibrate") == 0)
    {
        const int fams[12] = { kKick, kSnare, kClap, kRim, kClosedHat, kCrash, kRide, kTom2, kCowbell, kClave, kTamb, kFx };
        for (int f = 0; f < 12; ++f)
        {
            const int s = fams[f]; std::printf ("        { ");
            for (int m = 0; m < numAnalogModels (s); ++m)
            {
                float pk = 0;
                for (uint32_t seed = 1; seed <= 8; ++seed)
                {
                    AnalogVoice v; v.calibrated = false; v.prepare (48000.0f, s); v.trigger (m, Shape(), 1.0f, 0.0f, seed);
                    for (int n = 0; v.isActive() && n < 480000; ++n) pk = std::max (pk, std::fabs (v.process()));
                }
                std::printf ("%.3ff%s", kitgen::slotPeak (s) / pk, m + 1 < numAnalogModels (s) ? ", " : "");
            }
            std::printf (" },\n");
        }
        return 0;
    }

    const auto tLib0 = std::chrono::steady_clock::now();
    const auto& lib = kitgen::Library::get();
    const double libMs = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - tLib0).count();

    // Una muestra propia que es un impulso: con la cadena limpia, la salida es el impulso atrasado la latencia.
    SampleData impulse; impulse.rate = fs; impulse.chain = kChainClean; impulse.data.assign (64, 0.0f); impulse.data[0] = 1.0f;

    std::printf ("\nT1 Sincronía a la muestra: el golpe sale en su muestra con cualquier bloque, tempo y rendija del host\n");
    {
        bool ok = true; int checked = 0;
        for (double bpm : { 120.0, 125.0, 128.0, 133.33, 174.0 })
            for (int block : { 1, 64, 441, 512, 0 })
                for (double jit : { 0.0, 3e-10 })
                {
                    Engine e; e.prepare (fs, 4096);
                    EngineParams p = quietParams();
                    p.v[kRim].engine = 1; p.v[kRim].kit = kitgen::numKits(); p.v[kRim].chain = kChainClean; p.v[kRim].shape.decay = 1.0f; p.v[kRim].levelDb = 0.0f;
                    p.v[kRim].velSense = 0.0f;
                    e.set (p); e.setUserSample (kRim, &impulse);
                    e.bankRef().p[0] = onePattern ({ { kRim, { 0, 3, 5, 6, 10, 15 } } });
                    auto R = renderEngine (e, 8.0, bpm, block, fs, 0.0, false, jit);
                    // Lo esperado: cada paso en round(ppq / ppqPorMuestra) + latencia.
                    const double spb = fs * 60.0 / bpm;
                    std::vector<int64_t> want;
                    for (int bar = 0; bar < 8; ++bar) for (int st : { 0, 3, 5, 6, 10, 15 })
                    {
                        const double at = (bar * 16 + st) * 0.25 * spb;
                        const int64_t smp = (int64_t) std::floor (at + 0.5) + kVoiceLatency;
                        if (smp < (int64_t) R.l.size() - 8) want.push_back (smp);
                    }
                    std::vector<int64_t> got;
                    for (size_t i = 1; i + 1 < R.l.size(); ++i) if (R.l[i] > 0.3f && R.l[i] >= R.l[i - 1] && R.l[i] > R.l[i + 1]) got.push_back ((int64_t) i);
                    if (got != want) { ok = false; std::printf ("  %.2f BPM, bloque %d, rendija %g: %zu golpes (esperados %zu)%s\n", bpm, block, jit, got.size(), want.size(), got.size() == want.size() ? ", en otra muestra" : ""); }
                    ++checked;
                }
        std::printf ("  %d combinaciones de tempo, bloque y rendija; latencia de las voces %d muestras\n", checked, kVoiceLatency);
        check (ok, "cada golpe en su muestra exacta (+ latencia), ninguno perdido ni duplicado");
    }

    std::printf ("\nT2 Determinismo: huella del render completo con bloques de 1, 64, 512, 4096 y al azar\n");
    {
        auto run = [&] (int block) {
            Engine e; e.prepare (fs, 4096);
            EngineParams p;
            p.g.seq.swing = 0.58f; p.a.type = kGated; p.aReturnDb = -3.0f; p.b.type = kTape; p.bReturnDb = -6.0f;
            p.m.tape = true; p.m.comp = 0.5f; p.m.exciter = 0.3f; p.m.lofi = kLofi26; p.m.lofiMix = 0.5f;
            p.v[kSnare].sendA = 0.6f; p.v[kClosedHat].sendB = 0.3f; p.v[kCrash].engine = 1; p.v[kCrash].kit = 4; p.v[kTom2].engine = 1; p.v[kTom2].kit = 0;
            p.v[kClap].drive = 0.6f; p.v[kClap].bits = 8; p.v[kClap].rateHz = 11025; p.v[kRim].filter = 1; p.v[kRim].cutoff = 1200; p.v[kRim].reso = 0.7f;
            e.set (p);
            Pattern pt = onePattern ({ { kKick, { 0, 4, 8, 12, 14 } }, { kSnare, { 4, 12 } }, { kClosedHat, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } },
                                       { kClap, { 12 } }, { kCrash, { 0 } }, { kTom2, { 13, 15 } }, { kRim, { 7 } } });
            pt.t[kClosedHat].s[3].ratchet = 3; pt.t[kClosedHat].s[9].prob = 50; pt.t[kSnare].s[12].flam = 20; pt.t[kTom2].s[15].nudge = -30;
            pt.t[kClosedHat].len = 12;
            e.bankRef().p[0] = pt;
            return renderEngine (e, 6.0, 126.0, block, fs);
        };
        const auto a = run (1), b = run (64), c = run (512), d = run (4096), r = run (0);
        const uint64_t fa = fingerprint (a.l) ^ (fingerprint (a.r) << 1);
        std::printf ("  huella %016llx (6 s, 48 kHz, todos los efectos)\n", (unsigned long long) fa);
        const bool same = a.l == b.l && a.l == c.l && a.l == d.l && a.l == r.l && a.r == b.r && a.r == c.r && a.r == d.r && a.r == r.r;
        check (same, "idéntico bit a bit con cualquier tamaño de bloque");
        float pk = 0; for (float x : a.l) pk = std::max (pk, std::fabs (x));
        check (pk > 0.05f && pk <= 1.0f, "suena y respeta el techo");
    }

    std::printf ("\nT3 Swing: la segunda semicorchea de cada par cae al swing del par\n");
    {
        bool ok = true;
        for (float s : { 0.50f, 0.54f, 0.58f, 0.62f, 2.0f / 3.0f, 0.71f, 0.75f })
        {
            const double second = swingWarp (0.25, s, 1), firstNext = swingWarp (0.5, s, 1), eighth = swingWarp (0.5, s, 0), q = swingWarp (0.25, s, 0);
            std::printf ("  %4.1f %%: 2.ª semicorchea en %.4f negras (%.1f %% del par); corcheas: 2.ª en %.4f, 1/16 intermedia en %.4f\n", s * 100, second, second / 0.5 * 100, eighth, q);
            ok &= std::fabs (second - s * 0.5) < 1e-9 && std::fabs (firstNext - 0.5) < 1e-9 && std::fabs (eighth - s) < 1e-9;
        }
        check (ok, "16avos: 2.ª nota en s·½ negra; 8vos: 2.ª corchea en s negras; los tiempos fuertes no se mueven");
        // En el motor: charles en semicorcheas con 66,7 %: los intervalos alternan 2:1.
        Engine e; e.prepare (fs, 4096);
        EngineParams p = quietParams(); p.g.seq.swing = 2.0f / 3.0f;
        p.v[kRim].engine = 1; p.v[kRim].kit = kitgen::numKits(); p.v[kRim].chain = kChainClean; p.v[kRim].shape.decay = 1.0f; p.v[kRim].levelDb = 0; p.v[kRim].velSense = 0;
        e.set (p); e.setUserSample (kRim, &impulse);
        e.bankRef().p[0] = onePattern ({ { kRim, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } } });
        auto R = renderEngine (e, 2.0, 120.0, 512, fs);
        std::vector<int> on; for (size_t i = 1; i + 1 < R.l.size(); ++i) if (R.l[i] > 0.3f && R.l[i] >= R.l[i - 1] && R.l[i] > R.l[i + 1]) on.push_back ((int) i);
        const int d1 = on.size() > 2 ? on[1] - on[0] : 0, d2 = on.size() > 2 ? on[2] - on[1] : 0;
        std::printf ("  en el motor (120 BPM, 66,7 %%): intervalos %d y %d muestras\n", d1, d2);
        check (d1 == 8000 && d2 == 4000, "tresillo exacto: 8000 y 4000 muestras");
    }

    std::printf ("\nT4 Choke: el charles cerrado corta al abierto en 4 ms; juntos suenan los dos\n");
    {
        auto run = [&] (bool closeAfter, bool together) {
            Engine e; e.prepare (fs, 4096);
            EngineParams p = quietParams();
            p.v[kOpenHat].levelDb = 0; p.v[kOpenHat].shape.decay = 0.9f; p.v[kClosedHat].levelDb = closeAfter ? -60.0f : 0.0f;
            p.v[kOpenHat].choke = p.v[kClosedHat].choke = 1;
            e.set (p);
            if (together) e.bankRef().p[0] = onePattern ({ { kOpenHat, { 0 } }, { kClosedHat, { 0 } } }, 64);
            else e.bankRef().p[0] = onePattern ({ { kOpenHat, { 0 } }, { kClosedHat, { 2 } } }, 64);
            return renderEngine (e, 1.0, 120.0, 256, fs);
        };
        const auto open = run (true, false);        // el cerrado sin nivel: sólo se oye el corte del abierto
        const size_t at = (size_t) (2 * 0.25 * 0.5 * fs) + kVoiceLatency;   // paso 2 a 120 BPM
        const double before = rms (open.l, at - 2400, at), after = rms (open.l, at + 240, at + 2640);
        std::printf ("  abierto antes del corte %.1f dB, de 5 a 55 ms después %.1f dB\n", db (before), db (after));
        check (after < before * 0.01, "el abierto cae más de 40 dB tras el cerrado");
        const auto both = run (false, true);
        check (rms (both.l, (size_t) (0.25 * fs), (size_t) (0.4 * fs)) > 1e-3, "golpeados juntos, el abierto sigue sonando");
    }

    std::printf ("\nT5 Ratchet y flam\n");
    {
        Engine e; e.prepare (fs, 4096);
        EngineParams p = quietParams();
        p.v[kRim].engine = 1; p.v[kRim].kit = kitgen::numKits(); p.v[kRim].chain = kChainClean; p.v[kRim].shape.decay = 1.0f; p.v[kRim].levelDb = 0; p.v[kRim].velSense = 1.0f;
        e.set (p); e.setUserSample (kRim, &impulse);
        Pattern pt = onePattern ({ { kRim, { 4, 8 } } });
        pt.t[kRim].s[4].ratchet = 4; pt.t[kRim].s[8].flam = 20;
        e.bankRef().p[0] = pt;
        auto R = renderEngine (e, 2.0, 120.0, 333, fs);
        std::vector<std::pair<int, float>> on; for (size_t i = 1; i + 1 < R.l.size(); ++i) if (R.l[i] > 0.05f && R.l[i] >= R.l[i - 1] && R.l[i] > R.l[i + 1]) on.push_back ({ (int) i, R.l[i] });
        for (auto& o : on) if (o.first < 30000) std::printf ("  golpe en %d (%.3f s), nivel %.2f\n", o.first, (o.first - kVoiceLatency) / fs, o.second);
        const int st4 = (int) (4 * 6000) + kVoiceLatency, st8 = (int) (8 * 6000) + kVoiceLatency;
        bool ok = on.size() >= 6 && on[0].first == st4 && on[1].first == st4 + 1500 && on[2].first == st4 + 3000 && on[3].first == st4 + 4500;
        check (ok, "ratchet ×4: cuatro golpes cada 1/4 del paso");
        const int flamAt = st8 - (int) std::lround (0.020 * fs);
        ok = on.size() >= 6 && on[4].first == flamAt && on[5].first == st8 && on[4].second < on[5].second * 0.7f;
        check (ok, "flam de 20 ms: la nota de gracia 960 muestras antes y más suave");
    }

    std::printf ("\nT6 Probabilidad: determinista por posición, cerca del porcentaje\n");
    {
        auto count = [&] (int block) {
            Sequencer sq; Bank bk; bk.p[0] = onePattern ({ { 0, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } } });
            for (auto& s : bk.p[0].t[0].s) s.prob = 50;
            sq.setBank (&bk); SeqSettings st;
            std::vector<double> hits;
            const double rate = 120.0 / 60.0 / fs;
            for (int pos = 0; pos < (int) (60 * fs); pos += block) sq.advance (pos * rate, block, rate, 120.0, st, [&] (double t, int, float, bool) { hits.push_back (t); });
            return hits;
        };
        const auto a = count (512), b = count (77);
        const double frac = (double) a.size() / (120.0 * 4.0 * 1.0 / 1.0 * 1.0);   // 60 s a 120 BPM = 480 semicorcheas
        std::printf ("  50 %%: %zu de 480 pasos (%.1f %%)\n", a.size(), frac * 100.0);
        check (a == b, "el mismo resultado con otro tamaño de bloque");
        check (frac > 0.42 && frac < 0.58, "dentro de 50 ± 8 %");
    }

    std::printf ("\nT7 Condiciones, fills y fill automático\n");
    {
        Bank bk;
        bk.p[0] = onePattern ({ { 0, { 0 } }, { 1, { 4 } }, { 2, { 8 } }, { 3, { 12 } } });
        bk.p[0].t[1].s[4].cond = c1of2; bk.p[0].t[2].s[8].cond = cFill; bk.p[0].t[3].s[12].cond = cNotFill;
        bk.p[2] = onePattern ({ { 5, { 0, 2, 4, 6, 8, 10, 12, 14 } } });           // el fill del grupo 1
        auto run = [&] (int autofill, bool fillParam, double bars) {
            Sequencer sq; sq.setBank (&bk); SeqSettings st; st.autofill = autofill; st.fill = fillParam;
            std::map<int, std::vector<int>> perBar;
            const double rate = 120.0 / 60.0 / fs;
            for (int pos = 0; pos < (int) (bars * 2.0 * fs); pos += 512) sq.advance (pos * rate, 512, rate, 120.0, st, [&] (double t, int tr, float, bool) { perBar[(int) (t / 4.0)].push_back (tr); });
            return perBar;
        };
        auto pb = run (4, false, 8);
        auto has = [] (const std::vector<int>& v, int t) { return std::find (v.begin(), v.end(), t) != v.end(); };
        bool ok12 = true, okFill = true;
        for (int bar = 0; bar < 8; ++bar)
        {
            const bool fillBar = bar % 4 == 3;
            if (! fillBar && has (pb[bar], 1) != (bar % 2 == 0)) ok12 = false;
            if (fillBar != has (pb[bar], 5)) okFill = false;
            if (fillBar && has (pb[bar], 0)) okFill = false;
        }
        std::printf ("  compases 1-8 con fill cada 4:");
        for (int bar = 0; bar < 8; ++bar) std::printf (" %s", has (pb[bar], 5) ? "FILL" : (has (pb[bar], 1) ? "A+" : "A"));
        std::printf ("\n");
        check (ok12, "1:2 suena una vuelta sí y otra no");
        check (okFill, "el fill reemplaza el compás 4 y 8");
        // Sin patrón de fill: el compás de fill toca A con las condiciones de fill.
        Bank keep = bk; bk.p[2] = Pattern();
        auto pc = run (2, false, 4);
        check (has (pc[1], 2) && ! has (pc[1], 3) && has (pc[0], 3) && ! has (pc[0], 2), "sin patrón de fill: los pasos Fill / Not fill se turnan");
        bk = keep;
    }

    std::printf ("\nT8 Polimetría: una pista de 12 pasos sobre un patrón de 16 sigue su cuenta entre compases\n");
    {
        Bank bk; bk.p[0] = onePattern ({ { 0, { 0 } } }); bk.p[0].t[0].len = 12; bk.p[0].t[0].s[0].vel = 100;
        Sequencer sq; sq.setBank (&bk); SeqSettings st;
        std::vector<double> hits; const double rate = 120.0 / 60.0 / fs;
        for (int pos = 0; pos < (int) (6.0 * fs); pos += 512) sq.advance (pos * rate, 512, rate, 120.0, st, [&] (double t, int, float, bool) { hits.push_back (t); });
        std::printf ("  golpes en:"); for (double h : hits) std::printf (" %.2f", h); std::printf (" negras\n");
        bool ok = hits.size() >= 3; for (size_t i = 0; i < hits.size(); ++i) ok &= std::fabs (hits[i] - 3.0 * (double) i) < 1e-9;
        check (ok, "cada 12 semicorcheas (3 negras), cruzando las barras de 16");
    }

    std::printf ("\nT9 Canción: la cadena encadena y un salto cae en el mismo lugar que tocando de corrido\n");
    {
        Bank bk;
        for (int g = 0; g < 2; ++g) for (int vv = 0; vv < 2; ++vv) bk.p[(size_t) patIndex (g, vv)] = onePattern ({ { g * 2 + vv, { 0 } } });
        bk.p[(size_t) patIndex (1, 2)] = onePattern ({ { 9, { 0, 8 } } });
        bk.chain[0] = { 0, 0, 2, false }; bk.chain[1] = { 0, 1, 1, false }; bk.chain[2] = { 1, 0, 2, true }; bk.chainLen = 3;
        SeqSettings st; st.mode = 3;
        std::vector<int> seqCont;
        { Sequencer sq; sq.setBank (&bk); const double rate = 120.0 / 60.0 / fs; for (int pos = 0; pos < (int) (20.0 * fs); pos += 512) sq.advance (pos * rate, 512, rate, 120.0, st, [&] (double t, int tr, float, bool) { if (std::fmod (t, 4.0) < 1e-9) seqCont.push_back (tr); }); }
        std::printf ("  compases:"); for (int x : seqCont) std::printf (" %d", x); std::printf ("\n");
        const std::vector<int> want { 0, 0, 1, 2, 9, 0, 0, 1, 2, 9 };
        check (seqCont.size() >= want.size() && std::equal (want.begin(), want.end(), seqCont.begin()), "1A ×2, 1B, 2A ×2 con fill al final, y vuelve a empezar");
        // Salto al compás 4 (ppq 12): suena lo mismo que tocando de corrido.
        std::vector<int> jumped;
        { Sequencer sq; sq.setBank (&bk); const double rate = 120.0 / 60.0 / fs; for (int pos = 0; pos < (int) (4.0 * fs); pos += 512) sq.advance (12.0 + pos * rate, 512, rate, 120.0, st, [&] (double t, int tr, float, bool) { if (std::fmod (t, 4.0) < 1e-9) jumped.push_back (tr); }); }
        check (jumped.size() >= 2 && jumped[0] == 2 && jumped[1] == 9, "un salto del host al compás 4 toca 2A y después su fill");
    }

    std::printf ("\nT10 Salto del host y stop: el MIDI de salida queda balanceado\n");
    {
        Engine e; e.prepare (fs, 4096);
        EngineParams p = quietParams(); e.set (p);
        Pattern pt = onePattern ({ { kKick, { 0, 4, 8, 12 } }, { kClosedHat, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15 } }, { kSnare, { 15 } } });
        pt.t[kClosedHat].s[7].ratchet = 4;
        e.bankRef().p[0] = pt;
        std::vector<MidiOut> all; std::map<int, int> open; int bad = 0, ons = 0;
        const double rate = 120.0 / 60.0 / fs;
        double ppq = 0; bool playing = true;
        for (int pos = 0; pos < (int) (12 * fs); pos += 480)
        {
            if (pos == (int) (3 * fs)) ppq = 1.0;                 // salto hacia atrás
            if (pos == (int) (5 * fs)) ppq = 33.37;               // salto hacia adelante a mitad de paso
            if (pos == (int) (8 * fs)) playing = false;           // stop
            if (pos == (int) (9 * fs)) { playing = true; ppq = 0; }
            std::array<float, 480 * 2> buf {}; std::array<float*, 2 * kBuses> outs {}; outs[0] = buf.data(); outs[1] = buf.data() + 480;
            HostTime ht; ht.playing = playing; ht.ppq = ppq; ht.bpm = 120;
            std::vector<MidiOut> mo; e.process (outs.data(), 480, ht, nullptr, 0, &mo);
            for (auto& m : mo)
            {
                if (m.offset < 0 || m.offset >= 480) ++bad;
                if (m.vel > 0) { if (open[m.note] > 0) ++bad; ++open[m.note]; ++ons; }
                else if (open[m.note] > 0) --open[m.note];
            }
            if (playing) ppq += 480 * rate;
        }
        std::vector<MidiOut> fl; e.flushMidi (&fl);
        for (auto& m : fl) if (open[m.note] > 0) --open[m.note];
        int stuck = 0; for (auto& [n, c] : open) stuck += c;
        std::printf ("  %d note-on, %d sin note-off, %d fuera de orden o del bloque\n", ons, stuck, bad);
        check (ons > 100 && stuck == 0 && bad == 0, "todo note-on tiene su note-off, nunca dos note-on seguidos de la misma nota");
    }

    std::printf ("\nT11 MIDI de entrada: mapa GM y cromático\n");
    {
        bool ok = true;
        const int gm[] = { 36, 38, 39, 37, 42, 46, 49, 51, 41, 45, 48, 50, 56, 75, 54, 52 };
        for (int s = 0; s < kNumSlots; ++s) { ok &= noteToSlot (gm[s], 0) == s && slotToNote (s, 0) == gm[s] && noteToSlot (36 + s, 1) == s && slotToNote (s, 1) == 36 + s; }
        ok &= noteToSlot (35, 0) == kKick && noteToSlot (40, 0) == kSnare && noteToSlot (44, 0) == kClosedHat && noteToSlot (60, 0) == -1 && noteToSlot (52, 1) == -1;
        check (ok, "las 16 voces en GM (con alias 35, 40, 44…) y cromático 36-51");
        // Una nota en una muestra precisa dispara su voz ahí, con el transporte parado.
        Engine e; e.prepare (fs, 4096);
        EngineParams p = quietParams();
        p.v[kRim].engine = 1; p.v[kRim].kit = kitgen::numKits(); p.v[kRim].chain = kChainClean; p.v[kRim].shape.decay = 1.0f; p.v[kRim].levelDb = 0; p.v[kRim].velSense = 0;
        e.set (p); e.setUserSample (kRim, &impulse);
        std::vector<MidiIn> in { { 1234, 37, 100, false }, { 3000, 37, 0, true } };
        std::vector<float> l (4096), r (4096); std::array<float*, 2 * kBuses> outs {}; outs[0] = l.data(); outs[1] = r.data();
        HostTime ht; ht.playing = false;
        std::vector<MidiOut> mo; e.process (outs.data(), 4096, ht, in.data(), (int) in.size(), &mo);
        int pk = 0; for (int i = 0; i < 4096; ++i) if (std::fabs (l[(size_t) i]) > std::fabs (l[(size_t) pk])) pk = i;
        std::printf ("  nota 37 en la muestra 1234: impulso en %d; MIDI de salida %zu mensajes\n", pk, mo.size());
        check (pk == 1234 + kVoiceLatency && mo.empty(), "la nota suena en su muestra y no se repite por la salida MIDI");
    }

    std::printf ("\nT12 Salidas múltiples\n");
    {
        auto run = [&] (int routing, bool busesOn, bool tape) {
            Engine e; e.prepare (fs, 4096);
            EngineParams p = quietParams(); p.g.routing = routing; p.m.tape = tape;
            p.v[kKick].levelDb = 0; p.v[kSnare].levelDb = 0; p.v[kKick].out = kBusKick; p.v[kSnare].out = kBusSnare;
            e.set (p);
            e.bankRef().p[0] = onePattern ({ { kKick, { 0 } }, { kSnare, { 8 } } });
            return renderEngine (e, 2.0, 120.0, 512, fs, 0.0, busesOn);
        };
        const auto R = run (2, true, false);
        const double mainKick = rms (R.l, 0, 20000), auxKick = rms (R.bus[2 * kBusKick], 0, 20000), auxSnare = rms (R.bus[2 * kBusSnare], 48000, 68000), mainSnare = rms (R.l, 48000, 68000);
        std::printf ("  bombo: principal %.1f dB, bus Kick %.1f dB; caja: principal %.1f dB, bus Snare %.1f dB\n", db (mainKick), db (auxKick), db (mainSnare), db (auxSnare));
        check (mainKick < 1e-6 && auxKick > 1e-3 && auxSnare > 1e-3 && mainSnare < 1e-6, "cada voz sale sólo por su bus");
        const auto F = run (2, false, false);
        check (rms (F.l, 0, 20000) > 1e-3, "un bus que el host no activó se suma al principal");
        const auto M = run (0, true, false);
        check (rms (M.bus[2 * kBusKick], 0, 20000) == 0.0 && rms (M.l, 0, 20000) > 1e-3, "Main only: todo por el principal");
        // Con la cinta en el maestro (54 muestras más), los auxiliares se atrasan lo mismo.
        Engine e; e.prepare (fs, 4096);
        EngineParams p = quietParams(); p.g.routing = 2; p.m.tape = true; p.v[kKick].levelDb = 0; p.v[kSnare].levelDb = 0; p.v[kSnare].out = kBusSnare;
        e.set (p); e.bankRef().p[0] = onePattern ({ { kKick, { 0 } }, { kSnare, { 0 } } });
        const auto T = renderEngine (e, 0.5, 120.0, 512, fs, 0.0, true);
        auto onset = [] (const std::vector<float>& x) { for (size_t i = 0; i < x.size(); ++i) if (std::fabs (x[i]) > 0.01f) return (int) i; return -1; };
        std::printf ("  con cinta: latencia %d; arranque del bombo (principal) %d, de la caja (bus) %d\n", e.latency(), onset (T.l), onset (T.bus[2 * kBusSnare]));
        check (std::abs (onset (T.l) - onset (T.bus[2 * kBusSnare])) <= 2 && e.latency() == kVoiceLatency + Master::kTapeLatency, "los buses quedan alineados con la latencia de la cinta");
    }

    std::printf ("\nT13 Muestras propias: limpia, al revés, de 8 bits\n");
    {
        SampleData sw; sw.rate = 44100; sw.chain = kChainClean;
        for (int i = 0; i < 44100; ++i) sw.data.push_back (0.5f * (float) std::sin (2 * kPiD * 441.0 * i / 44100.0) * (1.0f - (float) i / 44100.0f));
        auto play = [&] (int chain, bool rev, float tune) {
            Slot sl; sl.prepare ((float) fs, kRim);
            VoiceParams vp; vp.engine = 1; vp.kit = kitgen::numKits(); vp.chain = chain; vp.shape.decay = 1.0f; vp.shape.tune = tune; vp.reverse = rev; vp.velSense = 0;
            sl.set (vp); sl.trigger (1.0f, false, 0.0f, &sw);
            std::vector<float> o ((size_t) (1.2 * fs)); sl.render (o.data(), (int) o.size());
            return o;
        };
        const auto c = play (kChainClean, false, 0.0f);
        const double f = zcFreq (c, fs, 4800, 24000), a0 = rms (c, 2400, 4800), a1 = rms (c, 36000, 38400);
        std::printf ("  limpia: %.2f Hz (441 esperado), nivel al principio %.1f dB y al final %.1f dB\n", f, db (a0), db (a1));
        check (std::fabs (f - 441.0) < 0.5 && a0 > a1 * 2.0, "la limpia suena a su altura y en su orden");
        const auto rv = play (kChainClean, true, 0.0f);
        check (rms (rv, 2400, 4800) < rms (rv, 36000, 38400), "al revés: crece en vez de caer");
        const auto up = play (kChainClean, false, 12.0f);
        check (std::fabs (zcFreq (up, fs, 4800, 14000) - 882.0) < 2.0, "una octava arriba: 882 Hz");
        // 8 bits companded con un seno fijo: lo que no es el seno (ruido de cuantización e imágenes que deja el filtro)
        // queda ≈ 30-40 dB por debajo.
        for (int i = 0; i < 44100; ++i) sw.data[(size_t) i] = 0.5f * (float) std::sin (2 * kPiD * 441.0 * i / 44100.0);
        const auto b8 = play (kChain8, false, 0.0f);
        double sc = 0, cc = 0, ss = 0, sx = 0, cx = 0;
        for (size_t i = 4800; i < 33600; ++i) { const double ph = 2 * kPiD * 441.0 * (double) i / fs, si = std::sin (ph), co = std::cos (ph); ss += si * si; cc += co * co; sc += si * co; sx += si * b8[i]; cx += co * b8[i]; }
        const double det = ss * cc - sc * sc, A = (sx * cc - cx * sc) / det, B = (cx * ss - sx * sc) / det;
        double e2 = 0, s2 = 0;
        for (size_t i = 4800; i < 33600; ++i) { const double ph = 2 * kPiD * 441.0 * (double) i / fs, fit = A * std::sin (ph) + B * std::cos (ph); e2 += (b8[i] - fit) * (b8[i] - fit); s2 += fit * fit; }
        const double snr = 10 * std::log10 (s2 / std::max (1e-20, e2));
        std::printf ("  8 bits companded, seno de 441 Hz a −6 dBFS: %.1f dB de señal sobre el resto\n", snr);
        check (snr > 30.0 && snr < 45.0, "la cuantización companded se oye pero no destruye (30-45 dB)");
    }

    std::printf ("\nT14 Mediciones de las voces contra los análisis publicados (research §3)\n");
    {
        // Bombo de puente en T: se asienta en 49,5 Hz y arranca ~15 % más arriba.
        auto bd = voice (kKick, 0);
        const double fEarly = zcFreq (bd, fs, (size_t) (0.012 * fs), (size_t) (0.06 * fs)), fLate = zcFreq (bd, fs, (size_t) (0.35 * fs), (size_t) (0.7 * fs));
        size_t i20 = 0; double pk = 0; for (size_t i = 0; i < bd.size(); ++i) pk = std::max (pk, (double) std::fabs (bd[i]));
        for (size_t i = (size_t) (0.01 * fs); i < bd.size(); i += 240) if (rms (bd, i, i + 960) < pk * 0.1 / 1.414) { i20 = i; break; }
        std::printf ("  bombo puente en T: %.1f Hz entre 12 y 60 ms, %.1f Hz asentado; −20 dB a los %.0f ms\n", fEarly, fLate, i20 / fs * 1000);
        check (std::fabs (fLate - 49.5) < 2.0 && fEarly > fLate * 1.05 && fEarly < fLate * 1.3, "49,5 Hz asentado, arranque entre +5 y +30 %");
        check (i20 > 0.15 * fs && i20 < 0.5 * fs, "−20 dB entre 150 y 500 ms con Decay al medio (el original: 300 ms)");
        // Bombo VCO: barrido rápido.
        auto vk = voice (kKick, 1);
        const double f20 = instFreq (vk, fs, 0.020), f40 = instFreq (vk, fs, 0.040), fEnd = zcFreq (vk, fs, (size_t) (0.25 * fs), (size_t) (0.5 * fs));
        std::printf ("  bombo VCO: %.0f Hz a los 20 ms, %.0f a los 40 ms, %.1f asentado (ajuste publicado: 166, 111, 49,5)\n", f20, f40, fEnd);
        check (f20 > 110 && f20 < 220 && f40 > 75 && f40 < 150 && std::fabs (fEnd - 49.5) < 3.0, "dentro de ±35 % del ajuste publicado");
        // Caja de puente en T: los dos resonadores.
        Shape sh; sh.a = 0.0f;                      // sin snappy: sólo el cuerpo
        auto sd = voice (kSnare, 0, sh);
        const double s1 = peakFreq (sd, 200, 300, 1, fs, 0, (size_t) (0.15 * fs)), s2 = peakFreq (sd, 420, 540, 1, fs, 0, (size_t) (0.1 * fs));
        std::printf ("  caja: resonadores en %.0f y %.0f Hz (238 y 476)\n", s1, s2);
        check (std::fabs (s1 - 238) < 4 && std::fabs (s2 - 476) < 6, "238 y 476 Hz");
        // Rim y clave.
        auto rs = voice (kRim, 0); auto cl = voice (kClave, 0);
        const double r1 = peakFreq (rs, 400, 520, 1, fs, 0, (size_t) (0.05 * fs)), r2 = peakFreq (rs, 1550, 1800, 2, fs, 0, (size_t) (0.03 * fs)), c1 = peakFreq (cl, 2300, 2700, 2, fs, 0, (size_t) (0.05 * fs));
        std::printf ("  rim %.0f + %.0f Hz (455 + 1.667), clave %.0f Hz (2.500)\n", r1, r2, c1);
        check (std::fabs (r1 - 455) < 8 && std::fabs (r2 - 1667) < 25 && std::fabs (c1 - 2500) < 30, "rim y clave en sus frecuencias");
        // Cencerro: los armónicos de 540 y 800 Hz.
        auto cb = voice (kCowbell, 0, Shape(), 1.0f, 0.0f, 0.5);
        double a540 = 0, a800 = 0, aOff = 0;
        for (int k : { 1, 3, 5 }) { a540 += toneAmp (cb, 540.0 * k, fs, 0, 9600); a800 += toneAmp (cb, 800.0 * k, fs, 0, 9600); aOff += toneAmp (cb, 670.0 * k, fs, 0, 9600); }
        std::printf ("  cencerro: armónicos impares de 540 Hz %.1f dB, de 800 Hz %.1f dB, entre ellos %.1f dB\n", db (a540), db (a800), db (aOff));
        check (a540 > 4 * aOff && a800 > 4 * aOff, "las dos cuadradas de 540 y 800 Hz dominan");
        // Charles: las rayas son armónicos impares de las seis cuadradas.
        Shape hs; hs.decay = 1.0f;
        auto oh = voice (kOpenHat, 0, hs, 1.0f, 0.0f, 1.0);
        int match = 0, total = 0;
        {
            // Las 12 rayas más fuertes entre 6 y 14 kHz.
            std::vector<std::pair<double, double>> pk;
            for (double f = 6000; f <= 14000; f += 5) pk.push_back ({ toneAmp (oh, f, fs, 2400, 38400), f });
            std::sort (pk.rbegin(), pk.rend());
            std::vector<double> picked;
            for (auto& [a, f] : pk) { bool near = false; for (double q : picked) near |= std::fabs (q - f) < 60; if (! near) picked.push_back (f); if (picked.size() == 12) break; }
            for (double f : picked)
            {
                ++total; bool m = false;
                for (int o = 0; o < 6; ++o) { const double h = std::round (f / metalFreqs()[o]); if ((int) h % 2 == 1 && std::fabs (f - h * metalFreqs()[o]) < 12) m = true; }
                match += m;
            }
        }
        std::printf ("  charles abierto: %d de las %d rayas más fuertes (6-14 kHz) son armónicos impares de 205,3 / 304,4 / 369,6 / 522,7 / 540 / 800 Hz\n", match, total);
        check (match >= 10, "el espectro es el de las seis cuadradas");
        // Aplauso: ráfagas cada ~11 ms y la cola.
        auto cp = voice (kClap, 0, Shape(), 1.0f, 0.0f, 0.5);
        std::vector<int> bursts; float prevE = 0;
        for (int i = 0; i < (int) (0.05 * fs); i += 24) { const float e = (float) rms (cp, (size_t) i, (size_t) i + 24); if (e > prevE * 3.0f && e > 0.02f) bursts.push_back (i); prevE = e; }
        std::printf ("  aplauso: ataques a"); for (int b : bursts) std::printf (" %.1f", b / fs * 1000); std::printf (" ms\n");
        check (bursts.size() >= 3 && std::fabs ((bursts[1] - bursts[0]) / fs * 1000 - 11.0) < 3.0, "ráfagas cada 11 ± 3 ms (el original: ~10 ms)");
        // Twin Tank: las tres cuadradas de 4.080, 5.620 y 6.170 Hz.
        Shape ms; ms.a = 1.0f; ms.decay = 1.0f;
        auto m78 = voice (kClosedHat, 2, ms, 1.0f, 0.0f, 0.2);
        const double m1 = toneAmp (m78, 5620, fs, 0, 4800), m2 = toneAmp (m78, 6170, fs, 0, 4800), mOff = toneAmp (m78, 5900, fs, 0, 4800);
        std::printf ("  twin tank: 5.620 Hz %.1f dB, 6.170 Hz %.1f dB, 5.900 Hz %.1f dB\n", db (m1), db (m2), db (mOff));
        check (m1 > 3 * mOff && m2 > 3 * mOff, "las cuadradas del metal en sus frecuencias");
        // Tom hexagonal: el "pew" baja.
        Shape th; th.b = 0.8f; th.a = 0.0f;           // sin ruido: se mide la altura del oscilador
        auto tm = voice (kTom2, 1, th);
        const double tA = zcFreq (tm, fs, (size_t) (0.005 * fs), (size_t) (0.03 * fs)), tB = zcFreq (tm, fs, (size_t) (0.4 * fs), (size_t) (0.6 * fs));
        std::printf ("  tom hexagonal con Bend 80 %%: %.0f Hz al principio, %.0f Hz a los 0,5 s (base 110 Hz)\n", tA, tB);
        check (tA > 2.0 * tB && std::fabs (tB - 110) < 12, "barrido de más de una octava que se asienta en la base");
    }

    std::printf ("\nT15 Convertidor companding de 8 bits (DAC-76)\n");
    {
        std::set<float> lv; for (int c = 0; c < 256; ++c) lv.insert (dac76::decode ((uint8_t) c));
        std::printf ("  %zu niveles distintos, máximo %.4f, primer paso %.6f (2/8031)\n", lv.size(), *lv.rbegin(), dac76::decode (0x81));
        check (lv.size() == 255 && std::fabs (*lv.rbegin() - 1.0f) < 1e-6 && std::fabs (dac76::decode (0x81) - 2.0f / 8031.0f) < 1e-7, "255 niveles, 72 dB");
        auto snr = [&] (double ampDb) {
            const double a = std::pow (10.0, ampDb / 20.0); double s2 = 0, e2 = 0;
            for (int i = 0; i < 48000; ++i) { const double x = a * std::sin (2 * kPiD * 997.0 * i / 48000.0); const double q = dac76::quantize ((float) x); s2 += x * x; e2 += (q - x) * (q - x); }
            return 10 * std::log10 (s2 / e2);
        };
        const double s0 = snr (0), s20 = snr (-20), s40 = snr (-40);
        std::printf ("  relación señal/ruido de un seno de 997 Hz: %.1f dB a 0 dBFS, %.1f a −20, %.1f a −40 (research: 39,5 / 37,1 / 34,7)\n", s0, s20, s40);
        check (std::fabs (s0 - 39.5) < 1.5 && std::fabs (s20 - 37.1) < 1.5 && std::fabs (s40 - 34.7) < 2.0, "el ruido sigue a la señal");
    }

    std::printf ("\nT16 Altura por reloj: las imágenes del convertidor quedan, la grilla del host no agrega aliasing\n");
    {
        // Una tabla de 9 kHz a 28 kHz: imagen en 28 − 9 = 19 kHz (real); la de 28 + 9 = 37 kHz doblaría a 11 kHz.
        SampleData t9; t9.rate = 28000; t9.chain = kChain8;
        for (int i = 0; i < 28000; ++i) t9.data.push_back (0.7f * (float) std::sin (2 * kPiD * 9000.0 * i / 28000.0));
        Slot sl; sl.prepare ((float) fs, kClave);
        VoiceParams vp; vp.engine = 1; vp.kit = kitgen::numKits(); vp.chain = kChain26; vp.shape.decay = 1.0f; vp.velSense = 0;
        // Con la cadena de 12 bits a 26,04 kHz sin filtro: tono 9 kHz (reloj fijo, tabla a 28 kHz → altura 9 kHz), imagen en 17,04 kHz.
        sl.set (vp); sl.trigger (1.0f, false, 0.0f, &t9);
        std::vector<float> o ((size_t) (0.8 * fs)); sl.render (o.data(), (int) o.size());
        const double base = toneAmp (o, 9000, fs, 4800, 28800), img = toneAmp (o, 26040 - 9000, fs, 4800, 28800), fold = toneAmp (o, 48000 - (26040 + 9000), fs, 4800, 28800);
        std::printf ("  12 bits 26 kHz: 9 kHz %.1f dB, imagen a 17,04 kHz %.1f dB, plegado del host a 12,96 kHz %.1f dB\n", db (base), db (img), db (fold));
        check (img > base * 0.1 && fold < base * 0.003, "la imagen está (> −20 dB) y el plegado no (< −50 dB)");
        // 8 bits con el reloj de la tabla, una octava abajo: el contenido baja, la imagen sube (14 − 4,5 = 9,5 kHz).
        Slot s8; s8.prepare ((float) fs, kClave);
        VoiceParams v8 = vp; v8.chain = kChain8; v8.shape.tune = -12.0f;
        s8.set (v8); s8.trigger (1.0f, false, 0.0f, &t9);
        std::vector<float> o8 ((size_t) (0.8 * fs)); s8.render (o8.data(), (int) o8.size());
        const double b8 = toneAmp (o8, 4500, fs, 4800, 28800), i8 = toneAmp (o8, 14000 - 4500, fs, 4800, 28800);
        std::printf ("  8 bits una octava abajo: 4,5 kHz %.1f dB, imagen a 9,5 kHz %.1f dB\n", db (b8), db (i8));
        check (i8 > b8 * 0.1, "bajar el reloj trae la imagen dentro de la banda (el brillo áspero de las cajas de 8 bits)");
    }

    std::printf ("\nT17 Reverb con compuerta: la cola se corta al tiempo de la compuerta\n");
    {
        Space sp; sp.prepare ((float) fs);
        SpaceParams pp; pp.type = kGated; pp.gateMs = 250; pp.size = 0.7f; sp.set (pp, true);
        auto sd = voice (kSnare, 0, Shape(), 1.0f, 0.0f, 1.0);
        std::vector<float> L = sd, R = sd;
        sp.process (L.data(), R.data(), (int) L.size());
        const double during = rms (L, (size_t) (0.15 * fs), (size_t) (0.22 * fs)), after = rms (L, (size_t) (0.3 * fs), (size_t) (0.5 * fs));
        std::printf ("  150-220 ms %.1f dB, 300-500 ms %.1f dB\n", db (during), db (after));
        check (during > 1e-3 && after < during * 0.003, "la sala suena entera durante la compuerta y cae más de 50 dB después");
        Space nl; nl.prepare ((float) fs);
        SpaceParams np; np.type = kNonlinear; np.gateMs = 300; np.shape = 0.0f; nl.set (np, true);
        std::vector<float> iL ((size_t) fs, 0.0f), iR = iL; iL[100] = iR[100] = 1.0f;
        nl.process (iL.data(), iR.data(), (int) iL.size());
        const double in = rms (iL, (size_t) (0.05 * fs), (size_t) (0.28 * fs)), out = rms (iL, (size_t) (0.33 * fs), (size_t) (0.6 * fs));
        double early = rms (iL, (size_t) (0.02 * fs), (size_t) (0.1 * fs)), late = rms (iL, (size_t) (0.2 * fs), (size_t) (0.28 * fs));
        std::printf ("  no lineal de 300 ms: dentro %.1f dB (primer tramo %.1f, último %.1f), después %.1f dB\n", db (in), db (early), db (late), db (out));
        check (out < in * 0.01 && std::fabs (db (early) - db (late)) < 4.0, "plana durante 300 ms y corte seco");
    }

    std::printf ("\nT18 Robustez: kits, ajustes extremos al azar, sin NaN ni continua\n");
    {
        bool ok = true; float peak = 0; double dcMax = 0;
        uint32_t s = 99;
        auto rnd = [&] { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) (s >> 8) / 16777216.0f; };
        for (int t = 0; t < 30; ++t)
        {
            Engine e; e.prepare (fs, 4096);
            EngineParams p;
            for (int v = 0; v < kNumSlots; ++v)
            {
                auto& vp = p.v[(size_t) v];
                vp.engine = rnd() < 0.5f; vp.model = (int) (rnd() * 5); vp.kit = (int) (rnd() * (kitgen::numKits() + 1)); vp.chain = (int) (rnd() * 5);
                vp.shape = { 48 * rnd() - 24, rnd(), rnd(), rnd(), rnd() }; vp.reverse = rnd() < 0.2f;
                vp.levelDb = -20 + 26 * rnd(); vp.pan = 2 * rnd() - 1; vp.drive = rnd() * rnd(); vp.bits = rnd() < 0.3f ? 2 + 14 * rnd() : 16; vp.rateHz = rnd() < 0.3f ? 1000 + 40000 * rnd() : 0;
                vp.filter = (int) (rnd() * 4); vp.cutoff = 20 + 19000 * rnd(); vp.reso = rnd(); vp.choke = (int) (rnd() * 5); vp.sendA = rnd(); vp.sendB = rnd();
            }
            p.a.type = (int) (rnd() * (float) kNumSpaceTypes); p.a.size = rnd(); p.a.gateMs = 40 + 1100 * rnd(); p.a.shape = 2 * rnd() - 1; p.aReturnDb = 0;
            p.b.type = (int) (rnd() * (float) kNumEchoTypes); p.b.feedback = 0.95f * rnd(); p.b.rate = rnd(); p.b.depth = rnd(); p.bReturnDb = 0;
            p.m.tape = rnd() < 0.5f; p.m.tapeDriveDb = 18 * rnd(); p.m.comp = rnd(); p.m.exciter = rnd(); p.m.lofi = (int) (rnd() * (float) kNumLofi);
            p.g.seq.swing = 0.5f + 0.25f * rnd();
            e.set (p);
            Pattern pt; pt.length = (uint8_t) (1 + (int) (rnd() * 64)); pt.scale = (uint8_t) (rnd() * 5);
            for (int tr = 0; tr < kTracks; ++tr) for (int st = 0; st < kMaxSteps; ++st) if (rnd() < 0.25f)
            {
                auto& sp = pt.t[(size_t) tr].s[(size_t) st];
                sp.vel = (uint8_t) (1 + rnd() * 126); sp.flags = rnd() < 0.3f; sp.prob = (uint8_t) (rnd() * 100); sp.ratchet = (uint8_t) (1 + rnd() * 7);
                sp.flam = rnd() < 0.2f ? (uint8_t) (rnd() * 60) : 0; sp.nudge = (int8_t) (rnd() * 100 - 50); sp.cond = (uint8_t) (rnd() * (float) kNumConds);
            }
            e.bankRef().p[0] = pt;
            const auto R = renderEngine (e, 3.0, 60 + 200 * rnd(), 0, fs);
            double mean = 0;
            for (size_t i = 0; i < R.l.size(); ++i) { ok &= dh::isFiniteF (R.l[i]) && dh::isFiniteF (R.r[i]); peak = std::max (peak, std::fabs (R.l[i])); mean += R.l[i]; }
            dcMax = std::max (dcMax, std::fabs (mean / (double) R.l.size()));
        }
        std::printf ("  30 configuraciones: pico máximo %.3f, continua máxima %.5f\n", peak, dcMax);
        check (ok, "sin NaN ni Inf");
        check (peak <= 1.0f, "nunca pasa el techo del maestro");
        check (dcMax < 0.01, "sin continua apreciable");
        // Silencio largo después de tocar: sin denormales (el tiempo por bloque no sube) y vuelve a cero.
        Engine e; e.prepare (fs, 4096); EngineParams p; p.a.type = kHall; p.aReturnDb = 0; p.b.type = kTape; p.bReturnDb = 0; p.m.tape = true; p.m.comp = 0.6f;
        for (auto& vp : p.v) { vp.sendA = 0.5f; vp.sendB = 0.3f; }
        e.set (p); e.bankRef().p[0] = onePattern ({ { kKick, { 0 } }, { kSnare, { 4 } }, { kCrash, { 0 } } });
        renderEngine (e, 1.0, 120.0, 512, fs);
        std::vector<float> l (512), r (512); std::array<float*, 2 * kBuses> outs {}; outs[0] = l.data(); outs[1] = r.data();
        HostTime stop; stop.playing = false;
        double tLoud = 0, tQuiet = 0; float last = 1;
        for (int b = 0; b < 2000; ++b)
        {
            const auto t0 = std::chrono::steady_clock::now();
            e.process (outs.data(), 512, stop, nullptr, 0, nullptr);
            const double dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            if (b < 100) tLoud += dt; if (b >= 1900) tQuiet += dt;
            last = 0; for (float x : l) last = std::max (last, std::fabs (x));
        }
        std::printf ("  después de 21 s de silencio: pico %.2e; tiempo por bloque al final / al principio %.2f\n", last, tQuiet / std::max (1e-12, tLoud));
        check (last < 1e-4f && tQuiet < tLoud * 3.0, "decae a silencio sin que el tiempo de cómputo suba");
    }

    std::printf ("\nT19 Kits de fábrica\n");
    {
        bool ok = true; size_t bytes = lib.bytes();
        for (int k = 0; k < kitgen::numKits(); ++k)
            for (int s = 0; s < kNumSlots; ++s)
            {
                const auto* sd = lib.sample (k, s);
                float pk = 0; bool fin = true; for (float x : sd->data) { pk = std::max (pk, std::fabs (x)); fin &= dh::isFiniteF (x); }
                if (! fin || pk < 0.1f || pk > 1.0f || sd->data.size() < 100) { ok = false; std::printf ("  %s / %d: pico %.2f, %zu muestras\n", sd->name.c_str(), s, pk, sd->data.size()); }
            }
        std::printf ("  %d kits × 16 sonidos generados en %.0f ms, %.1f MB\n", kitgen::numKits(), libMs, bytes / 1e6);
        check (ok, "todos los sonidos existen, sin NaN, con su nivel");
        check (libMs < 2000.0, "se generan en menos de 2 s");
    }

    std::printf ("\nT21 Swing por patrón, compuerta de la salida MIDI y solo\n");
    {
        auto onsets = [] (const std::vector<float>& x) { std::vector<int> on; for (size_t i = 1; i + 1 < x.size(); ++i) if (x[i] > 0.3f && x[i] >= x[i - 1] && x[i] > x[i + 1]) on.push_back ((int) i); return on; };
        auto base = [&] () {
            EngineParams p = quietParams();
            p.v[kRim].engine = 1; p.v[kRim].kit = kitgen::numKits(); p.v[kRim].chain = kChainClean; p.v[kRim].shape.decay = 1.0f; p.v[kRim].levelDb = 0; p.v[kRim].velSense = 0;
            return p;
        };
        // El patrón trae 66 %, el general queda en 50: manda el del patrón (120 BPM: 7920 y 4080 muestras).
        Engine e; e.prepare (fs, 4096);
        auto p = base(); p.g.seq.swing = 0.5f; e.set (p); e.setUserSample (kRim, &impulse);
        auto pt = onePattern ({ { kRim, { 0, 1, 2, 3 } } }); pt.swing = 66; e.bankRef().p[0] = pt;
        auto on = onsets (renderEngine (e, 1.0, 120.0, 512, fs).l);
        const int d1 = on.size() > 2 ? on[1] - on[0] : 0, d2 = on.size() > 2 ? on[2] - on[1] : 0;
        std::printf ("  patrón al 66 %% con el general al 50 %%: %d y %d muestras\n", d1, d2);
        check (d1 == 7920 && d2 == 4080, "el swing del patrón reemplaza al general");
        // Sin swing propio sigue al general.
        Engine e2; e2.prepare (fs, 4096); auto p2 = base(); p2.g.seq.swing = 0.75f; e2.set (p2); e2.setUserSample (kRim, &impulse);
        e2.bankRef().p[0] = onePattern ({ { kRim, { 0, 1, 2, 3 } } });
        auto on2 = onsets (renderEngine (e2, 1.0, 120.0, 512, fs).l);
        check (on2.size() > 2 && on2[1] - on2[0] == 9000, "sin swing propio, el general (75 %: 9000 muestras)");
        // Compuerta: 120 ms con negras a 120 BPM: note-off 5760 muestras después de cada note-on.
        Engine e3; e3.prepare (fs, 4096); auto p3 = base(); p3.g.midiGateMs = 120.0f; e3.set (p3);
        e3.bankRef().p[0] = onePattern ({ { kKick, { 0, 4, 8, 12 } } });
        auto R3 = renderEngine (e3, 2.1, 120.0, 333, fs);
        bool gateOk = R3.midi.size() >= 8; int64_t lastOn = -1; int pairs = 0;
        for (size_t i = 0; i < R3.midi.size(); ++i)
        {
            if (R3.midi[i].vel > 0) lastOn = R3.midiAbs[i];
            else if (lastOn >= 0) { gateOk &= R3.midiAbs[i] - lastOn == 5760; ++pairs; }
        }
        std::printf ("  compuerta de 120 ms: %d pares, note-off a %lld muestras\n", pairs, R3.midi.size() > 1 ? (long long) (R3.midiAbs[1] - R3.midiAbs[0]) : -1LL);
        check (gateOk && pairs >= 4, "las notas de la salida MIDI duran lo pedido");
        // Solo: con la caja en solo, el bombo no suena; sin solo, los dos.
        auto level = [&] (bool soloSnare) {
            Engine e4; e4.prepare (fs, 4096); EngineParams p4 = quietParams();
            p4.v[kKick].levelDb = 0; p4.v[kSnare].levelDb = 0; p4.v[kSnare].solo = soloSnare; e4.set (p4);
            e4.bankRef().p[0] = onePattern ({ { kKick, { 0 } }, { kSnare, { 8 } } });
            auto R = renderEngine (e4, 2.0, 120.0, 512, fs);
            return std::make_pair (rms (R.l, 0, (size_t) (0.4 * fs)), rms (R.l, (size_t) (1.0 * fs), (size_t) (1.4 * fs)));
        };
        const auto a = level (false), b = level (true);
        std::printf ("  bombo %.1f / %.1f dB, caja %.1f / %.1f dB (sin solo / caja en solo)\n", db (a.first), db (b.first), db (a.second), db (b.second));
        check (a.first > 1e-3 && b.first < 1e-6 && std::fabs (db (a.second) - db (b.second)) < 0.1, "el solo calla a los demás y deja la voz igual");
    }

    std::printf ("\nT20 CPU (48 kHz, bloques de 512, un núcleo)\n");
    {
        auto bench = [&] (EngineParams p, const char* what, bool allVoices) {
            Engine e; e.prepare (fs, 4096); e.set (p);
            Pattern pt;
            for (int tr = 0; tr < kTracks; ++tr)
                for (int st = 0; st < 16; ++st)
                    if (allVoices ? (st % 2 == tr % 2) : (tr == kKick ? st % 4 == 0 : tr == kSnare ? st % 8 == 4 : tr == kClosedHat ? true : (tr == kOpenHat && st == 14)))
                        pt.t[(size_t) tr].s[(size_t) st].vel = 100;
            e.bankRef().p[0] = pt;
            const auto t0 = std::chrono::steady_clock::now();
            renderEngine (e, 10.0, 128.0, 512, fs);
            const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
            std::printf ("  %-58s %.2f %%\n", what, secs * 10.0);
        };
        EngineParams basic; basic.aReturnDb = -200; basic.bReturnDb = -200;
        bench (basic, "bombo, caja y charles analógicos, sin efectos", false);
        EngineParams full; full.a.type = kGated; full.b.type = kTape; full.m.tape = true; full.m.comp = 0.5f; full.m.exciter = 0.3f;
        for (auto& vp : full.v) { vp.sendA = 0.3f; vp.sendB = 0.2f; }
        bench (full, "las 16 voces analógicas, gated, cinta, maestro completo", true);
        EngineParams dig = full; for (int v = 0; v < kNumSlots; ++v) { dig.v[(size_t) v].engine = 1; dig.v[(size_t) v].kit = v % kitgen::numKits(); }
        bench (dig, "las 16 voces digitales, mismos efectos", true);
        EngineParams nl = full; nl.a.type = kNonlinear; nl.a.gateMs = 800; nl.b.type = kEnsemble; nl.m.lofi = kLofi26;
        bench (nl, "16 voces, no lineal de 800 ms, ensemble, lo-fi, cinta", true);
    }

    std::printf ("\n%s (%d fallas)\n", fails ? "HAY FALLAS" : "TODO OK", fails);
    return fails ? 1 : 0;
}
