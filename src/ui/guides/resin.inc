// Explanations of the resin settings for setting_guide.cpp: {key, Spanish, English},
// one entry per setting. Own texts; see PROVENANCE.md.
    {"resin.layer_height",
     "Una impresora de resina arma la pieza de a una capa: la plataforma de impresión baja dentro de la cubeta con "
     "resina líquida y una pantalla que está abajo ilumina la forma de la capa, que se endurece con la luz. Este "
     "ajuste es el grosor de cada capa, el mismo para todas.\n\nCapas más finas dejan superficies más lisas y menos "
     "escalones en las curvas, pero son más capas y la impresión tarda más. Capas más gruesas imprimen más rápido, "
     "pero cada una necesita más luz para curarse del todo.\n\nEjemplo: 0,05 mm es lo habitual; 0,03 mm para una "
     "miniatura con mucho detalle; 0,1 mm para una pieza grande donde importa más el tiempo.",
     "A resin printer builds the part one layer at a time: the build plate dips into the vat of liquid resin and a "
     "screen underneath lights up the shape of the layer, which hardens under the light. This setting is how thick "
     "each layer is, the same for all of them.\n\nThinner layers give smoother surfaces and smaller steps on curves, "
     "but there are more of them and the print takes longer. Thicker layers print faster, but each one needs more "
     "light to cure all the way through.\n\nExample: 0.05 mm is the usual choice; 0.03 mm for a highly detailed "
     "miniature; 0.1 mm for a big part where time matters more."},
    {"resin.supports",
     "La pieza crece colgada de la plataforma, y cada capa nueva se pega a la anterior. Una zona que empieza en el "
     "aire, sin nada que la sostenga, como la punta del mentón de una figura, queda flotando en la resina y se "
     "pierde. Los soportes son pilares finos que bajan de esas zonas a la base y tocan la pieza con una punta "
     "chiquita.\n\nCon soportes, la pieza se imprime levantada (sobre la base y la altura de los soportes) y sale "
     "entera, pero quedan marquitas donde tocaban. Sin soportes, la pieza tiene que tener una cara plana para "
     "apoyarse directo en la plataforma.\n\nEjemplo: encendidos para figuras y formas complicadas; apagados para una "
     "placa plana, junto con la base alrededor de la pieza.",
     "The part grows hanging from the build plate, and each new layer sticks to the one before. A region that starts "
     "in mid-air with nothing holding it, like the tip of a figure's chin, ends up floating in the resin and is "
     "lost. Supports are thin pillars that run from those regions down to the base and touch the part with a tiny "
     "tip.\n\nWith supports the part is printed raised (on the base and the support height) and comes out whole, but "
     "small marks remain where they touched. Without them, the part needs a flat face to sit directly on the "
     "plate.\n\nExample: on for figures and complex shapes; off for a flat plate, together with the base around the "
     "part."},
    {"resin.support_elevation",
     "Es cuánto levantan los soportes a la pieza por encima de la base, en milímetros. Solo cuenta con los soportes "
     "encendidos.\n\nMás altura deja lugar para que los pilares lleguen bien a todos los puntos de abajo y aleja la "
     "pieza de las primeras capas, que se curan de más y quedan muy pegadas; también facilita cortar los soportes. A "
     "cambio hay más capas, más tiempo y más resina, y los pilares largos se doblan más. Con 0 la pieza queda "
     "apoyada sobre la base.\n\nEjemplo: 5 mm es un buen punto de partida; 3 mm para una pieza chica y plana; 8 a 10 "
     "mm para una figura con muchos detalles abajo.",
     "This is how far the supports lift the part above the base, in millimetres. It only matters with supports "
     "turned on.\n\nMore height leaves room for the pillars to reach every point underneath and keeps the part away "
     "from the first layers, which are over-cured and stick hard; it also makes the supports easier to cut. In "
     "exchange there are more layers, more time and more resin, and long pillars bend more. At 0 the part rests on "
     "the base.\n\nExample: 5 mm is a good start; 3 mm for a small, flat part; 8 to 10 mm for a figure with lots of "
     "detail underneath."},
    {"resin.support_density",
     "Es cuántos puntos de apoyo se ponen en las caras de la pieza que miran hacia abajo, en porcentaje. Con 100 % "
     "hay un punto cada 4 mm en cada dirección. Solo cuenta con los soportes encendidos.\n\nMás puntos sostienen "
     "mejor la pieza y hacen más difícil que una zona se despegue al subir la plataforma, pero dejan más marcas y "
     "dan más trabajo al limpiar. Menos puntos dejan la superficie más limpia, con más riesgo de que algo "
     "falle.\n\nEjemplo: 100 % para uso general; 150 a 200 % para una pieza pesada o con una cara grande mirando "
     "hacia abajo; 70 % para una pieza chica y liviana.",
     "This is how many support points go on the faces of the part that look down, in percent. At 100 % there is one "
     "point every 4 mm in each direction. It only matters with supports turned on.\n\nMore points hold the part "
     "better and make it harder for a region to tear off when the plate lifts, but they leave more marks and take "
     "more cleaning. Fewer points leave a cleaner surface, with more risk that something fails.\n\nExample: 100 % "
     "for general use; 150 to 200 % for a heavy part or one with a large face looking down; 70 % for a small, light "
     "part."},
    {"resin.support_tip_diameter",
     "Es el grosor de la punta del soporte, la parte finita que toca la pieza, en milímetros.\n\nUna punta más "
     "gruesa agarra con más firmeza y aguanta mejor el tirón de cada subida, pero deja una marca más grande y cuesta "
     "más sacarla. Una más fina deja una marquita casi invisible, aunque puede cortarse en plena impresión si la "
     "pieza pesa.\n\nEjemplo: 0,4 mm para la mayoría de las piezas; 0,3 mm para una miniatura delicada; 0,6 a 0,8 mm "
     "para una pieza grande y pesada.",
     "This is how thick the tip of the support is, the thin end that touches the part, in millimetres.\n\nA thicker "
     "tip grips more firmly and better withstands the pull of each lift, but leaves a bigger mark and is harder to "
     "remove. A thinner one leaves an almost invisible mark, though it can snap mid-print if the part is "
     "heavy.\n\nExample: 0.4 mm for most parts; 0.3 mm for a delicate miniature; 0.6 to 0.8 mm for a big, heavy "
     "part."},
    {"resin.support_tip_penetration",
     "Es cuánto se mete la punta del soporte dentro de la pieza, en milímetros, para que las dos queden bien "
     "unidas.\n\nSi entra más, la unión es más firme y la punta se suelta menos, pero al sacar el soporte queda un "
     "pocito más hondo en la superficie. Si entra poco, la marca es más chica, aunque la punta puede "
     "despegarse.\n\nEjemplo: 0,2 mm casi siempre alcanza; subilo a 0,3 o 0,4 mm si ves puntas sueltas que no "
     "llegaron a agarrar la pieza.",
     "This is how far the support tip sinks into the part, in millimetres, so the two are firmly joined.\n\nDeeper "
     "means a stronger joint and tips that let go less often, but removing the support leaves a deeper pit in the "
     "surface. Shallower leaves a smaller mark, though the tip may come loose.\n\nExample: 0.2 mm is almost always "
     "enough; raise it to 0.3 or 0.4 mm if you find loose tips that never caught the part."},
    {"resin.support_pillar_diameter",
     "Es el grosor de los pilares de los soportes, en milímetros. Al pie, donde se apoyan, se ensanchan al doble "
     "para agarrarse mejor.\n\nPilares más gruesos son más rígidos: la pieza se mueve menos en cada subida y "
     "aguantan piezas pesadas o altas. Gastan más resina y cuesta más cortarlos. Pilares finos ahorran resina y se "
     "sacan fácil, pero se doblan o se quiebran con más facilidad.\n\nEjemplo: 1 mm para uso general; 0,6 a 0,8 mm "
     "para piezas chicas; 1,5 a 2 mm para una pieza grande y pesada.",
     "This is how thick the support pillars are, in millimetres. At the foot, where they stand, they widen to twice "
     "that to hold on better.\n\nThicker pillars are stiffer: the part wobbles less on each lift and they carry "
     "heavy or tall parts. They use more resin and are harder to cut. Thin pillars save resin and come off easily, "
     "but bend or break more easily.\n\nExample: 1 mm for general use; 0.6 to 0.8 mm for small parts; 1.5 to 2 mm "
     "for a big, heavy part."},
    {"resin.support_critical_angle",
     "Los pilares de los soportes se unen entre sí con tramos inclinados, como un andamio. Este ajuste es la "
     "inclinación de esas uniones, en grados medidos desde la vertical.\n\nCon un ángulo chico las uniones son casi "
     "verticales: cada capa se apoya casi entera en la anterior y se imprimen con seguridad, pero necesitan más "
     "altura para llegar de un pilar al otro. Con un ángulo grande van más acostadas y llegan más lejos, pero "
     "cuelgan más y pueden fallar.\n\nEjemplo: 45° es el término medio; bajalo a 30 o 35° si ves uniones rotas o "
     "deformes.",
     "Support pillars are tied to each other with slanted links, like scaffolding. This setting is how steep those "
     "links are, in degrees from vertical.\n\nWith a small angle the links are nearly vertical: each layer sits "
     "almost fully on the one before and they print reliably, but they need more height to get from one pillar to "
     "the next. With a large angle they lie flatter and reach further, but they overhang more and may "
     "fail.\n\nExample: 45° is the middle ground; lower it to 30 or 35° if you find broken or misshapen links."},
    {"resin.pad",
     "La base es una placa de resina pegada a la plataforma de impresión, sobre la que se paran los soportes. Sin "
     "base, cada pilar se apoya directo en la plataforma.\n\nCon base, los soportes quedan unidos entre sí y la "
     "pieza se agarra a la plataforma por una superficie grande: se despega menos y sale de una sola vez con la "
     "espátula. Gasta algo de resina y suma capas. Ojo: si apagás los soportes y no usás la base alrededor de la "
     "pieza, la base no tiene qué sostener y el laminado se detiene con un error.\n\nEjemplo: encendida casi siempre "
     "que hay soportes.",
     "The base (or pad) is a slab of resin stuck to the build plate, which the supports stand on. Without it, each "
     "pillar stands directly on the plate.\n\nWith a base, the supports are tied together and the print grips the "
     "plate over a large area: it comes loose less often and pops off in one piece with the scraper. It uses some "
     "resin and adds layers. Note: if you turn supports off and do not use the base around the part, the base has "
     "nothing to hold and slicing stops with an error.\n\nExample: on almost every time there are supports."},
    {"resin.pad_around_object",
     "Con este ajuste encendido, la base se hace alrededor de la pieza, y la pieza queda apoyada directo en la "
     "plataforma, sin levantarse sobre soportes.\n\nSirve para piezas con una cara de abajo plana que se imprimen "
     "sin soportes: la base le agrega un borde que agranda la superficie pegada y ayuda a despegarla con la "
     "espátula. La cara de abajo de la pieza queda unida a la base y hay que separarla al terminar. Necesita la base "
     "encendida.\n\nEjemplo: una chapa o un llavero plano impreso acostado, con soportes apagados y la base "
     "alrededor de la pieza encendida.",
     "With this setting on, the base is built around the part, and the part sits directly on the build plate instead "
     "of being raised on supports.\n\nIt is meant for parts with a flat bottom printed without supports: the base "
     "adds a rim that enlarges the area stuck to the plate and helps to pry it off with the scraper. The bottom of "
     "the part is joined to the base and has to be separated afterwards. It needs the base turned on.\n\nExample: a "
     "flat tag or keychain printed lying down, with supports off and the base around the part on."},
    {"resin.pad_thickness",
     "Es el espesor de la base sobre la que se paran los soportes, en milímetros. Solo cuenta con la base "
     "encendida.\n\nUna base más gruesa es más rígida: no se dobla al sacarla con la espátula y aguanta mejor el "
     "tirón de las primeras subidas. Una más fina ahorra resina y capas, pero puede romperse o curvarse.\n\nEjemplo: "
     "2 mm para uso general; 1 a 1,5 mm para piezas chicas; 3 mm para una pieza grande y pesada.",
     "This is how thick the base under the supports is, in millimetres. It only matters with the base turned "
     "on.\n\nA thicker base is stiffer: it does not bend when you pry it off with the scraper and it takes the pull "
     "of the first lifts better. A thinner one saves resin and layers, but may break or curl.\n\nExample: 2 mm for "
     "general use; 1 to 1.5 mm for small parts; 3 mm for a big, heavy part."},
    {"resin.pad_margin",
     "Es cuánto se extiende la base más allá de lo que sostiene (los pies de los soportes o el contorno de la "
     "pieza), en milímetros. Solo cuenta con la base encendida.\n\nMás margen agranda la superficie pegada a la "
     "plataforma y deja un borde cómodo para meter la espátula, a cambio de algo más de resina y de lugar en la "
     "plataforma. Con poco margen, la base queda justa.\n\nEjemplo: 1,6 mm para uso general; 3 a 5 mm si las piezas "
     "se despegan durante la impresión.",
     "This is how far the base reaches beyond what it holds (the support feet or the part's outline), in "
     "millimetres. It only matters with the base turned on.\n\nA wider margin enlarges the area stuck to the plate "
     "and leaves a handy edge for the scraper, at the cost of a little more resin and room on the plate. With a "
     "small margin, the base is tight around its contents.\n\nExample: 1.6 mm for general use; 3 to 5 mm if prints "
     "come loose during printing."},
    {"resin.pad_wall_slope",
     "Es la inclinación del borde de la base, en grados. Con 90° el borde es vertical; con menos, la base se "
     "ensancha hacia abajo, como una rampa. Solo cuenta con la base encendida.\n\nUn borde inclinado deja un filo "
     "finito pegado a la plataforma, donde la espátula entra más fácil para despegar la impresión, y gasta un poco "
     "más de resina. Un borde vertical es más compacto.\n\nEjemplo: 90° para uso general; 45° si te cuesta meter la "
     "espátula debajo de la base.",
     "This is the slope of the base's edge, in degrees. At 90° the edge is vertical; lower values widen the base "
     "toward the bottom, like a ramp. It only matters with the base turned on.\n\nA sloped edge leaves a thin lip on "
     "the plate where the scraper slides in more easily to free the print, and uses a little more resin. A vertical "
     "edge is more compact.\n\nExample: 90° for general use; 45° if you struggle to get the scraper under the base."},
    {"resin.hollow",
     "Con este ajuste encendido, la pieza se vacía por dentro: queda una cáscara con paredes de al menos el espesor "
     "de la pared del vaciado, también arriba y abajo.\n\nUna pieza hueca gasta mucha menos resina, pesa menos y "
     "tira menos de la plataforma en cada subida. Pero un hueco cerrado se llena de resina líquida y hace de ventosa "
     "contra el fondo de la cubeta: necesita agujeros de drenaje para que la resina salga y entre aire. Una pieza "
     "que tiene que ser fuerte conviene maciza.\n\nEjemplo: vaciá una figura de 10 cm de alto; una pieza chica de "
     "unos pocos centímetros no lo necesita.",
     "With this setting on, the part is hollowed out: what remains is a shell with walls at least as thick as the "
     "hollowing wall, on the top and bottom too.\n\nA hollow part uses much less resin, weighs less and pulls less "
     "on the plate at each lift. But a closed cavity fills with liquid resin and acts as a suction cup against the "
     "bottom of the vat: it needs drain holes so the resin can get out and air can get in. A part that has to be "
     "strong is better solid.\n\nExample: hollow a 10 cm tall figure; a small part of a few centimetres does not "
     "need it."},
    {"resin.hollow_wall",
     "Es el espesor mínimo de la cáscara que queda al vaciar la pieza, en milímetros. Solo cuenta con el vaciado "
     "encendido.\n\nParedes más finas ahorran más resina, pero la pieza queda frágil y puede rajarse o deformarse. "
     "Paredes más gruesas la hacen más resistente y ahorran menos.\n\nEjemplo: 3 mm para uso general; 2 mm para una "
     "figura decorativa; 4 mm o más para una pieza que tiene que aguantar golpes.",
     "This is the minimum thickness of the shell left when the part is hollowed, in millimetres. It only matters "
     "with hollowing turned on.\n\nThinner walls save more resin, but the part becomes fragile and may crack or "
     "warp. Thicker walls make it tougher and save less.\n\nExample: 3 mm for general use; 2 mm for a decorative "
     "figure; 4 mm or more for a part that has to take knocks."},
    {"resin.hollow_closing",
     "Es cuánto se redondea el hueco de adentro al vaciar la pieza, en milímetros: el hueco no sigue los detalles "
     "más chicos que esto, y en las esquinas la pared queda más gruesa. Solo cuenta con el vaciado encendido.\n\nMás "
     "redondeo da un hueco más simple, sin rincones finitos donde la resina queda atrapada, y paredes más fuertes en "
     "las esquinas, a cambio de un poco menos de ahorro. Con 0 el hueco copia la forma de la pieza.\n\nEjemplo: 2 mm "
     "para uso general; 4 a 5 mm para una figura con dedos, orejas o detalles finos.",
     "This is how much the inner cavity is rounded when the part is hollowed, in millimetres: the cavity does not "
     "follow details smaller than this, and the wall is thicker at corners. It only matters with hollowing turned "
     "on.\n\nMore rounding gives a simpler cavity, with no thin nooks where resin gets trapped, and stronger walls "
     "at corners, at the cost of slightly less saving. At 0 the cavity follows the shape of the part.\n\nExample: 2 "
     "mm for general use; 4 to 5 mm for a figure with fingers, ears or fine details."},
    {"resin.hollow_infill",
     "Una pieza vaciada puede llevar adentro una estructura de barras que apuntala la cáscara: barras de sección "
     "cuadrada, verticales y cruzadas a 45°. Este ajuste es qué parte del hueco ocupan, en porcentaje. Solo cuenta "
     "con el vaciado encendido.\n\nMás relleno hace la pieza más rígida y sostiene los techos grandes del hueco, "
     "pero gasta más resina y hace más difícil que se escurra la resina de adentro. Con 0 el hueco queda "
     "vacío.\n\nEjemplo: 0 % para la mayoría; 20 % para una pieza grande con una cáscara fina. Con celdas de 8 mm, "
     "20 % son barras de 1,6 mm de ancho.",
     "A hollowed part can carry a structure of bars inside that props up the shell: square bars, vertical and "
     "crossed at 45°. This setting is how much of the cavity they fill, in percent. It only matters with hollowing "
     "turned on.\n\nMore infill makes the part stiffer and holds up large ceilings inside the cavity, but uses more "
     "resin and makes it harder for the resin inside to drain. At 0 the cavity is left empty.\n\nExample: 0 % for "
     "most parts; 20 % for a big part with a thin shell. With 8 mm cells, 20 % means bars 1.6 mm wide."},
    {"resin.hollow_infill_cell",
     "Es el tamaño de cada celda de la estructura de barras dentro del hueco, en milímetros. Solo cuenta con el "
     "vaciado y el relleno interior encendidos.\n\nCon el mismo porcentaje, celdas más chicas dan muchas barras "
     "finas y apretadas, que reparten mejor el apoyo pero dejan pasos estrechos para la resina; celdas más grandes "
     "dan pocas barras gruesas, con más espacio entre ellas.\n\nEjemplo: con 20 % de relleno, celdas de 8 mm dan "
     "barras de 1,6 mm; celdas de 16 mm, barras de 3,2 mm.",
     "This is the size of each cell of the bar structure inside the cavity, in millimetres. It only matters with "
     "hollowing and the inner infill turned on.\n\nAt the same percentage, smaller cells give many thin, close bars "
     "that spread the support better but leave narrow passages for the resin; larger cells give a few thick bars "
     "with more room between them.\n\nExample: with 20 % infill, 8 mm cells give 1.6 mm bars; 16 mm cells, 3.2 mm "
     "bars."},
    {"resin.exposure",
     "La exposición es el tiempo que la pantalla ilumina cada capa para endurecer la resina, en segundos. Este "
     "ajuste es para las capas normales, todas menos las primeras.\n\nCon poca exposición la resina queda blanda: "
     "las capas se separan, los detalles finos no salen o la pieza se despega de los soportes. Con demasiada, la luz "
     "cura de más: los agujeros se cierran, los detalles se empastan y la pieza sale un poco más grande. Depende de "
     "cada resina y cada impresora; la torre de exposición de las calibraciones ayuda a encontrarla.\n\nEjemplo: 2,5 "
     "s para una resina estándar; 3 a 4 s para resinas oscuras o más duras.",
     "Exposure is how long the screen lights each layer to harden the resin, in seconds. This setting is for the "
     "normal layers, all but the first ones.\n\nToo little exposure leaves the resin soft: layers separate, fine "
     "details are missing or the part tears off its supports. Too much over-cures: holes close up, details blur and "
     "the part comes out slightly larger. It depends on each resin and printer; the exposure tower among the "
     "calibrations helps you find it.\n\nExample: 2.5 s for a standard resin; 3 to 4 s for dark or tougher resins."},
    {"resin.bottom_exposure",
     "Las primeras capas son las que se pegan a la plataforma de impresión, y de ellas depende que todo lo demás "
     "quede agarrado. Este ajuste es su tiempo de exposición, en segundos, mucho más largo que el de las capas "
     "normales.\n\nCon poco tiempo, la impresión se despega y queda en el fondo de la cubeta. Con demasiado, esas "
     "capas salen gruesas y ensanchadas, y cuesta sacarlas de la plataforma.\n\nEjemplo: 30 s para una resina "
     "estándar; 20 a 25 s si cuesta despegar la impresión; 35 a 40 s si se suelta de la plataforma.",
     "The first layers are the ones that stick to the build plate, and everything else depends on them holding. This "
     "setting is their exposure time, in seconds, much longer than for normal layers.\n\nToo short, and the print "
     "comes loose and ends up at the bottom of the vat. Too long, and those layers come out thick and spread, and "
     "are hard to get off the plate.\n\nExample: 30 s for a standard resin; 20 to 25 s if prints are hard to remove; "
     "35 to 40 s if they let go of the plate."},
    {"resin.bottom_layers",
     "Es cuántas capas, desde la primera, se tratan como capas de fondo: usan la exposición de fondo, la subida de "
     "fondo y la potencia de luz de fondo. Con 0, igual la primera capa es de fondo.\n\nMás capas de fondo agarran "
     "mejor la impresión a la plataforma, pero esas capas quedan curadas de más y ensanchadas, y suman tiempo por su "
     "exposición larga. Con soportes y base, son capas de la base, así que no afectan a la pieza.\n\nEjemplo: 5 "
     "capas para uso general; 6 a 8 para piezas grandes o pesadas.",
     "This is how many layers, starting from the first, count as bottom layers: they use the bottom exposure, the "
     "bottom lift and the bottom light power. At 0, the first layer is still a bottom layer.\n\nMore bottom layers "
     "hold the print to the plate better, but those layers are over-cured and spread, and their long exposure adds "
     "time. With supports and a base, they are layers of the base, so they do not touch the part.\n\nExample: 5 "
     "layers for general use; 6 to 8 for big or heavy parts."},
    {"resin.light_pwm",
     "Es la potencia de la luz de la pantalla en las capas normales, de 1 a 255; 255 es la luz a pleno.\n\nCon menos "
     "potencia la resina recibe menos luz por segundo, así que hace falta más tiempo de exposición para curar lo "
     "mismo. Lo habitual es dejarla al máximo y ajustar el tiempo; bajala solo si tu resina pide una luz más "
     "suave.\n\nEjemplo: 255 para casi todo; 200 es cerca del 78 % de la potencia.",
     "This is the power of the screen's light on normal layers, from 1 to 255; 255 is full power.\n\nWith less power "
     "the resin gets less light per second, so it takes a longer exposure to cure the same amount. The usual "
     "approach is to leave it at maximum and adjust the time; lower it only if your resin asks for gentler "
     "light.\n\nExample: 255 for almost everything; 200 is about 78 % power."},
    {"resin.bottom_light_pwm",
     "Es la potencia de la luz de la pantalla en las capas de fondo, las primeras que se pegan a la plataforma, de 1 "
     "a 255; 255 es la luz a pleno.\n\nFunciona igual que la potencia de las capas normales: menos luz necesita más "
     "tiempo para curar lo mismo. Bajarla con una exposición larga puede dar una primera capa más pareja, pero casi "
     "siempre basta con ajustar el tiempo de fondo.\n\nEjemplo: 255 para casi todo.",
     "This is the power of the screen's light on the bottom layers, the first ones that stick to the plate, from 1 "
     "to 255; 255 is full power.\n\nIt works like the power for normal layers: less light needs more time to cure "
     "the same. Lowering it alongside a long exposure can give a more even first layer, but adjusting the bottom "
     "time is almost always enough.\n\nExample: 255 for almost everything."},
    {"resin.lift_distance",
     "Después de curar cada capa, la plataforma sube para despegar la capa del fondo transparente de la cubeta y "
     "dejar que entre resina fresca debajo; después baja la misma distancia. Este ajuste es cuánto sube en las capas "
     "normales, en milímetros.\n\nUna subida más alta despega con más seguridad y deja correr mejor la resina, pero "
     "cada capa tarda más. Una más corta ahorra tiempo, aunque la capa puede no despegarse del todo.\n\nEjemplo: 5 "
     "mm para uso general; 6 a 8 mm para piezas con mucha superficie por capa; 3 a 4 mm para piezas chicas.",
     "After each layer is cured, the build plate rises to peel the layer off the clear bottom of the vat and let "
     "fresh resin flow underneath; then it comes down the same distance. This setting is how high it rises on normal "
     "layers, in millimetres.\n\nA higher lift peels more reliably and lets the resin flow better, but each layer "
     "takes longer. A shorter one saves time, though the layer may not peel off completely.\n\nExample: 5 mm for "
     "general use; 6 to 8 mm for parts with a large area per layer; 3 to 4 mm for small parts."},
    {"resin.lift_speed",
     "Es la velocidad a la que sube la plataforma después de cada capa normal, en milímetros por segundo.\n\nMás "
     "lento despega la capa con suavidad: tira menos de los soportes y de la pieza, y falla menos. Más rápido ahorra "
     "tiempo en cada capa, pero el tirón es más fuerte y puede arrancar soportes o partes finas.\n\nEjemplo: 1,67 "
     "mm/s (unos 100 mm por minuto) para uso general; 1 mm/s para piezas delicadas o con mucha superficie por capa.",
     "This is how fast the build plate rises after each normal layer, in millimetres per second.\n\nSlower peels the "
     "layer off gently: it pulls less on the supports and the part, and fails less often. Faster saves time on each "
     "layer, but the pull is harder and can rip off supports or thin features.\n\nExample: 1.67 mm/s (about 100 mm "
     "per minute) for general use; 1 mm/s for delicate parts or a large area per layer."},
    {"resin.bottom_lift_distance",
     "Es cuánto sube la plataforma después de cada capa de fondo, en milímetros; después baja la misma distancia. "
     "Funciona como la subida de las capas normales.\n\nLas capas de fondo cubren mucha superficie y están muy "
     "curadas, así que se pegan con fuerza al fondo de la cubeta: conviene una subida algo más alta para despegarlas "
     "con seguridad. Como son pocas capas, el tiempo extra casi no se nota.\n\nEjemplo: 7 mm para uso general; 8 a "
     "10 mm si la base grande cuesta despegarse.",
     "This is how high the build plate rises after each bottom layer, in millimetres; then it comes down the same "
     "distance. It works like the lift for normal layers.\n\nBottom layers cover a large area and are heavily cured, "
     "so they stick hard to the bottom of the vat: a slightly higher lift peels them off more reliably. Since there "
     "are only a few, the extra time hardly shows.\n\nExample: 7 mm for general use; 8 to 10 mm if a large base is "
     "hard to peel off."},
    {"resin.bottom_lift_speed",
     "Es la velocidad a la que sube la plataforma después de cada capa de fondo, en milímetros por segundo.\n\nEsas "
     "capas son anchas y están muy pegadas al fondo de la cubeta; subir despacio las despega sin arrancar la "
     "impresión de la plataforma. Más rápido ahorra unos segundos, con más riesgo de que se suelte.\n\nEjemplo: 1,33 "
     "mm/s (unos 80 mm por minuto) para uso general; 1 mm/s si la impresión se despega de la plataforma en las "
     "primeras capas.",
     "This is how fast the build plate rises after each bottom layer, in millimetres per second.\n\nThose layers are "
     "wide and stick hard to the bottom of the vat; rising slowly peels them off without ripping the print from the "
     "plate. Faster saves a few seconds, with more risk of it coming loose.\n\nExample: 1.33 mm/s (about 80 mm per "
     "minute) for general use; 1 mm/s if the print lets go of the plate during the first layers."},
    {"resin.retract_speed",
     "Es la velocidad a la que baja la plataforma de vuelta a la cubeta después de cada subida, en milímetros por "
     "segundo.\n\nMás rápido ahorra tiempo en cada capa, pero la plataforma empuja la resina con más fuerza y "
     "aprieta la pieza contra el fondo de la cubeta, lo que puede mover partes finas. Más lento es más suave y tarda "
     "más.\n\nEjemplo: 2,5 mm/s (unos 150 mm por minuto) para uso general; 1,5 a 2 mm/s con resinas espesas o piezas "
     "delicadas.",
     "This is how fast the build plate comes back down into the vat after each lift, in millimetres per "
     "second.\n\nFaster saves time on each layer, but the plate pushes the resin harder and presses the part against "
     "the bottom of the vat, which can shift thin features. Slower is gentler and takes longer.\n\nExample: 2.5 mm/s "
     "(about 150 mm per minute) for general use; 1.5 to 2 mm/s with thick resins or delicate parts."},
    {"resin.wait_before_exposure",
     "Es cuánto espera la plataforma abajo, ya en su lugar, antes de prender la luz de la capa, en "
     "segundos.\n\nDespués de bajar, la resina sigue moviéndose un momento y la plataforma se acomoda. Esperar deja "
     "que todo se quede quieto, y la capa sale más nítida y pareja. Cada segundo se repite en todas las capas, así "
     "que suma mucho tiempo en total.\n\nEjemplo: 2,5 s para uso general; 1 s para piezas chicas con resina fluida; "
     "3 a 4 s con resinas espesas.",
     "This is how long the build plate waits at the bottom, in position, before the layer's light turns on, in "
     "seconds.\n\nAfter lowering, the resin keeps moving for a moment and the plate settles. Waiting lets everything "
     "come to rest, and the layer comes out sharper and more even. Each second repeats on every layer, so it adds up "
     "to a lot of time overall.\n\nExample: 2.5 s for general use; 1 s for small parts in a runny resin; 3 to 4 s "
     "with thick resins."},
    {"resin.wait_after_cure",
     "Es cuánto espera la plataforma después de apagar la luz y antes de subir, en segundos.\n\nUna pausa corta deja "
     "que la capa recién curada termine de asentarse antes de despegarla del fondo de la cubeta, y la subida tira "
     "menos de ella. Cada segundo se repite en todas las capas.\n\nEjemplo: 1 s para uso general; 0 si querés ganar "
     "tiempo con una resina que se despega bien; 2 s si se rompen detalles finos.",
     "This is how long the build plate waits after the light turns off and before it lifts, in seconds.\n\nA short "
     "pause lets the freshly cured layer settle before it is peeled off the bottom of the vat, and the lift pulls on "
     "it less. Each second repeats on every layer.\n\nExample: 1 s for general use; 0 to save time with a resin that "
     "peels off well; 2 s if fine details break."},
    {"resin.wait_after_lift",
     "Es cuánto espera la plataforma arriba, al final de la subida, antes de volver a bajar, en segundos.\n\nLa "
     "pausa deja que la resina termine de correr y cubra el fondo de la cubeta bajo la pieza, y que la pieza deje de "
     "gotear y de balancearse. Sin pausa se gana tiempo. Cada segundo se repite en todas las capas.\n\nEjemplo: 0 s "
     "para uso general; 1 a 2 s con resinas espesas o piezas con mucha superficie por capa.",
     "This is how long the build plate waits at the top of the lift before coming back down, in seconds.\n\nThe "
     "pause lets the resin finish flowing back over the bottom of the vat under the part, and lets the part stop "
     "dripping and swaying. No pause saves time. Each second repeats on every layer.\n\nExample: 0 s for general "
     "use; 1 to 2 s with thick resins or parts with a large area per layer."},
    {"resin.elephant_foot",
     "Las primeras capas se exponen mucho tiempo, y la luz de más hace que salgan más anchas de lo que deberían: la "
     "base de la pieza queda con un borde que sobresale, el «pie de elefante». Este ajuste achica la primera capa en "
     "esa medida, en milímetros, y cada capa de fondo siguiente un poco menos, hasta llegar a las normales.\n\nCon "
     "más corrección la base queda derecha; con demasiada, queda una muesca hacia adentro. Si la pieza va sobre "
     "soportes y base, las capas de fondo son de la base y esto casi no importa.\n\nEjemplo: 0,2 mm para uso "
     "general; 0,3 a 0,4 mm si todavía ves el borde.",
     "The first layers are exposed for a long time, and the extra light makes them come out wider than they should: "
     "the bottom of the part gets a rim that sticks out, the elephant's foot. This setting shrinks the first layer "
     "by that much, in millimetres, and each following bottom layer a little less, until the normal layers.\n\nMore "
     "correction keeps the bottom straight; too much leaves an inward notch. If the part sits on supports and a "
     "base, the bottom layers belong to the base and this hardly matters.\n\nExample: 0.2 mm for general use; 0.3 to "
     "0.4 mm if you still see the rim."},
    {"resin.scale_x",
     "La resina se encoge un poco al curarse, y la pieza puede salir algo más chica que el modelo. Este ajuste "
     "escala la pieza en X (el ancho) antes de cortarla, para compensarlo; 1 la deja igual. Se escala desde el "
     "centro de la pieza.\n\nMás de 1 agranda, menos de 1 achica. Tocalo solo si medís piezas impresas y no "
     "coinciden; el bloque de medidas de las calibraciones sirve para eso.\n\nEjemplo: un cubo de 20 mm que mide "
     "19,9 mm de ancho pide 1,005 (20 dividido 19,9).",
     "Resin shrinks a little as it cures, and the part may come out slightly smaller than the model. This setting "
     "scales the part along X (width) before slicing, to make up for it; 1 leaves it as is. It scales from the "
     "centre of the part.\n\nAbove 1 enlarges, below 1 shrinks. Only touch it if you measure printed parts and they "
     "do not match; the measuring block among the calibrations is made for that.\n\nExample: a 20 mm cube that "
     "measures 19.9 mm wide calls for 1.005 (20 divided by 19.9)."},
    {"resin.scale_y",
     "La resina se encoge un poco al curarse, y la pieza puede salir algo más chica que el modelo. Este ajuste "
     "escala la pieza en Y (el fondo) antes de cortarla, para compensarlo; 1 la deja igual. Se escala desde el "
     "centro de la pieza.\n\nMás de 1 agranda, menos de 1 achica. Tocalo solo si medís piezas impresas y no "
     "coinciden; el bloque de medidas de las calibraciones sirve para eso.\n\nEjemplo: un cubo de 20 mm que mide "
     "19,9 mm de fondo pide 1,005 (20 dividido 19,9).",
     "Resin shrinks a little as it cures, and the part may come out slightly smaller than the model. This setting "
     "scales the part along Y (depth) before slicing, to make up for it; 1 leaves it as is. It scales from the "
     "centre of the part.\n\nAbove 1 enlarges, below 1 shrinks. Only touch it if you measure printed parts and they "
     "do not match; the measuring block among the calibrations is made for that.\n\nExample: a 20 mm cube that "
     "measures 19.9 mm deep calls for 1.005 (20 divided by 19.9)."},
    {"resin.scale_z",
     "La resina se encoge un poco al curarse, y la pieza puede salir algo más chica que el modelo. Este ajuste "
     "escala la pieza en Z (la altura) antes de cortarla, para compensarlo; 1 la deja igual. Se escala desde la base "
     "de la pieza.\n\nMás de 1 agranda, menos de 1 achica. Tocalo solo si medís piezas impresas y no coinciden; el "
     "bloque de medidas de las calibraciones sirve para eso.\n\nEjemplo: un cubo de 20 mm que mide 19,9 mm de alto "
     "pide 1,005 (20 dividido 19,9).",
     "Resin shrinks a little as it cures, and the part may come out slightly smaller than the model. This setting "
     "scales the part along Z (height) before slicing, to make up for it; 1 leaves it as is. It scales from the "
     "bottom of the part.\n\nAbove 1 enlarges, below 1 shrinks. Only touch it if you measure printed parts and they "
     "do not match; the measuring block among the calibrations is made for that.\n\nExample: a 20 mm cube that "
     "measures 19.9 mm tall calls for 1.005 (20 divided by 19.9)."},
    {"resin.absolute_correction",
     "Agranda (con un valor positivo) o achica (con uno negativo) cada capa de la pieza en una medida fija, en "
     "milímetros, hacia todos los costados.\n\nA diferencia de la escala, corrige lo mismo en una pieza chica que en "
     "una grande: sirve para encastres, agujeros y pernos que salen justos o flojos. Agrandar la pieza también "
     "achica sus agujeros, y achicarla los agranda; cada agujero cambia el doble del valor, porque se corre de los "
     "dos lados.\n\nEjemplo: 0 si las medidas salen bien; −0,05 mm si los agujeros salen 0,1 mm más chicos de lo que "
     "deberían.",
     "Grows (with a positive value) or shrinks (with a negative one) every layer of the part by a fixed amount, in "
     "millimetres, on every side.\n\nUnlike scaling, it corrects a small part as much as a big one: it is for fits, "
     "holes and pins that come out too tight or too loose. Growing the part also makes its holes smaller, and "
     "shrinking it makes them bigger; each hole changes by twice the value, because it moves on both "
     "sides.\n\nExample: 0 if sizes come out right; −0.05 mm if holes come out 0.1 mm smaller than they should."},
    {"resin.z_correction_layers",
     "La luz no se queda solo en la capa que cura: atraviesa un poco y endurece también algo de resina más allá. "
     "Donde la pieza tiene algo debajo no se nota, pero en una parte que cuelga, como un techo o un voladizo, esa "
     "resina de más se pega abajo y la parte sale más gruesa de lo que debería. Con este ajuste, las primeras capas "
     "de lo que cuelga no se iluminan: la luz de las capas siguientes alcanza a curarlas igual.\n\nMás capas "
     "corrigen más; demasiadas dejan los techos más finos o con agujeros.\n\nEjemplo: 0 lo apaga; 2 a 4 capas de "
     "0,05 mm si los techos o los textos en relieve hacia abajo salen gruesos.",
     "Light does not stay in the layer it cures: it passes through a little and hardens some resin beyond. Where the "
     "part has something underneath this does not show, but on a part that hangs, like a ceiling or an overhang, "
     "that extra resin sticks below and the part comes out thicker than it should. With this setting, the first "
     "layers of whatever hangs are not lit: the light of the following layers cures them anyway.\n\nMore layers "
     "correct more; too many leave ceilings thinner or with holes.\n\nExample: 0 turns it off; 2 to 4 layers of 0.05 "
     "mm if ceilings or downward-facing lettering come out thick."},
    {"resin.gamma",
     "La pantalla está hecha de cuadraditos de luz, los píxeles, y un borde curvo con píxeles enteros quedaría en "
     "escalera. Por eso los píxeles del borde se pintan en gris según cuánto los cubre la capa: curan a medias y "
     "suavizan los escalones (el suavizado). Este ajuste cambia esos grises.\n\nCon 1 quedan como salen. Más alto "
     "los oscurece: los bordes curan menos y quedan más finos y definidos. Con 0 se quita el suavizado: cada píxel "
     "queda blanco si la capa cubre la mitad o más, y si no, negro.\n\nEjemplo: 1 para uso general; 1,5 a 2 si los "
     "bordes salen engordados o borrosos; 0 para bordes en blanco y negro.",
     "The screen is made of tiny squares of light, the pixels, and a curved edge drawn with whole pixels would look "
     "like a staircase. So the edge pixels are painted grey according to how much of them the layer covers: they "
     "cure partly and soften the steps (anti-aliasing). This setting changes those greys.\n\nAt 1 they stay as they "
     "come. Higher darkens them: the edges cure less and come out thinner and crisper. At 0 the smoothing is "
     "removed: each pixel is white if the layer covers half of it or more, and black otherwise.\n\nExample: 1 for "
     "general use; 1.5 to 2 if edges come out swollen or blurry; 0 for pure black-and-white edges."},
    {"resin.antialiasing",
     "Es el nivel de suavizado de bordes que se anota en el archivo que lee la impresora, de 1 a 16.\n\nEs solo un "
     "dato de la cabecera: este programa siempre pinta los bordes con el suavizado exacto, según cuánto cubre la "
     "capa a cada píxel, sea cual sea este número. Para cambiar cómo se ven los bordes de verdad, usá "
     "gamma.\n\nEjemplo: dejalo en 8.",
     "This is the edge-smoothing (anti-aliasing) level written into the file the printer reads, from 1 to 16.\n\nIt "
     "is only a header value: this program always paints edges with exact smoothing, based on how much of each pixel "
     "the layer covers, whatever this number says. To actually change how the edges look, use gamma.\n\nExample: "
     "leave it at 8."},
    {"resin.density",
     "Es cuánto pesa un mililitro de la resina, en gramos por mililitro. El programa sabe cuántos mililitros gasta "
     "la impresión y con este número calcula cuánto pesa.\n\nNo cambia la impresión, solo el peso que se muestra. La "
     "interfaz también lo puede sacar del peso y el volumen del frasco.\n\nEjemplo: 1 g/ml da un número redondo; las "
     "resinas comunes andan entre 1,05 y 1,2 g/ml, y el dato exacto suele estar en el frasco o en su ficha.",
     "This is how much one millilitre of the resin weighs, in grams per millilitre. The program knows how many "
     "millilitres the print uses, and with this number it works out the weight.\n\nIt does not change the print, "
     "only the weight shown. The interface can also derive it from the bottle's weight and volume.\n\nExample: 1 "
     "g/ml gives a round number; common resins are between 1.05 and 1.2 g/ml, and the exact figure is usually on the "
     "bottle or its data sheet."},
    {"resin.bottle_cost",
     "Es lo que te costó el frasco de resina, en la moneda que uses. Con él se calcula el costo de la impresión: los "
     "mililitros que gasta, por el costo del frasco, dividido por los mililitros del frasco. El resultado se anota "
     "en el archivo.\n\nNo cambia la impresión. Con 0 no se calcula costo.\n\nEjemplo: un frasco de 1000 ml que "
     "costó 30; una impresión que gasta 50 ml cuesta 1,5.",
     "This is what you paid for the bottle of resin, in whatever currency you use. It gives the cost of the print: "
     "the millilitres it uses, times the bottle's cost, divided by the bottle's millilitres. The result is written "
     "into the file.\n\nIt does not change the print. At 0 no cost is worked out.\n\nExample: a 1000 ml bottle that "
     "cost 30; a print using 50 ml costs 1.5."},
    {"resin.bottle_volume",
     "Es cuánta resina trae el frasco, en mililitros. Se usa con el costo del frasco para calcular cuánto cuesta "
     "cada impresión, y con el peso del frasco para sacar la densidad.\n\nNo cambia la impresión. Poné lo que dice "
     "la etiqueta.\n\nEjemplo: 1000 ml para un frasco de un litro; 500 ml para uno de medio litro.",
     "This is how much resin the bottle holds, in millilitres. It is used with the bottle's cost to work out what "
     "each print costs, and with the bottle's weight to get the density.\n\nIt does not change the print. Enter what "
     "the label says.\n\nExample: 1000 ml for a one-litre bottle; 500 ml for a half-litre one."},
    {"resin.bottle_weight",
     "Es cuánto pesa la resina del frasco, en kilos. La interfaz lo divide por el volumen del frasco para sacar la "
     "densidad de la resina, que sirve para calcular el peso de cada impresión.\n\nNo cambia la impresión. Poné lo "
     "que dice la etiqueta, sin contar el envase si podés.\n\nEjemplo: 1 kg en un frasco de 1000 ml da 1 g/ml; 1,1 "
     "kg en 1000 ml da 1,1 g/ml.",
     "This is how much the resin in the bottle weighs, in kilograms. The interface divides it by the bottle's volume "
     "to get the resin's density, which is used to work out the weight of each print.\n\nIt does not change the "
     "print. Enter what the label says, leaving out the container if you can.\n\nExample: 1 kg in a 1000 ml bottle "
     "gives 1 g/ml; 1.1 kg in 1000 ml gives 1.1 g/ml."},
    {"resin.profile_name",
     "Es el nombre del perfil de resina que se anota en la cabecera del archivo que lee la impresora. Puede quedar "
     "vacío.\n\nNo cambia la impresión: sirve para reconocer después con qué resina y qué ajustes se preparó el "
     "archivo. Entran hasta 31 bytes, y las letras con tilde ocupan dos.\n\nEjemplo: «Gris estandar 2,5s».",
     "This is the name of the resin profile written into the header of the file the printer reads. It can be left "
     "empty.\n\nIt does not change the print: it helps you tell later which resin and settings the file was made "
     "for. It holds up to 31 bytes, and accented letters take two.\n\nExample: Grey standard 2.5s."},
    {"resin.printer_name",
     "Es el modelo de impresora que se anota en la cabecera del archivo que lee la impresora.\n\nNo cambia las "
     "capas: es un dato para identificar para qué máquina se preparó el archivo. Conviene que coincida con tu "
     "impresora. Entran hasta 31 bytes.\n\nEjemplo: «MARS 5» para una Elegoo Mars 5, que es la impresora de los "
     "valores por defecto.",
     "This is the printer model written into the header of the file the printer reads.\n\nIt does not change the "
     "layers: it records which machine the file was prepared for. It is best to make it match your printer. It holds "
     "up to 31 bytes.\n\nExample: MARS 5 for an Elegoo Mars 5, the printer the defaults are for."},
