# Avance de la app de Android

Estado de la sesión de Android, para seguir sin depender de la conversación (decisión del dueño, 2026-11-04).

## Hecho

- **2026-11-04 — base y Tisma Cloud** (commit `feat(android): Android app with the Tisma Cloud screens`, PR #55
  `android` → `dev`): proyecto Gradle (`cloud`, `mockcloud`, `app`), CI aparte (`.github/workflows/android.yml`),
  pantallas de la nube (cuenta, planes, trabajos, trabajo nuevo, informes, preferencias), token cifrado con el
  Android Keystore. 38 pruebas en la JVM, todas pasan. Detalle en `README.md`.

- **2026-11-04 — impresoras de la red** (commit `feat(android): Moonraker and OctoPrint clients with simulators`
  y las pantallas): Klipper (Moonraker) y OctoPrint en Kotlin detrás de `PrinterConnection`, con simuladores propios;
  sección Impresora: lista con estado, agregar con prueba de conexión, detalle con avance, capa, tiempos,
  temperaturas, pausar, seguir, cancelar (con confirmación) y mandar un G-code. La clave, cifrada.
- **2026-11-04 — el motor del escritorio en el teléfono** (decisión del dueño: «usemos todo lo de la pc»): el mismo
  `tisma-engine` compilado con el NDK r29 para arm64-v8a y x86_64, sin cambios en `src/`, empaquetado en el APK y
  corrido como programa aparte (línea de comandos de `cloud.md`, regla 18). Pantalla Placa: abrir STL, OBJ o 3MF,
  calidad 0,12 / 0,20 / 0,28 mm y soportes, laminar con avance y cancelar, guardar el G-code o mandarlo a una
  impresora. `minSdk` 28 (Android 9), porque el motor usa `posix_spawn`.

- **2026-11-04 — pendientes, sin parar** (pedido del dueño: «seguí de forma automática con los pendientes»):
  sesiones abiertas de la cuenta (cerrar una o las otras) y los campos nuevos de `/v1/me`; la cámara de la
  impresora (una imagen por segundo, CM1); la búsqueda en la red con DNS-SD (OctoPrint, Moonraker; BD2, BD4) y su
  preferencia; la vista previa del G-code por capas (desde el teléfono y desde la nube, MA10); los avisos al terminar,
  pausar o fallar una impresión, con un servicio en primer plano mientras algo imprime.

- **2026-11-04 — «Compartir con Tisma»** (MA5): un STL, OBJ o 3MF compartido o abierto con Tisma desde otra app va a la Placa.
- **2026-11-04 — la tarjeta de la impresora y el catálogo:** el catálogo del motor (`resources/catalog/`, el mismo
  del escritorio) va en el APK; en Placa, la tarjeta de la impresora abre el selector de impresora, boquilla y
  filamento. El perfil lo arma el motor con `tisma-engine profile` (motor c0f68df); probado de punta a punta con el motor
  de esta máquina (perfil del catálogo más la calidad elegida: un cubo de 10 mm da 83 capas a 0,12 mm). Si el motor
  no puede armarlo, el laminado usa sus valores por defecto y la pantalla lo dice.

- **2026-11-04 — controles e impresoras nuevas:** controles de la impresora (temperaturas siempre; ir a casa,
  mover y extruir cuando está quieta) en Moonraker, OctoPrint y Duet; PrusaLink (Digest de la RFC 7616, su OpenAPI)
  y Duet (pedidos `rr_*` con clave de sesión, del wiki de RepRapFirmware), con simuladores. Sin probar con
  impresoras reales. En Duet, cancelar es M25 y M0 (lista de G-code de la wiki de RepRap): a confirmar con una
  Duet real; sus errores de G-code llegan después (`rr_reply`) y la app todavía no los lee.

- **2026-11-04 — Bambu Lab en modo LAN:** MQTT 3.1.1 propio sobre TLS (8883), el estado combinado de los informes,
  pausar, seguir, parar, controles por `gcode_line`, subir por FTPS implícito propio (990) como `.gcode.3mf` con su
  MD5 y empezar con `project_file`; la huella del certificado y el número de serie se confirman la primera vez y
  después se exigen (BL1 a BL3 contra un simulador propio con TLS). Sin probar con una impresora real: la reanudación
  de la sesión TLS en la conexión de datos del FTPS (el servidor de Bambu la exige), los valores de `gcode_state` que
  no son los de la documentación y la unidad de `mc_remaining_time` (se toma en minutos). Sin la cámara de Bambu
  todavía (puerto 6000 y RTSP).

- **2026-11-04 — Ingeniería en el teléfono:** con un proyecto 3MF abierto en Placa, «¿Aguanta?» (`analyze`, con los
  apoyos, cargas y material del proyecto) y Aerodinámica (`aero`), con el motor en el teléfono, avance y cancelar.
  Probado con el motor de esta máquina: un cubo de 20 mm da Cd 1,16; sin condiciones, `needs_conditions` con su
  `error_es`. Marcar caras en el teléfono necesita la vista 3D (pendiente). Pedido a la sesión de escritorio: los
  modos de Optimizar por `analyze`.

## Abierto

- **Impresoras:** faltan Bambu Lab (MQTT con TLS y FTPS: necesita una biblioteca MQTT o una propia; decidir), PrusaLink,
  Duet, mover ejes y poner temperaturas a mano. La cámara se ve por imágenes fijas, no por video (MJPEG o RTSP). Si el motor suma conexiones de red a `src/device/`, se usan
  desde la app en lugar de las de Kotlin («usemos todo lo de la pc»).
- **Motor en el teléfono:** nunca corrió en un Android real (esta máquina no tiene KVM ni emulador); probado solo
  con un motor falso. Falta: perfiles de impresora y filamento (paquete de perfiles, `profile_pack.md`), la
  estimación de memoria (MA9) y la vista previa del G-code binario. El resumen de un modelo suelto ya llega (`--summary`, motor
  9e8c43e).
- **Paso 4, laminar en el teléfono:** pedida al equipo del motor (session_01JDAJxTVh345Lfm4794rJF8) la API en C
  (`mobile_app.md`, regla 2), con una propuesta de forma. Sin respuesta todavía.
- **Respuestas del equipo de fichas (04-11-2026), a aplicar:** `GET /v1/me` trae además `unlimited`,
  `remote_printers`, `extra_printers`, `extra_printer_cents` y `has_password`; el reset devuelve `{token, email}` y
  acepta `device` (hoy la app no lo usa como sesión); `object`/`instance` son índices desde 0 (lo que ya hace la
  app); `GET /v1/me/sessions` → `{sessions: [{id, device, created_at, last_used, current}]}` (falta la pantalla);
  el servidor elige `slice_large` por triángulos (más de 300 000); `slice` en la nube acepta solo proyectos del motor
  (3MF con `Metadata/tisma_project.json`); `POST /v1/auth/google` toma `{id_token, device}` y puede pedir verificar.
- **API en C del motor:** el encabezado está en `dev` (9276c39, `include/tisma/tisma_engine.h`, `docs/C_API.md`), sin
  implementar; viene después de Ingeniería y Diseño. Hasta entonces la app corre el programa `tisma-engine`.
- **No probado:** teléfono real, emulador, servidor real.

## Decidido (por la sesión, a confirmar si una ficha dice otra cosa)

- Trabajo con la sesión de escritorio (pedido del dueño, 04-11-2026): propuestos (1) `tisma-engine settings-layout`
  para armar Impresión y Material con los mismos datos del panel del escritorio, (2) que el servicio local de
  `tisma` para las impresoras USB hable un subconjunto compatible con Moonraker (la app ya lo sigue), y (3) pasarle
  notas de comportamiento de las conexiones de red para su página Device.
- Bambu Lab en modo LAN se hace (dueño, 04-11-2026: «cuando dudes, decidí por la hoja de ruta y las fichas»;
  `mobile_app.md` regla 14 lo pide y `printer_connections.md` lo describe desde OpenBambuAPI): MQTT 3.1.1 propio
  (especificación OASIS), FTPS implícito propio, y la huella del certificado guardada la primera vez que la persona la
  confirma. Sin bibliotecas nuevas.

- Campos de `GET /v1/me` tomados del cliente de la nube del motor (`src/cloud/client.cpp`).
- `object` e `instance` como números que escribe la persona, 0 por defecto, hasta que la ficha diga qué son.
- En inglés, un trabajo fallido muestra el texto por `code` y si no `error`; en español, `error_es` primero (regla 19a).
- Identificador de la app `net.tismaservidores.tisma` (a confirmar por el dueño antes de publicar).
- Sin Google (inicio de sesión ni Play Billing) hasta que el dueño decida sobre esas bibliotecas.
- «Usemos todo lo de la pc» (dueño, 04-11-2026): se toma como el código del motor y del escritorio de este
  repositorio (misma licencia). No incluye Tisma Slicer (AGPL, sala limpia).
- El motor va como programa aparte y no como biblioteca por JNI: no hace falta tocar `src/`, cancelar es terminar el
  proceso, y un fallo del motor no cierra la app.

## Para que decida el dueño


- Herramientas de prueba con licencias fuera de la lista (no entran al APK): JUnit 4 (EPL-1.0) y
  `javax.annotation-api` (CDDL o GPL-2.0 con excepción), que Robolectric necesita (`THIRD_PARTY.md`).

## Compilar y probar

`cd apps/android && tools/ci.sh` (JDK 17+, SDK de Android con la plataforma 36 en `ANDROID_HOME` o `local.properties`).

Notas de la máquina de la sesión: Maven Central contesta 429 a veces por el proxy; reintentar. Robolectric baja
`org.robolectric:android-all-instrumented:15-robolectric-13954326-i7` (200 MB); si falla, bajarlo del espejo
`https://maven-central.storage-download.googleapis.com/maven2/` a `~/.m2/repository/`.
