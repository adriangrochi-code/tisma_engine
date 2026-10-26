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

## Para repetir la medición

La misma tabla con el motor nuevo, en el mismo equipo. El laminado tiene que dar el mismo G-code antes y después de
optimizar, o la diferencia se explica.
