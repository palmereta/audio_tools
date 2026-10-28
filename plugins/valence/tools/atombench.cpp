// valenceatombench: el costo de dibujo del átomo sin abrir ninguna ventana. Corre una demo con el mismo motor que
// valencerecord (AtomFeed), alimenta AtomView en modo vivo a ritmo real (un cuadro cada 1/hz s de reloj) y pinta en una
// juce::Image las mismas zonas que pediría la ventana. Mide el tiempo de CPU del proceso (getrusage) y los ms por cuadro.
// No mide el piso de composición de la ventana (Core Animation en macOS, X11 en Linux): eso sólo se ve con ventana.
//
//   valenceatombench <demo dir> [mode] [hz] [secs] [scale] [img]
//     mode   none        sólo el motor y la vista (ingest + zonas), sin pintar: la base a restar
//            direct      todo el átomo en cada cuadro, como antes (precesión a velocidad completa, sin cachés)
//            cachedfull  el modo vivo (cuadro propio con cachés y sprites a escala), pero entero en cada cuadro
//            dirty       el modo vivo, y la pantalla copia la unión de las zonas (Core Graphics en macOS las une)
//            dirtylist   el modo vivo, y la pantalla copia cada zona (el repintado de JUCE en Linux)
//     hz 30, secs 20, scale 1.7 (el editor al 85 % en pantalla retina; 1 en Linux sin escala), img soft|native
//   Salida: una línea con CPU % de un núcleo (CPU de proceso / tiempo de pared), ms por cuadro (media, mediana, p95,
//   máx), fracción del área repintada y reconstrucciones de caché.
#include <JuceHeader.h>
#include <sys/resource.h>
#if JUCE_MAC
 #include <pthread/qos.h>
#endif
#include "PluginProcessor.h"
#include "ui/AtomView.h"

using namespace dh::val::ui;

static double cpuSeconds()
{
    rusage u {};
    getrusage (RUSAGE_SELF, &u);
    return (double) u.ru_utime.tv_sec + (double) u.ru_utime.tv_usec * 1.0e-6 + (double) u.ru_stime.tv_sec + (double) u.ru_stime.tv_usec * 1.0e-6;
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: valenceatombench <demo dir> [none|direct|cachedfull|dirty|dirtylist] [hz] [secs] [scale] [soft|native]\n"); return 1; }
    juce::ScopedJuceInitialiser_GUI gui;
   #if JUCE_MAC
    pthread_set_qos_class_self_np (QOS_CLASS_USER_INTERACTIVE, 0);     // como el hilo de mensajes de una app con ventana
   #endif
    const juce::String mode = argc > 2 ? argv[2] : "dirty";
    const int hz = argc > 3 ? std::atoi (argv[3]) : 30;
    const double secs = argc > 4 ? std::atof (argv[4]) : 20.0;
    const float scale = argc > 5 ? (float) std::atof (argv[5]) : 1.7f;
    const bool native = argc > 6 && juce::String (argv[6]) == "native";

    AtomFeed feed; juce::String err;
    if (! feed.load (juce::File (juce::File::getCurrentWorkingDirectory().getChildFile (argv[1])), err)) { std::printf ("error: %s\n", err.toRawUTF8()); return 1; }
    AtomView view;
    view.live = mode != "direct";
    view.setSize (1320, 766);                 // la página del átomo en coordenadas del editor (1320 x 860 menos barra y pie)
    const int W = juce::roundToInt (1320.0f * scale), H = juce::roundToInt (766.0f * scale);
    const juce::NativeImageType nativeType; const juce::SoftwareImageType softType;
    juce::Image img (juce::Image::RGB, W, H, true, native ? (const juce::ImageType&) nativeType : (const juce::ImageType&) softType);
    juce::RectangleList<int> dirty;

    AtomSnap snap; std::vector<SparkEv> sparks;
    const double from = 4.0;                  // el motor arranca antes, para entrar con las capas andando
    feed.advanceTo (from, snap, sparks);
    view.ingest (snap, {}, from);
    {   // un primer paint entero: fija la escala física, como el primer cuadro de la ventana
        juce::LowLevelGraphicsSoftwareRenderer ctx (img); ctx.addTransform (juce::AffineTransform::scale (scale));
        juce::Graphics g (ctx); view.paintEntireComponent (g, true);
    }

    std::vector<double> ms; double area = 0.0, blitMs = 0.0; int frames = 0;
    const double wall0 = juce::Time::getMillisecondCounterHiRes() * 0.001, cpu0 = cpuSeconds();
    for (int f = 0; ; ++f)
    {
        const double t = from + (double) f / hz;
        if (t > from + secs) break;
        // a ritmo real: el cuadro f sale a su hora de reloj
        static const bool fast = std::getenv ("BENCH_FAST") != nullptr;      // sin esperar: el costo por cuadro en un núcleo rápido
        const double due = fast ? 0.0 : wall0 + (double) f / hz;
        for (double nowW = juce::Time::getMillisecondCounterHiRes() * 0.001; nowW < due; nowW = juce::Time::getMillisecondCounterHiRes() * 0.001)
            juce::Thread::sleep (std::max (1, (int) ((due - nowW) * 1000.0)));
        const auto a0 = juce::Time::getHighResolutionTicks();
        feed.advanceTo (t, snap, sparks);
        view.ingest (snap, sparks, t);
        dirty.clear();
        if (mode == "direct") dirty.add (view.getLocalBounds());
        else if (mode != "none") dirty = view.liveFrame (mode == "cachedfull");
        if (mode == "dirty" && ! dirty.isEmpty()) { const auto b = dirty.getBounds(); dirty.clear(); dirty.add (b); }
        if (mode != "none" && ! dirty.isEmpty())
        {
            const auto b0 = juce::Time::getHighResolutionTicks();
            auto clip = dirty; clip.clipTo (view.getLocalBounds());
            for (auto& r : clip) area += (double) r.getWidth() * r.getHeight() / (1320.0 * 766.0);
            if (native)
            {
                juce::Graphics g (img);
                g.addTransform (juce::AffineTransform::scale (scale));
                g.reduceClipRegion (clip);
                view.paintEntireComponent (g, true);
            }
            else
            {
                juce::RectangleList<int> phys;
                for (auto& r : clip) phys.add ((r.toFloat() * scale).getSmallestIntegerContainer().getIntersection (img.getBounds()));   // el redondeo a 1,7 pasaba del borde
                juce::LowLevelGraphicsSoftwareRenderer ctx (img, {}, phys);
                ctx.addTransform (juce::AffineTransform::scale (scale));
                juce::Graphics g (ctx);
                view.paintEntireComponent (g, true);
            }
            blitMs += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - b0) * 1000.0;
        }
        ms.push_back (juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - a0) * 1000.0);
        ++frames;
    }
    const double wall = juce::Time::getMillisecondCounterHiRes() * 0.001 - wall0, cpu = cpuSeconds() - cpu0;
    auto sorted = ms; std::sort (sorted.begin(), sorted.end());
    double sum = 0; for (double m : ms) sum += m;
    std::printf ("%-10s %3d Hz scale %.2f %s: CPU %.2f %% of one core (%.2f s CPU / %.2f s wall)  frame ms avg %.3f med %.3f p95 %.3f max %.2f  screen copy %.3f  area %.0f %%  frames %d\n",
                 mode.toRawUTF8(), hz, scale, native ? "native" : "soft", 100.0 * cpu / wall, cpu, wall, sum / frames, sorted[sorted.size() / 2],
                 sorted[(size_t) (sorted.size() * 0.95)], sorted.back(), blitMs / frames, 100.0 * area / frames, frames);
    if (const auto snapPath = juce::SystemStats::getEnvironmentVariable ("BENCH_PNG", {}); snapPath.isNotEmpty())
    {
        juce::File f (snapPath); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
    }
    return 0;
}
