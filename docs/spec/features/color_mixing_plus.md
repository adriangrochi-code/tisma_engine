# Multicolor ampliado: lo que suma Full Spectrum

**Qué hace:** amplía la mezcla de colores de `color_mixing.md` (una capa, un filamento, en ciclo) con lo que agregó el
programa Full Spectrum, para sacar muchos más colores de pocos filamentos:
- cuatro formas de crear una mezcla: por proporción, por secuencia escrita, por color buscado y en degradé;
- todas las mezclas de a dos generadas solas;
- capas de distinto alto para cada filamento, y subcapas solo donde hay mezcla;
- la mezcla solo en la superficie, con el interior de un solo filamento;
- un sesgo que esconde un poco un filamento para correr el color;
- una predicción del color más fiel, con la transparencia de cada filamento;
- colores desde una imagen o desde un modelo con textura.

**Pedido del dueño (26-10-2026):** documentar las funciones de Full Spectrum para que el motor tenga su capacidad de
impresión multicolor, según cómo funciona Full Spectrum.

**Fase:** después de `color_mixing.md`. Las reglas 1 a 16 y 20 a 24 van primero; las de imágenes (25 a 29) después.

**Fuentes** (todas públicas; no se leyó código de Full Spectrum):
- el README del proyecto, [ratdoux/OrcaSlicer-FullSpectrum](https://github.com/ratdoux/OrcaSlicer-FullSpectrum)
  (AGPL-3.0), leído el 26-10-2026;
- las notas de sus versiones 0.7-alpha, 0.9.10 y 0.9.12, en
  [NewReleases](https://newreleases.io/project/github/ratdoux/OrcaSlicer-FullSpectrum/release/v0.9.12);
- la guía de Snapmaker, [«Getting Started with Full Spectrum Slicing»](https://www.snapmaker.com/blog/getting-started-with-full-spectrum-slicing/),
  sobre su versión 2.3.3 beta;
- la idea de capas alternadas de Prusa Research (fuente de `color_mixing.md`);
- la teoría de Kubelka y Munk (1931) para la predicción del color (regla 21).

Nada de esto se midió todavía en Full Spectrum como caja negra: los nombres de las opciones y los valores por defecto
salen de esos textos y están **a confirmar** midiendo el programa (pruebas MX al final). Las reglas marcadas
«(diseño)» son decisiones del motor.

## Ajustes nuevos

Grupo `mixing` (nombres del motor):

| Ajuste | Tipo | Defecto | Rango | Qué hace |
|---|---|---|---|---|
| `auto_pairs` | sí/no | sí | | Genera solo las mezclas 50 : 50 de cada par de filamentos (regla 5) |
| `min_part` | % | 15 | 0 a 50 | Parte mínima de un filamento en una mezcla buscada por color (regla 4) |
| `max_cycle` | entero | 4 | 2 a 64 | Largo máximo del ciclo que propone la búsqueda por color (regla 4) |
| `cadence_heights` | sí/no | no | | Cada filamento de una mezcla de dos usa su propio alto de capa (regla 8) |
| `step` | mm | el de la capa | de `machine.min_layer_height` a la capa | Altura en la que se cuenta el ciclo (regla 9) |
| `height_a`, `height_b` | mm | el de la capa | de `machine.min_layer_height` a `machine.max_layer_height` | Alto de las capas del primer y del segundo filamento |
| `subdivide` | sí/no | no | | Subcapas solo en las zonas con mezcla (regla 10) |
| `sub_min_height` | mm | 0,04 | 0,01 a la capa | Alto mínimo de una subcapa |
| `sub_first_layer` | sí/no | no | | `no`: la primera capa no se divide |
| `sub_infill` | sí/no | no | | También se divide el relleno de las zonas con mezcla |
| `surface_only` | sí/no | no | | La mezcla va solo en la cáscara de la pieza (regla 13) |
| `surface_depth` | mm | 0,8 | 0,1 a 5 | Grosor de la cáscara que lleva la mezcla |
| `extra_walls` | entero | 0 | 0 a 8 | Paredes de más en las zonas con mezcla o pintadas de otro color |
| `bias` | sí/no | no | | Muestra el sesgo de cada mezcla (regla 15) |
| `predictor` | texto | `"mixer"` | `mixer`, `spectral` | Cómo se predice el color (regla 21) |

Del filamento:

| Ajuste | Tipo | Defecto | Rango | Qué hace |
|---|---|---|---|---|
| `filament.transmission_distance` | mm | 0 (desconocido) | 0 a 100 | Distancia de transmisión (TD): el espesor en el que el filamento deja pasar casi nada de luz; se usa en la predicción (regla 21) |

De cada mezcla (en el proyecto, junto a los datos de `color_mixing.md`):

| Dato | Qué es |
|---|---|
| `type` | `ratio`, `pattern`, `match` o `gradient` (regla 1) |
| `pattern` | La secuencia escrita, para `pattern` |
| `target` | El color buscado, para `match` |
| `enabled` | Si se ofrece en las listas; una mezcla apagada no se puede asignar |
| `name` | Nombre visible; por defecto, el del color previsto más cercano (regla 6) |
| `bias` | De −1 a 1, para cada par (regla 15) |

## Crear mezclas

1. **Cuatro tipos**, en un diálogo con una pestaña para cada uno y la vista previa del color siempre visible:

| Tipo | Lo que pone la persona | Resultado |
|---|---|---|
| Proporción (`ratio`) | 2 filamentos con un deslizador, o 3 con un triángulo (cada vértice un filamento) | El ciclo de `color_mixing.md`, reglas 7 y 8 |
| Secuencia (`pattern`) | Un texto con la secuencia de capas, o clics en los números de los filamentos | Ese ciclo, tal cual (regla 2) |
| Color buscado (`match`) | Un color, con el selector o en hexadecimal de 6 cifras | La mezcla de 2 o 3 filamentos que más se acerca (regla 4) |
| Degradé (`gradient`) | De 2 a 4 filamentos y dónde va cada uno | Un color que cambia de un lado a otro de la pieza (regla 3) |

2. **Secuencia escrita:**
   - acepta números de filamento (`1`, `2`…) o letras (`A`, `B`…: el primero y el segundo de la mezcla);
   - los separadores (espacio, coma, `/`, `-`) se ignoran: `1,1,2`, `1 1 2` y `AAB` son lo mismo;
   - la parte de cada filamento se calcula de la secuencia y se muestra;
   - la vista previa muestra los primeros 20 pasos, pero se imprime la secuencia entera, repetida;
   - un número de filamento que no existe marca el campo en rojo y no deja aceptar.
3. **Degradé:** de 2 a 4 filamentos, en un eje (X, Y o Z) o entre dos puntos de la pieza. Cada capa (o cada zona
   de la capa, si el degradé es en X o Y) usa la proporción del lugar, con el ciclo de `color_mixing.md`, regla 10.
   El degradé en X o Y necesita `subdivide` (regla 10) o cambia en escalones de una capa.
4. **Color buscado:**
   - se prueban todas las mezclas de 2 y de 3 filamentos con ciclos de hasta `max_cycle` capas, sin partes menores que
     `min_part`;
   - se elige la de menor distancia de color (CIEDE2000) con la predicción de la regla 21;
   - el diálogo muestra la elegida y las 3 siguientes como «recomendaciones», cada una con su distancia;
   - si la mejor está a más de 10 de distancia, un aviso dice que ese color no se alcanza con estos filamentos (diseño:
     el umbral).
5. **Pares automáticos:** con `auto_pairs`, al cargar filamentos se crean todas las mezclas 50 : 50 de dos: con 4
   filamentos, 6 mezclas (10 colores en total). Se pueden apagar de a una (`enabled`) o todas desde Preferences.
6. **Lista de mezclas:**
   - cada fila: muestra del color, nombre, componentes («Filament 1 + Filament 2»), proporción y casilla para apagarla;
   - el nombre por defecto es el del color previsto («Light purple»…) y la persona lo puede cambiar;
   - numeración: después de los filamentos reales (`color_mixing.md`);
   - acciones: editar, duplicar, «Transfer to» (pasa las piezas que la usan a otra mezcla o filamento), «Merge with»
     (une dos mezclas iguales) y borrar, con un diálogo que dice cuántas piezas la usan.
7. **Asignar:** una mezcla se asigna como un filamento: a una pieza, a una parte, a un modificador, o pintando caras.
   El balde de pintura rellena hasta un ángulo entre caras que la persona elige.

## Imprimir la mezcla

8. **Alto por filamento** (`cadence_heights`): en una mezcla de dos, las capas del primer filamento tienen
   `height_a` y las del segundo `height_b`. La proporción sale de los altos: A = 0,3 mm y B = 0,15 mm da 2 : 1 en
   espesor con una capa de cada uno.
   - solo en piezas con una sola mezcla en toda la altura; si hay zonas con otra mezcla o sin mezcla en las mismas
     capas, se avisa y se usa el ciclo normal (diseño, a confirmar en Full Spectrum);
   - los altos respetan los límites de la máquina y de la boquilla (`layers.height`).
9. **Paso** (`step`; el comportamiento exacto, a confirmar): la altura en la que se cuenta el ciclo; por defecto, la de la capa.
10. **Subcapas** (`subdivide`; en Full Spectrum se llamó «Local Z»):
    - en las zonas con mezcla, cada capa se divide en subcapas, una por filamento del ciclo o las que entren sin bajar
      de `sub_min_height`;
    - el resto de la capa (zonas de un solo filamento) se imprime una vez, con el alto entero;
    - cada subcapa se imprime a su altura, con el flujo de su alto;
    - así, una capa de 0,2 mm con una mezcla 1 : 1 lleva 0,1 mm de cada filamento: el color se mezcla en la mitad de
      altura y las franjas se notan menos;
    - con `sub_first_layer` en `no`, la primera capa no se divide; con `sub_infill` en `no`, el relleno de la zona
      tampoco (va con el filamento de la última subcapa).
11. **Subcapas con varias zonas:** dos zonas con mezcla en la misma capa pueden tener subcapas distintas; cada una
    termina exactamente en la altura de la capa, para que la capa siguiente empiece pareja.
12. **Costo de las subcapas:** cada subcapa es un cambio de filamento más. El programa lo cuenta en la estimación y
    en el aviso de costo (`multimaterial.md`, regla 9) antes de laminar.
13. **Solo la superficie** (`surface_only`): la mezcla se imprime en una cáscara de `surface_depth` de grosor
    (paredes y superficies de arriba y abajo); el interior de la zona va con un solo filamento, el de la mayor parte de
    la mezcla.
    - las capas sin superficie visible de esa zona no cambian de filamento;
    - con paredes de 0,4 mm y `surface_depth` de 0,8 mm, la mezcla va en las dos paredes de afuera.
14. **Paredes de más** (`extra_walls`): en las zonas con mezcla o pintadas, se suman paredes para que el color de
    afuera no se vea mezclado con el del relleno.
15. **Sesgo** (`bias` de la mezcla, de −1 a 1): en una mezcla de dos, un sesgo positivo hunde un poco las paredes de
    afuera de las capas del segundo filamento, y uno negativo las del primero. El color se corre hacia el otro sin
    cambiar el ciclo.
    - lo que se hunde, en mm: `bias` × la mitad del ancho de la pared externa (diseño, a confirmar en Full Spectrum);
    - la vista previa del color muestra el corrimiento estimado.
16. **Pintado:** una cara pintada con una mezcla se imprime como una zona con mezcla (reglas 10 a 15), igual que una
    pieza entera.
17. **Vista previa antes de laminar:** las piezas y las caras con mezcla se ven con el color previsto, y un degradé
    con un color único (el del medio). Después de laminar se ven las capas reales; desde el frente o el costado se
    distinguen las subcapas.
18. **Mezclas con soportes, relleno y torre:** los soportes y la torre de purga no usan mezclas; el relleno sí, salvo
    con `surface_only`.
19. **Una capa sin mezclas no cambia:** el G-code de una pieza sin mezclas es igual con o sin estas opciones.

## Predecir el color

20. **Mezclador simple** (`mixer`, por defecto): el color de la mezcla es el promedio de los colores de sus
    filamentos, pesado por su parte, calculado en un espacio de color perceptual (diseño: Oklab).
21. **Espectral** (`spectral`): con la teoría de Kubelka y Munk, cada filamento tiene absorción y dispersión que
    salen de su color y de su `transmission_distance`.
    - con la TD en 0 se usa el mezclador simple para ese filamento;
    - un filamento más translúcido (TD más alta) pesa más en la mezcla que su parte, porque deja ver el de abajo;
    - la guía de Snapmaker recomienda TD de 5 a 8 para mezclar; con TD de 3 o menos se notan franjas.
22. **Calibración del color:** una pieza de prueba con una muestra de cada mezcla, y un nombre al lado de cada
    muestra:
    - «paleta rápida» de 10 colores (ciclos de hasta 2 capas) y de 26 (ciclos de hasta 3 capas con 4 filamentos);
    - se agrega a `calibration_models.md` como un modelo más;
    - la persona puede fotografiar la muestra impresa y corregir el color previsto de cada mezcla a mano.
23. **Recomendaciones** que muestra el diálogo, de la guía de Snapmaker:
    - capas de 0,08 a 0,16 mm para mezclas; 0,2 mm para degradés;
    - ciclos cortos: 2 o 3 capas se notan menos que 5 o más;
    - un ciclo que suma menos de 0,2 mm no se distingue a 60 cm;
    - con capas de más de 0,2 mm y ciclos de 4 o más, un aviso dice que se van a ver franjas.
24. **Nombres de colores:** la lista de nombres (regla 6) es propia del motor, en inglés, con su traducción.

## Colores desde una imagen (después)

25. **Modelos con color:** abrir OBJ, glTF/GLB, PLY, FBX, DAE o 3DS conserva la textura, las coordenadas de textura
    y los colores por vértice o por cara (`formats.md`). El 3MF del proyecto guarda la textura.
26. **Proyectar una imagen:** una herramienta pone un PNG o un JPEG sobre las caras conectadas de la pieza, sin
    coordenadas previas, con tamaño, posición, giro, profundidad y ángulo máximo de superficie, en la vista y con
    números.
27. **De la imagen a filamentos:** los colores de la imagen se reducen a los filamentos y a las mezclas que hay (o se
    crean mezclas nuevas con la regla 4). Tres formas:
    - «Normal»: cada zona del color más cercano se asigna a ese filamento o mezcla;
    - un filamento por capa, y las paredes de afuera se corren hacia adentro o hacia afuera según el color de la imagen
      en ese punto (el ancho del cordón no cambia);
    - ciclos locales: cada zona tiene su propio ciclo; más cambios de filamento, color más fiel.
28. **Muestreo:** la imagen se lee a lo largo de la pared cada 0,16 mm (normal), 0,08, 0,04 o 0,02 mm (es un paso en
    el plano, no un alto de capa).
29. **Ajustes de la imagen:** exposición, contraste, saturación, realce de bordes y gamma, con vista previa en vivo
    del color que se puede imprimir.

## Lo que no se toma

- **Mezcla en la misma capa** («Pointillisme»): puntos de distintos filamentos dentro de una capa. Sus notas lo
  llaman «probablemente un camino sin salida» para producción. Queda fuera hasta que haya resultados.
- **Alto de capa variable dentro de una pasada** («Adaptive Local-Z»): mueve X, Y y Z a la vez y necesita firmware que
  lo acepte. Queda para cuando el motor imprima capas no planares de ese tipo (`nonplanar.md`).

## Archivo del proyecto

30. Las mezclas nuevas van en el mismo archivo que las de `color_mixing.md` (regla 14 de esa ficha), con `type`,
    `pattern`, `target`, `enabled`, `name` y `bias`. Un proyecto que solo tiene mezclas por proporción se sigue
    escribiendo igual que antes, para que lo abran las versiones anteriores.
31. Abrir un 3MF de Full Spectrum: se leen sus mezclas si el formato está documentado; si no, la pieza conserva su
    número de filamento y un aviso dice que las mezclas no se pudieron leer (a confirmar con un archivo real).

## Pruebas

Con la caja de `color_mixing.md` (20 × 20 × 10 mm, capas de 0,2 mm, una boquilla con 4 filamentos y torre de purga),
salvo donde se dice.

| # | Escenario | Resultado esperado |
|---|---|---|
| MX1 | 4 filamentos con `auto_pairs` | 6 mezclas 50 : 50, numeradas de 5 a 10 |
| MX2 | Apagar la mezcla 5 | No aparece para asignar; las piezas que la usaban avisan al laminar |
| MX3 | Secuencia `1,1,2` y `AAB` | El mismo G-code: capas 1, 1, 2 repetidas; parte del 1: 66,7 % |
| MX4 | Secuencia con el filamento 9 que no existe | El campo en rojo; no se puede aceptar |
| MX5 | Color buscado `#808080` con filamentos blanco y negro | Una mezcla de blanco y negro; las 3 recomendaciones con su distancia |
| MX6 | Color buscado `#00FF00` con filamentos blanco y negro | El aviso de que el color no se alcanza |
| MX7 | `cadence_heights` con A = 0,3 y B = 0,15 | Capas de 0,3 mm con el 1 y de 0,15 mm con el 2, alternadas; ninguna fuera de los límites de la máquina |
| MX8 | `subdivide` con mezcla 1 : 1 y capas de 0,2 mm | En cada capa, dos pasadas de 0,1 mm (una de cada filamento) en la zona con mezcla |
| MX9 | MX8 con la mitad de la caja sin mezcla | La mitad sin mezcla se imprime una vez por capa, con 0,2 mm |
| MX10 | MX8 con `sub_first_layer` en `no` | La primera capa entera con un filamento |
| MX11 | `surface_only` con `surface_depth` 0,8 | Solo las 2 paredes de afuera y las superficies alternan; el relleno de un solo filamento |
| MX12 | MX11 contra la caja sin `surface_only` | Menos cambios de filamento en MX11 en las capas que tienen relleno |
| MX13 | `bias` 0,5 en una mezcla 1 : 1 | Las paredes de afuera de las capas del segundo filamento quedan más adentro que las del primero |
| MX14 | `extra_walls` 2 en una zona pintada | 2 paredes más en esa zona |
| MX15 | Degradé en Z de 4 filamentos | Cada tramo de la altura usa sus filamentos; debajo del primero, todo el primero |
| MX16 | Pieza sin mezclas con todas las opciones prendidas | El G-code igual que sin las opciones |
| MX17 | Predicción `spectral` con la TD en 0 | El mismo color que con `mixer` |
| MX18 | Paleta rápida de 10 colores con 4 filamentos | 10 muestras con su nombre; una por cada mezcla de hasta 2 capas |
| MX19 | Proyecto con mezclas por proporción guardado y abierto en una versión anterior | Se abre igual (regla 30) |
| MX20 | Medir en Full Spectrum los valores marcados «a confirmar» | Se completan o corrigen las reglas 8, 9, 15 y 31 |
