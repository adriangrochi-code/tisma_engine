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
| Moonraker (Klipper) | mDNS con el nombre de host de `mdns_hostname` (por defecto el del sistema); SSDP solo si `enable_ssdp` está encendido | Busca nombres `.local` anunciados y, en cada uno, pregunta la información del servidor por la API de Moonraker en el puerto anunciado; si contesta, es una impresora Klipper |
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

## Pruebas

Contra simuladores propios de cada impresora (un programa que contesta como ella), sin impresora real.

| # | Escenario | Resultado esperado |
|---|---|---|
| U1 | Simulador USB que contesta `ok` a todo | Las líneas salen numeradas, con su suma, una por `ok` |
| U2 | El simulador pide `Resend: 5` en la línea 7 | Se vuelve a mandar desde la 5 y el archivo termina completo |
| U3 | El simulador se queda sin contestar un `ok` | Al rato se manda `M105` y la impresión sigue |
| U4 | El simulador manda reportes `T:210.0 /210.0 B:60.0 /60.0` | El estado muestra boquilla 210 °C y cama 60 °C |
| U5 | Cancelar a mitad de la impresión | Se manda `M108` y después los comandos de cancelación |
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
