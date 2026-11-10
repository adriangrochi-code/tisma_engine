# Ingeniería: los paneles de análisis estructural (FEA) y aerodinámico (CFD)

**Qué hace:** pone los dos análisis en la sección Ingeniería de la barra de arriba, cada uno en su pestaña, con todo lo
que hace falta para usarlos con el mouse, sin línea de comandos:
- **Estructural (FEA):** marcar en la pieza dónde está fija y dónde la empujan, elegir el material y analizar
  (`structural_analysis.md`, `structural_optimization.md`);
- **Aerodinámica (CFD):** elegir de dónde viene el aire y a qué velocidad, y analizar (`aerodynamics.md`).

**Pedido del dueño (02-11-2026):** «Análisis debe quedar en Ingeniería separado por FEA y CFD, junto con las formas de
usarlas: punto fijo, zona fija, zona de fuerza, punto de fuerza, material, temperatura y temperatura de extrusión; que
las temperaturas se saquen automático de la configuración, junto a la altura de capa y el ancho».

**Fuente:** el panel de Ingeniería de Tisma Slicer (`interface.md`, «Panel de Ingeniería», captura
`50_engineering.png`), observado como caja negra, con los cambios que pide el dueño.

**Fase:** en la 1.0. Reemplaza la pestaña «Análisis» de Impresión 3D (`cad_design.md`, regla 3).

## Reglas

### Dónde está

1. **Pestañas:** la sección Ingeniería tiene, en este orden, **Estructural**, **Aerodinámica** y **Diseño**. Impresión
   3D ya no tiene «Análisis». Cada pestaña de análisis usa la vista 3D de la placa con la pieza elegida, y su panel va
   en la columna izquierda, como el de Diseño.
2. **Qué pieza:** arriba del panel, una lista con las piezas de la placa; con una sola, ya está elegida. Al elegir
   otra, el panel muestra sus condiciones (cada pieza guarda las suyas). Las demás piezas se ven en gris.

### Valores que salen solos de la configuración

3. **Se toman del perfil elegido, sin escribirlos:**

   | Valor | De dónde sale |
   |---|---|
   | Material | El tipo de filamento del perfil de filamento (PLA, PETG, ABS…), buscado en la tabla de materiales de `structural_analysis.md`. Si el tipo no está en la tabla, queda «Elegí un material» y Analizar se apaga |
   | Temperatura de extrusión | La temperatura de la boquilla del perfil de filamento (la de las capas después de la primera) |
   | Temperatura de la cama | La del perfil de filamento; solo se muestra (para la adherencia de la primera capa) |
   | Altura de capa | La del perfil de impresión, y la de la primera capa |
   | Ancho de línea | El del perfil de impresión (el de las paredes y el del relleno, si son distintos) |
   | Paredes y relleno | Perímetros, capas sólidas, densidad y patrón del relleno, del perfil de impresión |

   - cada valor muestra de dónde viene («del perfil PLA Genérico», «del perfil 0,20 mm»);
   - si se cambia el perfil en Impresión 3D, los valores del panel cambian enseguida;
   - el material, la temperatura de extrusión, la altura de capa y el ancho se pueden cambiar a mano para probar sin
     tocar el perfil. Un valor cambiado a mano se marca con un punto violeta y tiene «Volver al del perfil». Un valor
     cambiado a mano no cambia el perfil de impresión ni el G-code.
4. **La temperatura de trabajo** (a la que va a estar la pieza en uso) no está en ningún perfil: empieza en 23 °C y se
   escribe. Si supera la máxima de servicio del material, el veredicto es «Fuera de temperatura»
   (`structural_analysis.md`).

### Estructural: cómo se marca la pieza

5. **Herramientas de marcado**, en botones, la elegida en violeta. Esc vuelve a «Rotar»:

   | Herramienta | Qué se hace con el mouse | Qué queda |
   |---|---|---|
   | Rotar | Nada se marca; se gira la vista | — |
   | **Punto fijo** | Clic en la superficie | Un disco fijo (no se mueve) centrado en el clic, de radio editable (5 mm al empezar) |
   | **Zona fija** | Clic en una cara: se elige la cara entera (la parte plana o lisa hasta los bordes marcados, como en `step_faces.md`); con Mayús se suman caras; con Ctrl se arrastra un pincel para elegir triángulos sueltos | Esa zona fija |
   | **Punto de fuerza** | Clic en la superficie | Una fuerza en un disco de radio editable (5 mm al empezar) |
   | **Zona de fuerza** | Como Zona fija | Una fuerza repartida en toda la zona |
   | Límite de desplazamiento | Como Zona fija | La zona no puede moverse más que el límite (mm o % de la mayor medida) |

6. **Cada fuerza** pide:
   - el valor en N (o en kg, que se pasa a N con g = 9,81 m/s², escrito al lado);
   - la dirección: perpendicular a la superficie, hacia adentro (al empezar), o en X, Y o Z (con signo), o tres
     números;
   - se dibuja como una flecha roja proporcional al valor, y se puede arrastrar la punta para cambiar la dirección.
7. **Lo fijo** se dibuja en azul con un rayado; las fuerzas en rojo; los límites en amarillo.
8. **Lista de condiciones:** debajo de las herramientas, un renglón por condición («Zona fija 1 · 3 caras», «Punto de
   fuerza 1 · 50 N hacia −Z»), con un ojo para ocultarla sin borrarla, un lápiz para editarla y una cruz para
   borrarla. Clic en el renglón la resalta en la vista. Deshacer y rehacer funcionan con cada cambio.
9. **Analizar** (botón violeta a todo el ancho) se apaga hasta que haya al menos algo fijo (punto o zona) y una fuerza
   (punto o zona). Debajo dice qué falta: «Agregá un punto o una zona fija.», «Agregá un punto o una zona de fuerza.».
10. **Más opciones:** factor de seguridad pedido (2 al empezar), resolución con el tamaño que resulta («Unos 60 000
    vóxeles de 1,12 mm, 284 MB, unos 14 s»), dirección de impresión (Z al empezar, o la de la impresora de cinta), y
    «Calcular en la nube» (`cloud.md`, regla 10b).

### Estructural: resultados

11. **Mientras calcula:** barra de avance con «Cancelar»; la vista se puede seguir girando.
12. **Al terminar:**
    - la pieza pintada con un mapa de colores: factor de seguridad (al empezar), tensión de von Mises o
      desplazamiento, con su escala;
    - el veredicto grande arriba del panel (Aguanta, Margen bajo, Fuera de carga, Demasiado flexible, Fuera de
      temperatura, No aplicable), con el punto crítico marcado en la vista y su valor;
    - masa de la pieza con su material;
    - si no aguanta, la sugerencia de materiales de `structural_analysis.md`, con un botón para probar cada uno;
    - la deformación exagerada, con un control de cuánto se exagera.
13. **Optimizar**, debajo de los resultados: Piel + núcleo, Relleno mínimo, Refuerzo local, Retícula 3D y Orientación
    de impresión (`structural_optimization.md`), plegados.
14. **Cambiar algo** (una condición, el material, el perfil) deja el resultado anterior en gris con «Desactualizado:
    analizá de nuevo».

### Aerodinámica

15. **Entradas:** dirección del aire (X, Y o Z con signo, o «la de la vista»), velocidad en m/s, calidad (solo
    rozamiento, rápida, normal, alta) y la altura de capa del perfil, que da la rugosidad (regla 3: sale sola y se
    puede cambiar a mano). Una flecha grande azul en la vista muestra de dónde viene el aire.
16. **Analizar** y «Calcular en la nube», como en Estructural. Resultados de `aerodynamics.md`: arrastre en N, Cd
    separado en presión y rozamiento, fuerza transversal, área frontal, Reynolds real y simulado, rugosidad media, y la
    pieza pintada con el coeficiente de presión (de −1,5 a 1) o con la rugosidad.

### Proyecto

17. Las condiciones de cada pieza (fijos, fuerzas, límites, material y temperaturas cambiados a mano, opciones) y el
    último resultado se guardan en el proyecto (`formats.md`), así se abren igual y viajan a la nube.

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| EP1 | Ingeniería con un cubo y el perfil «PLA» a 215 °C, capa 0,2 y ancho 0,45 | Estructural muestra material PLA, 215 °C «del perfil», 0,20 mm y 0,45 mm; Impresión 3D no tiene «Análisis» |
| EP2 | Cambiar el filamento a PETG a 240 °C en Impresión 3D y volver | Material PETG y 240 °C, sin tocar nada en el panel |
| EP3 | Escribir 230 °C a mano y cambiar de perfil | El valor sigue en 230 °C con el punto violeta; «Volver al del perfil» lo devuelve al del perfil nuevo; el G-code no cambia |
| EP4 | Barra de 100 × 10 × 10 mm: Zona fija en una punta, Punto de fuerza de 50 N hacia −Z en la otra, Analizar | Mapa, veredicto y desplazamiento máximo dentro de lo que dice `structural_analysis.md` para el voladizo |
| EP5 | Solo un Punto fijo, sin fuerza | Analizar apagado con «Agregá un punto o una zona de fuerza.» |
| EP6 | La misma barra con Zona de fuerza de 50 N en la cara de arriba en lugar del punto | Resultado coherente con la carga repartida (menor desplazamiento máximo que con el punto en la punta) |
| EP7 | Borrar una condición y Ctrl+Z | La condición vuelve; el resultado queda «Desactualizado» |
| EP8 | Guardar el proyecto con condiciones y abrirlo | Las mismas condiciones, valores a mano y el último resultado |
| EP9 | Aerodinámica de un cubo de 20 mm, aire en +X a 10 m/s, rápida | Cd entre 0,8 y 1,3 y el mapa de presión con la cara de adelante en rojo |
| EP10 | Filamento de un tipo que no está en la tabla de materiales | «Elegí un material» y Analizar apagado hasta elegir uno |
