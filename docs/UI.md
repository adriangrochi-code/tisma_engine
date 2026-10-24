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
tisma pieza.gcode              # para mirar un G-code
tisma ajustes.json modelo.stl  # con esos ajustes
```

### La ventana

| Lugar | Qué tiene |
|---|---|
| Barra superior | El logo; las pestañas **Preparar**, **Vista previa** y **Análisis**; los botones **Abrir**, **Laminar** (violeta), **Exportar G-code** y **Más** (idioma, tema claro u oscuro, acerca de, salir). |
| Panel izquierdo | Los ajustes, en tres secciones con sus íconos: **Impresión** (capas, paredes, superficies, relleno, soportes, falda y borde, velocidades), **Filamento** (diámetro, temperaturas, retracción, ventilador) e **Impresora** (cama, altura, boquilla, dialecto, G-code de inicio y fin). Abajo: cargar, guardar y restablecer. |
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
| Laminar | Ctrl+R |
| Exportar | Ctrl+E |
| Capas de la vista previa | Flechas arriba y abajo, Re Pág y Av Pág, Inicio y Fin, o la rueda sobre la barra de capas |

### Preparar

Al abrir un STL, el modelo queda centrado en la cama y apoyado en ella (Z = 0). Cambiar el tamaño de la cama lo vuelve
a centrar. **Laminar** corre el motor en segundo plano y la ventana sigue respondiendo.

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
  - **Color por:** tipo de línea, velocidad o ancho.
  - Cada tipo de línea se puede ocultar, y los viajes se pueden mostrar.
  - Los números: tiempo estimado, filamento (metros y cm³), capas y líneas del G-code.

**Abrir** un `.gcode` lo muestra igual, también si no lo escribió el motor: el lector entiende G-code sin los
comentarios del motor. Ese G-code no se puede volver a laminar ni exportar.

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

### Ajustes y preferencias

- **Guardar…** escribe los ajustes en el formato de `docs/CONFIG.md`, y **Cargar…** los lee. Un archivo `.json`
  también se puede abrir con **Abrir**.
- **Ajustes de la interfaz:** parten de una impresora genérica de 256 × 256 × 256 mm con Marlin. Su G-code de
  inicio calienta la cama y la boquilla con las variables `[bed_temperature]` y `[temperature]`, que se editan en
  Filamento.
- **Lo que se recuerda entre usos:** el idioma, el tema, la última carpeta y los ajustes. Se guardan en
  `%APPDATA%\Tisma\interface.json` (Windows) o `~/.config/tisma/interface.json` (Linux).
- **Idiomas:** español (por defecto) e inglés. Se elige en **Más** o con `--lang en`.

## Capturas y pruebas automáticas

`tisma` puede dibujarse sin nadie frente a la pantalla, para las pruebas y para mostrar cambios:

```sh
tisma modelo.stl --screenshot vista.png --size 1440x900 --view preview --layer 120
tisma modelo.stl --screenshot vista.png --view preview --export pieza.gcode
tisma --screenshot explorador.png --browse /una/carpeta
```

- `--screenshot` espera a que esté listo lo pedido: el laminado para `--view preview`, la estimación para
  `--view analysis`. Después escribe la ventana como PNG y termina.
- `--export` además exporta el G-code verificado, como el botón. Si eso falla, el programa termina con el código 1.
- En Linux sin pantalla corre bajo `xvfb-run`.
- CTest corre así la prueba `ui_slice_export_screenshot`: abre el cubo de prueba, lo lamina, exporta y dibuja la
  vista previa. Se registra solo si hay `xvfb-run`.

## Cómo está hecha

| Parte | Dónde | Qué hace |
|---|---|---|
| Lógica sin OpenGL | `src/ui/` (biblioteca `tisma_ui`) | Cámara orbital, datos de dibujo (cama, malla, cordones de la vista previa), íconos SVG a imagen, PNG, textos en dos idiomas, listado de carpetas, el trabajo de laminado en segundo plano. Se prueba con el motor (`tests/test_ui.cpp`). |
| Programa | `apps/tisma/` | Ventana (GLFW), paneles (Dear ImGui), cargador propio de OpenGL 3.3 (`gl.hpp`), dibujo 3D (`renderer.*`), explorador de archivos propio, recursos incrustados. |
| Recursos | `assets/brand/`, `cmake/embed.cmake` | Los íconos de Tisma y la fuente Roboto Medium van dentro del programa, que corre desde cualquier carpeta. |

- **Dibujo de la vista previa:** cada extrusión es una instancia de 40 bytes (extremos, ancho, alto, avance, tipo y
  capa). El shader de vértices arma un prisma de sección rómbica con normales de elipse. Los datos son contiguos y
  quedan en la GPU; mostrar otra capa no los vuelve a subir.
- **Íconos:** se dibujan blancos y la interfaz los tiñe con el color del texto del tema, como pide
  `assets/brand/README.md`.
- **Sin GTK ni Qt:** el explorador de archivos es propio, dibujado con Dear ImGui, porque esas bibliotecas son LGPL y
  la licencia del motor tiene que poder cerrarse (`CLEANROOM.md`).
- **Bibliotecas y licencias:** en `THIRD_PARTY.md`.
- **Procedencia de cada parte:** en `PROVENANCE.md`.
