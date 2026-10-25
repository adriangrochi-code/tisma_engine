# Perfiles de impresoras, boquillas, camas y materiales

**Qué hace:** guarda los datos de cada impresora, de cada material y de cada forma de imprimir. Los presenta de modo
que la persona elija la impresora por su modelo y, aparte, la boquilla y la cama.

**Fase:** P3 (después del motor).
- La organización (impresora por modelo, boquilla y cama aparte, temperatura de cama por tipo de cama) es la de Tisma
  Slicer, diseñada por el equipo del dueño.
- Los datos son nuevos: los perfiles de Tisma Slicer vienen de PrusaSlicer, OrcaSlicer y Cura, y no pasan al motor.

## Contenido de un perfil

- **Impresora:**
  - fabricante y modelo;
  - forma y tamaño de la cama, y altura máxima;
  - boquillas posibles;
  - firmware (Marlin, Klipper, RepRapFirmware…) y límites de la máquina: velocidades, aceleraciones y tirones por
    eje;
  - si calibra sola la primera capa o las vibraciones (para ofrecer solo las calibraciones que sirven,
    `calibrations.md`);
  - tipos de cama, si las tiene intercambiables;
  - G-code de inicio y de fin, con variables (`GCODE_OUTPUT.md`);
  - medidas de la cabeza para capas no planares (`nonplanar.md`).
- **Material:**
  - tipo (PLA, PETG, ABS…), fabricante, diámetro y densidad;
  - temperaturas de boquilla y de cama (primera capa y resto);
  - ventilador (`cooling.md`), caudal máximo, costo por kg;
  - temperatura de cama por tipo de cama.
- **Impresión:** altura de capa y el resto de los ajustes de las fichas, con los nombres de `docs/CONFIG.md`.

## Reglas

1. **Impresora por modelo:**
   - cada modelo aparece una vez en las listas, no una vez por boquilla;
   - se muestra el perfil de la boquilla elegida, o el de 0,4 mm si no se eligió otra.
2. **Boquilla aparte:**
   - la lista de boquillas es la de los perfiles de ese modelo;
   - elegir una cambia al perfil de esa boquilla;
   - una impresora de red asociada al modelo sigue elegida;
   - un perfil propio de la persona muestra su boquilla, sin cambiarla.
3. **Tipo de cama:**
   - solo en impresoras con camas intercambiables;
   - nombres de cama: los que usa el fabricante de la impresora;
   - la cama elegida se guarda en el proyecto. Al cambiar de impresora, vuelve la cama por defecto de la nueva.
4. **Temperatura de cama por tipo de cama:**
   - cada material puede tener una temperatura de cama (primera capa y resto) para cada tipo de cama;
   - al laminar, la de la cama elegida reemplaza la del material;
   - un 0 deja la del material;
   - sin cama elegida se usa la cama por defecto de la impresora, también para las variables del G-code de inicio.
5. **Materiales por diámetro:** se ofrecen solo los del diámetro que usan las impresoras instaladas (1,75 o 2,85 mm).
6. **Materiales al instalar una impresora:** se instalan sin preguntar los materiales recomendados para ella. Agregar
   un material después no vuelve a pedir la impresora.
7. **Perfiles propios:** la persona puede guardar, renombrar, borrar y comparar perfiles. Uno propio parte de uno del
   sistema.

## De dónde salen los datos

- **Solo fuentes oficiales,** cada valor con su fuente en el perfil o al lado: ficha técnica o página del producto del
  fabricante, manual, configuración de firmware que publica el fabricante. Por ejemplo, las impresoras de resina de
  `resin.md` toman la pantalla de las especificaciones de Elegoo.
- **Materiales:** temperaturas, densidad y velocidad de la ficha técnica (TDS) del fabricante del material.
- **Sin dato oficial:** se usa un valor genérico de la familia del material (PLA, PETG…), marcado como genérico.
- **Nunca** se copian perfiles de otros laminadores (PrusaSlicer, OrcaSlicer, Bambu Studio, Cura, Creality Print…),
  ni siquiera los números: sus repositorios de perfiles tienen licencias (AGPL, LGPL) que el motor no acepta, y
  `CLEANROOM.md` no permite leerlos.
- Un valor que da el fabricante, aunque otro laminador también lo use, se toma del fabricante y se cita el
  fabricante.

## Revisión de los perfiles

Una herramienta revisa cada perfil, con la herencia resuelta, y avisa de:
- campos que faltan (tipo de material, diámetro, densidad, cama);
- temperaturas fuera del rango de la familia del material;
- impresoras que no laminan un cubo de prueba.

## Pruebas

Escenarios de aceptación de la organización, como funcionan en Tisma Slicer.

| # | Escenario | Resultado esperado |
|---|---|---|
| PF1 | Un modelo con perfiles para boquillas de 0,25, 0,4, 0,6 y 0,8 | Una sola fila en la lista de impresoras; la lista de boquillas muestra las cuatro |
| PF2 | Elegir la boquilla de 0,6 | Cambia al perfil de 0,6 de ese modelo; la impresora sigue siendo la misma en la lista |
| PF3 | Impresora con camas intercambiables; material con 55 °C en la cama lisa y 65 °C en la texturada | Con la texturada elegida, el G-code calienta la cama a 65 °C |
| PF4 | El mismo material con 0 en la cama texturada | Se usa la temperatura de cama del material |
| PF5 | Cambiar a otra impresora | Vuelve la cama por defecto de la nueva impresora |
| PF6 | Instalar solo impresoras de 1,75 mm | La lista de materiales no ofrece materiales de 2,85 mm |
| PF7 | Perfil con una temperatura de PLA de 320 °C | La revisión avisa: fuera del rango de la familia |
