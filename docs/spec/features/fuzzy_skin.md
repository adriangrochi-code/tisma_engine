# Piel difusa

**Qué hace:** desplaza al azar los puntos del perímetro externo para que la superficie quede rugosa. Sirve para
agarres y para disimular las marcas de las capas. Se puede activar para toda la pieza o pintar a mano dónde va
(`model_tools.md`).

**Fase:** después de F4.

**Fuente del comportamiento:** documentación de usuario de los slicers y las mediciones de abajo.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `fuzzy.mode` | — | `none` | `none`, `external` (el perímetro externo de los contornos de afuera) o `all` (también el de los agujeros) |
| `fuzzy.thickness` | mm | 0,3 | Cuánto se aparta cada punto, como máximo, de la línea |
| `fuzzy.point_distance` | mm | 0,8 | Distancia media entre los puntos nuevos |

## Reglas

1. **Qué lazos:** solo los perímetros externos. Los perímetros internos no cambian nunca (prueba F2).
2. **Puntos:**
   - el lazo se rehace con puntos separados al azar alrededor de `fuzzy.point_distance`;
   - cada punto se corre al azar hacia afuera o hacia adentro, hasta `fuzzy.thickness`.
3. **Voladizos:** la parte que sale afuera de la capa de abajo cuenta como voladizo, con sus reglas
   (`walls_extra.md`, regla 8).
4. **Pintada:** donde se pintó, se aplica con estos mismos ajustes, aunque `fuzzy.mode` sea `none`.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `walls_extra.md`. Capa 10.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| F1 | Cubo de 20 mm | `external`, 0,3 y 0,8 | Perímetro externo de 4 a 97 puntos; distancia al centro de 9,493 a 10,075 mm (la línea va a 9,775: ±0,3); tramos de 0,85 mm en promedio, 1,8 como máximo. El perímetro interno sigue con 4 puntos a 9,368 |
| F1b | Cubo de 20 mm | `external`, 1 y 2 | 37 puntos; de 8,79 a 10,2 mm, y lo que sale más allá de la capa de abajo pasa a `Overhang perimeter` (hasta 10,72); tramos de 2,19 mm en promedio |
| F2 | Tubo de 10 mm de radio con agujero de 5 | `external` / `all` | `external`: el perímetro externo del agujero queda liso (radio mínimo 5,204). `all`: el agujero también (radio mínimo 4,942). En los dos, los perímetros internos no cambian (radios de 5,61 a 9,368) |
