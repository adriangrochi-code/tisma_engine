# Asistente sin conexión y tutoriales

**Qué hace:**
- un panel de ayuda que funciona sin red: problemas de impresión con sus causas y arreglos, procedimientos paso a
  paso, un glosario y fichas de materiales;
- tutoriales guiados la primera vez que se usa cada parte;
- cambiar un ajuste desde la ayuda, con confirmación y deshacer.

**Fase:** P1 (interfaz). Pedido del dueño (26-10-2026): «crear un asistente tutorial y datos de info» a partir de las
guías que entregó.

**Datos:**
- vienen en `knowledge.tar.xz` del paquete libre (`profile_pack.md`, regla 21);
- su formato está en `docs/KNOWLEDGE.md` del repositorio de perfiles;
- son texto propio escrito a partir de las guías de Rafa Barajas (`https://www.rafabarajas.com`): problemas,
  materiales, glosario y tablas de parámetros y secado.

**Diseño propio:** ningún laminador tiene este asistente; las reglas son de esta ficha.

## Ajustes

Preferencias de la interfaz:

| Clave | Por defecto | Qué hace |
|---|---|---|
| `assistant.tutorials` | sí | Ofrecer los tutoriales la primera vez que se usa cada parte |
| `assistant.glossary_tips` | sí | Mostrar la definición del glosario junto a cada ajuste que tenga una |
| `assistant.material_tips` | sí | Mostrar la ficha y los avisos del material al elegir un filamento |

## Reglas

### Panel de ayuda

1. **Dónde está:** un botón de ayuda en la barra superior y la tecla F1. Se abre a la derecha, sobre el panel de
   ajustes.
2. **Pestañas:** Problemas, Procedimientos, Glosario, Tutoriales. Un buscador arriba busca en todas, sin importar
   mayúsculas ni acentos.
3. **Sin datos:** si `knowledge.tar.xz` todavía no se bajó, el panel dice «La ayuda se descarga con los perfiles; se
   va a bajar cuando haya conexión».
4. **Fuente:** al pie del panel, «Basado en las guías de Rafa Barajas» con el enlace a `https://www.rafabarajas.com`.

### Problemas

5. **Lista:** los problemas por nombre, con el síntoma debajo.
6. **Un problema:** el síntoma y sus causas en el orden del archivo (la más común primero).
   - Cada causa muestra su explicación, la pregunta para confirmarla (`check`) y sus acciones.
   - Una causa con `last_resort` va al final, con la nota «Probar solo después de las anteriores».
7. **Acción `setting`:**
   - el botón **Aplicar** muestra el ajuste, el valor actual y el nuevo (`add` suma, `multiply` multiplica, `set`
     fija, `set_relative` fija `value` veces el ajuste `of`);
   - al confirmar, cambia el perfil al que pertenece la clave (impresora, filamento o impresión, `docs/PROFILES.md`)
     sin guardarlo: el perfil queda con su asterisco de cambios;
   - un valor fuera del rango de `docs/CONFIG.md` se acota al rango, con el aviso «Se usó el límite: …»;
   - las temperaturas (`machine.variables.*`) se suman como número sobre el valor de la variable;
   - **Deshacer** vuelve al valor anterior mientras el panel siga abierto, y el cambio entra en el deshacer general
     de la interfaz;
   - una clave que el motor no conoce no muestra el botón.
8. **Otras acciones:**
   - `calibration` abre esa prueba de calibración (`calibrations.md`) con la impresora y el filamento elegidos;
   - `procedure` abre el procedimiento;
   - `problem` lleva al otro problema, con un botón **Volver**;
   - `hardware` y `model` muestran el consejo, sin botón.
9. **Desde la revisión previa:** cada aviso de `print_review.md` con un problema relacionado tiene un enlace
   «¿Cómo lo arreglo?» a ese problema:

| Aviso de la revisión | Problema |
|---|---|
| Hilos probables | `stringing` |
| Esquinas que pueden levantarse | `warping` |
| Partes chicas sin tiempo de enfriarse | `overheating_small` |
| Primera capa con poco apoyo | `first_layer_adhesion` |
| Voladizos sin soporte | `support_surface` |

### Procedimientos

10. **Paso a paso:**
    - cada paso con su título, su texto y una casilla para marcarlo hecho;
    - las advertencias (`warnings`) van arriba, resaltadas;
    - las notas y la prevención, al final.

### Glosario

11. **En el panel:** los términos por orden alfabético, con su definición.
12. **Junto a los ajustes:** con `assistant.glossary_tips`, cada ajuste nombrado en el `settings` de un término
    muestra un ícono de información; al pasar el puntero, la definición. Si varios términos nombran el mismo ajuste,
    se muestra el primero en orden alfabético.

### Materiales

13. **Ficha:** al elegir un filamento, con `assistant.material_tips`, se busca su material:
    - primero por `material.type` del perfil del paquete;
    - si no hay, por el nombre del perfil (`PLA`, `PETG`…), sin importar mayúsculas;
    - si no se encuentra, no se muestra nada.

    La ficha muestra temperaturas, cama, cabina, ventilador, boquilla, superficies de cama, retracción (directa o
    Bowden según `extruder` de la impresora, si se sabe) y secado.
14. **Avisos del material**, en la ficha y en la barra de estado. Un aviso cuya condición depende de un dato que la
    impresora no tiene (0 o vacío) no se muestra.

| Condición | Aviso |
|---|---|
| El material pide cabina (`enclosure` necesaria) y la impresora no es cerrada (`machine.enclosed` no) | «Este material necesita una impresora cerrada» |
| `abrasive` y sin `machine.hardened_nozzle` | «Este material gasta las boquillas de latón: usá una de acero endurecido» |
| La cama mínima del material pasa `machine.max_bed_temp` | «Tu cama no llega a la temperatura de este material» |
| La boquilla mínima del material pasa `machine.max_nozzle_temp` | «Tu hotend no llega a la temperatura de este material» |
| `drying.before_printing` es «si» | «Conviene secarlo antes de imprimir: X a Y °C, N h», con enlace al procedimiento |

### Tutoriales

15. **Cuándo se ofrecen:**
    - `first_start`: al terminar el asistente de configuración del primer arranque;
    - `new_filament`: la primera vez que se elige un tipo de material;
    - `first_open:<parte>`: la primera vez que se abre esa parte (por ejemplo, `engineering`);
    - `on_request`: solo desde la pestaña Tutoriales.

    Se ofrecen con una tarjeta («¿Te muestro cómo?») que se puede aceptar, posponer o descartar. Uno descartado no
    vuelve a aparecer solo.
16. **Cómo se ven:**
    - el paso resalta la parte de la interfaz de su `anchor` y oscurece el resto;
    - una burbuja con el título y el texto, y los botones Atrás, Siguiente y Salir;
    - un paso con `wait_for` avanza solo cuando pasa ese evento, y también deja seguir con Siguiente;
    - un paso con `open` tiene un botón que abre esa acción.
17. **Anclas y eventos:** el motor da estos nombres a partes de su interfaz (`interface.md`). Un ancla que la
    interfaz no tenga en ese momento (por ejemplo, un panel cerrado) abre ese panel; si no existe, la burbuja se
    muestra en el centro.

| Ancla | Parte |
|---|---|
| `printer_selector`, `filament_selector` | Las listas de impresora y filamento del panel de la placa |
| `plate`, `plate_tools` | La placa y sus herramientas |
| `quick_settings` | Los ajustes rápidos (calidad, relleno, soportes) |
| `slice_button`, `send_button` | El botón principal en Laminar y en Enviar |
| `preview` | La vista previa |
| `calibrations` | La página de calibraciones |
| `help_problems`, `help_procedures` | Las pestañas del panel de ayuda |
| `engineering_panel`, `engineering_faces`, `engineering_loads`, `engineering_run`, `engineering_apply` | El panel de Ingeniería y sus pasos |

| Evento | Cuándo |
|---|---|
| `printer_selected`, `filament_selected` | Se eligió una impresora o un filamento |
| `model_added` | Hay al menos un modelo en la placa |
| `sliced` | Terminó un laminado sin error |
| `fixed_face_added`, `load_added`, `analysis_done` | En Ingeniería: se marcó una cara fija, se agregó una carga, terminó el cálculo |

18. **Progreso:** la interfaz recuerda qué tutoriales se vieron y en qué paso quedó cada uno (`interface.json`,
    `docs/UI.md`). Retomar uno empieza en ese paso.
19. **Idioma:** se usan los archivos cuyo `language` coincide con el de la interfaz; si no hay, los de `es`.

## Pruebas

Con un `knowledge.tar.xz` armado por el equipo del motor: dos materiales, tres problemas (uno con `last_resort`), un
procedimiento, cinco términos y un tutorial de tres pasos.

| # | Escenario | Resultado esperado |
|---|---|---|
| AS1 | Sin `knowledge.tar.xz` | El panel muestra el aviso de descarga; el resto de la interfaz funciona |
| AS2 | Buscar «hilos» | Aparece el problema de hilos y el término del glosario |
| AS3 | Problema con una causa `last_resort` | Esa causa va última, con su nota |
| AS4 | Aplicar `cooling.fan_off_layers` `set` 4 con el perfil en 1 | Diálogo «1 → 4»; al confirmar, el perfil de filamento queda en 4 y con asterisco; Deshacer lo vuelve a 1 |
| AS5 | Aplicar `machine.variables.temperature` `add` −5 con la variable en «215» | Queda «210» |
| AS6 | Aplicar `speed.max` `multiply` 0,8 con 80 | Queda 64 |
| AS7 | Aplicar `layers.height` `set_relative` 0,75 de `machine.nozzle_diameter` con boquilla de 0,4 | Queda 0,3 |
| AS8 | Aplicar un valor fuera de rango | Se usa el límite y aparece el aviso |
| AS9 | Elegir un filamento ABS en una impresora con `machine.enclosed` no | Ficha del ABS y aviso de impresora cerrada |
| AS10 | Material abrasivo sin `machine.hardened_nozzle` | Aviso de boquilla de acero endurecido |
| AS11 | Material con `drying.before_printing` «si» | Aviso de secado con temperatura y horas, y enlace al procedimiento |
| AS12 | Primer arranque terminado | Tarjeta del tutorial `first_print`; al aceptar, el primer paso resalta la lista de impresoras |
| AS13 | Paso con `wait_for` `model_added`: agregar un modelo | El tutorial pasa solo al siguiente paso |
| AS14 | Cerrar la interfaz en el paso 2 y volver a abrirla | Al retomar el tutorial empieza en el paso 2 |
| AS15 | Ajuste `walls.count` nombrado por un término | El ícono de información aparece junto al ajuste con la definición |
