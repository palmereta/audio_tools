#pragma once

#include "Core.h"
#include "Media.h"
#include "Theme.h"

namespace atl
{
class MainComponent;

/** The path with the home folder shortened to ~ (macOS). */
String tildePath (const File&);

/** One plugin in the list: thumbnail, name, tagline, versions, progress and the action button.
    It also holds the plugin's state, which its page reads. */
class ProductRow : public juce::Component
{
public:
    enum class Busy { none, queued, working };

    explicit ProductRow (const Product&);

    void setState (const Detected&, Status);
    void setBusy (Busy, double progress = -1.0, const String& stage = {});
    void setMessage (const String& text, bool isError);
    void setThumb (const juce::Image& i)   { thumb = i; repaint(); }

    const Product product;
    const juce::Colour accent;
    Detected detected;
    Status status = Status::notInstalled;
    Busy busy = Busy::none;
    double progress = -1.0;
    String stage, message;
    bool messageIsError = false;

    std::function<void()> onAction, onMore, onOpen, onChanged;

    String actionLabel() const;
    bool actionEnabled() const;
    String installedLine() const;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override  { repaint(); }
    void mouseExit (const juce::MouseEvent&) override   { repaint(); }

private:
    theme::Pill action, more;
    juce::Image thumb;
    void refreshButton();
    void changed();
};

/** The top of the home view: the series title and lead, the shelf of plugin images, the summary and
    the Install all / Update all buttons. */
class HomeHero : public juce::Component
{
public:
    HomeHero();

    struct Panel { String id, name, tag; juce::Colour accent; juce::Image image; };
    juce::Array<Panel> panels;
    String title, lead, message;
    theme::Pill updateAll { "Update all", theme::Pill::Style::primary };
    theme::Pill installAll { "Install all", theme::Pill::Style::ghost };
    theme::Pill retry { "Try again", theme::Pill::Style::primary };
    std::function<void (const String&)> onOpen;

    int heightFor (int width) const;
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    int hover = -1;
    juce::Rectangle<int> shelf;
    juce::Array<juce::Rectangle<int>> panelBounds() const;
    int textBlockHeight (int width) const;
};

/** One demo clip on a plugin's page: play/stop, title, description, waveform with progress. */
class TrackView : public juce::Component
{
public:
    TrackView (MainComponent&, const Product&, int index, juce::Colour accent);
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent&) override;
    String key() const   { return product.id + "/" + sample.id; }

    bool loading = false, failed = false;

private:
    MainComponent& owner;
    const Product& product;
    const int index;
    const Sample& sample;
    juce::Colour accent;
    juce::Rectangle<float> playArea() const;
    juce::Rectangle<float> waveArea() const;
};

/** A plugin's own page: big image, texts, versions, actions, manual and the demos. */
class ProductPage : public juce::Component, private juce::Timer
{
public:
    ProductPage (MainComponent&, ProductRow&);
    ~ProductPage() override;

    void refresh();
    int heightFor (int width);
    void setHero (const juce::Image& i)   { hero = i; repaint(); }
    TrackView* track (int i) const         { return tracks[i]; }

    void paint (juce::Graphics&) override;
    void resized() override;

    ProductRow& row;

private:
    MainComponent& owner;
    juce::Image hero;
    theme::Pill back { "All plugins", theme::Pill::Style::ghost };
    theme::Pill action { "Install", theme::Pill::Style::primary };
    theme::Pill remove { "Remove", theme::Pill::Style::ghost };
    theme::Pill manual { "Read the manual", theme::Pill::Style::ghost };
    theme::Pill site { "More on the website", theme::Pill::Style::quiet };
    juce::OwnedArray<TrackView> tracks;
    int heroH = 0, nameY = 0, descH = 0, infoY = 0, buttonsY = 0, statusY = 0, listenY = 0;

    void timerCallback() override;
    void layout (int width);
};

/** Settings drawn over the window: formats and, on Windows, the all-users VST3 folder. */
class SettingsPanel : public juce::Component
{
public:
    SettingsPanel (Settings&, const Paths&);
    std::function<void()> onClose, onChanged;
    std::function<void (bool)> onSystemWideRequested;   // Windows: may need to relaunch elevated

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;
    void sync();

private:
    Settings& settings;
    const Paths& paths;
    theme::Toggle vst3 { "VST3" }, au { "Audio Unit" }, standalone { "Standalone app" }, systemWide { "Install VST3 for all users" };
    theme::Pill done { "Done", theme::Pill::Style::primary };
    int placesTop = 0;
    juce::Rectangle<int> cardArea() const;
};

//==============================================================================
class MainComponent : public juce::Component
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void showSettings (bool);
    void openProduct (const String& id);
    void showHome();

    // Used by the views.
    void enqueue (ProductRow&, bool removeOnly);
    void confirmRemove (ProductRow&);
    void togglePlay (TrackView&, const Product&, int sampleIndex);
    void seek (TrackView&, double fraction);
    Player player;
    String playingKey() const   { return player.current(); }

private:
    Paths paths;
    Settings settings;
    InstalledDb db;
    juce::CriticalSection dbLock;
    Manifest manifest;
    bool ready = false, offline = false;
    String loadError;

    juce::ThreadPool pool { juce::ThreadPoolOptions{}.withNumberOfThreads (1) };
    std::atomic<bool> quitting { false };
    AssetLoader assets { paths };

    theme::Pill donate { "Support Audio Tools", theme::Pill::Style::ghost };
    theme::Pill settingsBtn { "Settings", theme::Pill::Style::quiet };
    theme::Pill bannerBtn { "Download", theme::Pill::Style::ghost };

    juce::Viewport viewport;
    juce::Component list;
    HomeHero homeHero;
    juce::OwnedArray<ProductRow> rows;
    juce::OwnedArray<juce::Label> groupLabels;
    std::unique_ptr<ProductPage> page;
    std::unique_ptr<SettingsPanel> settingsPanel;
    int homeScrollY = 0;

    juce::Rectangle<int> headerArea, bannerArea, footerArea;

    void loadManifest();
    void manifestArrived (const String& json, const String& location, bool fromCache, const String& error);
    void buildRows();
    void layoutList();
    void refreshAll();
    void refreshRow (ProductRow&);
    void updateSummary();
    bool launcherUpdateAvailable() const;
    String donateUrl() const;
    ProductRow* rowFor (const String& id) const;

    void showMoreMenu (ProductRow&);
    void requestSystemWide (bool);
    std::vector<juce::Component::SafePointer<ProductRow>> waitingForAdmin;   // Windows: filas que esperan la respuesta al pedido de administrador
    String installHint() const;
};

} // namespace atl
