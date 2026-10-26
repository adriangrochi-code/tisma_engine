# Preferencias

**Qué hace:** las opciones del programa que no son perfiles: cómo se ve, qué pregunta, qué recuerda. Se guardan en
`interface.json` (`docs/UI.md`) y no cambian el G-code, salvo donde se dice.

**Fase:** P1 (interfaz).

**Fuentes:**
- las capturas de Tisma Slicer en `interface/` (`70_preferences_general.png`, `71_preferences_gui.png` y
  `72_preferences_other.png`), tomadas el 24-10-2026 bajo Xvfb;
- la página Camera no se capturó: sus opciones salen de lo que muestra la interfaz de Tisma Slicer y se confirman al
  programarla.

**Dónde:** Más › Tisma › Preferences… (Ctrl+P). Un diálogo con las páginas a la izquierda y, a la derecha, cada opción
con un interruptor violeta o una lista (`interface.md`, «Diálogos»). Abajo, Cancel y OK.

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

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| PF1 | Abrir el diálogo con un `interface.json` vacío | Cada opción con el valor por defecto de la tabla |
| PF2 | Appearance en Dark y OK | La interfaz pasa a oscuro sin reiniciar; al reabrir sigue oscura |
| PF3 | Apagar «Ask for unsaved changes in project», cambiar una pieza y cerrar | Se cierra sin preguntar |
| PF4 | Buscar «binary» | Queda solo «Use binary G-code when the printer supports it» |
| PF5 | Exportar e importar en otra carpeta de datos | Las mismas opciones; sin códigos de acceso en el archivo |
| PF6 | «Volver a los valores de fábrica» | Todo como en PF1; los perfiles del usuario siguen |
