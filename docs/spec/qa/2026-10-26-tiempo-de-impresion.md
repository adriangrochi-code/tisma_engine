# Imprimir más rápido sin tocar la impresora (26-10-2026)

**Pedido del dueño:** técnicas, sacadas de papers y de lo que se puede medir, que bajen el tiempo de impresión sin
cambiar las velocidades ni las aceleraciones de la impresora: solo con lo que decide el laminador (qué se imprime, en
qué orden, con qué alto y ancho, por dónde viaja).

**Cómo se midió:** motor de `dev` en `5b0bdbd`, su tiempo estimado (`; estimated printing time`), capas de 0,2 mm,
las mismas velocidades en todos los casos (las de respaldo del motor). Benchy y conejo al 150 % de
`2026-10-26-velocidad.md`. Lo que se compara es la diferencia en %, no el tiempo absoluto.

## Lo que el motor ya tiene, medido

| Técnica | Benchy | Conejo al 150 % |
|---|---|---|
| Base (rectilíneo 20 %, 3 paredes) | 1 h 12 m | 18 h 4 m |
| Relleno combinado cada 2 capas (`infill.combine_layers` 2) | −5,0 % | −31,6 % |
| Relleno `lightning` | +0,4 % | −54,7 % |
| Relleno `support_cubic` | +0,4 % | −9,7 % |
| Relleno `adaptive_cubic` | +0,4 % | −8,7 % |
| 2 paredes en vez de 3 | −4,8 % | −6,3 % |
| Altura variable (`layers.cusp_height` 0,2 o 0,3; capas de 0,08 a 0,32 mm) | +7,5 % | 0 % |
| Capas fijas de 0,28 mm (referencia, pierde detalle) | −26,1 % | −26,9 % |

1. **El relleno es lo que más pesa en piezas grandes:** en el conejo, `lightning` ahorra más de la mitad.
2. **Combinar capas de relleno no se suma con `lightning`:** con los dos juntos, el conejo tarda lo mismo que solo con
   `lightning`, y el Benchy pierde el 5 % de combinar. A revisar: ¿se ignora `combine_layers` con ese patrón?
3. **La altura variable del motor solo afina:** con `cusp_height` 0,3 tarda lo mismo que con 0,2, y nunca usa capas
   más altas que `layers.height`, aunque la máquina acepte 0,32 mm. Donde las paredes son verticales podría usar capas
   gruesas sin perder detalle; ahí está casi todo el 26 % de las capas de 0,28.

**Viajes y arranques** (G-code de la tercera medición):

| G-code | Extrusión | Viajes | Viajes en % del recorrido | Arranques de extrusión | Retracciones |
|---|---|---|---|---|---|
| Benchy, motor | 121,2 m | 23,7 m | 16 % | 4 251 | 2 290 |
| Benchy, Tisma Slicer | 124,1 m | 29,1 m | 19 % | 4 132 | 3 026 |
| Conejo, motor | 2 994 m | 243,4 m | 8 % | 14 506 | 9 650 |
| Conejo, Tisma Slicer | 2 997 m | 189,6 m | 6 % | 11 479 | 14 232 |

4. En el conejo, el motor viaja un 28 % más que Tisma Slicer y arranca 26 % más veces. Cada arranque y cada viaje
   frena y acelera: con la misma aceleración, menos arranques es menos tiempo.

## Técnicas, por lo que pueden ahorrar

| # | Técnica | Qué hace | Fuente | Ahorro esperado |
|---|---|---|---|---|
| T1 | Altura variable que también engrosa | Capas gruesas (hasta `machine.max_layer_height`) donde las paredes son casi verticales; finas solo donde la pendiente lo pide | Sabourin, Houser y Bøhn (1996), «Adaptive slicing using stepwise uniform refinement», *Rapid Prototyping Journal*; Alexa, Hildebrand y Lefebvre (2017), «Optimal discrete slicing», *ACM TOG* | Hasta −26 % en piezas de paredes verticales (referencia de las capas de 0,28) |
| T2 | Paredes finas, relleno grueso | Paredes de afuera con la capa fina y relleno con capas de 2 o 3 veces más alto, siempre | Ya en el motor como `combine_layers`; falta que se combine con `lightning` y que la interfaz lo ofrezca por defecto | −5 a −32 % medido |
| T3 | Relleno según la pieza | `lightning` para piezas que solo se miran; relleno por tensiones del análisis (`infill.stress_density`) para las que cargan | Tricard y otros (2020), «Ribbed support vaults for 3D printing of hollowed objects», *Computer Graphics Forum*; Lu y otros (2014), «Build-to-last», *ACM TOG* | −10 a −55 % medido |
| T4 | Cordón más ancho en el relleno y las paredes de adentro | Con la misma velocidad en mm/s, un cordón de 0,6 mm con boquilla de 0,4 deja 50 % más de material por segundo y necesita un tercio menos de líneas; tope: `machine.max_flow` | Kuipers y otros (2020), «A framework for adaptive width control of dense contour-parallel toolpaths», *Computer-Aided Design* (anchos variables) | −15 a −30 % del tiempo de relleno (a medir) |
| T5 | Caminos continuos | Superficies y relleno en un solo trazo continuo (espirales de Fermat, zigzag conectado): menos arranques, frenadas y retracciones | Zhao y otros (2016), «Connected Fermat spirals for layered fabrication», *ACM TOG* | Pocos % en piezas con mucha superficie sólida (a medir) |
| T6 | Orden de recorrido | Ordenar islas, paredes y líneas como un problema del viajante para viajar menos | Ganganath y otros (2016), «Trajectory planning for 3D printing: a revisit to traveling salesman problem», ICCAR; Fok y otros (2019), «An ACO-based tool-path optimizer for 3-D printing applications», *IEEE TII* | El conejo viaja 28 % más que Tisma Slicer: llevarlo a ese nivel |
| T7 | Esquinas suavizadas y arcos | Redondear esquinas dentro de una tolerancia (por ejemplo 0,02 mm) y escribir arcos `G2`/`G3`: la impresora no frena del todo en cada vértice y no se queda sin comandos | Tajima y Sencer (2016), «Kinematic corner smoothing for high speed machine tools», *IJMTM*; `gcode_extras.md`, regla 10 (arcos, todavía no en el motor) | A medir; mayor en curvas finas |
| T8 | Filtrado del camino contra vibraciones | Corregir el camino en el G-code para que la impresora, con sus mismas aceleraciones, no deje marcas; así se pueden usar velocidades que hoy dan ondas | Duan, Yoon y Okwudire (2018), «A limited-preview filtered B-spline approach to tracking control», *Mechatronics* | Los autores reportan impresiones hasta 2 veces más rápidas con la misma calidad; necesita medir cada impresora (calibración de vibraciones) |
| T9 | Menos soporte | Orientación automática que minimiza soporte y altura; soporte en árbol | Ezair, Massarwi y Elber (2015), «Orientation analysis of 3D objects toward minimal support volume», *Computers & Graphics*; Vanek, Galicia y Benes (2014), «Clever support», *Computer Graphics Forum* | Depende de la pieza; el soporte fue casi la mitad del recorrido del cráneo de triceratops en Tisma Slicer (178 de 383 m) |
| T10 | Capas cortas sin esperar | Cuando una capa es más rápida que el tiempo mínimo, imprimir otra pieza de la placa en vez de frenar; avisar que conviene imprimir varias copias | `cooling.md` y `print_review.md` | Grande en placas con piezas chicas |

## Propuesta

En este orden, por ahorro y por lo poco que cambia la calidad:
1. **T1:** que la altura variable también use capas más altas que `layers.height`, hasta `machine.max_layer_height`,
   donde la pendiente lo permite.
2. **T2:** que `combine_layers` funcione con todos los patrones, `lightning` incluido, y que la interfaz lo sugiera.
3. **T3:** que la interfaz proponga el relleno según para qué es la pieza («para mirar» o «para usar»).
4. **T4:** ancho de cordón propio para el relleno y las paredes de adentro (`infill.width`, `walls.inner_width`),
   limitado por el flujo máximo.
5. **T6 y T5:** orden de recorrido y caminos continuos, con metas medidas en viajes y arranques.
6. **T7:** arcos y esquinas suavizadas (`gcode_extras.md`, regla 10).
7. **T8 y T10:** más adelante; T8 necesita una calibración de vibraciones por impresora.

Cada una con una prueba: el mismo modelo con y sin la técnica, el tiempo estimado y, para las que cambian la forma
impresa, las medidas de la pieza de prueba Mabel (`features/quality_model.md`).
