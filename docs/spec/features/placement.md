# Ubicación de los modelos en la cama

**Qué hace:** decide dónde quedan los objetos en la cama antes de cortarlos: centrados, ordenados si son varios, con
la base en Z = 0, y qué pasa si no entran.

**Fase:** F1. Responde la pregunta del equipo del motor del 24-10-2026: la esfera del banco está centrada en el
origen y, sin moverla, queda fuera de la cama.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `placement.arrange` | — | sí | Ubicar los objetos: el conjunto centrado y, si son varios, ordenados sin tocarse. «no» deja la posición en X e Y que traen |
| `placement.center` | mm (x, y) | el centro de la cama | Dónde queda el centro del conjunto |
| `placement.spacing` | mm | 6 | Separación entre las cajas de los objetos al ordenarlos |
| `placement.drop_to_bed` | — | sí | Llevar la base de cada objeto a Z = 0 |
| `machine.bed` | mm | — | Polígono de la cama (`SETTINGS_V1.md`) |
| `machine.max_height` | mm | — | Altura máxima de impresión |

## Reglas

1. **Con la interfaz de Tisma** (diseño de la unión entre el adaptador y el motor):
   - la persona ubica cada objeto en la placa: posición, giro y escala;
   - quien llama al motor manda los objetos ya en coordenadas de la cama y pide `placement.arrange` «no»;
   - el motor no los mueve en X e Y, y aplica las reglas 3 y 4.
2. **Ubicar** (`placement.arrange`), como hace la línea de comandos de Tisma Slicer:
   - el centro de la caja del conjunto va a `placement.center`, sin importar dónde estaba en el archivo (pruebas PL1,
     PL2 y PL6);
   - varias copias quedan en una grilla compacta, con `placement.spacing` entre cajas, y todo el conjunto centrado
     (PL7 y PL8). Qué copia va en qué lugar lo elige el motor;
   - no se gira ni se escala nada para que entre.
3. **Base** (`placement.drop_to_bed`):
   - lo que flota baja y lo que está en parte debajo de la cama sube, hasta que la base queda en Z = 0 (PL1, PL2, PL4);
   - con «no», lo que queda debajo de Z = 0 no se imprime (PL5). Un objeto que no toca la cama es un error: «objeto
     sin extrusiones en la primera capa», sin archivo y con código distinto de 0 (PL9).
4. **Fuera del volumen:**
   - después de ubicar, un objeto que sale de `machine.bed` en X o Y, aunque sea en parte, o pasa `machine.max_height`
     es un error: «los objetos quedan fuera del volumen de impresión», sin archivo (PL3, PL10, PL11);
   - el motor sale con un código distinto de 0, como en los demás errores de entrada (`slicing.md`). Tisma Slicer sale
     con 0 en este caso; eso no se copia;
   - sin `machine.bed` no se centra ni se comprueba nada en X e Y: los objetos quedan donde vienen y solo se aplica la
     regla 3 (diseño).
5. **Varios archivos en una misma placa:** en Tisma Slicer, la línea de comandos se cierra con una falla de memoria
   al juntarlos (PL13). No se copia: el motor los ordena como en la regla 2.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) desde la línea de comandos, como caja negra, con una cama de 0 a 200 mm en X e
Y, 200 mm de alto máximo, 2 perímetros y sin falda. Las posiciones son las de las caras de cada objeto; el cordón
externo queda a 0,22 mm hacia adentro.

| # | Pieza (en el archivo) | Ajustes | Resultado |
|---|---|---|---|
| PL1 | Cubo de 40 mm centrado en el origen (Z de −20 a 20) | — | X e Y de 80 a 120, Z de 0 a 40 |
| PL2 | Cubo de 20 mm en X e Y de 300 a 320 y Z de 50 a 70 | — | X e Y de 90 a 110, Z de 0 a 20 |
| PL3 | El cubo de PL1 | `placement.arrange` «no» | Error, fuera del volumen: una parte queda en X e Y negativos |
| PL4 | Cubo de 20 mm en X e Y de 90 a 110, Z de −5 a 10 | `placement.arrange` «no» | Mismo X e Y; Z de 0 a 15 (sube 5 mm) |
| PL5 | La pieza de PL4 | `placement.arrange` «no», `placement.drop_to_bed` «no» | Z de 0 a 10: los 5 mm de abajo no se imprimen |
| PL6 | El cubo de PL1 | `placement.center` (50, 60) | X de 30 a 70, Y de 40 a 80 |
| PL7 | Cubo de 20 mm, dos copias | — | Centros en (87, 100) y (113, 100): 6 mm entre caras |
| PL8 | Cubo de 20 mm, tres copias | — | Grilla de 2 × 2 con un lugar vacío: centros en (113, 113), (87, 113) y (113, 87); el conjunto de 77 a 123 en X e Y |
| PL9 | Cubo de 20 mm en X e Y de 90 a 110, Z de 50 a 70 | `placement.arrange` «no», `placement.drop_to_bed` «no» | Error: objeto sin extrusiones en la primera capa; sin archivo; código 1 |
| PL10 | Barra de 300 × 20 × 10 mm | — | Error, fuera del volumen: no se gira ni se escala para que entre |
| PL11 | Torre de 20 × 20 × 250 mm | — | Error, fuera del volumen (más alta que 200 mm) |
| PL12 | Barra de 190 × 20 × 10 mm | — | Entra: X de 5 a 195 |
| PL13 | El cubo de PL1 y un cubo de 20 mm, en una misma placa | — | Tisma Slicer se cierra con una falla de memoria (código 139). No se copia |
