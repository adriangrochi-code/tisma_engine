# Mabel: la pieza de prueba propia

**Qué hace:** una pieza de prueba de calidad propia del programa, como el Benchy pero llena de mecanismos que
funcionan, al estilo de las pruebas de tortura con forma de tostadora. Es una miniatura simplificada de la impresora de
producción del trabajo del dueño, sola, sin el pie, el alimentador ni la cinta de salida. Al sacarla de la cama se le
abren la tapa y la puerta del costado, se saca la bandeja de papel, se inclina el panel y giran las perillas. Cada
mecanismo aprovecha una parte de la impresora real para probar algo de la impresora 3D o de los ajustes:
- tolerancias de piezas que se deslizan y giran, impresas ya armadas;
- redondez de los círculos, acostados (las perillas de los cartuchos) y de pie (la perilla de encendido);
- voladizos (el alero de la tapa), puentes (la ranura de salida y el tope de papel), agujeros, paredes finas, texto y
  medidas exactas.

Se carga con un clic, se imprime sin soportes y se revisa con una guía que dice qué mirar en cada parte.

**Fase:** después de F5 (necesita soportes apagados, puentes y voladizos bien resueltos), con la interfaz P1.

**Pedido del dueño (26-10-2026):**
- un «Benchy propio» integrado en el programa, lleno de mecanismos;
- que se parezca lo más posible a la impresora de su trabajo, una Xanté Ilumina Digital Production Press GT a la que
  llaman «Mabel», pero sin su imagen de marca;
- con una foto de la impresora marcó qué es Mabel: solo la impresora, no el pie ni lo que tiene a los costados.

**Fuente de la forma:** esa foto del dueño, de la impresora sola vista desde adelante a la izquierda. De ella salen
solo la disposición y las proporciones (qué parte va dónde), no medidas exactas. Lo que se ve en la foto:
- un cuerpo casi cúbico, un poco más ancho que hondo, con la tapa de arriba redondeada hacia el frente;
- arriba, una bandeja de salida hundida; atrás a la izquierda, un bloque más alto con la ranura de salida y sus
  rodillos a la vista; atrás al centro, un tope de papel con forma de arco; a la derecha, ranuras de ventilación;
- al frente a la izquierda, un panel de control inclinado, con una pantalla rectangular y botones; arriba del panel, la
  manija hundida de la tapa; en el medio del frente, una placa chica con el nombre;
- abajo, la bandeja de papel en todo el ancho, con la manija abajo al centro y una ventanita del nivel de papel a la
  derecha;
- al costado derecho, una puerta grande con manija (la bandeja universal, que se abre hacia abajo); en la esquina de
  adelante, una palanca vertical; atrás, una perilla redonda sobre una placa cuadrada y una rejilla de ventilación;
  abajo, un panel con dos aberturas chicas.

**Parecida, sin su imagen de marca:**
- toma la disposición y las proporciones generales de la impresora real;
- no lleva logos, nombres, el color naranja del panel ni textos de la marca; la placa del nombre dice «MABEL» y el
  panel va con el violeta de Tisma;
- es una miniatura simplificada, no una copia: los detalles se reemplazan por pruebas (rejillas, agujeros, textos
  propios);
- «Mabel» es el nombre que le puso el dueño;
- la geometría la hace el equipo del motor desde las medidas de esta ficha.

Es una pieza distinta del 3DBenchy y no toma nada de él. Del Benchy solo comparte la idea de una pieza única que
prueba muchas cosas.

**Licencia de la pieza:** la del programa, o una más libre si el dueño la elige (por ejemplo, CC0), para que
cualquiera pueda compartir sus impresiones.

## Medidas

1. **Tamaño:** 60,00 × 54,00 × 44,00 mm (X × Y × Z) a escala 100 %. Entra en cualquier cama de 100 × 100 mm o más.
   El frente (el del panel y la bandeja de papel) es Y = 0; el costado de la puerta grande es X = 60.
   - a 100 %, de 2 a 3 horas en una impresora rápida con capas de 0,2 mm (estimado; se mide con el motor);
   - escala 50 % («Mabel mini»): sin los mecanismos, que no funcionan tan chicos (regla 4);
   - otras escalas se permiten; los mecanismos se apagan solos si su holgura queda por debajo de 0,2 mm.
2. **Orientación:** de pie, como la impresora real, con la bandeja de papel sobre la cama. No necesita soportes:
   - todo voladizo está a 45° o menos de la vertical, salvo las partes que son pruebas de voladizo y de puente;
   - entre una pieza móvil y la fija, las caras de abajo van a 45° o verticales: nunca una cara plana sobre la
     holgura, que se pegaría (diseño);
   - los huecos cerrados de adentro (sobre la bandeja de papel y detrás de la puerta) tienen techo a dos aguas a 45°.
3. **Partes fijas y qué prueba cada una:**

| Parte | Forma | Medidas (escala 100 %) | Qué prueba |
|---|---|---|---|
| Base | La parte de abajo del cuerpo, alrededor de la bandeja de papel | X de 0 a 60, Y de 0 a 54, Z de 0 a 12 | Paredes rectas, esquinas, costura, primera capa |
| Cuerpo | Caja con la tapa redondeada hacia el frente, como la real | 60,00 × 54,00 mm, Z de 12 a 36; radio 5 mm en el borde de arriba al frente | Medidas en X e Y; superficie curva arriba (ZAA, `z_antialiasing.md`) |
| Alero de la tapa | El borde de adelante de la tapa sobresale del frente, como en la real | 3 mm hacia adelante; la cara de abajo en tres tramos a 30°, 45° y 60° de la vertical | Voladizos a distintos ángulos sin soporte |
| Bandeja de salida | Hueco en la tapa | 30 × 20 mm, 2 mm de hondo, el piso inclinado 5° hacia atrás | Superficie de arriba dentro de un hueco; superficie poco inclinada |
| Bloque de salida | Bloque más alto atrás a la izquierda, con la ranura de salida hacia adelante | X de 2 a 36, Y de 40 a 54, Z de 36 a 44; ranura de 30 × 3 mm | Puente de 30 mm sobre la ranura |
| Tope de papel | Arco sobre la tapa, atrás al centro | X de 36 a 52, Y de 44 a 50, Z de 36 a 44; luz de 10 mm | Puente corto; arco |
| Panel de control | Al frente a la izquierda, inclinado 30° hacia atrás, sobresale 4 mm | X de 4 a 24, Z de 16 a 26 | Superficie inclinada |
| Pantalla y botones | Pantalla rectangular hundida; botones en dos columnas a su derecha, como los de la real | Pantalla 8 × 5 mm, 0,6 mm de hondo; botones Ø 1, 1,5 y 2 mm, 0,8 mm de hondo | Agujeros chicos, retracción |
| Manija de la tapa | Hueco arriba del panel | X de 6 a 22, Z de 28 a 32, 2 mm de hondo, techo a 45° | Hueco horizontal sin soporte |
| Placa del nombre | En el medio del frente, donde la real lleva su marca | «MABEL» en relieve, letras de 3 mm de alto y 0,5 mm de relieve | Texto chico en relieve |
| Ventanita del nivel de papel | Abajo a la derecha del frente de la bandeja | 2 × 5 mm, pasante | Agujero chico rectangular |
| Ranuras de ventilación | Arriba a la derecha | 5 ranuras de 0,4, 0,6, 0,8, 1,0 y 1,2 mm de ancho, 10 mm de largo | Separaciones chicas; hilos |
| Rejilla del costado | Costado derecho, atrás, debajo de la perilla de encendido | Y de 43 a 52, Z de 12 a 23; 6 paredes de 0,4, 0,6, 0,8, 1,0, 1,2 y 1,6 mm | Paredes finas; ancho mínimo |
| Puertos | Agujeros horizontales atrás de la base | Ø 3 y 5 mm redondos; Ø 5 mm en lágrima | Agujeros horizontales (el techo cae); comparación con la lágrima |
| Texto de atrás | «TISMA» grabado atrás del cuerpo | Letras de 4 mm de alto, 0,6 mm de hondo | Texto grabado |

   **Medidas de referencia** para el calibre: el cuerpo, 60,00 × 54,00 mm; el alto total, 44,00 mm; el puerto
   redondo, Ø 5,00 mm; las perillas de los cartuchos, Ø 6,00 mm; la perilla de encendido, Ø 5,00 mm.

4. **Mecanismos** (todos impresos ya armados, en la misma impresión; nada se pega después):

| Mecanismo | Cómo funciona | Medidas (escala 100 %) | Qué prueba |
|---|---|---|---|
| Bandeja de papel | La bandeja de abajo se saca hacia adelante hasta un tope, como la real; se imprime sobre la cama y corre en rieles en V a 45° de las paredes de la base | 48 × 44 × 9 mm (X de 6 a 54); holgura 0,3 mm; manija abajo al centro | Deslizamiento largo; pata de elefante (la bandeja toca la cama al lado de la base) |
| Hojas | Láminas de pie dentro de la bandeja | 5 láminas de 0,6 mm separadas 0,6 mm, 4 mm de alto | Separación entre partes e hilos; la bandeja tiene que seguir deslizando |
| Escala de holguras | En las dos aberturas de abajo del costado derecho, dos pestañas en cada una que se deslizan hacia afuera hasta un tope | Cada pestaña 8 × 4 × 2 mm; holguras de 0,2, 0,3, 0,4 y 0,5 mm, con el número grabado | Tolerancia al deslizar: la primera que se mueve dice la holgura mínima de la impresora |
| Tapa de arriba | Toda la tapa, con la bandeja de salida, abre hacia arriba 90° con la bisagra atrás (perno a lo largo de X, delante del bloque de salida), como la real | 56 × 38 mm; bisagra Ø 2,5 mm, holgura 0,3 mm; abajo, un hueco de 44 × 20 mm | Bisagra horizontal; puente de 20 mm sobre el hueco de los cartuchos |
| Cartuchos y sus perillas | Debajo de la tapa, cuatro cartuchos fijos de los cuatro colores, cada uno con una perilla de traba que gira sobre un perno vertical | Perilla Ø 6,00 mm, 2 mm de alto, con una barra; perno Ø 2,5 mm; holgura 0,3 mm; marcas cada 30° y un aro de referencia | Redondez de círculos acostados (en X e Y): si salen ovalados o con facetas, la perilla se traba o roza el aro |
| Panel inclinable | El panel de control se inclina sobre una bisagra en su borde de abajo (perno a lo largo de X) | De 30° a 60° de la vertical, con tope en los dos extremos; perno Ø 2 mm; holgura 0,3 mm | Bisagra con topes en una pieza inclinada |
| Puerta del costado | La puerta grande del costado derecho se abre hacia abajo sobre una bisagra en su borde de abajo (perno a lo largo de Y) y queda horizontal, como la bandeja universal de la real | Y de 8 a 38, Z de 13 a 33, 1,6 mm de espesor; perno Ø 2 mm; holgura 0,3 mm; tope a 90° | Bisagra horizontal; puerta delgada que no se deforma |
| Guías de papel | Dos guías en la cara de adentro de la puerta, que se deslizan a lo largo de Y en una ranura en T | Guías de 2 × 6 mm; holgura 0,3 mm | Deslizamiento en una ranura; piezas chicas |
| Palanca de la puerta | La palanca vertical de la esquina de adelante a la derecha gira sobre un perno vertical y traba la puerta cerrada | 2 × 8 mm; perno Ø 1,6 mm; holgura 0,3 mm; gira 30° | Perno vertical chico; traba |
| Perilla de encendido | Perilla redonda con una barra, sobre una placa cuadrada atrás del costado derecho (Y de 42 a 49, Z de 26 a 33); gira sobre un perno horizontal (a lo largo de X) | Placa 7 × 7 mm; perilla Ø 5,00 mm; perno Ø 2 mm; holgura 0,3 mm; marcas cada 45° | Redondez de círculos de pie (en Y y Z): escalones de las capas y techo del agujero |
| Rodillos de salida | Tres rodillos a lo ancho de la ranura de salida, que giran con el dedo | Ø 3 mm, 8 mm de largo; eje Ø 1,6 mm; holgura 0,3 mm | Ejes horizontales chicos; giro libre |

   - El programa marca cada mecanismo como parte del mismo objeto, para que el orden de impresión y la costura no los
     peguen (la costura no va en las caras que rozan).
   - Holgura de 0,3 mm en todos salvo la escala de holguras, que va de 0,2 a 0,5 mm (diseño).
   - En «Mabel mini» los mecanismos se reemplazan por relieves fijos con la misma forma.
5. **Colores** (opcional): el modelo trae las partes separadas (cuerpo, tapa, panel, bandeja de papel, puerta,
   perillas y cartuchos) para imprimirlo en varios colores (`multimaterial.md`): por ejemplo, el cuerpo de un gris
   claro, como la real, el panel en violeta y cada cartucho de su color. En una sola boquilla se imprime de un color.

## En el programa

6. **Dónde:** Más › Calibración › «Pieza de prueba Mabel», y en el menú del botón derecho de la placa, «Agregar pieza
   de prueba». La pieza queda centrada en la cama, con la escala elegida (100 % o mini).
7. **Generada, no cargada:** el programa la genera desde estas medidas, como las de `calibration_models.md`. La versión
   de la pieza va en el proyecto, para que dos impresiones de la misma versión se puedan comparar.
8. **Guía de revisión:** después de imprimir, «Revisar Mabel» abre una lista por parte.
   - Cada parte dice qué mirar y qué ajuste cambiar si sale mal, con un enlace a la guía de calibración que
     corresponde (`assistant.md`, reglas 20 a 24).
   - Por ejemplo: «El tramo de 60° del alero cuelga» → enfriamiento y velocidad de voladizos.
   - Las medidas de referencia (regla 3) se escriben con el calibre, y el programa dice el error en
     X, Y y Z. Si pasa de 0,1 mm, ofrece la calibración de medidas (`calibrations.md`).
9. **Qué se movió:** en «Revisar Mabel» se marca, mecanismo por mecanismo, si se mueve libre, duro o no se mueve.
   - La escala de holguras dice la holgura mínima que funciona en esa impresora; el programa la guarda en el perfil de
     la impresora («holgura de piezas móviles»), para usarla en otras piezas con partes armadas.
   - Las perillas de los cartuchos: si se traban en algún ángulo, la guía apunta a la redondez en X e Y (resolución de
     los arcos, juego de las correas, compensación de las esquinas) y a la calibración de medidas (`calibrations.md`).
   - La perilla de encendido: si se traba y las de los cartuchos no, el problema está en Z (alto de capa, techo del
     agujero que cae); la guía lleva al enfriamiento y a la forma de lágrima.
   - La bandeja de papel: si no sale, casi siempre es la pata de elefante; la guía lleva a su compensación.
   - Una bisagra pegada (tapa, panel, puerta) pide revisar el flujo y la holgura.
10. **En la biblioteca:** Mabel está en la biblioteca de Tisma Cloud (`model_library.md`) con laminados preparados
   para las impresoras más usadas. Así cualquiera puede comparar su impresión con las de otros en la misma máquina.

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| QM1 | Generar Mabel al 100 % | Ocupa 60,00 × 54,00 × 44,00 mm; malla cerrada; un objeto con sus partes |
| QM2 | Laminar con soportes apagados | Ningún aviso de voladizo sin soporte salvo los del tramo de 60° del alero, que son la prueba |
| QM3 | Las holguras de los mecanismos | Con boquilla de 0,4 y ajustes por defecto, el G-code no une con el cuerpo las pestañas de 0,3, 0,4 y 0,5 mm ni ninguno de los otros mecanismos, en ninguna capa |
| QM4 | Generar Mabel mini | Sin mecanismos; relieves fijos en su lugar; medidas a la mitad |
| QM5 | Escala 60 % | Se apagan los mecanismos cuya holgura quedaría por debajo de 0,2 mm (los de 0,3 mm y las pestañas de 0,2 y 0,3 mm); las pestañas de 0,4 y 0,5 mm siguen |
| QM6 | Las medidas de referencia en el G-code | Cuerpo de 60,00 × 54,00 mm por fuera; última capa a Z 44,00; puerto de Ø 5,00 mm; perillas de Ø 6,00 y 5,00 mm |
| QM7 | Rejilla del costado | Las 6 paredes se imprimen; las de 0,4 y 0,6 mm con el ancho mínimo de `perimeters.md` |
| QM8 | «Revisar Mabel» con un error de 0,3 mm en X | Ofrece la calibración de medidas |
| QM9 | Versión de la pieza | El proyecto guarda la versión; abrir un proyecto viejo genera la misma versión |
| QM10 | Los pernos horizontales (tapa, panel, puerta, perilla de encendido, rodillos) | En cada capa del agujero hay holgura alrededor del perno; el techo del agujero no se apoya en el perno |
| QM11 | Las perillas de los cartuchos en el G-code | La perilla y el aro son círculos separados por la holgura en todas sus capas; ninguna cara plana de la perilla está sobre la holgura |
| QM12 | La bandeja de papel en la primera capa | La bandeja y la base no comparten cordones en la primera capa |
| QM13 | «Revisar Mabel»: se movieron las pestañas de 0,3, 0,4 y 0,5 mm | El perfil de la impresora guarda 0,3 mm como holgura de piezas móviles |
| QM14 | «Revisar Mabel»: se trababan las perillas de los cartuchos | La guía ofrece revisar la redondez en X e Y y la calibración de medidas |
| QM15 | Las guías de papel de la puerta | La holgura con la ranura en T se ve en todas sus capas; ninguna guía queda unida a la puerta |
