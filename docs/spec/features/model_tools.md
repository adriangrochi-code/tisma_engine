# Partes, modificadores, ajustes por pieza y transformaciones

**Qué hace:** un objeto puede tener varias mallas con distintos papeles:
- partes que se imprimen;
- volúmenes que se restan;
- modificadores que cambian los ajustes de la zona que tocan;
- bloqueadores y forzadores de soporte.

Además, un objeto, una parte y un rango de alturas pueden tener sus propios valores de los ajustes. Esta ficha dice
cómo se combinan, medido como caja negra.

Los bloqueadores y forzadores de soporte están en `supports_more.md`; el extrusor por parte, en `multi_extruder.md`;
cómo se agregan desde la interfaz, en `settings_levels.md` (reglas 10 y 11); cómo se guardan en el proyecto, en
`formats.md`. Escalar, girar, cortar y separar están al final. Faltan medir, pintar, espejar, simplificar y los
conectores del corte.

**Fase:** F3 para partes, volúmenes negativos y ajustes por objeto; modificadores y rangos de alturas, P4.

**Fuentes:** mediciones del 25-10-2026 en Tisma Slicer desde la línea de comandos, con proyectos 3MF de un objeto y
varias mallas, y un proyecto guardado desde la interfaz para ver cómo guarda un rango de alturas.

## Tipos de malla dentro de un objeto

| Tipo | Se imprime | Efecto |
|---|---|---|
| Parte | Sí | Su volumen se suma al objeto |
| Volumen negativo | No | Su volumen se resta de todas las partes y regiones del objeto |
| Modificador | No | La zona que comparte con las partes toma los ajustes del modificador |
| Bloqueador de soporte | No | Ver `supports_more.md` |
| Forzador de soporte | No | Ver `supports_more.md` |

## Reglas

1. **Partes que se tocan o se pisan:** se unen en una sola pieza. Dos cajas de 20 mm que se pisan 10 mm dan un solo
   contorno de 30 mm, sin paredes en la unión (prueba MT2).
2. **Volumen negativo:**
   - resta en todas las capas que cruza, también si corta varias regiones (prueba MT10);
   - no cambia el tamaño del objeto: un negativo más alto que la pieza no agrega capas, y un negativo que sale por los
     costados no mueve el centro (prueba MT1);
   - un hueco cerrado adentro de la pieza se trata como cualquier hueco: paredes alrededor, las capas sólidas de
     arriba en el piso del hueco y las de abajo en su techo, que empieza con un puente (prueba MT3).
3. **Modificador:**
   - solo cuenta lo que comparte con las partes: un modificador que sale de la pieza no agrega material ni capas (prueba
     MT5);
   - la zona que comparte forma una región con sus propios ajustes;
   - si los ajustes cambian solo el relleno, la región no lleva paredes en su borde: las paredes rodean al objeto entero
     y el relleno cambia de patrón y densidad en el borde del modificador (prueba MT4);
   - si cambian la cantidad de perímetros, cada región lleva sus propias paredes, también en el borde entre las dos
     (prueba MT5).
4. **Ajustes que no se cambian por región:** la altura de capa es del objeto. Un modificador con otra altura de capa
   no cambia nada (prueba MT6); la altura se cambia por rango de alturas (regla 6).
5. **Precedencia:** cada valor sale del primero que lo define, en este orden:

| Orden | De dónde | Prueba |
|---|---|---|
| 1 (gana) | Modificador | MT9 |
| 2 | Parte | MT8, MT9 |
| 3 | Rango de alturas | MT9, MT13 |
| 4 | Objeto | MT7, MT13 |
| 5 | Perfil de impresión | — |

   Cuando dos mallas del mismo tipo se pisan, gana la que está después en la lista del objeto:
   - dos partes con distintos perímetros: la zona común toma los de la segunda (prueba MT11);
   - dos modificadores: la zona común toma los del segundo (prueba MT12).
   El motor muestra el orden en la lista de la pieza y deja cambiarlo.
6. **Rango de alturas:**
   - un tramo de Z (desde, hasta) del objeto con sus ajustes, incluida la altura de capa;
   - la capa cuya parte de arriba queda justo en el límite es del rango de abajo: con un rango de 4 a 6 mm a 0,1 mm,
     las capas van 4,0 → 4,1 … 6,0 y después 6,2 (prueba MT7b);
   - la interfaz crea por defecto un rango de 0 a 2 mm con la altura de capa del perfil y el extrusor «por defecto».
7. **Ajustes por objeto:** reemplazan a los del perfil solo en ese objeto (prueba MT7). Un objeto con relleno al 40 %
   lo lleva aunque el perfil diga 0 %.

## En el motor

8. **Modelo de datos:** cada objeto tiene una lista ordenada de mallas, cada una con su tipo y sus ajustes propios, y
   una lista de rangos de alturas. Los ajustes propios usan las mismas claves que el perfil (`walls.count`,
   `infill.density`, `infill.pattern`, `layers.height` solo en objeto y rango).
9. **Validar:** al poner un ajuste que no se puede cambiar por región (como `layers.height` en un modificador), el
   motor lo rechaza con un aviso en lugar de ignorarlo en silencio.
10. **Regiones:** una región por combinación de ajustes distinta. Dos regiones con los mismos ajustes de paredes
    comparten las paredes; si los de paredes cambian, cada una lleva las suyas (regla 3).

## Escalar, girar, cortar y separar

Medido desde la línea de comandos de Tisma Slicer; la interfaz tiene las mismas herramientas (`interface.md`). Las
claves son propuestas:

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `transform.scale` | factor o % | 1 | Escala igual en los tres ejes: `2` y `200%` son lo mismo |
| `transform.scale_to_fit` | mm (x, y, z) | — | Escala igual en los tres ejes hasta el mayor tamaño que entra en esa caja |
| `transform.rotate_x`, `transform.rotate_y`, `transform.rotate_z` | ° | 0 | Gira alrededor del centro de la caja del objeto |
| `transform.cut_z` | mm | — | Corta el objeto con un plano horizontal a esa altura |
| `transform.split` | — | no | Cada pedazo suelto de la malla pasa a ser un objeto |

11. **Escala:** alrededor del centro de la caja del objeto. `transform.scale_to_fit` usa el menor de los tres
    cocientes (caja pedida / tamaño): una caja de 30 × 20 × 10 mm con 60, 60, 60 se duplica; con 15, 100, 100 queda a
    la mitad (pruebas MT14 y MT15).
12. **Giro:** alrededor del centro de la caja; después se aplica la ubicación (`placement.md`): el objeto baja a la
    cama y se centra. Un giro de 90° en X de una caja de 20 mm de fondo y 10 de alto la deja de 20 mm de alto (prueba
    MT16).
13. **Corte:** las dos mitades quedan como objetos separados, sin girarlas; cada una baja a la cama y se ordenan una al
    lado de la otra. Un cono de 10 mm de radio y 10 de alto cortado a 5 mm da un tronco de 915,9 mm³ y una punta de
    130,8 mm³ (el exacto es 916,3 y 130,9) (prueba MT17).
14. **Separar:** dos cajas sueltas en un mismo archivo:
    - sin separar, son un objeto y conservan la distancia entre ellas (20 mm);
    - separadas, son dos objetos y se ordenan con `placement.spacing` entre ellas (6 mm) (prueba MT18).
15. **Copias en grilla:** en Tisma Slicer, la opción de copias en una grilla de filas y columnas cierra el programa con
    una falla de memoria (código 139) con cualquier pieza (prueba MT19). No se copia: el motor pone las copias en la
    grilla pedida, con `placement.spacing` entre cajas, y el conjunto centrado.

## Pruebas

Medidas en Tisma Slicer (25-10-2026) con los ajustes base de `walls_extra.md` (2 perímetros, sin relleno ni capas
sólidas, capas de 0,2 mm, centro en 100, 100). Cada prueba es un 3MF de un objeto; las medidas de X son las del G-code.

| # | Objeto | Resultado |
|---|---|---|
| MT1 | Caja de 20 × 20 × 10 mm menos un negativo de 10 × 10 × 20 mm en el medio | 50 capas; X de 90,2 a 109,8; dos contornos por capa (afuera y el agujero), 2 perímetros cada uno |
| MT2 | Dos cajas de 20 × 20 × 10 mm, la segunda corrida 10 mm en X | Un contorno de 30 mm (X de 85,2 a 114,8), 2 perímetros |
| MT3 | Caja con un negativo cerrado de 10 × 10 × 4 mm (Z de 3 a 7), 3 capas sólidas arriba y abajo | Agujero con paredes de Z 3,2 a 7,0. Piso: Z 2,6 puente (la pieza no tiene relleno), 2,8 relleno sólido, 3,0 superficie de arriba. Techo: Z 7,2 puente, 7,4 y 7,6 relleno sólido |
| MT4 | Caja de 40 × 20 × 10 mm, relleno `grid` 20 %; modificador sobre la mitad +X con `rectilinear` 60 % | Capa 25: un solo par de paredes alrededor de todo; mitad −X `grid` (45° y 135°) 183 mm; mitad +X una sola dirección, 541 mm |
| MT5 | Caja de 20 × 20 × 10 mm; modificador de Z 5 a 20 y X de 10 a 30 (sale de la pieza) con 5 perímetros | 50 capas, X igual. Capa 10: 2 perímetros. Capa 40: mitad −X con 2, mitad +X con 5, cada mitad con su contorno |
| MT6 | La caja de MT4 con un modificador de altura de capa 0,1 | 50 capas: sin cambio |
| MT7 | Caja de 20 × 20 × 10 mm con ajustes del objeto: 3 perímetros y relleno 40 % | 3 perímetros y relleno en todas las capas |
| MT7b | La misma caja con dos rangos: 4 a 6 mm con capa de 0,1; 6 a 10 mm con 4 perímetros y relleno 50 % | 60 capas: de 4,1 a 6,0 cada 0,1; desde 6,2, 4 perímetros y relleno |
| MT8 | Dos cajas separadas en un objeto; la segunda con 4 perímetros propios | Primera con 2, segunda con 4 |
| MT9 | Caja de 40 × 20 × 10 mm; objeto con 3 perímetros, la parte con 4, modificador +X con 5, rango de 6 a 10 mm con 6 | Mitad −X con 4 y mitad +X con 5 en todas las capas; el rango no cambia nada |
| MT10 | Caja de 40 × 20 × 10 mm con modificador +X de 4 perímetros y un negativo de 10 × 10 mm que cruza el borde de las dos regiones | Agujero en las dos regiones, cada una con sus perímetros |
| MT11 | Las dos cajas de MT2, la segunda con 4 perímetros; después, el mismo par con la de 4 primero | Segunda con 4: la zona común (X 95 a 115) con 4. Primera con 4: solo X 105 a 115 con 4 |
| MT12 | Caja de 40 × 20 × 10 mm; modificador X 10 a 30 con 3 perímetros y otro X 20 a 41 con 5 | X 0 a 10: 2; X 10 a 20: 3; X 20 a 40: 5 |
| MT13 | Caja de 20 × 20 × 10 mm; objeto con 3 perímetros y rango de 6 a 10 mm con 5 | Capa 10: 3. Capa 40: 5 |

Desde MT14, un STL de una caja de 30 × 20 × 10 mm que no está en el origen (de 5, 7, 3 a 35, 27, 13), salvo donde se
dice otra cosa:

| # | Ajustes | Resultado |
|---|---|---|
| MT14 | `transform.scale` 2 y 200 %; 150 % | 60 × 40 × 20 mm (100 capas) y 48 000 mm³; 45 × 30 × 15 mm y 20 250 mm³ |
| MT15 | `transform.scale_to_fit` (60, 60, 60) y (15, 100, 100) | 60 × 40 × 20 mm; 15 × 10 × 5 mm |
| MT16 | `transform.rotate_z` 90 y 45; `transform.rotate_x` 90; `transform.rotate_y` 90 | 20 × 30 × 10; 35,36 × 35,36 × 10; 30 × 10 × 20; 10 × 20 × 30 mm. En la cama: con 45°, X e Y de 82,6 a 117,4; con 90° en X, 100 capas |
| MT17 | `transform.cut_z` 5, en la caja y en un cono de 10 mm de radio y 10 de alto | Caja: dos objetos de 30 × 20 × 5 mm, 3000 mm³ cada uno, los dos impresos. Cono: tronco de 20 × 20 × 5 mm (915,9 mm³) y punta de 10 × 10 × 5 mm (130,8 mm³) |
| MT18 | Dos cajas sueltas de 10 × 10 × 10 y 10 × 10 × 5 mm a 20 mm, sin y con `transform.split` | Sin separar: X de 80 a 90 y de 110 a 120. Separadas: X de 87 a 97 y de 103 a 113 |
| MT19 | Cubo de 20 mm y la caja, copias en grilla de 2 × 2 y de 2 × 1 | Tisma Slicer se cierra (código 139). No se copia |

