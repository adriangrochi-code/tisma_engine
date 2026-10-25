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

**Lo que falta:**
- en la interfaz, pasar el caso de la pestaña Análisis al laminado;
- iterar: volver a analizar con el relleno guiado y ajustar.

**Prueba** (`tests/test_combine.cpp`): una barra de 60 × 10 × 10 empotrada en un extremo y cargada en el otro.
- La banda más cargada está junto al empotramiento: arriba, a menos de 5 mm de él y nunca más allá de 30 mm.
- Sobre la pieza no hay banda.
- Lo laminado lleva más de 1,5 veces el relleno por área en el tercio empotrado que en el libre. Sin tensiones, 1
  ± 0,15.
- El G-code pasa el chequeo.
