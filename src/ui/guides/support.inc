// Explanations of the support settings for setting_guide.cpp: {key, Spanish, English},
// one entry per setting. Own texts; see PROVENANCE.md.
    {"support.buildplate_only",
     "Con este ajuste encendido, el soporte solo puede pararse sobre la cama. Si una zona de soporte tendría que "
     "apoyarse en algún momento sobre la propia pieza, se quita entera, también la parte que tocaba la cama.\n\nAsí no "
     "quedan marcas de soporte sobre la superficie de la pieza, pero algunas partes que cuelgan pueden quedar sin nada "
     "abajo y salir feas. Solo vale con los soportes encendidos y el estilo en grilla.\n\nEjemplo: apagado para la "
     "mayoría; encendido en una figura cuyas caras querés impecables, revisando en la vista previa qué quedó sin "
     "soporte.",
     "With this on, support may only stand on the bed. If a support area would have to rest on the part itself at some "
     "point, the whole area is removed, including the bit that touched the bed.\n\nThis keeps support marks off the "
     "part's surface, but some overhangs may end up with nothing under them and come out rough. It only works with "
     "supports on and the grid style.\n\nExample: off for most prints; on for a figure whose surfaces you want "
     "spotless, checking in the preview what lost its support."},
    {"support.under_bridges",
     "Un puente es algo que cuelga pero está sostenido en los dos extremos, como el dintel de una puerta o la tapa de "
     "una caja: la impresora tiende el plástico en el aire de una punta a la otra. Con este ajuste apagado, los "
     "puentes no llevan soporte, sean largos o cortos; lo que cuelga de un solo lado sí lo lleva.\n\nEncendido, "
     "también se sostienen los puentes: salen más prolijos por abajo, pero gastan más y hay más para sacar. Necesita "
     "los soportes encendidos.\n\nEjemplo: apagado si los puentes miden pocos centímetros; encendido para un techo muy "
     "ancho que se te cae.",
     "A bridge is something that overhangs but is held at both ends, like the top of a doorway or the lid of a box: "
     "the printer stretches plastic across the air from one end to the other. With this off, bridges get no support, "
     "however long they are; anything held on one side only still does.\n\nOn, bridges are supported too: their "
     "underside comes out neater, but it uses more plastic and there is more to remove. It needs supports "
     "on.\n\nExample: off if your bridges span a few centimetres; on for a very wide ceiling that keeps sagging."},
    {"support.style",
     "Es la forma del soporte. En grilla son columnas rectas debajo de lo que cuelga, con un margen de 1,6 mm "
     "alrededor. Ajustado sigue el contorno de lo que cuelga, sin margen. Orgánico son soportes en árbol: puntas finas "
     "bajo la pieza que bajan como ramas, se juntan y terminan en troncos sobre la cama, esquivando la pieza.\n\nLa "
     "grilla es simple y firme; el ajustado gasta menos; los árboles tocan la pieza en menos lugares y se sacan más "
     "fácil en figuras con muchos salientes. Necesita los soportes encendidos.\n\nEjemplo: grilla para piezas de "
     "formas rectas; orgánico para una estatuilla con brazos y capa.",
     "This is the shape of the support. Grid makes straight columns under the overhang, with a 1.6 mm margin around "
     "it. Snug follows the outline of the overhang with no margin. Organic makes tree supports: thin tips under the "
     "part that run down like branches, merge, and end in trunks on the bed, steering around the part.\n\nGrid is "
     "simple and sturdy; snug uses less plastic; trees touch the part in fewer places and come off more easily on "
     "models with many overhangs. It needs supports on.\n\nExample: grid for boxy parts; organic for a statuette with "
     "arms and a cape."},
    {"support.threshold_angle",
     "Decide qué tan inclinada tiene que estar una cara para llevar soporte. El ángulo se mide desde la horizontal: "
     "una cara más tendida que este ángulo lleva soporte. En 0 decide el programa: cada capa puede sobresalir hasta "
     "medio cordón (media línea de plástico) más que la de abajo.\n\nUn ángulo más alto pone soporte también en caras "
     "más empinadas: más apoyo, más material y más marcas. Más bajo, solo lo llevan las caras casi planas. Necesita "
     "los soportes encendidos y la búsqueda automática.\n\nEjemplo: dejalo en 0 para empezar; probá 45° si ves caras "
     "inclinadas que salen feas sin soporte.",
     "This decides how slanted a face must be to get support. The angle is measured from the horizontal: a face "
     "flatter than this angle gets support. At 0 the program decides: each layer may stick out up to half a line of "
     "plastic beyond the one below.\n\nA higher angle also supports steeper faces: more holding, more plastic and more "
     "marks. A lower one supports only faces that are nearly flat. It needs supports on and automatic "
     "placement.\n\nExample: leave it at 0 to start; try 45° if slanted faces come out rough without support."},
    {"support.enforce_layers",
     "Fuerza soporte debajo de todo lo que cuelga en las primeras capas de la pieza, sin mirar el ángulo y aunque los "
     "soportes estén apagados. El número es cuántas capas, contando desde abajo.\n\nAyuda a que una pieza chica, o con "
     "detalles finos cerca de la cama, arranque firme. Ese soporte es solo base, sin la interfaz densa que toca la "
     "pieza, y llega hasta cuatro capas antes del número que pongas: por eso con menos de 4 no hace nada. 0 lo "
     "apaga.\n\nEjemplo: 10 capas para una pieza con un saliente bajo, a pocos milímetros de la cama.",
     "This forces support under everything that overhangs in the first layers of the part, whatever the angle and even "
     "with supports off. The number is how many layers, counting from the bottom.\n\nIt helps a small part, or one "
     "with fine details near the bed, start out solid. That support is base only, without the dense interface that "
     "touches the part, and it stops four layers short of the number you set, so anything below 4 does nothing. 0 "
     "turns it off.\n\nExample: 10 layers for a part with a low ledge a few millimetres above the bed."},
    {"support.free_bridge_length",
     "Es cuánto puede tender tu impresora en el aire de un apoyo a otro, en milímetros. No agrega soporte: el programa "
     "revisa los puentes (partes que cuelgan sostenidas en los dos extremos) y, si un tramo sin nada debajo es más "
     "largo que esto, te avisa.\n\nSubilo si tu impresora tiende puentes largos sin problema y los avisos sobran; "
     "bajalo si hasta los puentes cortos se te caen. 0 apaga la revisión.\n\nEjemplo: 16 mm por defecto; 30 mm para "
     "una impresora con buen ventilador que hace puentes largos.",
     "This is how far your printer can stretch plastic through the air between two supports, in millimetres. It adds "
     "no support: the program checks the bridges (overhangs held at both ends) and warns you when a stretch with "
     "nothing underneath is longer than this.\n\nRaise it if your printer bridges long gaps fine and the warnings are "
     "noise; lower it if even short bridges sag. 0 turns the check off.\n\nExample: 16 mm by default; 30 mm for a "
     "printer with a strong fan that handles long bridges."},
    {"support.tree_branch_diameter",
     "Con soportes en árbol (estilo orgánico), es el grosor de las ramas más finas, en milímetros.\n\nRamas más "
     "gruesas son más firmes y no se doblan ni se quiebran mientras se imprimen, pero gastan más y necesitan más lugar "
     "para esquivar la pieza. Más finas son más livianas y se sacan más fácil, pero pueden tambalear en soportes "
     "altos.\n\nEjemplo: 2 mm va bien con boquilla de 0,4 mm; 3 mm para soportes muy altos.",
     "With tree supports (organic style), this is the thickness of the thinnest branches, in millimetres.\n\nThicker "
     "branches are sturdier and do not bend or snap while printing, but they use more plastic and need more room to "
     "get around the part. Thinner ones are lighter and come off more easily, but they can wobble when the support is "
     "tall.\n\nExample: 2 mm suits a 0.4 mm nozzle; 3 mm for very tall supports."},
    {"support.tree_tip_diameter",
     "Con soportes en árbol, es el grosor de las puntas, la parte de cada rama que llega a la pieza, en "
     "milímetros.\n\nPuntas más finas tocan menos la pieza: dejan marcas más chicas y se despegan más fácil, pero "
     "sostienen menos. Más gruesas sostienen mejor y dejan más marca.\n\nEjemplo: 0,8 mm por defecto; 1,2 mm si las "
     "puntas se rompen o la pieza es pesada.",
     "With tree supports, this is the thickness of the tips, the end of each branch that reaches the part, in "
     "millimetres.\n\nThinner tips touch the part less: they leave smaller marks and pop off more easily, but they "
     "hold less. Thicker ones hold better and leave bigger marks.\n\nExample: 0.8 mm by default; 1.2 mm if the tips "
     "break or the part is heavy."},
    {"support.tree_top_rate",
     "Con soportes en árbol, dice qué tan juntas van las puntas debajo de la pieza, en porcentaje.\n\nUn valor más "
     "alto pone más puntas bajo lo que cuelga: la cara de abajo sale más pareja, pero hay más contacto y más trabajo "
     "para sacar el soporte. Más bajo, menos puntas y menos marcas, con zonas que pueden quedar flojas entre punta y "
     "punta.\n\nEjemplo: 15 % por defecto; 25 a 30 % bajo una superficie ancha y plana que cuelga.",
     "With tree supports, this sets how closely the tips are packed under the part, in percent.\n\nA higher value puts "
     "more tips under the overhang: its underside comes out smoother, but there is more contact and more work to "
     "remove the support. A lower one means fewer tips and fewer marks, with spots that may sag between "
     "tips.\n\nExample: 15 % by default; 25 to 30 % under a wide, flat overhanging surface."},
    {"support.tree_branch_distance",
     "Con soportes en árbol, es cuánto se separan las puntas entre sí, como un factor: 1 es la separación normal y 2 "
     "la duplica. Nunca quedan más juntas que su propio ancho ni que dos cordones (dos líneas de plástico).\n\nMás "
     "separación da menos ramas: ahorra material y tiempo, y el soporte se saca más fácil, pero lo que cuelga se apoya "
     "en menos puntos. Menos separación lo sostiene mejor.\n\nEjemplo: 1 para empezar; 1,5 a 2 en piezas grandes donde "
     "querés ahorrar plástico.",
     "With tree supports, this is how far apart the tips are, as a factor: 1 is the normal spacing and 2 doubles it. "
     "They are never closer than their own width or two lines of plastic.\n\nMore spacing means fewer branches: it "
     "saves plastic and time, and the support comes off more easily, but the overhang rests on fewer points. Less "
     "spacing holds it better.\n\nExample: 1 to start; 1.5 to 2 on large parts where you want to save plastic."},
    {"support.tree_angle_preferred",
     "Con soportes en árbol, es la inclinación con que una rama se va hacia otra para juntarse, en grados desde la "
     "vertical.\n\nUn ángulo más grande deja que las ramas se junten antes, con menos troncos y menos material, pero "
     "las ramas más acostadas son menos firmes. Más chico da ramas más derechas y más troncos separados.\n\nEjemplo: "
     "25° por defecto; 15° si las ramas se doblan o se caen al imprimirse.",
     "With tree supports, this is how much a branch leans when it heads toward another one to merge, in degrees from "
     "vertical.\n\nA larger angle lets branches merge sooner, giving fewer trunks and using less plastic, but leaning "
     "branches are less stable. A smaller one gives straighter branches and more separate trunks.\n\nExample: 25° by "
     "default; 15° if branches bend or fall over while printing."},
    {"support.tree_angle",
     "Con soportes en árbol, es lo más que puede inclinarse una rama para esquivar la pieza, en grados desde la "
     "vertical. 0 es una rama derecha.\n\nCon más ángulo las ramas rodean mejor la pieza y llegan a la cama en vez de "
     "apoyarse sobre ella, pero una rama muy acostada es más floja y puede salir mal. Con menos son más firmes, pero "
     "esquivan menos y más seguido terminan apoyadas sobre la pieza.\n\nEjemplo: 40° por defecto; 50° para una figura "
     "con muchos salientes que no querés marcar.",
     "With tree supports, this is the most a branch may lean to get around the part, in degrees from vertical. 0 is a "
     "straight branch.\n\nA larger angle lets branches curve around the part and reach the bed instead of landing on "
     "it, but a branch that leans a lot is weaker and may print badly. A smaller one gives sturdier branches that "
     "dodge less and more often end up resting on the part.\n\nExample: 40° by default; 50° for a figure with many "
     "overhangs you do not want marked."},
    {"support.tree_branch_diameter_angle",
     "Con soportes en árbol, es cuánto engordan las ramas a medida que bajan, en grados. 0 las deja todas del mismo "
     "grosor.\n\nMás ángulo da troncos anchos abajo: el árbol es más estable y aguanta mejor en piezas altas, pero "
     "gasta más plástico. Menos ángulo ahorra material, con ramas finas que pueden tambalear.\n\nEjemplo: 5° por "
     "defecto; 8 a 10° para soportes de más de 10 cm de alto.",
     "With tree supports, this is how much the branches widen on their way down, in degrees. 0 keeps them all the same "
     "thickness.\n\nA larger angle gives wide trunks at the bottom: the tree is steadier and holds up better on tall "
     "parts, but uses more plastic. A smaller one saves material, with thin branches that may wobble.\n\nExample: 5° "
     "by default; 8 to 10° for supports taller than 10 cm."},
    {"support.tree_double_wall_diameter",
     "Cada rama de un soporte en árbol tiene una pared, el contorno de afuera, y relleno adentro. Este ajuste dice "
     "desde qué grosor una rama lleva dos paredes en vez de una, en milímetros. 0: nunca dos.\n\nDos paredes hacen más "
     "fuertes las ramas gruesas, a cambio de más plástico y tiempo. Un valor más alto deja más ramas con una sola "
     "pared.\n\nEjemplo: 3 mm por defecto; 0 para ahorrar en una pieza chica y liviana.",
     "Each branch of a tree support has a wall, its outer outline, with infill inside. This setting is the thickness, "
     "in millimetres, above which a branch gets two walls instead of one. 0: never two.\n\nTwo walls make thick "
     "branches stronger, at the cost of more plastic and time. A higher value leaves more branches with a single "
     "wall.\n\nExample: 3 mm by default; 0 to save plastic on a small, light part."},
    {"support.z_gap",
     "Es el espacio vertical entre la parte de arriba del soporte y la pieza que sostiene, en milímetros. Se redondea "
     "a capas enteras.\n\nMás espacio hace que el soporte se despegue fácil y deje menos marca, pero la cara de abajo "
     "de la pieza sale más caída y rugosa. Menos espacio da una cara más prolija, pero el soporte se pega y cuesta "
     "sacarlo. Necesita los soportes encendidos.\n\nEjemplo: 0,2 mm, una capa de 0,2 mm, para empezar; 0,4 mm, dos "
     "capas, si el soporte queda muy pegado.",
     "This is the vertical gap between the top of the support and the part it holds, in millimetres. It is rounded to "
     "whole layers.\n\nA bigger gap lets the support pop off easily and leaves fewer marks, but the underside of the "
     "part droops and comes out rough. A smaller gap gives a neater underside, but the support sticks and is hard to "
     "remove. It needs supports on.\n\nExample: 0.2 mm, one 0.2 mm layer, to start; 0.4 mm, two layers, if the support "
     "is stuck too hard."},
    {"support.bottom_z_gap",
     "Cuando el soporte no sale de la cama sino que se apoya sobre la propia pieza, este es el espacio vertical entre "
     "la pieza y el pie del soporte, en milímetros. Se redondea a capas enteras. En 0 usa la misma separación que "
     "arriba del soporte.\n\nMás espacio deja más limpia la pieza donde se apoya el soporte; menos lo hace más firme. "
     "Solo vale con el estilo en grilla: los árboles ya dejan la separación de arriba.\n\nEjemplo: 0 para la mayoría; "
     "0,4 mm si el pie del soporte queda soldado a la pieza.",
     "When support rests on the part itself instead of the bed, this is the vertical gap between the part and the foot "
     "of the support, in millimetres. It is rounded to whole layers. At 0 it uses the same gap as the top of the "
     "support.\n\nA bigger gap leaves the part cleaner where the support stands; a smaller one makes the support "
     "steadier. It only applies to the grid style: tree supports already leave the top gap.\n\nExample: 0 for most "
     "prints; 0.4 mm if the foot of the support fuses to the part."},
    {"support.xy_gap",
     "Es la separación de costado entre el soporte y las paredes de la pieza. Se pone en milímetros o en porcentaje "
     "del ancho de las paredes: 50 % es media línea.\n\nMás separación evita que el soporte se pegue a los costados y "
     "deje marcas, pero sostiene menos los bordes de lo que cuelga. Menos separación sostiene mejor y se pega más. "
     "Necesita los soportes encendidos.\n\nEjemplo: 50 % por defecto, unos 0,22 mm con paredes de 0,45 mm; 0,8 mm si "
     "el soporte se pega a los costados.",
     "This is the sideways gap between the support and the walls of the part. You can set it in millimetres or as a "
     "percentage of the wall width: 50 % is half a line.\n\nA bigger gap stops the support sticking to the sides and "
     "leaving marks, but holds the edges of the overhang less. A smaller gap holds better and sticks more. It needs "
     "supports on.\n\nExample: 50 % by default, about 0.22 mm with 0.45 mm walls; 0.8 mm if the support sticks to the "
     "sides."},
    {"support.interface_layers",
     "La interfaz son capas densas, con las líneas juntas, en la parte de arriba del soporte, justo debajo de la "
     "pieza: un piso parejo donde se apoya lo que cuelga. Este ajuste es cuántas capas de interfaz hay. 0 no pone "
     "ninguna.\n\nMás capas dan una cara de abajo más lisa, pero gastan más y el soporte cuesta más de sacar. Menos "
     "capas ahorran y dejan una cara más rugosa. Necesita los soportes encendidos.\n\nEjemplo: 3 capas por defecto; 2 "
     "en piezas donde la cara de abajo no se ve.",
     "The interface is a few dense layers, with the lines packed close, at the top of the support right under the "
     "part: an even floor for the overhang to rest on. This setting is how many interface layers there are. 0 means "
     "none.\n\nMore layers give a smoother underside, but use more plastic and make the support harder to remove. "
     "Fewer layers save plastic and leave a rougher underside. It needs supports on.\n\nExample: 3 layers by default; "
     "2 on parts whose underside is never seen."},
    {"support.bottom_interface_layers",
     "Cuando el soporte se apoya sobre la pieza en vez de sobre la cama, su pie también puede llevar capas densas de "
     "interfaz, para no dejar marcas feas ahí. Este ajuste es cuántas. En −1 usa las mismas que la interfaz de arriba; "
     "0 no pone ninguna.\n\nMás capas reparten mejor el apoyo y cuidan la superficie de la pieza; menos ahorran "
     "material. Solo vale con el estilo en grilla.\n\nEjemplo: −1 para la mayoría; 0 si la zona donde se apoya no se "
     "ve.",
     "When support stands on the part instead of the bed, its foot can also have dense interface layers so it does not "
     "leave ugly marks there. This setting is how many. At −1 it uses the same number as the top interface; 0 means "
     "none.\n\nMore layers spread the load and protect the part's surface; fewer save plastic. It only applies to the "
     "grid style.\n\nExample: −1 for most prints; 0 if the area it stands on is never seen."},
    {"support.synchronize_layers",
     "Pide que el soporte se imprima en las mismas capas que la pieza, en vez de en capas propias más gruesas.\n\nEste "
     "motor ya imprime siempre el soporte en las capas de la pieza, así que hoy el ajuste se guarda pero no cambia "
     "nada: encendido o apagado, el resultado es el mismo.\n\nEjemplo: dejalo apagado, como viene.",
     "This asks for the support to be printed on the same layers as the part, rather than on thicker layers of its "
     "own.\n\nThis engine always prints support on the part's layers, so for now the setting is saved but changes "
     "nothing: on or off, the result is the same.\n\nExample: leave it off, as it comes."},
    {"support.interface_pattern",
     "Es el dibujo de las capas densas de interfaz, las que tocan la pieza. Líneas las pone rectas, cruzadas a 90° con "
     "las de la base del soporte. Concéntrico hace vueltas que siguen el borde de la zona hacia adentro. Automático "
     "hoy da líneas, mientras no haya material soluble.\n\nLas líneas son rápidas y se despegan en tiras; las vueltas "
     "concéntricas siguen la forma de lo que cuelga y pueden dejar más prolijo el borde en piezas redondas. Necesita "
     "los soportes y la interfaz.\n\nEjemplo: líneas para casi todo; concéntrico bajo una cúpula o un saliente "
     "redondo.",
     "This is the pattern of the dense interface layers, the ones that touch the part. Lines lays them straight, "
     "crossing the support's base lines at 90°. Concentric makes loops that follow the edge of the area inward. "
     "Automatic gives lines for now, as long as there is no soluble material.\n\nLines are quick and peel off in "
     "strips; concentric loops follow the overhang's shape and can leave a neater edge on round parts. It needs "
     "supports and an interface.\n\nExample: lines for almost everything; concentric under a dome or a round ledge."},
    {"support.interface_spacing",
     "Es el espacio libre entre las líneas de la interfaz, en milímetros. En 0 van juntas y forman una capa "
     "sólida.\n\nCon algo de espacio, la interfaz gasta menos y se despega más fácil, pero la cara de abajo de la "
     "pieza queda con marcas de rayas. Juntas, la cara sale más lisa y el soporte cuesta un poco más de sacar. "
     "Necesita los soportes y la interfaz.\n\nEjemplo: 0 para una cara de abajo prolija; 0,2 a 0,5 mm si el soporte "
     "queda muy pegado.",
     "This is the free space between the interface lines, in millimetres. At 0 they touch and form a solid "
     "layer.\n\nWith some space, the interface uses less plastic and comes off more easily, but the underside of the "
     "part shows a striped pattern. Packed together, the underside is smoother and the support is a little harder to "
     "remove. It needs supports and an interface.\n\nExample: 0 for a clean underside; 0.2 to 0.5 mm if the support "
     "sticks too hard."},
    {"support.interface_contact_loops",
     "Con este ajuste encendido, la capa de interfaz de más arriba, la que queda justo debajo de la pieza, empieza con "
     "una vuelta cerrada por el borde de cada zona, y el resto se llena con líneas.\n\nEsa vuelta sostiene mejor el "
     "contorno de lo que cuelga y deja más prolijo el borde de abajo, a cambio de un poco más de tiempo y de un "
     "soporte algo más pegado. Necesita los soportes y la interfaz.\n\nEjemplo: apagado normalmente; encendido si los "
     "bordes de un saliente salen caídos.",
     "With this on, the topmost interface layer, the one right under the part, starts with a closed loop around the "
     "edge of each area, and the rest is filled with lines.\n\nThat loop holds the outline of the overhang better and "
     "gives a neater lower edge, at the cost of a little more time and a support that sticks a bit more. It needs "
     "supports and an interface.\n\nExample: off normally; on if the edges of a ledge come out drooping."},
    {"support.spacing",
     "Es el espacio libre entre las líneas de la base del soporte, la parte con huecos que va debajo de la interfaz, "
     "en milímetros.\n\nMás espacio da un soporte más liviano, que gasta menos y se rompe fácil al sacarlo, pero "
     "sostiene menos. Menos espacio lo hace más firme y más pesado. Para que la cara de abajo salga lisa importa más "
     "la interfaz que esto. Necesita los soportes encendidos.\n\nEjemplo: 2,5 mm por defecto; 1,5 mm para una pieza "
     "pesada; 3,5 mm para ahorrar plástico.",
     "This is the free space between the lines of the support base, the open part below the interface, in "
     "millimetres.\n\nMore space gives a lighter support that uses less plastic and breaks away easily, but holds "
     "less. Less space makes it sturdier and heavier. For a smooth underside the interface matters more than this. It "
     "needs supports on.\n\nExample: 2.5 mm by default; 1.5 mm for a heavy part; 3.5 mm to save plastic."},
    {"support.pattern",
     "Es el dibujo de la base del soporte. Líneas las pone todas en la misma dirección. Cuadrícula usa las mismas "
     "líneas, pero giradas 90° en capas alternas. Panal hace celdas de seis lados con la misma cantidad de "
     "plástico.\n\nLas líneas son las más rápidas y fáciles de sacar; la cuadrícula y el panal son más firmes y se "
     "doblan menos en soportes altos, pero cuestan más de romper al sacarlos. Vale con el estilo en "
     "grilla.\n\nEjemplo: líneas para soportes bajos; cuadrícula para uno alto y fino que tambalea.",
     "This is the pattern of the support base. Lines runs them all in one direction. Grid uses the same lines, turned "
     "90° on every other layer. Honeycomb makes six-sided cells with the same amount of plastic.\n\nLines are the "
     "fastest and easiest to remove; grid and honeycomb are sturdier and bend less in tall supports, but are harder to "
     "break away. It applies to the grid style.\n\nExample: lines for short supports; grid for a tall, thin one that "
     "wobbles."},
    {"support.sheath",
     "Con este ajuste encendido, cada zona de la base del soporte lleva una vuelta cerrada por el borde, como una "
     "cáscara, y el dibujo adentro. Apagado, va solo el dibujo, en zigzag.\n\nLa vuelta hace el soporte más firme y "
     "evita que se desarme mientras se imprime, sobre todo en zonas chicas o altas. Sin ella, el soporte gasta un poco "
     "menos y se rompe más fácil al sacarlo. Necesita los soportes encendidos.\n\nEjemplo: encendido por defecto; "
     "apagalo si el soporte cuesta mucho de sacar.",
     "With this on, each area of the support base gets a closed loop around its edge, like a shell, with the pattern "
     "inside. Off, there is only the pattern, in a zigzag.\n\nThe loop makes the support sturdier and keeps it from "
     "falling apart while printing, especially in small or tall areas. Without it, the support uses a little less "
     "plastic and breaks away more easily. It needs supports on.\n\nExample: on by default; turn it off if the support "
     "is very hard to remove."},
    {"support.line_angle",
     "Es la dirección de las líneas de la base del soporte, en grados, y es la misma en todas las capas.\n\nCambiarla "
     "sirve para que las líneas crucen lo que cuelga en vez de correr a lo largo, así lo sostienen mejor, o para que "
     "queden en una dirección cómoda para arrancar el soporte. La interfaz va cruzada a 90° de estas líneas. Necesita "
     "los soportes encendidos.\n\nEjemplo: 0° por defecto; 45° o 90° si las líneas quedan paralelas a un saliente "
     "largo y fino.",
     "This is the direction of the lines in the support base, in degrees, and it is the same on every "
     "layer.\n\nChanging it lets the lines cross the overhang instead of running along it, so they hold it better, or "
     "puts them in a handy direction for pulling the support off. The interface runs at 90° to these lines. It needs "
     "supports on.\n\nExample: 0° by default; 45° or 90° if the lines end up parallel to a long, thin ledge."},
    {"support.width",
     "Es el ancho de cada línea de plástico del soporte y de su interfaz. Se pone en milímetros o en porcentaje de la "
     "altura de capa: con capas de 0,2 mm, 150 % da 0,3 mm. En 0 usa el ancho de las paredes.\n\nLíneas más finas "
     "hacen un soporte más liviano y fácil de romper; más anchas lo hacen más firme, pero más pegado. Conviene que no "
     "se aleje mucho del diámetro de la boquilla. Necesita los soportes encendidos.\n\nEjemplo: 0 para empezar; 0,35 "
     "mm con boquilla de 0,4 mm si querés soportes más fáciles de sacar.",
     "This is the width of each line of plastic in the support and its interface. You can set it in millimetres or as "
     "a percentage of the layer height: with 0.2 mm layers, 150 % gives 0.3 mm. At 0 it uses the wall "
     "width.\n\nThinner lines make a lighter support that breaks away easily; wider ones make it sturdier but more "
     "firmly stuck. Keep it close to the nozzle diameter. It needs supports on.\n\nExample: 0 to start; 0.35 mm with a "
     "0.4 mm nozzle if you want supports that come off more easily."},
    {"support.closing_radius",
     "Con el estilo ajustado, el soporte sigue de cerca la forma de lo que cuelga y puede quedar partido en muchas "
     "zonas chicas. Este ajuste junta las que quedan cerca: se cierran los huecos más angostos que el doble de este "
     "radio, en milímetros.\n\nUn valor más alto une más zonas en bloques grandes, más firmes pero con más material. "
     "Más bajo deja zonas sueltas y finas, que gastan menos pero pueden caerse.\n\nEjemplo: 2 mm por defecto; 4 mm si "
     "ves columnitas sueltas que tambalean.",
     "With the snug style, the support closely follows the shape of the overhang and can end up split into many small "
     "areas. This setting joins the ones that are close: gaps narrower than twice this radius, in millimetres, are "
     "closed.\n\nA higher value merges more areas into big blocks, sturdier but using more plastic. A lower one leaves "
     "thin, separate areas that use less but may topple.\n\nExample: 2 mm by default; 4 mm if you see loose little "
     "columns wobbling."},
    {"adhesion.skirt_layers",
     "Es en cuántas capas se repite la falda, la línea que rodea la pieza sin tocarla.\n\nCon 1, la falda es solo un "
     "dibujo en la cama para que el plástico salga parejo antes de empezar. Con más capas se vuelve una pared baja "
     "alrededor de la pieza, que gasta más plástico. Si usás el escudo contra corrientes limitado, este es su alto en "
     "capas.\n\nEjemplo: 1 para lo normal; 3 a 5 capas para un escudo bajo.",
     "This is how many layers the skirt, the line that circles the part without touching it, is repeated on.\n\nAt 1, "
     "the skirt is just an outline on the bed that gets the plastic flowing evenly before the part starts. With more "
     "layers it becomes a low wall around the part that uses more plastic. If you use the limited draft shield, this "
     "is its height in layers.\n\nExample: 1 for normal prints; 3 to 5 layers for a low shield."},
    {"adhesion.skirt_min_filament",
     "Es el largo mínimo de filamento que tiene que gastar la falda en la primera capa, en milímetros. Si las vueltas "
     "que pediste gastan menos, se agregan vueltas hacia afuera hasta pasarlo. En 0 no se revisa.\n\nSirve para piezas "
     "chicas: una falda corta a veces no alcanza para que el plástico salga parejo. Ojo que se mide en filamento sin "
     "derretir, el hilo que entra a la impresora: 1 mm de filamento da varios centímetros de línea.\n\nEjemplo: 0 para "
     "piezas grandes; 20 a 30 mm para una pieza chiquita.",
     "This is the minimum length of filament the skirt must use on the first layer, in millimetres. If the loops you "
     "asked for use less, more loops are added outward until it is reached. At 0 it is not checked.\n\nIt helps with "
     "small parts, where a short skirt may not be enough to get the plastic flowing evenly. Note that it counts "
     "unmelted filament, the strand fed into the printer: 1 mm of it makes several centimetres of line.\n\nExample: 0 "
     "for large parts; 20 to 30 mm for a tiny part."},
    {"adhesion.draft_shield",
     "El escudo contra corrientes es una falda que crece como una pared fina alrededor de la pieza, para frenar el "
     "aire y que el plástico no se enfríe de golpe y se tuerza. Apagado no hay escudo; limitado lo sube tantas capas "
     "como las de la falda; encendido lo sube hasta la última capa.\n\nAyuda con materiales delicados como el ABS en "
     "impresoras sin gabinete, a cambio de más plástico y tiempo. Con escudo, la falda se mide desde la pieza aunque "
     "haya borde.\n\nEjemplo: apagado para PLA; encendido para una pieza alta de ABS en una impresora abierta.",
     "A draft shield is a skirt that grows into a thin wall around the part, blocking moving air so the plastic does "
     "not cool suddenly and warp. Disabled means no shield; limited raises it as many layers as the skirt has; enabled "
     "raises it all the way to the last layer.\n\nIt helps with touchy materials like ABS on printers without an "
     "enclosure, at the cost of more plastic and time. With a shield, the skirt distance is measured from the part "
     "even when there is a brim.\n\nExample: disabled for PLA; enabled for a tall ABS part on an open printer."},
    {"adhesion.brim_type",
     "Dice dónde va el borde, las vueltas pegadas al pie de la pieza. Afuera lo pone alrededor del contorno exterior. "
     "Adentro lo pone dentro de los agujeros de la primera capa, desde su orilla hacia el centro. Afuera y adentro "
     "hace los dos, y ninguno lo saca.\n\nEl de adentro ayuda cuando la base tiene agujeros grandes cuyas orillas se "
     "levantan; afuera es lo habitual. Cuánto mide lo da el ancho del borde.\n\nEjemplo: afuera para casi todo; afuera "
     "y adentro para un marco de ABS con un agujero grande en el medio.",
     "This says where the brim, the loops attached to the foot of the part, goes. Outer puts it around the outside "
     "outline. Inner puts it inside the holes of the first layer, from their edge toward the middle. Outer and inner "
     "does both, and none removes it.\n\nInner helps when the base has large holes whose edges lift; outer is the "
     "usual choice. How wide it is comes from the brim width.\n\nExample: outer for almost everything; outer and inner "
     "for an ABS frame with a big hole in the middle."},
    {"adhesion.brim_gap",
     "Es un espacio entre la pieza y la primera vuelta del borde, en milímetros. El borde mantiene su ancho, solo se "
     "corre hacia afuera.\n\nCon 0 el borde queda pegado y agarra mejor, pero hay que cortarlo y puede dejar una "
     "rebaba. Con un espacio chico se arranca con los dedos y deja el contorno más limpio, a cambio de sujetar algo "
     "menos. Solo importa si hay borde.\n\nEjemplo: 0 por defecto; 0,1 a 0,2 mm para que se despegue fácil.",
     "This is a gap between the part and the first loop of the brim, in millimetres. The brim keeps its width; it just "
     "moves outward.\n\nAt 0 the brim is attached and grips best, but it must be cut off and may leave a ridge. A "
     "small gap lets you peel it off by hand and leaves a cleaner outline, at the cost of holding a little less. It "
     "only matters if there is a brim.\n\nExample: 0 by default; 0.1 to 0.2 mm so it peels off easily."},
    {"adhesion.raft_layers",
     "La balsa es una plataforma de varias capas que se imprime debajo de toda la pieza; la pieza se construye encima "
     "y después se despega. Este ajuste es cuántas capas tiene. 0 no hace balsa.\n\nAyuda cuando la pieza se pega mal "
     "o se levanta, cuando la cama no está pareja o la base es muy chica. A cambio gasta plástico y tiempo, y la cara "
     "de abajo de la pieza sale más rugosa. Con balsa no hay borde.\n\nEjemplo: 0 casi siempre; 3 capas para una pieza "
     "de ABS que se despega de la cama.",
     "A raft is a platform of several layers printed under the whole part; the part is built on top and peeled off "
     "afterwards. This setting is how many layers it has. 0 means no raft.\n\nIt helps when the part sticks poorly or "
     "lifts, when the bed is uneven, or when the base is very small. In return it costs plastic and time, and the "
     "underside of the part comes out rougher. With a raft there is no brim.\n\nExample: 0 almost always; 3 layers for "
     "an ABS part that keeps coming loose from the bed."},
    {"adhesion.raft_gap",
     "Es el espacio vertical entre la parte de arriba de la balsa y la pieza, en milímetros. Solo importa si hay "
     "balsa.\n\nMás espacio hace que la pieza se despegue fácil de la balsa, pero la cara de abajo sale más caída y "
     "rugosa. Menos espacio da una cara más prolija y una balsa que cuesta separar.\n\nEjemplo: 0,1 mm por defecto; "
     "0,15 a 0,2 mm si la balsa queda soldada a la pieza.",
     "This is the vertical gap between the top of the raft and the part, in millimetres. It only matters if there is a "
     "raft.\n\nA bigger gap makes the part come off the raft easily, but its underside droops and comes out rough. A "
     "smaller gap gives a neater underside and a raft that is hard to separate.\n\nExample: 0.1 mm by default; 0.15 to "
     "0.2 mm if the raft fuses to the part."},
    {"adhesion.raft_expansion",
     "Es cuánto sobresale la balsa alrededor de la primera capa de la pieza, en milímetros.\n\nUna balsa más ancha "
     "agarra más superficie de la cama y se levanta menos en las esquinas, pero gasta más plástico. Más angosta ahorra "
     "material y deja menos para recortar. Solo importa si hay balsa.\n\nEjemplo: 1,5 mm por defecto; 4 a 5 mm para "
     "una pieza que se despega en las puntas.",
     "This is how far the raft sticks out around the part's first layer, in millimetres.\n\nA wider raft grips more of "
     "the bed and lifts less at the corners, but uses more plastic. A narrower one saves material and leaves less to "
     "trim. It only matters if there is a raft.\n\nExample: 1.5 mm by default; 4 to 5 mm for a part that lifts at its "
     "corners."},
    {"adhesion.raft_first_layer_density",
     "Es qué tan llena va la primera capa de la balsa, la que toca la cama, en porcentaje.\n\nMás densidad da más "
     "superficie pegada a la cama y una base más firme, pero tarda más y puede costar despegarla. Menos densidad deja "
     "líneas separadas, que gastan menos pero agarran menos. Solo importa si hay balsa.\n\nEjemplo: 90 % por defecto; "
     "100 % si la balsa se levanta; 60 a 70 % para ahorrar en una cama que pega bien.",
     "This is how full the raft's first layer, the one touching the bed, is, in percent.\n\nHigher density means more "
     "area stuck to the bed and a steadier base, but it takes longer and can be hard to peel off. Lower density leaves "
     "spaced lines that use less plastic but grip less. It only matters if there is a raft.\n\nExample: 90 % by "
     "default; 100 % if the raft lifts; 60 to 70 % to save plastic on a bed that grips well."},
    {"adhesion.raft_first_layer_expansion",
     "La primera capa de la balsa puede ser más grande que el resto, como un pie ancho. Este ajuste es cuántos "
     "milímetros más se agranda esa primera capa, además de lo que ya sobresale la balsa.\n\nMás ancho agarra mejor la "
     "cama y se levanta menos; más angosto ahorra plástico y lugar en la cama. Solo importa si hay balsa.\n\nEjemplo: "
     "3 mm por defecto; 0 para que la balsa tenga el mismo tamaño en todas sus capas.",
     "The raft's first layer can be bigger than the rest, like a wide foot. This setting is how many extra millimetres "
     "that first layer grows, on top of how far the raft already sticks out.\n\nWider grips the bed better and lifts "
     "less; narrower saves plastic and bed space. It only matters if there is a raft.\n\nExample: 3 mm by default; 0 "
     "to keep the raft the same size on every layer."},
    {"interlocking.beams",
     "Cuando una pieza se imprime con dos materiales que se tocan, como un plástico duro y uno flexible, a veces no se "
     "pegan bien entre sí. Con este ajuste encendido, cerca de la unión cada material entra en el otro con vigas: "
     "franjas que se alternan y se cruzan capa a capa, como dedos entrelazados.\n\nLa unión queda trabada y más firme, "
     "aunque los materiales no se peguen solos. Solo sirve en piezas con partes de dos extrusores.\n\nEjemplo: "
     "encendido para una pieza de PLA con una parte flexible encastrada.",
     "When a part is printed in two materials that touch, like a hard plastic and a flexible one, they sometimes do "
     "not bond well. With this on, near the joint each material reaches into the other with beams: strips that "
     "alternate and cross layer by layer, like interlaced fingers.\n\nThe joint ends up locked together and stronger, "
     "even if the materials do not stick on their own. It only applies to parts with pieces from two "
     "extruders.\n\nExample: on for a PLA part with a flexible piece set into it."},
    {"interlocking.beam_width",
     "Con las vigas entre materiales encendidas, es el ancho de cada viga, en milímetros. También es la unidad con que "
     "se miden hasta dónde llegan las vigas y cuánto se alejan del exterior.\n\nVigas más anchas son más fuertes y "
     "fáciles de imprimir, pero hay menos cruces entre los materiales. Más finas entrelazan más, aunque si son "
     "demasiado finas para la boquilla no salen bien.\n\nEjemplo: 0,8 mm, el doble de una boquilla de 0,4 mm.",
     "With beams between materials on, this is the width of each beam, in millimetres. It is also the unit used to "
     "measure how far the beams reach and how far they stay from the outside.\n\nWider beams are stronger and easier "
     "to print, but there are fewer crossings between the materials. Thinner ones interlock more, though if they are "
     "too thin for the nozzle they will not print well.\n\nExample: 0.8 mm, twice a 0.4 mm nozzle."},
    {"interlocking.beam_layers",
     "Con las vigas entre materiales encendidas, es cuántas capas siguen las vigas en la misma dirección antes de "
     "girar 90°.\n\nCon pocas capas se cruzan más seguido y los materiales quedan trabados en todas las direcciones. "
     "Con más capas cada viga es más alta y firme, pero la traba es más gruesa.\n\nEjemplo: 2 capas por defecto; 3 o 4 "
     "si usás capas muy finas.",
     "With beams between materials on, this is how many layers the beams keep the same direction before turning "
     "90°.\n\nWith few layers they cross more often and the materials lock together in every direction. With more "
     "layers each beam is taller and sturdier, but the interlock is coarser.\n\nExample: 2 layers by default; 3 or 4 "
     "if you use very thin layers."},
    {"interlocking.orientation",
     "Con las vigas entre materiales encendidas, es la dirección de las vigas, en grados. Cada tantas capas giran 90° "
     "a partir de esta dirección.\n\nLo normal es no tocarla. Cambiarla puede servir para que las vigas no corran "
     "paralelas a una pared larga de la unión, así la cruzan y traban los dos materiales.\n\nEjemplo: 22,5° por "
     "defecto; 0° o 45° según cómo esté orientada la unión.",
     "With beams between materials on, this is the direction of the beams, in degrees. Every few layers they turn 90° "
     "from this direction.\n\nUsually you leave it alone. Changing it can help when the beams would run parallel to a "
     "long wall of the joint, so that they cross it and lock the two materials together.\n\nExample: 22.5° by default; "
     "0° or 45° depending on how the joint is oriented."},
    {"interlocking.depth",
     "Con las vigas entre materiales encendidas, es hasta dónde entran las vigas a cada lado de la unión, contado en "
     "anchos de viga.\n\nMás profundidad da una traba más larga y fuerte, pero mezcla los dos materiales en una zona "
     "más grande de la pieza. Menos profundidad deja cada material más puro cerca de la unión.\n\nEjemplo: 2 anchos "
     "por defecto, 1,6 mm con vigas de 0,8 mm; 3 o 4 para una unión que va a recibir tirones.",
     "With beams between materials on, this is how far the beams reach into each side of the joint, counted in beam "
     "widths.\n\nMore depth gives a longer, stronger interlock, but mixes the two materials over a larger part of the "
     "model. Less depth keeps each material purer near the joint.\n\nExample: 2 widths by default, 1.6 mm with 0.8 mm "
     "beams; 3 or 4 for a joint that will be pulled hard."},
    {"interlocking.boundary_avoidance",
     "Con las vigas entre materiales encendidas, es cuánto se alejan las vigas de la superficie de afuera de la pieza, "
     "contado en anchos de viga. Esa franja exterior queda sin vigas.\n\nMás distancia mantiene la cara de afuera "
     "limpia, de un solo material, sin rayas del otro; menos distancia lleva la traba hasta cerca del "
     "borde.\n\nEjemplo: 2 anchos por defecto, 1,6 mm con vigas de 0,8 mm; 0 si no importa cómo se ve la unión por "
     "fuera.",
     "With beams between materials on, this is how far the beams stay from the outer surface of the part, counted in "
     "beam widths. That outer band has no beams.\n\nMore distance keeps the outside clean, in a single material with "
     "no stripes of the other; less distance carries the interlock close to the surface.\n\nExample: 2 widths by "
     "default, 1.6 mm with 0.8 mm beams; 0 if it does not matter how the joint looks outside."},
    {"painting.max_region_width",
     "Cuando pintás colores sobre la pieza, cada color va con su propio extrusor. Este ajuste es el ancho máximo de "
     "una región pintada de un color, en milímetros; 0 es sin límite.\n\nPor ahora el motor lee y guarda este valor, "
     "pero todavía no imprime los colores pintados, así que no cambia la pieza. Queda listo para cuando el motor "
     "maneje varios extrusores.\n\nEjemplo: dejalo en 0.",
     "When you paint colours on a part, each colour is printed by its own extruder. This setting is the widest a "
     "region painted in one colour may be, in millimetres; 0 means no limit.\n\nFor now the engine reads and saves "
     "this value, but it does not print painted colours yet, so it does not change the part. It is ready for when the "
     "engine handles several extruders.\n\nExample: leave it at 0."},
    {"painting.interlocking_depth",
     "Cuando pintás colores sobre la pieza, las regiones de colores distintos se tocan. Este ajuste es cuánto se "
     "entrelazan entre sí, en milímetros, para que la unión quede firme.\n\nPor ahora el motor lee y guarda este "
     "valor, pero todavía no imprime los colores pintados, así que no cambia la pieza.\n\nEjemplo: dejalo en 0 por "
     "ahora.",
     "When you paint colours on a part, regions of different colours touch. This setting is how much they interlock "
     "with each other, in millimetres, so the joint holds firmly.\n\nFor now the engine reads and saves this value, "
     "but it does not print painted colours yet, so it does not change the part.\n\nExample: leave it at 0 for now."},
