# Lo nuevo de OrcaSlicer 2.5: qué toma el motor

**Qué hace:** recorre las novedades de OrcaSlicer 2.5.0 alpha y dice, para cada una, si el motor ya la tiene, si
entra con las reglas de esta ficha o si queda para una ficha propia más adelante. Las reglas de los ajustes están
escritas para que el motor las haga sin otra fuente.

**Pedido del dueño (29-10-2026):** «hay que ver sobre Orca 2.5 alpha qué integra y qué podemos integrar nosotros»;
«la idea sería integrarlo para Tisma Engine… dejando los planes prontos para que cuando el motor pueda seguir
trabajando, quede guardado».

**Fase:** por etapas, después de las fases de cada grupo (tabla «Plan»). Ninguna regla cambia lo que el motor hace
hoy con los valores por defecto: cada ajuste nuevo viene apagado o con el valor que deja todo igual.

**Fuentes** (solo documentación pública, nada de código; ver `CLEANROOM.md`):
- OrcaSlicer, notas de la versión 2.5.0 alpha (16-10-2026):
  `https://www.orcaslicer.com/wiki/releases/release_2_5_0_alpha`.
- Wiki de OrcaSlicer, una página por grupo de ajustes (leídas el 29-10-2026), citadas en cada regla:
  - costura: `https://www.orcaslicer.com/wiki/print_settings/quality/quality_settings_seam.html`;
  - retracción: `https://www.orcaslicer.com/wiki/printer_settings/extruder/printer_extruder_retraction.html`;
  - capas de arriba y abajo: `https://www.orcaslicer.com/wiki/print_settings/strength/strength_settings_top_bottom_shells.html`;
  - voladizos: `https://www.orcaslicer.com/wiki/print_settings/quality/quality_settings_overhangs.html`;
  - costura precisa: `https://www.orcaslicer.com/wiki/print_prepare/prepare_precise_seam.html`;
  - precisión (polyholes): `https://www.orcaslicer.com/wiki/print_settings/quality/quality_settings_precision.html`;
  - relleno: `https://www.orcaslicer.com/wiki/print_settings/strength/strength_settings_infill.html`;
  - patrones: `https://www.orcaslicer.com/wiki/print_settings/strength/strength_settings_patterns.html`;
  - borde: `https://www.orcaslicer.com/wiki/print_settings/others/others_settings_brim.html`;
  - balsa: `https://www.orcaslicer.com/wiki/print_settings/support/support_settings_raft.html`;
  - velocidades: `https://www.orcaslicer.com/wiki/print_settings/speed/speed_settings_other_layers_speed.html`;
  - ventilador: `https://www.orcaslicer.com/wiki/material_settings/cooling/material_cooling.html`.

Donde la wiki no da un valor por defecto, la ficha elige el que no cambia nada y lo dice («elegido por la ficha»).

## Plan

| Etapa | Qué | Estado en el motor | Dónde |
|---|---|---|---|
| O1 | Ajustes chicos de paredes, costura y retracción | Nuevos | Reglas 1 a 6 |
| O2 | Superficies y relleno | Nuevos | Reglas 7 a 13 |
| O3 | Borde, balsa, soporte, ventilador, piel difusa | Casi todo nuevo; la balsa ya está | Reglas 14 a 18 |
| O4 | Mismo G-code para la misma entrada | Nuevo como regla y prueba | Regla 19 |
| O5 | Plantillas de G-code | Nuevo | Reglas 20 a 23 |
| O6 | Línea de comandos | Nuevo | Reglas 24 a 28 |
| O7 | Vista e interfaz | Para fichas de interfaz | Regla 29 |
| O8 | Funciones grandes | Comparadas con las fichas que ya hay | Regla 30 |

Orden propuesto: O4 primero (es una prueba que protege todo lo demás), después O1, O2, O5, O6, O3, y O7–O8 cuando la
interfaz y sus fichas lo pidan.

## Ajustes

| Clave | Tipo | Por defecto | Rango | Regla |
|---|---|---|---|---|
| `retraction.wipe_inward` | sí/no | no | | 1 |
| `retraction.wipe_inward_distance` | mm o % | 50 % | 0 a 100 % del ancho de la pared externa | 1 |
| `retraction.after_wipe` | % | 0 | 0 a 100 − `retraction.before_wipe` | 2 |
| `walls.unsupported_last` | sí/no | no | | 3 |
| `walls.polyholes` | sí/no | no | | 5 |
| `walls.polyhole_margin` | mm o % | 20 % | > 0 | 5 |
| `walls.polyhole_twist` | sí/no | no | | 5 |
| `walls.polyhole_max_edges` | entero | 50 | 3 a 1000 | 5 |
| `surfaces.top_expansion` | mm | 0 | 0 a 100 | 7 |
| `surfaces.top_expansion_margin` | mm | 0 | 0 a 100 | 7 |
| `surfaces.top_expansion_direction` | texto | `inward_and_outward` | `inward`, `outward`, `inward_and_outward` | 7 |
| `surfaces.top_fill_order`, `surfaces.bottom_fill_order` | texto | `default` | `default`, `outward`, `inward` | 8 |
| `surfaces.top_angle`, `surfaces.bottom_angle` | grados | −1 | −1, o 0 a 360 | 9 |
| `infill.adaptive_mode` | texto | `disabled` | `disabled`, `distance_warp`, `smooth_blend`, `stepped_shells`, `lobes`, `normal_z`, `normal_y`, `normal_x` | 10 |
| `infill.adaptive_interior_density` | % | 20 | 0 a 100 | 10 |
| `infill.adaptive_gradient` | texto | `linear` | `linear`, `quadratic`, `exponential` | 10 |
| `infill.smooth_factor` | % | 0 | 0 a 100 | 11 |
| `infill.separated` | sí/no | no | | 12 |
| `infill.multiline` | entero | 1 | 1 a 10 | 13 |
| `adhesion.brim_ears` | sí/no | no | | 14 |
| `adhesion.brim_ears_max_angle` | grados | 125 | 0 a 180 | 14 |
| `adhesion.brim_ears_detection_length` | mm | 1 | ≥ 0 | 14 |
| `adhesion.brim_ears_outer_only` | sí/no | no | | 14 |
| `speed.small_support` | mm/s o % | 50 % | > 0 | 16 |
| `speed.small_support_threshold` | mm | 0 | ≥ 0 | 16 |
| `cooling.first_layer_fan` | % | −1 | −1, o 0 a 100 | 17 |
| `gcode.skip_config_block` | sí/no | no | | 22 |

Los valores por defecto de `walls.polyhole_margin` (20 %, el ejemplo de la wiki), `adhesion.brim_ears_max_angle`
(125°), `adhesion.brim_ears_detection_length` (1 mm), `infill.adaptive_interior_density` (20 %) y los rangos que la
wiki no da los eligió la ficha; con el ajuste principal apagado no cambian nada.

## Reglas

### O1. Paredes, costura y retracción

1. **Limpiar hacia adentro** (`retraction.wipe_inward`; wiki, costura, «Wipe inward»):
   - el barrido que sigue a una pared externa se corre de la superficie de afuera hacia la pared interna de al lado,
     donde la marca no se ve;
   - `retraction.wipe_inward_distance` es cuánto se corre el camino del barrido, en mm o en % del ancho real de la
     pared externa; 50 % lo corre medio ancho de pared; más de 100 % no cambia nada más; 0 usa el barrido de siempre;
   - solo con `retraction.wipe` prendido y una pared interna ya impresa al lado de la costura: hace falta más de una
     pared y el orden de adentro hacia afuera; con `walls.outer_first` se usa el barrido de siempre;
   - en las calibraciones de avance de presión y de retracción se apaga solo.
2. **Retracción después del barrido** (`retraction.after_wipe`; wiki, retracción):
   - `retraction.before_wipe` (ya existe) es la parte de la retracción, en %, que se hace quieta antes del barrido;
     `retraction.after_wipe` es la parte que se hace quieta después; lo que queda se hace durante el barrido;
   - `after_wipe` se recorta a 100 − `before_wipe`: las tres partes nunca suman más que `retraction.length`.
3. **Paredes sin apoyo al final** (`walls.unsupported_last`; wiki, voladizos, «Unsupported wall last»):
   - un lazo de pared está sin apoyo cuando ninguna parte de él tiene algo debajo en la capa de abajo; un lazo que
     se apoya en otro lazo anclado también cuenta como anclado;
   - los lazos sin apoyo se imprimen después de las paredes que los anclan, de adentro hacia afuera, sea cual sea el
     orden de las paredes;
   - un lazo al lado de una pared apoyada queda con esa pared, antes del relleno, que lo usa de ancla;
   - un lazo que no toca nada se imprime en una segunda pasada de paredes, después del relleno de su capa;
   - un lazo que solo pueden anclar los puentes de la capa espera a que estén hechos;
   - vale con paredes clásicas y con anchos variables, y con el relleno antes o después.
4. **Modificador de costura precisa** (wiki, costura precisa): un cuerpo auxiliar pegado a la pieza, que no se
   imprime, decide dónde va la costura de la pared externa.
   - **Fuertes** (fijan un punto; `walls.seam` no lo mueve): centro, izquierda o derecha del tramo donde el cuerpo
     corta la línea media de la pared externa (medio ancho de pared hacia adentro). Izquierda y derecha se miran desde
     afuera de la pieza, así que se invierten en la pared de un agujero.
   - **Débiles** (marcan zonas; `walls.seam` elige igual): preferida, prohibida o neutra. La neutra borra también la
     costura pintada donde corta. Un débil manda sobre la pintura dentro de su zona.
   - Si un fuerte corta una pared en varios tramos, vale el más largo; en un empate, el más cercano al fondo de la
     cama y después el de más a la izquierda. En esa pared, los demás puntos quedan prohibidos.
   - Un fuerte que contiene la pared entera no se usa en esa pared. Un débil que la contiene: preferida marca toda la
     pared; neutra la limpia; prohibida la bloquea entera, y si queda bloqueada toda, la costura va como en una pared
     sin marcas y hay un aviso.
   - Prioridad: el orden de la lista de objetos, los de arriba primero, y los fuertes siempre arriba de los débiles.
   - Vale para la pared externa, también la de los agujeros; las internas siguen a la externa. La costura bufanda, el
     hueco de costura y el barrido arrancan del punto elegido. En modo vaso no hace nada.
   - Un solo aviso «Costura precisa:» junta los problemas: cuerpos que no llegan a la línea media, varios tramos,
     pared contenida por un fuerte, pared bloqueada entera.
   - Se guarda en el proyecto con el tipo de cada modificador.
5. **Polyholes** (`walls.polyholes`; wiki, precisión):
   - un agujero redondo se imprime como un polígono de pocos lados planos, apoyado en el borde de afuera del agujero
     para dar el diámetro pedido;
   - cuenta como redondo un agujero convexo de más de 8 puntos que no se aparta del círculo estimado más que
     `walls.polyhole_margin` (en mm o en % del radio); el mismo margen junta el agujero de capa en capa, y se
     convierte solo si dura al menos dos capas o está en la primera;
   - la cantidad de lados es más o menos el doble del diámetro en mm con boquilla de 0,4, proporcionalmente menos con
     boquillas más grandes, nunca menos de 3 ni más de `walls.polyhole_max_edges` (con 50, a partir de unos 25 mm);
   - con `walls.polyhole_twist`, el polígono gira en cada capa entre cinco orientaciones que se repiten cada cinco
     capas, para que las esquinas no se apilen.
6. **Lo que no hace falta:** «Seam gap» ya es `walls.seam_gap` y las costuras internas escalonadas ya son
   `walls.staggered_inner_seams`.

### O2. Superficies y relleno

7. **Expansión de la superficie de arriba** (`surfaces.top_expansion`; wiki, capas de arriba y abajo):
   - agranda las superficies de arriba esa distancia, para unir superficies separadas y llenar los huecos alrededor
     de lo que sale de ellas; 0 lo apaga; se aplica a la superficie original, antes de detectar puentes y voladizos;
   - `surfaces.top_expansion_margin` deja la superficie agrandada a esa distancia de la banda de paredes (una sola
     pared si se pidió una sola pared arriba);
   - `surfaces.top_expansion_direction`: `inward` crece hacia los agujeros que dejan los detalles del medio,
     `outward` crece por el borde de afuera para unir superficies partidas, `inward_and_outward` las dos;
   - con la expansión prendida, las paredes internas que quedan sobre la superficie de arriba se sacan para que la
     superficie siga entera: las paredes de ancho fijo enteras, las de ancho variable solo en el tramo de arriba;
   - sin efecto con 0 capas de arriba o con densidad de superficie 0 %.
8. **Orden del relleno de arriba y de abajo** (`surfaces.top_fill_order`, `surfaces.bottom_fill_order`; misma
   página): solo en patrones que salen del centro (concéntrico, espiral de la regla 13 bis, cuerdas de Arquímedes,
   espiral octagrama). `default`: por el camino más corto, en cualquier sentido; `outward`: del centro al borde (el
   sobrante va al borde, mejor arriba); `inward`: del borde al centro (mejor abajo: las curvas de afuera pegan más
   en la primera capa).
9. **Ángulo fijo arriba y abajo** (`surfaces.top_angle`, `surfaces.bottom_angle`; wiki, relleno): ángulo en grados
   de las líneas de las superficies de arriba y de abajo; −1 sigue la dirección del relleno sólido. El ángulo de
   arriba vale también para el planchado (`ironing.md`).
10. **Densidad adaptativa en gyroid y TPMS** (`infill.adaptive_mode`; wiki, patrones):
    - solo con los patrones gyroid, TPMS-D y TPMS-FK; las celdas son finas al lado de las paredes y de las
      superficies de arriba y abajo y crecen hacia el centro de la pieza;
    - en la superficie, la densidad es `infill.density` (S); en el centro, `infill.adaptive_interior_density` (I),
      que puede ser mayor que S para un núcleo más denso; con t de 0 en la superficie a 1 en el punto más lejano de
      ella:

      | `infill.adaptive_gradient` | Densidad a la profundidad t |
      |---|---|
      | `linear` | S + (I − S) × t |
      | `quadratic` | S + (I − S) × t² |
      | `exponential` | S × (I / S)^t |

    - modos: `distance_warp`, un solo patrón deformado según la distancia a la superficie más cercana;
      `smooth_blend`, los patrones de densidades vecinas mezclados; `stepped_shells`, cáscaras con el patrón normal
      a su densidad, cada una unas 1,5 veces más densa que la de adentro; `lobes`, cada lóbulo (una parte unida al
      resto por un cuello más angosto) hacia su propio centro, y en una pieza alta el centro está a media altura;
      `normal_z`, `normal_y`, `normal_x`, cada corte normal a ese eje por separado, sin cambiar a lo largo de él;
    - con `infill.density` 100 % el relleno es sólido y los modos no se aplican; una pieza hecha solo de partes más
      finas que la grilla del campo (0,5 mm, hasta unos mm en piezas muy grandes) usa el patrón normal;
    - es experimental en OrcaSlicer: el motor la marca igual en la interfaz.
11. **Suavizar las esquinas del relleno** (`infill.smooth_factor`; wiki, relleno):
    - cambia las vueltas cerradas del relleno poco denso por curvas: 0 % deja el camino como está, 100 % la curva más
      grande que entra entre líneas vecinas;
    - una curva nunca usa más de la mitad del tramo más corto a cada lado de la esquina, así que dos curvas no se
      pisan; las vueltas en horquilla (más cerradas que unos 154°) quedan en punta;
    - patrones: Hilbert, espiral octagrama, lightning, panal, panal 3D, concéntrico y cruzado; grilla, triángulos,
      tri-hexágono y cúbico solo con `infill.multiline` mayor que 1;
    - el G-code crece (entre 1,5 y 8 veces el del camino en punta, según el patrón).
12. **Relleno separado por pieza** (`infill.separated`; wiki, relleno):
    - el relleno de cada parte se centra en su propia caja 3D, como si se laminara sola; apagado, todo el conjunto
      comparte un centro;
    - comparten centro las partes que se tocan o se pisan; tienen centro propio las de proyección separada (también
      objetos distintos), las islas sueltas de una misma malla y las partes entrelazadas que no se tocan;
    - vale para el relleno poco denso, el sólido interno y los puentes; no para las superficies de arriba y abajo;
    - no se ofrece con cúbico adaptativo ni cúbico de soporte (ya son por parte); no cambia lightning; el concéntrico
      ya sigue el contorno de cada parte.
13. **Relleno de varias líneas** (`infill.multiline`; wiki, relleno):
    - cada camino del relleno se imprime como hasta 10 líneas paralelas pegadas; 1 es el relleno de siempre;
    - con grilla, triángulos, tri-hexágono, cúbico, cúbico adaptativo, cúbico de soporte y panal 3D se usan caminos
      en trapecio que no se cruzan a sí mismos, y el patrón gira en cada capa;
    - en cúbico, los caminos siguen las mismas líneas que el patrón de una sola línea; en cúbico adaptativo y de
      soporte, en cada tamaño de cubo.

13 bis. **Espiral hacia adentro** (patrón `spiral_inset` para `surfaces.top_pattern`, `surfaces.bottom_pattern`, el
relleno sólido interno y `support.interface_pattern`; wiki, patrones):
- es un concéntrico cuyos lazos se unen en una sola espiral: cada lazo se corta un poco antes y sigue derecho al
  lazo de adentro;
- la punta de afuera va en una esquina convexa del lazo de afuera, mejor si es cerca de un ángulo recto; cada lazo
  de adentro sigue desde el punto más cercano al final del de afuera; el paso de un lazo al otro se extruye, no se
  viaja;
- empieza otra espiral cuando el lazo siguiente no se alcanza con un paso corto por dentro de la superficie: otra
  isla, más de cuatro separaciones de distancia, o un paso que cruzaría un agujero o una muesca;
- en las superficies sólidas de arriba y abajo los lazos tienen ancho variable; en el sólido interno, las interfaces
  y las superficies de menos de 100 % el ancho es fijo;
- una línea corta al centro llena el agujerito que puede dejar el último lazo;
- el sentido lo pone la regla 8.

### O3. Borde, balsa, soporte, ventilador y piel difusa

14. **Orejas de borde** (`adhesion.brim_ears`; wiki, borde):
    - en lugar del borde entero, discos chicos en las esquinas filosas del contorno, de radio `adhesion.brim_width`
      (en la interfaz se llama «Radio de las orejas» con este modo);
    - van donde el ángulo del contorno es más cerrado que `adhesion.brim_ears_max_angle`: 0 no pone ninguna, 180 en
      casi cualquier punto que no sea recto;
    - antes de buscar esquinas, el contorno se simplifica con desvíos de `adhesion.brim_ears_detection_length`; 0 no
      simplifica;
    - con `adhesion.brim_ears_outer_only`, solo en el contorno de afuera: ninguna oreja dentro de agujeros o zonas
      cerradas; vale también para las orejas pintadas.
15. **Balsa:** `adhesion.raft_expansion` y `adhesion.raft_first_layer_expansion` ya cubren «Raft expansion» de
    OrcaSlicer 2.5 (cada capa de la balsa se agranda; la primera, además, lo suyo). Falta solo la prueba OR15.
16. **Soporte chico más lento** (`speed.small_support`, `speed.small_support_threshold`; wiki, velocidades):
    - una extrusión de soporte es chica cuando su largo es ≤ 2π × el umbral (el umbral es un radio, en mm);
    - esas van a `speed.small_support`, en mm/s o en % de la velocidad de soporte o de interfaz que les toca;
    - umbral 0, el valor por defecto, lo apaga.
17. **Ventilador de la primera capa** (`cooling.first_layer_fan`; wiki, ventilador):
    - un valor exacto del ventilador en %, solo para la primera capa, por encima de los demás ajustes de
      enfriamiento; −1 lo apaga;
    - solo con `cooling.fan_off_layers` en 0; desde la segunda capa sigue el enfriamiento normal;
    - con `cooling.full_fan_layer`, la subida arranca desde este valor en la primera capa (y no desde 0).
18. **Piel difusa y puentes:** la piel difusa no se aplica a las paredes externas que cuelgan sobre nada (las que van
    como puente). Cambia `fuzzy_skin.md`; prueba OR18.

### O4. Mismo G-code para la misma entrada

19. **Determinismo** (notas de la 2.5, «Consistent Slicing Results»):
    - la misma pieza con los mismos ajustes da el mismo G-code byte por byte, en cada laminado y en cada exportación,
      con cualquier cantidad de núcleos del procesador;
    - incluye el relleno lightning, los soportes de árbol, las velocidades por voladizo, los puntos de pared de varios
      materiales y la numeración de los objetos en los comentarios;
    - solo cambian las líneas que dicen la fecha o la hora, si la plantilla las pide.

### O5. Plantillas de G-code

20. **`regex_replace(texto, /patrón/, reemplazo)`** en las plantillas (nombre de archivo, G-code propio,
    compatibilidad): reemplaza con una expresión regular, con grupos de captura (`$1`). Ejemplo: sacar `.stl` del
    nombre del objeto. Se suma a las funciones de `gcode_templates.md`, regla 5.
21. **Partes del tiempo de impresión:** `print_time_day`, `print_time_hour`, `print_time_minute` y `print_time_sec`
    dan cada parte del tiempo estimado, y `print_time_total_sec` el total en segundos. Se resuelven al final, como
    `print_time` (`gcode_templates.md`).
22. **Sin el bloque de ajustes** (`gcode.skip_config_block`): el G-code sale sin el bloque de ajustes del final (para
    firmwares que rechazan esas líneas de comentario). Un G-code así no puede devolver sus ajustes al abrirlo.
23. **Temperaturas de la primera capa según los filamentos que se usan:** la línea de la cama sigue el modo de la
    temperatura de cama (el primer filamento, o el más caliente de los usados) y la de la boquilla usa el primer
    filamento que imprime, no siempre el filamento 1.

### O6. Línea de comandos

Las notas de la 2.5 nombran estas opciones sin detallar el contenido de los archivos; los campos y el código de salida
de abajo los propone la ficha, con los nombres que ya usa el motor.

24. **`--strict`:** un aviso que hoy deja laminar pasa a ser error: no se escribe el G-code y la salida es el código 4
    (problema del modelo o de los ajustes, como hoy); el archivo de avisos (`--warnings`) lista igual todos.
25. **Apoyar la pieza:** `--ground-largest-face` apoya la cara plana más grande sobre la cama;
    `--ground-face-normal X,Y,Z` apoya la cara con esa normal; `--ground-face-point X,Y,Z` la cara que pasa por ese
    punto.
26. **Inspeccionar:** `--inspect-mesh` escribe un resumen en JSON de la malla (tamaño, volumen, triángulos, si está
    cerrada, partes); `--inspect-paint` lo mismo de lo pintado (costura, soporte, colores, piel difusa) por tipo.
27. **Estadísticas:** `--export-statistics archivo.json` escribe por placa el tiempo, el filamento por extrusor
    (mm, cm³, g, costo) y la cantidad de capas; `--export-settings -` escribe los ajustes resueltos por la salida
    estándar.
28. **Copias y giros:** `--align-to-y-axis` ordena las copias a lo largo de Y; girar las piezas al ordenarlas está
    apagado salvo que se pida.

### O7. Vista e interfaz

29. Van a sus fichas cuando se haga esa parte de la interfaz:
    - vista: rayos X (las piezas semitransparentes), corte con barra de profundidad, centro de masa con peso e
      inercia (encaja con `print_review.md` y `stability.md`), atenuar las capas de abajo en la vista previa
      (`preview.md`);
    - interfaz: paleta de comandos con la barra espaciadora, con favoritos (encaja con `assistant.md`); atajos de
      teclado configurables; «Publicar 3MF» con impresora, filamento y proceso; abrir un G-code exportado por el
      motor recupera sus perfiles (`project_save.md`).

### O8. Funciones grandes

30. Comparadas con lo que ya hay:
    - **mezcla de colores** («Mixed Filament», subcapas de color, degradé, «Decompose Color»): `color_mixing.md` y
      `color_mixing_plus.md` ya tienen mezcla por capas y subcapas; falta comparar «Decompose Color» (buscar la
      combinación de filamentos para un color) y la importación de glTF/GLB/FBX con colores;
    - **complementos**: `plugins.md` ya los especifica en cualquier lenguaje; de OrcaSlicer conviene sumar los
      permisos que se piden (archivos fuera de la carpeta, red, programas externos) y que el perfil recuerde qué
      complementos usa;
    - **impresoras de cinta**: `belt_printer.md`; de la 2.5 falta comparar el borde solo en el borde delantero, la
      torre de purga para cinta y la vista del G-code «diseñado» y «crudo»;
    - **IDEX/IQEX en paralelo** (copia, espejo, varias piezas a la vez): `multi_extruder.md`; ficha propia cuando
      llegue esa fase;
    - **textura en relieve** («Texture Displacement») y **pestaña de diseño CAD**: fichas propias; la de CAD encaja
      con las formas paramétricas pendientes (`model_tools.md`).

## Pruebas

Ajustes base de `walls_extra.md` (boquilla 0,4, capas de 0,2) salvo que se diga otro.

| # | Caso | Resultado |
|---|---|---|
| OR1 | Cubo de 20 mm, 3 paredes, `retraction.wipe` y `retraction.wipe_inward`, distancia 50 % | El barrido después de la pared externa va medio ancho de pared hacia adentro; con 0 % o con `walls.outer_first`, el barrido de siempre |
| OR2 | `retraction.length` 2, `before_wipe` 20 %, `after_wipe` 90 % | 0,4 mm quieta antes, 1,6 mm después (90 % recortado a 80 %), nada durante el barrido |
| OR3 | Pieza con un anillo que cuelga entero en una capa, `walls.unsupported_last` | El lazo sin apoyo va después del relleno de esa capa; apagado, en el orden de siempre |
| OR4 | Cilindro de 20 mm con un modificador de costura «centro» en un costado | La costura de todas las capas cae donde el cuerpo corta la línea media de la pared; con «prohibida» en ese mismo lugar, la costura nunca cae ahí |
| OR5 | Placa con agujeros de 3, 10 y 30 mm, `walls.polyholes` | Polígonos de unos 6, 20 y 50 lados (el de 30 mm topado en 50); con `polyhole_twist`, la orientación cambia cada capa y se repite cada 5 |
| OR6 | Dos cubos de 10 mm a 2 mm entre sí bajo una tapa común, `surfaces.top_expansion` 2 | La superficie de arriba sale en una sola pieza, a `top_expansion_margin` de las paredes |
| OR7 | Cubo, superficie de arriba concéntrica, `top_fill_order` `outward` | Los lazos van del centro al borde en todas las capas de arriba |
| OR8 | `surfaces.top_angle` 30 con planchado | Las líneas de arriba y las del planchado a 30° |
| OR9 | Cubo de 40 mm, gyroid 15 %, `adaptive_mode` `distance_warp`, interior 5 %, `linear` | Celdas más chicas cerca de las caras y más grandes en el centro; la densidad a media distancia es 10 % |
| OR10 | Panal 20 %, `infill.smooth_factor` 100 % | Sin esquinas en el relleno salvo las horquillas; menos tiempo de relleno que con 0 % |
| OR11 | Dos cubos separados, rectilíneo, `infill.separated` | Cada cubo tiene sus líneas de relleno centradas en él |
| OR12 | Cúbico 15 %, `infill.multiline` 3 | Tres líneas paralelas por camino, sin cruces de un camino consigo mismo |
| OR13 | Cuadrado de 20 mm, arriba `spiral_inset` | Una sola espiral por isla, con la punta de afuera en una esquina; el paso entre vueltas se extruye |
| OR14 | Cubo con un agujero cuadrado, `brim_ears` con ángulo 125 y `outer_only` | Cuatro orejas en las esquinas de afuera y ninguna en el agujero |
| OR15 | `raft_layers` 3, `raft_expansion` 1,5, `raft_first_layer_expansion` 2 | Las capas de la balsa a 1,5 mm de la pieza; la primera a 3,5 mm |
| OR16 | Soportes de árbol, `small_support_threshold` 2 | Las extrusiones de soporte de hasta 12,57 mm van a `speed.small_support` |
| OR17 | `cooling.first_layer_fan` 20, `fan_off_layers` 0, `full_fan_layer` 4 | 20 % en la capa 1, subiendo hasta el valor normal en la capa 4 |
| OR18 | Piel difusa externa en una pieza con un voladizo de pared | La pared que cuelga sobre nada sale lisa |
| OR19 | Pieza con lightning y soportes de árbol, laminada con 1, 2 y 8 núcleos y exportada dos veces | Los seis G-code son iguales byte por byte, salvo las líneas de fecha |
| OR20 | Nombre `{regex_replace(input_filename_base, /_v\d+$/, "")}.gcode` con `parte_v3.stl` | `parte.gcode` |
| OR21 | Impresión de 1 h 2 min 5 s, `{print_time_hour}:{print_time_minute}:{print_time_sec}` y `{print_time_total_sec}` | `1:2:5` y `3725` |
| OR22 | `gcode.skip_config_block` | El G-code termina sin el bloque de ajustes |
| OR23 | Dos filamentos, solo el 2 imprime la primera capa | La temperatura de boquilla de la primera capa es la del filamento 2 |
| OR24 | Ajuste que hoy da un aviso (por ejemplo, `no_heating`) con `--strict` | Termina con error y el aviso en el archivo de avisos |
| OR25 | Pieza en L acostada, `--ground-largest-face` | La cara más grande queda sobre la cama |
| OR26 | `--inspect-mesh` de un cubo de 20 mm | JSON con tamaño 20 × 20 × 20, volumen 8000 mm³, 12 triángulos, cerrada |
| OR27 | Dos placas, `--export-statistics` | Un JSON con tiempo, filamento por extrusor y capas de cada placa |
