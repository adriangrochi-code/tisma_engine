# Resina (MSLA)

**Qué hace:** para impresoras de resina con pantalla (MSLA):
- convierte cada capa de la pieza en una imagen en grises de la pantalla;
- guarda las imágenes y los movimientos en el archivo de la impresora (acá, el formato GOO de Elegoo);
- avisa de las ventosas: huecos que el film sella al despegar cada capa;
- ofrece dos calibraciones de resina: la torre de exposición y el bloque de medidas.

**Fase:** después de la primera versión.
- Tisma Slicer hereda de PrusaSlicer los soportes, la base (pad) y el vaciado de resina. Esas partes quedan para otra
  ficha, escrita desde la documentación de usuario de Prusa.
- Lo de esta ficha lo escribió el equipo del dueño: el formato GOO, el detector de ventosas y las calibraciones. La
  parte de imagen y exposición se midió como caja negra.

**Fuentes:**
- Elegoo, [«Goo Format Spec V1.2»](https://raw.githubusercontent.com/elegooofficial/GOO/main/Goo%20Format%20Spec%20V1.2.pdf):
  estructura del archivo GOO. Es un PDF de 8 páginas en el repositorio `elegooofficial/GOO`, que tiene licencia GPL
  v3. Se lee solo el PDF, que describe un formato y no trae código.
- Mango3D, [«Cupping»](https://doc.mango3d.io/?p=2272): qué es una ventosa en la impresión de resina y cómo se evita
  (agujeros de drenaje, orientación).

## Ajustes

Las velocidades se dan en mm/s y las esperas en s. El archivo GOO guarda las velocidades en mm/min.

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `machine.display_pixels_x`, `machine.display_pixels_y` | px | 4098, 2560 | Resolución de la pantalla |
| `machine.display_width`, `machine.display_height` | mm | 143,43, 89,6 | Tamaño del área que ilumina la pantalla |
| `machine.display_mirror_x`, `machine.display_mirror_y` | — | sí, no | Espejar las imágenes |
| `machine.max_height` | mm | 150 | Altura máxima de impresión |
| `resin.layer_height` | mm | 0,05 | Altura de todas las capas |
| `resin.exposure`, `resin.bottom_exposure` | s | 2,5, 30 | Exposición de las capas normales y de las de fondo |
| `resin.bottom_layers` | — | 5 | Capas de fondo, que se pegan a la plataforma |
| `resin.elephant_foot` | mm | 0,2 | Cuánto se achica la primera capa en XY |
| `resin.scale_x`, `resin.scale_y`, `resin.scale_z` | — | 1 | Corrección de escala de la resina (se expande o se contrae al curar) |
| `resin.lift_distance`, `resin.lift_speed` | mm, mm/s | 5, 1,6667 | Subida de la plataforma después de cada capa normal |
| `resin.bottom_lift_distance`, `resin.bottom_lift_speed` | mm, mm/s | 7, 1,3333 | Lo mismo en las capas de fondo |
| `resin.retract_speed` | mm/s | 2,5 | Velocidad de bajada; la distancia es la de subida |
| `resin.wait_before_exposure` | s | 2,5 | Espera con la plataforma abajo, antes de exponer |
| `resin.wait_after_cure` | s | 1 | Espera después de exponer, antes de subir |
| `resin.wait_after_lift` | s | 0 | Espera arriba, antes de bajar |
| `resin.light_pwm`, `resin.bottom_light_pwm` | 0 a 255 | 255 | Potencia de la luz |
| `resin.antialiasing` | — | 8 | Nivel de suavizado que se anota en el archivo |
| `resin.density` | g/ml | 1 | Para el peso de la cabecera |

Los valores por defecto son los de la Elegoo Mars 5 con la resina estándar genérica:
- la pantalla y la altura salen de las especificaciones publicadas por Elegoo;
- 2,5 s y 30 s de exposición están dentro del rango que Elegoo publica para su resina estándar (2,5 a 3 s y 25 a
  35 s). Es un punto de partida: con cada resina hay que imprimir la torre de exposición.

## Reglas

### Capas e imágenes

1. **Capas:** todas miden `resin.layer_height`. La capa `n` (desde 0) tiene su tope en `Z = (n + 1) · h`.
2. **Píxel:** `p = machine.display_width / machine.display_pixels_x` (0,035 mm en la Mars 5; igual en Y).
3. **Ubicación en la imagen:**
   - la imagen es la pantalla vista desde arriba, con la fila 0 en la Y máxima: `columna = X / p`, `fila =
     (machine.display_height − Y) / p`, con X e Y medidas desde la esquina de la pantalla;
   - con `machine.display_mirror_x`, las columnas van al revés; con `machine.display_mirror_y`, las filas;
   - el centro de la pantalla cae en el centro de la imagen (prueba RE3).
4. **Grises:** cada píxel vale la fracción de su área que cubre la sección, por 255 (±1). Un píxel del borde de un
   cubo cubierto en un 71,4 % vale 183, y el de la esquina (71,4 % de 71,4 %) vale 131 (prueba RE2).
5. **Escala:** antes de cortar, la pieza se escala por `resin.scale_x`, `resin.scale_y` y `resin.scale_z` (prueba RE6).
6. **Pie de elefante:**
   - la capa `i` de las `resin.bottom_layers` de fondo se achica en XY `resin.elephant_foot · (1 − i / N)`, con `N`
     la cantidad de capas de fondo;
   - la primera capa se achica entero, y la última de fondo casi nada (prueba RE5);
   - se midió con la pieza apoyada en la plataforma, sin soportes ni pad.

### Exposición y movimientos

7. **Capas de fondo:** las primeras `resin.bottom_layers` capas usan `resin.bottom_exposure`, la subida de fondo y
   `resin.bottom_light_pwm`. Con 0, igual la primera capa es de fondo (prueba RE4). No hay transición gradual: la
   capa siguiente ya usa los valores normales.
8. **Movimientos de cada capa:** subida a la distancia y velocidad de la capa; bajada a la misma distancia, a
   `resin.retract_speed`; esperas de `resin.wait_after_cure` antes de subir, `resin.wait_after_lift` arriba y
   `resin.wait_before_exposure` antes de exponer. No hay segunda etapa de subida ni de bajada.

### Archivo GOO

9. **Estructura:** la de la especificación de Elegoo, con todos los números en big-endian:
   - una cabecera de 195 477 bytes;
   - por cada capa, su definición, el tamaño de la imagen, la imagen y el separador `0D 0A`;
   - un final fijo de 11 bytes: `00 00 00 07 00 00 00 44 4C 50 00`.
10. **Cabecera:**

    | Campo de la especificación | Valor |
    |---|---|
    | Versión, marca | `V3.0`; `07 00 00 00 44 4C 50 00` |
    | Programa, versión, fecha | Nombre y versión del motor; fecha de creación, `AAAA-MM-DD hh:mm:ss` |
    | Impresora, tipo, perfil de resina | Modelo de la impresora (`MARS 5`); `MSLA`; nombre del perfil de resina, vacío si no hay |
    | Suavizado, gris, desenfoque | `resin.antialiasing`; 1; 0 |
    | Vistas previas | 116 × 116 y 290 × 290 píxeles en RGB565 |
    | Capas, resolución, espejos | Cantidad de capas; `machine.display_pixels_*`; `machine.display_mirror_*` |
    | Tamaño de la plataforma | `machine.display_width`, `machine.display_height`, `machine.max_height` |
    | Espesor, exposiciones, capas de fondo | `resin.layer_height`; `resin.exposure`, `resin.bottom_exposure`; `resin.bottom_layers` |
    | Modo de espera | 1 (esperas fijas); tiempo de apagado 0 |
    | Esperas, de fondo y normales | Antes de subir: `resin.wait_after_cure`. Después de subir: `resin.wait_after_lift`. Después de bajar: `resin.wait_before_exposure` |
    | Subida y bajada, de fondo y normales | Distancias de subida en mm, velocidades × 60 (mm/min). Bajada con la misma distancia que la subida, a `resin.retract_speed` × 60 |
    | Segunda etapa | Todo 0 |
    | Potencia de la luz | `resin.bottom_light_pwm`, `resin.light_pwm` |
    | Modo avanzado | Regla 12 |
    | Tiempo, volumen, peso, precio | Tiempo estimado en s; volumen de la pieza en mm³; volumen × `resin.density`; precio y moneda (`$`) |
    | Comienzo de las capas, escala de grises, capas de transición | 195 477; 1 (0 a 255); 0 |

11. **Cada capa:**
    - pausa 0, y Z de pausa = `machine.max_height`;
    - Z del tope de la capa, exposición, esperas, subida, bajada y potencia, según la regla 7;
    - la imagen en el RLE de la especificación: empieza con `0x55` y termina con una suma de control, el complemento a
      uno de la suma de 8 bits de los bytes entre los dos (prueba RE1).
12. **Modo avanzado:**
    - según la especificación, con 1 la impresora usa los valores de cada capa, y con 0 los de la cabecera;
    - el motor escribe 1 cuando alguna capa tiene valores distintos de los de la cabecera, como en la torre de
      exposición;
    - Tisma Slicer escribe siempre 0, aun en la torre (prueba CR1): eso no se copia.

### Ventosas

13. **Qué es:**
    - la pieza cuelga de la plataforma y cada capa nueva se cura contra el film;
    - un hueco cerrado del lado de la plataforma y abierto hacia el film queda sellado por el film al despegar;
    - el vacío puede arrancar o deformar la impresión.
14. **Detección:** capa por capa, desde la plataforma.
    - El aire que rodea la pieza está ventilado, y también el hueco que sigue hacia abajo hasta ese aire.
    - Un hueco encerrado, sin salida hacia abajo, es una ventosa: desde su piso hasta donde se cierra o termina la
      pieza.
    - Una salida cuenta solo si mide al menos 3 mm² (unos 2 mm de diámetro).
    - Los huecos de menos de 4 mm² no se avisan.
    - En la primera capa, un hueco está cerrado por la plataforma o por el pad, salvo que la pieza esté elevada sobre
      soportes.
15. **Aviso:**
    - va en el JSON de avisos (ver `BEHAVIOR.md`), con la pieza, las alturas y el área máxima;
    - dice cómo arreglarlo: un agujero de drenaje de 3 a 5 mm en el piso, o girar la pieza para que el hueco abra hacia
      la plataforma.

### Calibraciones de resina

16. **Torre de exposición:**
    - solo para formatos que guardan la exposición de cada capa (GOO);
    - una base y una franja por exposición: de 1,5 a 3,5 s cada 0,25 s, franjas de 2 mm;
    - cada franja tiene agujeros horizontales de 0,4, 0,6, 0,8, 1,0 y 1,4 mm, que se cierran con exposición de más;
    - atrás, aletas de 0,15, 0,25, 0,35 y 0,5 mm, que faltan o se doblan con exposición de menos;
    - una ranura al frente separa las franjas;
    - se imprime sobre la plataforma, sin soportes ni pad;
    - las capas de fondo mantienen su exposición.
17. **Exposición por franja:** la franja `i` (desde 0) empieza en `Z = i · alto de franja`. La franja 0 es la base,
    con `resin.exposure`. La franja `i ≥ 1` usa `inicio + (i − 1) · paso`, sin pasar del final. A diferencia de las
    calibraciones de FDM (`calibrations.md`), la base no es la primera franja de la prueba (prueba CR1).
18. **Aplicar el resultado:** la exposición de la franja elegida pasa a `resin.exposure`.
19. **Bloque de medidas:**
    - bloque de 30 × 30 × 10 mm;
    - con X, Y y Z medidos, la nueva corrección es `resin.scale_x` × medida deseada / medida impresa, y lo mismo en Y
      y en Z.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra.
- **Ajustes base:** los valores por defecto de esta ficha, sin soportes ni pad, pieza apoyada en la plataforma;
  `resin.bottom_layers` 10 salvo que se diga otra cosa.
- **Lectura del archivo:** con un lector escrito desde la especificación de Elegoo.
- **Ventosas (VE):** la línea de comandos de Tisma Slicer no muestra el aviso. Los casos VE son las pruebas propias
  de Tisma Slicer, sobre secciones dadas por capas.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| RE1 | Cubo de 20 mm en el centro de la pantalla | — | 200 capas, Z de 0,05 a 10,00. Cabecera de 195 477 bytes; comienzo de las capas en 195 477; final de 11 bytes. Volumen 4000 mm³, peso 4,0 g. Imagen de 4586 bytes por capa, con suma de control `0x03` (suma de 8 bits `0xFC`) |
| RE2 | El mismo | — | Píxeles con luz: columnas 1763 a 2334 y filas 994 a 1565. Grises 0, 131, 183 y 255. En la fila 1279: 0, 183, 255 … 255, 183, 0 |
| RE3 | Cubo con centro en X = 30, Y = 20 | Con espejo en X y sin él | Con espejo: columnas 2955 a 3526 (centro a 113,42 mm del borde de la imagen = 143,43 − 30). Sin espejo: 571 a 1142 (centro a 29,98 mm). Filas 1702 a 2274 (centro a 69,58 mm de arriba = 89,6 − 20) |
| RE4 | Cubo de RE1 | `resin.bottom_layers` 10, 5 y 0 | Capas con 30 s y subida de 7 mm: 10, 5 y 1 |
| RE5 | Cubo de RE1 | `resin.elephant_foot` 0,3; 10 capas de fondo | Píxeles achicados por lado, capas 0 a 9: 8, 7, 6, 6, 5, 4, 3, 2, 1, 0 (0,3 mm = 8,6 píxeles). Desde la capa 10, como RE2 |
| RE6 | Cubo de RE1 | `resin.scale_x` 1,05; `resin.scale_z` 1,1 | 600 columnas (21 mm), 220 capas (11 mm), volumen 4620 mm³ |
| RE7 | Cubo de RE1 | Esperas 0,5 / 0,25 / 1,5 s; subida 6 mm a 2 mm/s; de fondo 8 mm a 1 mm/s; bajada a 3 mm/s; luz 200 y 230; suavizado 4 | Cabecera y capas: antes de subir 0,5, después de subir 0,25, después de bajar 1,5; subida 6 a 120 mm/min y 8 a 60; bajada 6 y 8 a 180; luz 200 y 230 (fondo); suavizado 4 |
| CR1 | Cubo de RE1 | Torre: 1,5 a 3,5 cada 0,25, franjas de 2 mm, 5 capas de fondo | Capas 0 a 4: 30 s. Capas 5 a 39 (Z hasta 2,0): 2,5 s. Desde Z 2,05: 1,5 s; desde 4,05: 1,75; desde 6,05: 2,0; desde 8,05: 2,25. La cabecera dice 2,5 s y modo avanzado 0 |
| VE1 | Vaso: 5 capas de piso de 20 × 20 mm y 20 capas de paredes de 2 mm (hueco de 16 × 16) | Sobre la plataforma | Una ventosa, de la capa 5 a la 24, de 256 mm² |
| VE2 | El vaso de VE1 | Elevado sobre soportes | Una ventosa: lo cierra su propio piso |
| VE3 | El vaso con un agujero de 4 × 4 mm en el piso | Elevado | Sin ventosa |
| VE4 | El de VE3 | Sobre la plataforma | Una ventosa: la plataforma tapa el agujero |
| VE5 | El vaso con un agujero de 1 × 1 mm en el piso | Elevado | Una ventosa: el agujero es demasiado chico |
| VE6 | Tubo de paredes de 2 mm, 20 capas, sin piso | Elevado | Sin ventosa. Sobre la plataforma, una |
| VE7 | El vaso de VE1 dado vuelta (abierto hacia la plataforma) | Elevado | Sin ventosa |
| VE8 | Piso de 20 × 20 mm y 10 capas con un hueco de 1 × 1 mm | Sobre la plataforma | Sin aviso: menos de 4 mm² |
| VE9 | Vaso de revolución: radio 10, alto 10 mm, piso de 2 mm, capas de 0,1 mm | Sobre la plataforma, y sobre soportes con pad | Aviso de ventosa en los dos casos. Un cilindro macizo del mismo tamaño no da aviso |
