# Desarrollo

Cómo compilar, probar y cambiar Tisma engine sin romper nada. Cómo está armado el código está en
`docs/ARCHITECTURE.md`. Antes de cualquier aporte, leer `CONTRIBUTING.md`: por ahora no se aceptan aportes de otras
personas hasta que exista un acuerdo de cesión de derechos. También hay que leer `CLEANROOM.md`, que tiene las reglas
de sala limpia y de licencia.

## Compilar

Hace falta CMake 3.25 o más nuevo y un compilador de C++20 (gcc 13, clang 18 o MSVC 2022 están probados).

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
build/tisma-engine --help
```

La configuración descarga las dependencias a la versión exacta de `THIRD_PARTY.md`: oneTBB, Clipper2, nlohmann/json,
volk, los encabezados de Vulkan, Boost.Polygon con Boost.Config (el eje medio de referencia de las pruebas) y Catch2. Para compilar sin red, cada una acepta
`-DFETCHCONTENT_SOURCE_DIR_<NOMBRE>=<carpeta>` con una copia del mismo tag.

Opciones:

| Opción | Por defecto | Qué hace |
|---|---|---|
| `TISMA_BUILD_TESTS` | `ON` | Compila las pruebas |
| `TISMA_GPU` | `ON` | Compila el cómputo con Vulkan; sin dispositivo, el motor usa la CPU igual |

## Probar

```sh
ctest --test-dir build --output-on-failure -j 8
build/tests/tisma_tests "[gcode]"          # solo las pruebas con esa marca
```

- Las pruebas están en `tests/test_<módulo>.cpp`. Las marcas (`[gcode]`, `[fea]`, `[analysis]`, `[cli]`…) permiten
  correr una parte.
- CTest además corre:
  - el programa de punta a punta (`cli_*`);
  - la revisión de licencias (`license_check`);
  - la de especificaciones (`spec_check`);
  - la del registro de cambios (`release_notes_check`).
- **Pruebas de GPU:** sin dispositivo Vulkan se saltean. Con `TISMA_REQUIRE_GPU=1`, la falta de dispositivo hace
  fallar la prueba. Sin GPU, lavapipe (Mesa) sirve.
- **Antes de cada envío**, `tools/ci_linux.sh` corre lo mismo que la CI de Linux:
  - el SPIR-V generado;
  - gcc y clang en Release;
  - gcc con ASan + UBSan;
  - todas las pruebas con GPU obligatoria.

  En GitHub, la CI corre Windows en cada envío. Linux corre solo a mano.

Cada cambio de comportamiento llega con su prueba. Lo que se puede medir se mide: una prueba que compara con un valor
exacto o con una referencia es mejor que una que solo comprueba que «no falla».

## Convenciones del código

- **Estilo:**
  - C++20, sin extensiones del compilador;
  - 4 espacios de sangría y líneas de hasta 120 columnas;
  - llaves en la misma línea;
  - `snake_case` para funciones y variables, `PascalCase` para tipos, `kNombre` para constantes;
  - miembros privados con `_` al final.
- **Advertencias:** se compila con `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` (`/W4` en MSVC). El código nuevo
  no agrega advertencias en gcc ni en clang.
- **Idiomas:**
  - en inglés: los comentarios, los nombres, los mensajes de los commits y los de la línea de comandos;
  - en español: la documentación de `docs/` y los archivos de la raíz.
- **Comentarios:**
  - cada cabecera explica qué hace cada función pública, con unidades, casos borde y la fuente del método («Kuipers
    et al. 2020, sección 3.4», «own design; see PROVENANCE.md»);
  - adentro, se comenta el porqué, no el qué.
- **Determinismo:** nada de orden de iteración de mapas hash en lo que sale, ni de sumas en paralelo sin un orden
  fijo, ni de números al azar sin semilla fija (ver «Reglas» en `docs/ARCHITECTURE.md`).
- **Commits:** Conventional Commits (`feat(gcode): …`, `fix(slice): …`, `docs: …`, `perf: …`, `ci: …`). El cuerpo
  explica el porqué y lo que se midió. La historia no se reescribe nunca: es el registro de la procedencia.

## Recetas

### Agregar un ajuste

1. El campo con su valor por defecto en `Settings` (`src/config/settings.hpp`).
2. Su lectura en `parse_settings`: el grupo, la clave, el tipo y el rango, con un error que nombre la clave.
3. Su línea en `describe`, que va al final del G-code.
4. Su fila en `docs/CONFIG.md`: clave, tipo, valor por defecto, rango y significado.
5. Pruebas: el valor leído, el rango rechazado y el efecto en el resultado.

### Agregar un módulo

1. Una carpeta o archivos en `src/<área>/`, con la cabecera documentada y el `.cpp` en la lista de `tisma_core`, en
   `CMakeLists.txt`.
2. Su fila en `PROVENANCE.md`: artículo y sección, especificación (`docs/spec/…`) o diseño propio.
3. Sus pruebas en `tests/test_<módulo>.cpp`, agregadas en `tests/CMakeLists.txt`.
4. Si cambia lo que ve quien usa el motor: `docs/` y la sección «Sin publicar» de `CHANGELOG.md`.

### Agregar una biblioteca

Solo licencias permisivas: MIT, BSD, BSL-1.0, Apache-2.0, Zlib, y MPL-2.0 sin modificar. Nada GPL, LGPL ni «no
comercial». Antes de usarla:
1. Leer su licencia en el tag exacto, incluido lo que trae adentro.
2. Anotarla en `THIRD_PARTY.md` con nombre, versión, licencia, enlace y para qué se usa.
3. Fijar la versión en `CMakeLists.txt`.

`tools/check_licenses.py` (en CTest) falla si una dependencia no está anotada.

### Agregar un material

Un registro en `kRecords` (`src/fea/materials.cpp`) solo con los números que publica su fuente, y su fila con el
enlace en `docs/MATERIALS.md`. Las reglas de `as_printed` completan el resto.

### Publicar una versión

Ver `docs/VERSIONING.md`: número en `VERSION`, sección en `CHANGELOG.md`, comprobaciones en verde y la etiqueta
`v<versión>`. El flujo `release.yml` compila, prueba y publica.

## Sala limpia

Todo el código se escribe acá, desde especificaciones (`docs/spec/`), artículos, formatos públicos y bibliotecas
permisivas. Nunca se lee, copia ni traduce código de otros laminadores (PrusaSlicer, Slic3r, SuperSlicer, OrcaSlicer,
Bambu Studio, Cura y CuraEngine) ni de Tisma Slicer. Esos programas solo se comparan como cajas negras. Tampoco se
copia código de Stack Overflow, blogs o artículos. Los detalles están en `CLEANROOM.md`.
