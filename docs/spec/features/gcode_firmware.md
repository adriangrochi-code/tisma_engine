# G-code según el firmware

**Qué hace:** cambia los comandos del G-code según el firmware de la impresora: temperaturas, límites de la máquina,
etiquetas de cada objeto (para cancelar uno solo durante la impresión) y avance de la impresión. Lo demás del G-code
está en `GCODE_OUTPUT.md` y en `gcode_motion.md`.

**Fase:** F4 (Marlin y Klipper en la primera versión; RepRapFirmware después).

**Fuentes** (documentación de cada firmware, no su código):
- Marlin: [M73](https://marlinfw.org/docs/gcode/M073.html), [M201](https://marlinfw.org/docs/gcode/M201.html),
  [M203](https://marlinfw.org/docs/gcode/M203.html), [M204](https://marlinfw.org/docs/gcode/M204.html),
  [M205](https://marlinfw.org/docs/gcode/M205.html), [M486](https://marlinfw.org/docs/gcode/M486.html).
- Klipper: [Exclude Objects](https://www.klipper3d.org/Exclude_Object.html).
- RepRapFirmware: [G-code](https://docs.duet3d.com/User_manual/Reference/Gcodes) (`G10`, `M116`, `M201`, `M203`,
  `M204`, `M486`, `M566`).
- OctoPrint: [Cancel Objects](https://plugins.octoprint.org/plugins/cancelobject/) (lee comentarios).

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `machine.firmware` | — | `marlin2` | `marlin`, `marlin2`, `klipper`, `reprapfirmware` |
| `machine.limits_usage` | — | `time_only` | `emit`: escribir los límites en el G-code; `time_only`: usarlos solo para estimar el tiempo; `ignore` |
| `gcode.object_labels` | — | `none` | `none`, `octoprint` (comentarios) o `firmware` (comandos del firmware) |
| `gcode.progress` | — | no | Escribir el avance de la impresión (`M73`) |

Los límites de la máquina (aceleraciones, velocidades y tirones máximos por eje) están en el perfil de la impresora
(`profiles.md`).

## Reglas

1. **Temperaturas al empezar:**
   - antes del G-code de inicio se fija la temperatura de la boquilla sin esperar. Después del G-code de inicio se
     espera a que llegue;
   - Marlin y Klipper: `M104 S<t>` y `M109 S<t>`;
   - RepRapFirmware: `G10 S<t> P<herramienta>` y, para esperar, `M116`;
   - si el G-code de inicio ya tiene comandos de temperatura de boquilla, cama o cámara, el motor no los agrega:
     la persona decide el orden.
2. **Límites de la máquina** con `emit`, antes del G-code de inicio:

   | Firmware | Comandos | Unidades |
   |---|---|---|
   | Marlin y Marlin 2 | `M201 X Y Z E` (aceleraciones máximas); `M203 X Y Z E` (velocidades máximas); `M204 P R T` (impresión, retracción y viaje); `M205 X Y Z E` (tirones); `M205 S T` (velocidades mínimas al extruir y al viajar) | mm/s² y mm/s |
   | RepRapFirmware | `M201 X Y Z E`; `M203 X Y Z E`; `M204 P T`; `M566 X Y Z E` (tirones) | `M203` y `M566` en mm/min; el resto en mm/s² |
   | Klipper | No se escriben: Klipper los toma de su propia configuración. Pedir `emit` con Klipper es un error | — |

3. **E relativa en Marlin:** el firmware acumula la posición del extrusor y pierde precisión en impresiones largas.
   - Con E relativa (`M83`), cada cambio de capa lleva `G92 E0` después del salto en Z.
   - Tisma Slicer exige que la persona lo ponga en el G-code de cambio de capa, y si falta da error. El motor lo
     escribe solo.
4. **Etiquetas de objetos** (`gcode.object_labels`):
   - **`firmware`, Marlin:**
     - al empezar, se define cada copia: `M486 S<n>`, `M486 A<nombre> (Instance <k>)` y `M486 S-1`;
     - en cada capa, las extrusiones de una copia van entre `M486 S<n>` y `M486 S-1`;
   - **`firmware`, RepRapFirmware:** lo mismo, con nombre y número en una línea: `M486 S<n> A"<nombre> (Instance <k>)"`;
   - **`firmware`, Klipper:**
     - al empezar, una línea por copia: `EXCLUDE_OBJECT_DEFINE NAME='<nombre>' CENTER=<x>,<y> POLYGON=[[x,y],...]`,
       con un polígono que encierra la copia en XY;
     - en cada capa: `EXCLUDE_OBJECT_START NAME='<nombre>'` … `EXCLUDE_OBJECT_END NAME='<nombre>'`;
     - en el nombre, los espacios y los puntos se cambian por `_`;
   - **`octoprint`:** comentarios `; printing object <nombre> id:<objeto> copy <copia>` y
     `; stop printing object <nombre> id:<objeto> copy <copia>`.
5. **Avance** (`gcode.progress`):
   - `M73 P<porcentaje> R<minutos restantes>` al empezar, y cada vez que cambia el porcentaje entero o los minutos
     restantes;
   - en impresoras con modo silencioso, además `M73 Q<porcentaje> S<minutos>` para ese modo.
6. **Tiempo estimado al final:** comentario `; estimated printing time (normal mode) = <h>h <m>m <s>s`
   (`GCODE_OUTPUT.md`).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `slicing.md` y E relativa. Dos copias
del cubo de 20 mm, con centros en X = 100 y X = 74. Los valores de los límites son los del perfil de prueba.

| # | Ajustes | Resultado |
|---|---|---|
| FW1 | Marlin 2, `emit`, etiquetas `firmware`, avance | Al empezar: `M73 P0 R7`, `M73 Q0 S7`, `M201 X9000 Y9000 Z500 E10000`, `M203 X500 Y500 Z12 E120`, `M204 P1500 R1500 T1500`, `M205 X10.00 Y10.00 Z0.20 E2.50`, `M205 S0 T0`. Después `M486 S0`, `M486 Acube20.stl (Instance 1)`, `M486 S-1`, y lo mismo para la copia 2. Luego `M104 S200`, el inicio, `M109 S200` |
| FW2 | RepRapFirmware, lo mismo | `M201 X9000 Y9000 Z500 E10000`, `M203 X30000 Y30000 Z720 E7200` (mm/min), `M204 P1500 T1500`, `M566 X600.00 Y600.00 Z12.00 E150.00` (mm/min). `M486 S0 A"cube20.stl (Instance 1)"`. Temperatura con `G10 S200 P0` y espera con `M116` |
| FW3 | Klipper, `time_only`, etiquetas `firmware` | `EXCLUDE_OBJECT_DEFINE NAME='cube20_stl_(Instance_1)' CENTER=100.000,100.000 POLYGON=[[110.000,110.000],[90.000,110.000],[90.000,90.000],[110.000,90.000]]`, y la copia 2 con centro en 74. En cada capa, `EXCLUDE_OBJECT_START` y `EXCLUDE_OBJECT_END` con el mismo nombre |
| FW4 | Klipper con `emit` | Error: con Klipper no se escriben los límites |
| FW5 | Marlin 2 con E relativa y sin `G92 E0` en el G-code de capa | Error en Tisma Slicer. El motor escribe `G92 E0` en cada cambio de capa |
| FW6 | Marlin 2, etiquetas `octoprint` | `; printing object cube20.stl id:0 copy 0` … `; stop printing object cube20.stl id:0 copy 0`, sin `M486` |
| FW7 | Klipper con avance; impresión de 7 min 38 s | 107 líneas `M73`, de `P0 R7` a `P100`; al final, `; estimated printing time (normal mode) = 7m 38s` |
