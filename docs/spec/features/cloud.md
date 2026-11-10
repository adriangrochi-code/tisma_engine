# Tisma Cloud: el contrato con el motor

**Qué hace:** el motor habla con Tisma Cloud, el servicio del dueño en internet, de tres maneras:
- como **cliente**: cuenta, laminar y calcular en la nube, informar problemas;
- como **puente**: deja usar desde afuera las impresoras de la red local;
- como **programa de los nodos**: los servidores de la nube corren el motor para laminar y calcular.

**Fase** (decisión del dueño, 02-11-2026: el laminado en la nube entra en la versión 1.0 de enero):
- **en la 1.0:** la cuenta (reglas 1 a 5) con los planes públicos a la vista, laminar y calcular en la nube (reglas 6
  a 10, 10b y 10c: también el análisis aerodinámico y la orientación automática) desde el escritorio y desde la app de Android, informar un problema (regla 11) y el motor en los nodos (la línea
  de comandos que llaman los servidores);
- **después de la 1.0:** el puente de impresoras, la biblioteca de modelos y Thingiverse por el servidor.

**Por qué es un contrato y no una ficha de funciones:** el servidor de Tisma Cloud es un programa aparte, del dueño,
con licencia propietaria, y no tiene código de PrusaSlicer ni de Tisma Slicer: corre los programas como procesos
aparte. No entra al motor. El motor solo habla con él por la red y por la línea de comandos, con lo que dice esta
ficha (`INTEGRATION.md`, camino 2). Los cambios del lado del servidor los hace el equipo del dueño.

**Fuente:** la API del servidor, versión `v1`, descrita por el equipo que la escribió.

## Reglas generales

1. **Dirección:** el servidor se configura en Preferences (`preferences.md`); por defecto, el de Tisma Cloud,
   `https://tismaservidores.net` (confirmado por el dueño, 02-11-2026); la persona puede escribir otro. Todo va por HTTPS; los pedidos y las respuestas son JSON, salvo los archivos.
2. **Sesión:** `POST /v1/auth/login` con `email`, `password` y `device` (el nombre del equipo) devuelve `token` y
   `email`.
   - Cada pedido lleva `Authorization: Bearer <token>`.
   - Un 401 quiere decir sesión vencida o cerrada: la interfaz pide entrar de nuevo.
   - `POST /v1/auth/logout` cierra la sesión.
3. **El token es un secreto:** se guarda como una contraseña (el almacén de claves del sistema), nunca en
   `interface.json`, en registros ni en mensajes.
4. **Errores:** el servidor manda un código HTTP y un texto en `detail`. La interfaz muestra ese texto. Hay tres casos
   con su propio aviso:
   - 413, archivo demasiado grande;
   - 422, parámetros mal;
   - 429, demasiados pedidos.

## La cuenta

| Pedido | Para qué | Datos |
|---|---|---|
| `POST /v1/auth/register` | Crear la cuenta | `email`, `password` (8 o más caracteres), `plan` opcional |
| `POST /v1/auth/verify` | Confirmar el correo con el código de 6 cifras | `email`, `code`, `device` |
| `POST /v1/auth/resend` | Volver a mandar el código | `email` |
| `POST /v1/auth/password/forgot`, `POST /v1/auth/password/reset` | Recuperar la contraseña | `email`; después `email`, `code` y la contraseña nueva |
| `POST /v1/auth/google` | Entrar con Google | El token de Google |
| `GET /v1/me` | Datos de la cuenta, plan y horas usadas | — |
| `GET /v1/plans` | Planes públicos | — |
| `GET /v1/me/sessions`, `DELETE /v1/me/sessions/{id}`, `POST /v1/me/sessions/close-others` | Ver y cerrar sesiones | — |

5. **El cobro no pasa por el motor:** pagar y cambiar de plan se hace en la página de la cuenta, en el navegador. El
   motor abre esa página; no maneja datos de pago.

5a. **Planes a la vista:** el panel de la cuenta muestra los planes de `GET /v1/plans` (no hace falta sesión): `name`,
    `monthly_hours`, `max_concurrent`, `price_cents` y `currency`, tal como los manda el servidor, con el plan actual
    marcado, y `classes`: los cálculos que incluye cada plan (regla 10b). El motor no guarda ni inventa planes: la administración los cambia en el servidor y la lista se pide de nuevo al abrir el
    panel. Las horas del mes (`GET /v1/me`) ya incluyen las que la administración dio o quitó a la cuenta; el motor
    muestra el número que recibe, sin sumar nada.

## Laminar y calcular en la nube

6. **Mandar un trabajo:** `POST /v1/jobs`, multipart, con:
   - `kind`: `slice`, `fea`, `aero` u `orient`;
   - `file`: el proyecto como paquete 3MF (un ZIP con `3D/*.model`, `formats.md` regla 11); otro archivo da 422 «The
     project is not a 3MF file»;
   - `params`: JSON.
     - `slice`: vacío.
     - `fea`: `mode` (`analyze`, `lightest-infill`, `resin-structure`), `quality` (`fast`, `normal`, `high`,
       `ultra`), `object`, `instance`, y opcionales `solid`, `zones`, `ribs`.
     - `aero` (análisis aerodinámico, `aerodynamics.md`): `object`, `instance`, `quality` (`friction`, `fast`,
       `normal`, `high`: solo rozamiento, 16, 24 o 32 celdas en el tamaño frontal), `speed` (m/s, más de 0 y hasta
       100) y `direction` (`x`, `-x`, `y`, `-y`, `z`, `-z`, o la dirección de la vista como tres números).
     - `orient` (orientación automática, `orientation.md`): `object` e `instance`; el método y la precisión salen de
       los ajustes `orient.*` del proyecto.

   Devuelve el trabajo (regla 7).
7. **Un trabajo:** `GET /v1/jobs/{id}` devuelve:
   - `id`, `kind`, `job_class`, `params`;
   - `status`: `queued`, `running`, `done`, `failed`, `cancelled`;
   - `progress` (0 a 100), `seconds` (lo que corrió, que es lo que se cobra del plan) y `error`;
   - `summary`: en un laminado, tiempo de impresión y filamento; en un análisis, el veredicto;
   - `result_filename`, y las fechas `created_at`, `started_at` y `finished_at`.
8. **Seguir el trabajo:** la interfaz pregunta cada 2 s mientras está en cola o corriendo y muestra el avance en la
   barra de estado. `POST /v1/jobs/{id}/cancel` lo cancela.
9. **Bajar el resultado:** `GET /v1/jobs/{id}/result` devuelve el archivo:
   - un laminado da G-code de texto o binario, que se abre en la vista previa como un laminado local;
   - un análisis da su resultado, que se abre en Ingeniería (`structural_analysis.md`).
10. **Lista:** `GET /v1/jobs?limit=N` (50 por defecto, hasta 200) devuelve `{"jobs": [...]}`, del más nuevo al más
    viejo; cada trabajo es el mismo objeto de la regla 7.
10b. **Lo pesado, en la nube y cobrado por plan** (decisión del dueño, 02-11-2026):
    - cada cálculo pesado tiene, al lado de su botón, la opción **«En la nube»**: laminar, análisis estructural,
      relleno mínimo, análisis aerodinámico y orientación automática. Con sesión iniciada y el cálculo incluido en el
      plan, la opción está prendida por defecto en el teléfono y apagada en la computadora (`cloud.prefer`);
    - lo que incluye el plan está en `classes` de `GET /v1/plans` (los nombres de cola: `slice`, `slice_large`,
      `fea`, `fea_optimize`, `aero`, `orient`). Un cálculo que el plan no incluye se muestra con un candado y «Incluido
      en el plan …», con los planes que lo tienen, y abre la página de la cuenta; si igual se manda, el servidor
      contesta 402 con ese mismo texto en `detail` (regla 4);
    - sin sesión o sin conexión, todo se sigue calculando en la computadora como siempre: la nube nunca es
      obligatoria en el escritorio.
10c. **Resultados de los cálculos nuevos:** `aero` da un archivo `.taero` del motor (el resultado de
    `aerodynamics.md`: arrastre, coeficientes, Reynolds, rugosidad y el mapa por cara), que el motor abre en Ingeniería
    como un análisis local; `orient` da un JSON con `rotation` (matriz de 3 × 3, por filas), `cost` y el costo de la
    orientación que tenía, y el motor gira la pieza con eso (se deshace como cualquier giro).

## Informar un problema

11. `POST /v1/reports` con:
    - `app`: `pc`, `android` o `web`;
    - `kind`: `bug`, `crash` o `suggestion`;
    - `title` (3 caracteres o más), `description`, `steps`, `expected` y `actual`.

    Si la persona lo acepta, también la versión, el sistema, la impresora y las últimas 500 líneas del registro
    (`interface.md`, «Report a problem»). Se puede mandar sin cuenta. `GET /v1/me/reports` devuelve `{"reports": [...]}`,
    los propios del más nuevo al más viejo (hasta 100), con `id`, `kind`, `title`, `status` (`new`, `confirmed`, `fixed`,
    `wontfix`, `duplicate`), `category_name`, `created_at`, `updated_at` y `rewarded_at`; no trae la versión que lo
    arregló. La pantalla va en `problem_reports.md`.

## El motor como puente de impresoras

Deja usar desde afuera (la app, la página) las impresoras conectadas a esta computadora, sin abrir puertos.

12. **Encendido:** solo si la persona lo activa, con su sesión iniciada. Cada instalación es un puente con su propio
    identificador (letras, números, `-` y `_`, hasta 64).
13. **Estado:** `PUT /v1/bridges/{id}` cada pocos segundos mientras alguien mira, y más espaciado si nadie mira.
    - Lleva `name` y `printers`, hasta 32.
    - Cada impresora lleva `id`, `name`, `kind`, `camera` (sí o no), `actions` (entre `print`, `pause`, `resume` y
      `stop`) y `state`.
    - `state` trae `state` y `job_name` como texto, y `percent`, `remaining_min`, `nozzle_temp`, `nozzle_target`,
      `bed_temp`, `bed_target`, `layer` y `total_layers` como números.
    - Un campo que no está en esta lista no se manda.
14. **Órdenes:** `GET /v1/bridges/{id}/commands` trae las órdenes pendientes.
    - Para imprimir, el archivo se baja con `GET /v1/bridges/{id}/commands/{orden}/file` (`.gcode` o `.bgcode`).
    - El puente hace la orden con la conexión de esa impresora (`printer_connections.md`) y contesta con
      `POST /v1/bridges/{id}/commands/{orden}`, `ok` y, si falló, `error`.
15. **Cámara:** cuando alguien la mira, el puente sube una imagen JPEG por vez con
    `POST /v1/bridges/{id}/printers/{impresora}/camera`. Nunca sube imágenes si nadie mira.
16. **Límites:** hasta 10 puentes por cuenta. Cuántas impresoras se pueden usar desde afuera depende del plan; el
    servidor lo decide y el puente no lo controla.
17. **Seguridad:**
    - el puente nunca manda códigos de acceso ni contraseñas de las impresoras;
    - solo hace órdenes de la lista de la regla 13, y nunca G-code suelto que llegue de la nube.

## El motor en los nodos de la nube

Los nodos corren el motor como programa aparte para cada trabajo. El contrato es la línea de comandos.

18. **Laminar un proyecto:**

    ```text
    tisma-engine slice <proyecto.3mf> -o <salida.gcode> --summary <resumen.json> --sandbox [--no-gpu]
    ```

    - lamina el proyecto 3MF completo (piezas, ajustes por objeto, perfiles y cama elegida), sin interfaz y sin leer
      la carpeta de datos del usuario. También acepta proyectos de otros laminadores (`formats.md`, regla 18);
    - el G-code sale de texto o binario, como diga el perfil. El nodo reconoce el binario por sus primeros bytes;
    - por la salida estándar, una línea por cada cambio de avance: `progress <0 a 100>`;
    - `<resumen.json>` lleva `print_time` (texto, por ejemplo «1h 2m 3s»), `print_time_s`, `filament_mm`, `filament_g`
      y `layers`;
    - si falla: código de salida distinto de 0 y `error` en el resumen, con el texto para la persona; si no puede
      escribir el resumen, el error va a la salida de errores.
19. **Analizar:**

    ```text
    tisma-engine analyze <proyecto.3mf> -o <resultado.tea> --summary <resumen.json> --sandbox --mode <modo>
      [--quality <calidad>] --object <n> --instance <n> [--solid] [--no-zones] [--ribs] [--no-gpu] [--threads <n>]
    ```

    - los apoyos, las cargas y el material van en el proyecto (`formats.md`); los parámetros son los de la regla 6;
    - sin `--quality`, la calidad es la que guardó el proyecto: así la nube calcula lo mismo que la computadora
      (`formats.md`, prueba FM25). El servidor pasa `--quality` solo si la persona eligió otra al mandar el trabajo;
      `fast`, `normal`, `high` y `extreme` (`ultra` es el nombre viejo de `extreme`);
    - el resultado `.tea` es un archivo del motor, que el motor sabe abrir en Ingeniería;
    - el resumen lleva el veredicto del análisis (`structural_analysis.md`) o `error`. Avance y códigos de salida, como
      en la regla 18.
19a. **Errores que ve la persona:** cuando un cálculo falla, el resumen trae, además de `error` (en inglés), un `code`
    estable (`bad_mode`, `bad_quality`, `not_in_cloud`, `needs_conditions`, `no_valid_result`, `usage`, `error`) y,
    para los errores que puede encontrar una persona, `error_es` con el texto en español. El servidor los guarda en el
    `summary` del trabajo fallido (`GET /v1/jobs/<id>`). La interfaz muestra `error_es` si está en español y existe;
    si no, traduce por `code`; si no, el texto de `error`. Vale para `analyze`; `slice`, `aero` y `orient` van a usar los
    mismos campos.
19b. **Análisis aerodinámico:**

    ```text
    tisma-engine aero <proyecto.3mf> -o <resultado.taero> --summary <resumen.json> --sandbox --object <n>
      --instance <n> --quality <calidad> --speed <m/s> --direction <eje | x,y,z> [--threads <n>] [--no-gpu]
    ```

    - la pieza, su altura de capa y su material salen del proyecto; los parámetros son los de la regla 6;
    - el resumen lleva `drag_n`, `cd`, `cd_pressure`, `cd_friction`, `reynolds` y `reynolds_simulated`, o `error`.
      Avance y códigos de salida, como en la regla 18.
19c. **Orientación automática:**

    ```text
    tisma-engine orient <proyecto.3mf> -o <orientacion.json> --summary <resumen.json> --sandbox --object <n>
      --instance <n> [--threads <n>] [--no-gpu]
    ```

    - escribe el JSON de la regla 10c; el resumen lleva `cost`, `cost_before` y `candidates` (cuántas probó), o
      `error`.
19d. **`--threads <n>`** (en `slice`, `analyze`, `aero` y `orient`): el motor usa como mucho `n` hilos de cálculo. El
    nodo lo pasa con los núcleos de procesador que el plan de la persona reserva para cada trabajo; así un nodo reparte
    sus núcleos entre varios trabajos a la vez sin que uno tome todo. Sin la opción, usa todos.
20. **`--sandbox`:** no corre scripts de posproceso (`gcode_templates.md`, regla 16) ni complementos
    (`plugins.md`), no abre conexiones de red y no lee ni escribe fuera de las rutas que recibe. El tiempo máximo lo
    pone el nodo, que corta el proceso.
21. **El lado del servidor ya está hecho:** con `tisma_engine` en la configuración de un nodo, el nodo corre los
    trabajos con este contrato. Sin esa opción, sigue con los programas de Tisma Slicer.

## Biblioteca de modelos

23. La biblioteca de Tisma Cloud (modelos con perfiles probados, laminados hechos y análisis, e «Imprimir» en dos
    toques) va en `model_library.md`. Usa los trabajos, los puentes y los planes de esta ficha.

## Thingiverse por el servidor

22. **Paso a la API de Thingiverse:** `GET /v1/thingiverse/<ruta>`, con o sin sesión. El servidor agrega el token de
    la aplicación de Tisma y devuelve la respuesta de Thingiverse sin cambios. Las rutas permitidas, los límites y lo
    que no hace están en `thingiverse.md`, reglas 12 y 21. Falta programarlo del lado del servidor.

## Pruebas

Contra un servidor de prueba que el equipo del motor arma desde esta ficha.

| # | Escenario | Resultado esperado |
|---|---|---|
| CC1 | Entrar con correo y contraseña buenos | Sesión abierta; el token queda en el almacén de claves, no en `interface.json` |
| CC2 | Pedido con un token vencido | 401; la interfaz pide entrar de nuevo |
| CC3 | Laminar en la nube un proyecto de un cubo | El trabajo pasa por `queued` y `running` a `done`; el G-code se abre en la vista previa |
| CC4 | Cancelar un trabajo corriendo | Estado `cancelled`; la barra de estado lo dice |
| CC5 | Proyecto más grande que el límite | Aviso de archivo demasiado grande (413) |
| CC6 | Informar un problema sin cuenta | Se manda; sin el registro si la persona no lo aceptó |
| CC7 | Puente con una impresora USB simulada: orden `pause` | La impresora recibe la pausa; el puente contesta `ok` |
| CC8 | Orden con una acción que no está en la lista | El puente la rechaza y contesta con el error |
| CC9 | Nadie mira la cámara | El puente no sube imágenes |
| CC10 | `tisma-engine slice` de un 3MF con `--sandbox` | Líneas `progress`, G-code y resumen JSON con las cinco claves; código 0 |
| CC11 | El mismo proyecto con un script de posproceso en sus ajustes | El script no corre |
| CC12 | Proyecto con una pieza fuera de la cama | Código distinto de 0 y `error` en el resumen |
| CC13 | Abrir el panel de la cuenta sin sesión | Se ven los planes públicos que manda el servidor; un plan oculto no aparece |
| CC14 | La administración da 3 horas a una cuenta de un plan de 5 | El panel muestra 8 horas del mes al volver a pedir `/v1/me` |
| CC15 | La administración deshabilita la cuenta con la sesión abierta | El próximo pedido da 401; la interfaz pide entrar de nuevo |
| CC16 | Plan que incluye solo `slice`; pedir un análisis aerodinámico | Candado e «Incluido en el plan …» antes de mandar; si se manda, 402 con el texto del servidor |
| CC17 | `tisma-engine aero` de un cubo de 20 mm, `--quality fast --speed 10 --direction x --sandbox` | Archivo `.taero`, resumen con `cd` entre 0,8 y 1,3 (un cubo de cara al aire, Hoerner) y código 0 |
| CC18 | `tisma-engine orient` de una pieza en L apoyada de canto | JSON con `rotation`; `cost` no mayor que `cost_before` (regla 1 de `orientation.md`) |
| CC19 | `tisma-engine slice … --threads 1` en una máquina de 4 núcleos | El proceso no pasa de un núcleo ocupado; el G-code es el mismo que sin la opción |
| CC20 | Un trabajo `fea` con modo `lightest-infill` (que todavía no corre en la nube) | Falla; `GET /v1/jobs/<id>` da `error` en inglés y en `summary` `code` = `not_in_cloud` y `error_es`; la interfaz en español muestra `error_es` |
