# Aletas de soporte (support fins)

**Qué hace:** agrega a la pieza paredes de soporte finas que se quitan con la mano, las «aletas», unidas a la pieza
con **dientes** horizontales de una capa. Sirve para imprimir piezas en la orientación más fuerte (acostadas o en
diagonal) sin los soportes del laminador. Las aletas son sólidos cerrados que se suman a la malla antes de laminar,
así que salen igual con cualquier impresora y material.

**Fase:** después de la primera versión, junto con los soportes (`supports_skirt.md`, `supports_more.md`).

**Diseño:** propio del motor, basado en Support Fins (reglas 17 a 23).

**Pedido del dueño (26-10-2026):** «también hay que integrar esto», con el enlace a Support Fins.

**Basado en:** Support Fins, de gittrahan (MIT), `https://github.com/gittrahan/support-fins`. La interfaz lo muestra
en el panel de la función y en Créditos y fuentes (`credits.md`).

**Fuente:**
- Support Fins, de gittrahan: `https://github.com/gittrahan/support-fins`, commit `2c5014c` (25-10-2026), sitio
  `printfins.com`.
- Licencia MIT (leída en su `LICENSE`): es compatible con el motor y no está entre los programas prohibidos de
  `CLEANROOM.md`.
- Esta ficha sale de su documentación: `README.md`, `docs/FIN-SPEC.md`, `docs/PLUGIN-CONFIG.md`, `docs/NOZZLE.md` y
  `docs/CALIBRATION.md`. No se copió su código.
- **Cómo se integra:** diseño propio del motor (decisión del dueño, 26-10-2026, sección «Diseño propio del motor»).
  No se porta ni se lee el código de Support Fins; de ese repositorio solo se usa su documentación. Nunca se usan
  sus `plugins/` (complementos de laminadores prohibidos) ni `occt-import-js` (LGPL-2.1).
- **Nombre:** «grip fin» es el término de Slant3D, que mostró la idea en un video. El motor dice «aletas de soporte».

**Sin medir todavía:** no se corrió su programa de línea de comandos sobre piezas de prueba (este entorno no dejó
ejecutar código de terceros). Las pruebas de abajo salen de las reglas escritas en su documentación. Antes de cerrar la
ficha, el equipo que conoce el programa corre su línea de comandos sobre las piezas de las pruebas y anota los
resultados (cantidad de aletas y de dientes, lo que queda sin soporte).

## Ajustes

Propuesta de nombres (grupo `fins`); el motor elige los definitivos (`SETTINGS_V1.md`). Los valores son los de Support
Fins.

| Clave | Unidad | Por defecto | Rango | Qué hace |
|---|---|---|---|---|
| `fins.enabled` | — | no | sí/no | Agrega aletas a la pieza. Con aletas, los soportes del laminador quedan apagados para esa pieza |
| `fins.mode` | — | `automatic` | `automatic`, `full` | `automatic`: una aleta con dientes bajo cada voladizo con pendiente suficiente para agarrarse, y una pared lisa bajo los más tendidos. `full`: lo de `automatic` y paredes bajo todo voladizo que quedó sin nada |
| `fins.threshold` | ° | 45 | 30 a 70 | Ángulo de la cara medido **desde la cama**: una cara más tendida que esto necesita soporte. Ojo: muchas pruebas miden desde la vertical (su 60° es este 30°) |
| `fins.gap` | mm | 0,2 | 0,1 a 0,4 | Separación entre la parte de arriba de la aleta y la pieza. PETG: 0,3 |
| `fins.tines` | — | sí | sí/no | Dientes que unen la aleta a la pieza. No: pared lisa que solo apoya |
| `fins.tine_density` | % | 0 | 0 a 100 | Cuántos dientes: 0 = los menos (menos marcas, con un mínimo por pared), 100 = peine denso para piezas altas o que se vuelcan |
| `fins.tine_direction` | — | horizontal | `horizontal`, `vertical` | Horizontal: dientes de una capa (FDM). Vertical: puntas que suben a la pieza (resina) |
| `fins.pin_depth` | mm | 0,1 | 0,05 a 1 | Solo resina: cuánto entra cada punta en la pieza |
| `fins.wall_lines` | — | 2 | 2, 4, 6, 8 | Grosor de las paredes en líneas (regla 9) |
| `fins.pad` | — | `automatic` | `off`, `automatic`, `light`, `sure` | Base en la cama (regla 7) |
| `fins.coverage` | % | 50 | 0 a 100 | Solo `automatic`: cuántas aletas en un voladizo ancho; menos = menos marcas y más riesgo de que se combe |
| `fins.plate_only` | — | no | sí/no | Toda aleta apoya en la cama, nunca sobre la pieza; los voladizos con pieza debajo quedan sin soporte y se avisa |
| `fins.separate_walls` | — | `automatic` | `automatic`, `on`, `off` | Solo `full`: que las paredes no se crucen ni compartan base. `automatic`: encendido en resina, apagado en FDM |
| `fins.long_walls` | — | sí | sí/no | Solo `full`: primero filas de paredes largas a lo largo de la pieza, después lo que falte |
| `fins.cutout` | — | `none` | `none`, `diamond`, `triangle`, `arch`, `lattice`, `honeycomb` | Agujeros en las paredes altas para ahorrar material (regla 10) |
| `fins.rounded_feet` | — | no | sí/no | Pies con las esquinas redondeadas |
| `fins.sway` | — | no | sí/no | Contrafuertes para piezas altas (regla 8) |
| `fins.sway_grip_from` | mm | 0 | 0 a 2000 | Altura desde la que los contrafuertes se agarran; 0 = toda la altura |
| `fins.sway_tine_spacing` | mm | 6 | 2 a 30 | Distancia vertical entre los dientes de los contrafuertes |
| `fins.sway_depth` | % | 15 | 5 a 50 | Cuánto sale el contrafuerte en la cama, en % de su altura |

La boquilla y la altura de capa no son ajustes de la función: salen de `machine.nozzle_diameter` y `layers.height`
(Support Fins las pide a mano porque no es un laminador).

## Reglas

1. **La aleta:** una pared fina parada al lado de la pieza, a `fins.gap` de ella, que se quita con la mano.
   - Sin dientes, la pieza puede caerse de costado lejos de la aleta: los dientes son lo que la sostiene.
   - **Dientes:** horizontales, de una capa (`layers.height`). Cada uno sale de la aleta y termina justo en la
     superficie de la pieza, medido en la línea del diente, con 0,01 mm de solape.
     - Siguen las pendientes porque se miden en cada borde lateral; la búsqueda avanza de a 0,1 mm.
     - Se imprimen como un solo cordón sin retracción y se quiebran limpio en el plano de la capa.
     - No se usan dientes verticales en FDM: son frágiles y pueden no tocar la pieza.
   - **Cantidad:** de 7 a 8 cerca de la base y menos más arriba (unos 5 suelen alcanzar), cada pocas capas.
   - **Forma:** arriba redondeada, nunca en punta; abajo, un pie ancho y elíptico.
   - **Dónde:** en un borde o una esquina, nunca en el medio de una cara que se ve. Una pieza larga lleva dos aletas,
     en lados opuestos.
2. **Medidas con boquilla de 0,4 mm** (pared de dos líneas):

| Parte | Medida |
|---|---|
| Separación de la pieza | 0,2 mm (`fins.gap`) |
| Alto del diente | una capa |
| Ancho del diente | 0,5 mm |
| Punta de contacto | 0,6 mm |
| Pared | 1,0 mm |
| Cuña | 1,2 mm |
| Pie | elipse ancha, disco de 1 mm |

3. **Con otra boquilla** todo ancho se escala con k = boquilla ÷ 0,4:
   - diente y punta de contacto: × k (con 0,2: 0,25 y 0,3 mm; con 0,8: 1,0 y 1,2 mm);
   - pared, cuña y contrafuerte: × el mayor entre 1 y k (nunca más finos que con 0,4);
   - las alturas no cambian con la boquilla: el diente sigue siendo de una capa;
   - una pared no pasa de 6 mm (la mitad de los 12 mm entre filas de paredes, para que dos filas no se fundan). Con
     boquillas grandes solo se ofrecen las líneas que entran: con 2,0 mm, 2 líneas; con 1,2 mm, hasta 4;
   - la altura de capa máxima con aletas es 0,6 mm: con más, se avisa y no se generan.
4. **Agujeros chicos:** el techo de un agujero de unos 6 mm o menos queda sin soporte. Una pared cabría adentro, pero no
   se podría sacar.
   - Se marca en ámbar, aparte de lo que no se pudo soportar, como «puede hacer puente».
   - Con `full`, bajo esos techos queda un volumen libre para que ninguna otra pared lo invada.
5. **Agujeros y huecos redondos:** si un contorno se ajusta a un círculo (al menos 12 vértices, error hasta el mayor
   entre 0,002 mm y el radio × 0,0001), las paredes se reparten en círculo dentro de él.
6. **Paredes que bajan** (`full`): se trazan siguiendo la mayor pendiente, en pasos de 1 mm, desde los bordes y el
   medio de la zona (como mucho a 12 mm entre sí). Una línea termina donde la pendiente baja de 0,05 o a 3 mm de otra
   línea.
7. **Base en la cama** (`fins.pad`): una pieza inclinada apoya en un borde y se despega sin ella. Todas las bases se
   extienden 4 mm más allá del contacto.
   - **light:** una capa, a 0,12 mm del contorno de la primera capa de la pieza; se pega por el aplastado del borde y
     se quita como un borde normal. La separación tiene que pasar 0,098 mm para que el laminador no la cierre.
   - **sure:** 0,5 mm de alto (0,3 en PETG), pegada a la pieza 0,05 mm (PETG: −0,1 mm). Agarra más y cuesta más
     sacarla.
   - **automatic:** `sure` si la pieza toca la cama en un punto o en un pie redondo chico (contorno de la primera
     capa de menos de 20 mm); si no, `light`. Una elección a mano nunca se cambia.
   - No se hace chaflán en la base de la pieza: la base cumple esa función sin tocar la geometría.
8. **Contrafuertes** (`fins.sway`, para piezas altas): nervios verticales de canto contra la cara, a 0,2 mm.
   - Salen `fins.sway_depth` de la altura en la cama y se afinan hasta 4 mm arriba.
   - Grosor: 1,2 mm más 0,004 mm por mm de altura, hasta 2,4 mm.
   - Dientes de una capa cada `fins.sway_tine_spacing`. Al menos 3 dientes y el 30 % de las filas tienen que agarrar.
   - Al menos 1 mm de los otros soportes.
   - Automático: hasta 4 caras, a 60° entre sí, más o menos un nervio cada 100 mm de ancho. No pone un nervio que
     quede más de 40 mm (o el 40 % de la altura) por debajo de su primer agarre.
   - La profundidad se limita a 60 mm.
9. **Líneas de pared** (`fins.wall_lines`): la pared y la cuña se multiplican por líneas ÷ 2; el pie crece lo mismo.
   La punta y los dientes siguen de una línea, para que se quiebren igual.
10. **Agujeros en las paredes** (`fins.cutout`), solo en paredes altas:
    - rombo, triángulo, arco, o malla de rombos o hexágonos de 6 mm con barras de 1,2 mm;
    - quedan macizos el borde de arriba que sostiene la pieza, el pie y los extremos;
    - entre agujeros, al menos 1,6 mm;
    - todo techo de agujero tiene al menos 45° (se busca 55°) para imprimirse sin puente;
    - las aletas con dientes al costado no se agujerean.
11. **Paredes unidas** (`full`): una pared de relleno puede unirse a otra de relleno, nunca a una de `automatic` ni a la
    pieza. Las une un zigzag a 55°, de 1,0 mm de ancho y 1,2 mm de profundidad, entre paredes a 2 a 12 mm, hasta dos
    por pared y hasta el 70 % de la altura de la más baja.
12. **Resina** (`fins.tine_direction` vertical): puntas de 0,4 mm que se afinan a 0,25 mm en la pieza y entran
    `fins.pin_depth`, a la cuarta parte donde la pieza es fina.
    - Separadas de 2 a 5 mm, al menos 3 por pared, con 0,3 mm de solape en la parte de arriba de la pared.
    - Sin separación al pie.
    - Support Fins dice que no lo probó en una impresora de resina.
13. **Informe:** al terminar, la cantidad de paredes y de dientes y lo que quedó sin soporte, en el panel y en la
    revisión previa (`print_review.md`):
    - voladizos demasiado tendidos para una aleta en esta orientación;
    - partes que empiezan en el aire;
    - techos de agujeros chicos (regla 4).
    No es un error: se puede laminar igual.
14. **Orientación:** la función no gira la pieza. Puede puntuar orientaciones por resistencia, según la dirección de la
    carga y si la pieza se estira o hace palanca (como la orientación de `orientation.md`), pero solo sugiere; la
    persona decide.
15. **Exportar:** las aletas se pueden exportar con la pieza como STL o 3MF; en el 3MF, como otra parte del objeto.
16. **Calibración:** Support Fins trae piezas de prueba (ángulo, separación, agarre, ancho de puente, base, agujeros).
    Se suman a `calibrations.md` cuando se programe esta función: cada una fija un ajuste de la tabla.

### Diseño propio del motor

Decisión del dueño (26-10-2026): «hagamos nuestro propio fin support». El motor no porta el código de Support Fins:
toma la idea y las medidas de su documentación (con su crédito, `credits.md`) y la hace dentro del laminador, lo que
un programa aparte no puede hacer.

17. **Dentro del laminado, no en la malla:**
    - las aletas se generan como regiones de soporte de cada capa, no como sólidos sumados al STL;
    - así la separación con la pieza es un número entero de capas (la regla del Gap de Support Fins se cumple sola:
      0,2 mm con capa de 0,2 es una capa vacía), y cada diente es exactamente una capa;
    - los dientes salen como un solo cordón, sin retracción, con la etiqueta de soporte en el G-code
      (`GCODE_OUTPUT.md`), y en la vista previa se ven de su propio color, «Aleta».
    - Exportar la malla con las aletas (regla 15) sigue existiendo, para usarla en otro laminador.
18. **Boquilla y capa de verdad:** `machine.nozzle_diameter`, `layers.height`, `walls.width` y la altura de cada capa
    (también con altura variable, `slicing.md`) salen del perfil; nada se pide a mano. Con altura variable, un diente
    toma la altura de su capa.
19. **Pintar aletas** (`docs/PAINTING.md`): un canal nuevo, «Aletas», con Forzar y Bloquear.
    - Forzar pone una aleta donde se pinta, aunque el voladizo no la pida.
    - Bloquear no deja aletas en caras que se ven.
    - Reemplaza a la colocación a mano de Support Fins.
20. **Con la estabilidad** (`stability.md`): si la revisión de estabilidad dice que la pieza se vuelca o flexiona, se
    ofrece encender `fins.sway` y subir `fins.tine_density`, con un botón **Aplicar** que muestra los valores.
21. **Con la orientación** (`orientation.md`): la orientación automática puede contar las aletas como soporte que no
    marca la superficie, y sugerir la orientación más fuerte (regla 14); la persona elige.
22. **Con los soportes del laminador:** una pieza usa aletas o soportes comunes. Se elige en la sección Soportes
    (`settings_panel.md`) con chips: Sin soporte, Soporte, Aletas.
23. **Mejoras para medir** (diseño, se confirman con piezas impresas antes de dejarlas por defecto):
    - dientes que siguen el contorno de la capa en lugar de una recta, para agarrar curvas;
    - largo del diente según la fuerza que pide la estabilidad, no solo por la cantidad de capas.

## Pruebas

Medidas en la malla que genera el motor, con boquilla de 0,4 mm, capa de 0,2 mm y los valores por defecto, salvo que
se diga otra cosa.

| # | Escenario | Resultado esperado |
|---|---|---|
| SF1 | Una repisa a 30° de la cama sobre un poste | Al menos una aleta bajo la repisa, a 0,2 mm de ella, con dientes de 0,2 mm de alto y 0,5 mm de ancho que tocan la pieza |
| SF2 | La misma repisa a 60° | Sin aletas: no hace falta soporte con 45° |
| SF3 | SF1 con boquilla de 0,8 mm | Dientes de 1,0 mm de ancho y paredes de 2,0 mm |
| SF4 | SF1 con boquilla de 0,2 mm | Dientes de 0,25 mm; paredes de 1,0 mm, no menos |
| SF5 | `fins.wall_lines` 4 | Paredes de 2,0 mm; dientes de 0,5 mm |
| SF6 | Boquilla de 2,0 mm | Solo se ofrecen 2 líneas de pared |
| SF7 | Capa de 0,8 mm | Aviso y sin aletas |
| SF8 | Techo de un agujero de 5 mm | Sin pared adentro; marcado en ámbar en el informe |
| SF9 | Pieza que toca la cama en un pie redondo de 15 mm, `fins.pad` `automatic` | Base `sure` de 0,5 mm, 4 mm más allá del contacto |
| SF10 | Pieza con un apoyo de 60 mm, `fins.pad` `automatic` | Base `light` de una capa, a 0,12 mm de la pieza |
| SF11 | `fins.tines` no | Paredes lisas, sin dientes |
| SF12 | `fins.plate_only` con un voladizo que tiene pieza debajo | Ese voladizo queda sin soporte y el informe lo cuenta |
| SF13 | Pieza de 150 mm de alto con `fins.sway` | Contrafuertes con dientes cada 6 mm, separados 1 mm de las aletas; profundidad de 22,5 mm en la cama |
| SF14 | `fins.cutout` `diamond` en una pared alta | Agujeros con techo de 45° o más; borde de arriba, pie y extremos macizos |
| SF15 | Laminar SF1 | Cada diente es un solo cordón en una capa, sin retracción; hay una capa vacía entre la parte de arriba de la pared y la pieza |
| SF16 | `fins.enabled` y los soportes del laminador encendidos | Los soportes del laminador quedan apagados para esa pieza, con un aviso |
| SF17 | SF1 laminado con capa de 0,2 mm | Una capa vacía entre la aleta y la pieza; cada diente en una sola capa, en la vista previa con el color «Aleta» |
| SF18 | Capa variable de 0,1 a 0,3 mm | Cada diente tiene la altura de su capa |
| SF19 | Pintar Bloquear aletas en la cara de adelante | Ninguna aleta sobre esa cara; el informe dice si un voladizo quedó sin soporte |
| SF20 | Pieza alta que la revisión de estabilidad marca como que se vuelca | Se ofrece encender los contrafuertes, con los valores antes y después |
