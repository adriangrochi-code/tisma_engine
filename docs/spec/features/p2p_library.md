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
   `files` (`path`, `role`: `project`, `analysis` o `photo`, sin G-code; `size`, `sha256`), `analysis` (resumen),
   `peer_key`, `manifest_signature` (firma del par sobre `{id, revision, files}`), `swarm` (protocolo e id del enjambre,
   lo llena el motor) y `checked` (lo revisó un sembrador).
3 quater. **Lista de bloqueo:** `format` `tisma-library-blocklist`, `version` 1, `generated`, `sequence`, `expires`
   (generated + 7 días; se firma de nuevo cada 6 h), `files` (sha256 y motivo), `accounts`, `peers`, `addresses` (IP
   con `until`) y `revoked_keys`.
3 bis bis. **Publicar** (servidor, a4eb152): `POST /v1/library/peer-key`, `POST /v1/library/models` (JSON, con la
   miniatura en `thumbnail_webp` en base64, hasta 64 KB), `.../models/<id>/revisions`, `DELETE .../models/<id>`,
   `GET /v1/library/mine`, `POST /v1/library/reports`. `manifest_signature` es la firma Ed25519 de la clave del par
   sobre el JSON canónico `{"files":[...]}` (claves ordenadas, sin espacios, UTF-8, archivos ordenados por `path`).
   Licencias: CC0-1.0, CC-BY-4.0, CC-BY-SA-4.0, CC-BY-NC-4.0, CC-BY-NC-SA-4.0, CC-BY-ND-4.0, CC-BY-NC-ND-4.0, MIT,
   GPL-3.0-or-later, LicenseRef-All-Rights-Reserved. Una IP de la lista va como red (`203.0.113.5/32`). Sin certificado
   el servidor sirve igual (para probar) con `certificate` `null`; las apps no lo usan.
3 quinquies. **Clave** (decisión del dueño, 10-11-2026): el maestro genera una clave Ed25519 propia de la biblioteca
   (`storage/library.key`). El dueño firma una sola vez su clave pública con la clave del proyecto del USB
   (`app_updates.md`, regla 14), sobre los bytes `tisma-library-key/1\n` más la clave pública de 32 bytes, y sube ese
   certificado. Las apps solo traen la clave del proyecto y aceptan un índice o una lista firmados por una clave de
   biblioteca con certificado válido y que no esté en `revoked_keys`. Si se roba el maestro, el dueño certifica una
   clave nueva y la vieja pasa a `revoked_keys`. Sin certificado, la biblioteca no publica (503).

## La moderación

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
   La pieza escaneada es privada hasta que se publica.

## Privacidad

8. La app no comparte nada que la persona no haya publicado o bajado de la biblioteca. «Mi biblioteca» local
   (`local_library.md`) no se comparte nunca. La dirección IP de cada par la ven los pares con que se conecta, y la
   pantalla de compartir lo dice antes de activarlo.

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
