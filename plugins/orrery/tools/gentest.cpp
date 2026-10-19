// Pruebas del generador de Orrery sin JUCE:
//   clang++ -std=c++17 -O2 -ffp-contract=off -I plugins/orrery/Source/gen plugins/orrery/tools/gentest.cpp -o /tmp/orr_gentest && /tmp/orr_gentest
//   cl /std:c++17 /O2 /fp:precise /EHsc /I plugins\orrery\Source\gen plugins\orrery\tools\gentest.cpp   (Windows)
// La "huella" de la prueba 8 tiene que dar igual en macOS y en Windows.
#include "Generator.h"
#include <cstdio>
#include <map>
#include <set>

using namespace dh::orr;

static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? "ok" : "FALLA", what); if (! ok) ++fails; }

struct Log { double sec; uint8_t s, d1, d2; int lane; };

static std::vector<Log> run (Generator& g, double seconds, int block = 512, double fs = 48000.0)
{
    std::vector<Log> log; std::vector<OutEvent> ev;
    g.prepare (fs);
    const int blocks = (int) (seconds * fs / block);
    for (int b = 0; b < blocks; ++b)
    {
        g.process (block, false, 0.0, 120.0, ev);
        for (auto& e : ev) log.push_back ({ (b * block + e.offset) / fs, e.data[0], e.data[1], e.data[2], e.lane });
    }
    g.g.run = false;
    g.process (block, false, 0.0, 120.0, ev);
    for (auto& e : ev) log.push_back ({ (blocks * block + e.offset) / fs, e.data[0], e.data[1], e.data[2], e.lane });
    return log;
}

static void setupAmbient (Generator& g, uint32_t seed)
{
    g.g = GlobalParams{}; g.g.run = true; g.g.hostClock = false; g.g.tempo = 60.0f; g.g.seed = seed; g.g.tension = 0.0f; g.g.fadeIn = 0.0f;
    const int roles[7] = { Drone, Pad, Melody, Arp, Cloud, Bass, Pulse };
    for (int l = 0; l < kLanes; ++l) { g.lanes[(size_t) l] = LaneParams{}; }
    for (int l = 0; l < 7; ++l)
    {
        auto& L = g.lanes[(size_t) l]; L.on = true; L.role = roles[l]; L.channel = l + 1; L.follow = l == 2 ? FollowScale : FollowChord;
        L.euclidPulses = l == 6 ? 3 : 0; L.euclidSteps = 8;
    }
    g.lanes[4].echoes = 3;
    g.lanes[3].loop = true; g.lanes[3].loopBeats = 17.0f;
    g.lanes[2].ccNum = 74; g.lanes[2].ccShape = CcDrift;
    g.lanes[1].driftCents = 8.0f;
}

int main()
{
    std::printf ("1. Diez minutos de ambient (7 carriles, Re dórico)\n");
    {
        Generator g; setupAmbient (g, 42);
        const auto log = run (g, 600.0);
        const auto sc = scalePcs (scales()[2], 2);
        int ons = 0, outOfScale = 0, maxPoly = 0, maxRate = 0;
        std::map<int, std::set<int>> on;          // canal → notas sonando
        std::map<std::pair<int, int>, int> perSec; // (canal, segundo) → mensajes
        std::map<int, int> perLane;
        for (auto& e : log)
        {
            const int ch = e.s & 15, st = e.s & 0xF0;
            perSec[{ ch, (int) e.sec }]++;
            if (st == 0x90 && e.d2 > 0) { ++ons; perLane[e.lane]++; if (std::find (sc.begin(), sc.end(), e.d1 % 12) == sc.end()) { if (outOfScale++ < 5) std::printf ("  fuera: carril %d nota %d en %.2f s\n", e.lane, e.d1, e.sec); } on[ch].insert (e.d1); maxPoly = std::max (maxPoly, (int) on[ch].size()); }
            else if (st == 0x80 || (st == 0x90 && e.d2 == 0)) on[ch].erase (e.d1);
        }
        for (auto& kv : perSec) maxRate = std::max (maxRate, kv.second);
        int stuck = 0; for (auto& kv : on) stuck += (int) kv.second.size();
        std::printf ("  %d notas; por carril:", ons); for (auto& kv : perLane) std::printf (" %s %d", roleName (g.lanes[(size_t) kv.first].role), kv.second); std::printf ("\n");
        std::printf ("  polifonía máxima por canal %d, mensajes por segundo por canal (máx) %d\n", maxPoly, maxRate);
        check (outOfScale == 0, "con tensión 0 ninguna nota sale de la escala");
        check (stuck == 0, "al parar no queda ninguna nota colgada");
        check (maxRate < 120, "tráfico apto para MIDI DIN (menos de 120 mensajes por segundo por canal)");
        check (perLane.size() == 7, "los 7 roles suenan");
    }
    std::printf ("2. Semilla\n");
    {
        Generator a, b, c; setupAmbient (a, 7); setupAmbient (b, 7); setupAmbient (c, 8);
        const auto la = run (a, 60.0), lb = run (b, 60.0), lc = run (c, 60.0);
        bool same = la.size() == lb.size();
        for (size_t i = 0; same && i < la.size(); ++i) same = la[i].s == lb[i].s && la[i].d1 == lb[i].d1 && std::abs (la[i].sec - lb[i].sec) < 1e-9;
        check (same, "la misma semilla da exactamente el mismo resultado");
        check (la.size() != lc.size() || la[10].d1 != lc[10].d1 || la[20].d1 != lc[20].d1, "otra semilla da otro resultado");
    }
    std::printf ("3. Lazo de 17 pulsos (Eno)\n");
    {
        Generator g; setupAmbient (g, 3);
        for (int l = 0; l < kLanes; ++l) if (l != 3) g.lanes[(size_t) l].on = false;
        g.lanes[3].mutate = 0.0f; g.lanes[3].probability = 1.0f; g.g.humanizeMs = 0.0f; g.g.progression = 1; g.g.color = 0.0f; g.lanes[3].follow = FollowScale;
        const auto log = run (g, 17.0 * 6);
        std::vector<std::pair<double, int>> ons;
        for (auto& e : log) if ((e.s & 0xF0) == 0x90 && e.d2 > 0) ons.push_back ({ e.sec, e.d1 });
        int matched = 0, total = 0;
        for (auto& [t, n] : ons)
        {
            if (t < 17.0 || t >= 17.0 * 5) continue;
            ++total;
            for (auto& [t2, n2] : ons) if (std::abs (t2 - (t + 17.0)) < 0.02 && n2 == n) { ++matched; break; }
        }
        std::printf ("  %d de %d notas se repiten 17 s después\n", matched, total);
        check (total > 10 && matched >= total * 9 / 10, "el lazo se repite con su período");
    }
    std::printf ("4. Sesión de 1 minuto con fades\n");
    {
        Generator g; setupAmbient (g, 5); g.g.sessionMinutes = 1.0f; g.g.fadeIn = 10.0f; g.g.fadeOut = 15.0f;
        const auto log = run (g, 80.0);
        double lastOn = 0; int earlyLoud = 0, cc7Min = 127;
        std::map<int, std::set<int>> on;
        for (auto& e : log)
        {
            const int st = e.s & 0xF0;
            if (st == 0x90 && e.d2 > 0) { lastOn = e.sec; if (e.sec < 2.0 && e.d2 > 70) ++earlyLoud; on[e.s & 15].insert (e.d1); }
            else if (st == 0x80) on[e.s & 15].erase (e.d1);
            else if (st == 0xB0 && e.d1 == 7 && e.sec > 55.0) cc7Min = std::min (cc7Min, (int) e.d2);
        }
        int stuck = 0; for (auto& kv : on) stuck += (int) kv.second.size();
        std::printf ("  última nota a los %.1f s; CC7 al final %d\n", lastOn, cc7Min);
        check (lastOn <= 60.5 && stuck == 0, "termina al minuto y suelta todo");
        check (earlyLoud == 0 && cc7Min < 20, "entra y sale en fade");
    }
    std::printf ("5. Modo suave\n");
    {
        Generator g; setupAmbient (g, 9); g.g.gentle = true; g.g.brightness = 1.0f; g.g.density = 1.0f;
        for (auto& L : g.lanes) L.velocity = 1.0f;
        const auto log = run (g, 120.0);
        int maxV = 0, maxN = 0;
        for (auto& e : log) if ((e.s & 0xF0) == 0x90 && e.d2 > 0 && e.lane != 4) { maxV = std::max (maxV, (int) e.d2); maxN = std::max (maxN, (int) e.d1); }
        std::printf ("  velocidad máxima %d, nota más aguda %d\n", maxV, maxN);
        check (maxV <= 92 && maxN <= 88, "velocidades y registro acotados");
    }
    std::printf ("6. Respiración: los acordes cambian en respiraciones enteras\n");
    {
        Generator g; setupAmbient (g, 11); g.g.breathRate = 6.0f;   // 10 s por respiración
        for (int l = 0; l < kLanes; ++l) if (l != 1) g.lanes[(size_t) l].on = false;
        g.g.humanizeMs = 0.0f; g.lanes[1].length = 0.0f;
        const auto log = run (g, 120.0);
        std::set<int> starts;
        for (auto& e : log) if ((e.s & 0xF0) == 0x90 && e.d2 > 0) starts.insert ((int) std::lround (e.sec * 10.0));
        int off = 0, groups = 0; int prev = -1000;
        for (int t : starts) { if (t - prev > 20) { ++groups; const double s = t / 10.0; if (std::abs (s / 10.0 - std::round (s / 10.0)) > 0.05) ++off; } prev = t; }
        std::printf ("  %d entradas de pad, %d fuera de la respiración\n", groups, off);
        check (groups >= 3 && off == 0, "el pad entra al comienzo de cada ciclo de respiración");
    }
    std::printf ("7. Vínculo: una seguidora toca sobre los acordes de la líder\n");
    {
        Generator lead, fol; setupAmbient (lead, 21); setupAmbient (fol, 99);
        for (int l = 0; l < kLanes; ++l) { lead.lanes[(size_t) l].on = l == 1; fol.lanes[(size_t) l].on = l == 1; }
        fol.lanes[1].channel = 9; fol.g.key = 7; fol.g.scale = 0;      // otra tonalidad propia: debe ignorarla
        lead.g.humanizeMs = fol.g.humanizeMs = 0.0f;
        lead.prepare (48000.0); fol.prepare (48000.0);
        std::vector<OutEvent> ea, eb; int checked = 0, wrong = 0;
        for (int b = 0; b < (int) (240.0 * 48000.0 / 512); ++b)
        {
            lead.process (512, false, 0.0, 120.0, ea);
            const auto h = lead.published();
            fol.following = true; fol.external = h; fol.g.key = h.key; fol.g.scale = h.scale;
            fol.process (512, false, 0.0, 120.0, eb);
            for (auto& e : eb) if ((e.data[0] & 0xF0) == 0x90 && e.data[2] > 0) { ++checked; if (! ((h.mask >> (e.data[1] % 12)) & 1)) ++wrong; }
        }
        std::printf ("  %d notas de la seguidora, %d fuera del acorde de la líder\n", checked, wrong);
        check (checked > 20 && wrong == 0, "la seguidora usa los acordes de la líder");
    }
    std::printf ("8. La misma variación con otro host: tamaño de bloque y frecuencia de muestreo\n");
    {
        auto setup = [] (Generator& g) {
            setupAmbient (g, 11);
            g.g.breathRate = 6.0f; g.g.fadeIn = 20.0f; g.g.humanizeMs = 12.0f;
            g.lanes[4].loop = true; g.lanes[4].loopBeats = 23.0f; g.lanes[2].loop = true; g.lanes[2].loopBeats = 19.0f;
            g.lanes[5].ccNum = 1; g.lanes[5].ccShape = CcSine; g.lanes[6].ccNum = 11; g.lanes[6].ccShape = CcBreath;
        };
        const double secs = 180.0;
        Generator ref; setup (ref); auto a = run (ref, secs, 512, 48000.0);
        uint64_t h = 1469598103934665603ull; int notes = 0;
        for (auto& e : a) if (e.sec < secs - 1.0)
        {
            for (uint64_t v : { (uint64_t) e.s, (uint64_t) e.d1, (uint64_t) e.d2, (uint64_t) (e.lane + 1), (uint64_t) std::llround (e.sec * 48000.0) }) { h ^= v; h *= 1099511628211ull; }
            notes += (e.s & 0xF0) == 0x90;
        }
        std::printf ("  huella %016llx (%d notas en 3 minutos)\n", (unsigned long long) h, notes);
        bool allSame = true;
        for (auto cfg : { std::pair<int, double> { 64, 48000.0 }, { 441, 48000.0 }, { 1024, 48000.0 }, { 512, 44100.0 }, { 333, 96000.0 } })
        {
            Generator g; setup (g); auto b = run (g, secs, cfg.first, cfg.second);
            std::vector<bool> used (b.size(), false); int matched = 0, total = 0;
            size_t from = 0;
            for (auto& x : a)
            {
                if (x.sec >= secs - 1.0) continue;
                ++total;
                while (from < b.size() && b[from].sec < x.sec - 0.002) ++from;
                for (size_t j = from; j < b.size() && b[j].sec <= x.sec + 0.002; ++j)
                    if (! used[j] && b[j].s == x.s && b[j].d1 == x.d1 && b[j].d2 == x.d2 && b[j].lane == x.lane) { used[j] = true; ++matched; break; }
            }
            std::printf ("  bloque %4d a %5.0f Hz: %d de %d mensajes iguales (±2 ms)\n", cfg.first, cfg.second, matched, total);
            allSame = allSame && matched == total;
        }
        check (allSame, "mismas notas y controles con cualquier bloque y frecuencia de muestreo");
    }

    std::printf ("\n%s (%d fallas)\n", fails == 0 ? "TODO OK" : "HAY FALLAS", fails);
    return fails == 0 ? 0 : 1;
}
