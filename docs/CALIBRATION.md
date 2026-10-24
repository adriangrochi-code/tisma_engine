# Calibraciones

El dueño pidió (2026-10-24):
- todas las calibraciones estándar de una impresora;
- también calibraciones nuevas, que no existen en otros programas pero que sean prácticas;
- la compensación geométrica: medir la pieza y que el laminador corrija, para piezas de muy alta precisión.

Todo es diseño propio: las piezas de prueba las genera el motor (`src/calibration/`), no vienen de archivos ajenos.

## Cómo funciona

1. En la pestaña **Calibración** de la interfaz (`docs/UI.md`) se elige una prueba y se carga su pieza.
2. Se lamina e imprime como cualquier otra. Las torres traen sus **cambios por altura**: cada segmento prueba un
   valor (temperatura, retracción, flujo, velocidad) y el G-code lo cambia al empezar el segmento.
3. Se mide o se mira la pieza impresa y se carga el resultado:
   - la compensación se calcula y se aplica a los ajustes;
   - en las torres, el valor del mejor segmento se pasa a su ajuste.

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
| **Temperatura** | Torre de segmentos de 8 mm con un voladizo a 45°; de 230 a 190 °C de a 5 por defecto | El segmento con mejor voladizo, menos hilos y buena unión | La temperatura de la boquilla |
| **Retracción** | Dos columnas a 30 mm; el largo de 0,4 a 2,4 mm por defecto | El primer segmento sin hilos entre columnas | `retraction.length` |
| **Flujo** | Torre de 20 × 20 mm; de 90 a 110 % por defecto. Cada segmento deja una cara superior en el cuello | La cara superior lisa, sin huecos ni rebabas | `extrusion.flow` |
| **Velocidad máxima** | Torre de 30 × 30 mm; de 100 a 300 % de las velocidades | Dónde las paredes empiezan a fallar: el límite del fusor | Las velocidades, por debajo de ese segmento |

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
  - `compensation_from`.
- **`compensated` (`src/slice/pipeline.*`):** la escala.
- **`offset_holes` (`src/geom/polygons.*`):** los agujeros.
- **Cambios por altura:** el escritor y el verificador del G-code.
- **`apps/tisma/app.cpp`:** la pestaña.
- **Pruebas:** `tests/test_calibration.cpp`:
  - la pieza tiene su volumen exacto y el laminado su área;
  - la escala y los agujeros mueven lo que deben;
  - la compensación desde medidas;
  - cada torre escribe sus valores, y el verificador la rechaza si no conoce los cambios.
