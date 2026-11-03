# Proyectos

Un proyecto (`.tisma`) guarda en un solo archivo todo lo necesario para retomar una impresión: los ajustes, los
perfiles elegidos (`docs/PROFILES.md`), las piezas y la calibración que se está imprimiendo. Pedido del dueño
(«opciones de guardar», 2026-10-24). Dos formas del mismo contenido: el archivo propio (`.tisma`) y un paquete 3MF
(`.3mf`, desde el 2026-11-02; ver «El paquete 3MF»).

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
- La malla se guarda en las coordenadas propias de la pieza, sin su lugar en la cama, así las condiciones de
  Ingeniería no cambian aunque la pieza se mueva (`formats.md`, reglas 8 y 11 bis; FM24). Su lugar va aparte: un
  punto `p` de la malla está en la cama en `turn · p + offset`.
  - `offset`, en mm.
  - `turn`, solo si no es la identidad: nueve números por filas, con el giro y la escala.
- Los proyectos guardados antes del 2026-11-03 tienen la malla ya ubicada y `offset` en 0; se abren igual, y su marco
  es aquel lugar de la cama.
- `calibration` está solo si se guardó una torre. Cada segmento es `[z0, z1, valor]`.
- `models[i].extruder` (solo si no es 1) es el extrusor de la pieza, desde 1, o el número de una mezcla de colores;
  `models[i].wipe_into_infill` y `wipe_into_object` (solo si son `true`) dejan que un cambio de filamento purgue en su
  relleno o en toda la pieza (ficha `multi_extruder.md`, reglas 1 y 13).
- `models[i].settings` (solo si tiene) son los ajustes propios de la pieza: un objeto de grupos con las claves del
  perfil (`{"infill": {"density": 40}}`), que valen solo para ella (`model_tools.md`, regla 7). Hoy pueden ser de
  `walls`, `infill` y `surfaces`; otro grupo es un error al abrir.
- `models[i].volumes` (solo si tiene) son sus modificadores y volúmenes negativos: `kind` (`modifier` o `negative`),
  `shape` (`box` o `cylinder`), `offset` y `size` en mm (el lugar desde el centro de la caja de la pieza en X e Y y
  desde su base en Z, en la malla tal como se guarda), `turn` (solo si está girado: su giro propio alrededor de su
  centro, nueve números por filas) y, en un modificador, `settings` (`model_tools.md`, reglas 2 y 3). La pieza se
  guarda como está en la cama, con su giro y su escala aplicados: sus volúmenes se guardan donde quedan, y uno de
  una pieza girada se guarda con `turn`.
- `models[i].ranges` (solo si tiene) son sus rangos de alturas: `from` y `to` en mm sobre la base de la pieza y
  `settings`, que pueden tener `layers.height` (regla 6).
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

## El paquete 3MF

`formats.md`, reglas 6 a 11. Se guarda así cuando el nombre termina en `.3mf`, y así va siempre a Tisma Cloud: el
servidor solo acepta un ZIP con `3D/*.model` con triángulos.

| Archivo | Qué tiene |
|---|---|
| `[Content_Types].xml`, `_rels/.rels` | Los del estándar |
| `3D/3dmodel.model` | Cada modelo es un objeto con su malla en sus coordenadas propias; su giro, su escala y su corrimiento van en la transformación de su ítem de `<build>`. Cualquier lector de 3MF ve la placa |
| `Metadata/tisma_project.json` | El mismo JSON que la parte `JSON` del archivo propio, con los corrimientos en 0. `"mesh": n` es el ítem n de `<build>` y `"painting": k` es el archivo `tisma_paint_<k>.bin` |
| `Metadata/tisma_paint_<k>.bin` | Una pintura: `TPNT`, la versión (32 bits, 1) y la misma pintura que la parte `PNT ` (`painting_to_bytes`). El encabezado evita que empiece como un programa (`MZ`, ELF y otros), que el servidor rechaza |

- Lo que revisa el servidor (equipo de fichas, 2026-11-02): ZIP sin rutas raras ni cifrado, hasta 10 000 entradas,
  ningún archivo de programa (por extensión ni por sus primeros bytes), y nada de `<!DOCTYPE` en `.model` ni
  `.rels`. El nodo copia cada entrada tal cual.
- Las coordenadas se escriben con 9 cifras, para que vuelvan como el mismo `float`. El mismo proyecto da el mismo
  G-code desde `.tisma` y desde `.3mf` (pruebas `ui_project_3mf_*` y `[cli][cloud]`).
- Al abrirlo, los modelos vuelven ya ubicados, con su ítem aplicado y corrimiento 0.
- Abrir un `.3mf` lo trata como proyecto si tiene `Metadata/tisma_project.json`; si no, es un modelo.
- Un 3MF de otro programa todavía no se lee como proyecto (`formats.md`, regla 18).

## En el código

- **`src/project/project.*`:** `write_project`, `read_project`, `save_project`, `load_project`.
- **`stl_binary` (`src/io/stl.*`):** escribe una malla como STL binario.
- **`apps/tisma/app.cpp`:** guardar y abrir desde la interfaz.
- **Pruebas:** `tests/test_project.cpp`:
  - el STL binario sigue el formato y vuelve exacto;
  - un proyecto (dos piezas, ajustes con cambios por altura, perfiles, calibración) vuelve igual, también por archivo;
  - una torre de calibración vuelve con su malla y sus segmentos;
  - los archivos dañados o más nuevos se rechazan con su motivo, y las partes desconocidas se saltean.
