# Avance de la app de Android

Estado de la sesión de Android, para seguir sin depender de la conversación (decisión del dueño, 2026-11-04).

## Hecho

- **2026-11-04 — base y Tisma Cloud** (commit `feat(android): Android app with the Tisma Cloud screens`, PR #55
  `android` → `dev`): proyecto Gradle (`cloud`, `mockcloud`, `app`), CI aparte (`.github/workflows/android.yml`),
  pantallas de la nube (cuenta, planes, trabajos, trabajo nuevo, informes, preferencias), token cifrado con el
  Android Keystore. 38 pruebas en la JVM, todas pasan. Detalle en `README.md`.

## Abierto

- **Paso 3, impresoras de la red** (`printer_connections.md`, `devices.md`): siguiente. Preguntado al equipo del
  motor si espera a `src/device/` en C++ o si van clientes en Kotlin dentro de la app.
- **Paso 4, laminar en el teléfono:** pedida al equipo del motor (session_01JDAJxTVh345Lfm4794rJF8) la API en C
  (`mobile_app.md`, regla 2), con una propuesta de forma. Sin respuesta todavía.
- **Preguntas al equipo de fichas** (session_01QadJfL7KViJEaJL92p8Swb): campos de `GET /v1/me`; nombre del campo de la
  contraseña nueva en el reset; `object` e `instance` (¿índice o id del 3MF?); campos de `/v1/me/sessions`; si el
  servidor elige `slice_large` por tamaño; qué 3MF acepta `slice`; qué recibe `POST /v1/auth/google`.
- **No probado:** teléfono real, emulador, servidor real.

## Decidido (por la sesión, a confirmar si una ficha dice otra cosa)

- Campos de `GET /v1/me` tomados del cliente de la nube del motor (`src/cloud/client.cpp`).
- `object` e `instance` como números que escribe la persona, 0 por defecto, hasta que la ficha diga qué son.
- En inglés, un trabajo fallido muestra el texto por `code` y si no `error`; en español, `error_es` primero (regla 19a).
- Identificador de la app `net.tismaservidores.tisma` (a confirmar por el dueño antes de publicar).
- Sin Google (inicio de sesión ni Play Billing) hasta que el dueño decida sobre esas bibliotecas.

## Para que decida el dueño

- Herramientas de prueba con licencias fuera de la lista (no entran al APK): JUnit 4 (EPL-1.0) y
  `javax.annotation-api` (CDDL o GPL-2.0 con excepción), que Robolectric necesita (`THIRD_PARTY.md`).

## Compilar y probar

`cd apps/android && tools/ci.sh` (JDK 17+, SDK de Android con la plataforma 36 en `ANDROID_HOME` o `local.properties`).

Notas de la máquina de la sesión: Maven Central contesta 429 a veces por el proxy; reintentar. Robolectric baja
`org.robolectric:android-all-instrumented:15-robolectric-13954326-i7` (200 MB); si falla, bajarlo del espejo
`https://maven-central.storage-download.googleapis.com/maven2/` a `~/.m2/repository/`.
