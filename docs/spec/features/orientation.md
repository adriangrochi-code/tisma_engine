# Orientación automática

**Qué hace:** busca una rotación de la pieza que la imprima mejor y la aplica:
- **FDM, «menos material de soporte»:** menos soporte y buena adherencia a la cama;
- **resina, «menor fuerza de despegue»:** sin ventosas y con la capa más grande lo más chica posible.

**Fase:** después de la primera versión (preparación de la pieza). Los dos métodos los diseñó y escribió el equipo
del dueño en Tisma Slicer.

**Fuentes:**
- F. Liravi, S. Das, C. Zhou, [«Separation force analysis based on cohesive delamination model for bottom-up
  stereolithography using finite element analysis»](https://utw10945.utweb.utexas.edu/sites/default/files/2014-113-Liravi.pdf),
  Solid Freeform Fabrication Symposium 2014: la fuerza que separa cada capa del film crece con el área pegada.
- `resin.md`: detector de ventosas.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `orient.method` | — | según la tecnología | `least_support` (FDM) o `least_peel` (resina) |
| `orient.precision` | 0 a 1 | 0,75 | En resina se laminan `max(4, 16 · precisión)` candidatas: 12 por defecto |

En la interfaz, el método y la precisión elegidos se recuerdan por tecnología.

## Reglas

### Comunes

1. **Candidatas:** siempre se incluye la orientación actual, así el resultado nunca es peor que lo que había.
2. **Al terminar:** se gira la pieza alrededor de Z para que su caja mínima en XY quede apaisada, y se apoya en la
   cama.

### FDM: menos material de soporte

3. **Candidatas:** apoyar en la cama cada cara de la envolvente convexa de la pieza, más la orientación actual.
4. **Costo de cada candidata:**
   - **volumen de soporte:** cada cara que mira hacia abajo, más empinada que el umbral de soporte (`support.angle_limit`;
     45° desde la horizontal si es automático), suma su área proyectada por la altura hasta la pieza que tiene
     debajo o hasta la cama;
   - **menos la adherencia:** el área apoyada en la cama, por 1 mm;
   - **descarte:** las orientaciones que tocan la cama con menos de 10 mm² se evitan, porque se despegan o vuelcan;
   - **empate:** gana la más baja.
5. **Cómo medir la altura bajo cada cara:** se tira un rayo hacia abajo desde la cara. Conviene girar la dirección
   vertical y no la malla, y armar una sola vez una estructura de búsqueda de la malla.
6. **Límite:** es una estimación geométrica. No simula el generador de soportes: puentes, soportes en árbol o
   voladizos que salen con perímetros solapados.

### Resina: menor fuerza de despegue

7. **Candidatas:**
   - con la pieza sobre la plataforma (sin elevación, o con pad alrededor): apoyar cada cara de la envolvente convexa;
   - con soportes: direcciones repartidas de forma pareja sobre la esfera (por ejemplo, una red de Fibonacci);
   - más la orientación actual.
8. **Preselección:**
   - se ordenan las candidatas por una estimación de soporte y se quedan las `max(4, 16 · orient.precision)` mejores;
   - Tisma Slicer usa para esto el puntaje de voladizos que hereda de PrusaSlicer. El motor puede usar el costo de la
     regla 4.
9. **Elección:** cada candidata se lamina entera, con capas de `max(resin.layer_height, alto / 400)` (como mucho 400
   capas: se comparan orientaciones, no se imprime), y se elige:
   1. la que tiene menos ventosas;
   2. entre esas, la que tiene la capa más grande más chica.

## Pruebas

Son resultados de las pruebas propias de Tisma Slicer: la orientación es parte de la interfaz y no tiene línea de
comandos. «Z de la pieza» es hacia dónde apunta, después de girar, el eje Z original de la pieza.

| # | Pieza | Método | Resultado |
|---|---|---|---|
| OR1 | Hongo: tallo de 8 × 8 × 20 mm debajo de un sombrero de 30 × 30 × 3 mm, con toda la cara de abajo del sombrero en voladizo | FDM | Queda dado vuelta, con el sombrero en la cama (Z de la pieza hacia abajo, componente vertical < −0,99) |
| OR2 | Barra de 5 × 5 × 60 mm, parada | FDM | Queda acostada (componente vertical de Z de la pieza, menor que 0,01 en valor absoluto). Parada tampoco necesita soporte, pero toca la cama con solo 25 mm² |
| OR3 | Vaso de 20 × 20 × 20 mm, paredes y piso de 2 mm, abierto arriba | Resina, con soportes | No queda abierto hacia el film (componente vertical de Z de la pieza < 0,5) |
| OR4 | El vaso de OR3 | Resina, sobre la plataforma | Queda de costado (componente vertical de Z de la pieza, menor que 0,01 en valor absoluto): sin ventosa |
