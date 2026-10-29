// Explanations of the infill settings for setting_guide.cpp: {key, Spanish, English},
// one entry per setting. Own texts; see PROVENANCE.md.
    {"infill.width",
     "Es el ancho de cada línea de plástico del relleno de adentro, el que tiene huecos. Con 0, el programa elige solo "
     "un ancho un poco mayor que la boquilla: 1,125 veces su tamaño.\n\nLíneas más anchas ponen más plástico en cada "
     "pasada, así hacen falta menos líneas y el relleno se imprime más rápido; demasiado anchas para la boquilla, "
     "salen desparejas. Más finas, el relleno tarda más. El porcentaje de relleno no cambia: las líneas se separan más "
     "o menos para mantenerlo.\n\nEjemplo: con boquilla de 0,4 mm, dejalo en 0 (unos 0,45 mm). Para ganar tiempo en "
     "una pieza grande, probá 0,5 a 0,6 mm.",
     "It is the width of each plastic line in the inner infill, the part with gaps. At 0, the program picks a width a "
     "little larger than the nozzle on its own: 1.125 times its size.\n\nWider lines lay more plastic per pass, so "
     "fewer lines are needed and the infill prints faster; too wide for the nozzle, and they come out uneven. Thinner "
     "lines make the infill take longer. The infill percentage stays the same: the lines are spaced further apart or "
     "closer together to keep it.\n\nExample: with a 0.4 mm nozzle, leave it at 0 (about 0.45 mm). To save time on a "
     "large part, try 0.5 to 0.6 mm."},
    {"zaa.enabled",
     "Con capas planas, un techo inclinado o redondeado sale como una escalera, porque cada capa está entera o no "
     "está. El Z anti-aliasing (suavizado en Z) hace que las líneas que se ven arriba bajen dentro de su capa hasta la "
     "superficie real del modelo, con menos plástico donde la capa queda más fina.\n\nLas cúpulas y los techos "
     "inclinados quedan mucho más lisos, sin escalones marcados. Se mueven las paredes, el relleno de arriba y el "
     "relleno de huecos; el relleno interno, los puentes y el soporte quedan igual. Casi no suma tiempo.\n\nEjemplo: "
     "prendelo para una cúpula, un casco o el techo de una casita; en una caja de caras planas no cambia nada.",
     "With flat layers, a sloped or rounded top comes out as a staircase, because each layer is either all there or "
     "not there. Z anti-aliasing (smoothing in Z) lets the lines you see on top dip within their layer down to the "
     "model's real surface, with less plastic where the layer ends up thinner.\n\nDomes and sloped tops come out much "
     "smoother, without clear steps. It moves the walls, the top fill and the gap fill; the inner infill, bridges and "
     "supports stay as they are. It adds almost no time.\n\nExample: turn it on for a dome, a helmet or the roof of a "
     "little house; on a box with flat faces it changes nothing."},
    {"zaa.min_thickness",
     "Solo cuenta con el Z anti-aliasing prendido. Cuando una línea baja hasta la superficie, en ese lugar la capa "
     "queda más fina. Este ajuste pone un límite: la línea nunca queda más fina que este porcentaje de la altura de "
     "capa.\n\nMás bajo, la superficie sigue mejor al modelo, pero una línea muy fina lleva tan poco plástico que "
     "puede salir débil o cortada. Más alto, las líneas salen más seguras y queda algo de escalón en las pendientes "
     "muy suaves.\n\nEjemplo: 25 %, el valor de fábrica; con capas de 0,2 mm la línea más fina mide 0,05 mm. Si ves "
     "líneas cortadas arriba, subilo a 40 %.",
     "It only matters with Z anti-aliasing on. When a line dips down to the surface, the layer gets thinner there. "
     "This setting sets a limit: the line never gets thinner than this percentage of the layer height.\n\nLower, the "
     "surface follows the model more closely, but a very thin line carries so little plastic that it can come out weak "
     "or broken. Higher, the lines come out more reliably and a bit of a step remains on very gentle "
     "slopes.\n\nExample: 25%, the default; with 0.2 mm layers the thinnest line is 0.05 mm. If you see broken lines "
     "on top, raise it to 40%."},
    {"zaa.raise",
     "Solo cuenta con el Z anti-aliasing prendido. Normalmente las líneas solo bajan hasta la superficie. Con esto "
     "también pueden subir, hasta media capa, cuando la superficie del modelo queda apenas por encima de la "
     "capa.\n\nAsí la superficie sigue al modelo por los dos lados de cada capa y las pendientes suaves quedan todavía "
     "más parejas. A cambio, hay algunos arranques y cortes de plástico más, y las líneas que suben son más "
     "gruesas.\n\nEjemplo: apagado para empezar; prendelo para la mejor terminación en una cúpula o en un techo poco "
     "inclinado. El perfil Máxima calidad lo trae prendido.",
     "It only matters with Z anti-aliasing on. Normally lines only dip down to the surface. With this, they can also "
     "rise, by up to half a layer, when the model's surface sits just above the layer.\n\nThat way the surface follows "
     "the model on both sides of each layer and gentle slopes come out even smoother. In return, there are a few more "
     "starts and stops of plastic, and the lines that rise are thicker.\n\nExample: off to begin with; turn it on for "
     "the best finish on a dome or a gently sloped roof. The Maximum quality profile has it on."},
    {"ironing.enabled",
     "El planchado es una pasada extra de la boquilla caliente sobre la superficie de arriba recién impresa, con muy "
     "poco plástico, como una plancha sobre la ropa: derrite las crestas entre líneas y tapa los agujeritos.\n\nLas "
     "caras de arriba quedan lisas, casi sin líneas a la vista. A cambio, la impresión tarda bastante más, porque la "
     "pasada es lenta y con líneas muy juntas. Solo sirve en caras planas y horizontales.\n\nEjemplo: prendelo para la "
     "tapa de una caja, un cartel o una placa con texto; dejalo apagado en figuras sin caras planas arriba.",
     "Ironing is an extra pass of the hot nozzle over the top surface it has just printed, with very little plastic, "
     "like an iron over clothes: it melts down the ridges between lines and closes small holes.\n\nTop faces come out "
     "smooth, with almost no visible lines. In return, the print takes quite a bit longer, because the pass is slow "
     "and its lines are very close together. It only helps on flat, horizontal faces.\n\nExample: turn it on for a box "
     "lid, a sign or a plate with text; leave it off for figures with no flat faces on top."},
    {"ironing.surfaces",
     "Solo cuenta con el planchado prendido. Elige qué caras se planchan: «Todas las de arriba», lo que la capa "
     "siguiente no tapa; «Solo la más alta», lo que nada de más arriba tapa, la cara más alta de cada parte; «Todas "
     "las capas sólidas», cada capa maciza, también las que después quedan tapadas.\n\nCuantas más caras se planchan, "
     "más tarda la impresión. Planchar caras que después quedan adentro no se ve.\n\nEjemplo: «Todas las de arriba» "
     "para casi todo; «Solo la más alta» para no gastar tiempo en caras que quedan debajo de otra parte de la pieza.",
     "It only matters with ironing on. It chooses which faces get ironed: Every top surface, whatever the next layer "
     "does not cover; Only the topmost, whatever nothing higher up covers, the highest face of each part; Every solid "
     "layer, each solid layer, even those covered later.\n\nThe more faces are ironed, the longer the print takes. "
     "Ironing faces that end up inside does not show.\n\nExample: Every top surface for almost everything; Only the "
     "topmost to avoid spending time on faces that sit under another part of the piece."},
    {"ironing.flow",
     "Solo cuenta con el planchado prendido. Es cuánto plástico sale durante el planchado, en porcentaje de lo que "
     "pondría una capa normal: muy poco, apenas para llenar los surcos entre líneas.\n\nSi es poco, quedan surcos o "
     "agujeritos sin tapar. Si es mucho, el plástico de más se acumula delante de la boquilla y la superficie queda "
     "con rebabas o crestas.\n\nEjemplo: 15 %, el valor de fábrica. Bajalo a 10 % si ves rebabas; subilo a 20 o 25 % "
     "si siguen quedando surcos.",
     "It only matters with ironing on. It is how much plastic comes out while ironing, as a percentage of what a "
     "normal layer would lay down: very little, just enough to fill the grooves between lines.\n\nToo little, and "
     "grooves or small holes stay open. Too much, and the extra plastic piles up in front of the nozzle and the "
     "surface gets bumps or ridges.\n\nExample: 15%, the default. Lower it to 10% if you see bumps; raise it to 20 or "
     "25% if grooves remain."},
    {"ironing.spacing",
     "Solo cuenta con el planchado prendido. Es la distancia entre una pasada de planchado y la siguiente, en "
     "milímetros.\n\nPasadas más juntas alisan mejor, pero hay muchas más y el planchado tarda más. Más separadas, "
     "termina antes y pueden quedar marcas de cada pasada.\n\nEjemplo: 0,1 mm, el valor de fábrica, para una tapa bien "
     "lisa; 0,15 a 0,2 mm para ahorrar tiempo en una cara grande.",
     "It only matters with ironing on. It is the distance between one ironing pass and the next, in "
     "millimetres.\n\nCloser passes smooth better, but there are many more of them and ironing takes longer. Further "
     "apart, it finishes sooner and each pass may leave a mark.\n\nExample: 0.1 mm, the default, for a really smooth "
     "lid; 0.15 to 0.2 mm to save time on a large face."},
    {"ironing.speed",
     "Solo cuenta con el planchado prendido. Es qué tan rápido se mueve la boquilla mientras plancha, en milímetros "
     "por segundo.\n\nMás lento le da tiempo al calor para derretir y alisar la superficie, y queda más pareja, pero "
     "tarda más. Más rápido ahorra tiempo y alisa menos.\n\nEjemplo: 15 mm/s, el valor de fábrica. Probá 10 mm/s si "
     "todavía se ven las líneas; hasta 25 mm/s si la superficie ya queda bien y querés terminar antes.",
     "It only matters with ironing on. It is how fast the nozzle moves while ironing, in millimetres per "
     "second.\n\nSlower gives the heat time to melt and smooth the surface, which comes out more even, but it takes "
     "longer. Faster saves time and smooths less.\n\nExample: 15 mm/s, the default. Try 10 mm/s if the lines still "
     "show; up to 25 mm/s if the surface already looks good and you want to finish sooner."},
    {"infill.angle",
     "Es la dirección de las líneas del relleno, en grados, medida desde el eje X de la cama (de izquierda a derecha). "
     "En el dibujo de líneas, cada capa por medio suma 90°.\n\nNo cambia cuánto plástico lleva la pieza; cambia hacia "
     "dónde corren las líneas, y sirve para acomodarlas a la forma de la pieza, por ejemplo a sus lados. Algunos "
     "dibujos, como el giroide, no lo usan.\n\nEjemplo: 45°, el valor de fábrica, sirve para casi todo; 0° o 90° si "
     "querés las líneas paralelas a los lados de una pieza rectangular.",
     "It is the direction of the infill lines, in degrees, measured from the bed's X axis (left to right). In the "
     "lines pattern, every other layer adds 90°.\n\nIt does not change how much plastic the part gets; it changes "
     "which way the lines run, which lets you match them to the part's shape, for example to its sides. Some patterns, "
     "like gyroid, do not use it.\n\nExample: 45°, the default, works for almost everything; 0° or 90° if you want the "
     "lines parallel to the sides of a rectangular part."},
    {"infill.wall_overlap",
     "Las capas macizas de arriba se llenan con líneas rectas. Cuando esas líneas no se unen entre sí en zigzag, este "
     "ajuste dice cuánto entran en la pared de adentro, en porcentaje del ancho de una línea, para que queden bien "
     "pegadas a ella.\n\nMás solapamiento une mejor el techo con las paredes, pero si es demasiado sobra plástico en "
     "los bordes y se forma un reborde. Poco deja una rayita hueca a lo largo de la pared.\n\nEjemplo: 25 %, el valor "
     "de fábrica, deja el plástico justo. Bajalo a 15 % si ves un reborde junto a las paredes; subilo a 35 % si queda "
     "una rendija.",
     "The solid top layers are filled with straight lines. When those lines are not joined to each other in a zigzag, "
     "this setting says how far they reach into the inner wall, as a percentage of a line's width, so they bond well "
     "to it.\n\nMore overlap joins the top to the walls better, but too much leaves extra plastic at the edges and "
     "makes a raised rim. Too little leaves a thin empty groove along the wall.\n\nExample: 25%, the default, gives "
     "just the right amount of plastic. Lower it to 15% if you see a rim along the walls; raise it to 35% if a gap "
     "remains."},
    {"infill.solid_every",
     "Cada tantas capas, el relleno con huecos se reemplaza por una capa maciza, que se imprime como un puente sobre "
     "el relleno de abajo. Es como poner pisos firmes adentro de la pieza. Con 0 no se hace nunca.\n\nEsos pisos "
     "internos hacen más rígida una pieza alta y la ayudan a no torcerse ni partirse, a cambio de más tiempo y "
     "plástico. Cuanto más chico el número, más pisos.\n\nEjemplo: 0, el valor de fábrica, para casi todo; 20 con "
     "capas de 0,2 mm pone un piso macizo cada 4 mm en una pieza alta y fina.",
     "Every so many layers, the infill with gaps is replaced by a solid layer, printed as a bridge over the infill "
     "below. It is like adding firm floors inside the part. At 0 it never happens.\n\nThose inner floors make a tall "
     "part stiffer and help it resist bending or snapping, for more time and plastic. The smaller the number, the more "
     "floors.\n\nExample: 0, the default, for almost everything; 20 with 0.2 mm layers puts a solid floor every 4 mm "
     "in a tall, thin part."},
    {"infill.solid_below_area",
     "Cuando en una capa queda una zona de relleno muy chica, como el interior de una pata fina o de un perno, se "
     "llena maciza en vez de con huecos. Este ajuste es el tamaño, en mm², por debajo del cual pasa eso. Se mide lo "
     "que queda adentro de las paredes, no la cara entera.\n\nAsí las partes finas salen fuertes y no quedan con un "
     "relleno de dos o tres líneas sueltas. Más alto, más partes macizas y algo más de plástico. Con 0 no se aplica "
     "nunca.\n\nEjemplo: 70 mm², el valor de fábrica: con 3 paredes de 0,4 mm, un perno de 10 mm de diámetro sale "
     "macizo.",
     "When a layer has a very small infill area, like the inside of a thin leg or a pin, it is filled solid instead of "
     "with gaps. This setting is the size, in mm², below which that happens. It counts what is left inside the walls, "
     "not the whole face.\n\nThat way thin parts come out strong instead of with an infill of two or three loose "
     "lines. Higher, more parts become solid and use a little more plastic. At 0 it never applies.\n\nExample: 70 mm², "
     "the default: with 3 walls of 0.4 mm, a pin 10 mm across comes out solid."},
    {"infill.dense_under_top",
     "El techo de la pieza se apoya sobre el relleno. Con un relleno muy liviano, las líneas del techo pueden hundirse "
     "en los huecos. Con esto, la capa de relleno que queda justo debajo de cada techo se hace más densa, solo debajo "
     "de ese techo.\n\nAsí podés usar un relleno general bajo, que ahorra tiempo y plástico, sin que el techo salga "
     "hundido o con agujeros. Cuesta muy poco: es una sola capa. La densidad de esa capa se elige en el ajuste "
     "siguiente.\n\nEjemplo: relleno al 10 % con esta opción prendida y la capa de abajo del techo al 40 %.",
     "The top of the part rests on the infill. With a very light infill, the top's lines can sag into the gaps. With "
     "this, the infill layer right under each top surface is made denser, only under that top.\n\nThat lets you use a "
     "low overall infill, saving time and plastic, without the top coming out sunken or full of holes. It costs very "
     "little: it is a single layer. That layer's density is chosen in the next setting.\n\nExample: 10% infill with "
     "this option on and the layer under the top at 40%."},
    {"infill.dense_density",
     "Solo cuenta con «Denso bajo el techo» prendido. Es la densidad, en porcentaje, de la capa de relleno que va "
     "justo debajo de cada techo. Usa el mismo dibujo y la misma dirección que el resto del relleno.\n\nMás alta, el "
     "techo se apoya mejor y sale más liso; más baja, se parece al relleno común y ayuda menos.\n\nEjemplo: 40 %, el "
     "valor de fábrica, alcanza para un relleno general del 10 al 15 %. Si el techo igual se hunde, subila a 50 o 60 "
     "%.",
     "It only matters with Dense under tops on. It is the density, in percent, of the infill layer right under each "
     "top surface. It uses the same pattern and direction as the rest of the infill.\n\nHigher, the top rests better "
     "and comes out smoother; lower, it is closer to the normal infill and helps less.\n\nExample: 40%, the default, "
     "is enough for an overall infill of 10 to 15%. If the top still sags, raise it to 50 or 60%."},
    {"infill.stress_density",
     "Relleno guiado por tensiones. Necesita un caso de carga: un archivo que dice dónde se apoya la pieza y qué "
     "fuerzas recibe. El programa calcula qué zonas trabajan más y ahí sube el relleno, por bandas, hasta esta "
     "densidad. El resto queda con la densidad general. Con 0 está apagado.\n\nPone el plástico donde hace falta: "
     "podés bajar el relleno general y reforzar solo el camino de la fuerza, con más rigidez por gramo. Sin caso de "
     "carga no hace nada.\n\nEjemplo: relleno general al 10 % y esto en 50 %, para un soporte de estante que carga "
     "peso en una punta.",
     "Stress-guided infill. It needs a load case: a file that says where the part is held and which forces it takes. "
     "The program works out which areas work hardest and raises the infill there, in bands, up to this density. The "
     "rest keeps the general density. At 0 it is off.\n\nIt puts the plastic where it is needed: you can lower the "
     "overall infill and reinforce only the path of the force, for more stiffness per gram. Without a load case it "
     "does nothing.\n\nExample: 10% overall infill and this at 50%, for a shelf bracket that carries weight at one "
     "end."},
    {"infill.before_walls",
     "Cambia el orden dentro de cada parte de la capa: primero el relleno y después las paredes. Normalmente van "
     "primero las paredes.\n\nCon el relleno primero, las paredes se apoyan contra algo ya impreso, lo que a veces "
     "ayuda en paredes delicadas. A cambio, el dibujo del relleno puede marcarse un poco en la cara de afuera de la "
     "pieza.\n\nEjemplo: apagado para casi todo, sobre todo en piezas que se miran; probá prenderlo si las paredes "
     "internas salen flojas o despegadas del relleno.",
     "It changes the order within each part of the layer: infill first, then the walls. Normally the walls go "
     "first.\n\nWith infill first, the walls are printed against something already there, which sometimes helps "
     "delicate walls. In return, the infill pattern can show a little on the outside face of the part.\n\nExample: off "
     "for almost everything, especially parts meant to be seen; try turning it on if the inner walls come out loose or "
     "detached from the infill."},
    {"infill.max_link",
     "Para no cortar y saltar entre línea y línea, el relleno une el final de una con el comienzo de la siguiente con "
     "un tramo que corre pegado a la pared. Este ajuste es el largo máximo de ese tramo, en mm. Con 0 se usa un tope "
     "chico, de tres separaciones entre líneas.\n\nUniones más largas significan menos cortes, menos retracciones y "
     "menos hilos, pero más plástico apoyado a lo largo de las paredes. Más cortas, más saltos con la boquilla "
     "vacía.\n\nEjemplo: 50 mm, el valor de fábrica; bajalo a 5 mm si esas uniones dejan marcas o plástico de más "
     "junto a las paredes.",
     "To avoid stopping and jumping between lines, the infill joins the end of one line to the start of the next with "
     "a stretch that runs along the wall. This setting is the longest that stretch can be, in mm. At 0 a short limit "
     "is used, three line spacings.\n\nLonger links mean fewer stops, fewer retractions and fewer strings, but more "
     "plastic laid along the walls. Shorter, more jumps with the nozzle empty.\n\nExample: 50 mm, the default; lower "
     "it to 5 mm if those links leave marks or extra plastic along the walls."},
    {"infill.combine_layers",
     "Imprime el relleno de adentro en una de cada N capas, con el alto de las N juntas, en vez de una línea fina en "
     "cada capa. Las paredes y las caras que se ven siguen con capas finas. Con 1, cada capa lleva su relleno.\n\nCon "
     "capas finas para que las paredes salgan prolijas, el relleno no necesita ese detalle: combinándolo se imprime "
     "con la mitad o un tercio de pasadas y la pieza termina antes. La capa combinada nunca pasa del alto máximo ni de "
     "la boquilla.\n\nEjemplo: con capas de 0,1 mm y boquilla de 0,4 mm, 2 hace un relleno de 0,2 mm cada dos capas.",
     "It prints the inner infill on one out of every N layers, as tall as the N together, instead of a thin line on "
     "every layer. Walls and visible faces keep their thin layers. At 1, each layer has its own infill.\n\nWith thin "
     "layers for neat walls, the infill does not need that detail: combined, it prints in half or a third of the "
     "passes and the part finishes sooner. The combined layer never goes past the maximum height or the "
     "nozzle.\n\nExample: with 0.1 mm layers and a 0.4 mm nozzle, 2 makes a 0.2 mm infill every two layers."},
    {"infill.combine_auto",
     "En vez de elegir vos cuántas capas combinar, el programa junta todas las que entran en el alto máximo de la capa "
     "combinada.\n\nSaca el mayor ahorro de tiempo sin pasarse del alto que la boquilla puede imprimir bien. Sirve "
     "sobre todo con capas finas, donde entran varias.\n\nEjemplo: con capas de 0,12 mm, boquilla de 0,4 mm y el alto "
     "máximo en 100 %, combina 3 capas: un relleno de 0,36 mm.",
     "Instead of you choosing how many layers to combine, the program joins as many as fit in the maximum height of "
     "the combined layer.\n\nIt gets the most time saved without going past the height the nozzle can print well. It "
     "helps most with thin layers, where several fit.\n\nExample: with 0.12 mm layers, a 0.4 mm nozzle and the maximum "
     "height at 100%, it combines 3 layers: a 0.36 mm infill."},
    {"infill.combine_max_height",
     "Es el alto máximo que puede tener una capa de relleno combinada. Se escribe en milímetros o en porcentaje de la "
     "boquilla, y nunca pasa del tamaño de la boquilla.\n\nMás alto, se combinan más capas y se ahorra más tiempo, "
     "pero las líneas tan gruesas se pegan peor entre sí y la boquilla tiene que derretir mucho plástico de golpe. Más "
     "bajo, el relleno sale más firme.\n\nEjemplo: 100 %, el valor de fábrica (0,4 mm con boquilla de 0,4); 75 % (0,3 "
     "mm) si el relleno combinado sale flojo o la impresora no llega a derretir tanto.",
     "It is the tallest a combined infill layer can be. It is written in millimetres or as a percentage of the nozzle, "
     "and it never goes past the nozzle size.\n\nHigher, more layers are combined and more time is saved, but such "
     "thick lines bond less well and the nozzle has to melt a lot of plastic at once. Lower, the infill comes out "
     "firmer.\n\nExample: 100%, the default (0.4 mm with a 0.4 nozzle); 75% (0.3 mm) if the combined infill comes out "
     "weak or the printer cannot melt that much."},
    {"arcs.enabled",
     "Un voladizo es una parte de la pieza que sale hacia el costado sin nada debajo, apoyada solo de un lado (si se "
     "apoya de los dos lados es un puente). Con esto, se imprime sin soporte: con arcos que crecen alrededor de un "
     "centro dentro de la parte apoyada, cada uno pisando un poco al anterior, como los anillos de un árbol.\n\nAhorra "
     "soportes y las marcas que dejan, pero los arcos van muy lentos y la cara de abajo queda con aros visibles. La "
     "capa siguiente los tapa con una capa maciza.\n\nEjemplo: prendelo para un estante que sale de una pared o el ala "
     "de una figura que no querés llenar de soportes.",
     "An overhang is a part of the piece that sticks out sideways with nothing below, held from one side only (held "
     "from both sides, it is a bridge). With this, it prints without support: with arcs that grow around a centre "
     "inside the supported part, each one slightly over the last, like tree rings.\n\nIt saves supports and the marks "
     "they leave, but the arcs go very slowly and the underside shows visible rings. The next layer covers them with a "
     "solid layer.\n\nExample: turn it on for a shelf that sticks out of a wall, or a figure's wing you do not want "
     "full of supports."},
    {"arcs.overlap",
     "Solo cuenta con los arcos en voladizos prendidos. Es cuánto pisa cada arco al anterior, en porcentaje del ancho "
     "de la línea. Con 10 % y paredes de 0,4 mm, los arcos quedan a 0,36 mm uno de otro.\n\nMás solapamiento une mejor "
     "los arcos y el voladizo sale más firme, pero hay más arcos y tarda más. Poco solapamiento termina antes, pero "
     "los arcos pueden separarse y quedar huecos.\n\nEjemplo: 10 %, el valor de fábrica; 20 a 30 % si ves que los "
     "arcos se despegan entre sí.",
     "It only matters with arc overhangs on. It is how much each arc overlaps the previous one, as a percentage of the "
     "line width. At 10% with 0.4 mm walls, the arcs sit 0.36 mm apart.\n\nMore overlap joins the arcs better and the "
     "overhang comes out firmer, but there are more arcs and it takes longer. Little overlap finishes sooner, but the "
     "arcs can come apart and leave gaps.\n\nExample: 10%, the default; 20 to 30% if you see the arcs separating from "
     "each other."},
    {"arcs.speed",
     "Solo cuenta con los arcos en voladizos prendidos. Es la velocidad de los arcos, en mm/s. Con 0 usa la velocidad "
     "de los puentes.\n\nVa lenta a propósito: cada arco cuelga en el aire y tiene que enfriarse antes de que llegue "
     "el siguiente para sostenerlo. Más rápido ahorra tiempo pero los arcos pueden caerse; más lento salen más "
     "prolijos.\n\nEjemplo: 5 mm/s, el valor de fábrica; bajala a 3 mm/s si los arcos se caen o se curvan hacia abajo.",
     "It only matters with arc overhangs on. It is the speed of the arcs, in mm/s. At 0 it uses the bridge "
     "speed.\n\nIt is slow on purpose: each arc hangs in the air and has to cool before the next one arrives to rest "
     "on it. Faster saves time but the arcs can droop; slower makes them neater.\n\nExample: 5 mm/s, the default; "
     "lower it to 3 mm/s if the arcs droop or curl downward."},
    {"arcs.max_distance",
     "Solo cuenta con los arcos en voladizos prendidos. Es hasta qué distancia del centro llegan los arcos, en mm. Más "
     "allá de eso, el voladizo se imprime de la forma de siempre. Con 0 no hay límite.\n\nLos arcos muy lejos del "
     "centro son largos y lentos. Limitarlos ahorra tiempo cuando la parte de afuera ya tiene soporte o no importa "
     "cómo quede.\n\nEjemplo: 0, el valor de fábrica, para que cubran todo el voladizo; 20 mm si solo querés arcos "
     "cerca de donde empieza.",
     "It only matters with arc overhangs on. It is how far from the centre the arcs reach, in mm. Beyond that, the "
     "overhang prints the usual way. At 0 there is no limit.\n\nArcs far from the centre are long and slow. Limiting "
     "them saves time when the outer part already has support or it does not matter how it looks.\n\nExample: 0, the "
     "default, so they cover the whole overhang; 20 mm if you only want arcs near where it starts."},
    {"curved.mode",
     "Capas curvas: en vez de capas planas, la pieza se arma con capas que suben y bajan, en una impresora común. "
     "Modos: «Onda», una ondulación pareja; «Cono», capas en forma de cono; «Voladizos», capas que bajan hacia afuera "
     "para sacar voladizos sin soporte; «Caras superiores», capas de arriba que siguen las caras inclinadas para que "
     "queden lisas; «Automático», voladizos y caras juntos. «Tensiones» todavía no está.\n\nEl programa achica la "
     "curva para que el cabezal no choque, y si no queda nada útil, lamina plano. No va con el modo vaso ni el Z "
     "anti-aliasing.\n\nEjemplo: «Voladizos» para una pieza con un saliente; «Caras superiores» para un techo "
     "inclinado.",
     "Curved layers: instead of flat layers, the part is built from layers that rise and fall, on an ordinary printer. "
     "Modes: Wave, an even ripple; Cone, cone-shaped layers; Overhangs, layers that dip outward to print overhangs "
     "without support; Top faces, top layers that follow sloped faces so they come out smooth; Automatic, overhangs "
     "and faces together. Stress is not available yet.\n\nThe program flattens the curve so the head does not hit the "
     "part, and if nothing useful is left, it slices flat. It does not work with vase mode or Z "
     "anti-aliasing.\n\nExample: Overhangs for a part with a ledge; Top faces for a sloped roof."},
    {"curved.pattern",
     "Solo cuenta en el modo «Onda» de las capas curvas. Es la forma de la onda: «Huevera», bultos y pozos como un "
     "cartón de huevos, que suben y bajan en las dos direcciones; «Crestas», ondas en una sola dirección, como un "
     "techo de chapa; «Crestas torcidas», crestas cuya dirección va girando a medida que la pieza sube.\n\nCon la "
     "huevera las capas se encajan en todas las direcciones; con las crestas, solo cruzadas a ellas.\n\nEjemplo: "
     "«Huevera» para empezar; «Crestas» si la fuerza va a venir de un solo lado.",
     "It only matters in the Wave mode of curved layers. It is the shape of the wave: Egg crate, bumps and dips like "
     "an egg carton, rising and falling in both directions; Ridges, waves in a single direction, like a corrugated "
     "roof; Twisted ridges, ridges whose direction keeps turning as the part rises.\n\nWith the egg crate the layers "
     "nest in every direction; with ridges, only across them.\n\nExample: Egg crate to start with; Ridges if the force "
     "will come from one side only."},
    {"curved.fit_head",
     "Con esto, las capas curvas quedan tan curvas como deja el cabezal de tu impresora, según sus medidas (el ángulo "
     "libre y la altura que asoma la boquilla). El programa calcula solo la altura y el largo de la onda y los achica "
     "hasta que el cabezal no choque y el grosor de las capas no varíe de más.\n\nSin esto, la amplitud y el largo de "
     "onda los ponés vos, y si una capa queda más empinada de lo que el cabezal deja, el programa da error en vez de "
     "corregirla.\n\nEjemplo: prendido, el valor de fábrica; apagalo solo para probar una onda exacta, después de "
     "medir tu cabezal.",
     "With this, curved layers are as curved as your printer's head allows, going by its measurements (the free angle "
     "and how far the nozzle sticks out). The program works out the wave's height and length on its own and shrinks "
     "them until the head does not hit the part and the layer thickness does not vary too much.\n\nWithout it, you set "
     "the amplitude and wavelength yourself, and if a layer comes out steeper than the head allows, the program "
     "reports an error instead of fixing it.\n\nExample: on, the default; turn it off only to try an exact wave, after "
     "measuring your head."},
    {"curved.amplitude",
     "Solo cuenta con las capas curvas en «Onda» y sin «Ajustar al cabezal». Es cuánto sube y baja cada capa desde su "
     "altura del medio, en mm.\n\nMás amplitud da capas más onduladas y, con el mismo largo de onda, más empinadas: "
     "más encaje entre capas, pero más riesgo de que el cabezal choque, y si se pasa el programa da error. Menos "
     "amplitud, ondas más suaves.\n\nEjemplo: 0,6 mm, el valor de fábrica; con 16 mm de largo de onda la pendiente más "
     "fuerte ronda los 13°.",
     "It only matters with curved layers in Wave and without Fit to the print head. It is how far each layer rises and "
     "falls from its middle height, in mm.\n\nMore amplitude gives wavier layers and, with the same wavelength, "
     "steeper ones: more nesting between layers, but more risk of the head hitting the part, and past the limit the "
     "program reports an error. Less amplitude, gentler waves.\n\nExample: 0.6 mm, the default; with a 16 mm "
     "wavelength the steepest slope is about 13°."},
    {"curved.wavelength",
     "Solo cuenta con las capas curvas en «Onda» y sin «Ajustar al cabezal». Es la distancia de una cresta de la onda "
     "a la siguiente, en mm.\n\nMás corta, hay más ondas y con la misma amplitud son más empinadas, con más riesgo de "
     "choque con el cabezal. Más larga, ondas más suaves y fáciles de imprimir.\n\nEjemplo: 16 mm, el valor de "
     "fábrica, con 0,6 mm de amplitud; si subís la amplitud a 1,2 mm, alargala a 32 mm para mantener la misma "
     "pendiente.",
     "It only matters with curved layers in Wave and without Fit to the print head. It is the distance from one crest "
     "of the wave to the next, in mm.\n\nShorter, there are more waves and, with the same amplitude, they are steeper, "
     "with more risk of the head hitting the part. Longer, gentler waves that are easier to print.\n\nExample: 16 mm, "
     "the default, with a 0.6 mm amplitude; if you raise the amplitude to 1.2 mm, stretch it to 32 mm to keep the same "
     "slope."},
    {"curved.angle",
     "Solo cuenta con las capas curvas en «Onda» con crestas. Es hacia dónde miran las crestas, en grados. Con 0, la "
     "altura de la capa cambia a lo largo de X (de izquierda a derecha), y las crestas corren de adelante hacia "
     "atrás.\n\nLas capas encajan mejor cruzadas a las crestas, así que conviene que la fuerza que va a recibir la "
     "pieza las cruce.\n\nEjemplo: 0° para crestas de adelante hacia atrás; 90° para que corran de izquierda a "
     "derecha.",
     "It only matters with curved layers in Wave with ridges. It is which way the ridges face, in degrees. At 0, the "
     "layer's height changes along X (left to right), and the ridges run front to back.\n\nThe layers nest best across "
     "the ridges, so the force the part will take should cross them.\n\nExample: 0° for ridges running front to back; "
     "90° for ridges running left to right."},
    {"curved.twist",
     "Solo cuenta con la onda «Crestas torcidas». Es cuánto gira la dirección de las crestas por cada milímetro que "
     "sube la pieza, en grados.\n\nAl girar, las crestas de abajo y las de arriba quedan cruzadas, así el encaje entre "
     "capas no apunta siempre al mismo lado. Más giro cambia la dirección más rápido; menos, más despacio.\n\nEjemplo: "
     "3° por mm, el valor de fábrica: a los 30 mm de altura las crestas ya giraron 90°.",
     "It only matters with the Twisted ridges wave. It is how much the ridges' direction turns for each millimetre the "
     "part rises, in degrees.\n\nAs they turn, the ridges low down and high up end up crossed, so the nesting between "
     "layers does not always point the same way. More twist changes direction faster; less, more slowly.\n\nExample: "
     "3° per mm, the default: at 30 mm of height the ridges have turned 90°."},
    {"curved.cone_angle",
     "Solo cuenta con las capas curvas en «Cono» y sin «Ajustar al cabezal». Es la pendiente del cono, en grados. "
     "Positivo, las capas suben hacia afuera como un bol; negativo, bajan hacia afuera como un paraguas. El centro se "
     "redondea.\n\nLas capas que bajan hacia afuera ayudan a los voladizos que salen hacia afuera. Más pendiente, más "
     "efecto y más riesgo de que el cabezal choque.\n\nEjemplo: 15°, el valor de fábrica; un valor negativo, como "
     "−15°, para una pieza que se abre hacia arriba, como un hongo.",
     "It only matters with curved layers in Cone and without Fit to the print head. It is the slope of the cone, in "
     "degrees. Positive, the layers rise outward like a bowl; negative, they drop outward like an umbrella. The centre "
     "is rounded.\n\nLayers that drop outward help overhangs that stick outward. A steeper slope has more effect and "
     "more risk of the head hitting the part.\n\nExample: 15°, the default; a negative value such as −15° for a part "
     "that widens upward, like a mushroom."},
    {"curved.flat_below",
     "Solo cuenta con las capas curvas prendidas. Por debajo de esta altura de la pieza, en mm, las capas son "
     "planas.\n\nLa base tiene que ser plana para pegarse bien a la cama, que es plana. Más alto, una base plana más "
     "gruesa y menos parte de la pieza con capas curvas.\n\nEjemplo: 0,6 mm, el valor de fábrica (unas tres capas de "
     "0,2 mm); 2 mm si querés una base más firme antes de que empiecen las curvas.",
     "It only matters with curved layers on. Below this height of the part, in mm, the layers are flat.\n\nThe base "
     "has to be flat to stick well to the bed, which is flat. Higher, a thicker flat base and less of the part with "
     "curved layers.\n\nExample: 0.6 mm, the default (about three 0.2 mm layers); 2 mm if you want a firmer base "
     "before the curves start."},
    {"curved.ramp_height",
     "Solo cuenta con las capas curvas prendidas. Las capas no pasan de planas a curvas de golpe: la curva crece de a "
     "poco a lo largo de esta altura, en mm, desde donde terminan las capas planas.\n\nUna transición más alta es más "
     "suave, pero la curva completa empieza más arriba. Más corta, llega antes a la curva completa, con un cambio más "
     "brusco en el grosor de las capas.\n\nEjemplo: 5 mm, el valor de fábrica; 2 a 3 mm en una pieza baja, para que "
     "alcance a curvarse.",
     "It only matters with curved layers on. Layers do not jump from flat to curved: the curve grows gradually over "
     "this height, in mm, starting where the flat layers end.\n\nA taller transition is gentler, but the full curve "
     "starts higher up. Shorter, it reaches the full curve sooner, with a sharper change in layer "
     "thickness.\n\nExample: 5 mm, the default; 2 to 3 mm on a short part, so it has room to curve."},
    {"curved.flat_top",
     "Solo cuenta con las capas curvas prendidas. Con esto, las capas vuelven a ser planas en la parte de arriba de la "
     "pieza, a lo largo del alto de la transición.\n\nSirve cuando la cara de arriba tiene que quedar plana, porque "
     "con capas curvas hasta el final el techo sale ondulado.\n\nEjemplo: prendelo para una caja o una pieza que se "
     "apoya o encastra por arriba; apagado, el valor de fábrica, si el techo puede quedar con la forma de la curva.",
     "It only matters with curved layers on. With this, the layers turn flat again at the top of the part, over the "
     "transition height.\n\nIt helps when the top face has to be flat, because with curved layers all the way up the "
     "top comes out wavy.\n\nExample: turn it on for a box or a part that rests on or fits into something at the top; "
     "off, the default, if the top can keep the shape of the curve."},
    {"curved.segment_length",
     "Solo cuenta con las capas curvas prendidas. Para que la boquilla siga la curva, cada movimiento se parte en "
     "tramos rectos cortos. Este ajuste es el largo de esos tramos, en mm. Los tramos que quedan en línea se vuelven a "
     "unir.\n\nMás cortos siguen mejor la curva, pero el archivo de la impresión es más grande. Más largos, la "
     "boquilla corta camino en línea recta y se aparta de la curva.\n\nEjemplo: 0,5 mm, el valor de fábrica; 0,25 mm "
     "para ondas cortas y apretadas; 1 mm para curvas muy suaves.",
     "It only matters with curved layers on. To follow the curve, each nozzle move is split into short straight "
     "pieces. This setting is the length of those pieces, in mm. Pieces that end up in line are joined "
     "again.\n\nShorter pieces follow the curve better, but the print file gets larger. Longer, the nozzle cuts across "
     "in a straight line and strays from the curve.\n\nExample: 0.5 mm, the default; 0.25 mm for short, tight waves; 1 "
     "mm for very gentle curves."},
    {"curved.max_slope",
     "Solo cuenta con las capas curvas prendidas. En una capa empinada, el costado del cabezal (el ventilador, el "
     "bloque caliente) puede rozar lo ya impreso. Si no mediste el cabezal de tu impresora, este es el ángulo más "
     "empinado, en grados, que se permite.\n\nMás alto, capas más curvas, con más riesgo de choque. Más bajo, más "
     "seguro pero con menos efecto. Medir el cabezal da un límite más justo.\n\nEjemplo: 20°, el valor de fábrica; 10 "
     "a 15° si tu cabezal tiene piezas que bajan casi hasta la punta de la boquilla.",
     "It only matters with curved layers on. On a steep layer, the side of the print head (the fan, the heater block) "
     "can rub what is already printed. If you have not measured your printer's head, this is the steepest angle "
     "allowed, in degrees.\n\nHigher, more curved layers, with more risk of a crash. Lower, safer but with less "
     "effect. Measuring the head gives a better-fitted limit.\n\nExample: 20°, the default; 10 to 15° if your head has "
     "parts that come down almost to the nozzle tip."},
    {"curved.flow_policy",
     "Solo cuenta con las capas curvas prendidas. En una capa curva el grosor no es parejo, y donde es más grueso hace "
     "falta más plástico por cada milímetro. «Mantener» baja la velocidad ahí para que la boquilla no tenga que "
     "derretir más plástico por segundo; donde es más fino no acelera. «Parejo» hoy hace lo mismo. «Sin tocar» deja "
     "las velocidades como están.\n\nEjemplo: «Mantener», el valor de fábrica; «Sin tocar» solo si tu impresora "
     "derrite plástico de sobra y querés que tarde menos.",
     "It only matters with curved layers on. On a curved layer the thickness is not even, and where it is thicker more "
     "plastic is needed per millimetre. Keep slows down there so the nozzle does not have to melt more plastic per "
     "second; where it is thinner it does not speed up. Uniform does the same for now. Leave as is keeps the speeds "
     "unchanged.\n\nExample: Keep, the default; Leave as is only if your printer can melt plenty of plastic and you "
     "want it to take less time."},
    {"curved.uniform_flow",
     "Pensado para la opción «Parejo» del caudal de las capas curvas: un caudal fijo, en mm³ de plástico por segundo, "
     "para toda la capa. Por ahora el programa lo lee pero todavía no lo usa, y «Parejo» funciona igual que "
     "«Mantener».\n\nEjemplo: dejalo en 0, el valor de fábrica.",
     "Meant for the Uniform option of the curved layers' flow: a fixed flow, in mm³ of plastic per second, for the "
     "whole layer. For now the program reads it but does not use it yet, and Uniform works the same as "
     "Keep.\n\nExample: leave it at 0, the default."},
    {"curved.overhang_threshold",
     "Solo cuenta en los modos «Voladizos» y «Automático», y en «Cono» con «Ajustar al cabezal». Es el voladizo más "
     "inclinado, en grados desde la vertical, que tu impresora saca bien con capas planas. Los que se pasan de este "
     "ángulo piden capas curvas para que queden un poco por debajo de él.\n\nMás alto, el programa curva menos y solo "
     "los voladizos más extremos; más bajo, curva más zonas.\n\nEjemplo: 45°, el valor de fábrica; 35 a 40° si tu "
     "impresora sufre con los voladizos; 50 a 55° si los saca bien.",
     "It only matters in the Overhangs and Automatic modes, and in Cone with Fit to the print head. It is the steepest "
     "overhang, in degrees from vertical, that your printer handles well with flat layers. Overhangs beyond this angle "
     "get curved layers that bring them a little below it.\n\nHigher, the program curves less and only the most "
     "extreme overhangs; lower, it curves more areas.\n\nExample: 45°, the default; 35 to 40° if your printer "
     "struggles with overhangs; 50 to 55° if it handles them well."},
    {"infill.nonplanar",
     "Relleno ondulado: las líneas del relleno suben y bajan mientras avanzan, igual en todas las capas, así cada capa "
     "se acomoda dentro de la de abajo y la unión entre capas queda inclinada. Estudios publicados midieron bastante "
     "más resistencia en esa dirección, que es la más débil de una pieza impresa.\n\nSolo funciona con dibujos que se "
     "repiten capa a capa (grilla, triángulos, panal); con el de líneas que giran no ondula. Tarda bastante más, "
     "porque el eje vertical es lento, y el archivo es mucho más grande.\n\nEjemplo: prendelo con relleno en grilla "
     "para una pieza que van a tirar hacia arriba, como un gancho colgante.",
     "Wavy infill: the infill lines rise and fall as they go, the same in every layer, so each layer settles into the "
     "one below and the bond between layers is slanted. Published studies measured much more strength in that "
     "direction, which is the weakest in a printed part.\n\nIt only works with patterns that repeat layer after layer "
     "(grid, triangles, honeycomb); with the line pattern that turns, nothing waves. It takes much longer, because the "
     "vertical axis is slow, and the file is much larger.\n\nExample: turn it on with grid infill for a part that will "
     "be pulled upward, like a hanging hook."},
    {"infill.nonplanar_amplitude",
     "Solo cuenta con el relleno ondulado prendido. Es la altura de la onda, de lo más bajo a lo más alto, en "
     "porcentaje de la altura de capa.\n\nMás alta, más encaje entre capas, pero más subida y bajada, que es lento. El "
     "programa la limita para que la boquilla no toque las líneas vecinas, así que con relleno muy denso puede no "
     "quedar lugar para la onda.\n\nEjemplo: 250 %, el valor de fábrica: 0,5 mm con capas de 0,2 mm. 150 % si querés "
     "que tarde menos.",
     "It only matters with wavy infill on. It is the height of the wave, from lowest to highest point, as a percentage "
     "of the layer height.\n\nTaller, more nesting between layers, but more rising and falling, which is slow. The "
     "program limits it so the nozzle does not touch neighbouring lines, so with very dense infill there may be no "
     "room left for the wave.\n\nExample: 250%, the default: 0.5 mm with 0.2 mm layers. 150% if you want it to take "
     "less time."},
    {"infill.nonplanar_wavelength",
     "Solo cuenta con el relleno ondulado prendido. Es el largo de cada onda, en mm.\n\nMás corta, la onda es más "
     "empinada y la boquilla sube y baja más seguido, lo que tarda más. Más larga, la pendiente es más suave y el "
     "tiempo extra baja bastante, con algo menos de encaje.\n\nEjemplo: 2 mm, el valor de fábrica, con la pendiente de "
     "los estudios publicados; 4 mm reduce la pendiente a la mitad y el tiempo extra a la mitad o menos.",
     "It only matters with wavy infill on. It is the length of each wave, in mm.\n\nShorter, the wave is steeper and "
     "the nozzle goes up and down more often, which takes longer. Longer, the slope is gentler and the extra time "
     "drops a lot, with a little less nesting.\n\nExample: 2 mm, the default, with the slope from the published "
     "studies; 4 mm halves the slope and cuts the extra time by half or more."},
    {"infill.waves",
     "Ondas entrelazadas: el relleno sube y baja como un cartón de huevos, con la misma forma en todas las capas, así "
     "las capas encajan una en otra en vez de apoyarse sobre planos lisos. Las ondas se apagan cerca de las paredes y "
     "de las capas macizas.\n\nFunciona con cualquier dibujo de relleno. No se puede usar junto con el relleno "
     "ondulado, el modo vaso, las capas curvas ni los arcos G2/G3.\n\nEjemplo: prendelo para una pieza que tiene que "
     "aguantar fuerza entre capas, como un soporte que trabaja de costado.",
     "Interlocking waves: the infill rises and falls like an egg crate, with the same shape in every layer, so the "
     "layers fit into each other instead of resting on flat planes. The waves fade out near walls and solid "
     "layers.\n\nIt works with any infill pattern. It cannot be used together with wavy infill, vase mode, curved "
     "layers or G2/G3 arcs.\n\nExample: turn it on for a part that has to take force between layers, like a bracket "
     "loaded from the side."},
    {"infill.wave_amplitude",
     "Solo cuenta con las ondas entrelazadas prendidas. Es cuánto sube y baja el relleno desde su capa, en porcentaje "
     "de la altura de capa.\n\nMás alto, las capas encajan más hondo, pero las ondas son más empinadas. Más bajo, el "
     "encaje es más leve y la impresión se parece más a la normal.\n\nEjemplo: 50 %, el valor de fábrica: con capas de "
     "0,2 mm, el relleno sube y baja 0,1 mm.",
     "It only matters with interlocking waves on. It is how far the infill rises and falls from its layer, as a "
     "percentage of the layer height.\n\nHigher, the layers nest more deeply, but the waves are steeper. Lower, the "
     "nesting is lighter and the print is closer to a normal one.\n\nExample: 50%, the default: with 0.2 mm layers, "
     "the infill rises and falls 0.1 mm."},
    {"infill.wave_length",
     "Solo cuenta con las ondas entrelazadas prendidas. Es la distancia entre dos crestas del cartón de huevos, en mm, "
     "igual de izquierda a derecha que de adelante hacia atrás.\n\nMás corta, más bultos y más empinados; más larga, "
     "ondas más suaves y menos subida y bajada de la boquilla.\n\nEjemplo: 10 mm, el valor de fábrica; 5 mm en una "
     "pieza chica para que entren varias ondas.",
     "It only matters with interlocking waves on. It is the distance between two crests of the egg crate, in mm, the "
     "same left to right as front to back.\n\nShorter, more bumps and steeper ones; longer, gentler waves and less "
     "rising and falling of the nozzle.\n\nExample: 10 mm, the default; 5 mm on a small part so several waves fit."},
    {"vase.enabled",
     "El modo vaso (espiral) imprime la pieza como una sola pared que sube sin parar, en espiral, sin relleno ni techo "
     "y sin costura. Las primeras capas, las del piso, salen planas y macizas; desde ahí la boquilla da vueltas y sube "
     "un poco en cada vuelta.\n\nSale rápido, liso y sin marcas, pero la pieza es hueca y de pared fina, así que es "
     "frágil. No se puede con soportes, se imprime un solo objeto por vez, y una capa con dos contornos (un agujero, "
     "dos partes) sale plana.\n\nEjemplo: un florero, una maceta o una pantalla de lámpara.",
     "Vase mode (spiral) prints the part as a single wall that rises without stopping, in a spiral, with no infill, no "
     "top and no seam. The first layers, the floor, come out flat and solid; from there the nozzle goes round and "
     "rises a little on each turn.\n\nIt is fast, smooth and free of marks, but the part is hollow with a thin wall, "
     "so it is fragile. It cannot use supports, it prints one object at a time, and a layer with two outlines (a hole, "
     "two pieces) comes out flat.\n\nExample: a flower vase, a plant pot or a lampshade."},
    {"fuzzy.mode",
     "La piel difusa hace que la pared de afuera tiemble un poco hacia adentro y hacia afuera, al azar, mientras se "
     "imprime. La superficie queda áspera, como de felpa o piedra.\n\nEsconde las líneas de las capas y pequeñas "
     "fallas, y da mejor agarre, pero la pieza queda un poco más grande o más chica en la superficie. «Por fuera» la "
     "aplica al contorno exterior; «Por fuera y agujeros», también a los agujeros. Las caras pintadas con piel difusa "
     "la llevan aunque esté apagada.\n\nEjemplo: «Por fuera» para el mango de una herramienta; así los agujeros para "
     "tornillos quedan lisos.",
     "Fuzzy skin makes the outer wall wobble slightly in and out, at random, as it prints. The surface comes out "
     "rough, a bit like felt or stone.\n\nIt hides layer lines and small flaws and gives a better grip, but the "
     "surface ends up slightly larger or smaller. Outside applies it to the outer outline; Outside and holes, also to "
     "the holes. Faces painted with fuzzy skin get it even when it is off.\n\nExample: Outside for a tool handle; that "
     "way screw holes stay smooth."},
    {"fuzzy.thickness",
     "Solo cuenta con la piel difusa. Es cuánto puede apartarse la pared de su lugar, como máximo, hacia cada lado, en "
     "mm.\n\nMás alto, la superficie queda más áspera y con más relieve, y la medida de la pieza varía más. Más bajo, "
     "una textura sutil, casi como un mate.\n\nEjemplo: 0,3 mm, el valor de fábrica; 0,1 a 0,2 mm para una textura "
     "suave; 0,5 mm o más para un efecto muy rugoso.",
     "It only matters with fuzzy skin. It is the furthest the wall may move away from its place, to each side, in "
     "mm.\n\nHigher, the surface is rougher with more relief, and the part's size varies more. Lower, a subtle "
     "texture, almost like a matte finish.\n\nExample: 0.3 mm, the default; 0.1 to 0.2 mm for a soft texture; 0.5 mm "
     "or more for a very rough look."},
    {"fuzzy.point_distance",
     "Solo cuenta con la piel difusa. La pared pasa por puntos nuevos corridos al azar; este ajuste es la distancia "
     "media entre esos puntos, en mm. Cada distancia sale al azar entre la mitad y una vez y media de este "
     "valor.\n\nMás corta, una textura más fina y apretada, con un archivo más grande. Más larga, bultos más "
     "espaciados, como ondas.\n\nEjemplo: 0,8 mm, el valor de fábrica; 0,4 mm para una textura tipo lija fina; 2 mm "
     "para bultos grandes.",
     "It only matters with fuzzy skin. The wall passes through new points shifted at random; this setting is the "
     "average distance between those points, in mm. Each distance is picked at random between half and one and a half "
     "times this value.\n\nShorter, a finer, tighter texture and a larger file. Longer, more spaced-out bumps, like "
     "waves.\n\nExample: 0.8 mm, the default; 0.4 mm for a fine sandpaper-like texture; 2 mm for large bumps."},
