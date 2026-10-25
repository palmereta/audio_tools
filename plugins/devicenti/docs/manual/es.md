# Manual del DeVicenti

## Qué es el DeVicenti

El DeVicenti es una groovebox de FM: cuatro pistas, un secuenciador de 64 pasos y un camino de sonido hecho para música lenta, pesada y oscura. Todo el instrumento es una sola unidad naranja que tocás y programás desde su panel. Es un diseño original, no el modelo de ninguna máquina existente.

Cada una de las cuatro pistas es una de tres máquinas:

- **TONE**: una voz de FM de cuatro operadores con ocho algoritmos, un filtro saturante, una capa de sub, unison y una capa de ruido. Es polifónica.
- **KIT**: una caja de ritmos de ocho voces de FM (bombo, redoblante, clap, hi-hat cerrado, hi-hat abierto, tom grave, tom agudo, percusión), cada una un pequeño tambor de FM con sus propios pasos, locks y condiciones.
- **MIDI**: la pista manda sus notas, acordes y locks a un canal MIDI, así el DeVicenti puede tocar un sinte externo.

Por dónde pasa el sonido:

- Un filtro saturante de seis formas, **DRIVE** y **FOLD**, y **AGE**, una etapa de cinta que agrega wow, flutter, deriva y soplido.
- Un insert por pista: overdrive y fold, bitcrush y lo-fi, chorus y ensemble, phaser, modulador en anillo, o un resonador de peine afinado a la raíz del patrón.
- Un delay (TAPE, DUB o PING-PONG) y una reverb (SPRING, PLATE, HALL o VOID, con FREEZE y SHIMMER).
- Un bus master con **TAPE**, **GLUE**, **DUCK** y **LIMIT**.

Qué tiene el secuenciador:

- 16 patrones de hasta 64 pasos, con step locks en casi todos los parámetros, condiciones de paso y probabilidad, micro timing, retrigs, slides y ligaduras.
- Modo de acordes con acordes que respetan la escala, voicings e inversiones, ranuras de memoria de acordes, un strum y un arpegiador.
- Una escala por patrón, largo, velocidad y swing por pista, una cadena de canción, y salto directo entre patrones.
- MIDI de entrada y de salida: tocalo desde un controlador, grabá en él, y dejá que maneje otros instrumentos desde un DAW.

Trae 82 presets, cada uno un proyecto entero (sonidos, patrones y efectos) en bancos con el nombre de la música para la que están hechos: Trip Hop, Downtempo, Acid Jazz, Ambient, Dark Ambient y Heavy.

El DeVicenti funciona como VST3 en macOS, Windows y Linux, como Audio Unit en macOS, como LV2 en Linux y como aplicación independiente. Es un instrumento: se toca desde una pista MIDI. Es software libre y de código abierto bajo la GNU AGPL v3.

## Instalación

Cada zip incluye un `INSTALL.txt` con estos pasos.

### macOS

1. Descomprimí el archivo.
2. Copiá `DeVicenti.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `DeVicenti.component` a `~/Library/Audio/Plug-Ins/Components/`.
3. Volvé a escanear los plugins en tu DAW. En Ableton Live: Settings, Plug-Ins, Rescan.

`DeVicenti.app` es la versión independiente.

> Los binarios están firmados ad hoc y no notarizados. Si macOS se niega a abrirlos, corré esto en Terminal: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/DeVicenti.vst3 ~/Library/Audio/Plug-Ins/Components/DeVicenti.component`

### Windows

1. Descomprimí el archivo.
2. Copiá la carpeta `DeVicenti.vst3` a `C:\Program Files\Common Files\VST3\`. Windows pide permisos de administrador.
3. Volvé a escanear los plugins en tu DAW.

`DeVicenti.exe` es la versión independiente y corre sin DAW.

> Los binarios no están firmados. SmartScreen de Windows puede pedirte que confirmes la primera vez.

### Linux

Copiá `DeVicenti.vst3` a `~/.vst3/` y el bundle LV2 a `~/.lv2/`, y volvé a escanear en tu host.

## Cargar el DeVicenti en un DAW

El DeVicenti aparece con el nombre de fabricante **Audio Tools**, entre los instrumentos, como Audio Tools DeVicenti. Cargalo en una pista MIDI. Abre con el primer preset, Init: un kit en la pista 1 y tres pistas tone, a 88 BPM.

- **El secuenciador sigue al host.** En un DAW el transporte y el tempo son los del host: apretá play en el DAW y los patrones corren. El PLAY y el TEMPO de la unidad sólo funcionan en la app independiente.
- **Salida de sonido.** La salida estéreo principal lleva todo. Cada pista también puede ir a su propia salida estéreo (página MIX, Output): activá las salidas extra del plugin en tu DAW y mandá esa pista ahí para mezclarla aparte.
- **Entrada.** Las notas que llegan a la entrada MIDI de la pista tocan el DeVicenti como cualquier instrumento (ver MIDI de entrada).
- **MIDI de salida.** El DeVicenti también produce MIDI, para manejar otros instrumentos (ver MIDI de salida).

La app independiente abre el editor en su propia ventana. Elegí tus dispositivos de audio y MIDI en sus opciones. Ahí la unidad tiene su propio reloj: PLAY lo arranca y lo frena, y TEMPO fija la velocidad.

## Primer sonido

1. Apretá una flecha del preset en la barra superior, o hacé clic en el nombre y elegí uno del menú. Probá **Basement Boom** en Trip Hop.
2. Arrancá el transporte de tu DAW (o PLAY en la app independiente). El patrón corre y el LED rojo sobre las teclas STEP sigue la posición.
3. Hacé clic en **TRACK 2**, **TRACK 3** y **TRACK 4**, una tras otra: los operadores, el filtro y las teclas STEP muestran esa pista. Girá **CUTOFF** y **DRIVE** mientras suena.
4. Mantené apretada la tecla **Shift** del teclado de la computadora (o hacé clic en **FUNC**) y hacé clic en **TRACK 1**: el kit se silencia. Repetilo para devolverlo.
5. Girá **DELAY** y **REVERB**: mandan la pista actual a los efectos.

## Recorrido por la interfaz

![El DeVicenti con el preset Basement Boom: la unidad en perspectiva bajo la barra superior.](img/overview.jpg)

### Barra superior

- **Navegador de presets** en el medio: las flechas recorren los presets y un clic en el nombre abre el menú, agrupado por banco. El menú también guarda y vuelve a leer tus propios presets.
- **LEARN**: arma el aprendizaje MIDI. Hacé clic, hacé clic en una perilla y mové un control de tu teclado o controlador.
- **PAGES**: abre las páginas de atrás, la pantallita agrandada como editores completos (ver Páginas de atrás).
- **MIDI**: la página MIDI.
- **SETTINGS**: el tamaño de la ventana y un recordatorio de cómo usar el panel con el mouse.
- **SAVE**: guarda el estado actual como preset de usuario.
- **MANUAL**: abre este manual dentro del plugin, en inglés o en español. Escape o la cruz lo cierra.

En una página de atrás, **PANEL** a la izquierda de la barra vuelve a la unidad.

### La unidad

La unidad está dibujada en perspectiva. De arriba hacia abajo: las perillas de velocidad y swing, el bloque celeste con las perillas **ALGORITHM** y **DATA** y la pantallita, las perillas LFO y MASTER; la fila de teclas; cuatro bloques de operadores (A, B, C y D) en una grilla, cada uno con cinco perillas y su propia pantallita; la fila de abajo con el filtro, DRIVE y AGE, y los envíos DELAY y REVERB; y las dieciséis teclas STEP en el borde de adelante, cada una con un LED encima.

La ventana mantiene sus proporciones y escala del 60 al 160 por ciento. La página SETTINGS tiene botones de 75, 90, 100, 125 y 150 por ciento.

### Trabajar con los controles

- **Perillas**: arrastrá hacia arriba o hacia abajo, mantené Shift para moverlas despacio, usá la rueda del mouse encima, o hacé doble clic para volver al valor por defecto. Las perillas por pasos (ratio, type, algorithm) avanzan una posición por arrastre corto.
- **Teclas**: hacé clic. Una tecla que queda prendida está activa.
- **Lectura del valor**: mientras girás una perilla, su nombre y su valor aparecen al lado y en la pantallita.
- **Clic derecho** en una perilla abre el menú: MIDI learn, borrar los controladores aprendidos, valor por defecto y su número de CC. Clic derecho en una tecla: mute y solo (teclas de pista) o sostener y borrar locks (teclas de paso). Clic derecho en la pantallita abre una página.

### FUNC: la segunda capa

Cada perilla y cada tecla tiene una segunda función impresa chica, en óxido, debajo de su nombre. **FUNC** es la tecla de mayúsculas que llega a ella. Hacé clic en FUNC para trabarla (queda prendida hasta que hagas clic de nuevo), o mantené apretada la tecla **Shift** del teclado de la computadora todo el tiempo que la necesites. Escape suelta FUNC.

![FUNC trabado: la segunda capa de cada perilla y tecla está activa.](img/func.jpg)

La segunda capa figura con cada control en las secciones que siguen.

## Pistas y máquinas

Las cuatro teclas **TRACK** eligen qué pista edita el panel. Los operadores, la fila del filtro, DRIVE, AGE, los envíos, el LFO y las teclas STEP siempre son de la pista seleccionada. La pista seleccionada también es la que suena desde tu teclado MIDI cuando la entrada MIDI está en OMNI (ver MIDI de entrada).

- **FUNC + TRACK 1-4**: silencia o devuelve esa pista.
- **Mantener una tecla TRACK y apretar otra**, o **FUNC + Alt-clic** en una tecla de pista: solo.
- **FUNC + ALGORITHM** (impreso MACHINE): elige TONE, KIT o MIDI para la pista seleccionada. El campo Machine de las páginas VOICE y MIX hace lo mismo.

En una pista TONE todas las perillas editan la voz. En una pista KIT las perillas que dan forma al sonido (RATIO, FEEDBACK, DETUNE y ENV de los operadores, TYPE y la capa FUNC de la fila del filtro) editan la **voz seleccionada** (ver Voces del KIT), mientras que las perillas que son parámetros del host (CUTOFF, RESO, el ENV del filtro, DRIVE, AGE, DELAY, REVERB y las cuatro perillas LEVEL de operador) actúan sobre todo el kit.

## Fila de arriba

| Control | Girar | FUNC + girar |
|---|---|---|
| **TEMPO** | Tempo, de 40 a 240 BPM. En un DAW muestra el tempo del host y manda el host. | **SPEED**: la velocidad de la pista seleccionada en este patrón, de 1/8x a 2x |
| **SWING** | Swing de todo el patrón, de 50 a 80 por ciento | **TRK SWING**: cuánto de ese swing toma la pista seleccionada, de 0 a 100 por ciento |
| **ALGORITHM** | El algoritmo de FM de la pista seleccionada, de 1 a 8. Con un paso sostenido, elige el campo del paso a editar. | **MACHINE**: TONE, KIT o MIDI |
| **DATA** | Edita el campo que muestra la pantallita. | **PAGE**: elige la página de la pantallita (VOICE, OPS, FILTER/AMP, LFO, MOD, TRIG, CHORD/SCALE, MIX, FX, MIDI, SONG) |
| **LFO** | Profundidad del LFO 1 de la pista seleccionada, positiva o negativa | **LFO 2**: profundidad del LFO 2 |
| **MASTER** | Nivel de salida de toda la unidad | **TRK LEVEL**: nivel de la pista seleccionada |

El swing por defecto es 56 por ciento, un shuffle perezoso. Pistas distintas pueden correr a velocidades distintas (la polimetría de los presets Ambient usa pistas a 1/2x y 3/4x) y tener largos distintos, así los loops se desfasan entre sí.

La **pantallita** muestra el diagrama del algoritmo con el rol de cada operador, el campo que se edita y la posición. Hacé clic en su mitad izquierda para retroceder un campo y en la derecha para avanzar; DATA edita el campo. Todos los campos de todas las páginas están también en las páginas de atrás, todos juntos.

## Fila de teclas

| Tecla | Apretar | FUNC + apretar |
|---|---|---|
| **TRACK 1-4** | Seleccionar la pista | Silenciarla o devolverla |
| **PLAY** | Reproducir o pausar (independiente) | **RESTART**: desde el paso 1 |
| **STOP** | Parar (independiente); una segunda vez suelta todas las notas | **PANIC**: apaga todas las voces y notas MIDI |
| **REC** | Grabación de grilla prendida o apagada | **STEP REC**: grabación paso a paso prendida o apagada |
| **CHORD** | Modo de acordes prendido o apagado para la pista seleccionada | **SCALE**: abre la página CHORD/SCALE |
| **FUNC** | La tecla de mayúsculas | |

En un DAW el transporte del host arranca y frena el DeVicenti: PLAY y STOP sólo muestran un recordatorio. **REC + PLAY** arranca la grabación en vivo (ver Grabación).

## Los operadores

![Los cuatro bloques de operadores: A y B arriba, C y D abajo.](img/operators.jpg)

Una voz TONE tiene cuatro operadores, **A**, **B**, **C** y **D**, en una grilla de 2 por 2. Los cuatro tienen las mismas cinco perillas. El operador A siempre es portador, el que se oye. Los otros son portadores o moduladores según el algoritmo.

| Perilla | Girar | FUNC + girar |
|---|---|---|
| **RATIO** | Relación de frecuencia, de una tabla de 24 valores: 0.25, 0.5, 0.75, 1, 1.41, 1.5, 2, 2.5, 3, 3.5, 4, 5, 6, 7, 7.07, 8, 9 a 16 | **FINE**: la relación continua, de 0.25 a 16 |
| **LEVEL** | Portador: el nivel de salida. Modulador: cuánto modula (el índice de FM). | **VELOCITY**: cuánto cambia el nivel con la velocity |
| **FEEDBACK** | Autorrealimentación del operador, de 0 a 100 por ciento: lleva una senoidal hacia una sierra | **HARMONICS**: mezcla la senoidal con una forma armónica |
| **DETUNE** | Desafinación, más o menos 50 cents | **KEY SCALE**: hace que el nivel baje o suba con la altura |
| **ENV** | Una perilla que recorre un camino de envolventes: click, pluck, piano, organ, swell, pad | **TIME**: estira o achica la envolvente del operador, de 0.1x a 10x |

La pantallita de cada bloque muestra la envolvente del operador, si es portador o modulador, y su ratio.

Notas sobre las perillas:

- **RATIO.** Los números enteros dan sonidos armónicos. 1.41, 3.5 y 7.07 dan campanas y metal. Para convertir un modulador en un gruñido, probá 1 contra 2 y después FINE para un pequeño desfasaje.
- **LEVEL como índice.** Un modulador en cero no hace nada; al subirlo agrega bandas laterales. El sonido se vuelve más brillante y más duro. Como cada operador tiene su propia envolvente, un modulador con decay corto da un ataque brillante que se asienta.
- **FEEDBACK** funciona en cualquier operador, no sólo en uno por algoritmo. Al máximo se acerca a una serie de sierra.
- **HARMONICS** cambia la senoidal por una de seis formas: Sine, Saw, Square, Organ, Reed y Bell. La forma se elige en la página OPS.
- **ENV** mueve juntos el ataque, el decay, el sustain y el release. Sus extremos son un click y un pad de cuatro segundos. Para una envolvente precisa usá la página OPS: ataque, decay, sustain, release, curva, delay y un loop de envolvente que hace respirar al operador.

### Algoritmos

La perilla ALGORITHM elige cómo se conectan los operadores. La columna de ruteo dice qué operador alimenta a cuál: un modulador cambia la frecuencia del operador al que alimenta.

| # | Nombre | Ruteo | Portadores | Bueno para |
|---|---|---|---|---|
| 1 | STACK | D, después C, después B, después A | A | Metales, leads, bajos complejos |
| 2 | FORK | D y C juntos, después B, después A | A | Stabs, metales más brillantes |
| 3 | PAIRS | D a C, y B a A | A, C | Piano eléctrico, campanas, tonos en capas |
| 4 | FAN | D a A, B y C | A, B, C | Pads, órganos, coros desafinados |
| 5 | CROWN | B, C y D juntos a A | A | Bajos gordos, gruñidos |
| 6 | STACK+SINE | D a C a B, con A solo | A, B | Un bajo con una senoidal limpia bajo el gruñido |
| 7 | DIAMOND | D a B y C, después a A | A | Tonos que se mueven y vocales |
| 8 | CHOIR | Sin modulación: los cuatro se suman | A, B, C, D | Órganos, drawbars, sierras con feedback |

## Filtro, drive y age

La fila de abajo es de la pista seleccionada.

| Perilla | Girar | FUNC + girar |
|---|---|---|
| **CUTOFF** | Frecuencia de corte del filtro, de 20 Hz a 20 kHz | **KEYTRACK**: cuánto sigue el corte a la altura |
| **RESO** | Resonancia | **BASE**: un pasa-altos antes del filtro, de 10 Hz a 2 kHz |
| **ENV** | Cuánto abre o cierra el corte la envolvente del filtro (puede ser negativa) | **DECAY**: el decay de la envolvente del filtro |
| **TYPE** | LP24, LP12, BP, HP, NOTCH, PEAK u OFF | **WIDTH**: un pasa-bajos después del filtro, de 500 Hz a 22 kHz |
| **DRIVE** | Drive antes del filtro | **FOLD**: un plegador de onda |
| **AGE** | Qué tan gastado está el sonido | **VINTAGE**: un interruptor para la voz de los primeros digitales |
| **DELAY** | Envío al delay de la pista | **TIME/FDBK**: la realimentación del delay |
| **REVERB** | Envío a la reverb de la pista | **SIZE/DECAY**: el decay de la reverb |

- **El filtro.** LP24 es un pasa-bajos de cuatro polos que se mantiene pesado con mucha resonancia. LP12, BP, HP, NOTCH y PEAK salen de un segundo filtro. OFF lo puentea. BASE y WIDTH forman un par que limita la banda y saca el rumble grave y el chisporroteo antes del filtro. La envolvente completa del filtro (ataque, decay, sustain, release) y la compensación de graves están en la página FILTER/AMP.
- **DRIVE, FOLD y CRUSH.** DRIVE satura de forma asimétrica y llega lejos. FOLD pliega la onda sobre sí misma: es el sonido duro y metálico del banco Heavy. CRUSH está en la página FILTER/AMP.
- **AGE.** En la pista, AGE agrega una etapa de cinta: wow y flutter lentos que hacen derivar la altura, pérdida de agudos, saturación suave y un poco de soplido. En valores bajos sólo suaviza y desafina; en valores altos el sonido ondula como un casete gastado. La etapa de cinta trabaja sobre toda la pista, incluidos sus envíos a efectos, pero la capa de sub queda limpia.
- **VINTAGE** (FUNC + AGE) pasa la voz a un carácter de primeros digitales: una tabla senoidal gruesa y una salida de operador de 12 bits, que agrega grano y mordida al ataque sin agregar aliasing. El campo también está en la página VOICE.
- **Los envíos.** DELAY y REVERB son los niveles de envío de la pista seleccionada. FUNC convierte las mismas perillas en la realimentación del delay y el decay de la reverb, que son globales. Su tiempo y su tamaño están en la página FX.

## Peso: el sub, el unison y los graves

En la página VOICE de una pista TONE:

- **Sub level, Sub octave y Sub shape**: una senoidal o triangular una o dos octavas abajo, mezclada limpia. El sub no pasa por el insert ni por la etapa de cinta, así un bajo distorsionado conserva su peso.
- **Unison**: de una a cuatro voces por nota, con desafinación hasta 50 cents y apertura estéreo.
- **Low-end mono**: deja en el centro todo lo que está por debajo de unos 120 Hz. Viene prendido.
- **Voice mode**: Poly, Mono o Legato, con **Glide** de hasta dos segundos (y Glide always, para deslizar en todas las notas y no sólo en las que se superponen). **Max voices** de 1 a 16.
- **Pitch env depth y time**: una envolvente de altura de hasta más o menos 48 semitonos, para bombos y plucks.
- **Noise level, decay, frequency, Q, filter, attack y Follows amp**: una capa de ruido filtrado, para soplido, redoblantes y hi-hats.
- **Tune** (más o menos 24 semitonos), **Fine** (más o menos 100 cents) y **Bend range** (de 1 a 24 semitonos).

## LFOs y modulación

Cada pista tiene dos LFO. La perilla **LFO** es la profundidad del LFO 1 y FUNC + LFO es la del LFO 2; el resto está en la página LFO: forma (sine, triangle, saw, square, S&H o smooth random), velocidad (libre de 0.01 a 40 Hz, o sincronizada de 1/64 a 16 compases), modo (free, trig, hold o one-shot), fade de entrada o de salida, fase inicial y **destino**. El destino puede ser la altura, la amplitud, el índice de FM, la apertura del paneo o casi cualquier parámetro del sonido.

La página **MOD** es una matriz de ocho ranuras. Cada una toma una fuente (velocity, keytrack, rueda de modulación, aftertouch, poly pressure, pitch bend, LFO 1, LFO 2, random, breath o expression), un destino y una cantidad de menos a más 100 por ciento.

## Teclas STEP

En el borde de adelante hay dieciséis teclas STEP, cada una con un LED. Lo que hacen depende de REC:

| Modo | Tecla STEP | FUNC + STEP |
|---|---|---|
| **REC prendido** (grabación de grilla) | Prender o apagar el paso. Mantenela apretada para editarlo (step locks). | 1 a 4: mostrar los pasos 1-16, 17-32, 33-48, 49-64. 5: copiar la página. 6: pegar. 7: borrar la página. 8: fill, mientras la mantenés. 9 a 16: patrón 1 a 8 |
| **REC apagado**, pista TONE | Un teclado: dieciséis notas de la escala del patrón, desde la raíz. Con CHORD prendido, cada tecla toca un acorde. | Lo mismo |
| **REC apagado**, pista KIT | Las teclas 1 a 8 tocan las ocho voces; las teclas 9 a 16 eligen la voz que edita el panel | Lo mismo |

El LED sobre cada tecla muestra la pista seleccionada (la voz seleccionada en un KIT): prendido para un paso activo, tenue para un paso con condición o probabilidad, titilando para un paso con locks, y rojo para la posición de reproducción.

- **Páginas.** Un patrón tiene hasta cuatro páginas de dieciséis pasos. FUNC + STEP 1 a 4 elige la página que muestran las teclas. El largo del patrón (1 a 64) y el de cada pista están en la página CHORD/SCALE.
- **Copiar, pegar, borrar.** FUNC + STEP 5, 6 y 7 trabajan sobre la página que ves, para la pista (o voz) seleccionada.
- **Fill.** FUNC + STEP 8 activa la marca de fill mientras la mantenés. Los pasos con la condición FILL suenan sólo entonces.
- **Patrones.** FUNC + STEP 9 a 16 elige el patrón 1 a 8 del banco actual, y apretar el que está sonando pasa al otro banco (patrones 9 a 16). El cambio espera al final del patrón.
- **Salto directo.** Mantené **STOP** (o Alt-clic) mientras apretás FUNC + STEP 9 a 16 para saltar en el acto en vez de esperar.

## Programar: el flujo de los step locks

Un step lock es un valor de un parámetro que vale sólo para ese paso. Es lo que evita que una secuencia suene como un loop.

1. Hacé clic en **REC** para prender la grabación de grilla. Hacé clic en las teclas STEP para activar pasos. Un paso nuevo toca Do4 en una pista TONE y la altura de la voz en un KIT.
2. Para editar un paso, **sostenelo**: mantené la tecla apretada medio segundo, o hacé Alt-clic (o clic derecho). Su LED titila y la pantallita muestra la página TRIG de ese paso.
3. Mientras sostenés el paso, **girá cualquier perilla**. No cambia el sonido: se vuelve un lock de ese paso. La pantallita dice `LOCK CUTOFF` con el valor. Se pueden bloquear el corte, la resonancia, la envolvente del filtro, el drive, AGE, los envíos, el nivel, los ratios y niveles de los operadores, la altura y la afinación de la voz, y la realimentación del delay y el decay de la reverb. Un paso con locks titila.
4. Sin soltar el paso, girá **ALGORITHM** para elegir un **campo del paso** y **DATA** para fijarlo (ver abajo).
5. Hacé clic en el paso otra vez (o apretá Escape) para soltarlo. El paso entero queda guardado en el patrón.

![Un paso sostenido en grabación de grilla: la pantalla muestra los campos del paso y las perillas ahora escriben locks.](img/rec-held.jpg)

- **Un lock en un paso vacío** crea un paso que no toca ninguna nota pero aplica sus locks. Sus locks alcanzan a las notas que siguen sonando, así un "lanzamiento de filtro" en el paso 9 abre el filtro sobre una nota sostenida desde el paso 1.
- **Para quitar un lock**, hacé clic derecho en la perilla mientras el paso está sostenido y elegí Remove this lock, o clic derecho en el paso y elegí Clear its locks. Apagar el paso también quita sus locks.
- **Los locks son por patrón.** El mismo paso en otro patrón tiene los suyos.

### Campos del paso

La página TRIG (y ALGORITHM y DATA con un paso sostenido) tiene estos campos:

| Campo | Qué hace |
|---|---|
| **Note** | La altura del paso (pista TONE) |
| **Velocity** | De 1 a 127 |
| **Length** | Desde un doceavo de paso hasta ocho pasos, o **Tie**, que sostiene la nota hasta el próximo disparo |
| **Condition** | Cuándo suena el paso (ver abajo) |
| **Cond A, Cond B** | Los números de la condición A:B |
| **Probability** | La chance de que el paso suene, de 0 a 100 por ciento |
| **Micro timing** | Mueve el paso hasta medio paso antes o después, en 48avos de paso |
| **Retrig, Retrig count, Retrig ramp** | Un redoble dentro del paso: velocidad de 1/2, 1/3, 1/4, 1/6 o 1/8 de paso, de dos a dieciséis golpes, y una rampa de velocity hacia arriba o abajo |
| **Chord, Chord type, Chord size, Voicing, Inversion** | Convierten el paso en un acorde (ver Acordes y escala) |
| **Slide** | Desliza hacia la nota siguiente en una pista mono o legato |
| **Patch** | Cambia el sonido entero por uno de los sonidos guardados con el proyecto (un guion es sin cambio) |

### Condiciones

Una condición decide, cada vez que la pista vuelve a pasar, si el paso suena. Se combinan con la probabilidad.

| Condición | El paso suena |
|---|---|
| **FILL / !FILL** | sólo mientras el fill está prendido / sólo mientras está apagado |
| **PRE / !PRE** | si el paso condicional anterior de esta voz sonó / no sonó |
| **NEI / !NEI** | si el paso condicional de la voz vecina (o de la pista de arriba) sonó / no sonó |
| **1ST / !1ST** | sólo en la primera vuelta del patrón / en todas menos la primera |
| **LAST / !LAST** | sólo en la última vuelta antes de un cambio de patrón en cola / en todas las vueltas menos esa |
| **A:B** | en la A-ésima de cada B vueltas: 1:2 suena una vuelta sí y otra no, 2:4 en la segunda de cada cuatro |
| **BAR** | en el compás A de cada B compases, sin importar el largo de la pista |

Combinadas con la probabilidad y con largos de pista distintos (probá 12, 14 y 16 pasos en tres pistas), las condiciones hacen un patrón que evoluciona durante minutos sin que lo toques.

## Voces del KIT

![Una pista KIT: las ocho voces están en las teclas STEP cuando REC está apagado.](img/page-seq.jpg)

Una pista KIT reúne ocho tambores en una sola pista. Cada voz tiene sus propios pasos (64), locks, condiciones y, si querés, su propio largo.

| Voz | Tambor | Nota a la que responde |
|---|---|---|
| 1 | Bombo | 36 |
| 2 | Redoblante | 38 |
| 3 | Clap o rim | 39 |
| 4 | Hi-hat cerrado | 42 |
| 5 | Hi-hat abierto | 46 |
| 6 | Tom grave | 45 |
| 7 | Tom agudo | 50 |
| 8 | Percusión: shaker, impacto o metal | 70 |

Las notas son las de los kits de fábrica. Cada voz tiene su propia **Lane MIDI note** y su **Lane pitch** en la página VOICE, y además responde a las notas 36 a 43 y a unas pocas notas de batería usuales.

- **Para tocar las voces**: con REC apagado, las teclas STEP 1 a 8 tocan las voces 1 a 8.
- **Para elegir la voz que edita el panel**: con REC apagado, apretá una tecla de la 9 a la 16 (voz 1 a 8). Las perillas de sonido ahora editan esa voz, y en grabación de grilla las teclas STEP muestran sus pasos. La página SEQ muestra todas las voces a la vez.
- **Cada voz es una voz completa.** Tiene los cuatro operadores (usá frecuencias fijas para cuerpos de metal y de ruido: página OPS, Fixed), una envolvente de altura para la caída del bombo, una capa de ruido para redoblantes y hi-hats, DRIVE, FOLD y CRUSH.
- **Grupos de choke.** Las voces del mismo grupo (1 a 3) se cortan entre sí: poné los hi-hats cerrado y abierto en el grupo 1.
- **En el bus del kit.** En una pista KIT, CUTOFF y RESO son un pasa-bajos del kit sumado, el ENV del filtro escala el decay de todas las voces y DRIVE es un drive de todo el kit, así una perilla cierra o ensucia toda la batería. Las cuatro perillas LEVEL de operador son el nivel de grupos de voces: A bombo y tom grave, B redoblante y clap, C hi-hats, D tom agudo y percusión.
- **DUCK.** El DUCK del master usa la voz de bombo de la primera pista KIT como sidechain de las pistas tone (ver Master).

## Acordes y escala

Cada patrón tiene una **raíz** y una **escala**, que se fijan en la página CHORD/SCALE (FUNC + CHORD la abre). Hay 19 escalas: Major (Ionian), Natural minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Harmonic minor, Phrygian dominant, Double harmonic, Major pentatonic, Minor pentatonic, Hirajoshi, In-sen, Whole tone, Lydian augmented, Melodic minor, Diminished y Chromatic. La raíz y la escala son lo que siguen el teclado de las teclas STEP, el modo de acordes y la memoria de acordes.

### Pasos de acorde

Con un paso sostenido, el campo **Chord** lo convierte en un acorde:

- **Degree**: el paso tiene un grado de la escala y toca el acorde armado sobre él en la escala del patrón. Chord type (Diatonic, Sus2, Sus4, Add9, Quartal, Cluster, Open fifths, Shell o Power), size (2 a 6 notas), voicing (Close, Drop-2, Drop-3, Spread o No root) e inversion (0 a 3) le dan forma. Si cambiás la escala del patrón, los acordes la siguen.
- **Memory**: el paso toca una forma guardada (ver Memoria de acordes).

Una pista también puede tocar acordes en vivo: **CHORD** prende el modo de acordes y, desde entonces, cada nota que tocás (en las teclas STEP o desde MIDI) se vuelve un acorde sobre el grado más cercano de la escala. El tamaño es Chord size en la página CHORD/SCALE.

La página VOICE suma **Strum** (hasta 60 ms entre las notas, hacia arriba, hacia abajo o alternando), **Humanise velocity y timing**, y **Drone hold**: cuando a un paso de acorde le sigue el mismo acorde, las notas siguen sonando en vez de reiniciarse.

### Memoria de acordes

El proyecto tiene ocho ranuras de memoria de acordes, cada una una forma de hasta ocho intervalos contados desde la nota más grave.

- **Para guardar una forma**, mantené apretada **CHORD**, tocá las notas (desde MIDI, o en las teclas STEP con REC apagado) y soltá CHORD. La forma va a la primera ranura vacía, o a la ranura que sigue a la última guardada cuando las ocho están ocupadas. Un clic corto en CHORD sin notas sigue alternando el modo de acordes.
- **Con el mouse** no se puede mantener CHORD y hacer clic en una tecla STEP al mismo tiempo: hacé Alt-clic en CHORD. La tecla titila, tocá las notas y hacé clic otra vez en CHORD para guardar.
- **Para usar una forma**, sostené un paso y poné Chord en Memory. El paso toca la ranura a la que apunta su valor de **Chord type** (0 a 7 son las ranuras 1 a 8; la pantallita sigue mostrando los nombres de tipo de acorde para esos números), armada desde la nota del propio paso y con todas las notas cuantizadas a la escala del patrón. Como las formas se cuantizan, una forma guardada desde un acorde sigue la tonalidad cuando cambiás la escala o la raíz.
- Las ranuras, con sus intervalos, figuran en la página CHORD/SCALE (sólo lectura ahí). Se guardan con el proyecto, y por lo tanto con los presets de usuario.

### Canal de armonía

Si fijás un **Harmony channel** (página MIDI o CHORD/SCALE), las notas que mantenés apretadas en ese canal fijan la raíz y la escala del patrón mientras las sostenés: una tercera mayor o menor, con o sin séptima, da mayor, mixolidia, menor natural o dórica. Los acordes del secuenciador siguen lo que toca tu otra mano.

## El arpegiador

Cada pista TONE tiene un arpegiador para las notas que tocás (desde MIDI o desde las teclas STEP). Se prende con **Arp mode** en la página VOICE: Up, Down, Up-down, Random u Order. **Arp rate** es una división de nota de 1/64 a 4 compases y **Arp octaves** lo extiende de 1 a 4 octavas. Las notas que mantenés se tocan de a una, cada una al 80 por ciento de la velocidad. Order toca las notas sostenidas desde la más grave hacia arriba, igual que Up.

El arpegiador sigue el tempo, y graba en el patrón como cualquier nota tocada cuando estás grabando.

## Patrones, canción y grabación

### Patrones

Hay 16 patrones. El parámetro del host **Pattern** elige el actual, así que se puede automatizar. Cada patrón tiene su propia raíz, escala, largo, swing y, por pista, largo, velocidad y cantidad de swing. Una voz de KIT también puede tener su propio largo.

### Modo canción

La página **SONG** (FUNC + DATA) es una cadena: hasta 16 filas de patrón y repeticiones (1 a 16). Prendé **Song mode** y el DeVicenti toca la cadena desde la primera fila, repitiendo cada patrón, y vuelve a empezar. Con Song mode apagado, el patrón actual se repite.

### Grabación

- **Grabación de grilla** (REC): hacé clic en los pasos para activarlos, como se explicó arriba.
- **Grabación paso a paso** (FUNC + REC): con REC apagado, las notas que tocás en las teclas STEP se escriben en el paso sostenido (o en el siguiente) y el cursor avanza.
- **Grabación en vivo** (REC + PLAY): las notas que tocás desde tu teclado MIDI o las teclas STEP se escriben en el patrón mientras corre. **Record quantise** en la página MIDI elige To the step o Keep timing (micro timing). Las perillas que movés mientras grabás en la pista seleccionada se vuelven locks del paso actual.

## Páginas de atrás

La pantallita puede mostrar 11 páginas, y el menú PAGES abre 13 como editores completos con todos los campos a la vez, agrupados en columnas. Arrastrá una fila a izquierda y derecha, usá la rueda encima, hacé clic para recorrer una opción, o hacé doble clic en una fila para escribir su posición de 0 a 100.

![La página VOICE para una voz de KIT.](img/page-voice.jpg)

| Página | Qué hay |
|---|---|
| **VOICE** | Machine, KIT lane, altura y nota MIDI de la voz, grupo de choke, algorithm, voice mode, voices, unison, glide, tune, fine, bend range, level, pan, low-end mono. Envolvente de altura, sub y ruido. AGE, vintage, strum, humanise, drone hold y el arpegiador |
| **OPS** | Los cuatro operadores completos: ratio, frecuencia fija, detune, level, feedback, forma y cantidad de armónicos, ataque, decay, sustain, release, curva, delay y loop de la envolvente, velocity, key scaling y escala de tiempo |
| **FILTER/AMP** | Tipo de filtro, cutoff, resonancia, cantidad de envolvente, la envolvente del filtro, keytrack, base, width y compensación de graves; drive, fold, crush, la envolvente de amplitud y velocity a amplitud |
| **LFO** | Los dos LFO: forma, velocidad, sincronía, modo, fade, fase, destino, profundidad |
| **MOD** | Las ocho ranuras de modulación |
| **TRIG** | Los campos del paso sostenido (el primer paso si no hay ninguno sostenido) |
| **CHORD/SCALE** | Raíz, escala, largo y swing del patrón; largo, velocidad y swing de la pista; modo de acordes y tamaño; canal de armonía; las ranuras de memoria de acordes |
| **MIX** | Para cada pista: level, pan, mute, solo, salida (principal o propia) y machine. Master, tape, glue y limit |
| **FX** | Delay, reverb, bus master y el insert de la pista (ver Efectos) |
| **MIDI** | Entrada MIDI, CC, reloj y canales por pista (ver MIDI de entrada) |
| **SONG** | Song mode y la cadena |
| **SEQ** | La grilla de pasos del patrón: todas las pistas, todas las voces del KIT, 64 pasos. Hacé clic en una celda para activar un paso, arrastrá para pintar |
| **SETTINGS** | Tamaño de la ventana y ayuda |

![La página OPS: los cuatro operadores completos.](img/page-ops.jpg)

![La página CHORD/SCALE: raíz, escala, largos, ajustes de acordes y las ocho ranuras de memoria de acordes.](img/page-chord-scale.jpg)

## Efectos

![La página FX: los dos envíos, el bus master y el insert de la pista seleccionada.](img/page-fx.jpg)

### Insert

Uno por pista, después del filtro, que se elige en la página FX: **Overdrive/Fold**, **Bitcrush/Lo-fi**, **Chorus/Ensemble**, **Phaser**, **Ring Mod** o **Comb/Resonator**. Tiene una cantidad (CC 93), **Tone**, **Character** y **Mix**. Lo que hacen Tone y Character depende del tipo:

| Insert | Amount | Tone | Character |
|---|---|---|---|
| Overdrive/Fold | Drive, hasta 30 dB | Pasa-bajos después del drive | Cantidad de plegado |
| Bitcrush/Lo-fi | Profundidad de bits, de 16 a 4 | Frecuencia de muestreo, de 44.1 kHz a 2.7 kHz | Filtros anti-alias prendidos o apagados: apagados es el color granuloso de un sampler |
| Chorus/Ensemble | Profundidad | Velocidad, de 0.05 a 8 Hz | Chorus o un ensemble de tres voces |
| Phaser | Profundidad del barrido | Velocidad, de 0.02 a 5 Hz | Realimentación |
| Ring Mod | Cantidad | Frecuencia, de 5 Hz a 2 kHz | De senoidal a cuadrada dura |
| Comb/Resonator | Realimentación | Afinación, más o menos dos octavas alrededor de la raíz del patrón | Amortiguación |

La capa de sub de una pista TONE no pasa por el insert.

### Delay

Tres modos: **Tape** (un lazo de cinta con wow y flutter, realimentación filtrada y saturada; al cambiar el tiempo se desliza la altura), **Dub** (realimentación filtrada en banda y saturante que puede autooscilar por encima del 95 por ciento aproximadamente) y **Ping-pong**. Time es una división de nota, Feedback pasa del 100 por ciento para lanzamientos de dub, y además Tone, Wow, Return, **Into reverb** (cuánto del delay alimenta la reverb) y **Freeze**, que sostiene el lazo.

### Reverb

Cuatro modos: **Spring** (goteo y "boing", armada con lazos dispersivos), **Plate**, **Hall** y **Void** (larguísima, hasta 30 segundos, oscura y modulada, para dark ambient). Size, Decay, Damping, Pre-delay, Modulation, **Shimmer** (una realimentación una octava arriba, en Hall y Void), Return y **Freeze**, que sostiene la cola.

### Bus master

- **TAPE**: wow y flutter, saturación suave, soplido y crepitar de vinilo. **Tape noise** fija cuánto soplido y crepitar.
- **GLUE**: un compresor de bus. Una perilla mueve juntos el umbral y el makeup; ratio de 2:1 a 4:1, con release automático.
- **DUCK**: baja las pistas TONE en cada bombo de la primera pista KIT (o de la pista que elijas con **Duck source**), para pads que bombean. **Duck release** es el tiempo de recuperación.
- **LIMIT**: un limitador suave con techo de menos 1 dBFS. Siempre hay un techo de seguridad prendido.

## MIDI de entrada: tocarlo desde un controlador

![La página MIDI: modo de entrada, modo de CC, feedback, reloj y los canales de cada pista.](img/page-midi.jpg)

Abrí la página MIDI para los ajustes.

- **Input.** *OMNI to current track*, el valor por defecto, toca la pista seleccionada desde cualquier canal: hacé clic en una tecla TRACK para elegir qué pista toca tu teclado. *Channel per track* le da a cada pista su propio **In channel** (1 a 4 por defecto): las notas del canal 1 tocan la pista 1, y así.
- **Notas.** Velocity, pitch bend (el rango está en la página VOICE), rueda de modulación (CC 1), aftertouch, poly pressure, breath (CC 2), expression (CC 11) y sustain (CC 64). Todos son fuentes de la página MOD. CC 120 y 123 cortan todo el sonido y todas las notas de la pista.
- **Pistas KIT.** Las notas tocan las voces por su nota de voz (36 a 43 por defecto).
- **Pistas de máquina MIDI.** Las notas salen directo al canal de salida de la pista.
- **Grabación.** REC + PLAY graba en el patrón lo que tocás.

### El mapa de CC

Cada perilla tiene un número de CC fijo en el canal de cada pista, así lo mapeás una sola vez en tu controlador. La capa FUNC tiene su propio número. En modo OMNI los CC van a la pista seleccionada.

| Control | CC | Capa FUNC | CC |
|---|---|---|---|
| SWING | 9 | | |
| ALGORITHM | 15 | | |
| LFO | 12 | LFO 2 | 13 |
| MASTER | 16 | TRK LEVEL | 7 |
| Operador A: RATIO, LEVEL, FEEDBACK, DETUNE, ENV | 40 a 44 | FINE, VELOCITY, HARMONICS, KEY SCALE, TIME | 102 a 106 |
| Operador B | 45 a 49 | | 107 a 111 |
| Operador C | 50 a 54 | | 112 a 116 |
| Operador D | 55 a 59 | | 117 a 121 |
| CUTOFF | 74 | KEYTRACK | 80 |
| RESO | 71 | BASE | 81 |
| ENV del filtro | 75 | DECAY | 82 |
| TYPE | 76 | WIDTH | 83 |
| DRIVE | 77 | FOLD | 84 |
| AGE | 78 | VINTAGE | 85 |
| DELAY | 92 | realimentación | 86 |
| REVERB | 91 | decay | 87 |

Más CC que no son perillas del panel: **93** cantidad del insert, **10** pan, **88** TAPE, **89** GLUE, **90** DUCK, **3** LIMIT, **94** realimentación del delay y **95** tamaño de la reverb. TEMPO y las teclas no tienen CC.

### MIDI learn

Para asignar cualquier control a un CC propio: hacé clic en **LEARN** en la barra superior (o clic derecho en la perilla y elegí MIDI learn), hacé clic en la perilla y mové el control de tu controlador. El par se guarda con el proyecto. Clic derecho en una perilla y Clear learned CCs los quita. Learn funciona con toda perilla que fija un valor; las teclas y la pantallita no se pueden aprender.

### CC relativos y feedback

En la página MIDI, **CC mode** elige Absolute, Relative (two's complement) o Relative (offset 64). Usá un modo relativo para encoders sin tope: el valor parte de donde ya está la perilla, así que nada salta. Con **CC feedback** prendido, toda perilla mapeada que movés en el panel, o que cambia con un preset o con la automatización, sale como CC (por el canal 1 en OMNI, o por el canal de la pista), así los faders motorizados y los anillos de LED siguen al plugin.

### Program change, reloj y armonía

- **Program change**: elige un patrón (los programas 0 a 15 son los patrones 1 a 16), o un preset si ponés Program change en Preset.
- **Clock in y Clock out** (sólo en la independiente): siguen un reloj MIDI externo con start y stop, o lo envían. En un DAW se usan el tempo y el transporte del host.
- **Harmony channel**: ver Acordes y escala.

## MIDI de salida: manejar un sinte externo desde un DAW

El DeVicenti puede mandar notas, acordes y locks como MIDI, así un patrón puede tocar un sinte de hardware u otro plugin.

**Hacer que una pista mande MIDI**

- Poné la máquina de la pista en **MIDI** (FUNC + ALGORITHM). No hace sonido; sus pasos, acordes, velocities, largos y condiciones salen por el **Out channel** de la pista (página MIDI). Los locks de corte, resonancia, envolvente del filtro, drive, AGE, envíos, pan, nivel y profundidades de LFO salen como CC 74, 71, 75, 77, 78, 92, 91, 10, 7, 12 y 13.
- O dejá una pista TONE o KIT como está y prendé **Notes to MIDI out** para ella: toca su propio sonido y además manda las notas. Con **Thru** prendido, lo que tocás desde un controlador en esa pista también se reenvía.

**En un DAW**

1. Poné el DeVicenti en una pista MIDI, con sonido o sin él.
2. Creá otra pista MIDI con el sinte que querés manejar, o una que vaya a tu hardware.
3. Poné la entrada MIDI de esa pista en la pista del DeVicenti (en Ableton Live: **MIDI From**, después la pista, después su plug-in y el canal que fijaste como Out channel; en REAPER: agregá un envío MIDI desde la pista del DeVicenti, o una entrada desde ella) y armala para grabar.
4. Arrancá el transporte del DAW. Las notas llegan al sinte a tiempo con el host.

El preset **MIDI Out** del banco Init está hecho para probarlo: la pista 4 es una máquina MIDI en el canal 1, y la pista 2 suena y además manda sus notas por el canal 2.

El MIDI de salida sale retrasado por la latencia del plugin, así queda a tiempo con el sonido que hace el DeVicenti. En la app independiente el MIDI de salida va al dispositivo de salida que elijas y el reloj puede acompañarlo.

## Presets

Los 82 presets son proyectos completos: sonidos, patrones, efectos y tempo. Hacé clic en el nombre del preset para verlos por banco. **SAVE** guarda los tuyos como presets de usuario, que después aparecen en el menú bajo User; Rescan user presets lee los nuevos de la carpeta.

| Banco | Presets | Carácter |
|---|---|---|
| **Init** (2) | Init, MIDI Out | Un proyecto para empezar a 88 BPM, y un proyecto para probar el MIDI de salida |
| **Trip Hop** (15) | Basement Boom, Damp Hum, Slow Sag, Tine Haze, Velvet Tine, Dust Strings, Thin Voice, Bristol Kit, Sleepwalk Kit, Cracked Vinyl Kit, Murk Pad, Rainy Pluck, Low Swell, Half-Light, Dub Throws | Bajos de FM pesados y desafinados sobre un sub, kits polvorientos, pianos eléctricos tine por reverb spring, lo-fi |
| **Downtempo** (12) | Rolling Under, Warm Floor, Round Walk, Soft Vapor, Long Porch, Glass Pluck, Bamboo, Slow Choir, Soft Keys, Sunday Kit, Round Kit, Dub Whistle | Bajos rodantes, pads cálidos con AGE, plucks al delay dub |
| **Acid Jazz** (13) | Funk Tine, Wah Keys, Late Set EP, Sunday Organ, Church Drive, Pocket Slap, Round Fretless, Horn Section, Short Horn, Vibe Mallet, Club Kit, Brush Kit, Tape Chords | Pianos eléctricos con novenas y oncenas, un órgano de FM, bajos slap y fretless, metales, ride y escobillas |
| **Ambient** (13) | Slow Tide, Cold Dawn, Wide Room, Held Breath, Glass Rain, Soft Chime, Paper Piano, Warm Hum, Aurora, Distant Tape, Tick Garden, Frozen Lake, Frozen Tail | Pads que evolucionan, campanas en loops de largos distintos, shimmer y una reverb congelada |
| **Dark Ambient** (14) | Black Cavern, Iron Breath, Cold Furnace, Under Water, Rusted Choir, Rite Bell, Hollow Gong, Distant Clang, Dead Signal, Crypt EP, Heartbeat Kit, Pressure, Last Light, Slow Weight | Drones de peine, rumble de sub, pads en cluster, metal, la reverb void |
| **Heavy** (13) | Iron Growl, Basalt, Low Press, Concrete Sub, Hammer Stab, Slag Stab, Fold Lead, Heavy Pad, Wobble Drive, Anvil Kit, Grit Kit, Dub Slab, Drive and Fold | Bajos gruñones, plegados y aplastados, kits en half-time, stabs duros |

Un preset carga toda la unidad: las cuatro pistas, los patrones, la cadena y los efectos.

## Parámetros del host y automatización

El DeVicenti le muestra 64 parámetros al host. Para cada una de las cuatro pistas (T1 a T4): Cutoff, Resonance, Filter env, Drive, AGE, Delay send, Reverb send, Level, Op A level, Op B level, Op C level, Op D level e Insert amount. Globales: Swing, Master, Delay time, Delay feedback, Reverb size, Reverb decay, Tape, Glue, Duck, Limit, Tempo y Pattern. Todo lo demás (el resto del sonido, los patrones, los modos de efectos y los ajustes MIDI) es parte del estado del proyecto, se edita en el panel y en las páginas, se guarda con el proyecto y se puede cambiar paso a paso con locks.

En una pista KIT los valores por pista actúan sobre el bus del kit: cutoff y resonancia son un pasa-bajos del bus, ENV escala el decay de todas las voces, drive es un drive de bus, y los cuatro niveles de operador son los grupos de voces (A bombo y tom grave, B redoblante y clap, C hi-hats, D tom agudo y percusión).

## Recetas

### Un bajo lento y pesado en dos minutos

1. Elegí la pista 2 y poné ALGORITHM en 5 (CROWN) o 6 (STACK+SINE).
2. Girá LEVEL de B, C y D a la mitad más o menos, con RATIO 1, 2 y 2 y un poco de DETUNE en C.
3. Abrí la página VOICE, poné **Sub level** en 70 por ciento y **Unison** en 2.
4. Cerrá el filtro: CUTOFF cerca de 400 Hz, RESO un poco arriba, ENV del filtro positivo para que cada nota lo abra, y subí DRIVE.
5. En REC, activá los pasos 1, 4 y 11, y sostené el paso 11 para bloquear un CUTOFF más alto.

### Un kit que respira

1. La pista 1 como KIT. Con REC apagado, apretá la tecla STEP 9 para elegir la voz 1 y ajustá su envolvente de altura en la página VOICE.
2. Con REC prendido, activá la voz 1 en los pasos 1 y 11, la voz 2 en el 9, y la voz 4 en todos los pasos.
3. Sostené los pasos del hi-hat y dales 60 por ciento de probabilidad y un patrón de velocity con locks. Poné Choke group 1 en las voces 4 y 5.
4. Subí SWING por encima de 56 y empujá el TRK SWING de la pista.

### Acordes que siguen una tonalidad

1. Elegí una pista TONE. En la página CHORD/SCALE poné la raíz en Fa y la escala en Dorian.
2. En REC, sostené el paso 1 y poné Chord en Degree, Chord size en 4 y Voicing en Drop-2. Hacé lo mismo en el paso 9 con otra nota.
3. Cambiá la raíz o la escala y escuchá cómo cambian los acordes.

### Un patrón que no se repite

1. En la pista 3 dale al patrón un largo de 13 pasos. En la pista 4, un largo de 11.
2. Activá unos pocos pasos y dales Probability 60 por ciento, o la condición 1:2 o 3:4.
3. Agregá un paso sólo de lock, con un lock de CUTOFF, al final de uno de los loops.

### Tocar el DeVicenti desde un teclado mientras secuencia

1. En la página MIDI dejá Input en OMNI. Hacé clic en TRACK 3.
2. Tocá un lead sobre el patrón que corre. Girá las perillas: con REC + PLAY prendido, los movimientos se vuelven locks.
3. Para arpegios, poné Arp mode en la página VOICE de esa pista y mantené acordes.

## Consejos y solución de problemas

- **No suena en un DAW.** Fijate que el transporte del host esté andando (el secuenciador lo sigue), que la pista no esté silenciada y que su nivel esté arriba. Una pista en máquina MIDI no hace sonido por sí misma.
- **PLAY no hace nada en mi DAW.** En un plugin manda el transporte del host. PLAY y RESTART sólo funcionan en la app independiente.
- **Muevo una perilla en el panel y el sonido no cambia.** Hay un paso sostenido: está escribiendo un lock. Hacé clic en el paso otra vez o apretá Escape.
- **FUNC queda prendido.** Está trabado. Hacé clic de nuevo, o apretá Escape.
- **Un preset reinicia todo el proyecto.** Un preset carga toda la unidad. Guardá los tuyos con SAVE.
- **CPU alta.** El filtro, el unison y la reverb son lo que más cuesta. Bajá **Unison**, reducí **Max voices**, o usá Plate en lugar de Void.
- **El mismo paso suena distinto cada vez.** Tiene una probabilidad o una condición. Un LED tenue marca esos pasos.

## Especificaciones

- 4 pistas, cada una TONE (cuatro operadores, ocho algoritmos, sub, unison, ruido), KIT (ocho voces de FM) o MIDI.
- 16 patrones de hasta 64 pasos; largo, velocidad (1/8x a 2x) y swing por pista; step locks, condiciones, probabilidad, retrigs, micro timing, slides, ligaduras, cambio de patch, acordes, strum, arpegiador y una cadena de canción.
- Filtro de siete posiciones (LP24, LP12, BP, HP, NOTCH, PEAK, OFF) y un par base y width; DRIVE, FOLD, CRUSH, AGE.
- Seis inserts, un delay de tres modos, una reverb de cuatro modos más freeze y shimmer, y un master con tape, glue, duck y limit.
- 19 escalas, ocho ranuras de memoria de acordes, ocho ranuras de modulación, dos LFO por pista.
- 82 presets; 64 parámetros automatizables; una salida estéreo principal y cuatro salidas de pista opcionales.
- MIDI de entrada con mapa de CC fijo, MIDI learn, CC relativos y CC feedback; MIDI de salida con pistas de máquina MIDI y duplicación de notas; reloj MIDI en la independiente.
- VST3 en macOS, Windows y Linux, Audio Unit en macOS, LV2 en Linux y una aplicación independiente.
