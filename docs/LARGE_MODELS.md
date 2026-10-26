# La vista con modelos grandes

Pedido del dueño (2026-10-26): ver técnicas como LOD y otras formas de dibujar para subir los cuadros por segundo con
modelos grandes, «tipo raycaster».

## Qué cuesta hoy

La vista dibuja con OpenGL 3.3 (`apps/tisma/renderer.cpp`):
- **El modelo:** todos sus triángulos en cada cuadro, aunque sean más chicos que un píxel.
- **La vista previa:** cada extrusión es una instancia de 36 vértices (un prisma con la sección del cordón, unido en
  inglete con la siguiente). Se dibujan todas las de las capas elegidas, también las de adentro de la pieza, que
  nunca se ven desde afuera.
- **Con oclusión ambiental** (el modo por defecto), la escena se dibuja dos veces: una para las normales y la
  profundidad, otra con la luz.

El costo crece con la cantidad de extrusiones, no con lo que se ve: una pieza grande con millones de tramos manda a
la GPU decenas de millones de vértices por cuadro aunque la mayoría estén tapados o sean más chicos que un píxel.

## Técnicas

| Técnica | Qué ahorra | De dónde |
|---|---|---|
| Ocultar lo de adentro | Las capas debajo de la de arriba solo muestran su cáscara: el relleno disperso y los perímetros internos quedan tapados por el perímetro externo. Se dibujan solo en la capa de arriba | Diseño propio; es una oclusión conservadora sabida por la forma de una pieza impresa |
| Recorte por capas y cajas (frustum culling) | Lo que queda fuera de la pantalla | Clásico de los gráficos por computadora; Clark 1976, «Hierarchical geometric models for visible surface algorithms» |
| Menos detalle de lejos (LOD) | Una capa más fina que un píxel se puede dibujar junto con la de al lado, como un cordón del doble de alto | Luebke y otros 2002, *Level of Detail for 3D Graphics*: elegir el detalle por el tamaño en pantalla |
| Malla simplificada mientras se mueve | Un modelo de millones de triángulos se dibuja con una versión de menos triángulos mientras gira la cámara o se arrastra | Garland y Heckbert 1997, «Surface Simplification Using Quadric Error Metrics» (SIGGRAPH 97) |
| Rayos primarios («tipo raycaster») | Un rayo por píxel contra el árbol de cajas (BVH) de los cordones: el costo crece con los píxeles y apenas con la cantidad de tramos (el logaritmo) | El árbol y el recorrido en la GPU ya están para el ray tracing (`src/ui/raytrace.*`, Wald 2007); el recorrido en GPU, Aila y Laine 2009, «Understanding the Efficiency of Ray Traversal on GPUs» |
| Oclusión jerárquica (Hi-Z) | Lo tapado por lo ya dibujado | Greene, Kass y Miller 1993, «Hierarchical Z-Buffer Visibility». Necesita OpenGL 4 o Vulkan para hacerlo en la GPU: queda para después |

## Plan

1. Medir. Hecho: `--bench-frames N` y el tiempo de la vista en la barra de estado (ver abajo).
2. Ocultar lo de adentro en la vista previa. Hecho (ver abajo).
3. Pasar la vista a Vulkan (decisión del dueño, 2026-10-26: «la idea es que el motor funcione en Vulkan», y que
   detecte solo si la máquina lo tiene y si no use OpenGL). El resto de los pasos se hacen ahí, donde el recorte puede
   correr en la GPU (`PLAN.md`, «Vista con Vulkan»). Hecho el primero (V1): la ventana y los paneles con Vulkan.
4. Rayos primarios para la vista previa, como un modo de la vista.
5. Capas juntas de lejos, y menos vértices por cordón (solo las caras que miran a la cámara).
6. Malla simplificada del modelo mientras se mueve.

Cada paso se compara con la imagen de antes: lo que cambia es el tiempo, no lo que se ve.

## Medir

- **`--bench-frames N`** (con `--screenshot`): cuando la vista está lista, la dibuja N veces más y escribe el tiempo
  por cuadro, hasta que la GPU termina (`glFinish`), con la cantidad de cordones dibujados y lo que tardó armar los
  datos de la vista previa.
- **En la barra de estado**, a la derecha: «Vista 8,3 ms · 120 FPS». Es lo que tarda la GPU en dibujar la vista 3D
  en cada cuadro, medido por la GPU misma (una consulta de tiempo de OpenGL, leída dos cuadros después para no
  esperarla), suavizado. Sirve para muestrear en cada máquina: los FPS que da son los que permite la vista, sin
  contar los paneles.

## Ocultar lo de adentro

`hidden_inside` (`src/ui/scene.*`) marca, al armar la vista previa, las paredes internas y el relleno que la pieza
tapa por todos lados:
- adentro de lo que encierra la pared externa de la capa de abajo y de la de arriba, con una celda de margen: esas
  capas los tapan por debajo y por encima, y la pared externa de su propia capa por los costados;
- y lejos de un escalón: donde la pared externa de la capa vecina se aparta más de 0,05 mm de la de esta capa, la
  ranura entre sus cordones deja ver los de adentro, hasta unos dos cordones. Sin esto, la cúpula de una esfera
  mostraba puntitos donde faltaban paredes internas;
- nunca en la primera ni en la última capa; una pared externa que no cierra deja la fila sin saber, afuera.

Las zonas son grillas de celdas (0,2 mm, o más en piezas grandes) llenadas por la regla par-impar en cada fila; las
capas se calculan en paralelo.

La GPU recibe los cordones en dos bloques, cada uno por capa: lo que puede verse y lo tapado. Con la pieza entera a la
vista (desde la primera capa hasta la última y todos los tipos mostrados) se dibuja solo el primero; un corte, una capa
sola o un tipo oculto en la leyenda dibujan los dos. El ray tracing sigue usando todos los cordones.

## Medido

En el contenedor, sin GPU: la dibuja la CPU (llvmpipe), así que los tiempos sirven para comparar, no son los FPS de
una placa de video. Vista de 1200 × 800, la pieza entera, Release.

| Pieza | Cordones | Dibujados | Antes | Después | Armar los datos |
|---|---|---|---|---|---|
| Esfera de 80 mm (cada capa es un escalón: el peor caso) | 188 664 | 157 196 | 3 400 ms | 2 231 ms (rápida) | 181 ms |
| Cilindro de 60 × 80 mm (paredes rectas) | 181 384 | 52 499 | 2 330 ms | 633 ms (oclusión ambiental) | 121 ms |

La imagen de la esfera es la misma píxel por píxel antes y después.
