#pragma once
// Aspecto de Halation: una maqueta de arquitectura de yeso en una sala de exposición a oscuras. El marco es grafito
// cálido; los controles son de yeso, como piezas de la maqueta; la única luz es la de los ecos: tibia en los
// graves, fría en los agudos, violeta cuando todo queda suspendido. Jost (OFL) para todo el texto.

#include <JuceHeader.h>

namespace dh::sp::ui {

namespace col {
    const juce::Colour bg (0xff1f1d1b), panel (0xff282624), panel2 (0xff302e2b), line (0xff3d3a36), lineHi (0xff4a4641);
    const juce::Colour plaster (0xffe7e2d8), plasterMid (0xffc9c2b5), plasterDark (0xff8f897e), ink (0xffece6da), inkDim (0xffa59f94), inkFaint (0xff6f6a62);
    const juce::Colour warm (0xffffb066), cold (0xff8cd2ff), mist (0xfff4efe6), frozen (0xffb8a5ff), alarm (0xffff6b4a), amber (0xffffc35a);
}

class Fonts
{
public:
    static Fonts& get() { static Fonts f; return f; }
    juce::Font regular (float h) const { return make (medium, h); }
    juce::Font bold (float h) const { return make (semi, h); }
private:
    Fonts()
    {
       #if HALATION_HAS_ASSETS
        medium = juce::Typeface::createSystemTypefaceFor (BinaryData::JostMedium_ttf, BinaryData::JostMedium_ttfSize);
        semi = juce::Typeface::createSystemTypefaceFor (BinaryData::JostSemiBold_ttf, BinaryData::JostSemiBold_ttfSize);
       #endif
    }
    static juce::Font make (const juce::Typeface::Ptr& t, float h)
    {
        if (t != nullptr) return juce::Font (juce::FontOptions (t).withHeight (h));
        return juce::Font (juce::FontOptions ("Futura", h, juce::Font::plain));
    }
    juce::Typeface::Ptr medium, semi;
};
inline juce::Font regular (float h) { return Fonts::get().regular (h); }
inline juce::Font bold (float h) { return Fonts::get().bold (h); }

// Color de una luz según su calidez (1 tibia/grave … 0 fría/aguda).
inline juce::Colour lightColour (float warm) { return col::cold.interpolatedWith (col::warm, juce::jlimit (0.0f, 1.0f, warm)); }

class Look : public juce::LookAndFeel_V4
{
public:
    Look()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::panel2);
        setColour (juce::PopupMenu::textColourId, col::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::plaster);
        setColour (juce::PopupMenu::highlightedTextColourId, col::bg);
        setColour (juce::PopupMenu::headerTextColourId, col::inkDim);
        setColour (juce::ComboBox::backgroundColourId, col::panel2);
        setColour (juce::ComboBox::textColourId, col::ink);
        setColour (juce::ComboBox::outlineColourId, col::line);
        setColour (juce::ComboBox::arrowColourId, col::inkDim);
        setColour (juce::TooltipWindow::backgroundColourId, col::plaster);
        setColour (juce::TooltipWindow::textColourId, col::bg);
    }
    juce::Font getPopupMenuFont() override { return regular (16.0f); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return regular (15.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return bold (14.5f); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        const bool on = b.getToggleState();
        g.setColour (on ? col::plaster : (over || down ? col::panel2.brighter (0.06f) : col::panel2));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (on ? col::plaster.brighter (0.2f) : col::line);
        g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    }
    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setColour (b.getToggleState() ? col::bg : (b.isEnabled() ? col::ink : col::inkFaint));
        g.setFont (bold (14.5f));
        g.drawFittedText (b.getButtonText(), b.getLocalBounds().reduced (6, 0), juce::Justification::centred, 1, 0.85f);
    }
    void drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox&) override
    {
        auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
        g.setColour (col::panel2); g.fillRoundedRectangle (r, 6.0f);
        g.setColour (col::line); g.drawRoundedRectangle (r, 6.0f, 1.0f);
        juce::Path p; const float cx = (float) w - 14.0f, cy = (float) h * 0.5f;
        p.addTriangle (cx - 4.5f, cy - 2.0f, cx + 4.5f, cy - 2.0f, cx, cy + 3.5f);
        g.setColour (col::inkDim); g.fillPath (p);
    }
    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 0, box.getWidth() - 26, box.getHeight());
        label.setFont (regular (15.0f));
    }

    // Perilla de yeso: un cilindro claro con sombra hacia abajo y un trazo de grafito; la escala es un arco fino que
    // se ilumina hasta el valor.
    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto area = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float R = std::min (area.getWidth(), area.getHeight()) * 0.5f - 2.0f;
        const auto c = area.getCentre();
        const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
        juce::Path track; track.addCentredArc (c.x, c.y, R, R, 0, a0, a1, true);
        g.setColour (col::line); g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float aMid = 0.5f * (a0 + a1), a = a0 + pos * (a1 - a0);
        juce::Path val; val.addCentredArc (c.x, c.y, R, R, 0, bipolar ? aMid : a0, a, true);
        g.setColour (col::plaster); g.strokePath (val, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float kr = R * 0.70f;
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillEllipse (c.x - kr, c.y - kr + 3.0f, kr * 2, kr * 2);
        juce::ColourGradient body (col::plaster, c.x - kr * 0.4f, c.y - kr, col::plasterMid, c.x + kr * 0.5f, c.y + kr, false);
        g.setGradientFill (body); g.fillEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2);
        g.setColour (col::plasterDark.withAlpha (0.6f)); g.drawEllipse (c.x - kr, c.y - kr, kr * 2, kr * 2, 1.0f);
        juce::Path tick; tick.addRoundedRectangle (-1.6f, -kr * 0.92f, 3.2f, kr * 0.5f, 1.6f);
        g.setColour (col::bg); g.fillPath (tick, juce::AffineTransform::rotation (a).translated (c.x, c.y));
    }

    // La perilla larga Delay ↔ Space: a la izquierda pulsos separados, a la derecha niebla. El cursor es de yeso.
    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float sliderPos, float, float, juce::Slider::SliderStyle st, juce::Slider& s) override
    {
        if (! s.getProperties().getWithDefault ("space", false)) { LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, sliderPos, 0, 0, st, s); return; }
        const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
        const float cy = r.getCentreY();
        auto track = r.withHeight (14.0f).withCentre ({ r.getCentreX(), cy });
        g.setColour (col::panel2); g.fillRoundedRectangle (track, 7.0f);
        // Pulsos que se juntan y se vuelven niebla de izquierda a derecha.
        const int n = 34;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) (n - 1);
            const float px = track.getX() + 10.0f + t * (track.getWidth() - 20.0f);
            const float rad = 1.8f + 4.5f * t * t;
            const float alpha = (0.55f - 0.35f * t) * (px <= sliderPos + 1.0f ? 1.0f : 0.35f);
            g.setColour (col::mist.withAlpha (alpha));
            g.fillEllipse (px - rad, cy - rad, rad * 2, rad * 2);
        }
        g.setColour (col::line); g.drawRoundedRectangle (track, 7.0f, 1.0f);
        const float tw = 22.0f;
        auto thumb = juce::Rectangle<float> (sliderPos - tw * 0.5f, cy - 15.0f, tw, 30.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillRoundedRectangle (thumb.translated (0, 2.5f), 5.0f);
        juce::ColourGradient body (col::plaster, thumb.getX(), thumb.getY(), col::plasterMid, thumb.getRight(), thumb.getBottom(), false);
        g.setGradientFill (body); g.fillRoundedRectangle (thumb, 5.0f);
        g.setColour (col::bg); g.fillRect (juce::Rectangle<float> (sliderPos - 1.0f, thumb.getY() + 7, 2.0f, thumb.getHeight() - 14));
    }

    void drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h) override
    {
        g.fillAll (col::plaster);
        g.setColour (col::bg); g.setFont (regular (14.5f));
        g.drawFittedText (text, juce::Rectangle<int> (w, h).reduced (9, 5), juce::Justification::centredLeft, 4);
    }
};

} // namespace dh::sp::ui
