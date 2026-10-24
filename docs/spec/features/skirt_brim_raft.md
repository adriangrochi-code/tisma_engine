# Falda, escudo, tipos de borde y balsa

**Qué hace:** lo de la adherencia que no está en `supports_skirt.md`:
- un largo mínimo de falda, medido en filamento;
- el escudo contra corrientes: la falda sube con la pieza;
- bordes afuera, adentro de los agujeros o en los dos lados, y separados de la pieza;
- la balsa: unas capas de soporte debajo de toda la pieza, que la levantan de la cama.

**Fase:** F5 (la balsa usa el soporte de F5).

**Fuente del comportamiento:** documentación de usuario de los slicers (base de conocimiento de Prusa) y las mediciones
de abajo.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `adhesion.skirt_min_filament` | mm de filamento | 0 | Agrega vueltas de falda hasta gastar al menos este largo de filamento en la primera capa |
| `adhesion.draft_shield` | — | `disabled` | `disabled`: la falda solo en `adhesion.skirt_layers` capas. `limited`: escudo de `adhesion.skirt_layers` capas. `enabled`: escudo tan alto como la pieza más alta |
| `adhesion.brim_type` | — | `outer` | `none`, `outer` (afuera de la pieza), `inner` (adentro de los agujeros) o `outer_and_inner` |
| `adhesion.brim_gap` | mm | 0 | Separación entre la pieza y la primera vuelta del borde |
| `adhesion.raft_layers` | capas | 0 = sin balsa | Capas de la balsa |
| `adhesion.raft_gap` | mm | 0,1 | Separación en Z entre la balsa y la pieza |
| `adhesion.raft_expansion` | mm | 1,5 | Cuánto se agranda la balsa alrededor de la pieza |
| `adhesion.raft_first_layer_density` | % | 90 % | Densidad de la primera capa de la balsa |
| `adhesion.raft_first_layer_expansion` | mm | 3 | Cuánto más se agranda la primera capa de la balsa |
| `speed.first_layer_over_raft` | mm/s o % | 30 | Tope de todas las extrusiones de la primera capa de la pieza sobre la balsa; en %, de cada velocidad |

## Reglas

### Falda

1. **Largo mínimo:** si las vueltas de `adhesion.skirt_loops` gastan menos filamento que `adhesion.skirt_min_filament`,
   se agregan vueltas hacia afuera, a la separación de los cordones, hasta pasarlo (prueba B1).
2. **Con borde:** sin escudo, la falda rodea el borde: `adhesion.skirt_distance` se mide desde la vuelta externa del
   borde (prueba B2).
3. **Con balsa:** la falda rodea la primera capa de la balsa, a `adhesion.skirt_distance` de ella (prueba R9).

### Escudo contra corrientes

4. **Qué es:** una falda que sube con la pieza y la protege de las corrientes de aire (ABS, ASA). Con escudo:
   - la falda se mide desde la pieza, aunque haya borde;
   - la vuelta del borde que caería donde va la falda se quita, para no imprimir dos cordones en el mismo lugar;
   - `enabled`: la falda sigue en todas las capas, hasta la última de la pieza más alta;
   - `limited`: solo en las primeras `adhesion.skirt_layers` capas (prueba B3).

### Tipos de borde

5. **`outer`:** vueltas alrededor del contorno de afuera de la pieza, hacia afuera, hasta `adhesion.brim_width`.
6. **`inner`:** vueltas dentro de cada agujero de la primera capa, desde su borde hacia adentro, hasta
   `adhesion.brim_width` o hasta llenar el agujero.
7. **`outer_and_inner`:** las dos cosas (prueba B4).
8. **Separación:** con `adhesion.brim_gap`, el borde entero se aparta ese tanto de la pieza; su ancho sigue siendo
   `adhesion.brim_width` (prueba B4).
9. **Con balsa no hay borde:** la balsa ya agranda la primera capa (`adhesion.raft_first_layer_expansion`) (prueba R8).

### Balsa

10. **Capas:** con `adhesion.raft_layers` = N, la pieza sube N capas y debajo se imprime la balsa:
    - las de abajo son base de soporte (`;TYPE:Support material`): líneas a `support.spacing` como en el soporte
      (`supports_skirt.md`), la de más arriba a `support.line_angle` y las de debajo giradas 90° en capas alternas;
    - las de arriba, hasta `support.interface_layers` (3 por defecto), son interfaz
      (`;TYPE:Support material interface`): densas, a 45° y 135° en capas alternas, la de más arriba a 135°;
    - si N no pasa de `support.interface_layers`, todas son de interfaz; con N = 1, la única capa es de interfaz y va en
      la primera capa (prueba R2).
11. **Alturas:** la primera capa de la balsa usa la altura de la primera capa; las demás, la altura del soporte (0,3 mm
    con boquilla de 0,4 en Tisma Slicer), sin importar la altura de las capas de la pieza (prueba R3).
12. **La pieza sobre la balsa:** su primera capa se imprime a `adhesion.raft_gap` sobre la balsa, con la altura de una
    capa normal. Con `adhesion.raft_gap` 0, va pegada. Sus extrusiones no pasan de `speed.first_layer_over_raft`; la
    capa siguiente vuelve a sus velocidades (pruebas R1 y R7). Es una capa como las demás: lleva relleno sólido solo si
    le toca por las capas sólidas de abajo.
13. **Forma:** la balsa sigue la huella de la primera capa de la pieza (redonda bajo un cilindro), agrandada
    `adhesion.raft_expansion`. En Tisma Slicer el borde de la balsa se ajusta hacia afuera a la grilla de las líneas de
    la base (`support.spacing` más un cordón: 2,857 mm), así que crece a saltos; el motor puede usar el valor exacto
    (prueba R4).
14. **Primera capa de la balsa:**
    - se agranda además `adhesion.raft_first_layer_expansion` (prueba R5);
    - sus líneas van a `adhesion.raft_first_layer_density`: el largo es proporcional a la densidad (prueba R6).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `walls_extra.md` (boquilla 0,4, capas de
0,2, 2 perímetros, sin relleno, sin capas sólidas, sin falda ni borde, centro en (100, 100)). **Cubo:** 20 × 20 × 10 mm.
**Tubo:** 10 mm de radio con un agujero de 5, 5 mm de alto.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| B1 | Cubo | 1 vuelta de falda a 6 mm; `adhesion.skirt_min_filament` 0 / 300 | 0: una vuelta de 118,5 mm. 300: 57 vueltas, 10337,7 mm de recorrido en la primera capa |
| B2 | Cubo | `adhesion.brim_width` 5, falda de 1 vuelta a 3 mm, sin escudo | Borde de 14 vueltas (la externa de 110,3 mm) y la falda afuera del borde: 132,9 mm |
| B3 | Cubo | Lo de B2 con `adhesion.draft_shield` `enabled` | Falda de 99,68 mm a 3 mm de la pieza, en todas las capas hasta la última (capa 49). El borde pierde la vuelta de 99,1 mm que caía ahí: 13 vueltas. Con `limited` y `adhesion.skirt_layers` 5: falda solo en las capas 0 a 4 |
| B4 | Tubo | `adhesion.brim_width` 3 | `outer`: 8 vueltas afuera, ejes de radio 10,159 a 12,678. `inner`: 8 vueltas en el agujero, de radio 4,801 a 2,319. `outer_and_inner`: las dos. `outer` con `adhesion.brim_gap` 1: la primera vuelta a 11,168 y la externa a 13,673 |
| R1 | Cubo, relleno al 20 %, 3 capas sólidas arriba y abajo | `adhesion.raft_layers` 3 | Capa 0 (Z 0,2, alto 0,2): base de 3039,9 mm en 2 tramos, líneas a 0°, hasta 7,26 mm más allá del cubo. Capas 1 y 2 (Z 0,5 y 0,8, alto 0,3): interfaz de 2506,9 mm a 45° y de 2343,3 mm a 135°, hasta unos 5 mm más allá del cubo. La pieza empieza en Z 1,1 (0,8 + 0,1 + 0,2), con alto de extrusión 0,2. 53 capas en total. Con `adhesion.raft_gap` 0, Z 1,0; con 0,2, Z 1,2 |
| R2 | Cubo | `adhesion.raft_layers` 5 / 1 | 5: capas 0 y 1 de base (a 90° y a 0°; la 1 de 464,5 mm, a 2,857 mm entre líneas), capas 2 a 4 de interfaz (135°, 45°, 135°); la pieza empieza en Z 1,7. 1: una sola capa de interfaz de 2343,3 mm en Z 0,2; la pieza empieza en Z 0,5 |
| R3 | Cubo, `adhesion.raft_layers` 3 | Capas de 0,1 / 0,3 | 0,1: balsa en Z 0,1, 0,4 y 0,7; la pieza en Z 0,9. 0,3: balsa en Z 0,3, 0,6 y 0,9; la pieza en Z 1,3 |
| R4 | Cubo, `adhesion.raft_layers` 3 | `adhesion.raft_expansion` 0 a 10 | Borde de la interfaz más allá del cubo: 2,09 mm con 0, 0,5 y 1; 4,95 con 1,5, 2, 3 y 4; 7,80 con 6; 10,66 con 10. Bajo un cilindro de 10 mm de radio, la interfaz llega a un radio de 14,7 a 15,2 en todas las direcciones |
| R5 | Cubo, `adhesion.raft_layers` 3 | `adhesion.raft_first_layer_expansion` 0 / 3 / 6 | La base de la capa 0 llega a 4,76, 7,26 y 10,26 mm más allá del cubo |
| R6 | Cubo, `adhesion.raft_layers` 3 | `adhesion.raft_first_layer_density` 90 % / 50 % | Capa 0: 3039,9 mm / 1756,3 mm, a 0° en los dos casos |
| R7 | Pieza de R1 | `speed.first_layer_over_raft` 30 / 15 | Capa 3 (la primera de la pieza): todos los perímetros y el relleno sólido a 30 mm/s / 15 mm/s. Capa 4: perímetros internos a 60 mm/s, externo a 30, relleno sólido a 20, en los dos casos |
| R8 | Cubo, `adhesion.raft_layers` 3 | `adhesion.brim_width` 5 | Sin borde: la capa 0 es solo la base de la balsa |
| R9 | Cubo, `adhesion.raft_layers` 3 | Falda de 1 vuelta a 6 mm | La falda rodea la base de la balsa: su eje queda 6,18 mm afuera de la base (de 76,56 a 123,44 en X e Y; la base de 82,74 a 117,26) |
