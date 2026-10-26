# Papers de resina: representar fielmente la impresión (26-10-2026)

**Pedido del dueño:** papers sobre resina para que el motor represente fielmente cómo sale la impresión.

**Qué hay hoy:** `resin.md` y `resin_supports.md` se escribieron midiendo Tisma Slicer y con la especificación de
Elegoo y la documentación de Mango3D. No citan ningún paper, y ningún documento de investigación del motor trata la
resina.

**Reglas de esta página:** las mismas que en `2026-10-26-papers-calidad.md`. Son ideas con su fuente, no reglas; se
leyeron resúmenes y textos abiertos, no código (`CLEANROOM.md`); hay que revisar patentes antes de implementar; los
números son de los autores. Lo que no se pudo confirmar dice «sin verificar».

## Cuánto cura la luz

| # | Fuente | Qué aporta | Dónde sirve |
|---|---|---|---|
| R1 | Jacobs. *Rapid Prototyping & Manufacturing: Fundamentals of Stereolithography.* SME, 1992 (citado así en patentes y fichas de fabricantes de resina; no se leyó el libro) | La curva de trabajo: profundidad curada `Cd = Dp · ln(E / Ec)`. `E` es la dosis (irradiancia por tiempo), `Dp` la profundidad de penetración de la resina y `Ec` la dosis crítica. Las dos dependen de la resina y de la impresora, y se miden | El modelo que falta para todo lo demás. Torre de exposición (`resin.md`, reglas 16 a 18): ajustar `Dp` y `Ec` con las franjas, no solo elegir una. Capas de fondo (regla 7). Corrección de Z (regla 21): cuántas capas dejar sin exponer bajo lo que cuelga sale de `Cd`, no de un número fijo |
| R2 | Gong, Bickham, Woolley, Nordin. *Custom 3D printer and resin for 18 µm × 20 µm microfluidic flow channels.* Lab on a Chip 17 (2017). [BYU](https://scholarsarchive.byu.edu/facpub/1923) | Un modelo de la profundidad de penetración que solo necesita la absortividad del absorbente UV. Canales de 18 × 20 µm, achicando los canales al dibujarlos | Estimar `Dp` de la ficha de la resina cuando no hay torre impresa. Tamaño mínimo de agujeros y canales según `Dp` |
| R3 | Pritchard, de Beer, Whelan, Scott, Burns. *Modeling and Correcting Cure-Through in Continuous Stereolithographic 3D Printing.* Universidad de Michigan, 2019; DOI 10.1002/admt.201900700 (revista sin verificar) | La luz que atraviesa las capas de arriba también cura la resina de abajo (curado de más), sobre todo bajo las caras de abajo y dentro de los huecos. Cambian las imágenes para que cada punto reciba la dosis pedida | Generaliza la regla 21, que hoy deja sin exponer un número fijo de capas: bajar los grises de cada capa sobre un hueco o un voladizo según la dosis que ya recibió la resina de abajo |

## La pantalla y los píxeles

| # | Fuente | Qué aporta | Dónde sirve |
|---|---|---|---|
| R4 | Universidad Tecnológica de Rzeszów. *Development of a Calibration Procedure of the Additive Masked Stereolithography Method for Improving the Accuracy of Model Manufacturing.* Applied Sciences 15(13), 7412, 2025 (autores sin verificar) | Calibración en tres etapas para MSLA. 1: la pantalla no ilumina parejo; una regresión entre brillo del píxel e intensidad UV da una máscara de grises que lo compensa (la desviación baja de 0,26 a 0,17 mW/cm²). 2: el tiempo de exposición, entre resistencia y detalle perdido por la luz que se escapa. 3: la escala | Una calibración nueva: máscara por impresora aplicada a cada imagen. La etapa 3 coincide con `resin.scale_x` y el bloque de medidas (regla 19) |
| R5 | Xu y otros. *International Journal of Extreme Manufacturing* 6, 035004, 2024 (grupo de Yong Chen; título sin verificar) | Entre los píxeles de una pantalla LCD hay zonas oscuras y bordes escalonados; los corrigen moviendo la máscara menos de un píxel (10 a 50 µm, a 2 o 3 Hz) | Es de la máquina, pero dice qué dibujar en la vista previa: el borde real tiene escalones de un píxel y zonas oscuras, y los grises del borde (reglas 4 y 20) los suavizan solo en parte |

## Despegue de cada capa

| # | Fuente | Qué aporta | Dónde sirve |
|---|---|---|---|
| R6 | Pan, He, Xu, Feinerman. *Study of separation force in constrained surface projection stereolithography.* Rapid Prototyping Journal 23(2) (2017) 353–361 | Midieron la fuerza de despegue en una impresora de abajo hacia arriba y encontraron qué rasgos de la capa la deciden (área y forma); proponen el control por capa como paso siguiente | Movimientos por capa (reglas 8 y 12, valores por capa en el modo avanzado): subir más lento en las capas con mucha área. Un gráfico de fuerza estimada por capa en la vista previa, y aviso donde salta |
| R7 | *The Effect of Acceleration on the Separation Force in Constrained-Surface Stereolithography.* Applied Sciences 12(1), 442, 2022 (autores sin verificar) | Cómo la aceleración de la plataforma cambia la fuerza de despegue | Aceleración de la subida por capa, junto con R6 |

## Contracción y alabeo

| # | Fuente | Qué aporta | Dónde sirve |
|---|---|---|---|
| R8 | Flach, Chartoff. Modelo de contracción por grado de curado, Solid Freeform Fabrication Symposium 1994. [PDF](https://utw10945.utweb.utexas.edu/Manuscripts/1994/1994-23-Flach.pdf) | La contracción sale del cambio de volumen al curar, con un retraso entre el curado y la contracción; el alabeo (curl) es la principal fuente de error de medidas | Aviso de riesgo de alabeo en caras planas grandes y finas; más adelante, corrección de escala por zona |
| R9 | Chambers y otros (Sandia). *A phenomenological finite element model of stereolithography processing,* 1996. [OSTI](https://www.osti.gov/biblio/212696) | Elementos que se activan en el orden de curado, con un modelo viscoelástico; lo aplican al alabeo de una viga en voladizo | El mismo análisis estructural del motor (`structural_analysis.md`) puede predecir el alabeo si suma la contracción por capa |

## Ventosas y soportes

| # | Fuente | Qué aporta | Dónde sirve |
|---|---|---|---|
| R10 | Formlabs. *Cupping & Blowout* (documento técnico del fabricante, no un paper) | Al despegar, el aire encerrado en una ventosa baja de presión y empuja las paredes; si son finas, se hunden o revientan. Depende de la resina, el tamaño de la ventosa y el espesor de la pared | Regla 15 de `resin.md`: el aviso puede decir qué tan grave es según el tamaño del hueco y el espesor de la pared, no solo que existe |
| R11 | DTU. *Towards scarless support removal in vat photopolymerization additive manufacturing* (congreso; autores sin verificar) | Cuánto soporte hace falta de verdad en voladizos y bordes inclinados de menos de 45°: el error en Z bajó de 20 % a 6,9 %, con soporte en solo 1,51 % del área que lo pedía | `resin_supports.md`: menos puntas de soporte y marcas más chicas |

## Lo que esto permite mostrar «fielmente»

Con R1 y R3, el motor puede calcular, para cada capa, la dosis que recibe cada punto y cuánto cura de verdad. Con
eso, la vista previa de resina podría mostrar:
1. **Curado de más** bajo voladizos y huecos, en color, con su espesor en mm (R1 y R3).
2. **Borde real de cada capa:** escalones de píxel, grises y la luz que se escapa (R4 y R5).
3. **Fuerza de despegue por capa,** con las capas de riesgo marcadas (R6 y R7).
4. **Ventosas con su gravedad** (R10).

## Lo que más rinde para el motor, en orden

1. **R1 (curva de trabajo):** es la base de todo lo demás y mejora ya la torre de exposición y la corrección de Z.
2. **R3 (corregir el curado de más):** el defecto más visible en piezas con huecos y voladizos.
3. **R4 (máscara de iluminación):** una calibración nueva y barata que solo cambia las imágenes.
4. **R6 y R7 (despegue):** movimientos por capa y el gráfico de fuerza en la vista previa.
5. **R8 a R11:** avisos y soportes, más adelante.
