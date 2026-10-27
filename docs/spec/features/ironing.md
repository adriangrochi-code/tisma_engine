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
2. Las líneas van girando 45° desde las del relleno de arriba de esa capa: relleno a 135°, planchado a 0°; relleno a
   45°, planchado a 90° (IR1, IR2).
3. Material por mm de línea: `ironing.spacing × ironing.flow × altura de la capa` (mm³), convertido a filamento como
   en `gcode_motion.md`.
4. Cubre toda la superficie de arriba, también sobre los perímetros. A lo largo de las líneas, el eje para a 0,205 mm
   del borde; a lo ancho, la primera línea queda a 0,35 mm y la última a 0,25 con `ironing.spacing` 0,1 (0,5 y 0,3
   con 0,2). Junto a una pared que sube (un escalón), empieza a 0,15 mm de ella (IR3). Un agujero queda sin planchar
   (IR7).
5. Se marca `;TYPE:Ironing`.
6. **Qué superficies** (`ironing.surfaces`):
   - `top`: cada superficie de arriba, también la de un escalón más bajo (IR3);
   - `topmost`: solo la más alta de cada pieza (IR4);
   - `solid`: además, las capas sólidas de abajo y parte de las sólidas que quedan debajo de una superficie de arriba
     (IR5).
7. **Primera capa:** si se plancha la primera capa (con `solid`), va a la velocidad de la primera capa (30 mm/s en
   IR5), no a `ironing.speed`.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. Ajustes base de `gcode_motion.md`.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| IR1 | Cubo de 20 × 20 × 10 mm | `ironing.enabled` sí | Solo en la capa 50, a Z 10,0, después del relleno de arriba (que va a 135°): líneas a 0°, separación 0,1 mm, 15 mm/s, 0,001247 mm de filamento por mm (0,003 mm³/mm). Ejes de las líneas entre X 90,205 y 109,795 y entre Y 90,35 y 109,75 (la pieza va de 90 a 110). Largo total 3822 mm |
| IR2 | Cubo de 20 × 20 × 9,8 mm | `ironing.enabled` sí | Capa de Z 9,8: relleno de arriba a 45°, planchado a 90°; ejes entre Y 90,205 y 109,795 y entre X 90,35 y 109,75; 3822 mm (medido el 27-10-2026) |
| IR3 | Escalón: 20 × 20 × 5 mm y encima 10 × 20 × 5 mm en X 90 a 100 | `ironing.surfaces` `top` | Dos planchados: Z 5,0, de X 100,15 a 109,75 (1901 mm, a 90°); y Z 10,0, de X 90,205 a 99,795 (1872 mm, a 0°) |
| IR4 | El escalón de IR3 | `topmost` | Solo Z 10,0 (1872 mm) |
| IR5 | El escalón de IR3 | `solid` | Planchado en Z 0,2, 0,4 y 0,6 (3822 mm cada una; la de Z 0,2 a 30 mm/s), en Z 4,4 (128 mm), 4,8 (1775 mm), 5,0, 9,4 (163 mm), 9,8 (1862 mm) y 10,0 |
| IR6 | Cubo de IR1 | `ironing.flow` 30 %, `ironing.spacing` 0,2, `ironing.speed` 25 | 0,004989 mm de filamento por mm (4 veces IR1: el doble de material y el doble de separación), 25 mm/s, 1901 mm; a lo ancho, de Y 90,5 a 109,7 |
| IR7 | Marco de 20 × 20 × 6 mm con un agujero de 8 × 8 mm en el medio | `ironing.enabled` sí | 3116 mm en Z 6,0; nada sobre el agujero |
