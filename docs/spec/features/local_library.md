# Mi biblioteca: piezas guardadas con su laminado y su análisis

**Qué hace:** una biblioteca propia en la computadora y en el teléfono, sin cuenta ni red, donde cada pieza se guarda
con lo que hace falta para volver a imprimirla fácil: el proyecto, los ajustes con que se laminó para cada impresora
y su análisis de Ingeniería. No guarda G-code: al imprimir se lamina de nuevo, en el equipo o en la nube (decisión del
dueño, 05-11-2026: «hagamos que lamine en el teléfono/escritorio o la nube así no guardamos gcode»). Más adelante una entrada se puede publicar en la biblioteca de Tisma
Cloud (`model_library.md`).

**Pedido del dueño** (05-11-2026): «nuestra propia biblioteca de modelos que guarde las capas y otros datos para que
sea más fácil imprimir, guarde algunos ajustes para laminar fácil, y guarde los resultados de FEA para imprimir mejor
las piezas». Primero local; publicable después.

**Fase:** P3, en la computadora y en Android a la vez (mismo formato). La propuesta inicial es de la sesión de Android;
las decisiones abiertas las tomó el equipo de fichas el 05-11-2026 y quedan marcadas.

## Una entrada

1. **Qué guarda:**
   - el proyecto como paquete 3MF (`project_save.md`), con sus piezas, pinturas y condiciones de Ingeniería;
   - por cada impresora con que se laminó, una **receta**: la impresora (del paquete de perfiles o propia), el
     filamento, el perfil de impresión, los cambios a mano, la versión del motor y un resumen de los ajustes (`SHA-256`
     del archivo de ajustes completo);
   - el **resumen del último laminado** de esa receta (tiempo, filamento en metros y en gramos, capas), solo para
     mostrarlo en la lista; el G-code no se guarda;
   - los **análisis** (FEA `.tea`, CFD `.taero`) con su resumen en JSON (`model_library.md`, regla 24) y los ajustes
     con que se calcularon (`model_library.md`, regla 2);
   - una miniatura (PNG de 256 × 256), el nombre, etiquetas, notas y, si se imprimió, «La imprimí» con fecha, una foto
     opcional y una nota.
   - la fecha de «La imprimí» es un día real en formato `AAAA-MM-DD` (ISO 8601); sin fecha, el día de hoy en la
     computadora. Otra cosa («ayer», `2026-13-45`, `2026-02-30`) es un error de uso que nombra el formato, y no se
     guarda nada. La app del teléfono manda la fecha en ese mismo formato.
2. **Ninguna ruta de la computadora** queda guardada: la entrada se puede mover entre equipos.

## Acciones

3. **Guardar en la biblioteca:** desde el plato, después de laminar o de analizar. Si la pieza ya está en la
   biblioteca, se agrega la receta o el análisis nuevo a esa entrada (no se duplica la pieza). «Guardar como nueva»
   crea otra.
4. **Ver y buscar:** una lista con miniatura, nombre e íconos de «tiene laminado» y «tiene análisis»; buscar por nombre,
   etiqueta, impresora, material o «con análisis»; ordenar por fecha o nombre.
5. **Abrir:** pone el proyecto en el plato con la receta elegida (la de la impresora actual si hay una; si no, la última
   usada).
6. **Imprimir:** siempre se lamina con la receta de la impresora elegida (o con los ajustes actuales, si la persona
   los cambió), en el equipo o en Tisma Cloud (`cloud.md`) según lo que elija la persona y lo que permita su plan, y se
   manda el resultado a la impresora. El resumen de la receta se actualiza con ese laminado.
7. **Cuándo ya no coincide:**
   - **la receta:** si la versión del motor cambió desde que se guardó, la entrada avisa «Receta de una versión
     anterior: puede imprimir distinto» (la próxima impresión ya usa el motor nuevo); si un ajuste de la receta ya no
     existe, se dice cuál y se usa el valor por defecto;
   - **el análisis:** vale solo con los mismos ajustes que cambian la resistencia (`model_library.md`, regla 3); si
     alguno cambió, «El análisis no vale para estos ajustes» y se ofrece calcular de nuevo (en el equipo o en la nube).
8. **Borrar:** una entrada, una receta o un análisis, con confirmación; se puede deshacer hasta cerrar el programa.

## Formato y lugar

9. **Formato único** en la computadora y en el teléfono (decisión del equipo de fichas): cada entrada es una carpeta
   con `entry.json` (los datos de arriba, `format` `tisma-library-entry`, `version` 1), `project.3mf`, `thumb.png`,
   `recipes/<n>/settings.json`, `analyses/<n>.tea` o `.taero` con su `.json`, y `photos/`. Ningún
   G-code.
10. **Exportar e importar** una entrada como un solo archivo ZIP con esa carpeta adentro, de extensión **`.tismalib`**
    (decisión del equipo de fichas). Al importar se revisa como un proyecto del servidor (sin rutas raras ni archivos
    de programa) y si ya existe se pregunta «Reemplazar» o «Guardar como nueva». Compartir desde Android usa el mismo
    archivo.
11. **Dónde:** en la carpeta de datos del usuario (computadora) o en el almacenamiento propio de la app (Android), en
    `library/`. Las preferencias permiten mover la biblioteca a otra carpeta en la computadora.
12. **Límite de espacio:** por defecto 2 GB en la computadora y 500 MB en el teléfono (se cambia en Preferencias); lo
    que más ocupa son las fotos y los análisis. Al llegar al límite no se borra nada solo: se avisa y la lista
    muestra cuánto ocupa cada entrada y cada foto o análisis, para que la persona elija qué borrar.

## Más adelante

13. **Sincronizar** entre el teléfono y la computadora se hace con la cuenta de Tisma Cloud, después de la 1.0
    (decisión del equipo de fichas): sin cuenta, la biblioteca es solo de ese equipo y se mueve con «Exportar».
14. **Publicar** una entrada la convierte en un modelo de `model_library.md`: el proyecto, las recetas como perfiles
    probados (el servidor lamina para cada una) y los análisis con su resumen. Las fotos y las notas solo si la persona
    las marca. La biblioteca local no manda nada a la red sin esa acción.

## Pruebas

| # | Caso | Resultado esperado |
|---|---|---|
| ML1 | Laminar un cubo, «Guardar en la biblioteca», cerrar, abrir la entrada e «Imprimir» con la misma impresora | Se lamina con la receta (en el equipo) y se manda; el G-code es igual al del primer laminado |
| ML2 | «Imprimir» la misma entrada eligiendo la nube | Se manda un trabajo `slice` con el proyecto y la receta; el resultado va a la impresora |
| ML3 | Entrada con un FEA; cambiar el relleno | «El análisis no vale para estos ajustes» |
| ML4 | Entrada con un FEA; cambiar la velocidad de viaje | El análisis sigue valiendo |
| ML5 | Exportar en la computadora (`.tismalib`) e importar en Android | Misma pieza, recetas y análisis; abrir e imprimir funciona igual |
| ML6 | Importar un `.tismalib` con una entrada `../x` o un ejecutable | Se rechaza sin escribir nada |
| ML7 | Biblioteca llena, guardar una entrada nueva | No se borra nada solo: aviso y lista con lo que ocupa cada cosa |
| ML8 | Actualizar el motor a una versión nueva | Las entradas avisan «Receta de una versión anterior» y laminan con el motor nuevo |
| ML10 | Buscar archivos G-code en `library/` después de imprimir | No hay ninguno |
| ML9 | Sin red ni cuenta | Todo lo anterior funciona igual |
