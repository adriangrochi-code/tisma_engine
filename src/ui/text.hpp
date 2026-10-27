#pragma once

// Every text the interface shows, in each language, in one place: an entry is
// (id, Spanish, English). Texts with printf fields (%s, %d, %.1f) say so in
// their id's comment where it is not obvious. The engine's own messages (the
// reasons a model cannot be sliced, the G-code check) stay in English, as in
// the command line. Own design; see PROVENANCE.md.
#define TISMA_UI_TEXT(X)                                                                                              \
    X(TabPrepare, "Preparar", "Prepare")                                                                              \
    X(TabPreview, "Vista previa", "Preview")                                                                          \
    X(TabAnalysis, "Análisis", "Analysis")                                                                            \
    X(Open, "Abrir", "Open")                                                                                          \
    X(OpenTip, "Abrir un modelo STL, un proyecto, un G-code o ajustes (Ctrl+O)", "Open an STL model, a project, a G-code file or settings (Ctrl+O)") \
    X(Slice, "Laminar", "Slice")                                                                                      \
    X(SliceTip, "Laminar el modelo con estos ajustes (Ctrl+R)", "Slice the model with these settings (Ctrl+R)")       \
    X(Export, "Exportar G-code", "Export G-code")                                                                     \
    X(ExportTip, "Escribir el G-code, volver a leerlo del disco y verificarlo (Ctrl+E)",                              \
      "Write the G-code, read it back from the disk and check it (Ctrl+E)")                                           \
    X(ExportNeedsSlice, "Primero hay que laminar", "Slice first")                                                     \
    X(More, "Más", "More")                                                                                            \
    X(Printer, "Impresora", "Printer")                                                                                \
    X(Filament, "Filamento", "Filament")                                                                              \
    X(Print, "Impresión", "Print")                                                                                    \
    X(BedWidth, "Ancho de la cama", "Bed width")                                                                      \
    X(BedDepth, "Fondo de la cama", "Bed depth")                                                                      \
    X(MaxHeight, "Altura máxima", "Maximum height")                                                                   \
    X(MinLayerHeight, "Capa mínima", "Thinnest layer")                                                                \
    X(MaxLayerHeight, "Capa máxima", "Thickest layer")                                                                \
    X(LayerLimitTip, "La altura de capa variable se queda entre estas dos y nunca pasa el diámetro de la boquilla. 0: 0,04 mm y la boquilla.", "Variable layer heights keep between these two and never exceed the nozzle diameter. 0: 0.04 mm and the nozzle.") \
    X(Zaa, "Z anti-aliasing", "Z anti-aliasing")                                                                                                                                                                                                                 \
    X(ZaaTip, "Las extrusiones que se ven arriba bajan hasta la superficie dentro de su capa, con el material justo: los techos inclinados salen sin escalones. Ver docs/ZAA.md.", "The extrusions seen on top go down to the surface within their layer, with just the material it needs: sloping tops come out without steps. See docs/ZAA.md.") \
    X(ZaaMinThickness, "Espesor mínimo", "Thinnest bead")                                                                                                                                                                                                        \
    X(ZaaMinThicknessTip, "Hasta dónde puede bajar una extrusión: nunca queda más fina que esta parte de la capa.", "How far an extrusion may go down: it never gets thinner than this share of the layer.")                                                     \
    X(ZaaRaise, "Subir a la superficie", "Raise to the surface")                                                                                                                                                                                                 \
    X(ZaaRaiseTip, "También sube hasta la superficie que queda por encima de la capa, hasta media capa (Song et al. 2017): menos escalón, cordones más gruesos.", "Also goes up to the surface above the layer, half a layer at most (Song et al. 2017): smaller steps, thicker beads.") \
    X(CombineInfill, "Combinar relleno", "Combine infill")                                                                                                                                                                                                                               \
    X(CombineInfillTip, "El relleno poco denso se imprime cada N capas, con el alto de las N juntas (nunca más que la boquilla): el mismo relleno en menos pasadas. 1: cada capa el suyo. Ver docs/INFILL.md.", "Sparse infill prints every N layers, as tall as the N together (never more than the nozzle): the same infill in fewer passes. 1: each layer its own. See docs/INFILL.md.") \
    X(CombineAuto, "Combinar lo más posible", "Combine as much as fits")                                                                                                                                                                                                                 \
    X(StressDensity, "Guiado por tensiones", "Stress-guided")                                                                                                                                                                                                                            \
    X(StressDensityTip, "Con un caso de carga (apoyos y fuerzas, como en tisma-engine analyze), el motor analiza la pieza y sube el relleno hasta esta densidad donde trabaja más. 0: apagado. Ver docs/INFILL.md.", "With a load case (fixtures and forces, as tisma-engine analyze takes them), the engine analyses the part and raises the infill up to this density where it works hardest. 0: off. See docs/INFILL.md.") \
    X(LoadCase, "Caso de carga", "Load case")                                                                                                                                                                                                                                            \
    X(ChooseCase, "Elegir caso…", "Choose case…")                                                                                                                                                                                                                                        \
    X(CaseLoaded, "Caso de carga: %s", "Load case: %s")                                                                                                                                                                                                                                  \
    X(DenseUnderTop, "Denso bajo el techo", "Dense under tops")                                                                                                                                                                                                                          \
    X(DenseUnderTopTip, "La capa de relleno justo debajo de una superficie de arriba va más densa, solo bajo esa superficie: permite un relleno general más liviano sin que el techo se hunda. Ver docs/INFILL.md.", "The infill layer right under a top surface goes denser, only under that surface: a lighter infill elsewhere without the top sagging. See docs/INFILL.md.") \
    X(DenseDensity, "Densidad bajo el techo", "Density under tops")                                                                                                                                                                                                                      \
    X(Nozzle, "Boquilla", "Nozzle")                                                                                   \
    X(Flavor, "Dialecto de G-code", "G-code flavor")                                                                  \
    X(RelativeE, "Extrusión relativa (M83)", "Relative extrusion (M83)")                                             \
    X(StartGcode, "G-code de inicio", "Start G-code")                                                                 \
    X(EndGcode, "G-code de fin", "End G-code")                                                                        \
    X(FilamentDiameter, "Diámetro", "Diameter")                                                                       \
    X(NozzleTemp, "Temperatura de boquilla", "Nozzle temperature")                                                    \
    X(BedTemp, "Temperatura de cama", "Bed temperature")                                                              \
    X(TempTip, "Va al G-code de inicio como [temperature] y [bed_temperature]",                                       \
      "Goes into the start G-code as [temperature] and [bed_temperature]")                                            \
    X(Retraction, "Retracción", "Retraction")                                                                         \
    X(RetractSpeed, "Velocidad de retracción", "Retraction speed")                                                    \
    X(RetractLift, "Salto en Z", "Z hop")                                                                             \
    X(GcodeResolution, "Resolución del G-code", "G-code resolution")                                                  \
    X(GcodeResolutionTip, "Los puntos que mueven el camino menos que esto no se escriben: menos líneas para el firmware, la misma pieza. 0: se escriben todos.", "Points that move the path less than this are not written: fewer lines for the firmware, the same part. 0: every point is written.") \
    X(FanMin, "Ventilador normal", "Fan, normal layers")                                                              \
    X(FanMax, "Ventilador en capas lentas", "Fan, slowed layers")                                                     \
    X(MinLayerTime, "Tiempo mínimo de capa", "Minimum layer time")                                                    \
    X(KeepOuterSpeed, "Pared externa a su velocidad", "Outer wall at its speed")                                      \
    X(KeepOuterSpeedTip, "Para llegar al tiempo mínimo se frena primero lo de adentro: la pared externa no cambia de velocidad y la pieza brilla parejo (docs/GLOSS.md).", "To reach the minimum time the inside slows down first: the outer wall keeps its speed and the part has an even sheen (docs/GLOSS.md).") \
    X(LayerHeight, "Altura de capa", "Layer height")                                                                  \
    X(FirstLayerHeight, "Altura de la primera capa", "First layer height")                                            \
    X(DetailHeight, "Alto del detalle", "Detail height")                                                              \
    X(DetailHeightTip, "Capas finas solo donde la pieza cambia (pendientes, detalles, topes): el resto va en capas de la altura de capa. 0: apagado. Ver docs/LOCAL_LAYERS.md.", "Fine layers only where the part changes (slopes, details, tops): the rest goes in layers of the layer height. 0: off. See docs/LOCAL_LAYERS.md.") \
    X(DetailTolerance, "Escalón aceptado", "Accepted step")                                                           \
    X(DetailToleranceTip, "Donde las capas finas difieren menos que esto, va grueso.", "Where the fine layers differ by less than this, it goes thick.") \
    X(DetailMargin, "Margen del detalle", "Detail margin")                                                            \
    X(DetailMarginTip, "Cuánto se agranda la zona fina alrededor del detalle. 0: tres anchos de línea.", "How far the fine zone grows around the detail. 0: three line widths.") \
    X(CuspHeight, "Escalón en pendientes", "Slope step")                                                                                                                         \
    X(CuspHeightTip, "Altura variable automática: el escalón más alto que puede dejar una superficie inclinada. Las capas se afinan donde la pendiente es suave; con alto del detalle, solo en la zona del detalle. 0: apagado. Ver docs/VARIABLE_LAYERS.md.", "Automatic variable height: the highest step a sloping surface may leave. Layers thin where the slope is gentle; with a detail height, only in the detail's zone. 0: off. See docs/VARIABLE_LAYERS.md.") \
    X(LayerHeights, "Alturas por Z", "Heights by Z")                                                                                                                             \
    X(LayerHeightsTip, "Tramos: desde cada Z (sobre la cama) hacia arriba, esa altura de capa. Debajo del primero, la altura de capa.", "Steps: from each Z (above the bed) up, that layer height. Below the first, the layer height.") \
    X(AddStep, "Agregar tramo", "Add step")                                                                                                                                      \
    X(Walls, "Paredes", "Walls")                                                                                      \
    X(LineWidth, "Ancho de línea", "Line width")                                                                      \
    X(Seam, "Costura", "Seam")                                                                                        \
    X(FuzzySkin, "Piel difusa", "Fuzzy skin")                                                                         \
    X(FuzzyNone, "No", "None")                                                                                        \
    X(FuzzyExternal, "Por fuera", "Outside")                                                                          \
    X(FuzzyAll, "Por fuera y agujeros", "Outside and holes")                                                          \
    X(FuzzyThickness, "Espesor de la piel", "Skin thickness")                                                         \
    X(FuzzyDistance, "Distancia entre puntos", "Point distance")                                                      \
    X(SeamAligned, "Alineada", "Aligned")                                                                             \
    X(SeamNearest, "Más cercana", "Nearest")                                                                          \
    X(SeamRandom, "Aleatoria", "Random")                                                                              \
    X(SeamRear, "Atrás", "Rear")                                                                                      \
    X(TopLayers, "Capas superiores", "Top layers")                                                                    \
    X(BottomLayers, "Capas inferiores", "Bottom layers")                                                              \
    X(TopMinThickness, "Espesor mínimo arriba", "Minimum top thickness")                                              \
    X(BottomMinThickness, "Espesor mínimo abajo", "Minimum bottom thickness")                                         \
    X(SolidEvery, "Capa sólida cada", "Solid layer every")                                                            \
    X(SolidEveryTip, "Las capas N, 2N, 3N… llevan relleno sólido en todo el relleno, impreso como puente sobre el relleno de abajo. 0: nunca. Ver docs/INFILL.md.", "Layers N, 2N, 3N… get solid infill in all the infill, printed as a bridge over the infill below. 0: never. See docs/INFILL.md.") \
    X(SolidBelowArea, "Sólido si el relleno es menor que", "Solid when the infill is under")                       \
    X(SolidBelowAreaTip, "Una isla de relleno más chica que esto (una pata fina, un perno) se llena sólida. 0: nunca.", "An infill island smaller than this (a thin leg, a pin) is filled solid. 0: never.") \
    X(NonplanarInfill, "Relleno ondulado", "Wavy infill")                                                             \
    X(NonplanarInfillTip, "El relleno sube y baja en Z mientras avanza, igual en todas las capas, así cada capa se anida en la de abajo y la unión queda inclinada: más resistencia en Z. Solo con patrones que se repiten capa a capa (grilla, triángulos, estrellas, panal, alineado, concéntrico, curvas). Ver docs/NONPLANAR_INFILL.md.", "The infill goes up and down in Z as it runs, the same in every layer, so each layer nests on the one below and the joint leans: stronger in Z. Only with patterns that repeat layer to layer (grid, triangles, stars, honeycomb, aligned, concentric, curves). See docs/NONPLANAR_INFILL.md.") \
    X(NonplanarAmplitude, "Altura de la onda", "Wave height")                                                         \
    X(NonplanarWavelength, "Largo de la onda", "Wavelength")                                                          \
    X(InterlockWaves, "Ondas entrelazadas", "Interlocking waves") \
    X(InterlockWavesTip, "El relleno sube y baja como un cartón de huevos, igual en cada capa, así las capas encajan en vez de apoyarse en planos lisos.", "The infill rises and falls like an egg crate, the same in every layer, so the layers nest instead of lying on flat planes.") \
    X(WaveAmplitude, "Altura de las ondas", "Wave height") \
    X(WaveLength, "Largo de las ondas", "Wave length") \
    X(TopPattern, "Superficie superior", "Top surface")                                                               \
    X(Monotonic, "Monotónica", "Monotonic")                                                                           \
    X(Rectilinear, "Rectilínea", "Rectilinear")                                                                       \
    X(Grid, "Grilla", "Grid")                                                                                         \
    X(Gyroid, "Giroide", "Gyroid")                                                                                    \
    X(TrianglesInfill, "Triángulos", "Triangles")                                                                     \
    X(Cubic, "Cúbico", "Cubic")                                                                                       \
    X(AdaptiveCubic, "Cúbico adaptativo", "Adaptive cubic")                                                           \
    X(SupportCubic, "Cúbico de soporte", "Support cubic")                                                             \
    X(Lightning, "Rayo", "Lightning")                                                                                 \
    X(Stars, "Estrellas", "Stars")                                                                                    \
    X(AlignedRectilinear, "Rectilíneo alineado", "Aligned rectilinear")                                               \
    X(Zigzag, "Zigzag", "Zigzag")                                                                                     \
    X(LineInfill, "Línea", "Line")                                                                                    \
    X(Honeycomb, "Panal", "Honeycomb")                                                                                \
    X(ConcentricInfill, "Concéntrico", "Concentric")                                                                  \
    X(HilbertInfill, "Hilbert", "Hilbert")                                                                            \
    X(ArchimedeanInfill, "Cuerdas de Arquímedes", "Archimedean chords")                                               \
    X(OctagramInfill, "Espiral octagrama", "Octagram spiral")                                                         \
    X(InfillDensity, "Densidad de relleno", "Infill density")                                                         \
    X(InfillPattern, "Patrón de relleno", "Infill pattern")                                                           \
    X(InfillAngle, "Ángulo de relleno", "Infill angle")                                                               \
    X(Supports, "Soportes", "Supports")                                                                               \
    X(SupportAutomatic, "Soporte automático", "Automatic support")                                                     \
    X(SupportAutomaticTip, "Apagado, solo hay soporte bajo las caras pintadas con Pintar > Soportes",                   \
      "When off, support goes only under the faces painted with Paint > Supports")                                    \
    X(SupportAngle, "Ángulo de voladizo (0: automático)", "Overhang angle (0: automatic)")                           \
    X(SkirtLoops, "Vueltas de falda", "Skirt loops")                                                                  \
    X(SkirtDistance, "Distancia de la falda", "Skirt distance")                                                       \
    X(BrimWidth, "Ancho del borde", "Brim width")                                                                     \
    X(Speeds, "Velocidades", "Speeds")                                                                                \
    X(SpeedWalls, "Paredes internas", "Inner walls")                                                                  \
    X(SpeedOuterWall, "Pared externa", "Outer wall")                                                                  \
    X(SpeedInfill, "Relleno", "Infill")                                                                               \
    X(SpeedSolid, "Relleno sólido", "Solid infill")                                                                   \
    X(SpeedTop, "Superficie superior", "Top surface")                                                                 \
    X(SpeedSupport, "Soporte", "Support")                                                                             \
    X(SpeedTravel, "Viajes", "Travel")                                                                                \
    X(SpeedFirstLayer, "Primera capa", "First layer")                                                                 \
    X(SpeedMax, "Máxima", "Maximum")                                                                                  \
    X(LoadSettings, "Cargar…", "Load…")                                                                               \
    X(SaveSettings, "Guardar…", "Save…")                                                                              \
    X(ResetSettings, "Restablecer", "Reset")                                                                          \
    X(ViewIso, "Perspectiva", "Perspective")                                                                          \
    X(ViewTop, "Arriba", "Top")                                                                                       \
    X(ViewFront, "Frente", "Front")                                                                                   \
    X(ViewBack, "Atrás", "Back")                                                                                      \
    X(ViewLeft, "Izquierda", "Left")                                                                                  \
    X(ViewRight, "Derecha", "Right")                                                                                  \
    X(ZoomIn, "Acercar", "Zoom in")                                                                                   \
    X(ZoomOut, "Alejar", "Zoom out")                                                                                  \
    X(ZoomFit, "Encuadrar", "Frame all")                                                                              \
    X(ToolAdd, "Agregar modelo", "Add a model")                                                                       \
    X(ToolAddTip, "Agregar un modelo a la placa; se ordenan todos", "Add a model to the plate; all of them are arranged") \
    X(ToolRemoveTip, "Quitar el modelo elegido (Supr)", "Remove the chosen model (Delete)")                           \
    X(ToolDuplicateTip, "Duplicar el modelo elegido", "Duplicate the chosen model")                                   \
    X(ToolMoveTip, "Mover (M): arrastrá el modelo sobre la cama o escribí su posición", "Move (M): drag the model on the bed or type its position") \
    X(ToolRotateTip, "Rotar (R): ángulos en X, Y y Z", "Rotate (R): angles about X, Y and Z")                         \
    X(ToolScaleTip, "Escalar (S): en % o en mm", "Scale (S): in % or in mm")                                          \
    X(ToolLayFlatTip, "Apoyar en una cara (F): hacé clic en la cara que va contra la cama", "Lay on face (F): click the face that goes on the bed") \
    X(Info, "Qué hace",\
      "What it does")\
    X(InfoKey, "Clave",\
      "Key")\
    X(InfoDefault, "Por defecto",\
      "Default")\
    X(InfoRange, "Rango",\
      "Range")\
    X(InfoSpanish, "Texto de docs/CONFIG.md",\
      "Text from docs/CONFIG.md, in Spanish")\
    X(InfoTemperatures, "La temperatura que el G-code de inicio pide para la boquilla o la cama: es la variable [temperature] o [bed_temperature] de machine.variables.",\
      "The temperature the start G-code asks for the nozzle or the bed: the [temperature] or [bed_temperature] variable of machine.variables.")\
    X(Tutorial, "Tutorial",\
      "Tutorial")\
    X(TipsAgain, "Ver los consejos de nuevo",\
      "Show the tips again")\
    X(TipsOn, "Consejos al usar cada función",\
      "Tips when using each function")\
    X(GotIt, "Entendido",\
      "Got it")\
    X(NoMoreTips, "No mostrar más consejos",\
      "No more tips")\
    X(Next, "Siguiente",\
      "Next")\
    X(Back, "Atrás",\
      "Back")\
    X(Finish, "Terminar",\
      "Finish")\
    X(SkipTour, "Saltar",\
      "Skip")\
    X(Tour0Title, "Bienvenido a Tisma",\
      "Welcome to Tisma")\
    X(Tour0, "En un minuto te muestro cómo pasar de un modelo a una pieza impresa. Podés volver a ver esto desde el menú de los tres puntos > Tutorial.",\
      "In a minute, how to go from a model to a printed part. You can see this again from the three-dot menu > Tutorial.")\
    X(Tour1Title, "1. Abrí un modelo",\
      "1. Open a model")\
    X(Tour1, "Arrastrá un STL a la ventana o usá Abrir. El modelo queda centrado y apoyado en la cama. Con Agregar sumás más modelos a la placa.",\
      "Drop an STL on the window or use Open. The model sits centred on the bed. Add puts more models on the plate.")\
    X(Tour2Title, "2. Elegí impresora, filamento y perfil",\
      "2. Choose printer, filament and profile")\
    X(Tour2, "Arriba del panel de la izquierda: la impresora, el filamento y el perfil de impresión. El catálogo trae los de muchas marcas.",\
      "At the top of the left panel: the printer, the filament and the print profile. The catalog has those of many brands.")\
    X(Tour3Title, "3. Ajustá lo que necesites",\
      "3. Adjust what you need")\
    X(Tour3, "Cada ajuste tiene un botón (i) al lado de su nombre: dice qué hace, su valor por defecto y su rango.",\
      "Each setting has an (i) button beside its name: what it does, its default and its range.")\
    X(Tour4Title, "4. Ubicá y prepará la pieza",\
      "4. Place and prepare the part")\
    X(Tour4, "La columna a la izquierda de la vista mueve, gira, escala, apoya, ordena y pinta los modelos. Cada herramienta explica cómo se usa la primera vez.",\
      "The column at the left of the view moves, turns, scales, lays flat, arranges and paints the models. Each tool explains itself the first time.")\
    X(Tour5Title, "5. Laminá y revisá",\
      "5. Slice and check")\
    X(Tour5, "Laminar arma el G-code y lo verifica. Vista previa muestra cada capa tal como se va a imprimir.",\
      "Slice makes the G-code and checks it. Preview shows every layer as it will print.")\
    X(Tour6Title, "6. Imprimí",\
      "6. Print")\
    X(Tour6, "Exportar G-code lo guarda en un archivo; Impresora lo manda por USB y muestra cómo va.",\
      "Export G-code saves it to a file; Printer sends it over USB and shows how it goes.")\
    X(TipMoveTitle, "Mover",\
      "Move")\
    X(TipMove, "Arrastrá el modelo sobre la cama, o escribí X e Y en la tarjeta. Atajo: M.",\
      "Drag the model on the bed, or type X and Y in the card. Shortcut: M.")\
    X(TipRotateTitle, "Rotar",\
      "Rotate")\
    X(TipRotate, "Escribí los ángulos en X, Y y Z o usá los giros de 90°. «Sin giro» lo deja como venía. Atajo: R.",\
      "Type the angles about X, Y and Z or use the 90° turns. «No rotation» puts it back. Shortcut: R.")\
    X(TipScaleTitle, "Escalar",\
      "Scale")\
    X(TipScale, "En % o en mm, los tres ejes juntos o cada uno por su lado. Atajo: S.",\
      "In % or in mm, all three axes together or each one apart. Shortcut: S.")\
    X(TipLayTitle, "Apoyar en una cara",\
      "Lay on face")\
    X(TipLay, "Hacé clic en la cara que querés contra la cama: el modelo gira para apoyarla. Atajo: F.",\
      "Click the face you want on the bed: the model turns to lay it down. Shortcut: F.")\
    X(TipArrangeTitle, "Organizar",\
      "Arrange")\
    X(TipArrange, "Ordena todos los modelos en una grilla compacta, centrada en la cama, con 6 mm entre ellos. No los gira. Atajo: A.",\
      "Puts every model in a compact grid, centred on the bed, 6 mm apart. Nothing is turned. Shortcut: A.")\
    X(TipPaintTitle, "Pintar",\
      "Paint")\
    X(TipPaint, "Elegí qué pintar (soportes, costura o piel difusa), Forzar o Bloquear y el tamaño del pincel. Arrastrá sobre el modelo; con Shift, borrás. Atajo: P.",\
      "Choose what to paint (supports, seam or fuzzy skin), Enforce or Block and the brush size. Drag on the model; with Shift, erase. Shortcut: P.")\
    X(TipSliceTitle, "Laminado listo",\
      "Sliced")\
    X(TipSlice, "El motor cortó la pieza en capas, armó el G-code y lo verificó: abajo dice si encontró problemas. Mirá la Vista previa antes de imprimir.",\
      "The engine cut the part into layers, made the G-code and checked it: the bar below says whether it found problems. Look at the Preview before printing.")\
    X(TipPreviewTitle, "Vista previa",\
      "Preview")\
    X(TipPreview, "La barra de la derecha elige hasta qué capa ver. Abajo a la izquierda, los colores por tipo de línea, el tiempo y el filamento.",\
      "The bar at the right chooses the last layer shown. At the bottom left, the colours by kind of line, the time and the filament.")\
    X(TipAnalysisTitle, "Análisis",\
      "Analysis")\
    X(TipAnalysis, "Calcula cómo trabaja la pieza bajo una carga, tal como se imprime: dónde están los esfuerzos y qué tan segura es.",\
      "Works out how the part, as printed, takes a load: where the stresses are and how safe it is.")\
    X(TipCalibrationTitle, "Calibración",\
      "Calibration")\
    X(TipCalibration, "Pruebas para ajustar la impresora y el filamento: torres de temperatura y retracción, flujo y más. Imprimí la prueba, elegí el mejor tramo y aplicá el resultado.",\
      "Tests to tune the printer and the filament: temperature and retraction towers, flow and more. Print the test, pick the best segment and apply the result.")\
    X(TipDeviceTitle, "Impresora",\
      "Printer")\
    X(TipDevice, "Conectá la impresora por USB para ver su estado e imprimir, pausar o cancelar desde acá.",\
      "Connect the printer over USB to see its state and print, pause or cancel from here.")\
    X(ToolPaint, "Pintar", "Paint")                                                                                    \
    X(ToolPaintTip, "Pintar las caras: el soporte, la costura y la piel difusa (P)",                                  \
      "Paint the faces: support, seam and fuzzy skin (P)")                                                            \
    X(PaintSupports, "Soportes", "Supports")                                                                          \
    X(PaintSeam, "Costura", "Seam")                                                                                   \
    X(PaintFuzzy, "Piel difusa", "Fuzzy skin")                                                                        \
    X(PaintColors, "Colores", "Colors")                                                                               \
    X(PaintLater, "Llega cuando el motor tenga varios extrusores", "Comes when the engine has several extruders")   \
    X(PaintEnforce, "Forzar", "Enforce")                                                                              \
    X(PaintBlock, "Bloquear", "Block")                                                                                \
    X(BrushSize, "Pincel", "Brush")                                                                                   \
    X(PaintClear, "Borrar todo", "Clear all")                                                                         \
    X(PaintHint, "Clic izquierdo y arrastrar sobre el modelo para pintar; con Shift, borra. Arrastrar fuera del modelo gira la vista.", \
      "Left click and drag on the model to paint; with Shift, erase. Dragging off the model turns the view.")        \
    X(ToolArrangeTip, "Organizar (A): todos los modelos en una grilla, centrada en la cama", "Arrange (A): every model in a grid, centred on the bed") \
    X(ToolMoveHint, "El centro de su caja en la cama", "The centre of its box on the bed")                            \
    X(ToolLayFlatHint, "Hacé clic en una cara del modelo: queda apoyada en la cama.", "Click a face of the model: it goes down on the bed.") \
    X(ResetRotation, "Sin giro", "No rotation")                                                                       \
    X(Mirror, "Espejar", "Mirror")                                                                                    \
    X(ToolCutTip, "Cortar (C): con un plano horizontal, en dos piezas", "Cut (C): with a horizontal plane, in two pieces") \
    X(ToolCutHint, "El plano corta la pieza a esta altura sobre la cama. Las dos mitades quedan cerradas, cada una como una pieza.", \
      "The plane cuts the part at this height over the bed. Both halves come out closed, each one a part.")           \
    X(ToolMeasureTip, "Medir (U): distancias y ángulos de un modelo", "Measure (U): distances and angles of a model")  \
    X(ToolMeasureHint, "Clic en dos caras del mismo modelo (o Shift + clic: el vértice más cercano). Entre caras paralelas o puntos, la distancia; entre caras que no lo son, el ángulo.", \
      "Click two faces of the same model (or Shift + click: the nearest vertex). Between parallel faces or points, the distance; between faces that are not, the angle.") \
    X(MeasurePlane, "Una cara", "A face")                                                                             \
    X(MeasurePoint, "Un vértice", "A vertex")                                                                         \
    X(MeasureDistance, "Distancia", "Distance")                                                                       \
    X(MeasureAngle, "Ángulo", "Angle")                                                                                \
    X(MeasureShouldBe, "Debería medir", "It should be")                                                               \
    X(MeasureScale, "Escalar", "Scale")                                                                               \
    X(CutHeight, "Altura", "Height")                                                                                  \
    X(CutFlipLower, "Dar vuelta la parte de abajo", "Turn the lower part over")                                       \
    X(CutButton, "Cortar", "Cut")                                                                                     \
    X(SplitButton, "Separar en piezas", "Split into pieces")                                                          \
    X(SplitTip, "Cada pedazo suelto del modelo pasa a ser una pieza, con los huecos que tiene adentro",               \
      "Each loose piece of the model becomes a part, with the cavities inside it")                                    \
    X(SplitOnePiece, "El modelo es una sola pieza", "The model is a single piece")                                                                                     \
    X(ScaleTogether, "Los tres ejes juntos", "All three axes together")                                               \
    X(ResetScale, "Tamaño original", "Original size")                                                                 \
    X(Models, "modelos", "models")                                                                                    \
    X(TabCalibration, "Calibración", "Calibration")                                                                   \
    X(TabDevice, "Impresora", "Printer")                                                                              \
    X(DeviceTitle, "Impresora conectada", "Connected printer")                                                        \
    X(DeviceHint, "Por USB: elegí el puerto y conectá. Imprime lo que está laminado o el G-code abierto. La impresora simulada sirve para probar sin máquina.", "Over USB: pick the port and connect. It prints what is sliced or the opened G-code. The simulated printer is for trying without a machine.") \
    X(DevicePort, "Puerto", "Port")                                                                                   \
    X(DeviceBaud, "Velocidad", "Baud rate")                                                                           \
    X(DeviceSimulated, "Impresora simulada", "Simulated printer")                                                     \
    X(DeviceNoPorts, "No se ven puertos USB: conectá la impresora y tocá Actualizar.", "No USB ports in sight: plug the printer in and press Refresh.") \
    X(DeviceRefresh, "Actualizar", "Refresh")                                                                         \
    X(DeviceConnect, "Conectar", "Connect")                                                                           \
    X(DeviceDisconnect, "Desconectar", "Disconnect")                                                                  \
    X(DeviceStateDisconnected, "Desconectada", "Disconnected")                                                        \
    X(DeviceStateConnecting, "Conectando…", "Connecting…")                                                            \
    X(DeviceStateIdle, "Lista", "Ready")                                                                              \
    X(DeviceStatePrinting, "Imprimiendo", "Printing")                                                                 \
    X(DeviceStatePaused, "En pausa", "Paused")                                                                        \
    X(DeviceStateError, "Error", "Error")                                                                             \
    X(DeviceFirmware, "Firmware: %s", "Firmware: %s")                                                                 \
    X(DeviceNozzle, "Boquilla", "Nozzle")                                                                             \
    X(DeviceBed, "Cama", "Bed")                                                                                       \
    X(DeviceSet, "Fijar", "Set")                                                                                      \
    X(DeviceOff, "Apagar", "Off")                                                                                     \
    X(DevicePrint, "Imprimir", "Print")                                                                               \
    X(DevicePrintWhat, "Manda %s a la impresora, línea por línea.", "Sends %s to the printer, line by line.")         \
    X(DeviceNothingToPrint, "Laminá un modelo o abrí un G-code para imprimir.", "Slice a model or open a G-code file to print.") \
    X(DevicePause, "Pausar", "Pause")                                                                                 \
    X(DeviceResume, "Seguir", "Resume")                                                                               \
    X(DeviceCancel, "Cancelar", "Cancel")                                                                             \
    X(DeviceProgress, "%s · %.1f %% · %s de %s líneas", "%s · %.1f %% · %s of %s lines")                              \
    X(DeviceTimes, "Pasó %s · falta ≈ %s", "Elapsed %s · about %s left")                                              \
    X(DeviceControls, "Mover y extruir", "Move and extrude")                                                          \
    X(DeviceHome, "Inicio", "Home")                                                                                   \
    X(DeviceExtrude, "Extruir", "Extrude")                                                                            \
    X(DeviceRetract, "Retraer", "Retract")                                                                            \
    X(DeviceStep, "Paso", "Step")                                                                                     \
    X(DeviceFan, "Ventilador", "Fan")                                                                                 \
    X(DeviceLog, "Registro", "Log")                                                                                   \
    X(DeviceConnected, "Impresora conectada: %s", "Printer connected: %s")                                            \
    X(DevicePrintStarted, "Imprimiendo %s", "Printing %s")                                                            \
    X(DevicePrintFinished, "Terminó la impresión de %s", "%s finished printing")                                      \
    X(DevicePrintCancelled, "Impresión cancelada: %s", "Print cancelled: %s")                                         \
    X(DevicePrinterError, "Impresora: %s", "Printer: %s")                                                             \
    X(DeviceCannotOpen, "No se pudo abrir el puerto: %s", "Cannot open the port: %s")                                 \
    X(DeviceResends, "%zu líneas reenviadas", "%zu lines sent again")                                                 \
    X(CalibrationTitle, "Calibración de la impresora", "Printer calibration")                                         \
    X(CalibrationHint, "Tocá Crear: la pieza se arma y se lamina sola, con rangos elegidos desde tus ajustes. Imprimila, mirala y elegí lo que salió mejor.", \
      "Press Create: the part is built and sliced by itself, with ranges chosen from your settings. Print it, look at it and pick what came out best.") \
    X(Create, "Crear", "Create")                                                                                      \
    X(CalDimensionsWhat, "Corrige la escala en X, Y y Z y el tamaño de los agujeros.", "Corrects the scale in X, Y and Z and the size of holes.") \
    X(CalTemperatureWhat, "Voladizos, techo plano, resistencia entre capas y detalle, a cada temperatura.",             \
      "Overhangs, a flat roof, layer strength and detail, at each temperature.")                                       \
    X(CalRetractionWhat, "El largo de retracción que no deja hilos.", "The retraction length that leaves no strings.")  \
    X(CalFlowWhat, "El flujo que da caras lisas y medidas justas.", "The flow that gives smooth faces and true sizes.") \
    X(CalSpeedWhat, "La velocidad más alta que aguanta el fusor.", "The highest speed the hot end can take.")          \
    X(StepCreated, "1 · Creada y laminada", "1 · Created and sliced")                                                 \
    X(StepPrint, "2 · Imprimila y mirala", "2 · Print it and look at it")                                             \
    X(StepChoose, "3 · ¿Qué segmento salió mejor?", "3 · Which segment came out best?")                              \
    X(StepMeasure, "3 · Medila con calibre y cargá las medidas", "3 · Measure it with calipers and enter the measures") \
    X(TowerReady, "Lista: %d segmentos, %s de impresión", "Ready: %d segments, %s to print")                           \
    X(PartReady, "Lista: %s de impresión", "Ready: %s to print")                                                      \
    X(UseValue, "Usar %s", "Use %s")                                                                                  \
    X(ValueApplied, "%s: guardado en los ajustes", "%s: saved in the settings")                                        \
    X(AdjustRange, "Ajustar el rango (opcional)", "Adjust the range (optional)")                                      \
    X(Recreate, "Volver a crear", "Create again")                                                                     \
    X(CalDimensions, "Dimensiones (precisión geométrica)", "Dimensions (geometric accuracy)")                        \
    X(CalDimensionsHint, "Bloque de 30 × 20 × 15 mm con un agujero de 10 mm. Medí con calibre: X es el lado largo.",     \
      "A 30 × 20 × 15 mm block with a 10 mm hole. Measure with calipers: X is the long side.")                        \
    X(MeasuredX, "X medido", "Measured X")                                                                            \
    X(MeasuredY, "Y medido", "Measured Y")                                                                            \
    X(MeasuredZ, "Z medido", "Measured Z")                                                                            \
    X(MeasuredHole, "Agujero medido", "Measured hole")                                                                \
    X(Current, "Actual", "Current")                                                                                   \
    X(New, "Nueva", "New")                                                                                            \
    X(HoleCorrection, "Agujeros", "Holes")                                                                            \
    X(ApplyCompensation, "Aplicar la compensación", "Apply the compensation")                                         \
    X(ResetCompensation, "Quitar la compensación", "Remove the compensation")                                         \
    X(CompensationApplied, "Compensación aplicada: X %.3f %%, Y %.3f %%, Z %.3f %%, agujeros %+.3f mm",               \
      "Compensation applied: X %.3f %%, Y %.3f %%, Z %.3f %%, holes %+.3f mm")                                        \
    X(CalTemperature, "Torre de temperatura", "Temperature tower")                                                    \
    X(CalTemperatureHint, "En cada segmento mirá: la cresta de la izquierda (hasta qué ángulo sale prolija), el estante plano de la derecha, el número y las figuras del frente. Empujá el perno de atrás con el dedo: el que más resiste une mejor las capas.", \
      "On each segment look at: the ridge on the left (up to which angle it stays clean), the flat shelf on the right, the number and figures on the front. Push the pin at the back with a finger: the one that resists most bonds its layers best.") \
    X(CalRetraction, "Torre de retracción", "Retraction tower")                                                       \
    X(CalRetractionHint, "Buscá el primer segmento sin hilos entre las dos columnas: ese largo alcanza.",              \
      "Find the first segment without strings between the two columns: that length is enough.")                      \
    X(CalFlow, "Flujo", "Flow")                                                                                       \
    X(CalFlowHint, "Mirá la cara de arriba de cada segmento (el anillo junto al cuello): ni huecos ni rebabas.",       \
      "Look at the top face of each segment (the ring by the neck): neither gaps nor ridges.")                        \
    X(CalSpeed, "Velocidad máxima", "Maximum speed")                                                                  \
    X(CalSpeedHint, "Elegí el último segmento con paredes sanas, sin huecos ni capas flojas.",                         \
      "Pick the last segment with sound walls, without gaps or weak layers.")                                         \
    X(CalFirstLayer, "Primera capa", "First layer")                                                                   \
    X(CalFirstLayerWhat, "La altura de la boquilla y el nivel de la cama, en toda la cama.", "The nozzle's height and the bed's level, all over the bed.") \
    X(CalFirstLayerHint, "Cinco cuadrados de una capa: en el medio y cerca de cada esquina. Mirá cada uno de cerca y pasá el dedo.", "Five squares one layer thick: in the middle and near each corner. Look at each one closely and run a finger over it.") \
    X(StepJudge, "3 · ¿Cómo salió cada cuadrado?", "3 · How did each square come out?")                               \
    X(FirstLayerJudge, "Líneas redondas y separadas, que se despegan: la boquilla está alta; bajala. Superficie rugosa, transparente o con rebabas: está baja; subila. Líneas planas que se tocan sin huecos y cara lisa: bien. Si un cuadrado sale distinto de los otros, la cama no está nivelada en esa esquina. La altura se ajusta en la impresora (Z offset o babystepping).", "Round, separate lines that peel off: the nozzle is too high; lower it. A rough, see-through surface or ridges: it is too low; raise it. Flat lines that touch with no gaps and a smooth face: right. If one square differs from the others, the bed is not level at that corner. The height is set on the printer (Z offset or babystepping).") \
    X(CalRetractionSpeed, "Velocidad de retracción", "Retraction speed")                                              \
    X(CalRetractionSpeedWhat, "Qué tan rápido retraer, con el largo que ya tenés.", "How fast to retract, with the length you already have.") \
    X(CalRetractionSpeedHint, "Las dos columnas de la torre de retracción, cada segmento a otra velocidad. Elegí el más lento sin hilos: más rápido no ayuda y puede morder el filamento.", "The retraction tower's two columns, each segment at another speed. Pick the slowest one without strings: faster does not help and can grind the filament.") \
    X(CalPressureAdvance, "Avance de presión", "Pressure advance")                                                    \
    X(CalPressureAdvanceWhat, "Esquinas vivas y líneas parejas a cualquier velocidad.", "Sharp corners and even lines at any speed.") \
    X(CalPressureAdvanceHint, "Un tubo cuadrado de esquinas vivas. Con poco avance, las esquinas salen abultadas; con mucho, finas o con huecos al empezar cada línea. Elegí el segmento de esquinas más prolijas. La impresora tiene que tenerlo activado (en Marlin, LIN_ADVANCE).", "A square tube with sharp corners. With too little advance the corners bulge; with too much they come out thin or with gaps where lines start. Pick the segment with the neatest corners. The printer must have it enabled (in Marlin, LIN_ADVANCE).") \
    X(CalFit, "Tolerancias de encastre", "Fit tolerances")                                                            \
    X(CalFitWhat, "El juego que necesitan las piezas que encastran.", "The clearance that parts which fit together need.") \
    X(CalFitHint, "Siete bloques con un perno impreso dentro de cada agujero, con 0,1 a 0,5 mm de juego. Empujá cada perno: elegí el de menos juego que sale o gira.", "Seven blocks with a pin printed inside each hole, with 0.1 to 0.5 mm of clearance. Push each pin: pick the one with the least clearance that comes out or turns.") \
    X(CalMaxFlow, "Tope de flujo volumétrico", "Volumetric flow limit")                                                \
    X(CalMaxFlowWhat, "Cuántos mm³/s funde el fusor; el motor no pasa de ahí.", "How many mm³/s the hot end melts; the engine never goes past it.") \
    X(CalMaxFlowHint, "El tubo cuadrado, cada segmento con todas sus líneas a ese flujo (sin tope ni enfriamiento). Elegí el último segmento con paredes llenas y parejas: desde el siguiente, el fusor no da abasto.", "The square tube, each segment with every line at that flow (no cap, no slowdown). Pick the last segment with full, even walls: from the next one on, the hot end cannot keep up.") \
    X(CalAcceleration, "Resonancias (aceleración)", "Resonance (acceleration)")                                       \
    X(CalAccelerationWhat, "La aceleración que no deja ondas después de las esquinas.", "The acceleration that leaves no ripples after the corners.") \
    X(CalAccelerationHint, "El tubo cuadrado, cada segmento a otra aceleración (M204, o SET_VELOCITY_LIMIT en Klipper). Mirá las paredes justo después de cada esquina: elegí el último segmento sin ondas. Con input shaping en Klipper, calibralo primero.", "The square tube, each segment at another acceleration (M204, or SET_VELOCITY_LIMIT on Klipper). Look at the walls right after each corner: pick the last segment without ripples. With input shaping on Klipper, calibrate that first.") \
    X(CalElephantFoot, "Pata de elefante", "Elephant's foot")                                                          \
    X(CalElephantFootWhat, "Cuánto se ensancha la primera capa, para achicarla.", "How much the first layer widens, to shrink it.") \
    X(CalElephantFootHint, "Un bloque de 20 × 20 × 6 mm. Medí con calibre el ancho del borde de abajo (la primera capa) y el del cuerpo, en el mismo lado.", "A 20 × 20 × 6 mm block. Measure with calipers the width of the bottom edge (the first layer) and the body's, on the same side.") \
    X(CalResinExposure, "Torre de exposición", "Exposure tower") \
    X(CalResinExposureWhat, "Qué exposición deja los agujeros abiertos y las aletas firmes", "Which exposure keeps the holes open and the fins standing") \
    X(CalResinExposureHint, "Una franja por exposición, sobre una base. Elegí la más baja cuyas aletas finas salieron derechas y cuyos agujeros chicos no se cerraron.", "A band per exposure, over a base. Pick the lowest whose thin fins came out straight and whose small holes did not close.") \
    X(CalResinMeasure, "Bloque de medidas", "Measuring block") \
    X(CalResinMeasureWhat, "La corrección de escala de la resina", "The resin's scale correction") \
    X(CalResinMeasureHint, "Un bloque de 30 × 30 × 10 mm. Medilo lejos del borde de abajo, que se ensancha.", "A 30 × 30 × 10 mm block. Measure it away from its bottom edge, which spreads.") \
    X(ResinScaleApplied, "Corrección de escala de la resina aplicada", "Resin scale correction applied") \
    X(MeasuredBottom, "Ancho abajo", "Width at the bottom")                                                             \
    X(MeasuredBody, "Ancho del cuerpo", "Body width")                                                                   \
    X(FirstLayerShrink, "Achique de la primera capa", "First layer shrink")                                             \
    X(ApplyShrink, "Aplicar el achique", "Apply the shrink")                                                            \
    X(ShrinkApplied, "Primera capa achicada %.3f mm", "First layer shrunk by %.3f mm")                                  \
    X(StepChooseFit, "3 · ¿Cuál es el de menos juego que se suelta?", "3 · Which is the least clearance that comes free?") \
    X(FitAdvice, "Para piezas que encastran, dejá %s de juego por lado en tus diseños.", "For parts that fit together, leave %s of clearance per side in your designs.") \
    X(VolumetricFlow, "A %s: hasta %.1f mm³/s en la línea más rápida.", "At %s: up to %.1f mm³/s on the fastest line.") \
    X(CalExtruder, "Pasos del extrusor", "Extruder steps")                                                            \
    X(CalExtruderWhat, "Que el extrusor empuje lo que se le pide.", "That the extruder pushes what it is asked to.")  \
    X(ExtruderStep1, "1 · Marcá el filamento 120 mm arriba de la entrada del extrusor y corré este G-code: calienta y extruye 100 mm despacio.", "1 · Mark the filament 120 mm above the extruder's entry and run this G-code: it heats up and extrudes 100 mm slowly.") \
    X(ExportTest, "Exportar el G-code de prueba", "Export the test G-code")                                           \
    X(ExtruderStep2, "2 · Medí cuánto quedó entre la entrada y la marca", "2 · Measure what is left between the entry and the mark") \
    X(Remaining, "Quedó hasta la marca", "Left to the mark")                                                          \
    X(CurrentSteps, "Pasos por mm actuales (M503)", "Current steps per mm (M503)")                                    \
    X(CurrentRotation, "rotation_distance actual", "Current rotation_distance")                                       \
    X(ExtruderStep3, "3 · Guardalo en la impresora", "3 · Store it in the printer")                                   \
    X(ExtruderResult, "Extruyó %.1f mm de 100: el valor nuevo es %.3f.", "It extruded %.1f mm of 100: the new value is %.3f.") \
    X(ExtruderWhere, "En Klipper va en la sección [extruder] de printer.cfg; en RepRapFirmware, en config.g; en Marlin, por la consola.", "In Klipper it goes in printer.cfg's [extruder] section; in RepRapFirmware, in config.g; in Marlin, through the console.") \
    X(Copy, "Copiar", "Copy")                                                                                         \
    X(Copied, "Copiado", "Copied")                                                                                    \
    X(TestExported, "G-code de prueba guardado: %s", "Test G-code saved: %s")                                         \
    X(TowerStart, "Desde", "From")                                                                                    \
    X(TowerEnd, "Hasta", "To")                                                                                        \
    X(TowerStep, "Paso", "Step")                                                                                      \
    X(TowerSegment, "Alto de cada segmento", "Segment height")                                                        \
    X(SegmentAt, "Segmento: %s", "Segment: %s")                                                                       \
    X(MoreCalibrations, "Próximas calibraciones", "Coming calibrations")                                              \
    X(MoreCalibrationsList, "Puentes y voladizos · compensación por mínimos cuadrados · ancho de línea variable · adherencia entre capas con el análisis · laminado no planar: altura y ángulo (a definir). Ver docs/CALIBRATION.md.", \
      "bridges and overhangs · least-squares compensation · variable line width · layer adhesion with the analysis · non-planar slicing: height and angle (to be defined). See docs/CALIBRATION.md.") \
    X(Render, "Imagen", "Rendering")                                                                                  \
    X(RenderTip, "Cómo se dibuja la vista: luz, oclusión ambiental, ray tracing y suavizado de bordes",                \
      "How the view is drawn: light, ambient occlusion, ray tracing and anti-aliasing")                               \
    X(RenderFast, "Rápida", "Fast")                                                                                   \
    X(RenderAmbient, "Oclusión ambiental", "Ambient occlusion")                                                       \
    X(RenderRay, "Ray tracing", "Ray tracing")                                                                        \
    X(RenderFastTip, "Luz por píxel, sin oclusión: lo más liviano", "Per-pixel light, no occlusion: the lightest")     \
    X(RenderAmbientTip, "Oscurece rincones y uniones entre líneas, calculado en pantalla",                            \
      "Darkens corners and the joints between lines, computed on screen")                                             \
    X(RenderRayTip, "Sombras suaves y oclusión con rayos contra la escena; la imagen se afina cuadro a cuadro",       \
      "Soft shadows and occlusion with rays against the scene; the picture refines frame after frame")               \
    X(AoStrength, "Intensidad de la oclusión", "Occlusion strength")                                                  \
    X(Antialiasing, "Suavizado de bordes", "Anti-aliasing")                                                           \
    X(MsaaOff, "Sin multimuestreo", "No multisampling")                                                               \
    X(Fxaa, "FXAA (bordes finos y brillos)", "FXAA (thin edges and highlights)")                                      \
    X(MoireFilter, "Filtro de moiré (líneas más finas que un píxel)", "Moiré filter (lines finer than a pixel)")      \
    X(MoireFilterTip, "De lejos, las líneas de capa y la grilla se funden en vez de hacer bandas",                    \
      "From far away, the layer lines and the grid blend instead of banding")                                         \
    X(RaySamples, "Muestras: %d de %d", "Samples: %d of %d")                                                          \
    X(RayPreparing, "Preparando la escena para los rayos…", "Preparing the scene for the rays…")                     \
    X(RayTooBig, "La escena es demasiado grande para el ray tracing en esta GPU", "The scene is too large for ray tracing on this GPU") \
    X(DropHint, "Arrastrá un STL o un G-code acá, o usá Abrir", "Drop an STL or G-code file here, or use Open")      \
    X(KindTravel, "Viajes", "Travel")                                                                                 \
    X(KindOuterWall, "Pared externa", "Outer wall")                                                                   \
    X(KindInnerWall, "Pared interna", "Inner wall")                                                                   \
    X(KindOverhangWall, "Pared en voladizo", "Overhang wall")                                                         \
    X(KindSparseInfill, "Relleno", "Infill")                                                                          \
    X(KindSolidInfill, "Relleno sólido", "Solid infill")                                                              \
    X(KindTopSolidInfill, "Superficie superior", "Top surface")                                                       \
    X(KindBridgeInfill, "Puente", "Bridge")                                                                           \
    X(KindGapFill, "Relleno de huecos", "Gap fill")                                                                   \
    X(KindIroning, "Planchado", "Ironing")                                                                            \
    X(KindSkirtBrim, "Falda y borde", "Skirt and brim")                                                               \
    X(KindSupport, "Soporte", "Support")                                                                              \
    X(KindSupportInterface, "Interfaz de soporte", "Support interface")                                               \
    X(KindWipeTower, "Torre de purga", "Wipe tower")                                                                  \
    X(KindCustom, "G-code del usuario", "User G-code")                                                                \
    X(KindOther, "Otro", "Other")                                                                                     \
    X(ColorBy, "Color por", "Color by")                                                                               \
    X(ByKind, "Tipo de línea", "Line type")                                                                           \
    X(BySpeed, "Velocidad", "Speed")                                                                                  \
    X(ByWidth, "Ancho", "Width")                                                                                      \
    X(ByHeight, "Alto de capa", "Layer height")                                                                       \
    X(ByActualSpeed, "Velocidad alcanzada", "Actual speed") \
    X(ByFan, "Ventilador", "Fan speed") \
    X(ByTemperature, "Temperatura", "Temperature") \
    X(ByFlow, "Caudal", "Volumetric flow") \
    X(ByActualFlow, "Caudal alcanzado", "Actual volumetric flow") \
    X(ByLayerTime, "Tiempo de capa (lineal)", "Layer time (linear)") \
    X(ByLayerTimeLog, "Tiempo de capa (logarítmico)", "Layer time (logarithmic)") \
    X(ByTool, "Extrusor", "Tool") \
    X(ByColorPrint, "Color por capa", "Color print") \
    X(ToolN, "Extrusor %d", "Tool %d") \
    X(NoTypes, "Sin tipos: el G-code no dice qué es cada línea", "No types: the G-code does not say what each line is") \
    X(GcodePreviewMenu, "Vista previa de G-code…", "G-code preview…") \
    X(AddAtLayer, "Agregar en esta capa un cambio de color (clic derecho: más)", "Add a colour change at this layer (right click: more)") \
    X(AddColorChange, "Cambio de color", "Colour change") \
    X(AddPause, "Pausa…", "Pause…") \
    X(AddCustomGcode, "G-code propio…", "Custom G-code…") \
    X(PauseMessage, "Mensaje que muestra la impresora", "Message the printer shows") \
    X(CustomGcodeText, "G-code al empezar la capa", "G-code at the layer's start") \
    X(ChangeAtLayer, "Capa %d", "Layer %d") \
    X(RemoveChange, "Quitar", "Remove") \
    X(ColorChangeAt, "Cambio de color en la capa %d", "Colour change at layer %d") \
    X(PauseAt, "Pausa en la capa %d", "Pause at layer %d") \
    X(GcodeAt, "G-code propio en la capa %d", "Custom G-code at layer %d") \
    X(MoveN, "Movimiento %d de %d", "Move %d of %d") \
    X(IconTravels, "Viajes: los movimientos sin extruir", "Travels: the moves that do not extrude") \
    X(IconWipes, "Barridos: la boquilla que barre al terminar un lazo", "Wipes: the nozzle wiping at a loop's end") \
    X(IconRetracts, "Retracciones: donde se retrae el filamento", "Retractions: where the filament is pulled back") \
    X(IconUnretracts, "Vueltas de retracción: donde se vuelve a empujar", "Unretractions: where it is pushed back") \
    X(IconSeams, "Costuras: donde empieza cada lazo de pared externa", "Seams: where each outer wall loop starts") \
    X(IconToolChanges, "Cambios de herramienta", "Tool changes") \
    X(IconColorChanges, "Cambios de color", "Colour changes") \
    X(IconPauses, "Pausas", "Pauses") \
    X(IconCustom, "G-code propio: donde va un G-code puesto por la persona", "Custom G-code: where the user's own G-code goes") \
    X(IconGravity, "Centro de gravedad de lo impreso", "Centre of gravity of what is printed") \
    X(IconNozzle, "Boquilla en el último movimiento dibujado", "Nozzle at the last move drawn") \
    X(NozzleAt, "X %.2f · Y %.2f · Z %.2f · %.0f mm/s", "X %.2f · Y %.2f · Z %.2f · %.0f mm/s") \
    X(ColorN, "Color %d", "Color %d") \
    X(FirstLayerTime, "Primera capa", "First layer") \
    X(MovesTime, "Movimientos", "Moves") \
    X(Layer, "Capa", "Layer")                                                                                         \
    X(LayerOf, "Capa %d de %d  ·  Z %.2f mm", "Layer %d of %d  ·  Z %.2f mm")                                         \
    X(OnlyThisLayer, "Solo esta capa", "This layer only")                                                             \
    X(EstimatedTime, "Tiempo estimado", "Estimated time")                                                             \
    X(FilamentUsed, "Filamento", "Filament")                                                                          \
    X(Layers, "Capas", "Layers")                                                                                      \
    X(Lines, "Líneas", "Lines")                                                                                       \
    X(Triangles, "triángulos", "triangles")                                                                           \
    X(NoModel, "Sin modelo", "No model")                                                                              \
    X(Ready, "Listo", "Ready")                                                                                        \
    X(Slicing, "Laminando…", "Slicing…")                                                                              \
    X(SlicedIn, "Laminado en %.2f s", "Sliced in %.2f s")                                                             \
    X(GcodeAgainIn, "G-code rehecho en %.2f s (el corte no cambió)", "G-code made again in %.2f s (the cut did not change)") \
    X(ViewTime, "Vista %.1f ms · %.0f FPS", "View %.1f ms · %.0f FPS")                                                \
    X(ViewTimeTip, "Lo que tarda la GPU en dibujar la vista 3D en cada cuadro, y los cuadros por segundo que eso "     \
      "permite (docs/LARGE_MODELS.md)",                                                                               \
      "What the GPU takes to draw the 3D view each frame, and the frames per second that allows "                     \
      "(docs/LARGE_MODELS.md)")                                                                                       \
    X(Stale, "El modelo o los ajustes cambiaron: volvé a laminar", "The model or the settings changed: slice again")  \
    X(GcodeOk, "G-code verificado: %s líneas, sin problemas", "G-code checked: %s lines, no problems")                \
    X(GcodeBad, "El G-code no pasó la verificación (%d problemas)", "The G-code failed its check (%d problems)")      \
    X(Exported, "Exportado y verificado: %s", "Exported and checked: %s")                                             \
    X(ExportFailed, "No se pudo exportar el G-code", "The G-code could not be exported")                              \
    X(CannotSlice, "No se puede laminar", "Cannot slice")                                                             \
    X(CannotOpen, "No se pudo abrir el archivo", "The file could not be opened")                                      \
    X(SettingsLoaded, "Ajustes cargados: %s", "Settings loaded: %s")                                                  \
    X(SettingsSaved, "Ajustes guardados: %s", "Settings saved: %s")                                                   \
    X(FromCatalog, "Agregar desde el catálogo…", "Add from the catalog…")                                             \
    X(PrinterCatalog, "Catálogo de impresoras", "Printer catalog")                                                    \
    X(FilamentCatalog, "Catálogo de filamentos", "Filament catalog")                                                  \
    X(CatalogSearch, "Buscar marca o modelo", "Search brand or model")                                                \
    X(CatalogCount, "%d de %d", "%d of %d")                                                                           \
    X(CatalogHint, "Datos de las páginas oficiales de cada fabricante, con sus fuentes. Elegí uno y agregalo a tus perfiles.", "Data from each maker's official pages, with their sources. Pick one and add it to your profiles.") \
    X(BuildVolume, "Volumen de impresión", "Build volume")                                                            \
    X(BedRound, "cama redonda de %g mm", "round bed of %g mm")                                                        \
    X(Firmware, "Firmware", "Firmware")                                                                               \
    X(OwnFirmware, "propio del fabricante", "the maker's own")                                                        \
    X(Extruder, "Extrusor", "Extruder")                                                                               \
    X(DirectDrive, "directo", "direct drive")                                                                         \
    X(Bowden, "Bowden", "Bowden")                                                                                     \
    X(MaxTemps, "Máximos: boquilla %s, cama %s", "Maximums: nozzle %s, bed %s")                                       \
    X(MaxMotion, "Velocidad máx. %s, aceleración máx. %s", "Max speed %s, max acceleration %s")                       \
    X(MultiMaterial, "Multimaterial", "Multi-material")                                                               \
    X(Enclosed, "Cerrada", "Enclosed")                                                                                \
    X(Sources, "Fuentes", "Sources")                                                                                  \
    X(OwnStartGcode, "Este firmware tiene su propia secuencia de inicio: revisá el G-code de inicio antes de imprimir.", "This firmware has its own start sequence: check the start G-code before printing.") \
    X(NozzleSize, "Boquilla", "Nozzle")                                                                               \
    X(AddToProfiles, "Agregar a mis perfiles", "Add to my profiles")                                                  \
    X(Material, "Material", "Material")                                                                               \
    X(NozzleRange, "Boquilla", "Nozzle")                                                                              \
    X(BedRange, "Cama", "Bed")                                                                                        \
    X(Recommended, "recomendada", "recommended")                                                                      \
    X(Density, "Densidad", "Density")                                                                                 \
    X(NeedsEnclosure, "Necesita impresora cerrada", "Needs an enclosed printer")                                      \
    X(CatalogEmpty, "Sin resultados", "No results")                                                                   \
    X(ProfileAdded, "Agregado a tus perfiles: %s", "Added to your profiles: %s")                                      \
    X(SpeedFallback, "La impresora no trae velocidades: se usan las de una Ender 3 de fábrica (revisalas en "         \
      "Impresora › Velocidades)",                                                                                     \
      "The printer has no speeds of its own: a stock Ender-3's are used (check them under Printer › Speeds)")         \
    X(CatalogSpeedFallback, "Agregado a tus perfiles: %s. El catálogo no trae sus velocidades: van las de una Ender " \
      "3 de fábrica, hasta su velocidad máxima (revisalas en Impresora › Velocidades)",                               \
      "Added to your profiles: %s. The catalog has no speeds for it: a stock Ender-3's, up to its top speed (check "  \
      "them under Printer › Speeds)")                                                                                 \
    X(Kinematics, "Cinemática", "Kinematics")                                                                         \
    X(MaxFlow, "Flujo máximo", "Maximum flow")                                                                        \
    X(ChamberHeating, "Cámara", "Chamber")                                                                            \
    X(HardenedNozzle, "Boquilla endurecida", "Hardened nozzle")                                                       \
    X(Hotend, "Hotend", "Hot end")                                                                                    \
    X(Probe, "Nivelación", "Leveling probe")                                                                          \
    X(BedSurfaces, "Superficies de cama", "Bed surfaces")                                                             \
    X(Connectivity, "Conexión", "Connectivity")                                                                       \
    X(Features, "Funciones", "Features")                                                                              \
    X(MaterialSlots, "Ranuras de filamento", "Filament slots")                                                        \
    X(Tools, "Cabezales", "Tool heads")                                                                               \
    X(AbrasiveFilament, "Abrasivo: necesita boquilla endurecida", "Abrasive: needs a hardened nozzle")                \
    X(Drying, "Secado", "Drying")                                                                                     \
    X(Soluble, "Soluble (para soportes)", "Soluble (for supports)")                                                   \
    X(Hardness, "Dureza", "Hardness")                                                                                 \
    X(Shrinkage, "Contracción", "Shrinkage")                                                                          \
    X(Warnings, "%d avisos", "%d warnings")                                                                           \
    X(NoProfile, "Sin perfil", "No profile")                                                                          \
    X(BuiltinProfiles, "De fábrica", "Built-in")                                                                      \
    X(MyProfiles, "Mis perfiles", "My profiles")                                                                      \
    X(ProfileModifiedTip, "Cambiaste ajustes de este perfil: guardalos con el botón de guardar, o como un perfil nuevo con ⋮.", "You changed settings of this profile: save them with the save button, or as a new profile with ⋮.") \
    X(SaveProfileTip, "Guardar los cambios en el perfil", "Save the changes to the profile")                          \
    X(SaveProfileAs, "Guardar como perfil nuevo…", "Save as a new profile…")                                          \
    X(DeleteProfile, "Eliminar «%s»", "Delete \"%s\"")                                                                \
    X(SaveProfilePrinter, "Guardar el perfil de la impresora", "Save the printer profile")                            \
    X(SaveProfileFilament, "Guardar el perfil del filamento", "Save the filament profile")                            \
    X(SaveProfilePrint, "Guardar el perfil de impresión", "Save the print profile")                                   \
    X(ProfileIsBuiltin, "Ese nombre es de un perfil de fábrica.", "That name belongs to a built-in profile.")         \
    X(ProfileReplaces, "Reemplaza el perfil que ya existe.", "Replaces the profile that already exists.")             \
    X(ProfileSaved, "Perfil guardado: %s", "Profile saved: %s")                                                       \
    X(ProfileDeleted, "Perfil eliminado: %s", "Profile deleted: %s")                                                  \
    X(ProfilesSkipped, "%d archivos de perfil no se pudieron leer", "%d profile files could not be read")             \
    X(CannotSave, "No se pudo guardar", "Could not save")                                                             \
    X(SaveProject, "Guardar el proyecto", "Save the project")                                                         \
    X(SaveProjectAs, "Guardar el proyecto como…", "Save the project as…")                                             \
    X(ProjectSaved, "Proyecto guardado: %s", "Project saved: %s")                                                     \
    X(ProjectOpened, "Proyecto abierto: %s", "Project opened: %s")                                                    \
    X(ProjectFirstModel, "El proyecto tiene %d piezas; por ahora se abre la primera.", "The project has %d models; for now the first one opens.") \
    X(FlowRow, "Flujo", "Flow")                                                                                       \
    X(FlowTip, "El porcentaje del volumen que se extruye en todas las líneas; lo ajusta la calibración de flujo", "The percentage of the volume every line extrudes; the flow calibration sets it") \
    X(GcodeOpened, "G-code abierto: %s", "G-code opened: %s")                                                         \
    X(ViewerOnly, "Vista de un G-code abierto: no se puede volver a laminar", "Viewing an opened G-code file: it cannot be sliced again") \
    X(Details, "Detalles", "Details")                                                                                 \
    X(Close, "Cerrar", "Close")                                                                                       \
    X(Cancel, "Cancelar", "Cancel")                                                                                   \
    X(OpenFile, "Abrir archivo", "Open file")                                                                         \
    X(SaveGcode, "Guardar el G-code", "Save the G-code")                                                              \
    X(SaveSettingsTitle, "Guardar los ajustes", "Save the settings")                                                  \
    X(LoadSettingsTitle, "Cargar ajustes", "Load settings")                                                           \
    X(OpenButton, "Abrir", "Open")                                                                                    \
    X(SaveButton, "Guardar", "Save")                                                                                  \
    X(FileName, "Nombre", "Name")                                                                                     \
    X(Folder, "Carpeta", "Folder")                                                                                    \
    X(Up, "Subir", "Up")                                                                                              \
    X(ShowAllFiles, "Mostrar todos los archivos", "Show all files")                                                   \
    X(Replace, "Ya existe. ¿Reemplazarlo?", "It already exists. Replace it?")                                         \
    X(Yes, "Sí", "Yes")                                                                                               \
    X(No, "No", "No")                                                                                                 \
    X(Language, "Idioma", "Language")                                                                                 \
    X(Theme, "Tema", "Theme")                                                                                         \
    X(Dark, "Oscuro", "Dark")                                                                                         \
    X(Light, "Claro", "Light")                                                                                        \
    X(About, "Acerca de Tisma", "About Tisma")                                                                        \
    X(AboutText, "Tisma engine %s: un laminador propio, escrito desde cero.",                                         \
      "Tisma engine %s: an own slicer, written from scratch.")                                                        \
    X(AboutLibraries, "Usa GLFW, Dear ImGui, nanosvg, oneTBB, Clipper2, nlohmann/json y volk; la fuente es Roboto (Apache-2.0). Sus licencias y las fuentes de cada función, en Más › Créditos y fuentes.", \
      "Uses GLFW, Dear ImGui, nanosvg, oneTBB, Clipper2, nlohmann/json and volk; the font is Roboto (Apache-2.0). Their licenses and where each function comes from, in More › Credits and sources.") \
    X(BasedOn, "Basado en", "Based on")                                                                               \
    X(BasedOnBy, ", de ", ", by ")                                                                                    \
    X(Credits, "Créditos y fuentes", "Credits and sources")                                                           \
    X(CreditsIntro, "De dónde sale cada función y los componentes de terceros que lleva el programa, con sus licencias.", \
      "Where each function comes from, and the third-party components the program carries, with their licenses.") \
    X(CreditsFeatures, "Funciones", "Functions")                                                                      \
    X(CreditsLibraries, "Componentes de terceros", "Third-party components")                                          \
    X(CreditsFunction, "Función", "Function")                                                                         \
    X(CreditsSource, "Basado en", "Based on")                                                                         \
    X(CreditsAuthor, "Autor", "Author")                                                                               \
    X(CreditsLicense, "Licencia o tipo", "License or kind")                                                           \
    X(CreditsComponent, "Componente", "Component")                                                                    \
    X(CreditsVersion, "Versión", "Version")                                                                           \
    X(CreditsNotice, "Aviso de la licencia", "License notice")                                                        \
    X(CreditsShowNotice, "Ver el aviso", "Show the notice")                                                           \
    X(CreditsLibraryLicense, "Licencia", "License")                                                                   \
    X(WarnEnclosure, "Este material necesita una impresora cerrada", "This material needs an enclosed printer")       \
    X(WarnAbrasive, "Este material gasta las boquillas de latón: usá una de acero endurecido",                         \
      "This material wears brass nozzles out: use a hardened steel one")                                              \
    X(WarnBedTemperature, "Tu cama no llega a la temperatura de este material",                                       \
      "Your bed does not reach this material's temperature")                                                          \
    X(WarnNozzleTemperature, "Tu hotend no llega a la temperatura de este material",                                  \
      "Your hot end does not reach this material's temperature")                                                      \
    X(WarnDry, "Conviene secarlo antes de imprimir", "Better dry it before printing")                                 \
    X(RangeTo, " a ", " to ")                                                                                         \
    X(Help, "Ayuda", "Help")                                                                                          \
    X(HelpTip, "Ayuda: problemas de impresión, procedimientos, glosario y tutoriales, sin conexión (F1)",              \
      "Help: print problems, procedures, glossary and tutorials, offline (F1)")                                        \
    X(HelpProblems, "Problemas", "Problems")                                                                          \
    X(HelpProcedures, "Procedimientos", "Procedures")                                                                 \
    X(HelpGlossary, "Glosario", "Glossary")                                                                           \
    X(HelpTutorials, "Tutoriales", "Tutorials")                                                                       \
    X(HelpCalibrations, "Calibraciones", "Calibrations")                                                              \
    X(HelpSearch, "Buscar en la ayuda", "Search the help")                                                            \
    X(HelpNoData, "La ayuda se descarga con los perfiles; se va a bajar cuando haya conexión.",                       \
      "The help is downloaded with the profiles; it will be when there is a connection.")                             \
    X(HelpNotRead, "Los datos de la ayuda están en esta computadora, pero esta versión todavía no los lee.",          \
      "The help's data is on this computer, but this version does not read it yet.")                                  \
    X(HelpBasedOn, "Basado en las guías de", "Based on the guides by")                                                \
    X(HelpTour, "Recorrido de la interfaz", "A tour of the interface")                                               \
    X(HelpTourAbout, "Siete pasos, de abrir un modelo a imprimir.", "Seven steps, from opening a model to printing.") \
    X(HelpStart, "Empezar", "Start")                                                                                  \
    X(HelpNothingFound, "No se encontró nada", "Nothing found")                                                       \
    X(MaterialCard, "Ficha del material: %s", "Material card: %s")                                                    \
    X(GuideOffer, "¿Te muestro cómo?", "Shall I show you how?")                                                      \
    X(GuideAccept, "Mostrame", "Show me")                                                                             \
    X(GuideLater, "Después", "Later")                                                                                 \
    X(GuideNever, "No, gracias", "No, thanks")                                                                        \
    X(GuideWaits, "Sigue solo cuando lo hagas, o con Siguiente.", "It moves on when you do it, or with Next.")         \
    X(GuideExit, "Salir", "Exit")                                                                                     \
    X(GuideContinue, "Seguir", "Continue")                                                    \
    X(MaterialChamber, "Cabina", "Chamber")                                                                           \
    X(MaterialFan, "Ventilador", "Fan")                                                                               \
    X(MaterialEnclosure, "Impresora cerrada", "Enclosed printer")                                                     \
    X(MaterialNozzle, "Boquilla", "Nozzle")                                                                           \
    X(MaterialRetraction, "Retracción", "Retraction")                                                                 \
    X(MaterialDirect, "directa %.1f mm", "direct %.1f mm")                                                            \
    X(MaterialBowden, "Bowden %.1f mm", "Bowden %.1f mm")                                                             \
    X(MaterialDrying, "Secado", "Drying")                                                       \
    X(HelpMaterial, "Material", "Material")                                                                           \
    X(HelpBack, "‹ Volver", "‹ Back")                                                                                 \
    X(HelpLastResort, "Probar solo después de las anteriores", "Try only after the ones above")                       \
    X(HelpApply, "Aplicar", "Apply")                                                                                  \
    X(HelpOpen, "Abrir", "Open")                                                                                      \
    X(HelpSee, "Ver", "See")                                                                                          \
    X(HelpOpenCalibration, "Abrir la calibración", "Open the calibration")                                            \
    X(HelpPrevention, "Para que no vuelva a pasar", "So it does not happen again")                                    \
    X(HelpNotes, "Notas", "Notes")                                                                                    \
    X(HelpLimitUsed, "Se usó el límite: %s", "The limit was used: %s")                                                \
    X(HelpTutorialSteps, "%d pasos, unos %d min", "%d steps, about %d min")                                           \
    X(HelpGuidesSoon, "Las guías de calibración llegan con la página de calibraciones.", "The calibration guides come with the calibrations page.")                                                                                  \
    X(Quit, "Salir", "Quit")                                                                                          \
    X(AnalysisTitle, "Análisis estructural de la pieza impresa", "Structural analysis of the printed part")          \
    X(AnalysisHint, "La pieza se divide en vóxeles que siguen las capas. Más vóxeles: más detalle, más memoria y más tiempo.", \
      "The part is divided into voxels that follow the layers. More voxels: more detail, more memory and more time.") \
    X(QualityNormal, "Calidad normal", "Normal quality")                                                              \
    X(QualityHigh, "Calidad alta", "High quality")                                                                    \
    X(Voxels, "Vóxeles de la pieza", "Voxels of the part")                                                            \
    X(GridVoxels, "Vóxeles de la grilla", "Voxels of the grid")                                                       \
    X(VoxelSize, "Tamaño del vóxel", "Voxel size")                                                                    \
    X(Memory, "Memoria", "Memory")                                                                                    \
    X(ApproxTime, "Tiempo aproximado", "Approximate time")                                                            \
    X(Estimating, "Calculando…", "Estimating…")                                                                       \
    X(OverLimit, "Más que el máximo del análisis (%s vóxeles): no correría a esta calidad",                            \
      "More than the analysis's maximum (%s voxels): it would not run at this quality")                               \
    X(AnalysisNeedsModel, "Abrí un modelo para ver sus vóxeles", "Open a model to see its voxels")                    \
    X(AnalysisCli, "El análisis completo, con apoyos y cargas, corre por ahora con «tisma-engine analyze» (docs/ANALYSIS.md).", \
      "The full analysis, with supports and loads, runs for now with \"tisma-engine analyze\" (docs/ANALYSIS.md).") \
    X(SecQuality, "Calidad", "Quality") \
    X(SecStrength, "Resistencia", "Strength") \
    X(SecSpeed, "Velocidad", "Speed") \
    X(SecSupport, "Soportes", "Support") \
    X(SecAdhesion, "Adhesión", "Adhesion") \
    X(SecOther, "Otros", "Others") \
    X(SecTemperature, "Temperaturas", "Temperatures") \
    X(SecCooling, "Enfriamiento", "Cooling") \
    X(SecFlow, "Flujo", "Flow") \
    X(SecRetraction, "Retracción", "Retraction") \
    X(SecNeeds, "Necesidades", "Requirements") \
    X(SecMachine, "Máquina", "Machine") \
    X(SecNozzle, "Boquilla y extrusor", "Nozzle and extruder") \
    X(SecLimits, "Límites", "Limits") \
    X(SecGcode, "G-code", "G-code") \
    X(SubLayerHeight, "Altura de capa", "Layer height") \
    X(SubVariableHeight, "Altura variable", "Variable height") \
    X(SubFineDetail, "Detalle fino", "Fine detail") \
    X(SubLineWidth, "Ancho de línea", "Line width") \
    X(SubSeam, "Costura", "Seam") \
    X(SubPrecision, "Precisión", "Precision") \
    X(SubSlopes, "Superficies inclinadas", "Sloping surfaces") \
    X(SubWalls, "Paredes", "Walls") \
    X(SubTopBottom, "Arriba y abajo", "Top and bottom") \
    X(SubInfill, "Relleno", "Infill") \
    X(SubInfillMore, "Relleno avanzado", "Advanced infill") \
    X(SubCombine, "Combinar relleno", "Combined infill") \
    X(SubWavy, "Relleno ondulado", "Wavy infill") \
    X(SubFirstLayerSpeed, "Primera capa", "First layer") \
    X(SubWallSpeed, "Paredes y superficies", "Walls and surfaces") \
    X(SubInfillSpeed, "Relleno y soporte", "Infill and support") \
    X(SubTravelSpeed, "Viajes", "Travel") \
    X(SubSpeedCap, "Topes", "Limits") \
    X(SubSupport, "Soporte", "Support") \
    X(SubSupportGap, "Separación", "Gaps") \
    X(SubSupportInterface, "Interfaz", "Interface") \
    X(SubSupportTree, "Árbol", "Tree") \
    X(SubArcs, "Arcos en voladizos", "Arc overhangs") \
    X(SupportWidth, "Ancho de las líneas", "Line width") \
    X(ArcsEnabled, "Voladizos con arcos", "Overhangs in arcs") \
    X(ArcsOverlap, "Cuánto pisa cada arco", "Arc overlap") \
    X(ArcsSpeed, "Velocidad de los arcos", "Arc speed") \
    X(ArcsMaxDistance, "Hasta qué distancia", "Up to") \
    X(SumArcs, "Arcos a %s mm/s", "Arcs at %s mm/s") \
    X(SumNoArcs, "Sin arcos", "No arcs") \
    X(SubSupportPattern, "Patrón", "Pattern") \
    X(SubSkirt, "Falda", "Skirt") \
    X(SubBrim, "Borde", "Brim") \
    X(SubSpecialModes, "Modos especiales", "Special modes") \
    X(SubFuzzy, "Piel difusa", "Fuzzy skin") \
    X(SubGcodeOut, "Salida G-code", "G-code output") \
    X(SubSlicing, "Corte", "Slicing") \
    X(SubNozzleTemp, "Boquilla", "Nozzle") \
    X(SubBedTemp, "Cama", "Bed") \
    X(SubChamber, "Cabina", "Chamber") \
    X(SubFan, "Ventilador", "Fan") \
    X(SubShortLayers, "Capas cortas", "Short layers") \
    X(SubFlow, "Flujo", "Flow") \
    X(SubPressure, "Presión", "Pressure") \
    X(SubMaxFlow, "Flujo máximo", "Maximum flow") \
    X(SubRetract, "Retracción", "Retraction") \
    X(SubRetractTravel, "Viajes", "Travel") \
    X(SubNeeds, "Necesidades", "Requirements") \
    X(SubBedVolume, "Cama y volumen", "Bed and volume") \
    X(SubEnclosure, "Cabina", "Enclosure") \
    X(SubStability, "Estabilidad", "Stability") \
    X(NozzleForce, "Empuje de la boquilla", "Nozzle push") \
    X(BedAdhesion, "Adherencia a la cama", "Bed hold") \
    X(MaxDeflection, "Flecha máxima", "Largest bend") \
    X(SumNozzleForce, "Empuje medido: %s N", "Push measured: %s N") \
    X(SumNotMeasured, "Sin medir", "Not measured") \
    X(SubNozzle, "Boquilla", "Nozzle") \
    X(SubExtruder, "Extrusor", "Extruder") \
    X(SubSpeedLimits, "Velocidades", "Speeds") \
    X(SubAccel, "Aceleraciones", "Accelerations") \
    X(SubMaxTemps, "Temperaturas máximas", "Maximum temperatures") \
    X(SubFirmware, "Firmware", "Firmware") \
    X(SubStartEnd, "Inicio y fin", "Start and end") \
    X(SubVariables, "Variables", "Variables") \
    X(LevelBasic, "Básico", "Basic") \
    X(LevelNormal, "Normal", "Normal") \
    X(LevelAdvanced, "Avanzado", "Advanced") \
    X(LevelCustom, "Personalizado", "Custom") \
    X(LevelBasicTip, "Lo que casi toda impresión necesita", "What almost every print needs") \
    X(LevelNormalTip, "Más ajustes para afinar", "More settings to fine-tune") \
    X(LevelAdvancedTip, "Todos los ajustes", "All the settings") \
    X(LevelCustomTip, "Los ajustes que elegís vos", "The settings you choose") \
    X(LevelTip, "Cuántos ajustes se ven (Ctrl+Alt+1 a 4). El G-code no cambia con el nivel.", "How many settings show (Ctrl+Alt+1 to 4). The G-code does not change with the level.") \
    X(ChangesCount, "%d cambios", "%d changes") \
    X(OneChangeCount, "1 cambio", "1 change") \
    X(MoreSettings, "%d ajustes más en Normal y Avanzado", "%d more settings in Normal and Advanced") \
    X(MoreSettingsAdvanced, "%d ajustes más en Avanzado", "%d more settings in Advanced") \
    X(ShowAll, "Mostrar todos", "Show all") \
    X(SeeAllTogether, "Ver todo junto", "See all together") \
    X(SeeBySection, "Ver por secciones", "See by section") \
    X(ResetCard, "Restablecer", "Reset") \
    X(ResetCardTip, "Vuelve los ajustes de este apartado a los del perfil guardado", "Takes this part's settings back to the saved profile") \
    X(HiddenChanged, "Cambiado; se ve aunque el nivel lo esconde", "Changed; shown although the level hides it") \
    X(ChooseSettings, "Elegir ajustes…", "Choose settings…") \
    X(Undo, "Deshacer", "Undo") \
    X(BackToPlate, "‹ Placa", "‹ Plate") \
    X(PrinterSettingsTitle, "Configuración de la impresora", "Printer settings") \
    X(PrinterSettingsTip, "Configuración de la impresora: máquina, boquilla, límites, G-code y calibración", "Printer settings: machine, nozzle, limits, G-code and calibration") \
    X(SecCalibration, "Calibración", "Calibration") \
    X(SecConnection, "Conexión", "Connection") \
    X(SecNotes, "Notas", "Notes") \
    X(SearchPrinter, "Buscar impresora", "Search printer") \
    X(MyPrinters, "Mis impresoras", "My printers") \
    X(OtherPrinters, "Otras", "Others") \
    X(AddPrinter, "Agregar impresora…", "Add printer…") \
    X(BedType, "Cama", "Bed") \
    X(HardenedSteel, "acero endurecido", "hardened steel") \
    X(OtherNozzle, "Otra…", "Other…") \
    X(OtherNozzleTitle, "Otra boquilla", "Another nozzle") \
    X(NozzleChanged, "Boquilla de %s", "%s nozzle") \
    X(NotCalibrated, "Sin calibrar", "Not calibrated") \
    X(CalibratedOn, "Calibrado el %s", "Calibrated on %s") \
    X(CalibratedOtherNozzle, "Calibrado con otra boquilla (%s)", "Calibrated with another nozzle (%s)") \
    X(CalibratedWith, "con %s", "with %s") \
    X(CalibratedCount, "%d de %d calibradas", "%d of %d calibrated") \
    X(CalibrateInOrder, "Calibrar en orden", "Calibrate in order") \
    X(CalibrateButton, "Calibrar", "Calibrate") \
    X(OpenInDevices, "Abrir en Dispositivos", "Open in Devices") \
    X(ConnectionNone, "Sin conectar. La conexión por USB está en la pestaña Impresora.", "Not connected. The USB connection is in the Printer tab.") \
    X(ConnectionNow, "Conectada por USB: %s", "Connected by USB: %s") \
    X(NotesHint, "Notas sobre esta impresora: arreglos, repuestos, lo que sirvió…", "Notes about this printer: fixes, spare parts, what worked…") \
    X(ChooseSettingsTitle, "Los ajustes que se ven en Personalizado", "The settings Custom shows") \
    X(StartFrom, "Partir de:", "Start from:") \
    X(AllSettings, "Todos", "All") \
    X(NoSettings, "Ninguno", "None") \
    X(Accept, "Aceptar", "Accept") \
    X(FilterSettings, "Filtrar ajustes", "Filter settings") \
    X(HideSetting, "Quitar de la vista", "Take out of the view") \
    X(AddSetting, "+ Agregar ajuste", "+ Add a setting") \
    X(SearchSettings, "Buscar un ajuste (Ctrl+F)", "Search a setting (Ctrl+F)") \
    X(SearchNoMatch, "Ningún ajuste coincide", "No setting matches") \
    X(ExportView, "Exportar vista…", "Export view…") \
    X(ImportView, "Importar vista…", "Import view…") \
    X(ExportViewTitle, "Exportar la vista de los ajustes", "Export the settings view") \
    X(ImportViewTitle, "Importar una vista de los ajustes", "Import a settings view") \
    X(ViewExported, "Vista guardada en %s", "View saved to %s") \
    X(ViewImported, "Vista cargada: %d ajustes", "View loaded: %d settings") \
    X(VaseMode, "Modo vaso (espiral)", "Vase mode (spiral)") \
    X(SequentialRow, "Objeto por objeto", "One object at a time") \
    X(SequentialTip, "Cada objeto se imprime entero antes del siguiente: si uno falla, los otros siguen. Necesita lugar para el cabezal entre los objetos", "Each object is printed whole before the next: if one fails, the others go on. The head needs room between the objects") \
    X(ClearanceRadius, "Radio libre del cabezal", "Head clearance radius") \
    X(ClearanceHeight, "Altura libre bajo el eje X", "Clearance under the X axis") \
    X(BetweenObjectsGcode, "G-code entre objetos", "G-code between objects") \
    X(VaseModeTip, "Una sola pared que sube en espiral, sin costura: para vasos, macetas y pantallas. Sin relleno ni techo; las capas de abajo son planas. No se puede con soportes.", "One wall rising in a spiral, without a seam: for vases, pots and lampshades. No infill nor top; the bottom layers are flat. Not with supports.") \
    X(VaseManyObjects, "En modo vaso se imprime un solo objeto por vez", "Vase mode prints one object at a time") \
    X(SumLayer, "%s mm · primera %s mm", "%s mm · first %s mm") \
    X(SumSameHeight, "Igual en toda la pieza", "The same all over the part") \
    X(SumHeightSteps, "%d tramos por Z", "%d steps by Z") \
    X(SumAdaptive, "Según la pendiente", "By the slope") \
    X(SumOff, "Apagado", "Off") \
    X(SumMm, "%s mm", "%s mm") \
    X(SumScarf, "%s · en bufanda", "%s · scarf") \
    X(SumNoCorrections, "Sin correcciones", "No corrections") \
    X(SumCorrections, "Con correcciones", "With corrections") \
    X(SumZaaOff, "Z anti-aliasing apagado", "Z anti-aliasing off") \
    X(SumZaaOn, "Z anti-aliasing, mínimo %s %%", "Z anti-aliasing, at least %s %%") \
    X(SumWalls, "%d paredes", "%d walls") \
    X(SumOneWall, "1 pared", "1 wall") \
    X(SumNoWalls, "Sin paredes", "No walls") \
    X(SumTopBottom, "%d arriba · %d abajo", "%d top · %d bottom") \
    X(SumInfill, "%s %% · %s", "%s %% · %s") \
    X(SumNoChanges, "Sin cambios", "No changes") \
    X(SumCombine, "Cada %d capas", "Every %d layers") \
    X(SumCombineAuto, "Lo más posible", "As much as fits") \
    X(SumNotCombined, "Cada capa la suya", "Each layer its own") \
    X(SumSpeed, "%s mm/s", "%s mm/s") \
    X(SumWallSpeed, "%s · externa %s", "%s · outer %s") \
    X(SumTwoSpeeds, "%s · %s mm/s", "%s · %s mm/s") \
    X(SumNoSupport, "Sin soportes", "No supports") \
    X(SumAutoSupport, "Automáticos, %s°", "Automatic, %s°") \
    X(SumPaintedSupport, "Solo lo pintado", "Painted only") \
    X(SumGaps, "Z %s mm · XY %s", "Z %s mm · XY %s") \
    X(SumLayersN, "%d capas", "%d layers") \
    X(SumSkirt, "%d vueltas a %s mm", "%d loops at %s mm") \
    X(SumNoSkirt, "Sin falda", "No skirt") \
    X(SumNoBrim, "Sin borde", "No brim") \
    X(SumNormalPrint, "Normal", "Normal") \
    X(SumResolution, "Resolución %s mm", "Resolution %s mm") \
    X(SumTemp, "%d °C", "%d °C") \
    X(SumNoChamber, "Sin cabina", "No chamber") \
    X(SumFan, "%s–%s %%", "%s–%s %%") \
    X(SumMinLayer, "Mínimo %s s", "At least %s s") \
    X(SumPercent, "%s %%", "%s %%") \
    X(SumNoPa, "Sin avance de presión", "No pressure advance") \
    X(SumPa, "Avance %s", "Advance %s") \
    X(SumNoCap, "Sin tope", "No limit") \
    X(SumCap, "%s mm³/s", "%s mm³/s") \
    X(SumRetract, "%s mm a %s mm/s", "%s mm at %s mm/s") \
    X(SumNoRetract, "Sin retracción", "No retraction") \
    X(SumNoHop, "Sin salto en Z", "No Z hop") \
    X(SumHop, "Salto de %s mm", "Z hop of %s mm") \
    X(SumNoNeeds, "Sin requisitos", "No requirements") \
    X(SumNeeds, "Con requisitos", "With requirements") \
    X(SumBed, "%s × %s × %s mm", "%s × %s × %s mm") \
    X(SumOpen, "Abierta", "Open") \
    X(SumClosed, "Cerrada", "Enclosed") \
    X(SumHardened, "%s mm · acero endurecido", "%s mm · hardened steel") \
    X(SumFeeds, "%s mm/s en X e Y", "%s mm/s in X and Y") \
    X(SumAccel, "%s mm/s²", "%s mm/s²") \
    X(SumMaxTemps, "%s °C · %s °C", "%s °C · %s °C") \
    X(SumLines, "%d líneas · %d líneas", "%d lines · %d lines") \
    X(SumVariables, "%d variables", "%d variables") \
    X(ScarfMode, "Costura en bufanda", "Scarf seam") \
    X(ScarfNowhere, "No", "No") \
    X(ScarfContours, "Contornos", "Contours") \
    X(ScarfEverywhere, "También agujeros", "Holes too") \
    X(ScarfLength, "Largo de la rampa", "Ramp length") \
    X(ScarfEntire, "Rampa en toda la vuelta", "Ramp all the way round") \
    X(ScarfStart, "Altura inicial", "Start height") \
    X(ScarfSegment, "Tramo máximo", "Longest piece") \
    X(ScarfSmooth, "Solo lazos lisos", "Smooth loops only") \
    X(ScarfInner, "También paredes internas", "Inner walls too") \
    X(DetectOverhangs, "Paredes en voladizo", "Overhang walls") \
    X(MinLineWidth, "Cordón más fino", "Thinnest bead") \
    X(MinDetail, "Detalle más chico", "Smallest detail") \
    X(ScarfTip, "La pared empieza en una rampa desde la capa de abajo y termina pasando sobre ella: casi no deja costura. Ver walls_extra.md.", "The wall starts on a ramp up from the layer below and ends over it: the seam hardly shows. See walls_extra.md.") \
    X(ScaleXRow, "Escala en X", "Scale in X") \
    X(ScaleYRow, "Escala en Y", "Scale in Y") \
    X(ScaleZRow, "Escala en Z", "Scale in Z") \
    X(XyOffset, "Corrección XY", "XY offset") \
    X(FillModeRow, "Modo de llenado", "Fill mode") \
    X(FillUnion, "Unión", "Union") \
    X(FillEvenOdd, "Par e impar", "Even-odd") \
    X(FillHoles, "Llenar agujeros", "Fill holes") \
    X(GapClosing, "Cerrar huecos de hasta", "Close gaps up to") \
    X(SimplifyTolerance, "Simplificar contornos", "Simplify outlines") \
    X(WallOverlap, "Solapamiento con paredes", "Overlap with walls") \
    X(CombineMaxHeight, "Alto máximo combinado", "Thickest combined") \
    X(SupportZGap, "Separación en Z", "Z gap") \
    X(SupportXyGap, "Separación en XY", "XY gap") \
    X(SupportSpacing, "Espaciado", "Spacing") \
    X(SupportLineAngle, "Ángulo de las líneas", "Line angle") \
    X(SupportInterface, "Capas de interfaz", "Interface layers") \
    X(SupportStyle, "Estilo", "Style") \
    X(SupportGrid, "Grilla", "Grid") \
    X(SupportOrganic, "Árbol (orgánico)", "Tree (organic)") \
    X(SupportStyleTip, "Árbol: ramas redondas que se juntan hacia la cama; menos marcas en la pieza y se quitan fácil", \
      "Tree: round branches that join on their way to the bed; fewer marks on the part, easy to remove") \
    X(TreeAngle, "Inclinación para esquivar", "Angle to avoid the part") \
    X(TreeAnglePreferred, "Inclinación para juntarse", "Angle to join") \
    X(TreeBranchDiameter, "Diámetro de las ramas", "Branch diameter") \
    X(TreeBranchAngle, "Engorde hacia abajo", "Widening downwards") \
    X(TreeDoubleWall, "Dos paredes desde", "Two walls from") \
    X(TreeDoubleWallTip, "Las ramas más gruesas que esto llevan dos paredes; 0: nunca", \
      "Branches thicker than this get two walls; 0: never") \
    X(TreeBranchDistance, "Distancia entre puntas", "Tip distance") \
    X(TreeTipDiameter, "Diámetro de las puntas", "Tip diameter") \
    X(TreeTopRate, "Densidad de puntas", "Tip density") \
    X(SumTree, "Ramas de %s mm · puntas de %s mm", "%s mm branches · %s mm tips") \
    X(SkirtLayers, "Capas de falda", "Skirt layers") \
    X(HeightChanges, "Cambios por altura", "Changes by height") \
    X(ChamberTemp, "Temperatura de cabina", "Chamber temperature") \
    X(FanBelowTime, "Capa con ventilador", "Fan below") \
    X(FanAlways, "Ventilador siempre", "Fan always on") \
    X(FanBridges, "Ventilador en puentes", "Fan on bridges") \
    X(FanOffLayers, "Capas sin ventilador", "Layers without fan") \
    X(MinSpeed, "Velocidad mínima", "Minimum speed") \
    X(PressureAdvance, "Avance de presión", "Pressure advance") \
    X(FilamentMaxFlow, "Flujo máximo", "Maximum flow") \
    X(SpeedBridges, "Puentes", "Bridges") \
    X(SpeedOverBridges, "Sobre los puentes", "Over bridges") \
    X(SpeedGapFill, "Relleno de huecos", "Gap fill") \
    X(SpeedFirstLayerInfill, "Relleno de la primera capa", "First layer infill") \
    X(SpeedTravelZ, "Viajes en Z", "Travel in Z") \
    X(InfillMaxSpeed, "Tope del relleno", "Infill speed cap") \
    X(InfillMaxCrossingSpeed, "Tope del relleno que se cruza", "Crossing infill speed cap") \
    X(BridgeFlowRatio, "Flujo de los puentes", "Bridge flow") \
    X(SubAccelerations, "Aceleraciones", "Accelerations") \
    X(SubOverhangSpeeds, "Voladizos", "Overhangs") \
    X(DynamicOverhangs, "Más lento según el apoyo", "Slower by support") \
    X(Overhang0, "Sin apoyo", "Unsupported") \
    X(Overhang1, "Apoyado un cuarto", "A quarter supported") \
    X(Overhang2, "Apoyado la mitad", "Half supported") \
    X(Overhang3, "Apoyado tres cuartos", "Three quarters supported") \
    X(SumBySupport, "Según el apoyo", "By support") \
    X(FlowSlopeUp, "Subida del caudal", "Flow ramp up") \
    X(FlowSlopeDown, "Bajada del caudal", "Flow ramp down") \
    X(SubGcodeTemplates, "Plantillas de G-code", "G-code templates") \
    X(ZOffset, "Desfase de Z", "Z offset") \
    X(VolumetricE, "E volumétrica", "Volumetric E") \
    X(VerboseComments, "Comentarios detallados", "Verbose comments") \
    X(GcodeArcs, "Arcos G2/G3", "G2/G3 arcs") \
    X(GcodeBinary, "G-code binario (.bgcode)", "Binary G-code (.bgcode)") \
    X(FlavorNoExtrusion, "Sin extrusión (solo movimientos)", "No extrusion (moves only)") \
    X(SubJerk, "Tirones y esquinas", "Jerk and corners") \
    X(SkirtMinFilament, "Filamento mínimo de la falda", "Minimum skirt filament") \
    X(AutoTemperatures, "Temperaturas automáticas", "Automatic temperatures") \
    X(SubOutputFile, "Archivo de salida", "Output file") \
    X(FilenameFormat, "Nombre del archivo", "File name") \
    X(PostProcessRow, "Scripts de posproceso", "Post-processing scripts") \
    X(FilamentStartGcode, "G-code de inicio del filamento", "Filament start G-code") \
    X(FilamentEndGcode, "G-code de fin del filamento", "Filament end G-code") \
    X(DraftShieldRow, "Escudo contra corrientes", "Draft shield") \
    X(ShieldDisabled, "No", "No") \
    X(ShieldLimited, "En las capas de la falda", "On the skirt's layers") \
    X(ShieldEnabled, "Hasta arriba", "Up to the top") \
    X(BrimTypeRow, "Dónde va el borde", "Where the brim goes") \
    X(BrimOuter, "Afuera", "Outside") \
    X(BrimInner, "En los agujeros", "In the holes") \
    X(BrimBoth, "Afuera y en los agujeros", "Outside and in the holes") \
    X(BrimNone, "Ninguno", "None") \
    X(BrimGap, "Separación del borde", "Brim gap") \
    X(SubSilent, "Modo silencioso", "Silent mode") \
    X(RetractAcceleration, "Aceleración al retraer", "Retraction acceleration") \
    X(MaxAccelX, "Aceleración máxima en X", "Maximum acceleration in X") \
    X(MaxAccelY, "Aceleración máxima en Y", "Maximum acceleration in Y") \
    X(MaxAccelZ, "Aceleración máxima en Z", "Maximum acceleration in Z") \
    X(MaxAccelE, "Aceleración máxima del extrusor", "Maximum extruder acceleration") \
    X(JerkX, "Tirón en X", "Jerk in X") \
    X(JerkY, "Tirón en Y", "Jerk in Y") \
    X(JerkZ, "Tirón en Z", "Jerk in Z") \
    X(JerkE, "Tirón del extrusor", "Extruder jerk") \
    X(JunctionDeviation, "Desviación de unión", "Junction deviation") \
    X(MinExtrudeFeed, "Avance mínimo al extruir", "Minimum extruding feed") \
    X(MinTravelFeed, "Avance mínimo al viajar", "Minimum travel feed") \
    X(LimitsUsageRow, "Límites de la máquina", "Machine limits") \
    X(ObjectLabelsRow, "Etiquetas de objetos", "Object labels") \
    X(LabelsNone, "No", "No") \
    X(LabelsOctoPrint, "Comentarios (OctoPrint)", "Comments (OctoPrint)") \
    X(LabelsFirmware, "Del firmware (cancelar un objeto)", "The firmware's (cancel one object)") \
    X(SubExtruders, "Extrusores", "Extruders") \
    X(SubMultiMaterial, "Varios materiales", "Several materials") \
    X(ExtrudersRow, "Cantidad de extrusores", "Number of extruders") \
    X(ExtruderOffsetRow, "Desfase de las boquillas", "Nozzle offsets") \
    X(ToolchangeLength, "Retracción al cambiar de herramienta", "Retraction at a tool change") \
    X(ToolchangeRestartExtra, "Extra al volver a la herramienta", "Extra when the tool comes back") \
    X(ToolchangeGcode, "G-code al cambiar de herramienta", "Tool change G-code") \
    X(WallsExtruder, "Extrusor de los perímetros", "Walls extruder") \
    X(InfillExtruder, "Extrusor del relleno", "Infill extruder") \
    X(SolidInfillExtruder, "Extrusor del relleno sólido", "Solid infill extruder") \
    X(SupportExtruder, "Extrusor del soporte", "Support extruder") \
    X(SupportInterfaceExtruder, "Extrusor de la interfaz del soporte", "Support interface extruder") \
    X(BedTemperatureFrom, "Temperatura de cama del extrusor", "Bed temperature from extruder") \
    X(OozePrevention, "Bajar la temperatura de los que esperan", "Lower the waiting extruders' temperature") \
    X(StandbyDelta, "Cuánto bajarla", "By how much") \
    X(InterfaceShells, "Capas sólidas entre materiales", "Solid layers between materials") \
    X(SumExtrudersN, "%d extrusores", "%d extruders") \
    X(SumOneExtruder, "Un extrusor", "One extruder") \
    X(SumObjectExtruder, "El de cada objeto", "Each object's own") \
    X(SumByKind, "Por tipo de extrusión", "By kind of extrusion") \
    X(SubWipeTower, "Torre de purga", "Wipe tower") \
    X(SubWipeTowerMore, "Más de la torre de purga", "More of the wipe tower") \
    X(WipeTowerRow, "Torre de purga", "Wipe tower") \
    X(WipeTowerX, "Posición en X", "X position") \
    X(WipeTowerY, "Posición en Y", "Y position") \
    X(WipeTowerRotation, "Giro", "Rotation") \
    X(WipeTowerWidth, "Ancho", "Width") \
    X(WipeTowerBrim, "Borde de la torre", "Tower brim") \
    X(WipeTowerBridging, "Distancia entre apoyos", "Distance between supports") \
    X(WipeTowerExtruder, "Extrusor de la torre", "Tower extruder") \
    X(WipeTowerSpacing, "Separación de las líneas", "Line spacing") \
    X(WipeTowerFlow, "Flujo de las líneas", "Line flow") \
    X(WipeTowerCone, "Ángulo del cono de la base", "Base cone angle") \
    X(WipeTowerNoSparse, "Sin torre en las capas sin cambio", "No tower on layers without a change") \
    X(PurgeVolumesRow, "Volúmenes de purga", "Purge volumes") \
    X(PurgeMultiplier, "Multiplicador de purga", "Purge multiplier") \
    X(MinPurgeOnTower, "Purga mínima en la torre", "Minimum purge on the tower") \
    X(SubMmu, "MMU", "MMU") \
    X(SyncSupportLayers, "Soporte en las capas de la pieza", "Support on the part's layers") \
    X(SubInterlocking, "Entrelazado entre materiales", "Interlocking between materials") \
    X(InterlockBeams, "Vigas entre materiales", "Beams between materials") \
    X(InterlockWidth, "Ancho de las vigas", "Beam width") \
    X(InterlockLayers, "Capas por dirección", "Layers per direction") \
    X(InterlockOrientation, "Dirección de las vigas", "Beam direction") \
    X(InterlockDepth, "Profundidad (vigas)", "Depth (beams)") \
    X(InterlockAvoidance, "Distancia al exterior (vigas)", "Distance to the outside (beams)") \
    X(PaintMaxRegionWidth, "Ancho máximo de una región pintada", "Widest painted region") \
    X(PaintInterlockingDepth, "Entrelazado de las regiones pintadas", "Painted regions' interlocking") \
    X(SubMultiNozzle, "Boquillas distintas y colores", "Different nozzles and colours") \
    X(FineWallsCoarseInfill, "Paredes finas, relleno grueso", "Fine walls, coarse infill") \
    X(ColorMixingRow, "Mezcla de colores", "Color mixing") \
    X(ColorMixingCost, "Con una sola boquilla, cada capa con mezcla es un cambio de filamento con su purga: la impresión usa bastante más filamento y tarda más.", "With a single nozzle, every mixed layer is a filament change with its purge: the print uses much more filament and takes longer.") \
    X(TurnOn, "Prender", "Turn on") \
    X(DifferentNozzles, "boquillas distintas", "different nozzles") \
    X(ExtruderNozzle, "   Extrusor %d", "   Extruder %d") \
    X(ToolMaterialTip, "Extrusor y color del objeto", "The object's extruder and colour") \
    X(ObjectExtruder, "Extrusor", "Extruder") \
    X(ExtruderItem, "Extrusor %d", "Extruder %d") \
    X(VirtualExtruder, "Extrusor virtual #%d", "Virtual extruder #%d") \
    X(WipeIntoInfill, "Purgar en su relleno", "Purge into its infill") \
    X(WipeIntoObject, "Purgar en el objeto", "Purge into the object") \
    X(ColorMixingDialog, "Extrusores virtuales - Mezcla de colores", "Virtual Extruders - Color Mixing") \
    X(AddBlend, "Agregar mezcla", "Add blend") \
    X(MixingIntro, "Un extrusor virtual mezcla filamentos reales capa por capa para crear colores nuevos. Hacé clic en una mezcla de abajo para agregarla al proyecto.", "A virtual extruder blends physical filaments layer by layer to create new colors. Click a blend below to add it to your project.") \
    X(TwoColorBlends, "Mezclas de dos colores", "Two-color blends") \
    X(MixingEditorHint, "Mové el deslizador para elegir cuántas capas lleva cada filamento en el patrón que se repite.", "Drag the slider to set how many layers each filament gets in the repeating pattern.") \
    X(AddThirdExtruder, "Agregar un tercer extrusor", "Add third extruder") \
    X(DisplayColor, "Color que se muestra", "Display color") \
    X(LayerSequencePreview, "Secuencia de capas", "Layer sequence preview") \
    X(MixingTemplateError, "La plantilla de cambio de herramienta no funciona con la mezcla", "The tool change template does not work with mixing") \
    X(OkButton, "Aceptar", "OK") \
    X(SubCurved, "Capas curvas", "Curved layers") \
    X(SubCurvedMore, "Capas curvas: transición y caudal", "Curved layers: transition and flow") \
    X(SubHead, "Cabezal", "Print head") \
    X(CurvedModeRow, "Capas curvas", "Curved layers") \
    X(CurvedWave, "Onda", "Wave") \
    X(CurvedCone, "Cono", "Cone") \
    X(CurvedOverhangs, "Voladizos", "Overhangs") \
    X(CurvedStress, "Tensiones", "Stress") \
    X(CurvedSurface, "Caras superiores", "Top faces") \
    X(CurvedAutomatic, "Automático", "Automatic") \
    X(CurvedPatternRow, "Forma de la onda", "Wave shape") \
    X(CurvedEgg, "Huevera", "Egg crate") \
    X(CurvedRidges, "Crestas", "Ridges") \
    X(CurvedTwisted, "Crestas torcidas", "Twisted ridges") \
    X(CurvedFitHead, "Ajustar a la cabeza", "Fit to the head") \
    X(CurvedAmplitude, "Amplitud", "Amplitude") \
    X(CurvedWavelength, "Largo de onda", "Wavelength") \
    X(CurvedAngle, "Dirección de las crestas", "Ridge direction") \
    X(CurvedTwist, "Giro por mm", "Twist per mm") \
    X(CurvedConeAngle, "Pendiente del cono", "Cone slope") \
    X(CurvedFlatBelow, "Planas por debajo de", "Flat below") \
    X(CurvedRampHeight, "Alto de la transición", "Transition height") \
    X(CurvedFlatTop, "Planas arriba de todo", "Flat at the top") \
    X(CurvedSegment, "Largo de los tramos", "Piece length") \
    X(CurvedMaxSlope, "Pendiente máxima", "Steepest slope") \
    X(CurvedFlowPolicy, "Caudal", "Flow") \
    X(CurvedFlowKeep, "Mantener", "Keep") \
    X(CurvedFlowUniform, "Parejo", "Uniform") \
    X(CurvedFlowOff, "Sin tocar", "Leave as is") \
    X(CurvedUniformFlow, "Caudal parejo", "Uniform flow") \
    X(CurvedOverhangThreshold, "Voladizo sin capas curvas", "Overhang without curved layers") \
    X(HeadFreeAngleX, "Ángulo libre a lo largo de X", "Free angle along X") \
    X(HeadFreeAngleY, "Ángulo libre a lo largo de Y", "Free angle along Y") \
    X(HeadClearanceHeight, "Altura libre", "Clearance height") \
    X(HeadClearanceRadius, "Radio libre", "Clearance radius") \
    X(HeadProfile, "Perfil de la cabeza", "Head profile") \
    X(SecResin, "Resina", "Resin") \
    X(SubResinLayers, "Resina: capas", "Resin: layers") \
    X(SubResinDisplay, "Pantalla de resina", "Resin display") \
    X(SubResinExposure, "Exposición", "Exposure") \
    X(SubResinMoves, "Subida y esperas", "Lift and waits") \
    X(SubResinCorrections, "Correcciones de resina", "Resin corrections") \
    X(SubResinMaterial, "Datos de la resina", "Resin data") \
    X(ResinLayerHeight, "Altura de capa", "Layer height") \
    X(DisplayPixelsX, "Píxeles en X", "Pixels in X") \
    X(DisplayPixelsY, "Píxeles en Y", "Pixels in Y") \
    X(DisplayWidth, "Ancho de la pantalla", "Display width") \
    X(DisplayHeight, "Alto de la pantalla", "Display height") \
    X(DisplayMirrorX, "Espejar en X", "Mirror in X") \
    X(DisplayMirrorY, "Espejar en Y", "Mirror in Y") \
    X(ResinPrinterName, "Modelo de la impresora en el archivo", "Printer model in the file") \
    X(ResinProfileName, "Perfil de resina en el archivo", "Resin profile in the file") \
    X(ResinExposure, "Exposición", "Exposure") \
    X(ResinBottomExposure, "Exposición de fondo", "Bottom exposure") \
    X(ResinBottomLayers, "Capas de fondo", "Bottom layers") \
    X(ResinLightPwm, "Potencia de la luz", "Light power") \
    X(ResinBottomLightPwm, "Potencia de la luz de fondo", "Bottom light power") \
    X(ResinLiftDistance, "Subida", "Lift distance") \
    X(ResinLiftSpeed, "Velocidad de subida", "Lift speed") \
    X(ResinBottomLiftDistance, "Subida de fondo", "Bottom lift distance") \
    X(ResinBottomLiftSpeed, "Velocidad de subida de fondo", "Bottom lift speed") \
    X(ResinRetractSpeed, "Velocidad de bajada", "Retract speed") \
    X(ResinWaitBeforeExposure, "Espera antes de exponer", "Wait before exposure") \
    X(ResinWaitAfterCure, "Espera después de exponer", "Wait after curing") \
    X(ResinWaitAfterLift, "Espera arriba", "Wait after lifting") \
    X(ResinElephantFoot, "Pie de elefante", "Elephant foot") \
    X(ResinScaleX, "Escala en X", "Scale in X") \
    X(ResinScaleY, "Escala en Y", "Scale in Y") \
    X(ResinScaleZ, "Escala en Z", "Scale in Z") \
    X(ResinAbsoluteCorrection, "Corrección absoluta", "Absolute correction") \
    X(ResinZCorrection, "Corrección de Z", "Z correction") \
    X(ResinGamma, "Gamma del borde", "Edge gamma") \
    X(ResinAntialiasing, "Suavizado en el archivo", "Anti-aliasing in the file") \
    X(ResinDensity, "Densidad", "Density") \
    X(ResinBottleCost, "Costo del frasco", "Bottle cost") \
    X(ResinBottleVolume, "Volumen del frasco", "Bottle volume") \
    X(ResinBottleWeight, "Peso del frasco", "Bottle weight") \
    X(SumResinExposure, "%s s · fondo %s s", "%s s · bottom %s s") \
    X(SumPixels, "%d × %d px", "%d × %d px") \
    X(SumLift, "%s mm a %s mm/s", "%s mm at %s mm/s") \
    X(SumResinDensity, "%s g/ml", "%s g/ml") \
    X(SubResinSupports, "Resina: soportes", "Resin: supports") \
    X(Technology, "Tipo de impresora", "Printer type") \
    X(TechnologyFdm, "Filamento (FDM)", "Filament (FDM)") \
    X(TechnologyMsla, "Resina (MSLA)", "Resin (MSLA)") \
    X(SubBelt, "Impresora de cinta", "Belt printer") \
    X(BeltEnabled, "Impresora de cinta", "Belt printer") \
    X(BeltEnabledTip, "El pórtico está inclinado sobre una cinta que se lleva la pieza: se lamina en planos paralelos al pórtico y el G-code va en los ejes de la máquina. Sin soportes, balsa, borde ni falda.", "The gantry leans over a belt that carries the part away: the layers are planes along the gantry and the G-code is on the machine's axes. No supports, raft, brim or skirt.") \
    X(BeltAngle, "Ángulo del pórtico", "Gantry angle") \
    X(BeltIsZ, "La cinta es el eje Z", "The belt is the Z axis") \
    X(SumDegrees, "%s°", "%s°") \
    X(ResinLayersN, "%d capas", "%d layers") \
    X(ExportGoo, "Exportar GOO", "Export GOO") \
    X(ResinUsed, "Resina", "Resin") \
    X(ResinCost, "Costo", "Cost") \
    X(ResinSliced, "%d capas · %.1f ml · %.1f g", "%d layers · %.1f ml · %.1f g") \
    X(SubResinPad, "Resina: base", "Resin: pad") \
    X(SubResinHollow, "Resina: vaciado", "Resin: hollowing") \
    X(ResinSupports, "Soportes", "Supports") \
    X(ResinTipDiameter, "Diámetro de la punta", "Tip diameter") \
    X(ResinTipPenetration, "Penetración de la punta", "Tip penetration") \
    X(ResinPillarDiameter, "Diámetro de los pilares", "Pillar diameter") \
    X(ResinElevation, "Elevación", "Elevation") \
    X(ResinCriticalAngle, "Ángulo de las uniones", "Junction angle") \
    X(ResinSupportDensity, "Densidad de los puntos", "Point density") \
    X(ResinPad, "Base", "Pad") \
    X(ResinPadThickness, "Espesor de la base", "Pad thickness") \
    X(ResinPadMargin, "Margen de la base", "Pad margin") \
    X(ResinPadWallSlope, "Inclinación del borde", "Pad wall slope") \
    X(ResinPadAroundObject, "Base alrededor de la pieza", "Pad around the object") \
    X(ResinHollow, "Vaciar la pieza", "Hollow the part") \
    X(ResinHollowWall, "Espesor de la pared", "Wall thickness") \
    X(ResinHollowClosing, "Redondeo del hueco", "Closing distance") \
    X(ResinHollowInfill, "Relleno interior", "Inner infill") \
    X(ResinHollowInfillCell, "Celda del relleno", "Infill cell") \
    X(SubMmuLoad, "Carga y descarga en la MMU", "MMU load and unload") \
    X(SubMmuTimes, "Tiempos del cambio", "Change times") \
    X(CoolingTubeLength, "Largo del tubo de enfriamiento", "Cooling tube length") \
    X(CoolingTubePosition, "Posición del tubo de enfriamiento", "Cooling tube position") \
    X(ParkingPosition, "Posición de estacionamiento", "Parking position") \
    X(ExtraLoadingMove, "Corrección de la carga", "Loading correction") \
    X(HighCurrentOnSwap, "Más corriente durante el cambio", "More current during the swap") \
    X(LoadingSpeed, "Velocidad de carga", "Loading speed") \
    X(LoadingSpeedStart, "Velocidad al empezar a cargar", "Loading start speed") \
    X(UnloadingSpeed, "Velocidad de descarga", "Unloading speed") \
    X(UnloadingSpeedStart, "Velocidad al empezar a descargar", "Unloading start speed") \
    X(CoolingMoves, "Movimientos de enfriamiento", "Cooling moves") \
    X(CoolingInitialSpeed, "Velocidad del primero", "First move's speed") \
    X(CoolingFinalSpeed, "Velocidad del último", "Last move's speed") \
    X(RammingRow, "Compresión antes de descargar", "Ramming before unloading") \
    X(ToolchangeDelay, "Espera después de descargar", "Wait after unloading") \
    X(LoadTime, "Tiempo de carga", "Load time") \
    X(UnloadTime, "Tiempo de descarga", "Unload time") \
    X(SingleExtruderMmu, "Un extrusor con varios filamentos (MMU)", "One extruder, several filaments (MMU)") \
    X(LimitsTimeOnly, "Solo para el tiempo", "For the time only") \
    X(LimitsEmit, "Escribirlos en el G-code", "Write them in the G-code") \
    X(LimitsIgnore, "No usarlos", "Do not use them") \
    X(GcodeProgress, "Avance de la impresión (M73)", "Print progress (M73)") \
    X(SilentModeRow, "Tiene modo silencioso", "Has a silent mode") \
    X(SilentLimitsRow, "Límites del modo silencioso", "Silent mode limits") \
    X(SumJunction, "Desviación %s mm", "Deviation %s mm") \
    X(SumJerk, "Tirón %s mm/s", "Jerk %s mm/s") \
    X(SumSilent, "%d límites propios", "%d limits of its own") \
    X(ChamberMinTemp, "Esperar la cámara a", "Wait for the chamber at") \
    X(SubPrice, "Peso y costo", "Weight and cost") \
    X(FilamentDensity, "Densidad", "Density") \
    X(FilamentCost, "Costo por kg", "Cost per kg") \
    X(SpoolWeight, "Peso del carrete vacío", "Empty spool weight") \
    X(SubShrinkage, "Contracción", "Shrinkage") \
    X(ShrinkageXY, "Contracción en XY", "Shrinkage in XY") \
    X(ShrinkageZ, "Contracción en Z", "Shrinkage in Z") \
    X(SumNoShrinkage, "Sin compensar", "Not compensated") \
    X(SumShrinkage, "XY %s %% · Z %s %%", "XY %s %% · Z %s %%") \
    X(SumDensity, "%s g/cm³", "%s g/cm³") \
    X(ColorChangeGcode, "Cambio de color", "Colour change") \
    X(PauseGcode, "Pausa", "Pause") \
    X(BeforeLayerGcode, "Antes de cada capa", "Before each layer") \
    X(AfterLayerGcode, "Después de cada capa", "After each layer") \
    X(SubRaft, "Balsa", "Raft") \
    X(RaftLayers, "Capas de la balsa", "Raft layers") \
    X(RaftGap, "Separación con la pieza", "Gap to the part") \
    X(RaftExpansion, "Cuánto se agranda", "Expansion") \
    X(RaftFirstDensity, "Densidad de la primera capa", "First layer density") \
    X(RaftFirstExpansion, "Agrandar la primera capa", "First layer expansion") \
    X(SpeedOverRaft, "Primera capa sobre la balsa", "First layer over the raft") \
    X(AccelOverRaft, "Primera capa sobre la balsa", "First layer over the raft") \
    X(SumRaft, "%d capas", "%d layers") \
    X(SubIroning, "Planchado", "Ironing") \
    X(IroningEnabled, "Planchar", "Iron") \
    X(IroningSurfacesRow, "Qué superficies", "Which surfaces") \
    X(IroningTop, "Todas las de arriba", "Every top surface") \
    X(IroningTopmost, "Solo la más alta", "Only the topmost") \
    X(IroningSolid, "Todas las capas sólidas", "Every solid layer") \
    X(IroningFlow, "Flujo", "Flow") \
    X(IroningSpacing, "Separación", "Spacing") \
    X(IroningSpeed, "Velocidad", "Speed") \
    X(SumIroning, "Planchado a %s mm/s", "Ironing at %s mm/s") \
    X(WallGeneratorRow, "Ancho de los cordones", "Bead width") \
    X(WallVariable, "Variable", "Variable") \
    X(WallFixed, "Fijo", "Fixed") \
    X(WallGapFill, "Relleno de huecos", "Gap fill") \
    X(SubAccelTravel, "Aceleración de la primera capa y los viajes", "First layer and travel acceleration") \
    X(AccelDefault, "Por defecto", "Default") \
    X(AccelWalls, "Paredes internas", "Inner walls") \
    X(AccelOuterWall, "Pared externa", "Outer wall") \
    X(AccelInfill, "Relleno", "Infill") \
    X(AccelSolid, "Relleno sólido", "Solid infill") \
    X(AccelTop, "Superficie de arriba", "Top surface") \
    X(AccelBridges, "Puentes", "Bridges") \
    X(AccelFirstLayer, "Primera capa", "First layer") \
    X(AccelTravel, "Viajes", "Travels") \
    X(AccelShortTravel, "Viajes cortos", "Short travels") \
    X(RetractMinTravel, "Viaje mínimo para retraer", "Shortest travel to retract") \
    X(UnretractSpeed, "Velocidad de la vuelta", "Unretract speed") \
    X(RestartExtra, "Extra al volver", "Extra on restart") \
    X(RetractLayerChange, "Retraer al cambiar de capa", "Retract on layer change") \
    X(WipeRow, "Barrer al retraer", "Wipe while retracting") \
    X(BeforeWipe, "Retracción antes del barrido", "Retraction before the wipe") \
    X(FirmwareRetraction, "Retracción del firmware (G10/G11)", "Firmware retraction (G10/G11)") \
    X(LiftAbove, "Saltar solo por encima de", "Lift only above") \
    X(LiftBelow, "Saltar solo por debajo de", "Lift only below") \
    X(RampingLift, "Salto en rampa", "Ramping lift") \
    X(TravelSlope, "Pendiente de la rampa", "Ramp slope") \
    X(MaxLift, "Altura máxima de la rampa", "Ramp maximum height") \
    X(LiftBeforeObstacle, "Rampa más empinada ante obstáculos", "Steeper ramp before obstacles") \
    X(AvoidCrossingWalls, "Viajes sin cruzar paredes", "Avoid crossing walls") \
    X(MaxDetour, "Desvío máximo", "Maximum detour") \
    X(WallsClockwise, "Lazos en sentido horario", "Clockwise loops") \
    X(SubWallsMore, "Más opciones de paredes", "More wall options") \
    X(StaggeredSeams, "Costuras internas escalonadas", "Staggered inner seams") \
    X(TopOneWallRow, "Un solo perímetro arriba", "One wall on top") \
    X(TopOneWallNone, "No", "No") \
    X(TopOneWallTop, "En toda superficie de arriba", "On every top surface") \
    X(TopOneWallTopmost, "Solo en la de más arriba", "Only on the topmost") \
    X(FirstLayerOneWall, "Un solo perímetro en la primera capa", "One wall on the first layer") \
    X(ExtraIfNeeded, "Perímetros extra si hacen falta", "Extra walls if needed") \
    X(AvoidCurledOverhangs, "Esquivar voladizos levantados", "Avoid curled overhangs") \
    X(ExtraOnOverhangs, "Perímetros extra en voladizos", "Extra perimeters on overhangs") \
    X(SumWallsMore, "%d de 6 activas", "%d of 6 on") \
    X(SubMaterialRetraction, "Valores propios del material", "The material's own values") \
    X(SumPrinterValues, "Los de la impresora", "The printer's") \
    X(SumMaterialValues, "%d del material", "%d of the material's") \
    X(FilamentRetractionRow, "Retracción y viajes del material", "The material's retraction and travel") \
    X(Abrasive, "Abrasivo", "Abrasive") \
    X(MaxChamberTemp, "Cabina máxima", "Chamber maximum") \
    X(MachineMaxFlow, "Flujo máximo del cabezal", "Hot end maximum flow") \
    X(MaxFeedX, "Velocidad máxima en X", "Maximum speed in X") \
    X(MaxFeedY, "Velocidad máxima en Y", "Maximum speed in Y") \
    X(MaxFeedZ, "Velocidad máxima en Z", "Maximum speed in Z") \
    X(MaxFeedE, "Velocidad máxima del extrusor", "Maximum extruder speed") \
    X(CornerSpeed, "Velocidad en esquinas", "Corner speed") \
    X(AccelerationRow, "Aceleración", "Acceleration") \
    X(TravelAcceleration, "Aceleración en viajes", "Travel acceleration") \
    X(MaxNozzleTemp, "Boquilla máxima", "Nozzle maximum") \
    X(MaxBedTemp, "Cama máxima", "Bed maximum") \
    X(VariablesRow, "Variables del G-code", "G-code variables") \
    X(ListCount, "%d en la lista", "%d in the list") \
    X(ListEditTip, "Se editan en el archivo del perfil (docs/CONFIG.md)", "Edited in the profile's file (docs/CONFIG.md)")

namespace tisma::ui {

enum class T : int {
#define TISMA_UI_TEXT_ID(id, es, en) id,
    TISMA_UI_TEXT(TISMA_UI_TEXT_ID)
#undef TISMA_UI_TEXT_ID
        Count
};

enum class Language { Spanish, English };

void set_language(Language language);
Language language();

// The text in the current language.
const char* tr(T id);

} // namespace tisma::ui
