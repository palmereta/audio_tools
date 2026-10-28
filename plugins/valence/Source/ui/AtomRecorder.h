#pragma once
// VALENCE_RECORD=dir: un JPEG por cada 1/30 s de tiempo de sesión de una demo, para armar un video que coincide con el
// audio de esa demo. El motor es determinista: se corre fuera de tiempo real con el mismo preset y la misma entrada
// que valencerender (AtomFeed), así que el cuadro N muestra lo que suena en N/30 s del WAV de la demo, sin audio ni
// reloj de pared de por medio. Los JPEG se escriben en otro hilo.
//   VALENCE_DEMO=carpeta con preset.json (y input.mid, events.json)
//   VALENCE_RECORD_FROM=s (default 0)   VALENCE_RECORD_SECS=s (default 15)   VALENCE_RECORD_W / _H (1280 x 720)
//   VALENCE_RECORD_SCALE (default 1)
// Termina escribiendo dir/done.txt con la cantidad de cuadros.

#include "AtomView.h"
#include "AtomPage.h"

namespace dh::val::ui {

struct AtomRecorder : juce::Timer, juce::Thread
{
    AtomRecorder (juce::File outDir, juce::File demoDir, double from, double secs, int w, int h, float scale)
        : juce::Thread ("valence record"), dir (std::move (outDir)), t0 (from), t1 (from + secs), sc (scale)
    {
        dir.createDirectory(); dir.getChildFile ("done.txt").deleteFile(); dir.getChildFile ("error.txt").deleteFile();
        juce::String err;
        if (! feed.load (demoDir, err)) { dir.getChildFile ("error.txt").replaceWithText (err); ok = false; return; }
        view.live = false;
        view.setSize (w, h);
        startThread();
        startTimer (1);
    }
    ~AtomRecorder() override { stopTimer(); signalThreadShouldExit(); notify(); stopThread (15000); }

    void timerCallback() override
    {
        if (! ok) { stopTimer(); return; }
        { const juce::ScopedLock sl (lock); if (queue.size() > 24) return; }
        for (int burst = 0; burst < 4; ++burst)
        {
            const double t = (double) frame / 30.0;
            if (t > t1) { stopTimer(); if (logSparks) dir.getChildFile ("sparks.txt").replaceWithText (sparkLog);
            if (bench && ! benchMs.empty())
            {
                std::sort (benchMs.begin(), benchMs.end());
                double sum = 0; for (double m : benchMs) sum += m;
                const double avg = sum / (double) benchMs.size(), med = benchMs[benchMs.size() / 2], p95 = benchMs[(size_t) ((double) benchMs.size() * 0.95)];
                dir.getChildFile ("bench.txt").replaceWithText (juce::String::formatted ("frames %d  avg %.3f ms  median %.3f ms  p95 %.3f ms  max %.3f ms  => %.2f %% of one core at 60 Hz (avg)\n", (int) benchMs.size(), avg, med, p95, benchMs.back(), avg * 60.0 / 10.0));
            } finishing = true; notify(); return; }
            feed.advanceTo (t, snap, sparks);
            const auto a0 = juce::Time::getHighResolutionTicks();
            view.ingest (snap, sparks, t);
            if (logSparks) for (auto& e : sparks) sparkLog << juce::String (t - (double) e.age, 5) << ' ' << (e.layer + 1) << ' ' << e.note << ' ' << e.vel << ' ' << e.step << '\n';
            if (t >= t0 - 1.0e-9 && bench)
            {
                // VALENCE_BENCH=1: sólo el costo de un cuadro (datos + pintado), sin escribir nada
                auto img = view.createComponentSnapshot (view.getLocalBounds(), true, sc);
                const double ms = juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - a0) * 1000.0;
                benchMs.push_back (ms);
            }
            else if (t >= t0 - 1.0e-9)
            {
                auto img = view.createComponentSnapshot (view.getLocalBounds(), true, sc);
                const juce::ScopedLock sl (lock);
                queue.push_back ({ frame - firstFrame (), img });
            }
            ++frame;
        }
    }
    void run() override
    {
        while (! threadShouldExit())
        {
            wait (30);
            drain();
            if (finishing && queueEmpty()) { dir.getChildFile ("done.txt").replaceWithText (juce::String (written)); finishing = false; done = true; }
        }
        drain();
    }
    std::atomic<bool> done { false };

private:
    struct Item { int idx; juce::Image img; };
    juce::File dir; double t0, t1; float sc;
    AtomFeed feed; AtomView view; AtomSnap snap; std::vector<SparkEv> sparks;
    int frame = 0; int written = 0; bool ok = true;
    bool logSparks = juce::SystemStats::getEnvironmentVariable ("VALENCE_RECORD_LOG", "0").getIntValue() != 0;
    bool bench = juce::SystemStats::getEnvironmentVariable ("VALENCE_BENCH", "0").getIntValue() != 0;
    std::vector<double> benchMs;
    juce::String sparkLog;       // dir/sparks.txt: hora, capa, nota, velocidad, paso (para comparar con events.json de la demo)
    std::atomic<bool> finishing { false };
    juce::CriticalSection lock; std::vector<Item> queue;
    int firstFrame() const { return (int) std::ceil (t0 * 30.0 - 1.0e-9); }
    bool queueEmpty() { const juce::ScopedLock sl (lock); return queue.empty(); }
    void drain()
    {
        for (;;)
        {
            Item it;
            { const juce::ScopedLock sl (lock); if (queue.empty()) return; it = queue.front(); queue.erase (queue.begin()); }
            juce::JPEGImageFormat fmt; fmt.setQuality (0.92f);
            auto f = dir.getChildFile (juce::String::formatted ("f%05d.jpg", it.idx));
            f.deleteFile();
            juce::FileOutputStream os (f);
            if (os.openedOk()) fmt.writeImageToStream (it.img, os);
            ++written;
        }
    }
};

} // namespace dh::val::ui
