# Calidad, trayectorias y resistencia en FDM: lo publicado de 2018 a 2026

Pedido del dueño (2026-10-26): «buscar papers de impresión 3D en general para mejorar la calidad y sumar funciones».

Este informe amplía `docs/RESEARCH_MAP.md` (2026-10-25). No repite lo que ya está ahí ni en `docs/SUPPORT_RESEARCH.md`,
`docs/COOLING_SIMULATION.md`, `docs/INFILL.md`, `docs/GLOSS.md` y los informes de `docs/spec/qa/` del 2026-10-26. Cuando
un trabajo ya citado allí importa para una conclusión, se nombra con «(en el mapa)» o con el documento que lo cita.

**Fuera de alcance:** el laminado no planar y los arcos en voladizos (los cubre otro informe).

**Sala limpia:**
- se leyeron solo resúmenes, artículos abiertos, páginas de editoriales, registros de patentes y documentación de
  usuario;
- ningún código, tampoco el que publican algunos artículos;
- los métodos se describen con palabras propias, sin fórmulas ni pasos copiados.

**Calidad de los datos:**
- «(texto)»: se leyó el artículo completo;
- «(resumen)»: el número sale del resumen o de la página de la editorial;
- «(sin verificar)»: el dato sale de una búsqueda o de otro trabajo que lo cita, y no se pudo confirmar.

Antes de implementar algo, se lee el artículo completo.

## Resumen

1. **El planchado es lo que más mejora la superficie de arriba y el motor no lo genera.** Los estudios nuevos miden
   Ra entre −60 % y −83 % en PLA. El motor solo lo reconoce en la vista previa.
2. **El ventilador decide entre forma y resistencia.** Con más aire, mejor geometría pero menos resistencia: hasta 4
   veces de diferencia en tracción en un estudio, −40 % entre capas en otro. Esto choca con Allum et al. 2020 (en el
   mapa), que no vio efecto térmico en PLA: hay que medirlo con el banco propio.
3. **El flujo según la velocidad planeada sigue siendo la mejora de esquinas con más evidencia.** Un trabajo nuevo
   rehace solo el avance del filamento, sin tocar el movimiento, y baja el error de ancho a la mitad.
4. **Los bordes entre zonas de distinta densidad son puntos débiles.** Imprimir los cordones vecinos uno detrás del
   otro sube la deformación a la rotura entre un 26 % y un 33 %. Afecta al relleno guiado por tensiones del motor.
5. **El modelo de volumen del cordón del motor (rectángulo) sobrestima el material de los cordones sueltos** entre un
   11 % y un 27 % contra la forma medida (rectángulo con los extremos redondeados). Coincide con la diferencia de 2 contra
   1,79 de la prueba P11.
6. **Patentes nuevas a revisar.** La más urgente toca algo que el motor ya tiene: el análisis «como impresa» gira el
   tensor del material según la dirección de cada cordón, y eso parece estar reivindicado por una patente de WARF
   vigente hasta cerca de 2036. También: la familia de Stratasys sobre la costura (hasta 2029–2031) y la de Autodesk
   sobre el borde de ancho variable (hasta 2038). Ninguna de estas notas es una opinión legal.

## 1. Superficie

### 1.1 Planchado

| Trabajo | Qué muestra | Ganancia medida | ¿En el motor? | Cómo usarlo |
|---|---|---|---|---|
| Alzyod, Ficzere, *Scientific Reports* 14:2300, 2024 (texto) | PLA en una Ender-3 V2, capas de 0,2 mm. Diseño Box–Behnken: separación de pasadas 0,1 a 0,3 mm, velocidad 10 a 30 mm/s, flujo 10 a 20 %. Planchan **todas** las capas | Ra de 8,83 a 2,73 µm (−69 %). Óptimo: 0,136 mm, 23,5 mm/s, 17 %. Contra sin planchar: tracción +29 %, compresión +25 %, flexión +35 %, impacto +162 %. No miden el tiempo. Su tabla 3 tiene un valor que no cuadra con el texto | No: «Planchado» existe solo como tipo de la vista previa (`src/gcode/preview.hpp`). La ficha `ironing.md` está en ✔ en la cobertura | Implementar la ficha. Valores por defecto de la literatura. Ofrecer «todas las capas sólidas» como opción de resistencia, marcada como experimental hasta medirla |
| Alzyod, Ficzere, Borbas, *Discover Applied Sciences* 6:578, 2024 (resumen) | Planchado optimizado para resistencia y tiempo | Pesan la velocidad y la separación; el flujo casi no pesa en el rango probado. Óptimo: 14 % de flujo, 30 mm/s, 0,3 mm | No | Separación ancha y velocidad alta cuando lo que importa es el tiempo |
| Pricci, Percoco, *Progress in Additive Manufacturing* 10(9):6491–6506, 2025 (resumen) | Simulación de fluidos de una capa de arriba de tres cordones, validada con impresiones | Con 25 % de solape entre cordones queda porosidad (más con temperatura baja y capas altas); con 50 %, no. La separación del planchado manda en la porosidad y la velocidad en el tiempo. Bien elegidas: hasta −32 % de tiempo de fabricación y +21 % de resistencia | No | Separación y velocidad del planchado según la altura de capa, no fijas |
| Matras, *Materials* 18(22):5248, 2025 (resumen; ya en `docs/GLOSS.md`) | Primero optimiza la capa de arriba, después el planchado | Ra 5 veces menor (1,09 µm) y planitud 8 veces mejor | No | Mismo uso. Avisa de un reborde en el filo de la zona planchada: dejar un margen de medio cordón |

**Lectura:** la ganancia en Ra se repite en cuatro grupos distintos. La subida de resistencia al planchar cada capa
sale de un solo estudio: se mide antes de prometerla. El planchado también sirve para la interfaz del soporte (S2 de
`docs/SUPPORT_RESEARCH.md`).

### 1.2 Costura

| Trabajo | Qué muestra | Ganancia medida | ¿En el motor? | Cómo usarlo |
|---|---|---|---|---|
| Irimia, *Materials Research Proceedings* 46:41–48 (IManEE 2024), 2024 (resumen) | En superficies curvas la costura no se puede esconder en una esquina; ajusta los parámetros de la costura uno a uno | El resumen no da números | Costura `aligned`, `nearest`, `random`, `rear` y en bufanda | En piezas sin esquinas, la bufanda y la costura por costo (Q6) son lo que queda |
| Bazan, Turek, Kubik, *Materials* 19(15):3255, 2026 (resumen) | Dodecaedros en una Bambu Lab P1S, dos impresoras, nueve lugares de la cama, capa por capa contra pieza por pieza, costura alineada contra aleatoria | La orientación de la cara explica la mayor parte del error de medida (ΔR² = 0,776). Capa por capa da medidas más estables que pieza por pieza. La costura no explicó el error de medida. La rugosidad depende sobre todo de la geometría (ΔR² = 0,852) | La orientación automática está en la ficha `orientation.md` | La costura es un tema de aspecto, no de medida. La orientación automática puede sumar un término de precisión |
| Ermolai et al. 2025 y Grübel et al. 2026 (en el mapa) | Bufanda ajustada; la costura es un punto débil | — | Bufanda hecha (2026-10-26) | El costo de la costura mira también dónde carga la pieza (Q6) |

**Patente a revisar:** Stratasys, *Seam concealment for three-dimensional models*, US 8,349,239 B2 y sus
continuaciones US 8,974,715, US 9,724,866 y US 10,675,803 (prioridad 2009-09-23).
- La reivindicación principal, según el resumen: el punto de inicio o de fin del contorno queda **dentro** de la región
  interior de la capa.
- La descripción incluye variantes: rampa de flujo al empezar y al terminar, y solape de los extremos.
- Google Patents estima el vencimiento en 2031-02-28 para la primera y 2029-09-23 para US 8,974,715. Hubo juicios
  (Stratasys contra Afinia, 2013).
- Para el motor: no empezar ni terminar la pared externa dentro de la pieza (por ejemplo, un «barrido hacia adentro»
  que extruye) sin la revisión del abogado. La bufanda ya hecha empieza sobre el contorno, pero también conviene que
  la revise.

### 1.3 Medidas, agujeros y pieza de prueba

| Trabajo | Qué muestra | Ganancia medida | ¿En el motor? | Cómo usarlo |
|---|---|---|---|---|
| Moylan, Slotwinski, Cooke, Jurrens, Donmez, *J. Research of NIST* 119:429–459, 2014 (resumen) | Una pieza de prueba propuesta para normalizar: junta los rasgos más útiles de las anteriores para medir la capacidad de una máquina (agujeros, pernos, escalones y detalles finos, entre otros; verificar la lista en el texto) | Se imprimió en varios procesos y materiales; las medidas sirvieron para corregir un proceso | Hay piezas de calibración propias (`docs/CALIBRATION.md`), con un solo agujero de 10 mm | Una **pieza de capacidad** generada por el motor, de diseño propio con esos rasgos. Es obra del gobierno de EE. UU.; igual se usa solo la idea |
| ISO/ASTM 52902:2019, *Test artifacts — Geometric capability assessment of AM systems* | Norma con piezas de prueba y qué medir en cada una | — | No | La edición de 2019 figura como retirada en la ISO: ver la edición vigente antes de citarla. Es de pago |
| Şimşek, Yaman, ICEMS 2016 (sin verificar) | Cambiar las líneas interiores alrededor de un agujero para que tiren del perímetro hacia afuera, en vez de agrandar el agujero en el modelo | Cerca de −80 % de error en el agujero, según una página que lo resume | `compensation.hole_offset`, un solo valor | Comparar con el desplazamiento propio cuando haya agujeros de varios diámetros medidos |
| Boschetto 2016; Wang 2017; Koers 2023 (en el mapa) | Compensación por ángulo de la cara y por contracción | — | Escala por eje y agujeros | Q7 |

**Lectura:** el motor ya corrige con un valor por agujero. Con una pieza de agujeros y pernos de 2 a 20 mm, la
compensación puede depender del diámetro, que es lo que más se queja la gente (agujeros chicos que salen más chicos).
No encontré un modelo publicado y validado del error según el diámetro: lo mide el banco propio.

### 1.4 Altura de capa por lo que se ve

| Trabajo | Qué muestra | Ganancia medida | ¿En el motor? | Cómo usarlo |
|---|---|---|---|---|
| Wang, Chao, Tong, Yang, Tong, Li, Liu, Liu, *Computer Graphics Forum* 34(6):148–160, 2015 (resumen) | Altura de capa según la saliencia visual de la superficie | 30 a 40 % menos tiempo, con piezas que se ven como las de la capa más fina | Altura por pendiente, por zona y por lista (`docs/VARIABLE_LAYERS.md`). El informe de tiempo del 26-10 dice que nunca sube por encima de `layers.height` | Un término de saliencia en la altura variable, junto con capas más gruesas donde la pared es vertical |

### 1.5 Texturas y piel difusa

- No encontré ningún estudio revisado que mida la rugosidad o el agarre de la piel difusa. Todo lo publicado son
  guías de fabricantes.
- Hay trabajos de texturas por trayectoria, por ejemplo Takahashi y Miyashita, *Expressive Fused Deposition Modeling
  by Controlling Extruder Height and Extrusion Amount*, CHI 2017: cambian la altura de la boquilla y el flujo para
  hacer texturas táctiles. No dan una ganancia de calidad medible.
- El motor ya tiene la piel difusa (`fuzzy_skin.md`). No hace falta más por ahora.

## 2. Trayectorias y flujo

### 2.1 Forma y volumen del cordón

| Trabajo | Qué muestra | ¿En el motor? | Cómo usarlo |
|---|---|---|---|
| Serdeczny, Comminal, Pedersen, Spangenberg, *Additive Manufacturing* 24:145–153, 2018 (resumen) | Midieron al microscopio la sección de cordones con distintas alturas de capa y velocidades: va de casi circular a un rectángulo alargado con los bordes redondeados. Un modelo de fluidos 3D lo reproduce | El motor calcula la E con ancho × alto (rectángulo) | Ver abajo |
| Comminal, Serdeczny, Pedersen, Spangenberg, *Additive Manufacturing* 20:68–76, 2018 | El modelo numérico de esa deposición, del mismo grupo | — | Referencia del modelo |
| Xu, Agassant, Pigeonneau, *Additive Manufacturing* 59:103107, 2022 (resumen, según una búsqueda) | El ancho y el alto del cordón dependen de un solo número sin dimensión que junta la boquilla, la separación con la capa de abajo y la relación entre la velocidad de salida y la de avance | — | Validar el modelo de sección |

**Qué cambia para el motor:**
- Donde los cordones se tocan (paredes juntas, superficies sólidas), ancho × alto con separación igual al ancho llena
  justo el área. Ahí el rectángulo está bien.
- Un cordón suelto, sin vecinos (relleno disperso, capas de relleno combinadas, paredes de un cordón), toma la forma
  de un rectángulo con los extremos redondeados. Con el mismo ancho, esa forma tiene menos área que el rectángulo.
- **Cuenta nuestra:** con 0,45 × 0,2 mm, el rectángulo pone un 11 % más; con una capa combinada de 0,4 mm, un 24 %
  más; con el alto igual al ancho, un 27 % más. Es la diferencia de la prueba P11 de `docs/INFILL.md` (2 contra 1,79
  de Tisma Slicer: la misma cuenta con la forma redondeada da 1,79).
- **Propuesta:** área redondeada para los cordones sin vecinos, y separación consistente donde se tocan. Se valida con
  la torre de flujo y con la pieza de capacidad.

### 2.2 Flujo según el movimiento: esquinas y cambios de velocidad

| Trabajo | Qué muestra | Ganancia medida | ¿En el motor? | Cómo usarlo |
|---|---|---|---|---|
| Fravolini, Rossi, Moretti, Senin, Ferrante, *Control Engineering Practice* 164:106511, 2025 (resumen) | Identifican un modelo lineal de la extrusión con depósitos de prueba y una medida del ancho hecha después, sin sensor en la máquina. Con él, rehacen el **avance del filamento** (no el movimiento) por optimización convexa con suavizado | Error cuadrático del ancho al 49 % del de base (−51 %), mejor que el flujo proporcional a la velocidad. Hay que volver a identificar si cambia la máquina | Avance de presión de un solo valor; el estimador ya planea perfiles trapezoidales | Q4 ampliado: la E de cada tramo según la velocidad planeada, con un modelo calibrado con una pieza de prueba |
| Mollah et al., Solid Freeform Fabrication Symposium 2022 (sin verificar: solo la ficha del repositorio de Hull) | Esquinas de 15° a 135°, PLA, extrusor Bowden y directo, simulación y ensayo | Desvíos de alrededor de 0,1 mm cerca de las esquinas | No | La escala del problema que corrige Q4 |
| Hoteit, Balestra, Mingard, Balta, Lygeros, arXiv 2512.16333, 2025 (preimpresión) | Control con realimentación de fuerza y una referencia optimizada | Error −40 %, asentamiento −84 % | — | Necesita un sensor de fuerza: no sirve sin hardware nuevo |
| Lin et al. 2025, Comminal et al. 2019, Tronvoll et al. 2019, Wu y Okwudire (en el mapa) | Calibración con dos fotos de teléfono; flujo proporcional en esquinas; avance de presión según la altura; compensación con modelo | Esquinas −46 %, Ra −37 a −51 % (Lin) | No | Igual. Las patentes del grupo de Okwudire se revisan antes de un modelo propio |

**Lectura:** dos grupos independientes (Fravolini et al. y Lin et al.) llegan a cerca de la mitad del error de ancho
sin hardware nuevo, solo con una pieza de prueba medida y una E calculada desde el movimiento planeado. El motor tiene
la mitad del trabajo hecho: el estimador de tiempo ya conoce la velocidad de cada punto.

### 2.3 Tope de flujo del fusor

| Trabajo | Qué muestra | ¿En el motor? | Cómo usarlo |
|---|---|---|---|
| Serdeczny, Comminal, Pedersen, Spangenberg, *Additive Manufacturing* 32:100997, 2020 (resumen) | Midieron la fuerza de empuje del filamento según el avance, en PLA y ABS, con varias temperaturas, boquillas y largos de fusor. Hay un régimen lineal estable y uno inestable. Más largo de fusor y más temperatura amplían el estable. Un modelo de transferencia de calor predice el avance máximo y coincide con lo medido | `machine.max_flow`, `extrusion.max_flow` y la torre de tope de flujo | Q3 ampliado: con una sola torre a una temperatura, el modelo da el tope a otras temperaturas. También sirve para G4 de `docs/GLOSS.md` |

### 2.4 Caminos continuos y orden entre zonas

| Trabajo | Qué muestra | Ganancia medida | ¿En el motor? | Cómo usarlo |
|---|---|---|---|---|
| Hutton, Bartolai, *Solid Freeform Fabrication Symposium* 2023, pp. 822–834 (texto) | Probetas de tracción de 4 mm, sin pieles, con una zona central al 20 % y los extremos al 100 %. Los laminadores comunes arman cada zona por separado y los cordones vecinos del borde se imprimen lejos en el tiempo: el primero ya se enfrió | Resistencia: 7,98–8,01 MPa con dos laminadores comerciales contra 8,6–9,6 MPa con sus estrategias, sin diferencia significativa (5 probetas). Deformación a la rotura: 1,51–1,59 % contra 1,87–2,01 %, **significativa** (+26 a +33 %). Conclusión: imprimir los cordones vecinos uno detrás del otro, aunque no sea un solo trazo, y no debilitar la geometría para forzar un trazo continuo | El relleno guiado por tensiones y el denso bajo el techo son zonas aparte, «con el mismo patrón y la misma dirección»: sus líneas ya caen sobre las mismas rectas | Cruzar cada línea del relleno por todas las bandas en un solo pase, o imprimir los cordones de un lado y del otro del borde seguidos |
| Montalti, Yamashita, Nakamura, Liverani, Takezawa, *J. Manufacturing Processes* 173:799–816, 2026 (resumen) | Relleno orientado según la carga que se curva alrededor de un agujero sin cortarse. Usa solo el laminado y la dirección de la carga, sin simulación | Tracción +12 a +20 % y deformación a la carga máxima +19 a +35 % contra el mejor relleno común; módulo +1 a 3 %. La grieta del agujero tarda más en avanzar | No: el motor cambia la densidad según la tensión, no la dirección (Q5) | Primer caso concreto de Q5: piezas cargadas con agujeros |
| Xiao, Roh, Zhu, *Applied Sciences* 11(13):6100, 2021 (resumen) | Trayectoria continua con distribución al azar dentro de la capa, para que el plano sea isótropo | Al menos +20 % en tracción contra trayectorias comunes | No | Patrón experimental, después de Q5 |

El informe de tiempo del 26-10 y la ficha `toolpath_time.md` ya piden caminos continuos para ahorrar tiempo. Estos
trabajos agregan la razón de resistencia.

### 2.5 Retracción, hilos, coasting y barrido

- **Hilos:** casi todo lo publicado son estudios de una impresora y un material. El más usable mide los hilos con
  fotos y un diseño de superficie de respuesta (*Vision-guided process optimization for stringing reduction in FDM 3D
  printing using RSM*, *Engineering Research Express*, 2025): óptimo en 198 °C, retracción a 36 mm/s, 145 mm/s y
  capas de 0,14 mm. No se generaliza.
  - Lo útil para el motor es la idea: un puntaje de hilos desde una foto de la torre de retracción, como la calibración
    por foto de Lin et al.
- **Coasting y barrido:** no encontré mediciones revisadas. Irimia 2024 dice que el coasting de más deja la costura
  con falta de material. El motor no tiene coasting ni barrido; las fichas existen (`coasting.md`,
  `retraction_travel.md`). Implementarlos por la cobertura, sin esperar una ganancia medible sobre el avance de presión.
  - El barrido «hacia adentro» toca la patente de Stratasys de 1.2 si extruye.

### 2.6 Donde no hay evidencia

| Tema | Qué se encontró | Para el motor |
|---|---|---|
| Arcos `G2`/`G3` | Ningún estudio controlado. La documentación de OrcaSlicer dice que el ajuste de arcos no es para mejorar la calidad; la de Anycubic dice que Klipper vuelve a partir los arcos en segmentos | Bajo en la lista. Sirve por el tamaño del archivo y por la cobertura (`gcode_extras.md`) |
| Orden de las paredes (afuera primero, adentro primero, adentro-afuera-adentro) | Solo documentación de laminadores y foros | Que lo decida una prueba de caja negra propia con la pieza de capacidad |
| Purga en el relleno | Una prueba de usuario (Teaching Tech, contada en Hackaday) no vio un ahorro importante ni con 50 % de relleno. Lo que más baja la purga es cambiar menos veces y pasar de claro a oscuro | La meta de MM2 («menos de la mitad del filamento tirado») conviene medirla temprano; el orden de herramientas pesa más que la purga en el relleno |

## 3. Resistencia

### 3.1 Ventilador, temperatura y unión entre capas

| Trabajo | Qué muestra | Ganancia medida | ¿En el motor? | Cómo usarlo |
|---|---|---|---|---|
| Lee, Liu, *Additive Manufacturing* 25:196–203, 2019 (resumen) | Una boquilla de aire que se mueve con el cabezal, de 0 a 5 m/s, en PLA | Más aire: mejor geometría y menos resistencia. Entre 5 y 0 m/s, la tracción cambia **4 veces**. Proponen elegir el aire según la pieza y el material | Ventilador por capa (tiempo mínimo), en puentes y apagado en las primeras capas | Ventilador por tipo de tramo: alto donde manda la forma (voladizos, puentes, detalles, pared externa), bajo en el interior de piezas que cargan |
| Baum, Copetti, COBEM 2021 (resumen) | Probetas de tracción en PLA y PETG con distinto ventilador; repetido con otro fabricante | Fluencia entre capas un 40 % menor que con el ventilador apagado | Igual | Igual |
| Gajjar, Yang, Ye, Zhang, *Progress in Additive Manufacturing* 10(2):1261–1280, 2024 (resumen) | PLA: temperatura, velocidad, altura y ángulo de los cordones | Tracción: +10 % de 200 a 220 °C, +7 % de 60 a 40 mm/s, +12 % de 0,4 a 0,2 mm, **+40 % con los cordones a 0° de la carga en vez de 90°** | El relleno gira 90° cada capa desde `infill.angle` | Ángulo de los cordones alineado con la carga (ver 3.2) |
| Allum et al. 2020 (en el mapa) | En PLA la unión entre capas llega a la resistencia del material; temperatura, velocidad y tiempo de capa no cambiaron nada | — | — | **Contradice en parte** a Lee y Liu y a Baum y Copetti. Sus probetas aíslan la unión entre capas y su ventilador no era la variable. El banco propio tiene que medir el ventilador en PLA, PETG y ABS antes de fijar valores por defecto |

**Lectura:** el ventilador es barato de controlar y su efecto medido es grande, aunque no en todos los estudios.
Encaja con T3 de `docs/COOLING_SIMULATION.md` («menos ventilador donde la unión queda fría»), que gana apoyo.

### 3.2 Ángulos de las capas

| Trabajo | Qué muestra | ¿En el motor? | Cómo usarlo |
|---|---|---|---|
| Kiendl, Gao, *Composites Part B* 180:107562, 2020 (resumen; acceso abierto, CC BY-NC-ND) | En PLA con capas que alternan 90°, la rigidez y la resistencia son casi iguales en todas las direcciones, pero la **tenacidad** no: según la dirección de la carga la pieza rompe frágil o dúctil. Proponen otra secuencia de ángulos que sube a la vez la tenacidad y la resistencia | Alternancia fija de 90° | Una lista de ángulos por capa para el relleno y las superficies sólidas (por ejemplo ±45°, 0/90 o un giro fijo por capa), elegible por pieza. Se compara con el análisis propio y con probetas. No leí qué secuencia proponen: hay que leer el texto completo antes |
| Gajjar et al. 2024 (arriba) | +40 % con los cordones paralelos a la carga | — | Opción «alinear con el eje largo de la pieza» sin análisis, como paso previo a Q5 |

### 3.3 Paredes contra relleno, y patrones

| Trabajo | Qué muestra | ¿En el motor? | Cómo usarlo |
|---|---|---|---|
| Swanepoel, Botha, Bright, *MATEC Web of Conferences* 406:07011, 2024 (resumen) | Bajar la masa sin perder resistencia: la cantidad de paredes pesa más que el patrón y el porcentaje de relleno, en tracción y flexión | 3 paredes por defecto | Responde la pregunta abierta del mapa («perímetros contra relleno»), aunque sin igualar la masa. El banco propio ya vio lo mismo en flexión (`docs/INFILL.md`) |
| Gocyk, Afshar, Abali, *Polymers* 18(13):1619, 2026 (resumen) | Flexión en tres puntos, PLA de 50 a 100 %, panal, giroide y cuerdas de Arquímedes | Panal: la mayor resistencia absoluta; la resistencia por masa tuvo su pico al 90 %. Bajar del 100 al 50 % solo ahorró hasta un 17 % de masa en sus probetas: en piezas finas pesan las paredes | Los tres patrones existen | En piezas finas, más paredes en vez de más relleno. La interfaz puede mostrarlo con el análisis propio («rigidez por gramo») |

### 3.4 Relleno homogeneizado para analizar piezas grandes

| Trabajo | Qué muestra | ¿En el motor? | Cómo usarlo |
|---|---|---|---|
| Bean, Lopez-Anido, Vel, *Applied Sciences* 12(4):2180, 2022 (resumen) | Homogeneización del giroide con un elemento de volumen representativo y condiciones periódicas; validado en compresión. El giroide es casi isótropo. Ecuaciones semiempíricas de los módulos según la densidad | El análisis «como impresa» pone los huecos del relleno en vóxeles; un cubo de 60 mm pide 27 millones de vóxeles y no se puede analizar (`docs/INFILL.md`) | Material equivalente por patrón y densidad, calculado una vez por el propio análisis sobre una celda y comparado con estas curvas. Permite analizar piezas grandes e iterar el relleno guiado más rápido |
| Bean, Vel, Lopez-Anido, *Progress in Additive Manufacturing* 10(11):9697–9716, 2025 (resumen) | Resistencia a la fluencia equivalente del giroide según la densidad, con ensayos en PETG y PLA tenaz | Igual | Resistencia equivalente para el factor de seguridad. Subestima en densidades altas |

**Patente a revisar, urgente:** ver la sección 7 (WARF, US 10,353,378 B2): toca el análisis «como impresa» que el
motor ya tiene.

## 4. Alabeo y adherencia

| Trabajo | Qué muestra | ¿En el motor? | Cómo usarlo |
|---|---|---|---|
| Ramian, Ramian, Dziob, *Materials* 14(22):7070, 2021 (resumen) | 27 cubos de ABS en seis grupos (largo, alto, área, color, temperatura de boquilla y de cama), con cámara térmica y escáner 3D | No hay aviso de alabeo | Datos para calibrar el riesgo de alabeo (Q8) en ABS |
| Wijnen, Sanders, Pearce, *Progress in Additive Manufacturing* 3(4):193–203, 2018 (sin verificar el detalle) | Modelo de la deformación de paredes finas de PLA con un gradiente de temperatura físico, validado. Aconsejan la cama a 50 °C o menos en PLA: a 90 °C cristaliza y se suelta | El catálogo de filamentos da la cama | Valor por defecto de la cama en PLA, y una regla del aviso |
| Trofimov, Le Pavic, Pautard, Therriault, Lévesque, *Additive Manufacturing* 54:102693, 2022 | Temperatura y después deformación, validadas en PLA | — | Referencia fina para comparar el aviso rápido |
| Godderidge, Tekinalp, Ozcan, Garg, Kim, ASME MSEC 2024 (resumen) | Más material en cada cordón con trayectorias fuera del plano | Más de −40 % de alabeo en PP y nylon 11 | No: es no planar, y tiene una solicitud de patente. Solo se anota |
| Armillotta 2018; Samy 2022; Spörk 2018 (en el mapa) | Modelo cerrado del alabeo; patrón según el material; cama apenas sobre Tg | — | Q8 |

**Lectura:** no encontré ningún estudio que mida cuánto baja el alabeo con el borde o con «orejas de ratón». Lo que
hay es documentación de usuarios. El aviso de riesgo y el borde automático (Q8) se apoyan en los modelos del mapa y
se calibran con el banco propio.

**Patentes:** el borde de ancho variable según la convexidad es de Autodesk (US 10,591,897 B2, hasta 2038). Las
«anclas y faldas» de PEI son de Markforged (US 12,472,695 B2). Ver la sección 7. Un borde automático de ancho parejo
no parece tocarlas, pero lo decide el abogado.

## 5. Monitoreo y calibración

| Trabajo | Qué muestra | Ganancia medida | ¿En el motor? | Cómo usarlo |
|---|---|---|---|---|
| Petsiuk, Pearce, *Additive Manufacturing* 52:102690, 2022 (resumen) | Comparan la foto de cada capa con una imagen de referencia dibujada desde el G-code, por similitud de histogramas de gradientes. No necesita datos de entrenamiento | Detecta fallas temprano; resolución de 5 a 10 % del área observada (según una búsqueda) | La vista previa ya dibuja cada capa; P2 tiene la página de la impresora | Exportar la silueta esperada de cada capa para la página de la impresora y la app (P2, P6) cuando haya cámara |
| Moylan et al. 2014 (1.3) | Pieza de capacidad | — | No | Una calibración «capacidad de la impresora» |
| Lin et al. 2025, Deneault et al. 2021, Brion y Pattinson 2022 (en el mapa) | Calibración por fotos; optimización bayesiana; corrección en tiempo real | — | — | Sin cambios |

## 6. Multimaterial

| Trabajo | Qué muestra | ¿En el motor? | Cómo usarlo |
|---|---|---|---|
| Dairabayeva, Perveen, Talamona, *Rapid Prototyping Journal* 29(11):40–52, 2023 (resumen) | PLA tenaz con TPU: cara a cara, U, T, cola de milano, encapsulado, trabado y solape. Las interfaces con restricción mecánica son mejores. Entre materiales distintos, el encapsulado dio la mayor tracción; el solape y la T, la unión más robusta | No (MM1 pendiente). La ficha `multi_extruder.md` tiene vigas de entrelazado | Probetas de referencia para medir la interfaz de MM1 |
| Ermolai et al., *Mechanics & Industry* 25:24, 2024 (resumen) | ASA con TPU, poco compatibles: cola de milano, omega y T. Las trabas macroscópicas mejoran mucho las propiedades | Igual | Igual. Junto con Kuipers et al. 2022 (en el mapa) |

## 7. Patentes nuevas a revisar

Ninguna nota es una opinión legal. Las fechas son las estimadas por Google Patents. Todo pasa por el abogado antes de
vender, como ya decide `docs/RESEARCH_MAP.md`, sección 6.

| Patente | Titular | Qué cubre (según el resumen o la primera reivindicación) | Qué toca del motor | Vence (estimado) |
|---|---|---|---|---|
| **US 10,353,378 B2** | Wisconsin Alumni Research Foundation (inventores Shapiro y Liu; apoyo de NSF y NIST) | Tomar un tensor de propiedades del material con una dirección de referencia y el modelo de trayectorias; armar la geometría de los cordones por capa; **girar el tensor para alinearlo con cada cordón** y asignarlo, para obtener un modelo de geometría y material que se analiza | **El análisis «como impresa» que ya existe** (A1: material ortótropo con la dirección de cada vóxel tomada del laminado, `src/fea/as_printed.*`), y lo que se planea en N3 y Q5 | Cerca de 2036-12-13. Tasa del año 4 pagada en 2022; la del año 8 vence hacia fines de 2026 o comienzos de 2027: el abogado puede ver si se pagó |
| US 8,349,239 B2, US 8,974,715 B2, US 9,724,866, US 10,675,803 | Stratasys | El inicio o el fin del contorno dentro de la región interior de la capa; variantes con rampa de flujo y solape | Costura por costo, barrido hacia adentro, quizá la bufanda | 2029-09-23 (US 8,974,715) y 2031-02-28 (US 8,349,239) |
| US 10,591,897 B2 | Autodesk | Borde cuyo ancho cambia según cuánto sobresale el contorno, medido con los ángulos entre puntos del borde de la primera capa | Borde automático (Q8) si su ancho cambia por esquina | Cerca de 2038-03-30 |
| US 12,472,695 B2 (US 2024/0025125 A1) | Markforged | Balsa de PEI, soporte de otro material, anclas de PEI dentro del soporte y faldas que unen la pieza al ancla | Nada hoy; tenerla en cuenta en balsas y soportes de materiales técnicos | Cerca de 2044 |
| Solicitud de ORNL (sin número a la vista) | ORNL | Material extra en cada cordón con trayectorias fuera del plano, contra el alabeo | Nada hoy (es no planar) | — |

## 8. Los 10 pasos más valiosos

Ordenados por ganancia medida contra esfuerzo, con lo que el motor ya tiene. «Q» son los ítems de
`docs/RESEARCH_MAP.md`.

| # | Qué | Ganancia medida | Esfuerzo | Ya tiene | Encaja en | Trabajos |
|---|---|---|---|---|---|---|
| 1 | **Planchado** según la ficha `ironing.md`, con valores por defecto de la literatura (separación 0,1 a 0,15 mm, flujo 10 a 17 %, 20 a 30 mm/s), margen de medio cordón en el filo, y la opción experimental «todas las capas sólidas» | Ra −60 a −83 %; con separación y velocidad bien elegidas, hasta −32 % del tiempo extra | Chico a medio: reusa las líneas monotónicas de la capa de arriba | Solo el tipo en la vista previa | Superficies y S2 | Alzyod y Ficzere 2024; Alzyod et al. 2024; Pricci y Percoco 2025; Matras 2025 |
| 2 | **Modo resistencia completo** (Q1 ampliado): cordones anchos, más paredes antes que más relleno, ventilador bajo en el interior y alto solo en voladizos, puentes y pared externa, cordones alineados con la carga | +40 a +48 % entre capas (Allum 2023); tracción hasta 4 veces según el aire (Lee y Liu); −40 % con ventilador (Baum y Copetti); +40 % a 0° contra 90° (Gajjar) | Chico: perfil, ventilador por tipo de tramo y ángulo | Anchos variables; ventilador por capa y en puentes | Perfiles, enfriamiento, A1 | Allum et al. 2023 (mapa); Lee y Liu 2019; Baum y Copetti 2021; Gajjar et al. 2024; Swanepoel et al. 2024 |
| 3 | **Relleno sin bordes débiles entre zonas**: cada línea cruza las bandas del guiado por tensiones y del denso bajo el techo en un solo pase, o los cordones vecinos del borde se imprimen seguidos | Deformación a la rotura +26 a +33 % | Chico: las líneas ya son colineales | Bandas con el mismo patrón y dirección | `docs/INFILL.md`, etapas 2 y 4 | Hutton y Bartolai 2023 |
| 4 | **Sección redondeada del cordón** para los cordones sin vecinos (relleno disperso, capas combinadas, paredes de un cordón) | Corrige un exceso de 11 a 27 % del material de esos cordones (cuenta nuestra); explica P11 | Chico | E = ancho × alto en todo | Escritor y relleno combinado | Serdeczny et al. 2018; Comminal et al. 2018; Xu et al. 2022 |
| 5 | **Costura por costo** (Q6) sin empezar ni terminar dentro de la pieza | Aspecto: sin número publicado; evita el punto débil donde carga | Chico a medio | Cuatro modos y la bufanda | Escritor | Irimia 2024; Ermolai et al. 2025 y Grübel et al. 2026 (mapa); patente de Stratasys |
| 6 | **Flujo según la velocidad planeada**, calibrado con una pieza de prueba medida con calibre o con el teléfono (Q4 ampliado) | Error de ancho −46 a −51 %; Ra −37 a −51 %; la calidad de 27 mm/s a 60 mm/s | Medio a grande | El estimador ya planea la velocidad de cada punto | Escritor y calibraciones | Fravolini et al. 2025; Lin et al. 2025 y Comminal et al. 2019 (mapa); Mollah et al. 2022 |
| 7 | **Pieza de capacidad** y compensación de agujeros según el diámetro (y por ángulo, Q7) | Sin número general; el error de agujeros chicos es la queja más común | Chico a medio: el marco de calibraciones ya existe | Un agujero de 10 mm y `hole_offset` de un valor | Calibraciones | Moylan et al. 2014; ISO/ASTM 52902; Bazan et al. 2026; Şimşek y Yaman 2016; Boschetto 2016 (mapa) |
| 8 | **Secuencia de ángulos por capa** y relleno que rodea los agujeros en piezas cargadas (primer caso de Q5) | Tracción +12 a +20 % y deformación +19 a +35 % con agujeros; más tenacidad con otra secuencia | Chico (lista de ángulos) a medio (líneas que rodean agujeros) | Alternancia de 90°; densidad guiada, no dirección | Relleno y A1 | Kiendl y Gao 2020; Montalti et al. 2026; Xiao et al. 2021 |
| 9 | **Relleno homogeneizado en el análisis**, para piezas grandes y para iterar el relleno guiado | Analizar piezas que hoy no entran (60 a 100 mm) | Medio | Vóxeles con los huecos del relleno | A1, A2 | Bean et al. 2022 y 2025. **Antes, revisar la patente de WARF**, que toca el análisis actual |
| 10 | **Riesgo de alabeo y borde automático de ancho parejo** (Q8 ampliado) | Menos piezas perdidas; sin número publicado para el borde | Chico a medio | Borde de ancho fijo | Revisión antes de imprimir | Armillotta et al. 2018 y Spörk et al. 2018 (mapa); Wijnen et al. 2018; Ramian et al. 2021; patentes de Autodesk y Markforged |

**Después de estos diez:**
- la silueta esperada de cada capa para vigilar con cámara (Petsiuk y Pearce 2022), con P2 y P6;
- la altura de capa por saliencia (Wang et al. 2015), junto con capas gruesas donde la pared es vertical;
- el tope de flujo según la temperatura con el modelo de Serdeczny et al. 2020 (Q3);
- las interfaces trabadas entre materiales con MM1 (Dairabayeva et al. 2023; Ermolai et al. 2024);
- el puntaje de hilos desde una foto de la torre de retracción;
- coasting, barrido y arcos por la cobertura, sin esperar ganancias medidas.

**Lo que cambia en otros documentos** (para quien los mantenga):
- `docs/RESEARCH_MAP.md`, sección 6: sumar las patentes de la sección 7, sobre todo la de WARF.
- `docs/COOLING_SIMULATION.md`, T3: Lee y Liu 2019 y Baum y Copetti 2021 apoyan bajar el ventilador donde la unión
  importa; Allum et al. 2020 dice lo contrario en PLA. El banco decide.
- `docs/INFILL.md`, etapa 4: el borde entre bandas de densidad es un punto débil (Hutton y Bartolai 2023).
- `docs/MULTIMATERIAL.md`, MM2: medir temprano cuánto ahorra la purga en el relleno.

## Fuentes

Las URL de DOI llevan a la página de la editorial. Entre corchetes, cómo se leyó.

**Superficie**
- Alzyod, H., Ficzere, P. *Ironing process optimization for enhanced properties in material extrusion technology
  using Box–Behnken Design.* Scientific Reports 14:2300, 2024. [DOI 10.1038/s41598-024-52827-5](https://doi.org/10.1038/s41598-024-52827-5)
  ([PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC10821878/)) [texto]
- Alzyod, H., Ficzere, P., Borbas, L. *Optimizing ironing parameters in material extrusion (MEX) technology:
  enhancing efficiency and performance.* Discover Applied Sciences 6:578, 2024.
  [DOI 10.1007/s42452-024-06249-7](https://doi.org/10.1007/s42452-024-06249-7) [resumen]
- Pricci, A., Percoco, G. *Towards a digital twin of extrusion-based additive manufacturing: an experimentally
  validated numerical model of ironing process.* Progress in Additive Manufacturing 10(9):6491–6506, 2025.
  [DOI 10.1007/s40964-025-00988-8](https://doi.org/10.1007/s40964-025-00988-8) [resumen]
- Matras, A. *Application of Response Surface Methodology for Modeling and Optimization of the Top Surface Ironing
  Process in Parts Produced by MEX 3D Printing.* Materials 18(22):5248, 2025.
  [DOI 10.3390/ma18225248](https://doi.org/10.3390/ma18225248) [resumen]
- Irimia, A.-I. *Addressing surface quality via seam alignment parametrization.* Materials Research Proceedings 46:41–48
  (IManEE 2024), 2024. [DOI 10.21741/9781644903377-6](https://doi.org/10.21741/9781644903377-6) [resumen]
- Bazan, A., Turek, P., Kubik, P. *Metrological Evaluation of Dimensional and Surface Roughness of Thermoplastic PLA
  Parts in High-Speed MEX 3D Printing Using a Dodecahedron Benchmark Geometry.* Materials 19(15):3255, 2026.
  [DOI 10.3390/ma19153255](https://doi.org/10.3390/ma19153255) [resumen]
- Moylan, S., Slotwinski, J., Cooke, A., Jurrens, K., Donmez, M. A. *An Additive Manufacturing Test Artifact.* Journal of
  Research of NIST 119:429–459, 2014. [DOI 10.6028/jres.119.017](https://doi.org/10.6028/jres.119.017) [resumen]
- ISO/ASTM 52902:2019, *Additive manufacturing — Test artifacts — Geometric capability assessment of additive
  manufacturing systems.* [ISO](https://www.iso.org/standard/67287.html) [ficha del catálogo, vista en una búsqueda]
- Şimşek, S., Yaman, U. *Dimensional accuracy improvement of fused filament fabrication holes utilizing modified
  interior.* ICEMS 2016, Chiba. [METU](https://avesis.metu.edu.tr/yayin/d17aa25e-89d9-4e61-8910-40ac013fc322/dimensional-accuracy-improvement-of-fused-filament-fabrication-holes-utilizing-modified-interior) [sin verificar]
- Wang, W., Chao, H., Tong, J., Yang, Z., Tong, X., Li, H., Liu, X., Liu, L. *Saliency-Preserving Slicing Optimization
  for Effective 3D Printing.* Computer Graphics Forum 34(6):148–160, 2015.
  [DOI 10.1111/cgf.12527](https://doi.org/10.1111/cgf.12527) [resumen]
- Takahashi, H., Miyashita, H. *Expressive Fused Deposition Modeling by Controlling Extruder Height and Extrusion
  Amount.* CHI 2017, pp. 5065–5074. [DOI 10.1145/3025453.3025933](https://doi.org/10.1145/3025453.3025933) [solo el
  registro]

**Trayectorias y flujo**
- Serdeczny, M., Comminal, R., Pedersen, D., Spangenberg, J. *Experimental validation of a numerical model for the
  strand shape in material extrusion additive manufacturing.* Additive Manufacturing 24:145–153, 2018.
  [DOI 10.1016/j.addma.2018.09.022](https://doi.org/10.1016/j.addma.2018.09.022) [resumen]
- Comminal, R., Serdeczny, M., Pedersen, D., Spangenberg, J. *Numerical modeling of the strand deposition flow in
  extrusion-based additive manufacturing.* Additive Manufacturing 20:68–76, 2018.
  [DOI 10.1016/j.addma.2017.12.013](https://doi.org/10.1016/j.addma.2017.12.013) [solo el registro]
- Xu, D., Agassant, J.-F., Pigeonneau, F. *Dimensions of the deposited strand in the material extrusion process:
  Experimental and numerical investigations.* Additive Manufacturing 59:103107, 2022.
  [DOI 10.1016/j.addma.2022.103107](https://doi.org/10.1016/j.addma.2022.103107) [resumen, por una búsqueda]
- Serdeczny, M., Comminal, R., Pedersen, D., Spangenberg, J. *Experimental and analytical study of the polymer melt
  flow through the hot-end in material extrusion additive manufacturing.* Additive Manufacturing 32:100997, 2020.
  [DOI 10.1016/j.addma.2019.100997](https://doi.org/10.1016/j.addma.2019.100997) [resumen]
- Fravolini, M. L., Rossi, A., Moretti, M., Senin, N., Ferrante, F. *Data-driven reference shaping for optimal fused
  filament fabrication.* Control Engineering Practice 164:106511, 2025.
  [DOI 10.1016/j.conengprac.2025.106511](https://doi.org/10.1016/j.conengprac.2025.106511)
  ([IRIS Perugia](https://research.unipg.it/handle/11391/1606657)) [resumen]
- Hoteit, R., Balestra, A., Mingard, N., Balta, E. C., Lygeros, J. *Closed Loop Reference Optimization for Extrusion
  Additive Manufacturing.* arXiv 2512.16333, 2025. [arXiv](https://arxiv.org/abs/2512.16333) [resumen, preimpresión]
- Mollah, M. T., et al. Estudio de la precisión de esquinas de 15° a 135° en extrusión de material. Solid Freeform
  Fabrication Symposium 2022. [Repositorio de Hull](https://hull-repository.worktribe.com/output/4864035) [sin
  verificar: la página no abrió]
- Hutton, L. J., Bartolai, J. *Novel Concepts to Integrate Dense and Sparse Infill Regions in Material Extrusion AM
  Parts.* Proceedings of the 34th Solid Freeform Fabrication Symposium, pp. 822–834, 2023.
  [PDF](https://utw10945.utweb.utexas.edu/sites/default/files/2023/063%20NovelConceptstoIntegrateDenseandSparseInfillRegionsinMaterialExtrusionAMParts.pdf) [texto]
- Montalti, A., Yamashita, T., Nakamura, T., Liverani, A., Takezawa, A. *Geometry-driven hole-aware adaptive infill for
  improved tensile performance of FFF components.* Journal of Manufacturing Processes 173:799–816, 2026.
  [DOI 10.1016/j.jmapro.2026.06.075](https://doi.org/10.1016/j.jmapro.2026.06.075)
  ([IRIS Bolonia](https://cris.unibo.it/handle/11585/1071175)) [resumen]
- Xiao, X., Roh, B.-M., Zhu, F. *Strength Enhancement in Fused Filament Fabrication via the Isotropy Toolpath.* Applied
  Sciences 11(13):6100, 2021. [DOI 10.3390/app11136100](https://doi.org/10.3390/app11136100) [resumen]
- *Vision-guided process optimization for stringing reduction in FDM 3D printing using RSM.* Engineering Research
  Express, 2025. [DOI 10.1088/2631-8695/ae0d44](https://doi.org/10.1088/2631-8695/ae0d44) [resumen; los autores no se
  vieron]
- Documentación de usuario sobre arcos: OrcaSlicer, *Precision*
  ([wiki](https://www.orcaslicer.com/wiki/print_settings/quality/quality_settings_precision)); Anycubic, *Arc move*
  ([wiki](https://wiki.anycubic.com/en/software-and-app/new-page-anycubic-slicer-beta(orca-version)/arc-move)) [vistas
  por una búsqueda; solo texto de ayuda, ningún código]
- Prueba de usuario sobre la purga: Hackaday, *Reducing Poop On Multicolor Prints*
  ([Hackaday](https://hackaday.com/tag/purge/)) [vista por una búsqueda]

**Resistencia**
- Lee, C., Liu, C. *The influence of forced-air cooling on a 3D printed PLA part manufactured by fused filament
  fabrication.* Additive Manufacturing 25:196–203, 2019.
  [DOI 10.1016/j.addma.2018.11.012](https://doi.org/10.1016/j.addma.2018.11.012) [resumen]
- Baum, L. T., Copetti, J. *Analysis of the Impact of the Part Air Cooling on the Interlayer Yield Strength in
  Additive Manufacturing of PLA and PETG.* COBEM 2021.
  [DOI 10.26678/ABCM.COBEM2021.COB2021-0310](https://doi.org/10.26678/ABCM.COBEM2021.COB2021-0310) [resumen]
- Gajjar, T., Yang, R., Ye, L., Zhang, Y. *Effects of key process parameters on tensile properties and interlayer
  bonding behavior of 3D printed PLA using fused filament fabrication.* Progress in Additive Manufacturing
  10(2):1261–1280, 2024. [DOI 10.1007/s40964-024-00704-y](https://doi.org/10.1007/s40964-024-00704-y) [resumen]
- Kiendl, J., Gao, C. *Controlling toughness and strength of FDM 3D-printed PLA components through the raster layup.*
  Composites Part B 180:107562, 2020. [DOI 10.1016/j.compositesb.2019.107562](https://doi.org/10.1016/j.compositesb.2019.107562)
  ([NTNU Open](https://hdl.handle.net/11250/2628204)) [resumen]
- Swanepoel, M., Botha, I., Bright, G. *Impact of fused deposition modelling printing parameters on part mass and
  strength.* MATEC Web of Conferences 406:07011, 2024.
  [DOI 10.1051/matecconf/202440607011](https://doi.org/10.1051/matecconf/202440607011) [resumen]
- Gocyk, K., Afshar, R., Abali, B. E. *On Strength Variations Effected by Infill Patterns Such as Honeycomb, Gyroid,
  and Archimedean Chords Used in Additive Manufacturing.* Polymers 18(13):1619, 2026.
  [DOI 10.3390/polym18131619](https://doi.org/10.3390/polym18131619) [resumen]
- Bean, P., Lopez-Anido, R. A., Vel, S. S. *Numerical Modeling and Experimental Investigation of Effective Elastic
  Properties of the 3D Printed Gyroid Infill.* Applied Sciences 12(4):2180, 2022.
  [DOI 10.3390/app12042180](https://doi.org/10.3390/app12042180) [resumen]
- Bean, P., Vel, S. S., Lopez-Anido, R. A. *Investigation of the nonlinear response of gyroid infills for prediction
  of the effective yield strength.* Progress in Additive Manufacturing 10(11):9697–9716, 2025.
  [DOI 10.1007/s40964-025-01200-7](https://doi.org/10.1007/s40964-025-01200-7) [resumen]

**Alabeo**
- Ramian, J., Ramian, J., Dziob, D. *Thermal Deformations of Thermoplast during 3D Printing: Warping in the Case of
  ABS.* Materials 14(22):7070, 2021. [DOI 10.3390/ma14227070](https://doi.org/10.3390/ma14227070) [resumen]
- Wijnen, B., Sanders, P., Pearce, J. M. *Improved model and experimental validation of deformation in fused filament
  fabrication of polylactic acid.* Progress in Additive Manufacturing 3(4):193–203, 2018.
  [DOI 10.1007/s40964-018-0052-4](https://doi.org/10.1007/s40964-018-0052-4) [sin verificar el detalle]
- Trofimov, A., Le Pavic, J., Pautard, S., Therriault, D., Lévesque, M. *Experimentally validated modeling of the
  temperature distribution and the distortion during the Fused Filament Fabrication process.* Additive Manufacturing
  54:102693, 2022. [DOI 10.1016/j.addma.2022.102693](https://doi.org/10.1016/j.addma.2022.102693) [registro y una
  búsqueda]
- Godderidge, T., Tekinalp, H., Ozcan, S., Garg, N., Kim, S. *Reducing Warpage in Additive Manufacturing With Novel
  Toolpath Design.* ASME MSEC 2024, vol. 1. [DOI 10.1115/MSEC2024-125118](https://doi.org/10.1115/MSEC2024-125118)
  ([ORNL](https://impact.ornl.gov/en/publications/reducing-warpage-in-additive-manufacturing-with-novel-toolpath-de/))
  [resumen]

**Monitoreo y multimaterial**
- Petsiuk, A., Pearce, J. M. *Towards smart monitored AM: Open source in-situ layer-wise 3D printing image anomaly
  detection using histograms of oriented gradients and a physics-based rendering engine.* Additive Manufacturing
  52:102690, 2022. [DOI 10.1016/j.addma.2022.102690](https://doi.org/10.1016/j.addma.2022.102690)
  ([arXiv 2111.02703](https://arxiv.org/abs/2111.02703)) [resumen]
- Dairabayeva, D., Perveen, A., Talamona, D. *Investigation on the mechanical performance of mono-material vs
  multi-material interface geometries using fused filament fabrication.* Rapid Prototyping Journal 29(11):40–52, 2023.
  [DOI 10.1108/RPJ-07-2022-0221](https://doi.org/10.1108/RPJ-07-2022-0221) [resumen]
- Ermolai, V., Sover, A., Boca, M., Mihalache, A., Irimia, A., Hrițuc, A., Slătineanu, L., Nagîț, G.
  *Mechanical behavior of macroscopic interfaces for 3D printed multi-material samples made of dissimilar materials.*
  Mechanics & Industry 25:24, 2024. [DOI 10.1051/meca/2024017](https://doi.org/10.1051/meca/2024017) [resumen]

**Patentes** (registros públicos; ninguna nota es una opinión legal)
- Wisconsin Alumni Research Foundation, US 10,353,378 B2, *Homogenization of material properties in additively
  manufactured structures.* [Google Patents](https://patents.google.com/patent/US10353378)
- Stratasys, US 8,349,239 B2 y US 8,974,715 B2, *Seam concealment for three-dimensional models* (y US 9,724,866,
  US 10,675,803). [US 8,349,239](https://patents.google.com/patent/US8349239),
  [US 8,974,715](https://patents.google.com/patent/US8974715)
- Autodesk, US 10,591,897 B2, *Systems and methods of reducing curl and wrap in additive manufacturing.*
  [Google Patents](https://patents.google.com/patent/US10591897B2)
- Markforged, US 12,472,695 B2 (US 2024/0025125 A1), *Anchors and skirts to reduce part warping.*
  [Google Patents](https://patents.google.com/patent/US20240025125A1)

## Fuentes que no se pudieron leer

- Kiendl y Gao 2020: el texto completo no abrió (ScienceDirect 403, el repositorio de NTNU redirige a una página
  vacía). Falta la secuencia de ángulos que proponen.
- Lee y Liu 2019: solo el resumen (ScienceDirect no abre).
- Irimia 2024: el PDF abierto de Materials Research Forum respondió 403; solo el resumen de Crossref.
- Swanepoel et al. 2024 (MATEC): 403; solo el resumen.
- Mollah et al. 2022 (repositorio de Hull): 403; solo lo que mostró una búsqueda.
- Xu, Agassant y Pigeonneau 2022 (HAL): bloqueado por la protección contra robots; solo lo que mostró una búsqueda.
- Montalti et al. 2026: el PDF de IRIS supera el tamaño que se puede bajar; solo el resumen.
- Şimşek y Yaman 2016: la página de METU no tiene el resumen; el −80 % sale de un resumen de búsqueda.
- Fitzharris, Watanabe, Rosen, Shofner, *Effects of material properties on warpage in fused deposition modeling
  parts*, IJAMT 95:2059–2070, 2018 ([DOI](https://doi.org/10.1007/s00170-017-1340-8)): no encontré el resumen; no se usa.
- Huynh et al., *Minimizing Warpage for Macro-Size Fused Deposition Modeling Parts*, CMC 68(3):2913–2923, 2021
  ([DOI](https://doi.org/10.32604/cmc.2021.016064)): sin resumen; no se usa.
- Las páginas de Springer redirigen al inicio de sesión; los resúmenes se tomaron de Semantic Scholar y Crossref.
- La página de la ISO de la norma 52902 respondió 403.
- Un PDF de euspen (ICE24274) y uno de Atlantis Press sobre agujeros no se pudieron extraer; no se usan.
