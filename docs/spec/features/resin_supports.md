# Resina: soportes, base (pad) y vaciado

**Qué hace:**
- **soportes:** sostienen la pieza separada de la plataforma, con puntas finas que tocan la pieza en los puntos que
  cuelgan;
- **base (pad):** una placa bajo los soportes que se pega a la plataforma;
- **vaciado:** deja la pieza hueca, con una pared de espesor mínimo, para gastar menos resina y despegar con menos
  fuerza. Necesita agujeros de drenaje (`resin.md`, ventosas).

**Fase:** después de la primera versión, con `resin.md`.
- En Tisma Slicer estas tres funciones vienen de PrusaSlicer: no las escribió el equipo del dueño.
- El motor diseña sus propios algoritmos. Para los soportes en árbol, Vanek et al. 2014 (`SOURCES.md`).
- De Tisma Slicer se toma solo lo que la persona ve y ajusta, y lo medido como caja negra.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `resin.supports` | — | sí | Generar soportes |
| `resin.support_tip_diameter` | mm | 0,4 | Diámetro de la punta que toca la pieza |
| `resin.support_tip_penetration` | mm | 0,2 (0,4 en los perfiles de Elegoo) | Cuánto entra la punta en la pieza |
| `resin.support_pillar_diameter` | mm | 1 | Diámetro de los pilares |
| `resin.support_elevation` | mm | 5 | Cuánto levantan los soportes a la pieza sobre la base |
| `resin.support_critical_angle` | ° | 45 | Ángulo de las uniones entre soportes |
| `resin.support_density` | % | 100 | Densidad relativa de los puntos de apoyo |
| `resin.pad` | — | sí | Base bajo los soportes |
| `resin.pad_thickness` | mm | 2 (1 en los perfiles de Elegoo) | Espesor de la base |
| `resin.pad_margin` | mm | 1,6 | Cuánto se extiende la base alrededor de lo que sostiene |
| `resin.pad_wall_slope` | ° | 90 (45 en los perfiles de Elegoo) | Inclinación del borde de la base; 90 es vertical |
| `resin.pad_around_object` | — | no | Base alrededor de la pieza apoyada, sin elevarla |
| `resin.hollow` | — | no | Vaciar la pieza |
| `resin.hollow_wall` | mm | 3 | Espesor mínimo de la pared |
| `resin.hollow_closing` | mm | 2 | Cuánto se redondea el hueco: no sigue los detalles chicos de la pieza |

Los valores por defecto son los de Tisma Slicer.

## Reglas

1. **Altura de la pieza:**
   - con soportes y base, la pieza empieza en `Z = resin.pad_thickness + resin.support_elevation` (prueba RS1);
   - con `resin.pad_around_object`, la pieza se apoya en la base y no se eleva.
2. **Base:**
   - ocupa las primeras `resin.pad_thickness` mm y se extiende más allá de lo que sostiene;
   - su borde baja con la inclinación `resin.pad_wall_slope`;
   - sin soportes y sin `resin.pad_around_object`, la base no tiene qué sostener: se avisa que no se puede generar
     (prueba RS3).
3. **Soportes:**
   - puntos de apoyo en las caras que miran hacia abajo, con la densidad de `resin.support_density`;
   - cada punto lleva una punta de `resin.support_tip_diameter` que entra `resin.support_tip_penetration` en la pieza;
   - las puntas se unen en pilares de `resin.support_pillar_diameter`, con uniones a `resin.support_critical_angle`, y
     los pilares bajan a la base, con un ensanche al pie;
   - en Tisma Slicer, los pilares de un cubo elevado ocupan unos 20 mm² por capa a mitad de altura (prueba RS1).
4. **Vaciado:**
   - el hueco queda a `resin.hollow_wall` o más de la superficie, también arriba y abajo;
   - se redondea en `resin.hollow_closing`, así que la pared es más gruesa en las esquinas;
   - el hueco cerrado es una ventosa hasta que tiene agujeros de drenaje (`resin.md`, reglas 13 a 15). Se avisa.
5. **Agujeros de drenaje:**
   - la persona los pone en la pieza con su diámetro y su profundidad;
   - atraviesan la pared hasta el hueco;
   - Prusa recomienda al menos dos ([anuncio de PrusaSlicer 2.2](https://blog.prusa3d.com/prusaslicer-2-2-0-is-out-hollowing-support-for-3rd-party-printers-and-more_32728/)):
     uno deja salir la resina y el otro entrar el aire.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `resin.md` y el lector del archivo
GOO. Cubo de 20 mm en el centro de la pantalla.

| # | Ajustes | Resultado |
|---|---|---|
| RS1 | Soportes y base; elevación 5 mm, base de 2 mm | 340 capas (17 mm). Capas de Z 0,05 a 2,0: base de 680 mm², de 27,6 mm de ancho (3,8 mm más que el cubo por lado). De Z 2,0 a 2,8 la base se angosta (310 mm²); de 2,8 a 3,0, el ensanche al pie de los pilares; de 3,0 a 7,0, pilares (unos 20 mm² por capa en Z 5). El cubo empieza en Z 7,05 (400 mm²). Volumen 5569 mm³ |
| RS2 | Vaciado con pared de 2 mm | Volumen 2656 mm³ (macizo: 4000). Macizo hasta Z 1,55; hueco desde Z 3,05; macizo otra vez desde Z 9,55. Pared de 2,52 mm en Z 3,05, de 2,0 mm en Z 5,05 y de 3,12 mm en Z 7,55: el hueco tiene los bordes redondeados |
| RS3 | Solo base, sin soportes ni base alrededor de la pieza | Error: «no se puede generar la base con esta configuración», sin archivo |
| RS4 | Vaciado con pared de 3 mm | Tisma Slicer deja el cubo macizo (volumen 4000 mm³), aunque cabe un hueco de 14 mm. No se copia: el motor vacía la pieza si cabe un hueco |
