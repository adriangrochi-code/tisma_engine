# Más papers que pueden servir (26-10-2026)

**Pedido del dueño:** qué otros papers de impresión 3D podrían servir, además de los de
`2026-10-26-papers-calidad.md`.

**Cómo se eligieron:** se buscaron temas con ficha en el motor que todavía no tienen fuentes publicadas en
`SOURCES.md`, `docs/RESEARCH_MAP.md` ni los otros documentos de investigación. Las mismas reglas que en la página
anterior: ideas con fuente, no reglas; resúmenes leídos, no código (`CLEANROOM.md`); patentes a revisar antes de
implementar; los números son de los autores.

## Color y varios filamentos

Para `color_mixing.md`, `color_mixing_plus.md` y `multimaterial.md` (fases MM1 a MM4).

| # | Paper | Qué aporta | Dónde sirve |
|---|---|---|---|
| C1 | Hergel, Lefebvre. *Clean Color: Improving Multi-filament 3D Prints.* Computer Graphics Forum 33(2) (2014) 469–478 (Eurographics 2014) | Tres arreglos por software para el goteo de las boquillas que esperan: girar la pieza en la cama para que el goteo moleste menos, una muralla descartable al lado de la pieza donde las boquillas se limpian y recargan, y un planificador de caminos que evita o esconde lo que queda | Impresoras de varias boquillas: muralla de limpieza más chica que una torre de purga; orden de caminos que esconde el goteo |
| C2 | Song, Martínez, Bedell, Vennin, Lefebvre. *Colored Fused Filament Fabrication.* ACM TOG 38(5), 2019. [arXiv](https://arxiv.org/abs/1709.09689) | Con una boquilla que mezcla filamentos, cada capa se parte en franjas de color fijo; el espesor de cada franja cambia el color que se ve. Señala que los métodos anteriores, que mezclan por oclusión (un color tapa al otro), solo mostraron tonos de dos colores y dependen de la pendiente de la cara | Base publicada para partir la capa en franjas de `color_mixing_plus.md`, y razón para avisar que la mezcla por capas se ve distinta en caras inclinadas |
| C3 | Kuipers, Elkhuizen, Verlinden, Doubrovski. *Hatching for 3D prints: line-based halftoning for dual extrusion fused deposition modeling.* Computers & Graphics 74 (2018) 23–32. [arXiv](https://arxiv.org/abs/1805.01375) | Medios tonos con líneas (como el grabado en trama) en vez de puntos, que encajan con cómo se extruye: una imagen en grises con dos filamentos, casi sin agregar tiempo. Falta calibrar el tono que se percibe | Imágenes y degradés en la superficie con dos colores (proyección de imagen de `color_mixing_plus.md`) |

## Piezas más grandes que la cama

Para `fragment.md` y `placement.md`.

| # | Paper | Qué aporta | Dónde sirve |
|---|---|---|---|
| F1 | Luo, Baran, Rusinkiewicz, Matusik. *Chopper: Partitioning Models into 3D-Printable Parts.* ACM TOG 31(6), SIGGRAPH Asia 2012 | Partir una pieza grande en partes que entran en la impresora, optimizando: que se pueda armar, pocas partes, uniones que no se noten y resistencia; agrega conectores en las uniones | Criterios y orden de `fragment.md` (dónde cortar, cuántas partes) |
| F2 | Song, Fu, Liu, Fu. *Printing 3D objects with interlocking parts.* Computer Aided Geometric Design 35–36 (2015) 137–148 | Partes que se traban entre sí sin conectores ni pegamento, y se pueden desarmar y volver a armar; trabaja con vóxeles y corrige la geometría para no dejar pedazos sueltos | Encastres de `fragment.md` que no necesitan piezas extra |
| F3 | Chen, Zhang, Lin, Hu, Lu, Huang, Benes, Cohen-Or, Chen. *Dapper: Decompose-and-Pack for 3D Printing.* ACM TOG 34(6), SIGGRAPH Asia 2015 | Partir y acomodar en la misma optimización: menos soporte, menos tiempo y menos armado. Va encajando las partes en una pila con una búsqueda en haz | Partir y acomodar en la cama juntos (`fragment.md` con `placement.md`) |

## Estabilidad, resistencia y ahorro de material

Para `stability.md`, `structural_optimization.md` y `structural_analysis.md`.

| # | Paper | Qué aporta | Dónde sirve |
|---|---|---|---|
| E1 | Prévost, Whiting, Lefebvre, Sorkine-Hornung. *Make It Stand: Balancing Shapes for 3D Fabrication.* ACM TOG 32(4), art. 81, SIGGRAPH 2013 | Que una figura se pare sola: vacía partes del interior (y, si el usuario deja, ajusta la forma) para llevar el centro de masa sobre la base; también con varias bases | Aviso de `stability.md` cuando la pieza impresa se caería, y relleno que la equilibra sin cambiar la superficie |
| E2 | Stava, Vanek, Benes, Carr, Měch. *Stress Relief: Improving Structural Strength of 3D Printable Objects.* ACM TOG 31(4), SIGGRAPH 2012 | Busca zonas con mucha tensión con un análisis liviano y el eje medio, y las corrige vaciando, engrosando o agregando puntales, hasta que no quedan | Correcciones automáticas de `structural_optimization.md` |
| E3 | Wang, Wang, Yang, Liu, Tong y otros. *Cost-effective Printing of 3D Objects with Skin-Frame Structures.* ACM TOG 32(6), art. 177, SIGGRAPH Asia 2013 | Cáscara más un armazón interno de pocos puntales (optimización con esparcidad l0), estable y fiel a la forma; probado en impresoras de extrusión | Relleno de puntales para piezas que solo se miran, y piezas huecas de resina con armazón (`resin.md`) |

## Vista previa rápida

| # | Paper | Qué aporta | Dónde sirve |
|---|---|---|---|
| W1 | Mueller, Im, Gurevich, Teibrich, Pfisterer, Guimbretière, Baudisch. *WirePrint: 3D Printed Previews for Fast Prototyping.* UIST 2014 | Imprimir la pieza como un alambre (una malla en el aire, extruyendo en 3D y no por capas) para probar tamaños y encajes: hasta 10 veces más rápido. Mejor en impresoras delta | Un modo «borrador» para probar medidas antes de la impresión final; no hay ficha |

## Lo que no se encontró

- **Alabeo y esquinas que se levantan:** solo foros y documentación de slicers; ningún paper con un modelo que el
  motor pueda usar. `docs/RESEARCH_MAP.md` ya trata la deformación.
- **Resina:** lo publicado sobre la fuerza de despegue habla de la máquina (vibración, film tensado, cuba que se
  inclina), no del laminador; lo que depende del laminador (orientación, ventosas) ya está en `resin.md`.

## Lo que más rinde para el motor, en orden

1. **C1 y C2:** van directo a la mezcla de colores con una boquilla que el dueño pidió (`color_mixing_plus.md`).
2. **F1 y F2:** dan la base publicada a `fragment.md`.
3. **E1:** un aviso útil para cualquier figura, con una ficha que ya existe (`stability.md`).
4. **E2, E3, F3 y W1:** para más adelante.
