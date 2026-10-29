// Explanations of the speed settings for setting_guide.cpp: {key, Spanish, English},
// one entry per setting. Own texts; see PROVENANCE.md.
    {"speed.first_layer",
     "La primera capa es la que se pega a la cama, y conviene imprimirla despacio. Este ajuste es la velocidad máxima "
     "de esa capa, en mm/s (los milímetros que avanza la boquilla por segundo): ninguna línea de la primera capa va "
     "más rápido, salvo el relleno si le diste una velocidad propia.\n\nMás despacio, el plástico tiene tiempo de "
     "aplastarse y agarrarse a la cama; más rápido ahorra unos minutos, pero la pieza se puede despegar. Viene del "
     "perfil de tu impresora.\n\nEjemplo: 20 a 30 mm/s en casi cualquier impresora; si la primera capa no se pega, "
     "bajala a 15.",
     "The first layer is the one that sticks to the bed, and it is best printed slowly. This setting is the top "
     "speed for that layer, in mm/s (the millimetres the nozzle moves each second): no line on the first layer goes "
     "faster, except the infill if you gave it a speed of its own.\n\nSlower, the plastic has time to squash down "
     "and grip the bed; faster saves a few minutes, but the part may come loose. It comes from your printer's "
     "profile.\n\nExample: 20 to 30 mm/s on almost any printer; if the first layer does not stick, lower it to 15."},
    {"speed.first_layer_infill",
     "El relleno macizo de la primera capa (todo lo que va entre las paredes, apoyado en la cama) puede tener una "
     "velocidad propia, en mm/s o como porcentaje de la del relleno sólido. Es lo único de la primera capa que puede "
     "pasar el tope de esa capa.\n\nSirve para que una base grande no tarde tanto: las paredes van despacio y el "
     "interior un poco más rápido. Vacío, el relleno va a la velocidad de la primera capa.\n\nEjemplo: vacío para "
     "empezar; 40 mm/s o «50%» cuando la base es ancha y la cama ya pega bien.",
     "The solid fill of the first layer (everything between the walls, lying on the bed) can have a speed of its "
     "own, in mm/s or as a percentage of the solid infill's. It is the only thing on the first layer allowed past "
     "that layer's cap.\n\nIt keeps a large base from taking too long: the walls go slowly and the inside a bit "
     "faster. Left empty, the fill uses the first layer speed.\n\nExample: empty to start; 40 mm/s or 50% when the "
     "base is wide and the bed already grips well."},
    {"speed.first_layer_over_raft",
     "Solo cuenta si imprimís sobre una balsa (una plataforma de plástico que se imprime debajo de la pieza). Es el "
     "tope de velocidad de la primera capa de la pieza, la que se apoya sobre la balsa, en mm/s o como porcentaje de "
     "cada velocidad.\n\nEsa capa se apoya sobre una superficie con huecos, así que ir despacio ayuda a que se pegue "
     "y a que la cara de abajo salga prolija. La capa siguiente vuelve a sus velocidades. Vacío, usa el tope de la "
     "primera capa.\n\nEjemplo: vacío casi siempre; «50%» si la cara de abajo de la pieza sale fea.",
     "It only matters when you print on a raft (a plastic platform printed under the part). It is the speed cap for "
     "the part's first layer, the one resting on the raft, in mm/s or as a percentage of each speed.\n\nThat layer "
     "lies on a surface full of gaps, so going slowly helps it stick and gives a neater underside. The next layer "
     "goes back to its usual speeds. Left empty, it uses the first layer cap.\n\nExample: empty almost always; 50% "
     "if the underside of the part comes out rough."},
    {"speed.walls",
     "Las paredes son las líneas del contorno de cada capa. Este ajuste es la velocidad de las paredes de adentro, "
     "las que quedan tapadas por la de afuera, en mm/s.\n\nComo no se ven, pueden ir algo más rápido que la de afuera "
     "sin que se note; demasiado rápido, quedan desparejas, la pared de afuera se apoya sobre algo torcido y la pieza "
     "pierde algo de fuerza. Viene del perfil de tu impresora.\n\nEjemplo: 40 a 60 mm/s en una impresora común; 150 "
     "mm/s o más en una rápida.",
     "Walls are the outline lines of each layer. This setting is the speed of the inner walls, the ones hidden "
     "behind the outer one, in mm/s.\n\nSince they do not show, they can go somewhat faster than the outer wall "
     "without it being noticed; too fast, they come out uneven, the outer wall rests on something crooked and the "
     "part loses some strength. It comes from your printer's profile.\n\nExample: 40 to 60 mm/s on a typical "
     "printer; 150 mm/s or more on a fast one."},
    {"speed.outer_wall",
     "Es la velocidad de la pared de afuera, la única que se ve en la pieza terminada. Va en mm/s o como porcentaje "
     "de la de las paredes de adentro.\n\nMás despacio, la superficie sale más lisa y las esquinas más nítidas; más "
     "rápido, aparecen ondas y marcas, sobre todo al lado de las esquinas. Es la primera que conviene bajar si querés "
     "que la pieza se vea bien.\n\nEjemplo: 25 a 35 mm/s, o «50%» de las paredes; en una impresora rápida, 100 a 200 "
     "mm/s.",
     "It is the speed of the outer wall, the only one you see on the finished part. It is given in mm/s or as a "
     "percentage of the inner walls' speed.\n\nSlower gives a smoother surface and sharper corners; faster brings "
     "ripples and marks, especially next to corners. It is the first one to lower when you want the part to look "
     "good.\n\nExample: 25 to 35 mm/s, or 50% of the walls; on a fast printer, 100 to 200 mm/s."},
    {"speed.solid_infill",
     "El relleno sólido es el que llena por completo, sin huecos, como el piso de la pieza. Este ajuste es su "
     "velocidad, en mm/s. También la usan los puentes y el relleno de huecos chiquitos cuando no tienen una "
     "propia.\n\nMás rápido ahorra bastante tiempo en piezas con pisos grandes; demasiado rápido, las líneas no "
     "llegan a pegarse bien entre sí y quedan rendijas. Viene del perfil de tu impresora.\n\nEjemplo: 50 a 80 mm/s en "
     "una impresora común.",
     "Solid infill fills an area completely, with no gaps, like the floor of the part. This setting is its speed, in "
     "mm/s. Bridges and the fill of tiny gaps use it too when they have no speed of their own.\n\nFaster saves quite "
     "a bit of time on parts with large floors; too fast, the lines do not bond well to each other and leave slits. "
     "It comes from your printer's profile.\n\nExample: 50 to 80 mm/s on a typical printer."},
    {"speed.top_solid_infill",
     "Es la velocidad de la superficie de arriba, la última capa maciza que queda a la vista, en mm/s.\n\nMás "
     "despacio, las líneas quedan parejas y la superficie sale lisa y bien cerrada; más rápido, se notan rayas o "
     "agujeritos. Como suele ser poca superficie, bajarla cuesta poco tiempo.\n\nEjemplo: 30 a 50 mm/s; si la cara de "
     "arriba sale rugosa, bajala un poco.",
     "It is the speed of the top surface, the last solid layer that stays visible, in mm/s.\n\nSlower, the lines lie "
     "evenly and the surface comes out smooth and closed; faster, you see streaks or small holes. Since it is "
     "usually a small area, lowering it costs little time.\n\nExample: 30 to 50 mm/s; if the top comes out rough, "
     "lower it a little."},
    {"speed.bridges",
     "Un puente es una línea que cruza por el aire entre dos apoyos, como el techo de una ventana. Este ajuste es su "
     "velocidad, en mm/s, y también la de las paredes que cuelgan sobre el vacío.\n\nEn el aire, el plástico tiene "
     "que endurecerse antes de caer: muy lento, se hunde; muy rápido, se estira y se corta. La velocidad justa "
     "depende del material y del ventilador. Vacío, usa la del relleno sólido.\n\nEjemplo: 25 a 40 mm/s con PLA y el "
     "ventilador al máximo.",
     "A bridge is a line that spans the air between two supports, like the top of a window. This setting is its "
     "speed, in mm/s, and also the speed of walls hanging over empty space.\n\nIn the air the plastic has to harden "
     "before it falls: too slow and it sags, too fast and it stretches and snaps. The right speed depends on the "
     "material and the fan. Left empty, it uses the solid infill speed.\n\nExample: 25 to 40 mm/s with PLA and the "
     "fan at full."},
    {"speed.over_bridges",
     "Es la velocidad de la capa maciza que va justo encima de un puente. Solo los tramos que pasan por arriba del "
     "puente van a esta velocidad; el resto de la línea va a la suya. Se da en mm/s o como porcentaje del relleno "
     "sólido.\n\nEl puente de abajo es delgado y algo flojo; ir más despacio encima lo deforma menos y cierra mejor "
     "la superficie. Vacío, va a la del relleno sólido.\n\nEjemplo: vacío para empezar; «50%» si encima de los "
     "puentes la superficie sale con huecos.",
     "It is the speed of the solid layer right above a bridge. Only the stretches that pass over the bridge use it; "
     "the rest of each line keeps its own speed. It is given in mm/s or as a percentage of the solid infill.\n\nThe "
     "bridge underneath is thin and a bit loose; going slower on top of it disturbs it less and closes the surface "
     "better. Left empty, it uses the solid infill speed.\n\nExample: empty to start; 50% if the surface above "
     "bridges comes out with gaps."},
    {"speed.small_loops",
     "Las paredes muy cortas, como el contorno de un agujero chico o de un pin, pueden ir a una velocidad propia. "
     "Cuenta para toda pared cerrada de menos de unos 41 mm de largo (un círculo de 6,5 mm de radio), también la de "
     "afuera. Va en mm/s o como porcentaje de las paredes.\n\nEn un contorno chico la boquilla vuelve enseguida al "
     "mismo lugar y el plástico no llega a enfriarse; más lento, los agujeros salen redondos y los detalles firmes. "
     "Vacío, cada pared va a la suya.\n\nEjemplo: «25%» de las paredes, o 15 a 20 mm/s.",
     "Very short walls, like the outline of a small hole or a pin, can have a speed of their own. It applies to "
     "every closed wall under about 41 mm long (a circle of 6.5 mm radius), the outer one included. It is given in "
     "mm/s or as a percentage of the walls.\n\nOn a small outline the nozzle is back at the same spot almost at once "
     "and the plastic has no time to cool; slower, holes come out round and small details firm. Left empty, each "
     "wall keeps its own speed.\n\nExample: 25% of the walls, or 15 to 20 mm/s."},
    {"speed.gap_fill",
     "Cuando entre dos paredes queda un espacio demasiado angosto para una línea entera, se llena con líneas finitas: "
     "el relleno de huecos. Este ajuste es su velocidad, en mm/s.\n\nSon líneas cortas, con muchos cambios de "
     "dirección: más despacio salen más parejas, más rápido la máquina se sacude y quedan desprolijas. Vacío, usa la "
     "del relleno sólido.\n\nEjemplo: vacío para empezar; 20 a 40 mm/s si esos huecos salen desparejos.",
     "When the space between two walls is too narrow for a full line, it is filled with thin lines: the gap fill. "
     "This setting is their speed, in mm/s.\n\nThey are short lines with many changes of direction: slower, they "
     "come out more even; faster, the machine shakes and they get messy. Left empty, they use the solid infill "
     "speed.\n\nExample: empty to start; 20 to 40 mm/s if those gaps come out uneven."},
    {"speed.infill",
     "Es la velocidad del relleno de adentro, la estructura con huecos que no se ve, en mm/s.\n\nComo queda "
     "escondido, suele ser la más rápida y es la que más pesa en el tiempo total. Demasiado rápido, el plástico no "
     "alcanza a derretirse bien y el relleno sale débil o con líneas cortadas. Viene del perfil de tu "
     "impresora.\n\nEjemplo: 60 a 80 mm/s en una impresora común; 200 mm/s o más en una rápida.",
     "It is the speed of the infill, the hidden structure with gaps inside the part, in mm/s.\n\nSince it is hidden, "
     "it is usually the fastest and has the biggest effect on the total time. Too fast, the plastic does not melt "
     "fully and the infill comes out weak or with broken lines. It comes from your printer's profile.\n\nExample: 60 "
     "to 80 mm/s on a typical printer; 200 mm/s or more on a fast one."},
    {"speed.support",
     "Es la velocidad de los soportes, las columnas que sostienen lo que cuelga y se sacan al final, en mm/s.\n\nComo "
     "después se tiran, pueden ir bastante rápido; demasiado rápido, se tambalean o se caen antes de terminar. Si la "
     "interfaz (la parte de arriba, que toca la pieza) no tiene velocidad propia, también usa esta. Viene del perfil "
     "de tu impresora.\n\nEjemplo: 40 a 60 mm/s en una impresora común.",
     "It is the speed of the supports, the columns that hold up overhangs and are removed at the end, in mm/s.\n\nAs "
     "they are thrown away, they can go fairly fast; too fast, they wobble or fall over before the print is done. If "
     "the interface (the top part touching the model) has no speed of its own, it uses this one too. It comes from "
     "your printer's profile.\n\nExample: 40 to 60 mm/s on a typical printer."},
    {"speed.support_interface",
     "La interfaz es la parte de arriba del soporte, más densa, sobre la que se apoya la pieza. Este ajuste es su "
     "velocidad, en mm/s o como porcentaje de la del soporte. Solo importa si hay soportes.\n\nMás despacio, la "
     "interfaz sale más pareja y la cara de la pieza que se apoya ahí queda más prolija. Vacío, va a la velocidad del "
     "soporte.\n\nEjemplo: vacío para empezar; «50%» o 25 mm/s si la cara que va sobre el soporte sale fea.",
     "The interface is the denser top part of the support, the one the model rests on. This setting is its speed, in "
     "mm/s or as a percentage of the support speed. It only matters when there are supports.\n\nSlower, the "
     "interface comes out more even and the face of the part resting on it is neater. Left empty, it uses the "
     "support speed.\n\nExample: empty to start; 50% or 25 mm/s if the face printed on the support comes out rough."},
    {"speed.travel",
     "Un viaje es cuando la boquilla se mueve de un lugar a otro sin poner plástico. Este ajuste es la velocidad de "
     "esos movimientos, en mm/s.\n\nMás rápido, el plástico tiene menos tiempo para gotear en el camino, quedan menos "
     "hilos y se ahorra tiempo; demasiado rápido para tu máquina, los motores pueden perder pasos y las capas salen "
     "corridas. Viene del perfil de tu impresora.\n\nEjemplo: 150 a 180 mm/s en una impresora común; 300 mm/s o más "
     "en una rápida.",
     "A travel is when the nozzle moves from one place to another without laying plastic. This setting is the speed "
     "of those moves, in mm/s.\n\nFaster, the plastic has less time to ooze on the way, there are fewer strings and "
     "time is saved; too fast for your machine, the motors can skip steps and the layers shift. It comes from your "
     "printer's profile.\n\nExample: 150 to 180 mm/s on a typical printer; 300 mm/s or more on a fast one."},
    {"speed.travel_z",
     "Es la velocidad de los movimientos hacia arriba y hacia abajo: el paso de una capa a la siguiente y los saltos "
     "que da la boquilla para viajar sin rozar la pieza. Va en mm/s.\n\nEn muchas impresoras el eje vertical es mucho "
     "más lento que los otros, y pedirle demasiado lo hace trabarse o hacer ruido. Vacío, usa la velocidad de los "
     "viajes.\n\nEjemplo: 5 a 15 mm/s en una impresora común con tornillos en el eje vertical.",
     "It is the speed of the up and down moves: stepping from one layer to the next and the hops the nozzle makes to "
     "travel without scraping the part. It is given in mm/s.\n\nOn many printers the vertical axis is much slower "
     "than the others, and asking too much of it makes it stall or grind. Left empty, it uses the travel "
     "speed.\n\nExample: 5 to 15 mm/s on a typical printer with lead screws on the vertical axis."},
    {"travel.avoid_crossing_walls",
     "Con esto encendido, cuando la boquilla viaja de un punto a otro dentro de la misma pieza, busca un camino por "
     "adentro que no cruce las paredes: rodea un agujero en vez de pasarle por arriba.\n\nAsí no quedan marcas ni "
     "hilos sobre la superficie que se ve, a cambio de viajes algo más largos. Apagado, todo viaje va en línea recta "
     "y, si cruza una pared o sale de la pieza, retrae el filamento.\n\nEjemplo: dejalo encendido, como viene; "
     "apagalo si querés ganar algo de tiempo en una pieza donde las marcas no importan.",
     "With this on, when the nozzle travels from one point to another inside the same part, it looks for a route "
     "inside that does not cross the walls: it goes around a hole instead of over it.\n\nThat way no marks or "
     "strings are left on the visible surface, at the cost of somewhat longer travels. Off, every travel goes in a "
     "straight line and, if it crosses a wall or leaves the part, the filament is retracted.\n\nExample: leave it "
     "on, as it comes; turn it off to save a little time on a part where marks do not matter."},
    {"travel.max_detour",
     "Solo cuenta si los viajes evitan cruzar paredes (el ajuste de arriba). Pone un límite al rodeo: si el camino "
     "por adentro es más largo que el recto más este valor, el viaje va recto, cruzando las paredes, y retrae el "
     "filamento. Va en mm o como porcentaje del viaje recto.\n\nUn límite ahorra tiempo en piezas con muchos "
     "agujeros, donde los rodeos se alargan; sin límite, siempre se rodea. 0 es sin límite.\n\nEjemplo: 0 para "
     "empezar; «200%» si ves que la boquilla da vueltas enormes para llegar cerca.",
     "It only matters when travels avoid crossing walls (the setting above). It limits the detour: if the route "
     "inside is longer than the straight one plus this value, the travel goes straight, across the walls, and "
     "retracts the filament. It is given in mm or as a percentage of the straight travel.\n\nA limit saves time on "
     "parts with many holes, where detours grow long; with no limit, it always goes around. 0 means no "
     "limit.\n\nExample: 0 to start; 200% if you see the nozzle taking huge detours to reach a nearby spot."},
    {"speed.max",
     "Es un tope general: ninguna línea que pone plástico va más rápido que esto, sea pared, relleno o soporte. Los "
     "viajes no cuentan. Va en mm/s.\n\nSirve para frenar todo de una vez sin cambiar cada velocidad, por ejemplo con "
     "un material que no aguanta ir rápido o una impresora que se sacude. Vacío, no hay tope.\n\nEjemplo: vacío casi "
     "siempre; 40 mm/s para imprimir un filamento flexible sin tocar el resto de los ajustes.",
     "It is an overall cap: no line that lays plastic goes faster than this, whether wall, infill or support. "
     "Travels do not count. It is given in mm/s.\n\nIt slows everything down at once without changing each speed, "
     "for example with a material that cannot go fast or a printer that shakes. Left empty, there is no "
     "cap.\n\nExample: empty almost always; 40 mm/s to print a flexible filament without touching the other settings."},
    {"speed.max_flow",
     "El caudal es cuánto plástico sale por segundo, en mm³/s (milímetros cúbicos por segundo): una línea más ancha, "
     "más alta o más rápida pide más. La boquilla solo puede derretir una cierta cantidad por segundo. Este ajuste "
     "pone un tope: si una línea pediría más, va más despacio.\n\nTambién pueden poner un tope la impresora y el "
     "filamento; manda el menor de los tres. 0 es sin tope propio.\n\nEjemplo: 0 para empezar; 8 a 12 mm³/s con PLA "
     "en una impresora común, o lo que te dé la calibración de caudal.",
     "Flow is how much plastic comes out per second, in mm³/s (cubic millimetres per second): a wider, taller or "
     "faster line asks for more. The nozzle can only melt so much per second. This setting caps it: if a line would "
     "need more, it goes slower.\n\nThe printer and the filament can set a cap too; the lowest of the three wins. 0 "
     "means no cap of its own.\n\nExample: 0 to start; 8 to 12 mm³/s with PLA on a typical printer, or whatever the "
     "flow calibration gives you."},
    {"speed.flow_slope_up",
     "Al pasar de una línea lenta o fina a una rápida o gruesa, el caudal (cuánto plástico sale por segundo) pega un "
     "salto. Este ajuste limita cuánto puede subir por segundo, en mm³/s²: el comienzo de la línea nueva se arma en "
     "tramos cada vez más rápidos hasta llegar a su velocidad.\n\nLa presión dentro de la boquilla tarda en "
     "acomodarse; subir de a poco evita que falte plástico justo en el cambio. No se aplica después de una "
     "retracción. 0 lo deja libre.\n\nEjemplo: 0 casi siempre; probá un valor chico, como 5 mm³/s², si ves líneas "
     "flacas donde la velocidad cambia de golpe.",
     "When going from a slow or thin line to a fast or thick one, the flow (how much plastic comes out per second) "
     "jumps. This setting limits how fast it may rise, in mm³/s²: the start of the new line is built from steps that "
     "get faster until it reaches its speed.\n\nThe pressure inside the nozzle takes time to settle; rising "
     "gradually keeps plastic from running short right at the change. It does not apply after a retraction. 0 leaves "
     "it free.\n\nExample: 0 almost always; try a small value, like 5 mm³/s², if you see thin lines where the speed "
     "changes suddenly."},
    {"speed.flow_slope_down",
     "Es lo contrario del anterior: cuánto puede bajar el caudal por segundo, en mm³/s², al pasar de una línea rápida "
     "a una lenta. El final de la línea rápida se arma en tramos cada vez más lentos hasta el caudal de la "
     "siguiente.\n\nSin esto, la presión que quedó en la boquilla puede empujar plástico de más al empezar la línea "
     "lenta y dejar un engrosamiento. 0 lo deja libre.\n\nEjemplo: 0 casi siempre; probá un valor chico, como 5 "
     "mm³/s², si ves bultos donde la pieza pasa del relleno a una pared.",
     "It is the opposite of the previous one: how fast the flow may drop, in mm³/s², when going from a fast line to "
     "a slow one. The end of the fast line is built from steps that get slower until they reach the next line's "
     "flow.\n\nWithout it, the pressure left in the nozzle can push out too much plastic at the start of the slow "
     "line and leave a bulge. 0 leaves it free.\n\nExample: 0 almost always; try a small value, like 5 mm³/s², if "
     "you see blobs where the print goes from infill to a wall."},
    {"speed.dynamic_overhangs",
     "Una pared que sobresale de la capa de abajo queda en parte apoyada en el aire. Con esto encendido, cada pared "
     "va más despacio cuanto menos apoyo tiene: las cuatro velocidades de voladizo dicen a cuánto va con 0, 25, 50 y "
     "75 % de apoyo, y entre esos puntos pasa de una a otra de a poco.\n\nIr más lento en el aire le da tiempo al "
     "plástico a enfriarse y los voladizos salen mucho más prolijos, a cambio de algo de tiempo. Nunca hace ir una "
     "pared más rápido que la suya. No se aplica en el modo vaso.\n\nEjemplo: encendelo en piezas con partes "
     "inclinadas, como una figura con los brazos abiertos o la mitad de abajo de una esfera.",
     "A wall that sticks out past the layer below rests partly on air. With this on, each wall goes slower the less "
     "support it has: the four overhang speeds say how fast it goes with 0, 25, 50 and 75 % support, and between "
     "those points it shifts gradually from one to the next.\n\nGoing slower over air gives the plastic time to cool "
     "and overhangs come out much cleaner, for some extra time. It never makes a wall go faster than its own speed. "
     "It does not apply in vase mode.\n\nExample: turn it on for parts with sloping areas, like a figure with "
     "outstretched arms or the lower half of a sphere."},
    {"speed.overhang_0",
     "Solo cuenta con las velocidades de voladizo encendidas. Es la velocidad de una pared sin nada de apoyo (0 %), "
     "que cuelga entera en el aire, y de los tramos que van completamente por el aire. Va en mm/s o como porcentaje "
     "de la pared de afuera.\n\nEs la más lenta de las cuatro: el plástico tiene que endurecerse casi sin apoyo. Más "
     "lento cuelga menos, pero tarda más.\n\nEjemplo: 15 mm/s, como viene; 10 mm/s si esos tramos se caen.",
     "It only matters with the overhang speeds turned on. It is the speed of a wall with no support at all (0 %), "
     "hanging entirely in the air, and of stretches that run completely over air. It is given in mm/s or as a "
     "percentage of the outer wall.\n\nIt is the slowest of the four: the plastic has to harden with almost nothing "
     "under it. Slower droops less, but takes longer.\n\nExample: 15 mm/s, as it comes; 10 mm/s if those stretches "
     "fall."},
    {"speed.overhang_1",
     "Solo cuenta con las velocidades de voladizo encendidas. Es la velocidad de una pared con un cuarto de su línea "
     "apoyado en la capa de abajo (25 %) y tres cuartos en el aire. Va en mm/s o como porcentaje de la pared de "
     "afuera.\n\nCon tan poco apoyo conviene ir casi tan lento como sin apoyo.\n\nEjemplo: 15 mm/s, como viene; "
     "súbila a 20 si esos voladizos ya salen bien.",
     "It only matters with the overhang speeds turned on. It is the speed of a wall with a quarter of its line "
     "resting on the layer below (25 %) and three quarters in the air. It is given in mm/s or as a percentage of the "
     "outer wall.\n\nWith so little support it is best to go almost as slowly as with none.\n\nExample: 15 mm/s, as "
     "it comes; raise it to 20 if those overhangs already come out well."},
    {"speed.overhang_2",
     "Solo cuenta con las velocidades de voladizo encendidas. Es la velocidad de una pared con la mitad de su línea "
     "apoyada en la capa de abajo (50 %) y la otra mitad en el aire. Va en mm/s o como porcentaje de la pared de "
     "afuera.\n\nCon la mitad apoyada ya se puede ir un poco más rápido.\n\nEjemplo: 20 mm/s, como viene; bajala si "
     "las paredes inclinadas salen con rulos o despeinadas.",
     "It only matters with the overhang speeds turned on. It is the speed of a wall with half of its line resting on "
     "the layer below (50 %) and the other half in the air. It is given in mm/s or as a percentage of the outer "
     "wall.\n\nWith half of it supported it can go a little faster.\n\nExample: 20 mm/s, as it comes; lower it if "
     "sloping walls come out curled or ragged."},
    {"speed.overhang_3",
     "Solo cuenta con las velocidades de voladizo encendidas. Es la velocidad de una pared con tres cuartos de su "
     "línea apoyados (75 %). Entre este punto y el apoyo completo, la velocidad sube de a poco hasta la de la "
     "pared.\n\nEs la más rápida de las cuatro, porque la pared ya está casi toda apoyada.\n\nEjemplo: 25 mm/s, como "
     "viene; «50%» de la pared de afuera si querés que acompañe lo que pongas ahí.",
     "It only matters with the overhang speeds turned on. It is the speed of a wall with three quarters of its line "
     "supported (75 %). Between this point and full support, the speed rises gradually to the wall's own.\n\nIt is "
     "the fastest of the four, because the wall is already almost fully supported.\n\nExample: 25 mm/s, as it comes; "
     "50% of the outer wall if you want it to follow whatever you set there."},
    {"acceleration.default",
     "La aceleración dice qué tan rápido la boquilla llega a su velocidad y frena, en mm/s² (cuántos mm/s gana por "
     "cada segundo): baja, arranca y frena suave; alta, de golpe. Este es el valor general, al que se vuelve después "
     "de cada tipo de línea, y el del soporte, la falda y el relleno de huecos.\n\nMás aceleración ahorra tiempo, "
     "sobre todo con muchas esquinas, pero sacude la máquina y deja ondas en las paredes. En 0 no se usan las "
     "aceleraciones por tipo de línea: solo la de los viajes, si la tiene.\n\nEjemplo: 0 para empezar; 500 a 1000 "
     "mm/s² en una impresora con la cama que va y viene; 3000 a 10 000 en una rápida.",
     "Acceleration is how quickly the nozzle reaches its speed and slows down, in mm/s² (how many mm/s it gains each "
     "second): low starts and stops gently; high, abruptly. This is the general value, the one used again after each "
     "kind of line, and the one for supports, the skirt and gap fill.\n\nMore acceleration saves time, especially "
     "with many corners, but shakes the machine and leaves ripples on the walls. At 0 the per-line accelerations are "
     "not used: only the travel one, if set.\n\nExample: 0 to start; 500 to 1000 mm/s² on a printer whose bed slides "
     "back and forth; 3000 to 10000 on a fast one."},
    {"acceleration.walls",
     "Es la aceleración de las paredes de adentro, en mm/s². Solo cuenta si la aceleración general no está en "
     "0.\n\nComo no se ven, pueden llevar algo más que la pared de afuera sin que se note en la superficie. 0 usa la "
     "general.\n\nEjemplo: 0, o un poco más que la general, como 1500 mm/s² si la general es 1000.",
     "It is the acceleration of the inner walls, in mm/s². It only matters when the general acceleration is not "
     "0.\n\nSince they are hidden, they can take a bit more than the outer wall without it showing on the surface. 0 "
     "uses the general one.\n\nExample: 0, or a little above the general one, like 1500 mm/s² when the general one "
     "is 1000."},
    {"acceleration.outer_wall",
     "Es la aceleración de la pared de afuera, la que se ve, en mm/s². Solo cuenta si la aceleración general no está "
     "en 0.\n\nBajarla es la forma más directa de sacar las ondas que aparecen al lado de las esquinas: la máquina se "
     "sacude menos justo donde se nota. Cuesta poco tiempo. 0 usa la de las paredes de adentro.\n\nEjemplo: 500 a "
     "1000 mm/s² en una impresora común, aunque el resto vaya más alto.",
     "It is the acceleration of the outer wall, the visible one, in mm/s². It only matters when the general "
     "acceleration is not 0.\n\nLowering it is the most direct way to get rid of the ripples that show up next to "
     "corners: the machine shakes less exactly where it shows. It costs little time. 0 uses the inner walls' "
     "value.\n\nExample: 500 to 1000 mm/s² on a typical printer, even when the rest goes higher."},
    {"acceleration.infill",
     "Es la aceleración del relleno de adentro, el que tiene huecos, en mm/s². Solo cuenta si la aceleración general "
     "no está en 0.\n\nComo no se ve y tiene muchos cambios de dirección, suele ser la más alta: ahí es donde una "
     "aceleración alta ahorra más tiempo. Si es demasiado, la máquina vibra y esa vibración puede llegar a las "
     "paredes. 0 usa la general.\n\nEjemplo: 0, o el doble de la general, como 2000 mm/s² si la general es 1000.",
     "It is the acceleration of the sparse infill inside the part, in mm/s². It only matters when the general "
     "acceleration is not 0.\n\nSince it is hidden and full of changes of direction, it is usually the highest: that "
     "is where a high acceleration saves the most time. Too much, and the machine vibrates and the shaking can reach "
     "the walls. 0 uses the general one.\n\nExample: 0, or double the general one, like 2000 mm/s² when the general "
     "one is 1000."},
    {"acceleration.solid_infill",
     "Es la aceleración del relleno sólido, el que llena sin huecos, como el piso de la pieza, en mm/s². Solo cuenta "
     "si la aceleración general no está en 0.\n\nUn poco más baja que la del relleno con huecos deja las líneas más "
     "parejas y mejor pegadas. 0 usa la del relleno.\n\nEjemplo: 0, o algo menos que la del relleno, como 1500 mm/s² "
     "si esa es 2000.",
     "It is the acceleration of the solid infill, the one that fills with no gaps, like the floor of the part, in "
     "mm/s². It only matters when the general acceleration is not 0.\n\nA bit lower than the sparse infill's gives "
     "more even, better bonded lines. 0 uses the infill's value.\n\nExample: 0, or somewhat below the infill's, like "
     "1500 mm/s² when that one is 2000."},
    {"acceleration.top_solid_infill",
     "Es la aceleración de la superficie de arriba, la última capa que queda a la vista, en mm/s². Solo cuenta si la "
     "aceleración general no está en 0.\n\nMás baja, las líneas quedan más parejas y la superficie más lisa, con poco "
     "costo de tiempo. 0 usa la del relleno sólido.\n\nEjemplo: 0, o algo baja, como 800 a 1000 mm/s², si la cara de "
     "arriba sale con marcas.",
     "It is the acceleration of the top surface, the last layer that stays visible, in mm/s². It only matters when "
     "the general acceleration is not 0.\n\nLower, the lines lie more evenly and the surface is smoother, for little "
     "extra time. 0 uses the solid infill's value.\n\nExample: 0, or something low, like 800 to 1000 mm/s², if the "
     "top comes out with marks."},
    {"acceleration.bridges",
     "Es la aceleración de los puentes (líneas que cruzan por el aire entre dos apoyos) y de las paredes que cuelgan "
     "sobre el vacío, en mm/s². Solo cuenta si la aceleración general no está en 0.\n\nUna línea en el aire se "
     "tironea fácil: más suave, sale más recta y se corta menos. 0 usa la general.\n\nEjemplo: 0, o una baja, como "
     "500 mm/s², si los puentes se cortan o se tuercen.",
     "It is the acceleration of bridges (lines spanning the air between two supports) and of walls hanging over "
     "empty space, in mm/s². It only matters when the general acceleration is not 0.\n\nA line in the air is easily "
     "jerked: gentler, it comes out straighter and breaks less. 0 uses the general one.\n\nExample: 0, or a low one, "
     "like 500 mm/s², if bridges break or twist."},
    {"acceleration.wipe_tower",
     "Cuando se imprime con varios filamentos, la boquilla se limpia después de cada cambio en una torrecita al lado "
     "de la pieza: la torre de purga. Este ajuste es su aceleración, en mm/s². Solo cuenta si hay torre y la "
     "aceleración general no está en 0.\n\nLa torre es angosta y alta; una aceleración más baja evita que se sacuda o "
     "se caiga. 0 usa la general.\n\nEjemplo: 0 para empezar; algo menos que la general si la torre se tambalea.",
     "When printing with several filaments, the nozzle cleans itself after each change on a small tower next to the "
     "part: the wipe tower. This setting is its acceleration, in mm/s². It only matters when there is a tower and "
     "the general acceleration is not 0.\n\nThe tower is narrow and tall; a lower acceleration keeps it from shaking "
     "or falling over. 0 uses the general one.\n\nExample: 0 to start; somewhat below the general one if the tower "
     "wobbles."},
    {"acceleration.first_layer",
     "Es la aceleración de toda la primera capa, la que se pega a la cama, en mm/s². Con balsa (una plataforma que se "
     "imprime debajo de la pieza), es la de la primera capa de la balsa.\n\nMás suave, las líneas recién puestas no "
     "se arrastran ni se despegan de la cama. 0 deja la de cada tipo de línea.\n\nEjemplo: 500 mm/s² es un valor "
     "tranquilo para casi cualquier impresora; 0 si la primera capa ya sale bien.",
     "It is the acceleration of the whole first layer, the one that sticks to the bed, in mm/s². With a raft (a "
     "platform printed under the part), it is the raft's first layer.\n\nGentler, the freshly laid lines are not "
     "dragged or pulled off the bed. 0 keeps each kind of line's own value.\n\nExample: 500 mm/s² is a calm value "
     "for almost any printer; 0 if the first layer already comes out well."},
    {"acceleration.first_layer_over_raft",
     "Solo cuenta si imprimís sobre una balsa (una plataforma de plástico que se imprime debajo de la pieza). Es la "
     "aceleración de la primera capa de la pieza, la que se apoya sobre la balsa, en mm/s².\n\nEsa capa se apoya "
     "sobre algo con huecos; más suave, se pega mejor y la cara de abajo sale más prolija. 0 deja la de cada tipo de "
     "línea.\n\nEjemplo: 0 casi siempre; 500 mm/s² si la cara de abajo sale fea.",
     "It only matters when you print on a raft (a plastic platform printed under the part). It is the acceleration "
     "of the part's first layer, the one resting on the raft, in mm/s².\n\nThat layer lies on something full of "
     "gaps; gentler, it sticks better and the underside is neater. 0 keeps each kind of line's own "
     "value.\n\nExample: 0 almost always; 500 mm/s² if the underside comes out rough."},
    {"acceleration.travel",
     "Es la aceleración de los viajes, cuando la boquilla se mueve sin poner plástico, en mm/s². Es la única que se "
     "usa aunque la general esté en 0; en ese caso, las líneas que siguen al viaje se quedan con ella.\n\nEn un viaje "
     "no hay plástico que se arruine, así que suele ser la más alta; demasiado para tu máquina, los motores pueden "
     "perder pasos y las capas salen corridas. 0 no le da una propia.\n\nEjemplo: 0 para empezar; 1000 a 2000 mm/s² "
     "en una impresora común, 5000 o más en una rápida.",
     "It is the acceleration of travels, when the nozzle moves without laying plastic, in mm/s². It is the only one "
     "used even when the general one is 0; in that case the lines after the travel keep it.\n\nOn a travel there is "
     "no plastic to spoil, so it is usually the highest; too much for your machine, and the motors can skip steps "
     "and the layers shift. 0 gives travels no value of their own.\n\nExample: 0 to start; 1000 to 2000 mm/s² on a "
     "typical printer, 5000 or more on a fast one."},
    {"acceleration.short_travel",
     "Es la aceleración de los viajes muy cortos, los que miden menos que la distancia mínima para retraer (un ajuste "
     "de la retracción), en mm/s².\n\nEn un viaje corto la boquilla casi no llega a tomar velocidad, así que una "
     "aceleración más baja apenas suma tiempo y mueve más suave la máquina cerca de lo recién impreso. 0 usa la de "
     "los viajes.\n\nEjemplo: 0 casi siempre; la mitad de la de los viajes si ves marcas en zonas con muchos saltos "
     "cortos.",
     "It is the acceleration of very short travels, the ones shorter than the minimum distance for a retraction (a "
     "retraction setting), in mm/s².\n\nOn a short travel the nozzle hardly gets up to speed, so a lower "
     "acceleration adds almost no time and moves the machine more gently next to freshly printed plastic. 0 uses the "
     "travel value.\n\nExample: 0 almost always; half the travel value if you see marks in areas with many short "
     "hops."},
    {"travel.ramping_lift",
     "Cuando la boquilla viaja y retrae, puede subir un poco para no rozar la pieza. Normalmente sube derecho, viaja "
     "y baja. Con esto encendido, sube en rampa mientras ya avanza, como un avión al despegar, hasta la altura de la "
     "rampa; el resto del viaje va a esa altura y al final baja derecho.\n\nEl movimiento es más continuo, sin parar "
     "a subir. Necesita una pendiente mayor que 0: con 0, el salto es vertical, como siempre.\n\nEjemplo: apagado "
     "para empezar; encendido con una pendiente de unos 5 a 10° si querés probar viajes más suaves.",
     "When the nozzle travels and retracts, it can rise a little so it does not scrape the part. Normally it goes "
     "straight up, travels and comes down. With this on, it climbs in a ramp while already moving, like a plane "
     "taking off, up to the ramp height; the rest of the travel stays at that height and it drops straight down at "
     "the end.\n\nThe move is more continuous, with no stop to rise. It needs a slope above 0: at 0 the hop is "
     "vertical, as usual.\n\nExample: off to start; on with a slope of about 5 to 10° if you want to try smoother "
     "travels."},
    {"travel.slope",
     "Solo cuenta con el salto en rampa encendido. Es la inclinación de la rampa, en grados.\n\nCon pocos grados la "
     "rampa es suave y larga, y en un viaje corto quizás no llega arriba (sube hasta donde alcanza); con muchos, sube "
     "enseguida, casi en vertical. 0 vuelve al salto vertical de siempre.\n\nEjemplo: 5 a 10° para empezar.",
     "It only matters with the ramping lift turned on. It is the slope of the ramp, in degrees.\n\nWith few degrees "
     "the ramp is gentle and long, and on a short travel it may not reach the top (it climbs as far as it gets); "
     "with many, it rises at once, almost vertically. 0 goes back to the usual vertical hop.\n\nExample: 5 to 10° to "
     "start."},
    {"travel.max_lift",
     "Solo cuenta con el salto en rampa encendido. Es la altura hasta la que sube la rampa, en mm.\n\nMás alto, la "
     "boquilla pasa lejos de lo impreso y no lo roza, pero tarda más en subir y bajar. 0 usa la altura del salto de "
     "la retracción.\n\nEjemplo: 0 para usar la misma que el salto; 0,4 mm si la boquilla todavía raspa la pieza.",
     "It only matters with the ramping lift turned on. It is the height the ramp climbs to, in mm.\n\nHigher, the "
     "nozzle stays well clear of the print and does not scrape it, but takes longer to go up and down. 0 uses the "
     "retraction's lift height.\n\nExample: 0 to use the same as the lift; 0.4 mm if the nozzle still scrapes the "
     "part."},
    {"travel.lift_before_obstacle",
     "Pide una rampa más empinada cuando hay algo cerca del comienzo del viaje, para no llevárselo por delante. Por "
     "ahora este programa lo lee y lo guarda con tus ajustes, pero todavía no cambia la impresión: encendido o "
     "apagado, el resultado es el mismo.\n\nEjemplo: dejalo como está; no hace falta tocarlo.",
     "It asks for a steeper ramp when something is close to the start of the travel, so the nozzle does not run into "
     "it. For now this program reads it and keeps it with your settings, but it does not change the print yet: on or "
     "off, the result is the same.\n\nExample: leave it as it is; there is no need to touch it."},
