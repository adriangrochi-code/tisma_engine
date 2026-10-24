# Fichas de funciones

Una ficha por función de Tisma Slicer, con las reglas de «Fichas» de `CLEANROOM.md`: qué hace, ajustes, reglas,
fórmulas con su fuente, casos límite y pruebas como datos. Sin código ni nombres internos.

Los ajustes usan los nombres del motor (`docs/CONFIG.md`) o propuestas con su mismo formato (ver `SETTINGS_V1.md`).

| Ficha | Fase | Estado |
|---|---|---|
| [`slicing.md`](slicing.md): corte en capas, cierre de grietas, compensaciones en XY, modos de relleno de la malla | F1 | Para revisar |
| [`perimeters.md`](perimeters.md): cantidad, ancho y separación de los cordones, paredes finas (tabla de 0,1 a 3 mm), orden, costura | F2 | Para revisar |
| [`surfaces_infill.md`](surfaces_infill.md): capas sólidas, capa de arriba, `monotonic`, puentes sobre el aire y sobre el relleno, relleno poco denso (`rectilinear`, `grid`, `gyroid`), borde del relleno, zonas chicas | F3 | Para revisar |
| [`gcode_motion.md`](gcode_motion.md): filamento por tramo, velocidades por tipo, perímetros chicos, viajes, retracción, cambio de capa | F4 | Para revisar |
| [`cooling.md`](cooling.md): tiempo mínimo de capa con frenado debajo de la velocidad mínima, qué no se frena, ventilador | F4 | Para revisar |
| [`supports_skirt.md`](supports_skirt.md): qué lleva soporte (umbral automático y fijo), zona, separaciones, interfaz, base, falda y borde | F5 | Para revisar |
| [`coasting.md`](coasting.md): el final de cada trazo sin extruir | después de F4 | Para revisar |
| [`ironing.md`](ironing.md): planchado de las superficies de arriba | después de F3 | Para revisar |
| [`arc_overhangs.md`](arc_overhangs.md): voladizos sin soporte con arcos concéntricos | después de F5 | Para revisar |
| [`staggered_perimeters.md`](staggered_perimeters.md): perímetros internos alternados media capa más arriba, con la nota de la patente | después de F2 | Para revisar |
| [`interlock_infill.md`](interlock_infill.md): relleno sobre ondas en Z que se encajan entre capas | después de F3 | Para revisar |
| [`dense_infill.md`](dense_infill.md): relleno más denso solo bajo las superficies de arriba | después de F3 | Para revisar |
| [`structural_analysis.md`](structural_analysis.md): análisis estructural por vóxeles, material con capas más débiles, criterio de rotura, temperatura, paredes y relleno, tabla de materiales, validación con teoría y CalculiX | F7 | Para revisar |
| [`structural_optimization.md`](structural_optimization.md): relleno mínimo uniforme y por zonas, nervios a lo largo de las tensiones, refuerzos, lattice, orientación recomendada | F7 | Para revisar |
| [`aerodynamics.md`](aerodynamics.md): arrastre por presión (lattice Boltzmann) y rozamiento con la rugosidad de las capas | F7 | Para revisar |
| [`nonplanar.md`](nonplanar.md): capas curvas (onda, crestas, cono), rampa, ajuste a la cabeza, material según el espesor local | F7 | Para revisar |
| [`nonplanar_modes.md`](nonplanar_modes.md): capas curvas según la pieza: cono según los voladizos, solo bajo los voladizos, paralelas a las caras de arriba, automático y según las tensiones | F7 | Para revisar |
| [`z_antialiasing.md`](z_antialiasing.md): las extrusiones de arriba bajan hasta la superficie inclinada del modelo | después de F4 | Para revisar |
| [`belt_printer.md`](belt_printer.md): laminado para impresoras de cinta con pórtico inclinado y G-code en los ejes de la máquina | después de la primera versión | Para revisar |
| [`print_review.md`](print_review.md): avisos de lo que probablemente va a fallar, con sus capas | después de F4 | Para revisar |
| [`calibrations.md`](calibrations.md): pruebas por franjas y por parches, comandos por firmware, bloque de precisión de medidas | después de F4 | Para revisar |
| [`printer_connections.md`](printer_connections.md): USB, Bambu Lab en red local, interfaces web, botón de enviar | F9 | Para revisar |
| [`resin.md`](resin.md): imágenes de capa, exposición y movimientos, formato GOO de Elegoo, detector de ventosas, torre de exposición y bloque de medidas | después de la primera versión | Para revisar |
| [`orientation.md`](orientation.md): orientación automática, menos soporte en FDM y menor fuerza de despegue en resina | después de la primera versión | Para revisar |
| [`fragment.md`](fragment.md): dividir un objeto que no entra, con cortes en las secciones chicas y encastres (pernos o espigas) | después de la primera versión | Para revisar |

**Pendientes:**
- Resina: soportes, pad y vaciado (desde la documentación de usuario de Prusa); orientación automática.
- Análisis de estabilidad propio para la revisión antes de imprimir.
- Interfaz.
