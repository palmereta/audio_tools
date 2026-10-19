// Audio Tools Launcher: downloads the plugins, keeps them up to date, and stays out of the way.
// No account, no telemetry: the only network requests are the manifest and the zips.

#include <juce_gui_basics/juce_gui_basics.h>
#include "MainComponent.h"

namespace atl
{
class LauncherLookAndFeel : public juce::LookAndFeel_V4
{
public:
    LauncherLookAndFeel()
    {
        setDefaultSansSerifTypefaceName (theme::family());
        setColour (juce::PopupMenu::backgroundColourId, theme::card);
        setColour (juce::PopupMenu::textColourId, theme::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, theme::card2);
        setColour (juce::PopupMenu::highlightedTextColourId, theme::ink);
        setColour (juce::ResizableWindow::backgroundColourId, theme::bg);
        setColour (juce::TooltipWindow::backgroundColourId, theme::card2);
        setColour (juce::TooltipWindow::textColourId, theme::ink);
        setColour (juce::TooltipWindow::outlineColourId, theme::hair);
    }

    juce::Font getPopupMenuFont() override   { return theme::uiFont (15.0f); }
};

class MainWindow : public juce::DocumentWindow
{
public:
    MainWindow() : juce::DocumentWindow ("Audio Tools Launcher", theme::bg, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new MainComponent(), true);
        setResizable (true, true);
        setResizeLimits (720, 520, 2400, 2000);

        const auto size = juce::StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("LAUNCHER_SIZE", {}), "x", "");
        if (size.size() == 2) getContentComponent()->setSize (size[0].getIntValue(), size[1].getIntValue());
        centreWithSize (getWidth(), getHeight());
        setVisible (true);
    }

    void closeButtonPressed() override   { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class LauncherApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override       { return "Audio Tools Launcher"; }
    const juce::String getApplicationVersion() override    { return LAUNCHER_VERSION; }
    bool moreThanOneInstanceAllowed() override             { return false; }

    void initialise (const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&laf);
        window = std::make_unique<MainWindow>();

        // Screenshots without capturing the screen: LAUNCHER_SNAPSHOT=/path.png, LAUNCHER_WAIT=seconds,
        // LAUNCHER_SETTINGS=1 (with the settings open), LAUNCHER_SIZE=WxH.
        const auto snap = juce::SystemStats::getEnvironmentVariable ("LAUNCHER_SNAPSHOT", {});
        if (snap.isNotEmpty())
        {
            const int wait = juce::SystemStats::getEnvironmentVariable ("LAUNCHER_WAIT", "3").getIntValue();
            if (juce::SystemStats::getEnvironmentVariable ("LAUNCHER_SETTINGS", "0").getIntValue() != 0)
                juce::Timer::callAfterDelay (300, [this] { if (auto* mc = dynamic_cast<MainComponent*> (window->getContentComponent())) mc->showSettings (true); });
            juce::Timer::callAfterDelay (juce::jmax (500, wait * 1000), [this, snap]
            {
                auto* c = window->getContentComponent();
                const auto img = c->createComponentSnapshot (c->getLocalBounds(), true, 2.0f);
                const juce::File out (snap);
                out.deleteFile();
                juce::FileOutputStream os (out);
                juce::PNGImageFormat png;
                const bool ok = os.openedOk() && png.writeImageToStream (img, os);
                std::fprintf (stderr, "snapshot %s: %s\n", ok ? "written" : "FAILED", out.getFullPathName().toRawUTF8());
                setApplicationReturnValue (ok ? 0 : 1);
                quit();
            });
        }
    }

    void shutdown() override
    {
        window.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override   { quit(); }

private:
    LauncherLookAndFeel laf;
    std::unique_ptr<MainWindow> window;
};
} // namespace atl

START_JUCE_APPLICATION (atl::LauncherApp)
