# Fichas de funciones

Una ficha por función de Tisma Slicer, con las reglas de «Fichas» de `CLEANROOM.md`: qué hace, ajustes, reglas,
fórmulas con su fuente, casos límite y pruebas como datos. Sin código ni nombres internos.

Los ajustes usan los nombres del motor (`docs/CONFIG.md`) o propuestas con su mismo formato (ver `SETTINGS_V1.md`).

| Ficha | Fase | Estado |
|---|---|---|
| [`slicing.md`](slicing.md): corte en capas, cierre de grietas, compensaciones en XY, modos de relleno de la malla | F1 | Para revisar |
| [`placement.md`](placement.md): ubicación en la cama: centrado, copias ordenadas, base en Z = 0, objetos fuera del volumen | F1 | Para revisar |
| [`perimeters.md`](perimeters.md): cantidad, ancho y separación de los cordones, paredes finas (tabla de 0,1 a 3 mm), orden, costura | F2 | Para revisar |
| [`surfaces_infill.md`](surfaces_infill.md): capas sólidas, capa de arriba, `monotonic`, puentes sobre el aire y sobre el relleno, relleno poco denso (`rectilinear`, `grid`, `gyroid`), borde del relleno, zonas chicas | F3 | Para revisar |
| [`gcode_motion.md`](gcode_motion.md): filamento por tramo, velocidades por tipo, perímetros chicos, viajes, retracción, cambio de capa, orden dentro de la capa | F4 | Para revisar |
| [`cooling.md`](cooling.md): tiempo mínimo de capa con frenado debajo de la velocidad mínima, qué no se frena, ventilador | F4 | Para revisar |
| [`gcode_firmware.md`](gcode_firmware.md): comandos según el firmware: temperaturas, límites de la máquina, etiquetas de objetos para cancelarlos, avance de la impresión | F4 | Para revisar |
| [`supports_skirt.md`](supports_skirt.md): qué lleva soporte (umbral automático y fijo), zona, separaciones, interfaz, base, falda y borde | F5 | Para revisar |
| [`coasting.md`](coasting.md): el final de cada trazo sin extruir | después de F4 | Para revisar |
| [`ironing.md`](ironing.md): planchado de las superficies de arriba | después de F3 | Para revisar |
| [`arc_overhangs.md`](arc_overhangs.md): voladizos sin soporte con arcos concéntricos | después de F5 | Para revisar |
| [`staggered_perimeters.md`](staggered_perimeters.md): perímetros internos alternados media capa más arriba, con la nota de la patente | después de F2 | Para revisar |
| [`interlock_infill.md`](interlock_infill.md): relleno sobre ondas en Z que se encajan entre capas | después de F3 | Para revisar |
| [`dense_infill.md`](dense_infill.md): relleno más denso solo bajo las superficies de arriba | después de F3 | Para revisar |
| [`pressure_advance_gcode.md`](pressure_advance_gcode.md): estudio: el adelanto del extrusor escrito en el G-code para firmwares sin pressure advance, con los límites en las uniones y la simulación | después de F4, opcional | Para revisar |
| [`multimaterial.md`](multimaterial.md): boquillas de distinto diámetro por extrusor (paredes finas, relleno grueso) y mezcla de colores por capas, con su costo de purga | después de la primera versión | Para revisar |
| [`structural_analysis.md`](structural_analysis.md): análisis estructural por vóxeles, material con capas más débiles, criterio de rotura, temperatura, paredes y relleno, tabla de materiales, validación con teoría y CalculiX | F7 | Para revisar |
| [`structural_optimization.md`](structural_optimization.md): relleno mínimo uniforme y por zonas, nervios a lo largo de las tensiones, refuerzos, lattice, orientación recomendada | F7 | Para revisar |
| [`aerodynamics.md`](aerodynamics.md): arrastre por presión (lattice Boltzmann) y rozamiento con la rugosidad de las capas | F7 | Para revisar |
| [`nonplanar.md`](nonplanar.md): capas curvas (onda, crestas, cono), rampa, ajuste a la cabeza, material según el espesor local | F7 | Para revisar |
| [`nonplanar_modes.md`](nonplanar_modes.md): capas curvas según la pieza: cono según los voladizos, solo bajo los voladizos, paralelas a las caras de arriba, automático y según las tensiones | F7 | Para revisar |
| [`z_antialiasing.md`](z_antialiasing.md): las extrusiones de arriba bajan hasta la superficie inclinada del modelo | después de F4 | Para revisar |
| [`belt_printer.md`](belt_printer.md): laminado para impresoras de cinta con pórtico inclinado y G-code en los ejes de la máquina | después de la primera versión | Para revisar |
| [`print_review.md`](print_review.md): avisos de lo que probablemente va a fallar, con sus capas | después de F4 | Para revisar |
| [`stability.md`](stability.md): diseño propio: parte en el aire, vuelco por el peso, flexión de partes esbeltas y despegue de la cama, con la calibración de la fuerza de la boquilla y de la adherencia | después de F4 | Para revisar |
| [`calibrations.md`](calibrations.md): pruebas por franjas y por parches, comandos por firmware, bloque de precisión de medidas | después de F4 | Para revisar |
| [`calibration_models.md`](calibration_models.md): las piezas de cada prueba de calibración, con medidas, rangos por defecto y ajustes | después de F4 | Para revisar |
| [`printer_connections.md`](printer_connections.md): USB, Bambu Lab en red local, interfaces web, botón de enviar | F9 | Para revisar |
| [`resin.md`](resin.md): imágenes de capa, exposición y movimientos, formato GOO de Elegoo, detector de ventosas, torre de exposición y bloque de medidas | después de la primera versión | Para revisar |
| [`resin_supports.md`](resin_supports.md): soportes, base y vaciado de resina: ajustes, alturas y medidas como caja negra; el algoritmo es propio del motor | después de la primera versión | Para revisar |
| [`orientation.md`](orientation.md): orientación automática, menos soporte en FDM y menor fuerza de despegue en resina | después de la primera versión | Para revisar |
| [`fragment.md`](fragment.md): dividir un objeto que no entra, con cortes en las secciones chicas y encastres (pernos o espigas) | después de la primera versión | Para revisar |
| [`interface.md`](interface.md): la ventana en orden (barra superior, grupos de herramientas, panel de la placa, botón principal, vista previa, páginas de ajustes, Más, asistente, panel de Ingeniería), todos los menús con sus atajos, el aspecto (colores claro y oscuro, medidas de cada componente) y 39 capturas en [`interface/`](interface/) | P1 | Para revisar |
| [`profile_pack.md`](profile_pack.md): el paquete libre de perfiles (902 modelos FDM y 156 de resina, 11 651 filamentos, 484 resinas): descarga en el primer arranque, comprobación, compatibles, variables del G-code de inicio, actualizaciones y datos de la comunidad | P3 | Para revisar |
| [`assistant.md`](assistant.md): asistente sin conexión (problemas con causas y arreglos aplicables, procedimientos, glosario junto a los ajustes, fichas y avisos de materiales) y tutoriales guiados con anclas y eventos de la interfaz | P1 | Para revisar |
| [`pellets.md`](pellets.md): impresoras de pellets: alimentación, eje E en largo o en volumen, sección redonda del cordón, flujo según la velocidad, topes para boquillas y capas grandes | P3 | Para revisar |
| [`waam.md`](waam.md): estudio de impresoras de metal por arco: arco en lugar de E, menos arranques, paso entre cordones, intersecciones, espera entre capas, sobremedida para mecanizar | después de la primera versión | Para revisar |
| [`profiles.md`](profiles.md): impresora por modelo, boquilla y cama aparte, temperatura de cama por tipo de cama, datos solo de fuentes oficiales | P3 | Para revisar |
| [`mobile_app.md`](mobile_app.md): la app de Android nueva, de cero sobre el motor: API en C del motor, pantallas como el escritorio, laminar en el teléfono, Ingeniería, impresoras de la red, nube | después de P1 | Para revisar |
| [`cloud.md`](cloud.md): contrato con Tisma Cloud: cuenta, laminar y calcular en la nube, informar problemas, el motor como puente de impresoras y como programa de los nodos | después de la primera versión | Para revisar |
| [`step_faces.md`](step_faces.md): STEP con caras: calidad al abrir, cara por triángulo, el STEP en el proyecto, pintar y elegir cargas por cara, volver a convertir sin perder lo pintado, revisar el sólido | después de la primera versión | Para revisar |
| [`problem_reports.md`](problem_reports.md): la pantalla de informar un problema: campos, qué se adjunta y se ve antes, mandar, respuestas del servidor, los informes propios | después de la primera versión | Para revisar |
| [`thingiverse.md`](thingiverse.md): Thingiverse: búsqueda propia con la API (por Tisma Cloud o con token propio), licencia a la vista y descarga a la placa; el sitio dentro del programa como respaldo; contrato de uso y confirmación con un token | después de la primera versión | Para revisar |
| [`preferences.md`](preferences.md): todas las preferencias del programa por página (General, GUI, Other, Camera), las propias del motor, buscador, exportar e importar | P1 | Para revisar |
| [`walls_extra.md`](walls_extra.md): paredes: ancho fijo o variable, costura, voladizos, paredes extra | después de F2 | Para revisar |
| [`infill_patterns.md`](infill_patterns.md): patrones de relleno y opciones del relleno que no están en `surfaces_infill.md` | después de F3 | Para revisar |
| [`retraction_travel.md`](retraction_travel.md): retracción y viajes: vuelta, cambio de capa, barrido, saltos en Z y rampa, viajes por dentro, valores del material | después de F4 | Para revisar |
| [`speeds_accels.md`](speeds_accels.md): velocidades y aceleraciones por tipo, voladizos, primera capa | después de F4 | Para revisar |
| [`cooling_more.md`](cooling_more.md): enfriamiento que no está en `cooling.md` | después de F4 | Para revisar |
| [`fuzzy_skin.md`](fuzzy_skin.md): piel difusa | después de F4 | Para revisar |
| [`vase_mode.md`](vase_mode.md): una sola pared en espiral | después de F4 | Para revisar |
| [`gcode_extras.md`](gcode_extras.md): G-code binario, miniaturas, comentarios y otros extras de la salida | después de F4 | Para revisar |
| [`skirt_brim_raft.md`](skirt_brim_raft.md): falda, borde y balsa más allá de `supports_skirt.md` | F5 | Para revisar |
| [`supports_more.md`](supports_more.md): soportes: forzados, bloqueadores, separaciones, patrones, interfaz, `snug`, orgánicos, ancho | después de F5 | Para revisar |
| [`gcode_templates.md`](gcode_templates.md): G-code propio con variables, condiciones y cuentas | después de F6 | Para revisar |
| [`multi_extruder.md`](multi_extruder.md): varios filamentos: cambio de herramienta, torre de purga, extrusor por función | después de F6 | Para revisar |
| [`sequential.md`](sequential.md): objeto por objeto, con la holgura de la cabeza | después de F6 | Para revisar |
| [`devices.md`](devices.md): página Devices y conexión con impresoras por marca, con capturas | F9 | Para revisar |
| [`presets.md`](presets.md): perfiles del sistema y de la persona: guardar, heredar, compatibilidad, comparar, paquetes, instantáneas, parámetros propios | P3 | Para revisar |
| [`formats.md`](formats.md): leer STL, OBJ, AMF, 3MF, STEP y SVG, reparación al abrir, el proyecto 3MF y qué guarda, exportar STL y OBJ | F1 (leer), P4 | Para revisar |
| [`print_time.md`](print_time.md): tiempo estimado: límites de la máquina por firmware, modos normal y silencioso, qué cuenta y qué no, cómo hacerlo más preciso | después de F4 | Para revisar |
| [`settings_levels.md`](settings_levels.md): niveles de ajustes, lista propia (Quick settings), ajustes por objeto, con capturas | P1 | Para revisar |
| [`model_tools.md`](model_tools.md): partes, volúmenes negativos, modificadores, rangos de alturas, ajustes por objeto y por parte, y cuál gana; escalar, girar, cortar y separar | F3, P4 | Para revisar |
| [`preview.md`](preview.md): vista previa: las 13 vistas, leyenda con tiempos y material, escala de colores, deslizadores, con capturas | P1 | Para revisar |
| [`settings_panel.md`](settings_panel.md): el panel de ajustes en secciones (Calidad, Resistencia, Velocidad…) con apartados finos plegables, niveles Básico, Normal y Avanzado, vista Personalizada, tarjeta de impresora, cama y boquilla con dibujos, configuración de la impresora aparte con su calibración, conexión por red desde un ícono, boquilla con un clic y ventana de la cámara | P1 | Para revisar |
| [`support_fins.md`](support_fins.md): aletas de soporte que se quitan con la mano, unidas con dientes de una capa, con escala por boquilla, bases, contrafuertes y resina (de Support Fins, MIT) | después de la primera versión | Para revisar |
| [`credits.md`](credits.md): «Basado en…» en cada función que sale de otra fuente y la página Créditos y fuentes, con las licencias de terceros | P1 | Para revisar |
| [`plugins.md`](plugins.md): complementos en cualquier lenguaje con un `plugin.json` y JSON por la entrada y la salida; momentos `mesh`, `layers`, `review`, `gcode` y acciones; ajustes que dibuja el motor; instalar soltando un archivo; fallas que no frenan el laminado | después de la primera versión | Para revisar |

**Pendientes:**
- Interfaz: Dispositivos, preferencias y la app de teléfono, en detalle.
- Perfiles: los datos de cada impresora y material, desde sus fabricantes (P3).
- Formatos: lo que guarda Tisma en el 3MF desde la interfaz (pintura, modificadores, varias camas, análisis), y
  los proyectos de Bambu Studio y OrcaSlicer.
