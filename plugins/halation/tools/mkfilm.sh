#!/bin/sh
# Film de la web de Halation: las tres tomas de tools/blender/maquette.py (cuadros JPEG bajados de pc50 a $1) encadenadas
# con fundidos de 0,8 s. El sonido es un piano eléctrico propio (ATFS1) por Delay to Cathedral: Morph va de A (delay con
# puntillo) a B (catedral con shimmer) durante la segunda toma, y Freeze entra a los 12,4 s, cuando la imagen se congela.
# Escribe web/assets/video/halation.mp4 y el póster.
#   sh plugins/halation/tools/mkfilm.sh /ruta/a/film
set -e
cd "$(dirname "$0")/../../.."
F=$1; T=/tmp/claude-501/hafilm; mkdir -p $T
for s in echo space freeze; do
  ffmpeg -y -loglevel error -framerate 24 -i "$F/${s}_%04d.jpg" -vf "scale=1920:1080:flags=lanczos,format=yuv420p" -c:v libx264 -crf 14 -preset slow $T/$s.mp4
done
D=$(ffprobe -v error -show_entries format=duration -of csv=p=0 $T/echo.mp4)
O1=$(python3 -c "print($D - 0.8)"); O2=$(python3 -c "print(2 * $D - 1.6)"); L=$(python3 -c "print(3 * $D - 1.6)")
ffmpeg -y -loglevel error -i web/assets/samples/atfs1/tine-piano.m4a -t 12 -af "afade=t=out:st=11.2:d=0.8" -ar 48000 -ac 2 -c:a pcm_s24le $T/dry.wav
MORPH=$(python3 -c "print(' '.join(f'--at {5.4 + 0.2 * i:.1f} morph={100 * i / 23:.1f}' for i in range(24)))")
build/plugins/halation/halationrender_artefacts/Release/halationrender $T/dry.wav $T/wet.wav --preset "Delay to Cathedral" --bpm 92 --tail 8 $MORPH --at 12.4 on_freeze=1
ffmpeg -y -loglevel error -i $T/wet.wav -af "loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000" $T/wet16.wav
ffmpeg -y -loglevel error -i $T/echo.mp4 -i $T/space.mp4 -i $T/freeze.mp4 -i $T/wet16.wav \
  -filter_complex "[0:v][1:v]xfade=transition=fade:duration=0.8:offset=$O1[a];[a][2:v]xfade=transition=fade:duration=0.8:offset=$O2,fade=t=in:st=0:d=0.6,fade=t=out:st=$(python3 -c "print($L - 0.8)"):d=0.8[v];[3:a]atrim=0:$L,afade=t=in:st=0:d=0.3,afade=t=out:st=$(python3 -c "print($L - 1.5)"):d=1.5[au]" \
  -map "[v]" -map "[au]" -c:v libx264 -b:v 3600k -maxrate 5000k -bufsize 8000k -preset slow -pix_fmt yuv420p -c:a aac -b:a 192k -movflags +faststart web/assets/video/halation.mp4
mkdir -p web/assets/img/halation
ffmpeg -y -loglevel error -i "$F/space_0110.jpg" -vf scale=1920:1080:flags=lanczos -q:v 3 web/assets/img/halation/film-poster.jpg
ls -la web/assets/video/halation.mp4 web/assets/img/halation/film-poster.jpg
