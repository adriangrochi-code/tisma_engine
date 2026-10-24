# Z anti-aliasing

**Qué hace:** suaviza los escalones de las superficies de arriba inclinadas. Con capas planas, una superficie inclinada
sale como una escalera: cada capa está entera o no está. Donde la superficie del modelo corta una capa (entre su base
y su tope), las extrusiones visibles bajan hasta la superficie y extruyen solo el espesor que queda. Así el techo
sigue al modelo.

**Fase:** después de F4. Lo diseñó y escribió el equipo del dueño en Tisma Slicer.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `zaa.enabled` | — | no | Activa el Z anti-aliasing |
| `zaa.min_thickness` | % de la altura de capa | 25 % | Hasta dónde puede bajar una extrusión: nunca queda más fina que esto |

## Reglas

1. **Qué baja:** las extrusiones que se ven arriba: relleno de arriba de todo, planchado, perímetros y relleno de
   huecos.
2. **Altura:**
   - en cada punto, si la superficie del modelo está entre la base y el tope de la capa, la extrusión va a la Z de la
     superficie;
   - nunca baja más que `(1 − zaa.min_thickness) · h` por debajo del tope (0,15 mm con capas de 0,2 y 25 %).
3. **Material:** se extruye solo el espesor que queda: el material normal × (espesor que queda / `h`).
4. **Forma de los tramos:** se parten para que cada tramo siga la superficie. Un tramo puede llevar X, Y y Z en el
   mismo movimiento.
5. **Requisitos:** coordenadas absolutas en XYZ y sin arcos `G2`/`G3`. No se usa con modo espiral ni impresoras de
   cinta. Con capas no planares sigue el techo dentro de las capas curvas.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. Ajustes base de `gcode_motion.md`.

**Techos:** prismas de 20 × 20 mm en planta, de 3 mm de alto en X = −10 y con el techo inclinado hacia +X con el
ángulo indicado, medido desde la horizontal. La pieza va centrada en X = 100, Y = 100, así que la superficie está a
`Z = 3 + (X − 90) · tan(ángulo)`.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| Z1 | Techo de 5° | `zaa.enabled` sí | En los 1377 tramos paralelos a Y donde la superficie cae dentro de la capa (perímetro externo, perímetros y relleno de arriba), la Z del tramo es `máx(superficie, tope de la capa − 0,15)`, con error de 0,0003 mm. El material por mm sobre el normal es (espesor que queda) / 0,2: error mediano de 0,002. Ejemplo: capa 16 (tope en Z 3,2), tramos en X = 91,78, donde la superficie está en 3,156: van a Z 3,155 con el 77,5 % del material |
| Z2 | Techo de 5° | sin Z anti-aliasing | Ninguna extrusión sale de la Z de su capa |
| Z3 | Techos de 10° y 15° | `zaa.enabled` sí | Solo bajan los perímetros externos: hasta 0,087 mm (10°) y 0,04 mm (15°). La franja de cada capa donde la superficie la corta mide 0,2 / tan(ángulo): 1,13 y 0,75 mm, y la cubren los perímetros |
