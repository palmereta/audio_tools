#pragma once
// Página EXTENSIONS: vocoder, polifonía, delay, reverb y sistema, con el mismo panel y las mismas
// perillas renderizadas que el MS-20 (sólo cambia la serigrafía).

#include <JuceHeader.h>
#include "../PluginProcessor.h"
#include "Assets.h"
#include "LayoutExt.generated.h"

namespace dh {

class ExtPanelView : public juce::Component, private juce::Timer
{
public:
    explicit ExtPanelView (AT20VProcessor& p) : proc (p)
    {
        setSize ((int) A.viewWidth(), (int) A.viewHeight());
        for (const auto& k : layout::extKnobs)
        {
            Knob kv;
            kv.def = &k;
            kv.base = A.toView (k.x, k.y);
            kv.radius = k.radius * A.pxPerUnit * 0.5f;
            kv.param = proc.apvts.getParameter (k.param);
            kv.info = findExt (k.param);
            knobs.push_back (kv);
        }
        startTimerHz (30);
    }

    void paint (juce::Graphics& g) override
    {
        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        if (A.backgroundExt.isValid()) g.drawImage (A.backgroundExt, getLocalBounds().toFloat());
        else g.fillAll (juce::Colour (0xff1d1d1f));

        // LED de señal en la entrada (modulador del vocoder / ESP).
        const float lvl = juce::jlimit (0.0f, 1.0f, proc.inputLevel.load() * 4.0f);
        if (lvl > 0.02f)
        {
            const auto c = A.toView (layout::extLedX, layout::extLedY, 2.0f);
            juce::ColourGradient glow (juce::Colour (0xffff3018).withAlpha (0.6f * lvl), c.x, c.y, juce::Colour (0x00ff3018), c.x + 15.0f, c.y, true);
            g.setGradientFill (glow);
            g.fillEllipse (c.x - 15.0f, c.y - 15.0f, 30.0f, 30.0f);
            if (A.ledOn.isValid())
            {
                g.setOpacity (lvl);
                const float s = (float) A.ledOn.getWidth() * 0.5f;
                g.drawImage (A.ledOn, c.x - s * 0.5f, c.y - s * 0.5f + 1.0f, s, s, 0, 0, A.ledOn.getWidth(), A.ledOn.getHeight());
            }
        }

        g.setOpacity (1.0f);
        for (const auto& k : knobs)
        {
            const auto& sheet = A.knobSmall;
            if (! sheet.image.isValid()) continue;
            const float ang = angleOf (k);
            const int f = (int) std::lround ((ang < 0 ? ang + 360.0f : ang) / (360.0f / (float) sheet.frames)) % sheet.frames;
            const auto src = sheet.frame (f);
            const float s = (float) sheet.size * 0.5f;
            g.drawImage (sheet.image, k.base.x - sheet.anchorX * 0.5f, k.base.y - sheet.anchorY * 0.5f, s, s,
                         src.getX(), src.getY(), src.getWidth(), src.getHeight());
        }

        const int shown = dragKnob >= 0 ? dragKnob : hoverKnob;
        if (shown >= 0)
        {
            const auto& k = knobs[(size_t) shown];
            const juce::String title = juce::String (k.info != nullptr ? k.info->name : k.def->param).toUpperCase();
            const juce::String value = k.info != nullptr ? formatExt (*k.info, valueOf (k)) : juce::String();
            const juce::Font f1 (juce::FontOptions (11.0f, juce::Font::bold));
            const juce::Font f2 (juce::FontOptions (13.5f, juce::Font::plain));
            const float w = std::max (juce::GlyphArrangement::getStringWidth (f1, title), juce::GlyphArrangement::getStringWidth (f2, value)) + 22.0f;
            juce::Rectangle<float> r (k.base.x - w * 0.5f, k.base.y - k.radius * 2.0f - 46.0f, w, 40.0f);
            if (r.getY() < 4.0f) r.setY (k.base.y + k.radius + 12.0f);
            r = r.withX (juce::jlimit (4.0f, (float) getWidth() - w - 4.0f, r.getX()));
            g.setColour (juce::Colour (0xf0121214)); g.fillRoundedRectangle (r, 6.0f);
            g.setColour (juce::Colour (0x40ffffff)); g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
            g.setColour (juce::Colour (0xffa9a9a4)); g.setFont (f1);
            g.drawText (title, r.withHeight (20.0f).translated (0, 3), juce::Justification::centred);
            g.setColour (juce::Colour (0xfff2efe6)); g.setFont (f2);
            g.drawText (value, r.withTrimmedTop (18.0f), juce::Justification::centred);
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const int k = knobAt (e.position);
        if (k != hoverKnob) { hoverKnob = k; setMouseCursor (k >= 0 ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::NormalCursor); repaint(); }
    }
    void mouseExit (const juce::MouseEvent&) override { if (hoverKnob >= 0) { hoverKnob = -1; repaint(); } }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragKnob = knobAt (e.position);
        if (dragKnob < 0) return;
        lastY = e.position.y; lastX = e.position.x; accum = 0.0f;
        auto& k = knobs[(size_t) dragKnob];
        // Selectores de dos posiciones (OFF/ON, REV 1/2…): un clic alcanza para cambiarlos.
        if (k.def->steps == 2) { k.param->beginChangeGesture(); setValue (k, valueOf (k) > 0.5f ? 0.0f : 1.0f); toggled = true; }
        else { k.param->beginChangeGesture(); toggled = false; }
        e.source.enableUnboundedMouseMovement (! toggled);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (dragKnob < 0 || toggled) return;
        auto& k = knobs[(size_t) dragKnob];
        const float dy = (lastY - e.position.y) + (e.position.x - lastX); lastY = e.position.y; lastX = e.position.x;   // los dos ejes
        if (k.def->steps > 0)
        {
            accum += dy;
            if (std::abs (accum) >= 24.0f) { setValue (k, valueIndex (k) + (accum > 0 ? 1.0f : -1.0f)); accum = 0.0f; }
        }
        else
        {
            const float span = k.info != nullptr ? k.info->hi - k.info->lo : 1.0f;
            setValue (k, valueOf (k) + dy * span * (e.mods.isShiftDown() ? 0.2f : 1.0f) / 220.0f);
        }
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (dragKnob >= 0) knobs[(size_t) dragKnob].param->endChangeGesture();
        e.source.enableUnboundedMouseMovement (false);
        dragKnob = -1;
        repaint();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        const int i = knobAt (e.position);
        if (i < 0 || knobs[(size_t) i].info == nullptr || knobs[(size_t) i].def->steps == 2) return;
        auto& k = knobs[(size_t) i];
        k.param->beginChangeGesture(); setValue (k, k.info->def); k.param->endChangeGesture();
    }

    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override
    {
        const int i = knobAt (e.position);
        if (i < 0) return;
        auto& k = knobs[(size_t) i];
        const float d = w.isReversed ? -w.deltaY : w.deltaY;
        if (k.def->steps > 0) { if (std::abs (d) > 0.05f) setValue (k, valueIndex (k) + (d > 0 ? 1.0f : -1.0f)); }
        else if (k.info != nullptr) setValue (k, valueOf (k) + d * (k.info->hi - k.info->lo) * 0.08f);
    }

private:
    struct Knob
    {
        const layout::ExtKnobDef* def = nullptr;
        const ExtParam* info = nullptr;
        juce::RangedAudioParameter* param = nullptr;
        juce::Point<float> base;
        float radius = 20.0f, shown = -999.0f;
    };

    float valueOf (const Knob& k) const { return k.param != nullptr ? k.param->convertFrom0to1 (k.param->getValue()) : 0.0f; }
    float valueIndex (const Knob& k) const { return std::round (valueOf (k)); }

    float angleOf (const Knob& k) const
    {
        const float v = valueOf (k);
        if (k.def->steps > 0) return k.def->startDeg + (k.def->endDeg - k.def->startDeg) * v / (float) (k.def->steps - 1);
        const float t = k.info != nullptr ? (v - k.info->lo) / (k.info->hi - k.info->lo) : v;
        return k.def->startDeg + (k.def->endDeg - k.def->startDeg) * t;
    }

    void setValue (Knob& k, float v)
    {
        if (k.param == nullptr) return;
        if (k.def->steps > 0) v = juce::jlimit (0.0f, (float) (k.def->steps - 1), std::round (v));
        else if (k.info != nullptr) v = juce::jlimit (k.info->lo, k.info->hi, v);
        k.param->setValueNotifyingHost (k.param->convertTo0to1 (v));
    }

    int knobAt (juce::Point<float> p) const
    {
        for (size_t i = 0; i < knobs.size(); ++i)
            if (p.getDistanceFrom ({ knobs[i].base.x, knobs[i].base.y - knobs[i].radius * 0.35f }) < knobs[i].radius * 1.1f) return (int) i;
        return -1;
    }

    void timerCallback() override
    {
        bool dirty = false;
        for (auto& k : knobs) { const float v = valueOf (k); if (v != k.shown) { k.shown = v; dirty = true; } }
        const float lvl = proc.inputLevel.load();
        if (std::abs (lvl - shownLevel) > 0.01f) { shownLevel = lvl; dirty = true; }
        if (dirty) repaint();
    }

    AT20VProcessor& proc;
    Assets& A = Assets::get();
    std::vector<Knob> knobs;
    int dragKnob = -1, hoverKnob = -1;
    float lastY = 0.0f, lastX = 0.0f, accum = 0.0f, shownLevel = 0.0f;
    bool toggled = false;
};

} // namespace dh
