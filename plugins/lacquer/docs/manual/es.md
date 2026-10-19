# Manual de Lacquer

## Qué es Lacquer

Lacquer es un procesador de masterización automática. Le das una mezcla terminada y te devuelve un master: un tono equilibrado, la dinámica controlada, graves que suman bien en mono y una sonoridad adecuada para donde va la canción (Spotify, Apple Music, un club, un CD). Funciona de dos maneras:

- **En el bus master de tu DAW.** Apretás **Learn**, reproducís la canción y Lacquer arma toda su cadena con lo que escuchó. Desde ahí masteriza en tiempo real.
- **Con un archivo.** Abrís una mezcla (o la soltás sobre la ventana), Lacquer analiza la canción entera, renderiza el master, te deja compararlo con el original a la misma sonoridad y exporta un WAV con un informe.

El nombre viene del disco de laca: en la época del vinilo, la masterización terminaba cortando la música en una laca con un torno. El disco de la izquierda de la ventana es esa laca, y su surco es tu canción.

![Lacquer con una mezcla abierta como archivo: el disco con el surco del master a la izquierda, estilo, intensidad, objetivo y techo a la derecha, los lectores, y abajo el balance tonal y lo que hizo cada módulo.](img/overview.jpg)

## Cómo decide, en palabras simples

Lacquer no usa una red neuronal ni una base de canciones de otros. Mide tu mezcla y aplica reglas, las mismas que un ingeniero de mastering hace de cabeza:

1. **Escucha.** Mide la sonoridad como lo hacen los servicios de streaming (ITU-R BS.1770), los picos verdaderos entre muestras, qué tan dinámica es la canción, el espectro promedio de las partes fuertes, el ancho estéreo y si los graves están en mono, las resonancias ásperas y la sibilancia, los pasajes recortados y un corte en los agudos (señal de que la fuente fue un MP3).
2. **Compara el tono con un objetivo.** Cada estilo tiene su curva objetivo, armada con mediciones publicadas de música comercial (pendientes suaves, no una copia del producto de nadie). Donde tu mezcla ya está dentro de lo normal, Lacquer casi no toca; donde está claramente fuera, corrige el exceso, con límites de cuánto y de qué tan angosto.
3. **Ajusta cada módulo sólo lo necesario.** La compresión y el recorte dependen de cuánto pida el objetivo de sonoridad. Una mezcla que ya es densa y fuerte casi no recibe nada más que el limitador de seguridad.
4. **Llega a la sonoridad midiendo el resultado.** En modo archivo, Lacquer renderiza, mide y corrige hasta que el master mide el objetivo con un error de centésimas de LU.

Todo es determinista: la misma mezcla con los mismos ajustes da siempre exactamente el mismo master.

## Instalación

Descargá el paquete de tu sistema desde la web y descomprimilo.

En macOS:

1. Copiá `Lacquer.vst3` a `~/Library/Audio/Plug-Ins/VST3/` y `Lacquer.component` (Audio Unit) a `~/Library/Audio/Plug-Ins/Components/`.
2. Reescaneá los plugins en tu DAW.
3. `Lacquer.app` es la versión standalone y `lacquerrender` el render por línea de comandos.

Los archivos no están notarizados. Si macOS no los deja abrir, corré `xattr -dr com.apple.quarantine` seguido de la ruta de cada archivo.

En Windows:

1. Copiá `Lacquer.vst3` a `C:\Program Files\Common Files\VST3\`.
2. Reescaneá los plugins en tu DAW.
3. `Lacquer.exe` es la versión standalone y `lacquerrender.exe` el render por línea de comandos.

## Tu primer master en un minuto

Con un archivo (la manera más precisa):

1. Abrí la app standalone (o el plugin en cualquier pista).
2. Apretá **Master a file** y elegí tu mezcla, o soltá el archivo sobre la ventana. Acepta WAV, AIFF, FLAC, Ogg y MP3.
3. Esperá unos segundos mientras se corta el disco. La línea de estado dice qué tan fuerte era la mezcla y qué tan fuerte quedó el master.
4. Apretá **Play**. Hacé clic en cualquier parte del disco para saltar a ese momento de la canción.
5. Probá otro **Style** u otra **Intensity**: Lacquer vuelve a masterizar la canción solo.
6. Apretá **A/B with the original** para comparar a la misma sonoridad.
7. Apretá **Export master** para guardar un WAV de 24 bits y su informe.

En tu DAW:

1. Poné Lacquer en el bus master, último en la cadena.
2. Apretá **Learn** y reproducí la canción desde el principio. Se prende la luz roja y el disco empieza a cortar.
3. Dejá sonar la canción entera para una sonoridad exacta, o por lo menos sus 30 segundos más fuertes para un master rápido. Apretá **Stop** cuando hayas escuchado lo suficiente; Lacquer también para solo cuando la música para.
4. Unos segundos después el master está andando. Exportá la canción desde tu DAW como siempre.

## La interfaz de un vistazo

![La barra superior: Learn, Master a file, la luz roja de corte, la línea de estado y Manual.](img/topbar.jpg)

- **Barra superior**: **Learn** empieza y termina la escucha, **Master a file** abre un archivo, la luz roja está prendida mientras Lacquer escucha o renderiza, la línea de estado te cuenta qué pasó (y explica el control que tengas bajo el mouse), y **Manual** abre este manual.
- **Izquierda**: el disco y, debajo, el transporte del archivo (**Play**, el tiempo, **Export master**, **Back to live**).
- **Derecha**: las cuatro decisiones que son tuyas (estilo, intensidad, objetivo de sonoridad, techo), los lectores, el botón A/B y la vista previa de plataformas.
- **Abajo**: el balance tonal y la lista de lo que hizo cada módulo.

La ventana se puede agrandar desde la esquina de abajo a la derecha.

## Style

| Estilo | Qué hace |
|---|---|
| Balanced | Lleva el tono hacia un balance parejo y neutro y compacta con suavidad. El de fábrica. |
| Warm | Un poco más de cuerpo en los medios graves, agudos más suaves, saturación de cinta leve y glue más lento. |
| Bright | Más aire arriba de 10 kHz, menos barro cerca de 300 Hz, agudos un poco más abiertos. |
| Punchy | Glue con ataque más lento para que pasen bombo y redoblante, un toque de saturación de válvula y un recortador suave en los picos. |
| Loud | Más compresión y recorte: pensado para objetivos fuertes. |
| Gentle | La mitad de la corrección y sin glue a intensidad baja, para mezclas que ya están casi. |

Un estilo no es un preset fijo de perillas. Cambia el objetivo al que apunta el análisis, así que el mismo estilo hace cosas distintas en una mezcla oscura y en una brillante.

![Style, Intensity, Loudness target y Ceiling: las cuatro decisiones que Lacquer te deja a vos.](img/controls.jpg)

## Intensity

**Intensity** decide cuánto cambia Lacquer el tono y la dinámica. No cambia la sonoridad: eso es trabajo del objetivo, así que se puede lograr tanto un master suave y abierto como uno fuerte y denso.

| Intensidad | Corrección tonal | Compresión | Saturación | Ancho |
|---|---|---|---|---|
| 0 % | nada | nada | nada | sin tocar |
| 25 % | 40 % de lo sugerido, hasta 2 dB | leve | la mitad | un cuarto de lo sugerido |
| 50 % (de fábrica) | 60 %, hasta 3 dB | moderada | la del estilo | la mitad |
| 100 % | todo, hasta 4 dB | fuerte | la del estilo y más | todo |

A 0 % Lacquer sólo ajusta la sonoridad y ataja los picos: con el objetivo en **Match** y el techo alto, la salida es idéntica a la entrada.

## Loudness target y Ceiling

| Objetivo | Sonoridad | Techo | Cuándo |
|---|---|---|---|
| Streaming | -14 LUFS | -1 dBTP | Spotify, YouTube, Tidal, Amazon. El de fábrica. |
| Apple | -16 LUFS | -1 dBTP | Apple Music y un sonido más dinámico. |
| Loud | -11 LUFS | -2 dBTP | Más fuerte de lo que reproducen las plataformas: lo van a bajar. |
| Club | -8 LUFS | -2 dBTP | Sets de DJ y equipos de club. |
| CD | -9 LUFS | -0.3 dBTP | Entrega en CD o WAV sin codificar con pérdida. |
| Broadcast | -23 LUFS | -1 dBTP | Radio y TV (EBU R128). |
| Match | la de tu mezcla | -1 dBTP | Sólo tono y glue, sin cambiar el nivel. |

Los servicios de streaming reproducen todas las canciones a más o menos la misma sonoridad: un master más fuerte que -14 LUFS lo bajan, así que ahí no gana nada y pierde dinámica. La línea debajo de los lectores te dice cuánto lo va a bajar cada plataforma.

El **Ceiling** es el pico verdadero más alto permitido. Al elegir un objetivo, el techo va al habitual de ese objetivo; después lo podés cambiar (de -3 a -0.1 dBTP). Dejalo en -1 dBTP o más abajo para todo lo que se vaya a codificar en MP3 o AAC: la codificación agrega picos.

> Si una mezcla no puede llegar a un objetivo fuerte sin destrozarse, Lacquer se queda en un nivel razonable y lo avisa en la línea de estado en vez de aplastar la canción.

## Cómo leer el disco

![El disco: cada vuelta de la espiral es un tramo de la canción y el ancho del surco es su nivel. La intro suave se ve como un surco fino en el borde de afuera.](img/disc.jpg)

La canción va del borde de afuera hacia la etiqueta. Cuanto más ancha la ondulación del surco, más fuerte ese momento: una intro suave es una línea fina, un estribillo una banda ancha, y un master sobrecomprimido un surco ancho y parejo en todas partes.

- Mientras Lacquer escucha, el cabezal de corte avanza hacia adentro y la punta roja brilla.
- En modo archivo, el cabezal marca la posición de reproducción y un clic sobre el surco salta ahí.
- La etiqueta muestra el estilo, la sonoridad del master y su pico verdadero.

## Lectores, A/B y Hear it as

![Los lectores: sonoridad integrada, pico verdadero y rango de sonoridad de la mezcla y del master, el medidor en vivo con el objetivo marcado, cuánto lo bajan las plataformas, A/B y Hear it as.](img/readouts.jpg)

- **Mix in** y **Master** muestran la sonoridad integrada (LUFS), el pico verdadero (dBTP) y el rango de sonoridad (LU) de lo que Lacquer analizó y del resultado.
- **Live out** es la sonoridad momentánea de la salida (la marca brillante) y la de corto plazo (la barra), con el objetivo marcado.
- **A/B with the original** reproduce tu mezcla subida o bajada a la sonoridad del master (el botón dice cuánto). Lo más fuerte siempre suena mejor al principio, así que ésta es la única comparación justa.
- **Hear it as** reproduce el master como lo haría una plataforma: **Spotify** lo baja a -14 LUFS si es más fuerte, **Apple** a -16, **Broadcast** a -23. El A/B sigue igualado en todos los casos.

## Balance tonal

![Balance tonal: la banda objetivo del estilo, tu mezcla (gris), el master (ámbar) y la curva de EQ que aplicó Lacquer.](img/balance.jpg)

La banda verde es el objetivo del estilo con su tolerancia (angosta en los medios, más ancha en graves y agudos, donde la música varía más). La línea gris es tu mezcla y la ámbar el master, las dos alineadas con el objetivo como las compara la EQ automática. La línea negra de abajo es la EQ que aplicó Lacquer (de -6 a +6 dB).

No esperes que la línea ámbar quede sobre el objetivo: Lacquer sólo corrige lo que está claramente fuera de la banda, y nunca más que los límites de la intensidad.

## Qué hizo Lacquer

![Qué hizo Lacquer, módulo por módulo, con la reducción en vivo de los dinámicos.](img/modules.jpg)

La cadena va en este orden. Cada línea muestra qué está haciendo el módulo, o por qué está apagado; las barras de la derecha muestran la reducción de ganancia en vivo.

- **Tone EQ**: un estante grave, seis campanas a una octava una de otra y un estante agudo, ajustados a la curva de corrección. Cambia el tono sin cambiar la sonoridad. Nunca realza por encima de un corte de agudos que detecte.
- **Harshness**: una banda dinámica entre 2 y 5 kHz sobre la resonancia más marcada, que sólo recorta cuando esa zona sobresale. Queda apagada si la mezcla es oscura ahí.
- **De-esser**: la misma idea entre 5 y 10 kHz, para la sibilancia y los platillos filosos.
- **Multiband**: tres bandas (graves, medios, agudos) de compresión suave en los momentos más fuertes. Cuando no comprime, la señal pasa intacta.
- **Glue**: un compresor lento y de relación baja sobre toda la mezcla, que la compacta. Ignora los graves profundos, así el bombo no hace bombear la mezcla.
- **Stereo**: pone los graves en mono por debajo de unos 120 Hz (clave para vinilo, clubes y parlantes de teléfono) y ajusta el ancho de los costados cuando la mezcla es claramente muy angosta o muy ancha. Nunca abre una mezcla que ya tiene problemas de fase.
- **Saturation**: cinta (con preénfasis, agudos más suaves) o válvula (más segundo armónico), sobremuestreada por cuatro para no agregar aliasing.
- **Clipper**: redondea los transitorios más altos antes del limitador, también sobremuestreado. Sólo aparece cuando el objetivo pide más de lo que el limitador solo puede hacer limpio.
- **Limiter**: un limitador de pico verdadero con anticipación. Detecta los picos con un interpolador por ocho, así el master no pasa el techo ni siquiera después de codificarlo en MP3 o AAC. Su liberación es rápida para picos aislados y lenta para la reducción sostenida, lo que evita el bombeo.

![Estilo Loud con el objetivo Club (-8 LUFS): el recortador y el limitador trabajan mucho más, y la línea de abajo dice qué va a hacer el streaming con eso.](img/loud-club.jpg)

## Masterizar un archivo

**Master a file** (o soltar un archivo sobre la ventana) pasa Lacquer a modo archivo. Mientras hay un archivo abierto, la entrada del plugin se silencia: lo que escuchás es el archivo.

- El análisis abarca la canción entera, así que la sonoridad es exacta.
- Cambiar estilo, intensidad, objetivo o techo vuelve a renderizar la canción (unos segundos para una canción típica).
- **Export master** escribe un WAV de 24 bits a la frecuencia de muestreo del archivo y, al lado, un informe `.json`: cada medición de la mezcla y del master, cada ajuste de cada módulo y las notas. Con ese informe se puede reproducir el mismo master exacto.
- **Back to live** cierra el archivo y vuelve a procesar la entrada del plugin.

## Learn en el DAW

Learn graba lo que entra al plugin mientras reproducís la canción, salteando los silencios, hasta seis minutos. Reproducir la canción entera da la misma sonoridad exacta que el modo archivo. Reproducir sólo los 30 segundos más fuertes es más rápido; en ese caso, la canción entera va a medir un poco debajo del objetivo si tiene partes suaves, porque Lacquer fijó el nivel con la parte fuerte.

- Cambiar estilo, intensidad, objetivo o techo después de aprender vuelve a analizar el audio guardado, así que el cambio es exacto.
- Los ajustes aprendidos se guardan con tu proyecto. El audio aprendido no (haría proyectos enormes): al reabrir un proyecto podés cambiar el objetivo y el techo, pero un estilo o una intensidad nuevos necesitan Learn otra vez.
- Si cambiás la mezcla después de aprender, apretá Learn de nuevo. Lacquer nunca sigue a la mezcla solo: un master que cambia mientras escuchás sería imposible de juzgar.

## El render por línea de comandos

`lacquerrender` corre el mismo motor sobre un archivo WAV, sin ventana. Sirve para lotes y scripts.

`lacquerrender mezcla.wav master.wav --style warm --intensity 60 --target streaming --report master.json`

| Opción | Valores |
|---|---|
| --style | balanced, warm, bright, punchy, loud, gentle |
| --intensity | 0 a 100 (50 por defecto) |
| --target | streaming, apple, loud, club, cd, r128, match |
| --lufs | cualquier sonoridad, en vez de un objetivo |
| --ceiling | -6 a 0 dBTP |
| --bits | 16 (con dither), 24 (por defecto) o 32 (float) |
| --report | ruta del informe JSON |

La salida del render y la de Export master son idénticas con los mismos ajustes.

## Cómo sacarle el mejor resultado

- **Dejá margen en la mezcla.** Picos cerca de -6 dBFS y nada de limitador en el bus de mezcla le dan lugar a Lacquer. Una mezcla ya limitada sólo puede quedar más plana.
- **Arreglá el balance en la mezcla.** Si la voz está baja o el bajo retumba en una nota, el mastering no lo arregla sin efectos secundarios.
- **Lacquer va último.** Cualquier cosa después (un medidor está bien) puede llevar los picos por encima del techo.
- **Compará a la misma sonoridad.** Usá A/B, y Hear it as Spotify para escuchar lo que va a escuchar la gente.
- **Empezá en 50 % y movete de a 10 o 20 %.** Si te gusta el tono pero lo querés más abierto, bajá la intensidad antes de cambiar de estilo.
- **Mezclas recortadas.** Si la línea de estado dice que la entrada tiene pasajes recortados, exportá la mezcla de nuevo con más margen: el mastering no deshace el recorte.

## Límites

- La masterización automática da un buen master para la mayoría de las mezclas, no reemplaza a un ingeniero en una difícil. Lacquer no puede escuchar el género, la intención artística ni un instrumento mal balanceado.
- Learn sobre una parte de la canción fija el nivel con esa parte; para una entrega exacta usá la canción entera o el modo archivo.
- Los rangos dinámicos muy amplios (música clásica, bandas de sonido) se respetan, no se aplastan. Para esos suele ir mejor un objetivo más bajo, como Apple o Broadcast.

## Notas técnicas

- Sonoridad y pico verdadero: ITU-R BS.1770-5, probados con las señales de EBU Tech 3341 y 3342 a 44.1, 48 y 96 kHz. Rango de sonoridad según EBU Tech 3342.
- Limitador: anticipación de 3 ms, detección de picos por ocho, medido con un interpolador independiente para quedar a menos de 0.02 dB del techo.
- Saturación y recortador: sobremuestreo por cuatro con filtros de media banda, aliasing debajo de -90 dBFS.
- Latencia: 275 muestras a 48 kHz (5.7 ms), constante, informada al host.
- CPU: menos de 5 % de un núcleo a 48 kHz con todos los módulos activos (medido en Apple serie M).
- El mismo archivo con los mismos ajustes da el mismo master bit a bit, sea cual sea el tamaño de bloque.
- Motor basado en investigación publicada: Giannoulis, Massberg y Reiss sobre diseño de compresores; Pestana y otros sobre el espectro de la música comercial; Ma, Reiss y Black sobre ecualización automática; los filtros de BS.1770 como los deriva libebur128.
- Texto en Barlow Condensed (SIL Open Font License).
