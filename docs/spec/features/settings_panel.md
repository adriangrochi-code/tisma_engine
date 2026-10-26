# Panel de ajustes: secciones, niveles, vista propia, boquilla y cámara

**Qué hace:**
- parte los ajustes en secciones por lo que hacen (Calidad, Paredes, Relleno…), cada una plegable y con un resumen,
  en lugar de una sola lista larga;
- tres niveles, **Básico**, **Normal** y **Avanzado**, y un modo **Personalizado** donde la persona elige qué ajustes
  se ven;
- cambia la boquilla con un clic, al lado de la impresora;
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
| `ui.custom_visible` | lista vacía | Las claves que se ven en Personalizado, por pestaña (Impresión, Material, Impresora) |
| `ui.sections_open` | `["quality"]` | Las secciones abiertas, por pestaña |
| `ui.camera_window` | — | Posición, tamaño y si estaba abierta la ventana de la cámara (regla 27) |

## Reglas

### Secciones

1. **Secciones por lo que hacen.** Cada pestaña del panel (Impresión, Material, Impresora) se parte en secciones, en
   este orden. La sección de un ajuste sale de su grupo en `docs/CONFIG.md`; las excepciones van en la columna
   «Claves». La pestaña donde aparece la decide el perfil al que pertenece la clave (`docs/PROFILES.md`).

| Pestaña | Sección (`id`) | Claves | Resumen cerrada (ejemplo) |
|---|---|---|---|
| Impresión | Calidad (`quality`) | `layers.*`, `zaa.*` | «0,2 mm · primera capa 0,2 mm» |
| Impresión | Paredes (`walls`) | `walls.*` | «3 paredes · costura alineada» |
| Impresión | Arriba y abajo (`surfaces`) | `surfaces.*` | «3 capas arriba · 3 abajo» |
| Impresión | Relleno (`infill`) | `infill.*` | «20 % · rectilíneo» |
| Impresión | Soportes (`support`) | `support.*` | «Sin soportes» o «Automáticos» |
| Impresión | Adhesión (`adhesion`) | `adhesion.*` | «Falda de 1 vuelta · sin borde» |
| Impresión | Velocidad (`speed`) | `speed.*` | «Paredes 60 · relleno 80 mm/s» |
| Impresión | Medidas (`compensation`) | `compensation.*`, `slicing.xy_offset`, `slicing.first_layer_shrink` | «Sin correcciones» |
| Impresión | Salida (`output`) | `gcode.*`, `changes`, el resto de `slicing.*` | «Resolución 0,0125 mm» |
| Material | Temperaturas (`temperatures`) | `material.*` salvo `material.flow_factor`, y las temperaturas de `machine.variables` | «215 °C · cama 60 °C» |
| Material | Enfriamiento (`cooling`) | `cooling.*` | «Ventilador 35–100 % · capa mínima 5 s» |
| Material | Flujo (`flow`) | `extrusion.flow`, `extrusion.pressure_advance`, `extrusion.max_flow`, `material.flow_factor` | «100 % · sin avance de presión» |
| Material | Retracción (`retraction`) | `retraction.*` | «2 mm a 40 mm/s» |
| Material | Necesidades (`needs`) | `extrusion.abrasive`, `extrusion.needs_enclosure`, `extrusion.chamber_temperature` | «Sin requisitos» |
| Impresora | Máquina (`machine`) | `machine.bed`, `machine.max_height`, `machine.enclosed` | «250 × 210 × 210 mm · abierta» |
| Impresora | Boquilla y extrusor (`nozzle`) | `machine.nozzle_diameter`, `machine.filament_diameter`, `machine.hardened_nozzle`, `machine.max_flow`, `machine.min_layer_height`, `machine.max_layer_height` | «0,4 mm · 1,75 mm» |
| Impresora | Límites (`limits`) | `machine.max_feed_*`, `machine.acceleration`, `machine.travel_acceleration`, `machine.corner_speed`, `machine.max_nozzle_temp`, `machine.max_bed_temp`, `machine.max_chamber_temp` | «Aceleración 1000 mm/s²» |
| Impresora | G-code (`gcode`) | `machine.flavor`, `machine.relative_extrusion`, `machine.start_gcode`, `machine.end_gcode`, `machine.variables` | «Klipper» |

   - Un grupo que el motor agregue después lleva su sección en su ficha. Si no la tiene, va a una sección **Otros**
     al final de su pestaña, y la prueba SP1 falla hasta que se le asigne una.
   - Los ajustes de capas no planares, varios materiales y resina van en sus propias secciones (Capas no planares,
     Varios materiales, Resina), que solo aparecen con una impresora que los usa.
   - `compute.*` no va en el panel: es una preferencia del programa.
2. **Cada sección es una tarjeta plegable** con:
   - el ícono y el nombre de la sección;
   - cerrada, una línea de resumen con los dos o tres valores más importantes, como en la tabla, calculada con los
     valores actuales;
   - si tiene ajustes cambiados respecto del perfil guardado, «2 cambios» en violeta al lado del nombre;
   - una flecha que la abre o la cierra. Pueden estar varias abiertas; las abiertas se recuerdan
     (`ui.sections_open`).
   La primera vez solo está abierta Calidad.
3. **Subgrupos:** dentro de una sección, los ajustes van en subgrupos con un título chico. Por ejemplo, Calidad:
   «Altura de capa», «Detalle fino», «Alturas por Z», «Superficies inclinadas (Z anti-aliasing)».
4. **Índice:** arriba del panel, una fila de chips con las secciones de la pestaña. Tocar uno abre esa sección y la
   lleva al principio del panel. El chip de una sección con cambios lleva un punto violeta.
5. **Búsqueda:** el buscador de la barra superior (`interface.md`) abre la pestaña y la sección del ajuste, lo lleva a
   la vista y lo resalta 2 s. Si el ajuste está oculto por el nivel, se muestra igual, marcado (regla 12).
6. **Ajustes que dependen de otro:** se ven solo cuando sirven. La sección muestra el interruptor y, apagado, nada
   más:

| Interruptor | Lo que se oculta apagado |
|---|---|
| `support.enabled` | El resto de `support.*` |
| `zaa.enabled` | El resto de `zaa.*` |
| `infill.nonplanar` | `infill.nonplanar_amplitude`, `infill.nonplanar_wavelength` |
| `infill.dense_under_top` | `infill.dense_density` |
| `infill.combine_auto` encendido | `infill.combine_layers` (lo calcula el motor) |

7. **Una sección sin ajustes** en el nivel elegido no se muestra, ni su chip.
8. **Restablecer** (diseño): cada sección tiene, en su menú de tres puntos, «Restablecer esta sección», que vuelve sus
   ajustes al perfil guardado. El botón Restablecer del pie sigue haciendo lo mismo con todo el perfil.

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

| Sección | Claves |
|---|---|
| Calidad | `layers.height`, `layers.first_height` |
| Paredes | `walls.count`, `walls.seam` |
| Arriba y abajo | `surfaces.top_layers`, `surfaces.bottom_layers`, `surfaces.top_min_thickness`, `surfaces.bottom_min_thickness`, `surfaces.top_pattern` |
| Relleno | `infill.density`, `infill.pattern` |
| Soportes | `support.enabled`, `support.automatic` |
| Adhesión | `adhesion.skirt_loops`, `adhesion.skirt_distance`, `adhesion.brim_width` |
| Temperaturas | `material.nozzle_temp`, `material.first_nozzle_temp`, `material.bed_temp`, `material.first_bed_temp` |
| Enfriamiento | `cooling.fan_min`, `cooling.fan_max` |
| Retracción | `retraction.length`, `retraction.lift` |
| Boquilla y extrusor | `machine.nozzle_diameter` |

    - **Solo en Avanzado:** `layers.detail_*`, `layers.cusp_height`, `zaa.min_thickness`, `zaa.raise`,
      `infill.wall_overlap`, `infill.solid_every`, `infill.solid_below_area`, `infill.combine_*`,
      `infill.stress_density`, `cooling.keep_outer_speed`, `extrusion.pressure_advance`, `compensation.*`,
      `slicing.*`, `gcode.*`, `changes`, `machine.variables`, y los que `settings_levels.md` (regla 5) pone solo en
      Expert.
    - **Normal:** todo lo demás.
11. **Básico avisa lo que oculta:** al pie de cada sección con ajustes ocultos, «3 ajustes más en Normal y Avanzado» y
    el botón «Mostrar todos en esta sección», que la abre entera hasta que se cierre la sección o se cambie de
    pestaña. El nivel elegido no cambia.
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
      y el árbol pestaña › sección › ajuste, cada uno con su casilla (la de la sección marca o desmarca todos sus
      ajustes; a medias se ve con una raya). Arriba, «Partir de: Básico, Normal, Avanzado», «Todos» y «Ninguno».
      Abajo, Cancelar y Aceptar. Cada ajuste muestra su nivel en gris.
    - **Ocultar desde el panel:** en Personalizado, al pasar el puntero sobre un ajuste aparece un ojo tachado que lo
      quita de la vista.
    - **Agregar desde el panel:** al pie de cada sección, «+ Agregar ajuste» lista los ocultos de esa sección.
17. **Se guarda** en `interface.json` (`ui.custom_visible`) y vuelve al abrir el programa. Más › Configuration suma
    «Exportar vista…» e «Importar vista…» (un JSON con `format` `tisma-ui-view`, `version` 1 y la lista de claves) para
    llevarla a otra computadora o compartirla.
18. **Claves que no existen** en la lista (de otra versión) se ignoran sin aviso. Un ajuste nuevo de una versión
    nueva se ve si su nivel no pasa `ui.custom_base`.
19. **Las reglas 6, 7 y 12 siguen valiendo:** un ajuste dependiente se oculta con su interruptor apagado aunque esté
    marcado, una sección sin ajustes visibles no aparece, y un ajuste cambiado se ve aunque esté desmarcado.
20. **Ajustes rápidos** (Quick settings, `interface.md`) es otra lista y no cambia con el nivel ni con la vista propia.

### Boquilla rápida

21. **Dónde:** en la tarjeta Printer del panel de la placa, la lista **Nozzle** al lado de la impresora
    (`interface.md`). Muestra las boquillas del modelo elegido: las variantes del modelo en el paquete
    (`profile_pack.md`) y las impresoras del usuario hechas de ese modelo. Cada fila, el diámetro y, si el perfil lo
    dice, el tipo: «0,4 mm», «0,6 mm · acero endurecido» (`machine.hardened_nozzle`). La elegida, con una marca.
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

27. **Botón de cámara** (diseño): en la tarjeta Printer, al lado del engranaje, cuando la impresora elegida tiene una
    cámara (la suya o una USB de la computadora, `printer_connections.md`, «Cámara USB de la computadora»). Sin cámara
    no aparece.
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

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| SP1 | Recorrer todas las claves de `docs/CONFIG.md` | Cada una tiene sección y nivel; ninguna cae en Otros |
| SP2 | Pestaña Impresión, primera vez | Solo Calidad abierta; las demás cerradas con su resumen («20 % · rectilíneo» en Relleno con los valores por defecto) |
| SP3 | Cambiar `infill.density` a 40 y cerrar Relleno | El resumen dice «40 % · rectilíneo» y «1 cambio»; el chip de Relleno tiene el punto |
| SP4 | Abrir Paredes y Soportes, cerrar el programa y volver a abrirlo | Calidad, Paredes y Soportes siguen abiertas |
| SP5 | `support.enabled` apagado | Soportes muestra solo el interruptor; al encenderlo aparecen los demás |
| SP6 | Buscar «costura» | Se abre Paredes, se ve `walls.seam` resaltado |
| SP7 | Nivel Básico, pestaña Impresión | Solo las claves de la tabla de la regla 10; al pie de Relleno, «N ajustes más…» |
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
