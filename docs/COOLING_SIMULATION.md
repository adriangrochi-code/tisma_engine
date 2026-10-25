# Simulación del enfriamiento: los detalles chicos

Pedido del dueño (2026-10-25): «simular cómo se enfría el filamento para reducir las pérdidas de calidad en zonas que
sí llegan al tiempo mínimo pero tienen detalles muy chicos».

Este documento explica por qué pasa, junta lo publicado y propone el orden de trabajo (T1 a T4). Lo que ya hace el
motor está en las fichas `cooling.md` y `cooling_more.md`: el tiempo mínimo de capa, el frenado y el ventilador por
capa.

**Sala limpia:** se usan solo los artículos y la documentación de usuario; ningún código.

## El problema

El tiempo mínimo de capa mira la capa entera. Un detalle chico puede tener mucho menos tiempo que la capa, por cuatro
razones:

1. **Vuelta rápida.** El orden de islas por cercanía arranca cada capa donde terminó la anterior. Un detalle que se
   imprime último en una capa se imprime primero en la siguiente: la capa tarda 30 s, pero el detalle recibe la capa
   nueva a los 2 s.
2. **Poco cuerpo.** Una punta, un perno o unas letras en relieve no tienen masa debajo que se lleve el calor; se
   enfrían casi solo por el aire.
3. **La boquilla encima.** Al imprimir un detalle de 2 mm, la boquilla, a más de 200 °C, pasa todo ese tiempo a menos
   de un milímetro y lo recalienta. Es la queja conocida contra «levantar el cabezal» y esperar en las piezas chicas:
   la boquilla que espera encima derrite lo de abajo.
4. **Vecinos.** En un detalle, los cordones se ponen uno junto al otro con menos de un segundo de diferencia.

**Escala de tiempos.** Un cordón de PLA de 0,4 × 0,2 mm, solo al aire, tiene sección A ≈ 0,08 mm² y unos 0,8 mm de
borde libre (P). Con ρ ≈ 1240 kg/m³ y c ≈ 1800 J/(kg·K), su constante de tiempo es `τ = ρ·c·A / (h·P) ≈ 220 / h`
segundos:
- **sin ventilador** (h ≈ 8,5 W/(m²·K), convección natural): unos 26 s;
- **con ventilador** (h de 50 a 100, que depende de la impresora y hay que calibrar): de 2 a 5 s.

Sobre una pieza grande y fría, el cordón se enfría mucho más rápido, por conducción hacia abajo. Sobre un detalle
chico que ya está caliente, no.

## Lo publicado

| Trabajo | Qué modela | Qué sirve para el motor |
|---|---|---|
| Costa, Duarte, Covas, *J. Materials Processing Technology* 245:167–179, 2017 | Solución analítica de la conducción transitoria en cada segmento de filamento. Los contactos se activan según la geometría, el orden de deposición y las condiciones de trabajo: entre segmentos, con la cama y con el aire. Incluye un criterio de adhesión. Validado con temperaturas medidas y con ensayos de despegue | La red de cordones (T2) y el criterio de adhesión |
| Costa, Duarte, Covas, *Virtual and Physical Prototyping* 10(1):35–46, 2015 | Qué condiciones térmicas pesan al depositar: convección, conducción entre filamentos y radiación | Qué términos incluir |
| Zhang, Shapiro, *J. Manufacturing Science and Engineering* 140(7):071002, 2018 | Simulación en tiempo lineal de piezas FDM. Solo se actualizan los elementos recién depositados; los lejanos casi no cambian y se ponen al día con fórmulas analíticas cuando hace falta. El número de Biot de un cordón es menor que 0,1 | El conjunto activo, la puesta al día perezosa, y una temperatura por cordón |
| Zhang, Vasiliauskaite, De Kuyper et al., *3D Printing and Additive Manufacturing* 9(2):132–142, 2022 | Mediciones de temperatura en paredes simples. Bi ≈ 0,026; h ≈ 8,5 W/(m²·K) sin ventilador; emisividad 0,78. **Regla de semejanza:** con el mismo tiempo entre capas, el mismo campo de temperatura. Con menos tiempo entre capas, más temperatura al recalentar, e irregularidades en la superficie | Lo que manda es el tiempo entre pasadas sobre el mismo lugar, no el tiempo de la capa |
| Ramos, Mittermeier, Kiendl, *J. Manufacturing Processes* 94:550–563, 2023 | Elementos finitos con activación híbrida y engrosado adaptativo, también para rellenos con aire | La versión fina, para comparar |
| Bellehumeur, Li, Sun, Gu, *J. Manufacturing Processes* 6(2):170–178, 2004; Sun et al., *Rapid Prototyping J.*, 2008 | La unión entre filamentos crece mientras la interfaz está caliente, y la resistencia depende de esa historia de temperatura | El lado de la resistencia: si se enfría de más, pega peor |
| Grafos para metales (Yavari, Rao et al.: LPBF, DED, BAAM) | El calor en un grafo de nodos en vez de una malla: minutos en vez de horas | Otra forma de la red, para piezas grandes |
| Fathizadan et al., ASME 2020 e *IISE Transactions* 54(9), 2022; ORNL | Impresión de gran tamaño: el tiempo de capa según la temperatura medida o simulada, con un enfriamiento exponencial y una temperatura objetivo | La misma idea, pero por zona |
| Skeinforge, complemento Cool (documentación, 2008); Cura, «levantar el cabezal» | Esperar con la boquilla en órbita alrededor de la isla más grande, o lejos de la pieza, hasta cumplir el tiempo mínimo. Skeinforge baja además la temperatura de la boquilla en las capas cortas | Antecedentes de la pausa de enfriamiento |

## Patentes

Dos solicitudes de Siemens Industry Software tocan esto:
- **US 2023/0234136 A1**, *Heat-aware toolpath reordering for 3D printing of physical parts*, con base en
  PCT/EP2019/085918 (diciembre de 2019):
  - reivindica reordenar los tramos de una capa según una medida de criticidad térmica, para bajar el impacto del calor;
  - puede que esté concedida como US 12521795; no lo pude confirmar.
- **US 2023/0280717 A1**, *Heat-aware toolpath generation for 3D printing of physical parts*, presentada el
  19-06-2020: dividir la capa en zonas y elegir su orden según un criterio térmico.

Las páginas de patentes no dejan leer el texto completo de forma automática. Estas reivindicaciones salen de resúmenes
y no son una opinión legal.

**Consecuencia para el motor:**
- no reordena islas ni tramos según el calor simulado mientras no lo revise el abogado de patentes, que ya estaba
  previsto antes de vender;
- lo de abajo cambia la velocidad, el ventilador o agrega pausas, pero no el orden;
- no busqué todas las patentes: el abogado revisa también el frenado y el ventilador por zona.

## Propuesta para el motor

### T1 — Tiempo de vuelta local

Geometría y reloj, sin física:
- **El tiempo local:** para cada punto de un cordón, el tiempo desde que se puso el cordón de abajo. El reloj sale del
  estimador de tiempo del escritor, y el cordón de abajo de una grilla por capa, como la que usa el relleno ondulado
  para `below`.
- **La boquilla cerca:** también cuenta el tiempo que la boquilla pasó sobre el punto, dentro del radio de su cara
  plana, de alrededor de 1 mm.
- **Se muestra:**
  - un modo de color en la vista previa;
  - un aviso en la revisión antes de imprimir, «detalle sin tiempo para enfriarse», que dice el lugar. Se suma al
    aviso por capa de `print_review.md`.
- **Se corrige sin reordenar:**
  - se frenan solo los cordones del detalle, hasta que su tiempo local llega al mínimo (`cooling.min_detail_time`; por
    defecto, el tiempo mínimo de capa);
  - si así quedaría por debajo de la velocidad mínima, se hace una **pausa de enfriamiento**: retraer, salir de la caja
    de la pieza y esperar, como la órbita de Skeinforge o el cabezal levantado de Cura, pero lejos;
  - el ventilador va al máximo mientras se imprime el detalle.
- **Costo:** una pasada más sobre el G-code planeado. Va primero.

### T2 — Red térmica de cordones

Es la simulación propiamente dicha.

**Modelo:**
- **Los nodos:** cada tramo de cordón de hasta 1 mm, o del doble del ancho, es un nodo con una sola temperatura, porque
  Bi ≪ 1.
- **El nacimiento:** el nodo nace a la temperatura de salida, que es la de la boquilla menos una caída a calibrar, en
  el instante que da el reloj del escritor.
- **Los intercambios:**
  - con el aire, por las caras libres: convección con un h que depende del ventilador, más radiación con ε ≈ 0,78;
  - con los cordones que toca, abajo y a los costados: `k·A_contacto / d`, con d la distancia entre centros;
  - con la cama, en la primera capa.
- **La boquilla:** calienta los cordones que tiene debajo mientras pasa. Es lo que el tiempo mínimo de capa no ve.
- **La integración:**
  - implícita (Euler hacia atrás), estable con pasos del largo de cada movimiento;
  - solo sobre los nodos activos, que son los de las últimas capas y los que siguen calientes;
  - los demás se ponen al día con la exponencial cuando algo los toca (Zhang y Shapiro).
- **Escala:** unos 10⁵ nodos para 100 m de extrusión. Se mide.

**Resultados por cordón:**
- la temperatura que tiene cuando lo tapa el de arriba;
- el tiempo que cada unión pasa por encima de la transición vítrea (`glass_transition` del catálogo).

**Criterios:**
- **blando** (calidad): cuando llega el cordón de arriba, el de abajo sigue por encima de la temperatura de
  ablandamiento: el detalle se deforma. Esa temperatura sale de `heat_deflection` del catálogo, o se calibra;
- **frío** (resistencia): la unión pasó poco tiempo caliente y pega mal (Bellehumeur et al.; el criterio de adhesión
  de Costa et al.).

**Calibración:** el h con ventilador depende de la impresora y del conducto:
- una pieza de calibración con pernos de 2, 3, 4, 6 y 8 mm, todos a la misma velocidad;
- el usuario marca el más fino que salió bien;
- el motor ajusta h para que ese perno quede justo en el límite.

Con una cámara térmica se calibra mejor.

**Pruebas:**
- un bloque solo contra la exponencial;
- un cordón sobre una placa fría contra la solución analítica en 1D;
- un perno contra un cubo con el mismo tiempo de capa: el perno queda más caliente.

### T3 — Correcciones con la simulación

- **Calidad:** las mismas correcciones de T1 (frenado local, pausa y ventilador por zona), pero decididas por la
  temperatura y no solo por el tiempo.
- **Resistencia:** donde la unión queda fría, menos ventilador en esa zona. Pasa en piezas grandes y lentas con
  ventilador fuerte.
- **Análisis estructural:** el análisis «como impresa» (A1, N3) toma la adherencia de cada unión de la temperatura
  simulada, en vez de un valor fijo por material.
- **Orden:** reordenar según el calor solo si el abogado lo permite.

### T4 — GPU y el aire real

- La red de T2 en Vulkan compute (Jacobi con colores), probada contra la versión de CPU.
- Con A3, el aire del ventilador sobre la pieza (Lattice Boltzmann) da un h por cara: las caras a la sombra del
  conducto se enfrían menos.

### Varios ejes

La red trabaja con los puntos 3D de las trayectorias y busca los contactos por cercanía en 3D, no por «la capa de
abajo». Por eso sirve igual para:
- el relleno ondulado;
- las capas curvas de F7;
- los 4 y 5 ejes de F9.

En 4 y 5 ejes, el conducto del ventilador gira con el cabezal. El h de cada cara depende entonces de la orientación de
la boquilla, que cada punto ya guarda (`PLAN.md`, «Diseño para la visión»).

## Fuentes

- Costa, Duarte, Covas, *Estimation of filament temperature and adhesion development in fused deposition techniques*,
  J. Materials Processing Technology 245:167–179, 2017,
  [DOI 10.1016/j.jmatprotec.2017.02.026](https://doi.org/10.1016/j.jmatprotec.2017.02.026)
  ([repositorio de la Universidad do Minho](https://repositorium.sdum.uminho.pt/bitstream/1822/53016/1/2017-Estimation%20of%20filament%20temperature%20and%20adhesion%20development%20in%20fused%20deposition%20techniques.pdf)).
- Costa, Duarte, Covas, *Thermal conditions affecting heat transfer in FDM/FFE: a contribution towards the numerical
  modelling of the process*, Virtual and Physical Prototyping 10(1):35–46, 2015,
  [DOI 10.1080/17452759.2014.984042](https://doi.org/10.1080/17452759.2014.984042).
- Zhang, Shapiro, *Linear-Time Thermal Simulation of As-Manufactured Fused Deposition Modeling Components*, J.
  Manufacturing Science and Engineering 140(7):071002, 2018
  ([ASME](https://asmedigitalcollection.asme.org/manufacturingscience/article/140/7/071002/473198/Linear-Time-Thermal-Simulation-of-As-Manufactured)).
- Zhang et al., *Temperature Analyses in Fused Filament Fabrication: From Filament Entering the Hot-End to the Printed
  Parts*, 3D Printing and Additive Manufacturing 9(2):132–142, 2022,
  [DOI 10.1089/3dp.2020.0339](https://doi.org/10.1089/3dp.2020.0339)
  ([PMC](https://pmc.ncbi.nlm.nih.gov/articles/PMC9831539/)).
- Ramos, Mittermeier, Kiendl, *Efficient simulation of the heat transfer in fused filament fabrication*, J.
  Manufacturing Processes 94:550–563, 2023, [DOI 10.1016/j.jmapro.2023.03.030](https://doi.org/10.1016/j.jmapro.2023.03.030)
  ([arXiv 2305.03455](https://arxiv.org/abs/2305.03455)).
- Bellehumeur, Li, Sun, Gu, *Modeling of bond formation between polymer filaments in the fused deposition modeling
  process*, J. Manufacturing Processes 6(2):170–178, 2004.
- Sun, Rizvi, Bellehumeur, Gu, *Effect of processing conditions on the bonding quality of FDM polymer filaments*,
  Rapid Prototyping Journal 14(2), 2008, [DOI 10.1108/13552540810862028](https://doi.org/10.1108/13552540810862028).
- Fathizadan, Ju, Wang, Rowe, Hofmann, *Dynamic material deposition control for large-scale additive manufacturing*,
  IISE Transactions 54(9):817–831, 2022 ([IDEAS](https://ideas.repec.org/a/taf/uiiexx/v54y2022i9p817-831.html)).
- Grafos para metales: *Accelerated graph theory thermal modeling for big area additive manufacturing with
  experimental validation*, J. Manufacturing Processes, 2025
  ([ScienceDirect](https://www.sciencedirect.com/science/article/abs/pii/S1526612525012757)).
- Skeinforge, complemento Cool, documentación de 2008
  ([copia](https://secure.diyhpl.us/cgit/reprap-backup/plain/trunk/reprap/miscellaneous/python-beanshell-scripts/documentation/skeinforge_tools.craft_plugins.cool.html)).
- Siemens Industry Software, US 2023/0234136 A1 ([resumen y reivindicaciones](https://ipiq.io/patents/325090)) y
  US 2023/0280717 A1 ([Justia](https://patents.justia.com/patent/20230280717)).
