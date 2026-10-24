# Ajustes del motor (`--config ajustes.json`)

El formato es propio del motor: un objeto JSON con grupos, y nombres elegidos por este equipo. Las claves de otros
programas no se usan. Si Tisma Slicer necesita traducir sus ajustes, lo hace su adaptador, del lado de Tisma.

Reglas:
- Todas las claves son opcionales; las que faltan toman el valor por defecto.
- Una clave desconocida dentro de un grupo implementado es un error, para que los errores de tipeo no pasen en
  silencio.
- Los grupos todavía no implementados se aceptan, pero el motor avisa que no los usa.
- Las longitudes van en milímetros.

## Grupos implementados

### `layers`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `height` | número | 0.2 | > 0 | Alto de las capas, salvo la primera |
| `first_height` | número | 0.2 | > 0 | Alto de la primera capa |

### `slicing`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `fill_mode` | texto | `"union"` | `union`, `even_odd`, `fill_holes` | Cómo se decide qué está adentro cuando los contornos se superponen o se cruzan: `union` suma los volúmenes, `even_odd` alterna adentro y afuera en cada cruce, `fill_holes` además rellena los agujeros |
| `gap_closing_radius` | número | 0.049 | ≥ 0 | Largo máximo de un hueco que se cierra al unir contornos abiertos |
| `xy_offset` | número | 0 | cualquiera | Agranda (positivo) o achica (negativo) cada capa en XY; los agujeros se mueven al revés |
| `first_layer_shrink` | número | 0 | ≥ 0 | Cuánto se achica además la primera capa, contra la pata de elefante |
| `simplify_tolerance` | número | 0.01 | ≥ 0 | Se quitan los vértices de los contornos que se apartan menos que esto de la recta entre sus vecinos; 0 no simplifica. Menos segmentos aceleran los pasos siguientes (en una capa circular de 708 segmentos, 0,01 mm deja 208 y el eje medio tarda 3,4 veces menos) |

### `walls`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `count` | entero | 3 | 0 a 1000 | Paredes desde cada borde. Donde la pieza es más angosta que esas paredes, los cordones cambian de ancho para llenarla sin huecos (Kuipers et al. 2020); donde es más ancha, quedan las paredes al ancho de `width` y el medio queda para el relleno. 0: sin paredes, todo es relleno |
| `width` | número | 0.4 | > 0 | Ancho preferido de los cordones (w*); por ahora fijo, más adelante saldrá de la boquilla del grupo `machine` |
| `seam` | texto | `"aligned"` | `aligned`, `nearest`, `random`, `rear` | Dónde empieza cada lazo: `aligned`, en una esquina cóncava si hay, cerca de la costura de la capa de abajo; `nearest`, el punto más cercano al final del movimiento anterior; `random`, al azar pero igual en cada corrida; `rear`, el punto de Y máxima |

### `surfaces`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `top_layers` | entero | 3 | 0 a 1000 | Capas sólidas debajo de cada superficie superior expuesta |
| `bottom_layers` | entero | 3 | 0 a 1000 | Capas sólidas encima de cada superficie inferior expuesta |
| `top_pattern` | texto | `"monotonic"` | `monotonic`, `rectilinear` | Superficies superiores: `monotonic`, líneas sueltas todas en el mismo sentido, cada una después de las de abajo que la tocan (sin volver sobre lo hecho); `rectilinear`, en zigzag como las inferiores |

### `infill`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `density` | número | 20 | 0 a 100 | Densidad del relleno disperso, en %: líneas separadas `walls.width` / densidad. 0: sin relleno disperso. Las superficies sólidas van siempre al 100 % |
| `angle` | número | 45 | cualquiera | Ángulo de las líneas respecto del eje X, en grados; las capas impares suman 90 |
| `wall_overlap` | número | 25 | 0 a 100 | Cuánto entran en la pared interna, en % del ancho de cordón, las líneas sólidas que no se unen en zigzag (superficies superiores monotónicas). Con 25 %, el material depositado queda entre −0,5 % y +1,5 % del área |
| `pattern` | texto | `"rectilinear"` | `rectilinear`, `grid`, `gyroid` | Patrón del relleno disperso: `rectilinear`, líneas que giran 90° cada capa y se unen en zigzag; `grid`, las dos direcciones en cada capa, cada una al doble de separación (el mismo material); `gyroid`, el corte de la superficie giroide a la altura de cada capa, con el período que da la densidad (`angle` no se usa) |

### `machine`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `nozzle_diameter` | número | 0.4 | > 0 | Diámetro de la boquilla, en mm |
| `filament_diameter` | número | 1.75 | > 0 | Diámetro del filamento, en mm: la E de cada tramo es su volumen sobre esta sección |
| `relative_extrusion` | booleano | false | | `true`: E relativa (`M83`), cada movimiento lleva su E; `false`: absoluta (`M82`), con `G92 E0` en cada capa |
| `flavor` | texto | `"reprap"` | `marlin`, `klipper`, `reprap` | Dialecto de G-code (cambiará los comandos de ventilador, temperatura y aceleración) |
| `start_gcode`, `end_gcode` | texto | `""` | | G-code de inicio y de fin |
| `variables` | objeto | `{}` | nombre: valor o lista | Valores para `[nombre]`, `{nombre}` y `{nombre[i]}` dentro del G-code de inicio y fin. Los da quien llama al motor (el adaptador de Tisma pone ahí los valores de sus perfiles), así los nombres de los perfiles no entran al motor. Los números se escriben como en el JSON; un nombre desconocido queda como está |
| `max_feed_x`, `max_feed_y` | número | 200 | > 0 | Velocidad máxima de cada eje, en mm/s, para estimar el tiempo |
| `max_feed_z` | número | 12 | > 0 | Ídem para Z |
| `max_feed_e` | número | 120 | > 0 | Ídem para el extrusor |
| `acceleration` | número | 1000 | > 0 | Aceleración de los movimientos que extruyen, en mm/s² |
| `travel_acceleration` | número | 1250 | > 0 | Aceleración de los viajes y las retracciones, en mm/s² |
| `corner_speed` | número | 10 | ≥ 0 | Velocidad a la que se puede pasar por una esquina de 90° o más, en mm/s; en giros más suaves, la menor de las dos velocidades por el coseno del giro, nunca menos que esto |
| `bed` | lista de puntos | `[]` | 3 o más `[x, y]` | Polígono de la cama en mm (por ejemplo `[[0, 0], [250, 0], [250, 210], [0, 210]]`, la cama del banco). Si se da, un modelo con algún vértice fuera de ella (el borde cuenta como dentro) no se lamina: `slice` termina con código 4. Vacío: no se comprueba, porque el motor no mueve los modelos y los del banco están centrados en el origen |
| `max_height` | número | 0 | ≥ 0 | Altura máxima de impresión en mm; un modelo más alto termina con código 4. 0: no se comprueba |

### `speed`

Velocidades en mm/s. Ninguna extrusión pasa de `max`; en la primera capa, ninguna pasa de `first_layer`.

| Clave | Tipo | Por defecto | Significado |
|---|---|---|---|
| `walls` | número | 60 | Paredes internas |
| `outer_wall` | número o texto | `"50%"` | Pared externa: mm/s, o un porcentaje de `walls` como `"50%"` |
| `infill` | número | 80 | Relleno disperso |
| `solid_infill` | número | 80 | Superficies inferiores, puentes y relleno de huecos |
| `top_solid_infill` | número | 50 | Superficies superiores |
| `travel` | número | 130 | Viajes |
| `first_layer` | número | 30 | Tope de la primera capa |
| `max` | número | 80 | Tope de toda extrusión |

### `retraction`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `length` | número | 2 | ≥ 0 | Cuánto filamento se retrae, en mm; 0 no retrae |
| `speed` | número | 40 | > 0 | Velocidad de la retracción y de su vuelta, en mm/s |
| `lift` | número | 0 | ≥ 0 | Cuánto sube Z durante un viaje que retrae, en mm |
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

Soportes según `docs/spec/BEHAVIOR.md` («Soportes»). Por ahora el motor calcula dónde van (áreas por capa); las líneas
del soporte, la interfaz y los puentes cortos vienen después.

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `enabled` | booleano | false | | Activa los soportes |
| `threshold_angle` | número | 0 | 0 a menos de 90 | Una cara más tendida que este ángulo, medido desde la horizontal, lleva soporte; 0 es automático: cada capa puede salir medio cordón más afuera que la de abajo |
| `z_gap` | número | 0.2 | ≥ 0 | Separación en Z entre el soporte y la pieza de arriba, en mm (se redondea a capas enteras; valor de banco) |
| `xy_gap` | número | 0.6 | ≥ 0 | Separación horizontal entre el soporte y la pieza, en mm |

### `cooling`

Enfriamiento según `docs/spec/BEHAVIOR.md` («Enfriamiento»).

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `min_layer_time` | número | 5 | ≥ 0 | Tiempo mínimo de capa, en s; una capa más rápida se imprime más lento (valor de banco) |
| `min_speed` | número | 10 | > 0 | Primera etapa: las extrusiones bajan todas en la misma proporción, pero no por debajo de esto (en mm/s, valor de banco); si no alcanza, la segunda etapa las pone a todas a una misma velocidad, la más alta que llega al tiempo, nunca por debajo de 1 mm/min; las que ya iban más lento no cambian |
| `fan_min` | número | 35 | 0–100 | Ventilador en las capas normales, en % |
| `fan_max` | número | 100 | 0–100 | Ventilador en las capas que tuvieron que ir más lento, en % |
| `fan_off_layers` | entero | 1 | 0–1000 | Las primeras capas van sin ventilador, aunque se hayan frenado |

### `compute`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `gpu` | texto | `"auto"` | `auto`, `off` | `auto` usa la GPU si hay una (Vulkan); `off` usa solo la CPU |

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
