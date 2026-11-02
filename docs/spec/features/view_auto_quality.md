# Calidad automática de la vista según el equipo

**Qué hace:** el programa reconoce el equipo (procesador, memoria, placa de video, pantalla, batería), mide cuánto
le cuesta dibujar y elige solo la calidad de la vista 3D: la mejor que se mueve con soltura sin cargar la máquina.
Mientras algo se mueve baja lo que haga falta y, cuando se queda quieto, vuelve a la imagen completa.

**Pedido del dueño:** 02-11-2026: «autodetección de componentes para asegurar que funcione»; en el equipo de un amigo
del dueño, al sacar la oclusión ambiental los cuadros por segundo se disparaban; «habría que ver un punto en el que
podamos conseguir buen rendimiento, pero que el equipo no esté muy cargado». Lo mismo midió un probador
(31-10-2026, UI37): de 300 a 28 cuadros por segundo al mover o pintar, con la oclusión ambiental encendida.

**Fase:** con la interfaz P1; va antes que los niveles de la vista de `legacy_windows.md` y trabaja dentro de ellos.

**Fuentes:**
- lo que ya hace la vista (`docs/UI.md`, «Imagen»): modos Rápida, Oclusión ambiental (por defecto) y Ray tracing,
  multimuestreo de 2×, 4× u 8× más FXAA, el tiempo de la GPU por cuadro en la barra de estado y la espera de eventos
  cuando nada se mueve;
- OpenGL: consultas de tiempo de la GPU (núcleo desde OpenGL 3.3, extensión `ARB_timer_query`); la memoria de video
  la informan las extensiones `NVX_gpu_memory_info` (NVIDIA) y `ATI_meminfo` (AMD), si están;
- las cifras de la regla 3 (presupuesto de 16,7 ms a 60 cuadros por segundo, margen de 30 %) son del diseño de esta
  ficha, no de una norma: se ajustan con las pruebas.

## Ajustes

| Clave | Tipo | Por defecto | Qué hace |
|---|---|---|---|
| `ui.view_quality` | texto | `automatic` | `automatic`, `high`, `balanced`, `light`, `custom` (regla 4) |
| `ui.max_fps` | entero | 60 | 0 a 240; tope de cuadros por segundo de la vista; 0: la frecuencia de la pantalla |
| `ui.lower_while_moving` | sí/no | sí | Bajar la calidad solo mientras algo se mueve y volver al soltar (regla 5) |
| `ui.battery_saver` | sí/no | sí | Con la notebook a batería, tope de 30 cuadros por segundo y calidad `light` (regla 7) |

## Reglas

1. **Qué detecta al arrancar** (sin pedir nada y en menos de medio segundo):
   - procesador: modelo, núcleos físicos y lógicos;
   - memoria total y libre;
   - placa de video: fabricante, modelo, controlador, versión de OpenGL o Vulkan, si es integrada o dedicada, y su
     memoria cuando el controlador la informa; en notebooks con dos placas, cuál le tocó al programa;
   - pantalla: resolución, escala y frecuencia;
   - si la máquina está enchufada o a batería.

   Todo se ve en Ayuda > Acerca de > Equipo y va en los informes de problemas (`problem_reports.md`).
2. **Medición la primera vez** (y cuando cambia la placa, el controlador o la pantalla):
   - dibuja fuera de pantalla una escena de prueba (la cama, un modelo de 200 000 triángulos y una vista previa de
     500 000 cordones) con cada modo de imagen y cada suavizado, unos 2 segundos en total, mientras se muestra la
     pantalla de inicio;
   - mide el tiempo de la GPU por cuadro, no los cuadros por segundo de la ventana;
   - guarda el resultado por placa y controlador, así los arranques siguientes no miden; en Preferencias > Vista hay
     un botón «Medir de nuevo».
3. **El punto de equilibrio** (`automatic`):
   - el presupuesto de un cuadro es el de `ui.max_fps` (16,7 ms a 60); la vista usa como mucho el 70 % de ese
     presupuesto en la GPU, así le queda margen a la máquina (el resto del programa, el laminado, otras ventanas);
   - elige la calidad más alta que entra en ese 70 % con el modelo y la pantalla actuales;
   - nunca dibuja más rápido que `ui.max_fps`: con una placa que da 300 cuadros por segundo dibuja 60 y la placa
     descansa el resto del tiempo (menos calor, menos ruido, menos batería);
   - cuando nada se mueve no dibuja (como hoy), y la oclusión ambiental o el ray tracing terminan de afinar la imagen
     quieta sin apuro.
4. **Calidades fijas** para quien no quiera la automática:

   | Calidad | Modo de imagen | Suavizado | Al mover |
   |---|---|---|---|
   | `high` | Oclusión ambiental (Ray tracing a mano) | 8× + FXAA | No baja nada |
   | `balanced` | Oclusión ambiental | 4× + FXAA | Oclusión apagada mientras se mueve |
   | `light` | Rápida | 2× + FXAA | Resolución de la vista al 75 % mientras se mueve |
   | `custom` | Lo que la persona eligió en la barra de vistas | Lo elegido | Según `ui.lower_while_moving` |

   Cambiar el modo de imagen o el suavizado a mano en la barra de vistas pasa la calidad a `custom`.
5. **Mientras algo se mueve** (girar la cámara, arrastrar o rotar una pieza, pintar, mover el deslizador de capas),
   si un cuadro pasa el 70 % del presupuesto durante medio segundo, baja de a un paso, en este orden, hasta entrar:
   1. la oclusión ambiental se congela: usa la del último cuadro quieto y no se recalcula hasta soltar;
   2. el suavizado baja (8× a 4× a 2× a solo FXAA);
   3. la resolución de la vista baja al 75 % y después al 50 % (los paneles y el texto no cambian);
   4. la vista previa muestra menos cordones lejanos (`docs/LARGE_MODELS.md`);
   5. el modelo se dibuja simplificado.

   Al soltar, en el primer cuadro quieto vuelve todo. Para no parpadear, solo sube un paso si sobró más del 40 % del
   presupuesto durante 2 segundos.
6. **Pintar:** el pincel nunca espera a la oclusión ni a las sombras. Mientras se pinta, la oclusión queda congelada
   (paso 1) desde el primer trazo, aunque sobre presupuesto, y se recalcula una vez al soltar el botón. El trazo sigue
   al mouse sin atraso visible: entre el movimiento y el cuadro que lo muestra pasan como mucho dos cuadros (UI37).
7. **Equipo cargado y batería:**
   - si el laminado corre en segundo plano o el procesador de la máquina pasa el 85 % de uso durante 3 segundos, el
     tope baja a 30 cuadros por segundo hasta que se libere, así el laminado y las otras aplicaciones no se frenan;
   - si la memoria libre baja de 500 MB, la vista suelta lo que pueda rehacer (la oclusión, las texturas de sombra) y
     lo avisa en la barra de estado;
   - a batería, con `ui.battery_saver`, tope de 30 y calidad `light`; al enchufar vuelve a lo anterior.
8. **Qué se ve:**
   - en la barra de estado, a la derecha, junto al tiempo de la vista: «Calidad automática · 60 FPS» y, si bajó algo,
     el motivo: «oclusión congelada al mover», «a batería», «laminando»;
   - la primera vez que la calidad automática baja algo, un aviso corto: «Bajamos la calidad mientras se mueve para
     que la vista siga fluida. Podés cambiarlo en Preferencias > Vista»; no vuelve a salir;
   - Preferencias > Vista: Calidad, Tope de cuadros por segundo, Bajar al mover, Ahorro a batería, Medir de nuevo y
     el resumen del equipo.
9. **Lo que no cambia:** la calidad de la vista no toca el laminado ni el G-code, ni las capturas que pide la línea
   de comandos (esas usan la calidad que se les pida).

## Casos límite

- Placa que informa una versión de OpenGL que no tiene, o que da tiempos imposibles (0 ms): se descarta la medición
  y se usa `light` hasta medir de nuevo; si el primer cuadro falla, rige `legacy_windows.md`, regla 6.
- Dos pantallas con frecuencias distintas: el tope sigue a la pantalla donde está la ventana.
- Escritorio remoto: se mide igual; casi siempre queda en `light`.
- Una sola pieza muy grande (millones de triángulos) en un equipo fuerte: la medición de la regla 2 no alcanza; la
  regla 5 baja pasos al mover aunque la calidad elegida sea `high`, salvo que la persona apague
  `ui.lower_while_moving`.
- Con `ui.max_fps` en 0 y una pantalla de 240 Hz, el presupuesto es de 4,2 ms: se aplica igual el 70 %.

## Pruebas

| # | Entrada | Resultado esperado |
|---|---|---|
| AQ1 | Primer arranque en la integración continua (GPU por software, llvmpipe) | La medición termina en menos de 3 s y elige `light` o lo que entre en el 70 % de 16,7 ms; Acerca de > Equipo lista procesador, memoria, placa y controlador |
| AQ2 | Escena de prueba quieta, calidad `automatic`, placa rápida (o `--bench-frames` con un tope de 60) | Nunca más de 60 cuadros por segundo; con la vista quieta, ningún cuadro nuevo durante 5 s |
| AQ3 | Girar la cámara 5 s sobre un modelo de 2 millones de triángulos con la oclusión ambiental | El tiempo medio de la GPU por cuadro queda en el 70 % del presupuesto o menos; al soltar, el primer cuadro quieto tiene la oclusión |
| AQ4 | Pintar soportes 5 s con trazos rápidos sobre el benchy, oclusión ambiental, nivel `gl33` | La oclusión no se recalcula durante el trazo; atraso del trazo de 2 cuadros o menos; 30 cuadros por segundo o más en la GPU por software del contenedor (UI37 pedía no bajar de 300 a 28) |
| AQ5 | Laminar el benchy en segundo plano mientras se gira la cámara | El tope pasa a 30 y la barra dice «laminando»; el laminado tarda como mucho un 10 % más que sin girar |
| AQ6 | `--power battery` (simulado) | Tope 30, calidad `light`; con `--power ac` vuelve a la anterior |
| AQ7 | Carga artificial del procesador al 95 % durante 10 s | Tope 30 a los 3 s; vuelve a 60 dentro de los 2 s de liberarse, sin parpadear entre calidades |
| AQ8 | Cambiar el modo de imagen a mano a Ray tracing | La calidad pasa a `custom` y la automática no la cambia más |
| AQ9 | Mismo proyecto laminado con `high` y con `light` | G-code idéntico |
