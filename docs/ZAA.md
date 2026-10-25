# Z anti-aliasing

Pedido del dueño (2026-10-25): «seguí con la altura variable por Z y por Z anti-aliasing así las combinamos para
máxima calidad», también «en uno complejo». La altura variable por Z está en `docs/VARIABLE_LAYERS.md`.

**Qué hace:** con capas planas, un techo inclinado sale como una escalera: cada capa está entera o no está. Con Z
anti-aliasing, donde la superficie del modelo corta una capa, las extrusiones que se ven arriba van a la Z de la
superficie y extruyen solo el espesor que queda. El techo sigue al modelo.

## De dónde sale

- **La ficha** `docs/spec/features/z_antialiasing.md` (de Tisma Slicer, como caja negra): qué baja, hasta dónde, el
  material y los requisitos. Sus pruebas Z1 y Z2 son pruebas del motor.
- **El artículo:** Song, H.-C., Ray, N., Sokolov, D. y Lefebvre, S., *Anti-aliasing for fused filament deposition*,
  Computer-Aided Design 89 (2017) 25–34 (arXiv 1609.03032). Aporta:
  - la deformación de las capas hacia la superficie, que nunca pasa de media capa (Sección 3.1), también hacia
    arriba;
  - el orden de abajo hacia arriba, para que la boquilla no pase al lado de un cordón más alto que acaba de
    dejar (Sección 4).
- **Diseño propio:**
  - el índice de la superficie por celdas;
  - el corte de los caminos cada 0,2 mm;
  - la escritura con Z por punto;
  - el orden por franjas de altura y la pared externa primero;
  - la combinación con la altura por zona;
  - el chequeo.

## Ajustes (`docs/CONFIG.md`, grupo `zaa`)

| Clave | Por defecto | Qué hace |
|---|---|---|
| `zaa.enabled` | no | Activa el Z anti-aliasing |
| `zaa.min_thickness` | 25 % | Hasta dónde puede bajar una extrusión: nunca queda más fina que esta parte de la capa |
| `zaa.raise` | no | También sube hasta la superficie que queda por encima de la capa, hasta media capa (el artículo). La ficha solo baja |

En la interfaz, en Impresión: **Z anti-aliasing**, **Espesor mínimo** y **Subir a la superficie**. El perfil de
fábrica **Máxima calidad** lo trae prendido con subida, junto con la altura por zona y por pendiente.

## Cómo funciona

1. **La superficie:** las caras de la malla que miran hacia arriba, en una grilla de celdas en XY (unas dos caras
   por celda, de 0,1 mm como mínimo). Para un punto (x, y) devuelve la Z de la superficie que hay encima, dentro de un
   rango, la más cercana al tope de la capa.
2. **Qué se mueve:**
   - las paredes (externa e internas);
   - el relleno de las superficies de arriba;
   - el relleno de huecos;
   - en la altura por zona, también los de la capa gruesa, con la base y el tope del grupo.

   El relleno interno, el sólido de abajo, los puentes y el soporte quedan en la Z de su capa.
3. **La altura de cada punto,** para una capa de base b, tope t y alto h:
   - si la superficie está entre b y t, el punto va a la superficie, pero nunca más abajo que
     `t − (1 − min_thickness) · h`;
   - con `raise`, si la superficie está entre t y `t + h/2`, el punto sube hasta ella. Más arriba la capa siguiente
     cubre ese lugar (con el corte a mitad de capa), así que el punto queda en t.
4. **El corte:** donde un tramo sale del tope, se parte cada 0,2 mm (o medio ancho de línea), así cada pedazo
   sigue la superficie. Los tramos que quedan en el tope no se tocan.
5. **El material:** el escritor pone la Z de cada punto (redondeada como se escribe) y extruye el espesor que queda
   hasta la base de la capa, el promedio de los dos extremos de cada pedazo: `ancho × (h + (dz₀ + dz₁)/2) × largo`.
   El tope de flujo usa ese mismo espesor.
6. **Los viajes:** antes de viajar, la boquilla vuelve al tope de la capa. Con `raise`, viaja por encima del cordón
   más alto que subió en esa capa. El G-code sigue con coordenadas absolutas en XYZ y sin arcos, como pide la ficha.
7. **El orden** (del artículo, simplificado):
   - en cada grupo de caminos, los más bajos primero, por franjas de 0,02 mm;
   - en una isla con paredes fuera del tope, la pared externa (la más baja) va antes que las internas.

   El corte en subcaminos con un grafo de restricciones de altura y la búsqueda del orden con menos costuras
   (Sección 4.1–4.2) quedan para después.

## El chequeo y la vista previa

- **El chequeo** (`docs/GCODE_CHECK.md`) acepta una extrusión fuera de la Z de su capa solo con ZAA:
  - hasta `(1 − min_thickness) · h` por debajo;
  - con `raise`, hasta `h/2` por encima.

  El material que espera es el del espesor que queda bajo los dos extremos. La Z máxima permitida suma media capa
  con `raise`.
- **La vista previa** dibuja cada cordón con ese espesor.

## Con la altura variable

- **La altura por pendiente** (`cusp_height`) baja el escalón de las paredes en pendiente; ZAA baja el de las
  superficies de arriba. Se complementan:
  - en una pendiente suave, ZAA ya deja la superficie casi exacta;
  - en una empinada, las subcapas finas acotan el escalón de la pared.
- **La altura por zona:** ZAA mueve también los caminos de la capa gruesa, con su propio alto.

## Medido (2026-10-25)

**Cómo se mide:**
- cúpula sobre columna de 40 mm y esfera de 100 mm, generadas;
- el error es la distancia entre el tope de cada cordón visible y la superficie exacta de la cúpula, en los cordones
  que están arriba: la superficie está dentro de su capa o a menos de media capa por encima;
- largo ponderado: media de |error| y percentil 95; también la media en las pendientes de menos de 30°;
- «zona + pendiente» es 0,3 / 0,1 mm con escalón de 0,05 mm.

| Pieza | Modo | Tiempo | Error medio | p95 | Media < 30° |
|---|---|---|---|---|---|
| cúpula | plano 0,2 mm | 55,5 min | 0,049 | 0,096 | 0,029 |
| cúpula | ZAA (solo baja, la ficha) | 55,5 min | 0,045 | 0,096 | 0,018 |
| cúpula | ZAA con subida | 55,6 min | 0,006 | 0,010 | 0,005 |
| cúpula | zona + pendiente | 56,1 min | 0,015 | 0,025 | 0,015 |
| cúpula | zona + pendiente + ZAA con subida | 56,1 min | **0,004** | **0,006** | 0,004 |
| cúpula | plano 0,1 mm | 109,6 min | 0,028 | 0,047 | 0,028 |
| cúpula | plano 0,05 mm | 217,9 min | 0,013 | 0,024 | 0,013 |
| esfera | plano 0,2 mm | 447,2 min | 0,048 | 0,094 | 0,030 |
| esfera | ZAA (solo baja) | 447,2 min | 0,043 | 0,094 | 0,017 |
| esfera | ZAA con subida | 447,7 min | 0,003 | 0,005 | 0,002 |
| esfera | zona + pendiente + ZAA con subida | 526,1 min | **0,001** | **0,002** | 0,001 |
| esfera | plano 0,05 mm | 1787,7 min | 0,014 | 0,024 | 0,014 |

**Lo que dicen las medidas:**
- **Solo bajar (la ficha)** corrige la mitad del escalón: lo que sobra por encima de la superficie. El error que
  queda es lo que falta por debajo, y el p95 no cambia.
- **Con subida (el artículo)** la superficie de arriba queda a micras: en la esfera, 18 veces menos error medio que
  plano a 0,2 mm, en el mismo tiempo, y 5 veces menos que plano a 0,05 mm, en la cuarta parte del tiempo.
- **Combinado con zona + pendiente**, que acota el escalón de las paredes (`docs/VARIABLE_LAYERS.md`), es la mejor
  terminación por tiempo del banco: el perfil **Máxima calidad**.
- **Laminar** cuesta un 8–9 % más (cúpula 166 → 179 ms, esfera 736 → 804 ms).
- **Con ZAA apagado,** el G-code es el mismo de antes, salvo las tres líneas de los ajustes al final.

**No medido:**
- la impresión real (necesita una impresora);
- el daño que hace la boquilla a cordones vecinos más altos, que el orden simplificado reduce pero no elimina.

## Pruebas

En `tests/test_zaa.cpp`:
- la superficie sobre un punto: un techo, fuera de rango, fuera de la pieza, la base que mira hacia abajo;
- un camino a lo largo de Y en X = 91,78, el ejemplo de la ficha: la superficie en 3,156 dentro de la capa de 3,0 a
  3,2. También un camino pendiente arriba, con piso, superficie y tope; con subida; y uno sin superficie en la capa,
  que no cambia;
- **Z1** de la ficha en techos de 5°, 10° y 15°:
  - cada extrusión visible donde la superficie cae dentro de la capa va a `máx(superficie, tope − 0,15)`, con error
    de 0,0006 mm (el redondeo);
  - su material por mm es el espesor que queda sobre 0,2, con error de 0,03;
  - el relleno interno no se mueve;
  - el G-code pasa el chequeo;
  - la vista previa dibuja cada cordón con el espesor que queda;
- **Z2:** sin ZAA, ninguna extrusión sale de la Z de su capa;
- **con subida, altura por zona y por pendiente:** el G-code pasa el chequeo y las Z quedan entre `tope − 0,75 h` y
  `tope + h/2`.

El fuzz sortea `zaa.enabled`, `min_thickness` y `raise`.

**Z3 de la ficha:** en los techos de 10° y 15°, Tisma Slicer solo baja los perímetros externos, y lo mide sobre sus
propias capas. El motor cumple la misma regla (Z1) en esos techos. No se compara número a número: depende de dónde
cae el borde de cada capa.
