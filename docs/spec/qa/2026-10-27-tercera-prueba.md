# Tercera prueba de caja negra (27-10-2026)

**Qué es:** se repitió `2026-10-26-segunda-prueba.md` con el motor de `dev` en `bdff715`, y se midieron contra Tisma
Slicer los soportes en árbol (SM13), los arcos en voladizos (A1 a A5), la lectura de 3MF y el centrado. Mismo método:
piezas generadas por un script, G-code leído por sus etiquetas, sin leer código.

## Lo que ya estaba

| # | Estado en `bdff715` |
|---|---|
| QA1 a QA4, QA7 | Siguen bien |
| QA5 | Hecho |
| QA6 | No se puede comparar (velocidades del perfil de la impresora) |
| QA8 | Sigue: la aguja de 1 × 1 × 80 mm solo da el aviso de velocidades de respaldo. La sesión del motor dice que está hecho y espera la compilación de Linux |
| QA9 | Sigue: la pared de 0,2 mm da «nothing would be printed». Mismo caso que QA8 |

## Soporte en grilla (S4)

| # | Antes | Ahora | Estado |
|---|---|---|---|
| QA10 | Sin margen | Zona de X 98,75 a 111,45 e Y 88,57 a 111,43 en Z 1,2: pasa unos 1,5 mm la losa en +X, +Y y −Y | Arreglado |
| QA11 | Eje a 0,8 mm de la columna | Eje a 0,40 mm (0,2 de separación más medio cordón de 0,4) | Arreglado |
| QA12 | Líneas cada 2,5 mm, dirección alternada | Líneas a lo largo de X en todas las capas, cada 2,857 mm. Interfaz: 516,4 mm por capa (Tisma Slicer 524,5) | Arreglado |
| QA13 | Altura 0,2, perímetros comunes | Solo el tramo sobre el soporte: `Overhang perimeter` y `Bridge infill` con altura 0,4; lo de la columna con 0,2 | Arreglado |

## Soporte en árbol (SM13, voladizo)

| Medida | Tisma Slicer | Motor |
|---|---|---|
| Puntas en Z 7,2 | 22 | 21 |
| Piezas en Z 7,0 / 6,2 / 5,2 / 4,2 / 2,2 / 0,4 | 13 / 11 / 9 / 7 / 4 / 1 | 11 / 11 / 10 / 9 / 6 / 4 |
| Interfaz por capa | 524,5 mm, ejes de X 100,65 a 109,98 | 485,6 mm, de X 100,40 a 109,80 |

Arriba ya se parece; abajo las ramas no terminan en un solo tronco: en Z 0,4 quedan 4 piezas donde Tisma Slicer
tiene una de 11 × 22 mm.

## Arcos en voladizos (A1 a A5)

| # | Tisma Slicer | Motor |
|---|---|---|
| A1 | Centro (98,8; 100,0); radios cada 0,36 mm desde 0,35; 606,1 mm; 0,052245 mm de filamento por mm; `;HEIGHT` 0,4; F300 | Mismo centro; cada 0,36 desde 0,32; 673,9 mm (+11 %); 0,050104 mm por mm (−4 %); `;HEIGHT` 0,314; F300 |
| A2 (L) | Un grupo, centro (98,8; 103,8); 904,8 mm | Un grupo, centro (98,8; 105,0) en las mismas coordenadas; 1021,1 mm |
| A3 (agujero) | 557,4 mm; el agujero corta los arcos | 653,8 mm; igual |
| A4 (tira) | Centro cerca de (99; 100); 124,8 mm | Mismo centro; 184,3 mm |
| A5 (con soporte) | Soporte debajo y la capa igual a A1 | Igual |

Coordenadas de A2 y A4: el motor ahora centra la pieza en la cama; se restó ese corrimiento para comparar.

**Para revisar:**
- el motor imprime entre 11 y 48 % más largo de arcos. En la tira, los arcos de la raíz se abren más allá del ancho
  de la tira, sobre la columna, más que en Tisma Slicer;
- el filamento por mm queda 4 % abajo del cordón redondo de 0,4 mm de diámetro (0,052245 con filamento de 1,75);
- `;HEIGHT` de los arcos: Tisma Slicer escribe 0,4 (el diámetro); el motor 0,314.

## Formatos, centrado y el cráneo

- El 3MF del cráneo de triceratops se lee y se lamina: 6,5 s en total, 520 capas. OBJ no se midió acá.
- `box.stl` (fuera del centro) queda centrado en la cama: X de 78,8 a 121,2, con la falda.
- Soporte del cráneo (mismos ajustes que en `2026-10-26-velocidad.md`):

| | Antes (`dfcd143`) | Ahora (`bdff715`) | Tisma Slicer |
|---|---|---|---|
| Base | 63,2 m | 123,3 m | 145,5 m |
| Interfaz | 1,2 m | 4,4 m | 32,5 m |

La base ya casi alcanza. La interfaz sigue baja: Tisma Slicer también pone interfaz debajo del soporte, donde apoya
sobre la pieza (`supports_more.md`, regla 7, `support.bottom_interface_layers`), y eso el motor todavía no lo hace.

## Qué queda, por orden

1. QA8 y QA9, cuando la compilación de Linux pase.
2. Arcos: el largo de más, sobre todo en la raíz de las tiras, y el filamento por mm.
3. Árbol: que las ramas se junten hasta un tronco abajo.
4. Interfaz debajo del soporte sobre la pieza (`supports_more.md`, regla 7).
