# Muestras para `project_import.md`

Archivos guardados por cada programa usado como caja negra, de piezas propias (cajas generadas, sin modelos de
terceros). Libres para las pruebas del motor.

| Archivo | Programa y versión | Cómo se hizo | Qué trae |
|---|---|---|---|
| `pi2_prusaslicer.3mf` | PrusaSlicer 2.7.2 (línea de comandos) | `--merge --export-3mf` con dos cajas (20 × 20 × 20 y 30 × 10 × 15 mm) | Dos objetos (corregido el 09-11: la primera versión traía uno solo), `Metadata/Slic3r_PE_model.config`; sin ajustes (la línea de comandos no guarda `Slic3r_PE.config`) |
| `pi4_prusaslicer_parts.3mf` | PrusaSlicer 2.7.2 (interfaz, sin red) | Cubo de 20 mm, perfiles «0.20mm SPEED», «Generic PETG», «Original Prusa MK4 Input Shaper 0.4 nozzle»; se agregó un bloqueador de soporte (caja), un modificador (cilindro) y un volumen negativo (esfera), modo Experto; Guardar proyecto | `Metadata/Slic3r_PE.config` con todos los ajustes; partes `ModelPart`, `ParameterModifier`, `NegativeVolume`, `SupportBlocker` |

Faltan, para PI2 a PI8: proyectos guardados desde la interfaz de cada programa (con modificador, placas, pintura y
ajustes). Se piden al dueño y a los testers.
