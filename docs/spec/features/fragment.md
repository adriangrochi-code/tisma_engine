# Dividir para que entre en la impresora

**Qué hace:** corta un objeto más grande que el volumen de impresión en piezas que entran, con encastres en cada corte
para que las piezas queden alineadas al pegarlas.

**Fase:** después de la primera versión (preparación de la pieza). Lo diseñó y escribió el equipo del dueño en Tisma
Slicer.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `fragment.joint` | — | `dowel` | `dowel` (perno), `plug` (espiga) o `none` (solo pegamento) |
| `fragment.joint_diameter` | mm | 8 | Diámetro de los encastres |
| `fragment.min_joint_diameter` | mm | 3 | Diámetro más chico que se prueba donde la pared es fina |
| `fragment.joint_depth` | mm | 6 | Cuánto entra el encastre en cada pieza |
| `fragment.clearance` | mm | 0,15 | Holgura de los agujeros, en el radio y en la profundidad |
| `fragment.joint_wall` | mm | 2 | Pared mínima alrededor de un encastre |
| `fragment.max_joints` | — | 4 | Encastres como máximo por isla de cada sección |

En la interfaz se recuerdan entre usos.

## Tipos de encastre

- **Perno (`dowel`):**
  - agujeros en las dos piezas y un perno aparte que se imprime de pie;
  - es lo que mejor se imprime: los agujeros horizontales no necesitan soporte y el perno sale redondo.
- **Espiga (`plug`):** la espiga sale de una pieza y entra en un agujero de la otra. No hay piezas sueltas, pero en un
  corte vertical la espiga queda horizontal, en voladizo.
- **Ninguno (`none`):** solo pegamento.

## Reglas

1. **Tamaño útil:**
   - el rectángulo de la cama y la altura máxima. En una cama redonda se usa el cuadrado inscrito; en una de forma
     libre, el rectángulo que la contiene;
   - en X e Y se deja un margen por lado para lo que el acomodo pone alrededor de cada pieza: la falda (vueltas ×
     ancho + distancia), el borde y la mitad de la separación entre objetos;
   - con espigas se descuenta además lo que la espiga sobresale.
2. **Cantidad de piezas por eje:** la menor que entra, `⌈tamaño / tamaño útil⌉`. Si no hay forma de ubicar los cortes
   respetando el largo mínimo de una pieza (lugar para los encastres de sus dos caras), una más.
3. **Dónde cortar:**
   - se mide la sección del objeto cada 0,5 mm a lo largo del eje (o cada tamaño / 1500, si es menos);
   - se eligen las posiciones que dejan todas las piezas dentro del tamaño útil y suman la menor sección (menos para
     pegar y una costura más corta). Una búsqueda por programación dinámica lo hace;
   - se penaliza mucho una sección donde no entra ningún encastre, y un poco la distancia a piezas iguales: en una
     pieza de sección constante, los cortes quedan parejos;
   - los planos se corren 0,0137 mm de las posiciones redondas, para no pasar justo por vértices (por ejemplo, las
     esquinas que dejó otro corte).
4. **Encastres:** en cada corte, cada parte de la sección entre los otros cortes recibe encastres.
   - El centro de cada uno queda a `diámetro / 2 + holgura + pared` del borde de la sección. Queda a esa misma
     distancia de los otros planos de corte, también a una profundidad de encastre a cada lado del corte: así ningún
     encastre cruza otro plano.
   - Van de 1 a `fragment.max_joints` por isla, según su área, separados al menos 4 diámetros, el primero cerca del
     centro de la isla.
   - Si la pared no da para el diámetro elegido, se prueba con 0,6 veces el diámetro y con `fragment.min_joint_diameter`.
   - Las partes que ni así tienen lugar quedan solo para pegamento, y se informa cuántas son.
5. **Corte:**
   - un plano por vez: primero los de X, después los de Y y los de Z, cada uno sobre las piezas que atraviesa;
   - los agujeros se restan de las dos piezas con su holgura; los pernos son objetos aparte;
   - las piezas quedan cerradas (sin bordes abiertos) y conservan el volumen del objeto;
   - las piezas conservan la orientación del objeto.
6. **Resultado:**
   - las piezas y los pernos reemplazan al objeto (se puede deshacer) y se reparten en las camas que hagan falta;
   - si hay más piezas que camas, se avisa antes;
   - solo se divide un objeto con una sola copia.

## Pruebas

Son resultados de las pruebas propias de Tisma Slicer: la división es parte de la interfaz y no tiene línea de
comandos. Volumen de impresión de 250 × 210 × 220 mm y margen de 5 mm por lado (tamaño útil en X: 240 mm). Ajustes por
defecto salvo que se diga otra cosa.

| # | Objeto | Encastre | Resultado |
|---|---|---|---|
| FR1 | Cubo de 100 mm | — | No se divide |
| FR2 | Barra de 600 × 60 × 40 mm | Perno | 3 piezas en X; cortes en 200 y 400 mm (±10). Cada corte: sección de 2400 mm², al menos un encastre, ninguna parte solo para pegamento. Cada pieza ≤ 240 mm en X y con sus agujeros. Pernos = encastres. Volumen sin los pernos: 600 × 60 × 40 mm³ (±0,01 %) |
| FR3 | Barra de 470 × 50 × 50 mm | Espiga | 3 piezas: 470 entra en dos de 240, pero no con una espiga de 6 mm que sobresale. Cada pieza ≤ 240 mm con su espiga. Espigas = encastres. Volumen 470 × 50 × 50 mm³ |
| FR4 | Dos bloques de 150 × 80 × 80 mm unidos por un cuello de 60 × 20 × 20 mm (largo total 360 mm) | Espiga | Un corte, entre 150 y 210 mm (en el cuello), de 400 mm², con 1 encastre |
| FR5 | Placa de 400 × 300 × 20 mm | Perno | 2 × 2 piezas. Al menos 2 encastres por corte, de 8 mm, cada uno a 6,15 mm o más del otro plano de corte. 4 piezas más los pernos. Volumen conservado. Piezas cerradas |
| FR6 | Cilindro de radio 30 y 500 mm de alto | — | 3 piezas en Z |
| FR7 | Caja hueca de 500 × 100 × 60 mm, paredes de 8 mm | Perno | No entra un perno de 8 mm con 2 mm de pared alrededor: encastres de menos de 8 mm, y ninguna parte solo para pegamento |
| FR8 | Benchy al 500 % en una impresora con cama redonda de 150 mm | Perno | 24 piezas y 29 encastres (con el diámetro de 8 mm solo, 2). Más piezas que las 9 camas: aviso |
