# Código propio traído de Tisma Slicer

## Qué es

Código que escribió el equipo del dueño dentro de Tisma Slicer, en archivos **nuevos**, que no vienen de PrusaSlicer.
El dueño tiene sus derechos de autor y lo trae acá con la licencia de este repositorio (`LICENSE`, todos los derechos
reservados). En Tisma Slicer sigue publicado con la AGPL; las dos licencias conviven porque el autor es el mismo.

- **Origen:** `adriangrochi-code/Nuevo-slicer-basado-en-prusa-`, rama `main`, commit `dee3ea9` (24-10-2026).
- **Rutas:** las mismas de Tisma Slicer, debajo de `imported/tisma/`.
- **Cambios al copiar:**
  - la línea de licencia AGPL del encabezado se cambió por el aviso de este repositorio (o se agregó, si no había);
  - nada más.

## Cómo se usa

- Es una **base para adaptar**, no una biblioteca para enlazar: usa tipos de PrusaSlicer (lista abajo) que el motor
  no tiene ni puede tener.
- Al pasar un módulo a `src/`:
  - se cambian esos tipos por los del motor;
  - se anota en `PROVENANCE.md`: «propio, de `imported/tisma/<ruta>`».
- Las pruebas (`tests/`, `src/libtisma_fea/test/`) dicen qué resultados se esperan. Usan ayudas de prueba de
  PrusaSlicer (`test_data.hpp` y otras): se cambian por las del motor.

## Tipos de PrusaSlicer que hay que reemplazar

Se ven sus nombres y cómo se llaman; **no se busca su implementación**: es AGPL y está prohibida (`CLEANROOM.md`).
Para cada uno, el motor tiene o tendrá el suyo:

| De PrusaSlicer | Qué es (lo que hace falta saber) | En el motor |
|---|---|---|
| `TriangleMesh`, `indexed_triangle_set`, `admesh/stl.h` | Malla de triángulos indexada | Malla propia (F0) |
| `Point`, `Vec2d`, `Vec3d`, `Vec3f`, `Transform3d` | Vectores y transformaciones de Eigen con otros nombres | Eigen directamente |
| `BoundingBox`, `BoundingBoxf3` | Caja que contiene los puntos | Propia |
| `Polygon`, `ExPolygon`, `ClipperUtils` | Contornos y operaciones con polígonos | Clipper2 |
| `slice_mesh`, `TriangleMeshSlicer` | Corte de la malla en una Z | Corte propio (F1) |
| `AABBTreeIndirect` | Árbol de cajas para rayos y distancias a la malla | Propio, o Embree/nanoflann si su licencia lo permite |
| `Model`, `ModelObject`, `ModelVolume`, `ModelInstance` | El proyecto: objetos, sus partes y dónde están en la cama | Propio |
| `DynamicPrintConfig`, `PrintConfig`, `PrintObjectConfig` | Los ajustes | El JSON de `docs/spec/SETTINGS_V1.md` |
| `Print`, `PrintObject`, `Layer`, `SLAPrint` | El laminado en curso y sus capas | El laminado del motor |
| `GCodeReader` | Lee líneas de G-code: comando y ejes | Propio y chico |
| `load_3mf`, `store_3mf`, `Format/STEP.hpp` | Leer y guardar 3MF, leer STEP | 3MF desde su especificación; STEP con OCCT (LGPL con excepción: revisar antes) |
| `TriangleSelector` | Pintura de caras de la malla (soportes, costura) | Propio, cuando haga falta |
| `OpenVDBUtils`, `CutUtils`, `Geometry/ConvexHull` | Mallas por vóxeles, cortes con conectores, envolvente convexa | Bibliotecas permisivas u código propio |

## Qué hay

| Ruta | Qué hace | Para qué fase | Documento |
|---|---|---|---|
| `src/libtisma_fea/` | Análisis estructural (FEA) por vóxeles: materiales, cargas, solver con multigrid y GPU (Vulkan), relleno mínimo, refuerzos, lattice, orientación, capas por tensiones; análisis aerodinámico (CFD, LBM) | Después de la primera versión (F7) | `docs/tisma/FEA.md`, `docs/tisma/AERO.md` |
| `src/libslic3r/Engineering.hpp` | Datos del análisis guardados en el proyecto: cargas, zonas, límites, campo de capas | F7 | `docs/tisma/FEA.md` |
| `src/libslic3r/NonPlanar.*`, `NonPlanarFit.cpp` | Capas no planares: deformar la malla, laminar plano y volver; campos de capas (voladizos, tensiones, superficie, cono radial); límites del cabezal | Después de la primera versión | `docs/tisma/NONPLANAR_ANALYSIS.md` |
| `src/libslic3r/GCodeMoveLine.hpp` | Leer y escribir líneas G0/G1 para los filtros que cambian la Z | F4 y no planar | — |
| `src/libslic3r/ZAntiAliasing.*` | Z anti-aliasing: bajar las extrusiones visibles a la superficie inclinada | Después de F4 | `docs/tisma/Z_ANTIALIASING.md` |
| `src/libslic3r/InterlockInfill.*` | Relleno ondulado en Z que encastra entre capas | Después de F3 | `docs/tisma/EXISTING_FEATURES.md` |
| `src/libslic3r/BeltPrinter.*` | Impresoras de cinta: girar la pieza y pasar el G-code a los ejes de la máquina | Después de la primera versión | `docs/tisma/BELT_PRINTER.md` |
| `src/libslic3r/Fragment.*` | Partir una pieza más grande que la impresora, con encastres | Herramienta aparte | `docs/tisma/FRAGMENT.md` |
| `src/libslic3r/DimensionalCorrection.*` | Corrección de medidas desde un bloque de prueba medido | Calibración | `docs/tisma/EXISTING_FEATURES.md` |
| `src/libslic3r/CadSource.*` | Origen B-Rep de las mallas que vienen de STEP | Más adelante | `docs/tisma/EXISTING_FEATURES.md` |
| `src/libslic3r/SLA/InteriorInfill.*`, `SLA/SuctionCups.*` | Resina: relleno interior de piezas huecas y detección de ventosas | Resina | `docs/tisma/RESINA.md` |
| `tests/` | Pruebas de esas funciones y de otras que en Tisma están dentro de archivos de PrusaSlicer (arc overhangs, relleno denso, perímetros escalonados): sirven como especificación | La de cada función | — |
| `tools/research/pressure_advance_sim.py` | Simulación de pressure advance hecho en el G-code, para firmwares viejos | F4 | `docs/tisma/FIRMWARE_FEATURES_IN_GCODE.md` |
| `tools/gpu/compile_fea_shaders.py` | Compila los shaders del FEA a SPIR-V | F7 | — |

## Documentos (`docs/tisma/`)

Los escribió el mismo equipo. Describen qué hacen las funciones de Tisma y por qué; algunos nombran funciones de
PrusaSlicer, para decir dónde se engancha algo en Tisma Slicer. Esos nombres no se buscan.

- `EXISTING_FEATURES.md`: todo lo que agregó Tisma, con sus ajustes.
- `FEA.md` y `AERO.md`: el análisis estructural y el aerodinámico (método, validación con CalculiX, límites).
- `NONPLANAR_ANALYSIS.md`, `Z_ANTIALIASING.md`, `BELT_PRINTER.md`, `FRAGMENT.md`, `ORIENTATION.md`, `RESINA.md`,
  `BRICK_LAYERS.md` (con la revisión de la patente US11331848B2), `FIRMWARE_FEATURES_IN_GCODE.md`.

## Cómo se revisó

1. **Archivos nuevos:** solo los que no existen en PrusaSlicer 2.9.6 (etiqueta `upstream_2.9.6`).
2. **Sin código compartido:** se compararon de a 4 líneas seguidas (sin espacios ni comentarios) con todo el código de
   PrusaSlicer 2.9.6 y de PrusaSlicer 3.0.0-alpha12. Una sola coincidencia, en `GooSLA.cpp` (ver abajo).
3. **Sin fuentes de código ajeno:** se leyeron los encabezados y las notas de procedencia de cada archivo.

Quedaron afuera:

| Qué | Por qué |
|---|---|
| Archivos de PrusaSlicer que Tisma modificó (enfriamiento, soportes, perímetros, G-code y muchos más) | Son de PrusaSlicer con cambios nuestros: AGPL. Lo que hacen está en `docs/tisma/EXISTING_FEATURES.md` y en `docs/spec/BEHAVIOR.md` |
| `Format/GooSLA.*` (formato de resina de Elegoo) y su prueba | Una función sigue el patrón de los archivos SLA de PrusaSlicer, y el formato se contrastó con el lector de UVtools (AGPL). Se reescribe desde la especificación de Elegoo ([elegooofficial/GOO](https://github.com/elegooofficial/GOO)) |
| `Format/BambuProject.*`, `BedType.*`, `ModelPlate.*` | Se escribieron mirando el código de OrcaSlicer (AGPL) |
| Perfiles `Orca_*` | Vienen de OrcaSlicer |
| Interfaz, servidor, conexión con impresoras, app móvil | No son parte del motor de laminado |

**Revisar antes de cambiar la licencia:**
- `BeltPrinter.*`: sigue el método de Cura BlackBelt (LGPLv3), reimplementado sin copiar código (según
  `docs/tisma/BELT_PRINTER.md`).
- `NonPlanar.*`:
  - toma la idea del cono radial de Joshua Bird, sin usar su código;
  - la formulación de capas curvas en 3 ejes viene del artículo de CurviSlicer (Etienne et al. 2019), sin su código.
- `src/libtisma_fea/test/calculix/`: llama a CalculiX (GPL-2) como programa externo, para obtener resultados de
  referencia. Usar sus resultados no es usar su código.
