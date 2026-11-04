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

Cada biblioteca que se distribuye con el programa tiene además su entrada en `resources/credits.json` y su archivo de
licencia en `resources/licenses/`, que la interfaz muestra en Más › Créditos y fuentes (ficha `credits.md`). La prueba
«credits: every library the program carries has its license and notice» falla si falta alguna.

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
| SolidCraft | — (`solidcraft-cli`; no se compila ni se enlaza: el instalador lleva su ejecutable publicado) | 0.30.0 (release `v0.30.0`, 2026-11-02; `solidcraft-windows-x64-portable.zip`, SHA-256 `ae5f502bc0062656965bff31f50739504e24c219baf520526fbd9208dfc261ba`; `solidcraft-0.30.0-linux-x86_64.tar.gz`, SHA-256 `5cd95727352b9a343fc8ff4bfb688671a5901a1b601a71c46e94c7b20750c9a0`) | MIT OR Apache-2.0 | `LICENSE-MIT`, `LICENSE-APACHE` y `NOTICE` del paquete; su `THIRD-PARTY-LICENSES.txt` (560 cajas de Rust, revisado el 2026-10-29: permisivas o con opción permisiva, ninguna GPL, LGPL ni AGPL obligatoria; fuentes OFL 1.1 y modelos CC0 o de dominio público). Núcleo B-rep: truck (Apache-2.0). | https://github.com/tangency-labs/solidcraft | Solo la interfaz: la pestaña Diseño (ficha `cad_design.md`, regla 1) le manda guiones JSON de comandos al ejecutable `solidcraft-cli` que va en la carpeta `solidcraft/` junto al programa, y lee el 3MF que devuelve. Es otro proceso: el motor no compila ni enlaza nada suyo. El paquete suma 110 MB sin comprimir (43 MB en el zip). Al distribuirlo van sus licencias, su `NOTICE` y su `THIRD-PARTY-LICENSES.txt`. Crédito: «Basado en SolidCraft». |

SolidCraft en la app de Android (decisión del dueño, 2026-11-04): el flujo `.github/workflows/solidcraft-android.yml` compila `solidcraft-cli` desde la etiqueta `v0.30.0` (commit `afd81380ab06b0e77e03fdf68185d4b484d0e84e`, el mismo de la fila de arriba, MIT OR Apache-2.0) para arm64-v8a y x86_64 con el NDK r29, y lo guarda como artefacto con `LICENSE-MIT`, `LICENSE-APACHE`, `NOTICE` y `THIRD-PARTY-LICENSES.txt`. Con `-Ptisma.solidcraft=<carpeta>` la app lo lleva como `lib/<abi>/libsolidcraft.so` y sus licencias van a los créditos de la app. Las cajas de Rust que entran en la compilación para Android son las del `Cargo.lock` de esa versión (`--locked`); hay que revisar su `THIRD-PARTY-LICENSES.txt` del artefacto antes de publicar un APK que lo lleve.
| nanosvg | nanosvg | commit `239e102ec2c691f2902e20ace2ed36ee4a35cfe6` (2026-07-10; el proyecto no tiene tags) | Zlib | `LICENSE.txt` en ese commit. Su `AI_POLICY.md` rige los aportes a nanosvg, no su uso. | https://github.com/memononen/nanosvg | Solo la interfaz: lee y rasteriza los íconos SVG de `assets/brand/`. Solo cabeceras. |
| cpp-httplib | cpphttplib | 0.62.0 (commit `74ef3c42188acd6391f027c4337fb0e1a68c0024`, 2026-10-18) | MIT | `LICENSE` del tag `v0.62.0`; `httplib.h` dice «MIT License», un solo autor. Se usa solo la cabecera, sin su CMake; no se definen sus opciones de zlib, brotli ni zstd, así que no trae nada más. | https://github.com/yhirose/cpp-httplib | El cliente HTTP de la nube en Linux y macOS (`src/net/http_httplib.cpp`, ficha `cloud.md`) y el servidor simulado de las pruebas (`tests/test_cloud.cpp`). En Windows el cliente usa WinHTTP, del sistema. |
| OpenSSL | — (la del sistema: `find_package(OpenSSL 3)`; no se compila) | 3.0 o más nueva, la del sistema (en la sesión y la CI de Linux, 3.0.13 de Ubuntu 24.04) | Apache-2.0 | `LICENSE.txt` de OpenSSL 3.0 (Apache-2.0 desde la 3.0; las 1.x tenían otra licencia y no se aceptan: se pide la 3). | https://www.openssl.org | HTTPS del cliente de la nube en Linux y macOS, enlazada como biblioteca compartida del sistema; el paquete no la lleva. Sin OpenSSL 3 el cliente solo habla `http://`. |

## Herramientas de compilación (no se enlazan ni se distribuyen)

| Herramienta | Versión | Licencia | Uso |
|---|---|---|---|
| glslang (`glslangValidator`) | 15.1.0 (Ubuntu 24.04, paquete `glslang-tools`); licencia leída en el tag `vulkan-sdk-1.4.350.1`, commit `275822a6261ee689aadb1da5f09a0ec2f058685c` | Mezcla: el núcleo BSD-3-Clause; el preprocesador suma una licencia permisiva propia de NVIDIA; partes MIT y Apache-2.0; conserva el texto «GPL 3 with special bison exception», aunque dice que Bison ya no se usa | `tools/compile_shaders.py` pasa los shaders de GLSL a SPIR-V. El SPIR-V generado se guarda en `src/gpu/shaders/generated/` y es salida de la herramienta, no código de glslang. Nunca se enlaza ni se distribuye. |
| Mesa lavapipe | la de Ubuntu 24.04 (paquete `mesa-vulkan-drivers`) | MIT | Solo en la CI y en pruebas locales: un dispositivo Vulkan por software para correr las pruebas de GPU sin GPU. |

## App de Android (`apps/android/`, Gradle)

Las bibliotecas de la app (ficha `mobile_app.md`, regla 4) se declaran en `apps/android/gradle/libs.versions.toml`;
`tools/check_licenses.py` comprueba que cada una esté en esta tabla (columna «Gradle») con una licencia permitida, o en
la de herramientas de prueba de abajo. Revisado el 2026-11-04: las 92 bibliotecas que entran al APK (las de la tabla y
sus dependencias transitivas, según `./gradlew :app:dependencies --configuration releaseRuntimeClasspath`) son
Apache-2.0 según la licencia declarada en el POM publicado de cada versión; `com.google.guava:listenablefuture:1.0` no
la declara en su POM y la hereda de `guava-parent` (Apache-2.0). Ninguna trae GPL, LGPL ni AGPL. La lectura fue del POM,
no del archivo de licencia de cada fuente: antes de publicar la app, `resources/credits.json` y los avisos van a salir
de los mismos datos.

| Biblioteca | Gradle | Versión | Licencia | Leída en | URL | Uso |
|---|---|---|---|---|---|---|
| Kotlin coroutines | org.jetbrains.kotlinx:kotlinx-coroutines-core | 1.10.2 | Apache-2.0 | POM publicado | https://github.com/Kotlin/kotlinx.coroutines | Hilos de la app y seguimiento de trabajos. Trae la biblioteca estándar de Kotlin (Apache-2.0) y `org.jetbrains:annotations` (Apache-2.0). |
| Kotlin coroutines (Android) | org.jetbrains.kotlinx:kotlinx-coroutines-android | 1.10.2 | Apache-2.0 | POM publicado | https://github.com/Kotlin/kotlinx.coroutines | El hilo principal de Android. |
| kotlinx.serialization JSON | org.jetbrains.kotlinx:kotlinx-serialization-json | 1.9.0 | Apache-2.0 | POM publicado | https://github.com/Kotlin/kotlinx.serialization | Leer y escribir el JSON de Tisma Cloud (`cloud.md`). |
| AndroidX Core | androidx.core:core-ktx | 1.17.0 | Apache-2.0 | POM publicado | https://developer.android.com/jetpack/androidx | Base de la app. |
| AndroidSVG | com.caverock:androidsvg-aar | 1.4 | Apache-2.0 | POM publicado (sin dependencias) | https://bigbadaboom.github.io/androidsvg/ | Pasa a imagen las texturas SVG de las camas del paquete de perfiles (`profile_pack.md`, regla 14). |
| AndroidX Activity Compose | androidx.activity:activity-compose | 1.11.0 | Apache-2.0 | POM publicado | https://developer.android.com/jetpack/androidx | La actividad con Compose, elegir y guardar archivos. |
| AndroidX Lifecycle ViewModel Compose | androidx.lifecycle:lifecycle-viewmodel-compose | 2.9.4 | Apache-2.0 | POM publicado | https://developer.android.com/jetpack/androidx | El estado de las pantallas sobrevive a girar el teléfono. |
| AndroidX Lifecycle Runtime Compose | androidx.lifecycle:lifecycle-runtime-compose | 2.9.4 | Apache-2.0 | POM publicado | https://developer.android.com/jetpack/androidx | Leer el estado según el ciclo de vida. |
| Jetpack Compose BOM | androidx.compose:compose-bom | 2025.10.01 | Apache-2.0 | POM publicado | https://developer.android.com/jetpack/compose | Fija las versiones de Compose. |
| Compose UI | androidx.compose.ui:ui | la del BOM | Apache-2.0 | POM publicado | https://developer.android.com/jetpack/compose | La interfaz. |
| Compose Material 3 | androidx.compose.material3:material3 | la del BOM | Apache-2.0 | POM publicado | https://developer.android.com/jetpack/compose | Componentes, con los colores de `interface.md`. Sin el paquete de íconos de Material: los íconos son los de `assets/brand/`. |
| Compose UI Test Manifest | androidx.compose.ui:ui-test-manifest | la del BOM | Apache-2.0 | POM publicado | https://developer.android.com/jetpack/compose | Solo en la versión de depuración, para las pruebas de pantallas. |

**El motor dentro del APK** (desde el 2026-11-04): el APK lleva el programa `tisma-engine` compilado con el NDK
(`lib/<abi>/libtisma_engine.so`), y con él las bibliotecas del motor de la tabla de arriba que enlaza `tisma_core`:
oneTBB, Clipper2, nlohmann/json y Boost.Polygon con Boost.Config, en las mismas versiones. Sin GPU (`TISMA_GPU=OFF`), sin
interfaz y sin OpenSSL. La biblioteca de C++ es la `libc++` del NDK, enlazada estática (Apache-2.0 con la excepción
LLVM). Los avisos van en Más › Créditos y fuentes de la app (`credits.md`, regla 4): los del motor salen de
`resources/credits.json` y `resources/licenses/`, los de la app de `apps/android/credits_android.json` y
`apps/android/licenses/` (el texto de la licencia de libc++, Apache-2.0 con las excepciones de LLVM, del archivo
`libcxx/LICENSE.TXT` de LLVM); una prueba de la app compara esos créditos con `gradle/libs.versions.toml`.

### Herramientas de prueba de la app (no entran al APK)

Corren solo en la máquina que compila (pruebas en la JVM con Robolectric); no se enlazan ni se distribuyen, como las
herramientas de compilación de arriba. Dos traen licencias fuera de la lista permitida y quedan **para que decida el
dueño**: JUnit 4 (EPL-1.0), que Robolectric y las pruebas de Compose necesitan como ejecutor, y
`javax.annotation:javax.annotation-api` 1.3.2 (CDDL-1.1 o GPL-2.0 con la excepción de classpath), que Robolectric pide
en su ejecución (probado: sin ella no arranca). Ninguna se copia ni se modifica.

| Herramienta | Gradle (solo pruebas) | Versión | Licencia | Uso |
|---|---|---|---|---|
| JUnit 4 | junit:junit | 4.13.2 | EPL-1.0 (trae hamcrest-core 1.3, BSD-3-Clause) | Ejecutor de las pruebas. |
| kotlin-test-junit | org.jetbrains.kotlin:kotlin-test-junit | 2.2.21 | Apache-2.0 | Afirmaciones de las pruebas. |
| kotlinx-coroutines-test | org.jetbrains.kotlinx:kotlinx-coroutines-test | 1.10.2 | Apache-2.0 | El hilo principal en las pruebas. |
| Robolectric | org.robolectric:robolectric | 4.16 | MIT (trae ASM, BSD-3-Clause; Bouncy Castle, MIT; ICU4J, Unicode-3.0; Guava, Apache-2.0; javax.annotation-api, CDDL-1.1 o GPL-2.0 con excepción) | Android en la JVM, sin emulador. Baja `android-all-instrumented` (Apache-2.0, AOSP) en la primera corrida. |
| AndroidX Test Core | androidx.test:core | 1.7.0 | Apache-2.0 | El contexto de la app en las pruebas. |
| Compose UI Test JUnit4 | androidx.compose.ui:ui-test-junit4 | la del BOM | Apache-2.0 | Pruebas de las pantallas. |

| Herramienta de compilación de la app | Versión | Licencia | Uso |
|---|---|---|---|
| Gradle y su envoltorio (`gradlew`, `gradle/wrapper/`) | 8.14.3 | Apache-2.0 | Compila la app. |
| Android Gradle Plugin | 8.13.2 | Apache-2.0 | Compila el APK. |
| Kotlin (compilador y complementos Compose y serialization) | 2.2.21 | Apache-2.0 | Compila el código de la app. |
| Android SDK (plataforma 36, build-tools 36.0.0) | — | Licencia del SDK de Android (aceptada en la máquina que compila) | Compila el APK; no se guarda en el repositorio. |
| Android NDK | r29 (29.0.14206865) | Licencia del SDK de Android; su `libc++` es Apache-2.0 con la excepción LLVM | Compila `tisma-engine` para el teléfono. La r28 no sirve: su `libc++` no tiene `std::from_chars` con decimales, que el motor usa. |
