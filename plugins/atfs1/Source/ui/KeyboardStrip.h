#pragma once
// Teclado de cinco octavas al pie del editor: para probar sonidos sin controlador (velocidad según la
// altura del clic) y para ver lo que llega por MIDI.

#include "../../../../shared/ui/Showing.h"
#include "Widgets.h"

namespace dh::fs::ui {

class KeyboardStrip : public juce::Component, private juce::Timer
{
public:
    explicit KeyboardStrip (ATFS1Processor& p) : proc (p) { startTimerHz (30); }
    int lowNote = 36, octaves = 5;
    void paint (juce::Graphics& g) override
    {
        g.fillAll (col::bg);
        const auto r = getLocalBounds().toFloat().reduced (12, 4);
        const int whites = octaves * 7 + 1;
        const float ww = r.getWidth() / (float) whites;
        for (int i = 0, n = lowNote; i < whites; ++n)
        {
            if (isBlack (n)) continue;
            const auto k = juce::Rectangle<float> (r.getX() + (float) i * ww, r.getY(), ww - 1.0f, r.getHeight());
            const bool on = proc.live.keyDown[(size_t) n].load();
            g.setColour (on ? col::green : juce::Colour (0xffd9dcd6)); g.fillRoundedRectangle (k, 2.0f);
            if (n % 12 == 0) { g.setColour (col::faint); g.setFont (font (9.5f)); g.drawText ("C" + juce::String (n / 12 - 2), k.withTrimmedTop (k.getHeight() - 14).toNearestInt(), juce::Justification::centred); }
            ++i;
        }
        for (int i = 0, n = lowNote; i < whites; ++n)
        {
            if (! isBlack (n)) { ++i; continue; }
            const auto k = juce::Rectangle<float> (r.getX() + (float) i * ww - ww * 0.32f, r.getY(), ww * 0.62f, r.getHeight() * 0.6f);
            const bool on = proc.live.keyDown[(size_t) n].load();
            g.setColour (on ? col::green.darker (0.2f) : juce::Colour (0xff1b1f24)); g.fillRoundedRectangle (k, 2.0f);
        }
    }
    void mouseDown (const juce::MouseEvent& e) override { play (e); }
    void mouseDrag (const juce::MouseEvent& e) override { const int n = noteAt (e.position); if (n != held) { release(); play (e); } }
    void mouseUp (const juce::MouseEvent&) override { release(); }
    void mouseEnter (const juce::MouseEvent&) override { showHelp ("Keyboard: click lower on a key for a louder note"); }
    void mouseExit (const juce::MouseEvent&) override { showHelp ({}); }
private:
    ATFS1Processor& proc;
    int held = -1;
    std::array<bool, 128> shown {};
    static bool isBlack (int n) { const int m = n % 12; return m == 1 || m == 3 || m == 6 || m == 8 || m == 10; }
    int noteAt (juce::Point<float> p) const
    {
        const auto r = getLocalBounds().toFloat().reduced (12, 4);
        const int whites = octaves * 7 + 1;
        const float ww = r.getWidth() / (float) whites;
        if (p.y < r.getY() + r.getHeight() * 0.6f)
            for (int i = 0, n = lowNote; i < whites; ++n)
            {
                if (! isBlack (n)) { ++i; continue; }
                if (juce::Rectangle<float> (r.getX() + (float) i * ww - ww * 0.32f, r.getY(), ww * 0.62f, r.getHeight() * 0.6f).contains (p)) return n;
            }
        int idx = juce::jlimit (0, whites - 1, (int) ((p.x - r.getX()) / ww));
        for (int i = 0, n = lowNote; ; ++n) { if (isBlack (n)) continue; if (i == idx) return n; ++i; }
    }
    void play (const juce::MouseEvent& e)
    {
        held = noteAt (e.position);
        const float y = juce::jlimit (0.0f, 1.0f, (e.position.y - 4.0f) / (float) std::max (1, getHeight() - 8));
        proc.sendCommand (ATFS1Processor::CmdNoteOn, held, juce::jlimit (20, 127, (int) (40 + 87 * y)));
    }
    void release() { if (held >= 0) proc.sendCommand (ATFS1Processor::CmdNoteOff, held); held = -1; }
    bool hiddenNow = false;
    void timerCallback() override
    {
        if (! dh::reallyShowing (*this)) { hiddenNow = true; return; }   // ventana escondida (Live): nada que hacer (H3)
        if (hiddenNow) { hiddenNow = false; repaint(); }                  // vuelve a la vista con lo que cambió mientras tanto
        bool ch = false;
        for (int n = 0; n < 128; ++n) { const bool k = proc.live.keyDown[(size_t) n].load(); if (k != shown[(size_t) n]) { shown[(size_t) n] = k; ch = true; } }
        if (ch) repaint();
    }
};

} // namespace dh::fs::ui
