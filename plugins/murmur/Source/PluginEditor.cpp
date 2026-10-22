#include "PluginEditor.h"
#include "../../../shared/ui/Showing.h"

using namespace dh::mm;
using namespace dh::mm::ui;

MurmurEditor::MurmurEditor (MurmurProcessor& p)
    : AudioProcessorEditor (&p), proc (p), simple (p), advanced (p)
{
    setLookAndFeel (&lnf);
    content.painter = [this] (juce::Graphics& g) { paintBackdrop (g); };
    content.setOpaque (true);
    addAndMakeVisible (content);
    content.setSize (baseW, baseH);
    content.addChildComponent (simple); content.addChildComponent (advanced);
    for (auto* b : { &prevBtn, &nextBtn, &presetBtn, &saveBtn, &simpleBtn, &advBtn, &manualBtn }) content.addAndMakeVisible (b);
    prevBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n > 0) proc.loadPreset ((proc.getCurrentProgram() - 1 + n) % n); };
    nextBtn.onClick = [this] { const int n = (int) proc.getPresets().size(); if (n > 0) proc.loadPreset ((proc.getCurrentProgram() + 1) % n); };
    presetBtn.onClick = [this] { showPresetMenu(); };
    saveBtn.onClick = [this] { savePreset(); };
    simpleBtn.onClick = [this] { showPage (0); };
    advBtn.onClick = [this] { showPage (1); };
    manualBtn.onClick = [this] { showManual (manual == nullptr); };
   #if ! DH_HAS_MANUAL
    manualBtn.setVisible (false);
   #endif
    dh::HelpRouter::add (this);
    proc.onPresetChanged = [this] { refreshAll(); };
    showPage (0);
    setResizable (true, true);
    setResizeLimits (baseW / 2, baseH / 2, (int) (baseW * 1.5), (int) (baseH * 1.5));
    getConstrainer()->setFixedAspectRatio ((double) baseW / baseH);
    setSize ((int) (baseW * 0.85), (int) (baseH * 0.85));
    startTimerHz (8);

    // Capturas sin la pantalla (manual, web): MURMUR_SNAPSHOT=/ruta.png, MURMUR_PAGE=0|1, MURMUR_PRESET=n,
    // MURMUR_WAIT=segundos, MURMUR_MANUAL=1 (con el manual abierto), MURMUR_FREEZE_AT=segundos (activa Freeze a ese
    // tiempo, para mostrar el horizonte congelado). La entrada de audio la da el standalone.
    const auto snap = juce::SystemStats::getEnvironmentVariable ("MURMUR_SNAPSHOT", {});
    if (snap.isNotEmpty())
    {
        const int presetIndex = juce::SystemStats::getEnvironmentVariable ("MURMUR_PRESET", "-1").getIntValue();
        const int pg = juce::SystemStats::getEnvironmentVariable ("MURMUR_PAGE", "0").getIntValue();
        const int wait = juce::SystemStats::getEnvironmentVariable ("MURMUR_WAIT", "4").getIntValue();
        const bool man = juce::SystemStats::getEnvironmentVariable ("MURMUR_MANUAL", "0").getIntValue() != 0;
        const double freezeAt = juce::SystemStats::getEnvironmentVariable ("MURMUR_FREEZE_AT", "-1").getDoubleValue();
        juce::Component::SafePointer<MurmurEditor> safe (this);
        if (freezeAt >= 0.0)
            juce::Timer::callAfterDelay (600 + (int) (freezeAt * 1000.0), [safe] {
                if (safe == nullptr) return;
                if (auto* fp = safe->proc.apvts.getParameter ("freeze")) fp->setValueNotifyingHost (1.0f);
            });
        juce::Timer::callAfterDelay (600, [safe, presetIndex, pg, man] {
            if (safe == nullptr) return;
            if (presetIndex >= 0) safe->proc.loadPreset (presetIndex);
            safe->showPage (pg);
            if (man) safe->showManual (true);
        });
        juce::Timer::callAfterDelay (600 + wait * 1000, [safe, snap, man] {
            if (safe == nullptr) return;
            juce::Component& target = man && safe->manual != nullptr ? (juce::Component&) *safe : (juce::Component&) safe->content;
            const auto img = target.createComponentSnapshot (target.getLocalBounds(), true, 2.0f);
            juce::File f (snap); f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        });
    }
}

MurmurEditor::~MurmurEditor() { proc.onPresetChanged = nullptr; dh::HelpRouter::remove (this); manual.reset(); setLookAndFeel (nullptr); }

void MurmurEditor::showManual (bool on)
{
   #if DH_HAS_MANUAL
    if (! on) { manual.reset(); manualBtn.setToggleState (false, juce::dontSendNotification); return; }
    dh::manual::Theme t;
    t.bg = col::bg; t.panel = col::surface; t.text = col::text; t.muted = col::dim; t.accent = col::sun; t.line = col::border;
    auto m = dh::manual::makeEmbedded (t);
    m->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MurmurEditor> (this)] { if (safe != nullptr) safe->showManual (false); }); };
    addAndMakeVisible (*m);
    m->setBounds (getLocalBounds());
    m->grabKeyboardFocus();
    manual = std::move (m);
    manualBtn.setToggleState (true, juce::dontSendNotification);
   #else
    juce::ignoreUnused (on);
   #endif
}

void MurmurEditor::showPage (int p)
{
    page = juce::jlimit (0, 1, p);
    simple.setVisible (page == 0); advanced.setVisible (page == 1);
    simpleBtn.setToggleState (page == 0, juce::dontSendNotification);
    advBtn.setToggleState (page == 1, juce::dontSendNotification);
    refreshAll();
}

void MurmurEditor::refreshAll()
{
    presetBtn.setButtonText (proc.currentPresetName.get());
    simple.refresh(); advanced.refresh();
    content.repaint();
}

void MurmurEditor::paint (juce::Graphics& g) { g.fillAll (col::bg); }

void MurmurEditor::resized()
{
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) baseW));
    const int y = (barH - 34) / 2;
    int x = 300;
    prevBtn.setBounds (x, y, 34, 34); x += 38;
    presetBtn.setBounds (x, y, 230, 34); x += 234;
    nextBtn.setBounds (x, y, 34, 34); x += 40;
    saveBtn.setBounds (x, y, 64, 34);
    manualBtn.setBounds (baseW - 14 - 96, y, 96, 34);
    advBtn.setBounds (manualBtn.getX() - 12 - 104, y, 104, 34);
    simpleBtn.setBounds (advBtn.getX() - 4 - 90, y, 90, 34);
    simple.setBounds (0, barH, baseW, baseH - barH - footH);
    advanced.setBounds (0, barH, baseW, baseH - barH - footH);
    statusR = { 0, baseH - footH, baseW, footH };
    if (manual != nullptr) manual->setBounds (getLocalBounds());
}

void MurmurEditor::paintBackdrop (juce::Graphics& g)
{
    g.fillAll (col::bg);
    // Logotipo: tres pájaros y la palabra.
    juce::Path birds;
    for (int i = 0; i < 3; ++i)
    {
        const float bx = 26.0f + (float) i * 11.0f, by = 30.0f - (float) (i % 2) * 6.0f, s = 5.0f - (float) i;
        birds.startNewSubPath (bx - s, by - s * 0.45f); birds.quadraticTo (bx - s * 0.35f, by - s * 0.7f, bx, by);
        birds.quadraticTo (bx + s * 0.35f, by - s * 0.7f, bx + s, by - s * 0.45f);
    }
    g.setColour (col::sun); g.strokePath (birds, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (col::text); g.setFont (font (25.0f, true));
    g.drawText ("murmur", 66, 14, 160, 32, juce::Justification::centredLeft);
    // Medidores de entrada y salida.
    auto meter = [&] (int mx, const juce::String& label, float v) {
        g.setColour (col::dim); g.setFont (font (11.0f)); g.drawText (label, mx, 10, 30, 14, juce::Justification::centredLeft);
        g.setColour (col::border); g.fillRoundedRectangle ((float) mx, 28.0f, 30.0f, 8.0f, 4.0f);
        g.setColour (v > 0.98f ? col::sun : col::select); g.fillRoundedRectangle ((float) mx, 28.0f, 30.0f * juce::jlimit (0.0f, 1.0f, std::sqrt (v)), 8.0f, 4.0f);
    };
    meter (194, "in", proc.inLevel.load()); meter (236, "out", proc.outLevel.load());
    g.setColour (col::border); g.fillRect (0, barH - 1, baseW, 1);
    // Barra de ayuda.
    g.setColour (col::surface); g.fillRect (statusR);
    g.setColour (help.isNotEmpty() ? col::text : col::faint); g.setFont (font (13.0f));
    g.drawText (help.isNotEmpty() ? help : "Hover over a control to see what it does.   CPU " + juce::String (proc.cpuLoad.load() * 100.0f, 1) + " %",
                statusR.reduced (14, 0), juce::Justification::centredLeft);
}

void MurmurEditor::timerCallback()
{
    // Live crea el editor al insertar y después sólo esconde la ventana: escondido no se anima ni se repinta (QA en
    // Live 2026-10-22, H3).
    if (! dh::reallyShowing (*this)) return;
    if (presetBtn.getButtonText() != proc.currentPresetName.get()) presetBtn.setButtonText (proc.currentPresetName.get());
    content.repaint (0, 0, 290, barH);
    if (help.isEmpty()) content.repaint (statusR);
}

void MurmurEditor::showPresetMenu()
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
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetBtn), [this] (int r) { if (r > 0) proc.loadPreset (r - 1); });
}

void MurmurEditor::savePreset()
{
    auto* w = new juce::AlertWindow ("Save preset", "Name of the new user preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", proc.currentPresetName.get());
    w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r) {
        if (r == 1 && w->getTextEditorContents ("name").trim().isNotEmpty()) proc.saveUserPreset (w->getTextEditorContents ("name").trim());
    }), true);
}
