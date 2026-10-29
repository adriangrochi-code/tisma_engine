# Arcos en voladizos: lo publicado y cómo hacerlo en el motor

Pedido del dueño (2026-10-26): «buscá papers sobre arc overhangs». Es la base de S1 en `PLAN.md`.

Este informe amplía lo que ya dice `docs/SUPPORT_RESEARCH.md` (S1) y se apoya en la ficha
`docs/spec/features/arc_overhangs.md`, que dice qué hace Tisma Slicer. La ficha manda en el comportamiento. Los
artículos y la descripción del autor dan el método y los límites.

**Sala limpia:**
- de la técnica original, solo la descripción en palabras de su autor (la página principal del proyecto) y los
  valores de impresión que recomienda. No se abrió ningún archivo de código;
- lo mismo con la integración por posprocesado para PrusaSlicer: solo su descripción;
- no se abrieron el código de SuperPleccer, ni los laminadores derivados con «Wave Overhangs» (AGPL), ni el guion de la
  tesis sobre LaSO;
- los artículos se leyeron completos cuando estaban abiertos, y si no, por su resumen o por notas de prensa. Cada fila
  lo dice.

**Marcas:** **artículo** (leído completo), **resumen** (solo el resumen o la ficha), **nota** (prensa, un blog o un
tercero), **autor** (la descripción del autor de la técnica).

## Resumen

1. **La idea:**
   - cada cordón se apoya de costado en el cordón anterior, no en la capa de abajo;
   - los arcos crecen desde el borde apoyado, cada uno un poco más grande que el anterior;
   - así se imprime un voladizo de 90°, plano, sin soporte, en una impresora común y sin cambiar nada de la máquina.
2. **No hay un artículo revisado que mida los arcos solos.** El único estudio con números es el de Andersons, Sanchez
   y Vaneker (*Additive Manufacturing Letters*, 2026). Compara los arcos con «ondas»:
   - los arcos se hunden alrededor de sus centros, donde los arcos son cortos y no llegan a enfriarse;
   - con las ondas, según la prensa, el desvío baja de un 49 a un 66 % y el hundimiento máximo de un 30 a un 48 % en la
     forma más difícil.
3. **La idea de fondo es anterior a los arcos:**
   - platos impresos en espiral sin soporte (Kanada 2015);
   - «Print on Air»: caminos paralelos al borde apoyado, cada uno sobre el anterior (Fazzini et al. 2019; Barile et
     al. 2020);
   - un caso con FullControl (Alhamed et al. 2022).
4. **Lo que dice la práctica:**
   - lento: de 0,5 a 5 mm/s;
   - ventilador al máximo;
   - temperatura baja: 190 a 195 °C en PLA;
   - probado casi solo en PLA;
   - la cara de abajo queda áspera;
   - el alabeo aparece después, cuando las capas de arriba se enfrían y tiran.
5. **Recomendación:**
   - hacer S1 tal como pide la ficha: arcos, 10 % de solape, 5 mm/s, ventilador de puentes, sección redonda, relleno
     sólido arriba;
   - la geometría que la ficha no fija (dónde van los centros y cuándo empieza un grupo nuevo), con diseño propio;
   - como opciones propias, apagadas por defecto: radio mínimo, tiempo mínimo por arco, radio máximo y capas de arriba
     más suaves;
   - después, un modo de **frentes por distancia** (la idea de las ondas y de Print on Air), calculado con un campo de
     distancia en una grilla. Es lo que mejor midió el único estudio.

## 1. Cómo funciona (basado en la descripción de su autor y en los artículos citados)

1. **La zona:** la parte de la capa que no tiene nada debajo y que no es un puente. Un puente tiene apoyo de los dos
   lados; esta zona tiene apoyo de uno solo.
2. **El ancla:** el tramo del borde de la zona que está sobre lo impreso abajo.
3. **El primer arco:**
   - tiene el centro sobre el ancla y un radio chico;
   - los siguientes son círculos con el mismo centro, cada uno un poco más grande;
   - la separación es algo menor que el ancho de línea, así cada arco pisa al anterior y se pega a él;
   - cada círculo se recorta por el borde de la zona, y queda un arco.
4. **Un grupo nuevo:** cuando los arcos ya no pueden crecer, porque tocaron el borde o llegaron a un radio máximo, otro
   grupo empieza sobre lo ya impreso (el último arco). Así sigue hasta cubrir la zona. Un grupo se puede abrir en dos o
   más, para doblar esquinas.
5. **El cordón:**
   - se imprime en el aire, con sección redonda, como un puente;
   - lento y con el ventilador al máximo, para que se endurezca antes de que llegue el siguiente.
6. **La capa de arriba:** se imprime normal, sobre los arcos, como relleno sólido.

**Por qué arcos y no líneas rectas:**
- con caminos rectos paralelos al borde, las esquinas dejan pedazos sueltos que cuelgan, y el hundimiento se suma
  vuelta a vuelta;
- un arco queda pegado al anterior en todo su largo (explicación de un lector en Hackaday; sin ensayo).
- El artículo de 2026 llama a esto «voladizo apoyado de costado» (LaSO, *laterally supported overhang*):
  - la boquilla pisa un poco el cordón vecino y aprieta el nuevo contra él;
  - el cordón queda con forma de gota, con mucha superficie de contacto (nota).

## 2. Lo publicado

| Trabajo | Qué hace | Parámetros | Resultado | Marca |
|---|---|---|---|---|
| **Kanada 2015** (WASET 9(5); SFF Symposium 2015) | Platos y cuencos sin soporte: el filamento da vueltas en espiral que se abren, cada vuelta apoyada en parte sobre la anterior. Sin capas, sin costuras | Mucho más material del normal (4 a 6 veces en el método con capas); altura de capa según el ángulo | Platos con 15° desde la horizontal (75° de voladizo). Con capas, las costuras arruinan el plato; en espiral, no | artículo |
| **Print on Air** (Fazzini et al., MetroInd4.0&IoT 2019; Barile et al., *Sensors* 20(2), 2020) | A la sección de la capa se le resta la de abajo. Sobre la diferencia, caminos paralelos al borde apoyado, separados menos que el ancho de línea: cada uno pisa al anterior por una fracción de milímetro. Las paredes de afuera van al final, con un desplazamiento hacia adentro | Capas de 0,2 mm; ventilador; una velocidad que deje endurecer el plástico (sin números). Laminador propio, derivado de Slic3r | Voladizos de 90° (rectángulo, círculo y triángulo) y un puente, 10 veces cada uno. Ver la sección 4 | artículo |
| **Alhamed et al.** (*Mekatronika* 4(2), 2022) | El mismo método, con caminos hechos en FullControl | Capas de 0,2 mm; **0,5 mm/s** (30 mm/min) en el voladizo; ventilador al 100 % | Rectángulo de 90°, puente y T, sin deformación. Sugieren otro ventilador | artículo |
| **Arc Overhang** (Steven McCulloch, 2022–2023) | Arranca en el borde más largo de la zona. El primer arco es el más grande posible con el centro sobre ese borde. Los demás arcos arrancan sobre arcos ya hechos y se abren en ramas. Para cuando el próximo arco sería menor que medio ancho de línea. Las ramas se recorren en profundidad; el autor cree que en anchura sería más confiable | Lo más frío posible (195 °C en PLA); ventilador al máximo; unos 5 mm/s, a veces demasiado para los arcos más chicos. Boquilla de 0,4. Solo PLA; duda que sirva en ABS | Voladizos de 90° sin soporte. Prueba de concepto | autor |
| **CNC Kitchen, primera nota** (2022) | Prueba y explica la técnica | 2 a 5 mm/s; mucho ventilador; más ventilador permite ir más rápido | Sin soporte, la placa se cae en hilos; con soporte, cicatrices; con arcos, cubre todo el vano. La cara de abajo queda áspera. Algo de alabeo, cuando las capas de arriba se enfrían y tiran del disco de arcos | nota |
| **Integración por posprocesado** (Nicolai Wachenschwan, 2023) y **CNC Kitchen, segunda nota** | Reemplaza el relleno de puente de las zonas que tocan el perímetro de afuera. Exige área y largo mínimos. Mete la zona dentro del perímetro al menos medio ancho. Corre el centro hacia lo apoyado para que el arco más chico sea más grande y tenga tiempo de enfriarse. Después del último arco, el punto más lejano es el inicio del siguiente grupo | 190 °C en PLA; unos 2 mm/s; radio máximo de 40 mm, para que los arcos largos no se abomben. Capas de arriba: más lentas, ventilador al 10 %, relleno sólido en curva de Hilbert (tramos cortos que encogen menos) | Alabeo de menos de 2 mm en los bordes; pernos chicos en las esquinas lo bajan. Los arcos salen gruesos: la pieza puede quedar 0,1 a 0,5 mm más alta. Zonas angostas y largas fallan | autor y nota |
| **SuperPleccer** (laminador derivado de SuperSlicer; nota de CNC Kitchen) | Arcos automáticos en las caras horizontales | Puentes a 1 o 2 mm/s | También pone arcos en los puentes, donde solo pierde tiempo | nota |
| **Ondas** (Andersons, Sanchez, Vaneker; *Additive Manufacturing Letters* 18, 2026) | Los caminos son frentes de onda: cada frente nuevo es la envolvente de círculos centrados en el anterior (principio de Huygens). Rodean agujeros y esquinas, como una onda que se difracta. Se compara contra arcos | PLA, impresora abierta de 3 ejes. Unos 2 mm/s (según la prensa) | Ver la sección 4 | nota (el artículo no se pudo leer) |
| **LaSO con pernos** (Rieks Kaiser, tesis de maestría, Universidad de Twente) | Ondas con pernos chicos debajo, para que la placa no se curve hacia arriba | Ender 3 V3 | Según el proyecto: menos marcas, menos material y una cara estanca | nota (la tesis no se encontró) |
| **Wave Overhangs** (laminadores derivados, 2026; notas) | Las ondas de McCulloch y de Andersons en laminadores comunes | Unos 40 ajustes, sin valores recomendados | PLA con ventilador al máximo anda mejor. PETG, ABS y PC fallan seguido. Las zonas chicas salen bien; las grandes se alabean | nota |
| **Simulación de puentes** (Ao, Lin, Liu, Xia, Meng; *Scientific Reports* 14, 2024) | Simulación del fluido de un puente sin apoyo | 190 a 230 °C; 0,05 a 0,2 m/s | Hundimiento sobre luz: 3,4 a 6,8 % entre 190 y 230 °C, con un quiebre cerca de 210 °C; 14,8 a 4,3 % de 0,05 a 0,2 m/s | artículo |
| **Puentes largos** (He, Zhu, Zhang, Yang; *Polymers* 18(2), 2026; ya en `docs/RESEARCH_MAP.md`) | Puentes de 60 a 140 mm | — | Fallan por la tensión térmica, no por el peso | resumen |
| **Umbral de voladizo** (Jiang, Stringer, Xu, Zhong; *IJCIM* 31(10), 2018; ya en `docs/RESEARCH_MAP.md`) | Voladizos de 20 a 50° | — | El ángulo que sale bien depende del ventilador, la velocidad y la temperatura | resumen |

**Un contraste:** en un puente, más velocidad hunde menos (Ao et al.). En los arcos todos van lento. No se contradicen:
- un puente cuelga de sus dos extremos, ya fríos, y se endurece en el aire;
- un arco cuelga del arco de al lado, y tiene que endurecerse pegado a él antes de que lo cargue el siguiente.

## 3. Límites

| Límite | Qué se sabe | Fuente |
|---|---|---|
| **Forma de la zona** | Necesita un borde apoyado. Lo que tiene apoyo de los dos lados es un puente y se hace como puente. Las zonas angostas y largas fallan. Con formas complicadas salen muchos arcos chicos, que se hunden en sus centros. Los arcos rodean mal los agujeros; las ondas, bien | Integración; Andersons et al.; notas |
| **Radio mínimo** | El autor para cuando el próximo arco sería menor que medio ancho de línea. La integración corre el centro hacia lo apoyado para agrandar el arco más chico. En la ficha, el primer arco entero tiene 0,686 mm de radio | Autor; integración; ficha A1 |
| **Radio máximo** | 40 mm, para que los arcos largos no se abomben | Integración |
| **Velocidad** | 5 mm/s (autor, ficha); 2 a 5 (CNC Kitchen); unos 2 (integración, ondas); 1 a 2 (SuperPleccer); 0,5 (Alhamed et al.). Los arcos chicos piden menos | Varias |
| **Enfriamiento** | Ventilador al máximo en todos. Un segundo ventilador ayuda (Alhamed et al.). Con más enfriamiento se puede ir más rápido (CNC Kitchen). WirePrint, que extruye en el aire, también pide más aire para ir más rápido | Varias |
| **Temperatura** | Lo más baja posible: 190 a 195 °C en PLA. En puentes, el hundimiento crece rápido arriba de unos 210 °C | Autor; integración; Ao et al. |
| **Material** | PLA. PETG, ABS y PC fallan seguido. El autor duda de ABS y de los que no aguantan ventilador | Autor; notas |
| **Altura del cordón** | La ficha: sección redonda de 0,4 mm, como los puentes (0,1257 mm³ por mm en A1). Los arcos salen más gruesos que la capa: la pieza puede quedar 0,1 a 0,5 mm más alta. Print on Air quedó dentro de una capa (0,2 mm) del espesor buscado | Ficha; integración; Barile et al. |
| **Capas de arriba** | La ficha: relleno sólido. La práctica: más lentas, con poco ventilador (10 %) y con tramos cortos (Hilbert). Con poco enfriamiento se hunden; con mucho, alabean | Ficha; integración; CNC Kitchen |
| **Alabeo** | Menos de 2 mm en los bordes. Aparece al enfriarse las capas de arriba. Lo bajan pernos chicos en las esquinas | Integración; CNC Kitchen; Kaiser |
| **Cara de abajo** | Áspera, con un dibujo «cristalino»; un hoyuelo en el centro de cada grupo de arcos | CNC Kitchen; notas sobre las ondas |
| **Tiempo** | En piezas chicas, menos que con soporte (Barile et al.; Alhamed et al.). En el demostrador de las ondas, un 72 % más que con soporte, por ir a unos 2 mm/s | Artículos; nota |

## 4. Resultados medidos

| Trabajo | Pieza | Con soporte | Sin soporte | Arcos u ondas |
|---|---|---|---|---|
| Barile et al. 2020 | Rectángulo de 90° | 0:38 h | Tan hundido que no se pudo medir | Print on Air: 0:28 h; espesor a menos de una capa del buscado |
| | Círculo | 1:04 h | — | 0:41 h |
| | Triángulo | 0:25 h | — | 0:16 h |
| | Puente | 1:12 h | Lo resuelve el laminador común | 0:56 h; para puentes casi no hace falta |
| Alhamed et al. 2022 | Rectángulo de 90° | 0:41 h, unos 2 g de soporte, difícil de sacar | 0:34 h, deformado | 0:33 h, sin deformación |
| | Puente | 0:46 h, unos 2 g | 0:38 h, deformado | 0:39 h, sin deformación |
| | T | 0:41 h, unos 4 g | 0:31 h, deformado | 0:32 h, sin deformación |
| Andersons et al. 2026 | Una capa de PLA en tres formas (fácil, media, difícil), tres veces cada una, escaneadas en 3D | — | — | Ondas contra arcos: los arcos se hunden alrededor de sus centros. En la forma difícil, el desvío típico −49 a −66 % y el hundimiento máximo −30 a −48 % (VoxelMatters); «hasta −66 %» (Fabbaloo). Con varias capas encima, las ondas aguantan. Desvíos parecidos a los de una pieza con soporte, con algo de alabeo en los bordes |
| Andersons et al. 2026 | Demostrador | Base | — | Ondas: −39 % de material; +72 % de tiempo (Fabbaloo) |
| Integración por posprocesado | Piezas varias | — | — | Alabeo de menos de 2 mm en los bordes; 0,1 a 0,5 mm de más en la altura |
| Ficha, prueba A1 (Tisma Slicer) | Tabla de 20 × 20 × 2 mm que sobresale 10 mm | — | — | Arcos a 5 mm/s; radios de los arcos enteros 0,686, 1,407, 2,128 y 2,849 mm; 606 mm de arcos y 62 retracciones en la capa; ventilador al 100 %; relleno sólido en la capa siguiente |

**Calidad de estos números:**
- los de Barile y Alhamed salen de los artículos;
- los de Andersons et al., de notas de prensa: VoxelMatters y Fabbaloo no se pudieron abrir, y sus cifras llegaron por
  los resúmenes del buscador. Hay que confirmarlos en el artículo, que es de acceso abierto (CC BY);
- los datos crudos (escaneos, G-code y una definición de Grasshopper) están en Mendeley Data. La definición de
  Grasshopper es código: no se abre.

## 5. Relación con la ficha `arc_overhangs.md`

| Tema | La ficha | El autor y los artículos | Para el motor |
|---|---|---|---|
| Zona | Sin apoyo y no puente | Igual; la integración exige que toque el perímetro y un área mínima | La ficha |
| Forma | Círculos concéntricos recortados, desde el borde apoyado | Igual | La ficha |
| Separación | Ancho × (1 − solape): 0,36 mm | El autor no la da; Print on Air: «una fracción muy chica de milímetro» de solape | La ficha |
| Centros y grupos nuevos | No lo dice | Primer centro en el borde más largo; grupos nuevos sobre arcos hechos, desde el punto más lejano | Diseño propio (6.1) |
| Radio mínimo | No lo dice; A1 da el primer radio entero | Medio ancho (autor); centro corrido (integración) | Diseño propio que reproduzca A1 |
| Distancia y radio máximos | `arcs.max_distance` (0 = sin límite) | 40 mm de radio (integración) | La ficha, y un radio máximo opcional |
| Cordón | Sección redonda, como los puentes | Sale grueso | La ficha |
| Velocidad | `arcs.speed`, 5 mm/s | 0,5 a 5 mm/s | La ficha; tiempo mínimo por arco como opción |
| Ventilador | Al máximo (`cooling.fan_bridges`) | Al máximo | La ficha |
| Temperatura | No lo dice | Baja | Opción propia, apagada |
| Retracción entre arcos | Sí | — | La ficha |
| Tipo en el G-code | `;TYPE:Overhang perimeter` | — | La ficha |
| Capa siguiente | Relleno sólido | Más lenta, poco ventilador, Hilbert | La ficha; lo demás como opciones |
| Alabeo | No lo dice | Pernos en las esquinas | Más adelante, con los soportes pintados |

**Diferencias con el autor que hay que respetar:** la ficha arranca los arcos con su propia regla (A1) y retrae entre
arcos. El autor no dice que retraiga. El motor sigue la ficha.

## 6. Recomendación para el motor

### 6.1 S1, lo que pide la ficha

1. **Las zonas** (regla 1): la capa menos lo que la sostiene abajo, sin las zonas que tienen apoyo en dos lados
   opuestos (esas son puentes). El motor ya encuentra lo que queda sobre el aire (`src/surfaces/bridges.*`) y los
   voladizos (`src/support/support.*`); falta separar lo que tiene apoyo de un solo lado.
2. **El ancla:**
   - el tramo del borde de la zona que está sobre lo impreso;
   - la zona se extiende hacia adentro de lo apoyado al menos medio ancho de línea, así el primer arco muerde algo
     firme (la integración pide lo mismo; Print on Air termina con las paredes corridas hacia adentro).
3. **El primer centro** (diseño propio): sobre el ancla, en el punto desde el que los arcos cubren más zona antes de
   tocar el borde. Se prueban puntos del ancla cada medio ancho y se elige el que más cubre.
4. **Los arcos:**
   - radios que crecen de a `ancho × (1 − arcs.overlap)`;
   - cada círculo recortado por la zona y por lo ya cubierto (Clipper2);
   - se descarta un pedazo que no toque el arco anterior ni el ancla: no tendría de qué colgarse.
5. **Grupos nuevos** (diseño propio, con la idea del autor):
   - cuando un arco queda vacío o llega a `arcs.max_distance`, los puntos del último arco que miran a zona sin cubrir
     son candidatos;
   - el centro nuevo va sobre ese arco, en el candidato más lejano del centro anterior;
   - los grupos se recorren en anchura, como sugiere el autor;
   - termina cuando lo que falta es más angosto que medio ancho.
6. **El radio inicial:** se elige para reproducir A1 (0,686 mm en el primer arco entero, con 0,36 de separación).
   Nunca menor que medio ancho.
7. **Imprimir:**
   - cordón de sección redonda: 0,1257 mm³ por mm con ancho de 0,4 (A1: 0,052245 mm de filamento por mm);
   - a `arcs.speed`, ventilador `cooling.fan_bridges`, retracción entre arcos, `;TYPE:Overhang perimeter`;
   - del centro hacia afuera en cada grupo;
   - el tiempo mínimo de capa no frena los arcos, igual que a los puentes (`cooling.md`, K5).
8. **La capa siguiente:** relleno sólido sobre la zona (regla 6).
9. **El verificador:** acepta un cordón sobre el aire marcado como voladizo solo si cada punto está a menos de una
   separación de algo ya impreso (el ancla o un arco anterior). Es la regla que hace que el voladizo se sostenga, y
   sirve de chequeo.
10. **La vista previa:** los arcos con su sección redonda.
11. **El estimador:** cuenta los arcos lentos y las retracciones.

### 6.2 Opciones propias, apagadas por defecto

| Opción propuesta | Qué hace | Por qué |
|---|---|---|
| `arcs.min_radius` (mm) | Ningún arco más chico; el centro se corre hacia lo apoyado para lograrlo | Los arcos chicos no llegan a enfriarse y se hunden (autor; Andersons et al.) |
| `arcs.min_arc_time` (s) | Un arco corto se imprime más lento, hasta que dure al menos ese tiempo | Lo mismo, sin cambiar la geometría |
| `arcs.max_radius` (mm) | Un grupo nuevo cuando el arco llega a ese radio | Los arcos largos se abomban (integración: 40 mm) |
| `arcs.temperature` (°C, 0 = sin cambio) | Baja la boquilla durante los arcos | 190 a 195 °C en PLA. **Riesgo:** la temperatura tarda en cambiar; hay que medir si sirve dentro de una capa |
| `arcs.layers_above`, `arcs.fan_above`, `arcs.speed_above` | Las capas de arriba más lentas y con poco ventilador | Menos alabeo (integración, CNC Kitchen). Un relleno de tramos cortos, como el de Hilbert, sería otra opción |
| `arcs.pattern`: `arcs` o `fronts` | Ver 6.3 | El único estudio con números favorece a los frentes |

### 6.3 Más adelante: frentes por distancia

- **Qué:** en vez de círculos, las curvas de igual distancia al ancla, medida dentro de la zona (rodeando los
  agujeros), separadas lo mismo que los arcos.
- **De dónde viene:** es la idea publicada de Print on Air (caminos paralelos al borde apoyado) y de las ondas de
  Andersons et al. (cada frente, la envolvente de círculos sobre el anterior). La distancia medida dentro de la zona da
  esos mismos frentes y rodea los agujeros.
- **Cómo:**
  - la distancia exacta en una grilla (Felzenszwalb y Huttenlocher, citado en `nonplanar_modes.md`) hace falta
    también para el modo `overhangs` de las capas curvas, y su versión en GPU está en el plan (Q11 de
    `docs/RESEARCH_MAP.md`);
  - los frentes son curvas de nivel de esa distancia: el mismo principio de conjuntos de nivel del resto del motor.
- **Ventajas:**
  - no hay centros con arcos diminutos;
  - los frentes son largos y parejos;
  - rodean agujeros y esquinas;
  - en el único estudio, se hundieron bastante menos que los arcos.
- **Riesgos:**
  - las esquinas cóncavas del frente;
  - no encontré patentes sobre esto, pero no se revisó con un abogado;
  - los laminadores que lo implementan son AGPL: no se leen. El artículo es de acceso abierto y sí se puede leer.

## 7. Cómo probarlo

### 7.1 En el motor, sin imprimir

**Reglas de la geometría** (pruebas unitarias, en cada forma):

| # | Regla | Tolerancia |
|---|---|---|
| G1 | La separación entre arcos seguidos de un grupo es `ancho × (1 − solape)`: 0,36 con 0,4 y 10 % | 0,001 mm |
| G2 | Cada punto de cada arco está a menos de una separación de algo impreso antes (el ancla o un arco anterior) | 0,01 mm |
| G3 | Lo que queda sin cubrir (la zona menos los arcos engordados medio ancho) es más angosto que medio ancho | — |
| G4 | Ningún arco sale de la zona, salvo lo que entra en el ancla | 0,01 mm |
| G5 | Con `arcs.min_radius`, ningún arco es más chico | — |
| G6 | Dos grupos no se pisan más que el solape | 0,01 mm |

**La prueba de la ficha (A1):**
- capa 41 (Z 8,2), los 10 × 20 mm que sobresalen se imprimen con arcos, sin puente;
- F300, 0,052245 mm de filamento por mm;
- radios de los arcos enteros 0,686, 1,407, 2,128 y 2,849 mm;
- unos 606 mm de arcos y unas 62 retracciones en la capa;
- ventilador al 100 % durante los arcos;
- capa 42, relleno sólido sobre la zona.

**Formas generadas por código** (sin problemas de licencia):
- rectángulo de 90°, círculo y triángulo (las de Barile et al.);
- T y L con esquina entrante;
- zona con un agujero;
- tira angosta y larga: tiene que caer a lo normal, o avisar;
- puente: tiene que seguir siendo puente;
- el estante plano de 8 × 8 mm de la torre de temperatura (`PROGRESS.md`).

**Fuzz:** polígonos al azar, como hizo el autor para encontrar casos raros. Deben cumplir G1 a G4 y pasar el
verificador.

**El verificador:**
- rechaza un arco que no toca nada impreso antes;
- rechaza un cordón con el material o la velocidad que no corresponden.

**La vista previa y el tiempo:** los arcos se dibujan con su sección; se mide cuánto tarda generar los arcos por capa,
en el banco.

**`fronts`:** las mismas reglas; la distancia en CPU contra la de GPU.

### 7.2 Impresas (lo hace el dueño)

- **Piezas:** la de A1, el estante de 8 × 8, un círculo y una L con agujero (fácil, media y difícil, como Andersons et
  al.).
- **Qué medir:**
  - el hundimiento: máximo y desvío típico contra el plano, con un escáner o con un reloj comparador sobre una
    grilla;
  - el alabeo de los bordes después de 2 mm de capas encima;
  - el tiempo y el material contra soporte;
  - la rugosidad de la cara de abajo.
- **Qué variar:**
  - velocidad 2 y 5 mm/s;
  - temperatura 190 y 210 °C;
  - solape 5, 10 y 20 %;
  - arcos contra frentes.
- **Material:** PLA primero; PETG después (se espera que falle).
- Los números van a `PROGRESS.md`.

## 8. Preguntas de comportamiento para el equipo de fichas

Solo comportamiento y resultados de caja negra:

1. En A1 y en una zona en L, ¿dónde quedan los centros de los arcos, y cuándo empieza un grupo nuevo?
2. ¿Qué hace con una zona con un agujero, y con una tira angosta y larga?
3. Con `support.enabled` y `arcs.enabled` a la vez, ¿qué gana en una zona?
4. ¿Cambia algo en las capas de arriba, además del relleno sólido (ventilador, velocidad, patrón)?
5. ¿Hay un radio mínimo o máximo? ¿Qué pasa en A1 con `arcs.max_distance` de 5 mm?
6. ¿Los arcos de un grupo van siempre en el mismo sentido?

## Fuentes

**Artículos:**
- Andersons, J. A., Sanchez, S., Vaneker, T., *Wave-inspired path-planning strategy for support-free horizontal
  overhangs in FDM*, Additive Manufacturing Letters 18 (2026) 100392,
  [DOI 10.1016/j.addlet.2026.100392](https://doi.org/10.1016/j.addlet.2026.100392); preimpresión en
  [SSRN, DOI 10.2139/ssrn.6640458](https://doi.org/10.2139/ssrn.6640458); datos en
  [Mendeley Data, DOI 10.17632/xhw8xkjyc2.1](https://data.mendeley.com/datasets/xhw8xkjyc2/1) (CC BY 4.0).
- Barile, G., Leoni, A., Muttillo, M., Paolucci, R., Fazzini, G., Pantoli, L., *Fused-Deposition-Material
  3D-Printing Procedure and Algorithm Avoiding Use of Any Supports*, Sensors 20(2) (2020) 470,
  [DOI 10.3390/s20020470](https://doi.org/10.3390/s20020470) ([PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC7014049/)).
- Fazzini, G., Paolini, P., Paolucci, R., Chiulli, D., Barile, G., Leoni, A., Muttillo, M., Pantoli, L., Ferri, G.,
  *Print On Air: FDM 3D Printing Without Supports*, II Workshop on Metrology for Industry 4.0 and IoT, 2019, 350–354,
  [DOI 10.1109/METROI4.2019.8792846](https://doi.org/10.1109/METROI4.2019.8792846) (solo el resumen).
- Alhamed, A., Mohd Romlay, F. R., Muhamad, K. F., Mohd Zawawi, M. Z., Ishak, I., *Support-Free Fused Deposition
  Modelling Printing for Overhangs Structure*, Mekatronika 4(2) (2022) 64–71,
  [DOI 10.15282/mekatronika.v4i2.8945](https://doi.org/10.15282/mekatronika.v4i2.8945).
- Kanada, Y., *3D-Printing Plates without "Support"*, International Journal of Computer, Control, Quantum and
  Information Engineering (WASET) 9(5) (2015) 568–574,
  [PDF](https://www.kanadas.com/3dprint/3D-Printing-Plates-without-Support.pdf) (solo el resumen); Kanada, Y.,
  *Support-less Horizontal Filament-stacking by Layer-less FDM*, International SFF Symposium 2015,
  [PDF](https://www.kanadas.com/3dprint/HelicalSFF.pdf).
- Ao, X., Lin, S., Liu, J., Xia, H., Meng, J., *Modeling and self-supporting printing simulation of fuse filament
  fabrication*, Scientific Reports 14 (2024) 16275,
  [DOI 10.1038/s41598-024-67200-9](https://doi.org/10.1038/s41598-024-67200-9).
- He, H., Zhu, Z., Zhang, Y. X., Yang, R. (C.), *Investigation on Bridging Defects in 3D-Printed Polylactic Acid Beams
  Using Fused Filament Fabrication*, Polymers 18(2) (2026) 261,
  [DOI 10.3390/polym18020261](https://doi.org/10.3390/polym18020261) (desde `docs/RESEARCH_MAP.md`).
- Jiang, J., Stringer, J., Xu, X., Zhong, R. Y., *Investigation of printable threshold overhang angle in
  extrusion-based additive manufacturing for reducing support waste*, International Journal of Computer Integrated
  Manufacturing 31(10) (2018) 961–969, [DOI 10.1080/0951192X.2018.1466398](https://doi.org/10.1080/0951192X.2018.1466398)
  (desde `docs/RESEARCH_MAP.md`).
- Mueller, S., Im, S., Gurevich, S., Teibrich, A., Pfisterer, L., Guimbretière, F., Baudisch, P., *WirePrint: 3D
  printed previews for fast prototyping*, UIST 2014, 273–280,
  [DOI 10.1145/2642918.2647359](https://doi.org/10.1145/2642918.2647359) (solo el resumen de la página del proyecto).

**Descripción del autor y documentación pública** (solo la descripción en palabras; ningún código). Las dos primeras
son portadas de repositorios con código GPL: no se enlazan, y quien las busque lee solo el texto de la portada, sin
abrir ningún archivo:
- McCulloch, S., *Arc Overhang*, página principal del proyecto, 2022–2023 (se leyó solo el texto de la portada; el
  repositorio tiene código GPL y no se enlaza).
- Wachenschwan, N., *Arc Overhang PrusaSlicer integration*, página principal del proyecto, 2023 (solo el texto de la
  portada; tampoco se enlaza).
- CNC Kitchen (S. Hermann), *Arc Overhangs – A new way of printing without supports*, 2022,
  [nota](https://www.cnckitchen.com/blog/arc-overhangs-a-new-way-of-printing-without-supports).
- CNC Kitchen, *Arc Overhangs in PrusaSlicer*, 2023,
  [nota](https://www.cnckitchen.com/blog/arc-overhangs-in-prusaslicer).
- Hackaday, *Arc Overhangs Make "Impossible" 3D Prints*, 2022-12-21,
  [nota](https://hackaday.com/2022/12/21/arc-overhangs-make-impossible-3d-prints/), y *More On Wave Overhangs For 3D
  Printing*, 2026-08-16, [nota](https://hackaday.com/2026/08/16/more-on-wave-overhangs-for-3d-printing/).
- 3Dnatives, *Support-Free 3D Printing with 90° Overhang Angles?*, 2026-06-15,
  [nota](https://www.3dnatives.com/en/support-free-3d-printing-research-15062026/).
- VoxelMatters, *Wave-based toolpaths enable support-free horizontal overhangs in extrusion 3D printing*, 2026,
  [nota](https://www.voxelmatters.com/wave-based-toolpaths-enable-support-free-horizontal-overhangs-in-extrusion-3d-printing/)
  (no se pudo abrir; cifras desde el buscador).
- Fabbaloo, *Support-Free Horizontal FFF Overhangs with Wave-Based Toolpaths*, 2026-07-01,
  [nota](https://www.fabbaloo.com/?p=238439) (no se pudo abrir; cifras desde el buscador).
- 3D Printing Journal, *90-degree overhang printing without supports*, 2026,
  [nota](https://www.3dprintingjournal.com/p/90-degree-overhang-printing-without).
- WaveOverhangs, [página del proyecto](https://waveoverhangs.com/) (solo la descripción).
- La ficha `docs/spec/features/arc_overhangs.md`, y `gcode_motion.md` y `cooling.md` para el cordón de los puentes y
  el ventilador.

**No se pudo leer:**
- el artículo de Andersons et al. y su preimpresión (la editorial y SSRN bloquean el acceso automático);
- las notas de VoxelMatters, Fabbaloo y All3DP;
- la tesis de Kaiser (no se encontró el texto);
- el video del autor.

Las cifras de las ondas salen de notas de prensa y hay que confirmarlas en el artículo.
