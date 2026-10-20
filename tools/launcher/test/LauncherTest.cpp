// Headless test of the launcher core with a local manifest and fake product zips built here.
//   atlaunchertest                     the whole suite, in a temporary folder
//   LAUNCHER_TEST_REALZIP=<zip> atlaunchertest
//                                      also installs a real product zip (from web/downloads) into
//                                      the temporary folder and checks bundles and permissions
// Exit code 0 when every check passes.

#include "Core.h"
#include <cstdio>
#include <cstdlib>

using namespace atl;
using juce::File;
using juce::String;

static int failures = 0, checks = 0;

static void check (bool ok, const String& what)
{
    ++checks;
    if (! ok) ++failures;
    std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
}

static void setEnv (const char* k, const String& v)
{
   #if JUCE_WINDOWS
    _putenv_s (k, v.toRawUTF8());
   #else
    setenv (k, v.toRawUTF8(), 1);
   #endif
}

#if JUCE_WINDOWS
static const char* kOS = "Windows";
#else
static const char* kOS = "macOS";
#endif

/** A zip shaped like the real ones: <Name>-<version>-<OS>/ with bundles, INSTALL.txt and a tool. */
static File makeProductZip (const File& dir, const String& name, const String& version)
{
    const auto stage = dir.getChildFile ("stage-" + name + "-" + version);
    stage.deleteRecursively();
    const auto top = stage.getChildFile (name + "-" + version + "-" + kOS);
    const auto plist = [&] (const String& v)
    {
        return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist><dict>\n<key>CFBundleShortVersionString</key>\n<string>" + v + "</string>\n</dict></plist>\n";
    };
   #if JUCE_WINDOWS
    top.getChildFile (name + ".vst3/Contents/Resources/moduleinfo.json").create();
    top.getChildFile (name + ".vst3/Contents/Resources/moduleinfo.json").replaceWithText ("{\n  \"Name\": \"" + name + "\",\n  \"Version\": \"" + version + "\",\n}\n");
    top.getChildFile (name + ".vst3/Contents/x86_64-win/" + name + ".vst3").create();
    top.getChildFile (name + ".vst3/Contents/x86_64-win/" + name + ".vst3").replaceWithText ("binary " + version);
    top.getChildFile (name + ".exe").replaceWithText ("exe " + version);
   #else
    for (auto ext : { ".vst3", ".component", ".app" })
    {
        const auto b = top.getChildFile (name + ext);
        b.getChildFile ("Contents/MacOS").createDirectory();
        b.getChildFile ("Contents/Info.plist").replaceWithText (plist (version));
        b.getChildFile ("Contents/MacOS/" + name).replaceWithText ("binary " + version);
    }
   #endif
    top.getChildFile ("INSTALL.txt").replaceWithText ("install notes");
    top.getChildFile ("LICENSE.txt").replaceWithText ("license");
    top.getChildFile (name.toLowerCase() + "render").replaceWithText ("tool");

    juce::ZipFile::Builder zb;
    for (const auto& e : juce::RangedDirectoryIterator (stage, true, "*", File::findFiles))
        zb.addFile (e.getFile(), 6, e.getFile().getRelativePathFrom (stage).replaceCharacter ('\\', '/'));
    const auto zip = dir.getChildFile (name + "-" + version + "-" + kOS + ".zip");
    zip.deleteFile();
    {
        juce::FileOutputStream out (zip);
        zb.writeToStream (out, nullptr);
    }
    stage.deleteRecursively();
    return zip;
}

static String assetJson (const File& zip, const String& sha, const String& name)
{
    const String files = "[\"" + name + ".vst3\", \"" + name + ".component\", \"" + name + ".app\", \"" + name + ".exe\"]";
    return "{\"url\": \"" + zip.getFileName() + "\", \"sha256\": \"" + sha + "\", \"size\": " + String (zip.getSize())
         + ", \"files\": " + files + ", \"formats\": [\"VST3\", \"AU\", \"Standalone\"]}";
}

static File writeManifest (const File& dir, const String& fileName, const String& id, const String& name,
                           const String& version, const File& zip, const String& sha)
{
    const auto a = assetJson (zip, sha, name);
    const String json = "{\"schema\": 1, \"generated\": \"2026-10-19\", \"site\": \"https://audiotools.com.ar/\", \"donate_url\": \"\",\n"
                        " \"launcher\": {\"version\": \"1.0.0\", \"macos\": null, \"windows\": null},\n"
                        " \"products\": [{\"id\": \"" + id + "\", \"name\": \"" + name + "\", \"version\": \"" + version
                      + "\", \"category\": \"tools\", \"tagline\": \"A test.\", \"accent\": \"#3e8c84\", \"macos\": " + a + ", \"windows\": " + a + "}]}";
    const auto f = dir.getChildFile (fileName);
    f.replaceWithText (json);
    return f;
}

static Manifest load (const File& f)
{
    Manifest m;
    const auto r = Manifest::parse (f.loadFileAsString(), f.getFullPathName(), m);
    if (r.failed()) std::printf ("parse error: %s\n", r.getErrorMessage().toRawUTF8());
    return m;
}

int main()
{
    const auto tmp = File::getSpecialLocation (File::tempDirectory).getChildFile ("atlaunchertest-" + String (juce::Time::currentTimeMillis()));
    tmp.createDirectory();
    const auto root = tmp.getChildFile ("install-root");
    setEnv ("LAUNCHER_INSTALL_ROOT", root.getFullPathName());
    setEnv ("LAUNCHER_DATA_DIR", tmp.getChildFile ("data").getFullPathName());
    const auto paths = Paths::current();
    std::printf ("temp folder: %s\n", tmp.getFullPathName().toRawUTF8());

    // ── versions
    check (compareVersions ("1.0.10", "1.0.9") > 0, "1.0.10 is newer than 1.0.9");
    check (compareVersions ("1.0.9", "1.0.10") < 0, "1.0.9 is older than 1.0.10");
    check (compareVersions ("1.2", "1.2.0") == 0, "1.2 equals 1.2.0");
    check (compareVersions ("2.0.0", "1.99.99") > 0, "2.0.0 is newer than 1.99.99");
    check (formatOf ("AT20V FX.vst3") == Format::vst3 && formatOf ("INSTALL.txt") == Format::other, "formats by extension");

    // ── install roots come from the environment
    check (paths.overridden && paths.vst3User.isAChildOf (root) && paths.data.isAChildOf (tmp), "install roots overridden by LAUNCHER_INSTALL_ROOT");

    // ── manifest
    const auto zip109 = makeProductZip (tmp, "Fake", "1.0.9");
    const auto sha109 = sha256Of (zip109);
    auto mf = writeManifest (tmp, "manifest.json", "fake", "Fake", "1.0.9", zip109, sha109);
    auto m = load (mf);
    check (m.products.size() == 1 && m.products[0].version == "1.0.9" && m.products[0].forThisOS().size == zip109.getSize(),
           "manifest parsed (1 product, version, size)");
    check (resolveAssetUrl (m.products[0].forThisOS().url, m.location).getLocalFile() == zip109, "relative zip URL resolves next to a local manifest");
    check (resolveAssetUrl ("https://audiotools.com.ar/downloads/" + zip109.getFileName(), m.location).getLocalFile() == zip109,
           "an https zip URL is read from next to a local manifest (offline mode)");

    Settings settings;
    settings.systemWideVst3 = false;     // la carpeta por usuario de la raíz de prueba (en Windows el defecto es Common Files)
    InstalledDb db;
    db.load (paths.recordFile());
    juce::StringArray notes;
    double lastProgress = -1.0;
    const auto progress = [&] (double f) { lastProgress = f; return true; };

    // ── checksum mismatch is rejected and installs nothing
    {
        auto bad = writeManifest (tmp, "manifest-bad.json", "fake", "Fake", "1.0.9", zip109, String::repeatedString ("0", 64));
        auto mb = load (bad);
        const auto r = downloadAndInstall (mb.products[0], mb, paths, settings, db, progress, notes);
        check (r.failed() && r.getErrorMessage().contains ("checksum"), "sha256 mismatch rejected: " + r.getErrorMessage());
        check (! paths.vst3User.getChildFile ("Fake.vst3").exists() && db.get ("fake") == nullptr, "nothing installed after the mismatch");
    }

    // ── a size mismatch too
    {
        Manifest ms = m;
        ms.products.getReference (0).mac.size += 1;
        ms.products.getReference (0).windows.size += 1;
        const auto r = downloadAndInstall (ms.products[0], ms, paths, settings, db, progress, notes);
        check (r.failed(), "size mismatch rejected: " + r.getErrorMessage());
    }

    // ── install
    {
        const auto r = downloadAndInstall (m.products[0], m, paths, settings, db, progress, notes);
        check (r.wasOk(), "install 1.0.9: " + (r.wasOk() ? String ("ok") : r.getErrorMessage()));
        check (lastProgress > 0.9, "progress reported up to " + String (lastProgress, 2));
        check (paths.vst3User.getChildFile ("Fake.vst3/Contents").isDirectory(), "VST3 in " + paths.vst3User.getFullPathName());
       #if JUCE_WINDOWS
        check (paths.apps.getChildFile ("Fake/Fake.exe").existsAsFile(), "standalone in Applications/Fake/");
       #else
        check (paths.au.getChildFile ("Fake.component/Contents/Info.plist").existsAsFile(), "AU in " + paths.au.getFullPathName());
        check (paths.apps.getChildFile ("Fake.app/Contents/MacOS/Fake").existsAsFile(), "standalone in " + paths.apps.getFullPathName());
       #endif
        check (! root.findChildFiles (File::findFiles, true, "INSTALL.txt").size() && ! root.findChildFiles (File::findFiles, true, "fakerender*").size(),
               "INSTALL.txt, LICENSE.txt and tools are not installed");
        check (! paths.workDir().getChildFile ("fake-1.0.9.zip").exists() && ! paths.workDir().getChildFile ("fake-staging").exists(),
               "temporary zip and staging folder cleaned");

        InstalledDb fresh;
        fresh.load (paths.recordFile());
        const auto* rec = fresh.get ("fake");
        check (paths.recordFile().existsAsFile() && rec != nullptr && rec->version == "1.0.9", "record written: " + paths.recordFile().getFullPathName());
       #if JUCE_WINDOWS
        const int expected = 2;     // VST3 + standalone
       #else
        const int expected = 3;     // VST3 + AU + standalone
       #endif
        check (rec != nullptr && rec->paths.size() == expected, "record lists " + String (rec ? rec->paths.size() : 0) + " installed items");

        const auto d = detect (m.products[0], db, paths);
        check (d.present && d.viaLauncher && d.version == "1.0.9" && statusOf (m.products[0], d) == Status::upToDate, "detected as installed by the launcher, up to date");
    }

    // ── update detection: 1.0.10 > 1.0.9
    const auto zip110 = makeProductZip (tmp, "Fake", "1.0.10");
    auto mf2 = writeManifest (tmp, "manifest-110.json", "fake", "Fake", "1.0.10", zip110, sha256Of (zip110));
    auto m2 = load (mf2);
    {
        const auto d = detect (m2.products[0], db, paths);
        check (statusOf (m2.products[0], d) == Status::updateAvailable, "1.0.10 in the manifest, 1.0.9 installed: update available");
        Product older = m2.products[0];
        older.version = "1.0.8";
        check (statusOf (older, d) == Status::upToDate, "1.0.8 in the manifest, 1.0.9 installed: up to date");

        const auto r = downloadAndInstall (m2.products[0], m2, paths, settings, db, progress, notes);
        check (r.wasOk(), "update to 1.0.10: " + (r.wasOk() ? String ("ok") : r.getErrorMessage()));
        check (bundleVersion (paths.vst3User.getChildFile ("Fake.vst3")) == "1.0.10", "the VST3 on disk is 1.0.10");
        const auto d2 = detect (m2.products[0], db, paths);
        check (d2.version == "1.0.10" && statusOf (m2.products[0], d2) == Status::upToDate, "after the update: up to date");
    }

    // ── a copy the launcher did not install, in the other VST3 folder: it counts for the version (the DAW could load
    //    it) and the update removes it
    {
        const auto stray = paths.vst3System.getChildFile ("Fake.vst3");
        paths.vst3System.createDirectory();
        paths.vst3User.getChildFile ("Fake.vst3").copyDirectoryTo (stray);
        // A manual 1.0.8 in the system folder.
        auto plist = stray.getChildFile ("Contents/Info.plist");
        auto mi = stray.getChildFile ("Contents/Resources/moduleinfo.json");
        if (plist.existsAsFile()) plist.replaceWithText (plist.loadFileAsString().replace ("1.0.10", "1.0.8"));
        if (mi.existsAsFile()) mi.replaceWithText (mi.loadFileAsString().replace ("1.0.10", "1.0.8"));
        const auto d = detect (m2.products[0], db, paths);
        check (d.strays.contains (stray.getFullPathName()) && d.version == "1.0.8" && statusOf (m2.products[0], d) == Status::updateAvailable,
               "a manual 1.0.8 in the other VST3 folder is found: update available (" + d.version + ")");
        const auto r = downloadAndInstall (m2.products[0], m2, paths, settings, db, progress, notes);
        const auto d2 = detect (m2.products[0], db, paths);
        check (r.wasOk() && ! stray.exists() && d2.strays.isEmpty() && statusOf (m2.products[0], d2) == Status::upToDate,
               "the update removes the stray copy");
    }

    // ── a failed replacement leaves the installed copy as it was (two phases, roll back)
    {
        const auto dest = paths.vst3User.getChildFile ("Fake.vst3");
        const auto before = bundleVersion (dest);
        // The staging item cannot be moved where it goes: a plain file with the name of the AU destination's parent.
        const auto auDir = paths.au;
       #if ! JUCE_WINDOWS
        auDir.deleteRecursively(); auDir.getParentDirectory().createDirectory(); auDir.replaceWithText ("not a folder");
       #endif
        const auto r = downloadAndInstall (m2.products[0], m2, paths, settings, db, progress, notes);
       #if ! JUCE_WINDOWS
        check (r.failed() && bundleVersion (dest) == before && dest.getChildFile ("Contents").isDirectory(),
               "a failure in the middle leaves the VST3 that was installed (" + r.getErrorMessage() + ")");
        auDir.deleteFile(); auDir.createDirectory();
       #else
        juce::ignoreUnused (r, before);
       #endif
    }

    // ── formats from the settings
    {
        const auto zipOnly = makeProductZip (tmp, "Solo", "2.0.0");
        auto mo = load (writeManifest (tmp, "manifest-solo.json", "solo", "Solo", "2.0.0", zipOnly, sha256Of (zipOnly)));
        Settings vst3Only;
        vst3Only.systemWideVst3 = false;
        vst3Only.au = vst3Only.standalone = false;
        const auto r = downloadAndInstall (mo.products[0], mo, paths, vst3Only, db, progress, notes);
        check (r.wasOk() && paths.vst3User.getChildFile ("Solo.vst3").exists()
                 && ! paths.au.getChildFile ("Solo.component").exists() && ! paths.apps.getChildFile ("Solo.app").exists()
                 && ! paths.apps.getChildFile ("Solo").exists(),
               "with only VST3 selected, only the VST3 is installed");
        remove (mo.products[0], paths, db);
    }

    // ── a manual install is detected (bundle name + version in the bundle)
    {
        const auto manual = makeProductZip (tmp, "Hand", "1.0.2");
        auto mh = load (writeManifest (tmp, "manifest-hand.json", "hand", "Hand", "1.0.3", manual, sha256Of (manual)));
        juce::ZipFile zf (manual);
        const auto unz = tmp.getChildFile ("unz");
        zf.uncompressTo (unz);
        unz.getChildFile ("Hand-1.0.2-" + String (kOS) + "/Hand.vst3").copyDirectoryTo (paths.vst3User.getChildFile ("Hand.vst3"));
        const auto d = detect (mh.products[0], db, paths);
        check (d.present && ! d.viaLauncher && d.version == "1.0.2", "manual install found: version " + d.version);
        check (statusOf (mh.products[0], d) == Status::updateAvailable, "manual 1.0.2 vs latest 1.0.3: update available");
        const auto r = remove (mh.products[0], paths, db);
        check (r.wasOk() && ! paths.vst3User.getChildFile ("Hand.vst3").exists(), "remove deletes a manual install too");
    }

    // ── remove
    {
        const auto before = db.get ("fake") != nullptr ? db.get ("fake")->paths : juce::StringArray();
        const auto r = remove (m2.products[0], paths, db);
        bool gone = ! before.isEmpty();
        for (auto& p : before) gone = gone && ! File (p).exists();
        InstalledDb fresh;
        fresh.load (paths.recordFile());
        check (r.wasOk() && gone, "remove deleted the " + String (before.size()) + " installed items");
        check (fresh.get ("fake") == nullptr, "remove dropped the record");
        check (! detect (m2.products[0], db, paths).present, "not detected after remove");
    }

    // ── images, demos and texts of the manifest; the asset cache
    {
        // A tiny web/ tree: web/downloads/manifest.json and web/assets/...
        const auto web = tmp.getChildFile ("web");
        web.getChildFile ("assets/img/nav/fake.jpg").create();
        web.getChildFile ("assets/img/nav/fake.jpg").replaceWithText ("jpeg bytes");
        web.getChildFile ("assets/samples/fake/one.opus").create();
        web.getChildFile ("assets/samples/fake/one.opus").replaceWithText ("opus bytes");
        const String site = "https://audiotools.com.ar/";
        const String json = "{\"schema\": 1, \"site\": \"" + site + "\", \"home\": {\"title\": \"T\", \"lead\": \"L\"},"
            " \"products\": [{\"id\": \"fake\", \"name\": \"Fake\", \"version\": \"1.0.0\", \"description\": \"D\","
            " \"images\": {\"thumb\": \"" + site + "assets/img/nav/fake.jpg?v=abc\", \"card\": null, \"hero\": \"" + site + "assets/img/missing.jpg?v=1\"},"
            " \"manual\": \"" + site + "fake/manual/\", \"page\": \"" + site + "fake/\","
            " \"samples\": [{\"id\": \"one\", \"title\": \"One\", \"description\": \"First\", \"seconds\": 12.5,"
            " \"url\": \"" + site + "assets/samples/fake/one.opus?v=def\", \"peaks\": [0.1, 0.5, 1.0]}]}]}";
        const auto mfile = web.getChildFile ("downloads/manifest.json");
        mfile.create();
        mfile.replaceWithText (json);
        Manifest mm;
        const auto r = Manifest::parse (json, mfile.getFullPathName(), mm);
        const auto* p = mm.products.isEmpty() ? nullptr : &mm.products.getReference (0);
        check (r.wasOk() && mm.homeTitle == "T" && mm.homeLead == "L" && p != nullptr && p->description == "D"
                 && p->imageThumb.endsWith ("fake.jpg?v=abc") && p->imageCard.isEmpty() && p->manualUrl.endsWith ("/manual/")
                 && p->samples.size() == 1 && p->samples[0].peaks.size() == 3 && p->samples[0].seconds == 12.5,
               "manifest: home texts, description, images, manual and demos parsed");
        if (p != nullptr)
        {
            check (fetchToCache (paths, p->imageThumb, mm.location, mm.siteUrl) == web.getChildFile ("assets/img/nav/fake.jpg"),
                   "offline: an image URL of the site is read from the local web/ tree");
            check (fetchToCache (paths, p->samples[0].url, mm.location, mm.siteUrl) == web.getChildFile ("assets/samples/fake/one.opus"),
                   "offline: a demo clip URL is read from the local web/ tree");
            check (cacheFileFor (paths, "https://x/a.jpg?v=1") != cacheFileFor (paths, "https://x/a.jpg?v=2")
                     && cacheFileFor (paths, "https://x/a.jpg?v=1").getFileExtension() == ".jpg",
                   "cache file per URL (a new ?v= is a new file), keeping the extension");
            // A server that is not there: no file, no leftovers, and no exception.
            const String dead = "http://127.0.0.1:9/assets/img/x.jpg?v=1";
            check (fetchToCache (paths, dead, {}, {}) == File() && ! paths.cacheDir().getChildFile (cacheFileFor (paths, dead).getFileName() + ".part").exists(),
                   "an image that cannot be fetched is skipped quietly");
        }
    }

    // ── optional: a real product zip
    const auto real = juce::SystemStats::getEnvironmentVariable ("LAUNCHER_TEST_REALZIP", {});
    if (real.isNotEmpty())
    {
        const File z (real);
        const auto name = z.getFileName().upToFirstOccurrenceOf ("-", false, false);
        const auto ver = z.getFileName().fromFirstOccurrenceOf ("-", false, false).upToFirstOccurrenceOf ("-", false, false);
        const auto copy = tmp.getChildFile (z.getFileName());
        z.copyFileTo (copy);
        const auto sha = sha256Of (copy);
        auto mr = load (writeManifest (tmp, "manifest-real.json", name.toLowerCase(), name, ver, copy, sha));
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const auto r = downloadAndInstall (mr.products[0], mr, paths, settings, db, progress, notes);
        check (r.wasOk(), "real zip " + z.getFileName() + " installed in " + String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 1)
                          + " s: " + (r.wasOk() ? String ("ok") : r.getErrorMessage()));
       #if JUCE_MAC
        const auto bin = paths.vst3User.getChildFile (name + ".vst3/Contents/MacOS/" + name);
        check (bin.existsAsFile() && bin.hasReadAccess() && juce::File (bin).getFullPathName().isNotEmpty()
                 && (std::system (("test -x \"" + bin.getFullPathName() + "\"").toRawUTF8()) == 0),
               "the VST3 binary is executable");
        // The signature must survive exactly as it does with Apple's own unzip (ditto): same codesign verdict.
        const auto ref = tmp.getChildFile ("ditto");
        std::system (("ditto -x -k \"" + copy.getFullPathName() + "\" \"" + ref.getFullPathName() + "\"").toRawUTF8());
        const auto refTop = ref.getChildFile (z.getFileNameWithoutExtension());
        for (auto ext : { ".vst3", ".component", ".app" })
        {
            const auto b = (String (ext) == ".vst3" ? paths.vst3User : String (ext) == ".component" ? paths.au : paths.apps).getChildFile (name + ext);
            const auto verifyRc = [] (const File& f) { return std::system (("codesign --verify --deep --strict \"" + f.getFullPathName() + "\" 2>/dev/null").toRawUTF8()); };
            const int rc = verifyRc (b), rcRef = verifyRc (refTop.getChildFile (name + ext));
            check (rc == rcRef, "codesign verdict on the installed " + b.getFileName() + " matches ditto's ("
                                + (rc == 0 ? String ("valid") : String ("not valid, as in the zip")) + ")");
        }
        check (bundleVersion (paths.vst3User.getChildFile (name + ".vst3")) == ver, "installed VST3 reports version " + ver);
       #endif
        remove (mr.products[0], paths, db);
    }

    // ── optional: the network path (juce::URL over HTTPS), against the real site
    //    LAUNCHER_TEST_HTTPS=https://.../<zip> LAUNCHER_TEST_HTTPS_SHA=<sha256>
    const auto httpsUrl = juce::SystemStats::getEnvironmentVariable ("LAUNCHER_TEST_HTTPS", {});
    if (httpsUrl.isNotEmpty())
    {
        const auto to = tmp.getChildFile ("https.zip");
        int calls = 0;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const auto r = download (juce::URL (httpsUrl), to, 0, [&] (double) { ++calls; return true; });
        const auto secs = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
        check (r.wasOk() && sha256Of (to) == juce::SystemStats::getEnvironmentVariable ("LAUNCHER_TEST_HTTPS_SHA", {}).toLowerCase(),
               "HTTPS download of " + httpsUrl.fromLastOccurrenceOf ("/", false, false) + ": " + String (to.getSize() / 1e6, 1) + " MB in "
                 + String (secs, 1) + " s, " + String (calls) + " progress calls, sha256 matches");
        const auto missing = httpsUrl.upToLastOccurrenceOf ("/", true, false) + "does-not-exist.zip";
        const auto r404 = download (juce::URL (missing), tmp.getChildFile ("missing.zip"), 0, {});
        check (r404.failed(), "a missing file is an error: " + r404.getErrorMessage());

        // An image from the site goes to the cache once; the second call reads the cached copy.
        const auto img = httpsUrl.upToFirstOccurrenceOf ("/downloads/", false, false) + "/assets/img/nav/orrery.jpg?v=test";
        const auto f1 = fetchToCache (paths, img, {}, {});
        juce::MemoryBlock head;
        f1.loadFileAsData (head);
        const bool jpeg = head.getSize() > 3 && (juce::uint8) head[0] == 0xff && (juce::uint8) head[1] == 0xd8;
        const auto t1 = juce::Time::getMillisecondCounterHiRes();
        const auto f2 = fetchToCache (paths, img, {}, {});
        check (f1.isAChildOf (paths.cacheDir()) && jpeg && f2 == f1 && juce::Time::getMillisecondCounterHiRes() - t1 < 50.0,
               "an image from the site is cached on disk (" + String (f1.getSize() / 1024) + " KB) and reused");
    }

    tmp.deleteRecursively();
    std::printf ("\n%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
