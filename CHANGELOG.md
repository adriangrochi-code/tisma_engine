# Registro de cambios

Los cambios de Tisma engine que importan a quien lo usa, por versión. El formato sigue
[Keep a Changelog 1.1.0](https://keepachangelog.com/es-ES/1.1.0/) y los números de versión, el
[versionado semántico 2.0.0](https://semver.org/lang/es/). La política está en `docs/VERSIONING.md`.

Cada versión agrupa sus cambios en estas categorías: **Agregado** (funciones nuevas), **Cambiado** (lo que ya existía
y ahora se comporta distinto), **Obsoleto** (lo que se va a quitar), **Quitado**, **Corregido** (errores) y
**Seguridad**. Una categoría sin cambios no se escribe.

## [Sin publicar]

### Agregado

- **Menú del botón derecho en la placa:** fuera de los modelos, «Agregar modelo…», «Agregar forma» (cubo, cilindro y
  esfera de 20 mm) y «Organizar».
- **Deslizadores en Mover, Girar y Escalar**, bajo cada número: la cama de punta a punta, −180° a 180°, 10 % a 1000 %.

- **`tisma --dump-ui filas.json`** (con `--screenshot`): cada fila de ajustes dibujada, con su clave, su etiqueta, el
  valor guardado, su rectángulo y si la columna le corta la etiqueta; para revisar textos y campos sin mirar la imagen.

- **`tisma --draw-stats`:** cuántas llamadas de dibujo y cuántos vértices pide cada cuadro, los de la vista 3D y los de
  los paneles por separado, para medir la vista previa en cualquier placa de video; y cuánto ocupa la vista en la
  placa (instancias de la vista previa, recorridos, mallas y destinos de dibujo). Y el paso `slice` de
  `--ui-script` (lamina como el botón y abre la Vista previa), que ahora corre también sin `--screenshot`.

- **`--ui-script`: `expect-toast TEXTO`** espera un aviso con ese texto, y `click` sobre una casilla la marca o
  desmarca.

- **`tisma --ui-script pasos.txt`:** pruebas de la interfaz con mouse y teclado de verdad: clic en el campo de un
  ajuste, escribir, teclas, y `expect` con el valor guardado; sale con 1 si un paso falla. Pruebas nuevas de la página
  de la impresora y de un número de impresión escrito con coma.

- **`support.under_bridges`** (ficha `supports_skirt.md`, regla 1, S6 y S10): soporte también bajo los puentes. Sin
  él (por defecto), lo que cuelga sostenido en dos extremos, o todo alrededor como una tapa, no lleva soporte, sea
  cual sea su largo; un voladizo sostenido de un solo lado sí. En el panel, en Soportes.

- **Chequeo de la vista 3D en cada captura** (`tisma --screenshot`, pedido del dueño): en Preparar y Vista previa, la
  imagen propia de la vista no puede tener píxeles negros (un NaN de la luz) ni más de uno de cada mil que destelle;
  si falla, sale con 1 y deja la imagen al lado. `--no-view-check` lo saltea; `--ao-strength` elige la fuerza de la
  oclusión.

- **Grupo `object`** (ficha `multi_extruder.md`, reglas 1 y 13): en la línea de comandos, el extrusor del modelo
  (`object.extruder`) y si una purga va a su relleno o a todo él (`object.wipe_into_infill`, `object.wipe_into_object`).

- **`placement.drop_to_bed`** (ficha `placement.md`, regla 3): con `false` la pieza queda donde viene en Z; lo que
  está debajo de la cama no se imprime, y un objeto que no la toca se rechaza («no extrusions on the first layer»).

- **`retraction.coast`** (ficha `coasting.md`): el final de cada perímetro y de cada relleno se recorre a su velocidad
  sin extruir; la presión que queda en la boquilla lo termina. Menos gotas en la costura y menos hilos. En el panel,
  en Retracción, «Coasting».

- **`speed.max_flow`** (ficha `gcode_motion.md`, regla 4): un tope de caudal propio del perfil de impresión, en mm³/s;
  vale el menor entre él, `machine.max_flow` y `extrusion.max_flow`.

- **`speed.small_loops`** (ficha `gcode_motion.md`, regla 3): los perímetros cerrados de menos de 40,84 mm, también el
  externo, van a esa velocidad, en mm/s o un porcentaje de `speed.walls`. Sin valor, cada pared a la suya.

- **Guías de calibración en la ayuda** (ficha `assistant.md`): la pestaña Calibraciones con las guías del paquete en
  orden, del filamento y de la impresora; cada guía dice para qué sirve, cuándo repetirla, qué prueba conviene hacer
  antes, sus pasos, cómo leer la impresión, el resultado y sus fuentes («Basado en»). Con lo medido (la altura en
  una torre, el bloque o el parche elegido, segundos, un ángulo, capas) calcula el valor nuevo del ajuste, con inicio,
  paso y franja tomados de la última prueba generada, y ofrece aplicarlo después de corregirlo si hace falta. «Cómo se hace» abre la guía desde la página de calibraciones y desde la
  sección Calibración de la impresora.
- **Input shaping desde su guía:** con la impresora conectada, la guía manda la frecuencia elegida con el comando de
  su firmware (Klipper, Marlin 2 o RepRapFirmware). El tutorial de material nuevo abre las guías en orden: la primera
  que falta de temperatura, flujo máximo, avance de presión, flujo y retracción.
- **«¿Cómo lo arreglo?» en los avisos del laminado**: un clic en los avisos los lista con un enlace al problema de la
  ayuda que les corresponde.
- **Preferencias del asistente** en Más: ofrecer los tutoriales, las definiciones del glosario junto a los ajustes y
  la ficha y los avisos del material.

- **Más ajustes de las fichas:** `cooling.full_fan_layer` (el ventilador sube de a poco), `cooling.enabled`,
  `speed.support_interface`, `acceleration.wipe_tower`, `support.bottom_z_gap`, `support.bottom_interface_layers`,
  `support.buildplate_only` (solo soporte desde la cama), `walls.outer_first`, `walls.outer_width` (ancho propio de la pared externa), `walls.spread_count` (cuántos cordones del medio absorben la diferencia de ancho), `walls.transition_length` (largo de la rampa de N a N + 1 cordones), `walls.staggered` (perímetros escalonados: los internos impares media capa más arriba), `walls.transition_angle` (cantidades impares de cordones solo en esquinas cerradas), `cooling.dynamic_overhang_fan` y `cooling.overhang_fan_0` a `_3` (ventilador según cuánto se apoya el cordón de la pared), `cooling.slowdown_logic` (`consistent_surface`: la pared externa y la que la toca se frenan al final) y `cooling.transition_distance` (los últimos mm de una capa frenada a su velocidad), el relleno del soporte (`support.pattern`: líneas, grilla o panal; `support.sheath`, un lazo por el borde de la base; `support.interface_pattern`, `support.interface_spacing` y `support.interface_contact_loops`), `support.enforce_layers` (soporte bajo lo que cuelga en las primeras capas), el estilo `snug` de `support.style` con `support.closing_radius` (soporte ajustado a lo que cuelga), `support.free_bridge_length` (el aviso `long_bridge` de los puentes más largos que eso sobre nada), `machine.max_speed` y `machine.max_acceleration` (un límite para X e Y), los parámetros propios de las plantillas (`machine.custom_parameters`, `material.custom_parameters`, `print.custom_parameters`, `print.variables`), `gcode.custom_template` (el G-code propio de un cambio por altura con `"custom": true`), `gcode.corner_smoothing` (esquinas redondeadas en las paredes internas y los rellenos), `surfaces.bottom_pattern`, `surfaces.bridge_angle` (dirección fija de los puentes), `infill.before_walls` (los rellenos antes que las paredes) e `infill.max_link`, las pruebas por franjas del grupo `calibration` (`mode`, `start`, `step`, `end`, `band_height`: temperatura, avance de presión, retracción, caudal, aceleración, esquinas o input shaping a lo largo de la altura), `layers.first_line_width` (ancho de la primera capa, 200 % de su altura), anchos de línea propios (`surfaces.top_width`, `surfaces.solid_width`, `infill.width`), y los nombres de las fichas
  `support.angle_limit`, `arrange.distance`, `filament.pressure_advance` y `machine.e_mode`.
- **La pestaña Objetos** (`settings_levels.md`, reglas 11 y 13), junto a Impresión y Material: las piezas de la placa
  y, debajo de cada una, sus grupos de ajustes propios, modificadores, volúmenes negativos y rangos de alturas; la fila
  elegida muestra sus ajustes debajo de la lista.
- **Ajustes de una pieza** (`model_tools.md`, reglas 5, 7 y 8; `settings_levels.md`, reglas 10 y 11): clic derecho
  sobre la pieza, en los niveles Normal y Avanzado, para darle paredes, relleno o capas sólidas propios; se editan en
  la ventana «Ajustes de …» y se guardan en el proyecto. En el motor, la parte que va después gana donde dos se pisan.
- **Modificadores, volúmenes negativos y rangos de alturas desde la interfaz:** clic derecho sobre la pieza (niveles
  Normal y Avanzado), editados en la ventana «Ajustes de …» y guardados en el proyecto. Los modificadores y negativos
  siguen a la pieza al moverla, girarla, escalarla y espejarla.
- **Rangos de alturas en el motor** (`model_tools.md`, regla 6): un tramo de alturas de un objeto con sus ajustes,
  la altura de capa incluida (el tramo empieza una capa).
- **Modificadores en el motor** (`model_tools.md`, reglas 3 a 5): donde un modificador toca las partes de su objeto,
  esa zona toma sus ajustes (paredes, relleno, capas sólidas) con paredes propias; el de después gana donde dos se
  pisan, y una altura de capa en un modificador se deja de lado con un aviso. Uno que cambia solo el relleno cambia el
  relleno de su zona sin paredes en el borde. Todavía no se agregan desde la interfaz.
- **Volúmenes negativos:** un proyecto 3MF con volúmenes negativos los resta de la pieza en cada capa que cruzan, sin
  agregar capas ni mover la pieza (`model_tools.md`, regla 2). Todavía no con corte, separar o copias.
- **El grupo `material` de las fichas:** `material.nozzle_temp`, `material.bed_temp`, `material.flow_factor`,
  `material.type` y los demás se leen como los ajustes del motor que nombran (`docs/CONFIG.md`, «material»).

### Cambiado

- **Soportes bajo puentes:** se decide punto por punto. Una losa sobre tres paredes en U ya no lleva soporte; una
  sobre dos paredes en L, un pilar de un lado o uno central sí, en toda la zona que cuelga.

- **La primera vez se elige la impresora y el filamento** antes del recorrido de bienvenida (pedido del dueño): se abre
  el catálogo de impresoras («paso 1 de 2»), después el de filamentos, y al cerrarlos, elegido o no, el recorrido.
  `tisma --first-run` arranca así aunque ya haya preferencias.

- **La ventana descansa cuando no pasa nada:** sin mouse ni teclado por 3 s, sin trabajos en marcha ni impresora
  conectada, se despierta con un evento o cada 2 s en vez de diez veces por segundo; un laminado u otro trabajo que
  termina la despierta. Con la esfera de 80 mm laminada (llvmpipe), en 30 s de reposo: de 38 % de CPU y 260 cuadros a
  2 % y 15.

- **La vista previa dibuja la mitad de los vértices** (OpenGL y Vulkan): de cada cordón, solo los dos costados y la
  punta que miran al ojo (o a la luz, en la sombra); los otros quedan siempre detrás. La imagen no cambia. Con una
  esfera de 80 mm laminada (OpenGL, llvmpipe), de 2,6 a 1,4 s por cuadro completo.

- **La vista 3D quieta no se vuelve a dibujar** (OpenGL y Vulkan): si la cámara, las capas, los colores y los datos no
  cambiaron, se muestra la imagen anterior. Con una esfera de 80 mm laminada (OpenGL, llvmpipe), de 406 llamadas de
  dibujo por cuadro a 1; mover la vista dibuja todo como antes.

- **Medir en piezas grandes:** la herramienta prepara la malla una vez por pieza y no en cada clic. Con 2 millones de
  triángulos, el primer clic tarda lo mismo (unos 0,35 s) y los siguientes, de 0,1 a 26 ms (antes, 0,36 a 0,44 s cada
  uno).

- **Otra boquilla también en las impresoras del catálogo** (pedido del dueño): la lista trae los tamaños que el
  fabricante publica (a menudo solo el de fábrica) y ahora siempre ofrece «Otra boquilla…». La ficha del catálogo
  dice «Sin dato» donde no hay fuente (firmware, velocidad máxima…), en vez de «-».

- **Sin soporte bajo los puentes por defecto** (`support.under_bridges`): antes el soporte iba bajo todo lo que
  colgaba, también un puente entre dos pilares.
- **Lazos de contacto en las dos capas de interfaz de arriba** (`support.interface_contact_loops`, ficha
  `supports_more.md`, regla 12): antes solo en la de arriba.

- **Abrir modelos grandes:** con 2 millones de triángulos, de unos 0,75 s a unos 0,57 s antes del primer cuadro
  (la caja de la pieza sin armar la malla dos veces, una copia menos y los vértices de la vista en paralelo), y a
  unos 0,50 s con la lectura del STL binario, el giro y el apoyo en la cama también en paralelo.
  `TISMA_TIMING=1` muestra cuánto tarda cada paso.
- **Mover la vista con modelos grandes:** con más de un millón de triángulos en la placa, mientras la cámara gira, se
  desplaza o acerca, la vista dibuja una copia simplificada de unos 400 000 triángulos, armada en otro hilo; al
  soltar, vuelve la malla entera. Con GL por software y 2 millones de triángulos, de 672 a 159 ms por cuadro.

- **Las superficies de abajo van en líneas monótonas** (`surfaces.bottom_pattern`, `monotonic` por defecto, como la
  ficha): sin unirlas en zigzag. Las capas sólidas bajo una superficie de arriba siguen en zigzag.
- **El relleno poco denso une sus líneas por tramos de hasta 50 mm** siguiendo el borde (`infill.max_link`); antes, solo
  hasta tres separaciones de líneas.
- **Un cambio de temperatura en Z 0 es la temperatura del comienzo** (`calibrations.md`, regla 2): la espera inicial
  (`M109`) ya es la de la primera franja de una torre de temperatura, no la del filamento.
- **La base del soporte lleva vaina** (`supports_more.md`, regla 10): un lazo cerrado por el borde de cada zona y las
  líneas adentro, como Tisma Slicer por defecto. `support.sheath` en `false` vuelve al zigzag solo.
- **Las capas cortas se frenan a una velocidad común** (`cooling.md`, regla 2): todas las extrusiones más rápidas
  bajan a una misma velocidad, la más alta que llega al tiempo mínimo (antes bajaban todas en la misma proporción).
  Los puentes y los perímetros sobre el aire ya no se frenan (K5). Con `cooling.keep_outer_speed`, cuando frenar lo
  de adentro hasta `cooling.min_speed` no alcanza, ahora se frena solo la pared externa antes de bajar todo de la
  mínima. En C3 el motor da 27,82 mm/s donde Tisma Slicer da 27,93.
- **El cordón del medio llega más tarde:** una pared pasa a 3 o 5 cordones recién cuando el del medio tendría unos
  0,8 cordones de ancho (`perimeters.md`, tabla P2: 2 cordones en 1,1 mm y 3 en 1,2; 4 en 1,9 y 5 en 2,0). Antes
  aparecía medio cordón antes.

- **Cordón del medio solo en esquinas cerradas:** una pared pasa por cantidades impares de cordones solo en las
  esquinas del contorno de 10° o menos (`walls.transition_angle`, `perimeters.md` P14); antes, hasta 45°.

- **Cordones fuera de la Z de la capa:** en el relleno ondulado y en las costuras en rampa, el material es el del
  cordón de la capa por la parte de su altura que ocupa (lineal en el espesor), como en el Z anti-aliasing. Antes
  salía de la sección de un cordón de ese espesor.
- **Tarjeta «Paredes finas»** en Resistencia: `walls.min_line_width`, `walls.min_detail`, `walls.spread_count` y
  `walls.transition_length`.

- **Paredes finas de tres cordones o más:** solo los del medio cambian de ancho; los demás quedan nominales
  (`walls.spread_count` 1, `perimeters.md` P2). Antes todos se repartían la diferencia.

- **El cordón de las fichas** (`perimeters.md` regla 1, `gcode_motion.md` regla 1): el filamento sale de la sección
  con costados redondeados, `(w − h)·h + π·(h/2)²`, y los cordones vecinos se solapan `h·(1 − π/4)` (con 0,45 × 0,2,
  a 0,407 mm en vez de 0,45). Paredes, rellenos, torre de purga, verificador y vista previa usan el mismo modelo; los
  puentes y voladizos redondos se escriben tan altos como anchos. **Cambia el G-code de siempre.**

- **Los anchos de los rellenos en 0 salen de la boquilla** (`surfaces_infill.md`, I9): la capa de arriba de todo, una
  boquilla; el resto del relleno sólido y el poco denso, 1,125 boquillas (0,45 mm con 0,4). Antes seguían a
  `walls.width`. **Cambia el G-code de siempre** en los rellenos.
- **La pared externa en 0 sale de la boquilla** (`walls.outer_width`, `perimeters.md` P12): 1,125 × la boquilla
  (0,45 mm con 0,4), aunque `walls.width` tenga otro valor.
- **La primera capa lleva su ancho** (`layers.first_line_width`, 200 % de su altura: 0,4 mm con 0,2): paredes,
  rellenos, falda, borde y soporte. Antes, el de las paredes.
- **Torre de caudal máximo:** por defecto de 5 a 25 mm³/s de a 1, con franjas de 2 mm que se leen por su altura; con
  un caudal máximo conocido, de la mitad a una vez y media, también en franjas de 2 mm.
- **Una clave desconocida en `material` es un error**, como en los demás grupos (antes el grupo entero se ignoraba
  con un aviso).
- **Las fechas de las calibraciones van en los perfiles:** las de la impresora en el perfil de la impresora
  (`machine.calibrations`) y las del filamento en el del filamento (`extrusion.calibrations`, con la impresora en que
  se hicieron), en lugar de las preferencias de la interfaz. Las que ya estaban en las preferencias se siguen
  mostrando.
- Los informes de investigación dicen que los métodos que describen se basan en los trabajos citados.

- **Cada lazo de pared termina un poco antes de su inicio** (`walls.seam_gap`, 15 % de la boquilla: 0,06 mm con 0,4;
  `perimeters.md`, regla 5), para que la costura no se engrose. Antes cada lazo volvía a su inicio.
- **El soporte de grilla que apoya sobre la pieza deja una separación** (`support.bottom_z_gap`; sin valor, la de
  `support.z_gap`), como pide `supports_more.md` (regla 6). Antes empezaba pegado a la pieza. Sus primeras capas son
  de interfaz (`support.bottom_interface_layers`; sin valor, tantas como `support.interface_layers`, regla 7).

### Corregido

- **Un modelo grande quedaba con su copia simplificada después de mover la vista una vez** (más de un millón de
  triángulos): la copia de mover la cámara se mostraba para siempre, con menos detalle, porque el programa contaba los
  cuadros solo en las capturas. Ahora vuelve el modelo completo apenas la cámara se detiene.

- **La leyenda de la vista previa tapaba la barra de movimientos**, y tapaba media pieza en ventanas chicas: la barra
  va ahora a la derecha de la leyenda, o adentro de ella si no hay lugar, y un botón («−») pliega la leyenda a los tipos
  de línea solos. En una vista angosta se pliega sola (si completa taparía más del 45 % del ancho), y al encuadrar en
  Vista previa la pieza va entre la leyenda y la barra de capas; al pasar a Vista previa con la cámara sin tocar, se
  reencuadra.

- **Impresión por USB: un trabajo terminado se leía un instante como «imprimiendo al 100 %».** El progreso completo
  llega ahora junto con el fin del trabajo.

- **Los campos «mm o %» ignoraban la unidad escrita** (ancho de cordón, costura, detalle mínimo, separación y ancho
  del soporte, velocidad de la pared externa y otros): «0,3 mm» en un campo en % quedaba como 0,3 %. Ahora «mm» o «%»
  escritos con el número cambian la unidad; un número solo sigue en la unidad que muestra el campo.

- **Límites del panel más estrechos que los del archivo de ajustes:** el desplazamiento XY (±10 mm), la corrección
  de agujeros (±5 mm), la altura de capa (0,01 a 10 mm), la cantidad de paredes (hasta 1000) y la altura del relleno
  combinado se recortaban en silencio. Ahora valen los rangos de `docs/CONFIG.md`, y un valor escrito fuera de ellos
  queda en el límite con un aviso («Máximo: 1000»).

- **El volumen de purga no se podía escribir:** la fila solo decía cuántos valores tenía. Ahora un número vale para
  todos los pares de filamentos, como en el archivo; una lista con valores distintos se sigue editando en el archivo.

- **Etiquetas cortadas en Soportes** con el panel angosto: «Capas con soporte forzado» y «Ángulo de voladizo» (con 0
  el campo dice «Automático»). Una etiqueta que no entra muestra su texto completo al pasar el mouse.

- **Lo escrito en algunos campos se perdía** (el ancho y el fondo de la cama, los porcentajes): el campo tomaba su
  identidad de la dirección de una copia local, que puede cambiar de un cuadro a otro; entonces el campo «cambiaba» a
  mitad de la escritura. Ahora cada fila se identifica por su ajuste.

- **Textos:** «Apagado» también para la piel difusa y el escudo contra corrientes (decían «No»); los resúmenes de
  velocidades llevan la unidad en cada valor («40 mm/s · externa 30 mm/s»).

- **Los consejos tapaban controles:** la tarjeta era apenas transparente y en Vista previa caía sobre la barra de
  movimientos y la leyenda. Ahora es opaca y en Vista previa va arriba; el consejo del laminado se va si ya no hay
  laminado (quedaba después de borrar la pieza).

- **La barra de estado nombraba un objeto borrado:** con un solo objeto en la placa, ahora dice el suyo.
- **Las marcas de costura de la vista previa eran muy grandes:** puntos de 7 px fijos, que de lejos hacían una línea
  blanca gruesa sobre la pieza. Ahora miden lo que el cordón en pantalla (de 1 a 3,5 px).

- **Los campos de la página de la impresora no respondían al mouse** (informe del dueño): la ventana de la vista 3D,
  transparente, quedaba encima de la página y se llevaba los clics. Ahora la página se edita con el mouse.
- **Escribir en un campo numérico no hacía nada:** un clic empezaba a arrastrar el valor en vez de dejar escribir.
  Ahora un clic (sin arrastrar) deja escribir el número, y la coma decimal se lee como punto.
- **Laminar con la página de la impresora abierta** pasaba a Vista previa detrás de la página; ahora la cierra.
- **La búsqueda no encontraba los ajustes de la impresora** con palabras comunes («ancho»): la lista de 10 se llenaba
  con los de impresión. Ahora siempre entra la mejor coincidencia de cada pestaña.
- **Tema claro:** el ícono y el nombre de la sección elegida casi no se veían sobre el violeta; las listas
  desplegables eran apenas transparentes y dejaban ver el texto de atrás.
- **Datos que faltan, mostrados como ceros:** las velocidades, aceleraciones y tirones máximos sin dato de la
  impresora dicen «Sin dato» (el motor ya no los usaba como tope); la capa mínima y la máxima sin valor dicen
  «Automático» (salen de la boquilla); los resúmenes con «—» dicen «Sin dato» o «Sin límite».
- **Nombres de perfiles con coma decimal en español** («0,20 mm», «CR-5 Pro H 0,4 mm»), también en los avisos que
  aparecen abajo al guardar o cambiar un perfil; el archivo guarda el nombre como siempre.

- **La aplicación se cerraba al elegir una impresora del catálogo** en la lista de impresoras (informe del dueño en
  Windows; en Linux, `std::bad_alloc`): agregar la impresora vuelve a leer las listas de perfiles y la tarjeta seguía
  usando un puntero a la lista vieja. Lo mismo al guardar o borrar un perfil desde su fila.

- **Píxeles negros con la oclusión ambiental fuerte:** con la fuerza de la oclusión pasada de 1 (llega a 1,5), la luz
  ambiente salía negativa en los rincones y esos píxeles quedaban negros (861 en la vista previa de un cubo con rayos);
  ahora nunca baja de un poco de luz.
- **Vista previa laminada lenta mirando desde arriba:** las capas se dibujaban de abajo hacia arriba y desde arriba
  cada capa se pintaba y se tapaba con la siguiente; ahora van primero las más cercanas al ojo. Con una esfera de 80 mm
  en Vulkan por software, de 16,3 a 7,1 s por cuadro (en una GPU real los tiempos son mucho menores; la proporción es
  lo que vale).

- **Ray tracing con modelos de más de 4 194 304 triángulos:** la escena se cortaba en silencio y los rayos pasaban por
  donde faltaba, así que la luz salía mal en esas partes. Ahora la vista avisa que la escena es demasiado grande para
  el ray tracing, como cuando no entra en la GPU.

- **Números en la interfaz** (revisión de traducción del equipo de fichas, TR1 a TR5):
  - en castellano, coma decimal en todos lados: los campos, la vista previa, la barra de estado y la calibración de la
    impresora;
  - en inglés, la coma para los miles («4,013»);
  - la ficha de información en inglés dice «0 to 100»;
  - «1 bead takes the change in width»;
  - «%d per layer» en el análisis, y su comando en una línea propia.
- **Recorrido de bienvenida:** el contador sigue el número del título («2 / 6» en «2. Elegí impresora…»).
- **Vista previa:** la altura de la capa de arriba ya no queda tapada por el «+» de la barra de capas.

- **`layers.first_line_width`** (lo encontró el equipo de fichas, BH1): con 20 mm o más en una boquilla de 0,4 el
  G-code salía del área permitida y la exportación fallaba con el código 6. Ahora, como `walls.width`, tiene que estar
  entre un cuarto y cuatro veces el diámetro de la boquilla, y el mensaje lo dice al revisar los ajustes.

- **Verificación del G-code con limpieza:** un paso de la limpieza tan corto que su E redondeaba a la del anterior
  cortaba la retracción en dos, y la verificación rechazaba el archivo («retracts again without undoing the last
  retraction»; lo encontró el fuzzer). El archivo estaba bien; ahora la verificación también lo acepta.

- **Franjas de calibración:** un valor fuera de rango (por ejemplo una temperatura de 1000 °C) se escribía en el
  G-code; ahora se rechaza al leer los ajustes, con el mismo rango que un cambio por altura.

- **Orden de los trazos:** dos comienzos igual de cerca (por ejemplo los dos extremos de una línea corta en una isla
  simétrica, vistos desde su eje) se elegían por el ruido de las cuentas; la misma pieza movida en Z daba otro
  G-code. Ahora las distancias a menos de 1 nm empatan y gana siempre el mismo.

- **Capas curvas:** el primer viaje de la impresión terminaba a la Z plana de la capa aunque la capa fuera curva ahí;
  la primera extrusión arrancaba fuera de la superficie y el G-code no pasaba la verificación (lo encontró el
  fuzzer).

- **Un resto de filamento ya no salta con la boquilla:** lo que sobraba de un pedazo de cordón demasiado corto para
  escribirse se sumaba al primer tramo después de un viaje, en otro lugar de la pieza (lo encontró el fuzzer).

- **Capas curvas:** donde la rampa de las caras de arriba termina, un tramo podía llevar el filamento del espesor de
  un lado y el verificador leer el del otro, y el G-code se rechazaba. El espesor se toma ahora donde el G-code pone
  la boquilla, con sus números redondeados.
- **Rangos de alturas que se pisan:** pasado el de después, el de antes vuelve a su altura de capa (antes volvía la
  del perfil).
- **Mensajes:** un volumen negativo que se lleva toda la pieza lo dice (antes: «¿la malla es plana o abierta?»); un
  rango con ajustes que no son JSON nombra el objeto y el rango («box 2-4.5 mm»).
- **Más rápido:** escribir el G-code con `walls.detect_overhangs` en piezas con voladizos todo alrededor (una
  esfera de un millón de triángulos: de 300 a 177 ms), y el clic de Medir sobre una superficie curva en mallas
  grandes (de 0,87 a 0,18 s con un millón de triángulos).

## [0.10.0-alpha.2] - 2026-10-27

### Agregado

- **Espejar y Cortar en la placa** (ficha `model_tools.md`): Espejar en X, Y o Z en la herramienta Escalar, y la
  herramienta Cortar (C) con un plano horizontal que deja dos piezas cerradas, Separar en piezas, y Medir (U) distancias y ángulos entre caras y vértices, el diámetro de un agujero o un cilindro
  y el ángulo de un cono con un clic, y escalar la pieza a la medida que debería tener.
- **Escalar, girar, cortar y separar desde la línea de comandos** (ficha `model_tools.md`; grupo `transform`): escala o
  porcentaje, encajar en una caja, giros en X, Y y Z, un corte horizontal que deja dos piezas cerradas y separar los
  pedazos sueltos de una malla, cada uno como un objeto ordenado en la cama.
- **Panel de ayuda** (ficha `assistant.md`): F1, el botón **?** o Más › Ayuda abren la ayuda con sus pestañas y su
  buscador: problemas de impresión con sus causas y arreglos que se aplican con un clic (y se deshacen), procedimientos
  paso a paso, tutoriales guiados que iluminan cada parte de la interfaz y siguen solos cuando la persona hace el paso,
  y el glosario, que también aparece en el (i) de cada ajuste. La pestaña Material muestra la ficha del
  material elegido y avisa lo que la impresora no tiene para él. Los textos llegan con el paquete de perfiles.
- **Créditos y fuentes** (ficha `credits.md`): las fichas cuya función sale de un artículo o un proyecto dicen al pie
  «Basado en…», con un enlace a la fuente, y **Más > Créditos y fuentes** junta todas las fuentes y los componentes de
  terceros con su licencia y el texto de su aviso.

- **Resina** (ficha `resin.md`; grupo `resin` y `machine.display_*`, en Material › Resina, Impresora › Máquina ›
  Pantalla de resina y Otros › Resina: capas): `slice` con salida `.goo` lamina para impresoras de resina con pantalla
  y escribe el archivo GOO de Elegoo. Cada capa es una imagen en grises con el área exacta que la sección cubre de
  cada píxel; las capas de fondo se exponen más y se achican contra el pie de elefante; hay corrección de escala,
  corrección absoluta del contorno, corrección de Z bajo lo que cuelga y gamma de los bordes. Por defecto, una Elegoo
  Mars 5 con resina estándar. Avisa de las ventosas (huecos que el film sella al despegar cada capa) y dice cómo
  arreglarlas. El motor trae además la torre de exposición y el bloque de medidas de resina (todavía sin menú).
- **Impresoras de resina en la interfaz** (Impresora › Máquina › Tipo de impresora): con «Resina (MSLA)» el panel
  muestra solo los ajustes de resina, Laminar hace las imágenes de las capas, la vista previa muestra la pieza con
  sus soportes y su base capa por capa, «Exportar GOO» escribe el archivo, y Calibrar ofrece la torre de exposición
  (cuya franja elegida pasa a la exposición) y el bloque de medidas (que calcula la corrección de escala).
- **Soportes, base y vaciado de resina** (ficha `resin_supports.md`, en Otros › Resina: soportes, base y vaciado):
  la pieza queda elevada sobre pilares con puntas finas en las caras que miran hacia abajo, sobre una base pegada a
  la plataforma (o con la base alrededor de la pieza apoyada); puede vaciarse con paredes de un espesor mínimo y el
  hueco redondeado, con barras interiores que se sostienen solas; el motor acepta agujeros de drenaje. Los soportes
  y la base vienen activados, como en Tisma Slicer. El peso y el costo de la resina salen del frasco
  (`resin.bottle_*`).
- **Relleno ondulado entrelazado** (ficha `interlock_infill.md`; `infill.waves`, en Impresión › Relleno › Relleno
  ondulado): el relleno poco denso sube y baja como un cartón de huevos, igual en cada capa, para que las capas
  encajen en vez de apoyarse en planos lisos. Funciona con cualquier patrón.
- **Vista previa más completa** (ficha `preview.md`): 13 maneras de colorear (velocidad alcanzada, ventilador,
  temperatura, caudal, tiempo de capa…), la leyenda por tipo con tiempo, porcentaje y filamento, una fila de íconos
  para ver retracciones, costuras, cambios, pausas, el centro de gravedad y la boquilla, el deslizador de capas con dos
  manijas y el «+» para poner un cambio de color, una pausa o G-code en una capa, el deslizador de movimientos, la
  vista ortogonal (K) y las teclas 0 a 6, Z, E y L. El visor abre también G-code binario.
- **Tarjeta de la impresora** arriba del panel: la impresora con su dibujo y una lista con buscador (también las del
  catálogo, por marca), la cama en las impresoras que traen varias y la boquilla, que se cambia con un clic y se puede
  deshacer. El engranaje abre la **configuración de la impresora** en una página aparte, con su sección de
  **Calibración** (qué está calibrado, cuándo y con qué boquilla y filamento), la conexión y notas.
- **Buscar un ajuste** (Ctrl+F, en la barra superior): por su nombre, su tarjeta, su clave o lo que hace; lleva al
  ajuste y lo resalta.
- **Panel de ajustes:** el nivel **Personalizado**, con «Elegir ajustes…», el ojo para quitar un ajuste de la vista,
  «+ Agregar ajuste» y la vista exportable a otra computadora; **Ver todo junto**; y **Restablecer** en el menú de cada
  tarjeta.
- **Impresoras de cinta** (ficha `belt_printer.md`; grupo `belt`, en Impresora › Máquina › Impresora de cinta): para
  impresoras con el pórtico inclinado sobre una cinta (BlackBelt, Creality CR-30, IdeaFormer IR3). Se lamina en
  planos paralelos al pórtico y el G-code va en los ejes de la máquina, con la cinta en Z o en Y; la vista previa
  muestra la pieza sobre la cinta con las capas inclinadas.
- **Capas curvas** (ficha `nonplanar.md`, en Impresión › Otros › Capas curvas e Impresora › Máquina › Cabezal): capas
  en onda (huevera, crestas, crestas torcidas) o en cono para una impresora común de 3 ejes, tan curvas como deja la
  cabeza, con el material según el espesor de cada punto; necesitan E relativa. Con `surface`, las capas de arriba
  quedan paralelas a las caras de arriba inclinadas y no se ven escalones; con `overhangs` o el cono ajustado, las
  capas bajan bajo los voladizos y sacan el soporte que necesitarían; `automatic` hace las dos cosas.
- **Mezcla de colores** (ficha `color_mixing.md`): extrusores virtuales que alternan filamentos capa por capa para
  dar colores nuevos, con su diálogo (lista, mezclas de dos colores, editor con deslizador y secuencia de capas) y
  degradés por altura; cada pieza elige su extrusor, real o virtual, con la herramienta «Extrusor y color del
  objeto», y el proyecto guarda todo.
- **Boquillas distintas** (ficha `multimaterial.md`, en Impresora › Boquilla y Otros › Boquillas distintas y colores):
  cada extrusor con su boquilla; los anchos de las líneas siguen a la de cada extrusor, y «paredes finas, relleno
  grueso» imprime el relleno con la boquilla gruesa en capas combinadas. La mezcla de colores se ofrece con aviso de
  costo en las impresoras de una sola boquilla, y después de laminar se avisa cuánto filamento va a la purga.
- **Vigas entre materiales** (ficha `multi_extruder.md`, en Otros › Entrelazado entre materiales): donde se tocan
  dos materiales, cada uno entra en el otro con vigas que cambian de dirección cada pocas capas.
- **MMU de un extrusor** (ficha `multi_extruder.md`, en Impresora › MMU y Material › Carga y descarga en la MMU): cada
  cambio comprime la punta sobre la torre, descarga el filamento hasta el tubo de enfriamiento, lo enfría con idas y
  vueltas, cambia, carga el nuevo y purga.
- **Torre de purga** (ficha `multi_extruder.md`, en Otros › Torre de purga): los cambios de herramienta se hacen sobre
  una torre al costado de la pieza, con su posición, giro, ancho y borde; con una MMU de un extrusor cada cambio purga
  el volumen que pide ese par de filamentos (`wipe_tower.purge_volumes`, por el multiplicador de cada filamento); con
  varias boquillas, la nueva se ceba. Una torre fuera de la cama o sobre la pieza se rechaza. Puede no imprimir las
  capas sin cambio (la boquilla baja hasta la torre), y la purga puede ir primero al relleno o a todo un objeto, con
  un mínimo en la torre.
- **Varios extrusores** (ficha `multi_extruder.md`): impresoras de varias boquillas con `machine.extruders`; cada
  objeto se imprime con su extrusor y cada tipo de extrusión puede ir con otro (perímetros, relleno, relleno sólido,
  soporte, interfaz). Cada capa empieza con el extrusor con el que terminó la anterior, un cambio por extrusor; el
  cambio retrae su largo propio, corre el G-code de cambio y vuelve el filamento con su extra. Temperaturas por
  extrusor al empezar, espera con menos temperatura para los que no imprimen, cama del extrusor que corresponde,
  desfase de cada boquilla y capas sólidas entre materiales opcionales. En Impresora › Boquilla › Extrusores y Otros ›
  Varios materiales.
- **Etiquetas de objetos** (ficha `gcode_firmware.md`, en Impresora › G-code › Firmware): cada objeto marcado en el
  G-code para cancelar uno solo durante la impresión, con `M486` (Marlin y RepRapFirmware), `EXCLUDE_OBJECT`
  (Klipper) o los comentarios de OctoPrint.
- **Impresión secuencial** (ficha `sequential.md`, en Otros › Modos especiales): con varios objetos en la placa, cada
  uno se imprime entero antes del siguiente, con lugar para el cabezal entre ellos (Impresora › Cama y volumen) y G-code
  propio entre objetos; los objetos que el cabezal o el eje X chocarían se rechazan. Es la forma de imprimir varios
  objetos en modo vaso. Desde la línea de comandos, `slice --copies N` pone N copias del modelo.
- **Más opciones de paredes** (ficha `walls_extra.md`, en Resistencia › Más opciones de paredes): costuras internas
  escalonadas, un solo perímetro sobre las superficies de arriba (en todas o en la de más arriba), un solo perímetro
  en la primera capa y perímetros extra que llenan los voladizos sostenidos de un solo lado.
- **Retracción y viajes completos** (ficha `retraction_travel.md`): velocidad y extra de la vuelta del filamento,
  retraer en todo cambio de capa, barrido mientras retrae (con una parte quieta antes), salto en Z solo por encima o
  por debajo de una altura, salto en rampa con su pendiente y su altura, retracción del firmware con `G10`/`G11`,
  viajes rectos que cruzan las paredes o con un desvío máximo, lazos de pared en sentido horario y valores propios del
  material para la retracción y los viajes. En Material › Retracción y Viajes, Velocidad › Viajes y Resistencia ›
  Paredes.
- **Perímetros en voladizo** (ficha `walls_extra.md`, regla 8; `walls.detect_overhangs`, encendido por defecto, en
  Resistencia › Paredes): los tramos de pared que quedan enteros en el aire se marcan `Overhang perimeter`, van a la
  velocidad de los puentes y con su ventilador. En el Benchy, 218 mm en 34 capas.
- **La primera capa sobre el soporte** (ficha `supports_skirt.md`, regla 4; prueba de caja negra QA13): sobre el
  soporte, las paredes y el puente se hacen tan gruesos como la capa más la separación en Z (0,4 mm con capas de 0,2),
  así llenan el hueco hasta el soporte en vez de caer en el aire; el puente entra 0,6 mm sobre la parte apoyada para
  anclarse. La capa de arriba ya es normal.

- **Arcos en voladizos** (pedido del dueño; ficha `arc_overhangs.md`; `arcs.enabled`, apagado por defecto, en
  Soportes › Arcos en voladizos): un voladizo que sale por un solo lado se imprime sin soporte con arcos que crecen
  alrededor de un centro 1,2 mm adentro de lo apoyado, cada uno pisando al anterior, en vaivén, lentos (5 mm/s), con
  cordón redondo, ventilador al máximo y retracción entre arcos; la capa de arriba lo cubre sólida. Los agujeros cortan
  los arcos y las tiras angostas se llenan a lo ancho. Los puentes (apoyados de dos lados) siguen siendo puentes. En el
  estante de la ficha, 598 mm de arcos (la ficha: 606).

- **Revisión de estabilidad** (fichas `stability.md` y `print_review.md`; prueba de caja negra QA8): en piezas sin
  soportes, avisa de partes que empiezan en el aire (crítico), de lo impreso que se vuelca fuera de su apoyo, de
  columnas finas que se doblan con el empuje de la boquilla y de la pieza que se despega de la cama. El empuje y la
  adherencia se miden (`review.nozzle_force`, `review.bed_adhesion`, en la pestaña Impresora › Máquina ›
  Estabilidad); sin medir, informa las tres columnas más flexibles. La aguja de 1 × 1 × 80 mm de QA8 ahora sale.
  Cada aviso del JSON lleva su gravedad (`severity`).

- **Paredes más finas que la boquilla** (ficha `perimeters.md`, regla 3; prueba de caja negra QA9;
  `walls.min_line_width`, 85 % de la boquilla, y `walls.min_detail`, 25 %, en Resistencia › Paredes): una pared de un
  solo cordón más fina que el ancho mínimo se imprime con un cordón de ese ancho, centrado; más fina que el detalle
  mínimo, no. Una pared de 0,2 mm, que antes daba «nothing would be printed», sale con un cordón de 0,34 mm. En el
  Benchy, las capas con detalles perdidos pasan de 9 a 3; en el cráneo escaneado, de 31 a 8.

- **Ancho de las líneas del soporte** (`support.width`, ficha `supports_more.md`, regla 18; en Soportes › Patrón): en
  mm o en % de la altura de capa; 0, el de las paredes, como hasta ahora. Vale para la base, la interfaz y los árboles.

- **Velocidades propias** (ficha `speeds_accels.md`; en la sección Velocidad): `speed.bridges` para puentes y
  perímetros en voladizo, `speed.over_bridges` para el relleno sólido justo encima de un puente,
  `speed.first_layer_infill` para el relleno de la primera capa (puede pasar la velocidad de la primera capa),
  `speed.gap_fill` y `speed.travel_z` para los movimientos en Z. Sin valor, la de siempre.
- **Aceleraciones por tipo** (ficha `speeds_accels.md`; grupo `acceleration`, en Velocidad › Aceleraciones): una
  para cada tipo de extrusión, la primera capa y los viajes (también los cortos), con `M204` según el firmware.
- **Paredes más lentas en los voladizos** (`speed.dynamic_overhangs`, en Velocidad › Voladizos): cuanto menos de su
  cordón se apoya en la capa de abajo, más lenta va la pared, entre `speed.overhang_0` y `_3`.
- **Ecualizador del caudal** (`speed.flow_slope_up` y `speed.flow_slope_down`, en Velocidad › Topes): el caudal
  cambia de a poco entre una extrusión y la siguiente, en rampas de hasta 16 pedazos.
- **Paredes de ancho fijo con relleno de huecos** (ficha `walls_extra.md`; `walls.generator` `fixed` y
  `walls.gap_fill`, en Resistencia › Paredes): lazos todos del mismo ancho, y lo que queda entre ellos se llena con
  cordones de relleno de huecos a `speed.gap_fill`.
- **Planchado** (ficha `ironing.md`; grupo `ironing`, en Calidad › Planchado): la boquilla vuelve a pasar sobre las
  superficies de arriba con muy poco material en líneas muy juntas, para alisarlas; todas, solo la más alta o todas
  las capas sólidas.
- **Balsa** (ficha `skirt_brim_raft.md`; `adhesion.raft_layers` y sus ajustes, en Adhesión › Balsa): capas de
  soporte debajo de toda la pieza, base e interfaz, que la levantan de la cama; la primera capa de la pieza con su
  propio tope de velocidad y aceleración.
- **Cambio de color y pausas por altura** (ficha `gcode_templates.md`; `color_change` y `pause` en `changes`, con
  `gcode.color_change` y `gcode.pause`), y **G-code antes y después de cada cambio de capa** (`gcode.before_layer`,
  `gcode.after_layer`, con `layer_num` y `layer_z`), en Otros › Plantillas de G-code.
- **Peso y costo del filamento** al final del G-code (ficha `gcode_extras.md`; `extrusion.density`, `extrusion.cost`,
  en Filamento › Precio): `; filament used [mm]`, `[cm3]`, `[g]`, `; filament cost` y los totales.
- **Espera de la cámara** (`extrusion.chamber_min_temperature`): la cámara se pide antes del G-code de inicio y, después,
  se espera a que llegue a ese mínimo (`M191`).
- **Desfase de Z** (`machine.z_offset`) y **E volumétrica** (`gcode.volumetric_e`, E en mm³).
- **Contracción del filamento** (`extrusion.shrinkage_xy` y `_z`, en Filamento › Contracción): la pieza se lamina más
  grande para que, al contraerse, quede de la medida pedida.
- **G-code binario** (`gcode.binary`, `.bgcode`; ficha `gcode_extras.md`): el formato binario público, sin comprimir
  todavía, con sus bloques de datos y CRC32; se verifica antes de darle el nombre.
- **Lenguaje de plantillas de G-code** (ficha `gcode_templates.md`): en el inicio, el fin, los cambios de capa y de
  color, cuentas, textos, condiciones (`{if}`), funciones, variables propias y todos los ajustes por su nombre; un error
  (por ejemplo una variable desconocida) hace fallar la exportación diciendo la línea y la columna. Inicio y fin del
  filamento (`extrusion.start_gcode`, `end_gcode`), temperaturas automáticas con la cama (`gcode.auto_temperatures`),
  nombre del archivo con variables (`output.filename_format`, con `slice -o carpeta`) y scripts de posproceso
  (`output.post_process`, con confirmación o `--yes`).
- **Falda, escudo y bordes** (ficha `skirt_brim_raft.md`, en Adhesión): largo mínimo de filamento de la falda
  (`adhesion.skirt_min_filament`), escudo contra corrientes (`adhesion.draft_shield`, la falda sube con la pieza),
  borde afuera, en los agujeros o en los dos (`adhesion.brim_type`) y separado de la pieza (`adhesion.brim_gap`).
- **Límites de la máquina y firmware** (ficha `gcode_firmware.md`): aceleraciones por eje y de retracción, tirones o
  desviación de unión y avances mínimos (`machine.*`, en Impresora › Límites), que la estimación del tiempo usa y que,
  con `machine.limits_usage` `emit`, se escriben al empezar (Marlin 1 y 2, RepRapFirmware); la temperatura de la
  boquilla antes y después del G-code de inicio; `G92 E0` en cada capa con E relativa en Marlin; `M73` con el
  porcentaje y los minutos restantes (`gcode.progress`); sabores `marlin_legacy` y `reprapfirmware`.
- **Modo silencioso** (`machine.silent_mode`, `machine.silent`): una segunda estimación con sus límites, y la del
  tiempo de la primera capa.
- **Arcos `G2`/`G3`** (`gcode.arcs`, en Otros › Salida del G-code): los tramos que siguen un círculo se escriben
  como arcos; el G-code de una pieza redonda pesa hasta cuatro veces menos. La vista previa y el verificador los leen.
- **Firmwares antiguos** (`machine.flavor`, ficha `gcode_extras.md`): `repetier`, `smoothie`, `teacup`, `makerware`,
  `sailfish`, `mach3`, `machinekit` y `no_extrusion` (solo movimientos), cada uno con sus comandos.
- **Comentarios detallados** (`gcode.verbose_comments`, en Otros › Salida del G-code): cada línea que escribe el motor
  dice qué hace (`; perimeter`, `; retract`, `; travel to first layer point`…).
- **Topes del relleno por filamento** (`extrusion.infill_max_speed` y `extrusion.infill_max_crossing_speed`, en
  Filamento › Flujo): el relleno disperso no pasa de ese tope, uno para los patrones cuyas líneas se cruzan en la capa y
  otro para los demás.

### Corregido

- Con varios extrusores, el relleno combinado (`infill.combine_layers`) se perdía.
- La compresión de la MMU podía ir más rápido que la extrusión más rápida configurada.
- El laminado ya no se cuelga cuando una región de paredes trae anillos sin área (pasaba con «un solo perímetro arriba»
  en una esfera), y el reparto de los cordones de ancho variable es lineal en vez de cuadrático o peor en esos casos.
- Las pruebas de los arcos en voladizos (A1 a A4) volvieron: el commit de los arcos `G2`/`G3` había escrito las suyas
  encima.

- En el modo vaso, después de G-code del usuario que mueve el cabezal, la vuelta siguiente salía desde donde el cabezal ya no estaba.
- El verificador del G-code no contaba los cordones finos del planchado al calcular el avance de un flujo forzado.
- Las etiquetas de las velocidades en voladizos tenían un «%» que la prueba de textos leía como formato.
- El verificador del G-code rechazaba un G-code válido cuando una velocidad nueva (puentes, relleno de huecos, sobre
  los puentes, relleno de la primera capa) era más rápida que todas las demás.

### Cambiado

- Una variable desconocida en el G-code de inicio o fin ya no queda escrita tal cual: es un error y no se escribe el
  G-code (`gcode_templates.md`, regla 8). Un perfil de impresora del catálogo necesita el de filamento, que da
  `temperature` y `bed_temperature`.

- **Puentes con cordón redondo** (fichas `gcode_motion.md`, regla 1, y `speeds_accels.md`, regla 14): los puentes y
  los tramos de pared sobre el aire se extruyen como un cordón redondo del ancho de la boquilla (con 0,4 mm, 0,0522 mm
  de filamento por mm), y las líneas del puente van a esa distancia; `extrusion.bridge_flow_ratio` (1 por defecto)
  achica la sección y junta las líneas.
- **Soportes en árbol más parecidos a la ficha** (`supports_more.md`, SM13 remedido): una capa bajo las puntas, las
  vecinas se enlazan de a pares, con el soporte entre las dos, y se acercan hasta juntarse; las ramas juntadas
  engordan menos. En el estante de la ficha, las piezas por capa bajando: 21, 11, 12, 10, 5, 5 y 4 (la ficha: 22, 13,
  11, 9, 7, 4 y 1; antes 21, 21, 19, 15, 8, 7 y 3).

- Sin cama en los ajustes, `placement.center` dado a mano mueve la pieza igual (ficha `placement.md`, PL15); sin él,
  la pieza sigue donde viene.

### Corregido

- Una rama de árbol que se juntaba con otra que después se apoyaba en la pieza dejaba su pedazo en la capa de
  separación, pegado a la pieza.
- El G-code no escribe un ancho de línea sin un movimiento que lo use (`;WIDTH:0.000` en la punta de un cordón que se
  afina a nada, que la verificación rechazaba).

## [0.9.0-alpha.1] - 2026-10-27

Cuarta versión publicada. Trae los soportes en árbol, la lectura de OBJ y 3MF con la pieza centrada en la cama, la
costura en bufanda, el modo vaso, el panel de ajustes por secciones, tarjetas y niveles, las herramientas de la placa,
pintar soportes, costuras y piel difusa sobre el modelo, la ayuda en la interfaz y la vista 3D con Vulkan. El soporte en
grilla sigue ahora la ficha (margen, separación y líneas), el ventilador va según el tiempo de capa, la falda y el borde
como pide la ficha, y las velocidades salen del perfil de la impresora. Laminar y armar el G-code es bastante más rápido
(capas sólidas de abajo, piezas prismáticas, soportes, costura alineada, G-code en paralelo). Sigue siendo una alfa: los
formatos de ajustes, perfiles, proyectos e informes todavía pueden cambiar entre versiones 0.x, y cada cambio se anota
acá.

### Agregado

- **OBJ y 3MF** (ficha `formats.md`): la línea de comandos y la interfaz abren modelos OBJ y 3MF además de STL. Del
  3MF se toman los objetos de la placa con su ubicación, sus partes, la unidad del archivo (un 3MF en pulgadas sale
  25,4 veces más grande) y las mallas guardadas en otros archivos del paquete, como los de Bambu Studio y OrcaSlicer;
  los modificadores de un proyecto de laminador no se imprimen. `info` dice cuántos objetos y partes trae.
- **La pieza en el centro de la cama** (ficha `placement.md`): con una cama definida, la línea de comandos centra la
  pieza en ella, o en `placement.center`, venga donde venga en el archivo; `placement.arrange` en `false` la deja
  donde está.
- **Soportes en árbol** (pedido del dueño; ficha `supports_more.md`, reglas 15 a 17): `support.style` en `organic`,
  en Soportes › Soporte del panel, con sus ajustes en la tarjeta Árbol. Bajo lo que cuelga va una interfaz densa
  entera, y debajo puntas redondas que bajan como ramas, se juntan entre ellas y engordan hasta troncos sobre la cama;
  esquivan la pieza inclinándose, y donde no pueden se apoyan en ella. Tocan la pieza en menos lugares que la grilla y
  se sacan más fácil.
- **Modo vaso** (pedido del dueño de una pared en espiral; ficha `vase_mode.md`): `vase.enabled`, en Otros › Modos
  especiales del panel. La pieza sale como una sola pared que sube en espiral, sin costura, sobre capas de abajo
  planas (la última, superficie de arriba). Cada vuelta empieza donde terminó la anterior, sin viaje ni retracción, y
  sube una capa con el caudal de siempre. No se puede con soportes ni con varios objetos.
- **Costura en bufanda** (pedido del dueño; ficha `walls_extra.md`, regla 4): `walls.scarf` en `contours` o
  `everywhere` hace que la pared externa empiece en una rampa que sube desde la capa de abajo, en vez de un escalón,
  y que al final del lazo pase otra vez sobre la rampa con el cordón cada vez más fino: la costura casi no se ve, con
  el mismo filamento. Con `walls.scarf_entire_loop`, la rampa da la vuelta entera y la pared sube en espiral. Ajustes
  del largo, la altura inicial, el tramo máximo, solo en lazos lisos (por defecto) y también en las paredes internas.
- **Relaminar por etapas:** si después de laminar se cambia solo una velocidad, una temperatura, el ventilador, la
  retracción u otro ajuste que solo afecta al G-code, el programa no vuelve a cortar la pieza: rehace solo el G-code.
  En el Benchy, 0,4 s en vez de 1,4 s, con el mismo G-code byte a byte. La barra de estado dice «G-code rehecho».
- **Sombra de la pieza sobre la cama** en el modo de oclusión ambiental.
- **Ranuras entre capas en la vista previa** (pedido del dueño): cada capa de una pared se ve como un tubo, con su
  ranura oscura contra la de arriba, calculada sobre el perfil del cordón; y los colores saturados ya no se lavan hacia
  el blanco.
- **Ventana y vista 3D con Vulkan** (decisión del dueño; `PLAN.md`, «Vista con Vulkan»): `--graphics
  auto|vulkan|opengl` elige qué dibuja la ventana. Con Vulkan, los paneles y la vista 3D (cama, modelo, pintura, vista
  previa, MSAA y FXAA) los dibuja Vulkan, igual que OpenGL imagen contra imagen, con la luz rápida. La oclusión
  ambiental y el ray tracing llegan en el paso siguiente, así que por ahora el modo automático (el de siempre) sigue con
  OpenGL. Si la máquina no tiene Vulkan, o no puede mostrar la ventana con él, la ventana sigue con OpenGL y lo dice.
- **Tiempo de la vista y FPS** en la barra de estado: lo que tarda la GPU en dibujar la vista 3D en cada cuadro, para
  medir en cada máquina (`docs/LARGE_MODELS.md`). `--bench-frames N` lo mide para las capturas.
- **Herramientas de la placa** (pedido del dueño; `docs/UI.md`, «Herramientas de la placa»): varios modelos en la placa
  y una columna con **Agregar**, **Quitar**, **Duplicar**, **Mover** (también arrastrando el modelo), **Rotar**,
  **Escalar** (en % o en mm), **Apoyar en una cara** y **Organizar** (grilla compacta centrada, ficha `placement.md`).
  Atajos M, R, S, F, A y Supr. Soltar un STL sobre la placa lo suma; los proyectos guardan todos los modelos.
- **Pintar sobre el modelo** (pedido del dueño; `docs/PAINTING.md`): herramienta **Pintar** (P) con un pincel redondo.
  - **Soportes:** forzar o bloquear (ficha `supports_more.md`, reglas 3 a 5). Sobre una cara forzada lo que cuelga
    lleva soporte aunque la pendiente no lo pida; sobre una bloqueada, nunca. Ajuste nuevo `support.automatic`: en
    `false`, solo hay soporte bajo lo forzado.
  - **Costura:** forzada, la pared empieza en lo pintado (en el medio de una franja, con la costura alineada); bloqueada,
    afuera.
  - El pincel corta los triángulos grandes en pedazos chicos sin tocar la malla, y la pintura sigue al modelo cuando se
    mueve, gira o escala. Los proyectos la guardan (parte `PNT `). Los colores llegan con varios extrusores.
- **Ayuda en la interfaz** (pedido del dueño; `docs/UI.md`, «Ayuda»):
  - un asistente de siete pasos la primera vez, y desde **Más > Tutorial**;
  - un consejo la primera vez que se usa cada función (las herramientas de la placa, Pintar, el laminado y cada
    pestaña), que se puede apagar;
  - un botón (i) al lado de cada ajuste: qué hace, su clave, su valor por defecto y su rango, leídos de
    `docs/CONFIG.md`.
- **Piel difusa** (ficha `fuzzy_skin.md`; `fuzzy.mode`, `fuzzy.thickness` y `fuzzy.point_distance`, en la interfaz bajo
  Costura): la pared externa pasa por puntos nuevos cada 0,4 a 1,2 mm, corridos al azar hasta 0,3 mm hacia afuera o
  hacia adentro, y la superficie queda rugosa. `external` deja lisos los agujeros, `all` también los hace rugosos; las
  paredes internas no cambian. También se pinta (Pintar > Piel difusa): donde se pintó, se aplica aunque esté apagada.
- **Zonas de relleno chicas, sólidas** (`infill.solid_below_area`, 70 mm², de la ficha `surfaces_infill.md`; en la
  interfaz, sección Relleno): una isla del relleno de una capa más chica que eso (una pata fina, un perno) se llena
  sólida. Cambia las piezas chicas: un cubo de 10 mm con 3 paredes queda sólido entero, y una placa de 12 islas
  chicas lleva 60 % más de material (35 → 53 min). 0 lo apaga.

### Cambiado

- **Soporte en grilla como pide la ficha `supports_skirt.md`** (segunda prueba de caja negra, QA10 a QA12): la zona
  llega 1,6 mm más allá de lo que cuelga, así los pedazos chicos de una pieza orgánica se juntan en una zona firme; la
  separación con la pieza es por defecto la mitad del ancho de las paredes (`support.xy_gap` acepta mm o un
  porcentaje); la base va en líneas en una sola dirección (`support.line_angle`, nuevo) a `support.spacing` más un
  cordón, y la interfaz cruzada a 90°, a la separación de los cordones.
- **Panel de ajustes nuevo** (pedido del dueño: entre Cura y OrcaSlicer, simple y sin cargar de información): todos
  los ajustes del motor, también los que no tenían lugar (bufanda, compensación, corte, soportes, ventilador, límites
  de la máquina), en pestañas Impresión, Material e Impresora; cada una con sus secciones en una columna de íconos
  (Calidad, Resistencia, Velocidad, Soportes, Adhesión, Otros…) y tarjetas plegables que, cerradas, dicen lo
  importante en una línea. Tres niveles: Básico, Normal y Avanzado (Ctrl+Alt+1, 2 y 3). Lo cambiado respecto del
  perfil se marca en violeta y ningún nivel lo esconde. El nivel no cambia el G-code.
- **Costura alineada más rápida:** en piezas de superficie curva con muchos puntos por pared, armar el G-code con la
  costura alineada (la de siempre) tardaba tres veces más que con `nearest`; ahora tarda lo mismo. Una gota de 172 080
  triángulos lamina en 2,7 s en vez de 3,8 s, y el cráneo escaneado en 4,7 s en vez de 4,9 s, con el mismo G-code.
- **Soportes más rápidos** (pedido del dueño): la columna del soporte se calcula en una grilla de bits, en la CPU o en
  la GPU con el mismo resultado. Un cráneo escaneado de 546 684 triángulos lamina en 5,8 s en vez de 12,9 s, y el
  Benchy con soportes en 1,18 s en vez de 1,38 s. El borde de afuera del soporte queda a menos de 0,05 mm de antes,
  algo más grande, y se simplifica a medio cordón; la separación con la pieza no cambia.
- **Piezas huecas más rápidas:** sin relleno (`infill.density` 0), las capas sólidas de abajo ya no calculan los
  cordones de la capa entera para saber si son puentes. Una estrella hueca de 160 mm: el corte de 0,90 s a 0,62 s;
  el Benchy sin relleno, de 1,30 s a 1,04 s. El mismo G-code.
- **El G-code se arma y se verifica en paralelo** (V5 del informe de velocidad): mientras se escribe una capa se
  ordenan las siguientes, y la verificación lee el archivo por bloques. El mismo G-code byte a byte. El Benchy pasa de
  1,13 s a 1,00 s.
- **Lo que dejan las paredes se simplifica** como los contornos (V9): el Benchy, de 1,04 s a 0,98 s. El relleno se
  mueve menos que `slicing.simplify_tolerance` (0,01 mm); las paredes no cambian.
- **Piezas prismáticas mucho más rápidas** (V10 del informe de velocidad): las capas con la misma sección que la de
  abajo (un cilindro, una caja, una extrusión) ya no se cortan ni se calculan de nuevo; se copian la región, las
  paredes, las superficies y el relleno. Un cilindro de 32 768 lados y 40 mm de alto: de 2,7 s a 0,36 s; de 80 mm, de
  5,3 s a 0,63 s (0,29 s y 0,36 s con lo de arriba). En los cilindros, cada capa repite ahora exactamente la sección de la primera (antes la
  simplificación elegía otros puntos en cada capa); las paredes se mueven menos de 0,02 mm. El resto de las piezas da
  el mismo G-code byte a byte.
- **Vista previa más rápida con modelos grandes** (pedido del dueño; `docs/LARGE_MODELS.md`): con la pieza entera a la
  vista, las paredes internas y el relleno que la pieza tapa por todos lados ya no se dibujan; la imagen es la misma.
  En un cilindro de 60 × 80 mm se dibuja un 71 % menos y el cuadro tarda 3,7 veces menos; en una esfera, el peor caso
  (cada capa es un escalón), 1,5 veces menos. Un corte, una capa sola o un tipo oculto en la leyenda los dibujan.
- **Solo la capa de arriba de todo es «Top solid infill»** (ficha `surfaces_infill.md`, regla 2): las otras capas
  sólidas de arriba son relleno sólido, a su velocidad y en líneas comunes. Antes las tres iban a la velocidad de la
  superficie de arriba.
- **Ventilador por tiempo de capa** (ficha `cooling.md`; prueba de caja negra del equipo de fichas): el ventilador va
  al máximo en las capas que tardan `cooling.min_layer_time` o menos y baja en línea recta hasta `cooling.fan_min` en
  las que tardan `cooling.fan_below_time` (nuevo, 60 s); las más largas van sin ventilador, o con `fan_min` si
  `cooling.fan_always` (nuevo). Los puentes llevan `cooling.fan_bridges` (nuevo, 100 %) y después vuelve el de la capa.
  El comando va justo antes del `;LAYER_CHANGE` de su capa. Antes iba fijo en `fan_min`. `cooling.fan_off_layers`
  pasa de 1 a 3 capas sin ventilador; las que tienen que frenar lo siguen llevando al máximo.
- **Falda y borde** (ficha `supports_skirt.md`, reglas 8 y 9): la falda rodea la pieza con las esquinas redondeadas y
  empieza a 6 mm de ella (`adhesion.skirt_distance`, antes 3). Las vueltas del borde van a la separación de los
  cordones (el ancho menos lo que se encima un cordón de bordes redondos: 0,357 mm con 0,4 × 0,2, antes 0,4) y llegan
  al ancho pedido: un borde de 5 mm tiene 14 vueltas (antes 12 y 4,8 mm).
- **Las velocidades son de la impresora** (decisión del dueño): el motor ya no tiene velocidades por defecto. Todas
  (`speed`, `retraction.speed`, `cooling.min_speed` y las de `machine`) salen del perfil de la impresora elegida, porque
  dependen de ella; en la interfaz están en **Impresora › Velocidades**. Solo si la impresora no trae una (una creada
  de cero, una del catálogo, un archivo de ajustes sin ella) se usa la de una Creality Ender-3 de fábrica, con los
  números que publica Creality (paredes 40, externa 30, relleno 60, arriba 40, soporte 40, viajes 180, primera capa
  25 y retracción 25 mm/s), y se avisa: la interfaz al elegirla, `slice` con `warning: no speed for…` y el aviso
  `speed_fallback`. `speed.max` y `machine.max_feed_*` sin valor no topan. En los archivos, una velocidad sin valor se
  escribe `null`.
  - **Incompatible:** los perfiles pasan a la versión 2. Un perfil de impresión de la versión 1 se sigue abriendo, pero
    sin sus velocidades: hay que pasarlas al perfil de la impresora. Lo mismo `cooling.min_speed` de un perfil de
    filamento.
  - Un archivo de ajustes para `slice` sin velocidades ya no toma las de banco de antes (60, 80, 130 mm/s…), sino las de
    respaldo.
- **Impresoras de pellets** (ficha `pellets.md`): `layers.height` y `layers.first_height` aceptan hasta 10 mm (antes
  2) y `walls.width` hasta 40 mm (antes 10). Como siempre, nunca más gruesas que la boquilla ni más anchas que cuatro
  boquillas. Una impresora con boquilla de 8 mm y capas de 4 mm ya lamina.
- **Modelos grandes más fluidos** (pedido del dueño; `docs/UI.md`, «Modelos grandes»): con un modelo de un millón de
  triángulos, arrastrarlo ya no rehace la malla en cada cuadro (antes, 5 cuadros por segundo), y el clic y el pincel
  buscan en un árbol de cajas: un clic pasa de 57 ms a 3 µs.
- **Brillo parejo** (pedido del dueño; `docs/GLOSS.md`): para llegar al tiempo mínimo de capa, ahora se frena primero
  todo lo que no es pared externa, y la pared externa sigue a su velocidad (`cooling.keep_outer_speed`, encendido por
  defecto). Una pared que se frena sale de una boquilla más caliente y brilla distinto: las capas cortas dejaban bandas.
  Si frenar lo de adentro no alcanza, se frena todo como antes. En el cubo de 10 mm, la pared externa va a 30 mm/s en
  todas las capas (antes, entre 27,5 y 28,1 según la capa), con el mismo tiempo total.
- **Vista previa más parecida a la pieza impresa** (pedido del dueño, con una foto de una esquina): los cordones que
  siguen de un movimiento al siguiente se unen en inglete. En las curvas ya no sobresale el extremo de cada tramo en
  dientes, y las paredes no titilan donde dos tramos se encimaban. La luz sigue el perfil de un cordón real: plano
  arriba y redondo a los costados. `--look` acerca la cámara en las capturas.
- **Ray tracing con el filamento continuo** (pedido del dueño): los rayos chocan con los mismos cordones que dibuja la
  vista, unidos en inglete, en vez de una caja por tramo. Las sombras y la oclusión de una pared ya no tienen muescas
  oscuras entre tramos.
- **Laminar con capas sólidas de abajo es mucho más rápido:** el Benchy pasa de 6,3 s a 1,4 s. La detección de puentes
  calculaba el área de las paredes de la capa de abajo con un disco por punto; ahora es un trazo por recorrido y solo
  cerca de la superficie de abajo. El G-code cambia micras en el borde de algunos puentes.

### Corregido

- **Soportes en árbol con ajustes extremos:** puntas muy finas, muy densas y muy juntas bajo un voladizo grande
  agotaban la memoria y el proceso moría. Ahora las puntas nunca quedan más juntas que dos cordones, y el laminado
  termina: el caso extremo de una losa de 100 × 100 mm, en 5 s.
- **G-code con muchos lazos** (los de los soportes en árbol): escribirlo es de 10 a 20 veces más rápido. En una losa de
  100 × 100 mm con árboles, 0,8 s en vez de 17 s, con el mismo G-code.
- **Un 3MF dañado que dice tener una parte enorme** se rechaza con un mensaje claro, en vez de un error interno al
  quedarse sin memoria.
- Con `machine.min_layer_height` mayor que `machine.max_layer_height`, el error decía que la capa no entraba; ahora
  dice que los dos límites se contradicen (equipo de fichas).
- Con la primera capa más gruesa que las demás, faltaba la última capa: un cubo de 10 mm con capas de 0,3 y 0,2 tenía
  49 capas y terminaba en 9,9 mm; ahora tiene 50 y termina en 10,1 (ficha `slicing.md`, C4).
- Una pendiente justo en el ángulo límite automático (un escalón de medio cordón por capa, la cuña de 45° con capas de
  0,2) llevaba soporte; la regla es «más de» (ficha `supports_skirt.md`, S1).

## [0.8.0-alpha.1] - 2026-10-25

Tercera versión publicada. Trae el catálogo de 144 impresoras y 108 filamentos con sus capacidades, la altura de
capa variable por Z y por zona, el Z anti-aliasing, catorce patrones de relleno nuevos (entre ellos el cúbico
adaptativo y el rayo), el relleno guiado por tensiones y el ondulado, la pestaña Impresora para imprimir por USB (el
comienzo de P2), la tercera tanda de calibraciones y la resolución del G-code. Corrige lo que encontró la revisión profunda de
errores a pedido del dueño: la conexión con la impresora, G-code peligroso, ajustes imposibles, archivos dañinos,
paredes partidas o colgadas, y el rendimiento con muchas islas. Sigue siendo una alfa: los formatos de ajustes,
perfiles, proyectos e informes todavía pueden cambiar entre versiones 0.x, y cada cambio se anota acá.

### Agregado

- **Catálogo de impresoras y filamentos** (`docs/CATALOG.md`): 144 impresoras de 21 fabricantes y 108 filamentos de
  14, con los datos de las páginas oficiales de cada fabricante y sus fuentes. **Agregar desde el catálogo…** en las filas de perfil de Impresora y Filamento busca por marca y
  modelo, muestra la ficha y crea el perfil (cama, boquilla, firmware, retracción de partida, G-code de inicio propio;
  temperaturas y ventilador del filamento).
- **Capacidades de impresoras y filamentos** (`docs/CATALOG.md`, «Capacidades»): el catálogo trae, cuando el
  fabricante lo dice, la cinemática, el flujo máximo, la cámara calefaccionada, la boquilla endurecida, el hot end, el
  sensor, las camas, la conectividad y las funciones de cada impresora, y el secado, la dureza, si es abrasivo o
  soluble y las temperaturas del material de cada filamento. El motor las usa:
  - ninguna extrusión pasa del flujo máximo del hot end ni del filamento (`machine.max_flow`, `extrusion.max_flow`);
  - calienta la cámara que pide el filamento si la impresora puede (`M141` o `SET_HEATER_TEMPERATURE`) y la apaga al
    final;
  - avisa cuando el filamento pide más de lo que la impresora da: temperatura de boquilla o cama, cámara, impresora
    cerrada, boquilla endurecida.
- **Altura de capa por zona** (`docs/LOCAL_LAYERS.md`, pedido del dueño): con `layers.detail_height`, las capas se
  parten en subcapas finas solo donde la pieza cambia (pendientes, detalles, topes y fondos) y el resto se imprime en
  una capa gruesa, así dos partes a la misma altura salen con capas de alto distinto. Una caja recta y una pirámide
  lado a lado, con capas de 0,3 y detalle de 0,1 mm: 32 % menos de tiempo que todo a 0,1 mm, con el mismo material.
  En Impresión, **Alto del detalle**, **Escalón aceptado** y **Margen del detalle**; en la vista previa, **Color por →
  Alto de capa**.
- **Altura de capa variable por Z** (`docs/VARIABLE_LAYERS.md`, pedido del dueño):
  - `layers.heights`: una lista de tramos (Z, altura);
  - `layers.cusp_height`: altura automática por pendiente, según el escalón máximo que puede dejar una superficie
    inclinada;
  - las caras planas cierran una capa a su altura exacta;
  - `machine.min_layer_height` y `max_layer_height` limitan las alturas, y nunca pasan la boquilla.

  Con la altura por zona, la pendiente decide cuántas subcapas finas lleva cada capa gruesa, dentro de una misma pieza.
  En una cúpula sobre una columna, con escalón de 0,05 mm, tarda 56 min. Todo a 0,05 mm tarda 225 min, con el mismo
  escalón máximo.

  En Impresión se agregan **Escalón en pendientes** y **Alturas por Z**; en Impresora, **Capa mínima** y **Capa
  máxima**.
- **Cúbico adaptativo** (`docs/INFILL.md`, ficha `infill_patterns.md`, regla 7): `infill.pattern` acepta
  `adaptive_cubic`, el cúbico a 0°, 60° y 120° con cubos chicos junto a las paredes y superficies y más grandes
  adentro. En un cubo de 100 mm al 20 % usa un 26 % menos de material y tarda un 13 % menos que el cúbico; en piezas
  chicas da lo mismo que el cúbico. En Impresión, **Cúbico adaptativo** en el patrón de relleno.
- **Cúbico de soporte** (regla 8): `support_cubic`, el adaptativo sin contar las capas sólidas de abajo, que el relleno
  no sostiene: cubos chicos bajo los techos y grandes sobre los pisos. En un cubo de 100 mm al 20 %, un 30 % menos de
  material y un 14 % menos de tiempo que el cúbico.
- **Relleno ondulado** (`docs/NONPLANAR_INFILL.md`, N1): con `infill.nonplanar`, el relleno sube y baja en Z
  igual en todas las capas, así las capas quedan anidadas con la unión inclinada, según ZigZagZ (Allum et al. 2021:
  +62 % de resistencia en Z en sus ensayos). Solo con patrones que se repiten capa a capa; la onda se apaga cerca de
  paredes y pieles. En Impresión, **Relleno ondulado**, **Altura de la onda** y **Largo de la onda**. Con el eje Z a
  12 mm/s tarda bastante más; la resistencia todavía no está medida en el motor.
- **Hilbert, cuerdas de Arquímedes y espiral octagrama** (ficha `infill_patterns.md`, regla 12): `hilbert`, `archimedean` y
  `octagram` en `infill.pattern` y en la interfaz; iguales en todas las capas, las espirales centradas en la pieza.
- **Capa sólida cada N y espesor mínimo de la cáscara** (ficha `infill_patterns.md`, reglas 21 y 22):
  `infill.solid_every` imprime las capas N, 2N… sólidas, como puente sobre el relleno; `surfaces.top_min_thickness` y
  `surfaces.bottom_min_thickness` agregan capas sólidas hasta ese espesor. En Impresión, **Capa sólida cada**,
  **Espesor mínimo arriba** y **Espesor mínimo abajo**.
- **Línea, panal y concéntrico** (ficha `infill_patterns.md`, reglas 3, 10 y 13): `line`, `honeycomb` y `concentric`
  en `infill.pattern` y en la interfaz. En el cubo de 20 mm de la ficha, 182 a 185 mm por capa en un tramo (P3: 185,9),
  230 a 238 mm (P8: 229 a 244) y 200 mm en 5 lazos (P9: 199,4 en 5).
- **Estrellas, rectilíneo alineado y zigzag** (ficha `infill_patterns.md`, reglas 5, 1 y 2): `stars` (el patrón por
  defecto de Tisma Slicer), `aligned_rectilinear` y `zigzag` en `infill.pattern` y en la interfaz.
- **Rayo** (regla 9): `lightning`, solo ramas que nacen en las paredes y llegan bajo cada techo, retirándose hacia las
  paredes más abajo. Para piezas que solo se miran: en un cubo de 100 mm al 20 %, un 64 % menos de material y la
  mitad del tiempo que el cúbico. En Impresión, **Rayo** en el patrón de relleno.
- **Relleno guiado por tensiones** (`docs/INFILL.md`, etapa 4): con `infill.stress_density` y un caso de carga
  (`tisma-engine slice … --case caso.json`), el motor analiza la pieza bajo esa carga y sube el relleno por bandas
  donde trabaja más, hasta esa densidad. El resto queda liviano. `analyze` usa el mismo relleno. En Impresión,
  **Guiado por tensiones** y **Caso de carga** (un `.json` como el de `analyze`).
- **Relleno denso bajo el techo y patrones de triángulos y cúbico** (`docs/INFILL.md`, fichas `dense_infill.md` e
  `infill_patterns.md`):
  - con `infill.dense_under_top`, la capa de relleno justo debajo de una superficie de arriba va a
    `infill.dense_density` (40 %), solo bajo esa superficie: un relleno general más liviano sin que el techo se hunda;
  - `infill.pattern` acepta `triangles` (tres familias de líneas a 60°) y `cubic` (las mismas, corridas con la altura:
    cubos apoyados en un vértice, rígidos en las tres direcciones y en líneas rectas).

  En Impresión, **Denso bajo el techo**, **Densidad bajo el techo** y los patrones **Triángulos** y **Cúbico**.
- **Combinar capas de relleno** (`docs/INFILL.md`, ficha `infill_patterns.md`, primera etapa del relleno por
  resistencia y rapidez que pidió el dueño): con `infill.combine_layers` o `combine_auto`, el relleno disperso se
  imprime cada N capas con el alto de las N, nunca más que la boquilla. El mismo material en menos pasadas: en la
  esfera del banco, de 447 a 297 min con capas de 0,2 mm y de 736 a 402 min con capas de 0,12 mm. En Impresión,
  **Combinar relleno** y **Combinar lo más posible**.
- **Z anti-aliasing** (`docs/ZAA.md`, ficha `z_antialiasing.md`, pedido del dueño): con `zaa.enabled`, las paredes,
  el relleno de arriba y el de huecos bajan hasta la superficie donde esta corta su capa, con el material del espesor
  que queda (nunca menos que `zaa.min_thickness`). Con `zaa.raise` también suben hasta media capa, como en Song et al.
  2017.
  - El G-code lleva X, Y y Z en el mismo movimiento.
  - El chequeo y la vista previa lo entienden.
  - En la esfera del banco, el error de la superficie de arriba baja de 0,048 a 0,003 mm en el mismo tiempo.

  Nuevo perfil de impresión **Máxima calidad**: capas de 0,3 mm, subcapas de 0,1 mm en las pendientes y ZAA con
  subida. En la esfera deja 0,001 mm de error en 526 min, contra 0,014 mm en 1788 min de todo a 0,05 mm.
- **La capa de dispositivos** (`docs/DEVICE.md`, el comienzo de P2): la interfaz `Printer`, el puerto serie
  (Linux y Windows) y el host de G-code por USB —líneas numeradas con suma de verificación, reenvíos, temperaturas,
  trabajos con pausa y cancelación, controles— con una impresora simulada para las pruebas.
- **La pestaña Impresora** de la interfaz (`docs/DEVICE.md`): conectar por USB (o a la impresora simulada), ver el
  estado y las temperaturas, imprimir lo laminado o un G-code abierto con su avance y tiempo restante, pausar,
  seguir, cancelar, mover, extruir y el ventilador. `--device` y `--device-print` para las capturas.
- **Calibraciones estándar, tercera tanda** (`docs/CALIBRATION.md`): el tope de flujo volumétrico (cada segmento
  con todas sus líneas a ese flujo; el elegido pasa a `machine.max_flow`), las resonancias (cada segmento con su
  aceleración, `M204` o `SET_VELOCITY_LIMIT`; la elegida pasa a `machine.acceleration`) y la pata de elefante (un
  bloque que se mide; el exceso de la primera capa pasa a `slicing.first_layer_shrink`). Cambios por altura nuevos
  `volumetric_flow` y `acceleration`.
- **Filtro de moiré** en el menú de imagen de la vista 3D (pedido del dueño): de lejos, las líneas de capa de la
  vista previa más finas que un píxel se dibujan con la luz promedio de su perfil en vez de hacer bandas, y las líneas de
  la grilla se desvanecen cuando quedan a menos de unos píxeles de la siguiente. Activado por defecto; `--moire
  on|off` en las capturas.
- El plan de Device y conexiones (P2), el diseño de multimaterial con un cabezal: mezcla de colores por capas y purga
  en el relleno (`docs/MULTIMATERIAL.md`), y el plan de la app de Android que sigue la impresión, con USB
  (`docs/MOBILE.md`).
- **Resolución del G-code** (`gcode.resolution`, 0,0125 mm, de la ficha `gcode_extras.md`; en la interfaz, sección
  Impresora): los puntos que mueven el camino menos que eso no se escriben, sin cambiar el ancho, la Z ni el material.
  Las paredes repetían puntos casi iguales, el giroide escribía el triple de tramos y quedaban tramos de un micrón que
  extruían. Un cilindro de 128 lados pasa de 41 859 a 6 140 extrusiones y de 1,25 MB a 0,19 MB; la esfera del banco
  con giroide, de 269 mil a 73 mil (2,1 MB en vez de 7,7): menos líneas que el firmware tiene que leer y planear.

### Cambiado

- Escribir el G-code es un 28 % más rápido (la esfera del banco: de 0,81–0,88 s a 0,60–0,62 s), con el mismo texto
  byte a byte: los números se escriben por un camino propio que da lo mismo que `std::to_chars`.
- El G-code escribe la aceleración de los ajustes después del G-code de inicio (`M204 P… T…`, o
  `SET_VELOCITY_LIMIT ACCEL=…` en Klipper), así la impresora corre a la aceleración con la que se estima el tiempo.
- **Escribir el G-code de rellenos con muchos recorridos cortos** era lento: el orden miraba todos los recorridos para
  elegir el siguiente. Ahora busca en una grilla, con el mismo G-code: el rayo al 100 % en un cubo de 100 mm pasa de
  19,6 s a 0,7 s de escritura (y de 63 s a 2,1 s en total, con el índice de ramas del rayo).

### Corregido

Revisión profunda de errores (2026-10-25), a pedido del dueño: seis revisores por área del motor y la revisión de caja
negra del equipo de fichas. Cada arreglo tiene su prueba (muchas en `tests/test_hostile.cpp`).

- **Conexión con la impresora:**
  - un cable desenchufado se nota enseguida (antes, el programa seguía «conectado» con un núcleo al 100 %);
  - un «ok» perdido ya no cancela la impresión: el motor vuelve a mandar la línea y, si la impresora ya la había
    hecho, sigue con la siguiente sin repetirla;
  - si la impresión se abandona, apaga la boquilla, la cama y el ventilador;
  - una pausa o cancelación pedida justo al terminar un trabajo ya no afecta al siguiente;
  - las esperas de calentamiento (`M109`, `M190`) cuentan los informes de temperatura como señal de vida.
- **G-code peligroso:**
  - una variable del G-code de inicio con un salto de línea podía meter comandos de movimiento después de la
    impresión; ahora queda en su comentario, y los nombres con saltos de línea o corchetes se rechazan;
  - un índice de variable enorme (`{temperature[99999999999999999999]}`) daba un error interno y dejaba un `.part`.
- **Ajustes que la impresora no puede imprimir:**
  - capas más gruesas que la boquilla o que `machine.max_layer_height`, más finas que `machine.min_layer_height`, y
    líneas de menos de un cuarto o más de cuatro boquillas: la línea de comandos sale con código 2 y la interfaz no
    lamina;
  - límites de lectura: altura de capa de 0,01 a 2 mm, ancho de 0,05 a 10 mm, `slicing.xy_offset` de −10 a 10 mm,
    `first_layer_shrink` hasta 2 mm, `retraction.length` hasta 50 mm, `retraction.lift` hasta 10 mm,
    `machine.min_layer_height` y `max_layer_height` 0 o desde 0,01 mm, porcentajes finitos;
  - avisos nuevos: temperaturas de los cambios por altura por encima de las de la impresora, y anchos de línea poco
    comunes.
- **Archivos dañinos:** un perfil, proyecto o caso de carga con JSON anidado a gran profundidad tiraba el programa
  (desborde de pila); un número fuera de rango o un tipo equivocado salía como error interno. Ahora son errores del
  archivo.
- **Calibraciones:** las torres y el «usar valor» respetan los rangos de los ajustes (antes se podían guardar valores
  que después no se abrían), la torre de temperatura no pasa la temperatura máxima de la impresora, y las etiquetas de
  avance de presión muestran tres decimales cuando hace falta.
- **Paredes:** el diagrama de Voronoi propio podía dejar un nodo fuera de su bisectriz o fallar donde dos contornos se
  tocan en una esquina; las paredes se colgaban, se caían o salían en tramos de un micrón. Ahora el motor lo detecta y
  usa el diagrama de referencia para esa región.
- **Paredes partidas en las esquinas cóncavas y rectas:** cada pared se cortaba en pedazos abiertos en cada esquina
  cóncava (un agujero, una muesca, el ruido de una malla) y en cada punto en medio de un lado recto (los que deja el
  corte de la malla con `slicing.simplify_tolerance` 0). Un lazo abierto no tiene costura: la costura alineada o
  atrás no se aplicaba y cada pedazo empezaba donde caía. Un cilindro de 128 lados salía con 38 pedazos por capa;
  ahora cada pared es un lazo cerrado.
- **Rellenos de Arquímedes y octagrama:** ya no trazan una línea recta desde el centro del espiral hasta la pared.
- **Análisis estructural:** una pieza sujeta solo en algunos ejes (apoyada en la cama y empujada de costado) se avisa
  al instante, en vez de horas de cálculo o un resultado sin sentido; una carga que cae sobre la sujeción también se
  avisa.
- **Mallas:** las piezas a más de 2000 mm del origen o más altas que 2000 mm se rechazan con un mensaje claro (antes,
  memoria sin límite o desbordes); el conteo de cortes usa 64 bits; la altura variable ya no queda en un bucle con una
  altura mínima diminuta; `info` no se cae con piezas planas.
- **Exportación:** si el G-code no pasa la verificación o falla al escribirse, no queda ningún archivo viejo con el
  nombre pedido.
- **Verificador:** acepta las esquinas en punta de la falda y el borde (antes rechazaba el G-code propio de algunas
  piezas con esquinas agudas).
- **Interfaz:**
  - mover la temperatura de la boquilla cambiaba también la de la cama: compartían el identificador;
  - los cambios por altura de una torre de calibración quedaban en el modelo siguiente y se guardaban;
  - la respuesta «¿Reemplazar?» pasaba a otro archivo elegido después;
  - el explorador de archivos rompía una regla de ImGui (cuadro de error, y cierre en las compilaciones de depuración);
  - agregar del catálogo un perfil con el nombre de uno propio lo reemplazaba sin avisar: ahora toma «(2)»;
  - unas preferencias ilegibles se perdían enteras y se pisaban al salir: ahora se guardan aparte, y los ajustes y
    preferencias se escriben aparte y se mueven encima;
  - un archivo soltado con el explorador abierto cambiaba el modelo bajo la exportación pendiente.
- **Segunda tanda de la revisión:**
  - **Z anti-aliasing con «raise»:** con una capa siguiente más fina (la primera capa más gruesa, la altura por
    zona, los tramos de altura), la capa subía hasta donde la siguiente volvía a imprimir la misma superficie: la
    boquilla pasaba dentro del cordón fresco y el material salía dos veces. Ahora sube como mucho hasta la mitad de la
    capa siguiente.
  - **Una cara con la orientación invertida** hacía desaparecer la pieza entera; ahora la malla se orienta pareja
    antes de laminar.
  - **Después del G-code de un cambio por altura** (una pausa que estaciona el cabezal), el motor vuelve a fijar los
    modos y E, retrae y escribe X, Y, Z y F completos. Antes seguía desde donde creía que estaba, o el verificador
    rechazaba la exportación.
  - **Ventilador mínimo:** `M106 S1` es el ventilador al máximo en RepRapFirmware; ahora sale `S2`.
  - **Líneas sólidas por isla:** se ajustan a cada isla, no a la capa entera (quedaba sin cubrir una franja del borde
    de las otras islas).
  - **Proyectos:** una malla nombrada por muchos modelos multiplicaba la memoria; ahora cada malla es de un modelo.
  - **Mensajes:** una pieza que queda vacía dice si fue `slicing.xy_offset` o una malla con agujeros, y el relleno
    ondulado ignorado por el patrón avisa.
  - **Rendimiento con muchas islas** (pedido del equipo de fichas): las costuras alineadas buscan la costura de
    abajo en una grilla, las uniones del zigzag buscan el borde en una grilla, y las islas se eligen con cajas, con el
    mismo G-code byte a byte. 196 cilindros: escritura de 19,6 a 8,4 s, laminado de 14,1 a 10,0 s.
- **El relleno Hilbert desaparecía lejos del origen:** la grilla de la curva cubría 2 m alrededor del origen. Girada
  por el ángulo del relleno, una pieza en una esquina de una cama de 1 m quedaba afuera y salía sin relleno. Ahora
  cubre 16 m, y crece si hace falta.
- **Relleno ondulado sin longitud de onda:** con ajustes armados en código (sin pasar por el JSON), una longitud de
  onda de 0 marcaba el relleno como ondulado sin onda, con una conversión sin definir en el medio. Ahora queda plano.
- **Z anti-aliasing sobre la capa gruesa de la altura por zona:** el verificador podía rechazar un cordón válido. El
  piso sale del tope real de la capa, y tanto ese tope como la Z del movimiento se redondean a 3 decimales; el margen
  era el de un solo redondeo. Ahora cuenta los dos.
- **El análisis «como impresa» ignoraba las capas gruesas:** la zona gruesa de la altura por zona y el relleno
  combinado no se veían. Con altura por zona, el análisis veía el 3 % de los vóxeles impresos en vez del 53 %. Ahora
  cada capa gruesa llena todas las capas que abarca. La estimación de vóxeles y el modelo macizo usan las capas del
  laminado, también con altura variable.
- **Contornos con zigzags de 1 nm:** quedaban en las esquinas tras un desplazamiento XY de una fracción de micrómetro y
  hacían fallar el diagrama de Voronoi de los perímetros. Ahora se limpian antes. Antes el laminado terminaba sin
  mensaje; ahora sale bien, y el G-code de las piezas comunes no cambia.
- **Errores internos en `tisma-engine`:** terminan con el código 7 y un mensaje. Antes cortaban el programa.
- **Un cambio por altura (`changes`) a menos de 0,0005 mm del tope de una capa:** se aplicaba en esa capa en el
  verificador y en la siguiente en el escritor, y el G-code se rechazaba. Ahora los dos usan el tope tal como se
  escribe.

## [0.7.0-alpha.1] - 2026-10-24

Segunda versión publicada. Trae la interfaz propia del motor, `tisma` (el comienzo de P1: el dueño decidió que el
motor reemplace a Tisma Slicer), con calibraciones guiadas, perfiles con nombre y proyectos, y corrige la altura del
G-code para piezas que no empiezan en Z = 0. Sigue siendo una alfa: los formatos de ajustes, perfiles, proyectos e
informes todavía pueden cambiar entre versiones 0.x, y cada cambio se anota acá.

### Agregado

- **La interfaz `tisma`** (`docs/UI.md`), con los íconos y la paleta de Tisma, en español e inglés y con tema oscuro
  o claro:
  - abre modelos STL, G-code y ajustes (botón, Ctrl+O o soltando el archivo en la ventana);
  - centra el modelo en la cama;
  - edita los ajustes de impresión, filamento e impresora;
  - lamina en segundo plano;
  - muestra el G-code capa por capa, con color por tipo de línea, velocidad o ancho;
  - exporta el G-code verificado;
  - en la pestaña Análisis, muestra cuántos vóxeles tendría el análisis estructural, con su memoria y su tiempo;
  - recuerda el idioma, el tema, la carpeta y los ajustes.
- La vista 3D tiene luz por píxel en color lineal, oclusión ambiental (por defecto) y ray tracing activable (sombras
  suaves y oclusión con rayos, que se afinan cuadro a cuadro), con suavizado de bordes MSAA y FXAA (`docs/UI.md`).
- **Calibraciones** (`docs/CALIBRATION.md`), con su pestaña guiada en la interfaz:
  - cada prueba se crea con un clic, con su rango elegido desde los ajustes, y se lamina sola; después la pestaña
    guía en tres pasos (exportar, qué mirar, elegir el segmento) y **Usar** pasa el valor al ajuste;
  - dimensiones: un bloque con agujero; con las medidas se calcula y aplica la compensación;
  - torres de temperatura, retracción, flujo y velocidad máxima, con un valor por segmento escrito en relieve;
  - la torre de temperatura trae una cresta de ángulo variable (30° a 72°), un estante plano sin soporte (para el
    futuro Arc Overhang), un perno para probar la unión entre capas y detalle en números y figuras;
  - primera capa (cinco cuadrados de una capa repartidos en la cama), velocidad de retracción, avance de presión
    (tubo de esquinas vivas, con el comando de Marlin, Klipper o RepRapFirmware), tolerancias de encastre (pernos
    impresos dentro de agujeros con 0,1 a 0,5 mm de juego) y pasos del extrusor (G-code de prueba y la cuenta del
    valor nuevo, con el comando para guardarlo);
  - la torre de velocidad muestra el flujo volumétrico (mm³/s) del segmento elegido;
  - el catálogo con las siguientes calibraciones estándar y las nuevas propuestas.
- Ajustes nuevos:
  - `compensation`: escala por eje y corrección de agujeros;
  - `extrusion.flow` y `extrusion.pressure_advance`;
  - la lista `changes`: temperatura, cama, ventilador, retracción, flujo, velocidad o G-code desde una altura.

  El verificador del G-code juzga cada capa con los valores que le tocan.
- **Perfiles con nombre** (`docs/PROFILES.md`): los ajustes se reparten en impresora, filamento e impresión, que se
  eligen, guardan y eliminan por separado desde el panel. Trae perfiles de fábrica genéricos (Marlin 256 mm; PLA, PETG,
  ABS, TPU; capas de 0,12, 0,20 y 0,28 mm). Un asterisco avisa que los ajustes cambiaron desde el perfil.
- **Proyectos `.tisma`** (`docs/PROJECT.md`): los ajustes, los perfiles elegidos, la pieza y la calibración en curso en
  un archivo, con Ctrl+S. Una torre de calibración vuelve a su pestaña, laminada.
- El flujo (`extrusion.flow`) se edita en la sección Filamento.
- `--screenshot` (con `--export`) dibuja la interfaz sin pantalla, para pruebas y capturas; `--profile` elige perfiles
  y `--save-project` guarda el proyecto.
- Los ajustes se pueden escribir como JSON en su formato (`settings_json`), y leer de vuelta iguales.
- Un lector de G-code para la vista previa, que también abre G-code de otros programas.
- Las releases traen también la interfaz.

### Cambiado

- `machine.bed` acepta también una lista vacía, su valor por defecto.
- En la interfaz, la retracción pasó de la sección Filamento a Impresora, como en los perfiles.
- La release se publica sola al enviar a `dev` o `main` un `VERSION` nuevo: el flujo crea la etiqueta.

### Corregido

- Una pieza cuya malla no empieza en Z = 0 se imprime apoyada en la cama: las alturas del G-code cuentan desde su
  base. Antes, una pieza debajo de Z = 0 daba G-code que el verificador rechazaba (la esfera del banco, centrada en
  el origen, no se podía laminar) y una más arriba se habría impreso en el aire.
- En la interfaz, abrir otra pieza o cambiar ajustes mientras se lamina ya no muestra el resultado anterior como si
  fuera el nuevo: el de otra pieza se descarta y el de otros ajustes queda marcado como desactualizado.
- Los nombres de perfil que Windows reserva para dispositivos, también con extensión («NUL.txt»), se guardan con otro
  nombre de archivo.

## [0.6.0-alpha.1] - 2026-10-24

Primera versión publicada. Reúne todo lo anterior (las fases F0 a F4, que no se publicaron por separado), el comienzo
de F5, la fase de análisis A1 casi completa y la exportación de G-code verificada. Es una alfa: los formatos de
ajustes, de caso y de informe todavía pueden cambiar entre versiones 0.x, y cada cambio se anota acá.

### Agregado

- **Exportación de G-code verificada** (`docs/GCODE_CHECK.md`):
  - cada G-code se escribe en `<archivo>.part`, se vuelve a leer desde el disco y se verifica (archivo completo,
    números, lugar, velocidad, capas, filamento por línea, retracciones, ventilador);
  - solo después se renombra; si no pasa, queda como `<archivo>.rejected` y `slice` termina con el código 6.
- **Análisis estructural de la pieza como se imprime** (fase A1, `docs/ANALYSIS.md`), con el comando
  `tisma-engine analyze` y su caso en JSON:
  - vóxeles en forma de caja que siguen las capas, con calidad normal o alta;
  - material ortótropo con la dirección real de cada cordón;
  - los huecos del relleno ralo;
  - factor de seguridad de Tsai–Wu promediado a escala de cordón y modo de falla (cordón, unión entre cordones,
    unión entre capas);
  - chequeo de resolución con extrapolación;
  - informe JSON, mapa VTK para ParaView y malla PLY coloreada;
  - estimación de vóxeles, memoria y tiempo (`--estimate`) y avance (`--progress`) para una interfaz.
- **Base de 23 materiales impresos** (`docs/MATERIALS.md`): literatura (Song 2017, Casavola 2016, Cantrell 2017),
  hojas técnicas de Bambu Lab y eSUN, y ensayos de CNC Kitchen, cada número con su fuente.
- **Solucionador de elasticidad** en vóxeles: gradiente conjugado sin matriz, precondicionado con una multigrilla
  geométrica, en paralelo y determinista.
- **Diagrama de Voronoi propio** de puntos y segmentos (construcción incremental orientada a la topología); lo usa el
  eje medio de las paredes.
- **Falda y borde** (grupo `adhesion`).
- **Soportes:** áreas por ángulo de voladizo, líneas e interfaz densa (grupo `support`).
- **Marca de Tisma** en `assets/brand/` (logos, íconos de la aplicación y de la interfaz), para la interfaz que viene.
- **Versiones:** archivo `VERSION`, este registro de cambios y releases automáticas desde las etiquetas
  (`docs/VERSIONING.md`).

### Cambiado

- El eje medio de las paredes sale del Voronoi propio en lugar del de Boost.Polygon, que queda solo como referencia en
  las pruebas.
- Los lazos de pared van siempre en sentido antihorario, y las esquinas de costura se miden a 0,1 mm, así los zigzags
  diminutos no cuentan como esquinas.
- El ventilador va al máximo en las capas frenadas, también en las primeras.
- Cada archivo de salida (G-code, avisos, informe, mapa, malla) se escribe como `<archivo>.part` y se renombra al
  terminar: si el proceso se corta, no queda un archivo a medias.
- `--version` y la cabecera del G-code muestran la versión completa del archivo `VERSION`, con su etiqueta.

### Corregido

- Una pieza toda más fina que las líneas daba un G-code sin ninguna extrusión. Ahora el laminado la rechaza con un
  mensaje claro (código 4).
- Con una cama justa para la pieza, la falda o el borde podían quedar fuera de la cama y el G-code se escribía igual.
  Ahora se rechaza antes de escribir (código 4).

## [0.5.0] - 2026-10-24

Fase F4, el G-code. Hito interno, sin release publicada.

### Agregado

- Escritor de G-code en el formato de `docs/spec/GCODE_OUTPUT.md`:
  - G-code de inicio y fin con variables;
  - E absoluta o relativa;
  - E por volumen con el ancho variable de cada tramo.
- Orden dentro de la capa, recorridos por cercanía y costuras (`walls.seam`: `aligned`, `nearest`, `random`, `rear`).
- Retracción en viajes largos y salto en Z (grupo `retraction`).
- Viajes por dentro de la pieza sin cruzar perímetros.
- Orden por islas.
- Enfriamiento: tiempo mínimo de capa en dos etapas y ventilador (grupo `cooling`).
- Límites de la máquina y estimación del tiempo de impresión.
- Avisos del laminado en JSON (`--warnings`).
- Entradas imposibles con mensaje claro y código 4: malla vacía, sin capas cerradas, fuera de la cama, más alta que
  la máquina.

## [0.4.0] - 2026-10-24

Fase F3, superficies y relleno. Hito interno, sin release publicada.

### Agregado

- Superficies superiores e inferiores por capa (grupo `surfaces`), con ancho mínimo para las franjas sólidas angostas.
- Relleno rectilíneo, en grilla y giroide (grupo `infill`), anclado a las paredes.
- Puentes medidos contra lo impreso debajo.
- Superficies superiores monotónicas.
- Relleno de huecos opcional.

### Corregido

- El eje medio resiste entradas degeneradas.

## [0.3.0] - 2026-10-24

Fase F2, paredes de ancho variable (Kuipers et al. 2020, desde el artículo). Hito interno, sin release publicada.

### Agregado

- Eje medio de cada región, con las aristas curvas discretizadas.
- El método completo del artículo:
  - esquemas de reparto de cordones;
  - marcado del centro;
  - cantidades y transiciones con rampas;
  - interpolación de repartos;
  - reparto por nodo con resolución de conflictos;
  - extracción de recorridos de ancho variable.
- Límite de paredes (`walls.count`, `walls.width`) y la región que queda para el relleno.
- Medición del área cubierta (criterio de F2).

### Cambiado

- Los contornos se simplifican antes de armar las regiones (`slicing.simplify_tolerance`), para acelerar el eje medio.

## [0.2.0] - 2026-10-24

Fase F1, corte por capas. Hito interno, sin release publicada.

### Agregado

- Soldadura exacta de vértices.
- Contornos de nivel de un campo por vértice, unidos por la identidad de las aristas.
- Cierre de huecos chicos.
- Plan de capas y corte de todas las capas en paralelo (oneTBB).
- Regiones y compensaciones con Clipper2: modos de relleno, compensación XY y de primera capa.
- Cómputo en GPU opcional con Vulkan (volk). Los núcleos de rangos de capa y de segmentos se prueban contra la CPU.
- Formato de ajustes propio (`docs/CONFIG.md`) y `slice` con el corte completo.

## [0.1.0] - 2026-10-24

Fase F0, el esqueleto. Hito interno, sin release publicada.

### Agregado

- Proyecto CMake en C++20: biblioteca `tisma_core` y programa `tisma-engine` (`slice`, `info`).
- Lector de STL binario y ASCII.
- Pruebas con Catch2 y CI en Linux y Windows.
- Licencia PolyForm Noncommercial 1.0.0.
- Reglas de sala limpia, `THIRD_PARTY.md` y `PROVENANCE.md`.

[Sin publicar]: https://github.com/adriangrochi-code/tisma_engine/compare/v0.10.0-alpha.2...HEAD
[0.10.0-alpha.2]: https://github.com/adriangrochi-code/tisma_engine/compare/v0.9.0-alpha.1...v0.10.0-alpha.2
[0.9.0-alpha.1]: https://github.com/adriangrochi-code/tisma_engine/compare/v0.8.0-alpha.1...v0.9.0-alpha.1
[0.8.0-alpha.1]: https://github.com/adriangrochi-code/tisma_engine/compare/v0.7.0-alpha.1...v0.8.0-alpha.1
[0.7.0-alpha.1]: https://github.com/adriangrochi-code/tisma_engine/compare/v0.6.0-alpha.1...v0.7.0-alpha.1
[0.6.0-alpha.1]: https://github.com/adriangrochi-code/tisma_engine/releases/tag/v0.6.0-alpha.1
