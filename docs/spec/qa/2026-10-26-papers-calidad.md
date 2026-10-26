# Papers para mejorar la calidad: no planar, 5 ejes y en general (26-10-2026)

**Pedido del dueño:** buscar papers sobre cómo mejorar la calidad de impresión con capas no planares, con 5 ejes y
en general, para pasárselos al equipo del motor.

**Cómo leer esta página:**
- son ideas con su fuente, no reglas: ninguna cambia una ficha hasta que el dueño la apruebe;
- se leyó el resumen de cada paper (en algunos, el texto publicado); los números son los de los autores, no
  medidos por nosotros;
- varios grupos publican su código. Por `CLEANROOM.md` no se lee: se usa solo lo que dice el paper;
- antes de implementar, revisar patentes como en `docs/RESEARCH_MAP.md`;
- primero van los papers que no están en `docs/RESEARCH_MAP.md`, `docs/NONPLANAR_SLICING.md`, `docs/ZAA.md` ni
  `SOURCES.md`; al final, los que ya están, con lo que agregan.

## Nuevos: capas no planares con 3 ejes (fase F7)

| # | Paper | Qué aporta | Dónde sirve |
|---|---|---|---|
| NP1 | Ottonello, Hugron, Parmiggiani, Lefebvre. *QuickCurve: revisiting slightly non-planar 3D printing.* arXiv 2406.03966, 2024 (preprint). [arXiv](https://arxiv.org/abs/2406.03966) | Una sola superficie de corte curva que copia las caras de arriba, sin choques, resuelta con un único problema de mínimos cuadrados: sin malla de tetraedros (CurviSlicer la necesita) ni iteraciones. Los caminos de arriba siguen la curvatura máxima o mínima. El escalón empeora en las pendientes bajas, así que poca curvatura ya mejora mucho | `nonplanar_modes.md`, modo `surface` (reglas 14 a 17): forma más rápida de calcular la deformación; dirección de las líneas de la superficie de arriba según la curvatura |

## Nuevos: varios ejes (5 ejes y brazo robot)

Hoy no hay ficha para 5 ejes; `SOURCES.md` los deja para después de la primera versión. `docs/NONPLANAR_SLICING.md`
ya tiene Reinforced FDM, S³-Slicer y Neural Slicer; estos completan lo que falta para escribir la ficha.

| # | Paper | Qué aporta | Para qué |
|---|---|---|---|
| MA1 | Dai, Wang, Wu, Lefebvre, Fang, Liu. *Support-free volume printing by multi-axis motion.* ACM TOG 37(4), 2018 (en `SOURCES.md`, sin resumen en los documentos) | Parte el volumen en superficies curvas con un campo escalar que ordena la fabricación, y las superficies en caminos. Voladizos grandes y piezas con agujeros, sin soporte | Base de un modo de 5 ejes sin soporte |
| MA2 | Xu, Li, Chen, Tang. *Curved layer based process planning for multi-axis volume printing of freeform parts.* Computer-Aided Design 114 (2019) 51–63 | Capas desde un campo de distancia geodésica medido desde la base; cada capa se rellena en el plano y se lleva a 3D con un mapeo armónico. Un trabajo posterior anota que las capas se pueden cruzar si están muy juntas | Alternativa más simple que MA1; prueba de que dos capas no se cruzan |
| MA3 | Zhang, Chen, Fang, Tian, Wang. *Singularity-Aware Motion Planning for Multi-Axis Additive Manufacturing.* IEEE Robotics and Automation Letters 6(4) (2021) 6172–6179. [arXiv](https://arxiv.org/abs/2103.00273) | Cerca de la singularidad de los ejes giratorios, un camino suave se vuelve un movimiento brusco de la máquina y deja material de más o de menos. Planifica evitando singularidades y choques a la vez, con la velocidad de la punta entre un mínimo y un máximo | Regla de G-code de 5 ejes: rodear las singularidades y calcular el caudal con la velocidad real de la punta |
| MA4 | Zhang, Huang, Kukulski, Dutta, Fang, Wang. *Support Generation for Robot-Assisted 3D Printing with Curved Layers.* ICRA 2023 (en `SOURCES.md`, sin resumen en los documentos) | Soporte por esqueleto, guardado como sólido implícito para evitar errores numéricos, cuando la dirección de apilado cambia en cada capa | Soportes con capas curvas |
| MA5 | Han, Wu, Liu, Song, Cui. *Research on a Support-Free Five-Degree-of-Freedom Additive Manufacturing Method.* Micromachines, 2024. [PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC11279049/) | Impresora de 5 ejes que inclina la cama: modelo de su cinemática y caminos continuos. 23,58 % menos tiempo y 33,06 % menos filamento que con 3 ejes en sus piezas | El caso de cama que gira, el más común en kits de 5 ejes |

## Nuevos: calidad en general (3 ejes)

| # | Paper | Qué aporta | Dónde sirve |
|---|---|---|---|
| G1 | Andersons, Sanchez, Vaneker. *Wave-inspired path-planning strategy for support-free horizontal overhangs in FDM.* Additive Manufacturing Letters, 2026 (leído en la nota de prensa; ScienceDirect PII S277236902600040X) | Caminos que avanzan como ondas desde lo apoyado y rodean la forma, para voladizos horizontales sin soporte. Contra los arcos: menos caída, menos huecos, mejor cobertura; hasta 39 % menos material en su ejemplo. Falta probar otros materiales | `arc_overhangs.md`: otra forma de llenar el voladizo, para las formas donde los arcos dejan huecos |
| G2 | Hoteit, Balestra, Mingard, Balta, Lygeros. *Closed Loop Reference Optimization for Extrusion Additive Manufacturing.* arXiv 2512.16333, 2025 (preprint) | Con un sensor de fuerza en el extrusor y un regulador LQR: 39,57 % menos error en el ancho del cordón y 83,7 % menos tiempo de asentamiento | A futuro, impresoras con sensor de fuerza; mismo grupo que la calibración con foto de `docs/RESEARCH_MAP.md` |
| G3 | Wu. *Modeling and Feedforward Deposition Control in Fused Filament Fabrication.* Tesis doctoral, Universidad de Michigan, 2024 (leído el resumen) | Los métodos comunes usan un modelo lineal de la extrusión, que es muy no lineal. Propone un modelo no lineal para la extrusión continua y otro para arranques y cortes, con control anticipado distinto para extrusor directo y Bowden | Junta en un texto lo de Wu 2021 y 2023 de `docs/RESEARCH_MAP.md`. Es del grupo con patentes que ese documento pide revisar antes |

## Ya en los documentos del motor

| Paper | Dónde está | Lo que agrega esta búsqueda |
|---|---|---|
| CurviSlicer (Etienne y otros, 2019) | `docs/NONPLANAR_SLICING.md` | Nada nuevo; NP1 es su sucesor más rápido, del mismo grupo |
| Ahlers y otros, IEEE CASE 2019 | `docs/NONPLANAR_SLICING.md` | Acabado mucho mejor con un tiempo de impresión parecido: criterio para curvar solo donde mejora (modo `automatic`) |
| Song, Ray, Sokolov, Lefebvre, 2017 | `docs/ZAA.md` | La cota de media capa como prueba |
| Reinforced FDM, S³-Slicer, Neural Slicer | `docs/NONPLANAR_SLICING.md` | Nada nuevo |
| Lin y otros, calibración con foto, 2025 | `docs/RESEARCH_MAP.md` | Nada nuevo |
| Kuipers y otros 2020; Duan, Yoon, Okwudire 2018 | `SOURCES.md` y `qa/2026-10-26-tiempo-de-impresion.md` | Nada nuevo |

**Costura:** no se encontró un paper sobre dónde poner la costura; solo documentación de los slicers.

## Lo que más rinde para el motor, en orden

1. **G1 (ondas en voladizos):** mejora piezas reales con 3 ejes y ya hay una ficha donde va (`arc_overhangs.md`).
2. **NP1 (QuickCurve):** una forma más rápida y sin choques del modo `surface`, el que más se ve.
3. **MA1 a MA5:** junto con lo que ya está en `docs/NONPLANAR_SLICING.md`, alcanza para escribir una ficha de 5
   ejes cuando el dueño lo decida. MA3 (singularidades) es lo primero que hace falta para que el G-code de 5 ejes no
   deje marcas.
4. **G2 y G3:** para más adelante; G3 después de revisar patentes.
