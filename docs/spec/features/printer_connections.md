# Conexión con impresoras

**Qué hace:** manda el G-code a la impresora e informa su estado:
- por USB (puerto serie);
- por la red local, a las impresoras Bambu Lab y a los programas que manejan impresoras (OctoPrint, Moonraker, la
  interfaz web de Prusa y la de Duet).

**Fase:** F9. La impresión por USB y la conexión con Bambu Lab las escribió el equipo del dueño. Para las demás,
Tisma Slicer usa código de PrusaSlicer: se escriben desde la documentación pública de cada API.

**Fuentes:**
- G-code por puerto serie, números de línea y suma de control: [wiki de RepRap, G-code](https://reprap.org/wiki/G-code)
  (secciones de comprobación y de respuestas), y la documentación de [Marlin](https://marlinfw.org/meta/gcode/).
- Bambu Lab en red local: [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI), documentación de la comunidad
  (MQTT, FTPS, video).
- [OctoPrint REST API](https://docs.octoprint.org/en/master/api/index.html),
  [Moonraker API](https://moonraker.readthedocs.io/en/latest/external_api/introduction/),
  [PrusaLink](https://github.com/prusa3d/Prusa-Link-Web/blob/master/spec/openapi.yaml) (su especificación OpenAPI
  pública) y [RepRapFirmware HTTP](https://github.com/Duet3D/RepRapFirmware/wiki/HTTP-requests).

## USB (puerto serie)

1. **Conexión:** se abre el puerto y se manda `M110 N0` para reiniciar los números de línea. Abrir el puerto puede
   reiniciar la placa, así que mientras conecta se repite `M110` cada tanto, hasta la primera respuesta.
2. **Cada línea** se manda como `N<n> <comando>*<suma>`. La suma es el XOR de todos los bytes de `N<n> <comando>`.
   Antes se sacan los comentarios y las líneas vacías.
3. **Ritmo:** la línea siguiente sale recién cuando la impresora contesta `ok`.
4. **Errores y esperas:**
   - `Resend: N` o `rs N`: se vuelve a mandar desde la línea N;
   - `busy:` mantiene viva la conexión durante comandos largos;
   - si no llega ninguna respuesta en un rato (un `ok` perdido), se manda `M105` para provocar una.
5. **Temperaturas:**
   - se leen de los reportes `T:… B:…`;
   - se pide el reporte automático cada 2 s (`M155 S2`); si el firmware no lo tiene, se pregunta con `M105`
     periódicamente.
6. **Cancelar:** se interrumpe el calentamiento (`M108`) y se mandan los comandos de cancelación del perfil.
7. **Comandos de la persona:** se intercalan en cuanto se puede.
8. **Firmwares:** Marlin, el firmware de Prusa (MK2, MK3, MINI y MK4 por USB) y RepRapFirmware.
9. **Detección:** se recorren los puertos serie y las velocidades habituales (115200 y 250000 baudios).

## Bambu Lab en red local

1. **Requisitos en la impresora:** modo «solo LAN» y modo desarrollador. Desde el firmware de enero de 2025, el
   control de terceros solo funciona así. Hace falta el código de acceso que muestra la pantalla en ese menú.
2. **Estado:**
   - MQTT 3.1.1 sobre TLS, puerto 8883, usuario `bblp`, contraseña = el código de acceso;
   - se publica `pushall` en `device/<serie>/request` y se lee `device/<serie>/report`: estado, porcentaje, tiempo
     restante, temperaturas.
3. **Número de serie:** no se pide; es el nombre (CN) del certificado de la impresora.
4. **Certificado:**
   - la impresora presenta un certificado de la autoridad de Bambu Lab;
   - Tisma Slicer verifica la cadena con esa autoridad, tomada de código ajeno; el motor no la copia;
   - el slicer propio guarda la huella del certificado la primera vez que se conecta, después de que la persona la
     confirme, y después exige la misma huella;
   - el nombre no se comprueba: es el número de serie, no la IP.
5. **Enviar e imprimir:**
   - el G-code se empaqueta como `<nombre>.gcode.3mf`, con `Metadata/plate_1.gcode` y su MD5;
   - se sube por FTPS implícito (puerto 990) a la raíz de la tarjeta;
   - se inicia con el comando `project_file` por MQTT;
     sus campos como los manda la app de Android, que imprime en una A1 mini real (decisión de QA, 17-11-2026):
     `md5` es el MD5 del G-code de la placa (`Metadata/plate_1.gcode`), no el del paquete; `ams_mapping` va como texto
     vacío `""` sin AMS; no se agrega el campo `file`; `url` es `ftp:///<nombre>.gcode.3mf`. Escritorio, Android e
     iPhone mandan el mismo `project_file` (prueba BL4);
   - el servidor FTP exige que la conexión de datos reanude la sesión TLS de la de control. Con TLS 1.3, algunas
     bibliotecas no la reanudan; con TLS 1.2 alcanza el identificador de sesión.
6. **Cámara (A1 y P1):** TLS al puerto 6000, un paquete de autenticación de 80 bytes y después imágenes JPEG seguidas,
   como describe OpenBambuAPI.
7. **El perfil** de la impresora tiene que producir G-code de texto, no binario.
8. **El código de acceso es un secreto:** se guarda como una contraseña, nunca se escribe en registros ni en mensajes.

## Leer los límites de la impresora

**Pedido del dueño (28-10-2026):** si al perfil le faltan datos y la impresora está conectada, se le preguntan al
firmware y se pasan a la máquina.

**Fuentes:**
- Marlin: [M503](https://marlinfw.org/docs/gcode/M503.html) informa los valores en uso (los de la memoria, que pueden
  no ser los guardados). La página de [M203](https://marlinfw.org/docs/gcode/M203.html) remite a M503 para ver las
  velocidades máximas. [M115](https://marlinfw.org/docs/gcode/M115.html) da el firmware, la cinemática, la cantidad de
  extrusores y, si el firmware se compiló con `M115_GEOMETRY_REPORT`, el volumen de trabajo (mínimo y máximo por eje).
  La página de M503 no muestra el formato de la respuesta: se toma de respuestas reales, guardadas como prueba;
- Klipper (Moonraker): el objeto [`toolhead`](https://moonraker.readthedocs.io/en/latest/printer_objects/) trae
  `max_velocity`, `max_accel`, `square_corner_velocity`, `minimum_cruise_ratio`, `axis_minimum` y `axis_maximum`. El
  objeto `configfile` trae la configuración en `settings`;
- RepRapFirmware: pendiente de confirmar en la documentación de Duet3D antes de escribirlo.

1. **Cuándo:** al conectar una impresora (USB o red), si el perfil tiene algún dato vacío de los de abajo. También con
   un botón «Leer de la impresora» en Impresora › Límites, que lee todo aunque el perfil ya tenga datos.
2. **Qué se pide:**
   - Marlin por USB: `M115` y `M503`;
   - Klipper: el objeto `toolhead` y `configfile.settings`.
   La impresión en curso no se interrumpe: si la impresora está imprimiendo, se pregunta al terminar.
3. **Qué se toma:**

   | Dato del perfil | Marlin (`M503`) | Klipper |
   |---|---|---|
   | Velocidad máxima X, Y, Z, E | `M203 X Y Z E` (mm/s) | `max_velocity` (X e Y); Z y E de `configfile.settings` |
   | Aceleración máxima X, Y, Z, E | `M201 X Y Z E` (mm/s²) | `max_accel` |
   | Aceleraciones de impresión, retracción y viaje | `M204 P R T` | `max_accel` |
   | Tirones | `M205 X Y Z E` | `square_corner_velocity` |
   | Volumen de trabajo | `M115` con informe de geometría | `axis_minimum` y `axis_maximum` |
   | Avance de presión | `M900 K`, si aparece | `pressure_advance` del extrusor en `configfile.settings` |

4. **Sin pisar:** un dato que la persona escribió no se cambia solo. Los datos leídos se muestran en una lista («En el
   perfil» / «En la impresora») para aceptar uno por uno o todos. Los datos vacíos se llenan solos, con un aviso de una
   línea que dice cuántos se tomaron de la impresora.
5. **Origen a la vista:** cada dato tomado de la impresora lo dice en su ayuda: «Leído de la impresora (Marlin 2.1.2,
   28-10-2026)».
6. **Respuesta incompleta:** un valor que no llega (por ejemplo, Marlin sin informe de geometría o con `M503`
   desactivado) queda vacío, y la página lo dice: «La impresora no informa el tamaño de la cama: escribilo a mano».
   Nunca se escribe 0 en lugar de un dato que falta.
7. **Unidades:** se pasan a las del perfil. Marlin informa en las unidades en uso (`G21` mm, `G20` pulgadas).
8. **Por red, según la marca** (pedido del dueño: «leer más datos en la mayoría de las impresoras»):

   | Conexión | Qué se puede leer | Fuente |
   |---|---|---|
   | Klipper (Moonraker) | Todo lo de la regla 3 | [printer_objects](https://moonraker.readthedocs.io/en/latest/printer_objects/) |
   | OctoPrint (Marlin detrás) | `M115` y `M503` se mandan con `POST /api/printer/command`; la respuesta no viene en el pedido, llega por los mensajes en vivo (push) de OctoPrint, que el programa escucha | [API de OctoPrint](https://docs.octoprint.org/en/master/api/printer.html) |
   | Elegoo (SDCP 3.0) | `Cmd 1` vuelve a publicar los atributos; trae `XYZsize` (tamaño máximo de impresión, mm). No trae velocidades ni aceleraciones | [SDCP V3.0.0](https://github.com/cbd-tech/SDCP-Smart-Device-Control-Protocol-V3.0.0), de Shenzhen CBD Technology |
   | Bambu Lab (LAN) | Ningún límite de movimiento: el estado (`pushall`) trae `nozzle_diameter`, temperaturas y AMS, y `get_version` las versiones de cada módulo. `gcode_line` no devuelve la salida del comando. Los límites salen del perfil del catálogo | [OpenBambuAPI](https://github.com/Doridian/OpenBambuAPI) (comunidad; Bambu Lab no publica el protocolo) |
   | RepRapFirmware, PrusaLink, Creality, Flashforge | Pendiente: confirmar en la documentación de cada uno qué informa y cómo | — |

   Lo que una conexión no puede leer se dice en Límites: «Esta impresora no informa sus límites por red: vienen del
   perfil».

## Programas que manejan impresoras (red)

Para OctoPrint, Moonraker, la interfaz web de Prusa y la de Duet, el slicer hace cuatro cosas, cada una con la
llamada que describe la documentación oficial de esa API:
1. probar la conexión y las credenciales (clave de API, usuario y contraseña, o contraseña de la placa);
2. subir el archivo;
3. empezar la impresión, si se pidió;
4. leer el estado.

No se copian llamadas de otro programa: se leen de la documentación y se prueban contra un simulador propio.

## Búsqueda en la red

Para el ícono de conexión de `settings_panel.md` (reglas 41 a 43): el programa encuentra solo las impresoras de la red
local. Encontrar no conecta: conectar lo pide la persona.

**Fuentes:**
- Bambu Lab: la tabla de puertos de su wiki oficial (`https://wiki.bambulab.com/en/bambu-studio/manual/pin-code`):
  «Device discovering», puertos 1990 y 2021, SSDP por multicast o broadcast, en modo nube y en modo LAN;
- OctoPrint: la documentación oficial de su complemento Discovery
  (`https://docs.octoprint.org/en/master/bundledplugins/discovery.html`);
- Moonraker: la documentación oficial de su configuración, sección `[zeroconf]`
  (`https://moonraker.readthedocs.io/en/latest/configuration/`).

| Marca o programa | Cómo se anuncia | Qué hace el programa |
|---|---|---|
| Bambu Lab | Anuncios SSDP a los puertos UDP 1990 y 2021, con campos propios para el modelo y el nombre de la impresora (su forma exacta se mide con una impresora real) | Escucha esos puertos; muestra cada impresora una vez por el identificador del anuncio |
| OctoPrint | Zeroconf (mDNS y DNS-SD) con los servicios `_http._tcp` y `_octoprint._tcp`; el registro TXT trae `path`, `version`, `api` y, si los hay, `model` y `vendor` | Busca `_octoprint._tcp`; la dirección del servidor es la del servicio más `path` |
| Moonraker (Klipper) | mDNS con el nombre de host de `mdns_hostname` (por defecto el del sistema); SSDP solo si `enable_ssdp` está encendido | Busca el servicio DNS-SD `_moonraker._tcp` (el único que anuncia; no `_http._tcp`), con el nombre «<Nombre> @ <host>», el puerto HTTP de Moonraker (7125 por defecto) y el TXT `uuid`, `https_port` (vacío sin HTTPS), `version` y `route_prefix` (prefijo de las rutas de la API); con SSDP, el tipo `urn:arksine.github.io:device:Moonraker:1`. En cada uno pregunta `GET <route_prefix>/server/info` en el puerto anunciado; si contesta, es una impresora Klipper. Sin `[zeroconf]` no se anuncia: queda la IP a mano. Fuente: comportamiento de Moonraker observado por el equipo de especificación (la documentación no lo dice) |
| Otras (Prusa, Duet, Elegoo, Creality con firmware propio, Flashforge) | Sin fuente oficial revisada | Se agregan a mano; se suman a esta tabla cuando haya documentación del fabricante |

1. **Escucha y búsqueda:**
   - empiezan al abrir el programa, si la preferencia «Buscar impresoras en la red» está encendida (`preferences.md`);
   - se repite la búsqueda activa cada 30 s mientras el menú de conexión está abierto, y cada 5 minutos si no.
2. **Una impresora una sola vez:** se identifica por el identificador del anuncio (Bambu Lab) o por su dirección y
   puerto. Si aparece por dos caminos (por ejemplo, OctoPrint por `_http._tcp` y `_octoprint._tcp`), se muestra una vez.
3. **Desaparece** de la lista si pasan 3 minutos sin anuncio ni respuesta. Una impresora ya conectada no desaparece:
   su estado lo da la conexión.
4. **Ligar al modelo:** si el anuncio dice el modelo (Bambu Lab, `model` de OctoPrint) y coincide con una impresora
   del paquete de perfiles, el menú la ofrece para ese modelo; si no, aparece en «Otras impresoras de la red».
5. **Puertos ocupados:** solo un programa a la vez puede escuchar el puerto UDP 2021. Si otro programa lo tiene (por
   ejemplo, Bambu Studio abierto), se avisa «Otro programa está escuchando las impresoras Bambu Lab; cerralo o
   agregá la impresora por su dirección», y se puede agregar a mano.
6. **Problemas de la red:** si no aparece nada, el menú explica las causas comunes: computadora e impresora en redes
   distintas, cortafuegos que bloquea UDP 1990 y 2021, o en Windows la red marcada como «Pública».
7. **Nada sale de la red local:** la búsqueda no usa internet ni manda datos afuera.

## Cámara USB de la computadora

Lo diseñó el equipo del dueño. Una cámara USB (UVC) conectada a la computadora sirve de cámara de una impresora: para
las que no tienen, o tienen una lenta (las A1 y P1 de Bambu Lab mandan pocas imágenes por segundo por la red local).

1. **Elección:** en Dispositivos, cada impresora elige «La cámara de la impresora» (si tiene), «Sin cámara» o una de
   las cámaras USB conectadas. Se guarda por impresora y vuelve sola al abrir el programa. Al lado se ve lo que da la
   cámara: tamaño, formato e imágenes por segundo reales.
2. **Formato:**
   - el más grande hasta 1280 × 720 que dé al menos 10 imágenes por segundo;
   - MJPEG antes que sin comprimir: usa menos ancho de banda USB, y entra una segunda cámara en el mismo puerto.
3. **En pantalla:**
   - hasta 15 imágenes por segundo;
   - las que la interfaz no alcanza a dibujar se saltean, no se acumulan;
   - si la cámara se desconecta o pasa 10 s sin mandar imágenes, se avisa en la vista.
4. **Privacidad:** las imágenes quedan solo en memoria: no se guardan ni salen de la computadora.
5. **Dónde se ve:** en una impresora USB, en su panel; en una de red, debajo de sus datos (en las Bambu Lab, en lugar
   de su cámara).
6. **Bibliotecas:** una opción con licencia MIT es [openpnp-capture](https://github.com/openpnp/openpnp-capture). Usa
   DirectShow en Windows, AVFoundation en macOS y V4L2 en Linux. Se anota en `THIRD_PARTY.md` antes de usarla.
7. **macOS:** el programa declara para qué usa la cámara y pide el permiso de cámara. Sin eso, macOS lo cierra al
   abrirla.

## Cámara de la impresora

1. **En el menú de la impresora** (pedido del dueño, 28-10-2026): la cámara de la impresora elegida también se ve en
   la página Impresora de la barra de arriba, arriba a la derecha, con su estado («Conectando…», «Sin cámara», el
   error). Un clic la agranda. Si la impresora no tiene cámara o no se puede leer, se dice por qué.
2. **Cámara de cada marca:**

   | Conexión | Cómo se obtiene | Fuente |
   |---|---|---|
   | Bambu Lab A1 y P1 | TCP al puerto 6000, imágenes JPEG de 1280 × 720 (regla 6 de Bambu Lab) | [OpenBambuAPI, video](https://github.com/Doridian/OpenBambuAPI/blob/main/video.md) (comunidad) |
   | Bambu Lab X1 y P2S | `rtsps://<IP>:322/streaming/live/1`, usuario `bblp`, el código de acceso como contraseña | la misma |
   | Bambu Lab H2S y H2D | Igual que X1, pero apagada de fábrica: se prende en la pantalla, «LAN Only Liveview». Si está apagada, el estado trae `ipcam.rtsp_url = "disable"` y el programa lo explica | la misma |
   | Klipper (Moonraker) | `GET /server/webcams/list`: `stream_url` y `snapshot_url` de cada cámara, con giro y rotación | [webcams](https://moonraker.readthedocs.io/en/latest/external_api/webcams/) |
   | OctoPrint | Ajustes: `webcam.streamUrl` y `webcam.snapshotUrl` | [settings](https://docs.octoprint.org/en/master/api/settings.html) |
   | Elegoo (SDCP 3.0) | `Cmd 386` con `Enable` 1; la respuesta trae `VideoUrl` (RTSP). Código 1: «se superó el máximo de transmisiones» | [SDCP V3.0.0](https://github.com/cbd-tech/SDCP-Smart-Device-Control-Protocol-V3.0.0) |
   | Prusa | Sin transmisión local documentada; Prusa Connect solo tiene la API para subir fotos | [Prusa Connect camera API](https://help.prusa3d.com/article/prusa-connect-camera-api_569012) |
   | Otras | Pendiente | — |

## Botón principal

- **Prueba de conexión:** si la impresora elegida tiene una impresora de red guardada, cada 20 s se prueba si se puede
  abrir una conexión TCP con su dirección. El puerto es el de la dirección, o el del tipo: 8883 Bambu Lab, 8080 MKS,
  80 las interfaces web.
- **Si responde:** después de laminar, el botón principal pasa de «Exportar» a «Enviar a la impresora».
- **Si deja de responder:** vuelve a «Exportar».
- **Con varias impresoras de red para el mismo perfil:** se usa la última a la que se envió.

## Integración en sala limpia

Pedido del dueño (26-10-2026): integrar la conexión con impresoras respetando la sala limpia.

**El problema:** en Tisma Slicer, cada conexión salió de un lugar distinto. El motor no puede tomar ninguno de esos
códigos:

| Conexión | De dónde salió en Tisma Slicer | Por qué el motor no lo usa |
|---|---|---|
| OctoPrint, PrusaLink, Duet, AstroBox, Repetier, FlashAir, MKS | Código de PrusaSlicer | AGPL, programa prohibido (`CLEANROOM.md`) |
| Moonraker (Klipper) | Código de PrusaSlicer | Ídem |
| Flashforge | Código de OrcaSlicer, adaptado (`docs/FLASHFORGE.md` de Tisma Slicer lo dice) | Ídem |
| USB, Bambu Lab en red local, cámara USB | Escrito por el equipo del dueño dentro de Tisma Slicer | Es código de Tisma Slicer: tampoco se lee |
| Simuladores de prueba de Tisma Slicer (hosts web, Bambu Lab, Flashforge) | Escritos por el equipo del dueño | Son código de Tisma Slicer |

**Fuentes que sí puede usar el motor:** documentación de protocolos (no código) y medición de caja negra.

| Conexión | Fuente para el motor | Tipo | Falta |
|---|---|---|---|
| USB (serie) | G-code de la wiki de RepRap y documentación de Marlin | Oficial | Nada |
| OctoPrint | REST API y complemento Discovery | Oficial | Nada |
| Moonraker | Su API y su configuración | Oficial | Nada |
| PrusaLink | Su especificación OpenAPI publicada | Oficial | Nada |
| Duet (RepRapFirmware) | Pedidos HTTP de su wiki | Oficial | Nada |
| Repetier-Server | Su Web API | Oficial | Nada |
| Bambu Lab | Puertos: wiki oficial de Bambu Lab. Protocolo: OpenBambuAPI | Oficial y comunidad (documentación) | Forma exacta del anuncio SSDP, medida con una impresora |
| Flashforge | ff-5mp-api-py, `docs/protocols.md` (MIT, documentación de la comunidad) | Comunidad | Ver la nota de abajo |
| Elegoo (SDCP), Creality con firmware propio, Prusa Connect, FlashAir, MKS | Ninguna revisada | — | Documentación del fabricante; hasta entonces no se ofrecen |

**Nota de Flashforge:** las dos fuentes no coinciden. Tisma Slicer usa en el puerto 8898 los pedidos «detalle» y
«subir G-code» con el número de serie y el código en los encabezados. La documentación de la comunidad lista otros
nombres (`/info`, `/upload`, `/command`…) con `serialNumber` y `checkCode` en el cuerpo JSON. Puede ser por la versión
del firmware. Hay que medirlo con una impresora real (regla 4) antes de escribir la regla.

**Cómo se hace:**

1. **Ficha primero:** cada conexión tiene sus reglas en esta ficha, escritas desde las fuentes de la tabla: pedidos,
   respuestas, errores, tiempos de espera y secretos. Sin nombres de clases ni de archivos de otro programa.
2. **Simulador propio del motor:** el equipo del motor escribe sus simuladores desde la documentación y esta ficha.
   No usa los de Tisma Slicer.
3. **Intercambios grabados (datos, no código):** el equipo que conoce Tisma Slicer graba, con impresoras reales o con
   Tisma Slicer como caja negra, lo que va y viene por la red en cada caso de prueba. Lo deja en
   `printer_connections/<conexión>/<caso>.jsonl`, una línea por mensaje:

```json
{"n": 1, "dir": "out", "proto": "http", "method": "GET", "path": "/api/version", "headers": {"X-Api-Key": "<secreto>"}}
{"n": 2, "dir": "in", "proto": "http", "status": 200, "body": {"api": "0.1", "server": "1.10.2", "text": "OctoPrint 1.10.2"}}
```

   - los secretos (claves, contraseñas, códigos de acceso, números de serie) se reemplazan por `<secreto>` antes de
     guardar, y nunca se graba un código de acceso real de una impresora del dueño;
   - los archivos subidos se reemplazan por su tamaño y su suma SHA-256;
   - cada archivo dice de dónde salió: impresora real (modelo y firmware) o Tisma Slicer contra su simulador.
4. **Medir lo que no está documentado:** con una impresora real (Flashforge, el anuncio de Bambu Lab), se graban los
   intercambios de la regla 3 y la regla de la ficha se escribe desde esa grabación.
5. **Prueba cruzada:** el motor, contra los intercambios grabados (reproducidos por su simulador) y, cuando haya,
   contra impresoras reales. El equipo que conoce Tisma Slicer corre además el motor contra los simuladores de Tisma
   Slicer como caja negra e informa las diferencias, como en `docs/spec/qa/`.
6. **Nunca:** las bibliotecas cerradas de los fabricantes (por ejemplo, el complemento de red de Bambu Studio), ni
   código de otro laminador o de Tisma Slicer, aunque el protocolo sea el mismo.

## Lo que usa la app de Android (04-11-2026)

De la documentación pública de la tabla de arriba y de los simuladores de la app; **sin probar en hardware**. Lo
mandó la sesión de Android (solo comportamiento).

1. **Servicio local de la computadora para impresoras USB:** un subconjunto compatible con Moonraker, el mismo que la
   app ya usa con Klipper. JSON por HTTP; las respuestas van en `{"result": …}` y los errores en
   `{"error": {"code", "message"}}` con su código HTTP; el código de emparejamiento va como `X-Api-Key` (401 si no
   coincide).
   - `GET /server/info`: `moonraker_version` y `klippy_state` (`ready`, `startup`, `shutdown`, `error`).
   - `GET /printer/objects/query?print_stats&virtual_sdcard&extruder&heater_bed&webhooks`: `webhooks.state` y
     `state_message`; `print_stats.state` (`standby`, `printing`, `paused`, `complete`, `cancelled`, `error`),
     `filename`, `print_duration` (s), `message`, `info.current_layer`, `info.total_layer`; `virtual_sdcard.progress`
     (0 a 1, por la posición en el archivo); `extruder` y `heater_bed` con `temperature` y `target` (°C). La app
     pregunta cada 2 s; el tiempo restante es `print_duration / progress − print_duration`.
   - `POST /printer/print/pause`, `/resume`, `/cancel`: `"ok"`; 400 con el motivo si no está imprimiendo o en pausa.
   - `POST /server/files/upload` (multipart: `root=gcodes`, `print=true` opcional, `file`; con `Content-Length`):
     201 con `item.path`, `item.root`, `print_started` y `action`.
   - `POST /printer/print/start?filename=…`: `"ok"`; 404 si el archivo no existe.
   - `POST /printer/gcode/script?script=…` (líneas separadas por `\n`): `"ok"`; 400 con el motivo si se rechaza
     (movimientos o extrusión mientras imprime; «Extrude below minimum temp»). La app manda `M104 S`, `M140 S`,
     `G28`, `G91`/`G1`/`G90` para mover y `M83`/`G1 E` para extruir.
   - `GET /server/webcams/list`: `webcams` con `name`, `enabled`, `stream_url` y `snapshot_url` (relativas al host);
     la app pide una foto JPEG por segundo.
   - Búsqueda: `_moonraker._tcp` y `_http._tcp` por DNS-SD, y se confirma con `/server/info`.
2. **OctoPrint:** `X-Api-Key`. `GET /api/printer` da 409 sin conexión con la placa: se muestra desconectada con ese
   texto. Estado por `state.flags`; avance por `GET /api/job` (`completion`, `printTime`, `printTimeLeft`). Pausa y
   reanudar: `POST /api/job` con `command: "pause"` y `action`; cancelar con `command: "cancel"` (204; 409 si no
   imprime). Subir: `POST /api/files/local` con `select` y `print`. Temperaturas: `/api/printer/tool` (`target`) y
   `/api/printer/bed`; mover y llevar a cero: `/api/printer/printhead`; extruir: `/api/printer/tool` (`extrude`).
   Cámara: `webcam.streamUrl` y `snapshotUrl` de `GET /api/settings`. Búsqueda: `_octoprint._tcp` con `path` en el TXT.
3. **PrusaLink:** HTTP Digest (RFC 7616, MD5, `qop=auth`; usuario `maker`, la contraseña en la impresora).
   `GET /api/v1/status`: `printer.state` (`IDLE`, `BUSY`, `PRINTING`, `PAUSED`, `FINISHED`, `STOPPED`, `ERROR`,
   `ATTENTION`, `READY`), temperaturas, `job.progress`, `time_printing`, `time_remaining`. El trabajo y el nombre del
   archivo salen de `GET /api/v1/job` (204 sin trabajo). Pausa y reanudar: `PUT /api/v1/job/{id}/pause|resume`;
   detener: `DELETE /api/v1/job/{id}`. Subir: `PUT /api/v1/files/{almacén}/{nombre}` con `Content-Length`,
   `Print-After-Upload: ?1` y `Overwrite: ?1` (el almacén es el primero disponible de `GET /api/v1/storage`). La v1
   no tiene controles a mano ni cámara.
4. **Duet (RepRapFirmware):** `GET /rr_connect?password=…&sessionKey=yes` (`err` 0, 1 contraseña mal, 2 sin
   sesiones libres); `sessionKey` va como `X-Session-Key` en cada pedido y un 401 reconecta una vez. Estado con
   `rr_model` (`key=state`, `key=job`: archivo, tamaño, posición, duración, `timesLeft`, capas; `key=heat.heaters`
   con la cama y la herramienta 0). Subir: `POST /rr_upload?name=0:/gcodes/<nombre>` (cuerpo crudo, `err` 0).
   Imprimir `M32 "<ruta>"`, pausa `M25`, reanudar `M24`, cancelar `M25` y después `M0` (corre `stop.g`, según la
   referencia de G-code de Duet3D). Los errores de G-code no vuelven en `rr_gcode`: llegan después por `rr_reply`.
5. **Bambu Lab por la red local:** el fabricante no publica la especificación. Los valores de `gcode_state`, la unidad
   de `mc_remaining_time` y la reutilización de la sesión TLS del FTPS quedan **sin confirmar** hasta medirlos con una
   impresora real (regla 4 de «Integración en sala limpia»); un estado desconocido se muestra tal cual.

## Pruebas

Contra simuladores propios de cada impresora (un programa que contesta como ella), sin impresora real.

| # | Escenario | Resultado esperado |
|---|---|---|
| U1 | Simulador USB que contesta `ok` a todo | Las líneas salen numeradas, con su suma, una por `ok` |
| U2 | El simulador pide `Resend: 5` en la línea 7 | Se vuelve a mandar desde la 5 y el archivo termina completo |
| U3 | El simulador se queda sin contestar un `ok` | Al rato se manda `M105` y la impresión sigue |
| U4 | El simulador manda reportes `T:210.0 /210.0 B:60.0 /60.0` | El estado muestra boquilla 210 °C y cama 60 °C |
| U5 | Cancelar a mitad de la impresión | Se manda `M108` y después los comandos de cancelación |
| BL4 | El mismo `.gcode.3mf` mandado desde el escritorio y desde la app de Android al simulador | El `project_file` que recibe el simulador es igual campo por campo en los dos |
| BL1 | Simulador Bambu (MQTT con TLS + FTPS) con código de acceso de prueba | Lee estado, porcentaje y temperaturas; sube el `.gcode.3mf` y manda `project_file` |
| BL2 | El mismo con un código de acceso equivocado | Error de autenticación claro, sin escribir el código en ningún registro |
| BL3 | Certificado distinto del guardado | No se conecta y se avisa |
| N1 | Simulador de cada interfaz web | Probar, subir y empezar funcionan con credenciales buenas; error claro con credenciales malas |
| CA1 | Cámara simulada que ofrece MJPEG de 1920 × 1080, 1280 × 720 y 640 × 480 a 30 imágenes por segundo | Elige 1280 × 720 MJPEG; muestra 15 imágenes por segundo; la elección vuelve al reiniciar |
| CA2 | Sin cámaras conectadas | La lista ofrece solo «Sin cámara» (y la de la impresora, si tiene) |
| CA3 | La cámara deja de mandar imágenes | A los 10 s, aviso en la vista |
| BD1 | Anuncio SSDP simulado de una Bambu Lab al puerto 2021 | La impresora aparece en el menú de conexión con su modelo y nombre, una sola vez aunque el anuncio se repita |
| BD2 | Servicio `_octoprint._tcp` simulado con `path` `/octo/` | Aparece con la dirección del servicio más `/octo/` |
| BD3 | Puerto 2021 ocupado por otro programa | Aviso de la regla 5; agregar a mano sigue funcionando |
| BD4 | El anuncio deja de llegar | A los 3 minutos sale de la lista, salvo que esté conectada |
| CL1 | Reproducir `printer_connections/octoprint/probar.jsonl` contra el cliente del motor | Los pedidos del motor coinciden con las líneas `out` (método, ruta y encabezados, con los secretos aparte) |
| CL2 | Revisar los archivos de `printer_connections/` | Ningún secreto real: solo `<secreto>` |
| LM1 | Marlin por USB, perfil sin velocidades máximas; respuesta guardada de `M503` con `M203 X500 Y500 Z12 E120` | El perfil queda con 500, 500, 12 y 120 mm/s y el aviso «4 datos tomados de la impresora» |
| LM2 | Lo mismo, con la velocidad X escrita a mano (300) | X sigue en 300; la lista muestra «En el perfil 300 / En la impresora 500» para elegir |
| LM3 | Marlin sin informe de geometría | Tamaño de cama vacío, con el aviso de la regla 6; nunca 0 |
| LM4 | Klipper, `toolhead` con `max_velocity` 300 y `max_accel` 3000 | Velocidad máxima X e Y 300 mm/s; aceleración 3000 mm/s² |
| LM5 | Impresora imprimiendo al conectar | No se manda nada hasta que termina |
| LM6 | Bambu Lab por LAN, perfil sin velocidades | Límites del perfil sin cambios y el aviso «Esta impresora no informa sus límites por red» |
| LM7 | Elegoo SDCP, simulador con `XYZsize` «256x256x256» | Volumen 256 × 256 × 256 mm si el perfil no lo tenía |
| CM1 | Klipper con una cámara en `/server/webcams/list` | La cámara se ve en la página Impresora |
| CM2 | Bambu Lab H2D con `ipcam.rtsp_url = "disable"` | «La cámara está apagada en la impresora: prendé LAN Only Liveview en su pantalla» |

## Flashforge en red local (decisión del dueño, 17-11-2026: para la beta del 24-11)

Esta sección describe solo comportamiento observable: no tiene código. Las fuentes son comunitarias salvo donde dice
"oficial". Siglas de las fuentes:
- [W-HTTP] https://github.com/Parallel-7/flashforge-api-docs/wiki/HTTP-REST-API (comunitaria)
- [W-AUTH] https://github.com/Parallel-7/flashforge-api-docs/wiki/Authentication (comunitaria)
- [W-DISC] https://github.com/Parallel-7/flashforge-api-docs/wiki/Discovery-Protocol (comunitaria)
- [W-MCODE] https://github.com/Parallel-7/flashforge-api-docs/wiki/M-Code-Reference (comunitaria)
- [FF-C5] https://wiki.flashforge.com/en/creator-series/creator-5-series/manual/network-setup (oficial, manual de la Creator 5)
- [SP] https://simplyprint.io/setup-guide/flashforge/adventurer-5m.md (de terceros)

No pude leer `GhostTypes/ff-5mp-api/docs/protocols.md` (GitHub devolvió 503), así que no se usó.

### Familias de modelos
- **Modernas:** Adventurer 5M, 5M Pro, AD5X y Creator 5. Usan HTTP en el puerto 8898 para estado, subida y control.
  También tienen TCP en el 8899. [W-HTTP] [W-DISC]
- **Legado:** Adventurer 3 y Adventurer 4 (también la 4 Pro). Solo tienen TCP en el 8899, sin HTTP. [W-AUTH] [W-DISC]
- **Guider:** sin confirmar. Ninguna fuente consultada la menciona. Hasta que haya un dato, el programa la trata como
  legado y lo dice en la interfaz.

### Descubrimiento
- El programa manda un paquete UDP cualquiera, porque la impresora ignora el contenido. Para las modernas lo manda al
  grupo multicast 225.0.0.9, puerto 19000, y en broadcast al puerto 48899. Para las de legado lo manda al mismo grupo,
  puerto 8899. [W-DISC]
- Las modernas responden con 276 bytes (firmware 3.x y Creator 5) o 280 bytes (firmware 5.x). Los números van en big
  endian. Campos de la respuesta: nombre (128 B), puerto TCP (normalmente 8899), VID, PID, estado de la sesión TCP,
  tipo de producto, puerto HTTP (normalmente 8898), modo de conexión (0 = solo LAN, 1 = nube), vínculo con la nube y
  número de serie (128 B). Una respuesta de 196 bytes o más se considera moderna. [W-DISC]
- Las de legado responden con exactamente 140 bytes: nombre, puerto, VID, PID y estado (0 listo, 1 ocupada,
  2 error). La respuesta no trae número de serie; se pide por TCP con M115. [W-DISC]
- Si el descubrimiento no encuentra nada (red de invitados, VLAN o bandas separadas, según
  https://printago.io/guides/flashforge-orca-printer-offline, de terceros), el usuario puede cargar la IP a mano.

### Qué carga el usuario
- **IP:** se muestra en la pantalla de la impresora. [SP]
- **Número de serie:** está en Ajustes, en la sección Red, Acerca de o Info del dispositivo, y suele empezar con "SN".
  Si la impresora se encontró por descubrimiento, el programa lo completa solo. [W-AUTH] [W-DISC]
- **Código de verificación** ("Check Code" o "código de acceso"), solo en las modernas: se ve en la pantalla de modo
  de red, la misma donde se activa "Solo LAN". [W-AUTH] [SP]
  - En la Creator 5 la ruta es Red > Solo LAN. [FF-C5] (oficial)
  - En la 5M, la ruta exacta del menú está sin confirmar en una fuente oficial.
  - En la 5M y la AD5X son 8 caracteres de 0–9 y a–f. Si se genera uno nuevo, el anterior deja de valer. [W-AUTH]
- Las de legado no piden credenciales. [W-AUTH]

### HTTP (modernas, puerto 8898)
- Todas las llamadas son POST y toda respuesta trae `code` y `message`. [W-HTTP]
- Credenciales: casi todas las llamadas llevan `serialNumber` y `checkCode` en el cuerpo JSON. En `/uploadGcode` van en
  cabeceras. [W-HTTP]
- **Estado (`/detail`):** devuelve `status`, temperaturas, progreso y tiempos. Valores de `status`: ready, busy,
  calibrate_doing, error, heating, printing, pausing, pause, canceling, cancel, completed, downloading, loading,
  cloud_slicing, sending y unzipping. Un valor desconocido se trata como ocupada. [W-HTTP]
  - `/product` informa qué funciones tiene la impresora, pero no sirve para detectar el hardware. [W-HTTP]
- **Subida (`/uploadGcode`):** multipart, con el archivo en el campo `gcodeFile`. [W-HTTP]
  - Cabeceras: `serialNumber`, `checkCode`, `fileSize` (tamaño en bytes), `printNow` (imprimir al terminar) y
    `levelingBeforePrint` (nivelar antes). [W-HTTP]
  - Otras cabeceras: `flowCalibration`, `firstLayerInspection` y `timeLapseVideo`. [W-HTTP]
  - Solo AD5X: `useMatlStation`, `gcodeToolCnt` y `materialMappings` (JSON en Base64). [W-HTTP]
  - Los valores booleanos van como texto en minúscula: "true" o "false". [W-HTTP]
- **Imprimir un archivo que ya está en la impresora (`/printGcode`):** el cuerpo lleva `fileName` y
  `levelingBeforePrint`. Los dos son obligatorios en el firmware de la serie 5M. [W-HTTP]
- **Pausa, reanudar y cancelar (`/control`):** `cmd` = `jobCtl_cmd`, con acción `pause`, `continue` o `cancel`.
  Ojo: `/control` responde éxito aunque el comando no exista, así que el programa confirma cada acción leyendo
  `/detail`. [W-HTTP]
- **Códigos de respuesta y mensaje al usuario:** [W-HTTP]
  - 0: éxito.
  - -1 ("Parameters is error"): "La impresora rechazó el pedido".
  - 1 ("SN is different" / "Access code is different"): "Número de serie o código de verificación incorrecto. Revisalo
    en la pantalla de modo de red de la impresora".
  - 2: "La impresora está ocupada".
  - 3: "El archivo no está en la impresora".

### TCP (puerto 8899; el único que tienen las de legado)
- Los comandos van con prefijo `~`. Las respuestas empiezan con "CMD <código> Received." y terminan en "ok". El
  terminador de línea al enviar está sin confirmar. [W-MCODE]
- **Sesión:**
  - M601 S1 abre la sesión de control y responde "Control Success". [W-MCODE] [W-AUTH]
  - Si responde "Control Failed", otro cliente ya tiene la sesión. Mensaje: "Otro programa está controlando la
    impresora". [W-MCODE] [W-AUTH]
  - M602 la libera antes de desconectar. [W-MCODE] [W-AUTH]
- **Consultas:**
  - M115: modelo, firmware, número de serie y MAC.
  - M119: estado de la máquina y archivo actual.
  - M105: temperaturas.
  - M27: progreso; el formato de la respuesta está sin confirmar. [W-MCODE]
- **Trabajo:**
  - M23: elige el archivo (rutas `0:/user/...` o `/data/...`).
  - M24: inicia o reanuda.
  - M25: pausa.
  - M26: cancela. Después de cancelar, hay que cerrar el aviso en la pantalla de la impresora antes de volver a
    controlarla. [W-MCODE]
- **Subida en las de legado:** M28 y M29. Los detalles del envío binario están sin confirmar, porque no los leí.
  [W-MCODE]
- **Puerto cerrado o conexión rechazada** (8898 u 8899): "No se pudo conectar a la impresora en <IP>. Revisá que esté
  prendida y en la misma red". Si la impresora es moderna, además: "y que el modo Solo LAN esté activo". [SP]

### Archivos
- Las modernas aceptan `.gcode` y `.3mf`, según los ejemplos de [W-MCODE].
- Que las de legado usen `.gx` (el formato de FlashPrint con miniatura) está sin confirmar: no aparece en estas fuentes.

### Pruebas de caja negra contra un simulador

| FF1 | caso | resultado |
|---|---|---|
| FF1.1 | Descubrimiento: el simulador responde con 280 bytes (moderna) y con 140 bytes (legado) | Lista ambas. La moderna aparece con número de serie y puerto HTTP 8898. La de legado aparece sin número de serie hasta pedir M115 |
| FF1.2 | `/detail` con un código de verificación incorrecto: el simulador devuelve code 1 | Muestra el mensaje de credenciales incorrectas y no reintenta en bucle |
| FF1.3 | Subida con "imprimir al terminar" y "nivelar" activados | El simulador recibe multipart con el campo `gcodeFile`, `fileSize` igual al tamaño real, `printNow`="true" y `levelingBeforePrint`="true" en cabeceras |
| FF1.4 | Pausa y cancelación en una moderna | Se envía `jobCtl_cmd` con `pause` y después con `cancel`. El estado mostrado sale de `/detail` (pause, cancel) |
| FF1.5 | Legado: el simulador contesta "Control Failed" a M601 S1 | Muestra que otro programa controla la impresora y no manda M23 ni M24 |
| FF1.6 | Puerto 8898 cerrado | Muestra el mensaje de "no se pudo conectar" con la IP, sin colgarse, y lo corta por timeout |

## Creality de fábrica (Klipper sin Moonraker) en red local (decisión del dueño, 17-11-2026: para la beta del 24-11; la subida y las órdenes de impresión quedan sin confirmar hasta medir el programa de Creality con una impresora real)

Sección solo de comportamiento. No contiene código ni fragmentos copiados. Todo lo que no tiene una fuente
pública que lo diga está marcado **(sin confirmar)**. No se leyó código de ningún slicer ni de ningún cliente.

### Qué modelos

- La integración comunitaria de Home Assistant `ha_creality_ws` dice que habla con K1, K1C, K1C (2025), K1 SE,
  K1 Max, K2, K2 Pro, K2 Plus, Ender 3 V3, V3 KE, V3 Plus y Creality Hi por un WebSocket local, sin nube
  (https://github.com/3dg1luk43/ha_creality_ws).
- Un usuario dice que K1, K1C y K1 Max usan el mismo firmware y el mismo protocolo
  (https://community.home-assistant.io/t/adding-creality-k1-max-to-ha-using-nodered-no-root-required/794457).
  Es la opinión de un usuario, no una fuente oficial. **(sin confirmar para K2 Plus y Hi en el detalle de campos)**

### Detectar si la impresora es de fábrica o tiene root

1. Probar Fluidd en 4408 y Mainsail en 4409. Las guías de instalación con root los listan
   (https://simplyprint.io/setup-guide/creality/k1-max.md). Si alguno responde, la impresora tiene root.
2. Probar Moonraker en 7125. Si responde, usar la conexión Moonraker que ya existe y no usar esta sección.
   - Un escaneo de puertos de un K1C 2025 con firmware de fábrica reporta 7125 como «compatible con Moonraker,
     totalmente funcional» (https://forum.creality.com/t/k1c-2025-orcaslicer-workarounds-for-missing-web-interface/47164).
     Que 7125 responda no prueba que haya root: alcanza con que la API funcione. **(sin confirmar en otros modelos)**
3. Si nada de eso responde y el puerto 9999 acepta un WebSocket, tratarla como Creality de fábrica.
4. Si no responde nada, mostrar: «No encontramos la impresora en <IP>. Revisá que esté encendida y en la misma red.»

### Puertos de fábrica

Según el mismo escaneo (foro de Creality, enlace de arriba):
- 80: nginx, responde 404 en todas las rutas. Un mensaje posterior dice que 80 es una redirección (iptables) a 8081.
  Esto no se verificó en el hilo. **(sin confirmar)**
- 8081: responde 404 en la raíz.
- 9999: WebSocket con un flujo de estado de la impresora.
- 8000: cámara WebRTC.
- 22: SSH con una contraseña desconocida. No usarlo.

En firmware v1.0.0.x del K1C 2025 no hay interfaz web en 80. Aun así, OrcaSlicer sube e imprime con la IP y el
puerto 80 (mismo hilo).

### WebSocket de estado (puerto 9999)

- Dirección: `ws://<IP>:9999`. El hilo de Home Assistant solo dice «el socket está en el puerto 9999», sin ruta.
  La ruta raíz y el subprotocolo quedan **(sin confirmar)**.
- Envío: la impresora empuja el estado sola. El cliente no consulta (README de `ha_creality_ws`).
- Contenido: mensajes JSON. Algunos valores solo cambian mientras imprime (hilo de Home Assistant).
- Mantener la conexión viva: usuarios de Node-RED activaron un latido (heartbeat) cada 10 s y la conexión se
  rearmó sola (hilo de Home Assistant). El contenido del latido queda **(sin confirmar)**.
- Campos con nombre confirmado:
  - `printJobTime`: tiempo impreso, en segundos.
  - `printLeftTime`: tiempo restante, en segundos.
  - Ambos según el hilo de Home Assistant.
  - `model`, `modelVersion`, `hostname`: identidad. Si `model` viene vacío, `modelVersion` lleva un código de placa,
    por ejemplo F012, F021, F008, F001, F002, F005 o F018 (README de `ha_creality_ws`).
  - `boxTemp`, `maxBoxTemp`, `targetBoxTemp`: cámara. `lightSw`: luz. `webrtcSupport`: cámara WebRTC.
    Todos según el README de `ha_creality_ws`.
- Campos que existen pero cuyo nombre no conocemos **(sin confirmar)**:
  - temperatura de boquilla y de cama, con sus objetivos;
  - progreso en %;
  - capa actual y capas totales;
  - nombre del archivo;
  - estado.
  Las fuentes dicen que esos datos llegan, pero no nombran las claves.
- Estados que la integración muestra: `idle`, `processing`, `printing`, `paused`, `stopped`, `completed`, `error`,
  `self-testing`. No se sabe si son valores crudos o traducidos por la integración **(sin confirmar)**.
- Detener no tiene un estado propio. La impresora manda el mismo estado que al precalentar, y la integración
  sostiene esa forma ambigua unos 15 s antes de decidir. El motor tiene que tratar «detenida» como una transición
  y no como un valor (README de `ha_creality_ws`).

### Comandos por el WebSocket

- Los comandos son JSON con un método `set` (lo muestra una publicación de seguridad; ver la advertencia).
  Las claves exactas para iniciar un archivo por nombre, pausar, reanudar y detener quedan **(sin confirmar)**.
  No se implementan hasta tenerlas de una captura propia de tráfico o de una documentación pública.
- Ventiladores: el mismo canal acepta G-code `M106 P<canal> S<0–255>` (README de `ha_creality_ws`).
- Filamento (`modifyMaterial`): Creality no lo documenta. Mientras la impresora está ocupada, el cambio se
  rechaza (README de `ha_creality_ws`). Queda fuera de alcance.

### Subida del G-code

- OrcaSlicer tiene un tipo de host «CrealityPrint». Con la IP y el puerto 80, el usuario prueba la conexión,
  la prueba pasa, y sube e imprime
  (https://forum.creality.com/t/k1c-2025-orcaslicer-workarounds-for-missing-web-interface/47164).
  En las versiones viejas de OrcaSlicer el tipo no aparece. La respuesta es actualizar
  (https://forum.creality.com/t/how-do-i-get-orca-to-communicate-with-my-k1c-problem/52030).
- En K2 Pro, poner `:4408` en el campo del host devuelve el control del equipo, y el tipo de host da la
  sincronización con el CFS
  (https://forum.creality.com/t/orcaslicer-2-4-1-k2-pro-cfs-device-control-findings/52221).
  Es una pista, no un hecho confirmado.
- Ruta HTTP, método y nombre del campo del formulario: **(sin confirmar)**. Ninguna fuente pública los da.
  Antes de implementar, hay que obtenerlos con una captura caja negra del programa oficial contra una impresora real.
- Hay reportes de archivos que se suben pero no aparecen en la lista y no se pueden iniciar
  (https://forum.creality.com/t/k1-max-camera-works-but-uploaded-files-are-not-visible-and-remote-print-cannot-start-in-orcaslicer/49936).
  Después de subir, el motor confirma que el archivo esté en la lista antes de decir «enviado».
- Vista previa de la impresión en curso: GET a `/downloads/original/current_print_image.png`, primero por HTTPS y
  después por HTTP (README de `ha_creality_ws`).

### Errores y mensajes al usuario

- Conexión rechazada en 9999: «La impresora no acepta conexiones de estado. Revisá la IP y que el firmware esté
  actualizado.»
- Se cortó el WebSocket: reconectar con espera creciente y mostrar «Reconectando…». Nunca dar por perdida una
  impresión porque se cortó la conexión.
- La subida responde 404 o 501 en 80: «Esta impresora no acepta subidas por la red con este firmware. Probá con
  USB o actualizá el firmware.» En Simplify3D, el puerto 80 dio 501/404 (https://forum.simplify3d.com/viewtopic.php?p=66001).
- Se pidió una acción con la impresora ocupada: «La impresora está ocupada (<estado>). Esperá a que termine.»
- Nunca enviar G-code arbitrario por 9999 sin que el usuario lo confirme.

### Advertencia de seguridad

El WebSocket 9999 no pide autenticación. Hay una publicación de seguridad pública que lo usa para ejecutar
comandos. No se usa como fuente de diseño. El motor conecta solo a IPs que el usuario eligió en la red local.
Avisa que cualquiera en la LAN puede controlar la impresora, y nunca la expone fuera de la LAN.

### Pruebas (simulador)

| CR1 | caso | resultado |
|---|---|---|
| CR1-1 | El simulador responde en 7125, y no en 9999 | Se usa la conexión Moonraker, no la de Creality |
| CR1-2 | Solo 9999 abierto, empuja JSON con `printJobTime`/`printLeftTime` | Se muestran el tiempo transcurrido y el restante, en segundos convertidos |
| CR1-3 | `model` vacío y `modelVersion` = F012 | El modelo se deduce del código de placa, sin fallar |
| CR1-4 | Imprimiendo, luego el estado de precalentamiento durante 15 s sin volver | Se informa «detenida» después de la ventana, no antes |
| CR1-5 | Se corta el WebSocket a mitad de una impresión | Se reconecta, la impresión sigue figurando activa y aparece «Reconectando…» |
| CR1-6 | La subida devuelve 404 | Se muestra el mensaje de firmware sin subida por la red, sin reintentar sin fin |

# Impresoras Elegoo por red local (SDCP v3)

Spec de comportamiento solamente. No tiene código. Cada dato lleva su fuente:
- **[OF]** = oficial CBD-Tech, protocolo SDCP V3.0.0:
  https://github.com/cbd-tech/SDCP-Smart-Device-Control-Protocol-V3.0.0 (archivo `SDCP(Smart Device Control Protocol)_V3.0.0_EN.md`).
- **[OC]** = comunidad, OpenCentauri: http://docs.opencentauri.cc/software/api (antes en suchmememanyskill.github.io/OpenCentauri, hoy da 404).
- **[CA]** = comunidad, cassini: https://github.com/vvuk/cassini
- **[PG]** = comunidad, guía de Printago: https://docs.printago.io/docs/connecting-printers/elegoo

## 1. A qué modelos aplica
- Centauri Carbon (FDM), la primera generación: usa SDCP por WebSocket [PG][OC].
- Resina con placa ChiTu y WiFi, por ejemplo Saturn 3 Ultra: también usa SDCP [OF][CA]. Las variantes viejas (cassini) usan SDCP sobre MQTT: la impresora se conecta como cliente MQTT a la PC. Ver §8 [CA].
  Qué Saturn o Mars usan WebSocket y cuáles MQTT: **sin confirmar** modelo por modelo.
- **No aplica:**
  - Centauri Carbon 2 y Centauri 2 (incluidos los Combo): usan MQTT y la impresora es el broker. Piden modo «LAN-Only» y la contraseña de fábrica es 123456 [PG]. El detalle del protocolo está **sin confirmar** y sería otra spec.
  - Neptune 4: usa Klipper/Moonraker (dato de la comunidad), así que corresponde el conector Moonraker y no este. **Sin confirmar** en las fuentes leídas.

## 2. Descubrimiento
- El programa manda por broadcast UDP el texto ASCII `M99999` al puerto **3000** [OF].
- Cada impresora responde con un JSON: `Id`, y dentro de `Data` vienen `Name`, `MachineName`, `BrandName`, `MainboardIP`, `MainboardID`, `ProtocolVersion` y `FirmwareVersion` [OF].
- La lista de impresoras se arma con las respuestas que lleguen en una ventana de tiempo. Se elige la ventana; el protocolo no fija una.
  Se deduplica por `MainboardID`.
- Si no responde ninguna, el usuario puede escribir la IP a mano.

## 3. Conexión
- WebSocket en `ws://<MainboardIP>:3030/websocket` [OF]. OpenCentauri nombra rutas alternativas (`/ws`, `/`, `/api/websocket`, `/sdcp`) [OC], **sin confirmar** en el oficial.
- No se nombra ningún subprotocolo WebSocket: **sin confirmar**. No hay que exigir uno.
- Latido: el cliente manda el texto `ping` y la impresora responde `pong` [OF]. Si deja de responder, se reconecta.
- Límite: hasta 5 WebSocket abiertos; el sexto recibe un HTTP 500 con «too many client» (resultado de búsqueda de comunidad, **sin confirmar**). Al usuario hay que decirle «la impresora tiene demasiadas conexiones abiertas».
- No hay cifrado ni autenticación en LAN [CA].

## 4. Sobre de los mensajes (JSON por texto) [OF]
- **Pedido** (cliente → impresora):
  - `Id`: identificador de la conexión.
  - `Data` contiene:
    - `Cmd`: número de comando.
    - `Data`: parámetros.
    - `RequestID`: único por pedido; la respuesta lo repite.
    - `MainboardID`.
    - `TimeStamp`: segundos Unix.
    - `From`: 0 = PC por LAN, 1 = PC por web, 2 = cliente web, 3 = app, 4 = servidor.
  - `Topic`: `sdcp/request/<MainboardID>`.
- **Respuesta**: el mismo sobre, con `Topic` `sdcp/response/<MainboardID>` y `Data.Data.Ack`, donde 0 = éxito.
- **Estado y atributos** llegan solos, sin que se pidan, en `sdcp/status/...` (campo `Status`) y `sdcp/attributes/...` (campo `Attributes`).
- **Error y aviso** llegan en `sdcp/error/...` y `sdcp/notice/...`.
  El aviso tipo 1 es «historial sincronizado».
- La respuesta se empareja con su pedido por `RequestID`. Un pedido sin respuesta en un tiempo propio se informa como «la impresora no respondió».

## 5. Estado
- Comandos: 0 refresca el estado y 1 pide los atributos [OF].
- `CurrentStatus` es un arreglo [OC] con estos valores: 0 libre, 1 imprimiendo, 2 recibiendo archivo, 3 test de exposición o calibración, 4 autochequeo [OF][OC].
- `PrintInfo` [OF] tiene estos campos:
  - `Status`:
    - 0 libre, 1 homing, 2 bajando, 3 exponiendo, 4 subiendo;
    - 5 pausando, 6 en pausa, 7 deteniendo, 8 detenido;
    - 9 terminado, 10 verificando archivo.
    - En FDM se reutilizan los nombres de resina [OC].
  - `CurrentLayer` y `TotalLayer`.
  - `CurrentTicks` y `TotalTicks`: tiempo transcurrido y tiempo total. La unidad está **sin confirmar**.
  - `Filename`, `ErrorNumber` y `TaskId`.
- Campos de resina [OF]:
  - `TempOfUVLED`;
  - `TempOfBox` y `TempTargetBox`;
  - `ReleaseFilm`;
  - `TimeLapseStatus`.
- Campos de FDM (solo en la comunidad) [OC]:
  - `TempOfNozzle` y `TempTargetNozzle`;
  - `TempOfHotbed` y `TempTargetHotbed`;
  - `CurrentFanSpeed`;
  - `CurrenCoord`: texto «x,y,z», con la errata incluida;
  - `ZOffset` y `PrintSpeed`.
  Hay que aceptar las dos grafías (`CurrenCoord` y `CurrentCoord`) y tolerar que los campos vengan anidados a distinta profundidad.
- El progreso que se muestra es `CurrentLayer/TotalLayer`, o la proporción de ticks.

## 6. Subida del archivo (G-code o .goo) [OF][OC]
- HTTP POST `multipart/form-data` a `http://<MainboardIP>:3030/uploadFile/upload`.
- Se manda en trozos de **1 MB**: un POST por trozo, en orden.
- Campos de cada POST:
  - `S-File-MD5`: MD5 del archivo entero, en hex.
  - `Check`: «1» activa la verificación y «0» la apaga.
  - `Offset`: byte donde empieza el trozo.
  - `Uuid`: el mismo en todos los trozos del archivo.
  - `TotalSize`: tamaño total en bytes.
  - `File`: los datos del trozo.
- Respuesta: código «000000» si salió bien y «111111» si falló.
  Errores: -1 offset negativo, -2 offset que no coincide, -3 no pudo abrir el archivo, -4 error desconocido.
- Durante la subida, `CurrentStatus` incluye el 2.
- Si el MD5 falla, llega por `sdcp/error` el `ErrorCode` 1; si el formato es incorrecto, el `ErrorCode` 2 [OF].
- Cancelar: comando 255 con `Uuid` y `FileName`.
  Ack: 0 OK, 1 no hay transferencia en curso, 2 ya está verificando, 3 archivo no encontrado [OF].
- Variante vieja [CA]: el comando 256 le pasa a la impresora una URL, y es la impresora la que baja el archivo desde un HTTP que sirve el cliente.

## 7. Imprimir, pausar, parar [OF]
- Iniciar: comando 128 con `Filename` (el nombre en la impresora) y `StartLayer` (0 para empezar desde el principio).
  Ack:
  - 0 OK
  - 1 ocupada
  - 2 no existe el archivo
  - 3 falló el MD5
  - 4 error de lectura
  - 5 la resolución no coincide
  - 6 formato desconocido
  - 7 el modelo de impresora no coincide
- Pausar es el comando 129, parar el 130 y reanudar el 131.
  El 132 detiene la alimentación de material y el 133 saltea el precalentamiento.
- Otros comandos:
  - 258: lista de archivos, con `Url` `/local/` o `/usb/`.
  - 259: borrado de archivos.
  - 320 y 321: historial.
  - 386: video. Ack 1 = límite de streams y 2 = no hay cámara.
  - 387: timelapse.
  - 403: velocidad y ventiladores, solo en la comunidad [OC].

## 8. Errores y cómo avisar al usuario
- Cada Ack distinto de 0 se muestra con su texto en castellano, según las tablas de §6 y §7. Nunca se muestra solo el número.
- `PrintInfo.ErrorNumber` [OF]: 1 MD5, 2 lectura, 3 resolución, 4 formato, 5 modelo.
- `ErrorStatusReason` [OF] va del 1 al 34. Algunos ejemplos:
  - 1 sobretemperatura;
  - 3 y 16 poca resina;
  - 7 falló la nivelación;
  - 8 la pieza se despegó;
  - 12 se sacó el USB.
  La lista completa sale de la fuente y se muestra traducida. Un código desconocido se muestra como «error N de la impresora».
- MQTT en resina vieja [CA]: el cliente manda `M66666 <puerto>` por UDP 3000 y la impresora se conecta como cliente MQTT a ese puerto. El uso de esta variante está **sin confirmar**; no es parte de la primera versión.

## 9. Pruebas de caja negra contra un simulador

| EL1 | caso | resultado |
|---|---|---|
| EL1.1 | El simulador responde a `M99999` en UDP 3000 con un JSON válido; llegan dos respuestas iguales | Aparece una sola impresora, con nombre, IP y `MainboardID` |
| EL1.2 | Se sube un archivo de 2,5 MB | Llegan 3 POST con offsets 0, 1048576 y 2097152, el mismo `Uuid`, el `TotalSize` correcto y el MD5 del archivo entero |
| EL1.3 | El simulador responde «111111» o -2 al segundo trozo | La subida se corta y el usuario ve «falló la subida (offset no coincide)», sin pantalla colgada |
| EL1.4 | Comando 128 con Ack 1 | El usuario ve «la impresora está ocupada» y no se marca como imprimiendo |
| EL1.5 | Pausa (129) y parada (130); el simulador pasa `PrintInfo.Status` por 5→6 y 7→8 | La interfaz muestra «en pausa» y «detenida», y el `RequestID` coincide con cada respuesta |
| EL1.6 | El simulador deja de responder `pong` o rechaza la sexta conexión con un 500 | Se reconecta, o muestra «demasiadas conexiones», sin perder el estado anterior |
