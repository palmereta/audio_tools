#include "PluginEditor.h"

// El manual con los colores de la serie (el acento de este producto en la web).
static dh::manual::Theme manualTheme() { dh::manual::Theme t; t.accent = juce::Colour (0xfff2c878); return t; }

using namespace dh::orr;
using namespace dh::orr::ui;

namespace {
// Demos para la web: guarda un cuadro por cada 1/30 s de tiempo de sesión (no de reloj), así el video
// queda en sincronía con el audio renderizado del mismo preset aunque algún cuadro tarde. Los JPEG se
// escriben en otro hilo. ORRERY_RECORD=/carpeta ORRERY_RECORD_SECS=30 ORRERY_RECORD_SCALE=1.4545
struct DemoRecorder : juce::Timer, juce::Thread
{
    DemoRecorder (juce::Component& c, OrreryProcessor& p, juce::File d, double s, float sc)
        : juce::Thread ("orrery demo"), comp (c), proc (p), dir (std::move (d)), secs (s), scale (sc)
    { dir.createDirectory(); startThread(); startTimerHz (60); }
    ~DemoRecorder() override { stopTimer(); signalThreadShouldExit(); notify(); stopThread (10000); }

    void timerCallback() override
    {
        const auto& v = proc.view();
        if (! v.running) return;
        if (v.sessionSeconds > secs) { stopTimer(); dir.getChildFile ("done.txt").replaceWithText (juce::String (last + 1)); return; }
        const int idx = (int) std::floor (v.sessionSeconds * 30.0);
        if (idx <= last) return;
        last = idx;
        auto img = comp.createComponentSnapshot (comp.getLocalBounds(), true, scale);
        { const juce::ScopedLock sl (lock); queue.push_back ({ idx, img }); }
        notify();
    }
    void run() override
    {
        while (! threadShouldExit()) { wait (40); drain(); }
        drain();
    }
    void drain()
    {
        for (;;)
        {
            std::pair<int, juce::Image> item;
            { const juce::ScopedLock sl (lock); if (queue.empty()) return; item = queue.front(); queue.erase (queue.begin()); }
            auto f = dir.getChildFile (juce::String::formatted ("f_%05d.jpg", item.first));
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::JPEGImageFormat jpg; jpg.setQuality (0.94f);
            jpg.writeImageToStream (item.second, os);
        }
    }
    juce::Component& comp; OrreryProcessor& proc; juce::File dir; double secs; float scale;
    int last = -1;
    juce::CriticalSection lock;
    std::vector<std::pair<int, juce::Image>> queue;
};
}

OrreryEditor::OrreryEditor (OrreryProcessor& p)
    : AudioProcessorEditor (&p), proc (p), simple (p), lanes (p), harmony (p), outputs (p)
{
    setLookAndFeel (&lnf);
    content.painter = [this] (juce::Graphics& g) { paintBackdrop (g); };
    content.down = [this] (const juce::MouseEvent& e) { if (wordR.contains (e.getPosition())) showPage (0); };
    content.drag = [this] (const juce::MouseEvent& e) {
        if (captured.existsAsFile() && dragR.contains (e.getMouseDownPosition()))
            performExternalDragDropOfFiles ({ captured.getFullPathName() }, false, this);
    };
    content.setOpaque (true);
    addAndMakeVisible (content);
    content.setSize (baseW, baseH);
    for (auto* c : std::initializer_list<juce::Component*> { &simple, &lanes, &harmony, &outputs }) content.addChildComponent (c);
    for (auto* b : { &playBtn, &prevBtn, &nextBtn, &presetBtn, &saveBtn, &captureBtn, &panicBtn, &simpleBtn, &advBtn, &tabLanes, &tabHarm, &tabOut }) content.addAndMakeVisible (b);
    content.addAndMakeVisible (manualBtn);
    manualBtn.onClick = [this] { manualHost.toggle (*this, manualTheme()); };
    playBtn.onClick = [this] { proc.setValue ("run", proc.value ("run") > 0.5f ? 0.0f : 1.0f); };
    prevBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n > 0) { const bool r = proc.value ("run") > 0.5f; proc.loadPreset ((proc.getCurrentProgram() - 1 + n) % n); if (r) proc.setValue ("run", 1.0f); } };
    nextBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n > 0) { const bool r = proc.value ("run") > 0.5f; proc.loadPreset ((proc.getCurrentProgram() + 1) % n); if (r) proc.setValue ("run", 1.0f); } };
    presetBtn.onClick = [this] { showPresetMenu(); };
    saveBtn.onClick = [this] { savePreset(); };
    captureBtn.onClick = [this] { doCapture(); };
    panicBtn.onClick = [this] { proc.panic(); };
    simpleBtn.onClick = [this] { showPage (0); };
    advBtn.onClick = [this] { showPage (page == 0 ? 1 : page); };
    tabLanes.onClick = [this] { showPage (1); };
    tabHarm.onClick = [this] { showPage (2); };
    tabOut.onClick = [this] { showPage (3); };
    simple.onLane = [this] (int l) { lanes.highlight (l); showPage (1); };
    Help::get().show = [this] (const juce::String& s) { help = s; content.repaint (statusR); };
    proc.onPresetChanged = [this] { refreshAll(); };

    for (auto* pg : std::initializer_list<juce::Component*> { &simple, &lanes, &harmony, &outputs }) pg->setBounds (0, barH, baseW, baseH - barH - footH);
    showPage (0);
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.5), (int) (baseH * 1.5));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.85), (int) (baseH * 0.85));
    startTimerHz (8);

    // Verificación sin capturar la pantalla: ORRERY_SNAPSHOT=/ruta.png, ORRERY_PAGE=0..3, ORRERY_PRESET=n, ORRERY_RUN=1, ORRERY_WAIT=segundos.
    const auto snap = juce::SystemStats::getEnvironmentVariable ("ORRERY_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        const bool manualOpen = juce::SystemStats::getEnvironmentVariable ("ORRERY_MANUAL", "0").getIntValue() != 0;      // con el manual abierto
        const int presetIndex = juce::SystemStats::getEnvironmentVariable ("ORRERY_PRESET", "-1").getIntValue();
        const int pg = juce::SystemStats::getEnvironmentVariable ("ORRERY_PAGE", "0").getIntValue();
        const bool run = juce::SystemStats::getEnvironmentVariable ("ORRERY_RUN", "0").getIntValue() != 0;
        const int wait = juce::SystemStats::getEnvironmentVariable ("ORRERY_WAIT", "4").getIntValue();
        juce::Component::SafePointer<OrreryEditor> safe (this);
        const bool demo = juce::SystemStats::getEnvironmentVariable ("ORRERY_DEMO", "0").getIntValue() != 0;
        juce::Timer::callAfterDelay (600, [safe, manualOpen, presetIndex, pg, run, demo] {
            if (safe == nullptr) return;
            if (presetIndex >= 0) safe->proc.loadPreset (presetIndex);
            safe->showPage (pg);
            if (manualOpen) safe->manualHost.open (*safe, manualTheme());
            if (demo)   // los mismos ajustes que scripts/samples.py usa para los clips, y sin sonido propio
            {
                safe->proc.setValue ("fade_in", 3.0f); safe->proc.setValue ("session", 0.0f);
                safe->proc.setValue ("fade_cc7", 0.0f); safe->proc.setValue ("preview", 0.0f);
            }
            if (run) safe->proc.setValue ("run", 1.0f);
        });
        const auto rec = juce::SystemStats::getEnvironmentVariable ("ORRERY_RECORD", {});
        if (rec.isNotEmpty())
            recorder = std::make_unique<DemoRecorder> (content, proc, juce::File (rec),
                                                       juce::SystemStats::getEnvironmentVariable ("ORRERY_RECORD_SECS", "30").getDoubleValue(),
                                                       juce::SystemStats::getEnvironmentVariable ("ORRERY_RECORD_SCALE", "1.4545").getFloatValue());
        juce::Timer::callAfterDelay (600 + wait * 1000, [safe, snap] {
            if (safe == nullptr) return;
            const bool man = safe->manualHost.isOpen();
            juce::Component& target = man ? (juce::Component&) *safe : (juce::Component&) safe->content;
            const auto img = target.createComponentSnapshot (target.getLocalBounds(), true, man ? 2.0f * (float) baseW / (float) safe->getWidth() : 2.0f);
            juce::File f (snap); f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        });
    }
}

OrreryEditor::~OrreryEditor() { recorder.reset(); proc.onPresetChanged = nullptr; Help::get().show = nullptr; setLookAndFeel (nullptr); }

void OrreryEditor::refreshAll()
{
    for (auto* pg : std::initializer_list<Page*> { &simple, &lanes, &harmony, &outputs }) pg->refresh();
    content.repaint();
}

void OrreryEditor::showPage (int p)
{
    page = juce::jlimit (0, 3, p);
    simple.setVisible (page == 0); lanes.setVisible (page == 1); harmony.setVisible (page == 2); outputs.setVisible (page == 3);
    simpleBtn.setToggleState (page == 0, juce::dontSendNotification); advBtn.setToggleState (page != 0, juce::dontSendNotification);
    for (auto* t : { &tabLanes, &tabHarm, &tabOut }) t->setVisible (page != 0);
    tabLanes.setToggleState (page == 1, juce::dontSendNotification); tabHarm.setToggleState (page == 2, juce::dontSendNotification); tabOut.setToggleState (page == 3, juce::dontSendNotification);
    resized();
    content.repaint();
}

juce::String OrreryEditor::statusText() const
{
    const auto& v = proc.view();
    auto mmss = [] (float s) { const int t = (int) s; return juce::String (t / 60) + ":" + juce::String (t % 60).paddedLeft ('0', 2); };
    juce::String s = v.running ? "Playing  " + mmss (v.sessionSeconds) : (v.ended ? "Session finished" : "Stopped");
    const int session = (int) proc.value ("session");
    if (session > 0) s << " / " << session << ":00";
    if (proc.value ("breath") > 0.5f) s << "   breathing " << (int) proc.value ("breath") << " per minute";
    return s;
}

void OrreryEditor::paintBackdrop (juce::Graphics& g)
{
    g.fillAll (col::bg);
    // Barra superior.
    g.setColour (col::surface); g.fillRect (0, 0, baseW, barH);
    g.setColour (col::border); g.drawHorizontalLine (barH - 1, 0.0f, (float) baseW);
    // Marca: un sol con una órbita y un planeta, y el nombre.
    const juce::Point<float> c (34.0f, barH / 2.0f);
    g.setColour (col::sun); g.fillEllipse (c.x - 6, c.y - 6, 12, 12);
    g.setColour (col::sun.withAlpha (0.5f)); g.drawEllipse (c.x - 15, c.y - 15, 30, 30, 1.2f);
    g.setColour (col::lane (1)); g.fillEllipse (c.x + 9, c.y - 13, 6, 6);
    g.setColour (col::text); g.setFont (font (24.0f, true));
    g.drawText ("orrery", 58, 0, 110, barH, juce::Justification::centredLeft);
    wordR = { 10, 0, 160, barH };
    // Estado bajo el botón de play.
    g.setColour (col::dim); g.setFont (font (12.5f));
    g.drawText (statusText(), playBtn.getRight() + 14, 0, prevBtn.getX() - playBtn.getRight() - 20, barH, juce::Justification::centredLeft);
    // Pie: ayuda o mensaje, y el clip capturado para arrastrar.
    statusR = { 14, baseH - footH, baseW - 28, footH };
    const bool msg = juce::Time::getMillisecondCounter() < messageUntil;
    g.setColour (msg ? col::sun : col::dim); g.setFont (font (12.5f));
    dragR = {};
    if (captured.existsAsFile())
    {
        dragR = juce::Rectangle<int> (baseW - 300, baseH - footH + 4, 286, footH - 8);
        g.setColour (col::raised); g.fillRoundedRectangle (dragR.toFloat(), 10.0f);
        g.setColour (col::sun.withAlpha (0.7f)); g.drawRoundedRectangle (dragR.toFloat(), 10.0f, 1.0f);
        g.setColour (col::text); g.drawText ("Drag " + captured.getFileName() + " into your DAW", dragR.reduced (10, 0), juce::Justification::centredLeft, true);
    }
    g.setColour (msg ? col::sun : col::dim);
    g.drawText (msg ? message : help, statusR.withTrimmedRight (dragR.isEmpty() ? 0 : 310), juce::Justification::centredLeft, true);
}

void OrreryEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void OrreryEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    const int y = (barH - 34) / 2;
    playBtn.setBounds (186, y - 2, 110, 38);
    int x = 470;
    prevBtn.setBounds (x, y, 34, 34); x += 38;
    presetBtn.setBounds (x, y, 210, 34); x += 214;
    nextBtn.setBounds (x, y, 34, 34); x += 40;
    saveBtn.setBounds (x, y, 64, 34);
    panicBtn.setBounds (baseW - 14 - 120, y, 120, 34);
    captureBtn.setBounds (panicBtn.getX() - 8 - 120, y, 120, 34);
    // Modo y pestañas en una segunda fila dentro de las páginas avanzadas.
    advBtn.setBounds (captureBtn.getX() - 8 - 96, y, 96, 34);
    simpleBtn.setBounds (advBtn.getX() - 4 - 84, y, 84, 34);
    int tx = 14;
    for (auto* t : { &tabLanes, &tabHarm, &tabOut }) { t->setBounds (tx, barH + 10, t == &tabHarm ? 170 : 110, 30); tx += t->getWidth() + 6; }
    const int top = page == 0 ? barH : barH + 44;
    for (auto* pg : std::initializer_list<juce::Component*> { &simple, &lanes, &harmony, &outputs }) pg->setBounds (0, top, baseW, baseH - top - footH);
    manualBtn.setBounds (baseW - 14 - 96, baseH - footH + 3, 96, footH - 6);
    manualHost.layout (getLocalBounds());
}

void OrreryEditor::timerCallback()
{
    const float run = proc.value ("run");
    if (std::abs (run - lastPlay) > 0.1f)
    {
        lastPlay = run;
        playBtn.setButtonText (run > 0.5f ? "Stop" : "Play");
        playBtn.setToggleState (run > 0.5f, juce::dontSendNotification);
    }
    const auto st = statusText();
    if (st != lastStatus) { lastStatus = st; content.repaint (playBtn.getRight(), 0, 320, barH); }
    presetBtn.setButtonText (proc.currentPresetName);
    if (messageUntil != 0 && juce::Time::getMillisecondCounter() > messageUntil) { messageUntil = 0; content.repaint (statusR); }
}

void OrreryEditor::showPresetMenu()
{
    juce::PopupMenu m;
    std::map<juce::String, juce::PopupMenu> cats;
    const auto& ps = proc.getPresets();
    for (int i = 0; i < (int) ps.size(); ++i) cats[ps[(size_t) i].category].addItem (i + 1, ps[(size_t) i].name, true, i == proc.getCurrentProgram());
    for (auto& mood : moods()) if (cats.count (mood)) m.addSubMenu (mood, cats[mood]);
    if (cats.count ("User")) m.addSubMenu ("User", cats["User"]);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [this] (int r) {
        if (r <= 0) return;
        const bool playing = proc.value ("run") > 0.5f;
        proc.loadPreset (r - 1);
        if (playing) proc.setValue ("run", 1.0f);
    });
}

void OrreryEditor::savePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name of the new user preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.currentPresetName);
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r) {
        if (r == 1 && w->getTextEditorContents ("name").trim().isNotEmpty()) proc.saveUserPreset (w->getTextEditorContents ("name").trim());
    }), true);
}

void OrreryEditor::doCapture()
{
    juce::PopupMenu m;
    m.addItem (1, "Last minute"); m.addItem (2, "Last 2 minutes"); m.addItem (5, "Last 5 minutes"); m.addItem (10, "Last 10 minutes");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&captureBtn), [this] (int r) {
        if (r <= 0) return;
        const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Orrery");
        const auto f = proc.captureToFile ((double) r, dir);
        if (f.existsAsFile()) { captured = f; message = "Saved " + f.getFullPathName(); }
        else message = "Nothing to capture yet: press Play first";
        messageUntil = juce::Time::getMillisecondCounter() + 6000;
        content.repaint (0, baseH - footH, baseW, footH);
    });
}
