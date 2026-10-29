// Longer explanations of the basic settings, written for this program. Own
// texts; see PROVENANCE.md.

#include "ui/setting_guide.hpp"

#include "ui/text.hpp"

#include <cstddef>
#include <iterator>

namespace tisma::ui {
namespace {

struct Guide {
    std::string_view key, es, en;
};

constexpr Guide kGuides[] = {
    {"layers.height",
     "La impresora arma la pieza apilando capas finas de plástico, una sobre otra. Este ajuste es el grosor de cada "
     "capa.\n\nCapas más finas dejan la superficie más lisa y los detalles más nítidos, pero hacen falta más capas y la "
     "impresión tarda más. Capas más gruesas imprimen más rápido y la pieza queda un poco más fuerte, pero se notan "
     "los escalones en las curvas.\n\nEjemplo: con una boquilla de 0,4 mm, 0,2 mm es el punto medio de siempre; 0,12 mm "
     "para una figura con detalles; 0,28 mm para una pieza funcional que tiene que salir rápido. No conviene pasar de "
     "unas tres cuartas partes de la boquilla.",
     "The printer builds the part by stacking thin layers of plastic, one on top of the other. This setting is how "
     "thick each layer is.\n\nThinner layers give a smoother surface and sharper details, but there are more of them "
     "and the print takes longer. Thicker layers print faster and the part is a little stronger, but the steps show "
     "on curves.\n\nExample: with a 0.4 mm nozzle, 0.2 mm is the usual middle ground; 0.12 mm for a detailed figure; "
     "0.28 mm for a functional part you need quickly. Stay under about three quarters of the nozzle."},
    {"layers.first_height",
     "La primera capa es la que se pega a la cama, y de ella depende que toda la impresión salga bien.\n\nUna primera "
     "capa algo más gruesa que las demás perdona mejor una cama que no está perfectamente nivelada y se pega con más "
     "fuerza. Si es demasiado gruesa, la base de la pieza queda un poco más ancha (el «pie de elefante»).\n\nEjemplo: "
     "con capas de 0,2 mm, una primera capa de 0,2 a 0,3 mm. Si la pieza se despega o las líneas de la primera capa "
     "no se tocan, subila un poco y revisá la nivelación.",
     "The first layer is the one that sticks to the bed, and the whole print depends on it.\n\nA first layer a "
     "little thicker than the rest forgives a bed that is not perfectly level and sticks better. Too thick, and the "
     "bottom of the part comes out a little wider (the \"elephant's foot\").\n\nExample: with 0.2 mm layers, a first "
     "layer of 0.2 to 0.3 mm. If the part comes loose or the first layer's lines do not touch, raise it a little and "
     "check the levelling."},
    {"walls.count",
     "Las paredes son las líneas que recorren el contorno de la pieza en cada capa, como la cáscara de un huevo. "
     "Este ajuste dice cuántas líneas de contorno se imprimen, una al lado de la otra.\n\nMás paredes hacen la pieza "
     "más fuerte y menos propensa a romperse o a filtrar líquidos; también ocultan mejor el relleno de adentro. "
     "Cada pared de más suma tiempo y plástico.\n\nEjemplo: 2 paredes para una figura decorativa, 3 o 4 para una "
     "pieza que tiene que aguantar fuerza, como un soporte o un gancho.",
     "Walls are the lines that run around the outline of the part on each layer, like the shell of an egg. This "
     "setting is how many outline lines are printed, side by side.\n\nMore walls make the part stronger and less "
     "likely to break or leak; they also hide the infill better. Each extra wall adds time and plastic.\n\nExample: "
     "2 walls for a decorative figure, 3 or 4 for a part that has to hold a load, like a bracket or a hook."},
    {"walls.seam",
     "Cada capa de pared empieza y termina en algún punto, y ahí queda una pequeña marca. Una sobre otra, esas marcas "
     "forman una línea vertical: la costura.\n\nEste ajuste elige dónde va: alineada (todas juntas, en una línea "
     "prolija que conviene esconder en una esquina o atrás), en la esquina más escondida, o al azar (sin línea, pero "
     "con puntitos repartidos por toda la superficie).\n\nEjemplo: para una caja, alineada en una esquina; para un "
     "jarrón redondo donde no hay dónde esconderla, al azar o la más escondida.",
     "Each wall layer starts and ends somewhere, and leaves a small mark there. Stacked, those marks make a vertical "
     "line: the seam.\n\nThis setting chooses where it goes: aligned (all together, in a tidy line best hidden in a "
     "corner or at the back), in the most hidden corner, or random (no line, but small dots all over the "
     "surface).\n\nExample: for a box, aligned in a corner; for a round vase with nowhere to hide it, random or the "
     "most hidden."},
    {"surfaces.top_layers",
     "Por dentro la pieza no es maciza: lleva un relleno con huecos. Arriba se cierra con capas completamente "
     "llenas, que forman el techo. Este ajuste es cuántas capas macizas lleva ese techo.\n\nCon pocas, el relleno se "
     "nota a través de la superficie de arriba o quedan agujeritos. Más capas dan un techo liso y cerrado, a cambio "
     "de algo más de tiempo.\n\nEjemplo: con capas de 0,2 mm, 4 o 5 capas de techo (alrededor de 0,8 a 1 mm). Si ves "
     "el dibujo del relleno arriba, subilo.",
     "Inside, the part is not solid: it has infill with gaps. On top it is closed with completely full layers that "
     "make the roof. This setting is how many solid layers the roof has.\n\nWith too few, the infill shows through "
     "the top surface or small holes remain. More layers give a smooth, closed top, for a little more "
     "time.\n\nExample: with 0.2 mm layers, 4 or 5 top layers (about 0.8 to 1 mm). If you see the infill pattern on "
     "top, raise it."},
    {"surfaces.bottom_layers",
     "Abajo la pieza también se cierra con capas completamente llenas: el piso. Este ajuste es cuántas capas macizas "
     "lleva.\n\nEl piso se apoya sobre la cama, así que necesita menos capas que el techo para quedar bien. Más capas "
     "lo hacen más fuerte y más pesado.\n\nEjemplo: con capas de 0,2 mm, 3 o 4 capas de piso. Para una pieza que "
     "tiene que ser estanca, como un vaso, conviene una o dos más.",
     "At the bottom the part is also closed with completely full layers: the floor. This setting is how many solid "
     "layers it has.\n\nThe floor rests on the bed, so it needs fewer layers than the roof to come out well. More "
     "layers make it stronger and heavier.\n\nExample: with 0.2 mm layers, 3 or 4 bottom layers. For a part that has "
     "to hold water, like a cup, add one or two."},
    {"surfaces.top_min_thickness",
     "El techo de la pieza se mide en capas, pero si las capas son muy finas, pocas capas dan un techo demasiado "
     "delgado. Este ajuste pide un grosor mínimo en milímetros: si las capas no llegan, se agregan las que "
     "falten.\n\nAsí el techo queda bien cerrado aunque cambies la altura de capa.\n\nEjemplo: 0,8 mm de mínimo; con "
     "capas de 0,1 mm eso son 8 capas de techo, con 0,2 mm, 4.",
     "The part's roof is counted in layers, but with very thin layers a few of them make a roof that is too thin. "
     "This setting asks for a minimum thickness in millimetres: if the layers fall short, the missing ones are "
     "added.\n\nThat way the top stays well closed even when you change the layer height.\n\nExample: 0.8 mm "
     "minimum; with 0.1 mm layers that is 8 top layers, with 0.2 mm, 4."},
    {"surfaces.bottom_min_thickness",
     "Lo mismo que el mínimo del techo, para el piso: un grosor mínimo en milímetros. Si las capas de piso no "
     "llegan, se agregan.\n\nEjemplo: 0,6 mm de mínimo; con capas de 0,1 mm son 6 capas de piso.",
     "The same as the roof's minimum, for the floor: a minimum thickness in millimetres. If the bottom layers fall "
     "short, more are added.\n\nExample: 0.6 mm minimum; with 0.1 mm layers that is 6 bottom layers."},
    {"surfaces.top_pattern",
     "Es el dibujo con el que se llena la última capa de arriba, la que se ve.\n\nLíneas rectas dan una superficie "
     "pareja y rápida; concéntrico sigue la forma de la pieza, lindo en piezas redondas; otros dibujos dejan una "
     "textura distinta. No cambia la resistencia, solo cómo se ve.\n\nEjemplo: líneas para casi todo; concéntrico "
     "para la tapa de una caja redonda.",
     "It is the pattern that fills the last top layer, the one you see.\n\nStraight lines give an even, fast "
     "surface; concentric follows the part's shape, nice on round parts; other patterns leave a different texture. "
     "It does not change strength, only the look.\n\nExample: lines for almost everything; concentric for the lid "
     "of a round box."},
    {"infill.density",
     "Adentro, entre las paredes, la pieza lleva una estructura con huecos: el relleno. Este ajuste es qué parte del "
     "interior se llena, en porcentaje.\n\nMás relleno da una pieza más fuerte y pesada, que tarda más y gasta más "
     "plástico. Menos relleno ahorra tiempo y material. Para la fuerza ayudan más las paredes que el relleno.\n\n"
     "Ejemplo: 10 a 15 % para una figura, 20 % para uso general, 40 % o más para una pieza que va a recibir golpes o "
     "peso. 100 % es macizo.",
     "Inside, between the walls, the part has a structure with gaps: the infill. This setting is how much of the "
     "inside is filled, in percent.\n\nMore infill makes a stronger, heavier part that takes longer and uses more "
     "plastic. Less infill saves time and material. Walls help strength more than infill does.\n\nExample: 10 to "
     "15 % for a figure, 20 % for general use, 40 % or more for a part that will take hits or weight. 100 % is "
     "solid."},
    {"infill.pattern",
     "Es la forma de la estructura de adentro: líneas, cuadrícula, triángulos, panal de abejas y otras.\n\nAlgunos "
     "dibujos son más rápidos, otros resisten mejor la fuerza en todas las direcciones, y otros sostienen mejor el "
     "techo. Con el mismo porcentaje, el dibujo cambia cuánto aguanta la pieza.\n\nEjemplo: líneas o cuadrícula para "
     "piezas comunes; giroide o cúbico cuando la fuerza puede venir de cualquier lado.",
     "It is the shape of the structure inside: lines, grid, triangles, honeycomb and others.\n\nSome patterns are "
     "faster, others resist force from every direction better, and others hold up the top better. At the same "
     "percentage, the pattern changes how much the part can take.\n\nExample: lines or grid for common parts; gyroid "
     "or cubic when the force can come from any side."},
    {"support.enabled",
     "La impresora no puede poner plástico en el aire: cada capa se apoya en la de abajo. Si una parte de la pieza "
     "sobresale, como un brazo extendido o un techo, necesita apoyo. Los soportes son columnas que se imprimen "
     "debajo y se sacan al terminar.\n\nSin soportes, lo que cuelga se cae o sale feo. Con soportes, sale bien, pero "
     "dejan marcas donde tocaban y suman tiempo.\n\nEjemplo: una letra T parada necesita soporte debajo de los "
     "brazos; un cubo no necesita.",
     "The printer cannot lay plastic on air: each layer rests on the one below. If part of the model sticks out, "
     "like an outstretched arm or a ceiling, it needs something under it. Supports are columns printed underneath "
     "and removed when the print is done.\n\nWithout supports, what overhangs droops or fails. With them it comes out "
     "well, but they leave marks where they touched and add time.\n\nExample: a standing letter T needs support under "
     "its arms; a cube does not."},
    {"support.automatic",
     "Con este ajuste encendido, el programa busca solo las zonas que cuelgan demasiado y les pone soporte. Apagado, "
     "solo hay soporte donde lo pintes a mano.\n\nLa búsqueda automática usa un ángulo: lo que cuelga más que eso "
     "lleva soporte. Pintar a mano sirve cuando querés soporte en un lugar puntual y en ningún otro.\n\nEjemplo: "
     "dejalo automático para empezar, y mirá en la vista previa dónde quedaron.",
     "With this on, the program finds by itself the areas that overhang too much and supports them. Off, there is "
     "support only where you paint it by hand.\n\nThe automatic search uses an angle: whatever overhangs more than "
     "that gets support. Painting by hand is useful when you want support in one spot and nowhere else.\n\nExample: "
     "leave it automatic to start, and check in the preview where they ended up."},
    {"adhesion.skirt_loops",
     "La falda es una línea que se imprime alrededor de la pieza, sin tocarla, antes de empezar. Sirve para que el "
     "plástico ya salga parejo cuando la boquilla llega a la pieza, y para ver de un vistazo si la cama está bien "
     "nivelada.\n\nEste ajuste es cuántas vueltas da. 0 la saca.\n\nEjemplo: 1 o 2 vueltas casi siempre alcanzan.",
     "The skirt is a line printed around the part, without touching it, before it starts. It gets the plastic "
     "flowing evenly before the nozzle reaches the part, and shows at a glance whether the bed is level.\n\nThis "
     "setting is how many loops it makes. 0 removes it.\n\nExample: 1 or 2 loops are almost always enough."},
    {"adhesion.skirt_distance",
     "Es la separación entre la falda y la pieza.\n\nCerca se ve mejor si la cama está nivelada justo donde va la "
     "pieza; lejos es más fácil sacarla. Si es 0, la falda toca la pieza y la ayuda a pegarse, como un borde "
     "finito.\n\nEjemplo: 3 a 6 mm.",
     "It is the gap between the skirt and the part.\n\nClose, it shows better whether the bed is level right where "
     "the part goes; far, it is easier to remove. At 0 the skirt touches the part and helps it stick, like a thin "
     "brim.\n\nExample: 3 to 6 mm."},
    {"adhesion.brim_width",
     "El borde son varias vueltas de plástico pegadas al pie de la pieza, en la primera capa, como el ala de un "
     "sombrero. Agranda la superficie que se agarra de la cama.\n\nSirve para piezas con poca base o altas y finas, y "
     "para materiales que tienden a levantarse en las esquinas, como el ABS. Después se corta o se arranca.\n\n"
     "Ejemplo: 0 para una pieza con buena base; 5 a 8 mm para una torre fina o una pieza de ABS.",
     "The brim is several loops of plastic attached to the foot of the part on the first layer, like the brim of a "
     "hat. It enlarges the area that grips the bed.\n\nIt helps parts with a small base or tall and thin ones, and "
     "materials that tend to lift at the corners, like ABS. It is cut or peeled off afterwards.\n\nExample: 0 for a "
     "part with a good base; 5 to 8 mm for a thin tower or an ABS part."},
    {"machine.variables.temperature",
     "La boquilla derrite el plástico para que salga como un hilo fino. Esta es la temperatura a la que se calienta "
     "mientras imprime.\n\nCada material tiene su rango (lo dice el fabricante en la caja). Muy fría, el plástico no "
     "sale bien y las capas se separan; muy caliente, gotea, deja hilos y los detalles se deforman.\n\nEjemplo: PLA "
     "entre 195 y 220 °C, PETG entre 230 y 250 °C, ABS entre 240 y 260 °C. La calibración de temperatura te ayuda a "
     "encontrar el mejor valor para tu rollo.",
     "The nozzle melts the plastic so it comes out as a thin thread. This is the temperature it heats to while "
     "printing.\n\nEach material has its range (the maker gives it on the box). Too cold, the plastic does not flow "
     "well and layers split; too hot, it drips, leaves strings and details deform.\n\nExample: PLA between 195 and "
     "220 °C, PETG between 230 and 250 °C, ABS between 240 and 260 °C. The temperature calibration helps you find "
     "the best value for your spool."},
    {"machine.variables.bed_temperature",
     "La cama es la base donde se imprime. Calentarla ayuda a que la primera capa se pegue y a que la pieza no se "
     "despegue ni se curve mientras se enfría.\n\nCada material pide su temperatura: muy fría, la pieza se levanta en "
     "las esquinas; muy caliente, la base se ablanda y se deforma.\n\nEjemplo: PLA 55 a 65 °C, PETG 70 a 85 °C, ABS "
     "95 a 110 °C. Algunas camas no calientan: entonces va 0.",
     "The bed is the base the part is printed on. Heating it helps the first layer stick and keeps the part from "
     "lifting or warping as it cools.\n\nEach material asks for its own: too cold, the part lifts at the corners; too "
     "hot, the base softens and deforms.\n\nExample: PLA 55 to 65 °C, PETG 70 to 85 °C, ABS 95 to 110 °C. Some beds "
     "do not heat: then it is 0."},
    {"cooling.fan_min",
     "El ventilador sopla aire sobre el plástico recién puesto para que se endurezca rápido. Este es el mínimo con "
     "el que gira mientras imprime.\n\nMás ventilador da detalles más nítidos, voladizos más prolijos y puentes "
     "mejores; demasiado puede debilitar la unión entre capas y hacer que materiales como el ABS se partan.\n\n"
     "Ejemplo: PLA 100 %, PETG 30 a 60 %, ABS 0 a 20 %.",
     "The fan blows air on the freshly laid plastic so it hardens quickly. This is the least it spins while "
     "printing.\n\nMore fan gives sharper details, cleaner overhangs and better bridges; too much can weaken the bond "
     "between layers and make materials like ABS crack.\n\nExample: PLA 100 %, PETG 30 to 60 %, ABS 0 to 20 %."},
    {"cooling.fan_max",
     "Es el máximo del ventilador: llega a este valor en las capas chicas, que se imprimen tan rápido que no "
     "alcanzan a enfriarse solas.\n\nEjemplo: PLA 100 %, PETG 60 a 80 %, ABS 30 %.",
     "It is the fan's maximum: it reaches this on small layers, which print so fast that they cannot cool by "
     "themselves.\n\nExample: PLA 100 %, PETG 60 to 80 %, ABS 30 %."},
    {"retraction.length",
     "Cuando la boquilla tiene que viajar de un lugar a otro sin imprimir, el plástico derretido tiende a gotear y "
     "deja hilos finos entre las partes, como telarañas. La retracción tira el filamento un poco hacia atrás antes "
     "de cada viaje para que no gotee.\n\nEste ajuste es cuánto tira. Poco deja hilos; demasiado puede trabar el "
     "filamento o dejar huecos al volver a imprimir.\n\nEjemplo: 0,5 a 1 mm en impresoras con el motor encima de la "
     "boquilla (directas); 4 a 6 mm con tubo largo (Bowden).",
     "When the nozzle has to travel from one place to another without printing, the melted plastic tends to ooze "
     "and leaves thin strings between the parts, like cobwebs. Retraction pulls the filament back a little before "
     "each travel so it does not ooze.\n\nThis setting is how much it pulls. Too little leaves strings; too much can "
     "jam the filament or leave gaps when printing resumes.\n\nExample: 0.5 to 1 mm on printers with the motor over "
     "the nozzle (direct drive); 4 to 6 mm with a long tube (Bowden)."},
    {"retraction.lift",
     "En cada viaje, la boquilla puede subir un poco antes de moverse, para no rozar ni golpear lo ya impreso.\n\n"
     "Ayuda en piezas con partes salidas o superficies arriba que se marcarían; suma un poco de tiempo y a veces "
     "algún hilo.\n\nEjemplo: 0 si no hay problemas; 0,2 a 0,4 mm si la boquilla raspa la pieza.",
     "On each travel the nozzle can rise a little before moving, so it does not scrape or hit what is already "
     "printed.\n\nIt helps on parts with bits that stick up or top surfaces that would get marked; it adds a little "
     "time and sometimes a string.\n\nExample: 0 if there are no problems; 0.2 to 0.4 mm if the nozzle scrapes the "
     "part."},
    {"machine.nozzle_diameter",
     "Es el tamaño del agujero por donde sale el plástico. Tiene que ser el de la boquilla que tiene puesta tu "
     "impresora.\n\nUna boquilla más chica hace detalles más finos y tarda más; una más grande imprime mucho más rápido "
     "y más fuerte, con menos detalle. El ancho de las líneas y la altura máxima de capa dependen de ella.\n\n"
     "Ejemplo: 0,4 mm viene en casi todas las impresoras; 0,6 mm para piezas grandes; 0,2 mm para miniaturas.",
     "It is the size of the hole the plastic comes out of. It has to be the nozzle your printer has on.\n\nA smaller "
     "nozzle makes finer details and takes longer; a larger one prints much faster and stronger, with less detail. "
     "The line width and the largest layer height depend on it.\n\nExample: 0.4 mm comes with almost every printer; "
     "0.6 mm for large parts; 0.2 mm for miniatures."},
    {"machine.filament_diameter",
     "Es el grosor del hilo de plástico que entra en la impresora. Casi todos los rollos son de 1,75 mm; algunas "
     "impresoras usan 2,85 mm.\n\nEl programa lo usa para calcular cuánto plástico empujar. Si no coincide con tu "
     "rollo, la pieza sale con plástico de más o de menos.\n\nEjemplo: 1,75 mm; mirá la etiqueta del rollo.",
     "It is the thickness of the plastic thread going into the printer. Almost every spool is 1.75 mm; some "
     "printers use 2.85 mm.\n\nThe program uses it to work out how much plastic to push. If it does not match your "
     "spool, the part gets too much or too little plastic.\n\nExample: 1.75 mm; check the spool's label."},
    {"machine.bed",
     "Es el tamaño de la superficie donde se imprime. El programa lo usa para dibujar la cama y para avisarte si la "
     "pieza no entra.\n\nTiene que coincidir con tu impresora: si es más grande que la real, la boquilla podría "
     "intentar ir más allá del borde.\n\nEjemplo: 220 × 220 mm en muchas impresoras chicas, 256 × 256 mm en otras.",
     "It is the size of the surface you print on. The program uses it to draw the bed and to warn you when the part "
     "does not fit.\n\nIt has to match your printer: if it is larger than the real one, the nozzle could try to go "
     "past the edge.\n\nExample: 220 × 220 mm on many small printers, 256 × 256 mm on others."},
    {"machine.max_height",
     "Es lo más alto que puede imprimir tu impresora. Si la pieza es más alta, el programa avisa antes de "
     "laminar.\n\nEjemplo: 250 mm en muchas impresoras; mirá los datos de la tuya.",
     "It is the tallest your printer can print. If the part is taller, the program warns you before "
     "slicing.\n\nExample: 250 mm on many printers; check yours."},
};

} // namespace

std::string_view setting_guide(std::string_view key) {
    for (const Guide& g : kGuides)
        if (g.key == key)
            return language() == Language::English ? g.en : g.es;
    return {};
}

std::size_t setting_guide_count() { return std::size(kGuides); }

std::string_view setting_guide_key(std::size_t i) { return i < std::size(kGuides) ? kGuides[i].key : std::string_view(); }

} // namespace tisma::ui
