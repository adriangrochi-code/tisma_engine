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
9. **La lista:** `docs/FORMAT.md` del paquete de perfiles trae, en una tabla, cada variable que usan los G-code del
   paquete, su tipo (número, texto, sí/no, vector), en qué G-code está disponible (inicio, fin, cambio de capa, cambio
   de herramienta, pausa) y su significado. El motor llena todas las de esa tabla; una variable que el motor todavía
   no sabe llenar se avisa en el registro con su nombre y se marca «sin probar» en el panel de la impresora. La lista
   está en `gcode_variables.md`.
10. **Variables de las impresoras Bambu Lab:** además de las de temperatura y de la cama, sus G-code de inicio usan el
    tipo de placa elegido, si se nivela la cama, si se calibra el flujo y si se hace la calibración de vibraciones;
    esos valores salen de las opciones de impresión (regla 12).

10 bis. **Dialectos** (campo `dialect` del bloque `gcode` del paquete, `profile_pack_format.md`): `prusaslicer` es el
    lenguaje de arriba con los nombres de la documentación pública de PrusaSlicer; `orcaslicer`, `crealityprint` y
    `elegooslicer` usan el mismo lenguaje con sus propios nombres de variables, que el motor llena con el valor
    equivalente; `cura` solo reemplaza `{nombre}` por su valor, sin condiciones ni cuentas. Si el G-code de un
    dialecto no se puede interpretar, se usa la versión simple del perfil y se avisa en el registro.
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
      - `nozzle_diameter_at_nozzle_id`: el diámetro de la boquilla;
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
| GM9 | Todas las impresoras del paquete, cada una con sus G-code originales | Todas laminan; las variables que falten quedan en el registro |
