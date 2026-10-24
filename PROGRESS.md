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
