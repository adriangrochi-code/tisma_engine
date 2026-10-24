# Varios extrusores y varios materiales

**Qué hace:** imprimir una pieza con más de un filamento:
- impresoras con varias boquillas (cabezales o herramientas);
- un solo extrusor que cambia de filamento (MMU);
- qué extrusor imprime cada parte y cada tipo de extrusión;
- la torre de purga y el purgado dentro del relleno o de otros objetos;
- temperaturas de espera, desfases entre boquillas, cáscaras y vigas de entrelazado entre materiales.

**Fase:** después de F6.

**Fuente del comportamiento:** documentación de usuario de los slicers (base de conocimiento de Prusa) y las mediciones
de abajo.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `machine.extruders` | — | 1 | Cantidad de extrusores; cada ajuste de extrusor y de filamento pasa a ser una lista |
| `machine.single_extruder_mmu` | — | no | Un solo extrusor que cambia de filamento (MMU) |
| `machine.extruder_offset` | mm (x, y) por extrusor | 0, 0 | Desplazamiento de cada boquilla respecto de la primera; se resta de las coordenadas |
| `machine.min_layer_height`, `machine.max_layer_height` | mm por extrusor | 0,07 / 0 = 75 % de la boquilla | Límites de altura de capa del extrusor (capa variable y soporte) |
| `machine.high_flow_nozzle` | — por extrusor | no | Boquilla de alto caudal (se escribe en los datos del archivo) |
| `object.extruder` | — | 1 | Extrusor de un objeto o de una parte |
| `extruders.walls`, `extruders.infill`, `extruders.solid_infill`, `extruders.support`, `extruders.support_interface`, `extruders.wipe_tower` | — | el del objeto (0) | Extrusor por tipo de extrusión |
| `extruders.bed_temperature_from` | — | 0 | Extrusor cuyas temperaturas de cama valen; 0: el primero que imprime en la primera y la segunda capa |
| `extruders.ooze_prevention` | — | no | Bajar la temperatura de los extrusores que esperan |
| `extruders.standby_delta` | °C | −5 | Cuánto se bajan |
| `retraction.toolchange_length` | mm por extrusor | 10 | Retracción antes de cambiar de herramienta |
| `retraction.toolchange_restart_extra` | mm por extrusor | 0 | Filamento de más al volver a usar esa herramienta |
| `gcode.toolchange` | texto | vacío | G-code al cambiar de herramienta (`gcode_templates.md`), con `previous_extruder`, `next_extruder`, `toolchange_z` |
| `surfaces.interface_shells` | — | no | Capas sólidas donde se tocan materiales distintos, arriba y abajo |
| `interlocking.beams` | — | no | Vigas que entrelazan materiales distintos donde se tocan |
| `interlocking.beam_width` | mm | 0,8 | Ancho de las vigas |
| `interlocking.beam_layers` | capas | 2 | Alto de las vigas |
| `interlocking.orientation` | ° | 22,5 | Dirección de las vigas |
| `interlocking.depth` | celdas | 2 | Hasta dónde llegan desde el borde entre materiales |
| `interlocking.boundary_avoidance` | celdas | 2 | Distancia al exterior de la pieza sin vigas |
| `painting.max_region_width` | mm | 0 = sin límite | Ancho máximo de una región pintada de un color |
| `painting.interlocking_depth` | mm | 0 | Entrelazado entre regiones pintadas |
| `support.synchronize_layers` | — | no | Las capas del soporte son las de la pieza (pide `support.z_gap` 0) |
| `wipe_tower.enabled` | — | no | Torre de purga |
| `wipe_tower.x`, `wipe_tower.y`, `wipe_tower.rotation` | mm, ° | 180, 140, 0 | Posición y giro (se guardan con la cama del proyecto) |
| `wipe_tower.width` | mm | 60 | Ancho |
| `wipe_tower.brim_width` | mm | 2 | Borde de la torre |
| `wipe_tower.bridging` | mm | 10 | Distancia máxima entre apoyos en las capas ralas |
| `wipe_tower.cone_angle` | ° | 0 | Base en cono para que la torre no se caiga |
| `wipe_tower.extra_spacing` | % | 100 % | Separación de las líneas de purga |
| `wipe_tower.extra_flow` | % | 100 % | Flujo de las líneas de purga |
| `wipe_tower.no_sparse_layers` | — | no | Sin torre en las capas sin cambio de herramienta |
| `wipe_tower.purge_volumes` | mm³ por par de filamentos | 140 entre distintos, 0 en la diagonal | Cuánto purgar al pasar de un filamento a otro |
| `material.purge_multiplier` | % | 100 % | Multiplica el volumen de purga del filamento |
| `material.min_purge_on_tower` | mm³ | — | Lo mínimo que se purga en la torre antes de purgar en el relleno o en un objeto |
| `object.wipe_into_infill`, `object.wipe_into_object` | — | no | Purgar en el relleno de ese objeto, o en todo ese objeto |
| `mmu.cooling_tube_length`, `mmu.cooling_tube_position` | mm | 5 / 91,5 | Tubo de enfriamiento de la MMU |
| `mmu.parking_position` | mm | 92 | Cuánto se saca el filamento para estacionarlo |
| `mmu.extra_loading_move` | mm | −2 | Corrección de la carga |
| `mmu.high_current_on_swap` | — | no | Más corriente en el motor durante el cambio |
| `material.loading_speed`, `material.loading_speed_start`, `material.unloading_speed`, `material.unloading_speed_start` | mm/s | 28 / 3 / 90 / 100 | Velocidades de carga y descarga |
| `material.cooling_moves`, `material.cooling_initial_speed`, `material.cooling_final_speed` | —, mm/s | 4 / 2,2 / 3,4 | Movimientos de enfriamiento de la punta |
| `material.ramming` | lista | la del perfil | Caudal de la compresión (ramming) antes de descargar, en pasos de tiempo |
| `material.toolchange_delay`, `material.load_time`, `material.unload_time` | s | 0 | Esperas y tiempos para la estimación |

## Reglas

### Qué extrusor imprime qué

1. **Por parte:** cada objeto o parte tiene su extrusor; las regiones pintadas de un color usan el de ese color
   (`model_tools.md`).
2. **Por tipo:** `extruders.walls`, `extruders.infill`, etc. reemplazan el del objeto para ese tipo (prueba ME3).
3. **Orden en la capa:** cada capa empieza con el extrusor con el que terminó la anterior, así hay a lo sumo un cambio
   por extrusor y por capa. Con dos extrusores, cada capa empieza con el que terminó la de abajo y hace un cambio
   (prueba ME1).
4. **Temperatura de la cama:** la de `extruders.bed_temperature_from`; con 0, la del primer extrusor que imprime en la
   primera capa (para la primera) y en la segunda (para el resto) (prueba ME3).

### Cambio de herramienta (varias boquillas)

5. **Secuencia** (prueba ME2):
   1. retracción de `retraction.toolchange_length` en la herramienta que deja;
   2. el G-code de fin de su filamento;
   3. `gcode.toolchange`; si ese texto ya tiene el comando de cambio a la herramienta correcta (`T{next_extruder}`), no
      se agrega otro; si no, se escribe `T<n>`;
   4. el G-code de inicio del filamento nuevo;
   5. el viaje y la vuelta del filamento. Cuando una herramienta se vuelve a usar, su vuelta es
      `retraction.toolchange_length` + `retraction.toolchange_restart_extra`.
6. **Primera herramienta:** al empezar también se corre `gcode.toolchange`, con `previous_extruder` = −1 (prueba
   ME2).
7. **Espera con menos temperatura:** con `extruders.ooze_prevention`, los extrusores que no imprimen se ponen a su
   temperatura más `extruders.standby_delta`: al empezar, y antes de cada cambio la herramienta que deja (`M104 …
   ;cooldown`); la que entra espera su temperatura con `M109` (prueba ME4).
8. **Desfase:** las coordenadas de una herramienta se escriben restando su `machine.extruder_offset` (prueba ME5).

### Torre de purga

9. **Requisito:** Tisma Slicer solo la hace con E relativa (`M83`); con E absoluta no lamina (prueba ME6).
10. **Forma:** un rectángulo de `wipe_tower.width` de ancho, con la esquina en (`wipe_tower.x`, `wipe_tower.y`), de la
    profundidad que hace falta para la purga de la capa con más purga; `wipe_tower.brim_width` de borde en la primera
    capa (prueba ME6).
11. **Capas sin cambio:** la torre se imprime rala (relleno con apoyos cada `wipe_tower.bridging`) hasta la última capa
    con cambio. Con `wipe_tower.no_sparse_layers`, esas capas no llevan torre y la boquilla baja hasta ella cuando hace
    falta (prueba ME7).
12. **Purga:** con varias boquillas, la torre solo ceba y limpia (los volúmenes de purga no cambian nada). Con MMU, cada
    cambio purga el volumen de `wipe_tower.purge_volumes` para ese par, por `material.purge_multiplier`; la torre
    crece en profundidad con el volumen. `wipe_tower.extra_spacing` separa las líneas (torre más honda) y
    `wipe_tower.extra_flow` las hace más gruesas (menos largo) (prueba ME6).
13. **Purgar en la pieza:** con `object.wipe_into_infill`, parte de la purga se hace imprimiendo el relleno de ese
    objeto con el filamento nuevo; con `object.wipe_into_object`, en el objeto entero. Antes se purga en la torre al
    menos `material.min_purge_on_tower` (prueba ME8).
14. **Cono:** según la documentación, `wipe_tower.cone_angle` agrega una base en cono que ensancha la torre abajo. En
    las piezas de prueba no cambió nada; no se midió.

### Un extrusor con varios filamentos (MMU)

15. **Cambio** (prueba ME9), entre `; CP TOOLCHANGE START` y `; CP TOOLCHANGE WIPE`, sobre la torre:
    1. **compresión (ramming):** tramos sobre la torre con caudal creciente según `material.ramming`, cordón de 0,6;
    2. **descarga:** retracción rápida en pasos, de `material.unloading_speed_start` a `material.unloading_speed` y
       más lenta al final, hasta sacar `mmu.parking_position` más el tubo;
    3. **enfriamiento:** `material.cooling_moves` idas y vueltas de ±`mmu.cooling_tube_length` en el tubo, con la
       velocidad del filamento de `material.cooling_initial_speed` a `material.cooling_final_speed`;
    4. la retracción de cambio, el fin del filamento viejo, `T<n>` y el inicio del nuevo;
    5. **carga:** empuja `mmu.parking_position` + `mmu.extra_loading_move`, primero lento
       (`material.loading_speed_start`), después rápido y al final más lento;
    6. **purga:** líneas sobre la torre con el volumen de la regla 12.

### Materiales que se tocan

16. **Cáscaras entre materiales:** con `surfaces.interface_shells`, donde un material apoya sobre otro, la parte de
    abajo lleva sus capas sólidas de arriba y la de arriba sus capas sólidas de abajo, como si fueran piezas separadas
    (prueba ME10).
17. **Vigas de entrelazado:** con `interlocking.beams`, donde se tocan dos materiales cada uno entra en el otro con
    vigas de `interlocking.beam_width`, que cambian de dirección cada `interlocking.beam_layers` capas (prueba ME11).
18. **Regiones pintadas:** según la documentación, `painting.max_region_width` limita el ancho de una región pintada
    (lo de adentro queda del material de la pieza) y `painting.interlocking_depth` las entrelaza. No se midió: hace
    falta pintar en la interfaz (`model_tools.md`).

### Capas

19. **Soporte sincronizado:** con `support.synchronize_layers`, el soporte usa las capas de la pieza en vez de las
    suyas (prueba ME12).
20. **Altura máxima por extrusor:** `machine.max_layer_height` limita la altura de las capas del soporte (y de la capa
    variable); con 0 vale el 75 % de la boquilla (prueba ME12).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `walls_extra.md` (boquilla 0,4, capas
de 0,2, 2 perímetros, sin relleno, sin falda ni borde, centro en (100, 100)), dos extrusores de 0,4 mm y filamento de
1,75. **Dos colores:** dos mitades de 10 × 20 × 10 mm lado a lado (X 90 a 100 con el extrusor 1, X 100 a 110 con el
2), en un 3MF. **Apilada:** un bloque de 20 × 20 × 5 mm con el extrusor 1 y otro igual encima con el 2.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| ME1 | Dos colores, temperaturas 200 / 220 | — | `M104 S200 T0`, `M104 S220 T1`, `M109` de las dos, `T0`; después la capa 0 con T0, `T1`, capa 1 con T1, `T0`… 51 comandos `T` en 50 capas |
| ME2 | Dos colores | `retraction.toolchange_length` 4, `retraction.toolchange_restart_extra` 1, `gcode.toolchange` `;TC [previous_extruder]->[next_extruder] at {toolchange_z}` | `G1 E-4`, fin del filamento, `;TC 0->1 at 0.2`, `T1`, `M107`, inicio del filamento. Al volver a T0: `G1 E5`. Con `;TC {previous_extruder}` y `T{next_extruder} ; mine`: `;TC -1` y `T0 ; mine` al empezar, `;TC 0` y `T1 ; mine` después, sin otro `T` |
| ME3 | Cubo de 20 mm, relleno al 20 %, 3 capas sólidas; cama 55/50 (extrusor 1) y 75/70 (extrusor 2) en la primera capa / el resto | `extruders.walls` 1, `extruders.infill` 2, `extruders.solid_infill` 2 | Perímetros con T0; relleno, sólido y superficie con T1; la capa 5 empieza con el relleno (T1). Cama `M190 S55` y desde la capa 2 `M140 S70`. Todo con el extrusor 2 y `extruders.bed_temperature_from` 1: 55 y 50. Todo con el 2 y 0: 75 y 70 |
| ME4 | Dos colores, 200 / 220 | `extruders.ooze_prevention`, `extruders.standby_delta` −30 | Al empezar `M104 S190 T1`; antes de cada cambio `M104 S170 T0 ;cooldown`, `T1`, `M109 S220 T1` |
| ME5 | Dos colores | `machine.extruder_offset` 0x0, 10x5 | La mitad del extrusor 2 pasa de X 100,2–109,8 e Y 90,2–109,8 a X 90,2–99,8 e Y 85,2–104,8 |
| ME6 | Dos colores con torre | E absoluta; E relativa; MMU; ancho 40; borde 5; separación 150 %; flujo 150 %; volúmenes 300; multiplicador 50 % | E absoluta: «The Wipe Tower is currently only supported with the relative extruder addressing», no lamina. Varias boquillas: torre de X 180 a 240 e Y 140 a 143, 303,6 mm por capa, igual con volúmenes de 300. MMU: Y 140 a 154,4, 2038,1 mm por capa; con 300 mm³: hasta 169,4, 3840,4 mm. Ancho 40: X 180 a 220, hasta Y 162. Borde 5: la capa 0 desde X 175,0 (con 2: 178,2). Separación 150 %: hasta Y 161,35. Flujo 150 %: 1561,7 mm. Multiplicador 50 %: hasta Y 147,9, 1257,7 mm |
| ME7 | Apilada, MMU, torre | `wipe_tower.no_sparse_layers` no / sí | No: torre de 2388 mm en la capa 0, 387,4 mm (rala) en las capas 1 a 24, 2089,6 en la 25 (el cambio), nada encima. Sí: capas 1 a 24 sin torre |
| ME8 | Dos colores, MMU, torre, relleno al 20 % | `object.wipe_into_infill`; `object.wipe_into_object` | Capa 10: torre de 2038,1 mm sin nada, 1978,6 con purga en el relleno; con purga en el objeto, sin torre en esa capa |
| ME9 | Dos colores, MMU | Valores por defecto (descarga 100 → 90 mm/s, estacionar 92, tubo 5, carga 3 → 28 mm/s, enfriamiento 2,2 → 3,4 mm/s) | Compresión con F de 3554 a 5385 y cordón 0,6; `E-15 F6000`, `E-55.3 F5400`, `E-15.8 F2700`, `E-7.9 F1620` (94 mm); 4 idas y vueltas `E5`/`E-5` sobre 49,3 mm de X, de F1302 a F2013; `E2`, `E-10`, `T0`; carga `E18 F180`, `E63`, `E9` (90 mm); después la purga |
| ME10 | Apilada, relleno al 20 %, 3 capas sólidas | `surfaces.interface_shells` no / sí | No: capas 21 a 27 con relleno ralo en los dos materiales. Sí: capa 22 puente interno de 757 mm, 23 sólida, 24 superficie de arriba (extrusor 1); 25 a 27 sólidas (extrusor 2) |
| ME11 | Dos colores, relleno al 20 % | `interlocking.beams` | El extrusor 1 llega hasta X 102,48 (sin vigas: 99,8); el perímetro externo de cada mitad pasa de 58,3 a 119 mm. Capas 10 y 11 iguales, la 12 distinta |
| ME12 | Voladizo de `supports_skirt.md` | `support.synchronize_layers` con `support.z_gap` 0; `machine.max_layer_height` 0,25 | Sin sincronizar: 70 capas (soporte en Z 0,49, 0,8, 1,08…). Sincronizado: 50 capas, soporte en Z 0,2, 0,4, 0,6… Altura máxima 0,25: capas del soporte de 0,247 (sin el ajuste, 0,296) |
