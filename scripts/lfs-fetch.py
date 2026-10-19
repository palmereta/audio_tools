#!/usr/bin/env python3
"""Reemplaza los punteros de Git LFS de web/ por los archivos reales, sin git-lfs.

En el servidor de Ploi el remoto es https://oauth2:<token>@gitlab.com/...; con ese token se
pide el lote a la API de LFS de GitLab y se baja cada objeto con el header que devuelve.
(git-lfs 3.8 recibe HTTP 400 de GitLab en esta configuración; el mismo pedido con urllib anda.)
"""
import base64, hashlib, json, os, re, subprocess, sys, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
POINTER = b"version https://git-lfs.github.com/spec/v1"

def pointers():
    for d, _, files in os.walk(os.path.join(ROOT, "web")):
        for f in files:
            p = os.path.join(d, f)
            if os.path.getsize(p) < 400:
                data = open(p, "rb").read()
                if data.startswith(POINTER):
                    oid = re.search(rb"oid sha256:([0-9a-f]{64})", data).group(1).decode()
                    size = int(re.search(rb"size (\d+)", data).group(1))
                    yield p, oid, size

def main():
    todo = list(pointers())
    if not todo:
        print("lfs-fetch: no hay punteros"); return
    remote = subprocess.check_output(["git", "-C", ROOT, "remote", "get-url", "origin"], text=True).strip()
    m = re.match(r"https://([^:]+):([^@]+)@(.+?)(?:\.git)?/?$", remote)
    if not m: sys.exit("lfs-fetch: el remoto no trae credenciales: " + re.sub(r"//[^@]+@", "//***@", remote))
    user, token, host_path = m.groups()
    auth = "Basic " + base64.b64encode(f"{user}:{token}".encode()).decode()
    body = json.dumps({"operation": "download", "transfers": ["basic"],
                       "objects": [{"oid": o, "size": s} for _, o, s in todo]}).encode()
    req = urllib.request.Request(f"https://{host_path}.git/info/lfs/objects/batch", data=body, method="POST", headers={
        "Authorization": auth, "Accept": "application/vnd.git-lfs+json",
        "Content-Type": "application/vnd.git-lfs+json", "User-Agent": "audiotools-deploy"})
    actions = {o["oid"]: o for o in json.load(urllib.request.urlopen(req))["objects"]}
    for path, oid, size in todo:
        obj = actions[oid]
        if "error" in obj: sys.exit(f"lfs-fetch: {path}: {obj['error']}")
        dl = obj["actions"]["download"]
        r = urllib.request.Request(dl["href"], headers={**dl.get("header", {}), "User-Agent": "audiotools-deploy"})
        tmp = path + ".lfs-tmp"
        h = hashlib.sha256()
        with urllib.request.urlopen(r) as src, open(tmp, "wb") as dst:
            while chunk := src.read(1 << 20):
                h.update(chunk); dst.write(chunk)
        if h.hexdigest() != oid or os.path.getsize(tmp) != size:
            os.remove(tmp); sys.exit(f"lfs-fetch: {path}: el contenido no coincide con el oid")
        os.replace(tmp, path)
        print(f"lfs-fetch: {os.path.relpath(path, ROOT)} ({size / 1e6:.1f} MB)")

if __name__ == "__main__":
    main()
