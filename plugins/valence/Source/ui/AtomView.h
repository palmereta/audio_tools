#pragma once
// El átomo de Valence, dirección A "Bohr luminoso": órbitas finas de luz sobre negro, electrones como cometas, fotones
// como rayos. Qué es cada cosa (plan, sección 4): protones = notas apretadas (cálidos), neutrones = notas que agrega el
// plugin (fríos), órbita = capa (el radio es el registro), marcas = pasos (opacidad = probabilidad), electrón = cursor
// de la capa, fotón = nota que suena, fusión = todas las capas vuelven juntas al paso 0.
// (Las direcciones B "nube cuántica" y C "cámara de niebla" quedaron en la historia de git, commit 6607db9.)
//
// Dos modos de pintar, con las mismas funciones de dibujo:
//  - En vivo (live = true): lo quieto va a dos imágenes en caché (fondo y mitad de atrás de las órbitas con sus marcas;
//    mitad de adelante), que se rehacen sólo si cambian los datos o el tamaño, o cuando la precesión (mucho más lenta
//    que en los videos) movió las órbitas ~1 px. Por cuadro se repintan sólo las regiones que cambian (electrones con
//    su estela, núcleo, fotones, marcas encendidas, textos), y si nada cambia no se repinta. La cadencia es 24 Hz
//    tocando y 10 Hz parado (sólo gira el núcleo). En macOS cada cuadro tiene un piso fijo de Core Animation que no
//    depende de lo que se dibuje (README, Atom view): por eso importa tanto no repintar de más.
//  - Grabación (live = false, VALENCE_RECORD): todo se dibuja en cada cuadro con la precesión a velocidad completa; es
//    el aspecto de los videos.
// El cuadro sólo depende de (datos, tiempo) y de tablas fijas, sin estado por partícula ni caminos nuevos por cuadro.

#include "AtomData.h"
#include "Look.h"
#include "../../../../shared/ui/Showing.h"

namespace dh::val::ui {

class AtomView : public juce::Component, private juce::Timer
{
public:
    // Fuente en vivo: la llama el temporizador (sólo si la vista está a la vista).
    std::function<void (AtomSnap&, std::vector<SparkEv>&)> pull;
    bool live = true;          // false al grabar: sin cachés ni extrapolación, precesión a velocidad completa

    AtomView()
    {
        setOpaque (true);
        setRepaintsOnMouseActivity (false);
        for (int l = 0; l < kLayers; ++l) glow[(size_t) l] = makeGlow (col::layer (l));
        glowSun = makeGlow (col::sun); glowCold = makeGlow (col::cold); glowWhite = makeGlow (juce::Colour (0xfffff4d8));
        sphereP = makeSphere (col::sun); sphereN = makeSphere (col::cold);
    }
    ~AtomView() override { stopTimer(); }

    void refreshTimer() { updateTimer(); }
    static int hz() { static const int h = std::clamp (juce::SystemStats::getEnvironmentVariable ("VALENCE_ATOM_HZ", "24").getIntValue(), 5, 120); return h; }
    static constexpr int idleHz = 10;

    // Una instantánea nueva y las notas que sonaron desde la anterior, a la hora `t` (segundos).
    void ingest (const AtomSnap& s, const std::vector<SparkEv>& sparks, double t)
    {
        const double dt = lastNow > -1.0e8 ? std::clamp (t - lastNow, 0.0, 0.1) : 0.0;
        lastNow = now = t;
        snap = s;
        if (snap.running) driftClock += dt * kDrift;
        for (int l = 0; l < kLayers; ++l)
        {
            auto& A = snap.L[(size_t) l];
            const float raw = A.phase;
            if (raw != lastPhaseRaw[(size_t) l] || ! live) { lastPhaseRaw[(size_t) l] = raw; phaseSeen[(size_t) l] = t; }
            float est = raw;
            if (live && snap.running && A.on)     // entre bloques de audio el cursor sigue por su cuenta, según el reloj
            {
                const double cycleSec = A.cycleBeats * 60.0 / std::max (20.0, snap.bpm);
                est = (float) std::fmod ((double) raw + std::min (t - phaseSeen[(size_t) l], 0.05) / cycleSec, 1.0);
            }
            A.phase = est;
            auto& T = trail[(size_t) l];
            if (! snap.running || ! A.on) T.n = 0;
            else
            {
                T.t[(size_t) T.head] = t; T.ph[(size_t) T.head] = est; T.head = (T.head + 1) % kTrail; T.n = std::min (T.n + 1, kTrail);
            }
        }
        if (snap.chordSerial != lastChord) { kickNucleus (snap.chordSerial); lastChord = snap.chordSerial; }
        stepNucleus ((float) dt);
        computeFrame();
        for (auto& e : sparks) spawn (e);
        // Fusión: la que el motor predijo ya pasó.
        int on = 0; for (auto& A : snap.L) on += A.on ? 1 : 0;
        if (snap.running && pendingFusion >= 0 && snap.beat * (double) kTPB + 0.5 >= (double) pendingFusion && on >= 2) fusionAt = t;
        pendingFusion = snap.running ? snap.fusionTick : -1;
        if (! snap.running) fusionAt = -1.0e9;
    }

    void paint (juce::Graphics& g) override;
    void resized() override { bgDirty = true; staticDirty = true; geoKey = {}; }
    void visibilityChanged() override { updateTimer(); }
    void parentHierarchyChanged() override { updateTimer(); }
    void mouseMove (const juce::MouseEvent& e) override
    {
        int best = -1; float bd = 1.0e9f;
        for (int i = 0; i < nCount; ++i)
        {
            const float d = fs.nuc[(size_t) i].p.getDistanceFrom (e.position);
            if (d < fs.nuc[(size_t) i].r * 1.4f && d < bd) { bd = d; best = i; }
        }
        if (best != hover) { hover = best; fullDirty = true; }
    }
    void mouseExit (const juce::MouseEvent&) override { if (hover >= 0) { hover = -1; fullDirty = true; } }

    // En vivo, después de ingest: qué cambió (lo que se mueve ahora y lo que se pintó en el cuadro anterior, para
    // borrarlo), lo dibuja en el cuadro propio y devuelve esas zonas para repintar. El temporizador las pasa a repaint();
    // el banco de pruebas (tools/atombench.cpp) las usa igual, sin ventana.
    juce::RectangleList<int> liveFrame (bool whole = false)
    {
        auto d = dirtyRegions();
        if (whole) d = getLocalBounds();
        if (! d.isEmpty()) renderLive (d);
        return d;
    }

private:
    juce::RectangleList<int> dirtyRegions()
    {
        juce::RectangleList<int> cur, out;
        Comet C;
        for (int l = 0; l < kLayers; ++l) if (comet (l, C)) cur.add (cometBounds (C).getSmallestIntegerContainer());
        if (nCount > 0) cur.add (nucleusBounds().getSmallestIntegerContainer());
        for (auto& p : photons) if (photonAlive (p)) cur.add (photonBounds (p).getSmallestIntegerContainer());
        litBounds (cur);
        juce::String sig[3]; hudSigs (sig);
        for (int i = 0; i < 3; ++i)
            if (hud[(size_t) i].sig != sig[i] && now - hud[(size_t) i].built >= 0.12) cur.add (hudRect (i));
        if (fullDirty || staticDirty || fusionActive() || fb.isNull()) { fullDirty = fusionActive(); out.add (getLocalBounds()); }     // y un cuadro entero más al terminar la fusión
        else
        {
            out = cur;
            out.add (prevDirty);
            out.clipTo (getLocalBounds());
        }
        prevDirty = std::move (cur);
        return out;
    }

public:
    // La hora de pared en segundos, para quien alimenta en vivo.
    static double wallNow() { return juce::Time::getMillisecondCounterHiRes() * 0.001; }

private:
    using V2 = juce::Point<float>;
    struct V3 { float x = 0, y = 0, z = 0; };
    struct Proj { V2 p; float z = 0, s = 1; };
    static constexpr int kTrail = 48, kPhotons = 160, kComet = 9;

    // ───────── órbitas: radios, inclinaciones y precesiones distintos (nada de simetría perfecta) ─────────
    struct OrbitDef { float ecc, incl, node, omega, nodeRate, omegaRate, regOff; };
    static constexpr OrbitDef OD[kLayers] = {
        { 0.80f,  0.60f,  0.35f,  0.20f,  0.045f, -0.020f, 0.00f },
        { 0.62f, -0.95f,  1.90f,  1.10f, -0.030f,  0.035f, 0.025f },
        { 0.88f,  1.20f, -0.80f,  2.60f,  0.025f,  0.050f, 0.055f },
        { 0.70f,  0.15f,  2.70f, -0.70f, -0.050f, -0.030f, 0.09f } };
    // En vivo la precesión anda a esta fracción de la de los videos y avanza a saltos de ~kDriftPx: cada salto rehace
    // las cachés y repinta la vista entera. Con 0.03 (un salto por segundo) eso era un tercio del costo de la vista
    // (atombench); con 0.005 hay un salto cada ~6 s y las órbitas giran unos grados en diez minutos.
    static constexpr float kDrift = 0.005f, kDriftPx = 0.6f, kMaxRate = 0.11f;   // kMaxRate: la velocidad angular más alta (rad/s)

    struct FrameState
    {
        float cx = 0, cy = 0, R = 100, f = 320;
        std::array<V3, kLayers> U {}, W {};
        float gcx = 1, gsx = 0, gcy = 1, gsy = 0;       // inclinación y balanceo global del átomo
        struct Nuc { V2 p; float z = 0, r = 4, s = 1; int kind = 0, note = 60; } ;
        std::array<Nuc, kMaxPool * 2> nuc {};
        std::array<int, kMaxPool * 2> order {};
    } fs;

    struct Trail { std::array<double, kTrail> t {}; std::array<float, kTrail> ph {}; int head = 0, n = 0; };
    struct Photon { double t0 = -1.0e9; int layer = 0, slot = -1; float vel = 0.5f; bool accent = false; V2 a; };
    // La estela de un electrón, como la dibuja drawElectron: la cabeza y hasta kComet puntos hacia atrás.
    struct Comet { V2 pos; float z = 0, s = 1; int n = 0; std::array<V2, kComet> p {}; std::array<float, kComet> zz {}, ss {}; };

    AtomSnap snap;
    double now = 0, lastNow = -1.0e9, fusionAt = -1.0e9;
    int hover = -1, nCount = 0;
    int64_t pendingFusion = -1;
    uint32_t lastChord = 0xffffffffu;
    std::array<float, kLayers> lastPhaseRaw {};
    std::array<double, kLayers> phaseSeen {};
    std::array<Trail, kLayers> trail {};
    std::array<std::array<double, kMaxSteps>, kLayers> stepLit {};
    std::array<double, kMaxPool * 2> nucHit {};
    std::array<Photon, kPhotons> photons {};
    int photonHead = 0;
    // núcleo: un resorte por nucleón
    struct Spring { V3 p, v; };
    std::array<Spring, kMaxPool * 2> spring {};

    // sprites, fondo y cachés
    std::array<juce::Image, kLayers> glow;
    juce::Image glowSun, glowCold, glowWhite, sphereP, sphereN;
    juce::Image bg; bool bgDirty = true;
    int bgW = 0, bgH = 0; float physScale = 1.0f;
    std::array<juce::Path, kLayers> pBack, pFront;
    // en vivo: lo quieto, en dos capas (el núcleo y los electrones de atrás van entre las dos)
    juce::Image stBack, stFront, stAll; bool staticDirty = true;     // stAll = las dos juntas, para donde no hay nada atrás
    juce::Image fb;                                // en vivo: el cuadro entero, en píxeles físicos (se pinta acá y la pantalla lo copia)
    std::vector<float> geoKey;                     // lo que define las órbitas y las marcas: si cambia, se rehacen las cachés
    double geoT = 0.0;                             // la hora de la geometría (en vivo, la precesión lenta y a saltos)
    double driftClock = 0.0;                       // en vivo: avanza a kDrift de la hora, y sólo tocando (parado el átomo queda quieto)
    juce::RectangleList<int> prevDirty;           // lo que se pintó dinámico en el cuadro anterior (hay que borrarlo)
    bool fullDirty = true;

    // ───────── herramientas ─────────
    static float hash01 (uint32_t x)
    {
        x ^= x >> 16; x *= 0x7feb352dU; x ^= x >> 15; x *= 0x846ca68bU; x ^= x >> 16;
        return (float) (x & 0xffffff) / 16777216.0f;
    }
    static juce::Image makeGlow (juce::Colour c)
    {
        juce::Image im (juce::Image::ARGB, 64, 64, true);
        juce::Graphics g (im);
        juce::ColourGradient gr (c.withAlpha (1.0f), 32, 32, c.withAlpha (0.0f), 0, 32, true);
        gr.addColour (0.10, c.brighter (0.5f).withAlpha (0.95f)); gr.addColour (0.25, c.withAlpha (0.45f)); gr.addColour (0.5, c.withAlpha (0.12f)); gr.addColour (0.75, c.withAlpha (0.03f));
        g.setGradientFill (gr); g.fillRect (0, 0, 64, 64);
        return im;
    }
    static juce::Image makeSphere (juce::Colour c)
    {
        juce::Image im (juce::Image::ARGB, 96, 96, true);
        juce::Graphics g (im);
        juce::ColourGradient gr (c.brighter (0.9f), 36, 33, c.darker (0.85f), 92, 92, true);
        gr.addColour (0.35, c); gr.addColour (0.8, c.darker (0.55f));
        g.setGradientFill (gr); g.fillEllipse (4, 4, 88, 88);
        g.setColour (juce::Colours::white.withAlpha (0.25f)); g.fillEllipse (28, 20, 22, 14);
        return im;
    }
    void sprite (juce::Graphics& g, const juce::Image& im, V2 c, float size, float alpha)
    {
        if (alpha <= 0.003f || size < 1.0f) return;
        const float n = size * physScale;
        if (px != nullptr && n <= 320.0f)
        {
            // En vivo: el sprite ya escalado a su tamaño en píxeles (de a 2) y en una posición entera, así el dibujo es una
            // copia con mezcla y no un remuestreo (que era lo que más costaba). Medio píxel de corrimiento no se ve en un brillo.
            const int ni = std::max (2, 2 * juce::roundToInt (n * 0.5f));
            px->setOpacity (juce::jlimit (0.0f, 1.0f, alpha));
            px->drawImageAt (scaledSprite (im, ni), juce::roundToInt (c.x * physScale - (float) ni * 0.5f), juce::roundToInt (c.y * physScale - (float) ni * 0.5f));
            return;
        }
        g.setOpacity (juce::jlimit (0.0f, 1.0f, alpha));
        g.drawImage (im, juce::Rectangle<float> (size, size).withCentre (c), juce::RectanglePlacement::stretchToFit);
    }
    juce::Graphics* px = nullptr;                  // en vivo: el contexto en píxeles del cuadro propio
    std::map<std::pair<const void*, int>, juce::Image> scaled;
    const juce::Image& scaledSprite (const juce::Image& im, int n)
    {
        auto& out = scaled[{ im.getPixelData().get(), n }];
        if (out.isNull())
        {
            out = juce::Image (juce::Image::ARGB, n, n, true, juce::SoftwareImageType());
            juce::Graphics g (out);
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (im, juce::Rectangle<float> ((float) n, (float) n), juce::RectanglePlacement::stretchToFit);
        }
        return out;
    }
    static juce::Rectangle<float> around (V2 c, float r) { return { c.x - r, c.y - r, 2 * r, 2 * r }; }

    void updateTimer()
    {
        if (! pull) { stopTimer(); return; }
        if (isVisible() && getParentComponent() != nullptr) startTimerHz (hz()); else stopTimer();
    }
    void timerCallback() override
    {
        const bool log = std::getenv ("VALENCE_FPS") != nullptr;
        // Live esconde la ventana del plugin sin cerrarla: mientras no se vea, sólo un vigía lento, sin repintar.
        if (! dh::reallyShowing (*this)) { if (getTimerInterval() != 500) { startTimer (500); fullDirty = true; if (log) std::fprintf (stderr, "atom view: hidden, timer slowed to 2 Hz\n"); } return; }
        // Parado no hay electrones: sólo gira el núcleo y se apagan los fotones; 10 Hz alcanzan.
        const int want = snap.running || probe() != 0 ? hz() : idleHz;
        if (getTimerInterval() != 1000 / want) { startTimerHz (want); if (log) std::fprintf (stderr, "atom view: showing, timer at %d Hz\n", want); }
        std::vector<SparkEv>& sp = pullSparks;
        sp.clear();
        pull (pullSnap, sp);
        ingest (pullSnap, sp, wallNow());
        if (probe() == 1) repaint();                                                  // ver probe()
        else if (probe() == 2) repaint (getWidth() / 2 - 4, getHeight() / 2 - 4, 8, 8);
        else if (! live) repaint();
        else for (auto& r : liveFrame()) repaint (r);
        if (fpsT0 == 0.0) fpsT0 = wallNow();
        if (++ticks >= 150)
        {
            if (log) std::fprintf (stderr, "atom view: %.1f ticks/s, %.1f paints/s (%.0f %% full, clip %.0f %% of the view), paint %.3f ms avg, render %.3f ms avg, %d cache rebuilds (%.2f ms avg)\n",
                                   ticks / (wallNow() - fpsT0), paints / (wallNow() - fpsT0), paints > 0 ? 100.0 * fulls / paints : 0.0, paints > 0 ? 100.0 * clipArea / paints : 0.0,
                                   paints > 0 ? paintMs / paints : 0.0, renders > 0 ? renderMs / renders : 0.0, rebuilds, rebuilds > 0 ? rebuildMs / rebuilds : 0.0);
            ticks = paints = fulls = rebuilds = renders = 0; paintMs = rebuildMs = clipArea = renderMs = 0.0; fpsT0 = wallNow();
        }
    }
    AtomSnap pullSnap;
    // VALENCE_ATOM_PROBE (mediciones del piso de la ventana, README): 1 repinta la vista entera de negro en cada cuadro,
    // 2 repinta sólo 8 x 8 píxeles. Lo que cuesta así es lo que cobra el sistema de ventanas por cuadro, sin dibujo.
    // VALENCE_ATOM_SCALE: tope de la escala del cuadro en vivo (sin definir = sin tope). Pendiente de medir con ventana en
    // macOS: ahí el escalado final lo hace Core Graphics; en el banco lo hace el renderizador de JUCE y sale más caro.
    static float liveMaxScale() { static const float s = juce::SystemStats::getEnvironmentVariable ("VALENCE_ATOM_SCALE", "0").getFloatValue(); return s > 0.0f ? s : 100.0f; }
    static int probe() { static const int p = juce::SystemStats::getEnvironmentVariable ("VALENCE_ATOM_PROBE", "0").getIntValue(); return p; }
    std::vector<SparkEv> pullSparks;
    int ticks = 0, paints = 0, fulls = 0, rebuilds = 0, renders = 0; double fpsT0 = 0.0, paintMs = 0.0, rebuildMs = 0.0, clipArea = 0.0, renderMs = 0.0;   // VALENCE_FPS=1

    // ───────── geometría ─────────
    V3 rotX (V3 v, float c, float s) const { return { v.x, v.y * c - v.z * s, v.y * s + v.z * c }; }
    V3 rotY (V3 v, float c, float s) const { return { v.x * c + v.z * s, v.y, -v.x * s + v.z * c }; }
    V3 rotZ (V3 v, float c, float s) const { return { v.x * c - v.y * s, v.x * s + v.y * c, v.z }; }
    V3 global (V3 v) const { return rotX (rotY (v, fs.gcy, fs.gsy), fs.gcx, fs.gsx); }
    Proj project (V3 v) const
    {
        const float s = fs.f / (fs.f - v.z);
        return { { fs.cx + v.x * s, fs.cy + v.y * s }, v.z, s };
    }
    V3 orbitPoint (int l, float th) const
    {
        const float c = std::cos (th), s = std::sin (th);
        const auto &U = fs.U[(size_t) l], &W = fs.W[(size_t) l];
        return { U.x * c + W.x * s, U.y * c + W.y * s, U.z * c + W.z * s };
    }
    float orbitRadius (int l) const
    {
        const auto& A = snap.L[(size_t) l];
        const float reg = (float) A.octave + 0.5f * (float) (A.octaves - 1);
        return fs.R * juce::jlimit (0.36f, 1.0f, 0.60f + 0.085f * reg + OD[l].regOff);
    }
    float nucleonRadius() const { return fs.R * 0.050f; }

    void computeFrame()
    {
        const auto b = getLocalBounds().toFloat();
        fs.cx = b.getCentreX(); fs.cy = b.getCentreY() + b.getHeight() * 0.01f;
        fs.R = std::min (b.getHeight() * 0.5f, b.getWidth() * 0.5f) * 0.97f;
        fs.f = fs.R * 3.4f;
        if (live)
        {
            const double q = kDriftPx / (kMaxRate * std::max (1.0f, fs.R));          // segundos de precesión por salto
            geoT = std::floor (driftClock / q) * q;
        }
        else geoT = now;
        // lo que define las órbitas y las marcas
        geoBuf.clear();
        geoBuf.push_back ((float) geoT); geoBuf.push_back (b.getWidth()); geoBuf.push_back (b.getHeight());
        for (auto& A : snap.L)
        {
            geoBuf.push_back (A.on ? 1.0f : 0.0f); geoBuf.push_back ((float) A.length); geoBuf.push_back ((float) A.octave); geoBuf.push_back ((float) A.octaves);
            for (int k = 0; k < A.length; ++k) { geoBuf.push_back (A.hit[(size_t) k]); geoBuf.push_back ((float) A.flags[(size_t) k]); }
        }
        if (geoBuf != geoKey)
        {
            geoKey = geoBuf; staticDirty = true;
            const float tg = (float) geoT;
            const float tilt = 0.50f, wob = 0.20f * std::sin (tg * 0.07f) + 0.15f * std::sin (tg * 0.031f + 1.0f);
            fs.gcx = std::cos (tilt); fs.gsx = std::sin (tilt); fs.gcy = std::cos (wob); fs.gsy = std::sin (wob);
            for (int l = 0; l < kLayers; ++l)
            {
                const auto& d = OD[l];
                const float a = orbitRadius (l), bb = a * d.ecc;
                const float om = d.omega + d.omegaRate * tg, nd = d.node + d.nodeRate * tg;
                const float ci = std::cos (d.incl), si = std::sin (d.incl);
                auto xf = [&] (V3 v) {
                    v = rotZ (v, std::cos (om), std::sin (om)); v = rotX (v, ci, si); v = rotZ (v, std::cos (nd), std::sin (nd));
                    return global (v);
                };
                fs.U[(size_t) l] = xf ({ a, 0, 0 }); fs.W[(size_t) l] = xf ({ 0, bb, 0 });
                buildOrbit (l);
            }
        }
        // núcleo (gira a su ritmo, también en vivo)
        const int np = std::min (snap.protons, (int) kMaxPool), nn = std::min (snap.neutrons, (int) kMaxPool);
        nCount = np + nn;
        const float ry = (float) now * 0.33f, rx = 0.35f + 0.12f * std::sin ((float) now * 0.21f);
        for (int i = 0; i < nCount; ++i)
        {
            V3 p = rotX (rotY (spring[(size_t) i].p, std::cos (ry), std::sin (ry)), std::cos (rx), std::sin (rx));
            p = { p.x * fs.R, p.y * fs.R, p.z * fs.R };
            const auto q = project (global (p));
            auto& n = fs.nuc[(size_t) i];
            n.p = q.p; n.z = q.z; n.s = q.s; n.r = nucleonRadius() * q.s; n.kind = i < np ? 0 : 1;
            n.note = i < np ? snap.protonNotes[(size_t) i] : snap.neutronNotes[(size_t) (i - np)];
            fs.order[(size_t) i] = i;
        }
        std::sort (fs.order.begin(), fs.order.begin() + nCount, [this] (int a, int b) { return fs.nuc[(size_t) a].z < fs.nuc[(size_t) b].z; });
    }
    std::vector<float> geoBuf;

    void buildOrbit (int l)
    {
        auto& B = pBack[(size_t) l]; auto& F = pFront[(size_t) l];
        B.clear(); F.clear();
        constexpr int N = 96;
        bool prevFront = true;
        for (int j = 0; j <= N; ++j)
        {
            const auto q = project (orbitPoint (l, juce::MathConstants<float>::twoPi * (float) j / (float) N));
            const bool fr = q.z >= 0.0f;
            if (j == 0) (fr ? F : B).startNewSubPath (q.p);
            else if (fr == prevFront) (fr ? F : B).lineTo (q.p);
            else { (prevFront ? F : B).lineTo (q.p); (fr ? F : B).startNewSubPath (q.p); }
            prevFront = fr;
        }
    }

    // El núcleo: objetivos en una esfera de Fibonacci con un poco de desorden; un resorte lleva a cada nucleón.
    V3 nucleonTarget (int i, int n) const
    {
        if (n <= 1) return {};
        const float rc = 0.050f * (0.45f + 0.85f * std::cbrt ((float) n));
        const float y = 1.0f - 2.0f * ((float) i + 0.5f) / (float) n;
        const float r = std::sqrt (std::max (0.0f, 1.0f - y * y)), phi = (float) i * 2.3999632f;
        const float j = 0.80f + 0.35f * hash01 ((uint32_t) i * 7919u + 13u);
        return { r * std::cos (phi) * rc * j, y * rc * j, r * std::sin (phi) * rc * j };
    }
    void kickNucleus (uint32_t serial)
    {
        for (int i = 0; i < kMaxPool * 2; ++i)
            spring[(size_t) i].v = { (hash01 (serial * 131u + (uint32_t) i * 17u) - 0.5f) * 1.5f, (hash01 (serial * 71u + (uint32_t) i * 29u + 5u) - 0.5f) * 1.5f,
                                     (hash01 (serial * 53u + (uint32_t) i * 43u + 9u) - 0.5f) * 1.5f };
    }
    void stepNucleus (float dt)
    {
        const int n = std::min (snap.protons, (int) kMaxPool) + std::min (snap.neutrons, (int) kMaxPool);
        int steps = (int) std::ceil (dt / (1.0f / 120.0f)); steps = std::clamp (steps, 0, 12);
        const float h = steps > 0 ? dt / (float) steps : 0.0f;
        for (int i = 0; i < n; ++i)
        {
            auto& s = spring[(size_t) i]; const auto t = nucleonTarget (i, n);
            for (int k = 0; k < steps; ++k)
            {
                const V3 a { -62.0f * (s.p.x - t.x) - 7.5f * s.v.x, -62.0f * (s.p.y - t.y) - 7.5f * s.v.y, -62.0f * (s.p.z - t.z) - 7.5f * s.v.z };
                s.v = { s.v.x + a.x * h, s.v.y + a.y * h, s.v.z + a.z * h };
                s.p = { s.p.x + s.v.x * h, s.p.y + s.v.y * h, s.p.z + s.v.z * h };
            }
        }
        if (dt == 0.0f) for (int i = 0; i < n; ++i) spring[(size_t) i].p = nucleonTarget (i, n);
    }
    int slotFor (const SparkEv& e) const
    {
        const int np = std::min (snap.protons, (int) kMaxPool), nn = std::min (snap.neutrons, (int) kMaxPool);
        if (e.nucleon == 0) { for (int i = 0; i < np; ++i) if (snap.protonNotes[(size_t) i] == e.src) return i; }
        else for (int i = 0; i < nn; ++i) if (snap.neutronNotes[(size_t) i] == e.src) return np + i;
        return nCount > 0 ? (int) ((uint32_t) e.note % (uint32_t) nCount) : -1;
    }
    V2 electronPos (int l, float phase, float* z = nullptr, float* s = nullptr) const
    {
        const auto q = project (orbitPoint (l, juce::MathConstants<float>::twoPi * phase));
        if (z != nullptr) *z = q.z;
        if (s != nullptr) *s = q.s;
        return q.p;
    }

    void spawn (const SparkEv& e)
    {
        if (e.layer < 0 || e.layer >= kLayers) return;
        const auto& A = snap.L[(size_t) e.layer];
        const int len = std::max (1, A.length);
        const float ph = (float) (e.step % len) / (float) len;
        auto& p = photons[(size_t) photonHead]; photonHead = (photonHead + 1) % kPhotons;
        p.t0 = now - (double) e.age; p.layer = e.layer; p.slot = slotFor (e); p.vel = (float) e.vel / 127.0f; p.accent = (e.flags & FlagAccent) != 0;
        p.a = electronPos (e.layer, ph);
        stepLit[(size_t) e.layer][(size_t) (e.step % len) % kMaxSteps] = p.t0;
        if (p.slot >= 0) nucHit[(size_t) p.slot] = p.t0;
    }

    // ───────── fondo ─────────
    void ensureBackground (const juce::Graphics& g)
    {
        float sc = g.getInternalContext().getPhysicalPixelScaleFactor() > 0.0f ? g.getInternalContext().getPhysicalPixelScaleFactor() : 1.0f;
        // En vivo el cuadro propio se puede dibujar a menos resolución que la pantalla (liveMaxScale) y escalarse al copiarlo:
        // el dibujo cuesta según los píxeles (atombench a 1,7: 9 % de un núcleo; a 1,0: 3,5 %), más el escalado.
        if (live) sc = std::min (sc, liveMaxScale());
        const int w = juce::roundToInt ((float) getWidth() * sc), h = juce::roundToInt ((float) getHeight() * sc);
        if (sc != physScale) { staticDirty = true; fullDirty = true; }
        physScale = sc;
        if (! bgDirty && w == bgW && h == bgH && bg.isValid()) return;
        bgDirty = false; bgW = w; bgH = h; staticDirty = true;
        bg = juce::Image (juce::Image::RGB, w, h, false);
        juce::Graphics gb (bg);
        const juce::Point<float> c ((float) w * 0.5f, (float) h * 0.5f);
        juce::ColourGradient gr (juce::Colour (0xff0b0e24), c, juce::Colour (0xff030409), { 0.0f, 0.0f }, true);
        gr.addColour (0.55, juce::Colour (0xff070917)); gb.setGradientFill (gr); gb.fillAll();
        for (int i = 0; i < 160; ++i)           // estrellas fijas, muy tenues
        {
            const float x = hash01 ((uint32_t) i * 2u + 1u) * (float) w, y = hash01 ((uint32_t) i * 2u + 2u) * (float) h, a = 0.06f + 0.22f * hash01 ((uint32_t) i + 700u);
            gb.setColour (juce::Colour (0xffc9d2ff).withAlpha (a)); gb.fillRect (x, y, (i % 7 == 0 ? 2.0f : 1.0f), (i % 7 == 0 ? 2.0f : 1.0f));
        }
    }

    // ───────── dibujo ─────────
    void orbitStrokes (juce::Graphics& g, int l, bool front, float aBack, float aFront, float fl)
    {
        const auto c = col::layer (l);
        const bool on = snap.L[(size_t) l].on;
        const float k = on ? 1.0f : 0.35f;
        if (front)
        {
            if (on) { g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, aFront * 0.10f * (1.0f + 2.0f * fl)))); g.strokePath (pFront[(size_t) l], juce::PathStrokeType (5.0f)); }
            g.setColour (c.withAlpha (juce::jlimit (0.0f, 1.0f, aFront * k * (1.0f + 0.5f * fl)))); g.strokePath (pFront[(size_t) l], juce::PathStrokeType (1.4f));
        }
        else { g.setColour (c.withAlpha (aBack * k)); g.strokePath (pBack[(size_t) l], juce::PathStrokeType (1.0f)); }
    }
    // En vivo la fusión corre al doble (dura ~0,9 s en vez de ~1,8): mientras dura se repinta el átomo entero, y era el
    // pico de CPU con capas cortas que se realinean seguido (atombench, Reese Engine). La grabación queda a tiempo real.
    double fusionAge() const { return (now - fusionAt) * (live ? 2.0 : 1.0); }
    float fusionGlow() const { const double a = fusionAge(); return a < 0 || a > 3.0 ? 0.0f : (float) std::exp (-a * 2.0); }
    // En vivo la fusión se repinta entera mientras se vea: hasta que el destello cae por debajo de 2 %.
    bool fusionActive() const { const double a = fusionAge(); return a >= 0.0 && a < 1.8; }
    void drawFusion (juce::Graphics& g)
    {
        const double a = fusionAge();
        if (a < 0.0 || a > 3.0) return;
        const float f = (float) std::exp (-a * 2.2);
        const V2 c (fs.cx, fs.cy);
        if (px != nullptr)
        {
            // En vivo, la fusión liviana: un solo brillo de tamaño fijo (una mezcla de un sprite ya escalado, en caché) y el
            // anillo. Los dos brillos que crecen de la grabación pasan de 320 px y se remuestreaban en cada cuadro.
            const int ni = 2 * juce::roundToInt (fs.R * 0.9f * physScale);
            px->setOpacity (juce::jlimit (0.0f, 1.0f, 0.75f * f));
            px->drawImageAt (scaledSprite (glowWhite, ni), juce::roundToInt (c.x * physScale) - ni / 2, juce::roundToInt (c.y * physScale) - ni / 2);
        }
        else
        {
            sprite (g, glowWhite, c, fs.R * (1.2f + 1.6f * (float) a), 0.85f * f);
            sprite (g, glowSun, c, fs.R * (2.6f + 1.2f * (float) a), 0.45f * f);
        }
        g.setColour (juce::Colour (0xfffff4d8).withAlpha (0.8f * std::max (0.0f, 1.0f - (float) a / 1.1f)));
        const float r = fs.R * (0.2f + 0.9f * (float) a);
        g.drawEllipse (c.x - r, c.y - r * 0.8f, 2 * r, 1.6f * r, 1.6f);
    }
    float stepGlow (int l, int k) const { const double a = now - stepLit[(size_t) l][(size_t) k]; return a < 0.0 || a > 1.2 ? 0.0f : (float) std::exp (-a * 5.5); }
    float hitGlow (int slot) const { const double a = now - nucHit[(size_t) slot]; return a < 0.0 || a > 0.8 ? 0.0f : (float) std::exp (-a * 7.0); }

    juce::Rectangle<float> nucleusBounds() const
    {
        juce::Rectangle<float> r;
        for (int i = 0; i < nCount; ++i) r = r.getUnion (around (fs.nuc[(size_t) i].p, fs.nuc[(size_t) i].r * 4.2f + 2.0f));
        return r;
    }
    void drawNucleus (juce::Graphics& g, float haloAlpha)
    {
        for (int idx = 0; idx < nCount; ++idx)
        {
            const int i = fs.order[(size_t) idx]; const auto& n = fs.nuc[(size_t) i];
            const float hg = hitGlow (i);
            const float depth = juce::jlimit (0.0f, 1.0f, 0.5f + n.z / (fs.R * 0.3f) * 0.5f);
            sprite (g, n.kind == 0 ? glowSun : glowCold, n.p, n.r * (5.0f + 3.0f * hg), haloAlpha * (0.5f + 0.5f * depth) + 0.55f * hg);
            sprite (g, n.kind == 0 ? sphereP : sphereN, n.p, n.r * 2.0f * (1.0f + 0.12f * hg), 0.62f + 0.38f * depth);
        }
    }
    void drawHover (juce::Graphics& g)
    {
        if (hover < 0 || hover >= nCount) return;
        const auto& n = fs.nuc[(size_t) hover];
        g.setColour (juce::Colours::white.withAlpha (0.8f)); g.drawEllipse (n.p.x - n.r * 1.25f, n.p.y - n.r * 1.25f, n.r * 2.5f, n.r * 2.5f, 1.2f);
        const auto txt = juce::String (noteName (n.note % 12)) + juce::String (n.note / 12 - 1) + (n.kind == 0 ? "  proton" : "  neutron");
        g.setFont (font (14.0f, true));
        juce::GlyphArrangement ga; ga.addLineOfText (font (14.0f, true), txt, 0.0f, 0.0f);
        const int w = (int) std::ceil (ga.getBoundingBox (0, -1, true).getWidth()) + 16;
        const auto r = juce::Rectangle<float> ((float) w, 22.0f).withPosition (n.p.x + n.r * 1.5f, n.p.y - 30.0f);
        g.setColour (col::raised.withAlpha (0.92f)); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (n.kind == 0 ? col::sun : col::cold); g.drawText (txt, r.toNearestInt(), juce::Justification::centred);
    }

    // Las marcas de los pasos van en pocos caminos (reusados): lo que cuesta en pantalla es la cantidad de operaciones.
    juce::Path mStrong, mWeak, mHollow, mTie;
    void drawMarks (juce::Graphics& g, int l, bool front, bool marks, bool glows, const juce::RectangleList<int>* clip = nullptr)
    {
        const auto& A = snap.L[(size_t) l]; const auto c = col::layer (l);
        const float on = A.on ? 1.0f : 0.35f, depth = front ? 1.0f : 0.5f;
        mStrong.clear(); mWeak.clear(); mHollow.clear(); mTie.clear();
        for (int k = 0; k < A.length; ++k)
        {
            const float h = A.hit[(size_t) k], lit = glows && h > 0.0f ? stepGlow (l, k) : 0.0f;
            if (! marks && lit <= 0.02f) continue;
            const auto q = project (orbitPoint (l, juce::MathConstants<float>::twoPi * (float) k / (float) A.length));
            if ((q.z >= 0.0f) != front) continue;
            const float r = (h > 0.0f ? 2.3f + (A.flags[(size_t) k] & 1 ? 1.1f : 0.0f) : 1.9f) * q.s;
            if (lit > 0.02f && (clip == nullptr || hits (*clip, around (q.p, 17.0f * q.s)))) sprite (g, glow[(size_t) l], q.p, 34.0f * q.s, lit);
            if (! marks) continue;
            if (h > 0.0f)
            {
                (h >= 0.6f ? mStrong : mWeak).addEllipse (q.p.x - r, q.p.y - r, 2 * r, 2 * r);
                if (A.flags[(size_t) k] & 2)
                {
                    const auto n2 = project (orbitPoint (l, juce::MathConstants<float>::twoPi * (float) ((k + 1) % A.length) / (float) A.length));
                    mTie.startNewSubPath (q.p); mTie.lineTo (n2.p);
                }
            }
            else mHollow.addEllipse (q.p.x - r, q.p.y - r, 2 * r, 2 * r);
        }
        if (! marks) return;
        g.setColour (c.withAlpha (0.95f * on * depth)); g.fillPath (mStrong);
        g.setColour (c.withAlpha (0.45f * on * depth)); g.fillPath (mWeak);
        g.setColour (c.withAlpha (0.42f * on * depth)); g.strokePath (mHollow, juce::PathStrokeType (0.9f));
        if (! mTie.isEmpty()) { g.setColour (c.withAlpha (0.5f * on * depth)); g.strokePath (mTie, juce::PathStrokeType (1.0f)); }
    }
    void litBounds (juce::RectangleList<int>& out, int side = 0) const       // side: 0 todas, -1 sólo atrás
    {
        for (int l = 0; l < kLayers; ++l)
        {
            const auto& A = snap.L[(size_t) l];
            for (int k = 0; k < A.length; ++k)
                if (A.hit[(size_t) k] > 0.0f && stepGlow (l, k) > 0.02f)
                {
                    const auto q = project (orbitPoint (l, juce::MathConstants<float>::twoPi * (float) k / (float) A.length));
                    if (side < 0 && q.z >= 0.0f) continue;
                    out.add (around (q.p, 17.0f * q.s + 2.0f).getSmallestIntegerContainer());
                }
        }
    }

    // La estela: la cabeza y los puntos de hace span·m/kComet segundos (del registro de fases), como un cometa.
    bool comet (int l, Comet& C) const
    {
        const auto& A = snap.L[(size_t) l];
        if (! snap.running || ! A.on) return false;
        C.pos = electronPos (l, A.phase, &C.z, &C.s);
        C.n = 0;
        const int ks = A.length > 0 ? juce::jlimit (0, A.length - 1, (int) std::floor (A.phase * (float) A.length)) : 0;
        const double span = (A.flags[(size_t) ks] & 2) != 0 ? 1.1 : 0.7;
        const auto& T = trail[(size_t) l];
        if (T.n <= 1) return true;
        for (int m = 1; m <= kComet; ++m)
        {
            const double want = now - span * (double) m / (double) kComet;
            float ph = A.phase; bool found = false;
            for (int q = 0; q < T.n; ++q)
            {
                const int idx = (T.head - 1 - q + 2 * kTrail) % kTrail;
                if (T.t[(size_t) idx] <= want) { ph = T.ph[(size_t) idx]; found = true; break; }
            }
            if (! found) break;
            C.p[(size_t) C.n] = electronPos (l, ph, &C.zz[(size_t) C.n], &C.ss[(size_t) C.n]);
            ++C.n;
        }
        return true;
    }
    juce::Rectangle<float> cometBounds (const Comet& C) const
    {
        auto r = around (C.pos, 35.0f * C.s + 1.0f);
        for (int m = 0; m < C.n; ++m) r = r.getUnion (around (C.p[(size_t) m], 4.0f * C.ss[(size_t) m] + 1.0f));
        return r;
    }
    void drawElectron (juce::Graphics& g, int l, bool front, const Comet& C)
    {
        if ((C.z >= 0.0f) != front) return;
        const auto c = col::layer (l);
        V2 prev = C.pos;
        for (int m = 0; m < C.n; ++m)
        {
            const auto p = C.p[(size_t) m];
            const float k = 1.0f - (float) (m + 1) / (float) kComet;
            if ((C.zz[(size_t) m] >= 0.0f) == front && prev.getDistanceFrom (p) < fs.R * 0.8f)
            {
                g.setColour (c.interpolatedWith (juce::Colours::white, 0.35f).withAlpha (0.9f * k)); g.drawLine (prev.x, prev.y, p.x, p.y, (1.0f + 6.0f * k) * C.ss[(size_t) m]);
            }
            prev = p;
        }
        sprite (g, glow[(size_t) l], C.pos, 70.0f * C.s, 0.95f);
        g.setColour (juce::Colours::white); const float r = 2.8f * C.s; g.fillEllipse (C.pos.x - r, C.pos.y - r, 2 * r, 2 * r);
    }
    bool photonAlive (const Photon& p) const { const double age = now - p.t0; return age >= 0.0 && age <= 0.7; }
    V2 photonTarget (const Photon& p) const { return p.slot >= 0 && p.slot < nCount ? fs.nuc[(size_t) p.slot].p : V2 (fs.cx, fs.cy); }
    juce::Rectangle<float> photonBounds (const Photon& p) const
    {
        const float rad = (3.0f + 21.0f * p.vel) * 1.25f + 2.0f;
        auto r = around (p.a, rad);
        if (now - p.t0 < 0.38) r = r.getUnion (juce::Rectangle<float> (p.a, photonTarget (p)).expanded (3.0f));
        return r;
    }
    void drawPhotons (juce::Graphics& g, const juce::RectangleList<int>* clip = nullptr)
    {
        for (auto& p : photons)
        {
            if (! photonAlive (p) || (clip != nullptr && ! hits (*clip, photonBounds (p)))) continue;
            const double age = now - p.t0;
            const auto c = col::layer (p.layer);
            const V2 b = photonTarget (p);
            if (age < 0.38)
            {
                const float k = std::pow (1.0f - (float) age / 0.38f, 2.0f) * (p.accent ? 1.0f : 0.78f);
                g.setColour (c.brighter (0.5f).withAlpha (k)); g.drawLine (p.a.x, p.a.y, b.x, b.y, 1.0f + 2.2f * p.vel);
            }
            const float rk = (float) age / 0.7f;
            const float rad = (3.0f + (5.0f + 16.0f * p.vel) * (1.0f - (1.0f - rk) * (1.0f - rk))) * (p.accent ? 1.25f : 1.0f);
            g.setColour (c.brighter (0.4f).withAlpha ((1.0f - rk) * (p.accent ? 0.95f : 0.65f)));
            g.drawEllipse (p.a.x - rad, p.a.y - rad, 2 * rad, 2 * rad, 1.3f);
        }
    }

    // Grabación: todo, en el orden de profundidad exacto.
    void paintDirect (juce::Graphics& g)
    {
        const float fl = fusionGlow();
        Comet C;
        for (int l = 0; l < kLayers; ++l) { orbitStrokes (g, l, false, 0.22f, 0.8f, fl); drawMarks (g, l, false, true, true); if (comet (l, C)) drawElectron (g, l, false, C); }
        drawNucleus (g, 0.30f);
        for (int l = 0; l < kLayers; ++l) { orbitStrokes (g, l, true, 0.22f, 0.62f, fl); drawMarks (g, l, true, true, true); if (comet (l, C)) drawElectron (g, l, true, C); }
        drawPhotons (g);
        drawFusion (g);
        drawHover (g);
    }

    // En vivo: las dos cachés (a la resolución física de la pantalla) y por encima sólo lo que se mueve dentro del clip.
    void rebuildStatic()
    {
        const auto t0 = juce::Time::getHighResolutionTicks();
        staticDirty = false;
        stBack = juce::Image (juce::Image::RGB, bgW, bgH, false, juce::SoftwareImageType());
        {
            juce::Graphics gb (stBack);
            gb.drawImageAt (bg, 0, 0);
            gb.addTransform (juce::AffineTransform::scale (physScale));
            for (int l = 0; l < kLayers; ++l) { orbitStrokes (gb, l, false, 0.22f, 0.8f, 0.0f); drawMarks (gb, l, false, true, false); }
        }
        stFront = juce::Image (juce::Image::ARGB, bgW, bgH, true, juce::SoftwareImageType());
        {
            juce::Graphics gf (stFront);
            gf.addTransform (juce::AffineTransform::scale (physScale));
            for (int l = 0; l < kLayers; ++l) { orbitStrokes (gf, l, true, 0.22f, 0.62f, 0.0f); drawMarks (gf, l, true, true, false); }
        }
        indexFront();
        stAll = stBack.createCopy();
        { juce::Graphics ga (stAll); ga.drawImageAt (stFront, 0, 0); }
        ++rebuilds; rebuildMs += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0;
    }
    // En vivo: el cuadro propio (fb), sólo dentro de las zonas d. Lo vectorial va con la escala física; los sprites y las
    // cachés, en píxeles enteros (px). El orden de profundidad es el de paintDirect salvo que todas las órbitas y marcas
    // de atrás van juntas (caché) antes de los electrones de atrás.
    void renderLive (juce::RectangleList<int> d)
    {
        if (bg.isNull()) return;                   // todavía no hubo un paint: no se sabe la escala
        if (fb.isNull() || fb.getWidth() != bgW + 1 || fb.getHeight() != bgH + 2)
        {
            // margen de una columna y dos filas: el filtro bilineal de JUCE lee 4 bytes en el último píxel RGB (3 bytes) al
            // escalar el cuadro hacia arriba, y sin margen esa lectura caía fuera del buffer (EXC_BAD_ACCESS en un borde de página)
            fb = juce::Image (juce::Image::RGB, bgW + 1, bgH + 2, true, juce::SoftwareImageType());
            d = getLocalBounds();
        }
        const auto t0 = juce::Time::getHighResolutionTicks();
        if (staticDirty || stBack.isNull()) rebuildStatic();
        // Por grupos: recortar contra una lista de zonas cuesta más que pintar de más, y la unión de todo (los electrones
        // están en lados opuestos) era la mitad de la vista. Cada grupo es un rectángulo y una pasada con un clip simple;
        // si dos grupos se pisan, la zona común se pinta dos veces igual (cada pasada empieza copiando la caché).
        for (auto& r : clusters (d)) renderRect (r);
        px = nullptr;
        ++renders; renderMs += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0;
    }
    static std::vector<juce::Rectangle<int>> clusters (const juce::RectangleList<int>& d)
    {
        std::vector<juce::Rectangle<int>> g (d.begin(), d.end());
        const auto area = [] (juce::Rectangle<int> r) { return (double) r.getWidth() * (double) r.getHeight(); };
        for (bool merged = true; merged && g.size() > 1;)
        {
            merged = false;
            for (size_t i = 0; i < g.size() && ! merged; ++i)
                for (size_t j = i + 1; j < g.size(); ++j)
                {
                    const auto u = g[i].getUnion (g[j]);
                    // se juntan si la unión no agrega mucho más que lo que ya cubren (el costo fijo de una pasada ~ 64 x 64)
                    if (area (u) <= 1.3 * (area (g[i]) + area (g[j])) + 4096.0) { g[i] = u; g.erase (g.begin() + (std::ptrdiff_t) j); merged = true; break; }
                }
        }
        if (g.size() > 8) g = { d.getBounds() };
        return g;
    }
    void renderRect (juce::Rectangle<int> zone)
    {
        const juce::RectangleList<int> d (zone);
        juce::RectangleList<int> phys;
        for (auto& r : d) phys.add ((r.toFloat() * physScale).getSmallestIntegerContainer().getIntersection (fbArea()));
        juce::Graphics gp (fb); gp.reduceClipRegion (phys);
        juce::Graphics g (fb); g.reduceClipRegion (phys); g.addTransform (juce::AffineTransform::scale (physScale));
        px = &gp;
        std::array<Comet, kLayers> C; std::array<bool, kLayers> has {};
        for (int l = 0; l < kLayers; ++l) has[(size_t) l] = comet (l, C[(size_t) l]) && hits (d, cometBounds (C[(size_t) l]));
        // Donde hay algo detrás de las órbitas de adelante (núcleo, electrones y marcas encendidas de atrás) va la caché de
        // atrás, eso, y encima la de adelante; en el resto, las dos ya juntas (una copia en vez de una mezcla).
        juce::RectangleList<int> under;
        if (nCount > 0) under.add (nucleusBounds().getSmallestIntegerContainer());
        for (int l = 0; l < kLayers; ++l) if (has[(size_t) l] && C[(size_t) l].z < 0.0f) under.add (cometBounds (C[(size_t) l]).getSmallestIntegerContainer());
        litBounds (under, -1);
        juce::RectangleList<int> underPhys;
        for (auto& r : under) underPhys.add ((r.toFloat() * physScale).getSmallestIntegerContainer());
        underPhys.clipTo (phys);
        copyRects (stAll, phys);
        if (! underPhys.isEmpty())
        {
            copyRects (stBack, underPhys);
            for (int l = 0; l < kLayers; ++l) { drawMarks (g, l, false, false, true, &d); if (has[(size_t) l]) drawElectron (g, l, false, C[(size_t) l]); }
            if (nCount > 0 && hits (d, nucleusBounds())) drawNucleus (g, 0.30f);
            blendFront (underPhys);
        }
        for (int l = 0; l < kLayers; ++l) { drawMarks (g, l, true, false, true, &d); if (has[(size_t) l]) drawElectron (g, l, true, C[(size_t) l]); }
        drawPhotons (g, &d);
        drawFusion (g);
        drawHover (g);
        for (int i = 0; i < 3; ++i)
        {
            const auto R = hudRect (i);
            if (! d.intersectsRectangle (R)) continue;
            gp.setOpacity (1.0f);
            gp.drawImageAt (hudImage (i), juce::roundToInt ((float) R.getX() * physScale), juce::roundToInt ((float) R.getY() * physScale));
        }
    }
    // Restituir el fondo es copiar filas (memcpy): el mismo formato de píxel, sin pasar por el renderizador.
    void copyRects (const juce::Image& src, const juce::RectangleList<int>& rects)
    {
        const juce::Image::BitmapData s (src, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData d (fb, juce::Image::BitmapData::readWrite);
        jassert (s.pixelFormat == d.pixelFormat);
        for (auto r : rects)
        {
            r = r.getIntersection (fbArea());
            for (int y = r.getY(); y < r.getBottom(); ++y)
                std::memcpy (d.getPixelPointer (r.getX(), y), s.getPixelPointer (r.getX(), y), (size_t) (r.getWidth() * d.pixelStride));
        }
    }
    // La mitad de adelante de las órbitas encima de lo de atrás. Es casi toda transparente: al rehacer las cachés se anotan,
    // fila por fila, los tramos con algo (frontRuns), y se mezclan a mano sólo esos (con drawImageAt y un clip de varias
    // zonas era lo más caro del cuadro).
    std::vector<int> frontRow;                       // frontRuns[frontRow[y] .. frontRow[y + 1]) son los tramos de la fila y
    std::vector<std::pair<int, int>> frontRuns;
    void indexFront()
    {
        frontRow.assign ((size_t) bgH + 1, 0); frontRuns.clear();
        const juce::Image::BitmapData s (stFront, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < bgH; ++y)
        {
            frontRow[(size_t) y] = (int) frontRuns.size();
            auto* sp = reinterpret_cast<const juce::PixelARGB*> (s.getLinePointer (y));
            for (int x = 0; x < bgW;)
            {
                while (x < bgW && sp[x].getAlpha() == 0) ++x;
                const int x0 = x;
                while (x < bgW && sp[x].getAlpha() != 0) ++x;
                if (x > x0) frontRuns.emplace_back (x0, x);
            }
        }
        frontRow[(size_t) bgH] = (int) frontRuns.size();
    }
    void blendFront (const juce::RectangleList<int>& rects)
    {
        const juce::Image::BitmapData s (stFront, juce::Image::BitmapData::readOnly);
        juce::Image::BitmapData d (fb, juce::Image::BitmapData::readWrite);
        jassert (s.pixelFormat == juce::Image::ARGB && d.pixelFormat == juce::Image::RGB);
        for (auto r : rects)
        {
            r = r.getIntersection (fbArea());
            for (int y = r.getY(); y < r.getBottom(); ++y)
                for (int k = frontRow[(size_t) y]; k < frontRow[(size_t) y + 1]; ++k)
                {
                    const int x0 = std::max (frontRuns[(size_t) k].first, r.getX()), x1 = std::min (frontRuns[(size_t) k].second, r.getRight());
                    auto* sp = reinterpret_cast<const juce::PixelARGB*> (s.getPixelPointer (x0, y));
                    auto* dp = d.getPixelPointer (x0, y);
                    for (int x = x0; x < x1; ++x, ++sp, dp += d.pixelStride) reinterpret_cast<juce::PixelRGB*> (dp)->blend (*sp);
                }
        }
    }
    juce::Rectangle<int> fbArea() const { return { 0, 0, bgW, bgH }; }
    static bool hits (const juce::RectangleList<int>& d, juce::Rectangle<float> r) { return d.intersectsRectangle (r.getSmallestIntegerContainer()); }

    // ───────── texto sobre el átomo ─────────
    struct HudBlock { juce::Image img; juce::Rectangle<int> r; juce::String sig; double built = -1.0e9; float scale = 0; };
    std::array<HudBlock, 3> hud;
    static juce::String noteList (const std::array<uint8_t, kMaxPool>& a, int n)
    {
        juce::StringArray s; for (int i = 0; i < n; ++i) s.add (juce::String (noteName (a[(size_t) i] % 12)) + juce::String (a[(size_t) i] / 12 - 1));
        return n > 0 ? s.joinIntoString (" ") : juce::String ("-");
    }
    juce::String fusionText() const
    {
        if (snap.running && snap.fusionBeats >= 0.0) return "fusion in " + juce::String (snap.fusionBeats / 4.0, 1) + " bars";
        return snap.running ? "no fusion ahead" : "fusion in -";
    }
    void hudSigs (juce::String* sig) const
    {
        sig[0] = noteList (snap.protonNotes, snap.protons) + "|" + noteList (snap.neutronNotes, snap.neutrons);
        sig[1] = fusionText() + juce::String ((int) (fusionGlow() * 12.0f));
        sig[2] = {};
        for (int l = 0; l < kLayers; ++l) sig[2] << (snap.L[(size_t) l].on ? 1 : 0) << ',' << snap.L[(size_t) l].length << ',' << (snap.running ? snap.L[(size_t) l].step : -1) << ';';
    }
    juce::Rectangle<int> hudRect (int i) const
    {
        const auto b = getLocalBounds().reduced (18, 14);
        if (i == 0) return { b.getX(), b.getY(), 380, 84 };
        if (i == 1) return { b.getRight() - 270, b.getY(), 270, 28 };
        return { b.getX(), b.getBottom() - kLayers * 21, 270, kLayers * 21 };
    }
    // El texto cuesta mucho por cuadro en pantalla: cada bloque se dibuja en una imagen que sólo se rehace si cambia su contenido
    // (a lo sumo ~8 veces por segundo en vivo) y por cuadro es una sola operación.
    void renderHud (juce::Graphics& g, int i, float fl)
    {
        if (i == 0)
        {
            g.setFont (font (12.0f)); g.setColour (col::dim);
            g.drawText ("protons", 0, 0, 120, 16, juce::Justification::centredLeft);
            g.setColour (col::sun); g.setFont (font (16.0f, true));
            g.drawText (noteList (snap.protonNotes, snap.protons), 0, 15, 380, 22, juce::Justification::centredLeft);
            g.setFont (font (12.0f)); g.setColour (col::dim);
            g.drawText ("neutrons", 0, 42, 120, 16, juce::Justification::centredLeft);
            g.setColour (col::cold); g.setFont (font (16.0f, true));
            g.drawText (noteList (snap.neutronNotes, snap.neutrons), 0, 57, 380, 22, juce::Justification::centredLeft);
        }
        else if (i == 1)
        {
            g.setColour (col::dim.interpolatedWith (juce::Colour (0xfffff4d8), fl)); g.setFont (font (16.0f, true));
            g.drawText (fusionText(), 0, 2, 270, 22, juce::Justification::centredRight);
        }
        else
            for (int l = 0; l < kLayers; ++l)
            {
                const auto& A = snap.L[(size_t) l];
                const int y = l * 21;
                g.setColour (col::layer (l).withAlpha (A.on ? 1.0f : 0.35f)); g.fillEllipse (0.0f, (float) y + 4.0f, 9.0f, 9.0f);
                g.setColour (A.on ? col::text : col::faint); g.setFont (font (13.0f));
                g.drawText (A.on ? "layer " + juce::String (l + 1) + "   " + juce::String (A.length) + " steps   " + juce::String (snap.running ? A.step + 1 : 0) : "layer " + juce::String (l + 1) + "   off",
                            17, y, 250, 18, juce::Justification::centredLeft);
            }
    }
    const juce::Image& hudImage (int i)
    {
        juce::String sig[3]; hudSigs (sig);
        const auto R = hudRect (i);
        auto& h = hud[(size_t) i];
        const bool fresh = h.img.isNull() || h.r != R || h.scale != physScale;
        if (fresh || (h.sig != sig[i] && now - h.built >= (live ? 0.12 : 0.0)))
        {
            if (fresh) h.img = live ? juce::Image (juce::Image::ARGB, (int) std::ceil ((float) R.getWidth() * physScale), (int) std::ceil ((float) R.getHeight() * physScale), true, juce::SoftwareImageType())
                                    : juce::Image (juce::Image::ARGB, (int) std::ceil ((float) R.getWidth() * physScale), (int) std::ceil ((float) R.getHeight() * physScale), true);
            else h.img.clear (h.img.getBounds());
            h.r = R; h.sig = sig[i]; h.built = now; h.scale = physScale;
            juce::Graphics gi (h.img);
            gi.addTransform (juce::AffineTransform::scale (physScale));
            renderHud (gi, i, fusionGlow());
        }
        return h.img;
    }
    void paintHud (juce::Graphics& g)
    {
        for (int i = 0; i < 3; ++i)
        {
            const auto R = hudRect (i);
            if (! g.getClipBounds().intersects (R)) continue;
            g.setOpacity (1.0f);
            g.drawImage (hudImage (i), R.toFloat(), juce::RectanglePlacement::stretchToFit);
        }
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AtomView)
};

inline void AtomView::paint (juce::Graphics& g)
{
    if (lastNow < -1.0e8) { snap = AtomSnap(); ingest (AtomSnap(), {}, wallNow()); }     // primer cuadro sin datos: un átomo vacío
    const auto t0 = juce::Time::getHighResolutionTicks();
    if (probe() != 0 && live) { g.fillAll (juce::Colours::black); return; }
    ensureBackground (g);
    if (live)
    {
        // la pantalla sólo copia del cuadro propio la parte que pide el clip
        if (fb.isNull() || fb.getWidth() != bgW + 1 || fb.getHeight() != bgH + 2 || staticDirty) renderLive (getLocalBounds());
        // El cuadro entero contra el clip del contexto (sólo se calculan los píxeles del clip). Un getClippedImage escalado
        // hacia arriba hacía que el filtro de JUCE leyera fuera de la subimagen (EXC_BAD_ACCESS con el cuadro a 1x).
        if (! g.getClipBounds().isEmpty())
        {
            // Calidad baja: el cuadro ya está en píxeles físicos, pero con el editor al 85 % la vista empieza entre dos
            // píxeles y JUCE sólo copia directo si el corrimiento es entero o si se le permite redondearlo; si no,
            // remuestrea cada píxel (Linux, Xvfb: 3,7 ms por cuadro, más que todo el dibujo). Medio píxel no se ve.
            g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
            g.setOpacity (1.0f);
            g.drawImageTransformed (fb, juce::AffineTransform::scale (1.0f / physScale));
        }
    }
    else
    {
        g.setOpacity (1.0f);
        g.drawImage (bg, getLocalBounds().toFloat(), juce::RectanglePlacement::stretchToFit);
        paintDirect (g);
        paintHud (g);
    }
    ++paints; if (g.getClipBounds() == getLocalBounds()) ++fulls;
    clipArea += (double) g.getClipBounds().getWidth() * g.getClipBounds().getHeight() / std::max (1.0, (double) getWidth() * getHeight());
    paintMs += juce::Time::highResolutionTicksToSeconds (juce::Time::getHighResolutionTicks() - t0) * 1000.0;
}

} // namespace dh::val::ui
