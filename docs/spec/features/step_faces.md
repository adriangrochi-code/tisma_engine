# STEP con caras: pintar, cargas y volver a convertir en triángulos

**Qué hace:** una pieza abierta desde un STEP recuerda de qué cara del sólido sale cada triángulo y guarda el STEP en el
proyecto. Con eso:
- se pinta y se eligen apoyos y cargas por cara entera;
- se vuelve a convertir en triángulos con otra finura sin perder lo pintado;
- se revisa si el sólido tiene errores.

**Fase:** después de la primera versión, con Ingeniería (`structural_analysis.md`).

**Fuentes:**
- ISO 10303 (STEP): un sólido se describe por sus caras, cada una con su superficie y sus bordes (representación por
  bordes, «B-rep»). Las partes 203, 214 y 242 son las que escriben los programas de diseño;
- lo que hace Tisma Slicer, de la documentación del dueño y de su interfaz: el diálogo de calidad al abrir un STEP,
  «Tessellate again…» y «Check CAD geometry» en el menú de la pieza.

**Relación con otras fichas:**
- `formats.md`, regla 2: abrir un STEP. Esta ficha agrega lo que se guarda además de los triángulos.
- `model_tools.md`, regla 27: las herramientas de pintura.
- `structural_analysis.md`: caras fijas y cargas.

## Al abrir un STEP

1. **Calidad de la conversión:** si la preferencia «Show STEP file import parameters» está encendida
   (`preferences.md`), un diálogo «STEP import quality» pregunta la finura antes de abrir:

| Opción | Desvío de la superficie | Desvío del ángulo |
|---|---|---|
| Low | 0,005 mm | 1° |
| Medium (por defecto) | 0,003 mm | 0,5° |
| High | 0,001 mm | 0,25° |
| Custom | 0,001 a 0,005 mm | 0,25° a 1° |

   - **Desvío de la superficie:** lo más que un triángulo se aparta de la superficie real.
   - **Desvío del ángulo:** lo más que gira la superficie dentro de un triángulo.
   - Valores más chicos dan más triángulos.
   - **Remember my choice:** usa esa finura sin preguntar.
   - **Apply to all:** con varios STEP a la vez, usa la misma finura para todos.
2. **Cara por triángulo:** cada triángulo sabe de qué cara del sólido sale.
   - Las caras se numeran en el orden en que aparecen en el archivo. El mismo archivo da siempre la misma numeración.
   - La reparación de la malla (`formats.md`, regla 4) puede dar vuelta, mover o borrar triángulos, y cada triángulo
     que queda conserva su cara.
3. **El STEP se guarda en el proyecto:** el 3MF lleva el archivo STEP y, por cada pieza, la cara de cada triángulo,
   la finura usada y la posición de la pieza al convertirla.
   - Van en entradas propias del 3MF, que otros programas no leen. Un 3MF sin ellas se abre igual, como una malla
     común.
   - El nombre y el formato de esas entradas los elige el equipo del motor y los documenta en `docs/`.
   - Dos piezas del mismo STEP comparten el archivo guardado: no se guarda dos veces.
4. **Revisión del sólido:** al abrir, el motor revisa el sólido (caras abiertas, bordes sueltos, caras que se cruzan,
   superficies mal definidas).
   - Si encuentra errores, avisa con cuántos hay y de qué tipo. La pieza se abre igual.
   - El aviso ofrece «Check CAD geometry» (regla 11).

## Pintar y elegir por cara

5. **Herramienta «cara entera»** (diseño): un pincel más en `model_tools.md`, regla 27, solo en piezas que vienen de
   un STEP.
   - Un clic pinta todos los triángulos de la cara tocada.
   - Pasar el ratón por encima resalta la cara entera.
6. **En Ingeniería:** elegir las caras fijas y las caras de carga también se hace por cara entera
   (`structural_analysis.md`). La fuerza repartida en una cara usa el área real de la cara.
7. **Una cara chica** (diseño): las caras de menos de 1 mm² se pintan igual, pero se resaltan con un borde para que
   se vean.

## Volver a convertir en triángulos

8. **«Tessellate again…»:** en el menú de la pieza (y del objeto, para todas sus piezas del STEP).
   - Abre el mismo diálogo de la regla 1, con el título «Tessellate again» y el nombre del archivo.
   - Vuelve a convertir el STEP guardado en el proyecto, con la finura nueva. No necesita el archivo original en el
     disco.
9. **Lo que se conserva:**
   - **posición:** la pieza queda donde estaba, aunque se haya movido, girado, escalado o espejado después de abrirla;
   - **pintura por cara:** soporte, costura, colores, piel difusa y aletas (`support_fins.md`);
   - **Ingeniería:** caras fijas y cargas.

   Una cara pintada entera queda pintada entera. Una cara pintada en parte se pasa por cercanía: cada triángulo nuevo
   toma lo del triángulo viejo más cercano de la misma cara.
10. **Cuándo se pierde el vínculo con el STEP:** cortar, dividir, simplificar, agregar texto o hacer cualquier cambio
    que cambie los triángulos de la pieza.
    - La pieza sigue como una malla común, con su pintura.
    - Antes del cambio, el motor avisa (diseño): «Esta pieza deja de estar unida a su STEP: no se podrá volver a
      convertir ni elegir por cara».
    - Deshacer el cambio recupera el vínculo (`model_tools.md`, regla 29).

## Revisar el sólido

11. **«Check CAD geometry»:** en el menú de la pieza. Muestra el resultado de la revisión de la regla 4:
    - «sin errores», o la lista de errores, cada uno con su tipo y la cara;
    - un clic en un error resalta su cara en la vista 3D (diseño).

## Volver a cargar desde el disco

12. **El mismo STEP:** «Recargar desde el disco» (`model_tools.md`, regla 30) con el archivo sin cambios conserva todo,
    como la regla 9.
13. **Un STEP cambiado** (diseño): si la persona cambió el diseño, la numeración de las caras puede cambiar.
    - La pintura y las cargas pasan por cara solo si la cantidad de caras es la misma y el área de cada cara cambió
      menos del 1 %.
    - Si no, pasan por cercanía, como en una malla común, y el motor avisa: «El STEP cambió: conviene revisar la pintura
      y las cargas».

## Línea de comandos

14. Abrir un STEP desde la línea de comandos usa la finura Medium, sin preguntar.

## Pruebas

Piezas de prueba: las genera el equipo del motor con un programa de diseño libre y se guardan en las pruebas del motor
con su fuente.

| # | Escenario | Resultado esperado |
|---|---|---|
| SC1 | Abrir un cubo STEP de 20 mm en Medium | 6 caras; cada triángulo tiene su cara; la pieza está cerrada |
| SC2 | El mismo cubo en Low y en High | High tiene igual o más triángulos que Low; las 6 caras siguen |
| SC3 | Cilindro STEP de radio 10 mm en Low, Medium y High | Más triángulos de Low a High; ningún vértice se aparta del cilindro más que el desvío de la superficie |
| SC4 | Guardar el proyecto y abrirlo | La pieza sigue unida a su STEP; «Tessellate again…» está activo |
| SC5 | El mismo proyecto en otro laminador | Se abre como malla común, sin error |
| SC6 | Pintar entera la cara de arriba del cubo y volver a convertir en High | La cara de arriba sigue pintada entera; ninguna otra cara pintada |
| SC7 | Mover, girar 30° y escalar al 150 % el cubo; volver a convertir | Misma posición, tamaño y pintura |
| SC8 | Espejar en X una pieza asimétrica y volver a convertir | Sigue espejada; la pintura en las mismas caras |
| SC9 | Cara fija abajo y carga en la cara de arriba; volver a convertir | Ingeniería da el mismo veredicto |
| SC10 | Cortar el cubo por la mitad | Aviso de la regla 10; las dos mitades sin vínculo; Deshacer lo recupera |
| SC11 | STEP con una cara que falta | Se abre; aviso con un error; «Check CAD geometry» lo lista con su cara |
| SC12 | Dos copias del mismo STEP en el proyecto | El STEP se guarda una sola vez en el 3MF |
| SC13 | Recargar desde el disco un STEP con un agujero agregado | Aviso de la regla 13; la pintura pasa por cercanía |
| SC14 | Abrir el cubo STEP desde la línea de comandos | Sin preguntas; la misma cantidad de triángulos que en Medium |
