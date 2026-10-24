# Ajustes de la primera versión

**Formato:** JSON con claves en inglés, `snake_case`.

**Valores del banco:** los de la columna «Banco» son los que usó Tisma Slicer en `BENCHMARKS.md`. Hay que usar los
mismos para comparar.

**Los perfiles de Tisma:** los convierte el lado de Tisma, no el motor. El motor no lee archivos INI.

## Impresora

| Clave | Unidad | Significado | Banco |
|---|---|---|---|
| `bed_shape` | mm | Polígono de la cama | 250 × 210 |
| `nozzle_diameter` | mm | Diámetro de la boquilla | 0,4 |
| `gcode_flavor` | — | `marlin`, `klipper` o `reprap`; cambia los comandos de ventilador, temperatura y aceleración | reprap |
| `relative_e` | — | Extrusión relativa o absoluta | false |
| `max_feedrate_xyze`, `max_acceleration_xyze` | mm/s, mm/s² | Límites de la máquina, para estimar el tiempo | — |
| `start_gcode`, `end_gcode` | — | G-code de inicio y fin con variables (`GCODE_OUTPUT.md`) | — |

## Filamento

| Clave | Unidad | Significado | Banco |
|---|---|---|---|
| `filament_diameter` | mm | Diámetro del filamento | 1,75 |
| `extrusion_multiplier` | — | Multiplica el caudal | 1 |
| `temperature`, `first_layer_temperature` | °C | Boquilla | — |
| `bed_temperature`, `first_layer_bed_temperature` | °C | Cama | — |
| `fan_min_speed`, `fan_max_speed` | % | Ventilador | — |
| `slowdown_below_layer_time` | s | Tiempo mínimo de capa (ver «Enfriamiento» en `BEHAVIOR.md`) | 5 |
| `min_print_speed` | mm/s | Primera velocidad hasta la que se baja para llegar al tiempo mínimo | 10 |

## Capas y paredes

| Clave | Unidad | Significado | Banco |
|---|---|---|---|
| `layer_height` | mm | Altura de capa | 0,3 |
| `first_layer_height` | mm | Altura de la primera capa | 0,35 |
| `perimeters` | — | Cantidad de perímetros | 3 |
| `perimeter_width`, `external_perimeter_width` | mm | Ancho nominal; con 0 se elige solo a partir de la boquilla | automático |
| `top_solid_layers`, `bottom_solid_layers` | — | Capas sólidas arriba y abajo | 3 / 3 |
| `seam_position` | — | `aligned`, `nearest`, `random` o `rear` | aligned |

## Relleno

| Clave | Unidad | Significado | Banco |
|---|---|---|---|
| `fill_density` | % | Densidad del relleno | 20 % |
| `fill_pattern` | — | `rectilinear`, `grid` o `gyroid` en la primera versión | Tisma usó `stars`; para comparar, usar `grid` y anotarlo |
| `top_fill_pattern`, `bottom_fill_pattern` | — | Superficies: `rectilinear` o `monotonic` | monotonic |
| `fill_angle` | ° | Ángulo base del relleno | 45 |

## Velocidades

| Clave | Unidad | Banco |
|---|---|---|
| `perimeter_speed` | mm/s | 60 |
| `external_perimeter_speed` | mm/s o % del perímetro | 50 % |
| `infill_speed` | mm/s | 80 |
| `solid_infill_speed`, `top_solid_infill_speed` | mm/s | — |
| `travel_speed` | mm/s | 130 |
| `first_layer_speed` | mm/s | — |
| `max_print_speed` | mm/s | 80 |

## Retracción

| Clave | Unidad | Significado | Banco |
|---|---|---|---|
| `retract_length` | mm | Retracción | 2 |
| `retract_speed` | mm/s | Velocidad de la retracción | 40 |
| `retract_lift` | mm | Salto en Z al viajar | 0 |
| `retract_before_travel` | mm | Viaje mínimo que retrae | 2 |

## Soportes (fase F5)

| Clave | Unidad | Significado | Banco |
|---|---|---|---|
| `support_material` | — | Activa los soportes | — |
| `support_threshold_angle` | ° | Voladizos más inclinados que esto (medido desde la horizontal) llevan soporte; 0 = automático | automático |
| `support_contact_distance` | mm | Separación en Z entre el soporte y la pieza | 0,2 |
| `support_interface_layers` | — | Capas densas bajo la pieza | 3 |
| `support_spacing` | mm | Separación de las líneas del soporte | 2,5 |
| `support_xy_distance` | mm o % | Separación horizontal pieza–soporte | — |
| `max_bridge_length` | mm | Los puentes más cortos que esto no llevan soporte (es la capacidad de la impresora) | 16 |
