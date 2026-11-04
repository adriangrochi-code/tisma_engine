# Avance de la app de Android

Estado de la sesión de Android, para seguir sin depender de la conversación (decisión del dueño, 2026-11-04).

## Plan hasta la beta cerrada (dueño, 04-11-2026)

Corrección del dueño (04-11-2026, por la sesión de fichas): beta cerrada en diciembre de 2026 con lo que esté listo
(para arreglar cada error visto) y versión 1.0 en la segunda semana de enero de 2027. Las pruebas con hardware corren aparte y no frenan el software.

| Cuándo | Qué | Depende de |
|---|---|---|
| Noviembre, semanas 1 y 2 | ~~Impresión y Material~~ (hecho, 04-11-2026; Impresora, regla 8, también, salvo el estado de Calibración) — Impresión y Material con los datos del panel del escritorio (`settings-layout`); los niveles y la vista Personalizada (`mobile_app.md`, reglas 7 y 8, MA12) | `settings-layout` del motor (en curso) |
| Noviembre, semanas 2 y 3 | Paquete de perfiles: primer arranque y asistente (MA1), perfiles del paquete en la tarjeta de la impresora; ayuda sin conexión (regla 16) | `pack plan/install`, `knowledge` del motor |
| Noviembre, semana 4 | Ingeniería: Optimizar en el teléfono y en la nube; marcar caras fijas y de carga tocando la vista 3D (regla 13, MA6) | modos de `analyze` del motor |
| Diciembre, semana 1 | ~~Vista previa del G-code binario~~ (hecha, 04-11-2026) y ~~por tipo/ancho/velocidad~~ (hecha, 04-11-2026: `;WIDTH:` y la F de cada movimiento, rampa azul a rojo con mínimo y máximo); ~~vista previa en la vista 3D~~ (hecha, 04-11-2026: botón «3D», líneas por capa hasta la elegida, tope de 1,5 millones de tramos desde arriba; el dibujo no se probó en un teléfono) | — |
| Diciembre, semana 2 | API en C del motor por JNI en lugar del programa aparte, si está lista (avance, cancelar, memoria) | API en C del motor |
| Diciembre, semana 3 | Accesibilidad y uso táctil (`accessibility_touch.md`), tablets y horizontal, modo oscuro revisado; actualizaciones de la app (`app_updates.md`, regla 11) | — |
| Diciembre, semana 4 | Rendimiento medido en un teléfono de gama media (regla 18) y lo que haga falta ajustar; APK firmado para la beta | teléfono del dueño |
| Diciembre | Beta cerrada con lo que esté listo; cada error visto se arregla | — |
| Enero, semana 1 | Congelar funciones; solo errores; notas de la versión | — |
| Enero, semana 2 | Versión 1.0 | — |

Decisiones del dueño que siguen abiertas: inicio de sesión con Google y pagos de Google Play (bibliotecas con
licencia de Google), el identificador de la app, las herramientas de prueba con licencias fuera de la lista.

## Hecho

- **2026-11-04 — Impresión y Material (regla 7, MA12):** las pestañas salen del panel del escritorio
  (`tisma --settings-layout`, motor 63dee4f), guardado como `assets/settings/layout_{en,es}.json` por
  `tools/settings_layout.sh` (hay que volver a correrlo con un `tisma` de la máquina cuando cambia el panel; el CI no
  lo comprueba porque necesita X11). Hecho:
  - el chip de nivel (Básico, Normal, Avanzado, Personalizado, con «Elegir ajustes…»);
  - la búsqueda, que muestra marcado lo que oculta el nivel;
  - una sección por pantalla, con cada apartado como tarjeta y su «Restablecer»;
  - el punto violeta en la sección cambiada;
  - lo cambiado siempre a la vista, con su nivel en gris y la flecha atrás;
  - los interruptores de la regla 6 (salvo los del relleno no planar, que dependen de nombres de patrón que la ficha
    no da).
  El editor de cada fila sale del tipo y el rango: interruptor, número con tope, número o %, lista, texto; las listas
  y los objetos se ven pero se editan en el escritorio. Lo cambiado se guarda y va al motor sobre el perfil; la
  altura de capa y los soportes son los mismos chips de la Placa.
  - Impresora (regla 8; `settings_panel.md`, regla 37): la tarjeta «Configuración de la impresora» arriba de la
    sección Impresora abre el perfil (impresora, boquilla, filamento), Máquina, Boquilla y extrusor, Límites y G-code
    como el panel, y Notas (por impresora). Conexión es la propia sección (las impresoras de la red). Calibración
    (regla 38) espera las pruebas de calibración en el teléfono.
  - Pedido al escritorio: tipo y rango de `machine.variables.*` (hoy la app supone 0–600 °C), y las temperaturas de
    la primera capa en Básico.

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
  impresoras reales. En Duet, cancelar es M25 y M0 (confirmado en la referencia de G-code de RepRapFirmware:
  M0 cancela un trabajo en pausa y corre stop.g; sin probar en hardware); sus errores de G-code llegan después (`rr_reply`) y la app todavía no los lee.

- **2026-11-04 — Bambu Lab en modo LAN:** MQTT 3.1.1 propio sobre TLS (8883), el estado combinado de los informes,
  pausar, seguir, parar, controles por `gcode_line`, subir por FTPS implícito propio (990) como `.gcode.3mf` con su
  MD5 y empezar con `project_file`; la huella del certificado y el número de serie se confirman la primera vez y
  después se exigen (BL1 a BL3 contra un simulador propio con TLS). Abierto hasta probar con la impresora Bambu del dueño
  (Bambu Lab no publica el protocolo; consejo del equipo de fichas, 04-11-2026): la reanudación de la sesión TLS en la
  conexión de datos del FTPS (sin probar en hardware); `gcode_state`: solo IDLE está documentado, RUNNING y PAUSE se
  toman como imprimiendo y en pausa, cualquier otro valor se muestra tal cual; `mc_remaining_time` sin unidad
  documentada: el tiempo restante no se muestra. Sin la cámara de Bambu
  todavía (puerto 6000 y RTSP).

- **2026-11-04 — Ingeniería en el teléfono:** con un proyecto 3MF abierto en Placa, «¿Aguanta?» (`analyze`, con los
  apoyos, cargas y material del proyecto) y Aerodinámica (`aero`), con el motor en el teléfono, avance y cancelar.
  Probado con el motor de esta máquina: un cubo de 20 mm da Cd 1,16; sin condiciones, `needs_conditions` con su
  `error_es`. Marcar caras en el teléfono necesita la vista 3D (pendiente). Pedido a la sesión de escritorio: los
  modos de Optimizar por `analyze`.

- **2026-11-04 — memoria (MA9):** antes de laminar se cuentan los triángulos (STL, OBJ, 3MF) y se estima la memoria
  (150 MB más 3 kB por triángulo, valor de diseño a medir); si no alcanza la libre del teléfono, se ofrece la nube o
  probar igual. Se reemplaza por la estimación del motor cuando exista (su API en C).

- **2026-11-04 — vista 3D de la Placa** (`mobile_app.md`, regla 6): OpenGL ES 2.0 propio, la cama del perfil elegido
  como grilla con los ejes, el modelo (STL, OBJ, 3MF con componentes y transformaciones) parado en el centro; un dedo
  gira, dos mueven, pellizcar acerca, doble toque acerca; vistas 3D, arriba, frente, izquierda, derecha, atrás y
  «Encuadrar». Sin verlo en un teléfono (no hay emulador acá): probar que dibuje y que los gestos respondan.

- **2026-11-04 — cierre inesperado** (`problem_reports.md`, regla 10): al abrir después de un cierre por error, Informar un problema se abre con «Tisma se cerró» y el registro de esa sesión, una sola vez.
- **2026-11-04 — Créditos y fuentes** (`credits.md`): Más › Créditos y fuentes, con las funciones «Basado en» de
  `resources/credits.json` (los mismos datos del escritorio) y las bibliotecas del APK con su aviso (las del motor y
  las de la app, `credits_android.json`); una prueba exige un crédito para cada biblioteca que llega al APK.
- **Reglas nuevas (04-11-2026):** la app puede leer todo el código propio de Tisma (nunca PrusaSlicer ni Cura);
  preguntas de la API del servidor también a la sesión del servidor (session_01QadJfL79pTm8Ks4houx1oK). La sesión
  de escritorio aceptó `settings-layout` (en curso), el servicio local compatible con Moonraker (necesita ficha: la
  lista de pedidos se mandó al equipo de fichas) y las notas de comportamiento (mandadas).

- **Paquete de perfiles y ayuda (MA1, regla 16):** acordado con la sesión de escritorio que vive en el motor
  (`pack update/list`, `profile --pack`, `knowledge`), después de Optimizar; propuesto `pack plan` + `pack install`
  porque el motor en Android no tiene HTTPS (sin OpenSSL): la app baja los archivos y el motor los verifica e instala. Aceptado por el escritorio; el lado de la app (`ProfilePack`: plan, descarga en staging, install, dos rondas) está hecho y probado con un motor falso, falta conectarlo a una pantalla cuando el motor tenga los comandos.

## Abierto

- **Impresoras:** faltan Bambu Lab (MQTT con TLS y FTPS: necesita una biblioteca MQTT o una propia; decidir), PrusaLink,
  Duet, mover ejes y poner temperaturas a mano. La cámara se ve por imágenes fijas, no por video (MJPEG o RTSP). Si el motor suma conexiones de red a `src/device/`, se usan
  desde la app en lugar de las de Kotlin («usemos todo lo de la pc»).
- **Motor en el teléfono:** nunca corrió en un Android real (esta máquina no tiene KVM ni emulador); probado solo
  con un motor falso. Falta: perfiles de impresora y filamento (paquete de perfiles, `profile_pack.md`). La vista previa ya lee el G-code
  binario sin comprimir (`BinaryGcode`, 04-11-2026). El resumen de un modelo suelto ya llega (`--summary`, motor
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
