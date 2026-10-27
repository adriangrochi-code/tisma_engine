# Impresoras de metal por arco (WAAM): estudio

**Qué hace:** laminar para máquinas que depositan metal fundiendo un alambre con un arco de soldadura (wire arc
additive manufacturing). El cabezal es una antorcha. Los cordones son de varios milímetros, el calor se acumula capa
tras capa, y la pieza sale casi siempre para mecanizar después.

**Fase:** después de la primera versión. Pedido del dueño (26-10-2026, «en un futuro»).

Esta ficha junta lo que dice la literatura y propone cómo encajarlo en el motor; las reglas se cierran con una
máquina de prueba.

**Fuentes:**
- **Revisión de WAAM en acero** (ScienceDirect, 2025,
  `https://www.sciencedirect.com/science/article/pii/S2590123025007340`):
  - temperatura entre pasadas de 50 a 120 °C como rango típico, para evitar fisuras y controlar el grano;
  - un estudio la mantuvo en 150 °C con un pirómetro infrarrojo y enfriamiento, y mejoró la precisión.
- **Planificación de trayectorias en intersecciones** (PMC8585445,
  `https://pmc.ncbi.nlm.nih.gov/articles/PMC8585445/`):
  - si las trayectorias no tienen en cuenta el ancho y el alto del cordón, donde los cordones se cruzan la pieza crece
    de más;
  - el ancho y el alto del cordón dependen sobre todo de la velocidad del alambre y de la de avance.
- **Método de trayectorias para procesos con alambre** (Modelling 5(4), 2024,
  `https://doi.org/10.3390/modelling5040105`):
  paso entre cordones vecinos de 0,67 a 0,738 veces el ancho de un cordón.
- **Planificación de trayectorias para WAAM** (ResearchGate 362908640):
  - el perfil de un cordón se modela bien con una parábola;
  - con contornos y zigzag, el solape óptimo ronda la mitad del ancho.
- **Guía de la industria** (Manufactur3D, `https://manufactur3dmag.com/wire-arc-additive-manufacturing-waam-guide/`):
  - capas de 1 a 3 mm y cordones de 3 a 10 mm por pasada;
  - de 1 a 3 mm de sobremedida por cara para mecanizar.

  No es una fuente revisada por pares.

## Ajustes propuestos

| Clave | Por defecto | Qué hace |
|---|---|---|
| `machine.process` | `fff` | `waam` activa este modo (`fff` es el de hoy; resina sigue aparte, `resin.md`) |
| `waam.arc_on_gcode`, `waam.arc_off_gcode` | `""` | G-code que enciende y apaga el arco y el alambre. Cada controlador usa los suyos, así que vienen del perfil de la impresora, con las variables del motor |
| `waam.wire_feed` | — | Velocidad del alambre, m/min, como variable `wire_feed` para esos G-code |
| `waam.bead_width`, `waam.bead_height` | — | Ancho y alto del cordón medidos en la máquina con ese alambre y esa velocidad; reemplazan a `walls.width` y `layers.height` |
| `waam.stepover` | 0,7 | Separación entre cordones vecinos, en veces el ancho (la literatura da de 0,5 a 0,738) |
| `waam.interpass_temperature` | 0 | Temperatura máxima de la pieza para empezar la capa siguiente, °C; 0 no espera por temperatura |
| `waam.interpass_wait_gcode` | `""` | G-code que espera esa temperatura (lectura de un pirómetro, depende de la máquina) |
| `waam.interpass_dwell` | 0 | Espera fija entre capas, s, para máquinas sin pirómetro |
| `waam.machining_allowance` | 0 | Sobremedida en mm: las paredes de afuera salen esto más afuera y los agujeros, más adentro |

## Reglas propuestas

1. **Sin eje E:** en WAAM no se escribe E. Cada tramo de depósito queda entre `waam.arc_on_gcode` y
   `waam.arc_off_gcode`; los viajes van con el arco apagado.
2. **Menos arranques de arco:** cada encendido es un defecto posible. Las trayectorias se unen en el menor número
   de recorridos continuos:
   - contornos paralelos o zigzag continuo en el interior;
   - sin relleno disperso: las piezas salen macizas;
   - sin retracción.
3. **Ancho y paso:** el ancho del cordón es `waam.bead_width` y la separación entre cordones, `waam.stepover` ×
   ancho. El alto de capa es `waam.bead_height`.
4. **Intersecciones:** donde dos cordones se cruzan, uno de los dos se corta antes del cruce, para que la altura no
   crezca de más (PMC8585445). Qué cordón se corta se alterna entre capas.
5. **Calor entre capas:**
   - antes de cada capa se espera con `waam.interpass_wait_gcode` (si hay pirómetro) o `waam.interpass_dwell`;
   - el tiempo estimado suma esas esperas: con `waam.interpass_dwell`, el valor; con temperatura, una estimación
     marcada como tal.
6. **Arranques alternados:** el punto de encendido y el sentido de cada lazo cambian entre capas, para no acumular
   material en los mismos lugares.
7. **Sobremedida:** `waam.machining_allowance` agranda la pieza hacia afuera y achica los agujeros antes de laminar.
   La vista previa muestra la pieza final por adentro.
8. **Salida:** G-code para pórticos CNC con los comandos del perfil. Para robots (lenguajes de cada fabricante de
   brazos) hace falta otra ficha.
9. **Lo que no aplica:** soportes, puentes, ventilador, avance de presión, cambios de filamento.

## Pruebas propuestas

| # | Escenario | Resultado esperado |
|---|---|---|
| WA1 | Pared recta de 100 mm, 10 capas | 10 tramos con el arco encendido y apagado una vez por capa; ningún E |
| WA2 | Cuadrado de 50 mm macizo, cordón de 6 mm, paso 0,7 | Cordones separados 4,2 mm; un recorrido continuo por capa si la forma lo permite |
| WA3 | Cruz de dos paredes | En cada capa, uno de los cordones se corta antes del cruce; la otra pared en la capa siguiente |
| WA4 | `waam.interpass_dwell` 60, 10 capas | 9 esperas de 60 s; el tiempo estimado suma 540 s |
| WA5 | `waam.machining_allowance` 2 en un cilindro de 40 mm con agujero de 10 mm | Se deposita un cilindro de 44 mm con agujero de 6 mm |
