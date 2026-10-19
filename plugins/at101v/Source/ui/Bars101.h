#pragma once
// Barras del editor: arriba marca, presets (anterior/siguiente/menú/guardar/al azar) y pestañas;
// abajo voces, MIDI, reloj, transposición, paso de la secuencia, octava del teclado y CPU.

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Assets101.h"

namespace dh {

class BarLookAndFeel101 : public juce::LookAndFeel_V4
{
public:
    BarLookAndFeel101()
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

class TopBar101 : public juce::Component
{
public:
    std::function<void()> onManual;          // abre el manual (shared/manual)
    std::function<void (int)> onTab;

    explicit TopBar101 (AT101VProcessor& p) : proc (p)
    {
        setLookAndFeel (&lnf);
        for (auto* b : { &prevBtn, &nextBtn, &saveBtn, &rndBtn, &presetBtn, &tabSynth, &tabSeq, &tabExt, &manualBtn }) addAndMakeVisible (b);
        prevBtn.onClick = [this] { step (-1); };
        nextBtn.onClick = [this] { step (1); };
        saveBtn.onClick = [this] { save(); };
        rndBtn.onClick = [this] { proc.randomizePanel(); };
        rndBtn.setTooltip ("Random patch (keeps the sequence and the settings)");
        presetBtn.onClick = [this] { showPresetMenu(); };
        tabSynth.onClick = [this] { setTab (0); };
        tabSeq.onClick = [this] { setTab (1); };
        manualBtn.onClick = [this] { if (onManual) onManual(); };
        tabExt.onClick = [this] { setTab (2); };
        setTab (0);
        refresh();
    }
    ~TopBar101() override { setLookAndFeel (nullptr); }

    void setTab (int t)
    {
        tab = t;
        tabSynth.setToggleState (t == 0, juce::dontSendNotification);
        tabSeq.setToggleState (t == 1, juce::dontSendNotification);
        tabExt.setToggleState (t == 2, juce::dontSendNotification);
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
        // "AT101V" en puntos, como el logo del panel.
        drawDotLogo (g, 18.0f, (float) getHeight() * 0.5f - 9.0f, 18.0f);
        g.setColour (juce::Colour (0xff8d8a82));
        g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
        g.drawText ("AUDIO TOOLS", 128, 0, 120, getHeight(), juce::Justification::centredLeft);
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
        tabSeq.setBounds (tabExt.getX() - 4 - 104, cy - 13, 104, 26);
        tabSynth.setBounds (tabSeq.getX() - 4 - 72, cy - 13, 72, 26);
    }

private:
    static void drawDotLogo (juce::Graphics& g, float x, float y, float h)
    {
        // Fuente de 5×7 puntos (sólo los caracteres del nombre).
        static const std::map<char, std::array<uint8_t, 7>> font = {
            { 'A', { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 } }, { 'T', { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
            { '1', { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e } }, { '0', { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e } },
            { 'V', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04 } } };
        const float d = h / 7.0f;
        g.setColour (juce::Colour (0xffeeebe2));
        for (char ch : juce::String ("AT101V").toStdString())
        {
            const auto& rows = font.at (ch);
            for (int r = 0; r < 7; ++r)
                for (int c = 0; c < 5; ++c)
                    if (rows[(size_t) r] & (0x10 >> c)) g.fillRect (x + c * d, y + r * d, d * 0.78f, d * 0.78f);
            x += 6.0f * d;
        }
    }

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
            if (r == 10000) { Preset p; proc.loadPresetData (p); }
            else if (r == 10001) { AT101VProcessor::userPresetDir().createDirectory(); AT101VProcessor::userPresetDir().revealToUser(); }
            else if (r > 0) proc.loadPreset (r - 1);
        });
    }

    void save()
    {
        auto* w = new juce::AlertWindow ("Save preset", "The preset keeps the panel, the sequence and the settings.", juce::MessageBoxIconType::NoIcon);
        w->addTextEditor ("name", proc.currentPresetName);
        w->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        w->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        w->enterModalState (true, juce::ModalCallbackFunction::create ([this, w] (int r) {
            if (r == 1 && w->getTextEditorContents ("name").trim().isNotEmpty())
                proc.saveUserPreset (w->getTextEditorContents ("name").trim());
        }), true);
    }

    AT101VProcessor& proc;
    BarLookAndFeel101 lnf;
    int tab = 0;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, saveBtn { "SAVE" }, rndBtn { "RANDOM" }, presetBtn,
                     tabSynth { "SYNTH" }, tabSeq { "SEQUENCER" }, tabExt { "SETTINGS" }, manualBtn { "MANUAL" };
};

class LowerBar101 : public juce::Component, private juce::Timer
{
public:
    std::function<void (int)> onOctave;

    explicit LowerBar101 (AT101VProcessor& p) : proc (p)
    {
        for (auto* b : { &octDown, &octUp })
        {
            addAndMakeVisible (b);
            b->setColour (juce::TextButton::buttonColourId, juce::Colour (0xff1b1b1e));
            b->setColour (juce::TextButton::textColourOffId, juce::Colour (0xffd9d5ca));
        }
        octDown.onClick = [this] { if (onOctave) onOctave (-1); };
        octUp.onClick = [this] { if (onOctave) onOctave (1); };
        startTimerHz (15);
    }

    void setOctaveText (int o) { octave = o; repaint(); }

    void resized() override
    {
        const int cx = getWidth() / 2 + 120;
        octDown.setBounds (cx - 70, 4, 26, getHeight() - 8);
        octUp.setBounds (cx + 44, 4, 26, getHeight() - 8);
    }

    void paint (juce::Graphics& g) override
    {
        juce::ColourGradient bg (juce::Colour (0xff17171a), 0, 0, juce::Colour (0xff0f0f11), 0, (float) getHeight(), false);
        g.setGradientFill (bg); g.fillAll();
        g.setColour (juce::Colour (0xff2a2a2e)); g.fillRect (0, 0, getWidth(), 1);
        const juce::Font f (juce::FontOptions (12.0f)), fb (juce::FontOptions (12.0f, juce::Font::bold));
        const int h = getHeight();
        auto label = [&] (int x, const juce::String& t, juce::Colour c, bool bold = false, int w = 200) {
            g.setColour (c); g.setFont (bold ? fb : f); g.drawText (t, x, 0, w, h, juce::Justification::centredLeft);
        };
        const auto dim = juce::Colour (0xff86837b), txt = juce::Colour (0xffe2ded3);

        static const char* vn[] = { "Mono", "Poly 2", "Poly 4", "Poly 6", "Poly 8" };
        const int vi = juce::jlimit (0, 4, (int) proc.apvts.getRawParameterValue ("voices")->load());
        const int un = (int) proc.apvts.getRawParameterValue ("unison")->load();
        juce::String voices = vn[vi];
        if (un > 0) voices << "  Unison " << (un + 1);
        label (14, "VOICES", dim, true);
        label (70, voices, txt, false, 150);

        auto dot = [&] (int x, const char* t, bool on) {
            g.setColour (on ? juce::Colour (0xffff5a3c) : juce::Colour (0xff3a3a3e));
            g.fillEllipse ((float) x, h * 0.5f - 4.0f, 8.0f, 8.0f);
            label (x + 13, t, dim, true, 80);
        };
        dot (220, "MIDI IN", midiIn > 0);
        dot (300, "MIDI OUT", midiOut > 0);

        const bool host = proc.apvts.getRawParameterValue ("clock_src")->load() > 0.5f;
        label (400, "CLOCK", dim, true);
        label (452, host ? "Host tempo" : "Modulator", txt, false, 100);

        const int kt = proc.keyTranspose.load();
        label (560, "KEY TR", dim, true);
        label (614, proc.keyTrArmed.load() ? juce::String ("press a key") : (kt > 0 ? "+" : "") + juce::String (kt), txt, false, 90);

        g.setColour (txt); g.setFont (fb);
        g.drawText ("OCTAVE " + juce::String (octave > 0 ? "+" : "") + juce::String (octave), getWidth() / 2 + 120 - 42, 0, 84, h, juce::Justification::centred);

        const auto seq = proc.panelValue (P::seq_play) > 0.5f;
        if (seq || proc.loadingLed.load())
            label (getWidth() / 2 + 230, proc.loadingLed.load() ? juce::String ("LOADING ") + juce::String (proc.getSequence().length) + " / 100"
                                                               : juce::String ("STEP ") + juce::String (proc.stepPos.load() + 1),
                   juce::Colour (0xffff7a52), true, 160);

        label (getWidth() - 300, "CPU " + juce::String (proc.cpuLoad.load() * 100.0f, 1) + " %", dim, true);
        g.setColour (dim); g.setFont (f);
        g.drawText ("AT101V " + juce::String (JucePlugin_VersionString) + "  Audio Tools", getWidth() - 200, 0, 186, h, juce::Justification::centredRight);
    }

private:
    void timerCallback() override
    {
        if (proc.midiInFlash.exchange (false)) midiIn = 3; else if (midiIn > 0) --midiIn;
        if (proc.midiOutFlash.exchange (false)) midiOut = 3; else if (midiOut > 0) --midiOut;
        repaint();
    }

    AT101VProcessor& proc;
    juce::TextButton octDown { "-" }, octUp { "+" };
    int octave = 0, midiIn = 0, midiOut = 0;
};

} // namespace dh
