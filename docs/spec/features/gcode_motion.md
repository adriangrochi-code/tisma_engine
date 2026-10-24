# Extrusión, velocidades y viajes

**Qué hace:** convierte las trayectorias en movimientos de G-code: cuánto filamento empuja cada tramo, a qué
velocidad, y cómo se viaja entre tramos sin dejar hilos.

**Fase:** F4. El formato de las líneas y de los comentarios está en `GCODE_OUTPUT.md`; el enfriamiento, en
`cooling.md`.

**Fuente:** [Manual de Slic3r, «Flow Math»](https://manual.slic3r.org/advanced/flow-math): la sección de un cordón
(documentación de usuario).

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `material.diameter` | mm | 1,75 | Diámetro del filamento |
| `material.flow_factor` | — | 1 | Multiplica todo el filamento empujado |
| `speed.walls` | mm/s | 60 | Perímetros internos |
| `speed.outer_wall` | mm/s o % de `speed.walls` | 50 % | Perímetro externo |
| `speed.small_loops` | mm/s o % de `speed.walls` | 15 mm/s | Perímetros cerrados cortos (regla 3) |
| `speed.infill` | mm/s | 80 | Relleno poco denso |
| `speed.solid` | mm/s o % de `speed.infill` | 20 mm/s | Relleno sólido |
| `speed.top` | mm/s o % de `speed.solid` | 15 mm/s | Capa de arriba de todo |
| `speed.bridges` | mm/s | 60 | Puentes y perímetros sobre el aire |
| `speed.first_layer` | mm/s o % | 30 mm/s | Toda la primera capa |
| `speed.max` | mm/s | 80 | Tope de las velocidades de extrusión |
| `speed.max_flow` | mm³/s | 0 = sin tope | Tope del caudal |
| `speed.travel` | mm/s | 130 | Viajes, también en Z |
| `retraction.length` | mm | 2 | Retracción |
| `retraction.speed` | mm/s | 40 | Velocidad de la retracción y de la vuelta |
| `retraction.min_travel` | mm | 2 | Viaje mínimo que retrae |
| `retraction.lift` | mm | 0 | Salto en Z al viajar |

## Reglas

1. **Filamento por tramo:**
   - sección del cordón: `A = (w − h)·h + π·(h/2)²`, con `w` el ancho y `h` la altura (rectángulo con costados
     redondeados);
   - filamento por mm de tramo: `E = A · multiplicador / (π · (material.diameter / 2)²)`;
   - con `w` = 0,45, `h` = 0,2 y filamento de 1,75: 0,033849 mm por mm de tramo (prueba M1);
   - **puentes:** sección redonda de diámetro igual al ancho del puente (unos 0,40 mm): 0,052735 mm por mm (prueba M2).
2. **Velocidad de cada tipo:** la de su ajuste. En la primera capa todo va a `speed.first_layer` (prueba M1).
3. **Perímetros chicos:** un perímetro cerrado de menos de unos 41 mm de largo va a `speed.small_loops`, también el
   externo. Medido: con 40,6 mm sí, con 42,2 mm no (prueba M4). Es el largo de una circunferencia de 6,5 mm de radio.
4. **Topes:** ninguna extrusión supera `speed.max`; si `speed.max_flow` no es 0, tampoco su caudal
   (`A × velocidad`). Todavía sin prueba: en las de abajo ninguna velocidad llega a los topes.
5. **Viajes:**
   - van a `speed.travel`, en línea recta;
   - si el viaje es más largo que `retraction.min_travel`, se retrae antes de viajar y se vuelve a empujar al llegar,
     las dos cosas a `retraction.speed` (prueba M3).
6. **Fin de un perímetro externo:** al terminarlo, la boquilla sigue sin extruir unos 0,4 mm por el borde de la pieza
   y recién ahí retrae, así la retracción no queda en la costura (prueba M3).
7. **Cambio de capa:**
   1. el comando del ventilador de la capa nueva, si cambia;
   2. `;LAYER_CHANGE`, `;Z:`, `;HEIGHT:`;
   3. la retracción, si el viaje al primer punto la pide;
   4. el movimiento en Z, a `speed.travel`;
   5. el viaje al primer punto y la vuelta del filamento.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, desde la línea de comandos.

**Ajustes base:** los de `surfaces_infill.md` (boquilla 0,4, capas de 0,2, `walls.count` 2, 3 capas sólidas arriba y
abajo, relleno `rectilinear` al 20 %, sin falda ni borde), con E absoluta.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| M1 | Cubo de 20 × 20 × 10 mm | — | Primera capa: todo a 30 mm/s (F1800), 0,029691 mm de filamento por mm (ancho 0,4). Capa 11: externo a 30 mm/s, interno a 60 (F3600), relleno a 80 (F4800), todos a 0,033849 mm/mm. Capa 50 (la de arriba): 15 mm/s (F900), 0,029853 mm/mm (ancho 0,4019) |
| M2 | Cubo de M1 | — | Capa 48 (puente sobre el relleno): 60 mm/s, 0,052735 mm/mm |
| M3 | Cubo de M1 | — | Retracciones de 2 mm a 40 mm/s (F2400), con vuelta igual. Viajes a 130 mm/s (F7800). En las capas 1, 11, 48 y 50: el viaje más largo sin retraer mide 1,26 mm y el más corto con retracción 17,8 mm. Después del perímetro externo, un viaje de 0,40 mm sobre el borde antes de retraer |
| M4 | Prismas cuadrados de 10,2 / 10,6 / 11,0 / 11,4 mm de lado, 3 mm de alto, sin enfriamiento que frene | — | Perímetro externo de 38,9 y 40,5 mm: a 15 mm/s. De 42,1 y 43,7 mm: a 30 mm/s. Los internos, todos más cortos que 41 mm: a 15 mm/s |
