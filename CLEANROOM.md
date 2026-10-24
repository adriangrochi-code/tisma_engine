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
- **Tisma Slicer** (`adriangrochi-code/Nuevo-slicer-basado-en-prusa-`), incluido lo que escribimos dentro de ese fork:
  es AGPL y lo escribieron sesiones que leyeron PrusaSlicer;
- **la historia de `imported/` y `docs/tisma/` en este repositorio** (commits `4768a57` y `759eccd`): ahí se copió código
  de Tisma Slicer y se sacó el 24-10-2026 (decisión del dueño: todo pasa por fichas). No se lee con `git log`,
  `git show` ni de ninguna otra forma;
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
- **Las especificaciones de `docs/spec/`,** incluidas las fichas de `docs/spec/features/` (ver «Fichas» abajo).
- **Buscar en la web** artículos, especificaciones y documentación de usuario (manuales, bases de conocimiento). Nunca
  código fuente de los programas prohibidos.
- **Comparar con otros slicers solo como caja negra:** ejecutar su programa y comparar el G-code, los tiempos o el
  filamento. Nunca abrir su código.

## Fichas (cómo pasa lo que ya hace Tisma Slicer)

Todo lo de Tisma Slicer llega acá como **fichas**, nunca como código, también lo que escribió el equipo del dueño:

1. **Quién las escribe:** el equipo que conoce Tisma Slicer por dentro (el equipo «sucio»), en `docs/spec/features/`.
   Ese equipo no escribe código en este repositorio.
2. **Qué dice una ficha:**
   - qué hace la función y para qué sirve;
   - sus ajustes, con unidad, rango y valor por defecto;
   - las reglas que tiene que cumplir;
   - las fórmulas, con el artículo, la norma o la especificación de donde salen;
   - los casos límite;
   - las pruebas como datos: entrada y resultado esperado, medido en Tisma Slicer como caja negra.
3. **Qué no puede decir una ficha:**
   - código ni pseudocódigo;
   - nombres de clases, funciones o archivos de Tisma Slicer o de PrusaSlicer;
   - cómo está organizado ese código.

   Los ajustes usan los nombres del motor (`docs/spec/SETTINGS_V1.md`); la correspondencia con los de Tisma Slicer
   queda del lado de Tisma, en lo que llama al motor. Las etiquetas del G-code de `docs/spec/GCODE_OUTPUT.md` sí se
   usan: son el formato que lee la vista previa.
4. **Filtro:** antes de subir una ficha se corre `tools/check_spec.py` (de la rama `dev`; solo informa archivo, línea y
   regla) y se corrigen las líneas marcadas. Después la revisa el dueño en un pull request.
5. **Cómo se usa:** quien programa el motor parte de la ficha y de las fuentes públicas, decide su propia estructura y
   anota en `PROVENANCE.md` «ficha `docs/spec/features/<nombre>.md`».

Lo que Tisma Slicer tomó de código ajeno (el render de PrusaSlicer 3.0, hosts de impresoras de OrcaSlicer, perfiles de
OrcaSlicer y Cura) se especifica solo desde fuentes públicas: artículos, especificaciones de formatos, documentación de
protocolos y datos de los fabricantes.

## Procedencia

- Cada módulo anota en `PROVENANCE.md` de dónde sale: el artículo y la sección, la especificación, o «diseño propio».
- La historia de commits es la prueba de procedencia: no se reescribe ni se aplasta.

## Antes de cambiar la licencia

Lo revisa un abogado. El modelo de lenguaje que escribe este código se entrenó con código público, incluidos esos
proyectos. Esto no se puede excluir del todo; por eso el motor se escribe a partir de las fuentes permitidas, con
estructura propia, y nunca reproduce código de memoria.
