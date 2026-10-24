# Calibraciones

**Qué hace:** genera piezas de prueba en las que un ajuste cambia a lo largo de la altura (por franjas) o a lo largo de
X (por parches). La persona imprime la pieza, mira cuál franja o parche salió mejor y aplica ese valor. También hay un
bloque para medir con calibre y corregir las medidas.

**Fase:** después de F4. Las piezas y las reglas las diseñó el equipo del dueño. Los comandos de G-code salen de la
documentación de cada firmware: [Marlin](https://marlinfw.org/meta/gcode/),
[Klipper](https://www.klipper3d.org/G-Codes.html) y
[RepRapFirmware](https://docs.duet3d.com/User_manual/Reference/Gcodes).

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `calibration.mode` | — | apagado | Qué ajuste cambia (tabla «Pruebas por franjas») |
| `calibration.start` | — | 0 | Valor de la primera franja |
| `calibration.step` | — | 0 | Cuánto cambia en cada franja; puede ser negativo |
| `calibration.end` | — | 0 | Último valor; no se pasa de él |
| `calibration.band_height` | mm | 5 | Alto de cada franja |

## Reglas

1. **Franjas:**
   - la franja `i` (desde 0) empieza en Z = `i · band_height`;
   - su valor es `start + i · step`, sin pasar de `end`;
   - el valor se escribe al empezar la primera capa cuya base está en o arriba del comienzo de la franja, con un
     comentario `; calibration`.
2. **Temperatura:**
   - la espera inicial (`M109`) es a la temperatura de la primera franja;
   - el cambio normal de temperatura de la segunda capa no tiene que pisar el valor de la franja. Tisma Slicer escribe
     en la capa 2 primero la temperatura del filamento y enseguida la de la franja; el motor escribe solo la de la
     franja.
3. **Comandos por firmware:**

   | Prueba | Klipper | Marlin 2 | RepRapFirmware |
   |---|---|---|---|
   | Temperatura | `M104 S<°C>` | `M104 S<°C>` | `M104 S<°C>` |
   | Pressure advance | `SET_PRESSURE_ADVANCE ADVANCE=<v>` | `M900 K<v>` | `M572 D0 S<v>` |
   | Esquinas | `SET_VELOCITY_LIMIT SQUARE_CORNER_VELOCITY=<v>` | `M205 X<v> Y<v>` | `M566 X<v·60> Y<v·60>` (mm/min) |
   | Vibraciones (input shaping) | `SET_INPUT_SHAPER SHAPER_FREQ_X=<Hz> SHAPER_FREQ_Y=<Hz>` | `M593 F<Hz>` | `M593 F<Hz>` |
   | Aceleración | `M204 S<v>` | `M204 P<v>` | `M204 P<v>` (sin prueba) |

   La retracción, la velocidad volumétrica máxima y la velocidad de los perímetros no son comandos: el motor usa el
   valor de la franja al generar esas capas.
4. **Pruebas por parches a lo largo de X:** un valor por parche, como ajuste propio de cada parche:
   - planchado: flujo (5 a 25 %), separación (0,05 a 0,25 mm) y velocidad (10 a 50 mm/s). Parches macizos de 20 mm de
     fondo y 2 mm de alto, con la cara de arriba planchada;
   - arcos en voladizos: solape entre arcos (0 a 40 %). Columnas con una losa en voladizo.
5. **Qué pruebas se ofrecen:** solo las que sirven a la impresora elegida.
   - Pressure advance, esquinas y vibraciones: solo con Klipper, RepRapFirmware o Marlin (vibraciones: Marlin 2),
     porque los comandos son de esos firmwares.
   - En las Bambu Lab no se ofrecen ni las vibraciones ni la altura de la boquilla: la impresora las calibra sola
     ([Printer Calibration Guide](https://wiki.bambulab.com/en/general/printer-calibration)).
   - En las Prusa CORE One, MK4/S, MK3.9/S y XL no se ofrece el ajuste de la primera capa: lo calibran solas
     ([Live adjust Z](https://help.prusa3d.com/article/live-adjust-z_112427)).
6. **Bloque de precisión de medidas:**
   - **pieza:** cuatro escalones cuadrados de 80, 50, 30 y 10 mm, de 3 mm de alto cada uno, con agujeros de 4, 6, 8 y
     10 mm;
   - **ajuste:** con las medidas del calibre se ajusta una recta `medido = escala · diseño + desplazamiento`, por
     mínimos cuadrados. La escala es la contracción (crece con el tamaño) y el desplazamiento el borde (igual en
     cualquier tamaño);
   - **se calcula** por X, por Y, para los agujeros (con la escala de las medidas exteriores) y para las alturas,
     teniendo en cuenta los ajustes con los que se imprimió el bloque;
   - **resultado:** contracción del filamento en XY y en Z, `slicing.xy_offset` (medidas exteriores), un
     desplazamiento aparte para los agujeros, y `slicing.first_layer_shrink`;
   - el desplazamiento de las alturas se muestra pero no se aplica;
   - lo que la recta no explica se muestra como ruido: es el límite de precisión de esa impresora y ese calibre.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra: un prisma de 20 × 20 × 25 mm, capas de 0,2, franjas de 5 mm.

| # | Ajustes | Resultado |
|---|---|---|
| CA1 | Temperatura de 230 a 210, paso −5 | `M109 S230` al empezar; `M104 S225` en Z 5,2, `S220` en 10,2, `S215` en 15,2, `S210` en 20,2 |
| CA2 | Pressure advance de 0 a 0,08, paso 0,02, Klipper | `SET_PRESSURE_ADVANCE ADVANCE=0.0000` al empezar; `0.0200` en Z 5,2 … `0.0800` en 20,2 |
| CA3 | Lo mismo con Marlin 2 y con RepRapFirmware | `M900 K0.0200` … y `M572 D0 S0.0200` …, en las mismas alturas |
| CA4 | Retracción de 0,5 a 2,5, paso 0,5 | Las retracciones de cada franja miden 0,5, 1,0, 1,5, 2,0 y 2,5 mm |
| CA5 | Esquinas de 2 a 10, paso 2 | Klipper `SQUARE_CORNER_VELOCITY=2.00`, Marlin 2 `M205 X2.00 Y2.00`, RepRapFirmware `M566 X120 Y120`; en Z 5,2 los dobles |
| CA6 | Vibraciones de 20 a 60 Hz, paso 10 | Klipper `SET_INPUT_SHAPER SHAPER_FREQ_X=20.0 SHAPER_FREQ_Y=20.0`; Marlin 2 y RepRapFirmware `M593 F20.0`; en Z 5,2, 30 Hz |
| CA7 | Aceleración de 1000 a 5000, paso 1000 | Klipper `M204 S1000` … `S5000`; Marlin 2 `M204 P1000` … `P5000`, en Z 0,4, 5,2, 10,2, 15,2 y 20,2 |
