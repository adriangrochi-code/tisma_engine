# Análisis estructural (`tisma-engine analyze`)

```
tisma-engine analyze pieza.stl --config ajustes.json --case caso.json -o informe.json [--vtk mapa.vtk]
```

El motor corta la pieza con los ajustes (`docs/CONFIG.md`), la pasa a vóxeles que siguen las capas y resuelve la
elasticidad lineal de la pieza tal como se imprimiría. El material es ortótropo: rigidez y resistencia propias a lo
largo del cordón, entre cordones y entre capas, tomadas de `docs/MATERIALS.md`. El resultado es:
- el factor de seguridad de cada vóxel (criterio de Tsai–Wu);
- qué cede primero en cada vóxel: el cordón, la unión entre cordones o la unión entre capas;
- los desplazamientos.

Códigos de salida: 0 bien; 1 línea de comandos mal; 2 un archivo de entrada falta o está mal; 4 la pieza no se puede
imprimir; 5 el análisis no dio un resultado válido. Con 5 el informe se escribe igual cuando se puede, y dice por qué
(nada sostiene la pieza, ninguna carga la toca, el solucionador no convergió o hacen falta demasiados vóxeles).

## El caso (`--case caso.json`)

Un objeto JSON. Todas las claves son opcionales y una clave desconocida es un error. Las longitudes van en mm y las
fuerzas en N.

```json
{
  "material": "bambu-pla-basic",
  "model": "as_printed",
  "quality": "high",
  "fixtures": [{"name": "cama", "face": "bottom"}],
  "loads": [{"name": "gancho", "face": "top", "force": [0, 0, -100]}]
}
```

| Clave | Tipo | Por defecto | Significado |
|---|---|---|---|
| `material` | texto u objeto | `"bambu-pla-basic"` | Un registro de la base (`docs/MATERIALS.md`, columna «Registro»). También puede ser un objeto con `base` (un registro) y las constantes que se quieran cambiar: `e1`, `e2`, `e3`, `g12`, `g13`, `g23` y `nu12`, `nu13`, `nu23` (rigidez, MPa); `xt`, `xc`, `yt`, `yc`, `zt`, `zc`, `s12`, `s13` y `s23` (resistencias, MPa). Por ejemplo, `{"base": "bambu-petg-basic", "zt": 28}` |
| `model` | texto | `"as_printed"` | `as_printed`: cada vóxel toma la dirección del cordón que lo cruza en el laminado, y los huecos del relleno ralo quedan vacíos. `solid`: la pieza maciza, sin cortarla; con dirección desconocida, el material toma la rigidez promedio en la capa y para la falla se usa la dirección más débil. `solid` es más rápido y sirve para una primera idea |
| `quality` | texto | `"normal"` | `normal`: 2 vóxeles por ancho de línea (`walls.width`) y una fila por capa. `high`: 4 vóxeles por ancho de línea y filas tan altas como anchas (2 por capa de 0,2 mm con líneas de 0,4 mm). Para decidir si una pieza aguanta, usar `high` y leer «Qué tan confiable es el número» |
| `voxel` | número | según `quality` | Ancho del vóxel en X e Y. Con `as_printed`, los huecos del relleno solo aparecen si es como mucho medio ancho de línea; si es mayor, la pieza se analiza maciza y el informe lo avisa |
| `rows_per_layer` | entero | según `quality` | En cuántas filas se parte cada capa (de 1 a 64) |
| `max_voxels` | entero | 8 000 000 | Si el análisis pide más vóxeles, no corre. La memoria ronda 0,5 kB por vóxel |
| `fixtures` | lista | vacía | Apoyos: los nodos de la pieza en la región no se mueven en los ejes elegidos |
| `loads` | lista | vacía | Cargas: la fuerza total se reparte sobre las caras del borde de la pieza cuyo centro cae en la región, en proporción al área |
| `solver` | objeto | | `tolerance` (10⁻⁸: residuo relativo al que se para), `max_iterations` (20 000), `preconditioner` (`multigrid` o `jacobi`, la referencia lenta) |

Cada apoyo y cada carga tiene un `name` opcional, que aparece en los avisos, y una región:
- `"face"`: una cara de la pieza, `bottom`, `top`, `x_min`, `x_max`, `y_min` o `y_max`. Es el plano del extremo de la
  pieza en ese eje: por ejemplo, `top` son las caras de arriba de los vóxeles más altos.
- `"box"`: una caja con `min` y `max`, cada una `[x, y, z]` en coordenadas de la pieza. `null` en un eje quiere decir
  sin límite, y si falta `min` o `max` la caja no tiene límite de ese lado.

Además, los apoyos llevan `axes` (por defecto `"xyz"`), los ejes que quedan fijos; por ejemplo, `"z"` es un apoyo
que deja deslizar en el plano. Las cargas llevan `force`: `[fx, fy, fz]` en N.

Las partes de la pieza que ningún apoyo sostiene (unidas por caras a un vóxel que toca un apoyo) quedan fuera del
cálculo: su desplazamiento no está determinado. El informe dice cuántos vóxeles quedaron fuera.

## El informe (`-o informe.json`)

El cubo de prueba de 10 mm en PLA Basic de Bambu Lab, calidad `high`, base fija y 100 N de corte en la cara de arriba
(recortado):

```json
{
  "version": 1,
  "valid": true,
  "model": "as_printed",
  "material": {"id": "bambu-pla-basic", "name": "Bambu Lab PLA Basic (sheet V3.0)",
               "source": "Bambu Lab technical data sheet",
               "constants": {"e1": 2580.0, "e3": 2060.0, "xt": 35.0, "zt": 31.0, "s13": 26.66}},
  "voxels": {"size": 0.1, "rows_per_layer": 2, "grid": [100, 100, 100], "part": 1000000, "printed": 618028,
             "loose": 0, "analysed": 618028},
  "solver": {"preconditioner": "multigrid", "levels": 6, "iterations": 54, "residual": 6.99e-09,
             "tolerance": 1e-08, "converged": true, "prepare_seconds": 0.025, "solve_seconds": 29.3},
  "result": {"max_displacement_mm": 0.1024, "max_von_mises_mpa": 27.12, "min_safety": 1.983,
             "weakest": {"x": 9.95, "y": 9.85, "z": 0.05, "layer": 0, "mode": "between_layers"},
             "min_peak_safety": 1.283, "voxels_below_1": 0, "voxels_below_1_5": 0, "voxels_below_2": 4},
  "layers": [{"index": 0, "bottom": 0.0, "top": 0.2, "min_safety": 1.983, "mode": "between_layers"}],
  "warnings": []
}
```

- `valid`: el solucionador convergió, un apoyo sostiene la pieza y al menos una carga la toca.
- `voxels`: `part`, los vóxeles de la pieza; `printed`, los que cruza un cordón (`as_printed`); `loose`, los que
  quedaron fuera por sueltos; `analysed`, los que entraron en el cálculo.
- `min_safety`: cuántas veces podrían crecer las cargas antes de la primera falla según Tsai–Wu. Menos de 1 quiere
  decir que la pieza falla. `null` si nada está cargado. Cada vóxel compara las resistencias con la tensión promediada
  en un ancho de línea en el plano y en las filas de su capa. Las resistencias de un material impreso son promedios
  sobre sus líneas y capas, así que no tiene sentido compararlas con la tensión de un punto. Es el método de volumen
  de la teoría de distancias críticas (Taylor 2007).
- `min_peak_safety`: lo mismo con la tensión de un solo vóxel. Sirve para ver dónde se concentra la tensión, pero en
  las aristas de un apoyo y en las esquinas vivas sigue bajando al achicar el vóxel (la elasticidad lineal no tiene
  ahí una tensión finita), así que no sirve para decidir.
- `mode`: `line` (se rompe el cordón), `between_lines` (se despegan cordones vecinos de la misma capa),
  `between_layers` (se despegan las capas) o `none`.
- `layers`: el vóxel más débil de cada capa, para ver qué capas mandan.
- `warnings`, por su código:
  - `fixture_unused`: un apoyo que no toca la pieza;
  - `loose_voxels`: partes que ningún apoyo sostiene;
  - `load_not_applied`: cargas cuya región no toca la parte sostenida;
  - `not_converged`: el solucionador no llegó a la tolerancia;
  - `coarse_voxels`: vóxeles demasiado anchos para los huecos del relleno;
  - `unsafe`: hay vóxeles con factor de seguridad menor que 1;
  - `nothing_held`: ningún apoyo sostiene nada;
  - `no_loads`: el caso no tiene cargas.

## El mapa (`--vtk mapa.vtk`)

Un archivo VTK legado, binario: una rejilla rectilínea cuyos planos son los de los vóxeles, así que las filas de
capa conservan su altura. Se abre en ParaView y en cualquier lector de VTK.

Datos por celda:
- `solid`: 1 analizado; 0 vacío, un hueco del relleno o una parte suelta.
- `safety`: el factor de seguridad; −1 donde no hay material, y 10⁹ como tope donde nada carga.
- `peak_safety`: el mismo factor con la tensión de cada vóxel solo.
- `mode`: 0 nada, 1 cordón, 2 entre cordones, 3 entre capas.
- `von_mises`: en MPa.
- `direction`: la dirección del cordón, en grados de 0 a 180; −1 si no se conoce.

Datos por nodo: `displacement`, en mm. Para ver la pieza, filtrar con `Threshold` sobre `solid` igual a 1 y colorear
por `safety`.

## Qué tan confiable es el número

Cuánto cambian los resultados al achicar el vóxel. Medido en una placa de 12 × 4 × 2 mm de PLA Basic de Bambu Lab,
empotrada en `x_min` y con 5 N hacia abajo en `x_max`, ajustes por defecto (relleno al 20 %); el vóxel se achica a la
mitad en cada columna y las filas por capa se duplican:

| Placa | Vóxel | Desplazamiento máx. | `min_safety` | Lejos del empotramiento (x > 1 mm) | `min_peak_safety` |
|---|---|---|---|---|---|
| Maciza (`solid`) | 0,2 mm | 0,4199 mm | 1,664 | 1,734 | 1,565 |
| | 0,1 mm | 0,4220 mm | 1,573 | 1,721 | 1,249 |
| | 0,05 mm | 0,4226 mm | 1,527 | 1,716 | 0,990 |
| Como se imprime | 0,2 mm | 0,4416 mm | 1,615 | 1,615 | 1,476 |
| | 0,1 mm | 0,4546 mm | 1,499 | 1,520 | 0,920 |
| | 0,05 mm | 0,4620 mm | 1,380 | 1,380 | 0,649 |

- **Pieza maciza:** el desplazamiento y el factor promediado convergen. Cada mitad de vóxel cambia la mitad o menos
  que la anterior.
- **Pieza como se imprime:** cada refinamiento todavía cambia el factor alrededor de un 8 %. La forma de los cordones
  cambia con la resolución: las líneas a 45° se vuelven escalones de vóxeles y el contacto entre cordones cruzados
  queda distinto.
- **Pico de un vóxel:** no converge. Es la singularidad de la arista empotrada.

Para decidir, entonces:
- usar `high`;
- leer `min_safety`, no `min_peak_safety`;
- dejar margen: la calidad `normal` dio un factor alrededor de un 8 % más alto que `high` en la placa y un 6 % en el
  cubo de prueba (2,10 contra 1,98);
- un empotramiento perfecto no existe: el valor junto a un apoyo `xyz` es pesimista; un apoyo real cede algo.

Pendiente en `PLAN.md`: estimar el error de resolución corriendo dos calidades, y representar mejor el contacto entre
cordones.
