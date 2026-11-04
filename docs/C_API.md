# API en C del motor

La interfaz para las aplicaciones que no pueden enlazar C++ directamente: la app de Android por el NDK
(`mobile_app.md`, regla 2), y cualquier otra que hable C. El encabezado es `include/tisma/tisma_engine.h`; este
documento explica cómo se usa.

**Estado (2026-11-04):** interfaz acordada con la sesión de la app de Android; **todavía no está implementada**. La
app construye sus pantallas contra una versión falsa. La implementación va después de los ítems actuales de la hoja de
ruta, salvo que el dueño diga que es urgente.

**Primera versión prevista:** abrir, ajustes, laminar con avance y cancelar, resumen y G-code. Después: vista previa
por capas, análisis y estimación de memoria.

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
