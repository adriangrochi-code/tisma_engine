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
    X(NonplanarInfill, "Relleno ondulado", "Wavy infill")                                                             \
    X(NonplanarInfillTip, "El relleno sube y baja en Z mientras avanza, igual en todas las capas, así cada capa se anida en la de abajo y la unión queda inclinada: más resistencia en Z. Solo con patrones que se repiten capa a capa (grilla, triángulos, estrellas, panal, alineado, concéntrico, curvas). Ver docs/NONPLANAR_INFILL.md.", "The infill goes up and down in Z as it runs, the same in every layer, so each layer nests on the one below and the joint leans: stronger in Z. Only with patterns that repeat layer to layer (grid, triangles, stars, honeycomb, aligned, concentric, curves). See docs/NONPLANAR_INFILL.md.") \
    X(NonplanarAmplitude, "Altura de la onda", "Wave height")                                                         \
    X(NonplanarWavelength, "Largo de la onda", "Wavelength")                                                          \
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
    X(AboutLibraries, "Usa GLFW, Dear ImGui, nanosvg, oneTBB, Clipper2, nlohmann/json y volk (THIRD_PARTY.md); la fuente es Roboto (Apache-2.0).", \
      "Uses GLFW, Dear ImGui, nanosvg, oneTBB, Clipper2, nlohmann/json and volk (THIRD_PARTY.md); the font is Roboto (Apache-2.0).") \
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
      "The full analysis, with supports and loads, runs for now with \"tisma-engine analyze\" (docs/ANALYSIS.md).")

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
