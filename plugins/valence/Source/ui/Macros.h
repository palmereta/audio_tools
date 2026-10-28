#pragma once
// Las seis macros de la página Simple: figura, densidad, swing, octavas, mutación y gate. No son parámetros: cada una
// escribe parámetros que ya existen en las cuatro capas (automatizables, entre los primeros 64), así que el estado, los
// presets y la lista de Live no cambian, y el host graba lo que la macro mueve.
// - Lectura: la figura de la primera capa encendida; las demás, el promedio de las capas encendidas (todas, si no hay
//   ninguna). El texto muestra el rango si las capas difieren ("50-58 %").
// - Escritura: al empezar el gesto se guarda el valor de cada capa (la base) y el desplazamiento de la macro se suma a
//   esa base, recortado al rango del parámetro. Volver al punto de partida devuelve la base exacta, y el resultado sólo
//   depende de la base y de la posición final (determinista; la tabla está en el README, "Editor").

#include "Widgets.h"

namespace dh::val::ui {

enum MacroKind : int { MacFigure = 0, MacDensity, MacSwing, MacOctaves, MacMutate, MacGate, MacroCount };

class MacroKnob : public juce::Component
{
public:
    MacroKnob (ValenceProcessor& p, int k) : proc (p), kind (k)
    {
        static const char* suffix[MacroCount] = { "rate", "eu_pulses", "swing", "octaves", "mutate", "gate" };
        for (int l = 0; l < kLayers; ++l)
        {
            const juce::String pre = "l" + juce::String (l + 1) + "_";
            prm[(size_t) l] = p.apvts.getParameter (pre + suffix[kind]);
            lenP[(size_t) l] = p.apvts.getParameter (pre + "length");
            onP[(size_t) l] = p.apvts.getParameter (pre + "on");
        }
        setRepaintsOnMouseActivity (true);
        sample();
    }
    // La página la llama unas veces por segundo mientras se ve (un solo temporizador para las seis).
    void poll() { if (! dragging && sample()) repaint(); }
    static const char* name (int k) { static const char* N[MacroCount] = { "Figure", "Density", "Swing", "Octaves", "Mutation", "Gate" }; return N[k]; }
    static const char* help (int k)
    {
        static const char* H[MacroCount] = {
            "the step figure of every layer, one division at a time (1/4 to 1/32). Each layer keeps its distance from the others and its triplet or dotted feel; the knob shows the first layer that is on",
            "how many steps play, from a few to all: every layer thins or fills with evenly spread (Euclid) pulses. A layer drawn by hand keeps its own steps until its count changes",
            "swing of every layer (50 % is straight), each one moved by the same amount",
            "how many octaves the patterns span (1 to 4), every layer moved by the same amount",
            "how much each layer changes a little at every cycle (probability, octave, a step, velocity, ratchet), each with its own random stream",
            "note length as a part of the step, every layer moved by the same amount" };
        return H[k];
    }

    void paint (juce::Graphics& g) override
    {
        drawOrbit (g, getLocalBounds().toFloat(), dragging ? dragPos : pos, txt, name (kind), col::sun, true, isMouseOverOrDragging());
    }
    void mouseEnter (const juce::MouseEvent&) override { showHelp (juce::String (name (kind)) + ": " + help (kind) + ". Double-click: every layer back to its default"); }
    void mouseExit (const juce::MouseEvent&) override { showHelp ({}); }
    void mouseDown (const juce::MouseEvent&) override { begin(); dragging = true; dragPos = base.pos; }
    void mouseDrag (const juce::MouseEvent& e) override
    {
        dragPos = juce::jlimit (0.0f, 1.0f, base.pos - (float) e.getDistanceFromDragStartY() / (e.mods.isShiftDown() ? 900.0f : 220.0f));
        apply (dragPos);
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { end(); dragging = false; sample(); repaint(); }
    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        for (auto* p : prm) if (p != nullptr) { p->beginChangeGesture(); p->setValueNotifyingHost (p->getDefaultValue()); p->endChangeGesture(); }
        sample(); repaint();
    }
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const int k = wheel.take (w);
        if (k == 0) return;
        begin();
        apply (juce::jlimit (0.0f, 1.0f, base.pos + (float) k * unit()));
        end();
        sample(); repaint();
    }

private:
    ValenceProcessor& proc;
    int kind;
    std::array<juce::RangedAudioParameter*, kLayers> prm {}, lenP {}, onP {};
    struct Base { std::array<float, kLayers> v {}; std::array<int, kLayers> len {}, hits {}; float pos = 0.0f; };
    Base base;
    float pos = 0.0f, dragPos = 0.0f;
    bool dragging = false;
    juce::String txt;
    dh::WheelSteps wheel;

    float real (const juce::RangedAudioParameter* p) const { return p != nullptr ? p->convertFrom0to1 (p->getValue()) : 0.0f; }
    bool layerOn (int l) const { return real (onP[(size_t) l]) > 0.5f; }
    float unit() const
    {
        switch (kind) { case MacFigure: return 1.0f / 3.0f; case MacSwing: return 1.0f / 25.0f; case MacOctaves: return 1.0f / 3.0f; default: return 0.05f; }
    }
    // Pasos que suenan de la capa: los pulsos del euclidiano, o los pasos encendidos de las filas.
    static int rowHits (const Rows& rows, int l, int len, int pulses)
    {
        if (pulses > 0) return std::min (pulses, len);
        int n = 0; for (int k = 0; k < len; ++k) n += rows[(size_t) l][(size_t) k].on ? 1 : 0;
        return n;
    }
    // La posición 0..1 de una capa en la escala de la macro.
    float layerPos (int l, const Rows& rows) const
    {
        const float v = real (prm[(size_t) l]);
        switch (kind)
        {
            case MacFigure:  return (float) ((int) std::lround (v) / 3) / 3.0f;
            case MacDensity: { const int len = std::max (1, (int) std::lround (real (lenP[(size_t) l]))); return (float) rowHits (rows, l, len, (int) std::lround (v)) / (float) len; }
            case MacSwing:   return (v - 50.0f) / 25.0f;
            case MacOctaves: return (v - 1.0f) / 3.0f;
            case MacGate:    return (v - 0.05f) / 0.95f;
            default:         return v;
        }
    }
    int leadLayer() const { for (int l = 0; l < kLayers; ++l) if (layerOn (l)) return l; return 0; }

    // Lee la posición y el texto; devuelve si cambiaron.
    bool sample()
    {
        const auto rows = kind == MacDensity ? proc.getRows() : Rows {};
        float sum = 0.0f, lo = 1.0e9f, hi = -1.0e9f; int n = 0;
        bool any = false; for (int l = 0; l < kLayers; ++l) any = any || layerOn (l);
        for (int l = 0; l < kLayers; ++l)
        {
            if (any && ! layerOn (l)) continue;
            const float p = layerPos (l, rows);
            sum += p; ++n; lo = std::min (lo, p); hi = std::max (hi, p);
        }
        float np = n > 0 ? sum / (float) n : 0.0f;
        juce::String t;
        auto range = [&] (float a, float b, auto fmt) { const auto sa = fmt (a), sb = fmt (b); return sa == sb ? sa : sa + "-" + sb; };
        switch (kind)
        {
            case MacFigure: { const int l = leadLayer(); np = layerPos (l, rows); t = figureName ((int) std::lround (real (prm[(size_t) l]))); break; }
            case MacDensity: t = range (lo, hi, [] (float x) { return juce::String (juce::roundToInt (x * 100.0f)); }) + " %"; break;
            case MacSwing:   t = range (lo, hi, [] (float x) { return juce::String (juce::roundToInt (50.0f + x * 25.0f)); }) + " %"; break;
            case MacOctaves: t = range (lo, hi, [] (float x) { return juce::String (juce::roundToInt (1.0f + x * 3.0f)); }) + " oct"; break;
            default:         t = range (lo, hi, [] (float x) { return juce::String (juce::roundToInt (x * 100.0f)); }) + " %"; break;
        }
        const bool changed = std::abs (np - pos) > 1.0e-4f || t != txt;
        pos = np; txt = t;
        return changed;
    }
    void begin()
    {
        sample();
        const auto rows = proc.getRows();
        for (int l = 0; l < kLayers; ++l)
        {
            base.v[(size_t) l] = real (prm[(size_t) l]);
            base.len[(size_t) l] = std::max (1, (int) std::lround (real (lenP[(size_t) l])));
            base.hits[(size_t) l] = rowHits (rows, l, base.len[(size_t) l], (int) std::lround (base.v[(size_t) l]));
            if (prm[(size_t) l] != nullptr) prm[(size_t) l]->beginChangeGesture();
        }
        base.pos = pos;
    }
    void end() { for (auto* p : prm) if (p != nullptr) p->endChangeGesture(); }
    void apply (float v)
    {
        const float d = v - base.pos;
        for (int l = 0; l < kLayers; ++l)
        {
            auto* p = prm[(size_t) l];
            if (p == nullptr) continue;
            const float b = base.v[(size_t) l];
            float x = b;
            switch (kind)
            {
                case MacFigure:
                {
                    // Divisiones enteras (1/4, 1/8, 1/16, 1/32): cada capa conserva su familia (recta, tresillo, puntillo).
                    const int f = (int) std::lround (b), dd = juce::roundToInt (v * 3.0f) - juce::roundToInt (base.pos * 3.0f);
                    x = (float) (juce::jlimit (0, 3, f / 3 + dd) * 3 + f % 3);
                    break;
                }
                case MacDensity:
                {
                    const int len = base.len[(size_t) l], h0 = base.hits[(size_t) l];
                    const int n = juce::jlimit (1, len, juce::roundToInt (juce::jlimit (0.0f, 1.0f, (float) h0 / (float) len + d) * (float) len));
                    x = n == h0 ? b : (float) n;          // la misma cantidad: se queda como estaba (filas a mano o sus pulsos)
                    break;
                }
                case MacSwing:   x = (float) juce::jlimit (50, 75, juce::roundToInt (b + d * 25.0f)); break;
                case MacOctaves: x = (float) juce::jlimit (1, 4, juce::roundToInt (b + d * 3.0f)); break;
                case MacGate:    x = juce::jlimit (0.05f, 1.0f, b + d * 0.95f); break;
                default:         x = juce::jlimit (0.0f, 1.0f, b + d); break;
            }
            const float nv = p->convertTo0to1 (x);
            if (std::abs (nv - p->getValue()) > 1.0e-6f) p->setValueNotifyingHost (nv);
        }
    }
};

} // namespace dh::val::ui
