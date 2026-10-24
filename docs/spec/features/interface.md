# Interfaz de escritorio

**Qué hace:** la ventana del programa. Desde ahí se cargan las piezas en la placa, se eligen la impresora, el material
y los ajustes, se lamina, se ve la vista previa y se exporta o se manda a la impresora. Tiene además las herramientas
propias: Ingeniería, calibraciones, dispositivos y cuenta en la nube.

**Fase:** P1 (después del motor).
- El diseño es del dueño: la misma estructura, los mismos nombres y el mismo aspecto que la app de teléfono de Tisma,
  en tema claro y oscuro.
- Esta ficha describe lo que se ve y cómo se comporta en Tisma Slicer, observado el 24-10-2026 en una pantalla virtual.
- No trae capturas, íconos ni textos de ayuda copiados: el motor dibuja los suyos.
- Lo que no se pudo ver sin impresoras conectadas ni cuenta (USB, red, cámaras, informar un problema), y los puntos
  marcados «(diseño)», salen de la documentación del dueño.

## Principios

1. **Ninguna función se pierde:** todo lo que hace el programa tiene un lugar en la ventana nueva.
2. **Textos:** en inglés, con traducción al español.
3. **Teclado:**
   - todo valor numérico se puede escribir con el teclado;
   - los atajos de siempre funcionan (abrir, guardar, deshacer, exportar, laminar, vistas 0 a 6, etc.).
4. **Rueda del mouse:** sobre un campo del panel, desplaza el panel; no cambia el valor.
5. **Modos:**
   - **Simple:** solo los ajustes más comunes;
   - **Normal** y **Experto:** el resto, como velocidades y capas no planares;
   - el modo se elige en la barra superior;
   - en modo Simple, cada página de ajustes dice qué queda oculto y ofrece «Mostrar todos los ajustes».
6. **Ayuda de cada ajuste:** al pasar el mouse se ve qué hace, el valor por defecto y el nombre del ajuste en el
   archivo de configuración (en el motor, su nombre de `docs/CONFIG.md`).

## Ventana

### Barra superior (de izquierda a derecha)

- Logo y nombre.
- **Secciones:** Placa, Impresión, Impresora y Más. La sección activa queda subrayada. El filamento o la resina no
  tienen sección: se eligen y se editan desde el panel de la placa.
- **Grupos de herramientas de la placa:** Archivo, Vista, Organizar, Pintar, Ingeniería, Calibrar y Laminar.
  - Cada uno es un ícono; al tocarlo se abre debajo una tira con sus herramientas, cada una con ícono y nombre.
  - Con poco ancho se ven solo los íconos, con el nombre al pasar el mouse.
  - Las herramientas que no se pueden usar, por ejemplo sin piezas, se ven apagadas.
- **Búsqueda de ajustes:** lleva al ajuste en su página.
- **Impresora en uso:** su modelo, con un punto que indica si está conectada. Al tocarla se abre Dispositivos.
- **Modo:** Simple, Normal o Experto, con un punto de color.
- **Cuenta de Tisma Cloud.**

### Grupos de herramientas

| Grupo | Herramientas |
|---|---|
| Archivo | Abrir, Quitar, Quitar todo, Guardar, Deshacer, Rehacer |
| Vista | 3D, Arriba, Frente, Izquierda, Derecha, Atrás, Encuadrar |
| Organizar | Ordenar, Mover, Rotar, Escalar, Apoyar sobre una cara, Cortar, Separar en partes, Medir, Altura de capa variable |
| Pintar | Soportes, Costura, Piel difusa, Colores |
| Ingeniería | Análisis (abre el panel de Ingeniería), Cerrar |
| Calibrar | Las pruebas que la impresora en uso puede hacer (ver `calibrations.md`) y «Aplicar un resultado» |
| Laminar | Laminar ahora, Exportar, Enviar, USB |

Con una impresora FDM con firmware que acepta todo, Calibrar muestra: Temperatura, Presión (pressure advance),
Retracción, Caudal máximo, VFA, Aceleración, Esquinas, Vibraciones, Flujo, Coasting, Velocidad y largo de los arcos,
Solape de los arcos, Flujo, separación y velocidad del planchado, Resistencia, Medidas, Corrección y Aplicar un
resultado. Con una impresora que calibra sola la primera capa, no aparece esa prueba. Con una de resina, solo las de
resina (`resin.md`). Los mismos modelos de prueba se ofrecen con el clic derecho sobre la cama.

### Placa

- **Vista 3D:** ocupa todo el ancho que deja el panel. Muestra la cama con su modelo y las piezas.
  - Cada cama tiene una etiqueta con su número, «Bloquear» y «Ajustes» (una placa puede tener varias camas).
  - Abajo, la barra de estado: «Sin laminar», «Listo», cantidad de objetos y, después de laminar, el tiempo
    estimado, el peso y el costo.
- **Panel de la placa** (a la derecha), en tarjetas con títulos grandes:
  1. **Impresora:** el modelo (con un candado si es un perfil del sistema) y un engranaje que abre su página. Debajo,
     la **boquilla** y el **tipo de cama** como dos listas aparte. La boquilla elige el perfil del modelo para ese
     diámetro.
  2. **Filamento** (o resina): muestra de color, perfil y engranaje.
  3. **Ajustes de impresión:**
     - perfil y engranaje;
     - **Soportes** en chips: Ninguno, Solo desde la cama, En todas partes, Solo pintados;
     - **Relleno:** campo en % y chips 0, 10, 15, 20, 30, 50 y 100 %;
     - **Borde:** interruptor.
  4. **Datos del laminado** (después de laminar): filamento usado en g, m y mm³; costo; tiempo estimado (normal y
     silencioso); altura de capa; perímetros; relleno; soportes.
  5. **Ajustes rápidos:**
     - pestañas Impresión, Filamento e Impresora;
     - una lista corta de ajustes por grupo, por ejemplo capas y perímetros, relleno o velocidad;
     - «+ Agregar» suma un ajuste a la lista, y «Todos los ajustes» abre la página.
  6. **Abajo, fijo:**
     - el interruptor **«Laminar automáticamente»**: lamina después de cada cambio;
     - el **botón principal**;
     - a su izquierda, una flecha que esconde el panel.
- **Modo compacto (panel escondido):** sobre la vista 3D, abajo a la derecha, flotan una flecha para volver a mostrar
  el panel, el botón principal y su flecha.

### Botón principal

1. Sin piezas: «Laminar ahora», apagado.
2. Con piezas sin laminar: «Laminar ahora».
3. Laminado: la salida elegida, con una flecha al lado para elegir otra:
   - **Exportar G-code;**
   - **Imprimir por USB**, si hay una impresora USB conectada (diseño);
   - **Enviar** a la impresora de red conectada (diseño).

   La flecha muestra solo las salidas posibles, con la elegida marcada.
4. Con «Laminar automáticamente», el paso 2 no hace falta.

### Vista previa (después de laminar)

- Leyenda arriba a la izquierda:
  - por tipo de línea: perímetro, perímetro externo, relleno, sólido, sólido de arriba, puente, otros;
  - de cada tipo, tiempo, porcentaje y filamento en m y g;
  - tiempo de la primera capa y total;
  - botones para mostrar u ocultar cada tipo de movimiento.
- Deslizador vertical de capas, con la Z y el número de capa en cada extremo, y deslizador horizontal de movimientos
  dentro de la capa.

### Páginas de ajustes (Impresión, Filamento, Impresora)

- **Arriba:** el perfil (elegir, guardar, renombrar, borrar, comparar).
- **A la izquierda:** las páginas del perfil, por ejemplo Capas y perímetros, Relleno, Falda y borde, Material de
  soporte.
- **En el centro:** los ajustes en grupos con títulos, cada uno con su unidad.
  - Algunos grupos muestran textos calculados. Por ejemplo: «La capa de arriba mide 1 mm con capas de 0,2 mm; el
    mínimo es 0,7 mm».
- **Impresora** tiene dos páginas:
  - **Ajustes:** el perfil de la impresora;
  - **Dispositivos:** impresoras por USB y por red, con su estado, cámara y envío. Cada impresora puede usar una
    cámara USB de la computadora;
  - un selector arriba pasa de una a otra, y la sección abre la última usada.

### Más

Tarjetas armadas con lo que antes estaba en los menús, cada acción con su atajo y apagada si no se puede usar:
- **Tisma:** Thingiverse, cuenta de Tisma Cloud, Preferencias;
- **Archivo:** proyecto nuevo, abrir, recientes, guardar, importar, exportar, convertir G-code binario, reparar STL,
  visor de G-code, salir;
- **Editar:** seleccionar, borrar, deshacer, copiar, pegar, recargar, ajustes de la cama, buscar;
- **Ventana**, **Vista**, **Configuración** (asistente, actualizaciones, preferencias, idioma) y **Ayuda**.

**Ayuda > Informar de un problema:**
- manda a Tisma Cloud qué pasó y los pasos;
- si la persona quiere, también la versión, el sistema, la impresora en uso y las últimas 500 líneas del registro de
  la sesión, que se ven antes de mandar;
- se puede mandar sin cuenta.

### Asistente de configuración (primer uso)

1. **Impresoras:**
   - una búsqueda por nombre, la lista de marcas y la de impresoras de la marca, cada una con su casilla;
   - al buscar, aparece «Todos los resultados (n)» y las marcas con resultados;
   - a la derecha, la foto del modelo (si la hay) y sus boquillas, cada una con su casilla. Las impresoras se
     muestran una vez por modelo, no una vez por boquilla. Abajo: «Otra boquilla se puede elegir después, junto a la
     impresora»;
   - abajo: cuántas impresoras hay elegidas, y «Mi impresora no está en la lista: definir una personalizada»;
   - se pueden elegir varias marcas, de filamento y de resina.
2. **Filamentos:**
   - el paso aparece después de elegir una impresora;
   - cuatro columnas para filtrar: impresora, tipo, marca y perfil, con casillas;
   - botones Todos y Ninguno;
   - aviso: los marcados con * no sirven para alguna de las impresoras instaladas;
   - se ofrecen solo los filamentos del diámetro que usan las impresoras elegidas (diseño).
3. **Terminar:** instala las impresoras con sus ajustes y filamentos.

Agregar un filamento después no vuelve a pedir la impresora (diseño).

### Panel de Ingeniería

- Se abre sobre la vista 3D, a la izquierda, con el nombre de la pieza y dos pestañas: Estructural y Aerodinámica.
- **Condiciones:**
  - material, y un botón para editar la tabla de materiales;
  - temperatura en °C, factor de seguridad y resolución, con el tamaño resultante («unos 60 000 vóxeles de 0,61 mm»);
  - «Pieza impresa (paredes y relleno del perfil)» y «Calcular en la nube».
- **Apoyos, cargas y límites:** «Tocar la pieza para agregar». Modos: Nada (girar la vista), Cara fija, Carga en una
  cara, Carga en un punto y Límite de desplazamiento.
- **Analizar:** apagado hasta que haya una cara fija y una carga, con esa explicación debajo.
- **Optimizar:**
  - «Piel + núcleo»: piel no planar automática y el relleno más liviano con nervios;
  - secciones plegables: Relleno más liviano, Refuerzo local, Lattice 3D y Orientación de impresión
    (ver `structural_optimization.md`).

### Con una impresora de resina

- Se ocultan las funciones que solo sirven para FDM: capas no planares, arcos en voladizos, pintar costuras, piel
  difusa y multimaterial, ajustes por cama.
- En Ingeniería quedan el análisis estructural y «Paredes y relleno interior».
- Calibrar muestra las pruebas de resina.
- Dispositivos queda.

## Pruebas de aceptación

Escenarios para comprobar en la pantalla, como se observaron en Tisma Slicer.

| # | Escenario | Resultado esperado |
|---|---|---|
| UI1 | Primer arranque con una carpeta de configuración vacía | Se abre el asistente en «Impresoras», con Terminar apagado y «Ninguna impresora elegida» |
| UI2 | Buscar «MK4S» | Marcas: «Todos los resultados (4)» y la marca con resultados; las 4 impresoras que lo contienen |
| UI3 | Marcar una impresora y la boquilla de 0,4 | Aparece el paso Filamentos; el texto pasa a «1 impresora elegida»; se activan Siguiente y Terminar |
| UI4 | Terminar | Se abre la placa con la impresora, la boquilla de 0,4, un filamento y un perfil de impresión elegidos; barra de estado «Sin objetos» |
| UI5 | Placa vacía | Botón principal «Laminar ahora», apagado; herramientas de Organizar y Pintar apagadas |
| UI6 | Cargar una pieza | Barra de estado «Sin laminar» y «1 objeto»; «Laminar ahora» activo |
| UI7 | Laminar | Aparecen la vista previa con su leyenda y deslizadores y la tarjeta de datos del laminado; botón principal «Exportar G-code» con su flecha; barra de estado con tiempo, peso y costo |
| UI8 | Flecha del botón principal, sin impresoras conectadas | Solo «Exportar G-code», marcada |
| UI9 | Esconder el panel | La vista 3D ocupa todo el ancho; abajo a la derecha flotan la flecha para volver, el botón principal y su flecha |
| UI10 | Página Impresión en modo Simple | Pocas páginas (capas y perímetros, relleno, falda y borde, soportes) y, abajo a la izquierda, el aviso de modo Simple con «Mostrar todos los ajustes» |
| UI11 | Calibrar con una impresora que calibra sola la primera capa | Sin la prueba de primera capa |
| UI12 | Ingeniería > Análisis sin apoyos ni cargas | «Analizar» apagado, con «Agregar una cara fija y una carga» |
