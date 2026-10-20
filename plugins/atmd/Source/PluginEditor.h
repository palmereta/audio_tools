#pragma once
// Editor del ATMD: barra superior (presets, Save, PANEL / SETTINGS, MANUAL), el instrumento renderizado
// (ui/PanelViewMd.h) con la página de extensiones encima del panel (ui/SettingsViewMd.h) y la barra de ayuda.

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/PanelViewMd.h"
#include "ui/SettingsViewMd.h"
#include "ManualView.h"

namespace dh::md::ui {
namespace col {
    const juce::Colour bar (0xff16120e), barLine (0xff3a2f24), text (0xffeee6d6), dim (0xffa89c88), faint (0xff6f6556);
    const juce::Colour orange (0xffe0632a), blue (0xff3d8fd1), button (0xff2a231c), buttonHi (0xff3a3027);
}
inline juce::Font barFont (float h, bool bold = false) { return juce::Font (juce::FontOptions ("Futura", h, bold ? juce::Font::bold : juce::Font::plain)); }

class Look : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        const bool on = b.getToggleState();
        g.setColour (on ? col::orange : (down ? col::buttonHi : (over ? col::buttonHi : col::button)));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (on ? col::orange.brighter (0.2f) : col::barLine);
        g.drawRoundedRectangle (r, 4.0f, 1.0f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setColour (b.getToggleState() ? juce::Colour (0xff1a1209) : col::text);
        g.setFont (barFont (14.0f, true));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, 1, 0.8f);
    }
};

class Content : public juce::Component
{
public:
    std::function<void (juce::Graphics&)> painter;
    void paint (juce::Graphics& g) override { if (painter) painter (g); }
};
} // namespace dh::md::ui

class AtmdEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit AtmdEditor (AtmdProcessor&);
    ~AtmdEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& k) override;
    bool keyStateChanged (bool isKeyDown) override;

    static constexpr int baseW = 1502, panelH = 894, barH = 46, footH = 28, baseH = barH + panelH + footH;

private:
    AtmdProcessor& proc;
    dh::md::ui::Look lnf;
    dh::md::ui::Content content;
    dh::md::ui::PanelView panel;
    dh::md::ui::SettingsView settings;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, presetBtn { "Init" }, saveBtn { "SAVE" },
                     panelBtn { "PANEL" }, settingsBtn { "SETTINGS" }, manualBtn { "MANUAL" };
    std::unique_ptr<juce::Component> manual;
    juce::String help;
    juce::Rectangle<int> statusR;
    int page = 0;
    std::map<int, int> qwertyDown;          // tecla → la nota que disparó (para soltar sólo esa)
    int octave = 4;                 // octava del teclado de la computadora (Z / X)

    void showPage (int p);
    void showManual (bool on);
    void showPresetMenu();
    void savePreset();
    void paintBackdrop (juce::Graphics& g);
    void timerCallback() override;
    void setHelp (const juce::String& s) { help = s; content.repaint (statusR); }
};
