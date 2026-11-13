# API en C del motor

La interfaz para las aplicaciones que no pueden enlazar C++ directamente: la app de Android por el NDK
(`mobile_app.md`, regla 2), y cualquier otra que hable C. El encabezado es `include/tisma/tisma_engine.h`; este
documento explica cómo se usa.

**Estado (2026-11-12):** interfaz acordada con la sesión de la app de Android el 2026-11-04. **Implementada la
primera versión** (`src/cli/c_api.cpp`, pedido de la sesión del iPhone): contexto, abrir por ruta o bytes, ajustes,
estimación y límite de memoria, laminar con avance y cancelar, resumen y G-code (entero o por pedazos). Va en la
biblioteca de iOS (`tools/build_ios.sh`). La vista previa por capas y el análisis están desde el 2026-11-13.

## Cómo funciona la primera versión

- Cada contexto tiene una carpeta propia en la carpeta temporal del sistema (los ajustes, el modelo dado en bytes y
  el resultado), que se borra con `tisma_destroy`.
- Laminar corre el mismo `slice <modelo> --config` de la línea de comandos en un hilo del contexto: el G-code y sus
  comprobaciones son los mismos que en el escritorio.
- **Avance:** por etapas, con el nombre de la etapa: `loading` 0, `slicing` 5, `walls` 15, `surfaces` 30, `support` 40,
  `infill` 50, `gcode` 60, `finishing` 95, y `done` 100 (o `cancelled`, `failed`).
- **Cancelar:** se nota en la próxima etapa. Medido: entre 0,05 y 0,5 s en una esfera de 319 200 triángulos. El
  trabajo queda en `TISMA_STOPPED` con el código `TISMA_CANCELLED`, sin resultado.
- **Ajustes:** hay que darlos con `tisma_set_settings_json` antes de laminar (si no, `TISMA_BAD_SETTINGS`). Los
  scripts de posprocesado no corren: en una app no hay nadie que los confirme.
- **Proyectos:** un 3MF abre como modelo; sus ajustes todavía no se toman.
- **Memoria:** `tisma_estimate_memory` es una cota por arriba ajustada a lo medido en Linux x86_64: 80 MB, más 200 B
  por triángulo, más 16 B por mm² de la caja del modelo y por capa. La tabla de lo medido está en `PROGRESS.md`
  (2026-11-12).
- **Costo:** los gramos por `extrusion.cost` (dinero por kg), redondeado a centésimos. Sale `null` sin precio o sin
  la densidad del filamento (`extrusion.density`), que es lo que da los gramos.
- **Vista previa:** se lee del G-code terminado la primera vez que se pide, igual que la vista previa del escritorio,
  y queda en el contexto hasta el próximo trabajo. Hoy el G-code entero está en memoria mientras se lee; para un
  teléfono es lo próximo a mejorar si pesa. Tipos: las superficies de abajo salen como relleno sólido
  (`TISMA_PATH_BOTTOM` todavía no se da), y el relleno de huecos, el planchado y la torre de purga son `TISMA_PATH_OTHER`.
- **Análisis:**
  - `tisma_analyze_start(objeto, calidad)` corre el análisis de un proyecto abierto con `tisma_open_path`, igual que
    los nodos (`analyze <proyecto> --sandbox --mode analyze`), con los ajustes del proyecto.
  - El avance sale de sus líneas `progress`, leídas a medida que llegan; ahí se mira también la cancelación.
  - `tisma_analysis_json` da el resumen: veredicto, `min_safety` con su rango, desplazamiento, masa, material y vóxeles.
  - Sin zona fija o sin fuerza: `TISMA_NEEDS_CONDITIONS`; demasiado grande para la memoria: `TISMA_TOO_LARGE`.
  - Un análisis no deja G-code ni vista previa.
- **Errores de laminar:** el código sale del de la línea de comandos (2 → `TISMA_CANNOT_READ`, 4 →
  `TISMA_BAD_MODEL`, el resto `TISMA_ERROR`) y el mensaje es su primera línea `error:`.

## Reglas

- **Contexto:** `tisma_create` da un contexto y `tisma_destroy` lo libera. Cada contexto tiene su placa, sus ajustes
  y su último resultado.
- **Hilos:**
  - un contexto se usa desde un solo hilo a la vez;
  - laminar y analizar corren en un hilo propio del motor, y quien llama pregunta el avance con `tisma_poll`;
  - el motor no llama nunca funciones de la app (no hay callbacks).
- **Textos:** en UTF-8 y propiedad del contexto. Valen hasta la próxima llamada que devuelva un texto del mismo tipo:
  hay que copiarlos.
- **Errores:** `tisma_status` trae un código estable (`tisma_code`) y un texto en inglés para registros. La app
  traduce por código, como en la regla 19a de `cloud.md`.
- **Trabajos:** uno por contexto. Pedir otro mientras corre da `TISMA_BUSY`, y `tisma_cancel` lo para.

## Uso

1. `tisma_open_path` o `tisma_open_bytes`: STL, OBJ o 3MF. Un proyecto de Tisma trae sus ajustes y sus condiciones de
   Ingeniería.
2. `tisma_set_settings_json`, con las claves de `docs/CONFIG.md`. `tisma_get_settings_json` devuelve los ajustes
   efectivos.
3. `tisma_estimate_memory`. Si se fija un límite con `tisma_set_memory_limit`, laminar se niega con
   `TISMA_TOO_LARGE` cuando lo pasa (`mobile_app.md`, regla 19).
4. `tisma_slice_start`, y después `tisma_poll` hasta que el estado sea `TISMA_DONE`, `TISMA_FAILED` o
   `TISMA_STOPPED`.
5. `tisma_summary_json`: tiempo de impresión en segundos, filamento en mm y g, capas, costo (o `null`) y avisos.
6. `tisma_write_gcode` (a una ruta), o `tisma_read_gcode` por pedazos.
7. Vista previa: `tisma_layer_count` y `tisma_layer_z`. Después, `tisma_preview_segments` por rangos de capas y por
   tandas: cada segmento con sus extremos, ancho, alto, velocidad y tipo.
8. Ingeniería: `tisma_analyze_start(objeto, calidad)`, el mismo `tisma_poll`, y `tisma_analysis_json`, que da el
   informe estructural. Lleva el veredicto, el factor de seguridad con su rango probable, el desplazamiento y la
   masa: es el mismo informe que el `.tea` de la nube.

## Compilación (prevista)

- Una biblioteca compartida `libtisma_engine` que se compila con el NDK, para `arm64-v8a` y `x86_64`, sin
  dependencias de Android.
- Las mismas bibliotecas de terceros del motor (`THIRD_PARTY.md`), sin la interfaz gráfica ni la GPU.
