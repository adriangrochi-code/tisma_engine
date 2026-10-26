# Mabel: la pieza de prueba propia

**Qué hace:** una pieza de prueba de calidad propia del programa, como el Benchy pero con mecanismos. Tiene la forma
simplificada de una impresora de oficina grande, de las de papel. Cada parte prueba algo de la impresora o de los
ajustes:
- voladizos;
- puentes;
- agujeros y paredes finas;
- texto y superficies de arriba;
- medidas exactas;
- **piezas que se mueven impresas ya armadas:** una bandeja que se desliza, una tapa con bisagra y rodillos que
  giran.

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

1. **Tamaño:** 60,00 × 45,00 × 40,00 mm (X × Y × Z) a escala 100 %, con la bandeja de salida. Entra en cualquier
   cama de 100 × 100 mm o más.
   - a 100 %, unos 30 a 45 minutos en una impresora rápida con capas de 0,2 mm (estimado; se mide con el motor);
   - escala 50 % («Mabel mini»): sin los mecanismos, que no funcionan tan chicos (regla 4);
   - otras escalas se permiten; los mecanismos se apagan solos si su holgura queda por debajo de 0,2 mm.
2. **Orientación:** de pie, con las patas en la cama. No necesita soportes: todo voladizo está a 45° o menos de la
   vertical, salvo las partes que son pruebas de voladizo (regla 3).
3. **Partes y qué prueba cada una:**

| Parte | Forma | Medidas (escala 100 %) | Qué prueba |
|---|---|---|---|
| Patas | Cuatro cilindros en las esquinas, bajo el cuerpo | Ø 5 mm, de Z 0 a 1,5 mm, con chaflán de 0,5 mm abajo | Pata de elefante, primera capa, piezas chicas |
| Cuerpo | Caja con las esquinas verticales redondeadas, a la derecha | 46,00 × 45,00 mm (X de 14 a 60), de Z 1,5 a 37 mm; radio 3 mm | Paredes rectas, esquinas, costura, medidas en X e Y |
| Tapa del escáner | Tapa arriba, con bisagra atrás | 46 × 31 × 3 mm (de Z 37 a 40); bisagra de pasador Ø 3 mm, holgura 0,3 mm | Bisagra impresa armada; superficie de arriba |
| Vidrio del escáner | Hueco en el cuerpo, bajo la tapa | 40 × 25 mm, 1 mm de hondo | Puente grande (la tapa); superficie de arriba |
| Panel de control | El borde de arriba al frente, cortado a 30° | 46 × 12 mm (Y de 0 a 12) | Superficie inclinada; ZAA (`z_antialiasing.md`) |
| Botones | Agujeros redondos en el panel | Ø 1, 2 y 3 mm, 1 mm de hondo | Agujeros chicos, retracción |
| Pantalla | Hueco rectangular en el panel | 10 × 6 mm, 0,4 mm de hondo | Hueco poco profundo, de dos capas |
| Bandeja de salida | Placa que sale del costado izquierdo del cuerpo, con la cara de abajo en voladizo | Sale 14 mm (X de 0 a 14), 30 mm de ancho en Y, 1,2 mm de espesor; tres tramos de 10 mm con la cara de abajo a 30°, 45° y 60° de la vertical | Voladizos a distintos ángulos sin soporte |
| Ranura de salida | Ranura en el costado izquierdo, sobre la bandeja | 30 mm en Y × 6 mm de alto | Puente de 30 mm |
| Rodillos | Dos rodillos dentro de la ranura, en un eje a lo largo de Y | Ø 4 mm, 12 mm de largo cada uno; eje Ø 2 mm; holgura 0,3 mm | Piezas que giran impresas armadas; ejes horizontales |
| Cajones de papel | Cuatro cajones apilados al frente, abajo, que se deslizan, impresos adentro del cuerpo, como las bandejas de una impresora de producción | Cada uno 30 × 30 × 4 mm (X × Y × Z); holguras de 0,2, 0,3, 0,4 y 0,5 mm, de abajo hacia arriba, con el número grabado al frente y tope al final | Tolerancia de piezas que se deslizan: el primero que se mueve dice la holgura mínima de la impresora |
| Hojas | Láminas de pie dentro del cajón de 0,4 mm | 5 láminas de 0,6 mm separadas 0,6 mm, 3 mm de alto | Separación entre partes, hilos; el cajón tiene que seguir deslizando |
| Texto | «MABEL» en relieve al frente, sobre el cajón; «TISMA» grabado atrás | Letras de 5 mm de alto, 0,6 mm de relieve o de hondo | Texto en relieve y grabado |
| Rejilla | Ranuras verticales en el costado derecho | 6 paredes de 0,4, 0,6, 0,8, 1,0, 1,2 y 1,6 mm, 15 mm de alto | Paredes finas; ancho mínimo |
| Puertos | Agujeros horizontales atrás, abajo | Ø 3 y 5 mm redondos; Ø 5 mm en lágrima | Agujeros horizontales (el techo cae); comparación con la lágrima |

   **Medidas de referencia** para el calibre: el cuerpo, 46,00 × 45,00 mm; el alto total, 40,00 mm; el puerto
   redondo, Ø 5,00 mm.

4. **Mecanismos** (cajones, tapa y rodillos):
   - se imprimen ya armados, en un solo objeto. Los cajones tienen las holguras de la tabla; la tapa y los rodillos,
     0,3 mm (diseño);
   - el programa los marca como partes del mismo objeto, para que el orden de impresión y la costura no los peguen;
   - en «Mabel mini» se reemplazan por relieves fijos con la misma forma.
5. **Colores** (opcional): el modelo trae las partes separadas (cuerpo, tapa, bandejas, panel y rodillos) para
   imprimirlo en varios colores (`multimaterial.md`). En una sola boquilla se imprime de un color.

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
9. **Simular las partes móviles:** después de laminar, «Simular mecanismos» revisa cada parte móvil con lo que de
   verdad se va a imprimir, no con el modelo:
   - **cada parte dice cómo se mueve:** los cajones se deslizan hacia el frente hasta su tope, la tapa gira 100° en la
     bisagra y los rodillos giran en su eje. Lo trae Mabel; en otra pieza, la persona lo marca en la parte (regla 10);
   - **la holgura real, capa por capa:** se calcula con los cordones laminados (su ancho y su posición), la distancia
     entre la parte móvil y el cuerpo. Se muestra la mínima de cada parte y en qué capa está;
   - **pegada o libre:** si en alguna capa los cordones se tocan o se pisan, la parte queda «pegada» y se marca en rojo
     en la vista previa, con la capa. Si la holgura real queda por debajo de 0,1 mm, «justa», en amarillo (diseño);
   - **el movimiento en la vista:** un deslizador mueve la parte por su recorrido (abre el cajón, levanta la tapa,
     gira el rodillo) y pinta en rojo donde choca con lo impreso;
   - **antes de imprimir:** dice qué cajones se van a mover con esos ajustes («Se moverán los cajones 0,3, 0,4 y 0,5
     mm; el de 0,2 mm queda pegado»), así se puede corregir el ancho de cordón o la compensación en XY
     (`slicing.md`) sin gastar filamento.
10. **En cualquier pieza:** la simulación sirve para toda pieza con partes impresas armadas. En la lista de objetos, una
    parte se marca «Móvil» con su movimiento: deslizar (dirección y largo) o girar (eje y ángulo). Sin marcar nada, no
    se simula.
11. **Lo impreso enseña:** en «Revisar Mabel» (regla 8) se marca qué cajones se movieron. El programa compara con la
    simulación y guarda en el perfil de la impresora la holgura mínima que funciona («holgura de piezas móviles»). Si
    la simulación dijo «libre» y el cajón quedó pegado, propone revisar el flujo y la pata de elefante.
12. **En la biblioteca:** Mabel está en la biblioteca de Tisma Cloud (`model_library.md`) con laminados preparados
   para las impresoras más usadas. Así cualquiera puede comparar su impresión con las de otros en la misma máquina.

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| QM1 | Generar Mabel al 100 % | Ocupa 60,00 × 45,00 × 40,00 mm; malla cerrada; un objeto con sus partes |
| QM2 | Laminar con soportes apagados | Ningún aviso de voladizo sin soporte salvo los de la bandeja de 60°, que son la prueba |
| QM3 | Las holguras de los mecanismos | Con boquilla de 0,4 y ajustes por defecto, el G-code no une con el cuerpo los cajones de 0,3, 0,4 y 0,5 mm, la tapa ni los rodillos en ninguna capa |
| QM4 | Generar Mabel mini | Sin mecanismos; relieves fijos en su lugar; medidas a la mitad |
| QM5 | Escala 60 % | Se apagan los mecanismos cuya holgura quedaría por debajo de 0,2 mm (los cajones de 0,2 y 0,3 mm, la tapa y los rodillos); los cajones de 0,4 y 0,5 mm siguen |
| QM6 | Las medidas de referencia en el G-code | Cuerpo de 46,00 × 45,00 mm por fuera; última capa a Z 40,00; puerto de Ø 5,00 mm |
| QM7 | Rejilla | Las 6 paredes se imprimen; las de 0,4 y 0,6 mm con el ancho mínimo de `perimeters.md` |
| QM8 | «Revisar Mabel» con un error de 0,3 mm en X | Ofrece la calibración de medidas |
| QM9 | Versión de la pieza | El proyecto guarda la versión; abrir un proyecto viejo genera la misma versión |
| QM10 | «Simular mecanismos» con compensación en XY de +0,15 mm | El cajón de 0,2 mm sale «pegado», con su capa; los de 0,4 y 0,5 mm, «libres» |
| QM11 | Deslizador del cajón de 0,3 mm | El cajón se mueve hasta su tope; sin rojo si está libre |
| QM12 | Tapa con la bisagra pegada en una capa | La simulación la marca en rojo en esa capa y la tapa no gira en el deslizador |
| QM13 | Pieza propia con una parte marcada «Móvil» (girar, eje Z, 360°) | Se simula como los rodillos de Mabel |
| QM14 | «Revisar Mabel»: se movieron los cajones de 0,3, 0,4 y 0,5 mm | El perfil de la impresora guarda 0,3 mm como holgura de piezas móviles |
