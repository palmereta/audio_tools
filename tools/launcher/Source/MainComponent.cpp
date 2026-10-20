#include "MainComponent.h"
#include "Config.h"
#include "Platform.h"

namespace atl
{
using namespace theme;
using juce::Graphics;
using juce::Justification;
using juce::Rectangle;

static constexpr int kRowH = 86, kRowGap = 10, kGroupH = 40, kSide = 28, kShelfH = 210;

String tildePath (const File& f)
{
    auto s = f.getFullPathName();
   #if ! JUCE_WINDOWS
    const auto home = File::getSpecialLocation (File::userHomeDirectory).getFullPathName();
    if (s.startsWith (home)) s = "~" + s.substring (home.length());
   #endif
    return s;
}

/** Draws an image filling the area (cropped, centred), or the plugin colour when it isn't there. */
static void drawCover (Graphics& g, const juce::Image& img, Rectangle<float> area, float radius, juce::Colour tint)
{
    juce::Graphics::ScopedSaveState s (g);
    juce::Path clip;
    clip.addRoundedRectangle (area, radius);
    g.reduceClipRegion (clip);
    if (img.isValid())
        g.drawImage (img, area, juce::RectanglePlacement::fillDestination);
    else
    {
        g.setGradientFill (juce::ColourGradient (tint.withAlpha (0.55f), area.getX(), area.getY(),
                                                 card2, area.getRight(), area.getBottom(), false));
        g.fillRect (area);
    }
}

static String formatsLine (const Product& p)
{
    StringArray f;
    for (auto& x : p.forThisOS().formats) f.add (x == "AU" ? "Audio Unit" : x == "Standalone" ? "standalone app" : x);
    return f.joinIntoString (", ");
}

//==============================================================================
ProductRow::ProductRow (const Product& p) : product (p), accent (theme::parse (p.accent))
{
    action.setAccent (accent);
    more.setDots (true);
    more.setStyle (Pill::Style::quiet);
    more.setTooltip ("More");
    addAndMakeVisible (action);
    addAndMakeVisible (more);
    action.onClick = [this] { if (onAction) onAction(); };
    more.onClick = [this] { if (onMore) onMore(); };
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    refreshButton();
}

void ProductRow::changed()
{
    refreshButton();
    repaint();
    if (onChanged) onChanged();
}

void ProductRow::setState (const Detected& d, Status s)  { detected = d; status = s; changed(); }

void ProductRow::setBusy (Busy b, double p, const String& st)
{
    busy = b;
    progress = p;
    stage = st;
    if (b != Busy::none) message.clear();
    changed();
}

void ProductRow::setMessage (const String& text, bool isError)
{
    message = text;
    messageIsError = isError;
    changed();
}

String ProductRow::actionLabel() const
{
    if (busy != Busy::none) return busy == Busy::queued ? "Queued" : "Working";
    if (status == Status::notInstalled) return "Install";
    if (status == Status::updateAvailable) return "Update";
    return "Installed";
}

bool ProductRow::actionEnabled() const   { return busy == Busy::none && status != Status::upToDate; }

String ProductRow::installedLine() const
{
    if (! detected.present)                 return "Not installed";
    if (detected.version.isEmpty())         return "Installed, version unknown";
    if (detected.viaLauncher)               return "Installed " + detected.version;
    return "Installed by hand, " + detected.version;
}

void ProductRow::refreshButton()
{
    action.setButtonText (actionLabel());
    action.setStyle (actionEnabled() ? Pill::Style::primary : Pill::Style::ghost);
    action.setEnabled (actionEnabled());
    more.setEnabled (busy == Busy::none);
}

void ProductRow::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked() && onOpen) onOpen();
}

void ProductRow::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (isMouseOverOrDragging (true) ? card2 : card);
    g.fillRoundedRectangle (r, 18.0f);

    // The thumbnail from the site's menu (16:10), or the plugin colour until it arrives.
    const auto th = Rectangle<float> (14.0f, 12.0f, (r.getHeight() - 24.0f) * 1.6f, r.getHeight() - 24.0f);
    drawCover (g, thumb, th, 11.0f, accent);

    const int right = action.getX() - 22;
    const int infoW = juce::jmin (190, juce::jmax (0, right - 360));
    const int textX = (int) th.getRight() + 18, textW = right - infoW - textX - 12;

    g.setColour (ink);
    g.setFont (heading (27.0f));
    g.drawText (product.name, textX, 15, textW, 30, Justification::centredLeft, true);

    const bool showMsg = message.isNotEmpty();
    g.setColour (showMsg ? (messageIsError ? Colour (0xffe8743b) : ink) : ink2);
    g.setFont (uiFont (14.5f));
    g.drawFittedText (showMsg ? message : product.tagline, textX, 47, textW, 22, Justification::centredLeft, 1, 0.9f);

    // Versions or progress.
    if (infoW < 90) return;
    const int ix = right - infoW;
    if (busy == Busy::working)
    {
        auto track = Rectangle<float> ((float) ix, 40.0f, (float) infoW, 4.0f);
        g.setColour (ink.withAlpha (0.12f));
        g.fillRoundedRectangle (track, 2.0f);
        if (progress >= 0.0)
        {
            g.setColour (accent);
            g.fillRoundedRectangle (track.withWidth (juce::jmax (4.0f, track.getWidth() * (float) progress)), 2.0f);
        }
        g.setColour (ink2);
        g.setFont (uiFont (13.0f));
        g.drawText (stage + (progress >= 0.0 && stage.startsWith ("Downloading") ? "  " + String (juce::roundToInt (progress * 100.0)) + " %" : String()),
                    ix, 48, infoW, 18, Justification::centredRight, true);
        return;
    }

    g.setColour (ink);
    g.setFont (uiFont (14.5f, true));
    g.drawText ("Latest " + product.version, ix, 22, infoW, 20, Justification::centredRight, true);
    g.setColour (status == Status::updateAvailable ? accent : ink3);
    g.setFont (uiFont (13.0f));
    g.drawText (installedLine(), ix, 44, infoW, 18, Justification::centredRight, true);
}

void ProductRow::resized()
{
    auto r = getLocalBounds().reduced (18, 0);
    more.setBounds (r.removeFromRight (34).withSizeKeepingCentre (34, 34));
    r.removeFromRight (6);
    action.setBounds (r.removeFromRight (112).withSizeKeepingCentre (112, 36));
}

//==============================================================================
HomeHero::HomeHero()
{
    for (auto* b : { &updateAll, &installAll, &retry }) addChildComponent (*b);
}

int HomeHero::textBlockHeight (int width) const
{
    juce::AttributedString t;
    t.append (title, heading (44.0f), ink);
    juce::TextLayout tl;
    tl.createLayout (t, (float) juce::jmin (width, 820));
    juce::AttributedString l;
    l.append (lead, uiFont (16.5f), ink2);
    juce::TextLayout ll;
    ll.createLayout (l, (float) juce::jmin (width, 720));
    return (int) std::ceil (tl.getHeight()) + 12 + (lead.isEmpty() ? 0 : (int) std::ceil (ll.getHeight()));
}

int HomeHero::heightFor (int width) const
{
    const int inner = width - 2 * kSide;
    return 26 + textBlockHeight (inner) + 26 + (panels.isEmpty() ? 0 : kShelfH + 22) + 44 + 16;
}

juce::Array<Rectangle<int>> HomeHero::panelBounds() const
{
    // The site's shelf: one panel opens wide (the one under the mouse, or the first), the rest stay narrow.
    juce::Array<Rectangle<int>> out;
    if (panels.isEmpty() || shelf.isEmpty()) return out;
    const int gap = 8, n = panels.size(), open = hover >= 0 ? hover : 0;
    const float unit = (float) (shelf.getWidth() - gap * (n - 1)) / (float) (n - 1 + 5);
    float x = (float) shelf.getX();
    for (int i = 0; i < n; ++i)
    {
        const float w = unit * (i == open ? 5.0f : 1.0f);
        out.add (Rectangle<float> (x, (float) shelf.getY(), w, (float) shelf.getHeight()).toNearestInt());
        x += w + (float) gap;
    }
    return out;
}

void HomeHero::paint (Graphics& g)
{
    auto r = getLocalBounds().reduced (kSide, 0).withTrimmedTop (26);
    const int inner = r.getWidth();

    juce::AttributedString t;
    t.append (title, heading (44.0f), ink);
    juce::TextLayout tl;
    tl.createLayout (t, (float) juce::jmin (inner, 820));
    tl.draw (g, r.toFloat().withHeight (tl.getHeight()));
    r.removeFromTop ((int) std::ceil (tl.getHeight()) + 12);
    if (lead.isNotEmpty())
    {
        juce::AttributedString l;
        l.append (lead, uiFont (16.5f), ink2);
        juce::TextLayout ll;
        ll.createLayout (l, (float) juce::jmin (inner, 720));
        ll.draw (g, r.toFloat().withHeight (ll.getHeight()));
    }

    const auto bounds = panelBounds();
    const int open = hover >= 0 ? hover : 0;
    for (int i = 0; i < bounds.size(); ++i)
    {
        auto b = bounds[i].toFloat();
        const auto& p = panels.getReference (i);
        drawCover (g, p.image, b, 16.0f, p.accent);
        juce::Graphics::ScopedSaveState s (g);
        juce::Path clip;
        clip.addRoundedRectangle (b, 16.0f);
        g.reduceClipRegion (clip);
        g.setGradientFill (juce::ColourGradient (juce::Colours::black.withAlpha (0.0f), 0, b.getY() + b.getHeight() * 0.45f,
                                                 juce::Colours::black.withAlpha (0.78f), 0, b.getBottom(), false));
        g.fillRect (b);
        g.setColour (p.accent);
        g.fillRect (b.removeFromBottom (3.0f));
        if (i == open)
        {
            // La frase en una línea si entra; si no (ventana angosta), en dos, y el nombre sube.
            auto tb = bounds[i].reduced (18, 14);
            const auto tagFont = uiFont (14.0f);
            const bool twoLines = juce::GlyphArrangement::getStringWidth (tagFont, p.tag) > (float) tb.getWidth();
            auto tagArea = tb.removeFromBottom (twoLines ? 38 : 20);
            g.setColour (ink);
            g.setFont (heading (30.0f));
            g.drawText (p.name, tb.removeFromBottom (34 + 4).removeFromTop (34), Justification::bottomLeft, true);
            g.setColour (ink.withAlpha (0.85f));
            g.setFont (tagFont);
            g.drawFittedText (p.tag, tagArea, Justification::bottomLeft, twoLines ? 2 : 1, 1.0f);
        }
    }

    // Summary under the shelf, buttons on the right.
    g.setColour (ink2);
    g.setFont (uiFont (16.0f));
    const int y = getHeight() - 16 - 44;
    int buttonsLeft = getWidth() - kSide;
    for (auto* b : { &updateAll, &installAll, &retry })
        if (b->isVisible()) buttonsLeft = juce::jmin (buttonsLeft, b->getX());
    g.drawFittedText (message, kSide, y, buttonsLeft - kSide - 16, 44, Justification::centredLeft, 2, 0.9f);
}

void HomeHero::resized()
{
    const int inner = getWidth() - 2 * kSide;
    shelf = panels.isEmpty() ? Rectangle<int>()
                             : Rectangle<int> (kSide, 26 + textBlockHeight (inner) + 26, inner, kShelfH);
    auto btns = Rectangle<int> (kSide, getHeight() - 16 - 44, inner, 44).withSizeKeepingCentre (inner, 38);
    for (auto* b : { &updateAll, &installAll, &retry })
        if (b->isVisible())
        {
            const int w = b->idealWidth() + 6;
            b->setBounds (btns.removeFromRight (w));
            btns.removeFromRight (10);
        }
}

void HomeHero::mouseMove (const juce::MouseEvent& e)
{
    int h = -1;
    const auto b = panelBounds();
    for (int i = 0; i < b.size(); ++i)
        if (b[i].contains (e.getPosition())) h = i;
    setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    if (h >= 0 && h != hover) { hover = h; repaint(); }
}

void HomeHero::mouseExit (const juce::MouseEvent&)   {}

void HomeHero::mouseUp (const juce::MouseEvent& e)
{
    const auto b = panelBounds();
    for (int i = 0; i < b.size(); ++i)
        if (b[i].contains (e.getPosition()) && e.mouseWasClicked() && onOpen) onOpen (panels[i].id);
}

//==============================================================================
TrackView::TrackView (MainComponent& o, const Product& p, int i, juce::Colour a)
    : owner (o), product (p), index (i), sample (p.samples.getReference (i)), accent (a)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> TrackView::playArea() const   { return { 16.0f, (float) getHeight() * 0.5f - 20.0f, 40.0f, 40.0f }; }

juce::Rectangle<float> TrackView::waveArea() const
{
    const float w = juce::jlimit (120.0f, 380.0f, (float) getWidth() * 0.38f);
    return { (float) getWidth() - 70.0f - w, 16.0f, w, (float) getHeight() - 32.0f };
}

void TrackView::paint (Graphics& g)
{
    const bool playing = owner.playingKey() == key();
    g.setColour (card);
    g.fillRoundedRectangle (getLocalBounds().toFloat(), 16.0f);

    // Play / stop.
    const auto pa = playArea();
    g.setColour (failed ? ink3.withAlpha (0.4f) : accent);
    g.fillEllipse (pa);
    g.setColour (inkOn (accent));
    if (loading)
    {
        juce::Path arc;
        const float t = (float) (juce::Time::getMillisecondCounter() % 1000) / 1000.0f * juce::MathConstants<float>::twoPi;
        arc.addCentredArc (pa.getCentreX(), pa.getCentreY(), 9.0f, 9.0f, t, 0.0f, 4.2f, true);
        g.strokePath (arc, juce::PathStrokeType (2.2f));
    }
    else if (playing)
        g.fillRect (pa.withSizeKeepingCentre (12.0f, 12.0f));
    else
    {
        juce::Path tri;
        const auto c = pa.getCentre();
        tri.addTriangle (c.x - 4.5f, c.y - 7.0f, c.x - 4.5f, c.y + 7.0f, c.x + 7.5f, c.y);
        g.fillPath (tri);
    }

    // Title and description.
    const auto wa = waveArea();
    const int tx = (int) pa.getRight() + 16, tw = (int) wa.getX() - tx - 20;
    g.setColour (ink);
    g.setFont (uiFont (15.5f, true));
    g.drawText (sample.title, tx, 12, tw, 22, Justification::centredLeft, true);
    g.setColour (ink2);
    g.setFont (uiFont (13.5f));
    g.drawFittedText (failed ? String ("Could not load this clip.") : sample.description, tx, 34, tw, 20, Justification::centredLeft, 1, 0.9f);

    // Waveform, filled up to the play position, as on the site.
    const int n = sample.peaks.size();
    if (n > 0)
    {
        const float pos = playing ? (float) owner.player.position() : 0.0f;
        const float bw = wa.getWidth() / (float) n;
        for (int i = 0; i < n; ++i)
        {
            const float h = juce::jmax (0.06f, std::pow (sample.peaks[i], 0.7f)) * wa.getHeight();
            const auto bar = Rectangle<float> (wa.getX() + (float) i * bw, wa.getCentreY() - h * 0.5f, juce::jmax (1.0f, bw - 1.2f), h);
            g.setColour ((float) i / (float) n < pos ? accent : ink.withAlpha (0.28f));
            g.fillRoundedRectangle (bar, juce::jmin (1.0f, bw * 0.3f));
        }
    }

    const auto secs = playing ? owner.player.position() * sample.seconds : sample.seconds;
    g.setColour (ink3);
    g.setFont (uiFont (13.0f));
    g.drawText (String ((int) secs / 60) + ":" + String ((int) secs % 60).paddedLeft ('0', 2),
                (int) wa.getRight() + 8, 0, 50, getHeight(), Justification::centredRight);
}

void TrackView::mouseUp (const juce::MouseEvent& e)
{
    if (! e.mouseWasClicked() || failed) return;
    const auto wa = waveArea();
    if (wa.contains (e.position) && owner.playingKey() == key())
        owner.seek (*this, (e.position.x - wa.getX()) / wa.getWidth());
    else
        owner.togglePlay (*this, product, index);
}

//==============================================================================
ProductPage::ProductPage (MainComponent& o, ProductRow& r) : row (r), owner (o)
{
    for (auto* b : { &back, &action, &remove, &manual, &site }) addAndMakeVisible (*b);
    action.setAccent (row.accent);
    back.setFontHeight (13.5f);
    back.onClick = [this] { juce::MessageManager::callAsync ([&o = owner] { o.showHome(); }); };
    action.onClick = [this] { owner.enqueue (row, false); };
    remove.onClick = [this] { owner.confirmRemove (row); };
    manual.onClick = [this] { juce::URL (row.product.manualUrl).launchInDefaultBrowser(); };
    site.onClick = [this] { juce::URL (row.product.pageUrl).launchInDefaultBrowser(); };
    manual.setVisible (row.product.manualUrl.isNotEmpty());
    site.setVisible (row.product.pageUrl.isNotEmpty());

    for (int i = 0; i < row.product.samples.size(); ++i)
        addAndMakeVisible (tracks.add (new TrackView (owner, row.product, i, row.accent)));
    refresh();
    startTimerHz (30);
}

ProductPage::~ProductPage()   { stopTimer(); }

void ProductPage::refresh()
{
    action.setButtonText (row.actionLabel());
    action.setStyle (row.actionEnabled() ? Pill::Style::primary : Pill::Style::ghost);
    action.setEnabled (row.actionEnabled());
    remove.setVisible (row.detected.present && row.busy == ProductRow::Busy::none);
    resized();
    repaint();
}

void ProductPage::timerCallback()
{
    for (auto* t : tracks)
        if (t->loading || owner.playingKey() == t->key() || t->getProperties()["wasPlaying"])
        {
            t->getProperties().set ("wasPlaying", owner.playingKey() == t->key());
            t->repaint();
        }
}

void ProductPage::layout (int width)
{
    const int inner = juce::jmin (width - 2 * kSide, 760);
    heroH = juce::jlimit (220, 430, (int) ((float) width * 0.44f));
    nameY = heroH - 46;
    juce::AttributedString d;
    d.append (row.product.description, uiFont (17.0f), ink2);
    juce::TextLayout tl;
    tl.createLayout (d, (float) inner);
    descH = row.product.description.isEmpty() ? 0 : (int) std::ceil (tl.getHeight());
    infoY = nameY + 84 + 38 + descH + 18;
    buttonsY = infoY + 30;
    statusY = buttonsY + 52;
    listenY = statusY + 36;
}

int ProductPage::heightFor (int width)
{
    layout (width);
    return listenY + (tracks.isEmpty() ? 0 : 60 + tracks.size() * (70 + 8)) + 40;
}

void ProductPage::resized()
{
    layout (getWidth());
    back.setBounds (kSide, 18, back.idealWidth(), 32);
    int x = kSide;
    for (auto* b : { &action, &remove, &manual, &site })
        if (b->isVisible())
        {
            const int w = b == &action ? 130 : b->idealWidth() + 4;
            b->setBounds (x, buttonsY, w, 40);
            x += w + 10;
        }
    int y = listenY + 60;
    for (auto* t : tracks)
    {
        t->setBounds (kSide, y, getWidth() - 2 * kSide, 70);
        y += 78;
    }
}

void ProductPage::paint (Graphics& g)
{
    const auto& p = row.product;
    // Big image fading into the page, as the site's product heroes.
    const auto heroArea = Rectangle<float> (0, 0, (float) getWidth(), (float) heroH);
    drawCover (g, hero, heroArea, 0.0f, row.accent);
    g.setGradientFill (juce::ColourGradient (bg.withAlpha (0.0f), 0, heroArea.getHeight() * 0.35f, bg, 0, heroArea.getBottom(), false));
    g.fillRect (heroArea);
    // A dark backing so the back button reads on any image.
    g.setColour (deep.withAlpha (0.55f));
    g.fillRoundedRectangle (back.getBounds().toFloat(), 16.0f);

    g.setColour (ink);
    g.setFont (heading (80.0f));
    g.drawText (p.name, kSide, nameY, getWidth() - 2 * kSide, 84, Justification::bottomLeft, true);
    g.setFont (uiFont (24.0f, true));
    g.drawText (p.tagline, kSide, nameY + 88, getWidth() - 2 * kSide, 32, Justification::centredLeft, true);

    if (descH > 0)
    {
        juce::AttributedString d;
        d.append (p.description, uiFont (17.0f), ink2);
        juce::TextLayout tl;
        tl.createLayout (d, (float) juce::jmin (getWidth() - 2 * kSide, 760));
        tl.draw (g, Rectangle<float> ((float) kSide, (float) (nameY + 84 + 38), tl.getWidth(), tl.getHeight()));
    }

    // Versions and formats.
    {
        const String dot (juce::CharPointer_UTF8 ("   \xc2\xb7   "));
        g.setFont (uiFont (14.5f));
        g.setColour (ink);
        String line = "Latest " + p.version + dot + row.installedLine();
        if (auto f = formatsLine (p); f.isNotEmpty()) line += dot + f;
        g.drawText (line, kSide, infoY, getWidth() - 2 * kSide, 20, Justification::centredLeft, true);
    }

    // Progress or the last result.
    if (row.busy == ProductRow::Busy::working)
    {
        auto track = Rectangle<float> ((float) kSide, (float) statusY + 8.0f, 260.0f, 4.0f);
        g.setColour (ink.withAlpha (0.12f));
        g.fillRoundedRectangle (track, 2.0f);
        if (row.progress >= 0.0)
        {
            g.setColour (row.accent);
            g.fillRoundedRectangle (track.withWidth (juce::jmax (4.0f, track.getWidth() * (float) row.progress)), 2.0f);
        }
        g.setColour (ink2);
        g.setFont (uiFont (13.5f));
        g.drawText (row.stage, kSide + 276, statusY, 300, 20, Justification::centredLeft, true);
    }
    else if (row.message.isNotEmpty())
    {
        g.setColour (row.messageIsError ? Colour (0xffe8743b) : ink2);
        g.setFont (uiFont (14.0f));
        g.drawText (row.message, kSide, statusY, getWidth() - 2 * kSide, 20, Justification::centredLeft, true);
    }

    if (! tracks.isEmpty())
    {
        g.setColour (hair);
        g.fillRect (kSide, listenY, getWidth() - 2 * kSide, 1);
        g.setColour (ink);
        g.setFont (heading (40.0f));
        g.drawText ("Listen.", kSide, listenY + 12, 400, 44, Justification::centredLeft);
    }
}

//==============================================================================
MainComponent::MainComponent()
{
    paths = Paths::current();
    settings.load (paths.settingsFile());
   #if ! JUCE_WINDOWS
    settings.systemWideVst3 = false;
   #endif
    db.load (paths.recordFile());
    // Snapshots and tests stay silent: LAUNCHER_MUTE=1 (on by default with LAUNCHER_SNAPSHOT).
    player.setMuted (juce::SystemStats::getEnvironmentVariable ("LAUNCHER_MUTE",
                         juce::SystemStats::getEnvironmentVariable ("LAUNCHER_SNAPSHOT", {}).isNotEmpty() ? "1" : "0").getIntValue() != 0);

    addAndMakeVisible (donate);
    addAndMakeVisible (settingsBtn);
    addChildComponent (bannerBtn);
    donate.setFontHeight (13.5f);
    settingsBtn.setFontHeight (13.5f);
    bannerBtn.setFontHeight (13.0f);

    donate.onClick = [this] { if (auto u = donateUrl(); u.isNotEmpty()) juce::URL (u).launchInDefaultBrowser(); };
    settingsBtn.onClick = [this] { showSettings (true); };
    bannerBtn.onClick = [this]
    {
        const auto& a = manifest.launcherForThisOS();
        resolveAssetUrl (a.url, manifest.location).launchInDefaultBrowser();
    };

    homeHero.title = "Every Audio Tools plugin, in one place.";
    homeHero.message = "Checking for the latest versions...";
    homeHero.retry.onClick = [this] { loadError.clear(); homeHero.retry.setVisible (false); homeHero.message = "Checking for the latest versions..."; layoutList(); loadManifest(); };
    homeHero.updateAll.onClick = [this] { for (auto* r : rows) if (r->busy == ProductRow::Busy::none && r->status == Status::updateAvailable) enqueue (*r, false); };
    homeHero.installAll.onClick = [this] { for (auto* r : rows) if (r->busy == ProductRow::Busy::none && r->status == Status::notInstalled) enqueue (*r, false); };
    homeHero.onOpen = [this] (const String& id) { openProduct (id); };
    list.addAndMakeVisible (homeHero);

    viewport.setViewedComponent (&list, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (8);
    viewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, ink.withAlpha (0.25f));
    addAndMakeVisible (viewport);

    donate.setVisible (donateUrl().isNotEmpty());
    setSize (960, 720);
    loadManifest();
}

MainComponent::~MainComponent()
{
    quitting = true;
    player.stop();
    pool.removeAllJobs (true, 15000);
}

String MainComponent::donateUrl() const
{
    return manifest.donateUrl.isNotEmpty() ? manifest.donateUrl : String (config::kDonateUrl);
}

bool MainComponent::launcherUpdateAvailable() const
{
    return ready && manifest.launcherForThisOS().url.isNotEmpty()
        && compareVersions (manifest.launcherVersion, LAUNCHER_VERSION) > 0;
}

ProductRow* MainComponent::rowFor (const String& id) const
{
    for (auto* r : rows)
        if (r->product.id == id) return r;
    return nullptr;
}

//==============================================================================
void MainComponent::loadManifest()
{
    const auto local = juce::SystemStats::getEnvironmentVariable ("LAUNCHER_MANIFEST", {});
    const auto cache = paths.cacheFile();
    juce::Component::SafePointer<MainComponent> safe (this);

    pool.addJob ([safe, local, cache]
    {
        String json, location, error;
        if (local.isNotEmpty())
        {
            const File f = local.startsWithIgnoreCase ("file://") ? juce::URL (local).getLocalFile() : File (local);
            json = f.loadFileAsString();
            location = f.getFullPathName();
            if (json.isEmpty()) error = "Could not read " + location + ".";
        }
        else
        {
            int status = 0;
            location = config::kManifestUrl;
            if (auto in = juce::URL (location).createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                                                       .withConnectionTimeoutMs (15000)
                                                                       .withNumRedirectsToFollow (5)
                                                                       .withExtraHeaders ("Cache-Control: no-cache")
                                                                       .withStatusCode (&status)))
                if (status == 0 || status == 200)
                    json = in->readEntireStreamAsString();
            if (json.isEmpty()) error = "Could not reach audiotools.com.ar.";
        }

        bool fromCache = false;
        if (json.isEmpty() && local.isEmpty() && cache.existsAsFile())
        {
            json = cache.loadFileAsString();
            fromCache = json.isNotEmpty();
        }
        juce::MessageManager::callAsync ([safe, json, location, fromCache, error]
        {
            if (safe != nullptr) safe->manifestArrived (json, location, fromCache, error);
        });
    });
}

void MainComponent::manifestArrived (const String& json, const String& location, bool fromCache, const String& error)
{
    Manifest m;
    auto res = json.isNotEmpty() ? Manifest::parse (json, location, m) : juce::Result::fail (error);
    if (res.failed())
    {
        loadError = res.getErrorMessage() + " Check your internet connection and try again.";
        homeHero.message = loadError;
        homeHero.retry.setVisible (true);
        layoutList();
        repaint();
        return;
    }
    if (! fromCache && juce::SystemStats::getEnvironmentVariable ("LAUNCHER_MANIFEST", {}).isEmpty())
    {
        paths.cacheFile().getParentDirectory().createDirectory();
        paths.cacheFile().replaceWithText (json);
    }
    manifest = std::move (m);
    assets.setSource (manifest.location, manifest.siteUrl);
    offline = fromCache;
    ready = true;
    loadError.clear();
    homeHero.retry.setVisible (false);
    if (manifest.homeTitle.isNotEmpty()) homeHero.title = manifest.homeTitle;
    homeHero.lead = manifest.homeLead;
    donate.setVisible (donateUrl().isNotEmpty());
    bannerBtn.setVisible (launcherUpdateAvailable());
    buildRows();
    refreshAll();
    resized();
    repaint();

    // For snapshots and checks of the whole path: LAUNCHER_INSTALL=<id>[,<id>...] presses Install on those rows,
    // LAUNCHER_PAGE=<id> opens a plugin's page and LAUNCHER_PLAY=<n> plays its clip n (muted in snapshots).
    for (auto& id : StringArray::fromTokens (juce::SystemStats::getEnvironmentVariable ("LAUNCHER_INSTALL", {}), ",", ""))
        if (auto* row = rowFor (id.trim())) enqueue (*row, false);
    if (auto id = juce::SystemStats::getEnvironmentVariable ("LAUNCHER_PAGE", {}); id.isNotEmpty())
    {
        openProduct (id);
        const auto n = juce::SystemStats::getEnvironmentVariable ("LAUNCHER_PLAY", {});
        if (page != nullptr && n.isNotEmpty())
            if (auto* t = page->track (n.getIntValue()))
                togglePlay (*t, page->row.product, n.getIntValue());
    }
    if (auto y = juce::SystemStats::getEnvironmentVariable ("LAUNCHER_SCROLL", {}); y.isNotEmpty())   // LAUNCHER_SCROLL=<px>
        viewport.setViewPosition (0, y.getIntValue());
}

void MainComponent::buildRows()
{
    if (page != nullptr) showHome();
    rows.clear();
    groupLabels.clear();
    homeHero.panels.clear();

    String lastGroup;
    for (auto& p : manifest.products)
    {
        if (p.category != lastGroup)
        {
            lastGroup = p.category;
            auto* l = groupLabels.add (new juce::Label ({}, p.category == "synths" ? "Synthesizers"
                                                           : p.category == "tools" ? "Tools and effects" : p.category));
            l->setFont (uiFont (13.0f, true));
            l->setColour (juce::Label::textColourId, ink3);
            l->getProperties().set ("before", rows.size());
            list.addAndMakeVisible (l);
        }
        auto* row = rows.add (new ProductRow (p));
        row->onAction = [this, row] { enqueue (*row, false); };
        row->onMore = [this, row] { showMoreMenu (*row); };
        row->onOpen = [this, row] { openProduct (row->product.id); };
        row->onChanged = [this, row] { if (page != nullptr && &page->row == row) page->refresh(); };
        list.addAndMakeVisible (row);

        juce::Component::SafePointer<ProductRow> safeRow (row);
        assets.image (p.imageThumb, [safeRow] (juce::Image img) { if (safeRow != nullptr && img.isValid()) safeRow->setThumb (img); });

        const int idx = homeHero.panels.size();
        homeHero.panels.add ({ p.id, p.name, p.tagline, theme::parse (p.accent), {} });
        juce::Component::SafePointer<HomeHero> safeHero (&homeHero);
        assets.image (p.imageCard.isNotEmpty() ? p.imageCard : p.imageHero, [safeHero, idx] (juce::Image img)
        {
            if (safeHero != nullptr && img.isValid() && idx < safeHero->panels.size())
            {
                safeHero->panels.getReference (idx).image = img;
                safeHero->repaint();
            }
        });
    }
    layoutList();
    // Instalaciones pedidas al relanzar como administrador (--install id,id): se ponen en cola una vez.
    if (! pendingInstalls.isEmpty())
    {
        const auto ids = pendingInstalls; pendingInstalls.clear();
        for (auto* r : rows) if (ids.contains (r->product.id)) enqueue (*r, false);
    }
}

void MainComponent::layoutList()
{
    const int w = viewport.getMaximumVisibleWidth() > 0 ? viewport.getMaximumVisibleWidth() : viewport.getWidth();
    if (w <= 0) return;
    if (page != nullptr)
    {
        page->setSize (w, page->heightFor (w));
        return;
    }
    const int heroH = homeHero.heightFor (w);
    homeHero.setBounds (0, 0, w, heroH);
    int y = heroH + 6, gi = 0;
    for (int i = 0; i < rows.size(); ++i)
    {
        while (gi < groupLabels.size() && (int) groupLabels[gi]->getProperties()["before"] == i)
        {
            groupLabels[gi]->setBounds (kSide - 4, y + (gi == 0 ? 0 : 10), w - 2 * kSide, kGroupH - 10);
            y += kGroupH + (gi == 0 ? 0 : 10);
            ++gi;
        }
        rows[i]->setBounds (kSide, y, w - 2 * kSide, kRowH);
        y += kRowH + kRowGap;
    }
    list.setSize (w, y + 16);
}

void MainComponent::openProduct (const String& id)
{
    auto* row = rowFor (id);
    if (row == nullptr) return;
    if (page == nullptr) homeScrollY = viewport.getViewPositionY();
    player.stop();
    page = std::make_unique<ProductPage> (*this, *row);
    juce::Component::SafePointer<ProductPage> safe (page.get());
    assets.image (row->product.imageHero, [safe] (juce::Image img) { if (safe != nullptr && img.isValid()) safe->setHero (img); });
    viewport.setViewedComponent (page.get(), false);
    layoutList();
    viewport.setViewPosition (0, 0);
}

void MainComponent::showHome()
{
    player.stop();
    viewport.setViewedComponent (&list, false);
    page.reset();
    layoutList();
    viewport.setViewPosition (0, homeScrollY);
}

void MainComponent::togglePlay (TrackView& t, const Product& p, int index)
{
    if (playingKey() == t.key()) { player.stop(); t.repaint(); return; }
    if (index < 0 || index >= p.samples.size()) return;
    player.stop();
    t.loading = true;
    t.repaint();
    juce::Component::SafePointer<TrackView> safe (&t);
    const auto key = t.key();
    assets.file (p.samples.getReference (index).url, [this, safe, key] (File f)
    {
        if (safe == nullptr) return;
        safe->loading = false;
        safe->failed = f == File() || ! player.play (f, key);
        if (page != nullptr) page->repaint();
    });
}

void MainComponent::seek (TrackView& t, double fraction)
{
    player.seek (fraction);
    t.repaint();
}

void MainComponent::refreshRow (ProductRow& row)
{
    const juce::ScopedLock sl (dbLock);
    const auto d = detect (row.product, db, paths);
    row.setState (d, statusOf (row.product, d));
}

void MainComponent::refreshAll()
{
    for (auto* r : rows) refreshRow (*r);
    updateSummary();
}

void MainComponent::updateSummary()
{
    int installed = 0, updates = 0, missing = 0;
    for (auto* r : rows)
    {
        if (r->status != Status::notInstalled) ++installed;
        if (r->status == Status::updateAvailable && r->busy == ProductRow::Busy::none) ++updates;
        if (r->status == Status::notInstalled && r->busy == ProductRow::Busy::none) ++missing;
    }
    String summary = String (installed) + " of " + String (rows.size()) + " plugins installed."
                   + (updates > 0 ? " " + String (updates) + (updates == 1 ? " update available." : " updates available.") : String());
    if (installed == rows.size() && updates == 0) summary = "Everything is installed and up to date.";
    if (offline) summary = "Offline: showing the last list this launcher saw. " + summary;
    homeHero.message = summary;
    homeHero.updateAll.setButtonText (updates > 1 ? "Update all (" + String (updates) + ")" : "Update all");
    homeHero.updateAll.setVisible (updates > 0);
    homeHero.installAll.setStyle (updates > 0 ? Pill::Style::ghost : Pill::Style::primary);
    homeHero.installAll.setVisible (missing > 0);
    homeHero.resized();
    homeHero.repaint();
}

//==============================================================================
void MainComponent::enqueue (ProductRow& row, bool removeOnly)
{
   #if JUCE_WINDOWS
    // VST3 va por defecto a Common Files (la carpeta que escanean todos los DAW), que pide administrador. Sin
    // permiso: reabrir como administrador, o instalar sólo para este usuario (puede que el DAW no la escanee).
    if (! removeOnly && settings.vst3 && settings.systemWideVst3 && ! platform::canWrite (paths.vst3System))
    {
        // Un solo diálogo aunque se pida "Install all": las filas esperan y la respuesta vale para todas.
        waitingForAdmin.push_back (juce::Component::SafePointer<ProductRow> (&row));
        if (waitingForAdmin.size() > 1) return;
        juce::Component::SafePointer<MainComponent> safe (this);
        juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                               .withIconType (juce::MessageBoxIconType::InfoIcon)
                                               .withTitle ("Administrator rights needed")
                                               .withMessage ("VST3 plugins go to " + paths.vst3System.getFullPathName()
                                                             + ", the folder every DAW scans. Writing there needs administrator rights.\n\n"
                                                             "Open the launcher again as administrator, or install for this user only in "
                                                             + paths.vst3User.getFullPathName() + " (some DAWs, such as Ableton Live, may need that folder added by hand).")
                                               .withButton ("Relaunch as administrator")
                                               .withButton ("This user only")
                                               .withButton ("Cancel")
                                               .withAssociatedComponent (this),
                                           [safe] (int result)
                                           {
                                               if (safe == nullptr) return;
                                               auto rows = std::move (safe->waitingForAdmin);
                                               safe->waitingForAdmin.clear();
                                               if (result == 0)
                                               {
                                                   // El launcher elevado sigue con lo que estaba en cola (antes había que volver a pedirlo).
                                                   juce::StringArray ids;
                                                   for (auto& r : rows) if (r != nullptr) ids.add (r->product.id);
                                                   if (platform::relaunchElevated ("--install " + ids.joinIntoString (","))) juce::JUCEApplicationBase::quit();
                                                   return;
                                               }
                                               if (result == 1)
                                               {
                                                   safe->settings.systemWideVst3 = false;
                                                   safe->settings.save (safe->paths.settingsFile());
                                                   if (safe->settingsPanel) safe->settingsPanel->sync();
                                                   for (auto& r : rows) if (r != nullptr) safe->enqueue (*r, false);
                                               }
                                           });
        return;
    }
   #endif
    row.setBusy (ProductRow::Busy::queued);
    updateSummary();

    const auto product = row.product;
    const auto m = manifest;
    const auto s = settings;
    juce::Component::SafePointer<MainComponent> safe (this);
    juce::Component::SafePointer<ProductRow> safeRow (&row);

    pool.addJob ([this, safe, safeRow, product, m, s, removeOnly]
    {
        if (quitting) return;
        auto post = [safe, safeRow] (std::function<void (MainComponent&, ProductRow&)> fn)
        {
            juce::MessageManager::callAsync ([safe, safeRow, fn]
            {
                if (safe != nullptr && safeRow != nullptr) fn (*safe, *safeRow);
            });
        };
        post ([removeOnly] (MainComponent&, ProductRow& r) { r.setBusy (ProductRow::Busy::working, removeOnly ? -1.0 : 0.0, removeOnly ? "Removing" : "Downloading"); });

        StringArray notes;
        juce::Result res = juce::Result::ok();
        if (removeOnly)
        {
            const juce::ScopedLock sl (dbLock);
            res = remove (product, paths, db);
        }
        else
        {
            int lastPct = -1;
            const auto progress = [&] (double f)
            {
                const int pct = f < 0 ? -1 : juce::roundToInt (f * 100.0);
                if (pct != lastPct)
                {
                    lastPct = pct;
                    const String st = f < 0.9 ? "Downloading" : f < 0.95 ? "Checking" : "Installing";
                    post ([f, st] (MainComponent&, ProductRow& r) { r.setBusy (ProductRow::Busy::working, f < 0.9 ? f : 1.0, st); });
                }
                return ! quitting.load();
            };
            // The download runs without the lock; only the install touches the record.
            const auto& a = product.forThisOS();
            const auto zip = paths.workDir().getChildFile (product.id + "-" + product.version + ".zip");
            if (! a.isValid())
                res = juce::Result::fail ("Not available for this system yet.");
            else
                res = download (resolveAssetUrl (a.url, m.location, m.siteUrl), zip, a.size, [&] (double f) { return progress (f < 0 ? -1.0 : f * 0.9); });
            if (res.wasOk()) { progress (0.92); res = verify (zip, a.sha256, a.size); }
            if (res.wasOk())
            {
                progress (0.96);
                const juce::ScopedLock sl (dbLock);
                res = install (product, zip, paths, s, db, notes);
            }
            zip.deleteFile();
        }

        post ([res, notes, removeOnly] (MainComponent& mc, ProductRow& r)
        {
            r.setBusy (ProductRow::Busy::none);
            mc.refreshRow (r);
            mc.updateSummary();
            if (res.failed())
                r.setMessage (res.getErrorMessage(), true);
            else
                r.setMessage (removeOnly ? String ("Removed.") : ("Installed. " + notes.joinIntoString (" ")).trim(), false);
        });
    });
}

void MainComponent::showMoreMenu (ProductRow& row)
{
    juce::PopupMenu menu;
    const bool present = row.detected.present;
    menu.addItem (1, row.status == Status::notInstalled ? "Install" : "Reinstall");
    menu.addItem (2, "Remove", present);
   #if JUCE_MAC
    menu.addItem (3, "Show in Finder", present);
   #elif JUCE_LINUX
    menu.addItem (3, "Show in file manager", present);
   #else
    menu.addItem (3, "Show in Explorer", present);
   #endif
    menu.addSeparator();
    menu.addItem (4, "Open the plugin page");
    juce::Component::SafePointer<ProductRow> safe (&row);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&row).withMinimumWidth (180),
                        [this, safe] (int id)
                        {
                            if (safe == nullptr) return;
                            if (id == 1) enqueue (*safe, false);
                            if (id == 2) confirmRemove (*safe);
                            if (id == 3 && ! safe->detected.paths.isEmpty()) File (safe->detected.paths[0]).revealToUser();
                            if (id == 4) openProduct (safe->product.id);
                        });
}

void MainComponent::confirmRemove (ProductRow& row)
{
    StringArray names;
    for (auto& p : row.detected.paths) names.add (tildePath (File (p)));
    juce::Component::SafePointer<ProductRow> safe (&row);
    juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                           .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                           .withTitle ("Remove " + row.product.name + "?")
                                           .withMessage ("This deletes:\n\n" + names.joinIntoString ("\n"))
                                           .withButton ("Remove")
                                           .withButton ("Cancel")
                                           .withAssociatedComponent (this),
                                       [this, safe] (int result)
                                       {
                                           if (result == 0 && safe != nullptr) enqueue (*safe, true);
                                       });
}

void MainComponent::requestSystemWide (bool want)
{
    if (! want || platform::canWrite (paths.vst3System))
    {
        settings.systemWideVst3 = want;
        settings.save (paths.settingsFile());
        if (settingsPanel) settingsPanel->sync();
        return;
    }
    if (settingsPanel) settingsPanel->sync();      // back to off until it is really possible
    juce::Component::SafePointer<MainComponent> safe (this);
    juce::NativeMessageBox::showAsync (juce::MessageBoxOptions()
                                           .withIconType (juce::MessageBoxIconType::InfoIcon)
                                           .withTitle ("Administrator rights needed")
                                           .withMessage ("Installing for all users writes to " + paths.vst3System.getFullPathName()
                                                         + ", which needs administrator rights. Open the launcher again as administrator?")
                                           .withButton ("Relaunch as administrator")
                                           .withButton ("Cancel")
                                           .withAssociatedComponent (this),
                                       [safe] (int result)
                                       {
                                           if (result != 0 || safe == nullptr) return;
                                           Settings s = safe->settings;
                                           s.systemWideVst3 = true;
                                           s.save (safe->paths.settingsFile());
                                           if (platform::relaunchElevated())
                                               juce::JUCEApplicationBase::quit();
                                       });
}

void MainComponent::showSettings (bool show)
{
    if (! show)
    {
        settings.save (paths.settingsFile());
        settingsPanel.reset();
        return;
    }
    settingsPanel = std::make_unique<SettingsPanel> (settings, paths);
    settingsPanel->onClose = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<MainComponent> (this)] { if (safe) safe->showSettings (false); }); };
    settingsPanel->onChanged = [this] { settings.save (paths.settingsFile()); };
    settingsPanel->onSystemWideRequested = [this] (bool want) { requestSystemWide (want); };
    addAndMakeVisible (*settingsPanel);
    settingsPanel->setBounds (getLocalBounds());
}

String MainComponent::installHint() const
{
   #if JUCE_WINDOWS
    return "Plugins go to " + paths.vst3Target (settings).getFullPathName() + ". No account, nothing to sign up for.";
   #elif JUCE_LINUX
    return "Plugins go to " + tildePath (paths.vst3User) + " and " + tildePath (paths.lv2) + ". No account, nothing to sign up for.";
   #else
    return "Plugins go to " + tildePath (paths.vst3User.getParentDirectory()) + ". No account, nothing to sign up for.";
   #endif
}

//==============================================================================
void MainComponent::paint (Graphics& g)
{
    g.fillAll (bg);

    // Header: the site's global nav.
    g.setColour (deep);
    g.fillRect (headerArea);
    g.setColour (hair);
    g.fillRect (headerArea.withTop (headerArea.getBottom() - 1));
    {
        auto f = juce::Font (juce::FontOptions (family(), 15.0f, juce::Font::bold).withKerningFactor (0.08f));
        g.setColour (ink);
        g.setFont (f);
        const String wm = "AUDIO TOOLS";
        g.drawText (wm, kSide, 0, 200, headerArea.getHeight(), Justification::centredLeft);
        const int w = juce::GlyphArrangement::getStringWidthInt (f, wm);
        g.setColour (ink2);
        g.setFont (uiFont (14.0f));
        g.drawText ("Launcher", kSide + w + 12, 0, 120, headerArea.getHeight(), Justification::centredLeft);
    }

    if (! bannerArea.isEmpty())
    {
        g.setColour (card2);
        g.fillRoundedRectangle (bannerArea.toFloat(), 14.0f);
        g.setColour (ink);
        g.setFont (uiFont (14.5f));
        g.drawText ("A new launcher is available (" + manifest.launcherVersion + "). You have " + LAUNCHER_VERSION + ".",
                    bannerArea.withTrimmedLeft (20).withTrimmedRight (bannerBtn.getWidth() + 30), Justification::centredLeft, true);
    }

    // Footer.
    g.setColour (hair);
    g.fillRect (footerArea.getX(), footerArea.getY(), footerArea.getWidth(), 1);
    g.setColour (ink3);
    g.setFont (uiFont (12.5f));
    auto f = footerArea.reduced (kSide, 0);
    const String dot (juce::CharPointer_UTF8 ("  \xc2\xb7  "));
    g.drawText ("Launcher " + String (LAUNCHER_VERSION) + (ready && manifest.generated.isNotEmpty() ? dot + "list of " + manifest.generated : String()),
                f.removeFromRight (260), Justification::centredRight, true);
    g.drawText (installHint(), f, Justification::centredLeft, true);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    headerArea = r.removeFromTop (56);
    footerArea = r.removeFromBottom (36);

    auto h = headerArea.reduced (kSide, 0);
    settingsBtn.setBounds (h.removeFromRight (settingsBtn.idealWidth()).withSizeKeepingCentre (settingsBtn.idealWidth(), 32));
    h.removeFromRight (8);
    if (donate.isVisible())
        donate.setBounds (h.removeFromRight (donate.idealWidth()).withSizeKeepingCentre (donate.idealWidth(), 32));

    bannerArea = {};
    if (launcherUpdateAvailable())
    {
        r.removeFromTop (14);
        bannerArea = r.removeFromTop (48).reduced (kSide, 0);
        r.removeFromTop (2);
        bannerBtn.setBounds (bannerArea.withTrimmedRight (8).removeFromRight (bannerBtn.idealWidth()).withSizeKeepingCentre (bannerBtn.idealWidth(), 32));
    }

    viewport.setBounds (r);
    layoutList();

    if (settingsPanel) settingsPanel->setBounds (getLocalBounds());
}

} // namespace atl
