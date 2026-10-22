# Manual de Halation

## Qué es Halation

Halation es un delay y una reverb en un solo espacio. El mismo conjunto de líneas de retardo toca ecos separados sobre el pulso o una sala entera, y un solo control, **Delay a Space**, va de uno a otro: de repeticiones limpias, pasando por ecos que se funden entre sí, a una sala cuya cola podés estirar hasta que no termine nunca.

Alrededor de ese espacio hay diez módulos que lo llevan a otro lado: congelarlo, subirlo una octava, tocarlo al revés, dispersarlo en granos, correrlo en una nube quieta, afinarlo en un acorde, cortarlo sobre el pulso, hacer que sus paredes respiren, que se corra cuando tocás o cortarlo en seco como las baterías de los años ochenta.

La ventana muestra lo que escuchás. La sala de yeso de arriba se dibuja a partir del efecto mismo: cada eco es un pulso de luz que sale de la fuente, viaja, rebota en las paredes y vuelve. Su tamaño es su nivel y su color es su brillo (tibio para los sonidos oscuros, frío para los brillantes). En una sala los pulsos se parten en cada pared y se vuelven niebla; cuando congelás, la luz se queda donde estaba.

El nombre viene de la fotografía en película: la halación es el halo suave que se forma alrededor de las luces fuertes cuando se reflejan dentro de la película. Eso es una cola: el sonido, brillando después de haberse ido.

![Halation con Cathedral of Glass: la sala de yeso arriba, la barra Delay a Space, las cinco perillas principales, carácter y sala, y la fila de módulos con los ajustes de Shimmer.](img/overview.jpg)

## Instalación

Bajá el paquete para tu sistema desde el sitio y descomprimilo.

En macOS:

1. Copiá `Halation.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `Halation.component` (Audio Unit) a `~/Library/Audio/Plug-Ins/Components/`.
2. Volvé a escanear los plugins en tu DAW.
3. `Halation.app` es la versión independiente y `halationrender` el renderizador de línea de comandos.

Los archivos no están notarizados. Si macOS no los deja abrir, corré `xattr -dr com.apple.quarantine` seguido de la ruta de cada archivo.

En Windows:

1. Copiá `Halation.vst3` a `C:\Program Files\Common Files\VST3\`.
2. Volvé a escanear los plugins en tu DAW.
3. `Halation.exe` es la versión independiente y `halationrender.exe` el renderizador de línea de comandos.

El Audio Unit es un "efecto musical": en Logic Pro aparece en **Audio Units > Audio Tools** en los slots de efecto de una pista de audio, y también como efecto controlado por MIDI, que es como el Resonator sigue las notas que tocás.

## Primeros sonidos en un minuto

1. Poné Halation en una pista o en un envío (en un envío, llevá **Mix** a 100 %).
2. Abrí el menú de presets (el nombre en la barra de arriba) y elegí **Tape Echo 1974**. Tocá algo corto: escuchás ecos sobre el pulso.
3. Llevá **Delay a Space** despacio hacia la derecha. Los ecos empiezan a desdibujarse y después se vuelven una sala. Mirá cómo los pulsos se parten y sube la niebla.
4. Subí **Length**. La cola crece; arriba del todo, no termina nunca.
5. Hacé clic en **Shimmer** en la fila de módulos. La cola ahora sube una octava.
6. Hacé clic en **Freeze**. Lo que hay en la sala se queda ahí. Tocá encima y después volvé a hacer clic en Freeze para soltarlo.

Cada control se explica solo en la línea de ayuda de abajo de la ventana cuando lo señalás.

## La interfaz de un vistazo

De arriba hacia abajo:

- **Barra superior.** Nombre del preset con anterior y siguiente, **Store B** y la barra **Morph** (dos escenas, ver más abajo) y **Manual**.
- **La sala.** Lo que está haciendo el efecto, dibujado. Arriba a la izquierda, una frase que lo dice con palabras; arriba a la derecha, el gráfico de caída; abajo, los tiempos de los ecos sobre una grilla de dos compases; y un aviso cuando la realimentación se acerca a la autooscilación.
- **Delay a Space.** La barra larga.
- **Cinco perillas.** Time, Size, Length, Tone y Mix.
- **Ecos.** Echo pattern, Sync y Ping-pong.
- **Character y Room.** Cómo suenan los ecos y en qué tipo de sala se funden.
- **Módulos.** Diez botones y **More**. El panel de abajo muestra los ajustes del elegido.

## Cómo leer la sala

![En delay, cada eco es un pulso de luz que sale de la fuente, rebota y vuelve. La grilla de abajo muestra los tiempos de los ecos sobre dos compases del tempo del host.](img/room-delay.jpg)

- **Pulsos.** Cada vez que la entrada tiene un ataque, salen pulsos de la fuente. En delay vuelven al oyente como puntos separados, uno por eco, y la grilla de abajo marca dónde cae cada repetición en el compás.
- **Tamaño y color.** Un pulso grande es un eco fuerte. Los pulsos tibios (ámbar) llevan sobre todo graves, los fríos (azules) sobre todo agudos. Con un Tone oscuro los colores se entibian a medida que avanza la cola, porque los agudos mueren primero.
- **Niebla.** En una sala los pulsos se parten en cada pared en fragmentos más chicos, que dejan niebla atrás. Cuanto más densa la niebla, más densa la reverb.
- **Las paredes.** Size las mueve. Breathe hace que se muevan solas, y Collapse las cierra mientras tocás.
- **El gráfico de caída.** La línea recta es la cola cayendo a -60 dB con el Length actual; la línea naranja es el nivel que estás escuchando de verdad. Con Length al máximo dice "endless".

![En una sala, los pulsos se parten en cada pared y se vuelven niebla. La curva de abajo a la derecha es la cola cayendo hasta -60 dB.](img/room-space.jpg)

![Congelado: la luz se queda donde estaba y la sala se tiñe de violeta.](img/room-frozen.jpg)

## Delay a Space

![Delay a Space: el control que convierte ecos separados en una sala.](img/space.jpg)

A la izquierda, Halation es un delay: una línea limpia por lado, con los ecos exactamente donde los ponen Time y el patrón. A medida que vas a la derecha, las líneas empiezan a alimentarse entre sí, los tiempos de eco se abren hacia las medidas propias de la sala y los difusores corren cada repetición. A la derecha es una reverb.

Dos cosas se mantienen mientras lo movés: la cola conserva su largo (Length es un tiempo, no una cantidad de realimentación) y el nivel queda cerca de donde estaba. Así podés automatizar Delay a Space durante una canción sin saltos de volumen.

Entre 30 y 70 % está la zona más interesante: ecos que todavía se pueden contar, cada uno ya una sala chica.

## Time, Size, Length, Tone y Mix

![Time, Size, Length, Tone y Mix.](img/knobs.jpg)

- **Time.** El tiempo entre ecos. Con **Sync** prendido es una figura del tempo del host (de 1/32 a 4 compases, con valores con puntillo y tresillos); con Sync apagado va en milisegundos, de 1 ms a 4 s. Si lo cambiás mientras suena, se desliza como una cinta (ver Tape glide en More).
- **Size.** Qué tan grande es la sala, de 25 a 400 %. En delay casi no influye; en una sala fija qué tan separadas están las reflexiones. Tamaños chicos con un Length largo dan espacios metálicos y resonantes.
- **Length.** Cuánto dura el sonido: el tiempo que tarda la cola en caer 60 dB, de 0,2 s a 60 s. Arriba del todo la cola no termina nunca. En delay se lee como una cantidad de repeticiones.
- **Tone.** De oscuro a brillante: cuánto más rápido se apagan los agudos que el resto. Hacia la izquierda es como una sala con cortinas y alfombras; hacia la derecha, vidrio y azulejos.
- **Mix.** Cuánto efecto escuchás contra el sonido seco. En un envío, usá 100 %.

## Echo pattern, Sync y Ping-pong

![Pattern, Sync y Ping-pong para los ecos; Character y Room para cómo suenan y dónde se funden.](img/character.jpg)

El patrón reparte los ecos sobre el pulso: **Single**, **Dotted** (con puntillo), **Triplet** (tresillo), **Dotted + 1/4**, **Rain** (cuatro ecos que caen a tiempos desparejos), **Gallop** (galope), **Cascade** y **Swing**. **Ping-pong** manda los ecos de un lado al otro. Los patrones pesan más en delay; a medida que vas hacia Space se disuelven en la sala.

## Character

- **Digital.** Limpio, cada eco idéntico al sonido.
- **Tape.** Un eco de cinta: wow lento y flutter rápido, saturación tibia, un pequeño refuerzo en los medios graves y repeticiones que se oscurecen. **Tape wear** (en More) agrega desgaste: más ondulación y momentos en que el sonido se apaga, como una parte gastada de la cinta.
- **BBD.** El sonido oscuro y apenas granulado de los chips "bucket brigade" de los años setenta y ochenta. Cuanto más largo el tiempo, más oscuro, como en los de verdad.
- **Spring.** Un tanque de resortes: el goteo y el "boing" de la guitarra surf, hecho con una cadena de filtros pasa-todo estirados.

## Room

El tipo de sala en la que se funden los ecos: **Room** (chica y cercana), **Hall** (grande, pareja), **Plate** (brillante y densa desde el primer instante) y **Chamber** (una sala de piedra, de color más oscuro). Size escala la que elijas.

## Módulos

![La fila de módulos. Un botón encendido está prendido; el panel de abajo muestra los ajustes del elegido, acá el Resonator de Tuned Abyss.](img/modules.jpg)

Un clic en el botón de un módulo lo elige y muestra sus ajustes. Un clic en el módulo ya elegido lo prende o lo apaga. Un botón encendido está prendido.

- **Freeze.** El espacio retiene lo que tiene mientras Freeze esté prendido. **Bands** elige qué se congela: **All** (se detiene toda la sala), o **Lows**, **Mids**, **Highs** y **Lows + mids**, donde sólo se retienen esas bandas y el resto sigue moviéndose con lo que tocás. Congelar sólo los graves arma un bordón debajo de una melodía que sigue viva.
- **Shimmer.** Cada repetición sube un intervalo: **Octave**, **Fifth**, **Octave + fifth**, **Fourth**, **Major chord**, **Minor chord** u **Octave down**. Como pasa dentro del lazo, cada repetición sube un poco más: la cola se eleva como un coro. **Amount** fija cuánto del sonido transportado vuelve al espacio.
- **Reverse.** La cola suena al revés en tramos de **Length** (una figura), así que crece hacia el pulso siguiente. Con Shimmer prendido también, los tramos al revés suben.
- **Grains.** Fragmentos de la cola se dispersan de vuelta en el espacio, unos hacia adelante y otros hacia atrás, con **Grain size** de 20 a 500 ms. Granos chicos dan destellos; grandes, un recuerdo corrido.
- **Spectral.** La cola se corre en el tiempo y sus fases se dispersan, en una nube quieta y vidriosa, sin ataques. **Smear** fija cuánto.
- **Resonator.** La cola suena en una nota o un acorde. **Root** es la nota, **Chord** elige **Single**, **Fifth**, **Major**, **Minor**, **Sus** u **Octaves**. Con **Follow MIDI** prendido, suena en las notas que le mandes a Halation (en Ableton Live, poné la salida de una pista MIDI en Halation; en Logic, usá el efecto controlado por MIDI).
- **Stutter.** Sobre el pulso, se atrapa un tramo de la salida y se repite. **Chance** es cada cuánto pasa, **Length** el tamaño del tramo.
- **Breathe.** Una sala imposible. En modo **Breathe** las paredes se mueven despacio hacia adentro y hacia afuera (**Depth** y **Rate**); en modo **Collapse** se cierran mientras tocás y se vuelven a abrir en los silencios.
- **Duck.** El efecto da un paso atrás mientras tocás y florece en los huecos. El detector es el mismo que usa Lacquer para su compresión, con una liberación suave.
- **Gate.** La cola se corta en seco poco después de que termina el sonido, y **Hold** fija cuánto espera. Pensá en las baterías grandes de los ochenta, o en una voz con una sala enorme que se corta limpia.

## More

![More: los ajustes que dan forma a cada preset pero que rara vez hace falta tocar.](img/more.jpg)

- **Pre-delay.** Un hueco antes de que empiece el efecto, hasta 250 ms. Separa el sonido seco de su sala.
- **Diffusion.** Qué tan rápido los ecos se corren en un colchón. Con valores bajos las reflexiones se siguen oyendo, como el aleteo de una sala vacía.
- **Low cut.** Saca graves antes de que entren al espacio, de 20 a 800 Hz.
- **Bass decay.** Cuánto duran los graves comparados con los medios, de 30 a 200 %.
- **Modulation** y **Mod rate.** Un movimiento lento de las líneas de retardo que evita que las colas largas suenen metálicas. Con mucha profundidad se vuelve un chorus.
- **Width.** El ancho estéreo del efecto, de mono a más ancho que los parlantes.
- **Tape wear.** Ver Character: el desgaste de la cinta.
- **Envelope** y **Env. target.** Lo que tocás mueve un control: **Space**, **Size**, **Shimmer**, **Tone** o **Mix**. Los valores positivos lo suben cuando tocás fuerte; los negativos lo bajan.
- **Tape glide.** Prendido, cambiar Time dobla la afinación como una máquina de cinta que cambia de velocidad. Apagado, el tiempo nuevo entra con un fundido, sin doblar.
- **Output.** Nivel de salida del efecto, de -24 a +12 dB.

## Escenas y Morph

Halation guarda dos escenas. **A** es lo que dicen las perillas. **Store B** recuerda los ajustes del momento como **B**; desde ahí, la barra **Morph** de la barra superior lleva cada control de A a B.

Una automatización lenta de Morph es la forma más simple de tener un espacio que cambia a lo largo de un tema: un delay ajustado en la estrofa, una sala grande en el estribillo, o una sala que deriva durante diez minutos en una pieza ambient.

## Presets

Treinta presets en cuatro grupos:

- **Classic.** Tape Echo 1974, Analog Bucket, Clean Digital Quarter, Ping-Pong Machine, Plate Vocal, Small Room, Concert Hall, Spring Surf.
- **Ambient.** Cathedral of Glass, Endless Pad, Breathing Room, Rising Choir, Warm Blanket, Slow Tide, Tuned Abyss, Spectral Ice.
- **Rhythmic.** Dotted Rain, Stutter Grid, Gallop, Ducked Dub, Gated 1984, Swing Taps.
- **Experimental.** Ghost Before You, Broken Satellite, Collapsing Hall, Self-Oscillation, Reverse Shimmer, Frozen Lows, Delay to Cathedral, Grain Storm.

**Delay to Cathedral** trae su propia escena B: mové Morph y un delay con puntillo se convierte en una sala brillante e infinita con shimmer. En los demás presets no hay B hasta que guardes uno, y Morph no hace nada.

En Ableton Live, que no muestra la lista de programas propia de un plugin, los presets de fábrica aparecen en el navegador de Live en **Plug-Ins**, **Audio Tools Halation** (desplegá el plugin). Los instala el launcher; con el zip, copiá las carpetas de su carpeta **Presets** a `~/Library/Audio/Presets/Audio Tools/` (macOS).

## Sesiones largas, meditación y baños de sonido

Halation se pensó para música larga y lenta: colas infinitas, congelados que sostienen un acorde durante minutos, salas que respiran a una décima de hertz, shimmer que no deja de subir. Algunas cosas ayudan:

- **Infinito no quiere decir más fuerte.** Con Length al máximo, Freeze o autooscilación, un limitador dentro del lazo y un techo a la salida (-1 dBFS) mantienen el nivel a raya. Frenan picos, no volumen: fijá el volumen al empezar la sesión y revisalo cuando la cola ya se armó.
- **Usá Duck para una voz sobre un espacio.** Una guía que habla sobre un pad congelado se entiende, y la sala llena los silencios.
- **Automatización lenta antes que cambios bruscos.** Morph entre dos escenas a lo largo de minutos; Delay a Space se puede mover sin saltos.
- **Congelá y después tocá.** Congelá un acorde en Lows + mids y tocá una melodía encima: el bordón se sostiene y la melodía tiene su propia sala.

Halation es una herramienta para hacer sonido. No trata nada; en contextos terapéuticos, el o la profesional decide cómo usarla.

## El renderizador de línea de comandos

`halationrender` pasa un archivo por el efecto completo, el mismo motor que el plugin:

```
halationrender in.wav out.wav --preset "Cathedral of Glass" --tail 12 --bpm 92
halationrender in.wav out.wav --preset "Slow Tide" --tail 16 --at 9 on_freeze=1
halationrender in.wav out.wav --set space=60 --set length=80 --set on_shimmer=1
```

- `--preset` carga un preset por nombre. Sin él, el primero (Tape Echo 1974), como una instancia nueva en un DAW.
- `--tail` agrega segundos de silencio al final para que suene la cola (6 por defecto).
- `--bpm` es el tempo para los tiempos sincronizados (120 por defecto).
- `--set id=valor` fija un parámetro antes de empezar; `--at segundos id=valor` lo cambia en ese momento.

Imprime lo que tardó como porcentaje de un núcleo de CPU.

## Cómo sacarle el jugo

- **En un envío** podés compartir un espacio entre varias pistas y tratarlo como una sala real. **En inserción** conviene para los ecos con carácter (cinta, BBD, resorte) y para los módulos experimentales.
- **Low cut** entre 150 y 300 Hz evita que las salas grandes embarren la mezcla.
- **Tone** hacia la izquierda y **Bass decay** un poco por debajo de 100 % dan las colas más naturales.
- **Duck** suele andar mejor que bajar Mix: el efecto queda grande donde hay lugar para él.
- **Spectral, Grains y Shimmer** se suman entre sí; probalos de a uno primero.

## Límites

- El congelado por bandas no es perfectamente limpio: cerca de los bordes entre bandas se filtra un poco del sonido en vivo en la parte congelada.
- Spectral y Freeze por bandas usan un análisis espectral con una ventana de unos 43 ms: los ataques muy secos pierden algo de definición al pasar por ellos. Es parte de su sonido.
- Follow MIDI necesita un host que mande MIDI a un efecto.
- Con Tape glide prendido, los cambios de Time doblan la afinación a propósito; para automatizar sin doblar, apagalo.

## Notas técnicas

- **El espacio.** Ocho líneas de retardo con una matriz de mezcla ortogonal cuyo ángulo va de la identidad (ecos separados) a una mezcla total tipo Hadamard (una sala densa). Los largos de las líneas se interpolan en pasos logarítmicos entre los tiempos del patrón de ecos y las medidas propias de la sala. La caída de cada línea se calcula a partir de su largo y del tiempo de Length, así que la cola mantiene el mismo T60 en cualquier posición de Delay a Space.
- **Amortiguación.** Tres bandas por línea (cruces en 250 Hz y 2,5 kHz) con sus propios tiempos de caída, a partir de Tone y Bass decay.
- **Modulación.** Cada línea tiene su propia modulación aleatoria lenta con interpolación de Hermite; el carácter de cinta agrega wow (unos 0,5 Hz), flutter (unos 7 Hz) y una deriva.
- **Seguridad.** Un limitador suave en cada línea, un limitador de energía en el lazo y un techo de -1 dBFS a la salida. No pasan ni NaN ni niveles desbocados, comprobado en las pruebas automáticas con todos los módulos prendidos y Length en infinito.
- **Latencia.** Cero muestras. El largo de la cola se informa al host (infinito con Length al máximo o con Freeze prendido).
- **Determinismo.** La misma entrada con los mismos ajustes da la misma salida bit a bit, con cualquier tamaño de bloque.
- **CPU.** Con todos los módulos prendidos, 2,6 % de un núcleo en un Apple M4. Los presets van de 0,4 a 1,6 %.
- **Probado** con pluginval en rigor 10 y auval en macOS, a 44,1, 48 y 96 kHz.
