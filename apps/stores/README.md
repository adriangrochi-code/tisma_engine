# Fichas de las tiendas

Lo que se carga en App Store Connect y en Google Play Console para la beta, en español y en inglés. Los textos
respetan los límites de cada tienda (marcados al lado). Las capturas salen de la CI (ver «Capturas»).

- `listing_es.md`, `listing_en.md`: nombre, subtítulo, descripción corta y larga, palabras clave, novedades.
- `privacy_es.md`, `privacy_en.md`: la política de privacidad (qué datos salen del teléfono y adónde). Se publica en
  una dirección pública antes de mandar la app a revisión; la de la tienda apunta ahí.
- «Privacidad de la app» (App Store) y «Seguridad de los datos» (Google Play): las respuestas, al final de
  `privacy_es.md`.

## Capturas

- **App Store:** iPhone de 6,9" (1320 × 2868) o 6,5" (1284 × 2778), de 1 a 10. La CI de iOS
  (`.github/workflows/ios.yml`) guarda en el artefacto `ios-screenshots` las de cada pantalla, en el simulador
  «iPhone 16 Pro Max» (1320 × 2868) cuando está.
- **Google Play:** teléfono, de 2 a 8, PNG o JPEG, lados entre 320 y 3840 px, proporción hasta 2:1. La CI de Android
  guarda las del emulador en su artefacto de pruebas.
- Para la tienda: Placa con el modelo, Impresión, Impresora con la tarjeta, Ingeniería con el mapa de seguridad,
  Vista previa, Más.

Pendiente del dueño: el nombre definitivo, el sitio de soporte y la dirección pública de la política.
