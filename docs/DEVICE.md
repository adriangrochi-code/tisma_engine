# Device: las impresoras conectadas

Pedido del dueño (2026-10-24): «la parte de device y las capacidades de conexión de múltiples marcas de impresora»;
después: «el slicer de PC puede imprimir por USB; el celular, solo por WiFi». Es la fase P2 de `PLAN.md`; la app de
Android (`docs/MOBILE.md`) usa la misma capa por la red.

Todo es diseño propio. Los protocolos salen de la documentación pública de cada firmware y, cuando esté en `main`,
de la ficha de Device (`docs/spec/features/devices.md`, PR #11). Los hosts de Tisma Slicer vienen de código de
OrcaSlicer y no se miran (`CLEANROOM.md`).

## Una sola capa: `src/device/`

| Parte | Qué es |
|---|---|
| `printer.hpp` | **`Printer`**, lo que toda conexión ofrece: conectar y desconectar; el estado (`PrinterStatus`: desconectada, conectando, lista, imprimiendo, en pausa, error; el firmware; temperaturas de boquilla y cama con sus objetivos; la posición; el trabajo, sus líneas enviadas y totales, el avance, el tiempo; los reenvíos; el último error); los eventos (`PrinterEvent`: conectada, desconectada, empezó, pausó, siguió, terminó, cancelada, error); un trabajo (`start_print`, `pause`, `resume`, `cancel`); comandos sueltos (`send`) y los controles de siempre (`set_nozzle`, `set_bed`, `home`, `move`, `extrude`, `fan`). Nada bloquea: la interfaz pregunta el estado y retira los eventos |
| `stream.hpp` | **`ByteStream`**, por donde habla una conexión: leer con tiempo de espera, escribir, cerrar. `make_pipe` da dos extremos unidos en memoria, para el simulador y las pruebas |
| `serial_port.*` | **`SerialPort`**, el puerto serie del USB: 8N1 sin control de flujo, a cualquier velocidad (115 200 y 250 000 son las habituales). Código propio sobre `termios2` en Linux (admite 250 000) y el puerto COM en Windows. `list_serial_ports` lista `/dev/ttyUSB*`, `/dev/ttyACM*` o los `COMn` que existen |
| `gcode_host.*` | **`GcodeHost`**, el host de G-code (abajo): una `Printer` sobre cualquier `ByteStream` |
| `simulator.*` | **`SimulatedPrinter`**, una impresora que contesta como una placa Marlin sobre un `ByteStream`, para las pruebas y para probar la interfaz sin máquina |

## El host de G-code por puerto serie

Es lo que hablan Marlin y su familia por USB (y RepRapFirmware por su USB): líneas de G-code, una respuesta por
línea. El formato sale de la documentación pública de esos firmwares (los documentos de cada comando, no el código).

- **Conexión:** una línea vacía y `M110 N0` (sin número) repetido cada segundo hasta el primer `ok` (una placa que se
  reinicia al abrir el puerto tarda unos segundos); después `M115` (el nombre del firmware) y `M155 S2` (que mande
  las temperaturas sola cada 2 s; si no conoce el comando, el host pregunta con `M105` cada 2 s).
- **Cada línea** va numerada y con suma de verificación: `N<n> <comando>*<suma>`, la suma es el XOR de todos los
  bytes anteriores al `*`. El host manda una línea y espera su `ok` antes de la siguiente.
- **Reenvío:** si la línea llegó rota, el firmware contesta `Error:…`, `Resend: <n>` y un `ok`; el host traga ese
  `ok` y manda de nuevo la línea con el número que pide (si la placa se reinició y pide desde 1, también). Hasta 5
  veces por línea; después, el trabajo se cancela con error.
- **Mientras espera** el `ok`, lee lo que llegue: temperaturas (`T:… /… B:… /…`, también en `ok T:…`), la posición
  (`X:… Y:… Z:… E:…`), `echo:busy: processing` (reinicia el tiempo de espera: un `M109` tarda lo que tarda en
  calentar) y los errores (`Error:` que no son de reenvío quedan en el estado y en un evento).
- **Un trabajo** es el texto del G-code: se manda línea por línea sin comentarios ni líneas vacías; el avance es
  líneas enviadas sobre totales. **Pausar** deja de mandar (y sigue leyendo temperaturas); **cancelar** deja de
  mandar y apaga la boquilla, la cama y el ventilador y suelta los motores (`M104 S0`, `M140 S0`, `M107`, `M84`).
- **Comandos sueltos** (`send` y los controles) se mandan en el primer hueco, también durante una impresión.
- **Desconectar** cancela el trabajo y cierra el puerto. Si la impresora deja de contestar (30 s sin `ok`) o el
  puerto se cae, el estado pasa a error y lo dice.

El host corre en su propio hilo; `status()` devuelve una copia y `take_events()` lo acumulado desde la última vez.

## El simulador

`SimulatedPrinter` lee las líneas, verifica número y suma (y pide reenvío como Marlin: `Error:…`, `Resend:`, `ok`),
y contesta `M110`, `M115` («TismaSim»), `M105`, `M155`, `M104`/`M140`, `M109`/`M190` (espera diciendo
`echo:busy` hasta que la temperatura simulada llega), `M114`, `G28`, `G90`/`G91`, `M82`/`M83`, `G92`, `G0`/`G1`
(lleva la posición), `M27` (sin tarjeta) y `ok` a lo demás. Las temperaturas se acercan al objetivo un 30 % por
tick (50 ms). Con `fail_line = n`, contesta la línea `n` con error de suma una vez, para probar el reenvío. Guarda
cada comando recibido, así las pruebas comprueban qué llegó.

## La página Impresora de `tisma`

La pestaña **Impresora** (ícono de impresora, a la derecha de Calibración) muestra un panel a la izquierda; la pieza
se corre a la derecha, como en Calibración:

1. **Conexión:** el puerto (los USB que se ven, y **Impresora simulada**), **Actualizar** la lista, la velocidad
   (115 200, 250 000, 57 600 o 230 400) y **Conectar** / **Desconectar**. El puerto y la velocidad se recuerdan.
2. **Estado:** un punto de color con el estado (lista, imprimiendo, en pausa, error), el firmware, el último error, y
   las temperaturas de boquilla y cama (actual / objetivo) con un campo, **Fijar** y **Apagar**.
3. **Trabajo:** **Imprimir** manda lo laminado (o el G-code abierto) línea por línea; la barra de avance, las líneas
   enviadas, el tiempo que pasó y el que falta (con el tiempo estimado del G-code), los reenvíos; **Pausar**,
   **Seguir** y **Cancelar**.
4. **Mover y extruir** (con la impresora lista): **Inicio**, ±X, ±Y y ±Z con un paso, extruir y retraer 5 mm, y el
   ventilador.
5. **Registro:** los eventos (conectada, empezó, terminó, cancelada, errores), los más nuevos arriba. Los importantes
   también salen como avisos.

`--device PUERTO|sim` conecta al empezar; `--view device` abre la pestaña; con `--screenshot`, `--device-print`
lamina, imprime y espera el final (sale con 1 si la impresión se cancela o la impresora da error). La prueba
`ui_device_prints_to_simulator` lo hace con el cubo contra la impresora simulada.

## Pruebas (`tests/test_device.cpp`)

- Los textos del protocolo: la suma del ejemplo de Marlin (`N0 M110 N0*125`), temperaturas en sus formas, la
  posición, `Resend:` y `rs`, qué es un `ok`.
- El tubo en memoria: bytes en los dos sentidos, tiempo de espera, cierre.
- Un trabajo entero contra el simulador con una línea que llega rota: cada `G1` llega una sola vez y en orden,
  un reenvío, el avance a 1, los eventos; después, temperaturas, un movimiento relativo y `M114` desde la
  interfaz.
- Pausar, seguir y cancelar (con un firmware sin `M155`: el host pregunta `M105`), y la secuencia de apagado.
- Una impresora que nunca contesta (error con «no answer») y una conexión que se corta (error con «lost»).
- El estado y su evento cambian juntos (bajo el mismo bloqueo): quien ve «error» o «terminó» encuentra el evento.
  Una carrera que mostró UBSan en el CI de Linux (2026-10-25).
- Los puertos serie se listan y un puerto inexistente falla con su motivo.

## Qué sigue (en orden, `docs/MOBILE.md`)

1. ~~La página Device de `tisma`~~: hecha (arriba).
2. El servicio local de `tisma` en la red y la app de Android que lo sigue.
3. Las conexiones de red de la ficha de Device, en esta misma capa: Moonraker, OctoPrint, PrusaLink, Duet y las
   marcas.
