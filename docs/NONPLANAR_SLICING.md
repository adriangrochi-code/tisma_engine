# Laminado no planar: qué dicen los artículos y cómo conviene hacerlo

Pedido del dueño (2026-10-25): «buscá papers de la manera más óptima de aplicar slicing no planar».

No hay una sola manera óptima: depende de qué se busca y de la máquina. Este documento junta los métodos publicados,
qué logra cada uno y qué conviene para el motor. Es la base de F7 (no planar en 3 ejes) y de F9 (5 ejes) en `PLAN.md`.

**Sala limpia:** varios de estos trabajos publicaron su código (CurviSlicer, S³-Slicer). Se usan solo los artículos;
el código no se abre.

## Qué se busca

| Objetivo | Por qué importa | Con qué se mide |
|---|---|---|
| **Superficie** | Las capas planas dejan escalones en las caras poco inclinadas | Error de volumen, rugosidad |
| **Resistencia** | La unión entre capas es la parte débil; las capas curvas la ponen donde menos se carga | Carga de rotura, rigidez |
| **Sin soportes** | Con 5 ejes, las capas curvas siguen al voladizo | Volumen de soporte |

## Los métodos

| Método | Máquina | Cómo | Resultado publicado |
|---|---|---|---|
| **CurviSlicer** (Etienne, Ray, Panozzo, Hornus, Wang, Martínez, McMains, Alexa, Wyvill, Lefebvre; ACM TOG 38(4), SIGGRAPH 2019) | 3 ejes, con una boquilla en punta | Optimiza una deformación vertical `h(p)` de la pieza en una malla de tetraedros. La pieza deformada se lamina plana y los recorridos vuelven a la pieza original. Es un problema convexo (QP) que se resuelve por iteraciones. **Restricciones:** el espesor entre un mínimo y un máximo (`1 ≤ ∂h/∂z ≤ τmax/τmin`), y la pendiente bajo el ángulo de la boquilla (`|∂h/∂x|, |∂h/∂y| ≤ ∂h/∂z / tan θmax`). **Objetivos:** aplanar las caras de arriba, alinearlas con el tope de una capa, poner las demás verticales, y suavidad | Con las mismas 40 capas, error de volumen de 57 mm³, contra 149 de la mejor altura adaptativa y 270 de capas parejas. Recorridos sin choques garantizados. Ejemplo con θmax = 30° y espesor de 0,1 a 0,6 mm |
| **Capas planas con techo curvo** (Ahlers, Wasserfall, Hendrich, Zhang, IEEE CASE 2019) | 3 ejes | El interior plano y solo las últimas capas siguen la superficie de arriba | Superficie lisa en techos poco inclinados; sin el problema de optimizar toda la pieza |
| **Mezcla de capas planas y no planas** (arXiv 2411.07225, 2024) | 3 ejes | Elige por zona entre capas planas y no planas | Mejor superficie |
| **Capas isotérmicas** (J. Manufacturing Processes, 2022) | 3 ejes | Capas como superficies de temperatura pareja | +14 % de tracción y −63 % de rugosidad |
| **No planar en 3 ejes con ángulos optimizados** (CIRP J. Manuf. Sci. Tech., 2025) | 3 ejes | Ángulo e inclinación de capa optimizados | Hasta +11 % en XY y +24 % en Z; baja con ángulos grandes |
| **Reinforced FDM** (Fang, Zhang, Zhong, Chen, Zhong, Wang; ACM TOG 39(6), SIGGRAPH Asia 2020) | 5 ejes | Las capas y los cordones siguen las direcciones de tensión principal del análisis por elementos finitos | Hasta **6,35 veces** la carga de una pieza con capas planas |
| **S³-Slicer** (Zhang, Fang, Huang, Dutta, Lefebvre et al.; SIGGRAPH Asia 2022) | 5 ejes | Un campo de direcciones de impresión (cuaterniones) según el objetivo, una deformación de la pieza que lo sigue, y las capas como superficies de nivel de la altura deformada. Junta sin soportes, resistencia y superficie | Un marco general para los tres objetivos |
| **Neural Slicer** (Liu, Wang et al.; ACM TOG, SIGGRAPH 2024) | 5 ejes | El campo de las capas como una red neuronal, optimizado con las pérdidas de cada objetivo | Capas más alineadas a la tensión máxima que S³-Slicer |
| **Fibra continua en capas curvas** (arXiv 2311.17265, 2023) | 5 ejes | Fibra continua por las líneas de tensión en capas curvas | Rotura a 9,27 kN: +168 % que con capas planas |

## Qué conviene para el motor

El motor ya corta capas como superficies de nivel de un campo escalar (`PLAN.md`, «Diseño para la
visión»). Todos estos métodos dan ese campo: CurviSlicer lo da como la altura deformada `h(p)`, S³-Slicer como la altura
del sólido deformado. Lo que falta es calcularlo bien.

**En 3 ejes (F7), por orden:**
1. **Techos curvos** (como Ahlers 2019): el interior plano y las últimas capas bajo la superficie de arriba
   siguiéndola, dentro de la pendiente que deja la boquilla. Es lo más barato y lo que más se ve. El Z anti-aliasing,
   que ya está, es su versión más chica: mueve la boquilla dentro de la capa, sin curvar la capa.
2. **El campo optimizado de CurviSlicer**, reescrito desde el artículo:
   - el campo `h` en la grilla de vóxeles que ya usa el análisis (A1), en vez de tetraedros;
   - las restricciones de espesor y pendiente tal cual el artículo;
   - el problema convexo con un solucionador propio o una biblioteca permisiva. Por ejemplo OSQP, con licencia
     Apache-2.0, que se revisa y se anota en `THIRD_PARTY.md` antes de usarla.
3. **Lo que no hace ninguno en 3 ejes, y el motor puede:** sumar al objetivo un término de resistencia con las
   tensiones del análisis propio (A1). Las capas se inclinan hacia la dirección de la tensión principal, dentro del
   cono que permite la boquilla: la idea de Reinforced FDM llevada a una impresora común. Se mide con el análisis «como
   impresa», con la unión entre capas inclinada (N3 de `docs/NONPLANAR_INFILL.md`).

**Límite de las impresoras comunes:** la pendiente máxima es la del cono de la boquilla y lo que libra el carro.
CurviSlicer usa una boquilla en punta y θmax = 30°; con una boquilla común, de cara plana ancha, el ángulo útil es
menor. La pendiente se pone como ajuste de la impresora: hay que medirla o tomarla del fabricante.

**En 5 ejes (F9):** el marco de S³-Slicer (campo de direcciones, deformación y superficies de nivel), con los objetivos
de Reinforced FDM para la resistencia y el campo de Neural Slicer después, si hace falta.

**Junto con el relleno ondulado (N1):** son complementarios. Las capas curvas cambian la forma de toda la capa; el
relleno ondulado traba capas vecinas a escala del cordón. Se pueden usar juntos.

## Fuentes

- Etienne et al., *CurviSlicer: Slightly curved slicing for 3-axis printers*, ACM TOG 38(4), 2019,
  [DOI 10.1145/3306346.3323022](https://doi.org/10.1145/3306346.3323022) ([PDF](https://cims.nyu.edu/gcl/papers/2019-CurviSlicer.pdf)).
- Ahlers, Wasserfall, Hendrich, Zhang, *3D Printing of Nonplanar Layers for Smooth Surface Generation*, IEEE CASE
  2019 ([DOI 10.1109/COASE.2019.8843116](https://dl.acm.org/doi/10.1109/COASE.2019.8843116)).
- Fang et al., *Reinforced FDM: Multi-Axis Filament Alignment with Controlled Anisotropic Strength*, ACM TOG 39(6),
  2020, [DOI 10.1145/3414685.3417834](https://doi.org/10.1145/3414685.3417834).
- Zhang et al., *S³-Slicer: A General Slicing Framework for Multi-Axis 3D Printing*, SIGGRAPH Asia 2022.
- Liu et al., *Neural Slicer for Multi-Axis 3D Printing*, ACM TOG, SIGGRAPH 2024 ([arXiv 2404.15061](https://arxiv.org/abs/2404.15061)).
- *Exceptional Mechanical Performance by Spatial Printing with Continuous Fiber*, 2023 ([arXiv 2311.17265](https://arxiv.org/abs/2311.17265)).
- *Additive manufacturing of non-planar layers using isothermal surface slicing*, J. Manufacturing Processes, 2022
  ([ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/S1526612522009173)).
- *Improving surface quality and mechanical strength through the optimization and enhancement of FDM for non-planar 3D
  printing*, 2025 ([ScienceDirect](https://www.sciencedirect.com/science/article/pii/S1755581725001762)).
- Combinación de capas planas y no planas, 2024 ([arXiv 2411.07225](https://www.arxiv.org/abs/2411.07225)).
