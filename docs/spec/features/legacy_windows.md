# Windows viejo, 32 bits y placas de video viejas

**Qué hace:** el programa corre en la mayor cantidad posible de computadoras: Windows Vista en adelante, de 32 y de
64 bits, y con cualquier placa de video, desde las que tienen OpenGL 3.3 hasta las que solo tienen el OpenGL 1.1 que
trae Windows sin controlador. Con una placa vieja la vista 3D se ve más simple, pero todo lo demás (ajustes,
laminado, G-code, envío a la impresora) funciona igual.

**Pedido del dueño:** 02-11-2026, después del pedido de un probador de soporte para Windows Vista y 32 bits: «anexar
para dar uso mejor de 32 bits y de Vista para abarcar lo máximo posible. Habría que ver con lo que GL va a funcionar,
para que funcionen la mayor cantidad de equipos posibles».

**Fase:** cuando el dueño lo ordene; las reglas 1 a 4 (paquetes y compilación) pueden ir antes que las de la vista
(5 a 9).

**Fuentes** (02-11-2026; lo que no está confirmado dice «a verificar»):
- Microsoft, [Configurar programas para Windows XP](https://learn.microsoft.com/en-us/cpp/build/configuring-programs-for-windows-xp):
  el compilador de Visual Studio 2019 en adelante no genera programas para XP; el de XP es el de Visual Studio 2017
  (`v141_xp`), con las funciones de C++ de esa versión (C++17 como mucho) y marcado como obsoleto. Vista no figura
  como destino documentado del compilador actual.
- GCC 13, modelo de hilos `win32` de mingw-w64: da los hilos, mutex y variables de condición de C++ solo si el
  programa apunta a Vista o más nuevo (`_WIN32_WINNT` 0x0600)
  ([informe 29966](https://sourceware.org/bugzilla/show_bug.cgi?id=29966), que cita el encabezado de GCC 13).
- llvm-mingw, [README](https://github.com/mstorsjo/llvm-mingw): apunta a Windows 7 por defecto y su biblioteca de C++
  supone Windows 7 o más nuevo; la biblioteca de C universal (UCRT) se puede instalar desde Vista y la vieja
  `msvcrt.dll` viene en todos los Windows.
- GLFW, [novedades de 3.4](https://www.glfw.org/docs/3.4/news.html): XP y Vista quedan obsoletos pero siguen; la
  [3.5](https://www.glfw.org/docs/3.5/news.html) los quita y pide Windows 7.
- oneTBB, [requisitos](https://github.com/uxlfoundation/oneTBB/blob/master/SYSTEM_REQUIREMENTS.md): solo Windows 10
  y 11 (y Server 2019 en adelante); la guía de Intel dice que desde 2022.0 los binarios de 32 bits salen solo de la
  versión de código abierto.
- Rust, [blog del 26-02-2024](https://blog.rust-lang.org/2024/02/26/Windows-7/): desde Rust 1.78 los destinos de
  Windows piden Windows 10; los de Windows 7 quedan en el nivel 3 (sin compilar ni probar automáticamente). Afecta al
  componente de diseño (`cad_design.md`), escrito en Rust.
- Placas de video y su OpenGL en Windows (foros de Khronos e Intel, Geeks3D, g-truc, Phoronix; datos de 2009 a
  2015): Intel GMA X3100 2.0 a 2.1, GMA 4500 2.1, HD Graphics de primera generación (2010) 2.1, HD 2000 y 3000 (2011)
  3.1, HD 4000 (2012) 3.3 o más; AMD Catalyst 9.1 dio OpenGL 3.0 en Vista (Radeon HD 2000 en adelante); NVIDIA dio
  OpenGL 3.0 en XP y Vista para la serie GeForce 8 en adelante. Sin controlador del fabricante (o por escritorio
  remoto en los Windows viejos) Windows da el OpenGL 1.1 por software de Microsoft. A verificar en cada máquina: lo
  que informa el controlador depende de su versión.
- Dear ImGui: su dibujante para OpenGL 2 usa la tubería fija (sirve desde OpenGL 1.1); el de OpenGL 3 acepta
  sombreadores GLSL 1.20 (OpenGL 2.1) cambiando la versión de GLSL.
- Steam dejó Windows de 32 bits el 01-01-2026 con 0,01 % de sus equipos en Windows 10 de 32 bits
  ([PC Gamer](https://www.pcgamer.com/gaming-industry/steam-announces-end-of-support-for-windows-10-32-bit-saying-only-0-01-percent-of-pcs-are-still-running-it)):
  entre jugadores es casi nada; en talleres, escuelas y máquinas viejas al lado de una impresora, es más (el pedido
  vino de un probador).

## Ajustes

| Clave | Tipo | Por defecto | Qué hace |
|---|---|---|---|
| `ui.render_level` | texto | `automatic` | `automatic`, `gl33`, `gl30`, `gl21`, `gl11`: el nivel de la vista 3D (regla 5); `automatic` usa el más alto que funcione |

## Reglas

### Paquetes y compilación

1. **Dos familias de paquetes para Windows:**
   - **Actual** (`windows-x64`, la de hoy): Windows 10 y 11 de 64 bits, con todo (Vulkan, Diseño).
   - **Clásica** (nueva): `windows-x86-clasico` (32 bits) y `windows-x64-clasico` (64 bits), que arrancan en
     **Windows Vista SP2** y en todo lo que vino después. La de 64 bits existe porque hay Vista y 7 de 64 bits con
     más de 4 GB de memoria.
   - La página de descargas y la actualización automática eligen sola la familia según el Windows y la cantidad de
     bits; la actual no se ofrece en lo anterior a Windows 10.
   - Las dos familias laminan igual: mismo motor, mismos ajustes, mismo G-code (prueba LW2).
2. **Qué pide la compilación clásica** (comportamiento, no herramienta):
   - el programa y todo lo que trae solo llaman a funciones de Windows que existen en Vista SP2;
   - no pide instalar nada aparte: la biblioteca de C va adentro del paquete o es la `msvcrt.dll` que trae Windows;
   - en 32 bits, procesador con SSE2 como mínimo (Pentium 4 y Athlon 64 en adelante), y las cuentas de punto flotante
     con SSE2, no con la unidad x87, para que el G-code salga idéntico al de 64 bits;
   - según las fuentes, el compilador de Visual Studio actual no sirve para Vista y llvm-mingw tampoco (supone
     Windows 7); GCC 13 o más nuevo con mingw-w64 apuntando a Vista sí da los hilos de C++20. El motor elige y lo
     anota en su documentación de compilación.
3. **Bibliotecas en la compilación clásica:**
   - ventana: GLFW 3.4 (la última con Vista), fija en esa versión aunque la actual use otra;
   - cálculo en paralelo: oneTBB no promete Vista ni 7, así que la clásica tiene que funcionar sin ella, con un
     reparto en hilos propio que use todos los núcleos; si se usa una versión vieja que sí corra, se mide igual;
   - Vulkan: no existe en Vista; la clásica lo busca al arrancar y, si no está (Vista, o 7 sin controlador), usa la
     CPU como hoy (el ajuste `gpu` en su valor automático), sin aviso;
   - Diseño (`cad_design.md`): el componente es Rust y Rust pide Windows 10. En la clásica la pestaña Diseño se ve
     apagada con «Diseño necesita Windows 10 o más nuevo»; el resto de la pestaña Preparar no cambia. Los proyectos
     que traen un diseño se abren igual: la pieza se ve y se lamina, pero no se puede volver a editar.
4. **Memoria en 32 bits:**
   - el programa se marca para usar hasta 4 GB cuando corre en un Windows de 64 bits (en uno de 32 bits, 2 GB, o 3 GB
     si el sistema está configurado así);
   - antes de abrir un modelo o laminar, estima la memoria que necesita; si no alcanza, avisa antes de empezar
     «Este modelo necesita unos N MB y este programa de 32 bits puede usar M MB. Podés simplificarlo, laminarlo por
     partes o usar la versión de 64 bits» y ofrece Simplificar (`model_tools.md`);
   - si igual se queda sin memoria, el laminado se cancela con ese mismo mensaje; el programa no se cierra y el
     proyecto no se pierde.

### Vista 3D según la placa de video

5. **Niveles de la vista** (el motor decide cómo dibuja cada uno; esto es lo que se ve):

   | Nivel | Necesita | Placas típicas | Qué se ve |
   |---|---|---|---|
   | `gl33` | OpenGL 3.3 | Intel HD 4000 en adelante, GeForce 8 y Radeon HD 2000 en adelante con controladores nuevos | Todo, como hoy: oclusión ambiental, sombras, materiales |
   | `gl30` | OpenGL 3.0 a 3.2 | Intel HD 2000 y 3000 (3.1); GeForce 8 y Radeon HD 2000 con controladores de Vista | Igual que `gl33`; lo que necesite algo de 3.2 o 3.3 se hace de otra forma o se apaga, con la lista en la ficha de ajustes |
   | `gl21` | OpenGL 2.1 (o 2.0) | Intel GMA X3100 y 4500, Intel HD de 2010, GeForce 6 y 7, Radeon X1000 | Luz por píxel o por vértice, sin oclusión ambiental ni sombras; vista previa con menos detalle en los modelos grandes; la interfaz igual |
   | `gl11` | OpenGL 1.1 | Sin controlador del fabricante, escritorio remoto, máquinas virtuales sin aceleración | La interfaz completa; el modelo con una luz fija y simplificado a lo que se pueda mover con soltura; la vista previa del G-code capa por capa, de a una, desde arriba o en 3D con pocas capas |

   - en todos los niveles se puede: mover, girar y escalar, apoyar en una cara, pintar, laminar, ver la vista previa,
     exportar y enviar; lo que un nivel no dibuja (por ejemplo, la oclusión ambiental) se ve apagado en su ajuste con
     el motivo «tu placa de video no lo soporta (OpenGL X)»;
   - la elección de colores de la pintura (UI35) y el resto de la vista tiene que leerse en todos los niveles.
6. **Cómo se elige el nivel** con `automatic`:
   - al arrancar pide el nivel más alto y baja de a uno si la ventana no se puede crear o el nivel no está;
   - después de crear la ventana dibuja un cuadro de prueba fuera de la pantalla y lo compara con lo esperado; si no
     coincide (controladores que dicen tener una versión que no tienen), baja un nivel;
   - si el programa se cierra de golpe durante el primer cuadro de un nivel, el próximo arranque empieza un nivel más
     abajo y lo dice: «La última vez la vista 3D falló con OpenGL X; se usa OpenGL Y. Podés cambiarlo en
     Preferencias > Vista»;
   - el nivel elegido, la placa, el controlador y la versión de OpenGL se ven en Ayuda > Acerca de y van en los
     informes de problemas (`problem_reports.md`).
7. **Aviso en los niveles bajos:** con `gl21` o `gl11` una franja chica arriba de la vista dice «Vista 3D simple:
   tu placa de video tiene OpenGL X» con un enlace «Cómo mejorarla» (instalar el controlador del fabricante); se
   puede cerrar y no vuelve hasta que cambie el nivel. Con `gl11` sin controlador, el texto dice que Windows está
   usando su dibujo por software.
8. **Cambiarlo a mano:** Preferencias > Vista > «Nivel de la vista 3D» (`ui.render_level`) con Automático y los
   cuatro niveles; los que la placa no tiene se ven apagados. El cambio se aplica al reiniciar la vista, sin cerrar el
   programa, y el proyecto abierto queda como estaba. La línea de comandos acepta `--render-level` para las pruebas.
9. **Fluidez:** en cada nivel la vista mantiene al menos 20 cuadros por segundo al girar con el modelo de prueba de
   ese nivel (prueba LW5); si no llega, baja el detalle (menos triángulos del modelo, menos capas de la vista previa)
   antes de bajar la velocidad.

## Casos límite

- Windows Vista sin SP2, o XP: la clásica no arranca; el instalador o el archivo de léame lo dicen antes («Necesita
  Windows Vista SP2 o más nuevo»).
- Notebook con dos placas: se usa la que Windows dé al programa; el nivel puede cambiar al enchufar o desenchufar.
- Escritorio remoto: en Vista y 7 suele dar OpenGL 1.1 aunque la máquina tenga placa; el programa usa `gl11` y
  vuelve a subir en el próximo arranque local.
- Pantallas de 1024 × 768 (comunes en esas máquinas): la interfaz entra sin cortar textos (`accessibility_touch.md`).
- Procesador de 32 bits sin SSE2: el programa no arranca y lo dice en un cuadro de Windows antes de abrir la ventana.
- Rutas con acentos y usuarios con nombres no ASCII en Vista: abrir, guardar y laminar funcionan igual.

## Pruebas

| # | Entrada | Resultado esperado |
|---|---|---|
| LW1 | Compilación clásica de 32 bits, todas las pruebas del motor, en un Windows de 64 bits (las máquinas de la integración continua) | Pasan todas, igual que en 64 bits |
| LW2 | El cubo de 20 mm y el benchy con los ajustes base, laminados con la actual de 64 bits y con la clásica de 32 y de 64 | Los tres G-code son idénticos byte por byte |
| LW3 | La lista de funciones de Windows que importan los ejecutables y bibliotecas de la clásica, contra las de Vista SP2 | Ninguna falta en Vista; la prueba falla si aparece una nueva que no esté |
| LW4 | La clásica arrancada sin interfaz (laminar desde la línea de comandos) en Wine configurado como Windows Vista | Lamina el cubo y el G-code es el de LW2 |
| LW5 | `--render-level gl33`, `gl30`, `gl21` y `gl11` con Mesa en Linux (versión forzada) y el modelo de prueba (esfera de 200 000 triángulos) | Cada nivel abre, dibuja la placa, el modelo y la vista previa; la captura coincide con su referencia; 20 cuadros por segundo o más al girar |
| LW6 | Nivel `gl33` pedido en una máquina que solo tiene 2.1 | Arranca en `gl21` sin error; Acerca de dice «OpenGL 2.1, nivel gl21» |
| LW7 | Un cierre forzado durante el primer cuadro de `gl30` | El próximo arranque usa `gl21` y muestra el aviso de la regla 6 |
| LW8 | Clásica de 32 bits, un modelo de 3 millones de triángulos | Antes de abrirlo, el aviso de memoria de la regla 4 con Simplificar; sin cierre |
| LW9 | Clásica en Windows 7 sin controlador de Vulkan | Lamina con la CPU, sin aviso; Diseño apagado con su texto |
| LW10 | Prueba a mano en una máquina real con Vista (la del probador o una virtual) y otra con una placa Intel GMA o HD de primera generación | Instalar, abrir, laminar, ver la vista previa y exportar; anotar placa, controlador, nivel elegido y cuadros por segundo |
