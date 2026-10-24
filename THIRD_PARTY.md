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
| oneTBB | TBB | 2022.3.0 (commit `f1862f38f83568d96e814e469ab61f88336cc595`) | Apache-2.0 | `LICENSE.txt` y `third-party-programs.txt` del tag `v2022.3.0`. Incluye ITT Notify (BSD-3-Clause) y hwloc (BSD-3-Clause). | https://github.com/uxlfoundation/oneTBB | Paralelismo por capas y ordenamiento paralelo. Se enlaza estática. Al distribuir el programa hay que incluir su licencia y sus avisos. |

## Para revisar antes de sumar

- **glslang** (compilador de shaders para Vulkan, F1): revisar su archivo de licencia en la versión exacta. Tiene
  partes con licencias distintas, y alguna generada por Bison podría venir con GPL-3.0 más la excepción de Bison. Si
  algo no es permisivo, compilar los shaders fuera del programa o usar otro compilador.
