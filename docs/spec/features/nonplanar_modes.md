# Modos no planares según la pieza

**Qué hace:** decide dónde y cuánto se curvan las capas mirando la pieza, en lugar de una forma fija (`nonplanar.md`):
- **cónico** (`conical` con `curved.fit_head`): un cono alrededor del centro de la pieza, con la pendiente que piden
  sus voladizos en cada radio;
- **voladizos** (`overhangs`): capas planas que solo bajan bajo los voladizos que llevarían soporte;
- **caras superiores** (`surface`): las capas de arriba quedan paralelas a las caras de arriba inclinadas, que se ven
  como una sola capa y no como escalones;
- **automático** (`automatic`): voladizos y caras superiores a la vez;
- **tensiones** (`stress`): las capas se inclinan hacia donde tira la carga, según el análisis estructural.

**Fase:** F7. Lo diseñó y escribió el equipo del dueño en Tisma Slicer. Vale todo lo de `nonplanar.md`: deformar,
laminar plano y volver; material según el espesor local `J`; E relativa; capas planas bajo `curved.flat_below`;
límites de la cabeza.

**Fuentes:**
- CurviSlicer (Etienne et al., SIGGRAPH 2019, en `SOURCES.md`): capas apenas curvas para impresoras de 3 ejes, con
  límites de pendiente y espesor, para quitar los escalones de las caras de arriba.
- S³-Slicer (Zhang et al., SIGGRAPH Asia 2022, en `SOURCES.md`): capas que siguen las tensiones principales.
- Felzenszwalb y Huttenlocher 2012 (en `SOURCES.md`): distancia exacta en una grilla, una forma de medir la distancia
  de la regla 12.
- La pendiente del cono que cambia con el radio es la idea de Joshua Bird ya citada en `nonplanar.md`. Acá la pendiente
  sale de la pieza.

## Ajustes

Los de la cabeza (`head.*`), `curved.flat_below`, `curved.max_slope` y `curved.fit_head` están en `nonplanar.md`.

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `curved.mode` | — | `disabled` | `conical`, `overhangs`, `surface`, `automatic`, `stress`; los demás modos en `nonplanar.md` |
| `curved.overhang_threshold` | ° desde la vertical | 45 | El voladizo más inclinado que sale bien con capas planas. Lo usan `conical`, `overhangs` y `automatic` |

Las capas de `stress` no son un ajuste: las calcula el análisis estructural y se guardan con el objeto en el proyecto.

## Definiciones

- `β`: inclinación de una cara que mira hacia abajo, desde la vertical (0° es una pared; 90°, un techo).
- `u`: `curved.overhang_threshold`.
- **Pendiente para llevar un voladizo a α.** Si las capas bajan con pendiente `s` hacia donde sale el voladizo, el
  voladizo queda, respecto de ellas, con `cot β' = cot β + s`. La pendiente que lo deja en α es `s(α) = cot α − cot β`.
  Con `u` = 45°:

  | β | `s(40°)` | `s(43°)` |
  |---|---|---|
  | 48° | 0,291 | 0,172 |
  | 52° | 0,410 | 0,291 |
  | 55° | 0,492 | 0,372 |
  | 60° | 0,614 | 0,495 |

- **Límites de la cabeza:**
  - `t_libre`: tangente del menor ángulo libre medido (`head.free_angle_x`, `head.free_angle_y`), o de
    `curved.max_slope` si no se midieron;
  - `t_holgura` = `head.clearance_height` / `head.clearance_radius` (0,2 con 3 mm a 15 mm).
- **Sin choque:** dentro de `head.clearance_radius` alrededor de la boquilla, lo ya impreso no sube más de
  `head.clearance_height` por encima de ella.

## Reglas

### Voladizos que cuentan (`conical`, `overhangs`, `automatic`)

1. Las caras que miran hacia abajo con `β` mayor que `u` y menor que 80°, por encima de `curved.flat_below`. Las más
   planas son techos: quedan para los puentes y los soportes.
2. **Pendiente objetivo:** `s(u − 5°)`. Si la cabeza no deja tanto, alcanza con llegar a `s(u − 2°)`. Si ni eso entra,
   el voladizo queda para los soportes.

### Cónico (`conical` con `curved.fit_head`)

3. `r` es la distancia en XY al centro de la caja de la pieza, y `R` el mayor `r` de la pieza.
4. **Pedidos:** cada voladizo de la regla 1 pide su pendiente objetivo en todos los radios que cubre. Cuenta solo la
   parte radial de su inclinación. El tope es `t_libre`, y se aplica la regla 2.
   - Si el voladizo mira hacia afuera (un hongo), las capas bajan hacia afuera.
   - Si mira hacia adentro (el borde de un cuenco dado vuelta), suben hacia afuera (prueba CO7).
5. **Perfil:**
   - pendiente 0 en el centro;
   - antes del primer radio con pedidos, la pendiente crece de forma pareja de 0 a la pedida, a lo largo de
     max(`R`/4, 5 mm) (prueba CO5);
   - nunca más empinada que `t_libre`.
6. **Altura:** la deformación crece con la rampa `S` de `nonplanar.md`, desde `curved.flat_below` hasta la Z del
   voladizo más bajo que pide (prueba CO6).
7. **Límites:** toda la deformación se achica por un mismo factor hasta que `J` quede en [0,6; 1,4] y no haya choque
   (prueba CO2: con 3 mm a 15 mm, la pendiente baja a 0,21).
8. **Sin efecto:**
   - una pieza sin pedidos se lamina plana (prueba CO1);
   - si, ya achicado, el cono no saca ningún voladizo del soporte, el motor lamina la pieza plana. Tisma Slicer la
     curva igual, y en la prueba CO7 el soporte crece: eso no se copia.

### Voladizos (`overhangs`)

9. **Zonas:** los voladizos cercanos forman una zona. Su base es la sección de la pieza donde empieza el voladizo, lo
   ya impreso justo debajo.
10. **Pendiente de la zona:**
    - la objetivo de la regla 2;
    - como máximo, el 90 % del límite de la cabeza `min(t_libre, t_holgura)`. Tisma Slicer deja ese margen: 0,18 con
      un límite de 0,2 y 0,42 con 0,466 (pruebas VO2 y VO4);
    - si con ese máximo el voladizo no llega a `u − 2°`, la zona queda plana.
11. **Forma:**
    - dentro de la base, las capas siguen planas;
    - afuera bajan `s · d`, con `d` la distancia horizontal a la base, hasta el punto más lejano del voladizo;
    - con ángulos libres distintos en X y en Y, `d` se mide de modo que en ninguna dirección la pendiente pase el
      ángulo libre de esa dirección;
    - el borde de la base se suaviza en alrededor de 1 mm (prueba VO5).
12. **Transición:** la zona entra de a poco desde `curved.flat_below` hasta el comienzo del voladizo, con `J` en
    [0,6; 1,4]. Si no hay altura, la zona se inclina menos. Si así no llega a `u − 2°`, queda plana (prueba VO6).
13. **Arriba del voladizo:** las capas siguen inclinadas hasta el final de la pieza. Una cara plana de arriba dentro
    de la zona se arma con los extremos de las capas inclinadas: queda a la altura del modelo, pero sus líneas siguen
    las capas curvas (prueba VO7).

### Caras superiores (`surface`)

14. **Pedidos:**
    - cada cara que mira hacia arriba pide que, en una piel de 3 mm debajo de ella, las capas tengan su misma
      pendiente, con el tope de `t_libre`;
    - el pedido pesa más cuanto más área tiene la cara y cuanto más horizontal es: en las casi planas es donde más se
      ven los escalones.
15. **Deformación:**
    - una sola deformación suave para toda la pieza, ajustada a los pedidos por mínimos cuadrados;
    - plana bajo `curved.flat_below`;
    - la transición ocupa toda la altura, hasta las caras: la pendiente de las capas crece de forma pareja con la
      altura (prueba SU3).
16. **Límites:** la deformación se achica lo necesario para que `J` quede en [0,6; 1,4], la pendiente no pase
    `t_libre` y no haya choque.
17. **Piezas bajas:** una pieza baja no tiene altura para inclinar las capas sin salir de [0,6; 1,4], y mejora poco
    (prueba SU4).
18. **Primeras capas:** las capas bajo `curved.flat_below` quedan planas. Tisma Slicer inclina también la primera capa
    en este modo (prueba SU5): eso no se copia.

### Automático (`automatic`)

19. La deformación es la suma de la de voladizos y la de caras superiores.
20. Si la suma se pasa de algún límite (espesor, pendiente o choque), se achica solo la parte de las caras superiores:
    la de voladizos ya cumple sola. Donde un voladizo y una cara de arriba están muy cerca, manda el voladizo.

### Tensiones (`stress`)

21. **Dónde se calculan:** en el análisis estructural (`structural_analysis.md`), con la pieza, sus apoyos y sus
    cargas. Las capas se guardan con el objeto, relativas a la esquina mínima de su caja: la pieza se puede mover en
    la cama.
22. **Cálculo:**
    - análisis con capas planas;
    - en cada vóxel, la tensión principal mayor, si es de tracción, y su dirección. La capa debería subir a lo largo de
      la parte horizontal del tirón, con pendiente `vertical / horizontal`, limitada por los ángulos libres en X y en Y;
    - los tirones casi verticales no piden nada: inclinar la capa 20 a 30° casi no los ayuda;
    - una deformación suave en una grilla 3D, por mínimos cuadrados: los pedidos pesan según la tracción y según lo
      horizontal del tirón; poco cambio a lo largo de Z; plana en las primeras capas;
    - análisis de nuevo con las capas tal como se van a laminar (rampa, `J` en [0,6; 1,4], cabeza). Se muestran los
      dos factores de seguridad, con capas planas y curvas.
23. **Al laminar:**
    - la deformación se achica, igual que en las reglas 7 y 16, hasta cumplir `J` y la cabeza;
    - si la pieza no tiene capas calculadas, o cambió de tamaño o de orientación desde el cálculo, es un error que pide
      calcularlas de nuevo (prueba TE1).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, salvo TE2 y TE3, que son resultados de su análisis estructural.

**Ajustes base:**
- los de `surfaces_infill.md`: boquilla 0,4; capas de 0,2; `walls.count` 2; relleno al 20 %; 3 capas sólidas
  arriba y abajo;
- E relativa y `support.enabled` sí;
- **cabezal por defecto:** ángulos libres sin medir (`curved.max_slope` 20°), 3 mm de altura libre a 15 mm;
- **cabezal de 25°:** `head.free_angle_x` y `head.free_angle_y` 25, `head.clearance_height` 7,
  `head.clearance_radius` 15.

**Piezas** (centradas en X = 100, Y = 100; las redondas, de 128 lados):
- **hongo alto de β:** pie de radio 8 y 25 mm de alto. Encima, un sombrero cuya cara de abajo se abre a β de la
  vertical hasta radio 20, con un borde vertical de 2 mm y tapa plana;
- **hongo bajo de β:** lo mismo con un pie de 10 mm;
- **hongo con cúpula:** el hongo alto de 52° con la tapa en cono, 3 mm más alta en el centro que en el borde;
- **cuenco dado vuelta:** anillo de radio 20 a 24 mm y 14 mm de alto, cerrado arriba (Z 12 a 14). La cara de adentro
  cuelga hacia el centro a 52° de la vertical, de radio 20 (Z 2,6) a radio 8 (Z 12);
- **bloque con techo:** 40 × 40 mm, con la cara de arriba subiendo a lo largo de X de 20 a 24 mm (5,7°). El bajo, de
  5 a 9 mm.

**Medidas:**
- **soporte:** filamento (mm) de las líneas de soporte y de interfaz;
- **pendiente:** recta o plano por mínimos cuadrados sobre los puntos extruidos de la capa;
- **error de la cara de arriba:** en celdas de 1 × 1 mm, la Z extruida más alta menos la altura del modelo, sin los
  2 mm del borde. Se da el desvío estándar y el intervalo del 95 %.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| CO1 | Bloque de 40 × 40 × 12 mm | `conical`, `overhangs`, `surface` o `automatic` | 60 capas planas, igual que sin capas curvas |
| CO2 | Hongos altos de 48°, 52°, 55° y 60° | `conical`, cabezal por defecto | 48° y 52°: capas curvas, sin soporte; bajo el sombrero, pendiente 0,21 (la holgura achica el cono). 55° y 60°: planos, con el soporte de VO1 |
| CO3 | Hongos altos | `conical`, ángulos libres de 25° y 3 mm de altura libre | 48°, 52° y 55°: curvas, sin soporte, pendiente 0,21 a 0,22. 60°: plano |
| CO4 | Hongos altos | `conical`, cabezal de 25° | Pendiente entre r = 9 y 19: 0,291 (48°), 0,410 (52°) y 0,4665 (55°, tan 25°). Sin soporte. 60°: plano |
| CO5 | Hongo alto de 52° | Como CO4 | Capa 181 (Z 36,2 en el centro): plana hasta r = 3. Pendiente por mm de 0,06, 0,15, 0,25, 0,35 y 0,39 entre r = 3,5 y 8,5. Después, 0,41 ± 0,02 |
| CO6 | Hongo alto de 52° | Como CO4 | Caída en r = 7 respecto del centro: 0,027 mm en Z 4; 0,135 en 8; 0,276 en 12; 0,427 en 16; 0,554 en 20; 0,618 en 23; 0,637 en 25. Es 0,63 · S((Z − 0,6) / 24,4) |
| CO7 | Cuenco dado vuelta | `conical`, cabezal de 25° | Las capas suben hacia afuera, con pendiente 0,047: el voladizo empieza en Z 2,6 y no hay altura. El soporte crece de 718 a 1052 mm |
| VO1 | Hongos altos | Plano | Soporte: ninguno con 48° (42° desde la horizontal, sobre el umbral automático de 41,6°); 3065 mm con 52°; 3690 con 55°; 3504 con 60° |
| VO2 | Hongos altos | `overhangs`, cabezal por defecto | 48°: pendiente 0,18 bajo el sombrero. 52°, 55° y 60°: planos, con el soporte de VO1 |
| VO3 | Hongos altos | `overhangs`, ángulos libres de 25° y 3 mm de altura libre | Igual que VO2 (48°: 0,18): manda la holgura (3 / 15) |
| VO4 | Hongos altos | `overhangs`, cabezal de 25° | Pendiente: 0,29 (48°), 0,41 (52°) y 0,42 (55°). Sin soporte. 60°: plano, 3504 mm |
| VO5 | Hongo alto de 55° | Como VO4 | Hasta Z 10, capas planas. Capa 161 (Z 32,2): Z = 32,200 hasta r = 7; afuera, una recta de pendiente 0,417 entre r = 8,5 y 14 (residuo máximo 0,05 mm) |
| VO6 | Hongos bajos de 48° y 52° | `overhangs`, cabezal de 25° | 48°: pendiente 0,26 (pide 0,29, pero el pie de 10 mm no deja más), sin soporte. 52°: plano, 1714 mm de soporte, igual que en plano |
| VO7 | Hongo alto de 55° | Como VO4 | La tapa (Z 35,40) sale a +0,01 mm del modelo (de −0,01 a +0,05). Las capas 194 a 200 llegan a ella inclinadas, de Z 33,6 a 35,4 |
| SU1 | Bloque con techo de 20 a 24 mm | Plano | Error de la cara de arriba: 0,057 mm (de −0,083 a +0,037) |
| SU2 | El mismo | `surface` | 0,010 mm (de −0,048 a −0,014). Solo con Z anti-aliasing (`z_antialiasing.md`), con capas planas: 0,039 |
| SU3 | El mismo | `surface` | Pendiente de la capa a lo largo de X (a lo largo de Y, 0): 0,010 en Z 2; 0,048 en 10; 0,074 en 15; 0,090 en 18; 0,097 en 20; 0,099 en las de arriba. 110 capas (120 en plano) |
| SU4 | Bloque con techo de 5 a 9 mm | `surface` | Pendiente arriba 0,087. Error 0,056 mm (plano: 0,057). Con Z anti-aliasing, 0,034 |
| SU5 | El de SU4 | `surface` | La primera capa de Tisma Slicer va de Z 0,135 a 0,282. El motor la deja plana |
| AU1 | Hongos altos | `automatic`, con los dos cabezales | Igual que `overhangs` (VO2 y VO4) |
| AU2 | Bloques con techo | `automatic` | Igual que `surface` (SU2 a SU4) |
| AU3 | Hongo con cúpula | Cabezal de 25° | Soporte: 3065 mm en plano, 1436 con `surface`, ninguno con `overhangs` ni con `automatic` |
| TE1 | Bloque de 40 × 40 × 12 mm sin capas calculadas | `stress` | Error que pide calcular las capas en el análisis estructural, sin G-code. Tisma Slicer sale con 0; el motor sale con otro código |
| TE2 | Barra de 8 × 8 × 40 mm inclinada 45°, empotrada abajo y tirada a lo largo de su eje | `stress`, cabezal de 25° | Factor de seguridad de 5,97 con capas planas a 7,83 con capas curvas (+31 %) |
| TE3 | Barra horizontal tirada a lo largo | `stress` | Capas planas: el tirón ya va a lo largo de las capas |
