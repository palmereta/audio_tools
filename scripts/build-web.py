#!/usr/bin/env python3
"""Genera la web estática de Audio Tools en inglés (por defecto) y español, con cada instrumento
presentado como un lanzamiento (hero, highlights, film con reproductor propio, colores, specs):

    web/index.html, web/es/index.html               portada: la serie y por qué código abierto
    web/at20v/index.html, web/es/at20v/index.html   AT20V (Korg MS-20)
    web/at101v/index.html, web/es/at101v/index.html AT101V (Roland SH-101)

Una plantilla y un diccionario de textos por idioma y por página, para que las versiones no se
desincronicen. Versiones de los CMakeLists y tamaños de las descargas de web/downloads/.
"""
import datetime, hashlib, html, json, os, re, shutil, subprocess, zipfile
import manual_web

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, "web")

def version(plugin):
    cm = open(os.path.join(ROOT, "plugins", plugin, "CMakeLists.txt")).read()
    return re.search(rf"set\({plugin.upper()}_VERSION (.+?)\)", cm).group(1)

VERSION = version("at20v")
MAC = f"AT20V-{VERSION}-macOS.zip"
WIN = f"AT20V-{VERSION}-Windows.zip"
V101 = version("at101v")
MAC101 = f"AT101V-{V101}-macOS.zip"
WIN101 = f"AT101V-{V101}-Windows.zip"

def size_mb(name):
    p = os.path.join(WEB, "downloads", name)
    return f"{os.path.getsize(p) / 1e6:.0f} MB" if os.path.exists(p) else "—"

def preset_counts():
    cats = {}
    for f in os.listdir(os.path.join(ROOT, "plugins", "at101v", "assets", "presets")):
        if f.endswith(".json"):
            import json
            c = json.load(open(os.path.join(ROOT, "plugins", "at101v", "assets", "presets", f))).get("category", "")
            cats[c] = cats.get(c, 0) + 1
    return cats

PC = preset_counts()
N101 = sum(v for k, v in PC.items() if k != "Init")
V42 = version("at42")
MAC42 = f"AT42-{V42}-macOS.zip"
WIN42 = f"AT42-{V42}-Windows.zip"

def preset_counts42():
    import json
    cats = {}
    d = os.path.join(ROOT, "plugins", "at42", "assets", "presets")
    for f in os.listdir(d):
        if f.endswith(".json"):
            c = json.load(open(os.path.join(d, f))).get("category", "")
            cats[c] = cats.get(c, 0) + 1
    return cats

PC42 = preset_counts42()
N42 = sum(v for k, v in PC42.items() if k != "Init")

V700 = version("at700")
MAC700 = f"AT700-{V700}-macOS.zip"
WIN700 = f"AT700-{V700}-Windows.zip"

def preset_counts700():
    import json
    cats = {}
    d = os.path.join(ROOT, "plugins", "at700", "assets", "presets")
    for f in os.listdir(d):
        if f.endswith(".json"):
            c = json.load(open(os.path.join(d, f))).get("category", "")
            cats[c] = cats.get(c, 0) + 1
    return cats

PC700 = preset_counts700()
N700 = sum(v for k, v in PC700.items() if k != "Basics")
def pc700(*cats): return sum(PC700.get(c, 0) for c in cats)

VFS1 = version("atfs1")
MACFS1 = f"ATFS1-{VFS1}-macOS.zip"
WINFS1 = f"ATFS1-{VFS1}-Windows.zip"

def preset_countsfs1():
    import json
    cats = {}
    d = os.path.join(ROOT, "plugins", "atfs1", "assets", "presets")
    for f in os.listdir(d):
        if f.endswith(".json"):
            c = json.load(open(os.path.join(d, f))).get("category", "")
            cats[c] = cats.get(c, 0) + 1
    return cats

PCFS1 = preset_countsfs1()
NFS1 = sum(PCFS1.values())
def pcfs1(*cats): return sum(PCFS1.get(c, 0) for c in cats)
NFSEQ = len([f for f in os.listdir(os.path.join(ROOT, "plugins", "atfs1", "assets", "fseq")) if f.endswith(".json")])

VOR = version("orrery")
MACOR = f"Orrery-{VOR}-macOS.zip"
WINOR = f"Orrery-{VOR}-Windows.zip"
def preset_countsor():
    import json
    cats = {}
    d = os.path.join(ROOT, "plugins", "orrery", "assets", "presets")
    for f in os.listdir(d):
        if f.endswith(".json"):
            c = json.load(open(os.path.join(d, f))).get("category", "")
            cats[c] = cats.get(c, 0) + 1
    return cats
PCOR = preset_countsor()
NOR = sum(PCOR.values())

VMD = version("atmd")
MACMD = f"ATMD-{VMD}-macOS.zip"
WINMD = f"ATMD-{VMD}-Windows.zip"
def preset_countsmd():
    import json
    cats = {}
    d = os.path.join(ROOT, "plugins", "atmd", "assets", "presets")
    for f in os.listdir(d):
        if f.endswith(".json"):
            c = json.load(open(os.path.join(d, f))).get("category", "")
            cats[c] = cats.get(c, 0) + 1
    return cats
PCMD = preset_countsmd()
NMD = sum(v for k, v in PCMD.items() if k != "Init")

VMM = version("murmur")
MACMM = f"Murmur-{VMM}-macOS.zip"
WINMM = f"Murmur-{VMM}-Windows.zip"
def preset_countsmm():
    import json
    cats = {}
    d = os.path.join(ROOT, "plugins", "murmur", "assets", "presets")
    for f in os.listdir(d):
        if f.endswith(".json"):
            c = json.load(open(os.path.join(d, f))).get("category", "")
            cats[c] = cats.get(c, 0) + 1
    return cats
PCMM = preset_countsmm()
NMM = sum(PCMM.values())

VLQ = version("lacquer")
MACLQ = f"Lacquer-{VLQ}-macOS.zip"
WINLQ = f"Lacquer-{VLQ}-Windows.zip"

VHA = version("halation")
MACHA = f"Halation-{VHA}-macOS.zip"
WINHA = f"Halation-{VHA}-Windows.zip"

VTN = version("tarnish")
MACTN = f"Tarnish-{VTN}-macOS.zip"
WINTN = f"Tarnish-{VTN}-Windows.zip"

VA3 = version("at303")
MACA3 = f"AT303-{VA3}-macOS.zip"
WINA3 = f"AT303-{VA3}-Windows.zip"

VPL = version("plumb")
MACPL = f"Plumb-{VPL}-macOS.zip"
WINPL = f"Plumb-{VPL}-Windows.zip"

# ───────────────────────── AT20V (y textos de la portada sobre código abierto) ─────────────────────────
T = {
"en": dict(
    lang="en", other="es", other_label="Español", here_label="English",
    title_20="AT20V — a free Korg MS-20 recreation with a live patch bay | Audio Tools",
    h1_20="A semi-modular that screams, rebuilt in the open.",
    lead_20="AT20V is a free, open-source semi-modular synthesizer modeled circuit by circuit on the 1978 Korg MS-20, with its two filters, the full patch bay, a vocoder and polyphony.",
    title="Audio Tools — legendary synthesizers, rebuilt in the open",
    description="Free, open-source recreations of classic synthesizers. AT20V, the first one, is a semi-modular synth modeled on the 1978 Korg MS-20, for macOS, Windows and Linux.",
    h1="Legendary synthesizers, rebuilt in the open.",
    lead="Audio Tools recreates classic instruments as free, open-source plugins for macOS, Windows and Linux. The first one is AT20V, modeled circuit by circuit on the 1978 Korg MS-20.",
    dl_mac="Download for macOS", dl_win="Download for Windows",
    version_line=f"AT20V {VERSION}. VST3, Audio Unit and standalone app.",
    flow_label="Page contents",
    flow=[("#sound", "Sound engine", "Korg35 filters"), ("#patch", "Patch bay", "35 jacks"), ("#extensions", "Extensions", "vocoder, poly, FX"),
          ("#presets", "Presets", "51 sounds"), ("#download", "Download", "macOS, Windows"), ("#open-source", "Open source", "GNU AGPL v3")],
    sound_h="A filter that screams like the original.",
    sound_p1="The MS-20 gets its character from two small, misbehaving filters. AT20V models them from Tim Stinchcombe's circuit analysis and Korg's service manual, and solves their nonlinear feedback loop sample by sample instead of approximating it.",
    sound_p2="It was built with measurements, not guesswork: every figure below was checked against the numbers in the original documentation.",
    spec=[("Low-pass filter", "12 dB per octave, with the diode clipping that limits the resonance"),
          ("High-pass filter", "6 dB per octave, the real slope of the MS-20, not the textbook 12"),
          ("Calibration", "cutoff at 5 and peak at 10 self-oscillates at 500 Hz, as Korg calibrated it"),
          ("Filter revisions", "Rev 1 (Korg35): grit grows with the mixer level. Rev 2 (LM13600): clean passband"),
          ("Oscillators", "band-limited, with the integrator curve, ring modulation as XOR and PW that fades to silence"),
          ("Electrical conventions", "Hz/V keyboard, 1 V/oct and 2 oct/V modulation, S-trig triggers, high-note priority"),
          ("Oversampling", "the whole voice runs at 4x or 8x")],
    video_caption="The AT20V rendered in 3D. The sound is the engine itself playing Random Bleeps, a self-playing preset.",
    patch_h="Thirty-five jacks, all of them live.",
    patch_p1="Every input and output of the original patch panel works, including the hidden default connections: plug a cable and it replaces the internal route, exactly like the hardware. Cables hang with real physics, outputs take stacked plugs, and you can build feedback loops anywhere.",
    patch_p2="The External Signal Processor follows the pitch and level of a guitar or a voice and turns them into control voltages, so another instrument can play the synth.",
    patch_caption="The front panel and patch bay. Every knob and jack was rendered in Blender from measurements of the original instrument.",
    keys_caption="The 37-key keyboard with the control wheel and the momentary switch, both patchable.",
    ext_h="What the MS-20 never had.",
    ext_p="A second page adds what a modern studio expects, drawn in the same visual language as the original panel.",
    ext_facts=[("Vocoder.", "8 to 20 bands. Sing into the audio input and play chords; formant shift, resonance and sibilance included."),
               ("Polyphony.", "Mono like the original, or up to 12 voices with up to 4-voice unison, detune and stereo spread."),
               ("Tempo sync.", "The modulation generator locks to your DAW, so patches can play themselves in time."),
               ("MIDI out.", "Thru, or Patch mode, which turns the triggers of the patch bay into notes for other instruments."),
               ("Delay and reverb.", "A tempo-synced ping-pong delay and a plate reverb for the ambient presets."),
               ("AT20V FX.", "An effect version for audio tracks: put it on a vocal and drive it from a MIDI track.")],
    ext_caption="The Extensions page: vocoder, polyphony, delay, reverb and system settings.",
    presets_h="From the 1978 factory charts to patches that play themselves.",
    presets=[("14", "Factory", "Patches transcribed from the setting charts of the original owner's manual."),
             ("13", "Classics", "Basses, screaming leads, drums and effects."),
             ("11", "Self-playing", "Generative patterns synced to your DAW. Hold a key to transpose them."),
             ("9", "Ambient", "Slow polyphonic pads with delay and reverb."),
             ("4", "Vocoder", "Robot voices, choirs and whispers for the audio input.")],
    dl_h=f"Download AT20V {VERSION}.",
    dl_p="Free, no account, no activation, no telemetry.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit and standalone app",
    mac_steps=["Unzip the file.", "Copy <code>AT20V.vst3</code> and <code>AT20V FX.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy the two <code>.component</code> files to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Rescan plugins in your DAW. In Ableton Live: Settings, Plug-Ins, Rescan."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3 and standalone app",
    win_steps=["Unzip the file.", "Copy <code>AT20V.vst3</code> and <code>AT20V FX.vst3</code> to <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Rescan plugins in your DAW.", "<code>AT20V.exe</code> runs without a DAW."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed plugins, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps.",
    win_caption="The same plugin running on Windows.",
    os_h="Why open source.",
    os_p1="Synthesizers are cultural instruments. Records were made with them, sounds were designed on them, and people learned music on them. When a commercial plugin is discontinued or stops running on a new operating system, the songs and presets built with it can disappear too.",
    os_p2="Open source is how an instrument outlives the company that made it.",
    reasons=[("Preservation", "How a classic circuit behaves is knowledge that belongs to musicians. Written down in code that anyone can read, it cannot be locked away."),
             ("Learning", "Every filter equation, calibration constant and design decision is visible. A student can follow how an analog circuit becomes software."),
             ("Trust", "No accounts, no license servers, no telemetry. You can check exactly what runs in your studio."),
             ("Longevity", "Anyone can rebuild the plugin for the next operating system, in ten years or in thirty."),
             ("Collaboration", "Owners of the original hardware can measure, compare and correct the model. Each improvement is shared with everyone.")],
    license_p1="AT20V is free software released under the GNU Affero General Public License v3, the same license as the JUCE framework it is built on. You can use it, study it, change it and share it. If you distribute a modified version, you share your changes under the same terms.",
    license_p2="The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    next_h="One synth at a time.",
    next_p="AT20V is the first of a series. Each new instrument gets the same treatment: the original documentation, measurements, and a faithful model that anyone can study and improve.",
    footer_license="Free software under the GNU AGPL v3.",
    trademark="Korg and MS-20 are trademarks of Korg Inc. Audio Tools is an independent project, not affiliated with or endorsed by Korg or Arturia. Product names are used only to identify the historical instruments that inspired these recreations.",
    photo_credit="Panel layout measured from a public-domain (CC0) photograph by Wilfredor on Wikimedia Commons.",
    hero_alt="The AT20V synthesizer rendered in 3D: a charcoal front panel with black knobs, a patch bay with colored cables and a 37-key keyboard.",
    alt_synth="AT20V front panel with patch cables connected",
    alt_keys="AT20V keyboard with three keys pressed, the control wheel and the momentary switch",
    alt_ext="AT20V Extensions page with vocoder, polyphony, delay, reverb and system knobs",
    alt_close="Close-up of the AT20V patch bay with yellow, red and blue cables",
    alt_low="Low-angle view of the AT20V keyboard and front panel",
    alt_win="AT20V running on Windows",
),
"es": dict(
    lang="es", other="en", other_label="English", here_label="Español",
    title_20="AT20V — una recreación libre del Korg MS-20 con patchera real | Audio Tools",
    h1_20="Un semimodular que grita, reconstruido a la vista de todos.",
    lead_20="El AT20V es un sintetizador semimodular libre y de código abierto, modelado circuito por circuito sobre el Korg MS-20 de 1978, con sus dos filtros, la patchera completa, vocoder y polifonía.",
    title="Audio Tools — sintetizadores legendarios, reconstruidos a la vista de todos",
    description="Recreaciones libres y de código abierto de sintetizadores clásicos. El primero, AT20V, es un semimodular basado en el Korg MS-20 de 1978, para macOS, Windows y Linux.",
    h1="Sintetizadores legendarios, reconstruidos a la vista de todos.",
    lead="Audio Tools recrea instrumentos clásicos como plugins libres y de código abierto para macOS, Windows y Linux. El primero es el AT20V, modelado circuito por circuito sobre el Korg MS-20 de 1978.",
    dl_mac="Descargar para macOS", dl_win="Descargar para Windows",
    version_line=f"AT20V {VERSION}. VST3, Audio Unit y aplicación independiente.",
    flow_label="Contenido de la página",
    flow=[("#sound", "Motor de sonido", "filtros Korg35"), ("#patch", "Patchera", "35 jacks"), ("#extensions", "Extensiones", "vocoder, poli, efectos"),
          ("#presets", "Presets", "51 sonidos"), ("#download", "Descarga", "macOS, Windows"), ("#open-source", "Código abierto", "GNU AGPL v3")],
    sound_h="Un filtro que grita como el original.",
    sound_p1="El MS-20 saca su carácter de dos filtros chicos y rebeldes. El AT20V los modela a partir del análisis de circuito de Tim Stinchcombe y del manual de servicio de Korg, y resuelve su lazo de realimentación no lineal muestra por muestra en lugar de aproximarlo.",
    sound_p2="Se hizo midiendo, no adivinando: cada dato de esta lista se contrastó con los números de la documentación original.",
    spec=[("Filtro pasa bajos", "12 dB por octava, con el recorte de diodos que limita la resonancia"),
          ("Filtro pasa altos", "6 dB por octava, la pendiente real del MS-20 y no la de manual"),
          ("Calibración", "con el corte en 5 y el pico en 10 autooscila a 500 Hz, como lo calibraba Korg"),
          ("Revisiones del filtro", "Rev 1 (Korg35): ensucia más cuanto más nivel del mezclador. Rev 2 (LM13600): banda pasante limpia"),
          ("Osciladores", "limitados en banda, con la curva del integrador, ring como XOR y PW que llega al silencio"),
          ("Convenciones eléctricas", "teclado en Hz/V, modulación a 1 V/oct y 2 oct/V, disparos S-trig, prioridad de nota aguda"),
          ("Sobremuestreo", "toda la voz corre a 4x u 8x")],
    video_caption="El AT20V renderizado en 3D. El sonido es el propio motor tocando Random Bleeps, un preset que se toca solo.",
    patch_h="Treinta y cinco jacks, todos funcionando.",
    patch_p1="Cada entrada y salida de la patchera original funciona, incluidas las conexiones internas por defecto: al enchufar un cable reemplaza la ruta interna, igual que en el hardware. Los cables cuelgan con física real, las salidas aceptan fichas apiladas y se puede realimentar cualquier cosa.",
    patch_p2="El procesador de señal externa sigue la altura y el nivel de una guitarra o una voz y los convierte en tensiones de control, para que otro instrumento toque el sintetizador.",
    patch_caption="El panel frontal y la patchera. Cada perilla y cada jack se renderizó en Blender a partir de mediciones del instrumento original.",
    keys_caption="El teclado de 37 teclas con la rueda de control y el pulsador momentáneo, los dos patcheables.",
    ext_h="Lo que el MS-20 nunca tuvo.",
    ext_p="Una segunda página suma lo que espera un estudio actual, dibujada con el mismo lenguaje visual que el panel original.",
    ext_facts=[("Vocoder.", "De 8 a 20 bandas. Cantás por la entrada de audio y tocás acordes; con corrimiento de formantes, resonancia y sibilancia."),
               ("Polifonía.", "Mono como el original, o hasta 12 voces con unísono de hasta 4, desafinación y apertura estéreo."),
               ("Sincronía de tempo.", "El generador de modulación se engancha al DAW, así los presets pueden tocarse solos a tiempo."),
               ("Salida MIDI.", "Thru, o modo Patch, que convierte los disparos de la patchera en notas para otros instrumentos."),
               ("Delay y reverb.", "Un delay ping-pong sincronizado y una reverb de placa para los presets ambient."),
               ("AT20V FX.", "Una versión de efecto para pistas de audio: va en la voz y se toca desde una pista MIDI.")],
    ext_caption="La página de extensiones: vocoder, polifonía, delay, reverb y ajustes del sistema.",
    presets_h="De las cartas de fábrica de 1978 a patches que se tocan solos.",
    presets=[("14", "Fábrica", "Patches transcriptos de las cartas de ajuste del manual original."),
             ("13", "Clásicos", "Bajos, leads que gritan, percusión y efectos."),
             ("11", "Self-playing", "Patrones generativos sincronizados al DAW. Una tecla apretada los transpone."),
             ("9", "Ambient", "Pads polifónicos lentos con delay y reverb."),
             ("4", "Vocoder", "Voces robóticas, coros y susurros para la entrada de audio.")],
    dl_h=f"Descargá el AT20V {VERSION}.",
    dl_p="Gratis, sin cuenta, sin activación y sin telemetría.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit y aplicación independiente",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>AT20V.vst3</code> y <code>AT20V FX.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá los dos archivos <code>.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Reescaneá los plugins en tu DAW. En Ableton Live: Settings, Plug-Ins, Rescan."],
    win_h="Windows", win_req="Windows 10 u 11 de 64 bits.", win_fmt="VST3 y aplicación independiente",
    win_steps=["Descomprimí el archivo.", "Copiá <code>AT20V.vst3</code> y <code>AT20V FX.vst3</code> a <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Reescaneá los plugins en tu DAW.", "<code>AT20V.exe</code> funciona sin DAW."],
    file_label="archivo",
    dl_note="Los builds todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los plugins instalados, y SmartScreen de Windows puede pedir confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos.",
    win_caption="El mismo plugin funcionando en Windows.",
    os_h="Por qué código abierto.",
    os_p1="Los sintetizadores son instrumentos culturales. Con ellos se grabaron discos, se diseñaron sonidos y mucha gente aprendió música. Cuando un plugin comercial se discontinúa o deja de funcionar en un sistema operativo nuevo, las canciones y los presets hechos con él pueden desaparecer también.",
    os_p2="El código abierto es la forma en que un instrumento sobrevive a la empresa que lo hizo.",
    reasons=[("Preservación", "Cómo se comporta un circuito clásico es conocimiento que les pertenece a los músicos. Escrito en código que cualquiera puede leer, no se puede encerrar."),
             ("Aprendizaje", "Cada ecuación de filtro, cada constante de calibración y cada decisión de diseño está a la vista. Un estudiante puede seguir cómo un circuito analógico se vuelve software."),
             ("Confianza", "Sin cuentas, sin servidores de licencias, sin telemetría. Podés verificar exactamente qué corre en tu estudio."),
             ("Longevidad", "Cualquiera puede recompilar el plugin para el próximo sistema operativo, dentro de diez años o de treinta."),
             ("Colaboración", "Quien tiene el hardware original puede medir, comparar y corregir el modelo. Cada mejora se comparte con todos.")],
    license_p1="El AT20V es software libre publicado bajo la GNU Affero General Public License v3, la misma licencia del framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. Si distribuís una versión modificada, compartís tus cambios en los mismos términos.",
    license_p2="El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    next_h="Un sintetizador a la vez.",
    next_p="El AT20V es el primero de una serie. Cada instrumento nuevo recibe el mismo tratamiento: la documentación original, mediciones y un modelo fiel que cualquiera puede estudiar y mejorar.",
    footer_license="Software libre bajo la GNU AGPL v3.",
    trademark="Korg y MS-20 son marcas registradas de Korg Inc. Audio Tools es un proyecto independiente, sin relación ni aval de Korg ni de Arturia. Los nombres de productos se usan sólo para identificar los instrumentos históricos que inspiraron estas recreaciones.",
    photo_credit="La disposición del panel se midió sobre una fotografía de dominio público (CC0) de Wilfredor en Wikimedia Commons.",
    hero_alt="El sintetizador AT20V renderizado en 3D: panel frontal carbón con perillas negras, patchera con cables de colores y un teclado de 37 teclas.",
    alt_synth="Panel frontal del AT20V con cables de patcheo conectados",
    alt_keys="Teclado del AT20V con tres teclas apretadas, la rueda de control y el pulsador momentáneo",
    alt_ext="Página de extensiones del AT20V con perillas de vocoder, polifonía, delay, reverb y sistema",
    alt_close="Primer plano de la patchera del AT20V con cables amarillo, rojo y azul",
    alt_low="Vista en ángulo bajo del teclado y el panel del AT20V",
    alt_win="El AT20V funcionando en Windows",
),
}


# ───────────────────────── portada ─────────────────────────
H = {
"en": dict(
    title="Audio Tools — legendary synthesizers, rebuilt in the open",
    description="Free, open-source recreations of classic synthesizers for macOS, Windows and Linux: AT20V (Korg MS-20), AT101V (Roland SH-101), AT42 (Elta Music SOLAR 42), AT700 (Roland System-700) and ATFS1 (Yamaha FS1R).",
    h1="Legendary synthesizers and new tools, built in the open.",
    lead="Free, open-source plugins for macOS, Windows and Linux: classic instruments modeled from their original documentation and measurements, and tools of our own to play, process and master them. Anyone can read, check and improve the code.",
    see="See the instruments",
    inst_h="The instruments.",
    cards=[
        dict(slug="at20v", name="AT20V", year="1978", img="render-hero", imgdir="", ver=VERSION, mac=MAC, win=WIN,
             what="Semi-modular synthesizer modeled on the 1978 Korg MS-20.",
             body="Two filters that scream, a 35-jack patch bay with cables that hang, a vocoder for the audio input, polyphony and 51 presets.",
             alt="The AT20V rendered in 3D: a charcoal panel with black knobs and colored patch cables"),
        dict(slug="at101v", name="AT101V", year="1982", img="hero-grey", imgdir="at101v/", ver=V101, mac=MAC101, win=WIN101,
             what="Monophonic synthesizer modeled on the 1982 Roland SH-101.",
             body=f"The IR3109 four-pole filter, a sub-oscillator, an arpeggiator and a 100-step sequencer with a visual editor, accents, slides and {N101} presets.",
             alt="The AT101V rendered in 3D: a grey synthesizer with colored slider caps and a 32-key keyboard"),
        dict(slug="at42", name="AT42", year="2023", img="hero-cream", imgdir="at42/", ver=V42, mac=MAC42, win=WIN42,
             what="Ambient drone machine modeled on the Elta Music SOLAR 42.",
             body=f"Twenty drone oscillators with light orbs, two noise voices, two VCOs, dual Polivoks filters, a 13-cartridge effector, a touch keyboard, a 62-jack patch bay and {N42} presets.",
             alt="The AT42 rendered in 3D: a cream panel with coloured knobs, light orbs and a black touch keyboard"),
        dict(slug="at700", name="AT700", year="1976", img="hero", imgdir="at700/", ver=V700, mac=MAC700, win=WIN700,
             what="Modular synthesizer modeled on the 1976 Roland System-700.",
             body=f"The 17-module Main Console with its internal buses, the duophonic 701A keyboard, the 717A sequencer of 3 × 12 steps, 137 jacks, spring reverb, phaser and {N700} presets.",
             alt="The AT700 rendered in 3D: the black 717A sequencer standing on the Main Console, patched with coloured cables, behind a 61-key keyboard"),
        dict(slug="atfs1", name="ATFS1", year="1998", img="hero", imgdir="atfs1/", ver=VFS1, mac=MACFS1, win=WINFS1,
             what="Formant and FM synthesizer modeled on the 1998 Yamaha FS1R.",
             body=f"Sixteen operators per voice with eight spectral forms, 88 algorithms, Formant Sequences you can make from any recording, 32 voices with their filter, a working 1U front panel and {NFS1} presets.",
             alt="The ATFS1 rendered in 3D: a silver-blue rack unit with a yellow-green display and four black knobs"),
        dict(slug="atmd", name="ATMD", year="1970", img="hero", imgdir="atmd/", ver=VMD, mac=MACMD, win=WINMD,
             what="Monophonic synthesizer modeled on the Moog Minimoog Model D.",
             body=f"Three oscillators, the mixer that drives the transistor ladder filter, two contours, glide and the wheels, on a panel built to its real measurements, with polyphony, delay, reverb and {NMD} presets.",
             alt="The ATMD rendered in 3D: a black panel tilted up over a walnut case, behind a 44-key keyboard"),
        dict(slug="orrery", name="Orrery", year="2026", img="hero", imgdir="orrery/", ver=VOR, mac=MACOR, win=WINOR,
             what="A generative MIDI instrument for ambient, drone and calm music.",
             body=f"Eight lanes in orbit, each on its own MIDI channel, play drones, pads, melodies and clouds over a slow shared harmony. Seven moods, {NOR} presets, sessions with breathing pace, and direct output to hardware synths.",
             alt="A brass orrery rendered in 3D with a glowing sun and eight coloured planets"),
        dict(slug="murmur", name="Murmur", year="2026", img="hero", imgdir="murmur/", ver=VMM, mac=MACMM, win=WINMM,
             what="A real-time granular effect.",
             body=f"Up to 128 grains read the last 16 seconds of your sound and turn it into clouds, shimmering pads, frozen chords stretched for minutes and rhythmic stutters, drawn as a flock of birds. {NMM} presets and MIDI to play the cloud.",
             alt="A murmuration of starlings rendered in 3D over a low horizon at dusk"),
        dict(slug="lacquer", name="Lacquer", year="2026", img="hero", imgdir="lacquer/", ver=VLQ, mac=MACLQ, win=WINLQ,
             what="Automatic mastering, live on the master bus or from a file.",
             body="Press Learn or drop a mix: Lacquer measures it, balances the tone, controls the dynamics and hits the loudness of Spotify, Apple Music or a club, with a true-peak limiter and an A/B at the same loudness.",
             alt="A lacquer disc rendered in 3D on the platter of a cutting lathe, the cutting head with its red lamp over the freshly cut groove"),
        dict(slug="halation", name="Halation", year="2026", img="hero", imgdir="halation/", ver=VHA, mac=MACHA, win=WINHA,
             what="A delay and a reverb in one space.",
             body="One slider turns echoes on the beat into a room. Ten modules take it further: freeze, shimmer, reverse, grains, spectral smear, a tuned resonator, rooms that breathe. Drawn as light in a plaster room.",
             alt="A white plaster architectural model of a hall with columns, rendered in 3D, filled with warm and cold points of light"),
        dict(slug="tarnish", name="Tarnish", year="2026", img="hero", imgdir="tarnish/", ver=VTN, mac=MACTN, win=WINTN,
             what="Tape, cassette, vinyl, radio and phone.",
             body="Your sound through an old machine: magnetic tape saturation, wow and flutter, hiss, dropouts, crackle and scratches, AM fading and a phone line, plus tape stop and bit crushing. Every bit of wear is on screen, reaching the head as you hear it.",
             alt="A vintage open-reel tape recorder beside a turntable with a worn vinyl record and a stack of cassettes, on a dark walnut desk in warm light"),
        dict(slug="plumb", name="Plumb", year="2026", img="hero", imgdir="plumb/", ver=VPL, mac=MACPL, win=WINPL,
             what="Real-time pitch correction with 0.67 ms of latency.",
             body="Set the key and sing: Plumb moves each note onto pitch fast enough to monitor through, from a natural touch that keeps the vibrato to the hard, stepped effect. Formants stay in place, and the plumb bob hangs still when the note is right.",
             alt="A brass plumb bob rendered in 3D, hanging from a rail over a dark slate with two chalk lines of a sung phrase"),
        dict(slug="at303", name="AT303", year="1981", img="hero", imgdir="at303/", ver=VA3, mac=MACA3, win=WINA3,
             what="A bass line synthesizer with its own sequencer.",
             body="A diode ladder that squelches, accents that climb, slides that glide, and the original's two-list sequencer you can finally see. Plus an acid line generator, the classic circuit modifications and the circuit itself, live.",
             alt="A graphite bass line synthesizer rendered in 3D, its step pads lit amber, five large knobs and a one-octave keyboard"),
    ],
    about="About", dl_mac="macOS", dl_win="Windows", formats="VST3, Audio Unit and standalone app. Free.",
    hero_alt="Close-up of the blue AT101V rendered in 3D: sliders with black caps and the white AT101V logo",
    next_h="One synth at a time.",
    next_p="Each new instrument gets the same treatment: the original documentation, measurements, a faithful model and the features a modern studio expects. AT20V, AT101V, AT42, AT700, ATFS1 and ATMD are the first six; Orrery is the first tool that plays them, Murmur, Halation and Tarnish the first effects to process them, Plumb tunes the voice that sings over them, and Lacquer masters the result.",
),
"es": dict(
    title="Audio Tools — sintetizadores legendarios, reconstruidos a la vista de todos",
    description="Recreaciones libres y de código abierto de sintetizadores clásicos para macOS, Windows y Linux: el AT20V (Korg MS-20), el AT101V (Roland SH-101), el AT42 (Elta Music SOLAR 42), el AT700 (Roland System-700) y el ATFS1 (Yamaha FS1R).",
    h1="Sintetizadores legendarios y herramientas nuevas, hechos a la vista de todos.",
    lead="Plugins libres y de código abierto para macOS, Windows y Linux: instrumentos clásicos modelados a partir de su documentación original y de mediciones, y herramientas propias para tocarlos, procesarlos y masterizarlos. Cualquiera puede leer, verificar y mejorar el código.",
    see="Ver los instrumentos",
    inst_h="Los instrumentos.",
    cards=[
        dict(slug="at20v", name="AT20V", year="1978", img="render-hero", imgdir="", ver=VERSION, mac=MAC, win=WIN,
             what="Sintetizador semimodular basado en el Korg MS-20 de 1978.",
             body="Dos filtros que gritan, una patchera de 35 jacks con cables que cuelgan, vocoder por la entrada de audio, polifonía y 51 presets.",
             alt="El AT20V renderizado en 3D: panel carbón con perillas negras y cables de colores"),
        dict(slug="at101v", name="AT101V", year="1982", img="hero-grey", imgdir="at101v/", ver=V101, mac=MAC101, win=WIN101,
             what="Sintetizador monofónico basado en el Roland SH-101 de 1982.",
             body=f"El filtro de cuatro polos IR3109, sub-oscilador, arpegiador y un secuenciador de 100 pasos con editor visual, acentos, slides y {N101} presets.",
             alt="El AT101V renderizado en 3D: un sintetizador gris con capuchones de colores y teclado de 32 teclas"),
        dict(slug="at42", name="AT42", year="2023", img="hero-cream", imgdir="at42/", ver=V42, mac=MAC42, win=WIN42,
             what="Máquina de drones ambient basada en el Elta Music SOLAR 42.",
             body=f"Veinte osciladores drone con orbes de luz, dos voces de ruido, dos VCO, doble filtro Polivoks, un efector de 13 cartuchos, teclado táctil, una patchera de 62 jacks y {N42} presets.",
             alt="El AT42 renderizado en 3D: panel crema con perillas de colores, orbes de luz y un teclado táctil negro"),
        dict(slug="at700", name="AT700", year="1976", img="hero", imgdir="at700/", ver=V700, mac=MAC700, win=WIN700,
             what="Sintetizador modular basado en el Roland System-700 de 1976.",
             body=f"La consola principal de 17 módulos con sus buses internos, el teclado duofónico 701A, el secuenciador 717A de 3 × 12 pasos, 137 jacks, reverb de resorte, phaser y {N700} presets.",
             alt="El AT700 renderizado en 3D: el secuenciador 717A negro parado sobre la consola principal, con cables de colores, detrás de un teclado de 61 teclas"),
        dict(slug="atfs1", name="ATFS1", year="1998", img="hero", imgdir="atfs1/", ver=VFS1, mac=MACFS1, win=WINFS1,
             what="Sintetizador de formantes y FM basado en el Yamaha FS1R de 1998.",
             body=f"Dieciséis operadores por voz con ocho formas espectrales, 88 algoritmos, Formant Sequences que podés crear desde cualquier grabación, 32 voces con su filtro, un frente de 1U que funciona y {NFS1} presets.",
             alt="El ATFS1 renderizado en 3D: una unidad de rack gris azulada con una pantalla amarillo verdosa y cuatro perillas negras"),
        dict(slug="atmd", name="ATMD", year="1970", img="hero", imgdir="atmd/", ver=VMD, mac=MACMD, win=WINMD,
             what="Sintetizador monofónico modelado sobre el Moog Minimoog Model D.",
             body=f"Tres osciladores, el mezclador que satura el filtro escalera de transistores, dos contornos, glide y las ruedas, en un panel hecho con sus medidas reales, con polifonía, delay, reverb y {NMD} presets.",
             alt="El ATMD renderizado en 3D: un panel negro inclinado sobre un gabinete de nogal, detrás de un teclado de 44 teclas"),
        dict(slug="orrery", name="Orrery", year="2026", img="hero", imgdir="orrery/", ver=VOR, mac=MACOR, win=WINOR,
             what="Un instrumento MIDI generativo para ambient, drone y música tranquila.",
             body=f"Ocho carriles en órbita, cada uno en su canal MIDI, tocan drones, pads, melodías y nubes sobre una armonía lenta compartida. Siete climas, {NOR} presets, sesiones con ritmo de respiración y salida directa a sintes físicos.",
             alt="Un orrery de bronce renderizado en 3D con un sol encendido y ocho planetas de colores"),
        dict(slug="murmur", name="Murmur", year="2026", img="hero", imgdir="murmur/", ver=VMM, mac=MACMM, win=WINMM,
             what="Un efecto granular en tiempo real.",
             body=f"Hasta 128 granos leen los últimos 16 segundos de tu sonido y lo convierten en nubes, pads brillantes, acordes congelados estirados durante minutos y tartamudeos rítmicos, dibujados como una bandada. {NMM} presets y MIDI para tocar la nube.",
             alt="Una bandada de estorninos renderizada en 3D sobre un horizonte bajo al atardecer"),
        dict(slug="lacquer", name="Lacquer", year="2026", img="hero", imgdir="lacquer/", ver=VLQ, mac=MACLQ, win=WINLQ,
             what="Masterización automática, en vivo en el bus master o desde un archivo.",
             body="Apretás Learn o soltás una mezcla: Lacquer la mide, equilibra el tono, controla la dinámica y llega a la sonoridad de Spotify, Apple Music o un club, con un limitador de pico verdadero y un A/B a la misma sonoridad.",
             alt="Un disco de laca renderizado en 3D sobre el plato de un torno de corte, con el cabezal y su luz roja sobre el surco recién cortado"),
        dict(slug="halation", name="Halation", year="2026", img="hero", imgdir="halation/", ver=VHA, mac=MACHA, win=WINHA,
             what="Un delay y una reverb en un solo espacio.",
             body="Una barra convierte ecos sobre el pulso en una sala. Diez módulos la llevan más lejos: congelado, shimmer, reversa, granos, corrimiento espectral, un resonador afinado, salas que respiran. Dibujado como luz en una sala de yeso.",
             alt="Una maqueta de arquitectura de yeso blanco de una sala con columnas, renderizada en 3D, llena de puntos de luz tibios y fríos"),
        dict(slug="tarnish", name="Tarnish", year="2026", img="hero", imgdir="tarnish/", ver=VTN, mac=MACTN, win=WINTN,
             what="Cinta, casete, vinilo, radio y teléfono.",
             body="Tu sonido por una máquina vieja: saturación de cinta magnética, wow y flutter, soplido, caídas, crujido y rayas, el desvanecimiento de la AM y una línea de teléfono, más tape stop y reducción de bits. Todo el desgaste se ve en pantalla y llega a la cabeza cuando lo escuchás.",
             alt="Un grabador de carretes antiguo junto a un tocadiscos con un vinilo gastado y una pila de casetes, sobre un escritorio de nogal con luz cálida"),
        dict(slug="plumb", name="Plumb", year="2026", img="hero", imgdir="plumb/", ver=VPL, mac=MACPL, win=WINPL,
             what="Corrección de afinación en tiempo real con 0,67 ms de latencia.",
             body="Fijás la tonalidad y cantás: Plumb lleva cada nota a su altura lo bastante rápido como para monitorear a través de él, desde un toque natural que conserva el vibrato hasta el efecto duro, escalonado. Los formantes quedan en su lugar, y la plomada cuelga quieta cuando la nota está justa.",
             alt="Una plomada de bronce renderizada en 3D, colgando de un riel sobre una pizarra oscura con dos líneas de tiza de una frase cantada"),
        dict(slug="at303", name="AT303", year="1981", img="hero", imgdir="at303/", ver=VA3, mac=MACA3, win=WINA3,
             what="Un sintetizador de bajo con su propio secuenciador.",
             body="Una escalera de diodos que chilla, acentos que suben, slides que se deslizan y el secuenciador de dos listas del original, que por fin se ve. Más un generador de líneas ácidas, las modificaciones clásicas del circuito y el circuito mismo, en vivo.",
             alt="Un sintetizador de bajo de grafito renderizado en 3D, con los pads de los pasos encendidos en ámbar, cinco perillas grandes y un teclado de una octava"),
    ],
    about="Ver más", dl_mac="macOS", dl_win="Windows", formats="VST3, Audio Unit y aplicación independiente. Gratis.",
    hero_alt="Primer plano del AT101V azul renderizado en 3D: faders con capuchones negros y el logo AT101V en blanco",
    next_h="Un sintetizador a la vez.",
    next_p="Cada instrumento nuevo recibe el mismo tratamiento: la documentación original, mediciones, un modelo fiel y lo que espera un estudio actual. El AT20V, el AT101V, el AT42, el AT700, el ATFS1 y el ATMD son los seis primeros; Orrery es la primera herramienta que los toca, Murmur, Halation y Tarnish los primeros efectos que los procesan, Plumb afina la voz que canta encima, y Lacquer masteriza el resultado.",
),
}

# ───────────────────────── AT101V ─────────────────────────
S = {
"en": dict(
    title="AT101V — a free SH-101 recreation with a visual sequencer | Audio Tools",
    description=f"AT101V is a free, open-source monophonic synthesizer modeled on the 1982 Roland SH-101, with the IR3109 filter, arpeggiator, a 100-step sequencer with a visual editor and {N101} presets. For macOS, Windows and Linux.",
    h1="The bass line machine, rebuilt in the open.",
    lead="AT101V is a free, open-source monophonic synthesizer modeled on the 1982 Roland SH-101: one oscillator with a sub-oscillator, a four-pole filter that sings, an arpeggiator and a 100-step sequencer.",
    dl_mac="Download for macOS", dl_win="Download for Windows",
    version_line=f"AT101V {V101}. VST3, Audio Unit and standalone app.",
    flow_label="Page contents",
    flow=[("#sound", "Sound engine", "IR3109 filter"), ("#panel", "Panel", "every slider"), ("#sequencer", "Sequencer", "100 steps"),
          ("#extensions", "Extensions", "poly, sync, FX"), ("#presets", "Presets", f"{N101} sounds"), ("#download", "Download", "macOS, Windows")],
    sound_h="One oscillator, one filter, no shortcuts.",
    sound_p1="On paper the SH-101 is simple: a CEM3340 oscillator, a sub-oscillator made of two flip-flops and Roland's IR3109 four-pole filter. Its sound is in the details, and AT101V models them from the service notes and published circuit analysis.",
    sound_p2="Every value below was measured on the engine and compared with the original documentation.",
    spec=[("Filter", "IR3109 four-pole low-pass, 24 dB per octave, solved without a one-sample delay in the feedback loop"),
          ("Resonance", "diode-limited feedback that thins the bass like the original and self-oscillates as a sine tracking 1 V per octave"),
          ("Oscillator", "rising saw and pulse from one core; pulse width by hand, by the LFO or by the envelope, down to silence"),
          ("Sub-oscillator", "one or two octaves down, or the 25 % pulse, phase-locked to the oscillator like the flip-flops"),
          ("Envelope", "RC curves; the attack overshoots and stops at the peak. Attack 1.5 ms to 4 s, decay and release up to 10 s"),
          ("Keyboard logic", "last-note or low-note priority set by the trigger switch, Auto portamento only when legato"),
          ("Oversampling", "the whole voice at 2x, 4x or 8x")],
    video_caption="AT101V rendered in 3D. The sound is the engine itself playing Acid Line, one of the sequence presets.",
    panel_h="Every slider, every switch.",
    panel_p1="The front panel works the way it did in 1982. The modulator is also the clock of the arpeggiator and the sequencer, the bender pushes forward for vibrato and growl, HOLD sustains, and KEY TRANSPOSE shifts everything, a running sequence included.",
    panel_p2="The case, the keys and every control were rendered in Blender from measurements of the original, in the three classic body colours.",
    panel_caption="The front panel and the 32-key keyboard. Drag the faders, flick the switches, push the bender.",
    colours_caption="Grey, blue or red: the body colour is a setting.",
    seq_h="A sequencer you can see.",
    seq_p="The original records up to 100 steps from the keyboard without showing them. AT101V keeps that workflow and adds a step editor on its own page.",
    seq_facts=[("Note grid.", "Click to place a note, drag to paint, click again for a rest. Up to 100 steps on four pages."),
               ("Accent and slide.", "Accents open the filter and push the amplifier; slides glide into the next note without retriggering."),
               ("Probability.", "Each step can play 25, 50, 75 or 100 % of the time, so a pattern keeps moving."),
               ("Tools.", "Length, rotate, transpose by semitones or octaves, a random bass line generator and a mutate button."),
               ("Export.", "Save the pattern as a .mid file and drop it into your DAW."),
               ("Keys transpose.", "While a sequence runs, any key transposes it.")],
    seq_caption="The Sequencer page playing Acid Line: accents in orange, slides in green, the red LED marks the current step.",
    ext_h="What the SH-101 never had.",
    ext_p="A Settings page, drawn like the panel, adds what a modern studio asks for.",
    ext_facts=[("Polyphony and unison.", "Mono like the original, or up to 8 voices, with up to 4-voice unison, detune and stereo spread."),
               ("Host sync.", "The arpeggiator and the sequencer follow your DAW, with swing and gate length. Or keep the modulator as the clock."),
               ("Expression.", "Velocity to filter and amplifier, accent amount, slide time and arpeggios over up to four octaves."),
               ("Effects.", "Drive into the filter, a BBD-style chorus, a tempo-synced delay and a plate reverb."),
               ("MIDI.", "MIDI learn on every control, mod wheel as the bender push, sustain pedal as HOLD and MIDI out of every note the synth plays."),
               ("Presets.", "Save your own patches, sequence and settings included, or roll a random patch.")],
    ext_caption="The Settings page: voices, expression, clock, character, chorus, delay, reverb and system.",
    presets_h="Basses, leads and sequences that play themselves.",
    presets=[(PC.get("Bass", 0), "Bass", "Round 101 basses, rubbery ones and driven ones."),
             (PC.get("Lead", 0), "Lead", "Solo voices with portamento, PWM and the singing resonance."),
             (PC.get("Seq", 0), "Sequences", "Acid, electro, Berlin school and probability patterns. They play on their own."),
             (PC.get("Arp", 0), "Arpeggios", "Hold a chord. Latch it with HOLD and transpose it from the keys."),
             (PC.get("Ambient", 0), "Ambient", "Slow drones, wind and generative bells with delay and reverb."),
             (PC.get("Poly", 0) + PC.get("FX", 0), "Poly and FX", "Polyphonic brass and keys, sample and hold, zaps and lasers.")],
    dl_h=f"Download AT101V {V101}.",
    dl_p="Free, no account, no activation, no telemetry.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit and standalone app",
    mac_steps=["Unzip the file.", "Copy <code>AT101V.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy <code>AT101V.component</code> to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Rescan plugins in your DAW. In Ableton Live: Settings, Plug-Ins, Rescan."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3 and standalone app",
    win_steps=["Unzip the file.", "Copy <code>AT101V.vst3</code> to <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Rescan plugins in your DAW.", "<code>AT101V.exe</code> runs without a DAW."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed plugins, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps.",
    win_caption="The same plugin running on Windows.",
    os_h="Free and open source.",
    os_p="AT101V is free software under the GNU Affero General Public License v3, like the JUCE framework it is built on. You can use it, study it, change it and share it. The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    os_link="Why Audio Tools is open source",
    trademark="Roland and SH-101 are trademarks of Roland Corporation. Audio Tools is an independent project, not affiliated with or endorsed by Roland. Product names are used only to identify the historical instrument that inspired this recreation.",
    photo_credit="Panel layout measured from photographs on Wikimedia Commons by 909 bd (CC BY-SA 3.0), eternal jamzzzz and byCharly (CC BY 2.0).",
    hero_alt="The grey AT101V rendered in 3D from the front left: sliders with green and orange lines, black knobs, cream buttons and a 32-key keyboard.",
    alt_synth="AT101V front panel and keyboard in grey",
    alt_blue="AT101V in blue", alt_red="AT101V in red, low angle",
    alt_seq="AT101V Sequencer page with a 16-step acid pattern",
    alt_ext="AT101V Settings page with faders and LED selectors",
    alt_win="AT101V running on Windows",
),
"es": dict(
    title="AT101V — una recreación libre del SH-101 con secuenciador visual | Audio Tools",
    description=f"El AT101V es un sintetizador monofónico libre y de código abierto basado en el Roland SH-101 de 1982, con el filtro IR3109, arpegiador, un secuenciador de 100 pasos con editor visual y {N101} presets. Para macOS, Windows y Linux.",
    h1="La máquina de líneas de bajo, reconstruida a la vista de todos.",
    lead="El AT101V es un sintetizador monofónico libre y de código abierto basado en el Roland SH-101 de 1982: un oscilador con sub-oscilador, un filtro de cuatro polos que canta, arpegiador y un secuenciador de 100 pasos.",
    dl_mac="Descargar para macOS", dl_win="Descargar para Windows",
    version_line=f"AT101V {V101}. VST3, Audio Unit y aplicación independiente.",
    flow_label="Contenido de la página",
    flow=[("#sound", "Motor de sonido", "filtro IR3109"), ("#panel", "Panel", "cada fader"), ("#sequencer", "Secuenciador", "100 pasos"),
          ("#extensions", "Extensiones", "poli, sync, efectos"), ("#presets", "Presets", f"{N101} sonidos"), ("#download", "Descarga", "macOS, Windows")],
    sound_h="Un oscilador, un filtro, sin atajos.",
    sound_p1="En los papeles el SH-101 es simple: un oscilador CEM3340, un sub-oscilador hecho con dos flip-flops y el filtro de cuatro polos IR3109 de Roland. Su sonido está en los detalles, y el AT101V los modela a partir de las notas de servicio y del análisis de circuito publicado.",
    sound_p2="Cada valor de esta lista se midió sobre el motor y se comparó con la documentación original.",
    spec=[("Filtro", "pasa bajos IR3109 de cuatro polos, 24 dB por octava, resuelto sin retardo de una muestra en el lazo"),
          ("Resonancia", "realimentación limitada por diodos que adelgaza los graves como el original y autooscila en seno siguiendo 1 V por octava"),
          ("Oscilador", "sierra ascendente y pulso del mismo núcleo; ancho de pulso a mano, por el LFO o por la envolvente, hasta el silencio"),
          ("Sub-oscilador", "una o dos octavas abajo, o el pulso de 25 %, enganchado en fase como los flip-flops"),
          ("Envolvente", "curvas RC; el ataque apunta por encima y corta en el pico. Ataque de 1,5 ms a 4 s, decaimiento y relajación hasta 10 s"),
          ("Lógica de teclado", "prioridad de última nota o de nota grave según el disparo, portamento Auto sólo en legato"),
          ("Sobremuestreo", "toda la voz a 2x, 4x u 8x")],
    video_caption="El AT101V renderizado en 3D. El sonido es el propio motor tocando Acid Line, uno de los presets de secuencia.",
    panel_h="Cada fader, cada llave.",
    panel_p1="El panel funciona como en 1982. El modulador es también el reloj del arpegiador y del secuenciador, la palanca se empuja para vibrato y growl, HOLD sostiene y KEY TRANSPOSE corre todo, también una secuencia en marcha.",
    panel_p2="El gabinete, las teclas y cada control se renderizaron en Blender a partir de mediciones del original, en los tres colores clásicos.",
    panel_caption="El panel y el teclado de 32 teclas. Se arrastran los faders, se mueven las llaves y se empuja la palanca.",
    colours_caption="Gris, azul o rojo: el color del gabinete es un ajuste.",
    seq_h="Un secuenciador que se ve.",
    seq_p="El original graba hasta 100 pasos desde el teclado sin mostrarlos. El AT101V mantiene esa forma de trabajo y suma un editor de pasos en su propia página.",
    seq_facts=[("Grilla de notas.", "Un clic pone una nota, arrastrar pinta, otro clic la vuelve silencio. Hasta 100 pasos en cuatro páginas."),
               ("Acento y slide.", "Los acentos abren el filtro y empujan el amplificador; los slides se deslizan a la nota siguiente sin redisparar."),
               ("Probabilidad.", "Cada paso puede sonar el 25, 50, 75 o 100 % de las veces, así el patrón no se repite igual."),
               ("Herramientas.", "Largo, rotar, transponer por semitonos u octavas, un generador de líneas de bajo y un botón para mutar."),
               ("Exportar.", "Guardá el patrón como .mid y llevalo a tu DAW."),
               ("Transponer con las teclas.", "Mientras corre una secuencia, cualquier tecla la transpone.")],
    seq_caption="La página del secuenciador tocando Acid Line: acentos en naranja, slides en verde y el LED rojo marca el paso actual.",
    ext_h="Lo que el SH-101 nunca tuvo.",
    ext_p="Una página de ajustes, dibujada como el panel, suma lo que pide un estudio actual.",
    ext_facts=[("Polifonía y unísono.", "Mono como el original, o hasta 8 voces, con unísono de hasta 4, desafinación y apertura estéreo."),
               ("Sincronía con el host.", "El arpegiador y el secuenciador siguen al DAW, con swing y largo de gate. O el modulador como reloj, como el original."),
               ("Expresión.", "Velocidad al filtro y al amplificador, cantidad de acento, tiempo de slide y arpegios de hasta cuatro octavas."),
               ("Efectos.", "Drive antes del filtro, un chorus estilo BBD, delay sincronizado y reverb de placa."),
               ("MIDI.", "MIDI learn en cada control, la rueda como empuje de la palanca, el pedal como HOLD y salida MIDI de todo lo que toca el sinte."),
               ("Presets.", "Guardá tus propios sonidos, con la secuencia y los ajustes, o tirá un patch al azar.")],
    ext_caption="La página de ajustes: voces, expresión, reloj, carácter, chorus, delay, reverb y sistema.",
    presets_h="Bajos, leads y secuencias que se tocan solas.",
    presets=[(PC.get("Bass", 0), "Bajos", "Bajos redondos del 101, elásticos y saturados."),
             (PC.get("Lead", 0), "Leads", "Voces solistas con portamento, PWM y la resonancia que canta."),
             (PC.get("Seq", 0), "Secuencias", "Acid, electro, escuela de Berlín y patrones con probabilidad. Se tocan solas."),
             (PC.get("Arp", 0), "Arpegios", "Mantené un acorde. Con HOLD queda andando y se transpone desde las teclas."),
             (PC.get("Ambient", 0), "Ambient", "Drones lentos, viento y campanas generativas con delay y reverb."),
             (PC.get("Poly", 0) + PC.get("FX", 0), "Poli y efectos", "Bronces y teclados polifónicos, sample and hold, zaps y láseres.")],
    dl_h=f"Descargá el AT101V {V101}.",
    dl_p="Gratis, sin cuenta, sin activación y sin telemetría.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit y aplicación independiente",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>AT101V.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá <code>AT101V.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Reescaneá los plugins en tu DAW. En Ableton Live: Settings, Plug-Ins, Rescan."],
    win_h="Windows", win_req="Windows 10 u 11 de 64 bits.", win_fmt="VST3 y aplicación independiente",
    win_steps=["Descomprimí el archivo.", "Copiá <code>AT101V.vst3</code> a <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Reescaneá los plugins en tu DAW.", "<code>AT101V.exe</code> funciona sin DAW."],
    file_label="archivo",
    dl_note="Los builds todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los plugins instalados, y SmartScreen de Windows puede pedir confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos.",
    win_caption="El mismo plugin funcionando en Windows.",
    os_h="Libre y de código abierto.",
    os_p="El AT101V es software libre bajo la GNU Affero General Public License v3, como el framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    os_link="Por qué Audio Tools es de código abierto",
    trademark="Roland y SH-101 son marcas registradas de Roland Corporation. Audio Tools es un proyecto independiente, sin relación ni aval de Roland. Los nombres de productos se usan sólo para identificar el instrumento histórico que inspiró esta recreación.",
    photo_credit="La disposición del panel se midió sobre fotografías de Wikimedia Commons de 909 bd (CC BY-SA 3.0), eternal jamzzzz y byCharly (CC BY 2.0).",
    hero_alt="El AT101V gris renderizado en 3D desde adelante a la izquierda: faders con líneas verdes y naranjas, perillas negras, botones crema y teclado de 32 teclas.",
    alt_synth="Panel y teclado del AT101V en gris",
    alt_blue="El AT101V en azul", alt_red="El AT101V en rojo, en ángulo bajo",
    alt_seq="Página del secuenciador del AT101V con un patrón acid de 16 pasos",
    alt_ext="Página de ajustes del AT101V con faders y selectores de LED",
    alt_win="El AT101V funcionando en Windows",
),
}


# ───────────────────────── AT42 ─────────────────────────
Q = {
"en": dict(
    title="AT42 — a free drone machine inspired by the Elta SOLAR 42 | Audio Tools",
    description=f"AT42 is a free, open-source drone synthesizer modeled on the Elta Music SOLAR 42: twenty drone oscillators, two noise voices, two VCOs, dual Polivoks filters, a 13-cartridge effector, a touch keyboard and a 62-jack patch bay. For macOS, Windows and Linux.",
    h1="An ambient drone machine, rebuilt in the open.",
    lead="AT42 is a free, open-source recreation of the Elta Music SOLAR 42: four drone voices of five oscillators each, two noise voices, two VCOs, a dual Polivoks filter, a dual cartridge effector and a 62-jack patch bay.",
    dl_mac="Download for macOS", dl_win="Download for Windows",
    version_line=f"AT42 {V42}. VST3, Audio Unit and standalone app.",
    flow_label="Page contents",
    flow=[("#sound", "Drones", "20 oscillators"), ("#patch", "Patch bay", "62 jacks"), ("#keys", "Touch keyboard", "19 scales"),
          ("#extensions", "Extensions", "MIDI, sync"), ("#presets", "Presets", f"{N42} sounds"), ("#download", "Download", "macOS, Windows")],
    sound_h="Oscillators that drift like the real ones.",
    sound_p1="Each drone oscillator is a single-transistor relaxation circuit: a capacitor charges toward the supply and discharges at breakdown. AT42 models that curve, so the VOLT knob lowers the supply, stretches the period and, past halfway, lets the five oscillators pull on each other.",
    sound_p2="Every classic voice has its photo-sensitive orb. Turn on MOD and the pitch follows the light: the ambient light setting, the shadow of your mouse over the orb, and the red LED inside it, driven by any CV.",
    spec=[("Drone voices", "4 voices of 5 sawtooth oscillators with the tune ranges of the original, from 16 Hz to 2.3 kHz"),
          ("VOLT", "transposes the whole voice down and, past half travel, cross-modulates its oscillators"),
          ("Photo orbs", "light-dependent pitch with the slow release of a real light-dependent resistor"),
          ("Noise voices", "the Papa Srapa design: square modulator and tone oscillator with FM, AM, noise and sample and hold"),
          ("VCOs", "six waveforms including two morphing shapes, linear FM, hard sync, 8-position octave, ADSR with hold and loop"),
          ("Filters", "dual 12 dB Polivoks with slew-limited integrators, low-pass or band-pass, and stereo distortion"),
          ("Effector", "two engines and 13 cartridges of 3 programs: reverbs, shimmer, pitch and reverse delays, ring mods, crushers, generators")],
    video_caption="AT42 rendered in 3D. The sound is the engine itself playing Cathedral of A, a drone preset.",
    patch_h="Sixty-two jacks, with the hardware's normals.",
    patch_p1="Every control voltage has its jack: LFO A and B, the joystick, the 5-step sequencer and its pulser, the envelope follower, the drone envelopes, the sample and holds, and the touch keyboard's pitch, gates and pressure.",
    patch_p2="Unpatched inputs keep the original's internal connections, so the instrument plays before you plug anything. Cables hang with real physics, and outputs take stacked plugs.",
    patch_caption="The full panel with a Berlin-school patch: the sequencer drives both VCOs and opens the filter.",
    colours_caption="Cream, black, maize or blue: the panel colour is a setting.",
    keys_h="Twelve plates and a little screen.",
    keys_p="The touch keyboard quantises to 19 scales, arpeggiates in five directions and records a 16-step sequence, with portamento, vibrato and five pressure modes. Its pitch and gate are normalled to both VCOs, and MIDI notes play it too.",
    keys_facts=[("Scales.", "Semitones, the seven modes, blues, pentatonics, Japanese, Gamelan, Gypsy, Arabian, Flamenco, whole tone. All 12 plates follow the scale."),
                ("Arpeggiator.", "Forward, backward, ping-pong, random or in the order played, with hold and clock division."),
                ("Sequencer.", "Record up to 16 notes from the plates; the active plate transposes the pattern."),
                ("Clock.", "Internal tempo, your DAW's tempo, or any clock patched into the clock input.")],
    ext_h="What the SOLAR 42 doesn't do.",
    ext_p="A Settings page, drawn like the panel, adds what owners ask for most.",
    ext_facts=[("MIDI.", "Notes play the touch keyboard, C1 to F1 play the drone keys, pitch bend bends the VCOs, the mod wheel moves the joystick, and MIDI learn works on every control."),
               ("Host sync.", "LFO A and B, the pulser and the keyboard clock can follow your DAW."),
               ("Chromatic drones.", "Snap the drone oscillators to semitones, or keep the original analog tuning."),
               ("Longer releases.", "Drone envelopes up to 30 seconds."),
               ("Stereo width.", "A WIDTH knob before the filters and the effector, from mono to twice as wide."),
               ("Presets and colours.", "Save patches with their cables and keyboard sequence, roll a random patch, and pick a panel colour.")],
    ext_caption="The Settings page: character, sync, touch keyboard, expression, MIDI and system.",
    presets_h="Drones, storms, sirens and sequences.",
    presets=[(PC42.get("Drone", 0), "Drones", "Chords tuned on the real tuning curve, from cathedrals to saturated swarms."),
             (PC42.get("Ambient", 0), "Ambient", "Shimmer, breathing orbs, tape memory and an ocean of noise."),
             (PC42.get("Sci-Fi", 0), "Sci-fi", "Sirens, birds at dusk, a space monster and volleys from the noise voices."),
             (PC42.get("Sequence", 0), "Sequences", "Patched with cables: Berlin-school, ritual pulses and a techno monster."),
             (PC42.get("Keys", 0), "Keys", "Flute leads, arpeggios, binaural VCOs and pressure pads from the touch keyboard."),
             (PC42.get("Experimental", 0), "Experimental", "Piezo percussion, joystick spaces, ring bells and crushed choirs.")],
    dl_h=f"Download AT42 {V42}.",
    dl_p="Free, no account, no activation, no telemetry.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit and standalone app",
    mac_steps=["Unzip the file.", "Copy <code>AT42.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy <code>AT42.component</code> to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Rescan plugins in your DAW. In Ableton Live: Settings, Plug-Ins, Rescan."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3 and standalone app",
    win_steps=["Unzip the file.", "Copy <code>AT42.vst3</code> to <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Rescan plugins in your DAW.", "<code>AT42.exe</code> runs without a DAW."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed plugins, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps.",
    win_caption="The same plugin running on Windows.",
    os_h="Free and open source.",
    os_p="AT42 is free software under the GNU Affero General Public License v3, like the JUCE framework it is built on. You can use it, study it, change it and share it. The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    os_link="Why Audio Tools is open source",
    trademark="Elta Music and SOLAR 42 are trademarks of their owners. Audio Tools is an independent project, not affiliated with or endorsed by Elta Music. Product names are used only to identify the instrument that inspired this recreation.",
    photo_credit="Panel geometry measured from the manufacturer's published panel drawings. All graphics on the AT42 panel are our own.",
    hero_alt="Close-up of the blue AT42 rendered in 3D: coloured knobs, glowing light orbs and the striped plates of the touch keyboard.",
    alt_synth="AT42 panel with patch cables connected", alt_black="AT42 with a black panel", alt_maize="AT42 with a maize yellow panel, low angle",
    alt_ext="AT42 Settings page", alt_win="AT42 running on Windows",
),
"es": dict(
    title="AT42 — una máquina de drones libre inspirada en el Elta SOLAR 42 | Audio Tools",
    description=f"El AT42 es un sintetizador de drones libre y de código abierto basado en el Elta Music SOLAR 42: veinte osciladores drone, dos voces de ruido, dos VCO, doble filtro Polivoks, un efector de 13 cartuchos, teclado táctil y una patchera de 62 jacks. Para macOS, Windows y Linux.",
    h1="Una máquina de drones ambient, reconstruida a la vista de todos.",
    lead="El AT42 es una recreación libre y de código abierto del Elta Music SOLAR 42: cuatro voces drone de cinco osciladores cada una, dos voces de ruido, dos VCO, doble filtro Polivoks, un efector doble de cartuchos y una patchera de 62 jacks.",
    dl_mac="Descargar para macOS", dl_win="Descargar para Windows",
    version_line=f"AT42 {V42}. VST3, Audio Unit y aplicación independiente.",
    flow_label="Contenido de la página",
    flow=[("#sound", "Drones", "20 osciladores"), ("#patch", "Patchera", "62 jacks"), ("#keys", "Teclado táctil", "19 escalas"),
          ("#extensions", "Extensiones", "MIDI, sync"), ("#presets", "Presets", f"{N42} sonidos"), ("#download", "Descarga", "macOS, Windows")],
    sound_h="Osciladores que derivan como los de verdad.",
    sound_p1="Cada oscilador drone es un circuito de relajación de un solo transistor: un capacitor carga hacia la fuente y se descarga al llegar a la ruptura. El AT42 modela esa curva, así la perilla VOLT baja la fuente, estira el período y, pasada la mitad, hace que los cinco osciladores se tironeen entre sí.",
    sound_p2="Cada voz clásica tiene su orbe fotosensible. Con MOD encendido la altura sigue a la luz: el ajuste de luz ambiente, la sombra del mouse sobre el orbe y el LED rojo de adentro, que se maneja con cualquier CV.",
    spec=[("Voces drone", "4 voces de 5 osciladores de sierra con los rangos del original, de 16 Hz a 2,3 kHz"),
          ("VOLT", "transpone toda la voz hacia abajo y, pasada la mitad, modula sus osciladores entre sí"),
          ("Orbes", "altura que depende de la luz, con la caída lenta de una fotorresistencia real"),
          ("Voces de ruido", "el diseño de Papa Srapa: modulador cuadrado y oscilador de tono con FM, AM, ruido y sample and hold"),
          ("VCO", "seis formas de onda con dos morphing, FM lineal, sync, octava de 8 posiciones, ADSR con hold y loop"),
          ("Filtros", "doble Polivoks de 12 dB con integradores de slew limitado, pasa bajos o pasa banda, y distorsión estéreo"),
          ("Efector", "dos motores y 13 cartuchos de 3 programas: reverbs, shimmer, delays con pitch e invertidos, ring mod, crushers, generadores")],
    video_caption="El AT42 renderizado en 3D. El sonido es el propio motor tocando Cathedral of A, un preset de drones.",
    patch_h="Sesenta y dos jacks, con las conexiones internas del hardware.",
    patch_p1="Cada tensión de control tiene su jack: LFO A y B, el joystick, el secuenciador de 5 pasos y su pulser, el seguidor de envolvente, las envolventes de los drones, los sample and hold, y la altura, los gates y la presión del teclado táctil.",
    patch_p2="Las entradas sin cable mantienen las conexiones internas del original, así el instrumento suena antes de enchufar nada. Los cables cuelgan con física real y las salidas aceptan fichas apiladas.",
    patch_caption="El panel completo con un patch de escuela de Berlín: el secuenciador maneja los dos VCO y abre el filtro.",
    colours_caption="Crema, negro, maíz o azul: el color del panel es un ajuste.",
    keys_h="Doce placas y una pantallita.",
    keys_p="El teclado táctil cuantiza a 19 escalas, arpegia en cinco direcciones y graba una secuencia de 16 pasos, con portamento, vibrato y cinco modos de presión. Su altura y su gate van normalizados a los dos VCO, y las notas MIDI también lo tocan.",
    keys_facts=[("Escalas.", "Semitonos, los siete modos, blues, pentatónicas, japonesa, gamelan, gitana, árabe, flamenca y por tonos. Las 12 placas siguen la escala."),
                ("Arpegiador.", "Hacia adelante, hacia atrás, ida y vuelta, al azar o en el orden tocado, con hold y división de reloj."),
                ("Secuenciador.", "Graba hasta 16 notas desde las placas; la placa activa transpone el patrón."),
                ("Reloj.", "Tempo interno, el del DAW o cualquier reloj enchufado en la entrada clock.")],
    ext_h="Lo que el SOLAR 42 no hace.",
    ext_p="Una página de ajustes, dibujada como el panel, suma lo que más piden los dueños.",
    ext_facts=[("MIDI.", "Las notas tocan el teclado táctil, de C1 a F1 tocan las teclas drone, el pitch bend dobla los VCO, la rueda mueve el joystick y hay MIDI learn en cada control."),
               ("Sincronía con el host.", "LFO A y B, el pulser y el reloj del teclado pueden seguir al DAW."),
               ("Drones cromáticos.", "Los osciladores drone se pueden ajustar a semitonos, o mantener la afinación analógica original."),
               ("Releases más largos.", "Envolventes de los drones de hasta 30 segundos."),
               ("Ancho estéreo.", "Una perilla WIDTH antes de los filtros y del efector, de mono al doble de ancho."),
               ("Presets y colores.", "Guardá patches con sus cables y la secuencia del teclado, tirá un patch al azar y elegí el color del panel.")],
    ext_caption="La página de ajustes: carácter, sincronía, teclado táctil, expresión, MIDI y sistema.",
    presets_h="Drones, tormentas, sirenas y secuencias.",
    presets=[(PC42.get("Drone", 0), "Drones", "Acordes afinados sobre la curva real del pote, de catedrales a enjambres saturados."),
             (PC42.get("Ambient", 0), "Ambient", "Shimmer, orbes que respiran, memoria de cinta y un océano de ruido."),
             (PC42.get("Sci-Fi", 0), "Ciencia ficción", "Sirenas, pájaros al atardecer, un monstruo espacial y ráfagas de las voces de ruido."),
             (PC42.get("Sequence", 0), "Secuencias", "Con cables: escuela de Berlín, pulsos rituales y un monstruo techno."),
             (PC42.get("Keys", 0), "Teclado", "Leads de flauta, arpegios, VCO binaurales y pads con presión desde el teclado táctil."),
             (PC42.get("Experimental", 0), "Experimentales", "Percusión con el piezo, espacios con joystick, campanas de ring y coros triturados.")],
    dl_h=f"Descargá el AT42 {V42}.",
    dl_p="Gratis, sin cuenta, sin activación y sin telemetría.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit y aplicación independiente",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>AT42.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá <code>AT42.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Reescaneá los plugins en tu DAW. En Ableton Live: Settings, Plug-Ins, Rescan."],
    win_h="Windows", win_req="Windows 10 u 11 de 64 bits.", win_fmt="VST3 y aplicación independiente",
    win_steps=["Descomprimí el archivo.", "Copiá <code>AT42.vst3</code> a <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Reescaneá los plugins en tu DAW.", "<code>AT42.exe</code> funciona sin DAW."],
    file_label="archivo",
    dl_note="Los builds todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los plugins instalados, y SmartScreen de Windows puede pedir confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos.",
    win_caption="El mismo plugin funcionando en Windows.",
    os_h="Libre y de código abierto.",
    os_p="El AT42 es software libre bajo la GNU Affero General Public License v3, como el framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    os_link="Por qué Audio Tools es de código abierto",
    trademark="Elta Music y SOLAR 42 son marcas de sus dueños. Audio Tools es un proyecto independiente, sin relación ni aval de Elta Music. Los nombres de productos se usan sólo para identificar el instrumento que inspiró esta recreación.",
    photo_credit="La geometría del panel se midió sobre los dibujos de panel que publica el fabricante. Toda la gráfica del panel del AT42 es propia.",
    hero_alt="Primer plano del AT42 azul renderizado en 3D: perillas de colores, orbes de luz encendidos y las placas a rayas del teclado táctil.",
    alt_synth="Panel del AT42 con cables de patcheo", alt_black="El AT42 con panel negro", alt_maize="El AT42 con panel amarillo maíz, en ángulo bajo",
    alt_ext="Página de ajustes del AT42", alt_win="El AT42 funcionando en Windows",
),
}

# ───────────────────────── AT700 ─────────────────────────
Z = {
"en": dict(
    title="AT700 — a free modular synthesizer inspired by the Roland System-700 | Audio Tools",
    description="AT700 is a free, open-source modular synthesizer modeled on the 1976 Roland System-700: the 17-module Main Console, the duophonic 701A keyboard and the 717A analog sequencer, with 137 patch points. For macOS, Windows and Linux.",
    sound_h="Three oscillators, two filters, 1976 levels.",
    sound_p1="The three 702 oscillators share one saw core for four simultaneous waveforms, with strong and weak sync. Two filter families sit side by side: the 703E, four OTA stages that lose bass as the resonance rises, just like the original, and the 703B state-variable with high-pass, band-pass and low-pass.",
    sound_p2="The output module has a two-spring reverb, an eight-stage phase shifter, two VU meters and a standard oscillator to tune by. Everything runs in volts, as the hardware did: 10 V audio, 1 V per octave, +15 V gates.",
    spec=[("VCOs", "3 × 702: sine, triangle, saw and pulse at once, PWM, ranges LO to 2', strong and weak sync, 1/10 attenuators"),
          ("Filters", "703E 24 dB OTA low-pass with voltage-controlled resonance, 703B 12 dB high-pass, band-pass or low-pass; both self-oscillate in tune"),
          ("Amplifiers", "2 × 704 with linear (10 %/V) or exponential (10 dB/V) response"),
          ("Envelopes", "705A dual ADSR, 2 ms to 10 s or 0.2 ms to 1 s, gate+trigger, gate or external gate"),
          ("Modulation", "2 LFOs (delay or pulse width), sample and hold, ring modulator, white and pink noise, envelope follower and integrator, mixer, two multiples"),
          ("Output", "spring reverb, three-channel panner, phase shifter, VU meters, standard oscillator 110 Hz to 1.76 kHz"),
          ("Keyboard", "701A, 61 keys, duophonic: CV1 lowest key, CV2 highest; portamento, bender, pitch")],
    console_h="Seventeen modules, wired at the factory.",
    console_p="The Main Console comes with its internal buses: the VCOs reach the filters through their waveform selectors, the filters reach the amplifiers, the amplifiers reach the reverb, the panner and the phaser. Patch cords add what the buses don't: sync, the sequencer, the second key voltage, the envelope follower. 137 jacks, ten of them in two multiples.",
    console_caption="The Main Console with Berlin Twelve: the sequencer drives two oscillators and the filter, and its clock gates the envelope.",
    seq_h="The 717A sequencer.",
    seq_p="Three channels of twelve steps, or one melody of 36 in series. Every step has its own time divide and fine, a hold switch for rests, a manual button, a gate output and an analog input that turns the registers into level controls.",
    seq_facts=[("Clock.", "2 to 200 steps per minute, a control voltage input, an external clock, or your DAW's tempo."),
               ("Series.", "A, A-B, A-B-C or manual; stop or repeat; a step selector sets the length from 1 to 12."),
               ("Master controller.", "Start, continue, stop and end, each with a control pulse input, plus start and end pulse outputs."),
               ("Pulse shaper.", "Turns any signal into gates with a threshold and a 0 to 3 second switch-on time.")],
    seq_caption="The 717A running Berlin Twelve.",
    ext_h="What the System-700 doesn't do.",
    ext_p="A Settings page adds what a studio expects today, without touching the console.",
    ext_facts=[("Polyphony.", "Up to 8 voices: the voice modules are duplicated for every key, while the LFOs, sample and hold, output and sequencer stay shared."),
               ("MIDI.", "Notes play the 701A, pitch bend moves the bender, and velocity, mod wheel and aftertouch come out of jacks on the keyboard panel. MIDI learn on every control."),
               ("Host sync.", "The 717A can follow your DAW's tempo and transport: DIVIDE 1 is a whole note."),
               ("Two VCF-1 models.", "The 703E 24 dB low-pass of the known consoles, or the 703A 12 dB multimode of the 1976 manual."),
               ("Views.", "Sequencer, console, keyboard, or the three cabinets at once; cables keep hanging between them."),
               ("Presets.", f"{N700} factory patches with their cables, your own, and a random patch.")],
    ext_caption="The Settings page: voices, character, sequencer clock, MIDI and system.",
    presets_h="Brass, whistles, Berlin and thunder.",
    presets=[(pc700("Lead", "Brass"), "Leads and brass", "Tomita brass, a whistle from the filter alone, a flute, sync sweeps, glides and duophonic fifths."),
             (pc700("Bass"), "Bass", "Saw and pulse basses for synth-pop and a sub pulse."),
             (pc700("Strings", "Pads", "Keys", "Bells"), "Keys, strings and bells", "Ensemble strings with the phaser, pads, tine keys, clavi and ring-modulated bells."),
             (pc700("Sequence"), "Sequences", "Patched on the 717A: Berlin, transposed from the keys, 36 steps in series, rests and filter sequences."),
             (pc700("FX", "Drone"), "Effects and drones", "Random computer, sample and hold, surf and wind, lasers, thunder, spring drips and a space drone."),
             (pc700("Drums"), "Drums", "Kick, snare and hi-hat from the filters, the noise and the envelopes.")],
    dl_h=f"Download AT700 {V700}.",
    dl_p="Free, no account, no activation, no telemetry.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit and standalone app",
    mac_steps=["Unzip the file.", "Copy <code>AT700.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy <code>AT700.component</code> to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Rescan plugins in your DAW. In Ableton Live: Settings, Plug-Ins, Rescan."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3 and standalone app",
    win_steps=["Unzip the file.", "Copy <code>AT700.vst3</code> to <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Rescan plugins in your DAW.", "<code>AT700.exe</code> runs without a DAW."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed plugins, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps.",
    os_p="AT700 is free software under the GNU Affero General Public License v3, like the JUCE framework it is built on. You can use it, study it, change it and share it. The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    os_link="Why Audio Tools is open source",
    trademark="Roland and System-700 are trademarks of Roland Corporation. Audio Tools is an independent project, not affiliated with or endorsed by Roland. Product names are used only to identify the instrument that inspired this recreation.",
    photo_credit="Panel layout measured from the scaled drawings of the 1976 instruction manual. All graphics on the AT700 panels are our own.",
    hero_alt="The AT700 rendered in 3D: the 717A sequencer standing on the black Main Console, patched with red, blue and yellow cables, behind the 61-key keyboard.",
    alt_synth="AT700 Main Console with patch cables", alt_seq="AT700 717A sequencer running", alt_ext="AT700 Settings page",
),
"es": dict(
    title="AT700 — un sintetizador modular libre inspirado en el Roland System-700 | Audio Tools",
    description="El AT700 es un sintetizador modular libre y de código abierto basado en el Roland System-700 de 1976: la consola principal de 17 módulos, el teclado duofónico 701A y el secuenciador analógico 717A, con 137 puntos de patcheo. Para macOS, Windows y Linux.",
    sound_h="Tres osciladores, dos filtros, niveles de 1976.",
    sound_p1="Los tres osciladores 702 comparten un núcleo de sierra para cuatro formas de onda simultáneas, con sync fuerte y débil. Hay dos familias de filtro lado a lado: el 703E, cuatro etapas OTA que pierden graves al subir la resonancia, igual que el original, y el 703B de estado variable con pasa altos, pasa banda y pasa bajos.",
    sound_p2="El módulo de salida tiene una reverb de dos resortes, un phase shifter de ocho etapas, dos vúmetros y un oscilador patrón para afinar. Todo funciona en voltios, como el hardware: audio de 10 V, 1 V por octava, compuertas de +15 V.",
    spec=[("VCO", "3 × 702: seno, triángulo, sierra y pulso a la vez, PWM, rangos de LO a 2', sync fuerte y débil, atenuadores 1/10"),
          ("Filtros", "703E pasa bajos OTA de 24 dB con resonancia por tensión, 703B de 12 dB pasa altos, pasa banda o pasa bajos; los dos autooscilan afinados"),
          ("Amplificadores", "2 × 704 con respuesta lineal (10 %/V) o exponencial (10 dB/V)"),
          ("Envolventes", "705A doble ADSR, de 2 ms a 10 s o de 0,2 ms a 1 s, gate+trigger, gate o gate externo"),
          ("Modulación", "2 LFO (con retardo o ancho de pulso), sample and hold, modulador en anillo, ruido blanco y rosa, seguidor de envolvente e integrador, mezclador, dos múltiples"),
          ("Salida", "reverb de resorte, paneo de tres canales, phase shifter, vúmetros, oscilador patrón de 110 Hz a 1,76 kHz"),
          ("Teclado", "701A, 61 teclas, duofónico: CV1 la tecla más grave, CV2 la más aguda; portamento, palanca, afinación")],
    console_h="Diecisiete módulos, cableados de fábrica.",
    console_p="La consola principal viene con sus buses internos: los VCO llegan a los filtros por sus selectores de onda, los filtros llegan a los amplificadores y los amplificadores a la reverb, al paneo y al phaser. Los cables suman lo que los buses no hacen: sync, el secuenciador, la segunda tensión del teclado, el seguidor de envolvente. 137 jacks, diez de ellos en dos múltiples.",
    console_caption="La consola principal con Berlin Twelve: el secuenciador maneja dos osciladores y el filtro, y su reloj dispara la envolvente.",
    seq_h="El secuenciador 717A.",
    seq_p="Tres canales de doce pasos, o una melodía de 36 en serie. Cada paso tiene su propia división y ajuste fino de tiempo, una llave de hold para los silencios, un botón manual, una salida de gate y una entrada analógica que convierte los registros en controles de nivel.",
    seq_facts=[("Reloj.", "De 2 a 200 pasos por minuto, una entrada de tensión de control, un reloj externo o el tempo del DAW."),
               ("Serie.", "A, A-B, A-B-C o manual; stop o repeat; un selector fija el largo de 1 a 12 pasos."),
               ("Controlador maestro.", "Start, continue, stop y end, cada uno con su entrada de pulso de control, más salidas de pulso de inicio y de fin."),
               ("Pulse shaper.", "Convierte cualquier señal en compuertas, con umbral y un tiempo de encendido de 0 a 3 segundos.")],
    seq_caption="El 717A tocando Berlin Twelve.",
    ext_h="Lo que el System-700 no hace.",
    ext_p="Una página de ajustes suma lo que un estudio espera hoy, sin tocar la consola.",
    ext_facts=[("Polifonía.", "Hasta 8 voces: los módulos de voz se duplican para cada tecla, mientras los LFO, el sample and hold, la salida y el secuenciador quedan compartidos."),
               ("MIDI.", "Las notas tocan el 701A, el pitch bend mueve la palanca, y la velocidad, la rueda y el aftertouch salen por jacks del panel del teclado. MIDI learn en cada control."),
               ("Sincronía con el host.", "El 717A puede seguir el tempo y el transporte del DAW: DIVIDE 1 es una redonda."),
               ("Dos modelos de VCF-1.", "El 703E pasa bajos de 24 dB de las consolas conocidas, o el 703A multimodo de 12 dB del manual de 1976."),
               ("Vistas.", "Secuenciador, consola, teclado o los tres gabinetes juntos; los cables siguen colgando entre ellos."),
               ("Presets.", f"{N700} patches de fábrica con sus cables, los tuyos y un patch al azar.")],
    ext_caption="La página de ajustes: voces, carácter, reloj del secuenciador, MIDI y sistema.",
    presets_h="Bronces, silbidos, Berlín y truenos.",
    presets=[(pc700("Lead", "Brass"), "Leads y bronces", "Bronces a lo Tomita, un silbido hecho sólo con el filtro, una flauta, barridos de sync, glides y quintas duofónicas."),
             (pc700("Bass"), "Bajos", "Bajos de sierra y pulso para synth-pop y un pulso sub."),
             (pc700("Strings", "Pads", "Keys", "Bells"), "Teclados, cuerdas y campanas", "Cuerdas con el phaser, pads, teclados de lengüeta, clavi y campanas con modulador en anillo."),
             (pc700("Sequence"), "Secuencias", "Patcheadas en el 717A: Berlín, transpuestas desde el teclado, 36 pasos en serie, silencios y secuencias de filtro."),
             (pc700("FX", "Drone"), "Efectos y drones", "Computadora al azar, sample and hold, mar y viento, láseres, trueno, gotas de resorte y un drone espacial."),
             (pc700("Drums"), "Percusión", "Bombo, caja y hi-hat con los filtros, el ruido y las envolventes.")],
    dl_h=f"Descargá el AT700 {V700}.",
    dl_p="Gratis, sin cuenta, sin activación y sin telemetría.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit y aplicación independiente",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>AT700.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá <code>AT700.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Reescaneá los plugins en tu DAW. En Ableton Live: Settings, Plug-Ins, Rescan."],
    win_h="Windows", win_req="Windows 10 u 11 de 64 bits.", win_fmt="VST3 y aplicación independiente",
    win_steps=["Descomprimí el archivo.", "Copiá <code>AT700.vst3</code> a <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Reescaneá los plugins en tu DAW.", "<code>AT700.exe</code> funciona sin DAW."],
    file_label="archivo",
    dl_note="Los builds todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los plugins instalados, y SmartScreen de Windows puede pedir confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos.",
    os_p="El AT700 es software libre bajo la GNU Affero General Public License v3, como el framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    os_link="Por qué Audio Tools es de código abierto",
    trademark="Roland y System-700 son marcas de Roland Corporation. Audio Tools es un proyecto independiente, sin relación ni aval de Roland. Los nombres de productos se usan sólo para identificar el instrumento que inspiró esta recreación.",
    photo_credit="La disposición de los paneles se midió sobre los dibujos a escala del manual de instrucciones de 1976. Toda la gráfica de los paneles del AT700 es propia.",
    hero_alt="El AT700 renderizado en 3D: el secuenciador 717A parado sobre la consola principal negra, con cables rojos, azules y amarillos, detrás del teclado de 61 teclas.",
    alt_synth="Consola principal del AT700 con cables de patcheo", alt_seq="El secuenciador 717A del AT700 en marcha", alt_ext="Página de ajustes del AT700",
),
}


# ───────────────────────── ATFS1 ─────────────────────────
F = {
"en": dict(
    title="ATFS1 — a free formant and FM synthesizer inspired by the Yamaha FS1R | Audio Tools",
    description=f"ATFS1 is a free, open-source formant and FM synthesizer modeled on the 1998 Yamaha FS1R: 8 voiced and 8 noise operators, 88 algorithms, Formant Sequences you can make from any recording, and the full-screen editor the original never had. For macOS, Windows and Linux.",
    sound_h="Formants and FM in one operator.",
    sound_p1="Every voiced operator is a carrier inside a window that restarts at each period of the note. Change the window and the carrier and the same operator gives one partial, a group of harmonics, only the odd ones, a resonance or a vocal formant; give it FM and it does what a DX7 does. Next to each one, an unvoiced operator turns white noise into a band around a formant: the breath and the consonants.",
    sound_p2="The laws come from Yamaha's 1995 patent, the FS1R manuals and the measurements published by the FSVR project on real units; the code is our own and was checked against those numbers.",
    spec=[("Voiced operators", "8 per voice, 8 spectral forms: sine, all1, all2, odd1, odd2, res1, res2 and frmt, with skirt, bandwidth and resonance"),
          ("Unvoiced operators", "8 noise formants per voice, with bandwidth, skirt and resonance; normal, or locked to the pitch or to the voiced formant"),
          ("Algorithms", "the 88 of the FS1R with their feedback loops; FM depth of 3.37 cycles at full level"),
          ("Envelopes", "a 4-stage amplitude EG with hold and a frequency EG on every operator; pitch EG; filter EG"),
          ("Modulation", "LFO1 to pitch, amplitude, frequency and filter, LFO2 to the filter; Formant and FM knob matrices"),
          ("Filter", "per voice: low-pass 24, 18 and 12 dB, high-pass, band-pass and band-reject, with negative resonance and input drive"),
          ("Formant Sequences", "8 voiced and 8 unvoiced formants plus pitch, 126 frames per second, up to 512 frames")],
    fseq_h="A phrase becomes a synthesizer part.",
    fseq_p=f"A Formant Sequence stores how the formants of a voice move: frequency and level of eight voiced and eight unvoiced formants, plus the pitch, 126 times a second. Play a note and the operators follow it, so the synth says the phrase at your pitch, at your tempo, looped or scratched with the mod wheel. The ATFS1 comes with {NFSEQ}: vowels, choirs, scat and spoken words.",
    fseq_caption="The Fseq page: the formant tracks of a spoken phrase, the vowel generator and the playback settings.",
    ops_h="One operator, eight spectra.",
    ops_p="The form decides what the window does to the carrier. Pick one and the spectrum above the envelopes redraws, so you hear and see what the operator gives before it reaches the algorithm.",
    ops_facts=[("all1 and all2.", "A group of harmonics around the operator frequency; the skirt opens it from one partial to a bright band."),
               ("odd1 and odd2.", "Only the odd harmonics, from a hollow tone to a square-like buzz."),
               ("res1 and res2.", "Three lines on the harmonic you choose with the resonance: a whistle that moves through the partials."),
               ("frmt.", "Grains at the note pitch with the carrier on the formant: the vowel colour of a voice, a horn or a string.")],
    ops_caption="The Operator page: a resonant operator, its spectrum at A3, its envelopes and every parameter it reads.",
    ext_h="What the FS1R never had.",
    ext_p="The original asked for an editor on a computer and a lot of patience. The ATFS1 brings the editor inside, and the features owners kept asking for.",
    ext_facts=[("Your own Fseqs.", "Drop a WAV or AIFF and the ATFS1 tracks its pitch and formants; or type vowels and it builds a sequence. The FS1R could not create them."),
               ("An Fseq per preset.", "No limit of six; every preset carries its own, and they export as FS1R sysex."),
               ("The whole voice on screen.", "88 algorithms drawn with live levels, a spectrum per operator, envelopes you drag, and help that says what each parameter really does."),
               ("All 32 voices filtered.", "The FS1R halves its polyphony with the filter on; here every voice keeps it. An authentic switch brings the limit back."),
               ("Smoother sequences.", "Optional interpolation between frames and speeds from 10 to 1000 %."),
               ("FS1R sysex.", "Import voices and Fseqs from the hardware or other editors, and send your voices back.")],
    ext_caption="The Play page: presets, the algorithm with live levels, the output spectrum and the Formant Sequence.",
    presets_h="Talking, singing, ringing.",
    presets=[(pcfs1("Vo"), "Voices", "Phrases and scat sung back from their formants, a robot, choirs, whispers and a vowel lead."),
             (pcfs1("Pd"), "Pads", "Formant strings, a glass choir, vowels that morph, resonant sweeps and an odd-harmonic pad."),
             (pcfs1("Ky"), "Keys and bells", "A tine piano, formant bells, a glass marimba, a clav and an organ."),
             (pcfs1("Ld"), "Leads", "A formant lead on the mod wheel, a square-like odd lead, resonant brass and a breath flute."),
             (pcfs1("Bs"), "Bass", "FM, formant, odd-harmonic and wah basses."),
             (pcfs1("Sq", "Fx"), "Sequences and effects", "Rhythmic vowel loops, a formant scratch on the mod wheel, wind and space talk.")],
    dl_h=f"Download ATFS1 {VFS1}.",
    dl_p="Free, no account, no activation, no telemetry.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit and standalone app",
    mac_steps=["Unzip the file.", "Copy <code>ATFS1.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy <code>ATFS1.component</code> to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Rescan plugins in your DAW. In Ableton Live: Settings, Plug-Ins, Rescan."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3 and standalone app",
    win_steps=["Unzip the file.", "Copy <code>ATFS1.vst3</code> to <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Rescan plugins in your DAW.", "<code>ATFS1.exe</code> runs without a DAW."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed plugins, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps.",
    os_p="ATFS1 is free software under the GNU Affero General Public License v3, like the JUCE framework it is built on. You can use it, study it, change it and share it. The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    os_link="Why Audio Tools is open source",
    trademark="Yamaha and FS1R are trademarks of Yamaha Corporation. Audio Tools is an independent project, not affiliated with or endorsed by Yamaha. Product names are used only to identify the instrument that inspired this recreation.",
    photo_credit="Engine laws from the FS1R manuals, Yamaha's patent US 5,610,354 and the measurements published by the FSVR project. Panel layout measured from public-domain photos. All graphics on the ATFS1 panel are our own, and no Yamaha voices or sequences are included.",
    hero_alt="The ATFS1 rendered in 3D: a silver-blue rack unit with a yellow-green display reading Talking Formants and four black knobs, reflected on a dark desk.",
    alt_synth="ATFS1 Voice page with the algorithm graph", alt_fseq="ATFS1 Formant Sequence page", alt_ops="ATFS1 Operator page", alt_ext="ATFS1 Play page",
),
"es": dict(
    title="ATFS1 — un sintetizador de formantes y FM libre inspirado en el Yamaha FS1R | Audio Tools",
    description=f"El ATFS1 es un sintetizador de formantes y FM libre y de código abierto basado en el Yamaha FS1R de 1998: 8 operadores con voz y 8 de ruido, 88 algoritmos, Formant Sequences que podés crear desde cualquier grabación y el editor de pantalla completa que el original nunca tuvo. Para macOS, Windows y Linux.",
    sound_h="Formantes y FM en un mismo operador.",
    sound_p1="Cada operador con voz es una portadora dentro de una ventana que vuelve a empezar en cada período de la nota. Cambiando la ventana y la portadora, el mismo operador da una sola parcial, un grupo de armónicos, sólo los impares, una resonancia o un formante de voz; con FM hace lo mismo que un DX7. Al lado de cada uno, un operador sin voz convierte ruido blanco en una banda alrededor de un formante: el aliento y las consonantes.",
    sound_p2="Las leyes salen de la patente de Yamaha de 1995, de los manuales del FS1R y de las mediciones que publicó el proyecto FSVR sobre equipos reales; el código es propio y se verificó contra esos números.",
    spec=[("Operadores con voz", "8 por voz, 8 formas espectrales: sine, all1, all2, odd1, odd2, res1, res2 y frmt, con falda, ancho de banda y resonancia"),
          ("Operadores sin voz", "8 formantes de ruido por voz, con ancho, falda y resonancia; libres, o enganchados a la altura o al formante con voz"),
          ("Algoritmos", "los 88 del FS1R con sus lazos de realimentación; FM de 3,37 ciclos a nivel máximo"),
          ("Envolventes", "envolvente de amplitud de 4 etapas con hold y envolvente de frecuencia en cada operador; envolvente de altura; envolvente del filtro"),
          ("Modulación", "LFO1 a altura, amplitud, frecuencia y filtro, LFO2 al filtro; matrices de las perillas Formant y FM"),
          ("Filtro", "por voz: pasa bajos de 24, 18 y 12 dB, pasa altos, pasa banda y rechaza banda, con resonancia negativa y saturación de entrada"),
          ("Formant Sequences", "8 formantes con voz y 8 sin voz más la altura, 126 cuadros por segundo, hasta 512 cuadros")],
    fseq_h="Una frase se vuelve una parte de sintetizador.",
    fseq_p=f"Un Formant Sequence guarda cómo se mueven los formantes de una voz: frecuencia y nivel de ocho formantes con voz y ocho sin voz, más la altura, 126 veces por segundo. Tocás una nota y los operadores lo siguen: el sinte dice la frase en tu altura y a tu tempo, en lazo o recorrida con la rueda. El ATFS1 trae {NFSEQ}: vocales, coros, scat y palabras.",
    fseq_caption="La página Fseq: las pistas de formantes de una frase hablada, el generador de vocales y la reproducción.",
    ops_h="Un operador, ocho espectros.",
    ops_p="La forma decide qué le hace la ventana a la portadora. Elegís una y el espectro sobre las envolventes se redibuja: escuchás y ves lo que da el operador antes de que entre al algoritmo.",
    ops_facts=[("all1 y all2.", "Un grupo de armónicos alrededor de la frecuencia del operador; la falda lo abre de una parcial a una banda brillante."),
               ("odd1 y odd2.", "Sólo los armónicos impares, de un tono hueco a un zumbido de onda cuadrada."),
               ("res1 y res2.", "Tres líneas en el armónico que elegís con la resonancia: un silbido que recorre los parciales."),
               ("frmt.", "Granos a la altura de la nota con la portadora en el formante: el color de vocal de una voz, un bronce o una cuerda.")],
    ops_caption="La página Operator: un operador resonante, su espectro en La 3, sus envolventes y cada parámetro que lee.",
    ext_h="Lo que el FS1R nunca tuvo.",
    ext_p="El original pedía un editor en una computadora y mucha paciencia. El ATFS1 trae el editor adentro, y lo que sus dueños venían pidiendo.",
    ext_facts=[("Tus propios Fseqs.", "Soltás un WAV o AIFF y el ATFS1 sigue su altura y sus formantes; o escribís vocales y arma la secuencia. El FS1R no podía crearlos."),
               ("Un Fseq por preset.", "Sin el límite de seis; cada preset trae el suyo, y se exportan como sysex del FS1R."),
               ("La voz entera en pantalla.", "Los 88 algoritmos dibujados con el nivel en vivo, un espectro por operador, envolventes que se arrastran y una ayuda que dice qué hace de verdad cada parámetro."),
               ("Filtro en las 32 voces.", "El FS1R reduce la polifonía a la mitad con el filtro encendido; acá cada voz lo conserva. Una llave auténtica devuelve el límite."),
               ("Secuencias más suaves.", "Interpolación opcional entre cuadros y velocidades de 10 a 1000 %."),
               ("Sysex del FS1R.", "Importás voces y Fseqs del hardware o de otros editores, y devolvés tus voces.")],
    ext_caption="La página Play: presets, el algoritmo con el nivel en vivo, el espectro de la salida y el Formant Sequence.",
    presets_h="Hablan, cantan, suenan.",
    presets=[(pcfs1("Vo"), "Voces", "Frases y scat cantados desde sus formantes, un robot, coros, susurros y un lead de vocales."),
             (pcfs1("Pd"), "Pads", "Cuerdas de formantes, un coro de cristal, vocales que se transforman, barridos resonantes y un pad de armónicos impares."),
             (pcfs1("Ky"), "Teclados y campanas", "Un piano de púas, campanas de formantes, una marimba de cristal, un clav y un órgano."),
             (pcfs1("Ld"), "Leads", "Un lead de formantes en la rueda, un lead impar casi cuadrado, bronces resonantes y una flauta con aliento."),
             (pcfs1("Bs"), "Bajos", "Bajos FM, de formantes, de armónicos impares y con wah."),
             (pcfs1("Sq", "Fx"), "Secuencias y efectos", "Lazos rítmicos de vocales, un scratch de formantes en la rueda, viento y voces espaciales.")],
    dl_h=f"Descargá el ATFS1 {VFS1}.",
    dl_p="Gratis, sin cuenta, sin activación y sin telemetría.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit y aplicación independiente",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>ATFS1.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá <code>ATFS1.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Reescaneá los plugins en tu DAW. En Ableton Live: Settings, Plug-Ins, Rescan."],
    win_h="Windows", win_req="Windows 10 u 11 de 64 bits.", win_fmt="VST3 y aplicación independiente",
    win_steps=["Descomprimí el archivo.", "Copiá <code>ATFS1.vst3</code> a <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Reescaneá los plugins en tu DAW.", "<code>ATFS1.exe</code> funciona sin DAW."],
    file_label="archivo",
    dl_note="Los builds todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los plugins instalados, y SmartScreen de Windows puede pedir confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos.",
    os_p="El ATFS1 es software libre bajo la GNU Affero General Public License v3, como el framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    os_link="Por qué Audio Tools es de código abierto",
    trademark="Yamaha y FS1R son marcas de Yamaha Corporation. Audio Tools es un proyecto independiente, sin relación ni aval de Yamaha. Los nombres de productos se usan sólo para identificar el instrumento que inspiró esta recreación.",
    photo_credit="Las leyes del motor salen de los manuales del FS1R, de la patente de Yamaha US 5,610,354 y de las mediciones que publicó el proyecto FSVR. La disposición del panel se midió sobre fotos de dominio público. Toda la gráfica del panel del ATFS1 es propia, y no incluye voces ni secuencias de Yamaha.",
    hero_alt="El ATFS1 renderizado en 3D: una unidad de rack gris azulada con una pantalla amarillo verdosa que dice Talking Formants y cuatro perillas negras, reflejada sobre una mesa oscura.",
    alt_synth="Página Voice del ATFS1 con el gráfico del algoritmo", alt_fseq="Página Formant Sequence del ATFS1", alt_ops="Página Operator del ATFS1", alt_ext="Página Play del ATFS1",
),
}



# ───────────────────────── Orrery ─────────────────────────
O = {
"en": dict(
    title="Orrery — a free generative MIDI instrument for ambient, drone and calm music | Audio Tools",
    description=f"Orrery is a free, open-source generative MIDI instrument: eight lanes, each on its own MIDI channel, play drones, pads, melodies and clouds over a slow shared harmony, for ambient, dark ambient, drone, sleep and meditation. It plays any software or hardware synth. For macOS, Windows and Linux.",
    listen_p="Every clip is Orrery generating live from a factory preset, with no edits. Each lane was then played by one of our instruments from the MIDI it generated, and the lanes were mixed at a matched level. Download the MIDI and send it to your own synths.",
    sound_h="Generative, not random.",
    sound_p1="All the lanes listen to one slow harmony. Chords change by fourths, fifths and thirds, pads move each voice to the nearest note of the next chord, melodies walk the scale in phrases with rests, and every note stays inside the key unless you raise the tension. Lanes can record what they play and repeat it as a loop of their own length, with small mutations each pass: loops of 17 and 23 beats only line up again after 391 beats.",
    sound_p2="It was tested like the instruments: ten minutes of seven lanes with no note outside the scale and no hanging note at stop, the same variation number playing the same notes, note for note, on macOS, Windows and Linux and at any buffer size or sample rate, and at most 37 MIDI messages per second on a channel across all factory presets, where a 5-pin MIDI cable carries about a thousand.",
    spec=[("Lanes", "8, each with its own MIDI channel and role: drone, pad, melody, arpeggio, cloud, bass or pulse"),
          ("Harmony", "16 scales and modes, chord colour from triads to elevenths, tension from consonant to clusters and tritones, slow walk, drone or two-chord progressions"),
          ("Loops", "record and repeat per lane, any length from 2 to 128 beats, with mutation"),
          ("Rhythm", "Euclidean patterns, probability, rates from 2 beats to 1/8, humanized timing"),
          ("Expression", "MIDI echoes, a controller curve per lane (sine, drift, breath, swell), pitch-bend drift for analog warmth"),
          ("Session", "length with fades in and out, breathing pace, gentle mode"),
          ("Outputs", "DAW MIDI, direct MIDI ports per lane, MIDI clock, Lead and Follow link between instances, built-in preview sound")],
    demo_h="See it play.",
    demo_p="Each video is the plugin's own screen while it plays a factory preset. The sound is the clip from Hear it: the same variation makes the same notes, so every spark on screen lands on a note you hear.",
    demo_tabs="Demos",
    moods_h="Pick a mood. Press play.",
    moods_p=f"The Simple page is all most people need: seven moods with {NOR} presets, five knobs that shape everything at once (density, motion, space, brightness and tension), a key, a scale and a session timer. The orrery in the middle shows what is playing: one orbit per lane, the chord in the sun, a spark for every note.",
    moods_caption="The Simple page playing Tape Loops: each planet travels at its lane's loop length.",
    lanes_h="Every lane under your hand.",
    lanes_p="The Advanced pages open the whole machine, one column per lane.",
    lanes_facts=[("Roles.", "Drones hold the root, pads voice-lead the chords, melodies phrase and rest, arpeggios and pulses follow Euclidean patterns, clouds scatter glass-like notes, basses anchor."),
                 ("Eno loops.", "Give lanes loops of different lengths and they drift in and out of phase for hours."),
                 ("Controllers.", "Each lane can draw a slow curve on any CC (74 for the filter, 1 for the mod wheel, 11 for expression) and wander in pitch like an old oscillator."),
                 ("Capture.", "Save the last minutes as a MIDI file, one track per channel, and drag it into your DAW.")],
    lanes_caption="The Lanes page: 29 settings for each of the 8 lanes.",
    out_h="Plays your hardware, and every DAW.",
    out_p="Every lane always reaches the DAW on its own channel. Beyond that, each lane can go straight to a MIDI port: a hardware synth's USB or MIDI interface, or a virtual port to reach other tracks.",
    out_facts=[("Hardware.", "Pick a port per lane, send MIDI clock, and one Orrery plays a whole rack of synths."),
               ("Ableton Live.", "Live merges channels between tracks, so use a virtual port (IAC on macOS, loopMIDI on Windows) or one Orrery per track with Link: the Lead shares its chords and the Followers play over them."),
               ("Tuning.", "A reference pitch from 415 to 466 Hz applied by pitch bend, for synths that cannot retune."),
               ("Preview sound.", "A small built-in synth with a long reverb, so it plays the moment you open it.")],
    out_caption="The Outputs page: a port and a channel for each lane, clock, link and tuning.",
    presets_h="Seven moods.",
    presets=[(PCOR.get("Ambient", 0), "Ambient", "Slow chords, a voice and glass in the distance, and Eno-style tape loops."),
             (PCOR.get("Dark Ambient", 0), "Dark ambient", "Low, minor and unresolved: Phrygian, harmonic minor and drifting pitch."),
             (PCOR.get("Drone", 0), "Drone", "A centre that barely moves, a tanpura pattern and a monolith."),
             (PCOR.get("Sleep", 0) + PCOR.get("Meditation", 0), "Sleep and meditation", "Long sessions with slow fades, breathing pace, gentle mode."),
             (PCOR.get("Focus", 0), "Focus", "Steady and quiet, with no surprises."),
             (PCOR.get("Celestial", 0), "Celestial", "Bright, high and wide: Lydian, whole-tone, starfields.")],
    care="Orrery is a music tool for relaxation and focus. It is not a medical device and makes no therapeutic claims.",
    dl_h=f"Download Orrery {VOR}.",
    dl_p="Free, no account, no activation, no telemetry.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit and standalone app",
    mac_steps=["Unzip the file.", "Copy <code>Orrery.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy <code>Orrery.component</code> to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "In Ableton Live use the VST3: Audio Units cannot send MIDI to other tracks there."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3 and standalone app",
    win_steps=["Unzip the file.", "Copy <code>Orrery.vst3</code> to <code>C:\\\\Program Files\\\\Common Files\\\\VST3</code>.",
               "Rescan plugins in your DAW.", "<code>Orrery.exe</code> runs on its own and plays your MIDI ports."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed plugins, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps.",
    os_p="Orrery is free software under the GNU Affero General Public License v3, like the JUCE framework it is built on. You can use it, study it, change it and share it. The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    os_link="Why Audio Tools is open source",
    trademark="Ableton is a trademark of Ableton AG. Audio Tools is an independent project.",
    photo_credit="The brass orrery in the images is a 3D model made for this page. The loop technique is the one Brian Eno described for Music for Airports (1978).",
    hero_alt="A brass orrery rendered in 3D: a glowing sun on a column and eight coloured planets on arms of different lengths, over a dark base.",
    alt_synth="Orrery's Simple page playing", alt_lanes="Orrery's Lanes page", alt_out="Orrery's Outputs page",
),
"es": dict(
    title="Orrery — un instrumento MIDI generativo libre para ambient, drone y música tranquila | Audio Tools",
    description=f"Orrery es un instrumento MIDI generativo libre y de código abierto: ocho carriles, cada uno en su canal MIDI, tocan drones, pads, melodías y nubes sobre una armonía lenta compartida, para ambient, dark ambient, drone, dormir y meditar. Toca cualquier sinte virtual o físico. Para macOS, Windows y Linux.",
    listen_p="Cada clip es Orrery generando en vivo desde un preset de fábrica, sin ediciones. Después, cada carril lo tocó uno de nuestros instrumentos con el MIDI que generó, y se mezclaron a un nivel parejo. Bajá el MIDI y mandalo a tus propios sintes.",
    sound_h="Generativo, no al azar.",
    sound_p1="Todos los carriles escuchan una misma armonía lenta. Los acordes cambian por cuartas, quintas y terceras, los pads llevan cada voz a la nota más cercana del acorde siguiente, las melodías caminan la escala en frases con silencios, y ninguna nota sale de la tonalidad salvo que subas la tensión. Cada carril puede grabar lo que toca y repetirlo como un lazo de su propio largo, con pequeñas mutaciones en cada vuelta: lazos de 17 y 23 pulsos recién vuelven a coincidir a los 391.",
    sound_p2="Se probó como los instrumentos: diez minutos de siete carriles sin una nota fuera de la escala ni notas colgadas al parar, el mismo número de variación tocando las mismas notas, nota por nota, en macOS y en Windows y con cualquier tamaño de buffer o frecuencia de muestreo, y como mucho 37 mensajes MIDI por segundo en un canal entre todos los presets de fábrica, cuando un cable MIDI de 5 pines lleva unos mil.",
    spec=[("Carriles", "8, cada uno con su canal MIDI y su rol: drone, pad, melodía, arpegio, nube, bajo o pulso"),
          ("Armonía", "16 escalas y modos, color de acorde de tríadas a oncenas, tensión de consonante a clusters y tritonos, progresiones de caminata lenta, drone o dos acordes"),
          ("Lazos", "grabar y repetir por carril, de 2 a 128 pulsos, con mutación"),
          ("Ritmo", "patrones euclidianos, probabilidad, valores de 2 pulsos a 1/8, tiempo humanizado"),
          ("Expresión", "ecos MIDI, una curva de controlador por carril (seno, deriva, respiración, crecida), deriva de afinación por pitch bend para el calor analógico"),
          ("Sesión", "duración con fades de entrada y salida, ritmo de respiración, modo suave"),
          ("Salidas", "MIDI al DAW, puertos MIDI directos por carril, reloj MIDI, vínculo líder y seguidoras entre instancias, sonido de prueba incorporado")],
    demo_h="Miralo tocar.",
    demo_p="Cada video es la pantalla del plugin mientras toca un preset de fábrica. El sonido es el clip de Escuchalo: la misma variación genera las mismas notas, así que cada chispa en pantalla cae sobre una nota que escuchás.",
    demo_tabs="Demos",
    moods_h="Elegí un clima. Apretá Play.",
    moods_p=f"La página Simple es todo lo que la mayoría necesita: siete climas con {NOR} presets, cinco perillas que dan forma a todo a la vez (densidad, movimiento, espacio, brillo y tensión), una tonalidad, una escala y un temporizador de sesión. El orrery del centro muestra lo que suena: una órbita por carril, el acorde en el sol, una chispa por cada nota.",
    moods_caption="La página Simple tocando Tape Loops: cada planeta gira al largo del lazo de su carril.",
    lanes_h="Cada carril en tus manos.",
    lanes_p="Las páginas avanzadas abren toda la máquina, una columna por carril.",
    lanes_facts=[("Roles.", "Los drones sostienen la raíz, los pads conducen las voces de los acordes, las melodías frasean y descansan, arpegios y pulsos siguen patrones euclidianos, las nubes reparten notas de cristal y los bajos anclan."),
                 ("Lazos de Eno.", "Con lazos de largos distintos, los carriles entran y salen de fase durante horas."),
                 ("Controladores.", "Cada carril puede dibujar una curva lenta en cualquier CC (74 para el filtro, 1 para la rueda, 11 para la expresión) y derivar en afinación como un oscilador viejo."),
                 ("Captura.", "Guardás los últimos minutos como archivo MIDI, una pista por canal, y lo arrastrás al DAW.")],
    lanes_caption="La página de carriles: 29 ajustes para cada uno de los 8 carriles.",
    out_h="Toca tus sintes físicos, y en cualquier DAW.",
    out_p="Cada carril llega siempre al DAW por su propio canal. Además, cada carril puede salir directo a un puerto MIDI: la interfaz USB o MIDI de un sinte físico, o un puerto virtual para llegar a otras pistas.",
    out_facts=[("Hardware.", "Elegís un puerto por carril, mandás reloj MIDI, y una sola Orrery toca un rack entero de sintes."),
               ("Ableton Live.", "Live junta los canales entre pistas, así que usá un puerto virtual (IAC en macOS, loopMIDI en Windows) o una Orrery por pista con el vínculo: la líder comparte sus acordes y las seguidoras tocan encima."),
               ("Afinación.", "Una referencia de 415 a 466 Hz aplicada por pitch bend, para sintes que no se pueden reafinar."),
               ("Sonido de prueba.", "Un sinte chico incorporado con una reverb larga, para que suene apenas lo abrís.")],
    out_caption="La página de salidas: un puerto y un canal por carril, reloj, vínculo y afinación.",
    presets_h="Siete climas.",
    presets=[(PCOR.get("Ambient", 0), "Ambient", "Acordes lentos, una voz y cristal a lo lejos, y lazos de cinta a lo Eno."),
             (PCOR.get("Dark Ambient", 0), "Dark ambient", "Grave, menor y sin resolver: frigio, menor armónica y afinación a la deriva."),
             (PCOR.get("Drone", 0), "Drone", "Un centro que casi no se mueve, un patrón de tanpura y un monolito."),
             (PCOR.get("Sleep", 0) + PCOR.get("Meditation", 0), "Dormir y meditar", "Sesiones largas con fades lentos, ritmo de respiración, modo suave."),
             (PCOR.get("Focus", 0), "Foco", "Parejo y tranquilo, sin sorpresas."),
             (PCOR.get("Celestial", 0), "Celestial", "Brillante, agudo y abierto: lidio, tonos enteros, campos de estrellas.")],
    care="Orrery es una herramienta musical para relajarse y concentrarse. No es un dispositivo médico ni promete efectos terapéuticos.",
    dl_h=f"Descargá Orrery {VOR}.",
    dl_p="Gratis, sin cuenta, sin activación y sin telemetría.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit y aplicación independiente",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>Orrery.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá <code>Orrery.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "En Ableton Live usá el VST3: ahí los Audio Units no pueden mandar MIDI a otras pistas."],
    win_h="Windows", win_req="Windows 10 u 11 de 64 bits.", win_fmt="VST3 y aplicación independiente",
    win_steps=["Descomprimí el archivo.", "Copiá <code>Orrery.vst3</code> a <code>C:\\\\Program Files\\\\Common Files\\\\VST3</code>.",
               "Reescaneá los plugins en tu DAW.", "<code>Orrery.exe</code> funciona sola y toca tus puertos MIDI."],
    file_label="archivo",
    dl_note="Los builds todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los plugins instalados, y SmartScreen de Windows puede pedir confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos.",
    os_p="Orrery es software libre bajo la GNU Affero General Public License v3, como el framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    os_link="Por qué Audio Tools es de código abierto",
    trademark="Ableton es una marca de Ableton AG. Audio Tools es un proyecto independiente.",
    photo_credit="El orrery de bronce de las imágenes es un modelo 3D hecho para esta página. La técnica de los lazos es la que Brian Eno describió para Music for Airports (1978).",
    hero_alt="Un orrery de bronce renderizado en 3D: un sol encendido sobre una columna y ocho planetas de colores en brazos de distinto largo, sobre una base oscura.",
    alt_synth="La página Simple de Orrery tocando", alt_lanes="La página de carriles de Orrery", alt_out="La página de salidas de Orrery",
),
}

# ───────────────────────── ATMD ─────────────────────────
D = {
"en": dict(
    title="ATMD — a free Minimoog Model D recreation with the transistor ladder filter | Audio Tools",
    description="ATMD is a free, open-source recreation of the Moog Minimoog Model D: three oscillators, the mixer that drives the 24 dB transistor ladder filter, two contours, glide and the wheels, with polyphony, delay and reverb as extensions. VST3, Audio Unit and standalone, for macOS, Windows and Linux.",
    listen_p="Every clip is the engine on its own, playing a factory preset from a MIDI file written for it. Nothing added afterwards, only the level matched across clips. Download the MIDI and play it through the preset yourself.",
    sound_h="The bass that defined the bass.",
    sound_p1="The Model D's sound comes from a short chain pushed hard: three oscillators into a passive mixer that feeds the transistor ladder directly. Turn two or three sources up and the ladder saturates before it filters. ATMD models that chain from the 1971 schematics and the service manual, and solves the ladder's feedback loop without delay, with a saturating stage in every pole.",
    sound_p2="It was checked against the original's numbers: one octave per division on the cutoff dial with −1.1 at 440 Hz, self-oscillation from EMPHASIS 8, resonance that fades with a low cutoff, the bass loss when the emphasis rises, contours from 1 ms to 10 s and glide measured in time per octave.",
    spec=[("Oscillators", "three, LO and 32' to 2', six waveforms each (osc 3 with a reverse sawtooth), osc 3 as a modulator with keyboard control off"),
          ("Mixer", "five channels with on/off: three oscillators, external input with OVERLOAD lamp, white or pink noise"),
          ("Filter", "four-pole transistor ladder, 24 dB/oct, saturation in every stage, keyboard control 1/3, 2/3 or full"),
          ("Contours", "filter and loudness, attack, decay and sustain; DECAY switch for the release, as on the original"),
          ("Keyboard", "44 keys F to C, low-note priority, single trigger, glide; pitch and modulation wheels"),
          ("Extensions", "up to 8 voices and 4-voice unison, velocity and pressure, note priority and trigger choice, delay, reverb")],
    panel_h="The whole instrument, under your hand.",
    panel_p1="Every knob, rocker and wheel of the original works, drawn from a 3D model built to the panel's real measurements: knobs turn, rockers flip, the wheels move with your MIDI controller, the keys go down. The help line explains each control as you point at it, and the manual is one click away.",
    panel_caption="The panel playing Lucky Lead, with a chord held on the keyboard.",
    voice_h="Three oscillators, one filter, one voice.",
    voice_p="The signal path is the original's, in order.",
    voice_facts=[("Oscillators.", "Triangle, triangle-sawtooth, sawtooth, square and two pulses; oscillator 3 swaps the triangle-sawtooth for a reverse sawtooth and, free from the keyboard, becomes the modulator."),
                 ("Mixer into filter.", "Turn several sources up and the ladder saturates: the thick, compressed sound the Model D is known for."),
                 ("Ladder.", "Four transistor stages with feedback: 24 dB per octave, a singing self-oscillation that follows the keyboard, and thinner bass as the emphasis rises."),
                 ("Contours.", "Attack, decay and sustain from 1 ms to 10 s; the DECAY switch decides whether the note dies away or stops when you lift the key.")],
    voice_caption="The 3D model: oscillator bank, mixer and modifiers.",
    ext_h="What the original never had.",
    ext_p="The SETTINGS page sits on top of the panel and leaves the keyboard playable.",
    ext_facts=[("Voices.", "Up to eight voices for chords, or up to four stacked on one note with detune and stereo spread."),
               ("Expression.", "Velocity and pressure to the filter, the loudness or the modulation, and the pitch wheel range."),
               ("Keyboard.", "Low, last or high note priority, single or multi trigger."),
               ("Effects.", "A tempo-synced delay and a plate reverb, after the voice.")],
    ext_caption="The SETTINGS page: voices, expression, keyboard, delay, reverb and system.",
    presets_h=f"{NMD} sounds to start from.",
    presets=[(PCMD.get("Bass", 0), "Bass", "Fat, plucked, squelchy and sub: the reason the Model D exists."),
             (PCMD.get("Lead", 0), "Lead", "Gliding prog solos, square whistles, a singing filter and fifths."),
             (PCMD.get("Brass and keys", 0), "Brass and keys", "Brass sections, soft horns, organ, electric piano and harpsichord."),
             (PCMD.get("Effects and percussion", 0), "Effects and percussion", "Kick, snare, hi-hat, wind, laser zaps and bird calls, from the panel alone."),
             (PCMD.get("Drones and pads", 0), "Drones and pads", "The polyphonic extension: strings, unison stacks, slow sweeps and drones.")],
    dl_h=f"Download ATMD {VMD}.",
    dl_p="Free, no account, no activation, no telemetry.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit and standalone app",
    mac_steps=["Unzip the file.", "Copy <code>ATMD.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy <code>ATMD.component</code> to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Rescan plugins in your DAW and load ATMD on an instrument track."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3 and standalone app",
    win_steps=["Unzip the file.", "Copy <code>ATMD.vst3</code> to <code>C:\\\\Program Files\\\\Common Files\\\\VST3</code>.",
               "Rescan plugins in your DAW.", "<code>ATMD.exe</code> runs on its own with your MIDI keyboard."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed plugins, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps.",
    os_p="ATMD is free software under the GNU Affero General Public License v3, like the JUCE framework it is built on. You can use it, study it, change it and share it. The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    os_link="Why Audio Tools is open source",
    trademark="Moog and Minimoog are trademarks of Moog Music Inc. Audio Tools is an independent project, not affiliated with or endorsed by Moog Music. Product names are used only to identify the historical instrument that inspired this recreation.",
    photo_credit="The instrument in the images is a 3D model made for this project from the panel's published measurements and public-domain and Creative Commons photographs. All graphics on the ATMD panel are our own.",
    hero_alt="The ATMD rendered in 3D: a black panel tilted up over a walnut case, with chrome-capped knobs, orange and blue rockers and a 44-key keyboard.",
    alt_synth="ATMD's panel", alt_ext="ATMD's SETTINGS page", alt_voice="Close-up of the 3D panel",
),
"es": dict(
    title="ATMD — una recreación libre del Minimoog Model D con el filtro escalera | Audio Tools",
    description="ATMD es una recreación libre y de código abierto del Moog Minimoog Model D: tres osciladores, el mezclador que satura el filtro escalera de transistores de 24 dB, dos contornos, glide y las ruedas, con polifonía, delay y reverb como extensiones. VST3, Audio Unit y aplicación independiente, para macOS, Windows y Linux.",
    listen_p="Cada clip es el motor solo, tocando un preset de fábrica desde un MIDI escrito para él. No se le agregó nada después; sólo se igualó el nivel entre clips. Bajá el MIDI y tocalo vos con el mismo preset.",
    sound_h="El bajo que definió al bajo.",
    sound_p1="El sonido del Model D sale de una cadena corta y exigida: tres osciladores a un mezclador pasivo que entra directo a la escalera de transistores. Subís dos o tres fuentes y la escalera satura antes de filtrar. ATMD modela esa cadena a partir de los esquemas de 1971 y el manual de servicio, y resuelve el lazo de realimentación de la escalera sin retardo, con una etapa que satura en cada polo.",
    sound_p2="Se verificó contra los números del original: una octava por división en el dial de corte con −1,1 en 440 Hz, autooscilación desde EMPHASIS 8, resonancia que se apaga con el corte bajo, la pérdida de graves al subir el énfasis, contornos de 1 ms a 10 s y glide medido en tiempo por octava.",
    spec=[("Osciladores", "tres, LO y de 32' a 2', seis formas cada uno (el osc 3 con sierra invertida), el osc 3 como modulador sin control de teclado"),
          ("Mezclador", "cinco canales con encendido: tres osciladores, entrada externa con lámpara OVERLOAD, ruido blanco o rosa"),
          ("Filtro", "escalera de transistores de cuatro polos, 24 dB/oct, saturación en cada etapa, control de teclado de 1/3, 2/3 o completo"),
          ("Contornos", "de filtro y de volumen, attack, decay y sustain; el interruptor DECAY para el release, como en el original"),
          ("Teclado", "44 teclas de Fa a Do, prioridad a la grave, disparo único, glide; ruedas de pitch y de modulación"),
          ("Extensiones", "hasta 8 voces y unísono de 4, velocidad y presión, prioridad y disparo elegibles, delay, reverb")],
    panel_h="El instrumento entero, en tus manos.",
    panel_p1="Cada perilla, rocker y rueda del original funciona, dibujada desde un modelo 3D hecho con las medidas reales del panel: las perillas giran, los rockers se vuelcan, las ruedas se mueven con tu controlador MIDI, las teclas bajan. La línea de ayuda explica cada control cuando le pasás el mouse, y el manual está a un clic.",
    panel_caption="El panel tocando Lucky Lead, con un acorde en el teclado.",
    voice_h="Tres osciladores, un filtro, una voz.",
    voice_p="El camino de la señal es el del original, en orden.",
    voice_facts=[("Osciladores.", "Triángulo, triángulo-sierra, sierra, cuadrada y dos pulsos; el osc 3 cambia el triángulo-sierra por una sierra invertida y, libre del teclado, se vuelve el modulador."),
                 ("Mezclador al filtro.", "Subís varias fuentes y la escalera satura: el sonido grueso y comprimido por el que se conoce al Model D."),
                 ("Escalera.", "Cuatro etapas de transistores con realimentación: 24 dB por octava, una autooscilación que canta y sigue al teclado, y graves más finos al subir el énfasis."),
                 ("Contornos.", "Attack, decay y sustain de 1 ms a 10 s; el interruptor DECAY decide si la nota se apaga de a poco o se corta al soltar la tecla.")],
    voice_caption="El modelo 3D: banco de osciladores, mezclador y modificadores.",
    ext_h="Lo que el original nunca tuvo.",
    ext_p="La página SETTINGS se pone encima del panel y deja el teclado para tocar.",
    ext_facts=[("Voces.", "Hasta ocho voces para acordes, o hasta cuatro apiladas en una nota con desafinación y apertura estéreo."),
               ("Expresión.", "Velocidad y presión al filtro, al volumen o a la modulación, y el rango de la rueda de pitch."),
               ("Teclado.", "Prioridad a la grave, a la última o a la aguda, disparo único o múltiple."),
               ("Efectos.", "Un delay sincronizado al tempo y una reverb de placa, después de la voz.")],
    ext_caption="La página SETTINGS: voces, expresión, teclado, delay, reverb y sistema.",
    presets_h=f"{NMD} sonidos para arrancar.",
    presets=[(PCMD.get("Bass", 0), "Bajos", "Gordos, punteados, chirriantes y sub: la razón de ser del Model D."),
             (PCMD.get("Lead", 0), "Leads", "Solos progresivos con glide, silbidos de cuadrada, un filtro que canta y quintas."),
             (PCMD.get("Brass and keys", 0), "Bronces y teclados", "Secciones de bronces, cornos suaves, órgano, piano eléctrico y clave."),
             (PCMD.get("Effects and percussion", 0), "Efectos y percusión", "Bombo, redoblante, hi-hat, viento, disparos láser y pájaros, sólo con el panel."),
             (PCMD.get("Drones and pads", 0), "Drones y colchones", "La extensión polifónica: cuerdas, unísonos, barridos lentos y drones.")],
    dl_h=f"Descargá ATMD {VMD}.",
    dl_p="Gratis, sin cuenta, sin activación, sin telemetría.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit y aplicación independiente",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>ATMD.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá <code>ATMD.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Volvé a escanear los plugins en tu DAW y cargá ATMD en una pista de instrumento."],
    win_h="Windows", win_req="Windows 10 u 11, de 64 bits.", win_fmt="VST3 y aplicación independiente",
    win_steps=["Descomprimí el archivo.", "Copiá <code>ATMD.vst3</code> a <code>C:\\\\Program Files\\\\Common Files\\\\VST3</code>.",
               "Volvé a escanear los plugins en tu DAW.", "<code>ATMD.exe</code> funciona solo con tu teclado MIDI."],
    file_label="archivo",
    dl_note="Los binarios todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los plugins instalados, y SmartScreen de Windows puede pedirte confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos.",
    os_p="ATMD es software libre bajo la GNU Affero General Public License v3, como el framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    os_link="Por qué Audio Tools es de código abierto",
    trademark="Moog y Minimoog son marcas de Moog Music Inc. Audio Tools es un proyecto independiente, sin relación ni aval de Moog Music. Los nombres de productos se usan sólo para identificar el instrumento histórico que inspiró esta recreación.",
    photo_credit="El instrumento de las imágenes es un modelo 3D hecho para este proyecto con las medidas publicadas del panel y fotos de dominio público y Creative Commons. Toda la gráfica del panel del ATMD es propia.",
    hero_alt="El ATMD renderizado en 3D: un panel negro inclinado sobre un gabinete de nogal, con perillas de tapa cromada, rockers naranjas y azules y un teclado de 44 teclas.",
    alt_synth="El panel del ATMD", alt_ext="La página SETTINGS del ATMD", alt_voice="Primer plano del panel 3D",
),
}

# ───────────────────────── Murmur ─────────────────────────
M = {
"en": dict(
    title="Murmur — a free real-time granular effect: clouds, shimmer, freeze and stretch | Audio Tools",
    description="Murmur is a free, open-source granular effect for macOS, Windows and Linux. Up to 128 grains read the last 16 seconds of your sound and turn it into clouds, shimmering pads, frozen chords, rhythmic stutters and glitches. VST3, Audio Unit and standalone.",
    listen_p="Every clip is a clip from our instruments played through a Murmur factory preset, offline, with no edits: the source is named in each line. Only the level was matched across clips.",
    sound_h="A flock made of your sound.",
    sound_p1="Everything that comes in is written into a memory of up to 16 seconds. Up to 128 grains, from 5 ms to 2 s long, read that memory at once, each with its own place, pitch, direction, fade and position in the stereo field. Short and many, they blur into a cloud. Long and few, they overlap like echoes of phrases. Detuned, they become a chord.",
    sound_p2="Freeze stops the recording and the grains keep playing what was there, for as long as you want. Scan moves the reading point through the memory at its own speed, so a sound can last ten times longer without getting deeper. Feedback sends the cloud back into the memory, and with octave grains it rises like a choir.",
    spec=[("Memory", "1 to 16 seconds, recorded continuously; Freeze from the button or the sustain pedal"),
          ("Grains", "up to 128 at once, 5 ms to 2 s, 0.5 to 200 per second, cubic interpolation"),
          ("Pitch", "±24 semitones, fine tune, random spread free or snapped to semitones, octaves, fifths, a major or minor chord, the pentatonic or the whole-tone scale; octave-up and octave-down grains"),
          ("Time", "position, spray, and scan from −2 to +2 times speed; reverse grains"),
          ("Rhythm", "grains on the host grid from 1 bar to 1/32, with triplets, dotted notes and chance"),
          ("Movement", "two LFOs with five shapes, free or synced, on 9 targets"),
          ("After the grains", "low and high cut, feedback, ducking, tempo-synced delay and plate reverb"),
          ("MIDI", "Pitch mode plays the grains' pitch from a keyboard; Gate mode plays the cloud only while keys are held")],
    flock_h="See what you hear.",
    flock_p="The window draws the memory as a horizon and each grain as a bird: its place is the moment it reads, its height is its pitch, its size follows its fade, and grains playing backwards fly the other way. The light band shows where grains are born. The Simple page puts the flock next to the knobs that matter most.",
    flock_caption="The Simple page: the flock over the last seconds of a voice, with the six big knobs of the cloud.",
    adv_h="Every grain under your hand.",
    adv_p="The Advanced page shows all 48 controls in eight groups, and the help line explains each one as you point at it.",
    adv_facts=[("Grains.", "Size, density, their randomness, a voice limit, and rhythmic triggering on the host's grid with a chance to skip."),
               ("Pitch.", "Transpose, fine tune and a random spread snapped to chords and scales, plus shimmer an octave up and sub an octave down."),
               ("Shape.", "Grain envelopes from a soft bell to a hard gate, skewed for swells or plucks, spread across the stereo field."),
               ("LFOs.", "Two of them, on position, spray, size, density, pitch, pan, tone, mix or feedback.")],
    adv_caption="The Advanced page: grains, time, pitch, envelope and stereo, tone, two LFOs, effects and MIDI.",
    play_h="Freeze it. Stretch it. Play it.",
    play_p="A granular effect becomes an instrument when you can hold a sound and choose its notes.",
    play_facts=[("Freeze.", "Hold the sustain pedal or press Freeze and the memory stops; the grains keep the chord alive, the horizon turns teal."),
                ("Stretch.", "With Freeze on, Scan walks through the frozen sound as slowly as you like, without changing its pitch."),
                ("Play the cloud.", "In MIDI Pitch mode the keys you hold choose the grains' pitches; play a chord and the cloud sings it."),
                ("Gate.", "In MIDI Gate mode the cloud plays only while a key is held: rhythmic swells from your sequencer.")],
    play_caption="Frozen: the horizon stops and the grains keep reading what was there.",
    presets_h="Thirty starting points.",
    presets=[(PCMM.get("Clouds", 0), "Clouds", "From thin fog to a dense flock of short grains."),
             (PCMM.get("Shimmer", 0) + PCMM.get("Pitch", 0), "Shimmer and pitch", "Octaves, fifths, major and minor sparkle, a drone a fifth wide."),
             (PCMM.get("Stretch", 0), "Stretch", "Freeze and scan: a sound held and stretched for minutes."),
             (PCMM.get("Rhythm", 0), "Rhythm", "Grains on the beat: eighths, sixteenths, triplets and gated swells."),
             (PCMM.get("Texture", 0) + PCMM.get("Glitch", 0), "Texture and glitch", "Tape scatter, underwater washes, broken radios and skipping CDs."),
             (PCMM.get("MIDI", 0), "MIDI", "Presets made to be played from a keyboard.")],
    dl_h=f"Download Murmur {VMM}.",
    dl_p="Free, no account, no activation, no telemetry.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit and standalone app",
    mac_steps=["Unzip the file.", "Copy <code>Murmur.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy <code>Murmur.component</code> to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Rescan plugins in your DAW and insert Murmur on an audio track, a bus or a send."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3 and standalone app",
    win_steps=["Unzip the file.", "Copy <code>Murmur.vst3</code> to <code>C:\\\\Program Files\\\\Common Files\\\\VST3</code>.",
               "Rescan plugins in your DAW.", "<code>Murmur.exe</code> runs on its own and processes your audio input."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed plugins, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps.",
    os_p="Murmur is free software under the GNU Affero General Public License v3, like the JUCE framework it is built on. You can use it, study it, change it and share it. The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    os_link="Why Audio Tools is open source",
    trademark="Audio Tools is an independent project.",
    photo_credit="The murmuration in the images is a 3D scene made for this page. The sounds fed into Murmur on this page are clips of our own instruments.",
    hero_alt="A murmuration of starlings rendered in 3D, a dark flowing cloud over a low horizon at dusk.",
    alt_synth="Murmur's Simple page with the flock", alt_adv="Murmur's Advanced page", alt_frozen="Murmur frozen",
),
"es": dict(
    title="Murmur — un efecto granular libre en tiempo real: nubes, shimmer, freeze y estiramiento | Audio Tools",
    description="Murmur es un efecto granular libre y de código abierto para macOS, Windows y Linux. Hasta 128 granos leen los últimos 16 segundos de tu sonido y lo convierten en nubes, pads brillantes, acordes congelados, tartamudeos rítmicos y glitches. VST3, Audio Unit y aplicación independiente.",
    listen_p="Cada clip es un clip de nuestros instrumentos pasado por un preset de fábrica de Murmur, sin edición: cada línea dice de dónde sale el sonido. Sólo se igualó el nivel entre clips.",
    sound_h="Una bandada hecha de tu sonido.",
    sound_p1="Todo lo que entra se graba en una memoria de hasta 16 segundos. Hasta 128 granos, de 5 ms a 2 s, leen esa memoria a la vez, cada uno con su lugar, su altura, su sentido, su envolvente y su posición en el estéreo. Cortos y muchos, se funden en una nube. Largos y pocos, se superponen como ecos de frases. Desafinados, se vuelven un acorde.",
    sound_p2="Freeze frena la grabación y los granos siguen tocando lo que había, todo el tiempo que quieras. Scan mueve el punto de lectura por la memoria a su propia velocidad, así un sonido puede durar diez veces más sin ponerse más grave. Feedback devuelve la nube a la memoria, y con granos una octava arriba sube como un coro.",
    spec=[("Memoria", "de 1 a 16 segundos, grabando siempre; Freeze desde el botón o el pedal de sustain"),
          ("Granos", "hasta 128 a la vez, de 5 ms a 2 s, de 0,5 a 200 por segundo, interpolación cúbica"),
          ("Altura", "±24 semitonos, afinación fina, dispersión al azar libre o ajustada a semitonos, octavas, quintas, un acorde mayor o menor, la escala pentatónica o la de tonos enteros; granos una octava arriba y una abajo"),
          ("Tiempo", "posición, dispersión, y scan de −2 a +2 veces la velocidad; granos al revés"),
          ("Ritmo", "granos en la grilla del host de 1 compás a 1/32, con tresillos, puntillos y probabilidad"),
          ("Movimiento", "dos LFO con cinco formas, libres o sincronizados, sobre 9 destinos"),
          ("Después de los granos", "corte de graves y agudos, feedback, ducking, delay sincronizado y reverb de placa"),
          ("MIDI", "el modo Pitch toca la altura de los granos desde un teclado; el modo Gate hace sonar la nube sólo mientras hay teclas apretadas")],
    flock_h="Mirá lo que escuchás.",
    flock_p="La ventana dibuja la memoria como un horizonte y cada grano como un pájaro: su lugar es el momento que lee, su altura es su afinación, su tamaño sigue a su envolvente, y los granos que suenan al revés vuelan para el otro lado. La franja clara muestra dónde nacen los granos. La página Simple pone la bandada al lado de las perillas que más importan.",
    flock_caption="La página Simple: la bandada sobre los últimos segundos de una voz, con las seis perillas grandes de la nube.",
    adv_h="Cada grano en tus manos.",
    adv_p="La página Advanced muestra los 48 controles en ocho grupos, y la línea de ayuda explica cada uno cuando le pasás el mouse.",
    adv_facts=[("Granos.", "Tamaño, densidad, su azar, un límite de voces, y disparo rítmico en la grilla del host con probabilidad de saltear."),
               ("Altura.", "Transposición, afinación fina y una dispersión al azar ajustada a acordes y escalas, más shimmer una octava arriba y sub una abajo."),
               ("Forma.", "Envolventes de grano desde una campana suave hasta una compuerta dura, inclinadas para crecer o picar, repartidas en el estéreo."),
               ("LFO.", "Dos, sobre posición, dispersión, tamaño, densidad, altura, paneo, tono, mezcla o feedback.")],
    adv_caption="La página Advanced: granos, tiempo, altura, envolvente y estéreo, tono, dos LFO, efectos y MIDI.",
    play_h="Congelalo. Estiralo. Tocalo.",
    play_p="Un efecto granular se vuelve instrumento cuando podés retener un sonido y elegir sus notas.",
    play_facts=[("Freeze.", "Apretá el pedal de sustain o Freeze y la memoria se detiene; los granos mantienen vivo el acorde y el horizonte se pone verde agua."),
                ("Estirar.", "Con Freeze, Scan recorre el sonido congelado tan despacio como quieras, sin cambiar su altura."),
                ("Tocar la nube.", "En el modo MIDI Pitch las teclas que apretás eligen la altura de los granos; tocás un acorde y la nube lo canta."),
                ("Gate.", "En el modo MIDI Gate la nube suena sólo mientras hay una tecla apretada: crecidas rítmicas desde tu secuenciador.")],
    play_caption="Congelado: el horizonte se detiene y los granos siguen leyendo lo que había.",
    presets_h="Treinta puntos de partida.",
    presets=[(PCMM.get("Clouds", 0), "Nubes", "Desde una niebla fina hasta una bandada densa de granos cortos."),
             (PCMM.get("Shimmer", 0) + PCMM.get("Pitch", 0), "Shimmer y altura", "Octavas, quintas, brillo mayor y menor, un drone de una quinta."),
             (PCMM.get("Stretch", 0), "Estirar", "Freeze y scan: un sonido retenido y estirado durante minutos."),
             (PCMM.get("Rhythm", 0), "Ritmo", "Granos a tempo: corcheas, semicorcheas, tresillos y crecidas con compuerta."),
             (PCMM.get("Texture", 0) + PCMM.get("Glitch", 0), "Textura y glitch", "Cinta dispersa, lavados bajo el agua, radios rotas y CD que saltan."),
             (PCMM.get("MIDI", 0), "MIDI", "Presets hechos para tocarse desde un teclado.")],
    dl_h=f"Descargá Murmur {VMM}.",
    dl_p="Gratis, sin cuenta, sin activación, sin telemetría.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit y aplicación independiente",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>Murmur.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá <code>Murmur.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Volvé a escanear los plugins en tu DAW y poné Murmur en una pista de audio, un bus o un envío."],
    win_h="Windows", win_req="Windows 10 u 11, de 64 bits.", win_fmt="VST3 y aplicación independiente",
    win_steps=["Descomprimí el archivo.", "Copiá <code>Murmur.vst3</code> a <code>C:\\\\Program Files\\\\Common Files\\\\VST3</code>.",
               "Volvé a escanear los plugins en tu DAW.", "<code>Murmur.exe</code> funciona solo y procesa tu entrada de audio."],
    file_label="archivo",
    dl_note="Los binarios todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los plugins instalados, y SmartScreen de Windows puede pedirte confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos.",
    os_p="Murmur es software libre bajo la GNU Affero General Public License v3, como el framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    os_link="Por qué Audio Tools es de código abierto",
    trademark="Audio Tools es un proyecto independiente.",
    photo_credit="La bandada de las imágenes es una escena 3D hecha para esta página. Los sonidos que entran a Murmur en esta página son clips de nuestros propios instrumentos.",
    hero_alt="Una bandada de estorninos renderizada en 3D, una nube oscura que fluye sobre un horizonte bajo al atardecer.",
    alt_synth="La página Simple de Murmur con la bandada", alt_adv="La página Advanced de Murmur", alt_frozen="Murmur congelado",
),
}

# ───────────────────────── Lacquer ─────────────────────────
LQ = {
"en": dict(
    title="Lacquer — free automatic mastering: tone, dynamics and loudness for streaming, live or from a file | Audio Tools",
    description="Lacquer is a free, open-source automatic mastering plugin and app for macOS, Windows and Linux. Press Learn or drop a mix: it balances the tone, controls the dynamics, makes the low end mono-safe and hits -14 LUFS for Spotify, -16 for Apple Music or louder targets, with a true-peak limiter. VST3, Audio Unit, standalone and a command-line renderer.",
    listen_p="One demo mix made with our own instruments, unmastered, and the masters Lacquer made from it in five styles. Every clip was brought to the same loudness with a fixed gain, the way streaming services play them, so you compare the sound and not the level.",
    sound_h="A mastering engineer's checklist, run in seconds.",
    sound_p1="Lacquer measures your mix the way streaming services do, and then some: loudness and true peaks, how dynamic it is, the spectrum of the loud parts, the stereo width and whether the lows are mono, harsh resonances, sibilance, clipping and a cut-off in the highs. Then it sets every module only as much as the mix needs.",
    sound_p2="No neural network and no database of other people's songs: published research, our own target curves and rules you can read. The same mix with the same settings always gives exactly the same master.",
    spec=[("Analysis", "BS.1770-5 loudness, true peak, loudness range, spectrum of the loud parts at 1/3 and 1/6 octave, stereo, harshness, sibilance, clipping, band limit"),
          ("Tone", "target curves per style with a tolerance band; a shelf, six bells and a shelf fitted to the correction, never above a detected cut-off"),
          ("Dynamics", "two dynamic EQ bands, three-band compression that leaves the signal untouched when idle, a slow glue compressor"),
          ("Colour and width", "oversampled tape or valve saturation; mono lows below 100 to 150 Hz; side gain guarded by the correlation"),
          ("Loudness", "oversampled soft clipper and a look-ahead limiter with 8x true-peak detection; render mode measures and corrects until it reads the target"),
          ("Two ways", "Learn on the master bus of any DAW, or master a file (WAV, AIFF, FLAC, Ogg, MP3) with A/B and export")],
    flow_h="Learn it, or drop it.",
    flow_p="In your DAW, put Lacquer last on the master bus, press Learn and play the song: it listens, decides and masters in real time. Or open the app, drop a mix on the window, compare and export a WAV with a report of everything it did. The disc on the left is a lacquer, and its groove is your song: the louder the moment, the wider the groove.",
    flow_caption="A mix opened as a file: the groove of the master, style, intensity, loudness target and ceiling, the readouts, the tonal balance and what each module did.",
    chain_h="Every module, explained.",
    chain_p="Lacquer shows what it did and why. Each module has its line, its setting and, for the dynamic ones, the gain reduction as it happens.",
    chain_facts=[("Tone EQ.", "Corrects only what is clearly outside the style's target, with limits on how much and how narrow."),
                 ("Dynamic EQ.", "Tames a harsh resonance or sibilance only in the moments it sticks out."),
                 ("Compression.", "Three gentle bands and a slow glue, set from how much the loudness target asks for. An already mastered mix gets almost nothing."),
                 ("Width.", "Mono lows for clubs, vinyl and phone speakers; the sides adjusted only when the mix is clearly too narrow or too wide."),
                 ("Limiter.", "Detects peaks eight times between samples, so the master stays under the ceiling after MP3 or AAC encoding.")],
    chain_caption="Loud style at the Club target: the clipper and the limiter take over, and the line under the readouts says what streaming will do with it.",
    fair_h="Louder is not better.",
    fair_p="Streaming services turn every song to about the same loudness. Lacquer is built around that fact.",
    fair_facts=[("A/B at the same loudness.", "The original is turned up or down to the master's loudness, so you judge the sound, not the volume."),
                ("Hear it as Spotify.", "Plays both the way a platform would, turned down to -14, -16 or -23 LUFS."),
                ("Intensity is not loudness.", "One knob for how much it changes the sound, another for how loud it ends: a quiet open master and a loud dense one are both one click away."),
                ("It says no.", "If a mix cannot reach a loud target without being wrecked, Lacquer stops at a sensible level and tells you.")],
    fair_caption="The readouts: loudness, true peak and range of the mix and the master, and how much each platform will turn it down.",
    presets_h="Seven loudness targets.",
    presets=[("-14", "Streaming", "Spotify, YouTube, Tidal, Amazon. The default, with a -1 dBTP ceiling."),
             ("-16", "Apple", "Apple Music and a more dynamic sound."),
             ("-11", "Loud", "Louder than the platforms play, with a -2 dBTP ceiling."),
             ("-8", "Club", "DJ sets and club systems."),
             ("-9", "CD", "CD or WAV delivery with a -0.3 dBTP ceiling."),
             ("-23", "Broadcast", "Radio and TV under EBU R128.")],
    dl_h=f"Download Lacquer {VLQ}.",
    dl_p="Free, no account, no upload, no subscription: your music never leaves your computer.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit, standalone app and lacquerrender",
    mac_steps=["Unzip the file.", "Copy <code>Lacquer.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy <code>Lacquer.component</code> to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Rescan plugins in your DAW and put Lacquer last on the master bus, or open <code>Lacquer.app</code> and drop a mix on it."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3, standalone app and lacquerrender",
    win_steps=["Unzip the file.", "Copy <code>Lacquer.vst3</code> to <code>C:\\\\Program Files\\\\Common Files\\\\VST3</code>.",
               "Rescan plugins in your DAW.", "<code>Lacquer.exe</code> masters files on its own: drop a mix on the window."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed files, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps and the renderer's options.",
    os_p="Lacquer is free software under the GNU Affero General Public License v3, like the JUCE framework it is built on. You can use it, study it, change it and share it. The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    os_link="Why Audio Tools is open source",
    trademark="Audio Tools is an independent project. Spotify, YouTube, Apple Music, Tidal and Amazon are trademarks of their owners and are named only to describe their loudness normalization.",
    photo_credit="The lathe in the images is a 3D scene made for this page; its groove follows the real level of the demo master. The demo mix was made with our own instruments.",
    hero_alt="A lacquer disc rendered in 3D on the platter of a cutting lathe, the cutting head with its red lamp over the freshly cut groove.",
    alt_ui="Lacquer with a mix opened as a file", alt_mod="What Lacquer did, with the Loud style at the Club target", alt_read="Lacquer's loudness readouts and the A/B controls",
),
"es": dict(
    title="Lacquer — masterización automática gratis: tono, dinámica y sonoridad para streaming, en vivo o desde un archivo | Audio Tools",
    description="Lacquer es un plugin y una app de masterización automática, libres y de código abierto, para macOS, Windows y Linux. Apretás Learn o soltás una mezcla: equilibra el tono, controla la dinámica, deja los graves en mono y llega a -14 LUFS para Spotify, -16 para Apple Music u objetivos más fuertes, con un limitador de pico verdadero. VST3, Audio Unit, aplicación independiente y render por línea de comandos.",
    listen_p="Una mezcla de demostración hecha con nuestros propios instrumentos, sin masterizar, y los masters que hizo Lacquer en cinco estilos. Todos los clips se llevaron a la misma sonoridad con una ganancia fija, como los reproducen los servicios de streaming, así comparás el sonido y no el volumen.",
    sound_h="La lista de un ingeniero de mastering, en segundos.",
    sound_p1="Lacquer mide tu mezcla como lo hacen los servicios de streaming, y más: sonoridad y picos verdaderos, qué tan dinámica es, el espectro de las partes fuertes, el ancho estéreo y si los graves están en mono, resonancias ásperas, sibilancia, recortes y un corte en los agudos. Después ajusta cada módulo sólo lo que la mezcla necesita.",
    sound_p2="Sin redes neuronales y sin una base de canciones de otros: investigación publicada, curvas objetivo propias y reglas que se pueden leer. La misma mezcla con los mismos ajustes da siempre exactamente el mismo master.",
    spec=[("Análisis", "sonoridad BS.1770-5, pico verdadero, rango de sonoridad, espectro de las partes fuertes en tercios y sextos de octava, estéreo, aspereza, sibilancia, recortes, corte de banda"),
          ("Tono", "curvas objetivo por estilo con una banda de tolerancia; un estante, seis campanas y un estante ajustados a la corrección, nunca por encima de un corte detectado"),
          ("Dinámica", "dos bandas de EQ dinámica, compresión de tres bandas que deja la señal intacta cuando no actúa, un glue lento"),
          ("Color y ancho", "saturación de cinta o válvula sobremuestreada; graves en mono debajo de 100 a 150 Hz; ganancia de los costados cuidada por la correlación"),
          ("Sonoridad", "recortador suave sobremuestreado y un limitador con anticipación y detección de pico verdadero por ocho; en modo archivo mide y corrige hasta llegar al objetivo"),
          ("Dos maneras", "Learn en el bus master de cualquier DAW, o masterizar un archivo (WAV, AIFF, FLAC, Ogg, MP3) con A/B y exportación")],
    flow_h="Que aprenda, o soltalo.",
    flow_p="En tu DAW, poné Lacquer último en el bus master, apretá Learn y reproducí la canción: escucha, decide y masteriza en tiempo real. O abrí la app, soltá una mezcla sobre la ventana, compará y exportá un WAV con un informe de todo lo que hizo. El disco de la izquierda es una laca y su surco es tu canción: cuanto más fuerte el momento, más ancho el surco.",
    flow_caption="Una mezcla abierta como archivo: el surco del master, estilo, intensidad, objetivo de sonoridad y techo, los lectores, el balance tonal y lo que hizo cada módulo.",
    chain_h="Cada módulo, explicado.",
    chain_p="Lacquer muestra qué hizo y por qué. Cada módulo tiene su línea, su ajuste y, en los dinámicos, la reducción de ganancia mientras ocurre.",
    chain_facts=[("EQ tonal.", "Corrige sólo lo que está claramente fuera del objetivo del estilo, con límites de cuánto y de qué tan angosto."),
                 ("EQ dinámica.", "Doma una resonancia áspera o la sibilancia sólo en los momentos en que sobresale."),
                 ("Compresión.", "Tres bandas suaves y un glue lento, ajustados según cuánto pida el objetivo de sonoridad. Una mezcla ya masterizada casi no recibe nada."),
                 ("Ancho.", "Graves en mono para clubes, vinilo y parlantes de teléfono; los costados se ajustan sólo si la mezcla es claramente muy angosta o muy ancha."),
                 ("Limitador.", "Detecta los picos ocho veces entre muestras, así el master queda bajo el techo después de codificarlo en MP3 o AAC.")],
    chain_caption="Estilo Loud con el objetivo Club: el recortador y el limitador toman el mando, y la línea debajo de los lectores dice qué va a hacer el streaming con eso.",
    fair_h="Más fuerte no es mejor.",
    fair_p="Los servicios de streaming llevan todas las canciones a más o menos la misma sonoridad. Lacquer está pensado alrededor de eso.",
    fair_facts=[("A/B a la misma sonoridad.", "El original sube o baja a la sonoridad del master, así juzgás el sonido y no el volumen."),
                ("Escuchalo como Spotify.", "Reproduce los dos como lo haría una plataforma, bajados a -14, -16 o -23 LUFS."),
                ("La intensidad no es la sonoridad.", "Una perilla para cuánto cambia el sonido y otra para qué tan fuerte termina: un master suave y abierto o uno fuerte y denso, a un clic."),
                ("Sabe decir que no.", "Si una mezcla no puede llegar a un objetivo fuerte sin destrozarse, Lacquer se queda en un nivel razonable y te avisa.")],
    fair_caption="Los lectores: sonoridad, pico verdadero y rango de la mezcla y del master, y cuánto lo va a bajar cada plataforma.",
    presets_h="Siete objetivos de sonoridad.",
    presets=[("-14", "Streaming", "Spotify, YouTube, Tidal, Amazon. El de fábrica, con techo de -1 dBTP."),
             ("-16", "Apple", "Apple Music y un sonido más dinámico."),
             ("-11", "Loud", "Más fuerte de lo que reproducen las plataformas, con techo de -2 dBTP."),
             ("-8", "Club", "Sets de DJ y equipos de club."),
             ("-9", "CD", "Entrega en CD o WAV con techo de -0.3 dBTP."),
             ("-23", "Broadcast", "Radio y TV bajo EBU R128.")],
    dl_h=f"Descargá Lacquer {VLQ}.",
    dl_p="Gratis, sin cuenta, sin subir nada, sin suscripción: tu música nunca sale de tu computadora.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit, aplicación independiente y lacquerrender",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>Lacquer.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá <code>Lacquer.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Volvé a escanear los plugins en tu DAW y poné Lacquer último en el bus master, o abrí <code>Lacquer.app</code> y soltá una mezcla."],
    win_h="Windows", win_req="Windows 10 u 11, de 64 bits.", win_fmt="VST3, aplicación independiente y lacquerrender",
    win_steps=["Descomprimí el archivo.", "Copiá <code>Lacquer.vst3</code> a <code>C:\\\\Program Files\\\\Common Files\\\\VST3</code>.",
               "Volvé a escanear los plugins en tu DAW.", "<code>Lacquer.exe</code> masteriza archivos solo: soltá una mezcla sobre la ventana."],
    file_label="archivo",
    dl_note="Los binarios todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los archivos instalados, y SmartScreen de Windows puede pedirte confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos y las opciones del render.",
    os_p="Lacquer es software libre bajo la GNU Affero General Public License v3, como el framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    os_link="Por qué Audio Tools es de código abierto",
    trademark="Audio Tools es un proyecto independiente. Spotify, YouTube, Apple Music, Tidal y Amazon son marcas de sus dueños y se nombran sólo para describir su normalización de sonoridad.",
    photo_credit="El torno de las imágenes es una escena 3D hecha para esta página; su surco sigue el nivel real del master de demostración. La mezcla de demostración se hizo con nuestros propios instrumentos.",
    hero_alt="Un disco de laca renderizado en 3D sobre el plato de un torno de corte, con el cabezal y su luz roja sobre el surco recién cortado.",
    alt_ui="Lacquer con una mezcla abierta como archivo", alt_mod="Qué hizo Lacquer, con el estilo Loud y el objetivo Club", alt_read="Los lectores de sonoridad de Lacquer y los controles de A/B",
),
}

# ───────────────────────── Halation ─────────────────────────
HA = {
"en": dict(
    title="Halation — free delay and reverb in one space: shimmer, freeze, reverse, grains | Audio Tools",
    description="Halation is a free, open-source delay and reverb plugin for macOS, Windows and Linux. One slider goes from tempo-synced echoes to a hall; tape, BBD and spring characters; freeze by bands, shimmer, reverse, grains, spectral smear, a MIDI-tuned resonator, breathing rooms, ducking and a gate. VST3, Audio Unit, standalone and a command-line renderer.",
    listen_p="Clips of our own instruments, each passed through one of Halation's presets with nothing else added. They were brought to the same loudness so you compare the spaces and not the level. Headphones help with the ping-pong.",
    sound_h="Echoes and rooms are the same thing.",
    sound_p1="A delay is a few reflections far apart; a room is thousands of them close together. Halation builds both from the same eight delay lines and moves between them with one control: on the left the lines are separate echoes on the beat, on the right they feed each other into a hall.",
    sound_p2="Length is a time, not a feedback amount, so the tail lasts the same at any point of the slider and the level stays steady. At the top it never ends, and a limiter inside the loop keeps endless and frozen tails in check.",
    spec=[("Space", "eight delay lines with an orthogonal mixing matrix from separate echoes to a dense room; Length from 0.2 s to 60 s, or endless"),
          ("Echoes", "1 ms to 4 s or a note value of the tempo; eight patterns and ping-pong"),
          ("Character", "digital, tape (wow, flutter, saturation, wear), BBD and spring"),
          ("Rooms", "room, hall, plate and chamber, 25 to 400 % in size"),
          ("Modules", "freeze by bands, shimmer, reverse, grains, spectral smear, resonator, stutter, breathe, duck and gate"),
          ("Safety", "limiter inside the loop and a -1 dBFS ceiling: no runaway feedback, even at infinite")],
    flow_h="From an echo to a cathedral.",
    flow_p="Drag Delay to Space and watch the room. On the left every echo is a pulse of light that leaves the source and comes back on the beat. Halfway, the pulses start to split at the walls. On the right they turn into mist: a reverb. The tail keeps its length the whole way.",
    flow_caption="Cathedral of Glass: a bright hall with an octave of shimmer. The room above draws what you hear.",
    room_h="A room that shows what you hear.",
    room_p="The plaster room is drawn from the effect itself, not from a fixed animation.",
    room_facts=[("Size is level.", "A big pulse is a loud echo; the tail shrinks as it fades."),
                ("Colour is brightness.", "Warm pulses carry the lows, cold ones the highs. With a dark tone the tail turns warm as it goes, because the highs die first."),
                ("Mist is density.", "In a room each pulse splits at the walls and leaves mist; the thicker the mist, the denser the reverb."),
                ("The numbers too.", "Echo times on a two-bar grid of your tempo, the tail's fall to -60 dB, and a warning before the feedback self-oscillates.")],
    room_caption="Concert Hall: pulses split at every wall and turn into mist. Bottom right, the tail falling to -60 dB.",
    mods_h="Ten ways out of the room.",
    mods_p="Each module is one button with two or three settings. They are built into the loop, so every repeat goes further.",
    mods_facts=[("Freeze.", "Hold the whole room, or only the lows, mids or highs while the rest keeps moving."),
                ("Shimmer and reverse.", "Each repeat climbs an octave, a fifth or a chord; or the tail plays backwards into the next beat."),
                ("Grains and spectral.", "Scatter the tail into fragments, or smear it into a still, glassy cloud."),
                ("Resonator.", "The tail rings on a chord, or on the notes you play over MIDI."),
                ("Breathe, duck, gate, stutter.", "Walls that move or collapse while you play; a space that steps back when you play; a 1980s gate; slices repeated on the beat.")],
    mods_caption="Freeze: the light stops where it was and the room turns violet.",
    slow_h="Made for slow music.",
    slow_p="Endless tails, freezes that hold a chord for minutes, rooms that breathe at a tenth of a hertz, two scenes to morph between over a whole piece. Halation was designed for ambient work, long live sets, meditation music and sound baths as much as for a vocal plate. It is a tool for making sound: in therapy settings, the professional decides how to use it.",
    presets_h="Thirty presets, four moods.",
    presets=[("8", "Classic", "Tape Echo 1974, Analog Bucket, Ping-Pong Machine, Plate Vocal, Concert Hall, Spring Surf and more."),
             ("8", "Ambient", "Cathedral of Glass, Endless Pad, Breathing Room, Rising Choir, Slow Tide, Tuned Abyss, Spectral Ice."),
             ("6", "Rhythmic", "Dotted Rain, Stutter Grid, Gallop, Ducked Dub, Gated 1984, Swing Taps."),
             ("8", "Experimental", "Ghost Before You, Broken Satellite, Collapsing Hall, Self-Oscillation, Reverse Shimmer, Frozen Lows, Grain Storm.")],
    dl_h=f"Download Halation {VHA}.",
    dl_p="Free, no account, no subscription.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit, standalone app and halationrender",
    mac_steps=["Unzip the file.", "Copy <code>Halation.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy <code>Halation.component</code> to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Rescan plugins in your DAW and put Halation on a track or a send."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3, standalone app and halationrender",
    win_steps=["Unzip the file.", "Copy <code>Halation.vst3</code> to <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Rescan plugins in your DAW and put Halation on a track or a send."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed files, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps and the renderer's options.",
    os_p="Halation is free software under the GNU Affero General Public License v3, like the JUCE framework it is built on. You can use it, study it, change it and share it. The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    os_link="Why Audio Tools is open source",
    trademark="Audio Tools is an independent project. Halation is an original design and does not copy any commercial delay or reverb.",
    photo_credit="The plaster model in the images is a 3D scene made for this page; its points of light follow the same idea the plugin draws. The demo clips were made with our own instruments.",
    hero_alt="A white plaster architectural model of a hall with columns, rendered in 3D, filled with warm and cold points of light.",
    alt_ui="Halation with the Cathedral of Glass preset", alt_room="Halation's room with the Concert Hall preset", alt_frozen="Halation's room frozen, tinted violet",
),
"es": dict(
    title="Halation — delay y reverb gratis en un solo espacio: shimmer, freeze, reversa, granos | Audio Tools",
    description="Halation es un plugin de delay y reverb libre y de código abierto para macOS, Windows y Linux. Una barra va de ecos sincronizados al tempo a una sala; caracteres de cinta, BBD y resorte; congelado por bandas, shimmer, reversa, granos, corrimiento espectral, un resonador afinado por MIDI, salas que respiran, ducking y gate. VST3, Audio Unit, aplicación independiente y render por línea de comandos.",
    listen_p="Clips de nuestros propios instrumentos, cada uno pasado por un preset de Halation sin nada más. Se llevaron a la misma sonoridad así comparás los espacios y no el volumen. Con auriculares se aprecia mejor el ping-pong.",
    sound_h="Ecos y salas son lo mismo.",
    sound_p1="Un delay son unas pocas reflexiones separadas; una sala son miles, muy juntas. Halation arma los dos con las mismas ocho líneas de retardo y pasa de uno a otro con un solo control: a la izquierda las líneas son ecos separados sobre el pulso, a la derecha se alimentan entre sí hasta ser una sala.",
    sound_p2="Length es un tiempo, no una cantidad de realimentación, así que la cola dura lo mismo en cualquier punto de la barra y el nivel se mantiene. Arriba del todo no termina nunca, y un limitador dentro del lazo mantiene a raya las colas infinitas y congeladas.",
    spec=[("Espacio", "ocho líneas de retardo con una matriz de mezcla ortogonal, de ecos separados a una sala densa; Length de 0,2 s a 60 s, o infinito"),
          ("Ecos", "de 1 ms a 4 s o una figura del tempo; ocho patrones y ping-pong"),
          ("Carácter", "digital, cinta (wow, flutter, saturación, desgaste), BBD y resorte"),
          ("Salas", "room, hall, plate y chamber, de 25 a 400 % de tamaño"),
          ("Módulos", "congelado por bandas, shimmer, reversa, granos, corrimiento espectral, resonador, stutter, breathe, duck y gate"),
          ("Seguridad", "limitador dentro del lazo y techo de -1 dBFS: la realimentación no se desboca, ni en infinito")],
    flow_h="De un eco a una catedral.",
    flow_p="Mové Delay a Space y mirá la sala. A la izquierda cada eco es un pulso de luz que sale de la fuente y vuelve sobre el pulso. A mitad de camino, los pulsos empiezan a partirse contra las paredes. A la derecha se vuelven niebla: una reverb. La cola conserva su largo todo el recorrido.",
    flow_caption="Cathedral of Glass: una sala brillante con una octava de shimmer. La sala de arriba dibuja lo que escuchás.",
    room_h="Una sala que muestra lo que escuchás.",
    room_p="La sala de yeso se dibuja a partir del efecto mismo, no es una animación fija.",
    room_facts=[("El tamaño es el nivel.", "Un pulso grande es un eco fuerte; la cola se achica a medida que se apaga."),
                ("El color es el brillo.", "Los pulsos tibios llevan los graves, los fríos los agudos. Con un tono oscuro la cola se entibia, porque los agudos mueren primero."),
                ("La niebla es la densidad.", "En una sala cada pulso se parte en las paredes y deja niebla; cuanto más espesa, más densa la reverb."),
                ("Los números también.", "Los tiempos de los ecos sobre una grilla de dos compases de tu tempo, la caída de la cola hasta -60 dB y un aviso antes de que la realimentación oscile sola.")],
    room_caption="Concert Hall: los pulsos se parten en cada pared y se vuelven niebla. Abajo a la derecha, la cola cayendo hasta -60 dB.",
    mods_h="Diez salidas de la sala.",
    mods_p="Cada módulo es un botón con dos o tres ajustes. Están dentro del lazo, así que cada repetición va más lejos.",
    mods_facts=[("Freeze.", "Retené la sala entera, o sólo los graves, los medios o los agudos mientras el resto sigue moviéndose."),
                ("Shimmer y reversa.", "Cada repetición sube una octava, una quinta o un acorde; o la cola suena al revés hacia el pulso siguiente."),
                ("Granos y espectral.", "Dispersá la cola en fragmentos, o corréla en una nube quieta y vidriosa."),
                ("Resonador.", "La cola suena en un acorde, o en las notas que tocás por MIDI."),
                ("Breathe, duck, gate, stutter.", "Paredes que se mueven o se cierran mientras tocás; un espacio que da un paso atrás cuando tocás; un gate de los ochenta; tramos repetidos sobre el pulso.")],
    mods_caption="Freeze: la luz se queda donde estaba y la sala se tiñe de violeta.",
    slow_h="Hecho para música lenta.",
    slow_p="Colas infinitas, congelados que sostienen un acorde durante minutos, salas que respiran a una décima de hertz, dos escenas para fundir a lo largo de una pieza entera. Halation se pensó tanto para ambient, sets en vivo largos, música para meditar y baños de sonido como para una placa en la voz. Es una herramienta para hacer sonido: en contextos terapéuticos, el o la profesional decide cómo usarla.",
    presets_h="Treinta presets, cuatro climas.",
    presets=[("8", "Classic", "Tape Echo 1974, Analog Bucket, Ping-Pong Machine, Plate Vocal, Concert Hall, Spring Surf y más."),
             ("8", "Ambient", "Cathedral of Glass, Endless Pad, Breathing Room, Rising Choir, Slow Tide, Tuned Abyss, Spectral Ice."),
             ("6", "Rhythmic", "Dotted Rain, Stutter Grid, Gallop, Ducked Dub, Gated 1984, Swing Taps."),
             ("8", "Experimental", "Ghost Before You, Broken Satellite, Collapsing Hall, Self-Oscillation, Reverse Shimmer, Frozen Lows, Grain Storm.")],
    dl_h=f"Descargá Halation {VHA}.",
    dl_p="Gratis, sin cuenta, sin suscripción.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit, aplicación independiente y halationrender",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>Halation.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá <code>Halation.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Volvé a escanear los plugins en tu DAW y poné Halation en una pista o en un envío."],
    win_h="Windows", win_req="Windows 10 u 11, de 64 bits.", win_fmt="VST3, aplicación independiente y halationrender",
    win_steps=["Descomprimí el archivo.", "Copiá <code>Halation.vst3</code> a <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Volvé a escanear los plugins en tu DAW y poné Halation en una pista o en un envío."],
    file_label="archivo",
    dl_note="Los binarios todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los archivos instalados, y SmartScreen de Windows puede pedirte confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos y las opciones del render.",
    os_p="Halation es software libre bajo la GNU Affero General Public License v3, como el framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    os_link="Por qué Audio Tools es de código abierto",
    trademark="Audio Tools es un proyecto independiente. Halation es un diseño original y no copia ningún delay ni reverb comercial.",
    photo_credit="La maqueta de yeso de las imágenes es una escena 3D hecha para esta página; sus puntos de luz siguen la misma idea que dibuja el plugin. Los clips de demostración se hicieron con nuestros propios instrumentos.",
    hero_alt="Una maqueta de arquitectura de yeso blanco de una sala con columnas, renderizada en 3D, llena de puntos de luz tibios y fríos.",
    alt_ui="Halation con el preset Cathedral of Glass", alt_room="La sala de Halation con el preset Concert Hall", alt_frozen="La sala de Halation congelada, teñida de violeta",
),
}

# ───────────────────────── Tarnish ─────────────────────────
TN = {
"en": dict(
    title="Tarnish — free tape, cassette, vinyl, radio and lo-fi plugin with magnetic hysteresis | Audio Tools",
    description="Tarnish is a free, open-source tape and lo-fi plugin for macOS, Windows and Linux: studio tape, cassette, video, vinyl, AM radio and telephone, with Jiles-Atherton magnetic hysteresis, wow and flutter from real transport lines, dropouts, hiss, crackle and scratches, noise reduction mistracking, tape stop, varispeed and bit crushing. VST3, Audio Unit, standalone and a command-line renderer.",
    listen_p="Clips of our own instruments, each passed through one of Tarnish's presets with nothing else added, brought to the same loudness so you compare the machines and not the level.",
    sound_h="Modelled, not sampled.",
    sound_p1="The tape is a magnetic hysteresis model with the record and playback curves of real tape standards, so a hot cassette closes its top end and an under-biased tape grits up on quiet notes. Dropouts are the head lifting off the tape: the highs go first, as on a real one. The wobble is made of the lines a transport really produces, from the reels to the capstan.",
    sound_p2="Every random thing follows the song position. A bounce sounds exactly like what you heard, and bar 17 has the same dropouts whether playback starts at bar 1 or bar 16.",
    spec=[("Machines", "studio tape at four speeds, cassette in four types, video, vinyl at 33, 45 and 78, AM radio and telephone"),
          ("Tape", "Jiles-Atherton hysteresis at 4x with drive, saturation and bias; IEC, NAB and cassette equalisation; head bump"),
          ("Transport", "wow and flutter from the reels, roller, capstan and motor, weighted and measured; tape stop and varispeed"),
          ("Damage", "dropouts as spacing loss, wear, scrape, azimuth, print-through, hiss, hum, crackle and scratches"),
          ("Lo-fi", "noise reduction with mistracking, bit depth and sample rate with clean or raw modes and jitter"),
          ("Repeatable", "deterministic for any block size and locked to the timeline: bounces match playback")],
    flow_h="The machine, from above.",
    flow_p="Tarnish shows the machine it is playing through. The reels turn at the speed of the tape and their packs grow and shrink as it winds; the tape carries its wear; every dropout is a mark that reaches the play head at the moment you hear it. On the right, the service window: the record level, the magnetic loop live, and the measured wow and flutter.",
    flow_caption="Dusty Cassette: the amber marks on the tape are dropouts on their way to the play head.",
    room_h="Six machines, one control set.",
    room_p="Drive, Saturation, Bias, Wow, Flutter, Wear, Noise and Mix work the same on every machine; each machine brings its own physics.",
    room_facts=[("Studio and cassette.", "Four tape speeds and four cassette types. Faster tape is cleaner and brighter; a hot ferric cassette closes its top end."),
                ("Video.", "The linear audio of a home video recorder: narrow, hissy, with the wobble of a servo hunting for speed."),
                ("Vinyl.", "Crackle in three kinds, scratches at a fixed place that pop once per turn, a stylus that moves inward as the song plays."),
                ("Radio and phone.", "An AM set with fading, static and a detector that can't keep up; a landline or a breaking mobile call.")],
    room_caption="Worn Vinyl: the scratches turn with the record; the arm moves inward through the side.",
    mods_h="Damage you can see.",
    mods_p="The service window shows what the model is doing, not a decoration.",
    mods_facts=[("The magnetic loop.", "M against H, live: a thin line on clean tape, bent into the corners as you drive it, opened wide by under-bias."),
                ("Wow and flutter, measured.", "The reading comes from the transport's speed, weighted the way machines are measured."),
                ("Head-to-tape spacing.", "In micrometres, as wear and dropouts lift the tape. One micrometre costs 11 dB at 10 kHz on a cassette."),
                ("Tape stop.", "Pitch and level fall with the speed, as with a real playback head; on the button, any MIDI note or the host's transport.")],
    mods_caption="Under-Biased Grit: the magnetic loop opens and the quiet notes turn grainy.",
    slow_h="From glue to broken.",
    slow_p="On a mix bus, Tarnish is a little saturation and a softer low end. On a beat, a wobbly cassette. On a voice, a radio from another room. Pushed all the way, a tape the deck has eaten.",
    presets_h="Thirty-three presets.",
    presets=[("6", "Studio tape", "Warm Studio Tape, 30 ips Master, 7.5 ips Demo Reel, Hot Drum Bus, Bounced Four Times, Under-Biased Grit."),
             ("9", "Cassette", "Dusty Cassette, Chrome Mixtape, Pocket Player Jog, Fourth-Generation Dub, Answering Machine, Chewed Tape and more."),
             ("9", "Video and vinyl", "Broken VHS, Tracking Error, Worn Vinyl, Clean Pressing, Thrift Store 45, Shellac 78, Warped LP."),
             ("9", "Radio, phone and effects", "Old Radio, Distant Station, Telephone, Mobile Call Breakup, Tape Stop Drop, Half-Speed Dream, 12-bit Sampler.")],
    dl_h=f"Download Tarnish {VTN}.",
    dl_p="Free, no account, no subscription.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit, standalone app and tarnishrender",
    mac_steps=["Unzip the file.", "Copy <code>Tarnish.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy <code>Tarnish.component</code> to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Rescan plugins in your DAW and put Tarnish on a track, a bus or the master."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3, standalone app and tarnishrender",
    win_steps=["Unzip the file.", "Copy <code>Tarnish.vst3</code> to <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Rescan plugins in your DAW and put Tarnish on a track, a bus or the master."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed files, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps and the renderer's options.",
    os_p="Tarnish is free software under the GNU Affero General Public License v3, like the JUCE framework it is built on. You can use it, study it, change it and share it. The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    os_link="Why Audio Tools is open source",
    trademark="Audio Tools is an independent project. Tarnish models generic machine types and does not copy any commercial product; no brand names or logos appear in it.",
    photo_credit="The photographs are synthetic images generated for this page: they show no real product. The demo clips were made with our own instruments.",
    hero_alt="A vintage open-reel tape recorder beside a turntable with a worn vinyl record and a stack of cassettes, on a dark walnut desk in warm light.",
    alt_ui="Tarnish with the Dusty Cassette preset", alt_room="Tarnish's turntable view with the Worn Vinyl preset", alt_frozen="Tarnish's service window with an open magnetic loop",
),
"es": dict(
    title="Tarnish — plugin gratis de cinta, casete, vinilo, radio y lo-fi con histéresis magnética | Audio Tools",
    description="Tarnish es un plugin de cinta y lo-fi libre y de código abierto para macOS, Windows y Linux: cinta de estudio, casete, video, vinilo, radio AM y teléfono, con histéresis magnética de Jiles-Atherton, wow y flutter a partir de las líneas reales del transporte, caídas, soplido, crujido y rayas, reducción de ruido desalineada, tape stop, varispeed y reducción de bits. VST3, Audio Unit, aplicación independiente y render por línea de comandos.",
    listen_p="Clips de nuestros propios instrumentos, cada uno pasado por un preset de Tarnish sin nada más, llevados a la misma sonoridad para comparar las máquinas y no el volumen.",
    sound_h="Modelado, no sampleado.",
    sound_p1="La cinta es un modelo de histéresis magnética con las curvas de grabación y reproducción de las normas reales, así que una casete grabada fuerte cierra sus agudos y una cinta con sub-bias se ensucia en las notas suaves. Las caídas son la cabeza que se separa de la cinta: se van primero los agudos, como en una de verdad. El vaivén está hecho de las líneas que produce un transporte real, de los carretes al cabrestante.",
    sound_p2="Todo lo aleatorio sigue la posición de la canción. Un bounce suena exactamente como lo que escuchaste, y el compás 17 tiene las mismas caídas empiece la reproducción en el 1 o en el 16.",
    spec=[("Máquinas", "cinta de estudio en cuatro velocidades, casete en cuatro tipos, video, vinilo a 33, 45 y 78, radio AM y teléfono"),
          ("Cinta", "histéresis de Jiles-Atherton a ×4 con drive, saturación y bias; ecualización IEC, NAB y de casete; golpe de cabeza"),
          ("Transporte", "wow y flutter de los carretes, el rodillo, el cabrestante y el motor, ponderados y medidos; tape stop y varispeed"),
          ("Daño", "caídas como pérdida por separación, desgaste, scrape, azimut, copia, soplido, zumbido, crujido y rayas"),
          ("Lo-fi", "reducción de ruido con desalineación, bits y frecuencia de muestreo en modo limpio o crudo, con jitter"),
          ("Repetible", "determinista con cualquier tamaño de bloque y atado a la línea de tiempo: los bounces suenan como la reproducción")],
    flow_h="La máquina, desde arriba.",
    flow_p="Tarnish muestra la máquina por la que está pasando el sonido. Los carretes giran a la velocidad de la cinta y sus bobinas crecen y se achican; la cinta lleva su desgaste; cada caída es una marca que llega a la cabeza de reproducción en el momento en que la escuchás. A la derecha, la ventana de servicio: el nivel de grabación, el lazo magnético en vivo y el wow y flutter medidos.",
    flow_caption="Dusty Cassette: las marcas ámbar en la cinta son caídas camino a la cabeza de reproducción.",
    room_h="Seis máquinas, los mismos controles.",
    room_p="Drive, Saturation, Bias, Wow, Flutter, Wear, Noise y Mix funcionan igual en todas las máquinas; cada máquina trae su propia física.",
    room_facts=[("Estudio y casete.", "Cuatro velocidades de cinta y cuatro tipos de casete. La cinta rápida es más limpia y brillante; una casete ferric grabada fuerte cierra sus agudos."),
                ("Video.", "El audio lineal de una videocasetera hogareña: angosto, con soplido, con el vaivén de un servo buscando la velocidad."),
                ("Vinilo.", "Crujido de tres tipos, rayas en un lugar fijo que suenan una vez por vuelta, una púa que avanza hacia adentro con la canción."),
                ("Radio y teléfono.", "Una radio de AM con desvanecimiento, estática y un detector que no llega; una línea fija o una llamada de celular que se corta.")],
    room_caption="Worn Vinyl: las rayas giran con el disco; el brazo avanza hacia adentro a lo largo del lado.",
    mods_h="Un daño que se ve.",
    mods_p="La ventana de servicio muestra lo que está haciendo el modelo, no es decoración.",
    mods_facts=[("El lazo magnético.", "M contra H, en vivo: una línea fina en cinta limpia, doblada hacia las esquinas al subir el drive, abierta con sub-bias."),
                ("Wow y flutter, medidos.", "La lectura sale de la velocidad del transporte, ponderada como se miden las máquinas."),
                ("Separación cabeza-cinta.", "En micrómetros, a medida que el desgaste y las caídas despegan la cinta. Un micrómetro cuesta 11 dB a 10 kHz en una casete."),
                ("Tape stop.", "La afinación y el nivel caen con la velocidad, como con una cabeza de reproducción real; con el botón, cualquier nota MIDI o el transporte del host.")],
    mods_caption="Under-Biased Grit: el lazo magnético se abre y las notas suaves se ensucian.",
    slow_h="Del pegamento a lo roto.",
    slow_p="En un bus de mezcla, Tarnish es un poco de saturación y unos graves más suaves. En un beat, una casete que se ondula. En una voz, una radio desde otra habitación. Llevado al extremo, una cinta que se comió la casetera.",
    presets_h="Treinta y tres presets.",
    presets=[("6", "Cinta de estudio", "Warm Studio Tape, 30 ips Master, 7.5 ips Demo Reel, Hot Drum Bus, Bounced Four Times, Under-Biased Grit."),
             ("9", "Casete", "Dusty Cassette, Chrome Mixtape, Pocket Player Jog, Fourth-Generation Dub, Answering Machine, Chewed Tape y más."),
             ("9", "Video y vinilo", "Broken VHS, Tracking Error, Worn Vinyl, Clean Pressing, Thrift Store 45, Shellac 78, Warped LP."),
             ("9", "Radio, teléfono y efectos", "Old Radio, Distant Station, Telephone, Mobile Call Breakup, Tape Stop Drop, Half-Speed Dream, 12-bit Sampler.")],
    dl_h=f"Descargá Tarnish {VTN}.",
    dl_p="Gratis, sin cuenta, sin suscripción.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit, aplicación independiente y tarnishrender",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>Tarnish.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá <code>Tarnish.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Volvé a escanear los plugins en tu DAW y poné Tarnish en una pista, un bus o el master."],
    win_h="Windows", win_req="Windows 10 u 11, de 64 bits.", win_fmt="VST3, aplicación independiente y tarnishrender",
    win_steps=["Descomprimí el archivo.", "Copiá <code>Tarnish.vst3</code> a <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Volvé a escanear los plugins en tu DAW y poné Tarnish en una pista, un bus o el master."],
    file_label="archivo",
    dl_note="Los binarios todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los archivos instalados, y SmartScreen de Windows puede pedirte confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos y las opciones del render.",
    os_p="Tarnish es software libre bajo la GNU Affero General Public License v3, como el framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    os_link="Por qué Audio Tools es de código abierto",
    trademark="Audio Tools es un proyecto independiente. Tarnish modela tipos de máquina genéricos y no copia ningún producto comercial; no aparecen nombres de marcas ni logos.",
    photo_credit="Las fotografías son imágenes sintéticas generadas para esta página: no muestran ningún producto real. Los clips de demostración se hicieron con nuestros propios instrumentos.",
    hero_alt="Un grabador de carretes antiguo junto a un tocadiscos con un vinilo gastado y una pila de casetes, sobre un escritorio de nogal con luz cálida.",
    alt_ui="Tarnish con el preset Dusty Cassette", alt_room="La vista del tocadiscos de Tarnish con el preset Worn Vinyl", alt_frozen="La ventana de servicio de Tarnish con el lazo magnético abierto",
),
}

PL = {
"en": dict(
    title="Plumb — free real-time pitch correction with 0.67 ms latency | Audio Tools",
    description="Plumb is a free, open-source real-time pitch correction plugin for voice and monophonic instruments, for macOS, Windows and Linux: three latency modes from 0.67 ms, natural to hard-tune correction with one knob, formant preservation and throat, fourteen scales including a custom one, key detection, and MIDI scales and note lock. VST3, Audio Unit, standalone and a command-line renderer.",
    listen_p="A synthetic voice of our own, singing off pitch on purpose: no singer was recorded. Each clip plays the phrase twice, first as sung and then through one of Plumb's presets, with nothing else added and every clip at the same loudness.",
    sound_h="Fast enough to sing through.",
    sound_p1="In Live mode Plumb adds 32 samples, 0.67 ms at 48 kHz: less than the converters of most audio interfaces. A singer hears the corrected voice in their in-ears with no delay to feel. The default mode, Tight, adds 2.7 ms; Studio, for the mix, 21 ms. Plumb reports the exact figure to the host.",
    sound_p2="The voice is moved by repeating or leaving out single cycles at the quietest point of each, while a model of the vocal tract holds the formants in place. Nothing is stretched or smeared, so a voice keeps its edge and its breath.",
    spec=[("Latency", "Live 0.67 ms, Tight 2.7 ms, Studio 21 ms; the same in milliseconds at every sample rate, reported to the host"),
          ("Detection", "a difference-function detector refined over two periods, with octave guards; under 1.3 cents median error from 82 to 880 Hz"),
          ("Correction", "Amount, Speed and Style from natural to hard; transition, humanize, flex, hysteresis, vibrato and drift by hand"),
          ("Formants", "held in place by a 48th-order vocal-tract model, or moved on purpose with Throat"),
          ("Targets", "fourteen scales with a custom one note by note, key detection, MIDI scales and note lock with pitch bend"),
          ("Repeatable", "identical output with any block size and on every run")],
    flow_h="A plumb line for the voice.",
    flow_p="A plumb bob hangs on the left. It swings with the tuning error of what is sung and hangs still when the note is right. On the slate, the last seconds of singing: the sung pitch in chalk, the corrected pitch in brass, and the notes of the scale as lines.",
    flow_caption="Natural Lead in A minor: the brass line settles on the notes and keeps the vibrato of the chalk.",
    room_h="Natural to hard, with one knob.",
    room_p="Style sets the whole character of the correction at once. Turn on Custom to set each part by hand.",
    room_facts=[("Natural.", "Only the centre of each note is moved. Vibrato, scoops and bends stay as sung."),
                ("Hard.", "Every note exactly on its line, the steps as fast as Speed lets them. The vibrato is gone, and a note sung between two others does not flip."),
                ("Humanize and Flex.", "Long notes are corrected more slowly than their starts, and bends wider than the flex window pass untouched."),
                ("Warble, on purpose.", "Hysteresis at zero with Style at hard lets the target follow the vibrato from note to note.")],
    room_caption="Hard Tune: the same phrase, every note on its line.",
    mods_h="Everything else, on one page.",
    mods_p="The front has a key, a scale, a voice range and three knobs. The Advanced page holds the rest.",
    mods_facts=[("Formants that stay.", "With Preserve on, an octave down is a second, deeper voice rather than a slowed-down one. Throat moves them on purpose: darker and bigger, or brighter and younger."),
                ("Pitch and vibrato.", "Transpose and fine tuning, and a synthetic vibrato with rate, depth and delay on every held note."),
                ("MIDI.", "Play the chords and the voice follows their notes, or hold a note and the voice sings it, with pitch bend."),
                ("Listen.", "Sing for half a minute and Plumb proposes the key. It never changes anything by itself.")],
    mods_caption="The Advanced page: character, pitch, formants, MIDI and output.",
    slow_h="Set the key and sing.",
    slow_p="On a lead vocal, Plumb is a gentle hand that leaves the performance alone. On backing vocals, it locks the stack together. On stage, it is in the monitors with no delay to feel. Turned all the way, it is the stepped effect everybody knows.",
    presets_h="Twenty-five presets.",
    presets=[("6", "Natural", "Natural Lead, Gentle Polish, Pop Lead, Ballad, Backing Vocals, Choir Section."),
             ("5", "Hard", "Hard Tune, Hard Tune Live, Soft Robot, Stepped Glide, Warble."),
             ("7", "Live and voice", "Stage Natural, Stage Firm, Tracking, Low Voice, High Voice, Rap Pitch Lock, Instrument."),
             ("7", "Character", "Throat Down, Throat Up, Octave Down, Octave Up, Chipmunk, Added Vibrato, MIDI Note Lock.")],
    dl_h=f"Download Plumb {VPL}.",
    dl_p="Free, no account, no subscription.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit, standalone app and plumbrender",
    mac_steps=["Unzip the file.", "Copy <code>Plumb.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy <code>Plumb.component</code> to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Rescan plugins in your DAW and put Plumb on a vocal track."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3, standalone app and plumbrender",
    win_steps=["Unzip the file.", "Copy <code>Plumb.vst3</code> to <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Rescan plugins in your DAW and put Plumb on a vocal track."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed files, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps and the renderer's options.",
    os_p="Plumb is free software under the GNU Affero General Public License v3, like the JUCE framework it is built on. You can use it, study it, change it and share it. The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    os_link="Why Audio Tools is open source",
    trademark="Audio Tools is an independent project. Plumb is an original design and does not copy any commercial product; no brand names or logos appear in it.",
    photo_credit="The pictures are 3D renders made for this page. The voice in the demo clips is synthesized by our own code: no singer was recorded.",
    hero_alt="A brass plumb bob rendered in 3D, hanging from a rail over a dark slate with two chalk lines of a sung phrase.",
    alt_ui="Plumb with the Natural Lead preset", alt_room="Plumb's note view with the Hard Tune preset", alt_frozen="Plumb's Advanced page",
),
"es": dict(
    title="Plumb — corrección de afinación gratis en tiempo real con 0,67 ms de latencia | Audio Tools",
    description="Plumb es un plugin de corrección de afinación en tiempo real libre y de código abierto, para voz e instrumentos monofónicos, en macOS, Windows y Linux: tres modos de latencia desde 0,67 ms, corrección de natural a afinación dura con una perilla, preservación de formantes y garganta, catorce escalas con una propia, detección de tonalidad, y escalas y nota fija por MIDI. VST3, Audio Unit, aplicación independiente y render por línea de comandos.",
    listen_p="Una voz sintética propia, que canta desafinada a propósito: no se grabó a nadie. Cada clip pasa la frase dos veces, primero como se cantó y después por un preset de Plumb, sin nada más y con todos los clips a la misma sonoridad.",
    sound_h="Tan rápido que se puede cantar a través de él.",
    sound_p1="En modo Live Plumb agrega 32 muestras, 0,67 ms a 48 kHz: menos que los conversores de la mayoría de las interfaces de audio. Quien canta escucha la voz corregida en sus in-ears sin un retardo que se sienta. El modo por defecto, Tight, agrega 2,7 ms; Studio, para la mezcla, 21 ms. Plumb le informa la cifra exacta al host.",
    sound_p2="La voz se mueve repitiendo o salteando ciclos sueltos en el punto más silencioso de cada uno, mientras un modelo del tracto vocal sostiene los formantes en su lugar. Nada se estira ni se borronea, así que la voz conserva su filo y su aire.",
    spec=[("Latencia", "Live 0,67 ms, Tight 2,7 ms, Studio 21 ms; los mismos milisegundos en cualquier frecuencia de muestreo, informados al host"),
          ("Detección", "un detector de función diferencia refinado sobre dos períodos, con guardas de octava; error mediano por debajo de 1,3 cents de 82 a 880 Hz"),
          ("Corrección", "Amount, Speed y Style de natural a dura; transición, humanize, flex, histéresis, vibrato y deriva a mano"),
          ("Formantes", "sostenidos en su lugar por un modelo del tracto vocal de orden 48, o movidos a propósito con Throat"),
          ("Objetivos", "catorce escalas con una propia nota por nota, detección de tonalidad, escalas por MIDI y nota fija con pitch bend"),
          ("Repetible", "la misma salida con cualquier tamaño de bloque y en cada corrida")],
    flow_h="Una plomada para la voz.",
    flow_p="A la izquierda cuelga una plomada. Oscila con el error de afinación de lo que se canta y queda quieta cuando la nota está justa. En la pizarra, los últimos segundos de canto: la altura cantada en tiza, la corregida en bronce, y las notas de la escala como renglones.",
    flow_caption="Natural Lead en La menor: la línea de bronce se asienta sobre las notas y conserva el vibrato de la tiza.",
    room_h="De natural a dura, con una perilla.",
    room_p="Style fija de una vez todo el carácter de la corrección. Encendé Custom para ajustar cada parte a mano.",
    room_facts=[("Natural.", "Sólo se mueve el centro de cada nota. El vibrato, las entradas desde abajo y los bends quedan como se cantaron."),
                ("Dura.", "Cada nota exactamente sobre su renglón, con saltos tan rápidos como lo permita Speed. El vibrato desaparece, y una nota cantada entre otras dos no salta."),
                ("Humanize y Flex.", "Las notas largas se corrigen más despacio que sus arranques, y los bends más anchos que la ventana flex pasan intactos."),
                ("Warble, a propósito.", "Con la histéresis en cero y Style en dura, el objetivo sigue al vibrato de nota en nota.")],
    room_caption="Hard Tune: la misma frase, cada nota sobre su renglón.",
    mods_h="Todo lo demás, en una página.",
    mods_p="Al frente hay una tonalidad, una escala, un rango de voz y tres perillas. La página Advanced tiene el resto.",
    mods_facts=[("Formantes que se quedan.", "Con Preserve encendido, una octava abajo es una segunda voz más grave y no una voz ralentizada. Throat los mueve a propósito: más oscura y más grande, o más brillante y más joven."),
                ("Altura y vibrato.", "Transposición y afinación fina, y un vibrato sintético con velocidad, profundidad y retardo en cada nota sostenida."),
                ("MIDI.", "Tocás los acordes y la voz sigue sus notas, o sostenés una nota y la voz la canta, con pitch bend."),
                ("Listen.", "Cantás medio minuto y Plumb propone la tonalidad. Nunca cambia nada por su cuenta.")],
    mods_caption="La página Advanced: carácter, altura, formantes, MIDI y salida.",
    slow_h="Fijá la tonalidad y cantá.",
    slow_p="En una voz principal, Plumb es una mano suave que deja tranquila la interpretación. En los coros, ajusta todas las capas entre sí. En el escenario, está en los monitores sin un retardo que se sienta. Al máximo, es el efecto escalonado que todos conocen.",
    presets_h="Veinticinco presets.",
    presets=[("6", "Natural", "Natural Lead, Gentle Polish, Pop Lead, Ballad, Backing Vocals, Choir Section."),
             ("5", "Dura", "Hard Tune, Hard Tune Live, Soft Robot, Stepped Glide, Warble."),
             ("7", "En vivo y voz", "Stage Natural, Stage Firm, Tracking, Low Voice, High Voice, Rap Pitch Lock, Instrument."),
             ("7", "Carácter", "Throat Down, Throat Up, Octave Down, Octave Up, Chipmunk, Added Vibrato, MIDI Note Lock.")],
    dl_h=f"Descargá Plumb {VPL}.",
    dl_p="Gratis, sin cuenta, sin suscripción.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit, aplicación independiente y plumbrender",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>Plumb.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá <code>Plumb.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Volvé a escanear los plugins en tu DAW y poné Plumb en una pista de voz."],
    win_h="Windows", win_req="Windows 10 u 11, de 64 bits.", win_fmt="VST3, aplicación independiente y plumbrender",
    win_steps=["Descomprimí el archivo.", "Copiá <code>Plumb.vst3</code> a <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Volvé a escanear los plugins en tu DAW y poné Plumb en una pista de voz."],
    file_label="archivo",
    dl_note="Los binarios todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los archivos instalados, y SmartScreen de Windows puede pedirte confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos y las opciones del render.",
    os_p="Plumb es software libre bajo la GNU Affero General Public License v3, como el framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    os_link="Por qué Audio Tools es de código abierto",
    trademark="Audio Tools es un proyecto independiente. Plumb es un diseño original y no copia ningún producto comercial; no aparecen nombres de marcas ni logos.",
    photo_credit="Las imágenes son renders 3D hechos para esta página. La voz de los clips de demostración la sintetiza nuestro propio código: no se grabó a nadie cantando.",
    hero_alt="Una plomada de bronce renderizada en 3D, colgando de un riel sobre una pizarra oscura con dos líneas de tiza de una frase cantada.",
    alt_ui="Plumb con el preset Natural Lead", alt_room="La vista de notas de Plumb con el preset Hard Tune", alt_frozen="La página Advanced de Plumb",
),
}

A3 = {
"en": dict(
    title="AT303 — free bass line synthesizer with a diode ladder filter and step sequencer | Audio Tools",
    description="AT303 is a free, open-source bass line synthesizer for macOS, Windows and Linux, modelled from the circuit: a diode ladder filter, accent and slide, the original's two-list step sequencer with a visible pattern editor, an acid line generator, the classic circuit modifications, distortion and delay, and a live view of the circuit. VST3, Audio Unit, standalone and a command-line renderer.",
    listen_p="Each clip is one of AT303's presets playing its own pattern, with a knob ridden by hand in some of them. Nothing else added, all brought to the same loudness.",
    sound_h="From the circuit, not from samples.",
    sound_p1="The filter is a diode ladder with its real capacitor values, solved sample by sample: it loses bass as the resonance rises and rings near 500 Hz with the cutoff at half, as the factory calibrated it. The accent is the real sweep circuit, so a run of accents climbs. The slide is the original's RC glide.",
    sound_p2="The sequencer keeps the original's two lists, pitches in order and rhythm step by step, so changing the rhythm moves the melody along. That is where acid lines come from, and now you can see it happen.",
    spec=[("Voice", "saw or square, a four-pole diode ladder, filter and volume envelopes, accent with its sweep circuit, slide"),
          ("Sequencer", "64 patterns of up to 16 steps, triplets, the original's two lists, host sync or its own clock"),
          ("Generator", "acid lines in seven scales with density, accents, slides and ties; Mutate keeps the rhythm"),
          ("Extended", "sixteen classic circuit modifications on one page, with one button back to stock"),
          ("Output", "soft clip, hard clip or fuzz, a tempo-synced ping-pong delay, transpose"),
          ("Inspect", "the circuit drawn live: the ladder's capacitors, the envelopes and the accent capacitor")],
    flow_h="Everything moves.",
    flow_p="On top, the instrument: the knobs, the step lights following the pattern, the tempo on the display and the accent charge bar showing the voltage of the accent capacitor. Below, the software that the original never had: the pattern banks, the editor and the pages.",
    flow_caption="Squelch Line playing: the lit pad is the step you hear, the red ones are accents.",
    room_h="Two lists, finally visible.",
    room_p="On the original, a pattern is a list of pitches and a separate list of rhythm. Each note step takes the next pitch. AT303 shows both, and a line from each step to the pitch it plays.",
    room_facts=[("Change the rhythm.", "Turn a rest into a note and every note after it moves one pitch along: the same melody, a new line."),
                ("Edit on screen.", "Click a step to cycle note, tie and rest; drag a pitch; switch octave, accent and slide."),
                ("Or the original way.", "Pitch and Time modes on the instrument, with its keys and buttons."),
                ("Generate.", "An acid line in the key and scale you choose; Mutate keeps the rhythm and redraws the notes.")],
    room_caption="The pattern editor: the rhythm on top, the pitches below, and which pitch each step plays.",
    mods_h="The circuit, live.",
    mods_p="Inspect fades the instrument into its circuit while it plays. It is the same model you hear, drawn.",
    mods_facts=[("The diode ladder.", "Four capacitors charging and discharging, with the resonance and the frequency where the filter rings."),
                ("The accent capacitor.", "C13 fills with every accent and drains slowly through the Resonance pot: watch the fourth accent climb."),
                ("Two envelopes.", "The filter envelope and the volume envelope as they move, note by note."),
                ("Sixteen modifications.", "Overdrive, longer accents, filter tracking, filter FM, a higher cutoff, self-oscillation, full bass: one page, one button back to stock.")],
    mods_caption="Accent Climb in Inspect: the accent capacitor C13 charging across four accents.",
    slow_h="From a pluck to a scream.",
    slow_p="Under a track, AT303 is a round, deep bass. Turn the resonance up and it starts to talk. Ride the cutoff and the envelope, stack the accents, and it screams.",
    presets_h="Thirty-four presets, each with its pattern.",
    presets=[("8", "Classic", "First Squelch, Rubber Root, Sliding Fifths, Square Pulse, Accent Climb, Low Decay Chug, Long Sweep, Minor Walk."),
             ("8", "Acid", "Squelch Line, Night Bus, Wet Concrete, Glass Eel, Twelve O'Clock, Pressure Drop, Bubble Square, Off The Grid."),
             ("6", "Bass", "Deep Root, Dub Pulse, Sub Ladder, Funk Thumb, Tied Drone, Pocket."),
             ("12", "Driven, triplets and extended", "Hot Box, Fuzz Worm, Overdriven Ladder, Triplet Roll, Six Against Four, Screamer, Tracking Lead and more.")],
    dl_h=f"Download AT303 {VA3}.",
    dl_p="Free, no account, no subscription.",
    mac_h="macOS", mac_req="macOS 11 or later. Apple silicon and Intel in one universal binary.",
    mac_fmt="VST3, Audio Unit, standalone app and at303render",
    mac_steps=["Unzip the file.", "Copy <code>AT303.vst3</code> to <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copy <code>AT303.component</code> to <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Rescan plugins in your DAW, put AT303 on an instrument track and press play."],
    win_h="Windows", win_req="Windows 10 or 11, 64-bit.", win_fmt="VST3, standalone app and at303render",
    win_steps=["Unzip the file.", "Copy <code>AT303.vst3</code> to <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Rescan plugins in your DAW, put AT303 on an instrument track and press play."],
    file_label="file",
    dl_note="The builds are not code-signed yet. macOS may refuse to open them until you run <code>xattr -dr com.apple.quarantine</code> on the installed files, and Windows SmartScreen may ask you to confirm the first time. Both packages include an INSTALL.txt with the exact steps and the renderer's options.",
    os_p="AT303 is free software under the GNU Affero General Public License v3, like the JUCE framework it is built on. You can use it, study it, change it and share it. The source code is on <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, with every release.",
    os_link="Why Audio Tools is open source",
    trademark="Audio Tools is an independent project. AT303 is modelled on the published circuit of a classic 1981 bass line synthesizer; it copies no product's look, and no brand names or logos appear in it.",
    photo_credit="The instrument in the images is an original 3D design made for this page. The demo clips are AT303 itself.",
    hero_alt="A graphite bass line synthesizer rendered in 3D on a dark table, its step pads lit amber, five large knobs, a display and a one-octave keyboard.",
    alt_ui="AT303 playing the Squelch Line preset", alt_room="AT303's pattern editor with the time and pitch lists", alt_frozen="AT303's Inspect view with the circuit drawn live",
),
"es": dict(
    title="AT303 — sintetizador de bajo gratis con filtro de escalera de diodos y secuenciador | Audio Tools",
    description="AT303 es un sintetizador de bajo libre y de código abierto para macOS, Windows y Linux, modelado a partir del circuito: filtro de escalera de diodos, acento y slide, el secuenciador de dos listas del original con un editor de patrones visible, un generador de líneas ácidas, las modificaciones clásicas del circuito, distorsión y delay, y una vista del circuito en vivo. VST3, Audio Unit, aplicación independiente y render por línea de comandos.",
    listen_p="Cada clip es un preset del AT303 tocando su propio patrón, con una perilla movida a mano en algunos. Sin nada más, todos llevados a la misma sonoridad.",
    sound_h="Desde el circuito, no desde samples.",
    sound_p1="El filtro es una escalera de diodos con los valores reales de sus capacitores, resuelta muestra a muestra: pierde graves a medida que sube la resonancia y suena cerca de 500 Hz con el corte al medio, como lo calibraba la fábrica. El acento es el circuito de barrido real, así que una seguidilla de acentos va subiendo. El slide es el deslizamiento RC del original.",
    sound_p2="El secuenciador conserva las dos listas del original, las alturas en orden y el ritmo paso por paso, así que cambiar el ritmo corre la melodía. De ahí salen las líneas de acid, y ahora se ve cómo pasa.",
    spec=[("Voz", "sierra o cuadrada, escalera de diodos de cuatro polos, envolventes de filtro y volumen, acento con su circuito de barrido, slide"),
          ("Secuenciador", "64 patrones de hasta 16 pasos, tresillos, las dos listas del original, sincronía con el host o reloj propio"),
          ("Generador", "líneas ácidas en siete escalas con densidad, acentos, slides y ligaduras; Mutate conserva el ritmo"),
          ("Extendido", "dieciséis modificaciones clásicas del circuito en una página, con un botón para volver al original"),
          ("Salida", "recorte suave, recorte duro o fuzz, un delay ping-pong sincronizado, transposición"),
          ("Inspect", "el circuito dibujado en vivo: los capacitores de la escalera, las envolventes y el capacitor del acento")],
    flow_h="Todo se mueve.",
    flow_p="Arriba, el instrumento: las perillas, las luces de los pasos siguiendo el patrón, el tempo en el display y la barra de carga del acento mostrando la tensión de su capacitor. Abajo, el software que el original nunca tuvo: los bancos de patrones, el editor y las páginas.",
    flow_caption="Squelch Line sonando: el pad encendido es el paso que escuchás, los rojos son acentos.",
    room_h="Dos listas, por fin a la vista.",
    room_p="En el original, un patrón es una lista de alturas y otra lista aparte de ritmo. Cada paso de nota toma la altura siguiente. AT303 muestra las dos, y una línea de cada paso a la altura que toca.",
    room_facts=[("Cambiá el ritmo.", "Convertí un silencio en nota y todas las notas que siguen se corren una altura: la misma melodía, otra línea."),
                ("Editá en pantalla.", "Un clic en un paso alterna nota, ligadura y silencio; arrastrá una altura; cambiá octava, acento y slide."),
                ("O a la manera del original.", "Los modos Pitch y Time en el instrumento, con sus teclas y botones."),
                ("Generá.", "Una línea ácida en la tonalidad y la escala que elijas; Mutate conserva el ritmo y vuelve a dibujar las notas.")],
    room_caption="El editor de patrones: arriba el ritmo, abajo las alturas, y qué altura toca cada paso.",
    mods_h="El circuito, en vivo.",
    mods_p="Inspect funde el instrumento en su circuito mientras suena. Es el mismo modelo que escuchás, dibujado.",
    mods_facts=[("La escalera de diodos.", "Cuatro capacitores cargándose y descargándose, con la resonancia y la frecuencia en la que suena el filtro."),
                ("El capacitor del acento.", "C13 se llena con cada acento y se descarga despacio por el potenciómetro de Resonance: mirá subir el cuarto acento."),
                ("Dos envolventes.", "La del filtro y la del volumen mientras se mueven, nota por nota."),
                ("Dieciséis modificaciones.", "Overdrive, acentos más largos, seguimiento del filtro, FM del filtro, un corte más alto, autooscilación, graves completos: una página, un botón para volver al original.")],
    mods_caption="Accent Climb en Inspect: el capacitor del acento, C13, cargándose a lo largo de cuatro acentos.",
    slow_h="De un punteo a un grito.",
    slow_p="Debajo de un tema, AT303 es un bajo redondo y profundo. Subí la resonancia y empieza a hablar. Mové el corte y la envolvente, juntá los acentos, y grita.",
    presets_h="Treinta y cuatro presets, cada uno con su patrón.",
    presets=[("8", "Clásicos", "First Squelch, Rubber Root, Sliding Fifths, Square Pulse, Accent Climb, Low Decay Chug, Long Sweep, Minor Walk."),
             ("8", "Acid", "Squelch Line, Night Bus, Wet Concrete, Glass Eel, Twelve O'Clock, Pressure Drop, Bubble Square, Off The Grid."),
             ("6", "Bajos", "Deep Root, Dub Pulse, Sub Ladder, Funk Thumb, Tied Drone, Pocket."),
             ("12", "Distorsión, tresillos y extendido", "Hot Box, Fuzz Worm, Overdriven Ladder, Triplet Roll, Six Against Four, Screamer, Tracking Lead y más.")],
    dl_h=f"Descargá AT303 {VA3}.",
    dl_p="Gratis, sin cuenta, sin suscripción.",
    mac_h="macOS", mac_req="macOS 11 o posterior. Apple silicon e Intel en un solo binario universal.",
    mac_fmt="VST3, Audio Unit, aplicación independiente y at303render",
    mac_steps=["Descomprimí el archivo.", "Copiá <code>AT303.vst3</code> a <code>~/Library/Audio/Plug-Ins/VST3</code>.",
               "Copiá <code>AT303.component</code> a <code>~/Library/Audio/Plug-Ins/Components</code>.",
               "Volvé a escanear los plugins en tu DAW, poné AT303 en una pista de instrumento y dale play."],
    win_h="Windows", win_req="Windows 10 u 11, de 64 bits.", win_fmt="VST3, aplicación independiente y at303render",
    win_steps=["Descomprimí el archivo.", "Copiá <code>AT303.vst3</code> a <code>C:\\Program Files\\Common Files\\VST3</code>.",
               "Volvé a escanear los plugins en tu DAW, poné AT303 en una pista de instrumento y dale play."],
    file_label="archivo",
    dl_note="Los binarios todavía no están firmados. macOS puede negarse a abrirlos hasta que corras <code>xattr -dr com.apple.quarantine</code> sobre los archivos instalados, y SmartScreen de Windows puede pedirte confirmación la primera vez. Los dos paquetes traen un INSTALL.txt con los pasos exactos y las opciones del render.",
    os_p="AT303 es software libre bajo la GNU Affero General Public License v3, como el framework JUCE sobre el que está hecho. Lo podés usar, estudiar, modificar y compartir. El código fuente está en <a href='https://github.com/palmereta/audio_tools'>GitHub</a>, con cada versión.",
    os_link="Por qué Audio Tools es de código abierto",
    trademark="Audio Tools es un proyecto independiente. AT303 está modelado a partir del circuito publicado de un sintetizador de bajo clásico de 1981; no copia la apariencia de ningún producto y no aparecen nombres de marcas ni logos.",
    photo_credit="El instrumento de las imágenes es un diseño 3D original hecho para esta página. Los clips de demostración son el propio AT303.",
    hero_alt="Un sintetizador de bajo de grafito renderizado en 3D sobre una mesa oscura, con los pads de los pasos encendidos en ámbar, cinco perillas grandes, un display y un teclado de una octava.",
    alt_ui="AT303 tocando el preset Squelch Line", alt_room="El editor de patrones del AT303 con las listas de tiempos y alturas", alt_frozen="La vista Inspect del AT303 con el circuito dibujado en vivo",
),
}

# ───────────────────────── presentación (estilo lanzamiento) ─────────────────────────
# Mediciones de CPU (porcentaje de un núcleo a 48 kHz, sólo el motor, sobremuestreo por defecto
# salvo donde se indica). Apple M4 con macOS 26; AMD Ryzen 9 5900XT con Windows 11 (MSVC, /fp:fast).
WINCPU_MD = (1.54, 8.32)             # Ryzen 9 5900XT: atmdrender.exe Lucky Lead (4x) / atmd_dsptest prueba 12 (8 voces, 4x)
WINCPU_MM = (0.88, 1.50, 6.22)        # Ryzen 9 5900XT: murmurrender.exe First Murmur / Granular Wash, mm_dsptest prueba 9 (128 granos)
WINCPU_OR = (0.009, 3.8)            # Ryzen 9 5900XT, orreryrender.exe --bench / Orbiting con el sonido de prueba
WINCPU_FS1 = (0.7, 13.9, 21.9)      # Ryzen 9 5900XT, atfs1render.exe con efectos (promedio de tres corridas)
UICPU_FS1 = 4
DISK_OR = (44, 14)
DISK_MM = (51, 16)
DISK_MD = (81, 27)
UICPU_MM = 6
CPU_LQ = (2.84, 2.91)                # Apple M4: lacquerrender --bench sobre la mezcla demo (cadena aprendida / todos los módulos)
WINCPU_LQ = (4.84, 4.89)             # Ryzen 9 5900XT: lacquerrender.exe --bench (pc50 sin otra carga)
DISK_LQ = (55, 18)
UICPU_LQ = 6
CPU_HA = (0.47, 1.56, 2.65)          # Apple M4: halationrender sobre los clips demo (Cathedral of Glass / Spectral Ice) y halationtest con todo prendido
WINCPU_HA = (1.33, 2.25, 4.96)         # Ryzen 9 5900XT: halationrender.exe (pc50 sin otra carga)
DISK_HA = (75, 22)
CPU_TN = (5.29, 5.64, 0.47)           # Apple M4: tarnishrender sobre el piano demo (estudio por defecto, Dusty Cassette, Worn Vinyl)
WINCPU_TN = (5.46, 6.05, 1.11)        # Ryzen 9 5900XT: lo mismo con tarnishrender.exe (pc50 sin otra carga)
DISK_TN = (77, 23)
UICPU_TN = 6
CPU_A3 = (0.86, 1.17, 1.65)          # Apple M4: at303test T8 (sonido por defecto ×4 / cuadrada, distorsión, delay y FM / ×8), secuenciador corriendo
WINCPU_A3 = (1.79, 3.38, 2.99)       # Ryzen 9 5900XT: at303test.exe T8 (pc50 sin otra carga)
DISK_A3 = (65, 19)
UICPU_A3 = 5
CPU_PL = (3.16, 3.16, 7.95)            # Apple M4: plumbrender sobre la voz demo (Natural Lead / Hard Tune / Throat Down), modo Tight
WINCPU_PL = (3.53, 3.54, 9.61)        # Ryzen 9 5900XT: lo mismo con plumbrender.exe (pc50 sin otra carga)
DISK_PL = (81, 23)
UICPU_PL = 6
UICPU_HA = 8
DISK_FS1 = (80, 24)
CPU = {
    "at20v": [("mono", 1.6, 3.6), ("poly8", 11.2, 32.4), ("poly12", 28.9, 78.1)],
    "at101v": [("mono", 1.0, 1.8), ("poly6", 7.8, 14.5), ("poly6x8", 14.5, 27.6)],
    "at42": [("drones", 2.6, 7.9), ("all2", 2.6, 7.5), ("all4", 4.8, 14.5)],
    "at700": [("duo", 1.7, 3.5), ("poly6s", 6.1, 13.6), ("poly8x4", 16.1, 34.9)],
    "atfs1": [("speech", 0.4, WINCPU_FS1[0]), ("choir16", 6.8, WINCPU_FS1[1]), ("pad32", 8.7, WINCPU_FS1[2])],
    "orrery": [("orgen", 0.003, WINCPU_OR[0]), ("orprev", 1.6, WINCPU_OR[1])],
    "atmd": [("mdmono", 0.85, WINCPU_MD[0]), ("mdpoly8", 4.79, WINCPU_MD[1])],
    "murmur": [("mmfirst", 0.33, WINCPU_MM[0]), ("mmwash", 0.56, WINCPU_MM[1]), ("mm128", 2.55, WINCPU_MM[2])],
    "lacquer": [("lqlearned", CPU_LQ[0], WINCPU_LQ[0]), ("lqall", CPU_LQ[1], WINCPU_LQ[1])],
    "halation": [("hacath", CPU_HA[0], WINCPU_HA[0]), ("haspec", CPU_HA[1], WINCPU_HA[1]), ("haall", CPU_HA[2], WINCPU_HA[2])],
    "tarnish": [("tnstudio", CPU_TN[0], WINCPU_TN[0]), ("tndusty", CPU_TN[1], WINCPU_TN[1]), ("tnvinyl", CPU_TN[2], WINCPU_TN[2])],
    "at303": [("a3def", CPU_A3[0], WINCPU_A3[0]), ("a3all", CPU_A3[1], WINCPU_A3[1]), ("a3x8", CPU_A3[2], WINCPU_A3[2])],
    "plumb": [("plnat", CPU_PL[0], WINCPU_PL[0]), ("plhard", CPU_PL[1], WINCPU_PL[1]), ("plthroat", CPU_PL[2], WINCPU_PL[2])],
}
CPU_LABEL = {
    "en": {"mono": "Mono patch, 4x", "poly8": "8 voices with delay and reverb, 4x", "poly12": "12 voices, 8x (the heaviest setting)",
           "poly6": "6-voice pad, 4x", "poly6x8": "6-voice pad, 8x", "drones": "Two drone voices with effects, 2x",
           "all2": "Every voice and the effector, 2x", "all4": "Every voice and the effector, 4x",
           "duo": "Duo, one voice of the console, 2x", "poly6s": "6-voice strings with phaser and reverb, 2x", "poly8x4": "8 voices, every voice module, 4x",
           "speech": "One voice speaking a Formant Sequence", "choir16": "16-voice vowel choir with chorus and reverb", "pad32": "32 voices of a morphing vowel pad, 16 operators each",
           "orgen": "The generator: 8 busy lanes with loops, echoes and controller curves", "orprev": "With the built-in preview sound and its reverb",
           "mmfirst": "First Murmur, the default preset", "mdmono": "One voice, three oscillators, 4x", "mdpoly8": "Eight voices, three oscillators each, 4x", "mmwash": "Granular Wash, the heaviest factory preset", "mm128": "128 grains of 1 s at once, with delay and reverb",
           "lqlearned": "The chain learned from the demo mix (Balanced, -14 LUFS)", "lqall": "Every module on, oversampled saturation and clipper included",
           "hacath": "Cathedral of Glass, a hall with shimmer", "haspec": "Spectral Ice, the heaviest factory preset", "haall": "Every module on at once, Length at infinite",
           "tnstudio": "Studio tape, the default (magnetic model at 4x)", "tndusty": "Dusty Cassette", "tnvinyl": "Worn Vinyl (no magnetic model)",
           "a3def": "The default sound at 4x, sequencer running", "a3all": "Square wave, distortion, delay and filter FM, 4x", "a3x8": "The default sound at 8x",
           "plnat": "Natural Lead, Tight mode, formants preserved", "plhard": "Hard Tune, Tight mode", "plthroat": "Throat Down: formants moved"},
    "es": {"mono": "Patch mono, 4x", "poly8": "8 voces con delay y reverb, 4x", "poly12": "12 voces, 8x (el ajuste más pesado)",
           "poly6": "Pad de 6 voces, 4x", "poly6x8": "Pad de 6 voces, 8x", "drones": "Dos voces drone con efectos, 2x",
           "all2": "Todas las voces y el efector, 2x", "all4": "Todas las voces y el efector, 4x",
           "duo": "Duo, una voz de la consola, 2x", "poly6s": "Cuerdas de 6 voces con phaser y reverb, 2x", "poly8x4": "8 voces, todos los módulos de voz, 4x",
           "speech": "Una voz diciendo un Formant Sequence", "choir16": "Coro de vocales de 16 voces con chorus y reverb", "pad32": "32 voces de un pad de vocales, 16 operadores cada una",
           "orgen": "El generador: 8 carriles cargados con lazos, ecos y curvas de controlador", "orprev": "Con el sonido de prueba incorporado y su reverb",
           "mmfirst": "First Murmur, el preset por defecto", "mdmono": "Una voz, tres osciladores, 4x", "mdpoly8": "Ocho voces de tres osciladores, 4x", "mmwash": "Granular Wash, el preset de fábrica más pesado", "mm128": "128 granos de 1 s a la vez, con delay y reverb",
           "lqlearned": "La cadena aprendida de la mezcla demo (Balanced, -14 LUFS)", "lqall": "Todos los módulos activos, con la saturación y el recortador sobremuestreados",
           "hacath": "Cathedral of Glass, una sala con shimmer", "haspec": "Spectral Ice, el preset de fábrica más pesado", "haall": "Todos los módulos prendidos a la vez, Length en infinito",
           "tnstudio": "Cinta de estudio, por defecto (modelo magnético a ×4)", "tndusty": "Dusty Cassette", "tnvinyl": "Worn Vinyl (sin modelo magnético)",
           "a3def": "El sonido de fábrica a ×4, con el secuenciador corriendo", "a3all": "Cuadrada, distorsión, delay y FM del filtro, ×4", "a3x8": "El sonido de fábrica a ×8",
           "plnat": "Natural Lead, modo Tight, formantes preservados", "plhard": "Hard Tune, modo Tight", "plthroat": "Throat Down: formantes movidos"},
}
UI_CPU = {"at20v": 15, "at101v": 5, "at42": 11, "at700": 8, "atfs1": UICPU_FS1, "orrery": 4, "murmur": UICPU_MM, "atmd": 6, "lacquer": UICPU_LQ, "halation": UICPU_HA, "tarnish": UICPU_TN, "at303": UICPU_A3, "plumb": UICPU_PL}          # lo que suma el editor abierto en el M4 (% de un núcleo)
DISK = {"at20v": (219, 73), "at101v": (71, 24), "at42": (125, 42), "at700": (78, 26), "atfs1": DISK_FS1, "orrery": DISK_OR, "murmur": DISK_MM, "atmd": DISK_MD, "lacquer": DISK_LQ, "halation": DISK_HA, "tarnish": DISK_TN, "at303": DISK_A3, "plumb": DISK_PL}

REQ = {
"en": dict(
    min_h="Minimum", rec_h="Recommended", cpu_h="Measured CPU load",
    mac_min="Mac with Apple silicon (M1 or later), or Intel Core i5 quad-core from 2017 or later, with macOS 11 Big Sur or later.",
    win_min="PC with Intel Core i5-6500 (6th generation, 2015) or AMD Ryzen 5 1600, or better, with Windows 10 or 11, 64-bit.",
    common_min="4 GB of RAM, a 1280 × 800 screen and a 64-bit host for VST3 or Audio Unit, or the standalone app.",
    mac_rec="Any Mac with Apple silicon.",
    win_rec="Intel Core i5-12400 or AMD Ryzen 5 5600, or better.",
    common_rec="8 GB of RAM and a 1920 × 1080 screen.",
    cpu_note="Percentage of one CPU core at 48 kHz, sound engine only. With the editor open, the interface adds about {ui} % on the M4. Hosts run each instrument on one core, so single-core speed is what counts.",
    cpu_cols=("Setting", "Apple M4", "AMD Ryzen 9 5900XT"),
    disk="Disk space: {mac} MB on macOS (all formats), {win} MB on Windows.",
    heavy={"at303": "", "at20v": "The 12-voice, 8x setting needs the recommended processor.", "at101v": "", "at42": "", "at700": "Eight voices at 4x need the recommended processor.", "atfs1": "Thirty-two voices with every operator need the recommended processor.", "orrery": "", "murmur": "", "atmd": "", "lacquer": "", "halation": "", "tarnish": "", "plumb": ""},
),
"es": dict(
    min_h="Mínimo", rec_h="Recomendado", cpu_h="Carga de CPU medida",
    mac_min="Mac con Apple silicon (M1 o posterior), o Intel Core i5 de cuatro núcleos de 2017 en adelante, con macOS 11 Big Sur o posterior.",
    win_min="PC con Intel Core i5-6500 (6.ª generación, 2015) o AMD Ryzen 5 1600, o superior, con Windows 10 u 11 de 64 bits.",
    common_min="4 GB de RAM, una pantalla de 1280 × 800 y un host de 64 bits para VST3 o Audio Unit, o la aplicación independiente.",
    mac_rec="Cualquier Mac con Apple silicon.",
    win_rec="Intel Core i5-12400 o AMD Ryzen 5 5600, o superior.",
    common_rec="8 GB de RAM y una pantalla de 1920 × 1080.",
    cpu_note="Porcentaje de un núcleo de CPU a 48 kHz, sólo el motor de sonido. Con el editor abierto, la interfaz suma alrededor de {ui} % en el M4. Los DAW corren cada instrumento en un núcleo, así que lo que cuenta es la velocidad de un núcleo.",
    cpu_cols=("Ajuste", "Apple M4", "AMD Ryzen 9 5900XT"),
    disk="Espacio en disco: {mac} MB en macOS (todos los formatos), {win} MB en Windows.",
    heavy={"at303": "", "at20v": "El ajuste de 12 voces a 8x necesita el procesador recomendado.", "at101v": "", "at42": "", "at700": "Ocho voces a 4x necesitan el procesador recomendado.", "atfs1": "Treinta y dos voces con todos los operadores necesitan el procesador recomendado.", "orrery": "", "murmur": "", "atmd": "", "lacquer": "", "halation": "", "tarnish": "", "plumb": ""},
),
}

# Descarga para Linux: aparece en cada página (y en el launcher) sólo cuando su zip <Producto>-<versión>-Linux.zip está
# en web/downloads. Un solo texto para todos los productos; el formato sale del de Windows con LV2 agregado, salvo que
# el producto traiga su propio "linux_fmt".
LINUX = {
"en": dict(h="Linux", dl="Download for Linux",
           req="Linux x86_64 with glibc 2.35 or later (Ubuntu 22.04, Debian 12, Fedora 36 or newer).",
           steps=["Unzip the file.",
                  "Copy the <code>.vst3</code> folders to <code>~/.vst3</code> and the <code>.lv2</code> folders to <code>~/.lv2</code>.",
                  "Rescan plugins in your DAW. The standalone app is the file with the plugin's name: run it from the folder."],
           la_req="Linux x86_64 with glibc 2.35 or later.",
           la_steps=["Unzip the file and run <code>Audio Tools Launcher</code>.",
                     "Pick the plugins and press Install. VST3 goes to <code>~/.vst3</code>, LV2 to <code>~/.lv2</code> and the standalone apps to your applications menu."]),
"es": dict(h="Linux", dl="Descargar para Linux",
           req="Linux x86_64 con glibc 2.35 o posterior (Ubuntu 22.04, Debian 12, Fedora 36 o más nuevo).",
           steps=["Descomprimí el archivo.",
                  "Copiá las carpetas <code>.vst3</code> a <code>~/.vst3</code> y las <code>.lv2</code> a <code>~/.lv2</code>.",
                  "Reescaneá los plugins en tu DAW. La app independiente es el archivo con el nombre del plugin: se abre desde la carpeta."],
           la_req="Linux x86_64 con glibc 2.35 o posterior.",
           la_steps=["Descomprimí el archivo y abrí <code>Audio Tools Launcher</code>.",
                     "Elegí los plugins y tocá Instalar. El VST3 va a <code>~/.vst3</code>, el LV2 a <code>~/.lv2</code> y las apps independientes al menú de aplicaciones."]),
}

def linux_zip(mac_zip):
    """El zip de Linux que acompaña a uno de macOS, o None si todavía no está en web/downloads."""
    z = mac_zip.replace("-macOS.zip", "-Linux.zip")
    return z if os.path.exists(os.path.join(WEB, "downloads", z)) else None

UI = {
"en": dict(overview="Overview", film="Film", colours="Colours", specs="Tech specs", download="Download", watch="Watch the film",
           highlights="Get the highlights.", film_h="The film.", colours_h="Pick a colour.", specs_h="Tech specs.",
           prev="Previous", next="Next", close="Close", learn="Learn more", compare="Compare the instruments", why="Why open source",
           compare_h="Which one is yours?", dl_mac="Download for macOS", dl_win="Download for Windows", install="How to install",
           free="Free", prices="Free and open source. For macOS, Windows and Linux.", go_to="Go to slide",
           listen="Listen", listen_h="Hear it.", play="Play", pause="Pause", seek="Position in", midi_dl="Download the MIDI file of",
           manual="Manual", read_manual="Read the manual",
           listen_p="Every clip is the engine on its own, playing a factory preset from a MIDI file written for it. Nothing added afterwards, only the level matched across clips. Download the MIDI and play it through the preset yourself."),
"es": dict(overview="Resumen", film="Film", colours="Colores", specs="Especificaciones", download="Descargar", watch="Ver el film",
           highlights="Lo más destacado.", film_h="El film.", colours_h="Elegí un color.", specs_h="Especificaciones técnicas.",
           prev="Anterior", next="Siguiente", close="Cerrar", learn="Ver más", compare="Comparar los instrumentos", why="Por qué código abierto",
           compare_h="¿Cuál es el tuyo?", dl_mac="Descargar para macOS", dl_win="Descargar para Windows", install="Cómo instalar",
           free="Gratis", prices="Libre y gratis. Para macOS, Windows y Linux.", go_to="Ir a la tarjeta",
           listen="Escuchar", listen_h="Escuchalo.", play="Reproducir", pause="Pausa", seek="Posición en", midi_dl="Descargar el archivo MIDI de",
           manual="Manual", read_manual="Leer el manual",
           listen_p="Cada clip es el motor solo, tocando un preset de fábrica desde un MIDI escrito para él. No se le agregó nada después; sólo se igualó el nivel entre clips. Bajá el MIDI y tocalo vos con el mismo preset."),
}

PRES = {
"at20v": {
    "en": dict(tag="The semi-modular that screams.",
        highlights=[("render-closeup", "Thirty-five live jacks.", "Every input and output of the original patch bay works, with cables that hang like real ones."),
                    ("ui-synth", "Two filters with attitude.", "Korg35 low-pass and high-pass, solved sample by sample, with the slopes of the 1978 circuit."),
                    ("ui-keyboard", "Wheel and switch, patchable.", "The control wheel and the momentary switch can drive anything on the patch bay."),
                    ("ui-extensions", "A vocoder in the box.", "Sing into the audio input and play chords, from 8 to 20 bands."),
                    ("render-low", "Up to twelve voices.", "Mono like the original, or polyphonic with unison, detune and stereo spread.")],
        stats=[("35", "", "live jacks on the patch bay"), ("2", "", "filter revisions, Korg35 and OTA"), ("12", "", "voices with up to 4-voice unison"), ("51", "", "presets, from the 1978 charts to self-playing patches")],
        film_cap="AT20V rendered in 3D, in three shots. The sound is the engine playing Random Bleeps, a self-playing preset."),
    "es": dict(tag="El semimodular que grita.",
        highlights=[("render-closeup", "Treinta y cinco jacks vivos.", "Cada entrada y salida de la patchera original funciona, con cables que cuelgan como los de verdad."),
                    ("ui-synth", "Dos filtros con carácter.", "Pasa bajos y pasa altos Korg35, resueltos muestra por muestra, con las pendientes del circuito de 1978."),
                    ("ui-keyboard", "Rueda y pulsador patcheables.", "La rueda de control y el pulsador pueden manejar cualquier cosa en la patchera."),
                    ("ui-extensions", "Un vocoder adentro.", "Cantás por la entrada de audio y tocás acordes, de 8 a 20 bandas."),
                    ("render-low", "Hasta doce voces.", "Mono como el original, o polifónico con unísono, desafinación y apertura estéreo.")],
        stats=[("35", "", "jacks vivos en la patchera"), ("2", "", "revisiones del filtro, Korg35 y OTA"), ("12", "", "voces con unísono de hasta 4"), ("51", "", "presets, de las cartas de 1978 a patches que se tocan solos")],
        film_cap="El AT20V renderizado en 3D, en tres tomas. El sonido es el motor tocando Random Bleeps, un preset que se toca solo."),
},
"at101v": {
    "en": dict(tag="The bass line machine.",
        highlights=[("at101v/hl-filter", "A filter that sings.", "The IR3109 four-pole low-pass, with a resonance that self-oscillates in tune."),
                    ("at101v/hl-seq", "A sequencer you can see.", "100 steps with accent, slide, tie and probability, plus random, mutate and .mid export."),
                    ("at101v/hl-bender", "Every slider, every switch.", "Bender, portamento, transpose and the modulation push, as on the 1982 panel."),
                    ("at101v/hl-settings", "What the SH-101 never had.", "Polyphony, host sync, chorus, delay, reverb and MIDI learn on every control."),
                    ("at101v/hero-red", "Three classic colours.", "Grey, blue or red, rendered from measurements of the original.")],
        stats=[("1", "+ sub", "oscillator, as on the original"), ("100", "", "sequencer steps with accent and slide"), ("8", "", "voices when you want more than one"), (str(N101), "", "presets, from acid lines to ambient")],
        colours=[("grey", "Grey", "#5b5a56"), ("blue", "Blue", "#24629b"), ("red", "Red", "#ae352f")],
        film_cap="AT101V rendered in 3D in its grey, blue and red bodies: the keys follow the sequence, the LEDs blink on the beat and the filter faders move. The sound is the engine playing Acid Line, a sequence preset."),
    "es": dict(tag="La máquina de líneas de bajo.",
        highlights=[("at101v/hl-filter", "Un filtro que canta.", "El pasa bajos de cuatro polos IR3109, con una resonancia que autooscila afinada."),
                    ("at101v/hl-seq", "Un secuenciador que se ve.", "100 pasos con acento, slide, ligadura y probabilidad, más azar, mutar y exportar a .mid."),
                    ("at101v/hl-bender", "Cada fader, cada llave.", "Palanca, portamento, transposición y el empuje de modulación, como en el panel de 1982."),
                    ("at101v/hl-settings", "Lo que el SH-101 nunca tuvo.", "Polifonía, sincronía con el host, chorus, delay, reverb y MIDI learn en cada control."),
                    ("at101v/hero-red", "Tres colores clásicos.", "Gris, azul o rojo, renderizados a partir de mediciones del original.")],
        stats=[("1", "+ sub", "oscilador, como el original"), ("100", "", "pasos de secuencia con acento y slide"), ("8", "", "voces cuando querés más de una"), (str(N101), "", "presets, de líneas acid a ambient")],
        colours=[("grey", "Gris", "#5b5a56"), ("blue", "Azul", "#24629b"), ("red", "Rojo", "#ae352f")],
        film_cap="El AT101V renderizado en 3D en sus cuerpos gris, azul y rojo: las teclas siguen la secuencia, los LED marcan el pulso y se mueven los faders del filtro. El sonido es el motor tocando Acid Line, un preset de secuencia."),
},
"at42": {
    "en": dict(tag="Twenty oscillators. One slow storm.",
        highlights=[("at42/hl-orbs", "Light you can play.", "With MOD on, each drone follows its orb: ambient light, the shadow of your mouse and the red LED inside."),
                    ("at42/hl-effector", "Thirteen cartridges.", "Two effect engines with 39 programs: shimmer, reverse delays, ring mods, crushers and more."),
                    ("at42/hl-touch", "Twelve plates and a screen.", "19 scales, an arpeggiator and a 16-step sequencer on the touch keyboard."),
                    ("at42/hl-patch", "Sixty-two jacks.", "Every control voltage is patchable, and the internal normals keep it playing without a single cable."),
                    ("at42/hl-settings", "What the original doesn't do.", "MIDI, host sync, chromatic drones, 30-second releases and stereo width.")],
        stats=[("20", "", "drone oscillators with light orbs"), ("62", "", "jacks with the hardware's normals"), ("39", "", "effect programs on 13 cartridges"), (str(N42), "", "presets, from cathedrals to techno")],
        colours=[("cream", "Cream", "#e6d8c4"), ("black", "Black", "#1e1e20"), ("maize", "Maize", "#d99a22"), ("blue", "Blue", "#2c5a88")],
        film_cap="AT42 rendered in 3D: the light orbs breathe with the drone, the touch plates light up and the joystick moves. The sound is the engine playing Cathedral of A, a drone preset."),
    "es": dict(tag="Veinte osciladores. Una tormenta lenta.",
        highlights=[("at42/hl-orbs", "Luz que se toca.", "Con MOD encendido, cada drone sigue a su orbe: la luz ambiente, la sombra del mouse y el LED rojo de adentro."),
                    ("at42/hl-effector", "Trece cartuchos.", "Dos motores de efectos con 39 programas: shimmer, delays invertidos, ring mod, crushers y más."),
                    ("at42/hl-touch", "Doce placas y una pantalla.", "19 escalas, arpegiador y secuenciador de 16 pasos en el teclado táctil."),
                    ("at42/hl-patch", "Sesenta y dos jacks.", "Cada tensión de control se puede patchear, y las conexiones internas lo hacen sonar sin un solo cable."),
                    ("at42/hl-settings", "Lo que el original no hace.", "MIDI, sincronía con el host, drones cromáticos, releases de 30 segundos y ancho estéreo.")],
        stats=[("20", "", "osciladores drone con orbes de luz"), ("62", "", "jacks con las conexiones internas del hardware"), ("39", "", "programas de efectos en 13 cartuchos"), (str(N42), "", "presets, de catedrales al techno")],
        colours=[("cream", "Crema", "#e6d8c4"), ("black", "Negro", "#1e1e20"), ("maize", "Maíz", "#d99a22"), ("blue", "Azul", "#2c5a88")],
        film_cap="El AT42 renderizado en 3D: los orbes de luz respiran con el drone, las placas táctiles se encienden y el joystick se mueve. El sonido es el motor tocando Cathedral of A, un preset de drones."),
},
"at700": {
    "en": dict(tag="The modular that filled a wall.",
        highlights=[("at700/hl-console", "Seventeen modules.", "Three VCOs, noise and ring mod, two filters, two VCAs, two LFOs, sample and hold, envelopes and the output module, wired as in 1976."),
                    ("at700/hl-seq", "The 717A sequencer.", "Three channels of twelve steps or 36 in series, with a time divide, a hold and a gate on every step."),
                    ("at700/hl-filters", "Two filter families.", "The 703E OTA low-pass that loses bass with resonance, and the 703B multimode; both self-oscillate in tune."),
                    ("at700/hl-keys", "A duophonic keyboard.", "The 701A sends the lowest and the highest key; Settings turn it into an 8-voice polysynth."),
                    ("at700/hl-settings", "What 1976 didn't have.", "MIDI, host sync for the sequencer, polyphony, two VCF-1 models and presets with their cables.")],
        stats=[("17", "", "modules on the Main Console"), ("137", "", "jacks, with stacked plugs and multiples"), ("36", "", "steps on the 717A in series"), (str(N700), "", "presets with their cables")],
        film_cap="AT700 rendered in 3D: the 717A sequencer lamps running Berlin Twelve at 128 BPM, the VU meters and the cables, up to the whole wall. The sound is the engine playing that sequence."),
    "es": dict(tag="El modular que ocupaba una pared.",
        highlights=[("at700/hl-console", "Diecisiete módulos.", "Tres VCO, ruido y ring mod, dos filtros, dos VCA, dos LFO, sample and hold, envolventes y el módulo de salida, cableados como en 1976."),
                    ("at700/hl-seq", "El secuenciador 717A.", "Tres canales de doce pasos o 36 en serie, con división de tiempo, hold y gate en cada paso."),
                    ("at700/hl-filters", "Dos familias de filtro.", "El pasa bajos OTA 703E que pierde graves con la resonancia y el multimodo 703B; los dos autooscilan afinados."),
                    ("at700/hl-keys", "Un teclado duofónico.", "El 701A manda la tecla más grave y la más aguda; en Settings se convierte en un polifónico de 8 voces."),
                    ("at700/hl-settings", "Lo que 1976 no tenía.", "MIDI, sincronía del secuenciador con el host, polifonía, dos modelos de VCF-1 y presets con sus cables.")],
        stats=[("17", "", "módulos en la consola principal"), ("137", "", "jacks, con fichas apilables y múltiples"), ("36", "", "pasos del 717A en serie"), (str(N700), "", "presets con sus cables")],
        film_cap="El AT700 renderizado en 3D: las lámparas del secuenciador 717A corriendo Berlin Twelve a 128 BPM, los VU y los cables, hasta la pared entera. El sonido es el motor tocando esa secuencia."),
},
"atfs1": {
    "en": dict(tag="The synthesizer that talks.",
        highlights=[("atfs1/hl-fseq", "Formant Sequences.", "Eight formants and their breath, frame by frame, 126 times a second: vowels, words, whole phrases at the pitch you play."),
                    ("atfs1/hl-voice", "Eighty-eight algorithms, drawn.", "The FS1R showed them in a few pixels. Here every operator is a box you click, with its level live."),
                    ("atfs1/hl-operator", "Eight spectral forms.", "One partial, a group, only the odd ones, a resonance or a formant: each operator draws its spectrum as you turn it."),
                    ("atfs1/hl-knobs", "The front panel works.", "ATTACK, RELEASE, FORMANT and FM, the data knobs, PLAY and EDIT: the 1U front drives the editor."),
                    ("atfs1/hl-lcd", "Make your own Fseqs.", "Drop a recording and the ATFS1 tracks its pitch and formants, or type vowels and it builds one. The original could not.")],
        stats=[("16", "", "operators per voice, 8 voiced and 8 noise"), ("88", "", "algorithms with feedback"), ("32", "", "voices, every one with its filter"), (str(NFSEQ), "", "Formant Sequences, plus any you make")],
        film_cap="ATFS1 rendered in 3D in a studio rack: the display, the buttons and the knobs follow the music. The sound is the engine playing Talking Formants, whose Formant Sequence says Audio Tools."),
    "es": dict(tag="El sintetizador que habla.",
        highlights=[("atfs1/hl-fseq", "Formant Sequences.", "Ocho formantes y su aliento, cuadro por cuadro, 126 veces por segundo: vocales, palabras, frases enteras en la altura que tocás."),
                    ("atfs1/hl-voice", "Ochenta y ocho algoritmos, dibujados.", "El FS1R los mostraba en unos pocos píxeles. Acá cada operador es una caja que se clickea, con su nivel en vivo."),
                    ("atfs1/hl-operator", "Ocho formas espectrales.", "Una parcial, un grupo, sólo las impares, una resonancia o un formante: cada operador dibuja su espectro mientras lo girás."),
                    ("atfs1/hl-knobs", "El frente funciona.", "ATTACK, RELEASE, FORMANT y FM, las perillas de datos, PLAY y EDIT: el frente de 1U maneja el editor."),
                    ("atfs1/hl-lcd", "Hacé tus propios Fseqs.", "Soltás una grabación y el ATFS1 sigue su altura y sus formantes, o escribís vocales y arma uno. El original no podía.")],
        stats=[("16", "", "operadores por voz, 8 con voz y 8 de ruido"), ("88", "", "algoritmos con realimentación"), ("32", "", "voces, todas con su filtro"), (str(NFSEQ), "", "Formant Sequences, más los que hagas")],
        film_cap="El ATFS1 renderizado en 3D en un rack de estudio: la pantalla, los botones y las perillas siguen a la música. El sonido es el motor tocando Talking Formants, cuya Formant Sequence dice Audio Tools."),
},
"orrery": {
    "en": dict(tag="Music that plays itself, slowly.",
        highlights=[("orrery/hl-sky", "Eight planets, eight lanes.", "Each lane orbits at its own loop length, so the parts drift in and out of phase for hours, the way Eno's tape loops did."),
                    ("orrery/hl-moods", "Pick a mood, press play.", "Ambient, dark ambient, drone, sleep, meditation, focus and celestial, with five knobs that shape everything."),
                    ("orrery/hl-lanes", "Every lane under your hand.", "Role, register, density, loops, Euclidean rhythm, echoes, a controller curve and analog drift, lane by lane."),
                    ("orrery/hl-close", "Plays your hardware.", "Each lane can go to its own MIDI port and channel, with MIDI clock: one Orrery plays a whole rack."),
                    ("orrery/hl-top", "Sessions that breathe.", "Set a length with slow fades, pace the music to your breathing and keep it gentle for sleep.")],
        stats=[("8", "", "lanes, each on its own MIDI channel"), ("16", "", "scales and modes"), (str(NOR), "", "presets in seven moods"), ("&lt;0.01", "%", "of a CPU core for the generator")],
        film_cap="A brass orrery rendered in 3D: each planet orbits at its own speed and lights up when its lane plays, and the sun breathes with the music. The sound is Orrery playing First Light through ATFS1, AT700 and AT101V."),
    "es": dict(tag="Música que se toca sola, despacio.",
        highlights=[("orrery/hl-sky", "Ocho planetas, ocho carriles.", "Cada carril orbita al largo de su lazo, así las partes entran y salen de fase durante horas, como los lazos de cinta de Eno."),
                    ("orrery/hl-moods", "Elegí un clima, apretá Play.", "Ambient, dark ambient, drone, dormir, meditar, foco y celestial, con cinco perillas que le dan forma a todo."),
                    ("orrery/hl-lanes", "Cada carril en tus manos.", "Rol, registro, densidad, lazos, ritmo euclidiano, ecos, una curva de controlador y deriva analógica, carril por carril."),
                    ("orrery/hl-close", "Toca tus sintes físicos.", "Cada carril puede salir por su propio puerto y canal MIDI, con reloj: una sola Orrery toca un rack entero."),
                    ("orrery/hl-top", "Sesiones que respiran.", "Fijás una duración con fades lentos, llevás la música al ritmo de tu respiración y la dejás suave para dormir.")],
        stats=[("8", "", "carriles, cada uno en su canal MIDI"), ("16", "", "escalas y modos"), (str(NOR), "", "presets en siete climas"), ("&lt;0,01", "%", "de un núcleo para el generador")],
        film_cap="Un orrery de bronce renderizado en 3D: cada planeta orbita a su velocidad y se enciende cuando toca su carril, y el sol respira con la música. El sonido es Orrery tocando First Light con el ATFS1, el AT700 y el AT101V."),
},
"atmd": {
    "en": dict(tag="The fattest bass ever wired.",
        highlights=[("atmd/hl-panel", "Built to the millimetre.", "A 3D model made from the panel's published measurements: every knob, rocker and wheel where it was, and all of them work."),
                    ("atmd/hl-filter", "The ladder that started it.", "Four transistor stages, 24 dB per octave, a self-oscillation that sings from EMPHASIS 8 and follows the keyboard."),
                    ("atmd/hl-osc", "Three oscillators, six shapes.", "Oscillator 3 has the reverse sawtooth and, free from the keyboard, becomes the modulator for vibrato, trills and sweeps."),
                    ("atmd/hl-wheels", "Played like the original.", "Low-note priority, single trigger, glide in time per octave, and the two wheels moving with your MIDI controller."),
                    ("atmd/hl-settings", "And what it never had.", "Up to eight voices, unison, velocity and pressure, delay and reverb, on a page that leaves the keys playable."),
                    ("atmd/hl-keys", "Forty-four keys, F to C.", "Click the keys, play them from the computer keyboard, or from any MIDI controller.")],
        stats=[("3", "", "oscillators, six waveforms each"), ("24", "dB", "per octave, the transistor ladder"), (str(NMD), "", "presets in five categories"), ("0.9", "%", "of a CPU core for one voice, M4")],
        film_cap="ATMD rendered in 3D: the keys play the bass line and the overload lamp catches its peaks. The sound is the engine playing Fat Bass."),
    "es": dict(tag="El bajo más gordo jamás cableado.",
        highlights=[("atmd/hl-panel", "Hecho al milímetro.", "Un modelo 3D hecho con las medidas publicadas del panel: cada perilla, rocker y rueda donde estaba, y todas funcionan."),
                    ("atmd/hl-filter", "La escalera que lo empezó todo.", "Cuatro etapas de transistores, 24 dB por octava, una autooscilación que canta desde EMPHASIS 8 y sigue al teclado."),
                    ("atmd/hl-osc", "Tres osciladores, seis formas.", "El osc 3 tiene la sierra invertida y, libre del teclado, se vuelve el modulador para vibratos, trinos y barridos."),
                    ("atmd/hl-wheels", "Se toca como el original.", "Prioridad a la grave, disparo único, glide en tiempo por octava y las dos ruedas moviéndose con tu controlador MIDI."),
                    ("atmd/hl-settings", "Y lo que nunca tuvo.", "Hasta ocho voces, unísono, velocidad y presión, delay y reverb, en una página que deja el teclado para tocar."),
                    ("atmd/hl-keys", "Cuarenta y cuatro teclas, de Fa a Do.", "Hacé clic en las teclas, tocalas desde el teclado de la computadora o desde cualquier controlador MIDI.")],
        stats=[("3", "", "osciladores, seis formas cada uno"), ("24", "dB", "por octava, la escalera de transistores"), (str(NMD), "", "presets en cinco categorías"), ("0,9", "%", "de un núcleo para una voz, M4")],
        film_cap="El ATMD renderizado en 3D: las teclas tocan la línea de bajo y la lámpara de saturación marca sus picos. El sonido es el motor tocando Fat Bass."),
},
"murmur": {
    "en": dict(tag="Your sound, in flight.",
        highlights=[("murmur/hl-flock", "A flock made of sound.", "Up to 128 grains read the last 16 seconds of whatever you feed it, and move together like starlings at dusk."),
                    ("murmur/hl-simple", "See every grain.", "Each grain is a bird over the waveform: where it reads, how high it plays, which way it flies."),
                    ("murmur/hl-freeze", "Freeze and stretch.", "Hold the pedal and the memory stops; Scan walks through it ten times slower without changing the pitch."),
                    ("murmur/hl-pitch", "Grains in tune.", "Random pitch snapped to octaves, fifths, chords or scales, shimmer an octave up, and MIDI to choose the notes."),
                    ("murmur/hl-advanced", "Every control in reach.", "Forty-eight controls in eight groups: grains, time, pitch, envelope, tone, two LFOs, effects and MIDI.")],
        stats=[("128", "", "grains at once"), ("16", "s", "of memory to take grains from"), (str(NMM), "", "presets in eight categories"), ("0.6", "%", "of a CPU core at most for a factory preset, M4")],
        film_cap="A murmuration of starlings rendered in 3D that swells, splits and turns with the music. The sound is Murmur playing Choir Rising over an AT101V pad."),
    "es": dict(tag="Tu sonido, en vuelo.",
        highlights=[("murmur/hl-flock", "Una bandada hecha de sonido.", "Hasta 128 granos leen los últimos 16 segundos de lo que le des, y se mueven juntos como estorninos al atardecer."),
                    ("murmur/hl-simple", "Mirá cada grano.", "Cada grano es un pájaro sobre la forma de onda: dónde lee, qué tan alto suena, para qué lado vuela."),
                    ("murmur/hl-freeze", "Congelá y estirá.", "Apretás el pedal y la memoria se detiene; Scan la recorre diez veces más despacio sin cambiar la altura."),
                    ("murmur/hl-pitch", "Granos afinados.", "Altura al azar ajustada a octavas, quintas, acordes o escalas, shimmer una octava arriba, y MIDI para elegir las notas."),
                    ("murmur/hl-advanced", "Todo a mano.", "Cuarenta y ocho controles en ocho grupos: granos, tiempo, altura, envolvente, tono, dos LFO, efectos y MIDI.")],
        stats=[("128", "", "granos a la vez"), ("16", "s", "de memoria de donde tomar granos"), (str(NMM), "", "presets en ocho categorías"), ("0,6", "%", "de un núcleo como mucho para un preset de fábrica, M4")],
        film_cap="Una bandada de estorninos renderizada en 3D que crece, se parte y gira con la música. El sonido es Murmur tocando Choir Rising sobre un pad del AT101V."),
},
"at303": {
    "en": dict(tag="The squelch, from the circuit up.",
        highlights=[("at303/hl-close", "A filter that squelches.", "A diode ladder with its real capacitor values: it loses bass as the resonance rises and rings near 500 Hz at half cutoff."),
                    ("at303/hl-ui", "Accents that climb.", "Every accent charges a capacitor that drains slowly, so a run of them gets brighter and louder. The bar on the panel shows its voltage."),
                    ("at303/hl-editor", "Two lists, visible.", "The rhythm and the pitches, and which pitch each step plays. Change the rhythm and the melody moves along."),
                    ("at303/hl-inspect", "The circuit, live.", "Inspect draws the ladder's capacitors, the envelopes and the accent capacitor while it plays."),
                    ("at303/hl-top", "A line in one click.", "The generator writes acid lines in the key and scale you choose; Mutate keeps the rhythm and redraws the notes.")],
        stats=[("4", "", "poles of diode ladder, solved sample by sample"), ("64", "", "patterns saved with your project"), ("34", "", "presets, each with its own pattern"), ("0.9", "%", "of one core on an Apple M4")],
        film_cap="An original graphite instrument rendered in 3D: the pads chase the pattern, the accent and slide lamps flash, and the cutoff knob turns exactly as the filter opens in the sound. The sound is AT303 playing Squelch Line at 128 BPM."),
    "es": dict(tag="El chillido, desde el circuito.",
        highlights=[("at303/hl-close", "Un filtro que chilla.", "Una escalera de diodos con los valores reales de sus capacitores: pierde graves al subir la resonancia y suena cerca de 500 Hz con el corte al medio."),
                    ("at303/hl-ui", "Acentos que suben.", "Cada acento carga un capacitor que se descarga despacio, así que una seguidilla suena cada vez más brillante y fuerte. La barra del panel muestra su tensión."),
                    ("at303/hl-editor", "Dos listas, a la vista.", "El ritmo y las alturas, y qué altura toca cada paso. Cambiá el ritmo y la melodía se corre."),
                    ("at303/hl-inspect", "El circuito, en vivo.", "Inspect dibuja los capacitores de la escalera, las envolventes y el capacitor del acento mientras suena."),
                    ("at303/hl-top", "Una línea en un clic.", "El generador escribe líneas ácidas en la tonalidad y la escala que elijas; Mutate conserva el ritmo y vuelve a dibujar las notas.")],
        stats=[("4", "", "polos de escalera de diodos, resueltos muestra a muestra"), ("64", "", "patrones que se guardan con tu proyecto"), ("34", "", "presets, cada uno con su patrón"), ("0,9", "%", "de un núcleo en un Apple M4")],
        film_cap="Un instrumento original de grafito renderizado en 3D: los pads siguen el patrón, las lámparas de acento y slide titilan, y la perilla de corte gira igual que se abre el filtro en el sonido. El sonido es AT303 tocando Squelch Line a 128 BPM."),
},
"plumb": {
    "en": dict(tag="Sing in tune. Feel no delay.",
        highlights=[("plumb/hl-close", "0.67 ms.", "Live mode adds 32 samples: less than most audio interfaces. Sing through it on stage and in the headphones."),
                    ("plumb/hl-ui", "Chalk and brass.", "The sung pitch and the corrected one, live, over the notes of the scale."),
                    ("plumb/hl-bob", "The bob hangs still.", "A plumb bob swings with the tuning error and settles when the note is right."),
                    ("plumb/hl-adv", "Formants stay put.", "Big shifts keep the voice's own colour; Throat changes it on purpose."),
                    ("plumb/hl-top", "Natural to hard.", "One knob from a gentle hand that keeps the vibrato to the stepped effect.")],
        stats=[("0.67", "ms", "added in Live mode, reported to the host"), ("3", "", "latency modes, from stage to mix"), ("25", "", "presets"), ("3", "%", "of one core on an Apple M4")],
        film_cap="A brass plumb bob rendered in 3D, swinging over a slate and settling as the note comes into tune. The sound is the Natural Lead demo: a phrase as sung, then corrected."),
    "es": dict(tag="Cantá afinado. Sin sentir el retardo.",
        highlights=[("plumb/hl-close", "0,67 ms.", "El modo Live agrega 32 muestras: menos que la mayoría de las interfaces de audio. Se canta a través de él en el escenario y en los auriculares."),
                    ("plumb/hl-ui", "Tiza y bronce.", "La altura cantada y la corregida, en vivo, sobre las notas de la escala."),
                    ("plumb/hl-bob", "La plomada se queda quieta.", "Una plomada oscila con el error de afinación y se calma cuando la nota está justa."),
                    ("plumb/hl-adv", "Los formantes no se mueven.", "Los corrimientos grandes conservan el color propio de la voz; Throat lo cambia a propósito."),
                    ("plumb/hl-top", "De natural a dura.", "Una perilla, desde una mano suave que conserva el vibrato hasta el efecto escalonado.")],
        stats=[("0.67", "ms", "agregados en modo Live, informados al host"), ("3", "", "modos de latencia, del escenario a la mezcla"), ("25", "", "presets"), ("3", "%", "de un núcleo en un Apple M4")],
        film_cap="Una plomada de bronce renderizada en 3D, oscilando sobre una pizarra y calmándose a medida que la nota se afina. El sonido es la demo Natural Lead: una frase como se cantó y después corregida."),
},
"tarnish": {
    "en": dict(tag="Every sound gets old. Choose how.",
        highlights=[("tarnish/hl-close", "Real tape, modelled.", "Magnetic hysteresis with the record and playback curves of real standards: hot cassettes close their top end, under-biased tape grits up."),
                    ("tarnish/hl-ui", "Wear you can watch.", "Each dropout is a mark on the tape that reaches the play head the moment you hear it."),
                    ("tarnish/hl-vinyl", "Records, radios, phones.", "Crackle and scratches locked to the turn, an AM set fading in and out, a mobile call breaking up."),
                    ("tarnish/hl-loop", "The loop, live.", "The service window draws the tape's magnetic loop and measures the wow and flutter it is making."),
                    ("tarnish/hl-top", "Bounces match playback.", "Everything random follows the song position, so the damage is the same on every pass and in every render.")],
        stats=[("6", "", "machines from studio tape to a phone line"), ("4x", "", "oversampled magnetic hysteresis"), ("33", "", "presets on six shelves"), ("0", "", "samples: every bit of it is modelled")],
        film_cap="An open-reel machine rendered in 3D: the reels, the tape over the heads, the VU needles on the music and a tape stop. The sound is an ATFS1 piano through Warm Studio Tape, stopping the tape on cue."),
    "es": dict(tag="Todo sonido envejece. Elegí cómo.",
        highlights=[("tarnish/hl-close", "Cinta real, modelada.", "Histéresis magnética con las curvas de grabación y reproducción de las normas: las casetes grabadas fuerte cierran sus agudos, la cinta con sub-bias se ensucia."),
                    ("tarnish/hl-ui", "Un desgaste que se ve.", "Cada caída es una marca en la cinta que llega a la cabeza de reproducción en el momento en que la escuchás."),
                    ("tarnish/hl-vinyl", "Discos, radios, teléfonos.", "Crujido y rayas atados a la vuelta, una radio de AM que va y viene, una llamada de celular que se corta."),
                    ("tarnish/hl-loop", "El lazo, en vivo.", "La ventana de servicio dibuja el lazo magnético de la cinta y mide el wow y flutter que está generando."),
                    ("tarnish/hl-top", "Los bounces suenan igual.", "Todo lo aleatorio sigue la posición de la canción: el daño es el mismo en cada pasada y en cada render.")],
        stats=[("6", "", "máquinas, de la cinta de estudio a una línea de teléfono"), ("4x", "", "histéresis magnética sobremuestreada"), ("33", "", "presets en seis estantes"), ("0", "", "samples: todo está modelado")],
        film_cap="Un grabador de carretes renderizado en 3D: los carretes, la cinta por las cabezas, las agujas del VU con la música y un frenado de cinta. El sonido es un piano del ATFS1 por Warm Studio Tape, con la cinta frenando a tiempo."),
},
"halation": {
    "en": dict(tag="The glow a sound leaves behind.",
        highlights=[("halation/hl-close", "One slider, two worlds.", "Delay to Space turns echoes on the beat into a room, and the tail keeps its length the whole way."),
                    ("halation/hl-ui", "Light in a plaster room.", "Each echo is a pulse of light: size is level, colour is brightness, mist is density."),
                    ("halation/hl-freeze", "Hold the room.", "Freeze everything, or only the lows, and play over it. The light stops where it was."),
                    ("halation/hl-mods", "Ten modules in the loop.", "Shimmer, reverse, grains, spectral smear, a tuned resonator, rooms that breathe or collapse."),
                    ("halation/hl-top", "Safe at infinity.", "A limiter inside the loop and a -1 dBFS ceiling keep endless and frozen tails in check.")],
        stats=[("8", "", "delay lines from echo to room"), ("60", "s", "of tail, or endless"), ("30", "", "presets in four moods"), ("2.6", "%", "of a CPU core with every module on, M4")],
        film_cap="A plaster model rendered in 3D: every echo is a light that bounces on the beat, and the room fills with glow as Delay turns into Space. The sound is Halation playing Ping-Pong Machine on an arpeggio."),
    "es": dict(tag="El halo que deja un sonido.",
        highlights=[("halation/hl-close", "Una barra, dos mundos.", "Delay a Space convierte ecos sobre el pulso en una sala, y la cola conserva su largo todo el recorrido."),
                    ("halation/hl-ui", "Luz en una sala de yeso.", "Cada eco es un pulso de luz: el tamaño es el nivel, el color es el brillo, la niebla es la densidad."),
                    ("halation/hl-freeze", "Retené la sala.", "Congelá todo, o sólo los graves, y tocá encima. La luz se queda donde estaba."),
                    ("halation/hl-mods", "Diez módulos en el lazo.", "Shimmer, reversa, granos, corrimiento espectral, un resonador afinado, salas que respiran o se cierran."),
                    ("halation/hl-top", "Seguro en el infinito.", "Un limitador dentro del lazo y un techo de -1 dBFS mantienen a raya las colas infinitas y congeladas.")],
        stats=[("8", "", "líneas de retardo, del eco a la sala"), ("60", "s", "de cola, o infinito"), ("30", "", "presets en cuatro climas"), ("2,6", "%", "de un núcleo con todos los módulos, M4")],
        film_cap="Una maqueta de yeso renderizada en 3D: cada eco es una luz que rebota sobre el pulso, y la sala se llena de brillo cuando Delay pasa a Space. El sonido es Halation tocando Ping-Pong Machine sobre un arpegio."),
},
"lacquer": {
    "en": dict(tag="Cut it loud. Or cut it right.",
        highlights=[("lacquer/hl-close", "One knob of work.", "Press Learn or drop a mix. Lacquer measures it and sets a whole mastering chain in seconds."),
                    ("lacquer/hl-ui", "Your song, as a groove.", "The disc is a lacquer and its groove is your master: the louder the moment, the wider the cut."),
                    ("lacquer/hl-read", "Fair comparisons only.", "A/B at the same loudness, and Hear it as Spotify to play it the way listeners will."),
                    ("lacquer/hl-mod", "Nothing hidden.", "Every module says what it did and why, with its gain reduction as it happens."),
                    ("lacquer/hl-top", "Exact loudness.", "From a file, it renders, measures and corrects until the master reads the target within a few hundredths of a LU.")],
        stats=[("6", "", "styles over our own target curves"), ("8x", "", "true-peak detection in the limiter"), ("0.04", "LU", "worst loudness miss in the tests, reachable targets"), ("2.9", "%", "of a CPU core with every module on, M4")],
        film_cap="A cutting lathe rendered in 3D, in three shots; the groove follows the demo master. The sound is the demo mix mastered by Lacquer in the Balanced style."),
    "es": dict(tag="Cortalo fuerte. O cortalo bien.",
        highlights=[("lacquer/hl-close", "Una perilla de trabajo.", "Apretás Learn o soltás una mezcla. Lacquer la mide y arma una cadena de mastering entera en segundos."),
                    ("lacquer/hl-ui", "Tu canción, como un surco.", "El disco es una laca y su surco es tu master: cuanto más fuerte el momento, más ancho el corte."),
                    ("lacquer/hl-read", "Sólo comparaciones justas.", "A/B a la misma sonoridad, y Hear it as Spotify para escucharlo como lo va a escuchar la gente."),
                    ("lacquer/hl-mod", "Nada escondido.", "Cada módulo dice qué hizo y por qué, con su reducción de ganancia mientras ocurre."),
                    ("lacquer/hl-top", "Sonoridad exacta.", "Desde un archivo, renderiza, mide y corrige hasta que el master mide el objetivo con un error de centésimas de LU.")],
        stats=[("6", "", "estilos sobre curvas objetivo propias"), ("8x", "", "detección de pico verdadero en el limitador"), ("0,04", "LU", "peor error de sonoridad en las pruebas, objetivos alcanzables"), ("2,9", "%", "de un núcleo con todos los módulos, M4")],
        film_cap="Un torno de corte renderizado en 3D, en tres tomas; el surco sigue al master de demostración. El sonido es la mezcla de demostración masterizada por Lacquer en el estilo Balanced."),
},
}

# Especificaciones por producto (grupos → líneas).
SPECS = {
"at20v": {
    "en": [("Sound engine", ["Two VCOs with triangle, saw, pulse, noise and ring modulation; Hz/V keyboard", "Korg35 high-pass (6 dB/oct) and low-pass (12 dB/oct), Rev 1 or Rev 2 (OTA)",
                             "Two envelopes, modulation generator, sample and hold, External Signal Processor", "4x or 8x oversampling"]),
           ("Patch bay", ["35 jacks with the original normals", "Stacked plugs on outputs, rope-physics cables"]),
           ("Extensions", ["Vocoder with 8 to 20 bands", "Up to 12 voices, up to 4-voice unison", "Tempo-synced modulation generator", "MIDI out (Thru and Patch)", "Delay and plate reverb"]),
           ("Formats", ["VST3, Audio Unit (macOS) and standalone app", "AT20V FX: effect version for audio tracks"])],
    "es": [("Motor de sonido", ["Dos VCO con triángulo, sierra, pulso, ruido y ring; teclado en Hz/V", "Pasa altos Korg35 (6 dB/oct) y pasa bajos (12 dB/oct), Rev 1 o Rev 2 (OTA)",
                                "Dos envolventes, generador de modulación, sample and hold, procesador de señal externa", "Sobremuestreo 4x u 8x"]),
           ("Patchera", ["35 jacks con las conexiones internas del original", "Fichas apilables en las salidas, cables con física de cuerda"]),
           ("Extensiones", ["Vocoder de 8 a 20 bandas", "Hasta 12 voces, unísono de hasta 4", "Generador de modulación sincronizado al tempo", "Salida MIDI (Thru y Patch)", "Delay y reverb de placa"]),
           ("Formatos", ["VST3, Audio Unit (macOS) y aplicación independiente", "AT20V FX: versión de efecto para pistas de audio"])],
},
"at101v": {
    "en": [("Sound engine", ["CEM3340-style saw and pulse with PWM, sub-oscillator (−1, −2 oct, 25 % pulse), noise", "IR3109 four-pole low-pass with diode-limited resonance",
                             "RC envelope, modulator as clock, arpeggiator and 100-step sequencer", "2x, 4x or 8x oversampling"]),
           ("Extensions", ["Visual step editor: accent, slide, tie, probability, random, mutate, .mid export", "Up to 8 voices, up to 4-voice unison", "Host sync with swing and gate length",
                           "Velocity, drive, chorus, delay, reverb", "MIDI learn and MIDI out", "Grey, blue and red bodies"]),
           ("Formats", ["VST3, Audio Unit (macOS) and standalone app"])],
    "es": [("Motor de sonido", ["Sierra y pulso estilo CEM3340 con PWM, sub-oscilador (−1, −2 oct, pulso 25 %), ruido", "Pasa bajos de cuatro polos IR3109 con resonancia limitada por diodos",
                                "Envolvente RC, modulador como reloj, arpegiador y secuenciador de 100 pasos", "Sobremuestreo 2x, 4x u 8x"]),
           ("Extensiones", ["Editor de pasos visual: acento, slide, ligadura, probabilidad, azar, mutar, exportar .mid", "Hasta 8 voces, unísono de hasta 4", "Sincronía con el host con swing y largo de gate",
                            "Velocidad, drive, chorus, delay, reverb", "MIDI learn y salida MIDI", "Gabinete gris, azul o rojo"]),
           ("Formatos", ["VST3, Audio Unit (macOS) y aplicación independiente"])],
},
"at42": {
    "en": [("Sound engine", ["4 drone voices of 5 negistor-style saw oscillators, VOLT coupling, light orbs", "2 Papa Srapa noise voices with FM, AM, noise and sample and hold",
                             "2 AS3340-style VCOs with 6 waveforms, linear FM and sync; ADSR with hold and loop", "10-channel mixer, dual Polivoks filter (LP/BP) with distortion",
                             "Dual effector: 13 cartridges × 3 programs", "1x, 2x or 4x oversampling"]),
           ("Control", ["LFO A and B, joystick, 5-step sequencer and pulser, preamp and envelope follower", "Touch keyboard: 19 scales, arpeggiator, 16-step sequencer, pressure", "6 drone keys, 62-jack patch bay"]),
           ("Extensions", ["MIDI notes, drone keys, pitch bend, mod wheel and MIDI learn", "Host sync for LFOs, pulser and keyboard clock", "Chromatic drone tuning, 30 s releases, stereo width", "Four panel colours"]),
           ("Formats", ["VST3, Audio Unit (macOS) and standalone app, with stereo audio input"])],
    "es": [("Motor de sonido", ["4 voces drone de 5 osciladores de sierra estilo negistor, acople por VOLT, orbes de luz", "2 voces de ruido Papa Srapa con FM, AM, ruido y sample and hold",
                                "2 VCO estilo AS3340 con 6 formas de onda, FM lineal y sync; ADSR con hold y loop", "Mezclador de 10 canales, doble filtro Polivoks (LP/BP) con distorsión",
                                "Efector doble: 13 cartuchos × 3 programas", "Sobremuestreo 1x, 2x o 4x"]),
           ("Control", ["LFO A y B, joystick, secuenciador de 5 pasos y pulser, preamp y seguidor de envolvente", "Teclado táctil: 19 escalas, arpegiador, secuenciador de 16 pasos, presión", "6 teclas drone, patchera de 62 jacks"]),
           ("Extensiones", ["Notas MIDI, teclas drone, pitch bend, rueda y MIDI learn", "Sincronía con el host para LFOs, pulser y reloj del teclado", "Drones cromáticos, releases de 30 s, ancho estéreo", "Cuatro colores de panel"]),
           ("Formatos", ["VST3, Audio Unit (macOS) y aplicación independiente, con entrada de audio estéreo"])],
},
"at700": {
    "en": [("Sound engine", ["Main Console: 3 × 702 VCO, 708A noise and ring modulator, 703E and 703B VCFs, 2 × 704 VCA, 711A output module",
                             "712A monitor and voltage processor, 2 × 706 LFO, 709A sample and hold, 707A amplifier, envelope follower and integrator, 705A dual ADSR, 716A mixer, 710A multiples",
                             "Spring reverb, 8-stage phase shifter, VU meters, standard oscillator", "1x, 2x or 4x oversampling"]),
           ("Control", ["701A keyboard: 61 keys, duophonic (lowest and highest key), portamento, bender", "717A sequencer: 3 × 12 steps or 36 in series, divide and fine per step, hold, analog inputs, pulse shaper",
                        "137 jacks at 1/4 inch with the console's internal buses"]),
           ("Extensions", ["Up to 8 voices, MIDI notes, pitch bend, velocity, mod wheel and aftertouch jacks, MIDI learn", "Host tempo and transport for the 717A", "703E or 703A as VCF-1, VCO drift, key CV droop",
                           "Sequencer, console, keyboard and full views"]),
           ("Formats", ["VST3, Audio Unit (macOS) and standalone app"])],
    "es": [("Motor de sonido", ["Consola principal: 3 × VCO 702, ruido y modulador en anillo 708A, VCF 703E y 703B, 2 × VCA 704, módulo de salida 711A",
                                "Monitor y procesador de tensión 712A, 2 × LFO 706, sample and hold 709A, amplificador, seguidor de envolvente e integrador 707A, doble ADSR 705A, mezclador 716A, múltiples 710A",
                                "Reverb de resorte, phase shifter de 8 etapas, vúmetros, oscilador patrón", "Sobremuestreo 1x, 2x o 4x"]),
           ("Control", ["Teclado 701A: 61 teclas, duofónico (tecla más grave y más aguda), portamento, palanca", "Secuenciador 717A: 3 × 12 pasos o 36 en serie, división y ajuste fino por paso, hold, entradas analógicas, pulse shaper",
                        "137 jacks de 1/4 de pulgada con los buses internos de la consola"]),
           ("Extensiones", ["Hasta 8 voces, notas MIDI, pitch bend, jacks de velocidad, rueda y aftertouch, MIDI learn", "Tempo y transporte del host para el 717A", "703E o 703A como VCF-1, deriva de los VCO, caída de la CV del teclado",
                            "Vistas de secuenciador, consola, teclado y completa"]),
           ("Formatos", ["VST3, Audio Unit (macOS) y aplicación independiente"])],
},
"atfs1": {
    "en": [("Sound engine", ["8 voiced operators with 8 spectral forms and 8 unvoiced noise-formant operators per voice", "88 algorithms with feedback, FM depth of 3.37 cycles",
                             "Amplitude and frequency EG on every operator, pitch EG, LFO1 and LFO2, Formant and FM knob matrices",
                             "Per-voice filter: LPF 24/18/12 dB, HPF, BPF, BEF, resonance −16 to +100, input drive", "Insertion, variation and reverb effects, two-band EQ"]),
           ("Formant Sequences", [f"{NFSEQ} factory sequences: vowels, choirs, scat and spoken words", "WAV and AIFF analysis into a new Fseq; vowel generator",
                                  "126 frames per second, up to 512 frames, loops one-way or round, scratch on the mod wheel", "Speed 10 to 1000 %, optional interpolation"]),
           ("Extensions", ["Full-screen editor: algorithm graph, spectrum per operator, draggable envelopes, inline help", "32 voices with the filter on every voice (authentic 16-voice limit as an option)",
                           "FS1R sysex: import voices and Fseqs, export voices and Fseqs", "Working 1U front panel with LCD, perform and data knobs", "On-screen keyboard and output spectrum"]),
           ("Formats", ["VST3, Audio Unit (macOS) and standalone app"])],
    "es": [("Motor de sonido", ["8 operadores con voz con 8 formas espectrales y 8 operadores de formantes de ruido por voz", "88 algoritmos con realimentación, FM de 3,37 ciclos",
                                "Envolvente de amplitud y de frecuencia en cada operador, envolvente de altura, LFO1 y LFO2, matrices de las perillas Formant y FM",
                                "Filtro por voz: LPF de 24/18/12 dB, HPF, BPF, BEF, resonancia de −16 a +100, saturación de entrada", "Efectos de inserción, variación y reverb, EQ de dos bandas"]),
           ("Formant Sequences", [f"{NFSEQ} secuencias de fábrica: vocales, coros, scat y palabras", "Análisis de WAV y AIFF a un Fseq nuevo; generador de vocales",
                                  "126 cuadros por segundo, hasta 512 cuadros, lazos de ida o de ida y vuelta, scratch con la rueda", "Velocidad de 10 a 1000 %, interpolación opcional"]),
           ("Extensiones", ["Editor de pantalla completa: gráfico del algoritmo, espectro por operador, envolventes que se arrastran, ayuda en línea", "32 voces con filtro en cada una (límite auténtico de 16 como opción)",
                            "Sysex del FS1R: importar voces y Fseqs, exportar voces y Fseqs", "Frente de 1U funcional con LCD, perillas de ejecución y de datos", "Teclado en pantalla y espectro de la salida"]),
           ("Formatos", ["VST3, Audio Unit (macOS) y aplicación independiente"])],
},
"orrery": {
    "en": [("Generator", ["8 lanes with roles: drone, pad, melody, arpeggio, cloud, bass, pulse", "Shared slow harmony: 16 scales and modes, chord colour, tension, three progression types, voice leading",
                          "Per-lane loops (2-128 beats) with mutation, Euclidean rhythms, probability, MIDI echoes", "Variation number: the same settings and number always give the same music"]),
           ("Simple and advanced", [f"7 moods with {NOR} presets, 5 macro knobs, key, scale, session and breathing", "29 settings per lane, harmony, session and output pages", "Capture of the last 1-10 minutes as a MIDI file to drag into the DAW"]),
           ("Calm listening", ["Sessions up to 2 hours with fade in and fade out (velocity and CC7)", "Breathing pace from 1 to 12 breaths per minute", "Gentle mode: softer, lower, smaller leaps, little tension"]),
           ("Outputs", ["MIDI to the DAW, one channel per lane", "Direct MIDI ports per lane (hardware, IAC, loopMIDI) with MIDI clock", "Lead and Follow link between instances", "Controller curves and pitch-bend drift; reference pitch 415-466 Hz by pitch bend", "Built-in preview sound with reverb"]),
           ("Formats", ["VST3 (with MIDI output), Audio Unit (macOS) and standalone app"])],
    "es": [("Generador", ["8 carriles con roles: drone, pad, melodía, arpegio, nube, bajo, pulso", "Armonía lenta compartida: 16 escalas y modos, color de acorde, tensión, tres tipos de progresión, conducción de voces",
                          "Lazos por carril (2-128 pulsos) con mutación, ritmos euclidianos, probabilidad, ecos MIDI", "Número de variación: los mismos ajustes y número dan siempre la misma música"]),
           ("Simple y avanzado", [f"7 climas con {NOR} presets, 5 macros, tonalidad, escala, sesión y respiración", "29 ajustes por carril, páginas de armonía, sesión y salidas", "Captura de los últimos 1 a 10 minutos como archivo MIDI para arrastrar al DAW"]),
           ("Escucha tranquila", ["Sesiones de hasta 2 horas con fade de entrada y de salida (velocidad y CC7)", "Ritmo de respiración de 1 a 12 respiraciones por minuto", "Modo suave: más bajo, más grave, saltos chicos, poca tensión"]),
           ("Salidas", ["MIDI al DAW, un canal por carril", "Puertos MIDI directos por carril (hardware, IAC, loopMIDI) con reloj MIDI", "Vínculo líder y seguidoras entre instancias", "Curvas de controlador y deriva por pitch bend; referencia de 415 a 466 Hz por pitch bend", "Sonido de prueba incorporado con reverb"]),
           ("Formatos", ["VST3 (con salida MIDI), Audio Unit (macOS) y aplicación independiente"])],
},
"atmd": {
    "en": [("Sound engine", ["Three VCOs: LO and 32' to 2', triangle, triangle-sawtooth (osc 3: reverse sawtooth), sawtooth, square, 29 % and 15 % pulse", "Passive mixer that saturates the filter input; white/pink noise; external input with OVERLOAD lamp",
                             "Four-pole transistor ladder, 24 dB/oct, saturation in every stage, solved without delay", "Two ADS contours with the DECAY switch; glide; 1x to 8x oversampling"]),
           ("Controllers", ["Modulation mix of osc 3 and noise to the oscillators and the filter", "Osc 3 free from the keyboard as a modulator, ±3 octaves", "Pitch wheel (±7 st by default) and mod wheel, MIDI pitch bend and CC 1"]),
           ("Extensions", ["Up to 8 voices, up to 4-voice unison with detune and spread", "Velocity and pressure to filter, loudness and modulation", "Note priority low/last/high, single or multi trigger", "Tempo-synced delay and plate reverb"]),
           ("Formats", ["VST3, Audio Unit (macOS) and standalone app", "Optional stereo side input for the external input"])],
    "es": [("Motor de sonido", ["Tres VCO: LO y de 32' a 2', triángulo, triángulo-sierra (osc 3: sierra invertida), sierra, cuadrada, pulso de 29 % y 15 %", "Mezclador pasivo que satura la entrada del filtro; ruido blanco/rosa; entrada externa con lámpara OVERLOAD",
                                "Escalera de transistores de cuatro polos, 24 dB/oct, saturación en cada etapa, resuelta sin retardo", "Dos contornos ADS con el interruptor DECAY; glide; sobremuestreo de 1x a 8x"]),
           ("Controladores", ["Mezcla de modulación del osc 3 y el ruido hacia los osciladores y el filtro", "El osc 3 libre del teclado como modulador, ±3 octavas", "Rueda de pitch (±7 st por defecto) y de modulación, pitch bend y CC 1 por MIDI"]),
           ("Extensiones", ["Hasta 8 voces, unísono de hasta 4 con desafinación y apertura", "Velocidad y presión al filtro, al volumen y a la modulación", "Prioridad grave/última/aguda, disparo único o múltiple", "Delay sincronizado y reverb de placa"]),
           ("Formatos", ["VST3, Audio Unit (macOS) y aplicación independiente", "Entrada lateral estéreo opcional para la entrada externa"])],
},
"at303": {
    "en": [("Voice", ["One oscillator: band-limited saw, or square through a transistor waveshaper model", "Four-pole diode ladder with its capacitor ratio, zero-delay feedback, 9 Hz feedback high-pass, 2x, 4x or 8x oversampled",
                      "Filter envelope (200 ms to 2.5 s), volume envelope with the 4 ms note delay, accent with the RC sweep circuit and C13", "Slide: the RC glide of the control voltage, about 60 ms"]),
           ("Sequencer", ["64 patterns in 4 groups, 2 banks and 8 patterns, up to 16 steps, triplets", "The original's two lists: pitches in order (note, octave, accent, slide) and time per step (note, tie, rest)",
                          "Pitch and Time write modes on the instrument, or the editor on screen", "Host sync locked to the bar, or its own clock with Run and Tempo; MIDI mode with accent by velocity and slide by overlap"]),
           ("Extended and output", ["Sixteen modifications: overdrive, normal and accent decay, soft attack, slide time, tracking, filter FM, Env Mod range and zero, cutoff top, VCA click, sweep speed, muffler, high resonance, full bass, note delay",
                                    "Drive: soft clip, hard clip or fuzz, with tone", "Ping-pong delay from 1/16 to 1/2 with dotted values, feedback and tone", "Acid line generator: seven scales, octaves, density, accents, slides and ties; Mutate"]),
           ("Formats", ["VST3 instrument, Audio Unit (macOS) and standalone app", "at303render: command-line renderer with timed parameter changes", "No latency"])],
    "es": [("Voz", ["Un oscilador: sierra de banda limitada, o cuadrada por un modelo del transistor que le da forma", "Escalera de diodos de cuatro polos con la relación de sus capacitores, realimentación sin retardo, pasa altos de 9 Hz en la realimentación, sobremuestreo ×2, ×4 u ×8",
                    "Envolvente de filtro (de 200 ms a 2,5 s), envolvente de volumen con el retardo de nota de 4 ms, acento con el circuito RC de barrido y C13", "Slide: el deslizamiento RC de la tensión de control, unos 60 ms"]),
           ("Secuenciador", ["64 patrones en 4 grupos, 2 bancos y 8 patrones, hasta 16 pasos, tresillos", "Las dos listas del original: alturas en orden (nota, octava, acento, slide) y tiempo por paso (nota, ligadura, silencio)",
                             "Modos de escritura Pitch y Time en el instrumento, o el editor en pantalla", "Sincronía con el host enganchada al compás, o reloj propio con Run y Tempo; modo MIDI con acento por velocidad y slide por superposición"]),
           ("Extendido y salida", ["Dieciséis modificaciones: overdrive, decay normal y de acento, ataque suave, tiempo de slide, seguimiento, FM del filtro, rango y cero del Env Mod, tope del corte, golpe del VCA, velocidad del barrido, muffler, resonancia alta, graves completos, retardo de nota",
                                   "Drive: recorte suave, recorte duro o fuzz, con tono", "Delay ping-pong de 1/16 a 1/2 con puntillos, realimentación y tono", "Generador de líneas ácidas: siete escalas, octavas, densidad, acentos, slides y ligaduras; Mutate"]),
           ("Formatos", ["Instrumento VST3, Audio Unit (macOS) y aplicación independiente", "at303render: render por línea de comandos con cambios de parámetros en el tiempo", "Sin latencia"])],
},
"plumb": {
    "en": [("Latency and detection", ["Live 32 samples (0.67 ms), Tight 128 (2.7 ms), Studio 1024 (21 ms) at 48 kHz; the same milliseconds at 44.1 and 96 kHz, measured equal to the reported figure",
                                      "Difference-function detector on a 12 or 24 kHz copy every 8 samples, refined at full rate over two periods, with octave guards and an energy and steadiness gate",
                                      "Voice ranges: Low 65-520 Hz, Mid 80-880 Hz, High 175-1400 Hz, Instrument 40-2000 Hz", "Measured on synthetic vowels from 82 to 880 Hz: median error under 1.3 cents, no octave errors, lock in 11 to 23 ms"]),
           ("Correction", ["Amount, Speed (0 to 400 ms) and Style from Natural to Hard", "Note centre, drift and vibrato separated; target chosen on the centre with hysteresis",
                           "Custom character: transition, humanize, flex window, hysteresis, vibrato and drift kept", "Fourteen scales and a custom one with snap, skip and bypass per note; key detection; reference from 415 to 466 Hz"]),
           ("Pitch and formants", ["Shifting by repeating and dropping single cycles of the excitation at their quietest point, with short crossfades", "Formants from a 48th-order linear-prediction model through normalised lattice filters; Preserve, Amount and Throat (±12 semitones)",
                                   "Transpose ±24 semitones, fine ±100 cents, synthetic vibrato with rate, depth and delay", "MIDI: scale from held notes, note lock with pitch bend, auto"]),
           ("Formats", ["VST3, Audio Unit (macOS, music effect with MIDI input) and standalone app", "plumbrender: command-line renderer with timed parameter changes", "Mono in, the corrected voice on every output channel"])],
    "es": [("Latencia y detección", ["Live 32 muestras (0,67 ms), Tight 128 (2,7 ms), Studio 1024 (21 ms) a 48 kHz; los mismos milisegundos a 44,1 y 96 kHz, medidos iguales a la cifra informada",
                                     "Detector de función diferencia sobre una copia a 12 o 24 kHz cada 8 muestras, refinado a la frecuencia completa sobre dos períodos, con guardas de octava y una compuerta de energía y estabilidad",
                                     "Rangos de voz: Low 65-520 Hz, Mid 80-880 Hz, High 175-1400 Hz, Instrument 40-2000 Hz", "Medido sobre vocales sintéticas de 82 a 880 Hz: error mediano por debajo de 1,3 cents, sin errores de octava, enganche en 11 a 23 ms"]),
           ("Corrección", ["Amount, Speed (0 a 400 ms) y Style de Natural a Hard", "Centro de la nota, deriva y vibrato separados; el objetivo se elige sobre el centro, con histéresis",
                           "Carácter a mano: transición, humanize, ventana flex, histéresis, vibrato y deriva conservados", "Catorce escalas y una propia con snap, skip y bypass por nota; detección de tonalidad; referencia de 415 a 466 Hz"]),
           ("Altura y formantes", ["Corrimiento repitiendo y salteando ciclos sueltos de la excitación en su punto más silencioso, con fundidos cortos", "Formantes de un modelo de predicción lineal de orden 48 con filtros en celosía normalizados; Preserve, Amount y Throat (±12 semitonos)",
                                   "Transposición ±24 semitonos, fina ±100 cents, vibrato sintético con velocidad, profundidad y retardo", "MIDI: escala desde las notas apretadas, nota fija con pitch bend, automático"]),
           ("Formatos", ["VST3, Audio Unit (macOS, efecto musical con entrada MIDI) y aplicación independiente", "plumbrender: render por línea de comandos con cambios de parámetros en el tiempo", "Entrada mono, la voz corregida en todos los canales de salida"])],
},
"tarnish": {
    "en": [("Machines", ["Studio tape at 30, 15, 7.5 and 3.75 ips with IEC or NAB equalisation", "Cassette: ferric (120 µs), chrome and metal (70 µs), half speed", "Video linear audio, SP and EP",
                         "Vinyl at 33, 45 and 78 rpm through the RIAA curve, with inner-groove loss and tracing distortion", "AM radio: station compressor, AM envelope with overmodulation, selective fading, diode detector, static, whistle, IF filter, small speaker", "Telephone: 300-3400 Hz, 8 kHz, 8-bit mu-law; mobile with frame loss"]),
           ("Tape model", ["Jiles-Atherton hysteresis in normalised units, RK4 over the head-field path, 4x oversampled (RK2 in Eco)", "Drive -24 to +24 dB, saturation and bias (under to over), self-erasure",
                           "Playback losses with the spacing law (54.6·d/λ dB), head bump, azimuth loss and interchannel delay", "Hiss through the playback chain, modulation noise, hum, print-through from the reel geometry"]),
           ("Transport and damage", ["Wow and flutter from reel, roller, capstan and motor lines plus drift, 0 to 2 % and 0 to 1 % weighted", "Dropouts as head-to-tape spacing, with damaged stretches; wear",
                                     "Crackle, clicks and pops; up to three scratches locked to the rotation", "Tape stop 0.05 to 4 s or synced, with curve and start time; varispeed ±12 semitones"]),
           ("Formats", ["VST3, Audio Unit (macOS, music effect with MIDI input) and standalone app", "tarnishrender: command-line renderer with timed parameter changes", "Constant latency: 25 ms plus 55 samples, reported to the host"])],
    "es": [("Máquinas", ["Cinta de estudio a 30, 15, 7,5 y 3,75 ips con ecualización IEC o NAB", "Casete: ferric (120 µs), chrome y metal (70 µs), media velocidad", "Audio lineal de video, SP y EP",
                         "Vinilo a 33, 45 y 78 rpm por la curva RIAA, con pérdida del surco interior y distorsión de lectura", "Radio AM: compresor de emisora, envolvente de AM con sobremodulación, desvanecimiento selectivo, detector a diodo, estática, silbido, filtro de FI, parlante chico", "Teléfono: 300-3400 Hz, 8 kHz, ley mu de 8 bits; celular con pérdida de tramas"]),
           ("Modelo de cinta", ["Histéresis de Jiles-Atherton en unidades normalizadas, RK4 sobre el camino del campo, sobremuestreo ×4 (RK2 en Eco)", "Drive de -24 a +24 dB, saturación y bias (sub a sobre), autoborrado",
                                "Pérdidas de reproducción con la ley de separación (54,6·d/λ dB), golpe de cabeza, pérdida de azimut y retardo entre canales", "Soplido por la cadena de reproducción, ruido de modulación, zumbido, copia según la geometría del carrete"]),
           ("Transporte y daño", ["Wow y flutter de las líneas de carretes, rodillo, cabrestante y motor más deriva, de 0 a 2 % y de 0 a 1 % ponderados", "Caídas como separación cabeza-cinta, con tramos dañados; desgaste",
                                  "Crujido, clics y pops; hasta tres rayas atadas a la rotación", "Tape stop de 0,05 a 4 s o sincronizado, con curva y tiempo de arranque; varispeed ±12 semitonos"]),
           ("Formatos", ["VST3, Audio Unit (macOS, efecto musical con entrada MIDI) y aplicación independiente", "tarnishrender: render por línea de comandos con cambios de parámetros en el tiempo", "Latencia constante: 25 ms más 55 muestras, informada al host"])],
},
"halation": {
    "en": [("Space", ["Eight delay lines, Hermite interpolation, orthogonal mixing matrix from identity (echoes) to a full mix (room)", "Line lengths interpolated in log steps between the echo pattern and the room; T60 kept at any position",
                      "Three-band damping per line (250 Hz and 2.5 kHz), Tone and Bass decay; slow random modulation per line", "Length 0.2 to 60 s or endless; pre-delay up to 250 ms; low cut 20 to 800 Hz; width 0 to 150 %"]),
           ("Echoes and character", ["1 ms to 4 s, or 1/32 to 4 bars with dotted and triplet values; tape-style time glide", "Single, Dotted, Triplet, Dotted + 1/4, Rain, Gallop, Cascade, Swing; ping-pong",
                                     "Digital, Tape (wow, flutter, saturation, head bump, wear and dropouts), BBD (clock-dependent darkening), Spring (dispersive all-pass chain)", "Room, Hall, Plate and Chamber, size 25 to 400 %"]),
           ("Modules", ["Freeze: All, Lows, Mids, Highs, Lows + mids (spectral, 2048-point window)", "Shimmer: octave, fifth, octave + fifth, fourth, major, minor, octave down", "Reverse, Grains (20 to 500 ms), Spectral smear",
                        "Resonator on a root and chord or on held MIDI notes; Stutter on the beat; Breathe and Collapse; Duck; Gate with hold", "Envelope follower to Space, Size, Shimmer, Tone or Mix; scenes A/B with Morph"]),
           ("Formats", ["VST3, Audio Unit (macOS, music effect with MIDI input) and standalone app", "halationrender: command-line renderer with timed parameter changes", "Zero latency; tail length reported to the host"])],
    "es": [("Espacio", ["Ocho líneas de retardo, interpolación de Hermite, matriz de mezcla ortogonal de la identidad (ecos) a la mezcla total (sala)", "Largos de línea interpolados en pasos logarítmicos entre el patrón de ecos y la sala; T60 constante en cualquier posición",
                        "Amortiguación de tres bandas por línea (250 Hz y 2,5 kHz), Tone y Bass decay; modulación aleatoria lenta por línea", "Length de 0,2 a 60 s o infinito; pre-delay hasta 250 ms; low cut de 20 a 800 Hz; ancho de 0 a 150 %"]),
           ("Ecos y carácter", ["De 1 ms a 4 s, o de 1/32 a 4 compases con puntillo y tresillo; cambio de tiempo tipo cinta", "Single, Dotted, Triplet, Dotted + 1/4, Rain, Gallop, Cascade, Swing; ping-pong",
                                "Digital, Tape (wow, flutter, saturación, refuerzo de cabezal, desgaste y caídas), BBD (oscurecimiento según el reloj), Spring (cadena dispersiva de pasa-todos)", "Room, Hall, Plate y Chamber, tamaño de 25 a 400 %"]),
           ("Módulos", ["Freeze: All, Lows, Mids, Highs, Lows + mids (espectral, ventana de 2048 puntos)", "Shimmer: octava, quinta, octava + quinta, cuarta, mayor, menor, octava abajo", "Reverse, Grains (de 20 a 500 ms), corrimiento espectral",
                        "Resonator en una nota y un acorde o en las notas MIDI sostenidas; Stutter sobre el pulso; Breathe y Collapse; Duck; Gate con hold", "Seguidor de envolvente a Space, Size, Shimmer, Tone o Mix; escenas A/B con Morph"]),
           ("Formatos", ["VST3, Audio Unit (macOS, efecto musical con entrada MIDI) y aplicación independiente", "halationrender: render por línea de comandos con cambios de parámetros en el tiempo", "Latencia cero; largo de la cola informado al host"])],
},
"lacquer": {
    "en": [("Analysis", ["ITU-R BS.1770-5 loudness and true peak, EBU Tech 3342 loudness range, tested with the EBU Tech 3341 and 3342 signals", "Spectrum of the loud parts (short-term loudness above integrated - 3 LU), mid and side, 1/3 and 1/6 octave",
                         "Correlation overall and below 150 Hz, crest factor, transient density, harshness, sibilance, clipped runs, high-frequency cut-off"]),
           ("Processing", ["Tone EQ: low shelf, six bells, high shelf, least-squares fit; dead zone, per-band and slope limits", "Dynamic EQ: harshness 2 to 5 kHz and sibilance 5 to 10 kHz",
                           "Three-band compressor (Linkwitz-Riley crossovers, parallel form) and glue compressor with sidechain high-pass", "Mono lows, side gain and side high shelf; tape or valve saturation and soft clipper, 4x oversampled",
                           "Look-ahead true-peak limiter: 3 ms, 8x detection, dual release"]),
           ("Targets and styles", ["Streaming -14, Apple -16, Loud -11, Club -8, CD -9, Broadcast -23 LUFS, or the mix's own loudness", "Ceiling -3 to -0.1 dBTP", "Balanced, Warm, Bright, Punchy, Loud, Gentle; intensity 0 to 100 %"]),
           ("Formats", ["VST3, Audio Unit (macOS) and standalone app with file mode", "lacquerrender: command-line renderer with a JSON report", "Latency 275 samples at 48 kHz, constant"])],
    "es": [("Análisis", ["Sonoridad y pico verdadero ITU-R BS.1770-5, rango de sonoridad EBU Tech 3342, probados con las señales de EBU Tech 3341 y 3342", "Espectro de las partes fuertes (sonoridad corta por encima de la integrada - 3 LU), medio y lado, tercios y sextos de octava",
                         "Correlación general y debajo de 150 Hz, factor de cresta, densidad de transitorios, aspereza, sibilancia, tramos recortados, corte de agudos"]),
           ("Proceso", ["EQ tonal: estante grave, seis campanas, estante agudo, ajuste por mínimos cuadrados; zona muerta, topes por banda y de pendiente", "EQ dinámica: aspereza de 2 a 5 kHz y sibilancia de 5 a 10 kHz",
                        "Compresor de tres bandas (divisores Linkwitz-Riley, forma paralela) y glue con pasa altos en la detección", "Graves en mono, ganancia y estante agudo de los costados; saturación de cinta o válvula y recortador suave, sobremuestreo 4x",
                        "Limitador de pico verdadero con anticipación: 3 ms, detección 8x, liberación doble"]),
           ("Objetivos y estilos", ["Streaming -14, Apple -16, Loud -11, Club -8, CD -9, Broadcast -23 LUFS, o la sonoridad de la mezcla", "Techo de -3 a -0,1 dBTP", "Balanced, Warm, Bright, Punchy, Loud, Gentle; intensidad de 0 a 100 %"]),
           ("Formatos", ["VST3, Audio Unit (macOS) y aplicación independiente con modo archivo", "lacquerrender: render por línea de comandos con informe JSON", "Latencia de 275 muestras a 48 kHz, constante"])],
},
"murmur": {
    "en": [("Granular engine", ["Memory of 1 to 16 s, recorded continuously, with Freeze (button or sustain pedal)", "Up to 128 grains, 5 ms to 2 s, 0.5 to 200 per second or on the host grid with chance",
                                "Position, spray and scan (−2 to +2 ×), reverse grains, envelope shape and skew, stereo spread",
                                "Pitch ±24 semitones, fine tune, random spread snapped to 8 scales, shimmer and sub octaves"]),
           ("Around the grains", ["Low and high cut, feedback into the memory, ducking", "Tempo-synced stereo delay and plate reverb", "Two LFOs, five shapes, free or synced, on 9 targets"]),
           ("MIDI", ["Pitch mode: held notes set the grains' pitch", "Gate mode: the cloud plays only while keys are held", "Sustain pedal holds Freeze"]),
           ("Interface", ["Flock view: the memory as a horizon, each grain as a bird", "Simple and Advanced pages, help for every control, built-in manual"]),
           ("Formats", ["VST3 and Audio Unit effect (macOS) with MIDI input, and standalone app", "Mono or stereo input, stereo output"])],
    "es": [("Motor granular", ["Memoria de 1 a 16 s, grabando siempre, con Freeze (botón o pedal de sustain)", "Hasta 128 granos, de 5 ms a 2 s, de 0,5 a 200 por segundo o en la grilla del host con probabilidad",
                               "Posición, dispersión y scan (−2 a +2 ×), granos al revés, forma e inclinación de la envolvente, apertura estéreo",
                               "Altura ±24 semitonos, afinación fina, dispersión al azar ajustada a 8 escalas, octavas de shimmer y sub"]),
           ("Alrededor de los granos", ["Corte de graves y agudos, feedback a la memoria, ducking", "Delay estéreo sincronizado y reverb de placa", "Dos LFO, cinco formas, libres o sincronizados, sobre 9 destinos"]),
           ("MIDI", ["Modo Pitch: las notas apretadas fijan la altura de los granos", "Modo Gate: la nube suena sólo mientras hay teclas apretadas", "El pedal de sustain sostiene Freeze"]),
           ("Interfaz", ["Vista de bandada: la memoria como horizonte, cada grano como un pájaro", "Páginas Simple y Advanced, ayuda en cada control, manual incorporado"]),
           ("Formatos", ["Efecto VST3 y Audio Unit (macOS) con entrada MIDI, y aplicación independiente", "Entrada mono o estéreo, salida estéreo"])],
},
}

COMPARE_ORDER = ["at20v", "at101v", "at42", "at700", "atfs1", "atmd", "at303"]   # el orden de los valores de cada fila
COMPARE_ROWS = {
"en": [("Based on", ["Korg MS-20 (1978)", "Roland SH-101 (1982)", "Elta Music SOLAR 42 (2023)", "Roland System-700 (1976)", "Yamaha FS1R (1998)", "Moog Minimoog Model D (1970)", "The classic 1981 bass line synthesizer"]),
       ("Type", ["Semi-modular", "Monophonic with sequencer", "Ambient drone machine", "Modular with analog sequencer", "Formant and FM synthesizer", "Monophonic, three oscillators", "Bass line synthesizer with sequencer"]),
       ("Voices", ["Mono, or up to 12 with unison", "Mono, or up to 8 with unison", "8 fixed voices, 24 oscillators", "Duophonic, or up to 8", "Up to 32, every one filtered", "Mono, or up to 8 with unison", "Mono"]),
       ("Filter", ["Korg35 high-pass and low-pass", "IR3109 four-pole low-pass", "Dual Polivoks, LP or BP", "703E 24 dB OTA and 703B multimode", "Per voice: LPF 24/18/12, HPF, BPF, BEF", "Transistor ladder, 24 dB", "Diode ladder, 24 dB"]),
       ("Patch bay", ["35 jacks", "None", "62 jacks", "137 jacks and two multiples", "None; 16 operators and 88 algorithms", "None", "None; 16 circuit modifications"]),
       ("Sequencing", ["Through the patch bay", "Arpeggiator and 100-step sequencer", "5-step sequencer, arpeggiator, 16-step keys", "717A, 3 × 12 or 36 steps", "Formant Sequences, from any recording", "None; osc 3 as modulator, glide", "64 patterns of up to 16 steps, acid line generator"]),
       ("Effects", ["Vocoder, delay, reverb", "Drive, chorus, delay, reverb", "13 cartridges × 3 programs, distortion", "Spring reverb, phase shifter", "Insertion, variation, reverb, EQ", "Delay, reverb", "Drive, ping-pong delay"]),
       ("Audio input", ["Vocoder and ESP", "None", "Mixer channel and preamp", "None", "WAV analysis into Fseqs", "External input to the mixer", "None"]),
       ("Presets", ["51", str(N101), str(N42), str(N700), str(NFS1), str(NMD), str(len([f for f in os.listdir(os.path.join(ROOT, "plugins", "at303", "assets", "presets")) if f.endswith(".json")]))]),
       ("Typical CPU, Ryzen 9 / M4", ["3.6 % / 1.6 % of a core", "1.8 % / 1.0 % of a core", "7.5 % / 2.6 % of a core", "3.5 % / 1.7 % of a core", f"{WINCPU_FS1[1]:.1f} % / 6.8 % of a core", f"{WINCPU_MD[0]:.1f} % / 0.9 % of a core", f"{WINCPU_A3[0]:.1f} % / {CPU_A3[0]:.1f} % of a core"])],
"es": [("Basado en", ["Korg MS-20 (1978)", "Roland SH-101 (1982)", "Elta Music SOLAR 42 (2023)", "Roland System-700 (1976)", "Yamaha FS1R (1998)", "Moog Minimoog Model D (1970)", "El clásico sintetizador de bajo de 1981"]),
       ("Tipo", ["Semimodular", "Monofónico con secuenciador", "Máquina de drones ambient", "Modular con secuenciador analógico", "Sintetizador de formantes y FM", "Monofónico, tres osciladores", "Sintetizador de bajo con secuenciador"]),
       ("Voces", ["Mono, o hasta 12 con unísono", "Mono, o hasta 8 con unísono", "8 voces fijas, 24 osciladores", "Duofónico, o hasta 8", "Hasta 32, todas con filtro", "Mono, o hasta 8 con unísono", "Mono"]),
       ("Filtro", ["Korg35 pasa altos y pasa bajos", "IR3109 pasa bajos de cuatro polos", "Doble Polivoks, LP o BP", "703E OTA de 24 dB y 703B multimodo", "Por voz: LPF 24/18/12, HPF, BPF, BEF", "Escalera de transistores, 24 dB", "Escalera de diodos, 24 dB"]),
       ("Patchera", ["35 jacks", "No tiene", "62 jacks", "137 jacks y dos múltiples", "No tiene; 16 operadores y 88 algoritmos", "No tiene", "No tiene; 16 modificaciones del circuito"]),
       ("Secuencias", ["Desde la patchera", "Arpegiador y secuenciador de 100 pasos", "Secuenciador de 5 pasos, arpegiador, teclado de 16 pasos", "717A, 3 × 12 o 36 pasos", "Formant Sequences, desde cualquier grabación", "No tiene; osc 3 como modulador, glide", "64 patrones de hasta 16 pasos, generador de líneas ácidas"]),
       ("Efectos", ["Vocoder, delay, reverb", "Drive, chorus, delay, reverb", "13 cartuchos × 3 programas, distorsión", "Reverb de resorte, phase shifter", "Inserción, variación, reverb, EQ", "Delay, reverb", "Drive, delay ping-pong"]),
       ("Entrada de audio", ["Vocoder y ESP", "No tiene", "Canal del mezclador y preamp", "No tiene", "Análisis de WAV a Fseqs", "Entrada externa al mezclador", "No tiene"]),
       ("Presets", ["51", str(N101), str(N42), str(N700), str(NFS1), str(NMD), str(len([f for f in os.listdir(os.path.join(ROOT, "plugins", "at303", "assets", "presets")) if f.endswith(".json")]))]),
       ("CPU típica, Ryzen 9 / M4", ["3,6 % / 1,6 % de un núcleo", "1,8 % / 1,0 % de un núcleo", "7,5 % / 2,6 % de un núcleo", "3,5 % / 1,7 % de un núcleo", f"{WINCPU_FS1[1]:.1f} % / 6,8 % de un núcleo".replace(".", ","), f"{WINCPU_MD[0]:.1f} % / 0,9 % de un núcleo".replace(".", ",", 1), f"{WINCPU_A3[0]:.1f} % / {CPU_A3[0]:.1f} % de un núcleo".replace(".", ",")])],
}

# ───────────────────────── plantillas ─────────────────────────
def pic(base, name, alt, sizes="100vw", eager=False, cls=""):
    """Imagen con variantes de 1200 y 2400 px (webp + jpg), o de un solo tamaño si existe nombre.jpg."""
    load = 'fetchpriority="high"' if eager else 'loading="lazy" decoding="async"'
    c = f' class="{cls}"' if cls else ""
    if os.path.exists(os.path.join(WEB, "assets", "img", name + ".jpg")):
        return (f'<picture{c}><source type="image/webp" srcset="{base}img/{name}.webp">'
                f'<img src="{base}img/{name}.jpg" alt="{html.escape(alt)}" {load}></picture>')
    return (f'<picture{c}>'
            f'<source type="image/webp" srcset="{base}img/{name}-1200.webp 1200w, {base}img/{name}-2400.webp 2400w" sizes="{sizes}">'
            f'<img src="{base}img/{name}-1200.jpg" srcset="{base}img/{name}-1200.jpg 1200w, {base}img/{name}-2400.jpg 2400w" sizes="{sizes}" alt="{html.escape(alt)}" {load}></picture>')

PRODUCTS = [  # slug, nombre, año, mac, win, versión
    ("at20v", "AT20V", "1978", MAC, WIN, VERSION),
    ("at101v", "AT101V", "1982", MAC101, WIN101, V101),
    ("at42", "AT42", "2023", MAC42, WIN42, V42),
    ("at700", "AT700", "1976", MAC700, WIN700, V700),
    ("atfs1", "ATFS1", "1998", MACFS1, WINFS1, VFS1),
    ("atmd", "ATMD", "1970", MACMD, WINMD, VMD),
    ("orrery", "Orrery", "2026", MACOR, WINOR, VOR),
    ("murmur", "Murmur", "2026", MACMM, WINMM, VMM),
    ("lacquer", "Lacquer", "2026", MACLQ, WINLQ, VLQ),
    ("halation", "Halation", "2026", MACHA, WINHA, VHA),
    ("tarnish", "Tarnish", "2026", MACTN, WINTN, VTN),
    ("at303", "AT303", "1981", MACA3, WINA3, VA3),
    ("plumb", "Plumb", "2026", MACPL, WINPL, VPL),
]
# Un producto entra a la web cuando sus dos zips están en web/downloads: así se puede publicar uno mientras el
# siguiente sigue en preparación con su texto ya escrito.
PRODUCTS = [p for p in PRODUCTS if all(os.path.exists(os.path.join(WEB, "downloads", z)) for z in (p[3], p[4]))]
RELEASED = {p[0] for p in PRODUCTS}
# Menú de productos (encabezado): grupos y color de cada uno. Las miniaturas son web/assets/img/nav/<slug>.jpg.
NAV_GROUPS = [(g, [x for x in sl if x in RELEASED]) for g, sl in
              [("synths", ["at20v", "at101v", "at42", "at700", "atfs1", "atmd", "at303"]), ("tools", ["orrery", "murmur", "halation", "tarnish", "plumb", "lacquer"])]]
# El mismo orden en el carrusel de la portada, las fichas y el launcher: sintetizadores a la izquierda,
# herramientas y efectos a la derecha (pedido del usuario, 2026-10-19).
_ORDER = [s for _, sl in NAV_GROUPS for s in sl]
PRODUCTS.sort (key=lambda p: _ORDER.index (p[0]) if p[0] in _ORDER else len (_ORDER))
NAV_LABELS = {"en": {"synths": "Synthesizers", "tools": "Tools and effects", "button": "Plugins", "all": "All plugins", "close": "Close menu"},
              "es": {"synths": "Sintetizadores", "tools": "Herramientas y efectos", "button": "Plugins", "all": "Todos los plugins", "close": "Cerrar el menú"}}
# La imagen grande de cada página de producto (web/assets/img/<nombre>-1200.jpg); el launcher la usa en su ficha.
HERO_IMG = {"at20v": "render-hero", "at303": "at303/hero", "at101v": "at101v/hero-grey", "at42": "at42/hero-blue", "at700": "at700/hero", "atfs1": "atfs1/hero", "orrery": "orrery/hero", "murmur": "murmur/hero", "atmd": "atmd/hero", "lacquer": "lacquer/hero", "halation": "halation/hero", "tarnish": "tarnish/hero", "plumb": "plumb/hero"}
ACCENT = {"at20v": "#d24a35", "at101v": "#3a7bd5", "at42": "#ea8c3f", "at700": "#e0b341", "atfs1": "#a9c83c", "orrery": "#f2c878",
          "murmur": "#f3a27e", "atmd": "#e8743b", "lacquer": "#e0563f", "halation": "#b8a5ff", "tarnish": "#3e8c84", "at303": "#f2a03d", "plumb": "#9db4c8"}
TEXTS = lambda slug, lang: {"at20v": T, "at101v": S, "at42": Q, "at700": Z, "atfs1": F, "orrery": O, "murmur": M, "atmd": D, "lacquer": LQ, "halation": HA, "tarnish": TN, "at303": A3, "plumb": PL}[slug][lang]

# Visitas y descargas: Pulse (analytics.digitalhub.com.ar, la analítica propia), sitio "Audio Tools". Sólo carga en el
# dominio real, así las pruebas locales no cuentan. Las descargas llegan como eventos (ver site.js); el panel es privado.
PULSE_SRC = "https://analytics.digitalhub.com.ar/t/4sfku5fquj4c.js"

# Antes de cada descarga, un diálogo que pide colaborar con tokens (los plugins se hacen con tokens de IA) por Cafecito.
# La descarga sigue con un botón; sin JavaScript los enlaces descargan directo. El botón de Cafecito también va en el pie.
SUPPORT_COFFEE_URL = "https://cafecito.app/palmereta"   # vacía, el botón no se muestra
CAFECITO_IMG = "https://cdn.cafecito.app/imgs/buttons/button_5"
SUPPORT = {
    "en": dict(h="Your download is ready.",
               p="Audio Tools is free and stays free. It is built with AI tokens: if it ends up in your music, chip in for the next ones on Cafecito.",
               coffee="Chip in for tokens on cafecito.app", foot="Like Audio Tools? Chip in for tokens:",
               go="Continue to download", close="Close"),
    "es": dict(h="Tu descarga está lista.",
               p="Audio Tools es gratis y lo va a seguir siendo. Se hace con tokens de IA: si termina en tu música, colaborá con los próximos en Cafecito.",
               coffee="Invitame con unos tokens en cafecito.app", foot="¿Te gusta Audio Tools? Colaborá con unos tokens:",
               go="Seguir con la descarga", close="Cerrar"),
}

def cafecito_button(lang, cls="cafecito"):
    """El botón oficial de Cafecito (imagen de su CDN en 1x, 2x y 3,75x), con el texto alternativo en el idioma de la página."""
    if not SUPPORT_COFFEE_URL: return ""
    i = CAFECITO_IMG
    return (f'<a class="{cls}" href="{html.escape(SUPPORT_COFFEE_URL)}" rel="noopener" target="_blank">'
            f'<img srcset="{i}.png 1x, {i}_2x.png 2x, {i}_3.75x.png 3.75x" src="{i}.png" alt="{html.escape(SUPPORT[lang]["coffee"])}" height="40" loading="lazy"></a>')

def support_dialog(lang):
    u = SUPPORT[lang]
    return f"""  <dialog class="support-modal" aria-labelledby="support-h">
    <button class="support-x" type="button" aria-label="{u['close']}" data-support-close>×</button>
    <h2 id="support-h">{u['h']}</h2>
    <p>{u['p']}</p>
    <div class="support-actions">{cafecito_button(lang)}<a class="pill primary" href="#" data-support-go>{u['go']}</a></div>
    <p class="support-file" data-support-file></p>
  </dialog>
"""


# ───────────────────────── launcher y manifest ─────────────────────────
# El launcher (tools/launcher) lee web/downloads/manifest.json: los productos publicados (RELEASED), con url, sha256 y
# tamaño de cada zip, y su propia versión para avisar que hay una nueva. La página del launcher y sus enlaces aparecen
# cuando sus dos zips están en web/downloads, igual que un producto.
SITE_URL = "https://audiotools.com.ar/"
VLA = re.search(r"set\(LAUNCHER_VERSION (.+?)\)", open(os.path.join(ROOT, "tools", "launcher", "CMakeLists.txt")).read()).group(1)
MACLA = f"AudioToolsLauncher-{VLA}-macOS.zip"
WINLA = f"AudioToolsLauncher-{VLA}-Windows.zip"
LINLA = f"AudioToolsLauncher-{VLA}-Linux.zip"     # opcional: la página y el manifest lo ofrecen si está
LAUNCHER_RELEASED = all(os.path.exists(os.path.join(WEB, "downloads", z)) for z in (MACLA, WINLA))
MANIFEST = os.path.join(WEB, "downloads", "manifest.json")
LFS_HEAD = b"version https://git-lfs.github.com/spec/v1"
FORMAT_OF = {".vst3": "VST3", ".component": "AU", ".lv2": "LV2", ".app": "Standalone", ".exe": "Standalone"}

def format_of(f, system):
    """El formato de un ítem del zip. En Linux la standalone es el ejecutable sin extensión; las herramientas de línea
    de comandos (lacquerrender...) también lo son y no cuentan."""
    ext = os.path.splitext(f)[1]
    if system == "linux" and "." not in f and not f.endswith("render"):
        return "Standalone"
    return FORMAT_OF.get(ext)

def zip_asset(name, product, system, previous):
    """url, sha256, tamaño y contenido de un zip de web/downloads; None si no está. system: macos, windows o linux."""
    p = os.path.join(WEB, "downloads", name)
    if not os.path.exists(p):
        return None
    with open(p, "rb") as f:
        head = f.read(len(LFS_HEAD))
    if head == LFS_HEAD:
        # Puntero de Git LFS (un clon sin los binarios, como el de Ploi antes de lfs-fetch.py): el oid de LFS es el
        # sha256 del archivo real y "size" su tamaño, así el manifest sale igual. El contenido no se puede listar:
        # se toma del manifest anterior si el zip es el mismo, o se supone el de siempre.
        ptr = open(p).read()
        sha = re.search(r"oid sha256:([0-9a-f]{64})", ptr).group(1)
        size = int(re.search(r"^size (\d+)", ptr, re.M).group(1))
        old = previous.get(name)
        files = old["files"] if old and old.get("sha256") == sha else {
            "macos": [f"{product}.vst3", f"{product}.component", f"{product}.app"],
            "windows": [f"{product}.vst3", f"{product}.exe"],
            "linux": [f"{product}.vst3", f"{product}.lv2", product]}[system]
    else:
        h = hashlib.sha256()
        with open(p, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
        sha, size = h.hexdigest(), os.path.getsize(p)
        # Los zips guardan una carpeta <Producto>-<versión>-<SO>/ con los bundles adentro.
        with zipfile.ZipFile(p) as z:
            files = sorted({n.split("/")[1] for n in z.namelist() if n.count("/") >= 1 and n.split("/")[1]})
    files = sorted(f for f in files if format_of(f, system))
    formats = [x for x in ("VST3", "AU", "LV2", "Standalone") if any(format_of(f, system) == x for f in files)]
    return {"url": SITE_URL + "downloads/" + name, "sha256": sha, "size": size, "files": files, "formats": formats}

def asset_url(rel):
    """URL de un archivo de web/ con ?v=<hash del contenido>: el launcher guarda en caché por URL, así un archivo
    que cambia con el mismo nombre se vuelve a bajar."""
    p = os.path.join(WEB, rel)
    if not os.path.exists(p): return None
    return SITE_URL + rel + "?v=" + hashlib.sha256(open(p, "rb").read()).hexdigest()[:12]

def img_rel(name):
    """Como pic(): la variante de 1200 px, o name.jpg si la imagen es de un solo tamaño."""
    for rel in (f"assets/img/{name}-1200.jpg", f"assets/img/{name}.jpg"):
        if os.path.exists(os.path.join(WEB, rel)): return rel
    return None

def launcher_samples(slug, previous):
    """Las demos de la página del producto para el launcher. Van en Opus (web/assets/samples/<slug>/<id>.opus, 96 kb/s)
    porque JUCE lee Opus igual en macOS y Windows, y AAC (.m4a) sólo a través del sistema: en Windows no es confiable.
    Se codifican con ffmpeg cuando falta el .opus o cambió el .m4a (su hash va en "source")."""
    meta = json.load(open(os.path.join(WEB, "assets", "samples", "samples.json")))
    out = []
    for key in meta["_order"]:
        sm = meta.get(key)
        if not sm or sm["plugin"] != slug: continue
        m4a = os.path.join(WEB, "assets", "samples", key + ".m4a")
        opus_rel = f"assets/samples/{key}.opus"
        opus = os.path.join(WEB, opus_rel)
        if not os.path.exists(m4a): continue
        src = hashlib.sha256(open(m4a, "rb").read()).hexdigest()[:12]
        stale = not os.path.exists(opus) or previous.get(key, {}).get("source") not in (None, src)
        if stale and shutil.which("ffmpeg"):
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", m4a, "-map_metadata", "-1", "-c:a", "libopus", "-b:a", "96k",
                            "-fflags", "+bitexact", "-flags:a", "+bitexact", opus], check=True)
        if not os.path.exists(opus): continue
        out.append({"id": sm["id"], "title": sm["title"]["en"], "description": sm["desc"]["en"], "seconds": sm["secs"],
                    "url": asset_url(opus_rel), "source": src, "peaks": [round(v, 3) for v in sm["peaks"]]})
    return out

def write_manifest():
    """web/downloads/manifest.json. Sin marcas de tiempo: "generated" sólo cambia cuando cambia el contenido."""
    previous, old_generated = {}, None
    if os.path.exists(MANIFEST):
        try:
            old = json.load(open(MANIFEST))
            old_generated = old.get("generated")
            for pr in old.get("products", []) + [old.get("launcher") or {}]:
                for k in ("macos", "windows", "linux"):
                    a = pr.get(k)
                    if a: previous[a["url"].rsplit("/", 1)[-1]] = a
                for sm in pr.get("samples", []):
                    previous[pr["id"] + "/" + sm["id"]] = sm
        except (ValueError, KeyError):
            pass
    cat = {sl: g for g, sls in NAV_GROUPS for sl in sls}
    order = [sl for _, sls in NAV_GROUPS for sl in sls]
    names = {s: n for s, n, *_ in PRODUCTS}
    vers = {s: v for s, *_, v in PRODUCTS}
    products = []
    cards = {c["slug"]: c for c in H["en"]["cards"]}
    for sl in order:
        if sl not in names: continue
        n, v = names[sl], vers[sl]
        card_img = img_rel(cards[sl]["imgdir"] + cards[sl]["img"])
        hero_img = img_rel(HERO_IMG[sl])
        products.append({
            "id": sl, "name": n, "version": v, "category": cat[sl],
            "tagline": PRES[sl]["en"]["tag"], "description": cards[sl]["body"], "accent": ACCENT[sl],
            "images": {"thumb": asset_url(f"assets/img/nav/{sl}.jpg"),
                       "card": asset_url(card_img) if card_img else None,
                       "hero": asset_url(hero_img) if hero_img else None},
            "page": SITE_URL + sl + "/",
            "manual": SITE_URL + sl + "/manual/" if manual_web.has_manual(sl) else None,
            "samples": launcher_samples(sl, previous),
            "macos": zip_asset(f"{n}-{v}-macOS.zip", n, "macos", previous),
            "windows": zip_asset(f"{n}-{v}-Windows.zip", n, "windows", previous),
            "linux": zip_asset(f"{n}-{v}-Linux.zip", n, "linux", previous),
        })
    data = {
        "schema": 1, "generated": None, "site": SITE_URL, "donate_url": SUPPORT_COFFEE_URL,
        "home": {"title": H["en"]["h1"], "lead": H["en"]["lead"]},
        "launcher": {"version": VLA, "page": SITE_URL + "launcher/",
                     "macos": zip_asset(MACLA, "Audio Tools Launcher", "macos", previous),
                     "windows": zip_asset(WINLA, "Audio Tools Launcher", "windows", previous),
                     "linux": zip_asset(LINLA, "Audio Tools Launcher", "linux", previous)},
        "products": products,
    }
    old_body = None
    if old_generated is not None:
        try:
            o = json.load(open(MANIFEST)); o["generated"] = None; old_body = o
        except ValueError:
            pass
    data["generated"] = old_generated if old_body == data else datetime.date.today().isoformat()
    os.makedirs(os.path.dirname(MANIFEST), exist_ok=True)
    text = json.dumps(data, indent=1, ensure_ascii=False) + "\n"
    if not os.path.exists(MANIFEST) or open(MANIFEST).read() != text:
        open(MANIFEST, "w").write(text)
    return data

LA = {
"en": dict(
    title="Audio Tools Launcher — install and update every plugin | Audio Tools",
    description="A small free app for macOS, Windows and Linux that downloads the Audio Tools plugins and keeps them up to date. No account, no sign-up.",
    name="Launcher", tag="Every plugin, always up to date.",
    price="Free. No account. For macOS, Windows and Linux.",
    dl_h="Get the launcher.",
    dl_p="Open it, pick the plugins you want and press Install. Next time you open it, it offers the updates.",
    mac_req="macOS 11 or later, Apple Silicon or Intel.", win_req="Windows 10 or 11, 64-bit.",
    mac_steps=["Unzip the file and move <code>Audio Tools Launcher.app</code> to Applications.",
               "The first time, right-click the app and choose Open: it is free software and not notarized by Apple.",
               "Pick the plugins and press Install. They go to <code>~/Library/Audio/Plug-Ins</code>."],
    win_steps=["Unzip the file and run <code>Audio Tools Launcher.exe</code>.",
               "Windows SmartScreen may ask the first time: More info, then Run anyway.",
               "Pick the plugins and press Install. VST3 goes to your user VST3 folder, or to <code>C:\\Program Files\\Common Files\\VST3</code> if you choose all users in Settings."],
    how_h="What it does.",
    how=[("Always the latest.", "Every time it opens, it reads the list of releases from this site and shows what can be updated, one plugin or all at once."),
         ("Every plugin, up close.", "A page for each one with its pictures, the manual and the same demos as this site, to hear it before installing."),
         ("No account.", "Nothing to sign up for and nothing sent about you. It reads this site: the list of plugins, their pictures and demos, and the plugins you pick."),
         ("Checked before it installs.", "Each download is compared with its checksum before anything is copied, so a broken download never replaces a working plugin."),
         ("Your formats, the right folders.", "VST3, Audio Unit and the standalone app, each where your DAW and your system look for it. Turn off the ones you don't use."),
         ("Plays well with manual installs.", "Plugins you installed by hand are found and shown with their version, so you can update them from here too.")],
    manual_h="Prefer to install by hand?",
    manual_p="Nothing changes: every plugin page still has its own macOS, Windows and Linux downloads with the install steps.",
    manual_link="See all the plugins",
    note="Rescan plugins in your DAW after installing. Logic and GarageBand pick up new Audio Units after a restart.",
    nav="Launcher", promo="Or install and update every plugin with the", promo_link="Audio Tools Launcher"),
"es": dict(
    title="Audio Tools Launcher — instalá y actualizá todos los plugins | Audio Tools",
    description="Una app chica y gratis para macOS, Windows y Linux que descarga los plugins de Audio Tools y los mantiene al día. Sin cuenta ni registro.",
    name="Launcher", tag="Todos los plugins, siempre al día.",
    price="Gratis. Sin cuenta. Para macOS, Windows y Linux.",
    dl_h="Bajá el launcher.",
    dl_p="Lo abrís, elegís los plugins que querés y tocás Instalar. La próxima vez que lo abras, te ofrece las actualizaciones.",
    mac_req="macOS 11 o posterior, Apple Silicon o Intel.", win_req="Windows 10 u 11, 64 bits.",
    mac_steps=["Descomprimí el archivo y mové <code>Audio Tools Launcher.app</code> a Aplicaciones.",
               "La primera vez, clic derecho sobre la app y Abrir: es software libre y Apple no lo notarizó.",
               "Elegí los plugins y tocá Instalar. Van a <code>~/Library/Audio/Plug-Ins</code>."],
    win_steps=["Descomprimí el archivo y abrí <code>Audio Tools Launcher.exe</code>.",
               "La primera vez Windows SmartScreen puede preguntar: Más información y después Ejecutar de todas formas.",
               "Elegí los plugins y tocá Instalar. El VST3 va a tu carpeta VST3 de usuario, o a <code>C:\\Program Files\\Common Files\\VST3</code> si en Ajustes elegís todos los usuarios."],
    how_h="Qué hace.",
    how=[("Siempre la última versión.", "Cada vez que se abre, lee la lista de versiones de este sitio y muestra qué se puede actualizar, de a un plugin o todos juntos."),
         ("Cada plugin, de cerca.", "Una página por plugin con sus imágenes, el manual y las mismas demos de este sitio, para escucharlo antes de instalar."),
         ("Sin cuenta.", "No hay registro y no manda nada sobre vos. Lee este sitio: la lista de plugins, sus imágenes y demos, y los plugins que elegís."),
         ("Verifica antes de instalar.", "Cada descarga se compara con su checksum antes de copiar nada, así una descarga rota nunca reemplaza un plugin que anda."),
         ("Tus formatos, en su lugar.", "VST3, Audio Unit y la app independiente, cada uno donde lo buscan tu DAW y tu sistema. Apagá los que no uses."),
         ("Convive con lo instalado a mano.", "Los plugins que instalaste a mano aparecen con su versión, así también los actualizás desde acá.")],
    manual_h="¿Preferís instalar a mano?",
    manual_p="No cambia nada: cada página de plugin sigue teniendo sus descargas para macOS, Windows y Linux con los pasos de instalación.",
    manual_link="Ver todos los plugins",
    note="Reescaneá los plugins en tu DAW después de instalar. Logic y GarageBand ven los Audio Units nuevos después de reiniciarlos.",
    nav="Launcher", promo="O instalá y actualizá todos los plugins con el", promo_link="Audio Tools Launcher"),
}

def page_launcher(lang):
    t, ui = LA[lang], UI[lang]
    root = "../" if lang == "en" else "../../"
    a = root + "assets/"
    home = root + ("" if lang == "en" else "es/")
    steps = lambda lst: "".join(f"<li>{s}</li>" for s in lst)
    how = "\n".join(f'      <div class="reveal"><h3>{k}</h3><p>{v}</p></div>' for k, v in t["how"])
    # Linux, si su zip está en web/downloads: un botón más y una tarjeta a todo el ancho.
    L, has_lin = LINUX[lang], os.path.exists(os.path.join(WEB, "downloads", LINLA))
    lin_cta = f'<a class="pill ghost" href="{root}downloads/{LINLA}" download data-support>{L["dl"]}</a>' if has_lin else ""
    lin_card = (f"""
      <article class="dl-card reveal">
        <h3>{L['h']}</h3>
        <p class="req">{L['la_req']} {size_mb(LINLA)}.</p>
        <a class="pill primary" href="{root}downloads/{LINLA}" download data-support>{L['dl']}</a>
        <details><summary>{ui['install']}</summary><ol>{steps(L['la_steps'])}</ol></details>
      </article>""" if has_lin else "")
    shot = (f'\n    <div class="p-hero-img launcher-shot">{pic(a, "launcher/launcher", "Audio Tools Launcher", eager=True)}</div>'
            if os.path.exists(os.path.join(WEB, "assets", "img", "launcher", "launcher.jpg")) else "")
    body = f"""  <main>
  <section class="p-hero">
    <div class="wrap">
      <h1 class="p-name">{t['name']}</h1>
      <p class="p-tag">{t['tag']}</p>
      <p class="p-price">{t['price']}</p>
      <div class="p-cta"><a class="pill primary" href="{root}downloads/{MACLA}" download data-support>{ui['dl_mac']}</a><a class="pill ghost" href="{root}downloads/{WINLA}" download data-support>{ui['dl_win']}</a>{lin_cta}</div>
    </div>{shot}
  </section>
  <section class="sec" id="how">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['how_h']}</h2>
      <div class="reasons">
{how}
      </div>
    </div>
  </section>
  <section class="sec deep" id="download">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['dl_h']}</h2>
      <p class="lede reveal">{t['dl_p']}</p>
    <div class="dl-cards">
      <article class="dl-card reveal">
        <h3>macOS</h3>
        <p class="req">{t['mac_req']} {size_mb(MACLA)}.</p>
        <a class="pill primary" href="{root}downloads/{MACLA}" download data-support>{ui['dl_mac']}</a>
        <details><summary>{ui['install']}</summary><ol>{steps(t['mac_steps'])}</ol></details>
      </article>
      <article class="dl-card reveal">
        <h3>Windows</h3>
        <p class="req">{t['win_req']} {size_mb(WINLA)}.</p>
        <a class="pill primary" href="{root}downloads/{WINLA}" download data-support>{ui['dl_win']}</a>
        <details><summary>{ui['install']}</summary><ol>{steps(t['win_steps'])}</ol></details>
      </article>{lin_card}
    </div>
    <p class="note">{t['note']} Launcher {VLA}.</p>
      <div class="license reveal"><h3>{t['manual_h']}</h3><p>{t['manual_p']}</p><p><a class="more" href="{home}">{t['manual_link']}</a></p></div>
    </div>
  </section>
  </main>"""
    return shell(lang, "launcher/", t["title"], t["description"], body, "og-image.jpg", "", "p-launcher")

def launcher_promo(lang, home):
    """Debajo de las descargas manuales de cada plugin, que quedan como estaban."""
    if not LAUNCHER_RELEASED: return ""
    t = LA[lang]
    return f'<p class="dl-launcher">{t["promo"]} <a class="more" href="{home}launcher/">{t["promo_link"]}</a></p>'

def shell(lang, path, title, description, body, og_image, footer_extra, body_class=""):
    depth = path.count("/") + (1 if lang == "es" else 0)
    root = "../" * depth
    a = root + "assets/"
    en_href, es_href = root + path, root + "es/" + path
    here, there = (en_href, es_href) if lang == "en" else (es_href, en_href)
    home = root + ("" if lang == "en" else "es/")
    other = "es" if lang == "en" else "en"
    names = {s: n for s, n, *_ in PRODUCTS}
    nl = NAV_LABELS[lang]
    groups = []
    for g, slugs in NAV_GROUPS:
        items = "".join(
            f'<li><a class="gm-item" href="{home}{sl}/" style="--accent:{ACCENT[sl]}"{" aria-current=" + chr(34) + "page" + chr(34) if path.startswith(sl + "/") else ""}>'
            f'<picture><source type="image/webp" srcset="{a}img/nav/{sl}.webp"><img src="{a}img/nav/{sl}.jpg" alt="" loading="lazy" decoding="async" width="240" height="150"></picture>'
            f'<span class="gm-name">{names[sl]}</span><span class="gm-tag">{html.escape(PRES[sl][lang]["tag"])}</span></a></li>'
            for sl in slugs if sl in names)
        groups.append(f'<div class="gm-group"><h2>{nl[g]}</h2><ul>{items}</ul></div>')
    menu = (f'<button class="gn-menu-btn" type="button" aria-expanded="false" aria-controls="gn-menu">{nl["button"]}<span class="chev" aria-hidden="true"></span></button>'
            f'<div class="gn-menu" id="gn-menu" hidden><div class="wrap">{"".join(groups)}</div></div>')
    if LAUNCHER_RELEASED:
        menu += f'<a href="{home}launcher/"{" aria-current=" + chr(34) + "page" + chr(34) if path == "launcher/" else ""}>{LA[lang]["nav"]}</a>'
    return f"""<!doctype html>
<html lang="{lang}">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{title}</title>
  <meta name="description" content="{html.escape(description)}">
  <link rel="alternate" hreflang="en" href="{en_href or './'}">
  <link rel="alternate" hreflang="es" href="{es_href}">
  <link rel="alternate" hreflang="x-default" href="{en_href or './'}">
  <meta property="og:title" content="{html.escape(title)}">
  <meta property="og:description" content="{html.escape(description)}">
  <meta property="og:image" content="{a}img/{og_image}">
  <meta property="og:type" content="website">
  <meta name="twitter:card" content="summary_large_image">
  <link rel="icon" href="{a}favicon.svg" type="image/svg+xml">
  <link rel="preconnect" href="https://fonts.googleapis.com">
  <link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
  <link href="https://fonts.googleapis.com/css2?family=Archivo:wdth,wght@62..125,300..700&display=swap" rel="stylesheet">
  <link rel="stylesheet" href="{a}site.css">
  <script>document.documentElement.classList.add("js")</script>
  <script src="{a}site.js" defer></script>
  <script>if (location.hostname === "audiotools.com.ar") {{ var p = document.createElement("script"); p.defer = true; p.src = "{PULSE_SRC}"; document.head.appendChild(p); }}</script>
</head>
<body class="{body_class}">
  <header class="globalnav">
    <div class="wrap">
      <a class="wordmark" href="{home or './'}">Audio Tools</a>
      <nav class="gn-links" aria-label="{'Plugins'}">{menu}</nav>
      <nav class="lang" aria-label="{'Language' if lang == 'en' else 'Idioma'}">
        <a href="{here or './'}" aria-current="true" lang="{lang}">{"English" if lang == "en" else "Español"}</a>
        <a href="{there or './'}" lang="{other}" hreflang="{other}">{"Español" if lang == "en" else "English"}</a>
      </nav>
    </div>
  </header>
{body}
  <footer>
    <div class="wrap">
      <div class="row"><span>Audio Tools</span><span>{"Free software under the GNU AGPL v3." if lang == "en" else "Software libre bajo la GNU AGPL v3."} <a href="https://github.com/palmereta/audio_tools">{"Source code on GitHub" if lang == "en" else "Código fuente en GitHub"}</a></span></div>
{footer_extra}
      <div class="row support-row"><span>{SUPPORT[lang]['foot']}</span>{cafecito_button(lang)}</div>
    </div>
  </footer>
{support_dialog(lang)}</body>
</html>
"""

def facts_html(items):
    return "\n".join(f"          <li><strong>{k}</strong> {v}</li>" for k, v in items)

def presets_html(items):
    return "\n".join(f'      <div class="reveal"><div class="count">{n}</div><h3>{k}</h3><p>{v}</p></div>' for n, k, v in items)

def player_html(a, slug, cap, aria):
    return f"""<video data-player preload="metadata" playsinline poster="{a}img/{slug}/film-poster.jpg" aria-label="{html.escape(aria)}">
          <source src="{a}video/{slug}.mp4" type="video/mp4">
        </video>"""

# Demos de Orrery: la pantalla del plugin grabada con ORRERY_RECORD (ver plugins/orrery/README.md) y el
# audio del clip del mismo preset.
ORRERY_DEMOS = ["tape-loops", "undertow", "breath-six", "orbiting"]

def demos_html(a, t, lang):
    import json
    meta = json.load(open(os.path.join(WEB, "assets", "samples", "samples.json")))
    items = [(d, html.escape(meta[f"orrery/{d}"]["title"][lang]), html.escape(meta[f"orrery/{d}"]["desc"][lang])) for d in ORRERY_DEMOS]
    tabs = "".join(f'<button class="demo-tab" type="button" role="tab" aria-selected="{"true" if i == 0 else "false"}" data-src="{a}video/orrery-demo-{d}.mp4" '
                   f'data-poster="{a}img/orrery/demo/{d}.jpg" data-cap="{ti}. {de}">{ti}</button>' for i, (d, ti, de) in enumerate(items))
    d0, t0, de0 = items[0]
    return f"""  <section class="sec" id="demos">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['demo_h']}</h2>
      <p class="lede reveal">{t['demo_p']}</p>
      <div class="demo-tabs reveal" role="tablist" aria-label="{t['demo_tabs']}">{tabs}</div>
      <figure class="film-frame demo-frame reveal" data-demo>
        <video data-player preload="metadata" playsinline poster="{a}img/orrery/demo/{d0}.jpg" aria-label="{t0}">
          <source src="{a}video/orrery-demo-{d0}.mp4" type="video/mp4">
        </video>
        <figcaption data-demo-cap>{t0}. {de0}</figcaption>
      </figure>
    </div>
  </section>
"""

def samples_html(slug, lang, a, ui):
    import json
    meta = json.load(open(os.path.join(WEB, "assets", "samples", "samples.json")))
    rows = []
    for key in meta["_order"]:
        m = meta.get(key)
        if not m or m["plugin"] != slug: continue
        title, desc = html.escape(m["title"][lang]), html.escape(m["desc"][lang])
        # Los picos son RMS lineal normalizado; con exponente 0.7 las colas de reverb se ven sin aplanar los golpes.
        bars = "".join(f'<i style="--h:{max(0.06, v ** 0.7):.2f}"></i>' for v in m["peaks"])
        secs = m["secs"]; dur = f"{int(secs // 60)}:{int(round(secs % 60)):02d}"
        rows.append(f"""        <li class="track" data-src="{a}samples/{key}.m4a">
          <button class="tr-play" type="button" aria-label="{ui['play']}: {title}" data-play="{ui['play']}" data-pause="{ui['pause']}"></button>
          <div class="tr-text"><p class="tr-title">{title}</p><p class="tr-desc">{desc}</p></div>
          <div class="tr-wave" role="slider" tabindex="0" aria-label="{ui['seek']} {title}" aria-valuemin="0" aria-valuemax="{int(round(secs))}" aria-valuenow="0">
            <div class="wv">{bars}</div><div class="wv wv-fill" aria-hidden="true">{bars}</div>
          </div>
          <span class="tr-time">{dur}</span>
          {"" if m.get("nomidi") else f'<a class="tr-midi" href="{a}samples/{key}.mid" download aria-label="{ui[chr(109) + "idi_dl"]} {title}">MIDI</a>'}
        </li>""")
    return "\n".join(rows)

def specs_rows(slug, lang):
    r = REQ[lang]
    rows = [(g, items) for g, items in SPECS[slug][lang]]
    mac_d, win_d = DISK[slug]
    heavy = r["heavy"][slug]
    rows.append((r["min_h"], [f"<strong>macOS</strong> {r['mac_min']}", f"<strong>Windows</strong> {r['win_min']}", r["common_min"],
                              r["disk"].format(mac=mac_d, win=win_d)]))
    rows.append((r["rec_h"], [f"<strong>macOS</strong> {r['mac_rec']}", f"<strong>Windows</strong> {r['win_rec']}", r["common_rec"]] + ([heavy] if heavy else [])))
    dec = (lambda v: f"{v:.1f}".replace(".", ",")) if lang == "es" else (lambda v: f"{v:.1f}")
    pct = lambda v: ("&lt; 0,01" if lang == "es" else "&lt; 0.01") if v < 0.01 else dec(v)     # el generador de Orrery
    cols = r["cpu_cols"]
    table = (f'<table class="cpu-table"><thead><tr><th>{cols[0]}</th><th>{cols[1]}</th><th>{cols[2]}</th></tr></thead><tbody>'
             + "".join(f"<tr><td>{CPU_LABEL[lang][k]}</td><td>{pct(m)} %</td><td>{pct(w)} %</td></tr>" for k, m, w in CPU[slug])
             + "</tbody></table>")
    out = []
    for g, items in rows:
        lis = "".join(f"<li>{i}</li>" for i in items)
        out.append(f'      <div class="spec-row reveal"><h3>{g}</h3><ul>{lis}</ul></div>')
    out.append(f'      <div class="spec-row reveal"><h3>{r["cpu_h"]}</h3><div>{table}<p class="note">{r["cpu_note"].format(ui=UI_CPU[slug])}</p></div></div>')
    return "\n".join(out)

def linux_card(t, root, mac, lang):
    """La tercera tarjeta, al lado de macOS y Windows; vacía si no hay zip de Linux."""
    lin = linux_zip(mac)
    if not lin: return ""
    L = LINUX[lang]
    fmt = t.get("linux_fmt") or t["win_fmt"].replace("VST3", "VST3, LV2", 1)
    steps = "".join(f"<li>{s}</li>" for s in L["steps"])
    return f"""
      <article class="dl-card reveal">
        <h3>{L['h']}</h3>
        <p class="req">{L['req']} {fmt}. {size_mb(lin)}.</p>
        <a class="pill primary" href="{root}downloads/{lin}" download data-support>{L['dl']}</a>
        <details><summary>{UI[lang]['install']}</summary><ol>{steps}</ol></details>
      </article>"""

def download_html(t, ui, root, mac, win, slug, lang):
    steps = lambda lst: "".join(f"<li>{s}</li>" for s in lst)
    r = REQ[lang]
    return f"""    <div class="dl-cards">
      <article class="dl-card reveal">
        <h3>macOS</h3>
        <p class="req">{t['mac_req']} {t['mac_fmt']}. {size_mb(mac)}.</p>
        <a class="pill primary" href="{root}downloads/{mac}" download data-support>{ui['dl_mac']}</a>
        <details><summary>{ui['install']}</summary><ol>{steps(t['mac_steps'])}</ol></details>
      </article>
      <article class="dl-card reveal">
        <h3>Windows</h3>
        <p class="req">{t['win_req']} {t['win_fmt']}. {size_mb(win)}.</p>
        <a class="pill primary" href="{root}downloads/{win}" download data-support>{ui['dl_win']}</a>
        <details><summary>{ui['install']}</summary><ol>{steps(t['win_steps'])}</ol></details>
      </article>{linux_card(t, root, mac, lang)}
    </div>
    <p class="note">{t['dl_note']}</p>{f'<p class="dl-manual"><a class="more" href="manual/">{ui["read_manual"]}</a></p>' if manual_web.has_manual(slug) else ""}{launcher_promo(lang, root + ("" if lang == "en" else "es/"))}"""

def page_product(slug, lang):
    name, year, mac, win, ver = next((n, y, m, w, v) for s, n, y, m, w, v in PRODUCTS if s == slug)
    t, ui, pr = TEXTS(slug, lang), UI[lang], PRES[slug][lang]
    root = "../" if lang == "en" else "../../"
    a = root + "assets/"
    home = root + ("" if lang == "en" else "es/")
    colours = pr.get("colours")
    hero_img = HERO_IMG[slug]
    hero_alt = t.get("hero_alt", name)

    nav = [("highlights", ui["overview"]), ("film", ui["film"])] + ([("demos", "Demos")] if slug == "orrery" else []) + [("listen", ui["listen"])] + ([("colours", ui["colours"])] if colours else []) + [("specs", ui["specs"])]
    navlinks = "".join(f'<a href="#{i}">{l}</a>' for i, l in nav)
    if manual_web.has_manual(slug): navlinks += f'<a href="manual/">{ui["manual"]}</a>'
    cards = "\n".join(f"""        <article class="hl-card">{pic(a, img, b, "(max-width: 700px) 80vw, 620px")}<p><strong>{b}</strong> {txt}</p></article>"""
                      for img, b, txt in pr["highlights"])
    dots = "".join(f'<button class="hl-dot" type="button" aria-label="{ui["go_to"]} {i + 1}"></button>' for i in range(len(pr["highlights"])))
    stats = "\n".join(f'      <div class="reveal"><div class="stat-n">{n}{f"<small>{s}</small>" if s else ""}</div><div class="stat-l">{l}</div></div>' for n, s, l in pr["stats"])
    spec = "\n".join(f"          <dt>{k}</dt><dd>{v}</dd>" for k, v in t["spec"])

    # Bloques propios de cada instrumento (los textos ya existentes).
    if slug == "at20v":
        sound_img = ("render-closeup", t["alt_close"])
        mid = f"""
  <section class="sec statement" id="patch">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['patch_h']}</h2>
      <p class="lede center reveal">{t['patch_p1']}</p>
      <figure class="statement-img reveal">{pic(a, "ui-synth", t["alt_synth"])}<figcaption>{t['patch_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec deep" id="extensions">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['ext_h']}</h2><p class="lede">{t['ext_p']}</p><ul class="facts">
{facts_html(t['ext_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "ui-extensions", t["alt_ext"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['ext_caption']}</figcaption></figure>
    </div>
  </section>"""
    elif slug == "at101v":
        sound_img = ("at101v/hl-filter", t["alt_synth"])
        mid = f"""
  <section class="sec statement" id="panel">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['panel_h']}</h2>
      <p class="lede center reveal">{t['panel_p1']}</p>
      <figure class="statement-img reveal">{pic(a, "at101v/ui-synth", t["alt_synth"])}<figcaption>{t['panel_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec deep" id="sequencer">
    <div class="wrap split">
      <div class="reveal"><h2 class="sec-head">{t['seq_h']}</h2><p class="lede">{t['seq_p']}</p><ul class="facts">
{facts_html(t['seq_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "at101v/ui-seq", t["alt_seq"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['seq_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec" id="extensions">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['ext_h']}</h2><p class="lede">{t['ext_p']}</p><ul class="facts">
{facts_html(t['ext_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "at101v/ui-settings", t["alt_ext"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['ext_caption']}</figcaption></figure>
    </div>
  </section>"""
    elif slug == "at700":
        sound_img = ("at700/hl-filters", t["alt_synth"])
        mid = f"""
  <section class="sec statement" id="console">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['console_h']}</h2>
      <p class="lede center reveal">{t['console_p']}</p>
      <figure class="statement-img reveal">{pic(a, "at700/ui-console", t["alt_synth"])}<figcaption>{t['console_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec deep" id="sequencer">
    <div class="wrap split">
      <div class="reveal"><h2 class="sec-head">{t['seq_h']}</h2><p class="lede">{t['seq_p']}</p><ul class="facts">
{facts_html(t['seq_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "at700/ui-seq", t["alt_seq"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['seq_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec" id="extensions">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['ext_h']}</h2><p class="lede">{t['ext_p']}</p><ul class="facts">
{facts_html(t['ext_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "at700/ui-settings", t["alt_ext"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['ext_caption']}</figcaption></figure>
    </div>
  </section>"""
    elif slug == "atfs1":
        sound_img = ("atfs1/hl-voice", t["alt_synth"])
        mid = f"""
  <section class="sec statement" id="fseq">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['fseq_h']}</h2>
      <p class="lede center reveal">{t['fseq_p']}</p>
      <figure class="statement-img reveal">{pic(a, "atfs1/ui-fseq", t["alt_fseq"])}<figcaption>{t['fseq_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec deep" id="operators">
    <div class="wrap split">
      <div class="reveal"><h2 class="sec-head">{t['ops_h']}</h2><p class="lede">{t['ops_p']}</p><ul class="facts">
{facts_html(t['ops_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "atfs1/ui-operator", t["alt_ops"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['ops_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec" id="extensions">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['ext_h']}</h2><p class="lede">{t['ext_p']}</p><ul class="facts">
{facts_html(t['ext_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "atfs1/ui-play", t["alt_ext"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['ext_caption']}</figcaption></figure>
    </div>
  </section>"""
    elif slug == "orrery":
        sound_img = ("orrery/hl-sky", t["alt_synth"])
        mid = f"""
  <section class="sec statement" id="moods">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['moods_h']}</h2>
      <p class="lede center reveal">{t['moods_p']}</p>
      <figure class="statement-img reveal">{pic(a, "orrery/ui-simple", t["alt_synth"])}<figcaption>{t['moods_caption']}</figcaption></figure>
      <p class="muted center reveal">{t['care']}</p>
    </div>
  </section>
  <section class="sec deep" id="lanes">
    <div class="wrap split">
      <div class="reveal"><h2 class="sec-head">{t['lanes_h']}</h2><p class="lede">{t['lanes_p']}</p><ul class="facts">
{facts_html(t['lanes_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "orrery/ui-lanes", t["alt_lanes"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['lanes_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec" id="outputs">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['out_h']}</h2><p class="lede">{t['out_p']}</p><ul class="facts">
{facts_html(t['out_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "orrery/ui-outputs", t["alt_out"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['out_caption']}</figcaption></figure>
    </div>
  </section>"""
    elif slug == "atmd":
        sound_img = ("atmd/hl-panel", t["alt_voice"])
        mid = f"""
  <section class="sec statement" id="panel">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['panel_h']}</h2>
      <p class="lede center reveal">{t['panel_p1']}</p>
      <figure class="statement-img reveal">{pic(a, "atmd/ui-panel", t["alt_synth"])}<figcaption>{t['panel_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec deep" id="voice">
    <div class="wrap split">
      <div class="reveal"><h2 class="sec-head">{t['voice_h']}</h2><p class="lede">{t['voice_p']}</p><ul class="facts">
{facts_html(t['voice_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "atmd/hl-filter", t["alt_voice"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['voice_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec" id="extensions">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['ext_h']}</h2><p class="lede">{t['ext_p']}</p><ul class="facts">
{facts_html(t['ext_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "atmd/ui-settings", t["alt_ext"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['ext_caption']}</figcaption></figure>
    </div>
  </section>"""
    elif slug == "halation":
        sound_img = ("halation/hl-close", t["hero_alt"])
        mid = f"""
  <section class="sec statement" id="flow">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['flow_h']}</h2>
      <p class="lede center reveal">{t['flow_p']}</p>
      <figure class="statement-img reveal">{pic(a, "halation/ui-overview", t["alt_ui"])}<figcaption>{t['flow_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec deep" id="room">
    <div class="wrap split">
      <div class="reveal"><h2 class="sec-head">{t['room_h']}</h2><p class="lede">{t['room_p']}</p><ul class="facts">
{facts_html(t['room_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "halation/ui-room", t["alt_room"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['room_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec" id="modules">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['mods_h']}</h2><p class="lede">{t['mods_p']}</p><ul class="facts">
{facts_html(t['mods_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "halation/ui-frozen", t["alt_frozen"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['mods_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec statement" id="slow">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['slow_h']}</h2>
      <p class="lede center reveal">{t['slow_p']}</p>
    </div>
  </section>"""
    elif slug == "at303":
        sound_img = ("at303/hl-close", t["hero_alt"])
        mid = f"""
  <section class="sec statement" id="flow">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['flow_h']}</h2>
      <p class="lede center reveal">{t['flow_p']}</p>
      <figure class="statement-img reveal">{pic(a, "at303/ui-overview", t["alt_ui"])}<figcaption>{t['flow_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec deep" id="room">
    <div class="wrap split">
      <div class="reveal"><h2 class="sec-head">{t['room_h']}</h2><p class="lede">{t['room_p']}</p><ul class="facts">
{facts_html(t['room_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "at303/ui-editor", t["alt_room"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['room_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec" id="modules">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['mods_h']}</h2><p class="lede">{t['mods_p']}</p><ul class="facts">
{facts_html(t['mods_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "at303/ui-inspect", t["alt_frozen"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['mods_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec statement" id="slow">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['slow_h']}</h2>
      <p class="lede center reveal">{t['slow_p']}</p>
    </div>
  </section>"""
    elif slug == "plumb":
        sound_img = ("plumb/hl-close", t["hero_alt"])
        mid = f"""
  <section class="sec statement" id="flow">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['flow_h']}</h2>
      <p class="lede center reveal">{t['flow_p']}</p>
      <figure class="statement-img reveal">{pic(a, "plumb/ui-overview", t["alt_ui"])}<figcaption>{t['flow_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec deep" id="room">
    <div class="wrap split">
      <div class="reveal"><h2 class="sec-head">{t['room_h']}</h2><p class="lede">{t['room_p']}</p><ul class="facts">
{facts_html(t['room_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "plumb/ui-hard", t["alt_room"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['room_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec" id="modules">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['mods_h']}</h2><p class="lede">{t['mods_p']}</p><ul class="facts">
{facts_html(t['mods_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "plumb/ui-advanced", t["alt_frozen"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['mods_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec statement" id="slow">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['slow_h']}</h2>
      <p class="lede center reveal">{t['slow_p']}</p>
    </div>
  </section>"""
    elif slug == "tarnish":
        sound_img = ("tarnish/hl-close", t["hero_alt"])
        mid = f"""
  <section class="sec statement" id="flow">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['flow_h']}</h2>
      <p class="lede center reveal">{t['flow_p']}</p>
      <figure class="statement-img reveal">{pic(a, "tarnish/ui-overview", t["alt_ui"])}<figcaption>{t['flow_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec deep" id="room">
    <div class="wrap split">
      <div class="reveal"><h2 class="sec-head">{t['room_h']}</h2><p class="lede">{t['room_p']}</p><ul class="facts">
{facts_html(t['room_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "tarnish/ui-vinyl", t["alt_room"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['room_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec" id="modules">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['mods_h']}</h2><p class="lede">{t['mods_p']}</p><ul class="facts">
{facts_html(t['mods_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "tarnish/ui-service", t["alt_frozen"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['mods_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec statement" id="slow">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['slow_h']}</h2>
      <p class="lede center reveal">{t['slow_p']}</p>
    </div>
  </section>"""
    elif slug == "lacquer":
        sound_img = ("lacquer/hl-close", t["hero_alt"])
        mid = f"""
  <section class="sec statement" id="flow">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['flow_h']}</h2>
      <p class="lede center reveal">{t['flow_p']}</p>
      <figure class="statement-img reveal">{pic(a, "lacquer/ui-overview", t["alt_ui"])}<figcaption>{t['flow_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec deep" id="chain">
    <div class="wrap split">
      <div class="reveal"><h2 class="sec-head">{t['chain_h']}</h2><p class="lede">{t['chain_p']}</p><ul class="facts">
{facts_html(t['chain_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "lacquer/ui-loud", t["alt_mod"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['chain_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec" id="fair">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['fair_h']}</h2><p class="lede">{t['fair_p']}</p><ul class="facts">
{facts_html(t['fair_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "lacquer/ui-readouts", t["alt_read"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['fair_caption']}</figcaption></figure>
    </div>
  </section>"""
    elif slug == "murmur":
        sound_img = ("murmur/hl-flock", t["hero_alt"])
        mid = f"""
  <section class="sec statement" id="flock">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['flock_h']}</h2>
      <p class="lede center reveal">{t['flock_p']}</p>
      <figure class="statement-img reveal">{pic(a, "murmur/ui-simple", t["alt_synth"])}<figcaption>{t['flock_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec deep" id="advanced">
    <div class="wrap split">
      <div class="reveal"><h2 class="sec-head">{t['adv_h']}</h2><p class="lede">{t['adv_p']}</p><ul class="facts">
{facts_html(t['adv_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "murmur/ui-advanced", t["alt_adv"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['adv_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec" id="freeze">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['play_h']}</h2><p class="lede">{t['play_p']}</p><ul class="facts">
{facts_html(t['play_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "murmur/ui-frozen", t["alt_frozen"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['play_caption']}</figcaption></figure>
    </div>
  </section>"""
    else:
        sound_img = ("at42/hl-orbs", t["alt_synth"])
        mid = f"""
  <section class="sec statement" id="patch">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['patch_h']}</h2>
      <p class="lede center reveal">{t['patch_p1']} {t['patch_p2']}</p>
      <figure class="statement-img reveal">{pic(a, "at42/ui-synth", t["alt_synth"])}<figcaption>{t['patch_caption']}</figcaption></figure>
    </div>
  </section>
  <section class="sec deep" id="keys">
    <div class="wrap split">
      <div class="reveal"><h2 class="sec-head">{t['keys_h']}</h2><p class="lede">{t['keys_p']}</p><ul class="facts">
{facts_html(t['keys_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "at42/hero-cream", t["alt_synth"], "(max-width: 900px) 100vw, 58vw")}</figure>
    </div>
  </section>
  <section class="sec" id="extensions">
    <div class="wrap split reverse">
      <div class="reveal"><h2 class="sec-head">{t['ext_h']}</h2><p class="lede">{t['ext_p']}</p><ul class="facts">
{facts_html(t['ext_facts'])}
        </ul></div>
      <figure class="reveal">{pic(a, "at42/ui-settings", t["alt_ext"], "(max-width: 900px) 100vw, 58vw")}<figcaption>{t['ext_caption']}</figcaption></figure>
    </div>
  </section>"""

    colour_sec = ""
    if colours:
        imgs = "".join(f'<div class="colour-img{" on" if i == 0 else ""}">{pic(a, f"{slug}/color-{k}", f"{name} {n}", "(max-width: 1100px) 100vw, 1100px")}</div>' for i, (k, n, c) in enumerate(colours))
        sw = "".join(f'<button class="swatch" type="button" role="radio" aria-checked="{"true" if i == 0 else "false"}" aria-label="{n}" data-name="{n}" style="--c:{c}"></button>' for i, (k, n, c) in enumerate(colours))
        colour_sec = f"""
  <section class="sec deep" id="colours">
    <div class="wrap">
      <h2 class="sec-head center reveal">{ui['colours_h']}</h2>
      <div class="colours reveal" data-colours>
        <div class="colour-stage">{imgs}</div>
        <div class="swatches" role="radiogroup" aria-label="{ui['colours']}">{sw}</div>
        <p class="colour-name">{colours[0][1]}</p>
      </div>
    </div>
  </section>"""

    os_text = t.get("os_p") or (t["license_p1"] + " " + t["license_p2"])
    os_link = (S if slug == "at20v" else t)[lang]["os_link"] if slug == "at20v" else t["os_link"]
    body = f"""  <nav class="localnav" aria-label="{name}">
    <div class="wrap">
      <a class="ln-name" href="#top">{name}</a>
      <div class="ln-right">{navlinks}<a class="pill primary small" href="#download">{ui['download']}</a></div>
    </div>
  </nav>
  <main id="top">
  <section class="p-hero">
    <div class="wrap">
      <h1 class="p-name">{name}</h1>
      <p class="p-tag">{pr['tag']}</p>
      <p class="p-price">{ui['prices']} {name} {ver}.</p>
      <div class="p-cta">
        <a class="pill primary" href="#download">{ui['download']}</a>
        <button class="pill link" type="button" data-film-open>{ui['watch']} <span class="tri" aria-hidden="true"></span></button>
      </div>
    </div>
    <div class="p-hero-img">{pic(a, hero_img, hero_alt, eager=True)}</div>
  </section>

  <section class="sec" id="highlights">
    <div class="wrap"><h2 class="sec-head reveal">{ui['highlights']}</h2></div>
    <div class="hl" data-carousel>
      <div class="hl-track">
{cards}
      </div>
      <div class="hl-ctrl">
        <button class="hl-btn hl-prev" type="button" aria-label="{ui['prev']}"></button>
        <div class="hl-dots">{dots}</div>
        <button class="hl-btn hl-next" type="button" aria-label="{ui['next']}"></button>
      </div>
    </div>
  </section>

  <section class="sec deep" id="film">
    <div class="wrap">
      <h2 class="sec-head reveal">{ui['film_h']}</h2>
      <figure class="film-frame reveal">
        {player_html(a, slug, pr['film_cap'], pr['film_cap'])}
        <figcaption>{pr['film_cap']}</figcaption>
      </figure>
    </div>
  </section>
{demos_html(a, t, lang) if slug == "orrery" else ""}
  <section class="sec" id="sound">
    <div class="wrap split">
      <div class="reveal">
        <h2 class="sec-head">{t['sound_h']}</h2>
        <p class="lede">{t['sound_p1']}</p>
        <p class="muted">{t['sound_p2']}</p>
        <dl class="spec">
{spec}
        </dl>
      </div>
      <figure class="reveal">{pic(a, sound_img[0], sound_img[1], "(max-width: 900px) 100vw, 58vw")}</figure>
    </div>
    <div class="wrap"><div class="stats">
{stats}
    </div></div>
  </section>

  <section class="sec deep" id="listen">
    <div class="wrap">
      <h2 class="sec-head reveal">{ui['listen_h']}</h2>
      <p class="lede reveal">{t.get('listen_p', ui['listen_p'])}</p>
      <ol class="tracks reveal" data-tracks>
{samples_html(slug, lang, a, ui)}
      </ol>
    </div>
  </section>
{mid}
  <section class="sec deep" id="presets">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['presets_h']}</h2>
      <div class="presets">
{presets_html(t['presets'])}
      </div>
    </div>
  </section>
{colour_sec}
  <section class="sec" id="specs">
    <div class="wrap">
      <h2 class="sec-head reveal">{ui['specs_h']}</h2>
      <div class="specs">
{specs_rows(slug, lang)}
      </div>
    </div>
  </section>

  <section class="sec deep" id="download">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['dl_h']}</h2>
      <p class="lede reveal">{t['dl_p']}</p>
{download_html(t, ui, root, mac, win, slug, lang)}
      <div class="license reveal"><p>{os_text}</p><p><a class="more" href="{home}#open-source">{os_link}</a></p></div>
    </div>
  </section>
  </main>
  <dialog class="film-modal" aria-label="{ui['film_h']}">
    <button class="film-close" type="button" aria-label="{ui['close']}">×</button>
    <video data-player preload="none" playsinline poster="{a}img/{slug}/film-poster.jpg"><source src="{a}video/{slug}.mp4" type="video/mp4"></video>
  </dialog>"""
    foot = f"      <p>{t['trademark']}</p>\n      <p>{t['photo_credit']}</p>"
    title = t["title_20"] if slug == "at20v" else t["title"]
    og = {"at20v": "og-image.jpg", "at303": "at303/og-image.jpg", "at101v": "at101v/og-image.jpg", "at42": "at42/og-image.jpg", "at700": "at700/og-image.jpg", "atfs1": "atfs1/og-image.jpg", "orrery": "orrery/og-image.jpg", "murmur": "murmur/og-image.jpg", "atmd": "atmd/og-image.jpg", "lacquer": "lacquer/og-image.jpg", "halation": "halation/og-image.jpg", "tarnish": "tarnish/og-image.jpg", "plumb": "plumb/og-image.jpg"}[slug]
    return shell(lang, slug + "/", title, t["description"], body, og, foot, "p-" + slug)

def page_home(lang):
    h, t, ui = H[lang], T[lang], UI[lang]
    root = "" if lang == "en" else "../"
    a = root + "assets/"
    home = root + ("" if lang == "en" else "es/")
    cards = {c["slug"]: c for c in h["cards"]}
    tiles = []
    for slug, name, year, mac, win, ver in PRODUCTS:
        c, pr = cards[slug], PRES[slug][lang]
        tiles.append(f"""  <section class="tile {slug}">
    <div class="wrap">
      <p class="year">{year}</p>
      <h2>{name}</h2>
      <p class="t-tag">{pr['tag']}</p>
      <p class="lede center">{c['what']} {c['body']}</p>
      <div class="t-links"><a class="more" href="{home}{slug}/">{ui['learn']}</a><a class="more" href="{root}downloads/{mac}" download data-support>{ui['dl_mac']}</a><a class="more" href="{root}downloads/{win}" download data-support>{ui['dl_win']}</a>{f'<a class="more" href="{root}downloads/{linux_zip(mac)}" download data-support>{LINUX[lang]["dl"]}</a>' if linux_zip(mac) else ""}</div>
    </div>
    <a class="t-img" href="{home}{slug}/">{pic(a, c['imgdir'] + c['img'], c['alt'])}</a>
  </section>""")
    synths = [x for x in PRODUCTS if x[0] not in ("orrery", "murmur", "lacquer", "halation", "tarnish", "plumb")]
    cmp_heads = "".join(f"""<div class="cmp-head reveal">{pic(a, cards[s]['imgdir'] + cards[s]['img'], cards[s]['alt'], "(max-width: 900px) 33vw, 380px")}<h3>{n}</h3><p>{PRES[s][lang]['tag']}</p><a class="pill primary small" href="{home}{s}/" style="--accent:{ACCENT[s]}">{ui['learn']}</a></div>""" for s, n, *_ in synths)
    cmp_rows = "".join("".join(f'<div class="cmp-cell"><b>{label}</b><span>{vals[COMPARE_ORDER.index(sl)]}</span></div>' for sl, *_ in synths) for label, vals in COMPARE_ROWS[lang])
    reasons = "\n".join(f'      <div class="reveal"><h3>{k}</h3><p>{v}</p></div>' for k, v in t["reasons"])
    lic = ("Every Audio Tools instrument is free software under the GNU Affero General Public License v3, the same license as the JUCE framework they are built on. You can use them, study them, change them and share them. If you distribute a modified version, you share your changes under the same terms."
           if lang == "en" else
           "Todos los instrumentos de Audio Tools son software libre bajo la GNU Affero General Public License v3, la misma licencia del framework JUCE sobre el que están hechos. Los podés usar, estudiar, modificar y compartir. Si distribuís una versión modificada, compartís tus cambios en los mismos términos.")
    shelf = "".join(
        f'<a class="shelf-item" href="{home}{sl}/" style="--accent:{ACCENT[sl]}">{pic(a, cards[sl]["imgdir"] + cards[sl]["img"], cards[sl]["alt"], "(max-width: 700px) 70vw, 40vw", eager=(i < 3))}'
        f'<span class="sh-text"><span class="sh-year">{y}</span><span class="sh-name">{n}</span><span class="sh-tag">{PRES[sl][lang]["tag"]}</span></span></a>'
        for i, (sl, n, y, *_) in enumerate(PRODUCTS))
    body = f"""  <main>
  <section class="home-hero">
    <div class="wrap">
      <h1>{h['h1']}</h1>
      <p class="lede">{h['lead']}</p>
    </div>
    <div class="shelf" aria-label="{NAV_LABELS[lang]['all']}">{shelf}</div>
    <div class="wrap">
      <div class="p-cta"><a class="pill primary" href="#compare">{ui['compare']}</a><a class="more" href="#open-source">{ui['why']}</a></div>
    </div>
  </section>
{chr(10).join(tiles)}
  <section class="sec" id="compare">
    <div class="wrap">
      <h2 class="sec-head center reveal">{ui['compare_h']}</h2>
      <div class="compare"><div class="cmp" style="--cols:{len(synths)}">{cmp_heads}{cmp_rows}</div></div>
    </div>
  </section>
  <section class="sec deep" id="open-source">
    <div class="wrap">
      <h2 class="sec-head reveal">{t['os_h']}</h2>
      <p class="lede reveal">{t['os_p1']} {t['os_p2']}</p>
      <div class="reasons">
{reasons}
      </div>
      <div class="license reveal"><p>{lic}</p><p>{t['license_p2']}</p></div>
    </div>
  </section>
  <section class="sec" id="next">
    <div class="wrap"><h2 class="sec-head reveal">{h['next_h']}</h2><p class="lede reveal">{h['next_p']}</p></div>
  </section>
  </main>"""
    marks = {"en": "Korg and MS-20 are trademarks of Korg Inc. Roland, SH-101 and System-700 are trademarks of Roland Corporation. Yamaha and FS1R are trademarks of Yamaha Corporation. Moog and Minimoog are trademarks of Moog Music Inc. Elta Music and SOLAR 42 are trademarks of their owners. Audio Tools is an independent project, not affiliated with or endorsed by Korg, Roland, Yamaha, Moog Music or Elta Music. Product names are used only to identify the instruments that inspired these recreations.",
             "es": "Korg y MS-20 son marcas de Korg Inc. Roland, SH-101 y System-700 son marcas de Roland Corporation. Yamaha y FS1R son marcas de Yamaha Corporation. Moog y Minimoog son marcas de Moog Music Inc. Elta Music y SOLAR 42 son marcas de sus dueños. Audio Tools es un proyecto independiente, sin relación ni aval de Korg, Roland, Yamaha, Moog Music ni Elta Music. Los nombres de productos se usan sólo para identificar los instrumentos que inspiraron estas recreaciones."}
    foot = f"      <p>{marks[lang]}</p>"
    return shell(lang, "", h["title"], h["description"], body, "at101v/og-image.jpg", foot, "home")

if __name__ == "__main__":
    # Guion no separable en los nombres de modelo (si no, "MS-20" o "SH-101" se cortan en dos renglones).
    fix = lambda h: h.replace("MS-20", "MS‑20").replace("SH-101", "SH‑101")
    for lang in ("en", "es"):
        for rel, fn in [("index.html", page_home)] + [(f"{s}/index.html", (lambda s: lambda l: page_product(s, l))(s)) for s, *_ in PRODUCTS]:
            out = os.path.join(WEB, *([] if lang == "en" else ["es"]), rel)
            os.makedirs(os.path.dirname(out), exist_ok=True)
            open(out, "w").write(fix(fn(lang)))
        for s, n, *_ in PRODUCTS:
            if not manual_web.has_manual(s): continue
            out = os.path.join(WEB, *([] if lang == "en" else ["es"]), s, "manual", "index.html")
            os.makedirs(os.path.dirname(out), exist_ok=True)
            open(out, "w").write(fix(manual_web.page(s, n, lang, shell)))
        if LAUNCHER_RELEASED:
            out = os.path.join(WEB, *([] if lang == "en" else ["es"]), "launcher", "index.html")
            os.makedirs(os.path.dirname(out), exist_ok=True)
            open(out, "w").write(fix(page_launcher(lang)))
    m = write_manifest()
    print("manifest:", len(m["products"]), "productos,", "launcher", VLA, "publicado" if LAUNCHER_RELEASED else "sin zips", "|", m["generated"])
    print("web generada:", VERSION, size_mb(MAC), size_mb(WIN), "|", V101, size_mb(MAC101), size_mb(WIN101), "|", V42, size_mb(MAC42), size_mb(WIN42))
