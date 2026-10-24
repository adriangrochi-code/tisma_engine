# Soportes: opciones adicionales y soportes orgánicos

**Qué hace:** todo lo de los soportes que no está en `supports_skirt.md`:
- forzar soporte en las primeras capas;
- soportes solo desde la cama;
- separación e interfaz debajo del soporte, cuando apoya sobre la pieza;
- patrones de la base y de la interfaz, vaina, lazos de contacto y velocidad de la interfaz;
- estilo ajustado («snug») y soportes orgánicos (en árbol);
- bloqueadores y forzadores: volúmenes o caras pintadas que quitan o ponen soporte.

**Fase:** después de F5. Los orgánicos, después de la primera versión (ver `PLAN.md`).

**Fuentes:**
- soportes en árbol: Vanek, Garcia Galicia, Benes, *Clever Support* (2014), en `SOURCES.md`;
- el comportamiento sale de la documentación de usuario de los slicers (base de conocimiento de Prusa) y de las
  mediciones de abajo.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `support.enforce_layers` | capas | 0 | Soporte bajo todo lo que cuelga en las primeras N capas, sin importar el ángulo ni si el soporte está activo |
| `support.buildplate_only` | — | no | Solo soporte que apoya en la cama |
| `support.automatic` | — | sí | No: solo hay soporte dentro de los forzadores |
| `support.style` | — | `grid` | `grid` (sobre una grilla), `snug` (ajustado a lo que cuelga) u `organic` (ramas) |
| `support.closing_radius` | mm | 2 | Con `snug`, los huecos más chicos que esto entre zonas de soporte se cierran |
| `support.bottom_z_gap` | mm | 0 = igual que `support.z_gap` | Separación en Z entre la pieza y el soporte que apoya sobre ella |
| `support.bottom_interface_layers` | capas | −1 = igual que `support.interface_layers` | Capas de interfaz debajo del soporte, sobre la pieza |
| `support.pattern` | — | `lines` | Base: `lines`, `grid` u `honeycomb` |
| `support.interface_pattern` | — | `lines` | Interfaz: `lines`, `concentric` o `automatic` (líneas; concéntrica con material soluble) |
| `support.interface_spacing` | mm | 0 = sólida | Espacio libre entre las líneas de la interfaz |
| `support.interface_contact_loops` | — | no | Lazos sobre el borde de la capa de interfaz de arriba |
| `support.sheath` | — | sí | Un lazo alrededor de la base del soporte |
| `speed.support_interface` | mm/s o % | 100 % | Velocidad de la interfaz; en %, de `speed.support` |
| `support.tree_angle` | ° | 40 | Inclinación máxima de las ramas cuando esquivan la pieza |
| `support.tree_angle_preferred` | ° | 25 | Inclinación de las ramas cuando no tienen que esquivar nada |
| `support.tree_branch_diameter` | mm | 2 | Diámetro de las ramas más finas |
| `support.tree_branch_diameter_angle` | ° | 5 | Cuánto engordan las ramas hacia abajo (0 = mismo grosor) |
| `support.tree_double_wall_diameter` | mm | 3 | Las ramas con más área que un círculo de este diámetro llevan dos paredes; 0 = nunca |
| `support.tree_branch_distance` | — | 1 | Distancia entre las ramas donde tocan la pieza (sin unidad en Tisma Slicer) |
| `support.tree_tip_diameter` | mm | 0,8 | Diámetro de las puntas |
| `support.tree_top_rate` | % | 15 % | Densidad de las puntas bajo la pieza |

## Reglas

### Dónde va el soporte

1. **Primeras capas forzadas:** con `support.enforce_layers` = N, lo que cuelga en las primeras N capas lleva soporte
   aunque no pase el umbral y aunque `support.enabled` esté apagado. En Tisma Slicer es solo base (sin interfaz), en
   las capas 0 a N − 4 (prueba SM1).
2. **Solo desde la cama:** con `support.buildplate_only`, una zona de soporte que en alguna capa toca la pieza por
   debajo se quita entera, también la parte que estaría sobre la cama (prueba SM2).
3. **Automático apagado:** con `support.automatic` en no, solo hay soporte dentro de los forzadores (prueba SM12).
4. **Bloqueadores:** volúmenes (o caras pintadas) donde no va soporte: el soporte que cae dentro se quita (prueba SM12).
5. **Forzadores:** volúmenes (o caras pintadas) donde lo que cuelga lleva soporte, sin importar el umbral. Hace falta
   `support.enabled`; con el automático encendido no cambian nada fuera de ellos (prueba SM12). Pintar las caras es
   una herramienta de `model_tools.md`.

### Soporte sobre la pieza

6. **Separación de abajo:** el soporte que apoya sobre una superficie de la pieza empieza `support.bottom_z_gap` por
   encima de ella (0 = `support.z_gap`). Su primera capa es más gruesa: 0,4 mm en la prueba (prueba SM3).
7. **Interfaz de abajo:** sobre la pieza, las primeras `support.bottom_interface_layers` capas del soporte son de
   interfaz (prueba SM4).
8. **Separación de arriba 0:** con `support.z_gap` = 0, la interfaz llega hasta la capa de abajo de la pieza y la
   primera capa de la pieza se imprime como una capa normal: no como voladizo ni con el cordón redondo de los puentes
   (prueba SM5).

### Base e interfaz

9. **Patrones de la base:**
   - `lines`: líneas paralelas a `support.line_angle` (`supports_skirt.md`);
   - `grid`: las mismas líneas, giradas 90° en capas de soporte alternas;
   - `honeycomb`: panales hexagonales (prueba SM6).
10. **Vaina:** con `support.sheath`, la base de cada zona lleva un lazo cerrado por su borde y las líneas adentro. Sin
    vaina, la base es un solo zigzag (prueba SM7).
11. **Patrones de la interfaz:**
    - `lines` y `automatic`: líneas a 90° de las de la base, a la separación de `support.interface_spacing` (0:
      juntas, una capa sólida);
    - `concentric`: lazos paralelos al borde de la zona, hacia adentro, a la separación de los cordones (prueba SM10).
12. **Lazos de contacto:** con `support.interface_contact_loops`, la capa de interfaz de arriba empieza con lazos
    cerrados por el borde de la zona y el resto va en líneas (prueba SM10).
13. **Velocidad:** la interfaz va a `speed.support_interface` (prueba SM11).

### Estilo ajustado

14. **`snug`:** la zona de soporte sigue lo que cuelga, sin el margen de la grilla (`supports_skirt.md`, regla 2), y
    con la separación `support.xy_gap` de las paredes. Los huecos entre zonas más angostos que `support.closing_radius`
    se cierran y las zonas se juntan (pruebas SM8 y SM9).

### Soportes orgánicos

15. **Forma:** en vez de columnas, ramas redondas que bajan hasta la cama:
    - arriba, puntas de `support.tree_tip_diameter` repartidas bajo lo que cuelga, más juntas cuanto mayor es
      `support.tree_top_rate`;
    - cada punta baja como una rama de `support.tree_branch_diameter` y las ramas cercanas se van juntando;
    - las ramas engordan hacia abajo según `support.tree_branch_diameter_angle`, hasta troncos que apoyan en la cama;
    - según la documentación, se inclinan hasta `support.tree_angle_preferred` para juntarse, y hasta
      `support.tree_angle` cuando tienen que esquivar la pieza; las más gruesas que
      `support.tree_double_wall_diameter` llevan dos paredes. Esto no se midió.
16. **Capas:** usan las mismas capas que la pieza (las del soporte de grilla tienen las suyas, `supports_skirt.md`).
17. **Interfaz:** `support.interface_layers` capas justo bajo lo que cuelga, sin el margen de la grilla, con líneas a
    45° y 135° en capas alternas (prueba SM13).

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, con los ajustes base de `walls_extra.md` (boquilla 0,4, capas
de 0,2, 2 perímetros, sin relleno, sin capas sólidas, sin falda ni borde, centro en (100, 100)) y `support.enabled` en
sí. `speed.support` 60 mm/s.

**Piezas:**
- **voladizo:** columna de 10 × 20 × 10 mm con una losa de 20 × 20 × 2 mm arriba (Z 8 a 10) que sobresale 10 mm en +X
  (la de `supports_skirt.md`, S4). La columna queda en X 90 a 100 e Y 90 a 110;
- **estante:** base de 30 × 20 × 3 mm, una columna de 10 × 20 mm en su punta −X, de Z 3 a 10, y una losa de 30 × 20 ×
  2 mm arriba (Z 10 a 12) que cuelga 20 mm sobre la base;
- **horquilla:** el voladizo con la losa partida en dos brazos de 10 × 9,25 mm separados 1,5 mm en Y;
- **cuña de 70°** de `supports_skirt.md`.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| SM1 | Cuña de 70°, sin soportes | `support.enforce_layers` 10 / 20 | Sin el ajuste, nada. 10: base de soporte en las capas 0 a 6 (Z 0,2 a 1,4), sin interfaz; 492,5 mm en la capa 0 y unos 79 en las demás. 20: capas 0 a 16. Lo mismo con `support.enabled` en sí. Cubo de 20 mm con 10: nada |
| SM2 | Estante | `support.buildplate_only` no / sí | No: soporte sobre la cama alrededor de la base (158 mm por capa, Z 0,2 a 3,3), sobre la base desde Z 3,6 y hasta la interfaz de arriba (Z 9,0 a 9,8); 258,2 mm por capa sobre la base. Sí: ningún soporte; la primera capa de la losa lleva 115,8 mm de `Overhang perimeter` |
| SM3 | Estante | `support.bottom_z_gap` 0 / 1 | 0: la primera capa del soporte sobre la base (arriba en Z 3,0) en Z 3,6, con alto de extrusión 0,4 (separación 0,2). 1: en Z 4,4, alto 0,4 (separación 1,0) |
| SM4 | Estante | `support.bottom_interface_layers` −1 / 1 / 0 | −1: 3 capas de interfaz sobre la base (Z 3,6, 3,9 y 4,2; 1164,6 mm cada una). 1: solo la de Z 3,6. 0: ninguna; en Z 3,6, base de 313,8 mm |
| SM5 | Estante | `support.z_gap` 0,2 / 0 | 0,2: la interfaz termina en Z 9,8; la primera capa de la losa (Z 10,2) lleva `Overhang perimeter` con 0,05224 mm de filamento por mm. 0: la interfaz llega a Z 10,0, la misma capa que la pieza; la primera capa de la losa lleva perímetros normales, 0,03385 mm de filamento por mm |
| SM6 | Voladizo, capa 2 (Z 0,496) | `support.pattern` | `lines`: 158,8 mm, 60 % a 0° (con la vaina). `grid`: en capas de soporte alternas, 141,5 mm con el 79 % a 90° y 158,8 mm a 0°. `honeycomb`: 206 a 211 mm, a 0°, 90°, 60° y 120° |
| SM7 | Voladizo, capa 2 | `support.sheath` sí / no | Sí: un lazo cerrado de 68,9 mm por el borde y 90,0 mm de líneas adentro. No: un solo zigzag de 123,1 mm, 81 % a 0° |
| SM8 | Voladizo, capa 12 | `support.style` `grid` / `snug` | `grid`: base de X 100,0 a 111,4 e Y 88,6 a 111,4 (sobresale 1,4 mm de la losa). `snug`: de X 100,4 a 109,8 e Y 90,2 a 109,8 (el borde de la losa menos medio cordón); 136,6 mm contra 158,8 |
| SM9 | Horquilla, capa 12 | `snug` con `support.closing_radius` 0,5 / 2; `grid` | 0,5: dos zonas separadas, cada una con su vaina de 36,4 mm. 2: una sola zona, vaina de 57,9 mm. `grid`: una zona con el margen de la grilla, vaina de 68,9 mm |
| SM10 | Voladizo, interfaz (Z 7,0 a 7,8) | `support.interface_pattern`, `support.interface_spacing`, `support.interface_contact_loops` | `lines`: 743,0 mm por capa, 98 % a 90°; la última (Z 7,8) 642,5 mm. `automatic`: igual. `concentric`: 15 lazos por capa, cada uno 2,856 mm más largo que el de adentro (0,357 mm entre lazos). Separación 1 mm: 240,9 mm por capa. Lazos de contacto: en Z 7,8, dos lazos cerrados (92,7 y 51,6 mm) y 288,6 mm de líneas; en Z 7,6, un lazo de 71,6 mm y líneas |
| SM11 | Voladizo | `speed.support_interface` 100 % / 50 % | Interfaz a 60 mm/s / 30 mm/s |
| SM12 | Voladizo en 3MF con un volumen más | Bloqueador sobre la mitad −Y de lo que cuelga; forzador sobre la mitad +Y | Bloqueador: soporte solo en Y 100 a 111,4 (sin él, 88,6 a 111,4). Forzador con `support.automatic` no: solo en Y 100 a 111,4. Forzador con `support.automatic` sí: todo, como sin forzador. Forzador con `support.enabled` no: ningún soporte. `support.automatic` no sin forzador: ningún soporte |
| SM13 | Voladizo | `support.style` `organic` | Capas de 0,2, las de la pieza. En Z 7,2, 22 puntas redondas de 0,92 mm de diámetro, separadas entre 2,5 y 3,5 mm. Hacia abajo se juntan: 13 ramas en Z 7,0, 11 en 6,2, 9 en 5,2, 7 en 4,2, 4 troncos de 3,8 a 8 mm en 2,2 y uno de 11 × 22 mm en 0,4. Interfaz: 3 capas de 524,5 mm (Z 7,4 a 7,8), de X 99,4 a 109,8 e Y 90,0 a 110,0, a 45°, 135° y 45°. `support.tree_tip_diameter` 1,5: 18 puntas de 1,95 mm. `support.tree_top_rate` 30 %: 32 puntas. `support.tree_branch_diameter` 4: ramas más gruesas, 128,7 mm en Z 4,2 contra 73,7. `support.tree_branch_distance` 2: igual que 1 en esta pieza |
