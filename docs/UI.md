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
| Panel izquierdo | Los ajustes, en tres secciones con sus íconos, cada una con su fila de perfil arriba (ver «Perfiles y proyectos»): **Impresión** (capas —con la altura por zona, la altura variable por Z (escalón en pendientes y alturas por Z, `docs/VARIABLE_LAYERS.md`) y Z anti-aliasing (espesor mínimo y subida, `docs/ZAA.md`)—, paredes, superficies, relleno (con combinar capas, denso bajo el techo y guiado por tensiones con su caso de carga, `docs/INFILL.md`), soportes, falda y borde, velocidades), **Filamento** (diámetro, temperaturas, flujo, ventilador) e **Impresora** (cama, altura, boquilla, capa mínima y máxima, dialecto, retracción, G-code de inicio y fin). Abajo: cargar, guardar y restablecer todos los ajustes como JSON. |
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
| Herramientas de la placa | M mover, R rotar, S escalar, F apoyar en una cara, A organizar, Supr quitar, Esc ninguna |

### Preparar

Al abrir un STL, el modelo queda centrado en la cama y apoyado en ella (Z = 0). Cambiar el tamaño de la cama lo vuelve
a centrar. **Laminar** corre el motor en segundo plano y la ventana sigue respondiendo.

#### Herramientas de la placa

La placa puede tener varios modelos (pedido del dueño, 2026-10-25; los grupos de la ficha `interface.md`). A la
izquierda de la vista 3D, una columna con:

| Herramienta | Qué hace |
|---|---|
| **Agregar** | Suma un STL a la placa y la ordena. Soltar un STL sobre una placa con modelos también lo suma; **Abrir** empieza una placa nueva |
| **Quitar**, **Duplicar** | El modelo elegido |
| **Mover** (M) | Su posición en X e Y (el centro de su caja). Arrastrar un modelo lo mueve con cualquier herramienta |
| **Rotar** (R) | Ángulos en X, Y y Z (R = Rz·Ry·Rx), ±90° por eje, **Sin giro** |
| **Escalar** (S) | En % o en mm por eje, los tres juntos o cada uno, **Tamaño original** |
| **Apoyar en una cara** (F) | Un clic en una cara del modelo la deja contra la cama |
| **Organizar** (A) | Todos los modelos en una grilla compacta, 6 mm entre cajas, centrada en la cama, sin girarlos (ficha `placement.md`, regla 2) |

Cada modelo queda siempre apoyado en la cama. El elegido se ve más claro. La lógica está en `src/ui/plate.*`, sin
OpenGL, con sus pruebas (`tests/test_plate.cpp`). Un proyecto guarda cada modelo ya ubicado (con su giro y su escala
aplicados), así se abre igual; ya no se queda solo con el primero. Pintar (soportes, costura, colores) viene después:
necesita que el motor lea lo pintado.

El laminado hace lo mismo que `tisma-engine slice`, sin escribir archivos:
- las comprobaciones de la malla y de la pieza laminada;
- el G-code en memoria;
- su verificación (`docs/GCODE_CHECK.md`).

Si la pieza no se puede imprimir (fuera de la cama, más alta que la máquina, más fina que las líneas), un cuadro
muestra el motivo que da el motor, en inglés como en la línea de comandos.

### Vista previa

Muestra el G-code tal como se imprime: es el mismo texto que se exporta, leído de nuevo
(`src/gcode/preview.hpp`). Cada extrusión se dibuja como un cordón de su ancho y su alto.
- **Barra de capas:** a la derecha; muestra hasta la capa elegida. «Solo esta capa» deja una sola.
- **Leyenda:** abajo a la izquierda.
  - **Color por:** tipo de línea, velocidad, ancho o alto de capa.
  - Cada tipo de línea se puede ocultar, y los viajes se pueden mostrar.
  - Los números: tiempo estimado, filamento (metros y cm³), capas y líneas del G-code.

**Abrir** un `.gcode` lo muestra igual, también si no lo escribió el motor: el lector entiende G-code sin los
comentarios del motor. Ese G-code no se puede volver a laminar ni exportar.

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
move|rotate|scale|lay` abre esa herramienta, y `--look X,Y,Z,DISTANCIA,GIRO,INCLINACIÓN` pone la cámara.

`--calibration dimensions|first_layer|temperature|flow|retraction|retraction_speed|pressure_advance|speed|fit|max_flow|acceleration|elephant_foot` crea una
prueba de calibración como su botón **Crear** (`extruder` abre la de los pasos del extrusor), y
`--view calibration` muestra su pestaña. `--color kind|speed|width|height` elige el color de la vista previa (por alto
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
