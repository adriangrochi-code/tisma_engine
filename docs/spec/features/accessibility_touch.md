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

10. **Cuándo:** con `auto`, el modo táctil se prende si la pantalla es táctil y la computadora está en modo tableta
    (en Windows, `SM_MAXIMUMTOUCHES` > 0 y `SM_CONVERTIBLESLATEMODE` = 0), y se apaga al volver a modo notebook, con un
    aviso de una línea que permite deshacerlo. En una pantalla táctil sin modo tableta, se ofrece una vez: «Esta
    pantalla es táctil. ¿Usar la interfaz táctil?». `on` y `off` lo fijan.
11. **Forma:** la de la app de teléfono (`MOBILE.md` del motor), adaptada a pantalla ancha:
    - barra de abajo con las pestañas grandes (Preparar, Vista previa, Impresora…) y el botón principal (Laminar,
      después Guardar o Imprimir) grande abajo a la derecha;
    - los ajustes en una hoja que sube desde abajo o se abre al costado, con los títulos grandes y sin flechas
      (el pedido de diseño del 28-10-2026 vale igual);
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
15. **Entrada táctil:** GLFW no da toques. Opciones para el equipo del motor, que decide:
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
| TC3 | Windows convertible: plegar a modo tableta con `auto` | La interfaz pasa a táctil y vuelve al desplegar |
| TC4 | Modo táctil, ayuda (i) | Sale al mantener apretado, no hace falta mouse |
