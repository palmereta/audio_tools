#pragma once

// Everything the launcher does that is not drawing: the manifest, versions, where each format goes,
// the record of what it installed, finding manual installs, downloading, checking and installing.
// Only juce_core and juce_cryptography, so the headless test links it without a GUI.

#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <atomic>
#include <functional>
#include <map>

namespace atl
{
using juce::File;
using juce::String;
using juce::StringArray;

//==============================================================================
/** Compares dotted versions number by number: 1.0.10 > 1.0.9, 1.2 == 1.2.0. Returns -1, 0 or 1. */
int compareVersions (const String& a, const String& b);

enum class Format { vst3, au, standalone, other };

/** By the item's extension: .vst3, .component, .app (macOS) or .exe (Windows). Anything else
    (INSTALL.txt, LICENSE.txt, the command-line renderers) is "other" and is not installed. */
Format formatOf (const String& itemName);

//==============================================================================
struct Asset
{
    String url, sha256;
    juce::int64 size = 0;
    StringArray files;      // top-level items inside the zip's folder, e.g. "AT20V.vst3", "AT20V FX.app"
    StringArray formats;    // "VST3", "AU", "Standalone"
    bool isValid() const    { return url.isNotEmpty() && sha256.length() == 64; }
};

/** A demo clip from the product page ("Listen"): Opus, so JUCE plays it the same on macOS and Windows. */
struct Sample
{
    String id, title, description, url;
    double seconds = 0;
    juce::Array<float> peaks;    // level per slice, 0..1, for the waveform
};

struct Product
{
    String id, name, version, category, tagline, description, accent;
    String imageThumb, imageCard, imageHero;   // site URLs with ?v=<hash>, fetched lazily and cached
    String pageUrl, manualUrl;
    juce::Array<Sample> samples;
    Asset mac, windows;
    const Asset& forThisOS() const;
};

struct Manifest
{
    int schema = 0;
    String generated, donateUrl, siteUrl, homeTitle, homeLead;
    String launcherVersion;
    Asset launcherMac, launcherWindows;
    juce::Array<Product> products;

    /** Where the manifest came from (an https URL or a local file). Relative zip URLs resolve
        against it. */
    String location;

    const Asset& launcherForThisOS() const;
    const Product* find (const String& id) const;

    static juce::Result parse (const String& json, const String& location, Manifest& out);
};

/** The URL or local file a zip is read from. With a local manifest (offline testing), an https
    URL whose file name sits next to the manifest is read from there, so the real
    web/downloads/manifest.json works without the network. */
juce::URL resolveAssetUrl (const String& url, const String& manifestLocation, const String& siteUrl = {});

//==============================================================================
#if JUCE_WINDOWS
inline constexpr bool kSystemWideDefault = true;    // Windows: VST3 a Common Files por defecto
#else
inline constexpr bool kSystemWideDefault = false;
#endif

struct Settings
{
    bool vst3 = true, au = true, standalone = true;
    bool systemWideVst3 = kSystemWideDefault;   // Windows only: C:\Program Files\Common Files\VST3, the folder every DAW scans

    bool wants (Format f) const;
    void load (const File&);
    bool save (const File&) const;
};

/** Install and data locations. LAUNCHER_INSTALL_ROOT=<dir> sends every format under that folder
    (VST3/, Components/, Applications/, VST3-System/) and LAUNCHER_DATA_DIR=<dir> moves the
    launcher's own files, so tests never touch the real plug-in folders. */
struct Paths
{
    File vst3User, vst3System, au, apps, data;
    juce::Array<File> vst3Extra, auExtra, appsExtra;   // only searched, for manual installs
    bool overridden = false;

    static Paths current();

    File vst3Target (const Settings& s) const   { return s.systemWideVst3 ? vst3System : vst3User; }
    /** Where an item from the zip goes, or File() if this format is not installed here. */
    File destinationFor (const String& itemName, const String& productName, const Settings&) const;

    File recordFile() const    { return data.getChildFile ("installed.json"); }
    File settingsFile() const  { return data.getChildFile ("settings.json"); }
    File cacheFile() const     { return data.getChildFile ("manifest-cache.json"); }
    File workDir() const       { return data.getChildFile ("work"); }
    File cacheDir() const      { return data.getChildFile ("cache"); }
};

//==============================================================================
struct InstallRecord
{
    String version, installedAt;
    StringArray paths;
};

/** What the launcher installed, kept as JSON in the data folder. */
class InstalledDb
{
public:
    void load (const File&);
    bool save() const;

    const InstallRecord* get (const String& id) const;
    void set (const String& id, const InstallRecord&);
    void remove (const String& id);

private:
    File file;
    std::map<String, InstallRecord> records;
};

//==============================================================================
struct Detected
{
    String version;          // empty when nothing is installed (or a version can't be read)
    bool present = false;
    bool viaLauncher = false;
    StringArray paths;       // what is on disk now
    StringArray strays;      // copies in the plugin folders that the launcher's record does not list (a manual install,
                             // the other scope): the DAW may load them instead of the launcher's copy
};

/** Finds the product on disk: the launcher's record when its files are still there; otherwise
    any manual install, by bundle name, with the version from Info.plist (macOS) or
    moduleinfo.json (Windows). With several copies, the oldest version wins so updates show. */
Detected detect (const Product&, const InstalledDb&, const Paths&);

/** Reads the version a bundle declares, or an empty string. */
String bundleVersion (const File& bundle);

enum class Status { notInstalled, upToDate, updateAvailable };
Status statusOf (const Product&, const Detected&);

//==============================================================================
/** Progress: fraction 0..1 (or -1 when unknown). Returning false cancels. */
using ProgressFn = std::function<bool (double fraction)>;

juce::Result download (const juce::URL& from, const File& to, juce::int64 expectedSize, const ProgressFn&);
juce::Result verify (const File& zip, const String& sha256, juce::int64 size);
String sha256Of (const File&);

/** Unzips, keeps the formats the settings ask for (plus any format a previous launcher install
    put on disk, so an update never leaves an old copy behind) and moves them into place.
    Writes the record. 'notes' collects what to tell the user (e.g. rescan for Audio Units). */
juce::Result install (const Product&, const File& zip, const Paths&, const Settings&, InstalledDb&, StringArray& notes);

/** Deletes what the record lists, or what detect() found when the product was installed by hand. */
juce::Result remove (const Product&, const Paths&, InstalledDb&);

/** Images and demo clips: the cached copy of a URL, downloaded the first time. The URLs carry a
    content hash (?v=...), so a changed file is a new URL. Returns File() on failure. */
File cacheFileFor (const Paths&, const String& url);
File fetchToCache (const Paths&, const String& url, const String& manifestLocation, const String& siteUrl);

/** Download + verify + install, cleaning the temporary zip. */
juce::Result downloadAndInstall (const Product&, const Manifest&, const Paths&, const Settings&, InstalledDb&,
                                 const ProgressFn&, StringArray& notes);

} // namespace atl
