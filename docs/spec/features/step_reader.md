# Lector de STEP propio

**Qué hace:** abre archivos STEP (ISO 10303, partes 203, 214 y 242) y convierte el sólido en triángulos, sin usar una
biblioteca de terceros. Es lo que pide `formats.md`, regla 2, y la base de `step_faces.md`.

**Decisión del dueño (28-10-2026):** el lector se escribe desde cero en el motor (opción C). Se descartaron usar
OpenCASCADE en un programa aparte (LGPL), leer con una biblioteca permisiva solo la estructura y dejar STEP fuera de la
primera versión.

**Fase:** P4.

**Fuentes:**
- esquemas EXPRESS de las partes 203, 214 y 242 (AIM/MIM «longform»), de descarga libre en el MBx-IF (antes CAx-IF),
  `https://www.mbx-if.org/home/?p=613`. Dicen qué entidades hay y qué atributos tiene cada una;
- prácticas recomendadas del MBx-IF para implementadores, de descarga libre, `https://www.mbx-if.org/home/?p=194`:
  unidades, ensambles, colores y geometría facetada;
- el texto de las normas ISO 10303-21 (formato del archivo) y 10303-42 (geometría) es pago y no se compró. Si hace
  falta, lo decide el dueño;
- B-splines y NURBS: Piegl y Tiller, *The NURBS Book*, 2.ª edición, Springer, 1997;
- piezas de prueba: archivos STEP del NIST (proyecto MBE PMI), `https://www.nist.gov/document/nist-pmi-step-files`.
  Según el NIST, «se pueden usar sin restricciones»; pide que se lo nombre al usarlos.

## Qué traen los archivos

Las 11 piezas del NIST sin PMI (carpeta «AP203 geometry only») usan estas entidades. Es el mínimo que el lector tiene que
entender:

| Grupo | Entidades |
|---|---|
| Sólido | `MANIFOLD_SOLID_BREP`, `CLOSED_SHELL`, `OPEN_SHELL`, `SHELL_BASED_SURFACE_MODEL`, `ADVANCED_BREP_SHAPE_REPRESENTATION`, `MANIFOLD_SURFACE_SHAPE_REPRESENTATION` |
| Caras y bordes | `ADVANCED_FACE`, `FACE_OUTER_BOUND`, `FACE_BOUND`, `EDGE_LOOP`, `VERTEX_LOOP`, `ORIENTED_EDGE`, `EDGE_CURVE`, `VERTEX_POINT` |
| Superficies | `PLANE`, `CYLINDRICAL_SURFACE`, `CONICAL_SURFACE`, `SPHERICAL_SURFACE`, `TOROIDAL_SURFACE`, `B_SPLINE_SURFACE_WITH_KNOTS` y la racional (instancia compuesta con `RATIONAL_B_SPLINE_SURFACE`) |
| Curvas | `LINE`, `CIRCLE`, `ELLIPSE`, `B_SPLINE_CURVE_WITH_KNOTS`, `TRIMMED_CURVE`, `COMPOSITE_CURVE`, `COMPOSITE_CURVE_SEGMENT` |
| Posición | `CARTESIAN_POINT`, `DIRECTION`, `VECTOR`, `AXIS2_PLACEMENT_3D` |
| Unidades | `SI_UNIT` (milímetros), `CONVERSION_BASED_UNIT` (pulgadas), `UNCERTAINTY_MEASURE_WITH_UNIT` |

Los estilos (`SURFACE_STYLE_*`, `CURVE_STYLE`) y la PMI se pueden ignorar en la primera versión.

## Reglas

1. **El archivo:** encabezado y sección de datos; instancias simples (`#12 = PLANE(...)`) e instancias compuestas
   (`#12 = ( NOMBRE(...) NOMBRE(...) )`). Las referencias pueden apuntar hacia adelante en el archivo.
2. **Archivos con errores:** el NIST avisa que sus archivos no están libres de errores de sintaxis. Una entidad que no
   se entiende se saltea; la pieza se abre igual.
   - Si falta una cara, aviso con la cantidad de caras que no se pudieron leer.
   - Si no queda ningún sólido, el error dice por qué.
   - El motor nunca se cierra por un archivo roto.
3. **Unidades:** todo pasa a milímetros. Una pieza en pulgadas se multiplica por 25,4 (prueba ST3: 12 pulgadas dan
   304,80 mm).
4. **Superficies analíticas** (plano, cilindro, cono, esfera, toro): se evalúan con sus fórmulas, no con una
   aproximación.
5. **B-splines:** curvas y superficies, racionales o no, se evalúan como en Piegl y Tiller.
6. **Caras recortadas:** cada cara es su superficie dentro de su borde de afuera, menos los agujeros (`FACE_BOUND`).
   El sentido de cada borde sale de `ORIENTED_EDGE` y de la cara (`same_sense`).
7. **Finura:** los triángulos se apartan de la superficie a lo sumo el desvío elegido. Dentro de un triángulo la
   normal gira a lo sumo el ángulo elegido. Los valores y el diálogo están en `step_faces.md`, regla 1; por defecto
   Medium: 0,003 mm y 0,5°.
8. **Costuras entre caras:** dos caras que comparten un borde usan los mismos puntos en ese borde. Un sólido cerrado
   da una malla cerrada, sin bordes sueltos (pruebas ST1 a ST11: 0 bordes abiertos).
9. **Cara de cada triángulo:** cada triángulo guarda el número de su cara, en el orden del archivo (`step_faces.md`,
   regla 2).
10. **Varios sólidos y ensambles:** cada sólido es una parte del objeto. Las posiciones de un ensamble se aplican antes
    de convertir. Se mide aparte con las piezas AP242 del NIST, en una segunda etapa.
11. **Geometría facetada** (AP242, el archivo `nist_ftc_08_asme1_ap242-e1-tg.stp`): los triángulos se toman tal cual.
    Segunda etapa.

## Etapas

| Etapa | Contenido | Termina cuando |
|---|---|---|
| E1 | Leer el archivo (regla 1), unidades, planos y líneas | El bloque de `formats.md` FM4 sin el agujero: caja exacta |
| E2 | Cilindros, conos, esferas, toros, círculos, elipses, caras con agujeros | FM4 y ST11 |
| E3 | B-splines y NURBS, curvas compuestas y recortadas | ST1 a ST10 |
| E4 | Cara por triángulo, varios sólidos, ensambles y geometría facetada | `step_faces.md` y las piezas AP242 del NIST |

## Pruebas

Medidas en Tisma Slicer: cada archivo de la carpeta «AP203 geometry only» del NIST se abrió desde la línea de comandos
con la finura por defecto y se guardó como STL. Volumen y tamaño en mm.

**Para pasar:**
- tamaño igual, ±0,05 mm;
- volumen igual, ±0,5 %;
- 0 bordes abiertos.

La cantidad de triángulos es de referencia: el motor puede dar otra si cumple la finura de la regla 7.

| # | Archivo | Unidad | Tamaño (X × Y × Z) | Volumen (mm³) | Triángulos | Bordes abiertos |
|---|---|---|---|---|---|---|
| ST1 | `nist_ctc_01_asme1_rd.stp` | mm | 800,00 × 450,00 × 150,00 | 14 642 881 | 21 918 | 0 |
| ST2 | `nist_ctc_02_asme1_rc.stp` | mm | 680,00 × 860,00 × 400,00 | 47 100 985 | 424 454 | 0 |
| ST3 | `nist_ctc_03_asme1_rc.stp` | pulgada | 302,96 × 485,44 × 162,68 | 331 932 | 12 578 | 0 |
| ST4 | `nist_ctc_04_asme1_rd.stp` | mm | 500,00 × 780,00 × 130,60 | 17 519 254 | 215 980 | 0 |
| ST5 | `nist_ctc_05_asme1_rd.stp` | pulgada | 527,05 × 558,80 × 482,60 | 12 694 854 | 163 642 | 0 |
| ST6 | `nist_ftc_06_asme1_rd.stp` | pulgada | 304,80 × 97,79 × 247,65 | 3 323 521 | 154 346 | 0 |
| ST7 | `nist_ftc_07_asme1_rd.stp` | pulgada | 311,61 × 135,89 × 222,71 | 1 726 254 | 58 958 | 0 |
| ST8 | `nist_ftc_08_asme1_rc.stp` | pulgada | 311,59 × 222,69 × 48,26 | 503 592 | 31 978 | 0 |
| ST9 | `nist_ftc_09_asme1_rd.stp` | pulgada | 190,50 × 3,04 × 279,40 | 136 449 | 15 376 | 0 |
| ST10 | `nist_ftc_10_asme1_rb.stp` | mm | 158,00 × 31,00 × 75,00 | 188 331 | 65 076 | 0 |
| ST11 | `nist_ftc_11_asme1_rb.stp` | mm | 63,00 × 63,00 × 3,00 | 5 127 | 33 492 | 0 |
| ST12 | Un archivo con una entidad rota en una cara | — | — | — | — | Se abre sin esa cara, con el aviso de la regla 2 |
| ST13 | Un archivo sin ningún sólido | — | — | — | — | Error que lo dice, sin cerrarse |
