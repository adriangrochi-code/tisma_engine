// Explanations of the gcode settings for setting_guide.cpp: {key, Spanish, English},
// one entry per setting. Own texts; see PROVENANCE.md.
    {"gcode.resolution",
     "El G-code es la lista de órdenes que sigue la impresora, una por línea: mover a tal punto, calentar, empujar "
     "plástico. Las curvas se escriben como muchos tramos rectos y cortitos. Este ajuste es cuánto se puede apartar "
     "el camino al saltear puntos que casi no cambian nada.\n\nMás alto da un archivo más chico, con menos órdenes, "
     "lo que ayuda a impresoras que se traban con muchos tramos diminutos; demasiado alto y las curvas se ven "
     "facetadas. 0 escribe todos los puntos.\n\nEjemplo: 0,0125 mm, el valor de fábrica, no se nota en la pieza; "
     "probá 0,02 a 0,05 mm si la impresora se detiene a tirones en las curvas.",
     "G-code is the list of instructions the printer follows, one per line: move here, heat up, push plastic. Curves "
     "are written as many short straight moves. This setting is how far the path may stray when points that barely "
     "change anything are skipped.\n\nHigher gives a smaller file with fewer instructions, which helps printers that "
     "stutter on lots of tiny moves; too high and curves look faceted. 0 writes every point.\n\nExample: 0.0125 mm, "
     "the default, does not show on the part; try 0.02 to 0.05 mm if the printer jerks and pauses on curves."},
    {"changes",
     "Una lista de cambios que empiezan a una altura de la pieza: desde ahí la impresora usa otra temperatura, otro "
     "ventilador, otra retracción, otro flujo o velocidad, o hace un cambio de color o una pausa.\n\nCada cambio vale "
     "desde la primera capa que pasa esa altura hasta que otro lo vuelva a cambiar; lo que no se nombra queda como "
     "venía. Las torres de calibración la llenan solas, un tramo por valor.\n\nEjemplo: a 8 mm, cambio de color, para "
     "que la parte de arriba de un cartel salga de otro color; o 230 °C desde 0 mm y 225 °C desde 8 mm.",
     "A list of changes that start at a height of the part: from there the printer uses another temperature, fan, "
     "retraction, flow or speed, or makes a colour change or a pause.\n\nEach change holds from the first layer above "
     "that height until another one changes it again; whatever it does not name stays as it was. The calibration "
     "towers fill it by themselves, one segment per value.\n\nExample: at 8 mm, a colour change, so the top of a sign "
     "comes out in another colour; or 230 °C from 0 mm and 225 °C from 8 mm."},
    {"gcode.volumetric_e",
     "Normalmente el G-code dice cuánto plástico poner en milímetros de filamento que entran en la boquilla. Con este "
     "ajuste lo dice en milímetros cúbicos de plástico, y la impresora hace la cuenta con el diámetro del "
     "filamento.\n\nSolo sirve si el firmware (el programa que corre dentro de la impresora) está preparado para eso. "
     "En una impresora de filamento, el motor no le avisa el diámetro: tenés que ponerlo vos en el G-code de inicio, "
     "o la pieza sale con plástico de más o de menos. En una de pellets el motor lo escribe solo en Marlin y "
     "RepRapFirmware.\n\nEjemplo: apagado para casi todos; encendido con M200 D1.75 en el G-code de inicio si tu "
     "firmware usa extrusión volumétrica.",
     "Normally the G-code says how much plastic to lay in millimetres of filament fed into the nozzle. With this on, "
     "it says it in cubic millimetres of plastic, and the printer does the maths with the filament's diameter.\n\nIt "
     "only works if the firmware (the program running inside the printer) is set up for it. On a filament printer the "
     "engine does not tell it the diameter: you add that to the start G-code yourself, or the part gets too much or "
     "too little plastic. On a pellet printer the engine writes it for Marlin and RepRapFirmware.\n\nExample: off for "
     "almost everyone; on, with M200 D1.75 in the start G-code, if your firmware uses volumetric extrusion."},
    {"gcode.verbose_comments",
     "El G-code es el archivo de órdenes que lee la impresora. Con este ajuste, cada línea que escribe el motor lleva "
     "al final un comentario corto que dice qué hace: pared, relleno, retracción, viaje, ventilador.\n\nLa impresora "
     "ignora los comentarios, así que la pieza sale igual; el archivo queda más grande. Sirve para aprender cómo se "
     "arma una impresión o para encontrar qué estaba haciendo la impresora cuando algo salió mal.\n\nEjemplo: apagado "
     "para imprimir; encendido cuando quieras abrir el archivo en un editor de texto y entender una línea en "
     "particular.",
     "G-code is the file of instructions the printer reads. With this on, every line the engine writes ends with a "
     "short comment saying what it does: wall, infill, retraction, travel, fan.\n\nThe printer ignores comments, so "
     "the part comes out the same; the file gets bigger. It is useful to learn how a print is put together or to find "
     "out what the printer was doing when something went wrong.\n\nExample: off for printing; on when you want to "
     "open the file in a text editor and understand a particular line."},
    {"gcode.arcs",
     "Una curva se suele escribir en el G-code como muchos tramos rectos y cortos. Con este ajuste, los tramos que "
     "siguen un círculo se juntan en una sola orden de arco (G2 o G3), que la impresora recorre como curva.\n\nEl "
     "archivo queda mucho más chico (un cilindro de prueba pasa de 179 a 46 kB) y la impresora tiene menos órdenes "
     "que leer. Pero tiene que entender arcos: en Marlin hace falta tenerlos activados y en Klipper la sección "
     "[gcode_arcs]; si no, las curvas salen mal o no salen.\n\nEjemplo: apagado si no sabés; encendido si tu firmware "
     "acepta arcos, sobre todo en piezas con muchas curvas.",
     "A curve is usually written in G-code as many short straight moves. With this on, moves that follow a circle are "
     "merged into a single arc instruction (G2 or G3), which the printer runs as a curve.\n\nThe file gets much "
     "smaller (a test cylinder goes from 179 to 46 kB) and the printer has fewer instructions to read. But it has to "
     "understand arcs: Marlin needs them enabled and Klipper the [gcode_arcs] section; otherwise the curves come out "
     "wrong or not at all.\n\nExample: off if unsure; on if your firmware accepts arcs, especially for parts with "
     "lots of curves."},
    {"gcode.binary",
     "Guarda el archivo en el formato binario .bgcode en vez del G-code de texto de siempre. Además de las órdenes, "
     "lleva en bloques aparte los datos de la impresión (impresora, filamento, tiempos, todos los ajustes) y una suma "
     "de control en cada bloque, que deja ver si el archivo llegó dañado.\n\nSolo lo leen las impresoras que aceptan "
     ".bgcode; las demás no lo entienden. Por ahora no lleva miniaturas y no se comprime. Si el nombre termina en "
     ".gcode, pasa a .bgcode.\n\nEjemplo: apagado para casi todas las impresoras; encendido solo si el manual de la "
     "tuya dice que lee .bgcode.",
     "Saves the file in the binary .bgcode format instead of the usual text G-code. Besides the instructions, it "
     "holds the print's data in separate blocks (printer, filament, times, every setting) and a checksum in each "
     "block, which shows whether the file arrived damaged.\n\nOnly printers that accept .bgcode can read it; the rest "
     "do not understand it. For now it has no thumbnails and is not compressed. A name ending in .gcode becomes "
     ".bgcode.\n\nExample: off for almost every printer; on only if your printer's manual says it reads .bgcode."},
    {"gcode.corner_smoothing",
     "En una esquina en punta la impresora tiene que frenar casi del todo, girar y volver a acelerar. Este ajuste "
     "redondea esas esquinas con una curvita que se aparta del vértice como mucho esta distancia, para que pase sin "
     "frenar tanto.\n\nSolo toca las paredes de adentro y el relleno: la pared de afuera, la que se ve, queda en "
     "punta. Más alto ahorra algo de tiempo en piezas con muchas esquinas; como el máximo es muy chico, la forma no "
     "cambia a la vista.\n\nEjemplo: 0 deja las esquinas en punta; 0,02 a 0,05 mm para ganar un poco de velocidad en "
     "rellenos con muchas vueltas.",
     "At a sharp corner the printer has to almost stop, turn and speed up again. This setting rounds those corners "
     "with a tiny curve that strays from the corner point by at most this distance, so the head gets through without "
     "slowing down as much.\n\nIt only affects the inner walls and the infill: the outer wall, the one you see, stays "
     "sharp. Higher saves some time on parts with many corners; since the maximum is tiny, the shape does not visibly "
     "change.\n\nExample: 0 keeps corners sharp; 0.02 to 0.05 mm to gain a little speed in infill with many turns."},
    {"output.filename_format",
     "Es el nombre que se le da al archivo de G-code (las órdenes para la impresora) cuando el programa lo elige "
     "solo. Puede llevar variables entre corchetes que se cambian por su valor, como el nombre del modelo, la altura "
     "de capa o el relleno.\n\nSirve para reconocer los archivos de un vistazo en la tarjeta de memoria. El tiempo de "
     "impresión y el peso se conocen al terminar: el archivo se renombra al final con ellos.\n\nEjemplo: "
     "[input_filename_base].gcode deja el nombre del modelo, como robot.gcode; "
     "[input_filename_base]_[print_time].gcode da robot_1h7m.gcode.",
     "It is the name given to the G-code file (the printer's instructions) when the program picks it by itself. It "
     "can hold variables in square brackets that are replaced by their value, like the model's name, the layer height "
     "or the infill.\n\nIt helps you recognise files at a glance on the memory card. Print time and weight are only "
     "known at the end: the file is renamed with them when slicing finishes.\n\nExample: [input_filename_base].gcode "
     "keeps the model's name, like robot.gcode; [input_filename_base]_[print_time].gcode gives robot_1h7m.gcode."},
    {"output.post_process",
     "Es una lista de scripts (programitas tuyos) que se corren sobre el G-code ya terminado. Cada uno recibe la ruta "
     "del archivo y puede leerlo y cambiarlo: agregar líneas, ajustar algo, copiarlo a otro lado. También recibe los "
     "valores de los ajustes de la impresión.\n\nCorré solo scripts en los que confíes, porque pueden hacer cualquier "
     "cosa en tu computadora. La línea de comandos pide confirmación antes; si un script falla, el comando falla. La "
     "interfaz todavía no los corre.\n\nEjemplo: vacío para casi todos; /home/ana/scripts/foto_por_capa.py para un "
     "script que agrega una orden de la cámara en cada capa.",
     "It is a list of scripts (small programs of your own) run on the finished G-code. Each one gets the file's path "
     "and can read and change it: add lines, adjust something, copy it somewhere else. It also gets the values of the "
     "print's settings.\n\nOnly run scripts you trust, since they can do anything on your computer. The command line "
     "asks for confirmation first; if a script fails, the command fails. The interface does not run them "
     "yet.\n\nExample: empty for almost everyone; /home/ana/scripts/photo_per_layer.py for a script that adds a "
     "camera command on every layer."},
    {"gcode.auto_temperatures",
     "El G-code de inicio son las órdenes que la impresora corre antes de imprimir: ir a su posición de origen, "
     "calentar, limpiar la boquilla. Con este ajuste encendido, si ese inicio no tiene ninguna orden de temperatura, "
     "el motor agrega las suyas: calienta la cama y espera, prende la boquilla y, después del inicio, espera a que "
     "llegue.\n\nAsí nunca se empieza a imprimir en frío. Si tu inicio ya calienta, el motor no agrega nada. Apagado, "
     "no las agrega nunca.\n\nEjemplo: dejalo encendido; apagalo si el inicio llama a una macro de tu firmware que ya "
     "calienta por su cuenta, como PRINT_START en Klipper.",
     "The start G-code is the instructions the printer runs before printing: move to its home position, heat up, wipe "
     "the nozzle. With this on, if that start has no temperature command at all, the engine adds its own: it heats "
     "the bed and waits, turns the nozzle on and, after the start, waits for it to get there.\n\nThat way printing "
     "never begins cold. If your start already heats, the engine adds nothing. Off, it never adds them.\n\nExample: "
     "leave it on; turn it off if the start calls a firmware macro that already heats by itself, like PRINT_START in "
     "Klipper."},
    {"gcode.before_layer",
     "Es G-code propio (órdenes para la impresora escritas por vos) que se mete justo antes de que la boquilla suba a "
     "cada capa nueva, desde la segunda. Puede usar variables como el número de capa [layer_num], que empieza en 0, y "
     "su altura [layer_z].\n\nNo cambia la pieza por sí solo: sirve para mostrar algo en la pantalla, avisarle algo a "
     "otro programa o preparar la impresora para la capa siguiente. Vacío, no se agrega nada.\n\nEjemplo: M117 Capa "
     "{layer_num + 1} muestra en la pantalla de la impresora en qué capa va.",
     "It is your own G-code (printer instructions you write) inserted just before the nozzle rises to each new layer, "
     "from the second one on. It can use variables like the layer number [layer_num], which starts at 0, and its "
     "height [layer_z].\n\nIt does not change the part by itself: it is for showing something on the screen, "
     "signalling another program, or getting the printer ready for the next layer. Empty adds nothing.\n\nExample: "
     "M117 Layer {layer_num + 1} shows on the printer's screen which layer it is on."},
    {"gcode.after_layer",
     "Lo mismo que el G-code de antes de cada capa, pero va justo después de que la boquilla sube a la capa nueva. "
     "Usa las mismas variables, como [layer_num] y [layer_z].\n\nConviene para lo que tiene que pasar ya a la altura "
     "nueva, como sacar una foto para un video acelerado (timelapse). Vacío, no se agrega nada.\n\nEjemplo: M117 Z "
     "[layer_z] muestra la altura actual en la pantalla; o la orden de tu cámara para sacar una foto por capa.",
     "The same as the G-code before each layer, but it goes right after the nozzle rises to the new layer. It uses "
     "the same variables, like [layer_num] and [layer_z].\n\nIt suits things that should happen at the new height, "
     "like taking a photo for a sped-up video (a timelapse). Empty adds nothing.\n\nExample: M117 Z [layer_z] shows "
     "the current height on the screen; or your camera's command to take one photo per layer."},
    {"gcode.between_objects",
     "Con la impresión secuencial encendida, cada objeto de la cama se imprime entero antes de empezar el siguiente. "
     "Este es el G-code propio (órdenes escritas por vos) que va entre un objeto y el otro. Puede usar "
     "[current_object_idx], el número del objeto que sigue, desde 0.\n\nSin la impresión secuencial no se usa. Si tu "
     "texto pone temperaturas, el motor no escribe las suyas para el objeto siguiente. Vacío, no se agrega "
     "nada.\n\nEjemplo: M117 Objeto {current_object_idx + 1} para ver en la pantalla cuál va; o M104 S205 para bajar "
     "un poco la boquilla en el que sigue.",
     "With sequential printing on, each object on the bed is printed completely before the next one starts. This is "
     "your own G-code (instructions you write) that goes between one object and the next. It can use "
     "[current_object_idx], the number of the next object, starting at 0.\n\nWithout sequential printing it is not "
     "used. If your text sets temperatures, the engine does not write its own for the next object. Empty adds "
     "nothing.\n\nExample: M117 Object {current_object_idx + 1} to see on the screen which one is printing; or M104 "
     "S205 to run the next one a little cooler."},
    {"gcode.color_change",
     "Podés pedir un cambio de color a cierta altura: la impresora se detiene, cambiás el filamento por otro color y "
     "sigue. Esta es la orden que se escribe en ese punto del G-code (el archivo de órdenes de la "
     "impresora).\n\nTiene que ser una que tu firmware (el programa de la impresora) entienda como cambio de "
     "filamento; si no, sigue de largo con el mismo color. Puede usar variables.\n\nEjemplo: M600, el de fábrica, "
     "sirve en Marlin y en muchas impresoras; en Klipper, el nombre de la macro de cambio de filamento que tengas "
     "configurada.",
     "You can ask for a colour change at a given height: the printer stops, you swap the filament for another colour "
     "and it carries on. This is the instruction written at that point of the G-code (the printer's instruction "
     "file).\n\nIt has to be one your firmware (the printer's program) understands as a filament change; otherwise it "
     "carries on with the same colour. It can use variables.\n\nExample: M600, the default, works on Marlin and many "
     "printers; on Klipper, the name of the filament change macro you have set up."},
    {"gcode.pause",
     "Podés pedir una pausa a cierta altura, por ejemplo para meter un imán o una tuerca adentro de la pieza antes de "
     "que se cierre. Esta es la orden que se escribe en ese punto del G-code.\n\nTiene que ser una que tu firmware "
     "(el programa de la impresora) entienda como pausa; si no, la impresora sigue sin detenerse. Puede usar "
     "variables.\n\nEjemplo: M601, el de fábrica, lo entienden varias impresoras; en Klipper suele ser PAUSE. Mirá el "
     "manual de la tuya.",
     "You can ask for a pause at a given height, for example to drop a magnet or a nut inside the part before it gets "
     "closed. This is the instruction written at that point of the G-code.\n\nIt has to be one your firmware (the "
     "printer's program) understands as a pause; otherwise the printer keeps going without stopping. It can use "
     "variables.\n\nExample: M601, the default, is understood by several printers; on Klipper it is usually PAUSE. "
     "Check your printer's manual."},
    {"gcode.custom_template",
     "Además de los cambios de color y las pausas, podés marcar una altura para meter G-code propio (órdenes escritas "
     "por vos). Este es el texto que se escribe en ese punto, al empezar la capa.\n\nSirve para cualquier cosa que tu "
     "impresora entienda: un mensaje en la pantalla, un sonido, mover el cabezal a un costado. Puede usar variables. "
     "Vacío, no se agrega nada.\n\nEjemplo: M117 Falta la mitad muestra un aviso en la pantalla cuando la pieza llega "
     "a esa altura.",
     "Besides colour changes and pauses, you can mark a height to insert your own G-code (instructions you write). "
     "This is the text written at that point, as the layer starts.\n\nIt is for anything your printer understands: a "
     "message on the screen, a beep, moving the head aside. It can use variables. Empty adds nothing.\n\nExample: "
     "M117 Halfway there shows a note on the screen when the part reaches that height."},
    {"retraction.speed",
     "La retracción tira el filamento un poco hacia atrás antes de un viaje sin imprimir, para que la boquilla no "
     "gotee. Este ajuste es a qué velocidad tira, y también a qué velocidad lo vuelve a empujar, salvo que pongas una "
     "aparte para la vuelta.\n\nMás rápido corta antes el goteo y se pierde menos tiempo en cada viaje; demasiado "
     "rápido, el motor que empuja el filamento puede saltear pasos o morder el plástico y gastarlo. Si no lo tocás, "
     "vale el de la impresora.\n\nEjemplo: 25 a 45 mm/s es lo común. Si oís chasquidos en el motor del filamento al "
     "retraer, bajalo.",
     "Retraction pulls the filament back a little before a travel without printing, so the nozzle does not ooze. This "
     "setting is how fast it pulls, and also how fast it pushes it back, unless you set a separate speed for the "
     "return.\n\nFaster stops the ooze sooner and wastes less time on each travel; too fast, the motor that drives "
     "the filament can skip or chew into the plastic and grind it. If you leave it alone, the printer's value "
     "applies.\n\nExample: 25 to 45 mm/s is common. If you hear clicking from the filament motor when it retracts, "
     "lower it."},
    {"retraction.unretract_speed",
     "Después de un viaje, el filamento que se había tirado hacia atrás vuelve a entrar para seguir imprimiendo. Este "
     "ajuste es la velocidad de esa vuelta. En 0 usa la misma que la retracción.\n\nUna vuelta más lenta deja que la "
     "presión en la boquilla se arme con suavidad y evita una gotita al principio de la línea; demasiado lenta suma "
     "tiempo en cada viaje. Tocalo solo si ves marcas donde arrancan las líneas.\n\nEjemplo: 0 para la mayoría; 15 a "
     "25 mm/s si aparecen granitos al empezar cada línea después de un viaje.",
     "After a travel, the filament that was pulled back goes in again so printing can continue. This setting is the "
     "speed of that return. At 0 it uses the same speed as the retraction.\n\nA slower return lets the pressure in "
     "the nozzle build up gently and avoids a small blob at the start of the line; too slow adds time to every "
     "travel. Only touch it if you see marks where lines begin.\n\nExample: 0 for most people; 15 to 25 mm/s if small "
     "blobs show up at the start of each line after a travel."},
    {"retraction.restart_extra",
     "Al volver de una retracción, el filamento entra lo mismo que salió. Este ajuste empuja un poquito más, para "
     "reponer el plástico que igual goteó durante el viaje.\n\nSi después de los viajes las líneas arrancan finitas o "
     "con un huequito, subilo un poco; si arrancan con una gota, es demasiado. No se usa con la retracción del "
     "firmware.\n\nEjemplo: 0 casi siempre; 0,05 a 0,2 mm si ves huecos al empezar las líneas después de viajes "
     "largos.",
     "When coming back from a retraction, the filament goes in as much as it came out. This setting pushes a tiny bit "
     "more, to make up for the plastic that oozed during the travel anyway.\n\nIf lines start thin or with a small "
     "gap after travels, raise it a little; if they start with a blob, it is too much. It is not used with firmware "
     "retraction.\n\nExample: 0 almost always; 0.05 to 0.2 mm if you see gaps at the start of lines after long "
     "travels."},
    {"retraction.on_layer_change",
     "Normalmente se retrae (se tira el filamento hacia atrás) solo en los viajes largos. Con este ajuste, también se "
     "retrae en cada cambio de capa, antes de subir, aunque el viaje al primer punto de la capa nueva sea "
     "corto.\n\nAyuda a que no queden gotitas o hilos donde empieza cada capa, que suelen alinearse en la costura. "
     "Suma una retracción por capa, que es poco tiempo.\n\nEjemplo: apagado de fábrica; encendelo si ves granitos en "
     "el lugar donde arranca cada capa.",
     "Normally the filament is retracted (pulled back) only on long travels. With this on, it is also retracted at "
     "every layer change, before rising, even if the travel to the new layer's first point is short.\n\nIt helps "
     "avoid small blobs or strings where each layer starts, which tend to line up along the seam. It adds one "
     "retraction per layer, which is little time.\n\nExample: off by default; turn it on if you see small bumps where "
     "each layer begins."},
    {"retraction.wipe",
     "En vez de retraer quieta, la boquilla vuelve un poco sobre la línea que acaba de imprimir mientras tira el "
     "filamento hacia atrás, y después hace su viaje.\n\nAsí el plástico que queda en la punta se limpia contra la "
     "pieza en vez de quedar como gota o hilo. Suma un movimiento corto antes de cada viaje que retrae. No va junto "
     "con la retracción del firmware.\n\nEjemplo: apagado de fábrica; encendelo si ves gotitas en la costura o hilos "
     "que la retracción sola no saca.",
     "Instead of retracting while standing still, the nozzle moves back a little over the line it just printed while "
     "pulling the filament back, and then makes its travel.\n\nThat way the plastic left at the tip gets wiped off "
     "against the part instead of ending up as a blob or a string. It adds a short move before every travel that "
     "retracts. It does not work together with firmware retraction.\n\nExample: off by default; turn it on if you see "
     "blobs on the seam or strings that retraction alone does not fix."},
    {"retraction.before_wipe",
     "Solo cuenta con el barrido encendido. Es qué parte de la retracción, en porcentaje, se hace con la boquilla "
     "quieta antes de empezar a barrer; el resto se hace mientras barre.\n\nMás alto corta la presión de golpe antes "
     "de moverse; más bajo deja más filamento para retraer durante el barrido, que limpia mejor la punta.\n\nEjemplo: "
     "0 % hace toda la retracción barriendo; 50 % hace la mitad quieta y la mitad barriendo.",
     "It only matters with wipe on. It is how much of the retraction, in percent, is done with the nozzle standing "
     "still before the wipe starts; the rest is done while wiping.\n\nHigher cuts the pressure right away before "
     "moving; lower leaves more filament to retract during the wipe, which cleans the tip better.\n\nExample: 0 % "
     "does the whole retraction while wiping; 50 % does half standing still and half while wiping."},
    {"retraction.firmware",
     "El firmware es el programa que corre dentro de la impresora. Con este ajuste, el G-code no dice cuánto ni a qué "
     "velocidad retraer: solo dice retraé (G10) y volvé (G11), y la impresora usa los valores que tiene "
     "guardados.\n\nAsí podés ajustar la retracción desde la pantalla o la consola de la impresora sin volver a "
     "laminar. El largo de acá solo decide si se retrae o no (0 es no), y el salto en Z sigue saliendo del G-code. No "
     "va con el barrido.\n\nEjemplo: apagado para la mayoría; encendido si tu firmware tiene la retracción propia "
     "activada, como [firmware_retraction] en Klipper.",
     "Firmware is the program running inside the printer. With this on, the G-code does not say how much or how fast "
     "to retract: it only says retract (G10) and return (G11), and the printer uses the values it has stored.\n\nThat "
     "lets you tune retraction from the printer's screen or console without slicing again. The length here only "
     "decides whether to retract (0 means no), and the Z lift still comes from the G-code. It does not work with "
     "wipe.\n\nExample: off for most people; on if your firmware has its own retraction enabled, like "
     "[firmware_retraction] in Klipper."},
    {"retraction.min_travel",
     "Los viajes más cortos que esta distancia no retraen: la boquilla va directo sin tirar el filamento hacia atrás. "
     "Un viaje que sale de la pieza retrae siempre, sea del largo que sea.\n\nMás bajo retrae más seguido, con menos "
     "hilos, pero gasta más tiempo y castiga el filamento, que va y viene muchas veces en el mismo lugar. Más alto "
     "retrae menos, con más riesgo de hilos en detalles chicos.\n\nEjemplo: 2 mm, el de fábrica, anda bien; 1 mm si "
     "quedan hilitos entre detalles muy juntos; 3 a 5 mm si el filamento se muerde por retraer tanto.",
     "Travels shorter than this distance do not retract: the nozzle goes straight there without pulling the filament "
     "back. A travel that leaves the part always retracts, however long it is.\n\nLower retracts more often, with "
     "fewer strings, but takes more time and wears the filament, which goes back and forth many times at the same "
     "spot. Higher retracts less, with more risk of strings on small details.\n\nExample: 2 mm, the default, works "
     "well; 1 mm if fine strings remain between details close together; 3 to 5 mm if the filament gets chewed from "
     "retracting so much."},
    {"retraction.lift_above",
     "Solo cuenta si la boquilla sube en los viajes (el salto en Z). Con este ajuste, el salto se hace solo en las "
     "capas que están más arriba de esta altura; abajo, viaja sin subir. 0 no pone límite.\n\nSirve cuando el salto "
     "hace falta recién más arriba, por ejemplo para no hacerlo en las primeras capas, donde no hay nada alto con qué "
     "chocar y subir solo suma tiempo o algún hilo.\n\nEjemplo: 0 para saltar en todas las capas; 0,3 mm para no "
     "saltar en la primera capa.",
     "It only matters if the nozzle rises on travels (the Z lift). With this, the lift happens only on layers above "
     "this height; below it, the nozzle travels without rising. 0 sets no limit.\n\nIt is useful when the lift is "
     "only needed higher up, for example to skip it on the first layers, where there is nothing tall to hit and "
     "rising only adds time or a string.\n\nExample: 0 to lift on every layer; 0.3 mm to skip the lift on the first "
     "layer."},
    {"retraction.lift_below",
     "Solo cuenta si la boquilla sube en los viajes (el salto en Z). Con este ajuste, el salto se hace solo en las "
     "capas que están por debajo de esta altura; más arriba, viaja sin subir. 0 no pone límite.\n\nSirve cuando solo "
     "una parte baja de la pieza tiene cosas con las que la boquilla podría chocar, y más arriba subir solo suma "
     "tiempo. Junto con el límite de abajo, deja el salto en una franja de alturas.\n\nEjemplo: 0 para saltar en "
     "todas las capas; 20 mm si los detalles salidos están solo en los primeros 20 mm.",
     "It only matters if the nozzle rises on travels (the Z lift). With this, the lift happens only on layers below "
     "this height; above it, the nozzle travels without rising. 0 sets no limit.\n\nIt is useful when only a low part "
     "of the model has things the nozzle could hit, and higher up rising only adds time. Together with the lower "
     "limit, it keeps the lift within a band of heights.\n\nExample: 0 to lift on every layer; 20 mm if the parts "
     "that stick up are only in the first 20 mm."},
    {"retraction.coast",
     "Cuando la boquilla deja de empujar plástico, adentro queda presión y sigue saliendo un poco. El coasting "
     "aprovecha eso: el final de cada pared y de cada relleno se recorre sin empujar filamento, y lo que queda en la "
     "boquilla lo termina.\n\nAyuda a sacar la gota que se forma al cerrar una vuelta, en la costura. Demasiado deja "
     "un hueco al final de la línea. No se usa en líneas muy cortas ni en modo vaso. 0 lo apaga.\n\nEjemplo: 0 de "
     "fábrica; 0,2 a 0,5 mm si la costura tiene una gotita que no se va con la retracción.",
     "When the nozzle stops pushing plastic, pressure is left inside and a little keeps coming out. Coasting uses "
     "that: the end of each wall and each infill line is run without pushing filament, and what is left in the nozzle "
     "finishes it.\n\nIt helps remove the blob that forms where a loop closes, at the seam. Too much leaves a gap at "
     "the end of the line. It is not used on very short lines or in vase mode. 0 turns it off.\n\nExample: 0 by "
     "default; 0.2 to 0.5 mm if the seam has a small blob that retraction does not remove."},
    {"retraction.toolchange_length",
     "Solo cuenta en impresoras con varios extrusores (varios filamentos). Antes de cambiar de uno a otro, el que se "
     "deja tira su filamento hacia atrás esta distancia, quieto, para que no gotee mientras espera.\n\nMás largo saca "
     "mejor el plástico de la zona caliente, pero tarda más y al volver hay más filamento que empujar. Podés poner un "
     "valor para todos o uno por extrusor.\n\nEjemplo: 10 mm, el de fábrica; si cada filamento tiene su boquilla, 2 a "
     "5 mm suelen alcanzar. Si tu sistema pide otro largo, usá el que diga el fabricante.",
     "It only matters on printers with several extruders (several filaments). Before switching from one to another, "
     "the one being left pulls its filament back by this distance, standing still, so it does not ooze while it "
     "waits.\n\nLonger pulls the plastic further out of the hot zone, but takes longer and there is more filament to "
     "push back later. You can give one value for all or one per extruder.\n\nExample: 10 mm, the default; if each "
     "filament has its own nozzle, 2 to 5 mm is usually enough. If your system asks for another length, use the "
     "maker's."},
    {"retraction.toolchange_restart_extra",
     "Solo cuenta con varios extrusores. Cuando una herramienta vuelve a usarse después de un cambio, su filamento "
     "entra lo que había retraído más esta cantidad, para reponer lo que goteó mientras esperaba.\n\nSi las primeras "
     "líneas después de un cambio salen finas o cortadas, subilo un poco; si arrancan con una gota, bajalo. Va un "
     "valor para todos o uno por extrusor.\n\nEjemplo: 0 de fábrica; 0,5 a 1 mm si después de cada cambio falta "
     "plástico al empezar.",
     "It only matters with several extruders. When a tool is used again after a change, its filament goes in by what "
     "it had retracted plus this amount, to make up for what oozed while it waited.\n\nIf the first lines after a "
     "change come out thin or broken, raise it a little; if they start with a blob, lower it. One value for all or "
     "one per extruder.\n\nExample: 0 by default; 0.5 to 1 mm if plastic is missing at the start after each change."},
    {"gcode.toolchange",
     "Solo cuenta con varios extrusores. Es G-code propio (órdenes escritas por vos) que se mete en cada cambio de "
     "herramienta, entre la retracción de la que sale y el arranque de la que entra. También se corre al empezar. "
     "Puede usar [previous_extruder], [next_extruder] (desde 0) y [toolchange_z], la altura de la capa.\n\nSi tu "
     "texto no tiene la orden de cambio (T0, T1…), el motor la agrega. Sirve para mover el cabezal a un costado o a "
     "una estación de limpieza antes de cambiar.\n\nEjemplo: vacío alcanza en muchas impresoras; G1 Z{toolchange_z + "
     "2} F600 sube 2 mm antes de cambiar.",
     "It only matters with several extruders. It is your own G-code (instructions you write) inserted at each tool "
     "change, between the retraction of the outgoing tool and the start of the incoming one. It also runs at the "
     "start. It can use [previous_extruder], [next_extruder] (from 0) and [toolchange_z], the layer's height.\n\nIf "
     "your text has no change command (T0, T1…), the engine adds it. It is useful to move the head aside or to a "
     "cleaning station before switching.\n\nExample: empty is enough on many printers; G1 Z{toolchange_z + 2} F600 "
     "rises 2 mm before the change."},
    {"gcode.progress",
     "Agrega al G-code órdenes (M73) que le dicen a la impresora qué porcentaje lleva y cuántos minutos faltan, según "
     "el tiempo que calculó el programa. La impresora lo muestra en su pantalla.\n\nNo cambia la pieza. Sin estas "
     "órdenes, muchas impresoras calculan el avance por su cuenta, con menos precisión. Sirve solo si tu firmware (el "
     "programa de la impresora) entiende M73.\n\nEjemplo: encendido en impresoras cuya pantalla muestra el tiempo "
     "restante con M73; apagado si no sabés si la tuya lo usa.",
     "Adds instructions (M73) to the G-code that tell the printer what percentage it has done and how many minutes "
     "are left, from the time the program estimated. The printer shows it on its screen.\n\nIt does not change the "
     "part. Without them, many printers work out progress on their own, less accurately. It only helps if your "
     "firmware (the printer's program) understands M73.\n\nExample: on for printers whose screen shows the remaining "
     "time from M73; off if you do not know whether yours uses it."},
    {"gcode.object_labels",
     "Marca en el G-code qué parte es de cada objeto de la cama. Así, si uno falla a mitad de camino, podés cancelar "
     "solo ese y dejar que los demás sigan.\n\nfirmware escribe las marcas que entiende la impresora misma (Marlin, "
     "RepRapFirmware o Klipper; en Klipper hace falta la sección [exclude_object]). octoprint escribe comentarios "
     "para OctoPrint, un programa que maneja la impresora desde una computadora. Con marcas, en cada capa se termina "
     "lo de un objeto antes de pasar al siguiente.\n\nEjemplo: none con un solo objeto; firmware al imprimir varias "
     "piezas iguales en Klipper o Marlin.",
     "Marks in the G-code which part belongs to each object on the bed. Then, if one fails halfway, you can cancel "
     "just that one and let the rest carry on.\n\nfirmware writes the marks the printer itself understands (Marlin, "
     "RepRapFirmware or Klipper; Klipper needs the [exclude_object] section). octoprint writes comments for "
     "OctoPrint, a program that runs the printer from a computer. With marks, each layer finishes one object's part "
     "before moving to the next.\n\nExample: none with a single object; firmware when printing several identical "
     "parts on Klipper or Marlin."},
