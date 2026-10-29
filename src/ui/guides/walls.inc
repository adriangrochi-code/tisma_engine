// Explanations of the walls settings for setting_guide.cpp: {key, Spanish, English},
// one entry per setting. Own texts; see PROVENANCE.md.
    {"layers.heights",
     "Normalmente todas las capas tienen el mismo alto. Esta lista te deja cambiarlo por tramos según la altura sobre "
     "la cama: cada tramo dice «desde esta altura para arriba, capas de tanto». Debajo del primer tramo vale la altura "
     "de capa común, y vacía no cambia nada.\n\nSirve para imprimir rápido las partes simples y fino donde hay "
     "detalle, en la misma pieza. Cada altura queda dentro de lo que admite la impresora, y cada tramo empieza una "
     "capa nueva.\n\nEjemplo: un tramo en 0 mm con 0,28 mm y otro en 30 mm con 0,12 mm: la base de una figura sale "
     "rápida y, desde los 30 mm, la cabeza sale con detalle.",
     "Normally every layer has the same height. This list lets you change it in stretches by height above the bed: "
     "each entry says from this height up, use layers this thick. Below the first entry the normal layer height "
     "applies, and an empty list changes nothing.\n\nIt lets you print the simple parts fast and the detailed parts "
     "fine, in the same model. Each height is kept within what the printer allows, and each entry starts a new "
     "layer.\n\nExample: one entry at 0 mm with 0.28 mm and another at 30 mm with 0.12 mm: the base of a figure prints "
     "quickly and, from 30 mm up, the head prints with detail."},
    {"layers.cusp_height",
     "En una superficie inclinada cada capa deja un escaloncito, como una escalera. Este ajuste es el escalón más alto "
     "que aceptás, en mm, y el programa elige solo el alto de cada capa para no pasarlo: capas finas donde la "
     "superficie es casi plana (la punta de una cúpula, una rampa suave) y sin achicar donde las paredes son rectas. 0 "
     "lo apaga.\n\nMás chico da curvas más lisas y más capas; más grande imprime más rápido. Además, las caras planas "
     "quedan justo a su altura. Si usás capas de detalle, decide cuántas capas finas lleva cada capa.\n\nEjemplo: 0,1 "
     "mm con capas de 0,2 mm para una figura redondeada.",
     "On a sloping surface each layer leaves a small step, like a staircase. This setting is the tallest step you "
     "accept, in mm, and the program picks each layer's height by itself so it is never exceeded: thin layers where "
     "the surface is nearly flat (the top of a dome, a gentle ramp) and no thinning where the walls are straight. 0 "
     "turns it off.\n\nSmaller gives smoother curves and more layers; larger prints faster. Flat faces also land at "
     "their exact height. With detail layers on, it decides how many thin layers each layer gets.\n\nExample: 0.1 mm "
     "with 0.2 mm layers for a rounded figure."},
    {"layers.detail_height",
     "Con este ajuste la pieza puede tener capas de distinto alto a la misma altura: finas donde hay detalle (una "
     "cúpula, una punta, el tope de algo) y gruesas donde la forma no cambia, como una pared recta o el interior. Es "
     "el alto de esas capas finas, en mm; 0 lo apaga.\n\nCada capa normal se parte en 2 a 4 capas finas solo donde "
     "hace falta, así ganás detalle sin pagar el tiempo de imprimir toda la pieza fina. Tiene que ser menor que la "
     "altura de capa, y ninguna capa fina baja de 0,04 mm.\n\nEjemplo: con capas de 0,2 mm, 0,1 mm parte cada capa en "
     "dos donde hay detalle; 0,05 mm, en cuatro.",
     "With this setting the part can have layers of different heights at the same height: thin where there is detail "
     "(a dome, a tip, the top of something) and thick where the shape does not change, like a straight wall or the "
     "inside. It is the height of those thin layers, in mm; 0 turns it off.\n\nEach normal layer is split into 2 to 4 "
     "thin layers only where needed, so you gain detail without paying for printing the whole part thin. It has to be "
     "smaller than the layer height, and no thin layer goes below 0.04 mm.\n\nExample: with 0.2 mm layers, 0.1 mm "
     "splits each layer in two where there is detail; 0.05 mm, in four."},
    {"layers.detail_tolerance",
     "Solo cuenta si usás capas de detalle. Es el escalón que aceptás sin pasar a capas finas, en mm: donde la forma "
     "cambia de una capa fina a la otra menos que esto, esa zona se imprime gruesa igual.\n\nMás chico manda a capas "
     "finas hasta los cambios mínimos (más detalle, más tiempo); más grande deja más zonas gruesas y solo afina donde "
     "el cambio se nota.\n\nEjemplo: 0,05 mm, el de fábrica, va bien con boquilla de 0,4; subilo a 0,1 mm si ves capas "
     "finas en lugares donde no hacían falta.",
     "Only matters with detail layers on. It is the step you accept without switching to thin layers, in mm: where the "
     "shape changes less than this from one thin layer to the next, that area prints thick anyway.\n\nSmaller sends "
     "even the tiniest changes to thin layers (more detail, more time); larger leaves more thick areas and only goes "
     "thin where the change shows.\n\nExample: 0.05 mm, the default, works well with a 0.4 mm nozzle; raise it to 0.1 "
     "mm if you see thin layers where they were not needed."},
    {"layers.detail_margin",
     "Solo cuenta si usás capas de detalle. La zona en capas finas se agranda un poco alrededor del detalle, para que "
     "las paredes y el relleno que lo rodean también vayan finos y el paso no se note. Este ajuste es cuánto, en mm; 0 "
     "usa tres anchos de línea (unos 1,35 mm con boquilla de 0,4).\n\nMás margen da un paso más suave entre lo fino y "
     "lo grueso, a cambio de más tiempo; menos margen ahorra tiempo.\n\nEjemplo: 0 para empezar; 2 mm si ves un borde "
     "marcado alrededor del detalle.",
     "Only matters with detail layers on. The thin-layer area is grown a little around the detail, so the walls and "
     "infill around it are thin too and the change does not show. This setting is by how much, in mm; 0 uses three "
     "line widths (about 1.35 mm with a 0.4 mm nozzle).\n\nA larger margin gives a smoother change between thin and "
     "thick, at the cost of more time; a smaller one saves time.\n\nExample: 0 to start; 2 mm if you see a visible "
     "edge around the detail."},
    {"walls.width",
     "Es el ancho de cada línea de plástico de las paredes, en mm. La boquilla puede poner líneas algo más anchas o "
     "más finas que su agujero, aplastando más o menos el plástico. 0 elige solo: 1,125 veces la boquilla (0,45 mm con "
     "una de 0,4).\n\nLíneas más anchas dan paredes más fuertes y mejor pegadas entre sí; más finas marcan mejor los "
     "detalles chicos. Tiene que estar entre un cuarto y cuatro veces la boquilla; fuera de 0,6 a 2,5 veces, el "
     "programa te avisa.\n\nEjemplo: 0 casi siempre; 0,5 mm con boquilla de 0,4 para una pieza más robusta.",
     "It is the width of each plastic line in the walls, in mm. The nozzle can lay lines a little wider or narrower "
     "than its hole by squashing the plastic more or less. 0 picks it for you: 1.125 times the nozzle (0.45 mm with a "
     "0.4 mm one).\n\nWider lines make stronger walls that bond better; narrower ones show small details better. It "
     "must be between a quarter and four times the nozzle; outside 0.6 to 2.5 times, the program warns "
     "you.\n\nExample: 0 almost always; 0.5 mm with a 0.4 mm nozzle for a sturdier part."},
    {"walls.outer_width",
     "Es el ancho de la línea de la pared de afuera, la que se ve, en mm. Las paredes de adentro siguen con el ancho "
     "de las paredes. 0 usa 1,125 veces la boquilla (0,45 mm con una de 0,4), aunque hayas cambiado el ancho de las "
     "demás.\n\nUna línea externa más fina marca mejor los detalles y deja la superficie más prolija; más ancha, la "
     "pared de afuera queda más firme.\n\nEjemplo: con boquilla de 0,4, la externa a 0,42 mm para que la figura se vea "
     "prolija y las internas a 0,5 mm para que aguante.",
     "It is the width of the outer wall's line, the one you see, in mm. The inner walls keep the wall width. 0 uses "
     "1.125 times the nozzle (0.45 mm with a 0.4 mm one), even if you changed the width of the others.\n\nA narrower "
     "outer line shows details better and leaves a neater surface; a wider one makes the outer wall "
     "firmer.\n\nExample: with a 0.4 mm nozzle, the outer one at 0.42 mm for a neat-looking figure and the inner ones "
     "at 0.5 mm for strength."},
    {"layers.first_line_width",
     "Es el ancho de todas las líneas de la primera capa: paredes, relleno, falda, borde y soporte. Va en mm o en "
     "porcentaje del alto de la primera capa: el de fábrica, 200 %, con una primera capa de 0,2 mm da líneas de 0,4 "
     "mm. 0 usa los mismos anchos que las demás capas.\n\nLíneas más anchas apoyan más plástico sobre la cama y la "
     "pieza se pega mejor, aunque la primera capa tarda un poco más.\n\nEjemplo: 200 % casi siempre; 250 % (0,5 mm con "
     "una primera capa de 0,2) si la pieza se despega.",
     "It is the width of every line on the first layer: walls, infill, skirt, brim and support. It is given in mm or "
     "as a percentage of the first layer's height: the default, 200 %, with a 0.2 mm first layer gives 0.4 mm lines. 0 "
     "uses the same widths as the other layers.\n\nWider lines put more plastic on the bed and the part sticks better, "
     "though the first layer takes a little longer.\n\nExample: 200 % almost always; 250 % (0.5 mm with a 0.2 mm first "
     "layer) if the part comes loose."},
    {"surfaces.top_width",
     "Es el ancho de las líneas de la última capa de arriba, la superficie que se ve, en mm. 0 usa el diámetro de la "
     "boquilla (0,4 mm con una de 0,4), sin importar el ancho de las paredes.\n\nLíneas más finas dejan una superficie "
     "de arriba más lisa y pareja, con más pasadas; más anchas la terminan antes.\n\nEjemplo: 0 para la mayoría; 0,35 "
     "mm con boquilla de 0,4 si querés una tapa más prolija.",
     "It is the width of the lines on the very top layer, the surface you see, in mm. 0 uses the nozzle diameter (0.4 "
     "mm with a 0.4 mm nozzle), whatever the wall width is.\n\nNarrower lines leave a smoother, more even top surface, "
     "with more passes; wider ones finish it sooner.\n\nExample: 0 for most parts; 0.35 mm with a 0.4 mm nozzle if you "
     "want a neater lid."},
    {"surfaces.solid_width",
     "Es el ancho de las líneas de las capas macizas (las del piso y las de abajo del techo), salvo la superficie de "
     "arriba de todo, que tiene su propio ancho. En mm; 0 usa 1,125 veces la boquilla (0,45 mm con una de "
     "0,4).\n\nLíneas más anchas llenan esas capas más rápido; más finas tardan más. Como casi todas quedan "
     "escondidas, rara vez hace falta tocarlo.\n\nEjemplo: 0 casi siempre; 0,5 mm con boquilla de 0,4 para ganar algo "
     "de tiempo en una pieza grande.",
     "It is the width of the lines in the solid layers (the floor and the layers under the roof), except the very top "
     "surface, which has its own width. In mm; 0 uses 1.125 times the nozzle (0.45 mm with a 0.4 mm one).\n\nWider "
     "lines fill those layers faster; narrower ones take longer. Since most of them end up hidden, you rarely need to "
     "touch it.\n\nExample: 0 almost always; 0.5 mm with a 0.4 mm nozzle to save some time on a large part."},
    {"walls.seam_gap",
     "Si una vuelta de pared terminara justo donde empezó, el plástico se juntaría en ese punto y dejaría un granito "
     "en la costura. Por eso cada vuelta cerrada termina un poquito antes. Este ajuste es ese hueco, en mm o en "
     "porcentaje de la boquilla.\n\nMuy chico deja la costura abultada; muy grande deja un agujerito a la vista. No se "
     "usa con la costura en rampa ni en el modo vaso.\n\nEjemplo: 15 %, el de fábrica, son 0,06 mm con boquilla de "
     "0,4; subilo a 20 o 25 % si la costura queda con un bulto.",
     "If a wall loop ended exactly where it started, plastic would pile up at that point and leave a bump on the seam. "
     "So each closed loop stops a tiny bit early. This setting is that gap, in mm or as a percentage of the "
     "nozzle.\n\nToo small leaves a bulging seam; too large leaves a visible pinhole. It is not used with the ramped "
     "seam or in vase mode.\n\nExample: 15 %, the default, is 0.06 mm with a 0.4 mm nozzle; raise it to 20 or 25 % if "
     "the seam has a bump."},
    {"walls.scarf",
     "La costura en rampa (también llamada en bufanda) es otra forma de cerrar cada vuelta de la pared de afuera: en "
     "lugar de empezar de golpe a la altura de la capa, la línea arranca subiendo de a poco desde la capa de abajo, y "
     "al final de la vuelta pasa otra vez sobre esa rampa con una línea cada vez más fina. Las dos partes se completan "
     "y la costura casi desaparece.\n\nOpciones: apagada (de fábrica), solo en el contorno de afuera de cada pieza, o "
     "en todas partes, agujeros incluidos. Nunca va en la primera capa.\n\nEjemplo: en el contorno de afuera para un "
     "jarrón o una figura redonda, donde una costura común se notaría.",
     "The ramped seam (also called a scarf seam) is another way to close each loop of the outer wall: instead of "
     "starting abruptly at the layer's height, the line starts by rising gradually from the layer below, and at the "
     "end of the loop it passes over that ramp again with an ever thinner line. The two halves complete each other and "
     "the seam nearly disappears.\n\nOptions: off (the default), only on the outer outline of each part, or "
     "everywhere, holes included. It is never used on the first layer.\n\nExample: on the outer outline for a vase or "
     "a round figure, where a normal seam would show."},
    {"walls.scarf_length",
     "Solo cuenta con la costura en rampa encendida. Es el largo de la rampa a lo largo de la pared, en mm: el tramo "
     "en el que la línea va subiendo hasta la altura de la capa.\n\nUna rampa más larga reparte el cambio en más "
     "distancia y la costura se ve todavía menos, pero ocupa más parte de la pared; más corta concentra el cambio en "
     "menos lugar.\n\nEjemplo: 20 mm, el de fábrica, va bien en piezas medianas; en una pieza chica cuyas vueltas "
     "miden 30 mm, probá con 10 mm.",
     "Only matters with the ramped seam on. It is the length of the ramp along the wall, in mm: the stretch over which "
     "the line climbs up to the layer's height.\n\nA longer ramp spreads the change over more distance and the seam "
     "shows even less, but it covers more of the wall; a shorter one packs the change into less space.\n\nExample: 20 "
     "mm, the default, suits medium parts; on a small part whose loops are 30 mm long, try 10 mm."},
    {"walls.scarf_entire_loop",
     "Solo cuenta con la costura en rampa encendida. Encendido, la rampa ocupa la vuelta entera: la pared de afuera "
     "sube en espiral a lo largo de toda la capa y una segunda vuelta la completa.\n\nAsí no queda ningún punto de "
     "cambio marcado, a cambio de pasar dos veces por toda la pared, que tarda más. Apagado (de fábrica), la rampa "
     "ocupa solo su largo.\n\nEjemplo: apagado para casi todo; encendido para una pieza redonda y lisa, como un "
     "cilindro, donde no querés que se vea nada.",
     "Only matters with the ramped seam on. When on, the ramp covers the whole loop: the outer wall climbs in a spiral "
     "around the entire layer and a second lap completes it.\n\nThat leaves no visible change point at all, at the "
     "cost of going over the whole wall twice, which takes longer. Off (the default), the ramp only covers its set "
     "length.\n\nExample: off for almost everything; on for a smooth round part, like a cylinder, where you want "
     "nothing to show."},
    {"walls.scarf_start_height",
     "Solo cuenta con la costura en rampa encendida. Es la altura de donde arranca la rampa, en porcentaje de la capa, "
     "medida sobre la capa de abajo. Con 0 (de fábrica) la rampa sale pegada a la capa de abajo; con 50, desde la "
     "mitad de la capa.\n\nDesde 0 la línea empieza casi sin plástico, que da el cambio más suave pero a veces cuesta "
     "que salga parejo; empezar más arriba hace que la línea tenga algo de grosor desde el principio.\n\nEjemplo: 0 "
     "para empezar; probá 10 a 20 % si al comienzo de la rampa ves que falta plástico.",
     "Only matters with the ramped seam on. It is the height the ramp starts from, as a percentage of the layer, "
     "measured above the layer below. At 0 (the default) the ramp starts right on the layer below; at 50, from half "
     "the layer.\n\nFrom 0 the line starts with almost no plastic, which gives the smoothest change but can be hard to "
     "extrude evenly; starting higher gives the line some thickness from the beginning.\n\nExample: 0 to start; try 10 "
     "to 20 % if plastic is missing at the start of the ramp."},
    {"walls.scarf_max_segment",
     "Solo cuenta con la costura en rampa encendida. La rampa no sube en una sola línea: se parte en tramos cortos, "
     "cada uno un poco más alto que el anterior, y lo mismo su segunda pasada. Este ajuste es el largo máximo de cada "
     "tramo, en mm.\n\nTramos más cortos dan una subida más pareja, con más instrucciones en el archivo; más largos, "
     "una subida más escalonada.\n\nEjemplo: 1 mm, el de fábrica; 0,5 mm en una pieza chica con curvas cerradas.",
     "Only matters with the ramped seam on. The ramp does not climb in one single line: it is split into short pieces, "
     "each a little higher than the last, and so is its second pass. This setting is the longest each piece may be, in "
     "mm.\n\nShorter pieces give a smoother climb, with more instructions in the file; longer ones, a more stepped "
     "climb.\n\nExample: 1 mm, the default; 0.5 mm on a small part with tight curves."},
    {"walls.scarf_only_smooth",
     "Solo cuenta con la costura en rampa encendida. Encendido (de fábrica), la rampa se usa solo en las vueltas sin "
     "esquinas marcadas, las que nunca giran más de 30° de golpe. En las que tienen esquinas, la costura común va a "
     "una esquina, donde ya casi no se ve.\n\nApagado, todas las vueltas llevan rampa, también las de piezas con "
     "esquinas.\n\nEjemplo: dejalo encendido: en un cubo la costura queda en una esquina y en un cilindro va en rampa.",
     "Only matters with the ramped seam on. When on (the default), the ramp is only used on loops with no sharp "
     "corners, those that never turn more than 30° at once. On loops with corners, the normal seam goes into a corner, "
     "where it barely shows.\n\nWhen off, every loop gets a ramp, including those of parts with corners.\n\nExample: "
     "leave it on: on a cube the seam sits in a corner, and on a cylinder it is ramped."},
    {"walls.scarf_inner",
     "Solo cuenta con la costura en rampa encendida. Normalmente la rampa va solo en la pared de afuera; con esto "
     "encendido, las paredes de adentro también empiezan en rampa.\n\nComo esas paredes no se ven, el aspecto cambia "
     "poco: sirve sobre todo si notás que las costuras de adentro se marcan desde atrás en la superficie. Suma algo de "
     "tiempo.\n\nEjemplo: apagado casi siempre; encendido en una pieza translúcida de paredes finas, donde las "
     "costuras internas se transparentan.",
     "Only matters with the ramped seam on. Normally only the outer wall gets the ramp; with this on, the inner walls "
     "also start on a ramp.\n\nSince those walls are hidden, the look changes little: it mostly helps if you notice "
     "the inner seams showing through on the surface. It adds a little time.\n\nExample: off almost always; on for a "
     "translucent part with thin walls, where the inner seams show through."},
    {"slicing.xy_offset",
     "Agranda o achica cada capa de la pieza hacia los costados, en mm: un valor positivo la hace más ancha y uno "
     "negativo más angosta. Los agujeros hacen lo contrario: con un valor positivo se cierran un poco.\n\nSirve para "
     "corregir medidas cuando tu impresora saca las piezas algo más grandes o más chicas, sobre todo en piezas que "
     "tienen que encastrar. 0 no cambia nada.\n\nEjemplo: si un cubo de 20 mm sale de 20,2 mm, probá −0,1 mm, que se "
     "descuenta de cada lado.",
     "It grows or shrinks every layer of the part sideways, in mm: a positive value makes it wider and a negative one "
     "narrower. Holes do the opposite: with a positive value they close up a little.\n\nUse it to correct sizes when "
     "your printer makes parts slightly too big or too small, especially parts that have to fit together. 0 changes "
     "nothing.\n\nExample: if a 20 mm cube comes out at 20.2 mm, try −0.1 mm, which is taken off each side."},
    {"slicing.first_layer_shrink",
     "La primera capa se aplasta contra la cama y suele salir un poco más ancha que el resto: la base queda con un "
     "reborde, la «pata de elefante». Este ajuste achica solo la primera capa, en mm, además de lo que se achique el "
     "resto, para compensarlo.\n\nMás valor quita más reborde; demasiado deja la base metida hacia adentro. 0 no la "
     "toca.\n\nEjemplo: 0,1 a 0,2 mm si la base de tus piezas sale con reborde y no encastran.",
     "The first layer is squashed against the bed and often comes out a little wider than the rest: the bottom gets a "
     "lip, the elephant's foot. This setting shrinks only the first layer, in mm, on top of any shrinking of the rest, "
     "to make up for it.\n\nA higher value removes more of the lip; too much leaves the bottom tucked in. 0 leaves it "
     "alone.\n\nExample: 0.1 to 0.2 mm if the bottom of your parts has a lip and they do not fit."},
    {"walls.detect_overhangs",
     "Un voladizo es una parte de la pieza que sobresale sin nada debajo. Encendido (de fábrica), el programa detecta "
     "los tramos de pared que quedan enteros en el aire, o apoyados sobre el soporte, y los imprime con la velocidad y "
     "el ventilador de los puentes, pensados para eso.\n\nAsí esos tramos cuelgan menos y salen más prolijos. Apagado, "
     "toda la pared se imprime igual, sin importar qué tiene debajo.\n\nEjemplo: dejalo encendido; apagalo solo si "
     "querés que toda la pared vaya a la misma velocidad.",
     "An overhang is a part of the model that sticks out with nothing beneath it. When on (the default), the program "
     "finds the stretches of wall that sit entirely on air, or on support, and prints them with the bridge speed and "
     "fan, which are meant for that.\n\nThose stretches then droop less and come out cleaner. When off, the whole wall "
     "prints the same, whatever is under it.\n\nExample: leave it on; turn it off only if you want the whole wall at "
     "one speed."},
    {"walls.generator",
     "Elige cómo se dibujan las paredes. Con ancho variable (de fábrica), las líneas se ensanchan o se afinan para "
     "llenar justo el espacio, sin huecos, incluso en partes finas o en punta. Con ancho fijo, todas las vueltas "
     "tienen el mismo ancho, cada una donde entra entera, y lo que sobra en el medio se llena aparte con el relleno de "
     "huecos.\n\nEl variable deja las partes finas más sólidas; el fijo da líneas más regulares, todas iguales, que "
     "piden menos cambios de caudal.\n\nEjemplo: variable para casi todo; fijo si tu material no se lleva bien con los "
     "cambios de caudal, como algunos flexibles.",
     "It chooses how the walls are laid out. With variable width (the default), the lines widen or narrow to fill the "
     "space exactly, with no gaps, even in thin or pointed areas. With fixed width, every loop has the same width, "
     "each placed where a whole line fits, and what is left in the middle is filled separately with gap "
     "fill.\n\nVariable makes thin areas more solid; fixed gives more regular lines, all alike, that need fewer flow "
     "changes.\n\nExample: variable for almost everything; fixed if your material copes poorly with flow changes, like "
     "some flexibles."},
    {"walls.gap_fill",
     "Solo cuenta con las paredes de ancho fijo. Entre las paredes, o entre ellas y el relleno, a veces queda un "
     "espacio angosto donde no entra una línea entera. Encendido (de fábrica), los espacios de al menos media línea se "
     "llenan con líneas de ancho variable, a su propia velocidad.\n\nApagado, esos huecos quedan vacíos: se ahorra un "
     "poco de tiempo, pero la pieza queda más débil y pueden verse rendijas.\n\nEjemplo: dejalo encendido; apagalo "
     "solo en una pieza decorativa donde los huecos no importan.",
     "Only matters with fixed-width walls. Between the walls, or between them and the infill, a narrow space is "
     "sometimes left where a whole line does not fit. When on (the default), spaces of at least half a line are filled "
     "with variable-width lines, at their own speed.\n\nWhen off, those gaps stay empty: it saves a little time, but "
     "the part is weaker and slits may show.\n\nExample: leave it on; turn it off only for a decorative part where the "
     "gaps do not matter."},
    {"walls.clockwise",
     "Elige para qué lado da la vuelta la boquilla al imprimir las paredes: encendido, en el sentido de las agujas del "
     "reloj; apagado (de fábrica), al revés. Vale para todas las vueltas, también las de los agujeros y la espiral del "
     "modo vaso, y cada vuelta sigue empezando en la costura.\n\nEn general no cambia la calidad; algunas impresoras "
     "dejan la superficie un poco distinta según el sentido.\n\nEjemplo: dejalo apagado, salvo que quieras probar si "
     "tu impresora deja paredes más prolijas girando para el otro lado.",
     "It chooses which way the nozzle goes around when printing walls: when on, clockwise; off (the default), "
     "counterclockwise. It applies to every loop, including those of holes and the vase-mode spiral, and each loop "
     "still starts at the seam.\n\nIt usually does not change quality; some printers leave a slightly different "
     "surface depending on the direction.\n\nExample: leave it off, unless you want to test whether your printer makes "
     "neater walls going the other way."},
    {"walls.outer_first",
     "Elige el orden de las paredes en cada capa. Apagado (de fábrica), se imprimen de adentro hacia afuera y la de "
     "afuera va última, apoyada en las otras. Encendido, la de afuera va primero y después las de adentro.\n\nCon la "
     "de afuera primero, las medidas suelen salir más exactas porque nada la empuja hacia afuera; pero los voladizos "
     "sufren, porque esa pared no tiene dónde apoyarse de costado.\n\nEjemplo: apagado para casi todo; encendido para "
     "una pieza que tiene que encastrar justo y no tiene voladizos.",
     "It chooses the order of the walls on each layer. Off (the default), they print from the inside out and the outer "
     "wall goes last, leaning on the others. On, the outer wall goes first and then the inner ones.\n\nWith the outer "
     "wall first, sizes tend to come out more accurate because nothing pushes it outwards; but overhangs suffer, since "
     "that wall has nothing beside it to lean on.\n\nExample: off for almost everything; on for a part that has to fit "
     "precisely and has no overhangs."},
    {"walls.min_line_width",
     "Algunas partes de la pieza son más finas que una línea normal, como una aleta o el filo de una hoja. Este ajuste "
     "es el ancho mínimo de una línea, en mm o en porcentaje de la boquilla: una pared de una sola línea más fina que "
     "esto se imprime igual, con una línea de este ancho centrada en la pared.\n\nMás bajo respeta mejor el grosor "
     "real de las partes finas, pero las líneas muy finas salen débiles o cortadas; más alto las hace más firmes y "
     "algo más gruesas que el modelo.\n\nEjemplo: 85 %, el de fábrica, son 0,34 mm con boquilla de 0,4.",
     "Some parts of a model are thinner than a normal line, like a fin or a blade's edge. This setting is the "
     "narrowest a line may be, in mm or as a percentage of the nozzle: a one-line wall thinner than this is still "
     "printed, with a line this wide centred on the wall.\n\nLower follows the real thickness of thin parts more "
     "closely, but very thin lines come out weak or broken; higher makes them sturdier and a bit thicker than the "
     "model.\n\nExample: 85 %, the default, is 0.34 mm with a 0.4 mm nozzle."},
    {"walls.min_detail",
     "Es el grosor por debajo del cual una parte de la pieza ni se imprime, en mm o en porcentaje de la boquilla. Lo "
     "que sea más fino, como un relieve casi plano o un filo de papel, se descarta. Si por esto la pieza entera queda "
     "vacía, el programa da un error.\n\nMás bajo intenta imprimir detalles más finos (que se engrosan hasta el ancho "
     "mínimo de línea); más alto descarta más cosas chicas.\n\nEjemplo: 25 %, el de fábrica, son 0,1 mm con boquilla "
     "de 0,4.",
     "It is the thickness below which a part of the model is not printed at all, in mm or as a percentage of the "
     "nozzle. Anything thinner, like an almost flat relief or a paper-thin edge, is dropped. If that leaves the whole "
     "part empty, the program reports an error.\n\nLower tries to print finer details (they are thickened to the "
     "minimum line width); higher drops more small bits.\n\nExample: 25 %, the default, is 0.1 mm with a 0.4 mm "
     "nozzle."},
    {"walls.spread_count",
     "Con las paredes de ancho variable. Una zona de la pieza casi nunca mide justo un número entero de líneas. Este "
     "ajuste es cuántas líneas, contando desde el medio de la pared, se ensanchan o se afinan para absorber esa "
     "diferencia; las demás quedan con su ancho normal.\n\nCon 1 (de fábrica), solo cambia la del medio y las de "
     "afuera quedan parejas; con más, el cambio se reparte y cada línea varía menos.\n\nEjemplo: con 1, una pared de "
     "1,2 mm sale con líneas de 0,45, 0,39 y 0,45 mm.",
     "With variable-width walls. An area of the part is almost never exactly a whole number of lines wide. This "
     "setting is how many lines, counting from the middle of the wall, widen or narrow to absorb that difference; the "
     "rest keep their normal width.\n\nWith 1 (the default), only the middle one changes and the outer ones stay even; "
     "with more, the change is shared and each line varies less.\n\nExample: with 1, a 1.2 mm wall gets lines of 0.45, "
     "0.39 and 0.45 mm."},
    {"walls.transition_length",
     "Con las paredes de ancho variable. Cuando la pieza se ensancha y pasa a entrar una línea más, el cambio no es de "
     "golpe: hay un tramo de transición. Este ajuste es el largo de ese tramo a lo largo de la pared, en mm o en "
     "porcentaje de la boquilla; 0 usa la separación entre líneas.\n\nUna transición más larga es más suave; más corta "
     "sigue la forma más de cerca. Rara vez hace falta tocarlo.\n\nEjemplo: 100 %, el de fábrica, son 0,4 mm con "
     "boquilla de 0,4.",
     "With variable-width walls. When the part widens and one more line fits, the change is not abrupt: there is a "
     "transition stretch. This setting is that stretch's length along the wall, in mm or as a percentage of the "
     "nozzle; 0 uses the spacing between lines.\n\nA longer transition is smoother; a shorter one follows the shape "
     "more closely. You rarely need to touch it.\n\nExample: 100 %, the default, is 0.4 mm with a 0.4 mm nozzle."},
    {"walls.transition_angle",
     "Con las paredes de ancho variable. En una punta de la pieza, donde dos bordes se juntan en ángulo, el ancho "
     "crece desde el vértice y la cantidad de líneas sube de a poco. Este ángulo, en grados, decide cómo: en las "
     "puntas no más abiertas que esto se permite una línea sola en el medio y se pasa por 1, 2, 3, 4 líneas; en las "
     "más abiertas van de a pares, 2, 4, 6, y cada borde lleva las suyas.\n\nSubirlo pone una línea al medio en más "
     "puntas; bajarlo deja más puntas con líneas de a pares. Rara vez hace falta tocarlo.\n\nEjemplo: con 10°, el de "
     "fábrica, una cuña de 5° pasa por 1, 2, 3, 4 líneas y una de 20°, por 2, 4, 6.",
     "With variable-width walls. At a pointed part of the model, where two edges meet at an angle, the width grows "
     "from the tip and the number of lines rises gradually. This angle, in degrees, decides how: in tips no wider than "
     "this a single line is allowed in the middle, going through 1, 2, 3, 4 lines; in wider ones they come in pairs, "
     "2, 4, 6, each edge with its own.\n\nRaising it puts a middle line in more tips; lowering it leaves more tips "
     "with paired lines. You rarely need to touch it.\n\nExample: with 10°, the default, a 5° wedge goes through 1, 2, "
     "3, 4 lines and a 20° one through 2, 4, 6."},
    {"walls.transition_margin",
     "Con las paredes de ancho variable. A veces una pared apenas pasa el ancho en el que entra una línea más, y solo "
     "en un tramo. Con este ajuste por encima de 0, esos tramos que se pasan por muy poco (0,155 mm o menos) se quedan "
     "con las líneas que tenían, en vez de sumar una línea que entra y sale enseguida. Con 0, cada cambio suma su "
     "línea.\n\nEvitar esos saltos chicos deja paredes más parejas. Tal como se midió, el tamaño del margen no mueve "
     "ese corte: lo que importa es si es 0 o no.\n\nEjemplo: dejalo en 25 %, el de fábrica; ponelo en 0 solo si querés "
     "que hasta el cambio más chico sume una línea.",
     "With variable-width walls. Sometimes a wall only just passes the width where one more line fits, and only for a "
     "short stretch. With this setting above 0, those stretches that pass it by very little (0.155 mm or less) keep "
     "the lines they had, instead of adding a line that comes and goes right away. At 0, every change adds its "
     "line.\n\nAvoiding those small jumps gives more even walls. As measured, the margin's size does not move that "
     "cut-off: what matters is whether it is 0 or not.\n\nExample: leave it at 25 %, the default; set it to 0 only if "
     "you want even the smallest change to add a line."},
    {"walls.staggered_inner_seams",
     "Normalmente todas las paredes de una capa empiezan en el mismo lugar, y sus costuras quedan una al lado de la "
     "otra. Encendido, cada pared de adentro empieza un poco antes que la de afuera sobre su propia vuelta (un ancho "
     "de línea por cada nivel), y las costuras forman un zigzag hacia adentro en vez de una línea. La de afuera queda "
     "donde diga la costura.\n\nRepartir las costuras evita que todas se junten en un mismo punto, que queda más "
     "débil.\n\nEjemplo: encendido en una pieza que tiene que aguantar fuerza o ser estanca, como un vaso.",
     "Normally all the walls of a layer start at the same place, and their seams sit side by side. When on, each inner "
     "wall starts a little before the outer one along its own loop (one line width per level), and the seams form a "
     "zigzag inwards instead of a line. The outer one stays where the seam setting puts it.\n\nSpreading the seams "
     "keeps them from bunching up at one spot, which would be weaker.\n\nExample: on for a part that has to take a "
     "load or hold water, like a cup."},
    {"walls.staggered",
     "Perímetros escalonados: las paredes de adentro alternadas (la primera interna, la tercera...) se imprimen media "
     "capa más arriba que las demás. Así las líneas de una capa encajan entre las de la siguiente, como ladrillos, en "
     "vez de quedar apiladas justo una sobre otra. La pared de afuera no se mueve; en la primera capa y bajo una "
     "superficie de arriba se ajusta el plástico para que todo cierre.\n\nLa idea es que las capas queden más trabadas "
     "entre sí. No se puede usar con el modo vaso ni con capas curvas.\n\nEjemplo: encendido en una pieza mecánica, "
     "como un soporte, que va a recibir fuerza de costado.",
     "Staggered perimeters: alternate inner walls (the first inner one, the third...) are printed half a layer higher "
     "than the rest. The lines of one layer then nest between those of the next, like bricks, instead of sitting "
     "straight on top of each other. The outer wall does not move; on the first layer and under a top surface the "
     "amount of plastic is adjusted so everything closes up.\n\nThe idea is to lock the layers together better. It "
     "cannot be used with vase mode or curved layers.\n\nExample: on for a mechanical part, like a bracket, that will "
     "be loaded from the side."},
    {"walls.top_one_wall",
     "Alrededor de una superficie de arriba suelen verse varias vueltas de pared antes de que empiece el relleno liso. "
     "Con esto, donde la capa de arriba deja la zona al aire (más ancha que una línea), queda solo la pared de afuera: "
     "las de adentro rodean solo lo que sigue subiendo, y la superficie de arriba ocupa ese lugar.\n\nOpciones: "
     "apagado (de fábrica), en cada capa que tenga superficie de arriba, o solo en la última capa de la "
     "pieza.\n\nEjemplo: en cada capa para una pieza con escalones planos, como una escalera; solo en la última para "
     "una caja de tapa plana.",
     "Around a top surface you usually see several wall loops before the smooth fill starts. With this, where the "
     "layer above leaves the area open (wider than a line), only the outer wall remains: the inner ones go only around "
     "what keeps rising, and the top surface takes their place.\n\nOptions: off (the default), on every layer that has "
     "a top surface, or only on the part's last layer.\n\nExample: every layer for a part with flat steps, like a "
     "staircase; only the last one for a box with a flat lid."},
    {"walls.first_layer_one_wall",
     "Encendido, la primera capa lleva solo la pared de afuera, en vez de todas las paredes, y el resto de esa capa se "
     "llena con el relleno macizo del piso.\n\nLa base queda con menos vueltas de contorno y un dibujo más parejo de "
     "punta a punta; apagado (de fábrica), la primera capa lleva las mismas paredes que las demás.\n\nEjemplo: "
     "encendido para un cartel o una placa cuya cara de abajo se va a ver.",
     "When on, the first layer gets only the outer wall instead of all the walls, and the rest of that layer is filled "
     "with the solid floor fill.\n\nThe bottom ends up with fewer outline loops and a more even pattern from edge to "
     "edge; off (the default), the first layer has the same walls as the others.\n\nExample: on for a sign or a plate "
     "whose underside will be seen."},
    {"walls.extra_if_needed",
     "La idea es agregar paredes de más donde la pieza tiene paredes muy inclinadas, para que no queden huecos entre "
     "capas. El ajuste se lee y se guarda en el perfil, pero por ahora este motor no agrega ninguna pared extra: en "
     "las piezas de prueba no se agregó ninguna.\n\nHoy encenderlo o apagarlo no cambia la impresión; se conserva para "
     "que el perfil no pierda el dato.\n\nEjemplo: dejalo como viene, encendido.",
     "The idea is to add extra walls where the part has steeply sloping walls, so no gaps open between layers. The "
     "setting is read and saved in the profile, but for now this engine adds no extra wall: none were added on the "
     "test parts.\n\nToday turning it on or off does not change the print; it is kept so the profile does not lose the "
     "value.\n\nExample: leave it as it comes, on."},
    {"walls.extra_on_overhangs",
     "Un voladizo que se sostiene de un solo lado, como una repisa, no se puede imprimir como un puente, porque no "
     "tiene dónde apoyarse del otro. Encendido, esas zonas se llenan con vueltas que siguen su contorno de afuera "
     "hacia adentro, a la velocidad de los puentes, en lugar de su relleno común, y el voladizo cuelga menos.\n\nSuma "
     "algo de tiempo en esas zonas. Si los arcos en voladizos están encendidos, esas zonas las imprimen los "
     "arcos.\n\nEjemplo: encendido para una pieza con repisas o aleros sin soporte.",
     "An overhang held from only one side, like a shelf, cannot be printed as a bridge because it has nothing to rest "
     "on at the other end. When on, those areas are filled with loops that follow their outline from the outside in, "
     "at bridge speed, instead of their usual fill, and the overhang droops less.\n\nIt adds some time in those areas. "
     "If arc overhangs are on, the arcs print those areas instead.\n\nExample: on for a part with shelves or eaves and "
     "no support."},
    {"walls.avoid_curled_overhangs",
     "La idea es que los viajes de la boquilla (cuando se mueve sin imprimir) esquiven los voladizos que pueden "
     "haberse levantado al enfriarse, para no chocarlos. El ajuste se lee y se guarda en el perfil, pero hoy no cambia "
     "nada en la impresión: todavía no se midió cómo tiene que comportarse.\n\nEjemplo: dejalo apagado, como viene; si "
     "la boquilla golpea voladizos, probá con levantar la boquilla en los viajes.",
     "The idea is for the nozzle's travel moves (when it moves without printing) to steer around overhangs that may "
     "have curled up as they cooled, so it does not hit them. The setting is read and saved in the profile, but today "
     "it changes nothing in the print: how it should behave has not been measured yet.\n\nExample: leave it off, as it "
     "comes; if the nozzle hits overhangs, try lifting the nozzle on travel moves."},
    {"surfaces.bottom_pattern",
     "Es el dibujo de las superficies de abajo, las que quedan mirando hacia la cama, como la base de la pieza. "
     "Monótono (de fábrica): líneas sueltas, todas en el mismo sentido y en orden, sin volver sobre lo ya hecho. "
     "Zigzag: una línea que va y vuelve sin cortarse.\n\nEl monótono deja una superficie más pareja, sin cambios de "
     "brillo; el zigzag hace menos cortes y viajes, y suele ser algo más rápido.\n\nEjemplo: monótono si la base de la "
     "pieza se va a ver; zigzag para una pieza que va a quedar apoyada.",
     "It is the pattern of the bottom surfaces, the ones facing the bed, like the base of the part. Monotonic (the "
     "default): separate lines, all in the same direction and in order, never going back over finished work. Zigzag: "
     "one line going back and forth without breaking.\n\nMonotonic leaves a more even surface, with no changes in "
     "shine; zigzag makes fewer breaks and travel moves, and is usually a little faster.\n\nExample: monotonic if the "
     "bottom of the part will be seen; zigzag for a part that will sit on it."},
    {"surfaces.bridge_angle",
     "Un puente es una parte de la pieza que cruza por el aire entre dos apoyos, como el techo de una ventana; sus "
     "líneas se tienden de un apoyo al otro. Con 0 (de fábrica), el programa elige la dirección de cada puente según "
     "dónde se apoya. Con otro valor, las líneas de todos los puentes van en ese ángulo, en grados.\n\nForzarlo sirve "
     "si la elección automática no te convence; un ángulo malo deja líneas colgando sin apoyo.\n\nEjemplo: 0 casi "
     "siempre; si un puente sale con las líneas paralelas a sus apoyos, poné el ángulo que lo cruza, por ejemplo 90°.",
     "A bridge is a part of the model that spans the air between two supports, like the top of a window; its lines are "
     "laid from one support to the other. At 0 (the default), the program picks each bridge's direction from where it "
     "rests. With any other value, the lines of every bridge run at that angle, in degrees.\n\nForcing it helps if you "
     "do not like the automatic choice; a bad angle leaves lines hanging with nothing to hold them.\n\nExample: 0 "
     "almost always; if a bridge comes out with lines parallel to its supports, set the angle that crosses it, for "
     "example 90°."},
    {"surfaces.interface_shells",
     "Solo cuenta si imprimís con más de un extrusor, por ejemplo con dos materiales o colores. Donde la parte de uno "
     "se apoya sobre la del otro, encendido, cada una lleva sus propias capas macizas de techo y de piso, como si "
     "fueran piezas separadas. Apagado (de fábrica), el otro material tapa como si todo fuera la misma "
     "pieza.\n\nEncenderlo deja cada parte cerrada por su cuenta, a cambio de más plástico y tiempo.\n\nEjemplo: "
     "apagado para una pieza de dos colores; encendido si una parte es de material soluble y la otra tiene que quedar "
     "cerrada al disolverla.",
     "Only matters if you print with more than one extruder, for example two materials or colours. Where one's part "
     "rests on the other's, when on, each gets its own solid roof and floor layers, as if they were separate parts. "
     "Off (the default), the other material covers it as if it were all one part.\n\nTurning it on leaves each part "
     "closed on its own, at the cost of more plastic and time.\n\nExample: off for a two-colour part; on if one part "
     "is a soluble material and the other has to stay closed once it is dissolved."},
    {"slicing.fill_mode",
     "Al cortar el modelo, cada capa da contornos. Si el archivo tiene partes que se cruzan o se superponen, hay que "
     "decidir qué es adentro. Unión (de fábrica): todo lo que está dentro de alguna parte es pieza. Par-impar: en cada "
     "cruce de contorno se pasa de adentro a afuera y al revés, así lo superpuesto queda hueco. Rellenar agujeros: "
     "como unión, pero además tapa los agujeros.\n\nEjemplo: unión casi siempre; rellenar agujeros si un modelo roto "
     "sale con huecos que no van (ojo: también tapa los que sí van); par-impar para modelos hechos con superficies "
     "dobles.",
     "When the model is cut, each layer gives outlines. If the file has parts that cross or overlap, something has to "
     "decide what is inside. Union (the default): anything inside any part is solid. Even-odd: each time an outline is "
     "crossed it switches between inside and outside, so overlaps end up hollow. Fill holes: like union, but it also "
     "closes the holes.\n\nExample: union almost always; fill holes if a broken model comes out with holes that do not "
     "belong (careful: it also closes the ones that do); even-odd for models built from doubled surfaces."},
    {"slicing.gap_closing_radius",
     "Si el modelo tiene pequeñas fallas, al cortarlo una capa puede dar contornos que no cierran, con un huequito "
     "entre las puntas. El programa une esas puntas cuando el hueco mide hasta este valor, en mm, para que el contorno "
     "quede cerrado.\n\nMás alto repara modelos más rotos, pero puede unir puntas que no van juntas; más bajo es más "
     "fiel. Con modelos sanos no influye.\n\nEjemplo: 0,049 mm, el de fábrica; probá 0,2 mm si un modelo con fallas "
     "sale con capas incompletas.",
     "If the model has small flaws, cutting it can give a layer outlines that do not close, with a tiny gap between "
     "the ends. The program joins those ends when the gap is at most this value, in mm, so the outline is "
     "closed.\n\nHigher repairs more broken models, but may join ends that do not belong together; lower is more "
     "faithful. With sound models it makes no difference.\n\nExample: 0.049 mm, the default; try 0.2 mm if a flawed "
     "model comes out with incomplete layers."},
    {"slicing.simplify_tolerance",
     "Los modelos están hechos de muchas caras chiquitas, y cada capa sale con contornos de muchísimos puntos. Este "
     "ajuste quita los puntos que se apartan menos que esto, en mm, de la recta entre sus vecinos: la forma casi no "
     "cambia y los pasos siguientes van más rápido. 0 no simplifica.\n\nMás alto acelera más, pero puede achatar "
     "curvas finas; más bajo es más fiel.\n\nEjemplo: con 0,01 mm, el de fábrica, un círculo de 708 tramos queda en "
     "208, y el cálculo de las paredes de esa capa tarda 3,4 veces menos.",
     "Models are made of many tiny faces, and each layer comes out with outlines of a great many points. This setting "
     "removes the points that stray less than this, in mm, from the straight line between their neighbours: the shape "
     "hardly changes and the next steps run faster. 0 does not simplify.\n\nHigher speeds things up more, but can "
     "flatten fine curves; lower is more faithful.\n\nExample: at 0.01 mm, the default, a circle of 708 segments drops "
     "to 208, and working out that layer's walls takes 3.4 times less."},
