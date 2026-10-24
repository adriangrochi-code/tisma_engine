# Perfiles

El dueño pidió «opciones de guardar» (2026-10-24). Los ajustes se guardan de dos maneras:
- **perfiles con nombre**, en tres partes que se eligen por separado: la impresora, el filamento y la impresión;
- **proyectos** (`docs/PROJECT.md`): todo lo de una impresión en un archivo.

Además siguen los botones **Cargar…** y **Guardar…** del panel, que leen y escriben todos los ajustes en un JSON
(`docs/CONFIG.md`).

Todo es diseño propio. Los perfiles de Tisma Slicer no entran (`docs/spec/SETTINGS_V1.md`); los de fábrica de los
fabricantes llegan en P3 (`PLAN.md`).

## Qué guarda cada perfil

Cada ajuste de `docs/CONFIG.md` pertenece a un solo tipo de perfil (`setting_kind`):

| Perfil | Grupos y ajustes | Por qué |
|---|---|---|
| **Impresora** | `machine` (menos `filament_diameter` y las variables de temperatura), `retraction` | Lo que es de la máquina: la cama, la boquilla, el dialecto, el G-code de inicio y fin, los límites de movimiento. La retracción depende sobre todo del extrusor (directo o Bowden) |
| **Filamento** | `machine.filament_diameter`, las variables con `temperature` en el nombre (`temperature`, `bed_temperature`, `first_layer_temperature`…), `cooling`, `extrusion`, `compensation` | Lo que es del material. La compensación va acá porque cada material se contrae distinto (`docs/CALIBRATION.md`, «Contracción por material») |
| **Impresión** | `layers`, `slicing`, `walls`, `surfaces`, `infill`, `speed`, `adhesion`, `support`, `compute` | Cómo se lamina la pieza |
| (ninguno) | `changes` | Los cambios por altura son de una torre de calibración: se guardan en el proyecto, no en un perfil |

**Elegir un perfil** cambia solo sus ajustes (`with_profile`) y deja los demás como estaban. Las variables del G-code
se separan por nombre: un perfil de impresora reemplaza las variables de la impresora y deja las de temperatura.

## Archivo de un perfil

```json
{
  "format": "tisma-profile",
  "version": 1,
  "kind": "filament",
  "name": "PETG negro",
  "settings": {
    "machine": { "filament_diameter": 1.75, "variables": { "bed_temperature": "80", "temperature": "240" } },
    "cooling": { "min_layer_time": 5, "min_speed": 10, "fan_min": 20, "fan_max": 50, "fan_off_layers": 1 },
    "compensation": { "scale_x": 100, "scale_y": 100, "scale_z": 100, "hole_offset": 0 },
    "extrusion": { "flow": 100 }
  }
}
```

- `settings` tiene el formato de `docs/CONFIG.md`, solo con los ajustes de su tipo. Los que falten toman su valor por
  defecto.
- Un ajuste de otro tipo es un error que nombra la clave («`layers.height`: a print setting, not a filament one»).
- Una versión mayor que la que el programa conoce es un error.

## Dónde se guardan

Los perfiles del usuario son archivos en la carpeta de configuración de la interfaz (al lado de `interface.json`,
`docs/UI.md`), una carpeta por tipo:

```
profiles/printer/Mi impresora.json
profiles/filament/PETG negro.json
profiles/print/Detalle fino.json
```

- **El nombre del archivo** sale del nombre del perfil, sin los caracteres que algún sistema no acepta. Si dos nombres
  dan el mismo archivo, el segundo lleva « (2)». El nombre real está adentro del archivo.
- **Guardar con un nombre que ya existe** reemplaza ese perfil.
- **Se escribe aparte y después se mueve encima:** un error al guardar deja el archivo anterior entero.
- **Al listar**, los archivos rotos se saltean y se avisan.

## Perfiles de fábrica

Además, el catálogo (`docs/CATALOG.md`) hace perfiles de impresoras y filamentos reales desde los datos de sus
fabricantes.

La interfaz trae perfiles genéricos (`src/ui/presets.*`), para tener de dónde partir. No se pueden sobrescribir:
**Guardar** sobre uno de ellos pide un nombre nuevo.

| Perfil | Valores |
|---|---|
| Impresora **Marlin 256 × 256 mm** | Los ajustes con los que arranca la interfaz: cama de 256 × 256 mm, 256 mm de alto, Marlin |
| Filamento **PLA** | 210 °C, cama 60 °C, ventilador de 35 a 100 % |
| Filamento **PETG** | 240 °C, cama 80 °C, ventilador de 20 a 50 % |
| Filamento **ABS** | 250 °C, cama 100 °C, ventilador de 0 a 20 %, apagado las 3 primeras capas |
| Filamento **TPU** | 225 °C, cama 50 °C, ventilador de 50 a 100 % |
| Impresión **0.20 mm** | Los valores por defecto del motor |
| Impresión **0.12 mm** | Capas de 0,12 mm, 5 capas superiores y 4 inferiores |
| Impresión **0.28 mm** | Capas de 0,28 mm (también la primera), 2 paredes, relleno al 15 % |

Son valores comunes de la impresión 3D, elegidos acá; no salen de los perfiles de ningún otro programa.

## En la interfaz

Arriba de cada sección del panel de ajustes (Impresión, Filamento, Impresora) hay una fila con:
- la lista de perfiles: los de fábrica y los del usuario. Un **asterisco** después del nombre avisa que los ajustes
  cambiaron desde que se eligió;
- **Guardar** (ícono de disco): guarda los cambios en el perfil elegido, o pide un nombre si es de fábrica;
- **Más** (⋮): **Guardar como perfil nuevo…** y **Eliminar**.

La interfaz recuerda qué perfil está elegido en cada parte, y el proyecto también lo guarda.

## En el código

- **`src/config/profiles.*`:** el reparto (`setting_kind`), `with_profile`, `same_profile`, el archivo
  (`profile_file_json`, `parse_profile`) y la carpeta (`load_profiles`, `save_profile`, `remove_profile`).
- **`src/ui/presets.*`:** los perfiles de fábrica.
- **`apps/tisma/app.cpp`:** las filas de perfiles del panel.
- **Pruebas:** `tests/test_profiles.cpp`:
  - cada ajuste tiene un solo tipo, salvo los cambios por altura;
  - elegir un perfil cambia solo lo suyo, y los tres juntos dan todo de vuelta;
  - el archivo vuelve igual, y los errores nombran la clave;
  - guardar, reemplazar, listar, eliminar, nombres que chocan y archivos rotos.
