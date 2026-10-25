#!/usr/bin/env python3
"""DeviceNTI V: the C++ engine against the Python prototype (plan §7, phase 1: "port the prototype's patch values
first and compare spectra and envelopes: levels within 1 dB, envelope times within 5 %").

    /Volumes/Docs/Applications/pyaudio-venv/bin/python plugins/devicenti/tools/proto_compare.py [--build build-devicenti]

Each prototype patch (audiotools-private/vicenti/sound/proto/dv_patches.py) is translated to DeviceNTI V's keys and one
note is rendered by both: the prototype's offline kernel (4x) and the plugin through devicentirender (2x, no effects,
AGE off in both, no LFOs). Reported per patch: the median level difference over the frames above −40 dB, the time
to fall 20 dB from the peak, and the spectral centroid. Known differences by design: the drive curve (DeviceNTI V's
goes to 36 dB, the prototype's to 18 dB), the resonance curve and per-stage saturation of the ladder, and the
feedback's pitch limit (stricter at 2x).
"""
import json, os, subprocess, sys, tempfile
import numpy as np, soundfile as sf

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, "..", "..", ".."))
PROTO = os.path.abspath(os.path.join(ROOT, "..", "audiotools-private", "vicenti", "sound", "proto"))
sys.path.insert(0, HERE)
sys.path.insert(0, PROTO)
from presets import project, Pattern, kit_track, tone, kit, fxs  # noqa: E402

FT = {"LP24": 0, "LP12": 1, "BP": 2, "HP": 3, "OFF": 6}
HS = {"sine": 0, "saw": 1, "square": 2, "organ": 3, "reed": 4, "bell": 5}


def convert(s):
    """Prototype sound dict → DeviceNTI V sound keys (one-to-one where the semantics match)."""
    d = {"algo": s["algo"] - 1}
    for k, n in zip("abcd", "ABCD"):
        o = s["ops"][n]
        a, dd, su, r = o["env"]
        d.update({f"{k}_ratio": max(0.25, o["ratio"]), f"{k}_level": o["level"], f"{k}_fb": o["fb"], f"{k}_detune": o["detune"],
                  f"{k}_att": a * s["attack_scale"], f"{k}_dec": dd * s["decay_scale"], f"{k}_sus": su, f"{k}_rel": r * s["decay_scale"],
                  f"{k}_vel": o["vel"], f"{k}_ks": o["ks"], f"{k}_fixed": o["fixed"], f"{k}_hshape": HS[o["harm"][0]], f"{k}_hamt": o["harm"][1]})
    f = s["filter"]
    fa, fd, fs_, fr = f.get("adsr", (0.002, 0.3, 0.0, 0.2))
    d.update({"ftype": FT[f["type"]], "cutoff": f["cutoff"], "reso": f["reso"], "fenv": f["env"] / 7.0, "f_att": fa, "f_dec": fd, "f_sus": fs_, "f_rel": fr,
              "ktrk": f["ktrk"], "base": max(10.0, s["base"])})
    aa, ad, asu, ar = s["amp"]
    d.update({"amp_att": aa, "amp_dec": ad, "amp_sus": asu, "amp_rel": ar, "level": s["level_db"], "vel_amp": 0.0})
    # Prototype drive: gain 1 + 7·d (linear); ours: 36·d dB.
    if s["drive"] > 0:
        d["drive"] = 20 * np.log10(1 + 7 * s["drive"]) / 36.0
    d["fold"] = s["fold"]
    nz = s["noise"]
    d.update({"noise_level": nz["level"], "noise_decay": nz["decay"], "noise_freq": nz["freq"], "noise_q": nz["q"], "noise_mode": {0: 0, 1: 1, 2: 2}[nz["mode"]], "noise_att": nz["att"]})
    d.update({"penv_depth": s["pitch_env"][0], "penv_time": s["pitch_env"][1], "age": 0.0, "lowmono": 0, "voices": 8, "vmode": 0})
    return d


def proto_render(s, midi, gate):
    import dv_seq as S
    import dv_engine as E
    s = dict(s); s["age"] = 0.0; s["lfo"] = []; s["vintage"] = False
    st, y = S.render_note(s, [(0.0, midi)], 0.0, gate, 100, None, 1, 0.125)
    return y


def ours_render(R, d, midi, gate, tmp):
    P = Pattern(0, 64)
    P.trig(1, 0, notes=midi, vel=100, len=int(round(gate / 0.125 * 12)))
    proj = project("cmp", "x", 0, "", 120, 50, [kit_track(kit({})), tone(d), tone({"a_level": 0.0}), tone({"a_level": 0.0})], [P], fx=fxs())
    proj["params"].update({"tape": 0.0, "glue": 0.0, "limit": 0.0, "t1_level": -60.0, "t2_level": 0.0})
    pj = os.path.join(tmp, "p.json"); json.dump(proj, open(pj, "w"))
    wav = os.path.join(tmp, "o.wav")
    subprocess.run([R, wav, "--project", pj, "--seconds", str(gate + 0.05), "--tail", "4"], check=True, capture_output=True)
    x, sr = sf.read(wav)
    # The plugin pans constant-power: the centre is −3 dB per side, so the mono sum is L + R at 1/√2 each.
    return (x[:, 0] + x[:, 1]) / np.sqrt(2.0)


def env_db(y, sr=48000, hop=480):
    n = len(y) // hop
    return 10 * np.log10((y[: n * hop].reshape(n, hop) ** 2).mean(axis=1) + 1e-20)


def metrics(y, sr=48000):
    e = env_db(y)
    pk = int(np.argmax(e))
    after = np.where(e[pk:] < e[pk] - 20)[0]
    t20 = (after[0] if len(after) else len(e) - pk) * 0.01
    seg = y[: int(0.5 * sr)]
    sp = np.abs(np.fft.rfft(seg * np.hanning(len(seg)))) ** 2
    f = np.fft.rfftfreq(len(seg), 1 / sr)
    return e, t20, float((sp * f).sum() / (sp.sum() + 1e-20))


def main():
    build = sys.argv[sys.argv.index("--build") + 1] if "--build" in sys.argv else "build-devicenti"
    R = os.path.join(ROOT, build, "plugins", "devicenti", "devicentirender_artefacts", "Release", "devicentirender")
    import dv_patches as PP
    cases = [("Cold Bass", PP.cold_bass(), 33, 0.5), ("Glass Keys", PP.glass_keys(), 57, 1.0), ("Dust Choir", PP.dust_choir(), 57, 3.0),
             ("Glass Bell", PP.glass_bell(), 69, 0.5), ("Night Lead", PP.night_lead(), 64, 1.0), ("Polar Stab", PP.polar_stab(), 60, 0.4)]
    tmp = tempfile.mkdtemp()
    print(f"{'patch':12s} {'level diff (median)':>20s} {'T-20 proto / ours':>20s} {'centroid proto / ours':>24s}")
    out = []
    for name, s, midi, gate in cases:
        s = dict(s); s["vel"] = 100
        yp = proto_render(s, midi, gate)
        yo = ours_render(R, convert(s), midi, gate, tmp)
        n = min(len(yp), len(yo))
        yp, yo = yp[:n], yo[:n]
        ep, tp, cp = metrics(yp); eo, to, co = metrics(yo)
        m = min(len(ep), len(eo)); mask = ep[:m] > ep.max() - 40
        diff = float(np.median(eo[:m][mask] - ep[:m][mask]))
        out.append(dict(patch=name, level_diff_db=round(diff, 2), t20_proto=round(tp, 3), t20_ours=round(to, 3), centroid_proto=round(cp), centroid_ours=round(co)))
        print(f"{name:12s} {diff:+19.2f}  {tp:8.3f} / {to:<8.3f}  {cp:10.0f} / {co:<10.0f}")
    json.dump(out, open(os.path.join(tmp, "compare.json"), "w"), indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
