#!/bin/sh
# Film de la web del AT303: las tres tomas de tools/blender/scene303.py --mode film (orbit, knobs, pads; cuadros
# JPEG en $1) encadenadas con fundidos de 0,8 s. El sonido es el AT303 tocando Squelch Line a 128 BPM, el tempo al
# que se encienden los pads de la escena; en la toma de las perillas el corte se mueve como la perilla en pantalla.
#   sh plugins/at303/tools/mkfilm.sh /ruta/a/film
set -e
cd "$(dirname "$0")/../../.."
F=$1; T=/tmp/claude-501/a3film; mkdir -p $T
for s in orbit knobs pads; do
  ffmpeg -y -loglevel error -framerate 24 -i "$F/${s}_%04d.jpg" -vf "scale=1920:1080:flags=lanczos,format=yuv420p" -c:v libx264 -crf 14 -preset slow $T/$s.mp4
done
D=$(ffprobe -v error -show_entries format=duration -of csv=p=0 $T/orbit.mp4)
O1=$(python3 -c "print($D - 0.8)"); O2=$(python3 -c "print(2 * $D - 1.6)"); L=$(python3 -c "print(3 * $D - 1.6)")
# El corte de la toma de perillas: 42 % ± 30 % con período de 6 s, desde que empieza esa toma.
AT=$(python3 -c "
import math
o=[]; t0=$O1; t=0.0
while t < $D:
    o += ['--at', '%.2f' % (t0 + t), 'cutoff=%.1f' % (100 * (0.42 + 0.3 * math.sin(2 * math.pi * t / 6.0)))]; t += 0.1
o += ['--at', '%.2f' % (t0 + $D), 'cutoff=42']
print(' '.join(o))")
build/plugins/at303/at303render_artefacts/Release/at303render $T/wet.wav --preset "Squelch Line" --bpm 128 --seconds $(python3 -c "print($L + 1)") --set cutoff=42 $AT
ffmpeg -y -loglevel error -i $T/wet.wav -af "loudnorm=I=-16:TP=-1.5:LRA=11,aresample=48000" $T/wet16.wav
ffmpeg -y -loglevel error -i $T/orbit.mp4 -i $T/knobs.mp4 -i $T/pads.mp4 -i $T/wet16.wav \
  -filter_complex "[0:v][1:v]xfade=transition=fade:duration=0.8:offset=$O1[a];[a][2:v]xfade=transition=fade:duration=0.8:offset=$O2,fade=t=in:st=0:d=0.6,fade=t=out:st=$(python3 -c "print($L - 0.8)"):d=0.8[v];[3:a]atrim=0:$L,afade=t=in:st=0:d=0.3,afade=t=out:st=$(python3 -c "print($L - 1.0)"):d=1.0[au]" \
  -map "[v]" -map "[au]" -c:v libx264 -b:v 3600k -maxrate 5000k -bufsize 8000k -preset slow -pix_fmt yuv420p -c:a aac -b:a 192k -movflags +faststart web/assets/video/at303.mp4
mkdir -p web/assets/img/at303
ffmpeg -y -loglevel error -i "$F/orbit_0070.jpg" -vf scale=1920:1080:flags=lanczos -q:v 3 web/assets/img/at303/film-poster.jpg
ls -la web/assets/video/at303.mp4 web/assets/img/at303/film-poster.jpg
