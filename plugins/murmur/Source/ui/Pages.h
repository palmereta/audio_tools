#pragma once
// Las dos páginas de Murmur. Simple: la bandada y seis perillas grandes. Advanced: todo, por sección.

#include "FlockView.h"

namespace dh::mm::ui {

class Page : public juce::Component
{
public:
    explicit Page (MurmurProcessor& p) : proc (p) {}
    virtual void refresh() { for (auto* c : getChildren()) c->repaint(); }
protected:
    MurmurProcessor& proc;
    std::vector<std::unique_ptr<juce::Component>> owned;
    template <typename T, typename... A> T* make (A&&... a)
    {
        auto c = std::make_unique<T> (std::forward<A> (a)...);
        auto* raw = c.get(); addAndMakeVisible (*raw); owned.push_back (std::move (c)); return raw;
    }
    // Perilla o caja según el tipo de parámetro.
    juce::Component* control (const juce::String& id, const juce::String& label, bool big = false)
    {
        const auto& d = paramDef (id);
        if (d.choices.isNotEmpty()) return make<ParamBox> (proc, id, label);
        return make<BirdKnob> (proc, id, label, juce::String(), big);
    }
};

class SimplePage : public Page
{
public:
    explicit SimplePage (MurmurProcessor& p) : Page (p), flock (p)
    {
        addAndMakeVisible (flock);
        for (auto* id : { "size", "density", "pitch", "spray", "feedback", "mix" })
            big.push_back (control (id, paramDef (id).name, true));
        freeze = make<ParamBox> (proc, "freeze", "Freeze", "", false);
        freeze->tint = col::select;
        for (auto* id : { "position", "pitch_rand", "shimmer", "reverse", "rev_mix", "delay_mix" })
            small.push_back (control (id, juce::String (id) == "rev_mix" ? "Reverb" : (juce::String (id) == "delay_mix" ? "Delay" : paramDef (id).name)));
    }
    void paint (juce::Graphics& g) override
    {
        panel (g, rightR, "Shape the cloud");
        panel (g, bottomR, {});
    }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 10);
        rightR = r.removeFromRight (360);
        r.removeFromRight (12);
        bottomR = r.removeFromBottom (128);
        r.removeFromBottom (10);
        flock.setBounds (r);
        auto inner = rightR.withTrimmedTop (36).reduced (14, 4);
        freeze->setBounds (inner.removeFromBottom (46).reduced (10, 2));
        inner.removeFromBottom (8);
        gridLayout (inner, big, 2, (inner.getHeight() - 16) / 3, 8);
        gridLayout (bottomR.reduced (14, 10), small, 6, bottomR.getHeight() - 20, 8);
    }
private:
    FlockView flock;
    std::vector<juce::Component*> big, small;
    ParamBox* freeze = nullptr;
    juce::Rectangle<int> rightR, bottomR;
};

class AdvancedPage : public Page
{
public:
    explicit AdvancedPage (MurmurProcessor& p) : Page (p)
    {
        auto group = [this] (const juce::String& title, std::initializer_list<const char*> ids) {
            Group gr; gr.title = title;
            for (auto* id : ids) gr.items.push_back (control (id, shortName (id)));
            groups.push_back (gr);
        };
        group ("Grains", { "size", "size_rand", "density", "density_rand", "sync", "chance", "voices" });
        group ("Time", { "buffer", "position", "spray", "scan", "freeze" });
        group ("Pitch", { "pitch", "fine", "pitch_rand", "quant", "shimmer", "sub", "reverse" });
        group ("Envelope and stereo", { "shape", "skew", "pan", "width" });
        group ("Tone and level", { "low_cut", "high_cut", "feedback", "duck", "in_gain", "out_gain", "mix" });
        group ("LFO 1", { "lfo1_target", "lfo1_amount", "lfo1_rate", "lfo1_sync", "lfo1_shape" });
        group ("LFO 2", { "lfo2_target", "lfo2_amount", "lfo2_rate", "lfo2_sync", "lfo2_shape" });
        group ("Effects and MIDI", { "delay_mix", "delay_time", "delay_fb", "delay_sync", "rev_mix", "rev_size", "rev_damp", "midi_mode" });
    }
    void paint (juce::Graphics& g) override { for (auto& gr : groups) panel (g, gr.bounds, gr.title); }
    void resized() override
    {
        auto r = getLocalBounds().reduced (14, 10);
        const int cols = 4, gap = 12;
        const int cw = (r.getWidth() - gap * (cols - 1)) / cols, rh = (r.getHeight() - gap) / 2;
        for (size_t i = 0; i < groups.size(); ++i)
        {
            auto& gr = groups[i];
            gr.bounds = { r.getX() + (int) (i % cols) * (cw + gap), r.getY() + (int) (i / cols) * (rh + gap), cw, rh };
            auto inner = gr.bounds.withTrimmedTop (34).reduced (10, 6);
            // Perillas en una grilla de 3 columnas; las cajas (elecciones) abajo, de a 2.
            std::vector<juce::Component*> knobs, boxes;
            for (auto* c : gr.items) (dynamic_cast<ParamBox*> (c) != nullptr ? boxes : knobs).push_back (c);
            const int boxRows = ((int) boxes.size() + 1) / 2;
            auto boxArea = inner.removeFromBottom (boxRows * 46);
            gridLayout (boxArea, boxes, 2, 38, 8);
            const int kRows = std::max (1, ((int) knobs.size() + 2) / 3);
            gridLayout (inner, knobs, 3, std::min (100, (inner.getHeight() - 8 * (kRows - 1)) / kRows), 8);
        }
    }
private:
    struct Group { juce::String title; std::vector<juce::Component*> items; juce::Rectangle<int> bounds; };
    std::vector<Group> groups;
    static juce::String shortName (const juce::String& id)
    {
        auto n = paramDef (id).name;
        for (auto pre : { "LFO 1 ", "LFO 2 " }) if (n.startsWith (pre)) n = n.substring ((int) std::strlen (pre));
        if (n == "Delay Feedback") n = "Delay Fb";
        if (n == "Reverb Damping") n = "Damping";
        return n;
    }
};

} // namespace dh::mm::ui
