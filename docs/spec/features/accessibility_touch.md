# Accesibilidad y modo táctil

**Qué hace:** hace el programa usable para más personas: con daltonismo, con baja visión, sin mouse, con un lector de
pantalla, con sensibilidad al movimiento; y cómodo en tabletas y notebooks con pantalla táctil, con una interfaz
parecida a la del teléfono.

**Pedido del dueño (29-10-2026):** «investigar funciones de accesibilidad, por ejemplo para gente daltónica o con
diferentes problemas» y «una función táctil para tablets y laptops que cambie la interfaz, más parecida a la del
teléfono».

**Fase:** P1 (interfaz). El lector de pantalla, aparte (regla 9).

**Fuentes:**
- WCAG 2.2 (W3C, `https://www.w3.org/TR/WCAG22/`): criterios 1.4.1 (no usar solo el color), 1.4.3 (contraste del
  texto, 4,5:1; texto grande 3:1), 1.4.4 (texto hasta 200 %), 1.4.11 (contraste de controles y gráficos, 3:1), 2.1.1
  (todo con el teclado), 2.4.7 (foco visible), 2.5.8 (blancos de al menos 24 × 24 px), 2.3.3 (animaciones que se
  pueden apagar). Son para la web; se toman como vara para el programa;
- paleta para daltonismo de Okabe e Ito («Color Universal Design», 2002/2008): #E69F00, #56B4E9, #009E73, #F0E442,
  #0072B2, #D55E00, #CC79A7 y negro;
- mapa de color `cividis` (Nuñez, Anderton y Renslow, «Optimizing colormaps with consideration for color vision
  deficiency…», PLOS ONE, 2018) para los valores continuos;
- simulación de daltonismo para las pruebas: Machado, Oliveira y Fernandes, «A Physiologically-based Model for
  Simulation of Color Vision Deficiency», IEEE TVCG, 2009;
- lector de pantalla: AccessKit (`https://github.com/AccessKit/accesskit`, con enlaces para C en
  `https://github.com/AccessKit/accesskit-c`), licencia MIT o Apache-2.0 (con partes de Chromium bajo BSD). No se
  encontró una integración hecha con Dear ImGui: habría que armar el árbol de accesibilidad desde los controles de cada
  cuadro;
- blancos para dedos: Apple Human Interface Guidelines (44 × 44 pt) y Material Design (48 × 48 dp);
- pantallas táctiles en Windows: `GetSystemMetrics` (`https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getsystemmetrics`):
  `SM_MAXIMUMTOUCHES` (contactos que soporta la pantalla, 0 si no hay), `SM_DIGITIZER` (bits `NID_INTEGRATED_TOUCH`,
  `NID_READY`…), `SM_CONVERTIBLESLATEMODE` (0 = modo tableta; cuando cambia llega `WM_SETTINGCHANGE` con
  «ConvertibleSlateMode»);
- Windows 11 sin modo tableta: `https://learn.microsoft.com/en-us/uwp/api/windows.ui.viewmanagement.uiviewsettings`
  («In Windows 11, Tablet mode is removed and new functionality is included for keyboard attach and detach
  postures»); problemas de detección reportados por Mozilla: `https://bugzilla.mozilla.org/show_bug.cgi?id=1735765`
  y `https://bugzilla.mozilla.org/show_bug.cgi?id=1750584`;
- cómo se toman las tabletas: A List Apart, sobre Hoober y Shank (`https://alistapart.com/1-9-tablet-zones-jpg/`);
- GLFW, que usa la interfaz hoy, no tiene entrada táctil (en el foro de GLFW, 2025, sigue sin soporte; el backend de
  Dear ImGui para GLFW avisa que solo en Windows distingue toque de mouse). SDL3 (licencia zlib) tiene eventos de dedo
  (`SDL_EVENT_FINGER_DOWN/MOTION/UP`, `https://wiki.libsdl.org/SDL3/CategoryTouch`).

## Ajustes (Preferencias › Accesibilidad)

| Clave | Opciones | Por defecto | Qué hace |
|---|---|---|---|
| `ui.color_vision` | `normal`, `deuteranopia`, `protanopia`, `tritanopia`, `achromatopsia` | `normal` | Cambia los colores de la vista previa, de los avisos y de los mapas (reglas 1 a 3) |
| `ui.text_scale` | 100 a 200 % | 100 % | Agranda textos y controles, aparte de la escala de Windows (regla 5) |
| `ui.high_contrast` | sí, no, «como el sistema» | como el sistema | Tema de alto contraste (regla 4) |
| `ui.reduce_motion` | sí, no, «como el sistema» | como el sistema | Sin animaciones de cámara ni transiciones (regla 7) |
| `ui.touch_mode` | `auto`, `on`, `off` | `auto` | Interfaz táctil (reglas 10 a 15) |
| `ui.sounds` | sí, no | no | Un sonido al terminar de laminar o imprimir, además del aviso en pantalla |

## Reglas: accesibilidad

1. **Nunca solo el color:** todo lo que se distingue por color también se distingue de otra forma.
   - Vista previa por tipo de línea: la leyenda nombra cada tipo; al pasar el mouse (o tocar) sobre una línea, se
     muestra su tipo.
   - Voladizos y zonas de aviso en Preparar: además del rojo, un rayado.
   - Estados (bien, aviso, error): siempre con ícono y texto, no solo el color.
2. **Paletas para daltonismo:** con `ui.color_vision` distinto de `normal`:
   - los tipos de línea toman la paleta de Okabe e Ito, en un orden fijo; los tipos que sobran se distinguen por brillo
     y por la leyenda;
   - los valores continuos (velocidad, caudal, temperatura, ventilador, altura de capa, tensiones de Ingeniería) usan
     `cividis` en lugar de un arco iris;
   - con `achromatopsia`: escala de grises con rayados.
3. **Prueba con simulación:** cada paleta se pasa por la simulación de Machado et al. de los tres tipos de
   daltonismo, y cada par de colores vecinos en la leyenda tiene que seguir distinguiéndose (prueba AC2).
4. **Contraste:** los dos temas cumplen 4,5:1 en textos, 3:1 en textos grandes, íconos, bordes de campos y
   controles; también el ícono sobre la opción elegida (el error del tema claro del 28-10-2026). Con `ui.high_contrast`
   (o el alto contraste de Windows encendido): fondo y texto puros, bordes marcados en todos los campos.
5. **Tamaño:** `ui.text_scale` hasta 200 % agranda texto, campos e íconos juntos; el panel se reacomoda y nada queda
   cortado ni encimado (el panel ya mostró textos cortados al 100 %: «Soporte forzado en las primer…»).
6. **Teclado:** todo se puede hacer sin mouse: Tab y Mayús+Tab recorren los controles en orden de lectura, Enter y
   Espacio los activan, las flechas cambian listas y números, Esc cierra. El foco se ve siempre (un borde de 2 px del
   color de acento). En la vista 3D: flechas para girar, Mayús+flechas para mover, +/− para acercar, teclas numéricas
   para las vistas.
7. **Movimiento:** con `ui.reduce_motion`, la cámara salta en vez de animarse, y no hay transiciones ni parpadeos.
8. **Mensajes:** los avisos se quedan hasta que la persona los cierra o hasta que se resuelven (no desaparecen
   solos en 3 s); se pueden leer de nuevo en una lista.
9. **Lector de pantalla:** cada control tiene nombre, rol, valor y estado (la misma información que pide
   `--dump-ui`). Etapa 1: ese árbol existe y se prueba. Etapa 2: se publica con AccessKit (UI Automation en Windows,
   NSAccessibility en macOS, AT-SPI en Linux). Antes de la etapa 2 se anota AccessKit en `THIRD_PARTY.md`.

## Reglas: modo táctil

10. **Cuándo:** con `auto`, el modo táctil sigue a la computadora, como pidió el dueño (29-10-2026): en una Surface,
    al sacar el teclado la interfaz pasa sola a táctil, y al ponerlo vuelve.
    - Windows 11 ya no tiene «modo tableta»: lo reemplazó por posturas con y sin teclado (nota de Microsoft en
      `UIViewSettings`). La señal es `UIViewSettings.GetPreferredInteractionMode` (táctil o mouse) con su aviso
      `PreferredInteractionModeChanged`; una aplicación de escritorio la pide para su ventana.
    - Windows 10 y respaldo: `SM_CONVERTIBLESLATEMODE` (0 = tableta) y el aviso `WM_SETTINGCHANGE` con
      «ConvertibleSlateMode».
    - Las dos fallan en algunos equipos (Mozilla lo reporta para Firefox en Windows 11, bugs 1735765 y 1750584). Por
      eso: el cambio se avisa con una línea que permite deshacerlo, y se prueba en una Surface real (TC3) antes de
      darlo por bueno.
    - Además, sin postura: en una pantalla táctil sin teclado conectado, se ofrece una vez: «Esta pantalla es táctil.
      ¿Usar la interfaz táctil?». `on` y `off` lo fijan.
11. **Forma:** parecida a la app de teléfono (`mobile_app.md`), para que quien usa las dos no tenga que aprender otra
    cosa, pero pensada para tener la tableta en la mano:
    - **abajo, una barra con las secciones:** Placa (preparar), Laminado (vista previa) e Impresora. Siempre visible;
      cambiar de sección es un toque;
    - **al alcance de los pulgares:** con la tableta tomada con las dos manos, los pulgares llegan a los bordes de abajo
      y de los costados. Ahí van la barra, el botón principal (Laminar, después Guardar o Imprimir, abajo a la derecha)
      y las herramientas de la pieza (columna al costado izquierdo). Arriba quedan solo cosas que se miran: el nombre,
      el estado, la búsqueda;
    - **sentado también:** el uso de tabletas es casi siempre sentado (A List Apart, sobre los estudios de Hoober y
      Shank: 88 % sentado), con la tableta apoyada o en la mano; la interfaz no supone una sola forma de tomarla, y nada
      importante queda en el centro de arriba;
    - los ajustes en una hoja que sube desde abajo o se abre al costado, con títulos grandes y sin flechas (el pedido
      de diseño del 28-10-2026 vale igual);
    - blancos de al menos 48 × 48 px (más que los 24 × 24 de WCAG; las guías de Apple piden 44 × 44 pt y las
      de Android, 48 × 48 dp, para dedos), con 8 px entre blancos vecinos;
    - números con botones − y + grandes y un deslizador, además de poder escribir; el teclado de la pantalla se abre
      solo al tocar un campo de texto.
12. **Gestos en la vista 3D:** un dedo gira; dos dedos mueven y pellizcan para acercar; doble toque encuadra la
    pieza; tocar una pieza la elige; mantener apretado abre el menú que da el clic derecho (cargar, calibraciones,
    borrar…). Con lápiz: como el mouse.
13. **Nada depende de pasar el mouse:** las ayudas que salen al pasar el mouse salen al mantener apretado el ícono
    (i); los menús que se abren al pasar, se abren al tocar.
14. **Mover, rotar y escalar con el dedo:** manijas grandes sobre la pieza y, además, los mismos deslizadores del
    pedido del 28-10-2026.
15. **Sección Impresora (táctil y escritorio):** al tocar Impresora en la barra se ve la impresora elegida:
    - estado, temperaturas, avance, capa y tiempo que falta, en letra grande;
    - la cámara grande (`printer_connections.md`, «Cámara de la impresora»), que se agranda a pantalla completa con un
      toque;
    - pausar, seguir y cancelar (cancelar pide confirmación);
    - con varias impresoras, se pasa de una a otra deslizando.
16. **Cámara en Tisma Cloud:** en la misma sección, un interruptor «Ver esta impresora desde Tisma Cloud» prende el
    puente de `cloud.md` (reglas 12 a 17) para esa impresora: el estado y la cámara llegan a nuestros servidores y se
    ven desde la app del teléfono o la página, desde cualquier lugar.
    - Solo con la sesión iniciada y con el permiso explícito de la persona; se puede apagar en cualquier momento.
    - Como dice `cloud.md`, regla 15: nunca se suben imágenes si nadie mira.
    - Mientras se sube, se ve un indicador «Compartiendo cámara» con cuántas imágenes por minuto y cuántos datos van.
    - Es también la forma de probar el servicio de cámara de Tisma Cloud (pedido del dueño): la página de la cuenta
      muestra, para cada impresora compartida, la última imagen, cuándo llegó y la demora.
17. **Entrada táctil:** GLFW no da toques. Opciones para el equipo del motor, que decide:
    - A: en Windows, leer los mensajes de puntero (`WM_POINTER*`) de la ventana propia; en Linux y macOS, seguir con
      mouse y trackpad;
    - B: pasar la capa de ventana a SDL3 (zlib), que da eventos de dedo en los tres sistemas.
    Cualquiera que se elija se anota en `THIRD_PARTY.md` y en la decisión del plan.

## Pruebas

| # | Caso | Resultado |
|---|---|---|
| AC1 | Recorrer todas las pantallas con `--light` y sin él, medir el contraste de cada texto y control contra su fondo | Ninguno por debajo de 4,5:1 (texto) o 3:1 (texto grande, ícono, borde) |
| AC2 | Cada paleta de `ui.color_vision`, simulada con Machado et al. | Colores vecinos de la leyenda distinguibles en los tres tipos (diferencia de color ΔE2000 de al menos 10; valor a confirmar con personas daltónicas) |
| AC3 | `ui.text_scale` 200 %, ventana de 1366 × 768 | Ningún texto cortado ni encimado; todo se alcanza con el desplazamiento |
| AC4 | Solo teclado: cargar un cubo, cambiar la altura de capa, laminar, exportar | Se puede, y el foco se ve en cada paso |
| AC5 | `ui.reduce_motion` sí, apretar «encuadrar» | La cámara salta, sin animación |
| AC6 | Vista previa por tipo de línea con `deuteranopia` | Colores de Okabe e Ito; la leyenda nombra cada tipo |
| AC7 | `--dump-ui` de cada pantalla | Cada control con nombre, rol y valor; ninguno sin nombre |
| TC1 | `ui.touch_mode` `on`, 1280 × 800 | Barra de abajo, botón principal abajo a la derecha, blancos de 48 px o más |
| TC2 | Modo táctil, gestos simulados: un dedo, dos dedos, pellizco, doble toque, mantener | Girar, mover, acercar, encuadrar, menú del clic derecho |
| TC3 | Surface u otro convertible con Windows 11 y con Windows 10, `auto`: sacar el teclado y ponerlo | La interfaz pasa a táctil y vuelve; si la señal de Windows no llega, la persona puede cambiarlo con un toque |
| TC5 | Modo táctil, tableta tomada con las dos manos | La barra, el botón principal y las herramientas se alcanzan con los pulgares sin soltar la tableta |
| TC6 | Sección Impresora con una impresora con cámara | Estado grande, cámara que se agranda, pausar/seguir/cancelar |
| TC7 | «Ver esta impresora desde Tisma Cloud» encendido; abrir la impresora desde la app del teléfono | Estado y cámara llegan; con la app cerrada no se sube ninguna imagen; el indicador muestra imágenes por minuto y datos |
| TC4 | Modo táctil, ayuda (i) | Sale al mantener apretado, no hace falta mouse |
