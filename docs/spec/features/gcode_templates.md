# Plantillas de G-code, nombre del archivo y posproceso

**Qué hace:** el usuario escribe trozos de G-code propios (inicio, fin, cambio de capa, filamento…) con variables,
cuentas y condiciones que se resuelven al exportar. También se elige el nombre del archivo con variables y se pueden
pasar scripts sobre el G-code terminado.

**Fase:** después de F6. Hoy el motor reemplaza `[nombre]`, `{nombre}` y `{nombre[i]}` en el inicio y el fin
(`GCODE_OUTPUT.md`); esta ficha es el lenguaje completo.

**Fuentes:**
- documentación de usuario de Prusa: [Macros](https://help.prusa3d.com/article/macros_1775) y
  [lista de variables](https://help.prusa3d.com/article/list-of-placeholders_205643);
- las mediciones de abajo, que fijan lo que esa documentación no muestra.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `machine.start_gcode`, `machine.end_gcode` | texto | `G28` y salto de 5 mm / apagar y `M84` | Inicio y fin de la impresora |
| `material.start_gcode`, `material.end_gcode` | texto | un comentario | Inicio y fin del filamento |
| `gcode.before_layer` | texto | vacío | Antes del movimiento en Z de cada cambio de capa |
| `gcode.after_layer` | texto | vacío | Después del movimiento en Z de cada cambio de capa |
| `gcode.color_change` | texto | `M600` | Cambio de color en una altura |
| `gcode.pause` | texto | `M601` | Pausa en una altura |
| `gcode.custom_template` | texto | vacío | G-code propio en una altura |
| `gcode.auto_temperatures` | — | sí | Escribir solo las temperaturas que el inicio no pone |
| `machine.custom_parameters`, `material.custom_parameters`, `print.custom_parameters` | JSON plano | vacío | Parámetros propios para usar en las plantillas |
| `output.filename_format` | texto | `[input_filename_base].gcode` | Nombre del archivo, con variables |
| `output.post_process` | lista de rutas | vacía | Scripts que se corren sobre el G-code terminado |

El G-code entre objetos está en `sequential.md`; el del cambio de herramienta, en `multi_extruder.md`.

## Reglas

### El lenguaje

1. **Dos formas:**
   - `[nombre]`: la forma vieja; se reemplaza por el valor, sin cuentas;
   - `{…}`: la forma nueva, con expresiones. Las listas (un valor por extrusor) se leen con índice: `temperature[0]`.
2. **Cuentas:** `+ − * / %`. La división de dos enteros da un entero (10 / 4 da 2). Los textos van entre comillas
   dobles y `+` los une.
3. **Lógica:** comparaciones, `and`, `or`, `not`, `true`, `false`; el ternario va entre paréntesis
   (`condición ? sí : no`); `=~ /patrón/` compara con una expresión regular.
4. **Condiciones:** `{if …}…{elsif …}…{else}…{endif}` dentro del texto.
5. **Funciones medidas:**
   - `min`, `max`;
   - la parte entera (`int`): sin redondear, 3,7 da 3
   - `round`: al entero más cercano, las mitades hacia arriba (3,5 da 4; 2,5 da 3);
   - `digits` y `zdigits` con un número, un ancho total y unos decimales: rellena con espacios o con ceros;
   - `is_nil`, `one_of` (si el primero es igual a alguno de los otros), `size` y `empty` (solo de listas);
   - `interpolate_table` con un valor y pares (x, y): interpola en línea recta.
6. **Variables propias:** `local` (vale en esa plantilla) y `global` (vale en las que siguen); pueden ser listas.
7. **Variables de la impresión:** todos los ajustes por su nombre, más, entre otras, `total_layer_count`,
   `input_filename_base`, `num_extruders`, `initial_extruder`, `initial_tool`, `is_extruder_used[i]`,
   `first_layer_print_min[i]`, `first_layer_print_max[i]`, `first_layer_print_size[i]`, `year`, `timestamp`; en el
   cambio de capa, `layer_num` (desde 0), `layer_z` y `max_layer_z`.
8. **Errores:** una variable desconocida, una función mal usada o un error de sintaxis hacen fallar la exportación: no
   se escribe el G-code y el mensaje dice la sección, la línea y una marca en la columna del error. (El motor hoy deja
   igual un nombre desconocido; con esta ficha pasa a ser un error.)

```text
{if temperature[0] > 190}hot{elsif temperature[0] > 100}warm{else}cold{endif}
{(temperature[0] > 190 ? "H" : "C")}
{digits(3.14159, 6, 2)}|{zdigits(3.14159, 6, 2)}
{local a = 5}{a * 2}
{interpolate_table(15, (0, 0), (10, 100), (20, 300))}
```

### Dónde va cada plantilla

9. **Inicio:** las temperaturas automáticas (`gcode_firmware.md`), `machine.start_gcode`, la espera de la boquilla y
   después `material.start_gcode` (prueba T9).
10. **Temperaturas automáticas:** con `gcode.auto_temperatures`, si el inicio de la impresora ya tiene `M104`, `M109`,
    `M140`, `M190`, `M141` o `M191`, no se escribe ninguna temperatura automática; si no tiene, se escriben antes y
    después del inicio. Sin `gcode.auto_temperatures`, nunca se escriben: el inicio del usuario se encarga (prueba
    T10).
11. **Cambio de capa:** `gcode.before_layer` va justo antes del movimiento en Z y `gcode.after_layer` justo después.
    En Tisma Slicer no se escriben en la primera capa (la 0): aparecen desde la capa 1 (prueba T8).
12. **Fin:** `material.end_gcode` y después `machine.end_gcode` (prueba T9).
13. **Alturas:** el cambio de color, la pausa y el G-code propio se ponen a una altura desde la vista previa
    (`preview.md`) y se guardan en el proyecto. Desde la línea de comandos no se pudieron poner (prueba T6).
14. **Parámetros propios:** cada perfil puede traer un JSON de un nivel (`{"clave": valor}`) cuyas claves se usan en
    las plantillas con un prefijo. En la línea de comandos de Tisma Slicer no se resolvieron con los nombres probados
    (prueba T7); el prefijo exacto se toma de la interfaz en `presets.md`.

### Archivo

15. **Nombre:** `output.filename_format` usa el mismo lenguaje. `default_output_extension` incluye el punto
    (`.gcode`). Las variables que solo se conocen al terminar (`print_time`, `total_weight`) se resuelven después de
    laminar: el nombre provisional las deja sin reemplazar y el final las tiene (prueba T11).
16. **Posproceso:** cada script de `output.post_process` se corre con la ruta del G-code como único argumento, y el
    script puede cambiar el archivo (prueba T12). Recibe en el entorno:
    - `SLIC3R_PP_OUTPUT_NAME` (el nombre final) y `SLIC3R_PP_HOST` (`File` al guardar en disco);
    - cada ajuste como `SLIC3R_<NOMBRE EN MAYÚSCULAS>`.
17. **Confirmación:** antes de correr un script que vino en la configuración, Tisma Slicer pregunta; desde la línea de
    comandos espera `Y` o `N`, y con `N` no exporta (prueba T12).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `walls_extra.md` (boquilla 0,4, capas
de 0,2, sin falda ni borde, centro en (100, 100)), temperatura de 200 °C y filamento PLA. **Cubo:** 20 × 20 × 10 mm.

| # | Plantilla | Resultado |
|---|---|---|
| T1 | Inicio con `[temperature] [first_layer_temperature] [layer_height]` y lo mismo con llaves | `200 200 0.2`; `{first_layer_bed_temperature[0]}` da `0` |
| T2 | `{temperature[0] + 5} {layer_height * 2} {10 / 4} {10 % 4} {2 - 7}` | `205 0.4 2 2 -5` |
| T3 | Condición, ternario, lógica, expresión regular sobre las notas de la impresora (`hello TESTNOTE`) | `hot`, `H`, `true false true`, `match` |
| T4 | `min` y `max` de 3 y 5, `round` de 3,5 y de 2,5; `digits`/`zdigits` de 3,14159 con 6 y 2, `digits` de 42 con 3 | `3 5`, `4 3`; `  3.14`, `003.14`, ` 42` |
| T4b | La parte entera (`int`) de 3,7 | `3` |
| T5 | Textos, `local`, `global`, `is_nil`, `one_of`, `size`, `empty`, `interpolate_table` de 15 en (0,0) (10,100) (20,300), lista local `(1, 2, 3)` índice 1 | `abcdef PLA`, `10`, `7`, `false`, `true`, `1 false`, `200`, `2` |
| T6 | `total_layer_count`, `input_filename_base`, `num_extruders`; `first_layer_print_min[0]`, `max[0]`, `size[0]`; `year`, `timestamp`; `initial_extruder`, `initial_tool`, `is_extruder_used[0]` | `50 cube20 1`; `90 110 20`; `2026 20261024-181915`; `0 0 true`. Alturas de cambio de color desde la línea de comandos: ningún `M600` |
| T7 | `empty` de un texto; la variable `print_time` en el inicio; parámetros propios `{"my_key": 5}` usados como `print_my_key`, `custom_parameter_my_key`, `my_key` y otros | Exportación fallida: «Parsing error at line 15: parameter of empty() is not a vector variable», con la línea y una marca; «Not a variable name» para `print_time` y para los parámetros propios |
| T8 | Antes y después del cambio de capa: `{layer_num} {layer_z}`; fin: `{total_layer_count} {layer_num}` | 49 de cada uno en 50 capas; la primera es `;BEFORE 1 0.4` antes de `G1 Z.4` y `;AFTER 1 0.4 0.4` después. Fin: `50 49` |
| T9 | Inicio y fin del filamento | Orden: `M104 S200`, inicio de la impresora, `M109 S200`, `;FILAMENT START PLA 0.2`, capas, `;FILAMENT END`, `;PRINTER END` |
| T10 | Inicio con `M190 S[first_layer_bed_temperature]` y `M109`; sin temperaturas; los dos sin `gcode.auto_temperatures` | Con temperaturas: solo las del usuario. Sin temperaturas: `M190 S60`, `M104 S200`, el inicio, `M109 S200`. Sin el ajuste: ninguna temperatura automática |
| T11 | Nombre `{input_filename_base}_{layer_height}mm_[fill_density]_{filament_type[0]}_{print_time}_{total_weight}.{default_output_extension}` | Provisional `cube20_0.2mm_15%_PLA_{print_time}_{total_weight}..gcode`; final `cube20_0.2mm_15%_PLA_11m_0..gcode` |
| T12 | Script de posproceso | Pregunta «A post-processing script has been detected in the config data … Continue(Y/N) ?». Con `N`: sin archivo. Con `Y`: 1 argumento (la ruta del G-code), `SLIC3R_PP_OUTPUT_NAME` igual a esa ruta, `SLIC3R_PP_HOST=File`, `SLIC3R_LAYER_HEIGHT=0.2`, `SLIC3R_FILL_DENSITY=15%`, 393 variables `SLIC3R_`; la línea que agregó el script queda al final |
