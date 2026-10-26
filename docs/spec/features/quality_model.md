# Mabel: la pieza de prueba propia

**Qué hace:** una pieza de prueba de calidad propia del programa, como el Benchy pero llena de mecanismos que
funcionan, al estilo de las pruebas de tortura con forma de tostadora. Es una miniatura simplificada de la impresora de
producción del trabajo del dueño: la impresora sobre su pie, con el alimentador de papel a un lado y la cinta de
salida al otro. Al sacarla de la cama se abre, se desliza, sube, gira y rueda. Cada mecanismo aprovecha una parte de
la impresora real para probar algo de la impresora 3D o de los ajustes:
- tolerancias de piezas que se deslizan, giran, suben y ruedan, impresas ya armadas;
- redondez de los círculos (la pantalla gira sobre su eje);
- voladizos (las patas en X de la cinta), puentes (la cinta entre las patas), agujeros, paredes finas, columnas
  finas, texto y medidas exactas.

Se carga con un clic, se imprime sin soportes y se revisa con una guía que dice qué mirar en cada parte.

**Fase:** después de F5 (necesita soportes apagados, puentes y voladizos bien resueltos), con la interfaz P1.

**Pedido del dueño (26-10-2026):**
- un «Benchy propio» integrado en el programa, lleno de mecanismos;
- que se parezca lo más posible a la impresora de su trabajo, una Xanté Ilumina Digital Production Press GT a la que
  llaman «Mabel», pero sin su imagen de marca.

**Fuentes de la forma:**
- el folleto oficial de la Ilumina Digital Production Press GT (Xanté, 2010): 233 × 62 × 46,2 cm con la cinta de
  salida (ancho × fondo × alto), 69 kg sin opciones, alimentador de alta capacidad de 4000 hojas, bandeja universal de
  530 hojas, salida por cinta transportadora de velocidad variable;
- las fotos del mismo folleto y de un vendedor de equipos usados: de ellas salen solo la disposición y las
  proporciones (qué parte va dónde), no medidas exactas.

**Parecida, sin su imagen de marca:**
- toma la disposición y las proporciones generales de la impresora real;
- no lleva logos, nombres, el color del panel ni textos de la marca; el panel va con el violeta de Tisma;
- es una miniatura simplificada, no una copia: los detalles se reemplazan por pruebas (rejillas, agujeros, textos
  propios);
- «Mabel» es el nombre que le puso el dueño;
- la geometría la hace el equipo del motor desde las medidas de esta ficha.

Es una pieza distinta del 3DBenchy y no toma nada de él. Del Benchy solo comparte la idea de una pieza única que
prueba muchas cosas.

**Licencia de la pieza:** la del programa, o una más libre si el dueño la elige (por ejemplo, CC0), para que
cualquiera pueda compartir sus impresiones.

## Medidas

1. **Tamaño:** 88,00 × 36,00 × 48,00 mm (X × Y × Z) a escala 100 %. Entra en cualquier cama de 100 × 100 mm o más.
   De izquierda a derecha: la cinta de salida (X de 0 a 30), la impresora sobre su pie (X de 30 a 66) y el
   alimentador (X de 66 a 88). El frente es Y = 0.
   - a 100 %, de 1,5 a 2 horas en una impresora rápida con capas de 0,2 mm (estimado; se mide con el motor);
   - escala 50 % («Mabel mini»): sin los mecanismos, que no funcionan tan chicos (regla 4);
   - otras escalas se permiten; los mecanismos se apagan solos si su holgura queda por debajo de 0,2 mm.
2. **Orientación:** de pie, como la impresora real. No necesita soportes: todo voladizo está a 45° o menos de la
   vertical, salvo las partes que son pruebas de voladizo (regla 3).
3. **Partes fijas y qué prueba cada una:**

| Parte | Forma | Medidas (escala 100 %) | Qué prueba |
|---|---|---|---|
| Pie de la impresora | Gabinete bajo la impresora, como el pie de la real | X de 32 a 64, Y de 4 a 36, Z de 0 a 22 | Paredes rectas, esquinas, costura, primera capa |
| Cuerpo de la impresora | Caja con los bordes de arriba redondeados | 36,00 × 34,00 mm (X de 30 a 66, Y de 2 a 36), Z de 22 a 44; radio 3 mm arriba | Medidas en X e Y; superficie curva arriba (ZAA, `z_antialiasing.md`) |
| Bandeja de salida de arriba | Hueco en la tapa, como la bandeja de la real | 20 × 16 mm, 2 mm de hondo | Superficie de arriba dentro de un hueco |
| Panel de control | Cara inclinada a 30° adelante a la izquierda del cuerpo | 14 × 8 mm | Superficie inclinada |
| Botones | Agujeros redondos en el panel, en rombo como los de la real | Ø 1, 1,5 y 2 mm, 0,8 mm de hondo | Agujeros chicos, retracción |
| Cinta de salida | Marco abierto arriba, de la ranura de salida de la impresora hacia la izquierda | X de 2 a 30, Y de 10 a 30, Z de 30 a 33 | Puente de 18 mm entre los pares de patas |
| Patas en X | Dos pares de patas cruzadas bajo la cinta, como las de la real | Varillas de 1,6 × 1,6 mm a 45°, de Z 0 a 30 | Voladizo a 45° en piezas finas; columnas que vibran |
| Bandeja final de la cinta | Bandeja al final de la cinta, levantada, con la cara de abajo en voladizo | 12 × 20 × 1,2 mm; tres tramos con la cara de abajo a 30°, 45° y 60° de la vertical | Voladizos a distintos ángulos sin soporte |
| Base del alimentador | Caja baja a la derecha | X de 66 a 88, Y de 6 a 34, Z de 0 a 14 | Paredes y esquinas |
| Torre del alimentador | Dos rieles verticales sobre la base | Z de 14 a 48 | Paredes altas y finas |
| Varillas guía | Dos varillas al frente del alimentador, como las de la real | Ø 1,6 mm, 20 mm de alto | Columnas finas, hilos entre ellas, retracción |
| Texto | «MABEL» en relieve al frente del cuerpo; «TISMA» grabado atrás | Letras de 4 mm de alto, 0,6 mm de relieve o de hondo | Texto en relieve y grabado |
| Rejilla | Ranuras atrás del cuerpo | 6 paredes de 0,4, 0,6, 0,8, 1,0, 1,2 y 1,6 mm, 12 mm de alto | Paredes finas; ancho mínimo |
| Puertos | Agujeros horizontales atrás del pie | Ø 3 y 5 mm redondos; Ø 5 mm en lágrima | Agujeros horizontales (el techo cae); comparación con la lágrima |

   **Medidas de referencia** para el calibre: el cuerpo de la impresora, 36,00 × 34,00 mm; el alto total, 48,00 mm;
   el puerto redondo, Ø 5,00 mm; la pantalla, Ø 8,00 mm.

4. **Mecanismos** (todos impresos ya armados, en la misma impresión; nada se pega después):

| Mecanismo | Cómo funciona | Medidas (escala 100 %) | Qué prueba |
|---|---|---|---|
| Cajones de papel del pie | Cuatro cajones apilados al frente del pie, que se deslizan hasta un tope, como las bandejas extra de la real | Cada uno 26 × 26 × 4 mm; holguras de 0,2, 0,3, 0,4 y 0,5 mm, de abajo hacia arriba, con el número grabado al frente | Tolerancia al deslizar: el primero que se mueve dice la holgura mínima de la impresora |
| Hojas | Láminas de pie dentro del cajón de 0,4 mm | 5 láminas de 0,6 mm separadas 0,6 mm, 3 mm de alto | Separación entre partes e hilos; el cajón tiene que seguir deslizando |
| Tapa de la impresora | La tapa de arriba, con bisagra atrás (perno a lo largo de X), que abre 90° como la real y deja ver los cuatro tambores de color | 36 × 30 × 4 mm; bisagra Ø 2,5 mm, holgura 0,3 mm; adentro, 4 ranuras de 1,6 mm separadas por paredes de 0,8 mm | Bisagra horizontal; puente grande de la tapa; ranuras y paredes finas |
| Pantalla giratoria | Disco en el panel que gira sobre un perno | Disco Ø 8,00 mm, 1,6 mm de espesor; perno Ø 2,5 mm; holgura 0,3 mm; marcas cada 30° y un aro de referencia | Redondez: si el círculo sale ovalado o con facetas, el disco se traba o roza el aro |
| Puerta delantera | Puerta a la derecha del frente del cuerpo, con bisagra vertical (perno en Z) | 12 × 16 × 1,2 mm; perno Ø 2 mm; holgura 0,3 mm; tope a 90° | Bisagra vertical; puerta delgada que no se deforma |
| Elevador del alimentador | La plataforma con la pila de papel sube y baja entre los rieles de la torre, como el elevador de la real | Plataforma 18 × 22 × 2 mm con una pila de 10 mm (ranuras de hojas al frente); holgura 0,3 mm con los rieles; se imprime abajo sobre tres puntos de corte de 1 mm que se rompen con un empujón | Deslizamiento vertical; puntos de corte |
| Rodillos de la cinta | Cuatro rodillos a lo ancho de la cinta, que giran con el dedo | Ø 3 mm, 18 mm de largo; eje Ø 1,6 mm; holgura 0,3 mm | Ejes horizontales chicos; giro libre |
| Bandeja final plegable | La bandeja final de la cinta se pliega sobre una bisagra a lo largo de Y | Bisagra Ø 2 mm; holgura 0,3 mm; traba arriba y abajo | Bisagra con traba |
| Ruedas del alimentador | Cuatro ruedas bajo la base del alimentador, que lo dejan rodar | Ø 5 mm, 2,5 mm de ancho; eje Ø 1,6 mm; holgura 0,3 mm; apoyan en la cama | Primera capa y pata de elefante (la rueda toca la cama y tiene que girar) |

   - El programa marca cada mecanismo como parte del mismo objeto, para que el orden de impresión y la costura no los
     peguen (la costura no va en las caras que rozan).
   - Holgura de 0,3 mm en todos salvo los cajones, que tienen la escala de 0,2 a 0,5 mm (diseño).
   - En «Mabel mini» los mecanismos se reemplazan por relieves fijos con la misma forma.
5. **Colores** (opcional): el modelo trae las partes separadas (pie, cuerpo, tapa, panel, cajones, cinta con sus
   patas, alimentador y elevador) para imprimirlo en varios colores (`multimaterial.md`): por ejemplo, el cuerpo y el
   alimentador de un color claro, como la real, y el panel en violeta. En una sola boquilla se imprime de un color.

## En el programa

6. **Dónde:** Más › Calibración › «Pieza de prueba Mabel», y en el menú del botón derecho de la placa, «Agregar pieza
   de prueba». La pieza queda centrada en la cama, con la escala elegida (100 % o mini).
7. **Generada, no cargada:** el programa la genera desde estas medidas, como las de `calibration_models.md`. La versión
   de la pieza va en el proyecto, para que dos impresiones de la misma versión se puedan comparar.
8. **Guía de revisión:** después de imprimir, «Revisar Mabel» abre una lista por parte.
   - Cada parte dice qué mirar y qué ajuste cambiar si sale mal, con un enlace a la guía de calibración que
     corresponde (`assistant.md`, reglas 20 a 24).
   - Por ejemplo: «La bandeja a 60° cuelga» → enfriamiento y velocidad de voladizos.
   - Las medidas de referencia (regla 3) se escriben con el calibre, y el programa dice el error en
     X, Y y Z. Si pasa de 0,1 mm, ofrece la calibración de medidas (`calibrations.md`).
9. **Qué se movió:** en «Revisar Mabel» se marca, mecanismo por mecanismo, si se mueve libre, duro o no se mueve.
   - Los cajones dicen la holgura mínima que funciona en esa impresora; el programa la guarda en el perfil de la
     impresora («holgura de piezas móviles»), para usarla en otras piezas con partes armadas.
   - La pantalla: si se traba en algún ángulo, la guía apunta a la redondez de los círculos (resolución de los arcos,
     juego de las correas, compensación de las esquinas) y a la calibración de medidas (`calibrations.md`).
   - Las ruedas: si no giran, casi siempre es la pata de elefante; la guía lleva a su compensación.
   - Las patas en X: si se doblan o tienen hilos, enfriamiento y velocidad de voladizos.
   - Una bisagra pegada pide revisar el flujo y la holgura.
10. **En la biblioteca:** Mabel está en la biblioteca de Tisma Cloud (`model_library.md`) con laminados preparados
   para las impresoras más usadas. Así cualquiera puede comparar su impresión con las de otros en la misma máquina.

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| QM1 | Generar Mabel al 100 % | Ocupa 88,00 × 36,00 × 48,00 mm; malla cerrada; un objeto con sus partes |
| QM2 | Laminar con soportes apagados | Ningún aviso de voladizo sin soporte salvo los de la bandeja final a 60°, que son la prueba |
| QM3 | Las holguras de los mecanismos | Con boquilla de 0,4 y ajustes por defecto, el G-code no une con el cuerpo los cajones de 0,3, 0,4 y 0,5 mm ni ninguno de los otros mecanismos, en ninguna capa |
| QM4 | Generar Mabel mini | Sin mecanismos; relieves fijos en su lugar; medidas a la mitad |
| QM5 | Escala 60 % | Se apagan los mecanismos cuya holgura quedaría por debajo de 0,2 mm (los de 0,3 mm y los cajones de 0,2 y 0,3 mm); los cajones de 0,4 y 0,5 mm siguen |
| QM6 | Las medidas de referencia en el G-code | Cuerpo de 36,00 × 34,00 mm por fuera; última capa a Z 48,00; puerto de Ø 5,00 mm; pantalla de Ø 8,00 mm |
| QM7 | Rejilla | Las 6 paredes se imprimen; las de 0,4 y 0,6 mm con el ancho mínimo de `perimeters.md` |
| QM8 | «Revisar Mabel» con un error de 0,3 mm en X | Ofrece la calibración de medidas |
| QM9 | Versión de la pieza | El proyecto guarda la versión; abrir un proyecto viejo genera la misma versión |
| QM10 | Los ejes y pernos horizontales (tapa, bandeja final, rodillos, ruedas) | En cada capa del agujero hay holgura alrededor del perno; el techo del agujero no se apoya en el perno |
| QM11 | La pantalla en el G-code | El disco y el aro son círculos separados por la holgura en todas sus capas |
| QM12 | Las ruedas en la primera capa | La rueda y su horquilla no comparten cordones en la primera capa |
| QM15 | El elevador del alimentador | Solo los tres puntos de corte lo unen a la base; la holgura con los rieles se ve en todas sus capas |
| QM13 | «Revisar Mabel»: se movieron los cajones de 0,3, 0,4 y 0,5 mm | El perfil de la impresora guarda 0,3 mm como holgura de piezas móviles |
| QM14 | «Revisar Mabel»: la pantalla se traba | La guía ofrece revisar la redondez y la calibración de medidas |
