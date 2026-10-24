# Manual del AT42

El AT42 es una máquina de drones ambient libre y de código abierto para macOS y Windows, como instrumento VST3 y Audio Unit y como aplicación independiente. Recrea la familia Elta Music SOLAR 42, con el último modelo (el 42N) como punto de partida, y suma lo que el hardware no tiene: MIDI, sincronía con el host, presets, un control de ancho estéreo y una página de ajustes. Este manual explica cada control, cómo funcionan las voces y la patchera, y cómo pasar del preset Init a un drone completo.

## Qué es

El AT42 es un instrumento semimodular con ocho voces fijas. No hay asignación ni robo de voces: cada voz está siempre ahí, y el mezclador y las envolventes deciden qué escuchás.

- Cuatro voces drone clásicas (DRONE 1, 2, 4 y 5), cada una con cinco osciladores de sierra, un orbe sensible a la luz y una envolvente de ataque y release.
- Dos voces de ruido (DRONE 3 y 6) según el diseño de Papa Srapa: un modulador de onda cuadrada, un oscilador de tono con FM y AM, ruido blanco y un sample and hold.
- Dos VCO (VCO A y B) con seis formas de onda, FM lineal, sync y su propia envolvente ADSR.
- Un mezclador estéreo de 10 canales que también recibe una entrada de audio externa (EXT IN) y un preamp.
- Dos filtros de 12 dB estilo Polivoks, pasa bajos o pasa banda, seguidos de una distorsión estéreo.
- Un efector doble con 13 cartuchos de 3 programas cada uno.
- LFO A y B, un joystick, un secuenciador de 5 pasos con su propio reloj (el pulser) y un seguidor de envolvente.
- Un teclado táctil de 12 placas con modos teclado, arpegiador y secuenciador de 16 pasos.
- Seis teclas drone, que también se tocan con las notas MIDI de C1 a F1.
- Una patchera de 62 jacks con las conexiones internas del hardware (normales), así el instrumento suena antes de enchufar un solo cable.

El recorrido de la señal: las ocho voces, EXT IN y PREAMP entran al mezclador, donde cada canal tiene volumen y paneo. La mezcla estéreo pasa por una saturación suave del bus y por el control WIDTH, después por FILTER L y FILTER R y la distorsión, después por los dos motores del efector (uno por lado) y al final por MASTER.

La gráfica del panel es propia. Elta Music y SOLAR 42 se nombran sólo para identificar el instrumento que inspiró esta recreación; los algoritmos del efector son del AT42.

## Primeros pasos

### Instalación en macOS

El AT42 necesita macOS 11 o posterior y funciona de forma nativa en Apple silicon e Intel.

1. Descomprimí el archivo descargado.
2. Copiá `AT42.vst3` a `~/Library/Audio/Plug-Ins/VST3/`.
3. Copiá `AT42.component` a `~/Library/Audio/Plug-Ins/Components/`.
4. Reescaneá los plugins en tu DAW (en Ableton Live: Settings, Plug-Ins, Rescan).

`AT42.app` es la versión independiente.

> Los builds tienen firma ad hoc y no están notarizados. Si macOS los bloquea, corré `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/AT42*.vst3 ~/Library/Audio/Plug-Ins/Components/AT42*.component` en la Terminal. El INSTALL.txt del paquete trae los mismos pasos.

### Instalación en Windows

El AT42 necesita Windows 10 u 11 de 64 bits y viene como VST3 y aplicación independiente.

1. Descomprimí el archivo descargado.
2. Copiá la carpeta `AT42.vst3` a `C:\Program Files\Common Files\VST3\` (pide permisos de administrador).
3. Reescaneá los plugins en tu DAW.

`AT42.exe` funciona sin DAW. Los binarios no están firmados, así que SmartScreen puede pedirte confirmación la primera vez.

### Cargarlo en un DAW

Insertá el AT42 en una pista de instrumento o MIDI. Recibe notas, pitch bend y controladores, y tiene una entrada de audio estéreo opcional llamada Audio In (Ext / Preamp). Si tu DAW deja mandar audio a un instrumento (muchas veces como side-chain), ese audio alimenta el canal EXT IN y el preamp; si no, los dos quedan en silencio. En la aplicación independiente elegí los dispositivos en sus ajustes de audio; en macOS te pide permiso de micrófono por el mismo motivo.

Arrastrá la esquina de la ventana para cambiar su tamaño, de la mitad a 1,4 veces el tamaño completo, con las proporciones fijas.

En Ableton Live, los presets de fábrica aparecen en el navegador de Live en Plug-Ins → Audio Tools AT42 (desplegá el plugin), instalados por el launcher o desde la carpeta Presets del zip. Live no muestra la lista de programas propia de un plugin; el menú de presets del editor los tiene todos. El AT42 tiene 230 parámetros, más que los 64 con los que Live llena solo el panel del dispositivo: hacé clic en Configure en la barra de título del dispositivo y tocá los controles que quieras ahí (Live automatiza hasta 128 a la vez). Piezo Percussion necesita audio en la entrada (un side-chain en Live).

### Tu primer sonido

El AT42 abre con el preset Init. Tocá una nota MIDI (o una placa del controlador táctil) y responde el VCO A por su camino normalizado, altura del teclado y envolvente A, sin cables. Las seis voces drone también están subidas en el mezclador, pero sus envolventes quedan cerradas hasta que algo las abra, igual que el hardware sin ninguna tecla apretada.

1. Hacé clic y mantené apretada la tecla drone 1, abajo a la derecha, bajo DRONE KEYS. El drone 1 suena mientras la mantenés y se apaga despacio al soltarla. La nota MIDI C1 hace lo mismo.
2. Para que siga sonando, encendé el botón **hold** de DRONE 1 (el botón gris claro a la izquierda de la voz).
3. Girá las cinco perillas TUNE de DRONE 1 para armar un acorde, y apretá botones MUTE para sacar osciladores.

Para un resultado inmediato, elegí un preset de las categorías Drone o Ambient: la mayoría tiene hold encendido y suena apenas se carga.

## Recorrida por la interfaz

![El panel completo con el preset Cathedral of A.](img/overview.jpg)

La barra de arriba tiene los controles de presets: **<** y **>** recorren los presets, el botón largo muestra la categoría y el nombre del preset actual y abre el menú de presets, **SAVE** guarda un preset de usuario y **RANDOM** tira un patch al azar. A la derecha, **SYNTH** muestra el panel y **SETTINGS** la página de ajustes. **MANUAL**, en el extremo derecho, abre este manual dentro del plugin, en inglés o en castellano; Esc o la cruz lo cierran.

El panel sigue al hardware. Arriba: DRONE 1 y 2 a la izquierda, DRONE 4 y 5 a la derecha, y entre ellos el DUAL EFFECTOR con los filtros en su franja inferior. Debajo: DRONE 3 en el extremo izquierdo, DRONE 6 en el extremo derecho, VCO A y VCO B con sus envolventes, y el VOICE MIXER en el centro. Después la fila de modulación: LFO A, JOYSTICK, 5 STEP SEQUENCER, PREAMP, ENVELOPE FOLLOWER y LFO B. Abajo de todo: el joystick, el TOUCH SENSITIVE CONTROLLER y las DRONE KEYS.

La barra de abajo muestra el modo, la escala y la tónica del teclado, una luz MIDI IN, los cartuchos cargados en cada lado, la carga de CPU y la versión.

### Cómo se usan los controles

- Perillas: arrastrá hacia arriba o hacia abajo; con Shift el movimiento es fino; la rueda del mouse también sirve. Doble clic vuelve la perilla a su valor por defecto. Mientras la girás, un globo muestra el nombre y el valor.
- Las perillas por pasos (octava y forma de onda de los VCO) se mueven de a una posición.
- Los botones quedan trabados: un clic enciende, otro apaga.
- Las llaves de dos posiciones cambian con cada clic. Las de tres posiciones van a la parte donde hacés clic: arriba, al medio o abajo.
- Clic derecho sobre cualquier control del panel abre MIDI Learn.
- Los orbes: pasá el mouse por encima, o apretá sobre uno y arrastrá, para hacerle sombra con la mano (ver Los orbes y la luz ambiente).
- Cada control y cada ajuste es un parámetro del host que podés automatizar, también la mano sobre cada orbe (Drone 1, 2, 4 y 5 Hand).

## Voces drone 1, 2, 4 y 5

![Los drones 1 y 2 con el preset Breathing Orbs: el LFO A va a las dos entradas cv.](img/drones.jpg)

Cada voz clásica tiene cinco osciladores de sierra sin entrada de 1 V por octava. Cada uno modela un circuito de relajación de un solo transistor: un capacitor carga a través del control TUNE y se descarga de golpe al llegar a la ruptura. Esa rampa apenas curva con una caída instantánea le da a los drones su cuerpo zumbón, de órgano.

La voz tiene tres filas, MUTE, TUNE y MOD, y cinco columnas, una por oscilador.

- **MUTE** (botones negros): silencia ese oscilador, para pasar de un racimo de cinco notas a un acorde, una quinta o un solo tono.
- **TUNE** (perillas chicas): la altura de cada oscilador dentro de su rango. Como en el original, el último tramo del recorrido (todo a la derecha) es mucho más sensible. El globo muestra la frecuencia en Hz antes de que VOLT y el orbe la muevan.
- **MOD** (botones grises): conecta ese oscilador al orbe. Con MOD apagado suena estable; con MOD encendido su altura sigue a la luz.

| Oscilador | Rango | Frecuencia |
|---|---|---|
| 1 | C0 a G2 | 16,4 a 98 Hz |
| 2 | B1 a G3 | 61,7 a 196 Hz |
| 3 | D3 a B4 | 146,8 a 493,9 Hz |
| 4 | E4 a C6 | 329,6 a 1046,5 Hz |
| 5 | G5 a D7 | 784 a 2349 Hz |

La ventanita negra de arriba a la derecha muestra un segmento rojo por cada oscilador que no está silenciado, y se ilumina más a medida que se abre la envolvente.

- **VOLT** (perilla turquesa): baja la tensión de alimentación de los cinco osciladores. Hacia la derecha transpone toda la voz hacia abajo y curva más la rampa. Pasada la mitad, la fuente cede con cada descarga y los osciladores se tironean entre sí: el acorde se vuelve una masa rugiente de modulación cruzada. Todo a la izquierda, los osciladores quedan exactamente en sus frecuencias de TUNE.
- **hold**: mantiene la voz abierta sin gate. Su envolvente también se abre, así que la salida env queda alta.
- **att**: ataque, de 1 ms a unos 5 s.
- **rls**: release, de unos milisegundos a unos 8 s (unos 30 s con el ajuste Drone Release).
- **cv**: cuánto de la entrada cv llega al LED rojo dentro del orbe.

El LED ámbar sigue a la envolvente. Los jacks son **gate** (entrada), **env** (salida, 0 a 10 V) y **cv** (entrada para el LED del orbe). La entrada gate está normalizada a la tecla drone del mismo número.

> Los drones derivan despacio, como los osciladores simples del original; el ajuste Drone Drift controla cuánto. Para acordes clavados en semitonos, poné Drone Tuning en Chromatic.

### Los orbes y la luz ambiente

El orbe de abajo a la derecha de cada voz clásica es una cúpula fotosensible. Una fotorresistencia queda en paralelo con el control TUNE de cada oscilador que tiene MOD encendido: más luz, más agudo. Tres cosas lo iluminan.

- La luz ambiente, que se fija con Ambient Light en la página de ajustes y es la misma para todos los orbes. Con su valor por defecto, encender MOD ya sube la altura un par de semitonos, así que afiná con MOD encendido si lo vas a usar.
- Tu mano: el mouse sobre un orbe le hace sombra. Cuanto más cerca del centro, más oscuro el orbe y más grave la altura. Pasá por encima para una sombra suave, o apretá sobre el orbe (un dedo en una pantalla táctil) para taparlo y arrastrá para alejar la mano o llevarla a otro orbe. Al soltar el botón la sombra vuelve a seguir al puntero; en una pantalla táctil se levanta. Las perillas, los botones y los jacks que rodean al orbe no le hacen sombra.
- El LED rojo de adentro, que maneja el jack cv escalado por la perilla cv. Como un LED de verdad, sólo enciende con tensión positiva, apenas por encima de 1,5 V, por eso los LFO unipolares (0 a 10 V) le quedan tan bien. La cúpula se tiñe de rojo cuando se enciende.

La fotorresistencia reacciona rápido cuando hay más luz y despacio cuando hay menos, así que la altura salta hacia arriba y vuelve deslizándose en una fracción de segundo. Por eso un LFO en cv no da un vibrato común sino un movimiento que respira, un poco rengo.

El orbe sólo cambia el sonido de los osciladores con MOD encendido. Con el puntero sobre un orbe, un globo muestra la mano (0 a 10) y cuántos osciladores de esa voz tienen MOD encendido; si no hay ninguno, avisa que hay que encender MOD. Con la luz de fábrica, MOD encendido sin mano sube un oscilador unos 2,6 semitonos y la mano justo sobre el centro lo vuelve a bajar unos 2,4.

La mano también es un parámetro del host para cada orbe: Drone 1 Hand, Drone 2 Hand, Drone 4 Hand y Drone 5 Hand, de 0 (sin mano) a 10 (tapado). Mover el mouse sobre un orbe lo escribe, así que el host puede grabarlo en un carril de automatización; también se puede dibujar o asignar a un CC con MIDI Learn. No se guarda en los presets ni los marca como modificados.

Presets con MOD encendido: Cathedral of A, Low Choir, Microtonal Fog, Shimmer Garden, Breathing Orbs, Tape Memory y Reverse Hymn. En los cinco que no son de orbes por nombre, uno o dos osciladores agudos por voz tienen MOD encendido y están afinados para que la luz ambiente los lleve a la nota del acorde: desde ahí, la mano los baja.

### Cómo se disparan los drones

![Las DRONE KEYS con las teclas 1 y 4 apretadas.](img/drone-keys.jpg)

Las seis voces drone (clásicas y de ruido) se abren de cuatro maneras.

- Las DRONE KEYS, ordenadas 1 y 4 arriba, 2 y 5 al medio, 3 y 6 abajo. El clic es momentáneo: la voz se abre mientras mantenés apretado el botón del mouse, y una luz roja muestra la tecla apretada.
- Las notas MIDI de C1 a F1, cuando MIDI Drone Keys está en C1-F1 (el valor por defecto). Esas notas entonces no tocan el teclado táctil.
- El jack gate de cada voz: cualquier tensión por encima de 1 V la abre, por ejemplo el gate del secuenciador, un LFO, el gate del seguidor de envolvente o la salida env de otro drone.
- El botón hold.

| Nota MIDI | Voz |
|---|---|
| C1 | DRONE 1 |
| C#1 | DRONE 2 |
| D1 | DRONE 3 |
| D#1 | DRONE 4 |
| E1 | DRONE 5 |
| F1 | DRONE 6 |

## Voces drone 3 y 6 (Papa Srapa)

![DRONE 3 con el preset Siren Tower.](img/noise-voice.jpg)

Las voces de ruido siguen los sintes de ruido caseros del artista ruso Papa Srapa: un modulador cuadrado lento y un oscilador de tono a ritmo de audio, más ruido blanco y un sample and hold. Pensá en sirenas, pájaros, monstruos espaciales, ráfagas, olas y viento.

- **rate**: frecuencia del modulador, una onda cuadrada de 0,1 a 30 Hz.
- **x10**: multiplica la velocidad por diez, hasta 300 Hz, y la FM se vuelve áspera y metálica.
- **fm** (encendido por defecto): el modulador dobla la altura del tono.
- **am**: corta el tono al ritmo del modulador.
- **mod**: profundidad de la FM, hasta unas cuatro octavas para cada lado.
- **divider**: suaviza el modulador antes de que llegue a la FM. Con valores bajos salta entre dos alturas (un trino); con valores altos se desliza (una sirena).
- **PITCH**: altura del tono, de C0 a C6 aproximadamente (16 Hz a 1 kHz).
- **hi**: lleva PITCH a un rango de C3 a E7 aproximadamente (131 Hz a 2,6 kHz).
- **NOISE**: suma ruido blanco; el tono baja pero nunca desaparece del todo.
- **hold**, **att**, **rls**: como en las voces clásicas.

Con los dos botones apagados tenés un tono continuo; con fm solo, barridos y trinos; con am solo, un tono cortado; con los dos, un barrido cortado. Para ruido, poné PITCH todo abajo, mod y divider todo arriba y NOISE al máximo.

Los jacks:

- **lfo** (salida): la cuadrada del modulador, 0 o 12 V.
- **cv** (entrada): modulación de la altura del tono, alrededor de una octava cada 5 V, con fm encendido o apagado.
- **gate** (entrada, normalizada a su tecla drone) y **env** (salida, 0 a 10 V).
- **in** (entrada): lo que muestrea el sample and hold, normalizado al ruido blanco de la propia voz.
- **clock** (entrada): cada flanco de subida por encima de 1,5 V toma una muestra. No pasa nada hasta que enchufes un reloj acá (la salida del pulser, un LFO cuadrado, la salida lfo de la misma voz).
- **out** (salida): la tensión retenida, de -5 a +5 V. El LED rojo S&H parpadea con cada muestra.

El sample and hold es sólo una tensión de control; mandá su out a la entrada 1v/oct de un VCO para melodías al azar, o al cv de un filtro.

## VCO A y VCO B

![El VCO A y la ENVELOPE A con el preset Berlin Five.](img/vco.jpg)

Los VCO modelan un chip oscilador de núcleo triangular del tipo AS3340, y por defecto se tocan desde el teclado táctil o por MIDI.

- **fm**: profundidad de la FM lineal desde la entrada fm. La FM lineal mantiene el centro de la altura y puede llevar la frecuencia a pasar por cero, para campanas y metales.
- Selector de octava (negro): LO, 64', 32', 16', 8', 4', 2' y 1'. 64' es C0 (16 Hz) y cada paso duplica, hasta C6 en 1'. LO queda alrededor de 2 Hz, para usar un VCO como modulador lento. En 8', la nota MIDI C3 (y la primera placa, con tónica C) toca C3, 131 Hz.
- **tune**: afinación fina, media octava para cada lado.
- **shape**: cuánto mueve el ancho de pulso la entrada shape.
- Selector de forma de onda (verde): sin, tri, saw, sqr y dos formas rotuladas morph.
- **pw**: ancho de pulso de sqr. En el primer morph va de sierra invertida (izquierda) a sierra (derecha) pasando por triángulo (centro); en el segundo pasa de seno a triángulo. Seno, triángulo y sierra lo ignoran.

Los jacks del VCO A son **1v/oct**, **fm**, **shape** y **out** (el oscilador crudo, de -5 a +5 V, antes de su envolvente). El VCO B tiene **sync** en lugar de out: cada vez que la tensión ahí sube por encima de 0 V, el VCO B reinicia su ciclo. Enchufá el out del VCO A ahí y barré el tune del VCO B para sonidos de sync.

Normales: las dos entradas 1v/oct siguen al v/oct del teclado, y la entrada fm del VCO B recibe la salida del VCO A. Subí la perilla fm del VCO B para FM entre dos osciladores sin cables.

### Envelope A y Envelope B

Cada VCO llega al mezclador sólo a través de su propia envolvente ADSR y su amplificador.

- **attack**: de 1 ms a unos 8 s.
- **decay** y **release**: de unos 2 ms a 10 s.
- **sustain**: el nivel que se mantiene mientras el gate está alto.
- **hold**: deja el amplificador abierto, así el VCO suena como drone; la envolvente sigue trabajando en su salida.
- **loop**: la envolvente repite ataque y decaimiento sola, ignorando sustain, con gate o sin él. Con tiempos cortos es un LFO más; con tiempos largos, una ola lenta.

El LED ámbar sigue a la envolvente. Los tres jacks sin rótulo son, de izquierda a derecha, la entrada de gate (normalizada al gate del teclado), la salida de la envolvente (0 a 8 V) y la entrada del amplificador (normalizada a la salida de su propia envolvente). Enchufá un LFO en la entrada del amplificador y maneja directamente el nivel del VCO.

## Mezclador y ancho estéreo

![El VOICE MIXER con el preset Fifth Machine.](img/mixer.jpg)

Diez canales, cada uno con paneo arriba y volumen abajo. Los nombres están en la franja negra.

| Canal | Fuente |
|---|---|
| DRONE 1, 2, 4, 5 | voces drone clásicas |
| DRONE 3, 6 | voces de ruido |
| EXT IN | la entrada de audio del plugin, sumada a mono |
| VCO A, VCO B | los VCO después de sus envolventes |
| PREAMP | la salida del preamp |

Los paneos son de potencia constante. El bus suma una saturación suave que crece con el ajuste Mixer Drive: si subís varias voces juntas, las pega y les da grano.

**WIDTH** (en la caja del efector, a la derecha de MASTER) fija el ancho estéreo antes de los filtros y del efector: todo a la izquierda es mono, el centro es normal y todo a la derecha duplica la diferencia entre los lados. El original no puede hacerlo.

## Filtros

La franja inferior de la caja DUAL EFFECTOR tiene FILTER L (lado izquierdo de la mezcla) y FILTER R (lado derecho). Son filtros de estado variable de 12 dB por octava cuyos integradores se quedan sin velocidad con señales fuertes, lo que da el carácter sucio del original. Con más resonancia no se pierden graves.

- **freq**: corte, de 20 Hz a 20 kHz.
- **res**: resonancia.
- **bp** (botoncito arriba de cada lado): pasa banda en lugar de pasa bajos.
- **cv L**, **cv R** (entradas): modulación del corte. cv R está normalizada a cv L, así un solo cable mueve los dos.
- **mod L**, **mod R** (perillitas debajo de las llaves de programa): profundidad de cv L y cv R, hasta una octava por voltio.
- **link**: FILTER R sigue freq, res y bp de FILTER L. Cada lado conserva su cv y su mod.
- **dist**: balance entre limpio y distorsionado después de los filtros.
- **gain**: cuánto entra a la distorsión; el volumen sube, así que cuidá MASTER.

Los dos lados difieren un poco, como dos circuitos analógicos; L/R Mismatch fija cuánto.

## Efector doble

![El DUAL EFFECTOR y los filtros con el preset Shimmer Garden.](img/effector.jpg)

Dos motores, uno por lado de la mezcla, cada uno con un programa de un cartucho. X, Y, Z y BLEND son compartidos.

- Ranura del cartucho: muestra lo que está cargado. Hacé clic para cargar un cartucho en Channel L, Channel R o Both channels.
- **program** (llaves L y R de tres posiciones): programa 1 (arriba), 2 o 3 para cada lado.
- **X**, **Y**, **Z**: los parámetros del programa (ver la tabla).
- **cv x**, **cv y**, **cv z** (entradas): suman una tensión a X, Y y Z; 10 V cubren todo el recorrido de una perilla y las tensiones negativas restan.
- **BLEND**: cantidad de efecto. Incluso al máximo la señal seca queda a la mitad de nivel.
- **MASTER**: el nivel de salida principal.

Cambiar el cartucho o el programa borra la memoria del motor, como recargar un cartucho. Programas distintos en cada lado dan un estéreo ancho y desparejo. El motor conserva los límites del procesador digital del original: unos 1,4 s de memoria de delay y un pasa bajos suave cerca de 14 kHz. El ajuste Effector Noise suma su soplido, que crece con MASTER.

### Cartuchos y programas

| Cartucho | Programa | X | Y | Z |
|---|---|---|---|---|
| CATHEDRAL | 1 Shimmer | octava arriba | octava abajo | decaimiento |
| CATHEDRAL | 2 Delay de octava | realimentación | tiempo | reverb |
| CATHEDRAL | 3 Space reverb | realimentación | tiempo | decaimiento |
| MAGIC | 1 Pitch delay | realimentación | tiempo | altura |
| MAGIC | 2 Pitch delay invertido | realimentación | largo del tramo | altura |
| MAGIC | 3 Pitch delay campana | realimentación | tiempo | altura |
| TIME | 1 Delay con reverb | realimentación | tiempo | reverb |
| TIME | 2 Delay chorus | realimentación | tiempo | profundidad |
| TIME | 3 Delay vibrato | realimentación | velocidad | profundidad |
| VIBROTREM | 1 Trémolo | profundidad | velocidad | reverb |
| VIBROTREM | 2 Vibrato | profundidad | velocidad | reverb |
| VIBROTREM | 3 Chorus | profundidad | velocidad | reverb |
| FILTER | 1 Auto wah | frecuencia | envolvente | reverb |
| FILTER | 2 HP y LP | corte pasa altos | corte pasa bajos | resonancia |
| FILTER | 3 Notch | notch 1 | notch 2 | resonancia |
| VIBE | 1 Phaser | profundidad | velocidad | reverb |
| VIBE | 2 Flanger | profundidad | velocidad | reverb |
| VIBE | 3 Flanger resonante | resonancia | velocidad | profundidad |
| PITCH SHIFTER | 1 SynthTaver | sub cuadrada | octava arriba | directo |
| PITCH SHIFTER | 2 Octaver | octava abajo | octava arriba | directo |
| PITCH SHIFTER | 3 Armonizador | altura 1 | altura 2 | mezcla de voces |
| INFINITY | 1 Reverb resonante | predelay | modulación del predelay | decaimiento |
| INFINITY | 2 Delay O.D.D. | realimentación | tiempo | altura |
| INFINITY | 3 Delay resonante | realimentación | largo | altura |
| STRING RINGER | 1 Anillo sintético | frecuencia | resonancia | sub |
| STRING RINGER | 2 Ring mod | frecuencia | velocidad de vaivén | reverb |
| STRING RINGER | 3 Ring mod S&H | frecuencia y glide | velocidad S&H | rango al azar |
| SYNTEX-1 | 1 Vibe synth | velocidad de vaivén | resonancia | sub |
| SYNTEX-1 | 2 Pulse synth | velocidad de trémolo | resonancia | sub |
| SYNTEX-1 | 3 Acid synth | tono | color | sub |
| DIGITAL | 1 Filter DAC | frecuencia de muestreo | corte | ganancia de entrada |
| DIGITAL | 2 LFO DAC | frecuencia de muestreo | velocidad del LFO | cantidad de LFO |
| DIGITAL | 3 Envelope crusher | frecuencia de muestreo | cantidad de envolvente | ganancia de entrada |
| GENERATOR | 1 FM tone | altura 1 | altura 2 | cantidad de FM |
| GENERATOR | 2 Ramp | velocidad del LFO | altura | modulación de altura |
| GENERATOR | 3 Voice | corte | altura | LP o HP |
| OCHRE | 1 Un disparo largo | tiempo | realimentación | umbral de disparo |
| OCHRE | 2 Un disparo corto | tiempo | realimentación | umbral de disparo |
| OCHRE | 3 Lazo libre | tiempo | realimentación | modulación del delay |

- Los controles de altura de MAGIC y de INFINITY 2 van de una octava abajo (izquierda) a una octava arriba (derecha), sin cambio en el centro. Las alturas del armonizador se mueven en semitonos, hasta una octava para cada lado.
- En CATHEDRAL 2 la realimentación pasa por un desplazador de octava arriba, así cada repetición sube.
- El directo de PITCH SHIFTER fija cuánto del original queda en el efecto.
- SYNTEX-1 cuadra la entrada, le suma una suboctava (Z) y se abre con el nivel de entrada: un sinte de bajo que anda mejor con un solo drone o un VCO.
- DIGITAL: X hacia la derecha baja la frecuencia de muestreo, así que la trituración crece.
- GENERATOR hace su propio sonido; el nivel de entrada sólo lo abre. En Ramp, Z al centro es sin movimiento de altura; en Voice, Z por debajo de la mitad es pasa bajos y por encima, pasa altos.
- OCHRE es un delay invertido. Los programas de un disparo tocan la grabación al revés una vez cada vez que la entrada supera el umbral de Z; el lazo libre repite sin parar.

## Fila de modulación

![LFO A, los controles del joystick y el secuenciador de 5 pasos con el preset Berlin Five.](img/cv-left.jpg)

### LFO A y LFO B

Salidas unipolares, de 0 a 10 V, porque los LED de los orbes sólo responden a tensión positiva.

- **wave**: de triángulo (izquierda) a cuadrada (derecha), con mezclas en el medio.
- **rate**: el LFO A es el lento (de 0,02 a 5 Hz aproximadamente) y el LFO B el rápido (de 0,2 a 40 Hz aproximadamente).
- Llave de velocidad (/6, x1, x10): divide la velocidad por seis o la multiplica por diez. El LFO A en /6 puede tardar minutos por ciclo; el LFO B en x10 llega a ritmo de audio.
- **out**: la salida; el LED azul la sigue.

Cada LFO puede seguir el tempo del host (ver MIDI y sincronía).

### Joystick

La palanca está en el extremo izquierdo, al lado del teclado. Arrastrá su capuchón; queda donde lo dejás, como el original que se traba en posición. Doble clic lo vuelve al centro.

- **x**, **y** (salidas): 5 V de recorrido para cada lado del offset, de -10 a +10 V en total.
- **offset x**, **offset y**: corren la ventana de cada eje. Todo a la izquierda da de -10 a 0 V, al centro de -5 a +5 V, todo a la derecha de 0 a +10 V. Usá ventanas positivas para los orbes.
- Los LED rojos muestran cuánto se aleja cada eje del centro.

La rueda de modulación también mueve el eje Y, y las dos posiciones se pueden automatizar.

### Secuenciador de 5 pasos

Una fuente de tensiones secuenciales en la tradición de Buchla.

- **pulser**: velocidad del reloj interno, de 0,3 a 30 Hz aproximadamente.
- **out** (salida): la cuadrada del pulser, de -10 a +10 V.
- **ext** (entrada): el reloj que hace avanzar al secuenciador, normalizado al pulser. Cada flanco de subida por encima de 2,5 V avanza un paso.
- Llave de etapas (3, 4, 5): largo de la secuencia.
- **step 1** a **step 5**: la tensión de cada paso, de 0 a 5 V. En un VCO, un semitono es alrededor de 0,083 V; el globo muestra voltios.
- Llaves gate: abajo, el paso manda gate; arriba, queda en silencio en la salida gate pero su tensión igual suena.
- **cv** (salida): el paso actual, de 0 a 5 V.
- **gate** (salida): 10 V mientras el reloj está alto en un paso con gate encendido.

Los LED rojos muestran el paso actual. El pulser puede seguir el tempo del host.

![PREAMP, ENVELOPE FOLLOWER y LFO B con el preset Piezo Percussion.](img/cv-right.jpg)

### Preamp

El hardware tiene un micrófono de contacto bajo el teclado. En el AT42 el preamp escucha la entrada de audio del plugin.

- **source** (entrada): normalizada a la entrada de audio del plugin, sumada a mono. Un cable acá la reemplaza, así también podés amplificar una tensión o una voz.
- **gain**: de 0 a 40 dB. El LED azul se enciende cuando el preamp satura.

Tiene su propio canal en el mezclador, PREAMP, y alimenta al seguidor de envolvente.

### Seguidor de envolvente

- **attack**, **release**: qué tan rápido sube (de 0,5 ms a 0,5 s aproximadamente) y baja (de 5 ms a 3 s aproximadamente).
- **env** (salida): el nivel de la entrada, de 0 a 10 V, que muestra el LED rojo.
- **gate** (salida): 8 V mientras el nivel supera un umbral, que muestra el LED verde. Sirve para disparar drones o envolventes desde un loop de batería o tu voz.

## Teclado táctil

![El teclado táctil y el joystick con el preset Arp Constellation y una placa apretada.](img/keyboard.jpg)

El TOUCH SENSITIVE CONTROLLER produce tensiones de control, no sonido: altura, gates y presión, que por defecto manejan los dos VCO y sus envolventes. En Init el VCO A está subido en el mezclador, así que las teclas lo tocan; subí VCO B para escuchar el segundo oscilador.

- Placas: hacé clic para tocar y arrastrá para deslizarte de una a otra. Más abajo en la placa es más presión. Las seis placas de la izquierda son la mitad izquierda; las seis de la derecha, la mitad derecha.
- Los dos botones rojos: transponen una octava hacia abajo o hacia arriba, de -2 a +4.
- La pantalla: un ajuste por vez. Clic para la página siguiente, clic derecho para la anterior.
- El encoder: arrastralo hacia arriba o hacia abajo, o usá la rueda del mouse encima, para cambiar el valor. Un clic sin girar pasa a la página siguiente.

Las placas siguen la escala: la placa 1 es la tónica y cada placa hacia la derecha es la nota siguiente de la escala, subiendo de octava cuando hace falta. Con Semitones las placas cubren una octava cromática; con una pentatónica, más de dos octavas.

| Página | Qué ajusta |
|---|---|
| MODE | Keyboard, Arpeggiator o Sequencer |
| SCALE | una de 19 escalas |
| ROOT | tónica, de C a B |
| OCTAVE | transposición, de -2 a +4 |
| ARP DIR | dirección del arpegiador |
| ARP HOLD | traba las notas |
| CLOCK | división: 1/4, 1/8, 1/16, 1/32 |
| TEMPO | tempo interno, de 10 a 300 BPM |
| PORTAMENTO | tiempo de deslizamiento |
| LEGATO | sin redisparo entre notas que se pisan |
| VIBRATO, VIB SPEED | profundidad y velocidad del vibrato |
| PRESSURE | modo de presión |
| RISE, FALL | tiempos de subida y bajada de la presión |
| SEQ LENGTH | de 2 a 16 pasos |
| SEQ RECORD | empieza y termina la grabación |

Las escalas: Semitones, Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian, Blues major, Blues minor, Pentatonic major, Pentatonic minor, Folk, Japanese, Gamelan, Gypsy, Arabian, Flamenco y Whole tone.

Los jacks de la columna central:

- **clock** (entrada): reloj externo para el arpegiador y el secuenciador. Los flancos de subida por encima de 1,5 V reemplazan al reloj interno; unos dos segundos después del último, vuelve el interno.
- **v/oct** (salida): 1 V por octava, normalizada a los dos VCO. 0 V es C3 con la octava en 0.
- **gate** (salida): el gate principal, 10 V, normalizado a las dos envolventes.
- **gate R** (salida): 10 V mientras está apretada una placa de la mitad derecha, o una nota MIDI de C4 para arriba.
- **press** (salida): presión, de 0 a 8 V.
- **reset** (entrada): un flanco de subida vuelve el arpegiador y el secuenciador al principio.

### Modo teclado

Un teclado monofónico: gana la última placa apretada, y al soltarla vuelve a una placa que siga apretada. El gate queda alto mientras haya alguna placa abajo. Cada nota nueva normalmente deja un hueco mínimo en el gate para que las envolventes arranquen de nuevo; con LEGATO encendido, no. PORTAMENTO desliza hasta un segundo aproximadamente; VIBRATO suma hasta un semitono, de 0,5 a 9,5 Hz (VIB SPEED).

### Modo arpegiador

Mantené varias placas y suenan de a una con el reloj, cada nota durante medio paso. ARP DIR: Forward o Backward (por altura), Ping-pong, Random u Order (en el orden en que las apretaste). Con ARP HOLD encendido las notas siguen sonando al soltar, y la próxima vez que apretás después de soltar todo empieza un acorde nuevo. El reloj es TEMPO dividido por CLOCK, el tempo del host o el jack clock.

### Modo secuenciador

1. Andá a SEQ RECORD (hacé clic en la pantalla o en el encoder hasta que aparezca).
2. Girá el encoder un paso (un arrastre corto o un paso de la rueda; cada paso en esta página prende o apaga la grabación). La pantalla muestra REC 0/16 y se borra la secuencia anterior.
3. Tocá hasta 16 notas en las placas o por MIDI; cada nota es un paso.
4. Girá el encoder otra vez para terminar.
5. Poné MODE en Sequencer y mantené una placa: la secuencia suena, transpuesta para que su primera nota caiga en la placa que mantenés.

SEQ LENGTH acorta el lazo sin borrar notas. El gate sólo se abre mientras hay una placa apretada, o después de apretar una con ARP HOLD encendido. La secuencia se guarda con los presets y con tu sesión.

### Modos de presión

- Pressure: dónde hiciste clic en la placa, o la velocity MIDI.
- ASR: sube mientras el gate está alto y baja cuando se cierra.
- AD: sube con cada gate nuevo y baja enseguida, aunque sigas apretando.
- Loop: sube y baja una y otra vez mientras el gate está alto.
- Random: un nivel al azar con cada gate nuevo.

RISE y FALL van de un par de milisegundos a unos 3 s. Mandá press al cv de un filtro o a la entrada de un VCA.

## Patchera

![El panel completo en negro con el patch Berlin Five.](img/patch.jpg)

Las salidas mandan una tensión y las entradas la reciben. Una entrada sin cable usa su normal, si tiene; un cable la corta.

- Para patchear: hacé clic en un jack y arrastrá hasta otro. Las salidas sólo se conectan con entradas; un anillo amarillo marca un destino válido.
- Una entrada acepta un cable; enchufar uno nuevo reemplaza al anterior.
- Una salida acepta fichas apiladas: mantené Shift y arrastrá desde una salida que ya tiene un cable.
- Para mover un cable, arrastrá una de sus puntas; soltalo sobre el panel vacío para sacarlo.
- Alt-clic (Option-clic en Mac) o clic derecho sobre un jack saca todos sus cables.
- Los colores rotan: rojo, amarillo, azul, verde, blanco y negro.

Los gates cuentan como altos por encima de 1 V en los gates de drones y envolventes, 1,5 V en las entradas de reloj, reset y sample and hold, y 2,5 V en el ext del secuenciador. Las tensiones se limitan a 15 V positivos o negativos, así los lazos de realimentación saturan en lugar de desbocarse.

| Jack | Caja | Tipo | Rango o uso | Normal |
|---|---|---|---|---|
| gate | DRONE 1, 2, 4, 5 | entrada | gate | su tecla drone |
| env | DRONE 1, 2, 4, 5 | salida | 0 a 10 V | ninguno |
| cv | DRONE 1, 2, 4, 5 | entrada | LED del orbe | ninguno |
| lfo | DRONE 3, 6 | salida | 0 o 12 V | ninguno |
| cv | DRONE 3, 6 | entrada | FM del tono | ninguno |
| gate | DRONE 3, 6 | entrada | gate | su tecla drone |
| env | DRONE 3, 6 | salida | 0 a 10 V | ninguno |
| in | DRONE 3, 6 | entrada | señal del S&H | ruido de la voz |
| clock | DRONE 3, 6 | entrada | reloj del S&H | ninguno |
| out | DRONE 3, 6 | salida | -5 a +5 V | ninguno |
| 1v/oct | VCO A, B | entrada | altura | v/oct del teclado |
| fm | VCO A | entrada | FM lineal | ninguno |
| fm | VCO B | entrada | FM lineal | out del VCO A |
| shape | VCO A, B | entrada | ancho de pulso | ninguno |
| out | VCO A | salida | -5 a +5 V | ninguno |
| sync | VCO B | entrada | sync | ninguno |
| gate | ENVELOPE A, B | entrada | gate | gate del teclado |
| env | ENVELOPE A, B | salida | 0 a 8 V | ninguno |
| vca | ENVELOPE A, B | entrada | nivel del VCA | su envolvente |
| cv x, y, z | DUAL EFFECTOR | entrada | -10 a +10 V | ninguno |
| cv L | FILTER L | entrada | corte | ninguno |
| cv R | FILTER R | entrada | corte | cv L |
| out | LFO A, B | salida | 0 a 10 V | ninguno |
| x, y | JOYSTICK | salida | -10 a +10 V | ninguno |
| out | 5 STEP SEQUENCER | salida | pulser | ninguno |
| ext | 5 STEP SEQUENCER | entrada | reloj | out del pulser |
| cv | 5 STEP SEQUENCER | salida | 0 a 5 V | ninguno |
| gate | 5 STEP SEQUENCER | salida | 0 o 10 V | ninguno |
| source | PREAMP | entrada | audio | entrada de audio |
| env | ENVELOPE FOLLOWER | salida | 0 a 10 V | ninguno |
| gate | ENVELOPE FOLLOWER | salida | 0 u 8 V | ninguno |
| clock | teclado | entrada | reloj | ninguno |
| v/oct | teclado | salida | 1 V por octava | ninguno |
| gate | teclado | salida | 0 o 10 V | ninguno |
| gate R | teclado | salida | 0 o 10 V | ninguno |
| press | teclado | salida | 0 a 8 V | ninguno |
| reset | teclado | entrada | reset | ninguno |

En total, 62 jacks: 26 salidas y 36 entradas. Los jacks de las envolventes no tienen nombre impreso: gate, env y vca de izquierda a derecha.

## Página de ajustes

![La página de ajustes.](img/settings.jpg)

Hacé clic en **SETTINGS** para ver todo lo que el hardware no tiene o esconde en su firmware, en cinco cajas dibujadas como el panel. Las perillas funcionan como en el panel. En las listas, un LED rojo marca la opción elegida; hacé clic en otra para cambiarla. Clic derecho sobre cualquier control ofrece MIDI Learn, Clear MIDI CC o Reset to default.

### CHARACTER

| Control | Rango | Qué hace |
|---|---|---|
| AMBIENT | 0 a 10 | luz que llega a los cuatro orbes |
| DRIFT | 0 a 10 | desafinación lenta y al azar de los drones |
| L/R | 0 a 10 | diferencia entre filtro y efector izquierdo y derecho |
| DRIVE | 0 a 10 | saturación del bus de mezcla |
| NOISE | 0 a 10 | soplido del efector |
| TUNING | Analog, Chromatic | osciladores drone en semitonos |
| RELEASE | 8 s, 30 s | release más largo de los drones |

Con Chromatic, cada oscilador suena en el semitono de A 440 Hz más cercano, después de VOLT: los drones quedan afinados con cualquier VOLT hasta la mitad del recorrido, y al girar VOLT se mueven de a semitonos (pasada la mitad, la modulación cruzada los vuelve a doblar). El globo de una perilla TUNE muestra la nota que suena. El orbe sigue doblando los osciladores con MOD.

### SYNC

| Control | Opciones |
|---|---|
| LFO A | Free, Host |
| CYCLE (LFO A) | 1/4, 1/2, 1 bar, 2 bars, 4 bars, 8 bars |
| LFO B | Free, Host |
| CYCLE (LFO B) | 1/16, 1/8, 1/4, 1/2, 1 bar, 2 bars |
| PULSER | Free, Host |
| DIV | 1/4, 1/8, 1/8 T, 1/16, 1/16 T, 1/32 |

### TOUCH KEYBOARD

![La caja TOUCH KEYBOARD de la página de ajustes.](img/settings-keys.jpg)

MODE, SCALE, ROOT, ARP DIRECTION y CLOCK son los mismos ajustes de la pantalla del teclado. **SOURCE** elige el reloj del arpegiador y del secuenciador: Internal (la perilla TEMPO, de 10 a 300 BPM) o Host. Un cable que mande pulsos al jack clock del teclado manda sobre los dos.

### EXPRESSION

Las perillas PORTA, VIBRATO, SPEED, RISE y FALL, y las listas PRESSURE, ARP HOLD y LEGATO, son los mismos ajustes de las páginas de la pantalla con esos nombres.

### MIDI AND SYSTEM

![La caja MIDI AND SYSTEM de la página de ajustes.](img/settings-midi.jpg)

| Control | Opciones | Qué hace |
|---|---|---|
| DRONE KEYS | Off, C1-F1 | las notas MIDI de C1 a F1 tocan las teclas drone |
| BEND | 0 a 12 | rango del pitch bend en semitonos |
| OVERSAMPLING | 1x, 2x, 4x | calidad de voces, filtros y distorsión |
| OUTPUT | -24 a +6 dB | ajuste final de salida |
| PANEL | Cream, Black, Maize, Blue | color del panel |

Estos cinco son del plugin, no del sonido: los presets nunca los cambian. El sobremuestreo (2x por defecto) reduce el aliasing en FM, sync y distorsión; el efector siempre corre a la frecuencia de la sesión. OUTPUT va después de MASTER y antes de un limitador suave que mantiene los picos por debajo del máximo.

## Presets

El menú de presets (clic en el nombre, en la barra de arriba) agrupa los presets por categoría, con los tuyos en User. También ofrece Init, un patch nuevo con los valores por defecto, y Show user presets folder. **<** y **>** recorren toda la lista, que tu DAW también ve como programas.

| Categoría | Presets de fábrica |
|---|---|
| Init | Init |
| Drone | Cathedral of A, Low Choir, Swarm, Fifth Machine, Microtonal Fog, Volt Storm |
| Ambient | Shimmer Garden, Breathing Orbs, Tape Memory, Ocean of Noise, Reverse Hymn |
| Sci-Fi | Siren Tower, Birds at Dusk, Space Monster, Volleys, Random Robot |
| Sequence | Berlin Five, Pulser Ritual, Techno Monster |
| Keys | Flute Lead, Arp Constellation, Binaural VCOs, Pressure Pad, Sequenced Plates |
| Experimental | Piezo Percussion, Joystick Space, Ring Bells, Crushed Choir |

Los acordes de los drones están afinados en notas reales sobre la curva de afinación de los propios osciladores. Muchos presets usan cables: mirá la patchera para ver cómo funcionan. Los presets Keys necesitan las placas o notas MIDI; Sequenced Plates suena mientras mantenés una placa; Piezo Percussion necesita audio en la entrada. La mayoría tiene un volumen parecido.

**SAVE** guarda con un nombre el panel, los cables, la secuencia grabada del teclado y los ajustes de sonido (carácter, sincronía, teclado, expresión); si el nombre ya existe, lo reemplaza. Los ajustes de MIDI AND SYSTEM no se guardan. Los presets de usuario son archivos JSON en `~/Library/Application Support/Audio Tools/AT42/Presets` en macOS, y en `Audio Tools\AT42\Presets` dentro de la carpeta de datos de aplicaciones de tu usuario (AppData) en Windows.

**RANDOM** cambia afinaciones, MUTE y MOD de los drones, VOLT, las voces de ruido, las formas de onda de los VCO, los filtros, cartuchos y programas, X, Y, Z, BLEND y las velocidades de los LFO. Conserva tus cables y el mezclador.

La sesión de tu DAW guarda todo, incluidos los cables, la secuencia y las asignaciones de MIDI Learn.

### En REAPER

REAPER muestra cada preset de fábrica dos veces en el menú de presets: una de la lista propia del plugin y otra como archivo (los .vstpreset que instalan el launcher o la carpeta Presets del zip para Ableton Live, en ~/Library/Audio/Presets/Audio Tools/ en macOS, Documentos\VST3 Presets\Audio Tools\ en Windows y ~/.vst3/presets/Audio Tools/ en Linux). Los dos cargan lo mismo. Si no usás Live, podés borrar ahí la carpeta "Audio Tools AT42" y cada preset aparece una sola vez.

## MIDI y sincronía

### MIDI

- Las notas tocan el teclado táctil, en semitonos y sin la escala. La nota C3 es 0 V en v/oct, transpuesta por OCTAVE. La velocity es la presión en el modo Pressure.
- Con MIDI Drone Keys en C1-F1, las notas de C1 a F1 tocan las teclas drone 1 a 6 en lugar del teclado. Ponelo en Off para liberarlas.
- El pitch bend mueve el v/oct del teclado hasta el rango de BEND (2 semitonos por defecto).
- El pedal de sustain (CC 64) sostiene las notas y las teclas drone que se sueltan con el pedal apretado, hasta levantarlo.
- La rueda de modulación (CC 1) mueve el eje Y del joystick, salvo que el CC 1 esté aprendido en otro control. Al mínimo la palanca queda al centro; al subir la rueda sube, hasta arriba del todo al máximo.
- All Notes Off (CC 123) y All Sound Off (CC 120) sueltan todas las notas y teclas drone.

MIDI Learn funciona en cada control del panel y cada ajuste.

1. Hacé clic derecho sobre un control y elegí MIDI Learn.
2. Mové una perilla o un fader de tu controlador. Todo su recorrido cubre ahora todo el recorrido del control.
3. Para deshacerlo, clic derecho sobre el control y Clear MIDI CC.

El pedal de sustain (CC 64) y los CC 120 en adelante no se pueden aprender. Cada control toma un CC. La página de ajustes muestra en rojo los CC aprendidos. Las asignaciones se guardan con la sesión, no en los presets.

### Sincronía con el host

En SYNC, LFO A, LFO B y el pulser pueden seguir a tu DAW. En Host se ignoran la perilla rate y la llave de velocidad, el ciclo es el valor de CYCLE o DIV (1/4 es un tiempo, 1 bar son cuatro tiempos, 1/8 T son tres pulsos por tiempo) y la fase sigue la posición de la canción. El reloj del teclado sigue al host con SOURCE en Host. Con el transporte parado, las fuentes sincronizadas siguen andando al tempo del host, así podés tocar sin darle play.

## Recetas

### Un acorde drone desde cero

1. Elegí Init en el menú de presets y encendé hold en DRONE 1.
2. Silenciá los osciladores 4 y 5, y afiná 1, 2 y 3 en una tónica, una quinta y una octava, mirando los Hz en el globo.
3. Encendé hold en DRONE 2, silenciá sus osciladores 1 y 2, y afiná 3, 4 y 5 en una tercera, una quinta y una octava más arriba.
4. Paneá DRONE 1 un poco a la izquierda y DRONE 2 un poco a la derecha.
5. Hacé clic en la ranura del cartucho, cargá CATHEDRAL en Both channels con el programa 1 en las dos llaves, y subí BLEND a la mitad y Z para una cola larga de shimmer.
6. Girá despacio el VOLT de DRONE 1 más allá de la mitad para escuchar cómo se tironean los osciladores.

### Orbes que respiran

1. Partí de un drone que suene, por ejemplo Cathedral of A.
2. Encendé MOD en dos o tres osciladores de DRONE 1.
3. Enchufá el out del LFO A en el jack cv de DRONE 1. Poné wave del LFO A todo a la izquierda, rate bajo y la llave de velocidad en /6.
4. Subí la perilla cv de DRONE 1 hasta que el orbe se ponga rojo en la cima de cada ciclo. Los osciladores con MOD suben con la luz y bajan despacio.
5. Pasá el mouse sobre el orbe, o apretá sobre él y arrastrá, para bajar la altura con la mano.

### Sirena y pájaros

1. Elegí Init, subí DRONE 3 en el mezclador y encendé su hold.
2. Con fm encendido, poné rate al medio, mod en dos tercios y divider alto: una sirena lenta.
3. Bajá divider para un trino y después subí rate.
4. Encendé hi y am, subí PITCH y rate: pájaros que pían.
5. Cargá TIME programa 1 en los dos lados y subí BLEND para tener ecos.

### Una secuencia de escuela de Berlín

1. Elegí Init. Subí VCO A en el mezclador y ponelo en saw y 16'.
2. Poné la ENVELOPE A corta: attack y sustain al mínimo, decay y release cortos.
3. Enchufá el cv del secuenciador en el 1v/oct del VCO A, y su gate en el gate de la ENVELOPE A.
4. Poné la llave de etapas en 5 y afiná los pasos en una melodía.
5. Enchufá la salida de la ENVELOPE A en cv L; cv R la sigue, así los dos filtros se abren con cada nota. Poné mod L bajo, freq bajo y res alto.
6. Ajustá la velocidad con pulser, o poné PULSER en Host en la página de ajustes.

### Melodías al azar

1. Elegí Init y subí VCO A. Encendé loop en la ENVELOPE A con attack y decay cortos y sustain en cero, así hace ping sola.
2. Enchufá el out del S&H de DRONE 3 en el 1v/oct del VCO A.
3. Enchufá el out del secuenciador en el clock de DRONE 3. Con cada pulso el S&H muestrea ruido y el VCO A salta a otra altura.
4. Sumá STRING RINGER programa 3 para un toque robótico.

### Grabar y transponer una secuencia

1. Subí VCO A y poné la ENVELOPE A para notas cortas.
2. En la pantalla del teclado, poné SCALE en Pentatonic minor.
3. Andá a SEQ RECORD, girá el encoder un paso, tocá ocho placas y giralo un paso otra vez.
4. Poné MODE en Sequencer y SEQ LENGTH en 8.
5. Mantené una placa para que la secuencia suene desde ahí; mantené otra para transponerla. Para un arpegio a tempo con tu DAW, poné MODE en Arpeggiator y SOURCE en Host.

### Drones disparados por audio

1. Elegí Init y mandá un loop de batería a la entrada de audio del AT42.
2. Subí gain del PREAMP hasta que el LED verde del seguidor parpadee con los golpes.
3. Enchufá el gate del seguidor en el gate de DRONE 1. Poné att corto y rls largo.
4. Enchufá el env del seguidor en cv L y subí mod L para un filtro que bombea.

### Tocar con el joystick

1. Cargá Cathedral of A.
2. Enchufá la salida x del joystick en el cv x del efector, y la y en cv L.
3. Poné mod L en un valor moderado y bajá un poco freq.
4. Mové la palanca mientras suena el drone. Queda donde la dejás, y la rueda de modulación también mueve Y.

## Consejos y problemas frecuentes

- **Los drones de Init no suenan.** Es a propósito: sus envolventes esperan una tecla. Apretá cualquier tecla drone, tocá de C1 a F1, o encendé el hold de una voz. Las demás notas MIDI tocan el VCO A.
- **Las placas o las notas MIDI no suenan.** El teclado sólo manda tensiones. Subí VCO A o B en el mezclador y fijate que ningún cable haya tomado el gate de la envolvente.
- **Las notas MIDI de C1 a F1 no tocan el teclado.** Tocan las teclas drone; poné MIDI Drone Keys en Off.
- **La altura del drone se mueve con el mouse.** El puntero sobre un orbe le hace sombra, y los osciladores con MOD siguen la luz. Alejate del orbe o apagá MOD. Si el host lo grabó, mirá los carriles de automatización Drone N Hand.
- **Los drones no se quedan afinados.** Bajá Drone Drift, apagá MOD, o usá Chromatic.
- **El arpegiador o el secuenciador no avanzan.** Revisá MODE, SOURCE y cualquier cable en el jack clock del teclado. El secuenciador necesita una secuencia grabada y una placa apretada (o ARP HOLD).
- **EXT IN, PREAMP o Piezo Percussion no suenan.** Necesitan audio en la entrada del plugin: rutealo en tu DAW, o elegí un dispositivo de entrada en la aplicación independiente y permití el micrófono en macOS.
- **Suena demasiado fuerte o distorsionado.** Bajá MASTER, dist y gain, Mixer Drive u OUTPUT. El limitador suave agarra los picos, pero no está pensado para trabajar todo el tiempo.
- **Hay un soplido.** Poné Effector Noise en cero.
- **La CPU está alta.** Oversampling en 1x reduce la carga más o menos a la mitad; dejá 4x para los renders finales.
- **Un LFO sincronizado ignora su perilla rate.** Es lo esperado mientras su sincronía está en Host.
- **Silencio en bypass.** El AT42 es un instrumento, así que el bypass lo calla y suelta las notas apretadas.

## Especificaciones

| Ítem | Valor |
|---|---|
| Formatos | VST3, AU y aplicación independiente en macOS; VST3 y aplicación independiente en Windows |
| Sistemas | macOS 11 o posterior, Apple silicon e Intel; Windows 10 u 11 de 64 bits |
| Tipo | instrumento, entrada MIDI, entrada de audio estéreo opcional |
| Salida | estéreo, o mono sumado |
| Voces | 4 voces drone de 5 osciladores, 2 voces de ruido, 2 VCO con ADSR |
| Mezclador | 10 canales, volumen y paneo |
| Filtros | 2 de 12 dB estilo Polivoks, pasa bajos o pasa banda, con distorsión |
| Efector | 2 motores, 13 cartuchos de 3 programas |
| Patchera | 62 jacks: 26 salidas, 36 entradas |
| Parámetros | 194 del panel y 35 ajustes, todos automatizables |
| Presets | 29 de fábrica más los de usuario |
| Sobremuestreo | 1x, 2x o 4x |
| Nivel de una voz drone | unos -5,2 dBFS de pico, una voz con 5 osciladores en la afinación por defecto |
| Nivel de los presets | unos -17 dBFS RMS |
| CPU | 10 a 20 por ciento de un núcleo en 2x, Apple silicon, aplicación independiente; 1x lo reduce a la mitad |
| Validación | pluginval con exigencia 8, auval en macOS |
| Licencia | GNU Affero General Public License v3 |
