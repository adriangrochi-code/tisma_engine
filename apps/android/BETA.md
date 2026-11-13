# Tisma para Android — beta 0.1.0-beta.1

Versión de prueba para quienes la prueban con el dueño. Se instala a mano (APK), sin Google Play.

## Cómo instalar

1. Bajá el APK que te pasaron (el de teléfono, `app-arm64-v8a-beta.apk`) y abrilo en el teléfono.
2. Si Android lo pide, permití instalar apps de esa fuente.
3. Si ya tenías otra versión de prueba y Android dice que la firma no coincide, desinstalala primero y volvé a
   instalar. Esta beta se firma con una clave de prueba: cada versión puede pedir lo mismo hasta que haya una clave
   fija.

Las actualizaciones automáticas están apagadas en esta beta. Cada versión nueva llega como un APK nuevo.

## Qué probar

1. **El primer arranque (con internet):** el asistente baja la lista de impresoras. Elegí tu impresora y su boquilla.
   Después elegí el filamento y el perfil de impresión de la lista: deberían aparecer solo los que sirven para esa
   impresora. Debajo se ven la licencia y el origen de cada perfil.
2. **Sin internet en el primer arranque:** el asistente lo avisa y ofrece las impresoras que trae la app.
3. **Plato:** abrí un STL, 3MF u OBJ (también desde «Compartir» de otra app). Girá, escalá, espejá, «Apoyar cara»,
   «Orientar automáticamente» y copias. Laminá y mirá el tiempo, el filamento y las capas.
4. **Vista previa:** abrí el G-code laminado, recorré las capas, probá los colores (tipo de línea, velocidad, ancho)
   y la vista 3D.
5. **Ajustes:** «Impresión» desde el plato, «Material» desde Impresora. Cambiá algo a mano, después elegí otro
   perfil: tus cambios a mano se mantienen y un aviso dice cuáles.
6. **Ingeniería:** abrí una pieza, marcá con toques las caras fijas y las cargadas, y calculá si aguanta. Probá
   también la aerodinámica. Desde ahí están el **CAD** (Diseño) y las **Piezas** (engranajes y tornillería).
7. **Calibraciones** (en Más): torre de temperatura, flujo, retracción, puentes y voladizos y las demás. Laminá la
   prueba, elegí el valor que salió mejor y mirá que llegue a los ajustes.
8. **Impresoras:** agregá la tuya en la red (o por dirección) y mandá un laminado a imprimir.
9. **Tisma Cloud** (en Más): entrar con la cuenta, mandar un trabajo, ver su estado.
10. **Bambu Lab:** en el plato, «Placa de la cama»; al mandar, nivelar la cama y calibrar el flujo (prendidas) y
    vibraciones y timelapse; en la página de la impresora, la cámara de la A1 y la P1, y el aviso al terminar.
11. **Reportar un problema** (en Más): si algo falla, mandalo desde ahí; va con el registro de la app.

## Límites conocidos

- **No medido en teléfonos reales:** los tiempos y la memoria se midieron en emuladores y computadoras.
- **Ingeniería puede tardar:** un análisis de una pieza grande puede tardar varios minutos en el teléfono. Si va a
  tardar más de un minuto, la app lo avisa antes y ofrece un cálculo rápido.
- **Mi biblioteca** es nueva: guardá una pieza después de laminarla o analizarla, buscala en la lista, «Imprimir»
  la vuelve a laminar con su receta (no se guarda G-code), «La imprimí» guarda una nota y una foto, y se puede
  exportar y compartir como `.tismalib`.
- **El CAD** usa SolidCraft dentro de la app; no se probó en teléfonos reales.
- Ninguna impresión real se hizo con esta beta todavía: revisá el G-code antes de imprimir algo grande.


## Publicar en Google Play, prueba interna (lo que carga el dueño)

El flujo `.github/workflows/stores.yml` («Stores», a mano) arma el App Bundle firmado (`:app:bundleRelease`, con R8)
y lo sube a la pista `internal` con `tools/play_upload.py` (la API de Google Play Developer v3, solo biblioteca
estándar). Sin estos secretos del repositorio dice qué falta y termina sin fallar:

- [ ] Cuenta de Google Play Console y la app creada con el identificador `net.tismaservidores.tisma` (o el que
      decida el dueño; no cambia después de publicar).
- [ ] La clave de subida (un .jks propio; Play firma la versión final con la suya, «Play App Signing»):
      `PLAY_UPLOAD_KEYSTORE_B64` (en base64), `PLAY_KEYSTORE_PASSWORD`, `PLAY_KEY_ALIAS`, `PLAY_KEY_PASSWORD`.
- [ ] Una cuenta de servicio de Google Cloud con acceso a la API, invitada en Play Console con permiso de versiones:
      su JSON en `PLAY_SERVICE_ACCOUNT_JSON`.
- [ ] La primera subida va como borrador (`play_status` = `draft`) mientras la app no se publicó nunca; después,
      `completed`.
- [ ] Los testers de la prueba interna (correos o un grupo de Google).
- [ ] Más adelante: los productos de suscripción de Google Play y su `google_play` en `store_products` del servidor.

El código de versión es `1000 + número de corrida`. Sin probar todavía con cuentas reales.
