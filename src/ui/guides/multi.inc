// Explanations of the multi settings for setting_guide.cpp: {key, Spanish, English},
// one entry per setting. Own texts; see PROVENANCE.md.
    {"sequential.enabled",
     "Con varios objetos en la cama, la impresora suele hacer una capa de todos, después la siguiente de todos, y "
     "así. Con este ajuste encendido los imprime uno por uno: termina un objeto entero y recién ahí empieza el "
     "siguiente, en el orden en que están.\n\nSi uno falla, los demás no se arruinan, y hay menos viajes y menos "
     "hilos entre piezas. A cambio, el cabezal pasa al lado de lo ya impreso sin poder chocarlo: los objetos van "
     "más separados y solo el último puede superar la altura libre del cabezal. Es la única forma de imprimir "
     "varios objetos en modo vaso.\n\nEjemplo: encendido para cuatro llaveros chicos; con una sola pieza no cambia "
     "nada.",
     "With several objects on the bed, the printer usually does one layer of all of them, then the next layer of "
     "all of them. With this on it prints them one at a time: it finishes a whole object before it starts the next, "
     "in the order they come in.\n\nIf one fails, the others are not ruined, and there is less travel and stringing "
     "between parts. The catch is that the head moves right next to finished parts and must not hit them: objects "
     "sit further apart and only the last one may be taller than the head's clearance. It is the only way to print "
     "several objects in vase mode.\n\nExample: on for four small keychains; with a single part it changes nothing."},
    {"extruders.walls",
     "Algunas impresoras tienen varios extrusores: varias boquillas, o una sola que cambia entre varios filamentos. "
     "Este ajuste elige con cuál se imprimen las paredes (el contorno de cada capa, también el de los voladizos y "
     "los rellenitos que quedan entre paredes). Se numeran desde 1; 0 usa el extrusor del objeto.\n\nSirve para que "
     "lo que se ve salga de un filamento y lo de adentro de otro. Cada cambio de extrusor en una capa suma tiempo. "
     "Solo tiene sentido con más de un extrusor.\n\nEjemplo: 0 para hacer todo con el del objeto; 1 para las "
     "paredes con un PLA de buen color y el relleno con otro rollo en el 2.",
     "Some printers have several extruders: several nozzles, or one nozzle that switches between filaments. This "
     "setting picks which one prints the walls (the outline of each layer, including overhang walls and the small "
     "fills left between walls). They are numbered from 1; 0 uses the object's extruder.\n\nIt lets what you see "
     "come from one filament and the inside from another. Each extruder change in a layer adds time. It only "
     "matters with more than one extruder.\n\nExample: 0 to print everything with the object's; 1 for walls in a "
     "nicely coloured PLA, with the infill from another spool in extruder 2."},
    {"extruders.infill",
     "Elige qué extrusor imprime el relleno con huecos de adentro de la pieza. 0 usa el del objeto. Solo tiene "
     "sentido con más de un extrusor.\n\nComo el relleno no se ve, puede ir con un filamento más barato, con el "
     "resto de un rollo o con una boquilla más gruesa que lo hace más rápido. Cada cambio de extrusor en una capa "
     "suma tiempo y, con un cambiador de filamentos (MMU), plástico de purga.\n\nEjemplo: paredes con el extrusor 1 "
     "y relleno con el 2, cargado con un rollo de cualquier color.",
     "Picks which extruder prints the infill, the structure with gaps inside the part. 0 uses the object's. It only "
     "matters with more than one extruder.\n\nSince the infill is hidden, it can use a cheaper filament, the end of "
     "a spool, or a coarser nozzle that prints it faster. Each extruder change in a layer adds time and, on a "
     "filament changer (MMU), purged plastic.\n\nExample: walls with extruder 1 and infill with extruder 2, loaded "
     "with a spool of any colour."},
    {"extruders.solid_infill",
     "Elige qué extrusor imprime las partes macizas que no son paredes: el techo y el piso, las capas llenas del "
     "interior, los puentes (líneas tendidas en el aire entre dos apoyos) y el planchado (una pasada que alisa el "
     "techo). 0 usa el del objeto. Solo tiene sentido con más de un extrusor.\n\nEl techo y el piso se ven, así que "
     "conviene que salgan del mismo filamento que las paredes, salvo que busques otro color arriba y "
     "abajo.\n\nEjemplo: 0, o el mismo número que pusiste para las paredes.",
     "Picks which extruder prints the solid parts that are not walls: the top and bottom, the full layers inside, "
     "bridges (lines laid across the air between two supports) and ironing (a pass that smooths the top). 0 uses "
     "the object's. It only matters with more than one extruder.\n\nThe top and bottom are visible, so they usually "
     "come from the same filament as the walls, unless you want another colour on top and underneath.\n\nExample: "
     "0, or the same number you chose for the walls."},
    {"extruders.support",
     "Elige qué extrusor imprime los soportes, las columnas que sostienen lo que cuelga y se sacan al final. 0 usa "
     "el que esté imprimiendo en ese momento, así no hace falta cambiar de extrusor. Solo tiene sentido con más de "
     "un extrusor.\n\nUn extrusor aparte para el soporte permite usar un material que se despega fácil o que se "
     "disuelve en agua, como el PVA, en piezas con soportes difíciles de sacar. Cada cambio de extrusor suma "
     "tiempo.\n\nEjemplo: 0 para un soporte común; 2 si en el extrusor 2 tenés un filamento soluble.",
     "Picks which extruder prints the supports, the columns that hold up overhangs and are removed afterwards. 0 "
     "uses whichever is printing at the time, so no extruder change is needed. It only matters with more than one "
     "extruder.\n\nA separate extruder for support lets you use a material that peels off easily or dissolves in "
     "water, like PVA, for parts whose supports are hard to remove. Each extruder change adds time.\n\nExample: 0 "
     "for ordinary support; 2 if extruder 2 holds a soluble filament."},
    {"extruders.support_interface",
     "La interfaz son las capas del soporte que tocan la pieza, las más apretadas. Este ajuste elige qué extrusor "
     "las imprime; 0 usa el que esté imprimiendo. Solo tiene sentido con más de un extrusor.\n\nComo la interfaz es "
     "lo que se pega a la pieza, hacerla con un material soluble o que se despega fácil deja limpia la superficie "
     "de abajo, y el resto del soporte puede ir con el filamento común, que es más barato.\n\nEjemplo: soporte con "
     "el extrusor 1 (PLA) e interfaz con el 2 (PVA soluble).",
     "The interface is the support layers that touch the part, the densest ones. This setting picks which extruder "
     "prints them; 0 uses whichever is printing. It only matters with more than one extruder.\n\nSince the "
     "interface is what sticks to the part, making it from a soluble or easy-release material leaves the underside "
     "clean, while the rest of the support uses the ordinary, cheaper filament.\n\nExample: support with extruder 1 "
     "(PLA) and interface with extruder 2 (soluble PVA)."},
    {"extruders.bed_temperature_from",
     "Cada filamento pide su temperatura de cama, pero la cama es una sola. Este ajuste dice de qué extrusor se "
     "toma. 0 elige solo: para la primera capa, la del primer extrusor que imprime en ella; para el resto, la del "
     "primero que imprime en la segunda capa. Solo tiene sentido con más de un extrusor.\n\nConviene elegir el "
     "filamento que más necesita la cama caliente para pegarse y no curvarse.\n\nEjemplo: con PLA en el 1 y PETG en "
     "el 2, poné 2 para que la cama vaya a la temperatura del PETG.",
     "Each filament asks for its own bed temperature, but there is only one bed. This setting says which extruder's "
     "value is used. 0 chooses by itself: for the first layer, that of the first extruder printing on it; for the "
     "rest, that of the first one printing on the second layer. It only matters with more than one "
     "extruder.\n\nPick the filament that most needs a hot bed to stick and not warp.\n\nExample: with PLA in "
     "extruder 1 and PETG in extruder 2, set 2 so the bed runs at the PETG temperature."},
    {"extruders.ooze_prevention",
     "Con varias boquillas, las que no están imprimiendo siguen calientes y el plástico gotea de ellas, dejando "
     "hilos o manchas en la pieza. Con este ajuste encendido, cada boquilla que espera su turno baja su temperatura "
     "(lo que diga el cambio en espera), y la que entra espera a llegar a la suya antes de imprimir.\n\nGotea "
     "menos, pero cada cambio tarda más, porque la boquilla tiene que volver a calentarse. Solo tiene sentido con "
     "más de un extrusor.\n\nEjemplo: apagado si no ves goteo; encendido si la boquilla que espera deja marcas "
     "sobre la pieza.",
     "With several nozzles, the ones not printing stay hot and plastic drips from them, leaving strings or blobs on "
     "the part. With this on, each nozzle waiting for its turn drops its temperature (by the standby change), and "
     "the incoming one waits until it is back at its own before printing.\n\nThere is less dripping, but each "
     "change takes longer because the nozzle has to heat up again. It only matters with more than one "
     "extruder.\n\nExample: off if you see no dripping; on if the waiting nozzle leaves marks on the part."},
    {"extruders.standby_delta",
     "Con la prevención de goteo encendida, es cuántos grados cambia la temperatura de las boquillas que esperan su "
     "turno. Negativo las enfría.\n\nUna baja más grande gotea menos, pero la boquilla tarda más en volver a "
     "calentarse en cada cambio. Una baja chica casi no suma tiempo.\n\nEjemplo: −5 °C, el valor de partida; −20 a "
     "−30 °C si la boquilla que espera sigue goteando y no te molesta esperar un poco más.",
     "With ooze prevention on, it is how many degrees the temperature of the waiting nozzles changes. Negative "
     "cools them.\n\nA bigger drop means less dripping, but the nozzle takes longer to heat back up at each change. "
     "A small drop hardly adds any time.\n\nExample: −5 °C, the starting value; −20 to −30 °C if the waiting nozzle "
     "still drips and you do not mind waiting a little longer."},
    {"multi.fine_walls_coarse_infill",
     "Para impresoras con boquillas de tamaños distintos. Encendido, lo que se ve o tiene que ser preciso (paredes, "
     "partes macizas, soporte y torre de purga) va con la boquilla más fina, y el relleno de adentro con la más "
     "gruesa, que hace una capa de relleno cada varias capas de la pieza: tantas como entren en tres cuartos de su "
     "diámetro.\n\nLa pieza sale con buen detalle y el relleno se imprime mucho más rápido. Mientras está "
     "encendido, manda sobre el extrusor elegido para cada parte; al apagarlo vuelve tu elección.\n\nEjemplo: "
     "boquillas de 0,4 y 0,8 mm con capas de 0,2 mm: el relleno va con la de 0,8 cada 3 capas (0,6 mm).",
     "For printers with nozzles of different sizes. With this on, whatever shows or has to be precise (walls, solid "
     "parts, support and the wipe tower) uses the finest nozzle, and the infill uses the coarsest, which lays one "
     "infill layer every few layers of the part: as many as fit in three quarters of its diameter.\n\nThe part "
     "keeps its detail and the infill prints much faster. While it is on, it overrides the extruder chosen for each "
     "part; turn it off and your choices come back.\n\nExample: 0.4 and 0.8 mm nozzles with 0.2 mm layers: the "
     "infill uses the 0.8 every 3 layers (0.6 mm)."},
    {"multi.color_mixing",
     "Con dos o más filamentos se pueden armar colores que no tenés en ningún rollo: alternando capas finas de "
     "amarillo y rojo, de lejos el ojo ve naranja. Este ajuste dice si se ofrecen esas mezclas. Vacío lo decide la "
     "impresora: sí con una boquilla por filamento, no con un cambiador de filamentos (MMU) de una sola boquilla, "
     "que purga en cada cambio.\n\nMezclar pide muchos cambios: con una MMU eso es mucho tiempo y mucho plástico "
     "tirado, y encenderlo te avisa antes del costo.\n\nEjemplo: dejalo vacío; con una MMU, encendelo solo si "
     "aceptás gastar bastante más filamento.",
     "With two or more filaments you can get colours that no spool has: alternate thin layers of yellow and red, "
     "and from a distance the eye sees orange. This setting says whether those mixes are offered. Left empty, the "
     "printer decides: yes with one nozzle per filament, no with a single-nozzle filament changer (MMU), which "
     "purges at every change.\n\nMixing needs many changes: on an MMU that means a lot of time and wasted plastic, "
     "and turning it on warns you of the cost first.\n\nExample: leave it empty; on an MMU, turn it on only if you "
     "accept using much more filament."},
    {"wipe_tower.enabled",
     "Al cambiar de filamento o de boquilla, la que entra todavía tiene restos del color anterior o no sale pareja. "
     "La torre de purga es un bloque que se imprime al costado de la pieza: los cambios de herramienta se hacen "
     "sobre ella, y ahí se tira ese plástico hasta que sale limpio. Solo funciona con la extrusión relativa de la "
     "impresora.\n\nCon un cambiador de filamentos (MMU) de una sola boquilla se purga lo que diga la tabla de "
     "purga; con varias boquillas, solo unas líneas para que el plástico vuelva a salir parejo. Gasta tiempo y "
     "plástico, pero evita colores mezclados y huecos en la pieza.\n\nEjemplo: encendida para cualquier pieza de "
     "dos colores.",
     "When switching filament or nozzle, the incoming one still holds traces of the old colour or does not flow "
     "evenly yet. The wipe tower is a block printed next to the part: tool changes happen on it, and that plastic "
     "is dumped there until it comes out clean. It only works with the printer's relative extrusion.\n\nWith a "
     "single-nozzle filament changer (MMU) it purges what the purge table says; with several nozzles, just a few "
     "lines to get the plastic flowing again. It costs time and plastic, but avoids mixed colours and gaps in the "
     "part.\n\nExample: on for any two-colour part."},
    {"wipe_tower.width",
     "Es el ancho de la torre de purga, en mm. Lo hondo lo decide la capa que más purga necesita: cuanto más "
     "plástico hay que tirar, más honda.\n\nUna torre más ancha queda menos honda, y al revés: elegí la forma que "
     "mejor entre en el lugar libre de la cama. Una torre muy angosta y alta es menos estable. Solo cuenta con la "
     "torre de purga encendida.\n\nEjemplo: 60 mm, el valor de partida; 40 mm si al costado de la pieza queda poco "
     "lugar a lo ancho.",
     "It is the width of the wipe tower, in mm. Its depth is set by the layer that needs the most purging: the more "
     "plastic to dump, the deeper it gets.\n\nA wider tower is shallower and vice versa: pick the shape that best "
     "fits the free space on the bed. A very narrow, tall tower is less stable. It only matters with the wipe tower "
     "on.\n\nExample: 60 mm, the starting value; 40 mm if there is little room across next to the part."},
    {"wipe_tower.brim_width",
     "Es un borde alrededor del pie de la torre de purga, en la primera capa, para que se agarre mejor a la cama. "
     "Se mide en mm y se hace con vueltas de una línea.\n\nLa torre es alta y angosta, y si se despega a mitad de "
     "la impresión el cabezal la puede voltear. Un borde más ancho la sujeta mejor, con un poco más de plástico. 0 "
     "lo saca.\n\nEjemplo: 2 mm; 4 a 5 mm si la torre se levanta o el material tiende a curvarse, como el ABS.",
     "It is a brim around the foot of the wipe tower on the first layer, so it grips the bed better. It is given in "
     "mm and made of single-line loops.\n\nThe tower is tall and narrow, and if it comes loose mid-print the head "
     "can knock it over. A wider brim holds it better for a little more plastic. 0 removes it.\n\nExample: 2 mm; 4 "
     "to 5 mm if the tower lifts or the material tends to warp, like ABS."},
    {"wipe_tower.x",
     "Es la posición de la esquina de la torre de purga en la cama, de izquierda a derecha, en mm.\n\nLa torre "
     "tiene que quedar dentro de la cama y lejos de la pieza, su falda y su borde: si no, el programa da un error. "
     "Conviene ponerla en un lugar libre y cerca de la pieza, así los viajes de ida y vuelta son "
     "cortos.\n\nEjemplo: 180 mm deja lugar para una torre de 60 mm de ancho en una cama de 250 mm.",
     "It is the position of the wipe tower's corner on the bed, from left to right, in mm.\n\nThe tower has to stay "
     "on the bed and clear of the part, its skirt and its brim, or the program reports an error. Put it in a free "
     "spot close to the part, so the trips back and forth are short.\n\nExample: 180 mm leaves room for a 60 mm "
     "wide tower on a 250 mm bed."},
    {"wipe_tower.y",
     "Es la posición de la esquina de la torre de purga en la cama, de adelante hacia atrás, en mm.\n\nIgual que "
     "con la otra coordenada, la torre tiene que entrar en la cama sin tocar la pieza, su falda ni su borde. "
     "Acordate de que la torre se hace más honda cuanto más purga hay, así que dejale lugar.\n\nEjemplo: 140 mm la "
     "lleva hacia el fondo de una cama de 210 o 250 mm; bajalo si la pieza ocupa esa zona.",
     "It is the position of the wipe tower's corner on the bed, from front to back, in mm.\n\nAs with the other "
     "coordinate, the tower has to fit on the bed without touching the part, its skirt or its brim. Remember the "
     "tower gets deeper the more purging there is, so leave it room.\n\nExample: 140 mm puts it towards the back of "
     "a 210 or 250 mm bed; lower it if the part takes up that area."},
    {"wipe_tower.rotation",
     "Gira la torre de purga sobre su esquina, en grados.\n\nSirve para acomodarla en un hueco libre que no es "
     "paralelo a los bordes de la cama, o para que quede a lo largo en vez de a lo ancho. Después de girarla, "
     "revisá que siga dentro de la cama y lejos de la pieza.\n\nEjemplo: 0, sin giro; 90 para que la torre quede a "
     "lo largo en un hueco angosto.",
     "Turns the wipe tower about its corner, in degrees.\n\nIt helps fit the tower into a free gap that is not "
     "parallel to the bed's edges, or run it lengthwise instead of across. After turning it, check that it is still "
     "on the bed and clear of the part.\n\nExample: 0, no turn; 90 to lay the tower lengthwise in a narrow gap."},
    {"wipe_tower.bridging",
     "La torre de purga no es maciza. En la primera capa se llena entera, pero más arriba lleva columnas separadas, "
     "y las líneas de purga van tendidas de una a otra, como puentes. Este ajuste es la distancia máxima entre esas "
     "columnas, en mm.\n\nMás distancia gasta menos plástico en la torre, pero las líneas tendidas pueden colgar y "
     "la torre queda menos firme. Menos distancia da una torre más sólida. Solo cuenta con la torre de purga "
     "encendida.\n\nEjemplo: 10 mm; bajalo a 5 mm si ves que las líneas de purga se caen entre las columnas.",
     "The wipe tower is not solid. Its first layer is filled completely, but higher up it has separate columns, and "
     "the purge lines span from one to the next, like bridges. This setting is the largest distance between those "
     "columns, in mm.\n\nA larger distance uses less plastic in the tower, but the spanning lines can sag and the "
     "tower is less sturdy. A smaller one gives a firmer tower. It only matters with the wipe tower on.\n\nExample: "
     "10 mm; lower it to 5 mm if the purge lines droop between the columns."},
    {"extruders.wipe_tower",
     "La torre de purga se sigue imprimiendo en las capas sin cambio de herramienta, hasta la del último cambio, "
     "para que crezca pareja con la pieza. Este ajuste elige qué extrusor la imprime en esas capas; 0 usa el que "
     "esté imprimiendo.\n\nCon 0 no hace falta ningún cambio para la torre. Con un número, la torre sale de ese "
     "filamento. Solo tiene sentido con más de un extrusor y la torre de purga encendida.\n\nEjemplo: 0 casi "
     "siempre; 2 si ese extrusor tiene un rollo barato para gastar en la torre.",
     "The wipe tower keeps being printed on layers without a tool change, up to the last change, so it grows along "
     "with the part. This setting picks which extruder prints it on those layers; 0 uses whichever is "
     "printing.\n\nWith 0 no change is needed for the tower. With a number, the tower comes from that filament. It "
     "only matters with more than one extruder and the wipe tower on.\n\nExample: 0 almost always; 2 if that "
     "extruder holds a cheap spool you are happy to spend on the tower."},
    {"wipe_tower.purge_volumes",
     "Cuando una sola boquilla cambia de filamento, adentro queda plástico del anterior y hay que empujar el nuevo "
     "hasta que salga limpio. Esto dice cuánto se tira en la torre en cada cambio, en mm³: un número para todos, o "
     "uno por cada par (de qué filamento sale a cuál entra). Solo cuenta con un cambiador de filamentos (MMU) y la "
     "torre encendida.\n\nPoco deja la pieza manchada con el color de antes; mucho gasta plástico y agranda la "
     "torre. Pasar de oscuro a claro pide más que al revés.\n\nEjemplo: 140 mm³ (unos 58 mm de filamento de 1,75 "
     "mm); de negro a blanco, 300 o más.",
     "When a single nozzle switches filament, plastic from the old one is left inside and the new one has to be "
     "pushed through until it comes out clean. This says how much is dumped on the tower at each change, in mm³: "
     "one number for all, or one per pair (which filament it leaves, which it loads). It only matters with a "
     "filament changer (MMU) and the wipe tower on.\n\nToo little leaves the part stained with the old colour; too "
     "much wastes plastic and grows the tower. Going from dark to light needs more than the other way "
     "round.\n\nExample: 140 mm³ (about 58 mm of 1.75 mm filament); black to white, 300 or more."},
    {"wipe_tower.extra_spacing",
     "Es la separación entre las líneas de la torre de purga, en porcentaje de su ancho. Con 100, las líneas van "
     "una al lado de la otra.\n\nMás separación deja espacio entre las líneas, y la torre se hace más honda para "
     "tirar el mismo plástico; menos las encima unas con otras y la torre queda más compacta. Solo cuenta con la "
     "torre de purga encendida.\n\nEjemplo: 100 % casi siempre; 150 % deja una torre más suelta y más honda.",
     "It is the spacing between the wipe tower's lines, as a percentage of their width. At 100 the lines sit side "
     "by side.\n\nMore spacing leaves gaps between the lines, and the tower gets deeper to dump the same plastic; "
     "less makes them overlap and the tower more compact. It only matters with the wipe tower on.\n\nExample: 100 % "
     "almost always; 150 % gives a looser, deeper tower."},
    {"wipe_tower.extra_flow",
     "Es el ancho de las líneas de la torre de purga, en porcentaje del ancho de las paredes de la pieza.\n\nLíneas "
     "más anchas tiran el mismo plástico en menos recorrido, así la torre queda más chica; líneas más finas la "
     "hacen más honda. Solo cuenta con la torre de purga encendida.\n\nEjemplo: 100 %, como las paredes; 130 a 150 "
     "% para que la torre ocupe menos lugar en la cama.",
     "It is the width of the wipe tower's lines, as a percentage of the part's wall width.\n\nWider lines dump the "
     "same plastic over a shorter path, so the tower is smaller; thinner lines make it deeper. It only matters with "
     "the wipe tower on.\n\nExample: 100 %, like the walls; 130 to 150 % so the tower takes less room on the bed."},
    {"wipe_tower.cone_angle",
     "Sirve para darle a la torre de purga una base en forma de cono, más ancha abajo, que la haga más estable "
     "cuando es alta y angosta. Se da en grados.\n\nPor ahora este programa guarda el valor pero no hace el cono: "
     "cambiarlo no modifica la impresión.\n\nEjemplo: 0, sin cono.",
     "It is meant to give the wipe tower a cone-shaped base, wider at the bottom, to steady it when it is tall and "
     "narrow. It is given in degrees.\n\nFor now this program stores the value but does not build the cone: "
     "changing it does not affect the print.\n\nExample: 0, no cone."},
    {"wipe_tower.no_sparse_layers",
     "Normalmente la torre de purga se imprime en todas las capas hasta el último cambio, aunque en alguna no haya "
     "ninguno. Encendido, la torre solo tiene la primera capa y las capas con cambio, apiladas una sobre otra: en "
     "esas capas la boquilla va hasta la torre, baja hasta ella, hace el cambio y sube derecho antes de volver a la "
     "pieza.\n\nAhorra tiempo y plástico cuando hay pocos cambios, pero la boquilla baja al lado de la pieza y "
     "podría rozar algo. Solo cuenta con la torre de purga encendida.\n\nEjemplo: encendido para un cartel con "
     "letras de otro color solo en las últimas capas.",
     "Normally the wipe tower is printed on every layer up to the last change, even on layers with none. With this "
     "on, the tower has only the first layer and the layers with a change, stacked on each other: on those layers "
     "the nozzle goes to the tower, moves down onto it, does the change and rises straight up before going back to "
     "the part.\n\nIt saves time and plastic when there are few changes, but the nozzle moves down right next to "
     "the part and could brush against something. It only matters with the wipe tower on.\n\nExample: on for a sign "
     "whose lettering is a different colour only in the last few layers."},
    {"sequential.clearance_radius",
     "Al imprimir objeto por objeto, el cabezal trabaja al lado de piezas ya terminadas. Este ajuste es el radio, "
     "en mm, de un cilindro alrededor de la boquilla que encierra todo el cabezal (ventilador, carro, "
     "sensores).\n\nLos objetos se acomodan al menos a esta distancia, y si dos quedan más cerca el programa da un "
     "error. Muy chico, el cabezal puede chocar una pieza; muy grande, entran menos objetos en la cama. Solo cuenta "
     "con objeto por objeto encendido.\n\nEjemplo: medí desde la boquilla hasta la parte más lejana del cabezal; 20 "
     "mm es el valor de partida, y un cabezal grande puede pedir 35 mm o más.",
     "When printing object by object, the head works next to finished parts. This setting is the radius, in mm, of "
     "a cylinder around the nozzle that encloses the whole head (fan, carriage, sensors).\n\nObjects are placed at "
     "least this far apart, and two closer than that are an error. Too small, and the head can hit a part; too "
     "large, and fewer objects fit on the bed. It only matters with object by object on.\n\nExample: measure from "
     "the nozzle to the furthest point of the head; 20 mm is the starting value, and a bulky head may need 35 mm or "
     "more."},
    {"sequential.clearance_height",
     "Al imprimir objeto por objeto, la barra que lleva el cabezal (el eje X) cruza la cama a poca altura y podría "
     "chocar contra los objetos ya terminados. Este ajuste es la altura libre, en mm, entre la punta de la boquilla "
     "y lo primero que puede chocar arriba, que suele ser esa barra.\n\nTodos los objetos salvo el último tienen "
     "que ser más bajos que esto; si no, el programa da un error. Solo cuenta con objeto por objeto "
     "encendido.\n\nEjemplo: medilo con una regla, con la boquilla apoyada en la cama; 20 mm es el valor de "
     "partida.",
     "When printing object by object, the bar that carries the head (the X axis) crosses the bed at a low height "
     "and could hit finished objects. This setting is the free height, in mm, between the nozzle tip and the first "
     "thing above that could collide, usually that bar.\n\nEvery object except the last must be shorter than this, "
     "or the program reports an error. It only matters with object by object on.\n\nExample: measure it with a "
     "ruler, with the nozzle resting on the bed; 20 mm is the starting value."},
    {"mmu.cooling_tube_length",
     "Un cambiador de filamentos (MMU) usa una sola boquilla para varios rollos: en cada cambio saca el filamento "
     "viejo de la parte caliente y mete el nuevo. Al salir, la punta está blanda; se la mueve arriba y abajo en el "
     "tubo de enfriamiento, una zona fría encima de la boquilla, para que se endurezca prolija y no trabe el "
     "cambio.\n\nEste ajuste es el largo de ese tubo, en mm: el recorrido de esas idas y vueltas. Tiene que "
     "coincidir con tu cabezal.\n\nEjemplo: 5 mm; usá el valor que da el fabricante de tu impresora.",
     "A filament changer (MMU) feeds several spools through a single nozzle: at each change it pulls the old "
     "filament out of the hot part and pushes the new one in. On the way out the tip is soft; it is moved up and "
     "down in the cooling tube, a cool zone above the nozzle, so it hardens into a neat shape that does not jam the "
     "change.\n\nThis setting is that tube's length, in mm: how far those back-and-forth moves go. It has to match "
     "your head.\n\nExample: 5 mm; use the value your printer's maker gives."},
    {"mmu.cooling_tube_position",
     "En un cambiador de filamentos (MMU), es la distancia en mm desde la boquilla hasta el tubo de enfriamiento, "
     "la zona fría donde se endurece la punta del filamento que sale. Al descargarlo, el filamento se retira hasta "
     "el medio de ese tubo.\n\nSi no coincide con tu cabezal, la punta se enfría en el lugar equivocado y puede "
     "quedar deformada o con hilos que traban el cambio. Es un dato de la impresora, no algo para ajustar a "
     "ojo.\n\nEjemplo: 91,5 mm con un tubo de 5 mm: la descarga lleva el filamento a 94 mm de la boquilla.",
     "On a filament changer (MMU), it is the distance in mm from the nozzle to the cooling tube, the cool zone "
     "where the tip of the outgoing filament hardens. When unloading, the filament is pulled back to the middle of "
     "that tube.\n\nIf it does not match your head, the tip cools in the wrong place and can come out misshapen or "
     "stringy, which jams the change. It is a property of the printer, not something to tune by eye.\n\nExample: "
     "91.5 mm with a 5 mm tube: unloading takes the filament to 94 mm from the nozzle."},
    {"mmu.parking_position",
     "En un cambiador de filamentos (MMU), es hasta dónde se retira cada filamento cuando no se usa, medido desde "
     "la boquilla en mm. Ahí queda estacionado, esperando su turno. Al cargarlo de nuevo se lo empuja esta "
     "distancia más la corrección de carga.\n\nSi no coincide con tu impresora, el filamento que vuelve no llega a "
     "la boquilla o empuja de más. Es un dato de la impresora.\n\nEjemplo: 92 mm; con la corrección de −2 mm, la "
     "carga empuja 90 mm.",
     "On a filament changer (MMU), it is how far each filament is pulled back while it is not in use, measured from "
     "the nozzle in mm. It stays parked there, waiting for its turn. When it is loaded again it is pushed this "
     "distance plus the loading correction.\n\nIf it does not match your printer, the returning filament falls "
     "short of the nozzle or pushes too far. It is a property of the printer.\n\nExample: 92 mm; with the −2 mm "
     "correction, loading pushes 90 mm."},
    {"mmu.extra_loading_move",
     "En un cambiador de filamentos (MMU), es una corrección de la carga, en mm: se suma a la posición de "
     "estacionamiento para saber cuánto empujar el filamento nuevo hasta la boquilla. Negativo empuja menos; "
     "positivo, más.\n\nSi al empezar la purga el plástico tarda en salir, el filamento no llegó: subilo un poco. "
     "Si al cargar el filamento se aplasta o el motor salta, empuja de más: bajalo.\n\nEjemplo: −2 mm; corregí de a "
     "1 mm.",
     "On a filament changer (MMU), it is a correction to loading, in mm: it is added to the parking position to "
     "work out how far to push the new filament to the nozzle. Negative pushes less; positive, more.\n\nIf plastic "
     "is slow to appear when the purge starts, the filament did not get there: raise it a little. If the filament "
     "gets squashed or the motor skips while loading, it pushes too far: lower it.\n\nExample: −2 mm; adjust in 1 "
     "mm steps."},
    {"mmu.high_current_on_swap",
     "En un cambiador de filamentos (MMU), pide más corriente para el motor durante el cambio, para que empuje y "
     "tire del filamento con más fuerza.\n\nPor ahora este programa lee y guarda el valor, pero no escribe la orden "
     "en el archivo de impresión: encenderlo no cambia nada.\n\nEjemplo: apagado.",
     "On a filament changer (MMU), it asks for more motor current during the change, so it pushes and pulls the "
     "filament harder.\n\nFor now this program reads and stores the value, but does not write the command into the "
     "print file: turning it on changes nothing.\n\nExample: off."},
