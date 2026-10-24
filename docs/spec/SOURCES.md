# Fuentes públicas permitidas

Se pueden leer y usar como base. Hay que citarlas en `PROVENANCE.md` cuando se usan.

También se puede buscar en la web más artículos, especificaciones y documentación de usuario: manuales y bases de
conocimiento de los slicers. Nunca su código fuente (ver `CLEANROOM.md`).

## Trayectorias y geometría

| Fuente | Para qué |
|---|---|
| Kuipers, Doubrovski, Wu, Wang. *A framework for adaptive width control of dense contour-parallel toolpaths in fused deposition modeling.* Computer-Aided Design 128 (2020) 102907. [TU Delft](https://research.tudelft.nl/en/publications/a-framework-for-adaptive-width-control-of-dense-contour-parallel-), arXiv 2004.13497 | Perímetros de ancho variable (fase F2): esqueleto de la sección, trapezoidación, cantidad y ancho de los cordones, compensación de presión |
| [Boost.Polygon Voronoi](https://www.boost.org/doc/libs/1_55_0/libs/polygon/doc/voronoi_main.htm) (BSL-1.0) | Diagrama de Voronoi de puntos y segmentos con coordenadas enteras, entrada del esqueleto |
| [Clipper2](https://github.com/AngusJohnson/Clipper2) (BSL-1.0) | Uniones, diferencias, intersecciones y offsets de polígonos |
| Douglas y Peucker (1973), simplificación de polilíneas | Reducir puntos de contornos y trayectorias con una tolerancia |
| Superficie giroide de Schoen: sin x·cos y + sin y·cos z + sin z·cos x = 0 | Relleno giroide (fase F3) |

## Soportes

| Fuente | Para qué |
|---|---|
| Vanek, Garcia Galicia, Benes. *Clever Support: Efficient Support Structure Generation for Digital Fabrication.* Computer Graphics Forum 33(5), 2014. [PDF](https://www.cs.purdue.edu/homes/bbenes/papers/Vanek14SGP.pdf) | Soportes en árbol (después de la primera versión) |

## Capas no planares y varios ejes (después de la primera versión)

| Fuente | Para qué |
|---|---|
| Ahlers, *3D Printing of Nonplanar Layers for Smooth Surface Generation*, tesis, Universidad de Hamburgo, 2018. [PDF](https://tams.informatik.uni-hamburg.de/publications/2018/MSc_Daniel_Ahlers.pdf) | Superficies superiores no planares; límites por el cabezal |
| Dai et al., *Support-free volume printing by multi-axis motion*, ACM TOG 37(4), 2018. [TU Delft](https://research.tudelft.nl/en/publications/support-free-volume-printing-by-multi-axis-motion) | Capas curvas sin soportes |
| Etienne, Ray, Panozzo, Hornus, Wang, Martínez, McMains, Alexa, Wyvill, Lefebvre. *CurviSlicer: Slightly curved slicing for 3-axis printers.* ACM Transactions on Graphics 38(4), SIGGRAPH 2019. [HAL](https://hal.science/hal-02120033) | Capas apenas curvas en 3 ejes, con límites de pendiente y espesor |
| Felzenszwalb y Huttenlocher. *Distance Transforms of Sampled Functions.* Theory of Computing 8, 2012. [Artículo](https://theoryofcomputing.org/articles/v008a019/) | Distancia exacta en una grilla (zonas bajo los voladizos) |
| Zhang et al., *S³-Slicer*, SIGGRAPH Asia 2022. [ACM SIGGRAPH](https://history.siggraph.org/?p=216073) | Laminado curvo con varios objetivos |
| Zhang et al., *Support Generation for Robot-Assisted 3D Printing with Curved Layers*, ICRA 2023. [arXiv](https://arxiv.org/pdf/2302.05510) | Soportes para capas curvas |
| Estudio de acabado con capas curvas (ScienceDirect, 2025). [Artículo](https://www.sciencedirect.com/science/article/pii/S2590123025043178) | Las capas curvas mejoran el acabado hasta unos 55° de inclinación |

## Formatos

| Fuente | Para qué |
|---|---|
| [Especificación 3MF](https://3mf.io/spec/) (3MF Consortium, ISO/IEC 25422:2025) | Leer proyectos 3MF |
| STL binario y ASCII (formato público) | Leer mallas |
| Elegoo, [«Goo Format Spec V1.2»](https://raw.githubusercontent.com/elegooofficial/GOO/main/Goo%20Format%20Spec%20V1.2.pdf) (PDF; el repositorio tiene licencia GPL v3, se lee solo el PDF) | Escribir archivos GOO para impresoras de resina Elegoo |
| Mango3D, [«Cupping»](https://doc.mango3d.io/?p=2272) | Ventosas en la impresión de resina |
| Documentación de G-code de [Marlin](https://marlinfw.org/docs/gcode/G092.html), [Klipper](https://www.klipper3d.org/G-Codes.html) y [RepRap](https://reprap.org/wiki/G-code) | Qué hace cada comando; se lee la documentación, no el firmware |
| Base de conocimiento de Prusa: [macros](https://help.prusa3d.com/article/macros_1775) y [lista de variables](https://help.prusa3d.com/article/list-of-placeholders_205643) | Sintaxis de las variables en el G-code de inicio y fin de los perfiles de Tisma (`GCODE_OUTPUT.md`) |

## Documentación de comportamiento (manuales, no código)

- [Wiki de OrcaSlicer](https://www.orcaslicer.com/wiki/print_settings/support/support_settings_advanced.html) y la base
  de conocimiento de Prusa: qué hace cada ajuste.
- Documentación de UltiMaker Cura: qué hace cada ajuste.
