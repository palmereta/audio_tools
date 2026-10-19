#pragma once

// Images and demo clips: fetched lazily from the site, cached on disk (data/cache), and never in
// the way of installing. If an image or a clip fails, the launcher shows a coloured placeholder
// or a disabled play button and carries on.

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Core.h"

namespace atl
{

class AssetLoader
{
public:
    explicit AssetLoader (const Paths& p) : paths (p) {}
    ~AssetLoader();

    void setSource (const String& manifestLocation, const String& siteUrl);

    /** The image for this URL, on the message thread (invalid on failure). Kept in memory. */
    void image (const String& url, std::function<void (juce::Image)> done);
    /** The cached local file for this URL, on the message thread (File() on failure). */
    void file (const String& url, std::function<void (File)> done);

private:
    const Paths& paths;
    String location, site;
    juce::ThreadPool pool { juce::ThreadPoolOptions{}.withNumberOfThreads (2) };
    std::map<String, juce::Image> images;
    std::map<String, std::vector<std::function<void (juce::Image)>>> waiting;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>> (true);
};

/** Plays one clip at a time. The audio device opens on the first play, not at startup. */
class Player
{
public:
    Player();
    ~Player();

    bool play (const File&, const String& key);
    void stop();
    void seek (double fraction);

    String current() const;      // key of the clip playing, or empty
    double position() const;     // 0..1
    void setMuted (bool m)       { muted = m; transport.setGain (m ? 0.0f : 1.0f); }

private:
    std::unique_ptr<juce::AudioDeviceManager> device;
    juce::AudioFormatManager formats;
    juce::AudioSourcePlayer source;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::AudioFormatReaderSource> reader;
    juce::TimeSliceThread readAhead { "launcher audio" };
    String key;
    bool muted = false;
};

} // namespace atl
