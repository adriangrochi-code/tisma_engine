# Altura de capa variable por Z

Pedido del dueño (2026-10-25): «seguí con la altura variable por Z y por Z anti-aliasing así las combinamos para
máxima calidad», y «no debe depender de dos objetos: debería poder hacerse en uno complejo».

La ficha `docs/spec/features/slicing.md`, «Altura de capa variable», pide una lista de pares (Z, altura) dentro de los
límites de la impresora, nunca por encima del diámetro de la boquilla, con el plano de corte a mitad de cada capa.
Tisma Slicer además la calcula con un botón según la inclinación de la superficie. Acá van las dos cosas, y se
combinan con la altura de capa por zona (`docs/LOCAL_LAYERS.md`).

## De dónde sale

- **El escalón** (cusp height): Dolenc, A. y Mäkelä, I., *Slicing procedures for layered manufacturing techniques*,
  Computer-Aided Design 26(2), 1994. Una capa de alto h sobre una cara de normal unitaria n deja un escalón de
  `h · |n_z|`. Si el escalón no debe pasar de c, la cara admite capas de hasta `c / |n_z|`.
- **Lo demás es diseño propio:**
  - las bandas de Z;
  - la búsqueda de la capa más alta;
  - las caras planas que cierran una capa;
  - las subcapas por pendiente con la altura por zona;
  - las pieles contadas en capas gruesas.

## Ajustes

| Clave | Qué hace |
|---|---|
| `layers.heights` | Lista de tramos `[z, altura]` en mm, ordenada por z. Desde cada z (sobre la cama) hacia arriba, esa altura. Debajo del primer tramo, `layers.height` |
| `layers.cusp_height` | El escalón más alto que puede dejar una superficie inclinada, en mm. 0: apagado |
| `machine.min_layer_height`, `machine.max_layer_height` | Las alturas que imprime la máquina. 0: 0,04 mm y el diámetro de la boquilla. Nunca por encima de la boquilla |

Sin lista ni `cusp_height`, la pila es exactamente la de antes: el mismo G-code, byte a byte.

## Cómo se planean las capas

La primera capa mide `layers.first_height`. Después, de abajo hacia arriba, cada capa (la «gruesa» cuando hay altura
por zona) se decide así:

1. **La lista:** la altura del tramo donde empieza la capa, acotada a los límites de la máquina. Si el próximo tramo
   empieza dentro de la capa, la capa termina ahí (si no queda más fina que el mínimo). Así cada tramo empieza en
   una capa propia.
2. **La pendiente** (con `cusp_height`):
   - cada cara inclinada limita las bandas de Z que cruza (bandas de 0,005 mm) a `c / |n_z|`;
   - las caras verticales no limitan nada;
   - las horizontales no limitan, pero se anotan como «planos».
3. **Sin altura por zona:** la capa entera mide lo más alto que dejan las bandas que cruza. La búsqueda puede cortar
   la capa justo antes de una banda más plana, en vez de achicarla a la medida de esa banda: debajo de un techo, la
   capa llega hasta donde empieza el techo.
4. **Con altura por zona** (`layers.detail_height`):
   - la capa gruesa se parte en `k` subcapas, tantas como pida la pendiente (`k = ⌈H / lo que dejan las bandas⌉`).
     Como máximo `H / detail_height` redondeado, de 2 a 4, ninguna de menos de 0,04 mm ni del mínimo de la máquina;
   - donde la pendiente no pide nada, `k = 1`: la capa se imprime gruesa entera;
   - donde ni las `k` más finas alcanzan, la capa gruesa se achica hasta que alcancen;
   - después, las zonas de `docs/LOCAL_LAYERS.md` deciden, dentro de cada grupo, qué va fino y qué va grueso.
5. **Los planos:** con `cusp_height`, una cara horizontal cierra una capa. Si está dentro de la capa, la capa termina
   en ella. Si está apenas por encima (menos que el mínimo), la capa se estira hasta ella, si no pasa el máximo. Así un
   techo plano o la base de un voladizo quedan a su altura exacta.

El plano de corte sigue a mitad de cada capa (la regla 2 de la ficha), y una capa cuyo medio queda por encima de la
pieza no se crea.

## Lo que cambia en el resto del motor

- **Las pieles** (`surfaces.top_layers`, `bottom_layers`) cuentan capas gruesas. Cada subcapa fina vale `1/k` de una
  gruesa, y la piel de una capa llega hasta juntar ese número. Sin altura por zona, cada capa vale 1: se cuentan
  capas, como siempre.
- **El soporte** usa la altura nominal (`height / k`) para pasar su separación en Z a capas.
- **Lo que ya leía la altura por capa** sigue igual:
  - el escritor (`;Z:` y `;HEIGHT:` de cada capa);
  - el chequeo;
  - la vista previa (colorear por «Alto de capa»);
  - el tiempo mínimo de capa.
- **El análisis estructural** (`fea/`) sigue usando capas uniformes.

## La interfaz

En «Impresión», debajo de la altura por zona:
- **«Escalón en pendientes»** (`cusp_height`);
- **«Alturas por Z»:** un botón para agregar tramos, y una fila por tramo con su Z, su altura y una × para quitarlo.
  Se ordena al soltar.

En «Impresora», «Capa mínima» y «Capa máxima».

## Por objeto y en una pieza compleja

- **La ficha pide la lista por objeto.** El motor lamina todos los objetos de la cama en una sola pila, así que la
  lista vale para toda la cama.
- **Lo que depende de la forma no depende del objeto:**
  - la pendiente y las zonas se deciden por cara y por sección;
  - dentro de una misma pieza compleja, la parte vertical va gruesa y la inclinada va fina (la prueba es una cúpula
    sobre una columna, en una sola malla);
  - dos objetos distintos a la misma Z también reciben alturas distintas por la altura por zona.

## Medidas (2026-10-25)

Banco con modelos generados:
- cúpula sobre columna de 40 mm (una sola malla, `dome.stl`);
- esfera de 100 mm.

«Escalón» es el escalón del perímetro externo, `h · |n_z|` con la normal exacta, ponderado por largo: medio, percentil
95 y máximo. El material queda en ±1,5 % en todos los modos.

**Escalón máximo 0,1 mm:**

| Pieza | Modo | Capas | Tiempo | Escalón medio | p95 | Máximo |
|---|---|---|---|---|---|---|
| cúpula | todo a 0,1 mm | 398 | 113,0 min | 0,019 | 0,079 | 0,100 |
| cúpula | todo a 0,3 mm | 133 | 38,6 min | 0,056 | 0,238 | 0,296 |
| cúpula | por zona (0,3 / 0,1) | 398 | 54,4 min | 0,033 | 0,085 | 0,100 |
| cúpula | por pendiente | 179 | 44,6 min | 0,046 | 0,101 | 0,104 |
| cúpula | zona + pendiente | 201 | 48,6 min | 0,041 | 0,094 | 0,099 |
| esfera | todo a 0,1 mm | 998 | 918,6 min | 0,042 | 0,088 | — |
| esfera | por zona | 998 | 495,7 min | 0,049 | 0,089 | — |
| esfera | por pendiente | 555 | 417,3 min | 0,086 | 0,101 | — |
| esfera | zona + pendiente | 668 | 425,8 min | 0,070 | 0,096 | — |

(En la esfera, el máximo es la primera capa de 0,3 mm, contra la cama.)

**Escalón máximo 0,05 mm (cúpula):**

| Modo | Tiempo | Escalón medio | p95 | Máximo |
|---|---|---|---|---|
| todo a 0,05 mm | 224,6 min | 0,009 | 0,039 | 0,050 |
| por pendiente | 61,4 min | 0,032 | 0,051 | 0,054 |
| zona + pendiente | 56,1 min | 0,031 | 0,050 | 0,063 |

**Lo que dicen las medidas:**
- **El escalón máximo:** zona + pendiente lo acota como todo fino, en la mitad del tiempo de la altura por zona sola
  (cúpula) y en menos de la mitad del de todo fino.
- **El escalón medio:** sale más alto que con todo fino, porque las paredes casi verticales van gruesas: ahí el escalón
  ya es chico.
- **Lo que falta:** Z anti-aliasing (`docs/spec/features/z_antialiasing.md`, próximo paso) baja el escalón que queda en
  las superficies de arriba de poca pendiente. Esas son justo las que más capas finas piden.

## Pruebas

En `tests/test_variable_layers.cpp`.

**La lista:**
- tramos con su altura;
- un tramo que empieza entre dos capas;
- los límites de la máquina y la boquilla;
- sin lista: la pila uniforme.

**La pendiente**, sobre un prisma de techo a 10°, una sola malla:
- las paredes van a 0,3 y el techo a lo que deja `c / cos 10°`;
- la capa debajo del techo llega justo hasta él;
- un techo plano cierra una capa, también cuando la capa tiene que estirarse;
- con altura por zona, el techo va en tres subcapas y las paredes en capas gruesas enteras.

**Las pieles en capas gruesas.**

**Punta a punta**, cúpula sobre columna con lista, pendiente, y zona + pendiente:
- el G-code pasa el chequeo;
- el material queda en ±6 %;
- la columna sigue a 0,3;
- hay zonas gruesas en la cúpula.

El fuzz (`tests/fuzz_gcode.cpp`) sortea `cusp_height`, tramos y límites de máquina.
