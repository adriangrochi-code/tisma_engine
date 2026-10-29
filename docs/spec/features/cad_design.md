# Diseño CAD dentro del programa (basado en SolidCraft)

**Qué hace:** un espacio de trabajo **Diseño** donde la persona dibuja y modela una pieza paramétrica (bocetos con
restricciones, extrusiones, redondeos, agujeros, roscas…) y la manda a la placa sin pasar por otro programa ni por un
STL. La pieza guarda su historial: se cambia una medida y se rehace entera, también después de mandarla a la placa.

**Pedidos del dueño:**
- 29-10-2026: «hay que ver sobre Orca 2.5 alpha qué integra y qué podemos integrar nosotros» (la pestaña Design de
  OrcaSlicer 2.5, ficha `orca_2_5.md`, regla 30);
- 29-10-2026: «manda el trabajo así seguimos basándonos en el CAD en SolidCraft»;
- 29-10-2026: «la idea es que se base en lo más posible al slicer integrando las herramientas de AutoCAD»: el diseño va
  dentro de la misma ventana del programa, no en otro programa, y el boceto se maneja como en los programas de dibujo
  técnico (línea de comandos, coordenadas tipeadas, referencias a objetos, ortogonal y rastreo polar).

**Fase:** después de la primera versión, como función experimental que se prende en Preferencias.

**Base:** SolidCraft (`https://github.com/tangency-labs/solidcraft`, antes `bherbruck/solidcraft`), un CAD paramétrico
de código abierto escrito en Rust, con licencia «MIT OR Apache-2.0, at your option». No es código de PrusaSlicer ni de
Cura: el motor puede usarlo como dependencia o como base, con el crédito «Basado en SolidCraft» y su licencia anotada
en la lista de dependencias. Antes de tomarlo hay que revisar su archivo de licencia y las licencias de lo que trae
adentro (su núcleo B-rep es la biblioteca `truck`; la importación de FreeCAD y Rhino pasa por `cadmpeg`).

**Fuentes:**
- SolidCraft, `README.md`, `docs/capabilities.md`, `docs/features.md`, `docs/scripts.md` y `docs/architecture.md`
  (leídos el 29-10-2026);
- OrcaSlicer, notas de la versión 2.5.0 alpha, sección «Design Tab»
  (`https://www.orcaslicer.com/wiki/releases/release_2_5_0_alpha`): solo el comportamiento, sin código.

**Relación con otras fichas:**
- `model_tools.md`: las formas paramétricas de la placa; con esta ficha pasan a ser primitivas de Diseño;
- `step_reader.md` y `step_faces.md`: el STEP que se abre en la placa y las caras que se conservan al rehacer;
- `project_save.md` y `formats.md`: dónde se guarda el historial;
- `stability.md` y `print_review.md`: el centro de masa y las medidas que da el diseño.

## Ajustes

| Clave | Tipo | Por defecto | Qué hace |
|---|---|---|---|
| `ui.cad_design` | sí/no | no | Muestra el espacio Diseño y las herramientas Boceto y Primitiva en la barra de Preparar |
| `cad.chord_tolerance` | mm | 0,01 | Cuánto se aparta como máximo la malla de la superficie exacta al mandar a la placa |
| `cad.angle_tolerance` | grados | 5 | Ángulo máximo entre facetas vecinas de la malla |

Los dos valores de tolerancia los elige la ficha (la calidad de STEP que ya usa la placa, `step_reader.md`).

## Reglas

### Cómo se integra

1. **Integración, dentro del programa** (pedido del dueño): Diseño es una pestaña más de la misma ventana, con la
   misma vista 3D, la misma cama, los mismos colores y los mismos atajos que Preparar; nunca abre otro programa.
   - De SolidCraft se toma el núcleo, no su ventana: el solucionador de restricciones, la línea de tiempo con
     parámetros, el núcleo B-rep, la teselación y la lectura y escritura de STEP (sus bibliotecas `geom`, `sketch`,
     `kernel`, `doc`, `io` y `engine`), enlazadas al motor por una interfaz en C. La interfaz la dibuja el motor.
   - Mientras eso no esté, se puede empezar mandando guiones JSON de comandos a `solidcraft-cli` (`run`), los mismos
     que usa la ventana de SolidCraft («todo es un comando»), pero el resultado se ve y se edita igual dentro de la
     pestaña Diseño.
   - El motor anota en `PLAN.md` el camino elegido antes de construir.
2. **Crédito:** «Basado en SolidCraft» en Créditos (`credits.md`) y en la ayuda de Diseño, con el enlace al proyecto y
   su licencia.

### El espacio Diseño

3. **Encender:** apagado de entrada. Se prende en Preferencias › General › Funciones con «Diseño CAD (experimental)».
   Con eso aparece la pestaña Diseño al lado de Preparar, y en Preparar dos herramientas: **Boceto** y **Primitiva**.
4. **Bocetos:** sobre un plano de origen, un plano corrido o una cara plana de un cuerpo. Herramientas: línea,
   rectángulo, círculo, arco, polígono, ranura, elipse, curvas spline, cónicas y texto; geometría de construcción,
   proyección, recorte, redondeo y chaflán de esquinas.
5. **Restricciones y cotas:** coincidente, punto sobre curva, horizontal, vertical, paralela, perpendicular, tangente,
   curvatura (G2), igual, concéntrica, colineal, punto medio, simetría y fijo; cotas lineales, alineadas, horizontales,
   verticales, de punto a recta, de recta a círculo, de punto a círculo, entre círculos, radio, diámetro, ángulo,
   barrido y largo de arco, y largo de ranura.
   - una barra muestra cuántos grados de libertad quedan; lo que queda totalmente fijado se dibuja en negro (en el
     tema oscuro, en blanco);
   - una restricción que sobra o contradice a otra se rechaza con un aviso y no se agrega;
   - al arrastrar, el boceto se resuelve en cada cuadro y la pieza se rehace una vez, al soltar.
6. **Perfiles:** las regiones cerradas del boceto se encuentran solas, también las partidas por líneas y los agujeros
   dentro de un contorno.
7. **Operaciones**, en una línea de tiempo: extruir, revolucionar (alrededor de cualquier recta del boceto, también
   una de construcción), barrer, recubrir (loft), redondear y achaflanar (varias aristas en una sola operación),
   vaciar (shell), dar ángulo de desmolde, agujeros (según las tablas de las normas ISO y ANSI), roscas modeladas,
   patrones, simetría, partir, combinar (unión, resta, intersección), mover y geometría de construcción.
8. **Línea de tiempo:** cada operación se puede editar, suprimir, renombrar, mover de lugar y borrar; al cambiar una,
   las de después se rehacen solas. Una operación que falla queda marcada en rojo con el motivo y no se pierden las
   de después: quedan en espera hasta que se arregle.
9. **Parámetros:** variables con nombre y expresiones con unidades (`2 * ancho + 5 mm`, `angulo / 2`), usables en
   cualquier cota u operación. Cambiar un parámetro rehace la pieza.
10. **Texto:** operación propia: fuente instalada, estilo y altura; el contorno se ve mientras se escribe y se guarda
    con el proyecto, así que la pieza se abre igual en una computadora sin esa fuente.
11. **Teclas:** Enter confirma; Esc vuelve un nivel atrás y nunca borra trabajo; deshacer y rehacer funcionan dentro
    del boceto. «Ajustar a la selección» desde la línea de tiempo y la lista de cuerpos.

### Herramientas de dibujo técnico

Como en los programas de dibujo técnico (AutoCAD y sus parecidos): son ideas de uso comunes a esos programas, escritas
acá con nombres propios; el programa no usa marcas ni textos de otros.

12. **Línea de comandos:** abajo de la vista de Diseño, un campo donde se escribe la herramienta por su nombre o un
    alias corto (`L` línea, `C` círculo, `A` arco, `REC` rectángulo, `TR` recortar, `EX` alargar, `O` desfasar, `F`
    redondear, `CHA` chaflán, `MI` simetría, `AR` matriz, `M` mover, `CO` copiar, `RO` girar, `SC` escalar, `DI`
    distancia, `AREA` área, `E` extruir, `REV` revolucionar). La herramienta pide los datos paso a paso en ese mismo
    campo, con las opciones entre corchetes; Enter o la barra espaciadora confirman, y en vacío repiten la última
    herramienta; Esc cancela. Los alias están en los dos idiomas y se pueden cambiar en Preferencias.
13. **Coordenadas tipeadas:** en cualquier pedido de punto: absolutas `x,y`; relativas al último punto `@dx,dy`; polares
    `@distancia<ángulo` (grados desde +X, antihorario). En el boceto de una cara, x e y son las del plano del boceto.
14. **Entrada junto al cursor:** al dibujar, al lado del puntero aparecen el largo y el ángulo (o el radio) del tramo;
    se tipea el número y Tab pasa al siguiente; lo tipeado queda como cota del boceto.
15. **Referencias a objetos:** el puntero se engancha en extremo, punto medio, centro, cuadrante, intersección,
    perpendicular, tangente, el más cercano sobre una curva y punto de la cuadrícula, con un marcador distinto para
    cada uno. F3 las prende y apaga; un clic derecho con Mayús elige una sola para el próximo punto. El enganche crea
    la restricción que corresponde (coincidente, punto medio, tangente…).
16. **Ortogonal y rastreo polar:** F8 limita los tramos a horizontal y vertical; F10 muestra guías cada ángulo elegido
    (15°, 30°, 45° o 90°) y desde los puntos enganchados, para alinear sin cotas. F9 engancha a la cuadrícula.
17. **Edición del boceto:** recortar y alargar hasta la curva más cercana, desfasar a una distancia, redondear y
    achaflanar esquinas, simetría respecto de una recta, matriz rectangular y polar, mover, copiar, girar y escalar
    con punto base. Selección por ventana (de izquierda a derecha, solo lo que queda adentro) y por cruce (de derecha a
    izquierda, todo lo que toca).
18. **Medir:** distancia y ángulo entre dos puntos, área y perímetro de una región, sin agregar cotas.

### Archivos y placa

19. **Abrir y traer:** STEP (AP203, AP214, AP242) e IGES como sólidos de base sobre los que siguen las operaciones; 3MF,
    STL, OBJ y glTF como cuerpos de malla (se ven, se miden, se mueven y se exportan, pero las operaciones de sólido
    necesitan un cuerpo B-rep); «Convertir malla» rehace como sólido paramétrico las piezas cerradas de sección
    constante con caras planas y cilíndricas.
20. **Exportar:** STEP AP242 (con nombres, colores y ensambles), 3MF, STL y OBJ.
21. **Mandar a la placa** (Ctrl+Mayús+P):
    - la pieza pasa a Preparar como un objeto, hecha malla con `cad.chord_tolerance` y `cad.angle_tolerance`, apoyada
      en la cama y centrada si es nueva;
    - el objeto recuerda su diseño: en la lista de objetos, «Editar en Diseño» lo vuelve a abrir;
    - al volver a mandarlo, reemplaza la malla y conserva la posición, el giro, los ajustes propios del objeto, sus
      modificadores y lo pintado, que se reproyecta sobre las caras como en `step_faces.md`;
    - con Boceto o Primitiva desde Preparar, la pieza va directo a la placa sin abrir Diseño.
22. **Proyecto:** el historial completo (bocetos, parámetros y operaciones) se guarda dentro del proyecto, así que se
    puede reabrir y cambiar una medida. Un programa que no conoce Diseño abre la malla igual.
23. **Medidas:** volumen, área, centro de masa y caja de cada cuerpo, con el material del filamento elegido para el
    peso (`stability.md`).

## Pruebas

| # | Caso | Resultado |
|---|---|---|
| CD1 | `ui.cad_design` apagado | No hay pestaña Diseño ni herramientas Boceto y Primitiva |
| CD2 | Boceto de un rectángulo de 40 × 30 mm en XY, extruir 20 mm | Un cuerpo de 24 000 mm³ y caja 40 × 30 × 20 |
| CD3 | CD2 con una cota del ancho, sin restricciones de posición | La barra dice 2 grados de libertad; con «fijo» en una esquina, 0, y el boceto en negro |
| CD4 | CD2, cambiar el parámetro del ancho a 50 | El cuerpo pasa a 30 000 mm³ sin rehacer nada a mano |
| CD5 | Cilindro de 20 mm de diámetro y 10 de alto, agujero pasante M5 de la tabla ISO | Agujero de 5,5 mm (holgura media de ISO 273 para M5) que atraviesa la pieza |
| CD6 | CD2 con un redondeo de 3 mm en una arista y después borrar el redondeo | Volumen menor con el redondeo; al borrarlo, otra vez 24 000 mm³ y las operaciones de después se rehacen |
| CD7 | Mandar CD2 a la placa, moverlo, pintar soporte en una cara, editar en Diseño y subir el alto a 25, volver a mandar | El objeto sigue en el mismo lugar, con sus ajustes y la pintura sobre la misma cara |
| CD8 | Guardar el proyecto con CD2, cerrarlo, abrirlo y cambiar el ancho | El historial vuelve entero y la pieza se rehace |
| CD9 | Abrir un STEP de una caja y agregar un vaciado de 2 mm | El STEP es el sólido de base y el vaciado se rehace al cambiar el espesor |
| CD10 | Esc dentro de una herramienta del boceto, y otra vez Esc | Primero sale de la herramienta, después del boceto; no se borra nada |
| CD11 | Texto «TISMA» de 10 mm, en una computadora sin esa fuente | La pieza se abre igual, con el contorno guardado |
| CD12 | Créditos | Dice «Basado en SolidCraft» con su licencia |
| CD13 | En la línea de comandos: `L`, `0,0`, `@40,0`, `@0,30`, `@-40,0`, `C` (cerrar) | Un rectángulo cerrado de 40 × 30 mm con sus cotas; Enter en vacío vuelve a empezar `L` |
| CD14 | `L`, `0,0`, `@50<30` | Un tramo de 50 mm a 30° desde +X: el extremo en (43,301; 25) |
| CD15 | Línea desde el punto medio de otra con la referencia «punto medio» | El punto queda en el medio y el boceto tiene la restricción de punto medio |
| CD16 | F8 prendido, tramo con el puntero a 10° de la horizontal | El tramo sale horizontal |
| CD17 | Dos líneas que se cruzan, `TR` sobre un extremo | Ese extremo se corta en la intersección |
| CD18 | `AR` polar de un círculo, 6 copias en 360° | Seis círculos a 60° entre sí |
| CD19 | Pestaña Diseño abierta | La misma ventana, vista y cama que Preparar; no se abre ningún otro programa |
