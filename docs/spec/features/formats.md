# Formatos de archivo: modelos que se leen, proyectos y exportaciones

**Qué hace:** abre las piezas en los formatos más comunes, guarda el proyecto entero en un 3MF y exporta la placa como
malla. Esta ficha dice qué entra y qué sale. El G-code está en `GCODE_OUTPUT.md`; los archivos de resina, en `resin.md`;
los paquetes de perfiles, en `presets.md`; las mezclas de colores en el proyecto, en `color_mixing.md`.

**Fase:** leer STL, OBJ y 3MF, en F1. El resto, P4.

**Fuentes:**
- 3MF Consortium, [3MF Core Specification](https://github.com/3MFConsortium/spec_core/blob/master/3MF%20Core%20Specification.md):
  el paquete, el modelo, las unidades y la transformación de cada pieza;
- 3MF Consortium, «Production Extension» (`https://github.com/3MFConsortium/spec_production`): mallas en archivos
  aparte, para los proyectos de Bambu Studio y OrcaSlicer (regla 18);
- las mediciones de abajo, del 25-10-2026, con Tisma Slicer como caja negra desde la línea de comandos, y un proyecto
  guardado desde la interfaz (24-10-2026).

## Leer modelos

| Formato | Se lee | Notas medidas |
|---|---|---|
| STL binario y de texto | Sí | Los dos dan la misma pieza (prueba FM1) |
| OBJ | Sí | Solo la malla (prueba FM2) |
| AMF | Sí | (prueba FM2) |
| 3MF | Sí | De cualquier programa; respeta la unidad del archivo (prueba FM3) |
| STEP | Sí | Se convierte en triángulos al abrir (prueba FM4) |
| SVG | Sí | Se convierte en un relieve de 10 mm de alto (prueba FM5) |
| ZIP con modelos adentro | Solo en la interfaz | Desde la línea de comandos: «Loading of a model file failed» (prueba FM6) |

1. **Unidades:** STL, OBJ y AMF se leen en milímetros. En un 3MF manda la unidad del archivo: `inch` multiplica por
   25,4 (prueba FM3). Para un STL en pulgadas, la interfaz tiene «Import STL (Imperial Units)».
2. **STEP:** la pieza se convierte en triángulos al abrirla. Un bloque de 20 × 20 × 10 mm con un agujero de 4 mm de
   radio da 372 triángulos y 3497,8 mm³ (el exacto es 3497,3). El motor elige la finura de la conversión; la pieza
   tiene que quedar cerrada. El lector es propio, sin bibliotecas de terceros (`step_reader.md`).
3. **SVG:** el dibujo se convierte en una pieza:
   - el contorno sale del trazado, con la regla de relleno del archivo (`evenodd` deja el agujero);
   - la medida es la del archivo (`20mm` × `10mm`);
   - el alto es 10 mm;
   - la pieza queda centrada en X e Y sobre el origen (prueba FM5).
4. **Reparación al abrir:** se informa y se corrige:
   - caras dadas vuelta: se dan vuelta solas, la pieza se lamina igual (prueba FM7);
   - agujeros en la malla: se informan los bordes abiertos y la pieza se lamina con las capas cerradas (prueba FM8).
   Los datos de la reparación se guardan con la pieza en el proyecto (regla 9).
5. **Varias partes:** un 3MF con un objeto de dos partes se abre como un objeto con dos partes. Exportado como STL,
   queda una sola malla con los triángulos de las dos (prueba FM9).

## El proyecto 3MF

6. **Paquete:** un ZIP con las partes del estándar más las del programa:

| Archivo | Qué tiene |
|---|---|
| `[Content_Types].xml`, `_rels/.rels` | Los del estándar; las relaciones apuntan al modelo y a la miniatura |
| `3D/3dmodel.model` | Las mallas y la ubicación de cada objeto en la cama, en milímetros; datos del proyecto: título, fechas, programa |
| `Metadata/thumbnail.png` | Miniatura de 256 × 256 px (solo desde la interfaz) |
| `Metadata/Slic3r_PE.config` | Todos los ajustes del proyecto (impresión, material, impresora), uno por línea (`; clave = valor`), 439 en la medición (solo desde la interfaz) |
| `Metadata/Slic3r_PE_model.config` | Por objeto y por parte: nombre, tipo de parte, matriz, extrusor, de qué archivo vino, datos de la reparación, ajustes propios |
| `Metadata/Prusa_Slicer_layer_config_ranges.xml` | Los rangos de alturas de cada objeto: desde, hasta y sus ajustes (`model_tools.md`, regla 6), si hay |
| `Metadata/Prusa_Slicer_wipe_tower_information.xml` | Posición y giro de la torre de purga, por cama |
| `Metadata/Prusa_Slicer_full_spectrum.json` | Las mezclas de colores (`color_mixing.md`), si hay |

7. **Mallas:** en `3D/3dmodel.model`, cada objeto es una malla con sus vértices y triángulos. Las partes de un objeto
   van en la misma malla, una detrás de otra; el archivo de objetos dice qué triángulos son de cada parte (`firstid` y
   `lastid`).
8. **Ubicación:** cada objeto tiene en `<build>` una transformación con la posición en la cama. La malla se guarda con
   la transformación de la parte ya aplicada, y el archivo de objetos guarda la matriz de la parte (`matrix`), para
   recuperar la malla original al abrir (prueba FM10).
9. **Datos por parte:** nombre, tipo (`ModelPart` y los demás tipos de `model_tools.md`), matriz, extrusor, archivo de
   origen con el número de objeto y de parte y el corrimiento, y lo que corrigió la reparación (bordes arreglados,
   caras degeneradas, quitadas o dadas vuelta).
10. **Lo que todavía no se midió** del proyecto (hace falta guardarlo desde la interfaz):
    - ajustes por objeto, por parte y por rango de alturas;
    - pintura (soporte, costura, piel difusa, colores);
    - modificadores;
    - varias camas;
    - análisis de Ingeniería;
    - capas no planares;
    - abrir proyectos de Bambu Studio y OrcaSlicer.
11. **Compatibilidad (propuesta):** el motor escribe el 3MF del estándar más su propio archivo de datos, y lee
    también los datos de los proyectos de Tisma Slicer (tabla de la regla 6), para no perder los proyectos que la
    persona ya tiene.

## Exportar

12. **STL:** binario, con la cabecera de 80 bytes en cero y la cantidad de triángulos. Toda la placa en una sola malla,
    en la posición de la cama (la pieza que estaba en el origen queda con su mínimo en 0, 0, 0) (prueba FM11).
13. **OBJ:** de texto, con los vértices compartidos (8 vértices y 12 caras para una caja) (prueba FM11).
14. **3MF desde la línea de comandos:** sin miniatura y sin el archivo de ajustes; con la malla, la ubicación y el
    archivo de objetos (prueba FM10).
15. **Con soportes y las trayectorias:** la interfaz tiene «Export Plate as STL/OBJ Including Supports» y «Export
    Toolpaths as OBJ». No se midieron.

16. **Exportar con soportes y trayectorias** (completa la regla 15):
    - «con soportes» suma a la malla de la placa el soporte generado al laminar, como sólidos capa por capa (cada
      región de soporte de una capa, levantada a la altura de su capa);
    - «trayectorias como OBJ» escribe cada extrusión como un tubo de sección rectangular de su ancho y su altura, con
      un material por tipo de línea (`GCODE_OUTPUT.md`), para verla en otro programa;
    - las dos piden un laminado al día; si no lo hay, laminan primero.
17. **Abrir un ZIP:** se listan los modelos que trae (STL, OBJ, 3MF, AMF, STEP) y se abren todos como objetos de la
    placa. Los otros archivos se ignoran, con un aviso que dice cuántos. Un ZIP dentro de otro no se abre.
18. **Abrir proyectos de Bambu Studio y OrcaSlicer:**
    - son 3MF que guardan las mallas en archivos aparte, referidos con la extensión de producción del estándar 3MF
      (3MF Consortium, «Production Extension»);
    - se leen las mallas, sus nombres, sus partes y su ubicación; si el proyecto tiene varias placas, cada placa pasa a
      una cama (`model_tools.md`, regla 26);
    - los ajustes de esos programas no se toman: un diálogo avisa «Se abrieron las piezas; los ajustes del proyecto
      no se usan» y ofrece elegir impresora y perfiles;
    - sin medir todavía con proyectos reales de cada programa: hace falta un archivo de muestra de cada uno, con
      permiso para usarlo en las pruebas.
19. **Guardar en una tarjeta o memoria:**
    - el programa detecta las unidades extraíbles conectadas (Windows: unidades de tipo extraíble; Linux: lo montado
      bajo `/media` o `/run/media`; macOS: lo montado bajo `/Volumes` que no sea el disco del sistema);
    - con una conectada, la salida «Guardar en la tarjeta» (Ctrl+U) aparece en el menú del botón principal
      (`interface.md`) y escribe ahí el G-code con su nombre (`gcode_templates.md`);
    - al terminar, una notificación ofrece **Expulsar** (Ctrl+T), que desmonta la unidad y avisa cuando se puede
      sacar;
    - si se desconecta durante la escritura, se avisa y el archivo a medias se borra si la unidad vuelve.
20. **Miniaturas en el G-code** (`gcode_extras.md`): tamaños y formatos salen del perfil de la impresora. Su forma
    exacta en el archivo (comentarios, codificación y largo de cada línea) se mide guardando un G-code desde la
    interfaz de Tisma Slicer: queda pendiente.

## Pruebas

Medidas en Tisma Slicer (25-10-2026) desde la línea de comandos: `--info` da medidas, triángulos y partes; las piezas
se laminan con los ajustes base de `walls_extra.md`.

| # | Archivo | Resultado |
|---|---|---|
| FM1 | Cubo de 20 × 20 × 10 mm en STL binario y en STL de texto | Los dos: 20 × 20 × 10, 12 triángulos, cerrado, 4000 mm³ |
| FM2 | La misma caja en OBJ (8 vértices, 12 caras) y en AMF | 20 × 20 × 10, 12 triángulos, cerrada, 4000 mm³ |
| FM3 | La caja en un 3MF sin datos del programa; el mismo con `unit="inch"` | Milímetros: 20 mm de lado. Pulgadas: 508 mm de lado |
| FM4 | STEP: bloque de 20 × 20 × 10 mm con un agujero de 4 mm de radio | 372 triángulos, cerrado, 3497,8 mm³ |
| FM5 | SVG de 20 × 10 mm con un agujero de 4 × 4 mm (`evenodd`) | Pieza de 20 × 10 × 10 mm, de −10 a 10 en X y de −5 a 5 en Y, 32 triángulos, 1840 mm³ (con el agujero) |
| FM6 | ZIP con un STL adentro | Línea de comandos: no lo abre |
| FM7 | La caja en STL con todas las caras dadas vuelta | Se informan caras dadas vuelta; se lamina igual: 50 capas |
| FM8 | La caja en STL sin la tapa (10 triángulos) | No cerrada, 4 bordes abiertos; se lamina con 50 capas y perímetros en todas |
| FM9 | 3MF con un objeto de dos partes de 10 × 20 × 10 mm (extrusores 1 y 2) | Un objeto, 2 partes, 24 triángulos. Exportado a STL: una malla de 24 triángulos |
| FM10 | OBJ de la caja (de 0 a 20) exportado a 3MF | Vértices de 0 a 20 en el modelo; ubicación del objeto: identidad; matriz de la parte con traslado (10, 10, 5) |
| FM11 | La caja exportada a STL y a OBJ | STL: binario, cabecera en cero, 12 triángulos, mínimo en (0, 0, 0). OBJ: 8 vértices, 12 caras |

Reglas 16 a 19:

| # | Escenario | Resultado esperado |
|---|---|---|
| FM20 | Exportar trayectorias como OBJ del cubo de `gcode_motion.md` | Un tubo por extrusión, con el ancho y el alto del G-code, y un material por tipo |
| FM21 | Abrir un ZIP con dos STL y un PDF | Dos objetos en la placa; aviso de 1 archivo ignorado |
| FM22 | Unidad extraíble simulada (una carpeta montada como extraíble) | Aparece «Guardar en la tarjeta»; el G-code queda en la unidad; Expulsar la desmonta |
