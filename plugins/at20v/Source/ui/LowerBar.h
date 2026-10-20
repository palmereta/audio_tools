#pragma once
// Barra inferior (como la del MS-20 V): voces, actividad MIDI, nivel de entrada, octava del
// teclado en pantalla y carga de CPU.

#include <JuceHeader.h>
#include "../PluginProcessor.h"

namespace dh {

class LowerBar : public juce::Component, private juce::Timer
{
public:
    std::function<void (int)> onOctave;

    explicit LowerBar (AT20VProcessor& p) : proc (p)
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
        const int cx = getWidth() / 2;
        octDown.setBounds (cx - 70, 4, 26, getHeight() - 8);
        octUp.setBounds (cx + 44, 4, 26, getHeight() - 8);
    }

    void paint (juce::Graphics& g) override
    {
        juce::ColourGradient bg (juce::Colour (0xff17171a), 0, 0, juce::Colour (0xff0f0f11), 0, (float) getHeight(), false);
        g.setGradientFill (bg); g.fillAll();
        g.setColour (juce::Colour (0xff2a2a2e)); g.fillRect (0, 0, getWidth(), 1);
        const juce::Font f (juce::FontOptions (12.0f));
        const juce::Font fb (juce::FontOptions (12.0f, juce::Font::bold));
        const int h = getHeight();
        auto label = [&] (int x, const juce::String& t, juce::Colour c, bool bold = false, int w = 200) {
            g.setColour (c); g.setFont (bold ? fb : f); g.drawText (t, x, 0, w, h, juce::Justification::centredLeft);
        };
        const auto dim = juce::Colour (0xff86837b), txt = juce::Colour (0xffe2ded3);

        // Voces.
        const auto* pv = proc.apvts.getRawParameterValue ("voices");
        const auto* pu = proc.apvts.getRawParameterValue ("unison");
        static const char* vn[] = { "Mono", "Poly 2", "Poly 4", "Poly 6", "Poly 8", "Poly 12" };
        juce::String voices = vn[juce::jlimit (0, 5, (int) pv->load())];
        if ((int) pu->load() > 0)
        {
            // Con unísono, las 12 voces se reparten: Poly 12 con Unison 4 toca 3 notas a la vez. Se dice cuántas.
            static const int vc[] = { 1, 2, 4, 6, 8, 12 };
            const int u = (int) pu->load() + 1, poly = vc[juce::jlimit (0, 5, (int) pv->load())];
            voices << juce::String::fromUTF8 ("  ·  Unison ") << u;
            if (poly > 1) voices << " (" << juce::jmax (1, juce::jmin (poly, 12 / u)) << " notes)";
        }
        label (14, "VOICES", dim, true);
        label (70, voices, txt, false, 220);

        // MIDI in/out.
        auto dot = [&] (int x, const char* t, bool on) {
            g.setColour (on ? juce::Colour (0xffffb347) : juce::Colour (0xff3a3a3e));
            g.fillEllipse ((float) x, h * 0.5f - 4.0f, 8.0f, 8.0f);
            label (x + 13, t, dim, true, 80);
        };
        dot (300, "MIDI IN", midiIn > 0);
        dot (380, "MIDI OUT", midiOut > 0);

        // Nivel de entrada (vocoder / ESP).
        label (480, "AUDIO IN", dim, true);
        const float lvl = juce::jlimit (0.0f, 1.0f, proc.inputLevel.load());
        const juce::Rectangle<float> meter (548.0f, h * 0.5f - 3.0f, 90.0f, 6.0f);
        g.setColour (juce::Colour (0xff2a2a2e)); g.fillRoundedRectangle (meter, 3.0f);
        g.setColour (lvl > 0.9f ? juce::Colour (0xffe0503a) : juce::Colour (0xffd8a85a));
        g.fillRoundedRectangle (meter.withWidth (meter.getWidth() * std::sqrt (lvl)), 3.0f);

        // Octava.
        g.setColour (txt); g.setFont (fb);
        g.drawText ("OCTAVE " + juce::String (octave > 0 ? "+" : "") + juce::String (octave), getWidth() / 2 - 42, 0, 84, h, juce::Justification::centred);

        // CPU y marca.
        label (getWidth() - 300, "CPU " + juce::String (proc.cpuLoad.load() * 100.0f, 1) + " %", dim, true);
        g.setColour (dim); g.setFont (f);
        g.drawText (juce::String::fromUTF8 ("AT20V 1.0  ·  Audio Tools"), getWidth() - 200, 0, 186, h, juce::Justification::centredRight);
    }

private:
    void timerCallback() override
    {
        if (proc.midiInFlash.exchange (false)) midiIn = 3; else if (midiIn > 0) --midiIn;
        if (proc.midiOutFlash.exchange (false)) midiOut = 3; else if (midiOut > 0) --midiOut;
        repaint();
    }

    AT20VProcessor& proc;
    juce::TextButton octDown { "-" }, octUp { "+" };
    int octave = 0, midiIn = 0, midiOut = 0;
};

} // namespace dh
