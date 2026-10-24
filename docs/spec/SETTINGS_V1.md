# Ajustes de la primera versión

**Formato y nombres:**
- los del motor, en `docs/CONFIG.md` (rama `dev`): JSON por grupos (`layers.height` es la clave `height` del grupo
  `layers`);
- los que esa página todavía no define son propuestas con el mismo formato, y el motor elige los definitivos;
- los nombres de los ajustes de Tisma Slicer y de PrusaSlicer no se usan; la correspondencia queda del lado de Tisma.

**Valores del banco:** los de la columna «Banco» son los que usó Tisma Slicer en `BENCHMARKS.md`. Hay que usar los
mismos para comparar.

**Los perfiles de Tisma:** los convierte el lado de Tisma, no el motor. El motor no lee archivos INI.

## Impresora

| Clave | Unidad | Significado | Banco |
|---|---|---|---|
| `machine.bed` | mm | Polígono de la cama | 250 × 210 |
| `machine.nozzle_diameter` | mm | Diámetro de la boquilla | 0,4 |
| `machine.flavor` | — | `marlin` (Marlin 2), `klipper` o `reprap` (RepRap genérico); cambia los comandos de ventilador, temperatura y aceleración. `features/gcode_firmware.md` propone además `marlin_legacy` y `reprapfirmware` | reprap |
| `machine.relative_extrusion` | — | Extrusión relativa o absoluta | false |
| `machine.max_speed`, `machine.max_acceleration` | mm/s, mm/s² | Límites de la máquina, para estimar el tiempo | — |
| `machine.start_gcode`, `machine.end_gcode` | — | G-code de inicio y fin con variables (`GCODE_OUTPUT.md`) | — |

## Filamento

| Clave | Unidad | Significado | Banco |
|---|---|---|---|
| `machine.filament_diameter` | mm | Diámetro del filamento | 1,75 |
| `material.flow_factor` | — | Multiplica el caudal | 1 |
| `material.nozzle_temp`, `material.first_nozzle_temp` | °C | Boquilla | — |
| `material.bed_temp`, `material.first_bed_temp` | °C | Cama | — |
| `cooling.fan_min`, `cooling.fan_max` | % | Ventilador | — |
| `cooling.min_layer_time` | s | Tiempo mínimo de capa (ver «Enfriamiento» en `BEHAVIOR.md`) | 5 |
| `cooling.min_speed` | mm/s | Primera velocidad hasta la que se baja para llegar al tiempo mínimo | 10 |

## Capas y paredes

| Clave | Unidad | Significado | Banco |
|---|---|---|---|
| `layers.height` | mm | Altura de capa | 0,3 |
| `layers.first_height` | mm | Altura de la primera capa | 0,35 |
| `walls.count` | — | Cantidad de perímetros | 3 |
| `walls.width`, `walls.outer_width` | mm | Ancho nominal; con 0 se elige solo a partir de la boquilla | automático |
| `surfaces.top_layers`, `surfaces.bottom_layers` | — | Capas sólidas arriba y abajo | 3 / 3 |
| `walls.seam` | — | `aligned`, `nearest`, `random` o `rear` | aligned |

## Relleno

| Clave | Unidad | Significado | Banco |
|---|---|---|---|
| `infill.density` | % | Densidad del relleno | 20 % |
| `infill.pattern` | — | `rectilinear`, `grid` o `gyroid` en la primera versión | Tisma usó `stars`; para comparar, usar `grid` y anotarlo |
| `surfaces.top_pattern`, `surfaces.bottom_pattern` | — | Superficies: `rectilinear` o `monotonic` | monotonic |
| `infill.angle` | ° | Ángulo base del relleno | 45 |

## Velocidades

| Clave | Unidad | Banco |
|---|---|---|
| `speed.walls` | mm/s | 60 |
| `speed.outer_wall` | mm/s o % del perímetro | 50 % |
| `speed.infill` | mm/s | 80 |
| `speed.solid_infill`, `speed.top_solid_infill` | mm/s | — |
| `speed.travel` | mm/s | 130 |
| `speed.first_layer` | mm/s | — |
| `speed.max` | mm/s | 80 |

## Retracción

| Clave | Unidad | Significado | Banco |
|---|---|---|---|
| `retraction.length` | mm | Retracción | 2 |
| `retraction.speed` | mm/s | Velocidad de la retracción | 40 |
| `retraction.lift` | mm | Salto en Z al viajar | 0 |
| `retraction.min_travel` | mm | Viaje mínimo que retrae | 2 |

## Soportes (fase F5)

| Clave | Unidad | Significado | Banco |
|---|---|---|---|
| `support.enabled` | — | Activa los soportes | — |
| `support.angle_limit` | ° | Voladizos más inclinados que esto (medido desde la horizontal) llevan soporte; 0 = automático | automático |
| `support.z_gap` | mm | Separación en Z entre el soporte y la pieza | 0,2 |
| `support.interface_layers` | — | Capas densas bajo la pieza | 3 |
| `support.spacing` | mm | Separación de las líneas del soporte | 2,5 |
| `support.xy_gap` | mm o % | Separación horizontal pieza–soporte | — |
| `support.free_bridge_length` | mm | Los puentes más cortos que esto no llevan soporte (es la capacidad de la impresora) | 16 |
