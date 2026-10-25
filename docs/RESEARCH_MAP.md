# Mapa de investigación: FDM y técnicas de laminado

Pedido del dueño (2026-10-25): «papers sobre FDM y tecnologías del slicer para mejorar lo más posible las funciones,
calidades y rendimientos».

Este documento junta lo publicado que el motor todavía no usa, por área, y propone qué hacer primero. No repite los
temas que ya tienen su propio documento:

| Tema | Documento |
|---|---|
| Rellenos | `docs/INFILL.md` |
| Relleno no planar | `docs/NONPLANAR_INFILL.md` |
| Laminado no planar | `docs/NONPLANAR_SLICING.md` |
| Soportes | `docs/SUPPORT_RESEARCH.md` |
| Enfriamiento | `docs/COOLING_SIMULATION.md` |
| Z anti-aliasing | `docs/ZAA.md` |
| Altura de capa | `docs/LOCAL_LAYERS.md` y `docs/VARIABLE_LAYERS.md` |

**Sala limpia:**
- se usaron solo resúmenes, artículos abiertos, páginas de editoriales, registros de patentes y documentación de
  usuario;
- ningún código, ni el que publican los artículos.

**Calidad de los datos:** varios números salen del resumen del artículo o de otro trabajo que lo cita; donde importa,
se dice. Las referencias que no se pudieron verificar llevan «(sin verificar)». Antes de implementar algo, se lee el
artículo completo.

## 1. Trayectorias y calidad de superficie

| Trabajo | Qué hace | Resultado | Para el motor |
|---|---|---|---|
| Ermolai, Sover, Irimia, *Innovations in Mechanical Engineering IV*, Springer, 2025, [DOI](https://doi.org/10.1007/978-3-031-93554-1_4) | Estudio Taguchi de la costura en bisel (velocidad, pasos, altura de inicio, largo) contra la costura común | La costura se esconde mucho mejor con los parámetros ajustados | La costura en bisel de la ficha `walls_extra.md` |
| Grübel et al., *Materials & Design* 266:115980, 2026, [DOI](https://doi.org/10.1016/j.matdes.2026.115980) | Tomografía de piezas de PLA con la costura en distintos lugares | Los vacíos y la mala unión se alinean con la costura: es un punto débil | El costo de la costura tiene que mirar también la tensión |
| Zhang, Le, Panotopoulou, Whiting, Wang, *ACM TOG* 34(6):215, 2015, [DOI](https://doi.org/10.1145/2816795.2818121) | Un puntaje aprendido de lo que se ve (saliencia, vista preferida) | Elige la orientación que esconde las marcas | Los mismos términos sirven para elegir dónde va la costura. No encontré un artículo que lo haga |
| Comminal, Serdeczny, Pedersen, Spangenberg, *Additive Manufacturing* 29:100753, 2019, [DOI](https://doi.org/10.1016/j.addma.2019.06.005) | Simulación del cordón en esquinas con perfiles de velocidad trapezoidales | Hay un redondeo óptimo de la esquina. Con flujo constante mientras frena, sobra material; con el flujo proporcional a la velocidad, el ancho queda parejo | El flujo de cada tramo según la velocidad que planea el estimador |
| Lin, Guidetti, Nagel, Balta, Lygeros, arXiv 2512.24905, 2025 (preimpresión) | Dos piezas fotografiadas con un teléfono identifican la dinámica de extrusión, y un control óptimo reescribe el G-code | Error de ancho en las esquinas −46 % y Ra −37 a −51 %. A 60 mm/s, la calidad de imprimir a 27 mm/s (2,25 veces más rápido) | Una calibración del lado del laminador |
| Hornus, Kuipers et al., *ACM TOG* 39(4):131, 2020, [DOI](https://doi.org/10.1145/3386569.3392448) | Cordones cerrados de ancho variable dentro de límites, sobre el eje medio | Llenado más denso, sin solapes | Alternativa para relleno de huecos y paredes finas |
| Armillotta, Cavallaro, *Rapid Prototyping J.* 23(6), 2017, [DOI](https://doi.org/10.1108/RPJ-02-2016-0020); Armillotta, *RPJ* 25(3), 2019, [DOI](https://doi.org/10.1108/RPJ-06-2018-0151) | Mapa y simulación de los defectos en los bordes según la altura de capa y tres ángulos | Frecuencia, tamaño y forma del defecto | Avisos en la vista previa |
| Jiang, Stringer, Xu, Zhong, *Int. J. Computer Integrated Manufacturing* 31(10), 2018, [DOI](https://doi.org/10.1080/0951192X.2018.1466398) | Voladizos de 20° a 50° impresos | El ángulo que sale bien cambia bastante con el ventilador, la velocidad y la temperatura | El umbral del soporte según esos ajustes, no fijo en 45° |
| He, Zhu, Zhang, Yang, *Polymers* 18(2):261, 2026, [DOI](https://doi.org/10.3390/polym18020261) | Puentes de 60 a 140 mm | El ancho crítico de pandeo crece de 1,2 a 4,3 mm con la luz. La velocidad pesa poco. Falla por la tensión térmica, no por el peso | Puentes con un ancho múltiplo entero del de la línea, y anclajes más grandes |
| Boschetto, Bottini, *Robotics and CIM* 37:103–114, 2016, [DOI](https://doi.org/10.1016/j.rcim.2015.07.005) | Un desplazamiento distinto por cara, según la altura de capa y el ángulo de la cara | Piezas muy cerca de la medida, sin pieza de prueba | Compensación en XY que depende del ángulo |
| Wang, Song, Huang, Tsung, *IEEE T-ASE* 14(2), 2017, [DOI](https://doi.org/10.1109/TASE.2016.2544941); Yaman, *IJAMT* 94, 2018, [DOI](https://doi.org/10.1007/s00170-017-1018-2); Koers, Magyar, *Macromol. Symp.* 411, 2023, [DOI](https://doi.org/10.1002/masy.202200185) | Modelos de la deformación en el plano y de la contracción, invertidos para corregir el contorno; agujeros; el «trébol» en los cilindros | Contornos corregidos | Sumar a la compensación que ya existe (`docs/CALIBRATION.md`) |
| Ahn, Kweon, Kwon, Song, Lee, *J. Mat. Proc. Tech.* 209, 2009, [DOI](https://doi.org/10.1016/j.jmatprotec.2009.05.016); Buj-Corral et al., *Materials* 12(23):3834, 2019, [DOI](https://doi.org/10.3390/ma12233834) | Ra según el ángulo de la cara | Coincide con lo medido hasta 80° | Un mapa de rugosidad por cara: vista previa, altura de capa y orientación |
| Sardinha et al., *Mat. Design & Proc. Comm.* 3(2):e151, 2020, [DOI](https://doi.org/10.1002/mdp2.151); Alzyod, Takács, Ficzere, *J. Reinf. Plast. Compos.* 43, 2024, [DOI](https://doi.org/10.1177/07316844231173059) | Planchado | Ra hasta −60 % en ABS (según una revisión). En PLA pesan la velocidad, la separación y el flujo | El planchado de la ficha `ironing.md` |

## 2. Movimiento, extrusión y tiempo

| Trabajo | Qué hace | Resultado | Para el motor |
|---|---|---|---|
| Medina-Sanchez et al., *Materials* 12(23):3982, 2019, [DOI](https://doi.org/10.3390/ma12233982) | Superficie de velocidad media según el largo del tramo y el giro | Error máximo menor al 8,5 % en 3 impresoras; Pronterface, hasta 48 %; largo / velocidad, hasta 59 % | Calibrar el estimador de tiempo contra impresiones medidas |
| Komineas et al., *Procedia Manufacturing* 21, 2018 (sin verificar) | Modelo analítico con aceleración y frenado | Error de 1,7 % contra 7,2 % del programa comercial (solo del resumen) | Igual |
| Documentación de Klipper (cinemática y compensación de resonancias) | El suavizado del modelador crece con la aceleración | — | Aceleración por tipo de tramo (más baja en el perímetro de afuera) y el estimador que la sigue |
| Luo, Wang, Migler, Seppala, *Additive Manufacturing* 32:101019, 2020, [DOI](https://doi.org/10.1016/j.addma.2019.101019); Edwards et al., *IMA J. Appl. Math.* 84, 2019 | El flujo máximo es el número de Péclet en el que el borde entre sólido y fundido llega a la punta | Coincide con lo medido bien por encima de Tg | Tope de flujo volumétrico según la temperatura y el material |
| Go, Hart, *Additive Manufacturing* 18:276–284, 2017 ([arXiv 1709.05918](https://arxiv.org/abs/1709.05918)) | Impresora de escritorio rápida | 127 cm³/h, 7 veces una comercial. Con menos de 1 s de enfriamiento por capa hubo que frenar el relleno; con aceleraciones altas, las esquinas de afuera se pasan y las de adentro sobran | Topes de velocidad por fusión y por enfriamiento |
| Tronvoll, Popp, Elverum, Welo, *Rapid Prototyping J.* 25(5), 2019, [DOI](https://doi.org/10.1108/RPJ-10-2018-0275) (CC BY) | Modelo de primer orden del avance de presión | La constante depende de la altura de capa | Avance de presión por tipo de tramo y altura |
| Wu, Ramani, Okwudire, *Additive Manufacturing* 48:102389, 2021, [DOI](https://doi.org/10.1016/j.addma.2021.102389); Wu, Qian, Okwudire, *AM* 78:103850, 2023 | Compensación anticipada de la extrusión con un modelo; también retracción y vuelta | — | Después de revisar sus patentes |
| Singer, Seering, *J. Dyn. Sys. Meas. Control* 112(1), 1990 | Modelado de la entrada (input shaping) | Vibración residual 25 veces menor | Va en el firmware. El laminador ayuda con la aceleración por tramo. Patentes vencidas |
| Pham, Pham, *IEEE T-RO* 34(3), 2018 ([arXiv 1707.07239](https://arxiv.org/abs/1707.07239)) | Velocidad óptima en tiempo sobre un camino fijo, con programas lineales chicos | Más rápido que la integración numérica, 100 % de éxito | Para 4 y 5 ejes (límites de cada eje) |
| Fok, Cheng, Ganganath, Iu, Tse, *IEEE TII* 15(4), 2019, [DOI](https://doi.org/10.1109/TII.2018.2889740) | El orden de la capa como problema del cartero rural, con colonia de hormigas | Mejor tiempo y calidad que los resolvedores comunes | La formulación; la colonia de hormigas es lenta para millones de tramos |

## 3. Resistencia, deformación y materiales

| Trabajo | Qué hace | Resultado | Para el motor |
|---|---|---|---|
| Allum, Moetazedian, Gleadall, Silberschmidt, *Additive Manufacturing* 34:101297, 2020, [DOI](https://doi.org/10.1016/j.addma.2020.101297) | La unión entre capas en PLA | **Llega a la resistencia del material.** Lo débil entre capas es la ranura entre cordones, no la unión. Temperatura (60 °C de rango), velocidad (16×) y tiempo de capa (8×) no cambiaron nada dentro de la dispersión | En PLA manda la geometría; el término térmico de T3 pesa más en ABS y otros (ver abajo) |
| Allum et al., *Additive Manufacturing* 61:103334, 2023, [DOI](https://doi.org/10.1016/j.addma.2022.103334) (CC BY) | Cordones de al menos 2,5 veces la boquilla: uno de 1,2 mm en vez de tres de 0,4 | Contacto entre capas del 63 % al 90 %; resistencia, deformación a la rotura y tenacidad +40 a +48 %; −67 % de tiempo. En dos casos reales, más del doble de carga | **El cambio más barato con más ganancia medida** |
| Kuznetsov et al., *Polymers* 10(3):313, 2018, [DOI](https://doi.org/10.3390/polym10030313) | Geometría del proceso en PLA | Boquilla de 0,8 con capas de 0,15 da la mejor resistencia entre capas | Igual que lo anterior: poca altura de capa contra el ancho |
| Coogan, Kazmer, *Rapid Prototyping J.* 23(2), 2017, [DOI](https://doi.org/10.1108/RPJ-03-2016-0050); Seppälä et al., *Soft Matter* 13, 2017, [DOI](https://doi.org/10.1039/C7SM00950J) | Unión en ABS; tiempo de soldadura isotérmico equivalente | En ABS, la unión entre capas va del 40 al 85 % del filamento según las condiciones | El criterio de unión de T2 y T3, por material |
| Tam, Mueller, *3D Printing and AM* 4(2), 2017, [DOI](https://doi.org/10.1089/3dp.2017.0001); Sales, Kwok, Chen, *J. Manuf. Processes* 64, 2021 | Cordones planos por las líneas de tensión principal | Cerca de +100 % de resistencia (según otro trabajo); +50 a +55 % de carga con el mismo material (según una tesis) | Ángulo de los cordones por región según el análisis propio (A1) |
| Li et al., *CIRP Annals* 70(1), 2021, [DOI](https://doi.org/10.1016/j.cirp.2021.04.037); Chen et al., *J. Manuf. Sci. Eng.*, cerca de 2025, [DOI](https://doi.org/10.1115/1.4072228) (sin verificar el volumen) | Regiones con un ángulo cada una; la dirección dentro de ±30° de la principal | +35 % en compresión en PLA; ±30° alcanza | Justifica regiones con ángulos gruesos en vez de curvas exactas |
| Casavola et al., *Materials & Design* 90, 2016, [DOI](https://doi.org/10.1016/j.matdes.2015.11.009); Garzon-Hernandez et al., *Materials & Design* 188, 2020, [DOI](https://doi.org/10.1016/j.matdes.2019.108414) | Teoría de laminados con los ángulos de cada capa; modelo térmico y de cuello que da la mesoestructura | Rigidez ortótropa a partir de las trayectorias | El análisis «como impresa» (N3) con las propiedades de las trayectorias reales |
| Kuipers, Su, Wu, Wang, *Additive Manufacturing* 50:102495, 2022, [DOI](https://doi.org/10.1016/j.addma.2021.102495) | Vigas de los dos materiales que se traban en la interfaz, con extrusión continua (ITIL) | 82 % del máximo teórico (PP con PLA), como una cola de milano | Interfaz entre materiales (con MM1). Publicación defensiva de 2020 |
| Duty et al., *Additive Manufacturing* 27, 2019 | Pernos verticales que cruzan muchas capas | Más de 3,5 veces en Z para PLA | N4, con la patente de ORNL sin revisar |
| Xu, Xu, Zhai, Jiang, *Additive Manufacturing* 89:104276, 2024, [DOI](https://doi.org/10.1016/j.addma.2024.104276) | Caminos de 3 ejes con Z modulada: cordones entrelazados | +31,9 % y +67,5 % de corte entre capas | Comparar con N1 y N2 |
| Samy et al., *Polymers* 14(13):2746, 2022, [DOI](https://doi.org/10.3390/polym14132746) | Simulación de tensiones residuales en PP | Concéntrico: −21 % de tensión y −5,5 % de alabeo; zigzag a ±45°: +37 % de alabeo | Patrón según el material en los semicristalinos |
| Armillotta, Bellotti, Cavallaro, *Robotics and CIM* 50, 2018, [DOI](https://doi.org/10.1016/j.rcim.2017.09.007); Wang, Xi, Jin, *IJAMT* 33, 2007 (el DOI difiere entre fuentes) | Modelos cerrados del alabeo según el tamaño, la altura de capa y la contracción | Predicción rápida | Aviso de riesgo de alabeo, y borde automático |
| Spörk et al., *Plastics, Rubber and Composites* 47(1), 2018, [DOI](https://doi.org/10.1080/14658011.2017.1399531) | Adherencia a la cama | Sube mucho con la cama apenas por encima de Tg | Valores por defecto del catálogo de filamentos |
| Fernandez-Vicente et al., *3D Printing and AM* 3(3), 2016, [DOI](https://doi.org/10.1089/3dp.2015.0036) | Patrón y densidad del relleno en tracción | El patrón cambia menos del 5 %; manda la densidad | No encontré un estudio serio de perímetros contra relleno con la misma masa: lo mide el banco propio |

## 4. Rendimiento del cómputo

| Trabajo | Qué hace | Resultado | Para el motor |
|---|---|---|---|
| Minetto, Volpato, Stolfi, Gregori, da Silva, *Computer-Aided Design* 92, 2017, [DOI](https://doi.org/10.1016/j.cad.2017.07.001) | Cada triángulo a la lista del primer plano que cruza, barrido con un conjunto activo | O(n + k + m) con capas parejas | El corte del motor ya reparte los triángulos por nivel (`src/slice/layers.*`); sirve para comparar |
| Mejia-Parra et al., *Mathematics* 8(9):1624, 2020, [DOI](https://doi.org/10.3390/math8091624) | Todos los casos degenerados (plano por un vértice, arista o cara), con contornos orientados | Robusto en sus pruebas | Robustez del corte, también para campos escalares (F7) |
| Barill, Dickson, Schmidt, Levin, Jacobson, *ACM TOG* 37(4):43, 2018 | Número de vueltas rápido para mallas rotas | Hasta unas 1000 veces más rápido que la suma directa | Laminar STL rotos sin repararlos antes |
| Cao, Tang, Mohamed, Tan, I3D 2010 | Transformada de distancia exacta en GPU, en tres pasadas | 8192² en 12 a 17 ms (versión posterior) | El primer núcleo raster en Vulkan: desplazamientos, rellenos por distancia, separación de soportes |
| Held, *Computational Geometry* 18(2), 2001; Karavelas, ISVD 2004 | Voronoi de segmentos: primero la topología, después los números; predicados filtrados | No falla por redondeo | La robustez del Voronoi propio |
| Wang et al., *Advances in Manufacturing* 12(2), 2024, [DOI](https://doi.org/10.1007/s40436-023-00468-8) | Sólidos y redes como campos de distancia, cortados por capa | Mucho menos tiempo y memoria que mallar | Rellenos TPMS y redes, y el laminado no planar |
| Platzman, Bartholdi, *J. ACM* 36(4), 1989; Johnson, McGeoch, 1997; Volpato et al., *J. Oper. Res. Soc.* 71(6), 2020, [DOI](https://doi.org/10.1080/01605682.2019.1590135) | Semilla por curva que llena el plano; después voraz con 2-opt y vecinos cercanos | Curva: unos 25 % sobre el óptimo; 3-opt: 3 a 4 %; inserción más 2-opt mejora al voraz en FDM | Orden de islas y viajes que escala |
| Stoll, Gumhold, Seidel, IEEE Visualization 2005 | Cada tramo como un cilindro dibujado por la GPU | — | Vista previa de G-code enormes |
| G-code binario de Prusa y MeatPack | Bloques comprimidos con suma de control; MeatPack empaqueta los caracteres comunes en 4 bits | Archivos unos 70 % más chicos; la conexión serie rinde como de 210 000 baudios | La especificación de `.bgcode` está dentro de un repositorio AGPL: decide el dueño si se puede leer, o se estudia como caja negra con la herramienta `bgcode` |
| Deneault et al., *MRS Bulletin* 46, 2021, [DOI](https://doi.org/10.1557/s43577-021-00051-1) | Optimización bayesiana de 4 parámetros con fotos | El objetivo en menos de 100 impresiones | Un asistente de calibración |
| Brion, Pattinson, *Nature Communications* 13, 2022, [DOI](https://doi.org/10.1038/s41467-022-31985-y) | Una red neuronal corrige flujo, velocidad, Z y temperatura con una cámara en la boquilla | Corrección en tiempo real; datos públicos | Va en la impresora o el host (P2), no en el motor |

## 5. Qué hacer primero

Ordenado por ganancia medida contra esfuerzo, con lo que el motor ya tiene:

| # | Qué | Por qué | Esfuerzo | Encaja en |
|---|---|---|---|---|
| Q1 | **Modo resistencia con cordones anchos**: perímetros y relleno de 2,5 veces la boquilla o más, con poca altura de capa contra el ancho, y el ancho de contacto entre capas en el análisis | +40 a +48 % entre capas y −67 % de tiempo (Allum et al. 2023) | Chico: el motor ya hace anchos variables | Perfiles y A1 |
| Q2 | **Estimador de tiempo que imita al firmware**: aceleración por tipo de tramo, la velocidad de esquina de Klipper y Marlin según su documentación, y calibración contra impresiones medidas | De 48 % a menos de 8,5 % de error (Medina-Sanchez et al. 2019). Además, es la función de costo de todo lo demás | Chico: ya hay perfiles trapezoidales | El pedido del tiempo de impresión |
| Q3 | **Topes por fusión y enfriamiento**: flujo volumétrico máximo según la temperatura y el material, y cuánto puede cambiar el flujo | Evita la falta de extrusión a alta velocidad (Luo et al. 2020; Go y Hart 2017) | Chico | Catálogo de filamentos y escritor |
| Q4 | **Flujo según la velocidad planeada en las esquinas**, y avance de presión por tipo de tramo | Esquinas parejas (Comminal et al. 2019; Tronvoll et al. 2019) | Medio | Escritor |
| Q5 | **Cordones alineados con la tensión** por región, con el análisis propio, y propiedades ortótropas según las trayectorias en el análisis | +35 % o más en piezas cargadas, y un análisis que mide lo que se imprime | Medio | A1, A2 y N3 |
| Q6 | **Costura**: costo por vértice (esquina cóncava, atrás, lo que se ve, tensión) y costura en bisel de la ficha | Superficie más limpia y sin punto débil donde carga | Chico a medio | Escritor y `walls_extra.md` |
| Q7 | **Mapa de rugosidad por cara y compensación en XY según el ángulo** | Vista previa de calidad, altura de capa y orientación; piezas a medida | Chico a medio | Vista previa y compensación |
| Q8 | **Riesgo de alabeo** con un modelo cerrado, borde automático y patrón según el material | Menos piezas perdidas | Chico | Revisión antes de imprimir |
| Q9 | **Orden de islas y viajes que escala**: semilla por curva y 2-opt con vecinos | Menos viajes y retracciones en capas con miles de islas | Medio | Escritor |
| Q10 | **Umbral de voladizo según ventilador, velocidad y temperatura**, y puentes con anchos enteros de línea | Menos soporte y puentes que no se doblan | Chico | F5 y S1 |
| Q11 | **Rendimiento:** transformada de distancia exacta en Vulkan como primer núcleo raster, número de vueltas para mallas rotas, y robustez del Voronoi propio | Velocidad y laminar lo que hoy falla | Medio | GPU y F2 |
| Q12 | **Interfaz trabada entre materiales** (ITIL) | Uniones fuertes entre materiales distintos | Medio | MM1 |

**Lo que cambia en otros documentos:**
- **T3** (`docs/COOLING_SIMULATION.md`): en PLA la unión entre capas ya llega a la resistencia del material (Allum et
  al. 2020), así que el criterio «frío» importa poco en PLA y más en ABS y en los materiales técnicos (Coogan y Kazmer
  2017). El criterio de unión de T2 puede usar el tiempo de soldadura isotérmico equivalente (Seppälä et al. 2017). El
  criterio «blando» (calidad de los detalles chicos) sigue igual.
- **N2 y N3:** comparar con el entrelazado de Xu et al. 2024.

## 6. Patentes a evitar o revisar

Ninguna es una opinión legal; las fechas de vencimiento son estimadas. Todo pasa por el abogado antes de vender.

| Patente | Titular | Qué cubre | Qué hace el motor |
|---|---|---|---|
| US 10,585,414 B2 (y EP, CN, JP) | Universidad de Michigan; licencia a Ulendo | Compensación de vibraciones con B-splines filtradas | No la implementa. Input shaping clásico, vencido, va en el firmware |
| US 10,639,850 B2 | Universidad de Shandong | Espirales de Fermat conectadas | No las implementa |
| US 11,534,977 B2 | UT-Battelle | Tramos puente para que el recorrido sea euleriano | Revisar antes de unir el relleno en un solo recorrido |
| US 10,399,280 B1 | Tulane | Imprimir detalles locales a través de varias capas | No lo hace |
| US 10,723,079 | LLNL | Laminado directo de redes con trazado de rayos | Revisar antes de laminar redes por campos |
| US 11,150,633 B2 | Purdue y USC | Modelos de desviación con transferencia entre procesos | La compensación propia no usa transferencia |
| US 2023/0234136 A1 y US 2023/0280717 A1 | Siemens | Reordenar tramos o zonas según el calor | No reordena por el calor (`docs/COOLING_SIMULATION.md`) |
| US 11,331,848 B2 y US 12,017,407 B2 | Addman | Brick layers | Ver `docs/NONPLANAR_INFILL.md` |
| CN 122442952 A (solicitud) | — | Anchos de cordón alternados en capas pares e impares | Revisar si se hacen cordones alternados |
| US 11,309,087 | — | Cálculo de diagramas de Voronoi | Revisar antes de un Voronoi en GPU |
| Sin revisar | ORNL; grupo de Okwudire | Pernos en Z; compensación anticipada de la extrusión y velocidad optimizada | Revisar antes de N4 y antes de Q4 con modelo |
