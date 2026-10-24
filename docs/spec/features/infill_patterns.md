# Patrones de relleno, combinación de capas y espesor de la cáscara

**Qué hace:** todo lo del relleno que no está en `surfaces_infill.md`:
- los demás patrones del relleno poco denso (Tisma Slicer usa `stars` por defecto);
- los demás patrones de las superficies de arriba y de abajo;
- combinar el relleno de varias capas en una más gruesa;
- forzar una capa sólida cada N capas;
- un espesor mínimo de la cáscara de arriba y de abajo.

**Fase:** después de F3.

**Fuentes:**
- curva de Hilbert: D. Hilbert, «Über die stetige Abbildung einer Linie auf ein Flächenstück», Math. Ann. 38 (1891);
- espiral de Arquímedes: `r = a·θ`;
- la geometría de los cubos, panales y octaedros truncados es geometría clásica;
- el comportamiento sale de la documentación de usuario de los slicers y de las mediciones de abajo.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `infill.pattern` | — | `stars` en Tisma Slicer | Además de `rectilinear`, `grid` y `gyroid`: `aligned_rectilinear`, `line`, `zigzag`, `triangles`, `stars`, `cubic`, `adaptive_cubic`, `support_cubic`, `lightning`, `honeycomb`, `honeycomb_3d`, `hilbert`, `archimedean`, `octagram`, `concentric` |
| `surfaces.top_pattern`, `surfaces.bottom_pattern` | — | `monotonic` | Además de `rectilinear` y `monotonic`: `monotonic_lines`, `aligned_rectilinear`, `concentric`, `hilbert`, `archimedean`, `octagram` |
| `infill.combine_layers` | capas | 1 | Imprime el relleno poco denso cada N capas, con el alto de las N |
| `infill.combine_auto` | — | no | Elige solo N, el mayor que no pase `infill.combine_max_height` |
| `infill.combine_max_height` | mm o % de la boquilla | 100 % | Alto máximo de la capa de relleno combinada |
| `infill.solid_every` | capas | 0 = nunca | Una capa de relleno sólido cada N capas |
| `surfaces.top_min_thickness`, `surfaces.bottom_min_thickness` | mm | 0 | Suma capas sólidas hasta llegar a este espesor |

## Reglas

### Patrones del relleno poco denso

Con `θ` el ángulo del relleno (`infill.angle`) y la separación elegida para que la densidad sea la pedida:

1. **`aligned_rectilinear`:** como `rectilinear`, pero todas las capas en la dirección `θ`; no gira 90° (prueba P2).
2. **`zigzag`:** en Tisma Slicer da exactamente lo mismo que `rectilinear` (prueba P1).
3. **`line`:** líneas como las de `rectilinear`, pero no paralelas: una de cada dos se inclina unos 6,7° (135° y
   128,3° en la prueba P3).
4. **`triangles`:** tres familias de líneas a `θ`, `θ + 60°` y `θ + 120°` en todas las capas, que se cruzan en los
   mismos puntos y forman triángulos (prueba P4).
5. **`stars`:** las mismas tres familias, pero una corrida media separación, así no se cruzan en un punto común y
   forman estrellas de seis puntas (prueba P4).
6. **`cubic`:** una red de cubos apoyados sobre un vértice. En cada capa se ven las mismas tres familias de `stars`,
   pero cada familia se corre con la altura: `z / √2` en su dirección normal, así las líneas de capas seguidas forman
   las caras de los cubos. El largo por capa cambia con Z (prueba P5).
7. **`adaptive_cubic`:** cubos más chicos cerca de las paredes y de las superficies, y más grandes adentro (un octree).
   En Tisma Slicer usa las direcciones 0°, 60° y 120° sin importar `θ` (prueba P6).
8. **`support_cubic`:** como `adaptive_cubic`, pero solo donde hace falta para sostener las superficies de arriba. En
   un cubo, toda la superficie de arriba necesita apoyo y da lo mismo que `adaptive_cubic` (prueba P6).
9. **`lightning`:** solo lo necesario para sostener las superficies de arriba. Son ramas que bajan desde la superficie
   y se juntan como un árbol: arriba hay mucho y abajo casi nada (prueba P7).
10. **`honeycomb`:** panales hexagonales que se repiten en todas las capas. Cada capa imprime en zigzag una de las
    tres direcciones de los lados (`θ − 30°`, `θ + 30°`, `θ + 90°`) como la principal (la mitad del largo); la
    dirección principal rota entre las tres, capa por capa (prueba P8).
11. **`honeycomb_3d`:** una red de octaedros truncados: en cada capa, líneas a 0° o 90° con tramos a 45° y 135° cuya
    parte cambia con la altura (prueba P8).
12. **`hilbert`, `archimedean`, `octagram`:** curvas que llenan el plano, recortadas por la región: la curva de
    Hilbert; cuerdas de una espiral de Arquímedes; y una espiral con forma de estrella de ocho puntas. Iguales en
    todas las capas (prueba P9).
13. **`concentric`:** lazos paralelos al borde de la región, hacia adentro, a la separación de la densidad. Con
    `walls.generator` `fixed`, de ancho fijo (prueba P9).

### Patrones de las superficies

14. **`monotonic_lines`:** como `monotonic`, pero cada línea es un tramo aparte, sin unir con la siguiente; todas en
    el mismo sentido (prueba P10).
15. **`aligned_rectilinear`:** no gira 90° entre capas, como en el relleno.
16. **`concentric`, `hilbert`, `archimedean`, `octagram`:** las mismas curvas de las reglas 12 y 13, densas.
17. Solo cambian la capa de arriba de todo y la de abajo de todo; las capas sólidas intermedias siguen como siempre.

### Combinar capas

18. **Combinar:** con `infill.combine_layers` = N, el relleno poco denso se imprime solo en una de cada N capas, con
    el alto de las N juntas y el filamento de ese alto. Las otras capas no llevan relleno poco denso (prueba P11).
19. **El alto combinado nunca pasa el diámetro de la boquilla:** con boquilla de 0,4 y capas de 0,2 se combinan como
    mucho 2 capas, aunque `infill.combine_max_height` permita más (prueba P11).
20. **Automático:** con `infill.combine_auto`, N es la mayor cantidad de capas que entra en `infill.combine_max_height`
    (y en la regla 19).

### Capas sólidas

21. **Una capa sólida cada N:** con `infill.solid_every` = N, las capas N, 2N, 3N… (contadas desde 0) llevan relleno
    sólido en toda la región de relleno. Como debajo hay relleno poco denso, se imprimen como puente interno
    (`;TYPE:Bridge infill`) (prueba P12).
22. **Espesor mínimo:** si las capas sólidas de arriba (o de abajo) suman menos que el espesor mínimo, se agregan
    capas hasta llegar (prueba P13).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. **Pieza:** cubo de 20 mm. **Ajustes base:** boquilla 0,4,
capas de 0,2 mm, 2 perímetros, 3 capas sólidas arriba y abajo, relleno al 20 %, `θ` 45°, sin falda ni borde. Los
largos son los del relleno poco denso de la capa; las direcciones, en % del largo (tramos de 0,3 mm o más).

| # | Patrón | Resultado |
|---|---|---|
| P1 | `rectilinear` y `zigzag` | Iguales: 197,3 mm por capa en un solo tramo; 82 % a 135° en las capas impares y a 45° en las pares, el resto en los bordes |
| P2 | `aligned_rectilinear` | 197,3 mm, 82 % a 45° en todas las capas |
| P3 | `line` | 185,9 mm en un tramo; líneas alternadas a 135° y 128,3° (capas pares: 45° y 51,7°) |
| P4 | `triangles` / `stars` | Tres direcciones, 45°, 105° y 165°, con un 28–30 % cada una, en todas las capas. `triangles` 196,9 mm en un tramo; `stars` 187,2 mm en dos tramos |
| P5 | `cubic` | Las mismas tres direcciones; el largo cambia con la capa: 184,1 (capa 5), 181,9 (6), 179,8 (7), 182,8 (20), 197,0 (40), 191,6 (45) |
| P6 | `adaptive_cubic` y `support_cubic` | Iguales en el cubo: direcciones 0°, 60° y 120°; 148 a 153 mm por capa (cerca del 76 % de `cubic`) |
| P7 | `lightning` | Crece hacia arriba: 18,9 mm en la capa 5, 39,2 en la 7, 51,4 en la 20, 93,8 en la 40 y 134,2 en la 45, de 2 a 18 tramos |
| P8 | `honeycomb` / `honeycomb_3d` | `honeycomb`: 229 a 244 mm; la dirección principal (49–51 %) pasa por 15°, 135° y 75° en las capas 5, 6 y 7 y se repite. `honeycomb_3d`: 190 a 209 mm en un tramo; capa 5: 55 % a 0° y 20 % a 45° y a 135°; capa 6: 43 % a 90°; capa 45: 78 % a 0° |
| P9 | `hilbert`, `archimedean`, `octagram`, `concentric` | Iguales en todas las capas. `hilbert` 206,3 mm en 3 tramos (0°, 45°, 90°, 135°). `archimedean` 194,3 mm en 4 tramos, todas las direcciones. `octagram` 208,3 mm en 8 tramos (0°, 90°, 45°, 135°). `concentric` 199,4 mm en 5 lazos |
| P10 | Superficie de arriba (capa 49) y de abajo (capa 0) | `rectilinear` y `monotonic` iguales en el cubo: 956,3 mm en un tramo a 135° (abajo 978,1 a 45°). `monotonic_lines`: 952,5 mm en 71 tramos, todos a 135°. `aligned_rectilinear`: 956,3 mm a 45°. `concentric`: 960,3 mm en 26 lazos. `hilbert`: 925,1 mm en 71 tramos. `archimedean`: 925,2 mm en 43. `octagram`: 923,9 mm en 88 |
| P11 | Relleno `rectilinear`; `infill.combine_layers` 2 | Relleno poco denso solo en las capas pares desde la 4 (Z 1,0, 1,4…), con `;HEIGHT:0,4` y 0,06056 mm de filamento por mm (0,03385 con capas de 0,2). La capa 3 (Z 0,8) queda sin relleno poco denso. Con `infill.combine_auto` y máximo 100 % o 0,6 mm, igual que con 2 |
| P12 | `infill.solid_every` 5 | Capas 5, 10, 15 … 45: `Bridge infill` de 759,2 mm, 0,05274 mm de filamento por mm |
| P13 | 1 capa sólida arriba y abajo; mínimos de 1 mm arriba y 0,6 abajo | Abajo, capas 0 a 2 sólidas (0,6 mm). Arriba, capas 45 a 49 (1 mm): la 45 puente interno, la 46 a 48 sólidas, la 49 superficie de arriba |
