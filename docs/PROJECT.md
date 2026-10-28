# Proyectos

Un proyecto (`.tisma`) guarda en un solo archivo todo lo necesario para retomar una impresión: los ajustes, los
perfiles elegidos (`docs/PROFILES.md`), las piezas y la calibración que se está imprimiendo. Pedido del dueño
(«opciones de guardar», 2026-10-24). Formato propio; 3MF llega en P4 (`PLAN.md`).

## En la interfaz

- **Guardar el proyecto:** menú **Más** o Ctrl+S. La primera vez pide dónde; después guarda en el mismo archivo.
- **Guardar el proyecto como…:** menú **Más** o Ctrl+Mayús+S.
- **Abrir** (Ctrl+O) también abre proyectos, y se pueden soltar en la ventana.

Al abrir un proyecto:
- vuelven los ajustes y los perfiles elegidos;
- la pieza vuelve, centrada en la cama;
- si era una torre de calibración, vuelve a la pestaña Calibración con sus segmentos y se lamina sola, para seguir
  en el paso donde se estaba (elegir el mejor segmento).
- un proyecto sin pieza (guardado antes de abrir un modelo) trae solo los ajustes y los perfiles.

## El archivo

Binario, little-endian, por partes (*chunks*):

| Bytes | Qué es |
|---|---|
| 8 | `TISMAPRJ` |
| 4 | La versión del formato (1) |
| Repetido hasta el final | Una parte: 4 letras de etiqueta, 8 bytes de tamaño, los bytes |

| Parte | Cuántas | Contenido |
|---|---|---|
| `JSON` | Una, la primera | Los ajustes, los perfiles, las piezas y la calibración (abajo) |
| `MESH` | Una por pieza, en orden | La malla de la pieza como STL binario (la especificación pública del formato) |
| `PNT ` | Una por pieza pintada, en orden | Sus caras pintadas (`docs/PAINTING.md`): por canal, la cantidad de triángulos pintados, y de cada uno su número, el largo de su árbol y el árbol (números de 32 bits, little-endian) |
| Otras | — | Se saltean: así una versión nueva puede agregar partes (por ejemplo, una miniatura) sin romper a las viejas |

El JSON:

```json
{
  "format": "tisma-project",
  "version": 1,
  "settings": { "...": "todos los ajustes, en el formato de docs/CONFIG.md, con los cambios por altura" },
  "profiles": { "printer": "Marlin 256 × 256 mm", "filament": "PLA", "print": "0.20 mm" },
  "models": [ { "name": "pieza.stl", "mesh": 0, "offset": [128.0, 128.0, 0.0] } ],
  "calibration": { "kind": "temperature", "unit": "C", "segments": [[0.0, 10.4, 230.0], [10.4, 20.4, 225.0]] }
}
```

- `models[i].mesh` es el número de la parte `MESH` (contando desde 0).
- `models[i].painting`, solo en una pieza pintada, es el número de su parte `PNT `. Un árbol roto, una marca fuera de
  rango o un triángulo que la malla no tiene se rechazan.
- `offset` es dónde estaba la pieza en la cama, en mm. Hoy la interfaz siempre centra la pieza; el dato queda para
  cuando se pueda mover.
- `calibration` está solo si se guardó una torre. Cada segmento es `[z0, z1, valor]`.
- `models[i].extruder` (solo si no es 1) es el extrusor de la pieza, desde 1, o el número de una mezcla de colores;
  `models[i].wipe_into_infill` y `wipe_into_object` (solo si son `true`) dejan que un cambio de filamento purgue en su
  relleno o en toda la pieza (ficha `multi_extruder.md`, reglas 1 y 13).
- `models[i].settings` (solo si tiene) son los ajustes propios de la pieza: un objeto de grupos con las claves del
  perfil (`{"infill": {"density": 40}}`), que valen solo para ella (`model_tools.md`, regla 7). Hoy pueden ser de
  `walls`, `infill` y `surfaces`; otro grupo es un error al abrir.
- `color_mixing` (solo si hay mezclas o colores de filamento) tiene las mezclas del proyecto en el formato de la ficha
  `color_mixing.md` (regla 15): `physical_extruders` con el color de cada filamento y `virtual_extruders` con cada
  mezcla, su número, `kind` (`fullspectrum` o `gradient`), su color, sus componentes (`extruder` y `ratio`, o
  `position` en un degradé) y, en un degradé, `minz` y `maxz`. Al abrir el proyecto en una impresora con menos
  filamentos, las mezclas que usan los que faltan se descartan (regla 5).
- Las mallas se guardan con los mismos `float` que tenían: vuelven exactas.

**Errores:** un archivo que no empieza con `TISMAPRJ`, cortado, con una versión mayor, con una parte `MESH` antes del
JSON, con una pieza sin su malla o con ajustes mal escritos se rechaza con un mensaje que dice qué está mal (los
ajustes nombran su clave).

**Guardar es seguro:** se escribe a un archivo aparte y después se mueve encima del anterior, así un error al guardar
no deja un proyecto a medias.

## En el código

- **`src/project/project.*`:** `write_project`, `read_project`, `save_project`, `load_project`.
- **`stl_binary` (`src/io/stl.*`):** escribe una malla como STL binario.
- **`apps/tisma/app.cpp`:** guardar y abrir desde la interfaz.
- **Pruebas:** `tests/test_project.cpp`:
  - el STL binario sigue el formato y vuelve exacto;
  - un proyecto (dos piezas, ajustes con cambios por altura, perfiles, calibración) vuelve igual, también por archivo;
  - una torre de calibración vuelve con su malla y sus segmentos;
  - los archivos dañados o más nuevos se rechazan con su motivo, y las partes desconocidas se saltean.
