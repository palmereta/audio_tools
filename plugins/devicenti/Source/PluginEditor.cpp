#include "PluginEditor.h"
#include "ui/UiTest.h"
#include "ManualView.h"

using namespace dh::dv::ui;

DeVicentiEditor::DeVicentiEditor (DeVicentiProcessor& p)
    : AudioProcessorEditor (p), proc (p), unit (p, state), fieldPage (p, state), seqPage (p, state), settingsPage (p)
{
    addAndMakeVisible (bar);
    addAndMakeVisible (unit);
    addChildComponent (fieldPage); addChildComponent (seqPage); addChildComponent (settingsPage);
    unit.onOpenPage = [this] (Page pg) { openPage ((int) pg); };
    unit.onLearnArmed = [this] { bar.repaint(); };
    settingsPage.onScale = [this] (float s) { setSize (juce::roundToInt (kBaseW * s), juce::roundToInt (kBaseH * s)); };
    proc.onPresetChanged = [this] { bar.repaint(); if (page >= 0) fieldPage.rebuild(); };

    setResizable (true, true);
    setResizeLimits (juce::roundToInt (kBaseW * 0.6), juce::roundToInt (kBaseH * 0.6), juce::roundToInt (kBaseW * 1.6), juce::roundToInt (kBaseH * 1.6));
    if (auto* c = getConstrainer()) c->setFixedAspectRatio ((double) kBaseW / (double) kBaseH);
    const int w = juce::SystemStats::getEnvironmentVariable ("DV_W", juce::String (kBaseW)).getIntValue();
    setSize (w, juce::roundToInt ((double) w * kBaseH / kBaseW));
    setupSnapshot();
}

DeVicentiEditor::~DeVicentiEditor() { proc.onPresetChanged = nullptr; manual.reset(); }

void DeVicentiEditor::paint (juce::Graphics& g) { g.fillAll (Theme::bg()); }

void DeVicentiEditor::resized()
{
    const float s = (float) getWidth() / (float) kBaseW;
    auto r = getLocalBounds();
    bar.setBounds (r.removeFromTop (juce::roundToInt (kBarH * s)));
    for (juce::Component* c : { (juce::Component*) &unit, (juce::Component*) &fieldPage, (juce::Component*) &seqPage, (juce::Component*) &settingsPage }) c->setBounds (r);
    if (manual != nullptr) manual->setBounds (getLocalBounds());
}

void DeVicentiEditor::toggleManual()
{
   #if DH_HAS_MANUAL
    if (manual != nullptr) { manual.reset(); bar.repaint(); return; }
    dh::manual::Theme t; t.bg = Theme::bg(); t.panel = Theme::panel(); t.text = Theme::ink(); t.muted = Theme::dim(); t.accent = Theme::accent(); t.line = Theme::ink().withAlpha (0.15f);
    auto m = dh::manual::makeEmbedded (t);
    m->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<DeVicentiEditor> (this)] { if (safe != nullptr) { safe->manual.reset(); safe->bar.repaint(); } }); };
    addAndMakeVisible (*m); m->setBounds (getLocalBounds()); m->grabKeyboardFocus();
    manual = std::move (m);
   #endif
}

void DeVicentiEditor::openPage (int pg)
{
    page = pg;
    unit.setVisible (pg < 0);
    fieldPage.setVisible (pg >= 0 && pg != (int) Page::Seq && pg != (int) Page::Settings);
    seqPage.setVisible (pg == (int) Page::Seq);
    settingsPage.setVisible (pg == (int) Page::Settings);
    if (fieldPage.isVisible()) fieldPage.setPage ((Page) pg);
    if (pg < 0) unit.grabKeyboardFocus();
    bar.repaint();
}

// ───────────── top bar ─────────────
void DeVicentiEditor::TopBar::paint (juce::Graphics& g)
{
    const float s = (float) getWidth() / (float) kBaseW;
    auto b = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff221915), 0, 0, juce::Colour (0xff15100d), 0, b.getHeight(), false));
    g.fillRect (b);
    g.setColour (juce::Colours::black.withAlpha (0.5f)); g.fillRect (b.removeFromBottom (1.0f));
    hits.clear();
    auto r = b.reduced (10 * s, 4 * s);
    g.setColour (Theme::ink()); g.setFont (Theme::font (15.0f * s));
    g.drawText ("Audio Tools DeviceNTI V", r.removeFromLeft (196 * s), juce::Justification::centredLeft);
    if (ed.page >= 0)
    {
        auto back = r.removeFromLeft (110 * s).reduced (2 * s, 1 * s);
        g.setColour (Theme::accent()); g.fillRoundedRectangle (back, 4 * s);
        g.setColour (juce::Colour (0xff1b1410)); g.setFont (Theme::font (13.5f * s));
        g.drawText (juce::CharPointer_UTF8 ("\xe2\x97\x80 PANEL"), back, juce::Justification::centred);
        hits.push_back ({ "back", back });
        g.setColour (Theme::accent2()); g.setFont (Theme::font (14.0f * s));
        g.drawText (pageName ((Page) ed.page), r.removeFromLeft (120 * s).withTrimmedLeft (8 * s), juce::Justification::centredLeft);
    }
    // Buttons on the right first: the preset browser takes the room left between them and the title.
    auto rr = b.reduced (10 * s, 4 * s);
    auto button = [&] (const juce::String& id, const juce::String& label, float w, bool on) {
        auto x = rr.removeFromRight (w * s).reduced (2 * s, 1 * s); rr.removeFromRight (4 * s);
        g.setColour (on ? Theme::accent() : juce::Colour (0xff2c221c)); g.fillRoundedRectangle (x, 4 * s);
        g.setColour (on ? juce::Colour (0xff1b1410) : Theme::ink()); g.setFont (Theme::font (13.0f * s));
        g.drawText (label, x, juce::Justification::centred);
        hits.push_back ({ id, x });
    };
    button ("save", "SAVE", 56, false);
   #if DH_HAS_MANUAL
    button ("manual", "MANUAL", 72, ed.manual != nullptr);
   #endif
    button ("settings", "SETTINGS", 82, ed.page == (int) Page::Settings);
    button ("midi", "MIDI", 52, ed.page == (int) Page::Midi);
    button ("pages", "PAGES", 64, ed.page >= 0 && ed.page != (int) Page::Midi && ed.page != (int) Page::Settings);
    button ("learn", "LEARN", 62, ed.unit.learnMode);
    // Preset browser: centred on the bar when it fits, otherwise centred in the free gap (never under the buttons).
    const float gapL = r.getX() + 8 * s, gapR = rr.getRight() - 8 * s;
    const float pw = juce::jmin (330 * s, gapR - gapL);
    const float px = juce::jlimit (gapL, gapR - pw, b.getCentreX() - pw / 2);
    auto pr = juce::Rectangle<float> (px, r.getY(), pw, r.getHeight());
    g.setColour (juce::Colour (0xff0e0b09)); g.fillRoundedRectangle (pr, 4 * s);
    g.setColour (juce::Colours::white.withAlpha (0.08f)); g.drawRoundedRectangle (pr, 4 * s, 1.0f);
    auto prev = pr.removeFromLeft (28 * s), next = pr.removeFromRight (28 * s);
    g.setColour (Theme::dim()); g.setFont (Theme::font (16.0f * s));
    g.drawText (juce::CharPointer_UTF8 ("\xe2\x80\xb9"), prev, juce::Justification::centred); g.drawText (juce::CharPointer_UTF8 ("\xe2\x80\xba"), next, juce::Justification::centred);
    g.setColour (Theme::ink()); g.setFont (Theme::font (14.5f * s, false));
    g.drawText (ed.proc.presetDisplayName(), pr, juce::Justification::centred, true);
    hits.push_back ({ "prev", prev }); hits.push_back ({ "next", next }); hits.push_back ({ "preset", pr });
}

void DeVicentiEditor::TopBar::mouseDown (const juce::MouseEvent& e)
{
    for (const auto& [id, r] : hits)
    {
        if (! r.contains (e.position)) continue;
        auto& P = ed.proc;
        if (id == "back") ed.openPage (-1);
        else if (id == "prev") { P.setCurrentProgram (std::max (0, P.getCurrentProgram() - 1)); }
        else if (id == "next") { P.setCurrentProgram (std::min (P.getNumPrograms() - 1, P.getCurrentProgram() + 1)); }
        else if (id == "preset") ed.presetMenu();
        else if (id == "pages") ed.pagesMenu();
        else if (id == "midi") ed.openPage (ed.page == (int) Page::Midi ? -1 : (int) Page::Midi);
        else if (id == "settings") ed.openPage (ed.page == (int) Page::Settings ? -1 : (int) Page::Settings);
        else if (id == "save") ed.savePreset();
        else if (id == "manual") ed.toggleManual();
        else if (id == "learn") { ed.unit.learnMode = ! ed.unit.learnMode; if (ed.page >= 0) ed.openPage (-1); }
        repaint();
        return;
    }
}

void DeVicentiEditor::presetMenu()
{
    juce::PopupMenu m;
    const auto& ps = proc.getPresets();
    std::map<juce::String, juce::PopupMenu> cats; juce::StringArray order;
    for (int i = 0; i < (int) ps.size(); ++i)
    {
        const auto& c = ps[(size_t) i].category.isEmpty() ? juce::String ("User") : ps[(size_t) i].category;
        if (! order.contains (c)) order.add (c);
        cats[c].addItem (i + 1, ps[(size_t) i].name, true, i == proc.getCurrentProgram());
    }
    for (const auto& c : order) m.addSubMenu (c, cats[c]);
    m.addSeparator();
    m.addItem (10000, "Save as user preset...");
    m.addItem (10001, "Rescan user presets");
    juce::Component::SafePointer<DeVicentiEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&bar), [safe] (int r) {
        if (safe == nullptr || r == 0) return;
        if (r == 10000) safe->savePreset();
        else if (r == 10001) safe->proc.rescanUserPresets();
        else safe->proc.setCurrentProgram (r - 1);
        safe->bar.repaint();
    });
}

void DeVicentiEditor::pagesMenu()
{
    juce::PopupMenu m;
    for (int pg = 0; pg < (int) Page::kCount; ++pg) m.addItem (pg + 1, pageName ((Page) pg), true, page == pg);
    m.addSeparator(); m.addItem (100, "Panel", true, page < 0);
    juce::Component::SafePointer<DeVicentiEditor> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&bar), [safe] (int r) {
        if (safe == nullptr || r == 0) return;
        safe->openPage (r == 100 ? -1 : r - 1);
    });
}

void DeVicentiEditor::savePreset()
{
    auto* aw = new juce::AlertWindow ("Save preset", "Name of the user preset", juce::MessageBoxIconType::NoIcon, this);
    aw->addTextEditor ("name", proc.currentPresetName.get());
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey)); aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<DeVicentiEditor> safe (this);
    aw->enterModalState (true, juce::ModalCallbackFunction::create ([safe, aw] (int r) {
        if (safe != nullptr && r == 1) { const auto n = aw->getTextEditorContents ("name").trim(); if (n.isNotEmpty()) safe->proc.saveUserPreset (n); safe->bar.repaint(); }
    }), true);
}

// ───────────── snapshots for the self-test ─────────────
void DeVicentiEditor::setupSnapshot()
{
    if (const auto rep = juce::SystemStats::getEnvironmentVariable ("DV_UITEST", {}); rep.isNotEmpty())
    {
        juce::Component::SafePointer<DeVicentiEditor> safe (this);
        juce::Timer::callAfterDelay (900, [safe, rep] {
            if (safe == nullptr) return;
            UiTest t { safe->proc, safe->unit, safe->state };
            t.run (juce::File (rep));
            if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit();
        });
        return;
    }
    const auto snap = juce::SystemStats::getEnvironmentVariable ("DV_SNAPSHOT", {});
    if (snap.isEmpty()) return;
    auto env = [] (const char* k, const char* d) { return juce::SystemStats::getEnvironmentVariable (k, d); };
    juce::Component::SafePointer<DeVicentiEditor> safe (this);
    juce::Timer::callAfterDelay (600, [safe, env] {
        if (safe == nullptr) return;
        auto& e = *safe;
        if (const int pr = env ("DV_PRESET", "-1").getIntValue(); pr >= 0) e.proc.setCurrentProgram (pr);
        if (env ("DV_PLAY", "0").getIntValue()) e.proc.standalonePlay = true;
        if (const int t = env ("DV_TRACK", "0").getIntValue(); t > 0) e.unit.pressKey ("track" + juce::String (t));
        if (env ("DV_REC", "0").getIntValue()) e.unit.pressKey ("rec");
        if (env ("DV_FUNC", "0").getIntValue()) e.unit.pressKey ("func");
        if (const int h = env ("DV_HOLD", "0").getIntValue(); h > 0) e.unit.holdStep (h - 1);
        const auto pg = env ("DV_PAGE", "");
        for (int i = 0; i < (int) Page::kCount; ++i) if (pg.equalsIgnoreCase (pageName ((Page) i))) e.openPage (i);
        if (pg.equalsIgnoreCase ("MANUAL")) e.toggleManual();
    });
    juce::Timer::callAfterDelay (2600, [safe, snap, env] {
        if (safe == nullptr) return;
        const float sc = env ("DV_SNAP_SCALE", "2").getFloatValue();
        const auto img = safe->createComponentSnapshot (safe->getLocalBounds(), true, sc);
        juce::File f (snap); f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        os.flush();
        if (env ("DV_SNAP_QUIT", "0").getIntValue())
            if (auto* app = juce::JUCEApplicationBase::getInstance()) app->systemRequestedQuit();
    });
}
