# Sala limpia

El motor se escribe desde cero para que su licencia la decida el dueño del proyecto. Desde el 2026-10-24 el
repositorio es público bajo la PolyForm Noncommercial License 1.0.0: código disponible, solo uso no comercial (ver
`LICENSE` y `README.md`). Por eso no puede contener código, ni traducciones de código,
de programas con licencias copyleft. Estas reglas valen para cualquier persona o sesión que escriba en este
repositorio.

## Prohibido leer, copiar o traducir

El código fuente de:

- PrusaSlicer, Slic3r, SuperSlicer, OrcaSlicer, Bambu Studio, Creality Print, Elegoo Slicer, Anycubic Slicer y
  cualquier otro derivado de Slic3r;
- Cura y CuraEngine (de ahí vienen Arachne, Lightning y los soportes en árbol de Thomas Rahm);
- **Tisma Slicer** (`adriangrochi-code/Nuevo-slicer-basado-en-prusa-`), incluido lo que escribimos dentro de ese fork:
  es AGPL;
- ArcWelderLib, Marlin, Klipper, RepRapFirmware y cualquier otro código GPL o AGPL;
- el slicer no planar de Ahlers et al. (una rama GPLv3 de Slic3r);
- el código publicado junto con los artículos de laminado no planar y de varios ejes (por ejemplo S3_DeformFDM o
  CurviSlicer). Se trabaja solo con el artículo. Si alguno tuviera licencia permisiva y conviniera usarlo como
  biblioteca, primero lo decide el dueño y se anota en `THIRD_PARTY.md`.

Una sesión o persona que haya leído ese código para este trabajo no escribe código acá.

## Permitido

- **Artículos, tesis y libros.** Por ejemplo Kuipers et al. 2020, *A framework for adaptive width control of dense
  contour-parallel toolpaths in fused deposition modeling*, Computer-Aided Design 128:102907 (arXiv 2004.13497).
- **Especificaciones públicas de formatos:**
  - STL;
  - 3MF (3MF Consortium);
  - G-code, a partir de la documentación de usuario de RepRap, Marlin y Klipper, que describe el comportamiento y no
    es código.
- **Bibliotecas con licencias permisivas,** anotadas en `THIRD_PARTY.md` antes de usarlas:

  | Biblioteca | Licencia |
  |---|---|
  | Clipper2 | BSL-1.0 |
  | Boost, incluido Boost.Polygon Voronoi | BSL-1.0 |
  | oneTBB | Apache-2.0 |
  | Eigen | MPL-2.0 |
  | Catch2 | BSL-1.0 |
  | nlohmann/json | MIT |
  | miniz | MIT |
- **Comparar con otros slicers solo como caja negra:** ejecutar su programa y comparar el G-code, los tiempos o el
  filamento. Nunca abrir su código.

## Procedencia

- Cada módulo anota en `PROVENANCE.md` de dónde sale: el artículo y la sección, la especificación, o «diseño propio».
- La historia de commits es la prueba de procedencia: no se reescribe ni se aplasta.

## Antes de cambiar la licencia

Lo revisa un abogado. La PolyForm Noncommercial se adoptó el 2026-10-24 por decisión del dueño, al hacer público el
repositorio; la revisión de un abogado sigue pendiente. El modelo de lenguaje que escribe este código se entrenó con código público, incluidos esos
proyectos. Esto no se puede excluir del todo; por eso el motor se escribe a partir de las fuentes permitidas, con
estructura propia, y nunca reproduce código de memoria.
