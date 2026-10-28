# Manual de Valence

## Qué es Valence

Valence es un arpegiador MIDI de cuatro capas. No hace sonido por sí solo: toma el acorde que tenés apretado, lo toca como arpegios y les manda las notas a otros instrumentos, sean sintes virtuales en tu DAW o sintes físicos por un cable MIDI.

Las cuatro **capas** recorren el mismo acorde, pero cada una tiene su propio largo en pasos, su figura, su patrón, su rango de octavas, su gate, su swing y su canal MIDI. Con largos de 16, 15 y 13 pasos las capas se van separando paso a paso y recién vuelven a coincidir después de minutos: 3120 semicorcheas, 195 compases. Ese momento, cuando todas las capas vuelven juntas a su primer paso, es la **fusión**, y Valence te dice a cuántos compases está.

Cada paso de cada capa tiene sus filas: encendido o apagado, velocidad, acento, ligadura, ratchet, probabilidad, octava, una nota fija, un valor de controlador. Una capa también puede mutar un poco en cada ciclo, así un patrón cambia despacio a lo largo de un tema.

La interfaz es un átomo. Las notas que apretás son los **protones** del núcleo, las notas que agrega Valence para completar el acorde son los **neutrones**, cada capa es una **órbita** con sus pasos marcados, un **electrón** recorre cada órbita mientras la capa toca, y cada nota es un **fotón** que sale del electrón hacia la nota que tocó.

Cada elección al azar sale de un número de **variación**: el mismo acorde, los mismos ajustes y la misma variación tocan siempre las mismas notas, en cualquier DAW y con cualquier tamaño de buffer. Un pequeño **sonido de prueba** incorporado te deja probar todo sin rutear nada, y la **armonía automática** toca una sucesión de acordes sola cuando no tenés un teclado a mano.

## Instalación

Valence viene como plugin VST3, como instrumento Audio Unit y como Audio Unit MIDI FX (sólo macOS), como plugin LV2 (sólo Linux) y como aplicación independiente. Funciona en macOS 11 o posterior (un solo binario universal para Apple silicon e Intel), en Windows 10 u 11 de 64 bits y en Linux de 64 bits.

En macOS:

1. Descomprimí el archivo.
2. Copiá `Valence.vst3` a `~/Library/Audio/Plug-Ins/VST3`.
3. Copiá `Valence.component` y `Valence MIDI FX.component` a `~/Library/Audio/Plug-Ins/Components`.
4. Reescaneá los plugins en tu DAW. En Ableton Live y en REAPER usá el VST3 (más abajo te contamos por qué); en Logic, el MIDI FX.

En Windows:

1. Descomprimí el archivo.
2. Copiá `Valence.vst3` a `C:\Program Files\Common Files\VST3`.
3. Reescaneá los plugins en tu DAW.
4. `Valence.exe` es la versión independiente: funciona sola y toca tus puertos MIDI.

En Linux, copiá `Valence.vst3` a `~/.vst3/` y el bundle LV2 a `~/.lv2/`, y volvé a escanear en tu host.

> Los builds todavía no están firmados. macOS puede negarse a abrirlos hasta que corras `xattr -dr com.apple.quarantine` sobre los plugins instalados, y SmartScreen de Windows puede pedir confirmación la primera vez. Cada paquete trae un INSTALL.txt con los pasos exactos.

## Cargarlo en un DAW

Valence se carga como instrumento en su propia pista MIDI. El acorde que tocás entra en esa pista (desde tu teclado o desde un clip MIDI) y los arpegios salen de ella; tus sintes van en otras pistas y toman su entrada MIDI de la pista de Valence.

Por defecto todas las capas salen por el canal MIDI 1, así que cualquier sinte las toca todas. Con **Output channels** en `Per layer` (en la página Outputs) cada capa conserva su canal, del 1 al 4 salvo que los cambies, para un sinte multitímbrico o para ruteos que filtran por canal.

Valence se queda con las notas que tocás: se vuelven el acorde y no pasan de largo. Los demás mensajes de controlador, el pitch bend y el aftertouch pasan tal cual. El pedal de sustain (CC 64) retiene el acorde mientras está apretado, y un All Notes Off o All Sound Off (CC 123, CC 120) de tu DAW frena el arpegio. Cuando el plugin está en bypass suelta todas las notas que suenan y se olvida de las teclas que estaban apretadas; un acorde retenido con latch queda.

Cada ajuste es también un parámetro del host que podés automatizar. Los primeros 64 son los ajustes globales y los principales de cada capa (`Layer 1 Rate`, `Layer 2 Length`...), que es lo que Ableton Live muestra en el panel del dispositivo; los ajustes finos de cada capa vienen después. Las filas de pasos no son parámetros: se guardan con el proyecto y con los presets.

### En Ableton Live

1. Arrastrá el VST3 de Valence a una pista MIDI vacía. Live no saca el MIDI que producen los plugins Audio Unit, así que la versión AU no puede tocar otras pistas ahí.
2. Armá la pista de Valence (o poné su **Monitor** en `In`) para que le llegue tu teclado, o poné un clip MIDI con acordes.
3. Poné tu sinte en otra pista MIDI.
4. En la sección de entrada y salida de la pista del sinte, elegí en **MIDI From** la pista de Valence, y en el selector de abajo elegí `Audio Tools Valence`.
5. Poné el **Monitor** de la pista del sinte en `In`.
6. Hacé clic en **Preview sound**, en la barra superior de Valence al lado de Play, hasta que diga `Preview sound: Off`. Así escuchás sólo tu sinte.
7. Apretá un acorde.

Live junta todos los canales MIDI cuando rutea MIDI de una pista a otra, así que ese sinte recibe todas las capas encendidas. Para un sinte que toca todo, perfecto. Para mandar cada capa a un sinte distinto hay dos caminos:

- **Un sinte multitímbrico.** En la página Outputs de Valence poné **Output channels** en `Per layer`. La capa 1 toca entonces por el canal 1, la capa 2 por el canal 2, y así (el **Channel** de cada capa decide cuál). Cargá un sinte que toque un sonido distinto en cada canal y ruteálo como arriba.
- **Layers as MIDI ports** (macOS). En la página Outputs prendé **Layers as MIDI ports**. Valence crea un puerto MIDI virtual por capa, de `Valence Layer 1` a `Valence Layer 4` (una segunda Valence que también los publica usa `Valence 2 Layer 1` y así). Una sola vez, en Settings de Live, en la pestaña Link, Tempo & MIDI, prendé **Track** en la entrada de cada puerto `Valence Layer`. Después, en cada pista de sinte, elegí en **MIDI From** un puerto, por ejemplo `Valence Layer 2`, con `All Channels`, y poné **Monitor** en `In`. Cada sinte escucha sólo su capa.

En Windows no hay puertos MIDI virtuales propios del sistema: creá un puerto con loopMIDI, elegilo para las capas en la página Outputs de Valence, poné Output channels en `Per layer`, y en cada pista de sinte elegí ese puerto en MIDI From con el canal de su capa.

Live ignora en su propia lista los presets de fábrica de los plugins VST3; usá el menú de presets de Valence, o el navegador: los presets de fábrica aparecen en el navegador de Live en Plug-Ins, Audio Tools Valence (desplegá el plugin), instalados por el launcher o copiando la carpeta Presets del zip.

### En REAPER

Usá el VST3: REAPER tampoco saca el MIDI que producen los plugins Audio Unit.

1. Poné Valence en una pista y armala para grabar con tu teclado como entrada y el monitoreo prendido (o poné en esa pista un ítem MIDI con acordes).
2. Poné tu sinte en otra pista.
3. Mandá la pista de Valence a la del sinte con un envío con MIDI. Con **Output channels** en `Per layer`, poné en cada envío el canal MIDI de la capa que tiene que tocar ese sinte, así varios sintes toman cada uno su capa de una sola Valence.

REAPER muestra cada preset de fábrica dos veces en el menú de presets: una de la lista propia del plugin y otra como archivo (los .vstpreset que instalan el launcher o la carpeta Presets del zip para Ableton Live, en ~/Library/Audio/Presets/Audio Tools/ en macOS, Documentos\VST3 Presets\Audio Tools\ en Windows y ~/.vst3/presets/Audio Tools/ en Linux). Los dos cargan lo mismo. Si no usás Live, podés borrar ahí la carpeta "Audio Tools Valence" y cada preset aparece una sola vez.

La lista de parámetros para automatizar de REAPER muestra primero los parámetros propios de Valence y después unas dos mil entradas que se llaman `MIDI CC`: son la manera en que un VST3 recibe controladores como el pedal de sustain, y podés ignorarlas.

### En Logic

En Logic usá **Audio Tools Valence MIDI FX**, la versión Audio Unit MIDI FX:

1. Creá una pista de instrumento de software con tu sinte.
2. En la ranura **MIDI FX** del canal, arriba del instrumento, elegí Audio Units, Audio Tools, Audio Tools Valence MIDI FX.
3. Tocá un acorde en esa pista, o poné una región con acordes.

El MIDI FX toca el instrumento de su propio canal, y las cuatro capas van a ese instrumento. Para escuchar las capas con sonidos distintos, poné **Output channels** en `Per layer` y usá un instrumento multitímbrico que toque un sonido distinto en cada canal MIDI. El MIDI FX no tiene sonido de prueba: lo que escuchás es siempre tu instrumento.

Lo que todavía no está probado en Logic: si Logic le da a un MIDI FX la posición de la canción, para que Clock en Host trabe las capas a los compases de Logic; Valence toma el tempo de Logic cuando Logic lo informa, y si no, el último tempo que vio o su propio Tempo. Mandar cada capa a una pista de instrumento distinta (con las salidas directas o los puertos virtuales de la versión instrumento) tampoco se probó en Logic.

## Primer sonido

1. Abrí Valence (la aplicación independiente, o el plugin en una pista MIDI).
2. Apretá un acorde de tres o cuatro notas en tu teclado.

El sonido de prueba arranca enseguida y el átomo en el centro de la página Simple se despierta: tus notas aparecen como protones en el núcleo, un electrón recorre cada órbita y cada nota dispara un fotón. Soltá y el arpegio para; prendé **Latch** para que siga tocando cuando soltás.

Si no tenés un teclado a mano, poné **Harmony** en `Auto` (en la página Simple, debajo de las llaves de las capas) y apretá **Play** en la barra superior: Valence toca entonces sola una sucesión de acordes en la tonalidad.

Lo que escuchás es el **sonido de prueba** de Valence, un sinte chico incorporado para probarla sin rutear nada: un pluck, un bajo con filtro, un stab de acorde y un sub, uno por capa. No es tu sinte. Mientras suena, el botón al lado de Play dice `Preview sound: On`. Cuando toquen tus sintes, hacé clic en el botón hasta que diga `Preview sound: Off`: el MIDI que manda Valence es el mismo de las dos maneras.

## La interfaz de un vistazo

La ventana tiene una barra superior, el área de páginas y un pie. Abre al 85 % de su tamaño completo y se puede agrandar o achicar desde la esquina, de la mitad a una vez y media su tamaño completo, manteniendo las proporciones. Vuelve a abrir en la última página que usaste.

![La barra superior: la línea de estado, Play, Preview sound, el selector de presets, Save, las cuatro páginas, Capture MIDI y All notes off.](img/topbar.jpg)

La barra superior, de izquierda a derecha:

- **valence**: el logo. Debajo, la línea de estado muestra `playing`, el tempo y el acorde actual, o `waiting for a chord`.
- **Play** / **Stop**: arranca y frena la armonía automática. Con Harmony en `Played`, Valence toca cada vez que tenés un acorde apretado, diga lo que diga este botón.
- **Preview sound: On** / **Off**: prende y apaga el sonido de prueba incorporado. Está al lado de Play para que siempre se vea si lo que suena es Valence o tu sinte. Es el mismo ajuste que Preview sound en la página Outputs.
- **<** y **>**: el preset anterior y el siguiente. El nombre del preset, entre las dos flechas, abre el menú de presets agrupados por estilo, con los tuyos bajo User. Un asterisco después del nombre quiere decir que cambiaste algo desde que se cargó el preset.
- **Save**: guarda los ajustes actuales, con las filas de pasos, como preset de usuario.
- **Simple**, **Layers**, **Harmony** y **Outputs**: las cuatro páginas.
- **Capture MIDI**: guarda los últimos minutos tocados como archivo MIDI.
- **All notes off**: corta todas las notas que suenan y manda All Notes Off en los 16 canales, en el DAW y en los puertos directos.

El pie muestra una línea de ayuda corta de lo que tengas bajo el mouse, mensajes como el nombre de un archivo capturado, y el archivo capturado mismo, listo para arrastrar al DAW.

Los controles funcionan igual en todos lados:

- Perillas redondas (las macros): arrastrá hacia arriba o hacia abajo. Con Shift el paso es fino, el doble clic vuelve al valor por defecto, y también sirve la rueda del mouse.
- Cajas con un triangulito: un clic abre un menú de opciones.
- Cajas con un número: arrastrá hacia arriba o hacia abajo, o usá la rueda. El doble clic vuelve al valor por defecto.
- Cajas de dos estados (Off y On, Played y Auto): un clic cambia el estado.

## La página Simple

La página Simple tiene el átomo, seis macros que mueven las cuatro capas a la vez, una llave por capa y los ajustes principales de la armonía. Alcanza para tocar Valence sin abrir las otras páginas.

![La página Simple tocando Auto Orbit: el átomo a la izquierda, y a la derecha las macros, las llaves de las capas y los ajustes de la armonía.](img/simple.jpg)

### Macros

![Las seis macros, las llaves de las capas y los ajustes de la armonía.](img/macros.jpg)

El panel **Shape** tiene seis macros. Cada una mueve el mismo ajuste de todas las capas y conserva sus diferencias: si la capa 1 tiene un gate de 30 % y la capa 2 de 50 %, subir Gate 20 puntos deja 50 % y 70 %. Cuando las capas difieren, la perilla muestra el rango (`68-100 %`); si no, muestra el valor. Lee las capas encendidas (las cuatro si no hay ninguna). Volver al punto de partida devuelve cada capa exactamente a su valor, y el doble clic pone todas las capas en su valor por defecto.

Las macros no son parámetros del host propios: cada una escribe los parámetros de las capas, así tu DAW graba exactamente lo que se movió y los presets no cambian.

| Macro | Mueve | Rango |
|---|---|---|
| Figure | el Rate de cada capa | de a una división: 1/4, 1/8, 1/16, 1/32 |
| Density | los Euclid pulses de cada capa | de un paso a todos los pasos de la capa |
| Swing | el Swing de cada capa | 50 a 75 % |
| Octaves | las Octaves de cada capa | 1 a 4 |
| Mutation | el Mutate de cada capa | 0 a 100 % |
| Gate | el Gate de cada capa | 5 a 100 % del paso |

- **Figure** conserva el carácter de cada capa: una capa en tresillo sigue en tresillo (1/8T pasa a 1/16T), una con puntillo sigue con puntillo. La perilla muestra la figura de la primera capa encendida.
- **Density** es la parte de los pasos de cada capa que suena. Al moverla, reparte esa cantidad de pulsos de manera pareja en la capa (un ritmo euclidiano, ver Filas de pasos). Una capa con los pasos dibujados a mano los conserva hasta que su cantidad de pasos cambia de verdad.
- **Mutation** es cuánto cambia cada capa en cada ciclo (ver Mutación).

### Llaves de las capas

Debajo de las macros hay una llave por cada una de las cuatro capas, en el color de la capa. Un clic prende o apaga la capa. Una capa que se apaga suelta sus notas enseguida y su órbita se atenúa en el átomo; si la volvés a prender mientras Valence toca, entra en el paso que le toca por posición, a tiempo con las demás.

### Armonía

Las seis cajas debajo de las llaves son los ajustes principales de la página Harmony:

- **Harmony**: `Played` (las notas que apretás) o `Auto` (una sucesión de acordes que toca sola cuando apretás Play).
- **Key** y **Scale**: los usan Complete, la armonía automática y las notas fijas de la fila Note.
- **Complete**: con una o dos notas apretadas, arma el acorde de la escala sobre la más grave.
- **Latch**: retiene el acorde cuando soltás (el pedal de sustain también lo retiene).
- **Variation**: el número que siembra cada elección al azar. El mismo número toca las mismas notas en todos lados.

## El átomo

![El átomo: protones y neutrones en el núcleo, cuatro órbitas con sus pasos, electrones y fotones, y el contador de la fusión.](img/atom.jpg)

El átomo no es un adorno: cada parte muestra algo que Valence está haciendo.

| Partícula | Qué muestra |
|---|---|
| Protones | Esferas cálidas en el núcleo: las notas del acorde, una por nota. Son las notas que apretás (o que retenés con Latch o el pedal), el acorde de la armonía automática, o el acorde de una Valence líder de Link. |
| Neutrones | Esferas azules, frías, en el núcleo: las notas que agrega Valence con Complete. Con Complete apagado no hay. |
| Órbitas | Una por capa, en el color de la capa, cada una con su inclinación, girando despacio. El tamaño de la órbita es el registro de la capa: las capas más agudas tienen órbitas más anchas. Una capa apagada tiene la órbita atenuada. |
| Marcas de los pasos | Puntos sobre la órbita, uno por paso de la capa. Los puntos llenos suenan, los huecos son silencios; los más tenues tienen menos probabilidad, los más grandes tienen acento, y una línea hasta el punto siguiente es una ligadura. Un ritmo euclidiano se ve como puntos repartidos de manera pareja alrededor de la órbita. |
| Electrón | El punto brillante con cola de cometa: la posición de la capa. Va de una marca a la siguiente mientras la capa toca, así que el swing se ve como un cambio de velocidad. |
| Fotón | Cada nota que suena: un rayo fino del electrón al protón o neutrón que tocó, y un anillo que se abre sobre la órbita, más grande cuanto más fuerte es la nota. Las notas con acento brillan más; un ratchet dispara una ráfaga de fotones. |
| Fusión | Cuando todas las capas vuelven a su primer paso al mismo tiempo, todo el átomo destella y se abre un anillo alrededor del núcleo. |

Con un acorde nuevo el núcleo se reacomoda con un pequeño resorte. Pasá el mouse sobre un protón o un neutrón para ver el nombre de su nota.

En los bordes:

- Arriba a la izquierda: los **protons** y **neutrons**, como nombres de notas (`C3 E3 G3`, `B3 D4`).
- Arriba a la derecha: **fusion in** y la cantidad de compases hasta la próxima fusión, `no fusion ahead` cuando las capas no pueden volver a alinearse (ver Polimetría y fusión), o `fusion in -` cuando Valence no está tocando.
- Abajo a la izquierda: cada capa con su largo y el paso en el que está, u `off`.

![Una fusión en Reese Engine: las capas de 16 y 5 pasos se encuentran en su primer paso cada cinco compases.](img/fusion.jpg)

## Cómo toca Valence

### El acorde: protones y neutrones

Valence arpegia las notas de un acorde, el **núcleo**. De dónde sale el acorde depende de **Harmony**:

- `Played`: las notas que tenés apretadas. Una mano nueva reemplaza el acorde; agregar una nota mientras sostenés las otras la suma. **Transpose** corre todo el acorde hasta dos octavas para arriba o para abajo.
- `Auto`: una sucesión de acordes en la tonalidad y la escala, la misma de Orrery, que toca una tríada que cambia cada **Chord length** compases. Arranca cuando apretás Play (o cuando arranca el transporte del DAW, con Clock en Host). **Progression** elige cómo se mueve: `Walk` camina por los acordes de la escala, `Two chords` alterna entre dos, `Drone` se queda en la tónica.
- Con **Link** en `Follow`, el acorde de la Valence que está en `Lead` (ver Link).

**Latch** retiene el acorde cuando soltás: una mano nueva lo reemplaza, agregar notas las suma, y apagar Latch lo suelta. El pedal de sustain también retiene, mientras esté apretado.

**Complete** agrega notas al acorde. Esas notas agregadas son los **neutrones**; las que apretás son los **protones**:

| Complete | Con una o dos notas apretadas | Con armonía automática |
|---|---|---|
| Off | no agrega nada | no agrega nada |
| Triad | la tríada de la escala sobre la nota más grave | no agrega nada (Auto ya toca una tríada) |
| Seventh | el acorde con séptima de la escala sobre la nota más grave | agrega la séptima |
| Ninth | el acorde con séptima y novena | agrega la séptima y la novena |
| Octaves | cada nota una octava más arriba | cada nota una octava más arriba |

Triad, Seventh y Ninth actúan sólo con una o dos notas apretadas: con tres o más, tu acorde se toca como está. Primero la nota más grave se lleva a la nota más cercana de la escala. Así, con C menor como tonalidad y escala, apretar un D solo con Complete en Seventh toca D, F, Ab y C.

**Chord order** es el orden en que los patrones recorren el acorde: `As played` (el orden en que apretaste las teclas), `Low to high` (el valor por defecto), `High to low`, o `Random` (un orden mezclado fijo que depende de la Variation).

### Patrones

Cada capa elige una nota del acorde por paso (o todas, con Chord) según su **Pattern**. Con un acorde de cuatro notas numeradas de 1 (la primera en el Chord order) a 4:

| Pattern | Orden de las notas |
|---|---|
| Up | 1 2 3 4, 1 2 3 4... |
| Down | 4 3 2 1... |
| Up-down | 1 2 3 4 3 2, 1 2 3 4 3 2... (sin repetir los extremos) |
| Up-down (repeat ends) | 1 2 3 4 4 3 2 1... |
| Converge | de afuera hacia adentro: 1 4 2 3... |
| Diverge | del medio hacia afuera: 3 2 4 1... |
| As played | el orden en que apretaste las teclas (con los neutrones agregados después), sea cual sea el Chord order |
| Random | cualquier nota del acorde en cada paso |
| Random walk | una nota más arriba o más abajo que la anterior, al azar |
| Chord | todas las notas del acorde juntas, en cada paso |
| Pedal | la primera nota alternando con cada una de las otras: 1 2 1 3 1 4... |
| Finger order | la nota que indica la fila Finger en cada paso |

Los patrones al azar son reproducibles: la misma Variation da las mismas elecciones.

El patrón es una secuencia de notas y los pasos son una secuencia de tiempo. Son independientes: un patrón de seis notas (Up-down sobre cuatro notas) en una capa de 16 pasos cae distinto en cada vuelta, y eso es otra fuente de desfasaje.

### Octavas, registro y corrimiento

**Octaves** (1 a 4) es cuántas octavas abarca el patrón. **Octave mode** elige cómo las recorre: `Stack` toca el patrón entero en la primera octava, después en la siguiente, y así; `Alternate` cambia de octava nota por nota; `Random` sortea la octava de cada nota. **Register** (-3 a +3) corre toda la capa por octavas: una capa de bajo en -2, un pluck agudo en +1. **Index shift** (0 a 15) arranca el patrón en otra nota del acorde, para que dos capas con el mismo patrón no toquen al unísono.

### Tiempo: figura, largo y swing

**Rate** es la duración de un paso: 1/4, 1/8, 1/16 o 1/32, recta, en tresillo (T) o con puntillo (D). **Rate x** la multiplica por 1 a 16, para capas muy lentas: 1/16 ×3 es un paso de tres semicorcheas. **Length** (1 a 32 pasos) es cuántos pasos forman un ciclo de la capa.

**Swing** (50 a 75 %) atrasa un paso de cada dos: en 50 % los pasos van rectos, en 66 % tienen sensación de tresillo, en 75 % un paso de cada dos se atrasa medio paso.

**Humanize** (0 a 30 ms) atrasa un poco cada paso, una cantidad al azar reproducible, nunca antes de la grilla. Como cada paso lee el acorde en su propio momento de la grilla, un acorde tocado justo en el tiempo nunca se pierde.

### Gate, velocidad y acento

**Gate** es la duración de cada nota como parte de su paso, de 5 a 100 %. **Velocity** elige de dónde sale la velocidad de la nota: `Fixed` (el valor de **Fixed vel.**, 100 por defecto), `Played` (la velocidad de la tecla que apretaste) o `Steps` (la fila Velocity). Un paso con acento suma encima el **Accent** de la página Harmony (24 por defecto).

### Varias capas en un canal

Con todas las capas en el canal 1 (el valor por defecto), dos capas pueden tocar la misma nota al mismo tiempo. Valence nunca manda una nota dos veces sin soltarla en el medio: si las dos atacan en el mismo momento, las dos notas se vuelven una, que dura lo que la más larga; si la nota ya estaba sonando, se suelta y se vuelve a atacar. La misma regla vale para los ratchets y las ligaduras.

## Polimetría y fusión

Cada capa cuenta sus propios pasos. Una capa de 16 pasos en 1/16 completa su ciclo cada compás; una de 15 lo completa una semicorchea antes, así que en cada compás arranca un paso más adelante. Las dos vuelven a arrancar juntas recién después de 16 × 15 = 240 semicorcheas, 15 compases. Con una tercera capa de 13 pasos, las tres arrancan juntas después de 3120 semicorcheas, 195 compases: más de seis minutos a 124 BPM.

Ese arranque común es la **fusión**. Valence la calcula exacta, a partir del largo y la figura de cada capa, apenas empieza a tocar, y cuenta hacia atrás: **fusion in** arriba a la derecha del átomo, y en la página Harmony, en compases de cuatro tiempos. Cuando llega, el átomo destella. La fusión es el mínimo común múltiplo de los ciclos de las capas, así que los largos primos (5, 7, 11, 13, 17, 19, 23, 29, 31) mantienen las capas separadas el mayor tiempo, y los largos que comparten un factor (16 y 12, por ejemplo) se encuentran antes.

Tres ajustes hacen que las capas vuelvan a coincidir antes:

- **New chord** (por capa, página Layers): con `Restart`, el valor por defecto, la capa vuelve a su primer paso cada vez que cambia el acorde. Con `Keep phase` sigue donde estaba. Para una polimetría libre a través de los cambios de acorde, poné todas las capas en `Keep phase`; para que cada acorde arranque en un tiempo fuerte nuevo, dejalo en `Restart`.
- **Phrase** (página Harmony): cada 1 a 16 compases todas las capas vuelven juntas al paso 1. `Off` es polimetría libre: sólo se realinean en la fusión.
- Arrancar el transporte del DAW: con Clock en Host cada capa toma el paso que le da su posición en la canción (ver Reloj y tempo), así el mismo compás toca siempre los mismos pasos.

Si algunas capas reinician con el acorde nuevo y otras conservan su fase, puede que no vuelvan a alinearse nunca: el contador dice entonces `no fusion ahead`.

## La página Layers

La página Layers muestra las cuatro capas arriba y, abajo, todos los ajustes y las filas de pasos de la capa elegida.

![La página Layers con Acid Shell: las cuatro capas como tiras arriba, la capa 1 elegida con sus ajustes y sus filas de pasos abajo.](img/layers.jpg)

### Las tiras de las capas

![Las tiras de las capas: On, un resumen y los pasos de cada capa, con el cursor.](img/strips.jpg)

Cada capa tiene una fila con su llave **On**, un resumen (largo, figura, patrón, y `E` con los pulsos cuando el generador euclidiano está prendido) y una tira con sus pasos, donde el marco blanco es el paso que suena ahora. Los pasos que pasan del largo de la capa son marcos vacíos. Un clic en la fila de una capa la elige para editarla abajo.

Podés editar los pasos directamente en la tira:

- Clic: prende o apaga un paso.
- Shift y clic: le pone acento (un punto arriba).
- Alt y clic: recorre su ratchet, de 1 a 4 (el número abajo).
- Cmd y clic (Ctrl y clic en Windows y Linux): lo liga al paso siguiente (una línea abajo).

La opacidad de un paso muestra su probabilidad. Cuando el generador euclidiano está prendido, decide él qué pasos suenan, y la tira muestra esos.

### Ajustes de la capa

![Los ajustes de la capa 1 en Acid Shell.](img/layersettings.jpg)

| Ajuste | Rango | Por defecto |
|---|---|---|
| Length | 1 a 32 pasos | 16, 15, 13 y 7 en las capas 1 a 4 |
| Rate | 1/4 a 1/32, recta, tresillo o puntillo | 1/16 en las capas 1 y 2, 1/8 en las capas 3 y 4 |
| Rate x | ×1 a ×16 | ×1 |
| Pattern | 12 patrones | Up, Up-down, Chord, Pedal |
| Octaves | 1 a 4 | 1 |
| Octave mode | Stack, Alternate, Random | Stack |
| Register | -3 a +3 octavas | 0, -1, 0, -2 |
| Index shift | 0 a 15 | 0 |
| Gate | 5 a 100 % | 50 % |
| Swing | 50 a 75 % | 50 % |
| Euclid | Steps, 1 a 32 pulsos | Steps |
| Rotate | 0 a 31 | 0 |
| Velocity | Fixed, Played, Steps | Fixed |
| Fixed vel. | 1 a 127 | 100 |
| Humanize | 0 a 30 ms | 0 ms |
| Mutate | 0 a 100 % | 0 % |
| New chord | Keep phase, Restart | Restart |
| CC row | Off, CC 0 a CC 119 | Off |
| Channel | 1 a 16 | el número de la capa |
| Preview voice | Pluck, Bass, Stab, Sub | Pluck, Bass, Stab, Sub |

Las capas 1 y 2 vienen prendidas, la 3 y la 4 apagadas. **Channel** se usa cuando Output channels está en `Per layer` (página Outputs). **Preview voice** sólo cambia el sonido de prueba incorporado, no el MIDI. Los demás ajustes se explican en Cómo toca Valence, Filas de pasos y Mutación.

### Filas de pasos

![Las filas de pasos de la capa 1 en Acid Shell: acentos, dos ligaduras, un ratchet de 3 en el último paso, saltos de octava y una fila de CC.](img/steprows.jpg)

Debajo de los ajustes, cada fila de la capa elegida está desplegada como una tira de 32 pasos; los números de paso van arriba y los pasos que pasan del largo de la capa aparecen atenuados. Una fila que no hace nada con los ajustes actuales se dibuja tenue, y su línea de ayuda dice por qué.

| Fila | Valores | Qué hace |
|---|---|---|
| On | prendido, apagado | si el paso suena |
| Velocity | 1 a 127 | la velocidad del paso, cuando la Velocity de la capa es `Steps` |
| Accent | prendido, apagado | suma el Accent (página Harmony) a la velocidad |
| Tie | prendido, apagado | sostiene la nota hasta el paso siguiente |
| Ratchet | 1 a 4 | notas dentro del paso |
| Probability | 0 a 100 % | la probabilidad de que el paso suene |
| Octave | -2 a +2 | octava de este paso, encima del Register de la capa |
| Note | -, o una nota de la escala | una nota fija en lugar de la del patrón |
| CC | nada, 0 a 127 | valor que se manda con el número de CC row de la capa |
| Finger | 1 a 16 | qué nota del acorde toca el paso, con el patrón Finger order |

Para editar:

- On, Accent, Tie: clic en un paso, o arrastrá sobre varios para pintarlos.
- Velocity, Probability, CC: clic a la altura que quieras, o arrastrá para dibujar una curva.
- Ratchet, Octave, Note, Finger: arrastrá hacia arriba o hacia abajo sobre un paso, o usá la rueda del mouse.
- El doble clic devuelve un paso a su valor por defecto.
- Clic derecho en una fila abre su menú: **Reset the row**, **Fill with step N's value**, **Rotate left**, **Rotate right** (dentro del largo de la capa).

Qué hace cada fila:

- **Tie** sostiene la nota hasta que arranca el paso siguiente. Si el paso siguiente toca la misma nota, la nota sigue sonando, así que una ligadura sobre varios pasos hace una sola nota larga. Si toca otra nota, la nota nueva arranca antes de que termine la vieja: un legato que los sintes con glide o con modo slide tocan como un slide.
- **Ratchet** divide el paso en 2, 3 o 4 notas iguales, cada una con el gate de la capa: un redoble rápido al final de una frase.
- **Probability** se sortea una vez por paso a partir de la Variation, así que un paso en 50 % suena en algunas vueltas y en otras no, igual cada vez que tocás lo mismo.
- **Note** reemplaza la nota del patrón por una nota fija de la escala, contada desde la tónica (se muestra como nombre de nota, con un apóstrofo por cada octava para arriba). Sirve para una nota pedal o un riff que ignora el acorde. Las notas fijas no hacen avanzar el patrón.
- **CC** manda un valor de controlador al principio del paso, con el número elegido en el ajuste **CC row** de la capa (por ejemplo 74, que suele ser el corte del filtro), por el canal de la capa. Un valor se manda sólo cuando cambia. Abajo de todo de la barra no se manda nada. En el sonido de prueba, el CC 74 abre el filtro de la voz de esa capa.
- **Finger** indica, en cada paso, qué nota del acorde tocar (1 es la primera en el Chord order; con Low to high, la más grave). Sólo se usa con el patrón `Finger order`: una melodía que sigue la forma del acorde.

### El generador euclidiano

Con **Euclid** en `Steps`, la fila On decide qué pasos suenan. Con un número de pulsos, Valence reparte esos pulsos de la manera más pareja posible sobre el largo de la capa e ignora la fila On: 3 pulsos sobre 8 pasos dan el clásico tres contra ocho, 5 o 7 sobre 16 un patrón que rueda. **Rotate** gira el patrón: corre su primer pulso sin cambiar cuántos hay. Las demás filas (velocidad, acento, ratchet y el resto) siguen valiendo para los pasos que suenan. La macro Density trabaja con este generador.

### Mutación

Con **Mutate** por encima de 0 %, una capa cambia un poco al principio de cada ciclo, desde el segundo: la probabilidad de un paso sube o baja, su octava se mueve, un paso se prende o se apaga (o se vuelve menos probable, cuando el generador euclidiano decide los pasos), su velocidad se mueve, o aparece o se va un ratchet de 2. Cuanto más alto el valor, más seguido y más cambios. Los cambios se acumulan de ciclo en ciclo mientras Valence toca; cada arranque nuevo (una mano nueva con el transporte parado, el transporte que arranca, un preset nuevo) vuelve a empezar desde las filas como las dibujaste, y lo mismo pasa al editar una fila.

Cada capa muta con su propia secuencia al azar, sembrada por la Variation: prender la mutación en una capa no cambia las notas de las otras, y la misma Variation muta igual cada vez.

## La página Harmony

![La página Harmony con Auto Orbit: entrada y armonía, tiempo y groove, y el núcleo en un teclado.](img/harmony.jpg)

### Entrada y armonía

| Ajuste | Rango | Por defecto |
|---|---|---|
| Harmony | Played, Auto | Played |
| Progression | Walk, Two chords, Drone | Walk |
| Chord length | 1 a 16 compases | 2 compases |
| Complete | Off, Triad, Seventh, Ninth, Octaves | Off |
| Chord order | As played, Low to high, High to low, Random | Low to high |
| Latch | Off, On | Off |
| Key | C a B | A |
| Scale | 16 escalas | Natural minor |
| Transpose | -24 a +24 semitonos | 0 |

Progression y Chord length valen sólo para la armonía automática. Las 16 escalas son Major (Ionian), Natural minor, Dorian, Phrygian, Lydian, Mixolydian, Locrian, Harmonic minor, Phrygian dominant, Double harmonic, Major pentatonic, Minor pentatonic, Hirajoshi, In-sen, Whole tone y Lydian augmented. Key y Scale no cambian las notas que apretás: importan para Complete, la armonía automática y la fila Note.

### Tiempo y groove

| Ajuste | Rango | Por defecto |
|---|---|---|
| Clock | Free, Host | Host |
| Tempo | 40 a 240 BPM | 122 |
| Phrase | Off, 1 a 16 compases | Off |
| Variation | 1 a 9999 | 1 |
| Accent | 0 a 60 | 24 |

**New variation** elige otro número de Variation al azar: otras elecciones al azar con los mismos ajustes.

### El núcleo

El panel Nucleus muestra el acorde actual por su nombre, los protones y los neutrones, la línea de la fusión (`Fusion (all layers back at step 0 together) in N bars`), y un teclado de C1 a C7 con los protones en color cálido y los neutrones en frío.

### Reloj y tempo

Con Clock en `Host` y el transporte del DAW andando, Valence sigue el tempo y la posición de la canción del DAW, y su propio Tempo se ignora. Cada capa toma entonces el paso que le da su posición en la canción (o la posición desde el último arranque de Phrase), así el mismo compás de la canción toca siempre los mismos pasos, en la reproducción, en un lazo y en un render offline. Arrancar el transporte es lo mismo que apretar Play, y pararlo es lo mismo que Stop. Cuando el transporte salta (movés la posición o un lazo vuelve al principio), Valence suelta sus notas y sigue desde la posición nueva, así cada vuelta de un lazo suena igual.

Con el transporte parado, o con Clock en `Free`, una mano nueva arranca las capas desde su primer paso en el momento en que la tocás, y paran cuando soltás. Con `Free` Valence usa siempre su propio Tempo; con `Host` y el transporte parado usa el tempo del DAW.

Un acorde que llega después de que arrancó un paso se escucha desde el paso siguiente. Los clips MIDI del DAW caen sobre la grilla, así que nunca llegan tarde; cuando tocás a mano, un acorde tocado apenas tarde entra en el paso siguiente.

### Determinismo

Cada elección al azar (los patrones Random y Random walk, la probabilidad, la mutación, el humanizado, el Chord order Random y la armonía automática) sale del número de Variation. La misma Variation, los mismos ajustes y la misma entrada tocan las mismas notas, en los mismos momentos, en cualquier DAW, con cualquier tamaño de buffer y frecuencia de muestreo, y en un render offline.

## La página Outputs

![La página Outputs: un puerto y un canal por capa, y las opciones.](img/outputs.jpg)

### Salidas MIDI directas

Cada capa va siempre al DAW. Además, cada capa puede ir directo a un puerto MIDI:

- Sintes físicos: su puerto USB o el puerto de tu interfaz MIDI.
- Otras pistas u otros programas: un puerto virtual (IAC Driver en macOS, loopMIDI en Windows).

Cada fila muestra la capa, un menú de puertos y el **Channel** de la capa (el mismo ajuste que en la página Layers). `DAW only` quiere decir sin puerto directo. **Rescan ports** vuelve a leer la lista de puertos MIDI; Valence también se da cuenta sola, en unos segundos, cuando aparece un puerto. Si al abrir un proyecto falta un puerto elegido, el menú dice `Not connected` con el nombre del puerto, y se vuelve a conectar cuando el puerto está de nuevo (Rescan ports). Los puertos se guardan con el proyecto del DAW, no con los presets.

### Opciones

| Ajuste | Rango | Por defecto |
|---|---|---|
| Output channels | All on 1, Per layer | All on 1 |
| MIDI clock out | Off, On | Off |
| Link | Off, Lead, Follow | Off |
| Preview sound | Off, On | On |
| Preview volume | 0 a 100 % | 60 % |
| Preview space | 0 a 100 % | 35 % |
| Layers as MIDI ports | Off, On (macOS y Linux) | Off |

**Output channels**: `All on 1` manda todas las capas por el canal MIDI 1, que anda con cualquier sinte. `Per layer` manda cada capa por su propio Channel, para un sinte multitímbrico o un ruteo que separa canales. Vale igual para el DAW, los puertos directos y los puertos virtuales.

**MIDI clock out** manda reloj MIDI (24 pulsos por negra) más Start y Stop a los puertos directos, para que cajas de ritmos, secuenciadores y delays sincronizados de tus equipos sigan a Valence.

**Link** conecta varias instancias de Valence dentro de la misma aplicación host. La que está en `Lead` comparte su acorde; las que están en `Follow` tocan sus propias capas sobre el acorde de la líder e ignoran su propia entrada. Usá una sola Lead. Es un vínculo propio de Valence: funciona entre instancias dentro de la misma aplicación host, no entre aplicaciones ni entre computadoras.

**Preview sound** es el sinte incorporado, que también se prende y apaga con el botón al lado de Play. Cada capa tiene su voz (la que elegís con Preview voice en la página Layers) y su lugar en el estéreo, con un delay corto y una reverb cuya cantidad es **Preview space**. **Preview volume** ajusta su nivel. Apagalo cuando toquen tus sintes: el MIDI es el mismo de las dos maneras. La versión MIDI FX de Logic no tiene sonido de prueba.

**Layers as MIDI ports** (macOS y Linux) publica cada capa como un puerto MIDI virtual, de `Valence Layer 1` a `Valence Layer 4`, que otras pistas y otros programas pueden elegir como entrada; una segunda Valence que también publica usa `Valence 2 Layer 1` y así. Los puertos desaparecen cuando apagás la opción, sacás esa Valence o cerrás el proyecto, y cada uno recibe un All Notes Off antes de cerrarse. Windows no tiene puertos MIDI virtuales propios, así que ahí la opción no aparece: creá un puerto con loopMIDI y elegilo en las salidas MIDI directas.

## Captura

**Capture MIDI** guarda como archivo MIDI lo que Valence acaba de tocar. Hacé clic y elegí `Last minute`, `Last 2 minutes` o `Last 5 minutes`. El archivo se guarda en una carpeta `Valence` dentro de tu carpeta de Música, con la fecha y la hora en el nombre, y el pie muestra dónde. Después, una caja a la derecha del pie dice "Drag ... into your DAW": arrastrala a una pista para usar el pasaje como MIDI común.

El archivo tiene una pista por capa, con los nombres `Layer 1` a `Layer 4`, con las notas de cada capa en su canal de salida, sus velocidades y su tiempo, y el tempo al que tocaba Valence. No guarda la fila de CC. Valence guarda las últimas 8192 notas, así que un preset muy cargado puede dar menos de cinco minutos. Si todavía no sonó nada, el pie dice "Nothing to capture yet: hold a chord first".

## Presets

Valence trae 43 presets de fábrica en siete grupos. Los presets cambian los ajustes musicales: cada capa con sus filas de pasos, la armonía, el tempo y la variación. No cambian Play, Clock, Output channels, los ajustes del sonido de prueba, MIDI clock out, Link, Layers as MIDI ports ni los puertos directos, así que cambiar de preset nunca rompe tu ruteo. Cargar un preset mientras Valence toca vuelve a arrancar la música desde el acorde que tenés apretado.

Una Valence nueva arranca con el primer preset, Phase Drift (lo que los hosts muestran como su primer programa). El tempo de un preset importa sólo con Clock en Free o con el DAW parado.

| Preset | Grupo | Tonalidad y escala | Tempo | Capas (largo × figura, patrón) |
|---|---|---|---|---|
| Phase Drift | Minimal Techno | A menor natural | 128 | 16 × 1/16 up-down, 15 × 1/16 as played, 12 × 1/16 random |
| Prime Lattice | Minimal Techno | D frigio | 127 | 13 × 1/16 up, 11 × 1/16 down, 7 × 1/16 pedal |
| Concrete Pulse | Minimal Techno | F menor natural | 129 | 16 × 1/16 random, 16 × 1/16 up, 8 × 1/8 pedal |
| Hollow Loop | Minimal Techno | C dórico | 126 | 16 × 1/16 pedal, 14 × 1/16 up-down |
| Tilt Shift | Minimal Techno | G menor natural | 128 | 16 × 1/16 up, 16 × 1/16 up, 9 × 1/8 random walk |
| Static Bloom | Minimal Techno | Eb menor natural | 130 | 16 × 1/16 up-down (repeat ends), 15 × 1/16 random |
| Bubble | Minimal House | G menor natural | 122 | 16 × 1/16 pedal, 12 × 1/8T up |
| Jack Groove | Minimal House | E menor natural | 124 | 16 × 1/16 pedal, 8 × 1/8 up-down |
| Late Shuffle | Minimal House | A menor natural | 120 | 16 × 1/16 up, 12 × 1/8 pedal |
| Velvet Keys | Minimal House | Bb mayor | 123 | 8 × 1/8 chord, 13 × 1/16 diverge |
| Warm Floor | Minimal House | E menor natural | 122 | 16 × 1/16 pedal, 16 × 1/16 up-down, 6 × 1/8T random |
| Sunday Loop | Minimal House | G mixolidio | 121 | 12 × 1/16 up, 16 × 1/16 pedal |
| Liquid Rolls | Drum and Bass | C mayor | 174 | 7 × 1/16 up-down, 11 × 1/8T random |
| Reese Engine | Drum and Bass | F menor natural | 172 | 16 × 1/16 pedal, 5 × 1/16 chord |
| Halftime | Drum and Bass | D menor natural | 172 | 23 × 1/8 random walk, 29 × 1/8 up |
| Neuro Steps | Drum and Bass | D frigio dominante | 174 | 16 × 1/32 random, 9 × 1/16 chord |
| Roller | Drum and Bass | A menor natural | 175 | 16 × 1/16 pedal, 6 × 1/16 up |
| Atmos Drift | Drum and Bass | Bb dórico | 170 | 13 × 1/8T up-down, 11 × 1/16 random, 4 × 1/4 chord |
| Rubber Line | Acid | A menor natural | 130 | 16 × 1/16 random walk |
| Acid Shell | Acid | A menor natural | 130 | 16 × 1/16 pedal, 7 × 1/16 chord |
| Squelch Walk | Acid | E frigio | 132 | 16 × 1/16 random, 7 × 1/16 chord |
| Acid Ratchet | Acid | G menor natural | 130 | 16 × 1/16 up-down |
| Tension Wire | Acid | C frigio dominante | 134 | 15 × 1/16 converge, 16 × 1/16 pedal |
| Resonant Steps | Acid | D dórico | 128 | 16 × 1/16 finger order |
| Night Glide | Acid | F menor natural | 128 | 12 × 1/16 up, 4 × 1/4 pedal |
| Dub Chord | Dub | C menor natural | 124 | 16 × 1/8 chord, 4 × 1/4 finger order |
| Echo Chamber | Dub | G dórico | 120 | 11 × 1/8D chord, 16 × 1/16 up |
| Deep Stab | Dub | C menor natural | 122 | 8 × 1/8 chord, 8 × 1/8 pedal |
| Fog Lights | Dub | F menor natural | 118 | 12 × 1/8T chord, 8 × 1/4 pedal |
| Sub Pressure | Dub | A frigio | 126 | 4 × 1/4 pedal, 6 × 1/8 chord |
| Steam Room | Dub | Eb dórico | 121 | 10 × 1/8 chord, 7 × 1/16 random |
| Auto Orbit | Ambient | D dórico | 84 | Armonía automática; 13 × 1/8 up-down, 11 × 1/8T random, 7 × 1/4 chord, 5 × 1/4 pedal |
| Glass Cycles | Ambient | E lidio | 76 | 17 × 1/8 converge, 19 × 1/8 diverge |
| Slow Bells | Ambient | C pentatónica mayor | 70 | 7 × 1/4 random, 11 × 1/4T random walk |
| Cloud Chamber | Ambient | A pentatónica menor | 90 | 29 × 1/16 random, 23 × 1/8 random walk |
| Long Tide | Ambient | F mayor | 66 | 31 × 1/8 up-down (repeat ends), 3 × 1/4 chord |
| Polar Night | Ambient | B frigio | 72 | Armonía automática; 19 × 1/8T up-down, 13 × 1/4 random, 8 × 1/4 pedal |
| Simple Up | Classics | C mayor | 120 | 16 × 1/16 up |
| Simple Down | Classics | C mayor | 120 | 16 × 1/16 down |
| Up and Down | Classics | C mayor | 120 | 16 × 1/16 up-down |
| Two Octaves Up | Classics | C mayor | 120 | 16 × 1/16 up sobre dos octavas |
| Random Notes | Classics | C mayor | 120 | 16 × 1/16 random |
| Chord Stabs | Classics | C mayor | 120 | 8 × 1/8 chord |

Auto Orbit y Polar Night tocan solos con la armonía automática: apretá Play. Todos los demás esperan tu acorde; la tonalidad y la escala de un preset importan para Complete y las notas fijas, así que con Complete apagado los podés tocar en cualquier tonalidad.

Para guardar los tuyos, apretá **Save**, escribí un nombre y confirmá. Los presets de usuario aparecen bajo User en el menú de presets, y las flechas los recorren después de los de fábrica. Son archivos en `~/Library/Application Support/Audio Tools/Valence/Presets` en macOS, en `Audio Tools\Valence\Presets` dentro de tu carpeta AppData\Roaming en Windows, y en `~/.config/Audio Tools/Valence/Presets` en Linux. Guardar con un nombre que ya existe reemplaza ese preset. Para borrar uno, borrá su archivo; desaparece la próxima vez que se abre Valence.

## Ruteo a los sintes

### Cada capa a su sinte

Las capas pueden llegar a sintes distintos de cuatro maneras; las dos primeras están explicadas paso a paso en En Ableton Live:

- **Output channels en Per layer y un sinte multitímbrico**: una pista, un sinte, un sonido distinto en cada canal.
- **Layers as MIDI ports** (macOS y Linux): cada pista de sinte toma un puerto `Valence Layer` como entrada.
- **Output channels en Per layer y un DAW que filtra por canal**: en REAPER, un envío por sinte con el canal de la capa; en otros DAW, la entrada MIDI de la pista del sinte en la pista de Valence y en el canal de la capa.
- **Salidas directas a un puerto virtual**: dales a todas las capas el mismo puerto virtual en la página Outputs, poné Output channels en `Per layer`, y en cada pista de sinte elegí ese puerto como entrada con el canal de la capa.

### Sintes físicos

1. Conectá los sintes por USB o con una interfaz MIDI.
2. En la página Outputs, elegí el puerto de cada sinte para la capa que lo tiene que tocar. Para varias capas en un sinte multitímbrico o en una cadena de sintes sobre un mismo cable, dales el mismo puerto, poné Output channels en `Per layer` y poné cada sinte a recibir por el canal de su capa.
3. Prendé **MIDI clock out** si una caja de ritmos, un secuenciador o un delay de tus equipos tiene que seguir el tempo de Valence.
4. Apagá Preview sound.

El tráfico entra de sobra en un cable MIDI: hasta una capa en 1/32 con ratchet de 4 a 180 BPM y un CC en cada paso manda unos 216 mensajes por segundo, y un cable de 5 pines lleva unos mil.

### La aplicación independiente

La aplicación independiente toca los puertos directos, con MIDI clock out si lo querés, y el sonido de prueba por tu placa de audio. El botón Options de la ventana abre los ajustes de audio y MIDI, donde elegís tu teclado como entrada MIDI. Sin host no hay transporte, así que Valence usa su propio Tempo.

## Recetas

### Minimal techno: tres capas que se desfasan

1. Cargá Phase Drift, o arrancá desde Simple Up.
2. Prendé tres capas en 1/16 con largos de 16, 15 y 12 (o 16, 15 y 13 para un desfasaje mucho más largo).
3. Dales patrones distintos: Up-down, As played, Random.
4. Poné el Swing entre 50 y 54 % con la macro y el Gate en alrededor de 30 %.
5. En la página Layers, poné New chord en `Keep phase` en todas las capas.
6. Apretá un acorde menor, o una sola nota con Complete en Seventh.

Mirá el contador de la fusión: con 16, 15 y 12 las capas se encuentran cada 15 compases.

### Un filtro que nunca cae igual sobre la misma nota

1. Arrancá desde cualquier preset de dos capas con Output channels en `All on 1` (el valor por defecto).
2. Prendé una tercera capa y dale un largo que las otras no compartan, por ejemplo 12 u 11.
3. En esa capa, apagá todos los pasos de la fila On, poné CC row en 74 y dibujá en la fila CC una curva lenta entre 40 y 90, más o menos.

Esa capa no toca notas, sólo la curva del filtro, por el mismo canal que las demás: el filtro se mueve con su propio ciclo, así que nunca cae igual sobre la misma nota.

### Minimal house: swing y notas fantasma

1. Cargá Jack Groove o Bubble.
2. Subí el Swing a 56 a 60 %.
3. En la capa de bajo (Pattern Pedal), poné Velocity en `Steps` y dibujá velocidades bajas (30 a 45) con una Probability de alrededor de 40 % en cuatro o cinco pasos débiles: notas fantasma.
4. Agregá una capa en 1/8T con Up e Index shift 1 para una línea de marimba.

### Drum and bass: ciclos cortos y redobles

1. Cargá Liquid Rolls o Roller (172 a 175 BPM).
2. Dejá una capa corta de 7 en 1/16 y otra de 11 en 1/8T, las dos casi rectas (Swing 50 a 54 %).
3. Poné un ratchet de 2 a 4 en el último paso de una capa para un redoble al final de cada ciclo.
4. Para halftime, cargá Halftime: dos capas lentas de 23 y 29 corcheas que tardan 667 pasos en encontrarse.

### Acid: acentos y slides

1. Cargá Acid Shell o Rubber Line.
2. En una capa de 16 pasos con Pattern `Pedal` o `Random walk`, poné acento en tres o cuatro pasos.
3. Ligá dos o tres pasos al siguiente: con un sinte que hace glide sobre notas superpuestas, se deslizan.
4. Poné un ratchet de 3 en el paso 16.
5. Poné CC row en 74 y abrí un poco más el filtro en los pasos con acento.

### Dub: stabs de acordes

1. Cargá Dub Chord o Deep Stab.
2. Usá Pattern `Chord` en 1/8 con Gate de 15 a 30 %, para que cada stab sea corto.
3. Bajá la Probability a alrededor de 60 % en la mayoría de los pasos y poné un Mutate bajo (10 a 20 %): los stabs se corren despacio de compás en compás.
4. Poné un delay de corchea con puntillo en el sinte: el delay les da cuerpo a los stabs.

### Guardar un pasaje

1. Cuando Valence toque algo que te guste, hacé clic en Capture MIDI y elegí cuánto para atrás.
2. Arrastrá el archivo del pie a una pista MIDI de tu DAW.
3. Cada capa llega en su propia pista, lista para editar o para tocar con otros sonidos.

Anotá también el número de Variation: con los mismos ajustes y los mismos acordes, Valence vuelve a tocar lo mismo.

## Problemas y soluciones

**No suena nada.** Valence hace MIDI, no audio. O la ruteás a un instrumento (ver Cargarlo en un DAW), o hacés clic en **Preview sound** en la barra superior hasta que diga `Preview sound: On`, con Preview volume arriba en la página Outputs. Después apretá un acorde.

**Aprieto Play y no pasa nada.** Con Harmony en `Played`, Valence toca cuando tenés un acorde apretado; Play sólo arranca la armonía automática. Apretá un acorde, o poné Harmony en `Auto`.

**Se para cuando suelto.** Prendé **Latch**, o sostené el pedal de sustain.

**Escucho un sonido, pero no es mi sinte.** Es el sonido de prueba de Valence: el botón al lado de Play dice `Preview sound: On`. Hacé clic para apagarlo.

**En Ableton Live todos los sintes tocan todas las capas.** Live junta los canales entre pistas. Usá Output channels en `Per layer` con un sinte multitímbrico, o Layers as MIDI ports, o un puerto virtual (ver En Ableton Live).

**En Ableton Live (o en REAPER) no llega MIDI a las otras pistas.** Estás usando el Audio Unit. Cargá el VST3.

**No encuentro Valence en la ranura MIDI FX de Logic.** Elegí `Audio Tools Valence MIDI FX`, en Audio Units, Audio Tools. La versión instrumento, `Audio Tools Valence`, va en cambio en la ranura de instrumento; para tocar otro instrumento desde Valence, usá el MIDI FX.

**Las capas vuelven al paso 1 con cada acorde.** Es New chord en `Restart` (el valor por defecto). Ponelo en `Keep phase` en cada capa para una polimetría libre.

**El contador de la fusión dice no fusion ahead.** Algunas capas reiniciaron con un acorde nuevo y otras conservaron su fase, así que ya no pueden alinearse. Poné New chord igual en todas las capas, o poné un Phrase.

**Hago clic en los pasos de la fila On y no cambia nada.** El generador euclidiano de la capa está prendido y decide los pasos. Poné Euclid en `Steps`.

**La fila Velocity no hace nada.** La Velocity de la capa no está en `Steps`. Mientras no hace nada, la fila se dibuja tenue.

**La fila CC no manda nada.** Elegí un número de controlador en el ajuste CC row de la capa, y dibujá valores por encima del fondo de la barra.

**La fila Finger no hace nada.** Sólo funciona con el patrón `Finger order`.

**Complete no agrega nada.** Triad, Seventh y Ninth actúan sólo con una o dos notas apretadas. Con tres o más, usá Octaves, o tocá menos notas.

**Parece que dos capas se cortan las notas.** Con todas las capas en el canal 1, la misma nota de dos capas se funde o se vuelve a atacar, nunca se duplica. Poné Output channels en `Per layer` para separarlas.

**Un acorde que toco entra un paso tarde.** Un acorde que llega después de que arrancó un paso suena desde el paso siguiente. Tocá apenas antes del tiempo, o cuantizá el clip.

**El ajuste Tempo no hace nada.** Con Clock en Host y el DAW tocando, Valence sigue el tempo del DAW. Poné Clock en Free para usar el Tempo propio de Valence.

**Valence arranca cuando aprieto Play en el DAW.** Con Clock en Host, arrancar el transporte es Play y pararlo es Stop. Poné Clock en Free si Valence tiene que ignorar el transporte.

**Los puertos de Valence no aparecen en Live.** Primero prendé Layers as MIDI ports, después abrí Settings de Live, Link, Tempo & MIDI, y prendé Track en las entradas `Valence Layer`. En Windows no hay puertos virtuales: usá loopMIDI.

**Un puerto no está en la lista.** Hacé clic en Rescan ports. En macOS, fijate que el IAC Driver tenga tildado "Device is online"; en Windows, que loopMIDI esté corriendo.

**Hay un asterisco después del nombre del preset.** Cambiaste un ajuste o un paso desde que se cargó el preset. Guardalo como preset de usuario para conservarlo.

**Quedan notas colgadas.** Hacé clic en All notes off. Suelta todo y manda All Notes Off en los 16 canales, en el DAW y en los puertos directos.

## Especificaciones

| Ítem | Valor |
|---|---|
| Formatos | VST3, Audio Unit y Audio Unit MIDI FX (macOS), LV2 (Linux), aplicación independiente |
| Sistemas | macOS 11 o posterior (universal), Windows 10 u 11 (64 bits), Linux (64 bits) |
| Capas | 4, cada una con su largo, figura, patrón, octavas, gate, swing y canal MIDI |
| Pasos | 1 a 32 por capa, con filas de encendido, velocidad, acento, ligadura, ratchet 1-4, probabilidad, octava, nota fija, CC y dedo |
| Figuras | 1/4 a 1/32, rectas, en tresillo y con puntillo, por 1 a 16 |
| Patrones | 12: up, down, up-down con y sin extremos repetidos, converge, diverge, as played, random, random walk, chord, pedal, finger order |
| Armonía | el acorde que apretás, completado a tríada, séptima o novena de 16 escalas, o una sucesión de acordes propia |
| Ritmo | generador euclidiano, swing de 50 a 75 %, humanizado hasta 30 ms, reinicio de frase cada 1 a 16 compases |
| Tempo | 40 a 240 BPM, o el del host |
| Salidas | MIDI al DAW en un canal o uno por capa, un puerto MIDI directo por capa, reloj MIDI, vínculo Lead y Follow, un puerto MIDI virtual por capa (macOS, Linux), sonido de prueba |
| Captura | Último 1, 2 o 5 minutos a un archivo MIDI, una pista por capa |
| Presets | 43 en siete grupos, más los tuyos |
| Salida de audio | Estéreo (sólo el sonido de prueba; ninguna en el MIDI FX) |

Medido sobre el motor:

| Prueba | Resultado |
|---|---|
| Misma entrada, misma variación | Mensajes idénticos a la muestra con buffers de 64 a 1024 muestras; los mismos mensajes en orden a 44,1 y 96 kHz; 169.529 de 169.529 en 20 minutos con 15 combinaciones de host |
| Grilla | 16.518 notas en la muestra exacta de su paso, a tres tempos, con swing, tresillos y puntillos |
| Polimetría | Capas de 16, 15 y 13 pasos se realinean en el paso 3120, previsto desde el principio |
| Complete | 79.192 notas en 5 escalas durante 50 minutos: ninguna fuera de la escala |
| Notas colgadas | Ninguna después de soltar, apagar el latch, parar el transporte, bypass, cambio de preset, capa apagada, cambio de canal, All notes off, CC 123 o el pedal |
| Tráfico MIDI | Una capa en 1/32, ratchet 4, 180 BPM, CC en cada paso: 216 mensajes por segundo en su canal |

CPU, como porcentaje de un núcleo:

| Ajuste | Apple M4 | AMD Ryzen 9 5900XT |
|---|---|---|
| Motor solo, cuatro capas ocupadas | 0,0037 % | 0,0083 % |
