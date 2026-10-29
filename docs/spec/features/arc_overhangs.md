# Arcos en voladizos

**Qué hace:** imprime un voladizo sin soporte con arcos concéntricos que crecen desde el borde apoyado. Cada arco
apoya en el anterior, así que el voladizo se sostiene solo, aunque sea plano.

**Basado en:** Arc Overhang, de Steven McCulloch (GPL-3.0), `https://github.com/stmcculloch/arc-overhang`. Se toma
la idea, descrita en sus publicaciones de 2023; su código no se usa.

**Fase:** después de F5.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `arcs.enabled` | — | no | Activa los arcos |
| `arcs.overlap` | % del ancho | 10 % | Cuánto pisa cada arco al anterior |
| `arcs.speed` | mm/s | 5 | Velocidad de los arcos; 0 = la de los puentes |
| `arcs.max_distance` | mm | 0 = sin límite | Hasta qué distancia del apoyo crecen los arcos; más allá, el voladizo se imprime como siempre |
| `arcs.long_bridges` | — | sí | Con los arcos encendidos, un puente más largo que `arcs.bridge_length` se acorta con arcos desde sus dos apoyos (regla 12) |
| `arcs.bridge_length` | mm | 10 (a calibrar) | Largo de puente que se deja sin arcos en el medio |
| `arcs.layout` | lista | `groups` | `single`: un solo grupo por zona, como Tisma Slicer (regla 7). `groups`: varios grupos chicos (regla 13) |
| `arcs.max_radius` | mm | 8 (a calibrar) | Radio máximo de un arco en `groups` |
| `arcs.nonplanar` | — | no | Arcos que suben a lo largo de varias capas (regla 14); necesita la impresora configurada para no planar |

## Reglas

1. Vale para las zonas de una capa que no apoyan en nada y que no son puentes: no tienen apoyo del otro lado. Los
   puentes largos tienen su propia regla (12).
2. **Forma:**
   - los arcos son círculos concéntricos, recortados por el borde de la zona;
   - crecen desde el borde apoyado, cada uno más grande que el anterior;
   - la separación entre arcos seguidos es `ancho × (1 − arcs.overlap)`: 0,36 mm con 0,4 de ancho y 10 %.
3. **Cordón:** sección redonda, como los puentes (`gcode_motion.md`).
4. **Impresión:**
   - a `arcs.speed`;
   - con el ventilador al máximo (`cooling.fan_bridges`);
   - con retracción entre arcos.
5. Se marcan `;TYPE:Overhang perimeter`.
6. La capa siguiente cubre la zona con relleno sólido.
7. **Centro:** cada zona tiene un solo grupo de arcos, con un solo centro: un punto 1,2 mm adentro de lo apoyado,
   frente al medio del borde apoyado (en A1, (98,8; 100,0) con el borde en X 100 de Y 90 a 110). En una zona en L, el
   borde apoyado sigue la L y el centro queda cerca de su punto medio (A2). No se abren grupos nuevos.
8. **Hasta dónde:** los arcos crecen hasta cubrir el punto más lejano de la zona; los de radio mayor que la distancia
   al borde opuesto salen recortados en pedazos, y así llenan las esquinas.
9. **Agujeros y tiras:** un agujero en la zona lleva sus perímetros de voladizo y corta los arcos que lo cruzan en
   pedazos; no tiene arcos propios (A3). Una tira angosta se llena con el mismo grupo: los arcos recortados por los
   lados de la tira quedan casi rectos, a lo ancho (A4).
10. **Sentido:** los arcos van en vaivén: uno en un sentido y el siguiente en el otro. Uno de cada dos sale en dos
    pedazos, partidos en la recta que pasa por el centro y es perpendicular al borde apoyado (A1).
11. **Con soportes:** con `support.enabled` y `arcs.enabled` los dos en sí, se generan los dos: el soporte y su
    interfaz debajo, y la primera capa del voladizo con arcos igual que sin soporte (A5).

## Mejoras propias (pedido del dueño, 28-10-2026)

No salen de Tisma Slicer ni de Arc Overhang: son diseño nuestro. Los valores «a calibrar» se fijan con impresiones
reales y con la calibración de arcos (`calibration_models.md`).

12. **Puentes largos:** un puente es lo agarrado por dos lados (`supports_skirt.md`, regla 3). Si mide más que
    `arcs.bridge_length` y los arcos están encendidos, crecen arcos desde cada apoyo hacia el medio, como en un
    voladizo, hasta que el tramo libre que queda entre los dos frentes mide `arcs.bridge_length`. Ese tramo se imprime
    como puente, de frente de arcos a frente de arcos. Un puente más corto se imprime como siempre.
13. **Varios grupos chicos (`groups`):** en un voladizo largo (por ejemplo, un rectángulo de 60 × 10 mm apoyado por su
    lado largo), un solo grupo da arcos de radio enorme que cuelgan. En cambio:
    - el borde apoyado se divide en tramos iguales, y cada tramo lleva su grupo, con el centro como en la regla 7;
    - la cantidad de tramos es la menor que deja todos los radios en `arcs.max_radius` o menos;
    - los grupos vecinos se pisan `arcs.overlap` donde se encuentran, para que no quede una costura sin unir;
    - en la capa siguiente, si vuelve a haber arcos, los centros se corren medio tramo, para que las uniones no
      queden una sobre otra.
14. **Arcos no planares (`arcs.nonplanar`):** cada arco sube de a poco y ocupa varias alturas de capa: se apoya en el
    anterior y en la capa de abajo a la vez, y queda trabado entre capas, más fuerte. Solo se usa si la impresora
    tiene cargados su ángulo libre y su altura de cabezal (página No planar de la impresora, `settings_panel.md`, y
    `nonplanar.md`), y cada movimiento pasa la comprobación de choques del cabezal. Es una línea de investigación:
    primero se prueba en una pieza de calibración, y la regla se completa con lo medido.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. Ajustes base de `gcode_motion.md`, sin soportes.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| A1 | Columna de 10 × 20 × 10 mm con una tabla de 20 × 20 × 2 mm arriba (de Z 8 a 10) que sobresale 10 mm en +X (la de `supports_skirt.md`, S4) | `arcs.enabled` sí | Capa 41 (Z 8,2): los 10 × 20 mm que sobresalen se imprimen con arcos, sin puente. Arcos a 5 mm/s (F300); 0,052245 mm de filamento por mm; radios de los arcos enteros 0,686, 1,407, 2,128 y 2,849 mm (de a dos arcos, 0,721 mm; uno, 0,36). Largo total 606 mm; 62 retracciones en la capa; ventilador al 100 % durante los arcos. Capa 42: relleno sólido sobre la zona |
| A2 | Columna de A1 con una losa en L: la de A1 más 10 × 10 mm en +Y sobre la columna (X 90 a 100, Y 110 a 120) | `arcs.enabled` sí | Un solo grupo con centro en (98,8; 103,8): 83 arcos de radio 0,35 a 15,4 mm que llenan los dos brazos (medido el 27-10-2026) |
| A3 | A1 con un agujero de 4 × 4 mm en la losa (X 103 a 107, Y 98 a 102) | `arcs.enabled` sí | El mismo centro que A1; el agujero lleva sus perímetros de voladizo y corta los arcos en pedazos. 557,4 mm de arcos contra 606,1 |
| A4 | Columna de A1 con una tira de 15 × 3 mm que sale en +X (Y 98,5 a 101,5) | `arcs.enabled` sí | Un solo grupo con centro cerca de (99; 100); los arcos, recortados por los lados, cruzan la tira casi rectos; 124,8 mm |
| A5 | A1 | `arcs.enabled` sí y `support.enabled` sí | Soporte de Z 0,2 a 6,7 e interfaz de Z 7,0 a 7,8 debajo; la capa de Z 8,2 sale igual que en A1, con arcos |
| A6 | Dos pilares separados 40 mm con una tabla encima (puente de 40 mm) | `arcs.enabled` sí, `arcs.bridge_length` 10 | Arcos desde los dos pilares; entre los dos frentes quedan 10 mm impresos como puente. Con `arcs.long_bridges` no: puente de 40 mm como siempre |
| A7 | Rectángulo de 60 × 10 mm que sobresale 10 mm de una pared de 60 mm | `arcs.layout` `groups`, `arcs.max_radius` 8 | Varios grupos a lo largo del borde, ningún radio mayor que 8 mm, uniones pisadas; con `single`, un solo grupo como en Tisma Slicer |
| A8 | A7 con dos capas de arcos | `groups` | Los centros de la segunda capa corridos medio tramo respecto de la primera |
