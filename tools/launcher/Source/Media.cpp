#include "Media.h"

namespace atl
{

AssetLoader::~AssetLoader()
{
    *alive = false;
    pool.removeAllJobs (true, 4000);
}

void AssetLoader::setSource (const String& manifestLocation, const String& siteUrl)
{
    location = manifestLocation;
    site = siteUrl;
}

void AssetLoader::image (const String& url, std::function<void (juce::Image)> done)
{
    if (url.isEmpty()) { done ({}); return; }
    if (auto it = images.find (url); it != images.end()) { done (it->second); return; }

    auto& w = waiting[url];
    w.push_back (std::move (done));
    if (w.size() > 1) return;     // already on its way

    pool.addJob ([this, url, token = alive, p = paths, loc = location, s = site]
    {
        if (! *token) return;
        const auto f = fetchToCache (p, url, loc, s);
        auto img = f == File() ? juce::Image() : juce::ImageFileFormat::loadFrom (f);
        if (! img.isValid() && f.isAChildOf (p.cacheDir())) f.deleteFile();   // a broken copy is fetched again next time
        juce::MessageManager::callAsync ([this, url, img, token]
        {
            if (! *token) return;
            if (img.isValid()) images[url] = img;
            auto cbs = std::move (waiting[url]);
            waiting.erase (url);
            for (auto& cb : cbs) cb (img);
        });
    });
}

void AssetLoader::file (const String& url, std::function<void (File)> done)
{
    pool.addJob ([url, done, token = alive, p = paths, loc = location, s = site]
    {
        if (! *token) return;
        const auto f = fetchToCache (p, url, loc, s);
        juce::MessageManager::callAsync ([f, done, token] { if (*token) done (f); });
    });
}

//==============================================================================
Player::Player()
{
    formats.registerBasicFormats();
   #if JUCE_USE_OPUS
    if (formats.findFormatForFileExtension ("opus") == nullptr)
        formats.registerFormat (new juce::OpusAudioFormat(), false);
   #endif
    readAhead.startThread();
}

Player::~Player()
{
    stop();
    if (device != nullptr)
    {
        device->removeAudioCallback (&source);
        source.setSource (nullptr);
        device->closeAudioDevice();
    }
    readAhead.stopThread (2000);
}

bool Player::play (const File& f, const String& k)
{
    stop();
    auto* r = formats.createReaderFor (f);
    if (r == nullptr) return false;

    if (device == nullptr)
    {
        device = std::make_unique<juce::AudioDeviceManager>();
        if (device->initialiseWithDefaultDevices (0, 2).isNotEmpty() || device->getCurrentAudioDevice() == nullptr)
        {
            delete r;
            device.reset();
            return false;
        }
        source.setSource (&transport);
        device->addAudioCallback (&source);
    }

    reader = std::make_unique<juce::AudioFormatReaderSource> (r, true);
    transport.setSource (reader.get(), 1 << 16, &readAhead, r->sampleRate, 2);
    transport.setGain (muted ? 0.0f : 1.0f);
    transport.setPosition (0.0);
    transport.start();
    key = k;
    return true;
}

void Player::stop()
{
    transport.stop();
    transport.setSource (nullptr);
    reader.reset();
    key.clear();
}

void Player::seek (double fraction)
{
    if (reader != nullptr)
        transport.setPosition (juce::jlimit (0.0, 1.0, fraction) * transport.getLengthInSeconds());
}

String Player::current() const
{
    if (key.isNotEmpty() && reader != nullptr && ! transport.hasStreamFinished()) return key;
    return {};
}

double Player::position() const
{
    const auto len = transport.getLengthInSeconds();
    return len > 0 ? juce::jlimit (0.0, 1.0, transport.getCurrentPosition() / len) : 0.0;
}

} // namespace atl
