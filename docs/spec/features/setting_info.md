# Ficha de cada ajuste (botón de información)

**Qué hace:** el ícono (i) de cada ajuste abre una ventana en el centro de la pantalla con todo lo que hay que saber de
ese ajuste: qué es, en qué afecta, con dibujos y una comparación hecha con el motor, los valores recomendados, los
ajustes relacionados y los problemas que causa o arregla. Vale para todos los ajustes de impresión, filamento e
impresora.

**Pedido del dueño (29-10-2026):** «cuando le das al info, un recuadro mucho más informativo, un menú en el centro de
la pantalla que te explique con detalle, que te muestre imágenes: si es el ancho de línea, qué es, en qué afecta… y así
con todo»; «aunque una persona que nunca usó un laminador entre, pueda tener toda la información y el paso a paso de
cómo, de qué manera y qué función activa cada cosa».

**Fase:** P1 (interfaz); el contenido crece con el paquete de conocimiento (`assistant.md`, `knowledge.tar.xz`).

**Relación con otras fichas:**
- el texto corto de hoy (la ayuda que sale al pasar el mouse) queda como está: es el resumen de esta ficha;
- `assistant.md`: los problemas y procedimientos a los que se enlaza;
- `accessibility_touch.md`: en modo táctil se abre igual, al tocar (i); colores, contraste y teclado de esa ficha;
- `credits.md`: lo que se toma de terceros lleva «Basado en …».

## La ventana

1. **Dónde y cómo:** ventana en el centro, de 720 × 560 px como mínimo (o casi toda la pantalla si es más chica),
   con el fondo oscurecido. Se cierra con Esc, con la cruz o tocando afuera. Se puede mover y agrandar. Con Tab se
   recorre; el lector de pantalla lee el título y las secciones (`accessibility_touch.md`, regla 9).
2. **Arriba:** el nombre del ajuste, dónde está (por ejemplo «Calidad › Ancho de línea»), la clave del motor en chico
   (`walls.line_width`) y el valor actual con su unidad, el valor por defecto y el rango.
3. **Secciones**, en este orden, cada una con su título grande (sin flechas, como pidió el dueño para los menús):
   1. **Qué es:** dos o tres frases simples, escritas para alguien que nunca usó un laminador; si usa una palabra
      técnica, la explica o la enlaza al glosario (regla 7b).
   1b. **Paso a paso:** cuándo tocar este ajuste y cómo, en pasos numerados: «1. Si las paredes salen con huecos…
      2. Subí el valor de a 0,05 mm… 3. Laminá y mirá la vista previa… 4. Imprimí la prueba de calibración…». Dice qué
      activa o desactiva el ajuste (por ejemplo, «al prender Soportes aparecen Estilo, Separación e Interfaz») y qué
      otros ajustes cambian solos con él.
   2. **Dibujo:** ver regla 4.
   3. **En qué afecta:** una fila por cada efecto con una flecha que sube o baja al aumentar el valor: calidad de
      superficie, resistencia, tiempo de impresión, material usado, adherencia, detalle fino, riesgo de fallas. Cada
      efecto con una frase de por qué.
   4. **Valores recomendados:** para la boquilla y el filamento elegidos (por ejemplo, ancho de línea entre 100 % y
      120 % de la boquilla), con de dónde sale la recomendación.
   5. **Comparación:** ver regla 5.
   6. **Se lleva con:** los ajustes relacionados, con su valor actual y un enlace a su ficha (ancho de línea →
      altura de capa, caudal, velocidad, caudal máximo de la boquilla).
   7. **Problemas:** los problemas del panel de ayuda donde este ajuste es causa o arreglo («Hilos», «Pared débil»),
      con enlace.
   8. **Calibración:** si hay una prueba que lo mide (`calibrations.md`), el botón «Calibrar».
   9. **Fuentes:** de dónde salen los datos y los dibujos, con «Basado en …» cuando corresponde.
4. **Dibujos propios, hechos por el programa:** no se copian imágenes de otros programas ni de wikis (licencias). Cada
   ajuste tiene un dibujo que el programa arma con el valor actual, y se redibuja si se cambia el valor desde la
   ventana:
   - cordones (ancho, altura, solape, caudal): el corte de dos o tres cordones vistos de frente, con las medidas;
   - paredes, superficies y relleno: la capa vista desde arriba, con el patrón real;
   - costura, orden, viajes y retracción: una vista de arriba con las flechas del recorrido;
   - soportes, puentes y voladizos: un corte de costado de una pieza de ejemplo;
   - temperaturas, ventilador y velocidades: una curva o una barra con el valor y la zona recomendada;
   - impresora: un dibujo de la cama, el cabezal o el extrusor con la medida marcada.
   Los dibujos usan los colores de la vista previa y siguen `ui.color_vision`.
5. **Comparación con el motor:** dos o tres miniaturas de una pieza de prueba chica (un cubo de 20 mm, un voladizo o
   una pared fina, según el ajuste), laminadas por el motor con el valor actual, uno menor y uno mayor (por ejemplo
   0,4, 0,45 y 0,6 mm de ancho), en la vista previa por tipo de línea, con el tiempo y el material de cada una. Se
   laminan en segundo plano al abrir la ventana; mientras tanto se ve «Preparando la comparación…». Se guardan en
   caché por ajuste y valor. Una miniatura se agranda con un clic y se puede girar.
6. **Acciones, abajo:** «Ir al ajuste» (cierra y lleva al campo, resaltado), «Usar este valor» (si se cambió en la
   comparación o en el dibujo, con deshacer), «Restablecer» (al valor del perfil), «Ver en la vista previa» (abre la
   vista previa de la pieza cargada con las líneas que dependen de este ajuste resaltadas).
7b. **Glosario y primeros pasos:**
   - un glosario con las palabras del laminado (cordón, capa, perímetro, relleno, voladizo, puente, retracción,
     caudal…), cada una con su dibujo; las palabras del glosario se ven subrayadas en todas las fichas y se explican
     al pasar el mouse o al tocarlas;
   - un recorrido «Mi primera impresión» que lleva paso a paso por los ajustes del nivel Básico, en el orden en que
     conviene mirarlos, abriendo la ficha de cada uno; se puede dejar y retomar;
   - las dos cosas se abren desde el botón de ayuda y desde cada ficha.
7. **Búsqueda:** el buscador de arriba también busca dentro de estas fichas («hilos» encuentra retracción,
   temperatura y viajes, aunque no estén en el nombre).

## Contenido

8. **Uno por ajuste:** cada clave de `docs/CONFIG.md` que tiene un campo en la interfaz tiene su ficha, en español e
   inglés. Mientras una no esté escrita, la ventana muestra lo que hay (nombre, valor, rango, texto corto) y «Esta
   ficha todavía no tiene la explicación completa».
9. **De dónde sale:** el texto vive en el paquete de conocimiento (el mismo de `assistant.md`), un archivo por
   ajuste, con estos campos: `what`, `effects` (lista de efecto, sentido y frase), `recommended` (por boquilla o
   material, con fuente), `related`, `problems`, `calibration`, `sources`, `drawing` (el tipo de dibujo de la regla 4)
   y `compare` (la pieza de prueba y los valores). El dibujo y la comparación los arma el programa: el archivo solo
   dice cuáles.
10. **Reglas del texto:** frases cortas, una idea por frase, sin jerga sin explicar; cada dato numérico con su
    fuente; nada inventado (regla del repositorio de perfiles: «cada valor tiene una fuente»). Lo tomado de un tercero
    lleva «Basado en …» (`credits.md`).
11. **Orden de trabajo:** primero las fichas de los ajustes del nivel Básico (las que más se usan), después Normal y
    después Avanzado.

## Pruebas

| # | Caso | Resultado |
|---|---|---|
| SI1 | (i) de `walls.line_width` con boquilla 0,4 | Ventana centrada con las nueve secciones; corte de los cordones con 0,45 mm marcado; comparación de 0,4 / 0,45 / 0,6 con tiempo y material |
| SI2 | Cambiar el valor desde la ventana y «Usar este valor» | El campo toma el valor; Ctrl+Z lo vuelve atrás |
| SI3 | (i) de un ajuste sin ficha escrita | Nombre, valor, rango, texto corto y el aviso de la regla 8 |
| SI4 | Recorrer todas las claves con campo | Cada una abre su ventana sin error; informe de cuántas tienen ficha completa en es y en en |
| SI5 | Buscar «hilos» | Aparecen retracción, temperatura de la boquilla y viajes |
| SI6 | Modo táctil y solo teclado | Se abre al tocar (i); con Tab y Esc se recorre y se cierra |
| SI8 | Persona sin experiencia: abrir «Mi primera impresión» | Recorre los ajustes de Básico en orden, cada uno con su ficha y su paso a paso; al salir y volver, sigue donde quedó |
| SI9 | Ficha de `support.enabled` | El paso a paso dice qué ajustes aparecen al prenderlo; las palabras «voladizo» e «interfaz» enlazan al glosario |
| SI7 | `ui.color_vision` `deuteranopia` | Dibujos y miniaturas con la paleta de `accessibility_touch.md` |
