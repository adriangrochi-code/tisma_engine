# Altura de capa por zona

Pedido del dueño (2026-10-25): «altura de capa variable que dependa del detalle y no de la pieza entera, así podemos
conseguir que aunque dos partes estén a la misma altura tengan capas de alturas distintas, para mejorar calidades».

La altura variable común (`docs/spec/features/slicing.md`, «Altura de capa variable») cambia la altura de **toda** la
capa según Z. Acá la altura cambia también en XY: a la misma altura, la zona con detalle va en capas finas y el resto
en capas gruesas.

## De dónde sale

- **Local adaptive slicing:** Tyberg, J. y Bøhn, J. H., *Local adaptive slicing*, Rapid Prototyping Journal 4(3),
  1998. Cada parte de la pieza lleva su propio espesor de capa, y todas se encuentran en alturas comunes.
- **Region-based adaptive slicing:** Mani, K., Kulkarni, P. y Dutta, D., *Region-based adaptive slicing*,
  Computer-Aided Design 31(5), 1999. Las zonas que piden capas finas se separan en XY de las que no.
- **El error de escalón** (cusp height): Dolenc, A. y Mäkelä, I., *Slicing procedures for layered manufacturing
  techniques*, Computer-Aided Design 26(2), 1994. Una capa de alto h en una superficie inclinada deja un escalón que
  crece con h y con lo que se aparta la superficie de la vertical.

De los artículos se toma la idea; el método de abajo (las zonas por diferencia de secciones, el recorte de los
caminos, la escritura en una sola pila) es diseño propio.

## La idea

- **Capas gruesas de base** de alto `H` (`layers.height`) y **subcapas finas** de alto `h = H / k` (`k` entero, 2 a
  4) dentro de cada una. Las dos pilas se encuentran en el tope de cada capa gruesa.
- **El motor lamina la pila fina entera** (las regiones, las paredes y los rellenos de cada subcapa, como hoy).
- **Por cada capa gruesa (un grupo de `k` subcapas) decide dónde hace falta lo fino:**
  - donde las secciones de sus `k` subcapas son iguales (una pared vertical, el interior), una capa gruesa da la
    misma forma: esa zona va **gruesa**;
  - donde difieren (una pendiente suave, una cúpula, un detalle chico, el tope o el fondo de algo), una capa gruesa
    dejaría el escalón: esa zona va **fina**.
- **La zona fina** es la diferencia entre la unión y la intersección de las `k` secciones (lo que no está en todas),
  más lo que no tiene nada debajo de la primera subcapa (un fondo, un puente) o encima de la última (un tope), sin las
  tiras más finas que una tolerancia (`layers.detail_tolerance`, el escalón que se acepta), agrandada un margen
  (`layers.detail_margin`) para que las paredes y el relleno de alrededor del detalle también vayan finos. **La zona
  gruesa** es la intersección de las `k` secciones menos la zona fina, sin los pedazos más angostos que dos anchos de
  línea (no vale la pena un cordón grueso ahí). El grupo más alto de la pieza es su tope: va todo fino.
- **Se imprime en una sola pila, la fina:**
  - en cada subcapa, lo que cae en la zona fina, con su alto `h`;
  - en la última subcapa del grupo, además, lo que cae en la zona gruesa, con alto `H`: la boquilla está en el tope
    del grupo y un cordón grueso llena el hueco de las `k` subcapas. `H` no puede pasar del 75 % de la boquilla.
- **En el G-code** cada tramo lleva su alto en `;HEIGHT:` (el verificador y la vista previa ya lo leen por tramo):
  `;HEIGHT:H` antes de lo grueso y `;HEIGHT:h` de vuelta.

Así, dos partes a la misma altura —una pared recta y una cúpula, por ejemplo— salen con capas de alto distinto: la
pared en capas de `H`, la cúpula en capas de `h`.

## Los bordes entre zonas

- **Las paredes** se recortan por zona como caminos abiertos: el tramo de una vuelta que cae en la zona fina se
  imprime en cada subcapa; el tramo en la gruesa, una vez, grueso. La vuelta exterior sigue el contorno real en las
  dos.
- **El relleno** se recorta igual, con un solape chico entre zonas (un cuarto de cordón) para que se peguen.
- **La primera capa** no se agrupa: va entera con `layers.first_height`.
- **Las capas superiores e inferiores sólidas** se cuentan en milímetros de capas gruesas (`top_layers × H`), así la
  piel no adelgaza en la pila fina.
- **Los soportes, la falda y el borde** salen de la pila fina, como hoy.

## Ajustes nuevos (`docs/CONFIG.md`)

| Clave | Por defecto | Qué hace |
|---|---|---|
| `layers.detail_height` | 0 | El alto de las subcapas finas, en mm; 0 apaga la altura por zona. Se redondea para que `layers.height` sea un múltiplo entero (`k` de 2 a 4), y ninguna subcapa baja de 0,04 mm (si no entran dos, se apaga) |
| `layers.detail_tolerance` | 0,05 | El escalón que se acepta, en mm: una diferencia entre subcapas más angosta que esto no pide capas finas |
| `layers.detail_margin` | 3 anchos de línea | Cuánto se agranda la zona fina alrededor del detalle, en mm |

## En el código

- **`src/slice/local_layers.*`:** `detail_ratio` (cuántas subcapas), `plan_layer_groups` (los grupos y sus zonas
  gruesas, en paralelo), `clip_toolpaths` (corta caminos por una zona, con el ancho interpolado en el corte y la
  vuelta cerrada que atraviesa su inicio unida en un solo tramo) y `split_thick_zones` (lo de cada grupo dentro de la
  zona gruesa pasa a su última subcapa como `ThickLayer`).
- **`src/slice/pipeline.cpp`:** con `layers.detail_height`, la pila es la fina; las pieles se cuentan en capas
  gruesas; el espacio de los soportes, en subcapas; al final, los grupos y las capas gruesas.
- **`src/gcode/writer.cpp`:** después de lo fino de cada capa, lo grueso de su grupo, entre `;HEIGHT:H` y `;HEIGHT:h`.
- **Pruebas:** `tests/test_local_layers.cpp` (las zonas de un bloque, una pirámide, una caja y una pirámide a la misma
  altura, un escalón menor que la tolerancia, un puente; el corte de caminos; una caja y una pirámide lado a lado
  laminadas a G-code verificado, con el material y el tiempo comparados) y la búsqueda aleatoria de G-code, que
  también sortea la altura por zona.

## Por etapas

1. ~~**Las zonas**~~ y 2. ~~**el recorte de caminos y la escritura**~~: hechas (2026-10-25).
3. **La interfaz**: los tres ajustes en Impresión; la vista previa con el color por alto de capa.
4. **Medir**: tiempo y escalón en el banco, contra todo fino y todo grueso; si conviene, recortar en la GPU.
