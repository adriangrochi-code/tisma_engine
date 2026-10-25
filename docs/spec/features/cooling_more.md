# Enfriamiento: opciones adicionales

**Qué hace:** lo del enfriamiento que no está en `cooling.md`:
- subir el ventilador de a poco en las primeras capas;
- más ventilador en los voladizos, según cuánto se apoya el cordón;
- frenar primero lo que no se ve, para que la superficie quede pareja;
- volver a la velocidad normal un poco antes de terminar la capa.

**Fase:** después de F4.

**Fuente del comportamiento:** documentación de usuario de los slicers (base de conocimiento de Prusa) y las mediciones
de abajo.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `cooling.full_fan_layer` | capa | 0 = no | Capa (contada desde 1) en la que el ventilador llega a su valor; antes sube de a poco |
| `cooling.dynamic_overhang_fan` | — | no | Ventilador según el apoyo del cordón |
| `cooling.overhang_fan_0`, `_1`, `_2`, `_3` | % | 0, 0, 0, 0 | Ventilador con el 0 %, 25 %, 50 % y 75 % del ancho del cordón apoyado |
| `cooling.slowdown_logic` | — | `uniform` | `uniform`: el frenado de `cooling.md` baja todo por igual. `consistent_surface`: primero lo que no se ve |
| `cooling.transition_distance` | mm | 0 | Con `consistent_surface`, los últimos mm de la capa vuelven a su velocidad normal |

## Reglas

1. **Subida gradual:** con `cooling.full_fan_layer` = F mayor que `cooling.fan_off_layers` = D, la capa número n
   (contada desde 1) lleva el ventilador que le toca por `cooling.md` multiplicado por `(n − D) / (F − D)`, entre la
   capa D + 1 y la F. Después, el valor completo. Si F no pasa de D, el ventilador llega al valor completo en la capa
   D + 1 (prueba C1).
2. **Apoyo del cordón:** como en `speeds_accels.md`, regla 6: `100 · (1 − d / w)` %.
3. **Ventilador en voladizos:** con `cooling.dynamic_overhang_fan`, mientras se imprime un perímetro con poco apoyo, el
   ventilador es `cooling.overhang_fan_0` con 0 %, `_1` con 25 %, `_2` con 50 % y `_3` con 75 %, interpolado en línea
   recta entre esos puntos (prueba C2).
4. **Entre 75 % y 100 %:** el valor baja en línea recta desde `cooling.overhang_fan_3` hasta 0 con 100 %. En todos los
   casos vale el mayor entre ese valor y el ventilador de la capa. En Tisma Slicer, si el ventilador de la capa está
   apagado del todo, los tramos con más del 75 % de apoyo no lo encienden (prueba C2).
5. **Frenado `consistent_surface`:** cuando la capa necesita frenarse (`cooling.md`, regla 2), primero se frenan todas
   las extrusiones salvo el perímetro externo y el perímetro que lo toca; esos dos se frenan solo si con lo demás no
   alcanza (prueba C3).
6. **Vuelta a la velocidad:** con `consistent_surface` y `cooling.transition_distance` = d, los últimos d mm de
   extrusión de la capa vuelven a su velocidad sin frenar (prueba C4).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `walls_extra.md` (boquilla 0,4, capas
de 0,2, 2 perímetros, sin relleno, sin capas sólidas, sin falda ni borde, centro en (100, 100)). **Cubo:** 20 mm.
**Embudo** y **hongos:** los de `speeds_accels.md`. El valor de `M106` que manda en cada capa se escribe justo antes
de su `;LAYER_CHANGE`.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| C1 | Cubo | `cooling.fan_off_layers` 3, `cooling.fan_always` sí; `cooling.full_fan_layer` 8 | Con ventilador de 100 %: capas 1 a 3 apagado; capa 4 al 20 % (`M106 S51`), 5 al 40, 6 al 60, 7 al 80, 8 en adelante al 100. Con ventilador de 40 %: 8, 16, 24, 32 y 40 %. Sin el ajuste: 100 % desde la capa 4 |
| C2 | Embudos y hongos, capa 30 (embudo) o 60 (hongo); sin frenar por tiempo | `cooling.dynamic_overhang_fan` sí; 100, 80, 60 y 40 % | Perímetro externo con el ventilador de la capa apagado: hongo de 60° (apoyo 23 %) al 81 %; hongo de 55° (37 %) al 70; embudo de 45° (56 %) al 53–55; de 35° (69 %) al 43–44; de 30° (74 %) al 37–40; de 27°, 25°, 22° y 20° (77 a 84 %) apagado. Con el ventilador de la capa al 1 %: embudo de 27° al 32–36, de 25° al 29–30, de 20° al 22–23. Con el de la capa al 40 o 60 %: el de la capa. Los perímetros internos, con el de la capa |
| C3 | Cubo, relleno al 20 %, 3 perímetros, 3 capas sólidas; capa 10 | `cooling.min_layer_time` 15 / 40 | `uniform`, 15 s: todo a 27,93 mm/s. `consistent_surface`, 15 s: el externo (30 mm/s) y el interno que lo toca (60) sin frenar; el otro interno y el relleno a 23,58. 40 s: `uniform` todo a 10,15; `consistent_surface` el relleno y el interno de adentro a 10,0, el externo y el que lo toca a 10,4 |
| C4 | Pieza de C3, 15 s, `consistent_surface` | `cooling.transition_distance` 2 / 5 / 10 | El final del relleno de la capa vuelve a 80 mm/s: los últimos 2,0 / 5,0 / 10,0 mm. Con `uniform`, sin efecto |
