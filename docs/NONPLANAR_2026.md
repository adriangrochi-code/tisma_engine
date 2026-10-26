# Laminado no planar: lo nuevo de 2022 a 2026 y el camino para el motor

Pedido del dueño (2026-10-26): «buscá papers de impresión no planar para elegir la opción más óptima».

Este informe actualiza `docs/NONPLANAR_SLICING.md` (2026-10-25). No repite lo que ya está ahí (CurviSlicer, Reinforced
FDM, S³-Slicer y Neural Slicer en general). Suma lo que faltaba, sobre todo de 2022 a 2026, y compara las opciones
para este motor.

**Sala limpia:**
- se usaron artículos, resúmenes, páginas de editoriales, notas de prensa y documentación de usuario;
- ningún código, tampoco el que publican los artículos (QuickCurve, INF-3DP, S4 y otros) ni el de los laminadores;
- los métodos se cuentan con palabras propias.

**Calidad de los datos.** Cada trabajo lleva una marca:
- **artículo:** leído completo;
- **resumen:** solo el resumen o la ficha de la editorial;
- **nota:** prensa o un tercero que lo cita.

Antes de implementar algo marcado «resumen» o «nota», hay que leer el artículo completo.

## Resumen

1. **Lo más útil que salió es Double QuickCurve** (Ottonello, Hugron, Parmiggiani y Lefebvre; Eurographics 2025) y su
   versión anterior, QuickCurve (2024):
   - dos superficies de corte (una para las caras de arriba y otra para las de abajo), cada una como alturas sobre
     una grilla 2D;
   - cada una sale de un solo problema de mínimos cuadrados, parecido a una ecuación de Poisson;
   - las capas se interpolan entre las dos;
   - no necesita malla de tetraedros;
   - tarda de segundos a 5 minutos; CurviSlicer tardó de 2 a 46 veces más en sus ejemplos, o no terminó (con la
     orientación de los caminos optimizada, QuickCurve fue más lento en un caso).
2. **Encaja directo con el motor:**
   - el campo de capas sale de la grilla y se evalúa en los vértices de la malla;
   - el corte por conjuntos de nivel que ya existe (`src/slice/level_set.*`) hace el resto;
   - los caminos se calculan en XY y se suben a la capa (regla 5 de «Diseño para la visión»).
3. **En una impresora común la pendiente útil es chica, de 20 a 27°:**
   - más empinado, la cara plana de la boquilla raya el plástico, y la superficie queda peor que plana (Elkaseer et
     al. 2020: mejora solo hasta 25°);
   - la forma de la punta pesa más que cualquier otro ajuste (Bengtsson et al. 2024).
4. **El motor ya tiene Z anti-aliasing con subida,** que deja el error de la cara de arriba en micras
   (`docs/ZAA.md`). Lo que suman las capas curvas sobre eso:
   - la piel de arriba continua, sin los bordes de franja de cada capa;
   - menos soporte en voladizos que miran hacia afuera: −20 a −50 % de material (Nti et al. 2026);
   - cáscaras continuas, que podrían ser más fuertes, pero eso no está medido en 3 ejes.
5. **Resistencia en 3 ejes: hay poca evidencia, y es mixta.** Un estudio da +11 a +24 %; otro da retículas curvas más
   blandas que las planas. El modo `stress` va último, y solo si el análisis «como impresa» muestra ganancia.

**Camino recomendado** (detalle en la sección 4):

| Paso | Qué | Objetivo | Ganancia esperada |
|---|---|---|---|
| 0 | La base: campo en los vértices, caminos subidos a la capa, material según el espesor, tope de Z, control de choques con mapa de alturas, verificador y vista previa. Se prueba con `wave` | Que todo lo demás se apoye en algo probado | Las pruebas NP1 a NP4 de la ficha |
| 1 | `surface`: la superficie de arriba de Double QuickCurve, más el Z anti-aliasing para lo que queda | Caras de arriba poco inclinadas sin escalones ni bordes de franja | Rugosidad −34 a −76 % contra plano (tres estudios). Contra el Z anti-aliasing, sin medir: hay que medirlo |
| 2 | `overhangs`: la superficie de abajo, con la pendiente según la distancia a lo apoyado | Menos soporte en voladizos hacia afuera | −20 a −50 % de material (Nti et al. 2026); en la ficha, sin soporte de 48° a 55° |
| 3 | `automatic`: las dos superficies a la vez, interpoladas entre las alturas que las separan | Las dos ganancias juntas | La suma, menos lo que se pierda por los límites |
| 4 | `conical` y las formas fijas que faltan | Lo que pide la ficha | — |
| 5 | `stress`, con el análisis «como impresa» antes y después | Resistencia | Sin evidencia firme en 3 ejes; la ficha mide +31 % en el factor de seguridad (TE2) |

**Lo que no conviene ahora:** la optimización por volumen de CurviSlicer (tetraedros o vóxeles con un problema
cuadrático). Es más lenta, falla en varias piezas según las comparaciones de 2024 y 2025, y su ganancia extra en 3 ejes
no está medida.

## 1. Lo nuevo desde el documento anterior

### 1.1 Métodos de capas curvas en 3 ejes

| Trabajo | Qué hace | Límites que usa | Resultado | Marca |
|---|---|---|---|---|
| **QuickCurve** (Ottonello, Hugron, Parmiggiani, Lefebvre; arXiv 2406.03966, 2024) | Una superficie de corte, como alturas sobre una grilla 2D de unos 50 µm. Las incógnitas son las alturas libres de la grilla y un desplazamiento vertical por cada cara de arriba: así se copia la pendiente de cada cara, no su altura. Un solo sistema de mínimos cuadrados. Después, un barrido de arriba hacia abajo recorta las pendientes que pasan el tope. Los caminos siguen la curvatura principal, con un desfase de 45° por capa. Un cierre morfológico borra detalles chicos (0,2 mm en piezas mecánicas, 0,5 a 3 mm en terrenos) | Cono de la boquilla, sin el cuerpo del cabezal. Pendiente objetivo de 27° en los ejemplos | Auto: 19 s (39 s con la orientación) contra 872 s de CurviSlicer. Tapón: 52 s (153 s) contra 1943 s. CurviSlicer no dio resultado en tres modelos. La calidad se juzga solo con fotos, sin números | artículo |
| **Double QuickCurve** (los mismos autores; Eurographics 2025, artículo corto, CC BY) | Dos superficies: una ajustada a las caras de arriba y otra a las de abajo. Donde la pieza no tiene una cara para copiar, la superficie se «empina» para cortar la pared lo más de frente posible. Fuera de la pieza, plana. Un barrido (el tallado iterativo de Hornus y Lefebvre 2018) rellena lo que pase el tope. Si hay dos alturas que separan las caras de abajo de las de arriba, las capas se interpolan solo entre ellas; si no, de abajo hacia arriba. Las capas se eligen para que ninguna pase un espesor máximo τ; donde se comprimen, baja el material. Los soportes se cortan igual, de la cama hasta la superficie de abajo. Los caminos se calculan en XY y se suben a la capa | Pendiente máxima θmax en toda la superficie: así la boquilla no choca con la capa ni con lo de abajo | Ala: 1 min 43 s contra 3 min 12 s de CurviSlicer. Pájaro: 4 min 59 s; CurviSlicer falló a los 50 min. Compatible con soportes y rellenos comunes. Límite: con muchas caras apiladas a distintas alturas, dos superficies no alcanzan | artículo |
| **Capas planas y no planas mezcladas** (Shahid, arXiv 2411.07225, 2024) | Encuentra las caras que conviene curvar, hace sus capas curvas, las resta de la malla y corta el resto plano | — | Con la distancia de Chamfer, 8 veces más parecido al modelo que plano. La precisión cambia según la dirección de los cordones | resumen |
| **Capas no planares con espesor variable** (Pelzer y Hopmann, *Additive Manufacturing* 37, 2021) | Capas descritas por funciones; el material de cada punto se calcula para que el espesor cambie dentro de la capa, sin huecos entre capas curvas distintas. También cómo elegir la boquilla | — | Rugosidad −76 % en caras de arriba curvas; indicios de más resistencia | resumen |
| **Capas isotérmicas** (Shan, Shui, Hua, Mao; *J. Manufacturing Processes* 86, 2023) | Las capas como superficies de igual temperatura | — | +14 % de tracción, −63 % de rugosidad | resumen (del documento anterior) |
| **Optimización de FDM no planar** (Sebbe, Costa, Lucas, Pinto, Pedroso, da Silva, Sales-Contini; *CIRP J. Manuf. Sci. Tech.* 63, 2025) | Ángulo e inclinación de capa optimizados | — | Rugosidad −34 y −61,7 % según la variante; hasta +11 % en XY y +24 % en Z, menos con ángulos grandes | resumen |
| **Corte por topografía, sin soportes** (Nti, Tucker, Bambach, Wüthrich; *Int. J. Adv. Manuf. Tech.* 145, 2026) | Detecta las caras que cuelgan; corta la pieza en tramos de altura; en una grilla 2D mide la distancia a la huella apoyada; la deformación crece con esa distancia, con una inclinación de 15 a 25°, es cero dentro de lo apoyado y se suaviza; deforma la malla en Z, la corta plana y devuelve los caminos; frena cerca de las caras que cuelgan | 15 a 25°. Boquilla Volcano larga, PLA a 210 °C | Material total: −20 % (ménsula de tubo y conejo) y −50 % (codo de tubo). Ra 93,6 → 55,7 µm sobre una línea. Desvíos de hasta −1,03 mm en zonas grandes sin apoyo, por alabeo, no por el algoritmo. Más tiempo de impresión por el frenado | artículo (preimpresión) |
| **Factibilidad y superficie en capas curvas** (Srikanth, Mathew, Bhagchandani; *Results in Engineering* 28, 2025) | Impresora de 3 ejes con una boquilla de aerógrafo | — | Mejor superficie en piezas cóncavas y convexas hasta 55° de inclinación. Más allá hace falta multieje; la boquilla raya | nota |
| **Diseño y ensayo en 3 ejes** (Bengtsson, Nordin, Tavčar; *Proc. Design Society* 4, 2024) | Boquilla 5 mm más larga (10 mm expuestos) y una punta con un segundo ángulo de 30° que achica la cara plana de 1,0 a 0,6 mm. Diseño de experimentos completo: 32 piezas, 5 factores | Pieza con pendientes hasta 45° | Lo que más pesa es la forma de la punta; después, el ángulo de los cordones de la superficie (0° fue mejor); después, la temperatura (220 °C fue mejor). La altura de capa no tuvo efecto claro | artículo |
| **Rugosidad según la inclinación** (Elkaseer, Müller, Rabsch, Scholz; KES-SDM 2020, Springer) | Pirámides con distintas inclinaciones, plano contra no planar | Boquilla común | La rugosidad baja hasta 25°; con más inclinación queda peor que plano, porque la boquilla aplasta los cordones vecinos | resumen |
| **Techos no planares** (Ahlers, Wasserfall, Hendrich, Zhang; IEEE CASE 2019) | Ya citado. Datos que faltaban: el modelo de choque y el umbral de ganancia (ver 1.4 y sección 2) | Ultimaker 2: 45° hasta 7,5 mm solo con la boquilla; 8° hasta 50 mm con todo el cabezal | Ala: mucho más lisa y un poco más rápida. Laminar: 213 s contra 7 s en plano (32 554 caras) | artículo |
| **Laminado cónico en 3 ejes** (CNC Kitchen, nota de usuario) | Deformar, laminar plano y volver | 16 a 20° en una Prusa Mini | Voladizos hacia afuera sin soporte. Los que miran hacia adentro necesitan **más** soporte. Hacia adentro y hacia afuera a la misma altura: sin solución | nota |

### 1.2 Resistencia en 3 ejes: lo medido

| Trabajo | Qué se midió | Resultado | Marca |
|---|---|---|---|
| Sebbe et al. 2025 | Tracción | Hasta +11 % en XY y +24 % en Z; baja con ángulos grandes | resumen |
| Shan et al. 2023 | Tracción | +14 % | resumen |
| Singamneni et al. 2010 | Flexión | +40 % de carga antes de romper | nota (lo cita una revisión; sin verificar) |
| Pérez-Castillo et al., *Materials* 16(9), 2023 | Flexión de retículas impresas sobre un molde curvo (±2,5 mm) en una Ender 5 Pro | **Las curvas fueron más blandas:** las planas, hasta un 64 % más rígidas. Los elementos curvos se enderezan primero | artículo |
| Ewougsi Tekeu et al., *Progress in Additive Manufacturing* 11, 2026 | Modelo de elementos finitos armado desde el G-code, con anisotropía, contra correlación de imágenes | Coincide en PLA. En probetas no planas la fuerza de la máquina de ensayo engaña (tracción con flexión); la correlación de imágenes sí sirve | resumen |
| ZigZagZ (Allum et al. 2021), ya en `docs/NONPLANAR_INFILL.md` | Tracción en Z con cordones que suben y bajan | +62 % | artículo |

**En 5 ejes la ganancia sí está medida:**
- Neural Slicer (Liu et al. 2024): +30,6 % de carga de rotura en la cabeza del conejo con el objetivo de resistencia
  (contra sin él), y +101,9 % en el puente contra S³-Slicer;
- Reinforced FDM llega a 6,35 veces.

**Conclusión:** en 3 ejes, la pendiente máxima (unos 20 a 25°) deja poca inclinación para alinear las capas con la
carga. La ganancia de resistencia es chica o no está probada. El trabajo de Ewougsi Tekeu et al. muestra cómo medirla:
análisis desde el G-code real, que es el N3 de `docs/NONPLANAR_INFILL.md`.

### 1.3 Superficie: lo medido

| Trabajo | Medida | Plano | No planar | Marca |
|---|---|---|---|---|
| CurviSlicer 2019 | Error de volumen, 40 capas | 270 mm³ (149 con altura adaptativa) | 57 mm³ | artículo (documento anterior) |
| Pelzer y Hopmann 2021 | Rugosidad de caras curvas de arriba | — | −76 % | resumen |
| Shan et al. 2023 | Rugosidad | — | −63 % | resumen |
| Sebbe et al. 2025 | Rugosidad | — | −34 a −61,7 % | resumen |
| Nti et al. 2026 | Ra sobre una línea | 93,6 µm | 55,7 µm | artículo |
| Shahid 2024 | Distancia de Chamfer al modelo | 8 veces la del no planar | Referencia | resumen |
| Elkaseer et al. 2020 | Rugosidad según la inclinación | — | Mejor hasta 25°; peor arriba | resumen |
| Ficha `nonplanar_modes.md`, SU1 y SU2 (Tisma Slicer) | Error de la cara de arriba, techo de 5,7° | 0,057 mm | 0,010 mm (con Z anti-aliasing que solo baja y capas planas: 0,039) | ficha |
| Motor, `docs/ZAA.md` | Error medio en la cúpula | 0,049 mm | Z anti-aliasing con subida: 0,006 mm (sin capas curvas) | medido |

**Lo que dicen juntos:**
- en caras poco inclinadas, las capas curvas bajan la rugosidad entre un 34 y un 76 %;
- el Z anti-aliasing del motor ya baja el error geométrico casi lo mismo, con capas planas;
- la diferencia que queda es de aspecto y de continuidad: con capas curvas, la piel de arriba es una sola capa
  continua; con Z anti-aliasing, cada capa deja su franja, con sus bordes;
- eso no está medido en ningún artículo: lo tiene que medir el banco propio (sección 6).

### 1.4 Sin modelo de choque contra con modelo de choque

| Nivel | Qué mira | Quién lo usa | Qué garantiza | Qué no |
|---|---|---|---|---|
| **Forma fija con un tope de pendiente** | Nada de la pieza | Ondas y cono simples; CNC Kitchen | Nada: depende de elegir bien la pendiente | Choques con lo ya impreso y con el cuerpo del cabezal |
| **Cono local** | La pendiente de la propia superficie de corte | CurviSlicer, QuickCurve, Double QuickCurve | La punta no choca con la capa que imprime ni con las de abajo: todas quedan debajo de la superficie | El cuerpo del cabezal, el ventilador, el sensor |
| **Dos parámetros, verificados capa a capa** | Lo ya impreso, capa por capa | Ahlers 2019 | Sin choques del cono hasta la altura dada. Para verificar, un «colisionador» que crece en cada capa lo que el cono avanza en un espesor de capa. Las caras que chocan se imprimen planas | Cabezales asimétricos |
| **Perfil del cabezal sobre un mapa de alturas** | Lo ya impreso como alturas en XY, contra el ángulo libre a cada altura (X e Y distintos) | La ficha (`head.profile`, regla 6 de `nonplanar.md`) | Lo mismo, con el cabezal real medido | Lo que la medición no ve (un sensor que baja, el carro) y el error de la medición |
| **Campo de distancia 3D que cambia en el tiempo** | La pieza que crece y todos los puntos del cabezal | INF-3DP (Qu et al. 2025), multieje | Choques globales | Costo; red neuronal |

**Lazo cerrado:** no encontré ningún trabajo de 3 ejes que detecte choques mientras imprime. Todos los evitan al
laminar.

**Viajes:** Ahlers sube la boquilla hasta la altura máxima ya impresa en todo viaje de más de dos anchos de línea. Los
más cortos van directo.

### 1.5 Tiempo de cálculo y GPU

| Método | Cómo resuelve | Tiempo publicado | Para el motor |
|---|---|---|---|
| CurviSlicer 2019 | Problema cuadrático en tetraedros, por iteraciones | 2 a 32 min en los ejemplos de QuickCurve y Double QuickCurve; falla en varios | Caro; difícil de pasar a GPU |
| QuickCurve 2024 y Double QuickCurve 2025 | Mínimos cuadrados en una grilla 2D, en CPU | 19 s a 5 min | Una ecuación tipo Poisson en una grilla: el multigrid propio (`src/fea/multigrid.*`) sirve en CPU, y en Vulkan es un núcleo regular |
| Neural Slicer 2024 | Dos redes chicas, entrenadas en GPU (RTX 4080) | Menos de 15 min | Para F9 |
| INF-3DP 2025 | Campos implícitos en GPU (RTX 4090) | Puntos de paso en 12,9 s contra 826,4 s de S³-Slicer (64 veces); error de 0,019 mm contra 0,26 | Para F9 |

### 1.6 Multieje (para F9)

- **INF-3DP** (Qu, Huang, Guo, Sun, Lyu, Dai, Yam, Fang; arXiv 2509.05345, 2025):
  - la pieza como campo de distancia con signo;
  - un campo guía con los objetivos (sin soporte, superficie, extrusión);
  - la secuencia y la orientación se optimizan juntas;
  - los choques, contra la pieza que crece, vista como un campo que cambia en el tiempo;
  - límite: puede sacrificar la continuidad de la impresión para no chocar.
- **Campos implícitos para capas y caminos** (Dutta et al., arXiv 2511.17578, 2025): capas y caminos con redes de
  activación senoidal y choques explícitos (resumen).
- **Fibra continua en capas curvas:** ya citada como preimpresión; salió en *Additive Manufacturing* 82 (2024) 104048.
- **S4** (Joshua Bird, nota de usuario): deforma la pieza con tetraedros según el camino más corto a la cama. Es para
  su impresora de 4 ejes, no para 3.

## 2. Qué limita en una impresora común

### 2.1 La pendiente

| Fuente | Máquina o boquilla | Pendiente usada o útil |
|---|---|---|
| Ahlers 2019 | Ultimaker 2 | 45° solo la boquilla, hasta 7,5 mm de alto; 8° con todo el cabezal, hasta 50 mm |
| CurviSlicer 2019 | Boquilla en punta | 30° |
| QuickCurve 2024 | Delta con boquilla larga | 27° de objetivo |
| Elkaseer et al. 2020 | Boquilla común | Gana hasta 25°; pierde arriba |
| Nti et al. 2026 | Volcano larga | 15 a 25° |
| CNC Kitchen | Prusa Mini, cono | 16 a 20° |
| Srikanth et al. 2025 | Boquilla de aerógrafo | Hasta 55° |
| Ficha `nonplanar.md` | Sin medir | `curved.max_slope` 20° |

**El umbral de ganancia** (Ahlers 2019): si los cordones van a lo largo de las curvas de nivel, el escalón entre dos
cordones vecinos es el ancho por la tangente de la pendiente. Ese escalón es menor que el de una capa plana solo si la
pendiente está debajo de `arctan(altura de capa / ancho)`:

| Altura de capa (ancho 0,45 mm) | Umbral |
|---|---|
| 0,1 mm | 12,5° |
| 0,2 mm | 24,0° |
| 0,3 mm | 33,7° |

Coincide con el 25° de Elkaseer et al. con capas de 0,2.

**La cara plana de la boquilla** (cuenta propia, con la idea de Ahlers y el resultado de Bengtsson). La cara queda
horizontal. Sobre una pendiente θ, su borde a la distancia R del orificio queda R·tan θ por encima o por debajo de la
superficie:

| Cara plana | 15° | 20° | 25° | 30° |
|---|---|---|---|---|
| 1,0 mm (R = 0,5) | 0,13 mm | 0,18 mm | 0,23 mm | 0,29 mm |
| 0,6 mm (R = 0,3) | 0,08 mm | 0,11 mm | 0,14 mm | 0,17 mm |

Con capas de 0,2 mm, una boquilla común pasa una capa entera de interferencia cerca de los 22°. Por eso:
- la punta pesa tanto en el estudio de Bengtsson;
- conviene un ajuste nuevo de la impresora, el diámetro de la cara plana, que acote la pendiente junto con los
  ángulos libres;
- el orden importa: hay que imprimir primero los cordones más bajos, como ya hace el Z anti-aliasing (el grafo de
  orden de `docs/ZAA.md`), así la cara nunca baja sobre un vecino más alto.

### 2.2 La dirección de los cordones

- **A lo largo de las curvas de nivel:** cada cordón queda plano, y el escalón pasa al costado (umbral de arriba).
- **Subiendo y bajando la pendiente:** el orificio se ve como una elipse; el cordón queda cóncavo o convexo según suba
  o baje (Ahlers 2019).
- QuickCurve alinea con la curvatura máxima en las zonas empinadas y con la mínima en las suaves. Bengtsson encontró
  que el ángulo de los cordones de la superficie es el segundo factor.
- El documento del Z anti-aliasing ya proponía orientar las líneas de arriba por las curvas de nivel.

**Conclusión:** la dirección de la piel de arriba es una elección por zona, y tiene que entrar en el banco de pruebas.

### 2.3 El espesor

- La ficha pide `J` entre 0,6 y 1,4: el espesor real entre 0,6 y 1,4 veces el de laminado.
- También valen los topes de la máquina, `machine.min_layer_height` y `machine.max_layer_height`
  (`docs/VARIABLE_LAYERS.md`).
- Double QuickCurve:
  - elige las capas para que ninguna pase un espesor máximo;
  - donde se comprimen, baja el material;
  - si se comprimen demasiado, propone quitar capas por zonas (Ezair, Fuhrmann y Elber 2018).
- Pelzer y Hopmann calculan el material punto por punto para que el espesor pueda cambiar dentro de la capa.

### 2.4 El eje Z y los viajes

- La boquilla sube y baja dentro de la capa. Muchas impresoras con husillo no pasan de 10 a 20 mm/s en Z.
- El motor ya limita eso con `machine.max_feed_z`; el relleno ondulado tardó un 85 % más por eso.
- Una pendiente de 20° pide en Z el 36 % del avance; a 60 mm/s, 22 mm/s.
- Los viajes suben sobre lo más alto ya impreso (Ahlers).

## 3. Las opciones para este motor

| Opción | Qué da | Ganancia medida | Costo y riesgo | Con el campo escalar | Veredicto |
|---|---|---|---|---|---|
| **A. Z anti-aliasing** (ya está) | Caras de arriba a micras, capas planas | Error de la cúpula 0,049 → 0,006 mm | Ninguno nuevo | No lo necesita | Se queda, y se combina con las demás |
| **B. Techos curvos de Ahlers** (solo las últimas capas siguen la cara) | Piel continua en caras simples | Ala más lisa, mismo tiempo | Corte especial por cara; falla en caras apiladas | Encaja, pero es un caso particular de C | Queda dentro de C |
| **C. Superficies base en grilla 2D** (Double QuickCurve) | Arriba y abajo de la pieza, en un solo marco | Rugosidad −34 a −76 % (otros métodos con el mismo fin); de 2 a 46 veces más rápido que CurviSlicer | Dos superficies no alcanzan si hay muchas caras apiladas | El campo sale de la grilla y del interpolado; se evalúa en los vértices | **La base de F7** |
| **D. Campo por volumen** (CurviSlicer) | Curvas en toda la pieza | Error de volumen −62 % contra la mejor altura adaptativa | Lento; falla en varias piezas; un solucionador cuadrático | Encaja, en la grilla de vóxeles | Más adelante, si C se queda corto |
| **E. Formas fijas** (`wave`, `conical`) | Lo que pide la ficha | Las pruebas de la ficha | Sin modelo de la pieza; el cono empeora los voladizos hacia adentro | Fórmula cerrada | La primera prueba de la base (`wave`); `conical` después |
| **F. Voladizos según la distancia** (ficha `overhangs`; Nti et al. 2026) | Menos soporte | −20 a −50 % de material | Alabeo en zonas grandes; más tiempo | Es la superficie de abajo de C | Paso 2 |
| **G. Tensiones** (ficha `stress`) | Resistencia | En 3 ejes, poca y mixta | Se puede perder rigidez | Encaja | Último, y medido |
| **H. Isotérmico** (Shan et al. 2023) | Superficie y tracción | +14 %, −63 % | Necesita simular el calor | Encaja | No ahora: C da lo mismo con menos |
| **I. Redes neuronales** (Neural Slicer, INF-3DP) | Multieje | Grandes en 5 ejes | Entrenar en GPU | Encaja | F9 |

## 4. Recomendación

### Paso 0 — La base, probada con `wave`

Antes de ningún modo nuevo, la parte común de todos:
- **el campo:** un valor por vértice, que el corte por conjuntos de nivel usa en lugar de z;
- **los caminos:** perímetros y rellenos en XY, como hoy, y después subidos a la capa, partidos cada
  `curved.segment_length`;
- **el material:** cada tramo por el espesor local `J`;
- **el eje Z:** la velocidad del tramo, limitada por `machine.max_feed_z`;
- **el control de choques:** lo ya impreso como mapa de alturas en XY, y en cada punto del camino el perfil del cabezal
  de la ficha (`head.profile`, ángulos en X y en Y). Sirve dos veces:
  - al laminar, para recortar la deformación;
  - en el verificador, para rechazar un G-code que choca.

  Es una operación de mapa de bits, fácil de pasar a Vulkan;
- **los viajes:** por encima de lo más alto ya impreso;
- **la vista previa y el verificador:** cordones con su espesor real, como con el Z anti-aliasing.

Se prueba con `wave`: su fórmula es cerrada y la ficha da números exactos (NP1 a NP4).

### Paso 1 — `surface` con la superficie de arriba de Double QuickCurve

- **Qué:** la superficie de arriba de la pieza, como alturas en una grilla 2D. Sale de mínimos cuadrados con estas
  pistas:
  - en las caras de arriba, la misma pendiente que la cara, con un desplazamiento libre por cara;
  - fuera de la pieza, plana;
  - dentro de la pieza y sin cara para copiar, suave, como pide la ficha. El «empinado» de Double QuickCurve (cortar
    las paredes lo más de frente posible) queda como opción, a medir.

  Después, un barrido recorta lo que pase el tope de pendiente.
- **La ficha manda en el comportamiento:**
  - los pesos por área y por lo horizontal de la cara (regla 14);
  - plana bajo `curved.flat_below` y la transición en toda la altura (reglas 15 a 18; prueba SU3);
  - el artículo da el método de cálculo, no los números.
- **El tope:** el menor entre el ángulo libre medido, el que permite la cara plana de la boquilla (2.1) y el umbral de
  ganancia (2.1). Con una impresora sin medir, unos 20°, como pide la ficha.
- **El Z anti-aliasing encima:** donde la superficie no pudo seguir la cara (pendiente recortada), el Z anti-aliasing
  corrige lo que queda dentro de la capa. La ficha mide los dos por separado (SU2 y SU4).
- **El orden:** de abajo hacia arriba, con el grafo del Z anti-aliasing.
- **La dirección de la piel de arriba:** a lo largo de las curvas de nivel, o según la curvatura. Se elige con el
  banco.
- **Cómo encaja:** el valor del campo en un punto es la altura de laminado `s` de la capa que pasa por él: su z menos
  la deformación de la ficha en ese punto (`s = z − D`). La deformación sale de la superficie de la grilla y de la
  rampa. Se evalúa en cada vértice y el corte hace el resto. El campo crece con z mientras `J` sea positivo, así cada
  capa es una sola superficie.
- **GPU:** la ecuación tipo Poisson con el multigrid propio en CPU como referencia; después, el mismo multigrid en
  Vulkan, probado contra la CPU.
- **Ganancia esperada:**
  - contra plano, la rugosidad de las caras poco inclinadas, entre un 34 y un 76 % menos;
  - contra el Z anti-aliasing con subida, sobre todo el aspecto (una piel continua). El banco tiene que decir cuánto.

### Paso 2 — `overhangs` con la superficie de abajo

- **Qué:** las reglas 9 a 13 de `nonplanar_modes.md`. La deformación crece con la distancia a la base del voladizo.
  Es lo mismo que hace Nti et al. 2026: una grilla 2D con la distancia a la huella apoyada.
- **Lo que hace falta:** la distancia exacta en una grilla (Felzenszwalb y Huttenlocher, citado en la ficha) y, en
  GPU, la transformada de distancia que propone `docs/RESEARCH_MAP.md` (Q11). Sirve también para los frentes de los
  arcos en voladizos (`ARC_OVERHANGS.md`).
- **Lo que suma el artículo:**
  - frenar cerca de las caras que cuelgan;
  - el alabeo en zonas grandes, que limita lo que se puede esperar.
- **Ganancia esperada:** −20 a −50 % de material en piezas con voladizos hacia afuera; en la ficha, los hongos de 48 a
  55° sin soporte con el cabezal de 25° (VO4).
- **Riesgo:** los voladizos que miran hacia adentro no ganan. La ficha ya trata cada zona por separado, en vez de un
  cono para toda la pieza.

### Paso 3 — `automatic` con las dos superficies

- La ficha dice: la suma de las dos deformaciones; si pasa un límite, se achica solo la de arriba (reglas 19 y 20).
- Double QuickCurve suma una idea mejor para piezas con una altura que separa las caras de abajo de las de arriba:
  interpolar solo entre esas alturas, así la superficie de arriba llega entera a las caras de arriba.
- Se prueba como opción, contra la regla de la ficha, con AU1 a AU3.

### Paso 4 — `conical` y lo que falte de la ficha

Las reglas 3 a 8 de `nonplanar_modes.md`, sobre la misma base.

### Paso 5 — `stress`, medido

- La ficha ya define el cálculo (reglas 21 a 23).
- Antes de darlo por bueno, el análisis «como impresa» con la unión entre capas inclinada (N3), armado desde el G-code,
  como Ewougsi Tekeu et al. 2026.
- Si no gana en las piezas del banco, queda como experimental.

### Valores de partida

| Qué | Valor | De dónde |
|---|---|---|
| Pendiente máxima sin medir | 20° | Ficha; cerca del umbral de 24° con capas de 0,2 |
| Pendiente con boquilla en punta o larga y cabezal medido | Hasta 27–30° | QuickCurve, CurviSlicer |
| `J` | 0,6 a 1,4 | Ficha |
| Grilla de las superficies | 0,1 mm para empezar (los artículos usan 0,05) | Se mide el tiempo |
| Detalles chicos | Cierre morfológico de 0,2 mm | QuickCurve |
| Viajes | Por encima de lo más alto impreso, salvo los de menos de dos anchos | Ahlers 2019 |

## 5. Riesgos

| Riesgo | Por qué | Qué hacer |
|---|---|---|
| **Raspar** | Arriba de unos 22–25° con boquilla común, la cara plana aplasta los cordones vecinos (Elkaseer; cuenta de 2.1) | Tope por la cara plana; orden de abajo hacia arriba |
| **Choques con el cabezal** | Ventiladores y sensores cuelgan cerca de la punta; los ángulos libres de las impresoras comunes no se publican | El perfil medido de la ficha; el verificador con el mapa de alturas |
| **Tiempo** | El eje Z lento | Tope de Z; el estimador lo cuenta |
| **Espesor fuera de rango** | Capas comprimidas donde la superficie cambia rápido | `J` en [0,6; 1,4]; quitar capas por zonas si hace falta |
| **Alabeo** | Zonas grandes sin apoyo (Nti et al. 2026) | Avisar; no prometer medidas exactas en esas zonas |
| **Resistencia** | Puede bajar la rigidez (Pérez-Castillo et al. 2023) | `stress` solo con el análisis antes y después |
| **G-code grande** | Tramos cortos | Volver a unir los tramos rectos (la ficha ya lo pide) |
| **Patentes** | No se revisaron para las capas curvas en 3 ejes | Revisión de un abogado antes de vender, como lo demás |

## 6. Cómo medirlo

**En el motor, sin imprimir:**
- **la cara de arriba:** el error contra la superficie exacta, como en `docs/ZAA.md` (cúpula, esfera y los bloques
  con techo de la ficha), en cuatro modos: plano, Z anti-aliasing, `surface`, y `surface` con Z anti-aliasing;
- **los bordes de franja:** cuántos bordes de capa cortan cada cara de arriba (con capas curvas debería ser casi
  ninguno). Es la medida de aspecto que el Z anti-aliasing no mejora;
- **el soporte:** filamento de soporte e interfaz en los hongos de la ficha, como VO1 a VO7;
- **choques:** cero en el verificador, con el perfil de cabezal por defecto y con el de 25°;
- **el tiempo:** de laminado y de impresión estimado (con el tope de Z);
- **la GPU:** el multigrid en Vulkan contra la CPU, en tiempo y en diferencia máxima.

**Impreso (lo hace el dueño):**
- rugosidad de los techos de 5, 10, 15 y 20° en los cuatro modos;
- la pendiente máxima que no raspa en su impresora, con su boquilla: una pieza con techos de 10 a 35°, cada 5°;
- el soporte que sobra en los hongos;
- para `stress`, probetas en Z planas y curvas, medidas con correlación de imágenes si se puede (Ewougsi Tekeu et al.).

## Fuentes

**Capas curvas en 3 ejes:**
- Ottonello, E., Hugron, P.-A., Parmiggiani, A., Lefebvre, S., *QuickCurve: revisiting slightly non-planar 3D
  printing*, preimpresión, 2024, [arXiv 2406.03966](https://arxiv.org/abs/2406.03966).
- Ottonello, E., Hugron, P.-A., Parmiggiani, A., Lefebvre, S., *Double QuickCurve: revisiting 3-axis non-planar 3D
  printing*, Eurographics 2025, artículos cortos, DOI 10.2312/egs.20251052,
  [Eurographics](https://diglib.eg.org/handle/10.2312/egs20251052), [HAL hal-05466215](https://inria.hal.science/hal-05466215v1).
- Shahid, S. T., *Modeling of non-planar slicer for improved surface quality in material extrusion 3D printing*,
  preimpresión, 2024, [arXiv 2411.07225](https://arxiv.org/abs/2411.07225).
- Pelzer, L., Hopmann, C., *Additive manufacturing of non-planar layers with variable layer height*, Additive
  Manufacturing 37 (2021) 101697, [DOI 10.1016/j.addma.2020.101697](https://doi.org/10.1016/j.addma.2020.101697).
- Shan, Y., Shui, Y., Hua, J., Mao, H., *Additive manufacturing of non-planar layers using isothermal surface
  slicing*, Journal of Manufacturing Processes 86 (2023) 326–335,
  [DOI 10.1016/j.jmapro.2022.12.054](https://doi.org/10.1016/j.jmapro.2022.12.054).
- Sebbe, N. P. V., Costa, J. P. M. M., Lucas, R. R., Pinto, A. M. G., Pedroso, A. F. V., da Silva, F. J. G.,
  Sales-Contini, R. C. M., *Improving surface quality and mechanical strength through the optimization and
  enhancement of Fused Deposition Modelling (FDM) for non-planar 3D printing*, CIRP Journal of Manufacturing Science
  and Technology 63 (2025) 375–392, [DOI 10.1016/j.cirpj.2025.10.003](https://doi.org/10.1016/j.cirpj.2025.10.003).
- Nti, J., Tucker, M., Bambach, M., Wüthrich, M., *Automated topography slicing for support-free 3-axis FDM*, The
  International Journal of Advanced Manufacturing Technology 145 (2026) 7423–7446,
  [DOI 10.1007/s00170-026-18768-1](https://doi.org/10.1007/s00170-026-18768-1); preimpresión en
  [Research Square, DOI 10.21203/rs.3.rs-9354022/v1](https://doi.org/10.21203/rs.3.rs-9354022/v1).
- Srikanth, M., Mathew, A. T., Bhagchandani, R. K., *Evaluation of printing feasibility and surface quality in curved
  layer fused deposition modeling of complex geometries*, Results in Engineering 28 (2025) 108271,
  [DOI 10.1016/j.rineng.2025.108271](https://doi.org/10.1016/j.rineng.2025.108271).
- Bengtsson, S., Nordin, A., Tavčar, J., *Design and evaluation of non-planar material extrusion on a 3-axis
  printer*, Proceedings of the Design Society 4 (DESIGN 2024) 1727–1736,
  [DOI 10.1017/pds.2024.175](https://doi.org/10.1017/pds.2024.175).
- Elkaseer, A., Müller, T., Rabsch, D., Scholz, S. G., *Impact of Nonplanar 3D Printing on Surface Roughness and
  Build Time in Fused Filament Fabrication*, Sustainable Design and Manufacturing 2020 (KES-SDM), Smart Innovation,
  Systems and Technologies 200, Springer, 2020, 285–295,
  [DOI 10.1007/978-981-15-8131-1_26](https://doi.org/10.1007/978-981-15-8131-1_26)
  ([ficha del KIT](https://publikationen.bibliothek.kit.edu/1000123922)).
- Ahlers, D., Wasserfall, F., Hendrich, N., Zhang, J., *3D Printing of Nonplanar Layers for Smooth Surface
  Generation*, IEEE CASE 2019, 1737–1743, [DOI 10.1109/COASE.2019.8843116](https://doi.org/10.1109/COASE.2019.8843116)
  ([PDF](https://tams-www.informatik.uni-hamburg.de/publications/2019/case_ahlers_2019.pdf)).
- Etienne, J., Ray, N., Panozzo, D., Hornus, S., Wang, C. C. L., Martínez, J., McMains, S., Alexa, M., Wyvill, B.,
  Lefebvre, S., *CurviSlicer: Slightly curved slicing for 3-axis printers*, ACM TOG 38(4), 2019,
  [DOI 10.1145/3306346.3323022](https://doi.org/10.1145/3306346.3323022).
- Hornus, S., Lefebvre, S., *Iterative carving for self-supporting 3D printed cavities*, Eurographics 2018,
  artículos cortos (citado por Double QuickCurve; no leído).
- Ezair, B., Fuhrmann, S., Elber, G., *Volumetric covering print-paths for additive manufacturing of 3D models*,
  Computer-Aided Design 100 (2018) 1–13 (citado por Double QuickCurve; no leído).
- Chermain, X., Zanni, C., Martínez, J., Hugron, P.-A., Lefebvre, S., *Orientable Dense Cyclic Infill for Anisotropic
  Appearance Fabrication*, ACM TOG 42(4), 2023, [DOI 10.1145/3592412](https://doi.org/10.1145/3592412) (la orientación
  de QuickCurve; no leído).
- Nayyeri, P., Zareinia, K., Bougherara, H., *Planar and nonplanar slicing algorithms for fused deposition modeling
  technology: a critical review*, The International Journal of Advanced Manufacturing Technology 119 (2022)
  2785–2810, [DOI 10.1007/s00170-021-08347-x](https://doi.org/10.1007/s00170-021-08347-x) (revisión; solo el
  resumen).
- Song, H.-C., Ray, N., Sokolov, D., Lefebvre, S., *Anti-aliasing for fused filament deposition*, Computer-Aided
  Design 89 (2017) 25–34, [DOI 10.1016/j.cad.2017.04.001](https://doi.org/10.1016/j.cad.2017.04.001).

**Resistencia:**
- Pérez-Castillo, J. L., Mora, A., Perez-Santiago, R., Roman-Flores, A., Ahmad, R., Cuan-Urquizo, E., *Flexural
  Properties of Lattices Fabricated with Planar and Curved Layered Fused Filament Fabrication*, Materials 16(9)
  (2023) 3451, [DOI 10.3390/ma16093451](https://doi.org/10.3390/ma16093451).
- Ewougsi Tekeu, C.-L., Tröger, J.-A., Afshari, A., Inkermann, D., Hartmann, S., *Toward numerical modeling and
  experimental full-field validation of non-planar 3D-printed components*, Progress in Additive Manufacturing 11
  (2026) 4657–4675, [DOI 10.1007/s40964-026-01598-8](https://doi.org/10.1007/s40964-026-01598-8).
- Singamneni, S., Diegel, O., Huang, B., Gibson, I., Chowdhury, R., *Curved-layer fused deposition modelling*,
  Journal for New Generation Sciences 8(2) (2010) 95–107 ([AUT](https://hdl.handle.net/10292/1712)); el +40 % sale de
  una revisión que lo cita.
- Allum, J., Kitzinger, J., Li, Y., Silberschmidt, V. V., Gleadall, A., *ZigZagZ: Improving mechanical performance in
  extrusion additive manufacturing by nonplanar toolpaths*, Additive Manufacturing 38 (2021) 101715,
  [DOI 10.1016/j.addma.2020.101715](https://doi.org/10.1016/j.addma.2020.101715).

**Multieje y GPU:**
- Liu, T., Zhang, T., Chen, Y., Huang, Y., Wang, C. C. L., *Neural Slicer for Multi-Axis 3D Printing*, ACM TOG 43(4),
  2024, [DOI 10.1145/3658212](https://doi.org/10.1145/3658212) ([arXiv 2404.15061](https://arxiv.org/abs/2404.15061)).
- Qu, J., Huang, Z., Guo, D., Sun, H., Lyu, A., Dai, C., Yam, Y., Fang, G., *INF-3DP: Implicit Neural Fields for
  Collision-Free Multi-Axis 3D Printing*, preimpresión, 2025, [arXiv 2509.05345](https://arxiv.org/abs/2509.05345).
- Dutta et al., *Implicit Neural Field-Based Process Planning for Multi-Axis Manufacturing: Direct Control over
  Collision Avoidance and Toolpath Geometry*, preimpresión, 2025, [arXiv 2511.17578](https://arxiv.org/abs/2511.17578)
  (solo el resumen).
- Zhang, T., Fang, G., Huang, Y., Dutta, N., Lefebvre, S., Kilic, Z. M., Wang, C. C. L., *S³-Slicer: A General Slicing
  Framework for Multi-Axis 3D Printing*, ACM TOG 41(6), 2022,
  [DOI 10.1145/3550454.3555516](https://doi.org/10.1145/3550454.3555516).
- Fang, G., Zhang, T., Huang, Y., Zhang, Z., Masania, K., Wang, C. C. L., *Exceptional mechanical performance by
  spatial printing with continuous fiber: Curved slicing, toolpath generation and physical verification*, Additive
  Manufacturing 82 (2024) 104048, [DOI 10.1016/j.addma.2024.104048](https://doi.org/10.1016/j.addma.2024.104048).

**Notas y documentación de usuario:**
- CNC Kitchen, *Conical Slicing: A different angle of 3D printing*,
  [nota](https://cnckitchen.com/blog/conical-slicing-a-different-angle-of-3d-printing).
- Hackaday, sobre el S4 de Joshua Bird, [etiqueta «non-planar slicer»](https://hackaday.com/tag/non-planar-slicer/).
- Las fichas `docs/spec/features/nonplanar.md` y `nonplanar_modes.md`, y `docs/ZAA.md`, `docs/NONPLANAR_SLICING.md`,
  `docs/NONPLANAR_INFILL.md` y `docs/VARIABLE_LAYERS.md` del motor.

**No se pudo leer completo** (la editorial bloquea el acceso automático): Sebbe et al. 2025, Srikanth et al. 2025,
Shan et al. 2023, Pelzer y Hopmann 2021 (solo el resumen), Elkaseer et al. 2020 (solo el resumen) y la revisión de
Nayyeri et al. 2022. Sus números salen del resumen o de terceros.
