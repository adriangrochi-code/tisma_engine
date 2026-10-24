# Registro de cambios

Los cambios de Tisma engine que importan a quien lo usa, por versión. El formato sigue
[Keep a Changelog 1.1.0](https://keepachangelog.com/es-ES/1.1.0/) y los números de versión, el
[versionado semántico 2.0.0](https://semver.org/lang/es/). La política está en `docs/VERSIONING.md`.

Cada versión agrupa sus cambios en estas categorías: **Agregado** (funciones nuevas), **Cambiado** (lo que ya existía
y ahora se comporta distinto), **Obsoleto** (lo que se va a quitar), **Quitado**, **Corregido** (errores) y
**Seguridad**. Una categoría sin cambios no se escribe.

## [Sin publicar]

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
- `--screenshot` (con `--export`) dibuja la interfaz sin pantalla, para pruebas y capturas.
- Los ajustes se pueden escribir como JSON en su formato (`settings_json`), y leer de vuelta iguales.
- Un lector de G-code para la vista previa, que también abre G-code de otros programas.
- Las releases traen también la interfaz.

### Cambiado

- `machine.bed` acepta también una lista vacía, su valor por defecto.
- La release se publica sola al enviar a `dev` o `main` un `VERSION` nuevo: el flujo crea la etiqueta.

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

[Sin publicar]: https://github.com/adriangrochi-code/tisma_engine/compare/v0.6.0-alpha.1...HEAD
[0.6.0-alpha.1]: https://github.com/adriangrochi-code/tisma_engine/releases/tag/v0.6.0-alpha.1
