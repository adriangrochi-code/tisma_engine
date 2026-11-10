# Tisma para iPhone — plan

Decisión del dueño (10-11-2026, por la sesión de QA): app de iPhone antes de la beta cerrada de diciembre,
reutilizando todo lo posible de la de Android. Mismo diseño (`docs/spec/features/mobile_app.md`).

## Cómo

1. **Módulo `shared` (Kotlin Multiplatform)** en `apps/android/shared`, destinos Android e iOS (`iosArm64`,
   `iosSimulatorArm64`). Se mueve primero la lógica pura, que ya no depende de Android: modelos de la nube, lista de
   bloqueo, índice, `SafeFile`, `SharingPolicy`, Ed25519 (propio), perfiles del paquete, estimación de memoria,
   enlaces de sitios, G-code binario. Lo de `java.io`/`MessageDigest`/`zip` pasa a `expect`/`actual` (o kotlinx-io).
   Android sigue compilando contra `shared` y sus pruebas siguen verdes en cada paso.
2. **Red**: Ktor (Apache-2.0; motor Darwin en iOS) para Tisma Cloud, Moonraker, OctoPrint, PrusaLink y Duet.
   Bambu (MQTT con TLS, FTPS implícito, cámara del puerto 6000) necesita sockets TLS con el certificado propio de la
   impresora: `expect`/`actual` con Network.framework en iOS.
3. **ViewModels en `shared`** (StateFlow), expuestos a Swift con envoltorios propios simples.
4. **Motor**: iOS no deja lanzar procesos. Hace falta el motor (y SolidCraft) como biblioteca: un `xcframework` con
   una entrada C que reciba los mismos argumentos que la línea de comandos y devuelva su salida. Lo pide la sesión del
   motor. Hasta que llegue, la app de iPhone funciona sin laminar: biblioteca, impresoras, cámara, nube y Compartida.
5. **Pantallas en SwiftUI** (`apps/ios/`), en este orden: Biblioteca, Impresora (con Material), Más (cuenta, ajustes),
   Placa (cuando llegue el motor), Ingeniería al final.
6. **CI**: `.github/workflows/ios.yml` en `macos-latest`: pruebas de `shared` en el simulador, `xcodebuild test` con
   el simulador de iPhone y capturas como artefacto. Las pruebas de `shared` también corren aquí en la JVM.
7. **Distribución**: TestFlight cuando el dueño tenga la cuenta de Apple Developer; mientras, solo simulador.

## Límites

- En esta sesión no hay Mac: lo de iOS se comprueba solo en los runners de GitHub. Lo compartido se prueba aquí antes
  de cada subida.
- `CLAUDE.md` deja macOS para el final del motor; esto es la app, no el motor, y lo pidió el dueño.

## Pendiente del dueño

- Identificador de la app (bundle id) y cuenta de Apple Developer para TestFlight.
