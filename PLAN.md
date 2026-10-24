# Plan del motor de laminado de Tisma

**Objetivo:** un motor de laminado propio, escrito desde cero (ver `CLEANROOM.md`), más rápido que el de Tisma Slicer
actual, que el dueño pueda licenciar como quiera.

**Primera versión:** FDM, una boquilla, capas planas, en unas 16 semanas dedicando un cuarto del tiempo.

**Fuera de la primera versión:**
- multimaterial y torre de purga;
- soportes en árbol;
- capas no planares;
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

## Fases

| Fase | Semanas | Contenido | Terminada cuando |
|---|---|---|---|
| F0 | 1 | Esqueleto: CMake, CI (Linux en cada envío; Windows y macOS a mano y una vez por semana, porque en un repositorio privado sus minutos cuestan más), Catch2, línea de comandos, lectura de STL (binario y ASCII), `THIRD_PARTY.md`, `PROVENANCE.md`, banco de pruebas | Compila y prueba en los 3 sistemas; lee los modelos del banco |
| F1 | 2–3 | Corte de la malla en capas: intersección triángulo-plano, unión de segmentos en contornos, cierre de huecos chicos, alturas de capa (primera capa y resto), offsets con Clipper2 | Las áreas por capa coinciden con las de la malla (prueba con piezas de área conocida); tiempo medido |
| F2 | 4–7 | Perímetros de ancho variable desde Kuipers et al. 2020: esqueleto de los contornos (Voronoi de segmentos con Boost.Polygon), trapezoidación, cantidad y ancho de cordones (repartido y centrado), extracción de trayectorias | Paredes de 0,3 a 1,2 mm sin huecos ni excesos (área cubierta medida); piezas comunes con 2–3 perímetros |
| F3 | 8–9 | Superficies: arriba, abajo, sólido y relleno; rellenos rectilíneo, grilla y giroide; puentes con dirección | Un cubo y el Benchy con capas sólidas correctas |
| F4 | 10–12 | G-code: orden de islas, viajes que no cruzan perímetros, retracción, salto en Z, costura (alineada, más cercana, aleatoria), enfriamiento con tiempo mínimo de capa, velocidades, aceleraciones, límites de la máquina, Marlin y Klipper, estimación de tiempo | El Benchy se imprime en una impresora real |
| F5 | 13–15 | Soportes en grilla (voladizos por ángulo, proyección, interfaz y distancia), falda y borde | Una pieza con voladizos se imprime y el soporte sale |
| F6 | 16 | Integración: Tisma Slicer y Tisma Cloud llaman al motor; comparación de calidad y velocidad con Tisma Slicer | Medido y documentado |

## Estado de F0

- [x] CMake, C++20
- [x] Catch2
- [x] Línea de comandos (`slice` solo lee el modelo y responde «not implemented»; `info` imprime el resumen)
- [x] Lectura de STL binario y ASCII, con pruebas
- [x] CI: Linux en cada envío (gcc, clang, ASan+UBSan); Windows y macOS a mano y los lunes
- [x] `THIRD_PARTY.md`, `PROVENANCE.md`
- [ ] Comprobar la CI en Windows y macOS (lanzarla a mano una vez)
- [ ] Banco de pruebas: modelos reales (Benchy, conejo, tornillo), con licencia que permita guardarlos

## Avance

Cada bloque de trabajo anota en `PROGRESS.md`:
- la fecha;
- qué hizo;
- qué midió;
- qué sigue.
