# Manual del ATFS1

## Qué es el ATFS1

El ATFS1 es un sintetizador de formantes y FM basado en el Yamaha FS1R, un generador de sonido de rack de una unidad de 1998. Cada voz tiene ocho operadores con voz y ocho sin voz. Un operador con voz pone una portadora dentro de una ventana corta que vuelve a empezar en cada período de la nota: según la ventana y la portadora, da una sola parcial, un grupo de armónicos, sólo los impares, una resonancia o un formante de voz, y los operadores se pueden modular en frecuencia entre sí como en un DX7. Cada operador sin voz convierte ruido blanco en una banda alrededor de un formante: el aliento, el aire y las consonantes.

Encima está lo que hizo famoso al FS1R, el Formant Sequence (Fseq): cómo se mueven los formantes de una voz, 126 veces por segundo, para que el sintetizador diga una palabra o cante una frase en la altura que tocás. El ATFS1 además crea secuencias nuevas desde una grabación o desde vocales que escribís, algo que el equipo no podía hacer.

Arriba tiene un frente de rack de 1U que funciona y abajo un editor de pantalla completa, con una línea de ayuda que dice qué hace cada control.

El ATFS1 es una recreación independiente, hecha a partir de los manuales publicados, de una patente vencida de Yamaha y de mediciones públicas de equipos reales, sin relación ni aval de Yamaha. No incluye voces, secuencias ni ningún otro dato de Yamaha.

## Instalación

El ATFS1 viene como plugin VST3, Audio Unit (sólo macOS) y aplicación independiente.

### macOS

macOS 11 o posterior, Apple silicon e Intel.

1. Descomprimí el archivo.
2. Copiá `ATFS1.vst3` a `~/Library/Audio/Plug-Ins/VST3/`.
3. Copiá `ATFS1.component` a `~/Library/Audio/Plug-Ins/Components/`.
4. Reescaneá los plugins en tu DAW (en Ableton Live: Settings, Plug-Ins, Rescan).

`ATFS1.app` es la versión independiente.

> Los binarios tienen firma ad hoc y no están notarizados. Si macOS se niega a abrirlos, corré esto en la Terminal: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/ATFS1*.vst3 ~/Library/Audio/Plug-Ins/Components/ATFS1*.component`

### Windows

Windows 10 y 11 de 64 bits.

1. Descomprimí el archivo.
2. Copiá la carpeta `ATFS1.vst3` a `C:\Program Files\Common Files\VST3\` (pide permisos de administrador).
3. Reescaneá los plugins en tu DAW.

`ATFS1.exe` es la versión independiente y funciona sin DAW. Los binarios no están firmados, así que SmartScreen puede pedirte confirmación la primera vez.

## Primeros pasos

### Cargarlo en un DAW

El ATFS1 es un instrumento con salida estéreo (también funciona en una salida mono). Insertalo en una pista de instrumento o MIDI y tocá: responde a notas en todos los canales MIDI. Una instancia es una parte; para cuatro partes como en el FS1R, cargá cuatro instancias.

El DAW guarda todo el estado con el proyecto, y los presets aparecen en su lista de programas.

En Ableton Live, los presets de fábrica aparecen en el navegador de Live en Plug-Ins → Audio Tools ATFS1 (desplegá el plugin), instalados por el launcher o desde la carpeta Presets del zip. Live no muestra la lista de programas propia del plugin; la lista de presets del editor los tiene todos.

En la versión independiente, elegí la interfaz de audio y la entrada MIDI en sus Options. Sin teclado MIDI, tocá el teclado del pie del editor.

### Tu primer sonido

1. Abrí el editor. Arranca en la página **Play** con el primer preset, Talking Formants.
2. Hacé clic y mantené una tecla del teclado de abajo, cerca de C2 o C3. El ATFS1 dice "Audio Tools" en esa altura.
3. Hacé clic en otro preset de la lista de la izquierda, o usá los botones **<** y **>** que están debajo.
4. Girá la perilla grande **FORMANT** del frente mientras mantenés una nota, y después hacé doble clic para volverla al centro.

![El editor del ATFS1: el frente, las pestañas, la página Play, la línea de estado y el teclado.](img/overview.jpg)

## Recorrida por el editor

De arriba hacia abajo:

- El **frente**: el rack de 1U con el LCD, los botones de página y de datos y cinco perillas.
- Las **pestañas**: Play, Voice, Operator, Common, Fseq, Perform, Effects y Utility.
- El botón **Manual**, a la derecha de las pestañas, abre este manual dentro del plugin, en inglés o en castellano. Esc o la cruz lo cierran.
- La **página**.
- La **línea de estado**: a la izquierda, la ayuda de lo que está bajo el mouse, o un mensaje en verde después de una acción (una importación, un archivo guardado); a la derecha, las voces que suenan y el CPU que usa el ATFS1.
- El **teclado**: de C1 a C6. Hacé clic más abajo en una tecla para una nota más fuerte. Las notas que llegan por MIDI también se iluminan.

La ventana se agranda desde la esquina, de la mitad a 140 %.

![El teclado y la línea de estado con las voces y el medidor de CPU.](img/keyboard.jpg)

### Cómo se usan los controles

- **Perillas**: arrastrá hacia arriba o hacia abajo; con Shift, pasos finos; doble clic, o clic derecho y Reset to default, para volver al valor de fábrica. La rueda del mouse mueve un paso.
- **Cajas** con un nombre arriba: la de encendido se alterna con un clic, la que tiene un triangulito abre un menú, la que tiene un número cambia al arrastrarla.
- **Campos** de la voz (cajitas oscuras con el valor en verde): arrastrá hacia arriba o hacia abajo, con Shift para pasos finos, con la rueda paso a paso. Doble clic para escribir un valor y Return; clic derecho para Reset. Los campos con valores con nombre (una forma, un modo, una onda) abren un menú.

Un campo que muestra `--` no lo usa la forma o el modo actual (Transpose, por ejemplo, sólo funciona en frmt). Cada control escribe su nombre y su valor en el LCD mientras lo movés, y la línea de ayuda lo explica.

### La voz y la parte

El ATFS1 mantiene la división del FS1R entre voz y parte.

La **voz** es el sonido: el algoritmo, los 16 operadores, las envolventes, los LFO, el filtro y las asignaciones de las perillas Formant y FM. Se edita en las páginas Voice, Operator y Common. No son parámetros del DAW (no se automatizan), pero se guardan con el proyecto y los presets, y son lo que contiene un sysex de voz del FS1R.

La **parte** es cómo se toca la voz: volumen, paneo, los desvíos que mueven las perillas del frente, mono o poly, portamento, controladores, la reproducción del Formant Sequence y los efectos. Los ajustes de la parte son parámetros del DAW que podés automatizar.

## El frente

El frente funciona como el original.

![El frente del ATFS1.](img/front-panel.jpg)

### Perillas

- **VOLUME** (al lado de PHONES): el volumen de la parte.
- **ATTACK** y **RELEASE**: desvíos que se suman a los tiempos de ataque y de release de todos los operadores. Hacia arriba es más lento.
- **FORMANT** y **FM**: las dos perillas de interpretación de la voz. Cada voz las asigna al nivel, la frecuencia o el ancho de hasta cinco operadores (ver Perillas Formant y FM). En las voces de formantes, Formant suele mover los formantes; en las de FM, FM suele cambiar el brillo.

Con doble clic vuelven al centro.

### El LCD

- El renglón 1 muestra el número y el nombre del preset, `P001 Talking Formants`. Un `*` quiere decir que editaste la voz desde que la cargaste.
- El renglón 2 depende de la página. En Play y Voice: algoritmo, realimentación, el nombre de la secuencia si hay una encendida, y las voces que suenan (`ALG01 FB0 AUDIO TO  2V`). En Operator: el operador, su forma y su relación o frecuencia, o `NOISE` y su nivel. En Fseq: el nombre de la secuencia y su cantidad de cuadros.

Las flechitas bajo el cristal señalan los rótulos impresos: **MIDI** se enciende mientras llega MIDI, **FLT** con el filtro encendido, **REV**, **VAR** e **INS** con cada efecto activo, **KEY** en mono, **VOL** y **PAN** con el volumen arriba y el paneo fuera del centro. **PART** y **BANK/PGM#** están siempre encendidos.

![El LCD y la botonera.](img/front-controls.jpg)

### Botones

- **PLAY**, **UTIL** y **SEARCH**: las páginas Play, Utility y Fseq.
- **EDIT PERFORM** y **EDIT EFFECT**: las páginas Perform y Effects.
- **EDIT VOICE**: la página Voice; apretalo otra vez para Common, y otra para Operator.
- **MUTE/SOLO**: alterna entre todos los operadores, sólo los con voz y sólo los sin voz; el renglón 2 termina en `V` o `N` mientras hay un solo. Es la forma más rápida de escuchar qué agrega el ruido.
- **PART - y +**: preset anterior o siguiente; en la página Operator, operador anterior o siguiente.
- **CURSOR - y +**: mueven un cursor verde por los campos de las páginas Voice, Operator y Common.
- **VALUE - y +**: cambian un paso el campo bajo el cursor.
- **ENTER**: muestra en el LCD el campo bajo el cursor.
- **EXIT**: vuelve a Play.

Un punto verde al lado de un botón de página indica la página abierta.

### Modo de las perillas y encendido

El botoncito entre los dos LED, a la izquierda de las perillas grandes, las pasa a entrada de datos: se enciende el LED de abajo y el LCD muestra `KNOBS` y `DATA`. Las perillas siguen entonces los rótulos azules que tienen debajo: **PART/OP** elige el operador y abre la página Operator, **GROUP** recorre las páginas, **CURSOR** mueve el cursor y **VALUE** cambia el campo. Volvé a hacer clic en el botón para devolverles sus funciones de interpretación.

La llave **POWER** silencia la salida y reinicia todas las voces; el LCD se apaga. Hacé clic otra vez para volver a encender el ATFS1. También es el remedio más rápido para una nota colgada.

![Las perillas grandes, el botón de modo entre los LED y la llave de encendido.](img/front-knobs.jpg)

## Página Play

En la página Play elegís y tocás sonidos.

![La página Play con el preset Vowel Choir.](img/play-page.jpg)

### Presets

- **Search presets**: filtra la lista por nombre o categoría.
- **Botones de categoría**: All, Vo (voces), Pd (pads), Ld (leads), Ky (teclados), Bs (bajos), Fx (efectos), Sq (secuencias) y User.
- La **lista**: hacé clic en un preset para cargarlo. El número es su lugar en la lista de programas del DAW; el código de la derecha, su categoría.
- **<** y **>**: preset anterior o siguiente de la lista filtrada.
- **Save as...**: guarda un preset propio (ver Presets).

### La vista en vivo

La columna del medio muestra el algoritmo con el nivel en vivo, el espectro de la salida y ocho tarjetas de operadores con su forma, su relación o frecuencia (o su pista de Fseq) y dos medidores, verde con voz y gris sin voz.

### Perform

- La pantalla del **Formant Sequence**, con el cursor de reproducción mientras suena una nota.
- **Formant Sequence**: enciende o apaga la secuencia.
- El **menú de secuencias**: las secuencias de fábrica. Una secuencia que hiciste o importaste aparece con su nombre y `(own)`; `No Fseq` quiere decir que no hay ninguna.
- Perillas: **V/N**, **Formant**, **FM**, **Cutoff**, **Resonance**, **Attack**, **Release** y **Fseq speed**, los mismos parámetros de la parte que en las páginas Perform y Fseq.

## Página Voice

La página Voice muestra la voz entera: el algoritmo a la izquierda y los parámetros principales de los 16 operadores a la derecha.

![La página Voice: el algoritmo y las tablas de operadores con voz y sin voz.](img/voice-page.jpg)

- El campo de **nombre**: hasta 10 caracteres, se guarda en la voz y en su sysex.
- **Choose algorithm...**: abre una grilla con los 88 algoritmos; hacé clic en uno para elegirlo.
- El **gráfico del algoritmo**: los moduladores arriba de sus portadoras, el lazo de realimentación y el nivel en vivo. Hacé clic en un operador para resaltarlo. En las esquinas se ven el algoritmo y el camino de la realimentación con su cantidad, por ejemplo `FB OP1 3`.
- **Algorithm**: 1 a 88.
- **Feedback**: 0 a 7.
- **Note shift**: -24 a +24 semitonos.
- **Category**: la categoría de voz del FS1R (Pf, Or, Ld, Pd, Vo y otras). Es sólo un rótulo.

La tabla de arriba tiene una fila por operador con voz: **Form**, **Mode**, **Coarse**, **Fine**, **Detune**, **Transp**, **Skirt**, **BW/Res**, **Level**, **Fseq** y **Track**. La de abajo, una fila por operador sin voz: **Mode**, **Freq**, **Fine**, **Transp**, **Band**, **Reso**, **Skirt**, **Level** y **Fseq**. Son los parámetros de la página Operator, uno al lado del otro. Un clic en el número de operador lo abre en la página Operator.

## Algoritmos y realimentación

El algoritmo decide cómo se conectan los ocho operadores con voz. Una **portadora** se escucha; un **modulador** no se escucha directamente sino que cambia la fase del operador que tiene debajo: eso es modulación de frecuencia. Cuanto más nivel tiene el modulador, más parciales agrega. A nivel 99 corre la fase de la portadora unos 3,4 ciclos, suficiente para tonos brillantes y metálicos.

Los 88 algoritmos son los del FS1R. El algoritmo 1 tiene los ocho operadores como portadoras, lo habitual en las voces de formantes, donde cada operador es un formante. El 8 apila el operador 1 sobre el 2; el 6 tiene dos pilas, 1 sobre 2 y 3 sobre 4; los números más altos tienen pilas más largas y varios moduladores por portadora. Los operadores sin voz van directo a la salida.

![El gráfico del algoritmo del preset Tine Piano: algoritmo 6, dos pilas, realimentación en el operador 1.](img/algorithm.jpg)

La mayoría de los algoritmos tiene un lazo de **realimentación**: un operador se modula a sí mismo, o uno posterior realimenta a uno anterior (`FB --` quiere decir que no tiene). Feedback 0 lo apaga y cada paso duplica la cantidad; sobre un seno, aclara el tono hacia un diente de sierra y se vuelve áspero arriba de todo.

Las formas del FS1R funcionan también dentro de la FM: un modulador puede ser all1 u odd1 y una portadora, un formante.

## Página Operator

La página Operator muestra un operador. Los botones de arriba lo eligen (**1** a **8**) y eligen el lado **Voiced** (con voz) o **Unvoiced** (sin voz). **Copy** copia los dos operadores del número actual; **Paste** pega el lado que estás mirando.

A la derecha: el **espectro** del operador solo en La 3 (A3, 220 Hz), que se redibuja mientras editás (los armónicos como barras, la banda de ruido como curva), la **envolvente de amplitud** y la **envolvente de frecuencia**.

![La página Operator con un operador con voz res2: parámetros, espectro y envolventes.](img/operator-voiced.jpg)

### Formas espectrales

| Forma | Qué se escucha |
|---|---|
| sine | Una parcial. La forma de la FM clásica. |
| all1 | Un grupo de armónicos alrededor de la frecuencia del operador; con falda 0 da una parcial, más falda abre el grupo. |
| all2 | Como all1, con un grupo más ancho y brillante. |
| odd1 | Sólo armónicos impares: hueco, hacia una onda cuadrada. |
| odd2 | Armónicos impares, grupo más ancho. |
| res1 | Tres líneas en el armónico que fija la resonancia: un silbido que recorre los parciales. |
| res2 | Como res1, con otra forma de ventana. |
| frmt | Un formante: granos a la altura de la nota, portadora en la frecuencia del operador. El color de una vocal o del cuerpo de un instrumento. |

### Operador con voz: oscilador y forma

- **Form**: una de las ocho formas.
- **Mode**: ratio sigue a la tecla; fixed es una frecuencia absoluta, el centro de un formante que no se mueve.
- **Coarse**: relación 0,5, 1, 2 y hasta 31. En modo fijo cuenta octavas: 440 Hz × 2^(coarse + fine/128 - 16), así que 12 es 27,5 Hz, 16 es 440 Hz y 21 unos 14 kHz. El campo muestra el resultado (`x2.00`, `880 Hz`).
- **Fine**: en ratio, +1 % por paso hasta 99; en fijo, 1/128 de octava por paso.
- **Detune**: -15 a +15, unos pocos cents, menos en las octavas altas. Sólo en ratio.
- **Transpose**: -24 a +24 semitonos, sólo frmt: corre la altura de los granos, la fundamental, sin mover el formante.
- **Key sync**: encendido reinicia el oscilador en cada nota. No en frmt.
- **Note scale**: 0 a 99, sólo en fijo: cuánto sigue la frecuencia al teclado alrededor de C3 (99 lo sigue entero).
- **Skirt**: 0 a 7, la pendiente de la ventana. Abre el grupo de armónicos (all, odd, res) o las faldas del formante (frmt), cerca de -1,5 dB por paso.
- **BW / Reso**: 0 a 99. En frmt, el ancho de banda: el ancho en Hz se duplica cada 8 pasos, sea cual sea la nota (el campo lo muestra, `40  99 Hz`). En res1 y res2, la resonancia: el grupo se ubica en el armónico valor + 1 (`h5`).
- **BW bias**: -7 a +7, sólo frmt: cuánto cambia este formante el controlador Voiced Bandwidth.
- **Fseq**: encendido toma la frecuencia y el nivel del Formant Sequence.
- **Fseq track**: Tr1 a Tr8, la pista que maneja este operador y a su compañero sin voz.

### Operador con voz: nivel, escalado y sensibilidad

- **Level**: 0 a 99, 0,75 dB por paso; en un modulador, la profundidad de FM. Con nivel 0 (y sin Fseq) el operador queda apagado.
- **Break point**: de A-1 a C8, el pivote del escalado de nivel.
- **Left depth** y **Right depth**: 0 a 99, el cambio de nivel debajo y arriba del punto de quiebre.
- **Left curve** y **Right curve**: -lin, -exp, +exp o +lin. Un modulador que baja hacia arriba evita que las notas agudas se pongan ásperas.
- **Amp velocity**: -7 a +7, velocidad al nivel.
- **Amp mod**: 0 a 7, sensibilidad al trémolo del LFO1.
- **Pitch mod**: 0 a 7, sensibilidad al vibrato del LFO1. En 0 el operador ignora el vibrato, incluido el de la rueda.
- **Freq mod**: 0 a 7, sólo en fijo: el LFO1 hace ondular la frecuencia.
- **Freq velocity**: -7 a +7, sólo en fijo: la velocidad mueve la frecuencia.
- **Freq bias**: -7 a +7, sólo en fijo: el controlador Freq Bias mueve la frecuencia.
- **EG bias**: -7 a +7: el controlador Amp EG Bias cambia el nivel.
- **Hold**: 0 a 99, una espera antes del ataque.
- **Time scale**: 0 a 7, las notas agudas recorren la envolvente más rápido.

### Operador sin voz

Un operador sin voz es ruido blanco convertido en una banda alrededor de un centro: el aliento, el susurro, el siseo de una s.

![La página Operator con el operador sin voz 1 de Noise Wind: una banda de ruido en 800 Hz.](img/operator-unvoiced.jpg)

- **Mode**: normal tiene su propio centro; linkFO lo pone en la altura de la nota; linkFF sigue al operador con voz del mismo número cuando ese es frmt, así el ruido viaja sobre el formante.
- **Coarse**: 0 a 21, el centro en octavas, con la misma ley que un operador con voz fijo (19 es 3,5 kHz). Sólo normal.
- **Fine**: 0 a 127, 1/128 de octava por paso. Sólo normal.
- **Transpose**: -24 a +24 semitonos, en cualquier modo.
- **Note scale**: 0 a 99, seguimiento de teclado del centro alrededor de C3. Sólo normal.
- **Bandwidth**: 0 a 99, unos 17 Hz por paso hasta 60. Los anchos angostos suenan casi como un seno; los anchos, sisean.
- **BW bias**: -7 a +7, respuesta al controlador Voiced Bandwidth.
- **Resonance**: 0 a 7. Hasta 3 no cambia nada; 4 angosta la banda; de 5 a 7 suman un tono puro en el centro.
- **Skirt**: 0 a 7, cada paso ensancha la banda unas 1,35 veces.
- **Level**: 0 a 99, 0,75 dB por paso.
- **Key scale**: -7 a +7, inclina el nivel a lo largo del teclado alrededor de C3.
- **Amp velocity**, **Amp mod** y **EG bias**: como en el operador con voz.
- **Freq mod** y **Freq bias**: el LFO1 y el controlador Freq Bias mueven el centro. Sólo normal.
- **Fseq**: encendido toma el centro y el nivel de la secuencia, en la pista del operador con voz del mismo número.
- **Hold** y **Time scale**: como en el operador con voz.

## Envolventes

### Envolvente de amplitud

Cada operador con voz y sin voz tiene una envolvente de cuatro etapas en la página Operator. Arrastrá un punto: de costado cambia el tiempo, hacia arriba y abajo el nivel. La zona sombreada es el sostén, y el punto bajo el mouse muestra sus valores.

- Arranca en L4, va a L1 en el tiempo T1, a L2 en T2 y a L3 en T3, y se queda en L3 mientras la tecla está apretada. Al soltar va a L4 en T4.
- Los niveles y los tiempos van de 0 a 99; los tiempos más altos son más lentos. Un recorrido completo dura 6,1 ms en 0 y unos 350 s en 99.
- Los decaimientos bajan en línea recta en decibeles; los ataques suben rápido y se asientan.

El primer punto fija L4, donde la envolvente empieza y termina. Las perillas **ATTACK** y **RELEASE** del frente y la perilla **Decay** de la página Perform corren estos tiempos en todos los operadores a la vez.

### Envolvente de frecuencia

Cada operador tiene además una envolvente de frecuencia: del nivel **Initial** al nivel **Attack** en el tiempo de ataque, y de vuelta a lo normal en el tiempo de decaimiento. Los niveles van de -50 a +50, hasta dos octavas para cada lado. Llega a los operadores con voz en modo ratio y a los sin voz en modo normal: es el golpe de altura del principio de un sonido percusivo.

## Página Common

La página Common tiene lo que comparte toda la voz.

![La página Common: envolvente de altura, LFO, filtro con su envolvente y las matrices de perillas.](img/common-page.jpg)

### Envolvente de altura

Dobla la altura de toda la voz: L0, después L1, L2 y L3 en T1 a T3, se sostiene L3, y va a L4 en T4 al soltar. Niveles de -50 a +50; arrastrá los puntos.

- **Range**: 8 oct, 2 oct, 1 oct o 1/2 oct, hasta dónde llega un nivel 50.
- **Velocity**: 0 a 7, las notas suaves reciben menos envolvente.
- **Time scale**: 0 a 7, las notas agudas van más rápido.

### LFO 1 y LFO 2

El LFO1 da vibrato, trémolo, formantes que se mueven y barridos de filtro; el LFO2 sólo mueve el filtro.

- **Wave**: triangle, saw down, saw up, square, sine o S&H (escalones al azar).
- **Speed**: LFO1 de 0 a 99, LFO2 de 0 a 127, los dos desde unos 0,06 Hz hasta 50 Hz. El campo muestra los Hz.
- **Delay** (LFO1): 0 a 99, entrada gradual después de que empieza la nota; el vibrato tardío de un cantante.
- **Key sync**: encendido reinicia el LFO en cada nota.
- **Phase** (LFO2): 0, 90, 180 o 270 grados, el punto de partida con key sync.
- **Pitch depth** (LFO1): 0 a 99, vibrato, escalado por el Pitch mod de cada operador.
- **Amp depth** (LFO1): 0 a 99, trémolo, escalado por el Amp mod de cada operador.
- **Freq depth** (LFO1): 0 a 99, mueve las frecuencias fijas y los centros de ruido, escalado por cada Freq mod.
- **Filter depth**: 0 a 99, mueve el corte.

### Filtro

Cada voz tiene su propio filtro después de los operadores. Como en el original, la parte lo enciende (la caja **Filter** de la página Perform, encendida de fábrica) y la voz le da forma.

- **Type**: LPF24, LPF18, LPF12 (pasa bajos de 24, 18 o 12 dB por octava), HPF, BPF o BEF (rechaza banda).
- **Cutoff**: 0 a 127, 17,4 Hz × 2^(valor/12); 127 lo deja abierto.
- **Resonance**: -16 a +100. Lo negativo es más amortiguado que plano; arriba de todo roza la autooscilación.
- **Reso velocity**: -7 a +7.
- **Input gain**: -12 a +12 dB; los valores positivos saturan el filtro con una distorsión suave.
- **Key scale**: -64 a +63, seguimiento de teclado del corte; 63 sigue el teclado entero.
- **Key point**: la nota sobre la que pivota el seguimiento.
- **EG depth**: -64 a +63, cantidad de envolvente del filtro; lo negativo la invierte.
- **EG velocity**: -7 a +7, la velocidad escala la profundidad.
- **EG attack vel**: 0 a 7, las notas fuertes arrancan más rápido.
- **EG time scale**: 0 a 7, las notas agudas van más rápido.

La **envolvente del filtro**, debajo de los campos, arranca en 0, va a L1, L2 y L3, se sostiene en L3 y va a L4 al soltar. Niveles de -50 a +50.

> Con Authentic polyphony encendido (página Utility), el filtro limita la parte a 16 voces, como en el FS1R. Apagado, se filtran las 32 voces.

### Perillas Formant y FM

Las perillas **FORMANT** y **FM** del frente no tienen una función fija: cada voz decide. Cada perilla tiene cinco casilleros, y cada casillero mueve una cosa de un operador.

![Las matrices de las perillas Formant y FM y la corrección de nivel de portadora.](img/knob-matrix.jpg)

- **Dest**: off, level (nivel de salida; en un modulador, profundidad de FM), freq o width (ancho de banda de un formante frmt o de una banda de ruido).
- **V/N**: V con voz, N sin voz.
- **Op**: 1 a 8.
- **Depth**: -64 a +63. Con +63 el casillero llega al efecto completo a media vuelta de la perilla; con +31, al final del recorrido.

El efecto completo es 24 dB de nivel, una octava de frecuencia o 24 pasos de ancho de banda. Los casilleros se suman, así que una perilla puede abrir un formante, mover otro y apagar una banda de ruido a la vez. Mueven estas perillas los parámetros Formant y FM de la parte, los CC 80 y 81, la rueda y el aftertouch.

### Corrección de nivel de portadora

Los ocho campos **Op** bajan un operador 1,5 dB por paso (0 a 15) cuando el algoritmo lo vuelve portadora, para equilibrar una voz entre algoritmos sin tocar los niveles que fijan la profundidad de FM.

## Formant Sequences

Un Formant Sequence registra cómo se mueve una voz, no cómo suena. Cada cuadro guarda la altura y la frecuencia y el nivel de ocho formantes con voz y ocho sin voz; hay 126 cuadros por segundo a velocidad normal, hasta 512 cuadros, poco más de 4 segundos.

Mientras suena una nota, cada operador con su llave **Fseq** encendida toma la frecuencia y el nivel del cuadro actual: la pista 1 maneja los operadores en Tr1, y así; un operador sin voz sigue la pista de su compañero con voz. Todo lo demás sigue funcionando, por eso una misma secuencia puede sonar como una persona, un coro o un robot.

El ATFS1 trae 21 secuencias:

| Tipo | Secuencias |
|---|---|
| Vocales | Vowels AEIOU, Vowels High, Ooh Aah, Ee Oh |
| Coros y tarareos | Choir Ah, Choir Oo, Hum Mm, Breathy Ah |
| Rítmicas | Yah Yah, Wah Wah, Robot Talk |
| Susurro | Whisper Hey |
| Palabras | Audio Tools, One Two, Formant, Shoo Bee Doo, Uno Dos Tres, Yeah, Synthesizer, La La La, I Am A Robot |

Las vocales salen de mediciones de formantes publicadas; las palabras, de una síntesis de voz libre analizada por el ATFS1. Ninguna viene del FS1R.

### Qué necesita una voz para tocar una secuencia

- Una secuencia elegida en el menú de la página Play o Fseq.
- La llave **Formant Sequence** de la parte encendida (elegir del menú la enciende).
- Operadores con **Fseq** encendido: lo ideal son ocho operadores frmt en las pistas 1 a 8, más operadores sin voz para el ruido.

Los presets de voz de fábrica (Talking Formants, Vowel Choir, Robot Voice y otros) están armados así: cargá uno y cambiá la secuencia.

### Página Fseq

![La página Fseq: las pistas de una frase hablada, las fuentes, el generador de vocales y la reproducción.](img/fseq-page.jpg)

La pantalla muestra los formantes con voz como líneas (más gruesas cuanto más fuertes), los sin voz como puntos y la altura abajo. Una línea vertical sigue la reproducción. Dejá el mouse encima para leer el número de cuadro, la altura (F0) y los dos primeros formantes (F1, F2).

La zona sombreada es el lazo. Arrastrá cerca de un borde para moverlo, o arrastrá en otro lugar para dibujar un lazo nuevo. Si Loop estaba en From Fseq, al tocar la pantalla pasa a Round.

### Reproducción

- **Speed**: 10 a 1000 %. A 100 % la secuencia corre a 126 cuadros por segundo, como en el FS1R; el rango es más amplio que el del original. Lento, una palabra se vuelve un pad; rápido, una frase se vuelve textura.
- **Fseq**: la misma llave que Formant Sequence en la página Play.
- **Interpolation**: Off (original) salta de cuadro en cuadro como el FS1R, con un movimiento algo granulado; On desliza entre cuadros, lo que ayuda mucho a velocidades lentas.
- **Pitch**: con Fseq pitch la melodía de la secuencia mueve la nota y la tecla la transpone. Con Key pitch sólo la tecla fija la altura y la secuencia sólo mueve los formantes. Los acordes y los coros casi siempre piden Key pitch.
- **Loop**: From Fseq usa el lazo guardado en la secuencia (una generada repite su última vocal; una grabación analizada no tiene lazo y suena una vez). One-way llega al final del lazo y salta al principio. Round va y viene. El lazo se repite mientras mantenés la tecla; al soltar, la secuencia sigue hasta el final.
- **Start frame**: 0 a 511, dónde empieza cada nota. Salteá una respiración o empezá un lazo por el medio.
- **Loop start** y **Loop end**: 0 a 511, el lazo de One-way y Round.
- **Speed velocity**: 0 a 7. Las notas suaves van más lento; con 7, una nota muy suave va a una décima de la velocidad.
- **Level velocity**: -64 a +63. Lo positivo baja los formantes en las notas suaves (hasta -24 dB); lo negativo hace lo contrario.
- **Delay**: 0 a 99, hasta unos 2 segundos antes de que la secuencia empiece a moverse; mientras tanto la voz se queda en el cuadro inicial.

Sin lazo, la secuencia se queda en su último cuadro mientras suena la nota.

### Fuentes

El **menú de secuencias** lista las de fábrica; elegir una enciende la reproducción. **Import .syx...** lee sysex del FS1R (ver Sysex del FS1R). **Export .syx...** guarda la secuencia actual como bulk de secuencia del FS1R.

### Crear una secuencia desde una grabación

El FS1R sólo podía tocar secuencias hechas por Yamaha. El ATFS1 las arma desde cualquier grabación.

- Soltá un WAV, AIFF o FLAC en cualquier parte del editor, o hacé clic en **Analyze audio...** y elegí uno.
- Se analizan los primeros 4 segundos, más o menos (hasta 512 cuadros), en segundo plano. La línea de estado muestra `Analyzing...`, después un mensaje como `Fseq from hola.wav: 212 frames`, y se abre la página Fseq.
- La altura se sigue por autocorrelación (60 a 500 Hz), los formantes se encuentran por predicción lineal y el ruido se mide en ocho bandas.
- La secuencia toma los primeros 8 caracteres del nombre del archivo. La reproducción se enciende y Loop queda en From Fseq: suena una vez hasta que dibujes un lazo.

Una buena fuente es una sola voz seca, sin música ni reverb: habla, canto, un silbido. El estéreo se mezcla a mono; una fracción de segundo es demasiado corta.

### Crear una secuencia desde vocales

- **Vowels**: hasta 24 caracteres entre `a e i o u y m`, y `-` para un silencio. `y` es la "ü" redondeada; `m` es un tarareo cerrado. Por ejemplo `aeiou`, `oa` o `ya-ya-ya-`.
- **Pitch Hz**: 50 a 600 Hz. Importa con Fseq pitch; con Key pitch decide la tecla.
- **Seconds each**: 0,05 a 2 s por vocal; cada una se desliza hacia la siguiente en el último tercio de su tiempo.
- **Low voice** o **High voice**: High sube los formantes un 17 % y la altura 1,8 veces.
- **No breath**, **Breathy** o **Whisper**: cuánto ruido llevan los formantes sin voz.
- **Create Fseq**: la arma y la carga, con los primeros 8 caracteres que escribiste como nombre.

El resultado tiene un vibrato leve y repite su última vocal yendo y viniendo, así una nota sostenida sigue cantando.

![La parte de abajo de la página Fseq: Source, Vowel generator y Playback.](img/fseq-tools.jpg)

### Guardar tus secuencias

Una secuencia que creás o importás reemplaza la de la parte. Se guarda en el proyecto del DAW, en los presets propios que guardes con ella y como `.syx` con **Export .syx...**.

## Página Perform

La página Perform tiene la parte. Cada control es un parámetro del DAW y se guarda en los presets.

![La página Perform con Formant Lead: mono, portamento y la perilla Formant en la rueda y el aftertouch.](img/perform-page.jpg)

### Parte y desvíos de envolvente y filtro

- **Volume** y **Pan** (L63 a R63).
- **V/N balance**: -64 a +63. Lo positivo baja los operadores con voz (sólo ruido en +63), lo negativo los sin voz.
- **Note shift**: -24 a +24 semitonos, sumados a los de la voz.
- **Detune**: -64 a +63, unos 50 cents para cada lado.
- **Attack**, **Decay** y **Release**: -64 a +63, se suman a T1, a T2 y T3, y a T4 de todos los operadores. Lo positivo es más lento. Attack y Decay valen al empezar la nota; Release, al soltarla, así que bajarlo también acorta las notas que ya están apretadas.
- **Formant** y **FM**: -64 a +63, giran las perillas de la voz (las del frente).
- **Cutoff**, **Resonance** y **Filter EG**: se suman al corte, la resonancia y la profundidad de envolvente del filtro.
- **LFO1 rate**, **LFO1 pitch**, **LFO1 delay**, **LFO2 rate** y **LFO2 depth**: se suman a las velocidades de los LFO, a la profundidad de altura y el retardo del LFO1, y a la profundidad de filtro del LFO2.
- **Initial**, **Attack time**, **Release level** y **Release time**: se suman a L0, T1, L4 y T4 de la envolvente de altura.

### Voice

- **Filter**: enciende el filtro de la voz en esta parte.
- **Mono/Poly**: en Mono, una nota tocada mientras otra está apretada va ligada: las envolventes siguen y se mantiene la primera velocidad. Al soltar la nota de arriba vuelve a la que sigue apretada.
- **Voices**: 1 a 32. Al pasar el límite, se toma primero la nota soltada más vieja y después la apretada más vieja.
- **Portamento**: deslizamiento de altura entre notas.
- **Porta mode**: Fingered desliza sólo cuando tocás ligado; Fulltime en todas las notas.
- **Porta time**: 0 a 127, de unos 1,5 ms a 2,4 s por octava.
- **Bend up** y **Bend down**: 0 a 24 semitonos.

### Velocidad y controladores

- **Vel depth**: 0 a 127, cuánto cuenta la velocidad: 64 como la tocás, 127 exagerada, 0 nada.
- **Vel offset**: 0 a 127, se suma a cada velocidad (64 no cambia nada). Con Vel depth en 0, todas las notas tienen la misma velocidad, la que fija Vel offset.
- **Mod wheel** y **MW depth** (-63 a +63): de fábrica, la rueda agrega vibrato del LFO1, profundidad 40.
- **Aftertouch** y **AT depth**: de fábrica va a Formant con profundidad 0, así que no hace nada hasta que subas la profundidad.

| Destino | Qué hace |
|---|---|
| Off | Nada. |
| LFO1 Pitch Mod | Agrega vibrato. |
| LFO1 Amp Mod | Agrega trémolo. |
| LFO1 Freq Mod | Agrega movimiento del LFO a las frecuencias fijas y los centros de ruido. |
| LFO1 Filter Mod | Agrega movimiento del LFO al corte. |
| Cutoff | Mueve el corte. |
| Formant | Gira la perilla Formant. |
| FM | Gira la perilla FM. |
| V/N Balance | Mueve el balance entre con voz y sin voz. |
| Fseq Speed | Acelera la secuencia (o la frena con profundidad negativa). |
| Formant Scratch | La posición del controlador pasa a ser la posición en la secuencia. |
| Freq Bias | Mueve las frecuencias fijas, según el Freq bias de cada operador. |
| Amp EG Bias | Cambia los niveles, según el EG bias de cada operador. |
| Voiced Bandwidth | Ensancha o angosta los formantes, según el BW bias de cada operador. |

## Página Effects

Los efectos siguen la topología del FS1R: inserción sobre la parte, envíos a variación y reverb, variación hacia la reverb y un EQ. Los algoritmos son propios del ATFS1; los nombres siguen la lista del original. El diagrama de arriba muestra el recorrido y atenúa los efectos apagados.

![La página Effects con Robot Voice: inserción Lo-Fi, variación Symphonic y reverb Hall1.](img/effects-page.jpg)

- **Type** de inserción: Thru, Distortion, Overdrive, Amp Sim, Auto Wah, Phaser, Rotary Sp, Lo-Fi o Compressor. **Amount** es la saturación (Distortion, Overdrive, Amp Sim), la sensibilidad (Auto Wah), la velocidad (Phaser), lento o rápido debajo o arriba de la mitad (Rotary Sp), la degradación (Lo-Fi) o el umbral y la relación (Compressor). **Mix** va de seco a procesado entero.
- **Type** de variación: No Effect, Chorus, Celeste, Flanger, Symphonic, Ens Detune, Tremolo, Auto Pan, Delay L,R, Echo o Cross Delay. **Rate** es la velocidad de la modulación o, en los tres retardos, el tiempo de retardo (más alto es más corto, de cerca de 1 s a 80 ms). **Depth** es la profundidad de la modulación o la realimentación del retardo. **Send** alimenta la variación; **To reverb** pasa la variación a la reverb.
- **Type** de reverb: No Effect, Hall1, Hall2, Room1, Room2, Stage, Plate o Canyon, con **Time** y **Send**.
- **Low** y **High**: EQ de estantes, -12 a +12 dB. **Volume** es otra vez el volumen de la parte.

## Página Utility

![La página Utility: sysex, banco importado, sistema y About.](img/utility-page.jpg)

- **Import FS1R .syx...** y **Export voice .syx...**: ver Sysex del FS1R.
- **Imported bank**: cuando un archivo trae varias voces, aparecen acá con su algoritmo y su categoría; hacé clic en una para cargarla.
- **Authentic polyphony**: encendido limita la parte a 16 voces mientras el filtro está encendido, como en el FS1R.
- **Voices**: lo mismo que en la página Perform.
- **Output**: -24 a +6 dB, el nivel final del plugin.
- **About**: qué es el ATFS1, los controladores de fábrica y lo que agrega al original.

Authentic polyphony y Output son globales: los presets no los guardan ni los cambian.

## Sysex del FS1R

El ATFS1 lee y escribe el sysex del FS1R, así las voces y las secuencias viajan entre el plugin, el equipo y los editores que hablan su formato.

### Importar

Usá **Import FS1R .syx...** en la página Utility, **Import .syx...** en la página Fseq, o soltá un archivo `.syx` o `.mid` sobre el editor. Los tres hacen lo mismo:

- Los bulks de voz (608 bytes) se vuelven voces. La primera se carga en el acto; un juego aparece en Imported bank.
- Los bulks de Formant Sequence (hasta 512 cuadros) pasan a ser la secuencia de la parte, y se enciende la reproducción. Una secuencia sin nombre toma el del archivo.
- En los archivos MIDI estándar se buscan los sysex, así que los volcados guardados como `.mid` también sirven.

La línea de estado informa el resultado, por ejemplo `Imported 32 voices and 1 Fseq`.

Las performances no se importan: en el ATFS1 los ajustes de parte, efectos y controladores son parámetros del plugin. Después de importar una voz, revisá la página Perform, sobre todo **Filter**, los desvíos Formant y FM y **Mono/Poly**.

> Muchas voces del FS1R se hicieron para una de las secuencias internas del equipo, que no vienen incluidas. Si una voz importada tiene operadores en Fseq, elegí una secuencia de la biblioteca o importá su bulk de secuencia.

### Exportar

**Export voice .syx...** (Utility) escribe la voz actual como un bulk que el FS1R acepta; **Export .syx...** (Fseq) escribe la secuencia actual. Mandalos con cualquier utilidad de sysex; cada campo de la voz viaja en los dos sentidos.

## Presets

### Presets de fábrica

| Categoría | Presets |
|---|---|
| Vo | Talking Formants, Shoo Bee Doo, Uno Dos Tres, Robot Voice, Vowel Choir, High Choir, La La Lead, Whisper Pad |
| Pd | Vowel Morph Pad, Formant Strings, Glass Choir, Resonant Sweep Pad, Odd Pad |
| Sq | Yah Yah Riff, Robot Talk Seq |
| Ld | Formant Lead, Odd Square Lead, Res Brass, Breath Flute |
| Ky | Tine Piano, Formant Bells, Glass Marimba, Formant Clav, Formant Organ, Init Formant |
| Bs | Wah Bass, FM Bass, Formant Bass, Odd Sub Bass |
| Fx | Noise Wind, Space Talk, Formant Scratch |

Init Formant es un solo operador frmt fijo, un buen punto de partida.

### Guardar los tuyos

1. Armá el sonido.
2. En la página Play hacé clic en **Save as...**.
3. Escribí un nombre y hacé clic en **Save**.

El preset aparece en User. Guarda la voz, la parte y los efectos, y la secuencia (una de fábrica por su nombre, una propia entera). Guardar con un nombre que ya existe lo reemplaza. Los presets propios son archivos JSON en:

- macOS: `~/Library/Application Support/Audio Tools/ATFS1/Presets/`
- Windows: `%APPDATA%\Audio Tools\ATFS1\Presets\`

Renombralos, borralos o compartilos ahí; aparecen después de los de fábrica.

## MIDI y controladores

El ATFS1 responde en todos los canales MIDI.

| Mensaje | Efecto |
|---|---|
| Pitch bend | Rangos Bend up y Bend down |
| CC 1 (rueda) | Destino de Mod wheel |
| Aftertouch de canal o de tecla | Destino de Aftertouch |
| CC 5 | Tiempo de portamento |
| CC 7 | Volumen |
| CC 64 | Sustain |
| CC 65 | Portamento encendido o apagado |
| CC 71 | Desvío de resonancia |
| CC 72 | Desvío de release |
| CC 73 | Desvío de ataque |
| CC 74 | Desvío de corte |
| CC 80 | Perilla Formant |
| CC 81 | Perilla FM |
| CC 91 | Envío a reverb |
| CC 93 | Envío a variación |
| CC 120, 123 | Todas las notas apagadas |
| CC 121 | Reinicia los controladores |

Los CC 80, 81 y 71 a 74 son los de fábrica del FS1R. No hay sincronía con el tempo: ajustá a oído la velocidad del Fseq, los LFO y los retardos.

## Recetas

### Un coro de vocales

1. Cargá **Talking Formants**.
2. En la página **Fseq** escribí `ooaa` en Vowels, poné **Seconds each** en `1.2`, elegí **Low voice** y **Breathy**, y hacé clic en **Create Fseq**.
3. Poné **Pitch** en Key pitch y **Speed** en 50 %.
4. En **Perform**, poné **Voices** en 24 y **Attack** cerca de +10.
5. En **Effects**, elegí **Symphonic** (Send cerca de 35) y **Hall2** (Send cerca de 35, Time cerca de 70).
6. Tocá acordes lentos. La voz ya trae un vibrato tardío (LFO1 Pitch depth 14, Delay 40).

### Un pad que habla

1. Cargá **Vowel Morph Pad** y elegí **One Two** o **Synthesizer** en el menú de secuencias.
2. En la página Fseq poné **Pitch** en Key pitch, **Interpolation** en On y **Speed** en 20 o 25 %.
3. Arrastrá un lazo sobre la parte de la frase que te guste; poné **Loop** en Round.
4. En Perform poné **V/N balance** cerca de +20 y subí **Release**.

Los acordes sostenidos articulan las palabras despacio.

### Una campana FM

1. Cargá **Init Formant**. En la página **Voice** hacé clic en **Choose algorithm...** y elegí el **8** (el operador 1 modula al 2).
2. Abrí el operador **2**: **Form** sine, **Mode** ratio, **Coarse** 1, **Level** 99. Dale un ataque instantáneo, L2 cerca de 80, T2 y T3 largos hasta L3 = 0, T4 cerca de 55 y **Time scale** 3.
3. Abrí el operador **1**: sine, ratio, **Coarse** 3, **Fine** 50 (`x3.50`), **Level** cerca de 65, ataque instantáneo y un decaimiento más rápido que el de la portadora, **Amp velocity** +4.
4. Poné **Feedback** en 2 y agregá una reverb **Plate**.

Probá relaciones como 1,41 o 2,76 en el operador 1, o **Form** all1 en el modulador.

### Convertir una grabación en un Formant Sequence

1. Buscá o grabá una frase corta: una sola voz seca, de menos de 4 segundos, en WAV o AIFF.
2. Cargá **Talking Formants**.
3. Arrastrá el archivo sobre el editor; se abre la página Fseq con las pistas nuevas.
4. Tocá una nota cerca de la altura de la grabación: la frase vuelve con su propia melodía.
5. Para tocar tu propia melodía, poné **Pitch** en Key pitch.
6. Si empieza con silencio, subí **Start frame**.
7. Para una nota sostenida que siga hablando, arrastrá un lazo sobre una vocal y poné **Loop** en Round.
8. Guardala con **Save as...**, o para un FS1R con **Export .syx...**.

### Un susurro

1. Cargá **Whisper Pad**, que toca **Whisper Hey**, o armá una secuencia propia con **Whisper** elegido.
2. En Perform llevá **V/N balance** a +63: queda sólo el ruido.
3. Apretá **MUTE/SOLO** para comparar todo, con voz y sin voz.

### Recorrer una frase con la rueda

1. Cargá **Formant Scratch**, o cualquier voz con Fseq y poné **Mod wheel** en Formant Scratch, **MW depth** en +63.
2. Mantené una nota y mové la rueda: abajo es el primer cuadro, arriba el último.

### Un barrido de formante

1. Cargá **Init Formant**.
2. En **Common**, casillero 1 de la perilla Formant: **Dest** freq, **V/N** V, **Op** 1, **Depth** +40.
3. Casillero 2: **Dest** width, **Op** 1, **Depth** -30, así el formante se angosta mientras sube.
4. Mantené una nota y girá **FORMANT** en el frente.
5. En Perform poné **Aftertouch** en Formant y **AT depth** en +40 para barrer con la presión de la tecla.

### Un lead ligado

1. Cargá **Formant Lead**: Mono, **Portamento** en On, **Porta mode** en Fingered.
2. Las notas superpuestas se deslizan y mantienen las envolventes; las notas separadas arrancan de cero sin deslizar.
3. Pasá **Porta mode** a Fulltime para deslizar en todas las notas.

### Un lazo rítmico de vocales

1. Cargá **Yah Yah Riff**; **Loop** está en One-way.
2. Arrastrá el lazo sobre una o dos sílabas.
3. Girá **Speed** hasta que las sílabas calcen con tu tempo, con Shift para los pasos finos.
4. Poné **Speed velocity** en 3 o 4 para que las notas suaves canten más lento.

### Una flauta con aliento

1. Cargá **Init Formant**. Hacé que el operador 1 sea **sine**, ratio, Coarse 1, Level 96, T1 cerca de 10, **Pitch mod** 5.
2. Hacé clic en **Unvoiced**. Operador 1: **Mode** linkFO, **Transpose** +24, **Bandwidth** 40, **Level** cerca de 62, una envolvente rápida que baja a un sostén bajo.
3. En Common dale al LFO1 onda sine, **Pitch depth** 12 y **Delay** 50.
4. En Perform encendé Mono y Portamento con un tiempo corto.

El ruido queda dos octavas arriba de la nota, como un soplido.

## Consejos y problemas

### No suena

- **POWER** apagado: el LCD está oscuro.
- **VOLUME** del frente u **Output** de Utility abajo.
- Un solo activo: apretá **MUTE/SOLO** hasta que el renglón 2 no muestre ni `V` ni `N`.
- Ninguna portadora con **Level** arriba de 0, o **V/N balance** en +63 en una voz sin ruido.

### La secuencia no suena

- **Formant Sequence** apagado, o el menú dice `No Fseq`.
- Las llaves **Fseq** de los operadores apagadas (página Voice, columna Fseq).
- Un **Delay** largo, un **Start frame** cerca del final o **Speed velocity** en 7 con notas suaves.

### La secuencia canta en una altura rara

Con **Pitch** en Fseq pitch la secuencia trae su propia melodía. Tocá cerca de la altura original o pasá a Key pitch.

### La rueda no da vibrato

Cada operador recibe vibrato sólo hasta su **Pitch mod**. Subilo en las portadoras.

### El análisis suena mal

Usá una sola voz seca, sin música ni reverb. Sólo se usan los primeros 4 segundos, más o menos.

### Clics, cortes o mucho CPU

Bajá **Voices** o encendé **Authentic polyphony**, y subí el buffer de tu interfaz. Los operadores en nivel 0 no cuestan nada.

### Una nota colgada

Hacé clic dos veces en **POWER**, o mandá un todas las notas apagadas (CC 120 o 123). Pasar entre Mono y Poly también suelta las voces.

## Especificaciones

| Ítem | Valor |
|---|---|
| Versión | 1.0.0 |
| Formatos | VST3, Audio Unit (macOS), independiente |
| Sistemas | macOS 11 o posterior (Apple silicon e Intel); Windows 10 y 11 de 64 bits |
| Salida | estéreo (admite mono) |
| Polifonía | 1 a 32 voces; 16 con Authentic polyphony y el filtro encendido |
| Partes | una por instancia |
| Operadores | 8 con voz (8 formas espectrales) y 8 sin voz por voz |
| Algoritmos | 88, realimentación 0 a 7 |
| Envolventes | amplitud con hold y frecuencia en cada operador; altura; filtro |
| LFO | LFO1 (altura, amplitud, frecuencia, filtro), LFO2 (filtro); 6 ondas |
| Filtro | LPF 24, 18, 12 dB, HPF, BPF, BEF; resonancia -16 a +100; ganancia de entrada -12 a +12 dB |
| Formant Sequences | 8 formantes con voz y 8 sin voz más la altura; 126 cuadros por segundo; hasta 512 cuadros; velocidad 10 a 1000 % |
| Efectos | 8 tipos de inserción, 10 de variación, 7 reverbs, EQ de 2 bandas, limitador suave |
| Contenido de fábrica | 32 presets, 21 Formant Sequences |
| Sysex | bulks de voz (608 bytes) y de Fseq del FS1R, lectura y escritura; .syx y .mid |

### Mediciones

Medidas con las herramientas de prueba del propio ATFS1.

| Prueba | Resultado |
|---|---|
| all1 con falda 0 | una parcial, la siguiente debajo de -100 dB |
| formas odd con falda 0 | sólo las parciales 1 y 3 |
| res1, resonancia 4 | líneas en los armónicos 4, 5 y 6; laterales a -6,0 dB; armónico 2 debajo de -130 dB |
| ancho del grupo con falda 7, de un lado | all1 1,23, all2 3,35 parciales |
| recorrido de envolvente | 6,1 ms (tiempo 0) a 350 s (tiempo 99); decaimiento lineal en dB |
| WAV a Fseq | /a/ sintética en 120 Hz: altura 120 Hz; F1, F2, F3 en 731, 1102, 2454 Hz (objetivos 730, 1090, 2440) |
| resíntesis de habla | las pistas de F1 y F2 correlacionan 0,82 y 0,74 con la frase analizada |
| sysex | voz de 619 bytes con suma de verificación, cada campo va y vuelve; frecuencias del Fseq dentro de 1,7 cents |
| CPU, Apple M4 | habla 0,4 %, coro de 16 voces 6,8 %, 32 voces 8,7 % de un núcleo |
| CPU, Ryzen 9 5900XT | habla 0,7 %, coro de 16 voces 13,9 %, 32 voces 21,9 % de un núcleo |

Yamaha y FS1R son marcas de Yamaha Corporation. El ATFS1 no tiene relación ni aval de Yamaha; los nombres sólo identifican el instrumento que lo inspiró.
