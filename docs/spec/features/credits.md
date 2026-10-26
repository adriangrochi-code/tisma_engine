# «Basado en»: créditos de las fuentes

**Qué hace:** cada función que sale de otra fuente (un proyecto, un artículo, una guía, una persona) lo dice en la
interfaz con «Basado en…», y una página de créditos las junta todas.

**Fase:** P1 (interfaz), y vale para toda función que se agregue después.

**Pedido del dueño (26-10-2026):** «hay que integrar el "basado en" cuando saquemos tecnologías de otras fuentes».

## Reglas

1. **En la ficha:** toda ficha cuya función sale de una fuente externa lleva, arriba, la línea **Basado en:** con:
   - el nombre de la fuente;
   - su autor;
   - la licencia, si es un proyecto;
   - el enlace.

   Es aparte de **Fuentes:**, que puede listar además documentación de formatos, firmwares y normas.
2. **Qué cuenta como «basado en»:**
   - una idea, un método o unas medidas tomados de un proyecto, un artículo, una tesis, una guía o un video;
   - datos tomados de otro lado (perfiles, tablas de materiales).

   No cuentan:
   - las especificaciones de formatos (STL, 3MF, G-code) y la documentación de los firmwares: son normas, van en
     **Fuentes:**;
   - las comparaciones de caja negra con otros laminadores: se miden, no se toman.
3. **En la interfaz:** el panel, el diálogo o la página de la función muestra al pie, en gris y chico:
   - «Basado en Support Fins, de gittrahan»;
   - el nombre es un enlace que abre la fuente en el navegador;
   - si la función tiene un grupo de ajustes, el mismo texto aparece al pasar el puntero por el título del grupo.
4. **Página de créditos:** en Más › Help, «Créditos y fuentes». Tiene dos tablas:
   - **Funciones:** función, fuente, autor, licencia y enlace, de todas las líneas «Basado en»;
   - **Componentes de terceros:** las bibliotecas de `THIRD_PARTY.md`, con su licencia y su aviso. Las licencias MIT,
     BSD y Apache piden repartir ese aviso con el programa: esta página lo cumple.
5. **Un solo lugar para los datos:** los créditos van en un archivo de datos del programa, que la interfaz lee para las
   reglas 3 y 4. Una prueba compara ese archivo con las líneas «Basado en» de las fichas y con `THIRD_PARTY.md`: si
   falta una, falla.
6. **Código de terceros:** usar código de otro proyecto lo decide el dueño (`CLEANROOM.md`). Si se usa, además del
   crédito va a `THIRD_PARTY.md` con su licencia. El crédito no reemplaza al aviso de la licencia.
7. **Idioma:** «Basado en» y «Créditos y fuentes» se traducen; los nombres de las fuentes y de los autores no.

## Créditos que ya tienen las fichas

| Función | Ficha | Basado en | Autor | Licencia o tipo |
|---|---|---|---|---|
| Aletas de soporte | `support_fins.md` | Support Fins, `https://github.com/gittrahan/support-fins` | gittrahan (idea de Slant3D) | MIT |
| Asistente y tutoriales | `assistant.md` | Guías de impresión 3D, `https://www.rafabarajas.com` | Rafa Barajas | Guías; texto propio |
| Guías de calibración | `assistant.md` (reglas 20 a 24) | Wiki de calibración de OrcaSlicer, `https://github.com/SoftFever/OrcaSlicer/wiki/calibration_guide` | Equipo y comunidad de OrcaSlicer | Documentación; texto propio |
| Guías de calibración | `assistant.md` (reglas 20 a 24) | Ellis' Print Tuning Guide, `https://ellis3dp.com/Print-Tuning-Guide/` | Andrew Ellis | Guía; texto propio |
| Guías de calibración | `assistant.md` (reglas 20 a 24) | Documentación de Klipper: avance de presión y resonancias, `https://www.klipper3d.org/` | Proyecto Klipper | Documentación; texto propio |
| Perfiles de resina del paquete | `profile_pack.md` | UVtools, `https://github.com/sn4k3/UVtools` | sn4k3 | AGPL-3.0 (datos, en el repositorio de perfiles) |
| Paredes de ancho variable | `perimeters.md` | Kuipers et al. 2020, Computer-Aided Design 128:102907 | Kuipers, Doubrovski, Wu y Wang | Artículo |
| Capas curvas en 3 ejes | `nonplanar_modes.md` | CurviSlicer, SIGGRAPH 2019 | Etienne et al. | Artículo |
| Capas según las tensiones | `nonplanar_modes.md` | S³-Slicer, SIGGRAPH Asia 2022 | Zhang et al. | Artículo |
| Soportes en árbol de resina | `resin_supports.md` | Clever Support, Computer Graphics Forum 33(5), 2014 | Vanek, Garcia Galicia y Benes | Artículo |
| Impresoras Bambu Lab en red local | `printer_connections.md` | OpenBambuAPI, `https://github.com/Doridian/OpenBambuAPI` | comunidad | Documentación |

El resto de las fichas con **Fuentes:** (por ejemplo `aerodynamics.md`, `structural_analysis.md`, `orientation.md` e
`infill_patterns.md`) se revisa una por una y suma su línea «Basado en» cuando corresponda (regla 2).

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| CR1 | Abrir el panel de las aletas de soporte | Al pie, «Basado en Support Fins, de gittrahan»; el enlace abre la página del proyecto |
| CR2 | Más › Help › Créditos y fuentes | Las dos tablas; cada biblioteca de `THIRD_PARTY.md` con su licencia y su aviso |
| CR3 | Agregar una ficha con «Basado en» sin sumar su crédito al archivo de datos | La prueba de la regla 5 falla y nombra la ficha |
| CR4 | Interfaz en inglés | «Based on Support Fins, by gittrahan» |
