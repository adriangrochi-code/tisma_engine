# Ajustes del motor (`--config ajustes.json`)

El formato es propio del motor: un objeto JSON con grupos, y nombres elegidos por este equipo. Las claves de otros
programas no se usan. Si Tisma Slicer necesita traducir sus ajustes, lo hace su adaptador, del lado de Tisma.

Reglas:
- Todas las claves son opcionales; las que faltan toman el valor por defecto.
- Una clave desconocida dentro de un grupo implementado es un error, para que los errores de tipeo no pasen en
  silencio.
- Los grupos todavía no implementados se aceptan, pero el motor avisa que no los usa.
- Las longitudes van en milímetros.

## Grupos implementados

### `layers`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `height` | número | 0.2 | > 0 | Alto de las capas, salvo la primera |
| `first_height` | número | 0.2 | > 0 | Alto de la primera capa |

### `slicing`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `fill_mode` | texto | `"union"` | `union`, `even_odd`, `fill_holes` | Cómo se decide qué está adentro cuando los contornos se superponen o se cruzan: `union` suma los volúmenes, `even_odd` alterna adentro y afuera en cada cruce, `fill_holes` además rellena los agujeros |
| `gap_closing_radius` | número | 0.049 | ≥ 0 | Largo máximo de un hueco que se cierra al unir contornos abiertos |
| `xy_offset` | número | 0 | cualquiera | Agranda (positivo) o achica (negativo) cada capa en XY; los agujeros se mueven al revés |
| `first_layer_shrink` | número | 0 | ≥ 0 | Cuánto se achica además la primera capa, contra la pata de elefante |
| `simplify_tolerance` | número | 0.01 | ≥ 0 | Se quitan los vértices de los contornos que se apartan menos que esto de la recta entre sus vecinos; 0 no simplifica. Menos segmentos aceleran los pasos siguientes (en una capa circular de 708 segmentos, 0,01 mm deja 208 y el eje medio tarda 3,4 veces menos) |

### `walls`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `count` | entero | 3 | 0 a 1000 | Paredes desde cada borde. Donde la pieza es más angosta que esas paredes, los cordones cambian de ancho para llenarla sin huecos (Kuipers et al. 2020); donde es más ancha, quedan las paredes al ancho de `width` y el medio queda para el relleno. 0: sin paredes, todo es relleno |
| `width` | número | 0.4 | > 0 | Ancho preferido de los cordones (w*); por ahora fijo, más adelante saldrá de la boquilla del grupo `machine` |

### `surfaces`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `top_layers` | entero | 3 | 0 a 1000 | Capas sólidas debajo de cada superficie superior expuesta |
| `bottom_layers` | entero | 3 | 0 a 1000 | Capas sólidas encima de cada superficie inferior expuesta |
| `top_pattern` | texto | `"monotonic"` | `monotonic`, `rectilinear` | Superficies superiores: `monotonic`, líneas sueltas todas en el mismo sentido, cada una después de las de abajo que la tocan (sin volver sobre lo hecho); `rectilinear`, en zigzag como las inferiores |

### `infill`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `density` | número | 20 | 0 a 100 | Densidad del relleno disperso, en %: líneas separadas `walls.width` / densidad. 0: sin relleno disperso. Las superficies sólidas van siempre al 100 % |
| `angle` | número | 45 | cualquiera | Ángulo de las líneas respecto del eje X, en grados; las capas impares suman 90 |
| `wall_overlap` | número | 25 | 0 a 100 | Cuánto entran en la pared interna, en % del ancho de cordón, las líneas sólidas que no se unen en zigzag (superficies superiores monotónicas). Con 25 %, el material depositado queda entre −0,5 % y +1,5 % del área |
| `pattern` | texto | `"rectilinear"` | `rectilinear`, `grid`, `gyroid` | Patrón del relleno disperso: `rectilinear`, líneas que giran 90° cada capa y se unen en zigzag; `grid`, las dos direcciones en cada capa, cada una al doble de separación (el mismo material); `gyroid`, el corte de la superficie giroide a la altura de cada capa, con el período que da la densidad (`angle` no se usa) |

### `compute`

| Clave | Tipo | Por defecto | Rango | Significado |
|---|---|---|---|---|
| `gpu` | texto | `"auto"` | `auto`, `off` | `auto` usa la GPU si hay una (Vulkan); `off` usa solo la CPU |

## Grupos previstos (todavía no implementados)

`machine`, `material`, `speed`, `retraction`, `cooling`, `support`. Se definen acá
cuando se implementan, a partir de las fichas de `docs/spec/features/`.

## Ejemplo

```json
{
  "layers": { "height": 0.3, "first_height": 0.35 },
  "slicing": { "xy_offset": -0.05, "first_layer_shrink": 0.1 },
  "walls": { "count": 2 },
  "compute": { "gpu": "auto" }
}
```
