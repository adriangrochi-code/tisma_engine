# Tisma engine

Laminador (slicer) para impresión 3D FDM, escrito desde cero para Tisma: el motor y su interfaz `tisma`. Va a
reemplazar a Tisma Slicer sin perder ninguna de sus funciones (decisión del dueño). Está en desarrollo: ver `PLAN.md`
y `PROGRESS.md`.

## Licencia: solo uso no comercial

El código es público para que se pueda leer y compilar, pero **no es software libre ni de código abierto** en el
sentido de la OSI: es de *código disponible*. Se publica bajo la
[PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0) (texto completo en
`LICENSE`).

- **Permitido:** usarlo, estudiarlo, modificarlo y compartirlo con fines no comerciales (uso personal, aficionado,
  estudio, investigación; también instituciones educativas, de investigación pública, ONG y organismos públicos).
  Quien lo comparta tiene que incluir `LICENSE` y su línea `Required Notice`.
- **No permitido:** ningún uso comercial. Por ejemplo venderlo, incluirlo en un producto o servicio pago, o usarlo para
  laminar como parte de un negocio.
- Para un uso comercial hace falta una licencia aparte, por escrito, del dueño del proyecto (cuenta de GitHub
  `adriangrochi-code`).

Las bibliotecas de terceros conservan sus propias licencias: ver `THIRD_PARTY.md`.

**English:** this repository is public so it can be read and built, but it is source-available, not open source. It is
licensed under the PolyForm Noncommercial License 1.0.0. Any commercial use is prohibited unless the owner grants a
separate written license.

## Compilar

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
build/tisma-engine --help
```

Hace falta CMake 3.25 o más nuevo y un compilador de C++20. La configuración descarga las dependencias de
`THIRD_PARTY.md`. Más detalles, las pruebas y las convenciones están en `docs/DEVELOPMENT.md`; cómo funciona el código, en
`docs/ARCHITECTURE.md`.

## Versiones

La versión actual está en `VERSION` y lo que trae cada una en `CHANGELOG.md`. Se sigue el versionado semántico, con
pre-releases (`-alpha`, `-beta`, `-rc`) mientras una fase está en curso; las releases se publican en GitHub con los
binarios de Windows y Linux (`docs/VERSIONING.md`).

## Usar

La interfaz, `tisma`, abre un modelo, cambia los ajustes, lamina, muestra el G-code capa por capa y lo exporta
verificado (`docs/UI.md`). Guarda perfiles de impresora, filamento e impresión (`docs/PROFILES.md`) y proyectos
`.tisma` (`docs/PROJECT.md`), y trae calibraciones guiadas (`docs/CALIBRATION.md`):

```sh
build/apps/tisma/tisma modelo.stl
```

La línea de comandos hace lo mismo sin ventana:

```sh
build/tisma-engine slice modelo.stl --config ajustes.json -o modelo.gcode --warnings avisos.json
```

Los ajustes están en `docs/CONFIG.md`. `--warnings` es opcional y escribe los avisos del laminado en JSON:
`{"version": 1, "warnings": [{"kind", "layer", "z", "message", "value"}]}`. Los tipos de aviso son:
- `thin_detail`: área en mm² de una capa más fina que el cordón más fino, que no se imprime;
- `layer_time`: segundos que dura una capa que no llega al tiempo mínimo ni a 1 mm/min;
- `open_contours`: contornos abiertos descartados en la capa;
- `gpu_fallback`: la GPU falló y se laminó con la CPU (sin capa: `layer` y `z` son `null`).

Códigos de salida: 0 bien; 1 línea de comandos mal escrita; 2 un archivo de entrada falta, está mal o no se puede
escribir la salida; 3 orden todavía sin hacer; 4 el modelo no se puede imprimir (sin triángulos, ninguna capa cerrada,
fuera de `machine.bed`, más alto que `machine.max_height`, falda o borde fuera de la cama, o nada que extruir); 5 el
análisis no dio un resultado válido; 6 el G-code no pasó su verificación (queda como `<archivo>.rejected`). Cada
salida se escribe como `<archivo>.part` y se renombra al terminar: con error, o si el proceso se corta, no queda un
archivo a medias. El G-code además se vuelve a leer desde el disco y se verifica antes de quedar con su nombre
(`docs/GCODE_CHECK.md`).

Análisis estructural de la pieza tal como se imprimiría (material ortótropo, adherencia entre capas y entre
líneas, factor de seguridad por vóxel):

```sh
build/tisma-engine analyze modelo.stl --config ajustes.json --case caso.json -o informe.json --vtk mapa.vtk
```

El caso (material, calidad, apoyos y cargas), el informe y el mapa para ParaView están en `docs/ANALYSIS.md`; los
materiales y sus fuentes, en `docs/MATERIALS.md`.
