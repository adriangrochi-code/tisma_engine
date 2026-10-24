# Modo vaso (espiral)

**Qué hace:** imprime la pieza como una sola pared que sube en espiral, sin costura: en cada vuelta la boquilla sube
de a poco la altura de una capa. Sirve para vasos, macetas y pantallas de una pared.

**Fase:** después de F4.

**Fuente del comportamiento:** documentación de usuario de los slicers (base de conocimiento de Prusa) y las mediciones
de abajo.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `vase.enabled` | — | no | Activa el modo vaso |

Las capas sólidas de abajo (`surfaces.bottom_layers`), la falda y el borde siguen valiendo.

## Reglas

1. **Ajustes que fuerza:** con `vase.enabled`, la pieza se lamina con un solo perímetro, sin relleno y sin capas
   sólidas arriba, sin importar lo que digan esos ajustes (prueba VA1).
2. **Lo que no se puede:**
   - soportes: la configuración se rechaza y no se lamina (prueba VA4);
   - más de un objeto, o más de una copia, en la misma impresión: se rechaza, salvo que se impriman uno por uno
     (`sequential.md`) (prueba VA4).
3. **Fondo:** las primeras `surfaces.bottom_layers` capas se imprimen planas, con el perímetro y relleno sólido. Como
   no hay capas de arriba, la última de ellas es la superficie de arriba (`;TYPE:Top solid infill`) (prueba VA1).
4. **Espiral:** desde la capa siguiente, cada capa es una sola vuelta del perímetro externo:
   - empieza donde terminó la vuelta de abajo, sin viaje ni retracción;
   - Z sube en línea recta con el largo recorrido, desde la altura de la capa de abajo hasta la de la capa, y llega al
     cerrar la vuelta;
   - el caudal es el de siempre, también en la primera y en la última vuelta (prueba VA2).
5. **Sin fondo:** con `surfaces.bottom_layers` 0, la primera capa es una vuelta plana y la espiral empieza en la
   segunda (prueba VA3).
6. **Tiempo de capa:** cada vuelta cuenta como una capa para el enfriamiento (`cooling.md`): una vuelta corta se frena
   hasta tardar `cooling.min_layer_time` (prueba VA2).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `walls_extra.md` (boquilla 0,4, capas
de 0,2, sin falda ni borde, centro en (100, 100)) y `vase.enabled` en sí. **Cilindro:** 10 mm de radio y 5 mm de alto.
**Cubo:** 20 × 20 × 10 mm.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| VA1 | Cilindro | 3 perímetros, relleno al 20 %, 3 capas sólidas arriba y abajo | Se lamina con 1 perímetro, relleno 0 y 0 capas arriba. Capas 0 y 1: perímetro externo y relleno sólido (823,8 y 720,5 mm). Capa 2: perímetro y `Top solid infill` (820,5 mm). Desde la capa 3, solo el perímetro externo. 25 capas |
| VA2 | Cilindro de VA1 | — | Capa 3: una vuelta de 61,43 mm de Z 0,6 a 0,8 (`G1 Z0.601`, `Z0.604`… `Z0.800`), 66 tramos. Capa 10: de Z 2,0 a 2,2, empieza y termina en (91,975; 105,568). Última capa (24): de Z 4,8 a 5,0. Caudal de 0,999 mm³/s en toda la vuelta, también en la primera y la última. Velocidad 12,3 mm/s: 61,4 mm en 5 s |
| VA3 | Cubo, sin capas sólidas abajo | — | Capa 0: vuelta plana de 78,34 mm a Z 0,2. Capa 1: de Z 0,2 a 0,4. Con 2 capas sólidas abajo: capa 0 relleno sólido, capa 1 superficie de arriba, espiral desde la capa 2 |
| VA4 | Cilindro; voladizo de `supports_skirt.md` | Dos copias; dos copias impresas una por una; soportes | Dos copias: «Only a single object may be printed at a time in Spiral Vase mode», no lamina. Una por una: lamina, 50 capas (25 por copia). Con soportes: «Spiral vase mode is not compatible with support material», no lamina |
