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
  cinemática queda como módulo aparte, sin atarse a ninguna máquina.
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
- [ ] En GPU: voladizos y proyección, si conviene
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
- [ ] ZAA: partir donde el orden con un vecino se da vuelta, menos costuras (Sección 4.2), y medir en una impresión real
- [ ] Pedidos del dueño en cola (2026-10-25):
  - relleno optimizado para resistencia y rapidez (`docs/INFILL.md`), por etapas:
    - [x] combinar capas de relleno (ficha `infill_patterns.md`, reglas 18 a 20): −24 a −45 % de tiempo, el mismo
      material
    - [x] relleno denso bajo las superficies de arriba (ficha `dense_infill.md`)
    - [x] patrones triángulos y cúbico (ficha `infill_patterns.md`, reglas 4 y 6)
    - [ ] cúbico adaptativo y de soporte (reglas 7 y 8), y rayo (regla 9)
    - [x] relleno guiado por tensiones (fase A2): `infill.stress_density` con `slice --case`
    - [x] el caso de carga en el laminado de la interfaz (un `.json` como el de `analyze`)
    - [ ] editar el caso en la interfaz (apoyos y cargas), y la iteración analizar-reforzar
    - [ ] medir la rigidez con el análisis «como impresa» leyendo el laminado real
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
- [ ] Arc Overhang (pedido del dueño): techos planos en el aire sin soporte, con cordones que crecen en arcos desde el
  borde apoyado. En sala limpia: desde la descripción pública de la idea, con diseño propio, sin leer código GPL.
  Se prueba con el estante plano de la torre de temperatura
- [ ] El análisis completo desde la interfaz: apoyos, cargas, material y el mapa de seguridad sobre la pieza
- [ ] Mover, girar y escalar el modelo; varias piezas en la cama
- [ ] Perfiles de impresoras y filamentos (P3)
- [ ] Fichas de la interfaz de Tisma, cuando lleguen

## Avance

Cada bloque de trabajo anota en `PROGRESS.md`:
- la fecha;
- qué hizo;
- qué midió;
- qué sigue.
