#include "PluginEditor.h"

using namespace dh::val;
using namespace dh::val::ui;

ValenceEditor::ValenceEditor (ValenceProcessor& p)
    : AudioProcessorEditor (&p), proc (p), simple (p), layers (p), harmony (p), outputs (p),
      previewBtn (p, "preview", "Preview sound", "Valence's own built-in sound, to try it without routing. Turn it off when your synths play: the MIDI is the same either way")
{
    setLookAndFeel (&lnf);
    content.painter = [this] (juce::Graphics& g) { paintBackdrop (g); };
    content.drag = [this] (const juce::MouseEvent& e) {
        if (captured.existsAsFile() && dragR.contains (e.getMouseDownPosition()))
            performExternalDragDropOfFiles ({ captured.getFullPathName() }, false, this);
    };
    content.setOpaque (true);
    addAndMakeVisible (content);
    content.setSize (baseW, baseH);
    for (auto* pg : pages) content.addChildComponent (pg);
    content.addAndMakeVisible (previewBtn);
    for (auto* b : { &playBtn, &prevBtn, &nextBtn, &presetBtn, &saveBtn, &captureBtn, &panicBtn }) content.addAndMakeVisible (b);
    static const char* tabHelp[4] = { "the atom and six macros", "every layer with all its step rows", "input, chords, scale and time", "where each layer's MIDI goes" };
    for (int i = 0; i < 4; ++i)
    {
        auto& t = tabs[(size_t) i];
        content.addAndMakeVisible (t);
        t.onClick = [this, i] { showPage (i); };
        t.setTooltip (t.getButtonText() + ": " + tabHelp[i]);
    }
    playBtn.onClick = [this] { proc.setValue ("run", proc.value ("run") > 0.5f ? 0.0f : 1.0f); };
    auto step = [this] (int d) {
        const int n = (int) proc.getPresets().size();
        if (n <= 0) return;
        const bool r = proc.value ("run") > 0.5f;
        proc.loadPreset ((proc.getCurrentProgram() + d + n) % n);
        if (r) proc.setValue ("run", 1.0f);
    };
    prevBtn.onClick = [step] { step (-1); };
    nextBtn.onClick = [step] { step (1); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    saveBtn.onClick = [this] { savePreset(); };
    captureBtn.onClick = [this] { doCapture(); };
    panicBtn.onClick = [this] { proc.panic(); };
    playBtn.setTooltip ("Play: with Harmony on Auto, Valence plays by itself. With your notes it plays whenever you hold a chord");
    dh::HelpRouter::add (this);
    proc.onPresetChanged = [this] { for (auto* pg : pages) pg->refresh(); content.repaint(); };
    showPage (juce::SystemStats::getEnvironmentVariable ("VALENCE_PAGE", juce::String (proc.editorPage)).getIntValue());
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.5), (int) (baseH * 1.5));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.85), (int) (baseH * 0.85));
    startTimerHz (8);
    lastPlay = -1.0f; timerCallback();          // los textos de Play y del preset desde el primer cuadro

    // Grabación de una demo (VALENCE_RECORD=carpeta, VALENCE_DEMO=carpeta de la demo): ver ui/AtomRecorder.h.
    if (const auto rec = juce::SystemStats::getEnvironmentVariable ("VALENCE_RECORD", {}); rec.isNotEmpty())
    {
        auto env = [] (const char* n, const char* d) { return juce::SystemStats::getEnvironmentVariable (n, d); };
        recorder = std::make_unique<AtomRecorder> (juce::File (rec), juce::File (env ("VALENCE_DEMO", "")),
                                                   env ("VALENCE_RECORD_FROM", "0").getDoubleValue(), env ("VALENCE_RECORD_SECS", "15").getDoubleValue(),
                                                   env ("VALENCE_RECORD_W", "1280").getIntValue(), env ("VALENCE_RECORD_H", "720").getIntValue(),
                                                   env ("VALENCE_RECORD_SCALE", "1").getFloatValue());
    }

    // Verificación sin capturar la pantalla: VALENCE_SNAPSHOT=/ruta.png VALENCE_PAGE=0-3 VALENCE_PRESET=n VALENCE_RUN=1 VALENCE_WAIT=s.
    // VALENCE_SNAPSHOT_SCALE=2 para las capturas de la web (el manual usa 1).
    const auto snap = juce::SystemStats::getEnvironmentVariable ("VALENCE_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        const int presetIndex = juce::SystemStats::getEnvironmentVariable ("VALENCE_PRESET", "-1").getIntValue();
        const int pg = juce::SystemStats::getEnvironmentVariable ("VALENCE_PAGE", "0").getIntValue();
        const bool run = juce::SystemStats::getEnvironmentVariable ("VALENCE_RUN", "0").getIntValue() != 0;
        const int wait = juce::SystemStats::getEnvironmentVariable ("VALENCE_WAIT", "4").getIntValue();
        const bool mute = juce::SystemStats::getEnvironmentVariable ("VALENCE_MUTE", "0").getIntValue() != 0;    // sin el sonido de prueba (mediciones de CPU)
        // Capturas del manual: VALENCE_CLEAN=1 toma lo que RUN cambió como el preset (sin el " *" de modificado).
        const bool clean = juce::SystemStats::getEnvironmentVariable ("VALENCE_CLEAN", "0").getIntValue() != 0;
        juce::Component::SafePointer<ValenceEditor> safe (this);
        if (juce::SystemStats::getEnvironmentVariable ("VALENCE_TOP", "0").getIntValue() != 0)     // mediciones de CPU: la ventana siempre a la vista, para que no cuente como tapada
            juce::Timer::callAfterDelay (800, [safe] { if (safe != nullptr) if (auto* w = safe->getTopLevelComponent()) w->setAlwaysOnTop (true); });
        juce::Timer::callAfterDelay (600, [safe, presetIndex, pg, run, mute, clean] {
            if (safe == nullptr) return;
            if (presetIndex >= 0) safe->proc.loadPreset (presetIndex);
            safe->showPage (pg);
            if (mute) safe->proc.setValue ("preview", 0.0f);
            if (run) { safe->proc.setValue ("source", 1.0f); safe->proc.setValue ("run", 1.0f); }
            if (clean) safe->proc.tracker.capture (safe->proc);
        });
        const float scale = juce::SystemStats::getEnvironmentVariable ("VALENCE_SNAPSHOT_SCALE", "1").getFloatValue();
        juce::Timer::callAfterDelay (600 + wait * 1000, [safe, snap, scale] {
            if (safe == nullptr) return;
            const auto img = safe->content.createComponentSnapshot (safe->content.getLocalBounds(), true, scale);
            juce::File f (snap); f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        });
    }
}

ValenceEditor::~ValenceEditor() { recorder.reset(); proc.onPresetChanged = nullptr; dh::HelpRouter::remove (this); setLookAndFeel (nullptr); }

void ValenceEditor::showPage (int p)
{
    page = juce::jlimit (0, (int) pages.size() - 1, p);
    proc.editorPage = page;
    for (size_t i = 0; i < pages.size(); ++i)
    {
        pages[i]->setVisible ((int) i == page);
        tabs[i].setToggleState ((int) i == page, juce::dontSendNotification);
    }
    resized();
    content.repaint();
}

void ValenceEditor::paintBackdrop (juce::Graphics& g)
{
    g.fillAll (col::bg);
    // JUCE no descuenta las páginas opacas de este fondo (está escalado: OpaqueLayer ignora los componentes con
    // transformación), así que cada cuadro del átomo lo volvería a pintar entero: sólo la barra y el pie, si tocan el clip.
    const auto clip = g.getClipBounds();
    if (! clip.intersects (juce::Rectangle<int> (0, 0, baseW, barH)) && ! clip.intersects (juce::Rectangle<int> (0, baseH - footH, baseW, footH))) return;
    g.setColour (col::surface); g.fillRect (0, 0, baseW, barH);
    g.setColour (col::border); g.drawHorizontalLine (barH - 1, 0.0f, (float) baseW);
    // Marca: un núcleo de dos nucleones y dos órbitas de radios e inclinaciones distintas, y el nombre.
    const juce::Point<float> c (32.0f, barH / 2.0f);
    g.setColour (col::sun); g.fillEllipse (c.x - 5.5f, c.y - 3.5f, 7, 7);
    g.setColour (col::cold); g.fillEllipse (c.x - 0.5f, c.y - 2.0f, 6, 6);
    juce::Path o1; o1.addEllipse (c.x - 17, c.y - 7, 34, 14); o1.applyTransform (juce::AffineTransform::rotation (0.5f, c.x, c.y));
    juce::Path o2; o2.addEllipse (c.x - 12, c.y - 15, 24, 30); o2.applyTransform (juce::AffineTransform::rotation (-0.35f, c.x, c.y));
    g.setColour (col::layer (0).withAlpha (0.7f)); g.strokePath (o1, juce::PathStrokeType (1.1f));
    g.setColour (col::layer (1).withAlpha (0.6f)); g.strokePath (o2, juce::PathStrokeType (1.1f));
    g.setColour (col::text); g.setFont (font (24.0f, true));
    g.drawText ("valence", 58, 4, 120, 34, juce::Justification::centredLeft);
    const auto& v = proc.view();
    g.setColour (col::dim); g.setFont (font (11.5f));
    juce::String st = v.running ? "playing, " + juce::String (v.bpm, 1) + " BPM" : "waiting for a chord";
    if (v.chord[0] != 0) st << "  " << juce::String (v.chord);
    g.drawFittedText (st, 59, 36, playBtn.getX() - 66, 18, juce::Justification::centredLeft, 1, 0.8f);
    statusR = { 14, baseH - footH, baseW - 28, footH };
    const bool msg = juce::Time::getMillisecondCounter() < messageUntil;
    dragR = {};
    if (captured.existsAsFile())
    {
        dragR = juce::Rectangle<int> (baseW - 300, baseH - footH + 4, 286, footH - 8);
        g.setColour (col::raised); g.fillRoundedRectangle (dragR.toFloat(), 10.0f);
        g.setColour (col::sun.withAlpha (0.7f)); g.drawRoundedRectangle (dragR.toFloat(), 10.0f, 1.0f);
        g.setColour (col::text); g.setFont (font (12.5f)); g.drawText ("Drag " + captured.getFileName() + " into your DAW", dragR.reduced (10, 0), juce::Justification::centredLeft, true);
    }
    g.setColour (msg ? col::sun : col::dim); g.setFont (font (12.5f));
    g.drawText (msg ? message : help, statusR.withTrimmedRight (dragR.isEmpty() ? 0 : 310), juce::Justification::centredLeft, true);
}

void ValenceEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void ValenceEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    const int y = (barH - 34) / 2;
    playBtn.setBounds (196, y - 2, 100, 38);
    previewBtn.setBounds (playBtn.getRight() + 10, y - 2, 166, 38);
    int x = previewBtn.getRight() + 12;
    prevBtn.setBounds (x, y, 34, 34); x += 38;
    presetBtn.setBounds (x, y, 150, 34); x += 154;
    nextBtn.setBounds (x, y, 34, 34); x += 40;
    saveBtn.setBounds (x, y, 60, 34);
    panicBtn.setBounds (baseW - 14 - 100, y, 100, 34);
    captureBtn.setBounds (panicBtn.getX() - 6 - 108, y, 108, 34);
    static const int tabW[4] = { 64, 66, 78, 74 };
    x = captureBtn.getX() - 14;
    for (int i = 3; i >= 0; --i) { x -= tabW[i]; tabs[(size_t) i].setBounds (x, y, tabW[i], 34); x -= 4; }
    for (auto* pg : pages) pg->setBounds (0, barH, baseW, baseH - barH - footH);
}

void ValenceEditor::timerCallback()
{
    if (lastPlay >= 0.0f && ! dh::reallyShowing (*this)) return;      // editor escondido (Live lo esconde sin cerrarlo): nada que repintar
    const float run = proc.value ("run");
    if (std::abs (run - lastPlay) > 0.1f)
    {
        lastPlay = run;
        playBtn.setButtonText (run > 0.5f ? "Stop" : "Play");
        playBtn.setToggleState (run > 0.5f, juce::dontSendNotification);
    }
    presetBtn.setButtonText (proc.presetDisplayName());
    content.repaint (0, 0, playBtn.getX(), barH);
    if (messageUntil != 0 && juce::Time::getMillisecondCounter() > messageUntil) { messageUntil = 0; content.repaint (statusR); }
}

void ValenceEditor::showPresetMenu()
{
    juce::PopupMenu m;
    std::map<juce::String, juce::PopupMenu> cats;
    const auto& ps = proc.getPresets();
    for (int i = 0; i < (int) ps.size(); ++i) cats[ps[(size_t) i].category].addItem (i + 1, ps[(size_t) i].name, true, i == proc.getCurrentProgram());
    for (auto& grp : ValenceProcessor::groups()) if (cats.count (grp)) m.addSubMenu (grp, cats[grp]);
    if (cats.count ("User")) m.addSubMenu ("User", cats["User"]);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [this] (int r) {
        if (r <= 0) return;
        const bool playing = proc.value ("run") > 0.5f;
        proc.loadPreset (r - 1);
        if (playing) proc.setValue ("run", 1.0f);
    });
}

void ValenceEditor::savePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name of the new user preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.currentPresetName.get());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r) {
        if (r == 1 && w->getTextEditorContents ("name").trim().isNotEmpty()) proc.saveUserPreset (w->getTextEditorContents ("name").trim());
    }), true);
}

void ValenceEditor::doCapture()
{
    juce::PopupMenu m;
    m.addItem (1, "Last minute"); m.addItem (2, "Last 2 minutes"); m.addItem (5, "Last 5 minutes");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&captureBtn), [this] (int r) {
        if (r <= 0) return;
        const auto dir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Valence");
        const auto f = proc.captureToFile ((double) r, dir);
        if (f.existsAsFile()) { captured = f; message = "Saved " + f.getFullPathName(); }
        else message = "Nothing to capture yet: hold a chord first";
        messageUntil = juce::Time::getMillisecondCounter() + 6000;
        content.repaint (0, baseH - footH, baseW, footH);
    });
}
