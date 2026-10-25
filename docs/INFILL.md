# Relleno: resistencia y rapidez

Pedido del dueño (2026-10-25): «el relleno debe optimizarse para fuerza y rapidez de impresión».

El relleno es la mayor parte del tiempo de una pieza grande y casi no se ve. El objetivo es más resistencia por
minuto y por gramo. El plan va por etapas, y cada una se mide en el banco:
- el tiempo, del estimador;
- el material;
- la rigidez, del análisis estructural del motor (`docs/ANALYSIS.md`, la pieza «como impresa»).

| Etapa | Qué | De dónde |
|---|---|---|
| 1 | **Combinar capas de relleno:** el relleno poco denso cada N capas, con el alto de las N | Ficha `infill_patterns.md`, reglas 18 a 20 |
| 2 | **Relleno denso bajo las superficies de arriba:** permite un relleno general más liviano | Ficha `dense_infill.md` |
| 3 | **Patrones para resistencia:** cúbico (3D, isótropo) y cúbico adaptativo (denso cerca de las superficies, ralo adentro) | Ficha `infill_patterns.md`, reglas 6 y 7 |
| 4 | **Relleno guiado por tensiones:** más denso donde el análisis estructural muestra tensión | Fase A2 de `PLAN.md` |

## Combinar capas (etapa 1)

| Clave | Por defecto | Qué hace |
|---|---|---|
| `infill.combine_layers` | 1 | El relleno poco denso se imprime en una de cada N capas, con el alto de las N |
| `infill.combine_auto` | no | N es la mayor cantidad de capas que entra en el alto máximo |
| `infill.combine_max_height` | 100 % | Alto máximo de la capa combinada, en mm o en % de la boquilla. Nunca más que la boquilla |

**Cómo:**
- las corridas de capas empiezan en la capa 1 y toman N a la vez (o las que entren, en automático), sin pasar el
  alto máximo;
- lo que todas las capas de una corrida tienen como relleno poco denso (la intersección, sin piezas más angostas que
  un cordón) se imprime una vez, en la capa de arriba de la corrida, con el alto de toda la corrida y el filamento de
  ese alto;
- el resto del relleno de cada capa se imprime en su capa;
- las corridas alternan la dirección del relleno, como las capas;
- con la altura por zona, el relleno ya va grueso en la zona gruesa: no se combina además.

Es el mismo mecanismo que la capa gruesa de la altura por zona (`docs/LOCAL_LAYERS.md`): `;HEIGHT:` cambia dentro
de la capa.

**Medido** (2026-10-25, CPU):
- tiempo estimado, y material sumado del G-code;
- cúpula sobre columna de 40 mm y esfera de 100 mm;
- relleno rectilíneo al 20 %.

| Pieza | Capas | Sin combinar | Combinado | Material |
|---|---|---|---|---|
| esfera | 0,2 mm, combinar 2 | 447,2 min | 297,2 min (−34 %) | 131,3 → 131,2 cm³ |
| esfera | 0,12 mm, automático (3 capas, 0,36 mm) | 736,0 min | 401,9 min (−45 %) | 130,9 → 130,7 cm³ |
| cúpula | 0,2 mm, combinar 2 | 55,5 min | 42,3 min (−24 %) | 13,63 → 13,61 cm³ |
| cúpula | 0,12 mm, automático | 90,0 min | 60,6 min (−33 %) | 13,46 → 13,44 cm³ |

El mismo material en menos pasadas: el ahorro crece cuanto más relleno tiene la pieza y más finas son las capas.

**No medido:**
- la resistencia de un relleno combinado. Se mide con el análisis «como impresa» cuando este lea el laminado con
  alturas por tramo; hoy corta sus propias capas uniformes;
- una impresión real.

**Pruebas** (`tests/test_combine.cpp`):
- las corridas: de a N, el límite de la boquilla, el alto máximo en mm y en %, el automático;
- **P11 de la ficha,** en el cubo de 20 mm:
  - relleno poco denso solo en las capas pares desde la 4 (Z 1,0, 1,4…), con `;HEIGHT:0.4`;
  - la capa 3 sin relleno poco denso;
  - el doble de filamento por mm que con capas de 0,2;
  - menos tiempo;
  - el G-code pasa el chequeo.
- **Diferencia con Tisma Slicer:** su filamento por mm en P11 es 1,79 veces el de una capa sola, por su forma de
  cordón; el motor usa ancho × alto, así que da 2.

El fuzz sortea la combinación.

## Relleno denso bajo las superficies de arriba (etapa 2)

| Clave | Por defecto | Qué hace |
|---|---|---|
| `infill.dense_under_top` | no | La capa de relleno disperso justo debajo de una superficie de arriba va más densa |
| `infill.dense_density` | 40 % | Su densidad |

**Cómo** (ficha `dense_infill.md`):
- en cada capa, la parte del relleno disperso que tiene encima la piel de arriba de la capa siguiente (la
  intersección, sin piezas más angostas que medio cordón) se rellena a `dense_density`, con el mismo patrón y la misma
  dirección;
- el resto de la capa sigue a la densidad general.

Así el techo se apoya bien aunque el relleno general sea liviano: menos material y tiempo con la misma superficie.
Se combina con combinar capas, porque la capa densa sale de la corrida antes de buscar la parte común.

**Prueba D1 de la ficha** (`tests/test_combine.cpp`):
- cubo de 20 × 20 × 10 al 10 %;
- la capa 46, debajo de la piel (47 a 49), lleva cuatro veces las líneas de una capa común en la misma dirección;
- las demás capas, iguales entre sí;
- el G-code pasa el chequeo.

## Triángulos y cúbico (etapa 3)

| `infill.pattern` | Qué es |
|---|---|
| `triangles` | Tres familias de líneas a `angle`, +60° y +120°, cada una con un tercio del material (separación `3 · ancho / densidad`), iguales en todas las capas: forman triángulos |
| `cubic` | Las mismas tres familias, pero cada una corrida `z / √2` en su normal con la altura: son las caras de cubos apoyados sobre un vértice |
| `stars` | Las tres familias de los triángulos, con la segunda corrida media separación: nunca se cruzan las tres en un punto y forman estrellas de seis puntas (regla 5). Es el patrón por defecto de Tisma Slicer |
| `aligned_rectilinear` | El rectilíneo con todas las capas a `angle`, sin girar 90° (regla 1) |
| `zigzag` | El rectilíneo: en Tisma Slicer dan exactamente lo mismo (regla 2) |
| `line` | El rectilíneo con una línea de cada dos girada 6,7° sobre su punto en el medio de la región (+6,7° en las capas pares, −6,7° en las impares, como P3), unidas en zigzag por el borde (regla 3). Cuando la región mide a lo largo de las líneas más de unas 17 separaciones (34 mm al 20 %), las giradas cruzan a sus vecinas en las puntas, como en `grid`; la ficha no mide piezas tan grandes |
| `honeycomb` | Hexágonos de lado `2 · ancho / (√3 · densidad)` con un centro en el origen y lados a `angle` − 30°, + 30° y + 90°. Cada capa los imprime como líneas onduladas en una dirección principal, la mitad del largo a lo largo de ella; dos vecinas comparten esos tramos, que salen dobles. La principal pasa por `angle` + 90°, + 30° y − 30° capa a capa, y como la teselación de hexágonos no cambia al girarla 60° sobre un centro, los panales de todas las capas coinciden (regla 10). El lado cuenta cada pared una vez, como mide Tisma Slicer: el material sale unos 4/3 de la densidad |
| `concentric` | Lazos paralelos al borde de la región, el primero a medio cordón y después cada `ancho / densidad` hacia adentro, de afuera hacia adentro (regla 13) |

**Por qué el cúbico:**
- es una estructura 3D: rígida en X, Y y Z, como el giroide;
- se imprime en líneas rectas, más rápido;
- con la normal de cada cara a un ángulo de coseno `1/√3` de la vertical, su traza en un plano horizontal se corre
  `z / √2` al subir `z` (ficha `infill_patterns.md`, regla 6, y la geometría del cubo).

**Pruebas** (`tests/test_combine.cpp`):
- los triángulos tienen las tres direcciones (45°, 105° y 165°) con un tercio del largo cada una, iguales en dos
  capas, con el largo que da la densidad;
- el cúbico tiene las mismas tres direcciones; sus líneas cambian de una capa a otra, y a un período `s·√2` de
  altura vuelve el mismo largo;
- el fuzz sortea los dos patrones y el relleno denso.

## Cúbico adaptativo (etapa 3)

`infill.pattern` = `adaptive_cubic`: los cubos del cúbico, chicos cerca de las paredes y de las superficies y más
grandes adentro, como en un octree (ficha `infill_patterns.md`, regla 7). Las tres familias van a 0°, 60° y 120°,
sin importar `infill.angle` (regla 7, prueba P6). La densidad es la de los cubos más chicos, junto a las superficies.

**Cómo:**
1. **Qué tan adentro está cada punto.** El relleno disperso de todas las capas se pasa a una grilla de vóxeles
   cúbicos de un cuarto de la separación de las líneas, con un vóxel de afuera alrededor. Un vóxel está adentro si
   todas las capas que toca tienen relleno disperso ahí: una capa sólida fina entre dos de relleno también cuenta
   como superficie. La distancia de cada vóxel al afuera más cercano sale de la transformada de distancia euclídea
   exacta de Felzenszwalb y Huttenlocher (2012): una pasada de la transformada 1D de distancias al cuadrado por eje.
   Si la grilla pasara de 4 millones de vóxeles, los vóxeles se agrandan.
2. **Los niveles.** Los planos del cúbico están a `s·√(2/3)` (la arista de sus cubos, con `s` la separación de las
   líneas en la capa), y las esquinas de un cubo quedan a `s/√2` de su centro. El nivel `l` (cubos `2^l` veces los
   más chicos) entra donde la profundidad es al menos `2^l · s / √2`: ahí un cubo entero de ese tamaño cabe adentro.
   La cantidad de niveles es la misma en todas las capas (hasta 5, cubos 32 veces los más chicos).
3. **Las líneas.** Son las del cúbico: la línea `k` de cada familia está a `k·s + desplazamiento`, con el
   desplazamiento `−z/√2` del cúbico. La línea es del nivel `r` si `2^r` divide a `k` y `2^(r+1)` no, y se dibuja
   donde el relleno es menos profundo que el comienzo del nivel `r + 1`. Así, en cada nivel más adentro queda una
   línea de cada dos, y las líneas gruesas son líneas del cúbico fino: siguen de corrido por todos los niveles y,
   capa a capa, forman las caras de los cubos grandes.
4. Las bandas del relleno guiado por tensiones y el relleno denso bajo el techo van con el cúbico parejo a su
   densidad; el relleno combinado usa la profundidad a la altura de su capa gruesa.

**Medido** (2026-10-25): cubos al 20 %, boquilla 0,4, capas de 0,2.

| Cubo | Cúbico | Adaptativo | Material | Tiempo |
|---|---|---|---|---|
| 20 mm | 18,0 min, 3,29 cm³ | 17,1 min, 3,29 cm³ | igual | −5 % |
| 40 mm | 88,4 min, 19,88 cm³ | 87,5 min, 19,11 cm³ | −4 % | −1 % |
| 60 mm | 237,8 min, 59,39 cm³ | 219,0 min, 52,67 cm³ | −11 % | −8 % |
| 100 mm | 866,6 min, 245,5 cm³ | 756,5 min, 180,6 cm³ | **−26 %** | **−13 %** |

- Cuanto más grande la pieza, más rinde: el centro de una pieza chica está demasiado cerca de las superficies para
  que entre un cubo más grande.
- En el cubo de 20 mm no entra ningún nivel: es el cúbico a 0°, 60° y 120°. Tisma Slicer da ahí un 76 % del largo
  del cúbico (prueba P6), con la separación pareja: la separación base se ajusta cuando la ficha lo precise.
- El laminado del cubo de 100 mm pasa de 0,13 a 0,27 s con 4 hilos: la grilla, la transformada y las regiones de
  cada nivel.
- **Rigidez** («como impresa», cubo de 40 mm comprimido con 500 N, el banco de arriba): el cúbico se hunde 0,083 mm y el
  adaptativo 0,096 mm, con un 4 % menos de material: un 10 % menos de rigidez por material. Bajo una carga que
  reparte todo el volumen, el centro ralo pierde más de lo que ahorra. El adaptativo es para piezas grandes que no
  trabajan así (ahorra material y tiempo en el centro); para resistencia, el guiado por tensiones. Las piezas
  grandes no se pueden analizar «como impresas» (el cubo de 60 mm pide 27 millones de vóxeles).

**Cúbico de soporte** (`support_cubic`, regla 8): el mismo cúbico adaptativo, pero las capas sólidas de abajo no
cuentan, porque el relleno no las sostiene. La profundidad se mide:
- desde lo que el relleno sostiene: los vóxeles justo encima de él (las superficies de arriba, los techos internos);
- desde las paredes, en el plano de cada vóxel, para que un cubo grueso siga entrando entre ellas.

Así, sobre una superficie de abajo los cubos son grandes, y bajo un techo, chicos. En un cubo chico da lo mismo que el
adaptativo (prueba P6): las paredes no dejan entrar un nivel más.

**Medido** (2026-10-25, al 20 %): en el cubo de 60 mm, 217,7 min y 50,75 cm³ (−8 % de tiempo y −15 % de material
contra el cúbico); en el de 100 mm, 743,8 min y 170,7 cm³ (**−14 % y −30 %**). En el de 20 mm, igual que el
adaptativo.

**Pruebas** (`tests/test_adaptive.cpp`):
- la transformada de distancia es exacta: contra la distancia a cada vóxel de afuera, en una grilla al azar;
- la profundidad de un bloque de relleno, la región profunda a una altura, una capa sólida fina en el medio y el tope
  de vóxeles;
- 0°, 60° y 120° con `infill.angle` 45°;
- junto a las superficies, las mismas líneas que el cúbico; en el medio, menos; y todas sobre la grilla del cúbico;
- un cubo de 60 mm laminado a G-code que pasa el chequeo, con un 8 a 25 % menos de relleno que el cúbico;
- el cúbico de soporte: grueso sobre las capas sólidas de abajo, el cúbico bajo el techo, y en el cubo de 20 mm igual
  al adaptativo (P6);
- el fuzz sortea los dos patrones.

## Capa sólida cada N y espesor mínimo de la cáscara

| Clave | Por defecto | Qué hace |
|---|---|---|
| `infill.solid_every` | 0 (nunca) | Las capas N, 2N, 3N… (contadas desde 0) llevan relleno sólido en todo su relleno disperso, impreso como puente (`;TYPE:Bridge infill`) sobre el relleno de abajo (regla 21) |
| `surfaces.top_min_thickness` | 0 | Si las capas sólidas de arriba suman menos que esto (mm), se agregan capas hasta llegar (regla 22) |
| `surfaces.bottom_min_thickness` | 0 | Lo mismo abajo |

- El espesor se cuenta en capas del alto nominal: `max(capas, ⌈espesor / alto⌉)`. Con alturas variables, esa cuenta
  pasa por el mismo reparto que `surfaces.top_layers`.
- La capa sólida se arma después de los puentes: si la capa ya tenía puentes, todo va con el ángulo de esos puentes;
  si no, con el del relleno.

**Pruebas** (`tests/test_combine.cpp`): con `solid_every` 5, las capas 5, 10… 95 del cubo de 20 mm son puente y no
tienen relleno disperso (P12); con una capa arriba y abajo y mínimos de 1 y 0,6 mm, las capas 0 a 2 y 95 a 99 son
sólidas (P13).

## Rayo (etapa 3)

`infill.pattern` = `lightning` (ficha `infill_patterns.md`, regla 9): solo lo que hace falta para sostener las
superficies de arriba. Son ramas que nacen en las paredes y llegan bajo cada techo; más abajo se retiran hacia las
paredes, así arriba hay mucho y abajo casi nada. No da resistencia: es para piezas que solo se miran.

**De dónde:** las nervaduras de soporte de Tricard, Claux y Lefebvre (2020, «Ribbed Support Vaults for 3D Printing of
Hollowed Objects»): de arriba hacia abajo, cada punto a sostener recibe una rama a menos de una distancia dada, unida a
la rama o la pared más cercana, y las ramas de cada capa son las de la de arriba, recortadas en las puntas lo que deja
un voladizo imprimible.

**Cómo,** capa por capa de arriba hacia abajo:
1. **Las ramas de la capa de arriba bajan,** recortadas en cada punta `alto de capa · tan 45°` (0,2 mm con capas de
   0,2): cada rama sobresale de la de abajo a lo sumo un voladizo de 45°. Una rama más corta que eso desaparece.
2. **Lo que sale del relleno de esta capa se va.** Una rama que quedó sin camino a la pared, o una raíz que ya no
   está en una pared, se vuelve a unir.
3. **Lo que hay que sostener:** lo que la capa de arriba tiene sobre el relleno de esta capa (pieles, paredes), sin
   partes más angostas que un cordón. Se toman puntos en una grilla fija al origen, a `alcance` de distancia: todo
   punto queda a `alcance / √2` de uno. Un punto recibe rama si ninguna pasa a menos del resto del alcance, así todo
   lo que hay que sostener queda a menos del `alcance` de una rama.
4. **Cada punto nuevo se une** a lo más cercano: una rama ya anclada o la pared. Si el tramo hasta la rama sale del
   relleno, va a la pared. Los puntos más cerca de las paredes van primero, así las ramas crecen desde ellas. Entre
   ramas igual de cerca (en una grilla pasa seguido) gana la más vieja, que nació más cerca de la pared: así las ramas
   entran derecho desde las paredes en vez de correr a lo largo, y se retiran antes. Las ramas y las paredes están en
   grillas de celdas cuadradas para buscar la más cercana sin mirar todas.
5. **Se imprimen** las ramas como líneas desde cada punta o bifurcación hasta la siguiente o la pared, dentro del
   relleno achicado medio cordón, sin tramos de menos de un cordón.

**El alcance** sale de la densidad: la separación del rectilíneo a esa densidad (`ancho / densidad`, 2,25 mm al 20 %
con cordones de 0,45). Con la mitad, bajo el techo del cubo de 20 mm salían 263 mm de rama, el doble que Tisma Slicer
(134 mm, prueba P7); con la separación entera salen 117 mm. Las bandas del guiado por tensiones y el relleno denso bajo el
techo van en cúbico; el relleno combinado no se usa con el rayo.

**Medido** (2026-10-25), cubos al 20 %:

| Cubo | Cúbico | Rayo | Material | Tiempo | Laminado |
|---|---|---|---|---|---|
| 20 mm | 18,0 min, 3,29 cm³ | 13,7 min, 2,30 cm³ | −30 % | −24 % | 0,02 s |
| 60 mm | 237,8 min, 59,39 cm³ | 140,7 min, 26,60 cm³ | −55 % | −41 % | 0,12 s |
| 100 mm | 866,6 min, 245,5 cm³ | 430,6 min, 87,8 cm³ | **−64 %** | **−50 %** | 0,53 s |

En el cubo de 20 mm, el largo por capa crece hacia arriba: nada debajo de z 11, 5 mm en z 13,2, 60 mm en z 17,2 y
117 mm bajo el techo, en 32 tramos. Tisma Slicer (P7) tiene 134 mm bajo el techo en hasta 18 tramos, y algo de rayo
cerca del fondo (18,9 mm en la capa 5).

**Pruebas** (`tests/test_lightning.cpp`):
- en una caja de 30 mm con techo, todo punto bajo el techo queda a menos del alcance de una rama o una pared;
- hay menos rama cuanto más abajo, y casi nada al fondo;
- cada rama sobresale de la de abajo a lo sumo el voladizo de una capa, o nace en una pared;
- el cubo de 20 mm de la ficha (P7) crece hacia arriba y su G-code pasa el chequeo;
- el fuzz sortea el patrón.

**Lo que falta:**
- enderezar y juntar ramas (el artículo las endereza y las funde con las paredes), e imprimir cada árbol en menos
  tramos (P7 tiene hasta 18 donde el nuestro tiene 32);
- lo que Tisma Slicer imprime cerca del fondo en P7, que la regla no explica;
- más velocidad en densidades altas: al 100 %, el cubo de 100 mm lamina en 1,4 s y escribe en 0,7 s (antes del índice
  de ramas y del orden con grilla, 63 s en total).

## Resistencia y rapidez, medidas (2026-10-25)

**Cómo:**
- con el análisis estructural del motor, modelo «como impresa» (`docs/ANALYSIS.md`), PLA Bambu Basic, calidad
  normal (vóxeles de medio ancho de línea, los huecos del relleno vacíos);
- **cubo** de 20 mm comprimido con 500 N (apoyado abajo, la carga arriba);
- **barra** de 60 × 10 × 10 mm en voladizo, con 20 N hacia abajo en la punta;
- ajustes por defecto (3 paredes, 3 capas sólidas) y capas de 0,2 mm, salvo donde se dice;
- «rigidez/cm³» es 1 / (desplazamiento × material): más es mejor.

| Pieza | Relleno | Tiempo | Material | Desplazamiento | Rigidez/cm³ | Seguridad |
|---|---|---|---|---|---|---|
| cubo | rectilíneo 20 % | 17,5 min | 3,52 cm³ | 0,180 mm | 1,58 | 1,38 |
| cubo | grilla 20 % | 19,8 min | 3,29 cm³ | **0,057 mm** | **5,29** | **3,18** |
| cubo | giroide 20 % | 17,2 min | 3,31 cm³ | 0,097 mm | 3,11 | 2,23 |
| cubo | triángulos 20 % | 18,9 min | 3,35 cm³ | 0,064 mm | 4,65 | 2,51 |
| cubo | cúbico 20 % | 18,0 min | 3,29 cm³ | 0,148 mm | 2,05 | 2,28 |
| cubo | cúbico 20 %, combinar 2 | 14,6 min | 3,29 cm³ | 0,157 mm | 1,94 | 1,86 |
| cubo | cúbico 12 % + denso bajo el techo | 15,8 min | 2,84 cm³ | 0,456 mm | 0,77 | 0,74 |
| cubo | cúbico 20 %, por zona 0,3 / 0,1 | **13,2 min** | 3,49 cm³ | 0,119 mm | 2,40 | 2,16 |
| barra | rectilíneo 20 % | 15,7 min | 3,06 cm³ | 1,348 mm | 0,242 | 1,80 |
| barra | grilla 20 % | 16,3 min | 2,88 cm³ | 1,401 mm | 0,248 | 1,79 |
| barra | giroide 20 % | 14,6 min | 2,94 cm³ | 1,351 mm | 0,252 | 1,81 |
| barra | triángulos 20 % | 16,2 min | 2,87 cm³ | 1,355 mm | 0,257 | 1,85 |
| barra | cúbico 20 % | 16,2 min | 2,87 cm³ | 1,344 mm | 0,259 | 1,84 |
| barra | cúbico 20 %, combinar 2 | 13,6 min | 2,87 cm³ | 1,353 mm | 0,257 | 1,83 |
| barra | cúbico 12 % + denso bajo el techo | 14,6 min | 2,60 cm³ | 1,384 mm | 0,278 | 1,71 |
| barra | cúbico 20 %, por zona 0,3 / 0,1 | **12,1 min** | 3,12 cm³ | **1,134 mm** | **0,282** | **2,02** |

**Lo que dicen las medidas:**
- **Compresión en Z:** ganan los patrones que forman paredes verticales en todas las capas: grilla (3,1 veces más
  rígida por material que el rectilíneo) y triángulos. El rectilíneo alterna la dirección y es el más blando. El
  cúbico y el giroide reparten la rigidez en las tres direcciones.
- **Flexión:** la cargan sobre todo las paredes y las pieles. Todos los patrones quedan a menos de un 4 %, así que
  conviene el más rápido o el más liviano. El cúbico al 12 % con denso bajo el techo da la mejor rigidez por material
  en capas de 0,2 mm.
- **Combinar 2 capas** ahorra un 16–19 % del tiempo y pierde menos de un 6 % de rigidez.
- **La altura por zona** (0,3 mm donde la pieza no cambia, 0,1 en el detalle) es la más rápida y la más rígida en
  flexión: menos capas, cordones más gruesos y más paredes por milímetro.

**Para elegir:**
- **Cargas en Z** (columnas, soportes): grilla o triángulos.
- **Cargas en varias direcciones:** cúbico o giroide.
- **Rapidez:** combinar capas o altura por zona.
- **Material:** relleno liviano con denso bajo el techo.

**No medido:** probetas impresas. El análisis es lineal y elástico, y no ve la adhesión real entre capas ni entre
cordones.

## Relleno guiado por tensiones (etapa 4)

| Clave | Por defecto | Qué hace |
|---|---|---|
| `infill.stress_density` | 0 (apagado) | La densidad máxima, en %, donde la pieza trabaja más bajo el caso de carga |

Necesita el caso de carga, el mismo de `tisma-engine analyze` (`docs/ANALYSIS.md`):

```sh
tisma-engine slice pieza.stl --config ajustes.json --case caso.json -o pieza.gcode
```

**Cómo:**
1. Se analiza la pieza maciza bajo el caso (modelo `solid`, a la resolución del caso, sin la comprobación de
   resolución). La pieza maciza sirve porque el relleno no cambia por dónde va la carga.
2. Cada vóxel recibe su carga: 1 / coeficiente de seguridad, sobre la del vóxel más cargado (de 0 a 1).
3. En cada capa, a la altura de su plano de corte, el relleno disperso se parte en tres bandas, de la más cargada a
   la menos:
   - carga ≥ 0,6: la densidad máxima;
   - carga ≥ 0,35: dos tercios del camino entre la densidad común y la máxima;
   - carga ≥ 0,15: un tercio del camino.

   Cada banda es la unión de los cuadrados de sus vóxeles, cerrada medio vóxel, menos las piezas más angostas que un
   cordón. Se rellena con el mismo patrón y la misma dirección. El resto queda a `infill.density`.
4. `tisma-engine analyze` con `infill.stress_density` usa el mismo relleno guiado en el modelo «como impresa».

**Por qué:** el material va donde trabaja. Se puede bajar la densidad general y reforzar solo los caminos de la
carga: más rigidez por gramo y por minuto.

**Medido** (2026-10-25): el mismo banco de arriba, con el caso de cada pieza como caso de carga.

| Pieza | Relleno | Tiempo | Material | Desplazamiento | Rigidez/cm³ | Seguridad |
|---|---|---|---|---|---|---|
| cubo | cúbico 20 % | 18,0 min | 3,29 cm³ | 0,148 mm | 2,05 | 2,28 |
| cubo | cúbico 10 → 50 % guiado | 21,1 min | 4,19 cm³ | 0,073 mm | 3,28 | 3,69 |
| cubo | cúbico 6 → 40 % guiado | 19,7 min | 3,75 cm³ | 0,094 mm | 2,84 | 3,41 |
| cubo | grilla 10 → 50 % guiada | 21,5 min | 4,19 cm³ | **0,036 mm** | **6,65** | 3,12 |
| barra | cúbico 20 % | 16,2 min | 2,87 cm³ | 1,344 mm | 0,259 | 1,84 |
| barra | cúbico 10 → 50 % guiado | 15,8 min | 2,79 cm³ | **1,303 mm** | 0,275 | **1,92** |
| barra | cúbico 6 → 40 % guiado | **14,7 min** | **2,56 cm³** | 1,353 mm | **0,289** | 1,80 |
| barra | grilla 10 → 50 % guiada | 16,4 min | 2,78 cm³ | 1,382 mm | 0,261 | 1,84 |

- **En el cubo comprimido** casi todo trabaja, así que el relleno sube en casi todas partes: más material, pero mucha
  más rigidez por material. La grilla guiada da la más alta del banco, 1,26 veces la grilla uniforme y 4,2 veces el
  rectilíneo.
- **En la barra** la carga se concentra junto al empotramiento.
  - El cúbico guiado del 6 al 40 % da la misma rigidez que el uniforme al 20 % con un 11 % menos de material y un 9 %
    menos de tiempo.
  - El del 10 al 50 % es más rígido y más seguro, y además más rápido.

**En la interfaz:**
- en Impresión, la fila **Guiado por tensiones** pone la densidad máxima;
- debajo, **Caso de carga** abre un caso `.json`, el mismo formato de `tisma-engine analyze`;
- el laminado lo usa. Si el análisis falla, o el caso no sostiene la pieza, el relleno queda parejo y hay un aviso.

**Lo que falta:**
- editar el caso en la interfaz (apoyos y cargas sobre la pieza), con la pestaña Análisis;
- iterar: volver a analizar con el relleno guiado y ajustar.

**Prueba** (`tests/test_combine.cpp`): una barra de 60 × 10 × 10 empotrada en un extremo y cargada en el otro.
- La banda más cargada está junto al empotramiento: arriba, a menos de 5 mm de él y nunca más allá de 30 mm.
- Sobre la pieza no hay banda.
- Lo laminado lleva más de 1,5 veces el relleno por área en el tercio empotrado que en el libre. Sin tensiones, 1
  ± 0,15.
- El G-code pasa el chequeo.
