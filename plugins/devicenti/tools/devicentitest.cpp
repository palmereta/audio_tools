// devicentitest: numbered tests of the DeviceNTI V engine, without JUCE (plan §7 Phase 1).
//
//   devicentitest            all tests
//   devicentitest T3 T7      only some
//
// T1  tuning per ratio and in fixed mode               T8  step conditions (A:B, PRE/NEI chains, FILL, 1ST, LAST, BAR, %)
// T2  aliasing at 2x (sine floor, feedback saw, FM 1:3.5, drive+fold)   T9  chord mode and chord memory
// T3  clicks: steal, choke, legato, kill (|d²| vs sine)  T10 MIDI in: channels, OMNI, KIT map, bend, sustain, CC
// T4  determinism with any block size                    T11 MIDI out: MIDI machine, doubling, locks as CC
// T5  reset and re-prepare give the same output          T12 CPU (typical project, 16 voices LP24 AGE 1)
// T6  no NaN, no DC, no denormal slowdown (random sounds) T13 latency and sub alignment
// T7  step timing against the host clock, block edges (T2b), micro timing, retrig, swing
//
// Exit code 1 if any test fails.
#include "Engine.h"
#include <chrono>
#include <complex>
#include <cstdarg>
#include <cstdlib>
#include <set>
#include <cstdio>
#include <functional>
#include <map>
#include <string>
#include <vector>

using namespace dh::dv;

static int fails = 0;
static void report (const char* t, const char* what, bool ok, const char* fmt = "", ...)
{
    char buf[512]; va_list ap; va_start (ap, fmt); std::vsnprintf (buf, sizeof (buf), fmt, ap); va_end (ap);
    std::printf ("  %-4s %-58s %s  %s\n", t, what, ok ? "PASS" : "FAIL", buf);
    if (! ok) ++fails;
}

// ── helpers ──
static constexpr double SR = 48000.0;

struct Rig
{
    std::unique_ptr<Engine> e = std::make_unique<Engine>();
    EngineParams P;
    std::vector<float> L, R;
    std::vector<MidiOut> midi;
    int64_t pos = 0;
    double bpm = 120.0;
    bool playing = false;
    explicit Rig (int block = 512) { e->prepare (SR, block); for (int t = 0; t < kTracks; ++t) P.t[(size_t) t].levelDb = 0.0f; e->project().midi.focus = 0; P.swing = 0.5f; }   // straight unless a test swings
    Project& pr() { return e->project(); }
    // Copy the track sound values that are host parameters into P (so automation does not overwrite them).
    void syncAuto()
    {
        for (int t = 0; t < kTracks; ++t)
        {
            const Sound& s = pr().sounds[(size_t) t][0]; auto& a = P.t[(size_t) t];
            a.cutoff = s[CUTOFF]; a.reso = s[RESO]; a.fenv = s[FENV]; a.drive = s[DRIVE]; a.age = s[AGE]; a.dly = s[SEND_DELAY]; a.rev = s[SEND_REVERB]; a.ins = s[INS_AMT];
            for (int o = 0; o < 4; ++o) a.op[o] = s[opParam (o, oLevel)];
        }
    }
    // Render n samples in blocks; midiIn events at absolute sample positions.
    void run (int n, int block, std::vector<std::pair<int64_t, MidiIn>> ev = {}, std::function<int (int)> blockFn = nullptr)
    {
        std::vector<float> l (8192), r (8192);
        std::vector<float> tr[8]; for (auto& v : tr) v.assign (8192, 0.0f);
        size_t ei = 0; int done = 0, bi = 0;
        while (done < n)
        {
            int b = blockFn ? blockFn (bi++) : block;
            b = std::min ({ b, n - done, 8192 });
            std::fill (l.begin(), l.begin() + b, 0.0f); std::fill (r.begin(), r.begin() + b, 0.0f);
            float* outs[10] = { l.data(), r.data(), nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };
            std::vector<MidiIn> mi;
            while (ei < ev.size() && ev[ei].first < pos + b) { MidiIn m = ev[ei].second; m.offset = (int) std::max<int64_t> (0, ev[ei].first - pos); mi.push_back (m); ++ei; }
            HostTime ht; ht.playing = playing; ht.bpm = bpm; ht.ppq = (double) pos / SR * bpm / 60.0;
            std::vector<MidiOut> mo;
            e->process (outs, b, ht, P, mi.data(), (int) mi.size(), &mo);
            for (auto& m : mo) { m.offset += (int) 0; midi.push_back ({ (int) (pos + m.offset), { m.d[0], m.d[1], m.d[2] }, 3 }); }
            L.insert (L.end(), l.begin(), l.begin() + b); R.insert (R.end(), r.begin(), r.begin() + b);
            pos += b; done += b;
        }
    }
};

static MidiIn mm (uint8_t a, uint8_t b, uint8_t c) { MidiIn m; m.d[0] = a; m.d[1] = b; m.d[2] = c; return m; }

static Sound sineSound (float att = 0.005f, float rel = 0.05f)
{
    Sound s; s[ALGO] = 7; s[A_LEVEL] = 0.5f; s[A_DEC] = 1.0f; s[A_SUS] = 1.0f;
    s[FTYPE] = kFilterOff; s[AMP_ATT] = att; s[AMP_REL] = rel; s[AMP_DEC] = 1.0f; s[AMP_SUS] = 1.0f;
    s[opParam (0, oAtt)] = 0.0005f; s[opParam (0, oRel)] = 5.0f;
    s[LOWMONO] = 0; s[VEL_AMP] = 0.0f; s[opParam (0, oVel)] = 0.0f;
    return s;
}

// Frequency by interpolated upward zero crossings.
static double measureFreq (const std::vector<float>& x, int a, int b)
{
    double first = -1, last = -1; int count = 0;
    for (int i = a + 1; i < b; ++i)
        if (x[(size_t) i - 1] < 0.0f && x[(size_t) i] >= 0.0f)
        {
            const double t = (i - 1) + x[(size_t) i - 1] / (x[(size_t) i - 1] - x[(size_t) i]);
            if (first < 0) first = t; else ++count;
            last = t;
        }
    return count > 0 ? count / ((last - first) / SR) : 0.0;
}

static void fft (std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) { size_t bit = n >> 1; for (; j & bit; bit >>= 1) j ^= bit; j ^= bit; if (i < j) std::swap (a[i], a[j]); }
    for (size_t len = 2; len <= n; len <<= 1)
    {
        const double ang = -2.0 * 3.14159265358979323846 / (double) len;
        const std::complex<double> wl (std::cos (ang), std::sin (ang));
        for (size_t i = 0; i < n; i += len)
        {
            std::complex<double> w (1);
            for (size_t j = 0; j < len / 2; ++j) { auto u = a[i + j], v = a[i + j + len / 2] * w; a[i + j] = u + v; a[i + j + len / 2] = u - v; w *= wl; }
        }
    }
}

// Energy away from multiples of f0/2 below 20 kHz, relative to the total (dB).
static double aliasRatio (const std::vector<float>& x, int start, double f0)
{
    const int N = 16384;
    std::vector<std::complex<double>> a ((size_t) N);
    for (int i = 0; i < N; ++i)
    {
        const double w = 0.35875 - 0.48829 * std::cos (2 * M_PI * i / (N - 1)) + 0.14128 * std::cos (4 * M_PI * i / (N - 1)) - 0.01168 * std::cos (6 * M_PI * i / (N - 1));
        a[(size_t) i] = x[(size_t) (start + i)] * w;
    }
    fft (a);
    double tot = 0, bad = 0;
    const double binHz = SR / N;
    for (int k = 1; k < N / 2; ++k)
    {
        const double f = k * binHz;
        if (f > 20000) break;
        const double p = std::norm (a[(size_t) k]);
        tot += p;
        const double g = f0 * 0.5, m = std::round (f / g);
        if (std::fabs (f - m * g) > 5 * binHz) { bad += p; if (std::getenv ("DV_DUMP") && p > tot * 1e-7) std::printf ("         bad bin %.0f Hz  %.1f dB\n", f, 10 * std::log10 (p)); }
    }
    return 10 * std::log10 (bad / std::max (tot, 1e-30) + 1e-20);
}

static void setStep (Project& pr, int pat, int t, int lane, int idx, int note, int vel = 100, int len = 12)
{
    Step& s = pr.bank.p[(size_t) pat].t[(size_t) t].lane[(size_t) lane][(size_t) idx];
    s.trig = 1; s.nNotes = note >= 0 ? 1 : 0; s.notes[0] = (int8_t) std::max (0, note); s.vel = (uint8_t) vel; s.len = (uint16_t) len;
}

static int countNoteOns (const std::vector<MidiOut>& m, int ch = -1, int note = -1)
{
    int c = 0;
    for (auto& e : m) if ((e.d[0] & 0xF0) == 0x90 && e.d[2] > 0 && (ch < 0 || (e.d[0] & 15) == ch - 1) && (note < 0 || e.d[1] == note)) ++c;
    return c;
}

// ───────────────────────── tests ─────────────────────────
static void T1()
{
    const float ratios[] = { 0.5f, 1.0f, 2.0f, 3.0f, 1.41f, 3.5f, 7.07f, 16.0f };
    double worst = 0;
    for (float r : ratios)
    {
        Rig g; g.pr().sounds[0][0] = sineSound(); g.pr().sounds[0][0][A_RATIO] = r; g.syncAuto();
        g.run (SR * 1.2, 512, { { 0, mm (0x90, 57, 100) } });
        const double f = measureFreq (g.L, (int) (0.1 * SR), (int) (1.1 * SR));
        const double want = 220.0 * r, cents = 1200.0 * std::log2 (f / want);
        worst = std::max (worst, std::fabs (cents));
    }
    report ("T1", "ratios 0.5 … 16 on A3 (220 Hz)", worst < 0.5, "worst %.3f cents", worst);
    // Fixed mode.
    Rig g; g.pr().sounds[0][0] = sineSound(); g.pr().sounds[0][0][A_FIXED] = 1000.0f; g.syncAuto();
    g.run (SR * 1.2, 512, { { 0, mm (0x90, 40, 100) } });
    const double f = measureFreq (g.L, (int) (0.1 * SR), (int) (1.1 * SR));
    report ("T1", "fixed 1000 Hz (note ignored)", std::fabs (f - 1000.0) < 0.5, "%.2f Hz", f);
    // Tune and fine.
    Rig h; h.pr().sounds[0][0] = sineSound(); h.pr().sounds[0][0][TUNE] = 7; h.pr().sounds[0][0][FINE] = 50; h.syncAuto();
    h.run (SR * 1.2, 512, { { 0, mm (0x90, 57, 100) } });
    const double fh = measureFreq (h.L, (int) (0.1 * SR), (int) (1.1 * SR));
    const double c = 1200.0 * std::log2 (fh / 220.0);
    report ("T1", "tune +7 st, fine +50 c", std::fabs (c - 750.0) < 0.5, "%.2f cents", c);
}

static void T2()
{
    auto run = [] (const char* name, std::function<void (Sound&)> make, double& worst, double& best, std::vector<int> notes = { 36, 48, 60, 72, 84, 96 }) {
        worst = -300; best = 0;
        for (int m : notes)
        {
            Rig g; Sound s = sineSound(); make (s); g.pr().sounds[0][0] = s; g.syncAuto();
            g.run ((int) (0.6 * SR), 512, { { 0, mm (0x90, (uint8_t) m, 110) } });
            const double r = aliasRatio (g.L, (int) (0.2 * SR), 440.0 * std::pow (2.0, (m - 69) / 12.0));
            worst = std::max (worst, r); best = std::min (best, r);
            if (std::getenv ("DV_VERBOSE")) std::printf ("       %-6s note %d: %.1f dB\n", name, m, r);
        }
        (void) name;
    };
    double fw, fb; run ("sine", [] (Sound&) {}, fw, fb);
    double sw, sb; run ("saw", [] (Sound& s) { s[A_LEVEL] = 0.8f; s[A_FB] = 1.0f; }, sw, sb);
    double mw, mb; run ("fm", [] (Sound& s) { s[ALGO] = 2; s[A_LEVEL] = 0.8f; s[B_RATIO] = 3.5f; s[B_LEVEL] = 0.75f; }, mw, mb);
    double dw, db; run ("drive", [] (Sound& s) { s[A_LEVEL] = 0.8f; s[A_FB] = 0.6f; s[DRIVE] = 1.0f; s[FOLD] = 0.5f; }, dw, db, { 36, 48, 60 });
    double hw, hb; run ("drvhi", [] (Sound& s) { s[A_LEVEL] = 0.8f; s[A_FB] = 0.6f; s[DRIVE] = 1.0f; s[FOLD] = 0.5f; }, hw, hb, { 72, 84, 96 });
    double mdw, mdb; run ("drvmid", [] (Sound& s) { s[A_LEVEL] = 0.8f; s[DRIVE] = 0.6f; }, mdw, mdb);
    std::printf ("       floor (pure sine) %.1f … %.1f dB\n", fb, fw);
    report ("T2", "feedback saw C2–C7, alias ratio within 6 dB of the floor", sw < std::max (fw + 6.0, -80.0), "worst %.1f dB", sw);
    report ("T2", "FM 1:3.5 C2–C7", mw < std::max (fw + 6.0, -80.0), "worst %.1f dB", mw);
    report ("T2", "drive 1 + fold 0.5 + feedback, C2–C4 (below −35 dB)", dw < -35.0, "worst %.1f dB", dw);
    report ("T2", "drive 0.6 on a sine, C2–C7 (below −60 dB)", mdw < -60.0, "worst %.1f dB", mdw);
    report ("T2", "drive 1 + fold 0.5 + feedback, C5–C7 (below −33 dB; −25 dB before the cap)", hw < -33.0, "worst %.1f dB", hw);
}

// Max |second difference| inside windows around given times vs the steady sine bound.
static double d2Max (const std::vector<float>& x, int a, int b)
{
    double m = 0; for (int i = std::max (2, a); i < b && i < (int) x.size(); ++i) m = std::max (m, (double) std::fabs (x[(size_t) i] - 2 * x[(size_t) i - 1] + x[(size_t) i - 2])); return m;
}

static void T3()
{
    // Steal: max 2 voices on the track, a third note steals; sine 220/330 Hz amplitude 0.5.
    {
        Rig g; Sound s = sineSound (0.01f, 0.3f); s[VOICES] = 2; g.pr().sounds[0][0] = s; g.syncAuto();
        std::vector<std::pair<int64_t, MidiIn>> ev = { { 0, mm (0x90, 57, 100) }, { 4800, mm (0x90, 64, 100) }, { 14400, mm (0x90, 69, 100) }, { 19200, mm (0x90, 60, 100) } };
        g.run ((int) (SR * 0.8), 512, ev);
        const int Lt = g.e->latency();
        const double steady = d2Max (g.L, 9600 + Lt, 14000 + Lt);
        const double around = std::max (d2Max (g.L, 14400 + 1000 + Lt, 14400 + 2000 + Lt), d2Max (g.L, 19200 + 1000 + Lt, 19200 + 2000 + Lt));
        // The new note's own attack (10 ms) is excluded: the windows start after it.
        const double atSteal = std::max (d2Max (g.L, 14400 - 50 + Lt, 14400 + 400 + Lt), d2Max (g.L, 19200 - 50 + Lt, 19200 + 400 + Lt));
        const double bound = 0.5 * std::pow (2 * M_PI * 440.0 / SR, 2) * 3.0;
        report ("T3", "voice steal fades (5 ms raised cosine)", atSteal < bound * 1.5 && around < bound * 1.5, "|d2| steal %.2e, after %.2e, steady %.2e, bound %.2e", atSteal, around, steady, bound);
    }
    // Choke: KIT, lane 4 (closed hat) chokes lane 5 (open hat), both sine lanes.
    {
        Rig g; auto& pr = g.pr(); pr.track[0].machine = kKit;
        for (int o = 0; o < 4; ++o) pr.sounds[0][0][opParam (o, oLevel)] = 0.75f;
        for (int l : { 4, 5 }) { Sound s = sineSound (0.002f, 0.5f); s[KIT_PITCH] = 69; s[CHOKE] = 1; s[LANE_NOTE] = l == 4 ? 42 : 46; pr.sounds[0][(size_t) l] = s; }
        pr.sounds[0][0][CUTOFF] = 20000; g.syncAuto();
        g.run ((int) (SR * 0.6), 512, { { 0, mm (0x90, 46, 100) }, { 9600, mm (0x90, 42, 100) } });
        const int Lt = g.e->latency();
        const double at = d2Max (g.L, 9600 - 20 + Lt, 9600 + 300 + Lt);
        const double bound = 0.5 * std::pow (2 * M_PI * 440.0 / SR, 2) * 2.5;
        report ("T3", "KIT choke (closed hat chokes open hat)", at < bound * 1.5, "|d2| %.2e, bound %.2e", at, bound);
    }
    // Legato glide: no retrigger, no step.
    {
        Rig g; Sound s = sineSound (0.01f, 0.1f); s[VMODE] = 2; s[GLIDE] = 0.05f; g.pr().sounds[0][0] = s; g.syncAuto();
        g.run ((int) (SR * 0.6), 512, { { 0, mm (0x90, 57, 100) }, { 9600, mm (0x90, 64, 100) }, { 9700, mm (0x80, 57, 0) } });
        const int Lt = g.e->latency();
        const double at = d2Max (g.L, 9600 - 20 + Lt, 9600 + 4800 + Lt);
        const double bound = 0.5 * std::pow (2 * M_PI * 330.0 / SR, 2) * 1.3;
        report ("T3", "mono legato with glide", at < bound * 1.5, "|d2| %.2e, bound %.2e", at, bound);
    }
    // Note start with drive and fold (a KIT kick): no spike before the body (the ADAA state starts primed).
    {
        Rig g; auto& pr = g.pr(); pr.track[0].machine = kKit;
        for (int o = 0; o < 4; ++o) pr.sounds[0][0][opParam (o, oLevel)] = 0.75f;
        Sound s = sineSound (0.0008f, 0.1f); s[A_LEVEL] = 0.85f; s[KIT_PITCH] = 31; s[PENV_DEPTH] = 24; s[PENV_TIME] = 0.035f; s[FOLD] = 0.05f; s[DRIVE] = 0.15f; s[LANE_NOTE] = 36;
        pr.sounds[0][1] = s; g.syncAuto();
        g.run ((int) (SR * 0.3), 512, { { 1000, mm (0x90, 36, 120) } });
        const int Lt = g.e->latency();
        float early = 0, body = 0;
        for (int i = 1000 + Lt; i < 1000 + Lt + 96; ++i) early = std::max (early, std::fabs (g.L[(size_t) i]));
        for (int i = 1000 + Lt + 96; i < 1000 + Lt + 4800; ++i) body = std::max (body, std::fabs (g.L[(size_t) i]));
        report ("T3", "note start through drive + fold: no onset spike", early <= body * 1.05f, "first 2 ms %.3f, body %.3f", (double) early, (double) body);
    }
    // Kill (all sound off) mid-note.
    {
        Rig g; g.pr().sounds[0][0] = sineSound (0.01f, 2.0f); g.syncAuto();
        g.run ((int) (SR * 0.3), 512, { { 0, mm (0x90, 57, 100) }, { 9600, mm (0xB0, 120, 0) } });
        const int Lt = g.e->latency();
        const double at = d2Max (g.L, 9600 - 20 + Lt, 9600 + 600 + Lt);
        const double bound = 0.5 * std::pow (2 * M_PI * 220.0 / SR, 2) * 1.3;
        // Audible tail: the sample-to-sample change (the master's 8 Hz DC filter rings below audibility after a stop).
        float tail = 0; for (int i = 9600 + 400 + Lt; i < (int) g.L.size(); ++i) tail = std::max (tail, std::fabs (g.L[(size_t) i] - g.L[(size_t) i - 1]));
        report ("T3", "kill (CC 120) fades and goes silent", at < bound * 1.5 && tail < 1e-5f, "|d2| %.2e, bound %.2e, tail %.1e", at, bound, (double) tail);
    }
}

// A demo-like project: KIT + bass + chords + pad, every feature on.
static void demoProject (Project& pr)
{
    pr.track[0].machine = kKit;
    for (int o = 0; o < 4; ++o) pr.sounds[0][0][opParam (o, oLevel)] = 0.75f;
    for (int l = 1; l <= 8; ++l)
    {
        Sound s; s[ALGO] = 2; s[A_LEVEL] = 0.8f; s[A_DEC] = 0.2f + 0.05f * l; s[A_SUS] = 0; s[B_LEVEL] = 0.3f; s[B_RATIO] = 1.5f + 0.3f * l; s[B_DEC] = 0.05f; s[B_SUS] = 0;
        s[KIT_PITCH] = 30.0f + 6.0f * l; s[PENV_DEPTH] = l == 1 ? 28.0f : 7.0f; s[PENV_TIME] = 0.03f; s[NOISE_LEVEL] = l >= 2 ? 0.5f : 0.1f; s[NOISE_DECAY] = 0.1f;
        s[FTYPE] = kFilterOff; s[AMP_SUS] = 1.0f; s[AMP_REL] = 0.1f; s[AMP_DEC] = 0.3f; s[LANE_NOTE] = defaultLaneNote (l); s[CHOKE] = (l == 4 || l == 5) ? 1 : 0;
        s[SEND_REVERB] = 0.2f; s[CRUSH] = l == 2 ? 0.3f : 0.0f;
        pr.sounds[0][(size_t) l] = s;
    }
    pr.sounds[0][0][CUTOFF] = 20000; pr.sounds[0][0][AGE] = 0.4f; pr.sounds[0][0][INS_TYPE] = kInsLofi; pr.sounds[0][0][INS_AMT] = 0.33f; pr.sounds[0][0][INS_P1] = 0.2f;
    Sound b; b[ALGO] = 5; b[A_RATIO] = 0.5f; b[A_LEVEL] = 0.55f; b[B_LEVEL] = 0.6f; b[C_LEVEL] = 0.42f; b[D_RATIO] = 2; b[D_LEVEL] = 0.3f; b[D_FB] = 0.35f;
    b[CUTOFF] = 420; b[RESO] = 0.38f; b[FENV] = 0.5f; b[F_DEC] = 0.22f; b[DRIVE] = 0.4f; b[FOLD] = 0.1f; b[VMODE] = 2; b[GLIDE] = 0.04f; b[AGE] = 0.3f; b[SUB_LEVEL] = 0.6f;
    b[LFO1_DEST] = destForParam (CUTOFF); b[LFO1_DEPTH] = 0.2f; b[LFO1_SYNC] = 16;
    pr.sounds[1][0] = b;
    Sound k; k[ALGO] = 2; k[A_LEVEL] = 0.55f; k[A_DEC] = 2.8f; k[A_SUS] = 0; k[B_LEVEL] = 0.36f; k[C_LEVEL] = 0.32f; k[D_RATIO] = 14; k[D_LEVEL] = 0.27f; k[D_DEC] = 0.07f; k[D_SUS] = 0;
    k[FTYPE] = kLP12; k[CUTOFF] = 3500; k[UNISON] = 2; k[INS_TYPE] = kInsChorus; k[INS_AMT] = 0.4f; k[SEND_REVERB] = 0.3f; k[STRUM] = 0.012f; k[AMP_REL] = 0.4f;
    pr.sounds[2][0] = k;
    Sound p; p[ALGO] = 3; p[A_LEVEL] = 0.4f; p[A_ATT] = 1.0f; p[B_LEVEL] = 0.4f; p[B_HAMT] = 0.35f; p[B_HSHAPE] = 1; p[C_LEVEL] = 0.2f; p[D_LEVEL] = 0.33f; p[D_LOOP] = 1; p[D_ATT] = 1.0f; p[D_DEC] = 2.0f;
    p[AMP_ATT] = 0.6f; p[AMP_REL] = 2.0f; p[AGE] = 0.7f; p[CUTOFF] = 1500; p[SEND_REVERB] = 0.5f; p[INS_TYPE] = kInsComb; p[INS_AMT] = 0.6f; p[INS_MIX] = 0.4f;
    p[LFO2_DEST] = mdIndex; p[LFO2_DEPTH] = 0.35f; p[LFO2_SHAPE] = 5; p[LFO2_RATE] = 0.23f;
    pr.sounds[3][0] = p;
    Pattern& pt = pr.bank.p[0]; pt.length = 16; pt.root = 9; pt.scale = 1;
    for (int i = 0; i < 16; i += 4) setStep (pr, 0, 0, 0, i, -1, 120);
    setStep (pr, 0, 0, 1, 4, -1, 110); setStep (pr, 0, 0, 1, 12, -1, 110);
    for (int i = 0; i < 16; i += 2) setStep (pr, 0, 0, 3, i, -1, 70);
    setStep (pr, 0, 0, 4, 14, -1, 90);
    pt.t[0].lane[1][15].trig = 1; pt.t[0].lane[1][15].cond = cFill; pt.t[0].lane[1][15].retrig = 3; pt.t[0].lane[1][15].retrigCount = 4;
    pt.t[0].lane[2][7].trig = 1; pt.t[0].lane[2][7].prob = 50;
    setStep (pr, 0, 1, 0, 0, 33, 110, 30); setStep (pr, 0, 1, 0, 3, 33, 100, 12); setStep (pr, 0, 1, 0, 6, 36, 100, 24); setStep (pr, 0, 1, 0, 10, 40, 100, 30);
    pt.t[1].lane[0][6].micro = 6;
    for (int i : { 0, 8 }) { Step& s = pt.t[2].lane[0][(size_t) i]; s.trig = 1; s.chord = chDegree; s.nNotes = 2; s.notes[0] = (int8_t) (i == 0 ? 0 : 5); s.notes[1] = 4; s.chordSize = 4; s.voicing = vcDrop2; s.len = 60; }
    { Step& s = pt.t[3].lane[0][0]; s.trig = 1; s.chord = chDegree; s.nNotes = 2; s.notes[0] = 0; s.notes[1] = 3; s.chordSize = 3; s.len = 180; s.lockCount = 1; s.lockStart = 0; pt.pool[0] = { (int16_t) CUTOFF, 900.0f }; pt.nLocks = 1; }
    pt.t[3].length = 13; pt.t[3].speed = 2;     // swing: renderProject sets the host Swing to 56 %
    pr.fx.reverbMode = kRevVoid; pr.fx.delayMode = kDelayDub;
}

static std::vector<float> renderProject (int block, int n, std::function<int (int)> fn = nullptr, Rig** keep = nullptr)
{
    static std::unique_ptr<Rig> holder;
    holder = std::make_unique<Rig> (std::max (block, 64));
    Rig& g = *holder;
    demoProject (g.pr()); g.syncAuto(); g.P.swing = 0.56f; g.P.tape = 0.5f; g.P.glue = 0.4f; g.P.duck = 0.3f; g.P.limit = 0.2f; g.P.delayFb = 0.6f; g.P.revDecay = 12.0f;
    g.bpm = 92; g.playing = true;
    g.run (n, block, { { 1000, mm (0x90, 60, 100) }, { 30000, mm (0x80, 60, 0) } }, fn);
    if (keep) *keep = &g;
    std::vector<float> out = g.L; out.insert (out.end(), g.R.begin(), g.R.end());
    return out;
}

static void T4()
{
    const int n = (int) (SR * 6);
    const auto ref = renderProject (512, n);
    double worst = 0; int worstB = 0;
    for (int b : { 32, 64, 113, 997, 2048 })
    {
        const auto x = renderProject (b, n);
        double d = 0; for (size_t i = 0; i < ref.size(); ++i) d = std::max (d, (double) std::fabs (x[i] - ref[i]));
        if (d > worst) { worst = d; worstB = b; }
    }
    const auto rnd = renderProject (512, n, [] (int i) { return 1 + (int) (mix64 ((uint64_t) i) % 700); });
    double dr = 0; for (size_t i = 0; i < ref.size(); ++i) dr = std::max (dr, (double) std::fabs (rnd[i] - ref[i]));
    report ("T4", "blocks 32/64/113/997/2048 = 512 (full project)", worst < 1e-5, "max diff %.2e (block %d)", worst, worstB);
    report ("T4", "random block sizes 1..700", dr < 1e-5, "max diff %.2e", dr);
}

static void T5()
{
    const int n = (int) (SR * 3);
    Rig g; demoProject (g.pr()); g.syncAuto(); g.bpm = 92; g.playing = true; g.P.tape = 0.5f;
    g.run (n, 512);
    const auto a = g.L;
    g.e->reset(); g.L.clear(); g.R.clear(); g.pos = 0;
    g.run (n, 512);
    double d = 0; for (int i = 0; i < n; ++i) d = std::max (d, (double) std::fabs (a[(size_t) i] - g.L[(size_t) i]));
    report ("T5", "reset() = fresh instance (sequencer position included)", d < 1e-6, "max diff %.2e", d);
    g.e->prepare (SR, 256); g.L.clear(); g.R.clear(); g.pos = 0;
    g.run (n, 256);
    double d2 = 0; for (int i = 0; i < n; ++i) d2 = std::max (d2, (double) std::fabs (a[(size_t) i] - g.L[(size_t) i]));
    report ("T5", "re-prepare (other block size) = fresh instance", d2 < 1e-6, "max diff %.2e", d2);
}

static void T6()
{
    int bad = 0; double worstDc = 0, worstPk = 0; double maxRt = 0;
    for (int k = 0; k < 24; ++k)
    {
        Rig g; Sound s;
        const auto& info = paramInfo();
        for (int p = 0; p < kNumSoundParams; ++p)
        {
            if (p >= LFO1_SHAPE && p <= MOD8_AMT) continue;
            const double u = hashUni (1234, (uint64_t) k, (uint64_t) p);
            s[p] = clampParam (p, fromNorm (p, (float) u));
            (void) info;
        }
        s[VOICES] = 8; s[UNISON] = 1 + k % 4; s[A_LEVEL] = 0.7f; s[LEVEL] = -6; s[AMP_SUS] = 1; s[AMP_DEC] = 1; s[AMP_REL] = 0.3f; s[AMP_ATT] = 0.01f;
        for (int o = 0; o < 4; ++o) { s[opParam (o, oDelay)] = 0; s[opParam (o, oAtt)] = std::min (s[opParam (o, oAtt)], 0.5f); }
        s[CHORD_HOLD] = 0; s[ARP_MODE] = 0; s[KIT_PITCH] = 48;
        g.pr().sounds[0][0] = s; g.syncAuto();
        std::vector<std::pair<int64_t, MidiIn>> ev;
        for (int i = 0; i < 12; ++i) { ev.push_back ({ (int64_t) (i * 4000), mm (0x90, (uint8_t) (30 + (i * 7) % 60), 100) }); ev.push_back ({ (int64_t) (i * 4000 + 3000), mm (0x80, (uint8_t) (30 + (i * 7) % 60), 0) }); }
        const auto t0 = std::chrono::steady_clock::now();
        g.run ((int) (SR * 1.5), 256, ev);
        const double dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        maxRt = std::max (maxRt, dt / 1.5);
        float pk = 0;
        for (float v : g.L) { if (! std::isfinite (v)) { ++bad; break; } pk = std::max (pk, std::fabs (v)); }
        // DC in steady state: one held note (C2), mean over the last second (short notes of a phase-reset sub
        // legitimately average to non-zero, so the busy part above is not used for this).
        Rig h; h.pr().sounds[0][0] = s; h.syncAuto();
        h.run ((int) (SR * 2.5), 256, { { 0, mm (0x90, 36, 100) } });
        double sum = 0, wsum = 0;
        for (int i = 0; i < (int) SR; ++i) { const double w = 0.5 - 0.5 * std::cos (2.0 * M_PI * i / SR); sum += w * h.L[(size_t) (i + (int) (1.5 * SR))]; wsum += w; }
        sum = sum / wsum * SR;
        sum *= g.L.size() / SR;   // so sum / size below is the mean over that second
        if (std::getenv ("DV_VERBOSE")) std::printf ("       sound %d: DC %.2e peak %.3f  ins %d ftype %d crush %.2f fold %.2f drive %.2f noise %.2f/%d sub %.2f/%d age %.2f\n", k, sum / (double) g.L.size(), (double) pk,
            s.i (INS_TYPE), s.i (FTYPE), s[CRUSH], s[FOLD], s[DRIVE], s[NOISE_LEVEL], s.i (NOISE_MODE), s[SUB_LEVEL], s.i (SUB_SHAPE), s[AGE]);
        // A sound driven into the output's soft ceiling (peak ≥ 0.9) clips asymmetric waveforms: allow 3e-3 there.
        worstDc = std::max (worstDc, std::fabs (sum / (double) g.L.size()) * (pk >= 0.9f ? 1.0 / 3.0 : 1.0)); worstPk = std::max (worstPk, (double) pk);
    }
    report ("T6", "24 random sounds: finite output", bad == 0, "%d bad", bad);
    report ("T6", "steady DC below 1e-3 (3e-3 when clipping), peak ≤ 1", worstDc < 1e-3 && worstPk <= 1.0, "DC %.1e, peak %.3f", worstDc, worstPk);
    report ("T6", "no denormal slowdown (worst sound < 25 % of real time)", maxRt < 0.25, "worst %.1f %% of real time", maxRt * 100);
}

static void T7()
{
    // Every 16th on a MIDI machine track at 120 BPM for 8 bars: 128 note-ons, each exactly on its sample.
    auto check = [] (std::function<int (int)> blk, double jitter, const char* name) {
        Rig g; auto& pr = g.pr(); pr.track[0].machine = kMidiMachine; pr.track[0].outChannel = 1;
        for (int i = 0; i < 16; ++i) setStep (pr, 0, 0, 0, i, 60, 100, 6);
        g.bpm = 120; g.playing = true;
        // Host ppq per block with a rounding error (the T2b case).
        const int n = (int) (SR * 16);
        std::vector<float> l (8192), r (8192);
        int64_t pos = 0; int bi = 0; std::vector<int64_t> ons;
        while (pos < n)
        {
            const int b = std::min (blk (bi++), (int) (n - pos));
            float* outs[10] = { l.data(), r.data() };
            for (int k = 2; k < 10; ++k) outs[k] = nullptr;
            std::fill (l.begin(), l.begin() + b, 0.0f); std::fill (r.begin(), r.begin() + b, 0.0f);
            HostTime ht; ht.playing = true; ht.bpm = 120; ht.ppq = (double) pos / SR * 2.0 + (jitter > 0 ? jitter * hashBi (5, (uint64_t) bi, 1) : 0.0);
            std::vector<MidiOut> mo;
            g.e->process (outs, b, ht, g.P, nullptr, 0, &mo);
            for (auto& m : mo) if ((m.d[0] & 0xF0) == 0x90 && m.d[2] > 0) ons.push_back (pos + m.offset);
            pos += b;
        }
        int wrong = 0;
        for (size_t i = 0; i < ons.size(); ++i) { const int64_t want = (int64_t) std::llround (i * SR * 0.125); if (std::llabs (ons[i] - want) > 1) ++wrong; }
        report ("T7", name, ons.size() == 128 && wrong == 0, "%d note-ons (128), %d off their sample", (int) ons.size(), wrong);
    };
    check ([] (int) { return 512; }, 0.0, "16ths, blocks of 512");
    check ([] (int i) { return 1 + (int) (mix64 ((uint64_t) i * 3) % 997); }, 0.0, "16ths, random blocks 1..997");
    check ([] (int) { return 480; }, 2e-10, "host ppq rounding gap between blocks (T2b)");
    // Micro timing, retrig, swing.
    {
        Rig g; auto& pr = g.pr(); pr.track[0].machine = kMidiMachine;
        setStep (pr, 0, 0, 0, 0, 60); setStep (pr, 0, 0, 0, 4, 62); pr.bank.p[0].t[0].lane[0][4].micro = 12;
        setStep (pr, 0, 0, 0, 8, 64); pr.bank.p[0].t[0].lane[0][8].retrig = 3; pr.bank.p[0].t[0].lane[0][8].retrigCount = 4; pr.bank.p[0].t[0].lane[0][8].retrigRamp = -60;
        setStep (pr, 0, 0, 0, 13, 65);
        g.P.swing = 0.66f;      // the host Swing parameter
        g.bpm = 120; g.playing = true;
        g.run ((int) (SR * 2.0), 333);
        std::vector<int64_t> ons; std::vector<int> vels;
        for (auto& m : g.midi) if ((m.d[0] & 0xF0) == 0x90 && m.d[2] > 0) { ons.push_back (m.offset); vels.push_back (m.d[2]); }
        // Steps are 6000 samples at 120 BPM. Step 4: +12/48 step = +1500. Step 8: 4 hits every 1/4 step = 1500. Step 13 (odd): swing 66 % → +0.32·6000·...
        const double sw = (0.66 - 0.5) * 2.0 * 6000.0;     // delay of an odd 16th
        const int64_t want[] = { 0, 24000 + 1500, 48000, 48000 + 1500, 48000 + 3000, 48000 + 4500, (int64_t) std::llround (13 * 6000 + sw) };
        bool ok = ons.size() >= 7;
        for (int i = 0; ok && i < 7; ++i) ok = std::llabs (ons[(size_t) i] - want[i]) <= 1;
        const bool ramp = ok && vels[2] > vels[3] && vels[3] > vels[4] && vels[4] > vels[5];
        report ("T7", "micro timing +12/48, retrig 4 × 1/4 with velocity ramp, swing 66 %", ok && ramp, "%d note-ons; first at %lld %lld %lld …", (int) ons.size(),
                ons.size() > 0 ? (long long) ons[0] : -1LL, ons.size() > 1 ? (long long) ons[1] : -1LL, ons.size() > 6 ? (long long) ons[6] : -1LL);
    }
    // The host Swing parameter is the swing (DAW QA DV1: it did nothing in 1.0.0). Every 16th on a MIDI track and a
    // TONE track at Swing 50 and 80 %: at 80 % each odd 16th is (0.8 - 0.5) · 2 · 6000 = 3600 samples late, the even
    // ones stay, and the audio differs.
    {
        auto runSw = [] (float sw, std::vector<int64_t>& ons) {
            Rig g; auto& pr = g.pr(); pr.track[0].machine = kMidiMachine; pr.track[0].outChannel = 1;
            for (int i = 0; i < 16; ++i) { setStep (pr, 0, 0, 0, i, 60, 100, 3); setStep (pr, 0, 1, 0, i, 57, 100, 3); }
            g.syncAuto(); g.P.swing = sw; g.bpm = 120; g.playing = true;
            g.run ((int) (SR * 2.0), 512);
            for (auto& m : g.midi) if ((m.d[0] & 0xF0) == 0x90 && m.d[2] > 0) ons.push_back (m.offset);
            return g.L;
        };
        std::vector<int64_t> o50, o80;
        const auto a50 = runSw (0.5f, o50), a80 = runSw (0.8f, o80);
        int wrong = 0;
        for (size_t i = 0; i < std::min (o50.size(), o80.size()); ++i)
        {
            const int64_t straight = (int64_t) std::llround ((double) i * 6000.0), swung = straight + (i % 2 == 1 ? 3600 : 0);
            if (std::llabs (o50[i] - straight) > 1 || std::llabs (o80[i] - swung) > 1) ++wrong;
        }
        double diff = 0, ref = 0;
        for (size_t i = 0; i < std::min (a50.size(), a80.size()); ++i) { diff += std::abs (a50[i] - a80[i]); ref += std::abs (a50[i]); }
        const bool ok = o50.size() == 16 && o80.size() == 16 && wrong == 0 && ref > 0 && diff > 0.1 * ref;
        report ("T7", "host Swing 50 vs 80 %: odd 16ths 3600 samples late at 80 %, the audio differs", ok,
                "%d / %d note-ons, %d off their time, audio difference %.0f %% of the level", (int) o50.size(), (int) o80.size(), wrong, ref > 0 ? 100.0 * diff / ref : 0.0);
    }
}

static void T8()
{
    auto play = [] (std::function<void (Project&)> setup, int bars, bool fill = false, int pattern = 0) {
        auto g = std::make_unique<Rig>(); auto& pr = g->pr();
        for (int t = 0; t < kTracks; ++t) { pr.track[(size_t) t].machine = kMidiMachine; pr.track[(size_t) t].outChannel = t + 1; }
        setup (pr);
        g->bpm = 120; g->playing = true; g->P.fill = fill; g->P.pattern = pattern;
        g->run ((int) (SR * 2.0 * bars), 512);
        return g;
    };
    // A:B on a 1-bar track over 8 bars: 1:2 plays bars 1,3,5,7; 2:4 plays bars 2, 6.
    {
        auto g = play ([] (Project& pr) {
            setStep (pr, 0, 0, 0, 0, 60); auto& s = pr.bank.p[0].t[0].lane[0][0]; s.cond = cAB; s.condA = 1; s.condB = 2;
            setStep (pr, 0, 0, 0, 4, 61); auto& u = pr.bank.p[0].t[0].lane[0][4]; u.cond = cAB; u.condA = 2; u.condB = 4; }, 8);
        std::vector<int> bars60, bars61;
        for (auto& m : g->midi) if ((m.d[0] & 0xF0) == 0x90 && m.d[2] > 0) (m.d[1] == 60 ? bars60 : bars61).push_back ((int) (m.offset / (SR * 2.0)));
        const bool ok = bars60 == std::vector<int> { 0, 2, 4, 6 } && bars61 == std::vector<int> { 1, 5 };
        report ("T8", "A:B 1:2 and 2:4", ok, "1:2 in %d bars, 2:4 in %d bars", (int) bars60.size(), (int) bars61.size());
    }
    // PRE chain: step 0 50 %, step 4 PRE, step 8 !PRE → 4 follows 0, 8 is its opposite. NEI on track 2.
    {
        auto g = play ([] (Project& pr) {
            setStep (pr, 0, 0, 0, 0, 60); pr.bank.p[0].t[0].lane[0][0].prob = 50;
            setStep (pr, 0, 0, 0, 4, 61); pr.bank.p[0].t[0].lane[0][4].cond = cPre;
            setStep (pr, 0, 0, 0, 8, 62); pr.bank.p[0].t[0].lane[0][8].cond = cNotPre;
            setStep (pr, 0, 1, 0, 2, 70); pr.bank.p[0].t[1].lane[0][2].cond = cNei;      // follows track 1's latest condition (step 0)
        }, 32);
        std::map<int, std::set<int>> perBar;
        for (auto& m : g->midi) if ((m.d[0] & 0xF0) == 0x90 && m.d[2] > 0) perBar[(int) (m.offset / (SR * 2.0))].insert (m.d[1]);
        int ok = 0, played = 0;
        for (int b = 0; b < 32; ++b)
        {
            const auto& s = perBar[b];
            const bool a = s.count (60) > 0;
            played += a;
            if ((s.count (61) > 0) == a && (s.count (62) > 0) == ! a && (s.count (70) > 0) == a) ++ok;
        }
        report ("T8", "50 % → PRE / !PRE / NEI chains over 32 bars", ok == 32 && played > 6 && played < 26, "%d/32 bars consistent, %d played", ok, played);
    }
    // FILL / !FILL, 1ST, BAR 2/4, LAST (queued change).
    {
        auto g = play ([] (Project& pr) {
            setStep (pr, 0, 0, 0, 0, 60); pr.bank.p[0].t[0].lane[0][0].cond = cFill;
            setStep (pr, 0, 0, 0, 4, 61); pr.bank.p[0].t[0].lane[0][4].cond = cNotFill; }, 2, true);
        const int f = countNoteOns (g->midi, -1, 60), nf = countNoteOns (g->midi, -1, 61);
        report ("T8", "FILL held: FILL plays, !FILL silent", f == 2 && nf == 0, "FILL %d, !FILL %d", f, nf);
    }
    {
        auto g = play ([] (Project& pr) {
            setStep (pr, 0, 0, 0, 0, 60); pr.bank.p[0].t[0].lane[0][0].cond = cFirst;
            setStep (pr, 0, 0, 0, 4, 61); auto& s = pr.bank.p[0].t[0].lane[0][4]; s.cond = cBar; s.condA = 2; s.condB = 4; }, 8);
        const int first = countNoteOns (g->midi, -1, 60), bar = countNoteOns (g->midi, -1, 61);
        report ("T8", "1ST once, BAR 2/4 twice in 8 bars", first == 1 && bar == 2, "1ST %d, BAR %d", first, bar);
    }
    {
        auto g = std::make_unique<Rig>(); auto& pr = g->pr(); pr.track[0].machine = kMidiMachine;
        setStep (pr, 0, 0, 0, 0, 60); setStep (pr, 0, 0, 0, 8, 61); pr.bank.p[0].t[0].lane[0][8].cond = cLast;
        setStep (pr, 1, 0, 0, 0, 72);
        g->bpm = 120; g->playing = true; g->P.pattern = 0;
        g->run ((int) (SR * 2.0 * 2.5), 512);    // bar 1, 2 and half of 3 on pattern 1
        g->P.pattern = 1;                         // queued during bar 3: bar 3 is the LAST
        g->run ((int) (SR * 2.0 * 2.5), 512);
        std::vector<int> lastBars; int p2 = 0;
        for (auto& m : g->midi) if ((m.d[0] & 0xF0) == 0x90 && m.d[2] > 0) { if (m.d[1] == 61) lastBars.push_back ((int) (m.offset / (SR * 2.0))); if (m.d[1] == 72) ++p2; }
        report ("T8", "LAST plays only in the pass before a queued change", lastBars == std::vector<int> { 2 } && p2 == 2, "LAST bars %d (want bar 3), pattern 2 played %d", lastBars.empty() ? -1 : lastBars[0] + 1, p2);
    }
    // Probability is deterministic for a seed and changes with it.
    {
        auto count = [&] (uint32_t seed) {
            auto g = play ([seed] (Project& pr) { pr.seed = seed; for (int i = 0; i < 16; ++i) { setStep (pr, 0, 0, 0, i, 60 + i); pr.bank.p[0].t[0].lane[0][(size_t) i].prob = 40; } }, 4);
            std::vector<int> v; for (auto& m : g->midi) if ((m.d[0] & 0xF0) == 0x90 && m.d[2] > 0) v.push_back ((int) m.offset);
            return v;
        };
        const auto a = count (1), b = count (1), c = count (2);
        report ("T8", "probability 40 %: same seed = same, other seed = other", a == b && a != c && a.size() > 10 && a.size() < 45, "%d hits of 64", (int) a.size());
    }
}

static void T9()
{
    std::array<ChordMem, kChordMems> mem {};
    auto chord = [&] (int deg, int size, int type, int voicing, int flags = 0, int inv = 0) {
        Step s; s.chord = chDegree; s.nNotes = 2; s.notes[0] = (int8_t) deg; s.notes[1] = 4; s.chordSize = (uint8_t) size; s.chordType = (uint8_t) type; s.voicing = (uint8_t) voicing; s.chordFlags = (uint8_t) flags; s.inversion = (uint8_t) inv;
        int out[16]; const int n = buildChord (s, 9, 1, mem, out);
        return std::vector<int> (out, out + n);
    };
    report ("T9", "A minor degree 0, 7th: A C E G", chord (0, 4, ctDiatonic, vcClose) == std::vector<int> { 69, 72, 76, 79 });
    report ("T9", "degree 5 (F) triad: F A C", chord (5, 3, ctDiatonic, vcClose) == std::vector<int> { 77, 81, 84 });
    report ("T9", "drop-2 of Am7: E A C G", chord (0, 4, ctDiatonic, vcDrop2) == std::vector<int> { 64, 69, 72, 79 });
    report ("T9", "sus2 on A: A B E", chord (0, 3, ctSus2, vcClose) == std::vector<int> { 69, 71, 76 });
    report ("T9", "quartal on A (scale 4ths): A D G", chord (0, 3, ctQuartal, vcClose) == std::vector<int> { 69, 74, 79 });
    report ("T9", "cluster: root, b2, fifth", chord (0, 3, ctCluster, vcClose) == std::vector<int> { 69, 70, 76 });
    report ("T9", "root bass + b9 flag on E (degree 4)", chord (4, 3, ctDiatonic, vcClose, cfRootBass | cfFlat9) == std::vector<int> { 64, 76, 79, 83, 89 });
    report ("T9", "first inversion of Am: C E A", chord (0, 3, ctDiatonic, vcClose, 0, 1) == std::vector<int> { 72, 76, 81 });
    // Chord memory: an open minor-11 shape (0, 7, 10, 14, 17), played from D, quantised to A minor.
    mem[0].n = 5; const int iv[] = { 0, 7, 10, 14, 17 }; for (int i = 0; i < 5; ++i) mem[0].iv[i] = (int8_t) iv[i];
    Step s; s.chord = chMemory; s.chordType = 0; s.notes[0] = 62; s.nNotes = 1;
    int out[16]; const int n = buildChord (s, 9, 1, mem, out);
    report ("T9", "chord memory from D in A minor: D A C E G", std::vector<int> (out, out + n) == std::vector<int> { 62, 69, 72, 76, 79 });
    // A chord step through the engine: four voices sound, strummed.
    Rig g; auto& pr = g.pr(); pr.sounds[0][0] = sineSound(); pr.sounds[0][0][STRUM] = 0.02f; pr.track[0].doubleMidi = true; g.syncAuto();
    Step& st = pr.bank.p[0].t[0].lane[0][0]; st.trig = 1; st.chord = chDegree; st.nNotes = 2; st.notes[0] = 0; st.notes[1] = 4; st.chordSize = 4; st.len = 12;
    g.playing = true; g.run ((int) (SR * 0.2), 512);
    std::vector<int> t; for (auto& m : g.midi) if ((m.d[0] & 0xF0) == 0x90 && m.d[2] > 0) t.push_back (m.offset);
    const bool strum = t.size() == 4 && std::llabs ((long long) (t[1] - t[0]) - 960) <= 1 && std::llabs ((long long) (t[3] - t[0]) - 2880) <= 1;
    report ("T9", "chord step through the engine: 4 notes strummed 20 ms apart", strum, "%d notes", (int) t.size());
}

static void T10()
{
    // Arpeggiator (DAW QA DV5): C-E-G pressed on a 1/16 tick at 120 BPM plays C on that tick, then E, G, C, E, one per
    // tick, and the release on a tick adds nothing. The notes go out as MIDI (doubling) so their times can be read.
    for (int blk : { 512, 97, 6000 })
    {
        Rig g; auto& pr = g.pr(); pr.sounds[0][0] = sineSound(); pr.sounds[0][0][ARP_MODE] = 1; pr.sounds[0][0][ARP_RATE] = 6;
        pr.track[0].doubleMidi = true; pr.track[0].outChannel = 2;
        g.syncAuto(); g.bpm = 120; g.playing = true;
        g.run (60000, blk, { { 12000, mm (0x90, 60, 100) }, { 12000, mm (0x90, 64, 100) }, { 12000, mm (0x90, 67, 100) },
                             { 42000, mm (0x80, 60, 0) }, { 42000, mm (0x80, 64, 0) }, { 42000, mm (0x80, 67, 0) } });
        std::vector<std::pair<int64_t, int>> ons; std::set<int> out; int strayOff = 0;
        for (auto& m : g.midi)
        {
            if ((m.d[0] & 0xF0) == 0x90 && m.d[2] > 0) { ons.push_back ({ m.offset, m.d[1] }); out.insert (m.d[1]); }
            else if (((m.d[0] & 0xF0) == 0x80 || (m.d[0] & 0xF0) == 0x90) && ! out.count (m.d[1])) ++strayOff;
        }
        const std::vector<std::pair<int64_t, int>> want { { 12000, 60 }, { 18000, 64 }, { 24000, 67 }, { 30000, 60 }, { 36000, 64 } };
        bool ok = ons.size() == want.size() && strayOff == 0;
        for (size_t i = 0; ok && i < want.size(); ++i) ok = std::llabs (ons[i].first - want[i].first) <= 1 && ons[i].second == want[i].second;
        report ("T10", ("arp: chord on a tick plays C E G C E from that tick, release on a tick adds nothing (blocks of " + std::to_string (blk) + ")").c_str(), ok,
                "%d note-ons, first %d at %lld, %d note-offs for notes that never went out", (int) ons.size(), ons.empty() ? -1 : ons[0].second,
                ons.empty() ? -1LL : (long long) ons[0].first, strayOff);
    }

    // Per-channel mode: channel 2 plays track 2.
    {
        Rig g; auto& pr = g.pr(); pr.midi.inMode = 1; for (int t = 0; t < kTracks; ++t) { pr.track[(size_t) t].inChannel = t + 1; pr.sounds[(size_t) t][0] = sineSound(); }
        g.syncAuto();
        g.run (4800, 256, { { 100, mm (0x91, 60, 100) } });
        int onTrack1 = 0; for (int i = 0; i < kSlots; ++i) if (g.e->voice (i).isAlive() && g.e->voice (i).track == 1) ++onTrack1;
        report ("T10", "channels: ch 2 plays track 2", onTrack1 == 1, "%d voices on track 2", onTrack1);
    }
    // OMNI: any channel plays the focused track.
    {
        Rig g; auto& pr = g.pr(); pr.midi.inMode = 0; pr.midi.focus = 2; for (int t = 0; t < kTracks; ++t) pr.sounds[(size_t) t][0] = sineSound(); g.syncAuto();
        g.run (4800, 256, { { 100, mm (0x9A, 60, 100) } });
        int on2 = 0; for (int i = 0; i < kSlots; ++i) if (g.e->voice (i).isAlive() && g.e->voice (i).track == 2) ++on2;
        report ("T10", "OMNI: channel 11 plays the focused track 3", on2 == 1, "%d voices", on2);
    }
    // KIT map: 38 → snare lane (2), 46 → open hat (5), 70 → perc lane (8).
    {
        Rig g; auto& pr = g.pr(); pr.track[0].machine = kKit;
        for (int l = 1; l <= 8; ++l) { Sound s = sineSound(); s[LANE_NOTE] = defaultLaneNote (l); s[AMP_REL] = 2; pr.sounds[0][(size_t) l] = s; }
        for (int o = 0; o < 4; ++o) pr.sounds[0][0][opParam (o, oLevel)] = 0.75f;
        g.syncAuto();
        g.run (4800, 256, { { 10, mm (0x90, 38, 100) }, { 20, mm (0x90, 46, 100) }, { 30, mm (0x90, 70, 100) } });
        std::set<int> lanes; for (int i = 0; i < kSlots; ++i) if (g.e->voice (i).isAlive()) lanes.insert (g.e->voice (i).lane);
        report ("T10", "KIT notes 38, 46, 70 → lanes snare, open hat, perc", lanes == std::set<int> { 2, 5, 8 });
    }
    // Pitch bend +2 st (range 2).
    {
        Rig g; g.pr().sounds[0][0] = sineSound(); g.syncAuto();
        g.run ((int) (SR * 1.0), 256, { { 0, mm (0xE0, 0x7F, 0x7F) }, { 10, mm (0x90, 57, 100) } });
        const double f = measureFreq (g.L, (int) (0.2 * SR), (int) (0.9 * SR));
        const double c = 1200.0 * std::log2 (f / 220.0);
        report ("T10", "pitch bend full up, range 2 → +200 cents", std::fabs (c - 200.0) < 1.0, "%.2f cents", c);
    }
    // Note off releases; sustain pedal holds.
    {
        Rig g; g.pr().sounds[0][0] = sineSound (0.005f, 0.01f); g.syncAuto();
        g.run ((int) (SR * 0.5), 256, { { 0, mm (0xB0, 64, 127) }, { 10, mm (0x90, 60, 100) }, { 4800, mm (0x80, 60, 0) } });
        const bool held = g.e->activeVoices() == 1;
        g.run ((int) (SR * 0.2), 256, { { g.pos + 10, mm (0xB0, 64, 0) } });
        report ("T10", "sustain pedal holds a released note, pedal up releases", held && g.e->activeVoices() == 0, "held %d, after %d", held ? 1 : 0, g.e->activeVoices());
    }
}

static void T11()
{
    // MIDI machine: notes, chords and locks as CC; balanced on/off; transport stop sends the offs.
    Rig g; auto& pr = g.pr(); pr.track[0].machine = kMidiMachine; pr.track[0].outChannel = 5;
    setStep (pr, 0, 0, 0, 0, 48, 100, 24);
    Step& c = pr.bank.p[0].t[0].lane[0][4]; c.trig = 1; c.chord = chDegree; c.nNotes = 2; c.notes[0] = 0; c.notes[1] = 4; c.chordSize = 3; c.len = 6;
    setStep (pr, 0, 0, 0, 8, 50, 90, Step::kTie);
    Step& l = pr.bank.p[0].t[0].lane[0][12]; l.trig = 1; l.nNotes = 1; l.notes[0] = 52; l.len = 6; l.lockCount = 1; l.lockStart = 0;
    pr.bank.p[0].pool[0] = { (int16_t) CUTOFF, 1000.0f }; pr.bank.p[0].nLocks = 1;
    g.bpm = 120; g.playing = true;
    g.run ((int) (SR * 1.9), 512);
    g.playing = false; g.run (4800, 512);
    int ons = 0, offs = 0, cc = 0, wrongCh = 0;
    std::map<int, int> balance;
    for (auto& m : g.midi)
    {
        const int st = m.d[0] & 0xF0;
        if ((m.d[0] & 15) != 4) ++wrongCh;
        if (st == 0x90 && m.d[2] > 0) { ++ons; balance[m.d[1]]++; }
        else if (st == 0x80 || (st == 0x90 && m.d[2] == 0)) { ++offs; balance[m.d[1]]--; }
        else if (st == 0xB0 && m.d[1] == 74) ++cc;
    }
    bool bal = true; for (auto& kv : balance) bal &= kv.second == 0;
    report ("T11", "MIDI machine: 6 notes (one a triad), lock → CC 74, ch 5", ons == 6 && cc == 1 && wrongCh == 0, "%d on, %d CC74, %d on other channels", ons, cc, wrongCh);
    report ("T11", "every note-on has its note-off (tie released at stop)", bal && ons == offs, "%d on / %d off", ons, offs);
    // Doubling: a TONE track sounds and sends.
    Rig d; auto& p2 = d.pr(); p2.sounds[0][0] = sineSound(); p2.track[0].doubleMidi = true; p2.track[0].outChannel = 3; d.syncAuto();
    for (int i = 0; i < 16; i += 4) setStep (p2, 0, 0, 0, i, 60 + i);
    d.bpm = 120; d.playing = true; d.run ((int) (SR * 2.0), 512);
    float pk = 0; for (float v : d.L) pk = std::max (pk, std::fabs (v));
    report ("T11", "doubling: TONE track sounds and sends its 4 notes on ch 3", countNoteOns (d.midi, 3) == 4 && pk > 0.05f, "%d notes, peak %.2f", countNoteOns (d.midi, 3), (double) pk);
}

static void T12()
{
    auto bench = [] (std::function<void (Rig&)> setup, std::vector<std::pair<int64_t, MidiIn>> ev, double secs, const char* name, double limit) {
        Rig g (512); setup (g);
        const int n = (int) (SR * secs);
        const auto t0 = std::chrono::steady_clock::now();
        g.run (n, 512, ev);
        const double dt = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
        const double pct = dt / secs * 100.0;
        report ("T12", name, pct < limit, "%.2f %% of one core (limit %.0f %%), %d voices at the end", pct, limit, g.e->activeVoices());
    };
    bench ([] (Rig& g) { demoProject (g.pr()); g.syncAuto(); g.bpm = 92; g.playing = true; g.P.tape = 0.4f; g.P.glue = 0.3f; }, {}, 8.0, "dense 4-track project (KIT, bass, 4-note unison chords, pad)", 15.0);      // M4 5.4 %, Ryzen 9 5900XT (MSVC) 12.8 %
    std::vector<std::pair<int64_t, MidiIn>> ev; for (int i = 0; i < 16; ++i) ev.push_back ({ (int64_t) i * 10, mm (0x90, (uint8_t) (36 + i * 3), 100) });
    bench ([] (Rig& g) {
        Sound s; s[ALGO] = 0; s[A_LEVEL] = 0.3f; s[B_LEVEL] = 0.5f; s[C_LEVEL] = 0.4f; s[D_LEVEL] = 0.3f; s[D_FB] = 0.4f; s[AGE] = 1.0f; s[CUTOFF] = 2000; s[RESO] = 0.4f; s[VOICES] = 16; s[AMP_REL] = 1;
        g.pr().sounds[0][0] = s; g.syncAuto(); }, ev, 6.0, "16 voices, STACK + feedback, LP24, AGE 1 (plan: ≤ 10 %)", 10.0);
}

static void T13()
{
    Rig g;
    report ("T13", "latency = voice 32 + track tape + master tape", g.e->latency() == 32 + (int) std::lround (0.002 * SR) + (int) std::lround (0.0015 * SR), "%d samples", g.e->latency());
    // Sub alignment: FM layer (sine at the note, via the decimator) and sub (sine −1 oct) cross zero together at the start.
    Rig h; Sound s = sineSound (0.0005f, 0.05f); s[SUB_LEVEL] = 1.0f; s[A_LEVEL] = 0.0001f; h.pr().sounds[0][0] = s; h.syncAuto();
    h.run (4800, 512, { { 0, mm (0x90, 57, 100) } });
    Rig k; Sound s2 = sineSound (0.0005f, 0.05f); s2[SUB_LEVEL] = 0.0f; s2[A_LEVEL] = 0.5f; k.pr().sounds[0][0] = s2; k.syncAuto();
    k.run (4800, 512, { { 0, mm (0x90, 57, 100) } });
    auto onset = [] (const std::vector<float>& x) { float pk = 0; for (float v : x) pk = std::max (pk, std::fabs (v)); for (size_t i = 0; i < x.size(); ++i) if (std::fabs (x[i]) > 0.02f * pk) return (int) i; return -1; };
    const int a = onset (h.L), b = onset (k.L);
    report ("T13", "sub and FM layer start on the same sample (±2)", std::abs (a - b) <= 2 && a > 0, "sub %d, FM %d", a, b);
}

int main (int argc, char** argv)
{
    std::map<std::string, std::function<void()>> tests { { "T1", T1 }, { "T2", T2 }, { "T3", T3 }, { "T4", T4 }, { "T5", T5 }, { "T6", T6 }, { "T7", T7 },
                                                         { "T8", T8 }, { "T9", T9 }, { "T10", T10 }, { "T11", T11 }, { "T12", T12 }, { "T13", T13 } };
    std::vector<std::string> run;
    for (int i = 1; i < argc; ++i) run.push_back (argv[i]);
    if (run.empty()) run = { "T1", "T2", "T3", "T4", "T5", "T6", "T7", "T8", "T9", "T10", "T11", "T12", "T13" };
    for (auto& t : run) { auto it = tests.find (t); if (it != tests.end()) { std::printf ("%s\n", t.c_str()); it->second(); } }
    std::printf ("\n%s: %d failure(s)\n", fails ? "FAIL" : "PASS", fails);
    return fails ? 1 : 0;
}
