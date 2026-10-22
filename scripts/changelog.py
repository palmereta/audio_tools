#!/usr/bin/env python3
"""El changelog de Audio Tools: qué cambió en cada versión de cada producto, sin fechas (pedido del usuario).

Fuente única: `scripts/release-notes.json` (inglés) y `scripts/release-notes.es.json` (castellano), con clave
`<plugin>-v<versión>` igual que los tags. Las mismas notas van:
  - a cada Release de GitHub (`scripts/publish-github.py`);
  - a `CHANGELOG.md` en la raíz (este script; GitHub lo muestra en el repo público);
  - a las páginas `/changelog/` y `/es/changelog/` del sitio (`scripts/build-web.py` importa `load`).

Entra cada versión con nota hasta la actual de su CMakeLists (una nota escrita de antemano para una versión que todavía
no se subió no aparece). Los plugins "In development" en la tabla del README quedan afuera, como en publish-github.

    python3 scripts/changelog.py           # escribe CHANGELOG.md
    python3 scripts/changelog.py --check   # falla si un tag o una versión actual no tiene nota en los dos idiomas
"""
import json, os, re, subprocess, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NOTES = {"en": os.path.join(ROOT, "scripts", "release-notes.json"),
         "es": os.path.join(ROOT, "scripts", "release-notes.es.json")}
# El orden del sitio (NAV_GROUPS de build-web.py): sintetizadores, después herramientas y efectos, y el launcher.
ORDER = ["at20v", "at101v", "at42", "at700", "atfs1", "atmd", "at303", "pallor", "thrum",
         "orrery", "murmur", "halation", "tarnish", "plumb", "lacquer", "launcher"]
NAMES = {"orrery": "Orrery", "murmur": "Murmur", "halation": "Halation", "tarnish": "Tarnish", "plumb": "Plumb",
         "lacquer": "Lacquer", "pallor": "Pallor", "thrum": "Thrum", "launcher": "Audio Tools Launcher"}
REPO_URL = "https://github.com/palmereta/audio_tools"
SITE_URL = "https://audiotools.com.ar"


def name(slug):
    return NAMES.get(slug, slug.upper())


def current_version(slug):
    path = (os.path.join(ROOT, "tools", "launcher", "CMakeLists.txt") if slug == "launcher"
            else os.path.join(ROOT, "plugins", slug, "CMakeLists.txt"))
    if not os.path.exists(path): return None
    m = re.search(rf"set\({slug.upper()}_VERSION (.+?)\)", open(path, encoding="utf-8").read())
    return m.group(1) if m else None


def in_development():
    readme = open(os.path.join(ROOT, "README.md"), encoding="utf-8").read()
    return set(re.findall(r"^\|[^\n]*\(plugins/([\w-]+)/\)[^\n]*\| In development \|\s*$", readme, flags=re.M))


def vkey(v):
    return tuple(int(x) for x in v.split("."))


def notes(lang):
    return {k: v for k, v in json.load(open(NOTES[lang], encoding="utf-8")).items() if not k.startswith("_")}


def load(lang="en", order=None):
    """[{slug, name, current, versions: [(versión, [puntos])]}] en el orden del sitio, versiones de la más nueva a la más vieja."""
    data, skip, out = notes(lang), in_development(), []
    for slug in order or ORDER:
        cur = current_version(slug)
        if not cur or slug in skip: continue
        vers = sorted((k.split("-v", 1)[1] for k in data if k.split("-v", 1)[0] == slug), key=vkey, reverse=True)
        vers = [v for v in vers if vkey(v) <= vkey(cur)]
        out.append({"slug": slug, "name": name(slug), "current": cur, "versions": [(v, data[f"{slug}-v{v}"]) for v in vers]})
    return out


def check():
    """Cada tag del repo y cada versión actual tienen nota en inglés y en castellano, con la misma cantidad de puntos."""
    tags = subprocess.run(["git", "tag"], cwd=ROOT, capture_output=True, text=True).stdout.split()
    skip = in_development()
    need = {t for t in tags if re.match(r"[\w-]+-v\d+\.\d+\.\d+$", t)}
    need |= {f"{s}-v{current_version(s)}" for s in ORDER if current_version(s) and s not in skip}
    en, es = notes("en"), notes("es")
    problems = [f"{k}: falta en {f}" for k in sorted(need) for f, d in (("release-notes.json", en), ("release-notes.es.json", es)) if k not in d]
    problems += [f"{k}: {len(en[k])} puntos en inglés y {len(es[k])} en castellano" for k in sorted(set(en) & set(es)) if len(en[k]) != len(es[k])]
    problems += [f"{k}: está en un solo idioma" for k in sorted(set(en) ^ set(es))]
    for p in problems: print(p)
    print(f"changelog: {len(need)} versiones publicadas o actuales, {len(problems)} problemas")
    return not problems


def markdown():
    products = load("en")
    lines = ["# Changelog", "",
             "What changed in each version of every Audio Tools plugin and of the launcher, newest version first.",
             f"Downloads are on the [releases]({REPO_URL}/releases) of this repository (one per version, tag",
             f"`<plugin>-v<version>`) and on {SITE_URL}, where this changelog is also published in English and Spanish.", "",
             "| Product | Current version |", "|---|---|"]
    lines += [f"| [{p['name']}](#{p['name'].lower().replace(' ', '-')}) | {p['current']} |" for p in products]
    for p in products:
        lines += ["", f"## {p['name']}"]
        for ver, items in p["versions"]:
            lines += ["", f"### {ver}", ""] + [f"- {i}" for i in items]
    lines += ["", "---", "",
              "Korg, MS-20, Roland, SH-101, System-700, Yamaha, FS1R, Moog, Minimoog, Elta Music and SOLAR 42 are trademarks",
              "of their owners. Audio Tools is an independent project, not affiliated with or endorsed by them; the names only",
              "identify the instruments that inspired these recreations.", ""]
    return "\n".join(lines)


if __name__ == "__main__":
    if "--check" in sys.argv:
        sys.exit(0 if check() else 1)
    path = os.path.join(ROOT, "CHANGELOG.md")
    open(path, "w", encoding="utf-8").write(markdown())
    print(f"CHANGELOG.md: {sum(len(p['versions']) for p in load('en'))} versiones de {len(load('en'))} productos")
