# Guardar el proyecto y guardado automático

**Qué hace:** guarda todo lo que hay en la placa (piezas, ubicación, ajustes, pinturas, modificadores, capas no
planares, análisis) en un proyecto 3MF (`formats.md`, reglas 6 a 11), con un menú de guardado siempre a mano. Mientras
se trabaja, el programa guarda solo una copia de seguridad, y después de un cierre inesperado ofrece recuperarla.

**Pedido del dueño (28-10-2026):** «que haya también opción para poder guardarlo, que haya un menú automático para
guardar».

**Fase:** P1 (interfaz).

**Relación con otras fichas:**
- `formats.md`: qué lleva el 3MF y cómo se abre;
- `preferences.md`: «Ask for unsaved changes in project» (prueba PF3) sigue valiendo;
- `interface.md`: la barra de arriba y el panel de Laminar abajo a la derecha;
- `print_monitor.md`: al mandar a imprimir, el G-code que se mandó se guarda con el trabajo, no en el proyecto.

## Ajustes

| Clave | Tipo | Por defecto | Rango | Qué hace |
|---|---|---|---|---|
| `ui.autosave` | sí/no | sí | | Guardar solo una copia de seguridad del proyecto mientras se trabaja |
| `ui.autosave_minutes` | entero | 5 | 1 a 60 | Cada cuántos minutos, si hubo cambios |
| `ui.autosave_copies` | entero | 3 | 1 a 20 | Cuántas copias viejas se guardan por proyecto; la más vieja se borra |
| `ui.save_on_slice` | sí/no | no | | Si el proyecto ya tiene archivo, guardarlo también cada vez que se lamina |

Las cuatro van en Preferencias, página General, debajo de «Ask for unsaved changes in project».

## Reglas

1. **El menú «Guardar»:** un botón con flecha en la barra de arriba, al lado de «Abrir» (y en el menú «Más» en
   pantallas angostas), con:
   - **Guardar** (Ctrl+S): si el proyecto ya tiene archivo, lo pisa sin preguntar; si no, hace «Guardar como»;
   - **Guardar como…** (Ctrl+Mayús+S): elegir carpeta y nombre; el nombre propuesto es el de la primera pieza;
   - **Guardar una copia…**: guarda en otro archivo sin cambiar cuál es el archivo del proyecto;
   - **Proyectos recientes**: los últimos 10, con miniatura, nombre, carpeta y fecha; los que ya no existen se ven
     tachados y se pueden quitar;
   - **Copias de seguridad…**: abre la lista de la regla 5;
   - un interruptor **Guardado automático** que es `ui.autosave`, con el intervalo al lado.
2. **Después de laminar:** el panel de Laminar (abajo a la derecha) muestra, junto a «Exportar G-code», el menú de
   enviar a la impresora y «Guardar el proyecto». Con `ui.save_on_slice` y un archivo ya elegido, se guarda solo y lo
   dice con un aviso corto («Proyecto guardado»).
3. **Cambios sin guardar:** el título de la ventana y el nombre del proyecto en la barra muestran un punto (•) mientras
   haya cambios sin guardar. Cuentan como cambio: agregar, quitar, mover, girar o escalar una pieza; cambiar un ajuste
   del proyecto, de una pieza o de un rango; pintar; cambiar de impresora, boquilla o filamento. No cuentan: girar la
   cámara, cambiar de vista, laminar, exportar el G-code.
4. **Guardado automático:** con `ui.autosave`, cada `ui.autosave_minutes` minutos, si hubo cambios desde la última
   copia, el programa guarda una copia de seguridad:
   - en segundo plano, sin trabar la interfaz; si la placa es grande y tarda, la barra de estado dice «Guardando
     copia…»;
   - en la carpeta de datos del programa (`backups/`), nunca encima del archivo del proyecto: guardar el archivo
     siempre es algo que pide la persona;
   - con el nombre del proyecto (o «Sin título») y la fecha y hora;
   - se guardan las últimas `ui.autosave_copies` por proyecto; al guardar el proyecto con éxito, se borran sus copias
     menos la última;
   - si falla (disco lleno, sin permiso), avisa una sola vez y lo vuelve a intentar en el siguiente intervalo.
5. **Recuperar:** al abrir el programa, si quedaron copias más nuevas que el último guardado del proyecto (porque el
   programa se cerró sin guardar o se colgó), muestra una ventana: «El programa se cerró sin guardar. ¿Recuperar el
   proyecto?», con la miniatura, la fecha de la copia y los botones Recuperar, Ver todas las copias y Descartar.
   - Recuperar abre la copia como proyecto sin guardar, con su archivo de origen si lo tenía (Guardar lo pisa);
   - Descartar borra esas copias después de confirmar;
   - si se cerró bien y se eligió no guardar, no se pregunta nada al abrir.
6. **Al cerrar o abrir otro proyecto:** con cambios sin guardar y «Ask for unsaved changes in project» prendido,
   pregunta Guardar, No guardar o Cancelar (prueba PF3). «No guardar» no deja copia para recuperar.
7. **Qué guarda:** todo lo de `formats.md` (reglas 6 a 11) más, para que el proyecto vuelva igual:
   - la impresora, la boquilla y el filamento elegidos, con los cambios sin guardar en sus perfiles;
   - el modo de ajustes (`ui.level`) no se guarda: es de la persona, no del proyecto.
8. **Táctil y teléfono:** en modo táctil (`accessibility_touch.md`) el menú se abre con el mismo botón; el guardado
   automático funciona igual. En el teléfono se guarda en la carpeta de la app y se puede compartir el archivo.
9. **Textos:** en español e inglés, con los mismos nombres en el menú, los avisos y las preferencias.

## Pruebas

| # | Caso | Resultado |
|---|---|---|
| PS1 | Pieza nueva, Ctrl+S | Se abre «Guardar como» con el nombre de la pieza; después del guardado, el punto (•) desaparece |
| PS2 | Mover la pieza, Ctrl+S | Pisa el mismo archivo sin preguntar; al abrirlo, la pieza está en la posición nueva |
| PS3 | Girar la cámara y laminar | No aparece el punto (•) |
| PS4 | `ui.autosave` sí, `ui.autosave_minutes` 1, un cambio, esperar 70 s | Hay una copia en `backups/`; el archivo del proyecto no cambió |
| PS5 | Sin cambios durante 3 minutos | No se agregan copias |
| PS6 | Cortar el programa (matar el proceso) después de PS4 y abrirlo | Ventana de recuperar; Recuperar deja la placa como estaba en la copia, con el punto (•) |
| PS7 | `ui.autosave_copies` 3, seis intervalos con cambios | Quedan 3 copias, las más nuevas |
| PS8 | `ui.save_on_slice` sí, proyecto con archivo, laminar | El archivo se actualiza y sale el aviso «Proyecto guardado» |
| PS9 | Carpeta de copias sin permiso de escritura | Un solo aviso; el programa sigue funcionando |
| PS10 | Proyectos recientes con un archivo borrado | Se ve tachado; abrirlo dice que no existe y ofrece quitarlo de la lista |
