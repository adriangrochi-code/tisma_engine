# Tiempo de impresión estimado

**Qué hace:** antes de imprimir, calcula cuánto va a tardar la pieza, simulando cada movimiento del G-code con los
límites de la máquina (aceleraciones, velocidades máximas, tirones). El dueño quiere la estimación «lo más precisa
posible». Esta ficha dice qué usa y qué deja afuera la de Tisma Slicer, medido como caja negra, y qué tiene que hacer
el motor para ser más preciso.

**Fase:** después de F4, con `gcode_firmware.md`.

**Fuentes:**
- Marlin, [M201](https://marlinfw.org/docs/gcode/M201.html), [M203](https://marlinfw.org/docs/gcode/M203.html),
  [M204](https://marlinfw.org/docs/gcode/M204.html), [M205](https://marlinfw.org/docs/gcode/M205.html) y
  [M31](https://marlinfw.org/docs/gcode/M031.html) (tiempo de la impresión según el firmware);
- Klipper, [Status reference, `print_stats`](https://www.klipper3d.org/Status_Reference.html#print_stats)
  (`print_duration`) y [`SET_VELOCITY_LIMIT`](https://www.klipper3d.org/G-Codes.html#set_velocity_limit);
- las mediciones de abajo, del 25-10-2026, con Tisma Slicer desde la línea de comandos.

## Ajustes

Usa los de `gcode_firmware.md` (`machine.flavor`, `machine.limits_usage`) y los límites de la máquina del perfil de la
impresora (`profiles.md`): aceleración máxima al extruir, al retraer y al viajar; velocidad máxima y aceleración
máxima por eje; tirón por eje o, en Marlin, desviación de unión (`M205 J`); velocidades mínimas.

Cada límite tiene **dos valores**: el normal y el silencioso (para el modo silencioso de algunas impresoras Prusa).

## Lo que mide Tisma Slicer

1. **Según el firmware:**

| `machine.flavor` | Límites de la máquina | Modos |
|---|---|---|
| `reprap` | No los usa: cambiar la aceleración no cambia el tiempo | Normal |
| `marlin` | Los usa | Normal y silencioso (los segundos valores) |
| `klipper` | Los usa; no se pueden escribir en el G-code («Machine limits cannot be emitted to G-Code when Klipper firmware flavor is used») | Normal |

2. **`machine.limits_usage`:**
   - `time_only` (por defecto) y `emit`: la estimación usa los límites del perfil. Con `emit`, además se escriben al
     comienzo (`M201`, `M203`, `M204`, `M205`) y el tiempo cambia en 2 s por esos comandos;
   - `ignore`: la estimación no usa los límites del perfil (con aceleración 500 da lo mismo que con 1500, el valor
     de fábrica).
3. **Qué límites cambian el tiempo** (Marlin, cubo de las pruebas, 10 min 39 s de base): la aceleración al extruir y
   al viajar, el tirón por eje, la desviación de unión (si es mayor que 0 reemplaza al tirón) y la velocidad máxima
   por eje. Las aceleraciones por tipo (`acceleration.default` y las demás de `speeds_accels.md`) también (prueba
   PT3).
4. **Comandos del G-code de inicio:** lee `M201`, `M203`, `M204` y `M205` y los aplica al resto del cálculo, solo en
   el modo normal. En Klipper lee `M204 S`, pero no `SET_VELOCITY_LIMIT` (prueba PT4).
5. **Lo que cuenta:**
   - retracciones y vueltas (con su largo y su velocidad);
   - saltos en Z;
   - cambios de capa;
   - el frenado por tiempo mínimo de capa (`cooling.md`): se calcula con las velocidades ya frenadas.
6. **Lo que no cuenta:**
   - el calentamiento: un `M109` o `M190` en el G-code de inicio no suma tiempo;
   - las esperas `G4 S…` o `G4 P…` en el G-code propio (inicio, antes del cambio de capa): 49 esperas de 1 s no
     cambiaron el total (prueba PT5).
7. **Salida:** comentarios al final del G-code:
   - `; estimated printing time (normal mode) = 10m 39s` y, en Marlin, `(silent mode)`;
   - `; estimated first layer printing time (normal mode) = 42s`;
   - con avance activo (`gcode.progress`), `M73` con el porcentaje y los minutos restantes (`gcode_firmware.md`).

## Para que el motor sea más preciso

8. **Usar lo que el firmware ejecuta:** los límites de la máquina, más los `M201`/`M203`/`M204`/`M205` y, en Klipper,
   `SET_VELOCITY_LIMIT` (`ACCEL`, `VELOCITY`, `SQUARE_CORNER_VELOCITY`, `MINIMUM_CRUISE_RATIO`) que aparezcan en
   cualquier parte del G-code, no solo en el inicio.
9. **Contar las esperas:** `G4 S` (segundos) y `G4 P` (milisegundos) suman su tiempo.
10. **Calentamiento:** con las temperaturas del inicio y una velocidad de calentamiento de la boquilla y de la cama
    en el perfil de la impresora (°C/s), el tiempo hasta llegar a cada `M109`/`M190`. Sin ese dato, mostrar el tiempo
    de impresión aparte del de calentamiento.
11. **Comparar con la impresora:** la única forma de saber si la estimación es precisa. Guardar, por impresora, pares
    (estimación, tiempo real) de impresiones terminadas: el tiempo que da el firmware (`M31` en Marlin,
    `print_duration` en Klipper) o el que informa la impresora por red (`devices.md`). Con varios pares, el motor
    puede corregir la estimación de esa impresora con un factor medido y mostrar el error típico.

**Pares medidos con impresoras reales:** no hay. Este entorno no tiene impresoras; Tisma Slicer tampoco guarda la
duración real de las impresiones. Quedan para cuando el dueño imprima las piezas de abajo (prueba PT6).

## Lo que hace el motor hoy

Medido el 26-10-2026, motor de `dev` en `5b0bdbd`, cubo de 20 × 20 × 10 mm con los valores por defecto: 11 min 10 s.

| Cambio | Estimación del motor | Lo que hace la impresora |
|---|---|---|
| `machine.flavor` `marlin`, `klipper` o `reprap` | 11 min 10 s en los tres | Cada firmware pasa las esquinas de otra forma (regla 12) |
| `machine.acceleration` y `travel_acceleration` 500 | 12 min 17 s | Igual: lo cuenta |
| Retracción de 5 mm | 11 min 56 s | Igual: lo cuenta |
| `machine.corner_speed` 2 / 20 | 11 min 37 s / 10 min 45 s | Igual: lo cuenta |
| `G4 S60` en el G-code de inicio | Sin cambio | 60 s más |
| `M190 S60` y `M109 S200` en el inicio | Sin cambio | El calentamiento: minutos |
| `G28` y `G29` en el inicio | Sin cambio | Ir al origen y medir la cama: de 1 a 15 min según la impresora |
| `M204 S500` en el inicio (Marlin) | Sin cambio | Lo mismo que bajar la aceleración: unos 67 s más |
| `SET_VELOCITY_LIMIT ACCEL=500` en el inicio (Klipper) | Sin cambio | Ídem |

El motor tampoco escribe el tiempo de la primera capa ni separa el tiempo del inicio del de la impresión.

## Para que diga una hora y sea una hora

Pedido del dueño (26-10-2026): que la estimación coincida con la realidad; hoy otros programas se equivocan por varios
minutos o más (dice una hora y tarda una hora y media).

12. **Esquinas como cada firmware** (fórmulas publicadas por cada proyecto):
    - Marlin con desviación de unión (`M205 J`): la velocidad en la esquina sale de la aceleración, la desviación y el
      ángulo entre los dos movimientos ([Marlin, M205](https://marlinfw.org/docs/gcode/M205.html));
    - Marlin o RepRapFirmware con tirón: el cambio de velocidad permitido por eje;
    - Klipper: `square_corner_velocity` y `minimum_cruise_ratio`; este último baja la aceleración efectiva de los
      movimientos cortos ([Klipper, Kinematics](https://www.klipper3d.org/Kinematics.html));
    - cada uno con su anticipación: el firmware frena antes de una esquina mirando los movimientos que vienen.
13. **Todo lo que cambia el movimiento, en cualquier parte del G-code:** `M201`, `M203`, `M204`, `M205`, `M220`
    (porcentaje de velocidad) y `SET_VELOCITY_LIMIT`, incluso los que escriben las plantillas de cambio de capa o de
    filamento (regla 8).
14. **El inicio y las esperas, contados y mostrados aparte:**
    - `G4` (regla 9);
    - calentamiento: con las temperaturas de partida (de ambiente, o las de la impresión anterior si el programa
      está conectado a la impresora) y la velocidad de calentamiento de la cama y la boquilla del perfil (regla 10);
    - `G28`, `G29` y las rutinas de calibración del inicio (las de cada marca, por ejemplo la nivelación y la
      vibración de las Bambu Lab): un tiempo por comando en el perfil de la impresora, medido;
    - cambios de filamento y de herramienta: el tiempo de su plantilla más la purga;
    - la salida dice, por ejemplo, «1 h 4 min (impresión 58 min, preparación 6 min)».
15. **Aprender de cada impresora** (amplía la regla 11):
    - con la impresora conectada (`devices.md`), cada impresión terminada guarda la estimación por partes (primera
      capa, paredes, rellenos, viajes, preparación) y el tiempo real que informa la impresora;
    - con 3 o más impresiones, el motor ajusta un factor por parte para esa impresora (mínimos cuadrados) y usa el
      ajustado;
    - muestra el error típico: «1 h ± 2 min (ajustado con 8 impresiones)»; sin datos, «1 h (sin ajustar)»;
    - sin conexión, la persona puede escribir el tiempo real al terminar.
16. **Durante la impresión:** el tiempo restante (`M73`, `gcode_firmware.md`) se recalcula con el avance real: si la
    impresora va más lenta que lo estimado, el resto se corrige en la misma proporción.

**Metas** (con la impresora real del dueño, prueba PT6):
- sin ajustar: error de 5 % o menos en el cubo, el cilindro y la pieza de 2 h, en Marlin y en Klipper;
- ajustada con 5 impresiones: error de 2 % o menos (en 1 h, menos de 1 min 12 s).

## Pruebas

Medidas en Tisma Slicer (25-10-2026) con los ajustes base de `walls_extra.md`, relleno al 20 %, 3 capas sólidas
arriba y abajo y E absoluta. **Cubo:** 20 × 20 × 10 mm.

| # | Ajustes | Resultado |
|---|---|---|
| PT1 | `reprap`; aceleración 1500 / 500; uso de los límites `time_only` / `emit` / `ignore` | 10 min 39 s en todos; con `emit` no escribe ningún `M20x` |
| PT2 | `marlin`; `time_only` / `ignore` / `emit` | 10 min 39 s y silencioso 11 min 0 s; con `ignore` igual; con `emit`, 10 min 37 s y 5 líneas `M201`…`M205` |
| PT3 | `marlin`: aceleración al extruir y al viajar 500; tirón X e Y 2; desviación de unión 0,01; velocidad máxima X e Y 50; `acceleration.default` 500 | 11 min 51 s; 11 min 3 s; 10 min 45 s; 11 min 30 s; 12 min 5 s. Con modo silencioso y 300 de aceleración en los segundos valores: normal 10 min 39 s, silencioso 12 min 56 s |
| PT4 | `marlin`, G-code de inicio `M204 P500 T500` / `M201 X500 Y500` / `M203 X50 Y50` / `M205 X2 Y2`; `klipper` con `M204 S500` / `SET_VELOCITY_LIMIT ACCEL=500` | Marlin: 12 min 4 s, 11 min 55 s, 11 min 29 s y 11 min 2 s en el normal; el silencioso queda en 10 min 59 s. Klipper: 12 min 4 s con `M204`; 10 min 38 s (sin cambio) con `SET_VELOCITY_LIMIT` |
| PT5 | `marlin`: retracción 0 / 5 mm a 10 mm/s; salto en Z 2 mm; tiempo mínimo de capa 30 s; `G4 S60` en el inicio; `G4 S1` antes de cada cambio de capa; `M109 S200` en el inicio | 10 min 18 s; 12 min 51 s; 11 min 34 s; 27 min 11 s; 10 min 38 s; 10 min 39 s (49 esperas sin contar); 10 min 38 s |
| PT6 | Pendiente con una impresora real: el cubo, un cilindro de 20 × 40 mm y una pieza de 2 h, en Marlin y en Klipper | Estimación del motor contra el tiempo del firmware; se anotan aquí |
| PT7 | Motor: `G4 S60`, `M109`/`M190`, `G28`/`G29` y `M204 S500` en el inicio (26-10-2026) | Hoy no cambian la estimación; deben sumar su tiempo (reglas 13 y 14) |
| PT8 | Motor: el mismo G-code con `marlin`, `klipper` y `reprap`, en una pieza con muchas curvas | Hoy da lo mismo; debe dar el tiempo de cada firmware (regla 12) |
| PT9 | Cinco impresiones reales en la misma impresora | El error ajustado baja a 2 % o menos (regla 15) |
