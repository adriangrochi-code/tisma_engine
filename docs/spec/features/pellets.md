# Impresoras de pellets

**Qué hace:** lamina para impresoras que funden granos de plástico (pellets) con un tornillo en lugar de empujar un
filamento. Son boquillas grandes (de 1 a 8 mm o más), cordones anchos, capas gruesas y flujos de cientos de mm³/s.

**Fase:** P3. Pedido del dueño (26-10-2026).

**Por qué ahora:** el paquete libre de perfiles (`profile_pack.md`) ya trae impresoras de pellets con boquillas de 5
y 8 mm y capas de 2,5 y 4 mm. Hoy el motor las rechaza porque `layers.height` no pasa de 2 mm.

**Fuentes:**
- **Dyze Design**, artículo del extrusor de pellets Pulsar (`https://dyzedesign.com`, «Flow-to-RPM»):
  - boquillas de 1; 1,5; 2; 3; 4 y 5 mm, y flujo máximo de 500 mm³/s;
  - el ejemplo usa boquilla de 3 mm, capa de 1 mm y cordón de 4 mm;
  - el flujo de un cordón es capa × (ancho − 0,215 × capa) × velocidad;
  - el tornillo gira «RPM por flujo» × flujo, con un factor de cada material.
- **Ginger Additive**, entrevista en 3Dnatives
  (`https://www.3dnatives.com/en/ginger-additive-pellet-3d-printing-accesible-150820234/`):
  - boquillas de hasta 8 mm, para cordones de hasta 2 cm;
  - hasta 2 kg/h;
  - volumen de 1 × 1 × 1 m.
- **Marlin**, `M200` (`https://marlinfw.org/docs/gcode/M200.html`):
  - `M200 D… S1` activa la extrusión volumétrica: «el eje E indica milímetros cúbicos en lugar de milímetros
    lineales»;
  - `L` fija un tope de mm³/s.
- **Una persona que usa un extrusor de pellets con RepRapFirmware**, foro de Duet
  (`https://forum.duet3d.com/topic/37013/`):
  - el diámetro de filamento del laminador es arbitrario: se ajusta con el multiplicador de extrusión;
  - el multiplicador que hace falta cambia con la velocidad.

  Es una observación, no un dato del fabricante.

## Ajustes

Nuevos, con el formato de `docs/CONFIG.md`:

| Clave | Tipo | Por defecto | Rango | Qué hace |
|---|---|---|---|---|
| `machine.feedstock` | texto | `"filament"` | `filament`, `pellet` | Con qué se alimenta la impresora |
| `machine.e_mode` | texto | `"length"` | `length`, `volume` | Qué escribe el eje E: `length`, mm de filamento de `machine.filament_diameter` (en pellets, un diámetro virtual que el firmware convierte en vueltas del tornillo); `volume`, mm³ de plástico |
| `extrusion.section` | texto | `"rectangle"` | `rectangle`, `rounded` | Cómo se calcula la sección de un cordón: `rectangle`, ancho × alto (como hoy); `rounded`, alto × (ancho − (1 − π/4) × alto), un rectángulo con los costados redondos |
| `extrusion.flow_by_speed` | lista de `[velocidad, %]` | `[]` | velocidad de 0 a 10 000 mm/s; % de 10 a 300 | Corrección del flujo según la velocidad del tramo, interpolada en línea recta entre los puntos y constante fuera de ellos. Vacía: sin corrección |

**Topes que cambian** (en filamento siguen igual en la práctica, porque las reglas de la boquilla ya los limitan):

| Clave | Rango de hoy | Rango nuevo |
|---|---|---|
| `layers.height`, `layers.first_height` | 0,01 a 2 | 0,01 a 10, y como hoy nunca más que la boquilla |
| `walls.width` | 0,05 a 10 | 0,05 a 40, y como hoy de un cuarto a cuatro veces la boquilla |
| `machine.nozzle_diameter` | > 0 | 0,05 a 30 |
| `machine.max_flow`, `extrusion.max_flow` | 0 a 1000 | 0 a 10 000 mm³/s |

## Reglas

1. **Alimentación con pellets** (`machine.feedstock` `pellet`):
   - los materiales que se ofrecen son los de pellets (`profile_pack.md` y `profiles.md`, regla 5: igual que con el
     diámetro del filamento);
   - un material de filamento en una impresora de pellets, o al revés, da un aviso, no un error;
   - lo demás del laminado es igual que con filamento.
2. **Eje E en largo** (`machine.e_mode` `length`):
   - E = volumen del tramo / (π × (`machine.filament_diameter` / 2)²) × flujo, como hoy;
   - con pellets, `machine.filament_diameter` es el diámetro virtual que espera el firmware de la máquina (por
     ejemplo, 1,75 mm).
3. **Eje E en volumen** (`machine.e_mode` `volume`):
   - E = volumen del tramo en mm³ × flujo;
   - después del G-code de inicio se escribe el comando del dialecto que pone el firmware en ese modo: en Marlin,
     `M200 D… S1`, con `machine.filament_diameter`;
   - en Klipper y RepRapFirmware, si el dialecto no tiene uno, no se escribe nada y hay un aviso: «Revisá que el
     firmware espere E en mm³».
4. **Sección redonda** (`extrusion.section` `rounded`):
   - la sección es h × (w − (1 − π/4) × h), con w el ancho y h el alto del cordón;
   - 1 − π/4 = 0,2146, el 0,215 del artículo de Dyze;
   - con capas finas casi no cambia nada. Con capas gruesas la diferencia es grande: con 6 × 2,5 mm, 13,66 mm² en
     lugar de 15 mm² (−9 %);
   - también vale para filamento.
5. **Flujo según la velocidad** (`extrusion.flow_by_speed`):
   - el flujo de cada tramo se multiplica por el % de su velocidad final, después del frenado por tiempo mínimo de
     capa y por `max_flow`;
   - el tope de flujo se compara con el volumen pedido, sin esta corrección.
6. **Velocidad por flujo:** con estos flujos, el tope que manda casi siempre es `machine.max_flow` (`docs/CONFIG.md`).
   Si baja una velocidad, el aviso dice cuánto y en qué tipo de extrusión, una vez por laminado.
7. **Lo que no cambia:** retracción, enfriamiento, soportes y avisos funcionan como con filamento, con sus mismos
   ajustes. En pellets conviene probar el coasting (`coasting.md`) en lugar de una retracción larga. Es una
   recomendación para el perfil, no una regla del motor.
8. **Calibración:** la torre de flujo por velocidad (`calibrations.md`, flujo volumétrico) sirve para llenar
   `extrusion.flow_by_speed`. Se mide el ancho de la pared a cada velocidad, y el % es ancho pedido / ancho medido.

## Pruebas

Boquilla de 5 mm, capa de 2,5 mm, cordón de 6 mm, `machine.filament_diameter` 1,75 (sección 2,40528 mm²), flujo
100 %.

| # | Ajustes | Resultado esperado |
|---|---|---|
| PE1 | Cubo de 100 mm, `machine.feedstock` `pellet`, la configuración de arriba | Lamina (hoy: código 2 porque la capa pasa de 2 mm) |
| PE2 | PE1, `machine.e_mode` `length`, sección `rectangle` | 6,2363 mm de E por mm de cordón |
| PE3 | PE1, `machine.e_mode` `length`, sección `rounded` | 5,6786 mm de E por mm |
| PE4 | PE1, `machine.e_mode` `volume`, sección `rounded`, dialecto `marlin` | 13,6587 de E por mm, y `M200 D1.75 S1` después del G-code de inicio |
| PE5 | Ejemplo de Dyze: capa de 1 mm, cordón de 4 mm, sección `rounded`, `volume` | 3,7854 mm³ por mm (Dyze: 1 × (4 − 0,215 × 1) = 3,785) |
| PE6 | PE3 con `machine.max_flow` 500 y `speed.walls` 60 | Paredes a 36,6 mm/s como mucho (500 / 13,6587) y un aviso |
| PE7 | PE3 con `extrusion.flow_by_speed` `[[20, 110], [60, 100]]`, paredes a 40 mm/s | 105 % del flujo: 5,9625 mm de E por mm |
| PE8 | Filamento de 0,4 mm con `layers.height` 2,5 | Error como hoy: la capa pasa la boquilla |
| PE9 | Material de filamento en una impresora de pellets | Aviso; lamina igual |
