# Interfaz de escritorio

**Qué hace:** la ventana del programa. Desde ahí se cargan las piezas en la placa, se eligen la impresora, el material
y los ajustes, se lamina, se ve la vista previa y se exporta o se manda a la impresora. Tiene además las herramientas
propias: Ingeniería, calibraciones, dispositivos y cuenta en la nube.

**Fase:** P1 (después del motor).
- El diseño es del dueño: la misma estructura, los mismos nombres y el mismo aspecto que la app de teléfono de Tisma,
  en tema claro y oscuro.
- Esta ficha describe lo que se ve y cómo se comporta en Tisma Slicer, observado el 24-10-2026 en una pantalla virtual
  de 1600×1000 (Linux, escala 100 %).
- **Capturas:** la carpeta [`interface/`](interface/) trae capturas de esas pantallas como referencia de cómo se ve
  cada parte (índice al final). Son de referencia: el motor dibuja su interfaz con sus propios componentes. Los logos y
  los íconos de Tisma están en `assets/brand/` y se pueden usar tal cual.
- La foto del fabricante de una impresora, en el asistente, está tapada en la captura: esas fotos no son de Tisma.
- Lo que no se pudo ver sin impresoras conectadas ni cuenta (USB, red, cámaras, informar un problema), y los puntos
  marcados «(diseño)», salen de la documentación del dueño.

## Principios

1. **Ninguna función se pierde:** todo lo que hace el programa tiene un lugar en la ventana nueva.
2. **Textos:** en inglés, con traducción al español.
3. **Teclado:**
   - todo valor numérico se puede escribir con el teclado;
   - los atajos de siempre funcionan (lista completa en «Más», abajo).
4. **Rueda del mouse:** sobre un campo del panel, desplaza el panel; no cambia el valor.
5. **Modos:**
   - **Simple:** solo los ajustes más comunes («The settings most prints need»);
   - **Advanced:** más ajustes («More settings»);
   - **Expert:** todos («All the settings»), como las capas no planares;
   - el modo se elige en la barra superior;
   - en modo Simple, cada página de ajustes dice qué queda oculto y ofrece «Mostrar todos los ajustes».
6. **Ayuda de cada ajuste:** al pasar el mouse se ve qué hace, el valor por defecto y el nombre del ajuste en el
   archivo de configuración (en el motor, su nombre de `docs/spec/SETTINGS_V1.md`). Se ve en la captura
   `04_plate_panel_bottom.png`.

## Ventana

De arriba abajo: la **barra superior** (52 px de alto en las capturas), el **contenido** de la sección elegida y la
**barra de estado** (28 px). En la sección Placa, el contenido es la **vista 3D** a la izquierda y el **panel de la
placa** a la derecha (unos 42 em, 500 px a 1600 de ancho).

### Barra superior (de izquierda a derecha)

Captura: `02_plate_part.png`.

1. **Logo y nombre:** el ícono de Tisma y «Tisma» en negrita.
2. **Secciones**, cada una con ícono y nombre: **Plate**, **Print**, **Printer** y **More**. La activa tiene el texto
   oscuro y una línea violeta de 2 px debajo; las otras, el texto gris. El filamento o la resina no tienen sección: se
   eligen y se editan desde el panel de la placa.
3. **Grupos de herramientas de la placa**, solo íconos (el nombre al pasar el mouse), en este orden: Archivo
   (carpeta), Vista (cubo), Organizar (flechas de mover), Pintar (soporte pintado), Ingeniería (viga doble T),
   Calibrar (torre) y Laminar (varita).
   - Al tocar uno se abre debajo una **tira** con sus herramientas: una tarjeta blanca pegada a la barra, alineada con
     el ícono; cada herramienta es un ícono sobre su nombre, en una celda de 8 × 5,4 em, en filas que se parten si no
     entran. Al pasar el mouse, la celda se resalta con un fondo redondeado (esquinas de 0,8 em).
   - El grupo abierto queda con un fondo violeta suave redondeado.
   - Las herramientas que no se pueden usar (por ejemplo, Mover sin una pieza elegida) se ven apagadas.
   - Tocar el mismo ícono o fuera de la tira la cierra. Ese toque solo cierra: no hace nada más.
4. **Búsqueda de ajustes:** un campo redondeado con lupa («Enter a search term»); lleva al ajuste en su página.
5. **Impresora en uso:** un chip con un círculo (vacío y gris si no está conectada, de color si lo está) y el modelo.
   Al tocarlo se abre Dispositivos.
6. **Modo:** un chip con un punto del color del modo (Simple verde, Advanced amarillo, Expert rojo `#E70000`) y su
   nombre. Al tocarlo, un menú con los tres modos (ver «Menús»).
7. **Cuenta de Tisma Cloud:** un chip con el ícono de persona y «Tisma Cloud». Abre el diálogo de la cuenta.

### Grupos de herramientas

Capturas: `10_group_*.png`.

| Grupo | Herramientas, en orden |
|---|---|
| Archivo | Open, Remove, Remove all, Save, Undo, Redo |
| Vista | 3D, Top, Front, Left, Right, Back, Frame |
| Organizar | Arrange, Move, Rotate, Scale, Lay on face, Cut, Split, Measure, Layer height (altura de capa variable) |
| Pintar | Supports, Seam, Fuzzy skin, Colors |
| Ingeniería | Analysis (abre el panel de Ingeniería), Close |
| Calibrar | Las pruebas que la impresora en uso puede hacer (ver `calibrations.md`) y «Apply result» |
| Laminar | Slice now, Export, Send, USB |

Con una impresora FDM con firmware que acepta todo, Calibrar muestra, en filas de ocho: Temperature, Pressure,
Retraction, Max flow, VFA, Acceleration, Corners, Vibrations / First layer, Flow, Coasting, Arc speed, Arc length, Arc
overlap, Iron flow, Iron spacing / Iron speed, Strength, Dimensions, Correction, Apply result. Con una impresora que
calibra sola la primera capa, no aparece esa prueba. Con una de resina, solo las de resina (`resin.md`). Los mismos
modelos de prueba se ofrecen con el clic derecho sobre la cama.

### Placa

- **Vista 3D:** ocupa todo el ancho que deja el panel. Fondo oscuro en los dos temas (degradé de `#1C1C21` arriba a
  `#141417` abajo); la cama es una grilla gris oscura (`#2B2B31`) con los ejes X rojo, Y verde y Z azul en el origen.
  - Cada cama tiene una etiqueta flotante a su izquierda con su número, «Lock» y «Settings» (una placa puede tener
    varias camas).
  - La pieza se ve del color del filamento; la elegida, violeta, con marcas en las esquinas de su caja.
  - **Avisos:** abajo al centro, sobre la vista 3D: una tarjeta por aviso con ícono, título en mayúsculas («WARNING:»
    en marrón, «ERROR:» en rojo oscuro), el texto, un enlace «[More]» si hay detalle y una × para cerrarla. Los avisos
    de progreso («Slicing finished», «Generating G-code» con barra) usan la misma tarjeta en gris oscuro.
  - **Barra de estado** (abajo): a la izquierda el estado («No objects», «Not sliced», «Ready») y la cantidad de
    objetos; a la derecha, después de laminar, «Estimated: 1h 54m 20s · 33.3 g · 0.93» (tiempo, peso y costo).
- **Panel de la placa** (a la derecha), en tarjetas blancas con títulos grandes violetas, de arriba abajo. Capturas:
  `02_plate_part.png` y `04_plate_panel_bottom.png`.
  1. **Printer:** el modelo (con un candado si es un perfil del sistema) y un engranaje que abre su página. Debajo, en
     una fila, **Nozzle** y **Bed type** como dos listas aparte. La boquilla elige el perfil del modelo para ese
     diámetro.
  2. **Filament** (o resina): muestra de color, candado, perfil y engranaje. El engranaje abre un menú: «Edit preset»,
     «Change extruder color», «Show/Hide template presets». Captura: `44_filament_gear_menu.png`.
  3. **Print settings:**
     - perfil y engranaje;
     - **Supports** en chips: None, Build plate only, Everywhere, Painted only;
     - **Infill:** campo en % a la derecha y chips 0, 10, 15, 20, 30, 50 y 100 %;
     - **Brim:** interruptor a la derecha y, si está encendido, el ancho en mm al lado.
  4. **Sliced Info** (después de laminar): Used Filament (g), (m) y (mm³); Cost; Estimated printing time (normal
     mode); Layer height; Perimeters; Infill; Supports. Dos columnas: nombre en gris, valor en negro.
  5. **Quick settings:**
     - a la derecha del título, «+ Add» (suma un ajuste a la lista) y «All settings» (abre la página);
     - pestañas Print, Filament y Printer: la elegida con fondo violeta y texto blanco;
     - los ajustes por grupo con subtítulo violeta (Layers and Perimeters, Infill, Speed, Skirt and brim…), nombre a la
       izquierda, campo y unidad a la derecha.
  6. **Objects:** la lista de piezas, cada fila con el nombre, un ojo (mostrar u ocultar) y un ícono de ajustes.
  7. **Abajo, fijo** (no se desplaza con el panel):
     - a la izquierda, un botón violeta angosto «»» que esconde el panel;
     - el interruptor **«Slice automatically»**: lamina después de cada cambio;
     - debajo, el **botón principal** a todo el ancho y, a su derecha, un botón cuadrado con una flecha ▲ que abre el
       menú de salidas.
- **Modo compacto (panel escondido):** sobre la vista 3D, abajo a la derecha, flota una barra oscura con «<<» (vuelve
  a mostrar el panel), el botón principal y la flecha ^. Captura `51_compact.png`.

### Botón principal

1. Sin piezas: «Slice now», apagado (gris).
2. Con piezas sin laminar: «Slice now», relleno violeta con texto blanco en negrita.
3. Laminado: la salida elegida (por ejemplo «Export G-code»), con borde violeta y texto violeta, y la flecha al lado
   para elegir otra:
   - **Export G-code;**
   - **Print via USB**, si hay una impresora USB conectada (diseño);
   - **Send** a la impresora de red conectada (diseño).

   El menú de la flecha muestra solo las salidas posibles, con la elegida marcada (captura `31_main_button_menu.png`).
4. Con «Slice automatically», el paso 2 no hace falta.
5. **Relaminar por etapas** (diseño; Tisma Slicer hace algo parecido; meta RV5 de `BENCHMARKS.md`): un cambio rehace solo lo que depende de
   ese ajuste, y el resultado es el mismo que el de un laminado completo.

| Etapa | Ajustes que la rehacen | Qué se rehace |
|---|---|---|
| Corte | `layers`, `slicing`, `compensation`, la malla, su escala o su giro | Todo |
| Trayectorias | `walls`, `fuzzy`, `surfaces`, `infill`, `support`, `adhesion`, `zaa` | Paredes, superficies, relleno, soportes y G-code; no se vuelve a cortar |
| G-code | `speed`, `cooling`, `retraction`, `extrusion`, `gcode`, `changes`, los límites y aceleraciones de `machine` | Solo el G-code y su revisión |

   - mover la pieza en X e Y sobre la cama rehace solo la etapa de G-code: las trayectorias se trasladan;
   - un ajuste que no está en la tabla rehace todo, hasta que se ubique en una etapa;
   - mientras se relamina, la vista previa sigue mostrando el resultado anterior.

### Vista previa (después de laminar)

Captura: `30_preview.png`.

- **Leyenda** arriba a la izquierda, en un panel oscuro semitransparente que se pliega con un triángulo («Legend»):
  - una lista para elegir qué colorea («Feature type», altura, ancho, velocidad…);
  - por tipo de línea, en filas: muestra de color, nombre, tiempo, barra violeta y porcentaje, metros y gramos. Tipos:
    Perimeter, External perimeter, Overhang perimeter, Internal infill, Solid infill, Top solid infill, Bridge infill,
    Gap fill, Skirt/Brim, Custom;
  - «Estimated printing times»: First layer y Total, con el nombre en violeta;
  - una fila de íconos para mostrar u ocultar viajes, retracciones, cambios de herramienta, costuras y otros
    movimientos.
- **Deslizador de capas:** vertical, a la derecha de la vista 3D, con pista violeta y dos manijas; en cada extremo, una
  etiqueta con la Z y el número de capa («44.60 (223)»). Debajo, un candado y un engranaje.
- **Deslizador de movimientos:** horizontal, abajo, para recorrer la capa; la etiqueta muestra el número de
  movimiento.

### Páginas de ajustes (Print, Filament, Printer)

Capturas: `41_printer_settings.png`, `42_print_settings.png`, `43_filament_settings.png`.

- **Arriba:** el título de la página en violeta («Print Settings», «Filaments», «Printers»), el perfil en una lista y
  sus botones: guardar, borrar, comparar.
- **A la izquierda:** la lista de páginas del perfil, cada fila con ícono y nombre; la elegida, con fondo violeta suave
  redondeado y texto violeta.
  - Print: Layers and perimeters, Infill, Skirt and brim, Support material, Speed, Non-planar, Multiple Extruders,
    Advanced, Output options, Notes, Dependencies.
  - Filament: Filament, Cooling, Advanced, Filament Overrides, Custom G-code, Notes, Dependencies.
  - Printer: General, Custom G-code, Machine limits, Extruder 1 (uno por extrusor), Notes, Dependencies.
- **En el centro:** los ajustes en grupos con título violeta en negrita y una línea fina debajo; cada ajuste con su
  nombre, el campo (o interruptor, o lista) y la unidad.
  - Algunos grupos muestran textos calculados. Por ejemplo: «Top shell is 1 mm thick for layer height 0.2 mm».
- **Printer** tiene dos páginas, elegidas con dos chips arriba a la izquierda (Settings y Devices):
  - **Settings:** el perfil de la impresora;
  - **Devices:** impresoras por USB y por red, con su estado, cámara y envío. Cada impresora puede usar una cámara USB
    de la computadora. El detalle, pantalla por pantalla, está en `devices.md`. Captura `40_devices.png`:
    - a la izquierda, la lista de dispositivos y abajo «+ Add network printer», «Edit printer…» y «Change access
      code…»;
    - a la derecha, el dispositivo elegido: nombre, aviso de cómo conectarlo, cámara, Connection, Active print
      («Print the sliced plate», «Print a file…», Pause, Cancel print), Temperature (gráfico de los últimos 10 minutos,
      línea punteada para el objetivo), Extruder y Bed (valor, − y +, Pre-heat, Off), Printer control (X/Y y Z con
      Home, Home all, Motors off, distancias 0.1, 1, 10 y 100 mm), Terminal y «Send G-code».
  - la sección abre la última página usada.

### Más

Tarjetas armadas con lo que antes estaba en los menús, en columnas de tres. Cada acción tiene su atajo a la derecha, en
gris, y se ve apagada si no se puede usar. Los subtítulos de una tarjeta van en gris y negrita. Capturas
`45_more_top.png`, `46_more_middle.png`, `47_more_bottom.png`. En orden:

1. **Tisma:** Thingiverse; Tisma Cloud account…; Preferences…
2. **File:**
   - New Project (Ctrl+N), Open Project… (Ctrl+O);
   - Recent projects: la lista, numerada;
   - Save Project (Ctrl+S), Save Project as… (Ctrl+Alt+S);
   - Import: Import STL/3MF/STEP/OBJ/AMF… (Ctrl+I), Import STL (Imperial Units), Import SLA Archive…, Import ZIP
     Archive…, Import Config… (Ctrl+L), Import Config from Project… (Ctrl+Alt+L), Import Config Bundle…;
   - Export: Export G-code… (Ctrl+G), Send G-code… (Ctrl+Shift+G), Export G-code to SD Card / Flash Drive… (Ctrl+U),
     Slice in the Cloud and Export G-code…, Export Plate as STL/OBJ…, Export Plate as STL/OBJ Including Supports…,
     Export Toolpaths as OBJ…, Export Config… (Ctrl+E), Export Config Bundle…, Export Config Bundle With Physical
     Printers…;
   - Convert: Convert ASCII G-code to binary…, Convert binary G-code to ASCII…;
   - Eject SD Card / Flash Drive… (Ctrl+T); (Re)Slice Now (Ctrl+R); Repair STL file…; G-code Preview…; Quit.
3. **Edit:** Select All (Ctrl+A), Deselect All (Esc), Delete Selected (Del), Delete All (Ctrl+Del), Undo (Ctrl+Z),
   Redo (Ctrl+Y), Copy (Ctrl+C), Paste (Ctrl+V), Reload from Disk (F5), Plate Settings…, Search (Ctrl+F).
4. **Window:** Plater Tab (Ctrl+1), Print Settings Tab (Ctrl+2), Filament Settings Tab (Ctrl+3), Printer Settings Tab
   (Ctrl+4), 3D (Ctrl+5), Preview (Ctrl+6), Shape Gallery, Print Host Upload Queue (Ctrl+J), Open New Instance
   (Ctrl+Shift+I), Compare Presets.
5. **View:** Iso (0), Top (1), Bottom (2), Front (3), Rear (4), Left (5), Right (6); con casilla: Show Labels (E), Show
   Legend (L), Collapse Sidebar (Shift+Tab), Fullscreen (F11).
6. **Configuration:** Configuration Wizard…, Configuration Snapshots…, Take Configuration Snapshot, Check for
   Configuration Updates, Check for Application Updates, Desktop Integration, Preferences (Ctrl+P), Language, Flash
   Printer Firmware, Print via USB…, Tisma Cloud Account…, Wi-Fi Configuration File.
7. **Calibration:** las pruebas (Temperature tower…, Pressure advance tower…, Retraction tower…, Maximum volumetric
   speed…, Vertical fine artifacts (VFA)…, Acceleration tower…, Cornering tower (jerk / junction deviation)…, Input
   shaping frequency…, First layer Z offset…, Flow rate (%)…, Coasting distance…, Arc overhang speed…, Arc overhang
   maximum distance…, Arc overhang overlap…, Ironing flow…, Ironing spacing…, Ironing speed…, Staggered perimeters:
   strength coupons…, Dimensional accuracy block…), Dimensional correction… y Apply a calibration result…
8. **Help:** Tisma Slicer Website, Software Releases, System Info, Show Configuration Folder, Report a problem…, About
   Tisma Slicer, Keyboard Shortcuts (?).

**Help > Report a problem…:**
- manda a Tisma Cloud qué pasó y los pasos;
- si la persona quiere, también la versión, el sistema, la impresora en uso y las últimas 500 líneas del registro de
  la sesión, que se ven antes de mandar;
- se puede mandar sin cuenta.

## Menús

Hoy los menús emergentes se ven con el estilo del sistema (fila resaltada en azul). (diseño) En el motor, la fila
resaltada va en violeta, como el resto de la interfaz.

- **Modo** (chip de la barra, captura `32_mode_menu.png`): tres opciones con botón de radio, cada una con su
  explicación: «Simple — The settings most prints need», «Advanced — More settings», «Expert — All the settings».
- **Clic derecho sobre una pieza** (captura `20_menu_object.png`), con separadores donde se marca «/»: Add instance (+),
  Remove instance (−), Set number of instances…, Fill bed with instances…, Delete (Delete) / Set as a Separated Objects
  / Printable (casilla) / Reload from disk, Convert from meters, Convert from imperial units, Replace with STL, Export
  as STL/OBJ… / Scale to print volume, Simplify model / Tessellate again…, Check CAD geometry (solo piezas que vienen
  de un STEP) / Mirror ▸, Split ▸, Split to fit the printer… / Add part ▸, Add negative volume ▸, Add modifier ▸, Add
  support blocker ▸, Add support enforcer ▸, Height range Modifier / Infill, Layers and Perimeters, Support material,
  Add settings ▸.
- **Clic derecho sobre la cama vacía** (captura `21_menu_bed.png`): Add Shape ▸ y Add calibration model ▸. El segundo
  lista las mismas pruebas de la tarjeta Calibration de Más, de Temperature tower… a Dimensional accuracy block…
- **Engranaje del filamento:** Edit preset, Change extruder color, Show/Hide template presets.
- **Flecha del botón principal:** las salidas posibles, con la elegida marcada.

## Diálogos

- **Tisma Cloud** (captura `33_cloud.png`): título violeta, una línea de qué hace («Slice and run the structural
  analyses on the Tisma Cloud servers. The account and the plan are managed on the web site.»), campos Server, E-mail y
  Password, el estado («Not logged in.») y los botones Log out (apagado sin sesión), Close (borde violeta) y Log in
  (relleno violeta).
- **Preferences** (capturas `70_preferences_general.png`, `71_preferences_gui.png`, `72_preferences_other.png`):
  páginas General, Camera, GUI y Other en una lista a la izquierda; a la derecha, cada opción con un interruptor
  violeta. En GUI, **Appearance**: Like the system, Light o Dark. Abajo, Cancel y OK.
- **Botones de todos los diálogos:** abajo a la derecha; la acción principal con relleno violeta y texto blanco, las
  otras con borde violeta; Escape es Cancel y Enter es la acción principal.

## Aspecto

Los colores salen del logo de Tisma (`assets/brand/README.md`). Valores medidos en las capturas y en la paleta de Tisma
Slicer.

### Colores

| Uso | Claro | Oscuro |
|---|---|---|
| Acento (botón principal, títulos, selección) | `#7A24C9` | `#7A24C9` |
| Acento al pasar el mouse | `#8E34DF` | `#8E34DF` |
| Acento apretado | `#5B0FA7` | `#5B0FA7` |
| Texto en color de acento | `#7A24C9` | `#C9A2F5` |
| Fondo violeta suave (grupo abierto, chip elegido, página elegida) | `#ECE1F9` | `#3B2A55` |
| Fondo de los paneles | `#F7F7F9` | `#1F1F24` |
| Tarjetas | `#FFFFFF` | `#27272D` |
| Separadores y bordes | `#E2E2E8` | `#34343C` |
| Texto | `#1A1A1F` | `#ECECF0` |
| Texto secundario | `#6B6B76` | `#9A9AA6` |
| Fondo apagado | `#D9D9DF` | `#3A3A42` |
| Texto apagado | `#9A9AA4` | `#7A7A84` |
| Barra superior | color de ventana del sistema (`#F6F5F4`) | color de ventana del sistema (`#353535`) |
| Vista 3D | degradé `#1C1C21` → `#141417` | igual |
| Íconos de línea | `#808080`, recoloreado al texto del tema | igual |

### Componentes

Medidas en **em**, la unidad de la letra de la interfaz (en las capturas, 1 em = 12 px).

| Componente | Cómo se ve |
|---|---|
| Tarjeta | fondo de tarjeta, esquinas de 0,6 em, margen interno de 0,8 em, 0,6 em entre tarjetas |
| Título de grupo | 1,25 veces el texto, semibold, en acento |
| Rótulo de un ajuste | chico, en negrita, texto secundario |
| Chip | alto 2,8 em en la barra superior (menos en los paneles), esquinas de medio alto, margen lateral 1,1 em, borde de 1 px del color de separadores; puede llevar un ícono o un punto de color de 0,8 em; elegido: fondo violeta suave, borde y texto en acento; al pasar el mouse: `#EEEEF2` (oscuro `#303038`) |
| Chips de opciones | una fila de chips, uno elegido; se parten en más filas si el panel es angosto |
| Botón principal | relleno de acento, texto blanco en negrita, esquinas de 0,6 em, margen 1,4 × 0,6 em, ancho mínimo 9 em |
| Botón secundario | borde y texto de acento sobre la tarjeta |
| Botón apagado | fondo y texto apagados |
| Interruptor | píldora; encendido, en acento con el círculo blanco a la derecha; apagado, gris |
| Pestañas | segmentos del mismo ancho; la elegida, relleno de acento y texto blanco |
| Lista de páginas | filas con ícono y nombre; la elegida, fondo violeta suave con esquinas de 0,6 em y texto semibold; al pasar el mouse, el fondo del chip |
| Ayuda al pasar el mouse | recuadro negro con texto blanco |

### Tema oscuro

Capturas `d*.png`. Mismos lugares y tamaños; cambian los colores de la tabla. La vista 3D no cambia. Observado en
Tisma Slicer: los íconos apagados de la barra casi no se ven sobre el fondo oscuro. (diseño) En el motor, un ícono
apagado usa el texto apagado del tema, no un gris fijo.

## Asistente de configuración (primer uso)

Capturas: `60_wizard_printers.png`, `61_wizard_checked.png`, `62_wizard_filaments.png`. Ventana propia, con los pasos
a la izquierda (un punto violeta en el paso actual) y el logo de Tisma abajo a la izquierda; Cancel abajo a la
izquierda; Back, Next y Finish abajo a la derecha.

1. **Printers** («Printer Selection»):
   - una búsqueda por nombre, la lista de marcas y la de impresoras de la marca, cada una con su casilla;
   - al buscar, aparece «All results (n)» y las marcas con resultados, con la cantidad elegida al lado
     («Creality (1)»);
   - a la derecha, la foto del modelo (si no hay, el dibujo de una impresora en violeta sobre un fondo violeta suave) y
     sus boquillas, cada una con su casilla. Las impresoras se muestran una vez por modelo, no una vez por boquilla.
     Abajo: «Another nozzle can be chosen later, next to the printer»;
   - al marcar una impresora se marca la boquilla de 0,4 mm (o la primera del modelo);
   - abajo: cuántas impresoras hay elegidas, y «My printer is not in the list: define a custom printer»;
   - se pueden elegir varias marcas, de filamento y de resina.
2. **Filaments** («Filament Profiles Selection»):
   - el paso aparece después de elegir una impresora;
   - cuatro columnas para filtrar: Printer, Type, Vendor y Profile, con casillas en la última;
   - botones All y None;
   - aviso: los marcados con * no sirven para alguna de las impresoras instaladas;
   - se ofrecen solo los filamentos del diámetro que usan las impresoras elegidas (diseño).
3. **Finish:** instala las impresoras con sus ajustes y filamentos.

Agregar un filamento después no vuelve a pedir la impresora (diseño).

## Panel de Ingeniería

Captura: `50_engineering.png`.

- Se abre sobre la vista 3D, a la izquierda, en un panel oscuro con el título «Engineering», el nombre de la pieza en
  violeta y dos pestañas: Structural y Aerodynamics.
- **Conditions:**
  - Material, y un botón «Edit the material table…»;
  - Temperature [°C], Safety factor y Resolution, con el tamaño resultante («About 60000 voxels of 1.12 mm.»);
  - «Printed part (walls and infill of the profile)» y «Compute in the cloud».
- **Supports, loads and limits:** «Click on the part to add:». Modos en botones: Nothing (rotate), Fixed face, Load on
  a face, Load on a point y Displacement limit; el elegido, en violeta.
- **Analyze:** botón violeta a todo el ancho, apagado hasta que haya una cara fija y una carga, con esa explicación
  debajo («Add a fixed face and a load.»).
- **Optimize:**
  - «Skin + core»: piel no planar automática y el relleno más liviano con nervios;
  - secciones plegables: Lightest infill, Local reinforcement, 3D lattice y Print orientation
    (ver `structural_optimization.md`).

## Con una impresora de resina

- Se ocultan las funciones que solo sirven para FDM: capas no planares, arcos en voladizos, pintar costuras, piel
  difusa y multimaterial, ajustes por cama.
- En Ingeniería quedan el análisis estructural y «Paredes y relleno interior».
- Calibrar muestra las pruebas de resina.
- Dispositivos queda.

## Pruebas de aceptación

Escenarios para comprobar en la pantalla, como se observaron en Tisma Slicer.

| # | Escenario | Resultado esperado |
|---|---|---|
| UI1 | Primer arranque con una carpeta de configuración vacía | Se abre el asistente en «Printers», con Finish apagado y «No printer selected.» |
| UI2 | Buscar «MK4S» | Marcas: «All results (4)» y la marca con resultados; las 4 impresoras que lo contienen |
| UI3 | Marcar una impresora | Se marca su boquilla de 0,4; aparece el paso Filaments; el texto pasa a «1 printer selected»; se activan Next y Finish |
| UI3b | Después de UI3, desmarcar la boquilla de 0,4 | La impresora queda desmarcada y el asistente sigue abierto (en Tisma Slicer el programa se cerraba: es un error, no se copia) |
| UI4 | Finish | Se abre la placa con la impresora, la boquilla de 0,4, un filamento y un perfil de impresión elegidos; barra de estado «No objects» |
| UI5 | Placa vacía | Botón principal «Slice now», apagado; herramientas de Organizar y Pintar apagadas |
| UI6 | Cargar una pieza | Barra de estado «Not sliced» y «1 object»; «Slice now» activo |
| UI7 | Laminar | Aparecen la vista previa con su leyenda y deslizadores y la tarjeta Sliced Info; botón principal «Export G-code» con su flecha; barra de estado con tiempo, peso y costo |
| UI8 | Flecha del botón principal, sin impresoras conectadas | Solo «Export G-code», marcada |
| UI9 | Esconder el panel | La vista 3D ocupa todo el ancho; abajo a la derecha flotan «<<», el botón principal y su flecha |
| UI10 | Página Print en modo Simple | Pocas páginas (capas y perímetros, relleno, falda y borde, soportes) y, abajo a la izquierda, el aviso de modo Simple con «Mostrar todos los ajustes» |
| UI11 | Calibrar con una impresora que calibra sola la primera capa | Sin la prueba de primera capa |
| UI12 | Ingeniería > Analysis sin apoyos ni cargas | «Analyze» apagado, con «Add a fixed face and a load.» |
| UI13 | Abrir un grupo de herramientas y tocar otro | El primer toque fuera de la tira solo la cierra; el segundo abre el otro grupo |
| UI14 | Clic derecho sobre la cama vacía | Menú con Add Shape ▸ y Add calibration model ▸ |
| UI15 | Preferences > GUI > Appearance: Dark, y reiniciar | Paneles y tarjetas con los colores oscuros de la tabla; la vista 3D igual |

## Capturas

Todas a 1600×1000, salvo el asistente y las preferencias, recortados a su ventana. Pieza de prueba: un soporte
mecánico de 81×95×45 mm sobre una cama de 420×420 mm.

| Archivo | Qué muestra |
|---|---|
| `01_plate_empty.png` | Placa vacía: barra superior, vista 3D, panel de la placa, botón principal apagado |
| `02_plate_part.png` | Una pieza cargada, sin laminar |
| `04_plate_panel_bottom.png` | El final del panel: Quick settings, Objects, la ayuda de un ajuste al pasar el mouse |
| `10_group_file.png` … `10_group_slice.png` | Cada grupo de herramientas abierto (Organizar, con la pieza elegida) |
| `20_menu_object.png` | Clic derecho sobre una pieza |
| `21_menu_bed.png` | Clic derecho sobre la cama vacía, con los modelos de calibración |
| `30_preview.png` | Vista previa con leyenda y deslizadores |
| `31_main_button_menu.png` | Menú de la flecha del botón principal |
| `32_mode_menu.png` | Menú del modo |
| `33_cloud.png` | Diálogo de Tisma Cloud |
| `40_devices.png` | Printer > Devices con la impresora USB |
| `41_printer_settings.png`, `42_print_settings.png`, `43_filament_settings.png` | Páginas de ajustes |
| `44_filament_gear_menu.png` | Menú del engranaje del filamento |
| `45_more_top.png`, `46_more_middle.png`, `47_more_bottom.png` | La página Más completa |
| `50_engineering.png` | Panel de Ingeniería |
| `51_compact.png` | Modo compacto |
| `60_wizard_printers.png`, `61_wizard_checked.png`, `62_wizard_filaments.png` | Asistente |
| `70_preferences_general.png`, `71_preferences_gui.png`, `72_preferences_other.png` | Preferencias |
| `d02_plate_part.png`, `d10_group_calibrate.png`, `d30_preview.png`, `d41_printer_settings.png`, `d42_print_settings.png`, `d45_more_top.png`, `d50_engineering.png` | Las mismas pantallas en tema oscuro |
