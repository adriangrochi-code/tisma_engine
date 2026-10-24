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
  | Eigen | MPL-2.0 (sin modificar sus archivos) |
  | Catch2 | BSL-1.0 |
  | nlohmann/json | MIT |
  | miniz | MIT |
- **Comparar con otros slicers solo como caja negra:** ejecutar su programa y comparar el G-code, los tiempos o el
  filamento. Nunca abrir su código.

## Procedencia

- Cada módulo anota en `PROVENANCE.md` de dónde sale: el artículo y la sección, la especificación, o «diseño propio».
- La historia de commits es la prueba de procedencia: no se reescribe ni se aplasta.

## Paso de información entre los dos equipos

Hay dos equipos. El que conoce Tisma Slicer (la sesión principal de Tisma, que leyó PrusaSlicer y Cura) dice *qué*
hay que construir. El equipo del motor lo construye desde cero sin haber visto ese código. Decisión del dueño
(2026-10-24): la información pasa sin código y con nombres propios, para no romper la licencia del motor.

1. **No pasa código**, ni como archivos del repositorio ni en mensajes o instrucciones de sesión. Tampoco
   pseudocódigo que copie la estructura de un código existente.
2. **Solo pasan especificaciones funcionales**, en `docs/spec/`:
   - comportamiento esperado;
   - parámetros con rangos y valores por defecto;
   - fórmulas con la cita del artículo;
   - casos de prueba de caja negra (modelo y ajustes que dan un resultado medible).
3. **Nombres propios:** las especificaciones no usan nombres internos de PrusaSlicer, Slic3r, Cura ni Tisma Slicer
   (tipos, funciones, archivos, variables). Los conceptos y los parámetros llevan nombres del motor. Si hace falta
   traducir las claves de configuración de Tisma Slicer a las del motor, esa tabla vive del lado de Tisma Slicer (en
   el adaptador que llama al motor), no en este repositorio.
4. **Filtro:** `tools/check_spec.py` revisa cada especificación antes de que el equipo del motor la lea. Busca bloques
   de código, líneas con forma de código, nombres de archivos fuente y nombres internos de los programas prohibidos.
   Informa solo archivo, línea y regla, sin mostrar el texto, para que quien lo corre no quede expuesto. El equipo
   que escribe corrige lo marcado. El motor solo lee especificaciones que pasan, y la CI lo comprueba.
5. **Del motor hacia el otro equipo** solo van preguntas de comportamiento y resultados de caja negra. Las respuestas
   vuelven como especificaciones. La revisión del PR del motor por la sesión principal se limita al comportamiento y
   las pruebas.
6. **Código importado de Tisma Slicer:** `imported/tisma/` y `docs/tisma/`, subidos a `main` el 2026-10-24, no se
   leen ni se usan. Están dentro del árbol de PrusaSlicer, usan sus tipos, los escribió una sesión que leyó
   PrusaSlicer y, según el filtro, `docs/tisma/` trae código. Se pidió sacarlos de `main`. Si borrar también su
   historia, lo deciden el dueño y un abogado.

## La licencia tiene que poder cerrarse

Decisión del dueño (2026-10-24): el motor tiene que poder pasar a una licencia cerrada (propietaria) en cualquier
momento. Nada de lo que entre al repositorio puede impedirlo:

- **Código copiado:** no se copia código de ningún lado, ni fragmentos chicos. Eso incluye Stack Overflow (CC BY-SA),
  blogs, ejemplos de otros proyectos y código de artículos. Todo se escribe acá, desde las fuentes permitidas.
- **Bibliotecas:** solo licencias permisivas, anotadas en `THIRD_PARTY.md` antes de usarlas. MPL-2.0 solo sin
  modificar sus archivos. Nada GPL, LGPL, AGPL, SSPL, CC-BY-SA ni «no comercial» de terceros, tampoco como
  dependencia opcional ni dentro de una biblioteca permitida. `tools/check_licenses.py` lo comprueba en la CI.
- **Datos:** modelos de prueba, fuentes tipográficas, imágenes y perfiles también tienen licencia. No se guardan en el
  repositorio si su licencia lo impediría (por ejemplo «sin obras derivadas» o «no comercial»).
- **Aportes de otras personas:** el dueño solo puede cambiar la licencia de código que es suyo. Hasta tener un acuerdo
  de cesión de derechos revisado por un abogado, no se aceptan aportes de terceros (ver `CONTRIBUTING.md`).
- **Lo ya publicado:** cerrar la licencia vale para las versiones nuevas. Quien ya recibió una versión bajo la
  PolyForm Noncommercial conserva esos permisos para esa versión, y solo para uso no comercial.

## Antes de cambiar la licencia

Lo revisa un abogado. La PolyForm Noncommercial se adoptó el 2026-10-24 por decisión del dueño, al hacer público el
repositorio; la revisión de un abogado sigue pendiente.

El modelo de lenguaje que escribe este código se entrenó con código público, incluidos esos
proyectos. Esto no se puede excluir del todo; por eso el motor se escribe a partir de las fuentes permitidas, con
estructura propia, y nunca reproduce código de memoria.
