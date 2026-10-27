# Velocidad del motor contra Tisma Slicer (26-10-2026)

**Qué es:** el equipo de fichas midió el motor como caja negra para saber por qué lamina más lento que Tisma Slicer.
No se leyó su código.

**Pedido del dueño:** alcanzar el ritmo de Tisma Slicer, o algo más rápido.

**Cómo:**
- motor de `dev` en `d3e814a`, compilado en Linux, versión Release;
- Tisma Slicer de su rama `dev`, compilado igual;
- contenedor Linux de 4 núcleos y 15 GB de memoria; mediana de 3 corridas, salvo donde se dice;
- piezas de `resources/shapes` de Tisma Slicer: Benchy (225 102 triángulos), conejo y tornillo M3; cada programa con
  sus ajustes por defecto.

## Tiempos

| Pieza | Tisma Slicer | Motor |
|---|---|---|
| Benchy | 3,3 s | 7,0 s |
| Benchy al doble, gyroid 15 % | 5,8 s | 17,8 s |
| Benchy con soportes | 5,5 s | 6,1 s |
| Conejo al 150 % | 1,9 s | 3,4 s |
| Tornillo M3 al 400 % | 1,0 s | 0,3 s |

**Ojo con la comparación:** con sus valores por defecto, Tisma Slicer lamina el Benchy en capas de 0,3 mm (160 capas)
y el motor en 0,2 mm (240 capas). Con las mismas capas de 0,2 mm:

| Benchy, capas de 0,2 mm | Tiempo | CPU | G-code | Movimientos `G1` |
|---|---|---|---|---|
| Tisma Slicer | 4,39 s | 11,9 s | 3,08 MB | 99 480 |
| Motor | 6,07 s | 7,0 s | 7,57 MB | 202 429 |

El motor gasta menos CPU que Tisma Slicer para la misma pieza; tarda más porque usa casi un solo núcleo.

## Por qué

1. **El trabajo total es parecido; el motor usa un solo núcleo.** En el Benchy, Tisma Slicer gasta 8,9 s de CPU en
   3,4 s (2,65 núcleos en promedio). El motor gasta 8,2 s de CPU en 7,3 s (1,14 núcleos).
2. **La línea de tiempo del motor** (uso de CPU cada 0,25 s, Benchy):
   - de 0 a 0,6 s, de 2 a 3,4 núcleos;
   - de 0,6 a 6,0 s, 1 núcleo;
   - al final, el G-code se escribe de una vez.
3. **Lo que no es:**
   - leer la malla: `tisma-engine info` tarda 0,1 s;
   - escribir el G-code: con la salida a `/dev/null` tarda 0,4 s menos;
   - la GPU: con `compute.gpu` en `off` tarda lo mismo.
4. **Lo que es: las capas sólidas de abajo.** Cambiando un ajuste por vez:

| Benchy, `surfaces.top_layers`, `surfaces.bottom_layers` | Tiempo |
|---|---|
| 0, 0 | 0,84 s |
| 3, 0 | 0,86 s |
| 0, 3 | 5,78 s |
| 1, 1 | 5,12 s |
| 3, 3 (por defecto) | 6,26 s |
| 6, 6 | 6,09 s |

   - con `bottom_layers` en 1 o más, se suman unos 5 s, casi sin depender de cuántas capas sean;
   - las capas de arriba casi no cuestan;
   - el Benchy al doble con gyroid baja de 17,9 s a 3,3 s sin capas sólidas;
   - otros ajustes cambian poco: relleno de 0 % a 50 %, de 6,0 s a 6,4 s; capas de 0,3 mm, 6,2 s; capas de 0,1 mm,
     7,3 s;
   - las paredes sí: 1 pared, 2,4 s; 4 paredes, 7,4 s. Puede ser porque el área que se compara entre capas depende de
     las paredes.

5. **Sin las capas de abajo, lo que queda** (Benchy con `bottom_layers` en 0: 0,93 s; uso de CPU cada 0,05 s):
   - de 0 a 0,15 s, 1 núcleo (leer y preparar la malla);
   - de 0,15 a 0,5 s, 4 núcleos (el resto del laminado);
   - de 0,5 a 0,85 s, 1 núcleo, y recién al final se escribe el archivo: el armado del G-code va en serie.
6. **Por qué el G-code pesa 2,5 veces más** que el de Tisma Slicer con las mismas capas:
   - `;WIDTH` va en 123 717 líneas (1,61 MB, el 21 % del archivo): casi en cada movimiento. En Tisma Slicer, solo
     cuando cambia el ancho (15 855 líneas);
   - el motor hace 2 veces más movimientos (202 429 contra 99 480). Parece que no simplifica los tramos muy cortos
     de las curvas (Tisma Slicer junta los tramos que se apartan menos que una resolución fija).

## Qué hacer

| # | Qué | Por qué | Ganancia esperada en el Benchy |
|---|---|---|---|
| V1 | Revisar el cálculo de las superficies de abajo: corre en un solo núcleo y cuesta casi lo mismo con 1 capa que con 3 | Es el 85 % del tiempo | De 6,3 s a cerca de 1 s si se lleva al costo de las de arriba |
| V2 | Si una capa solo necesita la de abajo ya cortada, hacerlo en paralelo por capa, como el resto | El paso que mira las capas vecinas queda fuera del paso en paralelo (`docs/ARCHITECTURE.md` del motor) | Hasta 4 veces en 4 núcleos |
| V3 | `;WIDTH` solo cuando cambia el ancho | 21 % del archivo (regla 6) | −1,6 MB en el Benchy |
| V4 | Simplificar las curvas: juntar tramos que se apartan menos que una resolución (por ejemplo 0,0125 mm) | 2 veces más movimientos que Tisma Slicer | Archivo y armado del G-code a la mitad; la impresora recibe menos comandos por segundo |
| V5 | Armar el G-code de cada capa en paralelo y unirlo en orden | 0,35 s en un solo núcleo al final | Unos 0,25 s en el Benchy, más en piezas grandes |
| V6 | En el servidor, un laminado por núcleo | Cada laminado del motor hoy usa casi un núcleo | Medido: 4 a la vez en 4 núcleos, 1737 Benchys por hora contra 1462 de Tisma Slicer (con sus capas de 0,3 mm) |

Con V1 y V2, el motor quedaría cerca de 1 s contra 4,4 s de Tisma Slicer con las mismas capas: unas 4 veces más
rápido. V3 y V4 bajan el G-code a menos de la mitad, que ayuda al guardado de la biblioteca y al envío a las
impresoras.

## Segunda medición: de qué depende el costo de las capas de abajo

**Cómo:** motor de `dev` en `1ffbdeb` (0.8.0-alpha.1), mismo equipo. Una corrida por caso salvo el Benchy base
(mediana de 3). El Benchy base sigue igual: 6,15 s.

1. **No depende del área ni de la cantidad de capas sólidas.** En cajas de 20 × 20 a 160 × 160 mm y en cilindros, las
   capas de abajo no suman tiempo (menos de 0,1 s).
2. **Depende del detalle de los contornos, y más que en proporción.** El Benchy cambiando solo
   `slicing.simplify_tolerance`:

| `simplify_tolerance` | Con `bottom_layers` 3 | Con `bottom_layers` 0 |
|---|---|---|
| 0 (sin simplificar) | 207,8 s | 7,5 s |
| 0,005 | 9,3 s | — |
| 0,01 (por defecto) | 6,1 s | 0,9 s |
| 0,02 | 4,4 s | — |
| 0,05 | 3,3 s | — |

   Sin simplificar, el resto del laminado tarda 8 veces más (de 0,9 a 7,5 s), pero las capas de abajo tardan 40
   veces más (de 5,2 a 200 s). Parece un paso que compara cada tramo de un contorno con cada tramo del otro.
3. **Depende de las paredes.** El Benchy con `bottom_layers` 3 y con 0:

| `walls.count` | Con `bottom_layers` 3 | Con `bottom_layers` 0 | Costo de las capas de abajo |
|---|---|---|---|
| 0 (y `top_layers` 0) | 0,74 s | — | casi nada |
| 1 | 2,55 s | — | — |
| 2 | 3,71 s | 0,84 s | 2,9 s |
| 3 | 5,72 s | 0,82 s | 4,9 s |
| 4 | 6,96 s | 0,96 s | 6,0 s |
| 6 | 7,61 s | 0,91 s | 6,7 s |

   Sin paredes el paso es barato aunque el contorno tenga todo su detalle. Puede ser que cada pared, al achicar el
   contorno con esquinas redondeadas, le sume vértices, y que la comparación con la capa de abajo se haga sobre esa
   zona de adentro.
4. **Un modelo muy detallado** (cilindro de Ø 60 mm y 40 mm de alto con 32 768 lados, 131 072 triángulos, 200 capas):
   el motor tarda 2,2 s y Tisma Slicer 352,9 s, una corrida cada uno. Acá el motor es mucho más rápido.
   - el tiempo del motor casi no cambia con `simplify_tolerance` (de 2,4 s con 0,005 a 1,8 s con 0,05);
   - las 200 capas son iguales, y cada una se calcula de nuevo.

## Qué hacer, segunda parte

| # | Qué | Por qué | Ganancia esperada en el Benchy |
|---|---|---|---|
| V7 | Hacer que el paso de las capas de abajo no crezca más que en proporción a los vértices: comparar con la capa de abajo usando un índice de tramos (o cajas por isla), en vez de todos con todos | Sin simplificar tarda 200 s; con 0,01 mm, 5 s (regla 2) | Las capas de abajo cerca del costo de las de arriba: de 6,1 s a cerca de 1 s (lo mismo que V1, por la causa) |
| V8 | Buscar las superficies de abajo en el contorno de la capa (como sale del corte), y recién después recortarlas con la zona de adentro de las paredes | Sin paredes el paso es barato (regla 3) | Igual que V7, si la causa es la zona de adentro |
| V9 | Simplificar con `simplify_tolerance` también después de cada pared, no solo al cortar | Cada pared suma tiempo al paso (regla 3) | Parte de V7, también en las paredes y el relleno |
| V10 | Reusar el resultado de una capa cuando su sección es igual a la de la capa de abajo (piezas prismáticas) | El cilindro de 200 capas iguales se calcula 200 veces (regla 4) | No cambia el Benchy; en piezas prismáticas, cerca de 1 capa en vez de N |
| V11 | En el programa, laminar por etapas: si se cambia solo una velocidad, una temperatura o el ventilador, rehacer solo el G-code, sin volver a cortar ni a calcular las superficies | Con «Slice automatically» (`interface.md`, `preferences.md`), cada cambio relamina todo | Un cambio de velocidad en el Benchy, de 6 s a lo que tarda escribir el G-code (0,3 s) |

Las fichas no piden un algoritmo: piden el resultado. El G-code tiene que salir igual antes y después de cada cambio, o
la diferencia se explica.

## Tercera medición: PrusaSlicer, Tisma Slicer y el motor

**Cómo:**
- PrusaSlicer 2.9.6 compilado desde su etiqueta `version_2.9.6`, sin cambios en el código: solo el ajuste de
  compilación para usar las librerías del sistema, el mismo que usa Tisma Slicer;
- Tisma Slicer de su rama de trabajo del 26-10-2026;
- el motor de `dev` en `1ffbdeb`;
- el mismo equipo de 4 núcleos, mediana de 3 corridas, capas de 0,2 mm y boquilla de 0,4 mm;
- PrusaSlicer y Tisma Slicer desde la línea de comandos con sus valores por defecto; el motor con los ajustes de la
  primera medición.

**Tiempo de laminado:**

| Pieza | PrusaSlicer 2.9.6 | Tisma Slicer | Motor |
|---|---|---|---|
| Benchy | 6,68 s | 3,73 s | 5,82 s |
| Benchy al doble, gyroid 15 % | 8,25 s | 7,56 s | 17,97 s |
| Benchy con soportes | 9,82 s | 6,65 s | 6,78 s |
| Conejo al 150 % | 2,01 s | 2,32 s | 3,60 s |
| Tornillo M3 al 400 %, relleno 40 % | 1,07 s | 1,07 s | 0,27 s |

**Tamaño del G-code y movimientos `G1`:**

| Pieza | PrusaSlicer 2.9.6 | Tisma Slicer | Motor |
|---|---|---|---|
| Benchy | 3,08 MB, 99 501 | 3,08 MB, 99 480 | 7,59 MB, 202 907 |
| Benchy al doble, gyroid 15 % | 16,46 MB, 556 626 | 16,46 MB, 556 625 | 22,78 MB, 750 848 |
| Benchy con soportes | 4,23 MB, 140 652 | 4,23 MB, 140 631 | 7,88 MB, 213 962 |
| Conejo al 150 % | 7,21 MB, 246 355 | 7,21 MB, 246 355 | 6,86 MB, 228 856 |
| Tornillo M3 al 400 % | 1,66 MB, 57 267 | 1,66 MB, 57 267 | 1,64 MB, 54 270 |

1. **Tisma Slicer contra PrusaSlicer:** 1,8 veces más rápido en el Benchy y 1,5 veces con soportes; un 15 % más lento
   en el conejo; igual en el tornillo. Con los valores por defecto, el G-code es casi el mismo.
2. **El motor:** el más rápido en el tornillo (4 veces) y parejo con Tisma Slicer en el Benchy con soportes. Más lento
   en el Benchy (1,6 veces), en el Benchy al doble con gyroid (2,4 veces) y en el conejo (1,6 veces).
3. **El G-code del motor** es hasta 2,5 veces más grande en los Benchy (reglas 6 de la primera medición, V3 y V4) y
   un poco más chico en el conejo y el tornillo.
4. **No se comparan** el tiempo estimado de impresión (el motor usa velocidades de respaldo sin perfil de impresora,
   y los otros dos las suyas) ni el filamento (el motor no lo escribe en el G-code).
5. Con las metas RV1 a RV6 (`BENCHMARKS.md`), esta tabla se repite con el motor nuevo.

## Cuarta medición: núcleos, memoria, casos extremos y robustez

**Cómo:** motor de `dev` en `1ffbdeb`, mismo equipo; con `taskset` para limitar los núcleos; una corrida por caso.

1. **Mismo resultado siempre:** el G-code de las 5 piezas es idéntico byte a byte con 1, 2 y 4 núcleos.
2. **Memoria máxima:** Benchy 130 MB, Benchy al doble 281 MB, Benchy con soportes 133 MB, conejo al 150 % 224 MB,
   tornillo 68 MB. El 1 GB por trabajo que reserva el servidor alcanza con margen.
3. **Núcleos:** el motor casi no gana con más núcleos.

| Pieza | 1 núcleo | 2 núcleos | 4 núcleos | 4 contra 1 |
|---|---|---|---|---|
| Benchy | 8,14 s | 6,31 s | 5,90 s | 1,38 veces |
| Benchy al doble, gyroid | 23,84 s | 18,64 s | 18,95 s | 1,26 veces |
| Benchy con soportes | 7,79 s | 7,05 s | 6,61 s | 1,18 veces |
| Conejo al 150 % | 5,27 s | 4,06 s | 3,87 s | 1,36 veces |
| Tornillo al 400 % | 0,63 s | 0,36 s | 0,25 s | 2,52 veces |

   Sin capas de abajo, el Benchy va de 1,89 s a 0,91 s (2,1 veces) y el Benchy al doble de 6,31 s a 2,61 s (2,4 veces).
4. **El Benchy al doble con gyroid no es por el gyroid:** con rectilíneo tarda 17,9 s y con gyroid 18,9 s; sin capas de
   abajo, 3,0 s, la mitad que Tisma Slicer (7,6 s). Es el mismo paso de la segunda medición.
5. **Casos que se disparan,** siempre por las capas de abajo:

| Conejo al 150 % | Tiempo | Con `bottom_layers` 0 |
|---|---|---|
| Por defecto | 3,4 s | 3,0 s |
| `infill.density` 0 | 187,4 s (165 s en el paso de corte) | 3,3 s |
| `walls.count` 1 | 50,9 s | 1,7 s |
| `infill.density` 5 | 3,1 s | — |

   Una pieza hueca, sin relleno, tarda 55 veces más. Es el caso más grave encontrado.
6. **El armado del G-code** se lleva 3,2 s de los 3,4 s del conejo (`write_ms`), en un solo núcleo, para 241 122 líneas
   (V5).
7. **Robustez:** 21 casos sin un solo cierre inesperado ni cuelgue.
   - archivo vacío, malla abierta, binario cortado, vértice NaN y basura: terminan con un error claro (códigos 2 y 4);
   - caras invertidas, triángulos sin área, dos cubos que se cruzan, STL en texto, una columna de 400 mm y una pieza por
     debajo de la cama: se laminan bien (la pieza baja a la cama);
   - pieza más grande que la cama o lejos de ella, pieza más fina que el cordón: error claro;
   - capas de 0,01 mm, 1000 paredes, relleno gyroid al 100 % y cama vacía: se laminan; JSON roto, boquilla de 0,1 con
     capas de 0,2 y capas de 5 mm: error claro.

## Qué hacer, tercera parte

| # | Qué | Por qué | Meta |
|---|---|---|---|
| V12 | Que una pieza hueca o con una sola pared no dispare el paso de las capas de abajo (parte de V7) | 187 s y 51 s en el conejo (regla 5) | RV7 |
| V13 | Armar el G-code en paralelo por capa y unirlo en orden (V5, ahora medido) | 3,2 s de 3,4 s en el conejo (regla 6) | RV8 |
| V14 | Repartir entre núcleos lo que hoy va en uno, después de V7 | 4 núcleos rinden 1,2 a 1,4 veces lo de 1 (regla 3) | RV9 |

## Quinta medición: después de las mejoras de superficies y cobertura

**Cómo:** motor de `dev` en `fe58b47` (con `519581a`, puentes cerca de las superficies de abajo, y `395b4f2`, cobertura
con un solo offset por trayectoria), mismo equipo, mediana de 3 corridas salvo donde se dice.

| Pieza | Motor en `1ffbdeb` | Motor en `fe58b47` | Tisma Slicer | PrusaSlicer 2.9.6 |
|---|---|---|---|---|
| Benchy | 5,82 s | 1,03 s | 3,73 s | 6,68 s |
| Benchy al doble, gyroid 15 % | 17,97 s | 3,22 s | 7,56 s | 8,25 s |
| Benchy con soportes | 6,78 s | 1,65 s | 6,65 s | 9,82 s |
| Conejo al 150 % | 3,60 s | 3,47 s | 2,32 s | 2,01 s |
| Tornillo M3 al 400 % | 0,27 s | 0,32 s | 1,07 s | 1,07 s |

| Meta (`BENCHMARKS.md`) | Medido | Cumple |
|---|---|---|
| RV1: Benchy en 1,5 s o menos | 1,03 s | Sí |
| RV2: capas de abajo con `simplify_tolerance` 0,005, no más del 25 % | 0,18 s de 1,05 s (17 %) | Sí |
| RV2: lo mismo con `simplify_tolerance` 0 (una corrida) | 1,62 s de 5,78 s (28 %; antes 200 s) | Casi |
| RV3: capas de abajo, no más de 0,3 s con cualquier cantidad de paredes | 1 pared 0,18 s; 3 paredes 0,26 s; 6 paredes 0,34 s | Casi (6 paredes) |
| RV7 (propuesta): pieza hueca, como mucho 1,25 veces | `infill.density` 0: 5,57 s contra 3,07 s (1,81 veces; antes 187 s); `walls.count` 1: 4,55 s (1,48 veces) | No todavía |
| RV8 (propuesta): armado del G-code del conejo, 1 s o menos | El conejo sigue en 3,5 s | No |

- el G-code tiene el mismo tamaño que antes (7,59 MB en el Benchy): V3 y V4 siguen pendientes;
- el conejo es la única pieza en la que el motor sigue más lento que los otros dos. La causa es la costura alineada:

| Pieza | `walls.seam` `aligned` (por defecto) | `nearest` | `rear` |
|---|---|---|---|
| Conejo al 150 % | 3,30 s (`write_ms` 2520) | 1,28 s (`write_ms` 620) | 1,14 s (`write_ms` 538) |
| Benchy | 1,05 s | 1,04 s | 1,01 s |
| Benchy al doble, gyroid | 2,77 s | 2,31 s | 2,23 s |

  En el conejo, ubicar la costura alineada se lleva unos 2 s de 3,3; crece con las paredes (`walls.count` 6:
  `write_ms` 3938). Con `nearest`, el conejo tarda 1,28 s: menos que Tisma Slicer (2,32 s) y que PrusaSlicer (2,01 s).
  Propuesta V15: la costura alineada en la GPU (meta RV10). El dueño la descartó el 26-10-2026: las GPU comunes
  hacen la precisión que necesita entre 32 y 64 veces más lento, y en los núcleos la meta ya se cumplió (1,02 veces).

## Sexta medición: las metas en `dev` `5b0bdbd`

Con V5 (G-code en cadena), V9 (simplificar lo que dejan las paredes), V10 (capas repetidas) y el arreglo de las
piezas huecas. Mismo equipo, mediana de 5 corridas.

| Meta | Medido | Cumple |
|---|---|---|
| RV7: conejo con `infill.density` 0, como mucho 1,25 veces | 1,20 s contra 1,19 s (1,01 veces) | Sí |
| RV7: conejo con `walls.count` 1 | 3,29 s (2,76 veces); sin capas de abajo, 0,87 s | No |
| RV8: armado del G-code del conejo, 1 s o menos | `write_ms` 647 | Sí |
| RV9: Benchy con 4 núcleos contra 1, 2,5 veces o más | 0,93 s contra 2,18 s (2,34 veces; antes 1,38) | Casi |
| RV10: conejo con costura `aligned` contra `nearest`, como mucho 1,2 veces | 1,19 s contra 0,74 s (1,61 veces) | No |

- El conejo con los valores por defecto tarda 1,19 s: menos que Tisma Slicer (2,32 s) y que PrusaSlicer (2,01 s).
- Con una sola pared, el conejo tiene 7 veces más superficie de arriba que con 3 (29 302 contra 4 389 mm²): en sus
  caras inclinadas, menos paredes dejan más superficie al descubierto. Más trabajo real, pero el paso de las capas de
  abajo sigue costando 2,4 s de los 3,3.

## Para repetir la medición

La misma tabla con el motor nuevo, en el mismo equipo. El laminado tiene que dar el mismo G-code antes y después de
optimizar, o la diferencia se explica.
