#!/usr/bin/env python3
"""Website clips for DeVicenti: takes the seven sound-first demo clips of audiotools-private/vicenti/demos (rendered by
devicentirender from tools/demo_projects, written by clips.py; -16 LUFS, the dark ambient and ambient ones -20), encodes them
like the rest of the site (AAC 160 kb/s with a 1.5 s fade out, web/assets/samples/devicenti/<id>.m4a) and puts their
waveform and texts in web/assets/samples/samples.json (the earlier DeVicenti entries are removed), which scripts/build-web.py
reads (the .opus for the launcher is made by build-web.py). Each clip is also a factory preset of the same name.

    python3 plugins/devicenti/tools/web_samples.py [demos folder]
"""
import json, os, shutil, subprocess, sys
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))
sys.path.insert(0, os.path.join(ROOT, "scripts"))
from samples import peaks, OUT

DEMOS = "/Volumes/Docs/Projects/own/audiotools-private/vicenti/demos"
# (id, demo file, preset or project, title en, title es, desc en, desc es)
DEVICENTI = [
    ("low-swell", "01-low-swell", "Low Swell", "Pulsing bass", "Bajo que late",
     "No drums: a heavy driven bass that pulses on the eighths and slowly swells open, under a dark pad, breath-like noise and two far metallic hits thrown into the dub delay.",
     "Sin batería: un bajo pesado con drive que late en corcheas y se abre de a poco, debajo de un pad oscuro, ruido como un respiro y dos golpes metálicos lejanos tirados al delay dub."),
    ("half-light", "02-half-light", "Half-Light", "Half-time", "Half-time",
     "Slow and heavy: a soft deep kick and a big snare far back in the hall, the pulsing driven bass, a slow dark pad and metallic hits into the dub delay.",
     "Lento y pesado: un bombo grave y suave y un redoblante grande al fondo de la sala, el bajo con drive que late, un pad oscuro y lento y golpes metálicos al delay dub."),
    ("dub-throws", "03-dub-throws", "Dub Throws", "Dub delay throws", "Lanzamientos al delay dub",
     "A dark lo-fi stab sent to the dub delay with more send and feedback each time, until the echoes run away; a low bass underneath.",
     "Un stab oscuro con lo-fi mandado al delay dub con más envío y realimentación cada vez, hasta que los ecos se disparan; un bajo grave abajo."),
    ("slow-weight", "04-slow-weight", "Slow Weight", "Heavy pad", "Pad pesado",
     "No pulse: a cluster pad with four unison voices opens from 420 Hz to 1.7 kHz and closes again over a sub rumble and breath, in the void reverb.",
     "Sin pulso: un pad en cluster con cuatro voces de unísono se abre de 420 Hz a 1,7 kHz y se vuelve a cerrar sobre un rumble de sub y un respiro, en la reverb void."),
    ("frozen-tail", "05-frozen-tail", "Frozen Tail", "Freeze and shimmer", "Freeze y shimmer",
     "Chords into a long hall while the shimmer rises, then FREEZE holds the tail and glass bells play over the frozen room.",
     "Acordes a una sala larga mientras sube el shimmer, después FREEZE congela la cola y unas campanas de vidrio tocan sobre la sala congelada."),
    ("drive-and-fold", "06-drive-and-fold", "Drive and Fold", "Drive and fold", "Drive y fold",
     "One bass line, clean at first, with DRIVE turned up bar by bar and then FOLD on top; the sub under it stays clean. A soft kick on the one.",
     "Una sola línea de bajo, limpia al principio, con el DRIVE que sube compás a compás y después el FOLD encima; el sub de abajo queda limpio. Un bombo suave en el uno."),
    ("tape-chords", "07-tape-chords", "Tape Chords", "Chord memory and AGE", "Memorias de acorde y AGE",
     "Four chord-memory shapes (minor 9, major 9, minor 11, 7 flat 9) on a tine electric piano over a fretless bass, with AGE rising from clean to worn tape.",
     "Cuatro formas de las memorias de acorde (menor 9, mayor 9, menor 11, 7 con novena bemol) en un piano eléctrico tine sobre un bajo fretless, con el AGE que sube de limpio a cinta gastada."),
]

def main():
    src = sys.argv[1] if len(sys.argv) > 1 else DEMOS
    d = os.path.join(OUT, "devicenti"); os.makedirs(d, exist_ok=True)
    meta_path = os.path.join(OUT, "samples.json")
    meta = json.load(open(meta_path))
    index = {e["name"]: e for e in json.load(open(os.path.join(src, "index.json")))}
    for k in [k for k in meta if k.startswith("devicenti/")]:
        del meta[k]                                              # the earlier clips (the phase 1 song demos)
    keys = []
    for sid, did, preset, ten, tes, den, des in DEVICENTI:
        wav = os.path.join(src, did + ".wav")
        dur = float(subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", wav], capture_output=True, text=True).stdout)
        m4a = os.path.join(d, sid + ".m4a")
        subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", wav, "-af", f"afade=t=out:st={max(0.0, dur - 1.5):.2f}:d=1.5",
                        "-c:a", "aac", "-b:a", "160k", "-movflags", "+faststart", m4a], check=True)
        midi = os.path.join(src, did + ".mid")
        has_midi = os.path.exists(midi)
        if has_midi: shutil.copy2(midi, os.path.join(d, sid + ".mid"))
        meta[f"devicenti/{sid}"] = dict(plugin="devicenti", id=sid, preset=preset, title={"en": ten, "es": tes}, desc={"en": den, "es": des},
                                        secs=round(dur, 1), peaks=peaks(wav), kb=round(os.path.getsize(m4a) / 1024), nomidi=not has_midi)
        keys.append(f"devicenti/{sid}")
        print(f"devicenti {sid:15s} {dur:5.1f} s  {os.path.getsize(m4a) / 1024:5.0f} KB  {index.get(did, {}).get('lufs')} LUFS")
    meta["_order"] = [k for k in meta["_order"] if not k.startswith("devicenti/")] + keys
    json.dump(meta, open(meta_path, "w"), indent=1, ensure_ascii=False)

if __name__ == "__main__":
    main()
