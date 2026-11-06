# Mi biblioteca en la línea de comandos

Ficha: `docs/spec/features/local_library.md` (en `spec/next`). Código: `src/library/library.*` y
`src/cli/library_cli.*`. Lo usan la app de Android y, más adelante, la interfaz.

Cada entrada es una carpeta de la biblioteca: `entry.json` (`format` `tisma-library-entry`, `version` 1),
`project.3mf`, `thumb.png`, `recipes/<n>/settings.json`, `analyses/<n>.tea` o `.taero` con su `.json`, y `photos/`.
Nunca un G-code (regla 9).

| Comando | Qué hace |
|---|---|
| `library list --dir D` | `{"entries": [...]}`: id, nombre, etiquetas, impresoras, materiales, cantidad de recetas y análisis, bytes, última modificación, `thumb` (`"thumb.png"` o `null`) |
| `library show --dir D --entry ID` | El `entry.json` |
| `library save --dir D (--new \| --entry ID) --model P.3mf --config S.json --recipe R.json [--summary S.json] [--analysis A] [--analysis-summary A.json] [--name N] [--thumb T.png]` | Guarda (regla 3). La receta `R` se guarda tal cual; si tiene la misma `printer`, `nozzle`, `filament` y `print` que una que ya está, la reemplaza. El resumen es el de `slice --summary`. Un análisis guarda los ajustes que cambian la resistencia (capa, paredes, relleno, patrón, material) |
| `library check --dir D --entry ID --recipe N --config S.json` | `same_settings`, `older_engine`, `missing_keys` (claves que este motor ya no conoce) y, por análisis, `valid` y `changed` (regla 7) |
| `library printed --dir D --entry ID [--date ISO] [--photo P.jpg] [--note TEXTO]` | «La imprimí» (regla 1); la foto va a `photos/` |
| `library delete --dir D --entry ID [--recipe N \| --analysis N]` | Borra la entrada, una receta o un análisis |
| `library export --dir D --entry ID -o F.tismalib` | Un ZIP con la carpeta de la entrada (regla 10) |
| `library import F.tismalib --dir D [--replace \| --new]` | Revisa todo antes de escribir: una sola carpeta, solo los archivos del formato, nada fuera de ella (ML6). Si ya existe: `{"exists": ID}` con código 3 |

Errores: `{"code", "error", "error_es"}` en la salida estándar, código 2.
