# Especificaciones del motor

Estos documentos dicen **qué** tiene que hacer el motor y **cómo se integra** con Tisma. No dicen cómo está programado
otro slicer.

Los escribió el equipo del dueño, que conoce Tisma Slicer por dentro. Así funciona la sala limpia de dos equipos:
1. Quien conoce el programa original escribe especificaciones funcionales, sin código ni estructura copiada.
2. Quien programa el motor trabaja solo con estas especificaciones y con fuentes públicas.

| Documento | Contenido |
|---|---|
| `SOURCES.md` | Artículos, especificaciones de formatos y documentación pública que se pueden usar, y para qué sirve cada uno |
| `GCODE_OUTPUT.md` | El G-code que tiene que escribir el motor para que la vista previa de Tisma lo muestre |
| `SETTINGS_V1.md` | Los ajustes de la primera versión, con su significado y unidades |
| `BEHAVIOR.md` | Cómo tiene que comportarse el laminado: requisitos y lecciones medidas en Tisma |
| `BENCHMARKS.md` | Números de referencia de Tisma Slicer, medidos como caja negra, para comparar |
| `COVERAGE.md` | Todo lo que hace Tisma Slicer, función por función, con la ficha que lo cubre: el motor va a reemplazar a Tisma Slicer y no se puede perder nada |
| `INTEGRATION.md` | Cada parte de Tisma Slicer (laminado, interfaz, conexiones, análisis, servidor, app) y por qué camino llega al motor: ficha, contrato o datos |

Lo que Tisma Slicer ya hace llega como fichas en `features/`, una por función, con las reglas de «Fichas» de
`CLEANROOM.md`.

Si una especificación contradice `CLEANROOM.md`, manda `CLEANROOM.md`.
