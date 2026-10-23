#!/bin/sh
# Packages every Audio Tools plugin (VST3, AU, Standalone) and the launcher into dist/ and web/downloads/.
#   sh scripts/package-macos.sh            # all plugins and the launcher
#   sh scripts/package-macos.sh at101v     # only one
#   sh scripts/package-macos.sh launcher   # only the launcher
# BUILD=<dir> reads the binaries from another build folder (default: build).
# Each plugin zip carries Presets/<VST3 name>/: one .vstpreset per factory preset, written by atvstpresets
# (tools/hosttest) from the bundles just packaged and checked by loading every file back. Ableton Live ignores
# VST3 program lists, so without them Live only shows "Default".
set -e
cd "$(dirname "$0")/.."

package() {
    DIR=$1; PRODUCT=$2
    UPPER=$(echo "$DIR" | tr a-z A-Z)
    VER=$(sed -n "s/^set(${UPPER}_VERSION \(.*\))/\1/p" "plugins/$DIR/CMakeLists.txt")
    NAME="$PRODUCT-$VER-macOS"
    OUT="dist/$NAME"
    rm -rf "$OUT" "dist/$NAME.zip"; mkdir -p "$OUT" web/downloads
    R="${BUILD:-build}/plugins/$DIR"
    # Todas las variantes del plugin (p. ej. AT20V y AT20V FX).
    for f in "$R"/*_artefacts/Release/VST3/*.vst3 "$R"/*_artefacts/Release/AU/*.component "$R"/*_artefacts/Release/Standalone/*.app; do
        cp -R "$f" "$OUT/"
    done
    # Herramientas de línea de comandos del producto (p. ej. lacquerrender).
    for t in lacquerrender halationrender tarnishrender at303render plumbrender thrumrender pallorrender; do
        for f in "$R/$t" "$R/${t}_artefacts/Release/$t"; do [ -x "$f" ] && cp "$f" "$OUT/" && break; done
    done
    # Presets como archivos (.vstpreset) para los hosts que no muestran la lista de programas del VST3 (Live).
    PRESETS_EN=""; PRESETS_ES=""
    if [ -d "plugins/$DIR/assets/presets" ]; then
        for f in "$OUT"/*.vst3; do
            "${BUILD:-build}/tools/hosttest/atvstpresets_artefacts/Release/atvstpresets" "$f" --out "$OUT/Presets" \
                --factory "plugins/$DIR/assets/presets" --clean
        done
        PRESETS_EN="4. Factory presets as files, for Ableton Live and other hosts that list preset files: copy the
   folders inside Presets/ to ~/Library/Audio/Presets/Audio Tools/
   (In Live: Browser > Plug-Ins, unfold \"Audio Tools $PRODUCT\" to see them; the launcher does this for you.)
   REAPER then lists every factory preset twice (the plugin's own list and the files): both are the same.
   If you don't use Live, skip this step.
"
        PRESETS_ES="4. Presets de fábrica como archivos (Ableton Live y otros): copiar las carpetas de Presets/ a
   ~/Library/Audio/Presets/Audio Tools/
   REAPER muestra entonces cada preset dos veces (la lista del plugin y los archivos): son los mismos.
   Si no usás Live, este paso no hace falta.
"
    fi
    cp LICENSE "$OUT/LICENSE.txt"
    cat > "$OUT/INSTALL.txt" <<TXT
$PRODUCT $VER by Audio Tools - macOS 11+ (Apple Silicon and Intel)

1. Copy the .vst3 bundles to ~/Library/Audio/Plug-Ins/VST3/
2. Copy the .component bundles to ~/Library/Audio/Plug-Ins/Components/
3. Rescan plugins in your DAW (Ableton Live: Settings > Plug-Ins > Rescan).
${PRESETS_EN}$PRODUCT.app is the standalone version.

The binaries are ad-hoc signed and not notarized. If macOS blocks them, run:
  xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/$PRODUCT*.vst3 ~/Library/Audio/Plug-Ins/Components/$PRODUCT*.component

--- Español ---
1. Copiar los .vst3 a ~/Library/Audio/Plug-Ins/VST3/
2. Copiar los .component a ~/Library/Audio/Plug-Ins/Components/
3. Reescanear los plugins en el DAW. Si macOS los bloquea, usar el comando xattr de arriba.
${PRESETS_ES}
Free and open source software under the GNU AGPL v3 (see LICENSE.txt).
TXT
    # Notas propias del producto (p. ej. Orrery: cómo rutear su MIDI).
    [ -f "plugins/$DIR/INSTALL-notes.txt" ] && cat "plugins/$DIR/INSTALL-notes.txt" >> "$OUT/INSTALL.txt"
    (cd dist && ditto -c -k --keepParent "$NAME" "$NAME.zip")
    cp "dist/$NAME.zip" web/downloads/
    ls -la "web/downloads/$NAME.zip"
}

# The launcher (tools/launcher): one app that installs and updates the plugins from web/downloads/manifest.json.
package_launcher() {
    VER=$(sed -n "s/^set(LAUNCHER_VERSION \(.*\))/\1/p" tools/launcher/CMakeLists.txt)
    NAME="AudioToolsLauncher-$VER-macOS"
    OUT="dist/$NAME"
    rm -rf "$OUT" "dist/$NAME.zip"; mkdir -p "$OUT" web/downloads
    cp -R "${BUILD:-build}/tools/launcher/AudioToolsLauncher_artefacts/Release/Audio Tools Launcher.app" "$OUT/"
    cp LICENSE "$OUT/LICENSE.txt"
    cat > "$OUT/INSTALL.txt" <<TXT
Audio Tools Launcher $VER - macOS 11+ (Apple Silicon and Intel)

1. Move "Audio Tools Launcher.app" to Applications.
2. The first time, right-click it and choose Open (it is ad-hoc signed and not notarized).
   Or run: xattr -dr com.apple.quarantine "/Applications/Audio Tools Launcher.app"
3. Pick the plugins and press Install. VST3 and Audio Units go to ~/Library/Audio/Plug-Ins,
   standalone apps to ~/Applications, the factory presets (for Ableton Live's browser) to
   ~/Library/Audio/Presets/Audio Tools.

No account and nothing to sign up for. The launcher only downloads the list of plugins
(https://audiotools.com.ar/downloads/manifest.json), the images and demos it shows, and the plugins you choose.

--- Español ---
1. Mover "Audio Tools Launcher.app" a Aplicaciones.
2. La primera vez, clic derecho y Abrir (está firmado ad hoc y sin notarizar).
3. Elegir los plugins y tocar Install.

Free and open source software under the GNU AGPL v3 (see LICENSE.txt).
TXT
    (cd dist && ditto -c -k --keepParent "$NAME" "$NAME.zip")
    cp "dist/$NAME.zip" web/downloads/
    ls -la "web/downloads/$NAME.zip"
}

ONLY=${1:-}
[ -z "$ONLY" ] || [ "$ONLY" = at20v ] && package at20v AT20V
[ -z "$ONLY" ] || [ "$ONLY" = at101v ] && package at101v AT101V
[ -z "$ONLY" ] || [ "$ONLY" = at42 ] && package at42 AT42
[ -z "$ONLY" ] || [ "$ONLY" = at700 ] && package at700 AT700
[ -z "$ONLY" ] || [ "$ONLY" = atfs1 ] && package atfs1 ATFS1
[ -z "$ONLY" ] || [ "$ONLY" = orrery ] && package orrery Orrery
[ -z "$ONLY" ] || [ "$ONLY" = murmur ] && package murmur Murmur
[ -z "$ONLY" ] || [ "$ONLY" = atmd ] && package atmd ATMD
[ -z "$ONLY" ] || [ "$ONLY" = lacquer ] && package lacquer Lacquer
[ -z "$ONLY" ] || [ "$ONLY" = halation ] && package halation Halation
[ -z "$ONLY" ] || [ "$ONLY" = tarnish ] && package tarnish Tarnish
[ -z "$ONLY" ] || [ "$ONLY" = launcher ] && package_launcher
[ -z "$ONLY" ] || [ "$ONLY" = at303 ] && package at303 AT303
[ -z "$ONLY" ] || [ "$ONLY" = plumb ] && package plumb Plumb
[ -z "$ONLY" ] || [ "$ONLY" = pallor ] && package pallor Pallor
[ -z "$ONLY" ] || [ "$ONLY" = thrum ] && package thrum Thrum
exit 0
