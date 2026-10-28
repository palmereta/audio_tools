#pragma once
// La página Simple: el átomo grande y, a la derecha, las seis macros (Macros.h), el encendido de cada capa y lo mínimo
// de la armonía. El átomo es el AtomPage de siempre como hijo: opaco y sin transformación propia, así que sus cuadros no
// repintan esta página (JUCE descuenta los hijos opacos del recorte) y fuera de esta página su temporizador baja a 2 Hz.

#include "AtomPage.h"
#include "Macros.h"
#include "Pages.h"

namespace dh::val::ui {

class SimplePage : public Page, private juce::Timer
{
public:
    explicit SimplePage (ValenceProcessor& p) : Page (p), atom (p)
    {
        addAndMakeVisible (atom);
        for (int k = 0; k < MacroCount; ++k) macros.push_back (make<MacroKnob> (p, k));
        startTimerHz (8);
        for (int l = 0; l < kLayers; ++l)
        {
            auto* t = make<PillToggle> (p, "l" + juce::String (l + 1) + "_on", "Layer " + juce::String (l + 1),
                                        "switch the layer on or off; its orbit dims in the atom. Every parameter of a layer is on the Layers page");
            t->tint = col::layer (l);
            layerT.push_back (t);
        }
        for (auto [id, lab, tip] : std::initializer_list<std::tuple<const char*, const char*, const char*>> {
                 { "source", "Harmony", "Played: the notes you hold. Auto: a chord walk plays by itself (press Play)" },
                 { "key", "Key", "for Complete, Auto harmony and fixed notes" }, { "scale", "Scale", "" },
                 { "complete", "Complete", "with one or two notes held, builds the chord of the scale on the lowest one" },
                 { "latch", "Latch", "keeps the chord after you let go (the sustain pedal also latches)" },
                 { "seed", "Variation", "the same number plays the same notes everywhere" } })
            boxes.push_back (make<ParamBox> (p, id, lab, tip));
    }
    void refresh() override { for (auto* m : macros) m->poll(); repaint(); }
    void paint (juce::Graphics& g) override
    {
        panel (g, side, "Shape");
        g.setColour (col::dim); g.setFont (font (13.0f, true));
        g.drawText ("Layers", layersTitle, juce::Justification::centredLeft);
        g.drawText ("Harmony", harmTitle, juce::Justification::centredLeft);
        g.setColour (col::faint); g.setFont (font (12.0f));
        g.drawFittedText ("Each macro moves the same setting of all four layers (Layers page) and keeps their differences.", noteR, juce::Justification::topLeft, 2);
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14);
        side = r.removeFromRight (372); r.removeFromRight (14);
        atom.setBounds (r);
        auto s = side.withTrimmedTop (32).reduced (12, 6);
        s.removeFromTop (6);
        gridLayout (s.removeFromTop (2 * 150 + 10), std::vector<juce::Component*> (macros.begin(), macros.end()), 3, 150, 10);
        noteR = s.removeFromTop (36);
        s.removeFromTop (8);
        layersTitle = s.removeFromTop (20); s.removeFromTop (4);
        gridLayout (s.removeFromTop (2 * 34 + 8), { layerT.begin(), layerT.end() }, 2, 34, 8);
        s.removeFromTop (16);
        harmTitle = s.removeFromTop (20); s.removeFromTop (4);
        gridLayout (s.removeFromTop (2 * 44 + 10), { boxes.begin(), boxes.end() }, 3, 44, 10);
    }
    AtomPage atom;
private:
    std::vector<MacroKnob*> macros;
    std::vector<juce::Component*> layerT, boxes;
    void timerCallback() override { if (onScreen()) for (auto* m : macros) m->poll(); }
    juce::Rectangle<int> side, layersTitle, harmTitle, noteR;
};

} // namespace dh::val::ui
