# Planchado

**Qué hace:** después de la capa de arriba, la boquilla caliente vuelve a pasar sobre ella con muy poco material, en
líneas muy juntas, para alisarla.

**Fase:** después de F3.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `ironing.enabled` | — | no | Activa el planchado |
| `ironing.surfaces` | — | `top` | `top`: todas las superficies de arriba; `topmost`: solo la más alta de cada pieza; `solid`: todas las capas sólidas |
| `ironing.flow` | % | 15 % | Material por mm, en % de una capa de la altura normal |
| `ironing.spacing` | mm | 0,1 | Separación entre líneas |
| `ironing.speed` | mm/s | 15 | Velocidad |

## Reglas

1. Se plancha en la misma capa y a la misma Z, después de su relleno de arriba.
2. Las líneas van a 45° de las del relleno de arriba de esa capa.
3. Material por mm de línea: `ironing.spacing × ironing.flow × altura de la capa` (mm³), convertido a filamento como
   en `gcode_motion.md`.
4. Cubre toda la superficie de arriba, también sobre los perímetros, hasta unos 0,2 a 0,35 mm del borde.
5. Se marca `;TYPE:Ironing`.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. Ajustes base de `gcode_motion.md`.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| IR1 | Cubo de 20 × 20 × 10 mm | `ironing.enabled` sí | Solo en la capa 50, a Z 10,0, después del relleno de arriba (que va a 135°): líneas a 0°, separación 0,1 mm, 15 mm/s, 0,001247 mm de filamento por mm (0,003 mm³/mm). Ejes de las líneas entre X 90,205 y 109,795 y entre Y 90,35 y 109,75 (la pieza va de 90 a 110). Largo total 3822 mm |
