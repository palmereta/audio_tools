#include "Core.h"
#include <regex>
#include <set>

#if JUCE_MAC
 #include <sys/xattr.h>
#endif

namespace atl
{
using juce::Result;
using juce::var;

//==============================================================================
int compareVersions (const String& a, const String& b)
{
    auto pa = StringArray::fromTokens (a.trim(), ".", "");
    auto pb = StringArray::fromTokens (b.trim(), ".", "");
    for (int i = 0; i < juce::jmax (pa.size(), pb.size()); ++i)
    {
        const auto x = i < pa.size() ? pa[i].getLargeIntValue() : 0;
        const auto y = i < pb.size() ? pb[i].getLargeIntValue() : 0;
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

Format formatOf (const String& itemName)
{
    const auto n = itemName.toLowerCase();
    if (n == "presets")            return Format::other;    // the factory .vstpreset files, installed apart
    if (n.endsWith (".vst3"))      return Format::vst3;
    if (n.endsWith (".component")) return Format::au;
    if (n.endsWith (".lv2"))       return Format::lv2;
   #if JUCE_WINDOWS
    if (n.endsWith (".exe"))       return Format::standalone;
   #elif JUCE_LINUX
    // El ejecutable standalone no lleva extensión; las herramientas de línea de comandos (lacquerrender...) no se instalan.
    if (! n.containsChar ('.') && ! n.endsWith ("render")) return Format::standalone;
   #else
    if (n.endsWith (".app"))       return Format::standalone;
   #endif
    return Format::other;
}

//==============================================================================
const Asset& Product::forThisOS() const
{
   #if JUCE_WINDOWS
    return windows;
   #elif JUCE_LINUX
    return linuxAsset;
   #else
    return mac;
   #endif
}

const Asset& Manifest::launcherForThisOS() const
{
   #if JUCE_WINDOWS
    return launcherWindows;
   #elif JUCE_LINUX
    return launcherLinux;
   #else
    return launcherMac;
   #endif
}

const Product* Manifest::find (const String& id) const
{
    for (auto& p : products)
        if (p.id == id) return &p;
    return nullptr;
}

static StringArray toStrings (const var& v)
{
    StringArray out;
    if (auto* arr = v.getArray())
        for (auto& x : *arr) out.add (x.toString());
    return out;
}

static Asset parseAsset (const var& v)
{
    Asset a;
    a.url = v["url"].toString();
    a.sha256 = v["sha256"].toString().toLowerCase();
    a.size = (juce::int64) v["size"];
    a.files = toStrings (v["files"]);
    a.formats = toStrings (v["formats"]);
    return a;
}

Result Manifest::parse (const String& json, const String& loc, Manifest& out)
{
    var root;
    if (auto r = juce::JSON::parse (json, root); r.failed() || ! root.isObject())
        return Result::fail ("The plugin list could not be read.");

    Manifest m;
    m.location = loc;
    m.schema = (int) root["schema"];
    if (m.schema < 1)
        return Result::fail ("The plugin list has an unknown format.");
    if (m.schema > 1)
        DBG ("manifest schema " << m.schema << " is newer than this launcher; reading what it knows");

    m.generated = root["generated"].toString();
    m.siteUrl = root["site"].toString();
    m.donateUrl = root["donate_url"].toString();
    m.homeTitle = root["home"]["title"].toString();
    m.homeLead = root["home"]["lead"].toString();
    const auto& l = root["launcher"];
    m.launcherVersion = l["version"].toString();
    m.launcherMac = parseAsset (l["macos"]);
    m.launcherWindows = parseAsset (l["windows"]);
    m.launcherLinux = parseAsset (l["linux"]);

    if (auto* arr = root["products"].getArray())
        for (auto& p : *arr)
        {
            Product pr;
            pr.id = p["id"].toString();
            pr.name = p["name"].toString();
            pr.version = p["version"].toString();
            pr.category = p["category"].toString();
            pr.tagline = p["tagline"].toString();
            pr.description = p["description"].toString();
            pr.accent = p["accent"].toString();
            pr.imageThumb = p["images"]["thumb"].toString();
            pr.imageCard = p["images"]["card"].toString();
            pr.imageHero = p["images"]["hero"].toString();
            pr.pageUrl = p["page"].toString();
            pr.manualUrl = p["manual"].toString();
            if (auto* ss = p["samples"].getArray())
                for (auto& sv : *ss)
                {
                    Sample sm;
                    sm.id = sv["id"].toString();
                    sm.title = sv["title"].toString();
                    sm.description = sv["description"].toString();
                    sm.url = sv["url"].toString();
                    sm.seconds = (double) sv["seconds"];
                    if (auto* pk = sv["peaks"].getArray())
                        for (auto& x : *pk) sm.peaks.add ((float) (double) x);
                    if (sm.url.isNotEmpty()) pr.samples.add (sm);
                }
            pr.mac = parseAsset (p["macos"]);
            pr.windows = parseAsset (p["windows"]);
            pr.linuxAsset = parseAsset (p["linux"]);
            if (pr.id.isNotEmpty() && pr.name.isNotEmpty() && pr.version.isNotEmpty())
                m.products.add (pr);
        }

    out = std::move (m);
    return Result::ok();
}

static bool isRemote (const String& s)   { return s.startsWithIgnoreCase ("http://") || s.startsWithIgnoreCase ("https://"); }

juce::URL resolveAssetUrl (const String& url, const String& manifestLocation, const String& siteUrl)
{
    if (url.startsWithIgnoreCase ("file://"))
        return juce::URL (url);
    if (File::isAbsolutePath (url))
        return juce::URL (File (url));

    const bool manifestIsLocal = manifestLocation.isNotEmpty() && ! isRemote (manifestLocation);
    if (manifestIsLocal)
    {
        const auto dir = (manifestLocation.startsWithIgnoreCase ("file://") ? juce::URL (manifestLocation).getLocalFile()
                                                                            : File (manifestLocation)).getParentDirectory();
        const auto name = juce::URL::removeEscapeChars (url.fromLastOccurrenceOf ("/", false, false));
        if (dir.getChildFile (name).existsAsFile())
            return juce::URL (dir.getChildFile (name));
        // Images and clips of a local web/ tree: the manifest is web/downloads/manifest.json.
        if (siteUrl.isNotEmpty() && url.startsWith (siteUrl))
        {
            const auto rel = juce::URL::removeEscapeChars (url.substring (siteUrl.length()).upToFirstOccurrenceOf ("?", false, false));
            const auto f = dir.getParentDirectory().getChildFile (rel);
            if (f.existsAsFile()) return juce::URL (f);
        }
        if (! isRemote (url))
            return juce::URL (dir.getChildFile (url));
    }
    if (isRemote (url))
        return juce::URL (url);
    // Relative to a remote manifest.
    return juce::URL (manifestLocation.upToLastOccurrenceOf ("/", true, false) + url);
}

//==============================================================================
bool Settings::wants (Format f) const
{
    switch (f)
    {
        case Format::vst3:       return vst3;
        case Format::au:         return au;
        case Format::lv2:        return lv2;
        case Format::standalone: return standalone;
        case Format::other:      break;
    }
    return false;
}

void Settings::load (const File& f)
{
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (! v.isObject()) return;
    auto get = [&] (const char* k, bool def) { return v.hasProperty (k) ? (bool) v[k] : def; };
    vst3 = get ("vst3", true);
    au = get ("au", true);
    lv2 = get ("lv2", true);
    standalone = get ("standalone", true);
    systemWideVst3 = get ("system_wide_vst3", kSystemWideDefault);
}

bool Settings::save (const File& f) const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("vst3", vst3);
    o->setProperty ("au", au);
    o->setProperty ("lv2", lv2);
    o->setProperty ("standalone", standalone);
    o->setProperty ("system_wide_vst3", systemWideVst3);
    f.getParentDirectory().createDirectory();
    return f.replaceWithText (juce::JSON::toString (var (o)) + "\n");
}

//==============================================================================
Paths Paths::current()
{
    Paths p;
    const auto root = juce::SystemStats::getEnvironmentVariable ("LAUNCHER_INSTALL_ROOT", {});
    const auto data = juce::SystemStats::getEnvironmentVariable ("LAUNCHER_DATA_DIR", {});

    if (root.isNotEmpty())
    {
        const File r (root);
        p.overridden = true;
        p.vst3User = r.getChildFile ("VST3");
        p.vst3System = r.getChildFile ("VST3-System");
        p.au = r.getChildFile ("Components");
        p.lv2 = r.getChildFile ("LV2");
        p.apps = r.getChildFile ("Applications");
        p.presets = r.getChildFile ("Presets");
       #if JUCE_LINUX
        p.desktopEntries = r.getChildFile ("applications");
       #endif
        p.data = data.isNotEmpty() ? File (data) : r.getChildFile (".launcher-data");
        p.vst3Extra.add (p.vst3System);
        return p;
    }

   #if JUCE_WINDOWS
    const auto local = File::getSpecialLocation (File::windowsLocalAppData);
    // The per-user VST3 folder of the VST3 specification; hosts scan it as well.
    p.vst3User = local.getChildFile ("Programs\\Common\\VST3");
    p.vst3System = File (juce::SystemStats::getEnvironmentVariable ("CommonProgramFiles", "C:\\Program Files\\Common Files")).getChildFile ("VST3");
    p.apps = local.getChildFile ("Programs\\Audio Tools");
    // Documents as the shell resolves it (also when it is redirected to OneDrive), as the VST3 hosts do.
    p.presets = File::getSpecialLocation (File::userDocumentsDirectory).getChildFile ("VST3 Presets\\Audio Tools");
    p.data = File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Audio Tools\\Launcher");
    p.vst3Extra.add (p.vst3System);
   #elif JUCE_LINUX
    const auto home = File::getSpecialLocation (File::userHomeDirectory);
    // Las carpetas por usuario que escanean los hosts (especificaciones de VST3 y LV2). Como en macOS, se instala
    // sólo para este usuario; las carpetas del sistema se revisan para encontrar instalaciones a mano.
    p.vst3User = home.getChildFile (".vst3");
    p.vst3System = p.vst3User;
    p.lv2 = home.getChildFile (".lv2");
    p.presets = home.getChildFile (".vst3/presets/Audio Tools");
    p.apps = home.getChildFile (".local/opt/audio-tools");
    p.desktopEntries = home.getChildFile (".local/share/applications");
    p.data = File::getSpecialLocation (File::userApplicationDataDirectory).getChildFile ("Audio Tools Launcher");   // ~/.config
    for (auto* d : { "/usr/lib/vst3", "/usr/local/lib/vst3" }) p.vst3Extra.add (File (d));
    for (auto* d : { "/usr/lib/lv2", "/usr/local/lib/lv2" })   p.lv2Extra.add (File (d));
   #else
    const auto home = File::getSpecialLocation (File::userHomeDirectory);
    p.vst3User = home.getChildFile ("Library/Audio/Plug-Ins/VST3");
    p.vst3System = p.vst3User;      // macOS installs per user; the system folder is only searched
    p.au = home.getChildFile ("Library/Audio/Plug-Ins/Components");
    p.presets = home.getChildFile ("Library/Audio/Presets/Audio Tools");
    p.apps = home.getChildFile ("Applications");
    p.data = home.getChildFile ("Library/Application Support/Audio Tools Launcher");
    p.vst3Extra.add (File ("/Library/Audio/Plug-Ins/VST3"));
    p.auExtra.add (File ("/Library/Audio/Plug-Ins/Components"));
    p.appsExtra.add (File ("/Applications"));
   #endif
    if (data.isNotEmpty()) p.data = File (data);
    return p;
}

File Paths::destinationFor (const String& item, const String& productName, const Settings& s) const
{
    juce::ignoreUnused (productName);
    switch (formatOf (item))
    {
        case Format::vst3: return vst3Target (s).getChildFile (item);
        case Format::lv2:  return lv2 == File() ? File() : lv2.getChildFile (item);
       #if JUCE_WINDOWS || JUCE_LINUX
        case Format::au:   return {};
        case Format::standalone: return apps.getChildFile (productName).getChildFile (item);
       #else
        case Format::au:   return au.getChildFile (item);
        case Format::standalone: return apps.getChildFile (item);
       #endif
        case Format::other: break;
    }
    return {};
}

/** Every folder where an item could be, launcher target first. */
static juce::Array<File> searchDirs (const Paths& p, Format f, const String& productName)
{
    juce::ignoreUnused (productName);
    juce::Array<File> d;
    switch (f)
    {
        case Format::vst3:       d.add (p.vst3User); d.add (p.vst3System); d.addArray (p.vst3Extra); break;
        case Format::au:         if (p.au != File()) d.add (p.au); d.addArray (p.auExtra); break;
        case Format::lv2:        if (p.lv2 != File()) d.add (p.lv2); d.addArray (p.lv2Extra); break;
       #if JUCE_WINDOWS || JUCE_LINUX
        case Format::standalone: d.add (p.apps.getChildFile (productName)); break;
       #else
        case Format::standalone: d.add (p.apps); d.addArray (p.appsExtra); break;
       #endif
        case Format::other: break;
    }
    juce::Array<File> unique;
    for (auto& x : d)
        if (x != File()) unique.addIfNotAlreadyThere (x);
    return unique;
}

//==============================================================================
void InstalledDb::load (const File& f)
{
    file = f;
    records.clear();
    const auto v = juce::JSON::parse (f.loadFileAsString());
    if (auto* o = v["products"].getDynamicObject())
        for (auto& prop : o->getProperties())
        {
            InstallRecord r;
            r.version = prop.value["version"].toString();
            r.installedAt = prop.value["installed_at"].toString();
            r.paths = toStrings (prop.value["paths"]);
            r.presetFiles = toStrings (prop.value["presets"]);
            records[prop.name.toString()] = r;
        }
}

bool InstalledDb::save() const
{
    auto* prods = new juce::DynamicObject();
    for (auto& [id, r] : records)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty ("version", r.version);
        o->setProperty ("installed_at", r.installedAt);
        juce::Array<var> paths;
        for (auto& s : r.paths) paths.add (s);
        o->setProperty ("paths", paths);
        if (! r.presetFiles.isEmpty())
        {
            juce::Array<var> presets;
            for (auto& s : r.presetFiles) presets.add (s);
            o->setProperty ("presets", presets);
        }
        prods->setProperty (id, var (o));
    }
    auto* root = new juce::DynamicObject();
    root->setProperty ("schema", 1);
    root->setProperty ("products", var (prods));
    file.getParentDirectory().createDirectory();
    return file.replaceWithText (juce::JSON::toString (var (root)) + "\n");
}

const InstallRecord* InstalledDb::get (const String& id) const
{
    auto it = records.find (id);
    return it == records.end() ? nullptr : &it->second;
}

void InstalledDb::set (const String& id, const InstallRecord& r)  { records[id] = r; }
void InstalledDb::remove (const String& id)                        { records.erase (id); }

//==============================================================================
static String firstMatch (const String& text, const char* pattern)
{
    const std::string s = text.toStdString();
    std::smatch m;
    if (std::regex_search (s, m, std::regex (pattern)) && m.size() > 1)
        return String (m[1].str()).trim();
    return {};
}

String bundleVersion (const File& bundle)
{
    if (! bundle.exists()) return {};
    const auto plist = bundle.getChildFile ("Contents/Info.plist");
    if (plist.existsAsFile())
        return firstMatch (plist.loadFileAsString(), "<key>CFBundleShortVersionString</key>\\s*<string>([^<]*)</string>");
    // VST3 bundle on Windows: moduleinfo.json (JSON5, trailing commas, so a regex rather than JSON::parse).
    const auto mi = bundle.getChildFile ("Contents/Resources/moduleinfo.json");
    if (mi.existsAsFile())
        return firstMatch (mi.loadFileAsString(), "\"Version\"\\s*:\\s*\"([^\"]*)\"");
    // LV2 (Linux): JUCE escribe la versión completa en dsp.ttl como doap:revision "1.0.6".
    const auto ttl = bundle.getChildFile ("dsp.ttl");
    if (ttl.existsAsFile())
        return firstMatch (ttl.loadFileAsString(), "doap:revision\\s*\"([^\"]*)\"");
    return {};
}

Detected detect (const Product& p, const InstalledDb& db, const Paths& paths)
{
    Detected d;
    if (auto* r = db.get (p.id))
    {
        for (auto& s : r->paths)
            if (File (s).exists()) d.paths.add (s);
        if (! d.paths.isEmpty())
        {
            d.present = d.viaLauncher = true;
            d.version = r->version;
            // Other copies in the plugin folders (a manual install, the other scope): the DAW may load an old one
            // while the launcher says "up to date". They count for the version, so the update replaces them.
            for (auto& item : p.forThisOS().files)
            {
                const auto f = formatOf (item);
                if (f == Format::other) continue;
                for (auto& dir : searchDirs (paths, f, p.name))
                {
                    const auto b = dir.getChildFile (item);
                    if (! b.exists() || d.paths.contains (b.getFullPathName())) continue;
                    d.strays.add (b.getFullPathName());
                    if (auto v = bundleVersion (b); v.isEmpty() || compareVersions (v, d.version) < 0) d.version = v;
                }
            }
            return d;
        }
    }

    StringArray versions;
    for (auto& item : p.forThisOS().files)
    {
        const auto f = formatOf (item);
        if (f == Format::other) continue;
        for (auto& dir : searchDirs (paths, f, p.name))
        {
            const auto b = dir.getChildFile (item);
            if (! b.exists()) continue;
            d.paths.addIfNotAlreadyThere (b.getFullPathName());
            if (auto v = bundleVersion (b); v.isNotEmpty()) versions.add (v);
        }
    }
    d.present = ! d.paths.isEmpty();
    for (auto& v : versions)
        if (d.version.isEmpty() || compareVersions (v, d.version) < 0) d.version = v;
    return d;
}

Status statusOf (const Product& p, const Detected& d)
{
    if (! d.present) return Status::notInstalled;
    if (d.version.isEmpty() || compareVersions (p.version, d.version) > 0) return Status::updateAvailable;
    return Status::upToDate;
}

//==============================================================================
Result download (const juce::URL& from, const File& to, juce::int64 expectedSize, const ProgressFn& progress)
{
    int status = 0;
    std::unique_ptr<juce::InputStream> in;
    if (from.isLocalFile())
        in = from.getLocalFile().createInputStream();
    else
        in = from.createInputStream (juce::URL::InputStreamOptions (juce::URL::ParameterHandling::inAddress)
                                         .withConnectionTimeoutMs (20000)
                                         .withNumRedirectsToFollow (5)
                                         .withStatusCode (&status));
    if (in == nullptr)
        return Result::fail ("Could not connect. Check your internet connection and try again.");
    if (status != 0 && status != 200)
        return Result::fail ("The server answered " + String (status) + ". Try again later.");

    to.getParentDirectory().createDirectory();
    to.deleteFile();
    {
        juce::FileOutputStream out (to);
        if (out.failedToOpen())
            return Result::fail ("Could not write " + to.getFullPathName());

        const auto total = expectedSize > 0 ? expectedSize : in->getTotalLength();
        juce::HeapBlock<char> buf (1 << 18);
        juce::int64 done = 0;
        for (;;)
        {
            const auto n = in->read (buf, 1 << 18);
            if (n < 0) return Result::fail ("The download was interrupted.");
            if (n == 0) break;
            if (! out.write (buf, (size_t) n)) return Result::fail ("Could not write the download to disk.");
            done += n;
            if (progress && ! progress (total > 0 ? juce::jlimit (0.0, 1.0, (double) done / (double) total) : -1.0))
            {
                out.flush();
                return Result::fail ("Cancelled.");
            }
        }
        out.flush();
        if (out.getStatus().failed()) return Result::fail ("Could not write the download to disk.");
        if (expectedSize > 0 && done != expectedSize)
            return Result::fail ("The download has the wrong size (" + String (done) + " bytes, expected " + String (expectedSize) + "). Try again.");
        if (total > 0 && done < total) return Result::fail ("The download was interrupted.");
    }
    return Result::ok();
}

String sha256Of (const File& f)
{
    juce::FileInputStream in (f);
    if (! in.openedOk()) return {};
    return juce::SHA256 (in).toHexString().toLowerCase();
}

Result verify (const File& zip, const String& sha256, juce::int64 size)
{
    if (! zip.existsAsFile())
        return Result::fail ("The download is missing.");
    if (size > 0 && zip.getSize() != size)
        return Result::fail ("The download is incomplete (" + String (zip.getSize()) + " of " + String (size) + " bytes). Try again.");
    if (sha256Of (zip) != sha256.toLowerCase())
        return Result::fail ("The download did not match its checksum and was not installed. Try again.");
    return Result::ok();
}

//==============================================================================
static void clearQuarantine (const File& f)
{
   #if JUCE_MAC
    removexattr (f.getFullPathName().toRawUTF8(), "com.apple.quarantine", XATTR_NOFOLLOW);
    if (f.isDirectory())
        for (const auto& e : juce::RangedDirectoryIterator (f, true, "*", File::findFilesAndDirectories))
            removexattr (e.getFile().getFullPathName().toRawUTF8(), "com.apple.quarantine", XATTR_NOFOLLOW);
   #else
    juce::ignoreUnused (f);
   #endif
}

static bool moveInto (const File& item, const File& dest)
{
    if (item.moveFileTo (dest)) return true;
    // Another volume: copy, then drop the staged copy.
    const bool ok = item.isDirectory() ? item.copyDirectoryTo (dest) : item.copyFileTo (dest);
    if (ok) item.deleteRecursively();
    return ok;
}

#if JUCE_LINUX
/** Linux: la entrada del menú de aplicaciones de una standalone instalada (~/.local/share/applications), para
    abrirla sin terminal. Devuelve el .desktop escrito, o File() si no se pudo. */
static File writeDesktopEntry (const Paths& paths, const File& exe, const String& productName)
{
    if (paths.desktopEntries == File() || ! paths.desktopEntries.createDirectory()) return {};
    // Exec entre comillas: se escapan " ` $ \ y, como todo valor del .desktop, otra vez la barra; % es %%.
    auto path = exe.getFullPathName().replace ("\\", "\\\\").replace ("\"", "\\\"").replace ("`", "\\`").replace ("$", "\\$");
    path = path.replace ("\\", "\\\\").replace ("%", "%%");
    const auto name = exe.getFileName();
    const auto file = paths.desktopEntries.getChildFile ("audiotools-" + name.toLowerCase().replaceCharacter (' ', '-') + ".desktop");
    const String text = "[Desktop Entry]\n"
                        "Type=Application\n"
                        "Name=" + name + "\n"
                        "Comment=Audio Tools " + productName + " (standalone)\n"
                        "Exec=\"" + path + "\"\n"
                        "Icon=audio-x-generic\n"
                        "Terminal=false\n"
                        "Categories=AudioVideo;Audio;Music;\n";
    return file.replaceWithText (text, false, false, "\n") ? file : File();    // sin \r: lo leería como parte del valor
}
#endif

static String writeHint (const File& dir)
{
   #if JUCE_WINDOWS
    if (dir.getFullPathName().startsWithIgnoreCase (File::getSpecialLocation (File::globalApplicationsDirectory).getFullPathName()))
        return " Installing for all users needs administrator rights.";
   #endif
    juce::ignoreUnused (dir);
    return {};
}

/** Deletes factory preset files the launcher installed, then the folders they leave empty (up to, not including,
    Paths::presets). Returns the files that could not be deleted. */
static StringArray removePresetFiles (const StringArray& files, const Paths& paths)
{
    StringArray failed;
    for (auto& s : files)
    {
        const File f (s);
        if (f.existsAsFile() && ! f.deleteFile()) { failed.add (s); continue; }
        if (paths.presets == File()) continue;
        for (auto d = f.getParentDirectory(); d.isAChildOf (paths.presets); d = d.getParentDirectory())
            if (! d.isDirectory() || d.getNumberOfChildFiles (File::findFilesAndDirectories) > 0 || ! d.deleteFile()) break;
    }
    return failed;
}

/** The zip's Presets/<plugin name>/[<category>/]<preset>.vstpreset into Paths::presets. Files of a previous install that
    this one does not bring are deleted; anything else in those folders (presets the user saved) is not touched.
    Returns the files now installed. */
static StringArray installPresets (const File& zipPresets, const Paths& paths, const StringArray& previous, StringArray& notes)
{
    StringArray placed, failed;
    if (paths.presets != File() && zipPresets.isDirectory())
    {
        for (const auto& e : juce::RangedDirectoryIterator (zipPresets, true, "*.vstpreset", File::findFiles))
        {
            const auto rel = e.getFile().getRelativePathFrom (zipPresets);
            const auto dest = paths.presets.getChildFile (rel);
            if (dest.getParentDirectory().createDirectory() && e.getFile().copyFileTo (dest))
                placed.add (dest.getFullPathName());
            else
                failed.add (dest.getFullPathName());
        }
    }
    StringArray stale;
    for (auto& s : previous)
        if (! placed.contains (s)) stale.add (s);
    failed.addArray (removePresetFiles (stale, paths));
    if (! failed.isEmpty())
        notes.add ("Some preset files could not be written or replaced in " + paths.presets.getFullPathName() + " (" + failed[0]
                   + (failed.size() > 1 ? " and " + String (failed.size() - 1) + " more" : String()) + ").");
    placed.sort (true);
    return placed;
}

Result install (const Product& p, const File& zip, const Paths& paths, const Settings& settings, InstalledDb& db, StringArray& notes)
{
    const auto work = paths.workDir().getChildFile (p.id + "-staging");
    work.deleteRecursively();
    if (! work.createDirectory())
        return Result::fail ("Could not create " + work.getFullPathName());

    juce::ZipFile zf (zip);
    if (zf.getNumEntries() == 0)
        return Result::fail ("The download is not a valid zip file.");

    for (int i = 0; i < zf.getNumEntries(); ++i)
    {
        const auto* e = zf.getEntry (i);
        if (e == nullptr || e->filename.startsWith ("__MACOSX")) continue;
        if (auto r = zf.uncompressEntry (i, work, juce::ZipFile::OverwriteFiles::yes, juce::ZipFile::FollowSymlinks::no); r.failed())
        {
            work.deleteRecursively();
            return Result::fail ("Could not unzip: " + r.getErrorMessage());
        }
       #if ! JUCE_WINDOWS
        // juce::ZipFile does not restore permissions: put the executable bit back (bundle binaries).
        const auto mode = (e->externalFileAttributes >> 16) & 0777;
        if (! e->isSymbolicLink && (mode & 0111) != 0 && ! e->filename.endsWithChar ('/'))
            work.getChildFile (e->filename).setExecutePermission (true);
       #endif
    }

    // The zips hold one folder, <Product>-<version>-<OS>/, with the bundles inside.
    auto root = work;
    {
        auto top = work.findChildFiles (File::findFilesAndDirectories, false, "*", File::FollowSymlinks::no);
        top.removeIf ([] (const File& f) { return f.getFileName().startsWith (".") || f.getFileName() == "__MACOSX"; });
        if (top.size() == 1 && top[0].isDirectory() && formatOf (top[0].getFileName()) == Format::other)
            root = top[0];
    }

    const auto* prev = db.get (p.id);
    StringArray prevNames;
    if (prev != nullptr)
        for (auto& s : prev->paths) prevNames.add (File (s).getFileName());

    // Two phases, so a failure never leaves a half-replaced plugin. First every old copy is moved aside (on Windows a
    // DAW holding the .vst3 makes that rename fail before anything is touched); then the new ones go in. Any failure
    // puts the old copies back.
    struct Step { File item, dest, aside; Format f; };
    std::vector<Step> steps;
    for (auto& item : root.findChildFiles (File::findFilesAndDirectories, false, "*", File::FollowSymlinks::no))
    {
        const auto name = item.getFileName();
        const auto f = formatOf (name);
        if (f == Format::other) continue;
        if (! settings.wants (f) && ! prevNames.contains (name)) continue;
        const auto dest = paths.destinationFor (name, p.name, settings);
        if (dest == File()) continue;
        steps.push_back ({ item, dest, dest.getSiblingFile (name + ".launcher-old"), f });
    }
    auto rollBack = [&] (size_t movedAside, size_t placed) {
        for (size_t k = 0; k < placed; ++k) steps[k].dest.deleteRecursively();
        for (size_t k = 0; k < movedAside; ++k) if (steps[k].aside.exists()) steps[k].aside.moveFileTo (steps[k].dest);
    };
    for (size_t k = 0; k < steps.size(); ++k)
    {
        auto& s = steps[k];
        if (! s.dest.getParentDirectory().createDirectory())
        { rollBack (k, 0); work.deleteRecursively(); return Result::fail ("Could not create " + s.dest.getParentDirectory().getFullPathName() + "." + writeHint (s.dest)); }
        s.aside.deleteRecursively();
        if (s.dest.exists() && ! s.dest.moveFileTo (s.aside))
        { rollBack (k, 0); work.deleteRecursively(); return Result::fail ("Could not replace " + s.dest.getFullPathName() + ". Close any app or DAW that is using it and try again." + writeHint (s.dest)); }
    }
    StringArray installed;
    bool gotAU = false;
    for (size_t k = 0; k < steps.size(); ++k)
    {
        auto& s = steps[k];
        if (! moveInto (s.item, s.dest))
        { rollBack (steps.size(), k); work.deleteRecursively(); return Result::fail ("Could not write " + s.dest.getFullPathName() + "." + writeHint (s.dest)); }
        clearQuarantine (s.dest);
        installed.add (s.dest.getFullPathName());
        gotAU = gotAU || s.f == Format::au;
    }
   #if JUCE_LINUX
    for (auto& s : steps)
        if (s.f == Format::standalone)
            if (const auto entry = writeDesktopEntry (paths, s.dest, p.name); entry != File())
                installed.add (entry.getFullPathName());
   #endif
    for (auto& s : steps)
        if (s.aside.exists() && ! s.aside.deleteRecursively())
            notes.add ("Could not delete the old copy " + s.aside.getFullPathName() + "; it is safe to delete it by hand.");
    // The factory presets go with the VST3 (hosts such as Ableton Live list these files, not the plugin's programs).
    const bool gotVst3 = std::any_of (steps.begin(), steps.end(), [] (const Step& s) { return s.f == Format::vst3; });
    const auto presetFiles = installPresets (gotVst3 ? root.getChildFile ("Presets") : File(), paths,
                                             prev != nullptr ? prev->presetFiles : StringArray(), notes);
    work.deleteRecursively();

    if (installed.isEmpty())
        return Result::fail ("Nothing to install. Turn on at least one format in Settings.");

    // Copies from a previous install that this one did not replace (moved folder, removed item), and copies the
    // launcher did not put there (a manual install, the other scope). If one cannot be removed (it needs other
    // permissions), say where it is: the DAW could keep loading it.
    StringArray old;
    if (prev != nullptr) old.addArray (prev->paths);
    old.addArray (detect (p, db, paths).strays);
    for (auto& s : old)
        if (! installed.contains (s) && File (s).exists() && ! File (s).deleteRecursively())
            notes.add ("An older copy is still at " + s + " and your DAW may load it. Delete it by hand (it needs other permissions).");

    installed.sort (true);
    InstallRecord rec;
    rec.version = p.version;
    rec.installedAt = juce::Time::getCurrentTime().toISO8601 (true);
    rec.paths = installed;
    rec.presetFiles = presetFiles;
    db.set (p.id, rec);
    if (! db.save())
        notes.add ("The install worked, but the launcher could not save its record.");

    if (gotAU)
        notes.add ("Your DAW may need a plugin rescan to see the Audio Unit.");
    return Result::ok();
}

Result remove (const Product& p, const Paths& paths, InstalledDb& db)
{
    const auto d = detect (p, db, paths);
    StringArray failed;
    for (auto& s : d.paths)
    {
        const File f (s);
        if (f.exists() && ! f.deleteRecursively()) failed.add (s);
    }
    if (auto* r = db.get (p.id))
        failed.addArray (removePresetFiles (r->presetFiles, paths));
    db.remove (p.id);
    db.save();
    if (! failed.isEmpty())
        return Result::fail ("Could not delete " + failed.joinIntoString (", ") + ". It may be in use or need administrator rights.");
    return Result::ok();
}

File cacheFileFor (const Paths& paths, const String& url)
{
    auto ext = url.upToFirstOccurrenceOf ("?", false, false).fromLastOccurrenceOf (".", true, false);
    if (ext.length() > 6 || ext.containsChar ('/')) ext = ".bin";
    return paths.cacheDir().getChildFile (String::toHexString (url.hashCode64()) + ext);
}

File fetchToCache (const Paths& paths, const String& url, const String& manifestLocation, const String& siteUrl)
{
    if (url.isEmpty()) return {};
    const auto f = cacheFileFor (paths, url);
    if (f.existsAsFile() && f.getSize() > 0) return f;
    const auto from = resolveAssetUrl (url, manifestLocation, siteUrl);
    if (from.isLocalFile())
        return from.getLocalFile().existsAsFile() ? from.getLocalFile() : File();
    const auto tmp = f.getSiblingFile (f.getFileName() + ".part");
    if (download (from, tmp, 0, {}).failed() || tmp.getSize() == 0 || ! tmp.moveFileTo (f))
    {
        tmp.deleteFile();
        return {};
    }
    return f;
}

Result downloadAndInstall (const Product& p, const Manifest& m, const Paths& paths, const Settings& settings, InstalledDb& db,
                           const ProgressFn& progress, StringArray& notes)
{
    const auto& a = p.forThisOS();
    if (! a.isValid())
        return Result::fail ("Not available for this system.");

    const auto zip = paths.workDir().getChildFile (p.id + "-" + p.version + ".zip");
    auto r = download (resolveAssetUrl (a.url, m.location), zip, a.size,
                       [&] (double f) { return progress == nullptr || progress (f < 0 ? -1.0 : f * 0.9); });
    if (r.wasOk())
    {
        if (progress) progress (0.92);
        r = verify (zip, a.sha256, a.size);
    }
    if (r.wasOk())
    {
        if (progress) progress (0.96);
        r = install (p, zip, paths, settings, db, notes);
    }
    zip.deleteFile();
    return r;
}

} // namespace atl
