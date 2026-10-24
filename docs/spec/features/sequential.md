# Impresión secuencial (objeto por objeto)

**Qué hace:** con varios objetos o copias en la cama, termina cada uno entero antes de empezar el siguiente, en vez
de imprimir todos capa por capa. Si uno falla, los otros no se pierden. Hace falta dejar lugar para que el cabezal no
choque con lo ya impreso.

**Fase:** después de F6.

**Fuente del comportamiento:** documentación de usuario de los slicers (base de conocimiento de Prusa) y las mediciones
de abajo.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `sequential.enabled` | — | no | Imprime objeto por objeto |
| `sequential.clearance_radius` | mm | 20 | Radio de un cilindro centrado en la boquilla que encierra el cabezal |
| `sequential.clearance_height` | mm | 20 | Distancia en Z entre la punta de la boquilla y lo primero que puede chocar arriba (las varillas del eje X) |
| `arrange.distance` | mm | 6 | Separación mínima entre objetos al acomodarlos |
| `gcode.between_objects` | texto | vacío | G-code que se escribe entre un objeto y el siguiente (variables en `gcode_extras.md`) |

## Reglas

1. **Orden:** se imprimen todas las capas de un objeto, después todas las del siguiente, y así. El orden es el de los
   objetos en la cama (prueba SQ1).
2. **Acomodo:** al acomodar los objetos, la separación entre sus bordes es la mayor entre `arrange.distance` y
   `sequential.clearance_radius` (prueba SQ2).
3. **Paso al siguiente objeto:** al terminar un objeto:
   - retrae;
   - viaja a la altura de su última capa (ya está por encima de todo lo impreso) hasta el centro del objeto siguiente;
   - escribe las temperaturas de la primera capa sin esperar (`M140` de la cama y `M104` de la boquilla) y después
     `gcode.between_objects`; si ese texto ya tiene un `M104`, `M109`, `M140` o `M190`, no escribe las suyas;
   - apaga el ventilador (el objeto nuevo empieza con su primera capa);
   - en la primera capa del objeto nuevo, viaja a la altura de antes hasta su primer punto y recién ahí baja (prueba
     SQ3).
4. **Altura:** según la documentación, ningún objeto salvo el último puede ser más alto que
   `sequential.clearance_height`, porque el eje X chocaría con él. En la línea de comandos de Tisma Slicer no se
   rechazó (prueba SQ4); el motor debe rechazarlo o avisar.
5. **Modo vaso:** es la única forma de imprimir más de un objeto en modo vaso (`vase_mode.md`).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `walls_extra.md` (boquilla 0,4, capas
de 0,2, 2 perímetros, sin relleno, sin falda ni borde, centro en (100, 100)). **Cubo:** 20 × 20 × 10 mm, copiado.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| SQ1 | 2 y 3 copias del cubo | `sequential.enabled` no / sí | No: 50 capas, las dos copias en cada capa. Sí: 100 capas (150 con 3); capas 0 a 49 la copia en X 90 a 110, 50 a 99 la de X 50 a 70, 100 a 149 la de X 10 a 30 |
| SQ2 | 2 copias | `sequential.clearance_radius` 5 / 20 / 35 | Bordes separados 6 / 20 / 35 mm (la segunda copia en X 64 a 84, 50 a 70, 35 a 55). Sin `sequential.enabled`: 6 mm |
| SQ3 | 2 copias, `sequential.enabled` | `gcode.between_objects` `;BETWEEN [current_object_idx]` / `M109 S[temperature]` (boquilla 205) | Al terminar la primera: `E` de retracción, `G1 Z10`, `G1 X60 Y100`, `M140 S0`, `M104 S200` (las de la primera capa), `;BETWEEN 1`, `M107`; capa 0 de la segunda: `G1 Z10`, viaje, `G1 Z.2`. Con `M109 S[temperature]`: solo `M109 S205`, sin `M140` ni `M104` |
| SQ4 | 2 torres de 20 × 20 × 25 mm | `sequential.enabled`, `sequential.clearance_height` 20 | Se laminan sin aviso |
