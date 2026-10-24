# Verificación del G-code exportado

Cada G-code que exporta el motor se vuelve a leer desde el disco y se verifica antes de quedar con el nombre pedido
(`src/gcode/export.*`, `src/gcode/check.*`). Así, un archivo `.gcode` del motor siempre está completo y pasó la
verificación. El verificador no usa los recorridos que usó el escritor: los límites salen de la pieza y de los ajustes,
así que un error del escritor se nota.

## Cómo se exporta

1. El G-code se escribe en `<archivo>.part`.
2. Ese archivo se vuelve a leer desde el disco y se verifica.
3. Si pasa, se renombra a `<archivo>`.
4. Si no pasa, se renombra a `<archivo>.rejected` para poder revisarlo. Con el nombre pedido no queda nada, y
   `tisma-engine slice` termina con el código de salida 6 y lista los problemas (los primeros 50, con su línea).
5. Si no se puede escribir o leer (carpeta inexistente, disco lleno), no queda nada y el código de salida es 2.

Antes de escribir, el laminado ya rechaza (código 4) los modelos que no se pueden imprimir:
- sin triángulos;
- ninguna capa cerrada;
- fuera de la cama o más alto que la máquina;
- la falda o el borde fuera de la cama;
- nada para extruir (toda la pieza más fina que las líneas).

## Qué se verifica

El G-code de inicio y de fin del usuario (los bloques `;TYPE:Custom`) no se juzga, porque puede tener cualquier cosa:
macros, mensajes, purgas en el borde de la cama. De esas líneas solo se sigue el estado: G90 y G91, M82 y M83, G92 y
G28. Todo lo demás lo escribió el motor y se verifica:

| Qué | Regla |
|---|---|
| Archivo completo | Termina con un salto de línea y con el resumen final (`; estimated printing time = …`). Un archivo cortado no pasa |
| Lectura | Cada línea es un comando (`G`, `M` o `T` con un número entero) con parámetros de una letra y un número. Nada de NaN, infinitos ni texto suelto |
| Lugar | Cada X e Y queda dentro de la pieza agrandada por la falda, el borde, dos anchos de línea y 1 mm de holgura para los viajes; con `machine.bed`, además dentro de la cama. Z queda entre 0 y la capa más alta más el alzado de los viajes |
| Velocidad | Ningún F supera la mayor de `speed.travel`, `speed.max` y `retraction.speed` |
| Capas | Cada `;LAYER_CHANGE` trae un `;Z:` más alto que el anterior y una altura positiva. Cada extrusión ocurre a la Z de su capa: el alzado de un viaje se deshace antes de extruir |
| Extrusión | El filamento de cada movimiento que extruye coincide con ancho × alto × largo / área del filamento, con un 2 % de margen más el filamento de 5 µm de recorrido (los tramos demasiado cortos para escribirse se suman al siguiente) |
| Retracción | Cada retracción mide `retraction.length`, no se retrae dos veces seguidas y se deshace por completo antes de volver a extruir. No se empuja filamento sin una retracción que deshacer, ni se tira hacia atrás en medio de un movimiento |
| Ventilador | `M106 S` entre 0 y 255 |
| G0 | Nunca mueve el extrusor |
| Contenido | Al menos una capa y una extrusión |

También cuenta: líneas, capas, extrusiones, viajes, retracciones, filamento neto, recorrido extruido y de viaje, y la
Z más alta.

## Cómo se probó

- **El cubo de 10 mm:** pasa limpio, con números razonables (50 capas, filamento entre 100 y 420 mm).
- **El mismo G-code dañado de nueve maneras:**
  - cortado a la mitad o sin el resumen final;
  - un salto fuera de la pieza;
  - una Y ilegible (`Ynan`);
  - un avance de 99 999 mm/min;
  - el ventilador en 300;
  - una capa que baja;
  - un movimiento con un 50 % más de filamento;
  - una retracción sin deshacer.

  Cada daño se detecta con el mensaje que corresponde. El G-code de inicio y de fin del usuario, con texto libre,
  macros y una purga fuera de la pieza, no genera falsos avisos.
- **Matriz fija:** 4 piezas (cubo, aro con agujero, dos islas, mesa con voladizo) por 20 variantes de ajustes. Las
  variantes: sin paredes, una, cinco; sin relleno, macizo, giroide, grilla; E relativo; soportes; borde sin falda;
  primera capa gruesa y capas finas; alzado en los viajes; sin retracción; capas lentas; costuras; Klipper; filamento
  de 2,85 mm con líneas anchas; con cama. Las 80 combinaciones pasan sin un problema.
- **Prueba aleatoria (fuera del repositorio, en la sesión):** 5300 combinaciones al azar de ajustes y piezas,
  incluidas una esfera y un cilindro de muchos triángulos y paredes de 0,5 mm. Encontró un caso real: una pieza toda
  más fina que las líneas daba un G-code sin extrusiones. Ahora el laminado la rechaza con un mensaje claro.
- **Prueba de la CLI:** encontró otro caso real. En una cama justa para la pieza, la falda quedaba fuera de la cama y
  el G-code se escribía igual. Ahora se rechaza antes de escribir (código 4).

**Costo:** en una esfera de 50 mm (159 200 triángulos, 297 115 líneas, 8,4 MB), verificar tarda 72 ms. Laminar tarda
521 ms y escribir 278 ms.

## Cambios por altura y flujo

Con `extrusion.flow` y la lista `changes` de los ajustes (`docs/CONFIG.md`), el verificador juzga cada capa con el
flujo y el largo de retracción que le tocan:
- `gcode_limits` arma la tabla por altura;
- el verificador la recorre en cada `;Z:`.

**La velocidad:** un cambio de `speed` sube el tope de avance permitido a `speed.max` por el mayor porcentaje.

**El G-code de un cambio** (`M104`, `M140`, el texto de `gcode`) va como `;TYPE:Custom` y no se juzga. Después el
escritor vuelve a poner el tipo anterior, así los viajes y retracciones que siguen se juzgan como siempre.

