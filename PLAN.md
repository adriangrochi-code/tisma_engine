# Plan del motor de laminado de Tisma

**Objetivo final:** rehacer en sala limpia todo lo que se pueda de Tisma Slicer, para que el dueño lo licencie como
quiera (hoy código disponible sin uso comercial de terceros; tiene que poder cerrarse en cualquier momento):
- todo se escribe desde cero (ver `CLEANROOM.md`), también lo que el equipo del dueño ya hizo en Tisma Slicer;
- lo que Tisma Slicer ya hace llega como fichas en `docs/spec/features/`.

**Primer paso:** el motor de laminado, más rápido que el de Tisma Slicer actual.

**Visión (decisión del dueño, 2026-10-24):** laminado **no planar** y **de 5 ejes**, con **aceleración por GPU**. La
primera versión plana es la base: la malla, el corte, los perímetros, los rellenos y el G-code se reutilizan. Por eso
las reglas de «Diseño para la visión» valen desde F1.

**Primera versión:** FDM, una boquilla, capas planas, **con aceleración por GPU desde F1**. Se estimó en unas 16
semanas dedicando un cuarto del tiempo, pero esa estimación es anterior a la GPU. Se vuelve a estimar al terminar F1,
con lo medido.

**Fuera de la primera versión:**
- multimaterial y torre de purga;
- soportes en árbol;
- capas no planares y 5 ejes (son la visión: fases F7 y F9);
- resina;
- interfaz propia.

## Cómo se usa

- Es un programa aparte: `tisma-engine slice modelo.stl --config ajustes.json -o salida.gcode`.
- Tisma Slicer lo llama como programa externo, igual que Tisma Cloud llama al slicer.
- El motor no enlaza con Tisma Slicer ni comparte código con él.
- Los ajustes van en un JSON propio, documentado en `docs/CONFIG.md`.

## Rendimiento

- Desde el diseño:
  - capas en paralelo (oneTBB);
  - GPU en los núcleos donde rinde, desde F1 (ver «GPU en la primera versión»);
  - Clipper2 para las operaciones con polígonos;
  - datos contiguos;
  - sin copias innecesarias.
- Cada fase se mide con el banco de pruebas: Benchy, Benchy ×2 con giroide, conejo ×1,5 y tornillo ×4.
- Referencia: Tisma Slicer actual lamina el Benchy en 2,9 s con 4 núcleos.

## Diseño para la visión

Reglas que se cumplen desde F1, aunque la primera versión sea plana, para no tener que reescribir el motor después:

1. **Las capas son conjuntos de nivel de un campo escalar.** F1 corta con el campo f(p) = z (capas planas). Aun así,
   la extracción de contornos recibe un valor por vértice, así que un campo curvo puede reemplazar a z sin reescribir el
   corte. Para 5 ejes el campo vive dentro del volumen y hará falta una malla de tetraedros; eso queda para F9.
2. **Las trayectorias son 3D y tienen orientación.** Cada punto guarda x, y, z y la dirección de la boquilla; en una capa
   plana esa dirección es (0, 0, 1). La cinemática de cada máquina es un módulo aparte que traduce a ejes y G-code:
   3 ejes, mesa giratoria A/C, cabezal B/C, etc. El resto del motor no sabe de ejes.
3. **Datos listos para la GPU:**
   - arreglos contiguos (estructura de arreglos);
   - índices en vez de punteros;
   - nada de funciones virtuales en los bucles internos;
   - núcleos escritos como funciones puras sobre `std::span`.

   Cada núcleo tiene una versión CPU (oneTBB), que es la referencia de las pruebas. La versión GPU se prueba contra ella.
4. **La GPU es opcional.** Siempre existe la ruta CPU: Tisma Cloud puede correr en servidores sin GPU, y no todos los
   usuarios tienen una buena.
5. **Operaciones 2D en capas curvas.** Las capas curvas de 3 ejes son funciones de altura sobre XY, como en CurviSlicer.
   Ahí los offsets y rellenos (Clipper2) se calculan en XY y después se proyectan sobre la capa. Para 5 ejes hay que
   parametrizar la superficie, y eso se investiga en F9.

**Fuentes** (solo los artículos; el código que los acompaña no se lee, ver `CLEANROOM.md`):

- **3 ejes, capas apenas curvas:**
  - Etienne et al., *CurviSlicer: Slightly curved slicing for 3-axis printers*, ACM TOG 38(4):81, 2019.
  - Ahlers, Wasserfall, Hendrich, Zhang, *3D Printing of Nonplanar Layers for Smooth Surface Generation*, IEEE CASE 2019.
- **Varios ejes, capas como isosuperficies de un campo:**
  - Dai et al., *Support-free volume printing by multi-axis motion*, ACM TOG 37(4):134, 2018.
  - Zhang et al., *S³-Slicer: A General Slicing Framework for Multi-Axis 3D Printing*, SIGGRAPH Asia 2022.
  - Liu et al., *Neural Slicer for Multi-Axis 3D Printing*, ACM TOG 43(4):85, 2024 (arXiv 2404.15061).

**Dónde ayuda la GPU y dónde no:**

- **Ayuda en el trabajo regular y masivo:**
  - corte de la malla en muchas capas a la vez;
  - operaciones rasterizadas;
  - el cálculo y la optimización de los campos para capas curvas (sistemas dispersos grandes o redes neuronales).
- **Ayuda poco en el trabajo con mucha ramificación o secuencial:**
  - operaciones booleanas de polígonos;
  - orden de las trayectorias;
  - G-code.

Cada núcleo que pasa a la GPU se mide con el banco de pruebas contra su versión CPU. Si no rinde, queda en CPU.

## GPU en la primera versión

La optimización importa sobre todo en la primera versión, así que la GPU entra desde F1 y no en una fase aparte
(decisión del dueño, 2026-10-24). Cada fase hace la versión CPU de sus núcleos y, en los que la GPU puede rendir,
también la versión GPU, probada contra la CPU.

**API: Vulkan compute.** Se usa salvo que el dueño diga otra cosa. Motivos:
- funciona con NVIDIA, AMD e Intel, y el controlador ya la trae, así que el usuario no instala nada;
- se puede probar en la CI sin GPU, con lavapipe, la implementación de Vulkan en CPU de Mesa. Las máquinas de GitHub no
  tienen GPU, y CUDA no se puede probar ahí;
- en macOS funciona sobre Metal con MoltenVK, para F10.

Las bibliotecas que haga falta (cabeceras y cargador de Vulkan, compilador de shaders) se anotan en `THIRD_PARTY.md`
antes de usarlas. Revisadas el 2026-10-24:
- Vulkan-Headers (Apache-2.0) y volk (MIT) se enlazan;
- glslang solo se usa como herramienta para compilar los shaders a SPIR-V (ver `THIRD_PARTY.md`);
- en la CI de Linux, lavapipe (Mesa) corre las pruebas de GPU sin GPU;
- donde no hay Vulkan, el motor usa la CPU.

**Núcleos candidatos por fase:**

| Fase | En GPU | Queda en CPU |
|---|---|---|
| F1 | infraestructura GPU (dispositivo, búferes, pruebas contra CPU, lavapipe en la CI); intersección de triángulos con todas las capas a la vez | unión de segmentos en contornos, offsets con Clipper2 |
| F2 | medir: quizá un campo de distancias como apoyo | Voronoi de segmentos y trapezoidación (mucha ramificación) |
| F3 | detección de superficies arriba y abajo por rasterizado, evaluación del giroide | recorte de los rellenos con Clipper2 |
| F4 | — | orden, viajes, G-code, estimación de tiempo |
| F5 | voladizos por ángulo (normal de cada triángulo), proyección rasterizada de las zonas a soportar | contornos finales del soporte |
| F6 | comparación con y sin GPU contra Tisma Slicer | — |

## Decisiones del dueño (2026-10-24)

- La GPU entra en la primera versión, desde F1, con Vulkan compute salvo que el dueño diga otra cosa.
- Después de la primera versión: F7 (no planar en 3 ejes) y F8 (lo propio de Tisma, desde sus fichas); después las
  fases de producto P1–P5, en el orden que decida el dueño; al final F9 (5 ejes) y F10 (macOS).
- La máquina de 5 ejes de referencia se elige al empezar F9, la última fase antes de macOS. Hasta entonces la
  cinemática queda como módulo aparte, sin atarse a ninguna máquina.

## Fases

| Fase | Semanas | Contenido | Terminada cuando |
|---|---|---|---|
| F0 | 1 | Esqueleto: CMake, CI (Linux y Windows en cada envío), Catch2, línea de comandos, lectura de STL (binario y ASCII), `THIRD_PARTY.md`, `PROVENANCE.md`, banco de pruebas | Compila y prueba en Linux y Windows; lee los modelos del banco |
| F1 | 2–3 | Corte de la malla en capas: intersección triángulo-plano, unión de segmentos en contornos, cierre de huecos chicos, alturas de capa (primera capa y resto), offsets con Clipper2. Infraestructura GPU (Vulkan) y corte en GPU | Las áreas por capa coinciden con las de la malla (prueba con piezas de área conocida); GPU y CPU dan los mismos segmentos; tiempos de CPU y GPU medidos |
| F2 | 4–7 | Perímetros de ancho variable desde Kuipers et al. 2020: esqueleto de los contornos (Voronoi de segmentos con Boost.Polygon), trapezoidación, cantidad y ancho de cordones (repartido y centrado), extracción de trayectorias | Paredes de 0,3 a 1,2 mm sin huecos ni excesos (área cubierta medida); piezas comunes con 2–3 perímetros |
| F3 | 8–9 | Superficies: arriba, abajo, sólido y relleno; rellenos rectilíneo, grilla y giroide; puentes con dirección. En GPU: detección de superficies y giroide | Un cubo y el Benchy con capas sólidas correctas; tiempos de CPU y GPU medidos |
| F4 | 10–12 | G-code: orden de islas, viajes que no cruzan perímetros, retracción, salto en Z, costura (alineada, más cercana, aleatoria), enfriamiento con tiempo mínimo de capa, velocidades, aceleraciones, límites de la máquina, Marlin y Klipper, estimación de tiempo | El Benchy se imprime en una impresora real |
| F5 | 13–15 | Soportes en grilla (voladizos por ángulo, proyección, interfaz y distancia), falda y borde. En GPU: voladizos y proyección | Una pieza con voladizos se imprime y el soporte sale |
| F6 | 16 | Integración: Tisma Slicer y Tisma Cloud llaman al motor; comparación de calidad y velocidad con Tisma Slicer, con y sin GPU | Medido y documentado |
| F7 | después de F6 | No planar en 3 ejes: capas apenas curvas cerca de la superficie superior (CurviSlicer, Ahlers et al.), con control de choques del cabezal | Una pieza con techo curvo se imprime sin escalones visibles en una impresora común; medido |
| F8 | después de F7 | Lo propio de Tisma, desde sus fichas: análisis estructural y aerodinámico, Z anti-aliasing, relleno entrelazado, impresoras de cinta, resina | Las pruebas de cada ficha pasan |

**Después del motor** (el orden lo decide el dueño):

| Fase | Contenido | Base |
|---|---|---|
| P1 | Interfaz propia: ventana, vista 3D, vista previa del G-code, ajustes, plancha | Fichas de la interfaz de Tisma (qué hay en cada pantalla y cómo se comporta) |
| P2 | Conexión con impresoras: USB, Bambu Lab, OctoPrint, Moonraker, PrusaLink y las demás | Documentación pública de sus API y fichas del comportamiento de Tisma |
| P3 | Perfiles de impresoras y filamentos, desde los datos oficiales de cada fabricante | Nuevo: los perfiles actuales vienen de PrusaSlicer, OrcaSlicer y Cura |
| P4 | Formatos: 3MF (especificación pública), STEP (OCCT es LGPL-2.1 con excepción: no entra en la lista de licencias permitidas; hace falta otra biblioteca o una decisión del dueño), formatos de resina desde las especificaciones de los fabricantes | Nuevo |
| P5 | Tisma Cloud, app móvil, CI, instaladores y marca sobre el slicer propio | Fichas; el logo lo da el dueño |

**Al final** (decisión del dueño, 2026-10-24: los 5 ejes son lo último antes de macOS, y macOS va al final de todo):

| Fase | Cuándo | Contenido | Terminada cuando |
|---|---|---|---|
| F9 | antes de macOS | 5 ejes: campo escalar en el volumen (malla de tetraedros), capas curvas como isosuperficies (Dai et al. 2018, S³-Slicer, Neural Slicer), orientación de la boquilla, cinemática de la máquina, choques y G-code con ejes giratorios | Una pieza con voladizos se imprime sin soportes en una máquina de 5 ejes (hace falta decidir cuál) |
| F10 | al final | macOS: CI en cada envío y arreglos para que compile y pase las pruebas (decisión del dueño, 2026-10-24: se deja para el final) | Compila y prueba en macOS |

**Fichas:** el equipo que conoce Tisma Slicer las escribe en el orden de las fases: primero corte, perímetros,
superficies, G-code (enfriamiento, coasting, arc overhangs, revisión antes de imprimir) y soportes.

## Estado de F0

- [x] CMake, C++20
- [x] Catch2
- [x] Línea de comandos (`slice` solo lee el modelo y responde «not implemented»; `info` imprime el resumen)
- [x] Lectura de STL binario y ASCII, con pruebas
- [x] CI en cada envío: Linux (gcc, clang, ASan+UBSan) y Windows (MSVC). macOS pasa a F10, al final
- [x] `THIRD_PARTY.md`, `PROVENANCE.md`
- [x] Comprobar que la CI pasa en Linux y Windows (run 4: también pasó macOS)
- [ ] Banco de pruebas: modelos reales (Benchy, conejo, tornillo), con licencia que permita guardarlos

## Estado de F1

- [x] Malla indexada: soldadura exacta de vértices (`weld_exact`)
- [x] Contornos de nivel de un campo por vértice (`level_set_segments`), con la altura z como campo plano
- [x] Unir los segmentos en contornos cerrados (por la clave de arista, sin comparar coordenadas)
- [x] Cierre de huecos chicos en mallas abiertas
- [x] Alturas de capa (primera capa y resto) y corte por capas sin recorrer todos los triángulos en cada capa
- [x] Paralelo por capas (oneTBB)
- [x] Offsets con Clipper2 (regiones por capa con la regla no nula, offsets en inglete)
- [x] Infraestructura GPU: Vulkan con volk (opcional en tiempo de ejecución), shaders compilados a SPIR-V, lavapipe en la CI de Linux
- [x] Primer núcleo en GPU: rango de capas por triángulo, igual a la CPU
- [x] Corte en GPU: tramos por (triángulo, capa) con suma de prefijos, reagrupados por capa; misma topología que la CPU
- [ ] Medir el corte en GPU contra la CPU en una GPU real (acá solo hay lavapipe)
- [x] Prueba de áreas por capa con piezas de área conocida (pirámide, cubo; esfera: el volumen cortado coincide con el de la malla al 0,0002 %); tiempos medidos
- [x] Ajustes propios (`docs/CONFIG.md`) y `slice` con el corte completo: modos de relleno, compensación XY y de primera capa
- [ ] Contrastar el corte con los casos de la ficha `docs/spec/features/slicing.md` cuando esté en `main` (simplificación, alturas variables)

## Estado de F2

- [x] Boost.Polygon y Boost.Config (solo cabeceras) anotadas en `THIRD_PARTY.md`
- [x] Eje medio de una región: aristas del Voronoi de segmentos que quedan adentro, con el radio del círculo inscrito en cada extremo
- [x] Discretizar las aristas curvas (parábolas) en tramos de hasta 0,2 mm
- [x] Clasificar las aristas interiores por el sitio de cada celda, sin recorrer los contornos
- [x] Medir y acelerar el eje medio: la construcción de Boost domina; con la simplificación de 0,01 mm, de 39–40 ms a 6 ms en la capa del medio de la esfera
- [x] Esquemas de reparto de cordones (Kuipers et al. 2020, sección 4): uniforme, distribuido y distribuido hacia adentro
- [x] Grafo del eje medio y marcado del centro por significancia (sección 3.3, αmax = 135°) y máximos locales
- [x] Cantidad de cordones en los nodos centrales y anclas de transición (sección 3.4, ecuaciones 4–6)
- [x] Filtro de anclas opuestas a menos de 1 mm y rampas de largo w* con cantidad fraccionaria (ecuación 7)
- [x] Filtro del marcado (sección 3.3, marcar tramos no centrales de menos de w* entre nodos centrales)
- [x] Interpolación entre repartos para las cantidades fraccionarias de las rampas (sección 3.5)
- [x] Trapezoidación: apoyos de cada extremo de arista en los dos lados (punto más cercano del borde, ecuación 1)
- [x] ~~Dominios por contorno~~: no hace falta, los segmentos se encadenan por la identidad de sus sitios
- [x] Reparto por nodo (sección 3.5) y sitios sobre los lados de apoyo de los trapecios (sección 3.6)
- [x] Sitios sobre las aristas no centrales del eje (donde el radio cruza lᵢ)
- [x] Unir los sitios del mismo índice en segmentos por trapecio y encadenarlos en polilíneas de ancho variable
  (por identidad del sitio, sin tolerancias): tira, L, polígonos regulares y la capa del medio de la esfera dan lazos cerrados
- [x] Acelerar la extracción: casilleros contiguos en vez de mapas; de 50 a 3,3–3,9 ms en la capa del medio de la esfera
- [x] Meta-esquema de cáscara (sección 5.1, ecuación 8): M paredes al ancho preferido donde la región es ancha y ancho
  adaptativo solo en las partes angostas, con nodos extra donde el reparto cambia de forma; el resto queda para el relleno
- [x] Región que queda para el relleno: lo que no cubren las paredes, como polígonos (para F3)
- [x] Perímetros de todas las capas en paralelo (oneTBB) dentro de `slice_model`, con el grupo de ajustes `walls`
- [ ] Acelerar el eje medio, que ahora domina el corte (con paredes, `slice` de la esfera pasa de 0,2 s a 1,0 s).
  Medido: el Voronoi de segmentos de Boost cuesta 13–28 µs por segmento con cualquier polígono, y el 54 % es su
  aritmética exacta (`extended_int`), porque cada vértice es extremo de dos segmentos. Es lo que informa el artículo
  (5·10⁻⁶·n·log n s). Opciones, cada una de varios bloques:
  - un Voronoi de segmentos propio para polígonos, a partir de artículos, pensado también para la GPU;
  - híbrido: desplazamientos de Clipper2 donde la cáscara es un desplazamiento simple, y el esqueleto solo donde la
    pieza es angosta (falta un criterio exacto y barato para separar las dos zonas).
  Descartado: tapar el fondo con un contorno desplazado 2·R hacia adentro (no cambia el eje con R menor, pero Boost
  tarda 2,4 veces más con el doble de segmentos).
- [x] Resolución de conflictos de reparto (figura 9)
- [x] Aristas entre dos esquinas en tramos de 0,2 mm (su radio no es lineal; antes los sitios se desviaban hasta ~0,2 mm)
- [x] Punto del cordón central impar en un máximo aislado: un recorrido de un solo punto con su ancho
- [x] Criterio de F2: medir el área cubierta en paredes de 0,3 a 1,2 mm (huecos y excesos), uniendo los cordones como
  franjas de ancho variable con Clipper2, y en piezas comunes con 2–3 perímetros. Sin superposición ni nada afuera; los
  únicos huecos son las esquinas redondeadas de los extremos (0,13–0,43 % en tiras de 30 mm). Cuña de 0,3 a 1,2 mm:
  huecos 0,3 %, superposición 1,3 %, en las transiciones
- [ ] Contrastar con la ficha `docs/spec/features/perimeters.md` cuando esté en `main`

## Estado de F3

Desde `docs/spec/BEHAVIOR.md` («Superficies y rellenos») y `docs/spec/SETTINGS_V1.md` («Relleno»), con nombres propios.

- [x] Superficies superiores e inferiores por capa: N capas contadas desde cada superficie expuesta (grupo `surfaces`)
- [x] Ancho mínimo de las superficies sólidas: las franjas más angostas que un cordón crecen un cordón hacia el relleno
- [x] Relleno rectilíneo: líneas paralelas que giran 90° cada capa, ángulo base y densidad (grupo `infill`)
- [x] Relleno sólido de las superficies, rectilíneo al 100 % (lo monotónico va con su propio ítem)
- [x] Relleno en grilla (dos direcciones por capa, cada una al doble de separación)
- [x] Giroide: la superficie de `docs/spec/SOURCES.md` cortada en cada Z (cuadrados marchantes, período por densidad)
- [x] Acelerar el giroide: de 0,85 a 0,45 s en la esfera (24 pasos por período, curvas simplificadas a 5 µm)
- [x] Anclar el giroide a las paredes: sus curvas se unen a lo largo del borde, hasta medio período (21,3 % al 20 %)
- [x] Anclar las líneas de relleno a los perímetros: zigzag que une los extremos a lo largo del borde, sin entrar en
  las paredes; en las superficies sólidas, líneas ajustadas a la región. En el cuadrado de 7,6 mm, el material depositado
  es el área dentro del 1 % a 0°, 30° y 45°
- [x] Cerrar los huecos de las superficies sólidas junto a las paredes: las superiores monotónicas entran un 25 % de
  cordón en la pared interna (`infill.wall_overlap`); deposita el área entre −0,5 % y +1,5 % (antes hasta −7 %)
- [x] Puentes: superficie inferior sin nada impreso debajo (medido contra lo impreso, no contra el borde de la capa
  anterior), tendidos en la dirección que apoya los dos extremos de las líneas
- [x] Superficies superiores monotónicas: siempre en el mismo sentido, sin volver sobre lo hecho (`surfaces.top_pattern`)
- [x] ~~GPU: detección de superficies y giroide~~: no paga (decisión con números, 2026-10-24). La grilla del giroide
  cuesta ~80 ms en un hilo para las 500 capas de la esfera (~20 ms en 4 núcleos), y pasarla a Vulkan movería ~0,8 MB por
  capa en cada sentido; la clasificación de superficies son operaciones booleanas de Clipper2. CLAUDE.md pide GPU solo
  donde paga. Lo que más pesa del corte sigue siendo el Voronoi de las paredes (decisión pendiente del dueño)
- [ ] Criterio de F3: un cubo y el Benchy con capas sólidas correctas; tiempos de CPU y GPU medidos (el cubo ya está
  probado; el Benchy espera los modelos de banco del dueño)

## Estado de F4

Desde `docs/spec/GCODE_OUTPUT.md` (la interfaz con la vista previa de Tisma) y `docs/spec/BEHAVIOR.md` («Viajes y
retracción», «Enfriamiento», «Errores y avisos»), con nombres propios en `docs/CONFIG.md`.

- [x] Grupo de ajustes de la máquina (boquilla, diámetro del filamento, extrusión relativa o absoluta, G-code de inicio
  y fin con su mapa de variables, dialecto) y de velocidades (`machine`, `speed`)
- [x] Escritor de G-code mínimo: cabecera `; generated by Tisma engine <versión>`, G-code de inicio y fin con
  variables `[nombre]`, `{nombre}` y `{nombre[i]}`, capas con `;LAYER_CHANGE`, `;Z:`, `;HEIGHT:`, `;TYPE:` y `;WIDTH:`,
  E por volumen (ancho × alto × largo sobre la sección del filamento), XYZ con 3 decimales, E con 5, F entero, solo
  los ejes que cambian, y al final un comentario por ajuste usado; `tisma-engine slice` escribe el archivo
- [x] Ancho variable: las paredes de Kuipers cambian de ancho por punto; la E de cada tramo usa el ancho medio
- [x] Orden dentro de la capa: paredes de adentro hacia afuera con la externa al final; después superficies, puentes,
  huecos y relleno disperso
- [ ] Escribir el G-code más rápido (hoy ~1,4 s para los 30 MB de la esfera): `std::to_chars` y capas formateadas en
  paralelo
- [ ] Orden de islas y de recorridos (el más cercano), y costura de los lazos (`aligned`, `nearest`, `random`, `rear`)
- [ ] Viajes por dentro de la pieza sin cruzar perímetros; retracción al salir de la pieza o en viajes largos; salto
  en Z
- [ ] Enfriamiento: tiempo mínimo de capa con las dos etapas de `BEHAVIOR.md` y ventilador al máximo en esas capas
- [ ] Aceleraciones y límites de la máquina; estimación de tiempo (opcional, como comentario)
- [ ] Avisos en un JSON aparte (detalles que no se imprimen, capas que no llegan al tiempo mínimo)
- [ ] Criterio de F4: el Benchy se imprime en una impresora real (necesita al dueño)

## Avance

Cada bloque de trabajo anota en `PROGRESS.md`:
- la fecha;
- qué hizo;
- qué midió;
- qué sigue.
