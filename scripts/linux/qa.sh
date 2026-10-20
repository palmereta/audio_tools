#!/bin/sh
# QA de Linux, dentro del contenedor de scripts/linux/Dockerfile (QA=1 sh scripts/build-linux.sh), sin pantalla (xvfb):
#   athosttest y pluginval (strictness 10, en proceso) sobre cada VST3, pluginval sobre cada LV2,
#   las pruebas unitarias que compila CMake, el launcher (prueba propia con un zip real) y cada standalone 3 s.
# Logs en $QA_DIR (por defecto /work/qa); build-linux.sh los copia a ../audiotools-private/qa/linux/.
#   sh scripts/linux/qa.sh            # todo
#   sh scripts/linux/qa.sh at101v     # un plugin
# Sale con 1 si algo falló. En athosttest se esperan como FAIL sólo los presets que tocan solos ("sigue sonando"):
# Endless Drift (AT20V y AT20V FX), Pulser Ritual y Arp Constellation (AT42).
B=${BUILD:-/work/build}
Q=${QA_DIR:-/work/qa}
DIST=${DIST:-/work/dist}
ONLY=${1:-}
PV_VERSION=${PV_VERSION:-v1.0.4}
mkdir -p "$Q/hosttest" "$Q/pluginval" "$Q/unit" "$Q/standalone" /work/tools
SUMMARY="$Q/summary.txt"
: > "$SUMMARY"
BAD=0
say() { echo "$*" | tee -a "$SUMMARY"; }

# pluginval: la release de Linux de github.com/Tracktion/pluginval, bajada una vez.
PV=/work/tools/pluginval
if [ ! -x "$PV" ]; then
    curl -fsSL -o /work/tools/pluginval.zip "https://github.com/Tracktion/pluginval/releases/download/$PV_VERSION/pluginval_Linux.zip" \
        && unzip -oq /work/tools/pluginval.zip -d /work/tools && chmod +x "$PV"
fi
HT="$B/tools/hosttest/athosttest_artefacts/Release/athosttest"

# ── plugins
for v in "$B"/plugins/${ONLY:-*}/*_artefacts/Release/VST3/*.vst3; do
    [ -d "$v" ] || continue
    n=$(basename "$v" .vst3)
    xvfb-run -a "$HT" "$v" --out "$Q/hosttest/out" > "$Q/hosttest/$n.txt" 2>&1
    fails=$(grep -c '^  \[FAIL\]' "$Q/hosttest/$n.txt"); warns=$(grep -c '^  \[WARN\]' "$Q/hosttest/$n.txt")
    passes=$(grep -c '^  \[PASS\]' "$Q/hosttest/$n.txt")
    other=$(grep '^  \[FAIL\]' "$Q/hosttest/$n.txt" | grep -vc 'sigue sonando')
    expected=""
    case "$n" in "AT20V"|"AT20V FX"|"AT42") expected=" (esperados: los presets que tocan solos)";; esac
    [ "$other" -gt 0 ] && BAD=1
    [ -z "$expected" ] && [ "$fails" -gt 0 ] && BAD=1
    say "athosttest $n: $passes PASS, $warns WARN, $fails FAIL$expected"
    grep '^  \[FAIL\]' "$Q/hosttest/$n.txt" | sed 's/^/    /' | tee -a "$SUMMARY"

    if [ -x "$PV" ]; then
        xvfb-run -a "$PV" --strictness-level 10 --validate-in-process --output-dir "$Q/pluginval" --validate "$v" > "$Q/pluginval/$n.vst3.txt" 2>&1
        rc=$?
        [ $rc -ne 0 ] && BAD=1
        say "pluginval $n.vst3: $([ $rc -eq 0 ] && echo SUCCESS || echo "FAILED ($rc)")"
    fi
done
for l in "$B"/plugins/${ONLY:-*}/*_artefacts/Release/LV2/*.lv2; do
    [ -d "$l" ] && [ -x "$PV" ] || continue
    n=$(basename "$l" .lv2)
    xvfb-run -a "$PV" --strictness-level 10 --validate-in-process --output-dir "$Q/pluginval" --validate "$l" > "$Q/pluginval/$n.lv2.txt" 2>&1
    rc=$?
    [ $rc -ne 0 ] && BAD=1
    say "pluginval $n.lv2: $([ $rc -eq 0 ] && echo SUCCESS || echo "FAILED ($rc)")"
done

# ── pruebas unitarias (lacquertest tarda: su referencia ×8 de pico verdadero es por fuerza bruta, ~20 min en el host)
for t in plumb/plumbtest tarnish/tarnishtest at303/at303test halation/halationtest lacquer/lacquertest; do
    d=${t%/*}; n=${t#*/}
    [ -z "$ONLY" ] || [ "$ONLY" = "$d" ] || continue
    (cd "$B/plugins/$d" && "./$n") > "$Q/unit/$n.txt" 2>&1
    rc=$?; [ $rc -ne 0 ] && BAD=1
    say "$n: $([ $rc -eq 0 ] && echo PASS || echo "FAIL ($rc)") ($(tail -1 "$Q/unit/$n.txt"))"
done
if [ -z "$ONLY" ] || [ "$ONLY" = lacquer ]; then
    # Necesita una mezcla: 45 s de AT303 con su secuenciador (at303render), sin archivos de afuera.
    "$B/plugins/at303/at303render_artefacts/Release/at303render" /tmp/qa-mix.wav --preset "Squelch Line" --seconds 45 --bpm 124 > /dev/null 2>&1
    "$B/plugins/lacquer/lacquerlivetest_artefacts/Release/lacquerlivetest" /tmp/qa-mix.wav > "$Q/unit/lacquerlivetest.txt" 2>&1
    rc=$?; [ $rc -ne 0 ] && BAD=1
    say "lacquerlivetest: $([ $rc -eq 0 ] && echo PASS || echo "FAIL ($rc)") ($(tail -1 "$Q/unit/lacquerlivetest.txt"))"
fi
if [ -z "$ONLY" ] || [ "$ONLY" = launcher ]; then
    LT="$B/tools/launcher/atlaunchertest_artefacts/Release/atlaunchertest"
    "$LT" > "$Q/unit/atlaunchertest.txt" 2>&1
    rc=$?; [ $rc -ne 0 ] && BAD=1
    say "atlaunchertest: $([ $rc -eq 0 ] && echo PASS || echo "FAIL ($rc)") ($(tail -1 "$Q/unit/atlaunchertest.txt"))"
    # Con un zip real de dist/: permisos, VST3 y LV2 en su lugar, versión del moduleinfo.json.
    z=$(ls "$DIST"/AT20V-*-Linux.zip 2>/dev/null | head -1)
    if [ -n "$z" ]; then
        LAUNCHER_TEST_REALZIP="$z" "$LT" > "$Q/unit/atlaunchertest-realzip.txt" 2>&1
        rc=$?; [ $rc -ne 0 ] && BAD=1
        say "atlaunchertest con $(basename "$z"): $([ $rc -eq 0 ] && echo PASS || echo "FAIL ($rc)") ($(tail -1 "$Q/unit/atlaunchertest-realzip.txt"))"
    fi
    # La app: lee el manifest del sitio por HTTPS (libcurl) y guarda una captura, y otra con los ajustes abiertos.
    LA="$B/tools/launcher/AudioToolsLauncher_artefacts/Release/Audio Tools Launcher"
    for s in home settings; do
        LAUNCHER_DATA_DIR=/tmp/atl-qa-data LAUNCHER_INSTALL_ROOT=/tmp/atl-qa-root LAUNCHER_SNAPSHOT="$Q/standalone/launcher-$s.png" LAUNCHER_WAIT=8 \
            LAUNCHER_SETTINGS=$([ $s = settings ] && echo 1 || echo 0) timeout 60 xvfb-run -a "$LA" > "$Q/standalone/launcher-$s.txt" 2>&1
        rc=$?; [ $rc -ne 0 ] && BAD=1
        say "launcher ($s): $([ $rc -eq 0 ] && echo "PASS, captura launcher-$s.png" || echo "FAIL ($rc)")"
    done
fi

# ── standalone: abre bajo xvfb, 3 s, SIGINT (JUCE lo toma como pedido de cierre) y tiene que salir con 0.
for e in "$B"/plugins/${ONLY:-*}/*_artefacts/Release/Standalone/*; do
    [ -f "$e" ] && [ -x "$e" ] || continue
    n=$(basename "$e")
    log="$Q/standalone/$(echo "$n" | tr ' ' '_').txt"
    xvfb-run -a sh -c '"$1" & p=$!; sleep 3; if kill -0 $p 2>/dev/null; then kill -INT $p; i=0; while kill -0 $p 2>/dev/null && [ $i -lt 100 ]; do sleep 0.1; i=$((i+1)); done; kill -0 $p 2>/dev/null && { kill -9 $p; echo "no cerró en 10 s"; exit 2; }; wait $p; exit $?; else wait $p; echo "terminó solo antes de 3 s: $?"; exit 3; fi' _ "$e" > "$log" 2>&1
    rc=$?; [ $rc -ne 0 ] && BAD=1
    say "standalone $n: $([ $rc -eq 0 ] && echo "PASS (3 s, cierre limpio)" || echo "FAIL ($rc), ver $(basename "$log")")"
done

say "QA $([ $BAD -eq 0 ] && echo OK || echo 'CON FALLAS')"
exit $BAD
