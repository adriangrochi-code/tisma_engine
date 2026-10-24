# Relleno ondulado entrelazado

**Qué hace:** imprime el relleno poco denso sobre ondas en Z, una superficie de «cartón de huevos» que se repite en
cada capa. Así las capas del relleno se encajan unas en otras, en vez de juntarse en planos lisos, que es por donde
suelen romperse las piezas.

**Fase:** después de F3. Experimental: la resistencia se comprueba con probetas propias.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `infill.waves` | — | no | Activa las ondas |
| `infill.wave_amplitude` | % de la altura de capa | 50 % | Cuánto sube y baja el relleno |
| `infill.wave_length` | mm | 10 | Distancia entre dos crestas, en X y en Y |

## Reglas

1. **Forma:**
   - la Z de cada punto del relleno es la Z de la capa más `a · sin(2πX/λ) · sin(2πY/λ)`;
   - X e Y son coordenadas de la cama, `a` = `infill.wave_amplitude` × altura de capa y `λ` = `infill.wave_length`;
   - la onda tiene la misma fase en todas las capas, por eso encajan.
2. **Lo que queda plano:** los perímetros, el relleno sólido y las superficies de arriba y de abajo. Las ondas se
   apagan cerca de ellos.
3. **Material:** sigue el espesor real entre el relleno de esta capa y el de la anterior.
4. **Requisitos:** coordenadas absolutas y sin arcos `G2`/`G3`. No se usa con capas no planares, modo espiral ni
   impresoras de cinta.

## Pruebas

Medidas en Tisma Slicer (24-10-2026) como caja negra. Ajustes base de `gcode_motion.md`.

| # | Pieza | Ajustes | Resultado |
|---|---|---|---|
| IL1 | Cubo de 20 × 20 × 10 mm, pieza centrada en X = 100, Y = 100 | `infill.waves` sí | Capas 11 (Z 2,2) y 12 (Z 2,4): el relleno va de Z − 0,094 a Z + 0,1. Las 328 posiciones del relleno de cada capa siguen `Z + 0,1·sin(2πX/10)·sin(2πY/10)`, con X e Y de la cama: error mediano de 0,001 mm; el 90 % por debajo de 0,036 mm (lo que más se aparta es cerca de los perímetros, donde la onda se apaga) |
