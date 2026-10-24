# Estabilidad durante la impresión

**Qué hace:** estima, capa por capa, si una pieza sin soportes puede empezar en el aire, volcarse por su peso,
doblarse cuando la boquilla la empuja o despegarse de la cama. Da los avisos de «Estabilidad» de `print_review.md`
(regla 6).

**Fase:** después de F4, con `print_review.md`.

**Origen:**
- es un diseño de esta ficha. Tisma Slicer usa ahí código ajeno, que no se describe ni se copia;
- la física es de manual:
  - **estática:** un cuerpo apoyado no vuelca mientras la proyección de su centro de masa cae dentro del polígono
    convexo de su apoyo;
  - **flexión de vigas** (Euler-Bernoulli): la flecha de un voladizo con una carga `F` en la punta es
    `δ = F · L³ / (3 · E · I)`. Está en cualquier texto de resistencia de materiales; por ejemplo, Gere y Goodno,
    *Mechanics of Materials*, en la tabla de flechas de vigas;
  - **flexión de una sección:** la tensión máxima con un momento `M` es `M / W`, con `W = I / c` (`c`: la distancia
    más lejana al eje que pasa por el centroide).
- `E_z`, el módulo a través de las capas, sale de la tabla de materiales de `structural_analysis.md`: una columna
  vertical que se dobla trabaja en Z.

**Lo que no hay:** no se encontró una fuente pública con la fuerza lateral de la boquilla sobre la pieza ni con la
adherencia a cada cama, en N/mm². Los dos valores se miden con la pieza de calibración (regla 6). Mientras no estén
medidos, las reglas que los usan solo informan.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `review.nozzle_force` | N | sin valor | Fuerza lateral de la boquilla sobre la capa que imprime. Se mide (regla 6) |
| `review.bed_adhesion` | N/mm² | sin valor | Tensión que aguanta la unión con la cama, para esa cama y ese material. Se mide (regla 6) |
| `review.max_deflection` | mm | 0,1 | Flecha máxima aceptada en la punta de una parte esbelta. Diseño: un cuarto de un cordón de 0,4 mm |

## Reglas

1. **Columnas:** las islas de cada capa se encadenan con las de la capa de abajo que se superponen.
   - **Sección** de una capa: las islas de la cadena en esa capa, juntas. Su rigidez es el menor de sus momentos de
     inercia principales, respecto de su centroide.
   - Para la isla de la capa `n` se baja capa por capa por la cadena, llevando `I_min`: la menor rigidez vista hasta
     ahí. Usar el mínimo deja la cuenta del lado seguro.
   - La **base** es la primera capa, bajando, cuya rigidez es de al menos 10 veces `I_min`. Si no hay, la base es la
     cama.
   - `L`: altura desde la base hasta la parte de arriba de la capa `n`.
2. **Parte en el aire** (crítico): una isla de una capa, desde la segunda, cuya superposición con la capa de abajo
   (del mismo objeto o de los soportes) mide menos que un cuadrado de un ancho de cordón (`walls.width`²). Se muestra
   desde su primera capa (`print_review.md`, prueba R2).
3. **Vuelco por el peso** (advertencia):
   - el centro de masa de todo lo impreso del objeto hasta la capa `n`, con densidad uniforme;
   - si su proyección cae fuera del polígono convexo de lo que toca la cama (la primera capa, más el borde si hay),
     desde esa capa la pieza se sostiene solo por la adherencia;
   - con `review.bed_adhesion` medida, es crítico si el momento del peso, `m · g · d`, supera `review.bed_adhesion · W`
     del apoyo (`d`: la distancia horizontal del centro de masa al centroide del apoyo).
4. **Parte esbelta que se dobla** (advertencia):
   - flecha en la capa `n`: `δ = review.nozzle_force · L³ / (3 · E_z · I_min)`;
   - se avisa desde la primera capa en que `δ` supera `review.max_deflection`, con la flecha calculada y la columna;
   - sugerencia: orientarla más baja, agregar un refuerzo o imprimirla junto a una parte que la sostenga;
   - sin `review.nozzle_force`, es información: las tres columnas más flexibles del objeto, con su flexibilidad
     `L³ / (3 · E_z · I_min)` en mm/N.
5. **Despegue de la cama** (advertencia), solo con los dos valores medidos:
   - momento en el apoyo cuando se imprime la capa `n`: `review.nozzle_force · z_n`, más el del peso de la regla 3;
   - aguanta `review.bed_adhesion · W` del apoyo, con el `W` más chico de sus ejes principales;
   - se avisa desde la primera capa en que el momento lo supera.
6. **Calibración** (pieza de este diseño):
   - **fuerza de la boquilla:**
     - cinco pilares de 3 × 3 mm, de 10, 20, 30, 40 y 50 mm de alto, a 15 mm entre centros, cada uno sobre un pie de
       10 × 10 × 1 mm;
     - la persona marca el pilar más bajo cuya punta muestra capas corridas;
     - con su alto `L` sobre el pie, `I = 3⁴ / 12 = 6,75 mm⁴` y el `E_z` del material:
       `review.nozzle_force = 3 · E_z · I · review.max_deflection / L³`;
   - **adherencia:**
     - con la fuerza ya medida, cilindros de 4, 6, 8 y 10 mm de diámetro y 30 mm de alto, directos sobre la cama;
     - la persona marca el más grueso que se despegó y la altura `h` donde se despegó;
     - con `W = π · d³ / 32` de su apoyo: `review.bed_adhesion = review.nozzle_force · h / W`;
     - se guarda por cama y material.
7. **Lo que esta versión no estima:** las extrusiones que se levantan al enfriarse; necesitan un modelo térmico. Los
   voladizos y puentes largos están en `print_review.md` (regla 2) y en `supports_skirt.md`.

## Pruebas

Cálculos para comprobar la implementación: no son medidas de una impresora. PLA (`E_z` 2600 MPa), capas de 0,2 mm,
`walls.width` 0,4 y sin soportes. Los valores de fuerza y adherencia marcados «de prueba» son solo para la cuenta.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| ST1 | Un bloque sobre la cama y, al lado, un bloque chico que empieza 10 mm arriba | — | «Parte en el aire», crítico, desde Z 10,2 |
| ST2 | Barra inclinada a 45° hacia +X, de sección horizontal 10 × 10 mm, que nace en la cama entre x 0 y 10 y sube 30 mm | — | El apoyo es la primera capa, cortada a media altura: de x 0,1 a 10,1. El centro de masa de lo impreso hasta Z está en `x = 5 + Z / 2`: queda en el borde en Z 10,2 y sale en Z 10,4. «Vuelco por el peso», advertencia, desde Z 10,4 |
| ST3 | Pilar de 4 × 4 × 40 mm sobre la cama (`I_min` 21,333 mm⁴) | `review.nozzle_force` 1 N (de prueba) | Flecha `L³ / 166 400` mm: llega a 0,1 mm con `L` = 25,53 mm. Advertencia desde Z 25,6; en Z 40, 0,385 mm |
| ST4 | El pilar de ST3 | sin `review.nozzle_force` | Información: flexibilidad 0,385 mm/N en Z 40, sin advertencia |
| ST5 | Cubo de 20 mm | `review.nozzle_force` 1 N (de prueba) | Sin aviso: flecha en Z 20 de 0,000077 mm |
| ST6 | Calibración: el pilar más bajo con capas corridas es el de 30 mm | `review.max_deflection` 0,1 | `review.nozzle_force` = 3 · 2600 · 6,75 · 0,1 / 30³ = 0,195 N |
| ST7 | Cilindro de 4 mm de diámetro y 30 mm de alto sobre la cama (`W` 6,283 mm³) | `review.nozzle_force` 1 N y `review.bed_adhesion` 1 N/mm² (de prueba) | El momento pasa 6,283 N·mm por encima de Z 6,28: «Despegue de la cama», advertencia, desde Z 6,4 |
| ST8 | Dos pilares de 4 × 4 × 40 mm sobre una placa de 30 × 10 × 2 mm | `review.nozzle_force` 1 N (de prueba) | La base de cada pilar es la placa (Z 2): `L` llega a 25,53 mm en Z 27,6, y el aviso sale desde Z 27,6 |
