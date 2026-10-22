# AT20V — Audio Tools

Sintetizador semimodular basado en el Korg MS-20 (1978), como plugin **VST3 / AU / LV2 / Standalone
para macOS, Windows y Linux**. El circuito está modelado a partir del manual de servicio, del estudio de
filtros de Tim Stinchcombe y de las cartas de ajuste de fábrica. La interfaz es fotorrealista, se
renderiza en Blender con patchera interactiva y cables con física, y tiene un teclado con rueda y
pulsador al estilo del Arturia KORG MS-20 V.

No usa gráficos, logos ni código de Korg ni de Arturia: el panel se dibujó desde cero midiendo una
foto CC0 (`docs/ref/`), y el nombre y la marca son de Audio Tools.

| | |
|---|---|
| Plugins | **AT20V** (instrumento) y **AT20V FX** (efecto con entrada MIDI, para vocoder sobre una pista de audio) |
| Formatos | macOS: VST3, AU (`aumu At2v Atls` / `aumf At2f Atls`), Standalone — binario universal arm64 + x86_64, macOS 11+ · Windows 10/11 x64: VST3, Standalone |
| MIDI | Entrada (notas, pitch bend → rueda, CC1, sustain → pulsador) y **salida** (Thru, Patch o ambas) |
| Polifonía | Mono (como el MS-20) o 2/4/6/8/12 voces, unísono de 1 a 4 con detune y apertura estéreo |
| Audio in | Sidechain en AT20V; entrada principal en AT20V FX. Alimenta la **ESP** y el **vocoder** |
| Presets | 55: 14 de fábrica del manual original, 13 clásicos propios, 11 **self-playing**, 9 **ambient**, 4 **vocoder**, 4 **effect**. Los que necesitan audio in llevan "(Audio In)" en el nombre; AT20V FX lista sólo los 21 que sirven en una pista de audio (effect, vocoder, guitar synth, self-playing) |
| Validación | `auval` PASS · `pluginval --strictness-level 8` SUCCESS en Mac (VST3 y AU de ambas variantes) y Windows (VST3) |
| 1.0.7 (QA en DAWs 2026-10-22) | `athosttest` 0 FAIL en VST3 y AU del AT20V (AT20V FX: 1 FAIL conocido, Endless Drift que toca solo), `hostgap`, `statebytes`, `midilatency` (el MIDI out sale 27 muestras tarde, igual que el audio) PASS; `pluginval --strictness-level 10` SUCCESS en los dos VST3; 63 de 68 parámetros automatizables (Live llena el panel hasta 64) |
| Framework | JUCE 9.0.4 + CMake/Ninja. Mac: Command Line Tools. Windows: VS 2022 Build Tools (MSVC 14.44) |

## Instalar

**macOS**: el build ya copia los plugins a `~/Library/Audio/Plug-Ins/{VST3,Components}`. En Ableton:
*Settings → Plug-Ins → Rescan*; aparecen en *Plug-Ins → Audio Tools*.

**Windows**: copiar `AT20V.vst3` y `AT20V FX.vst3` a `C:\Program Files\Common Files\VST3\`
(necesita permisos de administrador) y reescanear en el DAW. `AT20V.exe` es la versión standalone.

## Compilar

Desde la raíz del repo (`audiotools/`), que tiene JUCE como submódulo:

```bash
sh scripts/build-macos.sh                                            # macOS: compila y empaqueta
powershell -ExecutionPolicy Bypass -File scripts\build-windows.ps1  # Windows
```

Los paquetes quedan en `dist/` y en `web/downloads/`. La versión del plugin está en
`plugins/at20v/CMakeLists.txt` (`AT20V_VERSION`).

## Qué emula (y cómo)

Detalle técnico y fuentes: [`docs/hardware-spec.md`](docs/hardware-spec.md).

- **Filtros** (`Source/dsp/Korg35.h`): Sallen-Key de Stinchcombe en forma TPT con la no linealidad
  resuelta implícita (Newton). LPF 12 dB/oct; **HPF 6 dB/oct** como el hardware. Rev1 (Korg35):
  diodos en la etapa ×58, así que la distorsión crece con el nivel del mezclador (≈5 % con una VCO
  a fondo, ≈15 % con las dos) y la resonancia es asimétrica. Rev2 (LM13600): diodos sólo en el
  lazo. Calibración: CUTOFF 5 + PEAK 10 autooscila a **500 Hz**.
- **VCO** con PolyBLEP/BLAMP, RING como XOR, PW hasta desaparecer, deriva analógica.
- **Convenciones eléctricas del original**: Hz/V en el teclado y en VCO CV IN, 1 V/oct en
  FREQ/TOTAL, 2 oct/V en los CUTOFF, disparos S-trig, prioridad aguda con disparo único.
- **Patchera completa** (35 jacks) con sus normalizaciones; las salidas aceptan fichas apiladas.
- **ESP** con detector de altura YIN (sigue una guitarra con <0,3 % de error).
- Sobremuestreo 4x/8x de toda la voz con decimadores FIR half-band.

### Extensiones (página EXTENSIONS)

No son del MS-20. Se resolvieron con el mismo lenguaje visual que el panel.

- **Vocoder** de 8 a 20 bandas (pasa bandas de 4 polos), formant shift ±1 oct, resonancia,
  ataque/caída, sibilancia de consonantes y mezcla. El portador es el sintetizador; el modulador,
  la entrada de audio.
- **Polifonía**: voces, unísono, detune, apertura estéreo, prioridad, redisparo.
- **Delay** ping-pong sincronizado al tempo y **reverb de placa** (Dattorro).
- **System**: revisión del filtro, deriva analógica, **MG CLOCK** (el MG se sincroniza al tempo del
  DAW y FREQUENCY elige la división), rango de pitch bend, sobremuestreo, **MIDI OUT**, salida.

### MIDI OUT

- **Thru** reenvía el MIDI que entra y el del teclado en pantalla.
- **Patch** convierte en notas cada disparo del bus de teclado, incluidos los que genera la
  patchera (p. ej. MG → TRIG IN), con la altura que suena en VCO1. Así un preset self-playing
  puede tocar otro instrumento: en Ableton, otra pista MIDI con *MIDI From → AT20V*.

### Presets self-playing

Suenan solos: el MG en modo TEMPO dispara TRIG IN y el S/H. Mantener una tecla apretada transpone
el patrón (TRIG IN reemplaza al disparo del teclado, pero el CV del teclado sigue mandando).

### Vocoder en Ableton

- **AT20V FX** en la pista de voz y una pista MIDI con *MIDI To → [pista de voz] → AT20V FX*.
  Se toca desde esa pista MIDI.
- **AT20V** (instrumento) con *Sidechain* en la cabecera del dispositivo apuntando a la voz.

## Interfaz

- Perillas: arrastre vertical (Shift = fino), doble clic = valor por defecto. Los selectores de dos
  posiciones cambian con un clic.
- Cables: se arrastran desde un jack libre hasta uno compatible. Tomar una ficha la levanta;
  soltarla en el aire la desenchufa. Alt-clic desenchufa el jack y clic derecho cambia el color.
- Teclado: clic y glissando; rueda con clic central (doble clic la centra); pulsador. La barra
  inferior mueve la octava y muestra voces, MIDI in/out, nivel de entrada y CPU.

## Pipeline de arte

```
tools/layout.py          posiciones (panel medido en la foto; página de extensiones diseñada)
tools/silkscreen.py      serigrafías a 6400 px (--ext para la página de extensiones)
tools/blender/scene.py   (pc50-wsl) bg, knobs, plugs, leds, keys (teclado), hero/herostill
tools/pack_assets.py     hojas de sprites + ui.json → assets/ui (embebido en el binario)
tools/presets.py         presets de fábrica → assets/presets
```

## Verificación

- `at20vrender preset.json out.wav [--seq "48+55+60:2,..."] [--esp in.wav] [--voices N] [--rev 1|2]`:
  render offline estéreo con los ajustes del preset, efectos incluidos.
- `tools/dsptest.cpp`: respuesta de filtros, autooscilación, THD.
- `AT20V_SNAPSHOT=/ruta.png AT20V_PRESET=n AT20V_TAB=0|1 AT20V_HOLD=nota`: captura del editor a 2x
  sin grabar la pantalla (funciona igual en Windows).

## Trampas

- **La tabla de salidas de la reverb de Dattorro lee 1913 muestras de un allpass de 1800.** Con la
  línea del tamaño justo, la lectura caía fuera del buffer y aparecían NaN *sólo a veces* (cuando el
  fuzzing de pluginval prendía la reverb). Las líneas tienen que ser más largas que su retardo.
- **`drawImage` hereda la opacidad del último `setColour`**: hay que llamar a `setOpacity (1.0f)`
  antes de cada sprite.
- **JUCE antepone `_` a los recursos embebidos cuyo nombre empieza con un dígito** (`_01_…_json`).
- **Literales no ASCII en `juce::String`** (·, →, −): sin `String::fromUTF8` se ven como `Â·`.
- **`CMAKE_OSX_ARCHITECTURES` tiene que definirse antes de `project()`.** Si no, el binario sale
  sólo arm64 y no hay ningún aviso.
- **Windows: pluginval por SSH se cae en la prueba del editor** (0xC0000005). La sesión SSH no tiene
  escritorio, no es el plugin. Las pruebas de interfaz se corren con una tarea programada `/IT` en
  la sesión del usuario (`C:\dev\pvgui.bat`).
- **Windows LNK1104 al recompilar**: un `pluginval` o un standalone viejo deja el `.vst3` tomado.
  Hay que hacer `taskkill` antes.
- **OptiX no inicializa en WSL** (error 7805): Blender cae a CUDA.
