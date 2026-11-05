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
   decimales; `round`, que redondea a entero; `digits` con un valor, una cantidad de caracteres y, opcional, de
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
10 ter. **Extrusión:** con el G-code del bloque `gcode`, el motor extruye en relativo o absoluto según
    `relative_extrusion` de ese bloque.

## Dónde se usa

11. **Qué G-code:** inicio, fin, antes y después de cada cambio de capa, cambio de herramienta, pausa, y los de inicio
    y fin de cada filamento. Se evalúan en ese momento del laminado, con las variables de ese momento (`layer_num`,
    `layer_z` solo en el cambio de capa).
12. **Opciones al mandar a una Bambu Lab** (como en el programa del fabricante): nivelación de la cama, calibración de
    flujo, calibración de vibraciones, timelapse y la placa de la cama. Se eligen en la ventana de envío (por defecto:
    nivelación y calibración de flujo prendidas), van como variables al G-code de inicio y en el pedido de impresión
    por la red (`printer_connections.md`, «Bambu Lab en red local»). El G-code se lamina de nuevo si cambian, porque
    cambian el inicio.

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
| GM8 | Una Bambu Lab A1 del paquete con su G-code de inicio original y «nivelar la cama» prendido | El G-code tiene la parte de nivelación y de calibración; con la opción apagada, no |
| GM10 | Una impresora `cura` con `M82` en su inicio y `relative_extrusion` `false` | Todo el G-code extruye en absoluto |
| GM11 | Un G-code `orcaslicer` con `nozzle_temperature_initial_layer[0]` y un filamento de 220 °C de primera capa | `220` |
| GM9 | Todas las impresoras del paquete, cada una con sus G-code originales | Todas laminan; las variables que falten quedan en el registro |
