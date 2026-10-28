#pragma once
// La página del átomo. En vivo lee el procesador (cursor y fase del motor, acorde, notas que salen); la grabación de demos usa AtomView sin
// procesador (AtomRecorder.h).

#include "AtomView.h"
#include "Widgets.h"

namespace dh::val::ui {

class AtomPage : public juce::Component
{
public:
    explicit AtomPage (ValenceProcessor& p) : proc (p)
    {
        addAndMakeVisible (view);
        // VALENCE_ATOM_DEMO=carpeta de una demo: el átomo sigue esa demo con el reloj de pared en vez del procesador (medir
        // la ventana sin audio, p. ej. en Linux bajo Xvfb, donde no hay placa y el motor no corre).
        if (const auto d = juce::SystemStats::getEnvironmentVariable ("VALENCE_ATOM_DEMO", {}); d.isNotEmpty())
        {
            juce::String err;
            demo = std::make_unique<AtomFeed>();
            if (! demo->load (juce::File (d), err)) demo.reset();
        }
        // VALENCE_ATOM_DIRECT=1: el átomo como antes de las cachés (todo en cada cuadro), para comparar en las mediciones.
        if (juce::SystemStats::getEnvironmentVariable ("VALENCE_ATOM_DIRECT", "0").getIntValue() != 0) view.live = false;
        view.pull = [this] (AtomSnap& s, std::vector<SparkEv>& sp) { pull (s, sp); };
        view.refreshTimer();
    }
    void resized() override { view.setBounds (getLocalBounds()); }
    AtomView view;

private:
    ValenceProcessor& proc;
    AtomSnap cfg;
    double lastCfg = -1.0;

    void refreshConfig()
    {
        const auto& ps = params();
        std::vector<float> vals (ps.size());
        for (size_t k = 0; k < ps.size(); ++k) vals[k] = proc.value (ps[k].id);
        GlobalParams g; std::array<LayerParams, kLayers> lp;
        applyParams (vals.data(), g, lp);
        const auto rows = proc.getRows();
        for (int l = 0; l < kLayers; ++l) lp[(size_t) l].steps = rows[(size_t) l];
        fillAtomConfig (cfg, lp);
    }
    std::unique_ptr<AtomFeed> demo; double demoT0 = -1.0;
    void pull (AtomSnap& s, std::vector<SparkEv>& out)
    {
        const double t = AtomView::wallNow();
        if (demo != nullptr)
        {
            if (demoT0 < 0.0) demoT0 = t - 4.0;
            demo->advanceTo (t - demoT0, s, out);
            return;
        }
        if (lastCfg < 0.0 || t - lastCfg > 0.1) { refreshConfig(); lastCfg = t; }
        fillAtomView (cfg, proc.view());
        s = cfg;
        const int n = proc.sparkFifo.getNumReady();
        if (n <= 0) return;
        int s1, n1, s2, n2;
        proc.sparkFifo.prepareToRead (n, s1, n1, s2, n2);
        auto take = [&] (int start, int cnt) {
            for (int i = 0; i < cnt && out.size() < 256; ++i)
            {
                const auto& k = proc.sparks[(size_t) (start + i)];
                SparkEv e; e.layer = k.layer; e.note = k.note; e.vel = k.vel; e.nucleon = k.nucleon; e.src = k.src; e.step = k.step; e.flags = k.flags;
                out.push_back (e);
            }
        };
        take (s1, n1); take (s2, n2);
        proc.sparkFifo.finishedRead (n1 + n2);
    }
};

} // namespace dh::val::ui
