# Sala limpia

El motor se escribe desde cero para que su licencia la decida el dueño del proyecto (más adelante, una licencia de
código disponible que solo permita el uso no comercial). Por eso no puede contener código, ni traducciones de código,
de programas con licencias copyleft. Estas reglas valen para cualquier persona o sesión que escriba en este
repositorio.

## Prohibido leer, copiar o traducir

El código fuente de:

- PrusaSlicer, Slic3r, SuperSlicer, OrcaSlicer, Bambu Studio, Creality Print, Elegoo Slicer, Anycubic Slicer y
  cualquier otro derivado de Slic3r;
- Cura y CuraEngine (de ahí vienen Arachne, Lightning y los soportes en árbol de Thomas Rahm);
- **Tisma Slicer** (`adriangrochi-code/Nuevo-slicer-basado-en-prusa-`): es AGPL. La única excepción es lo que ya
  está en este repositorio, en `imported/tisma/` y `docs/tisma/` (ver «Código propio» abajo); el resto del fork sigue
  prohibido, aunque lo hayamos escrito nosotros;
- ArcWelderLib, Marlin, Klipper, RepRapFirmware y cualquier otro código GPL o AGPL.

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
- **Las especificaciones de `docs/spec/`:** las escribió el equipo que conoce Tisma Slicer, sin código ni estructura
  copiada (sala limpia de dos equipos).
- **Buscar en la web** artículos, especificaciones y documentación de usuario (manuales, bases de conocimiento). Nunca
  código fuente de los programas prohibidos.
- **Comparar con otros slicers solo como caja negra:** ejecutar su programa y comparar el G-code, los tiempos o el
  filamento. Nunca abrir su código.

## Código propio

`imported/tisma/` tiene código que el equipo del dueño escribió en archivos nuevos de Tisma Slicer, sin código de
PrusaSlicer, y `docs/tisma/` sus documentos. El dueño tiene sus derechos y lo trae con la licencia de este repositorio.
Se puede leer, adaptar y copiar al motor; `imported/README.md` dice cómo se revisó y qué hay que reemplazar.

Usa tipos de PrusaSlicer (`TriangleMesh`, `Model`, `PrintConfig` ...). Se cambian por los del motor y **no se busca su
implementación**.

## Procedencia

- Cada módulo anota en `PROVENANCE.md` de dónde sale: el artículo y la sección, la especificación, o «diseño propio».
- La historia de commits es la prueba de procedencia: no se reescribe ni se aplasta.

## Antes de cambiar la licencia

Lo revisa un abogado. El modelo de lenguaje que escribe este código se entrenó con código público, incluidos esos
proyectos. Esto no se puede excluir del todo; por eso el motor se escribe a partir de las fuentes permitidas, con
estructura propia, y nunca reproduce código de memoria.
