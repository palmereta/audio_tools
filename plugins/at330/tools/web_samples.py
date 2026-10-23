#!/usr/bin/env python3
"""Clips de la web del AT330: toma ocho de los demos de audiotools-private/at330/demos (ya a −16 LUFS, hechos con
plugins/at330/tools/demos.py), los codifica como el resto de la web (AAC 160 kb/s con un fundido de salida de 1,5 s,
web/assets/samples/at330/<id>.m4a) y agrega su forma de onda y sus textos a web/assets/samples/samples.json, que lee
scripts/build-web.py (el .opus del launcher lo genera build-web.py). Los clips de vocoder usan una voz sintética
hecha con tools/demo_voice.py: no hay voz grabada.

    python3 plugins/at330/tools/web_samples.py [carpeta de demos]
"""
import json, os, subprocess, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
from samples import peaks, OUT

DEMOS = "/Volumes/Docs/Projects/own/audiotools-private/at330/demos"
# (id, id del demo, preset, título en, título es, desc en, desc es)
AT330 = [
    ("strings", "01_strings", "Ensemble Strings", "Strings", "Cuerdas",
     "The string section, 4', through the four-BBD ensemble, on a slow minor progression.",
     "La sección de cuerdas, 4', por el ensemble de cuatro BBD, sobre una progresión menor lenta."),
    ("male-choir", "03_male_choir", "Male Choir 8'", "Male choir", "Coro masculino",
     "Male 8' voices through the seven-filter formant bank, with the vibrato arriving after its delay.",
     "Voces masculinas de 8' por el banco de siete filtros de formantes, con el vibrato que llega después de su retardo."),
    ("rain-on-neon", "06_rain_on_neon", "Rain on Neon", "Rain on Neon", "Rain on Neon",
     "Slow strings with male and female voices and a long release: wide minor chords with a melody above.",
     "Cuerdas lentas con voces masculinas y femeninas y una liberación larga: acordes menores abiertos con una melodía arriba."),
    ("vocoder", "07_vocoder", "Vocoder Ensemble (Audio In)", "Vocoder", "Vocoder",
     "The ten-band vocoder with the ensemble: a synthetic speaking voice over Am, F, C and G. No recorded voice: the voice is made by our own script.",
     "El vocoder de diez bandas con el ensemble: una voz hablada sintética sobre La menor, Fa, Do y Sol. No hay voz grabada: la voz la hace un script nuestro."),
    ("machine-voice", "08_machine_voice", "Machine Voice (Audio In)", "Machine voice", "Voz de máquina",
     "Fast band response and no ensemble: the synthetic voice counting over one held chord.",
     "Respuesta rápida de las bandas y sin ensemble: la voz sintética contando sobre un solo acorde sostenido."),
    ("rising-choir", "11_rising_choir", "Rising Choir", "Rising choir", "Coro ascendente",
     "Pitch Shift in Auto: every chord starts a fifth below and glides up to pitch.",
     "Pitch Shift en Auto: cada acorde empieza una quinta abajo y sube deslizando hasta la altura."),
    ("darkwave", "20_sketch_darkwave", "Night Shift Pad + Vocoder Ensemble", "Dark wave sketch", "Boceto de dark wave",
     "A dark wave sketch in A minor, 120 BPM: Pallor drums, a Thrum guitar, the AT330 pad from bar 9 and the vocoder with the synthetic voice from bar 17, mastered with Lacquer.",
     "Un boceto de dark wave en La menor, 120 BPM: batería de Pallor, una guitarra de Thrum, el pad del AT330 desde el compás 9 y el vocoder con la voz sintética desde el 17, masterizado con Lacquer."),
    ("ambient", "21_sketch_ambient", "Frozen Cathedral, Low Monks, Glass Hall", "Ambient sketch", "Boceto ambient",
     "An ambient sketch: Orrery generates the parts at 48 BPM and the AT330 plays them, a pad, a low male drone and a high cloud, mastered with Lacquer.",
     "Un boceto ambient: Orrery genera las partes a 48 BPM y el AT330 las toca, un pad, un bordón masculino grave y una nube aguda, masterizado con Lacquer."),
]

def main():
    src = sys.argv[1] if len(sys.argv) > 1 else DEMOS
    d = os.path.join(OUT, "at330"); os.makedirs(d, exist_ok=True)
    meta_path = os.path.join(OUT, "samples.json")
    meta = json.load(open(meta_path))
    index = json.load(open(os.path.join(src, "clips.json")))
    keys = []
    for sid, did, preset, ten, tes, den, des in AT330:
        wav = os.path.join(src, did + ".wav")
        dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", wav], capture_output=True, text=True).stdout)
        m4a = os.path.join(d, sid + ".m4a")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-af", f"afade=t=out:st={max(0.0, dur - 1.5):.2f}:d=1.5",
                        "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", m4a], check=True)
        meta[f"at330/{sid}"] = dict(plugin="at330", id=sid, preset=preset, title={"en": ten, "es": tes}, desc={"en": den, "es": des},
                                    secs=round(dur, 1), peaks=peaks(wav), kb=round(os.path.getsize(m4a) / 1024), nomidi=True)
        keys.append(f"at330/{sid}")
        print(f"at330 {sid:14s} {dur:5.1f} s  {os.path.getsize(m4a) / 1024:5.0f} KB  {index.get(did, {}).get('lufs')} LUFS")
    meta["_order"] = [k for k in meta["_order"] if not k.startswith("at330/")] + keys
    json.dump(meta, open(meta_path, "w"), indent=1, ensure_ascii=False)

if __name__ == "__main__":
    main()
