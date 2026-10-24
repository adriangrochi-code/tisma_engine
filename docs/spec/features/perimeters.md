# Perímetros

**Qué hace:** recorre el borde de cada isla y de cada agujero con cordones paralelos al contorno. Las paredes más finas
que los cordones pedidos se cubren con menos cordones, de ancho variable, sin huecos ni excesos.

**Fase:** F2.

**Fuentes:**
- Kuipers, Doubrovski, Wu, Wang 2020 (ver `SOURCES.md`): cantidad y ancho de los cordones en paredes finas,
  transiciones entre cantidades, reparto del ancho.
- [Manual de Slic3r, «Flow Math»](https://manual.slic3r.org/advanced/flow-math): modelo de la sección de un cordón,
  un rectángulo con los costados redondeados. Es documentación de usuario.

## Ajustes

`d` es `nozzle_diameter` y `h` la altura de la capa.

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `perimeters` | — | 3 | Cordones pedidos de cada lado de una pared |
| `perimeter_width`, `external_perimeter_width` | mm | 0: automático, 1,125 × `d` (0,45 con `d` = 0,4) | Ancho nominal de los cordones |
| `first_layer_width` | mm o % de la altura de la primera capa | 200 % | Ancho de los cordones de la primera capa |
| `min_bead_width` | mm o % de `d` | 85 % | Ancho mínimo de un cordón: un detalle más fino se imprime con este ancho |
| `min_feature_size` | mm o % de `d` | 25 % | Un detalle más fino que esto no se imprime |
| `transition_length` | mm o % de `d` | 100 % | Largo del tramo en el que una pared pasa de N a N + 1 cordones |
| `transition_angle` | ° | 10 | En esquinas más agudas que esto no se hacen transiciones |
| `transition_filter` | mm o % de `d` | 25 % | Margen que evita pasar una y otra vez entre N y N + 1 cordones |
| `distribution_count` | — | 1 | Cuántos cordones, desde el centro de la pared, absorben la diferencia de ancho |
| `external_perimeter_first` | — | no | Imprimir de afuera hacia adentro |
| `seam_position` | — | `aligned` | Dónde empieza cada perímetro cerrado (ver `SETTINGS_V1.md`) |
| `seam_gap` | mm o % de `d` | 15 % | Cuánto antes de su inicio termina un perímetro cerrado |

## Reglas

1. **Modelo del cordón:**
   - un cordón de ancho `w` (el de `;WIDTH:`) y alto `h` tiene sección de rectángulo con costados redondeados;
   - dos cordones vecinos se ponen a una distancia `w − h·(1 − π/4)` (en la prueba P1, 0,407 mm con `w` = 0,45 y
     `h` = 0,2);
   - el borde exterior del cordón externo coincide con el contorno de la pieza.
2. **Paredes comunes:** `perimeters` cordones de ancho nominal desde cada borde; lo que queda adentro es para el relleno.
3. **Paredes finas:**
   - si no entran 2 × `perimeters` cordones nominales, se usan menos, con ancho variable (Kuipers et al. 2020);
   - con `distribution_count` = 1, solo los cordones del centro de la pared cambian de ancho; los demás quedan
     nominales (tabla P2);
   - un detalle de una sola fila de cordones se imprime con un cordón tan ancho como el detalle;
   - más fino que `min_bead_width`, se imprime con un cordón de `min_bead_width`, centrado;
   - más fino que `min_feature_size`, no se imprime. Si la pieza entera queda vacía, es un error (prueba P3).
4. **Orden en cada isla:** de adentro hacia afuera; el cordón externo va último. Con `external_perimeter_first`, al
   revés. Los agujeros llevan sus propios perímetros, también en ese orden (prueba P4).
5. **Cierre:** un perímetro cerrado termina `seam_gap` antes de su inicio (0,06 mm con `d` = 0,4; prueba P1).
6. **Costura (`seam_position`):**
   - `aligned`: el inicio queda en el mismo lugar en todas las capas, en una esquina si hay (pruebas P5 y P6);
   - `rear`: en el punto de Y máxima;
   - `nearest`: el punto más cercano a donde estaba la boquilla; en un cilindro se corre de a poco, capa a capa;
   - `random`: en cualquier lugar, distinto en cada capa.
7. **Primera capa:** los cordones miden `first_layer_width` (0,4 mm con una primera capa de 0,2).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, desde la línea de comandos. Mismos ajustes base que
`slicing.md` (boquilla 0,4; capas de 0,2; sin relleno ni capas sólidas; sin falda ni borde; pieza centrada en
X = 100, Y = 100), con `perimeters` 3 salvo que se diga otra cosa. Las posiciones son del eje de cada cordón.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| P1 | Cubo de 20 × 20 × 10 mm | — | Por capa (desde la segunda), tres vueltas de 0,45 mm de ancho, a 0,225, 0,632 y 1,039 mm del borde. Largo de la vuelta externa: 78,14 mm (el cuadrado entero mediría 78,20) |
| P2 | Paredes rectas de 20 mm de largo, 5 mm de alto y espesor de 0,1 a 3,0 mm | — | Tabla P2 |
| P3 | Pared de 0,1 mm | — | No se imprime. Como no queda nada, Tisma Slicer da error: «la impresión está vacía» |
| P4 | Marco de 20 × 20 mm con un agujero de 10 × 10 mm | — | Orden por capa: los tres cordones del agujero, del más interno al que toca el agujero; después los tres de afuera, del más interno al externo |
| P5 | Cubo de P1 | `seam_position` `aligned` | Todas las capas empiezan en la esquina de X e Y máximas |
| P6 | Cilindro de radio 10 mm, 128 lados | `aligned` | Todas las capas empiezan en el punto de Y mínima (frente) |
| P7 | Cilindro de P6 | `rear` | Todas las capas empiezan en el punto de Y máxima |
| P8 | Cilindro de P6 | `nearest` | El inicio gira poco a poco: de −87° a −60° entre las capas 3 y 24 (ángulo medido desde +X) |
| P9 | Cilindro de P6 | `random` | Un ángulo distinto en cada capa, sin patrón |
| P10 | Cubo de P1 | `external_perimeter_first` | Mismas vueltas que P1, en orden inverso: la externa primero |

**Tabla P2.** Cordones de la capa 11 de cada pared: posición del eje respecto del centro de la pared (mm) y ancho
(mm). E es el cordón externo y P uno interno. Las paredes de 2,9 mm o más dejan un hueco en el centro, que llena el
relleno.

| Espesor | Cordones (posición: ancho) |
|---|---|
| 0,1 | ninguno |
| 0,2 | E 0: 0,383 |
| 0,3 | E 0: 0,383 |
| 0,4 | E 0: 0,400 |
| 0,5 | E 0: 0,500 |
| 0,6 | E 0: 0,600 |
| 0,7 | E ±0,164: 0,371 |
| 0,8 | E ±0,189: 0,421 |
| 0,9 | E ±0,214: 0,471 |
| 1,0 | E ±0,239: 0,521 |
| 1,1 | E ±0,264: 0,571 |
| 1,2 | E ±0,375: 0,450; P 0: 0,386 |
| 1,3 | E ±0,425: 0,450; P 0: 0,486 |
| 1,4 | E ±0,475: 0,450; P 0: 0,586 |
| 1,5 | E ±0,525: 0,450; P ±0,161: 0,364 |
| 1,6 | E ±0,575: 0,450; P ±0,186: 0,414 |
| 1,7 | E ±0,625: 0,450; P ±0,211: 0,464 |
| 1,8 | E ±0,675: 0,450; P ±0,236: 0,514 |
| 1,9 | E ±0,725: 0,450; P ±0,261: 0,564 |
| 2,0 | E ±0,775: 0,450; P ±0,368: 0,450; P 0: 0,372 |
| 2,1 | E ±0,825: 0,450; P ±0,418: 0,450; P 0: 0,472 |
| 2,2 | E ±0,875: 0,450; P ±0,468: 0,450; P 0: 0,572 |
| 2,3 | E ±0,925: 0,450; P ±0,518: 0,450; P ±0,157: 0,357 |
| 2,4 | E ±0,975: 0,450; P ±0,568: 0,450; P ±0,182: 0,407 |
| 2,5 | E ±1,025: 0,450; P ±0,618: 0,450; P ±0,207: 0,457 |
| 2,6 | E ±1,075: 0,450; P ±0,668: 0,450; P ±0,232: 0,507 |
| 2,7 | E ±1,125: 0,450; P ±0,718: 0,450; P ±0,257: 0,557 |
| 2,8 | E ±1,175: 0,450; P ±0,768: 0,450; P ±0,282: 0,607 |
| 2,9 | E ±1,225: 0,450; P ±0,818: 0,450; P ±0,411: 0,450 |
| 3,0 | E ±1,275: 0,450; P ±0,868: 0,450; P ±0,461: 0,450 |

Lo que muestra la tabla:
- **Paredes de un cordón (0,2 a 0,6 mm):** el ancho del cordón es el de la pared, y nunca menos que `min_bead_width`
  (0,34 mm de separación, 0,383 mm de ancho en el G-code).
- **Transiciones:** entre 0,6 y 0,7 mm la pared pasa de 1 a 2 cordones; entre 1,1 y 1,2 a 3; entre 1,4 y 1,5 a 4;
  entre 1,9 y 2,0 a 5; entre 2,2 y 2,3 a 6.
- **Reparto del ancho:** los cordones nominales quedan a 0,45 mm, y el o los del centro absorben la diferencia.
