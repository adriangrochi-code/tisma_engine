# La interfaz: `tisma`

`tisma` es la interfaz propia del motor: abre un modelo, cambia los ajustes, lamina, muestra el G-code capa por
capa y lo exporta verificado. Usa el mismo motor que `tisma-engine`; no hay nada que la línea de comandos haga de
otra manera.

## Compilar

Se compila junto con el motor (opción `TISMA_BUILD_UI`, encendida por defecto). Necesita OpenGL 3.3.

- **Windows:** nada más que Visual Studio 2022. El programa queda en `build/apps/tisma/Release/tisma.exe`, con el
  ícono de Tisma.
- **Linux:** además, los paquetes de desarrollo de X11. En Ubuntu 24.04:

  ```sh
  sudo apt-get install libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl-dev
  ```

  El programa queda en `build/apps/tisma/tisma`. En Wayland corre a través de XWayland.
- `-DTISMA_BUILD_UI=OFF` compila solo el motor, sin ventana ni esas dependencias.

## Usar

```sh
tisma                          # la interfaz vacía
tisma modelo.stl               # con un modelo
tisma proyecto.tisma           # un proyecto guardado
tisma pieza.gcode              # para mirar un G-code
tisma ajustes.json modelo.stl  # con esos ajustes
```

### La ventana

| Lugar | Qué tiene |
|---|---|
| Barra superior | El logo; las pestañas **Preparar**, **Vista previa**, **Análisis** y **Calibración**; los botones **Abrir**, **Laminar** (violeta), **Exportar G-code** y **Más** (guardar el proyecto, idioma, tema claro u oscuro, acerca de, salir). |
| Panel izquierdo | Los ajustes por secciones y tarjetas (ficha `settings_panel.md`, reglas 1 a 20; `src/ui/settings_layout.*` y `src/ui/settings_view.*`). Arriba de todo, la **tarjeta de la impresora** (reglas 21 a 36; `src/ui/printer_card.*`): el dibujo del tipo de impresora (resina, cinta, delta, cerrada o abierta; dibujos propios, sin fotos), la lista de impresoras con buscador (las de la persona primero, después las del programa y las del catálogo agrupadas por marca, cada una con su dibujo; al final **Agregar impresora…**), la lista de camas con su dibujo en las impresoras del catálogo que traen varias, el engranaje que abre la **configuración de la impresora** y, debajo, la **boquilla**: un clic pasa al perfil de la impresora con esa boquilla (creado desde el catálogo si no existe), con un aviso que tiene **Deshacer**; una impresora propia ofrece **Otra…**, que cambia solo el diámetro y deja el perfil marcado. La configuración de la impresora es una página a todo el ancho (regla 37), con **‹ Placa** (o Escape) para volver: Máquina, Boquilla y extrusor, Límites y G-code con las mismas tarjetas, y además **Calibración** (cada prueba que sirve a la impresora con lo que fija y su valor, «Sin calibrar», «Calibrado el …» o «Calibrado con otra boquilla», el filamento con que se hizo, **Calibrar** y **Calibrar en orden**; se anota al usar un resultado en Calibrar), **Conexión** (la de la pestaña Impresora) y **Notas** (texto libre). Lo calibrado y las notas se guardan por impresora en `interface.json` (`printers`). Debajo, las pestañas **Impresión** y **Material**, cada una con su fila de perfil (ver «Perfiles y proyectos»), y a la derecha el **nivel**: Básico (lo que casi toda impresión necesita), Normal (más ajustes para afinar), Avanzado (todos) o Personalizado (los que elige la persona), con Ctrl+Alt+1 a 4. Debajo, **Ver todo junto** muestra todas las secciones una debajo de otra, cada una con su título (y **Ver por secciones** vuelve). **Personalizado** empieza con lo que mostraba el nivel de antes; **Elegir ajustes…** (en el menú del nivel y arriba del panel) abre el árbol pestaña › sección › apartado › ajuste con una casilla en cada uno (la de una sección o un apartado marca todo lo de adentro; a medias se ve con una raya), un filtro, «Partir de» Básico, Normal o Avanzado, Todos y Ninguno; en el panel, el ojo tachado sobre un ajuste lo quita, y **+ Agregar ajuste** al pie de cada tarjeta lo devuelve. **Más › Exportar vista… / Importar vista…** la lleva a otra computadora (un JSON `tisma-ui-view`, versión 1; las claves que no existen se ignoran). Cada tarjeta tiene un menú de tres puntos con **Restablecer**, que vuelve sus ajustes a los del perfil guardado. A la izquierda, la columna de secciones con su ícono (Impresión: Calidad, Resistencia, Velocidad, Soportes, Adhesión, Otros; Material: Temperaturas, Enfriamiento, Flujo, Retracción, Necesidades; Impresora: Máquina, Boquilla y extrusor, Límites, G-code); un punto violeta marca la que tiene cambios, y al pasar el puntero se ven sus apartados. A la derecha, los apartados de la sección elegida en **tarjetas plegables**: cerrada, una línea con lo más importante («0,2 mm · primera 0,2 mm», «40 % · Rectilínea»); con cambios respecto del perfil, «2 cambios» en violeta. Los ajustes que cuelgan de un interruptor se ven solo con él encendido (soportes, Z anti-aliasing, relleno ondulado, denso bajo el techo, bufanda; la tarjeta Árbol de Soportes, con el estilo en árbol). Un nivel nunca esconde un ajuste cambiado: se ve con la etiqueta gris de su nivel; al pie de cada tarjeta, «N ajustes más…» con **Mostrar todos**. El nivel, la vista personalizada, «Ver todo junto», la sección de cada pestaña y las tarjetas abiertas se recuerdan (`ui.level`, `ui.custom_base`, `ui.custom_visible`, `ui.custom_known`, `ui.single_column`). El nivel no cambia el G-code. Abajo: cargar, guardar y restablecer todos los ajustes como JSON. Las velocidades son de la impresora, y una que no trae las suyas toma las de respaldo de una Ender-3, con un aviso (`docs/CONFIG.md`, «Velocidades»). |
| Vista 3D | La cama con su grilla y el modelo, o el G-code. Arriba, las vistas (perspectiva, arriba, frente, atrás, izquierda, derecha) y el zoom. |
| Barra de estado | El modelo (triángulos y tamaño), el resultado del laminado, la verificación del G-code y si se usó la GPU. |

### Ratón y teclado

| Acción | Cómo |
|---|---|
| Girar la vista | Arrastrar con el botón izquierdo |
| Mover la vista | Arrastrar con el botón derecho o el del medio |
| Acercar o alejar | La rueda |
| Encuadrar | Doble clic, o el botón de encuadrar |
| Abrir | Ctrl+O, el botón Abrir o soltar el archivo sobre la ventana |
| Guardar el proyecto | Ctrl+S (Ctrl+Mayús+S: guardar como) |
| Laminar | Ctrl+R |
| Exportar | Ctrl+E |
| Capas de la vista previa | Flechas arriba y abajo, Re Pág y Av Pág, Inicio y Fin, o la rueda sobre la barra de capas |
| Elegir un modelo de la placa | Clic sobre él; clic en la cama vacía: ninguno |
| Mover un modelo | Arrastrarlo con el botón izquierdo |
| Herramientas de la placa | M mover, R rotar, S escalar, F apoyar en una cara, A organizar, P pintar, Supr quitar, Esc ninguna |

### Preparar

Al abrir un STL, el modelo queda centrado en la cama y apoyado en ella (Z = 0). Cambiar el tamaño de la cama lo vuelve
a centrar. **Laminar** corre el motor en segundo plano y la ventana sigue respondiendo.

#### Herramientas de la placa

**Ajustes de una pieza** (fichas `settings_levels.md`, reglas 10 y 11, y `model_tools.md`, regla 7): clic derecho
sobre una pieza abre su menú. En el nivel Básico solo dice que esto está en Normal y Avanzado; en los demás ofrece
«Relleno», «Capas y perímetros», «Capas sólidas» y «Agregar ajuste ▸» (paredes, relleno y capas sólidas). Cada uno
agrega a la pieza esos ajustes con el valor del perfil y abre la ventana «Ajustes de …», con las mismas filas del
panel, una cruz por ajuste para devolverlo al perfil y «Agregar ajuste». Se guardan en el proyecto
(`models[i].settings`) y pasan a las piezas que salen de un corte o de separar. `--object-settings JSON` le pone esos
ajustes a la primera pieza al abrir, para las capturas.

La placa puede tener varios modelos (pedido del dueño, 2026-10-25; los grupos de la ficha `interface.md`). A la
izquierda de la vista 3D, una columna con:

| Herramienta | Qué hace |
|---|---|
| **Agregar** | Suma un modelo (STL, OBJ o 3MF) a la placa y la ordena. Soltar uno sobre una placa con modelos también lo suma; **Abrir** empieza una placa nueva |
| **Quitar**, **Duplicar** | El modelo elegido |
| **Mover** (M) | Su posición en X e Y (el centro de su caja). Arrastrar un modelo lo mueve con cualquier herramienta |
| **Rotar** (R) | Ángulos en X, Y y Z (R = Rz·Ry·Rx), ±90° por eje, **Sin giro** |
| **Escalar** (S) | En % o en mm por eje, los tres juntos o cada uno, **Tamaño original**, y **Espejar** en X, Y o Z alrededor del centro de su caja (un modelo girado toma su giro y su tamaño en la malla antes) |
| **Apoyar en una cara** (F) | Un clic en una cara del modelo la deja contra la cama |
| **Medir** (U) | Un clic en una cara toma su plano; en una superficie curva reconoce un agujero o un cilindro (su diámetro, con «Debería medir» para escalar a ese diámetro) o un cono (su ángulo y los diámetros de sus dos extremos); Shift + clic, el vértice más cercano. Entre dos elementos del mismo modelo, la distancia (puntos, punto y cara, caras paralelas) o el ángulo entre caras; con una distancia, **Debería medir** y **Escalar** agrandan o achican el modelo parejo en los tres ejes (ficha `model_tools.md`, regla 20). `--measure T1,T2` elige dos triángulos del primer modelo como caras |
| **Cortar** (C) | Un plano horizontal a la altura que se escribe, dibujado sobre la vista; **Cortar** deja dos piezas cerradas, una al lado de la otra, con el extrusor del modelo (la pintura no pasa a las mitades). «Dar vuelta la parte de abajo» la apoya sobre la cara del corte (ficha `model_tools.md`, reglas 13 y 18). `--cut Z` corta el primer modelo al abrirse. **Separar en piezas** deja cada pedazo suelto del modelo como una pieza, con los huecos que tiene adentro (regla 14) |
| **Organizar** (A) | Todos los modelos en una grilla compacta, 6 mm entre cajas, centrada en la cama, sin girarlos (ficha `placement.md`, regla 2) |
| **Pintar** (P) | Soportes, costura (Forzar o Bloquear) o piel difusa, con un pincel de 0,2 a 20 mm: clic izquierdo y arrastrar sobre el modelo pinta, con Shift borra (`docs/PAINTING.md`). Verde forzar, rojo bloquear. Colores aparece apagado hasta que el motor tenga varios extrusores |

Cada modelo queda siempre apoyado en la cama. El elegido se ve más claro. La lógica está en `src/ui/plate.*`, sin
OpenGL, con sus pruebas (`tests/test_plate.cpp`). Un proyecto guarda cada modelo ya ubicado (con su giro y su escala
aplicados) y su pintura, así se abre igual; ya no se queda solo con el primero.

#### Modelos grandes

Medido con una esfera de 1 002 528 triángulos (Release, `tests` sin interfaz):
- **Arrastrar:** antes, cada cuadro rehacía la malla ubicada, la de la placa y la copia de la vista (unos 200 ms sin contar
  la subida a la GPU: 5 cuadros por segundo). Ahora la vista corre el modelo con un desplazamiento en el shader y la malla se rehace una vez,
  al soltarlo.
- **Clic y pincel:** cada modelo ubicado tiene un árbol de cajas (`TriangleIndex`, `src/ui/plate.*`), armado la primera
  vez que hace falta después de un cambio (250 ms). Un clic pasa de 57 ms a 3 µs, y un paso del pincel, de 13–18 ms
  a 2 ms.
- **El motor**, para comparar: leer 83 ms, laminar 2,0 s, G-code 0,5 s, verificarlo 0,04 s y la vista previa 0,06 s.
- **Vulkan** (2026-10-26, `PLAN.md`, «Vista con Vulkan»): `--graphics auto|vulkan|opengl` elige qué dibuja la
  ventana. El programa detecta si la máquina tiene Vulkan al intentar arrancarlo (el cargador, un dispositivo Vulkan
  1.1, una superficie para la ventana y los shaders de la vista); si no puede, avisa y sigue con OpenGL. Vulkan dibuja
  los paneles y la vista 3D (cama, modelo, pintura, vista previa, MSAA y FXAA) con la luz rápida; la oclusión ambiental
  y el ray tracing todavía no, así que `auto` usa OpenGL hasta que estén. `--compare-view otra.png` compara la vista
  con la de otra captura (la de OpenGL): las pruebas `ui_view_*` piden que menos del 0,5 % de sus píxeles difiera en
  más de 24 niveles, porque cada driver rasteriza un poco distinto los bordes.
- **Vista previa** (2026-10-26, `docs/LARGE_MODELS.md`): con la pieza entera a la vista, las paredes internas y el
  relleno que la pieza tapa por todos lados no se dibujan; la imagen es la misma. En un cilindro de 60 × 80 mm se
  dibujan 52 499 cordones de 181 384, y el cuadro pasa de 2 330 a 633 ms en la GPU por software del contenedor. La
  barra de estado muestra lo que tarda la GPU en dibujar la vista y los FPS que permite; `--bench-frames N` lo mide
  para las capturas.

El laminado hace lo mismo que `tisma-engine slice`, sin escribir archivos. Va por etapas (2026-10-26): si desde el
último laminado cambiaron solo ajustes que lee el G-code (velocidades, límites de movimiento, enfriamiento y
ventilador, retracción, temperaturas, flujo, el G-code de la impresora), se rehace solo el G-code, su verificación y la
vista previa, con el mismo resultado byte a byte; la barra de estado dice «G-code rehecho». Pasos:
- las comprobaciones de la malla y de la pieza laminada;
- el G-code en memoria;
- su verificación (`docs/GCODE_CHECK.md`).

Si la pieza no se puede imprimir (fuera de la cama, más alta que la máquina, más fina que las líneas), un cuadro
muestra el motivo que da el motor, en inglés como en la línea de comandos.

### Vista previa

Muestra el G-code tal como se imprime: es el mismo texto que se exporta, leído de nuevo
(`src/gcode/preview.hpp`). Cada extrusión se dibuja como un cordón de su ancho y su alto.
- **Barra de capas:** a la derecha, con dos manijas para la capa de arriba y la de abajo que se ven, cada una con su
  altura y su número («6.00 (30)», «0.20 (1)»); la rueda y las flechas arriba y abajo mueven la de arriba. «Solo esta
  capa» deja una sola. Un **«+»** junto a la capa de arriba agrega un cambio de color al empezar esa capa; con clic
  derecho, un menú con cambio de color, **Pausa…** (pide el mensaje que muestra la impresora, que va como `M117`) y
  **G-code propio…**. Cada uno es un cambio por altura (`changes`, a mitad de camino entre esa capa y la de abajo, así
  empieza esa capa), se guarda con los ajustes y en el proyecto, cambia el G-code al laminar de nuevo y queda como una
  marca de color en la barra: al pasar el puntero se ve qué es, y con clic se edita su texto o se quita.
- **Barra de movimientos:** abajo, el último movimiento que se dibuja dentro de la capa de arriba, con su número
  («Movimiento 41 de 41»); las flechas izquierda y derecha lo mueven. La boquilla se dibuja al final de ese
  movimiento.
- **Leyenda:** abajo a la izquierda.
  - **Color por** (ficha `preview.md`, regla 1): tipo de línea, alto de capa, ancho, velocidad, velocidad alcanzada,
    ventilador, temperatura, caudal, caudal alcanzado, tiempo de capa (lineal o logarítmico), extrusor y color por
    capa. Cada movimiento lleva lo que el G-code fijó al pasar (M106/M107, M104/M109, T, M600) y su tiempo con las
    velocidades y aceleraciones de la impresora (`time_preview`, el mismo cálculo que el tiempo estimado, capa por capa
    desde parado); la velocidad alcanzada es el largo sobre ese tiempo.
  - Por tipo de línea: cada tipo con su casilla para ocultarlo, su tiempo, una barra con su parte del tiempo de los
    movimientos y su filamento en metros y gramos (con `extrusion.density`, o 1,24 g/cm³ si no se sabe). En las vistas
    con números, una escala de 11 pasos iguales de azul (lo menos) a rojo (lo más); en la logarítmica, iguales en
    logaritmo; el extrusor y el color, uno por fila.
  - **Fila de íconos** (regla 11), cada uno con su texto al pasar el puntero: viajes, barridos (los viajes que
    retraen mientras van), retracciones, vueltas de retracción, costuras (donde empieza cada lazo de pared externa),
    cambios de herramienta, cambios de color, pausas, G-code propio, centro de gravedad de lo dibujado y la boquilla en
    la última extrusión de la capa de arriba (con su posición y velocidad al pasar el puntero). Las marcas son puntos de
    colores distintos sobre la vista; viajes, costuras y boquilla vienen encendidos. El lector del G-code las toma de
    las retracciones (E solo, G10/G11, el barrido), de `;TYPE:External perimeter`, `T`, `;COLOR_CHANGE`/`M600`,
    `;PAUSE_PRINT`/`M601` y `;TYPE:Custom`.
  - Los números: tiempo estimado, primera capa, filamento (metros y cm³), capas y líneas del G-code.
- **Teclas de la vista** (placa y vista previa, regla 10): 0 isométrica, 1 arriba, 2 abajo, 3 frente, 4 atrás,
  5 izquierda, 6 derecha; K pasa a vista ortogonal y vuelve (se recuerda; se hace con un teleobjetivo: un ángulo de 1°
  desde lejos, con el mismo tamaño en pantalla, así la luz y las sombras siguen iguales); Z encuadra la pieza elegida
  o, sin elección, todo; E muestra el nombre de cada pieza; L oculta o muestra la leyenda.

**Abrir** un `.gcode` (o un `.bgcode`, el G-code binario) lo muestra igual, también si no lo escribió el motor: el
lector entiende G-code sin los comentarios del motor; sin `;TYPE:`, la vista por tipo dice «Sin tipos» y las demás
(altura, velocidad, ancho por el caudal) siguen andando. **Más › Vista previa de G-code…** abre uno, y soltar otro
archivo sobre la ventana lo reemplaza. Ese G-code no se puede volver a laminar ni exportar.

### Perfiles y proyectos

**Perfiles** (`docs/PROFILES.md`): los ajustes se reparten en tres perfiles con nombre, la impresora, el filamento y la
impresión, que se eligen por separado arriba de cada sección del panel.
- La lista tiene los perfiles **de fábrica** (una impresora Marlin genérica; PLA, PETG, ABS y TPU; capas de 0,12, 0,20
  y 0,28 mm) y **mis perfiles**.
- Elegir uno cambia solo sus ajustes. Si después se cambia alguno, el nombre lleva un **asterisco**.
- **Guardar** (el ícono de guardar) escribe los cambios en el perfil elegido; si es de fábrica, pide un nombre.
- **⋮** tiene **Guardar como perfil nuevo…** y **Eliminar**.
- En Impresora y Filamento, **Agregar desde el catálogo…** busca entre las impresoras y los filamentos de los
  fabricantes, muestra la ficha con sus fuentes y lo agrega a tus perfiles (`docs/CATALOG.md`).
- La compensación de la calibración de dimensiones y el flujo van con el filamento; la retracción, con la impresora.

**Proyectos** (`docs/PROJECT.md`): **Más → Guardar el proyecto** (Ctrl+S) guarda en un archivo `.tisma` los ajustes,
los perfiles elegidos, la pieza y la calibración en curso. **Abrir** lo trae de vuelta; una torre de calibración vuelve
a su pestaña, laminada, lista para elegir el segmento.

### Exportar

**Exportar G-code** pide el nombre y usa la exportación verificada del motor:
1. escribe `<archivo>.part`;
2. lo vuelve a leer del disco y lo verifica;
3. solo entonces lo renombra.

Si la verificación falla, el archivo queda como `<archivo>.rejected` y un cuadro muestra los problemas, línea por
línea. Exportar se habilita solo con un laminado al día y verificado. Si el modelo o los ajustes cambiaron, la barra
de estado pide laminar de nuevo.

### Análisis

Muestra cuántos vóxeles tendría el análisis estructural de la pieza (`docs/ANALYSIS.md`), en calidad normal y alta:
- los vóxeles de la pieza (los que atraviesa un cordón);
- los de la grilla que los contiene;
- el tamaño del vóxel;
- la memoria y el tiempo aproximados.

Si una calidad pasa el máximo del análisis (8 millones de vóxeles de grilla), lo avisa. El análisis completo, con
apoyos y cargas, corre por ahora con `tisma-engine analyze`.

### Calibración

La pestaña **Calibración** (`docs/CALIBRATION.md`) es una lista de tarjetas, una por prueba: dimensiones, primera capa,
temperatura, flujo, retracción, velocidad de retracción, avance de presión, velocidad máxima, tolerancias de encastre y
pasos del extrusor. Cada una dice en una línea para qué sirve y tiene un botón **Crear**. No hay
nada que configurar: la pieza se arma con un rango elegido desde los ajustes actuales y se lamina sola. Después, la
tarjeta activa guía en tres pasos:

1. **Creada y laminada:** cuántos segmentos y cuánto tarda, con **Exportar G-code**.
2. **Imprimila y mirala:** qué mirar en esa pieza (en la de temperatura: la cresta, el estante plano, el número y las
   figuras, y el perno que se empuja con el dedo).
3. **Elegir:**
   - **Torres:** un botón por segmento, con su valor. Al elegir uno, la vista muestra la torre laminada hasta ese
     segmento, y **Usar** pasa el valor a su ajuste y quita los cambios por altura.
   - **Dimensiones:** se cargan las medidas de X, Y, Z y el agujero; la tabla muestra la compensación actual y la
     nueva, y **Aplicar** la pasa a los ajustes (**Restablecer** vuelve a 100 %).
   - **Primera capa:** qué significa cada aspecto de los cuadrados y qué corregir en la impresora.
   - **Tolerancias de encastre:** un botón por juego; al elegir uno, dice cuánto dejar en los diseños.
   - **Velocidad máxima:** al elegir un segmento, además el flujo volumétrico de su línea más rápida.

**Pasos del extrusor** no tiene pieza: **Abrir** muestra los pasos. Exporta un G-code de prueba que extruye 100 mm,
pide el valor actual del firmware y cuánto quedó hasta la marca, y da el valor nuevo con el comando o la línea para
guardarlo (Marlin, RepRapFirmware o Klipper, según el dialecto), con **Copiar**.

**Ajustar el rango (opcional)** cambia el inicio, el fin, el paso y el alto de los segmentos, y vuelve a crear la
torre. Al pie, **Próximas calibraciones** lista las que siguen. La pieza se corre a la derecha para que el panel no
la tape. En la vista previa, la leyenda dice el valor del segmento de la capa que se mira.

### Imagen: luz, oclusión, ray tracing y suavizado

El botón de la derecha de la barra de vistas elige cómo se dibuja la vista 3D. La elección se recuerda.

| Modo | Qué hace | Costo |
|---|---|---|
| **Rápida** | Luz por píxel: cielo y suelo, luz principal, luz de relleno, brillo especular con Fresnel y contorno, en espacio de color lineal | El más liviano |
| **Oclusión ambiental** (por defecto) | Además oscurece rincones, uniones entre líneas y el contacto con la cama, calculado en pantalla desde la profundidad y las normales | Solo se recalcula cuando cambia la imagen |
| **Ray tracing** | Sombras suaves de la luz principal y oclusión ambiental con rayos de verdad contra la escena (el modelo y la cama, o los cordones de la vista previa). La imagen se afina cuadro a cuadro hasta 256 muestras y después se queda quieta | Mientras afina usa la GPU; quieto, nada |

- **Suavizado de bordes:** multimuestreo (MSAA 2×, 4× u 8×, según la GPU; 4× por defecto) y además FXAA, que suaviza
  bordes finos y brillos que el multimuestreo no alcanza.
- **Filtro de moiré** (pedido del dueño, 2026-10-24; activado por defecto): de lejos, o en una pieza grande, las
  líneas de capa de la vista previa quedan más finas que un píxel; sus luces y sombras ya no se pueden muestrear y
  hacen bandas de moiré. El filtro mide en cada píxel cuánto abarca de la sección de la línea (las derivadas de
  pantalla) y, a medida que la línea se achica por debajo de dos píxeles, reemplaza su normal por el promedio de las
  normales de todo el perfil de la línea, cada una pesada por cuánto mira al ojo (lo que ese píxel vería de verdad),
  y ensancha el brillo lo que se abren esas normales (Toksvig 2005). De cerca no cambia nada. En la grilla de la cama, cada línea se desvanece cuando la siguiente de su clase queda a
  menos de unos píxeles (las mayores aguantan más, porque están más separadas); el contorno de la cama no cambia.
- **Ranuras entre capas** (pedido del dueño, 2026-10-26: mejorar la oclusión y el color «como Prusa 3.0», con las
  imágenes del lanzamiento como referencia de caja negra): en los modos rápido y de oclusión, cada cordón de la vista
  previa calcula cuánto cielo y cuánta luz principal le tapan el cordón de la capa de arriba y el de la de abajo, sobre
  el perfil, con la fórmula de horizontes de la oclusión ambiental (Bavoil, Sainz y Dimitrov 2008) resuelta exacta para
  dos cordones que se tocan. Así cada capa se ve como un tubo con su ranura oscura, como en una pieza impresa. Se
  aplica solo entre los tipos que suelen apilarse (paredes, relleno, soporte), dentro de las capas mostradas: la
  superficie de arriba, el borde y la última capa mostrada no tienen nada encima. La luz principal es suave (unos 35°),
  así la sombra del cordón de arriba entra con penumbra. De lejos la ranura se promedia con el resto del perfil, como
  las normales del filtro de moiré. Con ray tracing no se aplica: los rayos ya ven a los vecinos.
- **Sombra sobre la cama** (modo de oclusión ambiental, mismo pedido): un mapa de profundidad desde la luz principal
  (Williams 1978), rehecho solo cuando cambia la imagen, como la oclusión, y leído con filtrado de porcentaje cercano
  (Reeves, Salesin y Cook 1987), así el borde sale suave. Por ahora solo la cama recibe la sombra: sobre la pieza, los
  escalones entre capas (del tamaño de un texel del mapa) se sombreaban entre sí en anillos. La oclusión en pantalla
  ya no cuenta lo que está a menos de unas dos capas: eso lo hacen las ranuras.
- **Color:** la curva de tono se aplica al canal más brillante y los otros se escalan con él, así un color saturado
  brillante no se lava hacia el blanco; los grises quedan como antes.
- **Intensidad de la oclusión:** se elige entre 0 y 1,5.
- **El ray tracing usa la GPU con OpenGL 3.3, sin hardware especial.** La escena se arma en otro hilo como una
  jerarquía de volúmenes (BVH) y viaja a la GPU como texturas.
  - El recorrido de la GPU es una copia del de la CPU (`src/ui/raytrace.*`), que es la referencia.
  - Los cordones de la vista previa son los mismos prismas que dibuja la vista, unidos en inglete: las sombras y la
    oclusión de una pared son continuas, sin muescas entre tramos.
  - La prueba `ui_raytrace_matches_cpu` dispara en la GPU los rayos de sombra del primer cuadro, repite los mismos
    rayos en la CPU y exige que coincidan.
- **Modelos muy grandes:** si la escena no entra en las texturas de la GPU (más de 4 millones de primitivas, o
  texturas chicas), el ray tracing avisa y la vista sigue con oclusión ambiental.

### Ajustes y preferencias

- **Guardar…** escribe los ajustes en el formato de `docs/CONFIG.md`, y **Cargar…** los lee. Un archivo `.json`
  también se puede abrir con **Abrir**.
- **Ajustes de la interfaz:** parten de una impresora genérica de 256 × 256 × 256 mm con Marlin. Su G-code de
  inicio calienta la cama y la boquilla con las variables `[bed_temperature]` y `[temperature]`, que se editan en
  Filamento.
- **Lo que se recuerda entre usos:** el idioma, el tema, la última carpeta, los ajustes y los perfiles elegidos. Se
  guardan en `%APPDATA%\Tisma\interface.json` (Windows) o `~/.config/tisma/interface.json` (Linux); los perfiles del
  usuario, en la carpeta `profiles` de al lado.
- **Idiomas:** español (por defecto) e inglés. Se elige en **Más** o con `--lang en`.
- **Buscar un ajuste:** el campo con lupa de la barra superior (Ctrl+F) busca por el nombre del ajuste, su tarjeta,
  su sección, su clave y lo que hace, sin importar mayúsculas ni tildes. Enter o un clic llevan al ajuste: abren su
  pestaña, su sección y su tarjeta, lo traen a la vista y lo resaltan 2 s; si el nivel lo esconde, se ve igual con la
  etiqueta de su nivel hasta cambiar de sección (ficha `settings_panel.md`, regla 5).

### Créditos y fuentes

Ficha `credits.md`. Las fichas del panel cuya función sale de otra fuente (un artículo, un proyecto, una guía) dicen al
pie, en gris y chico, «Basado en FFF anti-aliasing (2017), de Song et al.»; el nombre es un enlace que abre la fuente,
y el mismo texto aparece al pasar el puntero por el título de la ficha. **Más > Créditos y fuentes** junta todas las
funciones (función, fuente, autor, licencia o tipo) y los componentes de terceros que lleva el programa, con su versión,
su licencia y el texto del aviso que esa licencia pide repartir.

Decisión del dueño (2026-10-27): todo lo que se tomó de un tercero (un texto, una guía, una documentación, una idea),
aunque no se haya copiado, va como «Basado en …», nunca como «texto propio»: no son textos nuestros, nos basamos en
ellos para hacer el nuestro.

Los datos están en `resources/credits.json` y los avisos en `resources/licenses/`; el programa los lleva adentro. Una
función nueva que sale de otra fuente suma su entrada allí (con `cards`, las fichas donde se ve) y su línea
**Basado en:** en la ficha; una biblioteca nueva, su fila en `THIRD_PARTY.md`, su entrada en `libraries` y su archivo
de licencia. Las pruebas `[credits]` fallan si falta alguna de esas partes.

### Ayuda

Pedido del dueño (2026-10-26): un asistente, un consejo de cada función la primera vez que se usa y un botón de
información al lado de cada ajuste.

- **Panel de ayuda** (ficha `assistant.md`): F1, el botón **?** de la barra superior (si entra junto al buscador) o
  **Más > Ayuda**. Se abre sobre el panel de ajustes con un buscador y las pestañas Problemas, Procedimientos,
  Glosario, Tutoriales y Calibraciones; al pie, «Basado en las guías de Rafa Barajas». Sus datos llegan con el paquete
  de perfiles (`knowledge/`, en la carpeta `pack/` junto a las preferencias); sin ellos, el panel lo dice. Tutoriales
  ofrece además el recorrido de la interfaz. `--help-panel problems|procedures|glossary|tutorials|calibrations` lo
  abre en esa pestaña, `--knowledge CARPETA` lee los datos de otra carpeta y `--help-item ID` abre un problema, un
  procedimiento o una guía de calibración. En **Más** se apagan los tutoriales, las definiciones del glosario y la
  ficha del material (`assistant` en las preferencias). Un clic en los avisos del laminado (barra de estado) los
  lista con **¿Cómo lo arreglo?**, que abre el problema de la ayuda que les corresponde.
  - **Problemas:** la lista con su síntoma; uno muestra sus causas (las de último recurso al final, con su nota), la
    pregunta para confirmarla y sus acciones: **Aplicar** un ajuste (el valor de ahora y el nuevo, acotado al rango de
    `docs/CONFIG.md`; el cambio queda sin guardar en el perfil y el aviso ofrece **Deshacer**), abrir un procedimiento,
    otro problema (con **Volver**) o la calibración, o un consejo. Una clave que el motor no tiene no da botón.
  - **Calibraciones:** las guías en orden, del filamento y de la impresora. Una guía dice para qué sirve, cuándo
    repetirla, la prueba que conviene hacer antes si no tiene fecha en la sección Calibración (con Abrir; no impide
    seguir), **Calibrar** (si el motor tiene esa prueba), los pasos, la tabla «Lo que ves / Qué significa», el resultado
    con el valor de ahora y los campos para calcular el valor nuevo: en las torres, inicio, paso y alto de franja
    (tomados de la última prueba generada) y la altura medida; en las demás, el valor leído o las capas. El valor
    propuesto se puede corregir antes de **Aplicar** (que fecha la guía y su prueba); si el resultado va en la
    impresora, lo avisa. Al pie, «Basado en» con sus fuentes, en lugar del pie del panel. **Cómo se hace** abre la
    guía desde la página de calibraciones y desde la sección Calibración de la impresora.
  - **Procedimientos:** las advertencias arriba, los pasos con su casilla, cómo evitarlo y las notas.
  - **Glosario:** los términos en orden alfabético. El botón (i) de un ajuste que nombra un término muestra su
    definición al pasar el puntero y en la tarjeta.
  - **Tutoriales guiados:** los de los datos de la ayuda se ofrecen con una tarjeta («¿Te muestro cómo?»: Mostrame,
    Después, No, gracias) al terminar el asistente del primer arranque, la primera vez que se elige un material o que
    se abre Análisis; o desde la pestaña Tutoriales (Empezar, o Seguir donde quedó). Cada paso ilumina la parte de la
    interfaz que nombra y oscurece el resto (sin tapar los clics), con una burbuja al lado: Atrás, Siguiente, Salir, y
    Abrir si el paso tiene un procedimiento o una calibración. Un paso que espera algo (elegir impresora, agregar un
    modelo, laminar) sigue solo cuando pasa. Una parte que no está en pantalla abre su panel; si no hay, la burbuja va
    al centro. Lo visto se guarda en las preferencias (`tutorials`). `--tutorial ID[,PASO]` abre uno para las capturas.
  - **Ficha del material:** en la pestaña Material, el material del filamento elegido (por su nombre: «Generic ABS»
    es ABS) con temperaturas, cabina, ventilador, superficies, retracción y secado; lo que la impresora no tiene para
    ese material (cabina, boquilla endurecida, cama u hotend que no llegan, secado) se avisa ahí y en la barra de
    estado.

- **Asistente:** se abre solo la primera vez (sin preferencias guardadas), y desde **Más > Tutorial**. Siete pasos
  abajo de la vista: abrir un modelo, elegir impresora, filamento y perfil, los ajustes, las herramientas de la placa,
  laminar y revisar, imprimir. **Atrás**, **Siguiente** y **Saltar**.
- **Consejos:** la primera vez que se usa Mover, Rotar, Escalar, Apoyar en una cara, Organizar, Pintar, el laminado,
  la Vista previa, Análisis, Calibración o Impresora, una tarjeta abajo de la vista explica cómo se usa. **Entendido**
  no la muestra más; **No mostrar más consejos** los apaga. En **Más**: **Consejos al usar cada función** (prenderlos o
  apagarlos) y **Ver los consejos de nuevo**. Lo visto se guarda en las preferencias (`tips`, `tips_seen`).
- **Botón (i):** al lado del nombre de cada ajuste del motor. Un clic abre qué hace, su clave, su valor por defecto y
  su rango. El texto sale de `docs/CONFIG.md`, que el programa lleva adentro (`src/ui/help.*`): la ayuda dice siempre
  lo mismo que la documentación. En inglés, el texto sigue en español y lo avisa. La prueba «help: every setting the
  engine writes has its explanation in docs/CONFIG.md» falla si un ajuste nuevo no tiene su fila.

## Capturas y pruebas automáticas

`tisma` puede dibujarse sin nadie frente a la pantalla, para las pruebas y para mostrar cambios:

```sh
tisma modelo.stl --screenshot vista.png --size 1440x900 --view preview --layer 120
tisma modelo.stl --screenshot vista.png --view preview --export pieza.gcode
tisma modelo.stl --screenshot vista.png --render rt --ray-samples 64
tisma modelo.stl --screenshot vista.png --ray-check
tisma --screenshot explorador.png --browse /una/carpeta
tisma modelo.stl --profile filament=PETG --profile "print=0.28 mm" --screenshot vista.png --save-project p.tisma
```

Varios STL en la línea de comandos arman una placa: el primero la abre y los demás se suman. `--tool
move|rotate|scale|lay|paint` abre esa herramienta, y `--look X,Y,Z,DISTANCIA,GIRO,INCLINACIÓN` pone la cámara.
`--paint canal,marca,X,Y,Z,RADIO` pinta (`docs/PAINTING.md`). `--level basic|normal|advanced|custom` elige el nivel del
panel, `--search TEXTO` va al primer ajuste que encuentra, `--single-column` muestra todo junto y `--choose-settings`
abre «Elegir ajustes…»; `--credits` abre Créditos y fuentes. `--ortho` y `--labels` hacen lo de K y E. `--section printer/SECCIÓN` abre la configuración de la impresora en esa sección
(`machine`, `nozzle`, `limits`, `gcode`, `calibration`, `connection`, `notes`), y `--profile "printer=Marca Modelo 0.4 mm"`
elige también un modelo del catálogo con esa boquilla, solo para esa vez. Las capturas no muestran la ayuda salvo que se pida:
`--tips` (los consejos), `--tour PASO` (el asistente, de 0 a 6) y `--info grupo.clave` (la tarjeta de un ajuste).

`--calibration dimensions|first_layer|temperature|flow|retraction|retraction_speed|pressure_advance|speed|fit|max_flow|acceleration|elephant_foot` crea una
prueba de calibración como su botón **Crear** (`extruder` abre la de los pasos del extrusor), y
`--view calibration` muestra su pestaña. `--color kind|height|width|speed|actual_speed|fan|temperature|flow|actual_flow|layer_time|layer_time_log|tool|color_print`
elige el color de la vista previa (por alto
de capa: la altura por zona, `docs/LOCAL_LAYERS.md`). `--render fast|ao|rt`, `--msaa N`, `--fxaa on|off` y `--moire on|off` eligen la imagen. `--ray-check` compara los rayos de la GPU con
los de la CPU y termina con 1 si no coinciden. `--moire-check` dibuja la vista previa desde el doble de lejos, sin
multimuestreo, con y sin el filtro de moiré, y termina con 1 si donde el filtro actúa los píxeles vecinos no se
parecen más entre sí (al menos un 15 % menos de diferencia). Con `TISMA_DEBUG_VISIBILITY=1` la vista pinta la oclusión y las
sombras en lugar de la luz, para revisarlas.

- `--screenshot` espera a que esté listo lo pedido: el laminado para `--view preview`, la estimación para
  `--view analysis`. Después escribe la ventana como PNG y termina.
- `--export` además exporta el G-code verificado, como el botón. Si eso falla, el programa termina con el código 1.
- `--profile printer|filament|print=NOMBRE` elige un perfil al empezar; `--save-project` guarda el proyecto antes de
  la captura. Las capturas no leen los perfiles del usuario, así salen iguales en cualquier máquina.
- En Linux sin pantalla corre bajo `xvfb-run`.
- CTest corre así estas pruebas, que se registran solo si hay `xvfb-run`:
  - `ui_slice_export_screenshot`: abre el cubo de prueba, lo lamina, exporta y dibuja la vista previa;
  - `ui_raytrace_matches_cpu`: los rayos de la GPU contra los de la CPU;
  - `ui_moire_filter_smooths_fine_lines`: la vista previa de lejos, con y sin el filtro de moiré;
  - `ui_device_prints_to_simulator`: el cubo laminado e impreso en la impresora simulada desde la pestaña Impresora
    (`docs/DEVICE.md`);
  - `ui_project_save`, `ui_project_reopen` y `ui_project_same_gcode`: el cubo con los perfiles PETG y 0.28 mm se
    exporta y se guarda como proyecto; el proyecto se abre de nuevo y se exporta; los dos G-code son idénticos;
  - `ui_plate_save`, `ui_plate_reopen` y `ui_plate_same_gcode`: lo mismo con una placa de dos cubos, ordenada, con la
    herramienta Escalar abierta.
  - `ui_help_tour` y `ui_help_tip_and_info`: el cuarto paso del asistente; el consejo de Pintar con la tarjeta de
    `walls.seam` abierta;
  - `ui_paint_save`, `ui_paint_reopen` y `ui_paint_same_gcode`: una costura pintada en un cubo (`--paint`), exportada,
    guardada en el proyecto y vuelta a abrir, da el mismo G-code.

## Cómo está hecha

| Parte | Dónde | Qué hace |
|---|---|---|
| Lógica sin OpenGL | `src/ui/` (biblioteca `tisma_ui`) | Cámara orbital, datos de dibujo (cama, malla, cordones de la vista previa), íconos SVG a imagen, PNG, textos en dos idiomas, listado de carpetas, el trabajo de laminado en segundo plano, los perfiles de fábrica. Se prueba con el motor (`tests/test_ui.cpp`). Los perfiles y los proyectos son del motor (`src/config/profiles.*`, `src/project/`). |
| Programa | `apps/tisma/` | Ventana (GLFW), paneles (Dear ImGui), cargador propio de OpenGL 3.3 (`gl.hpp`), dibujo 3D (`renderer.*`), explorador de archivos propio, recursos incrustados. |
| Recursos | `assets/brand/`, `cmake/embed.cmake` | Los íconos de Tisma y la fuente Roboto Medium van dentro del programa, que corre desde cualquier carpeta. |

- **Dibujo de la vista previa:** cada extrusión es una instancia de 56 bytes (extremos, ancho, alto, avance, tipo,
  capa y las uniones). El shader de vértices arma un prisma de sección rómbica:
  - donde el cordón sigue en el movimiento siguiente, los dos terminan en el plano de la bisectriz (inglete) y
    comparten las esquinas, sin puntas que sobresalgan ni caras encimadas que titilen;
  - un extremo libre (la costura, un giro de más de 120°) crece medio ancho, como el cordón después de la boquilla;
  - la luz sigue el perfil de un cordón impreso, calculado por píxel: plano arriba y abajo, redondo a los costados.

  Los datos son contiguos y quedan en la GPU; mostrar otra capa no los vuelve a subir. `--look X,Y,Z,DISTANCIA,GIRO,
  INCLINACIÓN` pone la cámara de la captura cerca de las líneas.
- **Un cuadro de la vista 3D, todo fuera de pantalla y del tamaño de la vista:**
  1. normales y profundidad de la escena;
  2. cuánta luz principal y cuánta luz ambiente llega a cada píxel: oclusión en pantalla o rayos;
  3. la escena iluminada, con multimuestreo;
  4. resolución, FXAA y copia a la ventana.
- **Cuando nada se mueve,** la ventana espera eventos en lugar de dibujar 60 cuadros por segundo.
- **Íconos:** se dibujan blancos y la interfaz los tiñe con el color del texto del tema, como pide
  `assets/brand/README.md`.
- **Sin GTK ni Qt:** el explorador de archivos es propio, dibujado con Dear ImGui, porque esas bibliotecas son LGPL y
  la licencia del motor tiene que poder cerrarse (`CLEANROOM.md`).
- **Bibliotecas y licencias:** en `THIRD_PARTY.md`.
- **Procedencia de cada parte:** en `PROVENANCE.md`.
