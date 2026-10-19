#pragma once
// Barras del editor: arriba marca, presets (anterior/siguiente/menú/guardar/al azar) y pestañas;
// abajo el estado del teclado táctil, MIDI, reloj, cartuchos y CPU.

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Assets42.h"

namespace dh {

class BarLookAndFeel42 : public juce::LookAndFeel_V4
{
public:
    BarLookAndFeel42()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff17171a));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe6e2d6));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a2420));
        setColour (juce::PopupMenu::headerTextColourId, juce::Colour (0xffff7a52));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff17171a));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3a2420));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffbdb9ae));
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xffffb49a));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff2a2a2e));
    }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (13.5f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (12.0f, juce::Font::bold); }
};

class TopBar42 : public juce::Component
{
public:
    std::function<void()> onManual;          // abre el manual (shared/manual)
    std::function<void (int)> onTab;

    explicit TopBar42 (AT42Processor& p) : proc (p)
    {
        setLookAndFeel (&lnf);
        for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &rndBtn, &presetBtn, &tabSynth, &tabExt, &manualBtn }) addAndMakeVisible (b);
        prevBtn.onClick = [this] { step (-1); };
        nextBtn.onClick = [this] { step (1); };
        saveBtn.onClick = [this] { save(); };
        rndBtn.onClick = [this] { proc.randomizePanel(); };
        rndBtn.setTooltip ("Random patch: new drone chords, characters and effects (keeps cables and mixer)");
        presetBtn.onClick = [this] { showPresetMenu(); };
        tabSynth.onClick = [this] { setTab (0); };
        manualBtn.onClick = [this] { if (onManual) onManual(); };
        tabExt.onClick = [this] { setTab (1); };
        setTab (0);
        refresh();
    }
    ~TopBar42() override { setLookAndFeel (nullptr); }

    void setTab (int t)
    {
        tab = t;
        tabSynth.setToggleState (t == 0, juce::dontSendNotification);
        tabExt.setToggleState (t == 1, juce::dontSendNotification);
        if (onTab) onTab (t);
    }
    int getTab() const { return tab; }

    void refresh()
    {
        const auto& ps = proc.getPresets();
        const int i = proc.getCurrentProgram();
        const juce::String cat = juce::isPositiveAndBelow (i, (int) ps.size()) && ps[(size_t) i].name == proc.currentPresetName ? ps[(size_t) i].category : juce::String();
        presetBtn.setButtonText (cat.isNotEmpty() ? cat.toUpperCase() + "   " + proc.currentPresetName : proc.currentPresetName);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        juce::ColourGradient bg (juce::Colour (0xff1f1f22), 0, 0, juce::Colour (0xff121214), 0, (float) getHeight(), false);
        g.setGradientFill (bg); g.fillAll();
        g.setColour (juce::Colours::black); g.fillRect (0, getHeight() - 1, getWidth(), 1);
        g.setColour (juce::Colour (0xffeeebe2));
        g.setFont (juce::FontOptions ("Helvetica Neue", 26.0f, juce::Font::bold).withHorizontalScale (0.8f));
        g.drawText ("AT42", 18, 0, 100, getHeight(), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff8d8a82));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText ("AUDIO TOOLS", 88, 0, 120, getHeight(), juce::Justification::centredLeft);
    }

    void resized() override
    {
        const int cy = getHeight() / 2;
        int x = getWidth() / 2 - 250;
        prevBtn.setBounds (x, cy - 13, 28, 26); x += 30;
        presetBtn.setBounds (x, cy - 13, 330, 26); x += 332;
        nextBtn.setBounds (x, cy - 13, 28, 26); x += 34;
        saveBtn.setBounds (x, cy - 13, 54, 26); x += 58;
        rndBtn.setBounds (x, cy - 13, 74, 26);
        manualBtn.setBounds (getWidth() - 18 - 82, cy - 13, 82, 26);
        tabExt.setBounds (manualBtn.getX() - 10 - 96, cy - 13, 96, 26);
        tabSynth.setBounds (tabExt.getX() - 4 - 72, cy - 13, 72, 26);
    }

private:
    void step (int d)
    {
        const int n = (int) proc.getPresets().size();
        if (n > 0) proc.loadPreset (((proc.getCurrentProgram() + d) % n + n) % n);
    }

    void showPresetMenu()
    {
        juce::PopupMenu m;
        std::map<juce::String, juce::PopupMenu> cats;
        juce::StringArray order;
        const auto& ps = proc.getPresets();
        for (int i = 0; i < (int) ps.size(); ++i)
        {
            const auto cat = ps[(size_t) i].category.isEmpty() ? juce::String ("Other") : ps[(size_t) i].category;
            if (! order.contains (cat)) order.add (cat);
            cats[cat].addItem (i + 1, ps[(size_t) i].name, true, i == proc.getCurrentProgram());
        }
        for (auto& c : order) m.addSubMenu (c, cats[c]);
        m.addSeparator();
        m.addItem (10000, "Init");
        m.addItem (10001, "Show user presets folder");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn), [this] (int r) {
            if (r == 10000) { Preset42 p; proc.loadPresetData (p); }
            else if (r == 10001) { AT42Processor::userPresetDir().createDirectory(); AT42Processor::userPresetDir().revealToUser(); }
            else if (r > 0) proc.loadPreset (r - 1);
        });
    }

    void save()
    {
        auto* w = new juce::AlertWindow ("Save preset", "The preset keeps the panel, the cables, the keyboard sequence and the settings.", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", proc.currentPresetName);
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r) {
            if (r == 1 && w->getTextEditorContents ("name").trim().isNotEmpty())
                proc.saveUserPreset (w->getTextEditorContents ("name").trim());
        }), true);
    }

    AT42Processor& proc;
    BarLookAndFeel42 lnf;
    int tab = 0;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, rndBtn { "RANDOM" }, presetBtn,
                     tabSynth { "SYNTH" }, tabExt { "SETTINGS" }, manualBtn { "MANUAL" };
};

class LowerBar42 : public juce::Component, private juce::Timer
{
public:
    explicit LowerBar42 (AT42Processor& p) : proc (p) { startTimerHz (12); }

    void paint (juce::Graphics& g) override
    {
        juce::ColourGradient bg (juce::Colour (0xff17171a), 0, 0, juce::Colour (0xff0f0f11), 0, (float) getHeight(), false);
        g.setGradientFill (bg); g.fillAll();
        g.setColour (juce::Colour (0xff2a2a2e)); g.fillRect (0, 0, getWidth(), 1);
        const juce::Font f (juce::FontOptions (12.0f)), fb (juce::FontOptions (12.0f, juce::Font::bold));
        const int h = getHeight();
        auto label = [&] (int x, const juce::String& t, juce::Colour c, bool bold = false, int w = 220) {
            g.setColour (c); g.setFont (bold ? fb : f); g.drawText (t, x, 0, w, h, juce::Justification::centredLeft);
        };
        const auto dim = juce::Colour (0xff86837b), txt = juce::Colour (0xffe2ded3);
        auto ext = [&] (const char* id) { const auto* e = findExt42 (id); return formatExt42 (*e, proc.extValue (id)); };
        label (14, "KEYS", dim, true);
        label (56, ext ("kb_mode") + "  " + ext ("kb_scale") + " " + ext ("kb_root"), txt, false, 300);
        g.setColour (midiIn > 0 ? juce::Colour (0xffff5a3c) : juce::Colour (0xff3a3a3e));
        g.fillEllipse (360.0f, h * 0.5f - 4.0f, 8.0f, 8.0f);
        label (373, "MIDI IN", dim, true, 80);
        label (470, "CARTRIDGES", dim, true);
        const auto carts = panelChoices42 ((int) P42::fx_cart_l);
        label (560, carts[(int) proc.value (P42::fx_cart_l)] + " / " + carts[(int) proc.value (P42::fx_cart_r)], txt, false, 260);
        label (getWidth() - 300, "CPU " + juce::String (proc.cpuLoad.load() * 100.0f, 1) + " %", dim, true);
        g.setColour (dim); g.setFont (f);
        g.drawText ("AT42 " + juce::String (JucePlugin_VersionString) + "  Audio Tools", getWidth() - 200, 0, 186, h, juce::Justification::centredRight);
    }

private:
    void timerCallback() override
    {
        if (proc.midiInFlash.exchange (false)) midiIn = 3; else if (midiIn > 0) --midiIn;
        repaint();
    }
    AT42Processor& proc;
    int midiIn = 0;
};

} // namespace dh
