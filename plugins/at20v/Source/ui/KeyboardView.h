#pragma once
// Teclado renderizado en Blender (estilo MS-20 / MS-20 V): 37 teclas C–C, rueda de control y
// pulsador momentáneo. Las teclas apretadas se dibujan recortando la imagen "todas abajo" con el
// polígono de cada tecla; las negras se redibujan encima para que tapen bien a las blancas.

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Assets.h"

namespace dh {

class KeyboardView : public juce::Component, private juce::Timer
{
public:
    explicit KeyboardView (AT20VProcessor& p) : proc (p)
    {
        const auto& A = Assets::get();
        setSize (A.keysUp.isValid() ? A.keysUp.getWidth() / 2 : 1692, A.keysUp.isValid() ? A.keysUp.getHeight() / 2 : 260);
        startTimerHz (40);
    }

    int baseNote() const { return 36 + 12 * octave; }
    int getOctave() const { return octave; }
    void setOctave (int o) { releaseMouseNote(); octave = juce::jlimit (-2, 2, o); repaint(); }

    void paint (juce::Graphics& g) override
    {
        const auto& A = Assets::get();
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        const auto full = getLocalBounds().toFloat();
        if (! A.keysUp.isValid()) { g.fillAll (juce::Colours::black); return; }
        g.drawImage (A.keysUp, full);
        const auto toView = juce::AffineTransform::scale (0.5f);
        auto drawKey = [&] (const Assets::KeyShape& k, const juce::Image& img, bool pressed) {
            juce::Graphics::ScopedSaveState ss (g);
            g.reduceClipRegion (k.hull, toView);
            g.setOpacity (1.0f);
            g.drawImage (img, full);
            if (pressed)
            {
                // La tecla hundida recibe menos luz, sobre todo hacia el frente.
                const auto b = k.hull.getBounds().transformedBy (toView);
                juce::ColourGradient sh (juce::Colours::black.withAlpha (k.black ? 0.10f : 0.06f), b.getX(), b.getY(),
                                         juce::Colours::black.withAlpha (k.black ? 0.35f : 0.24f), b.getX(), b.getBottom(), false);
                g.setGradientFill (sh);
                g.fillRect (b);
            }
        };
        for (const auto& k : A.keyShapes)
            if (! k.black && isDown (k.note)) drawKey (k, A.keysDown, true);
        for (const auto& k : A.keyShapes)
            if (k.black) drawKey (k, isDown (k.note) ? A.keysDown : A.keysUp, isDown (k.note));

        // Rueda: el cuadro según la posición (−1..+1).
        if (A.wheelSheet.isValid() && ! A.wheelCrop.isEmpty())
        {
            const float w = proc.wheel.load();
            const int f = juce::jlimit (0, A.wheelFrames - 1, (int) std::lround ((w + 1.0f) * 0.5f * (A.wheelFrames - 1)));
            const auto dst = A.wheelCrop.toFloat() * 0.5f;
            g.setOpacity (1.0f);
            g.drawImage (A.wheelSheet, dst.getX(), dst.getY(), dst.getWidth(), dst.getHeight(),
                         (f % A.wheelCols) * A.wheelFw, (f / A.wheelCols) * A.wheelFh, A.wheelFw, A.wheelFh);
        }
        if (proc.momentary.load() && A.switchDown.isValid() && ! A.switchCrop.isEmpty())
        {
            const auto dst = A.switchCrop.toFloat() * 0.5f;
            g.drawImage (A.switchDown, dst);
        }
        // Rango tocable (la octava se mueve desde la barra inferior).
        if (octave != 0)
        {
            g.setColour (juce::Colour (0xffd8a85a));
            g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
            g.drawText ("OCT " + juce::String (octave > 0 ? "+" : "") + juce::String (octave), getWidth() - 110, 8, 90, 16, juce::Justification::centredRight);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto& A = Assets::get();
        const auto p2 = e.position * 2.0f;
        if (! A.switchCrop.isEmpty() && A.switchCrop.toFloat().expanded (6.0f).contains (p2)) { proc.momentary.store (true); repaint(); return; }
        if (! A.wheelCrop.isEmpty() && A.wheelCrop.toFloat().contains (p2)) { draggingWheel = true; lastY = e.position.y; return; }
        playAt (e.position);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (draggingWheel)
        {
            float v = proc.wheel.load() + (lastY - e.position.y) / 70.0f;
            if (std::abs (v) < 0.04f) v = 0.0f;          // clic central
            proc.wheel.store (juce::jlimit (-1.0f, 1.0f, v));
            lastY = e.position.y;
            repaint();
            return;
        }
        if (! proc.momentary.load()) playAt (e.position);   // glissando
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        draggingWheel = false;
        if (proc.momentary.load()) { proc.momentary.store (false); repaint(); }
        releaseMouseNote();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const auto& A = Assets::get();
        if (! A.wheelCrop.isEmpty() && A.wheelCrop.toFloat().contains (e.position * 2.0f)) { proc.wheel.store (0.0f); repaint(); }
    }

private:
    bool isDown (int keyIndex) const
    {
        const int n = baseNote() + keyIndex;
        return n >= 0 && n < 128 && proc.keyboardState.isNoteOnForChannels (0xffff, n);
    }

    int keyAt (juce::Point<float> p, float& velocity) const
    {
        const auto& A = Assets::get();
        const auto p2 = p * 2.0f;
        for (int pass = 0; pass < 2; ++pass)
            for (const auto& k : A.keyShapes)
                if (k.black == (pass == 0) && k.hull.contains (p2))
                {
                    const auto b = k.hull.getBounds();
                    velocity = juce::jlimit (0.2f, 1.0f, 0.25f + 0.85f * (p2.y - b.getY()) / b.getHeight());
                    return k.note;
                }
        return -1;
    }

    void playAt (juce::Point<float> p)
    {
        float vel = 0.8f;
        const int k = keyAt (p, vel);
        if (k == mouseKey) return;
        releaseMouseNote();
        if (k < 0) return;
        mouseKey = k;
        mouseNote = juce::jlimit (0, 127, baseNote() + k);
        proc.keyboardState.noteOn (1, mouseNote, vel);
        repaint();
    }

    void releaseMouseNote()
    {
        if (mouseNote >= 0) proc.keyboardState.noteOff (1, mouseNote, 0.0f);
        mouseNote = mouseKey = -1;
        repaint();
    }

    void timerCallback() override
    {
        // Redibuja cuando cambia algo (teclas por MIDI externo, rueda por pitch bend, pulsador por sustain).
        uint64_t h = 0;
        for (int k = 0; k < 37; ++k) if (isDown (k)) h |= (uint64_t) 1 << k;
        const float w = proc.wheel.load(); const bool m = proc.momentary.load();
        if (h != shownKeys || w != shownWheel || m != shownMom) { shownKeys = h; shownWheel = w; shownMom = m; repaint(); }
    }

    AT20VProcessor& proc;
    int octave = 0, mouseNote = -1, mouseKey = -1;
    bool draggingWheel = false, shownMom = false;
    float lastY = 0.0f, shownWheel = 9.0f;
    uint64_t shownKeys = 0;
};

} // namespace dh
