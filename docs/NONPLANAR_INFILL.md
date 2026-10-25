# Relleno y paredes no planares para más resistencia

Pedidos del dueño (2026-10-25):
- «hay que ver rellenos no planares más fuertes que los tradicionales»;
- «también quiero ver el tema de brick layers y proponer una versión no planar para no infringir patentes».

Este documento junta lo publicado, con sus resultados medidos, y propone qué construir y en qué orden. Todo sale de
artículos y de descripciones públicas. No hay código de otros programas, y los guiones de posprocesado publicados
tampoco se leen.

**No es una opinión legal.** Lo que se dice de patentes viene de las fichas públicas que se pudieron consultar. Antes
de vender el motor, un abogado de patentes tiene que revisar las reivindicaciones. Las páginas de patentes no se
pudieron leer completas desde la sesión, porque los sitios bloquean el acceso automático.

## El problema

Una pieza impresa por capas es mucho más débil entre capas (en Z) que a lo largo de los cordones. El estudio de ORNL
habla de un 50 a 75 % menos en Z como algo común (Duty et al. 2019).

Gleadall y su grupo mostraron que la unión entre capas tiene la resistencia del material. La debilidad viene de la
forma de los cordones: la superficie de contacto es angosta y plana, y las ranuras entre cordones concentran
tensiones. Por eso se gana cambiando la geometría: más contacto, y contacto inclinado en vez de plano.

## Lo publicado

| Técnica | Qué hace | Resultado medido | Fuente | Patentes |
|---|---|---|---|---|
| **ZigZagZ** | Los cordones suben y bajan en Z mientras avanzan (0,5 mm de alto cada 2 mm, capas de 0,2 mm, boquilla de 0,4). Cada capa repite la forma de la anterior, así las capas quedan anidadas, con uniones inclinadas | En Z: **+62 % de resistencia, +123 % de deformación a rotura, +245 % de tenacidad**. La rotura pasa a través del cordón, no por la unión | Allum, Kitzinger, Li, Silberschmidt, Gleadall, *ZigZagZ: Improving mechanical performance in extrusion additive manufacturing by nonplanar toolpaths*, Additive Manufacturing 38 (2021) 101715 | Ninguna encontrada |
| **Pernos en Z (z-pinning)** | Huecos alineados a lo largo de N capas, rellenados de corrido en la capa N + 1 | En Z, **más de 3,5 veces** la resistencia y la tenacidad (PLA y PLA con fibra de carbono); isótropo con los pernos más grandes. En piezas macizas, +40 % | Duty, Failla, Kim, Smith, Lindahl, Kunc, *Z-Pinning approach for 3D printing mechanically isotropic materials*, Additive Manufacturing 27 (2019) 175–184 | «Patente en trámite» según un artículo de 2017 (ORNL). Revisar antes de usar |
| **Ondas sinusoidales** | Capas onduladas hechas con FullControl GCode | Impacto (Charpy) **hasta +50 %** en probetas verticales, y menos anisotropía | Estudio de la ÉTS de Montreal (impacto, no tracción) | — |
| **Brick layers** | Perímetros alternados subidos media capa, como ladrillos | Unión entre capas **+14 % en PLA y +10 % en PETG**, sobre todo con capas altas; con 0,2 y 0,3 mm no fue significativo | Ensayos de CNC Kitchen (2024) | Ver abajo |
| **Costuras en Z (z-stitching)** | Puntadas que traban capas | Menos anisotropía; un 14 % más de tiempo | MDPI, 2025 | — |
| **Sobreextrusión para Z** | Relleno no planar hecho con movimientos planos y mucha sobreextrusión | Sin ensayos | Divulgación defensiva, Technical Disclosure Commons (Konvicny, 2021) | Es estado de la técnica publicado |

### Brick layers y sus patentes

- **Stratasys, US5653925 (1995):** describe la misma idea. Venció en 2015 o 2016, según la fuente; queda en dominio
  público.
- **Addman, US11331848B2, «3D printing bead configuration» (otorgada en 2022, vigente hasta cerca de 2040).** Según el
  resumen público, la reivindicación 1 es un método para la resistencia al corte en Z:
  - capas con un cordón terminal en cada extremo;
  - el segundo cordón terminal orientado normal a los demás y con la mitad de su ancho;
  - la posición y la orientación de esos cordones alternadas entre capas vecinas.

  La capa de abajo con cordones de alturas alternadas (uno de la mitad de alto) está en la descripción.
- **Addman, US12017407B2, «Interlocking infill» (2024, vigente hasta cerca de 2042):** muros de capas con anchos
  alternados que dejan huecos, rellenos de relleno; en la capa de abajo, cordones de alturas alternadas.
- **Addman, US20230249394A1 (solicitud):** bajar la altura del cordón reduciendo el flujo, en las capas de arriba y de
  abajo.

Lo que dejan libre estos documentos, tal como se leen en los resúmenes:
- cordones cuya Z cambia de corrido a lo largo del recorrido, en vez de cordones enteros subidos a una altura fija;
- ninguna capa de media altura hecha bajando el flujo;
- ningún cordón terminal girado ni de medio ancho.

## Propuesta para el motor

Todo usa lo que el motor ya tiene: el corrimiento en Z por punto (`dz`) del Z anti-aliasing, que el escritor, el
verificador y la vista previa ya entienden, y el campo de profundidad del cúbico adaptativo.

### N1 — Relleno ondulado (ZigZagZ en el relleno)

Los cordones del relleno suben y bajan en Z según un campo fijo en el espacio `w(x, y)`. Es el mismo en todas las
capas, así cada capa queda anidada sobre la de abajo, con la unión inclinada como en ZigZagZ.

- **Patrones:** solo sirve donde los cordones se apilan, es decir, donde el relleno se repite capa a capa: grilla,
  triángulos, estrellas, panal, rectilíneo alineado, concéntrico y las curvas. En el rectilíneo, que gira 90°, las
  líneas solo se tocan en los cruces.
- **El campo:** dos ondas triangulares cruzadas, `w = A/4 · (T(p·u₁/λ) + T(p·u₂/λ))`, con `u₁` y `u₂` a 45° del
  patrón, así toda familia de líneas sube y baja. Al ser una función de la posición, Z es continua a lo largo de
  cualquier recorrido.
- **Valores de partida, los de ZigZagZ:** `A` = 2,5 alturas de capa, `λ` = 5 anchos de cordón (0,5 mm cada 2 mm con
  capas de 0,2 y boquilla de 0,4). Pendiente máxima de 26,6°.
- **Cerca de paredes y pieles:** la amplitud baja hasta cero según la distancia que da el campo de profundidad del
  cúbico adaptativo. Así el relleno toca las paredes plano, no choca con ellas ni con la boquilla, y empieza y termina
  plano bajo las pieles.
- **El cordón:** su espesor es la distancia al cordón de abajo, `h + w_i − w_{i−1}`; es igual a `h` donde las dos capas
  tienen la misma amplitud. Hace falta que el punto sepa dónde está el de abajo: un campo nuevo en `ToolpathPoint`.
  El escritor, el verificador y la vista previa lo usan para el espesor.
- **Velocidad:** Z se mueve a la velocidad de avance por la pendiente (0,5 veces a 26,6°). Muchas impresoras con
  husillo en Z no pasan de 10 a 20 mm/s en Z, así que el relleno ondulado se limita a lo que dé el eje Z de la
  impresora. Hace falta un ajuste nuevo, `machine.max_speed_z`, que también sirve para el resto del motor.

### N2 — Paredes tejidas (la versión no planar de brick layers)

En vez de subir perímetros enteros media capa, cada perímetro ondula en Z a lo largo de su recorrido:
- los perímetros vecinos van en contrafase, así cruzan alturas en alternancia, como un tejido;
- cada capa repite la forma de la de abajo (anidadas, como N1): la unión entre capas queda inclinada y la unión entre
  perímetros vecinos queda trabada en Z;
- abajo y arriba, la amplitud crece y se apaga a lo largo de varias capas, sin capas de media altura y sin bajar el
  flujo de una capa;
- el perímetro externo puede quedar plano (la superficie no cambia) o ondular apenas, según elija el dueño.

Comparado con lo que dejan libre los documentos de arriba: la Z cambia a lo largo del cordón; no hay cordones enteros a
media altura; no hay capas de media altura; no hay cordones terminales girados ni de medio ancho.

### N3 — Medir con el análisis propio

El análisis «como impresa» (`docs/ANALYSIS.md`) toma la unión entre capas como un plano horizontal. Con N1 y N2, cada
vóxel inclina sus ejes según la pendiente del cordón: el plano débil queda inclinado. Así se puede medir con el
análisis propio cuánto gana una pieza tirada en Z, y comparar con ZigZagZ.

La prueba real la hace el dueño: probetas de tracción en Z, planas y onduladas, que se agregan a las calibraciones.

### N4 — Pernos en Z (después de revisar la patente)

Es lo que más resiste según lo publicado (más de 3,5 veces en Z). Hay que revisar antes la patente de ORNL, que estaba
en trámite.

## Orden propuesto

1. N1 en grilla y rectilíneo alineado, con el campo de profundidad, el espesor por la capa de abajo y el tope de
   velocidad en Z. Pruebas: continuidad, anidado, sin choques con paredes ni pieles, y el G-code verificado.
2. N3, para medir N1 con el análisis.
3. N2, paredes tejidas.
4. N1 en los demás patrones que se repiten capa a capa.
5. N4, si la patente lo permite.

## Fuentes

- Allum, Kitzinger, Li, Silberschmidt, Gleadall, *ZigZagZ: Improving mechanical performance in extrusion additive
  manufacturing by nonplanar toolpaths*, Additive Manufacturing 38 (2021) 101715,
  [DOI 10.1016/j.addma.2020.101715](https://doi.org/10.1016/j.addma.2020.101715);
  [versión de los autores](http://fullcontrolgcode.com/wp-content/uploads/2021/07/ZigZagZ-Improving-mechanical-performance-in-extrusion-additive-manufacturing-by-nonplanar-toolpaths.pdf).
- Duty, Failla, Kim, Smith, Lindahl, Kunc, *Z-Pinning approach for 3D printing mechanically isotropic materials*,
  Additive Manufacturing 27 (2019) 175–184 ([ORNL](https://impact.ornl.gov/en/publications/z-pinning-approach-for-3d-printing-mechanically-isotropic-materia/)).
- Estudio de impacto con capas no planares de la ÉTS de Montreal
  ([PDF](https://espace2.etsmtl.ca/32397/1/46%20-%20Impact%20analysis%20on%20non-planar%203D-pr.pdf)).
- CNC Kitchen, *Brick layers make 3D prints stronger* ([nota](https://cnckitchen.com/blog/brick-layers-make-3d-prints-stronger)).
- Fabbaloo, sobre las patentes de brick layers
  ([nota](https://fabbaloo.com/news/bricklayers-a-new-slicing-technique-to-strengthen-3d-prints-but-patent-issues-loom)).
- Patentes: US5653925 (Stratasys), US11331848B2, US12017407B2 y US20230249394A1 (Addman).
- Konvicny, *Increasing z-strength by over-extrusion*, Technical Disclosure Commons (2021)
  ([ficha](https://www.tdcommons.org/dpubs_series/4247)).
