# Qué le falta al motor frente a Tisma Slicer (26-10-2026)

**Pedido del dueño:** revisar qué funciones de Tisma Slicer le faltan al motor, para que no se pierda nada.

**Cómo:** motor de `dev` en `dfcd143`, como caja negra.
- Se sacaron de las fichas los 597 nombres de ajuste que aparecen como `grupo.clave`.
- Se le pasó cada uno al motor con un valor de prueba: si responde «unknown group» o «unknown setting», no lo tiene.
- Se comprobaron a mano los formatos de entrada y algunas funciones clave.

Resultado: el motor reconoce 138 nombres. Los otros 459 incluyen nombres que no son ajustes del laminado (los de la
interfaz, los dispositivos y la nube, y nombres de archivo), que se separan abajo. Que el motor reconozca un ajuste no
dice que pase todas las pruebas de su ficha.

## Lo que el motor ya tiene

Capas (alturas fijas, variables y por zona), corte, paredes de ancho variable, costura (alineada, más cercana, atrás,
al azar y en rampa), superficies, relleno (15 claves: rectilíneo, grilla, giroide, triángulos, cúbico, cúbico
adaptativo, `support_cubic`, `lightning`, estrellas, combinado, ondulado, denso bajo la superficie, por tensiones),
soportes en grilla (7 claves), falda y borde, velocidades base, enfriamiento base, retracción base, material (32
claves), compensación de medidas, Z anti-aliasing, piel difusa, GPU (`compute.gpu`), análisis estructural
(`analyze`), cambios por altura (`changes`) y modo vaso en camino a `dev`.

## Lo que falta, del laminado

| Área | Claves que faltan (ejemplos) | Ficha | Qué se pierde hoy |
|---|---|---|---|
| Resina (SLA) | 44: exposición, capas de base, elevación, antialiasing, botella… | `resin.md`, `resin_supports.md` | Toda la impresión en resina |
| Multimaterial | `wipe_tower.*` (12), `extruders.*` (9), `mmu.*` (5), `object.wipe_into_*` (3), `multi.*` (2) | `multi_extruder.md`, `multimaterial.md`, `color_mixing.md` | Más de un filamento, torre de purga, mezcla de colores (fases MM1 a MM4) |
| Soportes más allá de la grilla | 25: límite de ángulo, interfaz de abajo, solo desde la cama, forzar o bloquear, árbol | `supports_more.md`, `supports_skirt.md` | Soporte en árbol, pintar soporte, interfaces finas |
| Paredes extra | 21: relleno de huecos, voladizos detectados, pared extra donde hace falta, sentido, una pared en la primera capa | `walls_extra.md`, `staggered_perimeters.md` | Huecos entre paredes, paredes más lentas en voladizos |
| No planar de Tisma | `curved.*` (16), `head.*` (5) | `nonplanar.md`, `nonplanar_modes.md` | Las capas curvas propias de Tisma (fase F7) |
| Extras de G-code | 18: arcos, G-code binario, cambio de color, plantillas antes y después de capa, temperatura automática, esquinas suavizadas | `gcode_extras.md`, `gcode_templates.md` | Pausas y cambios de color por capa, arcos `G2`/`G3`, formato binario |
| Velocidades y aceleraciones por tipo | `speed.*` (13), `acceleration.*` (12) | `speeds_accels.md` | Velocidad de puentes y relleno de huecos, voladizos dinámicos, aceleración por tipo |
| Retracción y viajes | `retraction.*` (11), `travel.*` (6) | `retraction_travel.md` | Barrido, salto en rampa, viajes por dentro, retracción del firmware |
| Adherencia | 9: balsa, tipos de borde, separación del borde, pantalla | `skirt_brim_raft.md` | Balsa y protección contra corrientes |
| Enfriamiento extra | 8: ventilador en voladizos, capa con ventilador al máximo | `cooling_more.md` | Ventilador por voladizo |
| Planchado | `ironing.*` (6) | `ironing.md` | Superficies de arriba planchadas |
| Relleno entrelazado de materiales | `interlocking.*` (6) | `interlock_infill.md` | Unión entre dos materiales |
| Arcos en voladizos | `arcs.*` (4) | `arc_overhangs.md` | Voladizos de 90° sin soporte |
| Piezas en la cama | `placement.*` (5), `transform.*` (7), `orient.*` (2), `sequential.*` (4) | `placement.md`, `model_tools.md`, `orientation.md`, `sequential.md` | Centrar y acomodar, girar, escalar, cortar y separar, orientar solo, imprimir de a una pieza |
| Herramientas de Tisma | `fragment.*` (8), `fins.*` (20), `belt.*` (3), `calibration.*` (5), `review.*` (3) | `fragment.md`, `support_fins.md`, `belt_printer.md`, `calibration_models.md`, `print_review.md` | Partir piezas con encastres, aletas de soporte, impresoras de cinta, modelos de calibración, revisión antes de imprimir |
| Otros | `infill.before_walls`, `infill.width`, `surfaces.bridge_angle`, `extrusion.bridge_flow_ratio`, `filament.pressure_advance`, `output.filename_format`, `output.post_process`, `painting.*` | Varias | Detalles de cada función |

## Formatos y piezas

| Prueba | Resultado |
|---|---|
| `tisma-engine slice` con un 3MF (el cráneo de triceratops) | Lo lee como STL y falla («binary STL is truncated») |
| Con un OBJ | Lo lee como STL y falla («file too short to be an STL») |
| Con un STL fuera del centro (`box.stl` de Tisma Slicer) | No lo centra: lamina donde está y falla porque la falda sale de la cama. Tisma Slicer lo centra |

El motor lee solo STL desde la línea de comandos. `formats.md` pide OBJ y 3MF en F1, y STEP en P4.

## Lo que no es del laminado

`device.*`, `ui.*`, `assistant.*`, `packs.*`, `defaults.*` y `thingiverse.*` son de la interfaz, los dispositivos, el
paquete de perfiles y la nube (fases P1, P2, P3 y P5). No se miden con `tisma-engine slice`; se revisan cuando esas
fases tengan una forma de probarlas.

## Orden sugerido para no perder nada

1. Formatos de entrada (3MF y OBJ) y centrar la pieza en la cama: cualquier proyecto de Tisma Slicer los usa.
2. Velocidades y aceleraciones por tipo, relleno de huecos, planchado, balsa y extras de G-code (cambio de color y
   pausas): son de uso diario.
3. Soportes en árbol y pintados, paredes extra y retracción y viajes completos.
4. Multimaterial (MM1 a MM4), no planar de Tisma (F7) y resina, por su tamaño.
5. Las herramientas propias de Tisma (partir piezas, aletas, cinta, calibraciones, revisión).
