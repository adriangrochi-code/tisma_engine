# Caras pintadas

Pedido del dueño (2026-10-25): pintar sobre el modelo, como en todos los slicers, dónde va o no va el soporte, dónde
va la costura, la piel difusa y los colores. Las fichas que lo piden son `supports_more.md` (reglas 3 a 5,
bloqueadores y forzadores), `fuzzy_skin.md` (regla 4) y `multi_extruder.md` (regiones pintadas por color). La ficha
`model_tools.md`, que tiene la herramienta, todavía no llegó: la herramienta es diseño propio.

## Qué se puede pintar

| Canal | Marcas | Qué hace el motor | Estado |
|---|---|---|---|
| Soportes | Forzar, Bloquear | Lo que cuelga sobre una cara forzada lleva soporte, aunque la pendiente no lo pida; sobre una bloqueada, nunca. Con `support.automatic` en `false`, solo hay soporte bajo las forzadas. Hace falta `support.enabled` (ficha `supports_more.md`, reglas 3 a 5) | Hecho |
| Costura | Forzar, Bloquear | Un lazo que cruza una zona forzada empieza en ella; uno que cruza una bloqueada empieza afuera, si le queda algo afuera | Hecho |
| Piel difusa | Pintar | — | Cuando el motor tenga la piel difusa (`fuzzy_skin.md`) |
| Colores | Extrusor 1 a 16 | — | Cuando el motor tenga varios extrusores (`multi_extruder.md`) |

## Cómo se guarda

`src/mesh/paint.*`. Cada triángulo de la malla tiene, por canal, un árbol:
- o una sola marca para todo el triángulo;
- o el triángulo cortado por los puntos medios de sus lados en cuatro (el de la esquina 0, el de la 1, el de la 2 y el
  del medio), cada uno un árbol otra vez.

Así un pincel chico pinta un detalle en una cara grande sin tocar la malla. Como el árbol va con el triángulo y no con
coordenadas, la pintura sigue al modelo cuando se mueve, gira o escala. Un triángulo sin pintar no ocupa nada. Los
árboles se guardan en profundidad, un byte por nodo (una marca, o `0xFF` seguido de los cuatro hijos).

En el proyecto (`docs/PROJECT.md`), cada modelo pintado lleva un bloque `PNT ` con sus árboles. Un lector más viejo lo
salta. Un árbol roto, una marca fuera de rango o un triángulo que la malla no tiene se rechazan con el motivo.

## El pincel

Una esfera de radio elegible (0,2 a 20 mm) centrada en el punto de la cara bajo el mouse:
- un triángulo entero dentro de la esfera toma la marca;
- uno que cruza su borde se corta en cuatro, hasta piezas de un octavo del radio (0,05 mm como mínimo), y las más chicas
  toman la marca si su centro está dentro;
- cuatro hijos con la misma marca vuelven a ser uno;
- las caras que miran para el otro lado no se pintan: el pincel en el frente de una pared fina no pinta su espalda.

La distancia de un punto a un triángulo es la del pie del punto sobre el plano si cae dentro, y si no, la del lado más
cercano (diseño propio).

## Del pintado a cada capa

`src/slice/painted.*`. Antes de que `orient_consistently` dé vuelta algún triángulo (el árbol sigue el orden de las
esquinas), se juntan las piezas pintadas de cada marca. Para cada capa:
- cada pieza que llega a la capa se corta entre su base y su techo y se proyecta sobre XY;
- una cara vista de arriba o de abajo da un polígono, y se agranda;
- una pared da una línea, y se agranda como una banda.

Cuánto se agranda:
- soportes, medio cordón: la zona que cuelga queda justo debajo de la cara;
- costura, un cordón: la pared externa va medio cordón hacia adentro de la superficie.

## Soportes

`support_areas` (`src/support/support.cpp`):
- arma lo que cuelga en cada capa: lo del soporte automático (si está encendido), más lo que sale aunque sea 0,01 mm
  afuera de la capa de abajo dentro de las zonas forzadas, menos las zonas bloqueadas;
- después sigue como siempre: la columna hacia abajo, la separación y la interfaz.

Prueba: «the slicer follows painted support and seams» en `tests/test_paint.cpp`, un cubo con un estante.

## Costura

`order_paths` (`src/gcode/order.cpp`) recibe las zonas de la capa para las paredes:
- un lazo que toca una zona se recorre con puntos cada 0,25 mm (`kSeamPaintStep`), así una mancha entre dos esquinas
  se encuentra;
- forzada, con la costura alineada o atrás, va al medio del tramo pintado más largo del lazo: una línea pintada deja
  la costura sobre ella;
- forzada, con la costura más cercana o al azar, el modo elige entre los puntos pintados;
- bloqueada, el modo elige entre los puntos de afuera;
- el punto elegido se agrega al lazo si cae entre dos de los suyos.

Prueba: en un cubo de 20 mm con una franja pintada en Y = 15 sobre la cara +X, todas las paredes externas empiezan a
menos de 0,3 mm de la franja. En la interfaz, `ui_paint_save`, `ui_paint_reopen` y `ui_paint_same_gcode`: la costura
pintada, guardada en el proyecto y vuelta a abrir, da el mismo G-code.

## En la interfaz

Herramienta **Pintar** (P) en la columna de la placa (`docs/UI.md`):
- canal: Soportes, Costura; Piel difusa y Colores aparecen apagados hasta que el motor los tenga;
- marca: Forzar o Bloquear;
- tamaño del pincel; **Borrar todo** quita el canal del modelo elegido.

Clic izquierdo y arrastrar sobre el modelo pinta; con Shift, borra. Arrastrar fuera del modelo gira la vista. Las
marcas se ven sobre el modelo: verde forzar, rojo bloquear. Con `--paint canal,marca,X,Y,Z,RADIO` se pinta al
abrir, para las capturas y las pruebas.

## Lo que falta

- Volúmenes bloqueadores y forzadores (prueba SM12 de `supports_more.md`): piezas que se agregan, no caras.
- La piel difusa y los colores, con sus partes del motor.
- Pintar con relleno inteligente (toda una cara plana de un clic) y deshacer.

El pincel no mira todos los triángulos: el árbol de cajas del modelo (`TriangleIndex`, `docs/UI.md`, «Modelos
grandes») le da los que están cerca. En una esfera de un millón de triángulos, un paso tarda 2 ms.
