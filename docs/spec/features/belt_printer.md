# Impresoras de cinta

**Qué hace:** lamina para impresoras que imprimen sobre una cinta con el pórtico inclinado (Z infinito). Ejemplos:
BlackBelt, Creality CR-30, IdeaFormer IR3 V2.
- La pieza se apoya sobre la cinta como sobre una cama.
- Se lamina en planos paralelos al pórtico.
- El G-code usa los ejes de la máquina: X a lo ancho de la cinta, el eje de la cinta (avanza entre capas) y el eje
  del pórtico.

**Fase:** después de la primera versión. Lo diseñó y escribió el equipo del dueño en Tisma Slicer.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `belt.enabled` | — | no | La impresora es de cinta |
| `belt.angle` | ° | 45 | Ángulo entre el pórtico (el plano de impresión) y la cinta |
| `belt.belt_is_z` | — | sí | Sí: la cinta es el eje Z del firmware y el pórtico el Y. No: la cinta es Y y el pórtico Z |

## Reglas

Con α = `belt.angle`, `h` la altura de capa, y la pieza apoyada en la cinta (`x` a lo ancho, `y` a lo largo de la
cinta, `z` altura sobre la cinta):

1. **Capas:** planos paralelos al pórtico, separados `h` en la dirección perpendicular a ellos. La cantidad de capas
   es el largo de la pieza en esa dirección dividido por `h`: `(largo sobre la cinta + alto · cot α) · sin α / h`
   para un prisma.
2. **Ejes de la máquina:**
   - X = `x`;
   - posición de la cinta = `s / sin α`, donde `s` es la coordenada de laminado de la capa (la cinta avanza
     `h / sin α` por capa);
   - posición en el pórtico = `z / sin α` (distancia desde la cinta medida a lo largo del pórtico inclinado);
   - con `belt.belt_is_z`, la cinta va en Z y el pórtico en Y; si no, al revés.
3. **`;Z:` de cada capa:** lleva la coordenada de laminado `s` (0,2, 0,4, …), no la posición de la cinta.
4. **No disponibles en la cinta:** soportes, balsa, borde, falda, torre de purga, impresión secuencial, modo espiral y
   arcos `G2`/`G3`.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. Ajustes base de `gcode_motion.md`.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| B1 | Cubo de 20 × 20 × 10 mm (10 de alto sobre la cinta) | `belt.enabled`, α 45°, cinta en Z | 106 capas = (20 + 10) · sin 45° / 0,2. Capa 11 (`;Z:` 2,2): Z 3,111 = 2,2 / sin 45°, Y de 0,325 a 4,075. Capa 53 (`;Z:` 10,6): Z 14,991. Y máximo de toda la pieza: 14,129, cerca de 10 / sin 45° = 14,14. X de 90,1 a 109,9, como en una cama |
| B2 | El cubo de B1 | α 45°, cinta en Y | Lo mismo con Y y Z intercambiados: capa 11 en Y 3,111, Z de 0,325 a 4,075 |
| B3 | El cubo de B1 | α 30°, cinta en Z | 93 capas = (20 + 10 · cot 30°) · sin 30° / 0,2. Capa 11 (`;Z:` 2,2): Z 4,4 = 2,2 / sin 30°. Y máximo 19,988, cerca de 10 / sin 30° = 20 |
