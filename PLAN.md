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
antes de usarlas.

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
- [ ] Offsets con Clipper2
- [ ] Infraestructura GPU (Vulkan, lavapipe en la CI) y corte en GPU
- [ ] Prueba de áreas por capa con piezas de área conocida; tiempos medidos

## Avance

Cada bloque de trabajo anota en `PROGRESS.md`:
- la fecha;
- qué hizo;
- qué midió;
- qué sigue.
