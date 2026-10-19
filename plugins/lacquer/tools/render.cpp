// lacquerrender: masteriza un archivo en dos pasadas (análisis completo, después render) y escribe un informe JSON.
//   lacquerrender in.wav out.wav [--style balanced|warm|bright|punchy|loud|gentle] [--intensity 0-100]
//                 [--target streaming|apple|loud|club|cd|r128|match] [--lufs N] [--ceiling N] [--bits 16|24|32]
//                 [--report out.json] [--block N]
// Sin dependencias (WAV de 16/24/32 bits o float). Determinista: el mismo archivo da los mismos bytes.
#include "EngineMaster.h"
#include "ReportJson.h"
#include "wavio.h"
#include <chrono>
#include <cstring>

using namespace dh::lq;


int main (int argc, char** argv)
{
    if (argc < 3)
    {
        std::printf ("usage: lacquerrender in.wav out.wav [--style balanced|warm|bright|punchy|loud|gentle] [--intensity 0-100]\n"
                     "                     [--target streaming|apple|loud|club|cd|r128|match] [--lufs N] [--ceiling N]\n"
                     "                     [--bits 16|24|32] [--report out.json] [--block N]\n");
        return 1;
    }
    auto opt = [&] (const char* name, const char* def) -> std::string { for (int i = 2; i + 1 < argc; ++i) if (! std::strcmp (argv[i], name)) return argv[i + 1]; return def; };
    Options o;
    const int si = styleIndex (opt ("--style", "balanced"));
    if (si < 0) { std::fprintf (stderr, "unknown style\n"); return 1; }
    o.style = (Style) si;
    o.intensity = std::clamp (std::atof (opt ("--intensity", "50").c_str()) / 100.0, 0.0, 1.0);
    const std::string tgt = opt ("--target", "streaming");
    bool found = false;
    for (auto& t : loudTargets()) if (tgt == t.id) { o.targetLufs = t.lufs; o.ceilingDb = t.ceilingDb; o.matchInput = t.matchInput; found = true; }
    if (! found) { std::fprintf (stderr, "unknown target\n"); return 1; }
    if (auto v = opt ("--lufs", ""); ! v.empty()) { o.targetLufs = std::atof (v.c_str()); o.matchInput = false; }
    if (auto v = opt ("--ceiling", ""); ! v.empty()) o.ceilingDb = std::clamp (std::atof (v.c_str()), -6.0, 0.0);
    const int bits = std::atoi (opt ("--bits", "24").c_str());
    const int block = std::max (1, std::atoi (opt ("--block", "512").c_str()));

    Audio a; std::string err;
    if (! readWav (argv[1], a, err)) { std::fprintf (stderr, "%s\n", err.c_str()); return 2; }
    if (! std::strcmp (argv[2], "--bench"))
    {
        // CPU de la cadena como en el plugin (regiones sobremuestreadas siempre activas), con el plan aprendido y con
        // todos los módulos forzados; bloques de 512, porcentaje de un núcleo.
        o.constantLatency = true;
        Plan p = Learner::learn (a.L.data(), a.R.data(), a.L.size(), a.fs, o);
        auto run = [&] (const Plan& pl) {
            Chain c; c.prepare (a.fs, pl);
            auto L = a.L, R = a.R; double best = 1e9;
            for (int rep = 0; rep < 3; ++rep)
            {
                c.reset();
                const auto t0 = std::chrono::steady_clock::now();
                for (size_t pos = 0; pos + 512 <= L.size(); pos += 512) c.process (L.data() + pos, R.data() + pos, 512);
                best = std::min (best, std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count());
            }
            return 100.0 * best / ((double) L.size() / a.fs);
        };
        const double learned = run (p);
        p.harsh.on = p.sib.on = p.mbOn = p.glueOn = p.sat.on = p.clipOn = p.stereo.on = true; p.stereo.monoHz = 120; p.sat.driveDb = 2;
        const double all = run (p);
        std::printf ("CPU at %.0f Hz, 512-sample blocks: learned chain %.2f %%, every module on %.2f %% of one core\n", a.fs, learned, all);
        return 0;
    }
    const auto t0 = std::chrono::steady_clock::now();
    const size_t n = a.L.size();
    Report rep;
    const Plan plan = Learner::learn (a.L.data(), a.R.data(), n, a.fs, o, &rep);

    // Render: la cadena completa, por bloques del tamaño pedido, con la latencia compensada.
    Chain chain; chain.prepare (a.fs, plan);
    const int lat = chain.latency();
    Audio out; out.fs = a.fs;
    out.L.assign (n, 0.0f); out.R.assign (n, 0.0f);
    std::vector<float> bl ((size_t) block), br ((size_t) block);
    for (size_t pos = 0; pos < n + (size_t) lat; pos += (size_t) block)
    {
        const size_t k = std::min ((size_t) block, n + (size_t) lat - pos);
        for (size_t i = 0; i < k; ++i) { const size_t s = pos + i; bl[i] = s < n ? a.L[s] : 0.0f; br[i] = s < n ? a.R[s] : 0.0f; }
        chain.process (bl.data(), br.data(), (int) k);
        for (size_t i = 0; i < k; ++i)
        {
            const long long s = (long long) (pos + i) - lat;
            if (s >= 0 && (size_t) s < n) { out.L[(size_t) s] = bl[i]; out.R[(size_t) s] = br[i]; }
        }
    }
    const double secs = std::chrono::duration<double> (std::chrono::steady_clock::now() - t0).count();
    OutMeas m = measure (out.L, out.R, a.fs);
    {
        const Report ro = Analyzer::run (out.L.data(), out.R.data(), n, a.fs);
        m.distIn = spectralDistance (rep.mid, styleDef (o.style).target);
        m.distOut = spectralDistance (ro.mid, styleDef (o.style).target);
    }
    if (! writeWav (argv[2], out, bits == 16 || bits == 32 ? bits : 24, err)) { std::fprintf (stderr, "%s\n", err.c_str()); return 3; }
    const std::string js = reportJson (argv[1], argv[2], rep, plan, m, chain.clippedSubSamples(), secs);
    if (auto rp = opt ("--report", ""); ! rp.empty()) { FILE* f = std::fopen (rp.c_str(), "w"); if (f) { std::fputs (js.c_str(), f); std::fclose (f); } }
    std::printf ("%s: in %.2f LUFS %.2f dBTP (PLR %.1f) -> out %.2f LUFS %.2f dBTP, LRA %.1f -> %.1f, limiter %.1f dB avg, dist %.2f -> %.2f dB, %.1fx real time\n",
                 styleDef (o.style).name, rep.lufsI, rep.truePeakDb, rep.plr, m.lufs, m.tp, rep.lra, m.lra, plan.limiterAvgGrDb, m.distIn, m.distOut, (double) n / a.fs / secs);
    for (auto& s : plan.notes) std::printf ("  - %s\n", s.c_str());
    return 0;
}
