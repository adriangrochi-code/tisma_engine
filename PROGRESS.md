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
