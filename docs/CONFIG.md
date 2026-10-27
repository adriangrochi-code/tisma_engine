# Ajustes del motor (`--config ajustes.json`)

El formato es propio del motor: un objeto JSON con grupos, y nombres elegidos por este equipo. Las claves de otros
programas no se usan. Si Tisma Slicer necesita traducir sus ajustes, lo hace su adaptador, del lado de Tisma.

Reglas:
- Todas las claves son opcionales; las que faltan toman el valor por defecto.
- Una clave desconocida dentro de un grupo implementado es un error, para que los errores de tipeo no pasen en
  silencio.
- Un grupo desconocido (ni implementado ni planeado) también es un error. En los dos casos, el comando termina con el
  código de salida 2 y el mensaje nombra la clave.
- Los nombres de los grupos y las claves son estables. Si alguno cambia, se anuncia y se documenta acá.
- Los grupos todavía no implementados se aceptan, pero el motor avisa que no los usa.
- Las longitudes van en milímetros.
- La interfaz reparte estos grupos en perfiles de impresora, filamento e impresión (`docs/PROFILES.md`); un archivo de
  perfil usa este mismo formato, solo con los ajustes de su tipo.

## Grupos implementados

### `layers`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `height` | número | 0.2 | 0,01 a 10 | Alto de las capas, salvo la primera. Al laminar, nunca más que la boquilla ni `machine.max_layer_height`, ni menos que `machine.min_layer_height` (`check_settings`) |
| `first_height` | número | 0.2 | 0,01 a 10 | Alto de la primera capa, con los mismos topes que `height` |

Además, en `layers`, **la altura de capa por zona** (`docs/LOCAL_LAYERS.md`):

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `detail_height` | número | 0 | 0 a 10 | Alto de las subcapas finas, en mm. Con un valor menor que `height`, las capas de `height` se parten en `k` subcapas (`height / detail_height` redondeado, de 2 a 4, ninguna de menos de 0,04 mm) solo donde la pieza cambia; en el resto se imprime una capa gruesa por grupo. 0: apagado |
| `detail_tolerance` | número | 0,05 | 0,001 a 1 | El escalón que se acepta, en mm: donde las secciones de un grupo difieren menos que esto, va grueso |
| `detail_margin` | número | 0 | 0 a 10 | Cuánto se agranda la zona fina alrededor del detalle, en mm. 0: tres anchos de línea |

Y **la altura variable por Z** (`docs/VARIABLE_LAYERS.md`):

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `heights` | lista de `[z, altura]` | `[]` | z de 0 a 10000, ordenada sin repetir; altura de 0,01 a 10 | Tramos: desde cada z (mm sobre la cama) hacia arriba, esa altura de capa (la capa gruesa con `detail_height`). Debajo del primero, `height`. Cada altura se acota a `machine.min_layer_height` y `max_layer_height`, y un tramo empieza una capa |
| `cusp_height` | número | 0 | 0 a 1 | Altura automática por pendiente: el escalón más alto, en mm, que puede dejar una superficie inclinada (una cara de normal n admite capas de hasta `cusp_height / \|n_z\|`). Sin `detail_height`, la capa entera se afina; con él, la pendiente decide cuántas subcapas lleva cada capa gruesa. Las caras planas cierran una capa. 0: apagado |

### `slicing`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `fill_mode` | texto | `"union"` | `union`, `even_odd`, `fill_holes` | Cómo se decide qué está adentro cuando los contornos se superponen o se cruzan: `union` suma los volúmenes, `even_odd` alterna adentro y afuera en cada cruce, `fill_holes` además rellena los agujeros |
| `gap_closing_radius` | número | 0.049 | ≥ 0 | Largo máximo de un hueco que se cierra al unir contornos abiertos |
| `xy_offset` | número | 0 | −10 a 10 | Agranda (positivo) o achica (negativo) cada capa en XY; los agujeros se mueven al revés |
| `first_layer_shrink` | número | 0 | 0 a 2 | Cuánto se achica además la primera capa, contra la pata de elefante |
| `simplify_tolerance` | número | 0.01 | ≥ 0 | Se quitan los vértices de los contornos que se apartan menos que esto de la recta entre sus vecinos; 0 no simplifica. Menos segmentos aceleran los pasos siguientes (en una capa circular de 708 segmentos, 0,01 mm deja 208 y el eje medio tarda 3,4 veces menos) |

### `walls`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `count` | entero | 3 | 0 a 1000 | Paredes desde cada borde. Donde la pieza es más angosta que esas paredes, los cordones cambian de ancho para llenarla sin huecos (Kuipers et al. 2020); donde es más ancha, quedan las paredes al ancho de `width` y el medio queda para el relleno. 0: sin paredes, todo es relleno |
| `width` | número | 0.4 | 0,05 a 40 | Ancho preferido de los cordones (w*); por ahora fijo, más adelante saldrá de la boquilla del grupo `machine`. Al laminar, de un cuarto a cuatro veces la boquilla; fuera de 0,6 a 2,5 veces, un aviso |
| `seam` | texto | `"aligned"` | `aligned`, `nearest`, `random`, `rear` | Dónde empieza cada lazo: `aligned`, en una esquina cóncava si hay, cerca de la costura de la capa de abajo; `nearest`, el punto más cercano al final del movimiento anterior; `random`, al azar pero igual en cada corrida; `rear`, el punto de Y máxima |
| `scarf` | texto | `"nowhere"` | `nowhere`, `contours`, `everywhere` | Costura en bufanda (ficha `walls_extra.md`, regla 4): la pared externa empieza en una rampa que sube desde la capa de abajo en vez de un escalón, y al final del lazo pasa otra vez sobre la rampa con el cordón cada vez más fino, con el filamento de un lazo común. Casi no deja costura. `contours`: solo la pared de afuera de cada pieza; `everywhere`: también la de los agujeros. Nunca en la primera capa |
| `scarf_only_smooth` | booleano | `true` | — | La bufanda solo en los lazos sin esquinas marcadas (giros de más de 30°): en un cubo, la costura queda en una esquina, donde no se ve |
| `scarf_start_height` | número | 0 | 0 a 100 | Altura a la que empieza la rampa, en % de la capa, sobre la capa de abajo |
| `scarf_length` | número | 20 | más de 0, hasta 1000 | Largo de la rampa, en mm, antes de la costura |
| `scarf_entire_loop` | booleano | `false` | — | La rampa da la vuelta entera: la pared externa sube en espiral a lo largo de toda la capa y una segunda vuelta la completa |
| `scarf_max_segment` | número | 1 | más de 0, hasta 1000 | El tramo más largo de la rampa y de su segunda pasada, en mm |
| `scarf_inner` | booleano | `false` | — | También las paredes internas empiezan en rampa |
| `min_line_width` | número o texto | `"85%"` | 0 a 40 mm | Ancho mínimo de un cordón, en mm o en % de `machine.nozzle_diameter` (0,34 mm con boquilla de 0,4): una pared de un solo cordón más fina que esto se imprime con un cordón de este ancho, centrado en la pared (ficha `perimeters.md`, regla 3) |
| `min_detail` | número o texto | `"25%"` | 0 a 40 mm | Una pared más fina que esto, en mm o en % de la boquilla (0,1 mm con 0,4), no se imprime; si la pieza entera queda vacía, es un error |
| `generator` | texto | `"variable"` | `variable`, `fixed` | `variable`: cordones de ancho variable que llenan la región (Kuipers et al. 2020). `fixed`: lazos de `width` a `width` de distancia, cada uno donde entra un cordón entero, y lo que queda en el medio, con relleno de huecos (ficha `walls_extra.md`, regla 1). El relleno concéntrico sigue con ancho variable |
| `gap_fill` | booleano | `true` | — | Con `generator` `fixed`: lo que queda entre las paredes (y entre ellas y la región del relleno) de al menos medio cordón se llena con cordones de ancho variable, `;TYPE:Gap fill`, a `speed.gap_fill`; los extremos libres siguen medio ancho hasta el borde del hueco. Apagado, quedan vacíos (regla 2) |
| `detect_overhangs` | booleano | `true` | — | Los tramos de pared que quedan enteros sobre el aire (el cordón sin nada de la capa de abajo debajo) o sobre el soporte se marcan `;TYPE:Overhang perimeter` y van a la velocidad y con el ventilador de los puentes (ficha `walls_extra.md`, regla 8). Apagado, la pared se imprime entera como cualquier otra. Sobre el soporte, el tramo es igual de grueso que la capa más la separación en Z (ficha `supports_skirt.md`, regla 4) |

### `vase`

Modo vaso, ficha `docs/spec/features/vase_mode.md`: la pieza se imprime como una sola pared que sube en espiral, sin
costura.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `enabled` | booleano | `false` | — | Activa el modo vaso: una sola pared, sin relleno ni capas de arriba (se ignoran esos ajustes). Las primeras `surfaces.bottom_layers` capas son planas, con relleno sólido, y la última de ellas es la superficie de arriba; desde ahí cada capa es una vuelta de la pared externa que empieza donde terminó la de abajo, sin viaje ni retracción, y sube la altura de una capa a lo largo de la vuelta, con el caudal de siempre. Sin capas de abajo, la primera capa es una vuelta plana. No se puede con soportes. Una capa con más de un contorno (dos islas, un agujero) se imprime plana |

### `fuzzy`

Piel difusa, ficha `docs/spec/features/fuzzy_skin.md`: la pared externa pasa por puntos nuevos, a distancias al azar
y corridos al azar hacia afuera o hacia adentro, y la superficie queda rugosa. Las paredes internas no cambian. Donde
se pintó piel difusa (`docs/PAINTING.md`) se aplica con estos ajustes aunque `mode` sea `"none"`.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `mode` | texto | `"none"` | `none`, `external`, `all` | `external`: la pared externa de los contornos de afuera; `all`: también la de los agujeros |
| `thickness` | número | 0.3 | 0 a 5 | Cuánto se aparta cada punto de la línea, como máximo, para cada lado, en mm |
| `point_distance` | número | 0.8 | 0,1 a 20 | Distancia media entre los puntos nuevos, en mm: cada una, al azar entre la mitad y una vez y media |

### `arcs`

Arcos en voladizos, ficha `docs/spec/features/arc_overhangs.md`: un voladizo que sale de la capa de abajo por un solo
lado (no un puente, que se apoya de los dos) se imprime sin soporte con arcos que crecen alrededor de un centro, 1,2 mm
adentro de lo que lo sostiene y frente al medio del borde apoyado; cada arco pisa al anterior. Van antes que las
paredes de su isla, con cordón redondo (la sección de un cordón redondo del ancho de las paredes), como `;TYPE:Overhang
perimeter`, con el ventilador de los puentes y retracción antes de cada uno. La capa siguiente lo cubre con relleno
sólido. Con soportes, el soporte va igual debajo y la capa sobre él lleva arcos en vez del puente grueso.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `enabled` | booleano | `false` | | Activa los arcos |
| `overlap` | número | 10 | 0 a 90 | Cuánto pisa cada arco al anterior, en % del ancho: los arcos van a `walls.width × (1 − overlap)` (0,36 mm con 0,4) |
| `speed` | número | 5 | 0 a 1000 | Velocidad de los arcos, en mm/s; 0, la de los puentes (`speed.bridges`). Es la de la ficha, no una de la impresora: lenta para que cada arco se enfríe antes del siguiente |
| `max_distance` | número | 0 | 0 a 1000 | Hasta qué distancia del centro llegan los arcos, en mm; más allá, el voladizo se imprime como siempre. 0: sin límite |

### `review`

La revisión de estabilidad (fichas `docs/spec/features/stability.md` y `print_review.md`, regla 6), solo en piezas sin
soportes: avisa de partes que empiezan en el aire (crítico), de lo impreso que se vuelca por su peso, de columnas
esbeltas que se doblan con el empuje de la boquilla y de la pieza que se despega de la cama. Los avisos van en
`--warnings` con `severity` (`critical`, `warning` o `information`). Los dos primeros valores se miden con la pieza de
calibración de la ficha (regla 6); mientras no estén medidos (0), las columnas solo se informan (las tres más
flexibles, en mm por newton, si se doblan al menos una décima de `max_deflection` por newton) y el despegue no se
revisa. El módulo a través de las capas es el del PLA (2600 MPa) hasta
que el perfil del filamento traiga su material. En el perfil de la impresora.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `nozzle_force` | número | 0 | 0 a 1000 | Empuje lateral de la boquilla sobre la capa que imprime, en N; 0: sin medir |
| `bed_adhesion` | número | 0 | 0 a 1000 | Lo que aguanta la unión con la cama, en N/mm², para esa cama y ese material; 0: sin medir |
| `max_deflection` | número | 0.1 | más de 0, hasta 10 | Flecha máxima aceptada en la punta de una parte esbelta, en mm |

### `surfaces`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `top_layers` | entero | 3 | 0 a 1000 | Capas sólidas debajo de cada superficie superior expuesta |
| `bottom_layers` | entero | 3 | 0 a 1000 | Capas sólidas encima de cada superficie inferior expuesta |
| `top_min_thickness` | mm | 0 | 0 a 100 | Si las capas sólidas de arriba suman menos, se agregan capas del alto nominal hasta llegar (`docs/INFILL.md`) |
| `bottom_min_thickness` | mm | 0 | 0 a 100 | Lo mismo con las de abajo |
| `top_pattern` | texto | `"monotonic"` | `monotonic`, `rectilinear` | Superficies superiores: `monotonic`, líneas sueltas todas en el mismo sentido, cada una después de las de abajo que la tocan (sin volver sobre lo hecho); `rectilinear`, en zigzag como las inferiores |

### `infill`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `density` | número | 20 | 0 a 100 | Densidad del relleno disperso, en %: líneas separadas `walls.width` / densidad. 0: sin relleno disperso. Las superficies sólidas van siempre al 100 % |
| `angle` | número | 45 | cualquiera | Ángulo de las líneas respecto del eje X, en grados; las capas impares suman 90 |
| `wall_overlap` | número | 25 | 0 a 100 | Cuánto entran en la pared interna, en % del ancho de cordón, las líneas sólidas que no se unen en zigzag (superficies superiores monotónicas). Con 25 %, el material depositado queda entre −0,5 % y +1,5 % del área |
| `nonplanar` | sí/no | no | — | Relleno ondulado: sube y baja en Z, igual en todas las capas, solo con patrones que se repiten capa a capa (`docs/NONPLANAR_INFILL.md`) |
| `nonplanar_amplitude` | % | 250 | 0 a 500 | Altura de la onda, de valle a cresta, en % de la altura de capa |
| `nonplanar_wavelength` | mm | 2 | 0,5 a 20 | Largo de la onda |
| `solid_every` | entero | 0 | 0 a 1000 | Las capas N, 2N, 3N… llevan relleno sólido en todo el relleno disperso, impreso como puente sobre el de abajo; 0: nunca (`docs/INFILL.md`) |
| `solid_below_area` | número | 70 | 0 a 10000 | Una isla del relleno de una capa más chica que esto, en mm², se llena sólida: una pata fina, un perno (ficha `surfaces_infill.md`, regla 9). Se mide lo que queda adentro de las paredes, no la cara entera: con 3 paredes de 0,4 mm, un cilindro de 4,8 mm de radio (72 mm² de cara) deja unos 41 mm² de relleno y sale sólido. Con `density` 0 no se aplica. 0: nunca |
| `pattern` | texto | `"rectilinear"` | `rectilinear`, `grid`, `gyroid`, `triangles`, `cubic`, `adaptive_cubic`, `support_cubic`, `lightning`, `stars`, `aligned_rectilinear`, `zigzag`, `line`, `honeycomb`, `concentric`, `hilbert`, `archimedean`, `octagram` | Patrón del relleno disperso: `rectilinear`, líneas que giran 90° cada capa y se unen en zigzag; `grid`, las dos direcciones en cada capa, cada una al doble de separación (el mismo material); `gyroid`, el corte de la superficie giroide a la altura de cada capa, con el período que da la densidad (`angle` no se usa); `triangles`, tres familias de líneas a `angle`, +60° y +120°, cada una con un tercio del material, iguales en todas las capas; `cubic`, las mismas tres familias corridas `z / √2` con la altura: las caras de cubos apoyados en un vértice, resistentes en las tres direcciones; `adaptive_cubic`, el cúbico a 0°, 60° y 120° con cubos chicos junto a las paredes y superficies y más grandes adentro, con menos material en piezas grandes; `support_cubic`, el adaptativo sin contar las capas sólidas de abajo, que el relleno no sostiene; `lightning`, solo ramas desde las paredes hasta debajo de los techos, para piezas que solo se miran; `stars`, los triángulos con una familia corrida media separación (estrellas de seis puntas, el patrón por defecto de Tisma Slicer); `aligned_rectilinear`, el rectilíneo sin girar entre capas; `zigzag`, igual al rectilíneo; `line`, el rectilíneo con una línea de cada dos girada 6,7°; `honeycomb`, panales hexagonales que coinciden en todas las capas, con la dirección principal girando 60° por capa; `concentric`, lazos paralelos al borde; `hilbert`, la curva de Hilbert girada `angle`; `archimedean`, cuerdas de una espiral de Arquímedes alrededor del centro de la pieza; `octagram`, una espiral con forma de estrella de ocho puntas; las tres curvas son iguales en todas las capas (`docs/INFILL.md`) |
| `combine_layers` | entero | 1 | 1 a 20 | Combina el relleno disperso de N capas en una (`docs/INFILL.md`): lo que todas tienen se imprime una vez, en la de arriba, con el alto de las N. Nunca más alto que `combine_max_height` ni que la boquilla. No se usa con `layers.detail_height`, que ya imprime grueso el relleno. 1: cada capa el suyo |
| `combine_auto` | booleano | false | | N es la mayor cantidad de capas que entra en `combine_max_height` |
| `combine_max_height` | número o texto | `"100%"` | mm (0 a 10), o `"N%"` de la boquilla | Alto máximo de la capa de relleno combinada; nunca pasa `machine.nozzle_diameter` |
| `dense_under_top` | booleano | false | | La capa de relleno disperso justo debajo de una superficie de arriba va a `dense_density`, solo bajo esa superficie (`docs/INFILL.md`): permite un relleno general más liviano sin que el techo se hunda |
| `dense_density` | número | 40 | 0 a 100 | Densidad de esa capa, en %, con el mismo patrón y la misma dirección |
| `stress_density` | número | 0 | 0 a 100 | Relleno guiado por tensiones (`docs/INFILL.md`): con un caso de carga (`slice --case caso.json`), el relleno sube por bandas hasta esta densidad donde la pieza trabaja más. 0: apagado |

### `machine`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `nozzle_diameter` | número | 0.4 | > 0 | Diámetro de la boquilla, en mm |
| `filament_diameter` | número | 1.75 | > 0 | Diámetro del filamento, en mm: la E de cada tramo es su volumen sobre esta sección |
| `relative_extrusion` | booleano | false | | `true`: E relativa (`M83`), cada movimiento lleva su E; `false`: absoluta (`M82`), con `G92 E0` en cada capa |
| `flavor` | texto | `"reprap"` | `marlin`, `klipper`, `reprap` | Dialecto de G-code (cambiará los comandos de ventilador, temperatura y aceleración) |
| `start_gcode`, `end_gcode` | texto | `""` | | G-code de inicio y de fin |
| `variables` | objeto | `{}` | nombre: valor o lista | Valores para `[nombre]`, `{nombre}` y `{nombre[i]}` dentro del G-code de inicio y fin. Los da quien llama al motor (el adaptador de Tisma pone ahí los valores de sus perfiles), así los nombres de los perfiles no entran al motor. Los números se escriben como en el JSON; un nombre desconocido queda como está. Los nombres no llevan saltos de línea ni corchetes; en el comentario del final del G-code, los saltos de línea de los valores se escriben como `\n` |
| `max_feed_x`, `max_feed_y` | número | — (del perfil de la impresora) | > 0, o `null` | Velocidad máxima de cada eje, en mm/s, para estimar el tiempo. Sin valor (`null`): no se sabe y no se aplica |
| `max_feed_z` | número | — (del perfil de la impresora) | > 0, o `null` | Ídem para Z |
| `max_feed_e` | número | — (del perfil de la impresora) | > 0, o `null` | Ídem para el extrusor |
| `acceleration` | número | 1000 | > 0 | Aceleración de los movimientos que extruyen, en mm/s². Se escribe después del G-code de inicio, con `travel_acceleration`, para que la impresora use lo que supone el tiempo estimado: `M204 P… T…` (Marlin y RepRapFirmware) o `SET_VELOCITY_LIMIT ACCEL=…` (Klipper, un solo valor) |
| `travel_acceleration` | número | 1250 | > 0 | Aceleración de los viajes y las retracciones, en mm/s² |
| `corner_speed` | número | — (del perfil de la impresora; respaldo 10) | ≥ 0 | Velocidad a la que se puede pasar por una esquina de 90° o más, en mm/s; en giros más suaves, la menor de las dos velocidades por el coseno del giro, nunca menos que esto |
| `bed` | lista de puntos | `[]` | vacía, o 3 o más `[x, y]` | Polígono de la cama en mm (por ejemplo `[[0, 0], [250, 0], [250, 210], [0, 210]]`, la cama del banco). Si se da, un modelo con algún vértice fuera de ella (el borde cuenta como dentro) no se lamina: `slice` termina con código 4. Vacío: no se comprueba, porque el motor no mueve los modelos en X e Y y los del banco están centrados en el origen. En Z la pieza siempre se imprime apoyada en la cama: las alturas del G-code (y las de `changes`) cuentan desde su punto más bajo, esté donde esté la malla |
| `max_height` | número | 0 | ≥ 0 | Altura máxima de impresión en mm; un modelo más alto termina con código 4. 0: no se comprueba |
| `z_offset` | número | 0 | −10 a 10 | Se suma a todas las Z de la pieza en el G-code, también al comentario `;Z:` (ficha `gcode_extras.md`, regla 5); puede dar Z negativas. No toca el G-code de inicio. Los cambios por altura (`changes`) van por esa Z escrita |
| `min_layer_height`, `max_layer_height` | número | 0 | 0, o 0,01 a 10 | Las alturas de capa que imprime la máquina, en mm, para la altura variable (`docs/VARIABLE_LAYERS.md`). Nunca más que `nozzle_diameter`. 0: 0,04 mm y el diámetro de la boquilla |
| `max_flow` | número | 0 | 0 a 1000 | Flujo volumétrico máximo del hot end, en mm³/s. Ninguna extrusión lo pasa: su velocidad baja a flujo / (ancho × alto × `extrusion.flow`), redondeada hacia abajo a mm/min enteros. Con `extrusion.max_flow` vale el menor de los dos. 0: sin límite |
| `max_nozzle_temp`, `max_bed_temp` | número | 0 | 0 a 600 | Temperaturas máximas de la boquilla y la cama, en °C. Si las variables `temperature` o `bed_temperature`, o los cambios por altura, piden más, hay un aviso. 0: no se sabe |
| `max_chamber_temp` | número | 0 | 0 a 600 | Temperatura máxima de la cámara calefaccionada, en °C. 0: la impresora no calienta la cámara |
| `enclosed` | booleano | false | | La impresora es cerrada |
| `hardened_nozzle` | booleano | false | | La boquilla acepta filamentos abrasivos (acero endurecido o similar) |

Estas claves las llena el catálogo (`docs/CATALOG.md`, «Capacidades») con los datos del fabricante.

### `speed`

Velocidades en mm/s. Ninguna extrusión pasa de `max`; en la primera capa, ninguna pasa de `first_layer`, salvo el relleno con `first_layer_infill`.

**Velocidades** (decisión del dueño, 2026-10-26): ninguna velocidad tiene un valor por defecto en el motor. Todas
salen del perfil de la impresora elegida, porque dependen de la impresora: las de este grupo, `retraction.speed`,
`cooling.min_speed` y las de `machine` (`max_feed_*`, `corner_speed`). En los archivos, una sin valor se escribe
`null`. El motor no escribe G-code si falta alguna de las que necesita (todas menos `speed.max` y `machine.max_feed_*`,
que son topes: sin valor, no topan).

**Velocidades de respaldo:** solo cuando la impresora no trae una velocidad (una impresora creada de cero, una del
catálogo, que no sabe más que su velocidad máxima, o un archivo de ajustes que no la da), se usa la de una Creality
Ender-3 de fábrica, la máquina más común y la más segura para cualquier otra. Nunca reemplaza una que la impresora
trae. La interfaz lo dice al elegir la impresora; `slice` lo avisa (`warning: no speed for…` y el aviso
`speed_fallback`). Los números salen de lo que publica Creality para la Ender-3 (V2, Pro y S1):

| Clave | Respaldo | De dónde |
|---|---|---|
| `speed.walls` | 40 | Paredes: de 30 a 35 o 40 mm/s ([Creality](https://store.creality.com/blogs/all/ender-3-print-speed)) |
| `speed.outer_wall` | 30 | Los 30 mm/s de paredes de la misma página, para la de afuera |
| `speed.infill`, `speed.solid_infill` | 60 | Relleno de 40 a 60 mm/s; 60 mm/s, la velocidad de impresión recomendada (misma página) |
| `speed.top_solid_infill` | 40 | 40 mm/s para los detalles finos (misma página) |
| `speed.support` | 40 | Soporte de 30 a 40 mm/s (misma página) |
| `speed.travel` | 180 | Viajes a 180 mm/s (misma página) |
| `speed.first_layer` | 25 | Primera capa de 20 a 30 mm/s ([Creality, PETG en la Ender-3](https://store.creality.com/blogs/all/petg-print-speed-ender-3)) |
| `retraction.speed` | 25 | PLA: 25 mm/s ([Creality, filamentos para la Ender-3](https://store.creality.com/blogs/all/best-filament-for-ender-3-v2-pro-s1)) |
| `cooling.min_speed` | 10 | Valor propio del motor (Creality no da uno) |
| `machine.corner_speed` | 10 | Valor propio del motor (Creality no da uno) |

`speed.max` y `machine.max_feed_*` no tienen respaldo: sin valor, no topan nada. Una impresora del catálogo con
velocidad máxima conocida no pasa de ella en ninguna de las de respaldo.

| Clave | Tipo | Por defecto | Significado |
|---|---|---|---|
| `walls` | número | — (del perfil de la impresora; respaldo 40) | Paredes internas |
| `outer_wall` | número o texto | — (del perfil de la impresora; respaldo 30) | Pared externa: mm/s, o un porcentaje de `walls` como `"50%"` |
| `infill` | número | — (del perfil de la impresora; respaldo 60) | Relleno disperso |
| `solid_infill` | número | — (del perfil de la impresora; respaldo 60) | Superficies inferiores, y puentes y relleno de huecos sin velocidad propia |
| `top_solid_infill` | número | — (del perfil de la impresora; respaldo 40) | Superficies superiores |
| `support` | número | — (del perfil de la impresora; respaldo 40) | Soporte e interfaz |
| `travel` | número | — (del perfil de la impresora; respaldo 180) | Viajes |
| `first_layer` | número | — (del perfil de la impresora; respaldo 25) | Tope de la primera capa |
| `max` | número | — (del perfil de la impresora; sin respaldo) | Tope de toda extrusión; sin valor, ninguno |
| `bridges` | número | — (sin valor: `solid_infill`) | Puentes y perímetros en voladizo (`gcode_motion.md`, `walls_extra.md`, regla 8); los arcos sobre voladizos sin `arcs.speed` |
| `gap_fill` | número | — (sin valor: `solid_infill`) | Relleno de huecos (`speeds_accels.md`, regla 1) |
| `over_bridges` | número o texto | — (sin valor: `solid_infill`) | Relleno sólido de la capa que va justo encima de un puente sobre el aire, en mm/s o un porcentaje de `solid_infill` como `"50%"`; no la capa sobre las capas sólidas puestas sobre el relleno (`infill.solid_every`) (`speeds_accels.md`, regla 2). Los tramos de cada línea sobre el puente van a esta velocidad, el resto a la suya |
| `first_layer_infill` | número o texto | — (sin valor: `first_layer`) | Relleno sólido y superficie de la primera capa, en mm/s o un porcentaje de `solid_infill`; no queda topado por `first_layer` (`speeds_accels.md`, regla 3) |
| `dynamic_overhangs` | booleano | `false` | Las paredes más lentas cuanto menos de su cordón se apoya en la capa de abajo (`speeds_accels.md`, reglas 6 y 7): la parte apoyada sale de cuánto queda el centro del cordón adentro o afuera de la capa de abajo, contra su ancho; a 0, 25, 50 y 75 % van a `overhang_0` a `overhang_3`, apoyadas del todo a la suya, y en línea recta entre esos puntos, nunca más rápido que la suya. Los tramos en el aire (`Overhang perimeter`) van a `overhang_0`. No en el modo vaso |
| `overhang_0`, `overhang_1`, `overhang_2`, `overhang_3` | número o texto | 15, 15, 20, 25 | mm/s, o un porcentaje de `outer_wall` como `"50%"`; los de la ficha, no de la impresora |
| `flow_slope_up`, `flow_slope_down` | número | 0 | Ecualizador del caudal (`speeds_accels.md`, reglas 8 y 9), en mm³/s²: cuánto puede subir o bajar por segundo el caudal (sección del cordón por su velocidad) de una extrusión a la siguiente. Donde baja, el final de la primera se parte en pedazos cada vez más lentos hasta el caudal de la siguiente; donde sube, el comienzo de la segunda, cada vez más rápidos, salvo después de una retracción. Hasta 16 pedazos; si el camino es más corto que la rampa, la rampa se aprieta. 0: libre (de 0 a 1000) |
| `first_layer_over_raft` | número o texto | — (sin valor: `first_layer`) | Tope de todas las extrusiones de la primera capa de la pieza sobre la balsa, en mm/s o un porcentaje de cada velocidad como `"50%"` (`skirt_brim_raft.md`, regla 12); la capa siguiente vuelve a las suyas |
| `travel_z` | número | — (sin valor: `travel`) | Movimientos en Z: cambios de capa, saltos de `retraction.lift` y las subidas del Z anti-aliasing (`speeds_accels.md`, regla 4) |

### `acceleration`

Aceleraciones por tipo, en mm/s² (ficha `speeds_accels.md`, reglas 10 a 13). Antes de cada extrusión se fija la de
su tipo y, al terminar el tipo, se vuelve a `default`; antes de cada viaje, la de los viajes. El comando es
`M204 S…` (RepRapFirmware y Klipper), o en Marlin `M204 P…` para imprimir y `M204 T…` para los viajes, escrito solo
cuando cambia. Con `default` en 0 no se usan las de los tipos: solo se fija la de los viajes y las extrusiones siguen
con ella. Todas en 0 (lo de siempre): nada más que la `machine.acceleration` del comienzo. El tiempo estimado sigue
cada cambio. Todavía no hay topes por los límites de la máquina (se escriben en el G-code con `gcode_firmware.md`,
que falta), ni la de la torre de purga.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `default` | número | 0 | 0 a 100 000 | A la que se vuelve después de cada tipo, y la del soporte, la falda y el relleno de huecos. 0: sin aceleraciones por tipo |
| `walls` | número | 0 | 0 a 100 000 | Paredes internas; 0: `default` |
| `outer_wall` | número | 0 | 0 a 100 000 | Pared externa; 0: `walls` |
| `infill` | número | 0 | 0 a 100 000 | Relleno disperso; 0: `default` |
| `solid_infill` | número | 0 | 0 a 100 000 | Relleno sólido; 0: `infill` |
| `top_solid_infill` | número | 0 | 0 a 100 000 | Superficie de arriba; 0: `solid_infill` |
| `bridges` | número | 0 | 0 a 100 000 | Puentes y perímetros en voladizo; 0: `default` |
| `first_layer` | número | 0 | 0 a 100 000 | Toda la primera capa (con balsa, la de la balsa); 0: la de cada tipo |
| `first_layer_over_raft` | número | 0 | 0 a 100 000 | La primera capa de la pieza sobre la balsa; 0: la de cada tipo |
| `travel` | número | 0 | 0 a 100 000 | Viajes; 0: ninguna propia |
| `short_travel` | número | 0 | 0 a 100 000 | Viajes más cortos que `retraction.min_travel`; 0: `travel` |

### `ironing`

Planchado (ficha `ironing.md`): después del relleno de arriba de cada isla, en la misma capa y a la misma Z, la
boquilla vuelve a pasar sobre la superficie expuesta (también sobre los perímetros, hasta media boquilla del borde) en
líneas a 45° de las del relleno de arriba, `;TYPE:Ironing`. Cada línea va como un cordón de `spacing × flow` de ancho
y la altura de la capa: por mm, `flow` de lo que pondría una capa normal a esa separación (0,003 mm³ con los valores
de siempre). Las partes más angostas que dos líneas no se planchan.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `enabled` | booleano | `false` | — | Plancha |
| `surfaces` | texto | `"top"` | `top`, `topmost`, `solid` | `top`: lo que la capa de arriba no cubre; `topmost`: lo que nada de más arriba cubre (la superficie más alta de cada parte); `solid`: todas las superficies sólidas de cada capa |
| `flow` | número | 15 | más de 0 a 100 | Material, en % de una capa de la altura normal |
| `spacing` | número | 0,1 | 0,01 a 2 | Separación entre líneas, en mm |
| `speed` | número | 15 | más de 0 a 1000 | Velocidad, en mm/s (la de la ficha, no una de la impresora) |

### `retraction`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `length` | número | 2 | 0 a 50 | Cuánto filamento se retrae, en mm; 0 no retrae |
| `speed` | número | — (del perfil de la impresora; respaldo 25) | > 0 | Velocidad de la retracción y de su vuelta, en mm/s |
| `lift` | número | 0 | 0 a 10 | Cuánto sube Z durante un viaje que retrae, en mm |
| `min_travel` | número | 2 | ≥ 0 | Los viajes más cortos que esto (medidos por el camino que siguen) no retraen, en mm; un viaje que sale de la pieza retrae siempre |

### `adhesion`

Falda y borde alrededor de la primera capa (fase F5), como tipo `Skirt/Brim` del G-code.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `skirt_loops` | entero | 1 | 0–1000 | Vueltas de falda; 0 no hace falda |
| `skirt_distance` | número | 6 | ≥ 0 | Distancia en mm entre la pieza (o su borde) y la primera vuelta de falda (ficha `supports_skirt.md`) |
| `skirt_layers` | entero | 1 | 0–1000 | En cuántas capas se repite la falda |
| `brim_width` | número | 0 | ≥ 0 | Ancho del borde en mm: tantas vueltas de un cordón como entren, pegadas a la pieza y unidas entre piezas cercanas; 0 no hace borde. Con balsa, ninguno |
| `raft_layers` | entero | 0 | 0 a 100 | Capas de la balsa (ficha `skirt_brim_raft.md`, reglas 9 a 14): soporte debajo de toda la pieza, que sube esas capas y `raft_gap`. La primera capa de la balsa tiene la altura de `layers.first_height`; las demás, tres cuartos de `machine.nozzle_diameter` (0,3 mm con boquilla de 0,4). De arriba hacia abajo: `support.interface_layers` de interfaz densa a 135° y 45° alternados (como mucho todas menos la primera; con una sola capa, esa es de interfaz), y la base a `support.spacing`, a `support.line_angle` la de más arriba y girada 90° en capas alternas. La pieza empieza con una capa de `layers.height`, sin la compensación de pata de elefante; la falda rodea la primera capa de la balsa. 0: sin balsa |
| `raft_gap` | número | 0,1 | 0 a 100 | Separación en Z entre la balsa y la pieza, en mm |
| `raft_expansion` | número | 1,5 | 0 a 100 | Cuánto se agranda la balsa alrededor de la primera capa de la pieza, en mm (el valor exacto, sin ajustarse a la grilla de las líneas) |
| `raft_first_layer_density` | número | 90 | 5 a 100 | Densidad de las líneas de la primera capa de la balsa, en % |
| `raft_first_layer_expansion` | número | 3 | 0 a 100 | Cuánto más se agranda la primera capa de la balsa, en mm |

### `support`

Soportes según `docs/spec/BEHAVIOR.md` («Soportes») y la ficha `supports_skirt.md`: base en líneas e interfaz densa
bajo la pieza, como tipos `Support material` y `Support material interface` del G-code. La zona del soporte en grilla
es la proyección de lo que cuelga más un margen de 1,6 mm alrededor (regla 2), apartada `xy_gap` de la pieza. Los
puentes cortos sin soporte vienen después.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `enabled` | booleano | false | | Activa los soportes |
| `automatic` | booleano | true | | false: solo hay soporte bajo las caras pintadas para tenerlo (`docs/PAINTING.md`; ficha `supports_more.md`, regla 3). Las caras pintadas para no tenerlo nunca lo llevan |
| `threshold_angle` | número | 0 | 0 a menos de 90 | Una cara más tendida que este ángulo, medido desde la horizontal, lleva soporte; 0 es automático: cada capa puede salir medio cordón más afuera que la de abajo |
| `z_gap` | número | 0.2 | ≥ 0 | Separación en Z entre el soporte y la pieza de arriba, en mm (se redondea a capas enteras; valor de banco) |
| `xy_gap` | número o texto | `"50%"` | ≥ 0 | Separación horizontal entre el soporte y la pieza: en mm, o un porcentaje del ancho de las paredes (`walls.width`) como `"50%"` (ficha `supports_skirt.md`, regla 2) |
| `width` | número o texto | 0 | 0 a 40 mm | Ancho de los cordones del soporte y de su interfaz: en mm, o en % de la altura de capa (`"150%"` da 0,3 mm con capas de 0,2); 0, el de las paredes (`walls.width`). Ficha `supports_more.md`, regla 18 |
| `spacing` | número | 2.5 | > 0 | Espacio libre entre las líneas de la base, en mm: entre los centros hay además la separación de un cordón (2,857 mm con cordones de 0,4 y capas de 0,2; ficha `supports_skirt.md`, regla 6). Líneas en zigzag sobre una grilla fija al origen; la interfaz va a 90° de ellas, a la separación de los cordones |
| `line_angle` | número | 0 | −360 a 360 | Dirección de las líneas de la base, en grados, la misma en todas las capas |
| `interface_layers` | entero | 3 | 0–1000 | Capas densas bajo la pieza (valor de banco): el soporte que tiene la pieza encima en las capas siguientes a la separación en Z va en líneas a la separación de los cordones, a 90° de la base (con `style` `organic`, a 45° y 135° en capas alternas) |
| `style` | texto | `"grid"` | `grid`, `organic` | `grid`: columnas rectas bajo lo que cuelga. `organic`: soportes en árbol (ficha `supports_more.md`, reglas 15 a 17): bajo lo que cuelga, `interface_layers` capas de interfaz enteras; debajo, puntas redondas que bajan como ramas, se juntan y engordan hasta troncos sobre la cama, y esquivan la pieza; donde no pueden esquivarla, se apoyan en ella, con la separación en Z. Cada rama lleva una pared (dos si es gruesa) y el relleno del soporte adentro |
| `tree_angle` | número | 40 | 0 a 85 | Inclinación máxima de una rama, en grados desde la vertical, para esquivar la pieza |
| `tree_angle_preferred` | número | 25 | 0 a 85 | Inclinación de una rama para juntarse con otra |
| `tree_branch_diameter` | número | 2 | más de 0, hasta 100 | Diámetro de las ramas más finas, en mm |
| `tree_branch_diameter_angle` | número | 5 | 0 a 85 | Cuánto engordan las ramas al bajar, en grados (0: el mismo grosor) |
| `tree_double_wall_diameter` | número | 3 | 0 a 100 | Una rama más gruesa que un círculo de este diámetro, en mm, lleva dos paredes; 0: nunca |
| `tree_branch_distance` | número | 1 | más de 0, hasta 100 | Cuánto se separan las puntas: 2 las separa el doble. Nunca quedan más juntas que su ancho ni que dos cordones |
| `tree_tip_diameter` | número | 0.8 | más de 0, hasta 100 | Diámetro de las puntas, en mm |
| `tree_top_rate` | número | 15 | más de 0, hasta 100 | Qué tan juntas van las puntas bajo la pieza, en % |

### `cooling`

Enfriamiento según `docs/spec/BEHAVIOR.md` («Enfriamiento») y la ficha `cooling.md`.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `min_layer_time` | número | 5 | ≥ 0 | Tiempo mínimo de capa, en s; una capa más rápida se imprime más lento (valor de banco) |
| `min_speed` | número | — (del perfil de la impresora; respaldo 10) | > 0 | Primera etapa: las extrusiones bajan todas en la misma proporción, pero no por debajo de esto (en mm/s; es del perfil de la impresora, como toda velocidad); si no alcanza, la segunda etapa las pone a todas a una misma velocidad, la más alta que llega al tiempo, nunca por debajo de 1 mm/min; las que ya iban más lento no cambian |
| `fan_min` | número | 35 | 0–100 | Ventilador de una capa que tarda casi `fan_below_time`, en %. Entre `min_layer_time` y `fan_below_time` el ventilador baja en línea recta de `fan_max` a `fan_min` según el tiempo de la capa |
| `fan_max` | número | 100 | 0–100 | Ventilador de una capa que tarda `min_layer_time` o menos, y de toda capa que tuvo que ir más lento, en % |
| `fan_below_time` | número | 60 | ≥ 0 | Una capa que tarda esto o más va sin ventilador (o con `fan_min`, si `fan_always`), en s |
| `fan_always` | booleano | false | | El ventilador nunca baja de `fan_min`, ni en las capas largas (sí en las primeras, `fan_off_layers`) |
| `fan_bridges` | número | 100 | 0–100 | Ventilador mientras se imprime un puente, en %; al terminar el puente vuelve el de la capa |
| `fan_off_layers` | entero | 3 | 0–1000 | Las primeras capas van sin ventilador, salvo las que tienen que ir más lento por el tiempo mínimo: esas lo llevan al máximo, como todas (decisión del dueño, 2026-10-24) |
| `keep_outer_speed` | booleano | true | | Para llegar al tiempo mínimo, primero se frena todo lo que no es pared externa (como la primera etapa, sin bajar de `min_speed`), y la pared externa sigue a su velocidad: el plástico sale de la boquilla a la misma temperatura y la pieza brilla parejo (`docs/GLOSS.md`, G1 y G2). Si eso no alcanza, se frena todo como en las dos etapas. `false`: las dos etapas de siempre, como Tisma Slicer |

### `placement`

Dónde pone la pieza la línea de comandos (ficha `docs/spec/features/placement.md`, reglas 2 y 4). La interfaz ubica
las piezas en la placa ella misma y no usa estos ajustes; tampoco están en su panel.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `arrange` | booleano | `true` | — | Con una cama (`machine.bed`), el centro de la caja de la pieza va a `center`; `false` la deja donde viene en X e Y. Sin cama no se mueve nada, salvo que se dé `center` a mano (PL15). En Z la pieza siempre se apoya en la cama |
| `center` | `[x, y]` o `null` | `null` | mm | Dónde queda el centro de la pieza; `null` es el centro de la caja de la cama |

### `compute`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `gpu` | texto | `"auto"` | `auto`, `off` | `auto` usa la GPU si hay una (Vulkan); `off` usa solo la CPU |

### `zaa`

Z anti-aliasing (`docs/ZAA.md`, ficha `docs/spec/features/z_antialiasing.md`): las extrusiones que se ven arriba
siguen la superficie dentro de su capa.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `enabled` | sí/no | `false` | | Activa el Z anti-aliasing: paredes, relleno de arriba y relleno de huecos bajan hasta la superficie donde esta corta la capa, con el material del espesor que queda |
| `min_thickness` | número | 25 | 1 a 100 | Hasta dónde puede bajar una extrusión, en % de la altura de la capa: nunca queda más fina que esto |
| `raise` | sí/no | `false` | | También sube hasta la superficie que queda por encima de la capa, hasta media capa (Song et al. 2017). La ficha solo baja |

### `compensation`

Compensación geométrica de la impresora (`docs/CALIBRATION.md`). La pestaña Calibración la calcula desde las medidas
de la pieza de prueba.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `scale_x`, `scale_y`, `scale_z` | número | 100 | 50 a 200 | Escala del modelo en %, por eje, alrededor del centro de su base (Z desde abajo): la pieza queda donde estaba en la cama. Las comprobaciones de la cama y la altura ven la pieza escalada |
| `hole_offset` | número | 0 | −5 a 5 | Cuánto crecen los agujeros, en mm sobre el radio (negativo: se achican). Solo los agujeros: los bordes de afuera no se mueven, y una isla dentro de un agujero se conserva |

### `extrusion`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `flow` | número | 100 | 10 a 300 | Flujo en %: la E de cada extrusión es ancho × alto × largo / sección del filamento × flujo |
| `pressure_advance` | número | 0 | 0 a 2 | Avance de presión del firmware, escrito una vez después del G-code de inicio con el comando del dialecto: `M900 K…` (Marlin, linear advance), `SET_PRESSURE_ADVANCE ADVANCE=…` (Klipper) o `M572 D0 S…` (RepRapFirmware). 0: no se escribe y queda lo que tenga la impresora. Lo da la calibración de avance de presión |
| `max_flow` | número | 0 | 0 a 1000 | Flujo volumétrico máximo del filamento, en mm³/s (como `machine.max_flow`; vale el menor de los dos). 0: sin límite propio |
| `infill_max_speed` | número | 0 | 0 a 1000 | Tope del relleno disperso con este filamento, en mm/s, para los patrones cuyas líneas no se cruzan en la capa (`rectilinear`, `gyroid`, `honeycomb` y los demás). 0: sin tope (`speeds_accels.md`, regla 5; la `material.infill_max_speed` de la ficha) |
| `infill_max_crossing_speed` | número | 0 | 0 a 1000 | Ídem para los patrones cuyas líneas se cruzan en la misma capa: `grid`, `triangles`, `stars`, `cubic`, `adaptive_cubic`, `support_cubic` |
| `bridge_flow_ratio` | número | 1 | 0,1 a 2 | Multiplica la sección del cordón redondo de los puentes y de los perímetros sobre el aire. El cordón es redondo, de diámetro `machine.nozzle_diameter` × √`bridge_flow_ratio` (con 0,4 de boquilla, 0,052245 mm de filamento por mm), y las líneas del puente van a esa distancia: con menos flujo, más juntas (`speeds_accels.md`, regla 14; `gcode_motion.md`, regla 1) |
| `chamber_temperature` | número | 0 | 0 a 200 | Temperatura de cámara que pide el filamento, en °C. Si la impresora calienta la cámara (`machine.max_chamber_temp`), se escribe antes del G-code de inicio, para que caliente junto con la boquilla, y otra vez después, sin pasar de su máximo: `M141 S…`, o `SET_HEATER_TEMPERATURE HEATER=chamber TARGET=…` en Klipper; al final se apaga. Si no la calienta o no llega, hay un aviso. 0: ninguna |
| `chamber_min_temperature` | número | 0 | 0 a 200 | Con cámara: después del G-code de inicio, espera a que la cámara llegue a esto antes de volver a pedir `chamber_temperature` (`M191 S…`, o `TEMPERATURE_WAIT SENSOR="heater_generic chamber" MINIMUM=…` en Klipper; ficha `gcode_extras.md`, regla 3). 0: no espera |
| `density` | número | 0 | 0 a 30 | Densidad del filamento, en g/cm³, para el peso al final del G-code (`; filament used [g]`, regla 1). 0: no se sabe y no se escribe el peso |
| `cost` | número | 0 | ≥ 0 | Costo del filamento por kg, para `; filament cost` |
| `spool_weight` | número | 0 | ≥ 0 | Peso del carrete vacío, en g (para que la interfaz compare con el carrete pesado) |
| `abrasive` | booleano | false | | El filamento gasta una boquilla de latón (con fibra de carbono o de vidrio). Sin `machine.hardened_nozzle`, hay un aviso |
| `needs_enclosure` | booleano | false | | El filamento pide impresora cerrada. Sin `machine.enclosed`, hay un aviso |

Los avisos de capacidades (`nozzle_temperature`, `bed_temperature`, `chamber`, `enclosure`, `abrasive`) van con los
demás avisos del G-code (`src/report/warnings.hpp`; en la interfaz, en la barra de estado); no frenan la exportación.

### `gcode`

De la ficha `docs/spec/features/gcode_extras.md`. Va en el perfil de impresora (`docs/PROFILES.md`).

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `resolution` | número | 0.0125 | 0 a 1 | Resolución del G-code, en mm: un punto de una trayectoria no se escribe cuando el movimiento que lo reemplaza pasa a menos de esto, y el ancho, la Z y el cordón de abajo no cambian más de 0,0005 mm (`src/gcode/resolution.hpp`). La costura y los extremos quedan. 0: se escriben todos los puntos. Un cilindro de 128 lados (prueba C17 de `slicing.md`) pasa de 41 859 a 6 140 extrusiones, 64 por vuelta, y ningún tramo de menos de 0,01 mm |
| `volumetric_e` | booleano | `false` | | Toda E (extrusión y retracción) en mm³ de plástico en vez de mm de filamento: la E de siempre por la sección del filamento (ficha `gcode_extras.md`, regla 11). El motor no escribe `M200`: va en el G-code de inicio |
| `color_change` | texto | `"M600"` | | Lo que se escribe en un cambio de color de `changes` (ficha `gcode_templates.md`, regla 13), con las variables de `machine.variables`, después de la marca `;COLOR_CHANGE` |
| `pause` | texto | `"M601"` | | Lo mismo para una pausa, después de `;PAUSE_PRINT` |
| `before_layer` | texto | `""` | | G-code justo antes del movimiento en Z de cada cambio de capa, desde la segunda capa (regla 11, prueba T8), con las variables y además `layer_num` (desde 0), `layer_z` y `max_layer_z` (la Z escrita, sin ceros de más). Va como G-code propio (`;TYPE:Custom`); si tiene algo más que comentarios, el viaje siguiente vuelve a escribir todos los ejes |
| `after_layer` | texto | `""` | | Lo mismo, justo después del movimiento en Z |

### `changes` (lista)

Cambios desde una altura, para las torres de calibración. A diferencia de los grupos, es una lista de objetos:

```json
"changes": [{"z": 0, "temperature": 230}, {"z": 8, "temperature": 225}, {"z": 16, "temperature": 220}]
```

- **Desde cuándo vale:** cada valor rige desde la primera capa cuya parte de arriba queda por encima de `z`, hasta
  que otro cambio lo vuelva a poner.
- **Orden:** los cambios se ordenan por `z`.
- **Lo que falta:** una clave ausente deja ese valor como venía.

| Clave | Tipo | Rango | Significado |
|---|---|---|---|
| `z` | número (obligatorio) | ≥ 0 | Altura en mm |
| `temperature` | número | 0 a 500 | Temperatura de la boquilla: escribe `M104 S…` al empezar la capa |
| `bed_temperature` | número | 0 a 200 | Temperatura de la cama: `M140 S…` |
| `fan` | número | 0 a 100 | Ventilador en %, en lugar de `cooling.fan_min` y `fan_max` |
| `retraction_length` | número | 0 a 50 | Largo de la retracción en mm |
| `retraction_speed` | número | 1 a 500 | Velocidad de la retracción en mm/s |
| `flow` | número | 10 a 300 | Flujo en %, en lugar de `extrusion.flow` |
| `speed` | número | 5 a 1000 | Porcentaje de la velocidad de cada extrusión (puede pasar `speed.max`: es para la torre de velocidad) |
| `pressure_advance` | número | 0 a 2 | Avance de presión, con el comando del dialecto (como `extrusion.pressure_advance`; acá 0 sí se escribe) |
| `volumetric_flow` | número | 0,5 a 1000 | Flujo volumétrico forzado, en mm³/s: desde esa altura, toda extrusión corre a la velocidad que da ese flujo en su cordón, por encima de `speed.max` y de `machine.max_flow` y sin el frenado del tiempo mínimo de capa, hasta `machine.max_feed_x` o `_y`. Es para la torre de tope de flujo (`docs/CALIBRATION.md`); el verificador admite esas velocidades |
| `acceleration` | número | 100 a 100 000 | Aceleración, con el comando del dialecto (`M204 P… T…` con `machine.travel_acceleration`, o `SET_VELOCITY_LIMIT ACCEL=…`). El tiempo estimado sigue con `machine.acceleration` |
| `gcode` | texto | | G-code que se escribe al empezar la capa, con las variables de `machine.variables` |
| `color_change` | booleano | | Cambio de color a esa altura: `gcode.color_change` (`M600`) al empezar la capa (ficha `gcode_templates.md`, regla 13) |
| `pause` | booleano | | Pausa a esa altura: `gcode.pause` (`M601`) |

El verificador del G-code juzga cada capa con la retracción y el flujo que le tocan (`docs/GCODE_CHECK.md`).

## Grupos previstos (todavía no implementados)

`material`. Se define acá
cuando se implementan, a partir de las fichas de `docs/spec/features/`.

## Ejemplo

```json
{
  "layers": { "height": 0.3, "first_height": 0.35 },
  "slicing": { "xy_offset": -0.05, "first_layer_shrink": 0.1 },
  "walls": { "count": 2 },
  "compute": { "gpu": "auto" }
}
```
