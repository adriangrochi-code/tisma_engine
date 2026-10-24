# Optimización estructural

**Qué hace:** con el análisis de `structural_analysis.md`, busca la pieza más liviana que cumple los requisitos:
- factor de seguridad, límites de desplazamiento y temperatura;
- relleno mínimo, uniforme o por zonas;
- relleno alineado con las tensiones («nervios»);
- refuerzos junto a apoyos y cargas;
- estructura lattice;
- orientación recomendada.

El resultado vuelve al laminado como ajustes del objeto y zonas con su propio relleno.

**Fase:** F7. Lo diseñó y escribió el equipo del dueño en Tisma Slicer.

**Fuentes:**
- Tam y Mueller, «Additive Manufacturing Along Principal Stress Lines», *3D Printing and Additive Manufacturing*,
  2017: imprimir siguiendo las tensiones principales.
- Gibson y Ashby, *Cellular Solids*, 2.ª ed., 1997, cap. 5: propiedades de sólidos celulares.

## Reglas

1. **«Cumple»** quiere decir que el veredicto es «Aguanta». Más relleno nunca empeora los requisitos, así que se puede
   buscar por bisección.
2. **Relleno mínimo uniforme:**
   - bisección de la densidad, redondeada a un porcentaje entero;
   - si ni el 100 % alcanza, se informa: hacen falta más paredes, otro material u otra forma.
3. **Relleno por zonas:**
   - con las tensiones del relleno uniforme, cada punto del interior recibe la densidad que lleva su tensión a la
     admisible: `ρ · utilización^(1/nS)`, con `nS` el exponente de resistencia del patrón;
   - el interior se divide en celdas (3 vóxeles, mínimo 3 mm) y se agrupa en 3 niveles;
   - los tres niveles se escalan juntos, por bisección, hasta cumplir;
   - las zonas son las mallas de las celdas de cada nivel, tal como se van a imprimir, y el último análisis es el de
     esas mallas;
   - solo se propone si queda más liviano que el uniforme.
4. **Nervios (relleno a lo largo de las tensiones):**
   - en cada vóxel del relleno, la tensión en el plano de la capa (σxx, σyy, τxy) da dos tensiones principales. La de
     mayor valor absoluto da la dirección θ, y `a = (|σ1| − |σ2|) / (|σ1| + |σ2|)` mide cuánto trabaja en una sola
     dirección;
   - cada celda promedia `a · (cos 2θ, sin 2θ)`, pesado por el índice de rotura. El ángulo doble hace que tracción y
     compresión en la misma dirección sumen (en flexión, a los dos lados de la fibra neutra);
   - se quedan las celdas con al menos el 20 % del índice de rotura máximo y una direccionalidad media de 0,5 o más;
   - se agrupan por ángulo en pasos de 15° (desvío máximo 7,5°);
   - cada grupo es una zona con relleno rectilíneo en una sola dirección, al ángulo del grupo, en todas las capas.
     La densidad no cambia;
   - el ángulo se mide sobre la cama;
   - con nervios, el relleno mínimo se busca dos veces: primero sin nervios, de ahí salen los nervios, y después con
     ellos. Se usan solo si el resultado es más liviano.
5. **Relleno en una sola dirección en el análisis:**
   - no es igual en todas las direcciones. En el sistema de sus líneas (L a lo largo, T a lo ancho, N a través de las
     capas):

     | Propiedad | Factor sobre el material macizo | Fuente |
     |---|---|---|
     | E_L, E_N, G_LN y sus resistencias | ρ | Regla de las mezclas (cota de Voigt) |
     | E_T, G_LT, G_TN | ρ² | Gibson y Ashby, cap. 5 |
     | Resistencias a lo ancho | 0,3 · ρ^1,5, nunca más que el relleno común de esa densidad | Ídem |

   - rotura por tensión máxima en ese sistema: |σL|, |σT|, σN de tracción, los cortes τLN y τTN (contra 0,6 de la
     adherencia entre capas) y τLT (con la relación de von Mises).
6. **Refuerzos:**
   - las celdas de 2 mm a menos de un radio de las caras fijas y de las cargas forman una zona. El radio por defecto es
     2 × el radio de la carga, mínimo 5 mm;
   - esa zona lleva 2 perímetros más (el análisis la ve como una pared más gruesa) y relleno del 40 %, nunca menos que
     el de la pieza;
   - se busca el relleno uniforme más liviano del resto con la zona puesta, y se compara con el mejor sin ella.
7. **Lattice:**
   - **geometría imprimible sin soporte:** celdas cúbicas con barras verticales y cruces a 45° en los planos XZ e YZ,
     sin barras horizontales;
   - **barras:** prismas de 6 lados que se solapan en los nodos. Se quedan las que tocan la pieza, así llegan hasta
     las paredes;
   - **cómo se imprime:** como una zona maciza dentro de la pieza, con el relleno de la pieza al 0 %;
   - **análisis:** cada celda es un material uniforme con la densidad de sus barras, `ρ = π(1 + 4√2)(r/a)²` (r el
     radio de las barras, a el lado de la celda), y exponente 1,5 para rigidez y resistencia. Es aproximado: se calibra
     con probetas;
   - **búsqueda:** primero el diámetro uniforme mínimo, por bisección entre 0,9 mm (el mínimo que se imprime) y 3 mm;
     después un diámetro por celda, con la sección proporcional a su utilización, escalado en conjunto hasta cumplir.
8. **Orientación recomendada:**
   - se analiza la pieza con las capas apiladas en Z (como está), en X y en Y, con las mismas cargas;
   - se recomienda la de mayor factor de seguridad si mejora más de un 5 %;
   - al girar la pieza, las cargas y las caras giran con ella;
   - no se revisan los voladizos de la nueva orientación.
9. **Capas curvas:** con capas no planares, en cada vóxel la dirección débil es la normal de la capa curva que pasa
   por él. Las normales se agrupan cada unos 2°, cada grupo con su propia matriz. No se modela el encastre entre capas
   onduladas: la mejora real puede ser mayor que la calculada.

## Pruebas

Medidas con el análisis de Tisma Slicer. PLA de la tabla salvo que se diga otra cosa.

| # | Caso | Resultado |
|---|---|---|
| O1 | Límite de desplazamiento fijado igual al desplazamiento que da el 40 % de relleno | El relleno mínimo encuentra el 40 % en 10 análisis; el 38 % no cumple |
| O2 | Voladizo cargado hasta su resistencia | Uniforme: 17 % (4,33 g). Por zonas: base del 7 % y 2 zonas junto al empotramiento, 3,96 g (−8,5 %) |
| O3 | Pieza con el perfil (2 perímetros, giroide), 55 N, factor de seguridad 1,5 | Uniforme: 48 % (6,51 g). Por zonas: 5,51 g (−15 %). Aplicado al objeto y analizado de nuevo: misma masa (±2 %) y cumple |
| O4 | Probeta de tracción con mordazas macizas, nervios activados | El relleno mínimo baja del 31 % al 27 %. Con las líneas a lo largo, la barra es un 10 % más rígida y el relleno trabaja un 10 % menos cargado; con las líneas a lo ancho, 1,96 veces más cargado |
| O5 | Barra con carga puntual, refuerzo de 6 mm (+2 perímetros, 40 %) | Factor de seguridad de 2,63 a 2,90 con el mismo relleno. La zona es una malla cerrada alrededor del empotramiento y de la carga |
| O6 | Una celda de lattice | 12 barras cerradas, ninguna con menos de 45° de inclinación |
| O7 | Cubo de 30 mm a compresión, 6000 N, factor de seguridad 2 | Barras de 2,08 mm: 16,8 g frente a 33,5 g macizo. El diámetro variable no mejora: la tensión es uniforme |
| O8 | Voladizo de 90 × 20 × 20 mm, 200 N, factor de seguridad 2 | Lattice uniforme: barras de 2,25 mm (25,4 g). Variable: de 1,0 a 2,5 mm, unos 17 g (−30 %), más gruesas junto al empotramiento |
| O9 | Barra tirada hacia arriba con capas inclinadas ±25° | Factor de seguridad de 4,7 (capas planas) a 5,2 |
