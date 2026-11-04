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
