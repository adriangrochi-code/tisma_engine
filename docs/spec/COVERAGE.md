# Cobertura: todo lo que hace Tisma Slicer, en las fichas

**Decisión del dueño (24-10-2026):** el motor va a ser el nuevo slicer y Tisma Slicer se descarta. Ninguna función se
puede perder: todo lo que hace Tisma Slicer tiene que estar en una ficha antes de dejarlo.

Esta página es el inventario completo. Sale de recorrer Tisma Slicer entero el 24-10-2026:
- todos los ajustes de impresión, filamento, impresora y resina, página por página (unos 670);
- las herramientas, menús y diálogos de la interfaz;
- los formatos, las conexiones y los servicios.

Cada fila dice qué ficha la cubre y en qué estado está:
- **✔** cubierta;
- **◐** parcial: la ficha existe y la fila dice qué le falta;
- **✘** falta: la fila dice en qué ficha nueva va.

Los nombres son descripciones, no los nombres internos de Tisma Slicer.

## Consecuencias de la decisión

1. **Vista previa propia:** el motor muestra su G-code en su propia interfaz (P1). Las etiquetas de
   `GCODE_OUTPUT.md` siguen sirviendo: son las que lee la vista previa.
2. **Perfiles propios:** el motor carga sus perfiles (`features/profiles.md`) sin pasar por Tisma Slicer.
3. **Migración de los usuarios:** quien usa Tisma Slicer tiene proyectos 3MF y perfiles guardados. Hace falta una forma
   de pasarlos al motor; la decisión es del dueño (ver «Migración» al final).
4. **F6 cambia:** ya no es «Tisma Slicer llama al motor», sino «el motor reemplaza a Tisma Slicer». Las fases de
   producto (P1–P5) pasan a ser obligatorias antes de dejar Tisma Slicer.
5. **Cuándo se deja Tisma Slicer:** recomendación del equipo de fichas: cuando todas las filas estén en ✔ y el motor
   pase las pruebas de cada ficha.

## Laminado FDM

### Capas y corte

| Función | Ficha | Estado |
|---|---|---|
| Altura de capa y de la primera capa | `slicing.md` | ✔ |
| Cierre de grietas, modos de relleno de la malla (normal, par-impar, cerrar agujeros) | `slicing.md` | ✔ |
| Compensación XY de contornos y de agujeros, pata de elefante | `slicing.md` | ✔ |
| Resolución de los contornos | `slicing.md` | ✔ |
| Altura de capa variable: adaptativa, suavizado y pintada a mano, con los límites de la boquilla | `slicing.md` | ◐ falta la herramienta de la interfaz y los límites por extrusor → `model_tools.md` |
| Modo vaso (espiral): una sola pared que sube sin costura | `vase_mode.md` | ✔ |

### Paredes

| Función | Ficha | Estado |
|---|---|---|
| Cantidad, ancho variable, paredes finas, orden, costura (alineada, cercana, aleatoria, atrás), hueco de la costura | `perimeters.md` | ✔ |
| Costuras internas escalonadas | `walls_extra.md` | ✔ |
| Costura en bufanda (inicio en rampa): dónde, solo en lazos lisos, altura inicial, largo, lazo entero, tramo máximo, también en los internos | `walls_extra.md` | ✔ |
| Perímetros extra donde hacen falta y en voladizos | `walls_extra.md` | ✔ («donde hacen falta» no cambió nada en las piezas de prueba) |
| Asegurar el espesor de la cáscara vertical (no, parcial, sí) | `walls_extra.md` | ✔ |
| Detección de perímetros en voladizo y su velocidad por grado de voladizo | `walls_extra.md`, `speeds_accels.md` | ✔ |
| Evitar cruzar voladizos que se curvan | `walls_extra.md` | ✔ (regla de la documentación, sin medir) |
| Puentes gruesos (cordón redondo) | `gcode_motion.md` | ✔ |
| Relleno de huecos (activar o no) | `perimeters.md`, `walls_extra.md` | ✔ |
| Un solo perímetro en la capa de arriba (en todas o solo en la de más arriba) y en la primera capa | `walls_extra.md` | ✔ |
| Perímetros escalonados (Tisma) | `staggered_perimeters.md` | ✔ |
| Piel difusa (exterior o todas las paredes, espesor, distancia entre puntos) y pintada | `fuzzy_skin.md`, `model_tools.md` (regla 27) | ✔ |
| Z anti-aliasing (Tisma) | `z_antialiasing.md` | ✔ |
| Arcos en voladizos (Tisma) | `arc_overhangs.md` | ✔ |

### Superficies y relleno

| Función | Ficha | Estado |
|---|---|---|
| Capas sólidas arriba y abajo, espesor mínimo de la cáscara arriba y abajo | `surfaces_infill.md`, `infill_patterns.md` | ✔ |
| Relleno rectilíneo, grilla y giroide | `surfaces_infill.md` | ✔ |
| Los otros patrones de relleno: línea, triángulos, estrellas (el de Tisma por defecto), cúbico, cúbico adaptativo, cúbico de soporte, rayo, panal, panal 3D, Hilbert, cuerdas de Arquímedes, espiral octagrama, concéntrico, rectilíneo alineado, zigzag | `infill_patterns.md` | ✔ |
| Patrones de las superficies de arriba y abajo: monotónico, líneas monotónicas, concéntrico, Hilbert, cuerdas de Arquímedes, espiral octagrama | `surfaces_infill.md`, `infill_patterns.md` | ✔ |
| Anclaje del relleno a las paredes (largo y máximo) | `surfaces_infill.md` | ✔ |
| Relleno sólido bajo un área, ángulo del relleno, ángulo de los puentes | `surfaces_infill.md` | ✔ |
| Combinar el relleno cada N capas (y automático hasta una altura máxima) | `infill_patterns.md` | ✔ |
| Relleno sólido cada N capas | `infill_patterns.md` | ✔ |
| Relleno antes que las paredes | `gcode_motion.md` | ✔ |
| Solape del relleno con las paredes | `surfaces_infill.md` | ✔ |
| Relleno denso bajo las superficies de arriba (Tisma) | `dense_infill.md` | ✔ |
| Relleno ondulado entrelazado (Tisma) | `interlock_infill.md` | ✔ |
| Planchado: arriba, solo la de más arriba o todas las sólidas; flujo, separación, velocidad | `ironing.md` | ✔ |
| Proporción de flujo de los puentes | `speeds_accels.md` | ✔ |

### Falda, borde y balsa

| Función | Ficha | Estado |
|---|---|---|
| Falda: vueltas, distancia, capas | `supports_skirt.md` | ✔ |
| Largo mínimo de la falda | `skirt_brim_raft.md` | ✔ |
| Escudo contra corrientes (falda tan alta como la pieza, o solo en algunas capas) | `skirt_brim_raft.md` | ✔ |
| Borde: ancho | `supports_skirt.md` | ✔ |
| Tipo de borde (afuera, adentro, los dos, ninguno) y separación del borde | `skirt_brim_raft.md` | ✔ |
| Balsa: capas, distancia de contacto, expansión, densidad y expansión de su primera capa, velocidad de la primera capa sobre la balsa | `skirt_brim_raft.md` | ✔ |

### Soportes

| Función | Ficha | Estado |
|---|---|---|
| Activar, automático, umbral, separación Z y XY, interfaz, separación de líneas, ángulo, puentes sin soporte | `supports_skirt.md` | ✔ |
| Soportes en todas las capas hasta una altura (forzar las primeras N capas) | `supports_more.md` | ✔ |
| Solo desde la cama | `interface.md` (chips), `supports_more.md` | ✔ |
| Estilo: grilla, ajustado («snug») | `supports_skirt.md`, `supports_more.md` | ✔ |
| Soportes en árbol y orgánicos: ángulos, diámetros de rama y de punta, distancia entre ramas, densidad de las puntas | `supports_more.md` | ✔ (los ángulos y las dos paredes, sin medir) |
| Separación Z de abajo, capas de interfaz de abajo | `supports_more.md` | ✔ |
| Patrón del soporte (rectilíneo, grilla, panal) y de la interfaz (automático, rectilíneo, concéntrico), separación de la interfaz, lazos de contacto | `supports_skirt.md`, `supports_more.md` | ✔ |
| Vaina alrededor del soporte, radio de cierre | `supports_more.md` | ✔ |
| Soporte y anti-soporte pintados, y como piezas (bloqueador y forzador) | `supports_more.md`, `model_tools.md` (regla 27) | ✔ |
| Sincronizar las capas del soporte con las de la pieza (multimaterial) | `multi_extruder.md` | ✔ |

### Velocidades y aceleraciones

| Función | Ficha | Estado |
|---|---|---|
| Velocidades de paredes, externa, perímetros chicos, relleno, sólido, arriba, puentes, viajes, primera capa, máxima, caudal máximo | `gcode_motion.md` | ✔ |
| Velocidad de la interfaz del soporte, del relleno de huecos, sobre puentes, del relleno de la primera capa, de Z en los viajes | `speeds_accels.md`, `supports_more.md` | ✔ |
| Velocidad del soporte | `docs/CONFIG.md` (motor) | ✔ |
| Velocidades dinámicas en voladizos (cuatro grados de voladizo) | `speeds_accels.md` | ✔ |
| Aceleraciones por tipo (externa, paredes, arriba, sólido, relleno, puentes, primera capa, sobre la balsa, torre de purga, viajes, viajes cortos, por defecto) | `speeds_accels.md` | ✔ (la de la torre de purga, sin medir) |
| Ecualizador de presión (pendiente máxima del caudal al subir y al bajar) | `speeds_accels.md` | ✔ |
| Velocidad máxima del relleno por filamento y al cruzar el relleno | `speeds_accels.md` | ✔ |

### Enfriamiento

| Función | Ficha | Estado |
|---|---|---|
| Tiempo mínimo de capa en dos etapas, ventilador mínimo y máximo, en puentes, primeras capas sin ventilador, siempre encendido | `cooling.md` | ✔ |
| Capa desde la que el ventilador llega al máximo (subida gradual) | `cooling_more.md` | ✔ |
| Ventilador según el grado de voladizo (cuatro grados) | `cooling_more.md` | ✔ |
| Lógica del frenado y distancia de transición en los perímetros | `cooling_more.md` | ✔ |

### Retracción y viajes

| Función | Ficha | Estado |
|---|---|---|
| Largo, velocidad, viaje mínimo, salto en Z | `gcode_motion.md` | ✔ |
| Viajes por dentro sin cruzar paredes, desvío máximo | `retraction_travel.md` | ✔ |
| Velocidad de la vuelta (deretracción), extra al volver, retraer en el cambio de capa | `retraction_travel.md` | ✔ |
| Barrido al retraer y cuánto se retrae antes del barrido | `retraction_travel.md` | ✔ |
| Salto en Z: en rampa, pendiente, máximo, solo antes de un obstáculo, solo por encima o por debajo de una altura | `retraction_travel.md` | ✔ (el del obstáculo, sin medir) |
| Retracción del firmware (`G10`/`G11`) | `retraction_travel.md` | ✔ |
| Retracción al cambiar de herramienta | `multi_extruder.md` | ✔ |
| Preferir el sentido horario | `retraction_travel.md` | ✔ |
| Coasting (Tisma) | `coasting.md` | ✔ |
| Pressure advance escrito en el G-code (Tisma, estudio) | `pressure_advance_gcode.md` | ✔ |

### Anchos y flujo

| Función | Ficha | Estado |
|---|---|---|
| Anchos de paredes, externa, primera capa, relleno, sólido, arriba | `perimeters.md`, `surfaces_infill.md` | ✔ |
| Ancho del soporte; anchos automáticos | `supports_more.md` (regla 18) | ◐ los anchos automáticos no cambiaron nada desde la línea de comandos, ni con boquillas distintas por extrusor |
| Multiplicador de extrusión del filamento | `gcode_motion.md` | ✔ |

## Filamento

| Función | Ficha | Estado |
|---|---|---|
| Color, diámetro, temperaturas de boquilla y cama (primera capa y resto), cama por tipo de cama | `profiles.md`, `gcode_motion.md` | ✔ |
| Densidad, costo, peso del carrete: peso y costo de la impresión | `interface.md` (los muestra), `gcode_extras.md` | ✔ |
| Temperatura de la cámara y mínima de la cámara | `gcode_extras.md` | ✔ |
| Tipo, soluble, abrasivo | `profiles.md` (regla 8, prueba PF8) | ✔ no cambian el laminado |
| Compensación de la contracción en XY y en Z | `gcode_extras.md` | ◐ en la línea de comandos no cambió nada: falta medirla con la interfaz y una pieza real |
| G-code de inicio y fin del filamento | `gcode_templates.md` | ✔ |
| Cambios del filamento sobre la impresora (retracción, salto en Z, barrido por filamento) | `retraction_travel.md` (regla 10) | ✔ |
| Parámetros de cambio de herramienta, carga, descarga, enfriamiento, purga y «ramming» | `multi_extruder.md` | ✔ |

## Impresora y G-code

| Función | Ficha | Estado |
|---|---|---|
| Forma de la cama, altura máxima, tipo de cama por defecto | `placement.md`, `profiles.md` | ✔ |
| Desfase de Z | `gcode_extras.md` | ✔ |
| Firmware (dialectos) | `gcode_firmware.md`, `gcode_extras.md` | ✔ |
| Límites de la máquina (avance, aceleración, jerk, avance mínimo) y su uso (escribir, solo estimar, ignorar) | `gcode_firmware.md` | ✔ |
| Modo silencioso (segunda estimación de tiempo) | `gcode_extras.md`, `print_time.md` | ✔ |
| Tiempo de impresión estimado: límites, comandos del G-code, qué cuenta | `print_time.md` | ◐ faltan pares medidos con impresoras reales |
| Tiempo restante, etiquetas de objetos | `gcode_firmware.md` | ✔ |
| Miniaturas en el G-code (tamaños y formatos PNG, JPG, QOI, la de las pantallas de BTT) | `gcode_extras.md` | ◐ falta el formato exacto: solo la interfaz las escribe → `formats.md` |
| G-code binario | `gcode_extras.md` | ✔ |
| Arcos `G2`/`G3` (ajuste de arcos) | `gcode_extras.md` | ✔ |
| Extrusión relativa o absoluta | `GCODE_OUTPUT.md` | ✔ |
| E volumétrica | `gcode_extras.md` | ✔ |
| G-code de inicio y fin, con variables | `GCODE_OUTPUT.md`, `gcode_templates.md` | ✔ |
| G-code antes y después del cambio de capa, al cambiar de herramienta, entre objetos, al cambiar de color, en la pausa, plantilla propia; temperaturas automáticas en el inicio | `gcode_templates.md`, `sequential.md`, `multi_extruder.md` | ✔ |
| Cambios por altura (temperatura, ventilador, flujo, G-code) | `docs/CONFIG.md` (`changes`) | ✔ |
| Cambio de color y pausa en una capa, desde la vista previa | `gcode_templates.md`, `preview.md` (regla 12) | ✔ |
| Parámetros propios del perfil para usar en el G-code | `presets.md` (reglas 20 a 22), `gcode_templates.md` | ✔ |
| Comentarios detallados en el G-code | `gcode_extras.md` | ✔ |
| Formato del nombre del archivo de salida, con variables | `gcode_templates.md` | ✔ |
| Scripts de posproceso | `gcode_templates.md` | ✔ |
| Impresión secuencial (objeto por objeto), con el espacio del cabezal (radio y alto) | `sequential.md` | ✔ |
| Boquilla de alto caudal; altura de capa mínima y máxima por extrusor; desfase de cada extrusor | `multi_extruder.md` | ✔ |
| Impresoras de cinta (Tisma) | `belt_printer.md` | ✔ |
| Cabezal para capas no planares (Tisma) | `nonplanar.md` | ✔ |

## Multimaterial

| Función | Ficha | Estado |
|---|---|---|
| Boquillas de distinto diámetro y mezcla de colores por capas (Tisma) | `multimaterial.md` | ✔ |
| Mezcla de colores: ciclo de capas de cada proporción, degradés, costo de purga, archivo del proyecto | `color_mixing.md` | ✔ |
| Extrusor por tipo (paredes, relleno, sólido, soporte, interfaz, torre) y para la temperatura de la cama | `multi_extruder.md` | ✔ |
| Prevención de goteo (bajar la temperatura de los que esperan) | `multi_extruder.md` | ✔ |
| Torre de purga: ancho, borde, puentes, cono, separación, flujo extra, sin capas ralas, cebado | `multi_extruder.md` | ✔ (el cono y el cebado, sin medir) |
| Orden de los cambios de herramienta, cáscaras de interfaz | `multi_extruder.md` | ✔ |
| Regiones pintadas por color: ancho máximo de las franjas, profundidad del entrelazado | `multi_extruder.md` | ◐ sin medir: hace falta pintar en la interfaz → `model_tools.md` |
| Vigas de entrelazado entre materiales | `multi_extruder.md` | ✔ |
| Un extrusor con varios materiales (MMU): tubo de enfriamiento, posición de estacionamiento, carga extra, purga, corriente alta en el cambio | `multi_extruder.md` | ✔ |
| Volúmenes de purga entre filamentos | `multi_extruder.md` | ✔ |

## Resina

| Función | Ficha | Estado |
|---|---|---|
| Pantalla, exposición, capas iniciales, movimientos, GOO | `resin.md` | ✔ |
| Inclinación del tanque (rápida, lenta, alta viscosidad) y perfil de impresión del material | — | ✘ → `resin.md` (actualizar): solo para impresoras con tanque que se inclina; el formato GOO no lo guarda |
| Correcciones: absoluta, gamma, pata de elefante con ancho mínimo, capas de corrección de Z | `resin.md` (reglas 20 a 22) | ◐ la absoluta no cambió nada desde la línea de comandos; falta medirla desde la interfaz |
| Formatos de salida además de GOO, y su precisión | `resin.md` | ◐ → `formats.md` |
| Soportes, base y vaciado | `resin_supports.md` | ◐ faltan tipo de árbol, solo forzados, y las opciones finas de cabeza, pilar, uniones y base → `resin_supports.md` (actualizar) |
| Costo del frasco, volumen, peso, densidad: costo de la impresión | `gcode_extras.md` | ✔ |
| Agujeros de drenaje y puntos de soporte a mano | `resin_supports.md`, `model_tools.md` (regla 28) | ✔ |

## Herramientas sobre los modelos

| Función | Ficha | Estado |
|---|---|---|
| Mover, rotar, escalar (también por medidas), espejar | `model_tools.md` (reglas 11, 12, 16 y 17) | ✔ |
| Apoyar sobre una cara | `interface.md` (nombra) | ◐ → `model_tools.md` |
| Cortar: plano, con conectores (espigas, encastres en cola de milano) | `model_tools.md` (reglas 13 y 18) | ✔ |
| Separar en objetos y en partes | `model_tools.md` (reglas 14 y 19) | ✔ |
| Medir (distancias, ángulos, diámetros) | `model_tools.md` (regla 20) | ✔ |
| Simplificar la malla | `slicing.md` (contornos), `model_tools.md` (regla 21) | ✔ |
| Texto en relieve y SVG en relieve | `model_tools.md` (reglas 22 y 23) | ✔ |
| Pintar soportes, costura, piel difusa y colores | `model_tools.md` (regla 27) | ✔ |
| Partes, volúmenes negativos, modificadores, bloqueadores y forzadores de soporte, modificador por rango de alturas | `model_tools.md`, `supports_more.md` (bloqueadores y forzadores) | ✔ |
| Ajustes por objeto, por parte y por rango de alturas | `model_tools.md` (reglas 5 a 7), `settings_levels.md` (reglas 10 y 11) | ✔ |
| Copias (instancias), llenar la cama, escalar al volumen, convertir desde metros o pulgadas, reemplazar por otro STL, recargar desde el disco | `placement.md` (copias), `model_tools.md` (reglas 11, 15, 25 y 30) | ✔ |
| Ordenar (con rotación, separación y alineación) y varias camas | `placement.md`, `model_tools.md` (regla 26) | ✔ |
| Galería de formas | `model_tools.md` (regla 24) | ✔ |
| Orientación automática (Tisma) | `orientation.md` | ✔ |
| Dividir para que entre (Tisma) | `fragment.md` | ✔ |
| Reparar la malla | `slicing.md` (mallas sucias), `formats.md` (regla 4) | ✔ |

## Vista previa

| Función | Ficha | Estado |
|---|---|---|
| Leyenda por tipo, deslizadores, tiempos | `interface.md` | ✔ |
| Otras vistas: altura, ancho, velocidad, ventilador, temperatura, caudal, tiempo de capa (lineal y logarítmico), herramienta, cambio de color | `preview.md` | ✔ |
| Mostrar viajes, barridos, retracciones, costuras, cambios de herramienta y de color, pausas, centro de gravedad, la pieza | `interface.md` (fila de íconos), `preview.md` (reglas 5 y 11) | ✔ |
| Visor de G-code aparte | `interface.md` (Más), `preview.md` (regla 13) | ✔ |
| Estimación de tiempo normal y silenciosa, filamento por tipo | `interface.md` | ✔ |

## Archivos

| Función | Ficha | Estado |
|---|---|---|
| Leer STL, OBJ, 3MF, AMF, STEP, SVG, ZIP | `formats.md` (reglas 1 a 5 y 17) | ✔ |
| Proyecto 3MF: qué guarda (piezas, partes, ajustes por objeto, pintura, modificadores, varias camas, análisis, capas no planares) | `formats.md` | ◐ medidos el paquete, las mallas, las partes y los ajustes; falta lo que se guarda desde la interfaz (regla 10) |
| Abrir proyectos de Bambu Studio y OrcaSlicer | `formats.md` (regla 18) | ◐ falta medirlo con proyectos reales |
| Exportar la placa como STL u OBJ, con soportes, y las trayectorias como OBJ | `formats.md` (reglas 15 y 16) | ✔ |
| Archivos de resina: importar, exportar | `resin.md` | ◐ → `formats.md` |
| Exportar a una tarjeta o memoria y expulsarla | `formats.md` (regla 19) | ✔ |

## Perfiles y configuración

| Función | Ficha | Estado |
|---|---|---|
| Impresora por modelo, boquilla y cama aparte, datos oficiales | `profiles.md` | ✔ |
| Miles de impresoras, filamentos y resinas de otros laminadores, descargados de un paquete libre aparte | `profile_pack.md` | ✔ |
| Asistente sin conexión, glosario, fichas de materiales y tutoriales (pedido del dueño, 26-10-2026) | `assistant.md` | ✔ |
| Perfiles del sistema y del usuario, herencia, guardar, renombrar, borrar, separar del padre | `presets.md` | ✔ |
| Compatibilidad entre perfiles (condiciones) | `presets.md` | ✔ |
| Impresoras físicas (una impresora con su conexión) | `devices.md` | ✔ |
| Comparar perfiles, paquetes de configuración, instantáneas, actualizaciones de los perfiles | `presets.md` | ◐ falta medir la actualización de los perfiles del sistema y volver a una instantánea |
| Plantillas de filamento | `interface.md` (menú del filamento) | ◐ falta medir cómo se instalan y se usan (no entraron en `presets.md`) |
| Buscar un ajuste | `interface.md` | ✔ |
| Niveles de ajustes (Simple, Advanced, Expert), lista propia de ajustes (Quick settings) | `settings_levels.md` | ◐ falta el comienzo de tres páginas de impresión en Expert y las páginas de material e impresora en Expert |
| Asistente de primer uso | `interface.md` | ✔ |
| Preferencias | `interface.md` | ◐ falta la lista completa → `preferences.md` |

## Conexión con impresoras

| Función | Ficha | Estado |
|---|---|---|
| USB, Bambu Lab en red local, cámara USB, botón de enviar | `printer_connections.md` | ✔ |
| OctoPrint, Moonraker, Duet, AstroBox, Repetier, Flashforge, Bambu Lab: pantallas, pedidos, errores, por marca | `devices.md` | ✔ |
| PrusaLink y Prusa Connect con un perfil de Prusa; subir e imprimir en FlashAir y MKS | `devices.md` | ◐ falta medirlos (ver «Lo que falta medir») |
| Cola de envíos | `interface.md` (Más) | ◐ → `printer_connections.md` |
| Grabar el firmware de la impresora | `interface.md` (Más) | ◐ → `printer_connections.md` |

## Lo propio de Tisma

| Función | Ficha | Estado |
|---|---|---|
| Análisis estructural, optimización, aerodinámica | `structural_analysis.md`, `structural_optimization.md`, `aerodynamics.md` | ✔ |
| Capas no planares y sus modos | `nonplanar.md`, `nonplanar_modes.md` | ✔ |
| Calibraciones y sus piezas | `calibrations.md`, `calibration_models.md` | ✔ |
| Revisión antes de imprimir y estabilidad | `print_review.md`, `stability.md` | ✔ |
| Ubicación en la cama | `placement.md` | ✔ |
| Impresoras de pellets (pedido del dueño, 26-10-2026; Tisma Slicer no las trata aparte) | `pellets.md` | ✔ |
| Impresoras de metal por arco, WAAM (pedido del dueño, a futuro) | `waam.md` (estudio) | ◐ las reglas se cierran con una máquina de prueba |

## Servicios

| Función | Ficha | Estado |
|---|---|---|
| Tisma Cloud desde el programa: cuenta, laminar y calcular en la nube | `interface.md` (diálogo) | ◐ → `cloud.md` |
| Servidor de Tisma Cloud: maestro y nodos, cola con prioridades, cuentas, planes y cobro, seguridad | — | ✘ → `cloud.md` |
| Informar un problema, con catálogo de errores y recompensa | `interface.md` (qué manda) | ◐ → `problem_reports.md` |
| Thingiverse dentro del programa | `interface.md` (nombra) | ◐ → `thingiverse.md` |
| App de teléfono: laminar, ajustes, Ingeniería, impresoras por WiFi y cámara | — | ✘ → `mobile_app.md` |
| Acceso remoto a las impresoras y detección de fallas con la cámara | — | ✘ → `cloud.md` |

## Interfaz

| Función | Ficha | Estado |
|---|---|---|
| Ventana, menús, aspecto, asistente, Ingeniería | `interface.md` | ✔ |
| Controles de la vista 3D: girar, mover, acercar, vistas, perspectiva u ortogonal | `preview.md` (regla 10) | ✔ |
| Deshacer y rehacer | `model_tools.md` (regla 29) | ✔ |
| Idiomas (inglés y español) | `interface.md` (principio 2) | ✔ |

## Migración

Para no dejar a los usuarios sin sus proyectos y perfiles, hay dos caminos. Decide el dueño:
1. **Del lado de Tisma, antes de dejarlo:** una herramienta que pasa los perfiles y proyectos al formato del motor.
   Queda en el repositorio de Tisma Slicer y no toca el motor.
2. **Del lado del motor:** leer los proyectos 3MF que escribe Tisma Slicer, como formato observado en los archivos (no
   en el código), con una ficha de qué guarda cada uno (`formats.md`).

## Lotes de fichas nuevas

| Lote | Fichas |
|---|---|
| 5 | `walls_extra.md`, `infill_patterns.md`, `skirt_brim_raft.md`, `supports_more.md`, `speeds_accels.md`, `cooling_more.md`, `retraction_travel.md`, `vase_mode.md`, `fuzzy_skin.md` |
| 6 | `gcode_extras.md`, `gcode_templates.md`, `sequential.md`, `multi_extruder.md` |
| 7 | `model_tools.md`, `preview.md`, `formats.md`, `presets.md`, `preferences.md` |
| 8 | `cloud.md`, `problem_reports.md`, `thingiverse.md`, `mobile_app.md`; actualizar `printer_connections.md`, `resin.md`, `resin_supports.md`, `profiles.md` |
