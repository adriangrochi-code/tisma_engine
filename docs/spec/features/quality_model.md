# Mabel: la pieza de prueba propia

**Qué hace:** una pieza de prueba de calidad propia del programa, como el Benchy pero llena de mecanismos que
funcionan, al estilo de las pruebas de tortura con forma de tostadora. Tiene la forma simplificada de una impresora de
oficina grande, de las de papel. Al sacarla de la cama se abre, se desliza, gira y rueda: los cajones de papel, la
puerta, la tapa del escáner, la pantalla, las bandejas, la palanca, los rodillos y las ruedas. Cada mecanismo
aprovecha una parte real de la impresora para probar algo de la impresora o de los ajustes:
- tolerancias de piezas que se deslizan, giran y ruedan, impresas ya armadas;
- redondez de los círculos (la pantalla gira sobre su eje);
- voladizos, puentes, agujeros, paredes finas, texto y medidas exactas.

Se carga con un clic, se imprime sin soportes y se revisa con una guía que dice qué mirar en cada parte.

**Fase:** después de F5 (necesita soportes apagados, puentes y voladizos bien resueltos), con la interfaz P1.

**Pedido del dueño (26-10-2026):**
- un «Benchy propio» integrado en el programa, con varios mecanismos;
- con la forma de una impresora de papel industrial: la de su trabajo, una Xanté Ilumina Digital Production Press GT
  a la que llaman «Mabel» (según el dueño, unos 80 kg y 2850 hojas por hora);
- simplificada, para no copiar el diseño de nadie.

**Diseño propio, sin copiar:**
- de la impresora del trabajo solo toma la idea general de una impresora grande de producción: un cuerpo de caja,
  bandejas, una tapa de escáner y un panel;
- no reproduce la forma, las proporciones ni los detalles de ningún modelo real;
- no lleva marcas, logos ni nombres de fabricantes (tampoco el de la impresora del trabajo);
- «Mabel» es el nombre que le puso el dueño;
- la geometría la hace el equipo del motor desde las medidas de esta ficha.

Es una pieza distinta del 3DBenchy y no toma nada de él. Del Benchy solo comparte la idea de una pieza única que
prueba muchas cosas.

**Licencia de la pieza:** la del programa, o una más libre si el dueño la elige (por ejemplo, CC0), para que
cualquiera pueda compartir sus impresiones.

## Medidas

1. **Tamaño:** 60,00 × 45,00 × 40,00 mm (X × Y × Z) a escala 100 %, con las bandejas. Entra en cualquier cama de
   100 × 100 mm o más.
   - a 100 %, unos 45 a 60 minutos en una impresora rápida con capas de 0,2 mm (estimado; se mide con el motor);
   - escala 50 % («Mabel mini»): sin los mecanismos, que no funcionan tan chicos (regla 4);
   - otras escalas se permiten; los mecanismos se apagan solos si su holgura queda por debajo de 0,2 mm.
2. **Orientación:** de pie, con las ruedas en la cama. No necesita soportes: todo voladizo está a 45° o menos de la
   vertical, salvo las partes que son pruebas de voladizo (regla 3).
3. **Partes fijas y qué prueba cada una:**

| Parte | Forma | Medidas (escala 100 %) | Qué prueba |
|---|---|---|---|
| Cuerpo | Caja con las esquinas verticales redondeadas, a la derecha | 46,00 × 45,00 mm (X de 14 a 60), de Z 3 a 37 mm; radio 3 mm | Paredes rectas, esquinas, costura, medidas en X e Y |
| Panel de control | El borde de arriba al frente, cortado a 30° | 46 × 12 mm (Y de 0 a 12) | Superficie inclinada; ZAA (`z_antialiasing.md`) |
| Bandeja de salida | Placa fija que sale del costado izquierdo, con la cara de abajo en voladizo | Sale 14 mm (X de 0 a 14), 30 mm en Y, 1,2 mm de espesor; tres tramos de 10 mm con la cara de abajo a 30°, 45° y 60° de la vertical | Voladizos a distintos ángulos sin soporte |
| Ranura de salida | Ranura en el costado izquierdo, sobre la bandeja | 30 mm en Y × 6 mm de alto | Puente de 30 mm |
| Compartimento del tóner | Hueco detrás de la puerta (regla 4) | 18 × 20 × 12 mm de hondo, techo plano | Puente de 18 mm hacia adentro |
| Texto | «MABEL» en relieve al frente, sobre los cajones; «TISMA» grabado atrás | Letras de 5 mm de alto, 0,6 mm de relieve o de hondo | Texto en relieve y grabado |
| Rejilla | Ranuras verticales atrás, arriba | 6 paredes de 0,4, 0,6, 0,8, 1,0, 1,2 y 1,6 mm, 15 mm de alto | Paredes finas; ancho mínimo |
| Puertos | Agujeros horizontales atrás, abajo | Ø 3 y 5 mm redondos; Ø 5 mm en lágrima | Agujeros horizontales (el techo cae); comparación con la lágrima |

   **Medidas de referencia** para el calibre: el cuerpo, 46,00 × 45,00 mm; el alto total, 40,00 mm; el puerto
   redondo, Ø 5,00 mm; la pantalla, Ø 14,00 mm.

4. **Mecanismos** (todos impresos ya armados, en la misma impresión; nada se pega después):

| Mecanismo | Cómo funciona | Medidas (escala 100 %) | Qué prueba |
|---|---|---|---|
| Cajones de papel | Cuatro cajones apilados al frente, abajo, que se deslizan hacia afuera hasta un tope, como las bandejas de una impresora de producción | Cada uno 30 × 30 × 4 mm; holguras de 0,2, 0,3, 0,4 y 0,5 mm, de abajo hacia arriba, con el número grabado al frente | Tolerancia al deslizar: el primero que se mueve dice la holgura mínima de la impresora |
| Hojas | Láminas de pie dentro del cajón de 0,4 mm, que salen con el cajón | 5 láminas de 0,6 mm separadas 0,6 mm, 3 mm de alto | Separación entre partes e hilos; el cajón tiene que seguir deslizando |
| Pantalla giratoria | Disco táctil que gira sobre un perno vertical en el panel, como la pantalla de una impresora grande que se orienta hacia quien la usa | Disco Ø 14,00 mm, 2 mm de espesor; perno Ø 4 mm; holgura 0,3 mm; marcas cada 30° en el borde y un aro de referencia en el panel | Redondez: si el círculo sale ovalado o con facetas, el disco se traba en algún ángulo o roza el aro. Cara de arriba lisa |
| Puerta frontal | Puerta a la derecha de los cajones, con bisagra vertical (perno en Z), que abre 90° y muestra el compartimento del tóner | 14 × 20 × 1,6 mm; perno Ø 2,5 mm; holgura 0,3 mm; un tope para que no gire de más | Bisagra vertical (la más fácil); puerta delgada que no se deforma |
| Tapa del escáner | Tapa arriba, con bisagra horizontal atrás (perno a lo largo de X); abre hasta 100° y deja ver el vidrio | 46 × 31 × 3 mm; bisagra de pasador Ø 3 mm; holgura 0,3 mm; vidrio: hueco de 40 × 25 × 1 mm debajo | Bisagra horizontal (el agujero del perno es un agujero horizontal); puente grande de la tapa; superficie de arriba |
| Bandeja manual | Bandeja del costado derecho, impresa plegada contra la pared, que se baja a horizontal sobre una bisagra a lo largo de Y | 25 × 30 × 1,2 mm; bisagra Ø 2,5 mm; holgura 0,3 mm; traba en la posición abierta | Bisagra con traba; pared fina y alta impresa en vertical |
| Palanca de atascos | Palanca chica en el costado izquierdo, que gira 45° sobre un perno horizontal y vuelve contra un tope | 12 mm de largo; perno Ø 2 mm; holgura 0,3 mm | Perno chico horizontal; la pieza móvil más chica |
| Botón de encendido | Botón cilíndrico en el panel que se hunde 1 mm y no se sale, retenido por un reborde | Ø 4 mm en un agujero de Ø 4,6 mm; reborde de 0,6 mm | Holgura en vertical; pieza encerrada |
| Rodillos | Dos rodillos dentro de la ranura de salida, en un eje a lo largo de Y, que giran con el dedo | Ø 4 mm, 12 mm de largo cada uno; eje Ø 2 mm; holgura 0,3 mm | Ejes horizontales; giro libre |
| Ruedas | Cuatro ruedas bajo el cuerpo, en horquillas, sobre las que Mabel rueda por la mesa | Ø 6 mm, 3 mm de ancho; eje Ø 2 mm; holgura 0,3 mm; apoyan en la cama (Z de 0 a 6) | Primera capa y pata de elefante (la rueda toca la cama y tiene que girar); ejes horizontales chicos |

   - El programa marca cada mecanismo como parte del mismo objeto, para que el orden de impresión y la costura no los
     peguen (la costura no va en las caras que rozan).
   - Holgura de 0,3 mm en todos salvo los cajones, que tienen la escala de 0,2 a 0,5 mm (diseño).
   - En «Mabel mini» los mecanismos se reemplazan por relieves fijos con la misma forma.
5. **Colores** (opcional): el modelo trae las partes separadas (cuerpo, puerta, tapa, pantalla, bandejas, cajones,
   rodillos y ruedas) para imprimirlo en varios colores (`multimaterial.md`). En una sola boquilla se imprime de un
   color.

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
   - Una bisagra pegada pide revisar el flujo y la holgura.
10. **En la biblioteca:** Mabel está en la biblioteca de Tisma Cloud (`model_library.md`) con laminados preparados
   para las impresoras más usadas. Así cualquiera puede comparar su impresión con las de otros en la misma máquina.

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| QM1 | Generar Mabel al 100 % | Ocupa 60,00 × 45,00 × 40,00 mm; malla cerrada; un objeto con sus partes |
| QM2 | Laminar con soportes apagados | Ningún aviso de voladizo sin soporte salvo los de la bandeja de 60°, que son la prueba |
| QM3 | Las holguras de los mecanismos | Con boquilla de 0,4 y ajustes por defecto, el G-code no une con el cuerpo los cajones de 0,3, 0,4 y 0,5 mm ni ninguno de los otros mecanismos, en ninguna capa |
| QM4 | Generar Mabel mini | Sin mecanismos; relieves fijos en su lugar; medidas a la mitad |
| QM5 | Escala 60 % | Se apagan los mecanismos cuya holgura quedaría por debajo de 0,2 mm (los de 0,3 mm y los cajones de 0,2 y 0,3 mm); los cajones de 0,4 y 0,5 mm siguen |
| QM6 | Las medidas de referencia en el G-code | Cuerpo de 46,00 × 45,00 mm por fuera; última capa a Z 40,00; puerto de Ø 5,00 mm; pantalla de Ø 14,00 mm |
| QM7 | Rejilla | Las 6 paredes se imprimen; las de 0,4 y 0,6 mm con el ancho mínimo de `perimeters.md` |
| QM8 | «Revisar Mabel» con un error de 0,3 mm en X | Ofrece la calibración de medidas |
| QM9 | Versión de la pieza | El proyecto guarda la versión; abrir un proyecto viejo genera la misma versión |
| QM10 | Los ejes y pernos horizontales (tapa, bandeja manual, palanca, rodillos, ruedas) | En cada capa del agujero hay holgura alrededor del perno; el techo del agujero no se apoya en el perno |
| QM11 | La pantalla en el G-code | El disco y el aro son círculos separados por la holgura en todas sus capas |
| QM12 | Las ruedas en la primera capa | La rueda y su horquilla no comparten cordones en la primera capa |
| QM13 | «Revisar Mabel»: se movieron los cajones de 0,3, 0,4 y 0,5 mm | El perfil de la impresora guarda 0,3 mm como holgura de piezas móviles |
| QM14 | «Revisar Mabel»: la pantalla se traba | La guía ofrece revisar la redondez y la calibración de medidas |
