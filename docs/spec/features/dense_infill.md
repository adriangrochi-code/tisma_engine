# Relleno denso bajo las superficies de arriba

**Qué hace:** imprime más denso solo el relleno de la capa que queda justo debajo de una superficie de arriba, y solo
donde esa superficie necesita apoyo. Permite un relleno general mucho más liviano sin que la superficie de arriba se
hunda.

**Fase:** después de F3. La idea viene de SuperSlicer; se describe por su comportamiento.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `infill.dense_under_top` | — | no | Activa el relleno denso |
| `infill.dense_density` | % | 40 % | Densidad de esa capa |

## Reglas

1. La última capa de relleno poco denso bajo las capas sólidas de arriba se imprime con `infill.dense_density`, en la
   zona que queda debajo de la superficie.
2. Las demás capas siguen con `infill.density`.
3. Mismo patrón y dirección que el resto del relleno; solo cambia la separación entre líneas (`surfaces_infill.md`,
   regla 7).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. Ajustes base de `gcode_motion.md`.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| D1 | Cubo de 20 × 20 × 10 mm | `infill.density` 10 %, `infill.dense_under_top` sí | Capas 4 a 46: relleno con separación 4,071 mm (10 %). Capa 47: 1,018 mm (40 %). Capas 48 a 50: las capas sólidas de arriba |
