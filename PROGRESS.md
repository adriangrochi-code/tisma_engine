# Avance

Cada bloque de trabajo agrega una entrada con:
- la fecha;
- qué hizo;
- qué midió;
- qué sigue.

## 2026-10-24 — inicio

Se creó el repositorio con la licencia, las reglas de sala limpia y el plan. Todavía no hay código.

**Sigue:** F0 (esqueleto, CI, lectura de STL, banco de pruebas).

## 2026-10-24 — F0: esqueleto, línea de comandos y lectura de STL

**Hecho:**
- Proyecto CMake en C++20: biblioteca `tisma_core` y programa `tisma-engine`.
- Línea de comandos: `tisma-engine slice modelo.stl --config ajustes.json -o salida.gcode` comprueba los
  argumentos, que exista el JSON y lee el STL; después responde «slice: not implemented yet» con código de salida 3.
  `tisma-engine info modelo.stl` imprime formato, triángulos, tamaño, volumen y tiempo de lectura.
  Códigos de salida: 0 bien, 1 línea de comandos mal, 2 archivo de entrada mal, 3 sin implementar.
- Lector de STL binario y ASCII (`src/io/stl.*`), desde la especificación pública. Detecta el formato por el
  contenido (un binario cuya cabecera empieza con `solid` sigue siendo binario). Acepta mayúsculas, CRLF, varios
  `solid`, `endsolid` ausente y bytes sobrantes al final del binario. Rechaza archivos truncados, coordenadas NaN o
  infinitas y facetas con más de 3 vértices; los errores ASCII dicen la línea.
- Malla de triángulos contigua (`src/mesh/mesh.*`): caja envolvente y volumen con signo.
- 24 pruebas Catch2 y 3 pruebas del programa con CTest.
- CI de GitHub Actions: Linux en cada envío (gcc, clang y gcc con ASan+UBSan); Windows 2022 y macOS 14 solo a mano
  (`workflow_dispatch`) y los lunes.
- Catch2 3.7.1 anotada en `THIRD_PARTY.md` (licencia BSL-1.0 leída en el tag).

**Probado (local, Linux, 4 núcleos):** 27/27 pruebas pasan con gcc 13.3 (Release), clang 18 (Release) y gcc con
ASan+UBSan (Debug). Compila sin advertencias con `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`.

**No probado:** Windows ni macOS (la CI de esos sistemas todavía no se lanzó). Los modelos del banco no están en el
repositorio.

**CI bloqueada:** en GitHub los tres trabajos de Linux terminan en «failure» a los 2 s sin ejecutar ningún paso (sin
runner asignado, sin registros), también al reintentarlos una vez. No es un error del código ni del YAML: el YAML se
leyó bien (los trabajos se crearon con sus nombres y el de Windows/macOS quedó omitido como corresponde). Lo más
probable es la facturación de Actions de la cuenta (límite de gasto o minutos del repositorio privado) o una política
que no permite `actions/checkout`. El aviso exacto aparece en la página del run, en Actions. Lo tiene que revisar el
dueño.

**Medido** (`tisma-engine info`, esfera generada de 1 001 112 triángulos, archivo en caché, 5 corridas, Release):

| Archivo | Tamaño | Lectura |
|---|---|---|
| Binario | 50 MB | 58–67 ms |
| ASCII | 220 MB | 713–845 ms |

**Sigue:**
- Que el dueño desbloquee GitHub Actions (ver «CI bloqueada»); después, confirmar que Linux pasa y lanzar la CI a mano
  para ver Windows y macOS.
- Banco de pruebas con los modelos reales (pedir al dueño los archivos y confirmar su licencia).
- Después, F1: corte de la malla en capas. Necesitará una malla indexada (vértices compartidos) para unir
  segmentos en contornos, y Clipper2 (anotarla en `THIRD_PARTY.md` antes de usarla).

## 2026-10-24 — repositorio público, licencia no comercial

**Hecho:**
- El dueño hizo público el repositorio para que la CI de GitHub pueda correr; en privado los trabajos no arrancaban.
- `LICENSE` pasa de «todos los derechos reservados, repositorio privado» a la PolyForm Noncommercial License 1.0.0,
  copiada sin cambios del repositorio oficial (`polyformproject/polyform-licenses`, commit `76a278c`), más una línea
  `Required Notice` con el copyright y la prohibición del uso comercial sin una licencia aparte.
- `README.md` nuevo: aclara que el código es de código disponible y no de código abierto, qué se permite y qué no.
  Incluye también los pasos para compilar.
- `CLEANROOM.md` anota la licencia adoptada y que la revisión de un abogado sigue pendiente.

**Ojo:** `main` sigue con la `LICENSE` anterior, que dice «repositorio privado», hasta que se mezcle el PR de `dev`.

**Sigue:** confirmar que la CI de Linux pasa ahora que el repositorio es público. Después, lo mismo que en la entrada
anterior.

## 2026-10-24 — CI: Linux y Windows en cada envío

**Hecho:**
- Por decisión del dueño, Windows (MSVC, windows-2022) corre ahora en cada envío junto con Linux. Al ser público el
  repositorio, esos minutos ya no cuestan.
- macOS sale de la CI y pasa a una fase F7 al final del plan.
- Se quitó la corrida semanal: ya no hace falta, porque todo corre en cada envío.
- Arreglado el grupo de concurrencia de `ci.yml`: era uno solo por rama, así que la corrida lanzada a mano canceló la
  del envío (run 3). Ahora el grupo incluye el tipo de evento.
- `CLAUDE.md` y `PLAN.md` actualizados con la regla nueva.

**Sigue:** confirmar que la CI pasa en Linux y Windows. Después, el banco de pruebas y F1.

## 2026-10-24 — visión: no planar, 5 ejes y GPU

**Hecho:**
- El dueño fijó la visión del motor: laminado no planar y de 5 ejes, con aceleración por GPU.
- `PLAN.md`:
  - nueva sección «Diseño para la visión», con las reglas que valen desde F1 (capas como conjuntos de nivel de un
    campo, trayectorias 3D con orientación, datos listos para la GPU con la CPU como referencia, GPU opcional) y las
    fuentes;
  - fases nuevas F7 (no planar en 3 ejes), F8 (GPU) y F9 (5 ejes); macOS pasa a F10, al final.
- Las referencias de los artículos se verificaron en DBLP, arXiv y las páginas de los autores; solo se leyeron los
  resúmenes y los datos bibliográficos.
- `CLEANROOM.md` prohíbe además la rama no planar de Slic3r de Ahlers et al. (GPLv3) y el código publicado con los
  artículos.
- `CLAUDE.md` resume la visión para los próximos bloques.

**Decidido por el dueño:**
- El orden es F7 → F8 → F9.
- La API de GPU se elige en F8, después de medir.

- La máquina de 5 ejes se elige al empezar F9, la última fase antes de macOS.

**CI:** la corrida lanzada a mano sobre `c8e0565` (run 4) pasó entera, incluidos Windows 2022 (MSVC) y macOS 14. Ese
fue el último paso por macOS antes de sacarlo de la CI.

**Sigue:** el banco de pruebas y F1, ya con la regla del campo escalar.

## 2026-10-24 — la GPU entra en la primera versión

**Hecho:**
- Decisión del dueño: la GPU va dentro de F0–F6, porque ahí es donde importa la optimización. Se quitó la fase aparte
  de GPU y se renumeró: F7 es el no planar en 3 ejes, F8 los 5 ejes y F9 macOS. Las entradas anteriores de este
  archivo usan la numeración vieja.
- `PLAN.md`:
  - nueva sección «GPU en la primera versión», con los núcleos candidatos de cada fase (qué va a GPU y qué queda en
    CPU);
  - F1, F3, F5 y F6 suman su parte de GPU;
  - aviso de que las 16 semanas se estimaron antes de sumar la GPU y se vuelven a estimar al terminar F1.
- API: Vulkan compute, salvo que el dueño diga otra cosa. Se puede probar en la CI sin GPU con lavapipe (Mesa);
  CUDA no se puede probar ahí.
- `CLAUDE.md` suma la GPU a las reglas de rendimiento.

**Sigue:** el banco de pruebas; después F1, con el corte en CPU y la infraestructura de Vulkan.

## 2026-10-24 — F1, primer bloque nocturno de 20 minutos

**Hecho:**
- Bloques nocturnos: el dueño pidió bloques autónomos de 20 minutos de 1 a 8 (Montevideo) cuando no haya otra sesión
  del motor trabajando. Una tarea programada por hora arranca la cadena, y cada bloque agenda el siguiente.
- Malla indexada (`src/mesh/indexed_mesh.*`): `weld_exact` une las esquinas con coordenadas idénticas bit a bit
  (normaliza -0 y 0), conservando el orden y el sentido de los triángulos.
- Contornos de nivel (`src/slice/level_set.*`): `level_set_segments` saca los tramos de contorno de un campo dado por
  vértice (regla 1 del diseño: las capas son conjuntos de nivel; el corte plano usa `height_field`, que es z).
  - Un vértice con valor >= nivel cuenta como «arriba»; así cada triángulo se cruza por 0 o 2 aristas, aunque haya
    vértices justo sobre el nivel.
  - Los puntos se calculan en el orden de los índices de la arista, así que los dos triángulos de una arista dan el
    mismo punto bit a bit.
  - Cada tramo sabe por qué arista entra y por cuál sale, para unirlos después sin comparar coordenadas.
  - Con el campo z, los contornos exteriores salen en sentido antihorario visto desde arriba.
- `tisma-engine info` también imprime los vértices y el tiempo de soldadura.
- `bench/gen_sphere.py`: polos y costura exactos. Antes la esfera no era cerrada bit a bit, porque sin(π) no es 0.

**Probado:** 34/34 pruebas con gcc 13.3, clang 18 y gcc con ASan+UBSan, sin advertencias.
- Malla indexada: cubo → 8 vértices, -0 y 0, malla vacía.
- Contornos: cuadrado cerrado y antihorario (área 100 mm²), niveles que pasan por vértices, sentido invertido y un
  campo no plano (x + z) que también da un contorno cerrado.

**Medido** (esfera de 1 001 112 triángulos, 5 corridas): la lectura tarda 61–66 ms y la soldadura 445–461 ms
(500 558 vértices; V − A + C = 2, cerrada). Ordenar las claves contiguas en lugar de índices bajó la soldadura de
559–626 ms a ese valor, un 25 % más rápido.

**Sin terminar (F1):**
- la soldadura es 7 veces más lenta que la lectura; probar un ordenamiento radix o una tabla hash, y paralelizar
  con oneTBB;
- unir los segmentos en contornos.

**Segunda parte del bloque:**
- `src/slice/contours.*`: `join_segments` une los tramos en contornos encadenándolos por la clave de arista. Las
  cadenas abiertas (mallas con huecos) salen enteras, desde su primer punto hasta el último. Los puntos alineados se
  conservan; simplificarlos es un paso posterior.
- Pruebas: el cubo da 1 contorno cerrado y antihorario; dos cubos separados dan 2 contornos; un cubo sin una cara da
  un contorno abierto que empieza y termina en el borde del hueco. 38/38 pruebas con gcc, clang y ASan+UBSan.
- `tisma-engine info` corta además una capa a media altura.

**Medido** (esfera de 1 001 112 triángulos, 5 corridas): una capa a media altura, con 1416 tramos y 1 contorno,
tarda 6,0–6,6 ms. Hoy cada capa recorre todos los triángulos: con 500 capas serían unos 3 s. Hace falta repartir los
triángulos por capas antes de cortar.

**Tercera parte del bloque:**
- `src/slice/layers.*`:
  - `plan_layers` arma las capas (primera capa y resto) desde la z mínima. Las alturas salen del índice de la capa,
    así el error de redondeo no crece con la cantidad de capas. No se crea una capa cuyo medio quede en el techo del
    modelo o por encima: la altura impresa se redondea a la capa más cercana.
  - `slice_levels` corta todos los niveles de un campo. Cada triángulo se reparte en los niveles que cruza su rango
    de valores, y cada nivel corta solo sus triángulos. Sirve igual para un campo curvo (regla 1 del diseño).
- Pruebas:
  - plan de capas, y alturas no válidas;
  - cubo: 50 capas de 100 mm²;
  - pirámide: el área de cada capa coincide con el área exacta de la sección (10 − z)², con error menor a 0,001 mm²;
  - el corte repartido da los mismos tramos que recorrer todos los triángulos.

  44/44 pruebas con gcc, clang y ASan+UBSan.
- `tisma-engine info` corta el modelo entero con capas de 0,2 mm.

**Medido** (esfera de 1 001 112 triángulos, 500 capas, un hilo, 5 corridas): el corte completo tarda 181–235 ms
(antes 6 ms por capa, unos 3 s en total).

**Sigue:** paralelo por capas con oneTBB (anotarla en `THIRD_PARTY.md` antes). Después: soldadura más rápida, cierre
de huecos, Clipper2 y la infraestructura de Vulkan.

## 2026-10-24 — oneTBB y licencia cerrable

**Hecho:**
- oneTBB 2022.3.0 (Apache-2.0; incluye ITT y hwloc, BSD-3-Clause), anotada en `THIRD_PARTY.md` después de leer su
  licencia y `third-party-programs.txt` en el tag. Se enlaza estática, así `tisma-engine` es un solo programa.
  - `slice_levels` corta los niveles en paralelo (`tbb::parallel_for`);
  - `weld_exact` ordena en paralelo (`tbb::parallel_sort`).
- CMake 3.25 como mínimo, para declarar las dependencias como `SYSTEM`: sus cabeceras no reciben nuestras banderas de
  advertencia.
- **Licencia cerrable** (decisión del dueño): el motor tiene que poder pasar a licencia cerrada en cualquier momento.
  - `CLEANROOM.md`: nueva sección con las reglas (no copiar código de ningún lado, solo bibliotecas permisivas, datos
    con licencia compatible, sin aportes de terceros sin cesión de derechos). Aclara también que cerrar la licencia
    vale para las versiones nuevas: lo ya publicado bajo PolyForm sigue con esa licencia para quien lo recibió.
  - `CONTRIBUTING.md`: por ahora no se aceptan aportes de terceros.
  - `tools/check_licenses.py`, que corre como prueba de CTest y por lo tanto en la CI, falla si una dependencia de
    CMake no está en `THIRD_PARTY.md` con licencia permitida, o si un archivo propio trae texto de licencia copyleft.
    Se probó que detecta los tres casos.
  - `THIRD_PARTY.md` suma una columna con el nombre en CMake y deja anotado revisar glslang antes de usarlo.
  - `CLAUDE.md` resume la regla para las próximas sesiones.

**Probado:**
- 45/45 pruebas (44 más la de licencias) con gcc 13.3, clang 18 y gcc con ASan+UBSan, sin advertencias.
- Una configuración desde cero, descargando oneTBB y Catch2 por git como hará la CI, compila y pasa las pruebas en
  52 s.

**Ojo:** oneTBB avisa que compilarla estática «no está soportado» por ellos. Funciona en Linux. Windows lo dirá la
CI. Si da problemas, la alternativa es enlazarla dinámica y distribuir su DLL junto al programa.

**Medido** (esfera de 1 001 112 triángulos, 4 núcleos, 5 corridas):

| Fase | Un hilo | oneTBB |
|---|---|---|
| Soldadura | 445–461 ms | 168–193 ms |
| Corte de 500 capas | 181–235 ms | 66–81 ms |

**Sigue:**
- soldadura más rápida (ordenamiento radix o tabla hash);
- cierre de huecos chicos;
- Clipper2 (anotarla antes);
- infraestructura de Vulkan (revisar la licencia de glslang).

## 2026-10-24 — bloque nocturno 2: soldadura con tabla hash

**Hecho:**
- `weld_exact` usa una tabla hash de direccionamiento abierto (como mucho medio llena) en lugar de ordenar las
  esquinas.
  - Los vértices se numeran por orden de primera aparición: el resultado no depende de la cantidad de hilos, y los
    vértices de triángulos vecinos quedan cerca en memoria.
  - Es serial y no necesita oneTBB.
- Prueba nueva: el orden de primera aparición.

**Probado:** 45/45 pruebas con gcc, clang y ASan+UBSan.

**Medido** (esfera de 1 001 112 triángulos, 5 corridas):

| Soldadura | Tiempo |
|---|---|
| Tabla hash (serial) | 86–99 ms |
| Ordenamiento paralelo (4 núcleos) | 168–193 ms |
| Ordenamiento serial | 445–461 ms |

Ahora la soldadura tarda menos que el corte de las 500 capas (76–87 ms).

**Sigue:** cierre de huecos chicos en mallas abiertas; después Clipper2.

**Cierre de huecos (mismo bloque):**
- `close_gaps` une el final de una cadena abierta con el comienzo de otra (o de sí misma) cuando están a menos de
  `max_gap`, empezando por los pares más cercanos. Si una cadena vuelve a su comienzo, queda cerrada. No toca los
  contornos cerrados y no repite un punto cuando el hueco mide cero.
- `slice_levels` acepta un `max_gap` y cierra los huecos en cada capa.
- Pruebas: dos mitades de un cuadrado; un hueco más grande que el límite; el cubo sin una cara (no se cierra con
  0,5 mm, sí con 10 mm, y da 100 mm²); prioridad del par más cercano; hueco de largo cero; una pirámide sin una cara
  cortada con `max_gap`. 51/51 pruebas con gcc, clang y ASan+UBSan.

**Sigue:** Clipper2 (leer su licencia en la versión exacta y anotarla antes de usarla).

## 2026-10-24 — paso de información entre equipos

**Pasó:** la sesión principal de Tisma subió a `main` `docs/spec/` (especificaciones funcionales), `imported/tisma/`
(código de Tisma Slicer, unas 37 000 líneas) y `docs/tisma/`. También cambió las reglas para permitir copiar ese
código al motor. Esta sesión no mezcló `main` ni leyó ese código. Motivos: los archivos están en el árbol de
PrusaSlicer y usan sus tipos, los escribió una sesión que leyó PrusaSlicer, y eso choca con la regla de licencia
cerrable.

**Hecho:**
- El dueño pidió coordinar con la otra sesión para que la información pase sin código y con nombres propios.
- `CLEANROOM.md` y `CLAUDE.md`: protocolo «Paso de información entre los dos equipos».
- `tools/check_spec.py`: filtro de especificaciones. Informa solo archivo, línea y regla, sin mostrar el texto. Corre
  como prueba de CTest (`spec_check`) cuando existe `docs/spec/`.
  - Probado con un archivo de ejemplo: detecta nombre interno, bloque de código, instrucción de control, nombre de
    archivo fuente y asignación.
  - Resultado sobre `main`, sin leer los archivos: `docs/spec/` da 6 de 6 archivos limpios; `docs/tisma/` da 4 de 19
    limpios, con 102 problemas.
- Se le propuso el protocolo a la sesión principal y se le pidió sacar `imported/tisma/` y `docs/tisma/` de `main`.

**Pendiente:**
- la respuesta de la sesión principal;
- la decisión del dueño sobre borrar o no la historia de esos archivos (con un abogado);
- mezclar `main` en `dev` cuando `main` ya no tenga ese código.

## 2026-10-24 — información para el motor

El dueño agregó a `main`:
- `docs/spec/`: especificaciones funcionales (G-code, ajustes, comportamiento, números de referencia) y fuentes públicas;
- `imported/tisma/` y `docs/tisma/`: código y documentos propios traídos de Tisma Slicer. **Se sacaron el mismo día:**
  el dueño decidió que todo pase por fichas (`CLEANROOM.md`, «Fichas»). Su historia no se lee.

**Sigue:** el bloque siguiente une `main` en `dev` y continúa F0, usando las especificaciones.

## 2026-10-24 — `main` mezclado en `dev`

**Hecho:**
- La sesión principal confirmó el protocolo de paso de información. Sacó `imported/tisma/` y `docs/tisma/` de `main`
  (`db6db5a`) y alineó la sección «Fichas» (`66d6d1b`).
- Antes de mezclar se comprobó que el árbol de `main` no tiene archivos en `imported/` ni en `docs/tisma/`, y que
  `docs/spec/` pasa el filtro (7 de 7 archivos limpios). La historia de esos directorios no se leyó.
- Conflictos resueltos:
  - `CLEANROOM.md`: se juntan las dos listas de lo prohibido, se suma la sección «Fichas» de `main` y se conservan
    el protocolo y la licencia cerrable.
  - `PLAN.md`: se juntan las fases. Quedan F7 (no planar en 3 ejes) y F8 (lo propio de Tisma desde sus fichas);
    después las fases de producto P1–P5 de `main`, en el orden que decida el dueño; al final F9 (5 ejes) y F10
    (macOS), como decidió el dueño.
  - `PROVENANCE.md` y `PROGRESS.md`: se conservan las dos partes.
- Aviso en `PLAN.md`: STEP con OCCT no sirve tal cual, porque OCCT es LGPL-2.1 con excepción y no está en la lista
  de licencias permitidas.
- `docs/spec/` ya está en el árbol, así que la prueba `spec_check` corre en la CI.

**Sigue:** Clipper2; leer `docs/spec/README.md` y las fichas que pide F1.

## 2026-10-24 — bloque nocturno 3: Clipper2 y regiones por capa

**Hecho:**
- Clipper2 1.5.4 (BSL-1.0), anotada en `THIRD_PARTY.md` después de leer `LICENSE` en el tag. Solo la biblioteca de
  C++, sin Z, utilidades, ejemplos ni pruebas.
- `src/geom/polygons.*`:
  - polígonos enteros en nanómetros;
  - `to_polygons` proyecta los contornos cerrados en XY;
  - `layer_region` une con la regla no nula. Los agujeros horarios se restan, los volúmenes superpuestos suman, y
    una malla con el sentido invertido igual da su región;
  - `offset` agranda o achica con esquinas en inglete, como la compensación XY;
  - `layer_regions` procesa todas las capas en paralelo;
  - `area_mm2`.
- `tisma-engine info` arma también las regiones y el volumen cortado (suma de las áreas por el alto de capa).
- Se le respondió a la sesión principal qué fichas necesita F1, en qué orden y cómo usar `tools/check_spec.py`.

**Probado:**
- 57/57 pruebas con gcc, clang y ASan+UBSan. Incluyen:
  - unión con superposición (175 mm²), agujero (84 mm²), sentido invertido y cáscaras anidadas;
  - offsets que agrandan y achican, incluso hasta desaparecer;
  - capas del cubo como regiones de 100 mm², y con −0,1 mm, cuadrados de 9,8 mm.
- Una configuración desde cero descarga Clipper2 por git y pasa.

**Medido** (esfera de 1 001 112 triángulos, 500 capas, 4 núcleos, 5 corridas):
- regiones de todas las capas: 19–21 ms;
- volumen cortado 523 590 mm³ contra 523 589 mm³ de la malla (0,0002 %).

**Sigue:**
- compensación XY y de primera capa, cuando lleguen las fichas de corte;
- mientras tanto, infraestructura de Vulkan (revisar antes la licencia de glslang).

**Licencias para Vulkan (mismo bloque):**
- Revisadas en `vulkan-sdk-1.4.350.1`:
  - Vulkan-Headers: Apache-2.0, algunos archivos MIT;
  - volk: MIT;
  - glslang: mezcla con una licencia propia de NVIDIA en el preprocesador y el texto GPL-3 con excepción de Bison.
- Decisión: glslang no se enlaza ni se distribuye. Se usa solo para compilar los shaders a SPIR-V. Queda anotado en
  `THIRD_PARTY.md` y `PLAN.md`.
- La CI del merge (`b0bf6ac`) pasó en Linux y en Windows: oneTBB estática funciona también con MSVC.

**Sigue:** infraestructura de Vulkan:
- volk y Vulkan-Headers con FetchContent;
- shaders de cómputo en GLSL compilados a SPIR-V en la compilación;
- lavapipe en la CI de Linux;
- primer núcleo: el corte de triángulos en GPU, comparado con la CPU.

## 2026-10-24 — bloque nocturno 4: infraestructura de Vulkan

**Hecho:**
- Vulkan-Headers (Apache-2.0 y MIT) y volk (MIT), anotadas en `THIRD_PARTY.md` antes de usarlas. glslang y lavapipe
  quedan anotados como herramientas: no se enlazan ni se distribuyen. El control de licencias frenó la primera
  compilación porque faltaban en la tabla, como corresponde.
- `src/gpu/compute.hpp` y `compute_vulkan.cpp`:
  - `gpu::Context::create()` devuelve nulo si no hay cargador de Vulkan, si no hay un dispositivo adecuado, o si
    `TISMA_GPU=0`;
  - los dispositivos por software (lavapipe) se usan solo si se piden, como en las pruebas, porque son más lentos
    que la ruta propia en CPU;
  - búferes visibles desde la CPU y un despachador de shaders de cómputo.
- Opción de CMake `TISMA_GPU` (activada por defecto). Con `OFF` se compila `compute_none.cpp` y no hay dispositivo.
- Primer núcleo: `level_ranges.comp`, el rango de capas que cruza cada triángulo. La versión de CPU
  (`triangle_level_ranges`) se separó de `slice_levels` y es la referencia.
- `tools/compile_shaders.py` genera los SPIR-V en `src/gpu/shaders/generated/` (guardados en el repositorio).
  `--check` falla si están desactualizados; lo corre la CI de Linux.
- CI de Linux: instala lavapipe y define `TISMA_REQUIRE_GPU=1`, así las pruebas de GPU fallan en lugar de omitirse si
  falta el dispositivo. En Windows se omiten si no hay Vulkan (`SKIP_RETURN_CODE 4`).

**Probado:**
- 60/60 pruebas con gcc, clang y ASan+UBSan.
- Las 3 pruebas de GPU corren en lavapipe: esfera de 200 segmentos con 334 capas, niveles que pasan por vértices,
  casos vacíos. Dan lo mismo que la CPU, elemento por elemento.
- Con `TISMA_GPU=OFF` compila, y las pruebas de GPU quedan omitidas.

**No medido:** la velocidad en GPU. Acá solo hay lavapipe, que corre en la CPU, y medirlo no dice nada. Hace falta
una máquina con GPU real (Windows con NVIDIA, AMD o Intel).

**Sigue:** el corte en GPU completo: tramos por (triángulo, capa), con una suma de prefijos para ubicar la salida.
Después, la compensación XY y de primera capa con la ficha de corte, cuando esté aprobada.

## 2026-10-24 — bloque nocturno 5: corte en GPU

**Hecho:**
- La CI de `0e1ace5` pasó entera: Windows compila el backend de Vulkan, Linux corre las pruebas de GPU en lavapipe y
  la verificación de los SPIR-V da bien (la misma versión de glslang que acá).
- El punto de cruce se calcula ahora en `float`, en tres pasos redondeados (resta, producto, suma), con
  `-ffp-contract=off` para que ningún compilador los fusione. Así la CPU y la GPU hacen las mismas operaciones.
  Vulkan redondea exacto la suma, la resta y el producto, pero la división puede errar hasta 2,5 ULP. Por eso en una
  GPU real los puntos pueden diferir en unos pocos ULP, mientras la topología (aristas, triángulos, orden) es
  idéntica.
- `level_segments_by_triangle` (CPU, referencia) y su núcleo `level_segments.comp`: todos los tramos de cada
  triángulo, ubicados con una suma de prefijos.
- `slice_levels(…, gpu)`: calcula en GPU los rangos y los tramos, los reagrupa por capa en el mismo orden que la
  CPU (ordenamiento por conteo) y une los contornos en paralelo.

**Probado:**
- 62/62 pruebas con gcc, clang y ASan+UBSan.
- Pruebas nuevas de GPU:
  - tramos de la esfera de 200 segmentos con 334 capas: mismas aristas, triángulos y orden; diferencia máxima de
    coordenadas 0 mm en lavapipe (la prueba admite 0,0001 mm);
  - corte completo de una esfera en GPU contra CPU: mismos contornos y puntos.

**No medido:** la velocidad en GPU (falta una máquina con GPU real).

**Sigue:**
- usar la GPU desde `tisma-engine` cuando haya una (opción y medición);
- compensación XY y de primera capa, cuando la ficha de corte esté en `main`.

## 2026-10-24 — bloque nocturno 6: ajustes propios y `slice` con el corte completo

**Hecho:**
- La CI de `f31a014` (corte en GPU) estaba en curso al empezar el bloque; la anterior (`0e1ace5`) había pasado.
- **Nombres de los ajustes:** varias claves de `docs/spec/SETTINGS_V1.md` coinciden con claves de configuración de
  PrusaSlicer (por ejemplo `seam_position`, `slowdown_below_layer_time`, `extrusion_multiplier`,
  `retract_before_travel`). El dueño pidió nombres propios, así que el motor define su propio formato en
  `docs/CONFIG.md`: grupos (`layers`, `slicing`, `compute`; los demás se definen al implementarlos) y nombres elegidos
  acá. Se le pidió a la sesión principal que las fichas usen esos nombres o describan los ajustes por su significado.
  La traducción desde Tisma Slicer va del lado de Tisma.
- nlohmann/json 3.12.0 (MIT), anotada en `THIRD_PARTY.md`. Su repositorio trae un archivo GPL-3.0 solo entre sus
  pruebas, que no se compilan ni se distribuyen.
- `src/config/settings.*`: lectura del formato. Las claves desconocidas son error, con el nombre de la clave. Los
  grupos previstos todavía no implementados se aceptan con un aviso.
- Modos de relleno: `union` (regla no nula), `even_odd` y `fill_holes`.
- `src/slice/pipeline.*` (`slice_model`): plan de capas, contornos con cierre de huecos, regiones según el modo de
  relleno, compensación XY y de primera capa, y GPU si hay una.
- `tisma-engine slice`:
  - lee los ajustes y corta el modelo completo;
  - usa la GPU si hay una y `compute.gpu` no es `off`, y vuelve a la CPU si la GPU falla;
  - avisa de los contornos abiertos que se descartan;
  - el G-code sigue sin implementar (código de salida 3).

**Probado:**
- 69/69 pruebas con gcc, clang y ASan+UBSan.
- Pruebas nuevas:
  - ajustes por defecto y por grupo;
  - errores con el nombre de la clave;
  - los tres modos de relleno con un anillo con isla y cuadrados anidados;
  - XY −0,1 mm y primera capa −0,2 mm en el cubo (9,4 mm y 9,8 mm de lado);
  - `slice` de punta a punta y con ajustes inválidos.

**Medido** (esfera de 1 001 112 triángulos, 500 capas, 4 núcleos, sin GPU, 5 corridas): `tisma-engine slice` tarda
0,28–0,36 s en total, de los cuales la lectura son 58–68 ms y la soldadura más el corte 205–275 ms.

**Pendiente:**
- que la ficha de corte llegue a `main`, para contrastar con sus 19 casos;
- que `SETTINGS_V1.md` use los nombres de `docs/CONFIG.md`;
- después, sumar al filtro (`tools/check_spec.py`) una lista de claves típicas de esos programas.

**Sigue:** F2, los perímetros (Kuipers et al. 2020), cuando la ficha de perímetros esté aprobada. Mientras tanto se
puede empezar el esqueleto de Voronoi con Boost.Polygon (anotarla antes).

## 2026-10-24 — bloque nocturno 7: comienzo de F2 (eje medio)

**Hecho:**
- Boost.Polygon y Boost.Config (`boost-1.90.0`, BSL-1.0, solo cabeceras), anotadas en `THIRD_PARTY.md`. El Voronoi
  de Boost.Polygon solo necesita, fuera de sí misma, `boost/config.hpp` y `boost/cstdint.hpp`, así que no hace falta
  el resto de Boost. Ninguno de los dos repositorios trae archivo de licencia en la raíz, pero todas las cabeceras
  llevan el aviso BSL-1.0. Su CMake espera todo el árbol de Boost, así que se usan solo las cabeceras.
- `src/geom/medial_axis.*`: `medial_axis(region)` construye el Voronoi de los segmentos del borde y devuelve las
  aristas interiores, primarias y finitas, una vez cada una. Cada extremo trae su radio (la distancia al borde), que
  según Kuipers et al. 2020 decide cuántos cordones entran. Las aristas curvas se marcan; todavía no se discretizan.

**Probado:**
- 73/73 pruebas con gcc, clang y ASan+UBSan:
  - cuadrado: 4 aristas desde el centro (radio 5) a las esquinas (radio 0);
  - rectángulo de 20 × 10: línea central con radio 5;
  - anillo cuadrado: el eje se curva alrededor de las esquinas del agujero y llega a 3√2/(1+√2) ≈ 1,757 mm en la
    diagonal (valor exacto); nada adentro del agujero;
  - región vacía.
- La primera versión de la prueba del anillo esperaba un cuadrado con esquinas a 1,5 mm: estaba mal la prueba, no el
  código.
- La CI de `f68f030` (ajustes y `slice`) pasó.

**Sin terminar (F2):**
- la clasificación de aristas interiores recorre todos los contornos por vértice; es O(n) por vértice y hay que
  mejorarla antes de medir con piezas grandes;
- falta discretizar las parábolas.

**Sigue:** leer el método de Kuipers et al. 2020 (arXiv 2004.13497): cantidad y ancho de los cordones según el radio
del eje medio, y trapezoidación. Después, la ficha de perímetros cuando esté en `main`.

## 2026-10-24 — bloque nocturno 8: esquemas de reparto de cordones

**Hecho:**
- Se leyó el método de Kuipers et al. 2020 (arXiv 2004.13497), solo el artículo. Partes que se van a usar:
  - 3.2 y 3.3: trapezoidación del esqueleto; marcado del centro por la significancia de la ecuación 3 con
    αmax = 135°; filtro del marcado;
  - 3.4: cuantización (ecuación 4) y transiciones de n a n + 1 cordones (ecuaciones 5 a 7), con largo de transición
    w* y filtro de anclas a menos de 1 mm;
  - 3.5: propagación del reparto desde el centro hacia afuera, e interpolación entre dos repartos;
  - 3.6: extracción de los sitios por arista y unión en polilíneas;
  - sección 4 y tabla 1: esquemas de reparto.
  Las aristas parabólicas se discretizan en tramos de hasta 0,2 mm (sección 3.2).
- `src/perimeters/beading.*`:
  - `bead_count` (la cuantización q);
  - `transition_thickness` (su inversa, donde se anclan las transiciones);
  - `compute_beading` (anchos y centros de los cordones);
  - esquemas uniforme, uniformemente distribuido y distribuido hacia adentro.

**Probado:**
- 78/78 pruebas con gcc, clang y ASan+UBSan.
- Pruebas nuevas con valores exactos por esquema:
  - cantidades y saltos de la cuantización;
  - en un barrido de espesores entre 0,2 y 6 mm: los anchos suman exactamente el espesor, son simétricos, y en el
    esquema distribuido quedan entre w*/2 y 3w*/2.
- La CI de `445a441` (eje medio, primera descarga de Boost por git) seguía en curso al empezar el bloque.

**Sigue:** marcado del centro y transiciones (secciones 3.3 y 3.4); antes, discretizar las aristas parabólicas.

## 2026-10-24 — bloque nocturno 9: eje medio completo y marcado del centro

**Hecho:**
- La CI de `445a441` (Boost por git, también en Windows) pasó.
- Aristas parabólicas discretizadas en tramos de hasta 0,2 mm (Kuipers et al. 2020, sección 3.2). Los puntos se
  calculan sobre la parábola, equidistantes de la esquina y de la recta del segmento.
- La prueba de «adentro» ya no recorre los contornos por vértice. Usa el sitio de cada celda del Voronoi: la celda de
  un segmento está adentro de su lado izquierdo, y la de una esquina solo si la esquina es reflexa.
- `MedialGraph`: el eje medio como grafo con nodos compartidos.
- `mark_central` (sección 3.3): una arista es central si su ángulo bisector α = 2·arccos(|ΔR|/|Δv|) supera αmax
  (135° por defecto); un nodo es central si su radio es un máximo local. La desigualdad de la ecuación 3 se escribió
  a partir de esa definición, porque en el texto extraído del PDF el signo no se lee con claridad. Queda anotado en
  `PROVENANCE.md`.
- `tisma-engine info` mide el eje medio de la capa del medio.

**Probado:**
- 82/82 pruebas con gcc, clang y ASan+UBSan. Pruebas nuevas:
  - tramos parabólicos de hasta 0,2 mm, cada punto equidistante de la esquina del agujero y de la pared;
  - en una tira, solo la línea del medio es central (las bisectrices de esquinas de 90° tienen α = 90°);
  - el centro de un cuadrado es un máximo local;
  - una cuña de 30° (α = 150°) es central con αmax = 135° y deja de serlo con 160°.
- Dos fallas durante el bloque eran errores de la prueba: una comparaba cada punto con el radio del otro extremo.

**Medido** (capa del medio de la esfera, círculo de 708 segmentos, 2788 aristas, 3 corridas): eje medio y marcado
en 39–40 ms. Con 500 capas serían unos 20 s en un hilo: hay que ver dónde se va el tiempo (construcción de Boost,
discretización o el `std::map` del grafo) antes de seguir con F2.

**Sigue:**
- perfilar y acelerar el eje medio;
- después, el filtro del marcado y las transiciones (sección 3.4).

## 2026-10-24 — bloque nocturno 10: por qué el eje medio era lento, y simplificación

**Medido y encontrado** (con un programa de perfilado aparte, que no va al repositorio, y callgrind):
- El eje medio de un círculo de 708 segmentos tarda unos 17 ms. Casi todo es la construcción del Voronoi de Boost:
  el 55 % de las instrucciones está en su aritmética entera exacta (`extended_int::mul`). Armar el grafo y marcar el
  centro no llega a 1 ms.
- No es por degeneración: una elipse con los mismos segmentos tarda lo mismo (15 ms), y una grilla más gruesa no
  cambia nada. Pasar las coordenadas a micrómetros solo lo baja de 16 a 11 ms.
- La palanca es la cantidad de segmentos. En la esfera de prueba cada segmento se aparta apenas 0,0005 mm del círculo
  verdadero.

| Tolerancia | Segmentos | Eje medio | Área |
|---|---|---|---|
| 0 | 708 | 19,0 ms | 7853,879 mm² |
| 0,005 mm | 354 | 9,2 ms | 7853,569 mm² |
| 0,01 mm | 208 | 5,7 ms | 7852,549 mm² |
| 0,02 mm | 177 | 4,6 ms | 7852,332 mm² |

**Hecho:**
- Ajuste `slicing.simplify_tolerance`, por defecto 0,01 mm (`docs/CONFIG.md`). Es un valor de este equipo y se
  revisa cuando llegue la ficha de corte, que trae su propio caso.
- `simplify()` (Clipper2 `SimplifyPaths`) se aplica a los contornos antes de unirlos, así la unión limpia cualquier
  cruce que deje la simplificación.
- `tisma-engine info` mide el eje medio sobre la región como la arma el corte.

**Probado:** 83/83 pruebas con gcc, clang y ASan+UBSan. Prueba nueva: la simplificación de un círculo deja menos de
300 vértices y conserva el área al 0,1 %; un cuadrado queda igual.

**Medido** (esfera de 1 001 112 triángulos):
- eje medio de la capa del medio: de 39–40 ms a 5,9–6,1 ms, con 398 aristas en lugar de 2788;
- volumen cortado: 523 500 mm³ contra 523 590 sin simplificar (−0,017 %);
- `slice` completo: 230 ms.

**Sigue:** el filtro del marcado y las transiciones (Kuipers et al. 2020, sección 3.4); después, el eje medio de
todas las capas en paralelo.

## 2026-10-24 — bloque nocturno 11: anclas de transición

**Hecho:**
- La CI de `eddb38e` (parábolas y marcado) pasó; la de `c3d7087` (simplificación) estaba en curso.
- `src/perimeters/transitions.*` (Kuipers et al. 2020, sección 3.4):
  - `central_bead_counts`: q(2R) en cada nodo central (ecuación 4);
  - `transition_anchors`: en cada arista central cuyos extremos tienen distinta cantidad, el punto donde el
    espesor 2R cruza la transición q⁻¹(n) (ecuaciones 5 y 6), con R interpolado a lo largo de la arista. Pasa por
    cada cantidad intermedia; en el esquema uniforme salta de a dos.
- Se releyó la ubicación de la rampa: mide t(n) = w* y empieza a t(n)·(q⁻¹(n)/w* − n) antes del ancla; en el esquema
  distribuido queda centrada en el ancla.
- La sesión principal sumó a la ficha de perímetros el caso P11: una cuña de 0 a 3 mm en 30 mm, con cantidad y
  anchos medidos por milímetro. Sirve para contrastar las transiciones cuando la ficha esté en `main`.

**Probado:**
- 87/87 pruebas con gcc, clang y ASan+UBSan. En una cuña de 30°:
  - las cantidades siguen al espesor;
  - cada ancla cae exactamente donde 2R = (n + ½)·w*, sobre el eje y en orden;
  - el esquema uniforme salta de a dos;
  - una tira de ancho constante no tiene transiciones.

**Sigue:** filtro de anclas opuestas a menos de 1 mm, rampas de largo w* con cantidad fraccionaria (ecuación 7), y
filtro del marcado.

## 2026-10-24 — bloque nocturno 12: filtro de anclas y rampas de transición

**Hecho:**
- `apply_transitions` (Kuipers et al. 2020, sección 3.4), sobre el grafo marcado:
  - recorre el centro por cadenas de aristas centrales, entre nodos donde se ramifica o termina;
  - ubica las anclas a lo largo de cada cadena;
  - filtra pares consecutivos de anclas en sentidos opuestos a menos de 1 mm, y deja la cantidad de alrededor;
  - arma rampas de largo t(n) = w* que empiezan a t(n)·(q⁻¹(n)/w* − n) del ancla del lado fino, y descarta las que
    no entran en la cadena;
  - parte las aristas en los extremos de cada rampa y da a los nodos de adentro n + distancia/t(n) (ecuación 7).
- El resultado da, por nodo, la cantidad (entera en el centro, fraccionaria en las rampas, −1 fuera del centro) y
  cuántas anclas se encontraron, filtraron o descartaron.

**Probado:**
- 89/89 pruebas con gcc, clang y ASan+UBSan:
  - cuña de 30°: cada rampa tiene nodos a ±w*/2 del ancla con las cantidades enteras, las anclas que no se usaron
    son justo las descartadas, y todos los nodos centrales tienen cantidad;
  - tira de 0,98 mm con un abultamiento de 1,02 mm: las anclas 2→3→2 se cancelan y todo queda en 2 cordones.

**Sigue:**
- filtro del marcado (sección 3.3);
- propagación del reparto desde el centro hacia afuera (sección 3.5);
- extracción de las trayectorias (sección 3.6).

## 2026-10-24 — bloque nocturno 13: filtro del marcado

**Hecho:**
- Lo que pedía este bloque (filtro de anclas y rampas) ya había quedado en el bloque 12.
- `filter_marking` (Kuipers et al. 2020, sección 3.3): desde cada nodo central se recorren hacia arriba (radio
  creciente) las aristas no centrales; si se llega a otro nodo central dentro de la distancia (w* en el artículo),
  el tramo se marca como central. Así no quedan oscilaciones del tamaño de la boquilla.

**Probado:** 91/91 pruebas con gcc, clang y ASan+UBSan. Pruebas con grafos armados a mano:
- un hueco de 0,3 mm se cierra con d = 0,4 mm y uno de 0,6 mm no;
- solo se camina hacia arriba.

**Además:** `half_beading` (el reparto de un lado, desde el borde hasta el medio) e `interpolate_beading` (sección
3.5): para una cantidad fraccionaria interpola cordón por cordón entre los repartos de n y n + 1. El cordón del medio
que solo tiene el reparto mayor aparece escalando su ancho por la fracción. Esa última regla es propia, porque el
artículo solo la muestra en una figura. Dos pruebas más (93/93): extremos de la rampa y el caso n + 2/3 de la
figura 8.

**Sigue:**
- trapezoidación: aristas de soporte de cada nodo a su punto más cercano del borde, y dominios por contorno;
- después, propagación del reparto (sección 3.5) y extracción de trayectorias (sección 3.6).

## 2026-10-24 — bloque nocturno 14: apoyos de la trapezoidación

**Hecho:**
- La CI de `32bc2ee` estaba en curso al empezar el bloque. La de `cdceabe` se canceló porque el gancho de cierre
  pidió subir los commits pendientes; la de `32bc2ee` prueba todo, incluido ese commit.
- Apoyos (Kuipers et al. 2020, ecuación 1): cada arista del eje medio guarda, para cada extremo y para cada uno de
  los dos lados (las dos celdas del Voronoi que separa), el punto más cercano del borde: el pie de la perpendicular
  sobre el segmento, o la esquina. Cada arista con los apoyos de un lado es un trapecio (o un triángulo) de la
  trapezoidación del esqueleto (sección 3.2), y ahí se van a ubicar los cordones (sección 3.6).
- `apply_transitions` interpola los apoyos de los nodos nuevos al partir aristas.

**Probado:** 95/95 pruebas con gcc, clang y ASan+UBSan:
- en la tira, la línea del medio tiene apoyos justo enfrente, uno en cada lado;
- en la tira, la cuña y el anillo, cada extremo queda a su radio de sus dos apoyos y los apoyos caen sobre el borde;
- en el anillo, las piezas parabólicas se apoyan en una esquina del agujero;
- los nodos nuevos de las rampas conservan sus apoyos a su radio.

**Sigue:**
- propagación del reparto (sección 3.5);
- sitios sobre los lados de los trapecios (sección 3.6: el cordón i va donde el radio vale l_i);
- unión por contorno.

## 2026-10-24 — bloque nocturno 15: reparto por nodo y sitios

**Hecho:**
- `node_beadings` (Kuipers et al. 2020, sección 3.5): los nodos centrales toman el reparto de su cantidad (entera o
  fraccionaria, interpolada) y su espesor 2R. Los demás suben hacia el vecino de mayor radio hasta un nodo central y
  usan su reparto; si llegan a un pico sin centro, usan el espesor de ese pico.
- `support_sites` (sección 3.6): en cada lado de trapecio que va de un nodo a su apoyo, el cordón i se ubica en la
  fracción lᵢ/R, porque la distancia al borde crece en línea recta de 0 a R. Cada sitio guarda su ancho, su índice
  y a qué trapecio pertenece.

**Probado:**
- 98/98 pruebas con gcc, clang y ASan+UBSan:
  - tira de 2 mm: 5 cordones de 0,4 mm, exactamente 12 sitios en la arista del medio (2 extremos, 2 lados,
    3 cordones), en las filas 0,2 / 0,6 / 1,0 / 1,4 / 1,8;
  - todos los nodos de la tira, esquinas incluidas, usan ese reparto, y ningún sitio sale de la región;
  - cuña: sitios adentro, anchos hasta 3/2·w*, varios cordones por lado.
- La primera versión de la prueba de la tira esperaba sitios a lo largo de la arista; los sitios van en sus
  extremos, y el segmento entre ellos es el paso siguiente. El error era de la prueba.

**Sigue:**
- sitios sobre las aristas no centrales del eje (donde el radio cruza lᵢ);
- unir los sitios del mismo índice en segmentos por trapecio;
- encadenarlos en polilíneas de ancho variable.

## 2026-10-24 — bloque nocturno 16: trayectorias de ancho variable

**Hecho:**
- `medial_sites` (Kuipers et al. 2020, sección 3.6): en cada arista no central, el cordón i del reparto del nodo de
  arriba cruza donde R = lᵢ, para R_bajo < lᵢ < R_alto. Un cordón justo en R es un sitio del nodo (`on_node`).
- `extract_toolpaths`: une los sitios del mismo índice dentro de cada trapecio en segmentos y los encadena en
  polilíneas y lazos con ancho por punto (`Toolpath`, `ToolpathPoint`). El cordón del medio, que corre sobre la
  arista central, sale solo del lado 0.
- Encadenado por identidad: cada sitio se nombra por lo que es (nodo y punto de apoyo; nodo; arista del eje; más el
  índice). Dos trapecios vecinos producen la misma clave, así que no se comparan posiciones ni hay tolerancias. Un
  sitio con más de dos segmentos corta las polilíneas.
- Tres errores encontrados con la capa del medio de la esfera (7458 fragmentos en vez de 125 lazos):
  - la prueba de «interior» usaba una tolerancia absoluta sobre el producto cruz; el vértice de una esquina convexa,
    calculado en coma flotante, quedaba afuera y se perdían radios (8 de 64 en un 64-gono). Ahora la tolerancia es
    una distancia: 1 nm;
  - el Voronoi de un polígono casi regular se junta en el centro en un racimo de vértices a 1e-5 mm. Fusionarlos por
    cercanía dejaba aristas duplicadas y rompía las cadenas; se descartó la fusión y los nodos se identifican por
    posición exacta;
  - en una esquina reflexa, el pie de la perpendicular sobre el segmento cae en la esquina con error de coma
    flotante; ahora se ajusta al extremo exacto para que los dos trapecios nombren el mismo apoyo.
- `tisma-engine info` mide las trayectorias de la capa del medio.

**Probado:** 103/103 pruebas con gcc, clang y ASan+UBSan:
- tira de 30 × 2 mm: 2 lazos con el área exacta de su desplazamiento (0,2 y 0,6 mm) y la línea del medio en y = 1;
- 64-gono de radio 5: 12 lazos cerrados, cada uno con el área del 64-gono de su radio interior (±0,1 %), ancho
  2·r/25;
- L de brazos de 2 mm: 2 lazos y una línea; cada punto queda a (i + ½)·ancho del borde. En la unión el grosor sube
  a 2,34 mm, la transición a 6 cordones se filtra por corta y los 5 cordones se ensanchan hasta 0,4686 mm;
- cuña: anchos positivos y puntos adentro;
- cada esquina del 64-gono tiene su radio y el grafo es un árbol (la prueba falla sin el arreglo).

**Medido** (esfera de 1 001 112 triángulos, capa del medio, 4 núcleos):
- eje medio: 416 aristas, 5,5–5,7 ms;
- trayectorias: 125 lazos cerrados (250 cordones), 58–65 ms. Antes: 7458 fragmentos en 117–135 ms.

**Sigue:**
- acelerar la extracción: hoy usa un `std::map` por trapecio y ordena las claves; con 500 capas serían ~30 s en un
  núcleo;
- perímetros de todas las capas en paralelo dentro de `slice_model`;
- resolución de conflictos de reparto (figura 9) y el punto del cordón central impar en un máximo aislado (hoy no se
  emite: en el 64-gono falta un punto de 0,4 mm en el centro).

## 2026-10-24 — bloque nocturno 17: extracción más rápida

**Hecho:**
- `extract_toolpaths` sin mapas ni ordenamientos: cada sitio tiene un casillero en un arreglo contiguo, calculado
  directamente (radio × cordón, nodo, arista × cordón). Cada trapecio recorre sus índices de cordón y arma los
  segmentos sin agrupar sitios; los casilleros con exactamente dos extremos de segmento los enlazan.
- Los radios se identifican igual que antes (nodo y punto de apoyo exacto), ordenando solo los 4 extremos por arista.
- `support_sites`, `medial_sites` y la extracción usan las mismas funciones para ubicar los sitios.
- Leí la sección 5.1 del artículo (meta-esquema de cáscara, ecuación 8): queda como próximo ítem del PLAN, antes de
  llevar los perímetros a todas las capas, porque hoy se llena toda la región de cordones (125 lazos en la esfera).

**Probado:** 104/104 pruebas con gcc, clang y ASan+UBSan. Prueba nueva: en la tira, el 64-gono y la cuña, cada punto
de cada trayectoria es exactamente uno de los sitios de `support_sites` o `medial_sites`, con su ancho y su índice.

**Medido** (esfera, capa del medio, 4 núcleos):
- `extract_toolpaths`: de 50–54 ms a 3,3–3,9 ms (125 lazos);
- `info`, filtro + transiciones + reparto + extracción: de 58–65 ms a 5,8–6,2 ms. El eje medio sigue en 5,5–5,9 ms,
  así que una capa entera cuesta unos 12 ms.

**Sigue:**
- meta-esquema de cáscara (ecuación 8): M paredes, con nodos extra donde el reparto cambia de forma;
- perímetros de todas las capas en paralelo dentro de `slice_model`, con la cantidad de paredes como ajuste propio en
  `docs/CONFIG.md`.

## 2026-10-24 — bloque nocturno 18: límite de paredes (cáscara)

**Hecho:**
- `BeadingParams::wall_count` (M): como mucho M paredes desde cada borde; 0 llena toda la región como antes.
- Kuipers et al. 2020, sección 5.1 (meta-esquema de cáscara, ecuación 8): la cantidad de cordones se satura en 2M.
  El texto extraído de la ecuación no se lee con claridad, así que la forma es propia y continua: desde el grosor
  donde aparecería el cordón 2M + 1 (`shell_ramp().start`), los 2M cordones se angostan linealmente hasta w* en w*/2
  de grosor extra; el hueco del medio se abre de 0 a w* y después crece con el grosor. Con el esquema uniforme no
  hace falta rampa.
- `apply_transitions` agrega nodos en el centro donde el grosor cruza el inicio y el fin de esa rampa (la sección 5.1
  pide nodos en las no linealidades), con la cantidad interpolada.

**Probado:** 109/109 pruebas con gcc, clang y ASan+UBSan:
- reparto: saturación en 2M, sin cambios bajo la rampa, continuidad al empezar, mitad de camino a la mitad, dos
  paredes de 0,4 mm desde cada borde al terminar (2, 3 y 25 mm), uniforme sin rampa;
- tira de 2 mm: con M = 3 sigue con 2 lazos y la línea del medio; con M = 2, dos lazos exactos de 0,4 mm y nada en el
  medio;
- cuadrado de 20 mm: M = 1 y M = 3 dan exactamente M lazos de 0,4 mm con el área exacta;
- cuña: aparecen nodos centrales exactamente en el inicio y el fin de la rampa, y ningún cordón pasa del índice M − 1.
- Un error mío en una prueba: esperaba 2 cordones en 1 mm y son 3 (⌊1/0,4 + ½⌋).

**Medido** (esfera, capa del medio, 4 núcleos): con M = 3, 3 lazos cerrados; reparto + extracción 0,3 ms. El eje medio
(5,2–5,6 ms) es ahora casi todo el costo de la capa.

**Sigue:**
- la región que queda para el relleno (lo que no cubren las paredes), como polígonos;
- perímetros de todas las capas en paralelo dentro de `slice_model`, con la cantidad de paredes como ajuste propio en
  `docs/CONFIG.md`.

## 2026-10-24 — bloque nocturno 19: paredes en todas las capas y región para el relleno

**Hecho:**
- `layer_walls` (`src/perimeters/walls.*`): la cadena completa de paredes sobre una región y la región que dejan para
  el relleno. Con M paredes, esa región es la región achicada M·w* sin las partes de menos de w*/2 (diseño propio):
  - en lo ancho, la cáscara tiene sus M paredes exactamente a w*;
  - en lo angosto, los cordones llenan todo y no queda nada;
  - en la rampa de la cáscara, la región se superpone con las paredes ensanchadas hasta w*/4 de cada lado.
- `slice_model` calcula las paredes de todas las capas dentro del mismo `parallel_for` (oneTBB). `SlicedModel::walls`
  guarda los recorridos y la región para el relleno de cada capa.
- Ajustes: grupo `walls` con `count` (3; el valor de banco de `docs/spec/SETTINGS_V1.md`, con nombre propio) y
  `width` (0,4), en `docs/CONFIG.md`. `count` 0 no hace paredes y deja toda la región para el relleno.
- `tisma-engine slice` informa `wall_toolpaths`.

**Probado:** 111/111 pruebas con gcc, clang y ASan+UBSan:
- tira de 2 mm con 2 paredes: el relleno es la tira del medio de 28,4 × 0,4 mm; con 3, los 5 cordones la llenan y no
  queda relleno;
- cuadrado de 20 mm con 3 paredes: relleno de 17,6 × 17,6 mm;
- cubo de 10 mm por `slice_model`: 3 lazos cerrados de 0,4 mm en cada una de las 50 capas y relleno de 7,6 × 7,6 mm;
  con `count` 0, ningún recorrido y el relleno igual a la región;
- lectura del grupo `walls` y sus errores (no entero, negativo, ancho 0).

**Medido** (esfera de 1 001 112 triángulos, 500 capas, 4 núcleos):
- `slice` con 3 paredes: 1500 recorridos (3 por capa), 0,99–1,03 s. Sin paredes: 0,20–0,22 s;
- casi todo el costo nuevo es el eje medio de Boost (5,2–5,6 ms en la capa del medio).

**Sigue:**
- acelerar el eje medio. Idea: con M paredes solo hace falta el eje cerca del borde (hasta unos M·w* + w*), y en las
  partes anchas la cáscara es un desplazamiento exacto que Clipper2 hace mucho más rápido;
- resolución de conflictos de reparto (figura 9);
- contrastar con la ficha de perímetros cuando esté en `main`.

## 2026-10-24 — bloque nocturno 20: perfil del eje medio

**Hecho:** solo mediciones; no cambió el código del motor.

**Medido** (esfera, 500 capas, un hilo; arneses en el scratchpad):
- de los 2713 ms de `medial_graph` en todas las capas, 2645 ms son la construcción del Voronoi de Boost; nuestro
  recorrido de aristas cuesta 2–3 %;
- Boost cuesta lo mismo con otras formas de 212 segmentos: círculo 5,3 ms, elipse 6,8, engranaje 3,4, círculo con
  ruido de 20 µm 6,5, y una tira en zigzag de 214 puntos 2,8 ms (13–28 µs por segmento);
- con callgrind, el 54 % de las instrucciones está en `extended_int<64>::mul` (aritmética exacta) y otro 13 % en
  `frexp`, `robust_sqrt_expr` y restas exactas. En un polígono cada vértice es extremo común de dos segmentos, y Boost
  resuelve esos casos en precisión exacta;
- el artículo informa 5·10⁻⁶·n·log n s para su Voronoi (unos 8 ms para 212 vértices): estamos en ese nivel;
- probé tapar el fondo de las piezas anchas con un contorno desplazado 2·R hacia adentro como agujero. Por la
  desigualdad triangular no cambia el eje donde el radio es menor que R, pero duplica los segmentos y Boost tardó
  2,4 veces más. Descartado.

**Sigue:**
- el eje medio queda en el PLAN con dos opciones (Voronoi propio a partir de artículos, pensado para la GPU, o
  híbrido con desplazamientos de Clipper2 en las partes anchas); son varios bloques y conviene decidirlo con el dueño;
- mientras tanto, conflictos de reparto (sección 3.5, figura 9): interpolar desde el nodo marcado de abajo hacia el
  reparto de arriba a lo largo de w*.

## 2026-10-24 — bloque nocturno 21: conflictos de reparto

**Hecho:**
- Error encontrado y arreglado: una arista entre dos esquinas (por ejemplo, bajo una aleta fina, entre sus dos
  esquinas reflexas) es recta, pero su radio crece como una hipérbola, R = √(a² + s²). Los pasos siguientes interpolan R
  en línea recta entre nodos, y en la T de prueba los sitios se desviaban hasta ~0,2 mm. Ahora esas aristas se parten
  en tramos de hasta 0,2 mm, como las parábolas.
- Conflictos de reparto (Kuipers et al. 2020, sección 3.5, figura 9), en `node_beadings`: subiendo por un hueso no
  marcado desde un nodo marcado, el reparto pasa del de ese nodo al de la cima a lo largo de w* de camino. Si un nodo se
  alcanza desde varios nodos marcados, sigue al más cercano.
- `blend_beadings`: mezcla dos repartos cordón por cordón; un cordón que solo tiene uno de los dos se desvanece escalando
  su ancho. `interpolate_beading` (rampas) la usa también.

**Probado:** 113/113 pruebas con gcc, clang y ASan+UBSan:
- en la T (aleta de 1 mm sobre un cuerpo de 6 mm), los tramos de la bisectriz bajo la aleta miden como mucho 0,2 mm y
  tienen el radio exacto;
- los nodos del hueso a menos de w* del último nodo marcado de la aleta tienen el reparto interpolado exacto, y los de
  más arriba el del cuerpo; los recorridos quedan adentro de la T con anchos positivos;
- la tira, el 64-gono, la L y la cuña siguen igual.
- Mi primera versión de la prueba tomaba como base el nodo en y = 6, pero con la bisectriz partida, el primer tramo
  debajo es central (su pendiente de R es 0,19), así que el último nodo marcado está en y = 5,80. El error era de la
  prueba.

**Medido** (esfera): sin cambios, 125 lazos en la capa del medio; `slice` con 3 paredes 1,00–1,07 s.

**Sigue:**
- el punto del cordón central impar en un máximo aislado;
- contrastar con la ficha de perímetros cuando esté en `main`;
- la decisión del dueño sobre cómo acelerar el eje medio.

## 2026-10-24 — bloque nocturno 22: el punto del cordón central

**Hecho:**
- `extract_toolpaths`: un cordón que está justo sobre un nodo y no forma ningún segmento se emite como un recorrido de
  un solo punto con su ancho, para que el G-code pueda imprimir ese punto. Pasa en el centro de un polígono regular
  con cantidad impar, y en el extremo grueso de una cuña donde el último cordón aparece en un solo nodo.
- Revisé el estado de F2:
  - «dominios por contorno» ya no hace falta, porque el encadenado por identidad lo reemplazó;
  - falta el criterio de terminación de F2: medir el área cubierta (huecos y excesos) en paredes de 0,3 a 1,2 mm y en
    piezas comunes con 2–3 perímetros. Queda como próximo ítem.

**Probado:** 113/113 pruebas con gcc, clang y ASan+UBSan. El 64-gono de radio 5 da 12 lazos y un punto en el centro,
de ancho 2r/25, índice 12. La tira, la L y la T no cambian (sin puntos). En cuñas de 15° y 30° aparece un punto en el
nodo más grueso, donde el último cordón existe solo ahí; lo revisé y es correcto.

**Sigue:**
- medir el área cubierta para el criterio de F2;
- la decisión del dueño sobre el eje medio.

## 2026-10-24 — bloque nocturno 23: área cubierta (criterio de F2)

**Hecho:**
- `bead_area` y `measure_coverage` (`src/perimeters/coverage.*`, diseño propio), con Clipper2. Cada tramo de un
  recorrido es el cuadrilátero de medios anchos w0/2 y w1/2, y cada punto suma un disco de radio w/2 (uniones y
  extremos redondos, polígono de 64 lados). Informa:
  - el área cubierta del objetivo;
  - los huecos;
  - lo que queda afuera;
  - la superposición: la suma de las áreas de cada recorrido menos el área de su unión.

**Probado:** 117/117 pruebas con gcc, clang y ASan+UBSan.

**Medido** (área cubierta; las tres primeras filas son pruebas, la última sale del arnés):

| Pieza | Huecos | Superposición | Afuera |
|---|---|---|---|
| Tiras de 30 mm × 0,3 a 1,2 mm (de a 0,1) | 0,019–0,078 mm² (0,13–0,43 %) | 0 | 0 |
| Cuña de 0,3 a 1,2 mm | 0,073 mm² (0,3 %) | 0,28 mm² (1,3 %) | 0 |
| Cuadrado de 20 mm, 2 y 3 paredes (franja de paredes) | 0,069 y 0,104 mm² (0,11 %) | 0 | 0 |
| Esfera, capa del medio, lleno completo / 3 paredes | 0,0003 % / 0,0003 % | 0,0001 % / 0 | 0 |

- En las tiras, los huecos son exactamente las esquinas redondeadas de los extremos: 4·(w/2)²·(1 − π/4). En la tira
  de 0,3 mm la cuenta da 0,0193 mm² y medí 0,0194. A lo largo de las paredes no queda nada sin cubrir.
- En la cuña, la superposición está en las transiciones, donde un cordón empieza o termina con extremo redondo.
- La tira de 0,6 mm sale con un solo cordón de 0,6: 0,6/0,4 en coma flotante da 1,4999…, justo en el límite entre 1
  y 2 cordones. Cualquiera de los dos cubre bien.
- Medir el lleno completo de la capa de la esfera tarda 5 s (unión de unos 87 000 discos). Es una herramienta de
  medición y no corre en el corte.

**Estado de F2:** el criterio de la fase («paredes de 0,3 a 1,2 mm sin huecos ni excesos; piezas comunes con 2–3
perímetros») se cumple y está medido. Quedan:
- la decisión del dueño sobre cómo acelerar el eje medio;
- contrastar con la ficha de perímetros cuando esté en `main`.

**Sigue:** F3 (superficies y rellenos), a partir de `docs/spec/BEHAVIOR.md` («Superficies y rellenos»), si pasa
`tools/check_spec.py`.

## 2026-10-24 — bloque nocturno 24: F3, superficies superiores e inferiores

**Hecho:**
- `classify_surfaces` (`src/surfaces/surfaces.*`, diseño propio con Clipper2) divide la región de relleno de cada capa
  en superior, inferior y dispersa:
  - una parte es superior si alguna de las N capas de arriba no la cubre, o sea, fuera de la intersección de sus
    regiones; pasada la última capa no se cubre nada;
  - inferior, igual con las capas de abajo; lo superior gana sobre lo inferior;
  - las capas corren en paralelo (oneTBB).
- `slice_model` la aplica después de las paredes (`SlicedModel::surfaces`); `slice` informa `top_area`,
  `bottom_area` y `sparse_area`.
- Ajustes: grupo `surfaces` con `top_layers` y `bottom_layers` (3 y 3, valores de banco de la especificación, con
  nombres propios), en `docs/CONFIG.md`.
- `PLAN.md`: lista «Estado de F3» desde la especificación de superficies y rellenos.

**Probado:** 120/120 pruebas con gcc, clang y ASan+UBSan:
- columna de 10 capas iguales: con 3 arriba y 2 abajo, las capas 7–9 son superiores, 0–1 inferiores y el resto
  disperso; con 0/0 todo es disperso;
- cubo de 10 mm por `slice_model`: capas 0–2 inferiores, 47–49 superiores, el resto disperso, cada una con el relleno
  de 7,6 × 7,6 mm;
- escalón (caja de 20 mm con otra de 10 mm encima): las capas 22–24 tienen superior el relleno menos el cuadrado de
  10 mm (209,76 mm²) y la capa 21 nada; la caja de arriba, apoyada, no tiene superficie inferior;
- lectura del grupo `surfaces` y sus errores.

**Medido** (esfera, 500 capas, 4 núcleos): `slice` 1,02–1,11 s (antes 1,00–1,07 s); superior e inferior simétricas,
1580 mm² cada una sumando capas.

**Sigue:** el ancho mínimo de las superficies sólidas, y después el relleno rectilíneo.

## 2026-10-24 — bloque nocturno 25: ancho mínimo de las superficies sólidas

**Hecho:**
- `classify_surfaces` recibe un ancho mínimo (el del cordón; `slice_model` pasa `walls.width`). Las partes sólidas más
  angostas que eso (lo que quita una apertura de medio cordón) crecen un cordón hacia la región de relleno.
- Diseño propio. Elegí crecer y no descartar: en una pendiente suave la piel superior de cada capa es un anillo fino
  junto a las paredes, y descartarlo dejaría el relleno disperso a la vista.

**Probado:** 121/121 pruebas con gcc, clang y ASan+UBSan:
- en una pirámide escalonada (0,5 mm hacia adentro por lado y por capa), la piel superior es un anillo de 0,3 mm; con el
  mínimo de 0,4 mm pasa a 0,7 mm, tomado del relleno disperso;
- las capas que ya son sólidas enteras no cambian.

**Medido** (esfera, 500 capas, 4 núcleos):
- superficies superior e inferior, sumando capas: de 1580 a 2152–2153 mm²;
- `slice`: 1,12–1,13 s.

**Sigue:** el relleno rectilíneo (líneas a 45° que giran 90° cada capa, 20 %, sólido al 100 %), con un grupo de ajustes
`infill` propio.

## 2026-10-24 — bloque nocturno 26: relleno rectilíneo

**Hecho:**
- `rectilinear_fill` (`src/infill/rectilinear.*`, diseño propio): líneas paralelas en una grilla fija al origen, así
  las de distintas capas se alinean. Se recortan como caminos abiertos con Clipper2 a la región achicada medio
  cordón y salen como recorridos de ancho constante, ordenados entre líneas y a lo largo de cada una.
- `fill_layer`:
  - superficies superior e inferior al 100 % (un cordón de separación);
  - relleno disperso a ancho / densidad;
  - ángulo base, más 90° en las capas impares.
- `slice_model` arma el relleno de todas las capas en paralelo (`SlicedModel::fills`); `slice` informa
  `solid_infill_lines` y `sparse_infill_lines`.
- Ajustes: grupo `infill` con `density` (20), `angle` (45) y `pattern` (`rectilinear`), valores de banco de la
  especificación con nombres propios.

**Probado:** 124/124 pruebas con gcc, clang y ASan+UBSan:
- un cuadrado de 10 mm con líneas a 1 mm da 9 líneas en y = 1…9, de x = 0,2 a 9,8; a 90° son verticales en x enteros;
  corrido 0,5 mm, da las mismas líneas de la grilla;
- las capas pares van a 45° y las impares a 135°; las líneas dispersas al 20 % son unas cinco veces menos que las
  sólidas; densidad 0 no da relleno disperso;
- una capa sólida de 7,6 mm a 45°: el interior, un cordón hacia adentro, queda cubierto (huecos < 0,1 %).

**Medido:**
- capa sólida de 7,6 mm, 25 líneas a 45°: huecos de 2,72 mm² (4,7 %) en los bordes, nada afuera ni superpuesto;
- si las líneas llegan hasta el borde, los huecos bajan a 0,22 mm² (0,4 %), pero 3,69 mm² se meten en las paredes. A 0°
  la alineación con la grilla deja además una franja sin cubrir junto a un borde;
- esfera, 500 capas, 4 núcleos: 7743 líneas sólidas y 18 907 dispersas; `slice` 1,04–1,12 s (el relleno casi no suma).

**Sigue:** anclar las líneas a los perímetros uniendo los extremos a lo largo del borde, para cerrar los huecos de los
bordes sin meterse en las paredes; después grilla y giroide.

## 2026-10-24 — bloque nocturno 27: anclaje del relleno

**Hecho:**
- Las líneas de relleno se anclan a los perímetros en zigzag (diseño propio):
  - dos extremos de líneas adyacentes que se siguen a lo largo del borde de la región achicada medio cordón, a menos
    de 3 separaciones, se unen por ese tramo de borde;
  - una unión-búsqueda evita ciclos; las líneas unidas forman una sola polilínea;
  - no se mete nada en las paredes.
- Superficies sólidas: las líneas se ajustan a la región (tantas como entran, centradas) en vez de ir sobre la grilla
  fija. El relleno disperso sigue en la grilla, para que las capas se alineen.
- `measure_coverage`: la superposición ahora es material depositado (largo por ancho medio de cada tramo, más medio
  disco en cada extremo abierto) menos el área cubierta. Así cuenta lo que un mismo recorrido deposita dos veces (el
  zigzag sobre sus propios extremos, o el lado interno de una esquina viva). Con esa medida, un lazo con esquinas rectas
  deposita exactamente el área: lo que le falta en la esquina de afuera lo pone en la de adentro.

**Probado:** 124/124 pruebas con gcc, clang y ASan+UBSan. Las pruebas de cobertura de F2 ahora comparan también el
material depositado con el área.

**Medido** (capa sólida, material depositado menos área; nada entra en las paredes):

| Región | 0° | 30° | 45° |
|---|---|---|---|
| Cuadrado de 7,6 mm, antes (líneas sueltas sobre la grilla) | −6,3 % | −5,0 % | −4,7 % |
| Cuadrado de 7,6 mm, ahora | −0,06 % | −0,9 % | −0,6 % |
| L de 10 mm con brazos de 3 mm, ahora | −2,9 % | −1,7 % | −1,8 % |

- En la L a 0°, el ajuste es para toda la región y en el codo interior queda una franja de 0,2 mm sin cubrir. Queda en
  el PLAN ajustar por tramo.
- Cuña de F2 con la nueva medida: deposita 2,3 % más que su área, en las transiciones.
- Esfera, 500 capas: las polilíneas bajan de 7743 a 630 (sólidas) y de 18 907 a 1173 (dispersas); `slice` 1,13–1,20 s.

**Sigue:** ajustar las líneas sólidas por tramo; después grilla y giroide.

## 2026-10-24 — bloque nocturno 28: relleno en grilla

**Hecho:**
- `infill.pattern = "grid"`: las dos direcciones (ángulo y ángulo + 90°) en cada capa, cada una a 2·ancho/densidad,
  así deposita el mismo material que el rectilíneo. Las líneas no se unen en zigzag: se cruzan entre sí y sus extremos
  tocan las paredes (diseño propio).
- Revisé `docs/spec/SOURCES.md` (pasa `tools/check_spec.py`): el giroide es sin x·cos y + sin y·cos z + sin z·cos x = 0.

**Probado:** 125/125 pruebas con gcc, clang y ASan+UBSan:
- en un cuadrado de 20 mm hay tantas líneas a 45° como a 135°, en todas las capas;
- material depositado al 20 %: grilla 78,7 mm² (19,7 % del área), rectilíneo 88,6 mm² (22,2 %; los conectores del
  zigzag suman);
- lectura de `"grid"`.

**Sigue:** el giroide. Idea (diseño propio): a z fijo, la ecuación se resuelve para y dado x:
a·cos y + b·sin y = −c, con a = sin x, b = cos z y c = sin z·cos x, o sea y = φ ± arccos(−c/R). Eso da las curvas
muestreando x, sin una grilla 2D. El período se calibra para que deposite densidad × área.

## 2026-10-24 — bloque nocturno 29: relleno giroide

**Hecho:**
- `gyroid_fill` (`src/infill/gyroid.*`, diseño propio sobre la superficie de `docs/spec/SOURCES.md`):
  - cuadrados marchantes en una grilla fija al origen, de 32 pasos por período;
  - senos y cosenos precalculados por fila y columna, así cada muestra son tres productos;
  - las sillas se resuelven con el promedio de las cuatro esquinas;
  - los segmentos se encadenan por la identidad del cruce y se recortan como caminos abiertos con Clipper2.
- Período = 2,61·ancho/densidad (5,22 mm a 0,4 mm y 20 %). La constante 2,61 se midió numéricamente: largo de curva por
  unidad de área por período, contando cruces sobre líneas de prueba y promediando en z.
- Cambié el método respecto de lo anotado (resolver y en función de x): los cuadrados marchantes son más simples y no
  necesitan unir ramas.
- `infill.pattern = "gyroid"`; `fill_layer` recibe la altura de la capa.

**Probado:** 126/126 pruebas con gcc, clang y ASan+UBSan:
- los puntos cumplen la ecuación con error < 0,01 (en unidades de la función) y quedan dentro de la región achicada;
- promediado en 8 alturas, deposita 80,1 mm² en un cuadrado de 20 mm al 20 % (20,0 %);
- a otra altura las curvas cambian; `fill_layer` da lo mismo que `gyroid_fill`; lectura de `"gyroid"`.

**Medido** (esfera, 500 capas, 4 núcleos), `slice` según el patrón disperso:

| Patrón | Recorridos dispersos | Tiempo |
|---|---|---|
| Rectilíneo | 1173 | 1,14–1,15 s |
| Grilla | 18 916 | 1,13–1,14 s |
| Giroide | 17 713 | 1,97–2,02 s |

**Sigue:**
- acelerar el giroide (la grilla recorre toda la caja de cada capa; es candidato a la GPU) y anclarlo a las paredes;
- puentes;
- superficies monotónicas.

## 2026-10-24 — bloque nocturno 30: puentes

**Hecho:**
- `find_bridges` (`src/surfaces/bridges.*`, diseño propio desde `docs/spec/BEHAVIOR.md`, «Puentes»):
  - lo impreso en la capa de abajo se mide con lo que realmente se deposita: el área de los cordones de sus paredes,
    sus superficies sólidas y su región dispersa si hay relleno. Así, apoyar sobre el relleno de abajo no es puente,
    como pide la lección medida en Tisma;
  - el puente es la parte de la superficie inferior fuera de eso, sin partes más finas que un cordón, agrandado dos
    cordones sobre lo apoyado para anclarse;
  - ángulo: se prueban pasos de 5° y gana el de más largo de líneas con los dos extremos apoyados; entre iguales, el de
    líneas más cortas. El criterio «línea más corta» solo no alcanza: en una placa sobre dos pilares separados 10 mm,
    las líneas paralelas al hueco (7,6 mm) son más cortas pero cuelgan en el aire;
  - las líneas del puente van rectas, sin zigzag (una unión por el borde quedaría en el aire).
- `LayerSurfaces::bridge` y `bridge_angle`; `LayerFill::bridge`; `slice` informa `bridge_lines`.
- Atajo: con relleno disperso, lo impreso abajo cubre su región salvo rendijas finas, así que una superficie inferior
  dentro de la región de abajo no es puente y no hace falta calcular el área de los cordones. Sin el atajo el corte de
  la esfera pasaba de 1,14 a 1,48–1,64 s; con él, 1,10–1,19 s.

**Probado:** 128/128 pruebas con gcc, clang y ASan+UBSan:
- mesa (dos pilares de 5 × 10 mm y una placa de 20 × 10 mm encima): la primera capa de la placa tiene un puente de
  10 × 7,6 mm entre los pilares, a 0°, con líneas de pilar a pilar; la capa siguiente apoya en el puente y es superficie
  inferior común; ninguna otra capa tiene puente (tampoco la primera, sobre la cama);
- el cubo no tiene puentes.

**Medido** (esfera, 500 capas, 4 núcleos): 35 líneas de puente, cerca del polo de abajo, donde cada capa sobresale de la
anterior más que el ancho de las paredes; `slice` 1,10–1,19 s.

**Sigue:** superficies superiores monotónicas.

## 2026-10-24 — bloque nocturno 31: superficies superiores monotónicas

**Hecho:**
- Orden monotónico (diseño propio desde `docs/spec/BEHAVIOR.md`, «Superficies monotónicas»):
  - las superficies superiores llevan líneas sueltas, ajustadas a la región, todas en el mismo sentido;
  - cada pieza espera a las piezas de la línea de abajo que la tocan (tramos ampliados medio cordón);
  - entre las que pueden ir, va la que empieza más cerca de donde terminó la anterior. Así, en una U cada brazo se hace
    de una vez.
- Las superficies inferiores siguen en zigzag.
- Ajuste `surfaces.top_pattern`: `monotonic` (por defecto, valor de banco de `SETTINGS_V1.md`) o `rectilinear`.
- Precio: las líneas monotónicas no se unen por el borde, así que la superficie superior vuelve a tener los huecos de
  los extremos que el zigzag cerraba (4–6 % en un cuadrado de 7,6 mm, según el ángulo).

**Probado:** 129/129 pruebas con gcc, clang y ASan+UBSan:
- en un cuadrado y en una U, todas las líneas van de izquierda a derecha y ninguna va antes que una pieza que la toca
  en la línea de abajo;
- en la U, después de la barra se hace un brazo entero y después el otro (un solo cambio de brazo);
- lectura de `top_pattern`.

**Medido** (esfera, 500 capas, 4 núcleos): 4201 líneas sólidas (antes 630, porque las superiores ya no se unen);
`slice` 1,13 s.

**Sigue:** en la lista de F3 quedan:
- ajustar las líneas sólidas por tramo;
- acelerar y anclar el giroide;
- los núcleos de GPU (superficies y giroide);
- el criterio de F3, que necesita el Benchy del dueño.

El próximo bloque toma la GPU del giroide, que es lo que más tarda.

## 2026-10-24 — bloque nocturno 32: giroide más rápido

**Medido primero** (esfera, 500 capas, un hilo, con una copia del código con relojes en el scratchpad):
- 9,4 millones de puntos de giroide, uno cada 0,16 mm; 4,06 s;
- por fase:

| Fase | ms |
|---|---|
| Evaluación | 141 |
| Reservas | 78 |
| Marcha | 570 |
| Encadenado | 511 |
| Simplificación y recorte | 1070 |
| Salida | 83 |

**Hecho:**
- Las curvas se simplifican antes del recorte (`SimplifyPaths` de Clipper2, 5 µm): de 9,4 a 5,5 millones de puntos, de
  4,06 a 2,39 s. Con 10 o 20 µm quedan menos puntos, pero el tiempo casi no baja (lo que queda es la grilla) y el
  error pasa el umbral de la prueba; me quedé con 5 µm.
- La grilla baja de 32 a 24 pasos por período: 1,78 s en un hilo y 4,4 millones de puntos. El error de cuerda sigue
  debajo de 0,01 mm y el material sigue en 20,0 %.
- Los arreglos de cruces pasan a 32 bits.

**Probado:** 129/129 pruebas con gcc, clang y ASan+UBSan. La prueba del giroide (puntos sobre la superficie con error
< 0,01, material 20,0 %) pasa sin cambios.

**Medido** (esfera, `slice` con giroide, 4 núcleos): de 1,97–2,02 s a 1,57–1,64 s. Con rectilíneo tarda 1,13 s, así que
el giroide pasó de sumar 0,85 s a sumar 0,45 s.

**Nota:** en callgrind apareció un ordenamiento de pares (el índice de aristas de los contornos) con el 32 % de las
instrucciones. La medición nativa del corte de 500 capas da 66–87 ms, así que no cuadra; queda para revisar.

**Sigue:**
- anclar el giroide y su núcleo de GPU;
- el ajuste de las líneas sólidas por tramo.

## 2026-10-24 — bloque nocturno 33: fases del corte, relleno de huecos y robustez del eje medio

**Medido** (esfera, nativo, 4 núcleos, por fase de `slice_model`):

| Fase | ms |
|---|---|
| Contornos | 85–89 |
| Regiones | 9–11 |
| Paredes | 810–825 |
| Superficies | 38–40 |
| Puentes | 35–43 |
| Relleno | 71–74 |

El ordenamiento que callgrind mostraba con el 32 % no es el índice de contornos: el corte entero tarda menos de 90 ms.
Lo que domina son las paredes (el Voronoi de Boost), como ya se sabía.

**Hecho:**
- `gap_fill` (diseño propio): lo que las líneas sólidas no cubren, sin partes de menos de medio cordón, se rellena con
  cordones de ancho variable de la cadena de paredes, sin límite. Mejora la superficie superior monotónica del cuadrado
  a 30° de −4,8 % a −3,4 % y la de la L de −7,3 % a −5,1 %. En el codo de la L a 0° casi no ayuda (−2,88 % → −2,79 %):
  la franja mide justo medio cordón y los conectores del zigzag ya cubren parte. Cuesta 0,5 s en la esfera, así que
  quedó apagado (`FillParams::gap_fill`).
- Con eso aparecieron dos errores del eje medio ante entradas degeneradas, ya arreglados:
  - un foco sobre la recta del segmento (puntos colineales) dejaba una parábola sin ancho y el refinamiento producía
    NaN: ahora la entrada pasa por `TrimCollinear` y ese caso se trata como recta;
  - un canal de 0,2 µm entre dos manchas (dos áreas de cordón que casi se tocan) dejaba una bisectriz enorme entre dos
    esquinas, que `subdivide` intentaba partir en mil millones de pedazos: ahora hay un tope de 100 000 pedazos por
    arista, y el relleno de huecos abre su región un cuarto de cordón antes del esqueleto.

**Probado:** 131/131 pruebas con gcc, clang y ASan+UBSan. Pruebas nuevas:
- el polígono del canal capilar, guardado tal cual, ya no rompe el eje medio;
- el relleno de huecos achica los huecos de una superficie superior a 30°, sin salir de la región ni superponer.

**Sigue:** cerrar los huecos de las superficies sólidas junto a las paredes. Idea: que las líneas de las superficies
superiores entren un cuarto de cordón en la pared interna.

## 2026-10-24 — bloque nocturno 34: solape de las superficies superiores con la pared

**Medido primero** (superficie superior monotónica, cuadrado de 7,6 mm y L, a 0°, 30° y 45°; material depositado
menos área, y lo que entra en la pared interna):

| Cuánto entran las líneas | Material − área, media (peor) | Dentro de la pared, media (peor) |
|---|---|---|
| 0 | −4,87 % (−7,27 %) | 0 |
| 0,05 mm (12,5 %) | −2,16 % (−3,25 %) | 0,92 % (1,24 %) |
| 0,10 mm (25 %) | +0,61 % (−0,52 %) | 2,56 % (3,38 %) |
| 0,15 mm (37,5 %) | +3,37 % | 4,62 % (6,23 %) |
| 0,20 mm (50 %) | +8,37 % | 8,75 % (11,49 %) |

**Hecho:**
- Las líneas de las superficies superiores monotónicas (que no se unen en zigzag) llegan un 25 % de cordón más allá de
  la región achicada, dentro de la pared interna. Es el punto de equilibrio: el material depositado queda entre −0,5 % y
  +1,5 % del área, y entra en la pared entre 1,6 % y 3,4 % del área.
- Las inferiores siguen en zigzag por el borde interno (ya estaban dentro del 1 %).
- Ajuste `infill.wall_overlap` en % del cordón (25 por defecto).

**Probado:** 132/132 pruebas con gcc, clang y ASan+UBSan:
- con el valor por defecto, el material depositado está dentro del 2 % del área y lo que entra en la pared debajo del 4 %;
- con 0 no entra nada en la pared y se deposita menos;
- lectura del ajuste;
- la prueba del relleno de huecos usa ahora solape 0: con el solape ya no quedan huecos de medio cordón a 30°.

**Medido** (esfera, 500 capas, 4 núcleos): `slice` 1,18 s.

**Sigue** (lista de F3):
- la GPU para superficies o giroide;
- anclar el giroide;
- el criterio de F3, que necesita el Benchy (decisión del dueño).

## 2026-10-24 — bloque nocturno 35: giroide anclado, la GPU de F3 y el plan de F4

**Hecho:**
- La unión por el borde pasó a su propio módulo (`src/infill/connect.*`, `join_along_boundary`). Sirve para líneas de
  filas vecinas (el zigzag del rectilíneo) o para cualquier par de piezas.
- El giroide une los extremos de sus curvas a lo largo del borde, hasta medio período, así queda anclado a las paredes
  como pide `docs/spec/BEHAVIOR.md`.
- GPU de F3, decidido con números: no paga.
  - La grilla del giroide cuesta ~80 ms en un hilo para las 500 capas (~20 ms en 4 núcleos). Pasarla a Vulkan movería
    ~0,8 MB por capa en cada sentido, más caro que calcularla.
  - La clasificación de superficies son operaciones booleanas de Clipper2.
- `PLAN.md`: lista «Estado de F4» desde `docs/spec/GCODE_OUTPUT.md` y `BEHAVIOR.md` (pasan `tools/check_spec.py`).

**Probado:** 132/132 pruebas con gcc, clang y ASan+UBSan:
- el giroide deposita 21,3 % al 20 % (las uniones suman 1,3 puntos) y queda en menos de 40 recorridos en un cuadrado
  de 20 mm;
- los puntos que no están sobre el borde siguen sobre la superficie (error < 0,01).

**Medido** (esfera, giroide, 4 núcleos): los recorridos dispersos bajan de 17 713 a 8867; `slice` 1,67–1,77 s (antes
1,57–1,64 s).

**Estado de F3:** está hecho todo lo que no depende del dueño. Quedan el criterio con el Benchy (modelos de banco) y la
decisión sobre el eje medio.

**Sigue:** F4. Primero los ajustes de máquina y velocidades, y un escritor de G-code mínimo con las etiquetas que lee
la vista previa de Tisma.

## 2026-10-24 — bloque nocturno 36: F4, ajustes de máquina y velocidades, y el primer G-code

**Hecho:**
- Ajustes con nombres propios (los perfiles de Tisma los traduce su adaptador):
  - `machine`: boquilla, filamento, E relativa o absoluta, dialecto, G-code de inicio y fin, y un mapa `variables`;
  - `speed`: paredes, pared externa en mm/s o «50%», relleno, sólido, superior, viajes, primera capa y máximo;
  - valores de banco de `SETTINGS_V1.md` donde los hay.
- Las variables del G-code de inicio y fin (`[nombre]`, `{nombre}`, `{nombre[i]}`) se toman del mapa que entrega quien
  llama. Así los nombres de los perfiles de Tisma no entran al motor.
- Escritor de G-code (`src/gcode/writer.*`), siguiendo `docs/spec/GCODE_OUTPUT.md` al pie de la letra:
  - cabecera y G-code de inicio con `;TYPE:Custom`, `G90` y `M82` o `M83`;
  - por capa: `;LAYER_CHANGE`, `;Z:`, `;HEIGHT:`, `G92 E0` si la E es absoluta, y el movimiento en Z;
  - `;TYPE:` con las palabras exactas y `;WIDTH:` cuando cambia;
  - E por volumen, con el ancho medio de cada tramo para las paredes de ancho variable;
  - solo los ejes que cambian, XYZ con 3 decimales, E con 5 y F entera;
  - G-code de fin y un comentario por ajuste.
- Orden dentro de la capa: paredes de adentro hacia afuera con la externa al final, superficies inferiores,
  superiores, puentes, huecos y relleno disperso. Los viajes son rectos, todavía sin planificar.
- `LayerFill` separa las líneas superiores (`top`) de las demás sólidas, porque llevan otra etiqueta.
- `tisma-engine slice` escribe el archivo y sale con 0.

**Probado:** 136/136 pruebas con gcc, clang y ASan+UBSan:
- variables: valor, lista con índice, y nombres desconocidos que quedan como están;
- cubo de 10 mm:
  - la primera línea, el G-code de inicio y fin sustituido, `M82`;
  - 50 capas, cada una con `;Z:` y `;HEIGHT:0.200`;
  - solo etiquetas de la lista de la especificación, con las cinco que corresponden;
  - todas las líneas `G1` con los decimales pedidos;
  - la E total es el volumen esperado dentro del 10 %;
  - los comentarios de ajustes al final;
- con E relativa: `M83`, sin `G92`, y cada movimiento con su propia E;
- lectura de los grupos nuevos y sus errores.

**Medido** (esfera, 500 capas, 4 núcleos):
- `slice` completo con escritura en 3,0 s, de los cuales el corte son 1,27 s;
- 30 MB de G-code con 1,26 millones de líneas `G1`; escribir tarda ~1,4 s.

**Sigue:**
- escribir más rápido;
- orden de islas y recorridos, y costura;
- viajes por dentro con retracción;
- enfriamiento.

## 2026-10-24 — bloque nocturno 37: retracción, orden de recorridos y costura

**Hecho:**
- Grupo `retraction` con valores de banco y nombres propios:
  - `length` 2 mm, `speed` 40 mm/s, `lift` 0 mm, `min_travel` 2 mm;
  - antes de un viaje más largo que `min_travel` se retrae (y sube Z si hay salto);
  - antes de volver a extruir, E vuelve exactamente a donde estaba (absoluta o relativa);
  - una retracción que cruza el cambio de capa sigue valiendo después del `G92`.
- Orden (`src/gcode/order.*`, diseño propio): en cada grupo de la capa (cada índice de pared, de adentro hacia afuera;
  después cada tipo de relleno) va primero el recorrido que empieza más cerca. Las líneas abiertas pueden invertirse y
  los lazos empiezan en su costura.
- Costura (`walls.seam`, `aligned` por defecto, valor de banco), según `docs/spec/BEHAVIOR.md`:
  - `aligned`: esquinas cóncavas, si hay, cerca de la costura de la capa de abajo;
  - `nearest`, `rear` y `random` (determinista).
- Defecto corregido: los tramos que se redondean a ningún movimiento (3 decimales) se escribían como líneas con solo E.
  Ahora no se escriben y su E pasa al siguiente: en la esfera, de 1,26 millones a 853 000 líneas `G1` y de 30 a 24,9 MB.

**Probado:** 139/139 pruebas con gcc, clang y ASan+UBSan:
- retracción en el cubo:
  - 2 mm a 40 mm/s antes de viajes de más de 2 mm, y solo ahí;
  - E vuelve exacta y nunca se extruye retraído;
  - la E absoluta solo crece al extruir;
  - el salto en Z sube y baja 0,3 mm;
- con E relativa, las líneas de solo E son retracciones de ±2 mm;
- una grilla de 36 cuadrados en orden aleatorio viaja menos de la mitad una vez ordenada, y una línea se invierte si su
  otro extremo está más cerca;
- costuras:
  - `rear` en el punto de Y máxima;
  - `nearest` en la esquina más cercana;
  - `random` repetible;
  - `aligned` en la esquina cóncava de una L, y cerca de la costura anterior en un cuadrado;
- lectura de los ajustes nuevos.

**Medido** (esfera, 500 capas):
- viaje total de 70,1 m a 61,3 m (−12,5 %);
- 2056 retracciones;
- 24,9 MB de G-code;
- `slice` 1,25–1,28 s de corte.

**Sigue:**
- viajes por dentro de la pieza (que no crucen perímetros) y retracción al salir de ella;
- orden por islas;
- enfriamiento.


## 2026-10-24 — bloque nocturno 38: viajes por dentro de la pieza

**Hecho:**
- Planificador de viajes (`src/gcode/travel.*`, diseño propio), según `docs/spec/BEHAVIOR.md` («Viajes y retracción»):
  - si el segmento recto queda dentro de la región de la capa, el viaje va recto;
  - si no, va por la región de viaje (la región achicada un cordón, por dentro de la pared externa);
  - un extremo fuera de ella (por ejemplo en la pared externa) salta al punto más cercano de su borde;
  - cada tramo del segmento que sale de la región de viaje se reemplaza por el contorno que cruzó, por el lado más
    corto, y el camino se acorta con atajos rectos que quedan dentro;
  - si los extremos están en piezas distintas (dos objetos, una isla dentro de un agujero) el viaje sale de la pieza.
- El escritor sigue ese camino; retrae cuando el viaje sale de la pieza (aunque sea corto) o cuando el camino planeado
  es más largo que `retraction.min_travel`.

**Probado:** 146/146 pruebas con gcc, clang y ASan+UBSan:
- dentro de un cuadrado, recto;
- entre los brazos de una U, por abajo, a un cordón de las paredes y con el largo del camino más corto;
- desde la pared externa: salta adentro, rodea y vuelve a salir;
- un agujero descentrado se rodea por el lado corto;
- entre dos cuadrados separados 0,5 mm, sale de la pieza aunque mida 1,5 mm;
- hacia una isla dentro de un agujero, sale de la pieza;
- de punta a punta, un tubo cuadrado: ningún viaje cruza el agujero (sin el planificador, 13 lo cruzan).

**Medido:**
- tubo 40 × 40 × 10 mm con agujero de 20 × 20:
  - viajes que cruzan el agujero, de 28 a 0;
  - viaje total de 7,04 a 7,09 m (+0,7 %);
  - 581 retracciones antes y después;
- esfera (500 capas): igual que antes (61,3 m de viaje, 2056 retracciones), porque sus capas son discos y todo viaje ya
  quedaba dentro;
- `slice` con escritura 2,70–2,82 s, corte 1,14 s: el planificador no se nota.

**Sigue:**
- orden por islas;
- enfriamiento;
- escribir el G-code más rápido.

## 2026-10-24 — bloque nocturno 39: orden por islas

**Hecho:**
- Islas (`split_islands`, `island_of` en `src/gcode/order.*`, diseño propio): las piezas conexas de la región de la capa,
  cada contorno exterior con sus agujeros. Una isla dentro de un agujero es otra isla.
- El escritor termina cada isla antes de pasar a la siguiente, siempre la más cercana. Dentro de la isla sigue el orden
  de antes: paredes de adentro hacia afuera con la externa al final, después los rellenos por tipo.
- Defecto corregido en las costuras alineadas: se guardaba una sola costura por índice de pared, la última, y con varios
  lazos del mismo índice (islas, agujeros) cada uno se alineaba con la del otro. Ahora se guardan todas las de la capa de
  abajo y cada lazo toma la más cercana.

**Probado:** 148/148 pruebas con gcc, clang y ASan+UBSan:
- islas de una región: un marco con agujero, una isla dentro del agujero y un cuadrado aparte dan 3 islas; un punto en
  el agujero no está en ninguna; el borde cuenta como dentro;
- de punta a punta, dos cubos: en cada capa se cambia de cubo una sola vez (antes, 5 veces);
- costuras: la más cercana de varias costuras de abajo; `next_layer` conserva los índices que la capa no usó.

**Medido:**
- 9 cubos de 10 mm en una grilla de 3 × 3, 50 capas:
  - viajes entre cubos, de 1623 a 400 (8 por capa, el mínimo);
  - viaje total de 25,2 a 11,2 m (−56 %);
  - retracciones de 1841 a 1031 (−44 %);
  - `slice` con escritura ~0,1 s;
- tubo cuadrado (por las costuras): viaje de 7,09 a 6,86 m, retracciones de 581 a 532;
- esfera: igual que antes (una isla por capa); corte 1,18 s.

**Sigue:**
- enfriamiento (tiempo mínimo de capa, dos etapas, ventilador);
- escribir el G-code más rápido;
- aceleraciones y estimación de tiempo.

## 2026-10-24 — bloque nocturno 40: enfriamiento

**Hecho:**
- Grupo `cooling` (`docs/CONFIG.md`): `min_layer_time` 5 s y `min_speed` 10 mm/s (valores de banco), `fan_min` 35 %,
  `fan_max` 100 %, `fan_off_layers` 1.
- Tiempo mínimo de capa (`src/gcode/cooling.*`, diseño propio), según `docs/spec/BEHAVIOR.md`:
  - el escritor arma cada capa en memoria y suma su tiempo (largo sobre velocidad, con viajes y retracciones, sin
    aceleraciones);
  - si la capa es más rápida que el mínimo, la primera etapa baja todas las extrusiones en la misma proporción, sin bajar
    de `min_speed`;
  - si no alcanza, la segunda las pone a una misma velocidad, la más alta que llega al tiempo; las que ya iban más lento
    no cambian, y nunca por debajo de 1 mm/min;
  - la capa se vuelve a escribir desde el mismo estado con esas velocidades (F redondeada hacia abajo a mm/min enteros,
    para no quedar corta) y el ventilador al máximo.
- Ventilador: `M107` en las primeras `fan_off_layers` capas, `M106 S<0–255>` después, solo cuando cambia.

**Probado:** 152/152 pruebas con gcc, clang y ASan+UBSan:
- una capa más lenta que el mínimo no cambia;
- primera etapa: el mismo factor para todas, ninguna por debajo del mínimo, las más lentas quedan, y el tiempo exacto;
- segunda etapa: velocidad común exacta (2,5 mm/s en el caso de prueba), las más lentas quedan, piso de 1 mm/min;
- de punta a punta en el cubo:
  - sin mínimo, `M107` en la primera capa y `M106 S89` (35 %) en la segunda, una sola vez;
  - con el doble del tiempo de la capa más larga, todas las capas duran al menos el mínimo (medido desde el G-code) con
    `M106 S255`;
  - con 1000 s hace falta la segunda etapa y aparecen velocidades bajo 10 mm/s;
  - con un tiempo inalcanzable, nada baja de `F1`;
- lectura de los ajustes nuevos y sus errores.

**Medido** (esfera, 500 capas):
- una capa frenada (la última, la más chica);
- tiempo de impresión estimado 6,89 h, igual que antes;
- `slice` con escritura 2,85–3,25 s, antes 2,70–2,82 s: armar cada capa en memoria cuesta ~0,1 s.

**Pregunta para el equipo de especificación:** si una de las primeras capas (sin ventilador) tiene que frenar, ¿Tisma
prende el ventilador igual? Hoy el motor lo deja apagado.

**Sigue:**
- escribir el G-code más rápido;
- aceleraciones y estimación de tiempo;
- avisos en JSON.

## 2026-10-24 — bloque nocturno 41: escritura de G-code más rápida

**Hecho:**
- Perfil (callgrind, tubo): el 82 % de las instrucciones del escritor estaban en `snprintf` para formatear números.
- Los números se escriben con `std::to_chars` en formato fijo (la norma lo define con el mismo redondeo que `printf`),
  sin crear cadenas por eje ni por línea: cada capa se arma en un búfer y se escribe de una vez.
- `tisma-engine slice` informa `write_ms`.

**Probado:** 152/152 pruebas con gcc, clang y ASan+UBSan. El G-code es idéntico byte a byte al de antes (`cmp`) en:
- la esfera;
- el tubo y los 9 cubos, con los ajustes por defecto y con E relativa, giroide, salto en Z y tiempo mínimo de capa de
  20 s;
- el cubo con esos mismos ajustes.

**Medido** (esfera, 500 capas, 24,9 MB):
- escritura de 1,56–1,65 s a 0,63–0,77 s;
- en el perfil del tubo, el escritor baja de 1188 a 486 millones de instrucciones; `std::to_chars` es ahora dos
  tercios de lo que queda.

**Sigue:**
- más velocidad: formato propio exacto para pocos decimales, o capas formateadas en paralelo;
- aceleraciones y estimación de tiempo;
- avisos en JSON.

## 2026-10-24 — bloque nocturno 42: límites de la máquina y estimación de tiempo

**Hecho:**
- Ajustes nuevos en `machine` (`docs/CONFIG.md`), con valores por defecto propios:
  - velocidad máxima por eje: X e Y 200, Z 12, E 120 mm/s;
  - aceleración 1000 mm/s² al extruir y 1250 en viajes y retracciones;
  - `corner_speed` 10 mm/s.
- Estimación del tiempo de impresión (`src/gcode/print_time.*`, diseño propio con cinemática de libro de texto):
  - cada movimiento con perfil trapezoidal, o triangular si es corto;
  - la velocidad se recorta para que ningún eje pase su límite;
  - en cada unión, la menor de las dos velocidades por el coseno del giro, nunca menos que `corner_speed`;
  - una pasada hacia atrás y otra hacia adelante por capa, que empieza y termina en reposo.
- El G-code termina con `; estimated printing time = 7h 27m 56s`, el formato de `docs/spec/GCODE_OUTPUT.md`. No cuenta el
  G-code de inicio y fin.
- El enfriamiento sigue usando largo sobre velocidad, sin aceleraciones. Esa cuenta da capas más cortas que la real, así
  que frena un poco de más.

**Probado:** 157/157 pruebas con gcc, clang y ASan+UBSan:
- un movimiento largo coincide con la fórmula cerrada;
- uno corto no llega a su velocidad (triángulo, 2·√(L/a));
- un cuadrado tarda más que la misma longitud en línea recta, y tramos colineales se unen sin frenar;
- los límites de Z y de E recortan la velocidad, también en diagonal;
- formato de las duraciones: `1h 7m 36s`, `7m 36s`, `36s`, `1h 0m 5s`, `2d 1h 0m 5s`;
- el comentario aparece en el G-code del cubo;
- lectura de los ajustes nuevos y sus errores.

**Medido:**

| Modelo | Largo / velocidad | Con aceleraciones |
|---|---|---|
| Esfera, 500 capas | 6,89 h | 7 h 28 min (+8 %) |
| Tubo | 24,6 min | 29 min (+18 %) |
| 9 cubos | 27 min | 34,7 min (+28 %) |

Escritura de la esfera: 0,64–0,76 s, igual que antes. Fuera del comentario nuevo y los ajustes nuevos, el G-code no
cambia.

**Sigue:**
- avisos en un JSON aparte;
- comparar la estimación con el tiempo que calcula Tisma sobre el mismo G-code (caja negra; lo puede hacer el otro
  equipo);
- criterio de F4 (Benchy en una impresora real, con el dueño).

## 2026-10-24 — bloque nocturno 43: avisos en JSON

**Hecho:**
- Avisos (`src/report/warnings.*`, diseño propio), según `docs/spec/BEHAVIOR.md` («Errores y avisos»):
  - `thin_detail`: por capa, el área más fina que el cordón más fino que acepta `bead_count`. Se calcula con una
    apertura (achicar y agrandar) dentro del paso en paralelo por capa; se avisa desde 0,01 mm²;
  - `layer_time`: capas que no llegan a `cooling.min_layer_time` ni a 1 mm/min;
  - `open_contours`: contornos abiertos por capa (antes solo había el total);
  - `gpu_fallback`: la GPU falló y se laminó con la CPU.
- `tisma-engine slice ... --warnings avisos.json` los escribe como `{"version": 1, "warnings": [...]}` y la salida
  informa `warnings: N`. Documentado en `README.md`.

**Probado:** 159/159 pruebas con gcc, clang y ASan+UBSan:
- un cubo con una aleta de 0,1 mm da un aviso `thin_detail` por capa, de 0,5 mm² (0,1 × 5);
- el cubo limpio no da avisos, ni del modelo ni del escritor;
- con un tiempo mínimo imposible, un aviso `layer_time` por capa;
- el JSON vacío exacto;
- desde la línea de comandos, el JSON se escribe y `--warnings` sin valor es un error de uso.

**Medido** (esfera): 0 avisos. Corte 1,16–1,24 s, dentro del rango de antes (1,13–1,28 s): la apertura por capa no
se nota. Escritura 0,64–0,73 s.

**Sigue:**
- objeto fuera de la cama (necesita `bed_shape`) como error claro;
- criterio de F4 (Benchy en una impresora real, con el dueño);
- F5: soportes.

## 2026-10-24 — bloque nocturno 44: entradas imposibles

**Hecho:**
- Defecto corregido: un STL sin triángulos terminaba con código 0 y un G-code vacío.
- `src/slice/printable.*` (diseño propio), según `docs/spec/BEHAVIOR.md` («Errores y avisos»). `slice` termina con un
  mensaje de una línea y el código nuevo 4 cuando:
  - el modelo no tiene triángulos;
  - ninguna capa encierra área (malla plana o abierta);
  - algún vértice queda fuera de `machine.bed` (polígono; el borde cuenta como dentro);
  - el modelo es más alto que `machine.max_height`.
- La comprobación se hace antes de abrir la salida, así que no queda G-code a medias.
- Ajustes nuevos `machine.bed` y `machine.max_height`. Los códigos de salida están en `README.md`.
- **Decisión pendiente:** la esfera del banco está centrada en el origen (de −50 a 50), y con la cama del banco
  (250 × 210 desde el origen) quedaría fuera. Como el motor no mueve los modelos, `bed` vacío por defecto no comprueba
  nada. Hay que saber si Tisma manda los modelos ya ubicados en la cama, o si el motor tiene que centrarlos.

**Probado:** 160/160 pruebas con gcc, clang y ASan+UBSan:
- STL vacío: código 4, «no triangles», sin archivo de salida;
- el cubo con una cama lejos: código 4, «outside the bed», sin salida;
- con una cama justo de su tamaño: se lamina (el borde cuenta como dentro);
- más alto que `max_height`: código 4;
- un cuadrado plano: código 4, «encloses», sin salida;
- lectura de `bed` (menos de 3 puntos o puntos mal formados son errores) y de `max_height`.

**Medido** (esfera): sin cama, igual que antes (corte 1,24–1,38 s; escritura 0,67–0,71 s). Con una cama centrada de
250 × 210, la comprobación de 500 000 vértices no se nota (1,32 s).

**Sigue:**
- según lo que diga Tisma: centrar el modelo en la cama o leerlo ya ubicado;
- criterio de F4 (Benchy en una impresora real, con el dueño);
- F5: soportes.

## 2026-10-24 — bloque nocturno 45: falda y borde (empieza F5)

**Hecho:**
- Grupo `adhesion` (`docs/CONFIG.md`): `skirt_loops` 1, `skirt_distance` 3 mm, `skirt_layers` 1, `brim_width` 0.
- Falda y borde (`src/adhesion/*`, diseño propio con offsets de Clipper2 de los contornos exteriores de la primera capa):
  - el borde son vueltas de un cordón, la primera pegada a la pieza; las de piezas cercanas se unen;
  - la falda va a la distancia pedida de la pieza, o de su borde, y no entra en los agujeros.
- El escritor las imprime primero en sus capas (falda, después borde) como `;TYPE:Skirt/Brim`, a la velocidad de la
  primera capa.
- Nueva sección «Estado de F5» en `PLAN.md`.

**Probado:** 163/163 pruebas con gcc, clang y ASan+UBSan:
- una vuelta de falda a 3 mm de un cubo de 10 mm mide 4 × 16,4 mm;
- con 0 vueltas no hay falda; con 3, la tercera a 3 mm más 2,5 cordones;
- la falda rodea el borde; un agujero no lleva falda dentro;
- un borde de 2 mm da 5 vueltas, la primera pegada a la pieza;
- con dos piezas a 1 mm, la primera vuelta va separada en cada una y la última las rodea a las dos;
- en el G-code del cubo con falda en 2 capas y borde, las dos primeras capas empiezan con `Skirt/Brim` y la tercera no;
- lectura de los ajustes nuevos y sus errores.
- La prueba de islas ahora apaga la falda, porque la falda rodea los dos cubos.

**Medido:**
- esfera: 0,04 m de falda (su primera capa es un círculo chico); corte 1,25–1,31 s, escritura 0,70–0,77 s;
- 9 cubos con borde de 5 mm: 4,04 m de falda y borde; corte 38 ms.

**Sigue:**
- F5: voladizos por ángulo y su proyección, después el soporte en grilla.

## 2026-10-24 — bloque nocturno 46: voladizos y áreas de soporte

**Hecho:**
- Grupo `support` (`docs/CONFIG.md`): `enabled` false, `threshold_angle` 0 (automático), `z_gap` 0,2 mm (valor de
  banco), `xy_gap` 0,6 mm. El ángulo se mide desde la horizontal, como en `docs/spec/SETTINGS_V1.md`.
- Voladizos y áreas de soporte (`src/support/*`, diseño propio con Clipper2):
  - el voladizo de una capa es su región menos la de abajo agrandada altura / tan(ángulo); en automático, medio cordón;
  - de arriba hacia abajo, una columna suma los voladizos (con las capas de separación en Z), pierde lo que ocupa la
    pieza (ahí se apoya en ella) y queda a `xy_gap` de la pieza;
  - la columna se cierra por un cordón. Cada voladizo deja afuera la franja que su capa sostiene sola, así que una
    pendiente se proyectaba como cientos de anillos finos con huecos de 0,2 mm: unos 26 000 vértices y 7,7 s en la
    esfera. Con el cierre, 0,3–0,4 s.
- `SlicedModel.support_areas` por capa; `slice` informa `support_area`. Todavía no se imprimen: las líneas del soporte,
  la interfaz y los puentes cortos vienen después.

**Probado:** 166/166 pruebas con gcc, clang y ASan+UBSan:
- una T: el voladizo bajo el sombrero mide el área exacta, el soporte va de la cama hasta dos capas bajo el sombrero (con
  separación) con el área exacta a 0,6 mm del tronco, y nada arriba;
- una pirámide invertida a 45°: sin voladizo con 40° y con soporte con 50°;
- una repisa sobre una losa: el soporte se apoya en la losa y no sigue hasta la cama;
- lectura de los ajustes nuevos y sus errores.

**Medido** (esfera, 500 capas):
- área de soporte sumada por capa: 131 590 mm², con ángulo automático y con 45°; a mano da unos 135 500 mm² para el
  casquete más tendido que 45°, y la diferencia son las separaciones;
- corte 1,23–1,29 s sin soporte y 1,58–1,68 s con soporte.

**Sigue:**
- F5: las líneas del soporte en grilla, la interfaz densa y la base, y los puentes cortos sin soporte;
- la columna se calcula en serie, de arriba hacia abajo: se puede partir por tramos si hace falta.

## 2026-10-24 — bloque nocturno 47: soporte impreso

**Hecho:**
- Las áreas de soporte ahora se imprimen:
  - base en zigzag sobre una grilla fija al origen, con `support.spacing` (2,5 mm, valor de banco), a 0° y 90° en capas
    alternas;
  - interfaz densa (líneas a un cordón, a 45° de la base) donde la pieza está en las `support.interface_layers` capas
    (3, valor de banco) siguientes a la separación en Z;
  - el escritor las pone después de los rellenos de cada isla, como `;TYPE:Support material` y
    `;TYPE:Support material interface`, a `speed.support` (80 mm/s).
- Defecto corregido, del bloque 45: las vueltas de falda y borde tenían índice 0, el mismo que la pared externa, y su
  costura entraba en la memoria de costuras alineadas. En la esfera eso subía el viaje de 61,3 a 86,7 m y las
  retracciones de 2056 a 2273. Ahora usan índices negativos (−1 la primera): 61,33 m y 2057 retracciones, una más por
  la falda.

**Probado:** 167/167 pruebas con gcc, clang y ASan+UBSan:
- con una T: sin soporte, ningún tipo de soporte en el G-code;
- con soporte, base en las capas 0–5, interfaz en las 6–8 y nada desde la 9 (la separación);
- las líneas de la base caen en múltiplos de 2,5 mm;
- la interfaz pone más de 1,8 mm de línea por mm² y la base menos de 0,7;
- los dos tipos aparecen en el G-code;
- las vueltas de falda y borde tienen índices negativos;
- lectura de los ajustes nuevos.

**Medido** (esfera, 500 capas):

| | Sin soporte | Con soporte |
|---|---|---|
| Corte | 1,27 s | 1,73 s |
| Escritura | 0,77 s | 0,72 s |
| G-code | 24,9 MB | 26,5 MB |
| Tiempo estimado | 7 h 28 min | 8 h 4 min |

**Sigue:**
- F5:
  - puentes cortos sin soporte (`max_bridge_length`);
  - base del soporte menos densa abajo, si el spec lo pide;
  - soporte con el patrón de la ficha cuando llegue;
- criterio de F4 y F5 en una impresora real (con el dueño).

## 2026-10-24 — respuestas del dueño

- **Eje medio:** un Voronoi de segmentos propio, a partir de artículos. Queda como próximo trabajo en «Estado de F2»
  de `PLAN.md`, en pasos: elegir el método en los artículos, módulo propio probado contra Boost como oráculo, medirlo y
  reemplazar.
- **Ventilador:** una de las primeras capas que tiene que frenar lleva el ventilador al máximo. Hecho: el escritor ya
  no lo deja apagado en esas capas; la prueba del cubo lo comprueba (`M106 S255` desde la primera capa cuando todas
  frenan). 167/167 pruebas con gcc, clang y ASan+UBSan.
- **Centrado en la cama:** se decide con la sesión de Tisma; le mandé la pregunta de comportamiento (sin código).

## 2026-10-24 — Voronoi propio, parte 1: método y diagrama de puntos

**Hecho:**
- Método elegido en los artículos: inserción incremental orientada a la topología.
  - Sugihara e Iri 1992 para puntos; Imai 1996 y Held 2001 (VRONI) para segmentos.
  - Fuentes leídas: Held y Huber 2009 (sección 2) y la tesis de Huber 2008 (capítulo 2). Solo artículos; no se abrió
    ningún código (OpenVoronoi es GPL y quedó fuera).
  - Por qué: funciona en coma flotante y es robusta por construcción, porque los números solo eligen entre
    actualizaciones topológicamente válidas. Además inserta un sitio por vez, con cambios locales.
- `src/geom/voronoi.*`, diagrama de puntos:
  - estructura de medias aristas con nodos de grado tres;
  - tres sitios ficticios, con nodos lejanos fijos y aristas exteriores, para que nada sea infinito;
  - en cada inserción, nodo semilla, árbol de nodos marcados con las condiciones de Sugihara e Iri (cada celda
    conserva un nodo y sus nodos marcados quedan contiguos), nodos nuevos en las aristas semimarcadas y la celda nueva;
  - búsqueda del sitio más cercano por caminata entre celdas vecinas, empezando en una grilla gruesa;
  - `check()` verifica la estructura entera.

**Probado:** 171/171 pruebas con gcc, clang y ASan+UBSan. Nuevas, contra Boost.Polygon como oráculo (vértices dentro
de la caja de la entrada): tres puntos (circuncentro), 3000 puntos al azar, una grilla de 40 × 40 (muchos círculos de
cuatro puntos), un 64-gono (casi todos cocirculares) y una estrella. La estructura pasa `check()` en todos.

**Medido** (100 000 puntos):

| Entrada | Propio | Boost |
|---|---|---|
| Vértices de un círculo, en orden | 67–71 ms | 283–305 ms |
| Puntos al azar en orden espacial | 122–164 ms | 217–248 ms |
| Puntos al azar en orden aleatorio | 312 ms (2085 ms sin la grilla) | 222 ms |

**Sigue:** la inserción de segmentos:
- cono de influencia y semilla en la celda de un extremo;
- ruptura de ciclos con nodos de grado dos;
- nodos entre puntos y segmentos;
- partición de las parábolas en su vértice.

Después, la salida `MedialEdge` y la comparación contra Boost en polígonos.

## 2026-10-24 — Voronoi propio, parte 2: segmentos

**Hecho:**
- Inserción de segmentos en `src/geom/voronoi.*`, según Held y Huber 2009 (secciones 2.1–2.3) y Huber 2008:
  - cono de influencia;
  - semilla en las celdas de los extremos;
  - los nodos que coinciden con un punto de la entrada no se borran;
  - ciclos rotos con nodos de grado dos en el punto de la arista más cercano al punto encerrado, que se quitan al
    terminar.
- Diseño propio:
  - distancia al segmento cerrado; fuera del cono, un punto cuenta como no tomado, porque el extremo ya es un sitio;
  - el nodo nuevo se ubica por bisección sobre la arista exacta: la recta sale de los dos sitios, no de los nodos
    extremos, que arrastran redondeo; la parábola se sigue por el pie sobre la recta del segmento;
  - una celda puede tener varias tandas marcadas (un segmento que rodea la celda de un punto);
  - las aristas que salen de cada tanda se marcan por media arista, en tiempo constante.
- Defectos encontrados y corregidos en el camino:
  - la bisección se deslizaba por la zona fuera del cono, donde la diferencia de distancias es cero y el redondeo
    decide;
  - las rectas interpoladas entre nodos lejanos llevaban hasta 2,7 unidades de error a las esquinas;
  - la búsqueda de las aristas de cada tanda era cuadrática.

**Probado:** 174/174 pruebas con gcc, clang y ASan+UBSan. Contra Boost como oráculo, vértices dentro de la caja, con
`check()` después de cada segmento: cuadrado, L, 64-gono, estrella, cuadrado con agujero cuadrado y peine de 10 dientes.

**Medido** (polígonos de radio 25 mm en nm; puntos y segmentos en orden aleatorio, como hace Held), en µs por segmento,
todo incluido:

| Polígono | 500 lados | 5000 lados | 50 000 lados |
|---|---|---|---|
| Círculo, propio | 33 | 32 | 20 |
| Círculo, Boost | 26 | 23 | 25 |
| Estrella, propio | 26 | 30 | 48 |
| Estrella, Boost | 12 | 12 | 12 |
| Curva ondulada, propio | 23 | 24 | 28 |
| Curva ondulada, Boost | 34 | 33 | 32 |

En orden de polígono, el círculo de 500 lados costaba 217 µs por segmento: cada segmento nuevo cortaba la mitad de la
región central.

**Sigue:**
- nodos por fórmula cerrada en vez de bisección (dos tercios del tiempo);
- después, la salida `MedialEdge` y la comparación con el eje medio de hoy.

## 2026-10-24 — Voronoi propio: nodos por falsa posición; los análisis van antes

**Hecho:**
- Los nodos nuevos se ubican por falsa posición con la variante Illinois, siempre dentro del intervalo y con un paso de
  bisección cada tres, en vez de 64 bisecciones. Precisión: 10⁻¹⁵ del tamaño de la caja.
- Las pruebas comparan los vértices dentro de la caja con un margen igual a la tolerancia: una esquina puede caer a
  10⁻⁹ por fuera.

**Probado:** 174/174 pruebas con gcc, clang y ASan+UBSan.

**Medido** (µs por segmento, orden aleatorio):

| Polígono | 500 lados | 5000 lados | 50 000 lados |
|---|---|---|---|
| Círculo, propio | 20 | 18 | 19 |
| Círculo, Boost | 38 | 25 | 27 |
| Estrella, propio | 22 | 26 | 41 |
| Estrella, Boost | 13 | 16 | 14 |
| Curva ondulada, propio | 18 | 20 | 24 |
| Curva ondulada, Boost | 34 | 33 | 34 |

El propio ya es más rápido que Boost en círculos y curvas onduladas. En la estrella es 1,6–2,8 veces más lento.

**Decisión del dueño:**
- los análisis van antes de F5–F7, porque condicionan el diseño de la pieza;
- el diseño es generativo e iterativo, con optimización topológica.

Quedan en `PLAN.md` como fases nuevas:
- A1: elementos finitos;
- A2: optimización topológica y diseño generativo;
- A3: CFD.

**Sigue:** terminar el Voronoi propio (salida para el eje medio, comparación y reemplazo de Boost); después, A1.

**Decisión del dueño (2026-10-24):** A3 incluye, como opción, la simulación del enfriamiento al imprimir:
- el aire del ventilador sobre la pieza;
- el calor de cada capa;
- el ajuste por zona del ventilador, la velocidad y el tiempo de capa;
- la deformación anticipada.

## 2026-10-24 — Voronoi propio, parte 3: reemplaza a Boost en el eje medio

**Hecho:**
- `medial_axis` usa el Voronoi propio. Boost queda como `medial_axis_reference`, solo para comparar en las pruebas.
  - Toda la parte común (aristas interiores, partición de parábolas, radios y apoyos) es una sola clase para los dos.
  - Esquinas y segmentos se insertan en orden aleatorio con una mezcla determinista, igual en todas las plataformas.
- Los nodos unidos por una arista de menos de 1 nm se toman como uno solo, como el vértice exacto de Boost. Un vértice
  de más de tres aristas sale como varios nodos casi juntos.
- Defecto encontrado: el sentido de los lazos de pared dependía del orden de recorrido del grafo. Con Boost salían
  horarios y con el propio antihorarios. En sentido antihorario, unos quiebres de menos de 0,01 mm que ya existían
  contaban como esquinas cóncavas, y la costura alineada se iba a ellos: en la esfera, el viaje subía de 61 a 87 m.
  Arreglos:
  - todo lazo cerrado gira en sentido antihorario;
  - el giro de una esquina para la costura se mide hasta los puntos a 0,1 mm de cada lado.

**Probado:** 175/175 pruebas con gcc, clang y ASan+UBSan.
- Las 174 anteriores (eje medio, perímetros, cobertura, superficies, G-code) pasan con el diagrama propio.
- Prueba nueva: el eje medio propio contra el de Boost en cuadrado, tira, anillo, 64-gono, estrella, curva ondulada de
  400 lados, L y cuña de 0,3 a 1,2 mm. Cada extremo tiene un par a menos de 10⁻⁶ mm, y el largo y el radio integrado
  son iguales. No se exige la misma cantidad de piezas: Boost, exacto, guarda aristas de menos de 1 nm que el propio une.

**Medido** (esfera, 500 capas):
- `slice` de 1,23–1,29 s a 0,94–1,05 s;
- 61,33 m de viaje y 2057 retracciones, igual que con Boost;
- 7 h 28 min estimados;
- tubo: viaje 7,14 m y 545 retracciones; 9 cubos: viaje 11,73 m y 1003 retracciones (cambian por el sentido de los
  lazos y las costuras).

**Sigue:**
- limpiar los quiebres de menos de 0,01 mm de las paredes, que vienen de la extracción;
- acelerar la estrella;
- después, A1 (elementos finitos).

## 2026-10-24 — A1, paso 1: elementos finitos lineales en vóxeles

**Hecho:**
- `src/fea/voxels.*`: la pieza en vóxeles con el corte del propio motor (cada capa de vóxeles es un corte a media
  altura, rellenado por filas).
- `src/fea/hex8.*`: hexaedro trilineal de elasticidad lineal isótropa, con la formulación de libro (Gauss 2 × 2 × 2).
- `src/fea/elasticity.*`:
  - gradiente conjugado con Jacobi y sin armar la matriz: el producto junta por nodo los bloques de los ocho elementos
    de alrededor, en paralelo, sin conflictos y apto para la GPU;
  - productos escalares deterministas;
  - apoyos por eje y cargas como tracción uniforme sobre caras del borde;
  - escala de rigidez por elemento, pensada para la optimización topológica (A2);
  - von Mises en el centro de cada elemento.

**Probado:** 179/179 pruebas con gcc, clang y ASan+UBSan.
- El elemento es simétrico, de diagonal positiva, y sus seis movimientos rígidos no cuestan energía.
- El cubo de 10 mm da 1000 vóxeles con h = 1 y 8000 con h = 0,5.
- Barra de 20 × 4 × 4 mm sobre rodillos a 100 N: alargamiento, contracción lateral y tensión exactos (10⁻⁸).
- Voladizo de 40 × 4 × 4 mm a 10 N: flecha a 2 % de la teoría de vigas (Euler–Bernoulli con el término de corte de
  Timoshenko).

**Medido** (voladizo; teoría 2,880 mm):

| Vóxel | Elementos | Flecha | Error | Iteraciones | Tiempo |
|---|---|---|---|---|---|
| 1 mm | 640 | 2,733 mm | −5,1 % | 180 | 31 ms |
| 0,5 mm | 5120 | 2,820 mm | −2,1 % | 360 | 208 ms |
| 0,25 mm | 40 960 | 2,844 mm | −1,3 % | 718 | 2,6 s |

La von Mises máxima tiende a M·c/I = 37,5 MPa (25,8, 30,9 y 35,6 MPa). Es el valor en el centro de los elementos, que
queda por debajo del de la superficie.

**Pedido del dueño:** tener en cuenta la adherencia entre capas y entre líneas, para la máxima fiabilidad. Queda en
«Estado de A1» como el paso siguiente:
- material ortótropo «como se imprimió», con la dirección del cordón de cada elemento tomada del laminado;
- resistencias propias a lo largo del cordón, entre líneas y entre capas;
- criterio de falla anisótropo y mapa del factor de seguridad.

**Sigue:** el material ortótropo con direcciones del laminado y la falla anisótropa; después, la multigrilla.

## 2026-10-24 — A1, paso 2: vóxeles en forma de caja que siguen las capas

**Pedido del dueño:** en los modelos de alta calidad, 4 vóxeles por ancho de línea y la altura según la altura de
capa, así que no cubos, para simular la adherencia entre capas y entre paredes de la forma más fiable.

**Hecho:**
- Los vóxeles son cajas: hx × hy en el plano y, en altura, filas entre planos z que pueden seguir las capas.
  `layer_planes` parte cada capa (la primera también) en filas enteras, así que los límites entre capas caen en caras
  de elementos.
- El hexaedro se calcula para una caja hx × hy × hz. El solucionador arma un elemento por cada altura de fila distinta
  (dos con una primera capa más gruesa).
- Las cargas se reparten en proporción al área de cada cara.

**Probado:** 180/180 pruebas con gcc, clang y ASan+UBSan.
- Prueba nueva: barra de 2 × 0,4 × 0,7 mm con vóxeles de 0,1 × 0,1 mm, dos filas por capa de 0,2 mm y una primera
  capa de 0,3 mm. A tracción sigue exacta (10⁻⁸) en alargamiento, contracción en Y y en Z, y tensión.
- El voladizo de antes da lo mismo (−2,1 % con 0,5 mm, 181 ms).

**Sigue:**
- material ortótropo «como se imprimió»: dirección del cordón por elemento desde el laminado;
- elementos de interfaz entre capas (en sus caras) y entre cordones, con adherencia propia;
- criterio de falla anisótropo y factor de seguridad.

## 2026-10-24 — A1, paso 3: adherencia entre capas y entre líneas, y la base de materiales

**Pedidos del dueño:**
- tener en cuenta la adherencia entre capas y entre líneas, para la máxima fiabilidad;
- empezar con valores de la literatura y usar además los de fabricantes (Bambu Lab, eSUN) y de CNC Kitchen.

**Hecho:**
- Material ortótropo «como se imprimió» (`src/fea/material.*`):
  - ejes 1 a lo largo del cordón, 2 entre cordones en la capa, 3 entre capas;
  - rigidez como inversa de la compliancia ortótropa, girada al ángulo del cordón (Tᵀ D T);
  - promedio exacto en la capa con 8 ángulos, para las direcciones desconocidas;
  - criterio de Tsai–Wu con su factor de seguridad, y el modo de falla: el cordón, la unión entre cordones o la unión
    entre capas.
- El solucionador acepta varias clases de material, una matriz por clase, elegida por vóxel. `solve_as_printed` usa 36
  direcciones del cordón (pasos de 5°) más el promedio en la capa. Devuelve:
  - el factor de seguridad por vóxel, en los ejes de su cordón (el mínimo sobre las direcciones si no se conoce);
  - el modo de falla, el mínimo y el vóxel más débil.
- La pieza como se imprimió (`src/fea/as_printed.*`):
  - la dirección de cada vóxel sale de los recorridos reales de su capa (paredes, relleno sólido, superior, ralo,
    puentes, relleno de huecos);
  - `printed_voxels` deja solo los vóxeles que cubre un cordón, así que con 4 vóxeles por ancho de línea aparecen los
    huecos del relleno ralo.
- Base de materiales (`src/fea/materials.*`, `docs/MATERIALS.md`), 23 registros. Cada registro guarda solo lo que su
  fuente publica:
  - literatura: Song 2017 (PLA, con compresión), Casavola 2016 (PLA y ABS), Cantrell 2017 (ABS, con Z y corte);
  - hojas técnicas: Bambu Lab (11 materiales, X-Y y Z) y eSUN (PLA+ y PETG Basic);
  - CNC Kitchen: 6 condiciones de impresión (temperatura, cámara, ajustes «transparentes»).
- Lo que una fuente no da lo completan reglas escritas, cada una con la fuente de su cociente (Song, CNC Kitchen, Ahn,
  Cantrell). Ninguna regla agrega resistencia: sin dato de compresión se usa la tracción.
- Antes de usarlos, se compararon con los PDF de las fuentes todos los valores de Bambu Lab, el PLA+ de eSUN y las
  tablas de Song y Cantrell.

**Probado:** 185/185 pruebas (180 de Catch2 y 5 del programa) con gcc, clang y ASan+UBSan; clang compila sin
advertencias. Pruebas nuevas:
- material: el caso isótropo, un cuarto de vuelta, las dos formas de llevar las tensiones a los ejes del cordón, el
  promedio en la capa transversalmente isótropo;
- Tsai–Wu: factor 1 en cada resistencia, 2 y 4 en proporción, y el modo de falla;
- barras de 20 × 4 × 4 mm exactas (10⁻⁸) a lo largo del cordón (E1), a través (E2, también con el ángulo media vuelta
  después) y a través de las capas (E3), con su contracción y su factor de seguridad; con direcciones desconocidas, la
  rigidez queda entre E1 y E2 y el factor no supera el de 90°;
- el cubo de 10 mm laminado con vóxeles de 0,1 mm (4 por línea, 2 filas por capa): paredes con el cordón a 90° y 0°,
  la capa sólida a ±45° en el 100 % del interior, la capa rala cubierta en un 18 % (relleno al 20 %);
- cada registro de la base da un material válido (definido positivo, Zt ≤ Yt, Zc ≥ Zt) y conserva lo medido.

**Medido** (4 núcleos; cubo de 10 mm de PLA Basic de Bambu Lab, base fija, 100 N de corte en la cara de arriba,
tolerancia 10⁻⁶):

| Vóxel | Pieza | Vóxeles | Iteraciones | Tiempo | Desplazamiento máx. | Factor de seguridad mín. | Dónde y cómo |
|---|---|---|---|---|---|---|---|
| 0,2 mm, 1 fila por capa | maciza | 125 000 | 441 | 4,7 s | 0,0386 mm | 3,65 | primera capa, unión entre capas |
| 0,1 mm, 1 fila por capa | maciza | 500 000 | 840 | 36,8 s | 0,0386 mm | 3,21 | primera capa, unión entre capas |
| 0,1 mm, 1 fila por capa | como se imprimió (relleno al 20 %) | 309 014 | 2488 | 71,5 s | 0,1017 mm | 1,55 | capa 46, bajo la piel superior, en el cordón del relleno |

Con los huecos reales del relleno la pieza se deforma 2,6 veces más y la falla se muda de la base a los cordones del
relleno que sostienen la tapa. El caso isótropo no cambió con el refactor: el voladizo da las mismas iteraciones
(360 y 718 con 0,5 y 0,25 mm) y el mismo tiempo dentro del ruido que el commit anterior.

**No probado:** que la falla prevista coincida con una pieza real rota (hace falta un ensayo de caja negra). El
relleno ralo con vóxeles más gruesos que las líneas no se puede representar «como se imprimió»: con esos vóxeles
habrá que homogeneizarlo.

**Sigue:**
- multigrilla: con los huecos del relleno las iteraciones se triplican;
- ajustes del análisis y la línea de comandos: material de la base por su nombre, cargas y apoyos en JSON, mapa del
  factor de seguridad;
- más adelante, la unión entre capas según la temperatura (A3) y elementos de interfaz si el promedio en el vóxel no
  alcanza.

## 2026-10-24 — A1, paso 4: multigrilla

**Pedido del dueño:** seguir con la multigrilla.

**Hecho:**
- El operador de rigidez sin ensamblar pasó a su propio archivo (`src/fea/operator.*`), sin cambios de cálculo.
- Multigrilla geométrica (`src/fea/multigrid.*`), usada como precondicionador del gradiente conjugado:
  - cada nivel parte en dos los elementos de cada eje que tenga más de uno (una placa de un vóxel de espesor se
    engruesa solo en el plano); la interpolación es trilineal y respeta la altura real de cada fila de capa;
  - operador grueso de Galerkin (Pᵀ K P) armado elemento por elemento, sin los grados de libertad fijos ni vacíos: los
    agujeros, las líneas finas y los apoyos llegan a todos los niveles;
  - suavizado polinomial de Chebyshev de grado 3, que solo usa productos con K (paralelo y listo para la GPU);
  - Cholesky denso en el nivel más grueso (menos de 256 nodos);
  - todo por nodo, en paralelo y determinista: dos corridas dan el mismo resultado bit a bit.
- El solucionador recibe `SolverOptions` (tolerancia, iteraciones, precondicionador). La multigrilla es la opción por
  defecto y Jacobi queda como referencia.
- Las piezas que ningún apoyo sostiene quedan fuera del cálculo. Antes su desplazamiento quedaba indeterminado, y la
  multigrilla les sumaba un movimiento rígido. El resultado informa:
  - `loose_voxels`: cuántos vóxeles quedaron sueltos;
  - `unapplied_loads`: las cargas que no encontraron dónde aplicarse.

**Ajuste del suavizador** (cubo de 10 mm, vóxeles de 0,1 mm; tiempos con huecos / macizo):

| Suavizador | Iteraciones | Tiempo |
|---|---|---|
| Jacobi, 1 barrido | 172 / 34 | 19,4 s / 6,3 s |
| Jacobi, 2 barridos | 122 / 24 | 23,2 s / 7,6 s |
| Jacobi, 4 barridos | 88 / 17 | 28,0 s / 8,6 s |
| Ciclo W con Jacobi, 1 barrido | 166 / 28 | 24,5 s / 6,6 s |
| Chebyshev, grado 2, [λ/30, 1,1 λ] | 82 / 16 | 16,0 s / 5,6 s |
| **Chebyshev, grado 3, [λ/30, 1,1 λ]** | **61 / 11** | **16,8 s / 5,4 s** |
| Chebyshev, grado 4, [λ/100, 1,1 λ] | 46 / 11 | 15,9 s / 6,2 s |

El ciclo W no ayuda: lo que limita con huecos es el nivel fino, no los gruesos. Para el límite del suavizador, la
cota de Gershgorin da el doble que la estimación por potencias, así que se usa la de potencias con un margen de 10 %.

**Medido** (4 núcleos, tolerancia 10⁻⁸ en el voladizo y 10⁻⁶ en el cubo):

| Caso | Elementos | Jacobi | Multigrilla |
|---|---|---|---|
| Voladizo, vóxel de 1 mm | 640 | 180 it., 28 ms | 11 it., 28 ms (2 niveles) |
| Voladizo, 0,5 mm | 5120 | 360 it., 214 ms | 11 it., 92 ms (3 niveles) |
| Voladizo, 0,25 mm | 40 960 | 718 it., 2,8 s | 11 it., 0,53 s (4 niveles) |
| Voladizo, 0,125 mm | 327 680 | — | 12 it., 3,6 s (5 niveles) |
| Cubo macizo, 0,2 mm | 125 000 | 441 it., 4,7 s | 12 it., 1,4 s |
| Cubo macizo, 0,1 mm | 500 000 | 840 it., 36,8 s | 11 it., 5,4 s |
| Cubo como se imprimió (relleno 20 %), 0,1 mm | 309 014 | 2488 it., 71,5 s | 61 it., 16,9 s |

Los resultados coinciden con los de Jacobi: la misma flecha y, en el cubo, el mismo desplazamiento máximo y el mismo
factor de seguridad. Desglose con 327 680 elementos: armar la multigrilla 0,75 s, un producto con K 25 ms, un ciclo
0,2 s.

**Probado:** 187/187 pruebas (182 de Catch2 y 5 del programa) con gcc, clang y ASan+UBSan; clang compila sin
advertencias. Pruebas nuevas:
- la multigrilla da lo mismo que Jacobi (10⁻⁸) con diez veces menos iteraciones;
- al achicar el vóxel a la mitad, las iteraciones casi no cambian;
- dos corridas dan el resultado idéntico;
- una placa de un vóxel de espesor con lados impares y otra altura de fila: 3 niveles que no engruesan el espesor;
- un reticulado con agujeros: lo mismo que Jacobi;
- un bloque suelto: queda fuera, su desplazamiento es cero, el resto da igual que sin él, y una carga puesta solo
  sobre él se informa como no aplicada.

**No probado:**
- la multigrilla en la GPU;
- piezas de varios millones de vóxeles. Por cada vóxel fino, la multigrilla ocupa unos 180 bytes de matrices gruesas
  (precisión simple, solo los bloques de arriba) y 120 bytes de vectores del ciclo. Con 8 millones de vóxeles serían
  unos 2,4 GB, además de los vectores del gradiente conjugado.

**Sigue:**
- línea de comandos del análisis: material de la base por su nombre, cargas y apoyos en JSON, mapa del factor de
  seguridad;
- el núcleo de la GPU (producto con K y ciclo), probado contra la CPU;
- la multigrilla con huecos: duplicar los nodos gruesos por cada pedazo separado dentro de una celda.

## 2026-10-24 — A1, paso 5: la línea de comandos del análisis

**Pedido del dueño:** seguir con la línea de comandos del análisis.

**Hecho:**
- `tisma-engine analyze pieza.stl --config ajustes.json --case caso.json -o informe.json [--vtk mapa.vtk]`
  (`docs/ANALYSIS.md`). Código de salida nuevo: 5, el análisis no dio un resultado válido.
- El caso, en su propio JSON (`src/config/analysis_case.*`):
  - el material, por su registro o con constantes cambiadas;
  - el modelo, `as_printed` o `solid`;
  - la calidad: `normal`, 2 vóxeles por línea y una fila por capa; `high`, 4 por línea y 2 filas por capa;
  - los apoyos y las cargas, por caja o por cara de la pieza;
  - el solucionador.
  Como en los ajustes, los errores nombran la clave.
- El análisis completo (`src/fea/analysis.*`):
  - corta la pieza y arma los vóxeles;
  - toma las direcciones del laminado y saca los huecos del relleno;
  - resuelve;
  - resume: el más débil, el más débil de cada capa y los vóxeles por debajo de 1, 1,5 y 2;
  - avisa de apoyos que no tocan nada, piezas sueltas, cargas sin aplicar, falta de convergencia y falla.
- El informe JSON (`src/report/analysis_json.*`) y el mapa VTK para ParaView (`src/io/vtk.*`), escrito desde la
  descripción pública del formato. Se verificó como caja negra con el lector de VTK 9.7.1, instalado aparte solo para
  eso: dimensiones, planos, los siete campos y sus rangos coinciden.
- **Hallazgo y corrección:** el factor de seguridad de un vóxel no converge. En la arista de un empotramiento, en la
  placa de prueba, va 1,48 → 0,92 → 0,65 → 0,42 al achicar el vóxel de 0,2 a 0,025 mm (el desplazamiento sí
  converge). Ahora el factor se calcula sobre la tensión promediada:
  - en un ancho de línea en el plano y en las filas de la capa del vóxel (método de volumen de la teoría de
    distancias críticas, Taylor 2007);
  - es la escala a la que valen las resistencias medidas;
  - el pico de un vóxel queda aparte, como dato.
  Una primera ventana centrada en cada vóxel también en altura tenía un sesgo: en la capa de abajo se recortaba contra
  la superficie y se acercaba a ella al afinar. Promediar sobre la capa entera lo corrige.

**Convergencia** (placa de 12 × 4 × 2 mm empotrada, 5 N en la punta; tabla completa en `docs/ANALYSIS.md`):

| Placa | Vóxel de 0,2 mm | 0,1 mm | 0,05 mm |
|---|---|---|---|
| Maciza, factor promediado | 1,664 | 1,573 | 1,527 |
| Maciza, lejos del empotramiento | 1,734 | 1,721 | 1,716 |
| Como se imprime, factor promediado | 1,615 | 1,499 | 1,380 |
| Pico de un vóxel (como se imprime) | 1,476 | 0,920 | 0,649 |

La pieza maciza converge. La impresa todavía cambia un 8 % por refinamiento, porque la forma de los cordones en
vóxeles cambia con la resolución. Queda en `PLAN.md`: estimar ese error con dos calidades y representar mejor el
contacto entre cordones.

**Medido** (4 núcleos; cubo de 10 mm de PLA Basic de Bambu Lab, base fija, 100 N de corte arriba, comando completo):

| Calidad | Vóxeles analizados | Iteraciones | Tiempo | Factor mín. | Pico de un vóxel | Mapa VTK |
|---|---|---|---|---|---|---|
| `normal` (0,2 mm, 1 fila) | 77 258 | 58 | 4,3 s | 2,10 | 1,94 | 3,3 MB |
| `high` (0,1 mm, 2 filas) | 618 028 | 54 | 29,5 s | 1,98 | 1,28 | 26 MB |

**Probado:** 196/196 pruebas (190 de Catch2 y 6 del programa) con gcc, clang y ASan+UBSan. Pruebas nuevas:
- el lector del caso: valores por defecto, todas las claves y 16 errores con su clave;
- la resolución según la calidad;
- una columna maciza traccionada: tensión y factor exactos (10⁻⁶), el informe JSON y el mapa VTK byte a byte en su
  estructura;
- una placa como se imprime: huecos, direcciones y el más débil junto al empotramiento;
- los avisos: apoyo que no toca, carga que no toca, falla y demasiados vóxeles;
- la CLI: el informe y el mapa escritos, errores de argumentos, caso mal escrito y salida 5 con el informe igual
  escrito;
- `cli_analyze_writes_report` en CTest.

**No probado:** una pieza real rota contra la predicción.

**Sigue:**
- estimar el error de resolución con dos calidades;
- el núcleo de la GPU;
- el criterio de A1, la placa con agujero.

## 2026-10-24 — A1, paso 6: lo que la interfaz necesita del motor

**Decisión del dueño:** la interfaz del análisis va dentro de Tisma Slicer, reusando lo que ya tiene, y tiene que
mostrar la cantidad de vóxeles que componen la pieza (más vóxeles, más calidad). La construye la sesión principal de
Tisma, que llama al motor como caja negra: por la sala limpia, este equipo no lee ese código.

**Hecho:**
- `--estimate`: corta y arma los vóxeles sin resolver. Informa cuántos vóxeles tendría el análisis, la grilla, la
  memoria pico y el tiempo aproximado, y escribe un JSON con eso.
- `--progress`: líneas `progress: <etapa> <fracción>` (`slicing`, `voxels`, `solving`, `summary`, `done`). La
  fracción no retrocede; en la etapa `solving` avanza con el residuo en escala logarítmica.
- `--mesh voxeles.ply`: la superficie de los vóxeles analizados como malla PLY binaria (`src/io/ply.*`):
  - colores por bandas del factor de seguridad (rojo, naranja, amarillo, verde y gris sin carga);
  - el factor y el modo de falla por cara;
  - la cantidad de vóxeles en la cabecera.
  Se verificó con el lector de PLY de VTK: cantidad de puntos y caras, límites, colores y las 14 948 caras exteriores
  del cubo, todas mirando hacia afuera.
- `docs/ANALYSIS.md`: las tres opciones, los recursos medidos y una sección «Para la interfaz» con lo que hay que
  mostrar y de dónde sale cada dato.

**Medido** (cubo de 10 mm, 4 núcleos): la estimación contra la corrida real.

| Caso | Memoria real / estimada | Tiempo real / estimado |
|---|---|---|
| `as_printed`, `normal` | 123 / 124 MB | 4,7 / 4,4 s |
| `as_printed`, `high` | 591 / 640 MB | 31,2 / 35,2 s |
| `solid`, `normal` | 75 / 78 MB | 1,9 / 1,8 s |
| `solid`, `high` | 539 / 550 MB | 15,8 / 14,3 s |

**Probado:** 198/198 pruebas (192 de Catch2 y 6 del programa) con gcc, clang y ASan+UBSan. Pruebas nuevas:
- la estimación (vóxeles y memoria);
- el avance: etapas en orden, fracción que no retrocede, de 0 a 1;
- la malla del bloque de 20 × 20 × 50: 4800 caras, 19 200 vértices y el tamaño exacto del cuerpo binario;
- las bandas de color;
- la CLI con `--estimate`, `--progress` y `--mesh`.

**No probado:** la interfaz misma, que es de Tisma Slicer. El tiempo estimado supone las iteraciones típicas y la
máquina de referencia.

**Sigue:**
- responder a la sesión principal lo que necesite para la interfaz;
- estimar el error de resolución con dos calidades;
- el núcleo de la GPU.

## 2026-10-24 — A1, paso 7: estimación del error de resolución

**Pedido del dueño:** seguir con la estimación del error de resolución.

**Hecho:**
- `resolution_check` (activado por defecto): el caso se repite con vóxeles del doble de ancho, y la mitad de filas
  por capa si hay dos o más. El informe trae, en `resolution`:
  - la resolución gruesa y sus resultados;
  - el cambio relativo del factor mínimo;
  - `extrapolated_min_safety`, la extrapolación de Richardson de primer orden (2 S_fino − S_grueso) cuando el factor
    baja al afinar.
- Aviso `resolution_sensitive` si el factor cambia más de un 5 %.
- No se hace en `as_printed` con calidad `normal`, porque los vóxeles de 0,4 mm no representan los huecos del
  relleno; el informe dice por qué.
- El avance tiene una etapa `resolution` al final. La estimación de tiempo suma el chequeo.
- Corregido: en el paso anterior se había perdido el título de la sección «Qué tan confiable es el número» de
  `docs/ANALYSIS.md`.

**Validación** (placa de 12 × 4 × 2 mm empotrada, 5 N en la punta; se agregó la corrida con vóxeles de 0,025 mm, más
de 5,5 millones de vóxeles):

| Placa | 0,2 mm | 0,1 mm | 0,05 mm | 0,025 mm | Extrapolado desde `high` (0,1 contra 0,2) |
|---|---|---|---|---|---|
| Maciza | 1,664 | 1,573 | 1,527 | 1,504 | 1,482 (límite de la serie: 1,481) |
| Como se imprime | 1,615 | 1,499 | 1,380 | 1,381 | 1,383 |

- **Pieza maciza:** converge en primer orden; la diferencia se reduce a la mitad en cada paso.
- **Pieza como se imprime:** se estabiliza con 8 vóxeles por ancho de línea.
- **Extrapolación:** la de calidad `high` acierta dentro de un 0,2 % en los dos casos.

**Medido:** el cubo de 10 mm en calidad `high` tarda 33,4 s en total, unos 4 s más que sin el chequeo. Factor
mínimo 1,983, con vóxeles gruesos 2,104, extrapolado 1,863; aparece el aviso (6,1 %). Las corridas con vóxeles de
0,025 mm tardaron 119 s (maciza, 6,1 millones de vóxeles) y 183 s (como se imprime, 5,5 millones).

**Probado:** 199/199 pruebas (193 de Catch2 y 6 del programa) con gcc, clang y ASan+UBSan. Pruebas nuevas:
- tensión uniforme: cambio nulo y extrapolado igual al calculado;
- placa maciza con vóxeles gruesos: el cambio, la fórmula extrapolada y el aviso según el umbral;
- calidad `normal` como se imprime: no se hace y dice por qué;
- desactivado;
- la clave del caso y su error.

**Sigue:**
- el criterio de A1, la placa con agujero contra la solución de Kirsch, que amplía la evidencia de la extrapolación;
- el núcleo de la GPU.

## 2026-10-24 — pedidos de la sesión principal para la interfaz del análisis

La sesión principal de Tisma, que construye la interfaz dentro de Tisma Slicer, respondió:
- el PLY le sirve tal cual;
- el progreso con `--progress` alcanza;
- el adaptador de ajustes lo escribe ella.

Además pidió:

**Hecho:**
- Malla PLY: comentarios legibles por máquina en la cabecera (`units`, `voxels_analysed`, `grid_voxels`,
  `voxel_size_mm`, `grid_origin_mm`, `rows_per_layer`, `z_planes_mm`), y una tabla en `docs/ANALYSIS.md` con cada
  propiedad, su tipo y su unidad.
- Coordenadas: ya estaban en el sistema del STL, porque el motor no mueve las piezas. Ahora hay una prueba con una
  pieza fuera del origen y queda escrito en `docs/ANALYSIS.md`.
- Cada archivo de salida (G-code, avisos, informe, estimación, VTK y PLY) se escribe como `<archivo>.part` y se
  renombra al terminar: si la interfaz corta el proceso, no queda un archivo a medias con el nombre pedido.
- Los códigos de aviso del informe son estables; el texto puede cambiar.
- `docs/CONFIG.md`:
  - una clave o un grupo desconocido es un error con código de salida 2 (ya lo era);
  - los nombres son estables y un cambio se anuncia.

**Probado:** 201/201 pruebas (195 de Catch2 y 6 del programa) con gcc, clang y ASan+UBSan. Pruebas nuevas:
- la cabecera del PLY con sus valores;
- una pieza desplazada a (10, −5, 3) cuya grilla y cuya malla quedan exactamente en sus coordenadas;
- ningún `.part` queda después de escribir;
- una salida imposible de escribir no deja nada.

**Además:** la sesión principal subió los logos e íconos de Tisma en el PR #6 (rama `assets/brand`), solo imágenes.
El motor todavía no los necesita: no tiene interfaz propia.

## 2026-10-24 — Linux se compila dentro de la sesión

**Decisión del dueño:** compilar Linux de forma interna, para no perder tiempo esperando la CI.

**Hecho:**
- `tools/ci_linux.sh` corre los mismos pasos que los trabajos de Linux de la CI:
  - la revisión del SPIR-V generado;
  - gcc y clang en Release y gcc con ASan + UBSan en Debug;
  - todas las pruebas exigiendo un dispositivo Vulkan (lavapipe acá).
  Reusa los directorios de compilación y corre las pruebas en paralelo.
- `.github/workflows/ci.yml`: en cada envío solo corre Windows. Los trabajos de Linux quedan para lanzarlos a mano.
- `CLAUDE.md` y `PLAN.md` dicen que hay que correr `tools/ci_linux.sh` antes de cada envío.

**Medido:** `tools/ci_linux.sh` tarda 1 min 38 s con todo compilado y pasa 201/201 en las tres compilaciones, con las
pruebas de GPU en lavapipe. La CI de Linux en GitHub tardaba unos 5 minutos por envío.

## 2026-10-24 — exportación de G-code verificada

**Pedido del dueño:** asegurarse de que la exportación del G-code se haga correctamente y sin fallos (junto con la
interfaz, que viene después).

**Hecho:**
- Verificador propio del G-code (`src/gcode/check.*`, `docs/GCODE_CHECK.md`). Comprueba:
  - que el archivo esté completo y se pueda leer;
  - números finitos;
  - lugar y velocidad de cada movimiento;
  - capas que suben y extruyen a su altura;
  - filamento proporcional a cada línea;
  - retracciones completas y deshechas;
  - el ventilador.

  El G-code del usuario no se juzga, solo se sigue su estado.
- Exportación verificada (`src/gcode/export.*`):
  - se escribe en `.part`, se vuelve a leer desde el disco y se verifica;
  - solo entonces se renombra;
  - si no pasa, queda como `.rejected` y `slice` termina con el código de salida nuevo, 6.

  `slice` informa `gcode_check: ok (…)` con líneas, capas y filamento.
- **Dos errores reales encontrados y corregidos:**
  - una pieza toda más fina que las líneas daba un G-code sin extrusiones;
  - con una cama justa, la falda quedaba fuera de la cama y el G-code se escribía igual.

  Los dos ahora se rechazan antes de escribir, con un mensaje claro (código 4).
- Los íconos y logos de Tisma (PR #6) se fusionaron en `dev` para la interfaz, con su procedencia anotada.

**Probado:** 206/206 pruebas (200 de Catch2 y 6 del programa) con `tools/ci_linux.sh` (gcc, clang, ASan+UBSan, GPU con
lavapipe). Pruebas nuevas:
- el cubo limpio;
- nueve daños detectados, cada uno con su mensaje;
- G-code de usuario libre sin falsos avisos;
- matriz fija de 4 piezas × 20 variantes de ajustes, sin un problema;
- exportación buena, rechazada y sin carpeta;
- el modelo sin nada que extruir;
- la cama justa con y sin falda.

Además, fuera del repositorio, una prueba aleatoria de 5300 combinaciones de ajustes y piezas (con esfera, cilindro y
paredes finas): cero G-code inválidos, 7 piezas rechazadas como no imprimibles.

**Medido:** esfera de 50 mm, 159 200 triángulos y 297 115 líneas (8,4 MB). Laminar tarda 521 ms, escribir 278 ms y
verificar 72 ms.

**Sigue:** versiones y CHANGELOG, documentación para colaboradores, después la interfaz.

## 2026-10-24 — versiones, releases y documentación para colaboradores

**Pedidos del dueño:**
- categorizar las versiones correctamente;
- documentación detallada de cómo funciona el código, para futuros aportes, «lo más prolijo posible»;
- releases cada cierto número de mejoras grandes o relevantes.

**Hecho:**
- **Versionado semántico** con una sola fuente, el archivo `VERSION` (`0.6.0-alpha.1`):
  - CMake toma los números;
  - `--version` y la cabecera del G-code muestran la versión completa;
  - CMake se vuelve a configurar solo cuando cambia.
- **`CHANGELOG.md`** en formato Keep a Changelog, por categorías (Agregado, Cambiado, Corregido…):
  - las fases F0 a F4 quedan como hitos internos 0.1.0 a 0.5.0;
  - 0.6.0-alpha.1 es la primera publicada.
- **`docs/VERSIONING.md`**:
  - qué sube MENOR y qué sube PARCHE antes de 1.0.0;
  - las etiquetas `-alpha`, `-beta` y `-rc`;
  - cuándo hay release y los pasos para publicarla;
  - los formatos con versión propia.
- **`.github/workflows/release.yml`**: con una etiqueta `v<versión>` compila y prueba en Windows y Linux, empaqueta el
  programa con la licencia y la documentación de uso, y publica la release con las notas de su sección del CHANGELOG
  (pre-release si el número tiene etiqueta). Antes de compilar, `tools/release_notes.py` comprueba que la etiqueta
  coincida con `VERSION` y que el CHANGELOG tenga la sección. CTest corre la misma comprobación
  (`release_notes_check`).
- **`docs/ARCHITECTURE.md`**: cómo funciona el código:
  - las carpetas;
  - el camino del STL al G-code, con cada función y su módulo;
  - los tipos de datos y sus unidades;
  - las reglas comunes (determinismo, CPU como referencia, datos contiguos, topología antes que geometría, mensajes
    claros);
  - cada módulo, incluido el análisis.
- **`docs/DEVELOPMENT.md`**: compilar, opciones, probar, convenciones del código, recetas (agregar un ajuste, un
  módulo, una biblioteca, un material, publicar) y la sala limpia.
- `CONTRIBUTING.md` y `README.md` enlazan las guías. La regla de no aceptar aportes de terceros hasta que exista una
  cesión de derechos sigue igual.

**Probado:** `tools/ci_linux.sh` pasa (207/207 en las tres compilaciones, con la comprobación nueva del CHANGELOG).
`tools/release_notes.py` da las notas de `v0.6.0-alpha.1` y rechaza una etiqueta que no coincide. Los dos flujos de
GitHub son YAML válido. El flujo de releases solo se prueba de verdad al subir la etiqueta.

## 2026-10-24 — bloques de mantenimiento cada 8 horas

**Decisión del dueño:** cada 8 horas, dedicar tiempo a optimizar, buscar errores y resolver lo pendiente, para pulir
el motor y su estabilidad.

**Hecho:**
- La regla está en `CLAUDE.md` («Work blocks») y en `docs/DEVELOPMENT.md` («Mantenimiento»):
  - errores (comprobaciones, búsqueda aleatoria, «No probado», puntos débiles);
  - optimización medida;
  - robustez;
  - pendientes chicos;
  - registro, y release de corrección si hace falta.
- Una tarea programada dispara el bloque en esta sesión cada 8 horas (a los :40). Si otra sesión del motor está
  trabajando sobre `dev`, el bloque no se hace, para que nunca trabajen dos a la vez.
- La búsqueda aleatoria de G-code inválido pasó al repositorio (`tests/fuzz_gcode.cpp`, programa `tisma_fuzz_gcode
  [corridas] [semilla]`), así los bloques de mantenimiento la corren con semillas nuevas. CTest corre 12 casos
  (`fuzz_gcode_smoke`) para que siga compilando y andando.

## 2026-10-24 — primera release y la interfaz `tisma`

**Release:** `v0.6.0-alpha.1` está publicada en GitHub como pre-release, con los paquetes de Windows y Linux. La
sesión no puede enviar etiquetas (el proxy de git responde 403). Por eso el flujo de releases ahora corre con un
envío a `dev` o `main` que cambia `VERSION` (o el flujo), y crea la etiqueta él mismo si no existe. La primera se
publicó así, desde el código de 8eeee3e.

**Hecho (pedido del dueño: la interfaz con los íconos de Tisma, y que exportar el G-code ande bien):**
- **Bibliotecas** con las licencias leídas en la versión exacta: GLFW 3.4 (Zlib), Dear ImGui 1.92.9b (MIT; Roboto
  Medium, Apache-2.0) y nanosvg (Zlib). El cargador de OpenGL que trae ImGui es de dominio público (Unlicense, fuera
  de la lista), así que se escribió uno propio desde la especificación de OpenGL 3.3. El explorador de archivos es
  propio: GTK y Qt son LGPL.
- **En el motor, probado sin ventana:**
  - `settings_json` escribe los ajustes y `parse_settings` los lee iguales (prueba con cada clave cambiada);
  - `read_gcode_preview` lee el G-code en movimientos con su tipo, ancho, alto, avance, capa y el tiempo estimado.
    Con el G-code del motor coincide con el verificador en las extrusiones, el filamento y el recorrido;
  - `analysis_grid_voxels` cuenta la grilla del análisis sin armarla.
- **`src/ui/`** (`tisma_ui`): cámara, datos de dibujo, íconos, PNG, textos en dos idiomas, carpetas, laminado en
  segundo plano y estimación de vóxeles, con 10 pruebas.
- **`apps/tisma/`:**
  - ventana con la barra superior (logo, pestañas, Abrir, Laminar, Exportar, Más), el panel de ajustes con los
    íconos de Tisma, la vista 3D y la barra de estado;
  - la vista previa dibuja cada extrusión como un prisma armado por instancia en la GPU;
  - la pestaña Análisis muestra los vóxeles de la pieza en calidad normal y alta.
- **Prueba de punta a punta** con `xvfb-run` (`ui_slice_export_screenshot`): abre el cubo, lamina, exporta el G-code
  verificado y dibuja la vista previa; falla si algo de eso falla.

**Medido:**
- Torre hexagonal retorcida de 50 × 43 × 40 mm: laminado, G-code y verificación en 0,08 a 0,12 s; 47 326 líneas
  verificadas, 200 capas, 1 h 6 min estimados.
- Esa torre en el análisis, calidad normal: 1 971 052 vóxeles de pieza en una grilla de 10,8 millones, que pasa el
  máximo de 8 millones. O sea, una pieza de 5 cm ya no entra en el análisis en calidad normal. Es un límite a revisar
  en A1.

**No probado:**
- La interfaz en Windows: la CI la compila, pero nadie la abrió.
- El ratón y los clics: se probó con las capturas automáticas, no a mano.
- En pantallas con escala mayor a 1, el tamaño de todo sale de la escala del monitor; no se probó en una.

**Sigue:**
- Iluminación mejorada, oclusión ambiental, ray tracing activable y suavizado de bordes (pedido del dueño).
- Después, el análisis completo desde la interfaz.

## 2026-10-24 — luz, oclusión ambiental, ray tracing y suavizado en la vista 3D

**Pedidos del dueño:** «oclusión ambiental y mejor iluminación, con la posibilidad de activar ray tracing», y
«antialiasing para mejorar la calidad visual».

**Hecho:**
- **El render se rehízo fuera de pantalla** (`apps/tisma/renderer.*`):
  1. normales y profundidad;
  2. visibilidad de la luz;
  3. la escena iluminada con MSAA;
  4. resolución y FXAA hacia la ventana.
- **Luz por píxel en color lineal:**
  - hemisférica de cielo y suelo;
  - luz principal y de relleno, que siguen a la cámara;
  - Blinn-Phong con Fresnel de Schlick;
  - contorno;
  - curva de tono.
- **Oclusión ambiental en pantalla**, según McGuire et al. 2011, con desenfoque bilateral. Se recalcula solo si cambia
  la imagen.
- **Ray tracing activable:**
  - una jerarquía de volúmenes propia (SAH con cubetas, según Wald 2007) en `src/ui/raytrace.*`: es la referencia en
    CPU, probada contra la fuerza bruta en 4000 rayos al azar;
  - viaja a la GPU como texturas, y un shader la recorre igual que la CPU (Möller–Trumbore, losas de Kay–Kajiya);
  - sombras suaves y oclusión con rayos, que se promedian cuadro a cuadro hasta 256 muestras;
  - la escena es el modelo con la cama, o los cordones de la vista previa (como cajas) con la cama, filtrados por las
    capas y los tipos que se muestran.
- **Suavizado:** MSAA 1×, 2×, 4× u 8× y FXAA (versión propia según Lottes 2009).
- **Menú «Imagen»** en la barra de vistas: modo, intensidad de la oclusión, MSAA y FXAA. Se recuerda entre usos.
- **Opciones para las capturas:** `--render`, `--msaa`, `--fxaa`, `--ray-samples` y `--ray-check`.
- **La ventana espera eventos cuando nada se mueve.**

**Probado y medido (llvmpipe, sin GPU):**
- Comparación de la GPU con la CPU (`ui_raytrace_matches_cpu` y a mano), con los mismos rayos de sombra desde las
  mismas superficies:

  | Escena | Píxeles | En sombra | Coinciden |
  |---|---|---|---|
  | Torre, preparar | 28 845 | 1112 | 99,997 % |
  | Torre, vista previa | 28 845 | 613 | 100 % |
  | Cubo de las pruebas | 18 781 | 51 | 100 % |

- Ray tracing de la torre en 1000 × 640: 24 muestras en 4 s (malla) y en 23 s (47 mil cordones), en la CPU.
- Bordes con y sin suavizado comparados en un recorte ampliado: sin suavizado, escalones; con MSAA 4× y FXAA, lisos.

**No probado:** en una GPU real (solo llvmpipe), y la velocidad del ray tracing con modelos de millones de
triángulos.

**Sigue:** calibraciones y opciones de guardar (pedidos del dueño).

## 2026-10-24 — calibraciones, primera parte

**Pedidos del dueño:**
- una opción de calibraciones: compensación geométrica desde las medidas de una pieza, torres de temperatura y de
  retracción, pruebas de flujo y, más adelante, pruebas de laminado no planar (a definir);
- «todas las calibraciones estándar típicas, y analizar calibraciones nuevas que sean muy prácticas»;
- opciones de guardar.

**Hecho:**
- **Ajustes nuevos** (`docs/CONFIG.md`):
  - `compensation`: escala por eje y `hole_offset`;
  - `extrusion.flow`;
  - la lista `changes`: temperatura, cama, ventilador, retracción, flujo, velocidad o G-code desde una altura.

  Se leen con sus rangos, se escriben y vuelven iguales.
- **En el motor:**
  - la escala se aplica antes de laminar, y las comprobaciones de la cama la ven;
  - los agujeros crecen por pieza (`offset_holes`);
  - el escritor aplica los cambios capa por capa (una retracción que cruza un cambio se deshace con su propio largo);
  - el verificador juzga cada capa con su flujo y su retracción;
  - el análisis estructural usa la pieza sin compensar.
- **Corregido, encontrado por el verificador:** un viaje que seguía al G-code de un cambio quedaba bajo
  `;TYPE:Custom`, así que su retracción no se juzgaba y la vuelta parecía sobrar. Ahora el escritor vuelve al tipo
  anterior.
- **`src/calibration/`:** el bloque de dimensiones, las torres de temperatura (con voladizo a 45°), retracción (dos
  columnas), flujo y velocidad, y `compensation_from`.
- **Pestaña Calibración** en la interfaz:
  - cargar cada prueba;
  - medidas y compensación actual y nueva;
  - segmentos de la torre;
  - el valor del segmento en la leyenda de la vista previa.
- **`docs/CALIBRATION.md`:** el catálogo, con lo hecho, las estándar que siguen y cinco calibraciones nuevas
  propuestas (mínimos cuadrados, ancho de línea variable, adherencia entre capas con el análisis, contracción por
  material, no planar).

**Probado:** `tests/test_calibration.cpp`:
- el volumen del bloque (0,02 %) y el área de su corte (0,2 %);
- la escala alrededor del centro y desde abajo;
- los agujeros: ±0,25 mm en el radio cambian el área en lo esperado (2 %);
- la compensación desde medidas;
- cada torre: sus `M104` en orden y en su capa; el verificador acepta la retracción, el flujo y la velocidad por
  altura, y rechaza el mismo G-code si no conoce los cambios.

Capturas: la pestaña con la torre de temperatura y la vista previa con «Segmento: 215 °C».

**No probado:** impreso en una máquina real (necesita al dueño).

**Sigue:**
- opciones de guardar (perfiles con nombre y proyectos);
- las calibraciones estándar siguientes (velocidad de retracción y avance de presión primero).


## 2026-10-24 — calibraciones automáticas y guiadas; la torre de temperatura nueva

**Pedidos del dueño:**
- «Las creaciones tienen que ser automáticas, no que trate de configurarlas»: cómodo, intuitivo y fácil de usar;
- la torre de temperatura con una cresta de ángulo variable, una zona para probar la resistencia haciendo fuerza,
  detalle en letras y figuras, y una zona plana para probar el futuro Arc Overhang.

**Hecho:**
- **`automatic_calibration`:** cada prueba con su rango elegido desde los ajustes (`docs/CALIBRATION.md`, «Rangos
  automáticos»): la temperatura en uso ± 20 °C, la retracción según el extrusor (corta o larga), el flujo ± 10 %, la
  velocidad de 100 a 300 %.
- **`apply_calibration`:** pasa el valor elegido a su ajuste y quita los cambios por altura.
- **La torre de temperatura nueva**, segmentos de 10 mm:
  - cresta de 4 mm cuyo ángulo crece de 30° a 72° a lo largo;
  - estante plano de 8 × 8 mm sin nada debajo (zona de prueba del Arc Overhang);
  - perno de 2,4 mm sobre una ménsula a 45°, para empujar con el dedo;
  - el número, un rombo y un anillo en relieve.
- **Valores en relieve en todas las torres**, con una tipografía propia de siete segmentos (`label_strokes`,
  `add_label`).
- **Pestaña Calibración guiada:** una tarjeta por prueba con **Crear**; la pieza se arma y se lamina sola, y la tarjeta
  sigue en tres pasos (exportar, qué mirar, elegir). Al elegir un segmento, la vista muestra la torre laminada hasta
  él; **Usar** aplica el valor. El rango se puede ajustar, opcional.
- **Corregido en la interfaz:**
  - el encuadre se hacía antes de conocer el tamaño de la vista (la torre salía fuera de cuadro en las capturas):
    ahora espera al primer dibujo;
  - al entrar o salir de la pestaña Calibración la pieza se corre al lado del panel y vuelve;
  - `--screenshot` se queda en la pestaña pedida después de laminar.
- **Arc Overhang** anotado en `PLAN.md` como función planeada, en sala limpia: desde la descripción pública de la
  idea, sin leer código GPL.

**Probado:**
- `tests/test_calibration.cpp`, 6 casos y 94 comprobaciones:
  - las zonas de la torre de temperatura, capa por capa: el perno a la altura de su segmento, la cresta y el estante
    en el ancho esperado;
  - la tipografía;
  - cada calibración automática se lamina y pasa el verificador, y usar un valor lo deja en su ajuste.
- Capturas con `--calibration temperature`: la pestaña guiada con la torre a la derecha del panel, y la torre con ray
  tracing en Preparar.

**Medido** (torre de temperatura automática, ajustes por defecto, interfaz bajo xvfb):
- 9 segmentos de 230 a 190 °C;
- 3 444 triángulos, 44,3 × 16,6 × 90,0 mm;
- laminada en 3,1 a 3,4 s;
- 653 999 líneas de G-code, verificadas sin problemas;
- 1 h 26 min de impresión estimada.

**No probado:** impresa en una máquina real (necesita al dueño); en particular, si el perno y el estante dan
diferencias claras entre temperaturas.

**Sigue:**
- opciones de guardar (perfiles con nombre y proyectos);
- las calibraciones estándar siguientes (velocidad de retracción y avance de presión primero).

## 2026-10-24 — opciones de guardar: perfiles con nombre y proyectos

**Pedido del dueño:** «opciones de guardar».

**Hecho:**
- **Perfiles con nombre** (`docs/PROFILES.md`, `src/config/profiles.*`):
  - cada ajuste pertenece a la impresora, al filamento o a la impresión; los cambios por altura, a ninguno;
  - las variables del G-code se reparten por nombre (las de temperatura, al filamento);
  - la compensación de dimensiones va con el filamento, por la contracción de cada material;
  - elegir un perfil cambia solo sus ajustes (`with_profile`);
  - archivo propio (`tisma-profile`, versión 1), uno por perfil en `profiles/<tipo>/` al lado de `interface.json`.
- **Perfiles de fábrica** (`src/ui/presets.*`): Marlin 256 × 256 mm; PLA, PETG, ABS, TPU; capas de 0,12, 0,20 y 0,28 mm.
  El primero de cada tipo es lo que trae la interfaz.
- **En el panel:** arriba de cada sección, la lista de perfiles con un asterisco si los ajustes cambiaron, guardar, y
  ⋮ con guardar como y eliminar. El flujo pasó a editarse en Filamento y la retracción a Impresora.
- **Proyectos `.tisma`** (`docs/PROJECT.md`, `src/project/`): firma, versión y partes con etiqueta; el JSON con los
  ajustes, los perfiles, las piezas y la calibración, y cada malla como STL binario (`stl_binary`, nuevo). Ctrl+S y
  Ctrl+Mayús+S; **Abrir** también abre proyectos. Una torre de calibración vuelve a su pestaña, laminada.
- **Línea de comandos de la interfaz:** `--profile tipo=nombre` y `--save-project`.

**Probado:**
- `tests/test_profiles.cpp` (4 casos): cada ajuste tiene un solo tipo; elegir un perfil cambia solo lo suyo y los tres
  juntos dan todo de vuelta; el archivo vuelve igual y los errores nombran la clave; guardar, reemplazar, listar,
  eliminar, nombres que chocan y archivos rotos.
- `tests/test_project.cpp` (4 casos): el STL binario sigue el formato y vuelve exacto; un proyecto con dos piezas,
  cambios por altura, perfiles y calibración vuelve igual, también por archivo; una torre vuelve con su malla y sus
  segmentos; los archivos dañados o más nuevos se rechazan con su motivo.
- `tests/test_ui.cpp`: cada perfil de fábrica vuelve igual de su archivo y lamina el cubo a G-code verificado.
- CTest con la interfaz: el cubo con PETG y 0.28 mm se exporta y se guarda como proyecto; el proyecto se abre y se
  exporta de nuevo; los dos G-code son idénticos byte a byte (`ui_project_same_gcode`).
- Capturas: el panel con las filas de perfil; la torre de temperatura guardada y abierta de nuevo, en su paso 3.

**Medido:** el proyecto del cubo ocupa 3,7 kB; el de la torre de temperatura (3 444 triángulos), 177 kB, casi todo la
malla (50 bytes por triángulo).

**No probado:** guardar y cargar perfiles a mano en la ventana (las pruebas usan la línea de comandos y las funciones
del motor); en Windows, los nombres de perfil con caracteres que no son ASCII (la CI de Windows corre las pruebas del
motor, que los usan).

**Sigue:**
- avisar antes de cerrar o abrir otro archivo si el proyecto tiene cambios sin guardar;
- las calibraciones estándar siguientes (velocidad de retracción y avance de presión primero).

## 2026-10-24 — mantenimiento

**Revisado:**
- `tools/ci_linux.sh` (251/251 en gcc, clang y ASan + UBSan antes de los arreglos);
- la búsqueda aleatoria de G-code con semillas nuevas: 2000 corridas con una semilla al azar y 2000 con
  1792863862 en Release, 300 con 1792863864 con ASan + UBSan. 0 fallas (8 piezas rechazadas como no imprimibles, que
  es lo esperado);
- los cambios recientes (calibraciones, perfiles, proyectos, la interfaz) buscando casos borde;
- el banco: la esfera y el análisis del cubo.

**Encontrado y corregido (cada uno con su prueba):**
- **Las alturas del G-code salían en coordenadas de la malla.** Una pieza que no empezaba en Z = 0 se laminaba a esa
  altura: debajo de 0, el verificador rechazaba el G-code (la esfera del banco, centrada en el origen, terminaba con
  código 6, «move to Z-49.6 is out of 0 to 50»); arriba de 0, la primera capa habría ido en el aire sin que nada
  avisara. Ahora el escritor cuenta las alturas desde la base de la pieza (también las de `changes` y las de los
  avisos) y el verificador usa el alto de la pieza. Prueba: la mesa corrida −50, −3,5 y +12,25 mm da el mismo G-code
  que apoyada en 0, con su cambio de temperatura en el mismo lugar.
- **La interfaz mostraba un laminado viejo como nuevo.** Si mientras laminaba se abría otra pieza (o un G-code), el
  resultado de la anterior aparecía como el de la nueva y se podía exportar; si cambiaban los ajustes, se mostraba
  como al día. Ahora cada trabajo en segundo plano guarda de qué pieza y ajustes salió (`Revision`): el de otra pieza
  se descarta y el de otros ajustes queda marcado como desactualizado (en las capturas se vuelve a laminar). Lo mismo
  para la estimación de vóxeles, que además se rehace sola en la pestaña Análisis cuando cambian los ajustes. Prueba:
  `ui: a background result is matched against what changed since it started`.
- **Nombres de perfil reservados en Windows.** «con» ya se evitaba, pero «NUL.txt» o «Lpt9 .x» también son
  dispositivos en Windows; ahora se mira el nombre hasta el primer punto. Prueba en `test_profiles.cpp`.
- **Menores:** el botón ⋮ de los perfiles decía «Guardar el perfil…» al pasar el ratón (ahora «Más»); una suma de
  `bool` en una prueba.

**Medido** (4 núcleos, Release, CPU):
- Esfera de banco de 100 mm (`bench/gen_sphere.py --segments 400`): 319 200 triángulos, lectura 20 ms, 500 capas.
  Laminado completo 0,88 a 0,93 s; verificación 0,20 a 0,25 s; 884 537 líneas de G-code. Antes del arreglo no
  llegaba a escribirse.
- Análisis del cubo de prueba (5000 vóxeles de 1 mm): preparación 0,5 ms, solución 141 a 151 ms (22 iteraciones de
  multigrilla, 3 niveles), 0,23 a 0,24 s en total con la estimación de resolución.
- La fase más lenta del laminado de la esfera son las paredes y el relleno de las 500 capas (unos 0,8 s de 0,9). No
  se tocó: no hubo tiempo de medir una mejora de verdad en este bloque.

**No probado:** el arreglo de la interfaz a mano (abrir otra pieza mientras lamina); la prueba cubre la decisión, no
la ventana.

## 2026-10-24 — calibraciones estándar, segunda tanda

**Pedido del dueño:** «seguí con las calibraciones estándar siguientes».

**Hecho:**
- **Primera capa** (`first_layer_test`): cinco cuadrados de 30 × 30 mm y una capa, en el medio y a 30 mm de cada
  esquina de la cama (en una cama chica, solo el del medio). La tarjeta explica qué significa cada aspecto (líneas
  separadas, rugosa, un cuadrado distinto) y que la altura se corrige en la impresora.
- **Velocidad de retracción** (`retraction_speed_tower`): las dos columnas de la retracción, de 20 a 60 mm/s.
- **Avance de presión:**
  - ajuste nuevo `extrusion.pressure_advance` y la clave `pressure_advance` en `changes`;
  - el escritor pone el comando del dialecto (`M900 K`, `SET_PRESSURE_ADVANCE`, `M572 D0 S`) al empezar y en cada
    cambio (`pressure_advance_gcode`);
  - la torre (`pressure_advance_tower`): un tubo cuadrado de esquinas vivas con una ranura entre segmentos; de 0 a
    0,08 con extrusor directo, de 0 a 0,8 con Bowden.
- **Tolerancias de encastre** (`fit_test`): siete bloques, cada uno con un perno de 8 mm impreso dentro de su agujero,
  con 0,1 a 0,5 mm de juego. Al elegir uno, la tarjeta dice cuánto juego dejar en los diseños.
- **Pasos del extrusor:** sin pieza. Exporta un G-code que extruye 100 mm despacio, pide el valor actual y lo que
  quedó hasta la marca de 120 mm, y da el nuevo con su comando (`M92 E` y `M500`, o `rotation_distance` en Klipper),
  con un botón para copiarlo.
- **Flujo volumétrico:** al elegir un segmento de la torre de velocidad, los mm³/s de su línea más rápida.
- **En la interfaz:** las tarjetas nuevas; el rango ajustable solo en las torres, con límites y decimales por tipo;
  «Lista: … de impresión» en las pruebas que no son torres; el aviso de la vista vacía ya no queda tapado por el
  panel; `--calibration` acepta todas (y `extruder`).
- `CalibrationKind` con nombre (`calibration_kind_name`), `is_tower` y `calibration_tower`; los proyectos y la línea
  de comandos usan esos nombres.

**Probado:** `tests/test_calibration.cpp`, ahora 10 casos y 234 comprobaciones:
- cada tipo vuelve de su nombre, sabe si es torre, y su prueba automática se lamina y pasa el verificador;
- la torre de velocidad de retracción retrae exactamente a 25, 35, 45 y 55 mm/s (F1500 a F3300);
- el avance de presión en Marlin, Klipper y RepRapFirmware: cada segmento escribe su comando, en orden; el ajuste se
  lee, se escribe y se rechaza fuera de rango; 0 no escribe nada;
- el tubo: área de las paredes en un segmento y en la ranura;
- los cuadrados de la primera capa: una sola capa, 5 × 900 mm², en su lugar; en una cama de 100 mm, uno solo;
- los encastres: el área del bloque y de los pernos, y 15 contornos a 4,5 mm (cada juego queda abierto, el de 0,1 mm
  también);
- el flujo volumétrico (6,4 mm³/s al 100 %) y la cuenta de los pasos en los dos sentidos.

Capturas: las tarjetas de avance de presión, encastres, primera capa y pasos del extrusor.

**Medido:** la torre de avance de presión automática (Bowden, 9 segmentos) lamina en 5,9 s en la interfaz bajo xvfb,
132 793 líneas verificadas, 1 h 3 min; la de encastres en 1,3 s, 424 719 líneas, 39 min; la primera capa, 1261
líneas, 7 min.

**No probado:** impreso en una máquina real (necesita al dueño); el avance de presión en una impresora con cada
firmware.

**Sigue:**
- resonancias (que el motor escriba la aceleración), puentes y voladizos, tope de flujo volumétrico, pata de
  elefante;
- después, la release 0.7.0-alpha.1.

## 2026-10-24 — el motor reemplaza a Tisma Slicer (plan)

**Decisión del dueño**, que llegó por la sesión de fichas y el dueño confirmó en esta sesión («si, confirmo el plan»):
el motor pasa a ser el nuevo slicer y reemplaza a Tisma Slicer, sin perder ninguna de sus funciones.

**Hecho:**
- `PLAN.md`:
  - la decisión, arriba;
  - F6 pasa a ser el reemplazo de Tisma Slicer: cobertura de `docs/spec/COVERAGE.md`, comparación como caja negra y la
    migración que elija el dueño;
  - las fases P1–P5 pasan a ser obligatorias antes de dejar Tisma Slicer;
  - la interfaz propia sale de «Fuera de la primera versión»;
  - «Cómo se usa»: la interfaz es el slicer; `tisma-engine` lo mismo sin ventana;
  - la ubicación de las piezas queda decidida (la interfaz centra en X e Y; el motor apoya en Z).
- `CLAUDE.md` y `README.md` dicen lo mismo en una línea.

**Abierto, para el dueño:** la migración de los proyectos 3MF y los perfiles guardados de los usuarios de Tisma
Slicer (dos opciones en `docs/spec/COVERAGE.md`, «Migración»).

**Pendiente de las fichas:** `COVERAGE.md` y las fichas nuevas (interfaz, paredes extra, piel difusa, rellenos,
falda/borde/balsa, más soportes, velocidades y aceleraciones, enfriamiento, retracción y viajes, modo jarrón) están en
las PRs #2 a #8, todavía sin fusionar en `main`. Se leen cuando estén en `main` y pasen `tools/check_spec.py`.

## 2026-10-24 — capacidades de impresoras y filamentos

**Pedido del dueño:** integrar las capacidades de cada impresora «a lo mayor cantidad de detalles posible» y lo mismo
para los filamentos, para que el laminador use lo importante (más capacidad de la máquina); no le interesa que los
perfiles sean comerciales. Los perfiles de otros programas siguen afuera por la regla de sala limpia.

**Hecho:**
- **El catálogo** (`docs/CATALOG.md`, «Capacidades en detalle») acepta por impresora la cinemática, el flujo máximo,
  la cámara calefaccionada, la boquilla endurecida, el hot end, el sensor, las camas, la conectividad, las funciones,
  las ranuras multimaterial y los cabezales; por filamento el flujo máximo, la cámara, el secado, abrasivo, soluble,
  la dureza, Tg, HDT, la contracción y las camas. Todo opcional y validado con rangos (`parse_catalog`).
- **Los ajustes** (`docs/CONFIG.md`): `machine.max_flow`, `max_nozzle_temp`, `max_bed_temp`, `max_chamber_temp`,
  `enclosed`, `hardened_nozzle`; `extrusion.max_flow`, `chamber_temperature`, `abrasive`, `needs_enclosure`. Van al
  perfil de impresora y de filamento respectivamente, y los llenan `printer_profile` y `filament_profile`.
- **El motor los usa** (`src/gcode/writer.cpp`):
  - tope de flujo volumétrico: la velocidad de cada tramo no pasa de flujo / (ancho × alto × flujo %), con el menor
    de los dos límites, redondeada a mm/min enteros;
  - la cámara: `M141 S` (o `SET_HEATER_TEMPERATURE HEATER=chamber` en Klipper) después del G-code de inicio, sin
    pasar del máximo de la impresora, y apagada al final;
  - `capability_warnings`: boquilla o cama más calientes que el máximo, cámara que no calienta o no alcanza,
    impresora cerrada, boquilla endurecida (tipos `nozzle_temperature`, `bed_temperature`, `chamber`, `enclosure`,
    `abrasive`), sumados a los avisos del G-code.
- **La interfaz:** la ficha del catálogo muestra todos los detalles; los avisos van en la barra de estado con su
  texto al pasar el puntero.
- **`tools/catalog_merge.py`:** valida los valores contra los rangos del catálogo y tiene `--update`, que suma a una
  entrada existente solo las claves que no tenía y las fuentes nuevas.
- **Pruebas:** `tests/test_gcode.cpp` «capabilities» (el flujo medido del G-code del cubo baja al tope, 4 y 3 mm³/s;
  la cámara al principio y al final; los cinco avisos y ninguno cuando todo cabe o nada se sabe);
  `tests/test_catalog.cpp` «capabilities in detail» (los campos, los perfiles, los dos perfiles juntos con y sin
  avisos, `is_abrasive`, los errores de formato); `tests/test_settings.cpp` (ida y vuelta de las claves nuevas y
  sus rangos).

**Medido:** `tools/ci_linux.sh` pasa entero (264 pruebas por compilador, gcc, clang y ASan + UBSan, 215 s la
última). El cubo de prueba sin límite pasa de 4 mm³/s; con tope de 3 mm³/s, el flujo más alto medido en su G-code
queda entre 2,4 y 3,06 mm³/s (el redondeo a mm/min enteros).

**No probado:** una impresora real con cámara (`M141` sale de la documentación de Marlin y RepRapFirmware; la
calefacción de cámara de Klipper depende del nombre del calentador en la configuración de cada máquina); los
detalles del catálogo todavía no están cargados (la tanda de detalles, abajo).

**Sigue:**
- la tanda de detalles del catálogo (seis agentes de investigación, solo páginas oficiales; cayeron por el límite
  de la sesión y se retoman);
- el filtro de moiré de la vista 3D (pedido del dueño, 2026-10-24);
- un perfil de impresión «rápido» armado desde las capacidades (velocidad y aceleración máximas, flujo).

## 2026-10-24 — filtro de moiré en la vista 3D

**Pedido del dueño:** «hay que añadir al apartado visual un filtro de moire».

**Hecho** (`docs/UI.md`, «Imagen»):
- **Las líneas de la vista previa** (`apps/tisma/renderer.cpp`, `kPreviewFragment`): el shader de vértices pasa las
  coordenadas de la sección de cada línea, sus ejes y su tamaño; el de fragmentos mide con `fwidth` cuánto abarca el
  píxel de la sección y, por debajo de dos píxeles por línea, reemplaza la normal por el promedio de las normales de
  todo el perfil (16 puntos del rombo, las normales de la elipse), cada una pesada por cuánto mira al ojo: lo que el
  píxel vería de verdad. El brillo se ensancha con la fórmula de Toksvig 2005 según el largo de esa normal media. De
  cerca no cambia nada; también en el G-buffer, así la oclusión ambiental ve las mismas normales.
  - El primer intento (fundir la normal con el eje «más fino» de la sección) apenas bajaba el moiré (45,8 → 42,0
    a 2×): sobre una cara del rombo las dos coordenadas cambian igual de rápido y el eje elegido era arbitrario.
- **La grilla de la cama** (`kGridFragment`, un programa nuevo; el contorno sigue con el de líneas): cada línea se
  desvanece cuando la siguiente de su clase queda a menos de 6 px en pantalla (medido proyectando el punto vecino);
  las mayores aguantan más.
- **La interfaz:** casilla **Filtro de moiré** debajo de FXAA, con ayuda al pasar el puntero; activado por defecto;
  se guarda en `interface.json` (`moire_filter`); `--moire on|off`.
- **`--moire-check`** (con `--screenshot`): dibuja la vista previa desde el doble de lejos, sin multimuestreo, con y
  sin el filtro, guarda las dos imágenes al lado de la captura (`…_moire_off.png`, `…_moire_on.png`) y exige que,
  donde el filtro cambió píxeles, los vecinos difieran al menos un 15 % menos. Prueba
  `ui_moire_filter_smooths_fine_lines` en CTest.

**Medido:** con el cubo de prueba a 2× de distancia (unos 0,8 px por capa), 2356 píxeles cambiados; diferencia
media entre vecinos 45,4 sin el filtro y 23,2 con él (−49 %). A ojo, en los recortes ampliados: sin el filtro el
cubo lejano es un tablero de moiré y en la vista normal (1,6 px por capa) las paredes y la tapa tienen bandas
onduladas; con el filtro, caras lisas con la luz de cada cara. A 8× el cubo mide 10 px y la medida la domina la
grilla (38,2 → 32,1): por eso la prueba va a 2×.

**No probado:** el aspecto en una pantalla real y en piezas grandes; el ray tracing y la oclusión ambiental reciben
las normales filtradas, pero la profundidad sigue con las líneas (no se midió si la oclusión conserva ruido de
lejos).

**Sigue:** cargar la tanda de detalles del catálogo cuando terminen los agentes (ya terminaron otras marcas, 29
entradas con detalles y 6 modelos nuevos, y filamentos del grupo 2, 36 con detalles y 10 nuevos); un perfil de
impresión rápido desde las capacidades.

## 2026-10-24 — catálogo: tanda de detalles, grupos 1 de 3

**Hecho:** se cargaron con `tools/catalog_merge.py --update` los dos grupos de investigación que terminaron (solo
páginas oficiales; `BRIEF2.md` en el cuaderno de la sesión):
- **Impresoras de otras marcas** (AnkerMake, BIQU, Kingroon, Longer, LulzBot, Peopoly, Raise3D, RatRig, Snapmaker,
  Tronxy, Voron): 29 entradas con detalles y 6 modelos nuevos (Raise3D Pro3 HS, E3, E2CF; Kingroon KP5L Pro; LulzBot
  TAZ 8, Mini 3).
- **Filamentos del grupo 2** (Anycubic, colorFabb, Creality, Extrudr, Fiberlogy, Fillamentum, Hatchbox, Overture):
  36 entradas con detalles y 10 filamentos nuevos (colorFabb PA-CF Low Warp; Fiberlogy Nylon PA12+CF, BVOH, HIPS;
  Creality Hyper PETG-CF; Fillamentum HIPS Extrafill; Overture PC Professional, Easy Nylon; Anycubic ASA; Extrudr
  DuraPro PA12 CF).

El catálogo queda en **144 impresoras de 21 fabricantes y 108 filamentos de 14**. Campos de detalle cargados hasta
ahora: en impresoras, funciones 26, sensor 24, conectividad 23, hot end 22, camas 21, cinemática 18, boquilla
endurecida 11, flujo máximo 7, cabezales 5; en filamentos, secado 42, HDT 29, camas 29, Tg 16, flujo máximo 12,
abrasivo 11, dureza 8, contracción 6, soluble 3, cámara 1. La carga no descartó ninguna entrada ni cambió ningún
valor que ya estaba.

**Lo que los agentes dejaron vacío, y por qué** (una muestra): AnkerMake M5C sin página de producto (datos del blog
oficial de eufymake); Voron sin hot end ni cama (los documentos oficiales no los fijan); Tronxy X5SA-500 2E sin
cinemática (la página no la dice); LulzBot TAZ 8 sin diámetro de filamento («según el cabezal»); colorFabb sin flujo
máximo (dos tablas oficiales con valores distintos); Hatchbox sin hoja técnica publicada. Los rangos de secado se
guardan por el valor bajo; el HDT, el de 0,45 MPa cuando hay dos.

**Medido:** `tisma_tests "[catalog]"` pasa (1041 comprobaciones, cada entrada del catálogo incluido validada y con
su perfil).

**Sigue:** los otros cuatro grupos (Creality; Bambu Lab, Prusa, QIDI, Flashforge y UltiMaker; Elegoo, Anycubic,
Artillery y Sovol; filamentos de Bambu Lab, Elegoo, eSUN, Polymaker, Prusament y SUNLU), cuando terminen.

## 2026-10-24 — calibraciones estándar, tercera tanda

**Hecho** (`docs/CALIBRATION.md`), siguiendo la lista de pendientes de `PLAN.md` («sigue con la lista de
pendientes»):
- **Tope de flujo volumétrico** (`max_flow`): el tubo cuadrado, cada segmento con un cambio por altura nuevo,
  `volumetric_flow` (mm³/s): el escritor fuerza la velocidad de toda extrusión a la que da ese flujo en su cordón,
  por encima de `speed.max` y del tope de flujo, sin el frenado del tiempo mínimo de capa, hasta lo que la máquina
  se mueve (`max_feed_x`, `_y`); el verificador admite esas velocidades (`gcode_limits`). **Usar** pasa el valor a
  `machine.max_flow`. Rango automático alrededor del tope conocido, o de 4 a 30 mm³/s.
- **Resonancias (aceleración)**: cambio por altura `acceleration`, escrito con el comando del dialecto; el motor
  además escribe la aceleración de los ajustes después del G-code de inicio (`M204 P… T…` /
  `SET_VELOCITY_LIMIT ACCEL=…`), lo que faltaba según el plan. **Usar** pasa el valor a `machine.acceleration` (y sube
  `travel_acceleration` si quedaba más baja). Rango de 500 al doble de la aceleración en uso.
- **Pata de elefante**: un bloque de 20 × 20 × 6 mm; se miden el ancho de abajo y el del cuerpo;
  `elephant_foot_from` da `slicing.first_layer_shrink` = el actual + la mitad del exceso (0 a 2 mm). Tarjeta con las
  dos medidas, el valor actual y el nuevo, y **Aplicar el achique**.
- El tubo del avance de presión pasó a un ayudante (`tube_tower`) que comparten las tres torres de tubo.
- Textos, tarjetas, rangos ajustables y `--calibration max_flow|acceleration|elephant_foot`.

**Medido:** `tisma_tests "[calibration],[settings]"`: 2048 comprobaciones en 19 casos. En la torre de flujo, a
16 mm³/s las líneas de 0,4 × 0,2 mm corren a F12000 (200 mm/s) aunque `speed.max` sea 20 mm/s y el tiempo mínimo
de capa 60 s; con la máquina limitada a 150 mm/s, no pasa de F9000. La torre de aceleración escribe su comando por
segmento en los tres dialectos y el G-code pasa el verificador.

**No probado:** impreso en una impresora real (necesita al dueño). El tiempo estimado no sigue la aceleración por
segmento (usa `machine.acceleration`).

**Sigue:** puentes y voladizos (la última estándar); la tanda de detalles del catálogo cuando terminen los agentes;
la app móvil de Android (pedido nuevo del dueño: a definir qué sigue, ver `PLAN.md`).

## 2026-10-24 — la app de Android (plan)

**Decisiones del dueño:** «ve por la opción 1 y luego por devices, así podemos conectar impresoras; tiene que tener
soporte USB» y «algo similar a Prusa EasyPrint en estética, con nuestras funciones».

**Hecho:** `docs/MOBILE.md`, la fase P6 de `PLAN.md`:
- qué hace la app (impresoras, impresión, cámara, controles, archivos, avisos) y la estética como caja negra;
- una sola capa de dispositivos en C++ (`src/device/`) compartida con la página Device: la interfaz `Printer`, los
  transportes y un host de G-code por puerto serie propio (números de línea, suma de verificación, `ok`,
  `Resend:`, `M105`/`M155`, `M114`, `M27`), con un simulador de impresora serie para las pruebas de CI;
- la app en Kotlin + Jetpack Compose con el núcleo por NDK y el USB por la API USB Host de Android;
- el orden: USB en el escritorio → la app con USB OTG → las conexiones de red de la ficha de Device → cámara,
  archivos y avisos → laminar simple desde el teléfono (a confirmar);
- tres preguntas al dueño: teléfono e impresora de prueba, cámara, instalación.

**Aclaración del dueño, después:** «el slicer de PC puede imprimir por USB; el celular, solo por WiFi». El plan
quedó así: sin USB en el teléfono; las impresoras por USB de la PC se siguen por la red a través de un servicio
local de `tisma` (estado, avance, controles, cámara, con vinculación), y las que tienen API de red, directo.

**Sigue:** empezar `src/device/` con el host serie y el simulador cuando la ficha de Device esté en `main` (PR #11)
o antes, con la documentación pública de Marlin y RepRapFirmware; mientras, los grupos del catálogo que faltan.

## 2026-10-24 — Device: la capa de dispositivos y el host serie

**Pedido del dueño:** «arrancá con `src/device` y el host serie». Diseño en `docs/DEVICE.md`.

**Hecho:**
- `src/device/printer.hpp`: `Printer`, `PrinterStatus`, `PrinterEvent` y los controles de siempre como comandos.
- `src/device/stream.*`: `ByteStream` y el tubo en memoria (`make_pipe`).
- `src/device/serial_port.*`: el puerto serie, 8N1, cualquier velocidad en Linux (`termios2`, `BOTHER`: 250 000
  incluido) y el puerto COM en Windows; la lista de puertos.
- `src/device/gcode_host.*`: el host de G-code en su hilo: conexión con `M110 N0` hasta el primer `ok`, `M115`,
  `M155 S2` con `M105` de respaldo; líneas numeradas con suma de verificación; un `ok` por línea; reenvíos (traga
  el `ok` del pedido, renumera si la placa se reinició, 5 intentos); `echo:busy` reinicia la espera; temperaturas,
  posición y firmware desde los informes; trabajos con avance, pausa y cancelación (apaga calefactores y
  ventilador, suelta motores); comandos sueltos en el primer hueco; errores de conexión claros.
- `src/device/simulator.*`: la impresora simulada tipo Marlin, con reenvío provocado (`fail_line`), temperaturas
  que se mueven, `M109`/`M190` que esperan con `echo:busy`, y el registro de comandos.
- `tests/test_device.cpp`: seis casos (ver `docs/DEVICE.md`, «Pruebas»).

**Medido:** `tisma_tests "[device]"`: 108 comprobaciones en 6 casos; el trabajo de prueba (1265 líneas con una
rota) se imprime al simulador en menos de un segundo, con un solo reenvío y cada `G1` recibido una vez.

**No probado:** una impresora real por USB (necesita al dueño: qué impresora y en qué sistema); el puerto COM de
Windows se compila en el CI de Windows pero no se ejecutó contra un puerto real.

**Sigue:** la página Device en `tisma` (puertos, conectar, estado, imprimir lo exportado, pausar, cancelar; el
simulador como impresora de prueba); después el servicio local para la app y las conexiones de red de la ficha.

## 2026-10-25 — mantenimiento

Bloque de la tarea programada cada 8 horas (decisión del dueño). Antes, un arreglo del CI de Windows: a
`tests/test_device.cpp` le faltaba `#include <algorithm>` (MSVC no lo trae por otros encabezados; gcc y clang sí);
`8bf65d1`.

**Revisado:**
- `origin/main` en `dev`: nada nuevo.
- `tools/ci_linux.sh` (272 pruebas en gcc, clang y ASan + UBSan).
- La búsqueda aleatoria de G-code: 2000 corridas con la semilla 1792896531; después de ampliarla (abajo), 2000 más
  con 1792896647. 0 fallas (0 y 4 piezas rechazadas como no imprimibles, lo esperado).
- Los cambios recientes: el tope de flujo, la cámara, los avisos, el flujo forzado y la aceleración por altura, la
  capa de dispositivos. Una sospecha: que el verificador rechazara cordones más finos que un cuarto del ancho con
  flujo forzado. No se reproduce: en una cuña que se afina hasta un filo, las líneas forzadas corren a la velocidad del
  cordón de ancho completo; queda una prueba de guarda (`calibration: a forced flow on a wedge that thins to an edge
  checks`).
- Los avisos del compilador en los tres compiladores y en el CI de Windows.

**Encontrado y corregido:**
- **La búsqueda aleatoria no probaba nada de lo nuevo.** Ahora sortea también el dialecto, los topes de flujo de la
  máquina y del filamento, el avance de presión, las aceleraciones, la cámara y hasta tres cambios por altura de los
  de las torres (temperatura, flujo, velocidad, retracción, flujo forzado, aceleración, avance de presión,
  ventilador), y los muestra cuando una corrida falla.
- **Avisos del compilador:** clang (`-Wsign-conversion` en la suma de verificación del host serie); MSVC (C4996 en
  `std::getenv`, que acá solo se lee: `_CRT_SECURE_NO_WARNINGS` en las opciones de aviso; C4702 dentro de nanosvg,
  apagado alrededor de su `#include`); una sombra de variable en la búsqueda aleatoria.

**Optimizado:** la escritura del G-code se iba casi la mitad en `std::to_chars` con decimales fijos (callgrind: 2,84
de 5,84 mil millones de instrucciones del escritor, la esfera de 200 segmentos). Un camino rápido propio escala por
10^d, redondea a entero y pone el punto; los valores a menos de 0,001 de un medio, los enormes (desde 2^40) y los de
más de 9 decimales siguen yendo a `to_chars`. Da el mismo texto: lo prueba `numbers in the G-code: the fast
formatter writes what std::to_chars writes` (casos de borde y 2 millones de valores al azar) y el G-code de la esfera
sale igual byte a byte.

**Medido** (4 núcleos, Release, CPU):
- Esfera de banco de 100 mm (`bench/gen_sphere.py --segments 400`, 319 200 triángulos, 500 capas, 884 549 líneas):
  laminado 0,84–0,89 s; escritura del G-code de 0,81–0,88 s a **0,60–0,62 s**; verificación 0,22–0,24 s.
- Análisis del cubo de prueba: solución 173–210 ms (22 iteraciones, 3 niveles); el bloque anterior midió 141–151 ms.
  El código del análisis no cambió desde entonces (`git log src/fea`) y el laminado mide lo mismo: lo tomo como
  variación de la máquina; no se investigó más.

**No probado:** la búsqueda aleatoria ampliada con ASan (solo en Release).

## 2026-10-25 — la pestaña Impresora (Device)

**Pedido del dueño:** «seguí con la página Device». Diseño en `docs/DEVICE.md`, «La página Impresora».

**Hecho:**
- La pestaña **Impresora** en `tisma` (`apps/tisma/app.cpp`, `device_panel`): puerto y velocidad (recordados),
  conectar y desconectar, estado con su color, firmware y error, temperaturas con objetivo, imprimir lo laminado o
  el G-code abierto, avance con líneas, tiempo pasado y restante (el estimado del G-code), reenvíos, pausar, seguir,
  cancelar, mover los ejes, extruir, retraer, ventilador, y el registro de eventos; los eventos importantes también
  como avisos. La pieza se corre a la derecha, como en Calibración.
- `--device PUERTO|sim`, `--view device` y `--device-print` (lamina, imprime y espera el final); la prueba
  `ui_device_prints_to_simulator`.
- El host pide `M105` al conectar: las temperaturas aparecen enseguida, no al primer informe de `M155`.

**Encontrado y corregido en el camino:**
- **Una carrera en el host** (la encontró UBSan en el CI de Linux): el estado nuevo (error, lista, terminado, en
  pausa) se publicaba y su evento se agregaba después, en otro bloqueo; quien veía el estado y retiraba los eventos
  en ese instante los encontraba vacíos (y la prueba leía `.back()` de una lista vacía). Ahora `change()` publica
  los dos juntos; las pruebas nunca leen `.back()` sin comprobar que haya algo. 15 corridas seguidas con ASan +
  UBSan sin fallas.
- **La prueba de pausa en Windows:** el trabajo de 2000 líneas contra el simulador en memoria terminaba antes de que
  la prueba alcanzara a pausar (allá un `sleep_for(5ms)` dura ~15 ms). El simulador ahora puede tardar por movimiento
  (`move_time`, 2 ms en esa prueba): el trabajo dura segundos y la pausa siempre cae en medio.

**Medido:** el cubo, laminado e impreso a la impresora simulada desde la interfaz, en 1,4 s de punta a punta (2552
líneas, con las esperas de `M190` y `M109` del simulador).

**No probado:** con una impresora real por USB (necesita al dueño); la pestaña a mano, con el ratón (las pruebas
usan la línea de comandos).

**Sigue:** la altura de capa variable por zona (pedido del dueño): el plan, con *local adaptive slicing* (Tyberg y
Bøhn 1998); después, el servicio local para la app de Android.

## 2026-10-25 — altura de capa por zona (etapas 1 y 2)

**Pedido del dueño:** «altura de capa variable que dependa del detalle y no de la pieza entera, así podemos
conseguir que aunque dos partes estén a la misma altura tengan capas de alturas distintas para mejorar calidades».
Diseño en `docs/LOCAL_LAYERS.md`.

**Hecho:**
- La ficha `docs/spec/features/slicing.md` (pasa `tools/check_spec.py`) solo pide alturas por Z para el objeto
  entero; esto va más allá, con la idea de *local adaptive slicing* (Tyberg y Bøhn 1998) y *region-based adaptive
  slicing* (Mani, Kulkarni y Dutta 1999), y un método propio.
- `src/slice/local_layers.*`: los grupos de `k` subcapas (después de la primera capa), la zona gruesa de cada uno
  (donde sus secciones coinciden, sin fondos ni topes expuestos, lejos del detalle por un margen, sin pedazos de
  menos de dos anchos), el recorte de los caminos por zona con el ancho interpolado, y el reparto: lo fino en cada
  subcapa y lo grueso una vez, en la última, con alto `H`.
- La tubería lamina la pila fina; las pieles se cuentan en capas gruesas y el espacio de los soportes en subcapas.
- El escritor pone lo grueso al final de su capa entre `;HEIGHT:H` y `;HEIGHT:h`; el verificador y la vista previa ya
  leen el alto por tramo.
- Ajustes `layers.detail_height`, `detail_tolerance` y `detail_margin` (`docs/CONFIG.md`); la subcapa no baja de
  0,04 mm.
- La búsqueda aleatoria de G-code sortea la altura por zona.

**Encontrado en el camino:** la búsqueda aleatoria dio dos fallas con subcapas de 0,0215 mm (una capa de 0,086 mm
partida en 4): `;HEIGHT:` va con 3 decimales y a ese alto el redondeo ya es un 2,4 %, más que lo que tolera el
verificador. Con el piso de 0,04 mm, 0 fallas en 3000 casos (semillas 1792898697 y 1792898943).

**Medido** (Release, CPU): una caja de 20 × 20 × 9 mm y una pirámide de 20 mm de base y 9 de alto lado a lado, capas
de 0,3 mm con detalle de 0,1 mm (88 subcapas, 28 capas gruesas con zona gruesa): el tiempo estimado baja de 1425 s
(todo a 0,1 mm, con las mismas pieles en mm) a **968 s (−32 %)**, con el 97,8 % del material. En la interfaz,
cargando los ajustes desde un JSON: 16 min 4 s, G-code verificado.

**No probado:** impreso en una máquina real (necesita al dueño): en especial el pegado entre la zona gruesa y la fina
y si 0,3 mm de golpe llena bien el hueco de las subcapas; el aspecto en piezas con curvas suaves (solo cajas y
pirámides).

**Sigue:** la etapa 3 (los ajustes en la interfaz y la vista previa con el color por alto de capa).

## 2026-10-25 — altura de capa por zona: la interfaz (etapa 3)

**Hecho:**
- En la sección Impresión, debajo de la altura de la primera capa: **Alto del detalle**; cuando no es 0, **Escalón
  aceptado** y **Margen del detalle**, cada uno con su ayuda.
- En la vista previa, **Color por → Alto de capa**: la rampa de color va del alto más fino al más grueso de los
  cordones (el mínimo y el máximo vienen de `;HEIGHT:`, que ahora cambia dentro de una capa). `--color` para las
  capturas.
- `PreviewBuffers` guarda el rango de altos; la prueba de la interfaz lo comprueba con una zona gruesa dentro de una
  capa.

**Visto:** la caja y la pirámide del ejemplo, con capas de 0,3 mm y detalle de 0,1 mm, pintadas por alto: las
paredes rectas de la caja en rojo (0,3 mm), el tope de la caja y la pirámide en azul (0,1 mm), a la misma altura.

**No probado:** los campos a mano, con el ratón (las capturas cargan los ajustes desde un JSON).

**Sigue:** la etapa 4 (medir en el banco contra todo fino y todo grueso); la altura variable por Z de la ficha.

## 2026-10-25 — altura de capa por zona: medida en el banco (etapa 4)

**Hecho:** tres piezas en tres modos (todo a 0,1 mm, todo a 0,3 mm, por zona 0,3/0,1), con el tiempo estimado, el
material, la parte gruesa y la pared exterior por alto (`docs/LOCAL_LAYERS.md`, «Medido»). Y una optimización del
recorte que el banco mostró: las aristas de la zona indexadas por franjas de Y.

**Medido** (Release, CPU):
- Tiempo de impresión por zona contra todo fino: caja y pirámide −39 % (26,4 → 16,1 min), cilindro con cúpula
  −52 % (113,0 → 54,4 min), esfera de 100 mm −45 % (918,9 → 507,4 min); el material igual (±1 %).
- La pared exterior va fina en el 60, 62 y 93 % de su largo: las pendientes; gruesa donde es recta.
- El recorte: 4,1 veces menos instrucciones (callgrind, la cúpula: 2,56 → 0,62 mil millones); el laminado por zona
  entero, −30 % (10,5 → 7,4 mil millones); el mismo G-code byte a byte. Con el reloj, en esta máquina compartida, la
  esfera lamina en 2,1–2,5 s por zona contra 1,8–1,9 s todo fino.

**Decidido:** el recorte no va a la GPU (las zonas y el recorte son el 15 % del laminado).

**No probado:** la calidad impresa (el escalón real), que necesita al dueño y una impresora.

**Sigue:** la altura variable por Z de la ficha (`slicing.md`), una lista de (Z, alto) por objeto, que la altura
por zona generaliza; o lo que el dueño elija.

## 2026-10-25 — altura de capa variable por Z

**Pedido del dueño:** «seguí con la altura variable por Z y por Z anti-aliasing así las combinamos para máxima calidad»,
y que no dependa de dos objetos: que sirva en una pieza compleja.

**Hecho** (`docs/VARIABLE_LAYERS.md`):
- `layers.heights`: la lista de tramos (Z, altura) de la ficha (`slicing.md`).
- `layers.cusp_height`: la altura por pendiente, por el escalón máximo (Dolenc y Mäkelä 1994). Las caras de la malla
  limitan bandas de Z de 0,005 mm, y las caras planas cierran una capa.
- `machine.min_layer_height` y `max_layer_height`, nunca por encima de la boquilla.
- **Con la altura por zona**, la pendiente decide cuántas subcapas lleva cada capa gruesa (1 donde no hace falta). Si
  ni las más finas alcanzan, la capa gruesa se achica. Las pieles cuentan capas gruesas.
- **Sin lista ni pendiente,** la pila es la de siempre: el mismo G-code byte a byte (comprobado en la cúpula y la
  esfera, en cuatro modos).
- **La interfaz:** «Escalón en pendientes» y «Alturas por Z» (con agregar y quitar tramos) en Impresión; «Capa mínima»
  y «Capa máxima» en Impresora.
- **Pruebas:** 4 casos nuevos.
  - Un prisma de techo a 10° y una cúpula sobre una columna, cada uno en una sola malla.
  - La columna va gruesa y la cúpula fina.
  - Con lista, con pendiente y con zona + pendiente, el G-code pasa el chequeo y el material queda en ±6 %.
- **El fuzz** sortea pendiente, tramos y límites de máquina: 2000 corridas en 5 semillas, 0 fallas.

**Corregido, por el fuzz** (fallas que ya estaban y que las combinaciones nuevas del sorteo destaparon):
- **Un desplazamiento XY de −0,00013 mm sobre la esfera:** dejaba zigzags de aristas de 1 nm en las esquinas, y el
  diagrama de Voronoi propio lanzaba «no seed node». El de referencia, como respaldo, armaba grafos de 260 000 nodos
  y colgaba el laminado: ese respaldo se descartó.
  - Ahora las esquinas a menos de 10 nm de la anterior se quitan antes de armar el diagrama (de un cuelgue a 0,16 s).
  - El G-code de los modelos comunes no cambia.
  - Prueba con el contorno real de esa capa: falla sin el arreglo.
- **Un cambio por altura a 0,0002 mm del tope de una capa:** el escritor lo aplicaba en la capa siguiente y el
  verificador, que lee el Z redondeado a 3 decimales, en esa. Ahora el escritor usa el tope tal como se escribe.
- **`tisma-engine`:** un error interno no atrapado cortaba el programa («terminate called»). Ahora sale con el código
  7 y un mensaje.
- **El fuzz** deja el modelo, los ajustes y el G-code de una corrida que falla en la carpeta de `TISMA_FUZZ_DUMP`.

**Medido** (cúpula sobre columna de 40 mm y esfera de 100 mm, CPU; el escalón es `h · |n_z|` del perímetro externo):
- **Escalón máximo 0,1 mm:**
  - zona + pendiente: cúpula 48,6 min y esfera 425,8 min, con p95 de 0,094 y 0,096;
  - todo a 0,1 mm: 113,0 y 918,6 min, con p95 de 0,079 y 0,088;
  - por zona sola: 54,4 y 495,7 min.
- **Escalón máximo 0,05 mm, cúpula:**
  - zona + pendiente: 56,1 min, p95 0,050;
  - por pendiente sola: 61,4 min;
  - todo a 0,05 mm: 224,6 min, p95 0,039.
- **Material:** ±1,5 % en todos los modos.

**No probado:**
- la calidad impresa (necesita una impresora);
- la lista por objeto: el motor lamina toda la cama en una pila, así que la lista vale para toda la cama.

**Sigue:**
1. Z anti-aliasing (`docs/spec/features/z_antialiasing.md` y Song et al. 2017), para bajar el escalón que queda en
   las superficies de arriba de poca pendiente.
2. Después, en la cola del dueño:
   - el relleno por resistencia y rapidez;
   - el tiempo de impresión más preciso;
   - el panel izquierdo por niveles con Impresora, Filamento y Objetos.

## 2026-10-25 — Z anti-aliasing, y el perfil Máxima calidad

**Pedido del dueño:** «altura variable por Z y Z anti-aliasing, así las combinamos para máxima calidad», en una pieza
compleja.

**Hecho** (`docs/ZAA.md`):
- `src/slice/zaa.*`:
  - las caras que miran hacia arriba en una grilla de XY;
  - cada punto visible (paredes, relleno de arriba, relleno de huecos, también de las capas gruesas por zona) va a
    la superficie si esta corta su capa, con el piso de `zaa.min_thickness`;
  - con `zaa.raise` (Song et al. 2017), también sube hasta media capa;
  - los tramos se parten cada 0,2 mm donde salen del tope.
- **El punto del camino** lleva su `dz`, el primer paso hacia los caminos 3D de la visión.
- **El escritor:**
  - Z por punto, redondeada como se escribe;
  - el material del espesor que queda;
  - los viajes vuelven al tope, o pasan por encima de lo subido;
  - los caminos más bajos primero, y la pared externa antes que las internas cuando salen del tope.
- **El chequeo y la vista previa** entienden el espesor que queda.
- **Ajustes:** `zaa.enabled`, `zaa.min_thickness` y `zaa.raise`, en el perfil de impresión y en el panel.
- **Perfil de fábrica Máxima calidad:** 0,3 mm, detalle de 0,1 mm, escalón de 0,05 mm y ZAA con subida.
- **Pruebas:** 4 casos nuevos.
  - La regla Z1 de la ficha en techos de 5°, 10° y 15°: la Z con error de 0,0006 mm y el material con error de
    0,03.
  - Z2.
  - Con subida, altura por zona y por pendiente.
  - La vista previa.
- **El fuzz** sortea ZAA: 1200 corridas en 4 semillas, 0 fallas.
- **Con ZAA apagado,** el G-code no cambia: comparado en la cúpula y la esfera, en 4 modos. Solo cambian las 3
  líneas de los ajustes al final.

**Medido** (error entre el tope de cada cordón visible y la superficie exacta, en los cordones de arriba):

| Pieza | Modo | Error medio | p95 | Tiempo |
|---|---|---|---|---|
| esfera | plano 0,2 mm | 0,048 | 0,094 | 447 min |
| esfera | ZAA solo hacia abajo | 0,043 | 0,094 | 447 min |
| esfera | ZAA con subida | 0,0026 | 0,005 | 448 min |
| esfera | zona + pendiente + ZAA | 0,0011 | 0,002 | 526 min |
| esfera | plano 0,05 mm | 0,014 | 0,024 | 1788 min |
| cúpula | Máxima calidad | 0,004 | 0,006 | 56 min |
| cúpula | plano 0,05 mm | 0,013 | 0,024 | 218 min |

Laminar cuesta un 8–9 % más.

**No probado:**
- una impresión real;
- el daño de la boquilla a cordones vecinos más altos: el orden simplificado lo reduce, pero el del artículo
  (subcaminos y grafo de alturas) queda pendiente.

**Sigue,** en la cola del dueño:
1. el relleno por resistencia y rapidez;
2. el tiempo de impresión más preciso;
3. el panel izquierdo por niveles, con Impresora, Filamento y Objetos.

## 2026-10-25 — relleno por resistencia y rapidez: combinar capas (etapa 1)

**Pedido del dueño:** «el relleno debe optimizarse para fuerza y rapidez de impresión». El plan va por etapas en
`docs/INFILL.md`, cada una medida en tiempo, material y rigidez:
1. combinar capas;
2. relleno denso bajo las superficies de arriba;
3. cúbico y cúbico adaptativo;
4. relleno guiado por tensiones.

**Hecho:**
- `infill.combine_layers`, `combine_auto` y `combine_max_height` (ficha `infill_patterns.md`, reglas 18 a 20).
- Las corridas de capas empiezan en la capa 1. Lo que todas tienen como relleno disperso se imprime una vez, en la de
  arriba, con el alto de la corrida (la misma capa gruesa de la altura por zona), alternando la dirección por corrida.
- Nunca más alto que la boquilla, y no se combina con la altura por zona.
- En Impresión, **Combinar relleno** y **Combinar lo más posible**.
- **Pruebas:** 2 casos.
  - Las corridas.
  - P11 de la ficha en el cubo de 20 mm:
    - el relleno solo en las capas pares desde la 4, a 0,4 mm;
    - la capa 3 sin relleno;
    - el doble de filamento por mm;
    - menos tiempo;
    - el chequeo.
- **Fuzz:** 900 corridas en 3 semillas, 0 fallas.

**Medido:** el mismo material (±0,15 %).
- Esfera de 100 mm: 447 → 297 min (−34 %) con capas de 0,2; 736 → 402 min (−45 %) con capas de 0,12 en automático.
- Cúpula: 55,5 → 42,3 min (−24 %).

**No medido:**
- la resistencia del relleno combinado: el análisis «como impresa» corta sus propias capas uniformes, así que primero
  tiene que leer el laminado real;
- una impresión real.

**Sigue:** el relleno denso bajo las superficies de arriba (`dense_infill.md`), y después cúbico y cúbico adaptativo.

## 2026-10-25 — relleno: denso bajo el techo, triángulos y cúbico (etapas 2 y 3)

**Hecho** (`docs/INFILL.md`):
- `infill.dense_under_top` y `dense_density` (ficha `dense_infill.md`): la parte del relleno que tiene encima la piel
  de arriba de la capa siguiente va más densa, con el mismo patrón y dirección.
- Patrones `triangles` y `cubic` (ficha `infill_patterns.md`, reglas 4 y 6): tres familias a 60°. En el cúbico,
  corridas `z / √2` con la altura, con un `shift` nuevo en `rectilinear_fill`.
- En el panel: las dos filas del denso y los dos patrones.
- **Pruebas:** 2 casos.
  - Los patrones: direcciones, tercios, igualdad entre capas, el corrimiento y su período.
  - D1 de la ficha: cuatro veces las líneas bajo el techo, el resto igual, chequeo.
- **El fuzz** sortea los patrones y el denso: 600 corridas en 2 semillas, 0 fallas.

**Midiendo:** la rigidez de cada patrón con el análisis «como impresa» del motor:
- un cubo de 20 mm comprimido;
- una barra de 60 × 10 × 10 en voladizo;
- con tiempo y material.

Va en un commit aparte, con los números en `docs/INFILL.md`.

**Sigue:** cúbico adaptativo (más chico cerca de las superficies), con la respuesta de la sesión de fichas sobre el
tamaño de los cubos; después, el relleno guiado por tensiones.

## 2026-10-25 — el análisis «como impresa» lee las capas gruesas

**Encontrado** midiendo la rigidez del relleno: el análisis pintaba los vóxeles con las paredes y los rellenos de
cada capa, pero no con las capas gruesas (`SlicedModel::thick`): la zona gruesa de la altura por zona y el relleno
combinado.
- En un cubo de 10 mm, con altura por zona el análisis veía el 3 % de los vóxeles impresos, en vez del 53 %.
- Con relleno combinado veía el 71 % de los de la misma pieza sin combinar.
- La pieza parecía hasta 26 veces más blanda.

**Arreglado:**
- cada capa gruesa llena todas las capas que abarca;
- la estimación de vóxeles y el modelo macizo usan las capas del laminado (`plan_layer_stack`), también con altura
  variable;
- prueba nueva: con relleno combinado, los vóxeles impresos quedan a ±5 % de los de sin combinar; con altura por
  zona, la parte impresa queda a ±8 %. Falla sin el arreglo (71 % y 3 %).

**Medido** (`docs/INFILL.md`, «Resistencia y rapidez, medidas»): la rigidez de cada patrón con el análisis, en un
cubo comprimido con 500 N y una barra en voladizo con 20 N.
- **Compresión:** la grilla, 3,1 veces más rígida por material que el rectilíneo; los triángulos, 2,9.
- **Flexión:** todos los patrones dentro del 4 %.
- **Combinar 2 capas:** −16 a −19 % de tiempo con menos de un 6 % de pérdida de rigidez.
- **La altura por zona** (0,3 / 0,1): la barra en 12,1 min contra 16,2, con un 16 % menos de flecha.
- **No probado:** probetas impresas.

**Sigue:**
1. el cúbico adaptativo, con la ficha que respondió la sesión de fichas (PR #15, espera la fusión a `main`);
2. el relleno guiado por tensiones (A2);
3. el tiempo de impresión más preciso (ficha `print_time.md`, en el mismo PR);
4. el panel por niveles (ficha `settings_levels.md`).

## 2026-10-25 — relleno guiado por tensiones (etapa 4)

**Hecho** (`docs/INFILL.md`):
- `infill.stress_density` y `tisma-engine slice … --case caso.json`:
  - el motor analiza la pieza maciza bajo el caso;
  - la carga de cada vóxel es 1 / coeficiente de seguridad, normalizada;
  - el relleno de cada capa sube por tres bandas (cargas de 0,15, 0,35 y 0,6) hasta esa densidad;
  - `src/infill/stress_density.*`: el campo, las regiones por corridas de vóxeles y las bandas.
- `slice_model` y `run_analysis` aceptan el campo. `analyze` usa el mismo relleno guiado en «como impresa».
- **Prueba:** en una barra en voladizo, la banda más cargada está junto al empotramiento, y hay más de 1,5 veces el
  relleno por área en el tercio empotrado que en el libre. Sin tensiones, igual.

**Medido** (análisis «como impresa», `docs/INFILL.md`):
- **Cubo comprimido:** el cúbico guiado del 10 al 50 % es un 60 % más rígido por material que el uniforme al 20 %; la
  grilla guiada da 6,65, la más alta del banco.
- **Barra en voladizo:** el cúbico guiado del 6 al 40 % iguala la rigidez del uniforme al 20 % con un 11 % menos de
  material y un 9 % menos de tiempo; el del 10 al 50 % es más rígido, más seguro y más rápido.

**Falta:**
- la interfaz: pasar el caso de la pestaña Análisis al laminado;
- iterar analizar-reforzar.

## 2026-10-25 — bloque nocturno: el relleno guiado por tensiones en la interfaz

**Hecho:**
- `Slicer::run` recibe un caso de carga. Con `infill.stress_density`, analiza la pieza maciza y lamina con el relleno
  guiado. Si el análisis falla, queda parejo con un aviso `stress_infill`.
- En Impresión:
  - **Guiado por tensiones** (densidad máxima);
  - **Caso de carga**, que abre un `.json` de caso con el explorador de archivos.
- **Prueba:**
  - con el caso, la barra tiene más de 1,5 veces el relleno junto al empotramiento;
  - sin caso, o con un caso que no la sostiene, queda pareja;
  - el G-code pasa el chequeo.

**La pestaña Análisis** solo estima vóxeles: editar apoyos y cargas sobre la pieza queda para su interfaz (la de la
sesión principal, o el rediseño del panel por niveles).

**Sigue:** lo que no espera las fichas del PR #15. Por ejemplo, el orden completo de ZAA del artículo, o
mantenimiento.

## 2026-10-25 — bloque nocturno: el orden de Z anti-aliasing con el grafo de alturas

**Medido primero:** una métrica nueva (`scratchpad`, de banco) del largo extruido más bajo que un cordón de la misma
capa puesto antes a menos de 0,7 mm. Es lo que hace que la boquilla aplaste el relieve (Song et al. 2017,
Sección 4). Con el orden de antes:
- solo bajando: 0 %;
- con subida, en la cúpula: 0,85 % (1444 mm);
- zona + pendiente + ZAA: 0,08 %.

**Hecho** (`docs/ZAA.md`, «El orden»):
- en una isla con caminos fuera del tope, cada camino se parte donde pasa de abajo del tope a sobre él o arriba;
- los pedazos de paredes y rellenos se ordenan juntos: por franjas de 0,005 mm, y con el grafo de alturas del
  artículo entre los que corren al lado, decidido por la mayoría de sus puntos más cercanos;
- también en la capa gruesa de la altura por zona.

**Medido:**
- cúpula con subida: 0,85 % → 0,00 %;
- zona + pendiente + ZAA: 0,08 % → 0,00 %;
- techo a 8° con subida: 7,1 % → 1,0 %;
- costo: un 10 % más de arranques y 0,3 min en la cúpula con subida; nada solo bajando.

**Prueba:** el techo a 8° queda por debajo del 1,5 %. Sin el orden falla, con un 7 %.

**Sigue:** partir donde el orden con un vecino se da vuelta; menos costuras.

## 2026-10-25 — bloque nocturno: mantenimiento corto

**Revisado:** después de las funciones nuevas de la noche (altura variable, ZAA, relleno combinado, denso, cúbico,
guiado y el orden por grafo):
- **fuzz de release:** 2100 corridas con tres semillas nuevas (1792909101–103);
- **fuzz con ASan y UBSan:** 150 corridas, sin hallazgos.

**Encontrado y arreglado:** una falla del fuzz (semilla 1792909102, corrida 456).
- **El caso:** una mesa con altura por zona (0,126 / 0,063) y ZAA al 60,3 % con subida.
- **Lo que pasaba:** un cordón de la capa gruesa bajó a Z3.030 bajo `;Z:3.081`, y el verificador lo rechazó.
  - El piso del escritor sale del tope real de la capa, 3,0806.
  - El verificador lo calcula desde el `;Z:` redondeado, con margen de un solo redondeo (0,001 mm).
- **El arreglo:** con ZAA, el margen de los límites de Z es el de los dos redondeos (0,002 mm).
- **Prueba:** la misma pieza y los mismos ajustes; falla sin el arreglo con el mismo mensaje.

**Después del arreglo:** la semilla, 700 corridas, 0 fallas.

## 2026-10-25 — bloque nocturno: partir donde el orden se da vuelta (descartado)

**Probado:** partir cada pedazo de ZAA donde su orden de altura con un vecino se da vuelta (los subcaminos de Song
et al. 2017, Sección 4.1) antes del grafo de alturas.

**Medido:**
- techo a 8° con subida: interferencia del 1,34 al 1,27 %, con un 65 % más de arranques de extrusión;
- cúpula con subida: un 4,6 % más de arranques, y la interferencia de 0,8 a 5,3 mm.

**Decidido:** no se adopta; el código vuelve al orden de c21e3a9. Lo que queda en el techo son líneas de arriba
cruzadas a la pendiente. Lo más prometedor parece orientarlas según las curvas de nivel (`docs/ZAA.md`).

**Sigue:** lo que no espera las fichas del PR #15.

## 2026-10-25 — bloque nocturno: cúbico adaptativo

**Hecho:** `infill.pattern` = `adaptive_cubic` (ficha `infill_patterns.md`, regla 7; `docs/INFILL.md`).
- `src/infill/adaptive.*`: la grilla de vóxeles del relleno disperso, la transformada de distancia euclídea exacta
  (Felzenszwalb y Huttenlocher 2012) y las líneas del cúbico por nivel (una de cada dos por cada nivel más adentro).
- El laminado arma la profundidad antes de las bandas guiadas, el denso y el combinado; las bandas y el denso van con
  el cúbico parejo a su densidad.
- `cell_region` (celdas de una grilla a polígonos) pasa a `src/geom/polygons.*` y la usan el relleno guiado y el
  adaptativo.
- En la interfaz, **Cúbico adaptativo** en el patrón de relleno; el fuzz sortea el patrón.

**Medido** (cubos al 20 %): material igual, −4 %, −11 % y −26 % en cubos de 20, 40, 60 y 100 mm; tiempo −5 %, −1 %,
−8 % y −13 %. El laminado del cubo de 100 mm pasa de 0,13 a 0,27 s.

**Pruebas:** `tests/test_adaptive.cpp`: la transformada contra todos los pares, la profundidad de un bloque, las
direcciones de P6, las líneas sobre la grilla del cúbico, un cubo de 60 mm a G-code verificado.

**Pendiente:**
- la prueba P6 de la ficha da en el cubo de 20 mm un 76 % del largo del cúbico, con la separación pareja; el nuestro
  da el del cúbico. Se ajusta la separación base cuando la ficha lo precise;
- la rigidez «como impresa» solo se puede medir en piezas chicas (el cubo de 60 mm pide 27 millones de vóxeles), donde
  el adaptativo casi no cambia nada.

**Sigue:** el cúbico de soporte (regla 8) y el rayo (regla 9).

## 2026-10-25 — bloque nocturno: cúbico de soporte, y la rigidez del adaptativo

**Hecho:** `infill.pattern` = `support_cubic` (ficha `infill_patterns.md`, regla 8; `docs/INFILL.md`): el cúbico
adaptativo con la profundidad medida desde lo que el relleno sostiene (los vóxeles justo encima de él) y desde las
paredes en el plano de cada vóxel, sin las capas sólidas de abajo.

**Medido** (al 20 %): cubo de 60 mm, −8 % de tiempo y −15 % de material contra el cúbico; cubo de 100 mm, −14 % y
−30 %. En el de 20 mm, igual que el adaptativo (P6).

**Rigidez del adaptativo** («como impresa», cubo de 40 mm comprimido): 0,096 mm contra 0,083 del cúbico, con un 4 %
menos de material: un 10 % menos de rigidez por material. Queda anotado en `docs/INFILL.md`: el adaptativo ahorra en
piezas grandes; para resistencia, el guiado por tensiones.

**Pruebas:** grueso sobre las capas sólidas de abajo, el cúbico bajo el techo, y el cubo de 20 mm igual al adaptativo.

**Sigue:** el rayo (regla 9).

## 2026-10-25 — bloque nocturno: relleno rayo

**Hecho:** `infill.pattern` = `lightning` (ficha `infill_patterns.md`, regla 9; `docs/INFILL.md`), según las
nervaduras de soporte de Tricard, Claux y Lefebvre 2020: de arriba hacia abajo, cada punto a sostener recibe una rama
unida a la rama o la pared más cercana, y las ramas de cada capa son las de arriba recortadas 45° en las puntas.
- `src/infill/lightning.*`: los árboles con raíz en la pared, el recorte, las raíces que se vuelven a unir, los
  puntos en una grilla con un umbral que garantiza el alcance, y la impresión por cadenas entre bifurcaciones.
- El laminado lo arma de una vez para todas las capas, después de las bandas guiadas y el denso (que van en cúbico);
  el relleno combinado no se usa con el rayo.
- En la interfaz, **Rayo** en el patrón de relleno; el fuzz sortea el patrón.

**Medido** (al 20 %): −30 %, −55 % y −64 % de material y −24 %, −41 % y −50 % de tiempo contra el cúbico en cubos de
20, 60 y 100 mm. El alcance como la separación del rectilíneo da 117 mm bajo el techo del cubo de 20 mm (P7: 134).

**Pruebas:** `tests/test_lightning.cpp`: el alcance bajo el techo de una caja, menos rama más abajo, el voladizo de
cada rama sobre la de abajo, el cubo de P7 creciendo hacia arriba con G-code verificado.

**Pendiente:** enderezar y juntar ramas, menos tramos (P7 tiene hasta 18, el nuestro 32), lo que Tisma Slicer
imprime cerca del fondo en P7, y un índice espacial para techos grandes (0,53 s en el cubo de 100 mm).

**Sigue:** los otros patrones de la ficha (`stars`, `honeycomb`, `concentric`…) o el medidor de rigidez sobre el
laminado real, mientras las fichas del PR #15 no estén en `main`.

## 2026-10-25 — bloque nocturno: estrellas, rectilíneo alineado y zigzag

**Hecho:** tres patrones más de la ficha `infill_patterns.md` (`docs/INFILL.md`):
- `stars` (regla 5): las tres familias de los triángulos con la segunda corrida media separación;
- `aligned_rectilinear` (regla 1): el rectilíneo con todas las capas a `θ`;
- `zigzag` (regla 2): igual al rectilíneo.

Los nombres de los patrones pasan a una sola tabla en `settings.cpp`, para leer y escribir.

**Pruebas:** zigzag igual al rectilíneo punto por punto (P1); el alineado a 45° en capas pares e impares (P2); las
estrellas con las tres direcciones de P4 y la segunda familia a media separación de la de los triángulos.

**Sigue:** el resto de la ficha (línea, panal, curvas, concéntrico, patrones de superficies, capa sólida cada N).

## 2026-10-25 — bloque nocturno: capa sólida cada N y espesor mínimo de la cáscara

**Hecho** (ficha `infill_patterns.md`, reglas 21 y 22; `docs/INFILL.md`):
- `infill.solid_every`: las capas N, 2N… pasan todo su relleno disperso a puente;
- `surfaces.top_min_thickness` y `surfaces.bottom_min_thickness`: más capas sólidas, del alto nominal, hasta el
  espesor;
- en la interfaz, las tres filas; el fuzz las sortea.

**Pruebas:** P12 (capas 5, 10… 95 en puente, sin relleno disperso, G-code verificado) y P13 (capas 0 a 2 y 95 a 99
sólidas con una capa y mínimos de 0,6 y 1 mm).

**Sigue:** línea, panal, curvas y concéntrico; los patrones de las superficies.

## 2026-10-25 — mantenimiento

**Revisado:**
- fuzz de release: 2000 corridas con cuatro semillas nuevas (1792909701–704), sin fallas;
- fuzz con ASan y UBSan: 150 corridas (semilla 1792909801);
- lo nuevo de la noche (cúbico adaptativo, de soporte, rayo, estrellas, capa sólida cada N) con entradas grandes:
  densidades altas en un cubo de 100 mm.

**Encontrado:** el rayo al 100 % en un cubo de 100 mm tardaba 63 s (al 50 %, 7,8 s).
- El rayo mismo, 1,6 s: cada punto miraba todas las ramas y las paredes.
- El escritor, 19,6 s: el orden por el comienzo más cercano miraba todos los recorridos de la capa para elegir cada
  uno, y bajo el techo hay miles de tramos cortos.

**Arreglado:**
- `order_paths` busca el comienzo más cercano en una grilla de extremos (más de 64 recorridos y costuras que no
  siguen al cursor), con el mismo resultado: entre iguales, el primer recorrido y su frente. El G-code de la esfera, la
  cúpula y los cubos del banco sale idéntico byte a byte (sin los comentarios).
- El rayo busca ramas y paredes en grillas. Con la grilla, las ramas igual de cerca se elegían en otro orden y salían
  cadenas largas a lo largo de las paredes que no se retiraban (la prueba de la caja lo detectó: 202 mm en la capa 0);
  ahora gana la rama más vieja, como antes, y el G-code del cubo al 20 % sale idéntico.

**Prueba nueva** (`tests/test_order.cpp`): 420 recorridos con los extremos en una grilla de 1 mm (muchos empates) se
ordenan igual por la grilla que mirando todos; con el desempate al revés, la prueba falla.

**Medido** (cubo de 100 mm, rayo): al 100 %, 63 s → 2,1 s (laminado 1,4 s, escritura 0,7 s); al 20 %, 0,53 → 0,39 s.
La esfera de 200 mm sigue igual (escritura 0,5 s): ahí el orden no pesaba.

**Recibido del equipo de fichas** (resultados de caja negra, sin código): el cúbico de soporte de Tisma Slicer se
mide en P6c y P6d de la ficha, en el PR #15. Se ajusta cuando esté en `main`.


## 2026-10-25 — bloque: relleno línea, panal y concéntrico

**Hecho** (ficha `infill_patterns.md`, reglas 3, 10 y 13; `docs/INFILL.md`), en `src/infill/patterns.*`:
- `line`: el rectilíneo con una línea de cada dos girada 6,7° sobre su punto en el medio de la región (+6,7° en las
  capas pares, −6,7° en las impares), unidas en zigzag por el borde;
- `honeycomb`: hexágonos con un centro en el origen, impresos como líneas onduladas en la dirección principal, que
  pasa por `θ` + 90°, + 30° y − 30° capa a capa; como la teselación no cambia al girarla 60° sobre un centro, los
  panales de todas las capas coinciden. El lado cuenta cada pared una vez, como mide Tisma Slicer (P8);
- `concentric`: lazos paralelos al borde cada `ancho / densidad`, de afuera hacia adentro;
- en la interfaz, **Línea**, **Panal** y **Concéntrico**; el fuzz los sortea.

**Medido** (cubo de 20 mm, ajustes de la ficha, relleno poco denso por capa en el G-code):

| Patrón | Tisma engine | Ficha |
|---|---|---|
| `line` | 182,4 a 185,0 mm en 1 tramo | P3: 185,9 mm en 1 tramo |
| `honeycomb` | 229,9 a 237,8 mm en 4 tramos | P8: 229 a 244 mm |
| `concentric` | 200,0 mm en 5 lazos | P9: 199,4 mm en 5 lazos |
| `rectilinear` (referencia) | 190,9 a 194,0 mm | P1: 197,3 mm |

Tiempo estimado del cubo: rectilíneo 15 min 34 s, línea 14 min 44 s, panal 18 min 51 s, concéntrico 16 min 32 s.

**Pruebas** (`tests/test_combine.cpp`): P3 (las dos direcciones de cada capa, la mitad cada una, pocos tramos, el largo
del rectilíneo), P8 (la principal 15°, 135° y 75° en las capas 5, 6 y 7 con la mitad del largo, las otras un cuarto,
1,25 veces el rectilíneo; las cuentas de dos capas coinciden en más del 90 % del área, y con la grilla corrida medio
lado no), P9 (5 lazos cerrados del largo esperado, iguales en otra capa) y los tres laminados en un cubo con G-code
verificado. `tools/ci_linux.sh` pasa: 314 pruebas con gcc, clang y ASan + UBSan. Una prueba vieja usaba
`honeycomb` como patrón desconocido; ahora usa `voronoi`, que no está en la ficha.

**Sin medir ni probar:** las líneas de `line` cruzan a sus vecinas en regiones de más de unas 17 separaciones a lo
largo de las líneas (34 mm al 20 %); la ficha no mide piezas tan grandes. No probado en una impresora.

**Sigue:** panal 3D (regla 11), Hilbert, cuerdas de Arquímedes y octagrama (12), y los patrones de las superficies
(14 a 17). El cúbico de soporte espera las pruebas P6c y P6d del PR #15.

## 2026-10-25 — bloque: Hilbert, cuerdas de Arquímedes y octagrama

**Hecho** (ficha `infill_patterns.md`, regla 12; `docs/INFILL.md`), en `src/infill/patterns.*`:
- `hilbert`: la curva de Hilbert por celdas de `ancho / densidad`, recorrida en forma recursiva solo cerca de la región,
  sobre una grilla fija de 2 m alrededor del origen girada `angle`: igual en todas las capas;
- `archimedean`: cuerdas de la espiral `r = a·φ` con vueltas a `ancho / densidad` alrededor del centro de la pieza;
- `octagram`: una espiral con el contorno de dos cuadrados girados 45°, con cada lado sobre una recta de dirección fija
  que se aleja un dieciseisavo de la separación por lado (la primera versión, con el radio creciendo entre vértices,
  dejaba solo el 87 % del largo en las cuatro direcciones; la prueba lo detectó);
- el laminado pasa el centro de la pieza a las espirales; en la interfaz, **Hilbert**, **Cuerdas de Arquímedes** y
  **Espiral octagrama**; el fuzz los sortea.

**Medido** (cubo de 20 mm, ajustes de la ficha): Hilbert 195,4 mm en 2 tramos (P9: 206,3), Arquímedes 185,1 en 3 (P9:
194,3), octagrama 172,1 en 3 (P9: 208,3); el rectilíneo, 190,9 (P1: 197,3). El octagrama de Tisma Slicer pone un 21 %
más: queda por contrastar cómo cuenta la separación.

**Pruebas** (`tests/test_combine.cpp`): las tres iguales en dos capas, el largo cerca del rectilíneo, pocos tramos;
Hilbert mayormente a 45° y 135°, el octagrama con más del 95 % del largo a 0°, 45°, 90° y 135°, las cuerdas en todas
las direcciones; las tres laminadas a G-code verificado.

**Pedido nuevo del dueño:** rellenos no planares más fuertes que los tradicionales; es lo que sigue.

**Sigue:** el panal 3D (regla 11) necesita más mediciones de la ficha; los patrones de las superficies (14 a 17).

## 2026-10-25 — rellenos no planares y brick layers: lo publicado y la propuesta

**Pedido del dueño:** rellenos no planares más fuertes que los tradicionales, y una versión no planar de brick layers
que no infrinja patentes.

**Hecho:** `docs/NONPLANAR_INFILL.md`, con:
- lo publicado y medido: ZigZagZ (+62 % de resistencia y +245 % de tenacidad en Z), pernos en Z (más de 3,5 veces
  en Z), capas onduladas (+50 % al impacto) y brick layers (+10 a 14 %);
- las patentes de brick layers según sus resúmenes públicos (Stratasys, vencida; Addman, vigentes);
- la propuesta: N1, relleno ondulado; N2, paredes tejidas, la versión no planar de brick layers; N3, medir con el
  análisis; N4, pernos en Z si la patente lo permite.

**Sin verificar:** las reivindicaciones completas de las patentes de Addman y de ORNL. Los sitios de patentes bloquean
el acceso automático; lo dicho sale de los resúmenes. Antes de vender, que lo revise un abogado.

**Sigue:** N1.

## 2026-10-25 — bloque: relleno ondulado (N1)

**Hecho** (`docs/NONPLANAR_INFILL.md`, N1; `src/infill/nonplanar.*`): `infill.nonplanar`, el relleno que sube y baja
en Z según un campo fijo en el espacio, igual en todas las capas (capas anidadas, como ZigZagZ).
- La amplitud se apaga cerca de paredes y pieles con el campo de profundidad (interpolación trilineal nueva en
  `DepthField::at`), y se limita para que la boquilla pase sobre una línea vecina en cresta.
- Cada punto lleva la altura del cordón de abajo (`below`); el escritor pone los recorridos ondulados en un bloque
  `;NONPLANAR` con el espesor por movimiento, y el verificador y la vista previa lo leen.
- En la interfaz, **Relleno ondulado** con su altura y su largo; el fuzz lo sortea.

**Medido** (cubo de 40 mm, grilla al 20 %): 2 h 18 min contra 1 h 15 min plano (el eje Z a 12 mm/s), G-code de
11,5 MB contra 0,47 MB (de 21 MB antes de quitar los cortes en recta), el mismo filamento.

**Pruebas:** `tests/test_nonplanar.cpp` (4 casos); 319 pruebas en total; fuzz de 400 corridas sin fallas.

**Sin medir:** la resistencia (falta N3) y una impresión real.

**Sigue:** N3 (medir con el análisis), después N2 (paredes tejidas). El dueño pidió también buscar artículos sobre la
mejor manera de laminar no planar.

## 2026-10-25 — laminado no planar: los artículos

**Pedido del dueño:** buscar artículos sobre la manera más óptima de laminar no planar.

**Hecho:** `docs/NONPLANAR_SLICING.md`, con:
- **en 3 ejes:** CurviSlicer (con las mismas capas, error de superficie de 57 mm³ contra 149 de la altura adaptativa),
  techos curvos (Ahlers 2019), capas isotérmicas y no planar con ángulos optimizados;
- **en 5 ejes:** Reinforced FDM (hasta 6,35 veces la carga), S³-Slicer y Neural Slicer;
- **la propuesta para F7 y F9:** el motor ya corta capas como superficies de nivel de un campo, y todos estos métodos
  dan ese campo. Se suma la idea propia de un término de resistencia con el análisis A1 en 3 ejes.

**Sala limpia:** CurviSlicer y S³-Slicer publicaron código; se usan solo los artículos.

**Pedido nuevo del dueño:** artículos sobre soportes no planares, Arc Overhang, y soportes más fáciles de sacar con
mejor superficie debajo. Sigue con eso.

## 2026-10-25 — soportes: los artículos

**Pedido del dueño:** artículos sobre soportes no planares, Arc Overhang, y soportes más fáciles de sacar con mejor
superficie en la parte soportada.

**Hecho:** `docs/SUPPORT_RESEARCH.md`, con:
- **voladizos sin soporte:** Arc Overhang (con la ficha `arc_overhangs.md` que ya está en `main`) y laminado cónico
  (Wüthrich et al. 2021);
- **menos soporte:** árbol (Vanek et al. 2014: −40,5 % de material), ramificado (Schmidt y Umetani 2014: −75 %) y
  andamios con puentes (Dumas et al. 2014);
- **sacar y superficie:** el patrón y la separación en Z son lo que más pesa; una interfaz de otro material baja la
  fuerza para sacar el soporte un 63 % y la rugosidad mucho, a costa de purgas.

**Propuesta, por orden (S1 a S5):**
1. arcos en voladizos;
2. la superficie soportada, con una interfaz no planar propia que sigue la cara de abajo;
3. árbol;
4. laminado cónico;
5. interfaz de otro material.

No encontré publicada la interfaz no planar.

**Sigue:** S1, arcos en voladizos (ficha `arc_overhangs.md`).


## 2026-10-25 — simulación del enfriamiento: los artículos y el plan

**Pedido del dueño:** simular cómo se enfría el filamento, para no perder calidad en los detalles muy chicos de capas
que sí llegan al tiempo mínimo.

**Hecho:** `docs/COOLING_SIMULATION.md`, con:
- **por qué pasa:** el tiempo mínimo mira la capa entera, y el detalle puede tener mucho menos. Hay cuatro causas:
  - el detalle se imprime último en una capa y primero en la siguiente;
  - no tiene masa debajo que se lleve el calor;
  - la boquilla lo recalienta mientras pasa;
  - sus cordones se ponen uno junto al otro;
- **lo publicado:**
  - la red de segmentos de filamento con contactos y criterio de adhesión (Costa, Duarte y Covas 2017);
  - la simulación en tiempo lineal (Zhang y Shapiro 2018);
  - las mediciones de Zhang et al. 2022: lo que manda es el tiempo entre pasadas;
  - la unión según la historia de temperatura (Bellehumeur et al. 2004);
- **patentes:** dos solicitudes de Siemens reordenan tramos o zonas de la capa según el calor. El motor no reordena por
  el calor sin la revisión del abogado;
- **el plan, de T1 a T4:**
  1. el tiempo de vuelta local, con frenado, pausa y ventilador solo en el detalle;
  2. la red térmica de cordones, calibrada con pernos;
  3. las correcciones por temperatura, y la adherencia por unión en el análisis;
  4. la GPU y el aire de A3.

  La red trabaja en 3D, así que sirve también para 4 y 5 ejes.

**Rumbo del dueño:** un laminador que use las técnicas que más mejoran la calidad y la resistencia, compatible con una
impresora propia de 4 y 5 ejes. Quedó en `PLAN.md`:
- F9 cubre 4 y 5 ejes;
- la cinemática suma la boquilla inclinada que gira, y el G-code puede salir en coordenadas de la punta o en ejes de la
  máquina;
- la impresora del dueño es candidata a máquina de referencia.

**Sin código todavía:** T1 es lo primero que se puede hacer, en el escritor.

**En curso:** el pedido nuevo del dueño, artículos de FDM y de técnicas de laminado para mejorar funciones, calidad y
rendimiento. Va en cuatro búsquedas: trayectorias y superficie, movimiento y extrusión, resistencia y deformación, y
rendimiento del cómputo.

## 2026-10-25 — FDM y técnicas de laminado: el mapa de lo publicado

**Pedido del dueño:** artículos de FDM y de técnicas de laminado para mejorar lo más posible las funciones, la
calidad y el rendimiento.

**Hecho:** `docs/RESEARCH_MAP.md`, en cuatro búsquedas en paralelo (solo artículos, registros de patentes y
documentación de usuario; ningún código):
- **trayectorias y superficie:** costura en bisel y su punto débil, esquinas, ancho variable, puentes, voladizos,
  compensación en XY según el ángulo, rugosidad, planchado;
- **movimiento y extrusión:** estimadores de tiempo con 1,7 a 8,5 % de error contra 48 % de otros, flujo máximo según
  la temperatura, avance de presión, orden de viajes;
- **resistencia y deformación:**
  - en PLA, la unión entre capas llega a la resistencia del material;
  - con cordones de 2,5 veces la boquilla: +40 a +48 % y −67 % de tiempo;
  - cordones según la tensión, propiedades ortótropas, alabeo, interfaz ITIL;
- **cómputo:** corte robusto, número de vueltas para mallas rotas, transformada de distancia en GPU, Voronoi robusto,
  orden de viajes que escala, G-code binario.

**Propuesta:** Q1 a Q12 en `PLAN.md`, por ganancia medida contra esfuerzo. Lo primero: el modo resistencia con
cordones anchos, el estimador de tiempo que imita al firmware y los topes por fusión.

**Patentes que se evitan:** las B-splines filtradas (Universidad de Michigan), las espirales de Fermat conectadas
(Shandong), los tramos para recorridos eulerianos (UT-Battelle), los detalles locales a través de capas (Tulane) y
otras en la sección 6.

**Corregido en T3** (`docs/COOLING_SIMULATION.md`): el criterio de unión fría pesa poco en PLA y más en ABS y en los
materiales técnicos.

**Para el dueño:** la especificación del G-code binario de Prusa está dentro de un repositorio AGPL. Hace falta su
decisión para leerla; si no, se estudia como caja negra.

**Sigue:** el bloque de mantenimiento programado, después S1 (arcos en voladizos).

## 2026-10-25 — mantenimiento (16:40)

**Revisado:**
- búsqueda aleatoria de G-code: 2000 corridas con cuatro semillas nuevas (1792947616 a 1792947619), sin fallas;
- lo nuevo desde el último mantenimiento, con entradas raras:
  - Hilbert, cuerdas de Arquímedes y octagrama: piezas lejos del origen, densidad mínima y máxima, regiones de 0,5 a
    300 mm, centro fuera de la pieza;
  - relleno ondulado: ajustes armados en código, sin pasar por los rangos del JSON;
- los «Sin medir» de `PROGRESS.md`: las líneas del patrón `line` se cruzan en regiones de más de unas 17
  separaciones. Es lo que dice la ficha (una de cada dos inclinada 6,7°); los cruces son como los de la grilla. Queda
  como pregunta de comportamiento para el equipo de fichas, sin cambio.

**Encontrado y corregido:**
- **El relleno Hilbert desaparecía lejos del origen.** La grilla de la curva cubría 2 m alrededor del origen. Girada
  45°, una pieza cerca de (900, 900) quedaba afuera: sin relleno en las esquinas de una cama de 1 m. Ahora cubre 16 m
  y crece si la región sale de ahí; cuesta lo mismo. Prueba nueva en `tests/test_combine.cpp`: el mismo largo en el
  origen y a 900, 1500, −1900, 7000 y 12 000 mm, igual en dos capas. Con el código anterior falla.
- **Relleno ondulado con longitud de onda 0** (ajustes armados en código): se marcaba ondulado sin onda, con una
  conversión de infinito a entero sin definir. Ahora `wave_params` lo deja plano; la prueba lo cubre también con NaN.

**Medido** (4 núcleos, Release, CPU):
- esfera de banco de 100 mm (319 200 triángulos, 500 capas, 884 571 líneas): laminado 0,85–0,90 s, escritura
  0,62–0,66 s, verificación 0,14–0,18 s. Igual que el mantenimiento anterior;
- análisis del cubo de prueba: solución 194–225 ms (22 iteraciones, 3 niveles). El código no cambió, así que es
  variación de la máquina;
- **lo más lento encontrado:** Hilbert y cuerdas de Arquímedes en regiones grandes y densas. El recorte con Clipper2
  crece más que lineal, porque la lista de aristas activas es ancha (callgrind: 54 % en insertar mínimos locales):
  - 150 mm al 100 %: 0,14 s por capa;
  - 300 mm al 100 %: 1,2 s por capa;
  - 300 mm al 20 %: 13 ms por capa.

  Quedó en `PLAN.md` (recortar por franjas). No se arregló en este bloque: hay que volver a unir los pedazos en los
  bordes de las franjas, y es un caso raro (relleno ralo al 100 %).
- **Advertencia de clang** (`-Wsign-conversion`) en una prueba del espesor mínimo de la cáscara
  (`tests/test_combine.cpp`), de un bloque anterior: corregida.
- **Búsqueda aleatoria con ASan y UBSan:** 150 corridas (semilla 1792949149), sin fallas.

**Sigue:** S1, arcos en voladizos (ficha `arc_overhangs.md`).

## 2026-10-25 — revisión profunda de errores (primera tanda de arreglos)

**Pedido del dueño:** «una revisión general de bugs profunda, así corrijo errores graves».

**Cómo:**
- seis revisores en paralelo, uno por área: corte y geometría, trayectorias, G-code, ajustes y archivos, análisis /
  GPU / conexión, e interfaz;
- cada hallazgo, probado con una entrada concreta contra la compilación con ASan y UBSan;
- además, la revisión de caja negra del equipo de fichas: unos 420 laminados desde la línea de comandos y una tanda
  de rendimiento contra Tisma Slicer.

**Arreglado en esta tanda,** cada uno con su prueba (`tests/test_hostile.cpp`, `tests/test_device.cpp`,
`tests/test_combine.cpp`), detalle en `CHANGELOG.md`:
- **conexión:** cable desenchufado, «ok» perdido que cancelaba la impresión y dejaba los calentadores encendidos,
  pausa o cancelación que pasaba al trabajo siguiente, carrera de datos en `job_`, números de reenvío sin definir;
- **G-code peligroso:** variables con saltos de línea que metían movimientos después de la impresión, índice enorme;
- **ajustes:** alturas, anchos, elevación y retracción contra la máquina (`check_settings`), límites de lectura, avisos
  de temperaturas de los cambios y de anchos raros, porcentajes infinitos;
- **archivos:** JSON anidado que desbordaba la pila, excepciones de JSON como errores internos;
- **calibraciones:** rangos de las torres y del valor elegido, tope de temperatura, etiquetas de tres decimales;
- **paredes:** nodos del Voronoi propio fuera de la bisectriz, el «no seed node» y la recursión del marcado: el motor
  detecta el diagrama malo y usa el de referencia (Boost.Polygon) para esa región;
- **rellenos:** la recta desde el centro del espiral (Arquímedes y octagrama);
- **análisis:** piezas libres de moverse, que antes corrían horas, y cargas sobre la sujeción;
- **mallas:** piezas lejos o altas, conteo de 64 bits, bucle de la altura variable, `info` con piezas planas;
- **exportación:** sin archivo viejo con el nombre pedido cuando falla;
- **verificador:** la falda con esquinas en punta;
- **interfaz:** los siete de su revisor, entre ellos la temperatura de la cama atada a la de la boquilla y los
  cambios de una torre que quedaban en el modelo siguiente.

**Probado:** 332 pruebas en una compilación aparte. Las piezas de los revisores que se colgaban o se caían (`seed145`,
`seed58`, `rand74`, `vnotch`, `seed785`) laminan en 0,1 s. Las pruebas nuevas de conexión, espiral y Hilbert fallan
con el código anterior.

**Sigue, de la misma revisión:**
- **graves:** el Z anti-aliasing con «raise» imprime dos veces sobre los mismos cordones cuando la capa siguiente es
  más fina; una sola cara con la orientación invertida hace desaparecer la pieza;
- **medios:**
  - el escritor no se resincroniza después del G-code de un cambio por altura;
  - las líneas sólidas se ajustan a la capa entera y no a cada isla;
  - el proyecto puede repetir una malla muchas veces;
- **rendimiento** (equipo de fichas y revisores):
  - ordenar muchas islas cuesta más que lineal;
  - las paredes duplican vértices;
  - el gyroid escribe el triple de tramos y usa mucha memoria;
  - las uniones del zigzag son cuadráticas;
  - el G-code ocupa la memoria entera;
- **otros:** aviso de `infill.nonplanar` ignorado; `infill.solid_below_area` de la ficha.

## 2026-10-25 — revisión profunda de errores (segunda tanda)

**Arreglado,** cada uno con su prueba, que falla con el código anterior donde se pudo comprobar:
- **Z anti-aliasing con «raise»:** la subida llega como mucho a la mitad de la capa siguiente. Antes, con una capa
  siguiente más fina, las dos imprimían la misma superficie: en una placa de 0,42 mm con primera capa de 0,3 mm, la
  primera capa subía a 0,42 y la segunda pasaba por encima (+29 % de material);
- **orientación:** `orient_consistently` da la misma orientación a toda la malla y vuelve la pieza hacia afuera si
  hizo falta reparar algo. Una cara invertida borraba el cubo de 20 mm de la prueba del revisor;
- **escritor:** después del G-code de un cambio por altura vuelve a fijar los modos y E, retrae y escribe los ejes
  completos;
- **otros:** ventilador `S1` en RepRapFirmware, líneas sólidas ajustadas por isla, mallas repetidas en un proyecto,
  mensajes de pieza vacía, y aviso del relleno ondulado ignorado.

**Medido** (196 cilindros de 4 mm, 100 capas, 4 núcleos):

| Fase | Antes | Ahora | Por qué |
|---|---|---|---|
| Escritura | 19,6 s | 8,4 s | El 77 % era la costura alineada: cada esquina de cada lazo contra todas las costuras de abajo. Ahora busca en una grilla |
| Laminado | 14,1 s | 10,0 s | Las uniones del zigzag miraban todo el borde por cada extremo; ahora usan una grilla |

El G-code sale idéntico byte a byte. La elección de islas con cajas también da el mismo resultado; no era lo lento.

**Sigue:** lo que quedó en `PLAN.md`, con la resolución del G-code primero. Después, la release 0.8.0-alpha.1.

## 2026-10-25 — resolución del G-code y paredes partidas

**Hecho:**
- **Resolución del G-code** (`gcode.resolution`, 0,0125 mm, de la ficha `gcode_extras.md`; `src/gcode/resolution.*`,
  fila nueva en la sección Impresora): cada movimiento llega tan lejos por el camino como lo permite la tolerancia,
  sin cambiar el ancho, la Z ni el cordón de abajo más de 0,0005 mm. La costura queda. Va en el perfil de impresora.
- **Paredes partidas** (encontrado al medir lo anterior): en cada esquina cóncava y en cada punto en medio de un lado
  recto, la pared se cortaba en pedazos abiertos, porque los dos radios de esa esquina en un nodo leían puntos de
  apoyo que difieren en fracciones de nanómetro. Un lazo abierto no tiene costura, así que la costura alineada no se
  aplicaba. `extract_toolpaths` junta ahora de a pares los radios de un nodo con un solo trapecio cada uno y apoyos a
  menos de 1e-4 mm. Un polígono de 128 lados con muescas de 1 µm salía en 205 pedazos por pared, y un rombo con cuatro muescas de
  0,5 mm en 8; ahora, en uno.
- Pruebas nuevas: `tests/test_resolution.cpp` (4) y una de paredes con esquinas cóncavas y rectas, que falla con el
  código anterior; la de Z anti-aliasing con la resolución puesta.
- El test de dispositivo que falló en la CI de Windows (una pausa al final de un trabajo): la impresora simulada
  tarda 2 ms por movimiento, como en la otra prueba de pausa.

**Medido** (Release, 4 núcleos; slice entero con escritura):

| Modelo | Extrusiones antes | Ahora | Archivo antes | Ahora | Tiempo antes | Ahora |
|---|---|---|---|---|---|---|
| Cilindro de 128 lados (C17, `simplify_tolerance` 0) | 41 859 | 6 140 | 1,25 MB | 0,19 MB | 0,15–0,19 s | 0,12–0,13 s |
| Esfera de 50 mm, giroide | 269 112 | 73 153 | 7,7 MB | 2,1 MB | 0,70–0,77 s | 0,60–0,62 s |
| Esfera de 50 mm, por defecto | 292 907 | 88 907 | 8,4 MB | 2,6 MB | 0,72–0,76 s | 0,63–0,66 s |
| 12 islas | 18 701 | 13 301 | 0,52 MB | 0,41 MB | 0,07–0,09 s | 0,07–0,08 s |

- El perímetro externo del cilindro: 64 movimientos por vuelta (la ficha da 65 para Tisma Slicer), en un solo lazo;
  antes 556 en 38 pedazos. Tramos de menos de 0,01 mm: de 3229 a ninguno.
- El largo extruido cambia menos de 0,02 % (la cuerda en vez del arco).
- Partir en el punto más alejado (Douglas y Peucker) por tramos de 256 o 1024 puntos dejaba 12 a 27 % más
  movimientos que el alcance más lejano; quedó este.
- El tiempo estimado del cilindro sube de 4 min 8 s a 4 min 14 s: con lazos cerrados, cada pared empieza en su
  costura y hay más viajes con retracción.

**No medido:** una impresión real con la resolución; el efecto en placas con poca memoria de planificación.

**Sigue:** la release 0.8.0-alpha.1 con estos arreglos, después lo que queda de la revisión en `PLAN.md`.

## 2026-10-25 — release 0.8.0-alpha.1 y zonas chicas sólidas

**Hecho:**
- Release 0.8.0-alpha.1: `VERSION`, su sección del CHANGELOG y la fila de `docs/VERSIONING.md`; la CI de Linux pasó
  entera en la sesión y la de Windows con el commit anterior.
- `infill.solid_below_area` (70 mm², ficha `surfaces_infill.md`, regla 9): la parte dispersa de cada isla del relleno
  más chica que eso pasa a sólida. Prueba nueva con la pata de la mesa de la prueba I6 (zona de 62,6 mm², sólida en
  todas las capas) y el cubo de 20 mm (335 mm², disperso). Cinco pruebas con el cubo de 10 mm (57,8 mm² de relleno)
  lo apagan para seguir probando lo suyo.
- `check_settings` nombra primero un alto mínimo de capa mayor que el máximo (equipo de fichas).

**Medido:** la placa de 12 islas pasa de 69 750 a 111 570 mm extruidos (+60 %) y de 34 min 50 s a 52 min 49 s; la
esfera de 50 mm, +0,07 % (sus casquetes). El cilindro de 20 mm de diámetro no cambia.

**Medido por el equipo de fichas** (caja negra sobre 7f745d8, Release):
- ThreadSanitizer: 0 avisos en 14 laminados y 3 análisis.
- 400 cilindros: de 44,8 a 38,6 s en total; la verificación, de 6,96 a 3,77 s; la memoria, de 2283 a 2016 MB; el
  G-code, de 386 a 229 MB.
- Ningún tramo que extruye mide menos de 0,0125 mm, y ninguno mide cero.
- La verificación sigue creciendo más que lineal (×32 de 25 a 400 cilindros).

**Sigue:** lo que queda de la revisión en `PLAN.md` (verificación con muchas islas, `info`, gyroid por franjas,
streaming del G-code).

## 2026-10-25 — vista previa realista, ray tracing continuo, herramientas de la placa y brillo

Pedidos del dueño, con una foto de una esquina de la vista previa.

**Hecho:**
- **Vista previa:** los tramos seguidos se unen en inglete. Ya no sobresalen dientes en las curvas, y las caras
  encimadas no titilan. La luz sigue el perfil de un cordón impreso (`docs/UI.md`). `--look` acerca la cámara de las
  capturas.
- **Ray tracing:** el cordón es un poliedro convexo de seis planos, igual en la CPU y en la GPU (Haines 1991). Las
  sombras de una pared son continuas. `--ray-check`: la GPU y la CPU coinciden en el 99,999 % de los píxeles.
- **Herramientas de la placa:** varios modelos; Agregar, Quitar, Duplicar, Mover (y arrastrar), Rotar, Escalar, Apoyar
  en una cara y Organizar (ficha `placement.md`); íconos propios en `assets/icons/`; los proyectos guardan todos los
  modelos. `src/ui/plate.*` con 5 pruebas; `ui_plate_save`, `ui_plate_reopen` y `ui_plate_same_gcode`: dos cubos
  ordenados, exportados, guardados y vueltos a abrir dan el mismo G-code.
- **Brillo parejo:** `docs/GLOSS.md` con lo que se sabe (la temperatura de salida baja con el flujo; las capas que se
  frenan salen más brillantes) y lo que el motor puede hacer (G1 a G7). No hay mediciones públicas del brillo de piezas
  FFF sin pintar: hace falta una pieza de prueba y un medidor.

**Medido:** placa de 2 cubos de 10 mm, ordenada: centros a 6 mm entre caras, G-code idéntico al reabrir el proyecto.

**No probado:** las herramientas con una persona usando el mouse (solo capturas y pruebas automáticas); el
arrastre con modelos de más de un millón de triángulos (se recalcula la malla en cada cuadro).

**Sigue:** pintar (soportes y costura, con el motor), G1 y G2 del brillo, el rendimiento con modelos grandes y la
decisión del dueño sobre los perfiles abiertos.

## 2026-10-26 — brillo parejo: la pared externa no se frena (G1 y G2)

**Hecho:** para llegar al tiempo mínimo de capa, el escritor frena primero todo lo que no es pared externa (paredes
internas, relleno, soporte), sin bajar de la velocidad mínima. La pared externa sigue a su velocidad. Si eso no
alcanza, se frena todo como antes. `cooling.keep_outer_speed` (encendido por defecto) lo apaga. `src/gcode/cooling.*`
con una prueba nueva en `test_cooling.cpp`; fila en la interfaz.

**Medido:** cubo de 10 mm: la pared externa a 1800 mm/min en todas las capas (antes, 1651 a 1687 según la capa), con
el mismo tiempo total (6 min).

**No probado:** el brillo de una pieza impresa (hace falta la pieza de prueba de `docs/GLOSS.md` y un medidor).

**Sigue:** pintar (soportes y costura, con el motor), esperar lejos de la pieza cuando frenar lo de adentro no
alcanza.

## 2026-10-26 — pintar sobre el modelo: soportes y costura

Pedido del dueño: las herramientas de todos los slicers, también pintar.

**Hecho:**
- **Motor** (`docs/PAINTING.md`):
  - `src/mesh/paint.*`: un árbol de marcas por triángulo y canal (soportes, costura, piel difusa, colores). Un
    triángulo se corta en cuatro donde pasa el borde del pincel, sin tocar la malla. Pincel esférico que no pinta las
    caras de atrás.
  - `src/slice/painted.*`: lo pintado de cada capa, proyectado y agrandado.
  - Soportes forzados y bloqueados (ficha `supports_more.md`, reglas 3 a 5) y el ajuste `support.automatic`.
  - Costura forzada (en el medio del tramo pintado con la costura alineada o atrás) y bloqueada.
  - Parte `PNT ` en el proyecto; un número de triángulo fuera de la malla se rechaza antes de reservar memoria.
- **Interfaz:** herramienta Pintar (P): canal, Forzar o Bloquear, tamaño del pincel y Borrar todo. Las marcas se ven
  sobre el modelo (verde y rojo). `--paint` pinta al abrir, para capturas y pruebas.
- **Pruebas:** `tests/test_paint.cpp` (7 casos: pincel, bytes, capas, soportes, costura en los cuatro modos, un cubo
  con estante y un G-code real), una de la placa, una del proyecto, una hostil y `ui_paint_save`, `ui_paint_reopen`,
  `ui_paint_same_gcode`.

**Medido:** cubo de 10 mm con una franja de costura pintada en una cara: las 50 paredes externas empiezan en la franja
(sin pintar, en la esquina de atrás); el proyecto guardado y vuelto a abrir da el mismo G-code. El pincel en una
esfera de 1 002 528 triángulos (Release, un hilo): 13 a 18 ms por paso, porque mira todos los triángulos; recorrer lo
pintado para dibujarlo, 6 ms.

**No probado:** pintar con el mouse una persona (solo capturas y pruebas automáticas); el clic que busca la cara bajo
el mouse en esa esfera (también mira todos los triángulos).

**Sigue:** la piel difusa del motor y su pincel; colores con varios extrusores; volúmenes bloqueadores; el rendimiento
con modelos grandes; la decisión del dueño sobre los perfiles abiertos.

## 2026-10-26 — modelos grandes en la interfaz

Pedido del dueño: mejorar el rendimiento con modelos grandes.

**Medido** con una esfera de 1 002 528 triángulos (Release):
- el motor no es el problema: leer 83 ms, laminar 2,0 s, G-code 0,5 s, verificar 0,04 s, vista previa 0,06 s;
- en la interfaz, arrastrar el modelo rehacía en cada cuadro la malla ubicada (75–89 ms), la de la placa (69–75 ms) y
  la copia de la vista (45 ms): 5 cuadros por segundo;
- cada clic y cada cuadro del pincel miraban todos los triángulos: 57 ms.

**Hecho:**
- Arrastrar corre el modelo con un desplazamiento en el shader; la malla se rehace una vez, al soltarlo.
- `TriangleIndex` (`src/ui/plate.*`): un árbol de cajas por modelo, armado cuando hace falta (250 ms). Un clic pasa de
  57 ms a 3 µs; un paso del pincel, de 13–18 ms a 2 ms (`paint` recibe los triángulos cercanos). Prueba nueva: el árbol
  da el mismo triángulo y la misma distancia que mirarlos todos, en 400 rayos.

**No probado:** cuadros por segundo reales con una GPU (en la sesión solo hay llvmpipe, donde dibujar la vista previa
de esa esfera con MSAA 4 tarda más que todo lo anterior).

**Sigue:** armar el árbol y la malla de la placa en segundo plano; los campos de giro y escala sin rehacer toda la
placa.

## 2026-10-26 — piel difusa

Para que el pincel de Pintar tenga su tercer canal, y la ficha `fuzzy_skin.md`.

**Hecho:**
- `src/perimeters/fuzzy.*`: la pared externa pasa por puntos nuevos cada 0,5 a 1,5 veces `fuzzy.point_distance`,
  corridos al azar hasta `fuzzy.thickness` hacia afuera o hacia adentro (splitmix64: el mismo G-code en todos los
  compiladores). `external` deja lisos los agujeros, `all` no. Las paredes internas no cambian.
- Pintada: donde se pintó, aunque `fuzzy.mode` sea `none` (regla 4); el resto del lazo queda con sus puntos.
- Ajustes `fuzzy.mode`, `fuzzy.thickness`, `fuzzy.point_distance` (perfil de impresión), filas en la interfaz bajo
  Costura, y el canal Piel difusa de Pintar habilitado (`--paint fuzzy,...`).
- `region_contains` en `src/geom/polygons.*`, que ahora comparten la costura pintada y la piel difusa.
- Pruebas (`tests/test_fuzzy.cpp`): F1 en un cilindro, F2 en un tubo, y la pintada en una mancha.

**Medido:** cilindro de 20 mm, capa 10, `external`, 0,3 y 0,8: la pared externa de 9,51 a 10,08 mm del centro (la
línea a 9,8; F1 de la ficha: de 9,49 a 10,08), tramos de 0,92 mm en promedio y 1,92 como máximo (F1: 0,85 y 1,8); las
internas siguen a 9,0 y 9,4. G-code verificado sin problemas.

**No probado:** la regla 3 (lo que sale de la capa de abajo como voladizo: falta el voladizo de `walls_extra.md`) y las
paredes de las zonas gruesas de `docs/LOCAL_LAYERS.md`, que todavía no se hacen rugosas.

**Sigue:** el tutorial de cada función la primera vez que se usa y el botón de información al lado de cada ajuste
(pedido del dueño).

## 2026-10-26 — ayuda en la interfaz y mantenimiento

**Ayuda** (pedido del dueño: un asistente, un tutorial de cada función la primera vez y un botón de información al
lado de cada ajuste; `docs/UI.md`, «Ayuda»):
- asistente de siete pasos la primera vez (sin preferencias guardadas) y desde **Más > Tutorial**;
- consejos la primera vez que se usa cada herramienta de la placa, Pintar, el laminado y cada pestaña; se apagan, y
  lo visto se guarda (`tips`, `tips_seen`);
- botón (i) al lado de cada ajuste: qué hace, clave, valor por defecto y rango, leídos de `docs/CONFIG.md`, que el
  programa lleva adentro (`src/ui/help.*`). La prueba nueva falla si un ajuste del motor no tiene su fila.
- `--tips`, `--tour` e `--info` para las capturas; `ui_help_tour` y `ui_help_tip_and_info`.

Llegó la ficha `assistant.md` (con el PR #22): pide además un panel de ayuda con datos del paquete libre. Lo hecho es
el primer paso; lo que falta está en `PLAN.md`, «Fichas nuevas».

**Mantenimiento** (bloque de las 8 horas):
- fusionado `origin/dev` (PR #22, fichas nuevas) sin rebase;
- `tisma_fuzz_gcode 2000` con la semilla 20261026: 2000 corridas, 2 rechazadas por no imprimibles, ninguna falla
  (194 s);
- encontrado por el equipo de fichas (2317 impresoras del paquete libre): las impresoras de pellets fallaban porque
  `layers.height` no pasaba de 2 mm. Arreglado según `pellets.md`: capas hasta 10 mm y cordones hasta 40 mm, con los
  topes de la boquilla de siempre; prueba nueva con boquilla de 8 mm y capas de 4 mm;
- la CI de Linux corre más de 30 minutos con ASan (las dos pruebas del relleno por esfuerzos tardan 18 min cada una
  con ASan): el aviso de tiempo cortó una corrida; esas dos se corrieron aparte y pasaron.

**Sigue:** el resto de `assistant.md` (panel de ayuda con los datos del paquete), `settings_levels.md` y
`settings_panel.md`, y el tiempo de impresión (`print_time.md`).

## 2026-10-26 — prueba de caja negra del equipo de fichas: QA1 a QA7

El equipo de fichas corrió el motor como caja negra contra las fichas (`docs/spec/qa/2026-10-26.md`, llegó con el PR
#23). Arreglado:
- **QA1, QA2 (falda):** `adhesion.skirt_distance` 6 mm por defecto (antes 3), y la pieza se agranda con uniones
  redondeadas (`InflatePaths` con `JoinType::Round`). S7: 118,5 mm; S8: 287,0 mm por capa.
- **QA3 (borde):** las vueltas van a la separación de los cordones, el ancho menos (1 − π/4) por la altura de la
  primera capa (0,357 mm con 0,4 × 0,2), y son tantas como entran en el ancho pedido. S9: 14 vueltas, la de afuera
  con el eje a 4,82 mm, 1340,4 mm.
- **QA4 (soporte en el límite):** lo que sale justo medio cordón (o justo el ángulo pedido) ya no es voladizo: la
  ficha dice «más de». S1: la cuña de 45° queda sin soporte; las de 20°, 30° y 40° lo llevan.
- **QA5 (ventilador):** la regla 3 de `cooling.md`. Ajustes nuevos `cooling.fan_below_time` (60 s),
  `cooling.fan_always` y `cooling.fan_bridges` (100 %); `cooling.fan_off_layers` pasa a 3. El escritor deja marcas en
  cada capa (su comienzo y el comienzo y el fin de cada puente) y, cuando sabe el tiempo de la capa, escribe ahí los
  comandos: el de la capa queda justo antes de su `;LAYER_CHANGE` (regla 4). Las capas que frenan llevan
  `fan_max`, también entre las primeras (decisión del dueño del 24-10, que la ficha no contradice de forma explícita).
  Los porcentajes se pasan a S con 255/100 (50 % es S128; con 2,55 daba S127).
- **QA6:** `speed.top_solid_infill` 15 mm/s (antes 50). Queda reemplazado por la decisión del dueño de esta tarde:
  ninguna velocidad tiene valor por defecto (ver la entrada siguiente).
- **QA7 (C4):** con la primera capa de 0,3 y las demás de 0,2, un cubo de 10 mm tiene 50 capas hasta 10,1 mm (antes
  49 hasta 9,9): una capa se agrega mientras su plano de corte no pase el techo de la pieza.

**Medido** (cubo de 20 × 20 × 10 mm, como K1, con los ajustes del motor): capas 1 a 3 sin ventilador; capa 4 (8,1 s)
S246 (96 %); capas 5 a 47 (7,7 s) S247; capas 48 a 50 (56 s) S100 (39 %). La fórmula da lo mismo que K1 (98, 93, 86,
50 % para 6,6, 10,3, 16,8 y 46,7 s; prueba nueva en `tests/test_cooling.cpp`); los tiempos de capa son otros porque
las velocidades y las capas de arriba son otras (ver abajo). Un puente sobre dos pilares lleva `fan_bridges` solo
mientras se tiende, y vuelve el de la capa (prueba nueva en `tests/test_gcode.cpp`).

**Encontrado al medir:** el motor marca como «Top solid infill» las tres capas sólidas de arriba; la ficha
`surfaces_infill.md` (regla 2 e I1) marca así solo la de arriba de todo, y la primera sólida sobre el relleno es un
puente (regla 6, la capa 48 de K1). Quedan en `PLAN.md`.

**Falta:** QA8 (revisión de estabilidad) y QA9 (paredes finas con `walls.min_line_width`), y `cooling.enabled`.

**Probado:** `tools/ci_linux.sh` (gcc, clang y ASan+UBSan).

## 2026-10-26 — las velocidades son de la impresora (decisión del dueño)

**Pedido del dueño:** «velocidades de todas las variables deben ser exclusivamente por los perfiles de impresora, no
debe darse por defecto ninguna, ya que dependen exclusivamente de la impresora que se haya elegido». Y después: solo en
el caso extremo de una impresora creada de cero o sin ningún valor, y solo para lo que falte, un valor por defecto que
sea el de una Ender 3, lo más estándar y seguro para cualquier máquina.

**Hecho:**
- `Settings` ya no tiene velocidades: todas valen 0 («sin valor») y en el JSON se escriben `null`. Son las de
  `speed`, `retraction.speed`, `cooling.min_speed` y `machine.corner_speed`, que el G-code necesita, y los topes
  `speed.max` y `machine.max_feed_*`, que sin valor no topan.
- `write_gcode` se niega sin ellas (`require_speeds`, nombra las que faltan). `fill_missing_speeds` pone las de
  respaldo solo en las que faltan, y dice cuáles.
- Respaldo «Ender-3 de fábrica», de las guías públicas de Creality: paredes 40, externa 30, relleno y sólido 60, arriba
  40, soporte 40, viajes 180, primera capa 25, retracción 25 mm/s; `cooling.min_speed` y `machine.corner_speed` 10,
  propios. Tabla y enlaces en `docs/CONFIG.md`, «Velocidades de respaldo».
- Perfiles (versión 2): `speed` y `cooling.min_speed` pasan al perfil de impresora. Un perfil de impresión de la
  versión 1 se abre sin sus velocidades.
- Quién usa el respaldo: la impresora genérica de la interfaz (creada de cero); las del catálogo, que no saben más
  que su velocidad máxima (ninguna velocidad de respaldo la pasa; la retracción de 35 o 40 mm/s que inventaba el
  catálogo ya no está); un perfil de impresora guardado sin velocidades; `slice` con un archivo sin ellas (aviso
  `speed_fallback`). La interfaz lo dice al elegir la impresora, y las velocidades están ahora en **Impresora ›
  Velocidades**.
- Pruebas: la impresora de las pruebas (`tests/test_printer.hpp`) tiene las velocidades con que se midieron; prueba
  nueva de las reglas (sin valor, `null`, el respaldo solo en lo que falta, sus números); perfiles de versión 1 y 2;
  `slice` sin velocidades avisa y usa las de respaldo; `tests/data/printer_settings.json` para la prueba sin avisos.

**Cambia para quien usa el motor:** un archivo de ajustes sin velocidades ya no lamina con las de banco (60, 80,
130 mm/s…) sino con las de respaldo, más lentas: el cubo de 10 mm de `tests/data` con `{}` lleva paredes a 40 y viajes
a 180 mm/s.

**Para el equipo de fichas:** la ficha `gcode_motion.md` y otras dan velocidades por defecto medidas en Tisma Slicer;
con esta decisión el motor no tiene velocidades por defecto. QA6 (`speed.top_solid_infill` 15 mm/s) queda sin efecto:
esa velocidad es la que dé el perfil de la impresora.

## 2026-10-26 — vista previa de modelos grandes, primer paso

**Pedido del dueño:** técnicas como LOD y otras formas de dibujar para más FPS con modelos grandes, «tipo raycaster»;
se traba según cuántas capas se muestran; llegar por lo menos a 60 FPS; y «la idea es que el motor funcione en
Vulkan».

**Hecho** (`docs/LARGE_MODELS.md`):
- Estudio de técnicas con sus fuentes (recorte, LOD, simplificación por cuádricas, rayos primarios contra el BVH,
  oclusión jerárquica) y un plan; el paso a Vulkan queda en `PLAN.md`, «Vista con Vulkan».
- Medir: `--bench-frames N` para las capturas, y el tiempo de la vista en la barra de estado («Vista 8,3 ms · 120
  FPS», consulta de tiempo de la GPU) para muestrear en cada máquina.
- No dibujar lo que la pieza tapa (`hidden_inside`): paredes internas y relleno cubiertos por la capa de abajo, la de
  arriba y la pared externa, lejos de los escalones. La primera versión, sin lo de los escalones, dejaba puntitos en
  la cúpula de una esfera (vistos al comparar las imágenes); ahora la imagen es la misma píxel por píxel.
- Regla 2 de `surfaces_infill.md` (de la prueba de caja negra): solo la capa de arriba de todo es la superficie de
  arriba; las otras capas sólidas de arriba van como relleno sólido.

**Medido** (GPU por software del contenedor, 1200 × 800, pieza entera; sirve para comparar, no son FPS reales):
cilindro de 60 × 80 mm, 181 384 cordones → 52 499 dibujados, 2 330 → 633 ms por cuadro con oclusión ambiental;
esfera de 80 mm (peor caso), 188 664 → 157 196, 3 400 → 2 231 ms. Armar los datos: 121 y 181 ms.

**No probado:** FPS en una placa de video real (no hay en el contenedor): para eso está el número de la barra de
estado.

**Sigue:** V1 de «Vista con Vulkan».

## 2026-10-26 — vista con Vulkan, paso V1

**Pedido del dueño:** «la idea es que el motor funcione en Vulkan», y «que detecte automáticamente si soporta Vulkan;
si no lo hace, que vaya a OpenGL».

**Hecho** (`PLAN.md`, «Vista con Vulkan»):
- `apps/tisma/vk_window.*`: la ventana con Vulkan, desde la especificación (instancia, dispositivo, swapchain, un render
  pass que limpia y dibuja Dear ImGui, dos cuadros en vuelo, texturas para los íconos y el logo, y la imagen copiada
  a memoria para las capturas). Vulkan con volk y la superficie con GLFW, como el cómputo del motor; los paneles con
  el backend de Vulkan de Dear ImGui (MIT, anotado en `THIRD_PARTY.md`).
- `--graphics auto|vulkan|opengl`. La detección es arrancar Vulkan: sin cargador, sin dispositivo 1.1 o sin superficie
  para la ventana, avisa y sigue con OpenGL. Probado con lavapipe (Vulkan) y sin controlador (vuelve a OpenGL).
- El renderizador de OpenGL no hace nada si no se inició (ventana de Vulkan).
- Prueba `ui_vulkan_window`: la ventana con Vulkan y su captura; en la CI (`TISMA_REQUIRE_GPU=1`), volver a OpenGL
  es una falla.

**Falta:** la vista 3D en Vulkan (V2). Hasta entonces el modo automático usa OpenGL, para no perder la vista 3D.

## 2026-10-26 — vista con Vulkan, paso V2

**Pedido del dueño:** «la idea es que el motor funcione en Vulkan». En el camino pidió también greedy meshing para los
cordones y mejorar la oclusión ambiental y el color «como lo hace Prusa 3.0», con los videos del lanzamiento de
referencia (quedan en `PLAN.md`).

**Hecho** (`PLAN.md`, «Vista con Vulkan»):
- `apps/tisma/vk_view.*`: la vista 3D con Vulkan. Dibuja la cama, la grilla, el modelo con el elegido y su
  desplazamiento, la pintura y la vista previa instanciada (con lo tapado afuera, como en OpenGL), con MSAA y luego
  FXAA en una textura que ImGui muestra bajo los paneles. Los shaders son los de OpenGL en GLSL 450
  (`apps/tisma/shaders/`, compilados a SPIR-V por `tools/compile_shaders.py`).
- El orden de dibujo de la vista previa (lo que se ve y lo tapado, por capa) pasó a `src/ui/scene.*`, compartido por
  las dos vistas.
- El tiempo de la GPU con marcas de tiempo de Vulkan (la barra de estado) y `--bench-frames` con Vulkan.
- `--compare-view otra.png` y las pruebas `ui_view_prepare_*` y `ui_view_preview_*`: la misma escena con OpenGL y con
  Vulkan, imagen contra imagen; pasa si menos del 0,5 % de los píxeles de la vista difiere en más de 24 niveles.
- `src/ui/png.*` lee las capturas que escribe y compara dos imágenes (con su prueba).

**Errores encontrados y corregidos:**
- El cómputo del motor cargaba sus funciones en los punteros globales de volk, los mismos de la ventana: después de
  laminar, la ventana Vulkan llamaba funciones del otro dispositivo y se caía al cerrar (`vkDestroySurfaceKHR` nulo).
  Ahora el cómputo tiene su propia tabla de funciones y el cargador se arranca una sola vez (`load_vulkan`).
- La validación de sincronización de Khronos encontró dos carreras, ya corregidas:
  - el cuadro siguiente reescribía la profundidad y el color sin esperar al anterior;
  - la copia de la captura escribía el mismo buffer en dos cuadros en vuelo.
  Con la capa de validación, ahora no hay errores.
- lavapipe dibuja cortadas y gruesas las líneas que salen muy lejos de la pantalla o pasan detrás del ojo: la grilla
  de Vulkan va en tramos de 5 mm.

**Medido** (CPU del contenedor: llvmpipe para OpenGL, lavapipe para Vulkan; 1200 × 800, luz rápida; sirve para
comparar, no son FPS reales):

| Pieza | Cordones dibujados | OpenGL | Vulkan |
|---|---|---|---|
| Cilindro de 60 × 80 mm | 52 499 | 657 ms | 672 ms |
| Esfera de 80 mm | 241 612 | 3 838 ms | 3 787 ms |

Iguales, porque las dos usan el mismo rasterizador de Mesa por CPU. Lo que gana Vulkan viene con V4 (el recorte en la
GPU y los dibujos indirectos) y en una placa de video real.

**No probado:** una placa de video real y Windows con Vulkan.

**Sigue:**
- mejorar la oclusión y el color, con las imágenes del lanzamiento de PrusaSlicer 3.0 como referencia de caja negra;
- V3 (la oclusión y los rayos en Vulkan), y entonces el modo automático pasa a Vulkan;
- greedy meshing en V4.

## 2026-10-26 — oclusión y color de la vista previa; velocidad de las capas de abajo

**Pedidos:**
- **Del dueño:** mejorar la oclusión ambiental y el color «como lo hace Prusa 3.0», con los videos y las imágenes
  del lanzamiento como referencia. Los miré como caja negra: miniaturas de los videos e imágenes del blog, nunca
  código.
- **Del equipo de specs, medición de caja negra** (`docs/spec/qa/2026-10-26-velocidad.md`, metas RV1 a RV6 en
  `docs/spec/BENCHMARKS.md`, aprobadas por el dueño): las capas de abajo se llevaban unos 5 de los 6 s del Benchy.

**Lo que muestra la referencia (caja negra):**
- cada capa de una pared se ve como un tubo, con una ranura oscura y fina contra la de arriba;
- colores saturados;
- la sombra suave de la pieza sobre la cama;
- oscuro donde la pared toca la cama.

**Hecho, la vista** (OpenGL y Vulkan, siguen iguales imagen contra imagen):
- **Ranuras entre capas.** Cada cordón calcula, en su sección, cuánto cielo y cuánta luz principal le dejan el
  cordón de arriba y el de abajo: la oclusión por horizontes (Bavoil, Sainz y Dimitrov 2008) resuelta exacta para dos
  cordones que se tocan, con la luz principal de unos 35° de ancho, para que entre con penumbra. Solo entre los tipos
  que se apilan y dentro de las capas mostradas; de lejos se promedia como las normales del filtro de moiré. Con ray
  tracing no se aplica.
  - La primera versión ponía la cara de arriba del rombo como «techo plano», tapado por el cordón de arriba, y las
    paredes salían casi negras. Ahora la ranura sube por el costado según la altura en la sección: lo que se ve de
    costado.
- **Curva de tono sobre el canal más brillante:** los colores saturados no se lavan; los grises, igual que antes.
- **Falta, el paso siguiente:** la sombra de la pieza sobre la cama (un mapa de sombras desde la luz principal, en
  la pasada de oclusión).

**Hecho, la velocidad** (`PLAN.md`, «Velocidad del laminado»):
- Era la detección de puentes: el área de los cordones de la capa de abajo unía un disco de 64 lados por punto. Las
  paredes del Benchy cambian de ancho casi en todos lados, así que la primera mejora (un offset por tramo de un mismo
  ancho) no alcanzaba. Ahora:
  - cada recorrido es un offset variable de Clipper2 (el medio ancho de cada punto);
  - lo impreso abajo se calcula solo en una ventana tres anchos más grande que la superficie inferior, simplificado
    a 1 µm;
  - el apoyo de los anclajes se calcula solo alrededor del puente.

**Medido** (contenedor de 4 núcleos, mediana de 5 corridas, Benchy, antes 1ffbdeb y después):

| Meta | Caso | Antes | Ahora | Meta |
|---|---|---|---|---|
| RV1 | Benchy con los ajustes por defecto | 6,3 s | 1,39 s | 1,5 s |
| RV2 | Lo que suman las capas de abajo, tolerancia 0 | 7,5 → 200 s | 8,2 → 8,9 s (8 %) | 25 % |
| RV2 | Ídem, tolerancia 0,005 | 8,4 s más | 1,34 → 1,65 s (22 %) | 25 % |
| RV3 | Ídem, de 1 a 6 paredes | 1,7 a 6,7 s | 0,17 a 0,25 s | 0,3 s |

La parte de RV2 con tolerancia 0 es una sola corrida.

**RV6, el G-code:**
- Sin capas de abajo, el Benchy da el mismo G-code byte a byte.
- Con capas de abajo cambia una sola capa (Z 0,6, la del puente del casco sobre la primera): 767 líneas de 336 538.
  Hay la misma cantidad de puentes, de zonas sólidas y de relleno, 4 movimientos menos y el filamento casi igual.
  El borde de lo impreso abajo se movió a lo sumo 1 µm (la simplificación y los arcos del offset, que ahora empiezan
  en la normal de cada unión). Así cambian unas micras los bordes de esa zona sólida y del puente, y con eso el
  orden de sus líneas.

**No probado:** RV4 (V10, reusar capas iguales) y RV5 (V11, relaminar por etapas) siguen pendientes.

**Sigue:**
- la sombra sobre la cama;
- greedy meshing;
- V3/V4 del informe (`;WIDTH` solo cuando cambia y menos movimientos).

**Agregado después, mismo día:** la sombra de la pieza sobre la cama (mapa de sombras desde la luz principal, en la
pasada de oclusión; `docs/UI.md`). Probé que la pieza también la recibiera, pero los escalones entre capas, del
tamaño de un texel del mapa, se sombreaban entre sí y dibujaban anillos en una esfera. Por ahora solo la recibe la
cama. El dueño pidió dejar la vista para después y priorizar V7 a V11 de velocidad.

## 2026-10-26 — velocidad: V11, relaminar por etapas

**Pedido del dueño:** dejar la vista para después y priorizar las optimizaciones V7 a V11 del informe de velocidad
(`docs/spec/qa/2026-10-26-velocidad.md`, metas en `docs/spec/BENCHMARKS.md`).

**Estado de V7 a V11:**
- **V7:** hecho en el bloque anterior; RV1 a RV3 se cumplen.
- **V8:** no hace falta; las fichas piden el resultado y ya se cumple.
- **V11:** hecho ahora.
- **V10 y V9:** siguen.

**Hecho:**
- **El `Slicer` de la interfaz guarda el último corte** (`src/ui/slicing.*`). Si la malla, la pintura y los ajustes
  del corte son los mismos, rehace solo el G-code, su verificación y la vista previa. Los ajustes del corte son todos
  menos los que solo lee el G-code (`without_gcode_settings`):
  - velocidades, límites de movimiento y aceleraciones;
  - enfriamiento y ventilador, retracción;
  - temperaturas y las variables de la impresora, flujo y avance de presión;
  - los topes de flujo y de temperatura, la resolución del G-code, el sabor y la extrusión relativa;
  - el G-code de inicio y de fin.
- **La lista se armó buscando qué ajustes lee el laminado,** y una prueba la confirma de punta a punta. Una pieza con
  un puente: para cada ajuste de la lista, el G-code por etapas es igual byte a byte al de un laminado entero. Un
  ajuste del corte, otra malla u otra pintura cortan de nuevo.
- **Un corte con el caso de carga del relleno guiado por esfuerzos no sirve a otra corrida.** Lo encontró una prueba
  vieja.
- **Los planificadores de viajes de cada capa se arman antes, en paralelo,** y la verificación y la vista previa a la
  vez. El G-code no cambia.
- **La barra de estado dice «G-code rehecho en X s»** cuando fue por etapas.

**Medido** (Benchy, 4 núcleos):
- **RV5:** relaminar después de cambiar `speed.infill` tarda de 0,41 a 0,46 s, con mediana 0,41 s (meta 0,5 s).
  Un laminado entero tarda 1,35 s; el G-code es el mismo byte a byte.
- **Con los planificadores en paralelo:** el Benchy con capas de abajo tarda 1,34 s (antes 1,39 s) y sin ellas
  1,05 s, con el mismo G-code.

**Sigue:** V10 (reusar capas iguales, RV4) y medir V9.

## 2026-10-26 — velocidad: V10, capas repetidas

**Pedido del dueño:** priorizar V7 a V11 del informe de velocidad. Queda V10 (RV4: el cilindro de 80 mm debe tardar
como mucho un 25 % más que el de 40 mm) y medir V9.

**Hecho:**
- **El corte por tandas** (`src/slice/layers.*`). Un nivel sin ningún vértice entre él y el de abajo cruza las mismas
  aristas, unidas en el mismo orden (`same_crossings`). Solo se corta el primero de cada tanda; los demás calculan sus
  puntos sobre las aristas de ese, iguales bit a bit a un corte nuevo (lo prueba un test en una pirámide y un prisma).
  Sirve también para conos, pirámides y campos curvos.
- **La misma sección.** Si además toda arista cruzada es vertical, o está solo en triángulos verticales (los lados de
  un prisma partidos por diagonales), la sección en x e y es la misma (`same_sections`). El corte deja esos niveles
  vacíos y el laminado copia la región y las paredes de la capa de abajo; no los de la primera capa con su achique,
  ni con piel difusa (cada capa la dibuja de nuevo). Con un borde abierto en la malla corta todo, porque el extremo de
  un contorno abierto se movería.
- **Copias que dan el mismo resultado byte a byte:**
  - las superficies de una capa, si su región de relleno y las regiones que miran son las de la capa de abajo;
  - el relleno, si las superficies son las de la capa a un período del patrón (`fill_period`: 2, o 6 en panal; nada
    en giroide, cúbico y adaptativos, que se mueven con la altura); una prueba lo confirma para cada patrón;
  - el planificador de viajes y las islas del G-code, si la región es la de la capa de abajo.
- **El G-code en serie, más liviano:** la costura alineada y la simplificación de los recorridos comparan distancias
  al cuadrado y solo sacan la raíz cuando quedan a un pelo del límite (`hypot` era 9 millones de llamadas en el
  cilindro de 80 mm). La respuesta es la misma.

**Medido** (4 núcleos, mediana de 3, ajustes por defecto):

| Pieza | Antes | Ahora |
|---|---|---|
| Cilindro de 32 768 lados, 40 mm | 2,69 s | 0,36 s |
| Cilindro de 32 768 lados, 80 mm | 5,33 s | 0,63 s |
| Relación 80/40 (RV4, meta 1,25) | 1,98 | 1,75 |
| Benchy | 1,35 s | 1,35 s |

- **RV4 no se cumple todavía.** Lo que crece con las capas es la parte en serie del G-code. Por cada 200 capas del
  cilindro: el orden y las costuras unos 50 ms, la simplificación 22 ms, el texto unos 35 ms y su verificación 35 ms.
  El corte y el laminado ya casi no crecen (+10 ms). Hace falta V5: armar el texto de cada capa en paralelo y
  verificar en paralelo.
- **RV6:** el G-code del Benchy (con y sin giroide), la esfera, la esfera con soportes, el tubo, las islas, las cajas,
  el cubo con piel difusa y la pieza torcida sale igual byte a byte. En los cilindros (128 y 32 768 lados) cambia:
  antes cada capa simplificaba otros puntos de las diagonales de los lados, y ahora todas repiten la sección de la
  primera. Las paredes nuevas quedan a menos de 0,018 mm de las de antes (dos simplificaciones del mismo círculo,
  cada una dentro de `slicing.simplify_tolerance` y `gcode.resolution`).
- **Pruebas:** 395 de 395 pasan en Release. Nuevas: los niveles con las mismas aristas, la misma sección del prisma
  (con un lado abierto y con una pirámide como contraejemplo), un prisma laminado que repite región, paredes y
  relleno, el escalón de superficies y el período de cada patrón.

**Sigue:** V5 (el G-code en paralelo) para llegar a RV4; después medir V9.

## 2026-10-26 — velocidad: V5 y V9; RV4 se cumple

**Pedido del dueño:** V7 a V11 primero. V10 había quedado sin llegar a RV4 por la parte en serie del G-code, y V9
estaba sin medir.

**Hecho:**
- **V5, el G-code en un pipeline** (`src/gcode/writer.*`). Cada capa se planea (`plan_layer`: el orden de las islas
  y los recorridos y las costuras, que siguen de donde terminó la capa de abajo) y se escribe (`write_layer`) por
  separado. Un pipeline de oneTBB planea en orden, simplifica los recorridos en paralelo y escribe en orden, así que
  mientras se escribe una capa se planean las siguientes. Una capa que se frena por el tiempo mínimo se vuelve a
  escribir desde el mismo plan, sin planearla de nuevo.
- **La verificación por bloques** (`src/gcode/check.*`): lee bloques de unos 256 KiB de líneas enteras en paralelo
  y sigue el estado en orden. Una prueba nueva pone un error a cada lado del corte entre bloques y espera su número de
  línea exacto.
- **Menos trabajo en serie, con el mismo texto:**
  - la costura alineada calcula la distancia solo para los candidatos a un pelo del cuadrado más chico;
  - sin módulos en los recorridos de la costura;
  - el emisor formatea X, Y y el ancho una sola vez;
  - la exportación relee el archivo de una vez (carácter por carácter era un décimo de la exportación).
- **V9:** lo que dejan las paredes se simplifica con `slicing.simplify_tolerance`, como los contornos.

**Medido** (4 núcleos, sin otra carga, mediana de 9):

| Pieza | V10 | V5 | V5 y V9 |
|---|---|---|---|
| Cilindro de 32 768 lados, 40 mm | 0,365 s | 0,282 s | 0,291 s |
| Cilindro de 32 768 lados, 80 mm | 0,561 s | 0,343 s | 0,360 s |
| Relación 80/40 (RV4, meta 1,25) | 1,54 | 1,22 | 1,24 |
| Benchy (RV1, meta 1,5 s) | 1,133 s | 0,997 s | 0,975 s |
| Benchy sin capas de abajo | — | 0,758 s | 0,748 s |

- **RV4 se cumple, pero justo:** el ruido de la máquina mueve unos 0,02 s. Las corridas sueltas del cilindro van de
  0,26 a 0,40 s. Lo que crece con las capas es el texto del G-code, que va en serie (unos 55 ms por cada 200 capas),
  y la verificación (unos 10 ms).
- **RV6, V5:** el mismo G-code byte a byte en el Benchy (con y sin giroide), la esfera (con soportes, con la tabla de
  ventilador, con relleno ondulado y con ZAA), los cilindros, el tubo, las islas, las cajas, el cubo con piel difusa,
  la pieza torcida, las capas por zona y una torre de cambios por altura (temperatura, ventilador, velocidad,
  retracción, G-code propio y flujo forzado).
- **RV6, V9:** cambia el relleno del Benchy (29 234 de 336 533 líneas). Por tipo, el largo total cambia: relleno
  interno −0,2 %, sólido +0,1 %, superior −0,05 %, puentes +0,8 % (7,6 mm). Las paredes y el contorno quedan iguales.
  La esfera, el cilindro de 128 lados, el tubo y las islas no cambian.
- **Pruebas:** 396 de 396 en Release; el fuzzer de G-code, 150 casos sin fallas.

**No medido:** RV2 y RV3 con V9 (V9 solo saca vértices; ya se cumplían). RV5 no cambia: el relaminado por etapas
usa el mismo escritor, ahora más rápido.

**Sigue:** V3 (`;WIDTH` solo cuando cambia, hoy 21 % del archivo) y V4 (juntar tramos cortos). Ninguna de las dos
cambia el tiempo de corte, pero sí el tamaño del G-code y el tiempo de escribirlo. Después, la vista (pedido del
dueño: oclusión y color como Prusa 3.0, mallado codicioso).

## 2026-10-26 — mantenimiento (18:30)

El bloque de mantenimiento de las 16:40 se disparó mientras se terminaba V10; se hizo al terminar V5 y V9. Ninguna
otra sesión del motor estaba trabajando.

**Revisado:**
- `origin/main` no trae nada nuevo. Las fichas nuevas están en `spec/next` (PR #27, sin unir): RV7 a RV10 en
  `BENCHMARKS.md` y la regla 8 de `perimeters.md` (la costura alineada en la GPU). `check_spec` pasa en las tres.
- **Fuzzer de G-code:** 2000 casos con la semilla 20261026, 0 fallas (2 rechazados como no imprimibles, como
  corresponde), en 86 s.
- **CI de Linux** (gcc, clang, ASan) en verde sobre `2e4ff17`; Windows en verde sobre `1b2e782`.

**Encontrado y arreglado:**
- **Piezas huecas** (`infill.density` 0, la meta RV7). Con relleno impreso, una superficie inferior que queda toda
  sobre la región de abajo se descarta sin calcular cordones; sin relleno, ese atajo no corría. Las capas sólidas de
  abajo calculaban los cordones de toda la capa de abajo para descubrir que apoyan sobre la primera, que es sólida.
  Ahora el mismo atajo mira la región de abajo menos su relleno disperso, que no se imprime.
  - Estrella hueca de 160 mm (800 capas): corte de 0,90 a 0,62 s.
  - Benchy sin relleno: de 1,30 a 1,04 s; sin relleno y con 1 pared, de 1,41 a 1,13 s.
  - G-code igual byte a byte en las 9 piezas probadas (estrella y elipsoide huecos, esfera, Benchy con 1 pared, islas,
    esfera grande).
  - Prueba nueva: con relleno 0, el puente de la placa sobre dos pilares sigue igual y la estrella hueca no tiene
    puentes.

**Medido:**
- **RV3** (Benchy, lo que suman las capas de abajo, mediana de 5): 1 pared 0,21 s, 3 paredes 0,33 s, 6 paredes
  0,29 s. La meta es 0,3 s: queda en el límite con 3 paredes.
- **RV2 con `simplify_tolerance` 0** (mediana de 3): 5,12 s contra 3,74 s sin capas de abajo, un 37 % más (meta
  25 %). Sigue sin cumplirse: con tolerancia 0, el resto del laminado también es más lento.
- **La costura alineada (RV10) no se reproduce sin el conejo**, que sale de los recursos de Tisma Slicer y no se
  puede abrir. En piezas propias, `aligned` y `nearest` tardan casi lo mismo después de V5: Benchy 143 contra
  141 ms de `write_ms`; esfera 66 contra 74; una estrella de 7 puntas 308 contra 377; un elipsoide de pocas caras 74
  contra 57. Se pide al equipo de fichas que mida el conejo en `dev`.
- **Un modelo del dueño** (cráneo de triceratops, 3MF de 546 684 triángulos, ajustes de una MK3S con soportes): 13,2 s
  de laminado, 8 s de ellos en los soportes. Los soportes van en serie de arriba abajo, y lo caro es cerrar la
  columna con dos offsets en cada capa con voladizo arriba. Calcular antes, en paralelo, el offset de la pieza por
  `support.xy_gap` no se nota (12,1 contra 11,9 s), y se descartó. El cierre depende de la capa de arriba: va con
  RV9.

**Sigue:** RV9 (los soportes y lo demás que va en serie), RV2 con tolerancia 0, las mediciones del conejo (RV7, RV8 y
RV10) y la decisión del dueño sobre el ancho en V3 y V4.

## 2026-10-26 — soportes más rápidos, en la CPU y en la GPU

**Pedido del dueño:** mejorar la velocidad de laminado de los soportes, y buscar acelerarlos también con la GPU. Salió
de su cráneo de triceratops (3MF de una MK3S, 546 684 triángulos): 13,2 s de laminado, 8 s en los soportes.

**Por qué era lento:** la columna del soporte baja capa por capa y se cierra por un cordón (agrandar y achicar) en cada
capa con un voladizo arriba. En una pieza escaneada, cada capa trae de 26 a 51 voladizos diminutos (ruido). La columna
junta unos 250 anillos, y el offset de Clipper2 calcula todas sus intersecciones: 5,6 s en 307 cierres, 33 G
instrucciones. Calcular antes, en paralelo, el offset por `support.xy_gap` no cambió nada.

**Hecho:**
- **Una grilla de bits** (`src/geom/bitgrid.*`):
  - relleno por línea de barrido en los centros de los píxeles;
  - dilatación y erosión con un disco, con desplazamientos de palabras de 64 bits;
  - contornos por los bordes de los píxeles.
  - Pruebas propias: relleno, disco, cierre de huecos, borde de la grilla, contornos con agujeros y diagonales, y
    volver a rellenar el contorno da los mismos bits.
- **La columna del soporte en la grilla** (`support_areas`). Píxeles de un octavo de cordón (0,05 mm con cordón de
  0,4), y el cierre es un disco de un cordón. Los voladizos se agrandan tres cuartos de píxel antes de rasterizar: un
  anillo más fino que un píxel (una pendiente apenas pasada del límite) no se pierde; sin eso, el cráneo perdía un
  19 % de soporte. La grilla cubre solo los voladizos, y las capas de arriba del último no la usan. El contorno se
  simplifica a medio cordón (con un píxel, las uniones en zigzag sumaban un 40 % de movimientos) y la separación XY se
  resta exacta. Las grillas de cada tanda de 32 capas se llenan en paralelo y los contornos se sacan en paralelo; la
  columna va en orden.
- **La GPU:** `support_column.comp` hace los mismos pasos, una invocación por palabra de 32 bits, una tanda de capas en
  un solo envío (`run_steps`, con barreras entre pasos). `SupportParams::gpu` lo elige cuando hay contexto. Una prueba
  compara los bits con la CPU con radios 1, 8 y 31 y una grilla que no es múltiplo de la palabra; otra, los soportes
  enteros de una T con tapa en pendiente.

**Medido** (4 núcleos, mediana de 3, `slice_ms`):

| Pieza | Antes | Ahora |
|---|---|---|
| Cráneo, ajustes del 3MF (MK3S, soportes a 35°) | 12,9 s | 5,8 s |
| Benchy con soportes | 1,38 s | 1,18 s |
| Esfera grande con soportes | 2,10 s | 2,08 s |
| Esfera de 50 mm con soportes | 0,58 s | 0,58 s |

- **El paso de los soportes del cráneo, solo:** de 7,6 s a 1,4 s (rasterizar 81 ms, columna 529 ms, contornos y
  separación 773 ms).
- **La GPU acá es lavapipe**, que emula en la CPU: el cráneo tarda 8,95 s con ella contra 6,13 s sin ella, con el
  mismo G-code byte a byte. Falta medir en una placa real; por eso la línea de comandos sigue sin usar lavapipe.
- **RV6:** el soporte cambia. Área: cráneo +9 %, Benchy +5 %, esferas +1 % (el borde de afuera queda a menos de
  0,05 mm, del lado seguro). Movimientos de soporte: Benchy 8475 → 7925, esfera grande 40 432 → 40 510. Tiempo de
  impresión: cráneo 4 h 58 min → 4 h 50 min; Benchy 1 h 26 min 33 s → 1 h 27 min 28 s.
- **Pruebas:** 402 de 402 en Release, 600 casos del fuzzer sin fallas. La prueba del soporte de la T ya no pide el
  área exacta: la pide dentro de un píxel y medio por lado, y la separación de 0,6 mm del tallo, exacta.

**Sigue:** medir la GPU en una placa real; si rinde, pasar también el relleno de las grillas a la GPU. Los contornos
y la resta de la separación (0,77 s en el cráneo) son lo que más queda en la CPU.


## 2026-10-26 — costura alineada tan rápida como `nearest` (RV10)

**Por qué:** la sexta medición del equipo de fichas (`qa/2026-10-26-velocidad.md`, en `dev` `5b0bdbd`) dejó RV10 sin
cumplir: el conejo con costura `aligned` tardaba 1,61 veces lo de `nearest` (meta 1,2). Sin el conejo (no se puede
usar), se midió con una gota con bultos de 172 080 triángulos y 450 capas, generada por un script propio.

**Lo que costaba:** un perfil (callgrind) en la gota mostró que no eran los viajes sino la búsqueda de esquinas:
- las paredes de esta pieza tienen hasta 11 000 puntos por lazo, amontonados en las esquinas redondeadas del offset;
- para medir el giro de cada punto se caminaba, punto por punto, hasta el primero a 0,1 mm de cada lado: decenas de
  pasos por punto, 76 millones en 60 capas.

**Hecho** (`src/gcode/order.*`, `src/gcode/writer.cpp`):
- **Esquinas en tiempo lineal:** una cuerda nunca es más larga que el camino por el lazo, así que los puntos a menos
  de 0,1 mm por el lazo (con un margen mucho más grande que el redondeo de las sumas) no pueden estar a 0,1 mm en línea
  recta. Un puntero sobre el largo acumulado, que solo avanza, los saltea; los demás se miden como antes. Las esquinas
  son las mismas.
- **Candidatos aparte, elección en orden** (`perimeters.md`, regla 8, el camino sin GPU): `seam_candidates` depende solo
  del lazo, así que una etapa nueva del pipeline los busca en paralelo para todas las paredes de cada capa, antes del
  plan. El plan, que sigue a la capa de abajo, solo elige entre ellos.
- **Viajes en paralelo:** el viaje hasta cada recorrido se planea en la etapa paralela, desde donde termina el
  anterior (simplificar conserva las puntas). El escritor lo usa solo si la boquilla está exactamente ahí; si no, lo
  planea él, como antes.
- **Pruebas:** las esquinas contra la caminata punto por punto en 300 lazos al azar (estrellas con puntas redondeadas
  de hasta 40 puntos amontonados, puntos repetidos, lazos más chicos que 0,1 mm, en los dos sentidos); y los candidatos
  dados de antemano eligen las mismas costuras que sin ellos.

**Medido** (4 núcleos, mediana de 5, tiempo total del comando):

| Pieza | Antes | Ahora |
|---|---|---|
| Gota, costura `aligned` | 3,81 s (`write_ms` 1524) | 2,68 s (`write_ms` 463) |
| Gota, costura `nearest` | 2,69 s (`write_ms` 478) | 2,79 s (`write_ms` 466) |
| Alineada contra `nearest` (RV10, meta 1,2) | 1,41 veces | 0,96 veces |
| Cráneo de triceratops sin soportes | 4,93 s (`write_ms` 861) | 4,72 s (`write_ms` 566) |
| Benchy | 0,96 s | 0,97 s |

- Los viajes en paralelo solos (prueba A/B): cráneo `write_ms` 883 → 568, Benchy 158 → 130; en la gota, nada.
- **RV6:** el G-code es el mismo byte a byte en 21 casos: Benchy (por defecto, gyroid, alineada, una pared), cajas,
  cubo con piel difusa, cilindros, esferas (soportes, ZAA, torre, no planar), islas, placa, tubo, torcido, estrella
  hueca, gota y cráneo con dos juegos de ajustes.
- **Pruebas:** los 376 casos de la suite pasan en Release.

**La GPU no rinde acá** (regla 8 pide la GPU cuando hay una): para dar las mismas esquinas bit a bit, el cálculo tiene
que ser en doble precisión, que en las placas de consumo corre de 32 a 64 veces más lento que en simple. Y habría que
copiar a la GPU todos los puntos de todas las paredes (en la gota, unos 15 millones, unos 240 MB, estimado). Con el
cambio, la costura alineada ya cuesta lo mismo que `nearest`: no queda nada para ganar. Se avisa al equipo de fichas.

**Sigue:** la pared exterior en espiral (pedido del dueño, ficha `walls_extra.md`, regla 4, costura en bufanda con la
vuelta entera), V3 y V4 con la regla de 0,005 mm, RV7 con una pared y RV9.

## 2026-10-26 — costura en bufanda

**Pedido del dueño:** una forma de imprimir la pared de afuera como en el modo vaso, en espiral, para que la calidad
sea alta y casi no haga falta costura. La ficha `walls_extra.md` ya lo tenía como «costura en bufanda» (regla 4), con la
opción de la vuelta entera.

**Hecho:**
- **`scarf_loop`** (`src/gcode/scarf.*`): el lazo, desde su costura, se vuelve un camino abierto:
  - una rampa de `walls.scarf_length` antes de la costura, la boquilla subiendo desde la capa de abajo (más
    `walls.scarf_start_height`) hasta la capa, con el cordón tan alto como la boquilla;
  - el lazo entero a la altura de la capa, y sus últimos milímetros otra vez sobre la rampa con el cordón cada vez más
    fino: el filamento de un lazo común;
  - con `walls.scarf_entire_loop`, la rampa y su segunda pasada dan la vuelta entera;
  - la rampa y su segunda pasada en tramos de `walls.scarf_max_segment` como mucho.
- **El escritor** la aplica en la etapa en paralelo, después de simplificar (así la simplificación no junta los tramos
  de la rampa): la pared externa de los contornos (`contours`) o también la de los agujeros (`everywhere`), las
  internas con `walls.scarf_inner`, nunca en la primera capa ni sobre recorridos que ya salen de la capa (ZAA,
  relleno ondulado), y con `walls.scarf_only_smooth` (por defecto) solo en lazos sin giros de más de 30°. El alto de
  cada cordón va en los puntos (`dz` y `below`), como en el relleno ondulado.
- **El verificador** acepta esos bloques hasta la capa de abajo, un cordón no planar que termina en nada (el pie de la
  rampa) y, en todo cordón, el medio milésimo que puede esconder un `;HEIGHT` de 3 decimales (los primeros cordones
  de la rampa miden milésimas). El fuzzer, con la bufanda al azar, encontró además un caso viejo de ZAA que fallaba
  por eso mismo: un cordón de 0,009 mm en una capa de 0,046 mm.
- Los ajustes `walls.scarf*` solo los lee el G-code: cambiarlos rehace solo el G-code (V11).

**Medido** (cilindro de 128 lados, 10 mm de radio, capas de 0,2; capa 10, como la prueba W1 de la ficha):

| | Pared externa | Z | Filamento |
|---|---|---|---|
| Sin bufanda | 61,55 mm | 2,2 | 2,0472 mm |
| `everywhere` | 81,55 mm (lazo + 20) | de 2,0 a 2,2; termina en la costura de siempre | 2,0473 mm |
| Vuelta entera | 123,10 mm (dos vueltas) | de 2,0 a 2,2 | 2,0472 mm |

- El filamento de toda la pieza es el mismo con y sin bufanda (357,65 mm).
- **RV6:** con la bufanda apagada (por defecto), el G-code de los 21 casos de medición es el mismo; solo se suman las
  líneas de los ajustes nuevos al pie.
- **Pruebas:** rampa, largo, vuelta entera, altura inicial, filamento igual y tramos de 1 mm en un cuadrado;
  esquinas marcadas; en el G-code de un tubo, `contours` contra `everywhere` y la primera capa sin rampa; un cubo con y
  sin `walls.scarf_only_smooth`; todo verificado. 381 casos pasan. Fuzzer de G-code con la bufanda al azar en un
  tercio de las corridas: 3000 corridas (semillas 26 y 27), 0 fallas.
- **Sin probar:** una impresión real. La ficha mide la regla en Tisma Slicer como caja negra; las cifras exactas de W1
  dependen de su cilindro, así que las pruebas comparan la forma (lazo + rampa, desde la capa de abajo, mismo
  filamento), no los números.

**Sigue:** el panel de ajustes con todo lo agregado (pedido del dueño: entre Cura y OrcaSlicer, simple y sin cargar de
información). Más adelante, si el dueño quiere, una pared externa en espiral continua entre capas (como el modo vaso,
pero con paredes internas y relleno): la pared externa primero en cada capa, subiendo una capa por vuelta.

## 2026-10-26 — panel de ajustes por secciones y niveles

**Pedido del dueño:** poner en la interfaz todo lo agregado (perfiles, modos especiales, los ajustes nuevos), ordenado
como el menú de ajustes de Cura y con el estilo de OrcaSlicer, «un híbrido», simple, fácil e intuitivo, sin cargar de
información. La ficha `settings_panel.md` ya lo describe: de OrcaSlicer, pocas secciones grandes con apartados finos;
de Cura, los apartados plegables y los ajustes dependientes debajo de su interruptor (comparación de caja negra de la
ficha; el motor no miró el código de ninguno).

**Hecho:**
- **`src/ui/settings_layout.*`:** dónde va cada ajuste (pestaña, sección, apartado) y su nivel, y el resumen de cada
  apartado. Pruebas: toda clave que escribe el motor tiene lugar (salvo `compute.*`, una preferencia), ningún apartado
  pasa de 8 ajustes, cada clave aparece una vez, Básico es exactamente la lista de la regla 10, y los resúmenes en
  castellano y en inglés.
- **El panel** (`apps/tisma/app.cpp`): pestañas Impresión, Material e Impresora con su perfil; el nivel a la derecha;
  la columna de secciones con íconos propios (`assets/icons/section_*.svg`); las tarjetas plegables con su resumen y
  «N cambios»; los dependientes ocultos con su interruptor apagado; un ajuste cambiado se ve en cualquier nivel con su
  etiqueta gris; «N ajustes más…» con **Mostrar todos**; nivel, secciones y tarjetas recordados en `interface.json`.
- **Todos los ajustes tienen fila**, también los que no la tenían: la bufanda, la compensación, el corte, la
  separación, interfaz y patrón de los soportes, las capas de falda, el ventilador entero, la velocidad mínima, el
  avance de presión, el flujo máximo, el viaje mínimo para retraer, las necesidades del material y los límites de la
  máquina. Las listas (`changes`, `machine.variables`) muestran cuántas hay; se editan en el perfil.
- `--level` y `--section` en la línea de comandos de la interfaz, para las capturas.

**Capturas** (cubo de 20 mm, Xvfb 1600 × 1000): Calidad en Normal; Resistencia; Material › Enfriamiento en Avanzado;
Básico, que esconde Velocidad y Otros; y un perfil con relleno al 40 % y costura atrás en bufanda: «1 cambio» en
Relleno, «2 cambios» en Costura, el punto en las secciones.

**Falta** (de la ficha): Personalizado y su diálogo, «Ver todo junto», la búsqueda que abre la tarjeta, Restablecer por
tarjeta, la tarjeta de la impresora (boquilla, cama, cámara, conexión) y su configuración aparte; hoy la impresora es
una pestaña más. Los modos especiales (`vase_mode.md`) llegan con el modo vaso, que el motor todavía no tiene: la tarjeta
aparece cuando haya algo que mostrar (regla 7).

## 2026-10-26 — modo vaso

**Por qué:** el pedido del dueño de una pared en espiral, «como el Spiral Vase», y la tarjeta Modos especiales del
panel nuevo (ficha `vase_mode.md`).

**Hecho:**
- `vase.enabled` (perfil de impresión). El corte se rehace con una pared, sin relleno ni capas de arriba (regla 1), y
  deja relleno solo en las capas de abajo; la última de ellas es la superficie de arriba (regla 3).
- **Las vueltas** (regla 4): desde la capa sobre las de abajo (la segunda, sin ninguna: regla 5), una capa cuya única
  trayectoria es el lazo de su pared externa es una vuelta. Empieza en el punto más cercano a donde terminó la capa de
  abajo, y si esa fue una vuelta, exactamente ahí: el cambio de capa no sube Z y no hay viaje ni retracción. La
  boquilla sube una capa a lo largo del largo recorrido; cada cordón tiene la vuelta de abajo a la misma distancia,
  así que el caudal es el de siempre, también en la primera y la última vuelta. El tiempo mínimo de capa vale por
  vuelta (regla 6). Una capa con dos contornos (dos islas, un agujero) se imprime plana y la vuelta siguiente empieza
  de nuevo.
- **Lo que no se puede** (regla 2): con soportes, los ajustes se rechazan; con varios objetos en la placa, la interfaz
  no lamina (el motor recibe una sola malla).
- **Verificador:** acepta las vueltas hasta la capa de abajo. El fuzzer, con el vaso al azar, encontró además que
  una torre de flujo (caudal forzado) con bufanda acelera los cordones finísimos de la rampa hasta el tope de la
  máquina, más de lo que el verificador esperaba: con bufanda o ZAA, el tope es el de la máquina.

**Medido** (cilindro de 10 mm de radio y 5 de alto, 128 lados, capas de 0,2, como VA1 y VA2 de la ficha): capas 0 y 1,
pared y relleno sólido; capa 2, pared y superficie de arriba; desde la 3, una vuelta: la capa 10 va de Z 2,0 a 2,2,
mide 61,55 mm y empieza y termina en el mismo punto; desde la capa 4, ningún viaje ni retracción.

**Pruebas:** VA1 a VA4 (cilindro y cubo de la ficha: tipos por capa, Z de cada vuelta, la vuelta que sigue desde donde
terminó la anterior, caudal constante, cubo sin capas de abajo y con dos, rechazo con soportes). RV6: con el vaso
apagado, el G-code de los 21 casos es el mismo salvo la línea del ajuste nuevo al pie. Fuzzer: 3000 corridas (semillas
31 y 32) con vaso y bufanda al azar, 0 fallas.

**Pregunta para el equipo de fichas:** `settings_panel.md` pone las velocidades en Impresión y la retracción en
Material, pero dice que la pestaña la decide el perfil de la clave; en el motor, las velocidades y la retracción son
del perfil de la impresora (decisión del dueño). Hoy el panel sigue las tablas de la ficha.

**Sigue:** una pared externa en espiral continua en piezas con relleno (la idea del dueño, más allá del modo vaso:
la pared externa primero en cada capa, subiendo una capa por vuelta, y las internas y el relleno planos), si el dueño
la quiere después de ver el modo vaso y la bufanda.

## 2026-10-26 — caza de errores

**Por qué:** pedido del dueño, buscar errores de impresión y del sistema mientras se armaba el panel.

**Hecho y medido:**
- **Fuzzer con ASan:** 400 corridas, 0 fallas. **Sin ASan:** tandas de 3000 corridas, limpias después de los
  arreglos de la bufanda y el vaso (el redondeo de `;HEIGHT`, la altura 0 en bloques no planares, el caudal forzado con
  bufanda; están en sus entradas).
- **La interfaz con ASan:** 36 estados de la interfaz, ningún error de memoria; las únicas fugas son 112 bytes dentro
  del controlador GL de Mesa.
- **Calidad del G-code** (script propio sobre el Benchy y la calavera): ninguna extrusión repetida en la misma Z,
  ningún viaje por debajo de la capa. Viajes sin retracción
  que cruzan una pared externa: 18 en el Benchy y 6 en la calavera; quedan para RT1 a RT5 (`toolpath_time.md`, reglas
  4 y 5, y «los viajes no cruzan perímetros si pueden ir por dentro» de `BEHAVIOR.md`).
- Las primeras capas de la calavera a 1,1 mm/s son correctas: 22 mm de recorrido por capa con 20 s de tiempo mínimo.

**Sigue:** los viajes que cruzan paredes, con RT1 a RT5.

## 2026-10-26 — soportes en árbol

**Por qué:** pedido del dueño («hay que agregar tree support»); ficha `supports_more.md`, reglas 15 a 17 y prueba SM13.
Ítem S3 de `PLAN.md`.

**Hecho:**
- `support.style` (`grid` u `organic`) y los ocho `support.tree_*` de la ficha, con sus límites, en el perfil de
  impresión y en `docs/CONFIG.md`.
- **`src/support/tree.*`** (diseño propio sobre la idea de Vanek et al. 2014; `PROVENANCE.md`):
  - bajo lo que cuelga, la interfaz entera, `support.interface_layers` capas sin el margen de la grilla, con líneas a
    45° y 135° en capas alternas y sin pared (regla 17);
  - debajo, las puntas, repartidas sobre el contorno y adentro, a la separación que dan la tasa, el diámetro de punta
    y la distancia entre ramas (ajustada a las cantidades de SM13);
  - al bajar, cada rama se inclina hacia su vecina más cercana hasta `tree_angle_preferred`, se junta con la que toca
    (con el área de las dos), engorda (la punta a 45° hasta el diámetro de rama, después según
    `tree_branch_diameter_angle`) y se aparta de la pieza hasta `tree_angle`; donde no puede, se apoya en la pieza
    con la separación en Z;
  - cada rama lleva una pared, dos donde cabe un disco de `tree_double_wall_diameter`, y el relleno del soporte
    adentro.
- `supported_overhangs` en `src/support/support.*`: lo que necesita soporte (automático, forzado y bloqueado), igual
  para los dos estilos, así que lo pintado vale también con árboles.
- **Panel:** el estilo en Soportes › Soporte (Normal); la tarjeta Árbol con los ocho ajustes, visible con el estilo en
  árbol, resumen «Ramas de 2 mm · puntas de 0,8 mm».
- **Verificador:** la zona permitida incluye el soporte (las ramas se apartan de la pieza más allá de su caja; el
  fuzzer lo encontró).

**Medido** (SM13: columna de 10 × 20 × 10 con una losa de 20 × 20 × 2 de Z 8 a 10; capas de 0,2):

| | Ficha (Tisma Slicer) | Motor |
|---|---|---|
| Puntas en Z 7,2 | 22, de 0,92 mm | 21, de 0,8 mm |
| `tree_tip_diameter` 1,5 | 18 | 18 |
| `tree_top_rate` 30 % | 32 | 33 |
| Interfaz | Z 7,4 a 7,8, a 45°, 135° y 45°; 524,5 mm por capa, X 99,4 a 109,8 | Igual en Z y ángulos; 465 mm, X 100,6 a 110 (el borde de los cordones) |
| Piezas en Z 7,0 / 6,2 / 5,2 / 4,2 / 2,2 / 0,4 | 13 / 11 / 9 / 7 / 4 / 1 | 21 / 19 / 15 / 8 / 7 / 3 |

Las ramas del motor se juntan más abajo que en la ficha (allá, una capa bajo las puntas ya se tocan muchas: engordan
más rápido). La interfaz de la ficha empieza 0,6 mm dentro de la columna (X 99,4 con la columna hasta X 100); el motor
la deja a `support.xy_gap` de ella. Pregunta para el equipo de fichas.

Benchy (filamento, mm; sin soportes 4049): con grilla 4576, con árboles 5396. Los árboles sostienen el ala del casco
que la grilla casi no toca (la separación XY se come su columna) y bajan troncos hasta la cama: en los primeros 10 mm,
20 m de recorrido contra 1 m. Esfera de 50 mm: sin soportes 8153, grilla 8502, árboles 8719. El G-code verificado en
los tres.

Tiempo de laminado (4 núcleos, mediana de 3): Benchy sin soportes 743 ms, grilla 1103, árboles 1155; esfera, grilla
540, árboles 716. La simulación de las ramas va en orden, capa por capa (cada una depende de la de arriba); las áreas y
las trayectorias, en paralelo.

**Pruebas:** `test_tree_support.cpp` (separación de las puntas; SM13: puntas bajo la interfaz, cantidad y distancia
entre ellas, menos y más gruesas hacia la cama, siempre a la separación de la pieza, interfaz y hueco; menos puntas con
punta de 1,5 y más con 30 %; el estante: ramas apoyadas en la base con la capa de separación vacía; las trayectorias de
una rama, de un tronco y de la interfaz). La matriz de G-code con «organic supports». Fuzzer: árboles al azar con sus
ocho ajustes.

**Sigue:** que las ramas engorden y se junten antes, como en la ficha (y con menos material); la base del soporte que
apoya en la pieza (`support.bottom_*`); después, RT1 a RT5.

## 2026-10-26 — investigación: no planar, arcos en voladizos y calidad en general

**Por qué:** pedidos del dueño: papers de impresión no planar «para sacar la opción más óptima», sobre arc overhangs,
y sobre impresión 3D en general para mejorar calidad y funciones.

**Hecho** (solo artículos, resúmenes, registros de patentes y documentación de usuario; ningún código):
- `docs/NONPLANAR_2026.md`: lo publicado de 2022 a 2026. F7 cambia de base: las dos superficies de corte en grilla 2D
  de Double QuickCurve (Eurographics 2025), resueltas como una ecuación tipo Poisson con el multigrid propio, en vez
  del campo por volumen de CurviSlicer (de 2 a 46 veces más lento, y falla en varias piezas). La pendiente útil en una
  impresora común es de 20 a 27° (la cara plana de la boquilla raya más arriba); contra plano, la rugosidad baja 34 a
  76 %, pero contra el Z anti-aliasing con subida lo que se gana es sobre todo una piel continua, sin medir; en 3 ejes
  la ganancia de resistencia no está probada. Orden en `PLAN.md`.
- `docs/ARC_OVERHANGS.md`: no hay un artículo revisado que mida los arcos solos; el único estudio con números
  (Andersons et al. 2026) encuentra que los arcos se hunden en sus centros y que los frentes en ondas se hunden
  bastante menos. S1 sigue la ficha, con la geometría que la ficha no fija como diseño propio, y después un modo de
  frentes por distancia. Seis preguntas de comportamiento para el equipo de fichas.
- `docs/QUALITY_2026.md`: diez pasos por ganancia contra esfuerzo (Q13 a Q18 en `PLAN.md` y en el mapa). Lo más
  importante: el planchado (Ra −60 a −83 %), que el motor no genera; el volumen de los cordones sueltos, que el
  rectángulo sobrestima entre 11 y 27 % y explicaría P11; y una patente de WARF (US 10,353,378 B2, hasta cerca de
  2036) que toca el análisis «como impresa» que ya existe. Las patentes nuevas, en la sección 6 de
  `docs/RESEARCH_MAP.md`.
- Del equipo de fichas (`dev`, PR #28): la segunda prueba de caja negra (QA10 a QA13, el soporte en grilla del
  estante: margen, separación XY, líneas y primera capa sobre el soporte), la cobertura contra Tisma Slicer con el
  orden aprobado por el dueño, y su propia lista de papers. Anotados en `PLAN.md`.

**Sin leer completo:** varios números salen de resúmenes o de notas de prensa (cada informe lo marca); los de las
ondas hay que confirmarlos en el artículo, que es de acceso abierto.

**Sigue:** según el orden aprobado por el dueño (`docs/spec/qa/2026-10-26-cobertura.md`): formatos de entrada (3MF y
OBJ en la línea de comandos) y centrar la pieza en la cama; después QA10 a QA13 del soporte en grilla.

## 2026-10-26 — OBJ, 3MF y la pieza en el centro de la cama

**Por qué:** el primer punto del orden aprobado por el dueño (`docs/spec/qa/2026-10-26-cobertura.md`): el motor leía
solo STL («binary STL is truncated» con el 3MF del cráneo) y no centraba una pieza que viene fuera del centro.

**Hecho** (todo propio, desde las especificaciones públicas; ninguna biblioteca nueva):
- `src/io/zip.*`: el ZIP de PKWARE (directorio central, ZIP64) y DEFLATE de la RFC 1951, con el CRC de cada entrada.
- `src/io/xml.*`: un lector de XML de a una etiqueta.
- `src/io/model.*` y `src/io/threemf.*`: OBJ (caras de cualquier cantidad de vértices, índices negativos) y 3MF
  (objetos de la placa con sus transformaciones, componentes anidados, mallas en otros archivos del paquete, unidades,
  espejos con las caras hacia afuera, sin objetos de soporte ni ítems no imprimibles, sin los modificadores de un
  proyecto de laminador). `read_model` elige por el contenido y la extensión.
- `src/io/checksum.*`: el CRC-32 y el Adler-32 que estaban en la interfaz, ahora en el núcleo.
- `placement.arrange` y `placement.center` (`src/slice/placement.*`): con cama, la línea de comandos centra la pieza.
- La interfaz abre, suma y recibe soltados OBJ y 3MF.

**Medido:** el 3MF del cráneo del dueño (9,4 MB, 45,9 MB de XML, 546 684 triángulos) se lee en 0,62 a 0,68 s; antes
fallaba. Laminado con una cama de 250 × 210: sin soportes 5,07 s, con grilla 7,53 s, con árboles 7,91 s (G-code
verificado en los tres; filamento 5943, 8357 y 10 231 mm).

**Pruebas:** `test_formats.cpp`: los flujos DEFLATE de cada tipo de bloque (dinámico, fijo, guardado, por tramos, una
copia 23 000 bytes atrás, vacío) contra los datos que el test arma otra vez, cortados y con un tope menor; OBJ (FM2) y
sus errores; los 3MF de la caja (FM3 en pulgadas, FM9 en dos partes, guardado, con un soporte, con un modificador, con
la malla en otro archivo y en espejo); 400 paquetes con bytes cambiados al azar, que se leen o se rechazan sin
romperse; el XML; y la ubicación (PL1, PL2, PL3, PL6). En la línea de comandos: `info` y `slice` con OBJ y 3MF, y el
cubo en una cama lejana, centrado (y con `placement.arrange` en `false`, el error de antes). Los archivos los arma un
guion propio, `tests/data/formats/make.py`.

**No probado:** ZIP64 (sin un archivo de más de 4 GB ni de más de 65 535 entradas a mano); proyectos reales de Bambu
Studio y OrcaSlicer (la ficha pide muestras con permiso).

**Sigue:** QA10 a QA13 del soporte en grilla; después, del mismo orden, velocidades y aceleraciones por tipo, relleno
de huecos, planchado, balsa y los extras de G-code.

## 2026-10-26 — soporte en grilla según la ficha (QA10 a QA12)

**Por qué:** la segunda prueba de caja negra del equipo de fichas (`docs/spec/qa/2026-10-26-segunda-prueba.md`): el
soporte no tenía el margen de la regla 2 y en el cráneo salía en 96 pedazos sueltos; la separación XY, las líneas y la
interfaz no seguían las reglas 2, 5 y 6.

**Hecho:**
- **QA10:** cada voladizo se agranda 1,6 mm antes de sumarse a la columna (`SupportParams::margin`; la grilla de bits
  crece lo mismo).
- **QA11:** `support.xy_gap` acepta mm o un porcentaje del ancho de las paredes; por defecto, 50 %.
- **QA12:** `support.line_angle` (nuevo, 0°): la base en líneas en esa dirección en todas las capas, a
  `support.spacing` más la separación de un cordón (2,857 mm); la interfaz a 90°, a la separación de los cordones
  (0,357 mm). Los árboles siguen con la interfaz a 45° y 135° (`supports_more.md`, regla 17).

**Medido** (la pieza de S4, capas de 0,2, cordones de 0,4): la zona de Y 88,4 a 111,6 y hasta X 111,6 (la ficha: 1,6
mm más allá de la tabla en +X, +Y y −Y); filas de la base cada 2,857 mm en las capas 10, 11 y 37; interfaz en columnas
cada 0,357 mm, 516,4 mm por capa (la ficha: 524,5); el borde de las líneas junto a la columna a 0,4 mm de su cara (la
ficha, 0,425 con un perímetro de 0,45: la separación es la mitad del ancho).

**Pruebas:** la del soporte bajo la «T» (zona con el margen, separación de la columna, filas en una dirección cada
`spacing` más un cordón, interfaz cruzada y densa), la de la pintura (el forzador más el margen), y la lectura de
`support.xy_gap` y `support.line_angle`.

**Falta:** QA13, la primera capa de la pieza sobre el soporte con la altura de una capa más la separación y sus
perímetros como `;TYPE:Overhang perimeter`: necesita alturas y tipos por tramo en el escritor. Y el ancho del soporte
(`supports_more.md`, regla 18: el diámetro de la boquilla, no el de las paredes).

## 2026-10-27 — mantenimiento (00:40)

**Revisado:** `origin/dev` y `origin/main` sin cambios nuevos; fuzzer con semillas nuevas (1201, 1202 y 1301: 2400
corridas, 0 fallas); casos borde de lo agregado hoy (3MF dañados, árboles con ajustes extremos, orden de recorridos).

**Encontrado y arreglado:**
- **Árboles con ajustes extremos** (puntas de 0,2 mm, `tree_top_rate` 100 %, `tree_branch_distance` 0,3) bajo una
  losa de 100 × 100 mm sobre una pared: el laminado agotó la memoria (unas 180 000 puntas) y el sistema lo mató; el
  contenedor de trabajo se reinició. Las puntas ahora nunca quedan más juntas que su ancho ni que dos cordones, y las
  búsquedas de vecinas (cobertura de puntas, la vecina más cercana, las ramas que se tocan) van en una grilla de
  celdas: antes eran cuadráticas en la cantidad de ramas. SM13 da el mismo G-code byte a byte; en la esfera y el
  cráneo los árboles cambian un poco (una rama que crece al juntarse vuelve a buscar), con el G-code verificado.
- **Escribir G-code con muchos lazos** (los de las ramas): `order_paths`, con costura `nearest` (la de los rellenos),
  buscaba en cada paso el punto más cercano de todos los lazos que quedaban. Ahora mira las cajas de los lazos, de la
  más cercana con un montículo, y corta cuando una caja queda más lejos que el mejor inicio; los tramos abiertos van
  en la grilla aunque haya lazos. Mismo G-code byte a byte (Benchy y esfera con relleno concéntrico y soportes; los
  modelos del banco con y sin soporte).
- **Un 3MF que dice inflar a 4 GB** pedía toda esa memoria y terminaba en «error: internal: std::bad_alloc»; ahora se
  rechaza antes, porque DEFLATE no infla más de unas 1032 veces.

**Medido** (4 núcleos):

| | Antes | Ahora |
|---|---|---|
| Losa de 100 × 100 mm con árboles (por defecto): laminar / escribir | 3,8 s / 17,1 s | 4,1 s / 0,83 s |
| La misma, ajustes extremos | se queda sin memoria | 2,6 s / 2,9 s |
| Benchy, escribir | 90 ms | 79 ms |
| Esfera de 1 millón de triángulos, laminar | 1,16 s | 1,08 s |
| Análisis del cubo | 12,9 s | 12,6 s |

**Pruebas nuevas:** el orden con costura `nearest` entre 150 lazos y 150 tramos contra mirar cada punto; árboles con
ajustes extremos (cantidad acotada de puntas, ninguna a menos de dos cordones); el 3MF de 4 GB.

**Release 0.9.0-alpha.1:** `VERSION`, su sección del CHANGELOG (lo de los bloques desde la 0.8.0, en una sola lista
por tipo) y la fila de `docs/VERSIONING.md`; `tools/release_notes.py --check` pasa; la CI de Linux pasó en el commit
anterior y las pruebas de nuevo con la versión nueva.

**Sigue:** QA13; las ramas de los árboles que engordan y se juntan antes, con las mediciones nuevas del equipo de fichas
(a la altura de las puntas, pares que se tocan; cada rama pasa de 0,9 a 1,2 mm en una capa).


## 2026-10-27 — QA13: la primera capa sobre el soporte y los perímetros en voladizo

**Por qué:** pedido del dueño de seguir en orden lo pendiente; lo primero, QA13 de la segunda prueba de caja negra
(`docs/spec/qa/2026-10-26-segunda-prueba.md`). La respuesta del equipo de fichas (regla 4 de `supports_skirt.md`,
rama `spec/next` 2f55a4b, `tools/check_spec.py` limpio) se fusionó en `dev`.

**Hecho:**
- **Regla 8 de `walls_extra.md`** (`walls.detect_overhangs`, nuevo, encendido): un tramo de pared cuyo cordón queda
  entero fuera de la capa de abajo (su eje fuera de la región de abajo agrandada medio cordón) es `Overhang
  perimeter`, a la velocidad de los puentes (`speed.solid_infill`) y con su ventilador (`cooling.fan_bridges`). La
  pared se corta justo donde cruza ese borde. En la interfaz, Resistencia › Paredes (Avanzado).
- **Regla 4 de `supports_skirt.md`:** donde la primera capa de la pieza cuelga sobre el tope del soporte, sus
  perímetros y su puente llevan la altura de la capa más la separación en Z (`;HEIGHT:0.400` y el doble de filamento
  por mm con capas de 0,2); el puente entra 0,6 mm sobre la parte apoyada, sacado del relleno disperso. El resto de la
  capa y la capa siguiente, normales.
- Los soportes se calculan antes de las zonas del relleno (no dependen de ellas), para que el anclaje del puente quede
  en las superficies antes de que el relleno las use.

**Medido:**

| Estante S4, Z 8,2 | Ficha (Tisma Slicer) | Antes | Ahora |
|---|---|---|---|
| Perímetros sobre el soporte | `Overhang perimeter`, alto 0,4, X 100,2 a 109,8 | comunes, 0,2 | `Overhang perimeter`, 0,4, X 100,2 a 109,8 |
| Perímetros sobre la columna | comunes, 0,2 | comunes, 0,2 | comunes, 0,2 |
| Puente | alto 0,4, X 99,41 a 109,04 | 0,2, X 100,2 a 108,6 | 0,4, X 99,4 a 108,6 |
| Z 8,4 | normal | normal | normal |

El puente termina en 108,6 y no en 109,04: sus líneas van en otra dirección que las de Tisma Slicer (el motor no
cuenta el soporte como apoyo al elegir el ángulo). Benchy: 218 mm de perímetros en voladizo en 34 capas; el laminado
tarda lo mismo (0,59 a 0,65 s antes y después, dentro del ruido). Cráneo escaneado (546 684 triángulos, 1503 tramos
en voladizo): mirando cada borde de lo que cuelga para cada tramo de pared tardaba 4,8 s en vez de 2,7; con un
índice por capa (la caja de cada polígono y los bordes en una grilla de celdas), 2,72 s contra 2,66 antes, con el
mismo G-code byte a byte; con soporte, 3,60 contra 3,42 s. Fuzzer con la detección al azar: 1300 corridas, 0 fallas.

**Pruebas nuevas:** QA13 en el estante (áreas, alturas, tipos, el anclaje y el doble de filamento por mm; sin la
detección, los tipos comunes y la altura igual); W2 (los dos tramos sobre el aire, a la velocidad de los puentes); el
ajuste nuevo (lectura, error y relaminado por etapas); el ventilador de los puentes también en los perímetros en
voladizo.

**No probado:** el cordón redondo de los puentes de la regla 8 (con `speed.bridges` y `gcode_motion.md`, en el paso 2
de la cobertura); los viajes de largo cero entre tramos de una pared escriben un `G1 F` suelto (ya pasaba con las
paredes partidas en esquinas: 812 en el Benchy), inofensivo.

**Sigue:** del punto 1 de lo pendiente, las ramas de los árboles que engordan y se juntan antes (SM13 remedido),
los arcos en voladizos (reglas 7 a 11 de `arc_overhangs.md`), PL15 de ubicación, QA8, QA9 y `support.width`.

## 2026-10-27 — árboles: puntas enlazadas de a pares (SM13 remedido)

**Por qué:** segundo ítem de lo pendiente, en orden: las ramas se juntaban más abajo que en la ficha. El equipo de
fichas remidió SM13 (regla y tabla en `supports_more.md`, `spec/next` 2f55a4b): una capa bajo las 22 puntas ya hay 13
piezas, 8 de ellas pares de puntas juntas de costado (unos 3,2 × 1,25 mm).

**Hecho:**
- Las puntas vecinas a no más del hueco entre puntas (la separación de las puntas menos su ancho) se enlazan de a pares,
  por el hueco más chico primero; el soporte llena la envolvente de los dos discos (`TreeNode::to`), y se acercan al
  ángulo máximo (`support.tree_angle`) hasta juntarse. Solo las puntas se enlazan: más abajo, las ramas se inclinan y
  se juntan como antes.
- Al juntarse, el radio queda a mitad de camino entre el de la más ancha y el de las dos áreas juntas.
- **Arreglo:** una rama absorbida por otra que después se apoyaba en la pieza dejaba su ubicación en la capa de
  separación; ahora la que absorbe se lleva sus ubicaciones y se vacían juntas (lo mostró la prueba del estante con
  los cambios).
- **Arreglo de lo de QA13 (sin publicar):** en el cráneo con soporte, el G-code se rechazaba por `;WIDTH:0.000`: al
  cortar una pared en el borde de lo que cuelga, la punta de un cordón que se afina a nada quedaba como un tramo propio
  sin movimiento. El escritor ya no escribe un ancho sin un movimiento que lo use, y no corta a menos de una micra de
  los extremos de un tramo. Prueba nueva con esa punta sola (falla sin el arreglo).

**Medido** (estante de SM13, piezas por capa en Z 7,2 / 7,0 / 6,2 / 5,2 / 4,2 / 2,2 / 0,4):

| | Piezas |
|---|---|
| Ficha (Tisma Slicer) | 22 / 13 / 11 / 9 / 7 / 4 / 1 |
| Antes | 21 / 21 / 19 / 15 / 8 / 7 / 3 |
| Ahora | 21 / 11 / 12 / 10 / 5 / 5 / 4 |

Probé también el radio de las dos áreas juntas (21 / 11 / 12 / 5 / 3 / 2 / 2) y el de la más ancha (21 / 11 / 12 /
10 / 9 / 8 / 6): el promedio de los dos queda más cerca en total. Cerca de la cama la ficha junta todo en una pieza de
11 × 22 mm y el motor deja 4. Benchy con árboles: 1,40 s contra 1,55 s antes (menos ramas); cráneo con árboles: 7,5 s
contra 7,0 s. Los tres soportes en el cráneo y el Benchy con soporte, verificados. Fuzzer: 800 corridas, 0 fallas.

**No probado:** los tamaños de cada pieza contra la ficha (la medida de la ficha es de borde a borde menos el ancho de
línea, y la de mi guion es otra); la base ancha de los troncos cerca de la cama.

**Sigue:** los arcos en voladizos (reglas 7 a 11 de `arc_overhangs.md`), PL15, QA8, QA9 y `support.width`.

## 2026-10-27 — arcos en voladizos (S1)

**Por qué:** tercer ítem de lo pendiente, en orden. Los arcos no estaban hechos: faltaban las reglas 7 a 11 de la
ficha `arc_overhangs.md` (el centro, hasta dónde, agujeros y tiras, el sentido, con soportes), que el equipo de fichas
agregó con las pruebas A2 a A5 (`spec/next` 2f55a4b, `tools/check_spec.py` limpio). También era un pedido del dueño
(P1, «Arc Overhang»).

**Hecho:**
- `src/infill/arcs.*` (diseño propio sobre la idea publicada; `PROVENANCE.md`): las zonas que salen de la capa de abajo
  por un solo tramo de borde (dos tramos son un puente; el borde entero, un anillo alrededor de una columna, sigue con
  su puente), con los pedazos cuyos tramos se tocan en un extremo juntos (la L de A2); un centro por zona, 1,2 mm
  adentro del medio del tramo apoyado; círculos cada `walls.width × (1 − arcs.overlap)` (0,36 mm), cortados por la zona
  y una mordida de 1,2 mm en lo que la sostiene, hasta el punto más lejano; uno en un sentido y el siguiente en el
  otro, partido en el eje (A1: los enteros en 0,684, 1,404, 2,124 y 2,844 contra 0,686, 1,407, 2,128 y 2,849 de la
  ficha); un pedazo que no se apoya en el arco anterior ni en la capa de abajo no va.
- En el laminado, los arcos toman el lugar del relleno de la zona y de la mordida; en el G-code van antes que las
  paredes de su isla, como `Overhang perimeter`, a `arcs.speed` (5 mm/s; 0, la de los puentes), con cordón redondo
  (`;HEIGHT:0.314` con ancho 0,4), el ventilador de los puentes y retracción antes de cada uno (`Toolpath::retract`).
  Con soportes, el soporte va igual y la capa lleva arcos en vez del puente grueso de QA13 (A5).
- Ajustes `arcs.enabled`, `arcs.overlap`, `arcs.speed`, `arcs.max_distance` (perfil de impresión; `docs/CONFIG.md`);
  en el panel, Soportes › Arcos en voladizos.

**Medido** (estante A1, Z 8,2): 598 mm de arcos (ficha 606), 0,0522 mm de filamento por mm (ficha 0,052245), F300, 67
retracciones en la capa (ficha 62); la capa 8,4, sólida. A5: el mismo resultado con el soporte debajo. Benchy con
arcos: G-code verificado, 1,11 s (0,62 sin arcos); cráneo: 3,5 s (2,7 sin arcos). Fuzzer con arcos al azar: 600
corridas, 0 fallas.

**Pruebas nuevas:** `test_arcs.cpp` (A1: centro, radios, vaivén y corte en el eje, hasta la esquina más lejana, largo;
A2: un solo grupo con el centro cerca del de la ficha, (98,8; 105,0) contra (98,8; 103,8); A3: el agujero corta y
nada cae en él; A4: la tira; un puente y un anillo sin arcos; `max_distance`); en `test_gcode.cpp`, A1 y A5 en el
G-code; los ajustes.

**No probado:** impreso (el estante plano de la torre de temperatura; necesita al dueño). Los arcos sobre las paredes
de la zona: acá las paredes se imprimen después, como `Overhang perimeter`, y los arcos llenan solo el relleno y la
mordida; la ficha no dice qué pasa con las paredes (en A3 el agujero lleva las suyas).

**Sigue:** que los arcos tarden menos (los círculos se muestrean contra toda la zona); PL15 de ubicación, QA8, QA9 y
`support.width`.

## 2026-10-27 — PL15: el centro sin cama

**Hecho:** sin `machine.bed`, la pieza sigue donde viene (regla 4 de `placement.md`, diseño), salvo que se dé
`placement.center` a mano: entonces va ahí, como en Tisma Slicer (PL15). Prueba nueva con el cubo de 10 a 30: queda en
10 a 30, igual con `placement.arrange` en `false`, y de 40 a 60 con el centro en (50, 50). Tisma Slicer lo pone de 90 a
110 sin centro porque siempre tiene una cama de 200 × 200; eso no se copia (lo dice la ficha).

**Sigue:** QA8 (la revisión de estabilidad), QA9 y `support.width`.

## 2026-10-27 — QA8: la revisión de estabilidad

**Por qué:** QA8 de la primera prueba de caja negra: una aguja de 1 × 1 × 80 mm no daba ningún aviso. La ficha
`stability.md` (`tools/check_spec.py` limpio) es la regla 6 de `print_review.md`.

**Hecho:** `src/report/stability.*` (estática y vigas de Euler-Bernoulli, de manual; `PROVENANCE.md`), solo en piezas
sin soportes:
- columnas: las islas de cada capa encadenadas con las de abajo que se tocan; la rigidez de una sección es el menor
  momento principal de la suma de las islas de la cadena (teorema de Green); bajando, hasta una capa diez veces más
  rígida (la base) o la cama;
- partes en el aire (crítico), todas en un aviso con su cuenta y sus capas;
- vuelco por el peso: el centro de masa de lo impreso fuera del polígono convexo del apoyo (la primera capa y el
  borde); crítico si el momento del peso pasa lo que aguanta la cama;
- columnas que se doblan: `review.nozzle_force · L³ / (3 · E_z · I_min)` contra `review.max_deflection`; sin medir el
  empuje, las tres columnas más flexibles como información (las que se doblan al menos una décima de la flecha
  máxima por newton);
- despegue de la cama, con los dos valores medidos;
- ajustes `review.*` (perfil de la impresora; Impresora › Máquina › Estabilidad), la gravedad en cada aviso del JSON
  (`severity`), y la revisión en la línea de comandos y en la interfaz.

**Medido:** pruebas ST1 (parte en el aire en Z 10,2, crítico y primero), ST2 (vuelco desde Z 10,4), ST3 (desde Z
25,6), ST4 (0,385 mm/N en Z 40), ST5 (cubo sin aviso), ST7 (despegue desde Z 6,4), ST8 (base en la placa, desde Z
27,6) y la aguja de QA8 (787 mm/N). Benchy: 0,95 → 0,99 s; cráneo: 4,3 → 4,6 s. En el Benchy sin soportes, partes en
el aire entre Z 6,2 y 18 (los techos de las ventanas) y tres columnas como información.

**No probado:** con una impresora real (la calibración de la regla 6, necesita al dueño); `E_z` es el del PLA para todo
material hasta que el perfil del filamento traiga el suyo.

**Sigue:** QA9 (paredes más finas que la boquilla, `walls.min_line_width`) y `support.width`.

## 2026-10-27 — QA9: paredes más finas que la boquilla

**Hecho:** `walls.min_line_width` (85 % de `machine.nozzle_diameter`) y `walls.min_detail` (25 %), mm o porcentaje
(ficha `perimeters.md`, regla 3): en el reparto de cordones (Kuipers et al. 2020), un espesor desde el detalle mínimo
que daba cero cordones da uno, y una pared de un solo cordón nunca es más angosta que el ancho mínimo, centrada. En el
panel, Resistencia › Paredes (Avanzado).

**Medido:** la pared de 0,2 mm de QA9 sale con cordones de 0,34 mm en el medio (antes, «nothing would be printed»); la
de 0,05 mm, nada. Benchy: capas con detalles perdidos 9 → 3; cráneo: 31 → 8; los dos G-code verificados.

**Pruebas nuevas:** la pared de 0,2 mm (ancho, centro, `;WIDTH:0.340`, el ancho en mm), la de 0,05 mm; la lectura de
los ajustes.

**Sigue:** `support.width` (`supports_more.md`, regla 18), lo último del punto 1.

## 2026-10-27 — `support.width` y fin del punto 1

**Hecho:** `support.width` (ficha `supports_more.md`, regla 18): el ancho de los cordones de la base, la interfaz y los
árboles, en mm o en % de la altura de capa (150 % con capas de 0,2 da 0,3 mm); 0, el de las paredes. La separación de
la interfaz y el paso de la base salen de ese ancho. En el panel, Soportes › Patrón (Avanzado). Prueba SM14 (0, 0,6,
150 %, con grilla y con árboles). La primera capa del soporte con el ancho de la primera capa (también de SM14) espera
a `layers.first_line_width`, que el motor no tiene todavía.

**Punto 1 de lo pendiente, terminado:** QA13 y los perímetros en voladizo, los árboles de SM13, los arcos en
voladizos, PL15, QA8, QA9 y `support.width`.

**Sigue (punto 2, en el orden aprobado):** velocidades y aceleraciones por tipo, relleno de huecos, planchado, balsa,
cambio de color y pausas, y los extras del G-code.

## 2026-10-27 — Punto 2: velocidades propias, topes del relleno y el cordón redondo de los puentes

**Hecho** (ficha `speeds_accels.md`, reglas 1 a 5 y 14; `gcode_motion.md`, regla 1; `walls_extra.md`, regla 8):
- `speed.bridges` para los puentes y los perímetros en voladizo (y los arcos sin `arcs.speed`); sin valor, la del
  relleno sólido, como antes;
- el cordón de los puentes y de los tramos de pared sobre el aire es redondo, del diámetro de la boquilla por la raíz
  de `extrusion.bridge_flow_ratio`: su sección es π/4 del diámetro al cuadrado (0,052245 mm de filamento por mm con
  boquilla de 0,4) y las líneas del puente van a esa distancia. Los puentes sobre el soporte siguen con la altura de la
  capa más la separación (QA13);
- `speed.gap_fill` (el relleno de huecos, cuando lo haya), `speed.over_bridges` (mm/s o % del sólido: los tramos de
  relleno sólido justo encima de un puente sobre el aire, cortados en el borde del puente; no encima de las capas
  sólidas sobre el relleno), `speed.first_layer_infill` (mm/s o % del sólido, sin el tope de la primera capa),
  `speed.travel_z` (cambios de capa, saltos y subidas del ZAA);
- `extrusion.infill_max_speed` y `extrusion.infill_max_crossing_speed`: el tope del relleno disperso según si sus
  líneas se cruzan en la capa (`grid`, `triangles`, `stars`, los cúbicos) o no;
- todo en el panel (Velocidad y Filamento › Flujo, nivel Avanzado salvo `speed.bridges`) y en `docs/CONFIG.md`; las
  velocidades nuevas rehacen solo el G-code (V11).

**Corregido:** las pruebas de estabilidad guardaban punteros a los avisos de una revisión temporal (ASan, en la CI de
Linux de la sesión); ahora guardan copias.

**Medido:** M2/W2: 0,052245 mm/mm en puentes y perímetros en voladizo (la ficha: 0,052735 y 0,05224); con
`bridge_flow_ratio` 0,8, 0,8 de eso y el puente 1,10 veces más largo (la ficha: 1,098). V2 a V5 como en la ficha.
Benchy 0,65 → 0,68 s y cráneo 2,92 → 2,91 s (mediana de 5; sin cambio); los dos G-code verificados, también con todos
los ajustes nuevos puestos. Fuzzer con los ajustes nuevos: 400 corridas, 0 fallas.

**Pruebas nuevas:** `tests/test_speeds.cpp` (M2 y V10, V2, V3, V4, V5), W2 con el cordón redondo, y la lectura de las
claves nuevas.

**Sigue:** del mismo punto, las velocidades dinámicas en voladizos (V6), el ecualizador del caudal (V7) y las
aceleraciones por tipo (V8, V9); después, el relleno de huecos (V1, W6).

## 2026-10-27 — Punto 2: aceleraciones por tipo y velocidades dinámicas en voladizos

**Hecho** (ficha `speeds_accels.md`, reglas 6, 7 y 10 a 13):
- grupo `acceleration` (perfil de la impresora): `default`, `walls`, `outer_wall`, `infill`, `solid_infill`,
  `top_solid_infill`, `bridges`, `first_layer`, `travel` y `short_travel`, con los reemplazos de la tabla. Antes de
  cada extrusión, la de su tipo; al terminar el tipo, se vuelve a `default`; antes de cada viaje, la de los viajes (la
  de los cortos si es más corto que `retraction.min_travel`). Sin `default`, solo la de los viajes. `M204 S` en
  RepRapFirmware y Klipper; en Marlin `M204 P` al imprimir y `M204 T` para los viajes, cada uno cuando cambia. El
  tiempo estimado sigue cada cambio;
- `speed.dynamic_overhangs` y `speed.overhang_0` a `_3` (mm/s o % del perímetro externo; 15, 15, 20 y 25): la parte
  apoyada de cada tramo de pared sale de la arista más cercana del contorno de la capa de abajo (distancia y lado), cada
  0,5 mm cerca de ese contorno; la velocidad se interpola entre los cinco puntos de la regla 7, nunca más rápida que la
  de la pared; los tramos con velocidades a menos de 1 % se juntan, a su media. Los tramos en el aire, a `overhang_0`;
- en el panel, Velocidad › Voladizos, Aceleraciones y Aceleración de los viajes (Avanzado).

**Medido:** V6 con paredes de 0,45 como en la ficha: embudos de 20°, 30°, 40°, 45°, 55° y 60° con el perímetro externo
a 26,76, 24,87, 22,54, 21,11, 17,30 y 15,0 mm/s de media (la ficha: 27,0, 24,9, 22,6, 21,1, 17,31 y 15,0); de un
tramo a otro varía ±0,3 mm/s porque el contorno de cada capa se simplifica (`slicing.simplify_tolerance`). V8 y V9
como en la ficha (salvo la balsa). Costo con todo encendido (mediana de 5): Benchy 0,68 → 0,82 s, cráneo 2,83 → 4,02 s
(el cráneo cambia de apoyo en todas partes: 966 000 líneas contra 649 000); las aceleraciones solas no cuestan nada.
Los dos G-code verificados. Fuzzer con las opciones nuevas: 700 corridas, 0 fallas.

**Sigue:** el ecualizador del caudal (reglas 8 y 9, V7); después, el relleno de huecos (V1, W6).

## 2026-10-27 — Punto 2: ecualizador del caudal (fin de `speeds_accels.md`)

**Hecho** (ficha `speeds_accels.md`, reglas 8 y 9): `speed.flow_slope_down` y `speed.flow_slope_up` (mm³/s²). Donde
el caudal (sección del cordón por su velocidad) baja de una extrusión a la siguiente, el final de la primera se parte
en pedazos cada vez más lentos, con el caudal bajando a ese ritmo hasta el de la siguiente; donde sube, el comienzo de
la segunda, salvo después de una retracción. Hasta 16 pedazos, cada uno al caudal de su mitad; si el camino es más
corto que la rampa, la rampa se aprieta. En el panel, Velocidad › Topes (Avanzado).

**Corregido:** el verificador del G-code no contaba las velocidades nuevas al calcular la más rápida permitida (el
fuzzer lo encontró con un puente más rápido que los viajes): `fastest_extrusion` las suma. Era del commit anterior,
ya subido; prueba nueva que falla sin el arreglo.

**Medido:** V7: el perímetro interno baja de 60 a 30,x mm/s en 16 pedazos que duran 1,36 s (la ficha: 16 pedazos,
unos 1,4 s); la subida empieza a menos de 33 mm/s y llega a 60; el relleno después de una retracción, directo a 80.
Costo con las dos rampas (mediana de 5): Benchy 0,65 → 0,69 s, cráneo 2,77 → 2,92 s; los dos G-code verificados.
Fuzzer: 800 corridas con semillas nuevas, 0 fallas (después del arreglo).

**`speeds_accels.md` terminada**, salvo lo que espera a otras funciones (V1 al relleno de huecos; el tope de las
aceleraciones por los límites de la máquina; las aceleraciones de la balsa y la torre de purga).

**Sigue:** el relleno de huecos (`walls_extra.md`, W6 y V1).

## 2026-10-27 — Punto 2: paredes de ancho fijo y relleno de huecos

**Hecho** (ficha `walls_extra.md`, reglas 1 y 2; `speeds_accels.md`, V1): `walls.generator` (`variable`, como hasta
ahora, o `fixed`) y `walls.gap_fill` (encendido). Con `fixed`, hasta `walls.count` lazos de `walls.width` a esa
distancia, cada uno donde entra un cordón entero, todos en sentido antihorario; la región del relleno es la región
achicada `walls.count` anchos. Lo que queda (fuera del primer lazo, entre un lazo y el siguiente, adentro del último
cuando no hay lugar para relleno), de al menos medio cordón, se llena con cordones de ancho variable (la cadena de
Kuipers et al. 2020), isla por isla, `;TYPE:Gap fill`, a `speed.gap_fill`; sus extremos libres siguen medio ancho
hasta el borde del hueco, porque la sección del cordón es un rectángulo. En el panel, Resistencia › Paredes
(Avanzado). Falta el relleno concéntrico de ancho fijo.

**Encontrado:** el eje medio de ciertas regiones de varias islas da cordones muy afuera de la región (anotado en
`PLAN.md`, «Eje medio con varias islas», para un bloque de mantenimiento). El relleno de huecos va isla por isla, y
así no aparece; el verificador del G-code lo había rechazado en el Benchy con paredes fijas.

**Medido:** W6: perímetro externo de 41,2 mm y relleno de huecos de 19,1 mm (la ficha: 41,14 y 19,14); V1 a 20 y a
10 mm/s; sin relleno de huecos, solo el perímetro. Benchy y cráneo con paredes fijas: G-code verificado, más rápido
que con paredes variables (medido con la CI de Linux corriendo al lado: Benchy 0,94 → 0,87 s, cráneo 4,5 → 3,6 s) y
más corto (Benchy 133 000 líneas contra 337 000). Fuzzer con paredes fijas: 900 corridas, 0 fallas.

**Sigue:** planchado (`ironing.md`).

## 2026-10-27 — Punto 2: planchado

**Hecho** (ficha `ironing.md`, reglas 1 a 5): grupo `ironing` (`enabled`, `surfaces` `top`/`topmost`/`solid`, `flow`
15 %, `spacing` 0,1 mm, `speed` 15 mm/s, los de la ficha). En la misma capa y a la misma Z, después de los rellenos
de cada isla, líneas a 45° del relleno de arriba, en zigzag, sobre lo que la capa de arriba no cubre (o lo que nada de
más arriba cubre, o todas las superficies sólidas), también sobre los perímetros, hasta media boquilla del borde;
`;TYPE:Ironing`. Cada línea es un cordón de `spacing × flow` de ancho (redondeado a lo que escribe `;WIDTH`) y la
altura de la capa. En el panel, Calidad › Planchado.

**Corregido:** las etiquetas de `speed.overhang_*` terminaban en «%», que la prueba de textos lee como formato y ASan
la vio leer fuera de la cadena en la CI de Linux de la sesión (commit aparte). Esa corrida terminó con esa falla y
subí `dev` igual por no mirar el código de salida: el arreglo va en el siguiente envío, después de una CI completa.

**Medido:** IR1: solo en la capa de arriba, después de su relleno (135°), líneas a 0°, 0,1 mm, 15 mm/s, 0,001247 mm de
filamento por mm, ejes entre X 0,2 y 19,8 (la ficha: 0,205 y 19,795), largo dentro del 2 % de 3822 mm. Escalón y
techo: `top`, `topmost` y `solid` planchan lo que deben. Benchy y cráneo con planchado: G-code verificado (122 y 200
tramos de planchado). Fuzzer con planchado: 800 corridas, 0 fallas (antes del redondeo del ancho, 2 fallas).

**Sigue:** balsa (`skirt_brim_raft.md`).

## 2026-10-27 — Punto 2: balsa

**Hecho** (ficha `skirt_brim_raft.md`, reglas 3 y 9 a 14): `adhesion.raft_layers`, `raft_gap` (0,1), `raft_expansion`
(1,5), `raft_first_layer_density` (90 %), `raft_first_layer_expansion` (3), `speed.first_layer_over_raft` y
`acceleration.first_layer_over_raft`. La pieza se lamina como si estuviera en la cama (su primera capa con la altura
normal, sin pata de elefante ni borde) y debajo se ponen las capas de la balsa como capas del modelo: la primera con
la altura de la primera capa, las demás de 3/4 de la boquilla; arriba la interfaz densa a 135° y 45° alternados (como
mucho todas menos la primera, como miden R1 y R2; con una sola capa, esa), abajo la base a `support.spacing`, a
`support.line_angle` la de arriba y girada 90° en las alternas, la primera a su densidad. La balsa sigue la primera
capa de la pieza agrandada (el valor exacto, sin la grilla de Tisma Slicer), su primera capa más. La pieza sube la
balsa y la separación; su primera capa no pasa del tope sobre la balsa (o de la primera capa). La falda rodea la
primera capa de la balsa; sin borde. En el panel, Adhesión › Balsa y las filas en Velocidad.

**Corregido:** el verificador del G-code tomaba como cordón más angosto un cuarto de la pared al calcular el avance
de un flujo forzado (torre de flujo); con planchado hay cordones mucho más finos. Lo encontró el fuzzer (commit aparte).

**Medido:** R1 (base a 0° en Z 0,2 de 0,2; interfaz a 45° y 135° en 0,5 y 0,8 de 0,3; la pieza desde 1,1 con alto
0,2; 53 capas; con separación 0 y 0,2, desde 1,0 y 1,2), R2 (5 capas: base a 90° y 0°, interfaz 135°, 45°, 135°, la
pieza desde 1,7; 1 capa: interfaz, la pieza desde 0,5), R3 (capas de 0,1: balsa en 0,1, 0,4 y 0,7, la pieza en 0,9),
R5 con los valores exactos (4,5 y 1,5 mm), R6 (largo por la densidad: 0,56 de 50 contra 90), R7 (15 mm/s en la
primera capa de la pieza, 60 en la siguiente), R8 y R9 (falda a 6 mm de la base). Benchy y cráneo con 3 capas de
balsa: G-code verificado. Fuzzer con balsa: 800 corridas, 0 fallas (después del arreglo del verificador).

**Sigue:** cambio de color y pausas, y los extras del G-code (`gcode_extras.md`).

## 2026-10-27 — Punto 2: cambio de color, pausas y plantillas por capa

**Hecho** (ficha `gcode_templates.md`, reglas 11 y 13): `gcode.color_change` (`M600`) y `gcode.pause` (`M601`), que se
escriben al empezar la capa de un cambio por altura con `"color_change": true` o `"pause": true` (después de las
marcas `;COLOR_CHANGE` y `;PAUSE_PRINT`, para la vista previa); `gcode.before_layer` y `gcode.after_layer`, justo antes
y justo después del movimiento en Z de cada capa desde la segunda, con `layer_num`, `layer_z` y `max_layer_z`. Todos
van como G-code propio (`;TYPE:Custom`) y, si mueven algo, el viaje siguiente vuelve a escribir todos los ejes. En el
panel, Otros › Plantillas de G-code.

**Corregido:**
- en el modo vaso, después de G-code del usuario que mueve el cabezal (un cambio de color, una plantilla), la vuelta
  seguía sin viajar desde donde el cabezal ya no estaba; ahora viaja. Lo encontró el fuzzer;
- la prueba del verificador que corta el G-code a la mitad podía caer justo en un salto de línea (pasó al crecer el
  resumen final); ahora corta siempre a mitad de línea.

**Medido:** T8: `;BEFORE 1 0.4` justo antes de `G1 Z0.4` y `;AFTER 1 0.4 0.4` justo después, 49 de cada uno en 50
capas; cambio de color en la capa de Z 2,2 y pausa en la de 4,2 con cambios a 2 y 4 mm; verificados, también con una
plantilla que mueve Z y en el modo vaso. Fuzzer con plantillas, cambios de color y pausas: 800 corridas, 0 fallas
(después del arreglo del vaso).

**Sigue:** los extras del G-code (`gcode_extras.md`): peso y costo (X1), desfase de Z (X4), E volumétrica (X11),
cámara con espera (X2) y lo demás.

## 2026-10-27 — Punto 2: extras del G-code (peso y costo, cámara, desfase de Z, E volumétrica)

**Hecho** (ficha `gcode_extras.md`):
- regla 1 (X1): al final del G-code, `; filament used [mm]`, `[cm3]` y, con `extrusion.density`, `[g]`; con
  `extrusion.cost` (por kg), `; filament cost`; después los totales y `; total filament used for wipe tower [g] =
  0.00` (todavía sin torre). En el panel, Filamento › Precio, con el peso del carrete vacío (`spool_weight`);
- regla 3 (X2): con cámara, `M141` antes del G-code de inicio para que caliente junto con la boquilla; después del
  inicio, con `extrusion.chamber_min_temperature`, la espera (`M191 S…`, o `TEMPERATURE_WAIT` en Klipper) y otra vez
  `M141`;
- regla 5 (X4): `machine.z_offset` se suma a todas las Z de la pieza y al `;Z:`; no toca el G-code de inicio;
- regla 11 (X11): `gcode.volumetric_e`, toda E (extrusión, retracción, `G92`) en mm³; el verificador la lee con la
  sección 1.

**Medido:** X1 con PLA de 1,24 g/cm³ y 25 por kg: los valores cuadran con la sección del filamento al redondeo; el
Benchy local da 4 071 mm, 9,79 cm³, 12,14 g y 0,30 (laminado en 1,0 s con la CI de Linux corriendo al lado). X2: el
orden `M141 S50`, `M109 S200`, `M191 S40`, `M141 S50`, `M141 S0`. X4 con −0,3: la primera capa en `;Z:-0.100`.
X11: `E-2` sale `E-4.81056`, sin `M200`. 465 pruebas pasan; fuzzer con las opciones nuevas: 400 corridas, 0 fallas.

**Sigue:** el resto de `gcode_extras.md` (sabores viejos, modo silencioso, miniaturas, binario, arcos, comentarios
detallados, contracción).

## 2026-10-27 — Punto 2: contracción del filamento y comentarios detallados

**Hecho** (ficha `gcode_extras.md`):
- regla 4 (X3): `extrusion.shrinkage_xy` y `extrusion.shrinkage_z` (0 a 20 %): la pieza se lamina 100 / (100 − p)
  veces más grande, alrededor del centro de su base, como la escala de `compensation` (se multiplican). La ficha dice
  que en Tisma Slicer el ajuste no cambió nada desde la línea de comandos; el motor sí lo aplica, con el factor que
  propone la ficha. En el panel, Filamento › Contracción;
- regla 12 (X12): `gcode.verbose_comments`: cada línea que escribe el motor termina con `; ` y qué hace, con los
  textos de la ficha (`perimeter`, `infill`, `infill (bridge)`, `skirt`, `retract`, `unretract`, `lift`, `reset
  extrusion distance`, `simple layer change`, `travel to first layer point`, `move to first perimeter point`, `move to
  first infill point`, `enable fan`, `disable fan`…) y unos propios para lo que la ficha no nombra (`lower`, `set
  acceleration`, `support material`, `gap fill`, `ironing`). El G-code del usuario no cambia. Apagado no cuesta nada.

**Medido:** X3: un cubo de 20 × 20 × 10 con 1 % y 2 % se lamina de 20,202 × 20,202 × 10,204 mm, 51 capas en vez de
50. X12: en un cubo de 20 mm con relleno al 20 % y falda, 1684 `; infill`, 600 `; perimeter`, 161 `; retract`, 80
`; skirt`, 50 `; simple layer change`; sin los comentarios, línea por línea el mismo G-code que sin el ajuste, y el
verificador lo acepta. 467 pruebas pasan; fuzzer con contracción y comentarios: 400 corridas (semilla 1) y 1000
(semilla 271027), 0 fallas.

**Sigue:** los sabores viejos (X5), el modo silencioso (X6, con los límites de la máquina), los arcos (X10), el
G-code binario (X8) y las miniaturas (X7, con la interfaz).

## 2026-10-27 — Punto 2: dialectos de firmwares antiguos

**Hecho** (ficha `gcode_extras.md`, regla 6, X5): `machine.flavor` acepta además `repetier`, `smoothie`, `teacup`,
`makerware`, `sailfish`, `mach3`, `machinekit` y `no_extrusion`. Lo que cambia de cada uno está como datos en
`src/gcode/dialect.hpp` y lo leen el emisor, el verificador y la vista previa:
- `repetier` y `smoothie`: como `reprap` (el mismo archivo, salvo la línea del ajuste);
- `teacup`: `M106 S0` en vez de `M107`;
- `makerware` y `sailfish`: E absoluta sin `M82` ni `G92` (aunque se pida E relativa), `M126`/`M127`, `M104 S… T0`,
  `M73 P…` en cada capa; `makerware` también `M101 ; extruder on` y `M103 ; extruder off`;
- `mach3` y `machinekit`: el extrusor es `A`, `M104 P…`, `M106 P252.45`; `machinekit` con `G92 A0.00000` en cada
  capa y `M2` al final;
- `no_extrusion`: los mismos movimientos en X e Y, sin E y sin las retracciones (que serían solo `F`).
Los antiguos (salvo `repetier` y `smoothie`) no llevan `M204` ni avance de presión: la ficha no los mide y esas
máquinas no los conocen. En el panel, Impresora › Firmware.

**Medido:** X5 sobre un cubo de 20 × 20 × 10 mm (50 capas): `M73 P10` en la capa 5, `M104 S215 T0` en el cambio a
2 mm, `M106 P252.45` con el ventilador al 99 %; `no_extrusion` da tantos movimientos en X e Y como `reprap`; el
verificador acepta los once dialectos y la vista previa de `mach3` ve las mismas extrusiones que la de `reprap`. 468
pruebas pasan; fuzzer con los once sabores: 600 corridas (semilla 1) y 1000 (semilla 271028), 0 fallas.

**No probado:** ninguna de esas máquinas de verdad (no hay); solo contra la ficha.

**Sigue:** el modo silencioso (X6), que pide los límites de la máquina de `gcode_firmware.md`; los arcos (X10); el
G-code binario (X8); las miniaturas (X7, con la interfaz).

## 2026-10-27 — Punto 2: arcos `G2`/`G3`

**Hecho** (ficha `gcode_extras.md`, regla 10, X10): `gcode.arcs`. El ajuste (`src/gcode/arcs.*`, diseño propio) busca,
en cada trayectoria plana de un solo ancho, tramos de 3 movimientos o más cuyos puntos queden a menos de
`gcode.resolution` del círculo por el primero, el del medio y el último (los medios de los movimientos, a menos del
doble: la simplificación ya los dejó apartarse esa tolerancia), que giren siempre hacia el mismo lado y no pasen de
media vuelta; cada arco crece lo más que puede. El emisor los escribe como `G2`/`G3 X Y I J E F`, con la E del largo
del arco y el centro relativo al inicio tal como quedó escrito; el tiempo estimado sigue los movimientos que el arco
reemplaza. El verificador lee los arcos (largo por el círculo, el final sobre el círculo del inicio a 0,01 mm, el área
a lo largo del arco) y la vista previa los dibuja en tramos de 5° como mucho (antes, recta hasta el final).

**Medido:** cilindro de 10 mm de radio y 5 de alto (X10, 128 lados, relleno al 20 %): sin arcos 6341 `G1` y 179 kB;
con arcos 925 `G1`, 76 `G2` y 228 `G3`, 46 kB; con `resolution` 0,05, 1218 `G1` y 152 `G3` (cada vuelta de pared en
dos medias vueltas; la simplificación más gruesa deja menos arcos en las uniones del relleno). El mismo tiempo
estimado (4m 54s) y el mismo filamento al 0,2 %. Benchy local: 209 027 `G1` → 189 139 `G1`, 736 `G2` y 2 149 `G3`;
7,39 → 6,95 MB; el mismo tiempo estimado; el laminado tarda lo mismo dentro del ruido (con la CI de Linux corriendo
al lado). 470 pruebas pasan; fuzzer con arcos: 600 corridas (semilla 1) y 1500 (semilla 271029), 0 fallas.

**No probado:** en una impresora (Marlin necesita `ARC_SUPPORT`, Klipper `[gcode_arcs]`).

**Sigue:** de `gcode_extras.md` quedan el modo silencioso (X6), que pide los límites de la máquina de
`gcode_firmware.md`, las miniaturas (X7, con la vista 3D) y el G-code binario (X8). Con eso cierra el punto 2; sigue
el punto 3 (retracción y viajes completos, paredes extra, impresión secuencial).

## 2026-10-27 — Punto 2: firmware, límites de la máquina y modo silencioso

**Hecho** (fichas `gcode_firmware.md`, `gcode_extras.md` regla 7, `print_time.md` reglas 2, 3 y 7):
- límites nuevos en `machine`: aceleración máxima por eje (`max_accel_*`), de retracción, tirón por eje (`jerk_*`),
  desviación de unión y avances mínimos al extruir y al viajar; `machine.limits_usage` (`time_only`, `emit`,
  `ignore`). Con `emit` se escriben antes del G-code de inicio (Marlin 2: `M201`, `M203`, `M204 P R T`, `M205 X Y Z E`
  o `M205 J`, `M205 S T`; Marlin 1: `M204 S T`; RepRapFirmware: `M203` y `M566` en mm/min; Klipper: error) y ninguna
  aceleración por tipo los pasa (V9);
- la estimación usa esos límites: la aceleración y el avance de cada movimiento acotados por los de cada eje, la
  velocidad en las esquinas por la desviación de unión (el círculo que redondea la esquina a esa distancia) o por el
  tirón de cada eje, los avances mínimos y la aceleración de las retracciones; con `ignore`, no;
- `machine.silent_mode` y `machine.silent` (los límites de ese modo, por nombre): una segunda estimación; al final,
  `; estimated printing time (normal mode)` y `(silent mode)`, y los de la primera capa (la línea de siempre sigue);
- temperaturas al empezar (regla 1): con `variables.temperature`, `M104` antes del G-code de inicio y `M109` después
  (RepRapFirmware: `G10 S… P0` y `M116`; cada dialecto antiguo con lo suyo), salvo que el inicio ya tenga comandos de
  temperatura;
- Marlin con E relativa: `G92 E0` después del salto en Z de cada capa (regla 3);
- `gcode.progress` (regla 5): `M73 P R` por el tiempo estimado, al empezar y cada vez que cambia el porcentaje o los
  minutos, hasta `P100 R0`; con modo silencioso, `M73 Q S` (no en RepRap genérico). El G-code se guarda en memoria
  con marcas al empezar cada capa y cada trayectoria, y al final las marcas se cambian por los `M73`;
- sabores `marlin_legacy` (Marlin 1) y `reprapfirmware` (Duet), propuestos en la ficha;
- en el panel: Impresora › Límites (aceleraciones, tirones y esquinas, modo silencioso) y Firmware (uso de los
  límites, avance).

**Corregido:** el commit de los arcos `G2`/`G3` (470e2ee) había escrito sus pruebas encima de `tests/test_arcs.cpp`,
que tenía las de los arcos en voladizos (A1 a A4). Volvieron tal cual (10dea1e) y las de `G2`/`G3` pasaron a
`tests/test_gcode_arcs.cpp`. Lo noté al sumar el archivo de pruebas nuevo: estaba dos veces en `tests/CMakeLists.txt`.

**Medido:** FW1, FW2, FW4, FW5, FW7, FW8, FW9, X6 y V9 como pruebas. Benchy local con Marlin (aceleración 1500): sin
límites 1h 10m 17s; con los tirones de FW1 1h 14m 29s; con desviación de unión 0,013 mm 1h 14m 2s; modo silencioso
(960 mm/s², 100 mm/s en X e Y) 1h 19m 54s; primera capa 1m 8s (normal) y 1m 12s (silencioso); 350 líneas `M73`. El
laminado tarda lo mismo (0,93 a 0,98 s, con la CI de Linux al lado). 484 pruebas pasan; fuzzer con los límites, el
modo silencioso, `M73` y los sabores nuevos: 600 corridas (semilla 1) y 1500 (semilla 271030), 0 fallas.

**No probado:** contra una impresora (PT6 de `print_time.md` sigue pendiente).

**Sigue:** las etiquetas de objetos (regla 4) con la impresión secuencial; de `print_time.md`, leer los comandos de
movimiento y las esperas del G-code (reglas 8, 9, 13), el calentamiento (10, 14) y las esquinas como cada firmware
(12; Klipper). De `gcode_extras.md`, las miniaturas (X7) y el G-code binario (X8).

## 2026-10-27 — Punto 2: falda, escudo contra corrientes y tipos de borde

**Hecho** (ficha `skirt_brim_raft.md`, reglas 1 y 4 a 8):
- `adhesion.skirt_min_filament`: si las vueltas de falda gastan menos filamento que esto en la primera capa, se
  agregan vueltas hacia afuera hasta pasarlo (a lo sumo 1000);
- `adhesion.draft_shield` (`disabled`, `limited`, `enabled`): con escudo la falda se mide desde la pieza aunque haya
  borde, la vuelta del borde que cae debajo de ella se quita, y con `enabled` se repite en todas las capas;
- `adhesion.brim_type` (`outer`, `inner`, `outer_and_inner`, `none`): el borde de adentro sale de los agujeros de la
  primera capa hacia adentro, rodeando las islas que haya en ellos, hasta llenar el agujero;
- `adhesion.brim_gap`: el borde entero se aparta de la pieza, con el mismo ancho;
- el verificador cuenta las vueltas extra de la falda en su margen; en el panel, Adhesión › Falda y Borde.

**Medido:** B1 (cubo de 20 mm, una vuelta a 6 mm, 300 mm de filamento): 9153 mm de falda en la primera capa, las
vueltas justas para pasar los 300 mm (Tisma Slicer da 10337,7 mm; su cordón es más ancho, 0,45). B3: 14 vueltas de
borde sin escudo y 13 con él, la falda a 3,18 mm de la pieza; con `enabled` en las 50 capas, con `limited` y 5 capas
de falda en 5. B4 (tubo de 10 y 5 mm de radio, borde de 3 mm): 8 vueltas afuera de 10,18 a 12,68 mm (Tisma Slicer:
10,159 a 12,678), 8 en el agujero de 4,82 a 2,32 (4,801 a 2,319), con 1 mm de separación de 11,18 a 13,68 (11,168 a
13,673). 487 pruebas pasan; fuzzer con estos ajustes: 600 corridas (semilla 1) y 1500 (semilla 271031), 0 fallas.

**Sigue:** el lenguaje de plantillas (`gcode_templates.md`, reglas 1 a 10, 12 y 14 a 17); después, de `gcode_extras.md`,
el G-code binario (X8) y las miniaturas (X7, con la interfaz); luego el punto 3.

## 2026-10-27 — Punto 2: lenguaje de plantillas de G-code

**Hecho** (ficha `gcode_templates.md`):
- intérprete propio (`src/gcode/template.*`, reglas 1 a 8): `[nombre]` y `{expresión}` con números, textos, `+ - * /
  %` (entero entre entero da entero), comparaciones, `and`/`or`/`not`, el ternario, `=~ /regex/`, listas e índices;
  `min`, `max`, `int`, `round`, `digits`, `zdigits`, `is_nil`, `one_of`, `size`, `empty`, `interpolate_table`;
  `{if}/{elsif}/{else}/{endif}`; `local` y `global`. Un error (variable o función desconocida, mal usada, de
  sintaxis) hace fallar la exportación con la sección, la línea y una marca en la columna;
- variables (regla 7): cada ajuste por su nombre (`{layers.height}`), algunos con su nombre de siempre
  (`layer_height`…), las de `machine.variables` como listas, y `total_layer_count`, `input_filename_base`,
  `num_extruders`, `initial_extruder`, `initial_tool`, `is_extruder_used`, `first_layer_print_min/max/size`, `year`,
  `timestamp`, `layer_num`, `layer_z` y `max_layer_z`;
- todas las plantillas pasan por el intérprete: inicio y fin de la impresora y del filamento (nuevos
  `extrusion.start_gcode` y `end_gcode`, reglas 9 y 12), antes y después del cambio de capa, cambio de color, pausa y
  el G-code de `changes`;
- temperaturas automáticas (regla 10, `gcode.auto_temperatures`): sin comandos de temperatura en el inicio, `M190`
  de la cama y `M104` antes, `M109` después;
- `output.filename_format` (regla 15): con `slice -o carpeta`, el archivo se escribe con el nombre provisional y se
  renombra cuando se conocen `print_time` y `total_weight`;
- `output.post_process` (reglas 16 y 17): los scripts se corren con la ruta del G-code y las variables
  `SLIC3R_…`, después de preguntar (o con `--yes`); con `N` no queda G-code.

**Cambiado:** una variable desconocida ya no queda escrita: es un error. La prueba del catálogo que laminaba con un
perfil de impresora solo ahora usa también uno de filamento, como en la interfaz.

**Medido:** T1 a T5 (cuentas, textos, condiciones, funciones, variables propias) y T7 (errores) como pruebas del
intérprete; T6, T8, T9, T10 y T11 sobre el cubo de 20 mm; T12 desde la línea de comandos con un script (`sh` en Linux,
`cmd` en Windows) que agrega una línea con `SLIC3R_PP_HOST`, `SLIC3R_LAYERS_HEIGHT` y `SLIC3R_LAYER_HEIGHT`. 501
pruebas pasan; fuzzer con plantillas con condiciones, `digits` y `global`: 600 corridas (semilla 1) y 1500 (semilla
271032), 0 fallas.

**No probado:** la pregunta del posproceso respondida a mano (las pruebas usan `--yes`).

**Sigue:** de `gcode_extras.md`, el G-code binario (X8) y las miniaturas (X7, con la interfaz); luego el punto 3
(retracción y viajes completos, paredes extra, impresión secuencial).

## 2026-10-27 — Punto 2: G-code binario

**Hecho** (ficha `gcode_extras.md`, regla 9, X8): `gcode.binary` escribe el formato binario público (`.bgcode`):
encabezado `GCDE`, versión 1 y CRC32; bloques de datos del archivo (`Producer=Tisma engine …`), de la impresora
(`printer_model`, `filament_type`, `nozzle_diameter`, `layer_height`, temperaturas, `fill_density`, soporte,
planchado, filamento y tiempo), de la impresión (el resumen del final), del slicer (cada ajuste) y el G-code en
bloques de 64 KiB como mucho, cortados en finales de línea; sin compresión y el G-code sin codificar. La exportación
verifica el texto, escribe el binario, lo relee (estructura y CRC de cada bloque) y compara su G-code con el texto
antes de darle el nombre. Con `gcode.binary`, un nombre que termina en `.gcode` pasa a `.bgcode`, y
`read_gcode_totals` lee el tiempo y el peso de los datos de la impresión (para `slice -o carpeta`).

**Fuente:** la ficha y la descripción pública del formato que cita (solo el documento en prosa). Lo que el documento
no dice quedó como diseño propio: el CRC32 de cada bloque cubre su encabezado, sus parámetros y sus datos, y el tamaño
del bloque cuenta solo los datos.

**Pregunta para el equipo de fichas** (caja negra): abrir un `.bgcode` del motor en Tisma Slicer (visor de G-code) o
mandarlo a una impresora Prusa, y decir si lo lee. Si no, qué cubre el CRC en un archivo de Tisma Slicer (por ejemplo,
el CRC32 del primer bloque y sus bytes).

**Medido:** X8 como prueba (los primeros bytes `47 43 44 45`, `01 00 00 00`, `01 00`; los bloques en el orden 0, 3, 4,
2, 1; la lectura de vuelta; un byte cambiado rompe el CRC). El CRC-32 da `CBF43926` en «123456789», el valor de
control del estándar. Benchy local: 7 387 511 bytes en binario contra 7 386 489 en texto (sin compresión pesa lo
mismo). 504 pruebas pasan; fuzzer con ida y vuelta del binario: 600 corridas (semilla 1) y 1500 (semilla 271033), 0
fallas.

**No probado:** con un lector de otro programa ni con una impresora (no se pudo instalar uno en esta sesión).

**Sigue:** las miniaturas (X7) con la vista 3D de la interfaz; la compresión del binario. Con eso, el punto 2 queda
salvo esas dos; sigue el punto 3 (retracción y viajes completos, paredes extra, impresión secuencial).

## 2026-10-27 — Punto 3: retracción y viajes completos

**Hecho** (ficha `retraction_travel.md`, reglas 1 a 10):
- `retraction.unretract_speed` y `retraction.restart_extra` (regla 1): la vuelta a su velocidad y con su extra;
- `retraction.on_layer_change` (regla 2): todo cambio de capa retrae antes de subir;
- `retraction.wipe` y `retraction.before_wipe` (regla 3): la parte quieta primero, el resto mientras la boquilla vuelve
  sobre el camino impreso desde el último viaje, al 80 % de `speed.travel`, a lo largo de resto × velocidad /
  (0,95 × `retraction.speed`); si ese camino es más corto, lo que falta se retrae quieto al final;
- `retraction.lift_above` y `retraction.lift_below` (regla 4), también para la rampa;
- `travel.ramping_lift`, `travel.slope` y `travel.max_lift` (regla 5): la rampa sube desde donde está la boquilla
  hasta la altura y baja en vertical al final; el primer viaje de una capa sube desde la capa de abajo (el movimiento
  en Z del cambio de capa espera a la rampa, salvo que haya G-code después del cambio de capa);
- `travel.lift_before_obstacle` (regla 6) se lee y se guarda; no cambia el G-code porque la ficha no lo midió;
- `retraction.firmware` (regla 7): `G10` y `G11` sin E; con `retraction.wipe`, error de configuración;
- `travel.avoid_crossing_walls` y `travel.max_detour` (regla 8, en mm o en %): el planificador de viajes va recto
  cuando se le pide, o cuando el desvío pasa el máximo. En el motor viene prendido (`docs/spec/BEHAVIOR.md`);
- `walls.clockwise` (regla 9): los lazos de pared, también la espiral del modo vaso, en sentido horario desde la misma
  costura (la costura en bufanda decide antes de dar vuelta el lazo);
- los valores propios del material, `extrusion.retraction` y `extrusion.travel` (regla 10): reemplazan a los de la
  impresora en el escritor y en la verificación, aunque sean 0; `null` deja el de la impresora;
- el verificador del G-code acepta la retracción en partes durante el barrido, la vuelta con su extra y `G10`/`G11`, y
  la Z máxima con la rampa; el panel tiene las filas (Material › Retracción y Viajes, Velocidad › Viajes, Resistencia
  › Paredes) y `docs/CONFIG.md` las claves y el grupo `travel`.

**Medido** (caja negra de la ficha contra el motor, mismas piezas y ajustes; el cubo del motor tiene 20 mm de alto,
100 capas):
- R1: retracción `E-2` a F2400, vuelta `E2.5` a F1200 (igual que la ficha);
- R2 (cilindro, costura alineada, sin relleno): 0 retracciones sin el ajuste y 49 con él (la ficha, 3 y 51, con
  relleno); con relleno al 20 %, 99 en los dos casos: el viaje del relleno a la pared ya retraía;
- R3: 5,474 mm de barrido a F6240 (5,47 en la ficha), 10,94 con retracción a 20 mm/s, 8,42 a F9600 con viajes a 200,
  2,73 con 1 mm de retracción, y con `before_wipe` 50 %: `E-1` quieta y 1 mm en 2,73 mm, como la ficha;
- R4: el salto solo en capas por encima de 2 mm o por debajo de 1 mm;
- R5: la rampa sube a tan 10° hasta 1 mm y baja en vertical; el cambio de capa sube desde la capa de abajo;
- R6: 299 `G10` y 299 `G11` en el cubo, ninguna E de retracción; el salto sigue con `G1 Z`;
- R7 (tubo, capa 10): sin el ajuste, 6 viajes y 28,5 mm; con él, 27 tramos y 29,7 mm; con desvío máximo de 1 mm, igual
  que sin el ajuste; con 200 %, igual que sin límite (la ficha: 8 viajes y 33,5 mm; 90 tramos y 39,0 mm);
- R8: los cuatro lazos del tubo con área positiva, y negativa con `walls.clockwise`;
- R9 a R11: los valores del material (largo 0,8, velocidades 25 y 10, extra 0,4, largo 0, vacío; salto 0,3 y 0 sobre
  0 y 0,4 de la impresora: 698 y 100 movimientos en Z; viaje mínimo 20: 50 retracciones en lugar de 299; barrido del
  material: las 299 retracciones barren).
- 12 pruebas nuevas en `tests/test_retraction.cpp` y 2 del planificador en `tests/test_travel.cpp`; fuzzer con las
  opciones nuevas: 300 corridas (semilla 11), 0 fallas.

**Diseño propio** (la ficha no lo dice): la retracción del cambio de capa no fuerza el salto; un viaje que retrae por el
cambio de capa barre a la altura nueva sobre la capa de abajo; con la retracción del firmware, `restart_extra` no se
escribe y `retraction.length` 0 sigue sin retraer; `travel.max_lift` 0 usa `retraction.lift`.

**No probado:** en una impresora real (G10/G11 necesitan la retracción del firmware activada).

**Sigue:** el resto de `walls_extra.md` y la impresión secuencial (`sequential.md`, con las etiquetas de objetos).

## 2026-10-27 — Punto 3: paredes extra (W5, W7, W8)

**Hecho** (ficha `walls_extra.md`):
- `walls.staggered_inner_seams` (regla 3): cada pared interna empieza `walls.width` por nivel antes, sobre su propio
  lazo, en el sentido en que se imprime (después de `walls.clockwise`); la de afuera queda donde dice `walls.seam`;
- `walls.top_one_wall` (regla 10): donde la capa de arriba deja la región abierta (más ancha que un cordón) queda solo
  la pared de afuera; las internas se hacen alrededor de lo que sigue subiendo, como si fuera una región propia, y la
  superficie de arriba gana su lugar. `top` en cada capa; `topmost` en la última capa de la pieza;
- `walls.first_layer_one_wall` (regla 11): la primera capa con la pared de afuera sola;
- `walls.extra_if_needed` (regla 6) y `walls.avoid_curled_overhangs` (regla 9) se leen y se guardan, sin cambiar el
  G-code: en la ficha, Tisma Slicer no agregó perímetros, y lo segundo no se midió;
- la tarjeta «Más opciones de paredes» en Resistencia; `walls.clockwise` ahora también se guarda en el archivo de
  ajustes (faltaba en la escritura).

**Medido** (mismas piezas y ajustes que la ficha):
- W5 (cilindro de 10 mm, 4 paredes, costura alineada atrás): la de afuera en X 100,000 y las internas en 100,400,
  100,799 y 101,195 (Tisma: 0,42 mm por nivel con su ancho; el motor, 0,40 con `walls.width` 0,4);
- W7 (escalón): capa 19, paredes internas 187,2 mm sin el ajuste y 107,2 con `top` (Tisma: 186,51 y 106,42); la
  superficie de arriba pasa de 748 a 893 mm (Tisma: 986 a 1090, con otro espaciado); capa 39 sin paredes internas con
  `top` y `topmost`; `topmost` deja la capa 19 como sin el ajuste;
- W8 (cubo de 20 mm, 3 paredes): capa 0 solo la de afuera, 78,4 mm (Tisma 78,34); capa 1, las tres.
- 3 pruebas nuevas (`tests/test_walls_extra.cpp`).

**Sigue:** los perímetros extra en voladizos (W3), el espesor de la cáscara vertical (W4: antes, una pregunta de
comportamiento al equipo de fichas, porque los números de la ficha no alcanzan para saber dónde va el relleno sólido) y
el relleno concéntrico de ancho fijo; después la impresión secuencial.

## 2026-10-27 — Punto 3: perímetros extra en voladizos (W3)

**Hecho** (ficha `walls_extra.md`, regla 5): `walls.extra_on_overhangs` llena cada voladizo sostenido de un solo lado
(las mismas zonas que los arcos en voladizo, `held_zones` en `src/infill/arcs.*`), dentro de las paredes, con lazos
desde su contorno hacia adentro, a un ancho de distancia, como `Overhang perimeter` a la velocidad de los puentes y con
su cordón redondo, en lugar de su relleno; se imprimen después de las paredes de su isla. Con `arcs.enabled`, esas
zonas son de los arcos. El relleno concéntrico del motor ya era de ancho fijo (regla 1, último punto).

**Medido** (W3, columna de 10 × 20 × 8 mm con una losa de 20 × 20 × 2 mm que sobresale 10 mm, capa 40): sin el ajuste,
76,0 mm de `Overhang perimeter` en 2 tramos (Tisma: 75,8 en 2); con él, 489,6 mm en 13 lazos (Tisma: 513,2 en 4
tramos, sus lazos unidos). Prueba nueva en `tests/test_walls_extra.cpp`; 522 pruebas pasan; fuzzer con las opciones
de paredes nuevas: 300 corridas (semilla 13), 0 fallas.

**Pregunta para el equipo de fichas** (regla 7, W4, `walls.vertical_shell`): el motor ya pone sólido lo que no tapan
todas las `surfaces.top_layers` capas de arriba (o de abajo), que en el techo de 15° parece ser lo de `partial`. Para
saber qué cambia con `disabled` y con `enabled`, como caja negra en Tisma Slicer, con el techo de W4 en la capa 30: ¿qué
superficie es `Top solid infill` y cuál `Solid infill` en cada modo (por ejemplo, el ancho de cada franja medido desde
el borde alto del techo)? Y en el hongo (capa 60) y el cono (capa 20) con `enabled`: ¿dónde queda el relleno sólido
agregado (a qué distancia de la pared y de qué ancho)?

**Sigue:** la impresión secuencial (`sequential.md`, con las etiquetas de objetos de `gcode_firmware.md`, regla 4).

## 2026-10-27 — Punto 3: impresión secuencial

**Hecho** (ficha `sequential.md`):
- `sequential.enabled`, `sequential.clearance_radius`, `sequential.clearance_height`, `placement.spacing` (el
  `arrange.distance` de la ficha) y `gcode.between_objects`;
- el laminado recibe los objetos de la placa (sus triángulos en la malla: `MeshObject`); con la impresión secuencial,
  cada uno se lamina aparte (con su falda, borde y balsa) y sus capas siguen a las del anterior (`SlicedModel::objects`,
  `sequential`); sin ella, se guarda el contorno de cada uno para las etiquetas de objetos que siguen;
- el escritor imprime objeto por objeto (regla 1): cada uno con sus primeras capas (velocidad, aceleración, ventilador,
  falda, borde, balsa, costura en bufanda), sus cambios por altura desde abajo, y el paso al siguiente de la regla 3
  (retrae, sube a la capa más alta impresa si hace falta, viaja al centro de la caja del siguiente, temperaturas de la
  primera capa sin esperar o las de `gcode.between_objects`, ventilador apagado, y la primera capa viaja a esa altura
  hasta su primer punto antes de bajar). Una marca `;NEXT_OBJECT:<n>` le dice al verificador que la Z vuelve a empezar;
- se rechaza (regla 4) un objeto que no es el último más alto que `sequential.clearance_height`, dos objetos más cerca
  que `sequential.clearance_radius`, y varios objetos en modo vaso sin la impresión secuencial (regla 5);
- `slice --copies N` pone N copias del modelo en la cama (placement.md, regla 2), con `placement.spacing` entre sus
  cajas, o el radio del cabezal en la impresión secuencial (regla 2); `arrange` de la placa pasó al núcleo
  (`arrange_boxes`) y la interfaz acomoda con ese radio;
- la revisión de estabilidad mira cada objeto por su cuenta;
- en la interfaz: la opción en Otros › Modos especiales, los dos espacios del cabezal en Impresora › Cama y volumen,
  el G-code entre objetos con las plantillas, y el corte en etapas guarda los objetos.

**Medido** (SQ1 a SQ4 con el cubo de 20 × 20 × 10 y la base de la ficha): 2 copias, 50 capas con las dos en cada una
sin el ajuste y 100 capas (150 con 3) de a una con él; separación entre bordes 6 / 20 / 35 mm con los radios 5 / 20 /
35 (6 sin el ajuste), como la ficha; al pasar al segundo: retracción, viaje a (120, 100) a Z 10, `M140 S0`, `M104 S200`,
`;BETWEEN 1`, `M107`, y en su primera capa el viaje a Z 10 y después `G1 Z0.200`; con `M109 S[temperature]`, solo
`M109 S205`. Las torres de 25 mm se rechazan (la ficha: Tisma Slicer no avisa). 6 pruebas nuevas
(`tests/test_sequential.cpp`); fuzzer con copias juntas y de a una.

**Diseño propio:** el orden es el de los objetos en la placa (o de las copias); el centro de un objeto es el de la caja
de su contorno; el conjunto de copias se centra en la cama también en la impresión secuencial (Tisma Slicer deja la
primera en el centro); la altura de viaje es la de la capa más alta impresa hasta ahí, no la del último objeto.

**Corregido** (el fuzzer, con las copias y las opciones nuevas):
- con `walls.top_one_wall`, una esfera colgaba el laminado: la diferencia entre la capa y lo abierto arriba dejaba
  anillos sin área, el eje medio salía roto (209 350 nodos, uno a 30 m de una pieza de 16 mm) y la subida de cada nodo
  hasta el centro era cuadrática. Ahora esa región se abre un cuarto de cordón, las paredes descartan los anillos sin
  área, cada nodo sube una vez (lo que recorre queda guardado) y el reparto desde los nodos marcados es una búsqueda de
  Dijkstra desde todos a la vez (antes, una búsqueda por cada uno que podía volver a recorrer los mismos caminos una
  cantidad exponencial de veces). La región que colgaba se hace en 19 ms;
- el verificador tomaba la última pieza diminuta de un barrido largo por una retracción nueva; ahora una retracción
  nueva es solo la que viene después de otro movimiento;
- en la impresión secuencial, el verificador volvía a los valores de la base en la marca del objeto nuevo, pero el
  viaje hasta él es todavía del anterior: vuelve en la primera capa del nuevo.

Fuzzer: 400 corridas (semilla 17) y 1500 con las semillas 1, 271034 y 271035, 0 fallas.

**Sigue:** las etiquetas de objetos (`gcode_firmware.md`, regla 4), que ya tienen los objetos; después el punto 4.

## 2026-10-27 — Punto 3: etiquetas de objetos

**Hecho** (ficha `gcode_firmware.md`, regla 4): `gcode.object_labels` (`none`, `octoprint`, `firmware`). Con los
objetos de la placa (o las copias de `slice --copies`), el G-code define cada uno al empezar (Marlin: `M486 S<n>`,
`M486 A<nombre> (Instance <k>)`, `M486 S-1`; RepRapFirmware: `M486 S<n> A"…"`; Klipper: `EXCLUDE_OBJECT_DEFINE` con su
centro y su envolvente) y en cada capa marca los movimientos de cada uno (`M486 S<n>` … `M486 S-1`,
`EXCLUDE_OBJECT_START` … `EXCLUDE_OBJECT_END`, o los comentarios de OctoPrint); en RepRap genérico y los firmwares
viejos, nada. Para eso, en cada capa las islas de un objeto van todas antes de pasar al siguiente (la más cercana
primero, como siempre); la zona gruesa de capas locales queda fuera de los objetos. En la impresión secuencial, cada
capa va entera con la etiqueta de su objeto. El verificador acepta esas líneas. Fila en Impresora › G-code ›
Firmware.

**Medido** (FW1, FW3, FW6 y FW8 con dos copias del cubo de 20 mm): las definiciones y las marcas como en la ficha
(los nombres `cube20.stl (Instance 1)` y `cube20_stl_(Instance_1)`, `; printing object cube20.stl id:0 copy 0`); con
Klipper, la segunda copia centrada en 113 (Tisma Slicer: 74, porque no centra el conjunto). 5 pruebas nuevas
(`tests/test_labels.cpp`); fuzzer con etiquetas: 1000 corridas con las semillas 31 y 32, 0 fallas.

**Sigue:** el punto 3 queda completo salvo la cáscara vertical (W4, con la pregunta al equipo de fichas). Sigue el
punto 4 (multimaterial, F7, resina).

## 2026-10-27 — Punto 4a: el núcleo de varios extrusores

**Hecho** (ficha `multi_extruder.md`, reglas 1 a 8 y 16):
- ajustes nuevos: `machine.extruders` (1 a 16) y `machine.extruder_offset`; el grupo `extruders` (`walls`, `infill`,
  `solid_infill`, `support`, `support_interface`, `bed_temperature_from`, `ooze_prevention`, `standby_delta`);
  `retraction.toolchange_length` y `toolchange_restart_extra` (un valor o una lista por extrusor); `gcode.toolchange`;
  `surfaces.interface_shells`. Cada objeto de la placa (`MeshObject::extruder`) tiene su extrusor;
- el corte: con objetos de extrusores distintos, la placa se lamina entera (contorno de cada capa, soporte, falda,
  borde, balsa) y cada extrusor aparte sobre esas mismas capas. Cada parte tiene sus perímetros donde toca a otra; lo
  de todas las partes sostiene sus paredes, puentes y voladizos; las superficies ven lo de las otras como propio salvo
  con `surfaces.interface_shells`; lo que ya es de un extrusor anterior no es de este. Cada camino lleva su extrusor
  (`Toolpath::tool`). Con un solo extrusor en la máquina, nada cambia;
- el escritor: cada capa empieza con el extrusor con el que terminó la anterior y sigue con los demás en orden, un
  grupo por extrusor (el soporte sin extrusor propio, la falda y el borde van con el primero); el cambio retrae lo que
  falta hasta `toolchange_length`, quieto, cierra la etiqueta del objeto abierta, escribe la espera con menos
  temperatura, el fin del filamento, `gcode.toolchange` (`previous_extruder`, `next_extruder`, `toolchange_z`), `T<n>`
  si el texto no lo tiene, la espera del que entra y el inicio de su filamento; al volver, el filamento de esa
  herramienta vuelve lo que retrajo y su extra. Al empezar: cada extrusor usado calentado y esperado (o a su
  temperatura de espera), el G-code de cambio con `previous_extruder` −1 y `T` al primero; la cama de la primera capa
  y, desde la segunda, la del extrusor que corresponde (`M140`). El desfase de cada boquilla se resta de sus X e Y. Las
  variables `num_extruders`, `initial_extruder`, `initial_tool`, `is_extruder_used` y `current_extruder`;
- el verificador sigue cada herramienta: su retracción de cambio, lo que retrajo cada una y su extra al volver, y el
  área de movimiento corrida por los desfases;
- en la interfaz: Impresora › Boquilla › Extrusores (cantidad, desfases, retracción de cambio y su extra, G-code de
  cambio), Otros › Varios materiales (el extrusor de cada tipo, la cama, la espera) y las capas sólidas entre
  materiales en Superficies; `docs/CONFIG.md` con todas las claves.

**Medido** (con las piezas de la ficha: dos mitades de 10 × 20 × 10 mm y dos bloques apilados de 20 × 20 × 5 mm):
- ME1: `M104 S200 T0`, `M104 S220 T1`, el inicio, `M109 S200 T0`, `M109 S220 T1`, `T0`; capa 0 con T0 y T1, capa 1
  con T1 y T0…: 51 `T` en 50 capas, como la ficha. Cada mitad con su perímetro externo de 58,4 mm;
- ME2: `G1 E-4`, `;TC 0->1 at 0.2`, `T1`; al volver a T0, `G1 E5`. Con `T{next_extruder} ; mine` en el texto, ningún
  `T` más; al empezar, `;TC -1` y `T0 ; mine`. La ficha muestra también `M107` después de `T1`: el motor no lo escribe
  (el ventilador sigue el de la capa);
- ME3: perímetros con T0, relleno, sólido y superficie con T1; la capa 5 empieza con el relleno (T1); `M190 S55` y en
  la segunda capa `M140 S70`; todo con el extrusor 2: 55 y 50 desde el 1, 75 y 70 con 0;
- ME4: `M104 S190 T1` al empezar, sin esperarlo; en cada cambio `M104 S170 T0 ;cooldown`, `T1`, `M109 S220 T1`;
- ME5: la mitad del extrusor 2 de X 90,2 a 99,8 e Y 85,2 a 104,8;
- ME10: sin el ajuste, relleno ralo en las capas 21 a 27 de los dos materiales; con él, las capas 22 a 24 sólidas
  (la 24 superficie de arriba) y 25 a 27 sólidas, sin puente (apoyan en el bloque de abajo). Tisma Slicer imprime la
  capa 22 como puente interno sobre el relleno; el motor no hace puentes internos sobre el relleno ralo en ningún caso
  (pasa lo mismo con P13 de `infill_patterns.md`): queda anotado para hacerlo aparte.

Velocidad (dos cubos de 40 mm lado a lado, relleno al 20 %, 200 capas, con la CI de Linux corriendo en paralelo): con
un extrusor 5 ms de corte y 11 ms de G-code; con dos, 12 ms y 20 ms (la placa entera más cada parte; el G-code con los
cambios). 7 pruebas nuevas (`tests/test_multi_extruder.cpp`); el fuzzer con dos y tres extrusores (copias de extrusores
distintos, extrusor por tipo, desfases, espera, texto de cambio): 300 corridas (semilla 41), 0 fallas. Suite: 541
pruebas pasan.

**No hecho:** elegir el extrusor de cada objeto desde la línea de comandos o la interfaz (las pruebas lo dan en los
objetos); los colores pintados (regla 1, con `model_tools.md`); el tiempo de los cambios en la estimación; la vista
previa no colorea por extrusor.

**Sigue:** 4b, la torre de purga (reglas 9 a 14, ME6 a ME8).

## 2026-10-27 — Punto 4b: la torre de purga

**Hecho** (ficha `multi_extruder.md`, reglas 9 a 12):
- el orden de los extrusores de cada capa salió del escritor a `plan_tools` (`src/slice/wipe_tower.*`), para que la
  torre, el verificador y la revisión de imprimibilidad vean los mismos cambios;
- `plan_wipe_tower`: un rectángulo de `wipe_tower.width` con la esquina en (`x`, `y`), girado `rotation` alrededor de
  ella, tan hondo como la capa que más purga. En cada capa hasta la del último cambio: el lazo de contorno, un bloque
  de líneas en zigzag a lo ancho por cada cambio, impreso por la herramienta que entra (con
  `machine.single_extruder_mmu`, las que hacen el volumen de `wipe_tower.purge_volumes` por el
  `extrusion.purge_multiplier` del filamento que entra; con varias boquillas, 5 líneas para cebar), y el resto de
  adentro: denso en la primera capa, en columnas a lo sumo `bridging` aparte arriba, de las que cuelgan los bloques.
  En la primera capa, el borde (`brim_width`, en lazos de un cordón). `extra_spacing` separa las líneas y `extra_flow`
  las ensancha;
- el escritor: antes de cada cambio viaja a la torre y cambia ahí; el bloque y, en el primer cambio de la capa, el
  contorno y el relleno, con la herramienta nueva; en las capas sin cambio (y en la primera) la torre va primero, con
  la que está imprimiendo. Ningún camino de la torre queda dentro de la etiqueta de un objeto;
- errores: la torre con E absoluta (regla 9), fuera de la cama, o sobre la pieza, su falda o su borde;
- el verificador cuenta la torre en el área permitida, y el resumen del final dice los gramos de filamento de la
  torre (`; total filament used for wipe tower [g]`, antes siempre 0);
- en la interfaz: Otros › Torre de purga y Más de la torre de purga, el multiplicador en Material › Flujo y la MMU en
  Impresora › Extrusores; `docs/CONFIG.md`.

**Medido** (las piezas de la ficha; entre paréntesis, Tisma Slicer):
- varias boquillas: la torre de X 180,2 a 239,8 e Y 140,2 a 142,6 en la capa 10 (180 a 240, 140 a 143), 419,6 mm por
  capa (303,6), igual con volúmenes de 300;
- MMU: hasta Y 152,6 (154,4), 1919,6 mm por capa (2038,1); con 300 mm³ hasta 166,2 (169,4), 3959,6 mm (3840,4); ancho
  40: X hasta 219,8 (220); borde 5: la capa 0 desde X 175,0 (175,0), con 2 desde 178,2 (178,2); separación 150 %: 5 mm
  más honda o más; flujo 150 %: 1379,4 mm (1561,7); multiplicador 50 %: hasta 146,6 (147,9), 1019,6 mm (1257,7);
- ME7 (apilada, MMU): 2687,6 mm en la capa 0 (2388), 284,0 mm ralos en las capas 1 a 24 (387,4), 1919,6 en la 25
  (2089,6), nada encima; el `T1` después del viaje a la torre.
Las diferencias vienen del diseño propio de la torre (líneas del ancho de las paredes con el volumen como sección
rectangular, 5 líneas para cebar, columnas en vez de un zigzag ralo); los comportamientos de la ficha se cumplen.

**Corregido** (el fuzzer, con la torre y desfases): al cambiar sobre la torre a una boquilla con otro desfase, la
primera línea salía en diagonal desde donde había quedado la anterior. Ahora la boquilla nueva se mueve sobre el mismo
punto con sus coordenadas antes de extruir.

2 pruebas nuevas de la torre y una de desfase con torre (`tests/test_multi_extruder.cpp`, 9 en total); fuzzer con la
torre (MMU o boquillas, giro, ancho, borde, separación, flujo, multiplicador, volúmenes): 400 corridas (semilla 43), 0
fallas. Suite: 543 pruebas pasan.

**No hecho:** sin capas ralas (`no_sparse_layers`, regla 11: la boquilla tendría que bajar a la torre; se lee y se
guarda), la purga en el relleno o en el objeto (regla 13, ME8), el cono (regla 14, no medido), el extrusor propio de
la torre en las capas sin cambio (`extruders.wipe_tower`, se lee) y el tiempo de los cambios.

**Después, en el mismo bloque:**
- sin capas ralas (`wipe_tower.no_sparse_layers`, regla 11, ME7): la torre tiene solo la primera capa y las de un
  cambio, cada una sobre la anterior. En la capa del cambio la boquilla viaja a la torre a la altura de la capa, baja
  hasta la torre (en la pieza apilada, a Z 0,4 en la capa 25), imprime ahí con el alto de la capa y sube derecho
  antes de volver a la pieza (ni la rampa del salto ni la del cambio de capa salen de la torre). El verificador acepta
  la torre por debajo de la capa, nunca por encima. Medido: capas 1 a 24 sin torre, como la ficha;
- las pruebas que escriben archivos usan una carpeta temporal propia de cada proceso (`tests/test_temp.hpp`), que se
  borra al terminar: la etapa ASan de la CI de Linux falló una vez porque la suite del árbol de trabajo corría al mismo
  tiempo y las dos escribían `/tmp/tisma_test_profiles_store`. Sola, la prueba pasa (3 de 3).

Fuzzer con capas ralas o no: 400 corridas (semilla 44), 0 fallas. Suite: 543 pruebas pasan.

- la purga en los objetos (regla 13, ME8): cada objeto puede recibir la purga en su relleno ralo
  (`MeshObject::wipe_into_infill`) o en todo él (`wipe_into_object`). Con una MMU, después de cada cambio el
  filamento nuevo imprime primero, de esos objetos, lo que todavía no se imprimió en la capa (hasta cubrir la purga
  menos `extrusion.min_purge_on_tower`, que va a la torre antes), con la etiqueta de su objeto; la torre recibe solo
  lo que falta. Lo decide `plan_tools`, así la torre ya sale con el tamaño justo. Medido en la capa 10 de los dos
  colores con relleno al 20 %: torre de 1919,6 mm, 1860,4 con purga en el relleno (Tisma Slicer: de 2038,1 a 1978,6,
  59,5 mm menos; acá 59,2) y 1739,6 con purga en el objeto (Tisma Slicer: sin torre en esa capa; el motor la
  imprime con lo que falta, porque el objeto de una capa no llega a los 140 mm³). Con un mínimo de 10 000 mm³ en la
  torre, la torre no cambia. Todavía no se marcan los objetos en la interfaz ni en la línea de comandos.

Fuzzer con objetos que reciben la purga y mínimos: 300 corridas (semilla 45), 0 fallas.

**Sigue:** 4c, la secuencia de la MMU (regla 15, ME9).

## 2026-10-27 — Punto 4c: la MMU de un extrusor

**Hecho** (ficha `multi_extruder.md`, regla 15): con `machine.single_extruder_mmu` y la torre, cada cambio:
- comprime la punta con el filamento que sale: líneas de 0,6 mm (1,5 veces las paredes) sobre la torre, antes del
  bloque de purga, un tramo por cada cuarto de segundo de `extrusion.ramming` a la velocidad de su caudal;
- descarga hasta el medio del tubo de enfriamiento (`mmu.cooling_tube_position` más medio
  `cooling_tube_length`): los primeros 15 mm a `unloading_speed_start`, lo del medio a `unloading_speed`, el último
  cuarto más lento (lo que ya retrajo cuenta); espera `toolchange_delay` (`G4`);
- enfría la punta con `cooling_moves` idas y vueltas de `cooling_tube_length`, el filamento de `cooling_initial_speed`
  a `cooling_final_speed`, la boquilla a lo largo de la primera línea de purga mientras tanto;
- retrae lo del cambio, escribe el fin del filamento, `gcode.toolchange`, `T<n>` y el inicio del nuevo, y carga
  `parking_position` más `extra_loading_move` (un quinto lento, siete décimos rápido, el último décimo lento);
- purga en la torre. Todo eso va como G-code propio del cambio (`;TYPE:Custom`); el verificador toma la punta del
  filamento nuevo en la boquilla después del `T`. `load_time` y `unload_time` suman a la estimación.
Con una MMU no se usan los desfases de boquilla (hay una sola). Un viaje que no se mueve (un camino que sigue donde
terminó el anterior, como los tramos de la compresión) ya no retrae. En la interfaz: Impresora › MMU, Material ›
Carga y descarga en la MMU y Tiempos del cambio; `docs/CONFIG.md`.

**Medido** (ME9, dos colores con MMU y torre; entre paréntesis Tisma Slicer): la compresión a F 4900 a 5500 en la
primera capa con cordón de 0,6 (F 3554 a 5385); la descarga `E-13` (15 menos los 2 de la retracción del viaje) a
F6000, `E-55.5` a F5400, `E-15.67` a F2700 y `E-7.83` a F1620: 94 mm (`E-15`, `E-55.3`, `E-15.8`, `E-7.9`: 94); 4
idas y vueltas `E5`/`E-5` a lo largo de 58,8 mm, de F1552 a F2399 (sobre 49,3 mm, de F1302 a F2013); `E-10`, `T1`;
la carga `E18 F180`, `E63 F1680`, `E9 F180` (`E18 F180`, `E63`, `E9`: 90 mm). Con la compresión, la torre de ME6 llega
a Y 155,0 (154,4), con 300 mm³ a 168,6 (169,4), con el multiplicador al 50 % a 149,0 (147,9); 2130,7 mm por capa
(2038,1). La ficha muestra también `E2` antes de la retracción de cambio; el motor no lo escribe.

**Corregido** (el fuzzer): con una MMU y desfases de boquilla, la primera línea después del cambio salía en diagonal;
ahora una MMU no tiene desfases y, si los hubiera, la boquilla se mueve antes.

Prueba nueva ME9 (`tests/test_multi_extruder.cpp`, 11 en total); fuzzer con MMU: 300 corridas (semilla 46), 0
fallas.

**No hecho:** la corriente del motor durante el cambio (`mmu.high_current_on_swap`, se lee); los ajustes de carga y
descarga son uno para todos los filamentos (no por extrusor).

**Sigue:** 4d, las vigas de entrelazado (regla 17, ME11).

## 2026-10-27 — Punto 4d: vigas entre materiales

**Hecho** (ficha `multi_extruder.md`, regla 17): con `interlocking.beams`, antes de laminar las partes de cada extrusor
se calculan las regiones de todas; donde dos se tocan, lo que está a menos de `depth` anchos de viga del borde entre
ellas (y a más de `boundary_avoidance` del exterior de la pieza) se corta en franjas de `beam_width` a `orientation`
grados, giradas 90° cada `beam_layers` capas, que van por turnos a una parte y a la otra. Cada parte se lamina con su
región así cambiada (`PartFrame::regions`): sus perímetros y su relleno siguen las vigas. Grupo `painting` (regla 18):
se lee y se guarda; espera a que se laminen los colores pintados. En la interfaz: Otros › Entrelazado entre
materiales; `docs/CONFIG.md`.

**Medido** (ME11, dos colores con relleno al 20 %; entre paréntesis Tisma Slicer): el extrusor 1 llega hasta X 101,52
(102,48; sin vigas 99,8); el perímetro externo de cada mitad pasa de 58,4 a 123,3 mm (de 58,3 a 119); las capas 10 y
11 iguales y la 12 distinta, como la ficha.

Prueba nueva ME11 (12 en `tests/test_multi_extruder.cpp`); fuzzer con vigas: 300 corridas (semilla 47), 0 fallas.

**Sigue:** 4e, el soporte sincronizado y la altura máxima por extrusor (reglas 19 y 20, ME12).

**4e, en el mismo bloque:** el motor ya imprime siempre el soporte en las capas de la pieza, que es lo que la ficha
llama sincronizado (ME12: 50 capas, soporte en Z 0,2, 0,4, 0,6…). `support.synchronize_layers` se lee y se guarda (en
Soporte › Interfaz). Lo que no está: el soporte con capas propias más gruesas (ME12 sin sincronizar: 70 capas, soporte
a 0,296 mm, o 0,247 con `machine.max_layer_height` 0,25), que pide imprimir el soporte a otras Z que la pieza; queda
anotado en `PLAN.md`.

**Sigue:** 4f, `multimaterial.md` y `color_mixing.md`.

## 2026-10-27 — Punto 4f, primera parte: boquillas distintas

**Hecho** (ficha `multimaterial.md`, reglas 1 a 5, 9 a 11):
- `machine.nozzle_diameter` acepta una lista, una boquilla por extrusor (un número las cambia todas). Los anchos de
  las líneas son para la boquilla del primero y cada línea toma la de su extrusor en proporción: las paredes y lo que
  va con su ancho (sólido, puentes, soporte) la del extrusor de las paredes; el relleno ralo la del relleno
  (`FillParams::sparse_width`). Con boquillas distintas ninguna capa pasa el 75 % de la más fina (regla 2).
- `multi.fine_walls_coarse_infill`: con boquillas distintas, perímetros, sólido, soporte, interfaz y torre con la más
  fina; el relleno ralo con la más gruesa, combinado en tantas capas como entren en el 75 % de su boquilla
  (`kind_extruders`, que reemplaza a los `extruders.*` mientras está prendido; al apagarlo vuelven, regla 4).
- `multi.color_mixing` (sin poner: no con una MMU, sí con una boquilla por filamento); en la interfaz, prenderla con
  una MMU avisa antes del costo («Cancelar» / «Prender»).
- Con una MMU, después de laminar, el aviso `purge` (información) con los gramos que van a la torre y dentro de los
  objetos (regla 10).
- En la interfaz: la boquilla de cada extrusor debajo de la general, que dice «boquillas distintas» si no coinciden
  (Impresora › Boquilla), y la tarjeta Otros › Boquillas distintas y colores; `docs/CONFIG.md`.

**Corregido:** con varios extrusores el relleno combinado (`infill.combine_*`) se perdía: el laminado por extrusor no
juntaba las capas gruesas de cada parte, y el escritor las imprimía fuera de los grupos de cada extrusor (sin cambio
de herramienta). Ahora cada extrusor imprime las suyas en su grupo, todas las partes en las mismas tandas de capas, y
el plan de herramientas las cuenta.

**Medido** (MM1: cubo de 20 mm, boquillas 0,25 y 0,6, capas de 0,15, paredes de 0,3): perímetros y sólido con el
extrusor 1 (cordón de 0,3), relleno ralo solo con el 2, de 0,72 mm de ancho (0,3 × 0,6 / 0,25) y 0,45 mm de alto (tres
capas, el 75 % de 0,6), una capa de cada tres, con su cambio a `T1` en cada una. MM5: los gramos del aviso son los de
la torre del pie del G-code; purgando en el relleno, parte pasa a los objetos. Tisma Slicer no se midió en estos casos.

Pruebas nuevas (`tests/test_multi_extruder.cpp`, 17 en total): MM1, MM2, MM3 y la regla 2, MM5 y el relleno combinado
de dos extrusores; fuzzer con boquillas distintas: 600 corridas (semillas 48 y 49), 0 fallas. La interfaz (MM3, MM4,
MM6) se compiló y sus pruebas de tarjetas y ayuda pasan; no se probó a mano.

**No hecho:** un `support.width` propio de cada boquilla (regla 5: el soporte sale de la fina con el ancho de sus
paredes); el diámetro de cada extrusor no se escribe en la variable `nozzle_diameter` de las plantillas (sigue siendo
una lista de uno).

**Sigue:** 4f, segunda parte: `color_mixing.md` (las mezclas, FS1 a FS14).

## 2026-10-27 — Punto 4f, segunda parte: mezcla de colores

**Hecho** (ficha `color_mixing.md`):
- `src/slice/color_mix.*`: una mezcla (`ColorMix`) es un extrusor virtual numerado después de los reales, con hasta
  tres filamentos y su parte, o un degradé con la posición de cada uno. El ciclo de capas (regla 7) es el más corto
  de 2 a 64 en el que cada parte queda a menos de 3 puntos de la pedida, simplificado; el orden (regla 8) da cada capa
  al filamento más atrasado, el primero de la mezcla en un empate; el ciclo cuenta desde la primera capa donde está la
  mezcla (regla 9). Los degradés (regla 10) van por bandas, una cada 10 capas (2 a 11), con la mezcla del medio de la
  banda redondeada al 10 % y su propio ciclo; la banda de una capa sale de su Z de arriba, sumando las alturas de capa
  desde la cama.
- En el laminado (`slice_model(..., mixes)`): un objeto con el número de una mezcla se lamina como otro extrusor y
  `apply_mixes` le pone a cada capa el filamento que le toca, capa entera (regla 6); lo demás (plan de herramientas,
  torre, escritor) solo ve extrusores reales, y cada capa empieza con el que dejó la anterior (regla 11). Las mezclas
  que la impresora no puede imprimir se descartan (regla 5).
- `toolchange_template_error` dice antes de laminar si la plantilla de cambio falla (regla 13); la interfaz lo avisa
  al prender la mezcla.
- El proyecto guarda el extrusor de cada pieza, sus banderas de purga y las mezclas con los colores de los
  filamentos, en el JSON de la regla 15 (`docs/PROJECT.md`); al abrirlo en una impresora con menos filamentos se
  descartan las mezclas que no puede imprimir.
- En la interfaz: la herramienta «Extrusor y color del objeto» de la placa (el extrusor de cada pieza, real o
  virtual, y dónde purga) y el diálogo «Extrusores virtuales - Mezcla de colores» (la lista con su color y partes, el
  color de cada filamento, las mezclas de dos colores, el editor con el deslizador, el tercer extrusor, el color que
  se muestra y la secuencia de capas). El color previsto es propio: la relación de Yule y Nielsen (1951) con n = 2 en
  cada canal; el modelo de Prusa no se usó.

**Medido** (caja de 20 × 20 × 10 mm, 50 capas, MMU con torre; entre paréntesis Tisma Slicer): FS1 1, 2, 1, 2… con 49
cambios (49); FS2 a FS8 las secuencias de la ficha; FS9, el degradé, la secuencia exacta
`11112111111211121121212121212212221221222212222222`; FS10 49 cambios (49); FS11 4165 mm de filamento, 12,42 g en
total y 11,16 g en la torre, 57 min con la impresora de prueba (3753 mm, 11,19 g, 10,17 g, 55 min). Desde la interfaz,
el mismo proyecto lamina y verifica: 3,86 m.

**Nota para la ficha:** la regla 7 dice «a 3 puntos o menos», pero con eso 7 : 93 daría un ciclo de 10 (10 % está
justo a 3 puntos de 7 %); la tabla medida da 11, así que el motor usa «a menos de 3 puntos».

**Corregido** (el fuzzer, con filamento de 2,85 mm): la compresión de la MMU podía ir más rápido que `speed.max`;
ahora no pasa de ese límite (la torre de ME7 cambia de 2130,65 a 2123,98 mm por capa con la impresora de prueba).

Pruebas nuevas: `tests/test_color_mix.cpp` (9 casos: la tabla de la regla 7, FS1 a FS14 y el proyecto); fuzzer con
mezclas y degradés: 900 corridas (semillas 50 a 52), 0 fallas. Capturas del diálogo y de la herramienta bajo Xvfb.

**No hecho:** el 3MF (leer y escribir `Metadata/Prusa_Slicer_full_spectrum.json` y el extrusor de cada objeto: el
motor no escribe 3MF y lee el plato como una sola malla); la línea de comandos no asigna extrusores ni mezclas; el
botón de degradés no está en la interfaz (la ficha dice que Tisma Slicer tampoco lo tiene, regla 4).

**Sigue:** F7 no planar; después resina (`resin.md`, `resin_supports.md`).

## 2026-10-27 — F7, la base: capas curvas

**Hecho** (ficha `nonplanar.md`, reglas 1 a 8):
- Ajustes `curved.*` (Impresión › Otros › Capas curvas) y `head.*` (Impresora › Máquina › Cabezal), en
  `docs/CONFIG.md`.
- `src/slice/curved.*`: el campo `D = g · rampa` con la huevera, las crestas, las crestas torcidas y el cono; la rampa
  `S(t)` desde `flat_below` y, con `flat_top`, de vuelta a plano arriba; el ajuste a la cabeza (`λ = π · altura libre /
  tan`, la amplitud con esa pendiente y como mucho media altura libre, achicada por bisección hasta que `J` quede en
  [0,6; 1,4] y no haya choque); sin ajuste, una capa demasiado empinada, que choca o con `J` fuera de límites es un
  error claro.
- El laminado (`slice_model`): la malla se refina (cada arista más larga que λ/20, entre 0,25 y 1 mm, partida en
  mitades compartidas, así sigue cerrada; las de la zona plana no) y se mueve a `z − D`; se lamina plano; los objetos
  conservan sus triángulos.
- El G-code (`curve_gcode`, al final de `write_gcode`): cada movimiento sobre una capa curva se parte cada
  `segment_length`, cada punto vuelve a su Z real (`z − D = z'`, por iteración), el material de cada tramo es el plano
  por `J` en su medio, la velocidad baja donde `J` > 1 (no sube donde es menor, para no pasar los límites) y Z no pasa
  `machine.max_feed_z`; los tramos en línea y con casi el mismo `J` se vuelven a unir. Funciona con E y con el eje A.
- El verificador comprueba que cada extrusión esté sobre la curva de su capa y que su material sea el plano por `J`.
- Requisitos y límites: E relativa (NP4); no va con el modo vaso, el ZAA, el relleno ondulado, la costura en rampa,
  los arcos G2/G3, la impresión secuencial, las alturas por zona, la torre de purga ni las caras pintadas (error).

**Medido** (bloque de 40 × 40 × 12 mm en X 100, Y 100; entre paréntesis Tisma Slicer): NP1 `λ` 25,894 mm, amplitud
0,9545 mm (0,955; 0,952 calculado), 65 capas (65; plano 60), capas 1 a 3 planas, capa 41 (Z 8,2) con error mediano
0,00024 mm y máximo 0,0006 (0,0009 y 0,0022); NP2 crestas, error máximo menor a 0,0015 (0,0008); NP3 en la capa 11
(Z 2,2, dentro de la rampa) la relación material / (plano · `J`) tiene mediana 0,9999 y el 80 % entre 0,9985 y 1,0014
(1,000; 0,994 a 1,006), con `J` de 0,81 a 1,37 (0,82 a 1,36); NP4 con E absoluta, error. El G-code del bloque pesa
3,96 MB (103 mil líneas; plano 168 kB) y la interfaz lo lamina en 3,2 s.

**Corregido** (el fuzzer): la compresión de la MMU ahora no pasa la extrusión más rápida configurada
(`fastest_extrusion`, el mismo límite que el verificador).

Pruebas nuevas: `tests/test_curved.cpp` (6 casos: NP1 a NP4, el cono y las crestas torcidas, la malla refinada
cerrada); fuzzer con capas curvas: 1200 corridas (semillas 52 a 55), 0 fallas. Captura de la vista previa con la
huevera bajo Xvfb.

**No hecho:** suavizar las capas donde la cabeza chocaría (regla 6: hoy, sin ajuste, es un error, y con ajuste la
amplitud se achica para toda la pieza); el tope de aceleración de Z (regla 5, solo la velocidad); el caudal `uniform`
(hace lo mismo que `preserve`); el cono según los voladizos y los demás modos (`nonplanar_modes.md`); las caras
pintadas con capas curvas. La estimación de tiempo y el filamento del pie del G-code son los del laminado plano.

**Sigue:** F7, `surface` (las caras de arriba inclinadas como una sola capa) con `nonplanar_modes.md`; después resina.

## 2026-10-27 — F7: capas paralelas a las caras de arriba (`surface`)

**Hecho** (ficha `nonplanar_modes.md`, reglas 14 a 18): `curved.mode` `surface`. Cada cara que mira hacia arriba
pide su pendiente (como mucho la pendiente libre de la cabeza), con un peso por su área y por lo horizontal que es;
los pedidos se reparten por una grilla sobre la pieza (celdas de 1 mm, hasta 64 por lado) y `h(x, y)` sale por
mínimos cuadrados arista por arista entre los nodos, con un peso chico en todas partes para que quede suave donde nadie
pide (gradiente conjugado). La deformación es `h · rampa(z)`, con la rampa pareja desde `curved.flat_below` hasta donde
empiezan las caras inclinadas (regla 15), achicada por bisección hasta cumplir `J`, la pendiente y la holgura de la
cabeza (regla 16). Una pieza sin caras inclinadas se lamina plana (CO1), y las primeras capas quedan planas (SU5).
Corregido de paso: `J` es 1 en las capas planas aunque la rampa tenga un quiebre justo ahí.

**Medido** (bloque de 40 × 40 mm con techo de 20 a 24 mm a lo largo de X; entre paréntesis Tisma Slicer): error de la
cara de arriba, desvío estándar por celdas de 1 mm muestreando a lo largo de cada extrusión, 0,050 mm en plano (0,057)
y 0,0052 mm con `surface` (0,010); pendiente de las capas 0,0073 en Z 2 (0,010), 0,049 en 10 (0,048), 0,075 en 15
(0,074), 0,090 en 18 (0,090), 0,095 en 20 (0,097) y 0,098 arriba (0,099); 110 capas (110; plano 120). Bloque bajo de 5
a 9 mm (SU4): pendiente arriba 0,062 (0,087) y error 0,056 mm contra 0,050 en plano (0,056 contra 0,057): no gana, como
dice la regla 17, pero queda un poco peor que plano. CO1: el bloque de techo plano sale con 60 capas planas. G-code del
techo: 2,8 MB (plano 404 kB).

Pruebas nuevas en `tests/test_curved.cpp` (9 casos en total: CO1, SU1 a SU3 y SU5, SU4); fuzzer con `surface`: 600
corridas (semillas 56 y 57), 0 fallas.

**No hecho:** el Z anti-aliasing encima de `surface` (hoy no van juntos); que una pieza baja no quede peor que plana
(SU4); `overhangs`, el cono según la pieza y `automatic`.

**Sigue:** F7, `overhangs` y el cono según los voladizos (reglas 1 a 13, CO1 a VO7); después `automatic`.

## 2026-10-27 — F7: el cono según la pieza y los voladizos (`conical`, `overhangs`)

**Hecho** (ficha `nonplanar_modes.md`, reglas 1 a 13):
- Los voladizos que cuentan: caras que miran hacia abajo entre `curved.overhang_threshold` (nuevo, 45°) y 80° desde la
  vertical, arriba de `flat_below`; cada uno pide `s(u − 5°)`, o al menos `s(u − 2°)` si la cabeza no deja más, o
  nada.
- `conical` con `fit_head`: la pendiente por radio sale de esos pedidos (su parte radial; un hongo baja hacia afuera,
  el borde de un cuenco dado vuelta sube), cero en el centro, pareja a lo largo de max(R/4, 5 mm) antes del primer
  radio que pide y la última pedida más afuera; sube con la rampa `S` hasta el voladizo más bajo.
- `overhangs`: plana dentro de la sección donde empieza el voladizo (cortada por segmentos) y bajando afuera con la
  distancia a ella, el borde suavizado de 1 mm adentro a 0,5 mm afuera, hasta lo más lejano del voladizo; la pendiente
  como mucho el 90 % de lo que deja la cabeza (ángulo libre y holgura).
- Las dos van en la grilla de `surface`, y los límites (`J`, pendiente, holgura) se toman ahora en puntos de la pieza:
  columnas de una grilla donde un rayo en Z la encuentra adentro, y sus caras; no en las esquinas de su caja. Si ya
  achicada la deformación no saca ningún voladizo del soporte (con el ángulo del soporte del motor), la pieza va
  plana.
- Corregido: una pieza sin nada que curvar (CO1) ya no usa un campo vacío; se lamina plana.

**Medido** (hongos de la ficha; entre paréntesis Tisma Slicer): con el cabezal de 25°, `conical` 0,291 (48°), 0,410
(52°) y 0,4663 (55°) (0,291, 0,410, 0,4665), y 60° plano; `overhangs` 0,291, 0,41 y 0,42 (0,29, 0,41, 0,42), 60°
plano, y una capa bajo el sombrero de 55° con pendiente 0,42 (0,417); con el cabezal por defecto, `conical` 0,2 a 48°
(0,21) y `overhangs` 0,18 (0,18), y 52° plano en `overhangs` (plano). Soporte del hongo de 52° con el cabezal de 25°:
83,9 m de recorrido en plano (unos 2,8 m de filamento; 3,07 m) y nada con `conical` ni con `overhangs` (nada). Hongo de
48° con el cabezal por defecto: de 86,0 m a 8,2 m con `overhangs`.

**Distinto de Tisma Slicer:** con el cabezal por defecto, `conical` deja plano el hongo de 52° (CO2 lo curva): con
0,2 de pendiente el voladizo queda a 45,5° de la vertical, y el soporte automático del motor (medio cordón de 0,4 mm
por capa de 0,2, 45°) todavía lo toma; el de Tisma Slicer arranca a 48,4° (cordón de 0,45).

Pruebas nuevas en `tests/test_curved.cpp` (12 casos: CO1, CO2, CO4, VO2, VO4 y VO5); fuzzer con los cuatro modos:
600 corridas (semillas 58 y 59), 0 fallas.

**No hecho:** varias zonas de voladizos a distintas alturas (`overhangs` toma una sola, desde el voladizo más bajo);
el hongo con cúpula (AU3) y `automatic`; `stress`.

**Sigue:** `automatic` (la suma de `overhangs` y `surface`), y después resina.

**`automatic`, en el mismo bloque** (reglas 19 y 20): la deformación de `overhangs` más la de `surface`; si la suma se
pasa de `J`, la pendiente o la holgura, se achica solo la de las caras de arriba (la de voladizos ya cumple sola).
Medido: el hongo de 52° sale igual que con `overhangs` (AU1), el bloque con techo igual que con `surface` (AU2), y en el
hongo con cúpula (AU3, cabezal de 25°) el soporte pasa de 83,9 m de recorrido en plano a 83,4 con `surface` y a nada
con `overhangs` y `automatic` (Tisma Slicer: 3065 mm de filamento en plano, 1436 con `surface`, nada con los otros dos):
nuestro `surface` saca menos soporte bajo el sombrero. Prueba nueva AU1 a AU3 (13 casos en `tests/test_curved.cpp`).

**Sigue:** `stress` necesita las capas calculadas por el análisis estructural (regla 21 a 23): queda para cuando el
análisis las calcule. Siguiente punto: resina (`resin.md`, `resin_supports.md`).

## 2026-10-27 — Resina, R1: imágenes con el área exacta y el archivo GOO

**Hecho** (ficha `resin.md`, reglas 1 a 12 y 20 a 22):
- Ajustes: `machine.display_pixels_x`/`y`, `display_width`/`height` y `display_mirror_x`/`y` (los de la Elegoo Mars 5)
  y el grupo `resin` (altura de capa, exposiciones, capas de fondo, pie de elefante, escalas, subidas y esperas,
  potencias, suavizado, densidad, gamma, corrección de Z, corrección absoluta y los nombres de la cabecera). En los
  perfiles: la altura de capa en el de impresión, el nombre de la impresora en el de impresora y lo demás en el de
  material; en el panel, sus tarjetas (Material › Resina, Impresora › Máquina › Pantalla de resina y Otros › Resina:
  capas).
- `src/resin/resin.*`: la pieza escalada y apoyada, cortada en capas iguales a media altura; cada sección crecida por
  la corrección absoluta, achicada en las capas de fondo contra el pie de elefante (la `i` de `N`,
  `pie · (1 − i / N)`) y, con corrección de Z, recortada a lo que tienen debajo las secciones de las capas anteriores.
  La imagen sale fila por fila con el área exacta de cada píxel (cada borde suma el área con signo hasta el lado
  derecho del píxel; la suma a lo largo de la fila es la cobertura), el gris redondeado hacia arriba y la gamma; se
  codifica a medida que sale, sin guardar la imagen entera. La vista previa es la pieza vista desde arriba, sombreada
  por la pendiente de las alturas.
- `src/resin/goo.*`: el archivo GOO, escrito y leído según la especificación de Elegoo: cabecera de 195 477 bytes,
  capas y final; el modo avanzado se activa solo si alguna capa no sigue a la cabecera.
- Línea de comandos: `slice … -o pieza.goo` lamina para resina, con la pieza centrada en la pantalla.

**Medido:**
- RE1 y RE2 iguales a Tisma Slicer byte a byte: 200 capas, imagen de 4586 bytes con suma de control `0x03`, grises 0,
  131, 183 y 255, columnas 1763 a 2334 y filas 994 a 1565. Igual RE3 (2955 a 3526 con espejo, 571 a 1142 sin él),
  RE4, RE5 (8, 7, 6, 6, 5, 4, 3, 2, 1, 0 píxeles), RE6 (600 columnas, 220 capas, 4620 mm³), RE7, RE8 (bordes 73 y 37,
  21 y 5 con gamma 2; 326 612 píxeles con gamma 0) y RE9 (164 164 píxeles en las tres primeras capas de la losa).
- RE10 es distinto a propósito: la ficha dice que la línea de comandos de Tisma Slicer no aplica la corrección
  absoluta; el motor la aplica (regla 22).
- Benchy local (solo para medir) en resina: 960 capas en 1,56 s con 4 núcleos, archivo de 12,2 MB.

Pruebas nuevas en `tests/test_resin.cpp` (13 casos: el RLE, RE1 a RE10, los errores y los ajustes, un archivo dañado y la línea de
comandos).

**No hecho:** las ventosas (reglas 13 a 15), las calibraciones de resina (16 a 19), los soportes de resina
(`resin_supports.md`) y la resina en la interfaz (laminar, ver las imágenes y exportar).

**Sigue:** R2, las ventosas (VE1 a VE9) y la torre de exposición con el bloque de medidas (CR1).


## 2026-10-27 — Resina, R2 y R3a: ventosas, calibraciones, soportes, base y vaciado

**Hecho:**
- Ventosas (`resin.md`, reglas 13 a 15): `find_cups` recorre las capas desde la plataforma; un hueco de la sección
  queda ventilado si al menos 3 mm² de él están sobre aire ventilado en la capa de abajo (el aire alrededor de la
  pieza o un hueco ventilado); en la primera capa lo cierra la plataforma o la base, salvo que la pieza esté elevada.
  Los huecos cerrados que se superponen de una capa a la otra son una ventosa, y desde 4 mm² van al JSON de avisos
  (`cupping`) con las alturas, el área y cómo arreglarlo. Las superposiciones se calculan para todas las capas a la
  vez; la pasada en orden solo suma áreas.
- Calibraciones de resina (reglas 16 a 19): la torre de exposición (`ResinTower`, `tower_exposure`,
  `resin_tower_mesh`, con la forma de `calibration_models.md`: base, una franja por exposición con su ranura al pie,
  agujeros redondos que la atraviesan y aletas atrás; CM3 y CM5) y el bloque de medidas con su corrección de escala.
  Todavía sin menú en la interfaz.
- Soportes y base (`resin_supports.md`, reglas 1 a 3): puntos en los nodos de una grilla de 4 mm (a densidad 100 %)
  sobre las caras que miran hacia abajo a menos de 45° de la horizontal, más uno en cada isla que empieza en el aire;
  bajo cada punto, una punta que entra en la pieza, un pilar recto y un ensanche al pie, o un aterrizaje sobre la
  pieza cuando ella lo tapa; la base cubre los pies con su margen, cerrada entre ellos, o rodea la pieza apoyada.
  Sin soportes ni base alrededor, la base es un error (RS3).
- Vaciado (reglas 4 a 6): transformada de distancia exacta (Felzenszwalb y Huttenlocher) sobre una grilla de 0,1 a
  1 mm; el hueco es la pieza achicada por la pared y el redondeo y crecida por el redondeo, que se acota para que
  siempre quede un hueco donde cabe (RS4). Barras interiores en cinco direcciones y agujeros de drenaje (en el motor;
  la interfaz todavía no los pone). Un hueco cerrado avisa como ventosa.
- Panel: tarjetas Resina: soportes, base y vaciado.

**Medido:** VE1 a VE9 y CR1 como en la ficha (CR1 con modo avanzado 1, a propósito). RS1: 340 capas, el cubo desde
Z 7,05, 25 pilares (unos 20 mm² por capa a media altura, como Tisma Slicer). RS2 (pared de 2 mm): pared de 2 mm a
media altura; RS4: el cubo con pared de 3 mm queda hueco (Tisma Slicer lo deja macizo, no se copia). Benchy local
(solo para medir), 4 núcleos: 0,86 a 1,03 s sin soportes (960 capas, con la búsqueda de ventosas: el texto grabado
en la base, la bodega y la chimenea) y 1,30 s con soportes y base (1100 capas, archivo de 14,9 MB).

Pruebas nuevas en `tests/test_resin.cpp` (21 casos en total).

**No hecho:** las uniones entre pilares a `resin.support_critical_angle` (los pilares bajan rectos); poner los
drenajes y lanzar las calibraciones de resina desde la interfaz; laminar resina, ver sus imágenes y exportar `.goo`
desde la interfaz.

**Sigue:** R3b, la resina en la interfaz.

## 2026-10-27 — Resina, R3b: la resina en la interfaz

**Hecho:**
- `machine.technology` (`fdm` o `msla`), en Impresora › Máquina › Cama y volumen. Ninguna ficha nombra la clave; es
  decisión propia (`PROVENANCE.md`). Con `msla`:
  - el panel muestra solo los ajustes de resina, y la placa es la pantalla;
  - Laminar llama a `slice_resin` y la vista previa 3D muestra los contornos de cada capa (la pieza y, aparte, los
    soportes con la base), con el tiempo, los ml, los gramos y el costo;
  - «Exportar GOO» escribe el archivo entero o nada;
  - Calibrar muestra solo la torre de exposición y el bloque de medidas, sin soportes ni base: la franja elegida pasa
    a `resin.exposure`, y las medidas del bloque dan las correcciones de escala;
  - en la línea de comandos, `msla` pide una salida `.goo`.
- La torre de exposición tiene la forma de `calibration_models.md` (CM3, CM5), con agujeros redondos, y sus
  exposiciones viajan como cambios por altura (`changes`, clave nueva `exposure`).
- Costo de la resina por el frasco (`resin.bottle_cost`, `bottle_volume`, `bottle_weight`; `gcode_extras.md`, regla
  2): va en la cabecera del archivo y en la vista previa; en el panel, el peso del frasco fija la densidad.

**Medido:** comprobado en la interfaz con capturas (cubo sobre soportes y base, torre de exposición) y exportando un
`.goo` desde la interfaz, leído de vuelta: capa 150, 327 184 píxeles, el cubo exacto.

**No hecho:** las uniones entre pilares a `resin.support_critical_angle`; poner los drenajes desde la interfaz; ver
la imagen de una capa como imagen (la vista previa muestra contornos).

**Sigue:** subir R2, R3a y R3b cuando pase la verificación de Linux; después, el punto 5 del orden (lo propio de
Tisma) y la interfaz P1.

## 2026-10-27 — Punto 5: relleno ondulado entrelazado e impresoras de cinta

**Hecho:**
- Relleno ondulado entrelazado (ficha `interlock_infill.md`): `infill.waves`, `infill.wave_amplitude` (50 % de la
  altura de capa) e `infill.wave_length` (10 mm), en Impresión › Relleno › Relleno ondulado. El relleno poco denso va
  sobre `a · sin(2πX/λ) · sin(2πY/λ)` con X e Y de la cama, la misma fase en todas las capas. Usa lo del relleno
  ondulado (N1): la onda se apaga cerca de las paredes y del relleno sólido, y el material sigue el espesor real hasta
  la capa de abajo; vale con cualquier patrón. No va con `infill.nonplanar`, `gcode.arcs`, el modo espiral, las capas
  curvas ni la cinta (un error lo dice).
- Impresoras de cinta (ficha `belt_printer.md`): `belt.enabled`, `belt.angle` y `belt.belt_is_z`, en Impresora ›
  Máquina › Impresora de cinta. La pieza se gira sobre X para que los planos paralelos al pórtico queden planos
  (`s = y · sin α + z · cos α`), se lamina como siempre y el G-code pasa al final a los ejes de la máquina: la cinta
  a `s / sin α`, el pórtico a `z / sin α`; `;Z:` sigue llevando `s`. El verificador del G-code hace la cuenta inversa
  y revisa las capas como en una cama; la cama solo limita X. En la cinta, soportes, balsa, borde, torre de purga,
  impresión secuencial, modo espiral, arcos, capas curvas y ondas son un error; la falda no se imprime. La vista previa
  de la interfaz lee el G-code y devuelve la pieza a donde está sobre la cinta, con las capas inclinadas.

**Medido:**
- IL1 (cubo de 20 × 20 × 10 mm en X 100, Y 100): capa 11, 633 puntos del relleno, error mediano 0,000 mm, el 90 %
  por debajo de 0,016 mm, de −0,096 a +0,100; capa 12, el 90 % por debajo de 0,020 mm, de −0,098 a +0,100 (Tisma
  Slicer: 0,001, 0,036, de −0,094 a +0,1).
- B1 (α 45°, cinta en Z): 106 capas; capa 11, Z 3,111 e Y de 0,325 a 4,075; capa 53, Z 14,991; Y máximo 14,129;
  los mismos números que Tisma Slicer. X de 90,217 a 109,9 (Tisma: 90,1 a 109,9). B2 (cinta en Y): lo mismo con Y y
  Z cambiados. B3 (α 30°): 93 capas, capa 11 en Z 4,4, Y máximo 19,988, como Tisma.
- La vista previa de la cinta, comprobada con una captura de la interfaz.

**No hecho:** el visor dibuja el alto de cada cordón en vertical; con las capas inclinadas de la cinta, la cara
lateral de la pieza se ve con huecos entre capas.

**Sigue:** subir R3b y el punto 5 cuando pase la verificación de Linux; después, la interfaz P1.

## 2026-10-27 — Interfaz P1: el panel de ajustes, segundo paso

**Hecho** (ficha `settings_panel.md`):
- **Buscar un ajuste** (regla 5): un campo con lupa en la barra superior (Ctrl+F). Busca en el nombre del ajuste, su
  tarjeta, su sección, su clave y lo que hace (de `docs/CONFIG.md`), sin mayúsculas ni tildes, y ordena por dónde
  encontró cada palabra. Enter o un clic abren la pestaña, la sección y la tarjeta, traen la fila a la vista y la
  resaltan 2 s; un ajuste que el nivel esconde se ve con la etiqueta de su nivel.
- **Ver todo junto** (regla 4): todas las secciones una debajo de otra, con su título.
- **Restablecer por tarjeta** (regla 8): el menú de tres puntos de cada tarjeta vuelve sus ajustes al perfil guardado
  (`with_keys`, en `src/config/profiles.*`).
- **Personalizado** (reglas 15 a 20): parte del nivel de antes; «Elegir ajustes…» con el árbol, las casillas a
  medias, el filtro, «Partir de», Todos y Ninguno; el ojo tachado en cada fila; «+ Agregar ajuste» al pie de cada
  tarjeta; Ctrl+Alt+4; exportar e importar la vista (`tisma-ui-view`), ignorando las claves que no existen; un ajuste
  nuevo de otra versión se ve si su nivel entra en el de partida. Se recuerda en `interface.json`.
- Los ajustes de resina, de la cinta y de las ondas tienen ahora su botón (i) y su nombre en la búsqueda.

**Medido:** pruebas `settings view: …` (SP6, SP10, SP11, SP13, SP14, regla 8) y, con la interfaz entera,
`ui_settings_basic`, `ui_settings_custom_search` y `ui_settings_same_gcode` (SP9: el mismo G-code en Básico y en
Personalizado). Capturas revisadas: «costura» abre Calidad › Costura con la fila resaltada, todo junto, y el diálogo.

**No hecho:** la tarjeta de la impresora con su dibujo, la cama y la boquilla rápida (21 a 26, 32 a 36), la cámara
(27 a 31), la configuración de la impresora aparte con Calibración (37 a 40) y la conexión desde la tarjeta (41 a 44).

**Sigue:** la tarjeta de la impresora y su configuración aparte.

## 2026-10-27 — Interfaz P1: la tarjeta de la impresora y su página

**Hecho** (ficha `settings_panel.md`, reglas 21 a 23 y 32 a 40):
- **Tarjeta de la impresora** arriba del panel: el dibujo según el tipo (resina, cinta, delta por la cama redonda,
  cerrada o abierta), la lista de impresoras con buscador (las propias, las del programa y las 144 del catálogo por
  marca, cada una con su dibujo), las camas con su dibujo cuando el modelo trae varias, el engranaje y la boquilla.
- **Boquilla rápida:** un clic cambia al perfil de la impresora con esa boquilla (lo crea desde el catálogo si falta) y
  un aviso con **Deshacer** devuelve la impresora y los ajustes de antes; una impresora propia tiene **Otra…**, que
  cambia el diámetro y deja el perfil marcado con su asterisco.
- **Configuración de la impresora aparte:** página a todo el ancho con «‹ Placa» y Escape. El panel queda con
  Impresión y Material. Secciones Máquina, Boquilla y extrusor, Límites y G-code (las mismas tarjetas y niveles),
  Calibración (cada prueba con lo que fija, su valor, su estado y el filamento; «N de M calibradas» y «Calibrar en
  orden»), Conexión y Notas. Lo calibrado se anota al usar un resultado en Calibrar.
- `--profile printer=…` acepta un modelo del catálogo con su boquilla («Raise3D E3 0.4 mm»), solo para esa vez.

**Medido:** pruebas `printer card: …` (SP25, SP26, SP27, regla 21 y regla 38 con SP29 y SP30), y con la interfaz
entera `ui_printer_card` (SP22, la Raise3D E3 con sus camas) y `ui_printer_page` (SP28). Capturas revisadas.

**No hecho:** la cámara (27 a 31) y la conexión por red desde la tarjeta (41 a 44), que esperan
`printer_connections.md` (F9); la boquilla que informa la impresora conectada (26); el diálogo de cambios sin guardar
(24) y los perfiles de impresión y filamento compatibles al cambiar de boquilla (22), que esperan los perfiles de
`presets.md` y `profile_pack.md`; la cama elegida no cambia todavía la temperatura (`profiles.md`, regla 4). Pellets
no tiene dibujo propio hasta que exista `machine.feedstock`.

**Sigue:** el resto de P1 (`preview.md`, `credits.md`, `assistant.md`), después P2.

## 2026-10-27 — Interfaz P1: la vista previa (`preview.md`)

**Hecho:**
- **Las 13 vistas** (regla 1): tipo de línea, alto de capa, ancho, velocidad, velocidad alcanzada, ventilador,
  temperatura, caudal, caudal alcanzado, tiempo de capa lineal y logarítmico, extrusor y color por capa. El lector del
  G-code guarda en cada movimiento el ventilador, la temperatura, el extrusor y los cambios de color, y `time_preview`
  le da su tiempo con las velocidades y aceleraciones de la impresora (el mismo cálculo del tiempo estimado, capa por
  capa en paralelo). Las instancias llevan el número de la vista, así los dos renderizadores colorean igual.
- **Leyenda:** por tipo, cada uno con su tiempo, su parte del tiempo y su filamento en metros y gramos; en las vistas
  con números, 11 pasos iguales (en logaritmo para la logarítmica). Debajo, la primera capa además del total.
- **Fila de íconos** (regla 11): viajes, barridos, retracciones, vueltas, costuras, cambios de herramienta y de color,
  pausas, G-code propio, centro de gravedad y boquilla, dibujados sobre la vista.
- **Deslizadores** (reglas 4, 9 y 12): el de capas con dos manijas, altura y número, el «+» con cambio de color, pausa
  con mensaje o G-code (cambios por altura que empiezan esa capa), sus marcas, y el de movimientos dentro de la capa de
  arriba (los renderizadores cortan esa capa).
- **Teclas** (regla 10): 0 a 6 vistas fijas (con la de abajo), K ortogonal (un teleobjetivo, sin tocar los shaders; se
  recuerda), Z encuadrar, E nombres de las piezas, L leyenda.
- **Visor** (regla 13): abre `.bgcode`, «Sin tipos» sin `;TYPE:`, y Más › Vista previa de G-code….

**Medido:** pruebas del lector (ventilador, temperatura, extrusor, color, marcas, tiempos de 2,05 s para 100 mm a
50 mm/s con 1000 mm/s²), PV2 y PV3 (pasos de 23 mm/s y de 0,038 mm), PV7 (tantas retracciones marcadas como las del
G-code del motor), PV8 y PV9 (el cambio de color empieza la capa 25 y la pausa la 30, con su mensaje), PV10, el corte
de la capa de arriba y la cámara ortogonal. Capturas de las vistas, los íconos, los deslizadores y la vista ortogonal
(también con ray tracing).

**No hecho:** el ícono Pieza (el modelo transparente sobre las trayectorias necesita transparencia en los dos
renderizadores); el visor en una ventana aparte; el extrusor del cambio de color con varios extrusores; el panel
táctil y «Girar libre»; PV1 contra Tisma Slicer (falta la pieza en voladizo de sus pruebas).

**Sigue:** `credits.md` y `assistant.md` (P1); después P2.

## 2026-10-27 — Interfaz P1: créditos y fuentes (`credits.md`)

**Hecho:**
- **Un solo archivo de datos** (regla 5): `resources/credits.json` (formato `tisma-credits`) con las 11 fuentes de la
  ficha y las del motor que ya estaban en `PROVENANCE.md` (Kuipers et al. 2020, ZigZagZ, Song et al. 2017, el «arc
  overhang», Tricard et al. 2020, Felzenszwalb y Huttenlocher), cada una con las fichas del panel donde se ve; y las
  bibliotecas que se distribuyen, con su licencia y sus avisos en `resources/licenses/` (los archivos de licencia de
  cada una, sin cambios). El programa los lleva adentro.
- **Bajo las fichas** (reglas 1 a 3): «Basado en FFF anti-aliasing (2017), de Song et al.» en gris y chico, el nombre
  como enlace que abre la fuente; el mismo texto al pasar el puntero por el título. Paredes, Superficies inclinadas,
  Relleno ondulado, Arcos en voladizos, Soportes de resina, Vaciado y las dos fichas de capas curvas.
- **Más › Créditos y fuentes** (regla 4): la tabla de funciones (función, fuente con enlace, autor, licencia o tipo) y
  la de componentes de terceros (versión, licencia y «Ver el aviso» con el texto de la licencia). Acerca de lo nombra.
- **Idioma** (regla 7): «Basado en», «de», los títulos y las licencias se traducen; las fuentes y autores no.

**Medido:** las pruebas `[credits]` (CR3: cada ficha con «Basado en» y cada fila de la tabla de `credits.md` tienen su
crédito, y la prueba nombra la ficha que falta; cada biblioteca distribuida de `THIRD_PARTY.md` está con la misma
licencia, URL y versión y sus avisos existen; CR4 en inglés: «Based on Support Fins, by gittrahan») y `ui_credits`
(la página en inglés). Capturas de la página y del pie de Superficies inclinadas.

**No hecho:** CR1 en su panel (las aletas de soporte todavía no existen; su crédito ya está en el archivo). Las otras
fichas con **Fuentes:** (`aerodynamics.md`, `structural_analysis.md`, `orientation.md`, `infill_patterns.md`…) se
revisan una por una cuando se hagan (regla 2). Algunos DOI no se pudieron confirmar: esos enlaces van a la página que
ya cita `docs/spec/SOURCES.md` (HAL, arXiv, el PDF de los autores).

**Sigue:** `assistant.md` (P1); después P2.
