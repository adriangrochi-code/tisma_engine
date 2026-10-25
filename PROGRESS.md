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
