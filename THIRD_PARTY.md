# Componentes de terceros

Antes de usar una biblioteca se anota acá con:
- su versión exacta;
- su licencia, leída en su archivo de licencia en esa versión, y las licencias de lo que trae adentro;
- su URL.

**La licencia del motor tiene que poder cerrarse** (ver «La licencia tiene que poder cerrarse» en `CLEANROOM.md`).
Por eso solo se aceptan licencias permisivas: MIT, BSD-2-Clause, BSD-3-Clause, BSL-1.0, Apache-2.0 y Zlib.
MPL-2.0 (Eigen) se acepta solo sin modificar sus archivos. Nada GPL, LGPL, AGPL, SSPL, CC-BY-SA ni licencias «no
comerciales» de terceros.

La columna «CMake» es el nombre de `FetchContent_Declare`. `tools/check_licenses.py` comprueba, en cada compilación
de pruebas y en la CI, que cada dependencia de CMake esté en esta tabla con una licencia permitida.

| Biblioteca | CMake | Versión | Licencia | Leída en | URL | Uso |
|---|---|---|---|---|---|---|
| Catch2 | Catch2 | 3.7.1 (commit `fa43b77429ba76c462b1898d6cd2f2d7a9416b14`) | BSL-1.0 | `LICENSE.txt` del tag `v3.7.1` | https://github.com/catchorg/Catch2 | Solo pruebas. No se distribuye con el programa. |
| Clipper2 | Clipper2 | 1.5.4 (commit `ef88ee97c0e759792e43a2b2d8072def6c9244e8`) | BSL-1.0 | `LICENSE` del tag `Clipper2_1.5.4` | https://github.com/AngusJohnson/Clipper2 | Uniones, diferencias y offsets de polígonos por capa. Solo la biblioteca de C++ (`CPP/Clipper2Lib`), sin Z, sin utilidades, ejemplos ni pruebas. |
| Vulkan-Headers | VulkanHeaders | vulkan-sdk-1.4.350.1 (commit `8864cdc896bbc2a9b6eb36b3218fc9ef57908d77`) | Apache-2.0 AND MIT | `LICENSE.md` y `LICENSES/` del tag | https://github.com/KhronosGroup/Vulkan-Headers | Cabeceras de la API de Vulkan para el backend de GPU (opción `TISMA_GPU`). |
| volk | volk | vulkan-sdk-1.4.350.1 (commit `3ca312a4f38baa63d8006b6905abbeeb89c8087d`) | MIT | `LICENSE.md` del tag | https://github.com/zeux/volk | Carga Vulkan en tiempo de ejecución: sin Vulkan, el motor arranca y usa la CPU. |
| nlohmann/json | nlohmann_json | 3.12.0 (commit `55f93686c01528224f448c19128836e7df245f72`) | MIT | `LICENSE.MIT` y `.reuse/dep5` del tag `v3.12.0`. Todas las cabeceras de `include/` llevan `SPDX-License-Identifier: MIT`. El repositorio trae un archivo GPL-3.0-only solo entre sus pruebas (`tests/thirdparty/imapdl`), que no se compila ni se distribuye. | https://github.com/nlohmann/json | Lectura del archivo de ajustes (`docs/CONFIG.md`). Solo cabeceras. |
| Boost.Polygon | BoostPolygon | boost-1.90.0 (commit `8ba35b57c1436c4b36f7544aadd78c2b24acc7db`) | BSL-1.0 | No trae archivo de licencia en la raíz; todas las cabeceras de `include/` llevan el aviso BSL-1.0, que remite al texto estándar (el mismo leído en Catch2 y Clipper2). No hay nada GPL. | https://github.com/boostorg/polygon | Diagrama de Voronoi de segmentos, para el eje medio de los perímetros (F2). Solo cabeceras. |
| Boost.Config | BoostConfig | boost-1.90.0 (commit `a7d5a9b05d70c9cfea980dc3539ca3d3461411b3`) | BSL-1.0 | Igual que Boost.Polygon: aviso BSL-1.0 en todas las cabeceras. | https://github.com/boostorg/config | La única dependencia de Boost.Polygon fuera de sí misma (`boost/config.hpp`, `boost/cstdint.hpp`). Solo cabeceras. |
| oneTBB | TBB | 2022.3.0 (commit `f1862f38f83568d96e814e469ab61f88336cc595`) | Apache-2.0 | `LICENSE.txt` y `third-party-programs.txt` del tag `v2022.3.0`. Incluye ITT Notify (BSD-3-Clause) y hwloc (BSD-3-Clause). | https://github.com/uxlfoundation/oneTBB | Paralelismo por capas y ordenamiento paralelo. Se enlaza estática. Al distribuir el programa hay que incluir su licencia y sus avisos. |
| GLFW | glfw | 3.4 (commit `7b6aead9fb88b3623e3b3725ebb42670cbe4c579`) | Zlib | `LICENSE.md` del tag `3.4`; todos los archivos de `src/` llevan el mismo aviso. `deps/` (glad, getopt, tinycthread, nuklear, linmath, stb_image_write) solo lo usan sus ejemplos y pruebas, que no se compilan; `deps/mingw` solo se usa con MinGW y `deps/wayland` solo con Wayland, los dos apagados. | https://github.com/glfw/glfw | Solo la interfaz (`TISMA_BUILD_UI`): la ventana, el contexto de OpenGL, el teclado, el ratón y soltar archivos. Se enlaza estática. En Linux usa X11 del sistema. |
| Dear ImGui | imgui | 1.92.9b (commit `f1cc2ae15e53a861a874c3034aae6798fde194ab`) | MIT AND Apache-2.0 | `LICENSE.txt` del tag `v1.92.9b` (MIT). Trae `imstb_truetype.h`, `imstb_rectpack.h` e `imstb_textedit.h` con licencia doble MIT o dominio público: se elige MIT. Las fuentes incrustadas ProggyClean y ProggyForever son MIT (`docs/FONTS.md`). `misc/fonts/Roboto-Medium.ttf` (versión 2.001152) es Apache-2.0, según `docs/FONTS.md` y la tabla `name` del propio archivo. El cargador `backends/imgui_impl_opengl3_loader.h` es de dominio público (Unlicense, fuera de la lista): no se compila, el motor usa su propio cargador (`IMGUI_IMPL_OPENGL_LOADER_CUSTOM`). | https://github.com/ocornut/imgui | Solo la interfaz: los paneles, botones y controles, con sus backends de GLFW, OpenGL 3 y Vulkan (`backends/imgui_impl_vulkan.cpp`, MIT como el resto, sobre volk; solo con `TISMA_GPU`), y Roboto Medium como fuente, incrustada sin modificar. Al distribuir el programa hay que incluir su licencia y la de Apache-2.0 de la fuente. |
| nanosvg | nanosvg | commit `239e102ec2c691f2902e20ace2ed36ee4a35cfe6` (2026-07-10; el proyecto no tiene tags) | Zlib | `LICENSE.txt` en ese commit. Su `AI_POLICY.md` rige los aportes a nanosvg, no su uso. | https://github.com/memononen/nanosvg | Solo la interfaz: lee y rasteriza los íconos SVG de `assets/brand/`. Solo cabeceras. |

## Herramientas de compilación (no se enlazan ni se distribuyen)

| Herramienta | Versión | Licencia | Uso |
|---|---|---|---|
| glslang (`glslangValidator`) | 15.1.0 (Ubuntu 24.04, paquete `glslang-tools`); licencia leída en el tag `vulkan-sdk-1.4.350.1`, commit `275822a6261ee689aadb1da5f09a0ec2f058685c` | Mezcla: el núcleo BSD-3-Clause; el preprocesador suma una licencia permisiva propia de NVIDIA; partes MIT y Apache-2.0; conserva el texto «GPL 3 with special bison exception», aunque dice que Bison ya no se usa | `tools/compile_shaders.py` pasa los shaders de GLSL a SPIR-V. El SPIR-V generado se guarda en `src/gpu/shaders/generated/` y es salida de la herramienta, no código de glslang. Nunca se enlaza ni se distribuye. |
| Mesa lavapipe | la de Ubuntu 24.04 (paquete `mesa-vulkan-drivers`) | MIT | Solo en la CI y en pruebas locales: un dispositivo Vulkan por software para correr las pruebas de GPU sin GPU. |
