# Recorrido que ahorra tiempo

**Qué hace:** baja el tiempo de impresión solo con el recorrido: el orden en que se imprime cada cosa, hacia dónde va
cada línea, por dónde viaja la boquilla, cuándo retrae y cómo pasa por las esquinas. No cambia el perfil: las mismas
capas, paredes, rellenos, anchos, velocidades y aceleraciones, y el mismo material en el mismo lugar.

**Pedido del dueño (26-10-2026):** mejorar el recorrido (orden, dirección y viajes) para reducir el tiempo, sin
cambiar los perfiles ni la impresora.

**Fase:** con F5 (G-code); amplía `gcode_motion.md` (regla 8, orden dentro de la capa) y `retraction_travel.md`
(reglas 8 y 9, viajes por dentro y sentido de los lazos).

**Fuentes:**
- lo medido en `qa/2026-10-26-tiempo-de-impresion.md` (motor en `5b0bdbd` contra Tisma Slicer);
- Ganganath y otros (2016), «Trajectory planning for 3D printing: a revisit to traveling salesman problem», ICCAR:
  el orden de las islas y los tramos como un problema del viajante;
- Fok y otros (2019), «An ACO-based tool-path optimizer for 3-D printing applications», *IEEE Transactions on
  Industrial Informatics*: orden por colonia de hormigas, comparado con el vecino más cercano;
- Zhao y otros (2016), «Connected Fermat spirals for layered fabrication», *ACM TOG*: caminos continuos;
- Tajima y Sencer (2016), «Kinematic corner smoothing for high speed machine tools», *International Journal of
  Machine Tools and Manufacture*: esquinas suavizadas dentro de una tolerancia.

## Lo que se midió

En el Benchy, con el mismo perfil, el motor hace 8 004 viajes y Tisma Slicer 4 876:

| Viajes | Motor | Tisma Slicer |
|---|---|---|
| De menos de 1 mm | 4 392 | 2 103 |
| Entre líneas de superficie de arriba | 594 | 24 |
| Entre líneas de puente | 213 | 15 |
| Después de un perímetro interno | 4 194 | 2 684 |
| Tiempo de viajes y retracciones (modelo simple: viaje a 180 mm/s, con 0,14 s de aceleración y frenada por viaje; retracción de 2 mm a 25 mm/s) | unos 27 de 72 min | unos 22 min |

En el conejo al 150 %, el motor viaja 243,6 m y Tisma Slicer 189,6 m.

## Reglas

1. **Líneas vecinas unidas:** en relleno sólido, superficie de arriba, puentes y relleno disperso de líneas, dos líneas
   paralelas seguidas se unen con un tramo que se extruye a lo largo del borde (zigzag), sin viajar, cuando:
   - el tramo de unión va por dentro de la zona y mide como mucho 2 veces el ancho de la línea más el espacio entre
     líneas;
   - el orden monótono de `surfaces.top_pattern` (todas las líneas en el mismo sentido) se respeta: en ese caso se une
     solo cuando la línea siguiente va en el mismo sentido.
2. **Orden de islas y de piezas:** en cada capa, el orden de las islas (y de las piezas de la placa) se elige para
   que los viajes sumen lo menos posible: vecino más cercano y después mejoras de a dos (intercambiar dos tramos del
   recorrido mientras acorte). La capa empieza cerca de donde terminó la anterior (`gcode_motion.md`, regla 8).
3. **Inicio y sentido de cada línea suelta:** cada línea abierta se imprime desde su punta más cercana a la boquilla.
   El sentido de los lazos sigue `retraction_travel.md`, regla 9.
4. **Lazos de pared:** cada lazo empieza en la costura que pide `walls.seam`. Entre lazos de la misma isla, el viaje va
   por dentro y sin retraer cuando no cruza una pared externa.
5. **Retraer solo cuando hace falta:** un viaje que queda entero dentro de la pieza (por zonas que se rellenan, sin
   cruzar una pared externa ni aire) no retrae, aunque sea más largo que `retraction.min_travel`. Los demás siguen la
   regla 5 de `gcode_motion.md`.
6. **Viaje al cambiar de zona:** de un relleno al siguiente, o del relleno a la pared, la zona siguiente se elige y se
   empieza por el punto más cercano, dentro de lo que permite la regla 8 de `gcode_motion.md`.
7. **Esquinas** (opcional, `gcode.corner_smoothing`, mm, defecto 0 = apagado, de 0 a 0,05): una esquina de una pared
   interna o de un relleno se redondea con un arco que no se aparta más que este valor; la impresora no tiene que
   frenar del todo en el vértice. Nunca en la pared externa (cambiaría la forma visible).
8. **Lo que no cambia:** las líneas que se extruyen son las mismas (el mismo largo extruido, con una diferencia de
   menos de 1 % por las uniones de la regla 1), con el mismo ancho, alto y velocidad.

## Pruebas

Benchy y conejo al 150 % de `qa/2026-10-26-velocidad.md`, con el perfil por defecto, contra el motor de `5b0bdbd`.
Las metas están en `BENCHMARKS.md` (RT1 a RT6).

| # | Escenario | Resultado esperado |
|---|---|---|
| TT1 | Benchy: viajes de menos de 1 mm | 2 100 o menos (hoy 4 392) |
| TT2 | Benchy: viajes entre líneas de superficie de arriba y de puente | 100 o menos (hoy 807) |
| TT3 | Conejo: largo de los viajes | 190 m o menos (hoy 243,6) |
| TT4 | Benchy: tiempo estimado con el mismo perfil | Al menos 8 % menos que hoy (1 h 12 m 28 s) |
| TT5 | Largo extruido por tipo de línea, antes y después | Igual, con menos de 1 % de diferencia |
| TT6 | Cubo de 20 mm con relleno: viajes que no cruzan la pared externa | Ninguno retrae |
| TT7 | `gcode.corner_smoothing` 0,02 en el Benchy | La pared externa no cambia; ninguna esquina interna se aparta más de 0,02 mm |
