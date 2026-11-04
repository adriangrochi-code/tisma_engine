# Tisma para Android

La app de teléfono de Tisma (ficha `docs/spec/features/mobile_app.md`; decisiones del dueño en `docs/MOBILE.md`).
Kotlin y Jetpack Compose, tema claro y oscuro con los colores de `interface.md`, textos en inglés y español. Se
compila con Gradle, aparte del CMake del motor.

## Módulos

| Módulo | Qué es |
|---|---|
| `cloud/` | El cliente de Tisma Cloud (API v1 de `cloud.md`), en Kotlin sin Android: cuenta, planes, trabajos, resultados, informes. Se prueba en la JVM. |
| `mockcloud/` | Un servidor de prueba escrito desde `cloud.md`, en memoria. Lo usan las pruebas y sirve para probar la app en un emulador. |
| `printers/` | Clientes de Klipper (Moonraker) y OctoPrint, en Kotlin sin Android (`printer_connections.md`). |
| `mockprinters/` | Simuladores de Moonraker y OctoPrint, escritos desde su documentación pública. |
| `app/` | La app: secciones Placa, Impresión, Material, Impresora y Más (`mobile_app.md`, regla 5). |

## Qué hace hoy (2026-11-04)

- **Más › Cuenta de Tisma Cloud:** entrar, crear la cuenta con el código del correo, recuperar la contraseña, cerrar
  sesión; el plan, las horas del mes tal como las manda el servidor y los planes públicos con el actual marcado (reglas
  2 a 5a). Pagar se hace en la página de la cuenta, en el navegador (regla 5).
- **Más › Trabajos en la nube:** la lista del más nuevo al más viejo, con el avance, preguntando cada 2 s mientras uno
  corre (reglas 7, 8 y 10); el detalle con el resumen, cancelar y guardar el resultado donde elija la persona (regla 9).
- **Trabajo nuevo** (también desde Placa › «Laminar en la nube»): elegir un proyecto 3MF (se comprueba que sea un
  paquete 3MF antes de mandarlo), laminar, análisis estructural, relleno mínimo, aerodinámica u orientación con sus
  parámetros (regla 6). Un cálculo que el plan no incluye muestra el candado y los planes que lo tienen (regla 10b).
- **Errores:** 401 pide entrar de nuevo; 413, 422, 429 y 402 con su aviso y el texto del servidor (regla 4). Un trabajo
  fallido muestra `error_es` en español, si no el texto por `code`, si no `error` (regla 19a).
- **Más › Informar un problema** (`problem_reports.md`) con `app` `android`, y **Mis informes**.
- **Más › Preferencias:** la dirección del servidor (por defecto `https://tismaservidores.net`) y `cloud.prefer`.
- **El token** se guarda cifrado con AES-256-GCM con una clave del almacén de claves de Android, nunca en texto
  (regla 3; `mobile_app.md`, regla 14).

- **Impresora:** impresoras Klipper (Moonraker) y OctoPrint de la red: estado, avance, capa, tiempos, temperaturas,
  pausar, seguir, cancelar y mandar un G-code.
- **Placa:** el motor del escritorio adentro del APK (`tisma-engine` compilado con el NDK, sin cambios): abrir STL,
  OBJ o 3MF, calidad y soportes, laminar en el teléfono con avance y cancelar, guardar el G-code o mandarlo a una
  impresora.

Falta: Bambu Lab y las demás marcas, perfiles de impresora y filamento, la vista previa, entrar con Google y pagar con Google Play (los dos necesitan bibliotecas de Google con licencia propia: lo
decide el dueño), abrir el G-code en la vista previa (llega con el motor).

## Compilar y probar

Con un JDK 17 o más nuevo, el SDK de Android (plataforma 36), el NDK r29 (`29.0.14206865`), CMake y Ninja. El
APK compila el motor desde la raíz del repositorio (unos minutos por arquitectura la primera vez);
`-Ptisma.engine=false` lo deja afuera para trabajar solo en las pantallas:

```text
cd apps/android
tools/ci.sh                      # íconos, licencias, pruebas y el APK de depuración
./gradlew :app:assembleDebug     # solo el APK: app/build/outputs/apk/debug/app-debug.apk
```

Impresoras simuladas para un emulador: `./gradlew :mockprinters:run` (Moonraker en `http://10.0.2.2:7125` sin
clave; OctoPrint en `http://10.0.2.2:7126` con la clave `test-key`).

Probar en un emulador contra el servidor de prueba:

```text
./gradlew :mockcloud:run         # escucha en el puerto 8765
```

En la app, Más › Preferencias › servidor `http://10.0.2.2:8765` (solo la versión de depuración acepta `http://`).
Cuentas, todas con la contraseña `password1`: `ana@example.com` (plan Maker), `free@example.com` (Free, solo laminar),
`pro@example.com` (Pro, todo). El código de confirmación es `123456`. Un trabajo avanza un paso cada vez que se
pregunta por él. Para cambiar el estado: `POST /mock/grant {"email","hours"}`, `/mock/disable {"email"}`,
`/mock/plan {"email","plan"}`, `/mock/expire`, `/mock/fail-next {"status","detail"}`, `/mock/max-upload {"bytes"}`.

Los íconos de `app/src/main/res/drawable/tisma_*.xml` salen de `assets/brand/icons/` con `tools/brand_icons.py`.

## SolidCraft (Design)

The workflow «SolidCraft for Android» (`.github/workflows/solidcraft-android.yml`) builds `solidcraft-cli` for
arm64-v8a and x86_64. Download its two artifacts into one folder (each brings `<abi>/solidcraft-cli` and the licence
files) and build with `./gradlew :app:assembleDebug -Ptisma.solidcraft=<that folder>`: the APK then carries it as
`lib/<abi>/libsolidcraft.so`. Without the property the app has the engine's own parts only.
