#!/bin/sh
# Deploy del sitio estático en Ploi (audiotools.com.ar, carpeta pública /web).
# Script de Ploi:
#   cd /home/audiotools-aopch/audiotools.com.ar
#   git checkout -- web          # restaura los punteros de LFS para que el pull no choque
#   git pull origin main
#   sh scripts/deploy-site.sh
#
# El clon de Ploi no trae los archivos de Git LFS (zips de descarga y video): quedan los punteros
# de ~130 bytes. scripts/lfs-fetch.py los reemplaza usando la API de LFS de GitLab.
set -e
cd "$(dirname "$0")/.."
python3 scripts/lfs-fetch.py
if grep -rl "^version https://git-lfs.github.com/spec/v1" web 2>/dev/null | grep -q .; then
    echo "ERROR: quedaron punteros de LFS en web/"; exit 1
fi
echo "audiotools.com.ar deployed"
