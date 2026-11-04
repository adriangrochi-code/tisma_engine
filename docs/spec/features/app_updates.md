# Actualizador integrado

**Qué hace:** el programa se actualiza solo, como un juego en Steam: avisa que hay una versión nueva, la baja en
segundo plano mientras se sigue trabajando y la instala al cerrar o con un botón, sin ir a la página ni volver a
instalar. Los proyectos, los perfiles y las preferencias quedan como estaban.

**Pedido del dueño** (02-11-2026): «un actualizador integrado: cuando hace una nueva versión, te lo actualiza
automáticamente y no tenés que descargar desde la página web… como quien juega en Steam: se actualiza el juego y
seguís jugando».

**Fase:** 1.0 (enero de 2027), con los instaladores.

**Fuentes:**
- firmas digitales Ed25519, [RFC 8032](https://www.rfc-editor.org/rfc/rfc8032): cada paquete va firmado y el programa
  trae la clave pública para comprobarlo;
- resumen SHA-256, [FIPS 180-4](https://csrc.nist.gov/pubs/fips/180-4/upd1/final), para comprobar que la descarga
  llegó entera;
- el canal de publicación ya existente del motor (las versiones etiquetadas y sus paquetes, `docs/VERSIONING.md`).

## Ajustes

| Clave | Tipo | Por defecto | Qué hace |
|---|---|---|---|
| `updates.check` | sí/no | sí | Buscar versiones nuevas al abrir, como mucho una vez por día |
| `updates.channel` | texto | `stable` | `stable` (versiones estables) o `beta` (también las de prueba, para los probadores) |
| `updates.auto_download` | sí/no | sí | Bajar la versión nueva en segundo plano apenas se encuentra |
| `updates.install` | texto | `on_close` | `on_close` (al cerrar el programa), `ask` (preguntar) o `manual` (solo con el botón) |

Están en Configuración › General (`preferences.md`, que ya tiene «Notify about new releases»: lo reemplaza
`updates.channel`).

## Reglas

1. **Buscar:** al abrir, si pasó un día desde la última vez, el programa pide la lista de versiones del canal elegido.
   Sin internet no avisa nada y vuelve a probar al día siguiente. También hay un botón «Buscar actualizaciones» en Más
   › Ayuda.
2. **Qué versión le corresponde:** la más nueva del canal para su sistema y su familia de paquetes (actual o clásica
   de 32 o 64 bits, `legacy_windows.md`, regla 1). Una versión que pide un sistema más nuevo que el de la máquina no se
   ofrece; Acerca de dice por qué.
3. **Avisar:** una notificación chica en la barra de arriba, «Versión 1.0.1 lista · Ver novedades», que no tapa nada
   ni interrumpe. «Ver novedades» muestra las notas de esa versión en el idioma del programa.
4. **Bajar:** en segundo plano, con el avance en la notificación, sin frenar la vista ni el laminado (usa poco ancho de
   banda y se pausa mientras se manda un archivo a una impresora). Si se corta, sigue desde donde quedó.
5. **Comprobar antes de instalar:** el paquete tiene que coincidir con su resumen SHA-256 y con la firma Ed25519 de la
   clave del proyecto. Si no coincide, se borra y no se instala, y el aviso dice «La descarga llegó dañada; se va a
   intentar de nuevo».
6. **Instalar:** según `updates.install`:
   - `on_close`: al cerrar el programa se instala en unos segundos, sin preguntar; la próxima vez abre la versión nueva;
   - `ask`: un botón «Reiniciar y actualizar» en la notificación; si hay cambios sin guardar, primero pregunta;
   - `manual`: solo con ese botón.
   Nunca se instala mientras se lamina, se calcula Ingeniería o se manda un archivo a una impresora.
7. **Sin permisos de administrador:** el programa se instala por usuario (en su carpeta de programas), así la
   actualización no pide permiso cada vez. Si se instaló para todos los usuarios, la actualización pide el permiso
   una vez, como cualquier instalador.
8. **Volver atrás:** se guarda la versión anterior. Si la nueva no arranca dos veces seguidas, el programa vuelve solo a
   la anterior y lo dice. En Configuración › General hay «Volver a la versión anterior».
9. **Lo que no se toca:** proyectos, perfiles del usuario, preferencias, calibraciones guardadas y la carpeta de
   descargas. Si una versión nueva cambia el formato de las preferencias, guarda una copia de las viejas antes.
10. **Perfiles aparte:** el paquete de perfiles se sigue actualizando solo, por su cuenta (`profile_pack.md`); el
    actualizador del programa no lo reemplaza.
11. **Cada sistema:**
    - Windows: el instalador y la actualización son el mismo paquete; el programa abierto no se reemplaza mientras
      corre: un ayudante chico lo hace al cerrar;
    - Linux: el paquete AppImage se reemplaza por el nuevo; con paquetes del sistema, avisa y deja que actualice el
      sistema;
    - Android: por la tienda de aplicaciones donde se publique; si se instaló el APK a mano, el aviso lleva al nuevo
      APK firmado con la misma clave.
12. **Componentes:** lo que viene con el programa (el componente de Diseño, `cad_design.md`) se actualiza con él y se
    comprueba igual.

13. **La lista de versiones** (decisión del equipo de fichas, 04-11-2026, para que el programa de escritorio y la app
    lean lo mismo):
    - **dónde:** `updates.json` y su firma `updates.json.sig` (64 bytes, Ed25519 sobre los bytes exactos del JSON, con
      la misma clave del proyecto), como archivos de una release fija de etiqueta `updates` del repositorio del motor,
      que el flujo de publicación reemplaza en cada versión (como `nightly`):
      `https://github.com/adriangrochi-code/tisma_engine/releases/download/updates/updates.json`. Tisma Cloud la sirve
      igual en `GET /v1/updates` y `GET /v1/updates.sig`, sin sesión, como segunda dirección si la primera no
      responde;
    - **comprobar:** sin una firma válida la lista se descarta entera; después cada paquete se comprueba por su
      SHA-256 y su propia firma (regla 5);
    - **el JSON:**

      ```json
      {
       "format": "tisma-updates",
       "version": 1,
       "generated": "2027-01-12T10:00:00Z",
       "releases": [
        {
         "version": "1.0.1",
         "channel": "stable",
         "date": "2027-01-20",
         "notes": { "es": "…", "en": "…" },
         "packages": [
          {
           "system": "windows-x64",
           "family": "current",
           "min_system": "10.0.17763",
           "url": "https://…/TismaSetup-1.0.1-x64.exe",
           "size": 123456789,
           "sha256": "…",
           "signature": "…"
          }
         ]
        }
       ]
      }
      ```

    - **campos:** `version` según `docs/VERSIONING.md` (con etiqueta de pre-release si la tiene); `channel` `stable` o
      `beta` (con `beta` elegido se ofrecen las dos); `notes` en cada idioma del programa (si falta el del programa, en
      inglés); `system` uno de `windows-x64`, `windows-x86`, `linux-x64`, `android`; `family` `current` o `legacy`
      (`legacy_windows.md`); `min_system` la versión mínima del sistema (en Android, el nivel de API, por ejemplo
      `"26"`); `size` en bytes; `sha256` en hexadecimal; `signature` la firma Ed25519 del paquete en base64;
    - **orden:** de la más nueva a la más vieja; el programa elige la primera de su canal, sistema y familia cuyo
      `min_system` cumple la máquina (regla 2), y la ofrece solo si es más nueva que la instalada;
    - una clave desconocida se ignora; un `format` distinto o una `version` mayor de la lista, se ignora la lista y se
      vuelve a probar al día siguiente.

## Casos límite

- Dos ventanas del programa abiertas: instala cuando se cierra la última.
- Disco lleno: no baja y avisa cuánto espacio falta.
- Reloj de la máquina muy atrasado: la comprobación de la firma no depende de la fecha.
- Una versión retirada (con un error grave) desaparece de la lista; quien ya la tiene recibe la siguiente.
- Red con proxy de empresa: usa el proxy del sistema.

## Pruebas

| # | Entrada | Resultado esperado |
|---|---|---|
| AU1 | Versión 1.0.0 instalada, 1.0.1 publicada en `stable` | Al abrir, el aviso «Versión 1.0.1 lista»; la descarga termina sin frenar la vista; al cerrar se instala; al abrir dice 1.0.1 en Acerca de |
| AU2 | Paquete con un byte cambiado | No se instala; aviso de descarga dañada; la versión instalada sigue igual |
| AU3 | Paquete firmado con otra clave | No se instala |
| AU4 | Cortar la red a mitad de la descarga y volver | Sigue desde donde quedó |
| AU5 | Versión nueva que no arranca (prueba con un paquete roto a propósito) | Al segundo intento vuelve sola a la anterior y lo dice |
| AU6 | Laminando cuando se elige «Reiniciar y actualizar» | Espera a que termine el laminado |
| AU7 | Windows Vista con la familia clásica, versión nueva solo para Windows 10 | No se ofrece; ofrece la última clásica |
| AU8 | Proyecto, perfiles y preferencias antes y después de actualizar | Iguales byte por byte, salvo el cambio de formato anunciado |
| AU9 | `updates.channel` = `beta` | Se ofrecen también las versiones de prueba; con `stable`, no |
| AU10 | `updates.json` con un byte cambiado y la firma vieja | La lista se descarta; no se ofrece nada |
| AU11 | Lista con una versión cuyo `min_system` es más nuevo que la máquina | No se ofrece; Acerca de dice por qué |
| AU12 | GitHub no responde, Tisma Cloud sí | La lista llega por `GET /v1/updates` |
