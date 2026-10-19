#pragma once
// Página SETTINGS: lo que el Model D no tenía (voces, expresión, teclado, delay, reverb, sistema), en una
// placa negra con serigrafía blanca que tapa el panel y la tabla del nombre. El teclado queda a la vista y se
// sigue tocando. Usa las mismas perillas y rockers renderizados del panel, dibujados en píxeles del fondo.

#include "../PluginProcessor.h"
#include "AssetsMd.h"

namespace dh::md::ui {

class SettingsView : public juce::Component, private juce::Timer
{
public:
    std::function<void (const juce::String&)> onHelp;
    // Zona del fondo que cubre (px del fondo): del riel superior al final de la tabla del nombre.
    static juce::Rectangle<float> area() { return { 108.0f, 150.0f, 2788.0f, 935.0f }; }

    explicit SettingsView (AtmdProcessor& p) : proc (p)
    {
        const float x0 = area().getX(), y0 = area().getY();
        auto group = [&] (const char* title, float gx, float gy, float gw, float gh) { groups.push_back ({ title, { x0 + gx, y0 + gy, gw, gh } }); };
        auto knob = [&] (const char* id, const char* label, float cx, float cy) { items.push_back ({ id, label, Knob, { x0 + cx, y0 + cy } }); };
        auto rocker = [&] (const char* id, const char* label, float cx, float cy) { items.push_back ({ id, label, Rocker, { x0 + cx, y0 + cy } }); };
        auto choice = [&] (const char* id, const char* label, float cx, float cy) { items.push_back ({ id, label, Choice, { x0 + cx, y0 + cy } }); };
        // Fila 1: VOICES · EXPRESSION · KEYBOARD (títulos en la franja de abajo de cada grupo, como en el panel).
        group ("VOICES", 40, 50, 1140, 400);
        choice ("voices", "VOICES", 360, 150); choice ("unison", "UNISON", 360, 290);
        knob ("detune", "UNISON\nDETUNE", 800, 160); knob ("spread", "STEREO\nSPREAD", 950, 160); knob ("analog", "ANALOG\nDRIFT", 1100, 160);
        group ("EXPRESSION", 1210, 50, 940, 400);
        knob ("vel_filter", "VELOCITY\nTO FILTER", 1320, 150); knob ("vel_amp", "VELOCITY\nTO LOUDNESS", 1500, 150);
        knob ("at_filter", "PRESSURE\nTO FILTER", 1680, 150); knob ("at_mod", "PRESSURE\nTO MOD.", 1860, 150);
        choice ("bend_range", "PITCH WHEEL RANGE (SEMITONES)", 1680, 345);
        group ("KEYBOARD", 2180, 50, 568, 400);
        choice ("priority", "NOTE PRIORITY", 2464, 150); choice ("trigger", "TRIGGER", 2464, 290);
        // Fila 2: DELAY · REVERB · SYSTEM
        group ("DELAY", 40, 480, 1140, 430);
        rocker ("dly_on", "ON", 140, 640); choice ("dly_time", "TIME", 700, 585);
        knob ("dly_fb", "FEEDBACK", 450, 752); knob ("dly_tone", "TONE", 700, 752); knob ("dly_mix", "MIX", 950, 752);
        group ("REVERB", 1210, 480, 940, 430);
        rocker ("rev_on", "ON", 1300, 640);
        knob ("rev_size", "SIZE", 1520, 680); knob ("rev_damp", "DAMPING", 1740, 680); knob ("rev_mix", "MIX", 1960, 680);
        group ("SYSTEM", 2180, 480, 568, 430);
        choice ("oversampling", "OVERSAMPLING", 2464, 590); knob ("output", "OUTPUT", 2464, 752);
        startTimerHz (20);
    }

    void paint (juce::Graphics& g) override
    {
        const auto& A = Assets::get();
        const float s = scale();
        g.addTransform (juce::AffineTransform::scale (s).translated (-area().getX() * s, -area().getY() * s));
        // Placa: aluminio cepillado en el borde, cara negra.
        const auto r = area();
        g.setColour (juce::Colour (0xff8d9094)); g.fillRoundedRectangle (r, 6.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff141516), r.getX(), r.getY(), juce::Colour (0xff0d0e0f), r.getX(), r.getBottom(), false));
        g.fillRect (r.reduced (10.0f));
        g.setColour (juce::Colours::white);
        for (auto& gr : groups)
        {
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.drawRect (gr.r, 2.0f);
            g.setFont (font (38.0f, true));
            g.setColour (juce::Colours::white);
            g.drawText (gr.title, gr.r.withHeight (60.0f).translated (0, gr.r.getHeight() - 64.0f), juce::Justification::centred);
        }
        g.setFont (font (26.0f, true));
        g.drawText ("EXTENSIONS: WHAT THE ORIGINAL NEVER HAD", r.withTrimmedBottom (r.getHeight() - 40.0f).translated (0, 6.0f), juce::Justification::centred);
        for (auto& it : items) drawItem (g, A, it);
    }

    void mouseMove (const juce::MouseEvent& e) override { const auto* it = itemAt (e.position); help (it); setMouseCursor (it ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor); }
    void mouseExit (const juce::MouseEvent&) override { if (onHelp) onHelp ({}); }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto* it = itemAt (e.position);
        if (it == nullptr) return;
        auto* prm = proc.apvts.getParameter (it->id);
        if (prm == nullptr) return;
        if (it->kind == Rocker) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getValue() > 0.5f ? 0.0f : 1.0f); prm->endChangeGesture(); help (it); return; }
        if (it->kind == Choice)
        {
            const int idx = choiceAt (*it, toBg (e.position));
            if (idx >= 0) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->convertTo0to1 (paramDef (it->id).lo + (float) idx)); prm->endChangeGesture(); help (it); }
            return;
        }
        drag = it; dragY = e.position.y; startNorm = prm->getValue(); prm->beginChangeGesture();
    }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (drag == nullptr) return;
        if (auto* prm = proc.apvts.getParameter (drag->id))
            prm->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, startNorm + (dragY - e.position.y) / scale() / (e.mods.isShiftDown() ? 1500.0f : 300.0f)));
        help (drag);
    }
    void mouseUp (const juce::MouseEvent&) override { if (drag != nullptr) if (auto* prm = proc.apvts.getParameter (drag->id)) prm->endChangeGesture(); drag = nullptr; }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (const auto* it = itemAt (e.position); it != nullptr && it->kind == Knob)
            if (auto* prm = proc.apvts.getParameter (it->id)) { prm->beginChangeGesture(); prm->setValueNotifyingHost (prm->getDefaultValue()); prm->endChangeGesture(); }
    }

private:
    enum Kind { Knob, Rocker, Choice };
    struct Item { juce::String id, label; Kind kind; juce::Point<float> c; };
    struct Group { juce::String title; juce::Rectangle<float> r; };
    AtmdProcessor& proc;
    std::vector<Item> items;
    std::vector<Group> groups;
    const Item* drag = nullptr;
    float dragY = 0.0f, startNorm = 0.0f;
    std::vector<float> last;

    float scale() const { return (float) getWidth() / area().getWidth(); }
    juce::Point<float> toBg (juce::Point<float> p) const { return p / scale() + area().getPosition(); }
    static juce::Font font (float h, bool bold) { return juce::Font (juce::FontOptions ("Futura", h, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (0.03f); }

    // Opciones de un parámetro de elección (o entero), como celdas en una fila.
    juce::StringArray options (const juce::String& id) const
    {
        const auto& d = paramDef (id);
        if (d.choices.isNotEmpty()) return juce::StringArray::fromTokens (d.choices, "|", "");
        juce::StringArray s;
        if (id == "bend_range") { for (int v : { 1, 2, 3, 5, 7, 12, 24 }) s.add (juce::String (v)); return s; }
        for (int v = (int) d.lo; v <= (int) d.hi; ++v) s.add (juce::String (v));
        return s;
    }
    juce::Rectangle<float> cell (const Item& it, int i, int n) const
    {
        const float w = n > 6 ? 76.0f : (n > 4 ? 100.0f : 130.0f), h = 54.0f;
        const float x = it.c.x - (float) n * w * 0.5f + (float) i * w;
        return { x + 3.0f, it.c.y - h * 0.5f, w - 6.0f, h };
    }
    int choiceAt (const Item& it, juce::Point<float> bg) const
    {
        const auto opts = options (it.id);
        for (int i = 0; i < opts.size(); ++i)
            if (cell (it, i, opts.size()).expanded (2.0f).contains (bg)) return it.id == "bend_range" ? opts[i].getIntValue() - (int) paramDef (it.id).lo : i;
        return -1;
    }
    juce::Rectangle<float> itemBounds (const Item& it) const
    {
        if (it.kind == Knob) return { it.c.x - 90.0f, it.c.y - 80.0f, 180.0f, 200.0f };
        if (it.kind == Rocker) return { it.c.x - 60.0f, it.c.y - 40.0f, 120.0f, 120.0f };
        const int n = options (it.id).size();
        return cell (it, 0, n).getUnion (cell (it, n - 1, n)).withTrimmedTop (-60.0f);
    }
    const Item* itemAt (juce::Point<float> viewPos) const
    {
        const auto p = toBg (viewPos);
        for (auto& it : items) if (itemBounds (it).contains (p)) return &it;
        return nullptr;
    }

    void drawItem (juce::Graphics& g, const Assets& A, const Item& it) const
    {
        const float v = proc.value (it.id);
        auto* prm = proc.apvts.getParameter (it.id);
        const float norm = prm != nullptr ? prm->getValue() : 0.0f;
        g.setColour (juce::Colours::white);
        if (it.kind == Knob)
        {
            // Escala del dial: 11 marcas de −150° a +150°, como las perillas del panel.
            for (int k = 0; k <= 10; ++k)
            {
                const float a = juce::degreesToRadians (-150.0f + 30.0f * (float) k);
                const auto p0 = it.c + juce::Point<float> (std::sin (a), -std::cos (a) * 0.866f) * 62.0f;
                const auto p1 = it.c + juce::Point<float> (std::sin (a), -std::cos (a) * 0.866f) * (k % 5 == 0 ? 76.0f : 70.0f);
                g.drawLine ({ p0, p1 }, 2.5f);
            }
            const int frame = juce::roundToInt ((-150.0f + 300.0f * norm) / 360.0f * 128.0f);
            A.draw (g, "knob_std", it.c, ((frame % 128) + 128) % 128);
            g.setFont (font (24.0f, true));
            g.drawFittedText (it.label, juce::Rectangle<float> (it.c.x - 110.0f, it.c.y + 68.0f, 220.0f, 56.0f).toNearestInt(), juce::Justification::centredTop, 2);
            g.setColour (juce::Colours::white.withAlpha (0.6f)); g.setFont (font (22.0f, false));
            g.drawText (formatParam (paramDef (it.id), v), juce::Rectangle<float> (it.c.x - 110.0f, it.c.y - 112.0f, 220.0f, 28.0f), juce::Justification::centred);
        }
        else if (it.kind == Rocker)
        {
            A.draw (g, v > 0.5f ? "rocker_blue_h_1" : "rocker_blue_h_0", it.c);
            g.setFont (font (24.0f, true));
            g.drawText (it.label, juce::Rectangle<float> (it.c.x - 60.0f, it.c.y + 34.0f, 120.0f, 30.0f), juce::Justification::centred);
        }
        else
        {
            const auto opts = options (it.id);
            const int sel = it.id == "bend_range" ? opts.indexOf (juce::String ((int) std::lround (v))) : juce::roundToInt (v - paramDef (it.id).lo);
            g.setFont (font (24.0f, true));
            const auto first = cell (it, 0, opts.size()), lastC = cell (it, opts.size() - 1, opts.size());
            g.drawText (it.label, juce::Rectangle<float> (first.getX(), first.getY() - 44.0f, lastC.getRight() - first.getX(), 34.0f), juce::Justification::centred);
            for (int i = 0; i < opts.size(); ++i)
            {
                const auto r = cell (it, i, opts.size());
                const bool on = i == sel;
                g.setColour (on ? juce::Colour (0xff3d8fd1) : juce::Colour (0xff26282b));
                g.fillRoundedRectangle (r, 5.0f);
                g.setColour (juce::Colours::white.withAlpha (on ? 1.0f : 0.55f));
                g.drawRoundedRectangle (r, 5.0f, 1.5f);
                g.setFont (font (opts.size() > 6 ? 22.0f : 24.0f, true));
                g.drawFittedText (opts[i].toUpperCase(), r.toNearestInt(), juce::Justification::centred, 1, 0.7f);
            }
        }
    }

    void help (const Item* it)
    {
        if (! onHelp) return;
        if (it == nullptr) { onHelp ({}); return; }
        const auto& d = paramDef (it->id);
        onHelp (d.name + ": " + formatParam (d, proc.value (it->id)) + "   " + d.help);
    }

    void timerCallback() override
    {
        bool changed = last.size() != items.size();
        last.resize (items.size());
        for (size_t i = 0; i < items.size(); ++i)
        {
            const float v = proc.value (items[i].id);
            if (std::abs (v - last[i]) > 1e-6f) { last[i] = v; changed = true; }
        }
        if (changed) repaint();
    }
};

} // namespace dh::md::ui
