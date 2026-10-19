#!/bin/sh
# Film de la web de Lacquer: las tres tomas de tools/blender/lathe.py (cuadros JPEG bajados de pc50 a $1) encadenadas con
# fundidos de 0,8 s, con el master demo (Balanced) de sonido. Escribe web/assets/video/lacquer.mp4 y el póster.
#   sh plugins/lacquer/tools/mkfilm.sh /ruta/a/film
set -e
cd "$(dirname "$0")/../../.."
F=$1; T=/tmp/claude-501/lqfilm; mkdir -p $T
for s in orbit macro top; do
  ffmpeg -y -loglevel error -framerate 24 -i "$F/${s}_%04d.jpg" -vf "scale=1920:1080:flags=lanczos,format=yuv420p" -c:v libx264 -crf 14 -preset slow $T/$s.mp4
done
D=$(ffprobe -v error -show_entries format=duration -of csv=p=0 $T/orbit.mp4)
O1=$(python3 -c "print($D - 0.8)"); O2=$(python3 -c "print(2 * $D - 1.6)"); L=$(python3 -c "print(3 * $D - 1.6)")
build/plugins/lacquer/lacquerrender build/lacquer-demo/demo-mix.wav $T/master.wav --style balanced > /dev/null
ffmpeg -y -loglevel error -i $T/orbit.mp4 -i $T/macro.mp4 -i $T/top.mp4 -ss 4 -i $T/master.wav \
  -filter_complex "[0:v][1:v]xfade=transition=fade:duration=0.8:offset=$O1[a];[a][2:v]xfade=transition=fade:duration=0.8:offset=$O2,fade=t=in:st=0:d=0.6,fade=t=out:st=$(python3 -c "print($L - 0.8)"):d=0.8[v];[3:a]atrim=0:$L,afade=t=in:st=0:d=0.5,afade=t=out:st=$(python3 -c "print($L - 1.5)"):d=1.5[au]" \
  -map "[v]" -map "[au]" -c:v libx264 -b:v 3600k -maxrate 5000k -bufsize 8000k -preset slow -pix_fmt yuv420p -c:a aac -b:a 192k -movflags +faststart web/assets/video/lacquer.mp4
ffmpeg -y -loglevel error -i "$F/orbit_0060.jpg" -vf scale=1920:1080:flags=lanczos -q:v 3 web/assets/img/lacquer/film-poster.jpg
ls -la web/assets/video/lacquer.mp4 web/assets/img/lacquer/film-poster.jpg
