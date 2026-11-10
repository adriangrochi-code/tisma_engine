# Biblioteca compartida entre usuarios (red de pares con índice moderado)

**Qué hace:** los modelos que la gente publica (`model_library.md`) viajan de un equipo a otro, sin que el servidor de
Tisma guarde ni sirva los archivos. El servidor solo guarda un índice chico (qué modelos hay y cómo reconocerlos) y la
lista de lo prohibido; cada app la respeta.

**Pedido del dueño** (10-11-2026): «un servidor que almacene y modere pero no tenga en sí [los archivos]… si algo pasa
lee el teléfono / PC que ese archivo no está permitido o [esa] IP; así podríamos usar una red peer sin sus
desventajas».

**Fase:** a definir por el dueño (antes o después de la 1.0). Reemplaza la descarga directa desde el servidor de
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
