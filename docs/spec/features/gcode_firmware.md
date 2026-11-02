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
| `machine.flavor` | — | `reprap` | `marlin` (Marlin 2), `klipper`, `reprap` (RepRap genérico) y, propuestos, `marlin_legacy` (Marlin 1) y `reprapfirmware` (RepRapFirmware de Duet). Los tres primeros ya están en `docs/CONFIG.md` |
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
     en RepRapFirmware el mismo `G10` sin parámetros es la retracción del firmware (`retraction_travel.md`, regla 7):
     con las dos cosas, el G-code tiene `G10 S200 P0` para la temperatura y `G10` solo para retraer (medido el
     27-10-2026). Al terminar, todos los firmwares apagan la boquilla con `M104 S0`, también RepRapFirmware;
   - si el G-code de inicio ya tiene comandos de temperatura de boquilla, cama o cámara, el motor no los agrega:
     la persona decide el orden.
2. **Límites de la máquina** con `emit`, antes del G-code de inicio:

   | Firmware | Comandos | Unidades |
   |---|---|---|
   | Marlin 2 (`marlin`) | `M201 X Y Z E` (aceleraciones máximas); `M203 X Y Z E` (velocidades máximas); `M204 P R T` (impresión, retracción y viaje); `M205 X Y Z E` (tirones); `M205 S T` (velocidades mínimas al extruir y al viajar) | mm/s² y mm/s |
   | Marlin 1 (`marlin_legacy`) | Lo mismo, salvo `M204 S T`: S, la aceleración al imprimir, y T, la de retracción. En Marlin 2 la T es la de viaje ([M204](https://marlinfw.org/docs/gcode/M204.html)) | mm/s² y mm/s |
   | RepRapFirmware | `M201 X Y Z E`; `M203 X Y Z E`; `M204 P T`; `M566 X Y Z E` (tirones) | `M203` y `M566` en mm/min; el resto en mm/s² |
   | Klipper | No se escriben: Klipper los toma de su propia configuración. Pedir `emit` con Klipper es un error | — |
   | RepRap genérico (`reprap`) | No se escriben, sin error | — |

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
   - **`firmware`, RepRap genérico:** no se escribe nada;
   - **`octoprint`:** comentarios `; printing object <nombre> id:<objeto> copy <copia>` y
     `; stop printing object <nombre> id:<objeto> copy <copia>`.
5. **Avance** (`gcode.progress`):
   - `M73 P<porcentaje> R<minutos restantes>` al empezar, y cada vez que cambia el porcentaje entero o los minutos
     restantes;
   - en impresoras con modo silencioso, además `M73 Q<porcentaje> S<minutos>` para ese modo, salvo con RepRap
     genérico, que lleva solo `M73 P R`.
6. **Tiempo estimado al final:** comentario `; estimated printing time (normal mode) = <h>h <m>m <s>s`
   (`GCODE_OUTPUT.md`).
7. **Impresoras que analizan el archivo antes de imprimir (Anycubic Kobra 3, Kobra S1, Kobra X y sus variantes
   Combo, V2 y Max)**, pedido de un probador (31-10-2026: «la Kobra los lee todos, pero solo imprime con los de
   Anycubic Slicer»). Esas máquinas llevan un firmware propio de Anycubic derivado de Klipper que lee el archivo
   entero («parsing») antes de empezar, y rechaza o se traba con lo que no entiende; las Kobra anteriores (Kobra,
   Kobra 2, Neo, Go, Plus, Max, con Marlin) imprimen cualquier G-code. Lo que se sabe por fuentes públicas
   (02-11-2026), sin documentación oficial de Anycubic sobre el formato:
   - Kobra S1, S1 Max y X aceptan solo `.gcode` plano; la Kobra 3 V2 y la 3 Max también `.gcode.3mf` si el firmware lo
     trae ([SimplyPrint, tipos de archivo](https://help.simplyprint.io/en/article/anycubic-kobra-x-kobra-s1-and-kobra-3-connecting-and-which-file-types-work-1r760ze/)).
   - El analizador se cae con la línea `; filament_colour_type = 1;1;1;1` del bloque de configuración que escriben los
     programas derivados de Bambu Studio ([OrcaSlicer #12444](https://github.com/OrcaSlicer/OrcaSlicer/issues/12444)),
     y con listas de filamentos separadas por `;` en vez de `=` o `:`
     ([OrcaSlicer #16336](https://github.com/OrcaSlicer/OrcaSlicer/issues/16336)); la recomendación de esos programas
     es omitir el bloque de configuración entero (opción «skip config block»).
   - Anycubic Slicer Next es un derivado de OrcaSlicer; OrcaSlicer, ElegooSlicer y Cura traen perfiles de la Kobra S1
     que imprimen ([SimplyPrint, Kobra S1](https://simplyprint.io/compatibility/anycubic-kobra-s1)).
   Regla: la ficha de la impresora lleva un indicador «el firmware analiza el archivo» (propuesto
   `machine.strict_parser` = `anycubic`), encendido en los perfiles de esas Kobra. Con él, el motor:
   - escribe `.gcode` plano (no binario, no 3MF) aunque el perfil pida otra cosa, y lo dice en el aviso previo a la
     exportación;
   - no escribe el bloque de configuración al final del archivo ni ninguna línea cuyo valor tenga `;` adentro;
   - escribe al principio del archivo los comentarios de resumen que leen esas máquinas con los mismos nombres que
     el programa de la marca: cantidad total de capas, altura de capa, filamento usado en mm y en g, tiempo estimado,
     y la miniatura PNG que el perfil pida (`gcode_extras.md`, regla 8);
   - mantiene las etiquetas de objetos de Klipper (regla 4) y el avance `M73` (regla 5).
   Lo que falta verificar con una impresora real (prueba FW10): qué líneas exactas exige el analizador. El probador
   tiene que mandar un archivo del mismo modelo exportado con Anycubic Slicer Next y decir qué modelo de Kobra es;
   la prueba compara los comentarios del principio de ambos archivos y se imprime el del motor.

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
| FW8 | RepRap genérico, `emit`, etiquetas `firmware`, avance, modo silencioso | Sin límites, sin `M486` ni comentarios de objetos y sin error. `M73 P R` sin `Q` ni `S`. `M104 S200` antes del inicio y `M109 S200` después |
| FW9 | Marlin 1 y Marlin 2, `emit`, aceleraciones 1100 al imprimir, 1200 al retraer y 1300 al viajar | Marlin 1: `M204 S1100 T1200`. Marlin 2: `M204 P1100 R1200 T1300` |
| FW10 | Perfil Anycubic Kobra S1 (o la Kobra del probador), `strict_parser` = `anycubic`, cubo de 20 mm | Pendiente de medir: archivo `.gcode` plano sin bloque de configuración, sin `;` dentro de valores; los comentarios del principio con los mismos nombres que el archivo de Anycubic Slicer Next del mismo cubo; la impresora lo imprime sin quedarse en «parsing» |
