# Plan del motor de laminado de Tisma

**Objetivo final:** rehacer en sala limpia todo lo que se pueda de Tisma Slicer, para que el dueño lo licencie como
quiera (hoy código disponible sin uso comercial de terceros; tiene que poder cerrarse en cualquier momento):
- todo se escribe desde cero (ver `CLEANROOM.md`), también lo que el equipo del dueño ya hizo en Tisma Slicer;
- lo que Tisma Slicer ya hace llega como fichas en `docs/spec/features/`.

**Primer paso:** el motor de laminado, más rápido que el de Tisma Slicer actual.

**El motor pasa a ser el nuevo slicer (decisión del dueño, 2026-10-24, confirmada en esta sesión):** «tisma engine it
gonna become the new slicer», «tisma slicer its gonna be discard», «i need to not lose features». Ya no es un motor que
Tisma Slicer llama: lo reemplaza, con su propia interfaz (`tisma`). Ninguna función de Tisma Slicer se pierde: cada una
es una fila del inventario `docs/spec/COVERAGE.md` (que manda el equipo de fichas), con su ficha y su estado. Por eso:
- F6 pasa a ser el reemplazo de Tisma Slicer;
- las fases de producto P1–P5 son obligatorias antes de dejar Tisma Slicer;
- la interfaz propia ya no queda fuera de la primera versión.

**Visión (decisión del dueño, 2026-10-24):** laminado **no planar** y **de 5 ejes**, con **aceleración por GPU**. La
primera versión plana es la base: la malla, el corte, los perímetros, los rellenos y el G-code se reutilizan. Por eso
las reglas de «Diseño para la visión» valen desde F1.

**Rumbo (decisión del dueño, 2026-10-25):**
- un laminador que use las técnicas que más mejoran la **calidad** y la **resistencia**;
- compatible con una **impresora propia de 4 y 5 ejes** que el dueño quiere hacer a futuro.

Por eso:
- F9 cubre 4 y 5 ejes;
- la cinemática describe la cadena de ejes de cualquier máquina, sin atarse a una (regla 2 de abajo);
- todo análisis nuevo trabaja con las trayectorias 3D y su orientación, por ejemplo la simulación del enfriamiento
  (`docs/COOLING_SIMULATION.md`).

**Enfoque (decisión del dueño, 2026-10-24):** un laminador **profesional e industrial**, sin uso comercial de
terceros (la licencia de hoy), que sigue pudiendo cerrarse:
- **calidad y control antes que velocidad de laminado:** puede ser más pesado si imprime mejor;
- **motor propio en todo lo posible:** algoritmos escritos acá, para controlarlos y optimizarlos (por eso el Voronoi
  propio);
- **lo que Tisma Slicer ya hace**, integrado desde sus fichas;
- **análisis para imprimir mejor:** análisis estructural (elementos finitos) y de fluidos (CFD) que alimentan el
  laminado, más la optimización topológica para diseño generativo iterativo: fases A1–A3, antes de F5–F7.

**Primera versión:** FDM, una boquilla, capas planas, **con aceleración por GPU desde F1**. Se estimó en unas 16
semanas dedicando un cuarto del tiempo, pero esa estimación es anterior a la GPU. Se vuelve a estimar al terminar F1,
con lo medido.

**Fuera de la primera versión:**
- multimaterial y torre de purga (salió el 2026-10-24: ahora es la fase MM, `docs/MULTIMATERIAL.md`);
- soportes en árbol;
- capas no planares y 5 ejes (son la visión: fases F7 y F9);
- resina.

(La interfaz propia estaba en esta lista; salió el 2026-10-24, cuando el dueño decidió que el motor reemplaza a Tisma
Slicer.)

## Cómo se usa

- La interfaz `tisma` (`docs/UI.md`) es el slicer para el usuario; `tisma-engine slice modelo.stl --config
  ajustes.json -o salida.gcode` hace lo mismo sin ventana, y es lo que llaman Tisma Cloud y las pruebas.
- Mientras Tisma Slicer siga en uso, puede llamar a `tisma-engine` como programa externo.
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
   3 ejes, boquilla inclinada que gira (4 ejes, como la RotBot de Wüthrich et al. para el laminado cónico), mesa
   giratoria A/C, cabezal B/C, etc. El resto del motor no sabe de ejes. El G-code puede salir de dos maneras:
   - en coordenadas de la punta con su orientación, para firmwares que hacen la cinemática;
   - en ejes de la máquina, con la cinemática inversa propia.
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
- en las pruebas de Linux (`tools/ci_linux.sh`, dentro de la sesión), lavapipe (Mesa) corre las pruebas de GPU sin
  GPU;
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
  cinemática queda como módulo aparte, sin atarse a ninguna máquina. Desde el 2026-10-25 F9 cubre también 4 ejes.
  La impresora propia del dueño es candidata a máquina de referencia; decide el dueño.
- Eje medio: un Voronoi de segmentos propio, a partir de artículos, en vez del híbrido con Clipper2.
- Enfriamiento: una de las primeras capas (sin ventilador) que tiene que ir más lento por el tiempo mínimo lleva el
  ventilador al máximo, como cualquier otra capa frenada.
- El motor reemplaza a Tisma Slicer (ver arriba). Queda abierta la **migración** de los usuarios: sus proyectos 3MF y
  sus perfiles guardados. El equipo de fichas escribió dos opciones en `docs/spec/COVERAGE.md` («Migración»); decide
  el dueño.
- Ubicación de las piezas: la interfaz las centra en la cama (X e Y); el motor siempre las imprime apoyadas en ella
  (en Z las alturas cuentan desde la base de la pieza) y no las mueve en X ni Y.

## Fases

| Fase | Semanas | Contenido | Terminada cuando |
|---|---|---|---|
| F0 | 1 | Esqueleto: CMake, CI (Linux y Windows en cada envío), Catch2, línea de comandos, lectura de STL (binario y ASCII), `THIRD_PARTY.md`, `PROVENANCE.md`, banco de pruebas | Compila y prueba en Linux y Windows; lee los modelos del banco |
| F1 | 2–3 | Corte de la malla en capas: intersección triángulo-plano, unión de segmentos en contornos, cierre de huecos chicos, alturas de capa (primera capa y resto), offsets con Clipper2. Infraestructura GPU (Vulkan) y corte en GPU | Las áreas por capa coinciden con las de la malla (prueba con piezas de área conocida); GPU y CPU dan los mismos segmentos; tiempos de CPU y GPU medidos |
| F2 | 4–7 | Perímetros de ancho variable desde Kuipers et al. 2020: esqueleto de los contornos (Voronoi de segmentos con Boost.Polygon), trapezoidación, cantidad y ancho de cordones (repartido y centrado), extracción de trayectorias | Paredes de 0,3 a 1,2 mm sin huecos ni excesos (área cubierta medida); piezas comunes con 2–3 perímetros |
| F3 | 8–9 | Superficies: arriba, abajo, sólido y relleno; rellenos rectilíneo, grilla y giroide; puentes con dirección. En GPU: detección de superficies y giroide | Un cubo y el Benchy con capas sólidas correctas; tiempos de CPU y GPU medidos |
| F4 | 10–12 | G-code: orden de islas, viajes que no cruzan perímetros, retracción, salto en Z, costura (alineada, más cercana, aleatoria), enfriamiento con tiempo mínimo de capa, velocidades, aceleraciones, límites de la máquina, Marlin y Klipper, estimación de tiempo | El Benchy se imprime en una impresora real |
| A1 | — | Análisis estructural por elementos finitos: la pieza en vóxeles (hexaedros), elasticidad lineal, cargas y apoyos que da el usuario, solucionador iterativo sin matriz (gradiente conjugado con multigrilla), CPU de referencia y GPU; desplazamientos y tensiones | Casos de solución conocida (viga en voladizo, placa con agujero) dentro del error esperado |
| A2 | — | Optimización topológica y diseño generativo iterativo: densidades (SIMP), filtro, volumen objetivo, regiones fijas, restricción de voladizo para imprimir sin soportes; isosuperficie, malla nueva y laminado; laminado guiado por tensiones (relleno y paredes donde hacen falta) | Casos clásicos (viga MBB, ménsula) dan la forma conocida; la pieza optimizada se lamina |
| A3 | — | Fluidos (CFD), en vóxeles con Lattice Boltzmann y en GPU: aerodinámica de la pieza, y como opción la simulación del enfriamiento al imprimir (el aire del ventilador sobre la pieza, el calor que entra con cada capa y cómo se va; con eso se ajustan por zona el ventilador, la velocidad y el tiempo de capa, y se anticipa la deformación) | Casos de referencia (cilindro, escalón) dentro del error esperado; el enfriamiento de un bloque contra la solución conocida de la conducción con convección |
| F5 | 13–15 | Soportes en grilla (voladizos por ángulo, proyección, interfaz y distancia), falda y borde. En GPU: voladizos y proyección | Una pieza con voladizos se imprime y el soporte sale |
| F6 | 16 | Reemplazo de Tisma Slicer (decisión del dueño, 2026-10-24): Tisma Cloud llama al motor; comparación de calidad y velocidad con Tisma Slicer como caja negra, con y sin GPU; cada fila de `docs/spec/COVERAGE.md` hecha o con una decisión del dueño; la migración de proyectos y perfiles que elija el dueño | Medido y documentado; ninguna función de Tisma Slicer se pierde; junto con P1–P5, Tisma Slicer se puede dejar |
| F7 | después de F6 | No planar en 3 ejes: capas apenas curvas cerca de la superficie superior (CurviSlicer, Ahlers et al.), con control de choques del cabezal | Una pieza con techo curvo se imprime sin escalones visibles en una impresora común; medido |
| F8 | después de F7 | Lo propio de Tisma, desde sus fichas (los análisis pasaron a A1–A3): Z anti-aliasing, relleno entrelazado, impresoras de cinta, resina | Las pruebas de cada ficha pasan |

**Orden (decisión del dueño, 2026-10-24):** los análisis (A1–A3) van antes de F5–F7, porque condicionan el diseño
de la pieza, y el diseño es generativo e iterativo: la optimización topológica cambia la forma para que use las fuerzas
que necesita. Antes se termina el Voronoi propio, que está en curso (F2). Lo hecho de F5 (falda, borde, soportes)
queda; el resto de F5 sigue después de A3.

**Fuentes de A1–A3** (solo los artículos y libros; el código publicado junto a algunos, como los programas cortos de
optimización topológica, no se transcribe):
- Bendsøe, *Optimal shape design as a material distribution problem*, Struct. Optim. 1:193–202, 1989 (SIMP).
- Bendsøe y Sigmund, *Topology Optimization: Theory, Methods and Applications*, Springer, 2003.
- Svanberg, *The method of moving asymptotes*, Int. J. Numer. Methods Eng. 24:359–373, 1987.
- Langelaar, *An additive manufacturing filter for topology optimization of print-ready designs*, Struct. Multidisc.
  Optim. 55:871–883, 2017.
- Wu, Dick y Westermann, *A System for High-Resolution Topology Optimization*, IEEE TVCG 22(3), 2016 (multigrilla en
  GPU).
- Aage et al., *Giga-voxel computational morphogenesis for structural design*, Nature 550:84–86, 2017.
- Krüger et al., *The Lattice Boltzmann Method: Principles and Practice*, Springer, 2017.

**Fases de producto** (decisión del dueño, 2026-10-24: son obligatorias antes de dejar Tisma Slicer; el orden lo decide
el dueño; P1 ya empezó, ver «Estado de P1»):

| Fase | Contenido | Base |
|---|---|---|
| P1 | Interfaz propia: ventana, vista 3D, vista previa del G-code, ajustes, plancha | Fichas de la interfaz de Tisma (qué hay en cada pantalla y cómo se comporta) |
| P2 | Device y conexión con impresoras de varias marcas (pedido del dueño, 2026-10-24): la página de dispositivos (lista, estado, temperaturas, avance, cámara, controles, subir e imprimir); USB, Bambu Lab, OctoPrint, Moonraker/Klipper, PrusaLink, Duet/RepRapFirmware, Creality, Elegoo, Anycubic, Flashforge y las demás | La ficha de Device que se le pidió al equipo de fichas el 2026-10-24 (comportamiento, sin código: los hosts de Tisma Slicer vienen de código de OrcaSlicer) y la documentación pública oficial de cada API. Donde no hay documentación oficial (por ejemplo, el modo LAN de Bambu Lab), decide el dueño. Las bibliotecas (HTTP, TLS, MQTT, descubrimiento en la red, puerto serie) se revisan con las reglas de licencias antes de usarlas |
| P6 | App móvil de Android que sigue la impresión (decisión del dueño, 2026-10-24: la opción 1, la estética de la app de Prusa —EasyPrint— como caja negra, y el teléfono solo por WiFi: el USB es del laminador de PC): impresoras, impresión, cámara, controles, archivos y avisos, con la capa de dispositivos de P2 compartida en C++; las impresoras por USB de la PC se siguen a través de un servicio local de `tisma`; después, laminar simple desde el teléfono con el motor | `docs/MOBILE.md`; la ficha de Device; la documentación pública de la API USB Host de Android y de los chips serie; las bibliotecas revisadas con las reglas de licencias |
| P3 | Perfiles de impresoras y filamentos, desde los datos oficiales de cada fabricante (en curso: `docs/CATALOG.md`) | Nuevo: los perfiles actuales vienen de PrusaSlicer, OrcaSlicer y Cura, y no se usan |
| P4 | Formatos: 3MF (especificación pública), STEP (OCCT es LGPL-2.1 con excepción: no entra en la lista de licencias permitidas; hace falta otra biblioteca o una decisión del dueño), formatos de resina desde las especificaciones de los fabricantes | Nuevo |
| P5 | Tisma Cloud, app móvil, CI, instaladores y marca sobre el slicer propio | Fichas; el logo lo da el dueño |

**Multimaterial con un cabezal** (pedido del dueño, 2026-10-24: «el efecto de color de Full Spectrum, en el nuestro, con
impresoras de un solo cabezal, junto con purgas internas en el infill»). El diseño está en `docs/MULTIMATERIAL.md`.
Antes estaba «fuera de la primera versión»; ahora es una fase.

| Fase | Contenido | Terminada cuando |
|---|---|---|
| MM1 | Varias piezas por placa con su ranura, cambio de herramienta por plantilla, matriz de purga, torre de purga | Una pieza de dos colores se imprime con MMU o con `M600` |
| MM2 | Purga en el relleno y orden de herramientas por capa | Menos de la mitad del filamento tirado que con solo torre; medido |
| MM3 | Filamentos mezclados por capas, degradados y color buscado: el efecto de color con un solo cabezal | Dos filamentos dan tres tonos o más distinguibles; medido con fotos |
| MM4 | Pintar caras; mezcla dentro de la capa | Las pruebas de cada parte pasan |

**Al final** (decisión del dueño, 2026-10-24: los 5 ejes son lo último antes de macOS, y macOS va al final de todo):

| Fase | Cuándo | Contenido | Terminada cuando |
|---|---|---|---|
| F9 | antes de macOS | 4 y 5 ejes: campo escalar en el volumen (malla de tetraedros), capas curvas como isosuperficies (Dai et al. 2018, S³-Slicer, Neural Slicer), orientación de la boquilla, cinemática de la máquina (boquilla inclinada que gira, mesa giratoria, cabezal), choques y G-code con ejes giratorios | Una pieza con voladizos se imprime sin soportes en una máquina de 4 ejes y en una de 5 (hace falta decidir cuáles; la impresora propia del dueño es candidata) |
| F10 | al final | macOS: CI en cada envío y arreglos para que compile y pase las pruebas (decisión del dueño, 2026-10-24: se deja para el final) | Compila y prueba en macOS |

**Fichas:** el equipo que conoce Tisma Slicer las escribe en el orden de las fases: primero corte, perímetros,
superficies, G-code (enfriamiento, coasting, arc overhangs, revisión antes de imprimir) y soportes.

## Estado de F0

- [x] CMake, C++20
- [x] Catch2
- [x] Línea de comandos (`slice` solo lee el modelo y responde «not implemented»; `info` imprime el resumen)
- [x] Lectura de STL binario y ASCII, con pruebas
- [x] CI en cada envío: Linux (gcc, clang, ASan+UBSan) y Windows (MSVC). macOS pasa a F10, al final
- [x] Linux pasa a compilarse y probarse dentro de la sesión antes de cada envío (`tools/ci_linux.sh`, decisión del
  dueño, 2026-10-24); en GitHub, Windows en cada envío y Linux solo a mano
- [x] `THIRD_PARTY.md`, `PROVENANCE.md`
- [x] Versiones y releases (pedido del dueño, 2026-10-24): versionado semántico con el número en `VERSION`,
  `CHANGELOG.md` por categorías, releases automáticas desde las etiquetas (`docs/VERSIONING.md`); documentación para
  colaboradores (`docs/ARCHITECTURE.md`, `docs/DEVELOPMENT.md`)
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
- [x] Acelerar el eje medio, que ahora domina el corte (con paredes, `slice` de la esfera pasa de 0,2 s a 1,0 s).
  Medido: el Voronoi de segmentos de Boost cuesta 13–28 µs por segmento con cualquier polígono, y el 54 % es su
  aritmética exacta (`extended_int`), porque cada vértice es extremo de dos segmentos. Es lo que informa el artículo
  (5·10⁻⁶·n·log n s). **Decisión del dueño (2026-10-24): un Voronoi de segmentos propio**, a partir de artículos,
  pensado también para la GPU. Pasos, cada uno de uno o más bloques:
  - [x] elegir el método en los artículos: inserción incremental orientada a la topología (Sugihara e Iri; Held y
    Huber 2009, sección 2; Huber 2008, capítulo 2), en coma flotante, robusta por construcción;
  - [x] diagrama de puntos (`src/geom/voronoi.*`): coincide con Boost en puntos al azar, grillas, un 64-gono y una
    estrella; con los vértices de un polígono en orden es 4–5 veces más rápido que Boost;
  - [x] inserción de segmentos: cono de influencia, semilla en las celdas de los extremos, ruptura de ciclos con nodos
    de grado dos, nodos nuevos por bisección sobre la arista exacta; coincide con Boost en cuadrado, L, 64-gono,
    estrella, cuadrado con agujero y peine;
  - [ ] nodos por fórmula cerrada (punto–punto–segmento, punto–segmento–segmento, segmento–segmento–segmento) en vez de
    bisección: hoy la bisección es dos tercios del tiempo de un segmento;
  - [x] la misma salida que hoy (`MedialEdge`), probada contra Boost.Polygon como oráculo en cuadrado, tira, anillo,
    64-gono, estrella, curva ondulada, L y cuña;
  - [x] medido en la esfera y reemplazado: `slice` de 1,23–1,29 s a 0,94–1,05 s, con el mismo viaje y las mismas
    retracciones; Boost queda solo como referencia en las pruebas;
  - [ ] después, la parte que convenga en la GPU.
  Descartado por el dueño: el híbrido con desplazamientos de Clipper2.
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
- [x] Escribir el G-code más rápido: números con `std::to_chars` y sin cadenas por línea; la esfera pasa de 1,6 s a
  0,65 s de escritura con el mismo archivo byte a byte (más adelante: formato propio exacto o capas formateadas en
  paralelo)
- [x] Orden de recorridos (el más cercano) y costura de los lazos (`walls.seam`: `aligned`, `nearest`, `random`, `rear`)
- [x] Retracción en viajes largos y salto en Z (grupo `retraction`)
- [x] Viajes por dentro de la pieza sin cruzar perímetros, y retracción cuando el viaje sale de la pieza
  (`src/gcode/travel.*`)
- [x] Orden por islas (terminar una isla antes de pasar a la otra)
- [x] Enfriamiento: tiempo mínimo de capa con las dos etapas de `BEHAVIOR.md` y ventilador al máximo en esas capas
  (grupo `cooling`, `src/gcode/cooling.*`)
- [x] Aceleraciones y límites de la máquina; estimación de tiempo (opcional, como comentario)
  (`src/gcode/print_time.*`)
- [x] Avisos en un JSON aparte (detalles que no se imprimen, capas que no llegan al tiempo mínimo)
  (`src/report/warnings.*`, `slice --warnings`)
- [x] Entradas imposibles con mensaje claro y código 4 (malla vacía, sin capas cerradas, fuera de la cama, demasiado
  alto), sin G-code a medias (`src/slice/printable.*`)
- [ ] Criterio de F4: el Benchy se imprime en una impresora real (necesita al dueño)

## Estado de A1

Análisis estructural por elementos finitos en vóxeles (fuentes en «Fuentes de A1–A3»).

- [x] La pieza en vóxeles con el corte del motor (`src/fea/voxels.*`)
- [x] Hexaedro trilineal de elasticidad lineal isótropa (`src/fea/hex8.*`)
- [x] Gradiente conjugado sin matriz con Jacobi, en paralelo y determinista; apoyos por eje y cargas como tracción
  sobre caras (`src/fea/elasticity.*`)
- [x] Validación: barra a tracción exacta; voladizo a −5,1 %, −2,1 % y −1,3 % de la teoría de vigas con vóxeles de 1,
  0,5 y 0,25 mm
- [x] Vóxeles en forma de caja que siguen el laminado (pedido del dueño): hx × hy en el plano, por ejemplo 4 por ancho
  de línea en los modelos de alta calidad, y en altura cada capa partida en filas enteras (la primera capa también),
  así que los límites entre capas caen en caras de elementos
- [x] Ajustes del análisis: tamaño del vóxel desde el ancho de línea y la altura de capa (calidad normal y alta)
- [x] **Adherencia entre capas y entre líneas (pedido del dueño, para la máxima fiabilidad):** material ortótropo «como
  se imprimió» (`src/fea/material.*`): ejes del cordón, entre cordones y entre capas, con rigidez y resistencia propias
  en cada uno; la dirección de cada vóxel tomada del laminado real (`src/fea/as_printed.*`); el solver con una matriz
  por dirección (pasos de 5°); criterio de Tsai–Wu, mapa del factor de seguridad y modo de falla (cordón, unión entre
  cordones, unión entre capas); la pieza con los huecos del relleno ralo cuando hay 4 vóxeles por ancho de línea
- [x] Base de materiales (pedido del dueño): literatura (Song 2017, Casavola 2016, Cantrell 2017, Ahn 2002), hojas
  técnicas de Bambu Lab (11 materiales) y eSUN (2), y ensayos de CNC Kitchen (6); cada número con su fuente y reglas
  escritas para lo que falta (`src/fea/materials.*`, `docs/MATERIALS.md`)
- [ ] La unión entre capas según la temperatura de la capa de abajo al imprimir la de arriba (con la simulación del
  enfriamiento de A3 y los datos de CNC Kitchen)
- [x] Multigrilla como precondicionador del gradiente conjugado (`src/fea/multigrid.*`): las iteraciones ya no crecen
  al achicar el vóxel (11–12 en el voladizo de 1 a 0,125 mm); las piezas sueltas quedan fuera del cálculo
- [ ] Multigrilla más fuerte con huecos: nodos gruesos duplicados por cada pedazo separado dentro de una celda gruesa
  (el relleno ralo todavía pide 61 iteraciones contra 11 de la pieza maciza)
- [ ] Núcleo de la GPU, probado contra la CPU
- [x] Cargas y apoyos desde la línea de comandos (JSON) y resultado legible: `tisma-engine analyze` con el caso en
  JSON (material de la base, calidad, apoyos y cargas por caja o por cara), informe JSON y mapa VTK para ParaView
  (`docs/ANALYSIS.md`)
- [x] Factor de seguridad sobre la tensión promediada en un ancho de línea y una capa (el pico de un vóxel no converge
  en las aristas de los apoyos); estudio de convergencia en `docs/ANALYSIS.md`
- [x] Estimar el error de resolución: el caso se repite con vóxeles del doble de ancho, se avisa si el factor cambia
  más de un 5 % y se extrapola en primer orden; en la placa de prueba la extrapolación de `high` acierta dentro de un
  0,2 % (maciza y como se imprime)
- [ ] Representar mejor el contacto entre cordones en «como se imprime» (vóxeles parcialmente llenos en vez de
  todo o nada), para llegar al valor convergido con vóxeles más gruesos: hoy hacen falta 8 vóxeles por ancho de línea
- [ ] Criterio de A1: placa con agujero (concentración de tensiones de Kirsch, K = 3) dentro del error esperado
- [x] Lo que la interfaz necesita del motor: `--estimate` (vóxeles, memoria y tiempo antes de correr), `--progress`
  y `--mesh` (la superficie de los vóxeles coloreada); lo que debe mostrar está en `docs/ANALYSIS.md`, «Para la
  interfaz»
- [ ] Interfaz del análisis dentro de Tisma Slicer (decisión del dueño, 2026-10-24): la construye la sesión principal
  de Tisma con el comando como caja negra; el motor responde lo que haga falta

**Fuentes de la anisotropía:**
- Ahn, Montero, Odell, Roundy y Wright, *Anisotropic material properties of fused deposition modeling ABS*, Rapid
  Prototyping Journal 8(4):248–257, 2002.
- Tsai y Wu, *A general theory of strength for anisotropic materials*, J. Composite Materials 5(1):58–80, 1971.
- Hill, *A theory of the yielding and plastic flow of anisotropic metals*, Proc. R. Soc. A 193:281–297, 1948.
- Tsai y Pagano, *Invariant properties of composite materials*, 1968 (rigidez de un material ortótropo girado como suma
  de cinco términos en cos 2θ, sin 2θ, cos 4θ y sin 4θ).
- Jones, *Mechanics of Composite Materials*, 2.ª ed., 1999, cap. 2 (compliancia ortótropa y rotación de ejes).
- Los datos de cada material y sus fuentes (Song 2017, Casavola 2016, Cantrell 2017, Bambu Lab, eSUN, CNC Kitchen):
  `docs/MATERIALS.md`.

## Estado de F5

Desde `PLAN.md` (fase F5), `docs/spec/BEHAVIOR.md` («Soportes») y el tipo `Skirt/Brim` de `docs/spec/GCODE_OUTPUT.md`,
con nombres propios en `docs/CONFIG.md`.

- [x] Falda y borde (grupo `adhesion`, `src/adhesion/*`)
- [x] Voladizos por ángulo y su proyección hacia abajo (grupo `support`, `src/support/*`: solo las áreas por capa)
- [x] Soporte en líneas con separación en Z y XY, interfaz densa y base (tipos `Support material` y
  `Support material interface`)
- [ ] Puentes cortos sin soporte (`max_bridge_length`)
- [ ] En GPU: voladizos y proyección, si conviene. Hecho en parte (2026-10-26, pedido del dueño):
  - la columna del soporte va en una grilla de bits de un octavo de cordón (`src/geom/bitgrid.*`); el cierre por un
    cordón es dilatar y erosionar con un disco, y cuesta lo mismo aunque la pieza tenga ruido;
  - el mismo paso corre en la GPU (`support_column.comp`), con los mismos bits que la CPU (prueba con radios 1, 8 y
    31, y un laminado con soportes);
  - cráneo de 546 684 triángulos con soportes: 12,9 → 5,8 s; Benchy con soportes 1,38 → 1,18 s; las esferas igual;
  - falta: medir la GPU en una placa real (acá solo hay lavapipe, que emula la GPU en la CPU y tarda más que el
    camino de la CPU); los voladizos siguen en la CPU, con Clipper2, y cuestan poco (85 ms en el cráneo)
- [ ] Criterio de F5: una pieza con voladizos se imprime y el soporte sale (necesita al dueño)

## Altura de capa por zona (pedido del dueño, 2026-10-25)

«Altura de capa variable que dependa del detalle y no de la pieza entera»: a la misma altura, capas finas donde la
pieza cambia y gruesas donde no. Diseño en `docs/LOCAL_LAYERS.md` (local adaptive slicing, Tyberg y Bøhn 1998).

- [x] Las zonas, el recorte de caminos y la escritura, con `layers.detail_height`, `detail_tolerance` y
  `detail_margin`; G-code verificado
- [x] La interfaz: los ajustes en Impresión y la vista previa con el color por alto de capa
- [x] Medir en el banco contra todo fino y todo grueso (−39 a −52 % de tiempo contra todo fino); el recorte,
  indexado, cuesta poco: sin GPU
- [x] La altura variable por Z de la ficha (`slicing.md`): la lista de (Z, alto), la altura por pendiente (escalón
  máximo) y las dos combinadas con la altura por zona, dentro de una misma pieza (`docs/VARIABLE_LAYERS.md`). La
  lista vale para toda la cama: el motor lamina los objetos en una sola pila
- [x] Z anti-aliasing (`docs/spec/features/z_antialiasing.md`; Song, Ray, Sokolov y Lefebvre 2017): las extrusiones
  de arriba siguen la superficie dentro de la capa, hacia abajo (la ficha) y hacia arriba (el artículo), y el perfil
  Máxima calidad las combina con la altura por zona y por pendiente (`docs/ZAA.md`)
- [x] ZAA: el orden del artículo con grafo de alturas (pedazos por lado, franjas, el más bajo primero entre
  vecinos): interferencia del 7 % al 1 % en un techo a 8°, 0 % en la cúpula
- [ ] ZAA: las líneas de arriba según las curvas de nivel (partir donde el orden se da vuelta se probó y no rinde: +65 %
  de arranques por 0,07 puntos), menos costuras (Sección 4.2), y medir en una impresión real
- [ ] Pedidos del dueño en cola (2026-10-25):
  - relleno optimizado para resistencia y rapidez (`docs/INFILL.md`), por etapas:
    - [x] combinar capas de relleno (ficha `infill_patterns.md`, reglas 18 a 20): −24 a −45 % de tiempo, el mismo
      material
    - [x] relleno denso bajo las superficies de arriba (ficha `dense_infill.md`)
    - [x] patrones triángulos y cúbico (ficha `infill_patterns.md`, reglas 4 y 6)
    - [x] cúbico adaptativo (regla 7): la profundidad con la transformada de distancia exacta, una línea de cada dos
      por nivel; −26 % de material y −13 % de tiempo en un cubo de 100 mm al 20 %
    - [x] cúbico de soporte (regla 8): el adaptativo medido desde lo que el relleno sostiene y las paredes
    - [x] rayo (regla 9): ramas de arriba hacia abajo, según Tricard, Claux y Lefebvre 2020
    - [ ] rayo: enderezar y juntar ramas (el índice espacial ya está: 63 s → 2,1 s al 100 % en el cubo de 100 mm)
    - [ ] cúbico de soporte: ajustarlo a las pruebas P6c y P6d de la ficha cuando el PR #15 esté en `main`
  - el resto de la ficha `infill_patterns.md` (pieza de la cobertura de Tisma Slicer):
    - [x] estrellas, rectilíneo alineado y zigzag (reglas 1, 2 y 5)
    - [x] línea, panal y concéntrico (reglas 3, 10 y 13): los largos de P3, P8 y P9 dentro del 2 %
    - [x] Hilbert, cuerdas de Arquímedes y octagrama (regla 12): Hilbert y Arquímedes a −5 % de P9, el octagrama a
      −17 % (la separación de Tisma Slicer en ese patrón queda por contrastar)
    - [ ] Hilbert y cuerdas de Arquímedes en regiones grandes y densas: el recorte con Clipper2 crece más que lineal
      (lista de aristas activas ancha): 150 mm al 100 %, 0,14 s por capa; 300 mm, 1,2 s. Recortar por franjas y volver
      a unir los pedazos en los bordes de las franjas
    - [ ] panal 3D (regla 11): la ficha describe poco su forma; pedir más mediciones al equipo de fichas
    - [ ] los patrones de las superficies de arriba y de abajo (14 a 17)
    - [x] una capa sólida cada N y el espesor mínimo de la cáscara (21 y 22)
    - [x] relleno guiado por tensiones (fase A2): `infill.stress_density` con `slice --case`
    - [x] el caso de carga en el laminado de la interfaz (un `.json` como el de `analyze`)
    - [ ] editar el caso en la interfaz (apoyos y cargas), y la iteración analizar-reforzar
    - [ ] medir la rigidez con el análisis «como impresa» leyendo el laminado real
  - relleno y paredes no planares para más resistencia, y una versión no planar de brick layers que no infrinja
    patentes (`docs/NONPLANAR_INFILL.md`):
    - [x] N1: relleno ondulado (ZigZagZ en el relleno que se repite capa a capa), con la amplitud que baja cerca de
      paredes y pieles y el espesor según el cordón de abajo; con el tope de Z de 12 mm/s, un 85 % más de tiempo
    - [ ] N3: el análisis «como impresa» con la unión entre capas inclinada, para medir N1 y N2
    - [ ] N2: paredes tejidas (perímetros ondulados en contrafase, capas anidadas), la versión no planar de brick
      layers
    - [ ] N4: pernos en Z, si la patente de ORNL lo permite (decide el dueño)
    - revisión de patentes por un abogado antes de vender (decide el dueño)
  - laminado no planar: los artículos y qué conviene (`docs/NONPLANAR_SLICING.md`): en 3 ejes, techos curvos, después
    el campo optimizado de CurviSlicer y un término de resistencia con el análisis propio (F7); en 5 ejes, S³-Slicer
    (F9). Actualizado el 2026-10-26 (`docs/NONPLANAR_2026.md`): F7 sobre las superficies de corte en grilla 2D de
    Double QuickCurve, en este orden:
    - [ ] la base, probada con `wave` (NP1 a NP4): hechos (2026-10-27) la onda (huevera, crestas, torcidas) y el cono
      fijo, la rampa, el ajuste a la cabeza con `J` entre 0,6 y 1,4, la malla refinada y deformada, el G-code curvado
      con el material por `J` y el tope de velocidad de Z, el verificador sobre la curva y la vista previa. Faltan
      suavizar las capas donde la cabeza chocaría (hoy es un error), el tope de aceleración de Z y el caudal `uniform`
    - [ ] `surface` con la superficie de arriba (hecho 2026-10-27, SU1 a SU5 y CO1), y el Z anti-aliasing encima
    - [ ] `overhangs` con la superficie de abajo y `conical` según la pieza (hechos 2026-10-27, CO1 a CO4, VO2 a
      VO5) y `automatic` (AU1 a AU3)
    - [ ] `stress`, solo si el análisis «como impresa» muestra ganancia
  - soportes: menos, más fáciles de sacar y con mejor superficie (`docs/SUPPORT_RESEARCH.md`):
    - [x] S1: arcos en voladizos, según la ficha `arc_overhangs.md` (método, límites y pruebas en
      `docs/ARC_OVERHANGS.md`): `arcs.*`, reglas 1 a 11, A1 a A5 (2026-10-27)
    - [ ] después, frentes por distancia (las ondas de Andersons et al. 2026), con la transformada de distancia de
      Q11; y que los arcos no tarden tanto (el Benchy con arcos lamina en 1,1 s en vez de 0,6)
    - [ ] S2: la superficie soportada: lo que falta de `supports_more.md`, interfaz no planar que sigue la cara de
      abajo (con el mecanismo del ZAA), planchado de la interfaz, primera capa sobre el soporte como puente
    - [x] S3: soportes en árbol, según `supports_more.md` (reglas 15 a 17): `support.style` `organic` (2026-10-26);
      las puntas vecinas enlazadas de a pares y las piezas por capa cerca de SM13 remedido (2026-10-27)
    - [ ] S4: laminado cónico como campo (con F7)
    - [ ] S5: interfaz de otro material (con MM1)
  - simulación del enfriamiento para los detalles chicos que la capa entera no ve (`docs/COOLING_SIMULATION.md`):
    - [ ] T1: tiempo de vuelta local (desde el cordón de abajo y con la boquilla cerca): vista previa, aviso, y
      frenado, pausa y ventilador solo en el detalle
    - [ ] T2: red térmica de cordones (Costa et al. 2017; Zhang y Shapiro 2018), calibrada con pernos
    - [ ] T3: las correcciones por temperatura, menos ventilador donde la unión queda fría, y la adherencia por unión en
      el análisis «como impresa»
    - [ ] T4: la red en GPU, y el aire del ventilador de A3
    - no reordenar según el calor sin la revisión de patentes (dos solicitudes de Siemens)
  - lo publicado sobre FDM y técnicas de laminado que el motor todavía no usa (`docs/RESEARCH_MAP.md`), por ganancia
    medida contra esfuerzo:
    - [ ] Q1: modo resistencia con cordones anchos (2,5 veces la boquilla o más: +40 a +48 % entre capas, −67 % de
      tiempo, Allum et al. 2023)
    - [ ] Q2: estimador de tiempo que imita al firmware (aceleración por tramo, velocidad de esquina, calibración
      contra impresiones medidas); va junto con el pedido del tiempo de impresión
    - [ ] Q3: topes por fusión y enfriamiento (flujo volumétrico según la temperatura y el material)
    - [ ] Q4: flujo según la velocidad planeada en las esquinas, y avance de presión por tipo de tramo
    - [ ] Q5: cordones alineados con la tensión por región, y propiedades ortótropas según las trayectorias en el
      análisis
    - [ ] Q6: costura por costo (cóncava, atrás, lo que se ve, tensión) y en bisel (`walls_extra.md`). Hecha la
      costura en bufanda (`walls_extra.md`, regla 4, 2026-10-26, pedido del dueño): falta la costura por costo
    - [ ] Q7: mapa de rugosidad por cara y compensación en XY según el ángulo
    - [ ] Q8: riesgo de alabeo con un modelo cerrado, borde automático y patrón según el material
    - [ ] Q9: orden de islas y viajes que escala (curva que llena el plano y 2-opt)
    - [ ] Q10: umbral de voladizo según ventilador, velocidad y temperatura; puentes con anchos enteros de línea
    - [ ] Q11: transformada de distancia exacta en Vulkan, número de vueltas para mallas rotas, Voronoi robusto
    - [ ] Q12: interfaz trabada entre materiales (ITIL, con MM1)
    - patentes que evitar o revisar en la sección 6 del documento (entre ellas las B-splines filtradas y las espirales
      de Fermat conectadas)
    - lo nuevo de 2018 a 2026 (`docs/QUALITY_2026.md`, 2026-10-26), por ganancia contra esfuerzo:
      - [x] Q13: planchado según `ironing.md` (Ra −60 a −83 % en cuatro estudios; hecho 2026-10-27)
      - [ ] Q14: sección redondeada para los cordones sin vecinos (relleno ralo, capas combinadas, paredes de un
        cordón): el rectángulo pone 11 a 27 % de más; explicaría P11 de `docs/INFILL.md`
      - [ ] Q15: sin bordes débiles entre bandas de densidad del relleno (Hutton y Bartolai 2023: +26 a +33 % de
        deformación a la rotura)
      - [ ] Q16: ventilador por tipo de tramo (alto en voladizos, puentes y pared externa; bajo adentro), con Q1 y T3;
        medirlo en el banco, porque los estudios no coinciden
      - [ ] Q17: pieza de capacidad (agujeros y pernos de 2 a 20 mm) y compensación de agujeros según el diámetro
      - [ ] Q18: lista de ángulos por capa para el relleno y las superficies sólidas
      - patente US 10,353,378 B2 (WARF): toca el análisis «como impresa» que ya existe; al abogado ya (decide el
        dueño)
  - brillo parejo (pedido del dueño, 2026-10-25; `docs/GLOSS.md`):
    - [x] G1 y G2: la pared externa a flujo constante en toda la pieza; para el tiempo mínimo de capa, frenar primero
      lo de adentro (`cooling.keep_outer_speed`)
    - [ ] esperar con la boquilla lejos de la pieza cuando frenar lo de adentro no alcanza; G3, los voladizos
    - [ ] G4: temperatura según el flujo planeado (experimental, con Q3)
    - [ ] la calibración de brillo: bandas a flujo constante, de 2 a 20 mm³/s, a tres temperaturas
  - lo que quedó de la revisión profunda (2026-10-25), sin errores de seguridad:
    - [x] resolución del G-code (juntar puntos alineados con el mismo ancho, desvío de 0,0125 mm): las paredes
      duplican vértices, el gyroid escribe el triple de tramos y quedan tramos de un micrón que extruyen
    - [ ] el gyroid por franjas, sin la grilla entera en memoria; el orden monótono sin costo cuadrático
    - [ ] escribir y verificar el G-code por capas, sin tenerlo entero en memoria
    - [ ] el Voronoi propio más rápido en capas con miles de sitios (hoy es lo más lento con muchas islas)
    - [ ] el verificador con un mapa de alturas de lo ya impreso (ningún movimiento por debajo)
    - [x] `infill.solid_below_area` de la ficha `surfaces_infill.md` (zonas chicas sólidas)
    - [ ] `info` cuenta contornos de área cero en mallas con triángulos degenerados, y su `volume` suma las mallas que
      se pisan (el laminado está bien)
    - [ ] la verificación del G-code crece más que lineal con las islas (equipo de fichas: ×37 de 25 a 400 cilindros,
      el 16 % del total con 400)
  - el tiempo de impresión estimado lo más preciso posible;
  - el panel izquierdo con niveles simple, medio y avanzado más uno personalizado, y tres menús grandes (impresora,
    filamento, objetos) en el mismo panel

## Estado de P2

Empezado el 2026-10-24 por pedido del dueño («arrancá con `src/device` y el host serie»), antes de que la ficha de
Device esté en `main` (PR #11): lo primero es el USB del escritorio, que la ficha no cambia. Diseño en
`docs/DEVICE.md`.

- [x] La capa de dispositivos `src/device/`: `Printer`, `ByteStream`, el puerto serie (Linux y Windows), el host de
  G-code por serie (numeración, suma de verificación, reenvíos, temperaturas, pausa y cancelación) y la impresora
  simulada, con pruebas de punta a punta
- [x] La página Device de `tisma` (pestaña Impresora): puertos, conectar, estado y temperaturas, imprimir lo laminado
  o el G-code abierto, pausar, seguir, cancelar, mover y extruir, registro; la impresora simulada para probar sin
  máquina, con una prueba de punta a punta
- [ ] El servicio local de `tisma` para la app de Android (`docs/MOBILE.md`)
- [ ] Las conexiones de red de la ficha de Device: Moonraker, OctoPrint, PrusaLink, Duet, las marcas
- [ ] Probado con una impresora real por USB (necesita al dueño)

## Estado de P1

La interfaz propia se adelantó (pedido del dueño, 2026-10-24: «hagamos la interfaz con esos íconos y centrémonos en
eso»). Por ahora no hay fichas de la interfaz de Tisma; todo es diseño propio (`docs/UI.md`).

- [x] Bibliotecas con licencia permisiva revisadas y anotadas: GLFW, Dear ImGui, nanosvg; cargador de OpenGL propio
- [x] Ventana con los íconos y la paleta de Tisma, en español e inglés, tema oscuro y claro
- [x] Vista 3D con cámara orbital, vistas estándar y la cama
- [x] Abrir STL, G-code y ajustes (explorador propio y soltar archivos)
- [x] Ajustes de impresión, filamento e impresora; guardar y cargar
- [x] Laminado en segundo plano, vista previa por capas, exportación verificada
- [x] Cantidad de vóxeles del análisis (pedido del dueño)
- [x] Prueba de punta a punta con capturas (`ui_slice_export_screenshot`)
- [x] Iluminación mejorada, oclusión ambiental, ray tracing activable y suavizado de bordes (pedido del dueño)
- [x] Calibraciones, primera parte (pedido del dueño, 2026-10-24; `docs/CALIBRATION.md`): compensación geométrica
  desde medidas (escala por eje, agujeros), torres de temperatura, retracción, flujo y velocidad máxima
- [x] Calibraciones automáticas y guiadas (pedido del dueño): crear con un clic con rangos desde los ajustes, pasos
  guiados, usar el valor elegido; torre de temperatura con cresta de ángulo variable, estante plano, perno de
  resistencia y detalle en relieve
- [x] Calibraciones estándar, segunda tanda: primera capa, velocidad de retracción, avance de presión (ajuste nuevo
  `extrusion.pressure_advance`), tolerancias de encastre, pasos del extrusor y el flujo volumétrico en la torre de
  velocidad
- [x] Calibraciones estándar, tercera tanda: tope de flujo volumétrico (`machine.max_flow`), resonancias (el motor
  escribe la aceleración: `M204`, `SET_VELOCITY_LIMIT`), pata de elefante (`slicing.first_layer_shrink`)
- [ ] Calibraciones estándar que faltan: puentes y voladizos
- [ ] App móvil de Android para seguir la impresión (pedido del dueño, 2026-10-24). Decidido: la opción 1 (seguir
  la impresión, no la interfaz entera), solo por WiFi (el USB es de la PC), con la estética de la app de Prusa
  (EasyPrint) como caja negra; después, Device en el escritorio. El plan y el orden de trabajo están en `docs/MOBILE.md` (P6)
- [ ] Calibraciones nuevas propuestas: compensación por mínimos cuadrados, ancho de línea variable, adherencia entre
  capas con el análisis, contracción por material
- [ ] Pruebas de laminado no planar (altura y ángulo que admite la máquina): a definir con el dueño cuando llegue F7
- [x] Opciones de guardar (pedido del dueño): perfiles con nombre de impresora, filamento e impresión, con perfiles de
  fábrica genéricos (`docs/PROFILES.md`), y proyectos `.tisma` (`docs/PROJECT.md`)
- [x] Arc Overhang (pedido del dueño): techos planos en el aire sin soporte, con cordones que crecen en arcos desde el
  borde apoyado. En sala limpia: desde la descripción pública de la idea, con diseño propio, sin leer código GPL.
  Hecho el 2026-10-27 (`arcs.enabled`, Soportes › Arcos en voladizos). Falta probarlo impreso (el estante plano de la
  torre de temperatura; necesita al dueño)
- [ ] El análisis completo desde la interfaz: apoyos, cargas, material y el mapa de seguridad sobre la pieza
- [x] Mover, girar y escalar el modelo; varias piezas en la cama (pedido del dueño, 2026-10-25): Agregar, Quitar,
  Duplicar, Mover (y arrastrar), Rotar, Escalar, Apoyar en una cara y Organizar (`docs/UI.md`)
- [x] Pintar en el modelo (pedido del dueño): soportes forzados y bloqueados, costura forzada y bloqueada; el motor lee
  lo pintado (`docs/PAINTING.md`, ficha `supports_more.md`)
- [x] Piel difusa (ficha `fuzzy_skin.md`, reglas 1, 2 y 4) y su pincel; falta la regla 3 (lo que sale de la capa de
  abajo como voladizo, con los voladizos de `walls_extra.md`) y las paredes de las zonas gruesas de `docs/LOCAL_LAYERS.md`
- [ ] Pintar los colores (con `multi_extruder.md`); volúmenes bloqueadores y forzadores (SM12); relleno inteligente de
  una cara y deshacer
- [x] Ayuda (pedido del dueño, 2026-10-26): asistente, consejos de la primera vez y botón (i) en cada ajuste
  (`docs/UI.md`, «Ayuda»)
- [ ] Ayuda, lo que sigue: el texto de los ajustes en inglés; señalar en la pantalla lo que cada paso del asistente
  nombra
- [ ] Cortar, Partir y Medir de la ficha `interface.md`
- [x] Modelos grandes en la interfaz (pedido del dueño, 2026-10-26): arrastrar no rehace la malla en cada cuadro, y el
  clic y el pincel buscan en un árbol de cajas (`docs/UI.md`, «Modelos grandes»)
- [ ] Modelos grandes, lo que sigue: armar el árbol de cajas y la malla de la placa en segundo plano; los campos de
  giro y escala sin rehacer toda la placa en cada cambio
- [ ] Perfiles de impresoras y filamentos (P3)
- [ ] Fichas de la interfaz de Tisma, cuando lleguen
- [x] Modo vaso (`vase_mode.md`, pedido del dueño de una pared en espiral, 2026-10-26): una sola pared que sube en
  espiral, sobre capas de abajo planas; reglas 1 a 6 (la de varios objetos, en la interfaz: el motor lamina uno por
  vez). Varios objetos en modo vaso, uno por uno, con la impresión secuencial (2026-10-27)

## Fichas nuevas (2026-10-26)

Llegaron con el PR #22 a `dev`. Cada una se lee solo si `tools/check_spec.py` pasa sobre ella.

- [ ] `assistant.md` (P1): el panel de ayuda (F1) con Problemas, Procedimientos, Glosario y Tutoriales, sacado de
  `knowledge.tar.xz` del paquete libre; aplicar un ajuste desde la ayuda con deshacer; ficha y avisos del material;
  tutoriales con anclas, eventos y progreso guardado. Primer paso hecho (2026-10-26): el asistente de siete pasos, los
  consejos de la primera vez y el botón (i) de cada ajuste (`docs/UI.md`, «Ayuda»); el (i) mostrará también la
  definición del glosario (regla 12)
- [ ] `settings_levels.md` y `settings_panel.md` (P1): niveles de ajustes, lista propia y secciones más finas.
  Primer paso hecho (2026-10-26, pedido del dueño: entre Cura y OrcaSlicer, simple y sin cargar de información): todos
  los ajustes del motor en secciones con ícono y tarjetas plegables con su resumen, niveles Básico, Normal y Avanzado,
  los dependientes ocultos con su interruptor apagado, los cambios marcados y nunca ocultos (reglas 1 a 3, 6, 7 y 9 a
  14). Falta: Personalizado (15 a 20), «Ver todo junto» (4), la búsqueda (5), Restablecer por tarjeta (8), la tarjeta
  de la impresora con boquilla, cámara y conexión (21 a 44) y la configuración de la impresora aparte (hoy, una pestaña)
- [ ] `preview.md` (P1): las vistas de la vista previa (tipo, velocidad, tiempo de capa, ancho…)
- [ ] `credits.md` (P1): «Basado en…» en cada función que sale de otra fuente
- [ ] `print_time.md` (después de F4): el tiempo simulando cada movimiento del G-code
- [ ] `model_tools.md` (F3 y P4): partes, volúmenes negativos, modificadores, ajustes por objeto y por rango;
  escalar a medida, cortar, separar y copias en grilla
- [ ] `formats.md`: OBJ y 3MF (F1); el resto, P4. Hecho (2026-10-26): leer OBJ y 3MF (reglas 1, 5, 7 y 8; FM1 a FM3,
  FM9) en la línea de comandos y en la interfaz. Falta: AMF, STEP, SVG, ZIP (FM6, FM21), la reparación al abrir
  (regla 4) y escribir el proyecto 3MF y las exportaciones (reglas 6 a 16)
- [ ] `placement.md` (F1): hecho (2026-10-26) centrar la pieza en la cama o en `placement.center` (regla 2, PL1, PL2,
  PL3, PL6); PL15 el 2026-10-27 (sin cama, solo un centro dado a mano la mueve). Falta: varias copias en grilla con `placement.spacing` (PL7, PL8) y `placement.drop_to_bed` «no» (PL5,
  PL9); hoy la pieza siempre se apoya en la cama
- [ ] `profile_pack.md` y `presets.md` (P3): bajar el paquete libre, los perfiles del sistema y los propios. Las
  velocidades del perfil de impresión que el paquete da a cada impresora (`defaults.print`) van a su perfil de
  impresora: las velocidades son de la impresora (decisión del dueño, 2026-10-26; `docs/CONFIG.md`, «Velocidades»)
- [ ] `pellets.md` (P3): impresoras de pellets. Hecho (2026-10-26): los topes de `layers.height` (10 mm) y
  `walls.width` (40 mm); falta `machine.feedstock` y el resto de la ficha
- [ ] `devices.md` (F9; sus pantallas en P1)
- [ ] Después de la primera versión: `color_mixing.md`, `support_fins.md`, `plugins.md`, `waam.md`

Con el PR #23 (2026-10-26):
- [ ] Prueba de caja negra del equipo de fichas (`docs/spec/qa/2026-10-26.md`). Hechos (2026-10-26): QA1 a QA4 (falda
  a 6 mm y redondeada, borde a la separación de los cordones, soporte solo «más de» el límite), QA5 (ventilador por
  tiempo de capa), QA6 (`speed.top_solid_infill` 15 mm/s) y QA7 (la última capa con la primera más gruesa). Faltan:
  - [x] QA8: la revisión de estabilidad (`stability.md`, claves `review.*`; hecho 2026-10-27). Del resto de
    `print_review.md` faltan los detalles perdidos, los puentes largos, las capas sin tiempo (en parte, `layer_time`),
    los viajes por el aire sin retracción y las velocidades sobre el límite;
  - [x] QA9: paredes más finas que la boquilla con `walls.min_line_width` (`perimeters.md`; hecho 2026-10-27, con
    `walls.min_detail`);
  - segunda prueba (`docs/spec/qa/2026-10-26-segunda-prueba.md`), soporte en grilla del estante de S4. Hechos
    (2026-10-26): QA10, QA11 y QA12; QA13 el 2026-10-27, con la regla 8 de `walls_extra.md` (perímetros en voladizo,
    `walls.detect_overhangs`); el cordón redondo de los puentes, con `speed.bridges`, el 2026-10-27 (paso 2):
    - QA10: la zona de soporte unos 1,6 mm más allá de lo que cuelga (`supports_skirt.md`, regla 2); hoy termina
      0,15 mm adentro, y en piezas orgánicas sale en pedazos sueltos. La más importante;
    - QA11: la separación XY medida desde la cara de la pieza (el eje de la línea a 0,425 mm de la cara, no a 0,8);
    - QA12: la base en líneas paralelas a `support.line_angle` en todas las capas, cada `support.spacing` más un
      cordón (2,857 mm);
    - [x] QA13: la primera capa sobre el soporte, con la altura de capa más la separación y los perímetros como
      `;TYPE:Overhang perimeter` (regla 4; hecho 2026-10-27);
  - de K1 (`cooling.md`) e I1 (`surfaces_infill.md`): la primera capa sólida sobre el relleno ralo es un puente
    (regla 6). Hecho (2026-10-26): solo la capa de arriba de todo es «Top solid infill» (regla 2);
  - `cooling.enabled`
- [ ] Paso 2 de lo pendiente para integrar (orden aprobado por el dueño, 2026-10-27):
  - [x] `speeds_accels.md` (hecho 2026-10-27, salvo lo que espera a otras fichas: V1 al relleno de huecos, el tope de
    las aceleraciones por los límites de la máquina a `gcode_firmware.md`, las de la balsa y la torre de purga a esas
    funciones; V1 hecho con el relleno de huecos): `speed.bridges` (y el cordón redondo de los puentes y de los
    perímetros en voladizo, `gcode_motion.md` regla 1, con `extrusion.bridge_flow_ratio`, regla 14), `speed.gap_fill`,
    `speed.over_bridges`, `speed.first_layer_infill`, `speed.travel_z` y los topes del relleno por filamento
    (`extrusion.infill_max_speed` y `_crossing_speed`): reglas 1 a 5 y 14, pruebas V2 a V5, V10 y M2. También
    (2026-10-27) las velocidades dinámicas en voladizos (6 y 7, V6) y las aceleraciones por tipo (10 a 13, V8 y V9;
    sin el tope de los límites de la máquina, que espera a `gcode_firmware.md`, ni las de la balsa y la torre de
    purga), y el ecualizador del caudal (8 y 9, V7);
  - [x] relleno de huecos (`walls_extra.md`, `walls.generator` `fixed` y `walls.gap_fill`, W6 y V1; hecho
    2026-10-27). El relleno concéntrico del motor ya es de ancho fijo (regla 1, último punto);
  - [ ] el resto de `walls_extra.md`: hechos (2026-10-27) las costuras internas escalonadas (regla 3, W5), los
    perímetros extra en voladizos (regla 5, W3), un solo perímetro arriba (regla 10, W7) y en la primera capa (regla
    11, W8); `walls.extra_if_needed` (regla 6, Tisma Slicer no agregó ninguno) y `walls.avoid_curled_overhangs` (regla
    9, sin medir) se leen y se guardan. Falta el espesor de la cáscara vertical (regla 7, W4), con una pregunta al
    equipo de fichas (PROGRESS, 2026-10-27);
  - [x] planchado (`ironing.md`, IR1; hecho 2026-10-27);
  - [x] balsa (`skirt_brim_raft.md`, reglas 3 y 9 a 14, R1 a R9; hecho 2026-10-27), y el resto de la ficha (hecho
    2026-10-27): largo mínimo de falda (B1), escudo contra corrientes (B3), tipos de borde y su separación (B4); B2 ya;
  - [x] cambio de color y pausas (`gcode_templates.md`, reglas 11 y 13; hecho 2026-10-27): `gcode.color_change`,
    `gcode.pause`, `gcode.before_layer`, `gcode.after_layer` y `color_change`/`pause` en `changes`; y el lenguaje de
    plantillas (reglas 1 a 10, 12 y 15 a 17; hecho 2026-10-27). Faltan ponerlos desde la vista previa (regla 13, con
    la interfaz P1), los parámetros propios de cada perfil (regla 14, su prefijo sale de `presets.md`), el nombre del
    modelo en la interfaz (`input_filename_base`) y los scripts de posproceso desde la interfaz;
  - [ ] los extras del G-code (`gcode_extras.md`): hechos (2026-10-27) el peso y el costo al final (regla 1, X1), la
    cámara con espera (regla 3, X2), el desfase de Z (regla 5, X4), la E volumétrica (regla 11, X11), la contracción
    (regla 4, X3), los comentarios detallados (regla 12, X12), los dialectos antiguos (regla 6, X5), los arcos
    (regla 10, X10), el modo silencioso (regla 7, X6) y el G-code binario (regla 9, X8, sin comprimir). Faltan las
    miniaturas (X7, con la vista 3D de la interfaz), la compresión y MeatPack del binario, y confirmar con una caja
    negra qué cubre el CRC32 de cada bloque (se usó encabezado + parámetros + datos)
  - [x] el firmware (`gcode_firmware.md`; hecho 2026-10-27): temperaturas al empezar (regla 1), límites de la máquina
    con `machine.limits_usage` (regla 2; también en la estimación, `print_time.md` reglas 2 y 3, y el tope de las
    aceleraciones, V9), `G92 E0` con E relativa en Marlin (regla 3), `M73` por tiempo (regla 5), el tiempo por modo y
    de la primera capa (regla 6), sabores `marlin_legacy` y `reprapfirmware`, y las etiquetas de objetos (regla 4,
    `gcode.object_labels`, FW1, FW3, FW6 y FW8; hechas 2026-10-27)
  - [x] impresión secuencial (`sequential.md`, reglas 1 a 5, SQ1 a SQ4; hecha 2026-10-27): los objetos de la placa
    (o las copias de `slice --copies`) se laminan aparte y se imprimen uno por uno, con el paso de uno a otro de la
    regla 3, `gcode.between_objects`, la separación del cabezal al acomodar, y los objetos que chocarían se rechazan.
    En la interfaz, la opción en Otros › Modos especiales y el acomodo con el radio del cabezal
  - [x] retracción y viajes completos (`retraction_travel.md`, reglas 1 a 10, R1 a R11; hecho 2026-10-27): la vuelta
    con su velocidad y su extra, retraer en todo cambio de capa, el barrido, las alturas del salto, el salto en rampa,
    la retracción del firmware (`G10`/`G11`), los viajes rectos o con desvío máximo, los lazos en sentido horario y
    los valores propios del material. Faltan la rampa más empinada ante obstáculos (regla 6, la ficha no la midió) y
    editar los valores del material en la interfaz (hoy se ven; se editan en el archivo)
- [ ] Punto 4 de lo pendiente para integrar (orden aprobado por el dueño, 2026-10-27): varios extrusores y materiales
  (`multi_extruder.md`, `multimaterial.md`), después F7 no planar y resina:
  - [x] 4a, el núcleo de varios extrusores (`multi_extruder.md`, reglas 1 a 8 y 16; ME1 a ME5 y ME10; hecho
    2026-10-27): `machine.extruders` y `machine.extruder_offset`, el extrusor de cada objeto y de cada tipo de
    extrusión (`extruders.*`), un cambio por extrusor y por capa empezando con el último, la secuencia del cambio
    (`retraction.toolchange_length` y `toolchange_restart_extra`, `gcode.toolchange`, `T<n>`), la espera con menos
    temperatura, la cama por extrusor y las cáscaras entre materiales (`surfaces.interface_shells`). Falta asignar el
    extrusor de cada objeto desde la línea de comandos y la interfaz, y los colores pintados (regla 1, con
    `model_tools.md`)
  - [ ] 4b, la torre de purga (reglas 9 a 14, ME6 a ME8): hechos (2026-10-27) la torre con su contorno, un bloque por
    cambio (purga con MMU, cebado con varias boquillas), relleno denso abajo y en columnas arriba, borde, giro,
    separación y flujo de sus líneas, el multiplicador de purga, el cambio sobre la torre y los errores (E absoluta,
    fuera de la cama, sobre la pieza), sin capas ralas (regla 11, la boquilla baja a la torre) y la purga en el
    relleno o el objeto (regla 13, ME8). Faltan el cono (regla 14, no medido) y marcar los objetos en la interfaz
  - [x] 4c, la MMU de un extrusor (regla 15, ME9; hecho 2026-10-27): compresión, descarga, enfriamiento, carga y
    purga sobre la torre, con el grupo `mmu` y los ajustes del filamento. Falta la corriente del motor durante el
    cambio (se lee)
  - [x] 4d, las vigas de entrelazado (regla 17, ME11; hecho 2026-10-27). Las regiones pintadas (regla 18) esperan a
    los colores pintados (`model_tools.md`): sus claves se leen
  - [ ] 4e, el soporte sincronizado y la altura máxima por extrusor (reglas 19 y 20, ME12): el motor ya imprime
    siempre el soporte en las capas de la pieza (lo sincronizado; `support.synchronize_layers` se lee). Faltan las
    capas propias del soporte, más gruesas (hasta `machine.max_layer_height`, o el 75 % de la boquilla), que ME12
    mide sin sincronizar
  - [ ] 4f, `multimaterial.md` (MM1 a MM6) y `color_mixing.md`: hechos (2026-10-27) las boquillas distintas
    (`machine.nozzle_diameter` como lista, el 75 % de cada una, `multi.fine_walls_coarse_infill` con el relleno
    combinado de la gruesa, los anchos por boquilla), `multi.color_mixing` con su aviso de costo en la interfaz y el
    aviso de purga después de laminar (MM1 a MM6); y `color_mixing.md` (2026-10-27): las mezclas del proyecto, su
    ciclo de capas, los degradés, el proyecto, el extrusor de cada objeto en la interfaz y el diálogo de mezclas
    (FS1 a FS14). Falta leer y escribir las mezclas y el extrusor de cada objeto en el 3MF (el motor no escribe 3MF ni
    lee los objetos de uno por separado) y en la línea de comandos
  - [ ] F7 no planar (la base, `surface`, `overhangs`, el cono según la pieza y `automatic` hechos, 2026-10-27;
    falta `stress`, con el análisis estructural); resina (`resin.md`, `resin_supports.md`), en tres partes:
    - [x] R1, el núcleo (`resin.md`, reglas 1 a 12 y 20 a 22, RE1 a RE10; hecho 2026-10-27): los ajustes
      (`machine.display_*`, grupo `resin`), las imágenes con el área exacta de cada píxel, el pie de elefante, las
      correcciones de escala, absoluta y de Z, la gamma, el archivo GOO (escritura y lectura) y la salida `.goo` de la
      línea de comandos, con sus tarjetas en el panel
    - [x] R2, las ventosas (reglas 13 a 15, VE1 a VE9) y las calibraciones de resina (reglas 16 a 19, CR1; hecho
      2026-10-27): el aviso de ventosas en el JSON de avisos; la torre de exposición y el bloque de medidas en el
      motor, todavía sin menú en la interfaz
    - [ ] R3, los soportes de resina (`resin_supports.md`) y la resina en la interfaz (laminar, vista previa de las
      imágenes, exportar `.goo`): hechos (2026-10-27) los soportes (puntos, puntas, pilares, aterrizaje sobre la
      pieza), la base (también alrededor de la pieza), el vaciado con su redondeo, las barras interiores y los
      drenajes en el motor, con sus tarjetas en el panel (RS1 a RS4); y la resina en la interfaz
      (`machine.technology`): el panel con solo los ajustes de resina, laminar, la vista previa con los contornos de
      cada capa, exportar `.goo` y las dos calibraciones de resina en Calibrar. Faltan las uniones entre pilares a
      `resin.support_critical_angle` (los pilares bajan rectos) y poner los drenajes desde la interfaz
- [ ] `preview.md`, `preferences.md`, `formats.md` y `model_tools.md` (reglas 16 a 30), ampliadas
- [ ] `printer_connections.md` (F9), `cloud.md` (la línea de comandos del nodo, reglas 18 a 21), `mobile_app.md`
  (la API de C) e `INTEGRATION.md`
- [ ] Medición del equipo de fichas (2026-10-26, `dev` d3e814a contra Tisma Slicer, caja negra, cada uno con sus
  ajustes): en las piezas grandes el motor tarda 2 a 3 veces más (Benchy 7,0 s contra 3,3 s; Benchy al doble con
  gyroid 17,8 contra 5,8 s) y su G-code pesa hasta 3,3 veces más (Benchy 7,57 MB contra 2,29 MB). Cuenta para el
  servidor (`model_library.md`, reglas 16 y 17). Para un bloque de mantenimiento: perfilar el laminado del Benchy y
  ver qué sobra en el G-code (comentarios `;WIDTH`/`;HEIGHT`, decimales de E, puntos casi en línea). Lo que está
  bien: dos laminados iguales dan el mismo archivo byte a byte
- [ ] **Eje medio con varias islas** (encontrado 2026-10-27, para un bloque de mantenimiento): `layer_walls` sobre
  tres islas juntas (una banda en «C» de 0,4 mm abierta a la derecha y dos rectángulos de 0,3 × 0,7 mm frente a la
  abertura, uno arriba y otro abajo) da unos 140 cordones paralelos muy afuera de la región, a lo largo de la
  bisectriz abierta entre los dos rectángulos, como si estuviera adentro. Cada isla sola, o dos de ellas, da bien.
  Se reproduce con el Benchy local, `walls.generator` `fixed`, en la capa de Z 33,6 si el relleno de huecos se hace
  de una vez (hoy va isla por isla, por esto); una figura sintética parecida no lo reproduce (20 000 variantes al
  azar), así que depende de los números exactos. El dato no se sube: sale del Benchy, que es solo para medir. Las
  paredes variables de siempre pasan por el mismo eje medio con regiones de varias islas: revisar la clasificación
  de las aristas infinitas del diagrama (`src/geom/medial_axis.cpp`) y si conviene ir isla por isla en `layer_walls`

### Vista con Vulkan (decisión del dueño, 2026-10-26)

Pedido: más FPS con modelos grandes (LOD, «tipo raycaster»), hasta 60 FPS, y «la idea es que el motor funcione en
Vulkan». Lo estudiado y medido, en `docs/LARGE_MODELS.md`. Hecho con OpenGL (2026-10-26): no dibujar lo que la pieza
tapa, el tiempo de la vista en la barra de estado y `--bench-frames`. Pasos:
- [x] V1: la ventana, la cadena de imágenes (swapchain) y Dear ImGui con Vulkan (volk y GLFW, como el cómputo);
  OpenGL queda de respaldo donde no hay Vulkan 1.1 (hecho 2026-10-26: `--graphics`, `apps/tisma/vk_window.*`, la
  prueba `ui_vulkan_window`). Cuando la vista Vulkan haga todo lo que hace la de OpenGL (V3: la oclusión ambiental es
  el modo por defecto), `kVulkanViewReady` pasa a verdadero y el modo automático elige Vulkan primero (pedido del
  dueño: detectar solo si hay Vulkan y si no ir a OpenGL)
- [x] V2: la vista 3D en Vulkan (cama, modelo, pintura, cordones instanciados, MSAA, FXAA, capturas), comparada
  imagen contra imagen con la de OpenGL en la CI (lavapipe y llvmpipe). Hecho 2026-10-26: `apps/tisma/vk_view.*`,
  los shaders en `apps/tisma/shaders/`, `--compare-view` y las pruebas `ui_view_*`; el tiempo de la GPU por marcas
  de tiempo en la barra de estado y `--bench-frames`. Con la luz rápida: la oclusión y los rayos vienen en V3
- [ ] V3: oclusión ambiental y ray tracing con la escena en buffers (sin el tope de 4 millones de primitivas). Antes,
  mejorar la oclusión y el color de la vista previa (pedido del dueño, 2026-10-26: «como lo hace Prusa 3.0»; la
  referencia son los videos y las imágenes del lanzamiento, mirados como caja negra, nunca su código)
- [ ] V4: el recorte en la GPU: un shader de cómputo por cuadro elige por capa lo que se ve (cámara, lo tapado, capas
  juntas de lejos) y dibuja con `vkCmdDrawIndirect`; menos vértices por cordón
- [ ] V5: rayos primarios para la vista previa («tipo raycaster»), en cómputo
- [ ] V6: malla simplificada del modelo mientras se mueve (Garland y Heckbert 1997)
- [ ] Greedy meshing en la vista previa (pedido del dueño, 2026-10-26): juntar los cordones vecinos del mismo tipo en
  menos instancias, como el mallado voraz junta las caras de los vóxeles (`docs/LARGE_MODELS.md`)
- [ ] Armar los datos de la vista previa en el hilo del laminado (hoy, 120 a 180 ms en el de la interfaz)

Con el PR #24 (2026-10-26):
- [ ] `step_faces.md` (STEP con caras: calidad al abrir, la cara de cada triángulo, pintar y cargas por cara, volver a
  teselar), `problem_reports.md` (informar un problema) y `thingiverse.md` (el sitio dentro del programa; su API
  espera rutas confirmadas, regla 20)

### Velocidad del laminado (medición de caja negra del equipo de specs, 2026-10-26)

El dueño quiere alcanzar o superar el ritmo de Tisma Slicer. El equipo de specs midió el motor como caja negra
(`docs/spec/qa/2026-10-26-velocidad.md`, PR #24 y #25): el Benchy con capas sólidas de abajo tardaba unos 6 s, contra
0,9 s sin ellas; el costo crecía mucho más rápido que el detalle del contorno y con la cantidad de paredes.
- [x] El paso de las superficies de abajo (hecho 2026-10-26): era la detección de puentes. El área de los cordones de
  la capa de abajo unía un disco de 64 lados por punto; ahora cada recorrido es un offset variable de Clipper2, y lo
  impreso se calcula solo en una ventana alrededor de la superficie inferior, simplificado a 1 µm. Metas de
  `docs/spec/BENCHMARKS.md` medidas en el contenedor (4 núcleos, mediana de 5): RV1 Benchy 6,3 → 1,39 s (meta 1,5 s);
  RV2 las capas de abajo suman 8 % con tolerancia 0 (antes 200 s contra 7,5 s) y 22 % con 0,005 (meta 25 %); RV3
  suman de 0,17 a 0,25 s con 1 a 6 paredes (meta 0,3 s). RV6: el G-code cambia en una sola capa del Benchy (ver
  `PROGRESS.md`). Faltan RV4 (V10) y RV5 (V11)
- [ ] V3: `;WIDTH` solo cuando cambia el ancho (hoy, 21 % del archivo)
- [ ] V4: juntar los tramos cortos de las curvas dentro de `gcode.resolution` (el motor hace el doble de movimientos)
- [x] V5: armar el G-code de cada capa en paralelo y unirlo en orden (hecho 2026-10-26). Cada capa se planea (orden y
  costuras, que siguen de la capa de abajo) y se escribe por separado, en un pipeline de oneTBB: mientras se escribe
  una capa se planean las siguientes, y los recorridos se simplifican en paralelo. La verificación lee el texto en
  bloques en paralelo. Las costuras y la simplificación sacan la raíz solo cuando hace falta, y X, Y y el ancho se
  formatean una vez. El G-code es el mismo byte a byte en todas las piezas de medición
- [x] V7 (hecho con lo de arriba: el paso de las capas de abajo ya no crece más que los vértices; RV2 se cumple). V8
  no hace falta: las fichas piden el resultado, no el algoritmo, y RV1 a RV3 se cumplen
- [x] V11, relaminar por etapas (hecho 2026-10-26): el programa guarda el último corte; si solo cambian ajustes que
  lee el G-code (velocidades, límites de movimiento, enfriamiento y ventilador, retracción, temperaturas, flujo, el
  G-code de la impresora), rehace solo el G-code, su verificación y la vista previa. Una prueba compara byte a byte,
  ajuste por ajuste, contra un laminado entero (RV5). Benchy: 0,41 s contra 1,35 s entero (meta 0,5 s). Para
  llegar, los planificadores de viajes de cada capa se arman en paralelo, y la verificación y la vista previa a la vez
- [x] V10: reusar una capa igual a la de abajo (RV4, hecho 2026-10-26 con V5):
  - las capas que cruzan las mismas aristas que la de abajo (ningún vértice entre las dos) no se cortan de nuevo:
    sus puntos salen de las aristas de la primera de la tanda, iguales bit a bit;
  - si además la sección es la misma en x e y (aristas verticales, o caras verticales como los lados de un prisma
    partidos por diagonales), el corte las deja vacías y el laminado copia la región y las paredes de la capa de
    abajo;
  - se reusan, con el mismo resultado byte a byte: las superficies (si la región de relleno y las capas que miran
    son las de la capa de abajo), el relleno (si las superficies son las de la capa a un período del patrón) y el
    planificador de viajes y las islas del G-code (si la región es la misma);
  - cilindro de 32 768 lados: 40 mm, 2,69 → 0,36 s; 80 mm, 5,33 → 0,63 s. La relación 80/40 bajó de 1,98 a 1,75;
    lo que crecía con las capas era la parte en serie del G-code. Con V5 y V9: 0,29 s y 0,36 s, relación 1,24 (meta
    1,25, mediana de 9; queda justo, el ruido de la máquina mueve unos 0,02);
  - RV6: el G-code del Benchy, la esfera, el tubo, las islas y las cajas sale igual byte a byte. En los cilindros
    cambia: cada capa repite la sección de la primera, en vez de simplificar en cada capa otros puntos de las
    diagonales. Las paredes nuevas quedan a menos de 0,018 mm de las de antes
- [x] V9: simplificar después de cada pared (hecho 2026-10-26): lo que dejan las paredes se simplifica con
  `slicing.simplify_tolerance`, como los contornos. Benchy 1,04 → 0,98 s (6 %); con 6 paredes, 3 %. El G-code del
  Benchy cambia en el relleno (menos del 1 % de largo); las paredes quedan iguales
- [x] RV10, costura alineada tan rápida como `nearest` (hecho 2026-10-26; `perimeters.md`, regla 8): las esquinas de
  cada lazo se buscaban caminando, desde cada punto, hasta el primero a 0,1 mm; en lazos de 11 000 puntos amontonados
  en las esquinas redondeadas eran decenas de pasos por punto. Ahora un puntero sobre el largo acumulado del lazo
  saltea los que no pueden estar tan lejos, con las mismas esquinas. Los candidatos de todos los lazos se buscan en
  una etapa en paralelo antes del plan, y en el plan solo queda elegir, capa tras capa. Los viajes también se planean
  en paralelo. Gota de 172 080 triángulos: alineada 3,81 → 2,68 s (1,41 → 0,96 veces `nearest`). El G-code es el mismo
  byte a byte. La GPU no rinde acá (ver `PROGRESS.md`)

## Avance

Cada bloque de trabajo anota en `PROGRESS.md`:
- la fecha;
- qué hizo;
- qué midió;
- qué sigue.
