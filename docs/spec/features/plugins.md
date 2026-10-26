# Complementos (plugins)

**Qué hace:** deja que otras personas agreguen funciones al programa sin tocar el motor, por ejemplo:
- piezas o soportes nuevos antes de laminar;
- cambios al G-code;
- avisos en la revisión previa;
- botones con acciones propias.

**Fase:** después de la primera versión.

**Pedido del dueño (26-10-2026):** «hay que agregar soporte para plug-ins, que debe ser fácil de usar y fácil de
implementar».

**Diseño:** propio del motor. No se leyó el código de ningún sistema de complementos de otro laminador.

**Objetivos:**
- **Fácil de usar:** instalar es soltar un archivo en la ventana. Los ajustes del complemento aparecen en el panel como
  los demás, y si falla, la impresión sigue.
- **Fácil de implementar:**
  - un complemento es un programa en cualquier lenguaje, más un archivo JSON que lo describe;
  - habla con el motor con una línea de JSON de entrada y una de salida;
  - no hace falta compilar nada contra el motor, ni dibujar ventanas.

## El paquete

1. **Forma:** una carpeta, o un archivo `.tismaplugin` (un ZIP de esa carpeta). Adentro:
   - `plugin.json`, la descripción;
   - el programa (un script o un ejecutable por sistema);
   - opcional: `icon.svg`, `README.md` y las traducciones.
2. **`plugin.json`:**

```json
{
  "format": "tisma-plugin",
  "version": 1,
  "id": "com.ejemplo.marcas-de-agua",
  "name": {"es": "Marcas de agua", "en": "Watermarks"},
  "plugin_version": "1.2.0",
  "author": "Nombre de quien lo hizo",
  "license": "MIT",
  "url": "https://ejemplo.com/marcas",
  "based_on": [{"name": "Proyecto de origen", "author": "Su autor", "license": "MIT", "url": "https://ejemplo.com"}],
  "description": {"es": "Graba un texto en la base de la pieza.", "en": "Engraves a text on the part's base."},
  "api": 1,
  "run": {"windows": ["marcas.exe"], "linux": ["node", "marcas.js"], "macos": ["node", "marcas.js"]},
  "hooks": ["mesh"],
  "timeout_s": 60,
  "permissions": {"network": false},
  "settings": [
    {"key": "text", "type": "texto", "default": "TISMA", "label": {"es": "Texto", "en": "Text"}, "level": "basic"},
    {"key": "depth", "type": "numero", "unit": "mm", "min": 0.2, "max": 2, "step": 0.1, "default": 0.4,
     "label": {"es": "Profundidad", "en": "Depth"}, "level": "normal"}
  ],
  "section": {"tab": "print", "section": "other", "name": {"es": "Marcas de agua", "en": "Watermarks"}}
}
```

| Campo | Obligatorio | Qué es |
|---|---|---|
| `format`, `version` | sí | Siempre `tisma-plugin` y `1` |
| `id` | sí | Único, en minúsculas, con puntos y guiones; no cambia entre versiones |
| `name`, `description` | sí | Por idioma; si falta el de la interfaz, se usa `en`, y si no, el primero |
| `plugin_version` | sí | Versión del complemento (`mayor.menor.parche`) |
| `author`, `license`, `url` | `author` y `license` sí | Se muestran al instalar y en Créditos y fuentes (`credits.md`) |
| `based_on` | no | Las fuentes en las que se basa; van a Créditos y fuentes como cualquier función (`credits.md`) |
| `api` | sí | Versión de esta ficha que usa; el motor rechaza una que no conoce, con el aviso «Este complemento pide una versión más nueva del programa» |
| `run` | sí | El comando por sistema, relativo a la carpeta del complemento. Un sistema sin comando no lo ofrece |
| `hooks` | sí | En qué momentos corre (regla 6) |
| `timeout_s` | no | Segundos que puede tardar cada llamada; 60 por defecto, hasta 600 |
| `permissions` | no | Lo que dice que necesita (regla 13) |
| `settings`, `section` | no | Sus ajustes y dónde van en el panel (regla 9) |

3. **Kit de inicio:** el proyecto publica, con licencia MIT para que cualquiera pueda usarlo, un kit chico para Python
   y otro para JavaScript (Node). Cada uno lee la entrada, avisa el progreso y escribe la salida. Más ›
   Complementos › «Crear complemento…» arma una carpeta con un `plugin.json` y un ejemplo que funciona en el lenguaje
   elegido.

## Cómo habla con el motor

4. **Una llamada:**
   - el motor arranca el programa en una carpeta temporal propia de esa llamada, con los archivos de entrada;
   - le escribe en la entrada estándar **una línea** de JSON con el pedido y cierra la entrada;
   - el programa puede escribir líneas de progreso y termina con **una línea** de resultado en la salida estándar;
   - todo lo que escriba en la salida de errores va al registro del motor;
   - código de salida 0 es éxito; cualquier otro, error.
5. **Mensajes:**

```json
{"api": 1, "hook": "mesh", "language": "es", "units": "mm", "settings": {"text": "TISMA", "depth": 0.4}, "profile": {"layers.height": 0.2, "machine.nozzle_diameter": 0.4}, "objects": [{"id": 3, "name": "soporte.stl", "mesh": "in/3.stl", "transform": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]}], "out": "out/"}
```

```json
{"progress": 0.4, "message": "Grabando el texto"}
```

```json
{"result": {"add": [{"object": 3, "mesh": "out/3_texto.stl", "role": "negative_volume"}], "warnings": [{"message": "El texto es más largo que la base", "object": 3}]}}
```

   - `profile` trae los ajustes del motor con sus nombres (`docs/CONFIG.md`), ya resueltos para ese objeto;
   - las mallas van como STL binario en coordenadas de la cama, en mm;
   - una ruta de la respuesta fuera de `out/` se rechaza.

6. **Momentos (`hooks`)**, en el orden en que corren:

| Momento | Recibe | Puede devolver |
|---|---|---|
| `mesh` | Cada objeto: malla, transformación, ajustes | Partes, volúmenes negativos, modificadores, bloqueadores o forzadores de soporte que se suman al objeto (`role`); o una malla que reemplaza a la del objeto |
| `layers` | Las regiones de cada capa como polígonos (JSON, en mm), con su tipo | Regiones nuevas con un tipo (soporte, relleno, pared…) que el motor rellena con sus trayectorias |
| `review` | El resultado del laminado: capas, tiempos, material | Avisos para la revisión previa (`print_review.md`), con capa y objeto |
| `gcode` | La ruta del G-code terminado | El mismo archivo cambiado, o uno nuevo en `out/` |
| `action` | Lo elegido en la placa (objetos, caras) cuando la persona toca el botón del complemento | Lo mismo que `mesh`, o un mensaje |

   - Un complemento declara solo los momentos que usa.
   - Varios complementos en el mismo momento corren en el orden de la lista de complementos (regla 11).
   - `layers` es la API más rica y la más difícil: su forma exacta la fija el motor cuando programe esta ficha, con el
     mismo estilo de mensajes.
7. **Acciones** (`action`): el complemento declara sus botones en `plugin.json` (`actions`, cada una con `id`, nombre
   e ícono). Aparecen en el menú de clic derecho de la pieza y en Más › Complementos. Ejemplo: «Agregar aleta aquí».
8. **Los scripts de posproceso de siempre** (`gcode_templates.md`, regla 16) siguen funcionando igual, sin
   `plugin.json`. Un complemento con `gcode` es lo mismo, más ajustes, progreso y errores claros.

## En la interfaz

9. **Ajustes del complemento:** el motor los dibuja, el complemento no dibuja nada.
   - Tipos: `booleano`, `entero`, `numero` (con unidad, mínimo, máximo y paso), `lista` (opciones con su nombre) y
     `texto`.
   - Cada uno con su nivel (`basic`, `normal`, `advanced`) para los niveles y la vista propia (`settings_panel.md`,
     PR #19).
   - Van en un apartado propio con el nombre del complemento, dentro de la sección que pide `section`, o en Otros.
   - Se guardan en el perfil y en el proyecto como `plugins.<id>.<clave>`.
   - Un perfil con ajustes de un complemento que no está instalado los conserva sin usarlos y lo avisa al abrirlo.
10. **Instalar:**
    - soltar el `.tismaplugin` o la carpeta en la ventana, o usar Más › Complementos › «Instalar…»;
    - un diálogo muestra el nombre, el autor, la licencia, la versión, los momentos en que corre, lo que pide
      (regla 13) y el aviso «Un complemento es un programa que corre en tu computadora con tus permisos. Instalá
      solo los que vengan de alguien en quien confíes»;
    - Instalar y Cancelar. Se copia a la carpeta de complementos del usuario, junto a `interface.json`
      (`docs/UI.md`);
    - instalar otra versión del mismo `id` la reemplaza, con la versión vieja y la nueva en el diálogo.
11. **Página Complementos** (Más › Complementos): la lista con ícono, nombre, versión y autor, cada uno con:
    - un interruptor para activarlo o desactivarlo;
    - flechas para el orden;
    - «Desinstalar» y «Abrir carpeta»;
    - su `README.md`, si trae;
    - el último error, si hubo.
12. **Si falla** (código de salida distinto de 0, respuesta que no es JSON válido, ruta fuera de `out/`, o pasa
    `timeout_s`):
    - el motor lo detiene, sigue el laminado sin ese paso y muestra una notificación con el nombre del complemento,
      el error y las últimas 20 líneas de su salida de errores;
    - en la revisión previa queda el aviso «El complemento X no corrió»;
    - la persona puede marcar un complemento como «Necesario para laminar» en su fila: si falla, el laminado se
      detiene en lugar de seguir sin él.
    Cancelar el laminado detiene al complemento.
13. **Permisos y seguridad:**
    - el complemento recibe solo los archivos de su llamada, en su carpeta temporal, que se borra al terminar;
    - `permissions.network` dice si usa la red; se muestra al instalar;
    - el motor no puede impedir lo que haga un programa de la computadora: por eso el aviso de la regla 10, y nunca se
      instala nada sin que la persona lo confirme;
    - los complementos no ven los secretos del motor (códigos de acceso, cuenta de la nube, `printer_connections.md`).
14. **Línea de comandos:**
    - `tisma-engine` laminando corre los complementos activos, igual que la interfaz;
    - `--no-plugins` los apaga;
    - `--plugin-test <carpeta> <modelo>` corre cada momento del complemento con ese modelo y revisa sus respuestas
      contra esta ficha, para quien lo está programando.
15. **Licencia:** decisión del dueño (26-10-2026): cada complemento elige su licencia. La licencia del motor no
    alcanza a los complementos: son programas aparte que intercambian archivos con él. El kit de inicio es MIT para
    no imponer nada. Esto se anota en el `README.md` del motor, en la parte de la licencia.

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| PL1 | Soltar un `.tismaplugin` válido | Diálogo con nombre, autor, licencia, momentos y aviso; al instalar aparece en la lista, activo |
| PL2 | `plugin.json` sin `id` | No se instala: «Al complemento le falta el campo id» |
| PL3 | `api` 2 | No se instala: «Este complemento pide una versión más nueva del programa» |
| PL4 | Complemento `mesh` que suma un volumen negativo | El G-code tiene el hueco; el ajuste `depth` aparece en su apartado, en Otros, en nivel Normal |
| PL5 | Complemento que tarda más que `timeout_s` | Se detiene; el laminado termina sin él; notificación y aviso en la revisión |
| PL6 | Lo mismo marcado «Necesario para laminar» | El laminado se detiene con el error |
| PL7 | Respuesta con una ruta fuera de `out/` | Se rechaza; error claro |
| PL8 | Dos complementos `gcode` en orden A, B | B recibe el G-code ya cambiado por A; al invertir el orden, al revés |
| PL9 | Abrir un proyecto con ajustes de un complemento no instalado | Se conservan; aviso con el nombre del complemento |
| PL10 | `--plugin-test` con un complemento que devuelve JSON mal formado | Informe con el momento y el error |
| PL11 | Script de posproceso común en `output.post_process` | Corre igual que antes |
| PL12 | Desactivar un complemento | No corre; sus ajustes se ocultan del panel |
| PL13 | Más › Complementos › «Crear complemento…» en Python | Carpeta con `plugin.json` y un ejemplo que pasa `--plugin-test` |
