#include "PluginEditor.h"
#include "../../../shared/ui/Showing.h"

using namespace dh::md;
using namespace dh::md::ui;

AtmdEditor::AtmdEditor (AtmdProcessor& p)
    : AudioProcessorEditor (&p), proc (p), panel (p), settings (p)
{
    setLookAndFeel (&lnf);
    content.painter = [this] (juce::Graphics& g) { paintBackdrop (g); };
    content.setOpaque (true);
    addAndMakeVisible (content);
    content.setSize (baseW, baseH);
    content.addAndMakeVisible (panel);
    content.addChildComponent (settings);
    for (auto* b : { &prevBtn, &nextBtn, &presetBtn, &saveBtn, &panelBtn, &settingsBtn, &manualBtn }) content.addAndMakeVisible (b);
    prevBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n > 0) proc.loadPreset ((proc.getCurrentProgram() - 1 + n) % n); };
    nextBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n > 0) proc.loadPreset ((proc.getCurrentProgram() + 1) % n); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    saveBtn.onClick = [this] { savePreset(); };
    panelBtn.onClick = [this] { showPage (0); };
    settingsBtn.onClick = [this] { showPage (1); };
    manualBtn.onClick = [this] { showManual (manual == nullptr); };
   #if ! DH_HAS_MANUAL
    manualBtn.setVisible (false);
   #endif
    panel.onHelp = [this] (const juce::String& s) { setHelp (s); };
    settings.onHelp = [this] (const juce::String& s) { setHelp (s); };
    proc.onPresetChanged = [this] { presetBtn.setButtonText (proc.currentPresetName.get()); content.repaint(); };
    presetBtn.setButtonText (proc.currentPresetName.get());
    showPage (0);
    setWantsKeyboardFocus (true);
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.5), (int) (baseH * 1.5));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.8), (int) (baseH * 0.8));
    startTimerHz (8);

    // Capturas sin la pantalla (manual, web): ATMD_SNAPSHOT=/ruta.png, ATMD_PAGE=0|1, ATMD_PRESET=n, ATMD_WAIT=segundos,
    // ATMD_MANUAL=1 (con el manual abierto), ATMD_HOLD="48,55,60" (teclas apretadas), ATMD_MOD y ATMD_BEND (ruedas).
    const auto snap = juce::SystemStats::getEnvironmentVariable ("ATMD_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        auto env = [] (const char* n, const char* d) { return juce::SystemStats::getEnvironmentVariable (n, d); };
        const int presetIndex = env ("ATMD_PRESET", "-1").getIntValue();
        const int pg = env ("ATMD_PAGE", "0").getIntValue();
        const int wait = env ("ATMD_WAIT", "3").getIntValue();
        const bool man = env ("ATMD_MANUAL", "0").getIntValue() != 0;
        const auto hold = juce::StringArray::fromTokens (env ("ATMD_HOLD", ""), ",", "");
        const float mod = env ("ATMD_MOD", "-1").getFloatValue(), bend = env ("ATMD_BEND", "0").getFloatValue();
        juce::Component::SafePointer<AtmdEditor> safe (this);
        juce::Timer::callAfterDelay (600, [safe, presetIndex, pg, man, hold, mod, bend] {
            if (safe == nullptr) return;
            if (presetIndex >= 0) safe->proc.loadPreset (presetIndex);
            safe->showPage (pg);
            for (auto& h : hold) if (h.trim().isNotEmpty()) safe->proc.keyboardState.noteOn (1, h.getIntValue(), 0.8f);
            if (mod >= 0.0f) safe->proc.modWheel.store (mod);
            safe->proc.pitchWheel.store (bend);
            if (man) safe->showManual (true);
        });
        juce::Timer::callAfterDelay (600 + wait * 1000, [safe, snap, man] {
            if (safe == nullptr) return;
            juce::Component& target = man && safe->manual != nullptr ? (juce::Component&) *safe : (juce::Component&) safe->content;
            const auto img = target.createComponentSnapshot (target.getLocalBounds(), true, man ? 2.0f * (float) baseW / (float) safe->getWidth() : 2.0f);
            juce::File f (snap); f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        });
    }
}

AtmdEditor::~AtmdEditor() { proc.onPresetChanged = nullptr; manual.reset(); setLookAndFeel (nullptr); }

void AtmdEditor::showManual (bool on)
{
   #if DH_HAS_MANUAL
    if (! on) { manual.reset(); manualBtn.setToggleState (false, juce::dontSendNotification); return; }
    dh::manual::Theme t;
    t.bg = juce::Colour (0xff15110d); t.panel = juce::Colour (0xff1f1913); t.text = col::text; t.muted = col::dim; t.accent = col::orange; t.line = col::barLine;
    auto m = dh::manual::makeEmbedded (t);
    m->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<AtmdEditor> (this)] { if (safe != nullptr) safe->showManual (false); }); };
    addAndMakeVisible (*m);
    m->setBounds (getLocalBounds());
    m->grabKeyboardFocus();
    manual = std::move (m);
    manualBtn.setToggleState (true, juce::dontSendNotification);
   #else
    juce::ignoreUnused (on);
   #endif
}

void AtmdEditor::showPage (int p)
{
    page = juce::jlimit (0, 1, p);
    settings.setVisible (page == 1);
    panelBtn.setToggleState (page == 0, juce::dontSendNotification);
    settingsBtn.setToggleState (page == 1, juce::dontSendNotification);
    content.repaint();
}

void AtmdEditor::paint (juce::Graphics& g) { g.fillAll (col::bar); }

void AtmdEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    const int y = (barH - 32) / 2;
    int x = 250;
    prevBtn.setBounds (x, y, 32, 32); x += 36;
    presetBtn.setBounds (x, y, 250, 32); x += 254;
    nextBtn.setBounds (x, y, 32, 32); x += 40;
    saveBtn.setBounds (x, y, 70, 32);
    manualBtn.setBounds (baseW - 14 - 100, y, 100, 32);
    settingsBtn.setBounds (manualBtn.getX() - 16 - 112, y, 112, 32);
    panelBtn.setBounds (settingsBtn.getX() - 4 - 96, y, 96, 32);
    panel.setBounds (0, barH, baseW, panelH);
    // La página de ajustes tapa el panel y la tabla del nombre, en la misma escala que el fondo.
    const float s = (float) baseW / (float) Assets::get().width;
    const auto a = SettingsView::area();
    settings.setBounds (juce::roundToInt (a.getX() * s), barH + juce::roundToInt (a.getY() * s), juce::roundToInt (a.getWidth() * s), juce::roundToInt (a.getHeight() * s));
    statusR = { 0, baseH - footH, baseW, footH };
    if (manual != nullptr) manual->setBounds (getLocalBounds());
}

void AtmdEditor::paintBackdrop (juce::Graphics& g)
{
    g.fillAll (col::bar);
    g.setColour (col::text); g.setFont (barFont (26.0f, true));
    g.drawText ("ATMD", 18, 0, 100, barH, juce::Justification::centredLeft);
    g.setColour (col::dim); g.setFont (barFont (12.0f));
    g.drawText ("AUDIO TOOLS", 104, 0, 120, barH, juce::Justification::centredLeft);
    g.setColour (col::barLine); g.fillRect (0, barH - 1, baseW, 1);
    // Barra de ayuda, con la CPU y las voces sonando cuando no hay nada bajo el mouse.
    g.setColour (col::bar); g.fillRect (statusR);
    g.setColour (col::barLine); g.fillRect (statusR.getX(), statusR.getY(), statusR.getWidth(), 1);
    g.setColour (help.isNotEmpty() ? col::text : col::faint); g.setFont (barFont (13.5f));
    const juce::String idle = "Hover over a control to see what it does. Computer keys A to K play notes, Z and X change octave.";
    g.drawText (help.isNotEmpty() ? help : idle, statusR.reduced (14, 0).withTrimmedRight (240), juce::Justification::centredLeft);
    g.setColour (col::faint);
    g.drawText ("voices " + juce::String (proc.voicesActive.load()) + "   CPU " + juce::String (proc.cpuLoad.load() * 100.0f, 1) + " %   ATMD " JucePlugin_VersionString,
                statusR.reduced (14, 0), juce::Justification::centredRight);
}

void AtmdEditor::timerCallback()
{
    if (! dh::reallyShowing (*this)) return;      // ventana escondida (Live): nada que repintar (H3)
    if (presetBtn.getButtonText() != proc.currentPresetName.get()) presetBtn.setButtonText (proc.currentPresetName.get());
    content.repaint (statusR.withTrimmedLeft (baseW - 260));
}

// Teclado de la computadora: A W S E D F T G Y H U J K = una octava desde C; Z / X bajan y suben la octava.
static int qwertyNote (int key)
{
    const juce::String row = "awsedftgyhujkolp";
    const int i = row.indexOfChar ((juce::juce_wchar) juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) key));
    return i;
}

bool AtmdEditor::keyPressed (const juce::KeyPress& k)
{
    const auto c = juce::CharacterFunctions::toLowerCase (k.getTextCharacter());
    if (c == 'z') { octave = juce::jmax (1, octave - 1); return true; }
    if (c == 'x') { octave = juce::jmin (7, octave + 1); return true; }
    const int i = qwertyNote (c);
    if (i < 0) return false;
    const int note = 12 * octave + i;
    if (qwertyDown.emplace (k.getKeyCode(), note).second) proc.keyboardState.noteOn (1, note, 0.8f);
    return true;
}

bool AtmdEditor::keyStateChanged (bool)
{
    for (auto it = qwertyDown.begin(); it != qwertyDown.end();)
        if (! juce::KeyPress::isKeyCurrentlyDown (it->first))
        {
            // Sólo la nota que disparó esta tecla (antes se soltaba esa nota en todas las octavas, y cortaba notas del
            // teclado MIDI o de otras teclas).
            proc.keyboardState.noteOff (1, it->second, 0.0f);
            it = qwertyDown.erase (it);
        }
        else ++it;
    return false;
}

void AtmdEditor::showPresetMenu()
{
    juce::PopupMenu m;
    std::map<juce::String, juce::PopupMenu> cats;
    juce::StringArray order;
    const auto& ps = proc.getPresets();
    for (int i = 0; i < (int) ps.size(); ++i)
    {
        const auto& c = ps[(size_t) i].category;
        if (! order.contains (c)) order.add (c);
        cats[c].addItem (i + 1, ps[(size_t) i].name, true, i == proc.getCurrentProgram());
    }
    for (auto& c : order) m.addSubMenu (c, cats[c]);
    m.addSeparator();
    m.addItem (-1, "Show user presets folder");
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [this] (int r) {
        if (r > 0) proc.loadPreset (r - 1);
        else if (r == -1) { AtmdProcessor::userPresetDir().createDirectory(); AtmdProcessor::userPresetDir().revealToUser(); }
    });
}

void AtmdEditor::savePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name of the new user preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.currentPresetName.get());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    // La ventana es otra ventana del escritorio: puede seguir abierta después de cerrar el editor.
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<juce::Component> (this), p = &proc, w] (int r) {
        if (safe == nullptr) return;
        if (r == 1 && w->getTextEditorContents ("name").trim().isNotEmpty()) p->saveUserPreset (w->getTextEditorContents ("name").trim());
    }), true);
}
