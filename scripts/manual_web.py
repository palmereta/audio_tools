"""Manuales en la web: convierte plugins/<p>/docs/manual/{en,es}.md (el subconjunto de shared/manual/FORMAT.md,
el mismo que entiende el ManualView del plugin) en HTML con índice, y copia las capturas a
web/assets/img/<p>/manual/. Lo usa scripts/build-web.py."""
import html, os, re, shutil

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, "web")


def manual_dir(slug):
    return os.path.join(ROOT, "plugins", slug, "docs", "manual")


def has_manual(slug):
    return all(os.path.exists(os.path.join(manual_dir(slug), f)) for f in ("en.md", "es.md"))


def slugify(text):
    s = text.lower()
    for a, b in zip("áéíóúüñ", "aeiouun"): s = s.replace(a, b)
    return re.sub(r"[^a-z0-9]+", "-", s).strip("-")


def inline(text):
    """Escapa y aplica **negrita** y `código`. El código se protege primero para que no se le apliquen negritas."""
    parts = re.split(r"(`[^`]+`)", text)
    out = []
    for p in parts:
        if p.startswith("`") and p.endswith("`") and len(p) > 1:
            out.append(f"<code>{html.escape(p[1:-1])}</code>")
        else:
            out.append(re.sub(r"\*\*(.+?)\*\*", r"<strong>\1</strong>", html.escape(p)))
    return "".join(out)


def image_size(path):
    try:
        from PIL import Image
        with Image.open(path) as im: return im.size
    except Exception:
        return None


def copy_images(slug):
    src = os.path.join(manual_dir(slug), "img")
    dst = os.path.join(WEB, "assets", "img", slug, "manual")
    if not os.path.isdir(src): return
    os.makedirs(dst, exist_ok=True)
    for f in sorted(os.listdir(src)):
        if f.endswith(".jpg"):
            s, d = os.path.join(src, f), os.path.join(dst, f)
            if not os.path.exists(d) or os.path.getmtime(d) < os.path.getmtime(s): shutil.copy2(s, d)


def render(md, img_base, slug):
    """Devuelve (título, [(id, sección)], html)."""
    lines = md.splitlines()
    title, toc, out = "", [], []
    para, items, kind, table = [], [], None, []
    used = set()

    def flush():
        nonlocal para, items, kind, table
        if para: out.append(f"<p>{inline(' '.join(para))}</p>"); para = []
        if items:
            tag = "ol" if kind == "ol" else "ul"
            out.append(f"<{tag}>" + "".join(f"<li>{inline(i)}</li>" for i in items) + f"</{tag}>"); items, kind = [], None
        if table:
            rows = [[c.strip() for c in r.strip().strip("|").split("|")] for r in table]
            head, body = rows[0], [r for r in rows[1:] if not all(re.fullmatch(r":?-+:?", c or "-") for c in r)]
            out.append('<div class="m-table"><table><thead><tr>' + "".join(f"<th>{inline(c)}</th>" for c in head) + "</tr></thead><tbody>"
                       + "".join("<tr>" + "".join(f"<td>{inline(c)}</td>" for c in r) + "</tr>" for r in body) + "</tbody></table></div>")
            table = []

    for raw in lines:
        line = raw.rstrip()
        if not line.strip(): flush(); continue
        if line.startswith("|"):
            if para or items: flush()
            table.append(line); continue
        elif table: flush()
        m = re.match(r"^(#{1,3}) (.+)$", line)
        if m:
            flush()
            level, text = len(m.group(1)), m.group(2).strip()
            if level == 1: title = text; continue
            hid = base = slugify(text) or "s"
            n = 2
            while hid in used: hid = f"{base}-{n}"; n += 1
            used.add(hid)
            if level == 2: toc.append((hid, text))
            out.append(f'<h{level} id="{hid}">{inline(text)}</h{level}>'); continue
        m = re.match(r"^!\[(.*)\]\((img/[^)]+)\)$", line.strip())
        if m:
            flush()
            cap, rel = m.group(1), m.group(2)
            name = rel.split("/", 1)[1]
            size = image_size(os.path.join(manual_dir(slug), rel))
            wh = f' width="{size[0]}" height="{size[1]}"' if size else ""
            out.append(f'<figure class="m-shot"><a href="{img_base}{name}" target="_blank" rel="noopener">'
                       f'<img src="{img_base}{name}" alt="{html.escape(cap)}"{wh} loading="lazy" decoding="async"></a>'
                       f"<figcaption>{inline(cap)}</figcaption></figure>"); continue
        if line.startswith("> "):
            flush(); out.append(f'<aside class="m-note"><p>{inline(line[2:].strip())}</p></aside>'); continue
        m = re.match(r"^- (.+)$", line)
        if m:
            if para or kind == "ol": flush()
            kind = "ul"; items.append(m.group(1)); continue
        m = re.match(r"^\d+\. (.+)$", line)
        if m:
            if para or kind == "ul": flush()
            kind = "ol"; items.append(m.group(1)); continue
        if items and raw.startswith("  "):          # continuación de un ítem
            items[-1] += " " + line.strip(); continue
        if items: flush()
        para.append(line.strip())
    flush()
    return title, toc, "\n".join(out)


UI = {
    "en": {"manual": "User manual", "contents": "Contents", "back": "Back to {name}", "other": "Leer en español",
           "top": "Back to top", "desc": "The complete user manual of {name}, with screenshots: every control, presets, MIDI and sync, recipes and troubleshooting.",
           "inplugin": "The same manual is inside the plugin: open it with the Manual button in the top bar."},
    "es": {"manual": "Manual de uso", "contents": "Contenido", "back": "Volver a {name}", "other": "Read in English",
           "top": "Volver arriba", "desc": "El manual de uso completo de {name}, con capturas: cada control, presets, MIDI y sincronización, recetas y solución de problemas.",
           "inplugin": "El mismo manual está dentro del plugin: abrilo con el botón Manual de la barra de arriba."},
}


def page(slug, name, lang, shell):
    """Página /<slug>/manual/ (o /es/<slug>/manual/) con el shell de build-web.py."""
    copy_images(slug)
    md = open(os.path.join(manual_dir(slug), f"{lang}.md"), encoding="utf-8").read()
    root = "../../" if lang == "en" else "../../../"
    title, toc, body = render(md, f"{root}assets/img/{slug}/manual/", slug)
    u = UI[lang]
    product = "../"
    other = (f"{root}es/{slug}/manual/" if lang == "en" else f"{root}{slug}/manual/")
    toc_html = "".join(f'<li><a href="#{i}">{html.escape(t)}</a></li>' for i, t in toc)
    content = f"""  <main id="top" class="manual">
    <div class="wrap m-grid">
      <aside class="m-toc" aria-label="{u['contents']}">
        <a class="m-back" href="{product}">{u['back'].format(name=name)}</a>
        <details class="m-toc-d" open><summary class="m-toc-h">{u['contents']}</summary>
        <ol>{toc_html}</ol></details>
        <a class="m-other" href="{other}" hreflang="{'es' if lang == 'en' else 'en'}">{u['other']}</a>
      </aside>
      <article class="m-body">
        <h1>{html.escape(title)}</h1>
        <p class="m-lede">{u['inplugin']}</p>
{body}
        <p class="m-top"><a href="#top">{u['top']}</a></p>
      </article>
    </div>
  </main>"""
    return shell(lang, f"{slug}/manual/", f"{title} | Audio Tools", u["desc"].format(name=name), content,
                 {"at20v": "og-image.jpg"}.get(slug, f"{slug}/og-image.jpg"), "", f"p-{slug} manual-page")
