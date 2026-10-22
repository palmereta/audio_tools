# Manual de Thrum

## Qué es Thrum

Thrum es una guitarra eléctrica para tu teclado y tu DAW, hecha por modelado físico: sin samples. Cada cuerda es una cuerda que vibra, simulada mientras suena, con su calibre, su tensión y su rigidez; las pastillas la escuchan a su distancia real del puente, a través de los potes de volumen y tono y del cable; y un guitarrista dentro del plugin decide en qué cuerda y en qué traste va cada nota, rasguea los acordes cuerda por cuerda, puntea con los dedos y toca las articulaciones. Después de la guitarra vienen una pedalera que podés reordenar, cuatro amplificadores y un gabinete con micrófonos que se mueven.

Como no hay nada grabado, la misma nota nunca suena dos veces igual: la púa cae un poco distinto cada vez, el rasgueo se abre distinto, la afinación se mueve unos cents con la fuerza de la púa. Y todo sigue siendo editable: cambiás la guitarra, las cuerdas o la afinación y cada nota acompaña.

Thrum tiene seis guitarras, todas descriptas de forma genérica: una sólida con tres single-coil, una sólida con dos humbuckers, una semi-hollow, una hollow de jazz, una barítono y una de siete cuerdas. Toca melodías (con hammer-ons, pull-offs, slides, bends y vibrato), acordes armados como los armaría un guitarrista, rasgueos sincronizados con tu tema y patrones de fingerpicking.

## Instalación

Bajá el paquete de tu sistema desde la web y descomprimilo. El launcher de Audio Tools también lo puede bajar e instalar por vos.

En macOS:

1. Copiá `Thrum.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `Thrum.component` (Audio Unit) a `~/Library/Audio/Plug-Ins/Components/`.
2. Reescaneá los plugins en el DAW. Thrum aparece como **Audio Tools Thrum**, un instrumento.
3. `Thrum.app` es la versión standalone y `thrumrender` el renderizador de línea de comandos.

Los archivos no están notarizados. Si macOS no los deja abrir, corré `xattr -dr com.apple.quarantine` seguido de la ruta de cada archivo.

En Windows:

1. Copiá `Thrum.vst3` a `C:\Program Files\Common Files\VST3\`.
2. Reescaneá los plugins en el DAW.
3. `Thrum.exe` es la versión standalone y `thrumrender.exe` el renderizador de línea de comandos.

En Linux:

1. Copiá `Thrum.vst3` a `~/.vst3/` y `Thrum.lv2` a `~/.lv2/`.
2. Reescaneá los plugins en el DAW. `Thrum` es la versión standalone (ALSA o JACK).

## Primeros sonidos en un minuto

1. Poné Thrum en una pista de instrumento y tocá algunas notas sueltas: el preset de arranque es una guitarra limpia en modo melodía.
2. Apretá un acorde con tres o cuatro teclas. En modo melodía las notas se ubican juntas en cuerdas distintas y se puntean de grave a agudo, como un rasgueo rápido.
3. Abrí el menú de presets y elegí **Campfire Strum** (en Strummed Chords). Dale play al DAW y mantené un acorde: Thrum lo rasguea a tiempo con tu tema. Cambiá de acorde y te sigue.
4. Elegí **Blues Lead Bends** y tocá una melodía con un dedo: las notas que se pisan se vuelven hammer-ons y pull-offs, y las notas largas toman vibrato al rato.
5. Elegí **Modern Chug** y tocá notas graves: alta ganancia con palm mute. Mové la rueda de pitch bend para un bend.

> Thrum responde a cómo tocás: la velocity es la fuerza de la púa, las notas que se pisan son ligados, y las teclas por debajo de la nota más grave de la guitarra son keyswitches que cambian la articulación.

## La interfaz de un vistazo

La barra de arriba tiene el menú de presets con los botones de anterior y siguiente, los botones de las páginas, **Inspect** y **Manual**, que abre este manual dentro del plugin.

![Thrum con Campfire Strum: arriba los presets, las páginas, Inspect y Manual; el diapasón en vivo a todo el ancho; abajo la página Guitar.](img/overview.jpg)

Debajo, a todo el ancho de la ventana, está el diapasón de la guitarra elegida. Muestra en vivo lo que hace el guitarrista de Thrum: un punto en cada cuerda y traste que suena, la cuerda vibrando mientras suena, las cuerdas al aire marcadas en la cejuela y la cejilla cuando hay. Es la forma más rápida de entender por qué una nota suena como suena: el mismo Mi puede ser una cuerda al aire, el quinto traste de la segunda o el noveno de la tercera, y cada uno tiene su color. Debajo del diapasón, Thrum muestra el acorde que reconoció, el modo, la articulación y dónde está la mano.

![El diapasón con Inspect encendido: el dedo en cada cuerda y traste, las cuerdas que suenan vibrando y el nombre de cada nota.](img/fretboard.jpg)

**Inspect** enciende la vista técnica, dibujada en cian sobre cada página: el nombre de cada nota que suena en el diapasón, dónde escucha cada pastilla en la guitarra (medido desde el puente) y cuáles enciende el selector, el camino de la señal y los ecos del delay en la página Pedals, y la respuesta en frecuencia del gabinete y los micrófonos en la página Amp / Cab. Apagalo para la vista simple.

![Inspect en la página Guitar: dónde escucha cada pastilla, medido desde el puente, cuáles enciende el selector, y la escala.](img/guitar-inspect.jpg)

Las páginas:

- **Guitar**: el instrumento, sus cuerdas, su afinación, las pastillas y los ruidos de la mano.
- **Play**: cómo toca el guitarrista: el modo, las articulaciones, los ligados, el vibrato y los bends, los acordes, los patrones de rasgueo y de punteo, la digitación y la variación humana.
- **Pedals**: la pedalera. Hacé clic en el pulsador de un pedal para encenderlo y arrastrá un pedal para moverlo en la cadena.
- **Amp / Cab**: el amplificador y el gabinete, con los micrófonos que podés mover frente al parlante.
- **Settings**: entrada y salida MIDI, keyswitches, los niveles de salida y la salida DI.

## La guitarra

### Tipos de guitarra

**Guitar** elige el instrumento. Cada tipo cambia el largo de escala y la cantidad de trastes, el juego de cuerdas y su tensión, el cuerpo (cuánto sostienen las notas, qué tan brillante es, cómo resuenan el cuerpo y el puente) y las pastillas (dónde están y de qué tipo son).

| Guitarra | Escala | Trastes | Pastillas | Sonido |
|---|---|---|---|---|
| Solid body, three single coils | 648 mm | 22 | tres single-coil, selector de cinco posiciones | brillante, cristalina, con el sonido "nasal" en las posiciones 2 y 4 |
| Solid body, two humbuckers | 628 mm | 22 | dos humbuckers, selector de tres posiciones | gorda, cálida, sostiene mucho |
| Semi-hollow body, two humbuckers | 628 mm | 22 | dos humbuckers | más redonda, con aire y algo de resonancia acústica |
| Hollow jazz body | 628 mm | 20 | dos humbuckers | oscura, de madera, sostiene poco, cuerdas pesadas |
| Baritone | 686 mm | 24 | un humbucker y un single-coil | una cuarta más grave, profunda y firme |
| Seven-string | 648 mm | 24 | dos humbuckers | afinación estándar más un Si grave |

### Cuerdas y afinación

- **Tuning**: Standard, Drop (la más grave un tono abajo), Half step down, Open G, Open D, Open E, DADGAD, Drop a tone lower y Custom. En Custom, de **String 1 tuning** a **String 7 tuning** suben o bajan cada cuerda en semitonos desde la afinación estándar (la cuerda 1 es la más aguda).
- **String gauge**: Light, Regular, Medium o Heavy. Las cuerdas más gruesas tienen más tensión, más fundamental y sostienen más; las finas se doblan más fácil y suenan más brillantes.
- **Capo** (0 a 12): una cejilla en ese traste. La digitación cuenta los trastes desde la cejilla, como un guitarrista.
- **Fine tune** (±50 cents): toda la guitarra.
- **String age** (0 a 100 %): las cuerdas nuevas brillan y sostienen; las viejas pierden los agudos enseguida.
- **Body sustain** (25 a 200 %): cuánto duran las notas.
- **Action** (0 a 100 %): la altura de las cuerdas. Con acción alta suena más limpio; con acción baja trastea antes cuando subís **Fret buzz**.
- **Fret buzz** (0 a 100 %): la cuerda tocando los trastes cuando vibra con amplitud, más fuerte con púa dura.
- **Sympathetic strings** (0 a 100 %): las cuerdas que no se tocan igual vibran un poco con las otras a través del puente, como en una guitarra real.
- **Mute unplayed strings**: encendido, la mano izquierda apaga todas las cuerdas que no se tocan, así nada resuena por simpatía. Sirve para rítmicas bien secas.

### Pastillas y controles

- **Pickup selector**: en la guitarra de tres single-coil, cinco posiciones: Bridge, Bridge + middle, Middle, Middle + neck, Neck. Las otras guitarras tienen dos pastillas y tres posiciones: Bridge, Both, Neck. Hacé clic en una posición de la llave. La del puente es brillante y fina, la del mástil redonda y llena; las posiciones intermedias de la single-coil tienen el sonido hueco, "nasal", de dos bobinas en lugares distintos de la cuerda.
- **Volume** y **Tone** (0 a 10): los potes de la guitarra. Bajar el volumen también saca algo de agudos (el pote contra el cable) y, con el amplificador saturado, limpia el sonido como en una guitarra real. El tono es el capacitor clásico: oscurece y, cerca de cero, agrega un pico resonante ("woman tone").
- **Cable length** (1 a 10 m): un cable más largo tiene más capacidad y suaviza los agudos.
- **Mains hum** (50 o 60 Hz) y **Pickup hum** (0 a 100 %): el zumbido y el soplido de las pastillas. Las single-coil zumban más que los humbuckers. Dejalo en cero para grabar limpio; subilo un poco para más realismo con alta ganancia.

### Ruidos de la mano

Cada ruido tiene su nivel (0 a 100 %):

- **Finger squeak**: los dedos deslizándose sobre las cuerdas entorchadas cuando la mano se mueve por el mástil, más fuerte cuanto más rápido.
- **Release noise**: el ruidito cuando un dedo suelta la cuerda.
- **Pick attack noise**: el raspado de la púa en el ataque.

## Tocar

![La página Play: modo, articulaciones y ligados a la izquierda; acordes, el patrón de rasgueo y el de punteo en el medio; vibrato, digitación, la mano derecha y la variación humana a la derecha.](img/play.jpg)

### Modos

**Play mode** define cómo las teclas se vuelven guitarra.

- **Melody (poly)**: cada tecla es una nota. Las notas que empiezan juntas se ubican juntas en cuerdas distintas (un acorde punteado rápido de grave a agudo); una línea sola se digita con ligados donde las notas se pisan.
- **Melody (mono)**: una nota por vez, como un solista. Una nota nueva reemplaza a la anterior; las que se pisan son hammer-ons, pull-offs o slides.
- **Chords**: las teclas que mantenés se reconocen como un acorde (22 tipos, con bajos invertidos) y Thrum lo toca armado como lo haría un guitarrista, con un rasgueo cada vez que lo cambiás.
- **Strum pattern**: el acorde que mantenés se rasguea con el patrón elegido, a tiempo con tu tema.
- **Fingerpicking**: el acorde se puntea cuerda por cuerda con el patrón de punteo elegido.

### Articulaciones

**Articulation** elige cómo se tocan las notas. También se cambia con keyswitches (ver MIDI), con la velocity y, algunas, con controladores.

- **Sustain**: notas normales.
- **Palm mute**: el canto de la mano derecha apoyado en las cuerdas cerca del puente. **Palm mute amount** dice cuánto (de un chug suave a totalmente ahogado). El CC 2 lo cambia en vivo.
- **Staccato**: notas cortas; **Staccato length** dice qué tan cortas (40 a 300 ms).
- **Dead notes**: la mano izquierda apenas toca las cuerdas: un golpe percusivo sin altura. **Dead notes below velocity** convierte cada nota suave en nota muerta, para tocar ghost notes con la velocity.
- **Harmonics**: armónicos naturales donde la nota existe como armónico de una cuerda al aire (trastes 12, 7, 5, 4 y 3); artificiales en el resto.
- **Pinch harmonics**: el chillido del pulgar que roza la cuerda justo después de la púa. **Pinch above velocity** convierte cada nota fuerte en pinch harmonic.
- **Tremolo picking**: la nota se puntea repetida a **Tremolo picking** (1/8, 1/16, 1/16T, 1/8T o 1/4 del tempo) mientras mantenés la tecla.
- **Bend in**: la nota empieza doblada desde abajo (**Bend-in amount**, 1 a 4 semitonos, en **Bend-in time**).
- **Slide in**: la nota entra deslizando desde unos trastes más abajo.
- **Slide out**: al soltar la tecla el dedo se desliza mástil abajo.

### Ligados, slides y vibrato

- **Legato**: Off, Hammer-on / pull-off, Slides o Auto (hammer-ons y pull-offs para intervalos chicos, slides para los grandes). Vale para las notas que se pisan en los modos de melodía, en la misma cuerda, dentro de **Legato reach** trastes (1 a 7). El CC 68 lo prende y lo apaga en vivo.
- **Slide time** (20 a 400 ms): cuánto tardan los slides.
- **Auto vibrato**: las notas largas sueltas toman un vibrato de mano después de **Vibrato delay**, a **Vibrato rate** (3 a 8 Hz) y **Vibrato depth** (en cents). El vibrato empuja la cuerda y la deja volver, como un guitarrista, así que sólo sube. La rueda de modulación (CC 1), la presión de canal y el aftertouch polifónico también agregan vibrato.

### Bends, palanca y feedback

- **Pitch bend range** (1 a 12 semitonos): la rueda de pitch bend dobla la última cuerda tocada, o todas con **Pitch bend bends every string**.
- **Whammy range** (1 a 24 semitonos): el CC 4 baja la palanca. Cada cuerda baja una cantidad distinta, como en un puente flotante, y al soltar la palanca el puente oscila un instante.
- **Feedback** (0 a 100 %) y **Feedback pitch**: una nota sostenida se vuelve de a poco el acople del amplificador: sostiene para siempre y, al rato, salta a la Fundamental, la Octava, la Octava + quinta o Dos octavas. Mantener el keyswitch de feedback hace lo mismo con cualquier nota.

### Acordes, rasgueo y fingerpicking

- **Voicing**: Auto, Open, Barre, Power, Triads o Jazz. Auto elige lo que un guitarrista tocaría más probablemente en la región elegida: acordes abiertos cerca de la cejuela, con cejilla más arriba.
- **Chord region** (traste 0 a 12): en qué parte del mástil se tocan los acordes.
- **Single key plays**: qué significa una sola tecla en los modos de acordes: un acorde mayor (Major chord), una quinta (Power chord) o la nota sola (The note).
- **Chord window** (0 a 80 ms): las teclas que llegan dentro de ese tiempo se toman como un solo acorde. Subilo si tocás los acordes sueltos.
- **Latch chord**: el acorde sigue sonando al soltar las teclas, hasta que tocás el siguiente.
- **Let open strings ring**: las cuerdas al aire siguen sonando cuando cambia el acorde, en vez de apagarse.
- **Strum pattern**: el patrón de rasgueo, de una biblioteca de 32 (rock, pop, funk, reggae, balada, post-punk y más). El patrón elegido se edita en la página Play.
- **Strum speed** (5 a 120 ms): cuánto tarda un rasgueo de la primera a la última cuerda, con fuerza media. Los rasgueos más fuertes son más rápidos.
- **Upstroke strings** (2 a 7): cuántas cuerdas agudas suele tocar un rasgueo hacia arriba.
- **Strum timing**: **Centred on the beat** (el de fábrica) empieza el rasgueo un poco antes del tiempo para que las cuerdas del medio caigan en él, que es como marcan el tiempo los guitarristas; **Starts on the beat** pone la primera cuerda en el tiempo.
- **Picking pattern**: el patrón de fingerpicking, de una biblioteca de 11 (Travis, bajo alternado, rolls, p-i-m-a, arpegios de balada y más).
- **Tempo (host stopped)**: con el DAW parado, los patrones siguen este tempo y arrancan con la primera tecla.

Con el DAW en marcha, los patrones siguen su posición, y cuando hace loop cada vuelta rasguea igual que la primera: el final del loop pasa a su principio sin perder ni repetir un golpe.

> Con Centred on the beat, un acorde que cambia justo en el tiempo en tu clip MIDI se toma unos milisegundos tarde: las cuerdas del acorde viejo que todavía no se habían tocado se saltean y el nuevo se rasguea enseguida. Para cambios bien justos, adelantá las notas del acorde nuevo una fusa (1/64), como la mano del guitarrista que se mueve antes del tiempo, o elegí Starts on the beat.

### El lenguaje de los patrones de rasgueo

Cada paso de un patrón de rasgueo es un golpe:

| Paso | Golpe |
|---|---|
| D / U | abajo / arriba, todo el acorde |
| d / u | abajo / arriba, sólo las cuerdas agudas |
| M / m | abajo / arriba con palm mute |
| X / x | rasgueo muerto abajo / arriba: la mano izquierda sólo apoya ("chk") |
| B | el bajo solo |
| A | el bajo alternado |
| S | abajo, y el acorde se corta medio paso después |
| . o - | nada |

Una barra vertical (el carácter pipe) apaga todas las cuerdas. Cada paso puede tener acento (normal, acento o fuerte); el patrón tiene un largo (hasta 32 pasos en el editor), una división (1/8, 1/16, 1/16T, 1/8T o 1/4) y swing para 1/8 y 1/16.

En la página Play, el patrón de rasgueo es una fila de pasos: clic en un paso para el golpe siguiente, Mayúsculas+clic para el anterior, clic derecho para cambiar su acento; **+** y **-** cambian el largo. Los botones de la división están arriba, y **Swing** cambia al arrastrarlo (doble clic: derecho). El patrón de punteo es el campo de texto al lado de su menú: escribí los grupos y apretá Return.

Los patrones de punteo usan grupos separados por espacios, uno por paso: `T` la cuerda del bajo, `A` el bajo alternado, `1` la cuerda más aguda del acorde, `2` la siguiente hacia abajo, `3`, `4`; varios en un grupo suenan juntos (`T1` es una pinza de bajo y melodía); `.` es un silencio y `>` delante de un grupo lo acentúa.

### Digitación

Cada nota va a una cuerda y un traste reales. Thrum elige por costo, como un guitarrista: deja la mano en un lugar cuando puede, evita saltar entre cuerdas y prefiere las posiciones bajas y las cuerdas al aire en la medida que le pidas.

- **Hand position** (traste −1 a 19): −1 deja que Thrum mueva la mano libremente; cualquier otro valor la mantiene cerca de ese traste.
- **Preferred string** (0 a 7): 0 es automático; de 1 a 7 toca en esa cuerda todas las notas que puede (la 1 es la más aguda). Ideal para una melodía en la cuerda grave o un riff entero en una sola cuerda.
- **Open strings** y **Low positions** (0 a 100 %): cuánto le gustan a Thrum.

### La mano derecha

- **Fingers instead of pick**: ataques más suaves y redondos, sin el raspado de la púa.
- **Pick position** (20 a 160 mm desde el puente): cerca del puente es fino y brillante; sobre el mástil, redondo y suave.
- **Pick hardness** (0 a 100 %): una púa blanda redondea el ataque y una dura lo hace más filoso. El CC 74 la cambia en vivo.
- **Pick angle** (0 a 100 %): una púa inclinada raspa más y excita la cuerda en otro plano, lo que cambia cómo se apaga.

### Variación humana

No hay dos notas iguales. La variación se calcula, no es al azar: el mismo MIDI da siempre el mismo audio, y otra **Variation seed** (en la página Settings) da otra interpretación igual de natural.

- **Human tuning**: unos cents de diferencia de nota a nota, porque los dedos aprietan la cuerda con distinta fuerza.
- **Human timing**: pequeños retrasos y un rasgueo menos regular.
- **Human velocity**: pequeños cambios de fuerza.
- **Human pick**: la púa cae en un lugar, un ángulo y una dureza apenas distintos cada vez. Es lo que reemplaza al round robin de las guitarras sampleadas.
- **Idle hand mute** (0,3 a 6 s): cuando dejás de tocar, pasado ese tiempo la mano se apoya en las cuerdas y apaga lo que quedaba sonando.

## Pedales

![La página Pedals con Cold Chorus Arpeggio: chorus, delay y reverb encendidos después del gabinete; el delay elegido, con sus controles abajo.](img/pedals.jpg)

La pedalera tiene trece pedales; el amplificador y el gabinete son parte de la misma cadena. Hacé clic en el pulsador de un pedal (o doble clic en el pedal) para encenderlo; clic en el pedal para ver sus controles debajo de la pedalera; arrastralo a otro lugar de la pedalera para cambiar su posición en la cadena. La pedalera se lee como una página: la fila de arriba de izquierda a derecha y después la de abajo. El orden habitual es gate, compresor, wah y saturaciones antes del amplificador; modulación, delay y reverb después del gabinete, donde están en estéreo.

- **Noise gate**: **Threshold** y **Release**. Calla el zumbido y el soplido entre frases; imprescindible con alta ganancia.
- **Compressor**: **Sustain**, **Attack**, **Level** y **Blend** (compresión en paralelo). Empareja el punteo y agrega sustain.
- **Wah**: **Mode** Manual (la perilla **Position**), Auto (sigue lo fuerte que tocás: **Sensitivity**, **Speed**) o MIDI CC 11 (un pedal de expresión o una línea de controlador). **Range** y **Resonance** dan forma al barrido.
- **Green overdrive**: un overdrive suave con los medios adelante: **Drive**, **Tone**, **Level**. El clásico delante de un amplificador que ya satura.
- **Distortion**: recorte duro: **Amount**, **Filter**, **Level**.
- **Fuzz**: fuzz de dos transistores, de **Silicon** o **Germanium**: **Amount**, **Bias** (con menos bias chisporrotea y se corta), **Tone**, **Level**. Se limpia al bajar el volumen de la guitarra.
- **Chorus**: **Rate**, **Depth**, **Mix**, **Tone**.
- **Phaser**: **Rate**, **Depth**, **Feedback**, **Mix**.
- **Flanger**: **Rate**, **Depth**, **Feedback**, **Mix**, **Manual**.
- **Tremolo**: **Shape** Sine, Optical o Harmonic; **Rate**, **Depth**; **Sync** al tempo.
- **Vibrato**: vibrato de altura, **Rate** y **Depth**.
- **Delay**: **Type** Digital, Analog (repeticiones más oscuras) o Tape (wow, flutter y saturación en el lazo); **Time** o, con **Sync**, una división de 1/32 a 1/2 con puntillos y tresillos; **Feedback**, **Mix**, **Tone**, **Modulation**.
- **Reverb**: **Type** Spring, Room o Hall; **Decay**, **Tone**, **Mix**, **Pre-delay**, **Size / drip** (en el resorte, cuánto "gotea"); **Freeze** congela la reverb para siempre, para colchones ambient.

## Amplificador y gabinete

![La página Amp / Cab con Crunch Rhythm: el amplificador British crunch y dos micrófonos frente al parlante que se arrastran en el panel.](img/amp.jpg)

### El amplificador

**Amp** elige uno de cuatro:

- **American clean**: mucho margen limpio y un tonestack con los medios bajos; satura sólo con mucha ganancia y un rasgueo fuerte.
- **British crunch**: tres etapas de ganancia, el crunch clásico hasta el rock.
- **Modern high gain**: cinco etapas con los graves apretados, para metal y rock moderno.
- **Small combo**: un combo chico clase A sin master: la etapa de potencia satura y se hunde.

Los controles:

- **Gain**: cuánto se exige al preamplificador.
- **Auto level** (encendido de fábrica): Gain cambia el carácter pero casi no el volumen, así lo podés barrer sin saltar fuera de la mezcla. Apagalo para el comportamiento clásico de un amplificador: Gain también sube el volumen, y mucho en el limpio y en el combo. Los dos modos suenan igual de fuerte con Gain en 5.
- **Bass**, **Middle**, **Treble**: el tonestack, el circuito real de estos amplificadores, donde los controles se influyen entre sí.
- **Presence** y **Depth**: resonancia aguda y grave de la etapa de potencia.
- **Master**: cuánto se exige a la etapa de potencia.
- **Sag** (0 a 10): la fuente que se hunde con la carga: más compresión y "floración" en los acordes fuertes.
- **Bright**: la llave de brillo, más notoria con poca ganancia.
- **Amp level** (±24 dB): el nivel después del amplificador.
- **Amp quality** 2x, 4x u 8x (sobremuestreo): más alto es más limpio con alta ganancia y usa más CPU. También fija la latencia (47, 55 o 57 muestras).

### El gabinete

**Cabinet source** elige de dónde sale el sonido del gabinete:

- **Model**: nuestro propio modelo del parlante y los micrófonos. **Speaker** (Vintage 12, British 12, American 12, Modern 12, Small 10) y **Cabinet** (1x12 abierto, 2x12 abierto, 4x12 cerrado, combo chico 1x10) definen la caja. Dos micrófonos, **Mic A** y **Mic B** (Dynamic, Condenser o Ribbon), cada uno con su **position** sobre el parlante (0 % en el centro del cono, brillante; 100 % en el borde, oscuro) y su **distance** (1 a 60 cm; de cerca hay más graves por el efecto de proximidad). Arrastrá los micrófonos en el panel debajo del parlante: a izquierda y derecha es la posición sobre el cono, arriba y abajo la distancia; la imagen del gabinete muestra dónde quedaron. **Mic mix A/B** los mezcla, con la diferencia de tiempo real entre ellos, y **Room reflection** agrega el piso.
- **Factory IR**: ocho respuestas al impulso hechas con el mismo modelo, listas para usar.

![Inspect en la página Amp / Cab: la respuesta en frecuencia del gabinete y los micrófonos tal como están.](img/amp-inspect.jpg)

- **User IR**: cargá tu propia respuesta al impulso (WAV, AIFF o FLAC, mono o estéreo, hasta 3 segundos). Se remuestrea a la frecuencia del proyecto y se guarda con él. También sirven respuestas largas: una sala, un resorte o una cadena de estudio completa.

## Presets

Sesenta y nueve presets en nueve categorías:

- **Clean**: Glass Arpeggio, Warm Jazz Box, Funk Scratch, Sparkle Pop Strum, Clean Picked Lead, Fingerstyle Semi, Chicken Pickin', Bossa Night.
- **Crunch & Blues**: Small Combo Breakup, Blues Lead Bends, Texas Shuffle, Crunch Rhythm, Edge of Breakup, Open G Slide, Delta Fingerpick, Green Drive Lead.
- **Rock & High Gain**: Classic Stack Eighths, Arena Lead, Seventies Riff, Modern Chug, Baritone Doom, Metal Lead, Thrash Palm Mute, Seven String Grid, Stoner Fuzz, Pinch Squeal.
- **Post-punk & Dark wave**: Cold Chorus Arpeggio, Night Drive Strum, Post-Punk Downstrokes, Gothic Lead, Ice Cathedral, Wire Stabs, Baritone Shadows, Coldwave Tremolo.
- **Surf, Shoegaze & Ambient**: Surf Spring Drip, Surf Rhythm, Twang and Spring, Shoegaze Wall, Haze Melody, Ambient Freeze, Swell Pad, Feedback Drone, Endless Sustain.
- **Strummed Chords**: Campfire Strum, Pop Punk Eighths, Reggae Skank, Funk Ghost Notes, Boom Chuck, Waltz Strum, Six-Eight Ballad, Anthem Strum, Half Time Pop.
- **Leads**: Legato Runs, Bend Into Note, Slide Lead, Trill Lead, Tremolo Picking Lead, Harmonics Bells, Whammy Dives, Fusion Lead.
- **Effects**: Auto Wah Funk, Wah Lead (CC 11), Seventies Phaser, Jet Flanger Chords, Tape Echo Twang, Harmonic Tremolo Chords.
- **Direct**: DI Single Coils, DI Humbuckers, DI Baritone: la guitarra sola, sin amplificador, para tus propios plugins de amplificador.

Un preset define la guitarra, la forma de tocar y toda la cadena. Los ajustes MIDI, el tempo para cuando el host está parado, la salida DI y la calidad del amplificador son de tu sesión y se conservan al cambiar de preset.

Ableton Live no muestra los presets de fábrica de los plugins (ignora las listas de programas de VST3). El menú de presets de Thrum funciona en todos lados, y los presets de fábrica aparecen además en el navegador de Live en Plug-Ins → Audio Tools Thrum (desplegá el plugin), instalados por el launcher o copiando la carpeta Presets del zip. Thrum tiene 182 parámetros y Live guarda como mucho 128 de un dispositivo para automatizar: hacé clic en **Configure** en el dispositivo de Thrum y mové los controles que quieras ahí. En la lista de parámetros de un host, los interruptores del amplificador y de la caja son **Amp On** y **Cabinet On** (**Amp** y **Cabinet** son el modelo y la caja).

## MIDI

![La página Settings: entrada y salida MIDI, las salidas, y los keyswitches y controladores con los nombres reales de las notas.](img/settings.jpg)

### Keyswitches

Las teclas por debajo de la cuerda al aire más grave de la guitarra son keyswitches; empiezan en **Keyswitch base note** (la nota MIDI 12 de fábrica, que es C0 en los DAW donde el Do central es C4, o C-1 donde es C3). Nunca suenan.

| Tecla | Articulación (queda hasta la siguiente) | | Tecla | Mientras se mantiene |
|---|---|---|---|---|
| base + 0 | Sustain | | base + 12 | Rasgueos hacia arriba |
| base + 1 | Palm mute | | base + 13 | Feedback |
| base + 2 | Staccato | | base + 14 | Vibrato |
| base + 3 | Dead notes | | base + 15 | Apagar todas las cuerdas |
| base + 4 | Harmonics | | base + 16 | Dedos |
| base + 5 | Pinch harmonics | | | |
| base + 6 | Tremolo picking | | | |
| base + 7 | Bend in | | | |
| base + 8 | Slide in | | | |
| base + 9 | Slide out | | | |

### Controladores

| Controlador | Hace |
|---|---|
| Pitch bend | dobla la última cuerda tocada (o todas, una opción), dentro de Pitch bend range |
| CC 1 (rueda de modulación) | profundidad del vibrato |
| CC 2 | cantidad de palm mute (0 = apagado) |
| CC 4 | palanca: la baja |
| CC 11 | posición del wah, con el wah en modo MIDI CC 11 |
| CC 64 (sustain) | dejar sonar las cuerdas |
| CC 68 | ligado sí o no |
| CC 74 | dureza de la púa |
| Presión de canal, aftertouch polifónico | vibrato |
| CC 120, CC 123 | apagar todo el sonido, todas las notas |

La velocity es la fuerza de la púa y, con **Dead notes below velocity** y **Pinch above velocity**, también puede elegir notas muertas y pinch harmonics.

### Modos de entrada MIDI

**MIDI input**, en la página Settings:

- **Normal**: cualquier canal, un instrumento.
- **Guitar (channel per string)**: para guitarras MIDI y controladores de guitarra que mandan cada cuerda por su canal. El canal 1 es la cuerda más aguda y el 6 (o el 7) la más grave; el pitch bend de cada canal dobla sólo su cuerda, dentro de **Per-string bend range**.
- **MPE**: para controladores MPE. Cada nota conserva su canal, y el pitch bend de ese canal (dentro de **MPE bend range**, 48 semitonos de fábrica), la presión (vibrato) y el CC 74 (dureza de la púa) actúan sólo sobre la cuerda que la toca. Poné el mismo rango de bend en tu controlador.

### Salida MIDI

Con **MIDI out** encendido, Thrum manda lo que la guitarra tocó de verdad: cada cuerda de cada rasgueo, cada nota digitada, ligados y slides, en el momento en que suenan. Grabalo en otra pista para editar un rasgueo nota por nota, o mandalo a otro instrumento para doblar la guitarra. **MIDI out channel** elige el canal, o **MIDI out channel per string** manda cada cuerda por su canal (el 1 la más aguda), listo para un instrumento tipo guitarra.

> Los rasgueos muertos y las cuerdas apagadas dentro de un rasgueo se tocan pero no salen como notas: no tienen altura.

Las notas salen de Thrum tan tarde como su audio (su latencia, 55 muestras con calidad 4x), que el host compensa sólo para el audio, así que un instrumento que dobla o una grabación quedan a tiempo con la guitarra.

En Ableton Live y en REAPER usá el VST3 para la salida MIDI: ninguno de los dos toma el MIDI que producen los plugins Audio Unit. En Live poné MIDI From de la pista que recibe en la pista de Thrum; en REAPER usá un envío con MIDI.

En la página Play, **Export pattern** guarda el patrón de rasgueo o de punteo actual como archivo `.mid`, tocado sobre el acorde que mantenés (o Do mayor), y **Export played** guarda todo lo que tocó Thrum desde la última vez que apretaste **Clear**.

## Salidas

Thrum tiene dos salidas estéreo: la principal, con toda la cadena, y **DI**, la guitarra sola antes de los pedales, retrasada exactamente la latencia del amplificador para que las dos queden alineadas. Grabá las dos y después podés reamplificar la DI con otro amplificador, el de Thrum o cualquier otro. En los hosts que toman una sola salida, **Main output is DI** manda la DI por la principal. **Output** y **DI level** fijan sus niveles.

## Cómo sacarle realismo

- Tocá como guitarrista, no como pianista. Los acordes de guitarra se arman en pocas formas; dejá que Thrum los arme (los modos de acordes) en vez de tocar voicings de piano en modo melodía.
- Pisá las notas para ligar y dejá espacio para las notas punteadas. Un slide o un hammer-on pasa sólo donde las notas se pisan.
- Usá la velocity. Un punteo suave es más oscuro y bajo; uno fuerte, más brillante y un instante más agudo, como las cuerdas reales.
- Dejá la variación humana encendida. Treinta por ciento es un guitarrista prolijo; setenta, uno suelto. Cero suena mecánico y sirve sólo para pruebas.
- Variá la articulación en un riff: un chug con palm mute y un acento abierto, una nota muerta entre dos acordes, un slide a la última nota. Los keyswitches en una línea MIDI aparte lo hacen fácil.
- Hacé los bends hacia la nota con la rueda y poné el vibrato al final de las notas largas, no al principio.
- Cambiá de acorde un poco antes del tiempo, como la mano del guitarrista, sobre todo con Strum timing en Centred.
- Usá la guitarra del estilo: single-coil para funk y pop limpio, humbuckers para rock, la hollow para jazz, la barítono para dark wave.
- No te pases con los ruidos: un poco de chirrido en los slides y en los cambios de posición convence; demasiado suena a error.
- Paneá dos tomas a izquierda y derecha con distintas **Variation seed** para una rítmica doblada.

## El renderizador de línea de comandos

`thrumrender` renderiza un preset a un WAV con el plugin completo, sin DAW:

- `thrumrender out.wav --preset "Campfire Strum" --chords "C G Am F" --bpm 96` rasguea una progresión.
- `thrumrender out.wav --preset "Blues Lead Bends" --midi solo.mid` toca un archivo MIDI; `--di di.wav` también escribe la DI.
- `thrumrender --list` lista los presets; `--bench` mide la CPU de cada preset; `--export-irs carpeta` escribe las respuestas al impulso de fábrica; `--export-pattern f.mid` escribe un patrón.

## Especificaciones

- Formatos: VST3, Audio Unit (macOS), LV2 (Linux), standalone; instrumento con entrada y salida MIDI.
- Sistemas: macOS 11 o posterior (Apple Silicon e Intel), Windows 10 u 11 (x64), Linux x86_64 (glibc 2.35 o posterior).
- Latencia: 47, 55 o 57 muestras (calidad del amplificador 2x, 4x, 8x), la misma con el amplificador apagado.
- CPU, un núcleo a 48 kHz: 3,3 % en promedio sobre los presets en un Apple M4, 6,2 % en un AMD Ryzen 9 5900XT; la guitarra sola 1,4 % y 2,7 %.
- Cuerdas: guías de onda digitales con dos polarizaciones, rigidez ajustada por traste, modulación de tensión, acople por el puente y punto muerto del mástil. Pastillas con su posición, su apertura y su circuito pasivo. Amplificadores con etapas a válvula, el tonestack real, etapa de potencia y sag, sobremuestreados. Gabinete por nuestro propio modelo de parlante y micrófonos o por respuestas al impulso de hasta 3 s (convolución particionada, sin latencia agregada).
- Sin samples: cada sonido se calcula mientras tocás.
