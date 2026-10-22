// thrumtest: pruebas numeradas del motor de Thrum sin JUCE (docs/research.md §10). Cada prueba imprime lo que midió
// y [ok] o [FALLA]; al final, el total. Las señales se sintetizan acá: no hacen falta archivos.
//   thrumtest            todas las pruebas
//   thrumtest 1 4 9      sólo esas
//   thrumtest --tuning   la tabla completa de afinación por cuerda y traste (todos los tipos)
//   thrumtest --amp-levels  la tabla del nivel automático de los amplificadores (Amp::autoLevelDb)
#include "Engine.h"
#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <cstdlib>
#include <set>
#include <string>
#if (defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)) && ! defined(__aarch64__)
 #include <xmmintrin.h>
#endif

using namespace dh::gt;
using dh::isFiniteF;
static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what); if (! ok) ++fails; }
static double db (double x) { return 20.0 * std::log10 (std::max (1e-12, x)); }
static std::string fmt (const char* f, ...) { char b[512]; va_list a; va_start (a, f); std::vsnprintf (b, sizeof b, f, a); va_end (a); return b; }

// ── Utilidades ──
static const double kFs = 48000.0;

struct Spectrum
{
    std::vector<double> mag; double fs = kFs; size_t N = 0;
    Spectrum (const std::vector<float>& x, size_t from, size_t N_, double fs_) : fs (fs_), N (N_)
    {
        std::vector<std::complex<double>> a (N);
        for (size_t i = 0; i < N && from + i < x.size(); ++i) a[i] = x[from + i] * (0.5 - 0.5 * std::cos (2.0 * kPiD * (double) i / (double) (N - 1)));
        fftInPlace (a, false);
        mag.resize (N / 2);
        for (size_t i = 0; i < N / 2; ++i) mag[i] = std::abs (a[i]);
    }
    // Pico más fuerte entre lo y hi (Hz), con interpolación parabólica en log.
    double peak (double lo, double hi, double* level = nullptr) const
    {
        const size_t a = std::max<size_t> (2, (size_t) (lo * N / fs)), b = std::min (mag.size() - 2, (size_t) (hi * N / fs));
        size_t bi = a; for (size_t i = a; i <= b; ++i) if (mag[i] > mag[bi]) bi = i;
        const double m0 = std::log (mag[bi - 1] + 1e-30), m1 = std::log (mag[bi] + 1e-30), m2 = std::log (mag[bi + 1] + 1e-30);
        const double den = m0 - 2 * m1 + m2;
        const double d = std::fabs (den) > 1e-12 ? 0.5 * (m0 - m2) / den : 0.0;
        if (level) *level = mag[bi];
        return ((double) bi + d) * fs / (double) N;
    }
    double band (double lo, double hi) const
    {
        double e = 0; for (size_t i = (size_t) (lo * N / fs); i < std::min (mag.size(), (size_t) (hi * N / fs)); ++i) e += mag[i] * mag[i];
        return std::sqrt (e);
    }
};

static uint64_t fingerprint (const std::vector<float>& x)
{
    uint64_t h = 1469598103934665603ull;
    for (float v : x) { uint32_t u; std::memcpy (&u, &v, 4); h = (h ^ u) * 1099511628211ull; }
    return h;
}

// Un motor con la cadena apagada (sólo la guitarra hasta el jack: la DI).
static EngineParams dryParams (int type = 0)
{
    EngineParams p;
    p.ins.type = type; p.ins.sympathetic = 0.0f; p.ins.pickupNoise = 0.0f;
    p.ampOn = false; p.cabOn = false;
    p.perf.humanTune = p.perf.humanTime = p.perf.humanVel = p.perf.humanPick = 0.0f;
    p.perf.autoVibrato = false; p.perf.releaseNoise = 0.0f; p.perf.attackNoise = 0.0f;
    return p;
}

struct Render { std::vector<float> l, r, di; std::vector<MidiOutEv> midi; };
// Renderiza secs segundos; block 0 = al azar (1..700, con bloques vacíos). hostJitter: la rendija de redondeo que deja
// un host entre bloques (negras).
static Render render (Engine& e, double secs, const std::vector<MidiMsg>& midiAbs, int block, bool playing = false, double bpm = 120.0, double hostJitter = 0.0)
{
    const int n = (int) (secs * kFs);
    Render R; R.l.assign ((size_t) n, 0.0f); R.r.assign ((size_t) n, 0.0f); R.di.assign ((size_t) n, 0.0f);
    uint32_t s = 12345;
    std::vector<MidiMsg> blk; std::vector<MidiOutEv> mo;
    size_t mi = 0;
    for (int pos = 0; pos < n;)
    {
        int b = block > 0 ? block : (int) ((s = s * 1664525u + 1013904223u) >> 22) % 701;
        b = std::min (b, n - pos);
        blk.clear();
        while (mi < midiAbs.size() && midiAbs[mi].offset < pos + std::max (b, 1)) { auto m = midiAbs[mi++]; m.offset = std::max (0, m.offset - pos); blk.push_back (m); }
        if (b == 0) { HostTime h; h.playing = playing; h.bpm = bpm; h.ppq = (double) pos / kFs * bpm / 60.0; e.process (nullptr, nullptr, nullptr, 0, h, nullptr, 0, nullptr); continue; }
        HostTime h; h.playing = playing; h.bpm = bpm; h.ppq = (double) pos / kFs * bpm / 60.0;
        if (hostJitter != 0.0 && pos > 0) h.ppq += ((s >> 9) & 1 ? 1.0 : -1.0) * hostJitter;
        mo.clear();
        e.process (R.l.data() + pos, R.r.data() + pos, R.di.data() + pos, b, h, blk.data(), (int) blk.size(), &mo);
        for (auto m : mo) { m.t += pos; R.midi.push_back (m); }
        pos += b;
    }
    return R;
}

static MidiMsg on (int at, int note, int vel = 100, int ch = 1) { return { at, (uint8_t) (0x90 | (ch - 1)), (uint8_t) note, (uint8_t) vel }; }
static MidiMsg off (int at, int note, int ch = 1) { return { at, (uint8_t) (0x80 | (ch - 1)), (uint8_t) note, 0 }; }
static MidiMsg ccm (int at, int num, int val, int ch = 1) { return { at, (uint8_t) (0xB0 | (ch - 1)), (uint8_t) num, (uint8_t) val }; }
static MidiMsg bend (int at, int v14, int ch = 1) { return { at, (uint8_t) (0xE0 | (ch - 1)), (uint8_t) (v14 & 127), (uint8_t) ((v14 >> 7) & 127) }; }

// ── T1: afinación por cuerda y traste con la inarmonicidad esperada ──
static void t1 (bool full)
{
    std::printf ("T1 afinación por cuerda y traste contra k·f0·√(1+Bk²)\n");
    double worst1 = 0, worstK = 0; int count = 0;
    for (int type = 0; type < 6; ++type)
    {
        const auto& g = guitar (type);
        for (int s = 0; s < g.nStrings; ++s)
            for (int fr : { 0, 5, 12, 17 })
            {
                if (fr > g.frets) continue;
                Instrument ins; InstrumentParams ip; ip.type = type; ip.sympathetic = 0.0f;
                ins.prepare ((float) kFs); ins.set (ip);
                PluckSpec ps; ps.fret = (float) fr; ps.velocity = 0.45f; ps.posMm = 110.0f; ps.seed = 7; ps.noise = 0;
                ins.string (s).pluck (ps);
                const int n = (int) (kFs * 2.2);
                std::vector<float> y ((size_t) n);
                ins.process (y.data(), n);
                const auto ph = stringPhysics (g, s, ins.openNote (s) - ip.capo, ip.gauge, 0);
                const double f0 = ph.openHz * std::exp2 (fr / 12.0), B = fretB (ph, (float) fr);
                const Spectrum sp (y, (size_t) (0.25 * kFs), 1 << 16, kFs);
                // Parciales hasta el rango del ajuste (1,5 kHz y < 50 cents de la serie armónica), hasta 8.
                int K = std::min (8, (int) (1500.0 / f0));
                while (K > 2 && 600.0 * std::log2 (1.0 + B * K * K) > 50.0) --K;
                K = std::max (1, K);
                double e1 = 0, ek = 0, ref = 0;
                std::string line;
                for (int k = 1; k <= K; ++k)
                {
                    const double fe = partialHz (f0, B, k);
                    const double fm = sp.peak (fe - 0.3 * f0, fe + 0.3 * f0);
                    const double c = 1200.0 * std::log2 (fm / fe);
                    if (k == 1) { e1 = c; ref = c; }
                    else ek = std::max (ek, std::fabs (c - ref));           // desvío relativo a la fundamental
                    if (full) line += fmt (" %+.1f", c);
                }
                worst1 = std::max (worst1, std::fabs (e1)); worstK = std::max (worstK, ek); ++count;
                if (full) std::printf ("  %-12s s%d traste %2d  f0 %7.2f Hz  B %.2e  cents:%s\n", g.shortName, s, fr, f0, B, line.c_str());
            }
    }
    std::printf ("  %d notas: peor fundamental %.2f cents, peor parcial (relativo) %.2f cents\n", count, worst1, worstK);
    check (worst1 < 1.5, "fundamental dentro de ±1,5 cents en todos los tipos, cuerdas y trastes");
    check (worstK < 3.0, "parciales 2..8 dentro de ±3 cents de la cuerda rígida (en el rango del ajuste)");
}

// ── T2: digitación correcta y reproducible ──
static void t2()
{
    std::printf ("T2 digitación\n");
    Tab tab; tab.n = 6; tab.open = { 40, 45, 50, 55, 59, 64, 0 }; tab.frets = 22;
    Fingerer f; f.setTab (tab); FingerPrefs p;
    bool allValid = true;
    for (int note = 40; note <= 86; ++note) { const auto fp = f.place (note, 0, p); if (! fp.valid() || tab.open[(size_t) fp.string] + fp.fret != note) allValid = false; }
    check (allValid, "cada nota de E2 a D6 cae en una cuerda y un traste que la dan");
    // Reproducible y con la mano en una ventana razonable.
    const int mel[] = { 64, 67, 69, 71, 72, 71, 69, 67, 64, 62, 60, 59, 57, 55, 57, 59, 60, 62, 64 };
    std::vector<FretPos> a, b;
    for (int rep = 0; rep < 2; ++rep)
    {
        f.reset(); auto& out = rep ? b : a;
        for (int n : mel) { const auto fp = f.place (n, 0, p); f.commit (fp); out.push_back (fp); }
    }
    bool same = true; int maxJump = 0;
    for (size_t i = 0; i < a.size(); ++i) { same &= a[i].string == b[i].string && a[i].fret == b[i].fret; if (i && a[i].fret && a[i - 1].fret) maxJump = std::max (maxJump, std::abs (a[i].fret - a[i - 1].fret)); }
    check (same, "la misma melodía da la misma digitación");
    std::printf ("    melodía: salto máximo de traste entre notas pisadas %d\n", maxJump);
    check (maxJump <= 5, "una melodía por grados no hace saltar la mano más de 5 trastes");
    // Posición fija y cuerda preferida.
    FingerPrefs pf; pf.fixedPosition = 7;
    bool inPos = true;
    for (int n : { 57, 59, 60, 62, 64, 65, 67, 69 }) { f.reset(); const auto fp = f.place (n, 0, pf); inPos &= fp.fret >= 6 && fp.fret <= 11; }
    check (inPos, "con la mano fija en el traste 7 las notas caen entre los trastes 6 y 11");
    FingerPrefs ps; ps.preferredString = 2;
    f.reset();
    bool onStr = true; for (int n : { 52, 55, 57, 60 }) { const auto fp = f.place (n, 0, ps); onStr &= fp.string == 2; }
    check (onStr, "con la cuerda preferida (la 4.ª) las notas que entran en ella van ahí");
    // Notas simultáneas: cuerdas distintas.
    const int ch[] = { 40, 47, 52, 56, 59, 64 };
    FretPos out[6]; f.reset();
    const int placed = f.placeChord (ch, 6, 0, p, out);
    std::set<int> strs; bool okNotes = true; for (int i = 0; i < 6; ++i) { strs.insert (out[i].string); okNotes &= out[i].valid() && tab.open[(size_t) out[i].string] + out[i].fret == ch[i]; }
    check (placed == 6 && strs.size() == 6 && okNotes, "un acorde de seis notas usa las seis cuerdas, cada una con su nota");
}

// ── T3: acordes y voicings ──
static void t3()
{
    std::printf ("T3 acordes reconocidos y voicings tocables\n");
    struct C { const char* want; std::vector<int> n; };
    const std::vector<C> cs = { { "C", { 48, 52, 55 } }, { "Am", { 57, 60, 64 } }, { "G7", { 43, 47, 50, 53 } }, { "Dm7", { 50, 53, 57, 60 } },
                                { "Fmaj7", { 53, 57, 60, 64 } }, { "E5", { 40, 47 } }, { "Bdim", { 47, 50, 53 } }, { "Asus4", { 45, 50, 52 } },
                                { "C/E", { 40, 48, 55 } }, { "Cadd9", { 48, 50, 52, 55 } }, { "F#m7b5", { 54, 57, 60, 64 } }, { "Ebaug", { 51, 55, 59 } } };
    int good = 0;
    for (const auto& c : cs) { const auto ch = detectChord (c.n.data(), (int) c.n.size()); const auto nm = chordName (ch); good += nm == c.want; if (nm != c.want) std::printf ("    %s reconocido como %s\n", c.want, nm.c_str()); }
    check (good == (int) cs.size(), fmt ("%d de %d acordes reconocidos con su raíz, tipo y bajo", good, (int) cs.size()).c_str());
    // Voicings: 12 raíces × 8 tipos × 6 estilos × 3 regiones × 3 afinaciones.
    int total = 0, valid = 0, wrong = 0; double us = 0;
    for (int tun : { kStandard, kDropD, kOpenG })
    {
        const auto& g = guitar (0);
        const auto notes = tuningNotes (g, tun, {});
        Tab tab; tab.n = 6; tab.frets = 22; for (int s = 0; s < 6; ++s) tab.open[(size_t) s] = notes[(size_t) s];
        for (int root = 0; root < 12; ++root)
            for (int type : { 0, 1, 3, 4, 5, 6, 12, 17 })
                for (int style = 0; style < kNumStyles; ++style)
                    for (int region : { 0, 5, 9 })
                    {
                        Chord ch; ch.root = root; ch.type = type; ch.bass = root; ch.pcs = rotate12 (chordTypes()[(size_t) type].mask, root);
                        const auto t0 = std::chrono::steady_clock::now();
                        const auto v = findVoicing (tab, ch, style, region);
                        us += std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - t0).count();
                        ++total;
                        if (! v.valid) continue;
                        ++valid;
                        // Comprobar: cada nota del acorde (o raíz y quinta en power), el bajo, el estiramiento y los dedos.
                        const uint16_t want = style == kPower ? (uint16_t) ((1u << root) | (1u << ((root + 7) % 12))) : ch.pcs;
                        int fmin = 99, fmax = 0, lowS = -1; bool bad = false;
                        for (int s = 0; s < 6; ++s)
                        {
                            const int f = v.fret[(size_t) s]; if (f < 0) continue;
                            if (lowS < 0) lowS = s;
                            const int pc = (tab.open[(size_t) s] + f) % 12;
                            if (! (want & (1u << pc))) bad = true;
                            if (f > 0) { fmin = std::min (fmin, f); fmax = std::max (fmax, f); }
                        }
                        if (lowS < 0 || (tab.open[(size_t) lowS] + v.fret[(size_t) lowS]) % 12 != root) bad = true;
                        if (fmax > 0 && fmax - fmin > 3) bad = true;
                        wrong += bad;
                    }
    }
    std::printf ("    %d pedidos, %d con voicing (%.1f %%), %d con notas fuera del acorde, bajo errado o mano imposible; %.1f µs por búsqueda\n",
                 total, valid, 100.0 * valid / total, wrong, us / total);
    check (valid >= total * 97 / 100, "hay voicing para al menos el 97 % de los pedidos (estilo, región, afinación)");
    check (wrong == 0, "todos los voicings son tocables: notas del acorde, bajo correcto, cuatro trastes");
    check (us / total < 400.0, "la búsqueda de un voicing tarda menos de 0,4 ms en promedio");
}

// ── T4: tiempo del rasgueo exacto entre bloques ──
static std::vector<std::pair<int64_t, int>> strumEvents (int block, double jitter, double bpm, bool hostPlaying = true)
{
    Engine e; e.setSyncCab (true); e.prepare (kFs, 4096);
    auto p = dryParams(); p.perf.mode = kModeStrum; p.perf.humanTime = 0.5f; p.perf.midiOut = true; p.perf.midiOutPerString = true;
    e.set (p); e.reset();
    e.performer().setStrumPattern (parseStrum ("DxUD.UDU d.UuD.MmXD", "2010010010100100200", kDiv16, 0.58f));
    std::vector<MidiMsg> m = { on (100, 48), on (100, 52), on (100, 55), on ((int) (2.0 * kFs), 45), on ((int) (2.0 * kFs), 48), on ((int) (2.0 * kFs), 52),
                               off ((int) (1.99 * kFs), 48), off ((int) (1.99 * kFs), 52), off ((int) (1.99 * kFs), 55) };
    std::sort (m.begin(), m.end(), [] (const MidiMsg& a, const MidiMsg& b) { return a.offset < b.offset; });
    const auto R = render (e, 4.0, m, block, hostPlaying, bpm, jitter);
    std::vector<std::pair<int64_t, int>> ev;
    for (const auto& x : R.midi) if ((x.status & 0xF0) == 0x90) ev.push_back ({ x.t, (x.status & 15) * 128 + x.d1 });
    return ev;
}
static void t4()
{
    std::printf ("T4 rasgueo: cada cuerda en la misma muestra con cualquier bloque y con la rendija del host\n");
    const auto ref = strumEvents (512, 0.0, 117.0);
    bool same = true; std::string tried;
    for (int blk : { 1, 7, 64, 333, 4096, 0 })
    {
        const auto ev = strumEvents (blk, 0.0, 117.0);
        const bool eq = ev == ref; same &= eq;
        if (! eq)
            for (size_t i = 0; i < std::max (ev.size(), ref.size()); ++i)
                if (i >= ev.size() || i >= ref.size() || ev[i] != ref[i])
                {
                    std::printf ("    bloque %d: primer evento distinto #%zu: %lld/%d contra %lld/%d (de %zu y %zu)\n", blk, i, i < ev.size() ? (long long) ev[i].first : -1LL, i < ev.size() ? ev[i].second : -1,
                                 i < ref.size() ? (long long) ref[i].first : -1LL, i < ref.size() ? ref[i].second : -1, ev.size(), ref.size());
                    break;
                }
        tried += fmt (" %d:%s", blk, eq ? "=" : "≠");
    }
    const auto jit = strumEvents (512, 1e-9, 117.0);
    std::printf ("    %zu notas por cuerda en 4 s; bloques%s; con rendija %s\n", ref.size(), tried.c_str(), jit == ref ? "=" : "≠");
    check (same, "mismos eventos (muestra, canal, nota) con bloques de 1, 7, 64, 333, 4096 y al azar");
    check (jit == ref, "la rendija de redondeo del host entre bloques no pierde ni duplica pasos");
    // El primer golpe de cada paso cae en la grilla: semicorcheas a 117 BPM con swing 58 %.
    check (! ref.empty(), "el patrón suena");
    // Sin transporte: el reloj propio arranca en la muestra de la primera tecla.
    const auto st0 = strumEvents (512, 0.0, 117.0, false);
    bool sameStopped = ! st0.empty();
    for (int blk : { 1, 64, 4096, 0 }) sameStopped &= strumEvents (blk, 0.0, 117.0, false) == st0;
    std::printf ("    con el host parado: %zu notas, primera en la muestra %lld (la tecla entra en la 100)\n", st0.size(), st0.empty() ? -1LL : (long long) st0[0].first);
    check (sameStopped && ! st0.empty() && st0[0].first >= 100 && st0[0].first < 100 + (int) (0.006 * kFs), "con el host parado el patrón arranca en la tecla (más el tiempo humano) y da los mismos eventos con cualquier bloque");

    // Dónde cae el rasgueo respecto del tiempo: centrado (el medio en el tiempo) o desde el tiempo (la primera cuerda).
    // Un golpe abajo por negra a 120 BPM sobre un acorde de seis cuerdas (E mayor abierto), sin variación humana.
    auto anchorRun = [] (bool centred, bool changeOnBeat, int block) {
        Engine e; e.setSyncCab (true); e.prepare (kFs, 4096);
        auto p = dryParams(); p.perf.mode = kModeStrum; p.perf.midiOut = true; p.perf.midiOutPerString = true;
        p.perf.strumCentred = centred; p.perf.strumMs = 35.0f;
        e.set (p); e.reset();
        e.performer().setStrumPattern (parseStrum ("D", "2", kDiv4, 0.5f));
        const int beat = (int) (0.5 * kFs);     // negra a 120 BPM
        std::vector<MidiMsg> m = { on (100, 40), on (100, 44), on (100, 47) };          // E
        if (changeOnBeat)
        {
            // En el tiempo 4 (muestra 4·beat) el acorde cambia a A, cuantizado: note-off y note-on en la misma muestra.
            for (int n : { 40, 44, 47 }) m.push_back (off (4 * beat, n));
            for (int n : { 45, 49, 52 }) m.push_back (on (4 * beat, n));
        }
        std::stable_sort (m.begin(), m.end(), [] (const MidiMsg& a, const MidiMsg& b) { return a.offset < b.offset; });
        return render (e, 3.0, m, block, true, 120.0);
    };
    auto stepOnsets = [] (const Render& R, int64_t beat) {
        std::vector<int64_t> t;
        for (const auto& x : R.midi) if ((x.status & 0xF0) == 0x90 && x.t > beat - (int64_t) (0.06 * kFs) && x.t < beat + (int64_t) (0.1 * kFs)) t.push_back (x.t);
        std::sort (t.begin(), t.end());
        return t;
    };
    const int64_t b2 = (int64_t) (2 * 0.5 * kFs);
    const auto cen = stepOnsets (anchorRun (true, false, 512), b2), sta = stepOnsets (anchorRun (false, false, 512), b2);
    const auto cen1 = stepOnsets (anchorRun (true, false, 1), b2);
    const double midC = cen.size() == 6 ? 0.5 * (double) (cen[2] + cen[3]) : -1.0;
    std::printf ("    tiempo 3 en la muestra %lld: centrado %zu cuerdas de %+.1f a %+.1f ms (medio %+.2f ms); desde el tiempo %zu cuerdas de %+.1f a %+.1f ms\n",
                 (long long) b2, cen.size(), cen.empty() ? 0.0 : (double) (cen.front() - b2) / kFs * 1e3, cen.empty() ? 0.0 : (double) (cen.back() - b2) / kFs * 1e3, (midC - (double) b2) / kFs * 1e3,
                 sta.size(), sta.empty() ? 0.0 : (double) (sta.front() - b2) / kFs * 1e3, sta.empty() ? 0.0 : (double) (sta.back() - b2) / kFs * 1e3);
    check (cen.size() == 6 && std::fabs (midC - (double) b2) <= 0.6 * kFs * 1e-3 * 35.0 / 5.0 + 1.0, "rasgueo centrado: el medio (entre la 3.ª y la 4.ª cuerda de seis) cae en el tiempo");
    check (cen == cen1, "rasgueo centrado: los mismos tiempos con bloques de 1 y de 512");
    check (sta.size() == 6 && sta.front() == b2, "rasgueo desde el tiempo: la primera cuerda cae en el tiempo");
    // Cambio de acorde cuantizado en el tiempo 4 con el rasgueo centrado: lo que faltaba del acorde viejo no suena y el nuevo
    // se toca completo en seguida (las notas de A, nunca después de las de E).
    const int64_t b4 = (int64_t) (4 * 0.5 * kFs);
    const auto R = anchorRun (true, true, 333);
    int oldAfter = 0, newNotes = 0; int64_t firstNew = -1;
    for (const auto& x : R.midi)
        if ((x.status & 0xF0) == 0x90 && x.t >= b4 - (int64_t) (0.05 * kFs) && x.t < b4 + (int64_t) (0.1 * kFs))
        {
            const int pc = x.d1 % 12;
            const bool isA = pc == 9 || pc == 1 || pc == 4;           // A C# E (E es común: cuenta como del nuevo)
            if (isA && x.t >= b4) { ++newNotes; if (firstNew < 0) firstNew = x.t; }
            if (! isA && x.t >= b4) ++oldAfter;
        }
    std::printf ("    cambio de acorde en el tiempo: %d notas del acorde nuevo desde %+.1f ms, %d del viejo después del cambio\n", newNotes, firstNew < 0 ? -1.0 : (double) (firstNew - b4) / kFs * 1e3, oldAfter);
    check (oldAfter == 0 && newNotes >= 5 && firstNew >= b4 && firstNew <= b4 + (int64_t) (0.012 * kFs), "cambio de acorde en el tiempo: el resto del acorde viejo se anula y el nuevo arranca en menos de 12 ms");
}

// ── T5: determinismo del motor completo ──
static std::vector<MidiMsg> phrase()
{
    std::vector<MidiMsg> m;
    const int notes[] = { 52, 55, 57, 59, 62, 64, 62, 59, 57, 55 };
    for (int i = 0; i < 10; ++i) { m.push_back (on (2400 + i * 9000, notes[i], 70 + (i * 13) % 50)); m.push_back (off (2400 + i * 9000 + 8000, notes[i])); }
    m.push_back (on (100000, 40, 110)); m.push_back (on (100000, 47, 110)); m.push_back (on (100000, 52, 110));
    m.push_back (bend (110000, 8192 + 4000)); m.push_back (bend (130000, 8192));
    m.push_back (off (140000, 40)); m.push_back (off (140000, 47)); m.push_back (off (140000, 52));
    std::sort (m.begin(), m.end(), [] (const MidiMsg& a, const MidiMsg& b) { return a.offset < b.offset; });
    return m;
}
static EngineParams fullParams()
{
    EngineParams p; p.ins.pickupNoise = 0.2f;
    p.amp.model = kAmpCrunch; p.amp.gain = 0.6f;
    p.pedal[kOd].on = true; p.pedal[kOd].p[0] = 0.4f; p.pedal[kOd].p[1] = 0.5f; p.pedal[kOd].p[2] = 0.5f;
    p.pedal[kChorus].on = true; p.pedal[kChorus].p[0] = 0.3f; p.pedal[kChorus].p[1] = 0.5f; p.pedal[kChorus].p[2] = 0.5f; p.pedal[kChorus].p[3] = 0.6f;
    p.pedal[kDelay].on = true; p.pedal[kDelay].type = 2; p.pedal[kDelay].p[0] = 0.4f; p.pedal[kDelay].p[1] = 0.4f; p.pedal[kDelay].p[2] = 0.3f; p.pedal[kDelay].p[3] = 0.5f; p.pedal[kDelay].p[4] = 0.3f;
    p.pedal[kReverb].on = true; p.pedal[kReverb].type = 1; p.pedal[kReverb].p[0] = 0.5f; p.pedal[kReverb].p[1] = 0.5f; p.pedal[kReverb].p[2] = 0.25f; p.pedal[kReverb].p[4] = 0.5f;
    return p;
}
static void t5()
{
    std::printf ("T5 determinismo\n");
    uint64_t ref = 0; bool same = true; std::string s;
    for (int blk : { 512, 32, 1000, 0 })
    {
        Engine e; e.setSyncCab (true); e.prepare (kFs, 1024); e.set (fullParams()); e.reset();
        const auto R = render (e, 3.2, phrase(), blk);
        auto all = R.l; all.insert (all.end(), R.r.begin(), R.r.end()); all.insert (all.end(), R.di.begin(), R.di.end());
        const uint64_t h = fingerprint (all);
        if (blk == 512) ref = h; else same &= h == ref;
        s += fmt (" %d:%016llx", blk, (unsigned long long) h);
    }
    std::printf ("   %s\n", s.c_str());
    check (same, "la salida (procesada y DI) es idéntica bit a bit con bloques de 512, 32, 1000 y al azar");
}

// ── T6: MIDI de entrada y salida, MPE y guitarra MIDI ──
static void t6()
{
    std::printf ("T6 MIDI: salida balanceada, guitarra MIDI (un canal por cuerda) y MPE\n");
    {
        Engine e; e.setSyncCab (true); e.prepare (kFs, 512);
        auto p = dryParams(); p.perf.midiOut = true; p.perf.legato = kLegatoAuto; p.perf.mode = kModeMono;
        e.set (p); e.reset();
        std::vector<MidiMsg> m;
        // Legato (solapadas), repetidas, acorde, all notes off.
        int t = 1000;
        for (int i = 0; i < 40; ++i) { m.push_back (on (t, 60 + (i * 5) % 12, 90)); m.push_back (off (t + 9000, 60 + (i * 5) % 12)); t += 6000; }
        m.push_back (ccm (t + 100, 123, 0));
        std::sort (m.begin(), m.end(), [] (const MidiMsg& a, const MidiMsg& b) { return a.offset < b.offset; });
        const auto R = render (e, (t + 30000) / kFs, m, 0);
        int bal = 0, ons = 0; std::set<int> sounding;
        for (const auto& x : R.midi) { const int key = (x.status & 15) * 128 + x.d1; if ((x.status & 0xF0) == 0x90) { ++ons; if (! sounding.insert (key).second) bal += 1000; } else if (sounding.erase (key) == 0) bal += 1; }
        std::printf ("    %d note-on de salida, %zu notas colgadas, %d note-off huérfanos\n", ons, sounding.size(), bal);
        check (ons > 0 && sounding.empty() && bal == 0, "cada note-on de salida tiene su note-off (con ligado y all notes off)");
    }
    {
        // Guitarra MIDI: canal 1 = 1.ª cuerda (la más aguda), canal 6 = 6.ª. Bend sólo en el canal 2.
        Engine e; e.setSyncCab (true); e.prepare (kFs, 512);
        auto p = dryParams(); p.perf.midiIn = kMidiPerString; p.perf.stringBendRange = 12.0f; p.perf.midiOut = true; p.perf.midiOutPerString = true;
        e.set (p); e.reset();
        std::vector<MidiMsg> m = { on (100, 64, 90, 1), on (100, 64, 90, 2), bend (24000, 8192 + 1365, 2) };   // la misma nota en la 1.ª y la 2.ª
        const auto R = render (e, 1.5, m, 512);
        int s1 = -1, s2 = -1;
        for (int s = 0; s < 6; ++s) { const auto v = e.performer().view (s); if (v.note >= 0 && s == 5) s1 = s; if (v.note >= 0 && s == 4) s2 = s; }
        const float f1 = e.instrument().string (5).pitchHz(), f2 = e.instrument().string (4).pitchHz();
        const double c = 1200.0 * std::log2 (f2 / f1);
        std::printf ("    canal 1 → cuerda %d, canal 2 → cuerda %d; bend de +2 st en el canal 2: %+.1f cents entre las dos\n", 6 - s1, 6 - s2, c);
        check (s1 == 5 && s2 == 4, "cada canal toca su cuerda (1 = la más aguda)");
        check (std::fabs (c - 200.0) < 8.0, "el bend de un canal mueve sólo su cuerda (+2 semitonos ±8 cents)");
        bool chOk = true; for (const auto& x : R.midi) if ((x.status & 0xF0) == 0x90) chOk &= (x.status & 15) <= 1;
        check (chOk, "la salida por cuerda usa el canal de cada cuerda");
    }
    {
        // MPE: dos notas en canales de nota distintos; el bend del canal 3 mueve sólo su nota.
        Engine e; e.setSyncCab (true); e.prepare (kFs, 512);
        auto p = dryParams(); p.perf.midiIn = kMidiMpe; p.perf.mpeBendRange = 48.0f;
        e.set (p); e.reset();
        std::vector<MidiMsg> m = { on (100, 57, 90, 2), on (100, 64, 90, 3), bend (20000, 8192 + 171, 3) };     // +1 semitono con rango 48
        render (e, 1.0, m, 256);
        int sa = -1, sb = -1;
        for (int s = 0; s < 6; ++s) { const auto v = e.performer().view (s); if (v.note == 57) sa = s; if (v.note == 64) sb = s; }
        const float fa = sa >= 0 ? e.instrument().string (sa).pitchHz() : 0, fb = sb >= 0 ? e.instrument().string (sb).pitchHz() : 0;
        const double ca = 1200.0 * std::log2 (fa / midiHz (57)), cb = 1200.0 * std::log2 (fb / midiHz (64));
        std::printf ("    MPE: nota del canal 2 %+.1f cents, nota del canal 3 %+.1f cents (bend +1 st sólo en el 3)\n", ca, cb);
        check (std::fabs (ca) < 6.0 && std::fabs (cb - 100.0) < 8.0, "el bend por nota del MPE mueve sólo su nota");
    }
}

// ── T7: sin NaN, continua ni denormales ──
static void t7()
{
    std::printf ("T7 sin NaN, sin continua, sin denormales, silencio al final\n");
    uint32_t seed = 99;
    auto rnd = [&] { seed = seed * 1664525u + 1013904223u; return (float) (seed >> 8) / 16777216.0f; };
    int bad = 0, dcBad = 0, denorm = 0, tailBad = 0; double worstDc = -200, worstTail = -200;
    int stuck = 0;
    for (int k = 0; k < 160; ++k)
    {
        Engine e; e.setSyncCab (true); e.prepare (kFs, 512);
        EngineParams p;
        p.ins.type = (int) (rnd() * 6); p.ins.tuning = (int) (rnd() * 8); p.ins.gauge = (int) (rnd() * 4); p.ins.capo = (int) (rnd() * 5);
        p.ins.selector = (int) (rnd() * 5); p.ins.volume = rnd(); p.ins.tone = rnd(); p.ins.sympathetic = rnd(); p.ins.buzz = rnd() < 0.3f ? rnd() : 0.0f;
        p.ins.pickupNoise = 0.0f;
        p.perf.mode = (int) (rnd() * 5); p.perf.art = (int) (rnd() * (float) kNumArts); p.perf.feedback = rnd() < 0.3f ? rnd() : 0.0f;
        p.amp.model = (int) (rnd() * 4); p.amp.gain = rnd(); p.amp.bass = rnd(); p.amp.mid = rnd(); p.amp.treble = rnd(); p.amp.presence = rnd(); p.amp.master = rnd(); p.amp.sag = rnd();
        p.amp.quality = rnd() < 0.5f ? 2 : 4;
        for (int u = 0; u < kNumPedals; ++u)
        {
            auto& pp = p.pedal[(size_t) u]; pp.on = rnd() < 0.35f; for (auto& x : pp.p) x = rnd(); pp.type = (int) (rnd() * 3);
            if (u == kReverb) { pp.p[5] = 0.0f; pp.p[0] *= 0.5f; }    // sin congelar y con colas de hasta ~3 s: tiene que terminar en silencio
            if (u == kDelay) { pp.p[1] = std::min (pp.p[1], 0.6f); pp.p[0] *= 0.5f; pp.p[5] = 0.0f; }   // ecos de hasta ~400 ms
        }
        p.cab.a.position = rnd(); p.cab.a.distanceCm = 1 + 40 * rnd(); p.cab.mix = rnd(); p.cab.speaker = (int) (rnd() * 5); p.cab.cab = (int) (rnd() * 4);
        e.set (p); e.reset();
        std::vector<MidiMsg> m;
        for (int i = 0; i < 12; ++i) { const int nt = 40 + (int) (rnd() * 40); m.push_back (on (1000 + i * 3000, nt, 1 + (int) (rnd() * 126))); m.push_back (off (1000 + i * 3000 + (int) (rnd() * 20000), nt)); }
        // Caos de MIDI: teclas repetidas sin soltar, notas cortísimas, acordes en la misma muestra, pedal, keyswitches.
        if (k % 2 == 0)
        {
            const int nt = 52 + (int) (rnd() * 12);
            m.push_back (on (5000, nt, 90)); m.push_back (on (7000, nt, 90)); m.push_back (off (9000, nt));
            m.push_back (on (11000, 64, 100)); m.push_back (off (11030, 64));
            for (int c : { 48, 52, 55, 60 }) m.push_back (on (14000, c, 80));
            for (int c : { 48, 52, 55, 60 }) m.push_back (on (16000, c, 80));
            for (int c : { 48, 52, 55, 60 }) m.push_back (off (30000, c));
            m.push_back (ccm (12000, 64, 127)); m.push_back (ccm (40000, 64, 0));
            m.push_back (on (8000, 12 + (int) (rnd() * 10), 100)); m.push_back (off (8100, 12));
        }
        m.push_back (bend (20000, (int) (rnd() * 16383))); m.push_back (ccm (25000, 4, (int) (rnd() * 127))); m.push_back (ccm (26000, 1, 100)); m.push_back (bend (60000, 8192)); m.push_back (ccm (61000, 4, 0));
        std::sort (m.begin(), m.end(), [] (const MidiMsg& a, const MidiMsg& b) { return a.offset < b.offset; });
        const auto R = render (e, 9.0, m, 0);
        if (std::getenv ("THRUM_DEBUG_CASE") && std::atoi (std::getenv ("THRUM_DEBUG_CASE")) == k)
        {
            for (const auto& x : m) std::printf ("      in  %6d  %02x %3d %3d\n", x.offset, x.status, x.d1, x.d2);
            for (const auto& x : R.midi) std::printf ("      out %6lld  %02x %3d %3d\n", (long long) x.t, x.status, x.d1, x.d2);
        }
        // Salida MIDI balanceada al final.
        { std::set<int> open; for (const auto& x : R.midi) { const int key = (x.status & 15) * 128 + x.d1; if ((x.status & 0xF0) == 0x90 && x.d2 > 0) open.insert (key); else open.erase (key); } stuck += (int) open.size(); }
        bool nan = false; double sum = 0; int n = 0;
        for (size_t i = 0; i < R.l.size(); ++i)
        {
            const float v = R.l[i];
            if (! isFiniteF (v) || ! isFiniteF (R.r[i]) || ! isFiniteF (R.di[i])) nan = true;
            if (v != 0.0f && std::fabs (v) < 1.17549435e-38f) ++denorm;
            sum += v; ++n;
        }
        bad += nan;
        // Continua: la media de los 9 s (la del final en silencio incluida: lo que no es continua se promedia a cero).
        const double dc = db (std::fabs (sum / std::max (1, n)));
        worstDc = std::max (worstDc, dc); dcBad += dc > -45.0;
        if (dc > -45.0)
        {
            std::string on; for (int u = 0; u < kNumPedals; ++u) if (p.pedal[(size_t) u].on) on += std::string (unitKey (u)) + " ";
            std::printf ("    caso %d: continua %.1f dB, modo %d, amp %s %d gain %.2f, cab %d, pedales %s\n", k, dc, p.perf.mode, p.ampOn ? "on" : "off", p.amp.model, p.amp.gain, p.cabOn, on.c_str());
        }
        double tail = 0; for (size_t i = R.l.size() - 4800; i < R.l.size(); ++i) tail = std::max (tail, (double) std::fabs (R.l[i]));
        worstTail = std::max (worstTail, db (tail)); tailBad += db (tail) > -70.0;
        if (db (tail) > -70.0)
        {
            std::string on; for (int u = 0; u < kNumPedals; ++u) if (p.pedal[(size_t) u].on) on += std::string (unitKey (u)) + " ";
            double diTail = 0; for (size_t i = R.di.size() - 4800; i < R.di.size(); ++i) diTail = std::max (diTail, (double) std::fabs (R.di[i]));
            std::printf ("    caso %d: cola %.1f dB (DI %.1f dB), modo %d, art %d, fb %.2f, amp %d gain %.2f, pedales %s\n", k, db (tail), db (diTail), p.perf.mode, p.perf.art, p.perf.feedback, p.amp.model, p.amp.gain, on.c_str());
            for (int s = 0; s < e.instrument().strings(); ++s)
            {
                const auto v = e.performer().view (s);
                const auto& S = e.instrument().string (s);
                std::printf ("      cuerda %d: nota %d, traste %.1f, suena %d, apretada %d, env %.2e, hz %.1f\n", s, v.note, v.fret, (int) S.sounding(), (int) S.gated(), S.envelope(), S.pitchHz());
            }
            for (int u = 0; u < kNumPedals; ++u)
            {
                if (! p.pedal[(size_t) u].on) continue;
                auto q = p; q.pedal[(size_t) u].on = false;
                Engine e2; e2.setSyncCab (true); e2.prepare (kFs, 512); e2.set (q); e2.reset();
                const auto R2 = render (e2, 9.0, m, 0);
                double t2 = 0; for (size_t i = R2.l.size() - 4800; i < R2.l.size(); ++i) t2 = std::max (t2, (double) std::fabs (R2.l[i]));
                const auto& pp = p.pedal[(size_t) u];
                std::printf ("      sin %s: %.1f dB   (type %d p %.2f %.2f %.2f %.2f %.2f %.2f)\n", unitKey (u), db (t2), pp.type, pp.p[0], pp.p[1], pp.p[2], pp.p[3], pp.p[4], pp.p[5]);
            }
        }
    }
    std::printf ("    160 ajustes al azar (la mitad con teclas repetidas, notas de 0,6 ms, acordes, pedal y keyswitches): %d con NaN/Inf, continua peor %.1f dB, %d muestras subnormales, cola peor %.1f dB, %d notas de salida colgadas\n", bad, worstDc, denorm, worstTail, stuck);
    check (bad == 0, "sin NaN ni Inf");
    check (dcBad == 0, "continua por debajo de −45 dBFS");
    check (denorm == 0, "sin muestras subnormales en la salida");
    check (tailBad == 0, "a los 9 s, después de soltar todo, la salida cae por debajo de −70 dBFS");
    check (stuck == 0, "ninguna nota de salida queda sin su note-off");
}

// ── T8: latencia declarada igual a la medida ──
static void t8()
{
    std::printf ("T8 latencia declarada contra la medida\n");
    bool ok = true;
    for (int q : { 2, 4, 8 })
    {
        // El sobremuestreo solo: un impulso de ida y vuelta.
        Oversampler os; os.prepare (q, 256);
        std::vector<float> x (256, 0.0f), y (256, 0.0f); x[10] = 1.0f;
        float* up = os.upsample (x.data(), 256); os.downsample (up, y.data(), 256);
        int pk = 0; for (int i = 0; i < 256; ++i) if (std::fabs (y[(size_t) i]) > std::fabs (y[(size_t) pk])) pk = i;
        // El motor con el amplificador apagado (se reemplaza por un retardo igual): la procesada es la DI, muestra a muestra.
        Engine e; e.setSyncCab (true); e.prepare (kFs, 512);
        auto p = dryParams(); p.ampOn = false; p.amp.quality = q;
        e.set (p); e.reset();
        const auto R = render (e, 0.6, { on (100, 64, 60) }, 512);
        double diff = 0; for (size_t i = 0; i < R.l.size(); ++i) diff = std::max (diff, (double) std::fabs (R.l[i] - R.di[i]));
        // Con el amplificador limpio: el desfase de la correlación es sólo la fase de sus filtros (informativo).
        Engine e2; e2.setSyncCab (true); e2.prepare (kFs, 512);
        p.ampOn = true; p.amp.model = kAmpClean; p.amp.gain = 0.05f; p.amp.master = 0.2f;
        e2.set (p); e2.reset();
        const auto R2 = render (e2, 1.0, { on (100, 64, 60) }, 512);
        int best = 0; double bc = -1;
        for (int lag = -80; lag <= 80; ++lag)
        {
            double c = 0, ea = 0, eb = 0;
            for (int i = 2000; i < 30000; ++i) { const double a = R2.di[(size_t) i], b = R2.l[(size_t) (i + lag)]; c += a * b; ea += a * a; eb += b * b; }
            c /= std::sqrt (ea * eb + 1e-30);
            if (std::fabs (c) > bc) { bc = std::fabs (c); best = lag; }
        }
        std::printf ("    %dx: declarada %d, impulso del sobremuestreo en %d; amplificador apagado: procesada − DI = %.1e; limpio: correlación máxima en %+d muestras (%.2f)\n",
                     q, e.latency(), pk - 10, diff, best, bc);
        ok &= pk - 10 == e.latency() && diff < 1e-6 && std::abs (best) <= 6;
    }
    check (ok, "la latencia declarada es la medida (impulso), constante con el amplificador apagado, y la DI sale alineada");
}

// ── T9: articulaciones ──
static double decayTime (const std::vector<float>& y, double from)
{
    // Tiempo hasta caer 30 dB desde el pico de la envolvente (ventanas de 10 ms).
    const int w = 480; double pk = 0; size_t pkAt = (size_t) (from * kFs);
    std::vector<double> env;
    for (size_t i = (size_t) (from * kFs); i + w < y.size(); i += w) { double e = 0; for (int k = 0; k < w; ++k) e = std::max (e, (double) std::fabs (y[i + (size_t) k])); env.push_back (e); }
    size_t ip = 0; for (size_t i = 0; i < env.size(); ++i) if (env[i] > pk) { pk = env[i]; ip = i; }
    (void) pkAt;
    for (size_t i = ip; i < env.size(); ++i) if (env[i] < pk * 0.0316) return (double) (i - ip) * w / kFs;
    return (double) env.size() * w / kFs;
}
static void t9()
{
    std::printf ("T9 articulaciones\n");
    auto run = [] (EngineParams p, std::vector<MidiMsg> m, double secs) { Engine e; e.setSyncCab (true); e.prepare (kFs, 512); e.set (p); e.reset(); auto R = render (e, secs, m, 256); return std::make_pair (R, e.performer().view (0)); };
    // Palm mute: más corto.
    auto p = dryParams();
    const auto rs = run (p, { on (100, 45, 100) }, 3.0).first;
    p.perf.art = kArtPalm; p.perf.palm = 0.8f;
    const auto rp = run (p, { on (100, 45, 100) }, 3.0).first;
    const double ts = decayTime (rs.di, 0.0), tp = decayTime (rp.di, 0.0);
    std::printf ("    caída de 30 dB: sostenida %.2f s, palm mute %.2f s\n", ts, tp);
    check (tp < ts * 0.25, "el palm mute acorta la nota a menos de un cuarto");
    // Armónico en el traste 12 de la 6.ª: la octava domina y la fundamental cae más de 20 dB.
    p = dryParams(); p.perf.art = kArtHarmonic;
    const auto rh = run (p, { on (100, 52, 100) }, 2.0).first;                 // E3 = armónico de la 6.ª al aire (k = 2)
    double l1, l2; const Spectrum sh (rh.di, (size_t) (0.1 * kFs), 1 << 15, kFs);
    sh.peak (78, 87, &l1); sh.peak (160, 170, &l2);
    std::printf ("    armónico (6.ª, traste 12): 82 Hz %.1f dB, 165 Hz %.1f dB\n", db (l1), db (l2));
    check (db (l2) - db (l1) > 20.0, "el armónico natural deja la octava y saca la fundamental (> 20 dB)");
    // Bend de la rueda: +2 semitonos.
    p = dryParams(); p.perf.bendRange = 2.0f;
    {
        Engine e; e.setSyncCab (true); e.prepare (kFs, 512); e.set (p); e.reset();
        render (e, 1.2, { on (100, 64, 90), bend (12000, 16383) }, 256);
        int s = -1; for (int k = 0; k < 6; ++k) if (e.performer().view (k).note >= 0) s = k;
        const double c = s >= 0 ? 1200.0 * std::log2 (e.instrument().string (s).pitchHz() / midiHz (64)) : 0;
        std::printf ("    bend de la rueda al máximo (rango 2): %+.1f cents\n", c);
        check (std::fabs (c - 200.0) < 6.0, "el bend llega a +200 cents (±6, con el glissando de tensión)");
    }
    // Ligado: un hammer-on no vuelve a picar (la salida MIDI cambia de nota, la cuerda es la misma).
    p = dryParams(); p.perf.mode = kModeMono; p.perf.legato = kLegatoHammer; p.perf.midiOut = true; p.perf.midiOutPerString = true;
    {
        Engine e; e.setSyncCab (true); e.prepare (kFs, 512); e.set (p); e.reset();
        const auto R = render (e, 1.0, { on (100, 57, 100), on (12000, 59, 100), off (12100, 57), off (40000, 59) }, 256);
        std::vector<std::pair<int, int>> ons; for (const auto& x : R.midi) if ((x.status & 0xF0) == 0x90) ons.push_back ({ x.status & 15, x.d1 });
        const bool same = ons.size() == 2 && ons[0].first == ons[1].first;
        std::printf ("    hammer-on A3→B3: %zu notas de salida, %s\n", ons.size(), same ? "en la misma cuerda" : "en cuerdas distintas");
        check (same, "el hammer-on queda en la misma cuerda");
    }
    // Una nota nueva en la misma cuerda corta la anterior.
    p = dryParams(); p.perf.prefs.preferredString = 3;
    {
        const auto R = run (p, { on (100, 57, 110), on (24000, 60, 110) }, 1.5).first;          // A3 y C4 en la 3.ª
        const Spectrum before (R.di, 12000, 8192, kFs), after (R.di, 24000 + 2400, 8192, kFs);
        double a1, a2; before.peak (215, 225, &a1); after.peak (215, 225, &a2);
        std::printf ("    A3 en la cuerda 3: antes %.1f dB, 50 ms después de picar C4 en la misma cuerda %.1f dB\n", db (a1), db (a2));
        check (db (a1) - db (a2) > 18.0, "la nota anterior cae más de 18 dB al picar la nueva en la misma cuerda");
    }
    // Pastillas: la del puente es más brillante que la del mástil.
    p = dryParams(); p.ins.selector = 0;
    const auto rb = run (p, { on (100, 52, 100) }, 1.0).first;
    p.ins.selector = 4;
    const auto rn = run (p, { on (100, 52, 100) }, 1.0).first;
    const Spectrum sb (rb.di, 2400, 1 << 14, kFs), sn (rn.di, 2400, 1 << 14, kFs);
    const double cb = db (sb.band (2000, 6000) / sb.band (150, 180)), cn = db (sn.band (2000, 6000) / sn.band (150, 180));
    std::printf ("    2-6 kHz contra la fundamental: puente %.1f dB, mástil %.1f dB\n", cb, cn);
    check (cb - cn > 6.0, "la pastilla del puente tiene menos fundamental y más agudos que la del mástil (> 6 dB)");
}

// ── T10: tonestack contra el análisis nodal del circuito ──
static std::complex<double> nodalFmv (const FmvParts& c, double t, double m, double l, double f)
{
    // Nodos: 0 Ntop, 1 Out, 2 N2, 3 N3 (arriba del pote de medios), 4 N1 (resistencia de pendiente), 5 Nw (cursor de medios).
    const std::complex<double> s (0.0, 2.0 * kPiD * f);
    std::complex<double> Y[6][6] {}, I[6] {};
    auto add = [&] (int a, int b, std::complex<double> y) {      // −1 = entrada, −2 = masa
        if (a >= 0) Y[a][a] += y; if (b >= 0) Y[b][b] += y;
        if (a >= 0 && b >= 0) { Y[a][b] -= y; Y[b][a] -= y; }
        if (a == -1 && b >= 0) I[b] += y; if (b == -1 && a >= 0) I[a] += y;
    };
    auto R = [] (double r) { return std::complex<double> (1.0 / std::max (r, 1e-3), 0.0); };
    add (-1, 0, s * c.C1); add (-1, 4, R (c.R4));
    add (0, 1, R ((1 - t) * c.R1)); add (1, 2, R (t * c.R1));
    add (4, 2, s * c.C2); add (4, 5, s * c.C3);
    add (5, -2, R (m * c.R3)); add (3, 5, R ((1 - m) * c.R3)); add (2, 3, R (l * c.R2));
    // Gauss.
    for (int k = 0; k < 6; ++k)
    {
        int pv = k; for (int r = k + 1; r < 6; ++r) if (std::abs (Y[r][k]) > std::abs (Y[pv][k])) pv = r;
        for (int j = 0; j < 6; ++j) std::swap (Y[k][j], Y[pv][j]); std::swap (I[k], I[pv]);
        for (int r = k + 1; r < 6; ++r) { const auto f2 = Y[r][k] / Y[k][k]; for (int j = k; j < 6; ++j) Y[r][j] -= f2 * Y[k][j]; I[r] -= f2 * I[k]; }
    }
    std::complex<double> v[6];
    for (int k = 5; k >= 0; --k) { auto acc = I[k]; for (int j = k + 1; j < 6; ++j) acc -= Y[k][j] * v[j]; v[k] = acc / Y[k][k]; }
    return v[1];
}
static void t10()
{
    std::printf ("T10 tonestack FMV: la transferencia contra el circuito resuelto por nodos\n");
    const FmvParts parts[] = { { 250e3, 1e6, 25e3, 56e3, 250e-12, 20e-9, 20e-9 }, { 220e3, 1e6, 22e3, 33e3, 470e-12, 22e-9, 22e-9 }, { 250e3, 250e3, 10e3, 100e3, 250e-12, 100e-9, 47e-9 } };
    double worst = 0;
    for (const auto& c : parts)
        for (double t : { 0.1, 0.5, 0.9 }) for (double m : { 0.1, 0.5, 0.9 }) for (double l : { 0.05, 0.5, 1.0 })
        {
            double B[4], A[4]; fmvCoefficients (c, t, m, l, B, A);
            for (double f : { 40.0, 150.0, 600.0, 2500.0, 9000.0 })
            {
                const std::complex<double> s (0.0, 2.0 * kPiD * f);
                const auto h = (B[1] * s + B[2] * s * s + B[3] * s * s * s) / (A[0] + A[1] * s + A[2] * s * s + A[3] * s * s * s);
                worst = std::max (worst, std::fabs (db (std::abs (h)) - db (std::abs (nodalFmv (c, t, m, l, f)))));
            }
        }
    std::printf ("    3 juegos de componentes × 27 posiciones × 5 frecuencias: diferencia máxima %.4f dB\n", worst);
    check (worst < 0.01, "la fórmula del tonestack coincide con el circuito (< 0,01 dB)");
}

// ── T11: gabinete ──
static void t11()
{
    std::printf ("T11 gabinete: posición del micrófono, proximidad, IR del usuario\n");
    CabParams c; c.mix = 0.0f;
    auto hf = [&] (CabParams cp) { const auto ir = designCabIr (cp, kFs, 2048); std::vector<float> x (ir.begin(), ir.end()); x.resize (8192, 0.0f); const Spectrum s (x, 0, 8192, kFs); return std::make_pair (s.band (4000, 8000), s.band (60, 150)); };
    c.a.position = 0.0f; const auto center = hf (c);
    c.a.position = 1.0f; const auto edge = hf (c);
    c.a.position = 0.3f; c.a.distanceCm = 1.0f; const auto close = hf (c);
    c.a.distanceCm = 40.0f; const auto far = hf (c);
    std::printf ("    4-8 kHz: centro %.1f dB, borde %.1f dB; 60-150 Hz: a 1 cm %.1f dB, a 40 cm %.1f dB\n", db (center.first), db (edge.first), db (close.second), db (far.second));
    check (db (center.first) - db (edge.first) > 6.0, "el micrófono en el centro del cono es más brillante que en el borde (> 6 dB)");
    check (db (close.second) - db (far.second) > 4.0, "de cerca hay efecto de proximidad (> 4 dB de graves)");
    // IR del usuario: una IR de 44,1 kHz se lleva a 48 kHz con su largo y su nivel normalizado.
    std::vector<float> u (2000); for (int i = 0; i < 2000; ++i) u[(size_t) i] = (float) (std::exp (-i / 200.0) * std::sin (i * 0.3));
    const auto r = resampleIr (u.data(), 2000, 44100.0, kFs, ConvKernel::maxLength (kFs));
    std::printf ("    IR del usuario 2000 muestras a 44,1 kHz → %zu a 48 kHz\n", r.size());
    check (std::abs ((int) r.size() - 2177) <= 2, "la IR del usuario se remuestrea con el largo correcto");
}

// ── T13: convolución particionada del gabinete ──
static void t13()
{
    std::printf ("T13 convolución particionada: igual a la directa, sin latencia, IR largas\n");
    // Una IR de sala de 2,5 s (ruido con caída exponencial y un comienzo de gabinete) a 48 kHz.
    const int n = (int) (2.5 * kFs);
    std::vector<float> h ((size_t) n);
    uint32_t s = 7;
    auto rnd = [&s] { s = s * 1664525u + 1013904223u; return (float) ((s >> 8) * (1.0 / 16777216.0)) * 2.0f - 1.0f; };
    for (int i = 0; i < n; ++i) h[(size_t) i] = rnd() * std::exp (-6.9f * (float) i / (float) n) * (i < 300 ? 3.0f : 0.3f);
    ConvKernel K; RealFft f; std::vector<float> sc;
    K.build (h.data(), n, kFs, f, sc);
    PartConv C; C.prepare (kFs); C.setKernel (&K); C.reset();
    // Entrada: un impulso y después ruido; referencia directa en double.
    const int len = (int) (3.0 * kFs);
    std::vector<float> x ((size_t) len, 0.0f), y ((size_t) len);
    x[0] = 1.0f; for (int i = 1000; i < len; ++i) x[(size_t) i] = 0.25f * rnd();
    for (int i = 0; i < len; ++i) y[(size_t) i] = C.process (x[(size_t) i]);
    double worst = 0, ref = 0;
    for (int i : { 0, 1, 255, 256, 257, 511, 512, 4000, 30000, 77777, 119999, 120000, 143000 })
    {
        double acc = 0; for (int k = 0; k <= i && k < n; ++k) acc += (double) h[(size_t) k] * x[(size_t) (i - k)];
        worst = std::max (worst, std::fabs (acc - (double) y[(size_t) i])); ref = std::max (ref, std::fabs (acc));
    }
    std::printf ("    IR de %d muestras (%.1f s): %d particiones de %d; error máximo %.2e (salida hasta %.2f)\n", n, n / kFs, K.parts, ConvKernel::kB, worst, ref);
    check (worst < 1e-4 * std::max (1.0, ref), "la convolución particionada da lo mismo que la directa (error < 1e-4 del pico)");
    check (std::fabs (y[0] - h[0]) < 1e-6f && std::fabs (y[300] - h[300]) < 1e-5f, "sin latencia: el impulso sale con la IR desde la muestra 0");
    // El motor: una IR de usuario de 2 s con el gabinete solo (sin amplificador).
    Engine e; e.setSyncCab (true); e.prepare (kFs, 512);
    auto p = dryParams(); p.cabOn = true; p.cabSource = 2; e.set (p); e.reset();
    e.loadIr (h.data(), (int) (2.0 * kFs), true);
    std::printf ("    largo máximo de una IR: %.1f s (%d muestras a 48 kHz)\n", kMaxIrSeconds, ConvKernel::maxLength (kFs));
    check (ConvKernel::maxLength (kFs) >= (int) (2.0 * kFs), "acepta IR de al menos 2 s");
    // CPU: 3 s de IR, un canal, bloques de 512.
    std::vector<float> h3 ((size_t) ConvKernel::maxLength (kFs)); for (auto& v : h3) v = 0.01f * rnd();
    ConvKernel K3; K3.build (h3.data(), (int) h3.size(), kFs, f, sc);
    PartConv C3; C3.prepare (kFs); C3.setKernel (&K3); C3.reset();
    const int secs = 20; float sink = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < secs * (int) kFs; ++i) sink += C3.process (0.1f * rnd());
    const double el = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    std::printf ("    IR de 3 s, un canal: %.2f %% de un núcleo (%d particiones)%s\n", 100.0 * el / secs, K3.parts, sink == 12345.0f ? " " : "");
    check (el / secs < 0.05, "una IR de 3 s cuesta menos del 5 % de un núcleo");
}

// ── T14: Auto level del amplificador ──
static void t14()
{
    std::printf ("T14 Auto level: Gain cambia el carácter (encendido) o también el volumen (apagado)\n");
    Engine e; e.setSyncCab (true); e.prepare (kFs, 512);
    auto p = dryParams(); p.perf.mode = kModeStrum; e.set (p); e.reset();
    e.performer().setStrumPattern (parseStrum ("D.DU.UDU", "20001010", kDiv8, 0.5f));
    const auto R = render (e, 4.0, { on (0, 40), on (0, 47), on (0, 52), on (0, 56) }, 512, true, 100.0);
    auto level = [&] (int model, float gain, bool autoLvl) {
        Amp a; AmpParams ap; ap.model = model; ap.gain = gain; ap.master = 0.5f; ap.quality = 4; ap.autoLevel = autoLvl;
        a.prepare ((float) kFs, 512); a.set (ap); a.reset();
        auto x = R.di;
        for (size_t pos = 0; pos < x.size(); pos += 512) a.process (x.data() + pos, (int) std::min<size_t> (512, x.size() - pos));
        double r = 0; const size_t a0 = x.size() / 6; for (size_t i = a0; i < x.size(); ++i) r += (double) x[i] * x[i];
        return db (std::sqrt (r / (double) (x.size() - a0)));
    };
    bool okAuto = true, okRaw = true, okMid = true;
    for (int m = 0; m < kNumAmps; ++m)
    {
        // (Con Auto level la compensación tiene un techo de +30 dB: el limpio con Gain 1 queda más bajo a propósito.)
        const double a1 = level (m, 0.3f, true), a9 = level (m, 0.9f, true), r1 = level (m, 0.1f, false), r9 = level (m, 0.9f, false);
        const double a5 = level (m, 0.5f, true), r5 = level (m, 0.5f, false);
        std::printf ("    %-17s Gain 3→9 con Auto level %+5.1f dB; Gain 1→9 sin %+5.1f dB; Gain 5: %+.2f dB de diferencia\n", ampName (m), a9 - a1, r9 - r1, r5 - a5);
        okAuto &= std::fabs (a9 - a1) < 6.0;
        okRaw &= r9 - r1 > 8.0;
        okMid &= std::fabs (r5 - a5) < 0.1;
    }
    check (okAuto, "con Auto level, Gain de 3 a 9 cambia el volumen menos de 6 dB");
    check (okRaw, "sin Auto level, Gain de 1 a 9 sube el volumen más de 8 dB en los cuatro amplificadores");
    check (okMid, "en Gain 5 los dos modos suenan igual de fuerte");
}

// ── T12: CPU ──
static void t12()
{
    std::printf ("T12 CPU (un núcleo, 48 kHz, bloques de 512)\n");
    struct Case { const char* name; std::function<void (EngineParams&)> f; int mode; };
    const Case cases[] = {
        { "guitarra sola, melodía", [] (EngineParams& p) { p.ampOn = false; p.cabOn = false; }, kModePoly },
        { "rasgueo, crunch 4x, gabinete", [] (EngineParams& p) { p.amp.model = kAmpCrunch; p.amp.gain = 0.6f; }, kModeStrum },
        { "rasgueo, cadena completa 4x", [] (EngineParams& p) { p = fullParams(); }, kModeStrum },
        { "alta ganancia 8x, delay y sala", [] (EngineParams& p) { p = fullParams(); p.amp.model = kAmpHigh; p.amp.quality = 8; }, kModeStrum },
    };
    for (const auto& c : cases)
    {
        Engine e; e.setSyncCab (true); e.prepare (kFs, 512);
        EngineParams p; c.f (p); p.perf.mode = c.mode; p.ins.sympathetic = 0.5f;
        e.set (p); e.reset();
        std::vector<MidiMsg> m;
        if (c.mode == kModeStrum) { m = { on (100, 48), on (100, 52), on (100, 55) }; }
        else for (int i = 0; i < 40; ++i) { m.push_back (on (100 + i * 12000, 52 + (i * 7) % 15, 90)); m.push_back (off (100 + i * 12000 + 11000, 52 + (i * 7) % 15)); }
        const auto t0 = std::chrono::steady_clock::now();
        render (e, 10.0, m, 512, true, 120.0);
        const double sec = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        std::printf ("    %-34s %6.2f %% de un núcleo\n", c.name, sec / 10.0 * 100.0);
    }
    check (true, "medido");
}


// ── T15: loops del host (QA en REAPER 2026-10-22, G1) ──
// Acorde sostenido, loop de 0 a 2 s a 120 BPM. En cada vuelta REAPER manda los note-off del acorde al final del loop y
// los note-on otra vez al principio. Cada pasada tiene que tocar los mismos golpes en los mismos lugares (antes la
// segunda arrancaba en 0,44 s: faltaban los golpes de 0,02 y 0,19 s). split: el host corta el bloque en el borde del
// loop; si no, el bloque que cruza el borde informa la posición de su comienzo y el siguiente ya está en la vuelta nueva.
static std::vector<std::vector<int64_t>> loopPasses (int block, bool split, bool centred, bool loopInfo = true)
{
    Engine e; e.setSyncCab (true); e.prepare (kFs, 4096);
    auto p = dryParams(); p.perf.mode = kModeStrum; p.perf.midiOut = true; p.perf.midiOutPerString = false;
    p.perf.strumCentred = centred; p.perf.humanTime = 0.0f;
    e.set (p); e.reset();
    e.performer().setStrumPattern (parseStrum ("D.DU.UDU", "20001010", kDiv8, 0.5f));
    const int64_t loop = (int64_t) (2.0 * kFs); const int passes = 4;
    const double bpm = 120.0, spb = kFs * 60.0 / bpm;
    std::vector<MidiOutEv> out; std::vector<MidiMsg> blk;
    std::vector<std::vector<int64_t>> per ((size_t) passes);
    std::vector<float> L (4096), R (4096), D (4096);
    for (int64_t abs = 0; abs < passes * loop;)
    {
        int b = (int) std::min<int64_t> (block, passes * loop - abs);
        if (split) b = (int) std::min<int64_t> (b, loop - abs % loop);          // el host corta en el borde del loop
        blk.clear();
        // Cada vuelta: note-off del acorde en la última muestra y note-on otra vez en la primera (como REAPER).
        for (int64_t k = abs / loop; k <= (abs + b) / loop; ++k)
        {
            const int64_t offAt = k * loop - 1, onAt = k * loop;
            if (k > 0 && offAt >= abs && offAt < abs + b) for (int n : { 48, 52, 55 }) blk.push_back (off ((int) (offAt - abs), n));
            if (onAt >= abs && onAt < abs + b) for (int n : { 48, 52, 55 }) blk.push_back (on ((int) (onAt - abs), n));
        }
        std::stable_sort (blk.begin(), blk.end(), [] (const MidiMsg& x, const MidiMsg& y) { return x.offset < y.offset; });
        HostTime h; h.playing = true; h.bpm = bpm; h.ppq = (double) (abs % loop) / spb;
        if (loopInfo) { h.looping = true; h.loopStart = 0.0; h.loopEnd = (double) loop / spb; }
        out.clear();
        e.process (L.data(), R.data(), D.data(), b, h, blk.data(), (int) blk.size(), &out);
        for (auto& m : out) if ((m.status & 0xF0) == 0x90 && m.d2 > 0) { const int64_t t = abs + m.t; per[(size_t) (t / loop)].push_back (t % loop); }
        abs += b;
    }
    for (auto& v : per) { std::sort (v.begin(), v.end()); v.erase (std::unique (v.begin(), v.end()), v.end()); }
    return per;
}
static void t15()
{
    std::printf ("T15 loop del host: cada pasada rasguea lo mismo (G1)\n");
    bool all = true;
    for (bool centred : { true, false })
        for (int blk : { 64, 333, 512, 1024 })
            for (bool split : { true, false })
            {
                const auto per = loopPasses (blk, split, centred);
                bool same = ! per[0].empty();
                for (size_t k = 1; k < per.size(); ++k) same &= per[k] == per[0];
                if (! same || blk == 512)
                {
                    std::printf ("    %s, bloque %4d, %s: golpes por pasada", centred ? "centrado" : "desde el tiempo", blk, split ? "corta en el borde" : "cruza el borde");
                    for (auto& v : per) std::printf (" %zu", v.size());
                    if (! same)
                    {
                        // Los golpes que difieren entre la primera y la segunda pasada.
                        std::printf (" (sólo en la 1.ª:");
                        for (auto t : per[0]) if (! std::binary_search (per[1].begin(), per[1].end(), t)) std::printf (" %.4f", t / kFs);
                        std::printf ("; sólo en la 2.ª:");
                        for (auto t : per[1]) if (! std::binary_search (per[0].begin(), per[0].end(), t)) std::printf (" %.4f", t / kFs);
                        std::printf (")");
                    }
                    std::printf ("\n");
                }
                all &= same;
            }
    check (all, "cada vuelta del loop toca las mismas notas en las mismas muestras, con bloques de 64 a 1024, cortados o no en el borde");
    // Un host que no informa el loop: la posición salta atrás. Ninguna vuelta pierde golpes de la primera.
    bool none = true;
    for (int blk : { 64, 512, 1024 })
        for (bool split : { true, false })
        {
            const auto per = loopPasses (blk, split, true, false);
            int missing = 0;
            for (size_t k = 1; k < per.size(); ++k)
                for (auto t : per[0])
                {
                    bool near = false;
                    for (auto u : per[k]) if (std::llabs (u - t) <= (int64_t) (0.03 * kFs)) { near = true; break; }
                    missing += ! near;
                }
            if (missing > 0 || blk == 512) std::printf ("    sin informar el loop, bloque %4d, %s: %d golpes de la 1.ª pasada faltan después\n", blk, split ? "corta en el borde" : "cruza el borde", missing);
            none &= missing == 0;
        }
    check (none, "sin la información del loop (sólo el salto atrás) ninguna vuelta pierde golpes");
}

int main (int argc, char** argv)
{
#if defined(__aarch64__)
    { uint64_t fpcr; __asm__ volatile ("mrs %0, fpcr" : "=r" (fpcr)); fpcr |= (1ull << 24); __asm__ volatile ("msr fpcr, %0" :: "r" (fpcr)); }   // FZ, como ScopedNoDenormals
#elif defined(__SSE__) || defined(_M_X64) || defined(_M_IX86)
    _mm_setcsr (_mm_getcsr() | 0x8040);
#endif
    std::set<int> only; bool tuning = false, ampLevels = false;
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp (argv[i], "--tuning") == 0) tuning = true;
        else if (std::strcmp (argv[i], "--amp-levels") == 0) ampLevels = true;
        else only.insert (std::atoi (argv[i]));
    }
    auto want = [&] (int k) { return only.empty() || only.count (k) > 0; };
    if (tuning) { t1 (true); return fails ? 1 : 0; }
    if (ampLevels)
    {
        // La tabla de Amp::autoLevelDb: un rasgueo de referencia (DI) por cada modelo con Gain 0..10, master 5.
        Engine e; e.setSyncCab (true); e.prepare (kFs, 512);
        auto p = dryParams(); p.perf.mode = kModeStrum; e.set (p); e.reset();
        e.performer().setStrumPattern (parseStrum ("D.DU.UDU", "20001010", kDiv8, 0.5f));
        const auto R = render (e, 6.0, { on (0, 40), on (0, 47), on (0, 52), on (0, 56) }, 512, true, 100.0);
        double di = 0; for (float v : R.di) di += (double) v * v; di = std::sqrt (di / (double) R.di.size());
        std::printf ("DI de referencia: %.1f dB RMS\n", db (di));
        Amp::rawLevel = true;
        for (int model = 0; model < kNumAmps; ++model)
        {
            std::printf ("            { ");
            for (int g = 0; g <= 10; ++g)
            {
                Amp a; AmpParams ap; ap.model = model; ap.gain = (float) g / 10.0f; ap.master = 0.5f; ap.quality = 4;
                a.prepare ((float) kFs, 512); a.set (ap); a.reset();
                auto x = R.di;
                for (size_t pos = 0; pos < x.size(); pos += 512) a.process (x.data() + pos, (int) std::min<size_t> (512, x.size() - pos));
                double r = 0; const size_t a0 = x.size() / 6; for (size_t i = a0; i < x.size(); ++i) r += (double) x[i] * x[i];
                std::printf ("%.1ff%s", db (std::sqrt (r / (double) (x.size() - a0))), g < 10 ? ", " : " },");
            }
            std::printf ("   // %s\n", ampName (model));
        }
        return 0;
    }
    if (want (1)) t1 (false);
    if (want (2)) t2();
    if (want (3)) t3();
    if (want (4)) t4();
    if (want (5)) t5();
    if (want (6)) t6();
    if (want (7)) t7();
    if (want (8)) t8();
    if (want (9)) t9();
    if (want (10)) t10();
    if (want (11)) t11();
    if (want (13)) t13();
    if (want (14)) t14();
    if (want (15)) t15();
    if (want (12)) t12();
    std::printf ("\n%s: %d falla(s)\n", fails ? "FALLA" : "ok", fails);
    return fails ? 1 : 0;
}
