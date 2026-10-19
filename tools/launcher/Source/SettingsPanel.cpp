#include "MainComponent.h"

namespace atl
{
using namespace theme;
using juce::Graphics;
using juce::Justification;
using juce::Rectangle;

SettingsPanel::SettingsPanel (Settings& s, const Paths& p) : settings (s), paths (p)
{
    for (auto* t : { &vst3, &au, &standalone, &systemWide })
    {
        addAndMakeVisible (*t);
        t->onClick = [this, t]
        {
            if (t == &systemWide)
            {
                const bool want = systemWide.getToggleState();
                if (onSystemWideRequested) onSystemWideRequested (want);
                return;
            }
            settings.vst3 = vst3.getToggleState();
            settings.au = au.getToggleState();
            settings.standalone = standalone.getToggleState();
            if (onChanged) onChanged();
            repaint();
        };
    }
   #if JUCE_WINDOWS
    au.setVisible (false);
   #else
    systemWide.setVisible (false);
   #endif
    addAndMakeVisible (done);
    done.onClick = [this] { if (onClose) onClose(); };
    sync();
}

void SettingsPanel::sync()
{
    vst3.setToggleState (settings.vst3, juce::dontSendNotification);
    au.setToggleState (settings.au, juce::dontSendNotification);
    standalone.setToggleState (settings.standalone, juce::dontSendNotification);
    systemWide.setToggleState (settings.systemWideVst3, juce::dontSendNotification);
    repaint();
}

static StringArray installPlaces (const Paths& paths, const Settings& settings)
{
    StringArray where;
    where.add ("VST3:  " + tildePath (paths.vst3Target (settings)));
   #if JUCE_WINDOWS
    where.add ("Standalone:  " + tildePath (paths.apps) + "\\<plugin>");
   #else
    where.add ("Audio Unit:  " + tildePath (paths.au));
    where.add ("Standalone:  " + tildePath (paths.apps));
   #endif
    return where;
}

Rectangle<int> SettingsPanel::cardArea() const
{
    int toggles = 0;
    for (auto* t : { &vst3, &au, &standalone, &systemWide }) toggles += t->isVisible() ? 1 : 0;
    const int h = 30 + 40 + 10 + 20 + 10 + toggles * 34 + 26 + 20 + installPlaces (paths, settings).size() * 19 + 28 + 38 + 30;
    return getLocalBounds().withSizeKeepingCentre (juce::jmin (540, getWidth() - 40), juce::jmin (h, getHeight() - 40));
}

void SettingsPanel::mouseUp (const juce::MouseEvent& e)
{
    if (! cardArea().contains (e.getPosition()) && onClose) onClose();
}

void SettingsPanel::paint (Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.62f));
    const auto c = cardArea();
    g.setColour (bg);
    g.fillRoundedRectangle (c.toFloat(), 26.0f);
    g.setColour (hair);
    g.drawRoundedRectangle (c.toFloat().reduced (0.5f), 26.0f, 1.0f);

    auto r = c.reduced (34, 30);
    g.setColour (ink);
    g.setFont (heading (34.0f));
    g.drawText ("Settings.", r.removeFromTop (40), Justification::centredLeft);
    r.removeFromTop (10);
    g.setColour (ink3);
    g.setFont (uiFont (13.0f, true));
    g.drawText ("FORMATS TO INSTALL", r.removeFromTop (20), Justification::centredLeft);

    // Where each format goes, under the switches.
    const auto where = installPlaces (paths, settings);
    int y = placesTop;
    g.setColour (ink3);
    g.setFont (uiFont (13.0f, true));
    g.drawText ("INSTALLS TO", c.getX() + 34, y, c.getWidth() - 68, 20, Justification::centredLeft);
    y += 24;
    g.setColour (ink2);
    g.setFont (uiFont (13.0f));
    for (int i = 0; i < where.size(); ++i)
        g.drawText (where[i], c.getX() + 34, y + i * 19, c.getWidth() - 68, 18, Justification::centredLeft, true);
}

void SettingsPanel::resized()
{
    auto r = cardArea().reduced (34, 30);
    r.removeFromTop (40 + 10 + 20 + 10);
    for (auto* t : { &vst3, &au, &standalone, &systemWide })
        if (t->isVisible()) t->setBounds (r.removeFromTop (34));
    placesTop = r.getY() + 26;
    done.setBounds (r.removeFromBottom (38).removeFromRight (110));
}


} // namespace atl
