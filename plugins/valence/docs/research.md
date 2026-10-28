# Valence: investigación previa

Fecha de la investigación: 2026-10-27. Documento interno de la Fase 0 (plan: `audiotools-private/pendientes/plan-arpegiador.md`).

Aviso: los nombres de productos y marcas aparecen acá porque es un documento interno de investigación. **Ninguno
puede aparecer en textos públicos** (web, presets, manual, demos, posts). En lo público se habla de "los
arpegiadores de los DAW" y "los arpegiadores de pasos comerciales", sin nombres.

Convención de fuentes: `[verificado]` = leído en la fuente; `[foro]` = reporte de usuarios, no oficial;
`[criterio propio]` = convención del género o decisión nuestra, sin una fuente única.

---

## 1. Competencia verificada

Búsquedas hechas el 2026-10-27 (WebSearch, modo standard). Los resultados son parciales: lo que no se pudo
confirmar queda marcado "sin confirmar" y hay que mirarlo en el manual del producto antes de afirmarlo en público.

### 1.1 Arpegiadores de los DAW

| DAW | Qué tiene | Fuente |
|---|---|---|
| Ableton Live | Dispositivo MIDI "Arpeggiator": toma las notas de un acorde y las toca como patrón rítmico; elección de estilo (orden de notas) y velocidad sincronizada al tempo o en milisegundos. Gate, octavas y retrigger no quedaron confirmados por la búsqueda, pero están en el manual | [Live MIDI Effect Reference](https://www.ableton.com/en/live-manual/11/live-midi-effect-reference/), [foro Ableton](https://forum.ableton.com/viewtopic.php?p=1782149) |
| Logic Pro | Plugin MIDI FX "Arpeggiator" en la ranura MIDI FX del canal. Note Order (arriba, abajo, arriba-abajo, adentro-afuera, azar, como se tocó), 4 variaciones por orden, rango de octavas hasta 4 (o inversiones), paneles Pattern, Options, Keyboard y Controller, Cycle Length. Rate hasta 1/128 según una fuente, hasta 1/32 según otra (contradicción sin resolver) | [Apple, Arpeggiator](https://help.apple.com/logicpro-effects/mac/10.2/en.lproj/lgce2f607310.html), [Sound On Sound](https://www.soundonsound.com/techniques/arp-nouveau), [Ask.Audio](https://ask.audio/articles/using-the-arpeggiator-in-logic-pro-x-part-1), [Apple iPad](https://support.apple.com/en-bh/guide/logicpro-ipad/lpip064ba416/ipados) |
| Bitwig | Arpeggiator con control por paso: encendido, desplazamiento de altura, velocidad y gate; con Layers, Selectors y Modulators se llega a polifonía, azar y cambio de patrones. Es lo más cercano a una fila por paso en un DAW (`[foro]`, falta confirmarlo contra el manual de Bitwig) | [Attack Magazine](https://www.attackmagazine.com/technique/tutorials/complex-arps-arpeggiator/), [KVR](https://www.kvraudio.com/forum/viewtopic.php?p=8160768) |
| REAPER | Sin confirmar: la búsqueda no devolvió nada sobre arpegiadores propios de REAPER. Verificar contra el manual de REAPER (ReaPlugs/JSFX) antes de afirmar nada | sin fuente |

### 1.2 Arpegiadores y secuenciadores de pasos comerciales o gratuitos

| Producto | Qué tiene | Fuente |
|---|---|---|
| Cthulhu (Xfer Records) | Memorizador de acordes más arpegiador de pasos con 8 pestañas, cada una con su propio largo (arpegios polimétricos que evolucionan); modo acorde-arpegio, ligaduras, secuencia de duración y velocidad, transposición inteligente, armonía. Unos USD 39 según notas viejas. No hay salida MIDI en Reason | [Xfer](https://www.xferrecords.com/products/cthulhu), [KVR](https://www.kvraudio.com/news/21153), [MusicRadar](https://www.musicradar.com/reviews/tech/xfer-records-cthulhu-574652) |
| PEACH (510k Arts) | Arpegiador AU/VST/VST3 con carriles Gate, Hold, Octave, Velocity y MIDI CC, cada uno con su largo (polimetría), tirada de dados, bloqueo de pasos, latch, rango de octavas, swing. USD 59 en un comercio. Ratchet no apareció en ninguna fuente | [Pulse Audio](https://pulse.audio/product/peach-by-510k/), [Isotonik](https://isotonikstudios.com/product/peach-vst-by-alexkid/) |
| SEQUND y POLYLLOP (510k Arts) | SEQUND: secuenciador polirrítmico con largo independiente por carril y probabilidad por paso (no es arpegiador). POLYLLOP: plugin sólo MIDI | [Plugin Boutique](https://www.pluginboutique.com/product/81-Bundles/89-Complete-Collection/16301-510k-Complete-Bundle) |
| BLEASS Arpeggiator | VST3/AU/AAX, 62 patrones rítmicos y 20 modos de reproducción, probabilidad de disparo, tamaño de paso, salida MIDI dedicada. Largo independiente por carril: no encontrado | [Thomann](https://www.thomannmusic.com/bleass_arpeggiator.htm) |

Otros que conviene mirar a mano (no los cubrió la búsqueda): Kirnu, Hexachords Orb, Scaler (su salida MIDI y el
ruteo en Logic aparecen en la sección 3).

### 1.3 Qué le falta a cada uno frente a Valence

Marcas: **Sí** = lo tiene, **Parcial** = lo tiene a medias o no se confirmó, **No** = no lo tiene según lo leído.

| Característica de Valence | DAW (Live, Logic, Bitwig, REAPER) | Cthulhu | PEACH | BLEASS |
|---|---|---|---|---|
| Polimetría de 4 capas simultáneas sobre el mismo acorde | No (una capa) | Parcial (pestañas con largo propio dentro de una secuencia) | Parcial (carriles con largo propio en una sola capa de notas) | No confirmado |
| Filas por paso incluyendo CC | Parcial (Bitwig tiene filas por paso, sin CC según lo leído) | Parcial | Sí (CC) | No confirmado |
| Ratchet 1-4 por paso | No | No confirmado | No confirmado | No confirmado |
| Probabilidad por paso | Parcial (Bitwig con modulación) | No confirmado | Parcial (dados) | Sí (disparo) |
| Mutación lenta por ciclo con semilla propia | No | No | Parcial (dados) | No |
| Completar el acorde en una escala (neutrones) | Parcial (Logic limita por escala en Keyboard) | Parcial (memoriza acordes) | No confirmado | No |
| Variaciones reproducibles (misma semilla, misma salida con cualquier buffer) | No documentado | No documentado | No documentado | No documentado |
| Salida por capa: canal MIDI propio, puerto directo, puertos virtuales | No (una salida) | No | No | Parcial (salida MIDI única) |
| Linux / LV2 | No (cada DAW lo suyo) | No (Win/macOS) | No | No |
| Código abierto (AGPL) y gratis | No | No | No | No |
| El átomo como visual | No | No | No | No |

Conclusión: la combinación de polimetría de capas completas con su propio canal y salida, mutación reproducible,
completar acorde, Linux y AGPL no la cubre ningún producto de los revisados. Lo que sí existe y hay que igualar
como mínimo: largo independiente por fila (Cthulhu, PEACH), filas de CC (PEACH) y probabilidad (BLEASS).

Riesgo de comunicación: "polimetría" por sí sola ya la dicen Cthulhu y PEACH. Lo que se diferencia es **capas
completas con ritmo propio, canal propio y fusión visible**, no "carriles con largo propio".

### 1.4 Nombre: Valence (decidido el 2026-10-27)

Búsqueda del 2026-10-27: no aparece ningún plugin ni producto de audio llamado "Valence" ni "Nucleon" (KVR y
búsqueda general). "Isotope" sí existe: un sinte de EVM Synths ([KVR](https://www.kvraudio.com/product/isotope_by_evm_synths)).
El usuario eligió **Valence** (código `Vlnc`; el AU MIDI FX usa `Vlnm`).

---

## 2. Cómo se usan los arpegios en minimal techno, minimal house y drum and bass

Advertencia de origen: casi nada de esta sección tiene una fuente numérica única. Sale de convenciones de los
géneros y de lo que ya sabemos de nuestros sintes. Las fuentes que sí confirman partes están citadas. Los valores
son **puntos de partida para presets y demos**, para ajustar de oído.

### 2.1 Tempos

| Género | Rango habitual | Valor para demos | Notas |
|---|---|---|---|
| Minimal techno | 124-132 BPM | 128, 130 | Bombo cuatro al piso, hats sincopados |
| Dub techno | 118-126 BPM | 124 | Acordes con delay largo |
| Minimal house | 118-126 BPM | 120, 122, 124 | Más suelto y con más swing |
| Drum and bass (liquid, general) | 170-175 BPM | 172, 174 | Una fuente lo ubica en "usualmente 170+" |
| Halftime | grilla 160-175, sensación 80-87 | 172 (sensación de 86) | Los detectores de tempo suelen leer 87 en un tema de 174 |

Fuentes: [DnB Forum](https://dnbforum.com/threads/tempo-of-dnb-jungle.191069/), [Mixgraph, half-time](https://www.mixgraph.io/glossary/half-time), [Loopmasters, halftime](https://loopmasters.com/genres/157-Halftime/products/11361-Halftime-Neuro-Drum-Bass) (son listados comerciales y foros, no referencias autorizadas). Techno/house: `[criterio propio]`.

### 2.2 Figuras y largos de capa

- **Figura base:** 1/16 en techno, house y dnb. 1/8 para las capas "lentas" o de fondo. 1/32 sólo para ratchets y rolls
  (a 180 BPM un 1/32 son unos 83 ms).
- **Tresillos (1/8T, 1/16T):** marimbas y campanas en house; en dnb liquid dan el "balanceo" sobre el break.
- **Largos impares y polimetría:** la práctica es dejar una capa en 16 (ancla) y poner otras en 15, 13, 12, 7, 5. Con
  16 + 15 + 12 el realineo total es mcm(16,15,12) = 240 pasos (15 compases de 4/4 a 1/16; a 128 BPM son unos 28 s).
  Con 16, 15 y 13 el mcm es 3120 pasos (prueba 5 del plan). Con 23 y 29 (halftime) el mcm es 667 pasos.
- **Primos recomendados en la interfaz:** 5, 7, 11, 13, 17, 19, 23, 29, 31 (los primos no comparten divisores, así que
  el realineo es el producto: máximo desfasaje por la menor cantidad de pasos).
- **Ritmo euclidiano** (`E(k,n)`) para llenar pasos: E(5,16), E(7,16), E(3,8), E(5,12) y E(7,12) se oyen "naturales"
  `[criterio propio]`. El corrimiento (rotación) mueve el acento sin cambiar la densidad.
- **Conteo de capas por género:** minimal techno 2-3 capas activas; house 2; dnb 2 (más una de sub que casi no arpegia).

### 2.3 Swing

| Contexto | Swing sugerido | Nota |
|---|---|---|
| Minimal techno | 50-54 % | Casi recto; el groove sale de los acentos y la polimetría |
| Minimal house | 54-60 % | 58 % para "Bubble", más alto para "Late Shuffle" |
| Dub techno | 50-52 % | Recto; el movimiento lo pone el delay |
| Dnb | 50-54 % en 1/16 | El swing en dnb suele venir del break; las capas, casi rectas |
| Tresillos | 66,7 % (swing matemático de tresillo) | No sumar swing encima de un tresillo |

Consejo de capa contra batería (confirmado por un consejo de Roland): poner el swing un poco más alto en la batería
que en la línea del bajo/arpegio para obtener un empuje y tirón. Fuente: [Roland, acid bassline](https://articles.roland.com/beyond-acid-pushing-the-tb-303-into-new-sonic-territory/).

### 2.4 Gate

| Uso | Gate (como fracción del paso) |
|---|---|
| Pluck / marimba / campana | 25-45 % |
| Bajo con swing (house) | 40-60 % |
| Stab de acorde (dub) | 15-30 % (corto, el delay hace el cuerpo) |
| Acid con slide | 100 % o más (ligadura) en los pasos con slide, 40-50 % en el resto |
| Reese / sub | 90-100 % (sostenido, ratchet sólo en finales de frase) |

`[criterio propio]`, salvo la nota de acid: los slides unen dos notas y funcionan mejor contrastados con notas
cortas ([Roland](https://articles.roland.com/beyond-acid-pushing-the-tb-303-into-new-sonic-territory/)).

### 2.5 Notas fantasma por velocidad y probabilidad

- **Velocidad:** acento 110-127, normal 80-100, fantasma 25-50. Un patrón de 16 pasos típico: acentos en 1, 5 (ó 7), 11;
  fantasmas en los pasos débiles entre ellos.
- **Probabilidad:** pasos principales 100 %; pasos de relleno 35-70 %; ghost 20-45 %. Con semilla fija el resultado es
  reproducible (el diferencial de Valence).
- **Regla de mutación:** cambiar 1 paso cada 2-4 ciclos de la capa más larga; no tocar el paso 0 de la capa ancla.

### 2.6 CC por paso (filtro)

- Es la herramienta típica del minimal techno: una fila de CC74 (cutoff) con valores que oscilan en un rango acotado
  (por ejemplo 40-90 sobre 127), de modo que el filtro "respire" con el largo de la capa. Si la fila de CC tiene otro
  largo que las notas (por ejemplo CC en 12 y notas en 16), el filtro nunca cae igual sobre la misma nota: eso es lo que
  se busca.
- Acid: acento sube cutoff y volumen a la vez; abrir el filtro gradualmente con el tiempo, o dejarlo en un rango medio.
  Fuentes: [Roland](https://articles.roland.com/?p=92325), [Computer Music](https://everand.com/article/449092584/Composition) (las fuentes no coinciden en el método).
- CC útiles: 74 (cutoff), 71 (resonancia), 1 (mod wheel), 91/93 (envíos). Mensajes de CC cuentan en el tráfico MIDI
  (prueba 9).

### 2.7 Acid: acento y slide

- 16 pasos, acentos en 3-4 pasos, slides en 2-3, ratchet sólo en el último paso para llevar al compás siguiente.
- Slide = ligadura del plan (nota se funde con la siguiente sin note-off intermedio): la duración dura hasta el
  note-on siguiente y el sinte (AT303) hace el portamento. Documentar en el manual si la ligadura se envía como
  solapamiento (nota nueva antes del note-off viejo) o como legato en el sinte.
- Un acorde corto de 3 notas a 130 BPM con acentos produce el efecto "acid shell" de la demo.

### 2.8 Dub: acordes con delay

- Patrón "acorde completo" (stab) con silencios y probabilidad; gate corto (15-30 %).
- El delay hace el trabajo: **corchea con puntillo** es el tiempo que más se repite en los tutoriales; feedback bajo a
  medio (los foros hablan de 25-30 % con 30-40 % wet), y a veces tiempo no sincronizado para acercarse al hardware
  original. Fuentes: [Attack Magazine, Basic Channel style](https://www.attackmagazine.com/technique/beat-dissected/basic-channel-style-dub-techno/), [Attack, dub delays](https://www.attackmagazine.com/technique/tutorials/creating-dub-delays-with-standard-plugins/), [Audiotent](https://www.audiotent.com/production-tips/dub-techno-chord-sound-design), [foro Ableton](https://forum.ableton.com/viewtopic.php?p=990796).
- Un acorde menor es lo habitual para el stab (Audiotent). Mutación lenta de probabilidades para que el ritmo del
  stab se corra de a poco.

### 2.9 Rolls y ratchets

- Ratchet 2 = dos 1/32 en un 1/16; ratchet 3 = tresillo de 1/32; ratchet 4 = cuatro 1/64. En dnb a 174 BPM, 1/32 con
  ratchet 4 = 8 notas por 1/8, unas 46 por segundo por capa: se acerca al límite de tráfico (prueba 9).
- Usos: último paso de la frase (techno), relleno cada 4 compases (dnb), velocidad decreciente en la ráfaga (efecto
  de "rebote").
- El ratchet también puede llevar velocidad decreciente (100 %, 80 %, 60 %, 40 %): opción a evaluar.

### 2.10 Valores sugeridos por demo

| Demo | BPM | Capa | Figura | Largo | Patrón | Swing | Gate | Extras |
|---|---|---|---|---|---|---|---|---|
| Phase Drift | 128 | 1 | 1/16 | 16 | arriba-abajo | 52 % | 35 % | CC74 (40-90) con largo 12 |
| | | 2 | 1/16 | 15 | como se tocó | 52 % | 30 % | vel 70-95, prob. 70 % en pasos débiles |
| | | 3 | 1/16 | 12 | al azar | 52 % | 30 % | octava +1 en pasos pares |
| Acid Shell | 130 | 1 | 1/16 | 16 | nota pedal (más grave) | 54 % | 45 %, slide 100 % | 4 acentos (vel 120), 2 slides, ratchet 3 en el paso 16 |
| | | 2 | 1/16 | 7 | acorde completo (stab) | 54 % | 25 % | prob. 85 % |
| Dub Chord | 124 | 1 | 1/8 | 16 | acorde completo | 50 % | 20 % | prob. 60 %, mutación 1 cada 3 ciclos |
| | | 2 | 1/4 | 4 | nota pedal | 50 % | 90 % | sub de ATMD |
| Bubble | 122 | 1 | 1/16 | 16 | octavas alternadas | 58 % | 40 % | vel por paso 85-115 |
| | | 2 | 1/8T | 12 | arriba, corrimiento de índice +1 | tresillo | 35 % | marimba |
| Jack Groove | 124 | 1 | 1/16 | 16 | nota pedal | 56 % | 50 % | ghost: vel 30-45, prob. 40 % en 5 pasos |
| | | 2 | 1/8 | 8 | arriba-abajo | 56 % | 45 % | órgano de 2 notas |
| Late Shuffle | 120 | 1 | 1/16 | 16 | arriba | 60 % | 40 % | completar: séptima, escala menor |
| | | 2 | 1/8 | 12 | nota pedal | 60 % | 60 % | bajo |
| Liquid Rolls | 174 | 1 | 1/16 | 7 | arriba-abajo | 52 % | 30 % | campanas FM, octavas 2 |
| | | 2 | 1/8T | 11 | al azar | tresillo | 30 % | prob. 75 %, ratchet 2 en el paso 11 |
| Reese Engine | 172 | 1 | 1/16 | 16 | nota pedal | 50 % | 90 % | CC74 por paso, ratchet 4 en pasos 8 y 16 |
| | | 2 | 1/16 | 5 | acorde completo | 50 % | 15 % | stab corto |
| Halftime | 172 | 1 | 1/8 | 23 | caminata al azar | 50 % | 55 % | mutación lenta, semilla fija |
| | | 2 | 1/8 | 29 | arriba | 50 % | 55 % | octavas 2, vel 60-90 |

Todos son punto de partida: la verificación es de oído en el checkpoint 1 del plan.

---

## 3. Logic y el AU MIDI FX

Decisión ya tomada por el usuario: el 1.0 incluye un AU MIDI FX (`aumi`) además del AU instrumento.

### 3.1 Versión de JUCE

`external/JUCE/modules/juce_core/system/juce_StandardHeader.h:42-44`: JUCE **9.0.4**
(`JUCE_MAJOR_VERSION 9`, `JUCE_MINOR_VERSION 0`, `JUCE_BUILDNUMBER 4`).

### 3.2 Cómo JUCE construye un MIDI FX (con ruta y línea)

Todo en `external/JUCE/extras/Build/CMake/JUCEUtils.cmake`:

- `:1849`: `IS_MIDI_EFFECT` vale FALSE por defecto.
- `:1630`: la propiedad se traduce a la macro `JucePlugin_IsMidiEffect`.
- `:1896-1907`: selección del tipo AU principal. Si `IS_MIDI_EFFECT` es verdadero, `AU_MAIN_TYPE =
  kAudioUnitType_MIDIProcessor`; si no, y `IS_SYNTH`, `kAudioUnitType_MusicDevice` (`aumu`); si no, y entra MIDI,
  `MusicEffect` (`aumf`); si no, `Effect` (`aufx`). **Importante:** la rama MIDI FX va primero, así que con
  `IS_MIDI_EFFECT TRUE` + `IS_SYNTH TRUE` gana `aumi`.
- `:1912-1923` y `:1936-1948`: tabla que mapea `kAudioUnitType_MIDIProcessor` al código de cuatro letras `'aumi'`
  (cuarta posición de la lista) y lo guarda en `JUCE_AU_MAIN_TYPE_CODE`.
- `:1642`: ese código llega al wrapper como `JucePlugin_AUMainType`.
- `:1952`: en el mismo bloque, `IS_MIDI_EFFECT` también cambia la categoría AAX a `MIDIEffect`.
- `docs/CMake API.md:550-552`: `IS_MIDI_EFFECT` = "algunos hosts dan una ubicación especial en el canal para
  plugins MIDI".

Wrapper AU (`external/JUCE/modules/juce_audio_plugin_client/juce_audio_plugin_client_AU_1.mm`):

- `:91`: `pluginIsMidiEffect = JucePlugin_AUMainType == kAudioUnitType_MIDIProcessor`. O sea que el wrapper decide por
  el **tipo AU**, no por `JucePlugin_IsMidiEffect`.
- `:94-106`: si es MIDI FX, `pluginProducesMidiOutput` y `pluginWantsMidiInput` son verdaderos aunque no se declaren.
- `:291`: `BusCountWritable` devuelve falso (no se pueden agregar buses).
- `:1558-1560`: sale el MIDI con `pushMidiOutput` al final de `render`.
- `:1577-1596`: el MIDI entrante llega por `HandleMIDIEvent` y `HandleSysEx`.
- `:1278`: el playhead se obtiene con `CallHostBeatAndTempo` (ver 3.4).

### 3.3 Hace falta un target aparte?

**Sí.** El tipo AU (`AU_MAIN_TYPE`) es una propiedad del target (`JUCE_AU_MAIN_TYPE`, `JUCEUtils.cmake:1896-1948`), y un
target produce un solo componente AU. El proyecto actual de Orrery declara `IS_SYNTH TRUE` / `IS_MIDI_EFFECT FALSE`
(`plugins/orrery/CMakeLists.txt:28-31`), por lo que el AU resultante es `aumu`.

Para sumar `aumi` hay que declarar un **segundo `juce_add_plugin`** (por ejemplo `ValenceMidiFx`) con:

- `FORMATS AU` solamente (el VST3 y el Standalone no se duplican).
- `IS_SYNTH FALSE`, `IS_MIDI_EFFECT TRUE`, `NEEDS_MIDI_INPUT TRUE`, `NEEDS_MIDI_OUTPUT TRUE`.
- `PLUGIN_NAME` distinto (por ejemplo "Audio Tools Valence MIDI") y el mismo `PLUGIN_MANUFACTURER_CODE Atls`. El
  `PLUGIN_CODE` (subtype) puede repetirse porque lo que identifica un AU es la terna tipo/subtipo/fabricante y el tipo
  difiere (`aumi` vs `aumu`), pero conviene usar un código distinto por claridad y evitar caché de Logic.
- `BUNDLE_ID` propio (por ejemplo `com.audiotools.ValenceMidiFx`).
- Mismo código fuente del procesador (se compila el mismo `PluginProcessor` con `target_sources`), pero **sin
  Preview** y sin salidas de audio útiles: el `aumi` sólo transforma MIDI.

Alternativa sin segundo target: definir `AU_MAIN_TYPE kAudioUnitType_MIDIProcessor` en el único target, pero
perderíamos el AU instrumento. No sirve para el requisito del usuario.

Una consecuencia de diseño: el MIDI FX es un binario aparte que comparte el motor. Que `processBlock` y el estado sean
idénticos entre los dos targets es parte de las pruebas (sección 4).

### 3.4 Problemas conocidos

| Tema | Qué se sabe | Fuente |
|---|---|---|
| auval y buses | Un AU sólo-MIDI falló auval con "Default Format of unit does not match reported Channel handling capabilities". Causa: el wrapper le daba un bus de salida a los MIDI FX pero reportaba 0 canales; el bus hace falta para conocer la frecuencia de muestreo. Fix en JUCE `develop` de junio 2024 ("AU Client: Report correct AUChannelInfo for MIDI FX plugins", reporta -1 canales de salida). Antes se esquivaba con la configuración de canales `{0, 2}`. **No pude confirmar por grep que el fix esté en 9.0.4** (el wrapper arma el channel info con `AudioUnitHelpers::getAUChannelInfo`, `AU_1.mm:175`); queda como prueba de la primera build: correr auval sobre el `aumi`. | [foro JUCE](https://forum.juce.com/t/a-au-plugin-has-error-with-auval/55049) |
| Tipo `aumi` | Logic y otros hosts buscan los MIDI FX por el tipo `aumi`; con el tipo equivocado el host no lo encuentra. El ejemplo de referencia de JUCE es el Arpeggiator (`aumi Arpg ROLI`): `auval -v aumi <subtype> <fabricante>`. Hay reportes de que Logic no veía el plugin hasta corregir el tipo o rescanear | [foro](https://forum.juce.com/t/au-midi-effect-solved/19510), [foro](https://forum.juce.com/t/au-midi-effect-plugins-from-4-2-4-to-4-3-0/20280), [foro](https://forum.juce.com/t/juce-mididemo-au-not-showing-in-logic-pro/43894) |
| Otros hosts | Live y REAPER no muestran AU MIDI FX; por eso el VST3 sigue siendo el formato de esos hosts | [foro](https://forum.juce.com/t/au-midi-fx-issue-with-the-arpeggiator-demo-project/27158) |
| Audio en MIDI FX | Hubo reportes viejos de audio que no pasaba en un MIDI FX para Logic (2014) | [foro](https://forum.juce.com/t/midi-fx-for-logic-pro-x-audio-does-not-work/14190) |
| Playhead / transporte | El wrapper AU toma posición y tempo con `CallHostBeatAndTempo` (`AU_1.mm:1278`). Que Logic entregue esa información a un MIDI FX **no está confirmado**: hay que medirlo con un MIDI FX de prueba (posición en pulsos avanza, salto al lazar, parada). Si no hay playhead, el modo es reloj propio con el tempo del host o el último conocido | medición pendiente |
| Buffer de silencio | El wrapper igual llama a `render` con un buffer de audio. Un `aumi` no debe producir audio; limpiar el buffer al terminar y no depender de él | `AU_1.mm:1550-1563` |
| Pregunta abierta sobre el tipo | Si algún host reconoce un AU MIDI que no sea `aumi` (consulta de un usuario, sin respuesta) | [foro](https://forum.juce.com/t/juceplugin-ismidieffect-and-aumi-au-type-kaudiounittype-midiprocessor/63855) |

### 3.5 Cómo ruta Logic la salida de un MIDI FX

- La salida MIDI de un MIDI FX en la ranura MIDI FX llega **sólo al instrumento del mismo canal de la consola**. Logic
  no saca MIDI de los AU instrumento; sólo de los AU MIDI FX de esa ranura (reporte de usuarios, no oficial).
  Fuentes: [KVR, Logic and AU instrument midi out](https://www.kvraudio.com/forum/viewtopic.php?p=8798094), [VI-Control](https://vi-control.net/community/threads/send-midi-to-outside-world-in-logic.132280/).
- Consecuencia: **no se puede mandar cada capa a un sinte distinto desde un MIDI FX** en Logic. Con un instrumento
  multitímbrico, todo el MIDI debe pasar por la única ranura MIDI FX de ese canal
  ([foro VSL](https://forum.vsl.co.at/post/297998)). El AU "no soporta salida MIDI multicanal" según el foro de
  Scaler ([Scaler Multi Out en Logic](https://forum.scalermusic.com/t/scaler-multi-out-on-logic-pro/12585)).
- Dentro de un solo canal sí funciona: las 4 capas con **canales MIDI distintos** hacia un sinte multitímbrico
  (por ejemplo un Kontakt multi). Es el caso del "canal por capa" ya previsto.
- **Workaround para sintes separados:** usar el driver IAC de macOS. La pista fuente lleva un instrumento "External
  Instrument" que envía a IAC Driver Bus 1, y las pistas destino reciben de IAC en canales distintos. Hay reportes
  mixtos (a algunos les funciona, a otros no) y riesgo de bucle de realimentación.
  Fuentes: [Sound On Sound, extraer datos MIDI](https://www.soundonsound.com/techniques/logic-pro-x-extracting-midi-plug-data), [Guía de ruteo de Scaler 3](https://scalermusic.com/wp-content/uploads/2025/06/Scaler-3-MIDI-Output-DAW-Routing-Guide.pdf), [KVR](https://www.kvraudio.com/forum/viewtopic.php?p=6752086).
- En el VST3 (instrumento) ya tenemos los puertos virtuales por capa de macOS: en Logic se pueden usar como IAC
  propio, sin pasar por el driver IAC, con pistas de "External MIDI" o instrumentos que escuchan ese puerto. A
  verificar en la primera build.
- Apple: Logic puede canalizar MIDI a los canales 1-16 y mandarlo a puertos de salida específicos, pero separar
  dispositivos requiere una interfaz multipuerto ([Apple, Logic Pro 10.1](https://help.apple.com/logicpro/mac/10.1/en.lproj/lgcpebe9781c.html)).

### 3.6 Qué decir en el manual ("In Logic")

1. Con el AU MIDI FX en la ranura MIDI FX del canal de un instrumento: todas las capas van a ese instrumento. Para
   usarlas por separado: canal por capa más un sinte multitímbrico.
2. Cada capa a un sinte distinto: usar el AU instrumento de Valence (con su Preview o el MIDI de salida por
   puertos virtuales) y la receta de puertos virtuales o IAC.
3. Aclarar que el MIDI FX no incluye el sonido de prueba.

### 3.7 Pruebas del `aumi` (se agregan a la sección 4)

- `auval -v aumi <subtype> Atls` sin errores ni advertencias de canales (ver 3.4).
- Logic: aparece en la ranura MIDI FX; con un acorde apretado, el instrumento del canal suena; al parar el
  transporte no quedan notas colgadas; con el transporte detenido y sin playhead, el plugin no se cuelga.
- Playhead de Logic dentro del MIDI FX: registrar posición, tempo y salto en pulsos con el lazo de Logic.
- El motor da la misma huella en el target `aumu` y en el `aumi`.

---

## 4. Plan de pruebas

Herramientas: `tools/valencetest.cpp` (pruebas numeradas, sin JUCE), `valencerender` (`--bench`),
`valencemiditest`, `tools/presets.py`. Los criterios son concretos y verificables; "huella" es un hash de la secuencia
de eventos (nota, velocidad, CC, muestra, canal), como `60e8a62919b73c14` en Orrery.

### 4.1 Las 12 pruebas de la Fase 1

| # | Prueba | Criterio de aprobado |
|---|---|---|
| 1 | Cada patrón da la secuencia exacta sobre acordes conocidos (incluidos 1, 2 y 7 notas) | Para cada patrón (arriba, abajo, arriba-abajo con y sin extremos repetidos, converger, divergir, como se tocó, azar, caminata, stab, nota pedal, orden de dedos) y para los acordes de 1, 2, 3 y 7 notas, la secuencia de 2 ciclos coincide con una tabla escrita a mano; 0 diferencias. Con 1 nota no hay error ni división por cero |
| 2 | Determinismo | Para buffers 64, 333, 441, 512, 1024 y frecuencias 44,1/48/96 kHz, los eventos (nota, velocidad, CC, muestra) son idénticos byte a byte; huella impresa y **la misma en macOS arm64, macOS x86_64, Windows x64 y Linux x86_64**. Un bit de diferencia = fallo |
| 3 | Grilla | Cada note-on cae en la muestra calculada de su paso, desvío 0 (±0), con swing de 50-75 % y tresillos, en los tres tempos de las demos (120, 128, 174) y con tempo no entero (127,35) |
| 4 | Notas colgadas | Todos los casos de la sección 5 del plan: soltar, apagar latch, parar transporte, bypass, cambio de preset, apagar o cambiar de canal una capa a mitad de nota, pánico. Al final del caso, cada note-on tiene su note-off (contador por canal y nota = 0). Prueba extra: 100 casos al azar con semilla fija |
| 5 | Polimetría | Capas de 16, 15 y 13 pasos se realinean exactamente en el paso 3120; el contador de fusión predice el compás con 0 de error; ni un realineo antes (paso 0 < k < 3120 sin coincidencia de las tres) |
| 6 | Latch, sustain, acorde nuevo con y sin reinicio | Latch retiene al soltar; CC64 actúa como latch; acorde nuevo con reinicio vuelve al paso 0 de cada capa en la muestra exacta; sin reinicio mantiene la fase (índice de paso idéntico antes y después) |
| 7 | Ratchets y ligaduras | Largos correctos (ratchet 2/3/4 divide el paso en partes iguales ±0 muestras); ninguna nota repetida sin note-off en medio (el mismo canal y nota); ligadura con solapamiento documentado y consistente |
| 8 | Completar | En 10 minutos de sesión simulada, con cada escala y tónica soportada, ninguna nota fuera de la escala **entre las notas agregadas** (las que toca la persona pueden estar fuera: no se corrigen) |
| 9 | Tráfico MIDI | A 1/32 con ratchet 4 a 180 BPM: mensajes por segundo y por canal medidos y reportados; el máximo se compara con ~1000 msg/s de un cable DIN. Si lo supera, el plugin avisa o limita (decidir y documentar) |
| 10 | Transporte | Lazo del host (salto atrás), pre-roll de REAPER, cambio de tempo a mitad de compás, parada y arranque: la salida coincide con la de un arranque limpio desde ese punto; Play = sesión nueva, anclada a la grilla de 1/16 de pulso. Molde: gentest 17 de Orrery |
| 11 | Reloj MIDI y Link | Reloj MIDI de salida: 24 pulsos por negra con desvío ≤ 1 muestra acumulado en 10 minutos; Link: dos instancias en fase al paso (molde Orrery) |
| 12 | CPU del motor | `valencerender --bench`: tiempo del motor solo por segundo de audio, reportado; objetivo: bajo el 0,5 % de un núcleo con las 4 capas y ratchet (valor a fijar con la primera medición; el presupuesto del editor aparte es 3 % M4 / 6 % Ryzen) |

Pruebas que se suman (propias de Valence):
- 13. Mutación: con la misma semilla, la mutación produce la misma secuencia con cualquier buffer; con semilla
  distinta cambia al menos un paso en 8 ciclos.
- 14. Nota de entrada: una nota apretada a mitad de bloque se aplica en su muestra exacta, no al principio.
- 15. Estado: serialización estable byte a byte (`statebytes`), carga/guardado de las filas de pasos.
- 16. Equivalencia `aumu`/`aumi`: misma huella en los dos targets (sección 3.7).

### 4.2 QA de host

| Qué | Cómo | Criterio |
|---|---|---|
| athosttest | Modos `hostgap`, `statebytes`, `repro`, `zipper`, `progorder`, `ports` | Todos en OK. `hostgap`: bypass sin notas colgadas (REAPER no llama a un plugin en bypass, `dh::HostGap` al inicio de `processBlock`). `repro`: dos corridas idénticas. `zipper`: sin saltos al automatizar. `progorder`: instancia nueva = programa 0. `ports`: puertos virtuales aparecen y reciben |
| pluginval | Strictness 10, VST3 y AU (y LV2 en Linux si aplica) | 0 fallos. Cualquier fallo en strictness 10 bloquea |
| auval | `auval -v aumu <code> Atls` (instrumento) y `auval -v aumi <code> Atls` (MIDI FX) | "AU VALIDATION SUCCEEDED" en los dos, sin errores de formato de canales |
| REAPER | Arnés en `audiotools-private/qa/daw-2026-10-22/` | Cada preset aparece dos veces (documentado); misma salida con cualquier buffer; sin notas colgadas al parar |
| Live | Mismo arnés. Se abre sólo si está cerrado y el usuario lleva más de 10 minutos inactivo. Caso real: arpegiador en una pista MIDI y dos sintes en otras pistas tomando su MIDI (todas las capas al mismo sinte, y por canal en un sinte multitímbrico) | El MIDI llega por VST3; el log de Ableton tiene claves de licencia: sólo `grep` |
| Logic | Sólo si hay una Mac con Logic a mano: `aumu` en una pista de instrumento y `aumi` en la ranura MIDI FX | Ver 3.7 |
| Parámetros | ≤ 64 automatizables; `JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0` | Live arma su panel con los primeros 64 |

### 4.3 Dónde corre cada cosa

| Qué | Dónde | Por qué |
|---|---|---|
| Compilar el motor, `valencetest`, `valencemiditest`, `valencerender` (corridas cortas), builds de desarrollo arm64 | Mac mini M4 (`nice -n 10 cmake --build ... -j 3`, máximo 2 agentes) | Liviano; no saturar la Mac |
| Huella en Linux y barrido de buffers/frecuencias largo, `--bench` estable | `pc50-wsl` (24 núcleos) o Docker VM 110 (`ssh docker`) | Pesado; la huella debe ser la misma en los cuatro sistemas |
| Huella y build en Windows | `ssh pc50` (`rel.ps1`) | Prueba 2 en Windows x64 |
| Huella en macOS x86_64 | Mac con `arch -x86_64` o Rosetta | Prueba 2 |
| pluginval y athosttest | Mac (arm64) y Linux en Docker (`QA=1 sh scripts/build-linux.sh valence`) | Tres sistemas |
| auval | Mac | Sólo existe en macOS |
| REAPER y Live | Mac, con los arneses | Live con la regla de los 10 minutos |
| Renders de demos, mezclas, video, Blender, film | `pc50-wsl` (Blender siempre bajo `flock ~/ai/prerendered/.gpu.lock`; no tocar ComfyUI ni `~/ai`) | Pesado; procesos largos con `setsid nohup ... < /dev/null &` |

Cuidado con `pkill -f`/`pgrep -f` con texto que esté en el propio comando (mata la sesión SSH).

### 4.4 Orden sugerido

1. `valencetest` 1-8 y 13-14 en la Mac mientras se escribe el motor.
2. Huella en las otras tres plataformas apenas haya prueba 2 estable (en pc50, pc50-wsl y Docker).
3. Pruebas 9-12 con el motor cerrado.
4. Plugin funcional con editor simple: athosttest, pluginval 10, auval (`aumu` y `aumi`).
5. Arneses de REAPER y Live.
6. Demos en pc50 y checkpoint 1.

---

## 5. Pendientes que salen de esta investigación

- Verificar con WebSearch que el nombre "Valence" no choque (sección 1.4) y consultarlo con el usuario.
- Confirmar por manual: gate/retrigger de Live, ReaArp/JSFX de REAPER, Bitwig; y ratchet en Cthulhu, PEACH y BLEASS.
- Confirmar si el fix de AUChannelInfo para MIDI FX está en JUCE 9.0.4 corriendo auval sobre el primer `aumi`.
- Medir si Logic entrega playhead al MIDI FX.
- Decidir si el segundo target `aumi` lleva su propio `PLUGIN_CODE`.
- Decidir la política de ratchet con velocidad decreciente y de ligadura (solapamiento vs. legato).

Fuente de las búsquedas: WebSearch standard, 2026-10-27. Foros (JUCE, KVR, VI-Control, Ableton) son reportes de
usuarios; verificar en el producto antes de afirmarlo en público.
