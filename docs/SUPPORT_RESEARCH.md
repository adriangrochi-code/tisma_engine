# Soportes: menos, más fáciles de sacar y con mejor superficie

Pedido del dueño (2026-10-25): «ver papers de soportes no planares, arc overhang y formas de hacer soportes más fáciles
de remover con mejor resultado de calidad en la parte soportada».

Este documento junta lo publicado y propone el orden de trabajo. Las fichas mandan cuando dicen qué hace Tisma Slicer:
`docs/spec/features/arc_overhangs.md`, `supports_more.md` y `supports_skirt.md`. Los artículos dan el método.

**Actualización (2026-10-26):** S3 está hecho (`support.style` `organic`, `src/support/tree.*`). Para S1,
`docs/ARC_OVERHANGS.md` junta lo publicado sobre los arcos y las ondas, y cómo probarlos.

**Sala limpia:** varios de estos trabajos publicaron código: el guion de Arc Overhang, ConicalSlicer (GPL-3.0) y
otros. Se usan solo los artículos y las descripciones públicas; el código no se abre.

## Lo publicado

### Voladizos sin soporte

| Método | Cómo | Resultado | Fuente |
|---|---|---|---|
| **Arc Overhang** | Arcos concéntricos que crecen desde el borde apoyado; cada arco se apoya en el anterior | Voladizos de hasta 90° sin soporte, aun planos. Lento (2 a 5 mm/s en PLA), con la cara de abajo áspera y algo de alabeo | Steven McCulloch (2022–2023); ensayos de CNC Kitchen. No hay artículo revisado. La ficha `arc_overhangs.md` dice cómo lo hace Tisma Slicer |
| **Laminado cónico** | Capas en forma de cono en vez de planos: con 4 ejes y boquilla inclinada 45°, voladizos de hasta 100°. Con un cono suave (unos 15°) sirve en una impresora común | Voladizos sin soporte | Wüthrich, Gubser, Elspass, Jaeger, *Applied Sciences* 11(18):8760, 2021 (CC BY 4.0) |
| **Capas curvas** | Ver `docs/NONPLANAR_SLICING.md`: en 5 ejes, S³-Slicer imprime sin soporte; en 3 ejes, las capas curvas reducen los voladizos | — | Ver ahí |

### Menos soporte y más estable

| Método | Cómo | Resultado | Fuente |
|---|---|---|---|
| **Árbol (Clever Support)** | Orienta la pieza para el menor área a sostener y arma un árbol de largo total mínimo | −29,4 % de tiempo y −40,5 % de material contra el programa de la impresora | Vanek, Galicia, Benes, *Computer Graphics Forum* 33(5), 2014 |
| **Soportes ramificados** | Ramas que se juntan hacia la cama | −75 % de plástico en su ejemplo | Schmidt, Umetani, *Branching Support Structures for 3D Printing*, Autodesk Research, 2014 |
| **Andamios con puentes** | Puentes apoyados en pilares en los dos extremos: más estables que los árboles, poco material | Menos material sin perder estabilidad | Dumas, Hergel, Lefebvre, *Bridging the Gap*, ACM TOG 33(4), SIGGRAPH 2014 |
| **Nervaduras internas** | Para voladizos internos (techos de piezas huecas) | Ver el relleno rayo, `docs/INFILL.md` | Tricard, Claux, Lefebvre 2020 |

### Más fácil de sacar y mejor superficie debajo

| Qué | Resultado | Fuente |
|---|---|---|
| **Lo que más pesa:** el patrón del soporte y la separación en Z | En un diseño de experimentos con ULTEM 9085: el patrón y la separación en Z son los factores con más efecto sobre la superficie y el despegue | Tesis, Universidad de Bolonia (factorial fraccionado: patrón, densidad, interfaz, separación en Z) |
| **Separación en Z** | 1 a 1,5 veces la altura de capa es el punto medio: más se saca fácil pero la cara cae; menos queda mejor pero se pega | Guías de fabricantes y usuarios |
| **Capas de interfaz** | 2 o 3 capas rectilíneas: superficie más limpia y fácil de sacar | Ensayos de usuarios |
| **Interfaz de otro material** | PETG con interfaz de PLA en una boquilla: la anisotropía de rugosidad baja un 86 a 99 %, la fuerza para sacar el soporte un 63 %, y sale sin dañar. Cuesta un 57 % más de tiempo y un 35 % más de material por las purgas | Estudio de la Universidad de Bolonia (cris.unibo.it) |

No encontré artículos sobre una interfaz no planar que siga la cara inclinada de abajo de la pieza.

## Propuesta para el motor

1. **S1 — Arcos en voladizos**, según la ficha `arc_overhangs.md`:
   - arcos concéntricos desde el borde apoyado, a `ancho × (1 − solape)`;
   - 5 mm/s, ventilador al máximo, `;TYPE:Overhang perimeter`, relleno sólido encima;
   - la geometría (dónde van los centros y cuándo empieza un abanico nuevo) es diseño propio desde la descripción
     pública;
   - se prueba con la pieza A1 de la ficha y con el estante plano de la torre de temperatura. Ya estaba en `PLAN.md`.
2. **S2 — La superficie soportada:**
   - lo que falta de la ficha `supports_more.md`: separación de abajo, interfaz de abajo, patrones, lazos de
     contacto y vaina;
   - **interfaz no planar** (propia, no la vi publicada): la capa de arriba de la interfaz sube y baja dentro de la
     capa para seguir la cara inclinada de abajo de la pieza, con el mecanismo del Z anti-aliasing. La primera capa
     de la pieza sobre el soporte también se apoya en la cara real. La separación en Z queda pareja medida contra la
     superficie, y la cara soportada pierde los escalones;
   - **planchado de la interfaz** antes de la pieza, para una cara de contacto lisa;
   - **la primera capa sobre el soporte como puente** (flujo, velocidad y ventilador de puente), si la ficha no dice
     otra cosa.
3. **S3 — Soportes en árbol**, según la ficha `supports_more.md` (reglas 15 a 17: ramas redondas hasta la cama). El
   método sale de Vanek et al. 2014 y de Schmidt y Umetani 2014; los andamios con puentes de Dumas et al. 2014 quedan
   como opción para piezas altas.
4. **S4 — Laminado cónico** como un campo más del laminado por superficies de nivel (F7): `f = z − r·tan α` alrededor
   de un eje, con α limitado por la boquilla. Imprime voladizos hacia afuera sin soporte.
5. **S5 — Interfaz de otro material**, cuando esté el cambio de herramienta (MM1, `docs/MULTIMATERIAL.md`): solo las
   capas de interfaz en el material que no pega, con las purgas mínimas.

## Fuentes

- Arc Overhang: [CNC Kitchen, *Arc Overhangs*](https://www.cnckitchen.com/blog/arc-overhangs-a-new-way-of-printing-without-supports);
  [Hackaday](https://hackaday.com/blog/page/1833/?cat=en-CA); la ficha `docs/spec/features/arc_overhangs.md`.
- Wüthrich, Gubser, Elspass, Jaeger, *A Novel Slicing Strategy to Print Overhangs without Support Material*,
  Applied Sciences 11(18):8760, 2021, [DOI 10.3390/app11188760](https://doi.org/10.3390/app11188760).
- Vanek, Galicia, Benes, *Clever Support: Efficient Support Structure Generation for Digital Fabrication*, Computer
  Graphics Forum 33(5), 2014 ([Eurographics](https://diglib.eg.org/handle/10.1111/v33i5pp117-125)).
- Schmidt, Umetani, *Branching Support Structures for 3D Printing*, 2014
  ([Autodesk Research](https://research.autodesk.com/app/uploads/2023/03/branching-support-structures-for.pdf_recDjolFHUPxoeq6Q.pdf)).
- Dumas, Hergel, Lefebvre, *Bridging the Gap: Automated Steady Scaffoldings for 3D Printing*, ACM TOG 33(4), 2014,
  [DOI 10.1145/2601097.2601153](https://doi.org/10.1145/2601097.2601153).
- Interfaz de PLA sobre PETG en una boquilla: Universidad de Bolonia ([cris.unibo.it](https://cris.unibo.it/handle/11585/1077894)).
- Parámetros de soporte en ULTEM 9085: tesis de la Universidad de Bolonia ([amslaurea](https://amslaurea.unibo.it/id/eprint/39235/)).
