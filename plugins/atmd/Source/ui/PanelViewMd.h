#pragma once
// El instrumento: el fondo renderizado y, encima, cada control en su estado (cuadro de la perilla, rocker
// apretado de un lado o del otro, rueda, lámpara) y las teclas apretadas. Todo se dibuja en píxeles del fondo
// con una sola transformación de escala; sólo se repinta lo que cambió.
//
// Mouse: perillas con arrastre vertical (Shift = fino, doble clic = valor por defecto), selectores RANGE y
// WAVEFORM con clic o arrastre por posiciones, rockers con clic, ruedas con arrastre (la de pitch vuelve al
// centro al soltarla), teclas con clic (más abajo en la tecla = más fuerte).

#include "../PluginProcessor.h"
#include "AssetsMd.h"

namespace dh::md::ui {

class PanelView : public juce::Component, private juce::Timer, private juce::MidiKeyboardState::Listener
{
public:
    std::function<void (const juce::String&)> onHelp;

    explicit PanelView (AtmdProcessor& p) : proc (p)
    {
        setOpaque (true);
        setWantsKeyboardFocus (false);
        proc.keyboardState.addListener (this);
        for (auto& c : assets().controls) last[c.id] = -1e9f;
        startTimerHz (30);
    }
    ~PanelView() override { proc.keyboardState.removeListener (this); }

    float scale() const { return (float) getWidth() / (float) assets().width; }

    void paint (juce::Graphics& g) override
    {
        const auto& A = assets();
        g.fillAll (juce::Colour (0xff0b0a09));
        if (! A.ok) { g.setColour (juce::Colours::white); g.drawText ("ATMD: missing panel images", getLocalBounds(), juce::Justification::centred); return; }
        g.addTransform (juce::AffineTransform::scale (scale()));
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImageAt (A.background, 0, 0);
        // Teclas apretadas: la imagen de "todas abajo" recortada por el contorno de cada tecla.
        for (auto& k : A.keys)
            if (keyDown (k.note))
            {
                juce::Graphics::ScopedSaveState s (g);
                g.reduceClipRegion (k.hull);
                g.drawImageAt (A.keysDown, (int) A.keysCrop.getX(), (int) A.keysCrop.getY());
                // Desde este ángulo una tecla que baja casi no cambia: una sombra leve muestra qué suena.
                g.setColour (k.black ? juce::Colours::white.withAlpha (0.10f) : juce::Colour (0xff3a2a18).withAlpha (0.16f));
                g.fillPath (k.hull);
            }
        for (auto& c : A.controls) drawControl (g, c);
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const auto* c = controlAt (e.position);
        const juce::String id = c != nullptr ? c->id : juce::String();
        if (id != hoverId) { hoverId = id; setMouseCursor (c != nullptr ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); help (c); }
    }
    void mouseExit (const juce::MouseEvent&) override { hoverId = {}; if (onHelp) onHelp ({}); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto bg = toBg (e.position);
        if (const auto* c = controlAt (e.position))
        {
            drag = c; dragStart = e.position; help (c);
            if (c->type == "knob" || c->type == "rotary")
            {
                if (auto* prm = param (c->id)) { startNorm = prm->getValue(); prm->beginChangeGesture(); }
            }
            else if (c->type == "rocker") toggle (*c);
            else if (c->type == "wheel") startNorm = c->id == "pitch_wheel" ? proc.pitchWheel.load() : proc.modWheel.load();
            return;
        }
        for (auto& k : assets().keys)
            if (k.hull.contains (bg))
            {
                const auto b = k.hull.getBounds();
                const float vel = juce::jlimit (0.25f, 1.0f, 0.35f + 0.65f * (bg.y - b.getY()) / b.getHeight());
                mouseNote = k.note;
                proc.keyboardState.noteOn (1, k.note, vel);
                return;
            }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag == nullptr)
        {
            // Glissando: arrastrar por el teclado cambia de tecla.
            if (mouseNote < 0) return;
            const auto bg = toBg (e.position);
            for (auto& k : assets().keys)
                if (k.hull.contains (bg) && k.note != mouseNote)
                {
                    proc.keyboardState.noteOff (1, mouseNote, 0.0f);
                    mouseNote = k.note; proc.keyboardState.noteOn (1, k.note, 0.8f);
                    break;
                }
            return;
        }
        const float dy = (dragStart.y - e.position.y) / scale();          // px del fondo, hacia arriba positivo
        if (drag->type == "knob")
        {
            if (auto* prm = param (drag->id))
            {
                const float speed = e.mods.isShiftDown() ? 1500.0f : 300.0f;
                prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, startNorm + dy / speed));
            }
        }
        else if (drag->type == "rotary")
        {
            if (auto* prm = param (drag->id))
            {
                const int steps = (int) drag->detents.size();
                const int base = juce::roundToInt (startNorm * (float) (steps - 1));
                const int st = juce::jlimit (0, steps - 1, base + juce::roundToInt (dy / 40.0f));
                prm->setValueNotifyingHost ((float) st / (float) (steps - 1));
            }
        }
        else if (drag->type == "wheel")
        {
            const bool pitch = drag->id == "pitch_wheel";
            const float range = pitch ? 2.0f : 1.0f;
            const float v = juce::jlimit (pitch ? -1.0f : 0.0f, 1.0f, startNorm + dy * range / drag->dragPx);
            (pitch ? proc.pitchWheel : proc.modWheel).store (v);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (drag != nullptr)
        {
            if (drag->type == "knob" || drag->type == "rotary")
            {
                if (auto* prm = param (drag->id))
                {
                    // Clic sin arrastre en un selector: avanza una posición (Shift o clic derecho: retrocede).
                    if (drag->type == "rotary" && e.getDistanceFromDragStart() < 3)
                    {
                        const int steps = (int) drag->detents.size();
                        int st = juce::roundToInt (prm->getValue() * (float) (steps - 1)) + (e.mods.isShiftDown() || e.mods.isPopupMenu() ? -1 : 1);
                        st = juce::jlimit (0, steps - 1, st);      // como el selector real: no da la vuelta
                        prm->setValueNotifyingHost ((float) st / (float) (steps - 1));
                    }
                    prm->endChangeGesture();
                }
            }
            if (drag->id == "pitch_wheel") proc.pitchWheel.store (0.0f);      // el resorte del centro
            drag = nullptr;
        }
        if (mouseNote >= 0) { proc.keyboardState.noteOff (1, mouseNote, 0.0f); mouseNote = -1; }
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (const auto* c = controlAt (e.position))
            if (c->type == "knob")
                if (auto* prm = param (c->id)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture(); }
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        if (const auto* c = controlAt (e.position))
            if (c->type == "knob")
                if (auto* prm = param (c->id))
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, prm->getValue() + w.deltaY * (e.mods.isShiftDown() ? 0.01f : 0.05f)));
                    prm->endChangeGesture();
                    help (c);
                }
    }

private:
    AtmdProcessor& proc;
    std::map<juce::String, float> last;
    const Control* drag = nullptr;
    juce::Point<float> dragStart;
    float startNorm = 0.0f;
    int mouseNote = -1;
    juce::String hoverId;
    std::atomic<bool> keysDirty { false };

    static const Assets& assets() { return Assets::get(); }
    juce::Point<float> toBg (juce::Point<float> p) const { return p / scale(); }
    juce::RangedAudioParameter* param (const juce::String& id) const { return proc.apvts.getParameter (id); }
    bool keyDown (int note) const { return proc.keyboardState.isNoteOnForChannels (0xffff, note); }

    const Control* controlAt (juce::Point<float> viewPos) const
    {
        const auto p = toBg (viewPos);
        for (auto& c : assets().controls)
            if (c.type != "lamp" && c.hit.contains (p)) return &c;
        return nullptr;
    }

    // Estado del rocker: 1 = apretado arriba o a la derecha (encendido). WHITE/PINK: WHITE está arriba.
    int rockerState (const Control& c) const
    {
        if (c.id == "power") return 1;
        const float v = proc.value (c.id);
        if (c.id == "noise_color") return v < 0.5f ? 1 : 0;
        return v > 0.5f ? 1 : 0;
    }

    void toggle (const Control& c)
    {
        if (c.id == "power") return;
        if (auto* prm = param (c.id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f);
            prm->endChangeGesture();
            help (&c);
        }
    }

    float currentValue (const Control& c) const
    {
        if (c.id == "pitch_wheel") return proc.pitchWheel.load();
        if (c.id == "mod_wheel") return proc.modWheel.load();
        if (c.id == "overload") return proc.overload.load() ? 1.0f : 0.0f;
        if (c.id == "power_led" || c.id == "power") return 1.0f;
        if (auto* prm = param (c.id)) return prm->getValue();
        return 0.0f;
    }

    void drawControl (juce::Graphics& g, const Control& c) const
    {
        const auto& A = assets();
        if (c.type == "knob")
        {
            const float n = currentValue (c);
            const float ang = c.a0 + (c.a1 - c.a0) * n;
            const int frame = juce::roundToInt (ang / 360.0f * 128.0f);
            A.draw (g, c.sprite, c.px, ((frame % 128) + 128) % 128);
        }
        else if (c.type == "rotary")
        {
            const int steps = std::max (1, (int) c.detents.size());
            const int st = juce::jlimit (0, steps - 1, juce::roundToInt (currentValue (c) * (float) (steps - 1)));
            const int frame = juce::roundToInt ((c.detents.empty() ? 0.0f : c.detents[(size_t) st]) / 360.0f * 128.0f);
            A.draw (g, c.sprite, c.px, ((frame % 128) + 128) % 128);
        }
        else if (c.type == "rocker" && c.sprites.size() >= 2) A.draw (g, c.sprites[rockerState (c)], c.px);
        else if (c.type == "lamp" && c.sprites.size() >= 2) A.draw (g, c.sprites[currentValue (c) > 0.5f ? 1 : 0], c.px);
        else if (c.type == "wheel")
        {
            auto it = A.sprites.find (c.sprite);
            if (it == A.sprites.end()) return;
            const int frames = it->second.frames;
            const float v = currentValue (c);
            const float t = c.id == "pitch_wheel" ? (v + 1.0f) * 0.5f : v;
            A.draw (g, c.sprite, c.px, juce::roundToInt (t * (float) (frames - 1)));
        }
    }

    void help (const Control* c)
    {
        if (! onHelp) return;
        if (c == nullptr) { onHelp ({}); return; }
        if (c->id == "pitch_wheel") { onHelp ("PITCH wheel: bends the oscillators (" + juce::String ((int) proc.value ("bend_range")) + " semitones); springs back to the centre"); return; }
        if (c->id == "mod_wheel") { onHelp ("MOD wheel: how much oscillator 3 or noise modulates, where the modulation switches send it"); return; }
        if (c->id == "power") { onHelp ("POWER: always on in the plugin"); return; }
        const auto& d = paramDef (c->id);
        onHelp (d.name + ": " + formatParam (d, proc.value (c->id)) + "   " + d.help);
    }

    void timerCallback() override
    {
        const auto& A = assets();
        const float s = scale();
        for (auto& c : A.controls)
        {
            const float v = c.type == "rocker" ? (float) rockerState (c) : currentValue (c);
            auto& l = last[c.id];
            if (std::abs (v - l) > 1e-5f)
            {
                l = v;
                repaint ((c.bounds.transformedBy (juce::AffineTransform::scale (s))).getSmallestIntegerContainer().expanded (2));
                if (c.id == hoverId && drag == &c) help (&c);
            }
        }
        if (keysDirty.exchange (false))
            repaint (A.keysCrop.transformedBy (juce::AffineTransform::scale (s)).getSmallestIntegerContainer().expanded (2));
    }

    void handleNoteOn (juce::MidiKeyboardState*, int, int, float) override { keysDirty = true; }
    void handleNoteOff (juce::MidiKeyboardState*, int, int, float) override { keysDirty = true; }
};

} // namespace dh::md::ui
