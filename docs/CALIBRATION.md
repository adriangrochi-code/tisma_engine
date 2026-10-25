# Calibraciones

El dueño pidió (2026-10-24):
- todas las calibraciones estándar de una impresora;
- también calibraciones nuevas, que no existen en otros programas pero que sean prácticas;
- la compensación geométrica: medir la pieza y que el laminador corrija, para piezas de muy alta precisión.

Todo es diseño propio: las piezas de prueba las genera el motor (`src/calibration/`), no vienen de archivos ajenos.

## Cómo funciona

El dueño pidió que las pruebas se creen solas: «cómodo, intuitivo y fácil de usar», sin configurarlas a mano. En la
pestaña **Calibración** de la interfaz (`docs/UI.md`) cada prueba es una tarjeta con un botón **Crear**, y los pasos
aparecen en orden:

1. **Crear:** la pieza se arma con un rango elegido desde los ajustes actuales (ver «Rangos automáticos») y se
   lamina sola. La tarjeta dice cuántos segmentos tiene y cuánto tarda, y ofrece **Exportar G-code**.
2. **Imprimir y mirar:** la tarjeta dice qué mirar en esa pieza.
3. **Elegir:**
   - en las torres, cada segmento es un botón con su valor. Al elegir uno, la vista muestra la torre laminada hasta
     ese segmento, y **Usar** pasa el valor a su ajuste (`apply_calibration`) y quita los cambios por altura;
   - en las dimensiones, se cargan las medidas con calibre, la tabla muestra la compensación actual y la nueva, y
     **Aplicar** la pasa a los ajustes.

Quien quiera otro rango lo cambia en **Ajustar el rango (opcional)** y vuelve a crear la torre.

Los tres mecanismos del motor están en `docs/CONFIG.md`:

| Mecanismo | Ajuste | Qué hace |
|---|---|---|
| Compensación | grupo `compensation` | Escala el modelo por eje antes de laminar, alrededor del centro de su base, y agranda o achica los agujeros |
| Flujo | `extrusion.flow` | El porcentaje del volumen que se extruye en cada línea |
| Avance de presión | `extrusion.pressure_advance` | El valor del firmware, escrito al empezar con el comando del dialecto |
| Cambios por altura | lista `changes` | Desde una altura: temperatura, cama, ventilador, retracción (largo y velocidad), flujo, velocidad, avance de presión o G-code propio |

- **El verificador del G-code conoce los cambios por altura** (`docs/GCODE_CHECK.md`): comprueba cada retracción y
  cada extrusión con el valor de su capa. Un G-code de torre se exporta verificado como cualquier otro.
- **El análisis estructural no aplica la compensación:** analiza la pieza como se diseñó, que es lo que la
  compensación hace que salga.

## Catálogo

### Hechas

| Prueba | Pieza | Qué se mide o se mira | Qué se ajusta |
|---|---|---|---|
| **Dimensiones** | Bloque de 30 × 20 × 15 mm con un agujero de 10 mm (lados distintos: X es el largo) | X, Y, Z y el diámetro del agujero, con calibre | `compensation.scale_x`, `_y`, `_z` y `hole_offset` |
| **Temperatura** | Torre de segmentos de 10 mm con cresta de ángulo variable, estante plano sin soporte, perno para empujar, el número y figuras en relieve (ver abajo) | Hasta qué ángulo sale prolija la cresta, el estante, el perno que más resiste, el detalle, los hilos | La temperatura de la boquilla |
| **Retracción** | Dos columnas a 30 mm, segmentos de 5 mm | El primer segmento sin hilos entre columnas | `retraction.length` |
| **Flujo** | Torre de 20 × 20 mm, segmentos de 5 mm. Cada segmento deja una cara superior en el cuello | La cara superior lisa, sin huecos ni rebabas | `extrusion.flow` |
| **Velocidad máxima** | Torre de 30 × 30 mm, segmentos de 5 mm, en % de las velocidades actuales | Dónde las paredes empiezan a fallar: el límite del fusor. Al elegir un segmento, la interfaz muestra el flujo volumétrico de su línea más rápida (mm³/s, `volumetric_flow`) | Las velocidades, por debajo de ese segmento |
| **Primera capa** | Cinco cuadrados de 30 × 30 mm y una capa: en el medio y a 30 mm de cada esquina de la cama (en una cama chica, solo el del medio) | Líneas redondas y separadas: boquilla alta. Rugosa o transparente: baja. Un cuadrado distinto: esa esquina desnivelada | Nada en los ajustes: la altura se corrige en la impresora (Z offset). La interfaz explica qué hacer en cada caso |
| **Velocidad de retracción** | Las dos columnas de la retracción, segmentos de 5 mm | El segmento más lento sin hilos | `retraction.speed` |
| **Avance de presión** | Tubo cuadrado de 36 mm con paredes de 1,6 mm y esquinas vivas, con una ranura entre segmentos de 5 mm. Cada segmento escribe el comando del dialecto (ver abajo) | Esquinas abultadas: poco avance. Finas o con huecos al empezar las líneas: mucho | `extrusion.pressure_advance` |
| **Tolerancias de encastre** | Siete bloques de 14 × 16 × 5 mm en fila, cada uno con un perno de 8 mm impreso adentro de su agujero, con 0,1, 0,15, 0,2, 0,25, 0,3, 0,4 y 0,5 mm de juego en el radio | El de menos juego cuyo perno sale o gira | Nada: es el juego que hay que dejar en los diseños que encastran. La interfaz lo dice al elegirlo |
| **Pasos del extrusor** | Sin pieza: un G-code que calienta y extruye 100 mm a 1 mm/s, con una marca en el filamento 120 mm arriba de la entrada | Cuánto quedó hasta la marca | El valor del firmware: `M92 E` (y `M500`) en Marlin, `M92 E` en `config.g` de RepRapFirmware, `rotation_distance` en `printer.cfg` de Klipper. La interfaz calcula el nuevo y lo deja para copiar |
| **Tope de flujo volumétrico** | El tubo cuadrado del avance de presión, segmentos de 5 mm; en cada segmento todas las líneas corren al flujo del segmento (`changes[].volumetric_flow`), por encima del tope de velocidad y sin el frenado del tiempo mínimo de capa, hasta lo que la máquina se mueve | El último segmento con paredes llenas y parejas: desde el siguiente, el fusor no funde a tiempo | `machine.max_flow`: el motor no pasa de ahí |
| **Resonancias (aceleración)** | El mismo tubo, cada segmento con su aceleración escrita en el comando del dialecto (`changes[].acceleration`: `M204 P… T…`, o `SET_VELOCITY_LIMIT ACCEL=…` en Klipper) | Las ondas en las paredes justo después de cada esquina: el último segmento sin ondas | `machine.acceleration` (y `travel_acceleration`, si quedaba más baja) |
| **Pata de elefante** | Bloque de 20 × 20 × 6 mm | El ancho del borde de abajo (la primera capa) y el del cuerpo, con calibre, del mismo lado | `slicing.first_layer_shrink`: el actual más la mitad del exceso (`elephant_foot_from`) |

Cada segmento de una torre lleva su valor escrito en relieve en la cara del frente (−Y), con una tipografía propia
de siete segmentos hecha de trazos (`label_strokes`, `add_label`). Así la pieza impresa se lee sin la pantalla.

#### Rangos automáticos

| Prueba | Rango | Por qué |
|---|---|---|
| Temperatura | La temperatura de la boquilla en uso, redondeada a 5 °C, ± 20 °C, de a 5 (9 segmentos, de la más caliente a la más fría) | Cubre el rango útil de casi cualquier filamento alrededor de lo que ya funciona |
| Retracción | Si el largo actual es de 1,5 mm o menos: de 0,2 a 2,0 mm de a 0,2. Si no: de 1,0 a 6,0 mm de a 0,5 | Un largo corto indica extrusor directo; uno largo, tubo Bowden |
| Flujo | El flujo actual ± 10 %, de a 2,5 | Corrige alrededor de lo que hay |
| Velocidad máxima | De 100 a 300 % de las velocidades actuales, de a 25 | Desde lo que ya imprime bien hasta el triple |
| Velocidad de retracción | De 20 a 60 mm/s, de a 5 | El rango habitual de directos y Bowden |
| Avance de presión | Extrusor directo (retracción de 1,5 mm o menos): de 0 a 0,08, de a 0,01. Bowden: de 0 a 0,8, de a 0,1 | Un tubo Bowden necesita unas diez veces más |
| Tope de flujo volumétrico | Si `machine.max_flow` se conoce (del catálogo o de una torre anterior): de la mitad a una vez y media, en unos diez pasos. Si no: de 4 a 30 mm³/s de a 2 | El rango de los fusores comunes, o alrededor del tope que ya se sabe |
| Resonancias (aceleración) | De 500 mm/s² hasta el doble de la aceleración en uso (al menos 5000), en unos diez pasos de 500 o más | Desde lo que cualquier máquina aguanta hasta el doble de lo que se usa |

**Al usar un valor** (`apply_calibration`):
- la temperatura va a la variable `temperature` (la que usa el G-code de inicio);
- la retracción a `retraction.length`, y su velocidad a `retraction.speed`;
- el flujo a `extrusion.flow`;
- el avance de presión a `extrusion.pressure_advance`;
- la velocidad multiplica las velocidades de paredes, relleno, relleno sólido, superficie, soportes y la máxima (y la
  de la pared exterior si es absoluta);
- en todos los casos se quitan los cambios por altura de la torre.

#### La torre de temperatura

Pedido del dueño: una cresta de ángulo variable para ver dónde se deforma, una zona para probar la resistencia
haciendo fuerza, detalle en letras y figuras, y una zona plana para probar el techo sin soporte del futuro Arc
Overhang. Cada segmento (10 mm, 9 por defecto) es un bloque de 24 × 12 mm con:

| Zona | Dónde | Qué es | Qué se mira |
|---|---|---|---|
| **Cresta de ángulo variable** | Izquierda (−X) | Un voladizo de 4 mm de alto cuyo ángulo desde la vertical crece a lo largo de la cresta, de 30° (frente) a 72° (fondo) | Hasta qué ángulo sale prolija: a más temperatura, el plástico tarda más en enfriarse y el voladizo cae antes |
| **Estante plano** | Derecha (+X) | Una losa de 8 × 8 mm y 1,6 mm de espesor, en el aire, sin nada debajo | Cómo queda un techo plano sin soporte a cada temperatura. Es la zona de prueba del **Arc Overhang** (ver abajo) |
| **Perno de resistencia** | Atrás (+Y) | Un perno de 2,4 × 2,4 mm parado sobre una ménsula a 45° | Se empuja con el dedo: se rompe entre capas, así que el segmento cuyo perno más resiste une mejor las capas |
| **Detalle** | Frente (−Y) | El número de la temperatura, un rombo y un anillo octogonal, en relieve de 0,6 mm | Cómo trata la temperatura los detalles chicos: bordes, puntas, hilos |

Un cuello de 1 mm, más angosto, separa los segmentos: así se distinguen y cada uno tiene su cara superior.

**Arc Overhang (planeado):** el dueño va a trabajar en una función de voladizos por arcos: los cordones crecen en
arcos concéntricos desde el borde apoyado, cada uno sobre el anterior, para imprimir un techo plano en el aire sin
soporte. Se implementará en sala limpia (`CLEANROOM.md`), desde la descripción pública de la idea y con diseño
propio, sin leer el código de ningún programa GPL. El estante de la torre ya sirve para comparar: hoy sale con
puentes y voladizo común; con la función, debería salir plano.

**Cómo se calcula la compensación** (`compensation_from`):
- **Escala:** la pieza se imprimió con las escalas actuales, así que cada escala nueva es la actual × nominal /
  medida.
- **Agujeros:** el agujero va a crecer con las escalas nuevas de X e Y. Lo que queda de su error, en el diámetro, se
  corrige moviendo el borde del agujero la mitad (`hole_offset`, en el radio).
- **Medida en 0:** deja ese valor como estaba.
- **Límites:** los rangos de `docs/CONFIG.md`.

#### El avance de presión

Cada segmento de la torre escribe, al empezar, el comando de su dialecto (`pressure_advance_gcode`, desde la
documentación pública de cada firmware):

| Dialecto | Comando |
|---|---|
| Marlin | `M900 K…` (linear advance: el firmware tiene que tenerlo compilado, `LIN_ADVANCE`) |
| Klipper | `SET_PRESSURE_ADVANCE ADVANCE=…` |
| RepRapFirmware | `M572 D0 S…` |

El valor elegido queda en `extrusion.pressure_advance`, que se escribe una vez al empezar cada impresión. Con 0 no
se escribe nada y vale lo que tenga la impresora.

### Siguientes (estándar)

| Prueba | Pieza o G-code | Qué ajusta |
|---|---|---|
| Puentes y voladizos | Puentes de 10 a 50 mm y voladizos de 20° a 70° | El ángulo de los soportes; la velocidad y el ventilador de puentes cuando existan esos ajustes |

### Nuevas (propias de este motor)

| Prueba | Idea | Por qué sirve |
|---|---|---|
| **Compensación por mínimos cuadrados** | Una pieza con varios largos (10 a 80 mm), agujeros y pernos (3 a 20 mm). Con todas las medidas se ajusta medida = escala × nominal + 2 × desplazamiento, por separado para largos, agujeros y pernos | Separa la contracción (escala) del exceso de línea (desplazamiento), que un solo cubo mezcla. Da precisión de centésimas en piezas de cualquier tamaño |
| **Ancho de línea variable** | Paredes de un solo cordón de 0,3 a 1,2 mm, el rango que usan las paredes de ancho variable del motor (Kuipers et al. 2020); se mide cada una | El flujo real no es lineal en los extremos: sale una curva de corrección por ancho, que solo tiene sentido en un motor con anchos variables |
| **Adherencia entre capas con el análisis** | Una probeta normalizada, que se carga hasta que se rompe (o una por segmento de la torre de temperatura) | El análisis del motor (A1) calcula la tensión de rotura con la carga que se anotó. Así se calibra la resistencia entre capas del material en esa impresora y a esa temperatura, y los factores de seguridad del análisis pasan a ser los reales |
| **Contracción por material** | La compensación se guarda con el perfil de filamento (**hecho**, `docs/PROFILES.md`). Falta: una pieza grande para medir la contracción de cada material con más precisión | Cada material se contrae distinto |
| **Laminado no planar: altura y ángulo** | Rampas y cúpulas con capas curvas de pendiente creciente; se mira dónde roza la boquilla | Da los límites de la máquina para las capas curvas de F7. **A definir con el dueño** cuando llegue F7 |

## En el código

- **`src/calibration/calibration.*`:**
  - las piezas: `dimension_test`, `first_layer_test`, `fit_test`, y las torres `temperature_tower`,
    `retraction_tower`, `retraction_speed_tower`, `flow_tower`, `speed_tower`, `pressure_advance_tower`;
  - `calibration_kind_name`, `is_tower`, `calibration_tower` (una torre con otro rango);
  - `volumetric_flow`;
  - los pasos del extrusor: `extruder_test_gcode`, `extruder_steps_from`, `extruder_steps_text`;
  - `tower_values`;
  - `automatic_calibration` (los rangos automáticos) y `apply_calibration` (usar un valor);
  - la tipografía en relieve: `label_strokes`, `add_label`;
  - `compensation_from`.
- **`compensated` (`src/slice/pipeline.*`):** la escala.
- **`offset_holes` (`src/geom/polygons.*`):** los agujeros.
- **Cambios por altura:** el escritor y el verificador del G-code. **`pressure_advance_gcode`** (`src/gcode/writer.*`):
  el comando de cada dialecto.
- **`apps/tisma/app.cpp`:** la pestaña.
- **Pruebas:** `tests/test_calibration.cpp`:
  - la pieza tiene su volumen exacto y el laminado su área;
  - la escala y los agujeros mueven lo que deben;
  - la compensación desde medidas;
  - cada torre escribe sus valores, y el verificador la rechaza si no conoce los cambios;
  - las zonas de la torre de temperatura están donde deben, capa por capa (cresta, estante, perno);
  - cada calibración automática se lamina y pasa el verificador, y usar un valor lo deja en su ajuste;
  - la torre de velocidad de retracción retrae a la velocidad de cada segmento;
  - el avance de presión en los tres dialectos, en orden, y el ajuste;
  - los cuadrados de la primera capa (una capa, en su lugar), los pernos de encastre (cada juego queda abierto, el
    de 0,1 mm también), el flujo volumétrico y la cuenta de los pasos del extrusor.
