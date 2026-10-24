# Capas no planares

**Qué hace:** imprime capas curvas en lugar de planas, con una impresora común de 3 ejes. Todo lo de la capa sigue la
curva: perímetros, relleno y soportes. Sirve para:
- trabar las capas entre sí;
- inclinar las capas bajo los voladizos, y así necesitar menos soporte;
- que las superficies de arriba queden como una sola capa continua.

**Fase:** F7. Lo diseñó y escribió el equipo del dueño en Tisma Slicer. La idea de deformar, laminar plano y volver
es conocida. El cono con pendiente según el radio toma la idea de Joshua Bird («radial non-planar slicer»), sin su
código. Para capas curvas con límites de pendiente y espesor, ver CurviSlicer (Etienne et al., SIGGRAPH 2019) en
`SOURCES.md`.

## Definiciones

- Las capas reales son las superficies `z = s + D(x, y, z)`, una para cada altura de laminado `s`.
- `D = g(x, y, z) · rampa(z)`, donde `g` es la forma de la capa y `rampa` la apaga cerca de la cama (y arriba, si se
  pide).
- **Una forma de hacerlo:**
  1. mover la malla a `z' = z − D`;
  2. laminarla con capas planas;
  3. devolver cada punto del G-code con `z = z' + D`.

  El motor puede hacerlo de otra manera si el resultado es el mismo.
- `x`, `y` se miden desde el centro de la caja de la pieza en XY.
- **Espesor local de la capa:** `J = 1 / (1 − ∂D/∂z)`, la relación entre el espesor real y el de laminado.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `curved.mode` | — | `disabled` | `wave`, `conical`; `overhangs`, `stress`, `surface` y `automatic` en `nonplanar_modes.md` |
| `curved.pattern` | — | `egg` | Forma de `wave`: `egg`, `ridges`, `twisted` |
| `curved.amplitude` | mm | 0,6 | Amplitud `a` de la onda, si no se ajusta a la cabeza |
| `curved.wavelength` | mm | 16 | Largo `λ` de la onda, si no se ajusta a la cabeza |
| `curved.angle` | ° | 0 | Dirección de `ridges` y ángulo inicial de `twisted` |
| `curved.twist` | °/mm | 3 | `twisted`: giro de la dirección por mm de altura |
| `curved.cone_angle` | ° | 15 | `conical` sin ajuste a la cabeza: pendiente del cono; positivo sube hacia afuera |
| `curved.fit_head` | — | sí | Las capas tan curvas como permite la cabeza (regla 3). En `conical`, el cono sale de la pieza (`nonplanar_modes.md`) |
| `curved.flat_below` | mm | 0,6 | Por debajo, capas planas |
| `curved.ramp_height` | mm | 5 | Alto de la transición de plano a curvo |
| `curved.flat_top` | — | no | Volver a capas planas arriba de todo |
| `curved.segment_length` | mm | 0,5 | Cada cuánto se parte un movimiento para seguir la curva; los tramos rectos se vuelven a unir |
| `curved.max_slope` | ° | 20 | Pendiente máxima si no se midió la cabeza |
| `head.free_angle_x`, `head.free_angle_y` | ° | 0 = no medido | La pendiente más empinada que la cabeza deja subir a lo largo de X y de Y |
| `head.clearance_height` | mm | 3 | Distancia vertical de la punta de la boquilla a lo más bajo de la cabeza a su alrededor |
| `head.clearance_radius` | mm | 15 | Distancia horizontal de la boquilla a lo más lejano de esas partes bajas |
| `curved.flow_policy` | — | `preserve` | `preserve`: mantener el caudal planeado ajustando la velocidad; `uniform`: todo (salvo externos, voladizos, puentes y relleno de huecos) al mismo caudal; `off`: no tocar velocidades |
| `curved.uniform_flow` | mm³/s | 0 = 80 % del caudal máximo | Caudal de `uniform` |

## Reglas

1. **Formas** (`k = 2π / λ`):
   - `egg`: `g = a · sin(k·x) · sin(k·y)`;
   - `ridges`: `g = a · sin(k·(x·cos θ + y·sin θ))`, con θ = `curved.angle`. Con 0, la altura cambia a lo largo de
     X y las crestas son paralelas a Y;
   - `twisted`: como `ridges` con `θ(z) = curved.angle + curved.twist · z`;
   - `conical`: `g = tan(cono) · (√(x² + y² + r²) − r)`, con `r` = 2 mm, que redondea la punta.
2. **Rampa:**
   - `rampa(z) = S((z − curved.flat_below) / curved.ramp_height)`, con `S(t) = t²(3 − 2t)` para t entre 0 y 1 (0 antes, 1 después);
   - con `curved.flat_top`, se multiplica además por `1 − S((z − (alto − curved.ramp_height)) / curved.ramp_height)`.
3. **Ajuste a la cabeza** (`curved.fit_head`):
   - `tan_x`, `tan_y`: tangentes de los ángulos libres medidos, o de `curved.max_slope` si no se midieron. `tan_min`
     es la menor;
   - **onda:** `λ = π · head.clearance_height / tan_min`, entre 4 y 100 mm;
   - **amplitud:** la que da la pendiente máxima `tan_min` (`a = tan_min / k`; en `ridges`, con la tangente de su
     dirección), y nunca más que `head.clearance_height / 2`, porque la onda sube `2a` de valle a cresta;
   - **cono:** la pendiente de cada radio sale de los voladizos de la pieza, y una pieza sin voladizos queda plana
     (`nonplanar_modes.md`, reglas 3 a 8). `curved.cone_angle` solo vale sin `curved.fit_head`;
   - **espesor:** si con eso algún punto de la pieza da `J` fuera de [0,6; 1,4], la amplitud (o la pendiente del cono)
     se achica por bisección hasta que entre.
4. **Material:** cada tramo extruye su material plano multiplicado por `J` en ese punto. Con `preserve`, la velocidad
   se ajusta para mantener el caudal.
5. **Eje Z:** la velocidad de cada tramo se limita para que el eje Z no supere la velocidad ni la aceleración máximas
   de la impresora.
6. **Cabeza:** donde la pieza ya impresa subiría junto a la boquilla más que `head.clearance_height` dentro de
   `head.clearance_radius`, las capas se suavizan. Una capa más empinada que lo que permite la cabeza es un error.
7. **Requisito:** E relativa (`M83`); sin eso, error.
8. **Primeras capas:** las capas debajo de `curved.flat_below` salen planas.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. Ajustes base de `gcode_motion.md`, con E relativa. Cabeza por
defecto: altura libre 3 mm, ángulos no medidos (20°).

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| NP1 | Bloque de 40 × 40 × 12 mm centrado en X = 100, Y = 100 | `curved.mode` `wave`, `egg` | 65 capas (plano: 60). Capas 1 a 3 planas. Capa 41 (`;Z:` 8,2): cada punto extruido cumple `Z = 8,2 + 0,955 · sin(k(X − 100)) · sin(k(Y − 100))` con `λ = 25,894` mm: error mediano 0,0009 mm, máximo 0,0022. La amplitud la limita `J ≤ 1,4` en la rampa (0,952 calculado) |
| NP2 | El bloque de NP1 | `wave`, `ridges` | Capa 41: `Z = 8,2 + 0,955 · sin(k(X − 100))`, error máximo 0,0008 mm |
| NP3 | El bloque de NP1 | `wave`, `egg` | Capa 11 (`;Z:` 2,2, dentro de la rampa): en 1257 tramos del relleno, filamento por mm / (0,033849 · `J`) da una mediana de 1,000 (el 80 % entre 0,994 y 1,006), con `J` entre 0,82 y 1,36 |
| NP4 | El bloque de NP1, con E absoluta | `wave` | Error: las capas no planares necesitan E relativa |
