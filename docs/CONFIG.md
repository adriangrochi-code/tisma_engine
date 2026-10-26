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
| `height` | número | 0.2 | 0,01 a 2 | Alto de las capas, salvo la primera. Al laminar, nunca más que la boquilla ni `machine.max_layer_height`, ni menos que `machine.min_layer_height` (`check_settings`) |
| `first_height` | número | 0.2 | 0,01 a 2 | Alto de la primera capa, con los mismos topes que `height` |

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
| `width` | número | 0.4 | 0,05 a 10 | Ancho preferido de los cordones (w*); por ahora fijo, más adelante saldrá de la boquilla del grupo `machine`. Al laminar, de un cuarto a cuatro veces la boquilla; fuera de 0,6 a 2,5 veces, un aviso |
| `seam` | texto | `"aligned"` | `aligned`, `nearest`, `random`, `rear` | Dónde empieza cada lazo: `aligned`, en una esquina cóncava si hay, cerca de la costura de la capa de abajo; `nearest`, el punto más cercano al final del movimiento anterior; `random`, al azar pero igual en cada corrida; `rear`, el punto de Y máxima |

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
| `max_feed_x`, `max_feed_y` | número | 200 | > 0 | Velocidad máxima de cada eje, en mm/s, para estimar el tiempo |
| `max_feed_z` | número | 12 | > 0 | Ídem para Z |
| `max_feed_e` | número | 120 | > 0 | Ídem para el extrusor |
| `acceleration` | número | 1000 | > 0 | Aceleración de los movimientos que extruyen, en mm/s². Se escribe después del G-code de inicio, con `travel_acceleration`, para que la impresora use lo que supone el tiempo estimado: `M204 P… T…` (Marlin y RepRapFirmware) o `SET_VELOCITY_LIMIT ACCEL=…` (Klipper, un solo valor) |
| `travel_acceleration` | número | 1250 | > 0 | Aceleración de los viajes y las retracciones, en mm/s² |
| `corner_speed` | número | 10 | ≥ 0 | Velocidad a la que se puede pasar por una esquina de 90° o más, en mm/s; en giros más suaves, la menor de las dos velocidades por el coseno del giro, nunca menos que esto |
| `bed` | lista de puntos | `[]` | vacía, o 3 o más `[x, y]` | Polígono de la cama en mm (por ejemplo `[[0, 0], [250, 0], [250, 210], [0, 210]]`, la cama del banco). Si se da, un modelo con algún vértice fuera de ella (el borde cuenta como dentro) no se lamina: `slice` termina con código 4. Vacío: no se comprueba, porque el motor no mueve los modelos en X e Y y los del banco están centrados en el origen. En Z la pieza siempre se imprime apoyada en la cama: las alturas del G-code (y las de `changes`) cuentan desde su punto más bajo, esté donde esté la malla |
| `max_height` | número | 0 | ≥ 0 | Altura máxima de impresión en mm; un modelo más alto termina con código 4. 0: no se comprueba |
| `min_layer_height`, `max_layer_height` | número | 0 | 0, o 0,01 a 10 | Las alturas de capa que imprime la máquina, en mm, para la altura variable (`docs/VARIABLE_LAYERS.md`). Nunca más que `nozzle_diameter`. 0: 0,04 mm y el diámetro de la boquilla |
| `max_flow` | número | 0 | 0 a 1000 | Flujo volumétrico máximo del hot end, en mm³/s. Ninguna extrusión lo pasa: su velocidad baja a flujo / (ancho × alto × `extrusion.flow`), redondeada hacia abajo a mm/min enteros. Con `extrusion.max_flow` vale el menor de los dos. 0: sin límite |
| `max_nozzle_temp`, `max_bed_temp` | número | 0 | 0 a 600 | Temperaturas máximas de la boquilla y la cama, en °C. Si las variables `temperature` o `bed_temperature`, o los cambios por altura, piden más, hay un aviso. 0: no se sabe |
| `max_chamber_temp` | número | 0 | 0 a 600 | Temperatura máxima de la cámara calefaccionada, en °C. 0: la impresora no calienta la cámara |
| `enclosed` | booleano | false | | La impresora es cerrada |
| `hardened_nozzle` | booleano | false | | La boquilla acepta filamentos abrasivos (acero endurecido o similar) |

Estas claves las llena el catálogo (`docs/CATALOG.md`, «Capacidades») con los datos del fabricante.

### `speed`

Velocidades en mm/s. Ninguna extrusión pasa de `max`; en la primera capa, ninguna pasa de `first_layer`.

| Clave | Tipo | Por defecto | Significado |
|---|---|---|---|
| `walls` | número | 60 | Paredes internas |
| `outer_wall` | número o texto | `"50%"` | Pared externa: mm/s, o un porcentaje de `walls` como `"50%"` |
| `infill` | número | 80 | Relleno disperso |
| `solid_infill` | número | 80 | Superficies inferiores, puentes y relleno de huecos |
| `top_solid_infill` | número | 50 | Superficies superiores |
| `support` | número | 80 | > 0 | Soporte e interfaz, en mm/s |
| `travel` | número | 130 | Viajes |
| `first_layer` | número | 30 | Tope de la primera capa |
| `max` | número | 80 | Tope de toda extrusión |

### `retraction`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `length` | número | 2 | 0 a 50 | Cuánto filamento se retrae, en mm; 0 no retrae |
| `speed` | número | 40 | > 0 | Velocidad de la retracción y de su vuelta, en mm/s |
| `lift` | número | 0 | 0 a 10 | Cuánto sube Z durante un viaje que retrae, en mm |
| `min_travel` | número | 2 | ≥ 0 | Los viajes más cortos que esto (medidos por el camino que siguen) no retraen, en mm; un viaje que sale de la pieza retrae siempre |

### `adhesion`

Falda y borde alrededor de la primera capa (fase F5), como tipo `Skirt/Brim` del G-code.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `skirt_loops` | entero | 1 | 0–1000 | Vueltas de falda; 0 no hace falda |
| `skirt_distance` | número | 3 | ≥ 0 | Distancia en mm entre la pieza (o su borde) y la primera vuelta de falda |
| `skirt_layers` | entero | 1 | 0–1000 | En cuántas capas se repite la falda |
| `brim_width` | número | 0 | ≥ 0 | Ancho del borde en mm: tantas vueltas de un cordón como entren, pegadas a la pieza y unidas entre piezas cercanas; 0 no hace borde |

### `support`

Soportes según `docs/spec/BEHAVIOR.md` («Soportes»): base en líneas e interfaz densa bajo la pieza, como tipos
`Support material` y `Support material interface` del G-code. Los puentes cortos sin soporte vienen después.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `enabled` | booleano | false | | Activa los soportes |
| `threshold_angle` | número | 0 | 0 a menos de 90 | Una cara más tendida que este ángulo, medido desde la horizontal, lleva soporte; 0 es automático: cada capa puede salir medio cordón más afuera que la de abajo |
| `z_gap` | número | 0.2 | ≥ 0 | Separación en Z entre el soporte y la pieza de arriba, en mm (se redondea a capas enteras; valor de banco) |
| `xy_gap` | número | 0.6 | ≥ 0 | Separación horizontal entre el soporte y la pieza, en mm |
| `spacing` | número | 2.5 | > 0 | Separación de las líneas del soporte, en mm (valor de banco); líneas en zigzag sobre una grilla fija al origen, a 0° y 90° en capas alternas |
| `interface_layers` | entero | 3 | 0–1000 | Capas densas bajo la pieza (valor de banco): el soporte que tiene la pieza encima en las capas siguientes a la separación en Z va con las líneas a un cordón, a 45° de la base |

### `cooling`

Enfriamiento según `docs/spec/BEHAVIOR.md` («Enfriamiento»).

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `min_layer_time` | número | 5 | ≥ 0 | Tiempo mínimo de capa, en s; una capa más rápida se imprime más lento (valor de banco) |
| `min_speed` | número | 10 | > 0 | Primera etapa: las extrusiones bajan todas en la misma proporción, pero no por debajo de esto (en mm/s, valor de banco); si no alcanza, la segunda etapa las pone a todas a una misma velocidad, la más alta que llega al tiempo, nunca por debajo de 1 mm/min; las que ya iban más lento no cambian |
| `fan_min` | número | 35 | 0–100 | Ventilador en las capas normales, en % |
| `fan_max` | número | 100 | 0–100 | Ventilador en las capas que tuvieron que ir más lento, en % |
| `fan_off_layers` | entero | 1 | 0–1000 | Las primeras capas van sin ventilador, salvo las que tienen que ir más lento por el tiempo mínimo: esas lo llevan al máximo, como todas (decisión del dueño, 2026-10-24) |
| `keep_outer_speed` | booleano | true | | Para llegar al tiempo mínimo, primero se frena todo lo que no es pared externa (como la primera etapa, sin bajar de `min_speed`), y la pared externa sigue a su velocidad: el plástico sale de la boquilla a la misma temperatura y la pieza brilla parejo (`docs/GLOSS.md`, G1 y G2). Si eso no alcanza, se frena todo como en las dos etapas. `false`: las dos etapas de siempre, como Tisma Slicer |

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
| `chamber_temperature` | número | 0 | 0 a 200 | Temperatura de cámara que pide el filamento, en °C. Si la impresora calienta la cámara (`machine.max_chamber_temp`), se escribe después del G-code de inicio, sin pasar de su máximo: `M141 S…`, o `SET_HEATER_TEMPERATURE HEATER=chamber TARGET=…` en Klipper; al final se apaga. Si no la calienta o no llega, hay un aviso. 0: ninguna |
| `abrasive` | booleano | false | | El filamento gasta una boquilla de latón (con fibra de carbono o de vidrio). Sin `machine.hardened_nozzle`, hay un aviso |
| `needs_enclosure` | booleano | false | | El filamento pide impresora cerrada. Sin `machine.enclosed`, hay un aviso |

Los avisos de capacidades (`nozzle_temperature`, `bed_temperature`, `chamber`, `enclosure`, `abrasive`) van con los
demás avisos del G-code (`src/report/warnings.hpp`; en la interfaz, en la barra de estado); no frenan la exportación.

### `gcode`

De la ficha `docs/spec/features/gcode_extras.md`. Va en el perfil de impresora (`docs/PROFILES.md`).

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `resolution` | número | 0.0125 | 0 a 1 | Resolución del G-code, en mm: un punto de una trayectoria no se escribe cuando el movimiento que lo reemplaza pasa a menos de esto, y el ancho, la Z y el cordón de abajo no cambian más de 0,0005 mm (`src/gcode/resolution.hpp`). La costura y los extremos quedan. 0: se escriben todos los puntos. Un cilindro de 128 lados (prueba C17 de `slicing.md`) pasa de 41 859 a 6 140 extrusiones, 64 por vuelta, y ningún tramo de menos de 0,01 mm |

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
