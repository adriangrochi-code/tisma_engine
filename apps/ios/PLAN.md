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
   Empezado (12-11): `AccountModel` (sesión, cuenta, planes, estado del servicio, sesión vencida), que «Más» del
   iPhone mira con `watch`; el token en el Llavero. Android sigue con su `CloudViewModel` hasta pasarse a este.
4. **Motor**: iOS no deja lanzar procesos. Hace falta el motor (y SolidCraft) como biblioteca: un `xcframework` con
   una entrada C que reciba los mismos argumentos que la línea de comandos y devuelva su salida. Lo pide la sesión del
   motor. Hasta que llegue, la app de iPhone funciona sin laminar: biblioteca, impresoras, cámara, nube y Compartida.
   Llegó (12-11): la CI del motor arma `TismaEngine.xcframework` con `tisma_run_cli`; la CI de iOS lo baja de `dev`,
   la app lo enlaza y Swift llama a la línea de comandos en el proceso. Lamina el cubo de 20 mm en el simulador
   (12-11). Perfiles (12-11): impresora, boquilla y filamento del catálogo del motor (`Catalog` en `shared`) y
   `tisma-engine profile` para el `--config`. Vista previa por capas (12-11, `GcodeLayers` en `shared`). Abrir STL, OBJ y
   3MF de los archivos del teléfono (12-11). Mandar el G-code a la
   impresora guardada y empezar (12-11). «Compartir con Tisma»: otras apps abren STL, OBJ
   y 3MF en Tisma y el modelo va a la Placa (12-11). Laminar un 3MF en la nube, seguirlo, bajar el G-code y
   verlo (12-11; `Jobs.kt` en `shared`). Impresión y Material básicos (12-11): altura de capa, relleno, soportes y
   temperaturas sobre el perfil (`PrintChanges` en `shared`). El panel completo (12-11): secciones Impresión y Material
   con los niveles Básico, Normal y Avanzado y las reglas de ocultar, del mismo `SettingsLayout` que Android (en
   `shared`); los cambios se aplican al laminar. Las secciones de abajo quedan como pide la regla 5.
   Asistente de primer uso (12-11, MA1): impresora por modelo, boquilla y filamento del catálogo; se vuelve a abrir
   desde Más. Orden siguiente (QA, 12-11): avisos de fin de impresión, tarjeta de impresora, Ingeniería.
   Avisos de fin de impresión (12-11): PrintWatcher lee la impresora guardada cada 5 s con la app abierta y da un
   aviso local (terminó, se paró, en pausa, error) con las reglas de PrintEvents de Android; en segundo plano, una
   tarea BGAppRefresh mientras imprime (iOS decide cuándo: el aviso puede llegar tarde). Sin probar en un iPhone.
   Tarjeta de impresora (12-11): en Placa, el dibujo de la impresora, la boquilla y la cama (Bambu Lab) con las mismas
   tablas que Android (reglas 34 a 36, ahora en el módulo compartido), el filamento y el icono de conexión (regla 42)
   que abre Impresora.
   Ingeniería (12-11): desde Placa, cara sujeta y cara con carga entre las caras propias del motor (sin tocar la
   pieza en 3D todavía), fuerza, material, calidad, seguridad pedida y modo rápido; `analyze` dentro de la app y el
   veredicto, la seguridad mínima, desplazamiento, tensión y avisos, como Android (EngineeringCase en el módulo
   compartido). Falta: el mapa de seguridad sobre la pieza y marcar caras tocando el modelo.
   Caras tocando la pieza y mapa de seguridad (12-11, QA): la pieza en 3D (SceneKit; STL y OBJ leídos con ModelIO,
   un 3MF sigue con las caras por nombre); un toque marca la cara sujeta o la cargada (la cara plana alrededor, su
   caja o la cara propia del motor, con FacePicking del módulo compartido, igual que Android); tras analizar, la
   superficie de vóxeles coloreada por seguridad (`analyze --mesh`, StressMap compartido).
5. **Pantallas en SwiftUI** (`apps/ios/`), en este orden: Biblioteca, Impresora (con Material), Más (cuenta, ajustes),
   Placa (cuando llegue el motor), Ingeniería al final.
   Empezado (12-11): `project.yml` (XcodeGen), las cuatro secciones, «Más» entra a la cuenta por el cliente común
   (`CloudBridge`, con `@Throws` para que un error llegue a Swift como error), textos en inglés y español.
   Biblioteca (12-11): la misma bajada que Android (`LibraryFetch` en `shared`: clave, lista e índice, guardados
   para cuando no hay red), firmas comprobadas con CryptoKit, búsqueda sin tildes. Sin la clave del proyecto (pendiente
   del dueño) no acepta nada, igual que Android.
   Impresora (12-11): Klipper, OctoPrint, PrusaLink y Duet con los clientes comunes (`PrinterBridge`), estado cada
   5 s solo con la pantalla a la vista, pausar, seguir y parar; la clave en el Llavero del iPhone. Bambu Lab (12-11):
   el mismo cliente que Android (MQTT, TLS con la huella fijada la primera vez que se prueba, FTPS) para el estado y
   los controles; mandarle archivos también (12-11: ZIP, CRC-32 y MD5 propios en `shared`). Nada probado contra una
   impresora.
6. **CI**: `.github/workflows/ios.yml` en macOS: las pruebas de `shared` en el simulador de iPhone (hecho, 11-11);
   después `xcodebuild test` con la app de SwiftUI (12-11) y una prueba de interfaz que guarda una captura de cada sección como artefacto `ios-screenshots`. Las pruebas de `shared` también corren
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

## Paridad con Android (`mobile_app.md`, QA 12-11)

Qué tiene Android que el iPhone todavía no, regla por regla. Se cierran de a uno, en este orden; cada hueco cerrado
se marca con la fecha.

| Regla | Android | iPhone | Hueco |
|---|---|---|---|
| 5 Secciones | sí | sí | — |
| 6 Placa | cama y modelo en 3D, tarjeta, Laminar, salida (guardar, compartir, mandar), gestos, tiras de herramientas | tarjeta, Laminar, vista previa, mandar; la cama y el modelo en 3D con gestos y Encuadrar (P1, 12-11); compartir y guardar el G-code (P2, 12-11) | P3: tiras de herramientas (Archivo, Vista, Ingeniería, Laminar) |
| 7 Impresión y Material | niveles y vista Personalizada | niveles y vista Personalizada (P4, 12-11) | — |
| 8 Impresora | Conexión y Calibración | Conexión y Calibración (P5, 12-11) | — |
| 9 Más | cuenta, biblioteca, ayuda, informar un problema, preferencias, créditos | cuenta, biblioteca compartida | ayuda (P21, 12-11); informar un problema (P6, `app` `ios`, 12-11: el servidor lo acepta desde cc28f98 de tisma-cloud, en producción cuando el dueño actualice); preferencias (el servidor) y créditos (P7, 12-11; la lista de bibliotecas del motor en iOS la confirma el motor) |
| 10 Abrir | STL, 3MF, OBJ, STEP, Compartir, enlace de sitio | STL, 3MF, OBJ, Compartir | P8: STEP; P9: enlaces de sitios de modelos |
| 11 Perfiles | paquete de perfiles y asistente | catálogo del motor y asistente; el paquete se baja e instala desde Más (P10, 12-11) | P10c: elegir otro filamento del paquete (hoy, el que trae la variante; las impresoras del paquete ya están en el asistente, P10b, 12-11) |
| 12 Laminar | avance, cancelar, memoria, costo, vista previa | vista previa; avance por etapas, cancelar y memoria con la API de trabajos del motor (P11, P12, 12-11; en dev desde 4986dc4) | P13: el costo (el motor lo da null por ahora) |
| 13 Ingeniería | caras tocadas, punto, dirección, temperatura, mapa, relleno mínimo | caras tocadas, mapa | P16: relleno mínimo (el motor lo hace sobre proyectos 3MF; en una pieza suelta, a preguntar). Carga en un punto, dirección y temperatura: P14, P15, 12-11 |
| 14 Impresoras | búsqueda en la red, cámara | las cinco clases, mandar, pausar, seguir, parar, avisos | — (búsqueda en la red con Bonjour, P17, y cámara, P18, 12-11) |
| 15 Nube | Google, analizar en la nube, impresoras remotas, pagos | cuenta, laminar en la nube | P19: analizar en la nube; P20: impresoras remotas de los puentes; restaurar compras con StoreKit 2 al abrir la sesión (12-11); comprar y Google esperan al dueño (cuenta de Apple) |
| 16 Ayuda sin conexión | sí | la ayuda del paquete en Más, con búsqueda (P21, 12-11) | — |
| 17 Teclado numérico | sí | teclado decimal (P22, 12-11; con signo en los desplazamientos) | — |
| 18, 19 Rendimiento | medido | sin medir | se mide en un iPhone cuando haya cuenta de Apple |

## Pendiente del dueño

- Cuenta de Apple Developer (TestFlight y StoreKit); productos de suscripción en App Store Connect y Google Play.
- Bundle id: `net.tismaservidores.tisma` (QA 10-11), salvo que el dueño diga otro.
