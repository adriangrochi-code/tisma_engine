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
| `clockwise` | booleano | `false` | | Los lazos de las paredes (también los de los agujeros y la espiral del modo vaso) van en sentido horario; apagado, todos en antihorario (ficha `retraction_travel.md`, regla 9, prueba R8). Cada lazo empieza en la misma costura |
| `staggered_inner_seams` | booleano | `false` | | Cada pared interna empieza antes que la de afuera sobre su propio lazo, `width` por nivel de profundidad: las costuras forman un zigzag hacia adentro en vez de una línea (ficha `walls_extra.md`, regla 3, prueba W5). La de afuera queda donde dice `seam` |
| `top_one_wall` | texto | `"none"` | `none`, `top`, `topmost` | Un solo perímetro sobre las superficies de arriba (regla 10, prueba W7): donde la capa de arriba deja la región abierta (más ancha que un cordón) queda solo la pared de afuera, las internas van alrededor de lo que sigue subiendo y la superficie de arriba gana ese lugar. `top`: en cada capa; `topmost`: solo en la última capa de la pieza |
| `first_layer_one_wall` | booleano | `false` | | La primera capa lleva solo la pared de afuera (regla 11, prueba W8) |
| `extra_if_needed` | booleano | `true` | | Perímetros extra en las paredes inclinadas (regla 6). Se lee y se guarda: Tisma Slicer no agregó ninguno en las piezas de la ficha, y el motor tampoco |
| `extra_on_overhangs` | booleano | `false` | | Un voladizo sostenido de un solo lado (donde un puente no puede anclarse de los dos) se llena, dentro de las paredes, con lazos desde su contorno hacia adentro, `;TYPE:Overhang perimeter`, a la velocidad de los puentes y con su cordón redondo, en vez de su relleno (regla 5, prueba W3). Con `arcs.enabled`, esas zonas son de los arcos |
| `avoid_curled_overhangs` | booleano | `false` | | Viajes que esquivan los voladizos que se pueden levantar (regla 9). Se lee y se guarda; no cambia el G-code: la ficha no lo midió |
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
| `interface_shells` | booleano | `false` | | Con varios extrusores, donde la parte de uno apoya sobre la de otro, cada una lleva sus capas sólidas de arriba y de abajo como si fueran piezas separadas; sin esto, lo de los otros materiales tapa como si fuera la misma pieza (ficha `multi_extruder.md`, regla 16, ME10) |
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
| `relative_extrusion` | booleano | false | | `true`: E relativa (`M83`), cada movimiento lleva su E (en Marlin, con `G92 E0` después del salto en Z de cada capa, para que el firmware no pierda precisión; ficha `gcode_firmware.md`, regla 3); `false`: absoluta (`M82`), con `G92 E0` en cada capa |
| `flavor` | texto | `"reprap"` | `marlin`, `marlin_legacy`, `klipper`, `reprap`, `reprapfirmware`, `repetier`, `smoothie`, `teacup`, `makerware`, `sailfish`, `mach3`, `machinekit`, `no_extrusion` | Dialecto de G-code. `marlin` es Marlin 2 y `marlin_legacy` Marlin 1 (su `M204 S` es la aceleración al imprimir); `reprapfirmware` es el de Duet (temperatura con `G10 S… P0`, espera con `M116`), y `reprap` el RepRap genérico. Con `variables.temperature` y un G-code de inicio sin comandos de temperatura, el motor fija la de la boquilla antes del inicio (`M104 S…`) y la espera después (`M109 S…`), cada dialecto con los suyos (ficha `gcode_firmware.md`, regla 1). Los antiguos (ficha `gcode_extras.md`, regla 6; `src/gcode/dialect.hpp`): `repetier` y `smoothie` escriben como `reprap`; `teacup` apaga el ventilador con `M106 S0`; `makerware` y `sailfish` ponen la E siempre absoluta, sin `M82` ni `G92`, el ventilador con `M126` y `M127`, `M104 S… T0` y `M73 P…` (avance) en cada capa, y `makerware` además `M101` antes de extruir y `M103` después; `mach3` y `machinekit` llaman `A` al extrusor y escriben temperaturas y ventilador con `P` (`M104 P215`, `M106 P252.45`), `machinekit` vuelve `A` a 0 en cada capa y termina con `M2`; `no_extrusion`, los mismos movimientos sin E. Con los antiguos, salvo `repetier` y `smoothie`, no se escriben aceleraciones (`M204`) ni avance de presión. La vista previa del panel lee la `A` de `mach3` y `machinekit`; con `no_extrusion` solo ve viajes |
| `start_gcode`, `end_gcode` | texto | `""` | | G-code de inicio y de fin, en el lenguaje de plantillas (ficha `gcode_templates.md`; ver «Plantillas de G-code» abajo). El orden al empezar: las temperaturas automáticas (`gcode.auto_temperatures`), este inicio, la espera de la boquilla y el inicio del filamento (`extrusion.start_gcode`); al terminar, el fin del filamento y después este |
| `variables` | objeto | `{}` | nombre: valor o lista | Variables de las plantillas de G-code (ver «Plantillas de G-code»), una lista por extrusor: `[nombre]`, `{nombre}`, `{nombre[0]}`. Las da quien llama al motor (el adaptador de Tisma pone ahí los valores de sus perfiles), así los nombres de los perfiles no entran al motor. Los números se escriben como en el JSON. Un nombre desconocido es un error: no se escribe el G-code. Los nombres no llevan saltos de línea ni corchetes; en el comentario del final del G-code, los saltos de línea de los valores se escriben como `\n` |
| `max_feed_x`, `max_feed_y` | número | — (del perfil de la impresora) | > 0, o `null` | Velocidad máxima de cada eje, en mm/s, para estimar el tiempo. Sin valor (`null`): no se sabe y no se aplica |
| `max_feed_z` | número | — (del perfil de la impresora) | > 0, o `null` | Ídem para Z |
| `max_feed_e` | número | — (del perfil de la impresora) | > 0, o `null` | Ídem para el extrusor |
| `acceleration` | número | 1000 | > 0 | Aceleración de los movimientos que extruyen, en mm/s². Se escribe después del G-code de inicio, con `travel_acceleration`, para que la impresora use lo que supone el tiempo estimado: `M204 P… T…` (Marlin y RepRapFirmware) o `SET_VELOCITY_LIMIT ACCEL=…` (Klipper, un solo valor) |
| `travel_acceleration` | número | 1250 | > 0 | Aceleración de los viajes y las retracciones, en mm/s² |
| `corner_speed` | número | — (del perfil de la impresora; respaldo 10) | ≥ 0 | Velocidad a la que se puede pasar por una esquina de 90° o más, en mm/s; en giros más suaves, la menor de las dos velocidades por el coseno del giro, nunca menos que esto |
| `max_accel_x`, `max_accel_y`, `max_accel_z`, `max_accel_e` | número | 0 | ≥ 0 | Aceleración máxima de cada eje, en mm/s² (`M201`; ficha `gcode_firmware.md`, regla 2). En la estimación, la aceleración de un movimiento no pasa la de ningún eje que mueve (escalada por cuánto lo mueve). 0: no se sabe |
| `retract_acceleration` | número | 0 | ≥ 0 | Aceleración de las retracciones (`M204 R`), en mm/s². 0: la de los viajes |
| `jerk_x`, `jerk_y`, `jerk_z`, `jerk_e` | número | 0 | ≥ 0 | Tirón de cada eje (`M205 X Y Z E`), en mm/s: cuánto cambia de golpe su velocidad en una esquina. En la estimación, la velocidad en la unión de dos movimientos es la mayor con la que ningún eje cambia más que su tirón. 0: no se sabe |
| `junction_deviation` | número | 0 | ≥ 0 | Desviación de unión de Marlin (`M205 J`), en mm; reemplaza al tirón. En la estimación, la velocidad en una esquina es la del círculo que la redondea a esa distancia del vértice, a la menor de las dos aceleraciones: radio d·sen(a/2)/(1 − sen(a/2)), con a el ángulo entre los movimientos, y velocidad √(aceleración · radio). 0: no |
| `min_extrude_feed`, `min_travel_feed` | número | 0 | ≥ 0 | Avance mínimo al extruir y al viajar (`M205 S T`), en mm/s: ningún movimiento va más lento |
| `limits_usage` | texto | `"time_only"` | `time_only`, `emit`, `ignore` | Para qué son los límites (ficha `gcode_firmware.md`, regla 2): `time_only`, solo para estimar el tiempo; `emit`, además se escriben antes del G-code de inicio (Marlin 2: `M201`, `M203`, `M204 P R T`, `M205 X Y Z E` o `M205 J`, `M205 S T`; Marlin 1: con `M204 S T`; RepRapFirmware: `M201`, `M203` y `M566` en mm/min, `M204 P T`; Klipper: error, los toma de su configuración; los demás: nada) y ninguna aceleración por tipo los pasa (`acceleration`, `travel_acceleration`); `ignore`, la estimación no usa estos últimos límites (aceleraciones por eje, tirones, desviación, avances mínimos, aceleración de retracción) |
| `silent_mode` | booleano | `false` | | La impresora tiene modo silencioso (ficha `gcode_extras.md`, regla 7): el G-code lleva una segunda estimación, `; estimated printing time (silent mode)` y la de la primera capa, con los límites de `silent`, y `M73 Q S` con `gcode.progress` |
| `silent` | objeto | `{}` | | Los límites del modo silencioso, por nombre: cualquiera de `max_feed_x` … `max_feed_e`, `max_accel_x` … `max_accel_e`, `acceleration`, `travel_acceleration`, `retract_acceleration`, `jerk_x` … `jerk_e`, `junction_deviation`, `min_extrude_feed`, `min_travel_feed` y `corner_speed`, ≥ 0. El que falta es el normal. Solo cambian la estimación; los que se escriben son los normales |
| `bed` | lista de puntos | `[]` | vacía, o 3 o más `[x, y]` | Polígono de la cama en mm (por ejemplo `[[0, 0], [250, 0], [250, 210], [0, 210]]`, la cama del banco). Si se da, un modelo con algún vértice fuera de ella (el borde cuenta como dentro) no se lamina: `slice` termina con código 4. Vacío: no se comprueba, porque el motor no mueve los modelos en X e Y y los del banco están centrados en el origen. En Z la pieza siempre se imprime apoyada en la cama: las alturas del G-code (y las de `changes`) cuentan desde su punto más bajo, esté donde esté la malla |
| `max_height` | número | 0 | ≥ 0 | Altura máxima de impresión en mm; un modelo más alto termina con código 4. 0: no se comprueba |
| `extruders` | entero | 1 | 1 a 16 | Cantidad de extrusores (ficha `multi_extruder.md`). Cada objeto de la placa se imprime con el suyo (en la interfaz, en el objeto; por omisión el 1) y cada tipo de extrusión con el de `extruders`. Con más de uno: cada extrusor que se usa se calienta al empezar (`M104 S… T<n>` con `variables.temperature[n]`, esperado con `M109 S… T<n>` después del inicio), se corre `gcode.toolchange` con `previous_extruder` −1 y se elige el primero (`T<n>`); cada capa empieza con el extrusor con el que terminó la anterior y sigue con los demás en orden, un cambio por extrusor (reglas 3 y 6, ME1). La cama de la primera capa es la `first_layer_bed_temperature` (o `bed_temperature`) del primer extrusor que imprime, y desde la segunda capa la `bed_temperature` del primero que imprime en ella, con `M140` (regla 4, ME3). Los objetos de cada extrusor se laminan aparte sobre las capas de la placa: cada uno lleva sus perímetros donde toca a otro; lo de un extrusor anterior queda de ese. Sin `layers.detail_height` |
| `single_extruder_mmu` | booleano | `false` | | Un solo extrusor que cambia de filamento (MMU): cada cambio purga en la torre el volumen de `wipe_tower.purge_volumes` (ficha `multi_extruder.md`, regla 12); sin esto, con varias boquillas, la torre solo ceba |
| `extruder_offset` | lista | `[]` | `[x, y]` de −1000 a 1000 por extrusor | Desfase de cada boquilla respecto de la primera, en mm: se resta de las X e Y que escribe esa herramienta (regla 8, ME5); al cambiar, la boquilla nueva se mueve primero sobre el mismo punto. Con `single_extruder_mmu` (una sola boquilla) no se usa |
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
| `unretract_speed` | número | 0 | 0 a 1000 | Velocidad de la vuelta del filamento, en mm/s; 0: la de `speed` (ficha `retraction_travel.md`, regla 1, prueba R1) |
| `restart_extra` | número | 0 | 0 a 10 | Filamento de más que empuja la vuelta, en mm: la vuelta es la retracción más esto (regla 1) |
| `on_layer_change` | booleano | `false` | | Todo cambio de capa retrae antes de subir, aunque el viaje al primer punto de la capa sea más corto que `min_travel`; la vuelta, al empezar a extruir (regla 2, prueba R2) |
| `wipe` | booleano | `false` | | En vez de retraer quieta, la boquilla vuelve sobre el camino recién impreso mientras retrae, al 80 % de `speed.travel`, a lo largo de lo que queda por retraer × esa velocidad / (0,95 × `speed`); después siguen el viaje, el salto y la vuelta de siempre. Si el camino desde el último viaje es más corto, lo que no alcanzó se retrae quieto al final (regla 3, prueba R3). No va con `firmware` |
| `before_wipe` | número | 0 | 0 a 100 | Parte de la retracción, en %, que se hace quieta antes del barrido (regla 3) |
| `lift_above`, `lift_below` | número | 0 | ≥ 0 | El salto en Z (`lift`, o la rampa de `travel`) solo en las capas cuya Z está por encima de `lift_above` y por debajo de `lift_below`, en mm; 0: sin límite (regla 4, prueba R4) |
| `firmware` | booleano | `false` | | Cada retracción es un `G10` y cada vuelta un `G11`, sin movimientos de E: el largo y la velocidad los pone el firmware (`M207`, `M208`); `length` sigue diciendo si se retrae (0: no) y el salto en Z sigue en el G-code. `restart_extra` no se escribe (regla 7, prueba R6) |
| `toolchange_length` | número o lista | 10 | 0 a 200 | Con varios extrusores, cuánto retrae, quieta, la herramienta que se deja antes de un cambio, en mm (lo que ya retrajo cuenta); un valor para todas o una lista por extrusor (una lista corta repite el último). Al volver a usar esa herramienta, su filamento vuelve eso más `toolchange_restart_extra` (ficha `multi_extruder.md`, regla 5, ME2) |
| `toolchange_restart_extra` | número o lista | 0 | 0 a 10 | Filamento de más al volver a usar una herramienta después de un cambio, en mm, por extrusor como `toolchange_length` |

### `travel`

De la ficha `docs/spec/features/retraction_travel.md`. `ramping_lift`, `slope`, `max_lift` y `lift_before_obstacle` van
en el perfil de impresora, como el salto; `avoid_crossing_walls` y `max_detour`, en el de impresión.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `ramping_lift` | booleano | `false` | | El salto en Z de un viaje que retrae se hace en rampa, subiendo mientras viaja con la pendiente `slope` hasta `max_lift`; el resto del viaje va a esa altura y al final baja en vertical. Un viaje corto sube hasta donde llega. El primer viaje de una capa sube desde la capa de abajo: el movimiento en Z del cambio de capa espera a la rampa, salvo que haya G-code de `gcode.after_layer` (regla 5, prueba R5). Con `slope` 0, salto vertical de `retraction.lift` |
| `slope` | número | 0 | 0 a 89 | Pendiente de la rampa, en grados |
| `max_lift` | número | 0 | 0 a 10 | Altura de la rampa, en mm; 0: `retraction.lift` |
| `lift_before_obstacle` | booleano | `false` | | Rampa más empinada si hay un obstáculo cerca del comienzo del viaje (regla 6). Se lee y se guarda, pero no cambia el G-code: la ficha no lo midió |
| `avoid_crossing_walls` | booleano | `true` | | Un viaje dentro de una isla sigue un camino por dentro de la pieza, a un cordón de las paredes, que no las cruza (rodea un agujero en vez de cruzarlo; `docs/spec/BEHAVIOR.md`, «Viajes y retracción»). Apagado, todo viaje va recto, y retrae si cruza paredes o sale de la pieza (regla 8, prueba R7). Tisma Slicer lo trae apagado; en el motor viene prendido porque es el comportamiento acordado con el dueño |
| `max_detour` | número o texto | 0 | ≥ 0, o `"N%"` | Si el camino por dentro es más largo que el recto más esto (en mm, o en % del viaje recto, como `"200%"`), el viaje va recto, cruzando las paredes, y retrae; 0: sin límite (regla 8) |

### `adhesion`

Falda y borde alrededor de la primera capa (fase F5), como tipo `Skirt/Brim` del G-code.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `skirt_loops` | entero | 1 | 0–1000 | Vueltas de falda; 0 no hace falda |
| `skirt_distance` | número | 6 | ≥ 0 | Distancia en mm entre la pieza (o su borde) y la primera vuelta de falda (ficha `supports_skirt.md`) |
| `skirt_layers` | entero | 1 | 0–1000 | En cuántas capas se repite la falda |
| `brim_width` | número | 0 | ≥ 0 | Ancho del borde en mm: tantas vueltas de un cordón como entren, pegadas a la pieza y unidas entre piezas cercanas; 0 no hace borde. Con balsa, ninguno |
| `brim_type` | texto | `"outer"` | `outer`, `inner`, `outer_and_inner`, `none` | Dónde va el borde (ficha `skirt_brim_raft.md`, reglas 5 a 7): alrededor del contorno de afuera, dentro de los agujeros de la primera capa desde su borde hacia adentro (rodeando las islas que haya en ellos, hasta llenar el agujero), o los dos |
| `brim_gap` | número | 0 | 0 a 100 | Separación en mm entre la pieza y la primera vuelta del borde (regla 8); el borde sigue teniendo `brim_width` de ancho |
| `skirt_min_filament` | número | 0 | 0 a 100 000 | Largo mínimo de filamento de la falda en la primera capa, en mm (regla 1): si `skirt_loops` gasta menos, se agregan vueltas hacia afuera hasta pasarlo (a lo sumo 1000). Cada vuelta gasta su largo por el ancho del cordón y la altura de la primera capa, sobre la sección del filamento. 0: no |
| `draft_shield` | texto | `"disabled"` | `disabled`, `limited`, `enabled` | Escudo contra corrientes (regla 4): con `limited` o `enabled` la falda se mide desde la pieza aunque haya borde, y la vuelta del borde que caería debajo de ella se quita; `enabled` la repite en todas las capas, hasta la última; `limited`, en las `skirt_layers` primeras |
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
| `spacing` | número | 6 | 0 a 1000 | Con `slice --copies N`, las copias van en una grilla compacta con esta separación entre sus cajas, y el conjunto centrado como una pieza sola (ficha `placement.md`, regla 2, PL7 y PL8); en la impresión secuencial, al menos `sequential.clearance_radius` (ficha `sequential.md`, regla 2). Es lo que la ficha `sequential.md` llama `arrange.distance` |

### `sequential`

De la ficha `docs/spec/features/sequential.md`: objeto por objeto. `enabled` va en el perfil de impresión; los dos
espacios del cabezal, en el de impresora.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `enabled` | booleano | `false` | | Con varios objetos (las copias de `slice --copies`, o los modelos de la placa en la interfaz), cada uno se lamina aparte y se imprime entero antes del siguiente, en el orden en que vienen (regla 1, SQ1). Al terminar uno: retrae, sube a la capa más alta impresa hasta ahí si hace falta, viaja al centro de la caja del siguiente, escribe las temperaturas de la primera capa sin esperar (`M140` y `M104` con `first_layer_bed_temperature` y `first_layer_temperature` de `machine.variables`, o `bed_temperature` y `temperature`) y `gcode.between_objects` (si ese texto ya pone temperaturas, solo el texto), apaga el ventilador, y la primera capa del siguiente viaja a esa altura hasta su primer punto y recién ahí baja (regla 3, SQ3). Cada objeto tiene su falda, su borde, su balsa y sus primeras capas (velocidad, ventilador, aceleración). Es la única forma de imprimir varios objetos en modo vaso (regla 5) |
| `clearance_radius` | número | 20 | 0 a 1000 | Radio, en mm, de un cilindro centrado en la boquilla que encierra el cabezal: las copias se acomodan al menos a esta distancia, y dos objetos más cerca son un error (regla 2, SQ2) |
| `clearance_height` | número | 20 | 0 a 1000 | Distancia en Z, en mm, de la punta de la boquilla a lo primero que puede chocar arriba (el eje X): un objeto más alto que esto que no es el último es un error (regla 4; Tisma Slicer no lo rechaza, SQ4) |

### `extruders`

De la ficha `docs/spec/features/multi_extruder.md`: qué extrusor imprime qué, con `machine.extruders` mayor que 1. Va en
el perfil de impresión.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `walls` | entero | 0 | 0 a 16 | Extrusor de los perímetros (también los de voladizo, los arcos y el relleno entre perímetros), desde 1; 0: el del objeto (regla 2, ME3) |
| `infill` | entero | 0 | 0 a 16 | Extrusor del relleno ralo; 0: el del objeto |
| `solid_infill` | entero | 0 | 0 a 16 | Extrusor del relleno sólido, las superficies, los puentes y el planchado; 0: el del objeto |
| `support` | entero | 0 | 0 a 16 | Extrusor del soporte; 0: el que esté imprimiendo (sin cambio de herramienta) |
| `support_interface` | entero | 0 | 0 a 16 | Extrusor de la interfaz del soporte; 0: el que esté imprimiendo |
| `bed_temperature_from` | entero | 0 | 0 a 16 | Extrusor cuyas temperaturas de cama valen; 0: el primero que imprime en la primera capa (para la primera) y en la segunda (para el resto) (regla 4, ME3) |
| `ooze_prevention` | booleano | `false` | | Los extrusores que esperan bajan a su temperatura más `standby_delta`: al empezar (sin esperarlos) y antes de cada cambio la que se deja (`M104 … ;cooldown`); la que entra espera su temperatura con `M109` (regla 7, ME4) |
| `standby_delta` | número | −5 | −300 a 300 | Cuánto cambia la temperatura de los que esperan, en °C |
| `wipe_tower` | entero | 0 | 0 a 16 | Extrusor que imprime la torre de purga en las capas sin cambio; 0: el que esté imprimiendo |

### `mmu`

De la ficha `docs/spec/features/multi_extruder.md` (regla 15): un solo extrusor que cambia de filamento
(`machine.single_extruder_mmu`), con la torre de purga. Va en el perfil de impresora.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `cooling_tube_length` | número | 5 | 0 a 1000 | Largo del tubo de enfriamiento, en mm: los movimientos de enfriamiento van y vuelven esto |
| `cooling_tube_position` | número | 91,5 | 0 a 1000 | Distancia de la boquilla al tubo, en mm: la descarga saca el filamento hasta el medio del tubo (esto más medio tubo) |
| `parking_position` | número | 92 | 0 a 1000 | Hasta dónde sale un filamento para estacionarlo, en mm: la carga lo empuja esto más `extra_loading_move` |
| `extra_loading_move` | número | −2 | −1000 a 1000 | Corrección de la carga, en mm |
| `high_current_on_swap` | booleano | `false` | | Más corriente en el motor durante el cambio: se lee y se guarda; el motor no escribe el comando |

Cada cambio, sobre la torre (ficha, regla 15, ME9; diseño propio sobre las medidas de la ficha): la compresión con el
filamento que sale (líneas de 1,5 veces el ancho de las paredes, un tramo por cuarto de segundo de
`extrusion.ramming`, cada uno a la velocidad de su caudal); la descarga hasta el medio del tubo (los primeros 15 mm a
`extrusion.unloading_speed_start`, el último cuarto más lento, dos tercios a la mitad de `unloading_speed` y el resto
al 30 %, lo demás a `unloading_speed`; lo que ya retrajo cuenta); la espera de `extrusion.toolchange_delay` (`G4`);
`extrusion.cooling_moves` idas y vueltas de `cooling_tube_length` en el tubo con la velocidad del filamento de
`cooling_initial_speed` a `cooling_final_speed`, la boquilla moviéndose a lo largo de la torre mientras tanto; la
retracción de cambio, el fin del filamento, `gcode.toolchange`, `T<n>` y el inicio del filamento nuevo; la carga
(un quinto a `loading_speed_start`, siete décimos a `loading_speed`, el último décimo a `loading_speed_start`), y la
purga en la torre. Sin la torre, el cambio no hace compresión ni movimientos sobre ella.

### `wipe_tower`

De la ficha `docs/spec/features/multi_extruder.md` (reglas 9 a 14): la torre de purga, con `machine.extruders` mayor que 1.
Va en el perfil de impresión.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `enabled` | booleano | `false` | | Torre de purga: los cambios de herramienta se hacen sobre ella. Solo con E relativa (`machine.relative_extrusion`; regla 9). Es un rectángulo con la esquina en (`x`, `y`), de `width` de ancho y tan hondo como pide la capa con más purga; en cada capa hasta la del último cambio: un lazo alrededor, un bloque de líneas a lo ancho por cada cambio, impreso por la herramienta que entra (con `machine.single_extruder_mmu`, las que hacen el volumen de `purge_volumes`; con varias boquillas, 5 líneas para cebar), y el resto de adentro lleno: denso en la primera capa, con columnas cada `bridging` arriba. En la primera capa también su borde. Antes de cada cambio el cabezal viaja a la torre; el cambio se hace ahí (regla 10 a 12, ME6 y ME7). Una torre fuera de la cama o sobre la pieza, su falda o su borde es un error |
| `x`, `y` | número | 180, 140 | −10000 a 10000 | La esquina de la torre en la cama, en mm |
| `rotation` | número | 0 | −360 a 360 | Giro de la torre, en grados, alrededor de su esquina |
| `width` | número | 60 | 1 a 1000 | Ancho de la torre, en mm |
| `brim_width` | número | 2 | 0 a 100 | Borde de la torre en la primera capa, en mm (en lazos de un cordón) |
| `bridging` | número | 10 | 1 a 100 | Distancia máxima entre las columnas de las capas ralas, en mm: las líneas de purga de arriba van de una a otra |
| `cone_angle` | número | 0 | 0 a 90 | Base en cono (regla 14): se lee y se guarda; la ficha no lo midió y el motor no la hace |
| `extra_spacing` | número | 100 | 50 a 500 | Separación de las líneas de la torre, en % de su ancho: más, torre más honda |
| `extra_flow` | número | 100 | 50 a 500 | Ancho de las líneas de la torre, en % del de las paredes: más, menos largo para el mismo volumen |
| `no_sparse_layers` | booleano | `false` | | Sin torre en las capas sin cambio (regla 11, ME7): la torre tiene solo la primera capa y las de un cambio, cada una sobre la anterior; en una capa con cambio, la boquilla viaja a la torre a la altura de la capa, baja hasta la torre y, al terminar ahí, sube derecho antes de volver a la pieza. Cuidado: baja al lado de la pieza |
| `purge_volumes` | número o lista de listas | 140 entre distintos, 0 en la diagonal | 0 a 10000 | Cuánto purgar al pasar de un filamento a otro, en mm³: un número para todos los pares distintos, o una fila por extrusor que sale con una columna por el que entra. Se multiplica por `extrusion.purge_multiplier` del que entra |

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
| `min_purge_on_tower` | número o lista | 0 | 0 a 10000 | Con una MMU y objetos que reciben la purga (en la interfaz y la API, por objeto: purgar en su relleno o en todo el objeto), lo que se purga igual en la torre antes, en mm³, por extrusor que entra. Después del cambio, el filamento nuevo imprime primero el relleno ralo (o todo) de esos objetos que todavía no se imprimió en esa capa, y la torre recibe solo lo que falta (ficha `multi_extruder.md`, regla 13, ME8) |
| `loading_speed`, `loading_speed_start` | número | 28, 3 | más de 0 a 1000 | Velocidades de carga del filamento en una MMU, en mm/s (ver `mmu`) |
| `unloading_speed`, `unloading_speed_start` | número | 90, 100 | más de 0 a 1000 | Velocidades de descarga, en mm/s (ver `mmu`) |
| `cooling_moves` | entero | 4 | 0 a 100 | Idas y vueltas de enfriamiento de la punta en el tubo de la MMU |
| `cooling_initial_speed`, `cooling_final_speed` | número | 2,2, 3,4 | más de 0 a 1000 | Velocidad del filamento en el primero y el último movimiento de enfriamiento, en mm/s |
| `ramming` | lista | de 7 a 11 en 11 pasos | hasta 100 caudales de 0 a 100 | Caudal de la compresión antes de descargar, en mm³/s, uno por cada cuarto de segundo (los valores por defecto son propios del motor) |
| `toolchange_delay` | número | 0 | 0 a 1000 | Espera después de descargar, en s (`G4`) |
| `load_time`, `unload_time` | número | 0 | 0 a 1000 | Tiempo que tarda la carga y la descarga, en s, para la estimación del tiempo (no se escribe nada) |
| `purge_multiplier` | número o lista | 100 | 0 a 1000 | % del volumen de purga (`wipe_tower.purge_volumes`) que pide el filamento al entrar, uno para todos o uno por extrusor (ficha `multi_extruder.md`, regla 12) |
| `infill_max_speed` | número | 0 | 0 a 1000 | Tope del relleno disperso con este filamento, en mm/s, para los patrones cuyas líneas no se cruzan en la capa (`rectilinear`, `gyroid`, `honeycomb` y los demás). 0: sin tope (`speeds_accels.md`, regla 5; la `material.infill_max_speed` de la ficha) |
| `infill_max_crossing_speed` | número | 0 | 0 a 1000 | Ídem para los patrones cuyas líneas se cruzan en la misma capa: `grid`, `triangles`, `stars`, `cubic`, `adaptive_cubic`, `support_cubic` |
| `bridge_flow_ratio` | número | 1 | 0,1 a 2 | Multiplica la sección del cordón redondo de los puentes y de los perímetros sobre el aire. El cordón es redondo, de diámetro `machine.nozzle_diameter` × √`bridge_flow_ratio` (con 0,4 de boquilla, 0,052245 mm de filamento por mm), y las líneas del puente van a esa distancia: con menos flujo, más juntas (`speeds_accels.md`, regla 14; `gcode_motion.md`, regla 1) |
| `chamber_temperature` | número | 0 | 0 a 200 | Temperatura de cámara que pide el filamento, en °C. Si la impresora calienta la cámara (`machine.max_chamber_temp`), se escribe antes del G-code de inicio, para que caliente junto con la boquilla, y otra vez después, sin pasar de su máximo: `M141 S…`, o `SET_HEATER_TEMPERATURE HEATER=chamber TARGET=…` en Klipper; al final se apaga. Si no la calienta o no llega, hay un aviso. 0: ninguna |
| `chamber_min_temperature` | número | 0 | 0 a 200 | Con cámara: después del G-code de inicio, espera a que la cámara llegue a esto antes de volver a pedir `chamber_temperature` (`M191 S…`, o `TEMPERATURE_WAIT SENSOR="heater_generic chamber" MINIMUM=…` en Klipper; ficha `gcode_extras.md`, regla 3). 0: no espera |
| `density` | número | 0 | 0 a 30 | Densidad del filamento, en g/cm³, para el peso al final del G-code (`; filament used [g]`, regla 1). 0: no se sabe y no se escribe el peso |
| `cost` | número | 0 | ≥ 0 | Costo del filamento por kg, para `; filament cost` |
| `spool_weight` | número | 0 | ≥ 0 | Peso del carrete vacío, en g (para que la interfaz compare con el carrete pesado) |
| `shrinkage_xy`, `shrinkage_z` | número | 0 | 0 a 20 | Cuánto se contrae el filamento al enfriarse, en %, a lo ancho de las capas y en Z: la pieza se lamina 100 / (100 − p) veces más grande, alrededor del centro de su base como `compensation`, para que al contraerse quede de la medida pedida (ficha `gcode_extras.md`, regla 4; con 1 %, un cubo de 99 mm se lamina de 100). Se suma a la escala de `compensation` |
| `start_gcode`, `end_gcode` | texto | `""` | | G-code de inicio y de fin del filamento (ficha `gcode_templates.md`, reglas 9 y 12): el inicio va después del de la impresora y de la espera de la boquilla, el fin antes del de la impresora. Mismo lenguaje de plantillas |
| `retraction`, `travel` | objeto | `{}` | | Los valores propios del filamento para la retracción y los viajes (ficha `retraction_travel.md`, regla 10, pruebas R9 a R11): `{"retraction": {"length": 0.8, "lift": 0}, "travel": {"slope": 5}}`. Se pueden dar `length`, `speed`, `unretract_speed`, `restart_extra`, `lift`, `lift_above`, `lift_below`, `min_travel`, `on_layer_change`, `wipe` y `before_wipe` de `retraction`, y `ramping_lift`, `slope` y `max_lift` de `travel`. Un valor puesto reemplaza al de la impresora para todo lo que imprime este filamento, aunque sea 0 (un salto de 0 anula el de la impresora); `null` o sin la clave, vale el de la impresora. Una `speed` de 0 deja la de la impresora |
| `abrasive` | booleano | false | | El filamento gasta una boquilla de latón (con fibra de carbono o de vidrio). Sin `machine.hardened_nozzle`, hay un aviso |
| `needs_enclosure` | booleano | false | | El filamento pide impresora cerrada. Sin `machine.enclosed`, hay un aviso |

Los avisos de capacidades (`nozzle_temperature`, `bed_temperature`, `chamber`, `enclosure`, `abrasive`) van con los
demás avisos del G-code (`src/report/warnings.hpp`; en la interfaz, en la barra de estado); no frenan la exportación.

### `gcode`

De la ficha `docs/spec/features/gcode_extras.md`. Va en el perfil de impresora (`docs/PROFILES.md`).

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `resolution` | número | 0.0125 | 0 a 1 | Resolución del G-code, en mm: un punto de una trayectoria no se escribe cuando el movimiento que lo reemplaza pasa a menos de esto, y el ancho, la Z y el cordón de abajo no cambian más de 0,0005 mm (`src/gcode/resolution.hpp`). La costura y los extremos quedan. 0: se escriben todos los puntos. Un cilindro de 128 lados (prueba C17 de `slicing.md`) pasa de 41 859 a 6 140 extrusiones, 64 por vuelta, y ningún tramo de menos de 0,01 mm |
| `toolchange` | texto | `""` | | G-code de cada cambio de herramienta (ficha `multi_extruder.md`, regla 5), con `previous_extruder`, `next_extruder` (desde 0) y `toolchange_z` (la Z de la capa). El cambio: la retracción de `retraction.toolchange_length`; con `extruders.ooze_prevention`, la que deja a su temperatura de espera (`M104 S… T<n> ;cooldown`); el fin del filamento que sale (`extrusion.end_gcode`); este texto; `T<n>` si el texto no tiene ya esa línea; con `ooze_prevention`, la espera de la que entra (`M109 S… T<n>`); el inicio del filamento que entra (`extrusion.start_gcode`). Al empezar también se corre, con `previous_extruder` −1 (regla 6). Cada cambio queda fuera de las etiquetas de objetos |
| `progress` | booleano | `false` | | `M73 P<porcentaje> R<minutos restantes>` al empezar y cada vez que cambia el porcentaje entero o los minutos, por el tiempo estimado (ficha `gcode_firmware.md`, regla 5), hasta `M73 P100 R0` al final; con `machine.silent_mode`, también `M73 Q S` con el tiempo del modo silencioso (salvo con `reprap` y los dialectos antiguos). Las marcas van al empezar cada capa y cada trayectoria |
| `object_labels` | texto | `"none"` | `none`, `octoprint`, `firmware` | Cada objeto de la placa (o copia de `slice --copies`) marcado en el G-code, para cancelar uno solo durante la impresión (ficha `gcode_firmware.md`, regla 4). `firmware`: en Marlin, al empezar `M486 S<n>`, `M486 A<nombre> (Instance <k>)` y `M486 S-1` por objeto, y en cada capa sus movimientos entre `M486 S<n>` y `M486 S-1`; en RepRapFirmware, lo mismo con `M486 S<n> A"<nombre> (Instance <k>)"` en una línea; en Klipper, `EXCLUDE_OBJECT_DEFINE NAME='<nombre>' CENTER=x,y POLYGON=[[x,y],...]` (la envolvente del objeto; espacios y puntos como `_`) y en cada capa `EXCLUDE_OBJECT_START`/`EXCLUDE_OBJECT_END`; en los demás firmwares, nada. `octoprint`: comentarios `; printing object <nombre> id:<objeto> copy <copia>` y `; stop printing object …`. En cada capa, los objetos de a uno: todas las islas de uno antes del siguiente |
| `auto_temperatures` | booleano | `true` | | Si el G-code de inicio de la impresora (ya resuelto) no tiene comandos de temperatura (`M104`, `M109`, `M140`, `M190`, `M141`, `M191`, `M116`, `G10 S`), el motor escribe antes la de la cama con espera (`M190 S…`, con `variables.bed_temperature`) y la de la boquilla (`M104 S…`), y después del inicio espera la boquilla (`M109 S…`), cada dialecto con los suyos (ficha `gcode_templates.md`, regla 10). `false`: nunca |
| `arcs` | booleano | `false` | | Los tramos de una trayectoria plana de un solo ancho que siguen un círculo se escriben como un arco: `G2` (horario) o `G3` (antihorario), con el centro `I J` relativo a donde empieza y la E de su largo (ficha `gcode_extras.md`, regla 10). Un arco junta 3 movimientos o más, cubre a lo sumo media vuelta, pasa a menos de `resolution` de cada punto y a menos del doble del medio de cada movimiento (`src/gcode/arcs.hpp`). Con `resolution` 0, a menos de 0,001 mm. La impresora tiene que aceptar arcos (en Marlin, `ARC_SUPPORT`; en Klipper, la sección `[gcode_arcs]`). En el cilindro de 10 mm de radio de X10 el G-code baja de 179 a 46 kB; en el Benchy, 2 885 arcos y de 7,39 a 6,95 MB |
| `binary` | booleano | `false` | | El archivo en el formato binario (`.bgcode`; ficha `gcode_extras.md`, regla 9; `src/gcode/binary.hpp`): encabezado `GCDE`, versión 1 y CRC32; bloques de datos del archivo (`Producer`), de la impresora (`printer_model`, `filament_type`, `nozzle_diameter`, `layer_height`, temperaturas, `fill_density`…), de la impresión (el resumen del final: filamento y tiempos), del slicer (cada ajuste) y el G-code en bloques de 64 KiB como mucho, cortados en finales de línea; sin comprimir y el G-code sin codificar. El CRC32 de cada bloque cubre su encabezado, sus parámetros y sus datos. Se verifica el texto y después se relee el binario antes de darle el nombre; un nombre que termina en `.gcode` pasa a `.bgcode`. Todavía sin miniaturas |
| `verbose_comments` | booleano | `false` | | Cada línea que escribe el motor lleva al final `; ` y qué hace (ficha `gcode_extras.md`, regla 12): `perimeter`, `infill`, `infill (bridge)`, `skirt`, `support material`, `gap fill`, `ironing` en las extrusiones; `retract`, `unretract`, `lift`, `lower`, `move to layer height` (después del G-code del usuario), `reset extrusion distance`, `simple layer change`, `travel to first layer point`, `move to first perimeter point` (o `infill`, `skirt`…), `travel`, `enable fan`, `disable fan`, `set acceleration`, `use absolute coordinates`, `use absolute distances for extrusion` (o `relative`). El G-code del usuario no cambia |
| `volumetric_e` | booleano | `false` | | Toda E (extrusión y retracción) en mm³ de plástico en vez de mm de filamento: la E de siempre por la sección del filamento (ficha `gcode_extras.md`, regla 11). El motor no escribe `M200`: va en el G-code de inicio |
| `color_change` | texto | `"M600"` | | Lo que se escribe en un cambio de color de `changes` (ficha `gcode_templates.md`, regla 13), con las variables de `machine.variables`, después de la marca `;COLOR_CHANGE` |
| `pause` | texto | `"M601"` | | Lo mismo para una pausa, después de `;PAUSE_PRINT` |
| `before_layer` | texto | `""` | | G-code justo antes del movimiento en Z de cada cambio de capa, desde la segunda capa (regla 11, prueba T8), con las variables y además `layer_num` (desde 0), `layer_z` y `max_layer_z` (la Z escrita, sin ceros de más). Va como G-code propio (`;TYPE:Custom`); si tiene algo más que comentarios, el viaje siguiente vuelve a escribir todos los ejes |
| `after_layer` | texto | `""` | | Lo mismo, justo después del movimiento en Z |
| `between_objects` | texto | `""` | | En la impresión secuencial, el G-code entre un objeto y el siguiente (ficha `sequential.md`, regla 3), con las variables y `current_object_idx`, el número del objeto que sigue (desde 0). Si pone temperaturas (`M104`, `M109`, `M140`, `M190`), el motor no escribe las suyas |


### `output`

De la ficha `docs/spec/features/gcode_templates.md` (reglas 15 a 17). Va en el perfil de impresión.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `filename_format` | texto | `"[input_filename_base].gcode"` | no vacío | Nombre del archivo cuando `slice -o` es una carpeta, en el lenguaje de plantillas, con las variables de la impresión, `fill_density` (`"15%"`) y `default_output_extension` (`".gcode"`, con el punto). `print_time` (`11m`, `1h7m`) y `total_weight` (g, con `extrusion.density`) se conocen al terminar: el archivo se escribe primero con ellas sin reemplazar (`{print_time}`) y después se renombra (prueba T11) |
| `post_process` | lista de rutas | `[]` | | Scripts que se corren sobre el G-code terminado (reglas 16 y 17), cada uno con la ruta del G-code como único argumento; pueden cambiar el archivo. En su entorno: `SLIC3R_PP_OUTPUT_NAME` (la ruta), `SLIC3R_PP_HOST` (`File`) y cada variable de las plantillas como `SLIC3R_<NOMBRE>` en mayúsculas, con `_` en vez de los puntos (`SLIC3R_LAYERS_HEIGHT`, `SLIC3R_LAYER_HEIGHT`, `SLIC3R_TEMPERATURE`). Antes de correrlos, la línea de comandos pregunta «Continue (Y/N)?» (o `--yes`); con `N` no queda G-code. Un script que no termina en 0 hace fallar el comando. La interfaz todavía no los corre |

### Plantillas de G-code

De la ficha `docs/spec/features/gcode_templates.md` (reglas 1 a 8; `src/gcode/template.hpp`). Todos los textos de
G-code (`machine.start_gcode` y `end_gcode`, `extrusion.start_gcode` y `end_gcode`, `gcode.before_layer`, `after_layer`,
`color_change`, `pause` y el `gcode` de `changes`) se resuelven al escribir:

- `[nombre]` se cambia por el valor de la variable; `{expresión}` por el de la expresión: números, `"textos"`, `true`,
  `false`; `+ - * / %` (un entero dividido por un entero da un entero; `+` une textos); comparaciones, `and`, `or`,
  `not`; `(condición ? sí : no)`; `texto =~ /expresión regular/`; `nombre[i]` de una lista (un valor solo es una
  lista de uno, y una lista de uno vale como su valor en cuentas y comparaciones); `(1, 2, 3)` hace una lista;
- funciones: `min`, `max`, `int` (sin redondear), `round` (las mitades hacia arriba), `digits(valor, ancho[,
  decimales])` y `zdigits` (rellena con espacios o ceros), `is_nil`, `one_of(valor, opciones…)`, `size` y `empty` (de
  listas), `interpolate_table(x, (x0, y0), (x1, y1)…)`;
- `{if …}…{elsif …}…{else}…{endif}`; `{local nombre = …}` (en esa plantilla) y `{global nombre = …}` (también en las
  que siguen);
- variables: cada ajuste por su nombre (`{layers.height}`), algunos por su nombre de siempre (`layer_height`,
  `first_layer_height`, `nozzle_diameter`, `filament_diameter`), las de `machine.variables` y las de la impresión:
  `total_layer_count`, `num_extruders`, `initial_extruder`, `initial_tool`, `is_extruder_used`,
  `input_filename_base` (el nombre del modelo en la línea de comandos), `first_layer_print_min`, `_max` y `_size`,
  `year`, `timestamp`, y `layer_num`, `layer_z` y `max_layer_z` (la primera capa hasta el primer cambio; en el fin, la
  última);
- un error (variable o función desconocida, mal usada, de sintaxis) hace fallar la exportación con la sección, la
  línea y una marca en la columna.

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
