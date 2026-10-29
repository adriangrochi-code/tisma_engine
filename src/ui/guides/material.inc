// Explanations of the material settings for setting_guide.cpp: {key, Spanish, English},
// one entry per setting. Own texts; see PROVENANCE.md.
    {"compensation.scale_x",
     "Ninguna impresora es perfecta: a veces una pieza que debería medir 30 mm de largo sale de 29,8. Este ajuste "
     "agranda o achica el modelo a lo largo del eje X (de izquierda a derecha en la cama), en porcentaje, para "
     "corregir esa diferencia.\n\n100 % deja el tamaño como está. Más de 100 agranda, menos achica. La pieza crece "
     "desde el centro de su base, así que queda en el mismo lugar de la cama. Tocalo solo si mediste una pieza con "
     "calibre y X no da.\n\nEjemplo: un lado de 30 mm sale de 29,7 mm; la corrección es 30 / 29,7, o sea 101 %. La "
     "calibración de dimensiones de la pestaña Calibración hace la cuenta por vos.",
     "No printer is perfect: a part that should be 30 mm long sometimes comes out at 29.8. This setting stretches or "
     "shrinks the model along the X axis (left to right on the bed), in percent, to correct that.\n\n100 % keeps the "
     "size as it is. Above 100 enlarges, below shrinks. The part grows from the centre of its base, so it stays in the "
     "same spot on the bed. Only touch it if you measured a part with calipers and X is off.\n\nExample: a 30 mm side "
     "comes out at 29.7 mm; the correction is 30 / 29.7, about 101 %. The dimensions test in the Calibration tab does "
     "the maths for you."},
    {"compensation.scale_y",
     "Lo mismo que la escala de X, pero para el eje Y (de adelante hacia atrás en la cama): agranda o achica el modelo "
     "en esa dirección, en porcentaje, para que las medidas salgan justas.\n\nCada eje de la impresora puede quedar un "
     "poco distinto, por eso se corrigen por separado. 100 % no cambia nada. Tocalo solo después de medir una pieza de "
     "prueba.\n\nEjemplo: un lado de 20 mm a lo largo de Y sale de 20,2 mm; 20 / 20,2 da 99 %. La pestaña Calibración "
     "lo calcula con la pieza de medidas.",
     "The same as the X scale, but for the Y axis (front to back on the bed): it stretches or shrinks the model in "
     "that direction, in percent, so the sizes come out right.\n\nEach axis of the printer can be slightly different, "
     "which is why they are corrected separately. 100 % changes nothing. Only touch it after measuring a test "
     "part.\n\nExample: a 20 mm side along Y comes out at 20.2 mm; 20 / 20.2 gives 99 %. The Calibration tab works it "
     "out from the dimensions test part."},
    {"compensation.scale_z",
     "Es la escala en altura (el eje Z): agranda o achica el modelo hacia arriba, en porcentaje. La base queda apoyada "
     "en la cama y la pieza crece o se achica desde abajo.\n\nSi las piezas salen un poco más bajas o más altas de lo "
     "que deberían, esto lo corrige. 100 % no cambia nada. La comprobación de altura máxima ya mira la pieza con la "
     "escala puesta.\n\nEjemplo: una pieza de 15 mm de alto sale de 14,85 mm; 15 / 14,85 da 101 %. Si la diferencia es "
     "de una sola capa, revisá antes la primera capa.",
     "It is the scale in height (the Z axis): it stretches or shrinks the model upwards, in percent. The base stays on "
     "the bed and the part grows or shrinks from the bottom.\n\nIf parts come out a little shorter or taller than they "
     "should, this corrects it. 100 % changes nothing. The maximum height check already sees the scaled "
     "part.\n\nExample: a part 15 mm tall comes out at 14.85 mm; 15 / 14.85 gives 101 %. If the difference is exactly "
     "one layer, check the first layer first."},
    {"compensation.hole_offset",
     "Los agujeros de las piezas impresas suelen salir un poco más chicos de lo que dice el modelo, y un tornillo o un "
     "eje no entra. Este ajuste agranda (o achica, con un valor negativo) solo los agujeros, en milímetros sobre el "
     "radio.\n\nEl contorno de afuera de la pieza no se mueve: cambia solo el tamaño de los huecos. Como el valor es "
     "sobre el radio, el diámetro cambia el doble.\n\nEjemplo: un agujero de 10 mm sale de 9,8 mm; le faltan 0,2 mm de "
     "diámetro, así que va 0,1 mm. La calibración de dimensiones lo calcula midiendo el agujero de la pieza de prueba.",
     "Holes in printed parts tend to come out a little smaller than the model says, so a screw or a shaft does not "
     "fit. This setting enlarges (or shrinks, with a negative value) only the holes, in millimetres on the "
     "radius.\n\nThe outside outline of the part does not move: only the size of the holes changes. Since the value is "
     "on the radius, the diameter changes by twice as much.\n\nExample: a 10 mm hole comes out at 9.8 mm; it is 0.2 mm "
     "short in diameter, so use 0.1 mm. The dimensions calibration works it out by measuring the hole in the test "
     "part."},
    {"extrusion.start_gcode",
     "El G-code es la lista de órdenes que lee la impresora: moverse, calentar, empujar plástico. Este es un bloque de "
     "órdenes propio del filamento, que se escribe al empezar: después del inicio de la impresora y de la espera de la "
     "boquilla.\n\nSirve para cosas que dependen del material y no de la impresora. Usa el mismo lenguaje de "
     "plantillas que el resto del G-code personalizado. Vacío, no agrega nada; si no sabés qué poner, dejalo "
     "así.\n\nEjemplo: vacío para casi todos los rollos; con un material que necesita algo especial al empezar, como "
     "encender un filtro de aire para el ABS, esa orden va acá.",
     "G-code is the list of instructions the printer reads: move, heat, push plastic. This is a block of instructions "
     "that belongs to the filament and is written at the start: after the printer's start code and the wait for the "
     "nozzle to heat.\n\nIt is for things that depend on the material rather than the printer. It uses the same "
     "template language as the rest of the custom G-code. Empty adds nothing; if you are not sure what to put, leave "
     "it that way.\n\nExample: empty for almost every spool; with a material that needs something special at the "
     "start, like switching on an air filter for ABS, that command goes here."},
    {"extrusion.end_gcode",
     "Es el bloque de órdenes del filamento que se escribe al terminar, justo antes del final de la impresora. Es la "
     "pareja del G-code de inicio del filamento.\n\nSirve para deshacer lo que hizo el inicio, o para algo que el "
     "material necesita al terminar. Usa el mismo lenguaje de plantillas. Vacío, no agrega nada.\n\nEjemplo: vacío "
     "casi siempre; si en el inicio encendiste un filtro de aire para el ABS, acá va la orden que lo apaga.",
     "It is the filament's block of instructions written at the end, right before the printer's end code. It is the "
     "partner of the filament's start G-code.\n\nIt is for undoing what the start code did, or for something the "
     "material needs at the end. It uses the same template language. Empty adds nothing.\n\nExample: empty almost "
     "always; if the start code switched on an air filter for ABS, the command that switches it off goes here."},
    {"extrusion.chamber_temperature",
     "Algunas impresoras son cerradas y pueden calentar el aire de adentro, la cámara. Este ajuste es la temperatura "
     "de cámara que pide el filamento, en °C.\n\nSi la impresora calienta la cámara, el programa la pide al empezar, "
     "para que caliente junto con la boquilla, sin pasar del máximo de la impresora, y la apaga al terminar. Una "
     "cámara tibia evita que materiales como el ABS se curven o se rajen al enfriarse. Si tu impresora no calienta la "
     "cámara o no llega, aparece un aviso. 0 no pide nada.\n\nEjemplo: PLA y PETG, 0; ABS o ASA, 40 a 50 °C; "
     "policarbonato, 50 a 60 °C si la impresora llega.",
     "Some printers are closed and can heat the air inside, the chamber. This setting is the chamber temperature the "
     "filament asks for, in °C.\n\nIf the printer heats its chamber, the program asks for it at the start, so it warms "
     "up together with the nozzle, never above the printer's maximum, and switches it off at the end. A warm chamber "
     "keeps materials like ABS from warping or cracking as they cool. If your printer does not heat its chamber or "
     "cannot reach the value, you get a warning. 0 asks for nothing.\n\nExample: PLA and PETG, 0; ABS or ASA, 40 to 50 "
     "°C; polycarbonate, 50 to 60 °C if the printer can reach it."},
    {"extrusion.chamber_min_temperature",
     "Calentar la cámara puede tardar bastante. Este ajuste es la temperatura mínima de cámara a la que la impresora "
     "espera antes de empezar a imprimir, en °C. Solo sirve con una impresora que calienta la cámara y con la "
     "temperatura de cámara puesta.\n\nMás alto, la primera capa empieza en un ambiente más estable, pero la espera es "
     "más larga. 0 no espera: la cámara sigue calentando mientras imprime.\n\nEjemplo: con una cámara pedida de 50 °C "
     "para ABS, esperar a 40 °C: arranca antes y el resto lo alcanza mientras imprime.",
     "Heating the chamber can take a while. This setting is the minimum chamber temperature the printer waits for "
     "before it starts printing, in °C. It only matters on a printer that heats its chamber and with a chamber "
     "temperature set.\n\nHigher means the first layer starts in a steadier environment, but the wait is longer. 0 "
     "does not wait: the chamber keeps warming while printing.\n\nExample: with 50 °C asked for ABS, wait for 40 °C: "
     "it starts sooner and reaches the rest while printing."},
    {"extrusion.density",
     "Es cuánto pesa un centímetro cúbico del plástico del rollo, en g/cm³. El programa ya sabe cuántos centímetros "
     "cúbicos usa la pieza; con la densidad calcula cuántos gramos son.\n\nEl peso se escribe al final del G-code y "
     "sirve para saber si te alcanza el rollo y cuánto cuesta la pieza. No cambia cómo se imprime. Con 0 el peso no se "
     "calcula.\n\nEjemplo: PLA alrededor de 1,24; PETG 1,27; ABS 1,04; TPU 1,21. El fabricante suele ponerlo en la "
     "ficha del material.",
     "It is how much one cubic centimetre of the spool's plastic weighs, in g/cm³. The program already knows how many "
     "cubic centimetres the part uses; with the density it works out how many grams that is.\n\nThe weight is written "
     "at the end of the G-code and tells you whether the spool is enough and what the part costs. It does not change "
     "how the part prints. At 0 the weight is not calculated.\n\nExample: PLA about 1.24; PETG 1.27; ABS 1.04; TPU "
     "1.21. The maker usually lists it on the material's data sheet."},
    {"extrusion.cost",
     "Es lo que pagaste por un kilo de este filamento, en la moneda que uses. Con el peso de la pieza, el programa "
     "calcula cuánto cuesta el plástico que lleva y lo escribe al final del G-code.\n\nNo cambia la impresión: es solo "
     "para llevar la cuenta. Necesita también la densidad, para saber el peso.\n\nEjemplo: si el rollo de 1 kg te "
     "costó 20, poné 20; una pieza de 50 g cuesta entonces 1 de plástico.",
     "It is what you paid for one kilogram of this filament, in whatever currency you use. With the part's weight, the "
     "program works out what the plastic in it costs and writes it at the end of the G-code.\n\nIt does not change the "
     "print: it is only for keeping track. It also needs the density, to know the weight.\n\nExample: if the 1 kg "
     "spool cost you 20, enter 20; a 50 g part then costs 1 in plastic."},
    {"extrusion.spool_weight",
     "Es cuánto pesa el carrete vacío, el centro de cartón o plástico donde viene enrollado el filamento, en "
     "gramos.\n\nCon este dato, si pesás el rollo en una balanza, el programa puede restar el carrete y decirte cuánto "
     "plástico te queda, para saber si alcanza para la próxima pieza. No cambia la impresión.\n\nEjemplo: muchos "
     "carretes vacíos pesan entre 150 y 250 g. Lo mejor es pesar uno vacío de la misma marca.",
     "It is how much the empty spool weighs, the cardboard or plastic core the filament comes wound on, in "
     "grams.\n\nWith this, if you weigh the spool on a scale, the program can subtract the core and tell you how much "
     "plastic is left, so you know whether it is enough for the next part. It does not change the print.\n\nExample: "
     "many empty spools weigh between 150 and 250 g. The best is to weigh an empty one of the same brand."},
    {"extrusion.shrinkage_xy",
     "El plástico se achica un poco al enfriarse, y cada material se achica distinto. Este ajuste dice cuánto se "
     "contrae este filamento a lo ancho y a lo largo de las capas, en porcentaje, y el programa imprime la pieza un "
     "poco más grande para que al enfriarse quede de la medida pedida.\n\nSe suma a la escala de compensación de la "
     "impresora. Tocalo si con este material las piezas salen sistemáticamente chicas y con otros no.\n\nEjemplo: PLA, "
     "0 a 0,3 %; ABS, alrededor de 0,5 a 0,8 %. Con 1 %, un cubo de 99 mm se imprime de 100 mm para que quede de 99.",
     "Plastic shrinks a little as it cools, and each material shrinks differently. This setting is how much this "
     "filament contracts across the layers, in percent, and the program prints the part a little larger so it ends up "
     "at the requested size once cool.\n\nIt adds to the printer's compensation scale. Touch it if parts in this "
     "material keep coming out small while others do not.\n\nExample: PLA, 0 to 0.3 %; ABS, about 0.5 to 0.8 %. At 1 "
     "%, a 99 mm cube is printed at 100 mm so it ends up at 99."},
    {"extrusion.shrinkage_z",
     "Lo mismo que la contracción a lo ancho, pero en altura: cuánto se achica este filamento hacia arriba al "
     "enfriarse, en porcentaje. La pieza se imprime un poco más alta para compensarlo.\n\nSe suma a la escala Z de la "
     "compensación. En altura suele haber menos diferencia que a lo ancho, así que muchas veces va 0.\n\nEjemplo: 0 "
     "para PLA y PETG; si las piezas de ABS salen más bajas de lo que deberían, probá 0,3 a 0,5 %.",
     "The same as the shrinkage across the layers, but in height: how much this filament shrinks upwards as it cools, "
     "in percent. The part is printed a little taller to make up for it.\n\nIt adds to the Z scale of the "
     "compensation. Height usually differs less than width, so it is often 0.\n\nExample: 0 for PLA and PETG; if ABS "
     "parts come out shorter than they should, try 0.3 to 0.5 %."},
    {"cooling.fan_below_time",
     "El ventilador se ajusta solo según lo que tarda cada capa: las capas cortas, que no tienen tiempo de enfriarse, "
     "llevan más aire; las largas, menos. Este ajuste es el tiempo de capa, en segundos, a partir del cual el "
     "ventilador se apaga.\n\nEntre el tiempo mínimo de capa y este valor, el ventilador baja de a poco del máximo al "
     "mínimo. Una capa que tarda esto o más va sin ventilador, salvo que esté encendido «ventilador siempre», que lo "
     "deja en el mínimo.\n\nEjemplo: 60 s por defecto; para ABS, que quiere poco aire, 30 s; para PLA conviene más "
     "encender «ventilador siempre».",
     "The fan adjusts itself to how long each layer takes: short layers, which have no time to cool, get more air; "
     "long ones, less. This setting is the layer time, in seconds, from which the fan switches off.\n\nBetween the "
     "minimum layer time and this value, the fan eases down from its maximum to its minimum. A layer that takes this "
     "long or longer runs with no fan, unless fan always on is enabled, which keeps it at the minimum.\n\nExample: 60 "
     "s by default; for ABS, which wants little air, 30 s; for PLA it is better to turn on fan always on."},
    {"cooling.fan_always",
     "Con esto encendido, el ventilador nunca baja del mínimo, ni siquiera en las capas largas que tardan mucho. "
     "Apagado, en esas capas se apaga del todo.\n\nMateriales como el PLA salen mejor con aire todo el tiempo: los "
     "detalles quedan más nítidos. Otros, como el ABS, prefieren poco aire para no rajarse. Las primeras capas sin "
     "ventilador siguen sin él igual.\n\nEjemplo: encendido para PLA y PETG; apagado para ABS y ASA.",
     "With this on, the fan never drops below its minimum, not even on long layers that take a while. Off, it switches "
     "off completely on those layers.\n\nMaterials like PLA come out better with air all the time: details stay "
     "sharper. Others, like ABS, prefer little air so they do not crack. The first layers without fan still go without "
     "it.\n\nExample: on for PLA and PETG; off for ABS and ASA."},
    {"cooling.fan_bridges",
     "Un puente es una línea de plástico tendida en el aire entre dos partes de la pieza, como un techo sin nada "
     "abajo. Este ajuste es la fuerza del ventilador mientras se imprime un puente, en porcentaje.\n\nMás aire "
     "endurece el hilo enseguida y evita que cuelgue. Al terminar el puente, el ventilador vuelve al valor de la capa. "
     "Bajalo solo con materiales que se rajan con mucho aire.\n\nEjemplo: PLA 100 %; PETG 70 a 100 %; ABS 40 a 60 %.",
     "A bridge is a line of plastic stretched through the air between two parts of the model, like a ceiling with "
     "nothing underneath. This setting is how hard the fan blows while printing a bridge, in percent.\n\nMore air "
     "hardens the strand at once and keeps it from sagging. When the bridge ends, the fan goes back to the layer's "
     "value. Lower it only with materials that crack with a lot of air.\n\nExample: PLA 100 %; PETG 70 to 100 %; ABS "
     "40 to 60 %."},
    {"cooling.fan_off_layers",
     "Las primeras capas se imprimen sin ventilador: así el plástico se mantiene caliente, se aplasta bien contra la "
     "cama y se pega mejor. Este ajuste es cuántas capas del principio van sin aire.\n\nMás capas ayudan a que la "
     "pieza no se despegue, sobre todo con materiales que se levantan en las esquinas. Una excepción: si una de esas "
     "capas es tan corta que tiene que ir más lenta por el tiempo mínimo, lleva el ventilador al máximo "
     "igual.\n\nEjemplo: 1 a 3 para PLA; 3 a 5 para PETG o ABS.",
     "The first layers are printed with no fan: this keeps the plastic hot so it presses well into the bed and sticks "
     "better. This setting is how many layers at the start go without air.\n\nMore layers help the part stay put, "
     "especially with materials that lift at the corners. One exception: if one of those layers is so short that it "
     "has to slow down for the minimum layer time, it gets full fan anyway.\n\nExample: 1 to 3 for PLA; 3 to 5 for "
     "PETG or ABS."},
    {"cooling.full_fan_layer",
     "Después de las capas sin ventilador, el aire puede entrar de golpe o ir subiendo de a poco. Este ajuste es la "
     "capa (contando desde 1) en la que el ventilador llega a su valor completo; antes sube en línea recta.\n\nSubirlo "
     "de a poco evita un cambio brusco entre las capas de abajo, calientes, y las de arriba, frías, que puede despegar "
     "o rajar la base. 0, o un número no mayor que las capas sin ventilador, pone el valor completo desde la primera "
     "capa con aire.\n\nEjemplo: con 1 capa sin ventilador y este en 4, la capa 2 lleva un tercio, la 3 dos tercios y "
     "la 4 todo.",
     "After the layers with no fan, the air can come on all at once or ramp up gradually. This setting is the layer "
     "(counting from 1) where the fan reaches its full value; before that it rises in a straight line.\n\nRamping up "
     "avoids a sudden change between the hot bottom layers and the cooled upper ones, which can lift or crack the "
     "base. 0, or a number no higher than the no-fan layers, gives the full value from the first layer with "
     "air.\n\nExample: with 1 layer without fan and this at 4, layer 2 gets a third, layer 3 two thirds and layer 4 "
     "all of it."},
    {"cooling.dynamic_overhang_fan",
     "Un voladizo es una pared que sale hacia afuera y queda apoyada solo en parte sobre la capa de abajo. Con esto "
     "encendido, el ventilador sube en las paredes según cuánto quedan en el aire, usando los cuatro valores de "
     "ventilador, de sin apoyo a apoyado tres cuartos.\n\nSirve para materiales que en general llevan poco aire, como "
     "el PETG o el ABS, pero que necesitan más en los voladizos para que no cuelguen. Vale el mayor entre este y el de "
     "la capa; los puentes siguen con su propio ventilador y las primeras capas siguen sin aire.\n\nEjemplo: apagado "
     "para PLA, que ya va con el ventilador alto; encendido para PETG con voladizos marcados.",
     "An overhang is a wall that leans outwards and rests only partly on the layer below. With this on, the fan rises "
     "on walls according to how much of them hangs in the air, using the four overhang fan values, from unsupported to "
     "three quarters supported.\n\nIt is for materials that usually get little air, like PETG or ABS, but need more on "
     "overhangs so they do not droop. The higher of this and the layer's fan wins; bridges keep their own fan and the "
     "first layers still go without air.\n\nExample: off for PLA, which already runs a high fan; on for PETG with "
     "steep overhangs."},
    {"cooling.overhang_fan_0",
     "Es el ventilador, en porcentaje, para una pared que no se apoya nada en la capa de abajo: todo el hilo queda en "
     "el aire. Es el caso más difícil, así que suele llevar el valor más alto. Solo cuenta con «Ventilador según el "
     "apoyo» encendido.\n\nEntre este y los demás puntos el ventilador cambia en línea recta.\n\nEjemplo: 100 %.",
     "It is the fan, in percent, for a wall that does not rest on the layer below at all: the whole strand is in the "
     "air. It is the hardest case, so it usually gets the highest value. It only counts with Fan by support turned "
     "on.\n\nBetween this and the other points, the fan changes in a straight line.\n\nExample: 100 %."},
    {"cooling.overhang_fan_1",
     "Es el ventilador, en porcentaje, para una pared apoyada en un 25 % sobre la capa de abajo: tres cuartas partes "
     "del hilo quedan en el aire. Solo cuenta con «Ventilador según el apoyo» encendido.\n\nEjemplo: 80 %, un poco "
     "menos que con la pared del todo en el aire.",
     "It is the fan, in percent, for a wall that rests 25 % on the layer below: three quarters of the strand are in "
     "the air. It only counts with Fan by support turned on.\n\nExample: 80 %, a little less than for a wall fully in "
     "the air."},
    {"cooling.overhang_fan_2",
     "Es el ventilador, en porcentaje, para una pared apoyada a medias, en un 50 %, sobre la capa de abajo. Solo "
     "cuenta con «Ventilador según el apoyo» encendido.\n\nEjemplo: 60 %; si los voladizos medianos todavía cuelgan, "
     "subilo.",
     "It is the fan, in percent, for a wall resting halfway, 50 %, on the layer below. It only counts with Fan by "
     "support turned on.\n\nExample: 60 %; if medium overhangs still droop, raise it."},
    {"cooling.overhang_fan_3",
     "Es el ventilador, en porcentaje, para una pared apoyada en un 75 % sobre la capa de abajo, un voladizo leve. De "
     "ahí hasta una pared apoyada del todo, el aire extra baja en línea recta hasta 0. Solo cuenta con «Ventilador "
     "según el apoyo» encendido.\n\nEjemplo: 40 %.",
     "It is the fan, in percent, for a wall resting 75 % on the layer below, a mild overhang. From there to a fully "
     "supported wall, the extra air fades in a straight line to 0. It only counts with Fan by support turned "
     "on.\n\nExample: 40 %."},
    {"cooling.enabled",
     "Es el enfriamiento automático: el programa mira cuánto tarda cada capa, frena las que son demasiado rápidas para "
     "que alcancen a enfriarse, y ajusta el ventilador según ese tiempo.\n\nApagado, ninguna capa se frena y el "
     "ventilador ya no sigue el tiempo de capa: queda apagado, o en el mínimo si está «ventilador siempre». El "
     "ventilador de los puentes sigue funcionando. Apagarlo puede arruinar piezas chicas, que se derriten al no "
     "enfriarse.\n\nEjemplo: encendido casi siempre; apagalo solo si querés manejar vos el ventilador, con «ventilador "
     "siempre» y el mínimo.",
     "It is the automatic cooling: the program looks at how long each layer takes, slows down the ones that are too "
     "fast to cool, and sets the fan from that time.\n\nOff, no layer is slowed and the fan no longer follows the "
     "layer time: it stays off, or at the minimum with fan always on. The bridge fan still works. Turning it off can "
     "ruin small parts, which melt because they never get to cool.\n\nExample: on almost always; turn it off only if "
     "you want to drive the fan yourself, with fan always on and the minimum."},
    {"cooling.min_layer_time",
     "Cada capa necesita unos segundos para enfriarse antes de que le pongan otra encima. En una pieza chica, o en la "
     "punta de una pirámide, la capa se termina tan rápido que el plástico de abajo todavía está blando y la pieza se "
     "deforma. Este es el tiempo mínimo por capa, en segundos: si una capa tardaría menos, se imprime más "
     "lento.\n\nMás tiempo da puntas y detalles chicos más prolijos; demasiado deja la boquilla mucho rato encima de "
     "la pieza.\n\nEjemplo: 5 s por defecto; 8 a 10 s para PLA en piezas muy chicas. Si son varias piezas chicas, "
     "imprimirlas juntas también ayuda.",
     "Each layer needs a few seconds to cool before the next one goes on top. On a small part, or at the tip of a "
     "pyramid, the layer is done so quickly that the plastic below is still soft and the part deforms. This is the "
     "minimum time per layer, in seconds: if a layer would take less, it is printed slower.\n\nMore time gives cleaner "
     "tips and small details; too much leaves the nozzle sitting over the part for a long while.\n\nExample: 5 s by "
     "default; 8 to 10 s for PLA on very small parts. If you have several small parts, printing them together also "
     "helps."},
    {"cooling.min_speed",
     "Cuando una capa es demasiado corta y hay que frenarla para que llegue al tiempo mínimo, este es el límite hasta "
     "donde se intenta bajar la velocidad, en mm/s. Las líneas que ya iban más lento no cambian, y los puentes no se "
     "frenan.\n\nSi a esta velocidad la capa todavía tardaría menos que el mínimo, se baja más. Muy lento, la boquilla "
     "caliente se queda encima de la pieza y puede derretir los detalles. Viene del perfil de la impresora; si no hay, "
     "se usa 10.\n\nEjemplo: 10 mm/s para la mayoría de las piezas; 5 a 8 mm/s para piezas diminutas en PLA.",
     "When a layer is too short and has to slow down to reach the minimum time, this is how low the speed is first "
     "allowed to go, in mm/s. Lines that were already slower stay as they are, and bridges are not slowed.\n\nIf the "
     "layer would still take less than the minimum at this speed, it goes lower. Too slow, and the hot nozzle lingers "
     "over the part and can melt the details. It comes from the printer profile; without one, 10 is used.\n\nExample: "
     "10 mm/s for most parts; 5 to 8 mm/s for tiny parts in PLA."},
    {"cooling.keep_outer_speed",
     "Cuando una capa hay que frenarla para que alcance a enfriarse, con esto encendido se frena primero todo lo de "
     "adentro y la pared de afuera, la que se ve, sigue a su velocidad de siempre.\n\nAsí el plástico de la cara "
     "visible sale siempre igual de caliente y la pieza brilla parejo, sin franjas en las capas frenadas. Solo si eso "
     "no alcanza se frena también la pared de afuera. Apagado, y con la forma de frenar «Todo por igual», todo se "
     "frena parejo.\n\nEjemplo: encendido, sobre todo con PLA brillante o seda, donde se notan las franjas; apagado si "
     "preferís que todo vaya parejo.",
     "When a layer has to slow down so it can cool, with this on everything inside slows down first and the outer "
     "wall, the one you see, keeps its usual speed.\n\nThat way the plastic on the visible face always comes out "
     "equally hot and the part shines evenly, with no bands on the slowed layers. Only if that is not enough does the "
     "outer wall slow down too. Off, with Everything alike as the way of slowing down, it all slows "
     "evenly.\n\nExample: on, especially with glossy or silk PLA, where bands show; off if you prefer everything to "
     "slow evenly."},
    {"cooling.slowdown_logic",
     "Elige cómo se reparte el frenado de una capa corta. «Todo por igual» frena todo parejo, o todo menos la pared de "
     "afuera si está encendida «Pared externa a su velocidad». «Superficie pareja» mantiene la pared de afuera igual "
     "que esa opción y, además, deja también para el final la pared que toca a la de afuera.\n\nLa segunda cuida más "
     "el aspecto: las dos paredes que dan forma a la cara visible van a la misma velocidad. Sirve si todavía ves "
     "bandas en las zonas frenadas.\n\nEjemplo: todo por igual para piezas comunes; superficie pareja para piezas "
     "lisas y brillantes, como un jarrón de PLA seda.",
     "It chooses how the slowdown of a short layer is shared out. Everything alike slows it all evenly, or all but the "
     "outer wall if Outer wall at its speed is on. Consistent surface keeps the outer wall like that option does and "
     "also leaves the wall touching the outer one to slow down last.\n\nThe second looks after appearance more: the "
     "two walls that shape the visible face run at the same speed. It helps if you still see bands where layers "
     "slowed.\n\nExample: everything alike for everyday parts; consistent surface for smooth, glossy parts like a silk "
     "PLA vase."},
    {"cooling.transition_distance",
     "Solo cuenta con la forma de frenar «Superficie pareja». En una capa frenada, los últimos milímetros de extrusión "
     "vuelven a su velocidad normal, y el resto de la capa se frena un poco más para llegar igual al tiempo "
     "mínimo.\n\nAsí la capa no termina lenta. Es un ajuste fino: más milímetros vuelven antes a la velocidad normal; "
     "0 lo apaga.\n\nEjemplo: 0 por defecto; si usás superficie pareja, probá unos 10 mm y compará en la vista previa.",
     "It only matters with the Consistent surface way of slowing down. On a slowed layer, the last millimetres of "
     "extrusion go back to their normal speed, and the rest of the layer slows a little more to still reach the "
     "minimum time.\n\nThat way the layer does not end slow. It is a fine adjustment: more millimetres return to "
     "normal speed sooner; 0 turns it off.\n\nExample: 0 by default; if you use consistent surface, try about 10 mm "
     "and compare in the preview."},
    {"extrusion.flow",
     "El programa calcula cuánto plástico tiene que salir en cada línea según su ancho, su alto y su largo. Este "
     "ajuste multiplica esa cantidad, en porcentaje.\n\nSi sale de más, las superficies quedan con rebabas, ásperas y "
     "las piezas un poco más grandes; si sale de menos, quedan huecos entre las líneas y la pieza es más débil. Cada "
     "rollo puede necesitar un toque distinto.\n\nEjemplo: 100 % para empezar; lo normal es entre 95 y 102 %. La "
     "calibración de flujo te da el valor justo para tu rollo.",
     "The program works out how much plastic each line needs from its width, height and length. This setting "
     "multiplies that amount, in percent.\n\nWith too much, surfaces get ridges and burrs, feel rough and parts come "
     "out a little large; with too little, gaps appear between lines and the part is weaker. Each spool may need a "
     "slightly different value.\n\nExample: 100 % to start; the usual range is 95 to 102 %. The flow calibration gives "
     "you the right value for your spool."},
    {"extrusion.bridge_flow_ratio",
     "En los puentes y en las paredes que quedan en el aire, el hilo sale redondo, sin nada que lo aplaste. Este "
     "ajuste multiplica cuánto plástico lleva ese hilo: con menos, el hilo es más fino y las líneas del puente van más "
     "juntas.\n\nUn hilo más fino pesa menos y cuelga menos, pero si es demasiado fino se corta. Más de 1 lo hace más "
     "grueso. Tocalo si los puentes salen caídos.\n\nEjemplo: 1 por defecto; 0,8 a 0,9 si los puentes cuelgan; con 0,8 "
     "y una boquilla de 0,4 mm, el hilo es de unos 0,36 mm.",
     "On bridges and on walls left in the air, the strand comes out round, with nothing below to flatten it. This "
     "setting multiplies how much plastic goes into that strand: with less, it is thinner and the bridge lines sit "
     "closer together.\n\nA thinner strand weighs less and droops less, but too thin and it snaps. Above 1 makes it "
     "thicker. Touch it if bridges sag.\n\nExample: 1 by default; 0.8 to 0.9 if bridges droop; at 0.8 with a 0.4 mm "
     "nozzle, the strand is about 0.36 mm."},
    {"extrusion.purge_multiplier",
     "Con varios filamentos y una sola boquilla, al cambiar de color hay que tirar un poco de plástico en la torre de "
     "purga hasta que el color nuevo salga limpio. Este ajuste es qué porcentaje de esa purga pide este filamento al "
     "entrar. Solo cuenta si imprimís con más de un filamento.\n\nMás purga da colores más limpios y gasta más "
     "plástico. Se puede dar un valor para todos o uno por extrusor.\n\nEjemplo: 100 % por defecto; 120 a 150 % para "
     "un blanco o un transparente, que muestran cualquier resto del color anterior; 70 a 80 % para un negro, que lo "
     "tapa.",
     "With several filaments and one nozzle, a colour change means dumping some plastic on the purge tower until the "
     "new colour comes out clean. This setting is what percentage of that purge this filament asks for when it comes "
     "in. It only matters if you print with more than one filament.\n\nMore purge gives cleaner colours and wastes "
     "more plastic. You can give one value for all or one per extruder.\n\nExample: 100 % by default; 120 to 150 % for "
     "white or clear, which show any trace of the previous colour; 70 to 80 % for black, which hides it."},
    {"extrusion.min_purge_on_tower",
     "Al cambiar de filamento se puede aprovechar la purga: en vez de tirarla toda en la torre, el filamento nuevo "
     "imprime primero el relleno de adentro de algunas piezas, donde no se ve el color mezclado. Este ajuste es lo que "
     "igual se purga en la torre antes de eso, en mm³.\n\nSolo cuenta con varios filamentos en una boquilla y con "
     "piezas marcadas para recibir la purga. La torre recibe primero este mínimo y después solo lo que "
     "falte.\n\nEjemplo: 0 por defecto; unos 15 mm³ para que el plástico recién cargado se estabilice en la torre "
     "antes de pasar a la pieza.",
     "When switching filament, the purge can be put to use: instead of dumping it all on the tower, the new filament "
     "first prints the hidden infill of some parts, where the mixed colour does not show. This setting is how much is "
     "still purged on the tower before that, in mm³.\n\nIt only matters with several filaments in one nozzle and parts "
     "marked to take the purge. The tower gets this minimum first and then only whatever is left.\n\nExample: 0 by "
     "default; about 15 mm³ so the freshly loaded plastic settles on the tower before moving to the part."},
    {"extrusion.flow_by_speed",
     "A mucha velocidad, la boquilla no siempre alcanza a derretir todo el plástico que se le pide, y sale un poco "
     "menos. Esta lista corrige el flujo según la velocidad de cada tramo: pares de velocidad, en mm/s, y porcentaje "
     "de flujo.\n\nEntre dos puntos el valor cambia en línea recta; más allá del primero o del último, vale el del "
     "extremo. Vacía, no corrige nada. Tocala solo si ves que las partes rápidas salen más flacas que las "
     "lentas.\n\nEjemplo: 20 mm/s con 100 % y 100 mm/s con 104 %: a 60 mm/s se empuja un 102 %.",
     "At high speed, the nozzle cannot always melt all the plastic it is asked for, and a little less comes out. This "
     "list corrects the flow by the speed of each move: pairs of speed, in mm/s, and flow percentage.\n\nBetween two "
     "points the value changes in a straight line; beyond the first or the last, the end value applies. Empty corrects "
     "nothing. Only touch it if the fast parts come out thinner than the slow ones.\n\nExample: 20 mm/s at 100 % and "
     "100 mm/s at 104 %: at 60 mm/s it pushes 102 %."},
    {"extrusion.section",
     "Para saber cuánto plástico empujar, el programa imagina el corte de cada línea impresa. Este ajuste elige esa "
     "forma: «Redondeada», un rectángulo con los costados redondos, como sale de verdad el plástico aplastado; o "
     "«Rectangular», con los costados rectos.\n\nCon rectangular se empuja algo más de plástico por línea. El ancho y "
     "la separación de las líneas no cambian, solo la cantidad.\n\nEjemplo: redondeada para casi todo; con líneas de "
     "0,45 mm y capas de 0,2 mm, rectangular empuja cerca de un 10 % más.",
     "To know how much plastic to push, the program pictures the cross-section of each printed line. This setting "
     "picks that shape: rounded, a rectangle with round sides, like squashed plastic really comes out; or rectangle, "
     "with straight sides.\n\nRectangle pushes a bit more plastic per line. Line width and spacing do not change, only "
     "the amount.\n\nExample: rounded for nearly everything; with 0.45 mm lines and 0.2 mm layers, rectangle pushes "
     "about 10 % more."},
    {"extrusion.feedstock",
     "Dice para qué tipo de impresora es este material: de filamento, el hilo enrollado en un carrete que usan casi "
     "todas, o de pellets, bolitas de plástico que un tornillo empuja y derrite.\n\nSi no coincide con la impresora "
     "elegida, aparece un aviso, pero se lamina igual. Sirve para no mandar por error un perfil de pellets a una "
     "impresora de filamento.\n\nEjemplo: filamento para cualquier rollo común; pellets solo si tenés una impresora "
     "que se alimenta con bolitas.",
     "It says what kind of printer this material is for: filament, the thread wound on a spool that almost every "
     "printer uses, or pellets, small plastic beads that a screw pushes and melts.\n\nIf it does not match the chosen "
     "printer, you get a warning, but it slices anyway. It keeps you from sending a pellet profile to a filament "
     "printer by mistake.\n\nExample: filament for any regular spool; pellets only if you have a printer fed with "
     "beads."},
    {"extrusion.pressure_advance",
     "El plástico derretido dentro de la boquilla se comporta como un resorte: cuando la boquilla acelera, sale un "
     "poco tarde, y cuando frena, sigue saliendo. Eso deja esquinas abultadas y comienzos de línea flacos. El avance "
     "de presión le pide al firmware de la impresora que se adelante para compensarlo.\n\nPoco deja las esquinas "
     "gordas; mucho las deja finas o con huecos. 0 no escribe nada y queda lo que tenga configurado la "
     "impresora.\n\nEjemplo: con el motor encima de la boquilla suele andar entre 0,02 y 0,1; con tubo largo (Bowden), "
     "entre 0,3 y 1. La calibración de avance de presión te da el número para tu rollo.",
     "Melted plastic inside the nozzle behaves like a spring: when the nozzle speeds up, it comes out a little late, "
     "and when it slows down, it keeps flowing. That leaves bulging corners and thin starts of lines. Pressure advance "
     "asks the printer's firmware to act ahead of time to make up for it.\n\nToo little leaves fat corners; too much "
     "makes them thin or gappy. 0 writes nothing and keeps whatever the printer has set.\n\nExample: with the motor "
     "right over the nozzle it usually lands between 0.02 and 0.1; with a long tube (Bowden), between 0.3 and 1. The "
     "pressure advance calibration gives you the number for your spool."},
    {"extrusion.max_flow",
     "La boquilla solo puede derretir una cierta cantidad de plástico por segundo. Este ajuste es ese límite para este "
     "filamento, en mm³/s: ninguna línea se imprime tan rápido que lo pase, y si hace falta se baja la "
     "velocidad.\n\nSi la impresora también tiene su propio límite, vale el menor de los dos. Muy alto, en las partes "
     "rápidas el plástico sale a medio derretir y las paredes quedan flojas; muy bajo, imprime más lento de lo "
     "necesario. 0 no pone límite propio.\n\nEjemplo: con una boquilla común, PLA 12 a 15 mm³/s, PETG 8 a 10, TPU 3 a "
     "5.",
     "The nozzle can only melt so much plastic per second. This setting is that limit for this filament, in mm³/s: no "
     "line is printed so fast that it goes over it, and the speed is lowered if needed.\n\nIf the printer has its own "
     "limit too, the lower of the two applies. Too high, and on fast sections the plastic comes out half melted and "
     "walls turn weak; too low, and it prints slower than it needs to. 0 sets no limit of its own.\n\nExample: with a "
     "standard hot end, PLA 12 to 15 mm³/s, PETG 8 to 10, TPU 3 to 5."},
    {"extrusion.infill_max_speed",
     "Es la velocidad máxima del relleno de adentro con este filamento, en mm/s, para los dibujos cuyas líneas no se "
     "cruzan dentro de una misma capa, como líneas, giroide o panal de abejas.\n\nAlgunos materiales no aguantan el "
     "relleno a toda velocidad: los flexibles se traban y otros salen débiles. Con un tope acá, el relleno de ese "
     "material nunca va más rápido, aunque el perfil de impresión pida más. 0 no pone tope.\n\nEjemplo: 0 para PLA; 30 "
     "a 40 mm/s para TPU.",
     "It is the top speed of the inner infill with this filament, in mm/s, for patterns whose lines do not cross "
     "within a layer, like lines, gyroid or honeycomb.\n\nSome materials cannot handle infill at full speed: flexible "
     "ones jam and others come out weak. With a cap here, that material's infill never goes faster, even if the print "
     "profile asks for more. 0 sets no cap.\n\nExample: 0 for PLA; 30 to 40 mm/s for TPU."},
    {"extrusion.infill_max_crossing_speed",
     "Lo mismo que el tope de velocidad del relleno, pero para los dibujos cuyas líneas se cruzan dentro de la misma "
     "capa: cuadrícula, triángulos, estrellas y los cúbicos.\n\nEn cada cruce la boquilla pasa por encima de una línea "
     "recién puesta, y a mucha velocidad puede engancharla o golpearla; por eso a veces conviene un tope más bajo que "
     "el de los dibujos sin cruces. 0 no pone tope.\n\nEjemplo: 0 para PLA; si con este material la boquilla golpea el "
     "relleno en cuadrícula, probá 100 mm/s, o 30 mm/s para TPU.",
     "The same as the infill speed cap, but for patterns whose lines cross within the same layer: grid, triangles, "
     "stars and the cubic ones.\n\nAt each crossing the nozzle passes over a freshly laid line, and at high speed it "
     "can catch or knock it; that is why a lower cap than for non-crossing patterns sometimes helps. 0 sets no "
     "cap.\n\nExample: 0 for PLA; if with this material the nozzle knocks the grid infill, try 100 mm/s, or 30 mm/s "
     "for TPU."},
    {"extrusion.retraction",
     "La retracción (tirar el filamento hacia atrás antes de viajar sin imprimir, para que no gotee) y los viajes se "
     "configuran en la impresora. Acá podés darle a este filamento sus propios valores, que reemplazan a los de la "
     "impresora solo cuando imprime este material.\n\nSe puede cambiar, entre otros, el largo y la velocidad de la "
     "retracción, el salto de la boquilla y la limpieza. Lo que no pongas sigue con el valor de la impresora; un valor "
     "puesto, aunque sea 0, manda.\n\nEjemplo: si el PETG te deja hilos y el PLA no, ponele al PETG una retracción "
     "algo más larga, como 1,2 mm, sin tocar la de la impresora.",
     "Retraction (pulling the filament back before a non-printing travel so it does not ooze) and travel moves are set "
     "in the printer. Here you can give this filament its own values, which replace the printer's only while this "
     "material prints.\n\nYou can change, among others, the retraction length and speed, the nozzle lift and the wipe. "
     "Whatever you leave out keeps the printer's value; a value you set, even 0, takes over.\n\nExample: if PETG "
     "leaves strings and PLA does not, give PETG a slightly longer retraction, like 1.2 mm, without touching the "
     "printer's."},
    {"extrusion.loading_speed",
     "Algunas impresoras cambian de filamento solas con un solo cabezal: sacan un filamento y meten otro. Este ajuste "
     "es la velocidad a la que se empuja el filamento nuevo en la mayor parte de la carga, en mm/s. Solo cuenta en "
     "impresoras de un cabezal con cambio automático de filamento.\n\nMás rápido hace cambios más cortos; demasiado "
     "rápido puede hacer que el filamento se trabe o patine.\n\nEjemplo: 28 mm/s por defecto; bajalo a 20 si la carga "
     "falla a veces.",
     "Some printers change filament by themselves with a single head: they pull one filament out and push another in. "
     "This setting is the speed the new filament is pushed at for most of the load, in mm/s. It only matters on "
     "single-head printers with automatic filament change.\n\nFaster makes shorter changes; too fast can make the "
     "filament jam or slip.\n\nExample: 28 mm/s by default; drop it to 20 if loading sometimes fails."},
    {"extrusion.loading_speed_start",
     "Es la velocidad lenta de la carga del filamento, en mm/s: la del primer tramo, para que los engranajes agarren "
     "bien la punta, y la del último, cuando el filamento llega a la boquilla. En el medio va a la velocidad de carga "
     "normal. Solo cuenta en impresoras de un cabezal con cambio automático de filamento.\n\nEjemplo: 3 mm/s por "
     "defecto; un poco más si los cambios tardan demasiado y la carga nunca falla.",
     "It is the slow speed of the filament load, in mm/s: the one for the first stretch, so the gears get a good grip "
     "on the tip, and for the last, when the filament reaches the nozzle. In between it runs at the normal loading "
     "speed. It only matters on single-head printers with automatic filament change.\n\nExample: 3 mm/s by default; a "
     "little more if changes take too long and loading never fails."},
    {"extrusion.unloading_speed",
     "Al cambiar de filamento, el que sale se tira hacia atrás hasta sacarlo de la boquilla. Este ajuste es la "
     "velocidad de la mayor parte de esa descarga, en mm/s; el último tramo va más lento. Solo cuenta en impresoras de "
     "un cabezal con cambio automático de filamento.\n\nMás rápido acorta el cambio; demasiado rápido puede cortar el "
     "filamento o dejarle una punta deforme que después no entra.\n\nEjemplo: 90 mm/s por defecto; bajalo a 60 si las "
     "puntas salen con hilos o se traban.",
     "When switching filament, the outgoing one is pulled back until it leaves the nozzle. This setting is the speed "
     "for most of that unload, in mm/s; the last stretch goes slower. It only matters on single-head printers with "
     "automatic filament change.\n\nFaster shortens the change; too fast can snap the filament or leave it a misshapen "
     "tip that will not go back in.\n\nExample: 90 mm/s by default; drop it to 60 if tips come out stringy or get "
     "stuck."},
    {"extrusion.unloading_speed_start",
     "Es la velocidad de los primeros 15 mm de la descarga, en mm/s: el tirón inicial que despega el filamento del "
     "plástico derretido. Después sigue a la velocidad de descarga normal. Solo cuenta en impresoras de un cabezal con "
     "cambio automático de filamento.\n\nUn tirón rápido ayuda a que el filamento salga con una punta "
     "limpia.\n\nEjemplo: 100 mm/s por defecto; bajalo si el filamento se corta al empezar a descargar.",
     "It is the speed of the first 15 mm of the unload, in mm/s: the initial pull that separates the filament from the "
     "melted plastic. After that it continues at the normal unloading speed. It only matters on single-head printers "
     "with automatic filament change.\n\nA quick pull helps the filament leave with a clean tip.\n\nExample: 100 mm/s "
     "by default; lower it if the filament snaps as unloading starts."},
    {"extrusion.cooling_moves",
     "Después de sacar el filamento de la boquilla, su punta queda blanda. Antes de guardarlo, la impresora lo mueve "
     "hacia adelante y hacia atrás dentro de un tubo de enfriamiento para que la punta se endurezca bien formada. Este "
     "ajuste es cuántas idas y vueltas hace. Solo cuenta en impresoras de un cabezal con cambio automático de "
     "filamento.\n\nMás movimientos dan puntas más prolijas y un cambio más largo. 0 no hace ninguno.\n\nEjemplo: 4 "
     "por defecto; 5 o 6 si las puntas salen con un hilo o engrosadas.",
     "After the filament leaves the nozzle, its tip is still soft. Before parking it, the printer moves it back and "
     "forth inside a cooling tube so the tip hardens in a good shape. This setting is how many back-and-forth moves it "
     "makes. It only matters on single-head printers with automatic filament change.\n\nMore moves give neater tips "
     "and a longer change. 0 makes none.\n\nExample: 4 by default; 5 or 6 if tips come out with a thread or swollen."},
    {"extrusion.cooling_initial_speed",
     "Es la velocidad del filamento en el primer movimiento de enfriamiento de la punta, en mm/s. De ahí la velocidad "
     "cambia de a poco hasta la del último movimiento. Solo cuenta con movimientos de enfriamiento, en impresoras de "
     "un cabezal con cambio automático de filamento.\n\nMás lento deja más tiempo para que la punta se enfríe al "
     "principio, cuando está más blanda.\n\nEjemplo: 2,2 mm/s por defecto.",
     "It is the filament's speed on the first tip-cooling move, in mm/s. From there the speed changes gradually to "
     "that of the last move. It only matters with cooling moves, on single-head printers with automatic filament "
     "change.\n\nSlower leaves more time for the tip to cool at the start, when it is softest.\n\nExample: 2.2 mm/s by "
     "default."},
    {"extrusion.cooling_final_speed",
     "Es la velocidad del filamento en el último movimiento de enfriamiento de la punta, en mm/s. Entre el primero y "
     "el último la velocidad cambia de a poco. Solo cuenta con movimientos de enfriamiento, en impresoras de un "
     "cabezal con cambio automático de filamento.\n\nPara el final la punta ya está más firme, por eso suele ir algo "
     "más rápido que al principio.\n\nEjemplo: 3,4 mm/s por defecto.",
     "It is the filament's speed on the last tip-cooling move, in mm/s. Between the first and the last the speed "
     "changes gradually. It only matters with cooling moves, on single-head printers with automatic filament "
     "change.\n\nBy the end the tip is firmer, so it usually goes a bit faster than at the start.\n\nExample: 3.4 mm/s "
     "by default."},
    {"extrusion.ramming",
     "Justo antes de sacar el filamento en un cambio, la impresora empuja plástico con fuerza sobre la torre de purga "
     "durante unos segundos. Esa compresión da forma a la punta para que salga limpia. Este ajuste es una lista de "
     "caudales, en mm³/s, uno por cada cuarto de segundo. Solo cuenta en impresoras de un cabezal con cambio "
     "automático de filamento, con la torre de purga.\n\nMás caudal o más pasos forman una punta más gruesa y firme; "
     "demasiado puede trabarla en el tubo.\n\nEjemplo: por defecto sube de 7 a 11 mm³/s en 11 pasos, poco menos de 3 "
     "segundos. Si las puntas salen con hilos, probá alargarla un par de pasos.",
     "Right before the filament is pulled out in a change, the printer pushes plastic hard onto the purge tower for a "
     "few seconds. That ramming shapes the tip so it comes out clean. This setting is a list of flow rates, in mm³/s, "
     "one per quarter of a second. It only matters on single-head printers with automatic filament change, with the "
     "purge tower.\n\nMore flow or more steps make a thicker, firmer tip; too much can jam it in the tube.\n\nExample: "
     "by default it rises from 7 to 11 mm³/s in 11 steps, just under 3 seconds. If tips come out stringy, try adding a "
     "couple of steps."},
    {"extrusion.toolchange_delay",
     "Es una pausa, en segundos, justo después de sacar el filamento en un cambio, antes de los movimientos de "
     "enfriamiento. Solo cuenta en impresoras de un cabezal con cambio automático de filamento.\n\nLa pausa deja que "
     "la punta se enfríe y se separe del todo. Suma ese tiempo a cada cambio, así que en una pieza con muchos cambios "
     "se nota.\n\nEjemplo: 0 por defecto; 1 a 3 s si con algún material las puntas salen blandas o pegadas.",
     "It is a pause, in seconds, right after the filament is pulled out in a change, before the cooling moves. It only "
     "matters on single-head printers with automatic filament change.\n\nThe pause lets the tip cool and fully "
     "separate. It adds that time to every change, so it shows on a part with many changes.\n\nExample: 0 by default; "
     "1 to 3 s if with some material the tips come out soft or stuck."},
    {"extrusion.load_time",
     "Es cuánto tarda la impresora en cargar este filamento en un cambio, en segundos. No cambia nada del G-code: solo "
     "se suma al tiempo estimado de la impresión.\n\nSirve para impresoras con cambiadores de filamento cuyo tiempo de "
     "carga el programa no puede saber solo. Si no lo ponés, el tiempo estimado queda corto en piezas con muchos "
     "cambios.\n\nEjemplo: 0 por defecto; si cronometraste que tu cambiador tarda 20 s en cargar, poné 20.",
     "It is how long the printer takes to load this filament in a change, in seconds. It changes nothing in the "
     "G-code: it is only added to the print's estimated time.\n\nIt is for printers with filament changers whose "
     "loading time the program cannot know by itself. Without it, the estimated time falls short on parts with many "
     "changes.\n\nExample: 0 by default; if you timed your changer at 20 s to load, enter 20."},
    {"extrusion.unload_time",
     "Es cuánto tarda la impresora en descargar este filamento en un cambio, en segundos. Igual que el tiempo de "
     "carga, no cambia el G-code: solo hace más justa la estimación del tiempo total.\n\nEjemplo: 0 por defecto; si tu "
     "cambiador tarda 15 s en sacar el filamento, poné 15.",
     "It is how long the printer takes to unload this filament in a change, in seconds. Like the loading time, it does "
     "not change the G-code: it only makes the total time estimate more accurate.\n\nExample: 0 by default; if your "
     "changer takes 15 s to pull the filament out, enter 15."},
    {"extrusion.abrasive",
     "Algunos filamentos llevan fibras de carbono o de vidrio, o polvos que brillan en la oscuridad. Son abrasivos: "
     "gastan una boquilla común de latón en poco tiempo y la agrandan. Marcalo si este filamento es así.\n\nSi está "
     "marcado y la impresora no dice que tiene una boquilla endurecida (de acero o similar), aparece un aviso. El "
     "aviso no frena la exportación: es para que lo revises.\n\nEjemplo: marcado para PLA-CF, PETG-GF, nylon con fibra "
     "o PLA que brilla en la oscuridad; sin marcar para PLA, PETG o ABS comunes.",
     "Some filaments contain carbon or glass fibre, or glow-in-the-dark powder. They are abrasive: they wear out a "
     "regular brass nozzle quickly and widen it. Tick this if the filament is one of them.\n\nIf it is ticked and the "
     "printer does not say it has a hardened nozzle (steel or similar), you get a warning. The warning does not stop "
     "the export: it is there so you check.\n\nExample: ticked for PLA-CF, PETG-GF, fibre-filled nylon or "
     "glow-in-the-dark PLA; unticked for plain PLA, PETG or ABS."},
    {"extrusion.needs_enclosure",
     "Algunos materiales se curvan o se rajan si los toca el aire frío del ambiente mientras imprimen, y necesitan una "
     "impresora cerrada, con tapa o gabinete. Marcalo si este filamento lo pide.\n\nSi está marcado y la impresora no "
     "figura como cerrada, aparece un aviso al laminar. El aviso no frena la exportación: es para que lo tengas en "
     "cuenta.\n\nEjemplo: marcado para ABS, ASA, policarbonato o nylon; sin marcar para PLA, PETG o TPU.",
     "Some materials warp or crack if the cool room air reaches them while printing, and need a closed printer, with a "
     "lid or an enclosure. Tick this if the filament needs one.\n\nIf it is ticked and the printer is not listed as "
     "enclosed, you get a warning when slicing. The warning does not stop the export: it is there so you keep it in "
     "mind.\n\nExample: ticked for ABS, ASA, polycarbonate or nylon; unticked for PLA, PETG or TPU."},
