# Paredes: opciones adicionales

**Qué hace:** las opciones de las paredes que no están en `perimeters.md`:
- elegir entre cordones de ancho variable o de ancho fijo;
- rellenar o no los huecos entre paredes;
- costuras internas escalonadas y costura en bufanda;
- perímetros extra en voladizos y donde hacen falta;
- espesor de la cáscara vertical;
- detección de los perímetros en voladizo;
- un solo perímetro arriba o en la primera capa.

**Fase:** después de F2 (las de voladizos, después de F3).

**Fuente del comportamiento:** documentación de usuario de los slicers (base de conocimiento de Prusa, wiki de
OrcaSlicer) y las mediciones de abajo.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `walls.generator` | — | `variable` | `variable`: cordones de ancho variable (`perimeters.md`). `fixed`: todos los cordones del mismo ancho, y lo que sobra entre paredes se rellena con «relleno de huecos» |
| `walls.gap_fill` | — | sí | Rellenar los huecos entre paredes y entre las paredes y el relleno |
| `walls.staggered_inner_seams` | — | no | Las costuras de los perímetros internos se corren hacia atrás según su profundidad |
| `walls.scarf` | — | `nowhere` | Costura en bufanda: `nowhere`, `contours` (solo los contornos de afuera) o `everywhere` (también los agujeros) |
| `walls.scarf_only_smooth` | — | sí | Solo en los lazos lisos, sin esquinas marcadas |
| `walls.scarf_start_height` | % de la capa | 0 % | Altura a la que empieza la rampa, sobre la capa de abajo |
| `walls.scarf_length` | mm | 20 | Largo de la rampa |
| `walls.scarf_entire_loop` | — | no | La rampa da la vuelta entera |
| `walls.scarf_max_segment` | mm | 1 | Largo máximo de cada tramo de la rampa |
| `walls.scarf_inner` | — | no | También en los perímetros internos |
| `walls.extra_if_needed` | — | sí | Agrega perímetros donde la pared inclinada deja huecos (regla 6) |
| `walls.extra_on_overhangs` | — | no | Llena con perímetros los voladizos que un puente no puede anclar |
| `walls.vertical_shell` | — | `enabled` | Asegurar el espesor de la cáscara vertical: `disabled`, `partial` o `enabled` |
| `walls.detect_overhangs` | — | sí | Los tramos de perímetro sobre el aire se imprimen como voladizo |
| `walls.avoid_curled_overhangs` | — | no | Los viajes esquivan los voladizos que se pueden levantar |
| `walls.top_one_wall` | — | `none` | Un solo perímetro en las superficies de arriba: `none`, `top` (todas) o `topmost` (solo la de más arriba) |
| `walls.first_layer_one_wall` | — | no | Un solo perímetro en la primera capa |

## Reglas

1. **Ancho fijo (`walls.generator` = `fixed`):**
   - todos los perímetros del ancho de `perimeters.md`;
   - donde no entra otro perímetro entero, el hueco del medio se rellena con un cordón de «relleno de huecos»
     (`;TYPE:Gap fill`), si `walls.gap_fill` está activo (prueba W6);
   - el relleno concéntrico también usa este ancho fijo.
2. **Sin relleno de huecos:** con `walls.gap_fill` apagado, esos huecos quedan vacíos (prueba W6).
3. **Costuras internas escalonadas:**
   - la costura del perímetro externo queda donde dice `walls.seam`;
   - cada perímetro interno empieza antes, sobre su propio lazo: unos 0,42 mm por cada nivel de profundidad (un ancho
     de cordón). Así las costuras forman un zigzag hacia adentro en vez de una línea (prueba W5).
4. **Costura en bufanda:** el lazo empieza con una rampa en vez de un escalón:
   - la rampa empieza `walls.scarf_length` antes de la costura, a la altura de la capa de abajo más
     `walls.scarf_start_height` de la capa, y sube hasta la altura de la capa en la costura;
   - desde la costura sigue el lazo entero a la altura de la capa. Al final pasa de nuevo sobre la rampa;
   - el filamento total del lazo es el de un lazo normal: la rampa empieza con poco caudal y lo sube, y el final del
     lazo, que pasa sobre la rampa, lo baja;
   - con `walls.scarf_entire_loop`, la rampa ocupa la vuelta entera;
   - la rampa se parte en tramos de no más de `walls.scarf_max_segment`;
   - con `walls.scarf_only_smooth`, los lazos con esquinas marcadas (un cubo) no la llevan;
   - `contours` la pone solo en el perímetro externo de los contornos de afuera; `everywhere`, también en el de los
     agujeros. Los internos solo con `walls.scarf_inner`.
5. **Perímetros extra en voladizos:** donde una capa sobresale de la de abajo y un puente no puede anclarse de los dos
   lados, la zona se llena con perímetros concéntricos anclados a la parte apoyada, marcados `;TYPE:Overhang perimeter`
   (prueba W3).
6. **Perímetros extra si hacen falta:** según la documentación, se agregan perímetros en las paredes inclinadas hasta
   que más del 70 % del lazo de arriba quede apoyado. En las piezas de prueba (techo de 5°, cono, cuñas, con y sin
   relleno, con ancho fijo y variable) Tisma Slicer no agregó ninguno: el motor puede dejarlo para después.
7. **Espesor de la cáscara vertical:**
   - `disabled`: solo las capas sólidas de arriba y abajo;
   - `partial`: además, relleno sólido debajo de las superficies de arriba inclinadas donde las capas sólidas no
     llegan al espesor pedido. Nada bajo los voladizos, ni donde las paredes ya cubren ese espesor (prueba W4);
   - `enabled`: lo mismo también bajo los voladizos y donde las paredes no alcanzan (prueba W4).
8. **Detección de los perímetros en voladizo:** la parte de un perímetro que queda sobre el aire:
   - se marca `;TYPE:Overhang perimeter`;
   - va a la velocidad de los puentes;
   - lleva el cordón redondo de los puentes, con su filamento por mm (`gcode_motion.md`);
   - lleva el ventilador al máximo mientras dura.

   Apagada, el perímetro se imprime entero como cualquier otro (prueba W2).
9. **Esquivar voladizos que se levantan:** según la documentación, los viajes evitan las zonas de voladizos
   redondeados y empinados donde el filamento puede haberse curvado hacia arriba, para no chocar con la boquilla. No
   se midió.
10. **Un solo perímetro arriba:**
    - `top`: en cada superficie de arriba, sobre la parte expuesta queda solo el perímetro externo; los internos siguen
      solo alrededor de lo que continúa hacia arriba, y la superficie gana ese lugar;
    - `topmost`: lo mismo, solo en la superficie de más arriba de la pieza (prueba W7).
11. **Un solo perímetro en la primera capa:** la primera capa lleva solo el perímetro externo (prueba W8).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. **Ajustes base:** boquilla 0,4, capas de 0,2 mm (también la
primera), 2 perímetros, sin relleno, sin capas sólidas, sin falda ni borde, pieza centrada en (100, 100).

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| W1 | Cilindro de 10 mm de radio y 5 de alto | `walls.scarf` `everywhere` | Capa 10 (Z 2,2): el perímetro externo mide 80,85 mm = lazo de 61,31 + rampa de 20; empieza a Z 2,0 y termina en la costura de siempre, (99,944; 90,245). Filamento 2,0814 mm, contra 2,0754 sin bufanda. Con `walls.scarf_start_height` 50 %, empieza a Z 2,1. Con `walls.scarf_length` 10, mide 71,08. Con `walls.scarf_entire_loop`, 122,69 (dos vueltas). Con `walls.scarf_inner`, el perímetro interno también: 77,47 mm desde Z 2,0 |
| W1b | Cubo de 20 mm | `walls.scarf` `everywhere` | Con `walls.scarf_only_smooth`, sin rampa (Z 2,2 fijo). Sin él, rampa en tramos de 1 mm: 97,60 mm en 44 movimientos |
| W1c | Tubo de 10 mm de radio con agujero de 5 | `walls.scarf` `contours` o `everywhere` | `contours`: rampa solo en el contorno de afuera; el perímetro externo del agujero queda a Z 2,2 (32,72 mm). `everywhere`: también el del agujero (53,59 mm desde Z 2,0) |
| W2 | Columna de 10 × 20 × 10 mm con una losa de 20 × 20 × 2 mm arriba (Z 8 a 10) que sobresale 10 mm en +X | `walls.detect_overhangs` sí / no | Capa 40 (Z 8,2), primera de la losa. Sí: los tramos sobre el aire, `Overhang perimeter`, de 37,07 y 38,70 mm, a 60 mm/s (la velocidad de los puentes), 0,05224 mm de filamento por mm, ventilador al 100 % durante esos tramos. No: perímetros normales enteros, sin ese tipo |
| W3 | La pieza de W2 | `walls.extra_on_overhangs` no / sí | Capa 40. No: 2 tramos de `Overhang perimeter` (75,8 mm). Sí: 4 tramos (513,2 mm): la zona de 10 × 20 mm que sobresale queda llena de perímetros |
| W4 | Techo de 15°: prisma de 20 × 20 mm, de 5 mm de alto en un lado y 10,36 en el otro; cono de 10 mm de radio y 10 de alto; hongo: tallo de 10 mm de alto y sombrero que se abre a 55° de la vertical hasta Z 18,4 | 3 capas sólidas arriba y abajo; `walls.vertical_shell` | Techo, capa 30: `disabled` 20,95 mm de superficie de arriba; `partial` y `enabled` 75,22 de superficie de arriba más 18,39 de relleno sólido. Hongo, capa 60: solo `enabled` agrega relleno sólido (124,4 mm). Cono, capa 20: solo `enabled` (30,68 mm) |
| W5 | Cilindro de W1 | 4 perímetros, costura `aligned`, `walls.staggered_inner_seams` | No: las 4 costuras en X ≈ 100 (99,97 a 100,00). Sí: el externo en X 100,004 y los internos en 99,566, 99,149 y 98,731 (0,42 mm más por nivel) |
| W6 | Pared recta de 1,5 × 20 × 5 mm | `walls.generator` `fixed` | Capa 10: un perímetro externo de 41,14 mm y un relleno de huecos de 19,14 mm. Con `walls.gap_fill` apagado, solo el perímetro |
| W7 | Escalón: bloque de 30 × 20 × 4 mm con otro de 10 × 20 × 4 encima (Z 4 a 8) | 3 perímetros, 3 capas sólidas; `walls.top_one_wall` | `none`: capa 19 (Z 4,0) con 186,51 mm de perímetros internos; capa 39 (Z 8,0) con 106,51. `top`: capa 19 con 106,42 mm de internos (solo alrededor de lo que sigue subiendo) y la superficie de arriba de 986 a 1090 mm; capa 39 sin internos. `topmost`: capa 19 igual que `none`, capa 39 sin internos |
| W8 | Cubo de 20 mm | 3 perímetros, `walls.first_layer_one_wall` | Capa 0: solo el perímetro externo (78,34 mm); capa 1: los 3 |
