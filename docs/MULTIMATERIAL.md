# Multimaterial con un cabezal: mezcla de colores y purga en el relleno

Pedido del dueño (2026-10-24): «laminar y generar el efecto de color de Full Spectrum, pero en el nuestro, con
impresoras de un solo cabezal, junto con purgas internas en el infill».

**Estado:** diseño. Todavía no hay código. Es diseño propio, desde conceptos públicos de la impresión 3D: cambio de
filamento, volumen de purga, torre de purga, purga en el relleno y mezcla de colores por capas. Full Spectrum es un
derivado de un laminador AGPL: no se lee su código ni su repositorio. Si hace falta igualar su comportamiento, el
equipo de fichas lo describe como caja negra (pedido el 2026-10-24).

## Qué hace falta

### 1. Varios filamentos por una boquilla

- **Ranuras:** la impresora tiene N ranuras (el AMS o AMS lite de Bambu Lab, el MMU de Prusa, el CFS de Creality, el ACE
  de Anycubic, un MMU con Klipper, o cambio a mano). Cada ranura tiene su perfil de filamento y su color.
- **Asignación:** cada pieza (después, cada cara pintada) tiene una ranura o un filamento mezclado (punto 4).
  - Hace falta que el motor lamine varias piezas a la vez, cada una con su herramienta por región.
  - Hoy lamina una sola malla: es el primer trabajo.
- **Cambio de herramienta:** un G-code por impresora (`machine.tool_change_gcode`), con variables: la ranura anterior y
  la siguiente, las temperaturas y el volumen de purga. Por defecto:
  - `T[next]` en Marlin con MMU y en Klipper (las macros del MMU);
  - `M600` para el cambio a mano.
- **Sistemas cerrados:** el AMS de Bambu Lab usa una secuencia propia, sin documentación oficial pública.
  - El usuario puede cargar su plantilla.
  - Que el motor traiga una lo decide el dueño (ver «Preguntas»).

### 2. La purga

Después de un cambio, la boquilla tiene filamento viejo. Hay que extruir un volumen V(de, a) antes de que el color salga
limpio.
- **Volumen por par de filamentos:** una matriz en mm³. Por defecto la estima el motor desde la diferencia de color: de
  oscuro a claro hace falta más. Se puede ajustar a mano.
- **Lo que hace el G-code de cambio:** en las impresoras con descarte, como el conducto de las Bambu Lab, purgan
  ahí. La purga en el relleno reduce cuánto tienen que tirar.

### 3. Purga en el relleno (pedido del dueño)

La primera parte del filamento nuevo, la de color mezclado, se extruye **adentro de la pieza**, donde no se ve, en lugar
de tirarse:
1. **Orden de la capa:** después de un cambio, el filamento nuevo empieza por el relleno ralo de esa capa, y deja las
   paredes y las superficies visibles para después.
2. **Cuenta:** se extruye relleno hasta sumar V(de, a). Si el relleno de esa capa alcanza, no se tira nada.
3. **Si no alcanza:**
   - se sigue en el relleno de otra pieza de la placa;
   - después, en una torre de purga chica o en el descarte de la impresora, y solo lo que falta.
4. **Opción por pieza:** se puede desactivar en piezas donde el relleno se ve (transparentes, por ejemplo).
5. **El verificador** sabe que esas extrusiones son de purga, y la vista previa las muestra con su propio tipo de
   línea.
6. **Orden de las herramientas en cada capa:** se elige para minimizar los cambios. La capa empieza con la última
   herramienta de la anterior.

### 4. Filamentos mezclados: el efecto de color

Un **filamento mezclado** es una receta de los físicos con pesos, por ejemplo 2 partes de amarillo y 1 de rojo, que da
naranja. Con una sola boquilla, la mezcla se hace **por capas**:
- **Una capa, un color:** cada capa de la región se imprime con uno de los filamentos de la receta.
- **La cuenta de capas sigue los pesos,** repartidas lo más parejo posible, por difusión del error: con 2 : 1 sale
  A A B A A B, nunca A A A A B B.
- **El ojo las mezcla** con capas finas (0,08 a 0,12 mm): la pared se ve del color promedio.
- **Degradado:** los pesos cambian con la altura. Se puede pasar de un filamento a otro a lo largo de Z, o seguir una
  curva de color.
- **Color buscado:** se da un color y el motor busca los pesos con las ranuras cargadas, por mínimos cuadrados no
  negativos en RGB lineal. Si el color no se alcanza, el motor dice cuál es el más cercano.
- **Vista previa:** cada capa con su color real, y de lejos el promedio.
- **El costo:** un cambio en muchas capas, y por eso la purga en el relleno (punto 3) es la que lo hace viable. El motor
  estima el tiempo y el filamento de purga antes de laminar.

Más adelante:
- mezcla dentro de la capa, alternando por paredes o por tramos de la pared exterior;
- un modelo de color que tenga en cuenta la translucidez de cada filamento.

## Fases

| Fase | Contenido | Terminada cuando |
|---|---|---|
| MM1 | Varias piezas por placa, cada una con su ranura; regiones por herramienta; cambio de herramienta con plantilla por impresora; matriz de purga; torre de purga; colores en la vista previa; el verificador sigue los cambios | Una pieza de dos colores se imprime en una impresora con MMU o con `M600` |
| MM2 | Purga en el relleno (el orden de la capa, la cuenta, la vuelta a la torre o al descarte), orden de herramientas por capa | La misma pieza tira menos de la mitad del filamento que con solo torre; medido |
| MM3 | Filamentos mezclados por capas, degradados, color buscado, vista previa del color | Una pieza con 2 filamentos muestra 3 tonos o más distinguibles; medido con fotos |
| MM4 | Pintar caras con filamentos; mezcla dentro de la capa | Las pruebas de cada parte pasan |

Necesitan que el motor lamine varias piezas a la vez (MM1), que también pide P4 (proyectos 3MF con varias piezas).

## Preguntas para el dueño

1. **¿Con qué impresora y sistema empezamos?** Por ejemplo, Bambu Lab A1 con AMS lite, Prusa MK4S con MMU3, Creality
   K2 con CFS, o cambio a mano con `M600`. Lo que se elige define la primera plantilla de cambio y la máquina de
   prueba.
2. **Bambu Lab:** su AMS no tiene documentación oficial pública del G-code de cambio. ¿El usuario carga su propia
   plantilla, o se busca otra vía con el dueño?
