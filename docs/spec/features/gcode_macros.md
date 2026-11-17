# Lenguaje de macros del G-code de inicio y de fin

**Qué hace:** el motor interpreta el lenguaje de macros con que vienen escritos los G-code de inicio, de fin y los
demás G-code a medida de las impresoras del paquete de perfiles (`profile_pack.md`), para usarlos tal cual, sin
convertirlos. Así una impresora hace lo que hace con el programa de su fabricante: nivelar la cama, ajustar la altura
de la boquilla, calibrar el flujo, limpiar la boquilla.

**Pedido del dueño** (05-11-2026): «usar el G-code directamente de cada máquina de forma original. De esa manera
evitamos tener que convertir G-code y perder funciones». Visto en la beta con una Bambu Lab: con el G-code de inicio
genérico no nivela ni calibra y la primera capa sale alta.

**Fase:** antes de la 1.0; primero para las Bambu Lab.

**Fuente:** la documentación pública para usuarios de los macros de PrusaSlicer
(`https://help.prusa3d.com/article/macros_1775`) y su lista pública de variables
(`https://help.prusa3d.com/article/list-of-placeholders_205643`). La lista de variables que de verdad se usan la
saca la sesión del paquete de perfiles de los datos del paquete (regla 9). Ningún código de otros programas.

## El lenguaje

1. **Construcciones:**
   - **condición:** `{if <condición>}…{elsif <condición>}…{else}…{endif}`; `elsif` y `else` son opcionales y puede
     haber varios `elsif`; pueden anidarse;
   - **expresión:** `{<expresión>}` se reemplaza por su valor;
   - **bloque de sentencias:** `{ sentencia; sentencia; … }` en varias líneas. Sentencias: `local nombre = valor;` y
     `global nombre = valor;` (variables nuevas, `local` solo dentro de esta plantilla); una asignación
     `nombre = valor;` o `nombre[i] = valor;` (también a variables del motor como `position[2]`, que cambia la posición
     que sigue); `if <condición> then … elsif <condición> then … else … endif` (pueden anidarse, sin `;` después de
     `endif`); y un texto o una expresión sueltos terminados en `;`, que se escriben en la salida tal cual (un salto de
     línea dentro de las comillas sale como salto de línea). Lo que no sea una sentencia de texto no escribe nada
     (prueba GM18);
   - **forma vieja:** `[variable]` o `[variable_índice]` (por ejemplo `[temperature_0]`) se reemplaza por el valor, sin
     cuentas.
2. **Valores:** números (enteros y con decimales), textos entre comillas dobles (`"texto"`, sin interpretar lo que
   tengan adentro: `{"[esto sale tal cual]"}`), sí/no (`true`, `false`), expresiones regulares entre barras
   (`/regex/`) y vectores de una dimensión (`temperature[0]`). Un vector de más dimensiones solo se puede escribir
   entero como texto, no usar en cuentas.
3. **Operadores:**
   - comparación `<`, `>`, `<=`, `>=`, `==`, `!=`, `<>` (igual que `!=`), `=` (igual que `==`);
   - lógicos `and`, `or`, `not` y sus formas `&&`, `||`, `!`;
   - cuentas `+`, `-`, `*`, `/` (y `%`, el resto, que usan los G-code del paquete);
   - ternario `(condición ? a : b)`, siempre entre paréntesis;
   - coincidencia con una expresión regular `=~` y no coincidencia `!~`;
   - paréntesis para agrupar. Precedencia usual: `not` y `-` de un operando, después `*` `/` `%`, `+` `-`,
     comparaciones, `and`, `or`, ternario.
   - **división entre enteros** (medido en PrusaSlicer 2.7.2 como caja negra, 17-11-2026): si los dos lados son
     enteros, `/` da la parte entera: `{1/3}` es 0, `{10/4}` es 2, `{2/3*1000000}` es 0 y `{first_layer_temperature[0]/3}`
     con 200 °C es 66. Si uno tiene decimales, la cuenta es con decimales: `{21/2.4053*60}` es 523,843 (prueba GM23);
   - **cómo se escribe un número calculado** (misma medición): con 6 cifras significativas como mucho, sin ceros al
     final y sin punto si queda entero: `{60}` → `60`, `{2.5}` → `2.5`, `{100.0}` → `100`, `{0.1+0.2}` → `0.3`,
     `{nozzle_diameter[0]*2}` → `0.8`, `{21/2.4053*60}` → `523.843`; los muy chicos o muy grandes en notación
     científica: `{1e-7*3}` → `3e-07`, `{123456789.123}` → `1.23457e+08`. Vale para todos los dialectos que usan
     este lenguaje (`prusaslicer`, `orcaslicer`, `crealityprint`, `elegooslicer`); OrcaSlicer no se pudo medir
     todavía (la red del equipo de especificación no lo deja bajar): si difiere, se corrige acá (prueba GM23).
4. **Funciones:** `min` y `max` de dos valores; la parte entera, que se escribe con la palabra `int` y corta los
   decimales; `round`, que redondea a entero; `ceil`, que redondea hacia arriba (dialecto `orcaslicer` y derivados); `digits` con un valor, una cantidad de caracteres y, opcional, de
   decimales (0 si falta): redondea a esos decimales, agrega ceros a la derecha si faltan y rellena con espacios a la
   izquierda hasta esa cantidad de caracteres; `zdigits`, igual pero rellenando con ceros; `is_nil` de una variable:
   si no tiene valor.
5. **Errores:** una sintaxis mal escrita, una variable desconocida o un índice fuera del vector no dejan el G-code
   roto: el laminado falla con el código `gcode_macro` y dice en castellano la línea, la columna y qué pasó
   («G-code de inicio, línea 12: la variable "curr_bed_type" no existe»). Nunca se escribe un G-code a medias.
6. **Seguridad:** el lenguaje no lee ni escribe archivos, no corre programas ni accede a la red; un G-code con un
   bucle o una expresión demasiado larga (más de 100 000 pasos de evaluación) se corta con `gcode_macro`.

## Las variables

7. **De dónde salen:** cada variable es un ajuste del motor, un valor del laminado (capa, altura, extrusor, caja de la
   primera capa, fecha, nombre del archivo) o un dato de la impresora y el filamento elegidos. Un G-code de otro
   programa usa sus propios nombres; el motor los acepta a todos los de la lista de la regla 9 y los llena con sus
   valores.
8. **Vectores por extrusor:** `temperature[0]`, `nozzle_temperature_initial_layer[1]`… tienen un valor por extrusor;
   con un solo extrusor, solo el índice 0.
   Una impresora de varios cabezales (`machine.extruders` mayor que 1, por ejemplo la Prusa XL de 2 y 5 cabezales)
   tiene un valor por cabezal en cada vector, aunque se imprima con uno solo: los que no se usan toman los valores del
   filamento del cabezal 0, e `is_extruder_used[i]` vale no. Un índice fuera del vector dentro de `is_nil(...)` da sí
   y no es un error (QA, 17-11-2026: las 32 variantes de la XL del paquete caían en la versión simple por
   `filament_multitool_ramming[1]`) (prueba GM20).
9. **La lista:** `docs/FORMAT.md` del paquete de perfiles trae, en una tabla, cada variable que usan los G-code del
   paquete, su tipo (número, texto, sí/no, vector), en qué G-code está disponible (inicio, fin, cambio de capa, cambio
   de herramienta, pausa) y su significado. El motor llena todas las de esa tabla; una variable que el motor todavía
   no sabe llenar se avisa en el registro con su nombre y se marca «sin probar» en el panel de la impresora. La lista
   está en `gcode_variables.md`.
10. **Variables de las impresoras Bambu Lab:** además de las de temperatura y de la cama, sus G-code de inicio usan el
    tipo de placa elegido, si se nivela la cama, si se calibra el flujo y si se hace la calibración de vibraciones;
    esos valores salen de las opciones de impresión (regla 12).

9 bis. **El G-code de la máquina tiene que salir igual que en el programa original** (decisión del dueño,
    17-11-2026). Ninguna variable que use un G-code del paquete queda vacía o en 0 si tiene un valor real: con un valor
    vacío, una condición `{if …}` toma otra rama y el G-code ya no es el de la máquina. Las que faltaban (barrido de QA
    del 17-11-2026, 447 variantes) y su valor:
    - `filament_abrasive[i]`: si el filamento del cabezal `i` es abrasivo (dato del filamento del paquete); si no lo
      trae, no;
    - `nozzle_high_flow[i]`: si la boquilla es de alto caudal (las variantes «HF» de Prusa: sí; dato de la impresora);
    - `nozzle_volume_type`: «High Flow» o «Standard», del mismo dato;
    - `zhop`: `retraction.lift`;
    - `outer_wall_acceleration`: la aceleración de la pared externa;
    - `print_sequence`: «by object» con `sequential.enabled`, si no «by layer»;
    - `timelapse_type`: 0 (sin video), salvo que la impresora lo pida;
    - `has_single_extruder_multi_material_priming`: no, salvo una impresora de un cabezal con varios filamentos que
      cebe al empezar;
    - `long_retraction_when_cut`: el dato del filamento si lo trae; si no, no;
    - `e_retracted[i]`: lo que está retraído el cabezal `i` en ese punto, en mm;
    - `position[0..2]`: la posición X, Y, Z de la boquilla en ese punto;
    - `extruded_volume[i]` (mm³), `extruded_weight_total` y `filament_weight` (g): lo usado hasta ese punto; en el
      G-code de fin, el total;
    - `print_settings_id`, `filament_settings_id`, `printer_preset`, `print_preset`, `physical_printer_preset`: el
      nombre del perfil de impresión, de filamento y de impresora elegidos; la impresora física, su nombre si la
      persona le dio uno, si no el de la impresora;
    - `notes`: las notas del perfil de impresión; `machine_name` (Cura): el nombre de la impresora;
    - `print_bed_max`: la esquina de la cama con X e Y más grandes;
    - `build_volume_temperature` (Cura): `extrusion.chamber_temperature`; `wall_thickness` (Cura): el número de
      paredes por el ancho de línea; `material_standby_temperature` (Cura): la de espera del filamento si la trae,
      si no la de impresión;
    - `nozzle_temperature_initial_layer_1` y las demás de la forma vieja `[variable_índice]`: el valor del cabezal
      `índice`, también en una impresora de varios cabezales (regla 8);
    - `date`, `time` (Cura): la fecha y la hora de laminado, en el formato de la documentación de Cura;
    - `bed_mesh_probe_count`, `bed_mesh_algo` (OrcaSlicer, malla adaptativa de Klipper): si el perfil trae los valores,
      esos; si no, falta medirlos en OrcaSlicer como caja negra: hasta entonces se avisa en la interfaz, no solo en el
      registro.
    - segunda tanda (QA, 17-11-2026, con el motor 272dd50): `wipe_tower_center_pos_valid`: no, salvo que haya torre de
      purga; `start_filament_gcode`: el G-code de inicio del filamento; `required_nozzle_HRC`: el del filamento, si no
      0; `filament_flush_volumetric_speed`: el del filamento, si no su caudal máximo; `inner_wall_speed`,
      `outer_wall_speed`, `initial_layer_speed`: las velocidades de pared interna, externa y de primera capa;
      `outer_wall_line_width`, `extrusion_width`: los anchos de línea de la pared externa y el general; `filament_notes`:
      las notas del filamento; `print_bed_max` también en el dialecto `prusaslicer` (Creality); y de Cura:
      `print_time` (el tiempo estimado, como lo escribe Cura), `top_bottom_thickness` (capas de arriba por la altura de
      capa), `filament_amount` (m), `filament_cost`, y `purge_distance`, `retraction_hop_height_after_extruder_switch`,
      `switch_extruder_retraction_amount` del perfil de la impresora si los trae, si no los valores por defecto de
      Cura. Datos de la definición base de Cura (`fdmprinter.def.json`, rama main, leída el 17-11-2026):
      `switch_extruder_retraction_amount` vale `machine_heat_zone_length`, 16 mm por defecto; la subida al cambiar de
      cabezal se llama en Cura `retraction_hop_after_extruder_switch_height` y vale la de `retraction_hop` (en Tisma,
      `retraction.lift`), así que `retraction_hop_height_after_extruder_switch` toma ese valor; `purge_distance` no
      existe en Cura (solo lo nombra el G-code de cambio de cabezal de la Sovol SV04, que en modo de un cabezal no se
      usa): queda con aviso; `print_time` solo aparece en comentarios (`;Print time: {print_time}`) y toma el tiempo
      estimado como «1h 2m 3s»;
    - `date` sale como `dd-mm-aaaa` y `time` como `HH:MM:SS`, la hora local (como Cura).
    Una variable sin valor conocido sigue saliendo con aviso, que la interfaz muestra al laminar (prueba GM22).
10 bis. **Dialectos** (campo `dialect` del bloque `gcode` del paquete, `profile_pack_format.md`): `prusaslicer` es el
    lenguaje de arriba con los nombres de la documentación pública de PrusaSlicer; `orcaslicer`, `crealityprint` y
    `elegooslicer` usan el mismo lenguaje con sus propios nombres de variables, que el motor llena con el valor
    equivalente; `cura` solo reemplaza `{nombre}` por su valor, sin condiciones ni cuentas. Si el G-code de un
    dialecto no se puede interpretar, se usa la versión simple del perfil y se avisa en el registro y en la interfaz.
    La versión simple siempre lleva a la impresora al origen: si no tiene `G28` ni una macro de inicio
    (`START_PRINT`, `PRINT_START`), el motor escribe `G28` antes de la primera extrusión (prueba GM21).
10 bis bis. **Listas donde se espera un número** (dialecto `orcaslicer` y sus derivados): una variable que es una lista
    de números o porcentajes, usada en una cuenta o una comparación sin índice (por ejemplo
    `{if outer_wall_acceleration > 0}`), vale su único elemento si tiene uno, y si no, el del extrusor en uso. Antes,
    un G-code así no se podía interpretar y caía en la versión simple (prueba GM15).
10 ter. **Extrusión:** con el G-code del bloque `gcode`, el motor extruye en relativo o absoluto según
    `relative_extrusion` de ese bloque.

10 quater. **Variables calculadas** (urgente, BH45): las que el G-code de una máquina usa para velocidades no pueden
    quedar en 0. El motor las calcula de sus ajustes:
    - `outer_wall_volumetric_speed`: velocidad de la pared externa × ancho de línea × altura de capa (mm³/s);
    - `flush_volumetric_speeds[i]` y `filament_max_volumetric_speed[i]`: el caudal máximo del filamento i (mm³/s);
    - `flush_temperatures[i]`, `nozzle_temperature_range_high[i]`: la temperatura del filamento i;
    - `initial_no_support_extruder`, `initial_extruder`: el extrusor que empieza;
    - las de `gcode_variables.md` con equivalente: el valor del equivalente;
    - del G-code de las Bambu Lab (medido con el paquete, 09-11-2026):
      - `filament_max_volumetric_speed[i]`: `extrusion.max_flow` del filamento i (ya viene en el paquete);
      - `first_non_support_filaments`: una lista con un valor por extrusor, el número del primer filamento que no es
        de soporte (con uno solo, `[0]`);
      - `first_layer_center_no_wipe_tower`: el centro `[x, y]` de la caja de la primera capa sin la torre de purga;
      - `initial_filament_id`, `initial_no_support_filament_id`, `initial_nozzle_id`, `current_filament_id`: el número
        del filamento o la boquilla que empieza (0 con uno solo);
      - `filament_ids`: una lista de textos, uno por filamento (vacíos si el perfil no da uno);
      - `curr_y_acceleration_limit`: la aceleración máxima en Y de la impresora;
      - `max_print_z`: la Z más alta que se imprime;
      - `nozzle_diameter_at_nozzle_id`: la lista de diámetros por número de boquilla física (empieza en 0); con una
        sola boquilla, `[diámetro]`;
      - `initial_no_support_nozzle_id`: el número de la boquilla física (empieza en 0) que imprime el primer
        filamento que no es de soporte; se fija una vez al empezar (0 con una sola boquilla). La Bambu Lab H3C lo usa
        como índice de `nozzle_diameter_at_nozzle_id` (paquete de perfiles, Bambu Studio 930fe93, 12-11-2026);
      - `has_tpu_in_first_layer`: verdadero si algún filamento de la primera capa tiene `filament.type` `TPU`; se fija
        al empezar;
      - `long_retraction_when_ec`: del filamento actual, la opción de retracción larga al cambiar de extrusor (falso
        si el paquete no la trae); se fija al empezar y en cada cambio de filamento;
      - `curr_bed_type`: el nombre de la placa del proyecto («Cool Plate», «Engineering Plate», «High Temp Plate»,
        «Textured PEI Plate», «Supertack Plate», los de `plate_temperature` en `profile_pack_format.md`); sin placa
        elegida, «Cool Plate» (Bambu Studio 930fe93, paquete de perfiles 12-11-2026);
      - `fins_extrude_safe_temp[i]`: del filamento `i`, en °C (0 a 350; 0 = apagado, el valor si el paquete no lo
        trae): la temperatura segura de la boquilla para extruir las aletas;
      - `overall_chamber_temperature`: la mayor temperatura de cámara de los filamentos (0 sin cámara);
      - `min_vitrification_temperature`, `temperature_vitrification[i]`: la de transición vítrea del filamento si el
        paquete la trae; si no, 0;
      - `hold_chamber_temp_for_flat_print`: no;
      - `additional_fan_full_speed_layer`, `close_additional_fan_first_x_layers`: del filamento si los trae; si no, 0.
    Un índice fuera de una lista de estas no corta: toma el último valor, con aviso.
    Una variable de velocidad o temperatura que el motor no sabe calcular toma un valor seguro (el caudal o la
    temperatura del filamento) en vez de 0, con aviso. Un `F0` dentro de un G-code de la máquina se escribe como `F`
    con la velocidad de viaje del perfil y avisa; nunca rechaza el laminado.

10 quinquies. **El G-code de la máquina sale tal cual** (pedido del dueño, 09-11-2026): fuera de lo que evalúa el
    lenguaje (las construcciones de la regla 1), el motor no cambia ninguna línea, comando, parámetro, comentario ni
    espacio del G-code de la máquina. La única excepción es la de la regla 10 quater: un `F0` como velocidad de un
    movimiento (`G0` a `G3`); en cualquier otro comando (por ejemplo `M862.x`, donde `F` no es una velocidad) se deja
    como está. Un G-code de la máquina que el motor no puede leer no se cambia por el genérico sin avisar: el aviso
    dice qué construcción no entendió.

## Dónde se usa

11. **Qué G-code:** inicio, fin, antes y después de cada cambio de capa, cambio de herramienta, pausa, y los de inicio
    y fin de cada filamento. Se evalúan en ese momento del laminado, con las variables de ese momento (`layer_num`,
    `layer_z` solo en el cambio de capa).
12. **Opciones al mandar a una Bambu Lab** (como en el programa del fabricante): nivelación de la cama, calibración de
    flujo, calibración de vibraciones, timelapse y la placa de la cama. Se eligen en la ventana de envío (por defecto:
    nivelación y calibración de flujo prendidas), van en el pedido de impresión por la red (`printer_connections.md`, «Bambu Lab en red local»). El G-code de inicio de las Bambu no tiene
    variables para nivelar ni calibrar: trae las dos partes siempre y la impresora decide si las corre según lo que
    dice el pedido (marcas que el firmware consulta con `M1002`). Solo la placa entra al G-code, como `curr_bed_type`,
    con estos textos: `Cool Plate`, `Engineering Plate`, `High Temp Plate`, `Textured PEI Plate`, `Supertack Plate` y
    `Textured Cool Plate` (el de la A1 compara con `Textured PEI Plate`). Cambiar la placa lamina de nuevo; cambiar las
    demás opciones no.

## Pruebas

| # | Caso | Resultado esperado |
|---|---|---|
| GM1 | `{if layer_height == 0.2};capa normal{else};otra{endif}` con capa 0,2 | `;capa normal` |
| GM2 | `M104 S{(first_layer_temperature[0]>220 ? 230 : 200)}` con 215 °C | `M104 S200` |
| GM3 | `{digits(3.14159, 6, 2)}` y `{zdigits(3.14159, 6, 2)}` | `  3.14` y `003.14` |
| GM4 | `{if printer_notes=~/.*PRUSA.*/}sí{endif}` con notas «PRINTER_VENDOR_PRUSA3D» | `sí` |
| GM5 | `[temperature_0]` con 210 °C | `210` |
| GM6 | Una variable que no existe | Falla con `gcode_macro`, línea y columna, en castellano; no queda G-code |
| GM7 | Un `{if}` sin `{endif}` | Falla con `gcode_macro` |
| GM8 | Una Bambu Lab A1 del paquete con su G-code de inicio original, «nivelar la cama» prendido y luego apagado | El G-code es el mismo y tiene la parte de nivelación; el pedido de red dice nivelar en el primer caso y no en el segundo. Con placa «Textured PEI Plate» sale la parte propia de esa placa |
| GM10 | Una impresora `cura` con `M82` en su inicio y `relative_extrusion` `false` | Todo el G-code extruye en absoluto |
| GM11 | Un G-code `orcaslicer` con `nozzle_temperature_initial_layer[0]` y un filamento de 220 °C de primera capa | `220` |
| GM12 | G-code de inicio original de la Bambu Lab A1 pegado en el G-code de inicio, filamento PLA | Lamina; ninguna línea con `F0`; las velocidades calculadas de los ajustes |
| GM13 | Cada G-code de máquina del paquete, comparado línea por línea con el texto original | Toda línea sin construcciones sale igual, en el mismo orden; las líneas con construcciones solo cambian en lo evaluado |
| GM14 | El inicio de una Prusa con `M862.1 ... F{(nozzle_high_flow[0] ? 1 : 0)}` sin alto flujo | `F0` queda en `M862.1` |
| GM15 | G-code `orcaslicer` con `{if outer_wall_acceleration > 0}` y la variable como lista de uno o de dos (dos extrusores, el segundo en uso) | Se interpreta: vale el único valor, o el del segundo extrusor; no cae en la versión simple |
| GM16 | `{ceil(2.1)}` y `{ceil(3)}` en un G-code `orcaslicer` | `3` y `3` |
| GM17 | Una condición que abre y cierra en la misma línea, dentro de un texto y con un índice de extrusor: `M104 T1 S{if filament_type[1] == "PLA"}215{else}240{endif}` con PLA | `M104 T1 S215`; no cae en la versión simple |
| GM20 | Prusa XL de 5 cabezales del paquete, un solo filamento | Se interpreta su G-code de inicio entero (`M17`, `M862.3 P "XL"`, `G28 XY`, `G28 Z`, la purga); no cae en la versión simple |
| GM21 | G-code de inicio que no se puede interpretar, versión simple sin `G28` | El G-code tiene `G28` antes de la primera extrusión y el aviso se ve en la interfaz |
| GM22 | Barrido de las 2 267 variantes FDM del paquete con el cubo de 20 mm | Ninguna da el aviso de variable sin llenar, salvo `bed_mesh_probe_count` y `bed_mesh_algo` sin valor en el perfil; las ramas `{if}` coinciden con las del programa original en las impresoras de referencia (MK4S, XL, X1C, K1, Voron 2.4) |
| GM23 | G-code de inicio con `;A {60}`, `;B {2.5}`, `;C {21/2.4053*60}`, `;D {1/3}`, `;E {100.0}`, `;F {2/3*1000000}`, `;G {0.1+0.2}`, `;H {10/4}`, `;I {first_layer_temperature[0]/3}` (200 °C), `;J {nozzle_diameter[0]*2}` (0,4), `;K {1e-7*3}`, `;L {123456789.123}` | `60`, `2.5`, `523.843`, `0`, `100`, `0`, `0.3`, `2`, `66`, `0.8`, `3e-07`, `1.23457e+08` (PrusaSlicer 2.7.2) |
| GM18 | `{\nlocal a = 1;\nif a < 2 then\n  a = 3;\nendif\n"A" + a + "\n";\n}` (con saltos de línea reales) | Sale `A3` y un salto de línea; nada más |
| GM9 | Todas las impresoras del paquete, cada una con sus G-code originales | Todas laminan; las variables que falten quedan en el registro |
