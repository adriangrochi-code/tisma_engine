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

`d` es `machine.nozzle_diameter` y `h` la altura de la capa.

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `walls.count` | — | 3 | Cordones pedidos de cada lado de una pared |
| `walls.width`, `walls.outer_width` | mm | 0: automático, 1,125 × `d` (0,45 con `d` = 0,4) | Ancho nominal de los cordones |
| `layers.first_line_width` | mm o % de la altura de la primera capa | 200 % | Ancho de los cordones de la primera capa |
| `walls.min_line_width` | mm o % de `d` | 85 % | Ancho mínimo de un cordón: un detalle más fino se imprime con este ancho |
| `walls.min_detail` | mm o % de `d` | 25 % | Un detalle más fino que esto no se imprime |
| `walls.transition_length` | mm o % de `d` | 100 % | Largo del tramo en el que una pared pasa de N a N + 1 cordones |
| `walls.transition_angle` | ° | 10 | Una cuña cuyo ángulo de punta es mayor que esto no tiene transiciones a un número impar de cordones: los cordones van de a pares y no hay cordón en el centro (prueba P14) |
| `walls.transition_margin` | mm o % de `d` | 25 % | Margen que evita pasar una y otra vez entre N y N + 1 cordones cuando el espesor oscila poco alrededor del punto de cambio (prueba P13) |
| `walls.spread_count` | — | 1 | Cuántos cordones, desde el centro de la pared, absorben la diferencia de ancho |
| `walls.outer_first` | — | no | Imprimir de afuera hacia adentro |
| `walls.seam` | — | `aligned` | Dónde empieza cada perímetro cerrado (ver `SETTINGS_V1.md`) |
| `walls.seam_gap` | mm o % de `d` | 15 % | Cuánto antes de su inicio termina un perímetro cerrado |

## Reglas

1. **Modelo del cordón:**
   - un cordón de ancho `w` (el de `;WIDTH:`) y alto `h` tiene sección de rectángulo con costados redondeados;
   - dos cordones vecinos se ponen a una distancia `w − h·(1 − π/4)` (en la prueba P1, 0,407 mm con `w` = 0,45 y
     `h` = 0,2). Si los dos tienen anchos distintos, `w` es el promedio de los dos (prueba P12);
   - `walls.outer_width` en 0 sale de la boquilla (1,125 × `d`), aunque `walls.width` tenga otro valor (prueba P12);
   - el borde exterior del cordón externo coincide con el contorno de la pieza.
2. **Paredes comunes:** `walls.count` cordones de ancho nominal desde cada borde; lo que queda adentro es para el relleno.
3. **Paredes finas:**
   - si no entran 2 × `walls.count` cordones nominales, se usan menos, con ancho variable (Kuipers et al. 2020);
   - con `walls.spread_count` = 1, solo los cordones del centro de la pared cambian de ancho; los demás quedan
     nominales (tabla P2);
   - un detalle de una sola fila de cordones se imprime con un cordón tan ancho como el detalle;
   - más fino que `walls.min_line_width`, se imprime con un cordón de `walls.min_line_width`, centrado;
   - más fino que `walls.min_detail`, no se imprime. Si la pieza entera queda vacía, es un error (prueba P3).
4. **Orden en cada isla:** de adentro hacia afuera; el cordón externo va último. Con `walls.outer_first`, al
   revés. Los agujeros llevan sus propios perímetros, también en ese orden (prueba P4).
5. **Cierre:** un perímetro cerrado termina `walls.seam_gap` antes de su inicio (0,06 mm con `d` = 0,4; prueba P1).
6. **Costura (`walls.seam`):**
   - `aligned`: el inicio queda en el mismo lugar en todas las capas, en una esquina si hay (pruebas P5 y P6);
   - `rear`: en el punto de Y máxima;
   - `nearest`: el punto más cercano a donde estaba la boquilla; en un cilindro se corre de a poco, capa a capa;
   - `random`: en cualquier lugar, distinto en cada capa.
7. **Primera capa:** los cordones miden `layers.first_line_width` (0,4 mm con una primera capa de 0,2).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, desde la línea de comandos. Mismos ajustes base que
`slicing.md` (boquilla 0,4; capas de 0,2; sin relleno ni capas sólidas; sin falda ni borde; pieza centrada en
X = 100, Y = 100), con `walls.count` 3 salvo que se diga otra cosa. Las posiciones son del eje de cada cordón.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| P1 | Cubo de 20 × 20 × 10 mm | — | Por capa (desde la segunda), tres vueltas de 0,45 mm de ancho, a 0,225, 0,632 y 1,039 mm del borde. Largo de la vuelta externa: 78,14 mm (el cuadrado entero mediría 78,20) |
| P2 | Paredes rectas de 20 mm de largo, 5 mm de alto y espesor de 0,1 a 3,0 mm | — | Tabla P2 |
| P3 | Pared de 0,1 mm | — | No se imprime. Como no queda nada, Tisma Slicer da error: «la impresión está vacía» |
| P4 | Marco de 20 × 20 mm con un agujero de 10 × 10 mm | — | Orden por capa: los tres cordones del agujero, del más interno al que toca el agujero; después los tres de afuera, del más interno al externo |
| P5 | Cubo de P1 | `walls.seam` `aligned` | Todas las capas empiezan en la esquina de X e Y máximas |
| P6 | Cilindro de radio 10 mm, 128 lados | `aligned` | Todas las capas empiezan en el punto de Y mínima (frente) |
| P7 | Cilindro de P6 | `rear` | Todas las capas empiezan en el punto de Y máxima |
| P8 | Cilindro de P6 | `nearest` | El inicio gira poco a poco: de −87° a −60° entre las capas 3 y 24 (ángulo medido desde +X) |
| P9 | Cilindro de P6 | `random` | Un ángulo distinto en cada capa, sin patrón |
| P10 | Cubo de P1 | `walls.outer_first` | Mismas vueltas que P1, en orden inverso: la externa primero |
| P11 | Cuña en planta: triángulo de 30 mm de largo cuyo espesor crece de 0 (X = −15) a 3 mm (X = +15), 3 mm de alto | — | Tabla P11: el espesor cambia de a poco, así que las transiciones quedan a lo largo de la pieza |
| P12 | Cubo de 20 × 20 × 10 mm, boquilla de 0,4, capas de 0,3, `walls.count` 3; ejes en la capa 5, medidos desde el borde | `walls.width` 0,5 y `walls.outer_width` 0; después `walls.width` 0,4 y `walls.outer_width` 0,6 | Primero: externo de 0,45 a 0,225; internos de 0,5 a 0,636 y 1,071. Después: externo de 0,6 a 0,300; internos de 0,4 a 0,736 y 1,071. Cada paso es el promedio de los dos anchos menos 0,0644 (`h·(1 − π/4)` con `h` = 0,3) |
| P13 | Tira de 40 mm de largo y 3 mm de alto: un borde recto y el otro ondulado, espesor 1,15 + A · sen(2π·x / λ); capas de 0,2, solo paredes; cordones que cruzan cada sección cada 0,05 mm, capa 8 | λ = 5 mm con A = 0,05 a 0,3; λ = 10 mm con A = 0,1. `walls.transition_margin` 0, 25 %, 50 % y 100 % | Tablas P13 y P13b: el margen aplana las oscilaciones de A ≤ 0,15 mm alrededor del cambio con 25 % o más, también con λ = 10 mm (crestas más largas) y en los cambios 3→4 y 4→5; con A ≥ 0,16 a 0,2 no aplana ninguna, ni con 100 % |
| P14 | Cuña en planta de 20 mm de largo, espesor de 0 en la punta, con ángulo de punta de 5°, 10° y 20°; solo paredes, capas de 0,2; cordones por sección, capa 8 | `walls.transition_angle` 1, 10 y 30 | Ángulo de 1°: en las tres cuñas solo números pares de cordones (0, 2, 4, 6…) y sin cordón en el centro. 10°: las cuñas de 5° y 10° pasan por todos los números (1, 2, 3, 4…); la de 20° sigue de a pares. 30°: las tres pasan por todos. Hay transición cuando el ángulo de la punta no pasa del ajuste |

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
- **Paredes de un cordón (0,2 a 0,6 mm):** el ancho del cordón es el de la pared, y nunca menos que `walls.min_line_width`
  (0,34 mm de separación, 0,383 mm de ancho en el G-code).
- **Transiciones:** entre 0,6 y 0,7 mm la pared pasa de 1 a 2 cordones; entre 1,1 y 1,2 a 3; entre 1,4 y 1,5 a 4;
  entre 1,9 y 2,0 a 5; entre 2,2 y 2,3 a 6.
- **Reparto del ancho:** los cordones nominales quedan a 0,45 mm, y el o los del centro absorben la diferencia.

**Tabla P13.** Largo de cada tramo de 3 cordones en las crestas (mm); en el resto, 2 cordones.

| λ | A | Margen 0 | Margen 25 %, 50 % y 100 % |
|---|---|---|---|
| 5 | 0,05 | 1,1 a 1,25 | sin tramos de 3 |
| 5 | 0,075 | 1,5 | sin tramos de 3 |
| 5 | 0,1 | 1,6 | sin tramos de 3 |
| 5 | 0,15 | 1,6 | sin tramos de 3 |
| 5 | 0,2 | 1,0 | 1,0 (igual que con 0) |
| 5 | 0,3 | 0,4 (uno de 0,95) | igual que con 0 |
| 10 | 0,1 | 3,85 | sin tramos de 3 |

**Tabla P13b** (medida el 29-10-2026, mismo método, λ = 5 mm; «N→M» es la cantidad de cordones en los valles y en
las crestas, con el largo de cada tramo de más cordones):

| Espesor medio | A | Margen 0 | Margen 25 % | Margen 100 % |
|---|---|---|---|---|
| 1,15 | 0,16 a 0,19 | 2→3, tramos de 1,0 | igual que con 0 | igual que con 0 |
| 1,45 | 0,05 | 3→4, tramos de 1,85 | 3 en toda la tira | 3 en toda la tira |
| 1,45 | 0,1 | 3→4, tramos de 2,35 | 3 en toda la tira | 3 en toda la tira |
| 1,45 | 0,15 | 3→4, tramos de 2,45 | **4 en toda la tira** | 3 en toda la tira |
| 1,45 | 0,17 a 0,25 | 3→4, tramos de 3,5 | igual que con 0 | igual que con 0 |
| 1,95 | 0,1 | 4 casi en toda la tira | 4 en toda la tira | 4 en toda la tira |
| 1,95 | 0,15 | 4→5, tramos de 1,4 | 4 en toda la tira | 4 en toda la tira |
| 1,95 | 0,2 y 0,25 | 4→5, tramos de 0,85 y 0,8 | igual que con 0 | igual que con 0 |

Lectura: el corte no depende del valor del margen (25 % y 100 % cortan en el mismo lugar) ni del largo del tramo.
Depende de cuánto pasa la cresta el punto de cambio: con el espesor medio justo en el cambio (1,15 para 2→3, 1,45
para 3→4, 1,95 para 4→5), las oscilaciones de A ≤ 0,15 mm se aplanan y las de A ≥ 0,16 mm (0,17 en 3→4, 0,2 en 4→5,
donde no se midió entre medio) quedan. Con margen 0 ya se ve el mismo salto: entre A = 0,15 y 0,16 el tramo de
1,15 pasa de 1,6 a 1,0 mm. El margen elige además hacia qué lado aplana: casi siempre al menor número de cordones,
pero con 1,45 y A = 0,15 y margen 25 % toda la tira quedó en 4.

Con paredes de 0,6 mm (mismo método, boquilla 0,4; el cambio de 2 a 3 cordones queda en 1,47 mm): con A = 0,1 y 0,15
el margen de 25 % o 100 % aplana la tira a 2 cordones; con A = 0,16 a 0,3 deja los tramos de 3 con cualquier margen
(1,0 mm con 0,16 y 0,17; 0,9 mm con 0,21 a 0,25; 0,4 mm con 0,3). El corte queda entre 0,15 y 0,16 mm, igual que
con 0,45: es una distancia fija sobre el punto de cambio, no una parte del ancho del cordón (que daría 0,21 mm).

**Tabla P11.** Cuña de la prueba P11, capa 8: cordones que cruzan la sección X = c (posición en Y: ancho del G-code;
E externo, P interno).

| X | Espesor | Cordones |
|---|---|---|
| −14 | 0,10 | ninguno |
| −13 | 0,20 | E 0: 0,426 |
| −11 | 0,40 | E 0: 0,553 |
| −9 | 0,60 | E 0: 0,639 |
| −8 | 0,70 | E ±0,16: 0,408 |
| −5 | 1,00 | E ±0,24: 0,551 |
| −4 | 1,10 | E ±0,26: 0,599 |
| −3 | 1,20 | E ±0,37: 0,450; P 0: 0,424 |
| −1 | 1,40 | E ±0,47: 0,450; P 0: 0,596 |
| 0 | 1,50 | E ±0,52: 0,450; P ±0,16: 0,408 |
| +4 | 1,90 | E ±0,72: 0,450; P ±0,26: 0,599 |
| +5 | 2,00 | E ±0,77: 0,450; P ±0,35: 0,500 |
| +6 | 2,10 | E ±0,82: 0,450; P ±0,42: 0,450; P 0: 0,510 |
| +7 | 2,20 | E ±0,87: 0,450; P ±0,47: 0,450; P 0: 0,596 |
| +8 | 2,30 | E ±0,92: 0,450; P ±0,52: 0,450; P ±0,12: 0,453 |
| +9 | 2,40 | E ±0,97: 0,450; P ±0,57: 0,450; P −0,18: 0,406 (en la transición, uno solo) |
| +11 | 2,60 | E ±1,07: 0,450; P ±0,67: 0,450; P ±0,23: 0,543 |
| +13 | 2,80 | E ±1,17: 0,450; P ±0,77: 0,450; P ±0,28: 0,634 |

Comparada con las paredes rectas de P2:
- las transiciones caen en espesores parecidos (de 1 a 2 cordones entre 0,6 y 0,7; de 5 a 6 entre 2,2 y 2,3);
- de 4 a 5 cordones, la cuña cambia entre 2,0 y 2,1, una décima después que la pared recta;
- los anchos del G-code son algo mayores que en la pared recta del mismo espesor, porque la cuña se ensancha a lo
  largo de cada tramo.
