# Fichas de funciones

Una ficha por función de Tisma Slicer, con las reglas de «Fichas» de `CLEANROOM.md`: qué hace, ajustes, reglas,
fórmulas con su fuente, casos límite y pruebas como datos. Sin código ni nombres internos.

| Ficha | Fase | Estado |
|---|---|---|
| [`slicing.md`](slicing.md): corte en capas, cierre de grietas, compensaciones en XY, modos de relleno de la malla | F1 | Para revisar |
| [`perimeters.md`](perimeters.md): cantidad, ancho y separación de los cordones, paredes finas (tabla de 0,1 a 3 mm), orden, costura | F2 | Para revisar |
| [`surfaces_infill.md`](surfaces_infill.md): capas sólidas, capa de arriba, `monotonic`, puentes sobre el aire y sobre el relleno, relleno poco denso (`rectilinear`, `grid`, `gyroid`), borde del relleno, zonas chicas | F3 | Para revisar |
| [`gcode_motion.md`](gcode_motion.md): filamento por tramo, velocidades por tipo, perímetros chicos, viajes, retracción, cambio de capa | F4 | Para revisar |
| [`cooling.md`](cooling.md): tiempo mínimo de capa con frenado debajo de la velocidad mínima, qué no se frena, ventilador | F4 | Para revisar |
