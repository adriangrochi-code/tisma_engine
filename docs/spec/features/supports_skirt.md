# Soportes, falda y borde

**Qué hace:**
- **soportes:** sostienen las partes de la pieza que cuelgan;
- **falda:** una línea alrededor de las piezas en la primera capa, para cebar la boquilla;
- **borde:** vueltas pegadas a la pieza en la primera capa, para que se agarre a la cama.

**Fase:** F5. La primera versión hace soportes en grilla; los soportes en árbol vienen después (ver `PLAN.md`).

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `support.enabled` | — | no | Activa los soportes |
| `support.angle_limit` | ° desde la horizontal | 0 = automático | Las caras que cuelgan con menos inclinación que esto llevan soporte |
| `support.z_gap` | mm | 0,2 | Separación en Z entre la interfaz y la pieza |
| `support.xy_gap` | mm o % del ancho del perímetro externo | 50 % | Separación horizontal entre el soporte y la pieza |
| `support.interface_layers` | — | 3 | Capas densas bajo la pieza |
| `support.spacing` | mm | 2,5 | Espacio libre entre las líneas de la base del soporte |
| `support.line_angle` | ° | 0 | Dirección de las líneas de la base |
| `support.under_bridges` | — | no | Si es no, los puentes no llevan soporte |
| `adhesion.skirt_loops` | — | 1 | Vueltas de la falda; 0 = sin falda |
| `adhesion.skirt_distance` | mm | 6 | Separación entre la pieza y la falda |
| `adhesion.skirt_layers` | capas | 1 | Capas de falda |
| `adhesion.brim_width` | mm | 0 | Ancho del borde; 0 = sin borde |

## Reglas

1. **Qué lleva soporte:**
   - una cara que cuelga con una inclinación menor que `support.angle_limit`;
   - en automático, una cara cuyo borde sobresale, de una capa a la siguiente, más de medio ancho de cordón. Con capas
     de altura `h` y cordón de ancho `w`, eso es una inclinación menor que `atan(2h / w)`: 41,6° con `h` = 0,2 y `w` =
     0,45. Es la regla que coincide con las pruebas S1 a S3;
   - los puentes (tramos sostenidos en los dos extremos, ver `surfaces_infill.md`) no llevan soporte, salvo con
     `support.under_bridges` (prueba S6). No importa el largo: `support.free_bridge_length` no cambia el soporte, solo
     el aviso de `print_review.md` (prueba S10).
2. **Zona de soporte:** la proyección hacia abajo de lo que cuelga, más un margen de unos 1,6 mm alrededor (prueba
   S4). Se aparta `support.xy_gap` de la pieza en cada capa: 0,225 mm con un perímetro externo de 0,45.
3. **Separación en Z:** la última capa de interfaz termina `support.z_gap` por debajo de la pieza.
4. **Primera capa de la pieza sobre el soporte:** se imprime como puente, con una altura de una capa más la separación
   (0,4 mm en la prueba S4). Solo el tramo que está sobre el soporte:
   - sus perímetros se marcan `;TYPE:Overhang perimeter` con altura 0,4 (en S4, de X 100,2 a 109,8); los perímetros
     sobre la columna siguen comunes, con 0,2;
   - el relleno de esa zona es `;TYPE:Bridge infill` con altura 0,4, y entra unos 0,6 mm sobre la parte apoyada (en
     S4, de X 99,41 a 109,04); el relleno sobre la columna sigue con 0,2.
5. **Interfaz:** las `support.interface_layers` capas de arriba son densas: líneas a 90° de las de la base, a la
   separación de los cordones (0,357 mm con ancho 0,4). Se marcan `;TYPE:Support material interface`.
6. **Base:**
   - líneas paralelas a `support.line_angle`, a `support.spacing` más la separación de un cordón (2,857 mm en la prueba);
   - unidas en zigzag por el borde de la zona;
   - ancho 0,4 mm;
   - se marcan `;TYPE:Support material`;
   - la primera capa del soporte es más ancha que las demás, para que se agarre a la cama.
7. **Capas del soporte:** en Tisma Slicer, el soporte que no toca la pieza usa capas propias de unos 0,3 mm,
   intercaladas con las de la pieza. En la primera versión del motor puede usar las mismas capas que la pieza.
8. **Falda:**
   - `adhesion.skirt_loops` vueltas en las primeras `adhesion.skirt_layers` capas;
   - la más interna queda a `adhesion.skirt_distance` de la pieza;
   - las esquinas son redondeadas (la falda es la pieza agrandada);
   - se marca `;TYPE:Skirt/Brim`.
9. **Borde:** vueltas pegadas a la pieza en la primera capa hasta `adhesion.brim_width`, a la separación de los cordones. También
   se marca `;TYPE:Skirt/Brim`.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, desde la línea de comandos.

**Ajustes base:** los de `surfaces_infill.md` (boquilla 0,4, capas de 0,2, `walls.count` 2, relleno al 20 %), con
`support.enabled` en sí para S1 a S6.

**Cuñas:** prismas de 20 mm de largo en Y y 10 mm de alto, con base de 10 mm en X y una cara que cuelga hacia +X con la
inclinación indicada, medida desde la horizontal.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| S1 | Cuñas de 20°, 30°, 40°, 45°, 50°, 55°, 60° y 70° | automático, capas de 0,2 | Llevan soporte las de 20°, 30° y 40°. Desde 45°, no |
| S2 | Cuñas de 20° y 30° | automático, capas de 0,1 | La de 20° lleva soporte; la de 30°, no |
| S3 | Cuña de 40° | automático, capas de 0,3 | Lleva soporte |
| S4 | Columna de 10 × 20 × 10 mm con una tabla de 20 × 20 × 2 mm arriba (de Z 8 a 10) que sobresale 10 mm en +X | — | Soporte bajo los 10 mm que sobresalen. Interfaz: 4 capas de soporte entre Z 6,8 y 7,8, líneas a 90° cada 0,357 mm. Base: líneas a 0° cada 2,857 mm, ancho 0,4. Junto a la columna, el eje de la línea del soporte queda a 0,425 mm de la cara (0,225 de separación). La zona llega 1,6 mm más allá de la tabla en +X, +Y y −Y. Primera capa de la tabla: Z 8,2, altura 0,4, como puente |
| S5 | Cuñas de 40°, 50° y 60° | `support.angle_limit` 55 | 40° y 50° llevan soporte; 60°, no |
| S6 | «Mesa» de `surfaces_infill.md` (puente de 10 mm) | — | Sin soporte: el puente se imprime en el aire |
| S7 | Cubo de 20 × 20 × 10 mm, sin soportes | falda por defecto | Una vuelta solo en la primera capa, con el eje a 6,18 mm de la pieza (borde interno a 5,98 mm), ancho 0,4 mm, largo 118,5 mm: el cuadrado más esquinas redondeadas |
| S8 | Cubo de S7 | `adhesion.skirt_loops` 3, `adhesion.skirt_distance` 2, `adhesion.skirt_layers` 2 | Tres vueltas en las capas 1 y 2; la externa con el eje a 2,89 mm de la pieza; 287,0 mm por capa |
| S9 | Cubo de S7 | sin falda, `adhesion.brim_width` 5 | Borde de 14 vueltas en la primera capa: la externa con el eje a 4,82 mm de la pieza (borde a 5,02 mm); 1340,4 mm |
| S10 | Dos pilares de 5 × 20 × 10 mm separados 10, 20 y 30 mm, con una tabla de 2 mm encima | Soporte encendido; `support.free_bridge_length` 0, 16 y 40; `support.under_bridges` no y sí | Con `under_bridges` no: ningún soporte en los tres puentes, con cualquier `free_bridge_length`. Con sí: soporte bajo los tres, igual con cualquier valor |
