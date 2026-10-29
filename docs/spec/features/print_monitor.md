# Seguir la impresión

**Qué hace:** una sección «Impresión» en el programa de escritorio, igual a la del modo táctil y a la de la app del
teléfono (`accessibility_touch.md`, regla 15; `mobile_app.md`). Al mandar a imprimir, muestra qué está haciendo la
impresora, sobre la vista previa del mismo G-code, con la cámara, el orden de las piezas, los ajustes que se pueden
cambiar en marcha y el mapa de nivelado de la cama.

**Pedido del dueño (29-10-2026):** «la sección de impresión tendría que ser igual en el slicer… cuando mandás
imprimir, poder ver qué está haciendo… cómo tiene orden… inclusive el nivelado de la cama».

**Fase:** P2, con `devices.md` y `printer_connections.md`.

**Fuentes:**
- Klipper por Moonraker, objetos de impresora (`https://moonraker.readthedocs.io/en/latest/printer_objects/`):
  - `virtual_sdcard`: `progress` (0 a 1), `file_position` y `file_size` (bytes), `is_active`;
  - `print_stats`: `state`, `print_duration`, `filament_used`, `info.current_layer` e `info.total_layer` (pueden
    venir vacíos);
  - `gcode_move`: `speed_factor` y `extrude_factor` (la tabla de la documentación dice `extruder_factor` y el ejemplo
    `extrude_factor`: se mira cuál manda la versión instalada);
  - `bed_mesh`: `profile_name`, `mesh_min`, `mesh_max`, `probed_matrix` (lo medido) y `mesh_matrix` (lo interpolado);
- Marlin: `M420 V` imprime la malla guardada, con `T0` legible, `T1` CSV
  (`https://marlinfw.org/docs/gcode/M420.html`); por USB el programa manda cada línea (`printer_connections.md`), así
  que sabe exactamente por cuál va;
- Bambu Lab por LAN: el estado trae `layer_num`, `total_layer_num`, `mc_percent`, `mc_remaining_time`, `spd_lvl`
  (OpenBambuAPI, comunidad). No se encontró cómo leer la malla de la cama;
- OctoPrint: el avance del trabajo por su API de trabajos (a confirmar en `https://docs.octoprint.org/` antes de
  escribir la regla de OctoPrint); la malla, solo con complementos de terceros, que no se usan.

## Reglas

1. **Dónde:** una sección «Impresión» en la barra de arriba del programa de escritorio (y abajo en modo táctil). Al
   mandar a imprimir, el programa pasa a esa sección solo.
2. **Qué se ve:**
   - la vista previa del G-code que se mandó, con lo ya impreso en color y lo que falta en gris claro, y la capa
     actual marcada; la boquilla dibujada donde está;
   - la cámara al lado (o arriba en una ventana chica), que se agranda con un clic;
   - estado, capa, porcentaje, tiempo que pasó y que falta, temperaturas con su objetivo, ventilador, filamento usado.
3. **Por dónde va:** según la conexión:
   - USB: la línea que se acaba de mandar;
   - Klipper: `file_position` sobre el archivo que se subió;
   - Bambu Lab: `layer_num` (la capa entera, sin el punto exacto);
   - el resto: el porcentaje.
   La vista previa usa el mismo G-code que se mandó, guardado con el trabajo; si la impresión empezó desde otro
   programa, se muestra solo el estado y la cámara.
4. **El orden:** una lista con las piezas en el orden en que se imprimen (las etiquetas de objeto del G-code,
   `gcode_firmware.md`), con la que se está imprimiendo marcada. Con impresión secuencial, el orden de las piezas;
   con varias por capa, el orden dentro de la capa. Desde la lista se puede saltar una pieza que falló, si la
   impresora lo permite (cancelar objeto de Klipper, `M486` de Marlin, saltar objetos de Bambu Lab).
5. **Ajustes en marcha:** velocidad (%), caudal (%), temperatura de boquilla y de cama, ventilador. Se mandan con
   los comandos de cada conexión (Marlin y Klipper: `M220`, `M221`, `M104`, `M140`, `M106`; Bambu Lab: el nivel de
   velocidad). Cada cambio queda anotado en el trabajo, con la capa en que se hizo. Al terminar se ofrece pasarlos al
   perfil: «Imprimiste con caudal 95 % desde la capa 3. ¿Guardarlo en el filamento?».
6. **Nivelado de la cama:** un botón «Malla de la cama» muestra el mapa:
   - en 3D (la cama deformada, exagerada en Z con una escala que se ve) y como mapa de colores con los valores;
   - lo medido y lo interpolado, si la conexión da los dos;
   - el rango (máximo − mínimo) y la inclinación, con un aviso si el rango pasa de un valor (por ejemplo 0,3 mm,
     a calibrar) y la sugerencia de nivelar;
   - colores de `accessibility_touch.md` (regla 2), con los números siempre visibles;
   - Klipper: `bed_mesh`; Marlin: `M420 V T1` (si el firmware no tiene malla, lo dice); otras: «Esta impresora no
     informa su malla».
   - Se puede pedir una medición nueva solo con la impresora quieta (Klipper `BED_MESH_CALIBRATE`, Marlin `G29`), con
     confirmación.
7. **Varias impresoras:** arriba de la sección, una tira con cada impresora y su estado; un clic cambia.
8. **Al terminar:** aviso en pantalla (y sonido si `ui.sounds`), una foto de la cámara y el resumen: tiempo real
   contra el estimado, filamento, cambios hechos en marcha.

## Pruebas

| # | Caso | Resultado |
|---|---|---|
| PM1 | Simulador Marlin por USB, mandar un cubo | Pasa sola a Impresión; la vista previa avanza línea por línea con lo mandado |
| PM2 | Simulador Klipper, `file_position` a la mitad del archivo | La vista previa marca la capa de esa posición |
| PM3 | Klipper, `bed_mesh` con una malla de 5 × 5 | Mapa 3D y de colores con los 25 valores, rango y aviso si corresponde |
| PM4 | Marlin, respuesta guardada de `M420 V T1` | El mismo mapa; sin malla, el aviso |
| PM5 | Cambiar el caudal a 95 % en la capa 3 | Se manda `M221 S95`; al terminar se ofrece guardarlo en el filamento |
| PM6 | Cuatro piezas, cancelar la segunda desde la lista | Se manda el comando de cancelar objeto; la pieza queda tachada en la lista y en la vista previa |
| PM7 | Bambu Lab (simulador) | Capa y porcentaje desde el estado; «Esta impresora no informa su malla» |
