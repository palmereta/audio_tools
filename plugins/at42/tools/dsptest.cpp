// Banco de pruebas del DSP del AT42: clang++ -std=c++20 -O2 -ISource/dsp -I../../shared/dsp tools/dsptest.cpp
#include "Engine42.h"
#include <cstdio>
#include <vector>
using namespace dh;

static void run (Engine42& e, int n, float& peak, double& rms)
{
    std::vector<float> L (256), R (256); peak = 0; double s = 0; long c = 0;
    for (int d = 0; d < n; d += 256)
    {
        const int m = std::min (256, n - d);
        e.render (L.data(), R.data(), nullptr, nullptr, m);
        for (int i = 0; i < m; ++i)
        {
            if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) { std::printf ("NaN!\n"); std::exit (1); }
            peak = std::max (peak, std::max (std::fabs (L[i]), std::fabs (R[i]))); s += L[i] * L[i]; ++c;
        }
    }
    rms = std::sqrt (s / std::max (1L, c));
}
static int idx (const char* id) { for (int i = 0; i < kNumP42; ++i) if (std::string (panelParam42 (i).id) == id) return i; return -1; }

int main()
{
    const double sr = 48000; float pk; double r;
    {
        Engine42 e; e.prepare (sr, 512, 2); e.p[(size_t) P42::fx_blend] = 0;
        e.droneKey[1] = true; run (e, 48000, pk, r);
        std::printf ("Drone 1 (5 osc, tecla): pico %.2f rms %.3f\n", pk, r);
        e.p[(size_t) P42::d1_volt] = 0.95f; run (e, 48000, pk, r);
        std::printf ("  VOLT 9.5 (FM cruzado): pico %.2f rms %.3f\n", pk, r);
    }
    {
        Engine42 e; e.prepare (sr, 512, 2); e.p[(size_t) P42::fx_blend] = 0;
        e.p[(size_t) P42::mix_vol3] = 0.7f; e.droneKey[3] = true; run (e, 48000, pk, r);
        std::printf ("Papa Srapa 3: pico %.2f rms %.3f\n", pk, r);
    }
    {
        Engine42 e; e.prepare (sr, 512, 2); e.p[(size_t) P42::fx_blend] = 0; e.p[(size_t) P42::mix_vol5] = 0.7f;
        e.keys.press (0, 0.8f, false); run (e, 48000, pk, r);
        std::printf ("VCO A por el teclado (C3, sierra): pico %.2f rms %.3f\n", pk, r);
    }
    {
        // Todos los programas de los 13 cartuchos, con drones sonando.
        int bad = 0;
        for (int c = 0; c < 13; ++c)
        {
            std::printf ("Cartucho %2d:", c);
            for (int pr = 0; pr < 3; ++pr)
            {
                Engine42 e; e.prepare (sr, 512, 1);
                e.p[(size_t) P42::fx_cart_l] = (float) c; e.p[(size_t) P42::fx_cart_r] = (float) c;
                e.p[(size_t) P42::fx_prog_l] = (float) pr; e.p[(size_t) P42::fx_prog_r] = (float) pr;
                e.p[(size_t) P42::fx_blend] = 1.0f; e.p[(size_t) P42::fx_x] = 0.8f; e.p[(size_t) P42::fx_y] = 0.6f; e.p[(size_t) P42::fx_z] = 0.7f;
                e.droneKey[1] = e.droneKey[2] = true;
                run (e, 48000 * 2, pk, r);
                std::printf ("  p%d %.2f/%.3f", pr + 1, pk, r);
                if (r < 1e-4) ++bad;
            }
            std::printf ("\n");
        }
        std::printf ("Programas en silencio: %d\n", bad);
    }
    {
        Engine42 e; e.prepare (sr, 512, 2); Rng rg (3); float mx = 0;
        for (int b = 0; b < 300; ++b)
        {
            for (int i = 0; i < kNumP42; ++i) { const auto& pp = panelParam42 (i); e.p[(size_t) i] = pp.steps > 0 ? (float) (rg.next() % pp.steps) : rg.unipolar(); }
            for (int k = 1; k <= 6; ++k) e.droneKey[(size_t) k] = rg.next() % 2;
            for (auto& s : e.patch.src) s = -1;
            for (int c = 0; c < 8; ++c)
            {
                const int d = (int) (rg.next() % jk42::kNumJacks), s = (int) (rg.next() % jk42::kNumJacks);
                if (! jk42::jacks[d].isOutput && jk42::jacks[s].isOutput) e.patch.src[(size_t) d] = (int8_t) s;
            }
            if (b % 7 == 0) e.keys.press ((int) (rg.next() % 24), 0.7f, false); else if (b % 5 == 0) e.keys.clear();
            e.joyX = rg.bipolar(); e.joyY = rg.bipolar();
            run (e, 512, pk, r); mx = std::max (mx, pk);
        }
        std::printf ("Fuzz 300 bloques con cables al azar: pico %.2f, sin NaN\n", mx);
    }
}
