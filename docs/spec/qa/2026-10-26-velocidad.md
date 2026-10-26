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

## Qué hacer

| # | Qué | Por qué | Ganancia esperada en el Benchy |
|---|---|---|---|
| V1 | Revisar el cálculo de las superficies de abajo: corre en un solo núcleo y cuesta casi lo mismo con 1 capa que con 3 | Es el 85 % del tiempo | De 6,3 s a cerca de 1 s si se lleva al costo de las de arriba |
| V2 | Si una capa solo necesita la de abajo ya cortada, hacerlo en paralelo por capa, como el resto | El paso que mira las capas vecinas queda fuera del paso en paralelo (`docs/ARCHITECTURE.md` del motor) | Hasta 4 veces en 4 núcleos |
| V3 | Un G-code más chico: el del motor pesa 3,3 veces el de Tisma Slicer para el mismo Benchy (7,57 MB contra 2,29 MB) | Menos para escribir, guardar y mandar (`model_library.md`, regla 16) | Poco en tiempo; mucho en espacio |

Con V1 y V2, el motor quedaría más rápido que Tisma Slicer: 0,84 s sin capas sólidas contra 3,3 s.

## Para repetir la medición

La misma tabla con el motor nuevo, en el mismo equipo. El laminado tiene que dar el mismo G-code antes y después de
optimizar, o la diferencia se explica.
