#!/usr/bin/env python3
"""Publica el código en el repo público de GitHub (github.com/palmereta/audio_tools), con sus versiones y builds.

Este repositorio (GitLab, privado) es el de desarrollo y sigue siendo la fuente de verdad. El público recibe una
exportación del árbol, no la historia: la historia de acá pesa más de 3 GB por los zips, videos y demos en Git LFS
de todas las versiones, y trae las fotos de referencia de los instrumentos originales (licencias CC, con logos).

Cada corrida:
  1. Arma el árbol público desde `--ref` (por defecto HEAD) sin `web/` (el sitio y los binarios), sin las fotos
     de `plugins/*/docs/ref/` (quedan sus CREDITS.md) y sin `.gitattributes` (LFS); el README.md se adapta (las
     descargas pasan a ser las Releases). El submódulo de JUCE va como submódulo.
  2. Si el árbol cambió, hace un commit encima del último público con el mensaje del commit de desarrollo y su
     hash ("Development commit: <sha>"): ese es el vínculo entre los dos repos. Lo empuja a `main`.
  3. Crea en GitHub el tag `<plugin>-v<versión>` (la versión de cada CMakeLists, más el launcher) que todavía no
     exista, sobre el commit publicado.
  4. Crea la Release de cada tag que no la tenga y le sube los zips de macOS, Windows y Linux de `web/downloads/`
     que falten (el de Linux, cuando exista: las releases anteriores lo reciben en la corrida siguiente). Si un zip es un puntero de LFS (clon sin `git lfs pull`), se avisa y se saltea.

El token sale de la variable GITHUB_TOKEN o, si no está, del `.env` de master-prompt (repo hermano). Nunca se
escribe en disco ni en la configuración de git: va como encabezado en cada llamado.

    python3 scripts/publish-github.py              # publicar
    python3 scripts/publish-github.py --dry-run    # mostrar qué haría, sin tocar GitHub
    python3 scripts/publish-github.py --ref plumb-v1.0.0
"""
import base64, json, os, re, subprocess, sys, tempfile, urllib.error, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OWNER, REPO = "palmereta", "audio_tools"
DESCRIPTION = "Free, open-source synthesizers and effects (VST3, Audio Unit, LV2, standalone) for macOS, Windows and Linux: AT20V, AT101V, AT42, AT700, ATFS1, ATMD, AT303, Orrery, Murmur, Halation, Tarnish, Plumb, Lacquer."
HOMEPAGE = "https://audiotools.com.ar"
WORK = os.path.join(ROOT, "build", "github-publish")          # clon de trabajo del repo público (build/ no se versiona)
AUTHOR = ("Alejandro Duarte", "alejandro.s.duarte@gmail.com")
DRY = "--dry-run" in sys.argv
REF = sys.argv[sys.argv.index("--ref") + 1] if "--ref" in sys.argv else "HEAD"


def token():
    t = os.environ.get("GITHUB_TOKEN")
    if not t:
        env = os.path.join(os.path.dirname(ROOT), "master-prompt", ".env")
        if os.path.exists(env):
            m = re.search(r"^GITHUB_TOKEN=(\S+)", open(env).read(), re.M)
            t = m.group(1).strip("'\"") if m else None
    if not t: sys.exit("publish-github: falta GITHUB_TOKEN (variable de entorno o master-prompt/.env)")
    return t


TOKEN = token()
AUTH_HEADER = "Authorization: Basic " + base64.b64encode(f"x-access-token:{TOKEN}".encode()).decode()


def git(*args, cwd=None, env=None, input=None, check=True):
    r = subprocess.run(["git", *args], cwd=cwd or ROOT, env={**os.environ, **(env or {})}, input=input,
                       capture_output=True, text=True)
    if check and r.returncode != 0: sys.exit(f"git {' '.join(args)}: {r.stderr.strip()}")
    return r.stdout.strip()


def wgit(*args, **kw):
    """git en el clon de trabajo, con el token como encabezado (no queda en .git/config)."""
    return git("-c", f"http.https://github.com/.extraheader={AUTH_HEADER}", *args, cwd=WORK, **kw)


def api(method, path, body=None, data=None, ctype="application/json", url=None):
    req = urllib.request.Request(url or f"https://api.github.com{path}", method=method,
                                 data=data if data is not None else (json.dumps(body).encode() if body is not None else None))
    req.add_header("Authorization", f"Bearer {TOKEN}")
    req.add_header("Accept", "application/vnd.github+json")
    req.add_header("X-GitHub-Api-Version", "2022-11-28")
    if data is not None or body is not None: req.add_header("Content-Type", ctype)
    try:
        with urllib.request.urlopen(req, timeout=600) as r:
            raw = r.read()
            return r.status, (json.loads(raw) if raw else None)
    except urllib.error.HTTPError as e:
        raw = e.read()
        try: return e.code, json.loads(raw)
        except Exception: return e.code, raw.decode(errors="replace")


# ── 1. El árbol público ──
def excluded(path):
    if path.startswith("web/") or path == ".gitattributes": return True
    m = re.match(r"plugins/[^/]+/docs/ref/(.+)$", path)
    return bool(m) and m.group(1) != "CREDITS.md"


def public_readme(text):
    """El README de desarrollo, con las descargas en las Releases y sin el sitio (que no se publica acá)."""
    url = f"https://github.com/{OWNER}/{REPO}"
    dl = (f"## Download\n\nPrebuilt packages for every plugin are attached to the [releases]({url}/releases) of this repository,\n"
          "one release per plugin version (tag `<plugin>-v<version>`), and on " + HOMEPAGE + ". Each plugin comes as\n"
          "`<Name>-<version>-macOS.zip` (macOS 11+, universal arm64 + x86_64), `<Name>-<version>-Windows.zip`\n"
          "(Windows 10/11 x64) and `<Name>-<version>-Linux.zip` (x86_64, glibc 2.35+, VST3 + LV2); each zip includes an `INSTALL.txt`. In hosts the plugins are named **Audio Tools <name>**\n"
          "(for example Audio Tools Tarnish), under the maker Audio Tools.\n\n")
    text = re.sub(r"## Download\n.*?(?=## Build from source)", lambda _: dl, text, flags=re.S)
    text = re.sub(r"## Website\n.*?(?=## Why open source)", lambda _: (
        "## Website\n\nThe website, " + HOMEPAGE + ", is generated by `scripts/build-web.py` with the demo clips of\n"
        "`scripts/samples.py`. Its folder (`web/`, with the downloads, films and clips) lives in the development\n"
        "repository and is not part of this one.\n\n"), text, flags=re.S)
    text = text.replace("git clone --recurse-submodules <repo>\ncd audiotools", f"git clone --recurse-submodules {url}.git\ncd {REPO}")
    text = re.sub(r"\nweb/ +static website[^\n]*", "", text)
    note = ("> This repository is published from the development repository with `scripts/publish-github.py`: each commit\n"
            "> here names the development commit it comes from. The website folder and the reference photographs of the\n"
            "> original instruments (`plugins/*/docs/ref/`, credited in their `CREDITS.md`) are not included.\n\n")
    return re.sub(r"(# Audio Tools\n\n)", lambda m: m.group(1) + note, text, count=1)


def build_tree():
    entries = git("ls-tree", "-r", "--full-tree", REF).splitlines()
    keep, dropped = [], 0
    for line in entries:
        meta, path = line.split("\t", 1)
        if excluded(path): dropped += 1; continue
        mode, kind, sha = meta.split()
        if path == "README.md":
            text = git("cat-file", "blob", sha)
            sha = git("hash-object", "-w", "--stdin", input=public_readme(text) + "\n")
        keep.append(f"{mode} {kind} {sha}\t{path}")
    idx = os.path.join(tempfile.mkdtemp(), "index")
    git("update-index", "--index-info", env={"GIT_INDEX_FILE": idx}, input="\n".join(keep) + "\n")
    tree = git("write-tree", env={"GIT_INDEX_FILE": idx})
    print(f"árbol público {tree[:12]}: {len(keep)} archivos ({dropped} quedan fuera)")
    return tree


# ── 2. El commit ──
# Lo nuevo de cada versión (scripts/release-notes.json, clave = tag), arriba de las notas de su release.
_rn = os.path.join(ROOT, "scripts", "release-notes.json")
RELEASE_NOTES = {k: v for k, v in json.load(open(_rn, encoding="utf-8")).items() if not k.startswith("_")} if os.path.exists(_rn) else {}


def versions():
    """{tag: (producto, versión)} de cada plugin y del launcher, leídos de sus CMakeLists en REF."""
    names = dict(re.findall(r"package (\w+) (\w+)", git("show", f"{REF}:scripts/package-macos.sh")))
    out = {}
    for d, product in names.items():
        cm = git("show", f"{REF}:plugins/{d}/CMakeLists.txt", check=False)
        m = re.search(rf"set\({d.upper()}_VERSION (.+?)\)", cm)
        if m: out[f"{d}-v{m.group(1)}"] = (product, m.group(1))
    m = re.search(r"set\(LAUNCHER_VERSION (.+?)\)", git("show", f"{REF}:tools/launcher/CMakeLists.txt"))
    if m: out[f"launcher-v{m.group(1)}"] = ("AudioToolsLauncher", m.group(1))
    return out


def main():
    full = git("rev-parse", REF)
    print(f"desarrollo: {REF} = {full[:12]}  ({git('log', '-1', '--format=%s', full)})")
    status, repo = api("GET", f"/repos/{OWNER}/{REPO}")
    if status == 404:
        print(f"el repo {OWNER}/{REPO} no existe: se crea público")
        if not DRY:
            status, repo = api("POST", "/user/repos", {"name": REPO, "description": DESCRIPTION, "homepage": HOMEPAGE,
                                                       "private": False, "has_wiki": False, "auto_init": False})
            if status != 201: sys.exit(f"no se pudo crear el repo: {status} {repo}")
    elif status != 200: sys.exit(f"GitHub respondió {status}: {repo}")
    elif isinstance(repo, dict) and repo.get("private"): sys.exit(f"{OWNER}/{REPO} es privado: este script publica en un repo público")

    tree = build_tree()
    if not os.path.isdir(os.path.join(WORK, ".git")):
        os.makedirs(WORK, exist_ok=True)
        git("init", "-q", "-b", "main", cwd=WORK)
        git("remote", "add", "origin", f"https://github.com/{OWNER}/{REPO}.git", cwd=WORK)
    # Los objetos del árbol público (sólo esos: ni la historia ni LFS) pasan al clon de trabajo en un paquete.
    if git("cat-file", "-t", tree, cwd=WORK, check=False) != "tree":
        objs = git("rev-list", "--objects", tree)
        pack = subprocess.run(["git", "pack-objects", "--stdout"], cwd=ROOT, input=objs.encode(), capture_output=True).stdout
        subprocess.run(["git", "index-pack", "--stdin", "--fix-thin"], cwd=WORK, input=pack, capture_output=True, check=True)
    remote_head = wgit("ls-remote", "origin", "refs/heads/main", check=False).split("\t")[0]
    parent = None
    if remote_head:
        wgit("fetch", "-q", "--no-tags", "origin", "main")
        parent = remote_head
    if parent and git("rev-parse", f"{parent}^{{tree}}", cwd=WORK) == tree:
        commit = parent
        print(f"main ya tiene este árbol: {commit[:12]}")
    else:
        subject = git("log", "-1", "--format=%s", full)
        body = git("log", "-1", "--format=%b", full)
        body = "\n".join(l for l in body.splitlines() if not l.startswith("Co-Authored-By:")).strip()
        msg = f"{subject}\n\n{body}\n\nDevelopment commit: {full}\n"
        when = git("log", "-1", "--format=%aI", full)
        env = {"GIT_AUTHOR_NAME": AUTHOR[0], "GIT_AUTHOR_EMAIL": AUTHOR[1], "GIT_COMMITTER_NAME": AUTHOR[0],
               "GIT_COMMITTER_EMAIL": AUTHOR[1], "GIT_AUTHOR_DATE": when}
        commit = git("commit-tree", tree, *(["-p", parent] if parent else []), cwd=WORK, env=env, input=msg)
        print(f"commit público {commit[:12]} sobre {parent[:12] if parent else '(primero)'}")
        if not DRY: wgit("push", "-q", "origin", f"{commit}:refs/heads/main")

    # ── 3 y 4. Tags y releases ──
    remote_tags = {l.split("\t")[1][len("refs/tags/"):] for l in wgit("ls-remote", "--tags", "origin", check=False).splitlines() if "\t" in l and not l.endswith("^{}")}
    for tag, (product, ver) in sorted(versions().items()):
        if tag not in remote_tags:
            print(f"tag {tag} → {commit[:12]}")
            if not DRY:
                wgit("tag", "-f", tag, commit); wgit("push", "-q", "origin", f"refs/tags/{tag}")
        status, rel = api("GET", f"/repos/{OWNER}/{REPO}/releases/tags/{tag}")
        if status == 404:
            title = f"{'Audio Tools Launcher' if product == 'AudioToolsLauncher' else product} {ver}"
            linux = os.path.exists(os.path.join(ROOT, "web", "downloads", f"{product}-{ver}-Linux.zip"))
            systems = ("macOS (11+, universal), Windows (10/11 x64) and Linux (x86_64, glibc 2.35+)" if linux
                       else "macOS (11+, universal) and Windows (10/11 x64)")
            notes = (f"{title} for {systems}. Unzip and follow INSTALL.txt, or install it "
                     f"with the Audio Tools Launcher. More on {HOMEPAGE}.\n\nThe builds are not code-signed yet: macOS may need "
                     "`xattr -dr com.apple.quarantine` on the installed files and Windows SmartScreen may ask to confirm.")
            changes = RELEASE_NOTES.get(tag)
            if changes: notes = "What's new:\n\n" + "\n".join(f"- {c}" for c in changes) + "\n\n" + notes
            print(f"release {tag}: {title}")
            if DRY: continue
            status, rel = api("POST", f"/repos/{OWNER}/{REPO}/releases", {"tag_name": tag, "name": title, "body": notes, "make_latest": "false"})
            if status != 201: print(f"  no se pudo crear: {status} {rel}"); continue
        elif status != 200: print(f"  release {tag}: GitHub respondió {status}"); continue
        assert isinstance(rel, dict)
        have = {a["name"] for a in rel.get("assets", [])}
        for plat in ("macOS", "Windows", "Linux"):
            name = f"{product}-{ver}-{plat}.zip"
            path = os.path.join(ROOT, "web", "downloads", name)
            if name in have: continue
            if not os.path.exists(path): print(f"  falta {name} en web/downloads"); continue
            with open(path, "rb") as f: head = f.read(2)
            if head != b"PK": print(f"  {name} es un puntero de LFS: correr `git lfs pull` y volver a publicar"); continue
            print(f"  sube {name} ({os.path.getsize(path) / 1e6:.1f} MB)")
            if DRY: continue
            up = rel["upload_url"].split("{")[0] + "?name=" + name
            status, res = api("POST", None, data=open(path, "rb").read(), ctype="application/zip", url=up)
            if status != 201: print(f"  falló la subida de {name}: {status} {str(res)[:200]}")
    print(f"listo: https://github.com/{OWNER}/{REPO}")


if __name__ == "__main__":
    main()
