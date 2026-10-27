# Brillo parejo: por qué una pieza sale mate en unas zonas y brillante en otras

Pedido del dueño (2026-10-25): ver qué cambia cómo refleja la luz el filamento impreso, para evitar piezas con zonas
opacas y otras brillantes. Esto resume una búsqueda de artículos, normas y documentación de fabricantes. No se
leyó código de ningún slicer ni firmware.

## Lo que se sabe

La conclusión más importante es que **no hay ningún estudio publicado que mida con un medidor de brillo**
(ISO 2813:2014 o ASTM D523, a 60°) la superficie de una pieza FFF sin pintar. Sí se mide cuán caliente sale el
plástico de la boquilla y cuán rugosa queda la superficie. El paso de eso al brillo lo dicen los fabricantes y los
usuarios.

| # | Causa | Qué tan probada | Datos |
|---|---|---|---|
| A | **La temperatura del plástico a la salida**, que baja con el flujo | La mejor | Badarinath y Prabhu 2022 (Materials 15(2):618, [doi:10.3390/ma15020618](https://doi.org/10.3390/ma15020618)) midieron la salida con un sensor infrarrojo: en promedio unos 25 °C bajo la temperatura pedida, y más baja cuanto más flujo, porque el plástico pasa menos tiempo en el hot end. Con sus ajustes, PLA a 210 °C sale a unos 186 °C con 3 mm³/s y a unos 176 °C con 13 mm³/s (cálculo nuestro sobre sus ecuaciones). Colon, Kazmer, Peterson y Seppala 2023 (Rapid Prototyping Journal 30(1):85–94, [doi:10.1108/RPJ-06-2023-0185](https://doi.org/10.1108/RPJ-06-2023-0185)), en un hot end de camino largo, midieron casi la temperatura pedida, y algo menos con flujo muy bajo: la punta pierde calor al aire |
| B | **Cambios de velocidad dentro de la pieza**: el tiempo mínimo de capa, la desaceleración en voladizos, los topes de flujo | Muchos informes de usuarios | Actúan por A. Las capas cortas que se frenan salen en bandas brillantes; tiras de prueba a unos 30 mm/s brillantes y a 120 mm/s mate. La wiki de Bambu Lab («Different gloss») y la de Polymaker («Printing Temperature») lo describen, y Polymaker aconseja 5 a 10 °C más para PLA rápido |
| C | **Rugosidad fina** (mucho menor que la longitud de onda de la luz) | Física probada; en FFF sin medir | La reflexión especular cae con la rugosidad fina (Bennett y Porteus 1961, JOSA 51:123). En FFF la producen el «sharkskin» del extrudido (desde unos 0,1 MPa de tensión de corte, Mackay 2018, J. Rheol. 62:1549), las cargas minerales de los filamentos mate y el plástico mal fundido |
| D | **El enfriamiento y el tiempo de capa** | Solo consejos de fabricantes | El plástico que queda blando más tiempo se alisa por tensión superficial (Orchard 1963, Appl. Sci. Res. 11:451). Formfutura aconseja el ventilador al 50 % o menos para los filamentos brillantes |
| E | **La altura de capa y la pendiente** | Medido para la rugosidad | Martín Fernández y Martín Sánchez 2023 (Polymers 15:585, [doi:10.3390/polym15030585](https://doi.org/10.3390/polym15030585)): Ra de 8,6 µm a 85° de pendiente y 19,4 µm a 20°; 8,0 µm con capas de 0,06 mm y 14,3 µm con 0,15 mm. Por eso, en una pieza curva, las zonas empinadas y las tendidas pueden brillar distinto |
| F | **La cristalinidad** | Débil | El PLA impreso es casi amorfo; más temperatura sube la cristalinidad, pero nadie lo midió contra el brillo |

## Qué puede hacer el motor

| # | Medida | Apoyo | Estado |
|---|---|---|---|
| G1 | **La pared externa a una sola velocidad, ancho y alto en toda la pieza**, así su flujo, y con él la temperatura de salida, no cambia | A, B | Hecho (2026-10-26) para el tiempo mínimo de capa, con G2: la pared externa ya no se frena mientras alcance con frenar lo demás. Los voladizos, G3 |
| G2 | **Para que una capa dure más, frenar primero las paredes internas, el relleno y los viajes**, o esperar con la boquilla lejos de la pieza, y dejar la pared externa a su velocidad. Sin frenarla a valores extremos (la punta se enfría con flujo muy bajo, A) | Práctica | Hecho (2026-10-26): `cooling.keep_outer_speed` (`docs/CONFIG.md`), encendido por defecto. Si no alcanza, se frena todo como antes. Esperar lejos de la pieza, pendiente |
| G3 | **Los voladizos frenan la pared externa de toda la capa**, o se limita cuánto frena | Práctica | Cuando haya velocidad de voladizo |
| G4 | **Subir la temperatura con el flujo planeado**, con anticipación por la inercia del hot end: con el ajuste de PLA de A, mantener lo que sale a 3 mm³/s cuando se imprime a 13 mm³/s pide unos 224 °C en lugar de 210 °C | Dirección probada (A); que empareje el brillo, no | Experimental: va con Q3 de `docs/RESEARCH_MAP.md` |
| G5 | **El ventilador parejo en las paredes externas**, sin rampas atadas al tiempo de capa | Práctica | A revisar con G2 |
| G6 | **Altura de capa variable en las pendientes y planchado arriba**: el planchado bajó la rugosidad del PLA de 5,17 a 1,09 µm (Matras 2025, Materials 18:5248, [doi:10.3390/ma18225248](https://doi.org/10.3390/ma18225248)) | Probado para la rugosidad | La altura variable ya está (`docs/VARIABLE_LAYERS.md`); el planchado, pendiente |
| G7 | **Una corrección por hot end** (un usuario necesitó 15 °C más con una boquilla de acero que con una de latón para el mismo aspecto) | Anécdota | Con los perfiles de impresora |

## Lo que hay que medir nosotros

Ningún dato público responde esto; hace falta una impresión de prueba y un medidor de brillo:
- el brillo a 60° (a 85° para los mate) contra el flujo, la temperatura, el ventilador y el tiempo de capa, en PLA,
  PETG, PLA seda y PLA mate, a lo largo y a lo ancho de las líneas;
- la temperatura de salida contra el flujo en los hot ends que usemos (solo hay uno medido);
- desde qué diferencia de brillo (o de temperatura de salida) el ojo la nota;
- si subir la temperatura con el flujo (G4) empareja de verdad el brillo;
- la inercia térmica del hot end y cuánto enfría la punta el ventilador.

**Pieza de prueba propuesta** (una calibración nueva, `docs/CALIBRATION.md`): una torre de bandas de pared plana, cada
una a flujo constante, de 2 a 20 mm³/s, a tres temperaturas, con y sin ventilador; y otra torre que cambia solo el
tiempo de capa, con pausas. Se mide cada banda con el medidor y se fotografía con luz rasante.
