# Paquete libre de perfiles: descarga e instalación

**Qué hace:** la primera vez que se abre, el motor se conecta al repositorio público de perfiles y descarga las
impresoras, los filamentos, los perfiles de impresión y las resinas que la persona elige. Después avisa cuando hay
una versión nueva. Así se tienen miles de impresoras sin que el motor contenga perfiles de otros programas.

**Fase:** P3.

**Decisión del dueño (26-10-2026):**
- los perfiles de otros programas viven en un repositorio aparte, de uso libre (AGPL-3.0):
  `https://github.com/adriangrochi-code/Tisma-profiles-`;
- el motor, de uso no comercial, no los incluye ni los copia: los descarga y los lee como datos;
- el catálogo propio del motor (`docs/CATALOG.md`, solo datos oficiales) sigue igual y es lo que hay sin red.

**Formato:** `docs/FORMAT.md` de ese repositorio (versión 1) describe cada archivo y cada campo. Esta ficha solo
dice qué hace el motor con él.

**Sala limpia:**
- quien programa el motor lee `docs/FORMAT.md`, pero no los perfiles del paquete;
- las pruebas usan un paquete chico que arma el equipo del motor, con datos inventados que sigan el formato;
- el campo `original` de cada perfil (los valores con los nombres del programa de origen) no se lee ni se muestra.

## Ajustes

Preferencias de la interfaz, no del laminado:

| Clave | Por defecto | Qué hace |
|---|---|---|
| `packs.url` | la dirección de la última versión del repositorio | De dónde se baja el paquete; otra dirección permite un espejo o un paquete propio |
| `packs.check_updates` | sí | Buscar una versión nueva al abrir, como mucho una vez por día |
| `packs.vendors` | — | Los fabricantes instalados; se elige en el asistente |
| `packs.download_all` | no | Bajar todos los fabricantes (unos 32 MB) en lugar de los elegidos |

## Reglas

### Primer arranque

1. **Índice:** al abrir por primera vez, el motor baja `index.json` y `compatible.json` (unos 2 MB) sin preguntar, y
   muestra un aviso en la barra de estado mientras tanto.
2. **Asistente:** la lista de impresoras del asistente (`interface.md`, «Asistente») suma los modelos del índice,
   agrupados por fabricante, con su imagen (`thumbnail`). Elegir un modelo baja el archivo de su fabricante. Lo
   mismo al agregar una impresora después.
3. **Sin red:** el asistente muestra solo el catálogo propio y el aviso «Sin conexión: se muestran las impresoras
   incluidas; el resto se descarga cuando haya red». El motor funciona igual.

### Descarga

4. **Comprobación:** cada archivo se compara con el tamaño y el SHA-256 del índice. Si no coincide, se descarta, se
   avisa y queda la versión anterior.
5. **Descompresión segura:** solo rutas relativas que empiezan por `vendors/<id>/` o `community/`. Una ruta absoluta,
   con `..`, un enlace o un archivo de más de 50 MB es un error y se descarta el archivo entero.
6. **Formato:** un índice con `format` distinto de `tisma-profile-pack`, o con una `version` mayor que la que conoce
   el motor, no se carga. Aviso: «El paquete de perfiles es de una versión más nueva: actualizá Tisma».
7. **Escritura:** se baja a una carpeta temporal y se cambia de una vez, así una descarga cortada no rompe lo
   instalado.
8. **Dónde se guarda:** en la carpeta de datos de la interfaz, aparte de los perfiles de la persona (`profiles/`,
   `docs/PROFILES.md`). Un paquete nunca escribe en la carpeta de la persona.

### Uso de los perfiles

9. **Perfiles de fábrica:** los del paquete se muestran como los de fábrica: no se pueden sobrescribir, y
   **Guardar** pide un nombre nuevo, como en `docs/PROFILES.md`.
10. **Qué se usa:** `profile` de cada archivo, que ya tiene el formato y las claves del motor.
    - Una clave que el motor no conoce se ignora con un aviso en el registro: el paquete puede ser más nuevo.
    - Un valor fuera de rango se rechaza como en un perfil propio, y el perfil se marca como «con errores».
11. **Modelo y boquilla:** cada modelo del índice aparece una sola vez; sus perfiles de impresora son las boquillas o
    variantes (`profiles.md`, reglas 1 y 2).
12. **Compatibles:**
    - un filamento o un perfil de impresión se ofrecen solo con las impresoras de su conjunto de `compatible.json`;
    - con `compatible` nulo, con todas;
    - los genéricos (`generic`) van después de los del fabricante de la impresora.
13. **Por defecto:** al elegir una impresora, se eligen el primer filamento de `defaults.filament` y el perfil de
    `defaults.print` (en resina, `defaults.resin` y `defaults.resin_print`).
14. **Cama e imágenes:** `bed_model` (STL) y `bed_texture` (SVG o PNG) del modelo se dibujan en la vista 3D, en las
    coordenadas de la cama, sin moverlas.
15. **Licencia y origen:** el panel de un perfil muestra su `license` y su `origin` en una línea chica, con el enlace
    al repositorio.

### G-code de inicio y fin

16. **Variables del laminado:** un perfil de impresora del paquete puede usar estas variables en su G-code de inicio y
    de fin. El motor las llena al laminar:

| Variable | Valor |
|---|---|
| `first_layer_min_x`, `first_layer_min_y`, `first_layer_max_x`, `first_layer_max_y` | Caja de los puntos de las extrusiones de la primera capa (falda y borde incluidos), mm |
| `first_layer_size_x`, `first_layer_size_y` | Su ancho y su largo, mm |
| `layer_count` | Cantidad de capas |
| `initial_tool` | Extrusor del primer tramo (0 con uno solo) |
| `travel_speed` | `speed.travel`, mm/s |
| `layer_height`, `first_layer_height` | `layers.height` y `layers.first_height`, mm |
| `year`, `month`, `day`, `hour`, `minute`, `second` | Fecha y hora local del laminado, con dos cifras salvo el año |
| `input_filename_base` | Nombre del primer modelo, sin carpeta ni extensión |

17. **Temperaturas:** las variables de temperatura (`temperature`, `first_layer_temperature`, `bed_temperature`,
    `first_layer_bed_temperature`, `chamber_temperature`, `chamber_minimal_temperature`, `idle_temperature`) vienen del
    perfil de filamento, como hoy (`docs/CONFIG.md`, `machine.variables`).
18. **Sin G-code de inicio:** cerca de 4 de cada 10 impresoras del paquete no traen G-code de inicio o de fin, porque
    el de origen usaba un lenguaje de macros. El motor usa entonces el suyo para el dialecto
    (`gcode_firmware.md`) y el panel de la impresora muestra «G-code de inicio genérico: revisalo antes de imprimir».

### Actualizaciones

19. **Versión nueva:** al abrir, como mucho una vez por día, el motor compara el `pack_version` instalado con el
    publicado. Si cambió, avisa en la barra de estado («Hay perfiles nuevos: actualizar»).
20. **Al actualizar:**
    - se bajan los fabricantes instalados cuyo `archive.sha256` cambió;
    - los perfiles de la persona no se tocan;
    - si un perfil elegido desapareció, queda elegido hasta que la persona cambie, con el aviso «Este perfil ya no
      está en el paquete».

### Datos de la comunidad

21. **Qué son:** `community.tar.xz` trae datos por impresora, filamento o resina, cada uno con su fuente y su tipo
    (`official`, `measured`, `community`). El formato está en `docs/COMMUNITY.md` del repositorio.
22. **Dónde se muestran:** en el panel de la impresora o del material, en una sección «Datos publicados», con un
    distintivo por tipo:
    - «Fabricante» para `official`;
    - «Medido» para `measured`;
    - «Sugerido» para `community`.

    Cada dato lleva el valor, la unidad, las condiciones y el enlace a la fuente (en un video, con el minuto).
23. **Llenar ajustes:**
    - un dato `official` cuya clave es un ajuste del motor puede llenar ese ajuste si el perfil no lo trae, con un
      botón **Usar este valor**;
    - los `measured` y `community` nunca cambian un ajuste solos: solo con ese botón y una confirmación.

## Pruebas

Escenarios de aceptación. El paquete de prueba lo arma el equipo del motor: dos fabricantes, tres impresoras (una de
resina), cuatro filamentos, dos perfiles de impresión y dos conjuntos de compatibles.

| # | Escenario | Resultado esperado |
|---|---|---|
| PK1 | Primer arranque con red | Baja el índice y los compatibles; el asistente muestra los modelos del paquete agrupados por fabricante |
| PK2 | Primer arranque sin red | Solo el catálogo propio y el aviso de sin conexión; el motor lamina igual |
| PK3 | Archivo de un fabricante con un byte cambiado | Se descarta (SHA-256 distinto) y se avisa; lo instalado sigue |
| PK4 | Archivo con una ruta `../x.json` adentro | Se descarta entero; nada se escribe fuera de la carpeta del paquete |
| PK5 | Índice con `"version": 2` | No se carga; aviso de versión más nueva |
| PK6 | Elegir una impresora del paquete | Se eligen su primer filamento y su perfil de impresión por defecto; laminar un cubo da código 0 |
| PK7 | Un filamento cuyo conjunto no tiene la impresora elegida | No aparece en la lista de filamentos de esa impresora |
| PK8 | Perfil con una clave desconocida | Se carga sin esa clave; un aviso en el registro |
| PK9 | G-code de inicio `G1 X{first_layer_min_x} Y{first_layer_min_y}` con un cubo de 20 mm centrado en (100, 100), sin falda | X e Y entre 90 y 90,5: el centro del cordón externo de la primera capa |
| PK10 | Impresora sin G-code de inicio en el paquete | Usa el genérico del dialecto y muestra el aviso |
| PK11 | Guardar sobre un perfil del paquete | Pide un nombre nuevo; el del paquete no cambia |
| PK12 | Versión nueva publicada, con un fabricante cambiado | Aviso de perfiles nuevos; al actualizar se baja solo ese fabricante; los perfiles propios no cambian |
| PK13 | Dato de comunidad `measured` de `machine.max_flow` | Se muestra con el distintivo «Medido» y su fuente; el ajuste no cambia hasta tocar **Usar este valor** y confirmar |
| PK14 | Descarga cortada a la mitad | Lo instalado sigue entero; el próximo intento baja de nuevo |
