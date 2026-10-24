# Boquillas distintas y mezcla de colores

**Qué hace:**
- **boquillas distintas:** en una impresora con varios extrusores, cada uno puede tener su propio diámetro. Las paredes
  salen con la boquilla fina y el relleno con la gruesa: buen acabado y menos tiempo;
- **mezcla de colores:** colores nuevos alternando capas finas de filamentos distintos, que a la distancia se ven
  mezclados.

**Fase:** después de la primera versión.
- Las dos funciones las trae PrusaSlicer.
- Lo propio de Tisma Slicer, del equipo del dueño, es cómo se ofrecen: la boquilla de cada extrusor en el panel, el
  interruptor «paredes finas, relleno grueso» y la mezcla como opción con aviso de costo en las impresoras de una sola
  boquilla.

**Fuentes:**
- Prusa Research, [«Experimental: Printing with Different Nozzle Diameters»](https://help.prusa3d.com/article/experimental-printing-with-different-nozzle-diameters_821176):
  qué va con cada boquilla, combinación del relleno, anchos automáticos, soportes y torre de purga.
- Prusa Research, [«Print with dozens of colors: Our new open-source ColorMix»](https://blog.prusa3d.com/our-new-open-source-colormix-model-in-prusaslicer-and-easyprint_136079/):
  - mezcla por capas alternadas, como la trama de la impresión en papel;
  - proporciones posibles por bloques de capas;
  - un modelo de predicción del color publicado con licencia MIT (`prusa-fdm-mixer`), que el motor puede usar
    anotándolo en `THIRD_PARTY.md`.

## Ajustes

| Clave | Unidad | Por defecto | Qué hace |
|---|---|---|---|
| `machine.nozzle_diameter` | mm, uno por extrusor | 0,4 | Diámetro de la boquilla de cada extrusor |
| `multi.fine_walls_coarse_infill` | — | no | Paredes con la boquilla más fina y relleno con la más gruesa |
| `multi.color_mixing` | — | no en impresoras de una sola boquilla; sí en las de una boquilla por filamento | Ofrecer colores mezclados |

## Reglas

### Boquillas distintas

1. **En el panel:**
   - una impresora con varios extrusores muestra la boquilla de cada uno debajo de la boquilla general;
   - la general cambia todas, y dice «boquillas distintas» si no coinciden.
2. **Altura máxima de capa** de cada extrusor: el 75 % de su diámetro.
3. **Paredes finas, relleno grueso** (con diámetros distintos):
   - con la boquilla más fina van los perímetros, el relleno sólido, los soportes, la interfaz y la torre de purga;
   - con la más gruesa, el relleno poco denso;
   - el relleno se combina cada varias capas, de modo que su espesor no pase el límite de la boquilla gruesa;
   - los anchos de las líneas se calculan de la boquilla de cada extrusor.
4. **Al apagarlo,** esos ajustes vuelven a los valores del perfil de impresión.
5. **Soportes:** con un solo ancho de soporte, que salga de la boquilla fina. Si salen de la gruesa con ese ancho,
   les falta material.

### Mezcla de colores

6. **Cómo:** una zona con un color mezclado se imprime alternando capas de los filamentos de la mezcla.
7. **Proporciones:**
   - son por bloques enteros de capas: 1:1, 1:3, 3:1, o 1:1:1 con tres colores;
   - un bloque largo (por ejemplo 3:7, diez capas) se ve a rayas y pierde resolución vertical;
   - conviene usar capas finas (Prusa da el ejemplo de 0,1 mm).
8. **Color esperado:** se calcula con el modelo de Prusa (base de Yule-Nielsen, con correcciones medidas). Está
   calibrado solo con su PLA en su impresora: con otros materiales es una aproximación.
9. **Impresoras de una sola boquilla con cambio de filamento** (un sistema que alimenta varios filamentos por la misma
   boquilla):
   - cada capa con mezcla es un cambio de filamento con su purga;
   - por eso la mezcla empieza apagada. Al prenderla, se avisa antes que la impresión usa bastante más filamento y
     tarda más;
   - un proyecto que ya trae mezclas conserva el botón para cambiarlas.
10. **Aviso después de laminar** (impresoras de una sola boquilla): cuánto filamento va a la purga (torre y
    lavado), en gramos si se conoce la densidad.
11. **Impresoras con una boquilla por filamento:** no purgan en cada cambio. La mezcla se ofrece siempre, sin aviso.

## Pruebas

Escenarios de aceptación, como funcionan en Tisma Slicer.

| # | Escenario | Resultado esperado |
|---|---|---|
| MM1 | Impresora de dos extrusores con boquillas de 0,25 y 0,6, «paredes finas, relleno grueso» | Perímetros, sólido y soportes en el extrusor 1; relleno poco denso en el 2; relleno combinado cada varias capas |
| MM2 | Apagar «paredes finas, relleno grueso» | Los extrusores de cada tipo de línea vuelven a los del perfil de impresión |
| MM3 | Boquillas de 0,4 y 0,6 en el panel | La boquilla general dice «boquillas distintas»; cada extrusor muestra la suya |
| MM4 | Impresora de una sola boquilla con cuatro filamentos | La mezcla aparece apagada; al prenderla, aviso de más filamento y más tiempo |
| MM5 | Laminar una pieza con una mezcla 1:1 en esa impresora | Aviso con los gramos de purga (torre y lavado) |
| MM6 | Impresora con una boquilla por filamento | La mezcla está disponible sin aviso |
