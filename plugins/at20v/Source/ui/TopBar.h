#pragma once
// Barra superior: marca, navegador de presets y pestañas SYNTH / EXTENSIONS.

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace dh {

class BarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    BarLookAndFeel()
    {
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff17171a));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe6e2d6));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a3226));
        setColour (juce::PopupMenu::headerTextColourId, juce::Colour (0xffd8a85a));
        setColour (juce::TextButton::buttonColourId, juce::Colour (0xff17171a));
        setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xff3a3226));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xffbdb9ae));
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xffffd28a));
    }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (13.5f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (12.0f, juce::Font::bold); }
};

class TopBar : public juce::Component
{
public:
    std::function<void()> onManual;          // abre el manual (shared/manual)
    std::function<void (int)> onTab;

    explicit TopBar (AT20VProcessor& p) : proc (p)
    {
        setLookAndFeel (&lnf);
        for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &presetBtn, &tabSynth, &tabExt, &manualBtn }) addAndMakeVisible (b);
        prevBtn.onClick = [this] { step (-1); };
        nextBtn.onClick = [this] { step (1); };
        saveBtn.onClick = [this] { save(); };
        presetBtn.onClick = [this] { showPresetMenu(); };
        tabSynth.setClickingTogglesState (false); tabExt.setClickingTogglesState (false);
        tabSynth.onClick = [this] { setTab (0); };
        manualBtn.onClick = [this] { if (onManual) onManual(); };
        tabExt.onClick = [this] { setTab (1); };
        setTab (0);
        refresh();
    }
    ~TopBar() override { setLookAndFeel (nullptr); }

    void setTab (int t)
    {
        tabSynth.setToggleState (t == 0, juce::dontSendNotification);
        tabExt.setToggleState (t == 1, juce::dontSendNotification);
        if (onTab) onTab (t);
    }

    void refresh()
    {
        const auto& ps = proc.getPresets();
        const int i = proc.getCurrentProgram();
        juce::String cat = juce::isPositiveAndBelow (i, (int) ps.size()) && ps[(size_t) i].name == proc.currentPresetName ? ps[(size_t) i].category : juce::String();
        shownName = proc.presetDisplayName();
        presetBtn.setButtonText (cat.isNotEmpty() ? cat.toUpperCase() + "   " + shownName : shownName);
        // El nombre lleva " *" en cuanto algo cambia (un clic, la automatización): se mira 4 veces por segundo.
        if (! nameWatch.isTimerRunning()) { nameWatch.tick = [this] { if (proc.presetDisplayName() != shownName) refresh(); }; nameWatch.startTimerHz (4); }
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        juce::ColourGradient bg (juce::Colour (0xff1f1f22), 0, 0, juce::Colour (0xff121214), 0, (float) getHeight(), false);
        g.setGradientFill (bg); g.fillAll();
        g.setColour (juce::Colours::black); g.fillRect (0, getHeight() - 1, getWidth(), 1);
        g.setColour (juce::Colour (0xffeeebe2));
        g.setFont (juce::FontOptions (25.0f, juce::Font::bold).withHorizontalScale (0.84f));
        g.drawText ("AT20V", 18, 0, 90, getHeight(), juce::Justification::centredLeft);
        g.setColour (juce::Colour (0xff8d8a82));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText ("AUDIO TOOLS", 100, 0, 120, getHeight(), juce::Justification::centredLeft);
    }

    void resized() override
    {
        const int cy = getHeight() / 2;
        int x = getWidth() / 2 - 200;
        prevBtn.setBounds (x, cy - 13, 28, 26); x += 30;
        presetBtn.setBounds (x, cy - 13, 320, 26); x += 322;
        nextBtn.setBounds (x, cy - 13, 28, 26); x += 34;
        saveBtn.setBounds (x, cy - 13, 54, 26);
        manualBtn.setBounds (getWidth() - 18 - 82, cy - 13, 82, 26);
        tabExt.setBounds (manualBtn.getX() - 10 - 110, cy - 13, 110, 26);
        tabSynth.setBounds (tabExt.getX() - 4 - 80, cy - 13, 80, 26);
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
        m.addItem (10000, "Init (Normal Setting)");
        m.addItem (10001, "Show user presets folder");
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (presetBtn), [this] (int r) {
            if (r == 10000) { Preset p; p.name = "Init"; proc.loadPresetData (p); }
            else if (r == 10001) { AT20VProcessor::userPresetDir().createDirectory(); AT20VProcessor::userPresetDir().revealToUser(); }
            else if (r > 0) proc.loadPreset (r - 1);
        });
    }

    void save()
    {
        auto* w = new juce::AlertWindow ("Save preset", "Name for the preset:", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", proc.currentPresetName);
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        // La ventana es otra ventana del escritorio: puede seguir abierta después de cerrar el editor.
        w->enterModalState (true, juce::ModalCallbackFunction::create ([safe = juce::Component::SafePointer<juce::Component> (this), p = &proc, w] (int r) {
            if (safe == nullptr) return;
            if (r == 1 && w->getTextEditorContents ("name").trim().isNotEmpty())
                p->saveUserPreset (w->getTextEditorContents ("name").trim());
        }), true);
    }

    struct NameWatch : juce::Timer { std::function<void()> tick; void timerCallback() override { if (tick) tick(); } } nameWatch;

    juce::String shownName;

    AT20VProcessor& proc;
    BarLookAndFeel lnf;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, presetBtn, tabSynth { "SYNTH" }, tabExt { "EXTENSIONS" }, manualBtn { "MANUAL" };
};

} // namespace dh
