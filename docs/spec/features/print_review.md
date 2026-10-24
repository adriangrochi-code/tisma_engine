# Revisión antes de imprimir

**Qué hace:** después de laminar, lista los problemas que la impresión probablemente va a tener. Cada problema dice
qué es, qué se encontró (con números), qué hacer y en qué capas pasa, así la vista previa salta a esa capa. Avisa y
nunca bloquea: decide la persona.

**Fase:** después de F4. La diseñó y escribió el equipo del dueño en Tisma Slicer.

## Resultado

Una lista de avisos. Cada uno tiene:
- tipo y gravedad: crítico, advertencia o información;
- título, descripción con números y sugerencia;
- objetos afectados (vacío = toda la impresión);
- Z del G-code de la capa a mostrar primero, y el rango de capas;
- cuántos lugares, capas o movimientos.

Orden: los más graves primero; a igual gravedad, de abajo hacia arriba. Va también en el JSON de avisos
(`BEHAVIOR.md`).

## Avisos y reglas

1. **Detalles perdidos** (advertencia):
   - partes de la sección de una capa donde no entra ninguna extrusión, encadenadas con las de la capa de abajo que se
     superponen;
   - se avisa solo si el detalle se pierde en 3 capas seguidas o más. La última capa de un techo en punta es una
     astilla que también se pierde, y no es un detalle del modelo.
2. **Puentes largos** (advertencia):
   - para cada extrusión de puente, el tramo más largo sin nada debajo;
   - «debajo» es la capa de abajo, agrandada medio ancho de cordón donde el puente se apoya;
   - los puentes sobre el relleno o sobre capas sólidas tienen la capa de abajo debajo y no cuentan;
   - se avisa si el tramo supera `support.free_bridge_length`, lo que puede tender esa impresora.
3. **Capas sin tiempo para enfriarse** (advertencia): capas cuyo tiempo final (con aceleraciones) queda por debajo de
   `cooling.min_layer_time` aun después del frenado de `cooling.md`, por ejemplo por puentes que no se frenan. No se
   revisa con el enfriamiento apagado. Se muestra en la capa más corta.
4. **Viajes por el aire sin retracción** (advertencia):
   - viajes que salen de la pieza: fuera de las islas de la capa y de los soportes, agrandadas un diámetro de
     boquilla. Se mira cada 1 mm, salvo el primer y el último milímetro;
   - no cuentan los viajes hacia o desde la falda, el borde, la torre de purga o el G-code propio;
   - solo se avisan los que van sin retracción. Retraer mientras se limpia cuenta como retracción;
   - no se revisa con impresoras de cinta ni capas no planares: su G-code no está en las coordenadas de las capas.
5. **Velocidades sobre el límite** (información): movimientos cuya velocidad en X o en Y supera la máxima de la
   máquina (con un margen de 0,1 %). La impresora los frena: tarda más que lo estimado.
6. **Estabilidad** (crítico o advertencia), solo en objetos sin soportes:
   - partes que empiezan en el aire;
   - voladizos grandes que caen o se imprimen mal;
   - extrusiones que se levantan;
   - pieza que se puede despegar de la cama;
   - partes finas que se pueden quebrar mientras se imprimen.

   Este análisis tiene que ser diseño propio del motor, a partir de la física (centro de masa, área de apoyo,
   adherencia a la cama, rigidez de la parte ya impresa). Tisma Slicer usa ahí código ajeno que no se describe.

## Pruebas

Escenarios de las pruebas de Tisma Slicer: pieza, ajustes y aviso esperado.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| R1 | Cubo común | — | Ningún aviso que no sea de información |
| R2 | Un bloque sobre la cama y, al lado, uno chico que empieza 10 mm arriba de la cama | sin soportes | «Parte en el aire», crítico, primero en la lista, mostrado entre Z 10 y 15 |
| R3 | Una pared de 0,05 mm de espesor y 5 mm de alto al lado de un bloque | — | Un «detalle perdido» que llega hasta su última capa, debajo de Z 5,2 |
| R4 | Un cono en punta | — | Sin «detalle perdido»: la punta no cuenta |
| R5 | Dos pilares a 30 mm unidos por una tabla: puente de 30 mm sobre el aire | `support.free_bridge_length` por defecto | «Puente largo» entre Z 10 y 10,5 |
| R6 | La pieza de R5 | `support.free_bridge_length` 35 | Sin aviso |
| R7 | Lo mismo con un puente de 10 mm | por defecto | Sin aviso |
| R8 | Un bloque con relleno poco denso: sus primeras capas sólidas sobre el relleno son puentes | — | Hay puentes en el G-code, pero sin «puente largo» |
| R9 | Un pilar fino que no llega a 5 s por capa a 60 mm/s | enfriamiento encendido | «Sin tiempo para enfriarse» en más de 10 capas. Con el enfriamiento apagado, sin aviso |
| R10 | Dos pilares separados: en cada capa se viaja de uno al otro | con retracción | Sin aviso |
| R11 | La pieza de R10 | retracción mientras limpia | Sin aviso |
| R12 | La pieza de R10 | sin retracción | «Viajes por el aire», advertencia, 20 o más |
| R13 | Un solo bloque | sin retracción | Sin aviso: los viajes quedan sobre la pieza (el de la falda no cuenta) |
| R14 | Una pieza con velocidades altas | velocidades sobre el máximo de la máquina en X e Y | «Velocidades sobre el límite» en más de 10 movimientos. Con los límites de la máquina desactivados, sin aviso |
