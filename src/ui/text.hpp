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
    X(TabDesign, "Diseño", "Design")                                                                                  \
    X(TabStructural, "Estructural", "Structural")                                                                     \
    X(TabAerodynamics, "Aerodinámica", "Aerodynamics") \
    X(AeroTitle, "Análisis aerodinámico", "Aerodynamic analysis") \
    X(AeroSubtitle, "El aire alrededor de la pieza tal como se imprime: para comparar diseños y orientaciones", "The air around the part as it prints: to compare designs and orientations") \
    X(AeroDirection, "Dirección del aire", "Air direction") \
    X(AeroDirectionView, "La de la vista", "The view's") \
    X(AeroSpeed, "Velocidad (m/s)", "Speed (m/s)") \
    X(AeroQuality, "Calidad", "Quality") \
    X(AeroQualityFriction, "Solo rozamiento", "Friction only") \
    X(AeroQualityFast, "Rápida (16 celdas)", "Fast (16 cells)") \
    X(AeroQualityNormal, "Media (24 celdas)", "Medium (24 cells)") \
    X(AeroQualityHigh, "Alta (32 celdas)", "High (32 cells)") \
    X(AeroQualityExtreme, "Extrema (48 celdas; lenta)", "Extreme (48 cells; slow)") \
    X(AeroLayer, "Altura de capa (mm, da la rugosidad)", "Layer height (mm, gives the roughness)") \
    X(AeroCloud, "Calcular en la nube", "Compute in the cloud") \
    X(AeroDrag, "Arrastre: %.4g N", "Drag: %.4g N") \
    X(AeroRange, "Rango probable del Cd: %.3f a %.3f (±%.0f %%: el método y la resolución)", "Likely Cd range: %.3f to %.3f (±%.0f %%: the method and the resolution)") \
    X(AeroRangeFriction, "Rango probable del Cd: %.3f a %.3f (±%.0f %% de las correlaciones; sin la presión, que pide simular)", "Likely Cd range: %.3f to %.3f (±%.0f %% of the correlations; without the pressure, which needs a simulation)") \
    X(AeroCd, "Cd %.3f (presión %.3f · rozamiento %.4f)", "Cd %.3f (pressure %.3f · friction %.4f)") \
    X(AeroCdFriction, "Cd de rozamiento %.4f (sin simular la presión)", "Friction Cd %.4f (pressure not simulated)") \
    X(AeroSide, "Fuerza transversal: %.4g N", "Side force: %.4g N") \
    X(AeroArea, "Área frontal: %.0f mm²", "Frontal area: %.0f mm²") \
    X(AeroReynolds, "Reynolds %.3g (simulado %.3g)", "Reynolds %.3g (simulated %.3g)") \
    X(AeroRoughness, "Rugosidad media %.1f µm · %.0f %% del rozamiento", "Mean roughness %.1f µm · %.0f %% of the friction") \
    X(AeroMap, "Mapa", "Map") \
    X(AeroLines, "Líneas de corriente (color: presión)", "Streamlines (colour: pressure)") \
    X(AeroMapCp, "Coeficiente de presión", "Pressure coefficient") \
    X(AeroMapRa, "Rugosidad", "Roughness") \
    X(AeroLegendCp, "−1,5 succión · 1 estancamiento", "−1.5 suction · 1 stagnation") \
    X(AeroLegendRa, "Azul liso · rojo escalonado (µm)", "Blue smooth · red stepped (µm)") \
    X(AeroAnalyzing, "Simulando el aire… %d %%", "Simulating the air… %d %%") \
    X(StructTitle, "Análisis estructural", "Structural analysis")                                                     \
    X(StructSubtitle, "Dónde está fija la pieza, dónde la empujan y con qué material", "Where the part is held, where it is pushed and in what material") \
    X(StructPart, "Pieza", "Part")                                                                                    \
    X(StructNoParts, "Agregá una pieza a la placa para analizarla.", "Add a part to the plate to analyze it.")       \
    X(StructValues, "Material y cómo se imprime", "Material and how it prints")                                       \
    X(StructMaterial, "Material", "Material")                                                                         \
    X(StructChooseMaterial, "Elegí un material", "Choose a material")                                                 \
    X(StructExtrusionTemp, "Temperatura de extrusión", "Extrusion temperature")                                       \
    X(StructBedTemp, "Temperatura de la cama", "Bed temperature")                                                     \
    X(StructLayerHeight, "Altura de capa", "Layer height")                                                            \
    X(StructFirstLayer, "Altura de la primera capa", "First layer height")                                            \
    X(StructLineWidth, "Ancho de línea", "Line width")                                                                \
    X(StructWorkingTemp, "Temperatura de trabajo", "Working temperature")                                             \
    X(StructWorkingTempTip, "A la que va a estar la pieza en uso. No está en ningún perfil.", "The temperature the part will be at in use. No profile has it.") \
    X(StructFromPrintProfile, "del perfil de impresión", "from the print profile")                                    \
    X(StructFromFilamentProfile, "del perfil de filamento", "from the filament profile")                              \
    X(StructWallsInfill, "Paredes: %d · capas sólidas arriba %d y abajo %d · relleno %.0f %% (%s)", "Walls: %d · solid layers %d top and %d bottom · infill %.0f %% (%s)") \
    X(StructByHand, "cambiado a mano: no cambia el perfil ni el G-code", "changed by hand: the profile and the G-code stay") \
    X(StructBackToProfile, "Volver al del perfil", "Back to the profile's")                                           \
    X(StructConditions, "Condiciones", "Conditions")                                                                  \
    X(StructToolRotate, "Rotar", "Rotate")                                                                            \
    X(StructToolFixedPoint, "Punto fijo", "Fixed point")                                                              \
    X(StructToolFixedZone, "Zona fija", "Fixed zone")                                                                 \
    X(StructToolForcePoint, "Punto de fuerza", "Force point")                                                         \
    X(StructToolForceZone, "Zona de fuerza", "Force zone")                                                            \
    X(StructToolLimit, "Límite de desplazamiento", "Displacement limit")                                              \
    X(StructPointHint, "Clic en la superficie de la pieza. Esc vuelve a Rotar.", "Click on the part's surface. Esc goes back to Rotate.") \
    X(StructZoneHint, "Clic en una cara: se toma entera. Con Mayús se suman caras. Esc vuelve a Rotar.", "Click on a face: it is taken whole. Shift adds faces. Esc goes back to Rotate.") \
    X(StructNoConditions, "Sin condiciones todavía.", "No conditions yet.")                                           \
    X(StructAnalyze, "Analizar", "Analyze")                                                                           \
    X(StructNeedMaterial, "Elegí un material.", "Choose a material.")                                                 \
    X(StructNeedFixed, "Agregá un punto o una zona fija.", "Add a fixed point or zone.")                              \
    X(StructNeedForce, "Agregá un punto o una zona de fuerza.", "Add a force point or zone.")                         \
    X(StructAnalyzing, "Calculando… %d %%", "Calculating… %d %%") \
    X(StructCancel, "Cancelar", "Cancel") \
    X(StructCancelled, "Análisis cancelado.", "Analysis cancelled.") \
    X(StructFailed, "No se pudo analizar: %s", "Could not analyze: %s") \
    X(VerdictHolds, "Aguanta", "Holds") \
    X(VerdictLowMargin, "Margen bajo", "Low margin") \
    X(VerdictOverloaded, "Fuera de carga", "Overloaded") \
    X(VerdictTooFlexible, "Demasiado flexible", "Too flexible") \
    X(VerdictOverTemp, "Fuera de temperatura", "Over temperature") \
    X(VerdictNotApplicable, "No aplicable", "Not applicable") \
    X(StructStale, "Desactualizado: analizá de nuevo", "Out of date: analyze again") \
    X(StructMinSafety, "Factor de seguridad mínimo: %.3g (pedido %.3g)", "Lowest safety factor: %.3g (asked %.3g)") \
    X(StructMaxDisplacement, "Desplazamiento máximo: %.3g mm", "Largest displacement: %.3g mm") \
    X(StructMass, "Masa: %.1f g de %s", "Mass: %.1f g of %s") \
    X(StructShowMap, "Ver el mapa", "Show the map") \
    X(StructMapSafety, "Factor de seguridad", "Safety factor") \
    X(StructMapStress, "Tensión de von Mises", "Von Mises stress") \
    X(StructMapMotion, "Desplazamiento", "Displacement") \
    X(StructLegendStress, "Azul: 0 · rojo: %.3g MPa", "Blue: 0 · red: %.3g MPa") \
    X(StructLegendMotion, "Azul: 0 · rojo: %.3g mm", "Blue: 0 · red: %.3g mm") \
    X(StructDeformed, "Ver la deformación exagerada", "Show the exaggerated deformation") \
    X(StructExaggerate, "× %.0f", "× %.0f") \
    X(StructLegend, "Rojo: menos de 1 · amarillo: hasta el pedido · verde claro: hasta el doble · verde: hasta 4 veces · azul: más", "Red: below 1 · yellow: up to the asked · light green: up to twice · green: up to 4 times · blue: more") \
    X(StructLimitLine, "Límite de desplazamiento: se mueve %.3g mm de %.3g", "Displacement limit: moves %.3g mm of %.3g") \
    X(StructFamilyTypical, "%s (valores típicos)", "%s (typical values)") \
    X(StructTryTitle, "Probá con:", "Try:") \
    X(StructResolution, "Resolución", "Resolution") \
    X(StructQualityFast, "Rápida (pieza maciza, unos 20 000 vóxeles)", "Fast (solid part, about 20 000 voxels)") \
    X(StructQualityNormal, "Media (2 vóxeles por ancho de línea)", "Medium (2 voxels per line width)") \
    X(StructQualityHigh, "Alta (4 vóxeles por ancho de línea)", "High (4 voxels per line width)") \
    X(StructQualityExtreme, "Extrema (6 vóxeles por ancho de línea; lenta)", "Extreme (6 voxels per line width; slow)") \
    X(StructSafetyBand, "Rango probable: %.3g a %.3g (±1σ de la adherencia y del tamaño de vóxel; falla %s)", "Likely range: %.3g to %.3g (±1σ of the adhesion and the voxel size; fails %s)") \
    X(StructFailsBetweenLayers, "entre capas", "between layers") \
    X(StructFailsBetweenLines, "entre líneas", "between lines") \
    X(StructFailsLine, "en la línea", "along the line") \
    X(StructAdhesion, "Adherencia a %.0f °C (referencia %.0f °C): entre capas %.0f %%, entre líneas %.0f %% de los datos del material", "Adhesion at %.0f °C (reference %.0f °C): between layers %.0f %%, between lines %.0f %% of the material's data") \
    X(StructAdhesionNone, "Sin datos de adherencia para este material: se usan sus valores tal cual (±50 %%)", "No adhesion data for this material: its values are used as they are (±50 %%)") \
    X(StructTooCold, "Temperatura baja para este material: las capas pegan poco", "Temperature low for this material: the layers bond poorly") \
    X(StructTooHot, "Temperatura alta para este material: el polímero se degrada", "Temperature high for this material: the polymer degrades") \
    X(Approximation, "Es una aproximación: sirve para comparar diseños, materiales y orientaciones, no reemplaza un ensayo ni un cálculo profesional.", "This is an approximation: it compares designs, materials and orientations; it does not replace a test or a professional calculation.") \
    X(StructEstimateCost, "Memoria: unos %.0f MB · tiempo: unos %s", "Memory: about %.0f MB · time: about %s") \
    X(StructPrintDirection, "Dirección de impresión", "Print direction") \
    X(StructPrintZ, "Z (capas horizontales)", "Z (flat layers)") \
    X(StructPrintBelt, "Cinta (capas a %.0f°)", "Belt (layers at %.0f°)") \
    X(StructPrintBeltOff, "La impresora elegida no es de cinta: se analiza en Z.", "The chosen printer is not a belt printer: analysed along Z.") \
    X(StructOptimize, "Optimizar", "Optimize") \
    X(StructOptimizing, "Buscando el relleno…", "Searching the infill…") \
    X(StructLightestInfill, "Relleno mínimo que aguanta", "Lowest infill that holds") \
    X(StructLightestInfillTip, "Busca el menor porcentaje de relleno con el que la pieza aguanta (varios análisis).", "Finds the lowest infill percentage at which the part holds (several analyses).") \
    X(StructOptimizeSolid, "Con la resolución Rápida la pieza se analiza maciza y el relleno no cuenta: elegí Media o más.", "At the Fast resolution the part is analysed solid and the infill does not count: choose Normal or finer.") \
    X(StructOptimizeNone, "Ni con 100 %% de relleno aguanta (factor %.2f): hacen falta más paredes, otro material u otra forma.", "Not even 100 %% infill holds (factor %.2f): it needs more walls, another material or another shape.") \
    X(StructOptimizeFound, "Aguanta con %d %% de relleno (factor %.2f).", "Holds with %d %% infill (factor %.2f).") \
    X(StructOptimizeApply, "Usar %d %% de relleno", "Use %d %% infill") \
    X(StructOptimizeApplied, "Relleno: %d %%", "Infill: %d %%") \
    X(StructZonedInfill, "Relleno por zonas", "Infill by zones") \
    X(StructZonedInfillTip, "Después del relleno uniforme, más relleno solo donde la tensión lo pide, en 3 niveles (varios análisis más).", "After the uniform infill, more infill only where the stress asks for it, in 3 levels (several more analyses).") \
    X(StructZonedFound, "Por zonas: %.0f / %.0f / %.0f %% (factor %.2f), %.2f g contra %.2f g del uniforme.", "By zones: %.0f / %.0f / %.0f %% (factor %.2f), %.2f g against %.2f g uniform.") \
    X(StructZonedHeavier, "Por zonas no queda más liviano que el uniforme: conviene el uniforme.", "By zones it is not lighter than uniform: the uniform infill is better.") \
    X(StructZonedNone, "Ninguna escala de las zonas aguanta: usá el relleno uniforme.", "No scale of the zones holds: use the uniform infill.") \
    X(StructZonedApply, "Usar las zonas", "Use the zones") \
    X(StructZonedApplied, "Relleno por zonas: %d modificadores", "Infill by zones: %d modifiers") \
    X(StructRibs, "Nervios", "Ribs") \
    X(StructRibsTip, "Después del relleno uniforme, el relleno en una sola dirección a lo largo de las tensiones donde trabajan en una dirección, y el mínimo buscado otra vez (varios análisis más).", "After the uniform infill, one-direction infill along the stresses where they work one way, and the lowest searched again (several more analyses).") \
    X(StructRibsFound, "Con nervios: %d %% (factor %.2f) contra %d %% sin ellos; %d zonas a %s.", "With ribs: %d %% (factor %.2f) against %d %% without; %d zones at %s.") \
    X(StructRibsNotLighter, "Los nervios no bajan el relleno: conviene el uniforme.", "The ribs do not lower the infill: the uniform infill is better.") \
    X(StructRibsNone, "No hay zonas donde el relleno trabaje en una sola dirección.", "There are no zones where the infill works one way.") \
    X(StructRibsApply, "Usar los nervios", "Use the ribs") \
    X(StructRibsApplied, "Nervios: %d %%, %d modificadores", "Ribs: %d %%, %d modifiers") \
    X(StructReinforce, "Refuerzos", "Reinforcements") \
    X(StructReinforceTip, "Después del relleno uniforme, 2 perímetros más y relleno más denso alrededor de los apoyos y las cargas, y el relleno mínimo del resto buscado otra vez (varios análisis más).", "After the uniform infill, 2 more walls and denser infill round the supports and the loads, and the lowest infill of the rest searched again (several more analyses).") \
    X(StructReinforceFound, "Con refuerzos (%.0f mm): el resto al %d %% (factor %.2f), %.2f g contra %.2f g sin ellos.", "With reinforcements (%.0f mm): the rest at %d %% (factor %.2f), %.2f g against %.2f g without.") \
    X(StructReinforceHeavier, "Con refuerzos no queda más liviano: conviene el uniforme.", "With reinforcements it is not lighter: the uniform infill is better.") \
    X(StructReinforceApply, "Usar los refuerzos", "Use the reinforcements") \
    X(StructReinforceApplied, "Refuerzos: el resto al %d %%, %d modificadores", "Reinforcements: the rest at %d %%, %d modifiers") \
    X(StructOrientation, "Orientación recomendada", "Recommended orientation") \
    X(StructOrientationTip, "Analiza la pieza con las capas en Z (como está), en X y en Y, con las mismas cargas (tres análisis). No revisa los voladizos de la nueva orientación.", "Analyses the part with its layers along Z (as it is), X and Y, with the same loads (three analyses). It does not check the overhangs of the new orientation.") \
    X(StructOrientationFactors, "Factor con capas en Z: %.2f; en X: %.2f; en Y: %.2f.", "Factor with layers along Z: %.2f; along X: %.2f; along Y: %.2f.") \
    X(StructOrientationKeep, "La orientación actual es la mejor, o la otra mejora menos de un 5 %%.", "The current orientation is the best, or the other one is less than 5 %% better.") \
    X(StructOrientationBetter, "Mejor con las capas en %s: factor %.2f (+%.0f %%).", "Better with the layers along %s: factor %.2f (+%.0f %%).") \
    X(StructOrientationNote, "La resistencia entre capas viene de la hoja del fabricante: una pieza real puede ser más débil entre capas. Revisá los voladizos antes de imprimir.", "The strength between layers comes from the maker's sheet: a real print may be weaker between layers. Check the overhangs before printing.") \
    X(StructOrientationApply, "Girar la pieza", "Turn the part") \
    X(StructOrientationApplied, "Pieza girada: capas en %s", "Part turned: layers along %s") \
    X(StructLattice, "Retícula", "Lattice") \
    X(StructLatticeTip, "La pieza sin relleno y por dentro una retícula imprimible sin soporte (barras verticales y a 45 grados); busca las barras más finas que aguantan (varios análisis).", "The part without infill and inside it a lattice that prints without support (vertical bars and bars at 45 degrees); finds the thinnest bars that hold (several analyses).") \
    X(StructLatticeFound, "Retícula: barras de %.2f mm en celdas de %.0f mm (factor %.2f), %.2f g.", "Lattice: bars of %.2f mm in cells of %.0f mm (factor %.2f), %.2f g.") \
    X(StructLatticeApply, "Usar la retícula", "Use the lattice") \
    X(StructLatticeApplied, "Retícula: barras de %.2f mm, la pieza sin relleno", "Lattice: bars of %.2f mm, the part without infill") \
    X(StructLatticeNone, "Ni con barras de 3 mm aguanta: conviene el relleno.", "Not even 3 mm bars hold: infill is better.") \
    X(StructZonedTurned, "La pieza está girada con escalas distintas: las zonas no caben como cajas.", "The part is turned with unequal scales: the zones do not fit as boxes.") \
    X(CloudPlanLock, "Incluido en el plan %s", "Included in the plan %s") \
    X(StructCloud, "Calcular en la nube", "Calculate in the cloud") \
    X(StructCloudTip, "Manda el análisis a Tisma Cloud; el resultado vuelve a esta pieza.", "Sends the analysis to Tisma Cloud; the result comes back to this part.") \
    X(StructCloudLogin, "Iniciá sesión en Tisma Cloud para calcular en la nube.", "Log in to Tisma Cloud to calculate in the cloud.") \
    X(StructAnalyzeCloud, "Analizar en la nube", "Analyze in the cloud") \
    X(StructCloudState, "En la nube: %s %d %%", "In the cloud: %s %d %%") \
    X(StructCloudDone, "Análisis en la nube listo.", "Analysis in the cloud done.") \
    X(StructCloudFailed, "El análisis en la nube falló: %s", "The analysis in the cloud failed: %s") \
    X(StructEstimate, "Unos %zu vóxeles de %.2f mm", "About %zu voxels of %.2f mm") \
    X(StructCritical, "Punto crítico: %.2f", "Critical point: %.2f") \
    X(StructNotValid, "El resultado no sirve:", "The result is not usable:") \
    X(StructSafetyFactor, "Factor de seguridad pedido", "Safety factor asked") \
    X(StructDirNormal, "Perpendicular, hacia adentro", "Normal, inward") \
    X(StructDirVector, "Tres números", "Three numbers") \
    X(StructKgIs, "= %.1f N", "= %.1f N") \
    X(StructForceUnitTip, "Cambiar entre N y kg (g = 9,81 m/s²)", "Switch between N and kg (g = 9.81 m/s²)") \
    X(TabFasteners, "Tornillería", "Fasteners") \
    X(ConfigMenu, "Configuración…", "Settings…") \
    X(PanelFolded, "Panel de ajustes plegado", "Settings panel folded") \
    X(CompactTitle, "Impresora, filamento y perfil", "Printer, filament and profile") \
    X(PanelFoldedNote, "Deja solo la impresora, el filamento y el perfil de impresión en un recuadro, y la vista a todo el ancho.", "Keeps only the printer, the filament and the print profile in a small card, and the view at full width.") \
    X(PanelFoldTip, "Plegar el panel: queda la impresora, el filamento y el perfil arriba de la vista", "Fold the panel: the printer, the filament and the profile stay on top of the view") \
    X(PanelUnfoldTip, "Abrir el panel de ajustes", "Open the settings panel") \
    X(ConfigTitle, "Configuración", "Settings") \
    X(ConfigSearch, "Buscar una opción", "Search an option") \
    X(ConfigGeneral, "General", "General") \
    X(ConfigInterface, "Interfaz", "Interface") \
    X(ConfigVideo, "Video", "Video") \
    X(ConfigCamera, "Cámara", "Camera") \
    X(ConfigAssistant, "Asistente", "Assistant") \
    X(ConfigPlugins, "Complementos", "Plug-ins") \
    X(ConfigOther, "Otros", "Other") \
    X(ConfigComputeGpu, "Cálculo en la placa de video", "Computing on the graphics card") \
    X(ConfigComputeGpuNote, "El laminado y el análisis usan la placa si hay; No los deja en el procesador.", "Slicing and the analysis use the card when there is one; No keeps them on the processor.") \
    X(ConfigAutomatic, "Automática", "Automatic") \
    X(ConfigNo, "No", "No") \
    X(ConfigLevel, "Nivel de ajustes", "Settings level") \
    X(ConfigMachine, "Este equipo", "This machine") \
    X(ConfigMachineCpu, "Procesador: %s (%d núcleos, %d hilos)", "Processor: %s (%d cores, %d threads)") \
    X(ConfigMachineRam, "Memoria: %s, %s libres", "Memory: %s, %s free") \
    X(ConfigMachineGpu, "Placa de video: %s · %s", "Graphics card: %s · %s") \
    X(ConfigMachineScreen, "Pantalla: %d × %d a %d Hz, escala %.2f", "Screen: %d × %d at %d Hz, scale %.2f") \
    X(ConfigMachineScreenNoRate, "Pantalla: %d × %d, escala %.2f", "Screen: %d × %d, scale %.2f") \
    X(ConfigOnBattery, "A batería", "On battery") \
    X(ConfigOnBatteryShort, "a batería", "on battery") \
    X(ConfigOnMains, "Enchufado", "Plugged in") \
    X(ConfigPowerUnknown, "Sin batería", "No battery") \
    X(ConfigRecommended, "Volver a lo recomendado", "Back to the recommended") \
    X(ConfigMeasureAgain, "Medir de nuevo", "Measure again") \
    X(ConfigVideoOptions, "Opciones", "Options") \
    X(ConfigGraphicsApi, "API gráfica", "Graphics API") \
    X(ConfigNoVulkan, "No hay Vulkan en este equipo", "There is no Vulkan on this machine") \
    X(ConfigNowDrawing, "Ahora dibuja con ", "Drawing now with ") \
    X(ConfigAppliesOnStart, "Un cambio se aplica al volver a abrir el programa.", "A change applies when the program opens again.") \
    X(ConfigGpuDevice, "Placa de video", "Graphics card") \
    X(ConfigGpuDeviceNote, "Automática pide la de alto rendimiento. Con OpenGL en Windows la elige Windows (Configuración de gráficos).", "Automatic asks for the high-performance one. With OpenGL on Windows, Windows picks it (Graphics settings).") \
    X(ConfigRenderLevel, "Nivel de OpenGL", "OpenGL level") \
    X(ConfigQuality, "Calidad", "Quality") \
    X(ConfigQualityChosen, "Para este equipo: %s", "For this machine: %s") \
    X(ViewQualityHigh, "Alta", "High") \
    X(ViewQualityBalanced, "Equilibrada", "Balanced") \
    X(ViewQualityLight, "Liviana", "Light") \
    X(ViewQualityCustom, "Personalizada", "Custom") \
    X(QualityAutomaticShort, "Calidad automática", "Automatic quality") \
    X(QualityStatus, "%s · %d FPS", "%s · %d FPS") \
    X(ReportMenu, "Informar un problema…", "Report a problem…") \
    X(ReportTitle, "Informar un problema", "Report a problem") \
    X(ReportWhat, "Contá qué falló, o qué te gustaría que Tisma haga. Le llega al equipo de Tisma, que junta los informes del mismo problema para arreglar primero los más frecuentes.", "Tell what went wrong, or what you would like Tisma to do. It goes to the Tisma team, which groups the reports of the same problem to fix the most frequent ones first.") \
    X(ReportPrivacy, "Los correos, las direcciones, el nombre de usuario en las rutas, las claves y los números de serie se quitan antes de guardarlo.", "E-mails, addresses, the user name in the paths, keys and serial numbers are removed before it is kept.") \
    X(ReportKindBug, "Algo no funciona", "Something does not work") \
    X(ReportKindCrash, "Tisma se cerró", "Tisma closed") \
    X(ReportKindSuggestion, "Sugerencia", "Suggestion") \
    X(ReportFewWords, "En pocas palabras", "In a few words") \
    X(ReportFewWordsHint, "p. ej. Faltan los soportes debajo del voladizo", "e.g. The supports are missing under the overhang") \
    X(ReportHappened, "Qué pasó", "What happened") \
    X(ReportSteps, "Pasos para que vuelva a pasar", "Steps to make it happen again") \
    X(ReportExpected, "Qué esperabas", "What you expected") \
    X(ReportAttach, "Adjuntar la versión, el sistema, la impresora en uso y el registro de esta sesión", "Attach the version, the system, the printer in use and the log of this session") \
    X(ReportSeeAttached, "Ver lo que se adjunta", "See what is attached") \
    X(ReportWithAccount, "Se manda con la cuenta %s: sus informes y su estado están en la copia de sus datos.", "Sent with the account %s: its reports and their state are in the copy of its data.") \
    X(ReportWithoutAccount, "Se manda sin cuenta.", "Sent without an account.") \
    X(ReportSend, "Mandar", "Send") \
    X(ReportNeedTitle, "Escribí en pocas palabras qué pasó.", "Write in a few words what happened.") \
    X(ReportSent, "Gracias: el informe le llegó al equipo de Tisma.", "Thank you: the report was sent to the Tisma team.") \
    X(ReportSimilar, "El mismo problema se informó %d veces antes: está entre los que se van a arreglar.", "The same problem was reported %d times before: it is among the ones to fix.") \
    X(ReportFailed, "No se pudo mandar el informe: %s", "The report could not be sent: %s") \
    X(ReportTooMany, "Se mandaron demasiados informes: probá de nuevo en una hora.", "Too many reports were sent: try again in an hour.") \
    X(CloudMenu, "Tisma Cloud…", "Tisma Cloud…") \
    X(CloudJobList, "Tus trabajos", "Your jobs") \
    X(CloudNoJobs, "Todavía no mandaste trabajos.", "You have not sent any jobs yet.") \
    X(CloudKindSlice, "Laminado", "Slice") \
    X(CloudKindFea, "Análisis", "Analysis") \
    X(CloudStateQueued, "En cola", "Queued") \
    X(CloudStateRunning, "Corriendo", "Running") \
    X(CloudStateDone, "Listo", "Done") \
    X(CloudStateFailed, "Falló", "Failed") \
    X(CloudStateCancelled, "Cancelado", "Cancelled") \
    X(CloudJobMinutes, "%s min", "%s min") \
    X(MyReportsButton, "Mis informes (%d)…", "My reports (%d)…") \
    X(MyReportsTitle, "Mis informes", "My reports") \
    X(MyReportsNone, "Todavía no mandaste informes con esta cuenta.", "You have not sent any reports with this account yet.") \
    X(ReportStateNew, "Nuevo", "New") \
    X(ReportStateConfirmed, "Confirmado", "Confirmed") \
    X(ReportStateFixed, "Arreglado", "Fixed") \
    X(ReportStateWontFix, "No se arregla", "Won't fix") \
    X(ReportStateDuplicate, "Repetido", "Duplicate") \
    X(CloudChipTip, "Tu cuenta de Tisma Cloud: entrar, horas del mes y planes", "Your Tisma Cloud account: log in, the month's hours and the plans") \
    X(CloudSliceMenu, "Laminar en la nube", "Slice in the cloud") \
    X(CloudSliceLogin, "Entrá a Tisma Cloud para laminar en la nube.", "Log in to Tisma Cloud to slice in the cloud.") \
    X(CloudSliceQueued, "Nube: en cola", "Cloud: queued") \
    X(CloudSliceRunning, "Nube: laminando %d %%", "Cloud: slicing %d %%") \
    X(CloudSliceSending, "Nube: mandando el proyecto", "Cloud: sending the project") \
    X(CloudSliceCancelTip, "Cancelar el laminado en la nube (también en el servidor)", "Cancel the slicing in the cloud (on the server too)") \
    X(CloudSliceDone, "Laminado en la nube: %s", "Sliced in the cloud: %s") \
    X(CloudSliceCancelled, "Laminado en la nube cancelado.", "Slicing in the cloud cancelled.") \
    X(CloudSliceFailed, "El laminado en la nube falló: %s", "Slicing in the cloud failed: %s") \
    X(CloudTooLarge, "El proyecto es más grande de lo que acepta el servidor.", "The project is larger than the server accepts.") \
    X(CloudBadParams, "El servidor no aceptó los parámetros: %s", "The server did not accept the parameters: %s") \
    X(CloudTitle, "Tisma Cloud", "Tisma Cloud") \
    X(CloudWhat, "Laminá y corré los análisis estructurales en los servidores de Tisma Cloud. La cuenta y el plan se manejan en el sitio web.", "Slice and run the structural analyses on the Tisma Cloud servers. The account and the plan are managed on the web site.") \
    X(CloudServer, "Servidor", "Server") \
    X(CloudEmail, "Correo", "E-mail") \
    X(CloudPassword, "Contraseña", "Password") \
    X(CloudNotLogged, "Sin sesión.", "Not logged in.") \
    X(CloudLoggedAs, "Con la sesión de %s.", "Logged in as %s.") \
    X(CloudLogIn, "Entrar", "Log in") \
    X(CloudLogOut, "Cerrar sesión", "Log out") \
    X(CloudWorking, "Hablando con el servidor…", "Talking to the server…") \
    X(CloudExpired, "La sesión terminó: entrá de nuevo.", "The session ended: log in again.") \
    X(CloudTooMany, "Demasiados pedidos: probá de nuevo en un rato.", "Too many requests: try again in a while.") \
    X(CloudNeedServer, "Escribí la dirección del servidor de Tisma Cloud.", "Enter the address of the Tisma Cloud server.") \
    X(CloudNeedLogin, "Escribí el correo y la contraseña.", "Enter the e-mail and the password.") \
    X(CloudNoStore, "Este equipo no tiene almacén de claves: la sesión dura hasta cerrar el programa.", "This computer has no secret store: the session lasts until the program closes.") \
    X(CloudPlan, "Plan", "Plan") \
    X(CloudHours, "Horas del mes", "Hours this month") \
    X(CloudHoursValue, "%s h de %s h (quedan %s h)", "%s h of %s h (%s h left)") \
    X(CloudPaidUntil, "Pagado hasta el %s", "Paid until %s") \
    X(CloudNotPaid, "Sin pagar", "Not paid") \
    X(CloudJobs, "Trabajos a la vez", "Jobs at a time") \
    X(CloudJobsValue, "%d de %d", "%d of %d") \
    X(CloudPlans, "Planes", "Plans") \
    X(CloudPlanHours, "%s h por mes", "%s h a month") \
    X(CloudPlanJobs, "%d a la vez", "%d at a time") \
    X(CloudPlanYours, "el tuyo", "yours") \
    X(CloudFree, "Gratis", "Free") \
    X(CloudNoPlans, "El servidor no mandó planes.", "The server sent no plans.") \
    X(CloudNoConnection, "No se pudo conectar con el servidor.", "Could not connect to the server.") \
    X(CloudPlansFailed, "El servidor no pudo mandar los planes (error %d).", "The server could not send the plans (error %d).") \
    X(CloudBilling, "Pagar o cambiar de plan…", "Pay or change the plan…") \
    X(CloudBillingTip, "Abre la página de la cuenta en el navegador: el pago no pasa por este programa.", "Opens the account page in the browser: payments do not go through this program.") \
    X(ViewHeldOcclusion, "oclusión congelada al mover", "occlusion held while moving") \
    X(ViewNoOcclusionMoving, "sin oclusión al mover", "no occlusion while moving") \
    X(ViewLoweredNotice, "Bajamos la calidad mientras se mueve para que la vista siga fluida. Podés cambiarlo en Configuración › Video.", "The quality goes down while something moves so the view stays smooth. You can change it in Settings › Video.") \
    X(ConfigBedShadow, "Sombra sobre la cama", "Shadow on the bed") \
    X(ConfigRenderScale, "Resolución de la vista", "View resolution") \
    X(ConfigLowerWhileMoving, "Bajar la calidad al mover", "Lower the quality while moving") \
    X(ConfigLowerWhileMovingNote, "Mientras algo se mueve la vista baja lo que haga falta y al soltar vuelve a la imagen completa.", "While something moves the view lowers what it must, and back to the full picture when released.") \
    X(ConfigMaxFps, "Tope de cuadros por segundo", "Frame rate cap") \
    X(ConfigFpsScreen, "La pantalla", "The screen's") \
    X(ConfigFpsNone, "Sin tope", "No cap") \
    X(ConfigVsync, "Sincronización vertical", "Vertical sync") \
    X(ConfigBatterySaver, "Ahorro a batería", "Battery saver") \
    X(ConfigShowFrameTime, "Mostrar el tiempo de la vista", "Show the view's time") \
    X(ConfigPerspective, "Cámara", "Camera") \
    X(ConfigPerspectiveOn, "Perspectiva", "Perspective") \
    X(ConfigOrthographic, "Ortogonal", "Orthographic") \
    X(ConfigShowAgain, "Mostrar de nuevo", "Show again") \
    X(ConfigRunPlugins, "Correr complementos al laminar", "Run plug-ins when slicing") \
    X(ConfigFactory, "Valores de fábrica", "Factory values") \
    X(ConfigFactoryButton, "Volver a los valores de fábrica", "Back to the factory values") \
    X(ConfigFactoryAsk, "¿Volver todas las opciones a los valores de fábrica? Los perfiles y los proyectos no cambian.", "Put every option back to its factory value? Profiles and projects do not change.") \
    X(ConfigFactoryYes, "Sí, volver", "Yes, put them back") \
    X(ConfigFactoryNote, "No toca perfiles ni proyectos.", "Leaves profiles and projects alone.") \
    X(ConfigExport, "Exportar e importar las preferencias", "Export and import the preferences") \
    X(ConfigOk, "Aceptar", "OK") \
    X(FastenerThumbScrew, "Tornillo de mano", "Thumb screw") \
    X(FastenerThumbNut, "Tuerca de mano", "Thumb nut") \
    X(FastenerStandoff, "Separador roscado", "Threaded standoff") \
    X(FastenerSpacer, "Espaciador", "Spacer") \
    X(GearsTitle, "Engranajes", "Gears") \
    X(GearsSubtitle, "Perfil de evolvente, con holgura para imprimir", "Involute profile, with backlash for printing") \
    X(DesignGearsTip, "Engranajes rectos, helicoidales y en espina de pescado, y cremalleras, con su pareja (también `engranaje` o `cremallera` en la línea de comandos)", "Spur, helical and herringbone gears, and racks, with their mate (also `gear` or `rack` on the command line)") \
    X(GearSpur, "Recto", "Spur") \
    X(GearHelical, "Helicoidal", "Helical") \
    X(GearHerringbone, "Espina de pescado", "Herringbone") \
    X(GearTeeth, "Dientes", "Teeth") \
    X(GearModule, "Módulo (mm)", "Module (mm)") \
    X(GearWidth, "Ancho (mm)", "Width (mm)") \
    X(GearBore, "Agujero (mm)", "Bore (mm)") \
    X(GearPressureAngle, "Ángulo de presión (°)", "Pressure angle (°)") \
    X(GearHelixAngle, "Ángulo de hélice (°)", "Helix angle (°)") \
    X(GearLeftHand, "Hélice a la izquierda", "Left-hand helix") \
    X(GearBacklash, "Holgura (mm)", "Backlash (mm)") \
    X(GearBacklashHint, "Cuánto más fino sale cada diente sobre el círculo primitivo, para que dos engranajes impresos giren sin trabarse. El agujero sale de la medida escrita: sumale 0,2 mm al eje.", "How much thinner each tooth comes out on the pitch circle, so two printed gears turn without binding. The bore comes out as typed: add 0.2 mm to the shaft.") \
    X(GearMate, "Pareja", "Mate") \
    X(GearMateTeeth, "Dientes del otro", "Mate's teeth") \
    X(GearCenterDistance, "Distancia entre ejes: %s mm", "Centre distance: %s mm") \
    X(GearRatio, "Relación: %s a 1", "Ratio: %s to 1") \
    X(GearPitchDiameter, "Diámetro primitivo", "Pitch diameter") \
    X(GearOutsideDiameter, "Diámetro exterior", "Outside diameter") \
    X(GearRootDiameter, "Diámetro de fondo", "Root diameter") \
    X(GearCircularPitch, "Paso circular", "Circular pitch") \
    X(GearSendPair, "Mandar con su pareja", "Send with its mate") \
    X(GearName, "Engranaje de %d dientes, m %s", "Gear, %d teeth, m %s") \
    X(RackName, "Cremallera de %d dientes, m %s", "Rack, %d teeth, m %s") \
    X(GearRack, "Cremallera", "Rack") \
    X(RackBase, "Base (mm)", "Base (mm)") \
    X(RackAxis, "Eje del engranaje: %s mm sobre la línea primitiva, %s mm sobre el pie de la cremallera", "The gear's axis: %s mm over the pitch line, %s mm over the rack's bottom") \
    X(RackTravel, "Una vuelta del engranaje la mueve %s mm", "One turn of the gear moves it %s mm") \
    X(RackLength, "Largo", "Length") \
    X(RackPitchLine, "Línea primitiva sobre el pie", "Pitch line over the bottom") \
    X(FastenersBack, "‹ Diseño", "‹ Design") \
    X(DesignFastenersTip, "Tornillos, tuercas, arandelas y varillas con rosca métrica, listos para imprimir (también `tornillo`, `tuerca`, `arandela` o `varilla` en la línea de comandos)", "Bolts, nuts, washers and rods with a metric thread, ready to print (also `bolt`, `nut`, `washer` or `rod` on the command line)") \
    X(UnitSwitchTip, "Cambiar entre milímetros y porcentaje (también se puede escribir la unidad con el número, como «0,3 mm»)", "Switch between millimetres and percent (you can also type the unit with the number, as \"0.3 mm\")") \
    X(ModePrint, "Impresión 3D", "3D printing") \
    X(ModePrintTip, "Preparar, laminar, analizar, calibrar e imprimir", "Prepare, slice, analyse, calibrate and print") \
    X(ModeEngineering, "Ingeniería", "Engineering") \
    X(ModeEngineeringTip, "Analizar y diseñar piezas: estructura, aire y diseño", "Analyze and design parts: structure, air and design") \
    X(FastenersSubtitle, "Rosca métrica ISO, con holgura para imprimir", "ISO metric thread, with play for printing") \
    X(FastenersEditing, "Editando una pieza de la placa.", "Editing a part of the plate.") \
    X(FastenersNew, "Pieza nueva", "New part") \
    X(FastenersSecPart, "Pieza", "Part") \
    X(FastenersSecSize, "Medida", "Size") \
    X(FastenerHexBolt, "Tornillo hexagonal", "Hex bolt") \
    X(FastenerSocketBolt, "Tornillo Allen", "Socket head bolt") \
    X(FastenerHexNut, "Tuerca", "Nut") \
    X(FastenerWasher, "Arandela", "Washer") \
    X(FastenerRod, "Varilla roscada", "Threaded rod") \
    X(FastenerMetric, "Métrica", "Metric size") \
    X(FastenerCustom, "A medida", "Custom") \
    X(FastenerDiameter, "Diámetro (mm)", "Diameter (mm)") \
    X(FastenerPitch, "Paso (mm)", "Pitch (mm)") \
    X(FastenerPitchHint, "0: el paso normal, %s mm", "0: the coarse pitch, %s mm") \
    X(FastenerLength, "Largo (mm)", "Length (mm)") \
    X(FastenerPlay, "Holgura (mm)", "Play (mm)") \
    X(FastenerPlayHint, "La rosca del tornillo sale así de más fina y la de la tuerca así de más ancha. 0,2 mm anda en la mayoría de las impresoras; si quedan duras, subila.", "The bolt's thread comes out this much thinner and the nut's this much wider. 0.2 mm works on most printers; if they are tight, raise it.") \
    X(FastenerThread, "Rosca", "Thread") \
    X(FastenerTapDrill, "Broca para roscar", "Tap drill") \
    X(FastenerClearanceHole, "Agujero de paso (ISO 273)", "Clearance hole (ISO 273)") \
    X(FastenerKey, "Llave", "Wrench") \
    X(FastenerBad, "Esas medidas no se pueden hacer.", "Those sizes cannot be made.") \
    X(FastenerPrintHint, "Los tornillos salen parados sobre la cabeza y las tuercas acostadas: así la rosca se imprime capa por capa, sin soportes.", "Bolts stand on their heads and nuts lie flat: so the thread prints layer by layer, with no supports.") \
    X(FastenerSendPair, "Mandar con su tuerca", "Send with its nut") \
    X(EditInFasteners, "Editar en Tornillería", "Edit in Fasteners") \
    X(Open, "Abrir", "Open")                                                                                          \
    X(OpenTip, "Abrir un modelo STL, un proyecto, un G-code o ajustes (Ctrl+O)", "Open an STL model, a project, a G-code file or settings (Ctrl+O)") \
    X(Slice, "Laminar", "Slice")                                                                                      \
    X(SliceTip, "Laminar el modelo con estos ajustes (Ctrl+R)", "Slice the model with these settings (Ctrl+R)")       \
    X(Export, "Exportar G-code", "Export G-code")                                                                     \
    X(ExportTip, "Escribir el G-code, volver a leerlo del disco y verificarlo (Ctrl+E)",                              \
      "Write the G-code, read it back from the disk and check it (Ctrl+E)")                                           \
    X(ExportNeedsSlice, "Primero hay que laminar", "Slice first")                                                     \
    X(More, "Más", "More")                                                                                            \
    X(CopyLink, "Copiar enlace", "Copy Link")                                                                         \
    X(OpenLink, "Abrir '%s'", "Open '%s'")                                                                           \
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
    X(FeedstockRow, "Alimentación", "Feedstock") \
    X(MaterialFeedstockRow, "Material para", "Material for") \
    X(FeedstockFilament, "Filamento", "Filament") \
    X(FeedstockPellet, "Pellets", "Pellets") \
    X(NozzleTemp, "Temperatura de boquilla", "Nozzle temperature")                                                    \
    X(BedTemp, "Temperatura de cama", "Bed temperature")                                                              \
    X(FirstNozzleTemp, "Boquilla, primera capa", "Nozzle, first layer")                                               \
    X(FirstBedTemp, "Cama, primera capa", "Bed, first layer")                                                         \
    X(FirstTempTip, "Vacío: la misma que en las demás capas", "Empty: the same as on the other layers")              \
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
    X(FuzzyNone, "Apagado", "Off")                                                                                        \
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
    X(NonplanarInfill, "Zigzag en Z", "Zigzag in Z")                                                                  \
    X(NonplanarRow, "Relleno no planar", "Nonplanar infill")                                                          \
    X(NonplanarNo, "No", "No")                                                                                        \
    X(NonplanarInfillTip, "El relleno deja de ser plano para que las capas se traben entre sí. Zigzag en Z: cada línea sube y baja como un diente de sierra, igual en todas las capas; solo con patrones que se repiten capa a capa (grilla, triángulos, estrellas, panal, alineado, concéntrico). Ondas entrelazadas: el relleno sube y baja como un cartón de huevos. Cuesta tiempo y un G-code más largo. La mejora de resistencia en Z medida en el artículo (un 62 por ciento más) fue en paredes de un cordón, no en relleno: falta medirla en el relleno.", "The infill stops being flat so the layers lock into each other. Zigzag in Z: each line rises and falls like a saw tooth, the same in every layer; only with patterns that repeat layer to layer (grid, triangles, stars, honeycomb, aligned, concentric). Interlocking waves: the infill rises and falls like an egg crate. It costs time and a longer G-code. The strength gain in Z measured in the paper (62 percent more) was on single-bead walls, not on infill: it is still to be measured on infill.") \
    X(NonplanarAmplitude, "Altura de la onda", "Wave height")                                                         \
    X(NonplanarWavelength, "Largo de la onda", "Wavelength")                                                          \
    X(InterlockWaves, "Ondas entrelazadas", "Interlocking waves") \
    X(InterlockWavesTip, "El relleno sube y baja como un cartón de huevos, igual en cada capa, así las capas encajan en vez de apoyarse en planos lisos.", "The infill rises and falls like an egg crate, the same in every layer, so the layers nest instead of lying on flat planes.") \
    X(WaveAmplitude, "Altura de las ondas", "Wave height") \
    X(WaveLength, "Largo de las ondas", "Wave length") \
    X(TopPattern, "Patrón de arriba", "Top pattern")                                                                  \
    X(Monotonic, "Monotónica", "Monotonic")                                                                           \
    X(MonotonicLines, "Líneas monotónicas", "Monotonic lines")                                                       \
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
    X(SupportAngle, "Ángulo de voladizo", "Overhang angle")                                                           \
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
    X(SpeedMaxFlow, "Caudal máximo", "Maximum flow")                                                                  \
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
    X(AddShape, "Agregar forma", "Add a shape")                                                                       \
    X(ShapeCube, "Cubo", "Cube")                                                                                      \
    X(ShapeSphere, "Esfera", "Sphere")                                                                                \
    X(SendToPrinter, "Mandar a la impresora", "Send to the printer")                                                  \
    X(SendToPrinterNone, "Mandar a la impresora (sin conectar)", "Send to the printer (not connected)")               \
    X(SaveOptions, "Más formas de guardar", "More ways to save")                                                      \
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
    X(Tutorial, "Volver al tutorial",\
      "Show the tutorial again")\
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
    X(TourSliceTitle, "Cómo se imprime una pieza",\
      "How a part is printed")\
    X(TourSlice, "La impresora no entiende un modelo 3D: sigue instrucciones, punto por punto. Laminar corta el modelo en capas finas, como fetas, y en cada capa dibuja el camino que hace la boquilla mientras derrite plástico. Esas instrucciones son el G-code, el archivo que va a la impresora.",\
      "The printer does not understand a 3D model: it follows instructions, point by point. Slicing cuts the model into thin layers and, on each one, draws the path the nozzle takes while it melts plastic. Those instructions are the G-code, the file that goes to the printer.")\
    X(TourLayersTitle, "Capas, paredes y relleno",\
      "Layers, walls and infill")\
    X(TourLayers, "Cada capa tiene paredes, que son el contorno de la pieza, y adentro un relleno con huecos que ahorra plástico y tiempo. Arriba y abajo, unas capas macizas cierran la pieza. Capas más finas, más detalle y más tiempo; más paredes y más relleno, más fuerza.",\
      "Each layer has walls, the outline of the part, and inside an infill with gaps that saves plastic and time. On top and at the bottom, solid layers close the part. Thinner layers mean more detail and more time; more walls and infill, more strength.")\
    X(TourSupportTitle, "Lo que cuelga y lo que se pega",\
      "What overhangs and what sticks")\
    X(TourSupport, "Cada capa se apoya en la de abajo: lo que sobresale en el aire necesita soportes, columnas que se sacan al final. Y todo empieza con la primera capa pegada a la cama; si la pieza tiene poca base, un borde alrededor la ayuda a no despegarse.",\
      "Each layer rests on the one below: whatever sticks out into the air needs supports, columns removed at the end. And everything starts with the first layer stuck to the bed; if the part has a small base, a brim around it helps it stay down.")\
    X(TourMaterialTitle, "El material y su temperatura",\
      "The material and its temperature")\
    X(TourMaterial, "Cada plástico se derrite a su temperatura: el PLA, el más fácil, entre 195 y 220 °C con la cama a unos 60 °C; el PETG y el ABS piden más. La caja del rollo dice su rango. Elegí tu filamento en el perfil de Material y, para afinarlo, usá Calibración: la torre de temperatura y la de retracción.",\
      "Each plastic melts at its own temperature: PLA, the easiest, between 195 and 220 °C with the bed at about 60 °C; PETG and ABS ask for more. The spool's box gives its range. Choose your filament in the Material profile and, to fine-tune it, use Calibration: the temperature tower and the retraction test.")\
    X(Tour1Title, "Abrí un modelo",\
      "Open a model")\
    X(Tour1, "Arrastrá un STL a la ventana o usá Abrir. El modelo queda centrado y apoyado en la cama. Con Agregar sumás más modelos a la placa.",\
      "Drop an STL on the window or use Open. The model sits centred on the bed. Add puts more models on the plate.")\
    X(Tour2Title, "Elegí impresora, filamento y perfil",\
      "Choose printer, filament and profile")\
    X(Tour2, "Arriba del panel de la izquierda: la impresora, el filamento y el perfil de impresión. El catálogo trae los de muchas marcas.",\
      "At the top of the left panel: the printer, the filament and the print profile. The catalog has those of many brands.")\
    X(Tour3Title, "Ajustá lo que necesites",\
      "Adjust what you need")\
    X(Tour3, "Cada ajuste tiene un botón (i) al lado de su nombre: abre una ficha con un dibujo, qué hace, qué cambia en la pieza y un ejemplo.",\
      "Each setting has an (i) button beside its name: it opens a card with a drawing, what it does, what it changes in the part and an example.")\
    X(Tour4Title, "Ubicá y prepará la pieza",\
      "Place and prepare the part")\
    X(Tour4, "La columna a la izquierda de la vista mueve, gira, escala, apoya, ordena y pinta los modelos. Cada herramienta explica cómo se usa la primera vez.",\
      "The column at the left of the view moves, turns, scales, lays flat, arranges and paints the models. Each tool explains itself the first time.")\
    X(Tour5Title, "Laminá y revisá",\
      "Slice and check")\
    X(Tour5, "Laminar arma el G-code y lo verifica. Vista previa muestra cada capa tal como se va a imprimir.",\
      "Slice makes the G-code and checks it. Preview shows every layer as it will print.")\
    X(Tour6Title, "Imprimí",\
      "Print")\
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
    X(OrientLeastPeel, "Orientar (menor fuerza de despegue)", "Orient (least peel force)") \
    X(OrientLeastSupport, "Orientar (menos soporte)", "Orient (least support)") \
    X(OrientDone, "Orientado: %d modelo(s) girado(s)", "Oriented: %d model(s) turned") \
    X(OrientNothing, "Ya estaba en la mejor orientación", "It already was in the best orientation") \
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
    X(ToolMeasureHint, "Clic en una cara del modelo (o Shift + clic: el vértice más cercano). Un agujero, un cilindro o un cono muestra enseguida su diámetro o su ángulo; entre dos elementos del mismo modelo, la distancia o, entre caras, el ángulo.", \
      "Click a face of a model (or Shift + click: the nearest vertex). A hole, a cylinder or a cone shows its diameter or angle at once; between two picks of the same model, the distance or, between faces, the angle.") \
    X(MeasurePlane, "Una cara", "A face")                                                                             \
    X(MeasureHole, "Agujero: Ø %.3f mm", "Hole: Ø %.3f mm")                                                           \
    X(MeasureCylinder, "Cilindro: Ø %.3f mm", "Cylinder: Ø %.3f mm")                                                  \
    X(MeasureCone, "Cono: %.2f° · Ø %.3f a %.3f mm", "Cone: %.2f° · Ø %.3f to %.3f mm")                                                                             \
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
    X(CalibrationHint, "Elegí una prueba: se abre su ficha, con qué hace y cómo se lee. Al crearla, la pieza se arma y se lamina sola.", \
      "Pick a test: its card opens, with what it does and how to read it. Created, the part is built and sliced by itself.") \
    X(CalSectionBasic, "Básicas, en este orden", "The basics, in this order")                                       \
    X(CalSectionMore, "Más pruebas", "More tests")                                                                    \
    X(CalHowToRead, "Cómo se lee", "How to read it")                                                                  \
    X(CalYourFilament, "Tu filamento: %s · boquilla %s · cama %s", "Your filament: %s · nozzle %s · bed %s")       \
    X(CalNoFilament, "el de los ajustes", "the one in the settings")                                                 \
    X(CalTowerRange, "Con tu filamento, la torre prueba de %s a %s, de a 5 °C.",                                      \
      "With your filament, the tower tries %s to %s, 5 °C apart.")                                                     \
    X(CalContinue, "Continuar: %s", "Continue: %s")                                                                   \
    X(CalAllBasicDone, "Las cuatro básicas están hechas con este filamento.", "The four basic ones are done with this filament.") \
    X(CalWhyFirst, "Por qué primero", "Why first")                                                                    \
    X(Create, "Crear", "Create")                                                                                      \
    X(CalBasicIntro, "Con estas cuatro pruebas cortas, en este orden y con el rollo que vas a usar, casi cualquier filamento imprime bien. Cada una arma su pieza sola: imprimila, mirala y elegí la que salió mejor.",\
      "With these four short tests, in this order and with the spool you will use, almost any filament prints well. Each one builds its own part: print it, look at it and pick the one that came out best.")\
    X(CalBasicFirstLayer, "La cama bien nivelada y la boquilla a la altura justa: si la primera capa no se pega pareja, nada de lo demás sale bien.",\
      "The bed level and the nozzle at the right height: if the first layer does not stick evenly, nothing else comes out well.")\
    X(CalBasicTemperature, "La mejor temperatura para tu rollo, dentro del rango que dice su caja: una torre que prueba de a 5 °C.",\
      "The best temperature for your spool, within the range on its box: a tower that tries it 5 °C at a time.")\
    X(CalBasicFlow, "Cuánto plástico empujar: ni huecos entre las líneas ni de más.",\
      "How much plastic to push: no gaps between the lines and no excess.")\
    X(CalBasicRetraction, "Cuánto tirar del filamento antes de cada viaje para que no queden hilos.",\
      "How far to pull the filament back before each travel so no strings are left.")\
    X(CalBasicDone, "Hecha", "Done")\
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
    X(PackSection, "Paquete libre de perfiles", "Free profile pack") \
    X(PackGetList, "Bajar la lista de impresoras del paquete", "Download the pack's printer list") \
    X(PackHint, "Miles de impresoras más, de un repositorio aparte de uso libre. Se baja solo lo que elegís.", "Thousands more printers, from a separate free repository. Only what you choose is downloaded.") \
    X(PackDownloadAdd, "Bajar y agregar", "Download and add") \
    X(PackUpdate, "Actualizar el paquete", "Update the pack") \
    X(PackOrigin, "Licencia %s · origen %s", "License %s · origin %s") \
    X(PackNotDownloaded, "Sus perfiles todavía no se bajaron.", "Its profiles are not downloaded yet.") \
    X(PackBusy, "Paquete: %s", "Pack: %s") \
    X(PackFailed, "No se pudo bajar el paquete: %s", "The pack could not be downloaded: %s") \
    X(PackReady, "Paquete de perfiles %s instalado", "Profile pack %s installed") \
    X(PackAdded, "Agregada %s con su filamento y su perfil de impresión", "Added %s with its filament and print profile") \
    X(PackTechnology, "Tecnología: %s", "Technology: %s") \
    X(Material, "Material", "Material")                                                                               \
    X(NozzleRange, "Boquilla", "Nozzle")                                                                              \
    X(BedRange, "Cama", "Bed")                                                                                        \
    X(Recommended, "recomendada", "recommended")                                                                      \
    X(Density, "Densidad", "Density")                                                                                 \
    X(NeedsEnclosure, "Necesita impresora cerrada", "Needs an enclosed printer")                                      \
    X(CatalogEmpty, "Sin resultados", "No results")                                                                   \
    X(CatalogNoMaterial, "El catálogo no tiene materiales para la alimentación o el diámetro de filamento de esta impresora", \
      "The catalog has no material for this printer's feedstock or filament diameter")                                \
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
    X(GuidesFilament, "Del filamento: se repiten con cada filamento", "Filament: repeated with each filament")        \
    X(GuidesPrinter, "De la impresora: al cambiar la máquina", "Printer: when the machine changes")                     \
    X(GuideRepeat, "Cuándo repetirla: %s", "When to repeat it: %s")                                                  \
    X(GuideBefore, "Antes conviene hacer: %s", "Better do first: %s")                                                 \
    X(GuideSteps, "Pasos", "Steps")                                                                                   \
    X(GuideLook, "Lo que ves", "What you see")                                                                        \
    X(GuideMeans, "Qué significa", "What it means")                                                                   \
    X(GuideResult, "Resultado", "Result")                                                                             \
    X(GuideNow, "Ahora: %s", "Now: %s")                                                                               \
    X(GuideStart, "Inicio de la prueba", "Test start")                                                                \
    X(GuideStepPerBand, "Paso por franja", "Step per band")                                                           \
    X(GuideBand, "Alto de franja (mm)", "Band height (mm)")                                                           \
    X(GuideHeight, "Altura medida (mm)", "Measured height (mm)")                                                      \
    X(GuideValue, "Valor leído en la pieza", "Value read on the print")                                               \
    X(GuideLayers, "Capas de separación", "Layers of gap")                                                            \
    X(GuideReadValue, "Leído: %s", "Read: %s")                                                                        \
    X(GuideProposed, "Valor propuesto", "Proposed value")                                                             \
    X(GuideFirmware, "Este resultado no va en el perfil: se guarda en la impresora.",                                 \
      "This result does not go in the profile: it is saved in the printer.")                                          \
    X(GuideTips, "Consejos", "Tips")                                                                                  \
    X(GuideFrequency, "Frecuencia (Hz)", "Frequency (Hz)")                                                          \
    X(GuideSend, "Mandar a la impresora", "Send to the printer")                                                      \
    X(GuideSent, "Mandado a la impresora: %s", "Sent to the printer: %s")                                             \
    X(GuideHow, "Cómo se hace", "How it is done")                                                                     \
    X(GuideCalibrate, "Calibrar", "Calibrate")                                                                        \
    X(ReviewFix, "¿Cómo lo arreglo?", "How do I fix it?")                                                            \
    X(AssistantTutorials, "Ofrecer los tutoriales", "Offer the tutorials")                                            \
    X(AssistantGlossary, "Definiciones del glosario junto a los ajustes", "Glossary definitions next to the settings") \
    X(AssistantMaterial, "Ficha y avisos del material", "Material card and warnings")                                 \
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
    X(AnalysisCli, "El análisis completo, con apoyos y cargas, corre por ahora en la línea de comandos (docs/ANALYSIS.md):", \
      "The full analysis, with supports and loads, runs for now on the command line (docs/ANALYSIS.md):") \
    X(PerLayer, "%d por capa", "%d per layer") \
    X(SecQuality, "Calidad", "Quality") \
    X(SecStrength, "Resistencia", "Strength") \
    X(SecSpeed, "Velocidad", "Speed") \
    X(SecSupport, "Soportes", "Support") \
    X(SecAdhesion, "Adhesión", "Adhesion") \
    X(SecOther, "Otros", "Others") \
    X(SecNonplanar, "No planar", "Non-planar") \
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
    X(SubWavy, "Relleno no planar", "Nonplanar infill") \
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
    X(SumNoArcs, "Apagado", "Off") \
    X(SubSupportPattern, "Patrón", "Pattern") \
    X(SubSkirt, "Falda", "Skirt") \
    X(SubBrim, "Borde", "Brim") \
    X(SubSpecialModes, "Modos especiales", "Special modes") \
    X(SubDesign, "Diseño", "Design") \
    X(ChordTolerance, "Tolerancia de la malla", "Mesh tolerance") \
    X(AngleTolerance, "Ángulo entre facetas", "Angle between facets") \
    X(TipDesignTitle, "Diseño", "Design") \
    X(TipDesign, "Dibujá y modelá una pieza acá adentro y mandala a la placa sin pasar por otro programa. Por ahora: primitivas con sus medidas; los bocetos y las operaciones vienen después.", \
      "Draw and model a part right here and send it to the plate without another program. For now: primitives with their sizes; sketches and operations come later.") \
    X(DesignTitle, "Diseño", "Design") \
    X(DesignSecNew, "Nuevo", "New") \
    X(DesignSecSketch, "Boceto", "Sketch") \
    X(DesignSecOperations, "Operaciones", "Operations") \
    X(DesignSecParameters, "Parámetros", "Parameters") \
    X(DesignSecHistory, "Historial", "History") \
    X(ParamAdd, "Agregar parámetro", "Add parameter") \
    X(ParamNone, "Sin parámetros. Una medida con fx usa uno: «2 * ancho + 5 mm».", "No parameters. A size with fx uses one: «2 * width + 5 mm».") \
    X(ParamDelete, "Borrar el parámetro", "Delete the parameter") \
    X(ParamBind, "Usar un parámetro o una expresión («ancho», «2 * ancho + 5 mm»); vacío: el número", "Use a parameter or an expression («width», «2 * width + 5 mm»); empty: the number") \
    X(ParamBadValue, "No da un valor", "Gives no value") \
    X(RectWidth, "Ancho", "Width") \
    X(RectHeight, "Alto", "Height") \
    X(DesignSecMeasures, "Medidas", "Measures") \
    X(DesignSketch, "Boceto", "Sketch") \
    X(DesignPrimitive, "Primitiva", "Primitive") \
    X(DesignOpenStep, "Abrir STEP", "Open STEP") \
    X(ComingSoon, "Próximamente", "Coming soon") \
    X(PrimBox, "Caja", "Box") \
    X(PrimCylinder, "Cilindro", "Cylinder") \
    X(PrimSphere, "Esfera", "Sphere") \
    X(PrimCone, "Cono", "Cone") \
    X(PrimTube, "Tubo", "Tube") \
    X(PrimTorus, "Toro", "Torus") \
    X(SizeLength, "Largo", "Length") \
    X(SizeWidth, "Ancho", "Width") \
    X(SizeHeight, "Alto", "Height") \
    X(SizeDiameter, "Diámetro", "Diameter") \
    X(SizeTopDiameter, "Diámetro de arriba", "Top diameter") \
    X(SizeInnerDiameter, "Diámetro interior", "Inner diameter") \
    X(SizeRingDiameter, "Diámetro del anillo", "Ring diameter") \
    X(SizeSectionDiameter, "Diámetro de la sección", "Section diameter") \
    X(DesignSend, "Mandar a la placa", "Send to the plate") \
    X(DesignSendTip, "Ctrl+Mayús+P: la pieza pasa a Preparar como un objeto; si vino de ahí, lo reemplaza y conserva su lugar, sus ajustes y lo pintado", \
      "Ctrl+Shift+P: the part goes to Prepare as an object; if it came from there, it replaces it and keeps its place, settings and painting") \
    X(DesignNeedsSolid, "Hacé un sólido primero.", "Make a solid first.") \
    X(DesignRestarted, "El componente de diseño se cerró y se abrió de nuevo.", "The design component closed and was opened again.") \
    X(DesignMissing, "Falta el componente de diseño: reinstalá Tisma para usar Diseño.", "The design component is missing: reinstall Tisma to use Design.") \
    X(DesignChecking, "Comprobando el componente de diseño…", "Checking the design component…") \
    X(DesignBuilding, "Construyendo…", "Building…") \
    X(DesignVolume, "Volumen", "Volume") \
    X(DesignBox, "Caja", "Box") \
    X(DesignBodies, "Cuerpos", "Bodies") \
    X(DesignNoBodies, "Todavía no hay cuerpos", "No bodies yet") \
    X(DesignSuppress, "Suprimir (el ojo)", "Suppress (the eye)") \
    X(DesignDeleteFeature, "Borrar", "Delete") \
    X(OpFillet, "Redondeo", "Fillet") \
    X(OpChamfer, "Chaflán", "Chamfer") \
    X(OpShell, "Vaciado", "Shell") \
    X(OpHole, "Agujero", "Hole") \
    X(OpMirror, "Simetría", "Mirror") \
    X(OpPatternCircular, "Patrón circular", "Circular pattern") \
    X(OpPatternRectangular, "Patrón en fila", "Row pattern") \
    X(OpCombine, "Combinar", "Combine") \
    X(OpThread, "Rosca", "Thread") \
    X(OpMove, "Mover cuerpo", "Move body") \
    X(OpSplit, "Partir", "Split") \
    X(OpDraft, "Desmoldeo", "Draft") \
    X(OpPickDraft, "Hacé clic en las caras a inclinar; Enter termina", "Click the faces to tilt; Enter finishes") \
    X(OpOffset, "Desplazar X, Y, Z (mm)", "Offset X, Y, Z (mm)") \
    X(OpTurn, "Girar (°)", "Turn (°)") \
    X(OpCopyBody, "Copia (deja el original)", "Copy (keeps the original)") \
    X(OpPlaneOffset, "Corrido (mm)", "Moved by (mm)") \
    X(OpDraftAngle, "Ángulo (°)", "Angle (°)") \
    X(OpNeutral, "Plano neutro", "Neutral plane") \
    X(OpPickThread, "Hacé clic en la cara cilíndrica a roscar; Enter termina", "Click the cylindrical face to thread; Enter finishes") \
    X(OpTapped, "Roscado (broca de macho)", "Tapped (tap drill)") \
    X(OpModeled, "Rosca modelada (no solo dibujada)", "Modelled thread (not only drawn)") \
    X(OpHand, "Sentido", "Hand") \
    X(OpHandRight, "Derecha", "Right") \
    X(OpHandLeft, "Izquierda", "Left") \
    X(OpHandBoth, "Las dos", "Both") \
    X(OpLength, "Largo (0: toda la cara)", "Length (0: the whole face)") \
    X(OpPickEdges, "Hacé clic en las aristas a redondear o achaflanar; Enter termina", "Click the edges to round or chamfer; Enter finishes") \
    X(OpPickFaces, "Hacé clic en las caras que quedan abiertas (ninguna: hueco cerrado); Enter termina", "Click the faces left open (none: closed hollow); Enter finishes") \
    X(OpPickHoles, "Hacé clic donde va cada agujero, sobre una cara; Enter termina", "Click where each hole goes, on a face; Enter finishes") \
    X(OpNoEdge, "Ahí no hay una arista", "There is no edge there") \
    X(OpPointAdded, "Punto %d agregado", "Point %d added") \
    X(OpBody, "Cuerpo", "Body") \
    X(OpRadius, "Radio", "Radius") \
    X(OpDistance, "Distancia", "Distance") \
    X(OpThickness, "Espesor", "Thickness") \
    X(OpDiameter, "Diámetro", "Diameter") \
    X(OpStandard, "Norma", "Standard") \
    X(OpDiameterFree, "Diámetro libre", "Free diameter") \
    X(OpFitClose, "Justo", "Close") \
    X(OpFitNormal, "Normal", "Normal") \
    X(OpFitLoose, "Holgado", "Loose") \
    X(OpDepth, "Profundidad (0: pasante)", "Depth (0: through)") \
    X(OpCount, "Cantidad", "Count") \
    X(OpAngle, "Ángulo", "Angle") \
    X(OpAxis, "Eje", "Axis") \
    X(OpSpacing, "Separación", "Spacing") \
    X(OpDirection, "Dirección", "Direction") \
    X(OpPlane, "Plano", "Plane") \
    X(OpTools, "Con", "With") \
    X(OpJoin, "Unir", "Join") \
    X(OpCut, "Restar", "Cut") \
    X(OpIntersect, "Intersecar", "Intersect") \
    X(OpPoints, "%d puntos", "%d points") \
    X(OpOnePoint, "1 punto", "1 point") \
    X(OpPickAgain, "Elegir en la vista", "Pick in the view") \
    X(OpPicking, "Eligiendo… (Enter termina)", "Picking… (Enter finishes)") \
    X(OpClearPoints, "Borrar puntos", "Clear points") \
    X(OpNeedBody, "Hacé un sólido primero.", "Make a solid first.") \
    X(DesignDone, "Listo", "Done") \
    X(DesignMoveUp, "Subir: se hace antes", "Move up: done earlier") \
    X(DesignMoveDown, "Bajar: se hace después", "Move down: done later") \
    X(DesignRenameTip, "Doble clic para cambiarle el nombre", "Double-click to rename") \
    X(DesignNameTaken, "Ya hay una operación que se llama así", "An operation already has that name") \
    X(DesignFails, "Falla: %s", "Fails: %s") \
    X(DesignWaiting, "En espera: falla una operación anterior", "Waiting: an earlier operation fails") \
    X(DesignSent, "Mandado a la placa", "Sent to the plate") \
    X(DesignReplaced, "El objeto se rehizo en la placa", "The object was remade on the plate") \
    X(EditInDesign, "Editar en Diseño", "Edit in Design") \
    X(DesignPref, "Pestaña Diseño", "Design tab") \
    X(DesignBasedOn, "Basado en SolidCraft (MIT o Apache-2.0)", "Based on SolidCraft (MIT or Apache-2.0)") \
    X(CommandLineHint, "Escribí una herramienta: caja, cilindro, esfera, cono, tubo, toro (Enter en vacío repite la última)", \
      "Type a tool: box, cylinder, sphere, cone, tube, torus (Enter on an empty line repeats the last one)") \
    X(CommandUnknown, "No conozco «%s». Herramientas: caja, cilindro, esfera, cono, tubo, toro, mandar", \
      "Unknown tool \"%s\". Tools: box, cylinder, sphere, cone, tube, torus, send") \
    X(CommandSoon, "«%s» viene en la próxima etapa (bocetos)", "\"%s\" comes in the next stage (sketches)") \
    X(CommandBadNumber, "Hace falta un número mayor que 0", "A number above 0 is needed") \
    X(CommandAdded, "%s agregado", "%s added") \
    X(CommandCancelled, "Cancelado", "Cancelled") \
    X(ToolSketchTip, "Boceto: abre Diseño y empieza un boceto", "Sketch: opens Design and starts a sketch") \
    X(SketchPlane, "Plano del boceto", "Sketch plane") \
    X(SketchPlaneXY, "XY (planta)", "XY (top)") \
    X(SketchPlaneXZ, "XZ (frente)", "XZ (front)") \
    X(SketchPlaneYZ, "YZ (lado)", "YZ (side)") \
    X(SketchToolLine, "Línea (L)", "Line (L)") \
    X(SketchToolRect, "Rectángulo (REC)", "Rectangle (REC)") \
    X(SketchToolCircle, "Círculo (C)", "Circle (C)") \
    X(SketchToolArc, "Arco (A)", "Arc (A)") \
    X(SketchExtrude, "Extruir (E)", "Extrude (E)") \
    X(SketchRevolve, "Revolucionar (REV)", "Revolve (REV)") \
    X(SketchImportDxf, "Traer DXF", "Bring in DXF") \
    X(SketchImportSvg, "Traer SVG", "Bring in SVG") \
    X(SketchExportDxf, "Exportar DXF", "Export DXF") \
    X(SketchImported, "Se trajeron %d curvas de %s", "Brought in %d curves from %s") \
    X(SketchCannotRead, "no se puede leer el archivo", "the file cannot be read") \
    X(SketchImportFailed, "No se pudo traer el dibujo", "The drawing could not be brought in") \
    X(DesignViewDxf, "Vista como DXF", "View as DXF") \
    X(DesignViewDxfHidden, "Ocultas punteadas", "Hidden dashed") \
    X(DesignViewDxfTip, "Guarda las aristas de la vista actual como líneas DXF.", "Saves the current view's edges as DXF lines.") \
    X(DesignViewExported, "Vista exportada: %s", "View exported: %s") \
    X(SketchExported, "Boceto exportado: %s", "Sketch exported: %s") \
    X(SketchFinish, "Terminar boceto", "Finish sketch") \
    X(SketchDof, "Grados de libertad: %d", "Degrees of freedom: %d") \
    X(SketchDofNone, "Totalmente definido", "Fully defined") \
    X(SketchDofUnsolved, "No se puede cumplir todo: revisá las cotas", "Not everything can be met: check the dimensions") \
    X(SketchRules, "Restricciones", "Constraints") \
    X(SketchEditTools, "Editar", "Edit") \
    X(EditTrim, "Recortar", "Trim") \
    X(EditExtend, "Alargar", "Extend") \
    X(EditOffset, "Desfasar", "Offset") \
    X(EditFillet, "Redondear", "Fillet") \
    X(EditChamfer, "Chaflán", "Chamfer") \
    X(EditMirror, "Simetría", "Mirror") \
    X(EditArray, "Matriz", "Array") \
    X(EditMove, "Mover", "Move") \
    X(EditCopy, "Copiar", "Copy") \
    X(EditRotate, "Girar", "Rotate") \
    X(EditScale, "Escalar", "Scale") \
    X(PromptTrim, "Recortar: hacé clic en la parte a quitar (Enter termina)", "Trim: click the part to cut away (Enter ends)") \
    X(PromptExtend, "Alargar: hacé clic cerca del extremo a alargar (Enter termina)", "Extend: click near the end to extend (Enter ends)") \
    X(PromptOffsetDistance, "Desfasar: distancia <%.4g>", "Offset: distance <%.4g>") \
    X(PromptOffsetPick, "Desfasar: elegí la curva (Enter termina)", "Offset: pick the curve (Enter ends)") \
    X(PromptOffsetSide, "Desfasar: hacé clic del lado hacia donde va", "Offset: click the side it goes to") \
    X(PromptChamferDistance, "Chaflán: distancia <%.4g>", "Chamfer: distance <%.4g>") \
    X(PromptFilletRadius, "Redondear: radio <%.4g>", "Fillet: radius <%.4g>") \
    X(PromptFilletPick, "Redondear: hacé clic en la esquina (Enter termina)", "Fillet: click the corner (Enter ends)") \
    X(PromptChamferPick, "Chaflán: hacé clic en la esquina (Enter termina)", "Chamfer: click the corner (Enter ends)") \
    X(PromptSelect, "Elegí los objetos: clic, o arrastrá (a la derecha, ventana; a la izquierda, cruce). Enter sigue", "Pick the objects: click, or drag (to the right, window; to the left, crossing). Enter goes on") \
    X(PromptMirror1, "Simetría: primer punto de la recta", "Mirror: first point of the line") \
    X(PromptMirror2, "Simetría: segundo punto de la recta", "Mirror: second point of the line") \
    X(PromptArrayKind, "Matriz [Rectangular/Polar] <Polar>", "Array [Rectangular/Polar] <Polar>") \
    X(PromptArrayCentre, "Matriz polar: centro", "Polar array: centre") \
    X(PromptArrayCount, "Matriz polar: cantidad en total <%d>", "Polar array: count in all <%d>") \
    X(PromptArrayAngle, "Matriz polar: ángulo a llenar <%.4g>", "Polar array: angle to fill <%.4g>") \
    X(PromptArrayColumns, "Matriz: columnas <%d>", "Array: columns <%d>") \
    X(PromptArrayRows, "Matriz: filas <%d>", "Array: rows <%d>") \
    X(PromptArrayDx, "Matriz: separación entre columnas <%.4g>", "Array: column spacing <%.4g>") \
    X(PromptArrayDy, "Matriz: separación entre filas <%.4g>", "Array: row spacing <%.4g>") \
    X(PromptBase, "Punto base", "Base point") \
    X(PromptSecond, "Segundo punto (o @dx,dy)", "Second point (or @dx,dy)") \
    X(PromptAngle, "Ángulo de giro <%.4g>", "Rotation angle <%.4g>") \
    X(PromptFactor, "Factor de escala <%.4g>", "Scale factor <%.4g>") \
    X(EditDistance, "Medir distancia", "Measure distance") \
    X(EditArea, "Medir área", "Measure area") \
    X(PromptDistance1, "Distancia: primer punto (Enter termina)", "Distance: first point (Enter ends)") \
    X(PromptDistance2, "Distancia: segundo punto", "Distance: second point") \
    X(PromptArea, "Área: hacé clic dentro de una región (Enter termina)", "Area: click inside a region (Enter ends)") \
    X(SketchMeasureDistance, "Distancia %.4g mm · ΔX %.4g · ΔY %.4g · ángulo %.4g°", "Distance %.4g mm · ΔX %.4g · ΔY %.4g · angle %.4g°") \
    X(SketchMeasureArea, "Área %.2f mm² · perímetro %.2f mm", "Area %.2f mm² · perimeter %.2f mm") \
    X(SketchMeasureHoles, " · %d agujeros", " · %d holes") \
    X(EditNoRegion, "Ahí no hay una región cerrada", "No closed region there") \
    X(EditNothing, "Ahí no hay nada para eso", "Nothing there for that") \
    X(EditNoSelection, "No hay nada elegido", "Nothing is picked") \
    X(EditDone, "Hecho", "Done") \
    X(RuleDimension, "Cota", "Dimension") \
    X(RuleHorizontal, "Horizontal", "Horizontal") \
    X(RuleVertical, "Vertical", "Vertical") \
    X(RuleParallel, "Paralela", "Parallel") \
    X(RulePerpendicular, "Perpendicular", "Perpendicular") \
    X(RuleEqual, "Igual", "Equal") \
    X(RuleCoincident, "Coincidente", "Coincident") \
    X(RuleConcentric, "Concéntrica", "Concentric") \
    X(RuleTangent, "Tangente", "Tangent") \
    X(RuleFixed, "Fijo", "Fix") \
    X(RuleAngle, "Ángulo", "Angle") \
    X(RulePickSegment, "%s: elegí una línea", "%s: pick a line") \
    X(RulePickSecondSegment, "%s: elegí la segunda línea", "%s: pick the second line") \
    X(RulePickPoint, "%s: elegí un punto", "%s: pick a point") \
    X(RulePickSecondPoint, "%s: elegí el segundo punto", "%s: pick the second point") \
    X(RulePickRound, "%s: elegí un círculo o arco", "%s: pick a circle or arc") \
    X(RulePickSecondRound, "%s: elegí el segundo círculo o arco", "%s: pick the second circle or arc") \
    X(RulePickAny, "Cota: elegí una línea, un círculo o un punto", "Dimension: pick a line, a circle or a point") \
    X(RuleNothingThere, "Ahí no hay nada que sirva para esta restricción", "Nothing there fits this constraint") \
    X(RuleValue, "Valor (Enter deja %s):", "Value (Enter keeps %s):") \
    X(RuleAdded, "Restricción agregada: %s", "Constraint added: %s") \
    X(RuleRedundant, "Esa restricción sobra: no se agregó", "That constraint is redundant: not added") \
    X(RuleConflicting, "Esa restricción contradice a otras: no se agregó", "That constraint contradicts others: not added") \
    X(SketchSnaps, "Referencias (F3)", "Snaps (F3)") \
    X(SketchOrtho, "Ortogonal (F8)", "Ortho (F8)") \
    X(SketchGrid, "Cuadrícula (F9)", "Grid (F9)") \
    X(SketchPolar, "Polar (F10)", "Polar (F10)") \
    X(SketchLength, "Largo", "Length") \
    X(SketchAngle, "Ángulo", "Angle") \
    X(SketchDistance, "Distancia", "Distance") \
    X(SketchAxis, "Eje (x/y)", "Axis (x/y)") \
    X(SketchHint, "Hacé clic en la vista o escribí un punto: x,y · @dx,dy · @largo<ángulo", "Click in the view or type a point: x,y · @dx,dy · @length<angle") \
    X(SketchStarted, "Boceto en %s. Herramientas: L, REC, C, A; E extruye, REV revoluciona, Esc sale", "Sketch on %s. Tools: L, REC, C, A; E extrudes, REV revolves, Esc leaves") \
    X(SketchClosed, "Contorno cerrado", "Outline closed") \
    X(SketchNeedsThree, "Hacen falta tres puntos para cerrar", "Three points are needed to close") \
    X(SketchNoRegion, "El boceto no tiene un contorno cerrado", "The sketch has no closed outline") \
    X(EditSketch, "Editar boceto", "Edit sketch") \
    X(OpExtrude, "Extruir", "Extrude") \
    X(OpRevolve, "Revolucionar", "Revolve") \
    X(SketchLeft, "Boceto guardado; se construye la pieza", "Sketch kept; the part is being built") \
    X(SketchToolLeft, "Herramienta cerrada", "Tool closed") \
    X(SnapEndpoint, "Extremo", "Endpoint") \
    X(SnapMidpoint, "Punto medio", "Midpoint") \
    X(SnapCenter, "Centro", "Centre") \
    X(SnapQuadrant, "Cuadrante", "Quadrant") \
    X(SnapIntersection, "Intersección", "Intersection") \
    X(SnapPerpendicular, "Perpendicular", "Perpendicular") \
    X(SnapTangent, "Tangente", "Tangent") \
    X(SnapNearest, "Más cercano", "Nearest") \
    X(SnapGrid, "Cuadrícula", "Grid") \
    X(ToolPrimitiveTip, "Primitiva: abre Diseño con una caja, un cilindro, una esfera…", "Primitive: opens Design with a box, a cylinder, a sphere…") \
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
    X(SubWipe, "Barrido", "Wipe") \
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
    X(OpenInDevices, "Abrir la pestaña Impresora", "Open the Printer tab") \
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
    X(SumOn, "Encendido", "On") \
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
    X(SumTwoSpeeds, "%s · %s", "%s · %s") \
    X(SumNoSupport, "Sin soportes", "No supports") \
    X(SumAutoSupport, "Automáticos, %s°", "Automatic, %s°") \
    X(SumPaintedSupport, "Solo lo pintado", "Painted only") \
    X(SumGaps, "Z %s mm · XY %s", "Z %s mm · XY %s") \
    X(SumLayersN, "%d capas", "%d layers") \
    X(SumSkirt, "%d vueltas a %s mm", "%d loops at %s mm") \
    X(SumNoSkirt, "Sin falda", "No skirt") \
    X(SumNoBrim, "Apagado", "Off") \
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
    X(SumMaxTemps, "Boquilla %s · cama %s", "Nozzle %s · bed %s") \
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
    X(SeamGap, "Hueco en la costura", "Seam gap") \
    X(OuterWidth, "Ancho de la pared externa", "Outer wall width") \
    X(SpreadCount, "Cordones que absorben la diferencia", "Beads that take the difference") \
    X(TransitionLength, "Largo de transición", "Transition length") \
    X(TransitionAngle, "Ángulo de las transiciones", "Transition angle") \
    X(TransitionMargin, "Margen de transición", "Transition margin") \
    X(FirstLineWidth, "Ancho en la primera capa", "First layer line width") \
    X(TopWidth, "Ancho de la capa de arriba", "Top surface width") \
    X(SolidWidth, "Ancho del relleno sólido", "Solid infill width") \
    X(InfillWidth, "Ancho del relleno", "Infill width") \
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
    X(SupportBottomZGap, "Separación sobre la pieza", "Gap over the part") \
    X(SupportBuildplateOnly, "Solo desde la cama", "From the bed only") \
    X(SupportUnderBridges, "Soporte bajo los puentes", "Support under bridges") \
    X(NotSet, "Sin dato", "Not set") \
    X(AutoValue, "Automático", "Automatic") \
    X(NoLimit, "Sin límite", "No limit") \
    X(SetupPrinter, "Para empezar, elegí tu impresora (paso 1 de 2; se puede cambiar después).", "To start, choose your printer (step 1 of 2; it can be changed later).") \
    X(SetupFilament, "Ahora, el filamento que vas a usar (paso 2 de 2).", "Now the filament you will use (step 2 of 2).") \
    X(LegendLess, "Menos detalle", "Less detail") \
    X(LegendMore, "Más detalle: tiempos, filamento y totales", "More detail: times, filament and totals") \
    X(ValueMax, "Máximo: %s", "Maximum: %s") \
    X(ValueMin, "Mínimo: %s", "Minimum: %s") \
    X(SupportBottomInterface, "Capas de interfaz sobre la pieza", "Interface layers over the part") \
    X(SupportXyGap, "Separación en XY", "XY gap") \
    X(SupportSpacing, "Espaciado", "Spacing") \
    X(SupportLineAngle, "Ángulo de las líneas", "Line angle") \
    X(SupportPatternRow, "Patrón de la base", "Base pattern") \
    X(SupportEnforceLayers, "Capas con soporte forzado", "Layers of enforced support") \
    X(SupportSheath, "Vaina", "Sheath") \
    X(SupportInterfacePatternRow, "Patrón de la interfaz", "Interface pattern") \
    X(SupportInterfaceSpacing, "Separación de la interfaz", "Interface spacing") \
    X(SupportContactLoops, "Lazos de contacto", "Contact loops") \
    X(PatternLines, "Líneas", "Lines") \
    X(PatternGrid, "Grilla", "Grid") \
    X(PatternAutomatic, "Automático", "Automatic") \
    X(SupportInterface, "Capas de interfaz", "Interface layers") \
    X(SupportStyle, "Estilo", "Style") \
    X(SupportGrid, "Grilla", "Grid") \
    X(SupportSnug, "Ajustado", "Snug") \
    X(SupportClosingRadius, "Radio de cierre", "Closing radius") \
    X(SupportFreeBridge, "Puente sin soporte", "Free bridge length") \
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
    X(FullFanLayer, "Ventilador completo desde la capa", "Full fan from layer") \
    X(CoolingEnabled, "Enfriamiento automático", "Automatic cooling") \
    X(MinSpeed, "Velocidad mínima", "Minimum speed") \
    X(PressureAdvance, "Avance de presión", "Pressure advance") \
    X(FilamentMaxFlow, "Flujo máximo", "Maximum flow") \
    X(SpeedBridges, "Puentes", "Bridges") \
    X(SpeedOverBridges, "Sobre los puentes", "Over bridges") \
    X(SpeedSmallLoops, "Perímetros chicos", "Small loops") \
    X(SpeedSupportInterface, "Interfaz del soporte", "Support interface") \
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
    X(SubOverhangFan, "Ventilador en voladizos", "Overhang fan") \
    X(SlowdownLogicRow, "Cómo frenar", "How to slow down") \
    X(SlowdownUniform, "Todo por igual", "Everything alike") \
    X(SlowdownConsistentSurface, "Superficie pareja", "Consistent surface") \
    X(TransitionDistance, "Final de capa a su velocidad", "Layer end at its speed") \
    X(DynamicOverhangFan, "Ventilador según el apoyo", "Fan by support") \
    X(OverhangFan0, "Ventilador sin apoyo", "Fan unsupported") \
    X(OverhangFan1, "Ventilador apoyado un cuarto", "Fan a quarter supported") \
    X(OverhangFan2, "Ventilador apoyado la mitad", "Fan half supported") \
    X(OverhangFan3, "Ventilador apoyado tres cuartos", "Fan three quarters supported") \
    X(FlowSlopeUp, "Subida del caudal", "Flow ramp up") \
    X(FlowSlopeDown, "Bajada del caudal", "Flow ramp down") \
    X(SubGcodeTemplates, "Plantillas de G-code", "G-code templates") \
    X(SubGcodeHeights, "G-code por altura", "G-code by height") \
    X(CornerSmoothing, "Esquinas redondeadas", "Corner smoothing") \
    X(BottomPattern, "Patrón de abajo", "Bottom pattern") \
    X(BridgeAngle, "Dirección de los puentes", "Bridge direction") \
    X(InfillBeforeWalls, "Relleno antes que las paredes", "Infill before walls") \
    X(MaxLink, "Unión máxima del relleno", "Maximum infill link") \
    X(ZOffset, "Desfase de Z", "Z offset") \
    X(VolumetricE, "E volumétrica", "Volumetric E") \
    X(VerboseComments, "Comentarios detallados", "Verbose comments") \
    X(GcodeArcs, "Arcos G2/G3", "G2/G3 arcs") \
    X(GcodeBinary, "G-code binario (.bgcode)", "Binary G-code (.bgcode)") \
    X(FlavorNoExtrusion, "Sin extrusión (solo movimientos)", "No extrusion (moves only)") \
    X(SubJerk, "Tirones y esquinas", "Jerk and corners") \
    X(SkirtMinFilament, "Filamento mínimo", "Minimum filament") \
    X(AutoTemperatures, "Temperaturas automáticas", "Automatic temperatures") \
    X(SubOutputFile, "Archivo de salida", "Output file") \
    X(FilenameFormat, "Nombre del archivo", "File name") \
    X(PostProcessRow, "Scripts de posproceso", "Post-processing scripts") \
    X(FilamentStartGcode, "G-code de inicio del filamento", "Filament start G-code") \
    X(FilamentEndGcode, "G-code de fin del filamento", "Filament end G-code") \
    X(DraftShieldRow, "Escudo contra corrientes", "Draft shield") \
    X(ShieldDisabled, "Apagado", "Off") \
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
    X(StrictParserRow, "Lector del firmware", "Firmware's reader") \
    X(StrictParserNone, "Lee cualquier G-code", "Reads any G-code") \
    X(StrictParserAnycubic, "Anycubic (Kobra 3, S1, X)", "Anycubic (Kobra 3, S1, X)") \
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
    X(ObjectInfill, "Relleno", "Infill") \
    X(ObjectWalls, "Capas y perímetros", "Layers and perimeters") \
    X(ObjectSurfaces, "Capas sólidas", "Solid layers") \
    X(Objects, "Objetos", "Objects") \
    X(ObjectsEmpty, "La placa no tiene piezas.", "The plate has no objects.") \
    X(ObjectsHint, "Clic derecho sobre una pieza para darle ajustes propios, modificadores, volúmenes negativos o rangos de alturas.", "Right-click an object to give it settings of its own, modifiers, negative volumes or height ranges.") \
    X(ObjectsPicked, "Ajustes de la pieza a cambiar", "Object settings to modify") \
    X(ObjectAddSetting, "Agregar ajuste", "Add setting") \
    X(ObjectSettingsOpen, "Ajustes de la pieza…", "Object settings…") \
    X(ObjectSettingsTitle, "Ajustes de %s", "Settings of %s") \
    X(ObjectSettingsHint, "Estos ajustes valen solo para esta pieza; el resto sale del perfil.", "These settings hold for this object only; the rest comes from the profile.") \
    X(ObjectSettingsNone, "La pieza no tiene ajustes propios.", "The object has no settings of its own.") \
    X(ObjectSettingsBasic, "Los ajustes por pieza se ponen en los niveles Normal y Avanzado.", "Settings per object are set at the Normal and Advanced levels.") \
    X(ObjectSettingsRemove, "Quitar de la pieza", "Remove from the object") \
    X(ObjectOwnSettings, "Ajustes propios: %d", "Own settings: %d") \
    X(AddModifier, "Agregar modificador", "Add modifier") \
    X(AddNegativeVolume, "Agregar volumen negativo", "Add negative volume") \
    X(AddHeightRange, "Agregar rango de alturas", "Add height range") \
    X(ModifierItem, "Modificador %d", "Modifier %d") \
    X(NegativeVolumeItem, "Volumen negativo %d", "Negative volume %d") \
    X(RangeItem, "Alturas de %.2f a %.2f mm", "Heights from %.2f to %.2f mm") \
    X(HeightFrom, "desde", "from") \
    X(HeightTo, "hasta", "to") \
    X(ShapeBox, "Caja", "Box") \
    X(ShapeCylinder, "Cilindro", "Cylinder") \
    X(RemoveVolume, "Quitar", "Remove") \
    X(VolumeOffset, "Lugar (mm)", "Place (mm)") \
    X(VolumeSize, "Tamaño (mm)", "Size (mm)") \
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
    X(CurvedFitHead, "Ajustar al cabezal", "Fit to the print head") \
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
    X(HeadProfile, "Perfil del cabezal", "Print head profile") \
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
    X(FlowBySpeedRow, "Flujo según la velocidad", "Flow by speed") \
    X(BeadSectionRow, "Sección del cordón", "Bead section") \
    X(BeadSectionRounded, "Redondeada", "Rounded") \
    X(BeadSectionRectangle, "Rectangular", "Rectangle") \
    X(ToolchangeDelay, "Espera después de descargar", "Wait after unloading") \
    X(LoadTime, "Tiempo de carga", "Load time") \
    X(UnloadTime, "Tiempo de descarga", "Unload time") \
    X(SingleExtruderMmu, "Un extrusor con varios filamentos (MMU)", "One extruder, several filaments (MMU)") \
    X(HighFlowNozzle, "Boquilla de alto caudal", "High-flow nozzle") \
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
    X(SumSpacingFlow, "Separación %s %% · flujo %s %%", "Spacing %s %% · flow %s %%") \
    X(SumDensity, "%s g/cm³", "%s g/cm³") \
    X(ColorChangeGcode, "Cambio de color", "Colour change") \
    X(PauseGcode, "Pausa", "Pause") \
    X(CustomTemplateGcode, "G-code propio por altura", "Custom G-code by height") \
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
    X(AccelWipeTower, "Torre de purga", "Wipe tower") \
    X(AccelFirstLayer, "Primera capa", "First layer") \
    X(AccelTravel, "Viajes", "Travels") \
    X(AccelShortTravel, "Viajes cortos", "Short travels") \
    X(RetractMinTravel, "Viaje mínimo para retraer", "Shortest travel to retract") \
    X(UnretractSpeed, "Velocidad de la vuelta", "Unretract speed") \
    X(RestartExtra, "Extra al volver", "Extra on restart") \
    X(RetractLayerChange, "Retraer al cambiar de capa", "Retract on layer change") \
    X(WipeRow, "Barrer al retraer", "Wipe while retracting") \
    X(BeforeWipe, "Retracción antes del barrido", "Retraction before the wipe") \
    X(AfterWipe, "Retracción después del barrido", "Retraction after the wipe") \
    X(WipeInward, "Barrer hacia adentro", "Wipe inward") \
    X(WipeInwardDistance, "Cuánto se corre el barrido", "How far the wipe moves in") \
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
    X(WallsOuterFirst, "Pared externa primero", "Outer wall first") \
    X(SubWallsMore, "Más opciones de paredes", "More wall options") \
    X(SubWallsThin, "Paredes finas", "Thin walls") \
    X(StaggeredSeams, "Costuras internas escalonadas", "Staggered inner seams") \
    X(TopOneWallRow, "Un solo perímetro arriba", "One wall on top") \
    X(TopOneWallNone, "No", "No") \
    X(TopOneWallTop, "En toda superficie de arriba", "On every top surface") \
    X(TopOneWallTopmost, "Solo en la de más arriba", "Only on the topmost") \
    X(FirstLayerOneWall, "Un solo perímetro en la primera capa", "One wall on the first layer") \
    X(ExtraIfNeeded, "Perímetros extra si hacen falta", "Extra walls if needed") \
    X(AvoidCurledOverhangs, "Esquivar voladizos levantados", "Avoid curled overhangs") \
    X(ExtraOnOverhangs, "Perímetros extra en voladizos", "Extra perimeters on overhangs") \
    X(SumWallsMore, "%d de 7 activas", "%d of 7 on") \
    X(SumWallsThin, "%d cordones reparten el cambio de ancho", "%d beads share the change in width") \
    X(SumWallsThinOne, "1 cordón toma el cambio de ancho", "1 bead takes the change in width") \
    X(WallsStaggered, "Perímetros escalonados", "Staggered perimeters") \
    X(SubMaterialRetraction, "Valores propios del material", "The material's own values") \
    X(SubCoast, "Coasting", "Coasting") \
    X(Coast, "Final sin extruir", "End without extruding") \
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

#include <cstdarg>
#include <optional>
#include <string>
#include <string_view>

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

// printf for the interface: as vsnprintf, with each float's decimal point
// the language's, a comma in Spanish (the spec team's TR1). Other
// conversions are as printf writes them.
std::string vformat(const char* fmt, std::va_list args);
std::string format(const char* fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

// A whole number with the language's thousands separator: 4 013 in
// Spanish, 4,013 in English (TR2).
std::string grouped(unsigned long long n);

// A number written by printf ("0.20") with the language's decimal point.
std::string localized(std::string number);

// The engine's own messages (a file that does not load, bad settings) in
// the language: known ones by their pattern, a path or a "3MF, line 4:"
// before them kept or translated, the rest as they are (the spec team's
// TR21: the open-file dialog mixed languages).
std::string translated_error(std::string_view message);

// A number typed in a field, with ',' or '.' for the decimal point and
// spaces around it; empty when it is not one number.
std::optional<double> typed_number(std::string_view text);

} // namespace tisma::ui
