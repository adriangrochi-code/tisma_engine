# Coasting

**Qué hace:** deja de extruir un poco antes del final de cada trazo y recorre el resto con la boquilla en movimiento.
La presión que queda en la boquilla termina la línea. Reduce las gotas en la costura y los hilos.

**Fase:** después de F4. Es la idea del «coasting» de Simplify3D, descrita en su documentación de usuario.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `retraction.coast` | mm | 0 = apagado | Largo del final de cada trazo que se recorre sin extruir |

## Reglas

1. Los últimos `retraction.coast` mm de cada trazo se recorren sin extruir, a la velocidad del trazo (`G1` con X e Y,
   sin E).
2. Un trazo de menos de 3 × `retraction.coast` no cambia.
3. Vale para perímetros, perímetro externo y relleno; después viene lo de siempre (retracción si el viaje la pide).
4. El filamento total baja en lo que no se extruye en esos tramos.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. Ajustes base de `gcode_motion.md`.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| CO1 | Cubo de 20 × 20 × 10 mm | `retraction.coast` 0,5 | Capa 11: tres tramos sin extruir al final de trazos: 0,501 mm a 60 mm/s (perímetro), 0,500 mm a 30 mm/s (externo) y 0,509 mm a 80 mm/s (relleno). Sin coasting no hay ninguno |
