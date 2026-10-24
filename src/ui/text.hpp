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
    X(OpenTip, "Abrir un modelo STL, un G-code o ajustes (Ctrl+O)", "Open an STL model, a G-code file or settings (Ctrl+O)") \
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
    X(Adhesion, "Adherencia", "Adhesion")                                                                             \
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
    X(SettingsFile, "Ajustes", "Settings")                                                                            \
    X(ViewIso, "Perspectiva", "Perspective")                                                                          \
    X(ViewTop, "Arriba", "Top")                                                                                       \
    X(ViewFront, "Frente", "Front")                                                                                   \
    X(ViewBack, "Atrás", "Back")                                                                                      \
    X(ViewLeft, "Izquierda", "Left")                                                                                  \
    X(ViewRight, "Derecha", "Right")                                                                                  \
    X(ZoomIn, "Acercar", "Zoom in")                                                                                   \
    X(ZoomOut, "Alejar", "Zoom out")                                                                                  \
    X(ZoomFit, "Encuadrar", "Frame all")                                                                              \
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
    X(ShowModel, "Mostrar el modelo", "Show the model")                                                               \
    X(EstimatedTime, "Tiempo estimado", "Estimated time")                                                             \
    X(FilamentUsed, "Filamento", "Filament")                                                                          \
    X(Layers, "Capas", "Layers")                                                                                      \
    X(Lines, "Líneas", "Lines")                                                                                       \
    X(Triangles, "triángulos", "triangles")                                                                           \
    X(Size, "Tamaño", "Size")                                                                                         \
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
