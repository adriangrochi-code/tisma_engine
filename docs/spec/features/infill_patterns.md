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
7. **`adaptive_cubic`:** la idea es un octree, con cubos más chicos cerca de las paredes y de las superficies, y más
   grandes adentro. Lo medido en Tisma Slicer:
   - usa las direcciones 0°, 60° y 120° sin importar `θ` (prueba P6);
   - la separación entre líneas paralelas es `165,4 mm / densidad en %` (8,27 mm al 20 %, 4,13 al 40 %): 1,356 veces
     la de `cubic` a la misma densidad, y sin saltos de potencias de 2 (15 % da 11,02 mm);
   - en bloques macizos de 40 × 40 × 20 y 100 × 100 × 50 mm, la separación es la misma en todas las capas: no se
     achica cerca de las paredes, de la tapa ni del fondo. El largo por capa varía con la altura (de 2272 a 4211 mm
     por capa en el bloque grande) porque las líneas de los cubos se cortan a distintas alturas, no por la cercanía a
     las superficies;
   - en total, entre el 66 % (al 40 %) y el 76 % (al 10 %) del largo de `cubic` a la misma densidad (prueba P6b).
   El motor puede hacer el octree de verdad (cubos más chicos solo donde hace falta sostener); la separación de
   arriba es la del cubo más grande.
8. **`support_cubic`:** las mismas líneas que `adaptive_cubic`, pero completas solo debajo de las superficies de
   arriba; más abajo quedan cada vez menos. Medido en un bloque de 40 × 40 × 60 mm (prueba P6c):
   - en los primeros 8 mm debajo de la superficie de arriba, igual que `adaptive_cubic` (97 a 100 % del largo);
   - más abajo baja de a poco: cerca de la mitad entre 16 y 32 mm de profundidad, y entre el 6 y el 20 % cerca de la
     cama. Nunca llega a cero;
   - a más densidad, la parte completa es más corta: al 40 % ya baja al 67 % entre 8 y 12 mm;
   - en total, 50 % del largo de `adaptive_cubic` al 20 % y 41 % al 40 %.
   Una pieza baja o con superficies de arriba en todas las alturas da casi lo mismo que `adaptive_cubic`: un cubo de
   20 mm (prueba P6) y un bloque de 40 × 40 × 10 mm con una columna de 20 × 20 mm hasta los 30 mm (prueba P6d).
   El motor lo hace como un octree con cubos que crecen hacia abajo, empezando por el tamaño de la regla 7 debajo de
   cada superficie de arriba.
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
| P6c | Bloque de 40 × 40 × 60 mm, 3 capas sólidas arriba y abajo; `adaptive_cubic` y `support_cubic` al 20 y 40 % | Largo de `support_cubic` / `adaptive_cubic` por profundidad debajo del relleno de arriba (20 % / 40 %): 0–4 mm 1,00 / 1,00; 4–8 mm 0,97 / 0,92; 8–12 mm 0,93 / 0,67; 12–16 mm 0,72 / 0,51; 16–20 mm 0,53 / 0,43; 28–32 mm 0,46 / 0,36; 40–44 mm 0,23 / 0,18; 56–60 mm 0,12 / 0,06. Total: 80 655 contra 160 024 mm; 97 878 contra 238 733 mm |
| P6d | Bloque de 40 × 40 × 10 mm con una columna de 20 × 20 mm de 10 a 30 mm, 20 % | `support_cubic` y `adaptive_cubic` dan casi lo mismo en cada capa: 439 a 675 mm contra 546 a 606 mm en la base, 131 a 160 mm en la columna |
| P6b | Bloque de 40 × 40 × 20 mm, 3 capas sólidas arriba y abajo; densidad 10, 15, 20, 30 y 40 % | `adaptive_cubic`: separación de 16,54, 11,02, 8,27, 5,51 y 4,13 mm, igual en todas las capas; 308, 439, 568, 847 y 971 mm de relleno por capa en promedio. `cubic`: separación de 12,20, 8,14, 6,10, 4,07 y 3,05 mm; 404, 580, 760, 1120 y 1479 mm por capa |
| P7 | `lightning` | Crece hacia arriba: 18,9 mm en la capa 5, 39,2 en la 7, 51,4 en la 20, 93,8 en la 40 y 134,2 en la 45, de 2 a 18 tramos |
| P8 | `honeycomb` / `honeycomb_3d` | `honeycomb`: 229 a 244 mm; la dirección principal (49–51 %) pasa por 15°, 135° y 75° en las capas 5, 6 y 7 y se repite. `honeycomb_3d`: 190 a 209 mm en un tramo; capa 5: 55 % a 0° y 20 % a 45° y a 135°; capa 6: 43 % a 90°; capa 45: 78 % a 0° |
| P9 | `hilbert`, `archimedean`, `octagram`, `concentric` | Iguales en todas las capas. `hilbert` 206,3 mm en 3 tramos (0°, 45°, 90°, 135°). `archimedean` 194,3 mm en 4 tramos, todas las direcciones. `octagram` 208,3 mm en 8 tramos (0°, 90°, 45°, 135°). `concentric` 199,4 mm en 5 lazos |
| P10 | Superficie de arriba (capa 49) y de abajo (capa 0) | `rectilinear` y `monotonic` iguales en el cubo: 956,3 mm en un tramo a 135° (abajo 978,1 a 45°). `monotonic_lines`: 952,5 mm en 71 tramos, todos a 135°. `aligned_rectilinear`: 956,3 mm a 45°. `concentric`: 960,3 mm en 26 lazos. `hilbert`: 925,1 mm en 71 tramos. `archimedean`: 925,2 mm en 43. `octagram`: 923,9 mm en 88 |
| P11 | Relleno `rectilinear`; `infill.combine_layers` 2 | Relleno poco denso solo en las capas pares desde la 4 (Z 1,0, 1,4…), con `;HEIGHT:0,4` y 0,06056 mm de filamento por mm (0,03385 con capas de 0,2). La capa 3 (Z 0,8) queda sin relleno poco denso. Con `infill.combine_auto` y máximo 100 % o 0,6 mm, igual que con 2 |
| P12 | `infill.solid_every` 5 | Capas 5, 10, 15 … 45: `Bridge infill` de 759,2 mm, 0,05274 mm de filamento por mm |
| P13 | 1 capa sólida arriba y abajo; mínimos de 1 mm arriba y 0,6 abajo | Abajo, capas 0 a 2 sólidas (0,6 mm). Arriba, capas 45 a 49 (1 mm): la 45 puente interno, la 46 a 48 sólidas, la 49 superficie de arriba |
