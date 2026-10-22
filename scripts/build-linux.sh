#!/bin/sh
# Builds and packages Audio Tools for Linux x86_64 (VST3, LV2, Standalone and the launcher) in Docker on the
# homelab's docker host, and copies the zips back to web/downloads/. Docker never runs on the Mac.
#   sh scripts/build-linux.sh              # every plugin and the launcher
#   sh scripts/build-linux.sh at101v       # only one (or: launcher)
#   QA=1 sh scripts/build-linux.sh         # also the Linux QA (scripts/linux/qa.sh), logs to ../audiotools-private/qa/linux/
# HOST=<ssh alias> (default: docker), REMOTE=<folder in the remote home> (default: audiotools-linux), JOBS=<n> (default: 3).
#
# En el host: ~/audiotools-linux/src (copia del árbol, por rsync), build/ (CMake + Ninja), dist/ (zips) y qa/.
# La imagen (scripts/linux/Dockerfile) es Ubuntu 22.04: glibc 2.35, para que los binarios corran en distros viejas.
set -e
cd "$(dirname "$0")/.."
HOST=${HOST:-docker}
REMOTE=${REMOTE:-audiotools-linux}
JOBS=${JOBS:-3}
ONLY=${1:-}
IMAGE=audiotools-linux:22.04

# Lo que no hace falta para compilar: binarios, el sitio, la historia, los renders crudos y las fotos de referencia.
ssh "$HOST" "mkdir -p $REMOTE/src $REMOTE/build $REMOTE/dist $REMOTE/qa"
rsync -a --delete --exclude='/build/' --exclude='/build-*/' --exclude='/dist/' --exclude='.git' --exclude='/web/' \
    --exclude='/.cache/' --exclude='/hosttest-out/' --exclude='plugins/*/assets/render/out*' --exclude='plugins/*/docs/ref' \
    --exclude='*.wav' --exclude='.DS_Store' --exclude='__pycache__' --exclude='external/JUCE/examples' \
    ./ "$HOST:$REMOTE/src/"
ssh "$HOST" "docker build -q -t $IMAGE $REMOTE/src/scripts/linux >/dev/null"

case "$ONLY" in
    "") TARGETS="" ;;
    launcher) TARGETS="tools/launcher/all" ;;
    *) TARGETS="plugins/$ONLY/all atvstpresets" ;;       # atvstpresets: los .vstpreset del zip (package-linux.sh)
esac
# qa.sh usa athosttest: con un plugin solo y una carpeta de build nueva no estaría compilado.
[ -n "$QA" ] && [ -n "$TARGETS" ] && TARGETS="$TARGETS athosttest"
# Con el usuario del host (los archivos de build/ y dist/ no quedan de root) y sin pasar de 6 GB: el host corre
# otros servicios (Postgres, Redis).
RUN="docker run --rm -u \$(id -u):\$(id -g) -e HOME=/tmp --memory=6g -v \$HOME/$REMOTE:/work -w /work/src $IMAGE"
# Un link por vez (pool "link"): los links con LTO de un plugin (VST3, LV2, standalone) piden ~2 GB cada uno y tres
# juntos pasaban los 6 GB (Orrery 1.0.6: "Killed signal terminated program lto1").
ssh "$HOST" "$RUN sh -c 'cmake -S /work/src -B /work/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_JOB_POOLS=link=1 -DCMAKE_JOB_POOL_LINK=link >/dev/null \
    && nice ninja -C /work/build -j$JOBS $TARGETS \
    && BUILD=/work/build DIST=/work/dist sh scripts/package-linux.sh $ONLY'"

if [ -n "$QA" ]; then
    ssh "$HOST" "$RUN sh scripts/linux/qa.sh $ONLY" || echo "QA: hubo fallas, ver los logs"
    mkdir -p ../audiotools-private/qa/linux
    rsync -a "$HOST:$REMOTE/qa/" ../audiotools-private/qa/linux/
fi

mkdir -p web/downloads
rsync -a --include='*-Linux.zip' --exclude='*' "$HOST:$REMOTE/dist/" web/downloads/
ls -la web/downloads/*-Linux.zip
