# Corte en capas

**Qué hace:** convierte la malla en secciones planas, una por capa. Cada sección tiene islas, y cada isla sus
agujeros. Es la entrada de los perímetros, los rellenos y los soportes.

**Fase:** F1.

## Ajustes

| Clave | Unidad | Rango | Por defecto | Qué hace |
|---|---|---|---|---|
| `layers.height` | mm | mayor que 0 y no mayor que `machine.nozzle_diameter` | 0,3 | Altura de las capas después de la primera |
| `layers.first_height` | mm | mayor que 0 y no mayor que `machine.nozzle_diameter` | 0,35 | Altura de la primera capa |
| `slicing.gap_closing_radius` | mm | 0 o más | 0,049 | Las grietas de una sección de hasta el doble de este valor se cierran siempre |
| `slicing.xy_offset` | mm | cualquier valor | 0 | Agranda (positivo) o achica (negativo) todas las secciones en XY |
| `slicing.first_layer_shrink` | mm | 0 o más | 0 | Achica solo la primera capa, contra el «pie de elefante» |
| `slicing.fill_mode` | — | `union`, `even_odd`, `fill_holes` | `union` | Cómo se interpreta la malla (ver «Reglas») |
| `slicing.simplify_tolerance` | mm | 0 o más | 0 | Desviación máxima al simplificar los contornos; 0 no simplifica |

## Reglas

1. **Alturas:** la primera capa mide `layers.first_height` y las demás `layers.height`. La Z de una capa es la de su
   tope: es la que va en `;Z:` (ver `GCODE_OUTPUT.md`).
2. **Plano de corte:** a mitad de cada capa. Una pared inclinada queda, en cada capa, donde la pieza corta esa
   mitad (prueba C5).
3. **Última capa:**
   - se agrega una capa mientras su plano de corte quede dentro de la pieza o justo en su tope;
   - la última capa no se acorta: la pieza impresa puede quedar hasta media capa más alta o más baja que el modelo
     (pruebas C2 y C3).
4. **Islas y agujeros:** una isla dentro del agujero de otra se imprime como isla aparte (prueba C12).
5. **`slicing.fill_mode`:**
   - `union`: lo que queda dentro de algún volumen es pieza; los volúmenes que se superponen se unen (prueba C10);
   - `even_odd`: una zona dentro de un número par de volúmenes es hueco (prueba C11);
   - `fill_holes`: se ignoran los agujeros, y cada isla queda llena (prueba C16).
6. **Normales:** las caras con la normal invertida no cambian el resultado (prueba C9).
7. **Grietas** (la malla no cierra y la sección queda abierta):
   - las de hasta 2 × `slicing.gap_closing_radius` se cierran siempre;
   - los extremos abiertos cercanos se unen con un tramo recto. En Tisma Slicer se cerraron grietas de 5 mm entre dos
     trozos de la sección y no una de 12 mm (pruebas C6 a C8). El motor puede elegir su propio límite, pero tiene
     que cerrar al menos las de 5 mm;
   - una sección que no se puede cerrar nunca se imprime abierta: se descarta y se avisa en el JSON de avisos (ver
     `BEHAVIOR.md`). Lo que hace Tisma Slicer en ese caso no sirve de referencia: imprime restos sueltos.
8. **`slicing.xy_offset`:** vale para todas las capas. Con un valor positivo crece el contorno exterior y se achican los
   agujeros; con uno negativo, al revés (pruebas C13 y C14).
9. **`slicing.first_layer_shrink`:** achica solo la primera capa, como un `slicing.xy_offset` negativo (prueba C15).
10. **`slicing.simplify_tolerance`:** quita puntos de los contornos sin apartarse más que la tolerancia (Douglas y Peucker,
    ver `SOURCES.md`) (prueba C17).
11. **Errores:**
    - `layers.height` o `layers.first_height` mayores que `machine.nozzle_diameter`: error, sin G-code (pruebas C18 y C19);
    - una primera capa sin nada que imprimir: error;
    - en los dos casos el motor sale con un código distinto de 0. Tisma Slicer sale con 0 en el primero: eso no se
      copia.

## Altura de capa variable

- **En Tisma Slicer:**
  - no es un ajuste del perfil de impresión;
  - cada objeto puede llevar, en el proyecto, una lista de alturas por tramo de Z, hecha a mano o con un botón que la
    calcula según la inclinación de la superficie;
  - sin esa lista, todas las capas miden `layers.height` (después de la primera).
- **En el motor, primera versión:** opcional. Si se agrega:
  - la entrada es una lista de pares (Z, altura) por objeto;
  - cada altura queda entre un mínimo y un máximo de la impresora y nunca supera `machine.nozzle_diameter`;
  - el plano de corte sigue a mitad de cada capa (regla 2).
- **Pruebas:** sin pruebas medidas todavía; se agregan cuando el motor lo implemente.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra, desde la línea de comandos.

**Ajustes base:**
- `machine.nozzle_diameter` 0,4; `layers.height` 0,2; `layers.first_height` 0,2;
- `walls.count` 2; `infill.density` 0; `surfaces.top_layers` 0; `surfaces.bottom_layers` 0; sin falda ni borde;
- pieza centrada en X = 100, Y = 100.

**Cómo se mide el borde:** el borde exterior de una capa es el extremo de los movimientos `;TYPE:External perimeter`
más la mitad de su ancho (`;WIDTH:`). En estas pruebas el ancho fue 0,4 mm en la primera capa y 0,45 mm en las demás.
Las medidas se dan respecto del centro de la pieza, con tolerancia de ±0,02 mm.

**Piezas:** se generan por código.

| # | Pieza | Ajustes (además de los base) | Resultado |
|---|---|---|---|
| C1 | Cubo de 20 × 20 × 10 mm | — | 50 capas, Z de 0,2 a 10,0. Borde exterior a ±10,00 mm en todas |
| C2 | Prisma de 20 × 20 × 10,1 mm | — | 51 capas; la última con Z 10,2 y altura 0,2 |
| C3 | Prisma de 20 × 20 × 10,09 mm | — | 50 capas; la última con Z 10,0 |
| C4 | Cubo de C1 | `layers.first_height` 0,3 | 50 capas: Z 0,3, 0,5, 0,7 … 10,1 |
| C5 | Cono de radio 10 y alto 10 mm, base de 128 lados | `layers.height` 0,3; `layers.first_height` 0,3 | El radio del borde es el del cono a mitad de la capa. Ver la tabla «C5» abajo |
| C6 | Cubo de C1 con la cara derecha corrida 0,05 mm hacia afuera: dos grietas verticales de 0,05 mm | — | Se cierra: ancho de 20,05 mm |
| C7 | Lo mismo con 5 mm | — | Se cierra: ancho de 24,94 mm (el modelo mide 25) |
| C8 | Lo mismo con 12 mm | — | En Tisma Slicer no se cierra. El motor descarta la sección y avisa |
| C9 | Cubo de C1 con todas las normales invertidas | — | Igual que C1 |
| C10 | Dos prismas de 15 × 20 × 10 mm que se superponen 10 mm en X (juntos ocupan 20 × 20) | — | Igual que C1: una sola isla |
| C11 | Los prismas de C10 | `slicing.fill_mode` `even_odd` | Dos islas de 5 × 20 mm: la zona de 10 mm cubierta dos veces es hueco. Largo del perímetro externo por capa: 96,28 mm |
| C12 | Marco cuadrado de 30 × 30 mm con un agujero de 20 × 20 mm, y un cubo de 10 × 10 mm en el centro del agujero; alto 5 mm | — | Tres contornos externos por capa (marco por fuera, marco por dentro y cubo): largo total 237,64 mm |
| C13 | Marco de 20 × 20 mm con un agujero de 10 × 10 mm, alto 5 mm | `slicing.xy_offset` −0,2 | Borde exterior a ±9,80 mm. El agujero crece 0,2 mm por lado: el largo total del perímetro externo por capa no cambia (119,50 mm, igual que sin `slicing.xy_offset`) |
| C14 | Cubo de C1 | `slicing.xy_offset` 0,2 | Borde exterior a ±10,20 mm |
| C15 | Cubo de C1 | `slicing.first_layer_shrink` 0,2 | Primera capa a ±9,80 mm; las demás a ±10,00 mm |
| C16 | Marco de C13 | `slicing.fill_mode` `fill_holes` | Igual que C1: sin agujero |
| C17 | Cilindro de radio 10 y alto 5 mm, 128 lados | `slicing.simplify_tolerance` 0 y luego 0,5 | Perímetro externo de la capa 11: 65 tramos con 0 y 16 tramos con 0,5 (Tisma Slicer simplifica un poco aun con 0, porque el G-code tiene una tolerancia propia de 0,0125 mm) |
| C18 | Cubo de C1 | `layers.first_height` 0,5 | Error: la primera capa es más alta que la boquilla. Sin G-code |
| C19 | Cubo de C1 | `layers.height` 0,5 | Error: la capa es más alta que la boquilla. Sin G-code |

**Tabla C5**, radios en mm:

| Capa | Z | Borde medido | Cono en la base de la capa | Cono a mitad de la capa | Cono en el tope de la capa |
|---|---|---|---|---|---|
| 1 | 0,3 | 9,84 | 10,00 | 9,85 | 9,70 |
| 11 | 3,3 | 6,84 | 7,00 | 6,85 | 6,70 |
| 21 | 6,3 | 3,85 | 4,00 | 3,85 | 3,70 |
| 31 | 9,3 | 0,83 | 1,00 | 0,85 | 0,70 |

El borde medido queda unos 0,01 mm por dentro del cono a mitad de la capa, porque la base es un polígono de 128 lados.
