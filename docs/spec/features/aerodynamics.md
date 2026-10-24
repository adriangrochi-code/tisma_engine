# Análisis aerodinámico

**Qué hace:** estima el aire alrededor de la pieza tal como se imprime:
- arrastre y fuerza transversal;
- mapa de presión;
- efecto de la rugosidad de las capas.

Sirve para comparar diseños y orientaciones. No reemplaza un túnel de viento ni un CFD profesional.

**Fase:** F7. Lo diseñó y escribió el equipo del dueño en Tisma Slicer.

**Fuentes:**
- Lattice Boltzmann D3Q19 con colisión BGK: Qian, d'Humières y Lallemand, «Lattice BGK models for Navier–Stokes
  equation», *Europhysics Letters* 17 (1992).
- Modelo de submalla de Smagorinsky: Smagorinsky, *Monthly Weather Review* 91 (1963).
- Capa límite: Blasius (laminar) y Schlichting, *Boundary-Layer Theory* (turbulento, liso y rugoso).
- Coeficientes de arrastre de referencia: Hoerner, *Fluid-Dynamic Drag* (1965).

## Entradas

| Entrada | Unidad | Qué es |
|---|---|---|
| Malla de la pieza | mm | Cerrada |
| Dirección del aire | — | Un eje, o la dirección de la vista |
| Velocidad | m/s | — |
| Calidad | — | Celdas en el tamaño frontal: rápida 16, normal 24, alta 32; «solo rozamiento» no simula |
| Altura de capa | mm | La del perfil; da la rugosidad |

## Resultados

- Arrastre (N) y coeficiente Cd, separado en presión y rozamiento.
- Fuerza transversal, área frontal, Reynolds real y Reynolds simulado.
- Rugosidad media y su parte en el rozamiento.
- Mapa del coeficiente de presión por cara, de −1,5 (succión) a 1 (estancamiento), o de la rugosidad.

## Método

El arrastre se separa en dos partes, porque la grilla no puede resolver la capa límite real a la velocidad real.

1. **Presión (forma):**
   - **simulación:** lattice Boltzmann D3Q19 con BGK y submalla de Smagorinsky, sobre la pieza en vóxeles, girada para
     que el aire vaya en +X;
   - **dominio:** 1,5 tamaños frontales delante, 3 detrás y 1,5 a los lados; laterales periódicos, entrada a
     velocidad fija, salida con gradiente nulo;
   - **presión:** el promedio del último tercio de la simulación se integra sobre las caras de la pieza;
   - **Reynolds:** el simulado queda limitado por la resolución, a 40 × las celdas del tamaño frontal. El arrastre de
     presión de las formas romas cambia poco por encima de unos 1000.
2. **Rozamiento:**
   - correlaciones de placa plana integradas sobre las caras, subdivididas cada unos 2 mm, según la distancia al
     borde de ataque;
   - laminar de Blasius, turbulento liso o totalmente rugoso de Schlichting;
   - la capa límite pasa a turbulenta por encima de `Re_x = 5·10⁵` o si la rugosidad la dispara (`U·ks/ν > 300`);
   - las caras que miran aguas abajo (estela separada) no suman rozamiento.
3. **Rugosidad de la impresión**, por cara, con Z la dirección de impresión, `h` la altura de capa y `n_z` la
   componente Z de la normal:
   - caras inclinadas: `Ra = máx(0,05·h, h·|n_z| / 4)` (escalones en diente de sierra);
   - paredes verticales y caras planas: `Ra = 0,05·h` (borde redondeado del cordón);
   - rugosidad equivalente de arena: `ks ≈ 4·Ra`.

## Pruebas

| # | Caso | Resultado de Tisma | Referencia |
|---|---|---|---|
| AE1 | Placa cuadrada de frente | Cd 1,16 | 1,17 a 1,2 (Hoerner) |
| AE2 | Cubo de frente | Cd 1,13 | 1,05 (Hoerner); +8 % por el bloqueo del dominio |
| AE3 | El mismo cubo con el aire en otra dirección | Igual que AE2 | Simetría |
| AE4 | Prisma 3:1 de punta roma, y con nariz piramidal | Cd 1,06 y 0,34 | La nariz baja el arrastre |
| AE5 | Rozamiento laminar de un prisma 6:1 | A menos de 5 % de Blasius | `1,328 / √Re_L` |
| AE6 | Prisma impreso inclinado 45° | Ra de 15 a 35 µm; rozamiento × 2,4 | Las caras escalonadas son más rugosas |

**Tiempo de referencia:** un Benchy en calidad normal tarda unos 5 minutos con 4 núcleos.

## Límites

- Presión simulada a Reynolds bajo: no capta la crisis de arrastre (esferas y cilindros por encima de unos 2·10⁵) ni
  las separaciones finas de los cuerpos fuselados.
- Bloqueo del dominio: los cuerpos romos dan entre 5 y 10 % más de Cd.
- Los detalles más finos que una celda (tamaño frontal / 24) no se ven.
