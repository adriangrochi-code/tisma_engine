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

## Reglas

1. Vale para las zonas de una capa que no apoyan en nada y que no son puentes: no tienen apoyo del otro lado.
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

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. Ajustes base de `gcode_motion.md`, sin soportes.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| A1 | Columna de 10 × 20 × 10 mm con una tabla de 20 × 20 × 2 mm arriba (de Z 8 a 10) que sobresale 10 mm en +X (la de `supports_skirt.md`, S4) | `arcs.enabled` sí | Capa 41 (Z 8,2): los 10 × 20 mm que sobresalen se imprimen con arcos, sin puente. Arcos a 5 mm/s (F300); 0,052245 mm de filamento por mm; radios de los arcos enteros 0,686, 1,407, 2,128 y 2,849 mm (de a dos arcos, 0,721 mm; uno, 0,36). Largo total 606 mm; 62 retracciones en la capa; ventilador al 100 % durante los arcos. Capa 42: relleno sólido sobre la zona |
| A2 | Columna de A1 con una losa en L: la de A1 más 10 × 10 mm en +Y sobre la columna (X 90 a 100, Y 110 a 120) | `arcs.enabled` sí | Un solo grupo con centro en (98,8; 103,8): 83 arcos de radio 0,35 a 15,4 mm que llenan los dos brazos (medido el 27-10-2026) |
| A3 | A1 con un agujero de 4 × 4 mm en la losa (X 103 a 107, Y 98 a 102) | `arcs.enabled` sí | El mismo centro que A1; el agujero lleva sus perímetros de voladizo y corta los arcos en pedazos. 557,4 mm de arcos contra 606,1 |
| A4 | Columna de A1 con una tira de 15 × 3 mm que sale en +X (Y 98,5 a 101,5) | `arcs.enabled` sí | Un solo grupo con centro cerca de (99; 100); los arcos, recortados por los lados, cruzan la tira casi rectos; 124,8 mm |
| A5 | A1 | `arcs.enabled` sí y `support.enabled` sí | Soporte de Z 0,2 a 6,7 e interfaz de Z 7,0 a 7,8 debajo; la capa de Z 8,2 sale igual que en A1, con arcos |
