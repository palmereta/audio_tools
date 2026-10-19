# Audio Tools Launcher

A small app for macOS and Windows that installs the Audio Tools plugins and keeps them up to date.
No account, no sign-up, no telemetry. The manual downloads on the website stay exactly as they are:
the launcher is a second way in, not a replacement.

## What it does

- Reads `https://audiotools.com.ar/downloads/manifest.json` (written by `scripts/build-web.py`) each
  time it opens, and keeps a copy for when there is no connection.
- Home: the series title and the shelf of plugin images (as on the site's home), then every plugin
  with its thumbnail, latest version, installed version and one button: Install, Update or
  Installed. The "..." menu has Reinstall, Remove, Show in Finder/Explorer. Install all and
  Update all sit under the shelf.
- A page per plugin (click a row or a shelf panel): big image, tagline, description, versions and
  formats, Install/Update/Remove, the online manual, a link to the plugin's web page, and the
  "Listen." demos with play/stop, a waveform with progress and seeking.
- Downloads with progress, checks size and SHA-256, unzips (restoring the executable bits) and
  moves each bundle into place. A failed check installs nothing.
- Finds plugins installed by hand (bundle name + `CFBundleShortVersionString` on macOS,
  `moduleinfo.json` on Windows), so manual and launcher installs live together.
- A quiet banner when the manifest lists a newer launcher; its button opens the download in the
  browser. The launcher never replaces itself.
- "Support Audio Tools" opens `donate_url` from the manifest, or `kDonateUrl` in
  `Source/Config.h` when the manifest has none. Both empty hides the button.

Network requests: the manifest, the zips the user installs, and the images and demo clips of the
site (fetched when shown, cached in the data folder under `cache/`, keyed by URL; every URL
carries `?v=<content hash>`, so a changed file is fetched again). If an image or a clip fails, a
coloured placeholder or a disabled play button is shown and installing still works.

## Where things go

| | macOS | Windows |
|---|---|---|
| VST3 | `~/Library/Audio/Plug-Ins/VST3` | `%LOCALAPPDATA%\Programs\Common\VST3` (per user, VST3 spec), or `C:\Program Files\Common Files\VST3` with "Install VST3 for all users" (asks to relaunch as administrator) |
| Audio Unit | `~/Library/Audio/Plug-Ins/Components` | — |
| Standalone | `~/Applications` | `%LOCALAPPDATA%\Programs\Audio Tools\<Plugin>\` |
| Launcher data | `~/Library/Application Support/Audio Tools Launcher` | `%APPDATA%\Audio Tools\Launcher` |

The data folder holds `installed.json` (what the launcher installed, with paths), `settings.json`
(formats, all-users VST3), `manifest-cache.json` and `cache/`. On macOS the quarantine attribute is
removed from every installed bundle. Manual installs in `/Library/...` and `/Applications` are
found but not written to. `INSTALL.txt`, `LICENSE.txt` and the command-line renderers in the zips
are not installed.

## The manifest

`python3 scripts/build-web.py` writes `web/downloads/manifest.json` next to the zips: the products
in `RELEASED` (both zips present), each with version, category, tagline, description, accent
colour, images (thumb, card, hero), manual and page URLs, demos and, per OS, url + sha256 + size +
the bundles inside + formats. Also the launcher's own version and zips, `donate_url`
(`SUPPORT_COFFEE_URL` in build-web.py, the same link as the site's download dialog) and the home
texts. `generated` only changes when the content does. If a zip is a Git LFS pointer, the sha256
and size come from the pointer.

The demos are the site's clips re-encoded to Opus (`web/assets/samples/<plugin>/<id>.opus`,
96 kb/s, by ffmpeg inside build-web.py when the .opus is missing or the .m4a changed): JUCE decodes
Opus itself on both systems, while AAC (.m4a) depends on the OS codecs and is not reliable on
Windows.

The launcher's version is `LAUNCHER_VERSION` in `tools/launcher/CMakeLists.txt`. The website shows
the Launcher page, the nav link and the line under each plugin's downloads only when
`web/downloads/AudioToolsLauncher-<version>-macOS.zip` and `-Windows.zip` both exist.

## Build and package

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target AudioToolsLauncher atlaunchertest
sh scripts/package-macos.sh launcher        # dist/ and web/downloads/AudioToolsLauncher-<v>-macOS.zip
python3 scripts/build-web.py                # manifest + pages
```

On Windows `scripts/build-windows.ps1` builds everything and packages the launcher as
`AudioToolsLauncher-<v>-Windows.zip`.

## Test

```sh
build/tools/launcher/atlaunchertest_artefacts/Release/atlaunchertest
```

Builds fake product zips and a local manifest in a temporary folder and checks versions
(1.0.10 > 1.0.9), checksum and size rejection, install into `LAUNCHER_INSTALL_ROOT`, the record,
update detection, format selection, manual-install detection, remove, the manifest's images and
demos and the asset cache. Optional: `LAUNCHER_TEST_REALZIP=web/downloads/<zip>` installs a real
product (executable bits, codesign verdict equal to ditto's); `LAUNCHER_TEST_HTTPS=<url of a zip on
the site>` with `LAUNCHER_TEST_HTTPS_SHA=<sha256>` tests the HTTPS download, a 404 and the image
cache against the live site.

## Environment variables (tests and screenshots)

| Variable | Effect |
|---|---|
| `LAUNCHER_MANIFEST=<file>` | Read a local manifest. Zip, image and clip URLs of the site are read from the same `web/` tree when present (`web/downloads/manifest.json` works offline as is). |
| `LAUNCHER_INSTALL_ROOT=<dir>` | Install under `<dir>/VST3`, `Components`, `Applications`, `VST3-System`; only that folder is searched. |
| `LAUNCHER_DATA_DIR=<dir>` | The launcher's own files. |
| `LAUNCHER_SNAPSHOT=<png>` `LAUNCHER_WAIT=<s>` `LAUNCHER_SIZE=WxH` | Save a 2x screenshot after the wait and quit. |
| `LAUNCHER_SETTINGS=1`, `LAUNCHER_PAGE=<id>`, `LAUNCHER_PLAY=<n>`, `LAUNCHER_SCROLL=<px>` | Open settings, a plugin page, play its clip n, scroll. |
| `LAUNCHER_INSTALL=<id,...>` | Press Install on those plugins. |
| `LAUNCHER_MUTE=1` | Silent playback (default with `LAUNCHER_SNAPSHOT`). |

## Not done yet

- No code signing or notarization (same as the plugins): macOS asks for right-click > Open the
  first time, Windows SmartScreen asks once.
- Windows was not built or run here; the Windows-only paths (per-user VST3, elevation) are written
  but untested.
- No Start menu shortcuts for the Windows standalone apps.
