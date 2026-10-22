#!/bin/sh
# Packages every Audio Tools plugin (VST3, LV2, Standalone) and the launcher for Linux x86_64 into dist/.
# Runs inside the Linux build container (scripts/build-linux.sh does it on the docker host and copies the zips
# back to web/downloads/). Same layout and INSTALL.txt style as scripts/package-macos.sh.
#   sh scripts/package-linux.sh            # all plugins and the launcher
#   sh scripts/package-linux.sh at101v     # only one
#   sh scripts/package-linux.sh launcher   # only the launcher
# BUILD=<dir> reads the binaries from another build folder (default: build), DIST=<dir> writes the zips there.
set -e
cd "$(dirname "$0")/.."
BUILD=${BUILD:-build}
DIST=${DIST:-dist}

# Sin símbolos: los .so y ejecutables quedan en un tercio (los símbolos dinámicos que buscan los hosts quedan).
strip_all() {
    find "$1" -type f \( -name '*.so' -o -perm -u+x \) -exec sh -c 'file -b "$1" | grep -q ELF && strip --strip-unneeded "$1"' _ {} \;
}

package() {
    DIR=$1; PRODUCT=$2
    UPPER=$(echo "$DIR" | tr a-z A-Z)
    VER=$(sed -n "s/^set(${UPPER}_VERSION \(.*\))/\1/p" "plugins/$DIR/CMakeLists.txt")
    NAME="$PRODUCT-$VER-Linux"
    OUT="$DIST/$NAME"
    # También las versiones anteriores: build-linux.sh trae a web/downloads todo lo que hay en dist/.
    rm -rf "$DIST/$PRODUCT"-*-Linux "$DIST/$PRODUCT"-*-Linux.zip; mkdir -p "$OUT"
    R="$BUILD/plugins/$DIR"
    # Todas las variantes del plugin (p. ej. AT20V y AT20V FX). La standalone es un ejecutable sin extensión.
    for f in "$R"/*_artefacts/Release/VST3/*.vst3 "$R"/*_artefacts/Release/LV2/*.lv2 "$R"/*_artefacts/Release/Standalone/*; do
        cp -R "$f" "$OUT/"
    done
    # Herramientas de línea de comandos del producto (p. ej. lacquerrender).
    for t in lacquerrender halationrender tarnishrender at303render plumbrender thrumrender pallorrender; do
        for f in "$R/$t" "$R/${t}_artefacts/Release/$t"; do [ -f "$f" ] && [ -x "$f" ] && cp "$f" "$OUT/" && break; done
    done
    strip_all "$OUT"
    cp LICENSE "$OUT/LICENSE.txt"
    cat > "$OUT/INSTALL.txt" <<TXT
$PRODUCT $VER by Audio Tools - Linux x86_64 (glibc 2.35 or later: Ubuntu 22.04, Debian 12, Fedora 36 or newer)

1. Copy the .vst3 folders to ~/.vst3/
2. Copy the .lv2 folders to ~/.lv2/
3. Rescan plugins in your DAW (REAPER: Options > Preferences > Plug-ins > VST > Re-scan).
$PRODUCT is the standalone version: run ./$PRODUCT (ALSA or JACK).

From a terminal, inside this folder:
  mkdir -p ~/.vst3 ~/.lv2 && cp -r *.vst3 ~/.vst3/ && cp -r *.lv2 ~/.lv2/

They use the libraries every Linux desktop has (ALSA, FreeType, fontconfig, X11; on Wayland, XWayland).

--- Español ---
1. Copiar las carpetas .vst3 a ~/.vst3/
2. Copiar las carpetas .lv2 a ~/.lv2/
3. Reescanear los plugins en el DAW. $PRODUCT es la versión standalone (./$PRODUCT, con ALSA o JACK).

Free and open source software under the GNU AGPL v3 (see LICENSE.txt).
TXT
    # Notas propias del producto (p. ej. Orrery: cómo rutear su MIDI).
    [ -f "plugins/$DIR/INSTALL-notes.txt" ] && cat "plugins/$DIR/INSTALL-notes.txt" >> "$OUT/INSTALL.txt"
    # zip de Info-ZIP: guarda los permisos (el bit de ejecución de la standalone y las herramientas).
    (cd "$DIST" && zip -qrX "$NAME.zip" "$NAME")
    ls -la "$DIST/$NAME.zip"
}

# The launcher (tools/launcher): one app that installs and updates the plugins from web/downloads/manifest.json.
package_launcher() {
    VER=$(sed -n "s/^set(LAUNCHER_VERSION \(.*\))/\1/p" tools/launcher/CMakeLists.txt)
    NAME="AudioToolsLauncher-$VER-Linux"
    OUT="$DIST/$NAME"
    rm -rf "$DIST"/AudioToolsLauncher-*-Linux "$DIST"/AudioToolsLauncher-*-Linux.zip; mkdir -p "$OUT"
    cp "$BUILD/tools/launcher/AudioToolsLauncher_artefacts/Release/Audio Tools Launcher" "$OUT/"
    strip_all "$OUT"
    cp LICENSE "$OUT/LICENSE.txt"
    cat > "$OUT/INSTALL.txt" <<TXT
Audio Tools Launcher $VER - Linux x86_64 (glibc 2.35 or later: Ubuntu 22.04, Debian 12, Fedora 36 or newer)

1. Keep "Audio Tools Launcher" anywhere (e.g. ~/.local/bin) and run it.
2. Pick the plugins and press Install. VST3 goes to ~/.vst3, LV2 to ~/.lv2, standalone apps to
   ~/.local/opt/audio-tools/<plugin>/ with an entry in the applications menu.

It uses libcurl for HTTPS (libcurl4 on Debian and Ubuntu, libcurl on Fedora), installed on almost every desktop.
No account and nothing to sign up for. The launcher only downloads the list of plugins
(https://audiotools.com.ar/downloads/manifest.json), the images and demos it shows, and the plugins you choose.

--- Español ---
1. Dejar "Audio Tools Launcher" en cualquier carpeta (p. ej. ~/.local/bin) y abrirlo.
2. Elegir los plugins y tocar Install.

Free and open source software under the GNU AGPL v3 (see LICENSE.txt).
TXT
    (cd "$DIST" && zip -qrX "$NAME.zip" "$NAME")
    ls -la "$DIST/$NAME.zip"
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
