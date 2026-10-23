#!/bin/sh
# Texturas de la madera del AT330: nogal negro de Poly Haven (CC0, https://polyhaven.com/a/black_walnut_veneer_02),
# color, rugosidad y normales a 4k. No van al repo (pesan ~25 MB): at330.py las busca en $AT330_TEX o, si no, en
# audiotools-private/at330-ref/textures/, y sin ellas usa el nogal procedural.
#   sh plugins/at330/tools/blender/fetch_textures.sh [carpeta]
set -e
D=${1:-${AT330_TEX:-$(cd "$(dirname "$0")/../../../../.." && pwd)/audiotools-private/at330-ref/textures}}
mkdir -p "$D"
B=https://dl.polyhaven.org/file/ph-assets/Textures/jpg/4k/black_walnut_veneer_02
for m in diff rough nor_gl; do
  f="black_walnut_veneer_02_${m}_4k.jpg"
  [ -f "$D/$f" ] || curl -fsSL -o "$D/$f" "$B/$f"
done
ls -la "$D"/black_walnut_veneer_02_*
