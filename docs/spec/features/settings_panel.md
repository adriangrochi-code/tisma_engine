# Panel de ajustes: secciones, niveles, vista propia, boquilla y cámara

**Qué hace:**
- parte los ajustes en secciones por lo que hacen (Calidad, Resistencia, Velocidad…) y cada sección en apartados
  finos (Altura de capa, Costura, Precisión…), plegables y con un resumen, en lugar de una sola lista larga;
- tres niveles, **Básico**, **Normal** y **Avanzado**, y un modo **Personalizado** donde la persona elige qué ajustes
  se ven;
- arriba del panel, solo la impresora, la cama y la boquilla, cada una con un dibujo; la configuración de la
  impresora, con su calibración, va en una página aparte;
- cambia la boquilla con un clic, debajo de la impresora;
- muestra la cámara de la impresora desde la placa, sin ir a Dispositivos.

**Fase:** P1 (interfaz).

**Pedido del dueño (26-10-2026):**
- «añadir la función que teníamos de la cámara y la boquilla para poder cambiarla de forma feliz y rápida»;
- «que la calidad esté en un tramo y tenga la altura de capa, los settings, esas cosas, para que no quede con tanta
  info»;
- «una interfaz cómoda, simple, pero a su vez muy capaz»;
- «ajustes de avanzado, normal y básico» y «un modo custom en el que podamos elegir qué cosas se ven y qué cosas no».

**Relación con otras fichas:**
- `interface.md`: la ventana, el panel de la placa (tarjeta Printer con Nozzle y Bed type) y las páginas de ajustes.
- `settings_levels.md` (PR #15): cómo reparte los ajustes por nivel Tisma Slicer, medido. Esta ficha fija los nombres
  y el comportamiento del motor: donde las dos difieren, manda esta, que es el pedido más nuevo.
- `printer_connections.md`: la cámara de cada impresora y la cámara USB de la computadora.
- `profile_pack.md` (PR #17): los modelos y sus variantes de boquilla en el paquete de perfiles.

**Diseño:** del dueño. Las reglas marcadas «(diseño)» no existen en Tisma Slicer; las demás siguen lo que hacía.

## Qué muestra hoy el motor

Medido en la interfaz del motor (`dev` en 24cafc8), bajo Xvfb a 1600 × 1000, con un cubo de 20 mm:
- **Preparar:** a la izquierda, el panel «Impresión» con la lista de perfiles y debajo **todos** los ajustes en una
  sola lista sin títulos: altura de capa, primera capa, detalle, alturas por Z, Z anti-aliasing, paredes, costura,
  capas de arriba y de abajo, relleno con todas sus opciones, soportes, falda… Abajo, Cargar…, Guardar… y
  Restablecer.
- **Sin niveles:** se ve todo siempre.
- **Sin boquilla:** no hay lista de boquilla al lado de la impresora.
- **Impresora:** «Impresora conectada» (USB, impresora simulada, Conectar), sin cámara.

Esta ficha cambia el panel; los ajustes y sus valores no cambian.

## Preferencias

Van en `interface.json` (`docs/UI.md`), no en los perfiles: no cambian el G-code.

| Clave | Por defecto | Qué hace |
|---|---|---|
| `ui.level` | `normal` | `basic`, `normal`, `advanced` o `custom` |
| `ui.custom_base` | `normal` | El nivel del que partió la vista propia (regla 15) |
| `ui.custom_visible` | lista vacía | Las claves que se ven en Personalizado, por pestaña (Impresión, Material) y en la configuración de la impresora |
| `ui.section` | `quality` | La sección elegida, por pestaña (regla 3) |
| `ui.sections_open` | el primer apartado de cada sección | Los apartados abiertos, por pestaña (regla 2) |
| `ui.single_column` | no | «Ver todo junto» (regla 4) |
| `ui.camera_window` | — | Posición, tamaño y si estaba abierta la ventana de la cámara (regla 27) |

## Reglas

### Secciones

**Cómo lo hacen otros laminadores** (comparación de caja negra, solo lo que se ve):
- **OrcaSlicer:** los ajustes de impresión van en seis pestañas, Quality, Strength, Speed, Support, Multimaterial y
  Others. Cada una se parte en apartados finos. Por ejemplo, Quality tiene Layer height, Line width, Seam, Precision,
  Ironing, Wall generator, Walls and surfaces, Bridging y Overhangs. Fuente: la wiki oficial,
  `https://github.com/SoftFever/OrcaSlicer/wiki`, consultada el 26-10-2026.
- **UltiMaker Cura:** el panel de ajustes propios es una sola columna de categorías plegables: Quality, Walls,
  Top/Bottom, Infill, Material, Speed, Travel, Cooling, Support, Build Plate Adhesion, Dual Extrusion, Mesh Fixes,
  Special Modes y Experimental. Un ajuste que depende de otro va sangrado debajo de él. La visibilidad se elige entre
  Basic, Advanced, Expert, All o una selección propia. Fuente: las categorías y la lista Basic del repositorio
  oficial, `https://github.com/Ultimaker/Cura`, rama `main`, el 26-10-2026.

El motor toma de los dos:
- de OrcaSlicer, pocas secciones grandes con apartados finos adentro;
- de Cura, los apartados plegables en una sola columna y los ajustes dependientes sangrados.

1. **Dos niveles: sección y apartado.**
   - **Sección:** una parte grande de lo que hace la impresión. Las secciones van en una columna de íconos con nombre
     a la izquierda del panel, o en una fila de chips arriba si el panel tiene menos de 32 em de ancho. Se ve una
     sección a la vez.
   - **Apartado:** un tramo chico dentro de la sección, con pocos ajustes (entre 1 y 8). Cada apartado es una tarjeta
     plegable (regla 2).
   - La sección y el apartado de cada ajuste van en las tablas de abajo. La pestaña donde aparece (Impresión,
     Material o Impresora) la decide el perfil al que pertenece la clave (`docs/PROFILES.md`).
   - Una clave con `*` incluye todo su grupo de `docs/CONFIG.md`, salvo las que otra fila nombra.
   - Las fichas nombradas entre paréntesis agregan sus claves a ese apartado cuando el motor las tenga.

   **Impresión:**

| Sección | Apartado | Claves | Resumen cerrado (ejemplo) |
|---|---|---|---|
| Calidad | Altura de capa | `layers.height`, `layers.first_height` | «0,2 mm · primera 0,2 mm» |
| Calidad | Altura variable | `layers.heights`, `layers.cusp_height` (`slicing.md`) | «Igual en toda la pieza» |
| Calidad | Detalle fino | `layers.detail_*` | «Apagado» |
| Calidad | Ancho de línea | `walls.width`, `walls.outer_width` | «0,4 mm» |
| Calidad | Costura | `walls.seam` (`perimeters.md`) | «Alineada» |
| Calidad | Precisión | `slicing.xy_offset`, `slicing.first_layer_shrink`, `compensation.*` | «Sin correcciones» |
| Calidad | Superficies inclinadas | `zaa.*` (`z_antialiasing.md`) | «Z anti-aliasing apagado» |
| Calidad | Planchado | (`ironing.md`) | «Apagado» |
| Calidad | Puentes y voladizos | (`arc_overhangs.md`, `walls_extra.md`) | «Voladizos en arco apagados» |
| Resistencia | Paredes | `walls.count` (`walls_extra.md`, `staggered_perimeters.md`) | «3 paredes» |
| Resistencia | Arriba y abajo | `surfaces.*` | «3 arriba · 3 abajo» |
| Resistencia | Relleno | `infill.density`, `infill.pattern`, `infill.angle` | «20 % · rectilíneo» |
| Resistencia | Relleno avanzado | `infill.wall_overlap`, `infill.solid_every`, `infill.solid_below_area`, `infill.combine_*`, `infill.dense_*`, `infill.stress_density` | «Sin cambios» |
| Resistencia | Relleno ondulado | `infill.nonplanar*` | «Apagado» |
| Velocidad | Primera capa | `speed.first_layer` | «30 mm/s» |
| Velocidad | Paredes y superficies | `speed.walls`, `speed.outer_wall`, `speed.solid_infill`, `speed.top_solid_infill` | «60 · externa 50 %» |
| Velocidad | Relleno y soporte | `speed.infill`, `speed.support` | «80 · 80 mm/s» |
| Velocidad | Viajes | `speed.travel` | «130 mm/s» |
| Velocidad | Aceleración | (`speeds_accels.md`) | «De la impresora» |
| Velocidad | Topes | `speed.max` | «80 mm/s» |
| Soportes | Soporte | `support.enabled`, `support.automatic`, `support.threshold_angle` | «Sin soportes» o «Automáticos» |
| Soportes | Separación | `support.z_gap`, `support.xy_gap` | «Z 0,2 · XY 0,6 mm» |
| Soportes | Interfaz | `support.interface_layers` (`supports_more.md`) | «3 capas» |
| Soportes | Patrón | `support.spacing` (`supports_more.md`) | «2,5 mm» |
| Soportes | Árbol | (`supports_more.md`) | «—» |
| Adhesión | Falda | `adhesion.skirt_*` | «1 vuelta a 3 mm» |
| Adhesión | Borde | `adhesion.brim_*` | «Sin borde» |
| Adhesión | Balsa | (`skirt_brim_raft.md`) | «Sin balsa» |
| Capas no planares | según `nonplanar.md` y `nonplanar_modes.md` | — | «Planas» |
| Varios materiales | Torre de purga, Goteo, Purga | (`multi_extruder.md`, `multimaterial.md`) | «1 extrusor» |
| Otros | Modos especiales | (`vase_mode.md`, `sequential.md`) | «Normal» |
| Otros | Piel difusa | (`fuzzy_skin.md`) | «Apagada» |
| Otros | Salida G-code | `gcode.*`, `changes` (`gcode_extras.md`) | «Resolución 0,0125 mm» |
| Otros | Corte | el resto de `slicing.*` | «Unión» |

   **Material:**

| Sección | Apartado | Claves | Resumen cerrado (ejemplo) |
|---|---|---|---|
| Temperaturas | Boquilla | `material.nozzle_temp`, `material.first_nozzle_temp` | «215 °C · primera 215 °C» |
| Temperaturas | Cama | `material.bed_temp`, `material.first_bed_temp` | «60 °C» |
| Temperaturas | Cabina | `extrusion.chamber_temperature` | «Sin cabina» |
| Enfriamiento | Ventilador | `cooling.fan_*` | «35–100 % · apagado 1 capa» |
| Enfriamiento | Capas cortas | `cooling.min_layer_time`, `cooling.min_speed`, `cooling.keep_outer_speed` (`cooling_more.md`) | «Mínimo 5 s» |
| Flujo | Flujo | `extrusion.flow`, `material.flow_factor` | «100 %» |
| Flujo | Presión | `extrusion.pressure_advance` (`pressure_advance_gcode.md`) | «Sin avance de presión» |
| Flujo | Flujo máximo | `extrusion.max_flow` | «Sin tope» |
| Retracción | Retracción | `retraction.length`, `retraction.speed` | «2 mm a 40 mm/s» |
| Retracción | Viajes | `retraction.lift`, `retraction.min_travel` (`retraction_travel.md`, `coasting.md`) | «Sin salto en Z» |
| Necesidades | Necesidades | `extrusion.abrasive`, `extrusion.needs_enclosure` | «Sin requisitos» |

   **Impresora** (solo en la configuración aparte, regla 37):

| Sección | Apartado | Claves | Resumen cerrado (ejemplo) |
|---|---|---|---|
| Máquina | Cama y volumen | `machine.bed`, `machine.max_height` | «250 × 210 × 210 mm» |
| Máquina | Cabina | `machine.enclosed`, `machine.max_chamber_temp` | «Abierta» |
| Boquilla y extrusor | Boquilla | `machine.nozzle_diameter`, `machine.hardened_nozzle`, `machine.min_layer_height`, `machine.max_layer_height` | «0,4 mm · latón» |
| Boquilla y extrusor | Extrusor | `machine.filament_diameter`, `machine.max_flow`, `machine.relative_extrusion` | «1,75 mm» |
| Límites | Velocidades | `machine.max_feed_*`, `machine.corner_speed` | «200 mm/s en X e Y» |
| Límites | Aceleraciones | `machine.acceleration`, `machine.travel_acceleration` | «1000 mm/s²» |
| Límites | Temperaturas máximas | `machine.max_nozzle_temp`, `machine.max_bed_temp` | «300 °C · 110 °C» |
| G-code | Firmware | `machine.flavor` (`gcode_firmware.md`) | «Klipper» |
| G-code | Inicio y fin | `machine.start_gcode`, `machine.end_gcode` (`gcode_templates.md`) | «12 líneas · 8 líneas» |
| G-code | Variables | `machine.variables` | «3 variables» |

   - Un grupo que el motor agregue después lleva su sección y su apartado en su ficha. Si no los tiene, va a un
     apartado **Sin ubicar** de la sección Otros, y la prueba SP1 falla hasta que se le asigne uno.
   - Capas no planares, Varios materiales y los apartados de resina solo aparecen con una impresora que los usa.
   - `compute.*` no va en el panel: es una preferencia del programa.
2. **Cada apartado es una tarjeta plegable** con:
   - su nombre y, cerrado, una línea de resumen con los valores más importantes, como en las tablas, calculada con
     los valores actuales;
   - si tiene ajustes cambiados respecto del perfil guardado, «2 cambios» en violeta al lado del nombre;
   - una flecha que la abre o la cierra. Pueden estar varios abiertos; los abiertos se recuerdan
     (`ui.sections_open`).
   La primera vez, en cada sección solo está abierto el primer apartado (Altura de capa en Calidad).
3. **La sección elegida** se recuerda por pestaña. En la columna de secciones, la que tiene ajustes cambiados lleva un
   punto violeta, y al pasar el puntero se ve la lista de sus apartados para ir directo a uno.
4. **Vista de una sola columna** (diseño): un botón arriba del panel, «Ver todo junto», muestra todas las secciones
   una debajo de otra, cada una con su título, como la columna de categorías de Cura. Se recuerda.
5. **Búsqueda:** el buscador de la barra superior (`interface.md`) abre la pestaña, la sección y el apartado del
   ajuste, lo lleva a la vista y lo resalta 2 s. Si el ajuste está oculto por el nivel, se muestra igual, marcado
   (regla 12).
6. **Ajustes que dependen de otro** van sangrados debajo de él y se ven solo cuando sirven:

| Interruptor | Lo que se oculta apagado |
|---|---|
| `support.enabled` | El resto de la sección Soportes |
| `zaa.enabled` | El resto de `zaa.*` |
| `infill.nonplanar` | `infill.nonplanar_amplitude`, `infill.nonplanar_wavelength` |
| `infill.dense_under_top` | `infill.dense_density` |
| `infill.combine_auto` encendido | `infill.combine_layers` (lo calcula el motor) |

7. **Un apartado o una sección sin ajustes** en el nivel elegido no se muestra.
8. **Restablecer** (diseño): cada apartado tiene, en su menú de tres puntos, «Restablecer», que vuelve sus ajustes al
   perfil guardado. El botón Restablecer del pie sigue haciendo lo mismo con todo el perfil.

### Niveles

9. **Cuatro opciones** en el chip de nivel de la barra superior, cada una con su explicación:

| Nivel | Texto en inglés | Explicación | Punto |
|---|---|---|---|
| Básico | Basic | «Lo que casi toda impresión necesita» | verde |
| Normal | Normal | «Más ajustes para afinar» | amarillo |
| Avanzado | Advanced | «Todos los ajustes» | rojo |
| Personalizado | Custom | «Los ajustes que elegís vos» | violeta |

   Estos nombres reemplazan a los de Tisma Slicer: su Simple es Básico, su Advanced es Normal y su Expert es Avanzado.
   Las tablas de `settings_levels.md` valen con esa equivalencia.
10. **Cada ajuste tiene un nivel** (`basic`, `normal` o `advanced`) en la tabla de ajustes del motor. Un nivel muestra
    los suyos y los de los niveles de abajo. Un ajuste sin nivel es `advanced`.
    - **Básico** (lista completa):

| Sección › apartado | Claves |
|---|---|
| Calidad › Altura de capa | `layers.height`, `layers.first_height` |
| Calidad › Costura | `walls.seam` |
| Resistencia › Paredes | `walls.count` |
| Resistencia › Arriba y abajo | `surfaces.top_layers`, `surfaces.bottom_layers`, `surfaces.top_min_thickness`, `surfaces.bottom_min_thickness`, `surfaces.top_pattern` |
| Resistencia › Relleno | `infill.density`, `infill.pattern` |
| Soportes › Soporte | `support.enabled`, `support.automatic` |
| Adhesión › Falda y Borde | `adhesion.skirt_loops`, `adhesion.skirt_distance`, `adhesion.brim_width` |
| Temperaturas › Boquilla y Cama | `material.nozzle_temp`, `material.first_nozzle_temp`, `material.bed_temp`, `material.first_bed_temp` |
| Enfriamiento › Ventilador | `cooling.fan_min`, `cooling.fan_max` |
| Retracción › Retracción y Viajes | `retraction.length`, `retraction.lift` |

    - **Solo en Avanzado:** `layers.detail_*`, `layers.cusp_height`, `zaa.min_thickness`, `zaa.raise`,
      `infill.wall_overlap`, `infill.solid_every`, `infill.solid_below_area`, `infill.combine_*`,
      `infill.stress_density`, `cooling.keep_outer_speed`, `extrusion.pressure_advance`, `compensation.*`,
      `slicing.*`, `gcode.*`, `changes`, `machine.variables`, y los que `settings_levels.md` (regla 5) pone solo en
      Expert.
    - **Normal:** todo lo demás.
    - Comparación: la lista Basic de Cura (fuente de la regla 1) trae lo mismo en espíritu (altura de capa, paredes,
      capas de arriba y abajo, densidad y patrón de relleno, temperaturas, retracción, ventilador, soporte y
      adhesión), más la velocidad de impresión, que el motor deja en Normal.
11. **Básico avisa lo que oculta:** al pie de cada apartado con ajustes ocultos, «3 ajustes más en Normal y
    Avanzado» y el botón «Mostrar todos», que muestra el apartado entero hasta que se cierre o se cambie de sección.
    El nivel elegido no cambia. Una sección entera oculta en Básico (por ejemplo, Velocidad) no tiene botón: se ve
    al cambiar de nivel.
12. **Nada cambiado queda oculto:** un ajuste distinto del perfil guardado, o distinto del valor por defecto en un
    perfil del usuario, se muestra en cualquier nivel, con la etiqueta gris «Avanzado» (o «Normal») y la flecha para
    volverlo atrás. Así un nivel más bajo nunca esconde un cambio.
13. **El nivel no cambia el laminado:** el G-code es el mismo en los cuatro niveles con los mismos perfiles.
14. **Atajos:** Ctrl+Alt+1, 2, 3 y 4 eligen Básico, Normal, Avanzado y Personalizado (diseño); se agregan a la lista
    de atajos de Más.

### Personalizado

15. **Al elegirlo por primera vez** se ven los mismos ajustes que en el nivel que estaba elegido; ese nivel se guarda
    en `ui.custom_base`.
16. **Elegir qué se ve,** de tres formas:
    - **«Elegir ajustes…»**, en el chip de nivel y arriba del panel en Personalizado. Abre un diálogo con un buscador
      y el árbol pestaña › sección › apartado › ajuste, cada uno con su casilla (la de una sección o un apartado
      marca o desmarca todo lo de adentro; a medias se ve con una raya). Arriba, «Partir de: Básico, Normal,
      Avanzado», «Todos» y «Ninguno». Abajo, Cancelar y Aceptar. Cada ajuste muestra su nivel en gris.
    - **Ocultar desde el panel:** en Personalizado, al pasar el puntero sobre un ajuste aparece un ojo tachado que lo
      quita de la vista.
    - **Agregar desde el panel:** al pie de cada apartado, «+ Agregar ajuste» lista los ocultos de ese apartado.
17. **Se guarda** en `interface.json` (`ui.custom_visible`) y vuelve al abrir el programa. Más › Configuration suma
    «Exportar vista…» e «Importar vista…» (un JSON con `format` `tisma-ui-view`, `version` 1 y la lista de claves) para
    llevarla a otra computadora o compartirla.
18. **Claves que no existen** en la lista (de otra versión) se ignoran sin aviso. Un ajuste nuevo de una versión
    nueva se ve si su nivel no pasa `ui.custom_base`.
19. **Las reglas 6, 7 y 12 siguen valiendo:** un ajuste dependiente se oculta con su interruptor apagado aunque esté
    marcado, un apartado o una sección sin ajustes visibles no aparece, y un ajuste cambiado se ve aunque esté
    desmarcado.
20. **Ajustes rápidos** (Quick settings, `interface.md`) es otra lista y no cambia con el nivel ni con la vista propia.

### Boquilla rápida

21. **Dónde:** en la tarjeta de la impresora (regla 32), la lista de boquilla debajo de la impresora. Muestra las
    boquillas del modelo elegido: las variantes del modelo en el paquete (`profile_pack.md`) y las impresoras del
    usuario hechas de ese modelo. Cada fila, su dibujo (regla 36), el diámetro y, si el perfil lo dice, el tipo:
    «0,4 mm», «0,6 mm · acero endurecido» (`machine.hardened_nozzle`). La elegida, con una marca.
22. **Cambiar es un clic** y no abre diálogos, salvo la regla 24:
    - la impresora pasa a la variante de esa boquilla;
    - el perfil de impresión se mantiene si es compatible con la variante nueva; si no, se elige el que el paquete
      marca por defecto para esa variante;
    - el filamento se mantiene si es compatible; si no, el primero compatible del mismo material (`material.type`) o,
      si no hay, el que el paquete marca por defecto;
    - una notificación dice qué cambió, por ejemplo «Boquilla de 0,6 mm: perfil de impresión 0.30 mm», con
      **Deshacer**, que vuelve a la boquilla y a los perfiles de antes;
    - con «Slice automatically», lamina de nuevo.
23. **Modelo con una sola boquilla:** la lista muestra el diámetro y la opción «Otra…», que pide un diámetro y crea una
    impresora del usuario a partir del modelo con ese `machine.nozzle_diameter`, sin guardar (con su asterisco).
24. **Cambios sin guardar** en el perfil de la impresora: el mismo diálogo que al cambiar de perfil (`presets.md`,
    PR #13): llevar los cambios a la boquilla nueva, descartarlos o cancelar.
25. **Varios extrusores:** una lista por extrusor, como en `multi_extruder.md`.
26. **La boquilla de la impresora conectada** (diseño): si la conexión informa el diámetro de la boquilla instalada
    (solo las conexiones de `printer_connections.md` que lo dan) y no coincide con la elegida, la tarjeta Printer
    muestra «La impresora tiene una boquilla de 0,6 mm» con el botón «Usar 0,6 mm», que hace la regla 22. Sin ese
    dato no se muestra nada.

### Cámara rápida

27. **Botón de cámara** (diseño): en la tarjeta de la impresora, al lado del engranaje, cuando la impresora elegida
    tiene una cámara (la suya o una USB de la computadora, `printer_connections.md`, «Cámara USB de la computadora»).
    Sin cámara no aparece.
    - Abre una **ventana flotante** sobre la vista 3D, con la imagen, el nombre de la impresora, el estado de la
      impresión (porcentaje, capa y tiempo que falta, si la conexión los da) y botones para pantalla completa y cerrar.
    - Se mueve y se agranda con el mouse, sin salir de la vista 3D; posición, tamaño y si estaba abierta se recuerdan
      (`ui.camera_window`).
    - En modo compacto (panel escondido) el botón va en la barra flotante, al lado de «<<».
28. **Cambiar de cámara** desde la ventana: una lista con «La cámara de la impresora» (si tiene), «Sin cámara» y las
    cámaras USB conectadas. Es la misma elección que en Dispositivos y se guarda igual, por impresora.
29. **Dispositivos:** la página de la impresora conectada muestra su cámara como dice `interface.md` («Devices») y
    `printer_connections.md`. Hoy el motor no la tiene (sección «Qué muestra hoy el motor»).
30. **Sin imágenes:** si la cámara se desconecta o pasa 10 s sin mandar imágenes, la ventana lo dice sobre la última
    imagen, en gris, y reintenta sola cada 5 s.
31. **Las imágenes** quedan solo en memoria: no se guardan en disco ni salen de la computadora.

### La impresora en el panel y su configuración aparte

Pedido del dueño (26-10-2026): en el panel de ajustes la impresora tiene que ser solo la elección de la impresora,
con la cama a la derecha y la boquilla abajo, cada una con un dibujo de lo que se tiene; la configuración de la
impresora va en un menú aparte, más detallado, con todo lo de la calibración, para que el panel no quede recargado.

32. **Tarjeta de la impresora,** arriba de todo en el panel de ajustes, antes del material y de las secciones de la
    impresión. No tiene ajustes de la impresora, solo tres listas con dibujo:

| Lugar | Lista | Dibujo | Texto |
|---|---|---|---|
| Arriba a la izquierda, lo más ancho | Impresora | La silueta del tipo de impresora (regla 34) | Marca y modelo («Bambu Lab A1 mini», «Creality CR-5 Pro») |
| Arriba a la derecha | Cama | La superficie de la cama (regla 35) | El nombre del fabricante («Textured PEI Plate») |
| Abajo, a todo el ancho | Boquilla | La boquilla (regla 36) | «0,4 mm» y, si se sabe, «acero endurecido» |

    - La lista de cama solo aparece en impresoras con camas intercambiables (`profiles.md`, regla 3). Sin ella, la
      impresora usa todo el ancho.
    - Con varios extrusores, una fila de boquilla por extrusor (regla 25).
    - A la derecha de la impresora, el engranaje abre la configuración de la impresora (regla 37) y, si tiene, el
      botón de cámara (regla 27).
    - La pestaña Impresora del panel deja de existir: el panel tiene Impresión y Material.
    - El dibujo mide 3 em de alto en la tarjeta y 2 em en las listas desplegadas, en violeta de una sola tinta sobre
      el fondo violeta suave (`interface.md`, «Aspecto»), en tema claro y oscuro.
33. **La lista de impresoras:** un buscador arriba, las impresoras agrupadas por marca, cada fila con su dibujo, y al
    final «Agregar impresora…», que abre el asistente de configuración (`interface.md`). Las impresoras del usuario
    van primero, con su nombre.
34. **Dibujo de la impresora:** propio del motor, uno por tipo, elegido con datos del perfil:

| Tipo | Cuándo |
|---|---|
| Resina | Impresora de resina (`resin.md`) |
| Pellets | Impresora de pellets (`pellets.md`, PR #18) |
| Cinta | Impresora de cinta (`belt_printer.md`) |
| Delta | Cama redonda (`machine.bed` con forma de círculo) |
| Cerrada | `machine.enclosed` sí |
| Abierta | El resto |

    - No se usan fotos de los fabricantes: no son del proyecto (`interface.md`). Si el paquete de perfiles trae algún
      día una imagen del modelo con una licencia que permita repartirla, `profile_pack.md` dirá cómo, y reemplaza al
      dibujo.
35. **Dibujo de la cama:** uno por familia, según el nombre del tipo de cama, sin importar mayúsculas:

| Familia | Nombre que contiene | Dibujo |
|---|---|---|
| Texturada | «textur», «satin» | Placa con puntos |
| Lisa | «smooth», «PEI» sin «textur» | Placa lisa con brillo suave |
| Vidrio | «glass», «vidrio» | Placa con un reflejo diagonal |
| Fría | «cool» | Placa con un copo |
| Ingeniería y alta temperatura | «engineering», «high temp» | Placa con una llama chica |
| Otra | Ninguno | Placa lisa |

36. **Dibujo de la boquilla:** la punta con el diámetro escrito al lado.
    - Latón: dibujo lleno; acero endurecido (`machine.hardened_nozzle`): dibujo con rayas;
    - alto flujo (el nombre de la variante contiene «high flow» o «HF»): el cuerpo más largo;
    - el tamaño de la punta crece con el diámetro, de 0,2 a 1,2 mm, para que se distingan a simple vista.
37. **Configuración de la impresora, aparte:** el engranaje de la tarjeta, o la sección Printer de la barra superior
    (`interface.md`), abre una página a todo el ancho con la lista de secciones a la izquierda y los apartados
    plegables (reglas 2 a 8):

| Sección | Apartados |
|---|---|
| Máquina | Cama y volumen, Cabina, las camas que tiene (con su dibujo y su nombre) |
| Boquilla y extrusor | Boquilla, Extrusor, las boquillas que tiene (con su dibujo) |
| Límites | Velocidades, Aceleraciones, Temperaturas máximas |
| G-code | Firmware, Inicio y fin, Variables |
| Calibración | Estado de cada calibración (regla 38) |
| Conexión | La conexión y la cámara de la impresora (`printer_connections.md`) |
| Notas | Texto libre de la persona |

    Las claves de cada apartado son las de la tabla «Impresora» de la regla 1.
38. **Sección Calibración:** una fila por cada prueba de `calibrations.md` que sirve a esa impresora (regla 5 de esa
    ficha), con:
    - el ajuste que fija y su valor actual en el perfil (por ejemplo, flujo 98 %, retracción 0,8 mm);
    - el estado: «Sin calibrar» (gris), «Calibrado el 26-10-2026» (verde), o «Calibrado con otra boquilla» (amarillo)
      si la boquilla cambió desde entonces;
    - el botón **Calibrar**, que abre esa prueba con la impresora, la boquilla y el filamento elegidos;
    - las que dependen del filamento (temperatura, flujo, avance de presión, retracción) dicen con qué filamento se
      hicieron.
    La fecha y la boquilla se guardan cuando se aplica un resultado desde «Apply a calibration result…» (`interface.md`,
    Más › Calibration). Arriba de la sección, el resumen «5 de 9 calibradas» y un botón «Calibrar en orden», que abre
    la primera sin calibrar en el orden del tutorial `new_material` (`assistant.md`, PR #17).
39. **Volver a la placa:** la página tiene arriba «‹ Placa»; Escape también vuelve. Los cambios sin guardar quedan con
    su asterisco, como en `presets.md` (PR #13).
40. **El nivel también vale aquí:** la configuración de la impresora usa el nivel o la vista propia de las reglas 9 a
    20, con la pestaña Impresora en el diálogo de la regla 16.

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| SP1 | Recorrer todas las claves de `docs/CONFIG.md` | Cada una tiene sección, apartado y nivel; ninguna cae en Sin ubicar; ningún apartado tiene más de 8 ajustes |
| SP2 | Pestaña Impresión, primera vez | Sección Calidad elegida, con Altura de capa abierta y los demás apartados cerrados con su resumen. En Resistencia, Relleno cerrado dice «20 % · rectilíneo» |
| SP3 | Cambiar `infill.density` a 40 y cerrar Relleno | El resumen dice «40 % · rectilíneo» y «1 cambio»; Resistencia tiene el punto en la columna de secciones |
| SP4 | Abrir Costura y Precisión, elegir la sección Velocidad, cerrar el programa y volver a abrirlo | Se abre en Velocidad; en Calidad siguen abiertos Altura de capa, Costura y Precisión |
| SP5 | `support.enabled` apagado | Soportes muestra solo el interruptor; al encenderlo aparecen los demás apartados, y los dependientes sangrados |
| SP6 | Buscar «costura» | Se elige Calidad, se abre Costura y se ve `walls.seam` resaltado |
| SP6b | «Ver todo junto» | Todas las secciones una debajo de otra con su título; los apartados mantienen su estado |
| SP7 | Nivel Básico, pestaña Impresión | Solo las claves de la tabla de la regla 10; al pie de Relleno, «N ajustes más…»; la sección Velocidad no aparece |
| SP8 | En Avanzado, `infill.solid_every` a 5; pasar a Básico | Relleno muestra `infill.solid_every` con la etiqueta «Avanzado» y la flecha atrás |
| SP9 | Laminar el mismo cubo en los cuatro niveles | Los cuatro G-code son iguales byte a byte |
| SP10 | Básico › Personalizado | Se ven los mismos ajustes que en Básico; `ui.custom_base` es `basic` |
| SP11 | «Elegir ajustes…», desmarcar la sección Velocidad y marcar `infill.wall_overlap` | Velocidad desaparece; Relleno muestra `infill.wall_overlap`; al reabrir el programa sigue igual |
| SP12 | Ojo tachado sobre `walls.seam` en Personalizado | Desaparece; «+ Agregar ajuste» de Paredes lo ofrece |
| SP13 | Exportar la vista e importarla en otra carpeta de datos | Los mismos ajustes visibles |
| SP14 | Importar una vista con una clave que no existe | Se ignora; el resto se aplica |
| SP15 | Modelo con variantes 0,4 y 0,6; perfil de impresión solo compatible con 0,4; elegir 0,6 | La impresora pasa a la variante 0,6, el perfil de impresión al de 0,6 por defecto; la notificación lo dice; Deshacer vuelve todo |
| SP16 | El mismo cambio con el perfil de impresora modificado | El diálogo de cambios sin guardar; Cancelar deja la boquilla de 0,4 |
| SP17 | Modelo con una sola boquilla, «Otra…» 0,8 | Impresora del usuario sin guardar con `machine.nozzle_diameter` 0,8 |
| SP18 | Impresora sin cámara | No hay botón de cámara |
| SP19 | Cámara USB simulada (la de la prueba CA1 de `printer_connections.md`) asignada a la impresora | Botón de cámara; la ventana muestra la imagen; moverla, cerrar el programa y abrirlo: vuelve en el mismo lugar |
| SP20 | Desconectar la cámara simulada | A los 10 s, el aviso sobre la última imagen; al volver, la imagen sigue sola |
| SP21 | Cambiar a «Sin cámara» desde la ventana | La ventana se cierra y el botón desaparece; Dispositivos muestra «Sin cámara» |
| SP22 | Panel de ajustes con una Bambu Lab A1 mini | Arriba, la tarjeta con la impresora y su dibujo, la cama a la derecha y la boquilla abajo; no hay pestaña Impresora ni ajustes de la impresora en el panel |
| SP23 | Impresora sin camas intercambiables | No hay lista de cama; la impresora ocupa todo el ancho |
| SP24 | Lista de impresoras, buscar «CR-5» | Se ven los modelos que coinciden, con su dibujo, agrupados por marca |
| SP25 | Dibujos según el perfil: resina, cama redonda, cerrada, abierta | Resina, Delta, Cerrada y Abierta |
| SP26 | Camas «Textured PEI Plate», «Cool Plate» y «Glass» | Placa con puntos, con copo y con reflejo |
| SP27 | Boquillas 0,2 de latón y 0,6 de acero endurecido | La de 0,6 más grande y rayada, con su diámetro escrito |
| SP28 | Engranaje de la tarjeta | Se abre la configuración aparte con Máquina, Boquilla y extrusor, Límites, G-code, Calibración, Conexión y Notas; Escape vuelve a la placa |
| SP29 | Aplicar un resultado de flujo y abrir Calibración | Flujo «Calibrado el» con la fecha, la boquilla y el filamento; las demás «Sin calibrar»; arriba «1 de N calibradas» |
| SP30 | Después de SP29, cambiar a la boquilla de 0,6 | Flujo pasa a «Calibrado con otra boquilla» |
