# Tisma Cloud: el contrato con el motor

**Qué hace:** el motor habla con Tisma Cloud, el servicio del dueño en internet, de tres maneras:
- como **cliente**: cuenta, laminar y calcular en la nube, informar problemas;
- como **puente**: deja usar desde afuera las impresoras de la red local;
- como **programa de los nodos**: los servidores de la nube corren el motor para laminar y calcular.

**Fase:** después de la primera versión.

**Por qué es un contrato y no una ficha de funciones:** el servidor de Tisma Cloud es un programa aparte, del dueño,
con licencia propietaria, y no tiene código de PrusaSlicer ni de Tisma Slicer: corre los programas como procesos
aparte. No entra al motor. El motor solo habla con él por la red y por la línea de comandos, con lo que dice esta
ficha (`INTEGRATION.md`, camino 2). Los cambios del lado del servidor los hace el equipo del dueño.

**Fuente:** la API del servidor, versión `v1`, descrita por el equipo que la escribió.

## Reglas generales

1. **Dirección:** el servidor se configura en Preferences (`preferences.md`); por defecto, el de Tisma Cloud. Todo va
   por HTTPS; los pedidos y las respuestas son JSON, salvo los archivos.
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

## Laminar y calcular en la nube

6. **Mandar un trabajo:** `POST /v1/jobs`, multipart, con:
   - `kind`: `slice` o `fea`;
   - `file`: el proyecto 3MF (`formats.md`);
   - `params`: JSON.
     - `slice`: vacío.
     - `fea`: `mode` (`analyze`, `lightest-infill`, `resin-structure`), `quality` (`fast`, `normal`, `high`,
       `ultra`), `object`, `instance`, y opcionales `solid`, `zones`, `ribs`.

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
10. **Lista:** `GET /v1/jobs` lista los trabajos de la cuenta.

## Informar un problema

11. `POST /v1/reports` con:
    - `app`: `pc`, `android` o `web`;
    - `kind`: `bug`, `crash` o `suggestion`;
    - `title` (3 caracteres o más), `description`, `steps`, `expected` y `actual`.

    Si la persona lo acepta, también la versión, el sistema, la impresora y las últimas 500 líneas del registro
    (`interface.md`, «Report a problem»). Se puede mandar sin cuenta. `GET /v1/me/reports` lista los propios con su
    estado (`new`, `confirmed`, `fixed`, `wontfix`, `duplicate`). La pantalla va en `problem_reports.md`.

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

18. **Laminar un proyecto:** el motor lamina un proyecto 3MF completo, con sus perfiles, sus ajustes por objeto y su
    cama elegida, sin interfaz y sin leer la carpeta de datos del usuario:
    - entrada: el 3MF; salida: el G-code (texto o binario, como diga el perfil);
    - progreso por la salida estándar, una línea por cambio: `progress <0 a 100>`;
    - al terminar, un resumen JSON con el tiempo estimado, el filamento en mm y en g, y la cantidad de capas;
    - código de salida 0 si salió bien. Si no, otro código y el error en la salida de errores.
19. **Analizar:** lo mismo con `analyze` (ya existe en el motor) a partir del 3MF y los parámetros de la regla 6.
20. **Aislado:** cada trabajo corre en su carpeta, sin red, y con un tiempo máximo que pone el nodo. El motor no lee
    nada fuera de lo que recibe.
21. **Lo que cambia del lado del servidor:** el nodo pasa a llamar al motor con esta línea de comandos en lugar de los
    programas de Tisma Slicer. Lo hace el equipo del dueño en el servidor; el motor no ve su código.

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
| CC10 | `tisma-engine` laminando un 3MF en modo nodo | Líneas `progress`, G-code y resumen JSON; código 0 |
