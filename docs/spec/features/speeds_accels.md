# Velocidades y aceleraciones adicionales

**Qué hace:** lo de las velocidades que no está en `gcode_motion.md`:
- velocidades del relleno de huecos, sobre los puentes, del relleno de la primera capa y de Z en los viajes;
- velocidades dinámicas en los voladizos, según cuánto se apoya el cordón en la capa de abajo;
- topes de velocidad del relleno por filamento;
- un ecualizador que cambia el caudal de a poco;
- una aceleración por tipo de extrusión;
- la proporción de flujo de los puentes.

La velocidad de la interfaz del soporte está en `supports_more.md`; la de la primera capa sobre la balsa, en
`skirt_brim_raft.md`.

**Fase:** después de F4.

**Fuente del comportamiento:** documentación de usuario de los slicers (base de conocimiento de Prusa), el comando
[M204](https://marlinfw.org/docs/gcode/M204.html) de Marlin y las mediciones de abajo.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `speed.gap_fill` | mm/s | 20 | Relleno de huecos |
| `speed.over_bridges` | mm/s o % de `speed.solid_infill` | 0 = `speed.solid_infill` | Relleno sólido de la capa que va justo encima de un puente |
| `speed.first_layer_infill` | mm/s o % de `speed.solid_infill` | 0 = `speed.first_layer` | Relleno sólido de la primera capa |
| `speed.travel_z` | mm/s | 0 = `speed.travel` | Movimientos en Z |
| `speed.dynamic_overhangs` | — | no | Velocidad según el apoyo del cordón |
| `speed.overhang_0`, `_1`, `_2`, `_3` | mm/s o % de `speed.outer_wall` | 15, 15, 20, 25 | Velocidad con el 0 %, 25 %, 50 % y 75 % del ancho del cordón apoyado en la capa de abajo |
| `material.infill_max_speed` | mm/s | 0 = sin tope | Tope del relleno poco denso cuyas líneas no se cruzan en la capa |
| `material.infill_max_crossing_speed` | mm/s | 0 = sin tope | Tope del relleno poco denso cuyas líneas se cruzan en la capa |
| `speed.flow_slope_up`, `speed.flow_slope_down` | mm³/s² | 0 = libre | Cuánto puede subir o bajar el caudal por segundo |
| `acceleration.default` | mm/s² | 0 | Aceleración a la que se vuelve después de cada tipo; 0 apaga las aceleraciones por tipo |
| `acceleration.walls` | mm/s² | 0 | Perímetros internos |
| `acceleration.outer_wall` | mm/s² | 0 = `acceleration.walls` | Perímetro externo |
| `acceleration.infill` | mm/s² | 0 | Relleno poco denso |
| `acceleration.solid_infill` | mm/s² | 0 = `acceleration.infill` | Relleno sólido |
| `acceleration.top_solid_infill` | mm/s² | 0 = `acceleration.solid_infill` | Superficie de arriba |
| `acceleration.bridges` | mm/s² | 0 | Puentes y perímetros en voladizo |
| `acceleration.first_layer` | mm/s² | 0 | Toda la primera capa |
| `acceleration.first_layer_over_raft` | mm/s² | 0 | Primera capa de la pieza sobre la balsa |
| `acceleration.wipe_tower` | mm/s² | 0 | Torre de purga (`multi_extruder.md`) |
| `acceleration.travel` | mm/s² | 0 | Viajes |
| `acceleration.short_travel` | mm/s² | 0 | Viajes más cortos que `retraction.min_travel` |
| `extrusion.bridge_flow_ratio` | — | 1 | Multiplica la sección del cordón redondo de los puentes |

## Reglas

### Velocidades

1. **Relleno de huecos:** a `speed.gap_fill` (prueba V1).
2. **Sobre los puentes:** el relleno sólido de la capa que va justo encima de un puente sobre el aire va a
   `speed.over_bridges`. No vale para la capa encima de un puente interno (`infill_patterns.md`) (prueba V2).
3. **Relleno de la primera capa:** el relleno sólido de la primera capa va a `speed.first_layer_infill`; con 0, a
   `speed.first_layer`. En Tisma Slicer este valor no queda topeado por `speed.first_layer` (prueba V3).
4. **Z:** los movimientos en Z (cambios de capa y saltos al viajar) van a `speed.travel_z`; con 0, a `speed.travel`
   (prueba V4).
5. **Topes del relleno por filamento:** el relleno poco denso no pasa de `material.infill_max_speed` si sus líneas no se
   cruzan en la misma capa (`rectilinear`, `gyroid`, `honeycomb`), ni de `material.infill_max_crossing_speed` si se
   cruzan (`grid`, `stars`) (prueba V5).

### Voladizos

6. **Apoyo:** para cada tramo de perímetro, la parte de su ancho que queda sobre la capa de abajo, en %: 100 % es
   apoyado del todo y 0 % es en el aire. Un perímetro que se corre `d` hacia afuera de la capa de abajo, con ancho `w`,
   tiene `100 · (1 − d / w)` %.
7. **Velocidad dinámica:** con `speed.dynamic_overhangs`, la velocidad es `speed.overhang_0` con 0 %, `_1` con 25 %,
   `_2` con 50 %, `_3` con 75 % y la velocidad normal del perímetro con 100 %, y entre esos puntos se interpola en línea
   recta (prueba V6). Nunca pasa el tope del caudal (`speed.max_flow`).

### Ecualizador del caudal

8. **Rampas:** con `speed.flow_slope_down` (o `_up`), cuando el caudal (mm³/s) baja (o sube) de una extrusión a la
   siguiente, el tramo de antes (o el de después) se parte en pedazos de velocidad decreciente (o creciente), así el
   caudal cambia a no más de ese valor por segundo (prueba V7).
9. **Retracción:** después de una retracción no hay rampa de subida: la presión ya bajó (prueba V7).

### Aceleraciones

10. **Por tipo:** antes de cada extrusión se fija la aceleración de su tipo, con los reemplazos de la tabla cuando es 0.
    El soporte usa `acceleration.default`. En la primera capa todo usa `acceleration.first_layer` (si no es 0); en la
    primera capa sobre la balsa, `acceleration.first_layer_over_raft` (prueba V8).
11. **Viajes:** a `acceleration.travel`; los más cortos que `retraction.min_travel`, a `acceleration.short_travel`
    (prueba V8).
12. **Sin `acceleration.default`:** si es 0, las aceleraciones por tipo no se usan: solo se fija la de los viajes, y
    las extrusiones siguen con ella (prueba V8).
13. **Comando:** `M204 S` en RepRap y Klipper. En Marlin 2, `M204 P` para imprimir y la de los viajes una vez, como
    `M204 T`. Si los límites de la máquina se escriben en el G-code (`gcode_firmware.md`), ninguna aceleración pasa el
    límite (prueba V9).

### Puentes

14. **Proporción de flujo:** `extrusion.bridge_flow_ratio` multiplica la sección del cordón redondo de los puentes; el
    diámetro baja con la raíz y las líneas del puente quedan más juntas (prueba V10).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `walls_extra.md` (boquilla 0,4, capas
de 0,2, 2 perímetros, sin falda ni borde, centro en (100, 100)) y sin bajar la velocidad por el tiempo de capa.
**Cubo:** 20 mm, relleno al 20 %, 3 capas sólidas arriba y abajo. **Mesa:** la de `surfaces_infill.md` (puente de
10 mm a Z 5). **Embudo:** tronco de cono macizo de 10 mm de alto, radio 5 en la cama, que se abre hacia arriba con el
ángulo indicado desde la vertical. **Hongos:** los de `walls_extra.md` (W4), sombrero a 48°, 52°, 55° y 60°.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| V1 | Pared de 1,5 mm de `walls_extra.md` (W6), `walls.generator` `fixed` | `speed.gap_fill` 20 / 10 | Relleno de huecos (19,1 mm) a 20 mm/s / 10 mm/s |
| V2 | Mesa, relleno al 20 %, 3 capas sólidas | `speed.over_bridges` 0 con `speed.solid_infill` 20; 50 % y 10 con `speed.solid_infill` 40 | Capa 26 (Z 5,4, justo encima del puente): relleno sólido de 530,2 mm a 20 / 20 / 10 mm/s. Capa 33 (encima de un puente interno): a 40 mm/s con 50 % y con 10 |
| V3 | Cubo, capa 0 | `speed.first_layer` 30, `speed.solid_infill` 40; `speed.first_layer_infill` 50 % / 0 / 50 | Relleno sólido a 20 / 30 / 50 mm/s; perímetros a 30 |
| V4 | Cubo, salto de 0,4 mm al viajar | `speed.travel_z` 0 / 10 | Movimientos en Z a 130 mm/s (F7800) / 10 mm/s (F600) |
| V5 | Cubo, capa 10 | `material.infill_max_speed` 50, `material.infill_max_crossing_speed` 35 | `rectilinear`, `gyroid`, `honeycomb` a 50 mm/s; `grid` y `stars` a 35 |
| V6 | Embudos y hongos, capa 30 (embudo) o 60 (hongo) | `speed.dynamic_overhangs` sí, valores por defecto, `speed.outer_wall` 30 | Perímetro externo: embudo de 20° a 27,0 mm/s (de 26,96 a 27,16), de 30° a 24,9, de 40° a 22,6, de 45° a 21,1; hongo de 48° a 20,1, de 52° a 18,63, de 55° a 17,31, de 60° a 15,0. Con `w` = 0,45 y `d` = 0,2 · tan(ángulo), la regla 7 da 26,8, 24,87, 22,54, 21,11, 20,12, 18,62, 17,30 y 15,0. Sin el ajuste, todo a 30 |
| V7 | Cubo, capa 6 | `speed.flow_slope_down` 1,8 | El perímetro interno (60 mm/s, 4,885 mm³/s) termina en 16 pedazos que bajan a 30 mm/s (2,442 mm³/s) para seguir con el externo: baja 2,44 mm³/s en unos 1,4 s. Con `speed.flow_slope_up` 1,8: el perímetro interno de la capa 2, que sigue al relleno sólido (20 mm/s) sin retracción, empieza a 22,3 mm/s y sube por pedazos hasta 60; el relleno de la capa 6, que viene después de una retracción, empieza directo a 80 mm/s |
| V8 | Cubo, mesa y cubo con balsa de 3 capas | Externo 800, internos 1000, relleno 3000, sólido 2500, arriba 1500, puentes 500, primera capa 700, viajes 4000, por defecto 2000 | Capa 0: todo a 700. Capa 2: internos 1000, externo 800, sólido 2500. Capa 5: relleno 3000. Capa 49: superficie de arriba 1500. Mesa: `Overhang perimeter` a 500. Viajes a 4000; un viaje de 0,6 mm a 600 con `acceleration.short_travel` 600. Solo internos 1000 y relleno 3000 (más por defecto 2000): externo 1000, sólido y arriba 3000. Sin `acceleration.default`: todo a la de los viajes. Balsa: soporte a 2000, la primera capa de la pieza a 900 con `acceleration.first_layer_over_raft` 900 |
| V9 | Cubo, Marlin 2 | Internos 1000, relleno 3000, viajes 4000, por defecto 2000 | `M204 P1000`, `M204 P3000`, `M204 P2000`, y una vez `M204 T4000`. Con los límites escritos (máximo 1500): relleno `M204 P1500` y `M204 T1500` |
| V10 | Mesa | `extrusion.bridge_flow_ratio` 1 / 0,8 | `Overhang perimeter`: 0,05224 / 0,0418 mm de filamento por mm. Puente de la capa 25: 477,2 mm a 0,05277 / 523,8 mm a 0,04275 |
