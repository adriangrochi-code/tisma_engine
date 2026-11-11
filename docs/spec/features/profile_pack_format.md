# Formato del paquete libre de perfiles

**Qué es:** el formato de los archivos del paquete de perfiles (`profile_pack.md`), para que el equipo del motor no
tenga que leer el repositorio de perfiles. Lo pasó el equipo de fichas el 04-11-2026 desde `docs/FORMAT.md` (versión
2), `docs/COMMUNITY.md` y `docs/KNOWLEDGE.md` de `adriangrochi-code/Tisma-profiles-`. Solo describe la estructura y
los campos; no trae ningún perfil.

**Sala limpia:** las pruebas del motor usan un paquete chico armado con datos inventados que sigan este formato.

## Descarga

1. Cada versión es un release del repositorio, con:
   - `index.json`: el índice (también `index.json.xz`, el mismo contenido comprimido, 0,12 MB contra 2,06 MB: se baja
     ese si está) (fabricantes, modelos, perfiles, archivos comprimidos con su tamaño y SHA-256);
   - `compatible.json`: qué impresoras sirve cada filamento y cada perfil de impresión;
   - `<fabricante>.tar.xz`: uno por fabricante, con `vendors/<fabricante>/…`;
   - `community.tar.xz`: los datos de la comunidad (`community/…`);
   - `knowledge.tar.xz`: la base del asistente (`knowledge/…`);
   - `stats.json`: cuentas de la conversión (el motor no lo necesita).
2. La última versión está en `https://github.com/adriangrochi-code/Tisma-profiles-/releases/latest/download/<archivo>`.
3. **Instalar:**
   - bajar `index.json` y guardarlo como está;
   - bajar `compatible.json` y los fabricantes elegidos (o todos);
   - comprobar tamaño y SHA-256 de cada archivo contra el índice; si no coinciden, se descarta;
   - descomprimir cada `.tar.xz` en la carpeta del paquete. Adentro solo hay rutas relativas que empiezan con
     `vendors/<id>/`, `community/` o `knowledge/`; una ruta absoluta, con `..` o que empieza de otra forma no se
     descomprime.
4. **Versión nueva:** cambió `pack_version` del índice.
5. **Tamaño de referencia** (versión del 26-10-2026): índice de unos 2,1 MB; cada fabricante de unos KB a 2,8 MB; todo
   junto unos 32 MB en 218 archivos.

## `index.json`

| Clave | Qué es |
|---|---|
| `format`, `version` | `"tisma-profile-pack"` y `2`. Una versión mayor que la conocida no se carga |
| `pack_version` | Versión del paquete, `AAAA.MM.DD-commit` |
| `generated` | Fecha y hora de la conversión (UTC) |
| `source`, `other_sources` | De dónde salieron los perfiles (repositorio, commit, carpeta) |
| `license` | `AGPL-3.0-or-later` |
| `vendors` | Los fabricantes (abajo) |
| `printers` | Todas las impresoras como `<archivo del modelo>#<variante>`, en orden: el número de una impresora es su lugar en esta lista |
| `compatible`, `community` | Nombre, tamaño y SHA-256 de esos archivos |
| `counts` | Cuentas de la conversión |

**Un fabricante** (`vendors[i]`):

| Clave | Qué es |
|---|---|
| `id` | Identificador; también el nombre del archivo comprimido y de su carpeta |
| `name` | Nombre para mostrar |
| `license`, `origin` | Licencia y de qué programa salieron sus perfiles |
| `models` | Sus modelos de impresora (abajo) |
| `printers` | Sus impresoras, con el mismo formato que en el índice |
| `filaments`, `prints`, `resins`, `resin_prints` | Rutas de cada perfil dentro de la carpeta del paquete |
| `archive` | Nombre, `size` y `sha256` de su archivo comprimido |
| `counts` | Cuántos de cada cosa |

**Un modelo** (`models[i]`): `id`, `name`, `technology` (`fff` o `sla`), `family`, `default_materials` (nombres),
`assets` (rutas de `bed_model` en STL, `bed_texture` en SVG o PNG y `thumbnail` en PNG, si las hay), `file` (su
archivo de modelo), `variants` (`id` y `nozzle` de cada una) y `default_variant`.

## Un modelo de impresora (`vendors/<id>/models/<nombre>.json`)

```json
{
 "format": "tisma-pack-model",
 "version": 2,
 "technology": "fff",
 "vendor": "…",
 "model": { "id": "…", "name": "…", "family": "…" },
 "default_variant": "0.4",
 "common": { "settings": { } },
 "variants": [ { "id": "0.4", "nozzle": 0.4, "name": "…", "settings": { }, "defaults": { } } ],
 "also_in": [ ],
 "license": "AGPL-3.0-or-later",
 "origin": "…"
}
```

1. **El perfil de una variante** es `common.settings` con los `settings` de la variante encima, grupo por grupo y
   clave por clave; se arma como perfil de tipo `printer` con el `name` de la variante. Las claves son las de
   `docs/CONFIG.md`.
2. **`default_variant`:** la boquilla de 0,4 mm si hay una que no es de alto flujo; si no, la primera.
3. **`defaults`** de una variante: rutas de los filamentos (`filament`, el primero es el elegido) y del perfil de
   impresión (`print`); en resina, `resin` y `resin_print`.
4. **`also_in`:** el mismo modelo venía de otra fuente; se guarda una vez y las impresoras de la otra fuente apuntan a
   la variante de la misma boquilla.
5. **`not_converted`** (en una variante): el G-code de inicio o de fin que no se pudo pasar, con el motivo; se puede
   mostrar como aviso.

## Un perfil de filamento, impresión o resina (`vendors/<id>/<grupo>/<nombre>.json`)

```json
{
 "format": "tisma-pack-entry",
 "version": 2,
 "technology": "fff",
 "vendor": "…",
 "profile": { "format": "tisma-profile", "version": 1, "kind": "filament", "name": "…", "settings": { } },
 "compatible": 0,
 "generic": false,
 "license": "AGPL-3.0-or-later",
 "origin": "…"
}
```

1. **`profile`:** un perfil del motor tal cual (`docs/PROFILES.md`); `kind` es `filament` o `print`, y en resina
   `resin` o `resin_print`.
2. **`compatible`:** el número de un conjunto de `compatible.json`, o `null` si no se sabe (sirve para todas).
3. **`generic`:** el perfil es genérico (de un paquete de plantillas), no de un fabricante.
4. **Filamentos:** además, `material` con `type` (PLA, PETG…), `vendor`, `density` (g/cm³), `cost_per_kg`, `colour` y
   `soluble`.
   Un filamento puede llevar `compatible_prints`: las rutas de los perfiles de impresión del paquete con los que sirve.
   Con ese campo, se ofrece solo si la impresión elegida está en la lista; sin él, sirve con cualquier impresión.
   Si se cambia la impresión y el filamento elegido ya no sirve con ella, se pasa al filamento por defecto de la
   variante (o al primero que sirva, del mismo material) y se avisa «El filamento no servía con esta impresión: se
   cambió a <nombre>».
5. **Impresión:** además, `nozzle`, la boquilla para la que está hecho, si todas sus impresoras usan la misma.
6. **Ajustes que cruzan de perfil** (11-11-2026): en los programas de origen, el flujo de los puentes es de la impresión
   y el volumen de purga por cambio, de la impresora. El paquete los escribe donde venían, y el motor los toma de ahí
   aunque su grupo sea de otro tipo de perfil:
   - un perfil de impresión puede traer `extrusion.bridge_flow_ratio`; manda sobre el del filamento (PF7);
   - un perfil de impresora puede traer `wipe_tower.purge_volumes`; es el valor por defecto, y el de la impresión manda
     si lo trae (PF8);
   - `extrusion.type` de un filamento es un ajuste conocido (`docs/CONFIG.md`): se lee, no se avisa «unknown
     setting» (PF9).
   - `extrusion.travel` de un filamento (los viajes propios del material, como `extrusion.retraction`) se lee como
     los otros ajustes del filamento (PF10);
   - en resina, `resin.bottom_layers` viene en el perfil de impresión de resina, como en el programa de origen; manda
     sobre el de la resina (PF11).
   Fuera de estas claves sigue la regla de `docs/PROFILES.md`: un ajuste de otro tipo se deja afuera con aviso.

## `compatible.json`

```json
{ "format": "tisma-pack-compatible", "version": 1, "sets": [[0, 1, 2], [5, 9]] }
```

Cada conjunto es una lista de números de impresora (`printers` del índice). Un filamento con `"compatible": 1` sirve
para las impresoras 5 y 9.

## G-code de inicio y de fin

1. Viene en el perfil de la impresora solo si usa las variables simples del motor (`[nombre]`, `{nombre}`,
   `{nombre[i]}`). Lo que salía del perfil de la impresora ya está escrito como número.
2. **Temperaturas,** que llena el perfil del filamento: `temperature`, `first_layer_temperature`, `bed_temperature`,
   `first_layer_bed_temperature`, `chamber_temperature`, `chamber_minimal_temperature`, `idle_temperature`.
3. **Valores del laminado** que el motor tiene que ofrecer con estos nombres: `first_layer_min_x`,
   `first_layer_min_y`, `first_layer_max_x`, `first_layer_max_y` y `first_layer_size_x`, `first_layer_size_y` (la caja
   de la primera capa, mm); `layer_count`; `initial_tool` (0 con un extrusor); `travel_speed` (mm/s); `layer_height`,
   `first_layer_height` (mm); `year`, `month`, `day`, `hour`, `minute`, `second`; `input_filename_base` (el nombre del
   modelo sin extensión).
4. Una impresora sin G-code de inicio o de fin usa el del motor para su dialecto.

## G-code de la máquina con macros (`gcode`, desde el 05-11-2026)

1. **Dónde:** cada modelo FFF trae `common.gcode` y cada variante su `gcode`, que va encima (como `settings`). Los
   filamentos traen el suyo igual. Es el G-code original de la máquina, con macros; el motor lo interpreta según
   `gcode_macros.md`. `profile.settings.machine.start_gcode` y `end_gcode` siguen siendo la versión simple de arriba.
2. **Campos:**
   - `dialect`: `prusaslicer`, `orcaslicer`, `crealityprint`, `elegooslicer` o `cura` (el lenguaje y los nombres de
     las variables);
   - `templates`: el G-code por nombre: `start`, `end`, `before_layer_change`, `layer_change`, `toolchange`,
     `between_objects`, `color_change`, `pause`, `custom`, `timelapse`, `extrusion_role_change`; en filamentos
     `filament_start` y `filament_end`; en `cura`, `extruder_start` y `extruder_end` son una lista, uno por extrusor
     (`null` si falta). Solo estos textos tienen macros;
   - `relative_extrusion`: si ese G-code espera extrusión relativa (`null` si no se sabe). Al usar `templates`, el motor
     extruye como dice este campo, aunque el perfil diga otra cosa (en `cura` casi siempre `false`, porque su inicio
     pone `M82`);
   - `values`: solo en la familia `orcaslicer`: valores de la máquina con los nombres del dialecto, para llenar sus
     variables;
   - `expressions`: solo en `cura`, G-code que su programa calcula con un lenguaje de programación: el motor no lo
     evalúa ni lo usa;
   - `origin`, `source`, `keys`, `from`: de dónde salió; solo para mostrar.
   - en los perfiles de impresión y de filamento, `source_values` (`{dialect, preset, values}`): los valores del perfil del
     programa de origen con los nombres del dialecto, solo los que usa el G-code de las máquinas y que no tienen nombre
     equivalente en `prusaslicer`. El motor los suma a las variables del G-code igual que `values` de la máquina; el
     perfil traducido manda si una variable sale de los dos.
3. **Nombres de variables por dialecto:** un G-code `orcaslicer` (y `crealityprint`, `elegooslicer`) usa sus nombres,
   por ejemplo `nozzle_temperature_initial_layer` por `first_layer_temperature` y
   `bed_temperature_initial_layer_single` por `first_layer_bed_temperature`. El motor los llena con el mismo valor
   (lista en `gcode_macros.md`, regla 9).

## Tabla de traducción (`translation.tar.xz`, desde el 09-11-2026)

1. **Dónde:** `translation/<dialecto>.json` para `prusaslicer`, `orcaslicer`, `crealityprint`, `elegooslicer` y
   `cura`, en `translation.tar.xz`; `index.json` lo nombra en `translation` (rutas) y `translation_archive` (nombre,
   tamaño y SHA-256). La usa `project_import.md`, regla 11.
2. **Una fila por clave de origen:** `engine` (la clave del motor o `null`), `group` (`translated`, `similar` o
   `none`) y `conversion`, de uno de estos tipos: `number`, `flag`, `factor`, `map` (valor a valor), `percent_of`
   (con `base`, otra clave), `first_item` (primer valor de una lista), `number_strip_percent`, `number_or_percent`,
   `absolute_only`, `at_least`, `points` y `rule` (una regla con nombre, documentada en el `FORMAT.md` del paquete).
3. **En los dialectos que no son `prusaslicer`:** además `prusaslicer` (el nombre equivalente), `seen` (en cuántos
   perfiles coincide) y `prusaslicer_values` (mapa de valores, si cambian; también en una clave que conserva su nombre, por ejemplo
   `seam_position` `back` → `rear`). Primero se aplica `prusaslicer_values` y después la conversión; un valor que no
   está en el mapa pasa igual.
   - `percent_of` (11-11-2026): `"nozzle"` cuando un valor `"N%"` de ese dialecto es un porcentaje de la boquilla (los
     anchos de OrcaSlicer, por ejemplo `line_width`), y no de la altura de capa como en `prusaslicer`. Al leerlo, el
     motor pasa `"N%"` a mm con la boquilla del proyecto (o del perfil de impresora) antes de seguir con la conversión
     de `prusaslicer`; un valor en mm pasa igual (PF12).
     `percent_of` también puede nombrar una clave del motor (por ejemplo `"acceleration.default"`): `"N%"` se pasa a
     número con el valor de esa clave en el proyecto antes de traducir; las aceleraciones en % de OrcaSlicer usan esto
     (PF13).
   - `plate_temperature` (11-11-2026), en las tablas de la familia OrcaSlicer: lista placa → (clave de temperatura,
     clave de primera capa), por nombre («Cool Plate», «Engineering Plate», «High Temp Plate», «Textured PEI Plate»,
     «Textured Cool Plate», «Supertack Plate») y por índice (0 «Default Plate» a 6). Al abrir el proyecto, la
     temperatura de cama y la de la primera capa salen de la placa de `curr_bed_type`; si falta, de `default_bed_type`;
     si falta, de la primera con valor en el orden High Temp, Textured PEI, Engineering, Cool, Textured Cool,
     Supertack (como el conversor del paquete) (PF14).
4. **Es la misma traducción** con que el paquete convierte los perfiles: un proyecto y un perfil del mismo programa
   dan los mismos ajustes del motor.
5. **Cobertura** (09-11-2026, claves de origen / con clave del motor): prusaslicer 806 / 88; orcaslicer 881 / 57;
   crealityprint 509 / 35; elegooslicer 90 / 0; cura 739 / 3.

## Datos de la comunidad (`community.tar.xz`)

1. **Rutas:** `community/printers/<Fabricante>/<Modelo>.json`, `community/filaments/<Fabricante>/<Nombre>.json`,
   `community/resins/<Fabricante>/<Nombre>.json`.
2. **Un archivo:** `vendor`; `model` (impresoras) o `name` (filamentos y resinas); `pack_model` opcional
   (`bundle` e `id` del modelo en el índice); `data`, una lista de valores.
3. **Un valor:** `key`, `value`, `unit`, `kind`, `conditions` (opcional) y `source` con `type` (`manufacturer`,
   `video` u otros), `url`, `title`, `author`, `published`, `at` (minuto de un video) y `accessed`.
4. **`kind`:**
   - `official` (lo publica el fabricante): puede llenar un ajuste vacío del perfil;
   - `measured` (medición con método): se muestra como dato medido, con su fuente; nunca reemplaza solo un ajuste;
   - `community` (recomendación sin medición): se muestra como sugerencia, con su fuente.
5. **Claves:** las de `docs/CONFIG.md`, o estas: `measured.max_flow`, `measured.acceleration`, `measured.speed`,
   `input_shaper.frequency_x`, `input_shaper.frequency_y`, `input_shaper.type`, `head.clearance_radius`,
   `head.gantry_height`, `head.nozzle_to_fan_height`, `bed.mesh_points`, `firmware.version`,
   `filament.tensile_strength_xy`, `filament.tensile_strength_z`, `filament.tensile_modulus_xy`,
   `filament.tensile_modulus_z`, `filament.elongation_xy`, `filament.elongation_z`, `filament.impact_strength`,
   `filament.hdt`, `filament.glass_transition`, `filament.shrinkage_xy`, `filament.shrinkage_z`,
   `filament.drying_temperature`, `filament.drying_hours`, `filament.price_per_kg`, `resin.exposure`,
   `resin.bottom_exposure`, `resin.viscosity`. Una clave desconocida se ignora.

## Base del asistente (`knowledge.tar.xz`)

1. **Archivos** en `knowledge/`: `materials.json`, `problems.json`, `procedures.json`, `glossary.json`,
   `tutorials.json`, `calibrations.json`.
2. **Cada archivo** es un objeto con `format` (`tisma-knowledge-materials`, `-problems`, `-procedures`, `-glossary`,
   `-tutorials` o `-calibrations`), `version` (1), `language` (hoy `es`), `sources` (`id`, `title`, `author`, `url`),
   `notes` y su lista (`materials`, `problems`, `procedures`, `terms`, `tutorials` o `calibrations`).
   `materials.json` trae además `fields`, que no hace falta leer.
3. **Acciones** de `problems.json` y `tutorials.json` (`type`): `setting` (cambia `key` con `op` `add`, `multiply`,
   `set` o `set_relative` de `of`), `calibration` (abre la prueba por su `id`), `procedure`, `problem`, `hardware` y
   `model` (consejo en `text`). `why` explica la acción; `last_resort` en una causa la deja para el final.
4. **Convenciones:** un campo opcional falta o vale `null` (la fuente no lo da); un rango es `[mín, máx]`.
5. **Cita:** cada archivo nombra sus fuentes en `sources` y el asistente la muestra («Basado en…»).

## Lo que el motor nunca lee

- `original` y `original_common` (los valores con los nombres del programa de origen): ni se leen ni se muestran.
- `stats.json`.

## Pruebas

| # | Caso | Resultado esperado |
|---|---|---|
| PF1 | Índice con `version` 3 | No se carga y avisa que el paquete es más nuevo que el programa |
| PF2 | Un fabricante con el SHA-256 cambiado | Se descarta ese archivo; los demás se instalan |
| PF3 | Un `.tar.xz` con una entrada `../x` | Esa entrada no se descomprime |
| PF4 | Modelo con `common.settings` y una variante que cambia una clave | El perfil de la variante tiene la clave de la variante y el resto de `common` |
| PF5 | Filamento con `"compatible": 1` y `sets` `[[0], [2]]` | Se ofrece solo para la impresora 2 |
| PF6 | Un archivo con `original` lleno | El motor no lo usa: el perfil es el mismo sin ese campo |
| PF7 | Impresión con `extrusion.bridge_flow_ratio` 0,8 y un filamento sin él | Los puentes salen con flujo 0,8, sin aviso de «filament setting» |
| PF8 | Impresora MMU con `wipe_tower.purge_volumes` 70; impresión sin él / con 120 | Cada cambio purga 70 mm³ / 120 mm³ |
| PF9 | Filamento con `extrusion.type` «PLA» | Sin aviso; el tipo queda «PLA» |
| PF10 | Filamento con `extrusion.travel` `{"ramping_lift": true, "slope": 1, "max_lift": 0.6}` | Sin aviso; los viajes con ese filamento suben en rampa hasta 0,6 mm |
| PF11 | Impresión de resina con `resin.bottom_layers` 4, resina con 6 | 4 capas de base, sin aviso |
| PF12 | Proyecto 3MF de OrcaSlicer con boquilla 0,6 y `outer_wall_line_width` "105%" | La pared externa a 0,63 mm (no 105 % de la altura de capa) |
| PF13 | Proyecto 3MF de OrcaSlicer con `default_acceleration` 5000 y `bridge_acceleration` "50%" | Puentes a 2500 mm/s² |
| PF14 | 3MF de OrcaSlicer con `curr_bed_type` «Textured PEI Plate», `textured_plate_temp` 65 y `hot_plate_temp` 55 / sin `curr_bed_type` ni `default_bed_type` | Cama a 65 °C / a 55 °C (High Temp primero) |
