# Registro de cambios

Los cambios de Tisma engine que importan a quien lo usa, por versión. El formato sigue
[Keep a Changelog 1.1.0](https://keepachangelog.com/es-ES/1.1.0/) y los números de versión, el
[versionado semántico 2.0.0](https://semver.org/lang/es/). La política está en `docs/VERSIONING.md`.

Cada versión agrupa sus cambios en estas categorías: **Agregado** (funciones nuevas), **Cambiado** (lo que ya existía
y ahora se comporta distinto), **Obsoleto** (lo que se va a quitar), **Quitado**, **Corregido** (errores) y
**Seguridad**. Una categoría sin cambios no se escribe.

## [Sin publicar]

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

### Cambiado

- Escribir el G-code es un 28 % más rápido (la esfera del banco: de 0,81–0,88 s a 0,60–0,62 s), con el mismo texto
  byte a byte: los números se escriben por un camino propio que da lo mismo que `std::to_chars`.
- El G-code escribe la aceleración de los ajustes después del G-code de inicio (`M204 P… T…`, o
  `SET_VELOCITY_LIMIT ACCEL=…` en Klipper), así la impresora corre a la aceleración con la que se estima el tiempo.

### Corregido

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

### Arreglado

- **Escribir el G-code de rellenos con muchos recorridos cortos** era lento: el orden miraba todos los recorridos para
  elegir el siguiente. Ahora busca en una grilla, con el mismo G-code: el rayo al 100 % en un cubo de 100 mm pasa de
  19,6 s a 0,7 s de escritura (y de 63 s a 2,1 s en total, con el índice de ramas del rayo).

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

[Sin publicar]: https://github.com/adriangrochi-code/tisma_engine/compare/v0.7.0-alpha.1...HEAD
[0.7.0-alpha.1]: https://github.com/adriangrochi-code/tisma_engine/compare/v0.6.0-alpha.1...v0.7.0-alpha.1
[0.6.0-alpha.1]: https://github.com/adriangrochi-code/tisma_engine/releases/tag/v0.6.0-alpha.1
