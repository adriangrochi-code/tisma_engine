# App móvil de Android

**Decisión del dueño (2026-10-24):** «ve por la opción 1 y luego por devices, así podemos conectar impresoras;
tiene que tener soporte USB», y la aclaración: «el slicer de PC puede imprimir por USB; el celular, solo por WiFi».
La app de Android **sigue la impresión** (opción 1 de las dos que se le plantearon: no es la interfaz entera del
laminador en el teléfono) y se conecta **solo por la red**. El USB es del laminador de escritorio: la capa de
conexiones es la misma de Device (P2 de `PLAN.md`), y el escritorio la usa con el puerto serie primero.

Todo es diseño propio, en sala limpia (`CLEANROOM.md`): nada de OctoPrint, Printoid, OctoApp, Mobileraker ni de los
hosts de Tisma Slicer (vienen de código de OrcaSlicer). Los protocolos salen de la documentación pública de cada
firmware y de la ficha de Device cuando esté en `main` (`docs/spec/features/devices.md`, PR #11).

**Estética (dueño, 2026-10-24):** «algo similar a Prusa EasyPrint en estética, con nuestras funciones». La
referencia es la app de Prusa (su modo EasyPrint): pantallas simples y grandes, pocas opciones a la vista, un
camino corto de «elegir, confirmar, mirar». Se toma como **caja negra**: capturas y documentación de usuario
públicas, para el aire general; ningún ícono, texto, color ni código se copia. Los íconos y la paleta son los de
`assets/brand/`, como en `tisma`.

## Qué hace

| Pantalla | Qué muestra o hace |
|---|---|
| **Impresoras** | La lista de impresoras conocidas, con su estado (desconectada, lista, imprimiendo, en pausa, error) |
| **Impresión** | El archivo, el avance en %, la capa, el tiempo que pasó y el que falta, las temperaturas de boquilla, cama y cámara con sus objetivos |
| **Cámara** | La imagen de la impresora (cuando la conexión la da), o la del teléfono apoyado frente a la máquina |
| **Controles** | Pausar, reanudar, cancelar; temperaturas; mover ejes y extruir cuando la impresora está parada |
| **Archivos** | Los G-code de la impresora o del teléfono: subir uno e imprimirlo |
| **Avisos** | Notificación al terminar, al pausar por falta de filamento y en un error |

## Conexiones

La app no tiene conexiones propias: usa la capa de dispositivos del motor, por WiFi.

- **Impresoras con API de red,** directo desde el teléfono: las de la ficha de Device, compartidas con el
  escritorio: Moonraker (Klipper), OctoPrint, PrusaLink, Duet/RepRapFirmware, Bambu Lab, Creality, Elegoo, Anycubic,
  Flashforge. Donde no hay documentación oficial (por ejemplo, el modo LAN de Bambu Lab), decide el dueño.
- **Impresoras conectadas por USB a la PC,** a través de la PC: `tisma` las maneja con su host de G-code por puerto
  serie (abajo) y ofrece en la red local un servicio propio y chico (HTTP y WebSocket, con un código de vinculación
  que se muestra en pantalla) con el estado, el avance, los controles y la cámara. El teléfono lo sigue como a
  cualquier impresora de red. Así, con una sola capa, el teléfono ve todas las impresoras que la PC ve.
- **Sin USB en el teléfono** (decisión del dueño).

## Cómo se hace

### Una sola capa de dispositivos, en C++ (`src/device/`)

Compartida por `tisma` (la página Device) y la app (por NDK/JNI), así cada protocolo se escribe y se prueba una vez.

- **`Printer`:** la interfaz de una impresora: conectar y desconectar, estado, temperaturas (actuales y objetivos),
  avance y tiempos, lista de archivos, subir, imprimir, pausar, reanudar, cancelar, mover, extruir, dirección de la
  cámara, y un canal de eventos (cambios de estado, avisos). Todo sin bloquear: la interfaz pregunta y recibe
  eventos.
- **Transportes:** puerto serie (termios en Linux, el puerto COM en Windows, código propio; solo en el
  escritorio), HTTP y WebSocket, MQTT, descubrimiento en la red. Cada biblioteca que
  haga falta se revisa y se anota en `THIRD_PARTY.md` antes de usarla; donde el protocolo es chico, código propio.
- **El host de G-code por serie (propio):** abre el puerto (115 200 o 250 000 baudios), espera el saludo, manda
  las líneas con número y suma de verificación (`N… *…`), espera el `ok` de cada una, reenvía la que el firmware
  pide (`Resend:`), lee las temperaturas (`T:… B:…` de `M105` o los informes automáticos de `M155`), la posición
  (`M114`) y el avance de una impresión desde la tarjeta (`M27`), y lleva la cola de la impresión desde el
  teléfono o la PC con pausa y cancelación. El formato de las respuestas sale de la documentación pública de Marlin
  y RepRapFirmware (los documentos de G-code, no el código).
- **Simulador de impresora serie (propio), para las pruebas:** un pseudo-terminal que contesta `ok`, `T:` y `M27`
  como una impresora Marlin. Las pruebas de CI conectan el host al simulador y le imprimen el cubo.

### La app

- **Kotlin + Jetpack Compose** (Apache-2.0) para las pantallas, con los íconos y la paleta de `assets/brand/`.
- **El núcleo en C++** (`src/device/` y lo que haga falta del motor: lectura de G-code para el avance por capas)
  compilado con el NDK; JNI delgado.
- **Solo red:** WiFi, por la capa compartida; sin USB en el teléfono. Para las impresoras por USB de la PC, el
  servicio local de `tisma`.
- **Cámara:** la imagen que da la impresora (MJPEG o la que diga la ficha), o la cámara del teléfono con CameraX.
- **Avisos:** notificaciones de Android desde un servicio en segundo plano mientras hay una impresión.
- **Publicación:** APK firmado para instalar directo; la Play Store, si el dueño la quiere, con su cuenta.

## Orden de trabajo

1. **`src/device/` con USB en el escritorio:** el host serie y el simulador, y la página Device mínima en `tisma`:
   conectar por USB, ver estado y temperaturas, imprimir el G-code recién exportado, pausar y cancelar. Con pruebas
   en CI contra el simulador.
2. **El servicio local de `tisma`** (estado, avance, controles, cámara, con vinculación) y **la app de Android** que
   lo sigue por WiFi: pantallas de Impresoras, Impresión y Controles. Prueba en un teléfono real (necesita al dueño).
3. **Las conexiones de red de la ficha de Device**, en la capa compartida (las usan la PC y el teléfono): primero
   Moonraker y OctoPrint (API pública y documentada), después PrusaLink y Duet, después las marcas.
4. **Cámara, archivos y avisos** en la app; la página Device completa en el escritorio.
5. **Laminar desde el teléfono, simple** (lo que EasyPrint hace con la nube, acá con el motor en el teléfono): elegir
   un modelo (STL o proyecto `.tisma`), la impresora y el filamento (del catálogo o de los perfiles guardados), una
   calidad (0,12 / 0,20 / 0,28 mm) y soportes sí o no; el motor lamina en el teléfono (es C++, va por el NDK, con el
   G-code verificado como siempre) y lo manda a la impresora. Sin servidor ni nube. A confirmar con el dueño cuando
   lleguen 1 a 4.

## Preguntas al dueño

- Qué teléfono y qué impresora se usan para probar (y si la impresora va por USB a la PC o tiene red).
- La cámara: la de la impresora, la del teléfono, o las dos.
- Instalación: APK directo, Play Store, o las dos.
