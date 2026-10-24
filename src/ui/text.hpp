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
    X(FanMin, "Ventilador normal", "Fan, normal layers")                                                              \
    X(FanMax, "Ventilador en capas lentas", "Fan, slowed layers")                                                     \
    X(MinLayerTime, "Tiempo mínimo de capa", "Minimum layer time")                                                    \
    X(LayerHeight, "Altura de capa", "Layer height")                                                                  \
    X(FirstLayerHeight, "Altura de la primera capa", "First layer height")                                            \
    X(Walls, "Paredes", "Walls")                                                                                      \
    X(LineWidth, "Ancho de línea", "Line width")                                                                      \
    X(Seam, "Costura", "Seam")                                                                                        \
    X(SeamAligned, "Alineada", "Aligned")                                                                             \
    X(SeamNearest, "Más cercana", "Nearest")                                                                          \
    X(SeamRandom, "Aleatoria", "Random")                                                                              \
    X(SeamRear, "Atrás", "Rear")                                                                                      \
    X(TopLayers, "Capas superiores", "Top layers")                                                                    \
    X(BottomLayers, "Capas inferiores", "Bottom layers")                                                              \
    X(TopPattern, "Superficie superior", "Top surface")                                                               \
    X(Monotonic, "Monotónica", "Monotonic")                                                                           \
    X(Rectilinear, "Rectilínea", "Rectilinear")                                                                       \
    X(Grid, "Grilla", "Grid")                                                                                         \
    X(Gyroid, "Giroide", "Gyroid")                                                                                    \
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
    X(TowerStart, "Desde", "From")                                                                                    \
    X(TowerEnd, "Hasta", "To")                                                                                        \
    X(TowerStep, "Paso", "Step")                                                                                      \
    X(TowerSegment, "Alto de cada segmento", "Segment height")                                                        \
    X(SegmentAt, "Segmento: %s", "Segment: %s")                                                                       \
    X(MoreCalibrations, "Próximas calibraciones", "Coming calibrations")                                              \
    X(MoreCalibrationsList, "Avance de presión · resonancias (input shaping) · primera capa · puentes y voladizos · tolerancias de encastre · ancho de línea variable · adherencia entre capas con el análisis · laminado no planar: altura y ángulo (a definir). Ver docs/CALIBRATION.md.", \
      "Pressure advance · resonances (input shaping) · first layer · bridges and overhangs · fit tolerances · variable line width · layer adhesion with the analysis · non-planar slicing: height and angle (to be defined). See docs/CALIBRATION.md.") \
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
