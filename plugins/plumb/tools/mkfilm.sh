#!/bin/sh
# Film de la web de Plumb: las tres tomas de tools/blender/plumb.py --mode film (swing, slate, orbit; cuadros JPEG en
# $1) encadenadas con fundidos de 0,8 s. El sonido es el clip de demostración Natural Lead (web/assets/samples): la voz
# sintética propia, primero como se cantó y después corregida.
#   sh plugins/plumb/tools/mkfilm.sh /ruta/a/film
set -e
cd "$(dirname "$0")/../../.."
F=$1; T=/tmp/claude-501/plfilm; mkdir -p $T
for s in swing slate orbit; do
  ffmpeg -y -loglevel error -framerate 24 -i "$F/${s}_%04d.jpg" -vf "scale=1920:1080:flags=lanczos,format=yuv420p" -c:v libx264 -crf 14 -preset slow $T/$s.mp4
done
D=$(ffprobe -v error -show_entries format=duration -of csv=p=0 $T/swing.mp4)
O1=$(python3 -c "print($D - 0.8)"); O2=$(python3 -c "print(2 * $D - 1.6)"); L=$(python3 -c "print(3 * $D - 1.6)")
ffmpeg -y -loglevel error -i $T/swing.mp4 -i $T/slate.mp4 -i $T/orbit.mp4 -i web/assets/samples/plumb/natural-lead.m4a \
  -filter_complex "[0:v][1:v]xfade=transition=fade:duration=0.8:offset=$O1[a];[a][2:v]xfade=transition=fade:duration=0.8:offset=$O2,fade=t=in:st=0:d=0.6,fade=t=out:st=$(python3 -c "print($L - 0.8)"):d=0.8[v];[3:a]atrim=0:$L,afade=t=in:st=0:d=0.3,afade=t=out:st=$(python3 -c "print($L - 1.0)"):d=1.0[au]" \
  -map "[v]" -map "[au]" -c:v libx264 -b:v 3600k -maxrate 5000k -bufsize 8000k -preset slow -pix_fmt yuv420p -c:a aac -b:a 192k -movflags +faststart web/assets/video/plumb.mp4
mkdir -p web/assets/img/plumb
ffmpeg -y -loglevel error -i "$F/swing_0040.jpg" -vf scale=1920:1080:flags=lanczos -q:v 3 web/assets/img/plumb/film-poster.jpg
ls -la web/assets/video/plumb.mp4 web/assets/img/plumb/film-poster.jpg
