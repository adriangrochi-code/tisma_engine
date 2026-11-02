# G-code: datos, firmwares antiguos y formatos de salida

**Qué hace:** lo de la salida que no está en `gcode_firmware.md` ni en `GCODE_OUTPUT.md`:
- peso y costo de la impresión (filamento y resina);
- temperatura de la cámara;
- compensación de la contracción del filamento;
- desfase de Z;
- los dialectos de firmwares antiguos;
- la segunda estimación de tiempo (modo silencioso);
- miniaturas, G-code binario, arcos, E volumétrica y comentarios detallados.

Las plantillas de G-code, sus variables, el nombre del archivo y los scripts de posproceso están en
`gcode_templates.md`.

**Fase:** después de F4 (el G-code binario y las miniaturas, con la interfaz P1).

**Fuentes:**
- G-code binario: especificación oficial de Prusa,
  [libbgcode, `doc/specifications.md`](https://github.com/prusa3d/libbgcode/blob/main/doc/specifications.md);
- comandos de Marlin: [M141](https://marlinfw.org/docs/gcode/M141.html),
  [M191](https://marlinfw.org/docs/gcode/M191.html), [G2-G3](https://marlinfw.org/docs/gcode/G002-G003.html),
  [M200](https://marlinfw.org/docs/gcode/M200.html);
- el resto, la documentación de usuario de los slicers y las mediciones de abajo.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `material.density` | g/cm³ | 0 | Densidad del filamento (solo para el peso y el costo) |
| `material.cost` | dinero/kg | 0 | Costo del filamento |
| `material.spool_weight` | g | 0 | Peso del carrete vacío (la interfaz lo usa para comparar con el carrete pesado) |
| `material.chamber_temperature` | °C | 0 = no se escribe | Temperatura de la cámara pedida |
| `material.chamber_min_temperature` | °C | 0 = no se espera | Temperatura de la cámara a la que ya se puede empezar |
| `material.shrinkage_xy`, `material.shrinkage_z` | % | 0 % | Contracción del filamento que se compensa agrandando la pieza |
| `machine.z_offset` | mm | 0 | Se suma a todas las Z del G-code |
| `machine.flavor` | — | `reprap` | Además de los de `gcode_firmware.md`: `repetier`, `smoothie`, `teacup`, `makerware`, `sailfish`, `mach3`, `machinekit` y `no_extrusion` |
| `machine.silent_mode` | — | no | La impresora tiene modo silencioso: segunda estimación con los segundos valores de los límites |
| `gcode.thumbnails` | — | vacío | Miniaturas: lista de `ancho x alto / formato` (`PNG`, `JPG`, `QOI`) |
| `gcode.binary` | — | no | Escribir G-code binario (`.bgcode`) |
| `gcode.arcs` | — | no | Reemplazar tramos rectos por arcos `G2`/`G3` |
| `gcode.resolution` | mm | 0,0125 | Tolerancia al simplificar el camino y al ajustar arcos |
| `gcode.volumetric_e` | — | no | La E en mm³ de plástico en vez de mm de filamento |
| `gcode.verbose_comments` | — | no | Un comentario que explica cada línea |
| `resin.bottle_cost` | dinero | 0 | Costo del frasco de resina |
| `resin.bottle_volume` | ml | 1000 | Volumen del frasco |
| `resin.bottle_weight` | kg | 1 | Peso del frasco |
| `resin.density` | g/ml | 1 | Densidad de la resina |

## Reglas

### Peso y costo

1. **Filamento:** al final del G-code, como comentarios (prueba X1):
   - `; filament used [mm]` y `; filament used [cm3]` (largo por la sección del filamento);
   - si `material.density` no es 0, `; filament used [g]` = cm³ × densidad;
   - `; filament cost` = gramos × `material.cost` / 1000;
   - `; total filament used [g]`, `; total filament cost` y `; total filament used for wipe tower [g]` (la torre de
     purga, `multi_extruder.md`), con dos decimales.
2. **Resina:** el material usado (ml) se escribe en el archivo de la impresora (`resin.md`). El peso es ml ×
   `resin.density` y el costo ml × `resin.bottle_cost` / `resin.bottle_volume`; la densidad es el peso del frasco
   dividido por su volumen. La interfaz los muestra (`interface.md`); el archivo no lleva el costo (prueba X9).

### Temperatura y tamaño

3. **Cámara:** si `material.chamber_temperature` no es 0, se escribe `M141 S<temperatura>` al empezar, antes de
   calentar la boquilla. Después de esperar la boquilla, si `material.chamber_min_temperature` no es 0, `M191 S<mínima>`
   (esperar) y otra vez `M141 S<temperatura>`; si es 0, solo otra vez `M141` (prueba X2).
4. **Contracción:** según la documentación, la pieza se agranda para que, al contraerse, quede de la medida pedida;
   el ejemplo de la documentación: si se midió 99 mm en vez de 100, se pone 1 %. En la línea de comandos de Tisma
   Slicer el ajuste no cambió nada (prueba X3); el motor puede usar el factor `100 / (100 − p)`, que deja el ejemplo
   exacto, y medirlo con una pieza real.
5. **Desfase de Z:** `machine.z_offset` se suma a todas las Z de la pieza, también a la del comentario `;Z:`; puede
   dar Z negativas. No toca el G-code de inicio escrito por el usuario (prueba X4).

### Firmwares

6. **Dialectos** (prueba X5):
   - `repetier` y `smoothie`: como `reprap`;
   - `teacup`: espera la temperatura con `M116` después de `M104`; apaga el ventilador con `M106 S0`;
   - `makerware` y `sailfish`: temperatura con `M104 S<t> T0`, sin `M109`; ventilador con `M126` (encender) y `M127`
     (apagar); `M73 P<porcentaje>` en cada capa; E absoluta sin `M82` y sin volver a 0 con `G92`. `makerware`
     además escribe `M101` antes de extruir y `M103` después;
   - `mach3` y `machinekit`: el eje del extrusor se llama `A` en vez de `E`; temperaturas y ventilador con `P` en vez
     de `S` (`M104 P200`, `M106 P252.45`). `mach3` no vuelve la E a 0; `machinekit` sí, con `G92 A0`, y termina con
     `M2`;
   - `no_extrusion`: los mismos movimientos sin ningún valor de E.
7. **Modo silencioso:** con `machine.silent_mode`, cada límite de la máquina tiene dos valores (normal y silencioso) y
   el G-code lleva dos estimaciones: `; estimated printing time (normal mode)` y `(silent mode)`, también para la
   primera capa. El avance `M73 Q S` del modo silencioso está en `gcode_firmware.md` (prueba X6).

### Formatos

8. **Miniaturas:** imágenes de la pieza de los tamaños y formatos de `gcode.thumbnails`, escritas al principio del
   G-code (o como bloques del binario). Las hace la vista 3D: Tisma Slicer no las escribe desde la línea de comandos
   (prueba X7). El formato exacto de los comentarios se mide con la interfaz en `formats.md`.
9. **G-code binario:** con `gcode.binary`, el archivo sigue la especificación de Prusa (prueba X8):
   - cabecera: `GCDE`, versión 1 y tipo de suma de control 1 (CRC32);
   - bloques en este orden: datos del archivo (`Producer=…`), datos de la impresora, miniaturas, datos de la impresión,
     datos del slicer y G-code; cada bloque con su tipo, compresión, tamaños y CRC32.
10. **Arcos:** con `gcode.arcs`, los tramos que siguen un arco dentro de `gcode.resolution` se reemplazan por `G2`
    (horario) o `G3` (antihorario) con el centro relativo `I J` y la E del arco. Con más tolerancia, menos arcos y más
    largos (prueba X10).
11. **E volumétrica:** con `gcode.volumetric_e`, toda E (extrusión y retracción) va en mm³: la E de siempre por la
    sección del filamento. Tisma Slicer no escribe `M200`; el usuario lo pone en su G-code de inicio (prueba X11).
    Esto vale para impresoras de filamento. Con `machine.feedstock` `pellet`, el motor escribe el comando del dialecto
    (`pellets.md`, regla 3).
12. **Comentarios detallados:** con `gcode.verbose_comments`, cada línea lleva al final `; ` y qué hace: `perimeter`,
    `infill`, `infill (bridge)`, `skirt`, `retract`, `unretract`, `reset extrusion distance`, `travel to first layer
    point`, `move to first perimeter point`, `move to first infill point`, `move inwards before travel`, `simple layer
    change`, `lift`, `enable fan`, `disable fan`, `set temperature`, `set temperature and wait for it to be reached`,
    y los del inicio y el fin (prueba X12).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `walls_extra.md` (boquilla 0,4,
filamento de 1,75, capas de 0,2, 2 perímetros, sin falda ni borde, centro en (100, 100)). **Cubo:** 20 × 20 × 10 mm.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| X1 | Cubo, relleno al 20 % | Densidad 1,24, costo 25 por kg, carrete de 200 g | `; filament used [mm] = 574.56`, `[cm3] = 1.38`, `[g] = 1.71`, `; filament cost = 0.04`, `; total filament used [g] = 1.71`, `; total filament cost = 0.04`, `; total filament used for wipe tower [g] = 0.00`. Con densidad 0: sin la línea de gramos, totales 0.00 |
| X2 | Cubo | Cámara 50, mínima 40 | `M141 S50` (línea 12), `M104 S200`, `M109 S200`, `M191 S40`, `M141 S50`. Sin mínima: `M141 S50` antes y después de `M109`. Igual en Marlin 2 |
| X3 | Cubo | Contracción 1 % en XY y 2 % en Z (también 5 %) | Sin cambio: perímetro externo de X 90,225 a 109,775 y 50 capas, como sin el ajuste |
| X4 | Cubo | `machine.z_offset` −0,3 | Capa 0: `;Z:-0.1` y `G1 Z-.1`; capa 1: `;Z:0.1`; última `G1 Z9.7`. El `G1 Z5 F5000 ; lift nozzle` del inicio no cambia |
| X5 | Cubo, salto de 0,4 | Cada dialecto | Capa 5 en `reprap`: `G1 X90.632 Y109.368 E27.92007`. `teacup`: `M104 S200`, `M116`, `M106 S0`. `makerware`: `M73 P10`, `M101 ; extruder on`, `E25.92007`, `M104 S200 T0`, `M126`/`M127`, sin `G92`. `sailfish`: lo mismo sin `M101`. `mach3`: `A25.92007`, `M104 P200`, `M109 P200`, `M106 P252.45`. `machinekit`: `A27.92007`, `G92 A0`, `M2` al final. `no_extrusion`: `G1 X90.632 Y109.368` sin E |
| X6 | Cubo, Marlin 2, límites escritos | `machine.silent_mode` | `; estimated printing time (normal mode) = 4m 24s` y `(silent mode) = 4m 27s`; primera capa 8 s en los dos. Límites `machine_max_acceleration_x = 9000,1000`, `machine_max_feedrate_x = 500,200`; `M201 X9000`, `M203 X500` (los normales) |
| X7 | Cubo | Miniaturas `16x16/PNG,32x24/JPG,20x20/QOI` | Desde la línea de comandos: ninguna miniatura en el G-code |
| X8 | Cubo | `gcode.binary` | Primeros bytes `47 43 44 45` (`GCDE`), `01 00 00 00` (versión 1), `01 00` (CRC32); primer bloque de tipo 0 sin comprimir, 67 bytes, con `Producer=Tisma Slicer 2.9.6` y `Produced on=…`; después un bloque de tipo 3 (impresora) con `printer_model=`, `filament_type=PLA`, `nozzle_diameter=0.4`… |
| X9 | Cubo, resina | Frasco de 50, 500 ml, 0,6 kg; densidad 1,2 | El archivo SL1 lleva `usedMaterial = 3.960000` (ml) y los ajustes del frasco; ningún costo. Costo esperado: 3,96 × 50 / 500 = 0,40; peso 4,75 g |
| X10 | Cilindro de 10 mm de radio, 5 mm de alto | `gcode.arcs` | Sin arcos: 3403 `G1`. Con arcos: 228 `G1`, 192 `G3` y 3 `G2`, por ejemplo `G3 X104.002 Y91.543 I-.08 J9.726 E42.29424`. Con `gcode.resolution` 0,05: 100 `G3` y 204 `G1` |
| X11 | Cubo, E relativa | `gcode.volumetric_e` | Tramo de 18,74 mm: `E.63419` sin el ajuste, `E1.52541` con él (× 2,405 mm²). Retracción `E-2` → `E-4.81056`. Sin `M200` |
| X12 | Cubo, relleno al 20 %, 3 capas sólidas, falda, salto | `gcode.verbose_comments` | `G1 Z1.2 F7800 ; simple layer change`, `G1 X109.368 Y109.368 ; travel to first layer point`, `G1 X90.632 Y109.368 E27.92007 ; perimeter`; en toda la pieza 1897 `; infill`, 400 `; perimeter`, 148 `; retract`, 112 `; infill (bridge)`, 20 `; skirt` |
