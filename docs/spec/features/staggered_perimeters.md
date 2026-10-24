# Perímetros escalonados

**Qué hace:** imprime uno de cada dos perímetros internos media capa más arriba. Así las juntas entre capas de
perímetros vecinos no quedan alineadas y la pared se traba como una de ladrillos: más resistencia entre capas.

**Fase:** después de F2.

## Patente

La técnica parecida llamada «brick layers» choca con la patente estadounidense US11331848B2 («3D printing bead
configuration», vigente hasta 2040; ver [Google Patents](https://patents.google.com/patent/US11331848B2/en)). Su
reivindicación 1 pide, todo junto:
- capas horizontales de cordones con un cordón terminal en cada extremo;
- un cordón terminal perpendicular a los demás y de la mitad de su ancho;
- los terminales alternando de extremo entre capas.

Este diseño evita esos elementos:
- solo mueve **lazos cerrados**, que no tienen extremos;
- todos los cordones tienen el mismo ancho y siguen el contorno;
- el desfase es en Z, no dentro del plano de la capa;
- el perímetro externo no se mueve.

Antes de usarlo en una versión comercial lo confirma un abogado de patentes.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `stagger_perimeters` | — | no | Activa el escalonado |

## Reglas

1. Se suben los perímetros internos impares (el 1.º interno, el 3.º, …), media capa (`h/2`), solo donde la capa
   siguiente también tiene pieza encima. El externo nunca se mueve.
2. **Primera capa:** el perímetro subido va de la cama a media capa más arriba, así que lleva 1,5 veces el material.
3. **Última capa bajo una superficie de arriba:** el perímetro subido vuelve a la Z de la capa con la mitad del
   material, para que nada sobresalga.
4. Con altura de capa variable, el material sale de la altura real entre el cordón de abajo y el de arriba.
5. No se usa con modo espiral, capas no planares ni impresoras de cinta.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. Ajustes base de `gcode_motion.md` con `perimeters` 3.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| ST1 | Cubo de 20 × 20 × 10 mm | `stagger_perimeters` sí | Capa 1 (Z 0,2): el 1.er perímetro interno a Z 0,3 con 0,04454 mm/mm (1,5 × 0,02969). Capas 2 a 49: el 1.er interno a Z de la capa + 0,1, con el material normal (0,03385 mm/mm); el externo y el 2.º interno a la Z de la capa. Capa 50 (Z 10,0): el 1.er interno a Z 10,0 con 0,01692 mm/mm (la mitad) |
