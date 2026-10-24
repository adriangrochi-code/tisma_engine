# Plan del motor de laminado de Tisma

**Objetivo:** un motor de laminado propio, escrito desde cero (ver `CLEANROOM.md`), más rápido que el de Tisma Slicer
actual, que el dueño pueda licenciar como quiera.

**Visión (decisión del dueño, 2026-10-24):** laminado **no planar** y **de 5 ejes**, con **aceleración por GPU**. La
primera versión plana es la base: la malla, el corte, los perímetros, los rellenos y el G-code se reutilizan. Por eso
las reglas de «Diseño para la visión» valen desde F1.

**Primera versión:** FDM, una boquilla, capas planas, en unas 16 semanas dedicando un cuarto del tiempo.

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

Antes de mover algo a la GPU se mide con el banco de pruebas.

**Decisiones del dueño (2026-10-24):**
- Orden de la visión: F7 (no planar en 3 ejes) → F8 (GPU) → F9 (5 ejes).
- La API de GPU se elige en F8, con las mediciones del banco. Vulkan compute es la candidata.
- Pendiente: la máquina de 5 ejes de referencia para F9.

## Fases

| Fase | Semanas | Contenido | Terminada cuando |
|---|---|---|---|
| F0 | 1 | Esqueleto: CMake, CI (Linux y Windows en cada envío), Catch2, línea de comandos, lectura de STL (binario y ASCII), `THIRD_PARTY.md`, `PROVENANCE.md`, banco de pruebas | Compila y prueba en Linux y Windows; lee los modelos del banco |
| F1 | 2–3 | Corte de la malla en capas: intersección triángulo-plano, unión de segmentos en contornos, cierre de huecos chicos, alturas de capa (primera capa y resto), offsets con Clipper2 | Las áreas por capa coinciden con las de la malla (prueba con piezas de área conocida); tiempo medido |
| F2 | 4–7 | Perímetros de ancho variable desde Kuipers et al. 2020: esqueleto de los contornos (Voronoi de segmentos con Boost.Polygon), trapezoidación, cantidad y ancho de cordones (repartido y centrado), extracción de trayectorias | Paredes de 0,3 a 1,2 mm sin huecos ni excesos (área cubierta medida); piezas comunes con 2–3 perímetros |
| F3 | 8–9 | Superficies: arriba, abajo, sólido y relleno; rellenos rectilíneo, grilla y giroide; puentes con dirección | Un cubo y el Benchy con capas sólidas correctas |
| F4 | 10–12 | G-code: orden de islas, viajes que no cruzan perímetros, retracción, salto en Z, costura (alineada, más cercana, aleatoria), enfriamiento con tiempo mínimo de capa, velocidades, aceleraciones, límites de la máquina, Marlin y Klipper, estimación de tiempo | El Benchy se imprime en una impresora real |
| F5 | 13–15 | Soportes en grilla (voladizos por ángulo, proyección, interfaz y distancia), falda y borde | Una pieza con voladizos se imprime y el soporte sale |
| F6 | 16 | Integración: Tisma Slicer y Tisma Cloud llaman al motor; comparación de calidad y velocidad con Tisma Slicer | Medido y documentado |
| F7 | después de F6 | No planar en 3 ejes: capas apenas curvas cerca de la superficie superior (CurviSlicer, Ahlers et al.), con control de choques del cabezal | Una pieza con techo curvo se imprime sin escalones visibles en una impresora común; medido |
| F8 | después de F7 | GPU: mover a la GPU las fases que más pesan según el banco, con la ruta CPU como referencia. La API se elige acá: candidata Vulkan compute (NVIDIA, AMD e Intel sin instalar kits); CUDA es más simple pero solo NVIDIA | Más rápido que la ruta CPU en el banco, con el mismo resultado |
| F9 | después de F8 | 5 ejes: campo escalar en el volumen (malla de tetraedros), capas curvas como isosuperficies (Dai et al. 2018, S³-Slicer, Neural Slicer), orientación de la boquilla, cinemática de la máquina, choques y G-code con ejes giratorios | Una pieza con voladizos se imprime sin soportes en una máquina de 5 ejes (hace falta decidir cuál) |
| F10 | al final | macOS: CI en cada envío y arreglos para que compile y pase las pruebas (decisión del dueño, 2026-10-24: se deja para el final) | Compila y prueba en macOS |

## Estado de F0

- [x] CMake, C++20
- [x] Catch2
- [x] Línea de comandos (`slice` solo lee el modelo y responde «not implemented»; `info` imprime el resumen)
- [x] Lectura de STL binario y ASCII, con pruebas
- [x] CI en cada envío: Linux (gcc, clang, ASan+UBSan) y Windows (MSVC). macOS pasa a F10, al final
- [x] `THIRD_PARTY.md`, `PROVENANCE.md`
- [x] Comprobar que la CI pasa en Linux y Windows (run 4: también pasó macOS)
- [ ] Banco de pruebas: modelos reales (Benchy, conejo, tornillo), con licencia que permita guardarlos

## Avance

Cada bloque de trabajo anota en `PROGRESS.md`:
- la fecha;
- qué hizo;
- qué midió;
- qué sigue.
