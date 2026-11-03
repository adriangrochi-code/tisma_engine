# Relleno en zigzag en Z

**Qué hace:** los cordones del relleno poco denso suben y bajan en Z mientras avanzan, en dientes de sierra de unos
pocos décimos de milímetro. La forma es la misma en todas las capas, así cada capa queda anidada en la de abajo: la
unión entre capas deja de ser un plano y queda inclinada y trabada, que es por donde suelen romperse las piezas
tiradas en Z.

**Fase:** después de F3. Experimental: la mejora en el relleno todavía no se midió con probetas propias (prueba ZI12).

**Relación con otras fichas:**
- `interlock_infill.md` (ondas entrelazadas): la otra forma de relleno no planar, con ondas suaves «de cartón de
  huevos» más largas y más bajas. Las dos no van juntas (regla 9).
- `nonplanar.md` y `nonplanar_modes.md`: curvan las capas enteras. El zigzag curva solo el relleno de cada capa y no
  va con ellas (regla 9).

**Fuentes:**
- Allum, Kitzinger, Li, Silberschmidt y Gleadall, *ZigZagZ: Improving mechanical performance in extrusion additive
  manufacturing by nonplanar toolpaths*, Additive Manufacturing 38 (2021) 101715,
  [DOI 10.1016/j.addma.2020.101715](https://doi.org/10.1016/j.addma.2020.101715). Lo que dice el texto del artículo
  (versión de los autores, leída el 03-11-2026):
  - tramos que se repiten cada 2 mm, capas de 0,2 mm, boquilla de 0,4 mm, 25 mm/s, 210 °C, Ultimaker 2;
  - en Z, hasta +62 % de resistencia, +123 % de deformación a la rotura y +245 % de tenacidad frente a la mejor
    probeta plana;
  - las probetas eran **paredes de un solo cordón** cortadas de cajas, no relleno: la mejora en un relleno al 20 % no
    está medida en el artículo.
- La forma, el tope de altura y las medidas de este motor están en su `docs/NONPLANAR_INFILL.md` (N1).

## Ajustes

Los nombres son los del motor (`docs/CONFIG.md`).

| Clave | Unidad | Por defecto | Rango | Qué hace |
|---|---|---|---|---|
| `infill.nonplanar` | — | no | — | Activa el zigzag en el relleno poco denso |
| `infill.nonplanar_amplitude` | % de la altura de capa | 250 | 0 a 500 | Altura del diente, de valle a cresta (0,5 mm con capas de 0,2) |
| `infill.nonplanar_wavelength` | mm | 2 | 0,5 a 20 | Largo de un diente completo, como el tramo de 2 mm del artículo |
| `machine.max_feed_z` | mm/s | del perfil de la impresora | más de 0 | Velocidad máxima del eje Z; frena el relleno en zigzag (regla 7) |

## Reglas

1. **Forma:** un diente de sierra simétrico en una dirección fija de la cama, a 45° de la dirección del relleno, así
   las dos familias de líneas de la grilla lo cruzan igual. Es una función de la posición en la cama, la misma en todas
   las capas: Z cambia sin saltos a lo largo de cualquier recorrido y cada capa se apoya en la forma de la de abajo.
   Con los valores por defecto la pendiente es de 0,5 (26,6°).
2. **Patrones:** solo donde los cordones se apilan capa sobre capa: `grid`, `triangles`, `stars`, `honeycomb`,
   `aligned_rectilinear` y `concentric`. En los patrones que cambian de capa a capa (`rectilinear`, que gira 90°,
   `gyroid`, `cubic`, `lightning` y los demás) no hay zigzag y el laminado avisa (`nonplanar_ignored`): «El zigzag en
   Z no se usa con el patrón ‹nombre›: sus líneas cambian de lugar en cada capa».
3. **Tope de altura:** la cara plana de la boquilla tiene que pasar sobre la línea vecina en su cresta sin tocarla. La
   altura se achica hasta que entre: `A ≤ separación entre líneas − 1,25 × boquilla − ancho / 2`. Con un relleno muy
   denso puede no quedar lugar y no hay zigzag.
4. **Junto a paredes y pieles:** la altura baja a cero cerca de los perímetros, del relleno sólido y de las superficies
   de arriba y de abajo. El relleno toca las paredes plano y empieza y termina plano bajo las pieles.
5. **Sin lugar:** cuando por las reglas 3 y 4 ninguna capa de una pieza lleva zigzag (pieza chica, relleno denso), el
   laminado lo avisa, igual que la regla 2: «El zigzag en Z no entra en ‹pieza›: es muy chica o el relleno muy denso».
   Hoy el motor no avisa (prueba ZI8).
6. **Material:** cada tramo extruye según su espesor real: la distancia hasta el cordón de abajo, no la altura de capa.
   Donde las dos capas tienen el diente completo, el espesor es la altura de capa.
7. **Velocidad:** el eje Z sube y baja junto con el avance, así que la velocidad escrita en el G-code (`F`) de cada
   tramo se baja para que Z no pase `machine.max_feed_z`, y el tiempo estimado sale de esa misma velocidad. Si el
   perfil de la impresora no dice `machine.max_feed_z`, se toma 5 mm/s, el valor del Ender-3 de fábrica que el motor ya
   usa para las demás velocidades que faltan, y se nombra en el mismo aviso. Con un eje Z lento el zigzag puede
   triplicar el tiempo (prueba ZI7), y el aviso de la regla 12 lo dice.
8. **Viajes:** van por encima de las crestas de la capa.
9. **No va junto con:** `infill.waves` (ondas entrelazadas), `curved.mode` (capas curvas), el relleno combinado de
   varias capas, la altura por zonas y las bandas del guiado por tensiones. Con cualquiera de ellos el laminado termina
   con error de ajustes (código 2) y el mensaje nombra los dos ajustes.
10. **Vista previa:** muestra el relleno con su forma en Z. En la vista de una sola capa se ven los dientes.
11. **Interfaz** (propuesta para `settings_panel.md`): las dos formas de relleno no planar van en **un solo renglón**,
    «Relleno no planar», con tres opciones: «No», «Zigzag en Z» y «Ondas entrelazadas». Hoy son dos casillas
    («Relleno ondulado» y «Ondas entrelazadas») con nombres parecidos, y marcar las dos termina en error. Debajo, solo
    los ajustes de la opción elegida. La ayuda (ⓘ) dice qué mejora (la unión entre capas, medida en paredes en el
    artículo), qué cuesta (tiempo y tamaño del G-code, regla 12) y con qué patrones funciona.
12. **Costo:** con zigzag el G-code es mucho más largo, porque cada diente son varios movimientos, y la impresión
    tarda más. El aviso previo a exportar dice cuánto tiempo suma el zigzag.

## Pruebas

Medidas en el motor (dev 45607f0, 03-11-2026) como caja negra, con el perfil de fábrica, capas de 0,2 mm, boquilla de
0,4 mm. «Cubo de 80» es un cubo de 80 mm. «dz» es la Z de cada movimiento con extrusión dentro de los bloques
`;NONPLANAR:1` … `;NONPLANAR:0`, menos la Z de su capa.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| ZI1 | Cubo de 80 | `grid` al 20 %, `infill.nonplanar` sí | dz de −0,25 a +0,25 mm (diente de 0,5 mm); 394 capas con zigzag |
| ZI2 | Cubo de 80 | ZI1 con `nonplanar_amplitude` 100 | dz de −0,10 a +0,10 mm |
| ZI3 | Cubo de 80 | `grid` al 80 %, `infill.nonplanar` sí | dz de −0,20 a +0,20 mm: el tope de la regla 3 achica el diente de 0,5 a 0,4 mm |
| ZI4 | Cubo de 80 | `triangles`, `stars`, `honeycomb`, `aligned_rectilinear` o `concentric` al 20 %, `infill.nonplanar` sí | Hay bloques con zigzag en los cinco |
| ZI5 | Cubo de 80 | `rectilinear`, `gyroid`, `cubic` o `lightning`, `infill.nonplanar` sí | Ningún bloque con zigzag; aviso `nonplanar_ignored` |
| ZI6 | Cubo de 80 | `grid` al 20 % sin y con zigzag | Tiempo estimado 10 h 39 min sin zigzag y 12 h 42 min con zigzag (+19 %); el G-code pasa de unas 98 000 a unos 2,6 millones de líneas |
| ZI7 | Cubo de 80 | ZI1 sin `machine.max_feed_z` en el perfil, y después con 5 mm/s | Medido en 45607f0/177eae6: los tramos con zigzag piden Z a 28,3 mm/s en los dos casos; el tiempo estimado pasa de 12 h 42 min a 1 d 10 h 11 min con 5 mm/s. Esperado: con 5 mm/s ningún tramo pide Z a más de 5 mm/s en su `F`, y sin el dato se usa 5 mm/s con aviso (regla 7) |
| ZI8 | Cubo de 10 mm | `grid` al 20 %, `infill.nonplanar` sí | Ningún bloque con zigzag (no hay lugar, regla 5). Esperado: el aviso de la regla 5. Hoy no hay aviso |
| ZI9 | Cubo de 10 mm | `infill.nonplanar` y `infill.waves` sí | Código 2: «infill.nonplanar: not with the interlocking waves (infill.waves)» |
| ZI10 | Cubo de 10 mm | `infill.nonplanar` sí y `curved.mode` = `wave` (E relativa) | Código 2: «infill.nonplanar: not with curved layers (curved.mode)» |
| ZI11 | Cubo de 80 | ZI1 | Medido: las puntas de los 21 276 tramos con zigzag, a 1,2 mm del borde de la pieza (junto a la pared interna), quedan a dz = 0 (regla 4) |
| ZI12 | Probetas de tracción en Z (las de la calibración de resistencia), PLA | Plano y con zigzag, cinco de cada una, impresas por el dueño | Resistencia y deformación a la rotura de cada grupo. Es la prueba de que el zigzag mejora el relleno; hasta tenerla, la ayuda dice que la mejora se midió en paredes |
