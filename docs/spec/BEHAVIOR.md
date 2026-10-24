# Comportamiento del laminado

Requisitos de **qué** tiene que pasar, y lecciones medidas en Tisma Slicer. El **cómo** sale de las fuentes de
`SOURCES.md` o es diseño propio.

## Mallas y capas

- **Mallas imperfectas:** se aceptan mallas con huecos chicos o caras invertidas. El corte cierra los contornos que
  quedan abiertos por menos de una tolerancia chica, y nunca se cae por una malla sucia.
- **Alturas de capa:**
  - la primera capa tiene su propia altura;
  - el resto, la altura de capa;
  - la Z de cada capa es la de su tope;
  - el corte se hace a mitad de la capa.
- **Agujeros:** una sección puede tener varias islas, y cada isla sus agujeros.

## Perímetros

- **Cuántos:** los perímetros pedidos, en las piezas comunes.
- **Paredes más finas que dos perímetros:** cordones de ancho variable, sin huecos ni excesos en el medio (Kuipers et
  al. 2020). Un detalle que entra en un solo cordón se imprime con un solo cordón más angosto o más ancho; no se pierde.
- **Ancho de los cordones:** entre un mínimo y un máximo derivados de la boquilla.
- **Orden:** de adentro hacia afuera, con el externo al final, salvo que un ajuste pida lo contrario.
- **Costura** (inicio de cada perímetro cerrado):
  - `aligned`: preferir esquinas cóncavas y que las costuras de capas seguidas queden una sobre otra;
  - `nearest`: el punto más cercano al final del movimiento anterior;
  - `random`;
  - `rear`: el lado de Y máxima.

## Superficies y rellenos

- **Superficies superiores e inferiores:** las capas sólidas pedidas, contadas desde cada superficie expuesta hacia
  adentro.
- **Puentes:** una superficie inferior sin nada debajo es un puente.
  - Se tiende en la dirección que cruza el vano más corto.
  - Lección medida en Tisma: una superficie que apoya sobre el relleno de abajo **no** es un puente. Hay que medir
    contra lo que realmente está impreso en la capa de abajo, no contra el borde de la capa anterior.
- **Relleno:**
  - rectilíneo: líneas paralelas que giran 90° cada capa;
  - grilla: dos direcciones por capa;
  - giroide: la superficie de `SOURCES.md` cortada en cada Z.
  - En todos, las líneas se anclan a los perímetros sin dejar huecos.
- **Superficies monotónicas:** las líneas de la superficie superior se imprimen siempre en el mismo sentido, sin
  volver sobre lo hecho.

## Viajes y retracción

- **Por dentro:** los viajes no cruzan perímetros si pueden ir por dentro de la pieza.
- **Retracción:** se retrae cuando el viaje sale de la pieza o es más largo que `retraction.min_travel`. Un viaje por
  el aire sin retracción deja hilos (Tisma lo avisa en su revisión antes de imprimir).

## Enfriamiento

Comportamiento acordado con el dueño, implementado así en Tisma:

1. Si una capa tarda menos que `cooling.min_layer_time`, se bajan las velocidades hasta `cooling.min_speed` para que
   tarde ese tiempo.
2. Si aun así es corta, se baja más: todas las extrusiones de la capa a una misma velocidad (la más alta que alcanza
   el tiempo), y las que ya iban más lento no cambian. Nunca por debajo de 1 mm/min.
3. Esas capas llevan el ventilador al máximo.

El motor tiene que hacer lo mismo.

## Soportes (fase F5)

- **Voladizos:** se soporta lo que cuelga más que el ángulo límite.
- **Puentes cortos:** los que la impresora puede hacer (`support.free_bridge_length`) no llevan soporte.
- **Separaciones:** el soporte deja una separación en Z y en XY con la pieza.
- **Interfaz:** bajo la pieza, capas densas; abajo, una base menos densa.
- **Retiro:** el soporte tiene que poder sacarse a mano. Más adelante: interfaz planchada y patrón concéntrico.

## Errores y avisos

- **Nunca se cae:** con una entrada imposible (malla vacía, objeto fuera de la cama), el motor termina con un mensaje
  claro y un código de salida distinto de 0.
- **Avisos:** los problemas del laminado (detalles que no se imprimen, capas que no llegan al tiempo mínimo) se
  escriben en un JSON aparte, para que Tisma los muestre.
