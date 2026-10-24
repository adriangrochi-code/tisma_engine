# Superficies y relleno

**Qué hace:** llena lo que queda adentro de los perímetros:
- capas sólidas cerca de las superficies de arriba y de abajo;
- puentes sobre el aire o sobre relleno poco denso;
- relleno poco denso en el resto.

**Fase:** F3.

**Fuentes:**
- `perimeters.md`: modelo del cordón y separación `w − h·(1 − π/4)`.
- Superficie giroide (ver `SOURCES.md`).

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `top_solid_layers`, `bottom_solid_layers` | — | 3 / 3 | Capas sólidas desde cada superficie de arriba y de abajo |
| `fill_density` | % | 20 % | Densidad del relleno poco denso |
| `fill_pattern` | — | ver `SETTINGS_V1.md` | `rectilinear`, `grid` o `gyroid` en la primera versión |
| `top_fill_pattern`, `bottom_fill_pattern` | — | `monotonic` | `monotonic` o `rectilinear` |
| `fill_angle` | ° | 45 | Dirección base de las líneas |
| `infill_overlap` | % de la separación de los perímetros | 25 % | Cuánto se mete el relleno en el último perímetro |
| `solid_infill_below_area` | mm² | 70 | Una zona de relleno más chica que esto se llena sólida |
| `infill_anchor_max` | mm | 50 | Largo máximo del tramo que une dos líneas de relleno siguiendo el perímetro |
| `bridge_angle` | ° | 0 = automático | Dirección de los puentes |
| `solid_infill_width`, `infill_width` | mm | 0: 1,125 × boquilla | Ancho de los cordones del relleno sólido y del poco denso |
| `top_infill_width` | mm | 0: el diámetro de la boquilla | Ancho de la capa de arriba de todo |

## Reglas

1. **Capas sólidas:**
   - desde cada superficie de abajo, `bottom_solid_layers` capas sólidas; desde cada superficie de arriba,
     `top_solid_layers`;
   - las líneas van a `fill_angle` y giran 90° en cada capa;
   - la separación es la de los cordones (`perimeters.md`): 0,407 mm con 0,45 de ancho y 0,2 de alto, y 0,358 mm en la
     primera capa (ancho 0,4).
2. **Capa de arriba de todo** (la última capa sólida bajo el aire): su ancho es el diámetro de la boquilla (0,4 mm)
   y su separación 0,359 mm.
3. **`monotonic`:**
   - todas las líneas de la superficie se recorren en un solo barrido, de un lado al otro;
   - una línea se imprime siempre después de su vecina del lado en el que empezó el barrido, aunque un agujero corte
     la superficie en pedazos;
   - el sentido de cada línea alterna (ida y vuelta).
4. **`rectilinear` en superficies:** también ida y vuelta, pero los pedazos que separa un agujero pueden imprimirse en
   cualquier orden (prueba I8).
5. **Puentes sobre el aire:**
   - la primera capa de una superficie de abajo que no apoya en nada es un puente;
   - las líneas cruzan el vano más corto, de un apoyo al otro (pruebas I6 e I7);
   - ancho 0,40 mm, igual a la boquilla, y separación 0,452 mm, con un espacio de 0,05 mm entre líneas;
   - los perímetros de esa capa que quedan sobre el aire se marcan `;TYPE:Overhang perimeter`;
   - el puente cuenta como la primera de las `bottom_solid_layers`.
6. **Puente sobre relleno:** la primera capa sólida que apoya sobre relleno poco denso también se imprime como puente:
   mismo ancho y separación que el punto 5, con las líneas cruzando las del relleno de abajo (prueba I1). Después
   siguen las demás capas sólidas.
7. **Relleno poco denso:**
   - `rectilinear`: una dirección por capa, `fill_angle` y `fill_angle` + 90° alternadas. Separación entre líneas =
     separación de los cordones / densidad (2,035 mm al 20 %, 1,018 mm al 40 %);
   - `grid`: las dos direcciones en cada capa, cada una con el doble de separación (4,071 mm al 20 %);
   - `gyroid`: la superficie giroide cortada en la Z de cada capa, con la escala que da la densidad pedida;
   - las líneas se unen de a dos siguiendo el último perímetro (tramos de hasta `infill_anchor_max`).
8. **Borde del relleno:**
   - el relleno se acerca al último perímetro como si fuera un cordón más, y se mete además `infill_overlap` (25 % de
     0,407 = 0,10 mm);
   - en la prueba I1, el eje del relleno queda a 0,305 mm del eje del último perímetro.
9. **Zonas chicas:** una zona de relleno de menos de `solid_infill_below_area` se llena sólida (prueba I6: las patas de
   la mesa).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, desde la línea de comandos.

**Ajustes base:**
- boquilla 0,4; capas de 0,2;
- `perimeters` 2; `top_solid_layers` 3; `bottom_solid_layers` 3; `fill_density` 20 %; `fill_pattern` `rectilinear`;
- sin falda ni borde; pieza centrada en X = 100, Y = 100.

Los ángulos se miden desde +X. «Capa n» cuenta desde 1.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| I1 | Cubo de 20 × 20 × 10 mm (50 capas) | — | Capas 1 a 3: sólidas (45°, 135°, 45°). Capas 4 a 47: relleno al 20 %. Capa 48: puente sobre el relleno a 135°, ancho 0,40, separación 0,452. Capa 49: sólida a 45°. Capa 50: la de arriba de todo, a 135°, ancho 0,40, separación 0,359 |
| I2 | Cubo de I1 | — | Relleno de la capa 11: líneas a 45°, separación 2,035 mm; capa 12: 135°. Largo por capa: 197,3 mm. El eje del relleno llega a 0,305 mm del eje del último perímetro |
| I3 | Cubo de I1 | `fill_density` 40 % | Separación 1,018 mm; largo por capa 358,4 mm |
| I4 | Cubo de I1 | `fill_pattern` `grid` | En cada capa, líneas a 45° y a 135°, cada dirección con separación 4,071 mm; largo por capa 191,6 mm |
| I5 | Cubo de I1 | `fill_pattern` `gyroid` | Largo por capa 176,7 mm |
| I6 | «Mesa»: dos patas de 5 × 20 × 5 mm en los extremos de X y una tabla de 20 × 20 × 2 mm encima (vano libre de 10 mm en X) | — | Patas: sólidas en todas sus capas (zona de relleno de unos 60 mm²). Capa 26 (primera de la tabla): puente con líneas a 0° (cruzan el vano en X), ancho 0,40, separación 0,452; perímetros sobre el aire marcados como «Overhang perimeter». Capas 27 y 28: sólidas sobre el vano; relleno sobre las patas |
| I7 | La mesa de I6 girada 90° (vano de 10 mm en Y) | — | Puente con líneas a 90° |
| I8 | Marco de 20 × 20 mm con un agujero de 10 × 10 mm, alto 5 mm | `top_fill_pattern` `monotonic`, y después `rectilinear` | Última capa, líneas a 45°: con `monotonic`, las 112 líneas van en orden de un lado al otro, con un solo salto hacia atrás, al pasar al otro lado del agujero. Con `rectilinear`, el orden cambia de sentido 55 veces |
