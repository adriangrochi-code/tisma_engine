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

Hace falta CMake 3.21 o más nuevo y un compilador de C++20. La configuración descarga Catch2 para las pruebas.
