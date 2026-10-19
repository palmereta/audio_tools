# Manual del AT700

## Qué es el AT700

El AT700 es un sintetizador modular basado en el Roland System-700 de 1976. Recrea tres gabinetes como estaban armados en un estudio: la consola principal con sus 17 módulos y la llave de encendido, el teclado controlador 701A abajo y el secuenciador analógico 717A arriba. Funciona como instrumento VST3 o Audio Unit en tu DAW, o solo como aplicación independiente, en macOS y Windows.

Como el original, viene parcialmente cableado: los osciladores ya llegan a los filtros, los filtros a los amplificadores y los amplificadores a la salida, así que lo podés tocar apenas carga. Los cables suman todo lo demás, en 137 jacks que funcionan todos.

Todo adentro funciona en voltios, como en el hardware, así que el patcheo es predecible: una salida del secuenciador en 1 V sube un oscilador exactamente una octava.

Una página de ajustes suma lo que un estudio espera hoy y el System-700 nunca tuvo: hasta ocho voces de polifonía, MIDI, dos filtros a elección para el VCF-1, sincronía del secuenciador con el tempo del host, sobremuestreo y presets.

![La consola principal con el preset Berlin Twelve: el secuenciador maneja dos osciladores y el filtro, y su reloj dispara la envolvente.](img/overview.jpg)

La gráfica de los paneles es propia. Roland y System-700 son marcas de Roland Corporation; los nombres se usan sólo para identificar el instrumento que inspiró esta recreación.

## Primeros pasos

### Instalación en macOS

El AT700 necesita macOS 11 o posterior y corre nativo en Apple silicon e Intel.

1. Descomprimí el archivo descargado.
2. Copiá `AT700.vst3` a `~/Library/Audio/Plug-Ins/VST3/`.
3. Copiá `AT700.component` a `~/Library/Audio/Plug-Ins/Components/`.
4. Reescaneá los plugins en tu DAW (en Ableton Live: Settings, Plug-Ins, Rescan).

`AT700.app` es la versión independiente y puede ir donde quieras, por ejemplo en Aplicaciones.

> Los binarios tienen firma ad hoc y no están notarizados. Si macOS se niega a abrir el plugin, corré esto en la Terminal: `xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/AT700*.vst3 ~/Library/Audio/Plug-Ins/Components/AT700*.component`

### Instalación en Windows

El AT700 necesita Windows 10 u 11 de 64 bits.

1. Descomprimí el archivo descargado.
2. Copiá la carpeta `AT700.vst3` a `C:\Program Files\Common Files\VST3\` (Windows pide permisos de administrador).
3. Reescaneá los plugins en tu DAW.

`AT700.exe` es la versión independiente y funciona sin DAW. Los binarios no están firmados, así que SmartScreen de Windows puede pedirte confirmación la primera vez.

### Cargarlo en un DAW

El AT700 es un instrumento con entrada MIDI y salida estéreo (también acepta salida mono). Creá una pista de instrumento o de MIDI y elegí AT700 entre los plugins de Audio Tools. Las notas en cualquier canal MIDI tocan el teclado 701A. El plugin informa una cola de seis segundos, así que los releases largos y la reverb de resorte no se cortan cuando termina un clip.

### Tu primer sonido

El AT700 abre con el preset **Init**: el VCO-1 con una sierra pasa por el VCF-1 y el VCA-1, la envolvente ADSR-1 abre el filtro y el amplificador, y se mezcla un poco de reverb de resorte. Tocá unas notas en tu teclado MIDI, o hacé clic en las teclas del 701A abajo de la ventana (elegí **KEYBOARD** en la barra de arriba para verlas). Después mové el fader **CUTOFF FREQ.** del VCF-1 y el fader **RES.** de al lado, y ya estás dándole forma al sonido.

Si preferís arrancar con algo terminado, hacé clic en el nombre del preset en la barra de arriba y elegí uno de los 32 presets de fábrica.

## Recorrida por la interfaz

El editor tiene tres partes: la barra de arriba, el lienzo con los tres gabinetes y una barra de estado abajo. La ventana se agranda o se achica desde la esquina; conserva las proporciones y va de un tercio de su tamaño completo a 1,3 veces.

![La barra de arriba: navegador de presets, SAVE, RANDOM y las cuatro vistas.](img/topbar.jpg)

### La barra de arriba

- Los botones **<** y **>** recorren los presets.
- El botón del medio muestra la categoría y el nombre del preset actual. Hacé clic para abrir el menú de presets.
- **SAVE** guarda el patch actual como preset de usuario.
- **RANDOM** arma un patch al azar: mueve osciladores, filtros, envolventes y LFO, y respeta tus cables y la mezcla de salida.
- **SEQUENCER**, **CONSOLE**, **KEYBOARD** y **ALL** mueven la vista entre los gabinetes.
- **SYNTH** muestra el lienzo y **SETTINGS** la página de ajustes, en el extremo derecho de la barra.
- **MANUAL** abre este manual dentro del plugin, en inglés o en castellano. Esc o la cruz lo cierran.

### El lienzo y las vistas

Los tres gabinetes están parados sobre un solo lienzo grande y los botones de vista se deslizan entre ellos. **CONSOLE** encuadra toda la consola principal, **SEQUENCER** el 717A con la parte de arriba de la consola, **KEYBOARD** la fila de abajo de la consola con el teclado adelante, y **ALL** muestra los tres gabinetes juntos a la mitad del tamaño. Los cables siguen colgando entre los gabinetes en todas las vistas.

![Los tres gabinetes juntos: el 717A arriba, la consola principal y el teclado 701A.](img/all.jpg)

Girá la rueda del mouse o arrastrá sobre una parte vacía del panel para subir o bajar.

### La barra de estado

La barra de abajo muestra el modo del teclado (**VOICES**), una luz **MIDI IN** que parpadea cuando llega MIDI, el estado del **SEQUENCER** (RUN o STOP, el paso actual y si usa el reloj interno o el del host), el modelo de **VCF-1** en uso, la carga de CPU y la versión.

### Cómo se manejan los controles

- Perillas: arrastrá hacia arriba o hacia abajo. Mantené Shift para el ajuste fino. Las perillas con posiciones (RANGE, DIVIDE, STEP SELECTOR) saltan de una posición a la siguiente.
- La perilla PITCH de cada oscilador es concéntrica: el anillo grande de afuera fija los semitonos y el centro chico es FINE.
- Faders, verticales u horizontales: arrastrá la tapa, o hacé clic en el recorrido para saltar ahí.
- Llaves deslizantes y palancas de forma de onda: hacé clic en la posición que quieras.
- Llaves de palanca: hacé clic para cambiarlas; las de tres posiciones de las envolventes siguen el lugar donde hacés clic.
- Los botones (START, STOP, MANUAL y los demás) actúan mientras los apretás.
- La rueda del mouse sobre una perilla o un fader lo mueve; con doble clic vuelve a su valor por defecto.
- Mientras movés un control, una etiqueta muestra su nombre y su valor en unidades reales. La palanca BENDER vuelve sola al centro cuando la soltás.
- Hacé clic en una tecla del 701A para tocarla, y arrastrá sobre las teclas para pasar de nota en nota.

Con clic derecho sobre cualquier perilla, fader o llave la asignás a un controlador MIDI (ver MIDI).

### Los cables

- Arrastrá de un jack a otro para enchufar un cable, empezando por cualquier punta. Los cables van tomando colores por turno.
- Un destino válido se ilumina con un anillo amarillo. No se puede ir de salida a salida ni de entrada a entrada, y un cable que soltás en cualquier otro lado desaparece.
- Para mover un cable, hacé clic en un jack enchufado y arrastrá: la ficha sale de ese jack y la podés poner en otro lado, o soltarla en un lugar vacío para sacar el cable.
- Para sacar varios cables de una misma salida, mantené Shift y arrastrá desde la salida: una ficha nueva se apila sobre las que ya están.
- Con Alt y clic, o clic derecho, sobre un jack sacás todos los cables enchufados en él.
- Una entrada acepta un solo cable. Si enchufás uno nuevo en una entrada ocupada, reemplaza al anterior. Las salidas aceptan todos los cables que quieras.

## Cómo funciona el sistema

### Niveles y tensiones

El AT700 respeta las convenciones del System-700, y los paneles las siguen.

| Señal | Nivel |
|---|---|
| Audio | 10 V pico a pico |
| Tensión de control | 0 a más o menos 10 V |
| Compuertas y disparos | +15 V |
| Afinación | 1 V por octava |
| Entradas de compuerta y reloj | conmutan a unos +5 V |

En cada módulo, los faders de arriba mezclan entradas de audio y los de abajo, bajo MODULATION, tensiones de control. Un fader de modulación en 10 suma la tensión que entra con toda su fuerza: en un filtro o un oscilador (con el atenuador en 1) eso es exactamente 1 V por octava, así que una secuencia que entra por ahí queda afinada.

Los filtros y amplificadores tienen dos luces junto a la salida. La verde indica señal. La roja se prende cuando la suma de la entrada pasa de unos 13 V pico a pico, lo que pasa cuando hay más de un fader de entrada cerca de 10; al mismo tiempo se apaga la verde. El módulo entra entonces en saturación suave, que puede ser el sonido que buscás.

### Los buses internos

La consola principal viene cableada de fábrica. Estas conexiones no necesitan cables:

- El WAVE FORM SELECTOR de cada oscilador manda la forma de onda elegida a los faders de entrada de los dos filtros y al modulador en anillo.
- El ruido blanco llega al fader W.NOISE del VCF-1, y el modulador en anillo al fader RING MOD del VCF-2.
- Los dos filtros llegan a los faders de entrada de los dos amplificadores.
- Los dos amplificadores llegan a las entradas de la reverb, a los canales PAN 1 y PAN 2 y a las entradas del phase shifter. La reverb llega a PAN 3.
- El KEY CV1 del teclado llega a las llaves KEY CV de los osciladores y a los faders KEY CV de los filtros. Su compuerta y su disparo llegan a las dos envolventes y a las llaves KEY TRIG de los LFO.
- El LFO-1 llega a los osciladores, a los filtros y al sample and hold. El ADSR-1 llega al VCO-2, y el sample and hold al VCO-3. Las dos envolventes llegan a los filtros y amplificadores por sus selectores ADSR1 y ADSR2.

Algunas entradas tienen conexión de corte: el jack lleva una señal interna hasta que enchufás un cable, y entonces el cable la reemplaza. PAN 1 y PAN 2 llevan el VCA-1 y el VCA-2, PAN 3 lleva la reverb, las entradas del phase shifter llevan el VCA-1 y el VCA-2, la entrada del seguidor de envolvente lleva la salida del amplificador, y la entrada del integrador lleva la salida del seguidor.

## La consola principal: fila de arriba

![Los tres VCO y el generador de ruido con el modulador en anillo.](img/vcos.jpg)

### 702 VCO-1, VCO-2 y VCO-3

Cada oscilador tiene cuatro salidas simultáneas, **seno**, **triángulo**, **sierra** y **pulso**, y una palanca **WAVE FORM SELECTOR** que elige cuál de las cuatro va a los filtros y al modulador en anillo por el bus interno.

| Control | Rango | Qué hace |
|---|---|---|
| RANGE | LO, 32', 16', 8', 4', 2' | Octava. 8' es la afinación normal; LO está cinco octavas abajo, para barridos subsónicos y modulación |
| PITCH | -12 a +12 semitonos | Afinación, anillo de afuera (-24 a +24 en LO) |
| FINE | -100 a +100 cents | Ajuste fino, la perilla chica del centro |
| PULSE WIDTH | 10 a 90 % | Ancho de la onda de pulso |
| KEY CV | ON, OFF | Si el oscilador sigue al teclado |

En 8' y con PITCH al centro, la tecla más grave del 701A toca un do de 65,4 Hz. Las salidas de sierra y pulso van de 0 a +10 V, y el seno y el triángulo oscilan entre más y menos 5 V. En el bus a los filtros las cuatro van centradas.

Cada oscilador tiene una sección **MODULATION** de faders, cada uno con una llave atenuadora **1/10 y 1** arriba. En 1, un fader en 10 pasa la tensión con toda su fuerza, 1 V por octava; en 1/10 pasa la décima parte, que es lo que querés para un vibrato.

- VCO-1: **LFO1** (interno) y **EXT** (el jack debajo del fader).
- VCO-2: **LFO1**, **ADSR1** (internos) y **EXT**.
- VCO-3: **LFO1**, **S&H** (interno, el sample and hold) y **EXT**.

El VCO-1 tiene además un fader **PWM** con su propio jack. El fader fija cuánto mueve el ancho de pulso la tensión que entra por ese jack; en 10, cada voltio ensancha el pulso un 10 %. El fader PWM no hace nada hasta que llega un cable al jack PWM.

El VCO-2 y el VCO-3 tienen un jack **SYNC.** con una llave **S y W**. S (fuerte) reinicia el ciclo del oscilador cada vez que la señal de SYNC sube de +5 V: el hard sync clásico, con su barrido rasgado cuando movés la afinación del oscilador sincronizado. W (débil) sólo adelanta un poco el ciclo, para un enganche más suave que también agarra algunas relaciones no armónicas. La fuente habitual es una sierra o un pulso de otro oscilador.

### 708A generador de ruido y modulador en anillo

La mitad de arriba es el **NOISE GENERATOR**, con dos salidas: **WHITE NOISE OUT** y **PINK NOISE OUT** (el rosa cae 3 dB por octava y suena más oscuro). El ruido blanco llega también al VCF-1 por adentro; el rosa sólo sale por su jack.

La mitad de abajo es el **RING MODULATOR**. Multiplica dos señales y da la suma y la diferencia de sus frecuencias, el sonido metálico de campanas y chapas. Dos palancas eligen sus entradas: **X** toma VCO1, VCO2, VCO3 o el jack X, e **Y** toma VCO1, VCO2, NOISE o el jack Y (la última posición de cada palanca elige su jack). El resultado va al fader RING MOD del VCF-2 y al jack **R.M. OUT**, y una luz verde muestra la actividad. Se filtra un poco de la señal X, como en el original calibrado.

![El VCF-1 (703E) y el VCF-2 (703B).](img/filters.jpg)

### 703E VCF-1

El VCF-1 es un filtro pasa bajos de 24 dB por octava armado con cuatro etapas de transconductancia, el tipo que traen las consolas que sobreviven. Sus cinco faders de entrada mezclan **VCO1**, **VCO2**, **VCO3**, **W.NOISE** y una señal externa por el jack que está arriba del quinto fader.

| Control | Rango | Qué hace |
|---|---|---|
| CUTOFF FREQ. | 20 Hz a 30 kHz | Frecuencia de corte; la escala está marcada en 100, 1K y 10K |
| RES. | 0 a 10 | Resonancia; el filtro silba solo desde unos 7,5 |

Como el original, el VCF-1 no compensa la resonancia: al subir RES, los graves se adelgazan (unos 10 dB menos en 4 y 13 dB en 7). Compensalo con los faders de entrada o el amplificador, o usá el VCF-2.

El jack **RES. C.V. IN** deja que una tensión controle la resonancia; cada voltio suma un poco menos de una unidad en la escala de RES.

Los faders de **MODULATION** mueven el corte: **KEY CV** (seguimiento del teclado; en 10 el filtro sigue al teclado exactamente, que es su valor por defecto), **ADSR** (con una llave **ADSR1 y ADSR2** abajo para elegir la envolvente), **LFO1**, y **EXT** con su jack. Cada fader en 10 da 1 V por octava. Autooscilando con KEY CV en 10, el filtro toca afinado desde el teclado.

En la página de ajustes podés reemplazar el VCF-1 por un 703A, el filtro multimodo de 12 dB que el manual de 1976 muestra en ese lugar (ver Ajustes). El jack RES. C.V. IN sólo actúa sobre el 703E.

### 703B VCF-2

El VCF-2 es un filtro de estado variable de 12 dB por octava con una llave **FILTER MODE**: **HP** (pasa altos), **BP** (pasa banda) y **LP** (pasa bajos). Sus entradas son **VCO1**, **VCO2**, **VCO3**, **RING MOD** y un jack externo. CUTOFF FREQ., RES. y la sección MODULATION funcionan como en el VCF-1, salvo que no tiene entrada de resonancia y su fader KEY CV arranca en 0. Autooscila desde RES 7,6, también afinado, y conserva los graves al subir la resonancia.

![El VCA-1 y el VCA-2.](img/vcas.jpg)

### 704 VCA-1 y VCA-2

Los dos amplificadores fijan el volumen del sonido. Cada uno mezcla **VCF1**, **VCF2** y un jack externo en sus faders de entrada.

| Control | Rango | Qué hace |
|---|---|---|
| VCA GAIN | 0 a 10 | Ganancia inicial, antes de cualquier envolvente |
| LIN y EXP | llave | Respuesta a la tensión de control |
| MODULATION: ADSR | 0 a 10 | Cantidad de envolvente, con una llave ADSR1 y ADSR2 |
| MODULATION: EXT | 0 a 10 | Cantidad de la tensión del jack de abajo |

Con VCA GAIN en 0 sólo la envolvente abre el amplificador, como corresponde a una nota tocada; en 10 queda abierto, para drones.

En **LIN** cada voltio suma un 10 % de ganancia, así que la forma de la envolvente pasa tal cual. En **EXP** cada voltio suma 10 dB, lo que da ataques más secos y colas largas que suenan naturales.

![El módulo de salida 711A: reverberador, paneo, oscilador patrón y phase shifter.](img/output.jpg)

### 711A módulo de salida

El módulo más ancho tiene cuatro secciones y los vúmetros.

**REVERBERATOR.** Una reverb de dos resortes con el tono oscuro y metálico y el goteo de los resortes de verdad (unos dos segundos como máximo). Tres faders mezclan **VCA1**, **VCA2** y el jack **REV IN**. El resultado va a PAN 3 y al jack **REV. OUT**.

**PANNING.** Tres canales con un fader de nivel cada uno y un fader horizontal **PAN1**, **PAN2** y **PAN3** que los ubica entre **L** y **R** (al centro por defecto). Cada canal tiene un jack de entrada con conexión de corte: PAN 1 lleva el VCA-1, PAN 2 el VCA-2 y PAN 3 la reverb, hasta que enchufás un cable.

**STANDARD OSC.** Un tono de referencia para afinar: **LEVEL** (OFF a 10) y una llave de frecuencia con **110**, **220**, **440**, **880** y **1760** Hz (ondas cuadradas) y **1KHz** (un seno). La llave **L, LR y R** lo manda a la salida izquierda, a las dos o a la derecha. Va directo a las salidas, después del paneo.

**PHASE SHIFTER.** Ocho etapas de desfasaje, barridas entre 100 Hz y 10 kHz. Dos faders mezclan **VCA1** y **VCA2** (las dos entradas tienen jacks con conexión de corte), y **RESONANCE** realimenta la salida para un barrido más marcado. **AUTO SPEED** (0,03 a 10 Hz) e **INTENSITY** (0 a 10) fijan el barrido interno; pasá la llave **AUTO y EXT** a EXT y una tensión en el jack **EXT IN** lo barre en su lugar, con INTENSITY como profundidad.

> La salida del phase shifter, **P.S. OUT**, no está conectada a nada por adentro, como en el original. Para escucharla, patcheá P.S. OUT en una de las entradas PAN y subí el nivel de ese canal. Los presets de fábrica que usan el phaser hacen exactamente eso.

Abajo están los dos vúmetros, **L ch** y **R ch**, con un jack **OUT** debajo de cada uno. Esos jacks llevan la mezcla final izquierda y derecha.

## La consola principal: fila de abajo

### 712A monitor y procesador de tensión

**MONITOR LEVEL** es el volumen general. En 7 la salida está en su nivel normal, en 10 suena unos 9 dB más fuerte y en 0 queda en silencio. La ficha STEREO HEADPHONES y la ficha y la llave EXT CONTROLLER están dibujadas como en el original pero no tienen función en el plugin.

El **VOLTAGE PROCESSOR** es una fuente de tensión fija: el fader **+10V** fija una tensión de 0 a +10 V en **+OUT** y la misma tensión negativa en **-OUT**. Sirve para correr una afinación o abrir un filtro a mano.

Los jacks **KEY C.V. OUT** y **GATE OUT** dan el CV1 y la compuerta del teclado (en modo poly llevan la primera voz).

![El LFO-1, el LFO-2 y el sample and hold.](img/lfo-sh.jpg)

### 706 LFO-1 y LFO-2

Dos osciladores de baja frecuencia para vibrato, trémolo, barridos y relojes.

| Control | Rango | Qué hace |
|---|---|---|
| FREQUENCY | 0,03 a 30 Hz | Velocidad |
| Llave de onda | 4 posiciones | Cuadrada (pulso en el LFO-2), rampa descendente, triángulo, seno |
| DELAY TIME (LFO-1) | 2 ms a 10 s | Tiempo de entrada del LFO-1 después de la primera tecla |
| PULSE WIDTH (LFO-2) | 10 a 90 % | Ancho de la onda de pulso del LFO-2 |
| KEY TRIG. | ON, OFF | Reinicia la onda cuando empieza una frase |

Las ondas cuadrada y de pulso van de 0 a +10 V, la rampa baja de +10 V a 0, y el triángulo y el seno oscilan entre más y menos 5 V. El jack **FREQ C.V. IN** cambia la velocidad a 1 V por octava. La luz muestra la onda.

El LFO-1 es la fuente de modulación de la consola: llega a los faders de MODULATION de los osciladores y los filtros y al sample and hold sin ningún cable. Su DELAY TIME hace que el vibrato llegue de a poco: cada vez que tocás una tecla después de haber soltado todas, el LFO-1 arranca desde cero y va entrando en el tiempo que fijaste. Con **KEY TRIG.** en ON, además, la onda vuelve a empezar desde el mismo punto en ese momento, así que cada frase arranca igual.

> El LFO-2 no tiene conexiones internas. Sólo funciona por su jack **LFO OUT**: patchealo donde lo necesites, por ejemplo en el jack PWM del VCO-1.

### 709A sample and hold

El sample and hold toma una muestra de una señal en cada pulso de reloj y mantiene esa tensión hasta el siguiente: escalones al azar con ruido, escaleras con un LFO.

- **SAMPLE MODE** elige qué se muestrea: **LFO1**, **NOISE** o (posición de abajo) el jack **EXT SIG.**
- La llave **EXT e INT** elige el reloj: INT usa el reloj interno fijado con **CLOCK RATE** (0,3 a 30 Hz), EXT usa pulsos en el jack **EXT CLOCK** (cada subida por encima de +5 V toma una muestra).
- **S&H OUT** da la tensión retenida (hasta más o menos 10 V), que llega también por adentro al fader de modulación S&H del VCO-3.
- **CLOCK OUT** da el reloj como una onda cuadrada de +15 V, útil para disparar una envolvente al ritmo de los escalones.

![El amplificador, seguidor de envolvente e integrador 707A, y el doble generador de envolvente 705A.](img/env.jpg)

### 707A amplificador, seguidor de envolvente e integrador

**AMPLIFIER.** Un preamplificador para señales bajas: una llave de rango (**x1000**, **x100** y **x10**) y un fader horizontal de ganancia de 0 a 1 multiplican la señal de **IN**; el resultado, limitado a más o menos 10 V, sale por **OUT**.

**ENVELOPE FOLLOWER.** Convierte el volumen de una señal en una tensión: 1 V por cada voltio pico a pico de entrada, hasta 10 V, con subida rápida y bajada más lenta. Su **IN** tiene conexión de corte con la salida del amplificador. Una luz verde muestra el nivel.

**INTEGRATER** (así está escrito en el panel). Un lag: su salida sigue a la entrada despacio, con **LAG TIME** de 0 a 3 segundos, y baja un poco más lento de lo que sube. Su **IN** tiene conexión de corte con la salida del seguidor, así que sin cables las tres secciones forman una sola cadena. Usado solo, suaviza escalones y los convierte en glissandos, por ejemplo en una salida del secuenciador.

### 705A doble generador de envolvente

Dos envolventes ADSR, **ADSR -1** y **ADSR -2**, cada una con cuatro faders:

| Control | Rango | Qué hace |
|---|---|---|
| ATTACK TIME | 2 ms a 10 s | Tiempo para subir a 10 V |
| DECAY TIME | 2 ms a 10 s | Tiempo para bajar al nivel de sustain |
| SUSTAIN LEVEL | 0 a 10 | Nivel que se mantiene mientras dura la compuerta |
| RELEASE TIME | 2 ms a 10 s | Tiempo para bajar a 0 cuando termina la compuerta |
| TIME | x1, x1/10 | x1/10 divide los tres tiempos por diez (0,2 ms a 1 s) |

Las envolventes van de 0 a +10 V y salen por sus jacks **OUT** además de llegar por adentro a los filtros y amplificadores (el ADSR-1 llega también al VCO-2). La llave de tres posiciones de la izquierda fija qué las dispara:

- Arriba, **GATE + TRIG**: la compuerta del teclado mantiene la envolvente y cada disparo nuevo reinicia el ataque, así que cada nota de una frase ligada queda articulada.
- Al medio, **GATE**: sólo cuenta la compuerta; una frase ligada recibe una sola envolvente de la primera a la última nota.
- Abajo, **EXT GATE**: la envolvente escucha el jack **EXT GATE** en lugar del teclado. Cualquier cosa por encima de +5 V es una compuerta: el CLOCK OUT del secuenciador o el GATE OUT de un paso, el CLOCK OUT del S&H, una cuadrada de un LFO.

El botón blanco **MANUAL GATE** abre las dos envolventes mientras lo mantenés apretado, estén en el modo que estén.

![El mezclador, los múltiples y la llave de encendido.](img/mixer-mult.jpg)

### 716A mezclador

Tres entradas con un fader cada una, sumadas y mandadas a dos salidas: **NON-INV. OUT** da la suma, e **INV. OUT** la da invertida. Sirve para audio o tensiones de control, por ejemplo para sumar dos modulaciones o invertir una envolvente.

### 710A múltiples

Dos columnas de cinco jacks. Cada jack de una columna lleva la suma de todo lo que está enchufado en esa columna, así que un múltiple deja que una salida alimente varias entradas, o suma unas cuantas señales. Un jack del múltiple funciona como entrada o como salida según lo que le enchufes: un cable de una salida a un múltiple alimenta la columna, y un cable del múltiple a una entrada saca la señal.

Las dos columnas se pueden unir con un cable, pero una columna no se puede alimentar a sí misma, y las dos columnas no se pueden alimentar entre sí en un lazo: el editor rechaza esos cables.

### 718A encendido

La llave **POWER**, con su luz. En OFF la salida se desvanece hasta el silencio y los vúmetros y las luces se apagan; volvé a ponerla en ON para escuchar la consola otra vez.

## La patchera

Acá están los 137 jacks por módulo, con sus señales. Las salidas pueden alimentar cualquier cantidad de entradas; cada entrada acepta un cable.

![La vista del teclado: la fila de abajo de la consola con sus cables, y el 701A adelante.](img/patchbay.jpg)

| Módulo | Entradas | Salidas |
|---|---|---|
| VCO-1 | PWM, EXT | seno, triángulo, sierra, pulso |
| VCO-2, VCO-3 | SYNC., EXT | seno, triángulo, sierra, pulso |
| 708A | X e Y del ring mod | ruido blanco, ruido rosa, R.M. OUT |
| VCF-1 | IN de audio, RES. C.V. IN, EXT (corte) | VCF OUT |
| VCF-2 | IN de audio, EXT (corte) | VCF OUT |
| VCA-1, VCA-2 | IN de audio, EXT (ganancia) | VCA OUT |
| 711A | REV IN, PAN 1 a 3, IN 1 y 2 del phaser, EXT IN | REV. OUT, P.S. OUT, OUT L, OUT R |
| 712A | ninguna | +OUT, -OUT, KEY C.V. OUT, GATE OUT |
| LFO-1, LFO-2 | FREQ C.V. IN | LFO OUT |
| 709A | EXT SIG., EXT CLOCK | S&H OUT, CLOCK OUT |
| 707A | AMP IN, E.F. IN, INT IN | AMP OUT, E.F. OUT, INT OUT |
| 705A | EXT GATE 1 y 2 | OUT de ADSR 1 y 2 |
| 716A | IN 1 a 3 | NON-INV. OUT, INV. OUT |
| 710A | 10 jacks, entrada o salida | |
| 717A | 4 de pulso de control, EXT CLOCK, C.V. IN, PULSE IN, 12 ANALOG IN | START y END PULSE, 2 CLOCK OUT, 2 del pulse shaper, 12 GATE OUT, 6 paralelas, 2 en serie |
| 701A | ninguna | KEY CV1, KEY CV2, GATE, KEY TRIG., BEND CV, VELOCITY, MOD WHEEL, AFTERTOUCH |

Y qué da cada tipo de salida:

| Salida | Señal |
|---|---|
| Sierra y pulso de los VCO | 0 a +10 V |
| Seno y triángulo de los VCO | más o menos 5 V |
| Filtros, amplificadores, ruido, ring mod, reverb | audio, unos 10 V pico a pico |
| Cuadrada, pulso y rampa de los LFO | 0 a +10 V |
| Triángulo y seno de los LFO | más o menos 5 V |
| ADSR, seguidor de envolvente | 0 a +10 V |
| Procesador de tensión | 0 a +10 V y 0 a -10 V |
| CV del teclado | 0 V en la tecla más grave, +5 V en la más aguda, 1 V por octava |
| Compuertas, disparos, relojes, pulsos | +15 V |
| BEND CV | más o menos 5 V |
| VELOCITY, MOD WHEEL, AFTERTOUCH | 0 a +10 V |
| Salidas paralelas y en serie del secuenciador | 0 a 10, 4 o 2 V |

Las entradas de compuerta y reloj (EXT GATE, EXT CLOCK del S&H y del secuenciador, SYNC.) responden a una subida por encima de unos +5 V. Los jacks CONTROL PULSE IN del secuenciador responden por encima de +7 V, y el pulse shaper tiene su propio umbral ajustable.

Las entradas que mueven una afinación o una frecuencia (EXT de los VCO y los VCF, FREQ C.V. IN de los LFO, C.V. IN del reloj) siguen 1 V por octava al máximo. Las entradas EXT de los VCA siguen la respuesta LIN o EXP.

> La realimentación está permitida: podés patchear una salida de vuelta en una entrada de su propio módulo. Las tensiones se frenan en los rieles de más y menos 15 V, como en el hardware, así que un lazo satura en lugar de dispararse. Sólo los múltiples están protegidos contra lazos.

En modo poly (ver el teclado), los jacks de los módulos de voz llevan una señal por voz. Un cable de un módulo compartido (un LFO, el sample and hold, el secuenciador) a un módulo de voz llega a todas las voces; un cable de un módulo de voz a uno compartido lleva la suma de las voces.

## El teclado controlador 701A

![El teclado controlador 701A, con un acorde apretado.](img/keyboard.jpg)

El teclado tiene 61 teclas en cinco octavas. Su tensión es de 0 V en la tecla más grave y +5 V en la más aguda, 1 V por octava. La nota MIDI 36 es la tecla más grave; las notas fuera de las 61 teclas suenan igual, sólo que no iluminan ninguna tecla.

![El panel del 701A con las salidas del panel trasero y los jacks MIDI.](img/kb-panel.jpg)

### Controles del panel

| Control | Rango | Qué hace |
|---|---|---|
| PITCH | -6 a +6 semitonos | Transpone todo el teclado |
| PORTAMENTO TIME | 0,1 a 10 s | Tiempo de deslizamiento entre notas |
| PORTAMENTO | OFF, ON | Activa el deslizamiento |
| Llave de rango del bend | 5V, 1V, 0.1V, OFF | Cuánto mueve la palanca BENDER la tensión del teclado |
| BENDER | palanca | Dobla todas las notas, vuelve sola a 0 |

El rango del bend está en voltios: en 1V la palanca dobla una octava para cada lado, en 0.1V poco más de un semitono, y en 5V cinco octavas, para efectos extremos. El pitch bend MIDI mueve la misma palanca (ver Ajustes).

### Salidas del panel trasero y jacks MIDI

Bajo **REAR PANEL OUTPUTS** están los jacks que el teclado original tenía atrás, traídos al frente:

- **KEY CV1** y **KEY CV2**: las dos tensiones del teclado (ver los modos más abajo). CV1 llega también a la consola por adentro; CV2 sólo está acá.
- **GATE OUT**: +15 V mientras haya una tecla apretada.
- **KEY TRIG.**: un pulso de 2 ms a +15 V cuando empieza una nota nueva.
- **BEND CV**: la posición de la palanca como más o menos 5 V, sin importar la llave de rango.

Bajo **MIDI** hay tres salidas que el original nunca tuvo: **VELOCITY**, **MOD WHEEL** y **AFTERTOUCH**, cada una de 0 a +10 V. No hacen nada hasta que las patcheás: la velocidad en la entrada EXT de un VCA para un volumen sensible al toque, la rueda en la entrada EXT de un VCO para un vibrato a mano, el aftertouch en la entrada EXT de un filtro.

### Modos del teclado

El modo se elige en la página de ajustes y se ve en la barra de estado.

**Duo (original).** El 701A es duofónico, como el original. KEY CV1 sigue a la tecla más grave apretada y KEY CV2 a la más aguda; con una sola tecla las dos son iguales. Las tensiones quedan donde están cuando soltás las teclas, así que las colas del release conservan su afinación. El disparo salta cada vez que cambia la tecla más grave. Con CV1 en un oscilador y CV2 patcheado en otro, tocás líneas a dos voces por un mismo filtro y un mismo amplificador.

**Mono.** Una nota por vez, con prioridad a la última tecla apretada. Si la soltás mientras seguís apretando otras, la tensión vuelve a la tecla más reciente que siga apretada. Cada tecla nueva dispara.

**Poly 4, Poly 6 y Poly 8.** Cada tecla recibe su propia voz: los módulos de voz de la consola (los tres VCO, el modulador en anillo, los dos filtros, los dos amplificadores, las dos envolventes, el 707A, el mezclador, los múltiples y las salidas del teclado) se duplican para cada voz, cables incluidos. Los LFO, el sample and hold, el ruido, el módulo de salida, el procesador de tensión y el secuenciador quedan compartidos, así que un mismo barrido de LFO mueve todas las voces juntas. Una nota nueva toma la voz que se soltó hace más tiempo, o le roba la voz a la más vieja si están todas ocupadas. En modo poly, CV2 es igual a CV1 en cada voz.

Cambiar el modo suelta las notas apretadas. KEY CV DROOP, también en la página de ajustes, hace que la tensión retenida baje despacio después de soltar, como perdía el sample and hold del original.

## El secuenciador analógico 717A

![El secuenciador analógico 717A tocando Berlin Twelve.](img/sequencer.jpg)

El 717A guarda tres filas de doce tensiones, CH-A, CH-B y CH-C, y las recorre a tiempo. Cada columna es un paso. Las filas pueden sonar como tres secuencias en paralelo (tres tensiones por paso, para afinación, filtro y lo que quieras a la vez) o una detrás de otra como una sola serie de hasta 36 pasos. Cada paso tiene además su propia duración, una llave de hold para los silencios, un botón manual, una salida de compuerta y una entrada analógica.

El secuenciador es compartido por todas las voces y toca sólo lo que patchees con sus salidas.

![Pasos 1 a 6: las tres perillas de canal, DIVIDE, FINE, la llave NORMAL y HOLD, MANUAL, ANALOG IN y GATE OUT.](img/seq-steps.jpg)

### Los pasos

Cada una de las doce columnas tiene, de arriba hacia abajo:

| Control | Rango | Qué hace |
|---|---|---|
| CH-A, CH-B, CH-C | 0 a 10 | Las tres tensiones del paso |
| DIVIDE | 1/32, 1/16, 1/8, 1/4, 1/2, 1 | Duración del paso, como fracción |
| FINE | 1 a 2 | Estira el paso, hasta el doble |
| NORMAL y HOLD | llave | HOLD convierte el paso en un silencio |
| MANUAL | botón | Salta a ese paso |
| ANALOG IN | jack | Convierte las perillas del paso en controles de nivel |
| GATE OUT | jack | +15 V mientras suena el paso |

**DIVIDE y FINE** fijan juntos la duración del paso: DIVIDE por FINE, como fracción de la unidad básica del reloj (ver el reloj más abajo). FINE en 1,5 sobre un paso de 1/8 da una corchea con puntillo; en 2 duplica el paso.

**HOLD** mantiene las tensiones del paso anterior y calla el CLOCK OUT durante ese paso. Cuando el reloj dispara tu envolvente, un paso en HOLD es un silencio que conserva la nota anterior. El GATE OUT del paso sale igual.

El **GATE OUT** de cada paso da +15 V durante la parte alta del pulso de reloj mientras suena ese paso. Patcheá la compuerta de un paso en una envolvente para acentuar ese paso, o en las entradas CONTROL PULSE para que un paso pare o reinicie la secuencia. Con el secuenciador parado, la compuerta del paso actual queda alta.

**ANALOG IN**: con una señal patcheada en el ANALOG IN de un paso, las salidas de ese paso dan la señal multiplicada por las perillas del paso en lugar de una tensión fija (10 es el nivel completo). El secuenciador se vuelve una llave de doce posiciones, incluso para audio.

Una luz bajo cada paso muestra el paso actual.

![El pulse shaper, el controlador maestro y el oscilador de reloj.](img/seq-clock.jpg)

### El oscilador de reloj

| Control | Rango | Qué hace |
|---|---|---|
| CLOCK RATE | 2 a 20 (STEP/MINUTE) | Velocidad del reloj interno |
| RANGE | x10, x1 | Multiplica la velocidad por 10 o por 1 |
| PULSE WIDTH | 10 a 90 % | Duración de cada pulso de reloj dentro del paso |

RANGE en x10 cubre de 20 a 200. La etiqueta de valor muestra la escala propia de la perilla, de 2 a 20, sin el x10.

CLOCK RATE cuenta en redondas: es la cantidad de pasos por minuto con DIVIDE en 1. Cada paso dura 60 segundos divididos por la velocidad, por su DIVIDE y por su FINE. Entonces:

- Para tocar semicorcheas a un tempo, poné la velocidad en el tempo dividido por 4 y DIVIDE en 1/16. Para 128 BPM: velocidad 32 (RANGE x10, perilla en 3,2), DIVIDE en 1/16 en todos los pasos.
- Corcheas a 120 BPM: velocidad 30, DIVIDE en 1/8.
- Con todos los DIVIDE en su valor por defecto de 1/4, una velocidad igual a los BPM da semicorcheas.

El jack **CONTROL VOLTAGE IN** cambia la velocidad del reloj interno a 1 V por octava (más o menos 5 V): un voltio la duplica.

El jack **EXT CLOCK** reemplaza el reloj interno por pulsos: cada subida por encima de +5 V cuenta como 1/32 de redonda, el divisor del original. Un paso con DIVIDE en 1/16 lleva dos pulsos, uno en 1/4 lleva ocho.

Los dos jacks **CLOCK OUT** dan un pulso de +15 V al comienzo de cada paso, tan largo como fija PULSE WIDTH. Se callan en los pasos en HOLD y con el secuenciador parado. La luz del reloj muestra los pulsos.

### El controlador maestro

Cuatro botones manejan la marcha, cada uno con una luz y un jack **CONTROL PULSE IN** abajo que hace lo mismo cuando una tensión sube de +7 V:

- **START**: arranca desde el paso 1 del canal A y manda un pulso por **START PULSE OUT**.
- **CONTINUE**: sigue desde el paso actual.
- **STOP**: para en el paso actual.
- **END**: para y vuelve al paso 1 del canal A.

**END PULSE OUT** da +15 V siempre que el secuenciador no esté andando, así que sube cuando una serie en STOP llega a su último paso.

El botón **MANUAL** de cada paso salta directo a ese paso, andando o parado; con el secuenciador parado, las tensiones del paso salen en el acto, así que podés afinar una secuencia de a un paso.

![Las salidas paralelas y en serie, con la llave de modo y el selector de pasos.](img/seq-outputs.jpg)

### Salidas paralelas y en serie

**PARALLEL OUT** tiene dos jacks por canal, CH A, CH B y CH C, que siempre dan la tensión del paso actual de ese canal. Una llave **RANGE** por canal fija la escala de sus perillas:

| RANGE | Perilla en 10 da | Sirve para |
|---|---|---|
| 10V | 10 V | Barridos de filtro, niveles de VCA, modulación amplia |
| 4V | 4 V | Melodías de cuatro octavas |
| 2V | 2 V | Melodías de dos octavas, las más fáciles de afinar |

En el rango 2V, una octava es 5 en la perilla y un semitono poco más de 0,4.

**SERIES OUT** tiene dos jacks que dan los canales uno detrás de otro, según la llave **MODE**:

- **A**: sólo el canal A, pasos 1 a 12.
- **A-B**: el canal A y después el B: 24 pasos.
- **A-B-C**: A, B y C por turno: hasta 36 pasos.
- **MANUAL**: el canal elegido con los botones MANUAL **CH A**, **CH B** y **CH C** que están arriba de la llave.

Las luces sobre los botones CH muestran qué canal está sonando. En los otros modos, apretar un botón CH salta a ese canal en el acto.

La llave **STOP y REPEAT** decide qué pasa al final de la serie: REPEAT vuelve a empezar, STOP para en el último paso (en modo MANUAL la secuencia siempre da la vuelta).

El **STEP SELECTOR** fija cuántos pasos usa cada canal, de 1 a 12. Los pasos que quedan más allá del selector se saltean.

### El pulse shaper

A la izquierda del 717A, el **PULSE SHAPER** convierte cualquier señal en compuertas limpias. Cada vez que la señal de **PULSE IN** sube por encima de **THRESHOLD LEVEL** (0 a 10 V), los dos jacks **OUT** dan +15 V durante el **SWITCH-ON TIME** (0 a 3 segundos; unos pocos milisegundos en el mínimo). Una luz muestra la salida. Usalo para sacar compuertas de un LFO o del seguidor de envolvente, o para estirar compuertas cortas.

### Sincronía con el host y Follow Transport

Dos ajustes de la página de ajustes conectan el 717A con tu DAW.

**CLOCK** elige **Internal** (el oscilador de reloj) o **Host**. Con Host, el secuenciador sigue el tempo y la posición del DAW: una redonda del DAW es DIVIDE 1, así que los pasos de 1/16 caen en las semicorcheas de tu proyecto y los de 1/4 en los tiempos. La perilla CLOCK RATE, su RANGE y el C.V. IN no tienen efecto, y los pasos sólo avanzan mientras el DAW reproduce. FINE sigue estirando los pasos, ahora en tiempo musical.

**TRANSPORT** (Follow Transport, activado por defecto) hace que el Play y el Stop del DAW actúen como START y STOP, con cualquiera de los dos relojes. Play arranca desde el paso 1. Con el reloj del host, un salto en el DAW (un loop que vuelve, o un clic en otro lugar de la línea de tiempo) vuelve a arrancar la secuencia desde el paso 1 en ese punto, así que armá los loops en las barras de compás y usá un largo de secuencia que entre justo en el loop para mantenerte alineado.

> Los presets de la categoría Sequence arrancan el secuenciador apenas cargan, para que los escuches enseguida. Con TRANSPORT activado, parar el DAW los para; apretá START, o Play en el DAW, para volver a escucharlos.

Con TRANSPORT desactivado, sólo los botones del panel y las entradas CONTROL PULSE arrancan y paran la secuencia; con el reloj del host, igual espera a que el DAW reproduzca para moverse.

## Ajustes

![La página de ajustes: voces, carácter, reloj del secuenciador, MIDI y sistema.](img/settings.jpg)

La página de ajustes tiene lo que el System-700 no trae. Hacé clic en **SETTINGS**, a la derecha de la barra de arriba, para abrirla, y en **SYNTH** (o en cualquier botón de vista) para volver al lienzo. Todo lo de acá se guarda con cada preset, salvo OVERSAMPLING y OUTPUT, que quedan como los dejaste.

| Sección | Ajuste | Opciones | Por defecto |
|---|---|---|---|
| VOICES | KEYBOARD (701A) | Duo (original), Mono, Poly 4, Poly 6, Poly 8 | Duo |
| VOICES | KEY CV DROOP | Off, On | Off |
| CHARACTER | VCF-1 | 703E 24 dB LP, 703A 12 dB multimode | 703E |
| CHARACTER | 703A MODE | HP, BP, LP | LP |
| CHARACTER | VCO DRIFT | 0 a 10 | 1.5 |
| SEQUENCER | CLOCK | Internal, Host | Internal |
| SEQUENCER | TRANSPORT | Off, On | On |
| MIDI AND SYSTEM | PITCH BEND | Off, Bender | Bender |
| MIDI AND SYSTEM | OVERSAMPLING | 1x, 2x, 4x | 2x |
| MIDI AND SYSTEM | OUTPUT | -24 a +6 dB | 0 dB |

- **VCF-1** cambia el 703E por el 703A, el filtro de estado variable de 12 dB que el manual de 1976 muestra como VCF-1. Conserva los graves con la resonancia alta y ofrece los modos de **703A MODE**; el jack RES. C.V. IN no tiene efecto sobre él.
- **VCO DRIFT** fija el vagabundeo lento al azar de la afinación de los osciladores. En 0 son perfectamente estables.
- **PITCH BEND** en Bender deja que el pitch bend MIDI mueva la palanca BENDER, con el rango de la llave de rango del bend.
- **OVERSAMPLING** hace andar la consola al doble o al cuádruple de la frecuencia de muestreo. 4x limpia el hard sync y la modulación en anillo; 1x ahorra CPU con muchas voces.
- **OUTPUT** es el nivel final, después de MONITOR LEVEL.

Hacé clic en una opción para elegirla; arrastrá las perillas (con doble clic vuelven a su valor). Con clic derecho sobre un ajuste tenés MIDI Learn o volverlo a su valor por defecto.

## Presets

El botón de preset de la barra de arriba muestra la categoría y el nombre del preset actual. Hacé clic para abrir el menú: un submenú por categoría, después **Init** (el panel por defecto, sin cables) y **Show user presets folder**. Los botones < y > recorren toda la lista, y la lista de programas de tu DAW muestra los mismos presets.

Un preset guarda el panel completo, cada cable con su color, y los ajustes (salvo el sobremuestreo y la salida). Los 32 presets de fábrica:

| Categoría | Presets |
|---|---|
| Basics | Init |
| Lead | Fat Saw Lead, Sync Sweep, Tomita Whistle, Flute, PWM Lead, Duophonic Fifths, Glide Saw |
| Brass | Tomita Brass |
| Strings | Ensemble Strings |
| Pads | Phaser Pad |
| Bass | 700 Bass, Sheffield Bass, Sub Pulse |
| Bells | Ring Bells |
| Keys | Tine Keys, Clavi |
| Sequence | Berlin Twelve, Transposed Sequence, Thirty-Six Steps, Sequenced Filter, Gate Patterns |
| FX | Random Computer, S&H Filter, Surf and Wind, Laser Zaps, Thunder, Spring Drips |
| Drone | Space Drone |
| Drums | Kick, Snare, Hi-Hat |

Para guardar los tuyos, hacé clic en **SAVE**, escribí un nombre y confirmá. Los presets de usuario aparecen en el menú bajo User. Son archivos JSON en:

- macOS: `~/Library/Application Support/Audio Tools/AT700/Presets`
- Windows: `%APPDATA%\Audio Tools\AT700\Presets`

Guardar con el nombre de un preset de usuario que ya existe lo reemplaza.

**RANDOM** elige ajustes nuevos de osciladores, filtros, envolventes y LFO y no toca los cables ni la mezcla de salida, así que un patch de secuencia sigue siendo un patch de secuencia.

La sesión de tu DAW guarda el estado completo, incluidas las asignaciones de controladores MIDI, que no forman parte de los presets.

## MIDI

- Las **notas** en cualquier canal tocan el 701A según el modo del teclado. La nota MIDI 36 es la tecla más grave (0 V).
- La **velocidad** sale por el jack VELOCITY del panel del teclado, de 0 a 10 V por voz. No cambia nada hasta que la patcheás.
- El **pitch bend** mueve la palanca BENDER cuando PITCH BEND está en Bender; su rango es la llave de rango del bend.
- La **rueda de modulación** (CC 1) sale por el jack MOD WHEEL, de 0 a 10 V.
- El **aftertouch**, de canal o polifónico, sale por el jack AFTERTOUCH, de 0 a 10 V.
- **All notes off** y **all sound off** (CC 123 y 120) sueltan todas las notas, y **reset all controllers** (CC 121) centra el bend y pone en cero la rueda y el aftertouch.

**MIDI Learn.** Hacé clic derecho sobre cualquier perilla, fader, llave o palanca del lienzo, o sobre cualquier ajuste, y elegí **MIDI Learn**. Mové una perilla o un fader de tu controlador: desde ese momento ese controlador mueve el control. Cada control toma un controlador; el menú muestra el CC asignado y ofrece borrarlo. Se puede aprender cualquier CC de 0 a 119 salvo el pedal de sustain (CC 64). Un CC 1 aprendido mueve su control en lugar del jack MOD WHEEL. Las asignaciones se guardan con la sesión de tu DAW.

Cada control del panel y cada ajuste es también un parámetro del plugin, con el nombre de su módulo y su control (por ejemplo VCF1 Cutoff o Step 5 CH-A), así que los podés automatizar desde tu DAW.

## Recetas

Cada receta arranca desde **Init** (menú de presets, Init), salvo que diga otra cosa. Los valores como "5" se refieren a la escala de 0 a 10 impresa en el panel.

### 1. Un bajo pulsado

1. Cargá Init. En el VCO-1, poné **RANGE** en 16'.
2. En el VCF-1, bajá **CUTOFF FREQ.** a unos 150 Hz (mirá la etiqueta de valor) y poné **RES.** en 3.
3. En el ADSR-1, poné **ATTACK TIME** abajo de todo, **DECAY TIME** en unos 250 ms, **SUSTAIN LEVEL** en 0 y **RELEASE TIME** en unos 150 ms.
4. Subí el fader ADSR de la sección MODULATION del VCF-1 a 5 más o menos. Ahora cada nota abre el filtro y lo vuelve a cerrar: un bajo pulsado.
5. Para un segundo oscilador, poné la palanca de onda del VCO-2 en pulso, su RANGE en 16' y FINE unos cents corrido, y subí el fader de entrada **VCO2** del VCF-1.

### 2. Un vibrato que llega tarde

1. Cargá Init. En el VCO-1, poné en 1/10 el atenuador que está arriba del fader de MODULATION **LFO1**, y subí el fader a 1,5 más o menos.
2. En el LFO-1, poné la llave de onda en triángulo o seno y **FREQUENCY** en unos 5 Hz.
3. Poné **DELAY TIME** en alrededor de 1 s. Mantené una nota: arranca derecha y el vibrato va entrando.
4. Poné **KEY TRIG.** en ON para que el vibrato arranque siempre desde el mismo punto de su onda. Acordate de que el retardo sólo vuelve a empezar cuando tocás después de soltar todas las teclas.

### 3. Modulación de ancho de pulso

1. Cargá Init. Mové el **WAVE FORM SELECTOR** del VCO-1 a pulso, y dejá **PULSE WIDTH** en 50 %.
2. Patcheá el **LFO OUT** del LFO-2 en el jack **PWM** del VCO-1.
3. Subí el fader **PWM** a 6 más o menos y poné el LFO-2 en triángulo a 1 Hz. El sonido engorda y se mueve como un ensamble.
4. Para cuerdas, sumá el VCO-2 y el VCO-3 en sierra apenas desafinados, hacé más lenta la envolvente y patcheá **P.S. OUT** en PAN 1 con el fader de entrada del phaser arriba. El preset Ensemble Strings es este patch.

### 4. Un barrido de hard sync

1. Cargá Init. Patcheá la salida de **sierra** del VCO-1 en el jack **SYNC.** del VCO-2, y poné la llave S y W del VCO-2 en S.
2. En el VCF-1, bajá el fader de entrada VCO1 y subí el VCO2, así escuchás sólo el oscilador sincronizado.
3. Poné el PITCH del VCO-2 en +7. Ahora suena a la altura del VCO-1, con otro timbre.
4. Subí el fader de modulación **ADSR1** del VCO-2 (atenuador en 1) a 3 más o menos, y dale al ADSR-1 un decay de 1,5 s con sustain en 2. Cada nota barre el timbre de brillante a oscuro: el sonido clásico del sync. El preset Sync Sweep lo hace por el VCF-2.

### 5. Dos voces desde un solo teclado

1. Cargá Init y fijate que la página de ajustes tenga KEYBOARD en Duo (original).
2. Patcheá **KEY CV2** (salidas del panel trasero del 701A) en el jack **EXT** del VCO-2.
3. En el VCO-2, poné **KEY CV** en OFF, subí su fader de modulación **EXT** a 10 con el atenuador en 1 (así sigue 1 V por octava), y elegí una onda de pulso.
4. Subí el fader de entrada VCO2 del VCF-1. Tocá dos teclas: el VCO-1 toca la más grave y el VCO-2 la más aguda, por el mismo filtro y el mismo amplificador. Con una sola tecla, los dos suenan al unísono. Es el preset Duophonic Fifths.

### 6. Campanas con modulador en anillo

1. Cargá Init. Poné el VCO-1 y el VCO-2 en seno, y el PITCH del VCO-2 en +7 con FINE en unos +35 cents, una relación inarmónica.
2. En el 708A, poné la palanca **X** en VCO1 y la palanca **Y** en VCO2.
3. En el VCF-2, subí el fader **RING MOD**, poné FILTER MODE en LP y CUTOFF FREQ. alto (unos 5 kHz).
4. En el VCA-2, subí el fader de entrada **VCF2** y poné el selector ADSR en ADSR2 con su fader ADSR en 10. Dale al ADSR-2 un ataque abajo de todo, un decay de unos 2,5 s, sustain 0 y release de unos 2 s.
5. Bajá el fader de entrada VCF1 del VCA-1 para que suenen sólo las campanas, y subí VCA2 en la reverb. Tocá notas agudas y comparalo con Ring Bells.

### 7. Escalones de filtro al azar

1. Cargá Init y poné el SUSTAIN del ADSR-1 en 10 para que las notas mantenidas queden abiertas.
2. En el 709A, poné SAMPLE MODE en NOISE, la llave de reloj en INT y **CLOCK RATE** en unos 7 Hz.
3. Patcheá **S&H OUT** en el jack **EXT** de la sección MODULATION del VCF-1 y subí ese fader EXT a 3,5 más o menos.
4. Poné el RES. del VCF-1 en unos 6,5. Mantené una nota: el filtro salta a una posición al azar nueva varias veces por segundo.
5. Sin ningún cable, los mismos escalones pueden mover la afinación: subí el fader de modulación **S&H** del VCO-3 y dejá entrar el VCO-3 al filtro, para el sonido de "computadora al azar".

### 8. Una secuencia de la escuela de Berlín

Esta receta arma a mano el preset Berlin Twelve.

1. Cargá Init. En la página de ajustes dejá CLOCK en Internal. Elegí **ALL** en la barra de arriba para ver el secuenciador y la consola mientras patcheás.
2. En el 717A, poné MODE en **A**, la llave del final en **REPEAT** y el STEP SELECTOR en 12. Poné el **RANGE** de PARALLEL OUT del CH A en **2V**.
3. Poné el **DIVIDE** de todos los pasos en 1/16. Poné el **RANGE** del reloj en x10 y **CLOCK RATE** en 3,2 (32 redondas por minuto: semicorcheas a 128 BPM).
4. Patcheá una salida paralela **CH A** en el jack **EXT** del VCO-1, y la otra salida CH A en el jack EXT del VCO-2.
5. En los dos osciladores poné **KEY CV** en OFF, el fader de modulación EXT en 10 y su atenuador en 1. Poné los dos RANGE en 16', las dos ondas en sierra, el FINE del VCO-2 en unos +8 cents, y subí el fader de entrada VCO2 del VCF-1.
6. Patcheá **CLOCK OUT** en el jack **EXT GATE** del ADSR-1 y poné la llave de modo del ADSR-1 abajo (EXT GATE). Poné ATTACK abajo de todo, DECAY unos 220 ms, SUSTAIN 0 y RELEASE unos 180 ms.
7. Ajustá el VCF-1: CUTOFF FREQ. en unos 110 Hz, RES. en 4, modulación ADSR en 4 y modulación KEY CV en 0.
8. Apretá **START**. Afiná las perillas del CH A con un patrón; en el rango 2V el 0 es la tónica, 5 la octava, cerca de 2,9 una quinta y cerca de 4,2 una séptima menor. Probá 0, 5, 2,9, 4,2, 0, 5, 1,25, 4,2, 0, 5, 2,9, 6,25.
9. Para los acentos, patcheá una salida paralela **CH B** en el jack de modulación EXT del VCF-1, subí ese fader EXT a 1,2 más o menos, dejá el RANGE del CH B en 10V y poné algunas perillas del CH B más altas que otras.
10. Para un movimiento lento, subí un poco el fader de modulación LFO1 del VCF-1 y poné el LFO-1 en unos 0,08 Hz.

### 9. Transponer una secuencia desde el teclado

1. Cargá el preset Berlin Twelve, o armá la receta 8.
2. En el VCO-1 y el VCO-2, volvé a poner **KEY CV** en ON. Ahora la tensión del teclado se suma a la secuencia.
3. Tocá una tecla: toda la secuencia se mueve a esa tonalidad y se queda ahí cuando la soltás, porque el 701A retiene su tensión. La envolvente sigue al reloj, así que las teclas sólo transponen. Tocá la tecla más grave para volver a la altura original. El preset Transposed Sequence funciona así.

### 10. Enganchar una secuencia a tu DAW

1. Cargá cualquier preset de Sequence.
2. En la página de ajustes poné **CLOCK** en Host y dejá **TRANSPORT** activado.
3. Fijate que los pasos usen el DIVIDE que querés en términos musicales: 1/16 para semicorcheas, 1/8 para corcheas.
4. Apretá Play en tu DAW. La secuencia arranca desde el paso 1 sobre la grilla del DAW y sigue sus cambios de tempo. Pará el DAW y se para; hacé un loop de unos compases y vuelve a empezar con cada vuelta.

## Consejos y problemas

**No suena nada.** Revisá MONITOR LEVEL en el 712A (7 es lo normal), la llave POWER del 718A, el ajuste OUTPUT y los faders de nivel de los canales PAN. Después seguí la señal: un fader de entrada de oscilador arriba en un filtro, un corte que no esté abajo de todo, un fader de entrada de filtro arriba en el amplificador, y una envolvente o el VCA GAIN abriendo el amplificador.

**Un módulo parece mudo.** Muchas entradas necesitan un cable. El LFO-2 no tiene conexiones internas; el ruido rosa sólo sale por su jack; el P.S. OUT del phase shifter hay que patchearlo en una entrada PAN con ese canal arriba; el fader PWM necesita un cable en el jack PWM; cada fader EXT necesita un cable en su jack; el mezclador, los múltiples, el procesador de tensión, la velocidad, la rueda, el aftertouch y KEY CV2 no hacen nada hasta que los patcheás.

**El sonido se cortó cuando patcheé una entrada PAN o del phaser.** Esos jacks tienen conexión de corte. Un cable en PAN 1 reemplaza al VCA-1 en ese canal, un cable en PAN 3 reemplaza a la reverb. Patcheá la señal reemplazada en otro lado, o usá otro canal.

**Una envolvente no arranca.** Una envolvente en EXT GATE ignora el teclado y espera una compuerta en su jack EXT GATE. Probala con el botón MANUAL GATE.

**La secuencia suena desafinada.** Para 1 V por octava, el fader EXT del oscilador tiene que estar en 10 con su atenuador en 1. Si KEY CV sigue en ON, la tensión del teclado se suma a la secuencia (útil para transponer). Poné VCO DRIFT en 0 si necesitás los osciladores totalmente quietos.

**La secuencia va cuatro veces más rápido o más lento.** El reloj cuenta redondas. Revisá RANGE (x10 o x1), las perillas DIVIDE, y acordate de que la etiqueta de valor muestra la perilla sin el x10.

**La secuencia no se mueve, o se para sola.** Con el reloj del host sólo avanza mientras el DAW reproduce. Con TRANSPORT activado, parar el DAW la para. Una serie en STOP se detiene después de su último paso: apretá START, o poné REPEAT.

**El filtro pierde graves cuando subo la resonancia.** Es el 703E, como el original. Subí los faders de entrada o el VCA, o usá el VCF-2 o el modelo 703A para un sonido resonante con todos los graves.

**El pitch bend no hace nada, o dobla demasiado.** Fijate que PITCH BEND esté en Bender y que la llave de rango del bend no esté en OFF. En 5V el bend cubre cinco octavas; usá 0.1V para un semitono, más o menos, o 1V para una octava.

**Notas colgadas.** Mandá all notes off desde tu DAW, o poné el plugin en bypass y volvé a activarlo: el bypass suelta todas las notas.

## Especificaciones

| Ítem | Valor |
|---|---|
| Inspirado en | Roland System-700 (1976): consola principal, 701A, 717A |
| Versión | 1.0.0 |
| Formatos | VST3, Audio Unit (macOS), aplicación independiente |
| Sistemas | macOS 11 o posterior (Apple silicon e Intel), Windows 10 y 11 de 64 bits |
| Tipo | Instrumento, entrada MIDI, salida estéreo (acepta mono) |
| Módulos | 17 en la consola principal más el encendido, teclado 701A, secuenciador 717A |
| Puntos de patcheo | 137 jacks, diez de ellos en dos múltiples |
| Controles del panel | 219 |
| Voces | Duo (original), mono, poly 4, 6 u 8 |
| Osciladores | 3, seno, triángulo, sierra y pulso a la vez, de LO a 2', sync fuerte y débil |
| Filtros | 703E pasa bajos de 24 dB o 703A multimodo de 12 dB, más el 703B de 12 dB HP, BP, LP |
| Afinación | La a 440 Hz (medido 439,9 Hz) en 8'; rangos en octavas exactas |
| Autooscilación de los filtros | 703E desde RES 7,5, 703B y 703A desde 7,6, siguiendo al teclado |
| Pérdida de banda pasante del 703E | -9,7 dB con RES en 4, -13 dB con RES en 7 |
| Secuenciador | 3 x 12 pasos, hasta 36 en serie, de 2 a 200 pasos por minuto, reloj externo o del host |
| Sobremuestreo | 1x, 2x o 4x |
| Presets | 32 de fábrica con sus cables, más los de usuario |
| CPU | 1,8 % una voz, 8,8 % ocho voces (Apple M4, 2x) |
| Cola | 6 s |
| Licencia | GNU AGPL v3 |
