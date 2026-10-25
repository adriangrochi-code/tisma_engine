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
