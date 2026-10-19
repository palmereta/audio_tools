#pragma once
// Estética de Orrery: cielo de noche, un sol dorado y un color por carril. Las perillas son órbitas: el
// valor es un planeta sobre su arco. Sin mayúsculas forzadas: los rótulos son información.

#include <JuceHeader.h>

namespace dh::orr::ui {

namespace col {
    const juce::Colour bg       { 0xff0a0b16 };
    const juce::Colour surface  { 0xff111329 };
    const juce::Colour raised   { 0xff191c39 };
    const juce::Colour border   { 0xff272b51 };
    const juce::Colour text     { 0xffece8f7 };
    const juce::Colour dim      { 0xff9b97b9 };
    const juce::Colour faint    { 0xff5f5b80 };
    const juce::Colour sun      { 0xfff2c878 };
    const juce::Colour select   { 0xffa79cff };
    inline juce::Colour lane (int l)
    {
        static const juce::uint32 C[8] = { 0xff7fd1c7, 0xffa99cf0, 0xfff09cb7, 0xff8cc0f5, 0xffb8e3a0, 0xfff2a07b, 0xffd8b4f8, 0xfff5e27a };
        return juce::Colour (C[(l % 8 + 8) % 8]);
    }
}

inline juce::Font font (float h, bool bold = false) { return juce::FontOptions (h, bold ? juce::Font::bold : juce::Font::plain); }

inline juce::Rectangle<int> panel (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& title)
{
    g.setColour (col::surface); g.fillRoundedRectangle (r.toFloat(), 10.0f);
    g.setColour (col::border); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 10.0f, 1.0f);
    if (title.isEmpty()) return r.reduced (12);
    g.setColour (col::dim); g.setFont (font (13.0f, true));
    g.drawText (title, r.getX() + 14, r.getY() + 8, r.getWidth() - 28, 18, juce::Justification::centredLeft);
    return r.withTrimmedTop (32).reduced (12, 6);
}

class LookAndFeelOr : public juce::LookAndFeel_V4
{
public:
    LookAndFeelOr()
    {
        setColour (juce::PopupMenu::backgroundColourId, col::raised);
        setColour (juce::PopupMenu::textColourId, col::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::select.withAlpha (0.35f));
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour (juce::PopupMenu::headerTextColourId, col::dim);
        setColour (juce::TextButton::buttonColourId, col::raised);
        setColour (juce::TextButton::buttonOnColourId, col::select.withAlpha (0.3f));
        setColour (juce::TextButton::textColourOffId, col::text);
        setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        setColour (juce::ComboBox::backgroundColourId, col::raised);
        setColour (juce::ComboBox::outlineColourId, col::border);
        setColour (juce::ComboBox::textColourId, col::text);
        setColour (juce::ComboBox::arrowColourId, col::dim);
        setColour (juce::TextEditor::backgroundColourId, col::bg);
        setColour (juce::TextEditor::outlineColourId, col::border);
        setColour (juce::TextEditor::focusedOutlineColourId, col::select);
        setColour (juce::TextEditor::textColourId, col::text);
        setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::ScrollBar::thumbColourId, col::border);
        setColour (juce::TooltipWindow::backgroundColourId, col::raised);
        setColour (juce::TooltipWindow::textColourId, col::text);
        setColour (juce::TooltipWindow::outlineColourId, col::border);
        setColour (juce::AlertWindow::backgroundColourId, col::surface);
        setColour (juce::AlertWindow::textColourId, col::text);
        setColour (juce::AlertWindow::outlineColourId, col::border);
    }
    juce::Font getPopupMenuFont() override { return font (14.0f); }
    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return font (std::min (14.0f, (float) h * 0.48f), true); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return font (14.0f); }
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bgc, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = b.getToggleState() ? findColour (juce::TextButton::buttonOnColourId) : bgc;
        if (over) c = c.brighter (0.1f);
        if (down) c = c.darker (0.15f);
        g.setColour (c); g.fillRoundedRectangle (r, r.getHeight() / 2);
        g.setColour (b.getToggleState() ? col::select : col::border); g.drawRoundedRectangle (r, r.getHeight() / 2, 1.0f);
    }
};

} // namespace dh::orr::ui
