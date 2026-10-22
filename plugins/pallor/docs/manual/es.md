# Manual de Pallor

## Qué es Pallor

Pallor es una caja de ritmos para el lado frío y oscuro de la música electrónica: dark wave, coldwave, post-punk, minimal wave, EBM, synthpop, goth rock, industrial, new beat, italo y hi-NRG, witch house, darkwave y trip-hop de los 90, y dark ambient. Es un diseño original, no la copia de ninguna máquina.

Tiene dieciséis voces. Cada una se puede tocar con dos motores: un motor analógico que modela los circuitos de las cajas de ritmos de 1978 a 1984 (resonadores bridged-T, tambores con VCO, metales de seis cuadradas, cajas y palmas de ruido, las baterías electrónicas hexagonales), y un motor digital que toca muestras a través de la cadena de las cajas de ritmos con muestras de los 80, con su conversor de 8 bits con companding y su afinación fijada por el reloj. Trae ocho kits digitales, todos sintetizados por el propio Pallor, y cada voz puede cargar una muestra tuya.

Alrededor de las voces están los efectos de estudio de la época (salas con gate, no lineales e invertidas, placas, resortes, eco de cinta, chorus de bucket-brigade, flangers, phasers, samplers lo-fi, cinta y compresión de bus) y un secuenciador por pasos con lo que piden los programadores de ritmos de hoy: probabilidad, ratchets, flams, condiciones, polimetría, swing por patrón, fills y una cadena de canción. Viene con 65 presets en 13 géneros y una biblioteca de 156 patrones.

![Pallor tocando Night Shift Mercury: la pantalla arriba con la página PATTERN, las ocho perillas de la voz elegida y los dieciséis pads.](img/overview.jpg)

## Instalación

Bajá el paquete para tu sistema desde el sitio y descomprimilo. El launcher de Audio Tools también puede bajarlo e instalarlo por vos.

En macOS:

1. Copiá `Pallor.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `Pallor.component` (Audio Unit) a `~/Library/Audio/Plug-Ins/Components/`.
2. Volvé a escanear los plugins en tu DAW. Pallor aparece como **Audio Tools Pallor**, entre los instrumentos.
3. `Pallor.app` es la versión independiente y `pallorrender` el renderizador de línea de comandos.

Los archivos no están notarizados. Si macOS no los deja abrir, corré `xattr -dr com.apple.quarantine` seguido de la ruta de cada archivo.

En Windows:

1. Copiá `Pallor.vst3` a `C:\Program Files\Common Files\VST3\`.
2. Volvé a escanear los plugins en tu DAW.

En Linux:

1. Copiá `Pallor.vst3` a `~/.vst3/` y `Pallor.lv2` a `~/.lv2/`.
2. Volvé a escanear los plugins en tu DAW. La versión independiente necesita JACK o ALSA.

## Primer sonido

1. Poné Pallor en una pista de instrumento (MIDI).
2. Dale play en tu DAW. Pallor sigue el transporte y el tempo del host, y el primer preset, Night Shift Mercury, empieza a tocar su canción.
3. Hacé clic en los pads para escuchar cada voz: un clic cerca de la parte de arriba de un pad suena más fuerte que cerca de la parte de abajo.
4. Recorré los presets con las flechas a cada lado del display, o hacé clic en el display para explorarlos por género.

Sin DAW, abrí la versión independiente y apretá **Run**: Pallor toca con su propio reloj, al **Tempo** de la barra de arriba.

## El panel

Pallor se ve como un aparato visto desde arriba, y funciona como uno.

- **La barra de arriba.** El display muestra el preset (con un asterisco cuando cambiaste algo), su género, el tempo y, mientras suena, el patrón y el compás. Las flechas recorren los presets. **Manual** abre este manual. Las tres perillas son **Tempo** (el reloj interno, que se usa cuando Pallor no sigue al host), **Swing** (el swing global, de 50 a 75 %) y **Volume** (la salida general).
- **La pantalla.** Ocho páginas, que se eligen con las pestañas de arriba: PATTERN, VOICE, MIXER, FX, SONG, SAMPLES, PRESETS y SETUP. El nombre de la voz elegida se ve siempre a la derecha de las pestañas.
- **La fila de la voz.** Ocho perillas para la voz que elegiste: Tune, Decay, Tone, los dos controles de carácter de su modelo (sus nombres cambian con el modelo), Drive, Cutoff y Level. A la izquierda, **Run** y **Fill**; a la derecha, **Mute** y **Solo** para la voz elegida.
- **Los pads.** Uno por voz. Un pad se ilumina cuando suena su voz, desde el secuenciador, desde MIDI o con tu clic. Hacé clic en un pad para tocar y elegir esa voz; con Alt-clic la elegís sin tocarla; con clic derecho cargás o borrás una muestra, la silenciás o la ponés en solo. Podés soltar un archivo de audio sobre un pad para cargarlo en esa voz.

Todas las perillas se giran arrastrando para arriba y para abajo (o de costado); con Shift el arrastre es fino, un doble clic las devuelve a su valor por defecto y la rueda del mouse las mueve de a pasos. Cada control tiene su globo de ayuda.

![La fila de la voz y los pads: las perillas siguen a la voz elegida y los pads se iluminan con cada golpe.](img/panel.jpg)

## Voces y motores

Las dieciséis voces son Kick, Snare, Clap, Rim, Closed Hat, Open Hat, Crash, Ride, Tom 1 a Tom 4, Cowbell, Clave, Tambourine y FX. Cada voz tiene el mismo juego de controles y elige su motor en la página VOICE: **Analog** o **Digital**.

![La página VOICE con la caja: el motor y el modelo a la izquierda, los controles al medio y el golpe mismo a la derecha, dibujado con los ajustes actuales.](img/voice.jpg)

### El motor analógico

El motor analógico sintetiza cada golpe a partir del modelo de un circuito. Cada voz ofrece los modelos de su familia:

| Voz | Modelos |
|---|---|
| Kick | Bridged-T, VCO Punch, Compact, Hexagonal, Dark |
| Snare | Bridged-T, VCO Punch, Hexagonal, Noise Box, Crack |
| Clap | Analog, Big, Tight |
| Rim | Bridged-T, Woody, Click |
| Closed Hat, Open Hat | Six-Square, Noise Box, Twin Tank, FM |
| Crash | Six-Square, Noise Wash, FM |
| Ride | Six-Square, Bell, FM |
| Toms | Bridged-T, Hexagonal, VCO Punch, Conga |
| Cowbell | Two-Square, Clank, Bell |
| Clave | Bridged-T, Woodblock, Blip |
| Tambourine | Maracas, Tambourine, Shaker |
| FX | Zap, Sweep, Metal, Drop, Static |

**Bridged-T** es el tambor resonante de las máquinas de fines de los 70: un resonador golpeado por un pulso corto, con el clic del pulso y una afinación que arranca un poco alta y se asienta. **VCO Punch** es el tambor de 1983 y 1984: un oscilador con una envolvente de afinación rápida. **Six-Square** es el metal de charles y platillos hecho con seis ondas cuadradas a frecuencias sin relación entre sí, a través de filtros pasa banda. **Hexagonal** es el pad de batería electrónica de principios de los 80 con su caída brusca de afinación, el "piu". **Twin Tank** es un metal más barato: ruido y tres cuadradas en dos tanques resonantes. **Dark** y **Crack** son voces de nuestro tiempo: un bombo de barrido largo y una caja dura y corta.

Cada modelo tiene dos controles de carácter cuyos nombres aparecen debajo de las perillas y en la página VOICE: en un bombo bridged-T son Click y Glide, en un charles de seis cuadradas Color y Spread, en un metal FM Index y Ratio. Cada modelo está calibrado para que un kit nuevo quede balanceado con todos los niveles en 0 dB.

### El motor digital y la cadena de los 80

El motor digital toca una muestra a través de la cadena de las cajas de ritmos con muestras de 1980 a 1990. Elegí un **Kit** y una **Chain**:

| Kit | Qué es |
|---|---|
| Studio 82 | Kit acústico, seco y ajustado, 8 bits con companding a 28 kHz |
| Pop 84 | Kit acústico con pegada, salas con gate en caja, palmas y toms, 8 bits a 25 kHz |
| Electro 81 | Las voces analógicas del propio Pallor sampleadas en un sampler de 12 bits a 26 kHz |
| Hexa 84 | El kit electrónico hexagonal sampleado a 8 bits: caídas, ruido y clics |
| Crunch 88 | Golpes acústicos grandes y calientes a través de un sampler de 12 bits a 26 kHz |
| Studio 91 | Kit acústico con más sala, 12 bits a 40 kHz |
| Foundry | Industrial: chapas, caños, tambores de metal y cadenas golpeados, 8 bits a 32 kHz |
| FM 85 | Tambores FM de dos operadores: finos, vidriosos, fríos |
| User sample | Tu propia muestra para esta voz |

No se usa ninguna muestra de ninguna máquina: Pallor sintetiza cada kit cuando arranca.

Las cadenas:

- **8-bit**: el conversor con companding de las primeras máquinas con muestras (255 niveles, unos 72 dB de rango, más ruido en los sonidos fuertes que en los suaves). **Tune** cambia el reloj de reproducción, como en esas máquinas: al bajar la afinación, las imágenes del conversor entran en la banda audible, el brillo áspero de la época. Bombos y toms pasan por un filtro de salida de 10 kHz que se abre en cada golpe; el resto, por uno de 16 kHz.
- **12-bit 26k**: 12 bits a 26,04 kHz fijos, afinación salteando y repitiendo muestras, sin filtro de salida: el sampler sucio de fines de los 80.
- **12-bit 40k**: 12 bits a 40 kHz, más limpio y más abierto.
- **Clean**: la muestra tal cual.
- **Kit default**: la cadena para la que se hizo el kit.

En el motor digital los dos controles de carácter son **Start** (dónde arranca la reproducción, para recortar el ataque) y **Pitch env** (una caída de afinación al principio del golpe). **Reverse** toca la muestra al revés.

### Darle forma a una voz

Los mismos controles funcionan en los dos motores:

- **Tune**: ±24 semitonos.
- **Decay**: cuánto suena el golpe.
- **Tone**: de oscuro a brillante.
- **Accent**: cuánto más fuerte y más duro suena un paso con acento en esta voz (la cantidad global está en SETUP).
- **Velocity**: cuánto cambia el nivel la velocidad de un paso o de una nota MIDI (con 0 % todos los golpes suenan igual).
- **Drive**: saturación.
- **Bits** y **Rate**: un reductor de bits y de frecuencia de muestreo por voz (con 16 bits y 48 kHz quedan apagados).
- **Filter**: Off, LP, HP o BP, con **Cutoff** y **Reso**.
- **Level** (−60 a +6 dB), **Pan**, **Send A** (a Space) y **Send B** (a Echo).
- **Choke**: las voces del mismo grupo se cortan entre sí. Los charles cerrado y abierto están en el grupo 1, así que un charles cerrado corta a un charles abierto que está sonando, como en una batería de verdad.
- **Bus**: la salida que usa esta voz cuando Outputs está en Per voice (ver Salidas múltiples).

El golpe que se dibuja a la derecha de la página VOICE es el real: se calcula con los ajustes actuales, así que podés ver cómo cambian el decay y el clic mientras girás las perillas. Hacé clic en él para tocar la voz.

## Cargar muestras

Cualquier voz puede tocar una muestra tuya: WAV, AIFF o FLAC, mono o estéreo, a cualquier frecuencia de muestreo. Hay cuatro maneras:

- Soltá el archivo sobre un pad: va a esa voz.
- Soltalo sobre la vista del golpe de la página VOICE, o en cualquier lugar de la pantalla: va a la voz elegida.
- Usá **Load sample...** en la página VOICE, o hacé clic derecho en un pad.
- Recorré tus carpetas en la página SAMPLES: un clic carga el archivo en la voz elegida y lo toca, así que podés escuchar una carpeta rápido haciendo clic de arriba para abajo en la lista. También podés arrastrar un archivo de la lista a un pad.

Cargar una muestra pasa la voz al motor digital con el kit User sample. Desde ahí pasa por la cadena que elijas, así que una muestra moderna se puede convertir en un golpe de 8 bits.

![La página SAMPLES: la lista de carpetas a la izquierda y, a la derecha, la voz que va a recibir el próximo archivo.](img/samples.jpg)

Las muestras de más de 20 segundos se recortan. Se guardan dentro de tu proyecto, comprimidas sin pérdida, hasta 8 MB para todas las voces juntas; más allá de eso solo se guarda la ruta del archivo, y el archivo tiene que seguir estando ahí cuando vuelvas a abrir el proyecto.

## El secuenciador

Pallor tiene 16 patrones. Cada patrón tiene dos variaciones, **A** y **B**, y un **Fill**. Un patrón tiene 16 pistas, una por voz, de hasta 64 pasos.

![La página PATTERN con Body Discipline: el banco de patrones a la izquierda, la grilla de pasos y, abajo, el carril de velocidad de la voz elegida.](img/pattern.jpg)

### Elegir qué editás

A la izquierda de la página PATTERN, hacé clic en un número para editar ese patrón y en **A**, **B** o **FILL** para editar esa variación. El patrón que está sonando tiene una lucecita en su número. **Follow** hace que la grilla salte a lo que esté sonando, algo útil mientras corre una canción. **PLAY THIS** hace que el patrón que estás editando sea el que suena.

### Escribir pasos

- Hacé clic en un paso para prenderlo o apagarlo; arrastrá a lo largo de una fila para pintar varios.
- Con **Shift**-clic ponés o sacás un acento (el paso se pone rosa).
- Con **Alt**-clic sumás repeticiones (ratchets) hasta 4; con **Cmd**-clic (Ctrl en Windows y Linux) bajás la probabilidad de a 25 %.
- Con clic derecho en un paso tenés todo: acento, velocidad, probabilidad, ratchet, condición, flam y nudge.
- Hacé clic en un nombre a la izquierda para elegir esa voz; con clic derecho le fijás a la pista su propio largo, la corrés a la izquierda o a la derecha, la copiás, la pegás o la borrás.

Un paso muestra lo que hace: más brillante quiere decir más fuerte, un paso lleno a medias tiene probabilidad, las barritas abajo son ratchets, un punto en la esquina es un flam, una etiqueta corta es una condición y una línea fina es un nudge.

### Los carriles

Debajo de la grilla, los carriles editan un valor de cada paso de la voz elegida. Elegí el carril con su pestaña y dibujá a lo largo de los pasos:

![Los carriles con el de probabilidad abierto: cada barra es la chance de que suene su paso.](img/lanes.jpg)

- **Velocity**: con cuánta fuerza suena cada paso.
- **Probability**: la chance de que suene un paso, de a 5 %. La chance la deciden la posición del paso y la **Chance seed** (SETUP): la misma semilla toca las mismas variaciones cada vez que renderizás, y un compás distinto toca una elección distinta.
- **Ratchet**: de 1 a 8 repeticiones dentro del paso.
- **Condition**: hacé clic en un paso para pasar por Always, Fill (solo en las pasadas con fill), Not fill, 1:2 (la primera de cada dos pasadas), 2:2, 1:4, 4:4, First (solo la primera pasada después de que arranca el patrón) y Not first. Con clic derecho, un menú.
- **Flam**: una nota de adorno más suave antes del golpe, de 0 a 40 ms.
- **Nudge**: toca el paso adelantado o atrasado, hasta medio paso.
- **Accent**: hacé clic o arrastrá para poner y sacar acentos.

Un doble clic en un paso de un carril devuelve su valor al de partida.

### Largo, paso y polimetría

**LENGTH** fija cuántos pasos dura el patrón, de 1 a 64 (los botones 16, 32 y 64 son atajos). **STEP** fija el largo de un paso: 1/32, 1/16, 1/8, o los tresillos 1/16T y 1/8T.

Una pista puede tener su propio largo (clic derecho en su nombre): un charles de 12 pasos sobre un patrón de 16 se corre respecto del compás en cada vuelta, y el patrón sigue tocando con su propio largo. Esto es polimetría, la manera de sacar un loop de cuatro compases de un solo compás.

### Swing

El swing atrasa la segunda nota de cada par de semicorcheas (o de corcheas, se elige en SETUP) hasta un porcentaje del par: 50 % es derecho, 66,7 % es un shuffle de tresillo, 75 % un ritmo con puntillo. La perilla **Swing** de la barra de arriba lo fija para todos los patrones; **SWING** en la página PATTERN le da a un patrón su propio valor, o **GLOBAL** para seguir a la perilla. El swing funciona con pasos de 1/32, 1/16 y 1/8; los pasos de tresillo nunca tienen swing.

### La biblioteca

**LIBRARY...** carga uno de 52 juegos de patrones (A, B y Fill, por género, con su tempo a la vista) en el patrón que estás editando. Su swing viene con él. **COPY** y **PASTE** mueven un patrón entero; **CLEAR** lo vacía.

### Modos de reproducción y fills

En la página SONG, **Play mode** elige qué suena:

- **A** o **B**: esa variación del patrón actual, en loop.
- **A > B**: A y B alternados.
- **SONG**: la cadena de canción.

El patrón actual se elige en la fila de números de abajo (o con PLAY THIS). Un cambio entra al final de la pasada que está sonando, así que siempre queda a tiempo con el compás.

Una pasada con fill toca el Fill del patrón en vez de A o B. Apretá **Fill** en el panel para tocar fills hasta que lo vuelvas a apretar (su luz queda prendida), o poné **Auto fill** para tocar el Fill cada 2, 4, 8 o 16 pasadas. Los pasos con las condiciones Fill y Not fill también reaccionan a las pasadas con fill, así que el patrón A puede tener sus propios cambios chicos en esos compases. Si el Fill de un patrón está vacío, suena el patrón A o B con sus condiciones de fill.

### La cadena de canción

La canción es una cadena de hasta 64 entradas. Cada entrada toca el A o el B de un patrón una cantidad de veces, y puede terminar con su fill.

![La página SONG: el modo de reproducción y el auto fill arriba, la fila de patrones y la canción como fichas.](img/song.jpg)

- **+ ADD** (o la ficha +) agrega al final el patrón que estás editando.
- Hacé clic en una ficha para elegirla y después fijá su patrón, su variación, las repeticiones (**repeats**, con - y +) y **Ends with fill** en la fila de abajo.
- Arrastrá una ficha para moverla, o usá **MOVE**. **DUPLICATE** y **DELETE** actúan sobre la ficha elegida; **CLEAR SONG** vacía la cadena.

Mientras suena la canción, la ficha actual está iluminada y una línea muestra cuánto avanzó. Al llegar al final, la canción vuelve a empezar. Cuando el host salta a otra posición, Pallor toca lo que corresponde a esa posición, como si hubiera tocado toda la canción desde el principio.

### Seguir al host

Con **Follow host transport and tempo** prendido (SETUP, prendido por defecto), Pallor toca cuando toca el host, al tempo del host, enganchado a sus compases. Cuando el host está parado, o con Follow apagado, **Run** arranca el reloj interno a **Tempo**.

## Efectos

Cada voz manda a dos efectos, y todo pasa por el master. La página FX tiene los tres.

![La página FX: Space en el envío A, Echo en el envío B y la cadena del master con el medidor de reducción de ganancia del compresor glue.](img/fx.jpg)

### Envío A: Space

- **Gated**: la sala con gate de los 80: una sala densa, aplastada y cortada por un gate que se abre con lo que le mandás. **Gate** fija cuánto tiempo queda abierto.
- **Nonlinear**: una ráfaga pareja de reflexiones que se corta en seco después del tiempo de Gate, el otro sonido de caja famoso de los 80.
- **Reverse**: la misma ráfaga, subiendo en vez de bajar.
- **Plate**, **Spring**, **Hall** y **Room**: los espacios clásicos; hall y room funcionan con el ancho de banda y el retorno de 12 bits de las reverbs digitales de la época.

**Size**, **Pre-delay**, **Shape** (en nonlinear y reverse: la envolvente de la ráfaga, que baja, queda pareja o sube), **Tone**, **Low cut**, **Width** y **Return** lo completan.

### Envío B: Echo

- **Digital 12-bit**: el delay digital limpio, apenas granulado, de mediados de los 80.
- **Tape**: eco de cinta con tres cabezales, wow y flutter, y la saturación de la cinta en el lazo.
- **BBD**: un delay de bucket-brigade, más oscuro a medida que el tiempo se alarga.
- **Chorus**, **Ensemble**, **Jet Flanger** y **Phaser**: efectos de modulación. Devuelven solo la señal con efecto, así que es la mezcla con el sonido seco la que hace el chorus, el peine o las muescas.

**Time** está siempre a tiempo con el tempo, de 1/32 a un compás. **Feedback**, **Tone**, **Rate**, **Depth**, **Width** y **Return** lo completan.

### Master

- **Lo-fi**: todo el kit a través de un sampler: 12 bits a 26 o 40 kHz, u 8 bits a 22 u 11 kHz, con su **Lo-fi mix**.
- **Tape**: el master a través de cinta, con **Tape drive**. Agrega 54 muestras de latencia, que Pallor le informa al host.
- **Glue**: un compresor de bus con **Ratio**, **Attack** y **Release** en los pasos de los compresores de consola (el release Auto se adapta al material), **Glue mix** para compresión en paralelo y un medidor de reducción de ganancia.
- **Exciter** y su **Freq**: brillo agregado por encima de la frecuencia elegida.
- **Volume**: el nivel de salida (el mismo que la perilla de la barra de arriba).

## El mixer

La página MIXER tiene un canal por voz: **M** (mute) y **S** (solo), Pan, Send A, Send B, un fader de nivel con su medidor y el bus de salida. A la derecha, los retornos de Space y Echo y el volumen general. Hacé clic en un canal para elegir su voz. Se pueden poner varias voces en solo a la vez.

![La página MIXER: dieciséis canales con mute, solo, pan, envíos, nivel, medidor y bus.](img/mixer.jpg)

## MIDI

### Tocar Pallor por MIDI

Las notas del DAW o de un controlador tocan las voces, en cualquier canal. El mapa de notas se elige en SETUP:

| Voz | Nota GM | Chromatic |
|---|---|---|
| Kick | 36 (también 35) | 36 |
| Snare | 38 (también 40) | 37 |
| Clap | 39 | 38 |
| Rim | 37 | 39 |
| Closed Hat | 42 (también 44) | 40 |
| Open Hat | 46 | 41 |
| Crash | 49 (también 55, 57) | 42 |
| Ride | 51 (también 53, 59) | 43 |
| Tom 1 | 41 (también 43) | 44 |
| Tom 2 | 45 | 45 |
| Tom 3 | 48 (también 47) | 46 |
| Tom 4 | 50 | 47 |
| Cowbell | 56 | 48 |
| Clave | 75 (también 76, 77) | 49 |
| Tambourine | 54 (también 69, 70, 82) | 50 |
| FX | 52 | 51 |

GM es el mapa de la mayoría de los controladores de batería y de los racks de batería de los DAW. Chromatic pone las dieciséis voces en las dieciséis notas de 36 (C1 en la mayoría de los DAW) a 51, una por pad de un controlador de 4 × 4. Una nota igual o mayor que **Accent at** (120 por defecto) suena con acento.

### Salida MIDI

Con **Send the sequencer as MIDI** prendido, Pallor manda cada paso que toca como una nota, con el mismo mapa de notas, en el **Channel** elegido (10 por defecto). Los pasos con acento salen con velocidad 127. **Gate** fija cuánto dura cada nota, de 2 a 1.000 ms (30 ms por defecto); una nota siempre termina antes de que vuelva a sonar la misma nota. Cuando el transporte para o salta, se terminan todas las notas que siguen prendidas. Usalo para tocar otra caja de ritmos o un sampler con el secuenciador de Pallor, o para grabar el patrón como MIDI.

En Ableton Live, usá la versión VST3 para la salida MIDI: Live no toma MIDI de los Audio Units.

### Arrastrar MIDI

**DRAG MIDI** en la página PATTERN tiene el patrón que estás editando como archivo MIDI. Arrastralo a una pista MIDI de tu DAW para tener el patrón como notas, con su swing, sus ratchets y sus flams, en el mapa de notas elegido. Si en cambio hacés clic, guardás el archivo `.mid`.

## Salidas múltiples

Pallor tiene una salida estéreo principal y seis más: Kick, Snare/Clap, Hats/Cymbals, Toms, Perc y FX. Con **Outputs** en **Per voice** (SETUP), cada voz va a su bus (se cambia en la página VOICE o en el MIXER). Los envíos y los efectos del master quedan en la salida principal. Un bus que el host no habilitó va a la salida principal, así que nunca se pierde nada.

En Ableton Live:

1. Poné Pallor en una pista MIDI y poné **Outputs** en **Per voice** en SETUP.
2. Creá una pista de audio por cada bus que quieras, por ejemplo una para el bombo y otra para la caja y las palmas.
3. En cada pista de audio, poné **Audio From** en la pista de Pallor y, en el segundo menú, elegí la salida (Kick, Snare-Clap, Hats-Cymbals, Toms, Perc o FX).
4. Poné el **Monitor** de esas pistas de audio en **In**.
5. Cada voz cuyo bus no ruteaste sigue sonando en la pista de Pallor.

En Logic Pro, insertá Pallor como instrumento multisalida y agregá los canales auxiliares con el botón + de su canal. En Reaper, abrí el botón I/O del plugin y ruteá las salidas extra a pistas nuevas.

Todas las salidas quedan alineadas con la principal, incluida la latencia de la cinta del master.

## Presets

Los 65 presets cubren 13 géneros, cinco por género: un kit, sus efectos, sus patrones y muchas veces una canción. La página PRESETS los lista por género con una descripción corta y el tempo.

![La página PRESETS: los géneros a la izquierda y los presets del género con sus descripciones.](img/presets.jpg)

Elegí qué carga un clic: **SOUND + PATTERNS** (todo), **SOUND ONLY** (el kit, el mixer y los efectos, conservando tus patrones) o **PATTERNS ONLY** (patrones y canción, conservando tu sonido). La segunda es la manera más rápida de escuchar tu propio patrón a través de cada kit.

**SAVE AS...** guarda todo como un preset tuyo, en el género User: kit, efectos, patrones, canción y las rutas de tus muestras. **DELETE** borra el preset de usuario elegido y **OPEN FOLDER** muestra dónde se guardan.

Cada preset está nivelado a unos −18 LUFS, así que podés compararlos sin que gane el que suena más fuerte.

## Setup

![La página SETUP: reloj, swing, acento y probabilidad, salidas y MIDI.](img/setup.jpg)

- **Follow host transport and tempo**: prendido por defecto.
- **Swing on**: corcheas o semicorcheas.
- **Accent**: cuánto suma cada acento, para todas las voces; **Chance seed**: la semilla de la probabilidad (1 a 999).
- **Outputs**: Main only o Per voice.
- **Send the sequencer as MIDI**, **Channel**, **Gate** y **Accent at**: ver MIDI.
- **Note map**: GM drums o Chromatic 36-51, para la entrada y la salida MIDI.

## Consejos por género

- **Dark wave**: un bombo bridged-T, una caja de 8 bits a la sala con gate, charles parejos en corcheas a través del chorus del envío B. Night Shift Mercury es el lugar para empezar.
- **Coldwave**: el kit Hexa 84 o los modelos Hexagonal, patrones despojados, la sala nonlinear; mantené el tempo entre 120 y 140.
- **Post-punk**: los toms llevan el ritmo. Dales a los cuatro toms decays cortos, panealos de lado a lado y dejá la caja en el contratiempo en una sala chica.
- **Minimal wave**: el bombo Compact y los charles Noise Box, una sensación de ritmo de fábrica, 12-bit 26k en el lo-fi del master.
- **EBM**: el bombo VCO Punch, palmas en el contratiempo, charles en semicorcheas, el Glue a 4:1 con ataque rápido y un cencerro metálico.
- **Synthpop**: Studio 82 o Pop 84, palmas y pandereta, la placa.
- **Goth rock**: tempos rápidos (140 a 160), un galope en el bombo, toms en los fills, el hall.
- **Industrial**: el kit Foundry, Drive y Bits en los metales, el jet flanger, la cinta bien exigida.
- **New beat**: lento (100 a 115), pesado, la cadena 12-bit 26k, las palmas con gate.
- **Italo y hi-NRG**: bombo en negras, charles abiertos a contratiempo, los toms Hexagonal haciendo "piu" en los fills.
- **Witch house**: alrededor de 70 BPM, charles en ratchets, caídas de sub en la voz FX, la sala reverse, 8-bit 11k en el master.
- **Darkwave y trip-hop de los 90**: un break con swing a 70 a 95 BPM en 12 bits, notas fantasma con velocidad y probabilidad bajas.
- **Dark ambient**: probabilidad por todos lados, decays largos, el ride y el metal de la voz FX a un hall largo y al eco de cinta.

## Consejos y problemas

- **No suena nada cuando le doy play en el DAW.** Fijate que Follow host esté prendido (SETUP) y que el modo de reproducción no sea SONG con la cadena vacía (en ese caso Pallor toca A).
- **Los charles suenan finos cuando se corta el abierto.** Los charles cerrado y abierto están en el grupo de choke 1; poné el Choke del charles abierto en NONE para que suenen juntos.
- **Un cambio de patrón no pasa enseguida.** Entra al final de la pasada que está sonando, para quedar a tiempo con el compás.
- **No sale MIDI en Ableton Live.** Usá la versión VST3, y en la pista que recibe poné MIDI From en la pista de Pallor.
- **Falta mi muestra cuando vuelvo a abrir el proyecto.** Solo 8 MB de muestras viajan dentro del proyecto; el resto se guarda por su ruta. Dejá los archivos donde estaban, o guardá el proyecto con menos muestras o más cortas.
- **La probabilidad siempre toca lo mismo.** Es a propósito: un render se puede repetir. Cambiá la Chance seed para tener otras elecciones.

## Especificaciones

| | |
|---|---|
| Formatos | VST3 (macOS, Windows, Linux), Audio Unit (macOS), LV2 (Linux), versión independiente, pallorrender |
| Voces | 16, cada una con un motor analógico (44 modelos de circuito) y un motor digital (8 kits y una muestra de usuario) |
| Secuenciador | 16 patrones × A, B y Fill, hasta 64 pasos, largo por pista, cadena de canción de hasta 64 entradas |
| Por paso | Velocidad, acento, probabilidad, ratchet de 1 a 8, flam, nudge ±50 %, nueve condiciones |
| Efectos | Space (7 tipos), Echo (7 tipos), lo-fi, cinta, compresor glue y exciter en el master |
| Salidas | Estéreo principal y seis buses estéreo |
| MIDI | Entrada: mapa de notas GM o cromático. Salida: el secuenciador, con gate ajustable |
| Latencia | 24 muestras; 78 con la cinta del master |
| Presets | 65 en 13 géneros; una biblioteca de 52 juegos de patrones (156 patrones) |
| Licencia | GNU Affero General Public License v3 |
