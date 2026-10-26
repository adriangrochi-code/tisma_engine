# Números de referencia (Tisma Slicer como caja negra)

## De dónde salen

- **Programa:** Tisma Slicer (rama `main` del 24-10-2026), desde la línea de comandos, con los ajustes por defecto.
  Sus valores principales están en la columna «Banco» de `SETTINGS_V1.md`.
- **Máquina:** 4 núcleos, GCC 13, `-O3`.
- **Tiempo:** mediana de 3 corridas.

## Modelos

- **Benchy:** [3dbenchy.com](https://www.3dbenchy.com). Revisar su licencia antes de agregarlo al repositorio; si no,
  descargarlo en el CI.
- **Conejo de Stanford:** del Stanford 3D Scanning Repository.
- **Tornillo M3×10.**

**Pruebas propias:** hacerlas con piezas generadas por código (cubos, cilindros, paredes finas, voladizos, puentes),
que no tienen problemas de licencia.

## Tiempo de laminado (segundos)

| Caso | Total | Corte | Perímetros | Sólidos | Cáscaras | Sup. externas | Puentes | Relleno y soportes | G-code |
|---|---|---|---|---|---|---|---|---|---|
| Benchy | 3,08 | 0,25 | 0,39 | 0,23 | 0,14 | 0,21 | 0,14 | 0,32 | 0,95 |
| Benchy ×2, giroide 15 % | 5,53 | 0,41 | 0,87 | 0,42 | 0,33 | 0,21 | 0,35 | 1,00 | 1,51 |
| Benchy con soportes | 5,12 | 0,24 | 0,33 | 0,21 | 0,13 | 0,19 | 0,14 | 2,23 | 1,19 |
| Benchy, soportes orgánicos | 3,81 | 0,25 | 0,34 | 0,21 | 0,13 | 0,19 | 0,14 | 1,19 | 0,96 |
| Conejo ×1,5 | 1,72 | 0,02 | 0,33 | 0,04 | 0,10 | 0,02 | 0,06 | 0,52 | 0,49 |
| Tornillo ×4, relleno 40 % | 0,85 | 0,02 | 0,10 | 0,02 | 0,03 | 0,00 | 0,01 | 0,12 | 0,45 |

## Dónde se va el tiempo

Perfil de instrucciones del Benchy con soportes, en un núcleo:

| Parte | Peso |
|---|---|
| Operaciones con polígonos (uniones, diferencias, offsets) | ~53 % |
| Soportes | 24 % |
| Rayos para la visibilidad de la costura | 15 % |
| Perímetros de ancho variable | ~7 % |

Lecciones para el diseño:
- conviene evitar offsets repetidos sobre contornos con muchos puntos;
- las etapas tienen que repartirse entre núcleos;
- la etapa que menos aprovechaba los 4 núcleos fue la propagación de soportes hacia abajo (1,7 veces más rápida que
  con un núcleo).

## Resultado

| Caso | Capas | Filamento (mm) | Tiempo estimado | Líneas de G-code |
|---|---|---|---|---|
| Benchy | 160 | 4 440 | 1 h 7 m 36 s | 93 148 |
| Benchy ×2, giroide 15 % | 320 | 24 269 | 4 h 54 m 52 s | 442 106 |
| Benchy con soportes | 302 | 7 522 | 1 h 58 m 39 s | 148 864 |
| Benchy, soportes orgánicos | 160 | 5 765 | 1 h 28 m 53 s | 151 931 |
| Conejo ×1,5 | 536 | 103 223 | 11 h 31 m 8 s | 241 482 |
| Tornillo ×4, relleno 40 % | 173 | 2 653 | 41 m 59 s | 44 896 |

El motor no tiene que dar los mismos números. Las diferencias de filamento o de tiempo de más de unos pocos por ciento
se explican en `PROGRESS.md`.

## Metas de tiempo del motor (aprobadas por el dueño, 26-10-2026)

Salen de las propuestas V7 a V14 del informe `qa/2026-10-26-velocidad.md`. Se miden en un equipo de 4 núcleos, con la
mediana de 3 corridas, contra el motor de `dev` en `1ffbdeb` (Benchy en 6,15 s). Cada una es una prueba que tiene que
pasar el motor; cómo se logra lo decide el equipo del motor.

| # | Propuesta | Caso | Hoy | Meta |
|---|---|---|---|---|
| RV1 | V7, V8, V9 | Benchy con los ajustes por defecto | 6,15 s | 1,5 s o menos |
| RV2 | V7 | Benchy con `slicing.simplify_tolerance` 0 y 0,005: lo que suman las capas de abajo (`bottom_layers` 3 contra 0) | 200 s y 8,4 s | No más del 25 % del tiempo con `bottom_layers` 0 |
| RV3 | V8, V9 | Benchy con `walls.count` de 1 a 6: lo que suman las capas de abajo | De 1,7 a 6,7 s, crece con las paredes | No más de 0,3 s con cualquier cantidad de paredes |
| RV4 | V10 | Cilindro de Ø 60 mm con 32 768 lados, de 40 y de 80 mm de alto (200 y 400 capas iguales) | 2,2 s a 40 mm | El de 80 mm tarda como mucho un 25 % más que el de 40 mm |
| RV5 | V11 | Benchy laminado; se cambia solo `speed.infill` y se lamina de nuevo en el mismo programa | Se lamina todo: 6 s | 0,5 s o menos |
| RV6 | Todas | El G-code de los casos de esta página antes y después de cada cambio | — | Igual byte a byte, o la diferencia se explica en `PROGRESS.md` (por ejemplo, V9 puede mover vértices menos que `simplify_tolerance`) |
| RV7 | V7, V12 | Conejo al 150 % con `infill.density` 0 y con `walls.count` 1 | 187,4 s y 50,9 s | Como mucho 1,25 veces el tiempo con los valores por defecto |
| RV8 | V5, V13 | Conejo al 150 %: el armado del G-code | 3,2 s | 1 s o menos |
| RV9 | V14 | Benchy con 4 núcleos contra 1 núcleo | 1,38 veces | 2,5 veces o más |
| RV10 | Costura en la GPU (`perimeters.md`, regla 8) | Conejo al 150 % con `walls.seam` `aligned` contra `nearest`, con GPU y con `compute.gpu` en `off` | 3,3 s contra 1,28 s (2,6 veces) | Como mucho 1,2 veces, y el mismo G-code con GPU y sin ella |
| RT1 | Recorrido (`features/toolpath_time.md`) | Benchy: viajes de menos de 1 mm | 4 392 | 2 100 o menos |
| RT2 | Recorrido, regla 1 | Benchy: viajes entre líneas de superficie de arriba y de puente | 807 | 100 o menos |
| RT3 | Recorrido, reglas 2 y 6 | Conejo al 150 %: largo de los viajes | 243,6 m | 190 m o menos |
| RT4 | Recorrido, todas | Benchy: tiempo estimado con el mismo perfil | 1 h 12 m 28 s | Al menos 8 % menos |

- Todas están aprobadas por el dueño: RV1 a RV6 y RV10 el 26-10-2026, y RV7 a RV9 el mismo día, después de medirlas.
  RT1 a RT4 salen de su pedido del mismo día: bajar el tiempo de impresión solo con el recorrido, sin tocar el
  perfil. Se miden contra el motor de `dev` en `5b0bdbd`.
- RV5 compara además el G-code del relaminado por etapas con el de un laminado completo con los mismos ajustes: tienen
  que ser iguales byte a byte.
- Las metas no bajan la calidad: los casos de `GCODE_OUTPUT.md` y las pruebas de las fichas siguen pasando.

