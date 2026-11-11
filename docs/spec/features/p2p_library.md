# Biblioteca compartida entre usuarios (red de pares con índice moderado)

**Qué hace:** los modelos que la gente publica (`model_library.md`) viajan de un equipo a otro, sin que el servidor de
Tisma guarde ni sirva los archivos. El servidor solo guarda un índice chico (qué modelos hay y cómo reconocerlos) y la
lista de lo prohibido; cada app la respeta.

**Pedido del dueño** (10-11-2026): «un servidor que almacene y modere pero no tenga en sí [los archivos]… si algo pasa
lee el teléfono / PC que ese archivo no está permitido o [esa] IP; así podríamos usar una red peer sin sus
desventajas».

**Fase:** lo antes posible, antes de la 1.0 (dueño, 10-11-2026: «lo antes posible así subimos modelos»). Reemplaza la descarga directa desde el servidor de
`model_library.md`; el resto de esa ficha (publicar, perfiles probados, análisis) sigue igual.

## Las partes

1. **El índice** (en Tisma Cloud): por cada modelo publicado, su nombre, autor (cuenta), licencia, miniatura, tamaño,
   resumen de los análisis y la **huella** (SHA-256) de cada archivo. El índice se baja firmado con la clave de Tisma
   (la misma forma que `app_updates.md`, regla 13); una app no usa un índice sin firma válida.
2. **Los archivos:** no están en el servidor. Los tiene quien los publicó y quien los bajó, y se pasan entre apps por
   una red de pares estándar con una biblioteca de licencia permisiva (la elige el motor y la anota en `THIRD_PARTY.md`).
   Cada pedazo se comprueba contra la huella del índice: lo que no coincide se descarta.
3. **Siembra opcional:** un nodo de Tisma Cloud o cualquier usuario que lo active en Preferencias puede mantener
   modelos para que estén siempre disponibles. Por defecto, la app comparte solo lo que la persona publicó o bajó, solo
   con Wi-Fi y con el cargador puesto en el teléfono, con un tope de subida configurable.

## Formato (propuesta del servidor, 10-11-2026)

3 bis. **Rutas de lectura, sin sesión:** `GET /v1/library/index.json`, `/v1/library/blocklist.json` y sus `.sig` (64
   bytes Ed25519 sobre los bytes exactos), `/v1/library/key` (`public_key` y `certificate`) y
   `/v1/library/thumbs/<sha256>.webp`.
3 ter. **Índice:** `format` `tisma-library-index`, `version` 1, `generated`, `sequence` (crece siempre: se rechaza uno
   menor que el último visto) y `models`, cada uno con `id`, `revision`, `published`, `name`, `description`, `tags`,
   `license` (SPDX, lista cerrada), `author` (`account` opaco y `name`), `thumbnail` (WebP, hasta 64 KB), `size`,
   `files` (`path`, `role`: `project`, `profile` (receta), `analysis` o `photo`, sin G-code; `size`, `sha256`), `analysis` (resumen),
   `peer_key`, `manifest_signature` (firma del par sobre `{"files":[...]}`, ver 3 bis bis), `swarm` (`{"protocol": "libp2p/tisma-library/1", "piece_size": 1048576, "piece_roots": {…}}`, donde
   `piece_roots` lleva una entrada por cada archivo del modelo y ninguna otra: la clave es el `sha256` del archivo y el
   valor, el SHA-256 de las huellas de sus pedazos (32 bytes cada una, seguidas en orden); acordado con el motor) y `checked` (lo revisó un sembrador).
3 quater. **Lista de bloqueo:** `format` `tisma-library-blocklist`, `version` 1, `generated`, `sequence`, `expires`
   (generated + 7 días; se firma de nuevo cada 6 h), `files` (sha256 y motivo), `models` (código de publicación y motivo), `accounts`, `peers`, `addresses` (IP
   con `until`) y `revoked_keys`. Una IP va como `{"sha256": SHA-256(sal del día + IP), "until"}`, con `address_salt` en la lista.
3 bis bis. **Publicar** (servidor, a4eb152): `POST /v1/library/peer-key`, `POST /v1/library/models` (JSON, con la
   miniatura en `thumbnail_webp` en base64, hasta 64 KB), `.../models/<id>/revisions`, `DELETE .../models/<id>`,
   `GET /v1/library/mine`, `POST /v1/library/reports`. `manifest_signature` es la firma Ed25519 de la clave del par
   sobre el JSON canónico `{"files":[...]}` (claves ordenadas, sin espacios, UTF-8, archivos ordenados por `path`).
   Licencias: CC0-1.0, CC-BY-4.0, CC-BY-SA-4.0, CC-BY-NC-4.0, CC-BY-NC-SA-4.0, CC-BY-ND-4.0, CC-BY-NC-ND-4.0, MIT,
   GPL-3.0-or-later, LicenseRef-All-Rights-Reserved. Una IP que se bloquea se manda al panel como dirección o como red de una sola dirección (`203.0.113.5/32`), o una IPv6 como su red de 64 bits (`2001:db8:1:2::/64`, escrita como dice RFC 5952; regla 8 quinquies); cualquier otro rango se rechaza; en la lista publicada va como huella (3 quater). Sin certificado
   el servidor responde 503, salvo en pruebas (`TISMA_LIBRARY_UNCERTIFIED`).
3 quinquies. **Clave** (decisión del dueño, 10-11-2026): el maestro genera una clave Ed25519 propia de la biblioteca
   (`storage/library.key`). El dueño firma una sola vez su clave pública con la clave del proyecto del USB
   (`app_updates.md`, regla 14), sobre los bytes `tisma-library-key/1\n` más la clave pública de 32 bytes, y sube ese
   certificado. Las apps solo traen la clave del proyecto y aceptan un índice o una lista firmados por una clave de
   biblioteca con certificado válido y que no esté en `revoked_keys`. Si se roba el maestro, el dueño certifica una
   clave nueva y la vieja pasa a `revoked_keys`. Sin certificado, la biblioteca no publica (503).

## Qué se sube y cómo se revisa (dueño, 10-11-2026)

3 sexies. **Sin G-code:** un modelo publicado lleva el proyecto (piezas), los **ajustes generales** con que se laminó
   (perfil de impresión y de filamento, como receta de `local_library.md`) y los **análisis** (FEA, CFD) con su
   resumen. Nunca G-code: quien lo baja lo lamina en su teléfono, su computadora o la nube, con su impresora y esa
   receta, y el análisis se muestra si vale para sus ajustes (`model_library.md`, regla 3). El servidor rechaza un
   archivo con rol o extensión de G-code (`.gcode`, `.bgcode`, `.gco`, `.g`).
3 septies. **Ajustes que no viajan:** de la receta se sacan antes de publicar el G-code de la máquina (inicio, fin,
   cambios), los scripts de posprocesado, las rutas de archivos y los datos de conexión a impresoras. Al bajar, se usan
   los de la impresora de quien baja.
3 octies. **Archivos seguros** (lo comprueba la app al bajar y antes de abrir, y el sembrador al revisar):
   - solo estos tipos: 3MF, STL, OBJ, STEP, la receta JSON del motor, `.tea`, `.taero` con su JSON, y fotos WebP, PNG o
     JPEG; cada archivo se reconoce por su contenido, no por la extensión;
   - un 3MF o ZIP se abre sin escribir nada fuera de su carpeta (sin rutas absolutas ni `..`), sin enlaces, con tope de
     tamaño descomprimido (10 veces el comprimido y 2 GB) y sin archivos de programa adentro (ejecutables, scripts,
     bibliotecas, macros de oficina);
   - el 3MF no puede traer scripts de posprocesado ni G-code propio: si los trae, se quitan con aviso;
   - un archivo que falla una comprobación no se abre ni se comparte, se borra, y se avisa «Archivo inseguro: no se
     abrió» con el motivo; la app ofrece «Denunciar»;
   - además, el sembrador lo pasa por un antivirus (ClamAV) y lo marca `checked`.

## La moderación

3 novies. **Quién publica y cómo se bloquea** (dueño, 10-11-2026): solo publica una cuenta registrada; cada
   publicación tiene su código (el `id` del índice), que se ve en la ficha del modelo y en «Mis publicaciones». Se
   puede bloquear una publicación sola por su código (lista `models`), sin tocar las demás de esa cuenta; una cuenta
   entera, con `accounts`.

4. **Lista de bloqueo** (en el servidor, firmada como el índice):
   - **huellas de archivos** prohibidos: la app no los baja, no los comparte, no los abre y borra su copia con el aviso
     «Este archivo fue retirado de la biblioteca de Tisma»;
   - **cuentas** suspendidas: sus modelos salen del índice y la app no acepta archivos que esa cuenta firme;
   - **pares** bloqueados: la app no se conecta con ellos.
5. **Pares por identidad, no por IP:** cada app tiene una clave propia, ligada a la cuenta al publicar. Se bloquea esa
   clave. Una dirección IP se puede bloquear también, pero solo como medida corta (24 h por defecto), porque cambia y la
   comparten muchas personas, y porque es un dato personal: se guarda lo mínimo y se borra al vencer.
6. **Cuándo se aplica:** la app baja la lista al abrir la biblioteca y cada 6 horas mientras comparte; sin red, usa la
   última. Una lista de más de 7 días sin actualizar apaga el compartir hasta que se renueve.
7. **Denunciar:** desde un modelo, «Denunciar» manda al servidor la huella y el motivo; lo revisa una persona en el
   panel de administración, que puede retirar el modelo (entra en la lista de bloqueo).

## Desde el escáner

7 bis. **Pieza escaneada** (pedido del dueño, 10-11-2026: «que desde el escáner 3D se puedan subir archivos a la misma
   nube así se pueden imprimir archivos escaneados»): cuando la nube termina un escaneo, la pieza (malla y medidas)
   queda en la cuenta de la persona como un archivo propio. Desde la app del escáner hay dos botones:
   - **«Imprimir»:** la pieza aparece en Tisma (teléfono y computadora, misma cuenta) en «Mi biblioteca» como entrada
     nueva, lista para laminar e imprimir (`local_library.md`);
   - **«Publicar»:** la publica en la biblioteca compartida como cualquier modelo (reglas 1 a 7), con nombre, licencia y
     miniatura del escaneo. La revisa un sembrador como cualquier otra.
   **Autoría:** el autor y titular de una pieza escaneada es la persona que la escaneó (su cuenta): va como `author` y
   elige la licencia al publicar.
   La pieza escaneada es privada hasta que se publica. Servidor: `POST /v1/files` con el trabajo de escaneo la guarda en
   la cuenta (`pieza.stl` y su informe); `GET /v1/files` las lista para las apps, que recuerdan cuáles ya trajeron a cada
   equipo; se publica con `from_file`. Cuota por cuenta de 5 GB.

## IP oculta (decisión del dueño, 10-11-2026: mixto)

8 bis. **Todo cifrado:** cada transferencia entre apps va cifrada de punta a punta (con las claves de par de la regla 5).
8 ter. **Intermediarios de Tisma:** nodos de Tisma Cloud que pasan los pedazos entre dos apps sin poder leerlos; cada
   app solo ve la dirección del intermediario. Los usan:
   - el **teléfono**, siempre;
   - la **computadora**, por defecto; en Preferencias puede elegir «Directo (más rápido; los otros ven mi IP)», con el
     aviso de la regla 8.
8 ter bis. **Direcciones de «Directo»:** una app que comparte directo anuncia solo la dirección pública con que la ve
   el servidor central (la que observa con su red de pares); el servidor rechaza otra, para que nadie mande el tráfico de
   un modelo a la IP de un tercero.
8 quater. **Gasto acotado:** cada intermediario tiene un tope de ancho de banda por hora y por cuenta, configurable en
   el panel; al llegar, las apps que eligieron directo siguen y las demás esperan («Biblioteca ocupada, se retoma
   sola»). Los sembradores (regla 3) comparten directo desde la nube, sin intermediario.
8 quinquies. **Quién sabe las IP:** solo el servidor central, para conectar y para el bloqueo de 24 h; no las publica,
   y en la lista de bloqueo una IP va como huella (SHA-256 con una sal que rota cada día), no en claro.
   Una dirección IPv6 se bloquea por su red /64 (los primeros 64 bits), porque una conexión hogareña recibe un /64 y
   puede cambiar de dirección dentro de él: la huella se calcula sobre el /64 y el par compara la huella del /64 de la
   dirección que ve. IPv4 (también escrita `::ffff:a.b.c.d`) sigue por dirección (11-11-2026).
   La huella es SHA-256(sal del día + texto), con el texto exacto: para IPv6, la red en forma comprimida de RFC 5952,
   en minúsculas, seguida de `/64` (de la dirección `2800:A4:1234:5600:abcd:0:0:9` queda el texto `2800:a4:1234:5600` seguido de dos veces dos puntos y `/64`, la red con los ceros finales comprimidos); para IPv4, la
   dirección con puntos (`200.40.10.7`). Otra forma de escribirla da otra huella y el bloqueo no coincide.

## Privacidad

8 sexies. **El permiso ante el intermediario** (propuesto por el servidor, 2026-11-11): protocolo
   `/tisma/relay-ticket/1.0.0`. La app abre el stream y escribe el ticket de `POST /v1/library/relay-ticket` seguido
   de un salto de línea; el intermediario responde una línea JSON, `{"ok":true,"expires":<unix>}` o
   `{"ok":false,"error":"…"}`. El intermediario comprueba: la firma con la clave de la biblioteca (su certificado, con
   la clave del proyecto); que `peer_id` sea el par autenticado por Noise; que no esté vencido; que ni la cuenta ni el
   par estén en la lista de bloqueo firmada. Sin ticket válido no hay reserva ni circuito. La app lo presenta antes de
   reservar y otra vez al renovar (dura 1 h). Los bytes que pasan se cuentan por cuenta y por hora; al pasar
   `cap_mb_per_hour` se cortan sus circuitos y no se aceptan nuevos hasta la hora siguiente («Biblioteca ocupada, se
   retoma sola»).
8 septies. **El protocolo entre pares** `/tisma/library/1.0.0` (diseño propio, 2026-11-11), sobre un stream cifrado:
   - Cada pedido es una línea JSON de hasta 64 KiB terminada en salto de línea; cada respuesta, otra línea JSON. Se
     pueden mandar varios pedidos seguidos por el mismo stream, de a uno: el siguiente después de la respuesta.
   - `{"op":"have","files":["<sha256>",…]}` (hasta 256): responde `{"ok":true,"have":{"<sha256>":"all"|"<base64>"}}`,
     donde el base64 es un mapa de bits de los pedazos que tiene (bit más alto del primer byte = pedazo 0); un archivo
     que no tiene no aparece.
   - `{"op":"pieces","file":"<sha256>"}`: responde `{"ok":true,"size":<bytes>,"hashes":["<hex>",…]}`, el SHA-256 de
     cada pedazo de 1 MiB (el último, más corto). Quien lo recibe comprueba que el SHA-256 de esas huellas crudas,
     seguidas en orden, es el `piece_roots` del índice firmado; si no, descarta la lista y deja ese par.
   - `{"op":"get","file":"<sha256>","piece":<n>}`: responde `{"ok":true,"length":<L>}` y a continuación L bytes
     crudos, el pedazo. Quien lo recibe comprueba su SHA-256 contra la lista; si no coincide, lo descarta y lo pide a
     otro par (P2P2).
   - Errores: `{"ok":false,"error":"not_found"|"blocked"|"busy"|"bad_request"}`. Un par solo sirve archivos que están
     en el índice firmado y no en la lista de bloqueo; uno bloqueado responde `blocked`. Con el tope de subida
     alcanzado responde `busy`. Un pedido mal formado: `bad_request` y se cierra el stream.
   - El sembrador de Tisma Cloud (regla 3) habla este mismo protocolo, igual que las apps.
8. La app no comparte nada que la persona no haya publicado o bajado de la biblioteca. «Mi biblioteca» local
   (`local_library.md`) no se comparte nunca. Con intermediario, los otros pares no ven la IP; con «Directo» sí, y la pantalla lo dice antes de activarlo.

## Pruebas

| # | Caso | Resultado esperado |
|---|---|---|
| P2P1 | Publicar un modelo en un equipo y bajarlo en otro con el servidor sin el archivo | Se baja de par a par; las huellas coinciden |
| P2P2 | Un par manda un pedazo alterado | Se descarta; se baja de otro par |
| P2P3 | La huella entra en la lista de bloqueo | Las apps dejan de compartirlo y borran su copia con el aviso |
| P2P4 | Una cuenta suspendida | Sus modelos salen del índice; sus archivos firmados se rechazan |
| P2P5 | Índice o lista sin firma válida | No se usan |
| P2P6 | Teléfono con datos móviles | No comparte (solo baja, si la persona lo pide) |
| P2P7 | Lista de bloqueo de más de 7 días | Compartir apagado con aviso |
| P2P8 | Pedir un circuito sin ticket, o con un ticket de otro par | El intermediario no reserva ni conecta |
| P2P9 | Una cuenta pasa `cap_mb_per_hour` | Sus circuitos se cortan; se retoma a la hora siguiente |
| P2P10 | `pieces` con una lista que no da `piece_roots` | Se descarta la lista y se deja ese par |
| P2P11 | `get` de un archivo bloqueado | `blocked`; nada se manda |
