# Análisis estructural

**Qué hace:** dice si una pieza impresa aguanta unas cargas dadas. Calcula desplazamientos, tensiones y factor de
seguridad, y da un veredicto. Tiene en cuenta que las capas son más débiles entre sí que a lo largo, y que la pieza
tiene paredes y relleno.

**Fase:** F7. Lo diseñó y escribió el equipo del dueño en Tisma Slicer; esta ficha describe su comportamiento.

**Fuentes:**
- Método de elementos finitos con hexaedros trilineales de 8 nodos e integración de Gauss 2 × 2 × 2: cualquier texto
  de elementos finitos (por ejemplo Hughes, *The Finite Element Method*, 1987, cap. 3).
- Gradiente conjugado precondicionado y multigrid geométrico: Saad, *Iterative Methods for Sparse Linear Systems*
  (2003), cap. 9 y 13.
- Material transversalmente isótropo y rotación de la matriz constitutiva con la matriz de Bond: textos de mecánica de
  materiales compuestos (por ejemplo Jones, *Mechanics of Composite Materials*, 1999).
- Teoría de vigas de Euler–Bernoulli para las pruebas.

## Entradas

| Entrada | Unidad | Qué es |
|---|---|---|
| Malla de la pieza | mm | Cerrada |
| Material | — | Una fila de la tabla «Materiales» o uno del usuario con los mismos campos |
| Temperatura de trabajo | °C | Uniforme en toda la pieza; no hay conducción de calor |
| Temperatura de la boquilla | °C | La del perfil de filamento; cambia la adherencia entre capas |
| Factor de seguridad pedido | — | Por ejemplo 2 |
| Caras fijas | — | Triángulos de la malla que no se mueven (empotramiento) |
| Cargas | N | Fuerza (X, Y, Z) en un punto con radio o repartida en caras |
| Límite de desplazamiento | mm o % de la mayor dimensión de la pieza | Por carga y por zona de caras; 0 = sin límite; con los dos, vale el menor |
| Dirección de impresión | — | Hacia dónde se apilan las capas; Z por defecto |
| Paredes y relleno | — | Del perfil: perímetros, capas sólidas, densidad y patrón del relleno, ancho y alto de los cordones |
| Resolución | — | Rápida (unos 20 000 vóxeles), normal (unos 60 000), alta (vóxel de 2 anchos de cordón), ultra (vóxel de 1 ancho) |

## Resultados

- Desplazamiento de cada vóxel, tensión de von Mises, factor de seguridad, masa de la pieza.
- Punto crítico: dónde el factor de seguridad es menor.
- **Veredicto:**

  | Veredicto | Cuándo |
  |---|---|
  | Aguanta | factor de seguridad ≥ el pedido en toda la pieza |
  | Margen bajo | entre 1 y el pedido en algún punto |
  | Fuera de carga | menor que 1 en algún punto |
  | Demasiado flexible | aguanta, pero una carga o una zona se mueve más que su límite |
  | Fuera de temperatura | la temperatura de trabajo supera la máxima de servicio del material |
  | No aplicable | el material es un elastómero (TPU): un cálculo lineal no lo describe |
- **Sugerencia:** si no aguanta, los materiales de la tabla que sí aguantarían a esa temperatura con el factor pedido.
  Con límites de desplazamiento, además tienen que ser lo bastante rígidos (el desplazamiento escala con 1/E).

## Método

1. **Unidades:** mm, N, MPa, °C.
2. **Vóxeles:**
   - rejilla regular sobre la caja de la pieza;
   - un vóxel es sólido si su centro está dentro de la pieza (se corta la malla a la altura del centro de cada fila de
     vóxeles);
   - el tamaño sale de la resolución pedida.
3. **Elementos:** cada vóxel es un hexaedro trilineal de 8 nodos con integración 2 × 2 × 2. Todos los vóxeles tienen
   la misma matriz de rigidez de elemento (antes de escalarla por el material del vóxel).
4. **Material transversalmente isótropo**, con el eje en la dirección de impresión:
   - `E_xy` a lo largo de las capas, `E_z` a través;
   - `G_xy = E_xy / 2(1 + ν)`, `G_z = E_z / 2(1 + ν)`;
   - el término de flexibilidad entre el plano y el eje es `−ν / √(E_xy · E_z)`, que deja la matriz simétrica y
     definida positiva aun con fibra de carbono;
   - si las capas se apilan en otra dirección, la matriz se rota con la matriz de Bond.
5. **Temperatura:** la rigidez y la resistencia se multiplican por `f(T)`:
   - 1 hasta 23 °C;
   - baja en línea recta hasta 0,5 en la temperatura máxima de servicio;
   - cae hasta 0,02 en los 15 °C siguientes.
6. **Adherencia entre capas:** la resistencia a través de las capas se multiplica, según la temperatura de la
   boquilla, por:
   - 1 en la mitad del rango de impresión del material, 1,15 en el máximo y 0,8 en el mínimo;
   - hasta 0,5 a 20 °C por debajo del mínimo.

   Es un modelo aproximado; se valida con probetas impresas.
7. **Sistema lineal:**
   - `K u = f` con gradiente conjugado precondicionado, sin armar la matriz global: el producto se hace vóxel por
     vóxel, en paralelo, en grupos de vóxeles que no comparten nodos;
   - precondicionador: multigrid geométrico sobre rejillas cada vez más gruesas, o Jacobi en piezas chicas (hasta unos
     15 000 grados de libertad);
   - opcional en la GPU.
8. **Tensión de cada vóxel:** en su centro, en el sistema de las capas (z a través).
9. **Criterio de rotura** (índice `I`; factor de seguridad = `1 / I`):
   - `I = máx( σ_vm / S_xy, máx(0, σ_zz) / S_z, τ_z / (0,6 · S_z) )`;
   - `σ_vm` es la tensión de von Mises, `σ_zz` la tracción a través de las capas y `τ_z = √(τ_xz² + τ_yz²)`;
   - `S_xy` y `S_z` ya incluyen `f(T)`; `S_z` también la adherencia;
   - la resistencia al corte entre capas es 0,6 de la de tracción.
10. **Paredes y relleno** (si se pide la pieza impresa y no maciza):
    - **capa maciza:** la parte de cada vóxel a menos del espesor de pared de la superficie, o del espesor de las capas
      sólidas en techos y suelos, es material macizo;
    - el espesor de pared es la suma de las separaciones de los perímetros (`perimeters.md`, regla 1), no perímetros ×
      ancho;
    - **relleno:** material continuo más blando, con rigidez `E · ρ^nE` y resistencia `S · ρ^nS`;
    - `ρ` es el material que deposita el patrón: `ρ = mín(1, k · densidad)`, con `k` y los exponentes de las tablas de
      abajo. Hay que calibrarlos con probetas.
    - Cada vóxel mezcla la parte maciza y la de relleno según su fracción.
11. **Masa:** la suma de la fracción de material de cada vóxel por su volumen y la densidad del material.

**Exponentes del relleno** (aproximados):

| Patrón | nE | nS |
|---|---|---|
| Rectilíneo, grilla | 1,2 | 1,2 |
| Panal | 1,4 | 1,4 |
| Cúbico | 1,5 | 1,4 |
| Giroide | 1,6 | 1,5 |
| Lightning (no estructural) | 2,5 | 2,5 |

**k:** material depositado por unidad de densidad pedida. Se midió en Tisma Slicer con un bloque de 30 × 30 × 20 mm,
capas de 0,3 y cordones de 0,45; es la media entre 15 % y 30 %.

| Patrón | k | Patrón | k |
|---|---|---|---|
| Rectilíneo, alineado, monotónico | 1,10 | Líneas monotónicas | 1,00 |
| Grilla | 1,07 | Triángulos | 1,10 |
| Estrellas | 1,05 | Cúbico | 1,06 |
| Línea | 1,06 | Concéntrico | 1,11 |
| Panal | 1,33 | Panal 3D | 0,885 |
| Giroide | 1,04 | Hilbert, cuerdas, espiral | 1,11 a 1,13 |

## Materiales

Valores típicos de piezas impresas, de hojas técnicas de marcas comunes y de bibliografía. Son un punto de partida:
la interfaz pide verificarlos con la hoja del filamento usado.

| Material | E_xy (MPa) | E_z (MPa) | ν | S_xy (MPa) | S_z (MPa) | Tg (°C) | Máx. servicio (°C) | Densidad (g/cm³) | Boquilla (°C) |
|---|---|---|---|---|---|---|---|---|---|
| PLA | 3000 | 2600 | 0,35 | 50 | 30 | 60 | 55 | 1,24 | 190–220 |
| PETG | 2000 | 1800 | 0,38 | 45 | 30 | 80 | 70 | 1,27 | 220–250 |
| ABS | 2000 | 1700 | 0,35 | 35 | 20 | 105 | 90 | 1,04 | 230–260 |
| ASA | 2000 | 1700 | 0,35 | 40 | 25 | 100 | 90 | 1,07 | 240–260 |
| HIPS | 1800 | 1500 | 0,35 | 25 | 15 | 100 | 85 | 1,04 | 220–250 |
| PC | 2300 | 2000 | 0,37 | 55 | 35 | 147 | 115 | 1,20 | 260–300 |
| PA (nylon) | 1500 | 1200 | 0,39 | 45 | 30 | 50 | 90 | 1,14 | 240–280 |
| PA con fibra de carbono | 6000 | 2500 | 0,35 | 70 | 35 | 60 | 140 | 1,18 | 260–290 |
| PC con fibra de carbono | 5000 | 2300 | 0,35 | 65 | 35 | 147 | 120 | 1,22 | 270–300 |
| PP | 1000 | 800 | 0,40 | 25 | 15 | −10 | 90 | 0,90 | 220–250 |
| POM | 2400 | 2000 | 0,37 | 50 | 25 | −60 | 110 | 1,41 | 210–230 |
| PEI (ULTEM 9085) | 2200 | 2000 | 0,36 | 70 | 40 | 186 | 150 | 1,27 | 350–390 |
| PEEK | 3500 | 3000 | 0,38 | 90 | 50 | 143 | 150 | 1,30 | 370–410 |
| PEKK | 3000 | 2600 | 0,38 | 80 | 45 | 160 | 150 | 1,30 | 340–380 |
| PSU | 2400 | 2100 | 0,37 | 60 | 35 | 185 | 170 | 1,24 | 340–380 |
| PVDF | 1800 | 1500 | 0,38 | 40 | 25 | −35 | 120 | 1,78 | 240–260 |
| TPU (elastómero, no aplicable) | 30 | 25 | 0,45 | 30 | 20 | −40 | 60 | 1,21 | 210–240 |

Las resinas (impresión SLA) son casi isótropas; sus valores vienen de las hojas de los fabricantes, post-curadas:

| Resina | E (MPa) | S (MPa) | Máx. servicio (°C) | Fuente |
|---|---|---|---|---|
| Estándar (Elegoo) | 709 (módulo a flexión) | 33,8 | 50 (supuesto) | Hoja de Elegoo Standard |
| Estándar (Formlabs) | 2800 | 65 | 73,1 | Formlabs FLGPCL03, 60 min a 60 °C |
| Tough 2000 (Formlabs) | 1800 | 40,4 | 70 | Formlabs Tough 2000 V2 |

La densidad de las resinas no está en las hojas: se usa 1,15 g/cm³, típica de fotopolímeros curados, y ν = 0,35.

## Pruebas

Valores de referencia. «Teoría» es cálculo a mano. «CalculiX» es el programa libre CalculiX 2.21, usado como caja
negra con hexaedros cuadráticos de 20 nodos. Solo se usan sus resultados, no su código.

| # | Caso | Resultado de Tisma | Referencia |
|---|---|---|---|
| E1 | Barra de PLA de 50 × 10 × 10 mm, empotrada en un extremo, 1000 N de tracción en el otro | 0,1633 mm; 10,000 MPa | Teoría: `F L / (E A)` = 0,1667 mm (−2 %: el empotramiento impide la contracción lateral); 10 MPa |
| E2 | Voladizo de PLA de 100 × 10 × 10 mm, 10 N en el extremo, vóxel de 2,5 mm | Flecha 1,257 mm | Teoría con corte: 1,344 mm (−6,5 %) |
| E3 | El mismo con vóxel de 1,25 mm | 1,306 mm | −2,8 %: converge al achicar el vóxel |
| E4 | Barra de 60 × 10 × 10 mm, 1000 N de tracción: desplazamiento a 59,5 mm y σxx a mitad | 0,1966 mm; 10,00 MPa | CalculiX: 0,1969 mm; 10,00 MPa |
| E5 | Voladizo horizontal de 60 × 10 × 10 mm, 100 N en el extremo: flecha y σxx en la fibra de arriba a mitad | −2,849 mm; 15,83 MPa | CalculiX: −2,871 mm; 15,93 MPa. Teoría: 2,88 mm; 15,93 MPa |
| E6 | Voladizo vertical de 10 × 10 × 60 mm (a lo largo de Z, rige `E_z`), 50 N: flecha y σzz a mitad | 1,639 mm; 7,91 MPa | CalculiX: 1,652 mm; 7,96 MPa |
| E7 | Escuadra en L: base de 40 × 10 × 10 mm y montante de 10 × 10 × 30 mm, 50 N arriba | 1,089 mm; σxx en la base 9,88 MPa | CalculiX: 1,102 mm; 9,94 MPa |
| E8 | Barra de PETG a 23 °C y a 50 °C, misma carga | Razón de desplazamientos = f(23) / f(50) | Modelo de temperatura (Método, punto 5) |
| E9 | Barra de PA con fibra de carbono, a lo largo de X y a lo largo de Z | Razón de desplazamientos = `E_xy / E_z` (±5 %) | Material, Método, punto 4 |
| E10 | PLA a 130 °C | «Fuera de temperatura»; sugiere PEI, entre otros | Tabla |
| E11 | Barra de PLA con 60 MPa a lo largo de las capas, y con 35 MPa a través | «Fuera de carga» en los dos | `S_xy` 50, `S_z` 30 |
| E12 | Barra de 80 × 10 × 10 mm, paredes de 0,9 mm, relleno del 20 % y del 60 % | Masa 4,53 g y 7,22 g (maciza: 9,92 g, exacto). Con menos relleno, más flexible y menos resistente | Coherencia del modelo |

Los casos E4 a E7 usan el PLA de la tabla con ν entre plano y eje (el que pide CalculiX) `ν · √(E_xy / E_z)`.
Tolerancia de la comparación con CalculiX: 3 %. El análisis de Tisma queda alrededor de 1 % más rígido, lo esperable
de un hexaedro lineal frente a uno cuadrático.

## Límites

- **Relleno:** se trata como un material continuo más blando; no se modelan las trayectorias reales.
- **Materiales:** valores típicos que cambian mucho con la impresión (temperatura, ventilación, orientación), sobre
  todo la adherencia entre capas.
- **Concentraciones:** en cargas puntuales y bordes de empotramientos, la tensión depende del tamaño del vóxel. El
  punto crítico se informa para que se revise.
- **Lineal:** pequeñas deformaciones y material elástico. No hay pandeo, fluencia lenta ni fatiga.
- **Bordes escalonados:** los vóxeles aproximan las superficies curvas; el error baja al achicar el vóxel.
