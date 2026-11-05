# Biblioteca de piezas de Diseño (tornillería, engranajes y lo que sigue)

**Qué hace:** dentro del espacio Diseño (`cad_design.md`), páginas que arman piezas mecánicas normalizadas listas para
imprimir con unas pocas medidas: tornillos, tuercas, arandelas, varillas, separadores, engranajes y, en las etapas que
siguen, cremalleras, poleas de correa, agujeros para insertos, bisagras impresas en su lugar y agujeros normalizados
sobre cualquier cuerpo del diseño. Cada pieza se puede volver a editar después de mandarla a la placa.

**Pedidos del dueño** (02-11-2026, al motor): la tornillería va «en la parte de diseño», no en otra pestaña; y «una
solución de AutoCAD bastante completa» para piezas impresas: tornillos, varillas, tornillos para ajustar con el dedo,
separadores y engranajes.

**Fase:** con Diseño; P1 (tornillería y engranajes) ya hecha por el motor el 02-11-2026; P2 a P6, en el orden de la
regla 9.

**Fuentes** (normas por su número y título; los valores de cada medida salen de la norma, no de esta ficha):
- rosca métrica: ISO 68-1 (perfil básico), ISO 261 (plan general de diámetros y pasos), ISO 965-1 (tolerancias);
- tornillos y tuercas: ISO 4017 (cabeza hexagonal, rosca completa), ISO 4762 (cabeza cilíndrica con hexágono
  interior), ISO 4032 (tuerca hexagonal, estilo 1), ISO 7089 (arandela plana, serie normal), ISO 273 (agujeros de
  paso, finos, medios y gruesos), ISO 10642 (cabeza avellanada con hexágono interior, para el avellanado de P2);
- engranajes: ISO 53 (perfil de la cremallera de referencia), ISO 54 (módulos), ISO 21771 (geometría de los
  engranajes cilíndricos de evolvente);
- correas: la hoja de datos de Gates para PowerGrip GT2 (paso de 2 mm y sus perfiles); se cita la hoja en el
  programa;
- insertos roscados que se colocan con calor: la hoja de cada fabricante (diámetro y largo del agujero por medida);
  la tabla del programa nombra fabricante y hoja de cada fila, y no inventa medidas.

## Ajustes

| Clave | Tipo | Por defecto | Qué hace |
|---|---|---|---|
| `cad.thread_play` | mm | 0,2 | Juego radial para imprimir: la rosca del tornillo se adelgaza y la de la tuerca se agranda en esto |
| `cad.gear_backlash` | mm | 0,15 | Juego entre dientes de un engranaje |
| `cad.part_play_by_material` | sí/no | sí | Tomar los dos juegos de la calibración de ajuste del material elegido (regla 8), si existe |

## Reglas

1. **Dónde:** en el panel de Diseño, «Nuevo» tiene un botón por familia (Tornillería, Engranajes y, al llegar, las de
   la regla 9). Cada uno abre su página en la misma columna izquierda, con «< Diseño» arriba para volver. La línea de
   comandos abre cada pieza por su nombre en español o en inglés (tornillo/bolt, allen, tuerca/nut, arandela/washer,
   varilla/rod, mariposa/moleteado, separador/standoff, espaciador/spacer, engranaje/gear, y los que se sumen).
2. **Cada página**, de arriba abajo: el tipo de pieza (con un dibujo chico de cada uno), las medidas (la medida
   normalizada en una lista o «A medida»), el juego para imprimir, los datos que resultan (por ejemplo, agujero de
   paso, broca para roscar, llave, volumen y caja; en engranajes, diámetros primitivo, exterior y de pie, paso
   circular, distancia entre centros y relación con su pareja) y los botones **Mandar a la placa** y **Mandar con su
   pareja** (tuerca, o el otro engranaje). La vista muestra la pieza mientras se eligen las medidas.
3. **Medidas normalizadas:** las de la lista salen de las normas de las fuentes; el programa muestra al lado la norma
   («ISO 4762, M8 × 30»). «A medida» deja escribir diámetro, paso y largo, y la pieza deja de decir la norma.
4. **Orientación en la placa:** cada pieza llega en la posición que mejor se imprime: tornillos parados sobre la
   cabeza, tuercas, arandelas y engranajes acostados, varillas paradas o acostadas según su largo y la altura de la
   máquina (la que entre). La pieza llega apoyada y centrada como cualquier objeto (`placement.md`).
5. **Editar después:** «Editar en Diseño» en el menú del objeto de la placa vuelve a abrir la página con las medidas
   que tenía y reemplaza la pieza en el mismo lugar, con sus ajustes, modificadores y pintura (como `cad_design.md`,
   regla 21). El proyecto guarda la pieza como sus medidas, no solo como malla.
6. **Rosca impresa:** el perfil sigue ISO 68-1; con `cad.thread_play` el tornillo se achica y la tuerca se agranda en
   ese juego, medido en el radio. Los pasos de menos de 0,7 mm avisan «Este paso puede no imprimirse bien con
   boquilla de X mm», según la boquilla elegida.
7. **Engranajes:** evolvente con el perfil de ISO 53 (ángulo de presión 20° por defecto), módulos de ISO 54;
   rectos, helicoidales y en espina de pescado; la pareja de uno helicoidal sale con la mano contraria. El juego
   `cad.gear_backlash` se reparte entre los dos engranajes de un par.
8. **Juego según el material** (`cad.part_play_by_material`): una calibración «Ajuste de piezas» imprime un tornillo
   M8 con tres tuercas de juego distinto y dos engranajes con tres juegos; la persona marca cuál enrosca a mano y cuál
   gira libre, y el programa guarda esos dos valores para ese filamento (`calibration_models.md`). Sin calibración,
   rigen los de la tabla de ajustes.
9. **Orden de las etapas siguientes**, de lo que más se usa al imprimir piezas mecánicas a lo menos usado:
   - **P2, agujeros normalizados sobre cualquier cuerpo** (la etapa D4 de `cad_design.md`): agujero de paso (ISO 273,
     fino, medio o grueso), agujero roscado con su rosca impresa, avellanado y cilíndrico para cabeza de tornillo,
     y **alojamiento de tuerca** hexagonal (cerrado, o abierto a un lado para meterla de costado), con un puente de
     una capa sobre el alojamiento para que se imprima sin soporte;
   - **P3, agujero para inserto roscado con calor**, por medida y por fabricante (la tabla de la fuente);
   - **P4, bisagra impresa en su lugar**: dos hojas y el eje en una sola pieza, con un juego que sale de la
     calibración de la regla 8;
   - **P5, polea GT2** (cantidad de dientes, ancho de correa, eje, prisionero con alojamiento de tuerca) con las
     medidas de la hoja de Gates;
   - **P6, cremallera** para un engranaje elegido (mismo módulo y ángulo de presión).

## Casos límite

- Medida que no entra en la cama: avisa con la medida máxima que entra y no la manda.
- Rosca o diente más chico que dos anchos de línea: aviso de que no se va a imprimir bien, con la boquilla que haría
  falta.
- Pareja de engranajes con menos dientes que el mínimo sin socavado del perfil elegido: aviso «Con N dientes el pie
  del diente queda socavado».
- Editar una pieza que la persona escaló o cortó en la placa: avisa que se pierden esos cambios antes de reemplazarla.

## Pruebas

| # | Entrada | Resultado esperado |
|---|---|---|
| DP1 | Tornillo hexagonal M8 × 30 con su tuerca, juego 0,2 mm, mandados a la placa | Dos objetos; el tornillo parado sobre la cabeza; Acerca de la pieza dice «ISO 4017, M8 × 30» |
| DP2 | Imprimir DP1 en PLA con la boquilla de 0,4 mm (prueba del dueño) | Enroscan a mano hasta el final, sin herramienta y sin juego que se note |
| DP3 | Dos engranajes de 20 dientes, módulo 1,5, juego 0,15 mm | Distancia entre centros 30 mm; impresos y montados en ejes a 30 mm, giran libres sin trabarse (prueba del dueño) |
| DP4 | Engranaje helicoidal de 30°, mano derecha, «Mandar con su pareja» | La pareja sale con mano izquierda y el mismo ángulo |
| DP5 | «Editar en Diseño» sobre el tornillo de DP1 movido y con un modificador | Se abre con M8 × 30; cambiar a M8 × 40 lo reemplaza en el mismo lugar con el modificador |
| DP6 | Proyecto con DP1 guardado y vuelto a abrir | «Editar en Diseño» sigue disponible con las mismas medidas |
| DP7 | Rosca M3 con boquilla de 0,8 mm | Aviso de paso chico para esa boquilla |
| DP8 | (P2) Alojamiento de tuerca M3 en una caja de 20 mm, impreso sin soportes | La tuerca ISO 4032 M3 entra a presión y el techo del alojamiento queda cerrado |
