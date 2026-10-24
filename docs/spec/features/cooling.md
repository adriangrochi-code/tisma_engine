# Enfriamiento

**Qué hace:** da tiempo a que cada capa se enfríe. Las capas cortas se imprimen más lento, y el ventilador sopla más
cuanto más corta es la capa.

**Fase:** F4. Es el comportamiento que el dueño acordó para Tisma Slicer (ver «Enfriamiento» en `BEHAVIOR.md`); las
pruebas se midieron con una versión de Tisma Slicer que ya lo tiene.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `cooling.enabled` | — | sí | Activa el frenado y el ventilador automático |
| `cooling.min_layer_time` | s | 5 | Tiempo mínimo de cada capa |
| `cooling.min_speed` | mm/s | 10 | Primer límite del frenado |
| `cooling.fan_below_time` | s | 60 | Por debajo de este tiempo de capa, el ventilador se enciende |
| `cooling.fan_min`, `cooling.fan_max` | % | 35 / 100 | Rango del ventilador |
| `cooling.fan_bridges` | % | 100 | Ventilador durante los puentes |
| `cooling.fan_off_layers` | — | 3 | Capas del principio con el ventilador apagado |
| `cooling.fan_always` | — | no | Si es sí, el ventilador nunca baja de `cooling.fan_min` |

## Reglas

1. **Tiempo de la capa:** el largo de cada movimiento dividido por su velocidad, sumado para toda la capa.
2. **Frenado:** si la capa tarda menos que `cooling.min_layer_time`:
   - se busca la velocidad común `v` más alta tal que, bajando a `v` todas las extrusiones más rápidas que `v`, la
     capa tarde ese tiempo. Las que ya iban más lento no cambian (prueba K2);
   - primero con `v` no menor que `cooling.min_speed` (prueba K2);
   - si no alcanza, `v` baja de `cooling.min_speed` hasta llegar al tiempo, sin otro límite que 1 mm/min (pruebas K3
     y K4);
   - **no se frenan** los puentes ni los perímetros sobre el aire (prueba K5);
   - los viajes y las retracciones no se frenan, pero su tiempo cuenta.
3. **Ventilador**, con `t` el tiempo final de la capa:
   - `t` ≤ `cooling.min_layer_time`: `cooling.fan_max`;
   - `cooling.min_layer_time` < `t` < `cooling.fan_below_time`:
     `cooling.fan_max − (cooling.fan_max − cooling.fan_min) · (t − cooling.min_layer_time) / (cooling.fan_below_time − cooling.min_layer_time)`;
   - `t` ≥ `cooling.fan_below_time`: apagado, o `cooling.fan_min` si `cooling.fan_always`;
   - una capa que se frenó por debajo de `cooling.min_speed` lleva `cooling.fan_max`;
   - las primeras `cooling.fan_off_layers` capas, apagado;
   - durante los puentes, `cooling.fan_bridges`, y al terminar vuelve el valor de la capa.
4. **Cuándo se escribe:** el comando del ventilador de una capa va justo antes de su `;LAYER_CHANGE` (ver
   `gcode_motion.md`).

## Pruebas

Medidas en Tisma Slicer con el enfriamiento acordado (24-10-2026), como caja negra.

**Ajustes base:** los de `gcode_motion.md`. Los tiempos son largo / velocidad, sin aceleraciones.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| K1 | Cubo de 20 × 20 × 10 mm | — | Capas 1 a 3: ventilador apagado. Capa 4 (6,6 s): 98 %. Capa 47 (10,3 s): 93 %. Capa 48, puente sobre el relleno (16,8 s): 86 %, y 100 % durante el puente. Capa 49 (46,7 s): 50 %. Capa 50 (67,9 s): apagado |
| K2 | Prisma de 8 × 8 × 5 mm (sin frenado tarda 9,2 s por capa: perímetros a 15 mm/s por ser chicos, relleno sólido a 20) | `cooling.min_layer_time` 10 | Capa 11: el relleno baja a 17,3 mm/s (F1036); los perímetros siguen a 15 mm/s. Tiempo: 10,0 s |
| K3 | El prisma de K2 | `cooling.min_layer_time` 30 | Todas las extrusiones a 5,4 mm/s (F324), por debajo de `cooling.min_speed`. Tiempo: 30,0 s; ventilador al 100 % |
| K4 | Prisma de 3 × 3 × 5 mm | `cooling.min_layer_time` 20 | Todas las extrusiones a 1,15 mm/s (F69). Tiempo: 20,0 s; ventilador al 100 % |
| K5 | «Mesa» de `surfaces_infill.md`, prueba I6 | `cooling.min_layer_time` 60, y después 300 | Capa 26 (el puente): con 60 s, perímetros y relleno a 4,1 mm/s (F247); con 300 s, a 0,72 mm/s (F43). En los dos casos, el puente y los perímetros sobre el aire siguen a 60 mm/s (F3600) |
