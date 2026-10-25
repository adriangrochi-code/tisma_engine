# Pressure advance en el G-code (estudio)

**Qué hace:** escribe en el G-code el adelanto del extrusor que haría el pressure advance del firmware. Es para
impresoras cuyo firmware no lo tiene (muchas Marlin baratas lo traen apagado). Sin él, el plástico sale tarde al
acelerar y sigue saliendo al frenar: esquinas abultadas, comienzos de línea flacos y costuras marcadas.

**Fase:** después de F4, opcional.
- Es un estudio del equipo del dueño, con una simulación y sin impresiones reales.
- Tisma Slicer todavía no lo hace.
- El motor lo implementa solo si el dueño lo pide, y antes de darlo por bueno hay que imprimir.

**Fuentes:**
- Klipper, [Kinematics](https://www.klipper3d.org/Kinematics.html): la posición con pressure advance es la nominal más
  `pressure_advance · velocidad nominal` del extrusor. [Pressure advance](https://www.klipper3d.org/Pressure_Advance.html):
  valores típicos de 0,050 a 1,000 s, los altos con extrusor Bowden.
- Marlin, [Linear Advance](https://marlinfw.org/docs/features/lin_advance.html): el filamento entre el engranaje y la
  boquilla se comporta como un resorte de constante K. [M205](https://marlinfw.org/docs/gcode/M205.html) (jerk y
  desviación de unión) y [M204](https://marlinfw.org/docs/gcode/M204.html) (aceleración).
- S. Jeon, [«Improving Grbl: Cornering Algorithm»](https://onehossshay.wordpress.com/2011/09/24/improving_grbl_cornering_algorithm/),
  2011: velocidad en las uniones con desviación de unión (junction deviation).
- `gcode_motion.md`: aceleraciones y estimación de tiempo del motor.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `gcode.pressure_advance_in_gcode` | — | no | Escribir el adelanto en el G-code, para firmwares sin pressure advance |
| `filament.pressure_advance` | s | 0 | K del material (de la prueba de pressure advance, `calibrations.md`) |
| `gcode.pressure_advance_piece` | mm | 1 | Largo mínimo de los trozos en que se cortan las rampas |

## Modelo

- Lo que sale por la boquilla, `D`, sigue a lo que empujó el extrusor, `E`, con un retardo `τ`:
  `dD/dt = (E − D) / τ`.
- Si el extrusor va adelantado `τ · dD/dt`, la boquilla deposita justo lo planeado. `τ` es el K del firmware.
- El adelanto, en mm de filamento, es `K · (área de la línea / área del filamento) · v`, con `v` la velocidad del
  cabezal en ese punto.
- Ese adelanto depende solo de la velocidad del cabezal, y la velocidad la decide el planificador del firmware:
  rampas trapezoidales de aceleración y un límite en cada unión. Si el motor reproduce ese planificador, sabe la
  velocidad en cada punto y puede escribir el adelanto.

## Reglas

1. **Planificar como el firmware:** velocidades en las uniones (jerk clásico o desviación de unión, según la
   impresora), rampas de aceleración y de frenada, con los límites del perfil de la impresora. Los valores reales de
   cada impresora se leen con `M503`.
2. **Cortar las rampas** en trozos iguales de al menos `gcode.pressure_advance_piece`. Lo que va a velocidad constante
   queda entero. No conviene bajar de 1 mm: con desviación de unión, el firmware frena en los movimientos muy cortos
   casi en línea. En la simulación, un trozo de 0,5 mm a 500 mm/s² no pasó de 87 mm/s.
3. **Filamento de cada trozo:** el nominal, más el adelanto al final, menos el adelanto al comienzo. Al acelerar
   empuja de más; al frenar empuja de menos, o retrae un poco.
4. **Volver a planificar** los trozos con su filamento nuevo, porque el extrusor también cuenta en las uniones. Repetir
   3 veces.
5. **Límite del extrusor en las uniones:**
   - el adelanto hace saltar la velocidad del extrusor en `K · (área de la línea / área del filamento) · aceleración`
     donde empieza o termina una rampa, y en el doble donde una aceleración pasa directo a frenada;
   - ese salto tiene que caber en lo que el firmware deja cambiar de golpe: el jerk del extrusor (`M205 E`), o, con
     desviación de unión, unos `√(8 · aceleración · desviación)` (7,2 mm/s con 0,013 mm y 500 mm/s²; 17,9 mm/s con
     0,08 mm);
   - si no cabe, por orden: subir ese límite al empezar la impresión (`M205 E…` o `M205 J…`) y devolverlo al final;
     bajar la aceleración (`M204`) donde no entra; o compensar solo una parte ahí. `M205 J` también deja tomar más
     rápido las esquinas en X e Y (más vibración); `M205 E` solo toca el extrusor.
6. **Ejemplo:** línea de 0,45 × 0,2 mm (área de la línea / área del filamento = 0,037) a 500 mm/s².
   - Extrusión directa con K = 0,05 pide 0,9 mm/s (1,9 en las esquinas): entra con los valores de fábrica.
   - Bowden con K = 0,5 pide 9,4 mm/s (18,7 en las esquinas): no entra con jerk E 5 ni con desviación de 0,013 mm.
7. **Más movimientos:** de 7 a 20 veces más líneas en los tramos que aceleran. Un firmware con cola corta, por USB o en
   una placa de 8 bits, puede vaciar la cola y frenar. Hay que medirlo en una impresora real.

## Otras funciones de firmwares nuevos

| Función del firmware | ¿En el G-code? | Comentario |
|---|---|---|
| Arcos `G2`/`G3` | Sí: no usarlos | Líneas rectas |
| Retracción del firmware (`G10`/`G11`) | Sí | Retracción del slicer |
| Cambio de color (`M600`) | Sí | Pausa con el G-code de la impresora |
| Cancelar objetos | En el programa que maneja la impresora | Comentarios con el nombre de cada objeto |
| Corrección de escuadra | Sí: transformación afín de X e Y de todo el G-code | Factible y simple; falta una prueba que mida la escuadra |
| Nivelación con malla | Parcial: modular Z en las primeras capas con una malla medida a mano | Poco beneficio y riesgo de choque si la malla está mal |
| Pressure advance | Sí | Esta ficha |
| Input shaping | En teoría, suavizando la trayectoria | No recomendado: el método conocido (filtered B-splines) se vende como producto comercial y no se revisaron patentes; además hay que medir las resonancias de cada impresora |

## Pruebas

Resultados de la simulación del equipo del dueño, con su propio modelo del planificador. No son impresiones.
- **Mismos supuestos:** aceleración de 500 mm/s²; línea de 0,45 × 0,2 mm; filamento de 1,75 mm; la boquilla con el
  modelo de arriba.
- **Medida:** el ancho depositado a lo largo del camino, sin el primer y el último milímetro, con 1,00 = el planeado.
  Se da el mínimo, el máximo y el error RMS.
- **Referencia:** el pressure advance ideal del firmware da 1,00 en todos los casos.

| # | Caso | Sin pressure advance | Con el adelanto en el G-code |
|---|---|---|---|
| PA1 | Directa, K = 0,05, jerk clásico (X/Y 10, E 5 mm/s); cuadrado de 20 mm a 60 mm/s | 0,45 a 2,78 (0,285) | 0,89 a 1,11 (0,025); de 5 a 35 movimientos |
| PA2 | Lo mismo a 100 mm/s | 0,45 a 2,98 (0,369) | 0,89 a 1,09 (0,025); de 5 a 98 movimientos |
| PA3 | Directa, jerk clásico; línea recta 30 → 120 → 30 mm/s | 0,51 a 1,82 (0,190) | 0,98 a 1,10 (0,014) |
| PA4 | Directa, desviación de unión 0,013 mm; cuadrado a 60 mm/s | 0,45 a 3,98 (0,361) | 0,79 a 1,11 (0,044) |
| PA5 | Directa, desviación de 0,08 mm; cuadrado a 100 mm/s | 0,45 a 2,99 (0,370) | 0,89 a 1,09 (0,025) |
| PA6 | En PA1 a PA5 | — | El tiempo de impresión cambia menos de ±0,5 % |
| PA7 | Bowden, K = 0,5, jerk E 5 (de fábrica); cuadrado a 60 / 100 mm/s | RMS 0,500 / 0,592 | Falla: RMS 0,736 / 0,455, y la impresión tarda 60 % / 131 % más (el firmware frena en las uniones) |
| PA8 | Bowden, jerk E 15 | — | RMS 0,030 / 0,084 (+6 % de tiempo a 100 mm/s); línea 0,012 |
| PA9 | Bowden, desviación de 0,08 mm | — | RMS 0,034 / 0,036 (+1 %); línea 0,015 |
| PA10 | Bowden, desviación de 0,013 mm (de fábrica) | — | RMS 0,383 / 0,324, con +39 % / +79 % de tiempo: no sirve sin subir el límite |
