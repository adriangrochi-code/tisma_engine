# Catálogo de impresoras y filamentos

Pedido del dueño (2026-10-24): «perfiles de impresoras y configuraciones de filamentos para tener la máxima capacidad
de impresoras del mercado». Es la fase P3 de `PLAN.md`.

El catálogo trae los **datos de cada fabricante**: el volumen de impresión, las boquillas, el firmware, el extrusor y
los límites de cada impresora, y las temperaturas, la cama y el ventilador de cada filamento. De cada entrada se hace
un perfil (`docs/PROFILES.md`) con un clic.

## De dónde salen los datos (regla de sala limpia)

- **Solo de las fuentes oficiales del fabricante:**
  - sus páginas de producto y de especificaciones, su tienda oficial, su wiki o centro de ayuda y sus manuales;
  - las hojas técnicas (TDS) de los filamentos;
  - para los diseños abiertos (Voron, RatRig), su documentación oficial.
- **Nunca de los perfiles de otro programa:** ni de PrusaSlicer, OrcaSlicer, Bambu Studio, Cura, Creality Print,
  Elegoo Slicer, AnycubicSlicer, FlashPrint, sus derivados ni Tisma Slicer. Esos perfiles son trabajo de otros, dentro
  de repositorios GPL, AGPL o LGPL. Tampoco de sitios que los copian.
- **Solo hechos:** números y nombres de modelos. Ningún texto se copia, salvo una cita corta como evidencia en la
  recolección, que no entra al catálogo.
- **Ningún G-code ajeno:** el G-code de inicio y fin es propio del motor (ver «Perfiles»).
- **Cada entrada lleva sus fuentes.** `allowed_source` rechaza los hosts de código (GitHub, GitLab, Bitbucket) y los
  sitios de laminadores, y el catálogo no se carga si alguna fuente no está permitida.
- **Si un dato no está en una página oficial, queda vacío.** Nunca se completa de memoria ni de otro sitio.

## Archivos

`resources/catalog/printers.json` y `resources/catalog/filaments.json`. Van dentro del motor (`cmake/embed.cmake`), así
que el programa no depende de archivos externos.

**Una impresora:**

| Clave | Qué es |
|---|---|
| `vendor`, `model` | Fabricante y nombre oficial del modelo |
| `build_volume` | `[x, y, z]` en mm. En una cama redonda (delta), `x` es el diámetro |
| `bed_shape` | `rectangular` (por defecto) o `circular` |
| `nozzle_diameters` | Las boquillas en mm: la de fábrica primero, después las que vende el fabricante. Si el fabricante no la dice, 0,4 mm, la más común |
| `filament_diameter` | 1,75 o 2,85 mm |
| `firmware` | `marlin`, `klipper`, `reprap`, `bambu` u `other` (propio del fabricante); vacío si el fabricante no lo dice: G-code genérico |
| `extruder` | `direct` o `bowden` |
| `heated_bed`, `max_bed_temp`, `max_nozzle_temp` | Cama caliente y temperaturas máximas (°C) |
| `max_speed`, `max_acceleration` | Velocidad (mm/s) y aceleración (mm/s²) máximas publicadas |
| `auto_leveling`, `enclosed` | Nivelación automática; impresora cerrada |
| `multi_material` | La opción multimaterial del fabricante, en texto (por ejemplo, «AMS lite, 4 filamentos») |
| `sources` | Las páginas oficiales de donde salen los números (obligatorio) |

**Un filamento:**

| Clave | Qué es |
|---|---|
| `vendor`, `name`, `material` | Fabricante, nombre oficial y material base (`PLA`, `PETG`, `ABS`, `ASA`, `TPU`, `PA-CF`…) |
| `diameter`, `density` | mm; g/cm³ |
| `feedstock` | `filament` (si falta) o `pellet`; en los pellets no cuenta el diámetro |
| `nozzle_temp`, `bed_temp` | `[mín, máx]` en °C |
| `nozzle_temp_recommended`, `bed_temp_recommended` | Si el fabricante da un valor recomendado |
| `fan` | `[mín, máx]` en %, solo si la hoja da números |
| `print_speed` | `[mín, máx]` en mm/s |
| `enclosure` | Si el fabricante pide impresora cerrada |
| `sources` | Las páginas u hojas técnicas oficiales (obligatorio) |

**Capacidades en detalle** (todas opcionales; solo lo que dice una página oficial):

| Clave | Qué es |
|---|---|
| `kinematics` | `cartesian` (cama que se mueve en Y), `corexy`, `corexz`, `delta`, `idex`, `belt`, `toolchanger` |
| `max_flow` | Flujo volumétrico máximo del hot end, mm³/s |
| `chamber_heating` | Temperatura máxima de la cámara calefaccionada activamente, °C (una impresora solo cerrada no la tiene) |
| `hardened_nozzle` | La boquilla de fábrica acepta filamentos abrasivos (`true`) o el fabricante dice que no (`false`) |
| `hotend`, `probe` | El hot end y el sensor de nivelación, en pocas palabras |
| `bed_surfaces` | Las superficies de cama que trae o vende el fabricante |
| `connectivity` | De `wifi`, `ethernet`, `lan`, `usb`, `sd`, `usb drive`, `cloud`, `app` |
| `features` | Lo que el fabricante dice que tiene: «input shaping», «filament runout sensor», «camera»… |
| `material_slots` | Filamentos que alimenta una unidad multimaterial oficial |
| `tools` | Cabezales independientes, si hay más de uno (IDEX: 2) |

Y en los filamentos:

| Clave | Qué es |
|---|---|
| `max_flow` | Flujo volumétrico máximo, mm³/s |
| `chamber_temp` | `[mín, máx]` de la cámara, °C |
| `drying_temp`, `drying_hours` | Secado: °C y horas |
| `abrasive`, `soluble` | Abrasivo (con fibra de carbono o vidrio); soluble (PVA, BVOH…) |
| `hardness` | Dureza Shore, en texto («95A») |
| `glass_transition`, `heat_deflection`, `shrinkage` | Transición vítrea y HDT en °C; contracción en % |
| `bed_surfaces` | Las camas o adhesivos que recomienda |

**Comprobaciones al leer (`parse_catalog`):** se rechazan las claves faltantes o fuera de rango (con el nombre de la
entrada), una entrada repetida y las fuentes no permitidas. Las entradas quedan ordenadas por fabricante y modelo.

## Perfiles

**De una impresora** (`printer_profile`, para una de sus boquillas):

| Ajuste | De dónde sale |
|---|---|
| `machine.bed` | Un rectángulo desde el origen, o un círculo de 64 lados alrededor del origen si la cama es redonda |
| `machine.max_height` | La altura del volumen |
| `machine.nozzle_diameter` | La boquilla elegida |
| `machine.flavor` | `klipper` → Klipper; `reprap` → RepRap; `marlin`, `bambu`, `other` y sin dato → Marlin (comandos que también entienden los demás) |
| `retraction.length` | Valores de partida propios: extrusor directo 0,8 mm; Bowden 4 mm. Las torres de calibración encuentran los de cada impresora |
| Las velocidades (`speed`, `retraction.speed`, `cooling.min_speed`, `machine.corner_speed`) | El catálogo no las sabe: las de respaldo de una Ender-3 de fábrica (`docs/CONFIG.md`, «Velocidades de respaldo»; decisión del dueño, 2026-10-26), ninguna por encima de la velocidad máxima publicada del modelo. La interfaz lo dice al agregar la impresora |
| `machine.max_feed_x`, `_y`, `acceleration` | Los máximos publicados, si los hay |
| `machine.max_flow`, `max_nozzle_temp`, `max_bed_temp`, `max_chamber_temp`, `enclosed`, `hardened_nozzle` | Sus capacidades, si se conocen (ver «Capacidades») |
| G-code de inicio y fin | Propios del motor, en comandos que entiende cualquier firmware: calentar, `G28`, `M420 S1` (la malla guardada) en Marlin con nivelación automática, esperar las temperaturas, una línea de cebado junto al borde, y al final apagar, subir y presentar la pieza |

**Firmware propio:** las impresoras `bambu` y `other` (por ejemplo, UltiMaker) tienen su propia secuencia de inicio o
encabezado de archivo. Su G-code de inicio lo avisa en un comentario y la interfaz muestra la advertencia: hay que
revisarlo antes de imprimir.

**De un filamento** (`filament_profile`):

| Ajuste | De dónde sale |
|---|---|
| `machine.filament_diameter` | El diámetro |
| Variables `temperature` y `bed_temperature` | La temperatura recomendada; si no hay, el medio del rango redondeado a 5 °C |
| `extrusion.max_flow`, `chamber_temperature` | Su flujo máximo; el medio del rango de cámara |
| `extrusion.abrasive` | Lo que dice la hoja; si no dice, abrasivo si el material lleva fibra (`-CF`, `-GF`) |
| `extrusion.needs_enclosure` | Si pide impresora cerrada |
| `cooling.fan_min`, `fan_max` | Los de la hoja; si no da números, los del material: PLA 35–100 %, PETG 20–50 %, TPU 50–100 %, ABS, ASA, PA y PC 0–20 % con las 3 primeras capas sin ventilador (los mismos de los perfiles de fábrica) |

Los nombres: «Fabricante Modelo 0.4 mm» y «Fabricante Filamento».

## Capacidades

El dueño pidió (2026-10-24) que el motor sepa «a lo mayor cantidad de detalles posible» lo que puede cada impresora y
lo que pide cada filamento, y que lo use. Con los dos perfiles elegidos:

- **El flujo:** ninguna extrusión pasa del flujo máximo del hot end ni del filamento (el menor de los dos). La
  velocidad de cada tramo baja lo necesario; el tiempo estimado lo tiene en cuenta.
- **La cámara:** si el filamento pide cámara y la impresora la calienta, el G-code la pone al principio (sin pasar del
  máximo de la impresora) y la apaga al final.
- **Los avisos:** si el filamento pide más que la impresora (boquilla o cama más calientes que su máximo, una cámara
  que no calienta o no alcanza, una impresora cerrada, una boquilla endurecida), la laminación lo avisa. No frena la
  exportación: un dato vacío del catálogo nunca avisa.
- **La ficha:** la interfaz muestra todos los detalles conocidos de la impresora y el filamento.

Un dato que el fabricante no da queda en 0 o falso: no limita nada.

## En la interfaz

En las filas de perfil de Impresora y Filamento (`docs/UI.md`), la lista y el menú ⋮ tienen **Agregar desde el
catálogo…**:
- un buscador por marca y modelo (todas las palabras tienen que estar);
- la lista agrupada por fabricante;
- la ficha del elegido: volumen, firmware, extrusor, máximos, opción multimaterial, las capacidades en detalle (o, en
  un filamento, el secado, la dureza, si es abrasivo o soluble), la advertencia de firmware propio y las fuentes como
  enlaces;
- en las impresoras, la boquilla;
- **Agregar a mis perfiles** lo guarda como perfil del usuario y lo elige.

Los materiales que se ofrecen, en el catálogo y en la lista de perfiles de Filamento, son los que toma la impresora
(`pellets.md`, regla 1; `profiles.md`, regla 5):
- con `machine.feedstock` `pellet`, solo los de pellets; con filamento, nunca los de pellets;
- con filamento, los del diámetro de las impresoras instaladas (la elegida y las del usuario), que sale del modelo del
  catálogo del que se hizo cada una (`printer_of_profile`); si alguna no viene del catálogo, el diámetro no se sabe y
  se ofrecen todos;
- el material elegido sigue en la lista aunque no coincida (el aviso `feedstock` lo dice);
- si no queda ninguno, el catálogo lo explica: «El catálogo no tiene materiales para la alimentación o el diámetro de
  filamento de esta impresora». Hoy pasa con las impresoras de 2,85 mm y con las de pellets: el catálogo no tiene
  todavía filamentos de 2,85 mm ni pellets.

`--catalog printer|filament` y `--catalog-search TEXTO` abren el catálogo en las capturas.

## Desde la línea de comandos

`tisma-engine profile --printer "<marca> <modelo>" [--nozzle <mm>] [--filament "<marca> <nombre>"] -o <ajustes.json>`
escribe los ajustes que arma el motor con el catálogo (el perfil de la impresora para esa boquilla y, si se nombra, el
del filamento; lo demás, los valores por defecto). Es el mismo documento que lee `slice --config`. Lo pidió la app de
Android, para no repetir la lógica en Kotlin.

- El nombre completo gana sobre los que lo contienen; si no, vale el único que tenga todas las palabras.
- Sin `--nozzle`, la boquilla de fábrica. Sin `-o`, a la salida estándar.
- Una impresora, filamento o boquilla que no está, o un nombre que alcanza a varios: código de salida 2 y el motivo
  en stderr.

### El paquete libre de perfiles

El paquete (ficha `profile_pack.md`) vive en `<carpeta>/pack`. El motor no baja nada: quien lo llama (la interfaz, la
app del teléfono) baja los archivos que pide el plan a una carpeta y el motor los instala desde ahí.

1. `tisma-engine pack plan --data <carpeta>` da la lista de descargas en JSON (`downloads`: `url`, `name`, `size`,
   `sha256`). Sin `--index`, solo el índice. Con `--index <index.json bajado>`, el resto: `compatible.json`,
   `community.tar.xz`, `knowledge.tar.xz` y cada fabricante. Solo entran los fabricantes cuyo archivo cambió. Sin
   `--vendors a,b`, los instalados, o todos si no hay ninguno. `--url` cambia la dirección de la última versión.
2. `tisma-engine pack install --data <carpeta> --from <carpeta con lo bajado>`:
   - compara cada archivo con el tamaño y el SHA-256 del índice;
   - descomprime cada `.tar.xz` en memoria y revisa cada entrada antes de escribir nada;
   - una ruta insegura, un enlace o un archivo de más de 50 MB descartan el archivo entero;
   - lo descartado y lo que no se bajó quedan como estaban;
   - arma el paquete nuevo al lado y lo cambia con un solo renombre;
   - escribe líneas `progress:` en stderr y el informe en JSON (`installed`, `kept`, `discarded`, `warnings`);
   - un índice de versión más nueva no cambia nada (código 2).
3. `tisma-engine pack list --data <carpeta>` da los fabricantes, sus modelos y variantes, cada una con su nombre de
   impresora `<archivo>#<variante>`.
4. `tisma-engine pack community --data <carpeta> --printer|--filament|--resin "<fabricante>/<nombre>"` da los datos
   publicados por la comunidad (reglas 22 a 24). Cada uno trae `key`, `value`, `unit`, `kind` (`official`, `measured`
   o `community`), `conditions` y `source`; los de clave o tipo desconocidos se dejan afuera. Solo un dato `official`
   puede llenar solo un ajuste vacío. Los demás necesitan que la persona confirme (PK13).
5. `tisma-engine profile --pack <carpeta> --printer "<archivo>#<variante>" [--filament <ruta>] [--print <ruta>]`
   escribe los ajustes de esa impresora del paquete, igual que `profile --printer`.
   - Sin `--filament` ni `--print`, usa los de `defaults` de la variante.
   - Un filamento que no es para esa impresora (`compatible.json`) da código 2.

### El paquete en la interfaz

- El catálogo de impresoras suma, bajo «Paquete libre de perfiles», los modelos del paquete instalado que coinciden con
  la búsqueda, agrupados por fabricante. Cada modelo es una fila, con sus boquillas en una lista (PK15).
- La ficha muestra la tecnología, la boquilla, y la licencia y el origen en una línea chica (regla 15).
- **Agregar a mis perfiles**, con el fabricante ya bajado: arma la impresora de esa boquilla y su filamento y su perfil de
  impresión por defecto, y los elige (reglas 9 y 13). Si el fabricante no está, el botón dice **Bajar y agregar**:
  baja ese fabricante en segundo plano y después la agrega.
- Los perfiles del paquete son de fábrica: no se guardan encima, como los propios del motor (regla 9).
- Se guarda una copia de cada impresora del paquete elegida (`pack_profiles.json`, junto a la carpeta `pack`). Si una
  actualización la quita del paquete, sigue elegida desde esa copia con el aviso «Este perfil ya no está en el
  paquete» (regla 20).
- Con una impresora del paquete elegida, el catálogo de filamentos suma los filamentos del paquete que sirven para ella,
  según `compatible.json` (regla 12, PK7). Los agregados quedan en `packs.filaments`.
- Las impresoras elegidas se guardan en las preferencias (`packs.printers`) y se vuelven a armar al abrir.
  `packs.url` cambia de dónde se baja.
- Al abrir, como mucho una vez por día, se busca si hay una versión publicada más nueva (regla 19). Si la hay, aparece
  el aviso «Hay perfiles nuevos». `packs.check_updates` en falso lo apaga; `packs.checked` guarda cuándo se buscó.
- Sin paquete instalado, el catálogo ofrece **Bajar la lista de impresoras del paquete**. Con uno, **Actualizar el
  paquete** baja solo lo que cambió.
- Con una impresora del paquete elegida, su página tiene la sección **Datos publicados** (reglas 23 y 24). Cada dato de
  la comunidad aparece con su distintivo («Fabricante», «Medido» o «Sugerido»), su valor y unidad, sus condiciones y
  el enlace a la fuente (en un video, con el minuto). **Usar este valor** cambia el ajuste: los del fabricante en el
  momento y los demás después de una confirmación (PK13). El filamento o la resina del paquete elegidos tienen la misma
  sección, plegada bajo su perfil en la pestaña Material.
- Una impresora de resina del paquete (modelo con `"technology": "sla"`) queda con `machine.technology = msla`, elige
  su primera resina (`defaults.resin`) y su perfil de impresión de resina (`defaults.resin_print`), y el catálogo le
  ofrece solo resinas. Una resina en una impresora de filamento, o al revés, se rechaza.
- Una impresora del paquete con `assets.bed_model` (STL) y `assets.bed_texture` (SVG) muestra en la vista 3D su cama:
  el modelo en el color de la placa y el dibujo estirado sobre la caja de la cama (regla 14). Un dibujo PNG todavía no
  se muestra.
- `tisma --pack CARPETA` usa otro paquete, también en las capturas. `--pick-pack-printer "<archivo>#<variante>"`
  agrega y elige una impresora del paquete; es para las pruebas.

## Cómo se arman los datos

1. **Recolección:** se buscan las páginas oficiales de cada fabricante y se anotan los números con la URL y una cita
   corta. Se hace por tandas de fabricantes; cada tanda queda en `PROGRESS.md`.
2. **Revisión:** cada fuente tiene que ser del dominio del fabricante; se descartan las entradas con datos dudosos o
   sin fuente.
3. **Carga:** `tools/catalog_merge.py` las suma al JSON del catálogo: solo con fuentes oficiales, sin claves ni valores
   fuera del formato. Con `--update`, una entrada que ya está solo gana las claves que no tenía y las fuentes nuevas;
   un valor distinto del que ya tiene se informa y no se toma. Las pruebas validan el catálogo entero.

Para agregar una impresora o un filamento: una entrada con sus fuentes oficiales, en orden alfabético.

## En el código

- **`src/catalog/catalog.*`:** el formato, `parse_catalog`, `builtin_catalog`, `allowed_source`, la búsqueda,
  `printer_profile` y `filament_profile`.
- **`src/catalog/catalog_files.hpp`:** los archivos incrustados.
- **`apps/tisma/app.cpp`:** el diálogo del catálogo.
- **Pruebas:** `tests/test_catalog.cpp`:
  - la lectura, el orden y la búsqueda;
  - los errores con su entrada y su clave, también las fuentes no permitidas;
  - los perfiles de impresora (rectangular y redonda, cada firmware, directo y Bowden), que laminan el cubo a G-code
    verificado;
  - los perfiles de filamento;
  - las capacidades en detalle, hasta los perfiles y sus avisos;
  - el catálogo incluido, entrada por entrada.
- **`tests/test_gcode.cpp`**, «capabilities»: el tope de flujo, la cámara y los avisos en el G-code.
