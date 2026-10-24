# Tisma engine

Motor de laminado (slicer) para impresión 3D FDM, escrito desde cero para Tisma. Está en desarrollo: ver `PLAN.md`
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

Hace falta CMake 3.25 o más nuevo y un compilador de C++20. La configuración descarga oneTBB y, para las pruebas, Catch2.

## Usar

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
fuera de `machine.bed` o más alto que `machine.max_height`). Con error no queda G-code a medias.
