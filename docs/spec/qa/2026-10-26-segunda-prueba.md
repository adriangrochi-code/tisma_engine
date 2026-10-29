# Segunda prueba de caja negra: lo que se arregló y los soportes (26-10-2026)

**Qué es:** se repitieron las diferencias de `2026-10-26.md` (motor en `24cafc8`) con el motor de `dev` en `dfcd143`,
y se midieron los soportes contra `supports_skirt.md`, por lo que mostró el cráneo de triceratops en
`2026-10-26-velocidad.md` (el motor imprimía 64 m de soporte y Tisma Slicer 178 m).

**Cómo:** como en la primera prueba: piezas generadas por un script, ajustes base de `surfaces_infill.md`, G-code
leído por sus etiquetas. Para comparar con Tisma Slicer se laminaron las mismas piezas en los dos, con capas de 0,2,
2 paredes, relleno al 20 %, soporte con 3 capas de interfaz y separación de 0,2.

## Las diferencias de la primera prueba

| # | Antes (`24cafc8`) | Ahora (`dfcd143`) | Estado |
|---|---|---|---|
| QA1 | Falda a 3 mm | Eje a 6,18 mm de la pieza, como S7 | Arreglado |
| QA2 | Falda con esquinas en ángulo recto | Esquinas redondeadas: 118,8 mm en S7 (pide 118,5) y 287,8 mm por capa en S8 (pide 287,0) | Arreglado |
| QA3 | Borde de 12 vueltas, 1190,4 mm | La externa con el eje a 4,82 mm; 1339,7 mm (pide 1340,4) | Arreglado |
| QA4 | La cuña de 45° llevaba soporte | 20°, 30° y 40° llevan; 45°, 50° y 60°, no | Arreglado |
| QA5 | Ventilador fijo | Capas 1 a 3 apagado; después cambia con el tiempo de capa (96 % en la capa 4, 82 % en la 47, 75 % en la 49) | Hecho; los valores de K1 no se pueden comparar porque las velocidades no son las mismas (ver abajo) |
| QA6 | Superficie de arriba a 50 mm/s | 40 mm/s, de las velocidades de respaldo | No se puede comparar: ahora las velocidades vienen del perfil de la impresora y, sin perfil, el motor usa las de una Ender-3 y lo avisa |
| QA7 | C4: 49 capas, la última en Z 9,9 | 50 capas, la última en Z 10,1 | Arreglado |
| QA8 | Sin aviso para una aguja de 1 × 1 × 80 mm | Solo el aviso de velocidades de respaldo | Sigue: falta la revisión de estabilidad (`review.*`, ver `2026-10-26-cobertura.md`) |
| QA9 | Una pared de 0,2 mm da error | Sigue el error «nothing would be printed» | Sigue: falta el ancho mínimo de `perimeters.md` |

## Soportes: diferencias nuevas

**Estante de S4** (columna de 10 × 20 × 10 mm con una tabla de 20 × 20 × 2 mm arriba que sobresale 10 mm en +X):

| # | Ficha y prueba | Pide la ficha | Hace el motor | Qué hacer |
|---|---|---|---|---|
| QA10 | `supports_skirt.md`, regla 2 y S4 | La zona de soporte llega unos 1,6 mm más allá de lo que cuelga, en +X, +Y y −Y | La zona termina 0,15 mm adentro del borde de la tabla: X hasta 109,85 e Y de 90,15 a 109,85, con la tabla de X 100 a 110 e Y 90 a 110 | Agregar el margen alrededor de la proyección de lo que cuelga |
| QA11 | `supports_skirt.md`, regla 2 y S4 | Junto a la columna, el eje de la línea del soporte queda a 0,425 mm de la cara (0,225 de separación) | A 0,8 mm | Usar `support.xy_gap` desde la cara de la pieza |
| QA12 | `supports_skirt.md`, regla 6 y S4 | Base en líneas paralelas a `support.line_angle` (0°), cada 2,857 mm (`support.spacing` más la separación de un cordón) | Líneas cada 2,5 mm, en una dirección en las capas impares y a 90° en las pares (78,4 y 64,1 mm por capa) | Misma dirección en todas las capas y separación de la regla |
| QA13 | `supports_skirt.md`, regla 4 y S4 | Primera capa de la tabla en Z 8,2 con altura 0,4 (una capa más la separación), como puente; sus perímetros con `;TYPE:Overhang perimeter` | Z 8,2 con altura 0,2; relleno de puente, pero los perímetros son comunes | Altura de capa más separación y marcar los perímetros |

**Por qué el cráneo de triceratops tiene la mitad de soporte:**
- con QA10, cada pedazo de lo que cuelga lleva su soporte sin margen. En una malla real (546 684 triángulos, con la
  inclinación de la superficie que sube y baja alrededor del límite) lo que cuelga sale en muchos pedazos chicos, y
  el soporte también: en Z 5, el motor imprime 548 mm de base en 96 tramos (mediana 1,1 mm); Tisma Slicer, 1001 mm en
  52 tramos (mediana 5,2 mm), una zona continua;
- debajo de la cúpula del cráneo quedan huecos entre los pedazos, en Z 5 y en Z 20 (Tisma Slicer los cubre);
- la interfaz es casi nada: 1,2 m contra 32,5 m en Tisma Slicer. En piezas simples la diferencia es menor (cuña de
  20°: 70 mm por capa contra 145 a 208; esfera de 30 mm: 1,3 m contra 5,8 m). El margen de QA10 también agranda la
  interfaz, porque cubre el borde de cada zona;
- el menor relleno sólido, de puente y de arriba del cráneo (37,7 m contra 55,4) viene de
  `walls.vertical_shell`, que el motor todavía no tiene (`walls_extra.md`, regla 7; ya en
  `2026-10-26-cobertura.md`).

QA10 es la que más importa: sin el margen, en una pieza orgánica el soporte sale en pedazos sueltos que se sostienen
mal y pueden dejar partes que cuelgan sin apoyo.

## Lo que coincide

| Ficha | Pruebas |
|---|---|
| `supports_skirt.md` | S1 (cuñas), S7, S8 y S9 (falda y borde), regla 5 (3 capas de interfaz en Z 7,4 a 7,8 en el estante) |
| `slicing.md` | C4 |
| `cooling.md` | Ventilador apagado en las capas 1 a 3 y según el tiempo de capa después |
