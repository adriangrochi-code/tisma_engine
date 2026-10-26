# Cómo funciona el código

Una guía para leer y mantener Tisma engine. Cuenta qué hace cada parte, cómo pasan los datos de una a otra y qué reglas
siguen todas. Los detalles de cada función están en los comentarios de su cabecera (`.hpp`). De dónde sale cada
algoritmo (artículo, especificación o diseño propio) está en `PROVENANCE.md`. Cómo compilar, probar y aportar está en
`docs/DEVELOPMENT.md`.

## Qué hay en el repositorio

| Carpeta | Qué tiene |
|---|---|
| `src/` | El motor: la biblioteca `tisma_core` |
| `apps/` | Los programas: `apps/main.cpp` es `tisma-engine`, que solo llama a la línea de comandos (`src/cli/`); `apps/tisma/` es la interfaz (`docs/UI.md`) |
| `tests/` | Pruebas con Catch2, un archivo por módulo (`test_<módulo>.cpp`), y datos en `tests/data/` |
| `tools/` | Herramientas del proyecto: revisión de licencias y de especificaciones, shaders, comprobaciones de Linux, notas de release |
| `docs/` | Documentación de uso (ajustes, análisis, materiales, verificación del G-code), esta guía, la de desarrollo, la de versiones |
| `docs/spec/` | Especificaciones funcionales que manda el equipo que conoce Tisma Slicer (ver `CLEANROOM.md`) |
| `assets/brand/` | Logos e íconos de Tisma |
| raíz | `PLAN.md` (fases), `PROGRESS.md` (bitácora), `PROVENANCE.md`, `THIRD_PARTY.md`, `CHANGELOG.md`, `VERSION`, `CLEANROOM.md`, `CONTRIBUTING.md`, `LICENSE` |

## Del STL al G-code

El laminado entero es una llamada, `slice_model` (`src/slice/pipeline.hpp`). Toma una malla y los ajustes y devuelve
un `SlicedModel` con todo lo que se va a imprimir, capa por capa. Después, `export_gcode` lo escribe y lo verifica.

```
archivo .stl
  │ read_stl                          io/stl            binario o ASCII, detectado por el contenido
  ▼
TriangleMesh ─ weld_exact ─────────▶ IndexedMesh       mesh/            vértices compartidos por igualdad exacta
  │
  │ plan_layers                       slice/layers      alturas de capa (la primera y el resto)
  │ height_field + slice_levels       slice/level_set,  contornos de nivel de un campo por vértice (z en el corte
  │                                   slice/contours    plano), unidos por la identidad de las aristas; huecos chicos
  │                                                     cerrados; en la GPU si hay
  ▼
contornos por capa
  │ simplify, layer_region, offset    geom/polygons     regiones con agujeros (Clipper2): modo de relleno,
  │                                                     compensación XY y de primera capa
  ▼
regiones por capa ── en paralelo por capa (oneTBB) ──────────────────────────────────────────────────────┐
  │ layer_walls                       perimeters/       eje medio (geom/medial_axis sobre el Voronoi propio,   │
  │                                                     geom/voronoi) y paredes de ancho variable (Kuipers et   │
  │                                                     al. 2020): reparto de cordones, transiciones, sitios y  │
  │                                                     recorridos; deja la región para el relleno             │
  ▼                                                                                                        │
classify_surfaces, find_bridges       surfaces/         superiores, inferiores, ralas y puentes (necesita ver   │
  │                                                     las capas vecinas, por eso va fuera del paso paralelo) │
  │ support_areas                     support/          voladizos y su proyección hacia abajo                  │
  │ brim_loops, skirt_loops           adhesion/         borde y falda alrededor de la primera capa             │
  ▼                                                                                                        │
fill_layer ── en paralelo por capa ───────────────────────────────────────────────────────────────────────┘
  │                                   infill/           rectilíneo, grilla, giroide, monotónico, huecos,
  │                                                     soportes e interfaz
  ▼
SlicedModel
  │ unprintable_reason                slice/printable   ¿se puede imprimir? (si no: mensaje claro, código 4)
  │ export_gcode                      gcode/export      escribe en .part, vuelve a leer, verifica, renombra
  │   write_gcode                     gcode/writer      orden (gcode/order), viajes (gcode/travel), enfriamiento
  │                                                     (gcode/cooling), tiempo (gcode/print_time)
  │   check_gcode                     gcode/check       la verificación (docs/GCODE_CHECK.md)
  ▼
archivo .gcode
```

## Los datos

| Tipo | Dónde | Qué es |
|---|---|---|
| `Vec3f`, `TriangleMesh` | `mesh/mesh.hpp` | La malla tal como viene del STL: tres vértices por triángulo, en `float` |
| `IndexedMesh` | `mesh/indexed_mesh.hpp` | Vértices únicos y triángulos como índices. Es la entrada del laminado |
| `Layer` | `slice/layers.hpp` | Una capa: `bottom` y `top`; se corta a media altura (`slice_z`) |
| `Contour` | `slice/contours.hpp` | Un contorno de nivel, cerrado o abierto |
| `Polygon`, `Polygons` | `geom/polygons.hpp` | Polígonos de Clipper2 en enteros: **1 unidad = 1 nm** (`kUnitsPerMm`). `to_mm` pasa a milímetros |
| `Point2d` | `geom/medial_axis.hpp` | Un punto en milímetros, en `double` |
| `Toolpath`, `ToolpathPoint` | `perimeters/sites.hpp` | Un recorrido: puntos con el ancho del cordón en cada uno, abierto o cerrado. Paredes, rellenos, falda y soportes son todos `Toolpath` |
| `LayerWalls`, `LayerSurfaces`, `LayerFill` | `perimeters/`, `surfaces/`, `infill/` | Lo de una capa: paredes y región para el relleno; superficies; rellenos por tipo |
| `SlicedModel` | `slice/pipeline.hpp` | Todo el laminado: capas, regiones, paredes, superficies, rellenos, falda, borde, soportes y avisos |
| `Settings` | `config/settings.hpp` | Los ajustes, con sus valores por defecto. Se leen de JSON (`docs/CONFIG.md`) |

**Unidades:** milímetros, segundos y mm/s en todo el motor. Los ángulos de los ajustes van en grados y adentro en
radianes. Las fuerzas del análisis van en N y las tensiones en MPa.

## Reglas que siguen todos los módulos

- **Determinismo:** el mismo modelo con los mismos ajustes da el mismo G-code, byte a byte, en cada corrida y con
  cualquier cantidad de núcleos.
  - El paralelismo es por capa o por nodo, y cada tarea escribe solo lo suyo.
  - Las sumas paralelas usan reducciones deterministas (`deterministic_dot`).
  - Lo «aleatorio», como la costura `random` o el orden de inserción del Voronoi, sale de un mezclador fijo: el
    finalizador de splitmix64.
- **CPU como referencia:** todo lo que corre en la GPU tiene su versión en CPU, y las pruebas comparan las dos.
- **Datos contiguos:** vectores planos e índices en lugar de punteros y mapas en los caminos calientes.
- **Topología antes que geometría:** los contornos se unen por la identidad de las aristas, los recorridos por la de
  sus sitios y el Voronoi por su estructura de árbol. Así el redondeo no rompe la forma.
- **Mensajes claros:** una entrada imposible termina con un mensaje que nombra el problema (la clave del ajuste, el
  punto fuera de la cama) y un código de salida documentado. Nunca se ignora en silencio.

## Los módulos

### `io/`, `mesh/`: entrada

`read_stl` acepta STL binario y ASCII. Un binario cuya cabecera empieza con `solid` sigue siendo binario. Rechaza
archivos truncados y números no finitos. `stl_binary` escribe una malla como STL binario (para los proyectos).
`weld_exact` une los vértices iguales bit a bit con una tabla hash abierta.

### `slice/`: capas y contornos

- `plan_layers` arma las alturas.
- `height_field` da el campo por vértice; en el corte plano es la altura z. Es el punto de entrada para capas curvas
  en el futuro: cualquier campo escalar sirve.
- `slice_levels` saca los segmentos de cada nivel visitando cada triángulo solo para los niveles que cruza, y
  `join_segments` los une en contornos por la clave de arista.
- Los niveles sin vértices entre uno y otro cruzan las mismas aristas (`same_crossings`): solo se corta el primero de
  la tanda, y los demás calculan sus puntos con `edge_crossing` sobre esas aristas. Si la sección en x e y es la misma
  (`same_sections`, los lados de un prisma), el corte deja el nivel vacío y `pipeline.cpp` copia la región y las
  paredes de la capa de abajo. Las superficies, el relleno y los planes de viaje del G-code se reusan igual cuando
  sus entradas son iguales a las de otra capa.
- `pipeline.cpp` encadena todas las etapas y `printable.cpp` decide si el resultado se puede imprimir.

### `geom/`: geometría

- **`polygons.*`:** las operaciones con Clipper2 (regiones, offsets, simplificación, áreas).
- **`voronoi.*`:** un diagrama de Voronoi propio de puntos y segmentos, con construcción incremental orientada a la
  topología (Sugihara e Iri; Held y Huber 2009). Cada inserción borra un árbol de nodos y lo reemplaza por la celda
  nueva; los números solo eligen entre actualizaciones topológicamente válidas.
- **`medial_axis.*`:** el eje medio de una región a partir de ese diagrama. Boost.Polygon queda solo como referencia
  en las pruebas.

### `perimeters/`: paredes de ancho variable

El método de Kuipers et al. 2020, desde el artículo, en este orden:
1. `medial_graph` y `mark_central`: el grafo del eje medio y su centro.
2. `beading.*`: los esquemas de reparto, es decir cuántos cordones y de qué ancho para cada espesor.
3. `transitions.*`: cantidades en el centro, anclas y rampas de transición.
4. `node_beadings`: el reparto en cada nodo.
5. `sites.*`: sitios sobre los trapecios y su encadenamiento en recorridos.

`walls.*` lo junta todo en `layer_walls`, que también aplica el límite de paredes y deja la región para el relleno.

### `surfaces/`, `infill/`, `support/`, `adhesion/`

- **`classify_surfaces`:** separa en superficies superiores, inferiores y ralas, contando capas desde cada superficie
  expuesta.
- **`find_bridges`:** marca los puentes, medidos contra lo impreso debajo.
- **`fill_layer`:** arma los rellenos de una capa:
  - rectilíneo y grilla con anclaje en zigzag a las paredes;
  - giroide por cuadrados marchantes;
  - superiores monotónicas;
  - relleno de huecos;
  - soportes con su interfaz.
- **`support_areas`:** proyecta los voladizos hacia abajo.
- **`skirt_loops` y `brim_loops`:** la falda y el borde.

### `gcode/`: escritura y verificación

- **`writer.*`:** recorre las capas, ordena islas y recorridos (`order.*`), planifica los viajes por dentro
  (`travel.*`), aplica el tiempo mínimo de capa (`cooling.*`) y estima el tiempo con aceleraciones (`print_time.*`).
  Cada capa se arma en un búfer, con los números formateados por `std::to_chars`. Las capas pasan por un pipeline de
  oneTBB: primero, en paralelo, los candidatos de la costura alineada de cada pared (`seam_candidates`);
  `plan_layer` ordena y elige las costuras, en orden, porque sigue desde donde terminó la capa de abajo; los
  recorridos se simplifican, toman la costura en bufanda (`scarf.*`) y se planean sus viajes en paralelo;
  `write_layer` escribe el texto, en orden. Mientras se escribe una capa se planean las siguientes, y una capa que se
  frena por el tiempo mínimo se vuelve a escribir desde el mismo plan.
- **`scarf.*`:** la costura en bufanda (`walls_extra.md`, regla 4): el lazo empieza en una rampa desde la capa de
  abajo y termina pasando otra vez sobre ella, con el alto de cada cordón en sus puntos (como el relleno ondulado).
- **`check.*`:** relee y verifica el G-code. Lee el texto en bloques de unos 256 KiB de líneas enteras, en paralelo,
  y sigue el estado en orden.
- **`export.*`:** junta escribir y verificar en una exportación que nunca deja un archivo malo con el nombre pedido.
- **`preview.*`:** lee G-code (el propio o cualquier otro) en movimientos con su tipo, ancho, alto, avance y capa,
  para la vista previa de la interfaz.

### `gpu/`: cómputo opcional

Vulkan a través de volk, cargado en tiempo de ejecución: sin GPU, el motor sigue en la CPU. Los shaders están en
`src/gpu/shaders/*.comp` y se compilan a SPIR-V con `tools/compile_shaders.py`; el SPIR-V generado se guarda en el
repositorio. Hoy corren en la GPU el rango de capas por triángulo, los segmentos de cada capa y la columna del
soporte en su grilla de bits (`support_column.comp`, con los mismos bits que `support_column` de la CPU).

### `fea/`: análisis estructural

| Archivo | Qué hace |
|---|---|
| `voxels.*` | La pieza en vóxeles con forma de caja; `layer_planes` parte cada capa en filas enteras |
| `hex8.*` | El hexaedro trilineal: rigidez por Gauss 2 × 2 × 2 y deformación en el centro |
| `material.*` | El material ortótropo impreso: rigidez girada al cordón, promedio en la capa, Tsai–Wu, modo de falla |
| `materials.*` | La base de materiales con sus fuentes y las reglas para lo que falta |
| `operator.*` | La rigidez sin ensamblar: el producto junta, nodo por nodo, los bloques de los ocho elementos de alrededor |
| `multigrid.*` | El precondicionador: niveles con operador de Galerkin elemento por elemento, Chebyshev, Cholesky abajo |
| `elasticity.*` | El gradiente conjugado, las piezas sueltas, el análisis «como se imprimió» y el factor promediado |
| `as_printed.*` | La dirección del cordón de cada vóxel desde los recorridos, y los huecos del relleno |
| `analysis.*` | El análisis completo de un caso: resolución, regiones, resumen, avisos, chequeo de resolución, estimación |

Las salidas del análisis están en `report/analysis_json.*` (informe), `io/vtk.*` (mapa) e `io/ply.*` (malla). El
caso se lee en `config/analysis_case.*`.

### `ui/`: la interfaz sin OpenGL

La biblioteca `tisma_ui`, que usa el programa `tisma` (`apps/tisma/`, ver `docs/UI.md`):
- **`camera.*`:** cámara orbital con +Z arriba, encuadre y vistas estándar.
- **`scene.*`:** los datos de dibujo, como arreglos contiguos: la malla con normales, la grilla de la cama recortada
  a su polígono y las instancias de la vista previa (40 bytes por extrusión).
- **`icons.*`:** los íconos SVG de Tisma a imágenes, con nanosvg, recoloreados al tema.
- **`png.*`:** escritura de PNG para las capturas.
- **`text.*`:** todos los textos de la interfaz en español e inglés.
- **`files.*`:** listado de carpetas para el explorador propio.
- **`slicing.*`:** el laminado completo en segundo plano (comprobaciones, G-code en memoria, verificación, vista
  previa) y la estimación de vóxeles del análisis.
- **`presets.*`:** los perfiles de fábrica (`docs/PROFILES.md`).

El programa agrega lo que necesita una ventana: GLFW, Dear ImGui, un cargador propio de OpenGL 3.3 y el dibujo 3D.

### `calibration/`, `project/`, `catalog/`, `device/`

- **`calibration/calibration.*`:** las piezas de prueba de las calibraciones, generadas (bloque de dimensiones,
  torres), sus cambios por altura, los rangos automáticos, usar el valor elegido y la compensación desde medidas
  (`docs/CALIBRATION.md`).
- **`catalog/catalog.*`:** el catálogo de impresoras y filamentos de los fabricantes, incrustado desde
  `resources/catalog/`, y los perfiles que salen de cada entrada (`docs/CATALOG.md`).
- **`project/project.*`:** el archivo de proyecto `.tisma`: un JSON con los ajustes, los perfiles, las piezas y la
  calibración, y cada malla como STL binario, en partes con etiqueta (`docs/PROJECT.md`).
- **`slice/local_layers.*`:** la altura de capa por zona (`docs/LOCAL_LAYERS.md`): los grupos de subcapas, sus zonas
  gruesas y el recorte de los caminos.
- **`infill/stress_density.*`:** el relleno guiado por tensiones (`docs/INFILL.md`): la carga por vóxel del análisis
  de la pieza maciza y las bandas de densidad por capa.
- **`infill/combine.*`:** combinar el relleno disperso de varias capas en una más gruesa (`docs/INFILL.md`): las
  corridas de capas; el pipeline corta la parte común y la imprime como capa gruesa.
- **`slice/zaa.*`:** Z anti-aliasing (`docs/ZAA.md`): el índice de las caras que miran hacia arriba y el corte de
  los caminos visibles, con la altura de cada punto (`ToolpathPoint::dz`) que escribe el G-code.
- **`slice/variable_layers.*`:** la altura variable por Z (`docs/VARIABLE_LAYERS.md`): planea la pila de capas con
  la lista de tramos, la pendiente (escalón máximo) y las subcapas de la altura por zona, y cuenta las pieles en capas
  gruesas.
- **`device/`:** las impresoras conectadas (`docs/DEVICE.md`): la interfaz `Printer`, el puerto serie, el host de
  G-code por USB en su propio hilo y la impresora simulada de las pruebas.

### `config/`, `report/`, `cli/`

- **`settings.*`:** lee los ajustes con nlohmann/json. Una clave desconocida es un error que la nombra.
  `settings_json` los escribe en el mismo formato.
- **`profiles.*`:** los perfiles con nombre: a qué perfil (impresora, filamento, impresión) pertenece cada ajuste,
  elegir uno, su archivo y la carpeta donde se guardan (`docs/PROFILES.md`).
- **`report/warnings.*`:** los avisos del laminado en JSON.
- **`cli/`:** los comandos `slice`, `analyze` e `info`, con sus códigos de salida (`ExitCode` en `cli.hpp`). Cada
  archivo de salida se escribe como `.part` y se renombra al terminar.
