// Explanations of the machine settings for setting_guide.cpp: {key, Spanish, English},
// one entry per setting. Own texts; see PROVENANCE.md.
    {"machine.technology",
     "Dice qué clase de impresora tenés. Las de filamento (FDM) derriten un hilo de plástico y lo van dejando con una "
     "boquilla; las de resina (MSLA) endurecen resina líquida con la luz de una pantalla, capa por capa.\n\nEl "
     "programa muestra los ajustes que sirven para esa clase y prepara el archivo para ella: con resina, en lugar de "
     "instrucciones de movimiento arma un archivo de imágenes (GOO). Si lo cambiás, cambian los ajustes que "
     "ves.\n\nEjemplo: fdm para casi todas las impresoras hogareñas de filamento; msla si tu impresora usa resina y "
     "una pantalla, como una Elegoo Mars.",
     "Tells the program what kind of printer you have. Filament printers (FDM) melt a plastic thread and lay it down "
     "with a nozzle; resin printers (MSLA) harden liquid resin with the light of a screen, layer by layer.\n\nThe "
     "program shows the settings that apply to that kind and prepares the file for it: for resin, instead of movement "
     "instructions it builds a file of images (GOO). Change it and the settings you see change too.\n\nExample: fdm "
     "for almost every home filament printer; msla if your printer uses resin and a screen, like an Elegoo Mars."},
    {"machine.z_offset",
     "Es una corrección de la altura de la boquilla. El programa suma este valor a todas las alturas de la pieza en el "
     "G-code, el archivo de instrucciones que lee la impresora.\n\nUn valor positivo deja la boquilla más lejos de la "
     "cama; uno negativo, más cerca. Sirve para ajustar una primera capa que sale aplastada o que no se pega, sin "
     "tocar la impresora. No cambia el G-code de inicio.\n\nEjemplo: 0 mm si la impresora ya está bien calibrada; 0,05 "
     "mm si la primera capa sale demasiado aplastada; −0,05 mm si las líneas no se pegan a la cama.",
     "A correction to the nozzle's height. The program adds this value to every height of the part in the G-code, the "
     "instruction file the printer reads.\n\nA positive value keeps the nozzle further from the bed; a negative one "
     "brings it closer. Use it to fix a first layer that comes out squashed or does not stick, without touching the "
     "printer. It does not change the start G-code.\n\nExample: 0 mm if the printer is already well calibrated; 0.05 "
     "mm if the first layer is too squashed; -0.05 mm if the lines do not stick to the bed."},
    {"machine.enclosed",
     "Indica si tu impresora es cerrada, con paredes y tapa alrededor de la zona donde imprime.\n\nAlgunos materiales, "
     "como el ABS, necesitan aire tibio y quieto alrededor para no deformarse ni rajarse. Si el filamento que elegiste "
     "pide impresora cerrada y este ajuste está apagado, el programa te avisa. No cambia cómo se imprime.\n\nEjemplo: "
     "activalo si tu impresora tiene gabinete y tapa; dejalo apagado si la cama y la boquilla quedan al aire.",
     "Says whether your printer is enclosed, with walls and a lid around the area where it prints.\n\nSome materials, "
     "like ABS, need warm, still air around them so they do not warp or crack. If the filament you picked asks for an "
     "enclosed printer and this setting is off, the program warns you. It does not change how the part is "
     "printed.\n\nExample: turn it on if your printer has a cabinet and a lid; leave it off if the bed and nozzle are "
     "open to the room."},
    {"machine.max_chamber_temp",
     "Algunas impresoras cerradas tienen un calefactor que calienta el aire de adentro, la cámara. Este ajuste es la "
     "temperatura más alta que puede alcanzar; 0 quiere decir que la impresora no calienta la cámara.\n\nSi el "
     "filamento pide una temperatura de cámara, el programa se la manda a la impresora sin pasar de este máximo. Si la "
     "impresora no calienta la cámara o no llega a lo que pide el filamento, te avisa.\n\nEjemplo: 0 °C en casi todas "
     "las impresoras; 60 °C en una con cámara calefaccionada, o lo que diga el fabricante.",
     "Some enclosed printers have a heater that warms the air inside, the chamber. This setting is the highest "
     "temperature it can reach; 0 means the printer does not heat its chamber.\n\nIf the filament asks for a chamber "
     "temperature, the program sends it to the printer without going above this maximum. If the printer cannot heat "
     "the chamber, or not as much as the filament wants, it warns you.\n\nExample: 0 °C on almost every printer; 60 °C "
     "on one with a heated chamber, or whatever the maker says."},
    {"belt.enabled",
     "Las impresoras de cinta imprimen sobre una cinta que avanza y se lleva la pieza, con el cabezal inclinado. Así "
     "pueden hacer piezas muy largas o muchas piezas seguidas.\n\nActivalo solo si tu impresora es de cinta: el "
     "programa corta la pieza en planos inclinados, paralelos al pórtico (la estructura que lleva el cabezal), y "
     "escribe los movimientos en los ejes de esa máquina. En la cinta no hay soportes, borde, balsa ni torre de purga, "
     "y la falda no se imprime.\n\nEjemplo: apagado en una impresora común de cama; encendido en una de cinta, junto "
     "con su ángulo.",
     "Belt printers print onto a moving belt that carries the part away, with the print head tilted. That lets them "
     "make very long parts, or many parts one after another.\n\nTurn it on only if your printer has a belt: the "
     "program cuts the part into tilted planes, parallel to the gantry (the frame that carries the head), and writes "
     "the moves in that machine's axes. On a belt there are no supports, brim, raft or wipe tower, and the skirt is "
     "not printed.\n\nExample: off on an ordinary bed printer; on for a belt printer, together with its angle."},
    {"belt.angle",
     "En una impresora de cinta, es el ángulo entre la cinta y el plano en el que imprime el cabezal (el pórtico). "
     "Solo se usa con la impresora de cinta activada.\n\nTiene que coincidir con cómo está construida tu máquina; si "
     "no, la pieza sale deformada. Con un ángulo más chico, la cinta avanza más en cada capa. El motor acepta de 10° a "
     "80°.\n\nEjemplo: 45°, el valor por defecto y el de muchas impresoras de cinta; mirá el dato de la tuya.",
     "On a belt printer, this is the angle between the belt and the plane the head prints in (the gantry). It is only "
     "used with the belt printer turned on.\n\nIt has to match how your machine is built; otherwise the part comes out "
     "distorted. With a smaller angle, the belt moves further for each layer. The engine accepts 10° to "
     "80°.\n\nExample: 45°, the default and the angle of many belt printers; check yours."},
    {"belt.belt_is_z",
     "En una impresora de cinta, dice qué eje del firmware (el programa que corre dentro de la impresora) mueve la "
     "cinta. Solo se usa con la impresora de cinta activada.\n\nEncendido: la cinta es el eje Z y el pórtico el eje Y. "
     "Apagado: al revés, la cinta es Y y el pórtico Z. Si no coincide con tu máquina, la impresora mueve el eje "
     "equivocado.\n\nEjemplo: encendido, el valor por defecto; dejalo como dice el perfil de tu impresora.",
     "On a belt printer, this says which firmware axis (the firmware is the program running inside the printer) drives "
     "the belt. It is only used with the belt printer turned on.\n\nOn: the belt is the Z axis and the gantry the Y "
     "axis. Off: the other way round, the belt is Y and the gantry Z. If it does not match your machine, the printer "
     "moves the wrong axis.\n\nExample: on, the default; leave it as your printer's profile says."},
    {"machine.display_pixels_x",
     "Solo para impresoras de resina. La pantalla que ilumina la resina está hecha de puntitos, los píxeles; este "
     "ajuste es cuántos tiene a lo ancho (en X).\n\nJunto con el ancho de la pantalla, define cuánto mide cada píxel, "
     "que es el detalle más chico que se puede imprimir. Tiene que ser el de tu pantalla: si no, la pieza sale "
     "estirada o achicada.\n\nEjemplo: 4098 píxeles, el valor por defecto; con una pantalla de 143,43 mm de ancho, "
     "cada píxel mide 0,035 mm.",
     "Resin printers only. The screen that lights the resin is made of tiny dots, the pixels; this setting is how many "
     "it has across (in X).\n\nTogether with the screen's width, it sets the size of each pixel, which is the smallest "
     "detail you can print. It has to be your screen's: otherwise the part comes out stretched or shrunk.\n\nExample: "
     "4098 pixels, the default; with a screen 143.43 mm wide, each pixel is 0.035 mm."},
    {"machine.display_pixels_y",
     "Solo para impresoras de resina. Es cuántos píxeles, los puntitos de la pantalla, tiene la pantalla a lo alto (en "
     "Y).\n\nJunto con el alto de la pantalla, define cuánto mide cada píxel en esa dirección. Tiene que ser el de tu "
     "pantalla: si no, la pieza sale estirada o achicada.\n\nEjemplo: 2560 píxeles, el valor por defecto; con una "
     "pantalla de 89,6 mm de alto, cada píxel mide 0,035 mm.",
     "Resin printers only. It is how many pixels, the tiny dots of the screen, the screen has from top to bottom (in "
     "Y).\n\nTogether with the screen's height, it sets the size of each pixel in that direction. It has to be your "
     "screen's: otherwise the part comes out stretched or shrunk.\n\nExample: 2560 pixels, the default; with a screen "
     "89.6 mm tall, each pixel is 0.035 mm."},
    {"machine.display_width",
     "Solo para impresoras de resina. Es el ancho, en mm, de la zona que ilumina la pantalla: lo más ancho que puede "
     "ser una pieza.\n\nLa pieza va centrada en esa zona. Si no entra, el programa no la lamina. Con la cantidad de "
     "píxeles, también define el tamaño de cada píxel, así que un valor equivocado cambia las medidas de la "
     "pieza.\n\nEjemplo: 143,43 mm, el valor por defecto; copiá el dato de tu impresora.",
     "Resin printers only. It is the width, in mm, of the area the screen lights: the widest a part can be.\n\nThe "
     "part is centred in that area. If it does not fit, the program does not slice it. Together with the pixel count, "
     "it also sets the size of each pixel, so a wrong value changes the part's dimensions.\n\nExample: 143.43 mm, the "
     "default; copy your printer's figure."},
    {"machine.display_height",
     "Solo para impresoras de resina. Es el alto, en mm, de la zona que ilumina la pantalla, en la otra dirección "
     "(Y).\n\nIgual que con el ancho: la pieza va centrada y, si no entra, no se lamina. Con la cantidad de píxeles "
     "define el tamaño de cada píxel en esa dirección, así que tiene que ser el real.\n\nEjemplo: 89,6 mm, el valor "
     "por defecto; copiá el dato de tu impresora.",
     "Resin printers only. It is the height, in mm, of the area the screen lights, in the other direction (Y).\n\nSame "
     "as the width: the part is centred and, if it does not fit, it is not sliced. With the pixel count it sets the "
     "size of each pixel in that direction, so it has to be the real one.\n\nExample: 89.6 mm, the default; copy your "
     "printer's figure."},
    {"machine.display_mirror_x",
     "Solo para impresoras de resina. Espeja de izquierda a derecha la imagen de cada capa (las columnas de píxeles "
     "van al revés).\n\nSegún cómo está montada la pantalla, la imagen tiene que ir espejada para que la pieza salga "
     "al derecho. Si este ajuste no coincide con tu impresora, la pieza sale como vista en un espejo: un texto se lee "
     "al revés.\n\nEjemplo: encendido, el valor por defecto; cambialo solo si una pieza con letras sale espejada.",
     "Resin printers only. It mirrors each layer's image from left to right (the columns of pixels are "
     "reversed).\n\nDepending on how the screen is mounted, the image has to be mirrored for the part to come out the "
     "right way round. If this setting does not match your printer, the part comes out as seen in a mirror: text reads "
     "backwards.\n\nExample: on, the default; change it only if a part with lettering comes out mirrored."},
    {"machine.display_mirror_y",
     "Solo para impresoras de resina. Espeja de arriba abajo la imagen de cada capa (las filas de píxeles van al "
     "revés).\n\nComo el espejado en X, depende de cómo está montada la pantalla. Si no coincide con tu impresora, la "
     "pieza sale dada vuelta como en un espejo.\n\nEjemplo: apagado, el valor por defecto; cambialo solo si una pieza "
     "con letras sale espejada en esa dirección.",
     "Resin printers only. It mirrors each layer's image from top to bottom (the rows of pixels are reversed).\n\nLike "
     "the mirroring in X, it depends on how the screen is mounted. If it does not match your printer, the part comes "
     "out flipped as in a mirror.\n\nExample: off, the default; change it only if a part with lettering comes out "
     "mirrored in that direction."},
    {"review.nozzle_force",
     "El programa revisa si una pieza sin soportes va a ser estable mientras se imprime; por ejemplo, si una columna "
     "fina se va a doblar cuando la boquilla la empuja. Este ajuste es la fuerza con que la boquilla empuja de costado "
     "la capa que imprime, en newtons (N).\n\nSe mide con la pieza de calibración. Mientras está en 0 (sin medir), las "
     "columnas finas solo se informan, sin avisos. Con el valor medido, el programa puede avisarte de verdad cuáles se "
     "van a doblar.\n\nEjemplo: 0 hasta que hagas la calibración; después, el valor que te dé la prueba.",
     "The program checks whether a part without supports will stay stable while it prints; for example, whether a thin "
     "column will bend when the nozzle pushes it. This setting is how hard the nozzle pushes sideways on the layer it "
     "is printing, in newtons (N).\n\nIt is measured with the calibration part. While it is 0 (not measured), thin "
     "columns are only reported, with no warnings. With a measured value, the program can actually warn you which ones "
     "will bend.\n\nExample: 0 until you run the calibration; then the value the test gives you."},
    {"review.bed_adhesion",
     "Es cuánto aguanta la unión de la pieza con la cama, en N/mm² (fuerza por cada milímetro cuadrado apoyado), para "
     "esa cama y ese material. Lo usa la revisión de estabilidad de las piezas sin soportes.\n\nCon este valor, el "
     "programa avisa si la pieza corre riesgo de despegarse de la cama. Mientras está en 0 (sin medir), esa revisión "
     "no se hace. Un valor más alto que el real hace que el programa confíe de más.\n\nEjemplo: 0 hasta que hagas la "
     "calibración; después, el valor que te dé la prueba con tu cama y tu filamento.",
     "It is how much the bond between the part and the bed can hold, in N/mm² (force per square millimetre of "
     "contact), for that bed and that material. The stability check for parts without supports uses it.\n\nWith this "
     "value, the program warns you when the part risks coming loose from the bed. While it is 0 (not measured), that "
     "check is skipped. A value higher than the real one makes the program too trusting.\n\nExample: 0 until you run "
     "the calibration; then the value the test gives with your bed and filament."},
    {"review.max_deflection",
     "En la revisión de estabilidad de las piezas sin soportes, es cuánto se acepta que se doble la punta de una parte "
     "fina, como una columna, cuando la boquilla la empuja. Se mide en mm.\n\nUn valor más chico es más exigente: el "
     "programa marca más partes como riesgosas. Uno más grande deja pasar columnas más flexibles, que pueden salir con "
     "las capas corridas.\n\nEjemplo: 0,1 mm, el valor por defecto; 0,05 mm para piezas que tienen que salir precisas; "
     "0,3 mm si un poco de temblor no importa.",
     "In the stability check for parts without supports, this is how far the tip of a thin part, like a column, may "
     "bend when the nozzle pushes it. It is measured in mm.\n\nA smaller value is stricter: the program flags more "
     "parts as risky. A larger one lets more flexible columns through, which can come out with shifted "
     "layers.\n\nExample: 0.1 mm, the default; 0.05 mm for parts that must be accurate; 0.3 mm if a little wobble does "
     "not matter."},
    {"head.free_angle_x",
     "Sirve para las capas curvas, que no son planas. Alrededor de la boquilla está el resto del cabezal (el bloque "
     "que calienta, el ventilador): si la capa sube muy empinada, el cabezal choca con lo ya impreso. Es la pendiente "
     "más empinada, en grados, que el cabezal deja subir a lo largo de X.\n\nMás grados permiten curvas más empinadas, "
     "pero si ponés más de lo real hay choques. En 0 (sin medir) se usa la pendiente máxima de las capas "
     "curvas.\n\nEjemplo: 0 si no lo mediste; 30° para un cabezal ancho; 45° para uno con la boquilla bien saliente.",
     "This is for curved layers, the ones that are not flat. Around the nozzle sits the rest of the head (the heater "
     "block, the fan): if the layer climbs too steeply, the head hits what is already printed. It is the steepest "
     "slope, in degrees, the head allows along X.\n\nMore degrees allow steeper curves, but set it higher than the "
     "real thing and the head crashes. At 0 (not measured) the curved layers' maximum slope is used.\n\nExample: 0 if "
     "you have not measured it; 30° for a bulky head; 45° for one with a long, protruding nozzle."},
    {"head.free_angle_y",
     "Igual que el ángulo libre en X, pero a lo largo de Y: la pendiente más empinada, en grados, que una capa curva "
     "puede subir en esa dirección sin que el cabezal choque con lo impreso. Solo importa con capas curvas.\n\nMuchos "
     "cabezales no son iguales en las dos direcciones (el ventilador suele estar de un lado), por eso se da aparte. En "
     "0 (sin medir) se usa la pendiente máxima de las capas curvas.\n\nEjemplo: 0 si no lo mediste; 25° si de ese lado "
     "el cabezal es más ancho que en X.",
     "Same as the free angle in X, but along Y: the steepest slope, in degrees, a curved layer can climb in that "
     "direction without the head hitting the print. It only matters with curved layers.\n\nMany heads are not the same "
     "in both directions (the fan usually sits on one side), so it is given separately. At 0 (not measured) the curved "
     "layers' maximum slope is used.\n\nExample: 0 if you have not measured it; 25° if the head is wider on that side "
     "than in X."},
    {"head.clearance_height",
     "Para las capas curvas: es la distancia vertical, en mm, entre la punta de la boquilla y la parte más baja del "
     "cabezal que la rodea (el bloque que calienta, el ventilador).\n\nDice cuánto puede sobresalir lo ya impreso por "
     "encima de la punta sin que el cabezal lo toque. Un valor más grande que el real hace que el programa permita "
     "curvas con las que el cabezal choca.\n\nEjemplo: 3 mm, el valor por defecto; medilo con una regla desde la punta "
     "hasta lo más bajo del cabezal.",
     "For curved layers: the vertical distance, in mm, between the tip of the nozzle and the lowest part of the head "
     "around it (the heater block, the fan).\n\nIt says how far what is already printed can rise above the tip without "
     "the head touching it. A value larger than the real one lets the program allow curves the head will crash "
     "into.\n\nExample: 3 mm, the default; measure it with a ruler from the tip to the lowest part of the head."},
    {"head.clearance_radius",
     "Para las capas curvas: es la distancia horizontal, en mm, desde la boquilla hasta lo más lejano de las partes "
     "bajas del cabezal (las que mide la altura libre).\n\nJunto con esa altura, describe el espacio libre alrededor "
     "de la punta. Un radio más grande es más prudente: el programa tiene en cuenta más cabezal y evita más choques, a "
     "costa de permitir curvas menos empinadas.\n\nEjemplo: 15 mm, el valor por defecto; medilo desde el centro de la "
     "boquilla hasta el borde del bloque o del ventilador.",
     "For curved layers: the horizontal distance, in mm, from the nozzle to the furthest of the head's low parts (the "
     "ones the clearance height measures).\n\nTogether with that height, it describes the free space around the tip. A "
     "larger radius is more cautious: the program accounts for more of the head and avoids more crashes, at the cost "
     "of allowing less steep curves.\n\nExample: 15 mm, the default; measure it from the centre of the nozzle to the "
     "edge of the block or the fan."},
    {"head.profile",
     "Para las capas curvas, una forma más precisa de describir el cabezal: el ángulo libre medido a varias alturas. "
     "Cada par dice una altura en mm y el ángulo en grados que el cabezal deja libre hasta ahí.\n\nAsí el programa "
     "sabe, por ejemplo, que cerca de la punta hay mucho lugar y más arriba el cabezal se ensancha. Lo ya impreso no "
     "puede subir más que cada altura dentro de la distancia que alcanza su ángulo. Vacío: no se usa.\n\nEjemplo: "
     "«2:45;5:30» quiere decir 45° hasta 2 mm de altura y 30° hasta 5 mm.",
     "For curved layers, a more precise way to describe the head: the free angle measured at several heights. Each "
     "pair gives a height in mm and the angle in degrees the head leaves free up to there.\n\nThat way the program "
     "knows, for instance, that there is plenty of room near the tip and the head gets wider further up. What is "
     "already printed may not rise above each height within the reach of its angle. Empty: not used.\n\nExample: "
     "2:45;5:30 means 45° up to 2 mm high and 30° up to 5 mm."},
    {"machine.hardened_nozzle",
     "Indica si la boquilla de tu impresora aguanta filamentos abrasivos, los que llevan fibra de carbono o de vidrio. "
     "Esos filamentos gastan rápido una boquilla común de latón; hace falta una de acero endurecido o similar.\n\nSi "
     "elegís un filamento abrasivo y este ajuste está apagado, el programa te avisa. No cambia cómo se "
     "imprime.\n\nEjemplo: apagado con la boquilla de latón que traen casi todas las impresoras; encendido si pusiste "
     "una de acero endurecido.",
     "Says whether your printer's nozzle can handle abrasive filaments, the ones with carbon or glass fibre in them. "
     "Those filaments quickly wear out an ordinary brass nozzle; you need a hardened steel one or similar.\n\nIf you "
     "pick an abrasive filament and this setting is off, the program warns you. It does not change how the part is "
     "printed.\n\nExample: off with the brass nozzle almost every printer comes with; on if you fitted a hardened "
     "steel one."},
    {"machine.min_layer_height",
     "Es la capa más fina que tu impresora puede hacer bien, en mm. La usa la altura de capa variable, que pone capas "
     "finas donde hay detalle y gruesas donde no.\n\nCon un valor más chico, la altura variable puede afinar más las "
     "curvas suaves, pero capas demasiado finas para tu máquina salen irregulares. En 0, el programa usa 0,04 "
     "mm.\n\nEjemplo: 0 para dejar el valor de siempre; 0,08 mm si tu impresora no hace bien capas más finas que eso.",
     "It is the thinnest layer your printer can print well, in mm. Variable layer height uses it: that feature puts "
     "thin layers where there is detail and thick ones where there is none.\n\nA smaller value lets variable layers "
     "refine gentle curves more, but layers too thin for your machine come out uneven. At 0, the program uses 0.04 "
     "mm.\n\nExample: 0 to keep the usual value; 0.08 mm if your printer does not print thinner layers well."},
    {"machine.max_layer_height",
     "Es la capa más gruesa que tu impresora puede hacer, en mm. La usa la altura de capa variable, que pone capas "
     "gruesas donde la pieza no tiene detalle.\n\nNunca puede pasar del diámetro de la boquilla. Un valor más alto "
     "ahorra tiempo en las partes rectas; si es demasiado, las capas se pegan mal entre sí. En 0, el programa usa el "
     "diámetro de la boquilla.\n\nEjemplo: con una boquilla de 0,4 mm, 0,3 mm es un tope prudente; 0 deja que llegue "
     "hasta 0,4 mm.",
     "It is the thickest layer your printer can print, in mm. Variable layer height uses it: that feature puts thick "
     "layers where the part has no detail.\n\nIt can never be more than the nozzle diameter. A higher value saves time "
     "on plain sections; too high, and the layers bond poorly. At 0, the program uses the nozzle diameter.\n\nExample: "
     "with a 0.4 mm nozzle, 0.3 mm is a safe cap; 0 lets it go up to 0.4 mm."},
    {"machine.feedstock",
     "Dice con qué se alimenta la impresora: con filamento, el hilo de plástico en rollo de casi todas las impresoras, "
     "o con pellets, bolitas de plástico que un tornillo derrite y empuja. Las de pellets suelen ser máquinas "
     "grandes.\n\nEl laminado es el mismo en los dos casos. Con pellets, el diámetro del filamento pasa a ser un "
     "diámetro imaginario: el que espera el firmware de la impresora para calcular cuánto plástico sacar.\n\nEjemplo: "
     "filament para casi todas las impresoras; pellet solo si tu máquina se carga con bolitas.",
     "Says what the printer is fed with: filament, the spooled plastic thread almost every printer uses, or pellets, "
     "small plastic beads a screw melts and pushes out. Pellet printers are usually large machines.\n\nSlicing is the "
     "same either way. With pellets, the filament diameter becomes an imaginary one: the diameter the printer's "
     "firmware expects in order to work out how much plastic to push.\n\nExample: filament for almost every printer; "
     "pellet only if your machine is loaded with beads."},
    {"machine.max_flow",
     "Es cuánto plástico puede derretir por segundo la parte caliente de la boquilla (el hot end), en mm³/s. Si se le "
     "pide más, el plástico no llega a derretirse y salen líneas flacas o con huecos.\n\nNinguna línea pasa de este "
     "caudal: si hace falta, el programa baja su velocidad y te avisa en qué parte lo hizo. Un valor más alto deja "
     "imprimir más rápido; si pasa de lo real, la pieza sale con falta de plástico. En 0, no hay límite.\n\nEjemplo: 0 "
     "si no lo sabés; unos 12 mm³/s en un hot end común; 25 mm³/s o más en uno de alto caudal.",
     "It is how much plastic the hot part of the nozzle (the hot end) can melt each second, in mm³/s. Ask for more and "
     "the plastic does not melt in time, giving thin lines or gaps.\n\nNo line goes above this flow: when needed, the "
     "program slows it down and tells you where it did. A higher value lets you print faster; above the real one, the "
     "part comes out short of plastic. At 0, there is no limit.\n\nExample: 0 if you do not know it; about 12 mm³/s "
     "for an ordinary hot end; 25 mm³/s or more for a high-flow one."},
    {"machine.relative_extrusion",
     "En el G-code, cada movimiento dice también cuánto filamento empujar. Hay dos formas de contarlo: absoluta, como "
     "un cuentakilómetros que suma todo desde el principio, o relativa, donde cada movimiento dice solo lo suyo.\n\nLa "
     "pieza sale igual con las dos; lo que importa es que coincida con lo que esperan tu impresora y tu G-code de "
     "inicio. Si no coinciden, el extrusor puede empujar de más o de menos de golpe.\n\nEjemplo: apagado (absoluta), "
     "el valor por defecto; encendido si el perfil de tu impresora o su G-code de inicio piden extrusión relativa.",
     "In the G-code, each move also says how much filament to push. There are two ways to count it: absolute, like an "
     "odometer that adds everything up from the start, or relative, where each move gives only its own amount.\n\nThe "
     "part comes out the same either way; what matters is that it matches what your printer and your start G-code "
     "expect. If they do not match, the extruder can suddenly push far too much or too little.\n\nExample: off "
     "(absolute), the default; on if your printer's profile or its start G-code asks for relative extrusion."},
    {"machine.high_flow_nozzle",
     "Indica si tenés puesta una boquilla de alto caudal, una que derrite más plástico por segundo que la común. Con "
     "varios extrusores se puede dar uno por cada uno.\n\nEste ajuste no cambia el laminado: solo queda anotado en los "
     "datos del archivo, para que la impresora sepa qué boquilla esperar. Para imprimir más rápido con ella, subí "
     "además el flujo máximo.\n\nEjemplo: apagado con la boquilla de fábrica; encendido si cambiaste a una de alto "
     "caudal.",
     "Says whether you have a high-flow nozzle fitted, one that melts more plastic per second than the usual kind. "
     "With several extruders you can give one value for each.\n\nThis setting does not change the slicing: it is only "
     "recorded in the file's data, so the printer knows which nozzle to expect. To actually print faster with it, "
     "raise the maximum flow as well.\n\nExample: off with the stock nozzle; on if you switched to a high-flow one."},
    {"machine.extruders",
     "Es cuántos extrusores tiene tu impresora. El extrusor es lo que empuja el filamento; con más de uno se puede "
     "imprimir con varios colores o materiales en la misma pieza.\n\nCon más de uno, cada objeto se imprime con el "
     "extrusor que le asignes, y el programa calienta cada extrusor que se usa al empezar y los cambia capa por capa. "
     "Cada cambio lleva tiempo, así que más colores hacen la impresión más larga.\n\nEjemplo: 1 en casi todas las "
     "impresoras; 2 en una de doble boquilla; 4 en una con cambiador de 4 filamentos.",
     "It is how many extruders your printer has. The extruder is what pushes the filament; with more than one you can "
     "print several colours or materials in the same part.\n\nWith more than one, each object is printed with the "
     "extruder you assign it, and the program heats every extruder in use at the start and switches between them layer "
     "by layer. Each switch takes time, so more colours make the print longer.\n\nExample: 1 on almost every printer; "
     "2 on a dual-nozzle one; 4 on one with a 4-filament changer."},
    {"machine.extruder_offset",
     "En impresoras con varias boquillas, cada una está corrida un poco respecto de la primera. Este ajuste dice "
     "cuánto, en mm en X y en Y, para cada extrusor.\n\nEl programa corrige los movimientos de cada boquilla con ese "
     "desfase, para que todas impriman en el mismo lugar. Si está mal, los colores salen corridos entre sí. Con un "
     "solo extrusor que cambia de filamento no se usa.\n\nEjemplo: [0, 0] para la primera y algo como [25, 0] para la "
     "segunda; el número exacto, y su signo, salen de calibrar tu impresora.",
     "On printers with several nozzles, each one sits a little away from the first. This setting says how far, in mm "
     "in X and Y, for each extruder.\n\nThe program corrects each nozzle's moves by that offset, so they all print in "
     "the same place. If it is wrong, the colours come out shifted against each other. With a single extruder that "
     "swaps filaments it is not used.\n\nExample: [0, 0] for the first and something like [25, 0] for the second; the "
     "exact figure, and its sign, come from calibrating your printer."},
    {"machine.single_extruder_mmu",
     "Activalo si tu impresora tiene una sola boquilla y un aparato que le cambia el filamento (un MMU), para imprimir "
     "con varios colores.\n\nAl cambiar de color, en la boquilla queda plástico del anterior. Con esto activado, en "
     "cada cambio el programa purga (tira) una cantidad de plástico en la torre de purga, una torrecita que se imprime "
     "al lado de la pieza. Sin esto, con varias boquillas, la torre solo ceba la boquilla nueva. Purgar más limpia "
     "mejor el color, pero gasta más filamento.\n\nEjemplo: encendido con un cambiador de filamentos de una boquilla; "
     "apagado si tenés dos boquillas.",
     "Turn it on if your printer has a single nozzle and a device that swaps its filament (an MMU), so it can print in "
     "several colours.\n\nWhen the colour changes, plastic of the old one is left in the nozzle. With this on, at each "
     "change the program purges (dumps) some plastic into the wipe tower, a little tower printed beside the part. "
     "Without it, with several nozzles, the tower only primes the new nozzle. More purging cleans the colour better "
     "but uses more filament.\n\nExample: on with a single-nozzle filament changer; off if you have two nozzles."},
    {"machine.max_feed_x",
     "Es la velocidad más alta a la que se puede mover el eje X (de izquierda a derecha), en mm/s. Viene del perfil de "
     "tu impresora.\n\nEl programa la usa para estimar cuánto va a tardar la impresión: ningún movimiento de ese eje "
     "va más rápido. Si los límites se escriben en el G-code, la impresora también la respeta. Sin valor, no se sabe y "
     "no limita nada.\n\nEjemplo: 500 mm/s en muchas impresoras comunes; copiá el dato del perfil de tu impresora.",
     "It is the highest speed the X (left to right) axis can move at, in mm/s. It comes from your printer's "
     "profile.\n\nThe program uses it to estimate how long the print will take: no move on that axis goes faster. If "
     "the limits are written into the G-code, the printer keeps to it too. With no value, it is unknown and limits "
     "nothing.\n\nExample: 500 mm/s on many ordinary printers; copy the figure from your printer's profile."},
    {"machine.max_feed_y",
     "Es la velocidad más alta a la que se puede mover el eje Y (de adelante hacia atrás), en mm/s. Viene del perfil "
     "de tu impresora.\n\nEl programa la usa para estimar cuánto va a tardar la impresión: ningún movimiento de ese "
     "eje va más rápido. Si los límites se escriben en el G-code, la impresora también la respeta. Sin valor, no se "
     "sabe y no limita nada.\n\nEjemplo: 500 mm/s en muchas impresoras comunes; copiá el dato del perfil de tu "
     "impresora.",
     "It is the highest speed the Y (front to back) axis can move at, in mm/s. It comes from your printer's "
     "profile.\n\nThe program uses it to estimate how long the print will take: no move on that axis goes faster. If "
     "the limits are written into the G-code, the printer keeps to it too. With no value, it is unknown and limits "
     "nothing.\n\nExample: 500 mm/s on many ordinary printers; copy the figure from your printer's profile."},
    {"machine.max_feed_z",
     "Es la velocidad más alta a la que se puede mover el eje Z (arriba y abajo), que suele ser mucho más lento que "
     "los otros, en mm/s. Viene del perfil de tu impresora.\n\nEl programa la usa para estimar cuánto va a tardar la "
     "impresión: ningún movimiento de ese eje va más rápido. Si los límites se escriben en el G-code, la impresora "
     "también la respeta. Sin valor, no se sabe y no limita nada.\n\nEjemplo: entre 5 y 12 mm/s en muchas impresoras; "
     "copiá el dato del perfil de tu impresora.",
     "It is the highest speed the Z (up and down), which is usually much slower than the others axis can move at, in "
     "mm/s. It comes from your printer's profile.\n\nThe program uses it to estimate how long the print will take: no "
     "move on that axis goes faster. If the limits are written into the G-code, the printer keeps to it too. With no "
     "value, it is unknown and limits nothing.\n\nExample: between 5 and 12 mm/s on many printers; copy the figure "
     "from your printer's profile."},
    {"machine.max_feed_e",
     "Es la velocidad más alta a la que se puede mover el eje del extrusor (el motor que empuja el filamento), en "
     "mm/s. Viene del perfil de tu impresora.\n\nEl programa la usa para estimar cuánto va a tardar la impresión: "
     "ningún movimiento de ese eje va más rápido. Si los límites se escriben en el G-code, la impresora también la "
     "respeta. Sin valor, no se sabe y no limita nada.\n\nEjemplo: entre 25 y 120 mm/s, según la impresora; copiá el "
     "dato del perfil de tu impresora.",
     "It is the highest speed the extruder (the motor that pushes the filament) can run at, in mm/s. It comes from "
     "your printer's profile.\n\nThe program uses it to estimate how long the print will take: no move on that axis "
     "goes faster. If the limits are written into the G-code, the printer keeps to it too. With no value, it is "
     "unknown and limits nothing.\n\nExample: between 25 and 120 mm/s, depending on the printer; copy the figure from "
     "your printer's profile."},
    {"machine.corner_speed",
     "Es la velocidad a la que el cabezal puede pasar por una esquina cerrada, de 90° o más, sin frenar del todo, en "
     "mm/s. En giros más suaves puede ir más rápido, pero nunca más lento que esto.\n\nEl programa la usa para estimar "
     "el tiempo de impresión: en piezas con muchas esquinas, un valor más chico da un tiempo más largo. Viene del "
     "perfil de la impresora; si no lo trae, se usa 10 mm/s.\n\nEjemplo: 10 mm/s si no lo sabés; un valor más alto en "
     "impresoras rápidas, según su perfil.",
     "It is the speed the head can take a sharp corner at, 90° or tighter, without stopping completely, in mm/s. On "
     "gentler turns it can go faster, but never slower than this.\n\nThe program uses it to estimate the print time: "
     "on parts with many corners, a smaller value gives a longer time. It comes from the printer's profile; if the "
     "profile does not have it, 10 mm/s is used.\n\nExample: 10 mm/s if you do not know it; a higher value on fast "
     "printers, as their profile says."},
    {"machine.acceleration",
     "Es qué tan rápido gana velocidad el cabezal cuando imprime, en mm/s². Un auto con más aceleración llega antes a "
     "su velocidad; acá pasa lo mismo.\n\nMás aceleración termina antes, sobre todo en piezas chicas con muchas "
     "vueltas, pero sacude la impresora y puede dejar ondas en las paredes. Menos da superficies más limpias y tarda "
     "más. El programa la escribe en el G-code para que la impresora use la misma que supone la estimación de "
     "tiempo.\n\nEjemplo: 1000 mm/s², el valor por defecto; 500 mm/s² para una impresora que tiembla; 3000 mm/s² o más "
     "en una rápida y firme.",
     "It is how quickly the head picks up speed while printing, in mm/s². A car with more acceleration reaches its "
     "speed sooner; this works the same way.\n\nMore acceleration finishes sooner, especially on small parts with many "
     "turns, but it shakes the printer and can leave ripples on the walls. Less gives cleaner surfaces and takes "
     "longer. The program writes it into the G-code so the printer uses the same value the time estimate "
     "assumes.\n\nExample: 1000 mm/s², the default; 500 mm/s² for a printer that wobbles; 3000 mm/s² or more on a "
     "fast, sturdy one."},
    {"machine.travel_acceleration",
     "Es qué tan rápido gana velocidad el cabezal en los viajes, cuando se mueve sin imprimir, y en las retracciones, "
     "en mm/s².\n\nComo en los viajes no sale plástico, se suele usar más que al imprimir: los viajes duran menos y "
     "gotea menos plástico en el camino. Demasiada puede sacudir la impresora o hacerle perder pasos a los motores, y "
     "la pieza sale corrida.\n\nEjemplo: 1250 mm/s², el valor por defecto; 2000 a 5000 mm/s² en impresoras rápidas.",
     "It is how quickly the head picks up speed on travels, when it moves without printing, and on retractions, in "
     "mm/s².\n\nSince no plastic comes out while travelling, it is usually higher than for printing: travels are "
     "shorter and less plastic oozes on the way. Too much can shake the printer or make the motors skip steps, and the "
     "part comes out shifted.\n\nExample: 1250 mm/s², the default; 2000 to 5000 mm/s² on fast printers."},
    {"machine.retract_acceleration",
     "Es qué tan rápido arranca el extrusor cuando tira el filamento hacia atrás para que no gotee (la retracción), en "
     "mm/s².\n\nMás aceleración hace retracciones más cortas y secas; demasiada puede hacer que el motor del extrusor "
     "pierda pasos. En 0, se usa la aceleración de los viajes.\n\nEjemplo: 0 para usar la de los viajes; 1000 mm/s² en "
     "una impresora con tubo largo (Bowden); 5000 mm/s² o más en una con el motor encima de la boquilla.",
     "It is how quickly the extruder gets going when it pulls the filament back to stop oozing (the retraction), in "
     "mm/s².\n\nMore acceleration gives shorter, crisper retractions; too much can make the extruder motor skip steps. "
     "At 0, the travel acceleration is used.\n\nExample: 0 to use the travel one; 1000 mm/s² on a printer with a long "
     "tube (Bowden); 5000 mm/s² or more on one with the motor over the nozzle."},
    {"machine.max_accel_x",
     "Es la aceleración más alta que admite el eje X (de izquierda a derecha), en mm/s²: qué tan rápido puede ganar "
     "velocidad.\n\nNingún movimiento acelera más de lo que permite cada eje que mueve. El programa lo usa para "
     "estimar el tiempo, y si los límites se escriben en el G-code, también lo recibe la impresora. En 0, no se sabe y "
     "no se usa.\n\nEjemplo: 500 a 3000 mm/s² en muchas impresoras comunes; copiá el dato del perfil de tu impresora.",
     "It is the highest acceleration the X (left to right) axis accepts, in mm/s²: how quickly it can pick up "
     "speed.\n\nNo move accelerates more than each axis it moves allows. The program uses it to estimate the time, and "
     "if the limits are written into the G-code, the printer gets it too. At 0, it is unknown and not "
     "used.\n\nExample: 500 to 3000 mm/s² on many ordinary printers; copy the figure from your printer's profile."},
    {"machine.max_accel_y",
     "Es la aceleración más alta que admite el eje Y (de adelante hacia atrás), en mm/s²: qué tan rápido puede ganar "
     "velocidad.\n\nNingún movimiento acelera más de lo que permite cada eje que mueve. El programa lo usa para "
     "estimar el tiempo, y si los límites se escriben en el G-code, también lo recibe la impresora. En 0, no se sabe y "
     "no se usa.\n\nEjemplo: 500 a 3000 mm/s² en muchas impresoras comunes; copiá el dato del perfil de tu impresora.",
     "It is the highest acceleration the Y (front to back) axis accepts, in mm/s²: how quickly it can pick up "
     "speed.\n\nNo move accelerates more than each axis it moves allows. The program uses it to estimate the time, and "
     "if the limits are written into the G-code, the printer gets it too. At 0, it is unknown and not "
     "used.\n\nExample: 500 to 3000 mm/s² on many ordinary printers; copy the figure from your printer's profile."},
    {"machine.max_accel_z",
     "Es la aceleración más alta que admite el eje Z (arriba y abajo), en mm/s²: qué tan rápido puede ganar "
     "velocidad.\n\nNingún movimiento acelera más de lo que permite cada eje que mueve. El programa lo usa para "
     "estimar el tiempo, y si los límites se escriben en el G-code, también lo recibe la impresora. En 0, no se sabe y "
     "no se usa.\n\nEjemplo: 100 a 500 mm/s², porque Z es más pesado y lento; copiá el dato del perfil de tu "
     "impresora.",
     "It is the highest acceleration the Z (up and down) axis accepts, in mm/s²: how quickly it can pick up "
     "speed.\n\nNo move accelerates more than each axis it moves allows. The program uses it to estimate the time, and "
     "if the limits are written into the G-code, the printer gets it too. At 0, it is unknown and not "
     "used.\n\nExample: 100 to 500 mm/s², since Z is heavier and slower; copy the figure from your printer's profile."},
    {"machine.max_accel_e",
     "Es la aceleración más alta que admite el eje del extrusor, en mm/s²: qué tan rápido puede ganar "
     "velocidad.\n\nNingún movimiento acelera más de lo que permite cada eje que mueve. El programa lo usa para "
     "estimar el tiempo, y si los límites se escriben en el G-code, también lo recibe la impresora. En 0, no se sabe y "
     "no se usa.\n\nEjemplo: 5000 mm/s² o más en muchas impresoras; copiá el dato del perfil de tu impresora.",
     "It is the highest acceleration the extruder accepts, in mm/s²: how quickly it can pick up speed.\n\nNo move "
     "accelerates more than each axis it moves allows. The program uses it to estimate the time, and if the limits are "
     "written into the G-code, the printer gets it too. At 0, it is unknown and not used.\n\nExample: 5000 mm/s² or "
     "more on many printers; copy the figure from your printer's profile."},
    {"machine.jerk_x",
     "El tirón (jerk) del eje X es cuánto puede cambiar su velocidad de golpe, sin frenar ni acelerar de a poco, en "
     "mm/s. Se nota sobre todo en las esquinas.\n\nMás tirón pasa las esquinas más rápido, pero sacude la impresora y "
     "puede dejar marcas. Menos es más suave y más lento. El programa lo usa para estimar el tiempo, y lo escribe en "
     "el G-code si los límites se escriben. En 0, no se sabe.\n\nEjemplo: 8 a 10 mm/s en muchas impresoras; dejalo en "
     "0 si tu impresora usa la desviación de unión.",
     "The X axis's jerk is how much its speed can change all at once, without easing on or off, in mm/s. It shows "
     "mostly at corners.\n\nMore jerk takes corners faster but shakes the printer and can leave marks. Less is "
     "smoother and slower. The program uses it to estimate the time, and writes it into the G-code when the limits are "
     "written. At 0, it is unknown.\n\nExample: 8 to 10 mm/s on many printers; leave it at 0 if your printer uses "
     "junction deviation."},
    {"machine.jerk_y",
     "El tirón (jerk) del eje Y es cuánto puede cambiar su velocidad de golpe, sin frenar ni acelerar de a poco, en "
     "mm/s. Se nota sobre todo en las esquinas.\n\nMás tirón pasa las esquinas más rápido, pero sacude la impresora y "
     "puede dejar marcas. Menos es más suave y más lento. El programa lo usa para estimar el tiempo, y lo escribe en "
     "el G-code si los límites se escriben. En 0, no se sabe.\n\nEjemplo: 8 a 10 mm/s en muchas impresoras; dejalo en "
     "0 si tu impresora usa la desviación de unión.",
     "The Y axis's jerk is how much its speed can change all at once, without easing on or off, in mm/s. It shows "
     "mostly at corners.\n\nMore jerk takes corners faster but shakes the printer and can leave marks. Less is "
     "smoother and slower. The program uses it to estimate the time, and writes it into the G-code when the limits are "
     "written. At 0, it is unknown.\n\nExample: 8 to 10 mm/s on many printers; leave it at 0 if your printer uses "
     "junction deviation."},
    {"machine.jerk_z",
     "El tirón (jerk) del eje Z es cuánto puede cambiar su velocidad de golpe, sin frenar ni acelerar de a poco, en "
     "mm/s. Se nota sobre todo en las esquinas.\n\nMás tirón pasa las esquinas más rápido, pero sacude la impresora y "
     "puede dejar marcas. Menos es más suave y más lento. El programa lo usa para estimar el tiempo, y lo escribe en "
     "el G-code si los límites se escriben. En 0, no se sabe.\n\nEjemplo: 0,3 a 0,6 mm/s, porque Z se mueve poco y "
     "lento; dejalo en 0 si tu impresora usa la desviación de unión.",
     "The Z axis's jerk is how much its speed can change all at once, without easing on or off, in mm/s. It shows "
     "mostly at corners.\n\nMore jerk takes corners faster but shakes the printer and can leave marks. Less is "
     "smoother and slower. The program uses it to estimate the time, and writes it into the G-code when the limits are "
     "written. At 0, it is unknown.\n\nExample: 0.3 to 0.6 mm/s, since Z moves little and slowly; leave it at 0 if "
     "your printer uses junction deviation."},
    {"machine.jerk_e",
     "El tirón (jerk) del extrusor es cuánto puede cambiar su velocidad de golpe, sin frenar ni acelerar de a poco, en "
     "mm/s. Se nota sobre todo en las esquinas.\n\nMás tirón pasa las esquinas más rápido, pero sacude la impresora y "
     "puede dejar marcas. Menos es más suave y más lento. El programa lo usa para estimar el tiempo, y lo escribe en "
     "el G-code si los límites se escriben. En 0, no se sabe.\n\nEjemplo: unos 5 mm/s en muchas impresoras; dejalo en "
     "0 si tu impresora usa la desviación de unión.",
     "The extruder's jerk is how much its speed can change all at once, without easing on or off, in mm/s. It shows "
     "mostly at corners.\n\nMore jerk takes corners faster but shakes the printer and can leave marks. Less is "
     "smoother and slower. The program uses it to estimate the time, and writes it into the G-code when the limits are "
     "written. At 0, it is unknown.\n\nExample: about 5 mm/s on many printers; leave it at 0 if your printer uses "
     "junction deviation."},
    {"machine.junction_deviation",
     "Es otra forma de decir qué tan rápido el cabezal pasa por las esquinas, que usan las versiones nuevas del "
     "firmware Marlin en lugar del tirón (jerk). Se mide en mm: imaginá que la esquina se redondea con un arco a esa "
     "distancia del vértice.\n\nUn valor más grande pasa las esquinas más rápido, con más sacudón; uno más chico, más "
     "despacio y más prolijo. El programa lo usa para estimar el tiempo. En 0, no se usa.\n\nEjemplo: 0 si tu "
     "impresora usa tirón; 0,013 a 0,05 mm en una con Marlin que usa desviación de unión.",
     "It is another way of saying how fast the head goes through corners, used by newer versions of the Marlin "
     "firmware instead of jerk. It is measured in mm: picture the corner rounded off by an arc at that distance from "
     "the point.\n\nA larger value takes corners faster, with more shaking; a smaller one, slower and neater. The "
     "program uses it to estimate the time. At 0, it is not used.\n\nExample: 0 if your printer uses jerk; 0.013 to "
     "0.05 mm on a Marlin printer that uses junction deviation."},
    {"machine.min_extrude_feed",
     "Es la velocidad más baja a la que la impresora hace un movimiento que imprime, en mm/s. Ningún movimiento "
     "extruyendo va más lento que esto.\n\nSirve para que la impresora no se arrastre demasiado lenta en tramos muy "
     "cortos. El programa lo usa para estimar el tiempo, y lo escribe en el G-code si los límites se escriben. En 0, "
     "no hay mínimo.\n\nEjemplo: 0, el valor por defecto; copiá el dato del perfil de tu impresora si lo trae.",
     "It is the lowest speed the printer makes a printing move at, in mm/s. No move that extrudes goes slower than "
     "this.\n\nIt keeps the printer from crawling too slowly on very short segments. The program uses it to estimate "
     "the time, and writes it into the G-code when the limits are written. At 0, there is no minimum.\n\nExample: 0, "
     "the default; copy your printer profile's figure if it has one."},
    {"machine.min_travel_feed",
     "Es la velocidad más baja de los viajes, los movimientos en los que la boquilla se mueve sin imprimir, en mm/s. "
     "Ningún viaje va más lento que esto.\n\nEl programa lo usa para estimar el tiempo, y lo escribe en el G-code si "
     "los límites se escriben. En 0, no hay mínimo.\n\nEjemplo: 0, el valor por defecto; copiá el dato del perfil de "
     "tu impresora si lo trae.",
     "It is the lowest speed of travels, the moves where the nozzle goes somewhere without printing, in mm/s. No "
     "travel goes slower than this.\n\nThe program uses it to estimate the time, and writes it into the G-code when "
     "the limits are written. At 0, there is no minimum.\n\nExample: 0, the default; copy your printer profile's "
     "figure if it has one."},
    {"machine.silent_mode",
     "Algunas impresoras tienen un modo silencioso, que mueve los motores más despacio para hacer menos ruido. "
     "Activalo si la tuya lo tiene.\n\nCon esto, el G-code lleva una segunda estimación de tiempo, la del modo "
     "silencioso, calculada con los límites del ajuste de modo silencioso. Así la impresora puede mostrar el tiempo "
     "que corresponde al modo en que está. No cambia los movimientos.\n\nEjemplo: apagado en la mayoría de las "
     "impresoras; encendido si la tuya tiene un modo silencioso en su menú.",
     "Some printers have a silent mode, which runs the motors more gently to make less noise. Turn this on if yours "
     "has one.\n\nWith it, the G-code carries a second time estimate, the silent mode's, worked out with the limits in "
     "the silent mode setting. That way the printer can show the time that matches the mode it is in. It does not "
     "change the moves.\n\nExample: off on most printers; on if yours has a silent mode in its menu."},
    {"machine.silent",
     "Son los límites de movimiento de la impresora en modo silencioso: velocidades máximas, aceleraciones, tirones y "
     "demás, con los mismos nombres que los normales. Solo se usa con el modo silencioso activado.\n\nEl que no pongas "
     "vale lo mismo que el normal. Estos valores solo cambian la segunda estimación de tiempo; lo que se escribe en el "
     "G-code son siempre los límites normales.\n\nEjemplo: una aceleración de 1000 mm/s² en lugar de los 3000 "
     "normales, y la velocidad máxima en X e Y a la mitad.",
     "These are the printer's motion limits in silent mode: top speeds, accelerations, jerks and the rest, with the "
     "same names as the normal ones. It is only used with silent mode turned on.\n\nAny one you leave out is the same "
     "as the normal one. These values only change the second time estimate; what is written into the G-code is always "
     "the normal limits.\n\nExample: an acceleration of 1000 mm/s² instead of the normal 3000, and half the top speed "
     "in X and Y."},
    {"machine.max_nozzle_temp",
     "Es la temperatura más alta que aguanta la boquilla de tu impresora, en °C.\n\nSi la temperatura del filamento, o "
     "un cambio de temperatura a cierta altura, pide más que esto, el programa te avisa: calentar de más puede dañar "
     "la boquilla o sus piezas plásticas. En 0, no se sabe y no se revisa.\n\nEjemplo: 260 °C en muchas impresoras "
     "comunes; 300 °C en las que imprimen materiales técnicos.",
     "It is the highest temperature your printer's nozzle can take, in °C.\n\nIf the filament's temperature, or a "
     "temperature change at some height, asks for more than this, the program warns you: overheating can damage the "
     "nozzle or the plastic parts around it. At 0, it is unknown and not checked.\n\nExample: 260 °C on many ordinary "
     "printers; 300 °C on those that print engineering materials."},
    {"machine.max_bed_temp",
     "Es la temperatura más alta a la que puede calentar la cama de tu impresora, en °C.\n\nSi la temperatura de cama "
     "del filamento, o un cambio a cierta altura, pide más que esto, el programa te avisa: la cama no va a llegar y la "
     "pieza puede despegarse. En 0, no se sabe y no se revisa.\n\nEjemplo: 100 °C en muchas impresoras comunes; 110 a "
     "120 °C en las que imprimen ABS o materiales técnicos.",
     "It is the highest temperature your printer's bed can heat to, in °C.\n\nIf the filament's bed temperature, or a "
     "change at some height, asks for more than this, the program warns you: the bed will not get there and the part "
     "may come loose. At 0, it is unknown and not checked.\n\nExample: 100 °C on many ordinary printers; 110 to 120 °C "
     "on those that print ABS or engineering materials."},
    {"machine.flavor",
     "Es el dialecto del G-code, el idioma de las instrucciones que lee la impresora. Cada firmware (el programa que "
     "corre dentro de la impresora) entiende algunos comandos distintos, por ejemplo para calentar o para el "
     "ventilador.\n\nSi elegís uno que no es el de tu impresora, puede ignorar comandos, no calentar o fallar a mitad "
     "de camino. Las impresoras viejas tienen sus propios dialectos, y algunos no llevan aceleraciones.\n\nEjemplo: "
     "marlin para la mayoría de las impresoras hogareñas; klipper si usás Klipper; reprapfirmware para una placa Duet.",
     "It is the G-code dialect, the language of the instructions the printer reads. Each firmware (the program running "
     "inside the printer) understands some commands differently, for instance for heating or for the fan.\n\nPick one "
     "that is not your printer's and it may ignore commands, not heat up, or fail halfway through. Old printers have "
     "their own dialects, and some of them carry no accelerations.\n\nExample: marlin for most home printers; klipper "
     "if you run Klipper; reprapfirmware for a Duet board."},
    {"machine.limits_usage",
     "Dice para qué sirven los límites de movimiento de la máquina (velocidades máximas, aceleraciones, "
     "tirones).\n\ntime_only: solo para estimar el tiempo de impresión. emit: además se escriben al principio del "
     "G-code, para que la impresora los use, y ninguna aceleración los pasa (con Klipper es un error, porque los toma "
     "de su propia configuración). ignore: la estimación deja de lado las aceleraciones por eje, los tirones y los "
     "mínimos, y da un tiempo más aproximado.\n\nEjemplo: time_only, el valor por defecto; emit si querés que el "
     "archivo le fije los límites a tu impresora.",
     "Says what the machine's motion limits (top speeds, accelerations, jerks) are for.\n\ntime_only: only for "
     "estimating the print time. emit: they are also written at the start of the G-code so the printer uses them, and "
     "no acceleration goes above them (with Klipper this is an error, since it takes them from its own configuration). "
     "ignore: the estimate leaves out the per-axis accelerations, jerks and minimums, giving a rougher "
     "time.\n\nExample: time_only, the default; emit if you want the file to set the limits on your printer."},
    {"machine.start_gcode",
     "Son las instrucciones que la impresora ejecuta antes de empezar la pieza: llevar los ejes a su origen, calentar, "
     "limpiar la boquilla con una línea al costado, y lo que necesite tu máquina.\n\nSe escribe en G-code, y puede "
     "usar plantillas con valores que el programa completa, como la temperatura. Un error acá puede hacer que la "
     "impresora choque o no caliente, así que conviene partir del que trae el perfil de tu impresora y cambiar "
     "poco.\n\nEjemplo: G28 para ir al origen, seguido de una línea de purga en el borde de la cama.",
     "These are the instructions the printer runs before starting the part: sending the axes home, heating up, wiping "
     "the nozzle with a line along the edge, and whatever else your machine needs.\n\nIt is written in G-code and can "
     "use templates with values the program fills in, like the temperature. A mistake here can make the printer crash "
     "or not heat up, so it is best to start from the one in your printer's profile and change little.\n\nExample: G28 "
     "to go home, followed by a purge line along the edge of the bed."},
    {"machine.end_gcode",
     "Son las instrucciones que la impresora ejecuta al terminar la pieza: apagar la boquilla, la cama y el "
     "ventilador, alejar el cabezal y presentar la pieza.\n\nSe escribe en G-code y puede usar plantillas con valores "
     "que el programa completa. Conviene que suba el cabezal antes de moverlo, para no rozar la pieza terminada. Partí "
     "del que trae el perfil de tu impresora.\n\nEjemplo: M104 S0 y M140 S0 para apagar la boquilla y la cama, subir "
     "unos 10 mm y llevar la cama hacia adelante.",
     "These are the instructions the printer runs when the part is done: switching off the nozzle, the bed and the "
     "fan, moving the head away and presenting the part.\n\nIt is written in G-code and can use templates with values "
     "the program fills in. It should lift the head before moving it, so it does not scrape the finished part. Start "
     "from the one in your printer's profile.\n\nExample: M104 S0 and M140 S0 to switch off the nozzle and the bed, "
     "lift about 10 mm and bring the bed forward."},
    {"machine.variables",
     "Son los valores que completan las plantillas del G-code de inicio y de fin, como la temperatura de la boquilla o "
     "de la cama, con una lista cuando hay un valor por extrusor.\n\nNormalmente las llena el programa con los datos "
     "de tus perfiles, así que casi nunca hace falta tocarlas. Si una plantilla nombra una variable que no existe, el "
     "programa no escribe el G-code y te avisa del error.\n\nEjemplo: temperature con 210 para la boquilla y "
     "bed_temperature con 60 para la cama, al imprimir PLA.",
     "These are the values that fill in the start and end G-code templates, like the nozzle or bed temperature, with a "
     "list when there is one value per extruder.\n\nThe program normally fills them in from your profiles, so you "
     "rarely need to touch them. If a template names a variable that does not exist, the program does not write the "
     "G-code and reports the error.\n\nExample: temperature at 210 for the nozzle and bed_temperature at 60 for the "
     "bed, when printing PLA."},
