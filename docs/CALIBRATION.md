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
| Cambios por altura | lista `changes` | Desde una altura: temperatura, cama, ventilador, retracción, flujo, velocidad o G-code propio |

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
| **Velocidad máxima** | Torre de 30 × 30 mm, segmentos de 5 mm, en % de las velocidades actuales | Dónde las paredes empiezan a fallar: el límite del fusor | Las velocidades, por debajo de ese segmento |

Cada segmento de una torre lleva su valor escrito en relieve en la cara del frente (−Y), con una tipografía propia
de siete segmentos hecha de trazos (`label_strokes`, `add_label`). Así la pieza impresa se lee sin la pantalla.

#### Rangos automáticos

| Prueba | Rango | Por qué |
|---|---|---|
| Temperatura | La temperatura de la boquilla en uso, redondeada a 5 °C, ± 20 °C, de a 5 (9 segmentos, de la más caliente a la más fría) | Cubre el rango útil de casi cualquier filamento alrededor de lo que ya funciona |
| Retracción | Si el largo actual es de 1,5 mm o menos: de 0,2 a 2,0 mm de a 0,2. Si no: de 1,0 a 6,0 mm de a 0,5 | Un largo corto indica extrusor directo; uno largo, tubo Bowden |
| Flujo | El flujo actual ± 10 %, de a 2,5 | Corrige alrededor de lo que hay |
| Velocidad máxima | De 100 a 300 % de las velocidades actuales, de a 25 | Desde lo que ya imprime bien hasta el triple |

**Al usar un valor** (`apply_calibration`):
- la temperatura va a la variable `temperature` (la que usa el G-code de inicio);
- la retracción a `retraction.length`;
- el flujo a `extrusion.flow`;
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

### Siguientes (estándar)

| Prueba | Pieza o G-code | Qué ajusta |
|---|---|---|
| Velocidad de retracción | La torre de retracción, cambiando la velocidad (`changes[].retraction_speed` ya existe) | `retraction.speed` |
| Avance de presión | Torre de esquinas vivas a velocidad alta; cada segmento escribe el comando del dialecto (`M900 K` en Marlin, `SET_PRESSURE_ADVANCE` en Klipper, `M572` en RepRap) con `changes[].gcode` | Un ajuste nuevo de avance de presión |
| Resonancias (input shaping) | Torre con aristas y aceleración creciente por segmento (`M204`, o `SET_VELOCITY_LIMIT` en Klipper) | Aceleraciones y, en Klipper, la frecuencia del filtro |
| Primera capa | Cuadrados de una capa repartidos en la cama | La altura de la boquilla (en la impresora) y `slicing.first_layer_shrink` (pata de elefante) |
| Puentes y voladizos | Puentes de 10 a 50 mm y voladizos de 20° a 70° | Velocidad y ventilador de puentes; el ángulo de los soportes |
| Tolerancias de encastre | Pernos en agujeros con juegos de 0,1 a 0,5 mm | El juego para piezas que encastran |
| Pasos del extrusor | G-code que extruye 100 mm en el aire | La calibración del extrusor (en la impresora) |
| Flujo volumétrico máximo | La torre de velocidad, mostrando los mm³/s de cada segmento | El tope de flujo del filamento |

### Nuevas (propias de este motor)

| Prueba | Idea | Por qué sirve |
|---|---|---|
| **Compensación por mínimos cuadrados** | Una pieza con varios largos (10 a 80 mm), agujeros y pernos (3 a 20 mm). Con todas las medidas se ajusta medida = escala × nominal + 2 × desplazamiento, por separado para largos, agujeros y pernos | Separa la contracción (escala) del exceso de línea (desplazamiento), que un solo cubo mezcla. Da precisión de centésimas en piezas de cualquier tamaño |
| **Ancho de línea variable** | Paredes de un solo cordón de 0,3 a 1,2 mm, el rango que usan las paredes de ancho variable del motor (Kuipers et al. 2020); se mide cada una | El flujo real no es lineal en los extremos: sale una curva de corrección por ancho, que solo tiene sentido en un motor con anchos variables |
| **Adherencia entre capas con el análisis** | Una probeta normalizada, que se carga hasta que se rompe (o una por segmento de la torre de temperatura) | El análisis del motor (A1) calcula la tensión de rotura con la carga que se anotó. Así se calibra la resistencia entre capas del material en esa impresora y a esa temperatura, y los factores de seguridad del análisis pasan a ser los reales |
| **Contracción por material** | Guardar la compensación con el perfil de filamento (pedido de opciones de guardar) | Cada material se contrae distinto |
| **Laminado no planar: altura y ángulo** | Rampas y cúpulas con capas curvas de pendiente creciente; se mira dónde roza la boquilla | Da los límites de la máquina para las capas curvas de F7. **A definir con el dueño** cuando llegue F7 |

## En el código

- **`src/calibration/calibration.*`:**
  - las piezas: `dimension_test`, `temperature_tower`, `retraction_tower`, `flow_tower`, `speed_tower`;
  - `tower_values`;
  - `automatic_calibration` (los rangos automáticos) y `apply_calibration` (usar un valor);
  - la tipografía en relieve: `label_strokes`, `add_label`;
  - `compensation_from`.
- **`compensated` (`src/slice/pipeline.*`):** la escala.
- **`offset_holes` (`src/geom/polygons.*`):** los agujeros.
- **Cambios por altura:** el escritor y el verificador del G-code.
- **`apps/tisma/app.cpp`:** la pestaña.
- **Pruebas:** `tests/test_calibration.cpp`:
  - la pieza tiene su volumen exacto y el laminado su área;
  - la escala y los agujeros mueven lo que deben;
  - la compensación desde medidas;
  - cada torre escribe sus valores, y el verificador la rechaza si no conoce los cambios;
  - las zonas de la torre de temperatura están donde deben, capa por capa (cresta, estante, perno);
  - cada calibración automática se lamina y pasa el verificador, y usar un valor lo deja en su ajuste.
