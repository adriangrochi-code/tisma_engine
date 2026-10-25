# Retracción y viajes: opciones adicionales

**Qué hace:** lo de la retracción y los viajes que no está en `gcode_motion.md`:
- velocidad y extra de la vuelta del filamento;
- retraer siempre al cambiar de capa;
- barrer con la boquilla mientras retrae;
- saltos en Z en rampa, y solo por encima o por debajo de una altura;
- retracción hecha por el firmware;
- viajes que no cruzan las paredes;
- imprimir los lazos en sentido horario.

**Fase:** después de F4.

**Fuente del comportamiento:** documentación de usuario de los slicers (base de conocimiento de Prusa), los comandos
`G10` y `G11` de [Marlin](https://marlinfw.org/docs/gcode/G010.html) y
[RepRapFirmware](https://docs.duet3d.com/User_manual/Reference/Gcodes), y las mediciones de abajo.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `retraction.unretract_speed` | mm/s | 0 = `retraction.speed` | Velocidad de la vuelta del filamento |
| `retraction.restart_extra` | mm | 0 | Filamento de más al volver |
| `retraction.on_layer_change` | — | no | Retraer en todo cambio de capa, aunque el viaje sea corto |
| `retraction.wipe` | — | no | La boquilla vuelve sobre lo recién impreso mientras retrae |
| `retraction.before_wipe` | % | 0 % | Parte de la retracción que se hace quieta, antes del barrido |
| `retraction.lift_above`, `retraction.lift_below` | mm | 0 = sin límite | El salto en Z solo por encima, o solo por debajo, de esa altura |
| `travel.ramping_lift` | — | no | El salto en Z se hace en rampa, viajando |
| `travel.slope` | ° | 0 | Pendiente de la rampa |
| `travel.max_lift` | mm | 0 | Altura máxima de la rampa |
| `travel.lift_before_obstacle` | — | no | Rampa más empinada si hay un obstáculo cerca del comienzo del viaje |
| `retraction.firmware` | — | no | Retraer con `G10` y volver con `G11` |
| `travel.avoid_crossing_walls` | — | no | Los viajes van por dentro de la pieza sin cruzar los perímetros |
| `travel.max_detour` | mm o % del viaje recto | 0 = sin límite | Si el desvío es más largo, el viaje va recto |
| `walls.clockwise` | — | no | Los lazos se imprimen en sentido horario |

## Reglas

1. **Vuelta:** la vuelta del filamento va a `retraction.unretract_speed` y empuja la retracción más
   `retraction.restart_extra` (prueba R1).
2. **Cambio de capa:** con `retraction.on_layer_change`, todo cambio de capa retrae, aunque el viaje al primer punto sea
   más corto que `retraction.min_travel` (prueba R2).
3. **Barrido:** con `retraction.wipe`, en vez de retraer quieta, la boquilla vuelve sobre el camino recién impreso
   mientras retrae:
   - a la velocidad de barrido, el 80 % de `speed.travel`;
   - primero retrae quieta `retraction.before_wipe` de la retracción, a `retraction.speed`;
   - el resto lo retrae durante el barrido, que mide `resto · velocidad de barrido / (0,95 · retraction.speed)`;
   - después sigue el viaje, el salto en Z y la vuelta de siempre (prueba R3).
4. **Alturas del salto:** el salto en Z (`retraction.lift`) se hace solo si la capa está por encima de
   `retraction.lift_above` y por debajo de `retraction.lift_below` (los que no son 0) (prueba R4).
5. **Rampa:** con `travel.ramping_lift`, en vez de subir en vertical, la boquilla sube mientras viaja, con la pendiente
   `travel.slope`, hasta `travel.max_lift`; el resto del viaje va a esa altura y al final baja en vertical. Si el viaje
   es corto, sube hasta donde llega y baja (prueba R5).
6. **Obstáculos:** con `travel.lift_before_obstacle`, si la rampa puede chocar con algo al comienzo del viaje, se hace
   más empinada. No se midió.
7. **Retracción del firmware:** con `retraction.firmware`, cada retracción es un `G10` y cada vuelta un `G11`, sin
   movimientos de E; el largo y la velocidad los pone el firmware. El salto en Z sigue en el G-code. No se puede usar
   con `retraction.wipe`: Tisma Slicer rechaza la combinación (prueba R6).
8. **Viajes por dentro:** con `travel.avoid_crossing_walls`, un viaje dentro de una isla sigue un camino por dentro de
   la pieza que no cruza los perímetros externos (por ejemplo, rodea un agujero en vez de cruzarlo). Si ese camino es
   más largo que el recto más `travel.max_detour`, el viaje va recto (prueba R7).
9. **Sentido:** los lazos de perímetro van en sentido antihorario, también los de los agujeros. Con `walls.clockwise`,
   todos en sentido horario (prueba R8).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `walls_extra.md` (boquilla 0,4, capas
de 0,2, 2 perímetros, sin capas sólidas, sin falda ni borde, centro en (100, 100)), retracción de 2 mm a 40 mm/s,
`retraction.min_travel` 2 y viajes a 130 mm/s. **Cubo:** 20 mm, relleno al 20 %. **Tubo:** 10 mm de radio con un
agujero de 5.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| R1 | Cubo, capa 5 | `retraction.unretract_speed` 20, `retraction.restart_extra` 0,5 | Retracción `E-2` a F2400; vuelta `E+2.5` a F1200 |
| R2 | Cilindro de 10 mm de radio, costura `aligned` | `retraction.on_layer_change` no / sí | No: el cambio de capa viaja 0,53 mm sin retraer; 3 retracciones en toda la pieza. Sí: retrae 2 mm antes de subir y vuelve después; 51 retracciones |
| R3 | Cubo, capa 5 | `retraction.wipe` | Retracción de 2 mm durante 5,47 mm de barrido a 104 mm/s (F6240) sobre el lazo recién impreso. Con `retraction.speed` 20: 10,94 mm; con 80: 2,73; con viajes a 200 mm/s: 8,42 mm a 160 mm/s; con retracción de 1 mm: 2,73. Con `retraction.before_wipe` 50 %: `E-1` quieta y 1 mm durante 2,73 mm de barrido |
| R4 | Cubo, `retraction.lift` 0,4 | `retraction.lift_above` 2 / `retraction.lift_below` 1 | Sin límites: salto de 0,4 en las capas 2 (Z 0,6) y 12 (Z 2,6). Encima de 2: solo en la 12. Debajo de 1: solo en la 2 |
| R5 | Cubo, capa 5 | `travel.ramping_lift`, `travel.slope` 10°, `travel.max_lift` 1 | Cambio de capa: sube de Z 1,0 a 2,0 en los primeros 5,67 mm del viaje (tan 10° · 5,67 = 1,0), sigue 8,57 mm a Z 2,0 y baja a Z 1,2. Viaje de 3,12 mm: sube hasta Z 1,751 (0,551 más) y baja a Z 1,2 |
| R6 | Cubo, `retraction.lift` 0,4 | `retraction.firmware` | 151 `G10` y 150 `G11`, sin E; el salto en Z con `G1 Z`. Con `retraction.wipe`: error de configuración, no lamina |
| R7 | Tubo, relleno al 20 %, capa 10 | `travel.avoid_crossing_walls`; `travel.max_detour` | Sin el ajuste: 8 viajes, 33,5 mm, 3 cruces de perímetros externos. Con el ajuste: viajes curvos de 90 tramos, 39,0 mm, 1 cruce. Con `travel.max_detour` 1 mm: 32 tramos, 34,1 mm. Con 200 %: igual que sin límite |
| R8 | Tubo, capa 10 | `walls.clockwise` no / sí | No: los cuatro lazos (dos alrededor del agujero, dos afuera) con área con signo positiva (antihorario). Sí: los cuatro negativos (horario) |
