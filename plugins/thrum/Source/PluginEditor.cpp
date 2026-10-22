#include "PluginEditor.h"

using namespace thrumui;

namespace {
const juce::StringArray kPages { "guitar", "play", "pedals", "amp", "settings" };
const char* kPageNames[] = { "Guitar", "Play", "Pedals", "Amp / Cab", "Settings" };
}

// ───────── El lienzo ─────────
struct ThrumEditor::Content : public juce::Component
{
    Content (ThrumProcessor& p, ThrumEditor& e) : proc (p), ed (e), fret (p), guitar (p), play (p), pedals (p), amp (p), settings (p)
    {
        setOpaque (true);
        addAndMakeVisible (fret);
        for (auto* pg : pages()) addChildComponent (pg);
        pedals.gotoAmp = [this] { show (3); };
        show (0);
    }
    std::vector<Page*> pages() { return { &guitar, &play, &pedals, &amp, &settings }; }
    void show (int i)
    {
        page = juce::jlimit (0, 4, i);
        auto ps = pages();
        for (int k = 0; k < 5; ++k) ps[(size_t) k]->setVisible (k == page);
        repaint (0, 0, baseW, 56);
    }
    void setInspect (bool on)
    {
        inspect = on; fret.inspect = on;
        for (auto* p : pages()) p->setInspect (on);
        fret.repaint(); repaint (0, 0, baseW, 56);
    }
    void resized() override
    {
        fret.setBounds (16, 62, 1328, 176);
        for (auto* p : pages()) p->setBounds (16, 260, 1328, 584);
    }
    // ── Barra de arriba ──
    juce::Rectangle<float> prevR() const { return { 150, 13, 30, 32 }; }
    juce::Rectangle<float> presetR() const { return { 186, 13, 330, 32 }; }
    juce::Rectangle<float> nextR() const { return { 522, 13, 30, 32 }; }
    juce::Rectangle<float> tabR (int i) const { return { 580.0f + 108.0f * (float) i, 8.0f, 104.0f, 42.0f }; }
    juce::Rectangle<float> inspectR() const { return { 1136, 13, 96, 32 }; }
    juce::Rectangle<float> manualR() const { return { 1244, 13, 100, 32 }; }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg);
        g.setColour (col::bar); g.fillRect (0, 0, baseW, 56);
        g.setColour (col::line); g.drawHorizontalLine (56, 0, (float) baseW);
        g.setColour (col::text); text (g, "THRUM", { 20, 0, 120, 56 }, juce::Justification::centredLeft, 32.0f, true, 0.04f);
        // Presets.
        auto box = [&] (juce::Rectangle<float> r, bool hot) { g.setColour (hot ? col::cell2 : col::bg); g.fillRoundedRectangle (r, 4.0f); g.setColour (col::line); g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.0f); };
        box (prevR(), hover == 1); box (presetR(), hover == 2); box (nextR(), hover == 3);
        g.setColour (col::dim); text (g, "<", prevR(), juce::Justification::centred, 18.0f, true); text (g, ">", nextR(), juce::Justification::centred, 18.0f, true);
        const int cur = proc.getCurrentProgram();
        const auto& ps = proc.getPresets();
        g.setColour (col::text); text (g, proc.presetDisplayName(), presetR().reduced (12, 0).withTrimmedRight (120), juce::Justification::centredLeft, 17.0f);
        if (cur >= 0 && cur < (int) ps.size()) label (g, ps[(size_t) cur].category, presetR().reduced (12, 0), juce::Justification::centredRight, 12.0f);
        // Páginas.
        for (int i = 0; i < 5; ++i)
        {
            auto r = tabR (i); const bool on = i == page;
            g.setColour (on ? col::amber : hover == 10 + i ? col::text : col::dim);
            text (g, juce::String (kPageNames[i]).toUpperCase(), r, juce::Justification::centred, 18.0f, on, 0.05f);
            if (on) { g.fillRoundedRectangle (r.getX() + 20, r.getBottom() - 3, r.getWidth() - 40, 2.5f, 1.2f); }
        }
        // Inspect y Manual.
        auto ir = inspectR();
        g.setColour (inspect ? col::cyan : (hover == 20 ? col::cell2 : col::panel)); g.fillRoundedRectangle (ir, 4.0f);
        if (! inspect) { g.setColour (col::cyanDim); g.drawRoundedRectangle (ir.reduced (0.5f), 4.0f, 1.0f); }
        g.setColour (inspect ? col::ink : col::cyan); text (g, "INSPECT", ir, juce::Justification::centred, 15.0f, true, 0.08f);
        box (manualR(), hover == 21);
        g.setColour (col::text); text (g, "MANUAL", manualR(), juce::Justification::centred, 15.0f, true, 0.08f);
        // Debajo del diapasón: el acorde, el modo y la articulación.
        dh::gt::Chord c; c.root = proc.view.chordRoot.load(); c.type = proc.view.chordType.load(); c.bass = proc.view.chordBass.load();
        const int mode = (int) proc.value ("p_mode");
        const bool chordMode = mode >= dh::gt::kModeChords;
        juce::String left = dh::gt::modeName (mode);
        if (chordMode && c.valid()) left = juce::String (dh::gt::chordName (c)) + "   /   " + left + "   /   " + dh::gt::styleName ((int) proc.value ("c_style"));
        g.setColour (col::amber); text (g, left.toUpperCase(), { 20, 238, 700, 18 }, juce::Justification::centredLeft, 14.0f, true, 0.05f);
        label (g, juce::String ("Articulation ") + dh::gt::artName (proc.view.art.load()) + "   /   hand at fret " + juce::String (juce::roundToInt (proc.view.hand.load())),
               { 640, 238, 704, 18 }, juce::Justification::centredRight, 13.5f);
    }
    void mouseMove (const juce::MouseEvent& e) override
    {
        int h = 0;
        if (prevR().contains (e.position)) h = 1; else if (presetR().contains (e.position)) h = 2; else if (nextR().contains (e.position)) h = 3;
        for (int i = 0; i < 5; ++i) if (tabR (i).contains (e.position)) h = 10 + i;
        if (inspectR().contains (e.position)) h = 20; if (manualR().contains (e.position)) h = 21;
        if (h != hover) { hover = h; repaint (0, 0, baseW, 56); }
    }
    void mouseExit (const juce::MouseEvent&) override { hover = 0; repaint (0, 0, baseW, 56); }
    void mouseDown (const juce::MouseEvent& e) override
    {
        const int n = (int) proc.getPresets().size();
        if (prevR().contains (e.position)) proc.loadPreset ((proc.getCurrentProgram() - 1 + n) % n);
        else if (nextR().contains (e.position)) proc.loadPreset ((proc.getCurrentProgram() + 1) % n);
        else if (presetR().contains (e.position)) presetMenu();
        else if (inspectR().contains (e.position)) setInspect (! inspect);
        else if (manualR().contains (e.position)) ed.showManual (true);
        for (int i = 0; i < 5; ++i) if (tabR (i).contains (e.position)) show (i);
    }
    void presetMenu()
    {
        juce::PopupMenu m;
        const auto& ps = proc.getPresets();
        juce::StringArray cats; for (auto& p : ps) cats.addIfNotAlreadyThere (p.category);
        for (auto& c : cats)
        {
            juce::PopupMenu sub;
            for (size_t i = 0; i < ps.size(); ++i) if (ps[i].category == c) sub.addItem ((int) i + 1, ps[i].name, true, (int) i == proc.getCurrentProgram());
            m.addSubMenu (c, sub);
        }
        juce::Component::SafePointer<Content> safe (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea (localAreaToGlobal (presetR().toNearestInt())).withMinimumWidth (220),
                         [safe] (int r) { if (safe != nullptr && r > 0) safe->proc.loadPreset (r - 1); });
    }
    void tick()
    {
        fret.tick();
        pages()[(size_t) page]->tick();
        // La línea del acorde y el nombre del preset: sólo si cambiaron.
        const juce::String sig = proc.presetDisplayName() + juce::String (proc.view.chordRoot.load()) + "," + juce::String (proc.view.chordType.load()) + "," + juce::String (proc.view.chordBass.load())
                                 + "," + juce::String (proc.view.art.load()) + "," + juce::String (juce::roundToInt (proc.view.hand.load())) + "," + juce::String ((int) proc.value ("p_mode")) + "," + juce::String ((int) proc.value ("c_style"));
        if (sig != lastSig) { lastSig = sig; repaint (0, 0, baseW, 56); repaint (0, 236, baseW, 22); }
    }
    ThrumProcessor& proc; ThrumEditor& ed;
    Fretboard fret;
    GuitarPage guitar; PlayPage play; PedalsPage pedals; AmpPage amp; SettingsPage settings;
    int page = 0, hover = 0; bool inspect = false; juce::String lastSig;
};

// ───────── Editor ─────────
ThrumEditor::ThrumEditor (ThrumProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    setLookAndFeel (&look);
    content = std::make_unique<Content> (p, *this);
    content->setSize (baseW, baseH);
    addAndMakeVisible (*content);
    proc.onPresetChanged = [this] { content->play.refreshPatterns(); content->repaint(); };
    proc.onPatternChanged = [this] { content->play.refreshPatterns(); };
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.6), (int) (baseH * 1.6));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.82), (int) (baseH * 0.82));
    startTimerHz (30);

    // Capturas sin pantalla (manual, web): THRUM_SNAPSHOT=/ruta.png, THRUM_PAGE=guitar|play|pedals|amp|settings,
    // THRUM_PRESET=n, THRUM_WAIT=s, THRUM_INSPECT=1, THRUM_MANUAL=1, THRUM_SCALE=2, THRUM_PEDAL=n (el pedal elegido),
    // THRUM_DEMO="40 47 52 56" (notas MIDI que se tocan por el teclado del procesador), THRUM_DEMO_PERIOD=s (repetir;
    // 0 = mantenerlas), THRUM_SET="id=valor,id=valor".
    auto env = [] (const char* n, const char* d) { return juce::SystemStats::getEnvironmentVariable (n, d); };
    const auto snap = env ("THRUM_SNAPSHOT", "");
    const auto demo = env ("THRUM_DEMO", "");
    if (demo.isNotEmpty())
    {
        juce::StringArray t; t.addTokens (demo, " ,", ""); t.removeEmptyStrings();
        for (auto& s : t) demoNotes.add (s.getIntValue());
        demoPeriod = env ("THRUM_DEMO_PERIOD", "0").getDoubleValue();
        demoStart = juce::Time::getMillisecondCounterHiRes() * 0.001 + 0.9;
    }
    if (snap.isNotEmpty())
    {
        const int preset = env ("THRUM_PRESET", "-1").getIntValue(), wait = env ("THRUM_WAIT", "3").getIntValue();
        const auto page = env ("THRUM_PAGE", ""), sets = env ("THRUM_SET", "");
        const bool insp = env ("THRUM_INSPECT", "0").getIntValue() != 0, man = env ("THRUM_MANUAL", "0").getIntValue() != 0;
        const int pedal = env ("THRUM_PEDAL", "-1").getIntValue();
        const float scale = (float) env ("THRUM_SCALE", "2").getDoubleValue();
        juce::Component::SafePointer<ThrumEditor> safe (this);
        juce::Timer::callAfterDelay (300, [safe, preset, page, sets, insp, man, pedal] {
            if (safe == nullptr) return;
            if (preset >= 0) safe->proc.loadPreset (preset);
            juce::StringArray kv; kv.addTokens (sets, ",", ""); kv.removeEmptyStrings();
            for (auto& s : kv) safe->proc.setValue (s.upToFirstOccurrenceOf ("=", false, false).trim(), s.fromFirstOccurrenceOf ("=", false, false).getFloatValue());
            if (page.isNotEmpty()) safe->showPage (std::max (0, kPages.indexOf (page)));
            if (pedal >= 0) { safe->content->pedals.setInspect (safe->content->inspect); }
            if (insp) safe->content->setInspect (true);
            if (man) safe->showManual (true);
        });
        if (pedal >= 0)
            juce::Timer::callAfterDelay (400, [safe, pedal] { if (safe != nullptr) safe->content->pedals.select (pedal); });
        juce::Timer::callAfterDelay (300 + wait * 1000, [safe, snap, man, scale] {
            if (safe == nullptr) return;
            juce::Image img = man && safe->manual != nullptr ? safe->createComponentSnapshot (safe->getLocalBounds(), true, scale * (float) baseW / (float) safe->getWidth())
                                                             : safe->content->createComponentSnapshot (safe->content->getLocalBounds(), true, scale);
            juce::File f (snap); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os);
        });
    }
}

ThrumEditor::~ThrumEditor()
{
    for (int n : demoNotes) proc.keyboardState.noteOff (1, n, 0.0f);
    proc.onPresetChanged = nullptr; proc.onPatternChanged = nullptr;
    manual.reset();
    content.reset();
    setLookAndFeel (nullptr);
}

void ThrumEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void ThrumEditor::resized()
{
    content->setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    if (manual != nullptr) manual->setBounds (getLocalBounds());
}

void ThrumEditor::showPage (int i) { content->show (i); }

void ThrumEditor::showManual (bool on)
{
   #if DH_HAS_MANUAL
    if (! on) { manual.reset(); return; }
    dh::manual::Theme t; t.bg = col::bg; t.panel = col::panel; t.text = col::text; t.muted = col::dim; t.accent = col::amber; t.line = col::line;
    auto m = dh::manual::makeEmbedded (t);
    m->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ThrumEditor> (this)] { if (safe != nullptr) safe->showManual (false); }); };
    addAndMakeVisible (*m); m->setBounds (getLocalBounds());
    manual = std::move (m);
   #else
    juce::ignoreUnused (on);
   #endif
}

void ThrumEditor::timerCallback()
{
    content->tick();
    if (! demoNotes.isEmpty())
    {
        const double now = juce::Time::getMillisecondCounterHiRes() * 0.001 - demoStart;
        if (now >= 0.0)
        {
            const bool down = demoPeriod <= 0.0 ? true : std::fmod (now, demoPeriod) < demoPeriod * 0.85;
            if (down != demoDown)
            {
                demoDown = down;
                for (int n : demoNotes) { if (down) proc.keyboardState.noteOn (1, n, 0.8f); else proc.keyboardState.noteOff (1, n, 0.0f); }
            }
        }
    }
}
