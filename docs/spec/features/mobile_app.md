# App de Android

**Qué hace:** la app de teléfono de Tisma.
- Abre un modelo, lo lamina en el teléfono con el motor y muestra tiempo, filamento y la vista previa.
- Guarda, comparte o manda el G-code a una impresora de la red.
- Calcula si la pieza aguanta (Ingeniería) y lamina en la nube.

**Fase:** después de la primera versión de la interfaz de escritorio (P1).

**Decisión del dueño (26-10-2026):** «la app Android hagámosla de cero basada en el slicer nuevo».
- La app actual de Tisma Slicer no se usa ni se lee: es código de Tisma Slicer y usa su núcleo (`CLEANROOM.md`).
- Esta ficha dice lo que la app tiene que hacer. Lo que hacía la app vieja está medido como caja negra, desde lo que
  ve la persona.
- La app nueva vive en el repositorio del motor, con su licencia.

**Fichas que aplica:** el diseño y el comportamiento son los mismos que en el escritorio, adaptados a una pantalla
táctil:
- `interface.md` (aspecto) y `settings_panel.md` (secciones, niveles, tarjeta de la impresora);
- `profile_pack.md` (perfiles), `assistant.md` (ayuda y guías) y `printer_connections.md` (impresoras);
- `structural_analysis.md` (Ingeniería) y `cloud.md` (nube).

## Cómo está hecha

1. **El motor adentro:** el motor compilado para Android como biblioteca, para `arm64-v8a` (teléfonos) y `x86_64`
   (emulador). La app lo usa por una API en C del motor (regla 2) y no tiene lógica de laminado propia.
2. **API del motor para apps** (la escribe el equipo del motor; también le sirve al escritorio y a los complementos):
   - abrir un modelo o un proyecto desde una ruta o desde bytes en memoria;
   - poner los ajustes como JSON (`docs/CONFIG.md`);
   - laminar en otro hilo, con avance (0 a 100) y cancelación;
   - leer el resultado: resumen (tiempo, filamento, capas) y G-code;
   - leer las trayectorias para la vista previa (tipo, ancho, alto, velocidad), en partes, para no cargar todo en
     memoria;
   - analizar (Ingeniería), con avance y cancelación;
   - errores con código y texto.

   La API no depende de Android. Se documenta en `docs/` del motor y tiene sus pruebas.
3. **Interfaz nativa:** Kotlin con Jetpack Compose (lo elige el equipo del motor; si cambia, se anota acá), en tema
   claro y oscuro con los colores de `interface.md`. Textos en inglés y español.
4. **Sin código de terceros prohibido:** las bibliotecas de la app siguen `THIRD_PARTY.md` (licencias permisivas).

## Pantallas

5. **Secciones**, en una barra abajo (o al costado en horizontal): **Placa**, **Impresión**, **Material**,
   **Impresora** y **Más**. Son las mismas del escritorio (`settings_panel.md`).
6. **Placa:**
   - la cama y el modelo ocupan la pantalla;
   - arriba, la tarjeta de la impresora (`settings_panel.md`, reglas 32 a 36), con dibujos de la impresora, la cama y
     la boquilla, y el ícono de conexión (regla 42);
   - abajo, el botón principal: «Laminar»; después, el resultado y la salida (guardar, compartir, mandar a la
     impresora);
   - gestos: un dedo gira, dos dedos mueven, pellizcar acerca; doble toque acerca en ese punto; botón «Encuadrar».
   - Herramientas en tiras abajo: Archivo (abrir, información, quitar), Vista (3D, arriba, frente, izquierda,
     derecha, atrás), Ingeniería y Laminar. Una herramienta con opciones abre una tarjeta debajo y el modelo sigue a la
     vista.
7. **Impresión y Material:**
   - las secciones y los apartados de `settings_panel.md`, siempre abiertos;
   - los niveles Básico, Normal, Avanzado y la vista Personalizada;
   - en el teléfono, una sección por pantalla, y cada apartado como tarjeta.
8. **Impresora:** la configuración aparte de `settings_panel.md` (regla 37), con Conexión y Calibración.
9. **Más:** cuenta de Tisma Cloud, la biblioteca de modelos (`model_library.md`; «Imprimir» en dos toques), ayuda (`assistant.md`), informar un problema (`cloud.md`, regla 11, con `app`
   `android`), preferencias y créditos (`credits.md`).

## Funciones

10. **Abrir:** STL, 3MF, OBJ y STEP desde los archivos del teléfono, desde «Compartir con Tisma» de otra app, o desde
    un enlace de un sitio de modelos (si la persona lo permite, `preferences.md`).
11. **Perfiles:** el paquete libre de perfiles (`profile_pack.md`), bajado la primera vez, con el mismo asistente de
    primer uso que el escritorio: impresora por modelo, boquilla aparte y filamentos.
12. **Laminar en el teléfono:**
    - con el motor, en segundo plano, con avance y cancelar;
    - si el teléfono tiene poca memoria para el modelo, lo avisa y ofrece laminar en la nube (regla 15);
    - el resultado: tiempo, filamento en m y g, costo, y la vista previa por capas.
13. **Ingeniería:** tocar las caras fijas y las de carga (fuerza, dirección, en un punto o en toda la cara), elegir el
    material, el factor de seguridad y la temperatura.
    - **Analizar:** dice si aguanta, con el mapa de colores sobre la pieza.
    - **Relleno mínimo:** busca el relleno más bajo que aguanta y ofrece aplicarlo al laminado.
14. **Impresoras de la red** (`printer_connections.md`):
    - Bambu Lab en modo LAN, Klipper (Moonraker) y OctoPrint;
    - búsqueda en la red, mandar el G-code, empezar, pausar, seguir y parar;
    - estado, temperaturas, avance y cámara.

    Los códigos de acceso y las claves se guardan cifrados con una clave del almacén de claves de Android (Android
    Keystore), nunca en texto.
15. **Nube** (`cloud.md`):
    - entrar a la cuenta (también con Google);
    - laminar y analizar en la nube;
    - ver y manejar las impresoras remotas de los puentes;
    - pagar el plan con las suscripciones de Google Play.
16. **Ayuda sin conexión:** problemas, procedimientos, glosario, fichas de materiales y guías de calibración
    (`assistant.md`). Los tutoriales del primer uso se adaptan a la pantalla del teléfono.
17. **Teclado:** todo valor numérico se escribe con el teclado numérico del teléfono.

## Rendimiento

18. **Objetivos** (diseño: se confirman o se corrigen al medir), en un teléfono de gama media, con lo medido anotado en
    el avance del motor:
    - abrir un STL de 1 millón de triángulos en menos de 3 s;
    - laminar el cubo de 20 mm en menos de 2 s;
    - la vista previa a 30 cuadros por segundo o más con una pieza de 100 000 triángulos.

    El laminado nunca congela la interfaz.
19. **Memoria:** antes de laminar se estima la memoria necesaria. Si no alcanza, la regla 12 ofrece la nube.

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| MA1 | Primer arranque sin perfiles | Baja el paquete de perfiles y abre el asistente de primer uso |
| MA2 | Abrir el cubo de 20 mm y laminar | Tiempo y filamento iguales a los del motor en el escritorio con los mismos ajustes |
| MA3 | Cancelar un laminado largo | Se detiene y la interfaz sigue respondiendo |
| MA4 | Girar el teléfono durante un laminado | El laminado sigue; la pantalla se reacomoda |
| MA5 | «Compartir con Tisma» un STL desde otra app | Se abre en la placa |
| MA6 | Ingeniería: viga apoyada con una carga | Veredicto igual al del motor en el escritorio |
| MA7 | Mandar el G-code a una impresora simulada (`printer_connections.md`) | La impresora lo recibe y empieza |
| MA8 | Código de acceso guardado | No aparece en texto en los archivos de la app |
| MA9 | Modelo demasiado grande para la memoria | Aviso y ofrece laminar en la nube |
| MA10 | Laminar en la nube | El trabajo termina y el G-code se ve en la vista previa |
| MA11 | Informar un problema | Llega al servidor con `app` `android` |
| MA12 | Nivel Básico en Impresión | Las mismas claves que en el escritorio (`settings_panel.md`, regla 10) |
