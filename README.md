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

**Complementos:** cada complemento (`docs/spec/features/plugins.md`) elige su propia licencia, también comercial o
cerrada. La licencia del motor no los alcanza: son programas aparte que intercambian archivos con él (decisión del
dueño, 26-10-2026).

**English:** this repository is public so it can be read and built, but it is source-available, not open source. It is
licensed under the PolyForm Noncommercial License 1.0.0. Any commercial use is prohibited unless the owner grants a
separate written license. Plugins choose their own license; the engine's license does not extend to them.

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
- `long_bridge`: el tramo más largo, en mm, de un puente sin nada debajo, si pasa `support.free_bridge_length`; uno por
  cada serie de capas seguidas;
- `gpu_fallback`: la GPU falló y se laminó con la CPU (sin capa: `layer` y `z` son `null`).

Un proyecto guardado por la interfaz se lamina entero, como lo hacen los nodos de Tisma Cloud (ficha `cloud.md`,
regla 18):

```sh
build/tisma-engine slice proyecto.3mf -o placa.gcode --summary resumen.json --sandbox
```

Por la salida estándar va solo una línea `progress N` (0 a 100) cada vez que avanza; los avisos van a la salida de
errores. El resumen lleva `print_time` («1h 2m 3s»), `print_time_s`, `filament_mm`, `filament_g` (solo si se conoce
la densidad del filamento) y `layers`, o `error` si falló. `--sandbox` no corre los scripts de posproceso de los
ajustes; `--no-gpu` lamina con la CPU. En cualquier orden, `--threads <n>` usa como mucho `n` hilos de cálculo.

Para cambiar ajustes del proyecto sin tocar el archivo, en `slice`, `analyze`, `aero` y `orient` de un proyecto:
- `--config-over <parcial.json>`: un JSON con solo los grupos y claves que cambian, encima de los del proyecto;
- `--set grupo.clave=valor`: una clave, se puede repetir y va después de `--config-over`. Un número, `true`, `false`,
  una lista o un objeto se leen como JSON; lo demás, como texto.

Una clave desconocida o un valor fuera de rango es un error (código 2, `bad_settings` en el resumen).

Para el teléfono, que solo corre `tisma-engine`:
- `part gear|fastener <spec.json> -o <pieza.stl>` arma un engranaje o una pieza de tornillería desde su JSON (el mismo
  de la pestaña Diseño); `--defaults` da uno para empezar;
- `calibration <tipo> --config <ajustes.json> --model <pieza.stl> --settings <ajustes.json>` arma la pieza de una
  calibración y los ajustes con sus cambios por altura, listos para `slice`; con `--start`, `--end`, `--step` y
  `--segment`, otro rango;
- `calibration apply <tipo> --value <v> --config … -o …` escribe el valor que salió mejor.
- `aero` y `orient` también toman un modelo con `--config <ajustes.json>` en lugar de un proyecto.
- `design new | check <diseño.json> | build <diseño.json> --solidcraft <exe> -o <pieza.stl|.3mf>`: la lógica de la
  pestaña Diseño con el `solidcraft-cli` que se le indique. Opciones: `--quality`, `--timeline` (la primera operación
  que falla) y `--save` (el archivo `.sc`).

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
