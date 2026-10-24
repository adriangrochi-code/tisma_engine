# Arcos en voladizos

**Qué hace:** imprime un voladizo sin soporte con arcos concéntricos que crecen desde el borde apoyado. Cada arco
apoya en el anterior, así que el voladizo se sostiene solo, aunque sea plano.

**Fase:** después de F5. Es la idea conocida como «arc overhang» (Steven McCulloch, 2023), descrita en sus
publicaciones; su código no se usa.

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

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. Ajustes base de `gcode_motion.md`, sin soportes.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| A1 | Columna de 10 × 20 × 10 mm con una tabla de 20 × 20 × 2 mm arriba (de Z 8 a 10) que sobresale 10 mm en +X (la de `supports_skirt.md`, S4) | `arcs.enabled` sí | Capa 41 (Z 8,2): los 10 × 20 mm que sobresalen se imprimen con arcos, sin puente. Arcos a 5 mm/s (F300); 0,052245 mm de filamento por mm; radios de los arcos enteros 0,686, 1,407, 2,128 y 2,849 mm (de a dos arcos, 0,721 mm; uno, 0,36). Largo total 606 mm; 62 retracciones en la capa; ventilador al 100 % durante los arcos. Capa 42: relleno sólido sobre la zona |
