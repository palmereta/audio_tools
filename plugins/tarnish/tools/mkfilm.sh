#!/bin/sh
# Film de la web de Tarnish: las tres tomas de tools/blender/deck.py (cuadros JPEG bajados de pc50 a $1) encadenadas
# con fundidos de 0,8 s. El sonido: una línea ácida propia (AT101V) por Dusty Cassette, y a los 12,5 s la cinta se
# para (tape stop), justo cuando la toma de arriba se aleja.
#   sh plugins/tarnish/tools/mkfilm.sh /ruta/a/film
set -e
cd "$(dirname "$0")/../../.."
F=$1; T=/tmp/claude-501/tnfilm; mkdir -p $T
for s in orbit heads top; do
  ffmpeg -y -loglevel error -framerate 24 -i "$F/${s}_%04d.jpg" -vf "scale=1920:1080:flags=lanczos,format=yuv420p" -c:v libx264 -crf 14 -preset slow $T/$s.mp4
done
D=$(ffprobe -v error -show_entries format=duration -of csv=p=0 $T/orbit.mp4)
O1=$(python3 -c "print($D - 0.8)"); O2=$(python3 -c "print(2 * $D - 1.6)"); L=$(python3 -c "print(3 * $D - 1.6)")
ffmpeg -y -loglevel error -stream_loop -1 -i web/assets/samples/at101v/acid-line.m4a -t 17 -ar 48000 -ac 2 -c:a pcm_s24le $T/dry.wav
build/plugins/tarnish/tarnishrender_artefacts/Release/tarnishrender $T/dry.wav $T/wet.wav --preset "Dusty Cassette" --bpm 120 --tail 1 --at 12.5 stop=1 --set stop_time=1.6
ffmpeg -y -loglevel error -i $T/wet.wav -af "loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000" $T/wet16.wav
ffmpeg -y -loglevel error -i $T/orbit.mp4 -i $T/heads.mp4 -i $T/top.mp4 -i $T/wet16.wav \
  -filter_complex "[0:v][1:v]xfade=transition=fade:duration=0.8:offset=$O1[a];[a][2:v]xfade=transition=fade:duration=0.8:offset=$O2,fade=t=in:st=0:d=0.6,fade=t=out:st=$(python3 -c "print($L - 0.8)"):d=0.8[v];[3:a]atrim=0:$L,afade=t=in:st=0:d=0.3,afade=t=out:st=$(python3 -c "print($L - 1.0)"):d=1.0[au]" \
  -map "[v]" -map "[au]" -c:v libx264 -b:v 3600k -maxrate 5000k -bufsize 8000k -preset slow -pix_fmt yuv420p -c:a aac -b:a 192k -movflags +faststart web/assets/video/tarnish.mp4
mkdir -p web/assets/img/tarnish
ffmpeg -y -loglevel error -i "$F/orbit_0070.jpg" -vf scale=1920:1080:flags=lanczos -q:v 3 web/assets/img/tarnish/film-poster.jpg
ls -la web/assets/video/tarnish.mp4 web/assets/img/tarnish/film-poster.jpg
