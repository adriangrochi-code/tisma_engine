# Fichas de funciones

Una ficha por función de Tisma Slicer, con las reglas de «Fichas» de `CLEANROOM.md`: qué hace, ajustes, reglas,
fórmulas con su fuente, casos límite y pruebas como datos. Sin código ni nombres internos.

Los ajustes usan los nombres del motor (`docs/CONFIG.md`) o propuestas con su mismo formato (ver `SETTINGS_V1.md`).

| Ficha | Fase | Estado |
|---|---|---|
| [`slicing.md`](slicing.md): corte en capas, cierre de grietas, compensaciones en XY, modos de relleno de la malla | F1 | Para revisar |
| [`perimeters.md`](perimeters.md): cantidad, ancho y separación de los cordones, paredes finas (tabla de 0,1 a 3 mm), orden, costura | F2 | Para revisar |
| [`surfaces_infill.md`](surfaces_infill.md): capas sólidas, capa de arriba, `monotonic`, puentes sobre el aire y sobre el relleno, relleno poco denso (`rectilinear`, `grid`, `gyroid`), borde del relleno, zonas chicas | F3 | Para revisar |
| [`gcode_motion.md`](gcode_motion.md): filamento por tramo, velocidades por tipo, perímetros chicos, viajes, retracción, cambio de capa, orden dentro de la capa | F4 | Para revisar |
| [`cooling.md`](cooling.md): tiempo mínimo de capa con frenado debajo de la velocidad mínima, qué no se frena, ventilador | F4 | Para revisar |
| [`supports_skirt.md`](supports_skirt.md): qué lleva soporte (umbral automático y fijo), zona, separaciones, interfaz, base, falda y borde | F5 | Para revisar |
| [`coasting.md`](coasting.md): el final de cada trazo sin extruir | después de F4 | Para revisar |
| [`ironing.md`](ironing.md): planchado de las superficies de arriba | después de F3 | Para revisar |
| [`arc_overhangs.md`](arc_overhangs.md): voladizos sin soporte con arcos concéntricos | después de F5 | Para revisar |
| [`staggered_perimeters.md`](staggered_perimeters.md): perímetros internos alternados media capa más arriba, con la nota de la patente | después de F2 | Para revisar |
| [`interlock_infill.md`](interlock_infill.md): relleno sobre ondas en Z que se encajan entre capas | después de F3 | Para revisar |
| [`dense_infill.md`](dense_infill.md): relleno más denso solo bajo las superficies de arriba | después de F3 | Para revisar |

**Pendientes:** Z anti-aliasing (la primera prueba con un techo de 15° no dio una medición clara; se repite con otra
pieza), capas no planares, análisis estructural y aerodinámico, revisión antes de imprimir, impresoras de cinta,
resina, interfaz y conexión con impresoras.
