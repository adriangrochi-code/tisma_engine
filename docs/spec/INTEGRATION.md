# Cómo llega el resto de Tisma Slicer al motor

Pedido del dueño (26-10-2026): analizar el resto del código de Tisma Slicer para integrarlo en el motor por la sala
limpia. Lo hizo el equipo que conoce Tisma Slicer; este documento no tiene código ni nombres de su código, solo qué
hay, de dónde salió y por qué camino entra al motor.

## Lo que hay en Tisma Slicer

Comparado con PrusaSlicer 2.9.6, Tisma Slicer agrega 117 archivos y cambia 172 en el programa. Además tiene tres
partes aparte: una biblioteca de análisis estructural, el servidor de Tisma Cloud y la app de Android.

| Parte | Qué es | Origen | Licencia |
|---|---|---|---|
| Laminado y formatos | Capas no planares, Z anti-aliasing, revisión previa, cinta, fragmentar, relleno ondulado, corrección de medidas, tipo de cama, STEP con caras, varias camas, proyectos de Bambu Studio, formato GOO, vaciado y ventosas de resina | Escrito dentro del fork, mezclado con código de PrusaSlicer | AGPL |
| Interfaz | Ventana nueva, panel de la placa, ajustes rápidos, Más, calibraciones, Ingeniería, Dispositivos, cámara, informar un problema, Tisma Cloud | Escrito dentro del fork, sobre la interfaz de PrusaSlicer | AGPL |
| Conexiones | USB, Bambu Lab en red local, Flashforge, cámara USB, búsqueda de impresoras | USB, Bambu Lab y cámara: equipo del dueño; Flashforge: de OrcaSlicer; el resto: de PrusaSlicer | AGPL |
| Análisis estructural | Biblioteca aparte: vóxeles, multigrilla, GPU, optimización, nervios, orientación, aerodinámica | Equipo del dueño | AGPL |
| Servidor de Tisma Cloud | Maestro, nodos, cola, cuentas, planes, cobro, informes de problemas, acceso remoto a impresoras | Equipo del dueño, sin código de PrusaSlicer | Propietaria |
| App de Android | Interfaz propia (Kotlin) que usa el núcleo de Tisma Slicer por una capa nativa | Equipo del dueño, sobre el núcleo de PrusaSlicer | AGPL |
| Herramientas | Conversor de perfiles, simuladores de impresoras, banco de pruebas | Equipo del dueño | AGPL |

## Tres caminos

Ningún código de Tisma Slicer entra al motor (`CLEANROOM.md`). Cada parte entra por uno de estos caminos:

1. **Por ficha:** la función se describe en `docs/spec/features/` y el motor la programa desde cero. Es el camino
   del laminado, los formatos, la interfaz, las conexiones y el análisis estructural.
2. **Por contrato:** los programas aparte del dueño (el servidor, la app) no entran al motor: hablan con él. La ficha
   describe el contrato (línea de comandos, mensajes de red, la API que el motor ofrece) y cada programa se adapta de
   su lado. Así el motor no recibe código, y esos programas siguen siendo del dueño.
3. **Por datos:** perfiles, base de conocimiento, íconos y logos, que no son código. Ya tienen su camino: el paquete
   de perfiles (`profile_pack.md`), la base de conocimiento (`assistant.md`) y `assets/brand/`.

## Estado por parte

| Parte | Camino | Ficha | Estado |
|---|---|---|---|
| Capas no planares y sus modos | Ficha | `nonplanar.md`, `nonplanar_modes.md` | Ficha lista; sin programar |
| Z anti-aliasing | Ficha | `z_antialiasing.md` | Programado |
| Revisión previa y estabilidad | Ficha | `print_review.md`, `stability.md` | Sin programar (QA8 de `qa/2026-10-26.md`) |
| Impresoras de cinta | Ficha | `belt_printer.md` | Sin programar |
| Fragmentar piezas | Ficha | `fragment.md` | Sin programar |
| Relleno ondulado | Ficha | `interlock_infill.md` | Parcial: el motor tiene `infill.nonplanar`; las claves de la ficha no están |
| Corrección de medidas | Ficha | `calibrations.md`, `gcode_extras.md` | Parcial |
| Tipo de cama y boquilla aparte | Ficha | `profiles.md`, `settings_panel.md` | Sin programar en la interfaz |
| STEP con caras para pintar y analizar | Ficha | `formats.md` (regla 2) | Falta lo de las caras: ficha nueva `step_faces.md` |
| Varias camas | Ficha | `model_tools.md` (regla 26) | Sin programar |
| Proyectos de Bambu Studio y OrcaSlicer | Ficha | `formats.md` (regla 18) | Falta medirlo con proyectos reales |
| Resina: GOO, vaciado, ventosas | Ficha | `resin.md`, `resin_supports.md` | Sin programar |
| Interfaz | Ficha | `interface.md`, `settings_panel.md`, `preferences.md`, `preview.md`, `model_tools.md` | En curso en el motor |
| Calibraciones | Ficha y datos | `calibrations.md`, `calibration_models.md`, `assistant.md` | Programadas en parte |
| Conexiones | Ficha | `printer_connections.md` («Integración en sala limpia»), `devices.md` | USB programado; el resto, sin programar |
| Análisis estructural, optimización, aerodinámica | Ficha | `structural_analysis.md`, `structural_optimization.md`, `aerodynamics.md` | El motor tiene su propio análisis (A1); faltan optimización y aerodinámica |
| Orientación automática | Ficha | `orientation.md` | Sin programar |
| Tisma Cloud: el motor como cliente y como programa de los nodos | Contrato | `features/cloud.md` | Ficha nueva |
| Informar un problema | Contrato | `features/cloud.md` (informes) | Ficha nueva; falta la pantalla en `problem_reports.md` |
| Acceso remoto a impresoras | Contrato | `features/cloud.md` (puentes) | Ficha nueva |
| App de Android | Contrato | `mobile_app.md` | Falta: la app pasa a usar el motor en lugar del núcleo de Tisma Slicer |
| Thingiverse | Contrato | `thingiverse.md` | Falta: su API pública |
| Detección de fallas con la cámara (Obico) | Contrato | `features/cloud.md` | Obico es AGPL: va como servicio aparte, nunca dentro del motor |
| Perfiles | Datos | `profile_pack.md` | Hecho (repositorio de perfiles) |
| Ayuda, tutoriales y guías de calibración | Datos | `assistant.md` | Hecho (repositorio de perfiles) |
| Íconos y logos | Datos | `assets/brand/` | Hecho |

## Lo que nunca entra

- Código de Tisma Slicer, de PrusaSlicer, de OrcaSlicer o de cualquier programa de `CLEANROOM.md`, aunque lo haya
  escrito el equipo del dueño dentro del fork.
- La biblioteca de análisis estructural de Tisma Slicer: el motor ya tiene la suya, y sigue las fichas.
- Bibliotecas cerradas de fabricantes (por ejemplo, el complemento de red de Bambu Studio).
- Obico dentro del motor (AGPL): solo como servicio aparte que habla por la red.

## Siguientes fichas, en orden

1. `mobile_app.md`: qué pantallas tiene la app y qué le pide al motor (la API que el motor tiene que ofrecer).
2. `step_faces.md`: caras del STEP para pintar, cargas de Ingeniería y volver a convertir en triángulos sin perder lo
   pintado.
3. `problem_reports.md`: la pantalla de informar un problema, con lo que manda (`cloud.md`).
4. `thingiverse.md`: buscar y abrir modelos con la API pública de Thingiverse.
