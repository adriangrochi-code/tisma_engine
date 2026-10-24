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
