# Manual del AT330

## Qué es el AT330

El AT330 es un instrumento de cuerdas, coro y vocoder recreado a partir de un teclado de 1979. Tiene tres secciones que se mezclan a gusto: una **sección de cuerdas** (una onda de sierra a 4'), un **coro** con voces masculinas a 8' y 4' y una voz femenina a 4', y un **vocoder de 10 bandas** que hace hablar al teclado con el sonido de tu voz. Todo pasa por un vibrato y por un **ensemble** hecho con cuatro líneas de retardo BBD (de cubo), que es lo que le da a este tipo de instrumento su sonido ancho y de remolino lento. Un **pitch shift** baja la afinación de cada nota y la devuelve.

Lo que viene del original:

- Un solo oscilador maestro con divisores: todas las notas son una sierra derivada del mismo contador, así que todas las octavas quedan en fase.
- Las cuerdas, con un filtro de tono fijo y un control TONE.
- El coro: un banco fijo de siete filtros formantes (200, 230, 600, 900, 1300, 2800 y 3300 Hz) que le da a cada voz su vocal.
- El vocoder: diez bandas de análisis y diez de síntesis de 200 Hz a 6 kHz, el paso de consonantes, el micrófono directo y Hold.
- Los dos vibratos BBD (coro y vocoder) y el ensemble de cuatro BBD.
- El pitch shift, que solo baja.
- La división del teclado en el do central, el release común y la envolvente que reinicia todo el acorde con cada tecla nueva.

Lo que es extra, y vive en la página Extended o en el host:

- Envolventes **Poly** (por defecto), velocity, una división ajustable, pitch bend hacia los dos lados y vibrato con la rueda de modulación.
- **Freeze** para el vocoder, **Formant Shift**, **Vocoder Response** y un **portador externo** por el canal derecho de la entrada.
- Las cuerdas pueden salir del ensemble, y la profundidad del ensemble va de 0 a 150 %.
- **Space**, una reverb de placa.

El AT330 funciona como VST3 en macOS, Windows y Linux, como Audio Unit y LV2 donde la plataforma los tiene, y como app independiente. Es un instrumento: se toca desde una pista MIDI. Su entrada de audio es la voz del vocoder y llega por sidechain. Es software libre y de código abierto bajo la GNU AGPL v3.

## Instalación

Cada zip trae un `INSTALL.txt` con estos pasos.

### macOS

1. Descomprimí el archivo.
2. Copiá `AT330.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `AT330.component` a `~/Library/Audio/Plug-Ins/Components/`.
3. Volvé a escanear los plugins en tu DAW. En Ableton Live: Settings, Plug-Ins, Rescan.

`AT330.app` es la versión independiente.

> Los binarios están firmados ad hoc y no notarizados. Si macOS se niega a abrirlos, corré esto en Terminal: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/AT330.vst3 ~/Library/Audio/Plug-Ins/Components/AT330.component`

### Windows

1. Descomprimí el archivo.
2. Copiá la carpeta `AT330.vst3` a `C:\Program Files\Common Files\VST3\`. Windows te pide permisos de administrador.
3. Volvé a escanear los plugins en tu DAW.

`AT330.exe` es la versión independiente y funciona sin DAW.

> Los binarios no están firmados. Windows SmartScreen puede pedirte que confirmes la primera vez.

### Linux

Copiá `AT330.vst3` a `~/.vst3/` y el bundle LV2 a `~/.lv2/`, y volvé a escanear en tu host.

## Cargar el AT330 en un DAW

El AT330 aparece con el nombre de fabricante **Audio Tools**, entre los instrumentos, como Audio Tools AT330. Cargalo en una pista MIDI y tocá: abre con el primer preset de fábrica, Ensemble Strings.

La app independiente abre el editor en su propia ventana; elegí tus dispositivos de audio y MIDI en las opciones. Las notas también se tocan con el mouse en el teclado de la pantalla.

## Primer sonido

1. Cargá el AT330 en una pista MIDI y tocá un acorde: cuerdas a través del ensemble.
2. Hacé clic en las llaves **FEMALE 4' UPPER** y **MALE 8' LOWER** para sumar voces, y tocá de nuevo.
3. Subí **RELEASE** para oír cómo el acorde se apaga despacio.
4. Apretá el nombre del preset en la barra de arriba y elegí **Strings and Choir** de la categoría Layers: es el sonido por el que se conoce a este instrumento.

## Recorrido por la interfaz

![El AT330 con Strings and Choir y un acorde sostenido: las cinco secciones arriba, la pantalla de bandas del vocoder y el teclado con la división.](img/overview.jpg)

### Barra superior

- **<** y **>** recorren todos los presets y dan la vuelta en los extremos.
- El botón del preset muestra el nombre del preset actual. Hacé clic para abrir el menú, con un submenú por categoría.
- **PANEL**, **EXTENDED** y **MANUAL** cambian de página. **MANUAL** abre este manual dentro del plugin, en inglés o en español; apretá Esc o la cruz para cerrarlo.
- **VOICE IN** es el medidor de nivel de la entrada de audio. Se pone rojo cerca de saturar.
- A la derecha, **SPLIT** muestra la nota de división y **CLASSIC** o **POLY** el modo de envolventes.

### Panel

De izquierda a derecha: **PITCH SHIFT**, **VIBRATO**, **STRINGS**, **CHOIR**, **VOCODER** y **MASTER**, y después el teclado. El cuadradito de color al lado de cada título es del color de sus llaves.

### Trabajar con los controles

- **Perillas**: arrastrá hacia arriba o hacia abajo. El valor aparece mientras arrastrás. Un doble clic devuelve la perilla a su valor por defecto.
- **Llaves**: hacé clic para prenderlas o apagarlas. Una llave encendida está activa.
- Pasá el mouse sobre una perilla para ver su nombre completo.
- La línea de abajo muestra el estado de los ajustes de Extended (Envelopes, Hold, Carrier, Formant shift, Ensemble depth, Space y Velocity).
- Arrastrá la esquina inferior derecha para cambiar el tamaño de la ventana, de la mitad a una vez y media.

### Llaves y teclado

Cada llave tiene dos renglones: qué es (UPPER, MALE, FEMALE, ENSEMBLE, HOLD) y su pie y mitad del teclado (por ejemplo 8' LOWER). El teclado se divide en dos: las notas desde la nota de división hacia arriba son la mitad **UPPER** y las de abajo la mitad **LOWER**. Una línea con las palabras LOWER y UPPER marca la división en el teclado; por defecto está en C4 (do central, nota MIDI 60) y se mueve en la página Extended.

El pie es la octava: 8' es la altura de la tecla y 4' suena una octava más arriba. Una llave que dice UPPER solo suena para las teclas de la mitad de arriba; una que dice LOWER solo para las de abajo. Así podés tocar un coro con la derecha sobre cuerdas con la izquierda.

El teclado de la pantalla tiene 49 teclas, de C2 a C6. Hacé clic en una tecla para tocarla. Las notas MIDI fuera de ese rango también suenan.

## Strings

![Las secciones STRINGS y CHOIR: las llaves dicen qué son en el primer renglón y el pie y la mitad del teclado en el segundo; abajo ATTACK, TONE y LEVEL.](img/strings-choir.jpg)

Una sierra a 4' por un filtro de tono fijo, con su propia envolvente.

- **UPPER 4'** y **LOWER 4'**: cuerdas en la mitad de arriba y en la de abajo del teclado. Las dos vienen prendidas.
- **ENSEMBLE CHORUS**: manda las cuerdas por el ensemble. En el original las cuerdas siempre pasan por él; acá podés apagarlo (extra) y oír la sierra pelada.
- **ATTACK**: de 5 ms a 4 s. Cuánto tardan las cuerdas en llegar al nivel pleno.
- **TONE**: de 0 a 100 %. Cierra o abre el brillo de las cuerdas.
- **LEVEL**: de 0 a 100 %.

## Choir

El coro es la misma sierra por un banco de siete filtros formantes fijos. Cada voz usa un conjunto distinto, y por eso las masculinas suenan más graves y redondas y la femenina más fina y abierta. Las voces no cambian de vocal: es la vocal fija de la máquina, y lo que la hace hablar es el vocoder.

- **FEMALE 4' UPPER**: la voz femenina, una octava arriba de la tecla, en la mitad de arriba.
- **MALE 8' UPPER**: la voz masculina a la altura de la tecla, en la mitad de arriba.
- **MALE 4' LOWER**: la voz masculina una octava más arriba, en la mitad de abajo.
- **MALE 8' LOWER**: la voz masculina a la altura de la tecla, en la mitad de abajo.
- **ENSEMBLE CHORUS**: manda el coro por el ensemble.
- **ATTACK**: de 5 ms a 1,5 s.
- **LEVEL**: de 0 a 100 %.

El coro pasa primero por el vibrato. Las cuatro llaves de voces vienen apagadas.

## Vocoder

![La sección VOCODER con Vocoder Ensemble: las dos llaves de 8', ENSEMBLE, HOLD, la pantalla de bandas sobre las diez bandas, el LED de nivel de la voz y los controles abajo.](img/vocoder.jpg)

El vocoder mide cuánta energía tiene tu voz en cada una de diez bandas de frecuencia (200, 280, 400, 600, 900, 1300, 2000, 2800, 4000 y 6000 Hz) y con eso abre diez bandas iguales sobre un portador. El portador es una sierra a 8' tocada desde el teclado: vos tocás las notas y la voz las convierte en palabras.

- **UPPER 8'** y **LOWER 8'**: el portador del teclado en cada mitad. Sin ninguna llave prendida no hay portador: el vocoder queda mudo, salvo que el portador venga de afuera.
- **ENSEMBLE CHORUS**: manda el vocoder por el ensemble.
- **HOLD VOWELS**: mantiene cada banda en su nivel unos 2,5 segundos después de que la voz se corta, así la vocal queda sonando mientras tocás. Un clic más lo apaga.
- **BANDS**: diez columnas de luces, una por banda, que muestran la voz. Sin voz quedan apagadas, y es lo primero que hay que mirar cuando el vocoder no suena.
- **MIC GAIN**: de -24 a +24 dB. La ganancia de la voz. El LED rojo debajo de la pantalla se enciende cuando la voz está cerca de saturar.
- **LEVEL**: de 0 a 100 %. El nivel del vocoder.
- **TONE**: de 0 a 100 %. Un pasabajos en la salida del vocoder.
- **CONSONANTS**: de 0 a 100 %. Mezcla directo las frecuencias altas de la voz (la s, la t y la f), pero solo mientras hay una tecla apretada. Sin eso un vocoder suena tapado.
- **DIRECT MIC**: de 0 a 100 %. Mezcla la voz seca.

### La voz entra por la entrada de audio

La entrada del plugin se llama Voice In (Vocoder) y es estéreo o mono. En un DAW es un sidechain: le mandás la pista con la voz (mirá las secciones de cada DAW más abajo). El medidor VOICE IN de la barra superior muestra lo que llega. Si los dos canales son distintos, se mezclan en una sola voz.

### El portador

En la página Extended, **Carrier** elige con qué habla el vocoder:

- **Keyboard**: la sierra de las teclas (las llaves del vocoder). Es el valor por defecto.
- **External (input R)**: otro sonido. La **voz va por el canal izquierdo y el portador por el canal derecho** de la entrada. El teclado no participa. Un sintetizador, una guitarra o un pad sirven de portador.
- **Keyboard + External**: los dos a la vez.

**External Level** (de -24 a +24 dB) fija el nivel del portador externo. El portador externo necesita una entrada estéreo.

### Hold y Freeze

**Hold** (la llave del panel, o **Vocoder Hold** en la página Extended) mantiene cada banda unos 2,5 s. **Freeze** (solo en la página Extended) guarda, para cada banda, el nivel más alto que oyó hasta que lo apagues: decí una vocal, prendé Freeze y tocá acordes encima todo el tiempo que quieras.

### Dar forma a la voz

- **Vocoder Response**: de 3 a 300 ms. Qué tan rápido siguen los detectores de banda a la voz. Corto es nítido y robótico; largo empasta las palabras en una voz fantasmal.
- **Formant Shift**: de -12 a +12 semitonos. Mueve las diez bandas del portador hacia arriba o hacia abajo: la voz suena más grande o más chica sobre las mismas notas.

> Los presets cuyo nombre termina en "(Audio In)" necesitan una voz en la entrada. Sin sonido ahí quedan mudos. Mirá el medidor VOICE IN y la pantalla BANDS.

## Vibrato

El vibrato es una modulación lenta de una línea de retardo BBD corta, no un simple temblor de afinación. Actúa sobre el **coro y el vocoder**; las cuerdas no pasan por él. El LEDcito se enciende al ritmo del vibrato.

- **RATE**: de 4,5 a 11 Hz.
- **DEPTH**: de 0 a 100 %. La rueda de modulación suma hasta 150 % en total.
- **DELAY**: de 0 a 3 s. Cuánto tarda el vibrato en aparecer después de la primera nota, como haría un cantante. Tocar ligado lo mantiene andando.

## Pitch shift

![PITCH SHIFT con Rising Choir (modo AUTO) y VIBRATO al lado.](img/pitch-vibrato.jpg)

El pitch shift baja todas las secciones, nunca las sube (para eso usá el pitch bend). El interruptor de tres posiciones **MODE** elige cómo funciona:

- **OFF**: nada.
- **AUTO**: cada tecla nueva arranca el sonido **DEPTH** semitonos más abajo y lo devuelve a su altura en el tiempo de **GLIDE TIME**. Una barrida hacia cada acorde.
- **MAN**: **BEND DOWN** baja la afinación a mano, hasta **DEPTH** semitonos. Se puede automatizar.

Los controles:

- **DEPTH**: de 0 a 15 semitonos, un poco más de una octava.
- **GLIDE TIME**: de 10 ms a 2 s. Cuánto tarda la vuelta en AUTO.
- **BEND DOWN**: de 0 a 100 %. Cuánto de DEPTH se aplica en MAN.

## Master

![La sección MASTER: TUNE y VOLUME arriba, el RELEASE común abajo.](img/master.jpg)

- **TUNE**: de -50 a +50 cents. En 0, el A4 es 440 Hz.
- **VOLUME**: de -36 a +6 dB.
- **RELEASE**: de 0,02 a 8 s. Cuánto tarda cada nota en apagarse después de soltarla (el tiempo hasta -40 dB). Es común a las cuerdas, el coro y el vocoder.

## Envolventes

En la página Extended, **Envelopes** elige cómo las teclas disparan los sonidos:

- **Poly**: cada tecla tiene su propia envolvente. Las notas que sumás a un acorde sostenido no lo molestan, y cada una tiene su ataque. Es el valor por defecto.
- **Classic**: el comportamiento del original. Hay una envolvente por sección para todas las teclas, y cada tecla nueva hunde todo el sonido durante 50 ms y ataca de nuevo, así que el acorde sostenido se corta y vuelve a atacar. La primera nota también espera esos 50 ms. Es la manera en que toca la máquina, y sirve para acordes cortos y para el carácter vintage; los presets Vintage Strings y Classic Retrigger la usan.

**Velocity** (de 0 a 100 %) hace que el nivel de cada tecla dependa de cuánto la golpeás. Solo funciona en modo Poly, y en 0 todas las notas tienen el mismo nivel.

## Ensemble

El ensemble son cuatro líneas BBD cuyo retardo cambia con dos moduladores lentos cada una (un triángulo de varios segundos y una senoide rápida), y cuyas salidas van a la izquierda y a la derecha en sentidos opuestos. El resultado es ese coro ancho y en movimiento. La llave ENSEMBLE de cada sección decide qué sonidos pasan por él; lo que no pasa va directo a la salida, seco. Su profundidad es **Ensemble Depth**, en la página Extended.

## Página Extended

![La página Extended: envolventes y teclas, los extras del vocoder, y el ensemble y el espacio, cada control con una línea de ayuda.](img/extended.jpg)

Lo que el original no tenía. Todo lo de acá es automatizable y se guarda con el preset.

### Envolventes y teclas

- **Envelopes**: Classic o Poly, como arriba.
- **Velocity**: de 0 a 100 %.
- **Split**: dónde empieza la mitad de arriba, de C2 a C6. Por defecto C4.
- **Bend Range**: de 0 a 12 semitonos, hacia arriba y hacia abajo. Por defecto 2.
- **Mod Wheel Vibrato**: de 0 a 100 %. Cuánto vibrato suma la rueda de modulación al coro y al vocoder.

### Vocoder

- **Vocoder Hold**: Off, Hold o Freeze.
- **Vocoder Response**: de 3 a 300 ms.
- **Formant Shift**: de -12 a +12 semitonos.
- **Carrier**: Keyboard, External (input R) o Keyboard + External.
- **External Level**: de -24 a +24 dB.

### Ensemble y espacio

- **Strings Ensemble**: prende o apaga el ensemble en las cuerdas.
- **Ensemble Depth**: de 0 a 150 %. En 100 % es el del original; en 0 % las líneas dejan de moverse; por encima de 100 % se vuelve más ancho y más mareante.
- **Space**: de 0 a 100 %. La cantidad de una reverb de placa al final.
- **Space Size**: de 0 a 100 %. El tamaño de la reverb.

## Teclado y división

![El teclado con un acorde sostenido a los dos lados de la división (LOWER y UPPER, acá en C4) y la línea de estado abajo.](img/keyboard.jpg)

Las teclas que tocás, con el mouse o por MIDI, se iluminan. Probá esto: en STRINGS apagá UPPER 4' y dejá LOWER 4', en CHOIR prendé FEMALE 4' UPPER y MALE 8' UPPER, y tocá con las dos manos. La mano izquierda son las cuerdas y la derecha el coro, y la división se puede mover hasta donde se encuentran tus manos.

## Presets

El AT330 trae 45 presets de fábrica en cinco categorías. Hacé clic en el nombre del preset de la barra superior para abrir el menú, o recorrelos con **<** y **>**.

| Categoría | Presets | Qué hay |
|---|---|---|
| Strings | 9 | La sección de cuerdas sola, de Ensemble Strings a Upper Strings Only |
| Choir | 8 | Voces masculinas y femeninas, de Male Choir 8' a Dry Choir |
| Layers | 8 | Cuerdas con el coro, de Strings and Choir a Velvet Poly |
| Vocoder | 12 | Las diez bandas, de Vocoder Ensemble a External Carrier |
| Pitch and Motion | 8 | Pitch shift, vibrato profundo y los dos modos de envolvente |

Los presets que dependen de la voz terminan en "(Audio In)" y quedan mudos sin una voz en la entrada. Los presets fijan todos los controles, así que después de cargar uno podés cambiar lo que quieras y el host guarda los cambios en el proyecto. El editor no tiene botón Save: para guardar tus propios sonidos usá el guardado de presets de tu host.

## MIDI

| Mensaje | Efecto |
|---|---|
| Notas | Tocan, en cualquier canal. La velocity cuenta en modo Poly con Velocity mayor que 0 |
| CC 64, pedal de sustain | Sostiene las notas mientras el pedal está apretado |
| CC 1, rueda de modulación | Suma vibrato al coro y al vocoder (Mod Wheel Vibrato) |
| Pitch bend | Curva todos los sonidos hacia arriba y hacia abajo según Bend Range |
| CC 120, CC 123 | All sound off, all notes off |

El AT330 no tiene salida MIDI.

## En Ableton Live

El AT330 está en Browser, Plug-Ins, Audio Tools. Si falta, revisá en Settings, Plug-Ins que las carpetas de sistema de VST3 (y, en Mac, de Audio Units) estén activadas y apretá Rescan.

Arrastralo a una pista MIDI, armá la pista y tocá.

Para usar el vocoder, la voz tiene que llegar a la entrada del plugin:

1. Poné la voz en una pista de audio (una grabación, o una entrada con un micrófono) y dejala.
2. En la pista del AT330, abrí la sección **Sidechain** de la barra de título del dispositivo y elegí la pista de audio de la voz.
3. Cargá un preset que termine en "(Audio In)", por ejemplo Vocoder Ensemble, y tocá acordes mientras suena la voz.

El sidechain alimenta el medidor VOICE IN y la pantalla BANDS. Para oír también la voz seca, dejá la pista de la voz audible o subí **DIRECT MIC**.

> Para un portador externo, el sidechain tiene que llevar la voz por su canal izquierdo y el portador por el derecho, por ejemplo una pista de grupo con la voz paneada a la izquierda de todo y el portador a la derecha de todo. Después elegí el portador External (input R).

Los presets de fábrica aparecen en el navegador de Live, en Plug-Ins, Audio Tools AT330: expandí el plugin. Los instala el launcher o la carpeta Presets del zip. Live no muestra la lista de programas del propio plugin; el menú de presets del editor siempre los tiene todos.

El AT330 tiene 44 parámetros automatizables, menos que los 64 que Live pone solo en el panel del dispositivo, así que todos se ofrecen para automatizar.

## En REAPER

REAPER muestra cada preset de fábrica dos veces en su menú de presets: una desde la lista del propio plugin (los programas) y otra como archivo de preset (los `.vstpreset` que instalan el launcher o la carpeta Presets del zip, en `~/Library/Audio/Presets/Audio Tools/` en macOS, `Documents\VST3 Presets\Audio Tools\` en Windows y `~/.vst3/presets/Audio Tools/` en Linux). Los dos cargan los mismos ajustes. Si no usás Live, podés borrar ahí la carpeta "Audio Tools AT330" y cada preset aparece una sola vez.

Para alimentar el vocoder, mandá una pista con la voz a la entrada del plugin por los canales 3 y 4:

1. Poné la pista del AT330 en 4 canales.
2. En la pista de la voz agregá un envío hacia la pista del AT330 y fijá sus canales de destino en 3/4.
3. En la ventana del AT330 abrí el botón de ruteo (In/Out pins) y conectá los canales de entrada 3 y 4 a las dos entradas del plugin.
4. Cargá un preset que termine en "(Audio In)" y tocá.

Para un portador externo, mandá la voz solo al canal 3 y el portador solo al canal 4, y conectalos en ese orden: la entrada izquierda es la voz, la derecha el portador.

## CPU y latencia

El AT330 no agrega latencia: la salida sigue a las notas en el mismo instante. Medido en un Apple M4 a 48 kHz:

| Caso | CPU, un núcleo |
|---|---|
| Cuerdas, un acorde de ocho notas | 1,3 % |
| Todo prendido, 16 notas, con el vocoder y Space | alrededor de 2 % |
| Las 49 teclas sostenidas con todo prendido | alrededor de 4,3 % |

## Consejos y solución de problemas

- **El vocoder no suena.** Mirá el medidor VOICE IN de la barra superior: si no se mueve, la voz no está llegando al plugin (revisá el sidechain o el ruteo). Revisá que haya una llave del vocoder (UPPER 8' o LOWER 8') prendida para la mitad del teclado que tocás, o que el portador sea External.
- **La voz suena opaca.** Subí CONSONANTS, abrí TONE y acortá Vocoder Response.
- **La voz suena demasiado espesa.** Apagá ENSEMBLE en el vocoder o bajá Ensemble Depth.
- **Un acorde sostenido se corta al sumar una nota.** Estás en Classic. Elegí Poly en la página Extended.
- **Notas que no suenan en la sección.** Revisá que la llave corresponda a la mitad del teclado: una llave UPPER no suena debajo de la división.
- **El pitch shift parece no hacer nada.** MODE está en OFF, o en MAN BEND DOWN está en 0.
- **No suena nada.** Revisá que haya al menos una llave prendida para la mitad del teclado que tocás, y que VOLUME y el nivel de la sección no estén al mínimo.
