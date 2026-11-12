# Tisma para iPhone — plan

Decisión del dueño (10-11-2026, por la sesión de QA): app de iPhone antes de la beta cerrada de diciembre,
reutilizando todo lo posible de la de Android. Mismo diseño (`docs/spec/features/mobile_app.md`).

## Cómo

1. **Módulo `shared` (Kotlin Multiplatform)** en `apps/android/shared`, destinos Android e iOS (`iosArm64`,
   `iosSimulatorArm64`). Se mueve primero la lógica pura, que ya no depende de Android: modelos de la nube, lista de
   bloqueo, índice, `SafeFile`, `SharingPolicy`, Ed25519 (propio), perfiles del paquete, estimación de memoria,
   enlaces de sitios, G-code binario. Lo de `java.io`/`MessageDigest`/`zip` pasa a `expect`/`actual` (o kotlinx-io).
   Android sigue compilando contra `shared` y sus pruebas siguen verdes en cada paso.
   Hecho (11-11): lista de bloqueo, índice, `SharingPolicy`, `PublishText`, búsqueda sin tildes, Ed25519, y los
   modelos y errores de la nube (`Account`, `Plan`, `Job`, `CloudException`…; `:cloud` los toma de `shared`). El cliente
   de la nube también (11-11, paso 2).
2. **Red**: sin biblioteca nueva (11-11): tipos HTTP propios en `shared` (`Http`, `HttpRequest`, `HttpResponse`,
   flujos `ByteInput`/`ByteOutput`), enviados con HttpURLConnection en Android y con NSURLSession en iOS. Hechos (11-11)
   Tisma Cloud y los clientes de Moonraker, OctoPrint, PrusaLink y Duet (con MD5 propio para Digest). Bambu queda
   en la JVM hasta su `expect`/`actual` de TLS. Las bajadas grandes en iOS (hasta
   1 GB) van a un archivo con la tarea de descarga de NSURLSession y se leen de ahí, no a memoria (12-11).
   Bambu (MQTT con TLS, FTPS implícito, cámara del puerto 6000) necesita sockets TLS con el certificado propio de la
   impresora: `expect`/`actual` con Network.framework en iOS.
3. **ViewModels en `shared`** (StateFlow), expuestos a Swift con envoltorios propios simples.
4. **Motor**: iOS no deja lanzar procesos. Hace falta el motor (y SolidCraft) como biblioteca: un `xcframework` con
   una entrada C que reciba los mismos argumentos que la línea de comandos y devuelva su salida. Lo pide la sesión del
   motor. Hasta que llegue, la app de iPhone funciona sin laminar: biblioteca, impresoras, cámara, nube y Compartida.
5. **Pantallas en SwiftUI** (`apps/ios/`), en este orden: Biblioteca, Impresora (con Material), Más (cuenta, ajustes),
   Placa (cuando llegue el motor), Ingeniería al final.
   Empezado (12-11): `project.yml` (XcodeGen), las cuatro secciones, «Más» entra a la cuenta por el cliente común
   (`CloudBridge`, con `@Throws` para que un error llegue a Swift como error), textos en inglés y español.
   Biblioteca (12-11): la misma bajada que Android (`LibraryFetch` en `shared`: clave, lista e índice, guardados
   para cuando no hay red), firmas comprobadas con CryptoKit, búsqueda sin tildes. Sin la clave del proyecto (pendiente
   del dueño) no acepta nada, igual que Android.
   Impresora (12-11): Klipper, OctoPrint, PrusaLink y Duet con los clientes comunes (`PrinterBridge`), estado cada
   5 s solo con la pantalla a la vista, pausar, seguir y parar; la clave en el Llavero del iPhone. Bambu después.
6. **CI**: `.github/workflows/ios.yml` en macOS: las pruebas de `shared` en el simulador de iPhone (hecho, 11-11);
   después `xcodebuild test` con la app de SwiftUI (12-11; las capturas como artefacto, después). Las pruebas de `shared` también corren
   aquí en la JVM.
7. **Planes** (dueño, 10-11; `cloud.md` 5b, spec/next cada191): se compran dentro de la app con la tienda. En iOS,
   StoreKit 2 con suscripciones que se renuevan solas, con los mismos ids de producto que Google Play donde se pueda;
   el servidor verifica la compra y el plan queda en la cuenta de Tisma; «Cancelar o cambiar el plan» abre la página
   de suscripciones de la tienda. La compra se hace en Swift (StoreKit 2 no se llama desde Kotlin) y el recibo va al
   servidor por el cliente de la nube de `shared`. Necesita la cuenta de Apple Developer y los productos creados en
   App Store Connect. En Android, Google Play Billing sigue pendiente (también lo decide la cuenta del dueño).
8. **Distribución**: TestFlight cuando el dueño tenga la cuenta de Apple Developer; mientras, solo simulador.

## Límites

- En esta sesión no hay Mac: lo de iOS se comprueba solo en los runners de GitHub. Lo compartido se prueba aquí antes
  de cada subida.
- `CLAUDE.md` deja macOS para el final del motor; esto es la app, no el motor, y lo pidió el dueño.

## Pendiente del dueño

- Cuenta de Apple Developer (TestFlight y StoreKit); productos de suscripción en App Store Connect y Google Play.
- Bundle id: `net.tismaservidores.tisma` (QA 10-11), salvo que el dueño diga otro.
