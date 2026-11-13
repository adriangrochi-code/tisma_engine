# Preferencias

**Qué hace:** las opciones del programa que no son perfiles: cómo se ve, qué pregunta, qué recuerda. Se guardan en
`interface.json` (`docs/UI.md`) y no cambian el G-code, salvo donde se dice.

**Fase:** P1 (interfaz).

**Fuentes:**
- las capturas de Tisma Slicer en `interface/` (`70_preferences_general.png`, `71_preferences_gui.png` y
  `72_preferences_other.png`), tomadas el 24-10-2026 bajo Xvfb;
- la página Camera no se capturó: sus opciones salen de lo que muestra la interfaz de Tisma Slicer y se confirman al
  programarla.

**Dónde** (pedido del dueño, 02-11-2026: «en el menú que hay, los tres puntos, no hay una opción específica de
configuración»): la primera entrada del menú **Más** (los tres puntos de la barra de arriba) es **Configuración…**
(Ctrl+, y también Ctrl+P), con un ícono de engranaje; también la abre un engranaje en la barra de estado, al lado del
tiempo de la vista. Un diálogo con las páginas a la izquierda (General, Interfaz, **Video**, Cámara, Asistente,
Complementos, Otros) y, a la derecha, cada opción con un interruptor violeta, una lista o un deslizador
(`interface.md`, «Diálogos»). Abajo, Cancelar y Aceptar.

## Lo que tiene Tisma Slicer

| Página | Opción | Por defecto | Qué hace en el motor |
|---|---|---|---|
| General | Remember output directory | sí | Exportar abre la última carpeta usada |
| General | Background processing | no | Es «Slice automatically» del panel (`interface.md`); se guarda aquí |
| General | Alert when supports needed | sí | Si una pieza tiene voladizos y los soportes están apagados, la revisión previa avisa (`print_review.md`) |
| General | Export sources full pathnames to 3mf and amf | no | El proyecto guarda la ruta completa del archivo de origen (para «Recargar desde el disco») |
| General | Update built-in Presets automatically | sí | Buscar y bajar actualizaciones del paquete de perfiles (`profile_pack.md`) |
| General | Suppress "- default -" presets | sí | No mostrar el perfil vacío de fábrica cuando hay otros |
| General | Suppress "Template" filament presets | no | No mostrar los filamentos genéricos (plantillas) |
| General | Show incompatible print and filament presets | no | Mostrar también los perfiles que no son compatibles, marcados |
| General | Show load project dialog | sí | Al abrir un proyecto, preguntar si se cargan piezas, ajustes o todo |
| General | Structural analysis on the graphics card | sí | Ingeniería usa la GPU (`compute.gpu`) |
| General | Allow just a single instance | no | Abrir un archivo con el programa ya abierto lo manda a esa ventana |
| General | Ask for unsaved changes in project | sí | Preguntar antes de cerrar o abrir otro proyecto |
| General | Ask to save unsaved changes in presets when closing or loading a new project | sí | Ídem para los perfiles (`presets.md`) |
| General | Ask for unsaved changes in presets when selecting new preset | sí | Ídem al cambiar de perfil |
| General | Ask for unsaved changes in presets when creating new project | sí | Ídem al empezar un proyecto |
| General | Show splash screen | sí | Pantalla de bienvenida al abrir |
| General | Restore window position on start | sí | La ventana vuelve a su lugar y tamaño |
| General | Clear Undo / Redo stack on new project | sí | Un proyecto nuevo empieza con el deshacer vacío (`model_tools.md`, regla 29) |
| GUI | Appearance | Like the system | Like the system, Light o Dark |
| GUI | Sequential slider applied only to top layer | sí | El deslizador de movimientos recorre solo la capa de arriba (`preview.md`) |
| GUI | Show sidebar collapse/expand button | no | El botón «»» que esconde el panel |
| GUI | Use colors for axes values in Manipulation panel | no | Los campos X, Y y Z en rojo, verde y azul |
| GUI | Order object volumes by types | sí | En la lista de objetos, partes primero, después volúmenes negativos y modificadores |
| GUI | Show non-manifold edges | sí | Marcar en la vista los bordes abiertos de una malla |
| GUI | Allow automatically color change | sí | Al abrir un modelo de varios colores, ofrecer los cambios de color por capa |
| GUI | Notify about new releases | All | All, solo las estables, o ninguna |
| GUI | Use custom size for toolbar icons | no | Tamaño propio de los íconos de la barra |
| GUI | Layout Options | Old regular layout with the tab bar | En el motor no aplica: la ventana tiene una sola disposición (`interface.md`) |
| GUI | Text colors | negro y violeta | Color del valor igual al del sistema y del valor cambiado |
| GUI | Mode markers | verde, amarillo, rojo | Colores de los niveles (`settings_panel.md`, regla 9) |
| Other | Use binary G-code when the printer supports it | sí | Exportar en binario si la impresora lo acepta (`gcode_extras.md`) |
| Other | Suppress to open hyperlink in browser | no | No abrir enlaces de la ayuda en el navegador |
| Other | Show STEP file import parameters | sí | Al abrir un STEP, preguntar la precisión de la conversión (`formats.md`, regla 2) |
| Other | Show "Log in" button in application top bar | sí | Botón de la cuenta de Tisma Cloud en la barra superior |
| Other | Allow downloads from supported websites | sí | Abrir los modelos que se bajan desde sitios con enlace al programa |
| Other | Download path | la carpeta del usuario | Dónde se guardan esas descargas |
| Other | Application font size | 10 | Tamaño de la letra de toda la interfaz |
| Camera | Use perspective camera | sí | Perspectiva u ortogonal (`preview.md`, regla 10) |
| Camera | Use free camera | no | Girar libre, sin mantener Z hacia arriba |
| Camera | Reverse direction of zoom with mouse wheel | no | Invierte la rueda |

## Reglas

1. **Todas las opciones de la tabla existen en el motor**, con los mismos valores por defecto, salvo:
   - «Layout Options», que no aplica;
   - «Background processing», que es el interruptor del panel y solo se refleja aquí.
2. **Las del motor se suman** en las páginas que corresponden:

| Página | Opción | Por defecto | Ficha |
|---|---|---|---|
| General | Buscar impresoras en la red | sí | `settings_panel.md`, regla 41 |
| General | Idioma | el del sistema | `interface.md` |
| GUI | Nivel de ajustes y vista propia | Normal | `settings_panel.md`, reglas 9 a 20 |
| GUI | Calidad de la vista 3D | la del motor | `docs/UI.md` (modos de dibujo) |
| Asistente | Tutoriales, glosario junto a los ajustes, fichas de materiales | sí | `assistant.md` |
| Complementos | Correr complementos al laminar | sí | `plugins.md` |

3. **Buscador:** arriba del diálogo, un campo que filtra las opciones por nombre en todas las páginas.
4. **OK guarda y aplica** sin reiniciar. Las que necesitan reiniciar (idioma, tamaño de letra) lo dicen al lado y
   ofrecen «Reiniciar ahora».
5. **Restablecer:** al pie, «Volver a los valores de fábrica», con confirmación. No toca perfiles ni proyectos.
6. **Exportar e importar** las preferencias como un JSON (`format` `tisma-preferences`, `version` 1), para llevarlas a
   otra computadora. No incluye secretos (contraseñas, códigos de acceso).
7. **Nada queda solo a mano** (pedido del dueño, 02-11-2026: «muchas cosas quedan, pero no puede ser manual»): toda
   opción que el programa guarda en sus preferencias y toda opción de la vista que hoy solo se cambia desde la línea de
   comandos (la API gráfica, el modo de imagen, el suavizado, el filtro de moiré) tiene su control en Configuración.
   La línea de comandos sigue existiendo para las pruebas y las capturas. Una prueba (PF7) recorre las claves de las
   preferencias y falla si alguna no tiene control.
8. **Página Video**, como la de ajustes de video de un juego. Arriba, el resumen del equipo (procesador, memoria,
   placa, controlador, versión de OpenGL o Vulkan, pantalla) de `view_auto_quality.md`, regla 1, con los botones
   **Medir de nuevo** y **Volver a lo recomendado**. Debajo, en este orden:

   | Opción | Clave | Valores | Por defecto | Qué hace |
   |---|---|---|---|---|
   | API gráfica | `ui.graphics_api` | Automática, Vulkan, OpenGL | Automática | Con qué dibuja la ventana (`docs/UI.md`, «Vulkan»); las que la máquina no tiene se ven apagadas con el motivo |
   | Placa de video | `ui.gpu_device` | Automática, y cada placa que se encuentre | Automática | En equipos con dos placas, cuál dibuja la vista; Automática pide la de alto rendimiento. Con OpenGL en Windows la elección final la hace Windows: al lado, el enlace a la configuración de gráficos de Windows |
   | Nivel de OpenGL | `ui.render_level` | Automático, 3.3, 3.0, 2.1, 1.1 | Automático | `legacy_windows.md`, reglas 5 a 8 |
   | Calidad | `ui.view_quality` | Automática, Alta, Equilibrada, Liviana, Personalizada | Automática | `view_auto_quality.md`, reglas 3 y 4; tocar cualquier opción de abajo pasa a Personalizada |
   | Modo de imagen | `ui.render_mode` | Rápida, Oclusión ambiental, Ray tracing | según la calidad | Los modos de `docs/UI.md`, «Imagen» |
   | Intensidad de la oclusión | `ui.ao_strength` | 0 a 1,5 | 1 | Cuánto oscurece la oclusión ambiental |
   | Sombra sobre la cama | `ui.bed_shadow` | sí / no | sí | La sombra de la pieza sobre la cama |
   | Luz de sol | `ui.sun` | sí / no | sí | Una sola luz desde una dirección, como el sol, con sus sombras sobre la pieza y la cama (regla 8 bis) |
   | Sol: dirección | `ui.sun_azimuth` | 0° a 359° | 135° | Desde dónde viene, alrededor de la cama (0° = desde +X, antihorario mirando desde arriba) |
   | Sol: altura | `ui.sun_elevation` | 5° a 90° | 50° | Cuán alto está sobre la cama (90° = justo arriba) |
   | Sol: sigue a la cámara | `ui.sun_follows_camera` | sí / no | no | Sí: el sol gira con la vista y la pieza se ve siempre iluminada igual; no: queda fijo respecto de la cama |
   | Suavizado (multimuestreo) | `ui.msaa` | No, 2×, 4×, 8× | según la calidad | Solo los que la placa admite |
   | Suavizado FXAA | `ui.fxaa` | sí / no | sí | |
   | Filtro de moiré | `ui.moire_filter` | sí / no | sí | `docs/UI.md`, «Filtro de moiré» |
   | Resolución de la vista | `ui.render_scale` | 50 % a 100 % | 100 % | Dibuja la vista 3D más chica y la agranda; los paneles y el texto no cambian |
   | Bajar la calidad al mover | `ui.lower_while_moving` | sí / no | sí | `view_auto_quality.md`, regla 5 |
   | Tope de cuadros por segundo | `ui.max_fps` | 30, 60, 75, 120, 144, la pantalla, sin tope | 60 | `view_auto_quality.md`, regla 3 |
   | Sincronización vertical | `ui.vsync` | sí / no | sí | Espera a la pantalla para no cortar la imagen |
   | Ahorro a batería | `ui.battery_saver` | sí / no | sí | `view_auto_quality.md`, regla 7 |
   | Cálculo en la placa de video | `gpu` y `compute.gpu` | Automático, No | Automático | El laminado y el análisis usan la GPU si hay; No los deja en el procesador |
   | Mostrar el tiempo de la vista | `ui.show_frame_time` | sí / no | sí | «Vista 8,3 ms · 60 FPS» en la barra de estado |

   - **Luz de sol (regla 8 bis, pedido del dueño, 02-11-2026: «la oclusión ambiental que salga de un solo punto que
     rote, como si fuera el sol»):** la luz principal viene de un solo punto lejano, como el sol: las caras que lo
     miran se aclaran, las de atrás quedan en penumbra y la pieza tira una sombra nítida sobre sí misma y sobre la cama
     en esa dirección. La oclusión ambiental sigue, más suave, oscureciendo los rincones donde el aire no llega, y se
     tiñe hacia el lado contrario al sol. En la tira de la vista hay un ícono de sol: arrastrarlo de costado gira el
     sol alrededor de la cama, y arriba o abajo lo sube o lo baja; mientras se arrastra, una flecha amarilla en la
     vista muestra de dónde viene la luz, y el cambio se ve en vivo. Doble clic vuelve a 135° y 50°. Con «Rápida» o
     «Liviana» queda solo la luz del sol, sin sombras (la sombra cuesta un dibujo más de la escena);
   - **Costo a la vista:** al lado de cada opción que cambia el dibujo, lo que cuesta en esta máquina según la última
     medición («+3,2 ms»), así se ve qué conviene apagar (el caso de la oclusión ambiental que reportó el dueño);
   - **Vista previa en vivo:** el diálogo deja ver la vista 3D detrás; los cambios se ven al momento y el tiempo de
     la vista se actualiza;
   - **Vuelta atrás segura:** si un cambio de API, placa o nivel deja la vista en negro o la cierra, el programa
     pregunta «¿Se ve bien?» con **Mantener** y **Volver** y una cuenta de 15 segundos; sin respuesta, vuelve a lo
     anterior (como el cambio de resolución de Windows). Si se cerró, el próximo arranque usa lo anterior y lo dice;
   - **Cuándo se aplica:** todo se aplica al momento salvo la API gráfica y la placa, que reinician la vista (no el
     programa) y conservan el proyecto abierto.
9. **Aceptar con la calidad en Personalizada** guarda las opciones de Video tal cual; la calidad automática no las
   vuelve a tocar hasta que la persona elija Automática o «Volver a lo recomendado».

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| PF1 | Abrir el diálogo con un `interface.json` vacío | Cada opción con el valor por defecto de la tabla |
| PF2 | Appearance en Dark y OK | La interfaz pasa a oscuro sin reiniciar; al reabrir sigue oscura |
| PF3 | Apagar «Ask for unsaved changes in project», cambiar una pieza y cerrar | Se cierra sin preguntar |
| PF4 | Buscar «binary» | Queda solo «Use binary G-code when the printer supports it» |
| PF5 | Exportar e importar en otra carpeta de datos | Las mismas opciones; sin códigos de acceso en el archivo |
| PF6 | «Volver a los valores de fábrica» | Todo como en PF1; los perfiles del usuario siguen |
| PF7 | Recorrer todas las claves de `interface.json` y las opciones de la vista de la línea de comandos | Cada una tiene su control en Configuración; la prueba nombra las que falten |
| PF8 | Abrir Más | La primera entrada es «Configuración…» con su engranaje; Ctrl+, y Ctrl+P abren el mismo diálogo |
| PF9 | Video: cambiar Modo de imagen de Oclusión ambiental a Rápida con el diálogo abierto | La vista cambia al momento, la calidad pasa a Personalizada y el tiempo de la vista baja en la barra de estado |
| PF10 | Video: elegir OpenGL 1.1 en una máquina con 3.3 y no contestar | Se ve el nivel simple; a los 15 s vuelve a 3.3 sin cerrar el programa |
| PF11 | Video: «Volver a lo recomendado» | Calidad Automática y el resto de Video como lo elige `view_auto_quality.md` para esta máquina |
| PF12 | Video en una máquina sin Vulkan | API gráfica muestra Vulkan apagado con «No hay Vulkan en este equipo» |
| PF13 | Cubo en la cama, Oclusión ambiental, sol a 90° de dirección y 30° de altura | Sombra larga del cubo sobre la cama hacia −Y; la cara que mira a +Y, clara; la de −Y, en penumbra |
| PF14 | Arrastrar el ícono de sol de costado | La sombra gira alrededor del cubo en vivo; al soltar, `ui.sun_azimuth` guarda la nueva dirección |
| PF15 | «Sigue a la cámara» prendido y girar la vista | La pieza se ve iluminada igual desde cualquier lado; apagado, la sombra queda fija en la cama mientras gira la vista |
