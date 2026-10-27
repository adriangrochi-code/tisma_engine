# Modelos de las pruebas de calibración

**Qué hace:** genera las piezas de las pruebas de `calibrations.md` y de `resin.md`. Cada pieza muestra el efecto del
ajuste que cambia: por franjas a lo largo de la altura, o por parches o columnas a lo largo de X.

**Fase:** después de F4. Las piezas las diseñó el equipo del dueño en Tisma Slicer. Se generan con medidas, no se
cargan de archivos.

## Reglas comunes

1. **Altura de capa:** la mitad del diámetro de la boquilla, redondeada a 0,01 mm (0,2 mm con boquilla de 0,4).
2. **Placa:** cargar una prueba reemplaza las piezas de la placa. La pieza queda centrada en la cama.
3. **Ajustes de la pieza:**
   - los de la tabla van como ajustes del objeto, y la página de ajustes los muestra como cambiados;
   - en las pruebas por parches, el valor de cada parche va como ajuste de esa parte.
4. **Valores:**
   - por franjas: `inicio + paso · ⌊altura / alto de la franja⌋`;
   - por parches o columnas: `inicio + paso · número`, desde la izquierda y desde 0.
5. **El paso** se escribe siempre positivo. El sentido sale de inicio y fin: de 230 a 190 baja. El paso y el alto (o
   ancho) de la franja tienen que ser mayores que 0.
6. **Números en relieve:**
   - dígitos de siete segmentos, con trazos de 0,8 mm (dos líneas de una boquilla de 0,4) y 0,6 mm de relieve;
   - cada dígito mide de ancho `max(2,2; 0,55 · alto)` mm, con 0,8 mm entre dígitos.

La pieza de prueba general, con todo junto y mecanismos, es Mabel: `quality_model.md`.

## Pruebas por franjas (torres)

**Caja hueca:** una caja de paredes con 1 o 2 perímetros, sin relleno, sin tapa, 3 capas abajo y sin paredes finas.

| Prueba | Pieza | Por defecto: inicio → fin, paso, franja | Ajustes de la pieza |
|---|---|---|---|
| Temperatura | Torre de bloques, uno por temperatura, sobre una placa de 1 mm (ver abajo) | 230 → 190 °C, 5, 10 mm | 2 perímetros, relleno 15 % |
| Pressure advance | Caja hueca de 50 × 50 mm, 2 perímetros | 0 → 0,1, 0,002, 1 mm | Perímetros a 100 mm/s |
| Retracción | Base de 52 × 12 × 1 mm y dos pilares cilíndricos de 8 mm de diámetro, con 40 mm entre centros | 0 → 2 mm, 0,1, 1 mm | 2 perímetros, relleno 15 % |
| Caudal máximo | Cilindro de 50 mm de diámetro | 5 → 25 mm³/s, 1, 2 mm | 1 perímetro, sin relleno ni tapa, 2 capas abajo |
| VFA | Caja hueca de 50 × 50 mm, 1 perímetro | 40 → 200 mm/s, 10, 5 mm | — |
| Aceleración | Caja hueca de 60 × 60 mm, 1 perímetro | 1000 → 10 000 mm/s², 1000, 5 mm | Perímetros a 120 mm/s |
| Esquinas | Caja hueca de 60 × 60 mm, 1 perímetro | 1 → 20, 1, 5 mm | Perímetros a 120 mm/s |
| Vibraciones | Caja hueca de 60 × 60 mm, 1 perímetro | 15 → 85 Hz, 5, 5 mm | Perímetros a 150 mm/s |

**Caudal máximo con un valor conocido** (extra del motor, aprobado por el dueño el 27-10-2026; Tisma Slicer no lo
hace): si la impresora tiene `machine.max_flow` mayor que 0, la torre va de 0,5 a 1,5 veces ese valor, con un paso de
una décima del valor redondeado a 0,5 mm³/s (mínimo 0,5) y franjas de 2 mm. Así la prueba queda centrada en esa
máquina. Sin valor conocido, la tabla de arriba.

**Torre de temperatura.** Cada bloque mide lo que la franja de alto y 12 mm de fondo. De izquierda a derecha:
- **pilar A** de 14 mm, con la temperatura en relieve al frente. Los dígitos miden `min(7; 0,6 · franja)` mm de alto
  y quedan centrados;
- **hueco** de 16 mm:
  - arriba, un puente de 1 mm de espesor;
  - si la franja mide más de 3 mm, en el medio un pin cilíndrico de 2,4 mm de diámetro y `franja − 2` mm de alto,
    para ver los hilos;
- **pilar B** de 6 mm;
- **voladizo a la derecha del pilar B:**
  - un cuarto de círculo que empieza vertical y llega a 75° de la vertical 1 mm debajo del bloque siguiente, en pasos
    de 5°;
  - radio `max(1; franja − 1) / sen 75°`;
- **si la franja mide 6 mm o más:**
  - marcas al frente del pilar B a la altura de 30°, 45° y 60° del voladizo, de `1,5 + 0,05 · ángulo` mm de largo,
    0,4 mm de alto y 0,6 mm de relieve;
  - a la izquierda del pilar A, una ménsula a 45° de `min(4; 0,4 · franja)` mm. Sobre ella, un pin de 2,4 mm de
    diámetro para quebrarlo con la mano: cuanto más cuesta, mejor se pegan las capas.

La placa de abajo va desde 6 mm a la izquierda del pilar A hasta `r + 2` mm a la derecha del pilar B (`r`, el radio
del voladizo), y se extiende 4 mm adelante y atrás.

## Pruebas por parches o columnas (a lo largo de X)

El ancho es el de cada parche más su separación; la pieza ocupa el ancho menos lo que dice la tabla.

| Prueba | Pieza de cada valor | Por defecto: inicio → fin, paso, ancho | Ajustes de la pieza |
|---|---|---|---|
| Primera capa (desfase Z) | Parche de una capa: (ancho − 4) × 30 mm | −0,10 → 0,10 mm, 0,05, 25 mm | 1 perímetro, 1 capa arriba y abajo, relleno 100 % |
| Flujo (%) | Parche de (ancho − 4) × 30 × 2 mm | 90 → 110 %, 5, 25 mm | 2 perímetros, 5 capas arriba, 3 abajo, relleno 100 %, superficie de arriba `monotonic` |
| Coasting | Torre hueca de (ancho − 6) × 15 × 20 mm | 0 → 1 mm, 0,2, 20 mm | 2 perímetros, sin relleno ni tapa, 3 capas abajo, costura atrás |
| Velocidad de los arcos | Columna de 10 × 10 mm con una losa de 2 mm arriba que sale 15 mm hacia atrás (+Y) sin nada abajo; ancho `max(4; ancho − 6)` | 2 → 10 mm/s, 2, 16 mm | Arcos en voladizos sí, sin distancia máxima, sin soportes, 2 perímetros, relleno 15 % |
| Distancia de los arcos | La misma columna; la losa sale lo que dice el valor | 10 → 30 mm, 5, 14 mm | Los mismos |
| Solape de los arcos | La columna con losa de 15 mm | 0 → 40 %, 10, 16 mm | Los mismos; el solape (0 a 50 %) va en cada columna |
| Flujo de planchado | Parche de `max(4; ancho − 4)` × 20 × 2 mm | 5 → 25 %, 5, 25 mm | 2 perímetros, 5 capas arriba, 3 abajo, relleno 100 %, `monotonic`, planchado solo en la capa de arriba de todo |
| Separación de planchado | El mismo parche | 0,05 → 0,25 mm, 0,05, 25 mm | Los mismos |
| Velocidad de planchado | El mismo parche | 10 → 50 mm/s, 10, 25 mm | Los mismos |

## Piezas para medir

- **Bloque de medidas (FDM):** cuatro escalones cuadrados de 80, 50, 30 y 10 mm de lado y 3 mm de alto cada uno.
  - El escalón más ancho tiene un agujero en cada esquina, a 32,5 mm del centro en X y en Y.
  - Los agujeros miden 4, 6, 8 y 10 mm de diámetro. El más chico va adelante a la izquierda y siguen en sentido
    antihorario, así se distingue X de Y.
  - Se imprime con 3 perímetros, relleno 15 % y sin soportes.
  - Las cuentas están en `calibrations.md`, regla 6.
- **Tubos de resistencia** (`staggered_perimeters.md`):
  - dos tubos iguales de 20 × 10 × 50 mm, separados 15 mm, solo de paredes: 4 perímetros, sin relleno ni tapa, 3
    capas abajo;
  - A con perímetros normales y B con perímetros alternados;
  - se empuja la punta de cada uno de costado, o se le cuelga peso, hasta que se rompe entre capas. La diferencia es
    lo que agregan los perímetros alternados con esa impresora y ese filamento.

## Resina

- **Torre de exposición:**
  - 30 mm de ancho y 4 mm de fondo, de `franja × (franjas + 1)` de alto. La primera franja es la base;
  - en cada franja:
    - una ranura al frente, al pie de la franja, de 0,3 mm de alto y 0,8 mm de fondo;
    - agujeros horizontales que la atraviesan de adelante hacia atrás, de 0,4, 0,6, 0,8, 1,0 y 1,4 mm de diámetro,
      centrados a media franja y en X = 3, 7, 11, 15 y 19 mm. Un agujero va solo si deja 0,3 mm de pared arriba y
      abajo (diámetro ≤ franja − 0,6);
    - atrás, aletas de 0,15, 0,25, 0,35 y 0,5 mm de espesor, en X = 22, 24, 26 y 28 mm. Salen 1,5 mm de la cara de
      atrás y van de 0,2 mm sobre el pie de la franja a 0,2 mm debajo de la siguiente;
  - por defecto, de 1,5 a 3,5 s cada 0,25 s, franjas de 2 mm;
  - sin soportes ni base, sobre la plataforma;
  - solo con formatos que guardan la exposición de cada capa (GOO).
- **Bloque de medidas de resina:** 30 × 30 × 10 mm, sin soportes ni base. Se mide lejos del borde de abajo, porque las
  primeras capas se ensanchan.

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| CM1 | Torre de temperatura por defecto | 9 bloques (230, 225 … 190 °C), cada uno de 10 mm, sobre la placa de 1 mm; con marcas de 30°, 45° y 60° y el pin para quebrar en cada bloque |
| CM2 | Torre de temperatura con franjas de 3 mm | Sin pin de hilos (franja no mayor que 3 mm), sin marcas ni pin para quebrar (franja menor que 6 mm) |
| CM3 | Torre de exposición con franjas de 1 mm | Solo los agujeros de 0,4 mm (≤ 0,4 mm de diámetro) |
| CM4 | Boquilla de 0,6 | Capas de 0,3 mm |
| CM5 | Paso 0 | No se genera la pieza: «el paso y su alto tienen que ser mayores que 0» |
| CM6 | Caudal máximo con `machine.max_flow` 20 | De 10 a 30 mm³/s, de a 2, franjas de 2 mm (11 franjas) |
| CM7 | Caudal máximo sin `machine.max_flow` | De 5 a 25 mm³/s, de a 1, franjas de 2 mm |
