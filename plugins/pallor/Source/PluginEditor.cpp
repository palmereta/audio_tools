#include "PluginEditor.h"

using namespace dh::pal;
using namespace dh::pal::ui;

// ── La pantalla: pestañas arriba y la página elegida ──
class PallorEditor::Screen : public juce::Component
{
public:
    explicit Screen (Ctx& c) : ctx (c), pattern (c), voice (c), mixer (c), fx (c), song (c), samples (c), presets (c), setup (c)
    {
        setOpaque (false);
        for (auto* p : pages()) addChildComponent (p);
        show ("pattern");
    }
    std::vector<juce::Component*> pages() { return { &pattern, &voice, &mixer, &fx, &song, &samples, &presets, &setup }; }
    static const juce::StringArray& ids() { static juce::StringArray s { "pattern", "voice", "mixer", "fx", "song", "samples", "presets", "setup" }; return s; }
    void show (const juce::String& id)
    {
        const int i = std::max (0, ids().indexOf (id));
        ctx.page = ids()[i];
        auto ps = pages();
        for (int k = 0; k < (int) ps.size(); ++k) ps[(size_t) k]->setVisible (k == i);
        repaint (0, 0, getWidth(), kTabH + 2);
    }
    static constexpr int kTabH = 30;
    juce::Rectangle<float> tabR (int i) const { return { 12.0f + 104.0f * (float) i, 0.0f, 100.0f, (float) kTabH }; }
    void resized() override { for (auto* p : pages()) p->setBounds (getLocalBounds().withTrimmedTop (kTabH + 4).reduced (4, 2)); }
    void paint (juce::Graphics& g) override
    {
        // El vidrio ya está en el render; acá sólo un velo para que el texto lea parejo sobre el reflejo.
        auto r = getLocalBounds().toFloat();
        g.setColour (col::glass.withAlpha (0.55f)); g.fillRect (r);
        static const char* names[] = { "PATTERN", "VOICE", "MIXER", "FX", "SONG", "SAMPLES", "PRESETS", "SETUP" };
        for (int i = 0; i < 8; ++i)
        {
            auto t = tabR (i);
            const bool on = ctx.page == ids()[i];
            g.setColour (on ? col::bone : col::boneDim);
            text (g, names[i], t, juce::Justification::centred, 15.0f, on, 0.16f);
            if (on) { g.setColour (col::rose); g.fillRoundedRectangle (t.getX() + 18, t.getBottom() - 3.0f, t.getWidth() - 36, 2.5f, 1.2f); }
        }
        g.setColour (col::line); g.drawHorizontalLine (kTabH, 8.0f, r.getRight() - 8.0f);
        // A la derecha: la voz elegida (todas las páginas la usan).
        g.setColour (col::boneDim); text (g, "VOICE", { r.getRight() - 230, 0, 60, (float) kTabH }, juce::Justification::centredRight, 12.5f, false, 0.14f);
        g.setColour (col::rose); text (g, juce::String (params::voiceName (ctx.voice)).toUpperCase(), { r.getRight() - 164, 0, 150, (float) kTabH }, juce::Justification::centredLeft, 16.0f, true, 0.1f);
    }
    void mouseDown (const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 8; ++i) if (tabR (i).contains (e.position)) { show (ids()[i]); return; }
    }
    void tick()
    {
        if (pattern.isVisible()) pattern.tick();
        if (mixer.isVisible()) mixer.tick();
        if (fx.isVisible()) fx.tick();
        if (song.isVisible()) song.tick();
        if (samples.isVisible()) samples.tick();
        if (ctx.voice != shownVoice) { shownVoice = ctx.voice; repaint (getWidth() - 240, 0, 240, kTabH); }
    }
    Ctx& ctx;
    PatternPage pattern; VoicePage voice; MixerPage mixer; FxPage fx; SongPage song; SamplesPage samples; PresetsPage presets; SetupPage setup;
    int shownVoice = -1;
};

PallorEditor::PallorEditor (PallorProcessor& p) : AudioProcessorEditor (&p), proc (p), ctx (p), overlay (ctx)
{
    setLookAndFeel (&look);
    addAndMakeVisible (content);
    content.setSize (baseW, baseH);
    staticLayer.setBounds (0, 0, baseW, baseH);
    overlay.setBounds (0, 0, baseW, baseH);
    content.addAndMakeVisible (staticLayer);
    content.addAndMakeVisible (overlay);
    screen = std::make_unique<Screen> (ctx);
    screen->setBounds (Layout::get().screen.toNearestInt());
    content.addAndMakeVisible (*screen);

    ctx.showPage = [this] (const juce::String& id) { screen->show (id); };
    ctx.toggleManual = [this] { showManual (manual == nullptr); };
    ctx.loadSample = [this] (int v, const juce::File& f) { loadSample (v, f); };
    ctx.onVoice.push_back ([this] { screen->repaint (screen->getWidth() - 240, 0, 240, Screen::kTabH); });

    proc.onPresetChanged = [this] { for (auto& f : ctx.onPreset) f(); for (auto& f : ctx.onPatterns) f(); };
    proc.onPatternsChanged = [this] { for (auto& f : ctx.onPatterns) f(); };

    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.5), (int) (baseH * 1.5));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.78), (int) (baseH * 0.78));
    startTimerHz (30);

    // Capturas sin pantalla (manual, web): PALLOR_SNAPSHOT=/ruta.png, PALLOR_PRESET=n, PALLOR_PAGE=id, PALLOR_VOICE=n,
    // PALLOR_RUN=1, PALLOR_WAIT=s, PALLOR_MANUAL=1, PALLOR_SCREEN=1 (sólo la pantalla), PALLOR_SCALE=2, PALLOR_LANE=n,
    // PALLOR_SAMPLES_DIR=carpeta (la que muestra SAMPLES, en vez de la última usada).
    const auto snap = juce::SystemStats::getEnvironmentVariable ("PALLOR_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        auto env = [] (const char* n, const char* d) { return juce::SystemStats::getEnvironmentVariable (n, d); };
        const int preset = env ("PALLOR_PRESET", "-1").getIntValue(), wait = env ("PALLOR_WAIT", "3").getIntValue(), voice = env ("PALLOR_VOICE", "-1").getIntValue();
        const auto page = env ("PALLOR_PAGE", ""), lane = env ("PALLOR_LANE", "");
        const bool run = env ("PALLOR_RUN", "0").getIntValue() != 0, man = env ("PALLOR_MANUAL", "0").getIntValue() != 0, onlyScreen = env ("PALLOR_SCREEN", "0").getIntValue() != 0;
        const float scale = (float) env ("PALLOR_SCALE", "2").getDoubleValue();
        juce::Component::SafePointer<PallorEditor> safe (this);
        juce::Timer::callAfterDelay (300, [safe, preset, page, voice, run, man, lane] {
            if (safe == nullptr) return;
            if (preset >= 0) safe->proc.loadPreset (preset);
            if (page.isNotEmpty()) safe->screen->show (page);
            if (voice >= 0) safe->ctx.selectVoice (voice);
            if (lane.isNotEmpty()) safe->screen->pattern.setLane (lane.getIntValue());
            if (run) { safe->proc.setValue ("sync", 0.0f); safe->proc.setValue ("run", 1.0f); }
            if (man) safe->showManual (true);
        });
        juce::Timer::callAfterDelay (300 + wait * 1000, [safe, snap, man, onlyScreen, scale] {
            if (safe == nullptr) return;
            juce::Image img;
            if (man && safe->manual != nullptr) img = safe->createComponentSnapshot (safe->getLocalBounds(), true, scale * (float) baseW / (float) safe->getWidth());
            else if (onlyScreen) img = safe->content.createComponentSnapshot (safe->screen->getBounds(), true, scale);
            else img = safe->content.createComponentSnapshot (safe->content.getLocalBounds(), true, scale);
            juce::File f (snap); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        });
    }
}

PallorEditor::~PallorEditor()
{
    proc.onPresetChanged = nullptr; proc.onPatternsChanged = nullptr;
    manual.reset();
    setLookAndFeel (nullptr);
}

void PallorEditor::paint (juce::Graphics& g) { g.fillAll (juce::Colours::black); }

void PallorEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    if (manual != nullptr) manual->setBounds (getLocalBounds());
}

void PallorEditor::timerCallback()
{
    // Niveles y destellos de cada voz: una lectura por cuadro, compartida por el panel y las páginas.
    for (int s = 0; s < kNumSlots; ++s)
    {
        const float pk = proc.view.peak[(size_t) s].exchange (0.0f);
        ctx.level[(size_t) s] = std::max (pk, ctx.level[(size_t) s] * 0.82f);
        const uint32_t tc = proc.view.trig[(size_t) s].load();
        if (tc != lastTrig[(size_t) s]) { lastTrig[(size_t) s] = tc; ctx.flash[(size_t) s] = 1.0f; }
        else ctx.flash[(size_t) s] *= 0.74f;
        if (ctx.flash[(size_t) s] < 0.01f) ctx.flash[(size_t) s] = 0.0f;
    }
    overlay.tick();
    screen->tick();
}

void PallorEditor::showManual (bool on)
{
   #if DH_HAS_MANUAL
    if (! on) { manual.reset(); return; }
    dh::manual::Theme t; t.bg = juce::Colour (0xff0d0e10); t.panel = col::cell; t.text = col::bone; t.muted = col::boneDim; t.accent = col::rose; t.line = col::line;
    auto m = dh::manual::makeEmbedded (t);
    m->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<PallorEditor> (this)] { if (safe != nullptr) safe->showManual (false); }); };
    addAndMakeVisible (*m); m->setBounds (getLocalBounds());
    manual = std::move (m);
   #else
    juce::ignoreUnused (on);
   #endif
}

void PallorEditor::loadSample (int voice, const juce::File& f)
{
    juce::String err;
    if (! proc.loadUserSample (voice, f, err))
    {
        juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Sample", err);
        return;
    }
    ctx.setParam (params::vid (voice, "engine"), 1.0f);
    ctx.setParam (params::vid (voice, "kit"), (float) kitgen::numKits());
    ctx.selectVoice (voice);
    ctx.say (juce::String (params::voiceName (voice)) + ": " + f.getFileName());
    proc.audition (voice, 0.85f);
}

void PallorEditor::filesDropped (const juce::StringArray& f, int x, int y)
{
    // Una muestra soltada en cualquier lugar que no sea un pad va a la voz elegida.
    const auto p = content.getLocalPoint (this, juce::Point<int> (x, y));
    const int pad = overlay.padAt (p);
    loadSample (pad >= 0 ? pad : ctx.voice, juce::File (f[0]));
}
