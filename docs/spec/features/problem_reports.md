# Informar un problema

**Qué hace:** desde el programa, la persona cuenta qué falló o qué le gustaría, y eso llega al equipo del motor por
Tisma Cloud. Puede sumar la versión, el sistema, la impresora y el registro de la sesión, que ve antes de mandar.

**Fase:** después de la primera versión, con `cloud.md`.

**Fuentes:**
- el diálogo «Report a problem» de Tisma Slicer, visto en su interfaz;
- lo que acepta y devuelve el servidor (`cloud.md`, regla 11), descrito por el equipo que lo escribió.

**Dónde:**
- Más › Ayuda › «Report a problem…» (`interface.md`, menú Help);
- el aviso de un cierre inesperado (regla 10);
- en la app, Más › «Informar un problema» (`mobile_app.md`, regla 9).

## El diálogo

1. **Título:** «Report a problem». Debajo, en gris:
   - «Tell what went wrong, or what you would like Tisma to do. It goes to the Tisma team, which groups the reports of
     the same problem to fix the most frequent ones first.»
   - «E-mails, addresses, the user name in the paths, keys and serial numbers are removed before it is kept.»
2. **Campos**, en este orden:

| Campo | Qué es | Va al servidor como |
|---|---|---|
| Kind | Tres botones: «Something does not work», «Tisma closed», «Suggestion» | `kind`: `bug`, `crash`, `suggestion` |
| In a few words | Una línea; de ejemplo, gris: «e.g. The supports are missing under the overhang» | `title`, de 3 a 200 caracteres |
| What happened | Varias líneas | `description` |
| Steps to make it happen again | Varias líneas | `steps` |
| What you expected | Una línea | `expected` |
| Tisma Cloud server | Solo si no hay servidor en Preferences | La dirección del pedido (regla 5) |

   - `description`, `steps` y `expected`: hasta 10 000 caracteres cada uno.
   - `app` es `pc` en el escritorio y `android` en la app.
3. **Adjuntar:** la casilla «Attach the version, the system, the printer in use and the log of this session»,
   encendida por defecto. Al lado, «See what is attached» abre una ventana con el texto exacto que se va a mandar.
   - `app_version`: la versión del programa;
   - `os`: el sistema, hasta 80 caracteres;
   - `log`: arriba, el nombre y la versión del programa, el sistema y la impresora en uso con el perfil de impresión y
     el de filamento (o resina); debajo, el registro de la sesión. Si pasa de 190 000 caracteres, se manda el final.

   Con la casilla apagada no van `app_version`, `os` ni `log`.
4. **Abajo:**
   - una línea con la cuenta: «Sent with the account <correo>: its reports and their state are in the copy of its
     data.», o «Sent without an account.»;
   - los botones «Send» (principal) y «Cancel».

## Mandar

5. **Antes de mandar:**
   - si «In a few words» tiene menos de 3 caracteres, la línea de estado dice «Write in a few words what happened.» y
     el cursor va a ese campo;
   - si hace falta servidor y está vacío: «Enter the address of the Tisma Cloud server.». Sin `http://` ni `https://`,
     se agrega `https://`.
6. **El pedido:** `POST /v1/reports` (`cloud.md`, regla 11), con la sesión si hay una (`Authorization`) y sin ella si
   no. El programa no espera más de 30 s (diseño).
7. **Si sale bien:** el diálogo se cierra y un aviso dice «Thank you: the report was sent to the Tisma team.»
   - El servidor contesta cuántos informes anteriores son del mismo problema (`similar`).
   - Si son más de 0, el aviso suma: «The same problem was reported N times before: it is among the ones to fix.»
   - La dirección del servidor escrita en el diálogo queda guardada en Preferences.
8. **Si falla:** el diálogo queda abierto con lo escrito y la línea de estado dice «The report could not be sent:
   <motivo>», con el texto del servidor (`cloud.md`, regla 4). Un 429 dice que se mandaron demasiados y que se pruebe
   en una hora.
9. **Nada se manda solo:** el programa nunca manda un informe sin que la persona toque «Send».

## Después de un cierre inesperado

10. **Al abrir de nuevo** (diseño: Tisma Slicer no lo hace) después de un cierre inesperado, el motor ofrece informarlo.
    - Abre el diálogo con «Tisma closed» elegido y el registro de la sesión que se cerró.
    - La persona escribe el resto o cancela. Si cancela, no se pregunta de nuevo por ese cierre.

## Mis informes

11. **Con cuenta** (diseño: en Tisma Slicer la lista solo está en la copia de los datos de la cuenta): Más › Tisma
    Cloud › «My reports» lista los informes propios (`GET /v1/me/reports`).
    - De cada uno se ve la fecha, el título y el estado: nuevo, confirmado, arreglado, no se arregla o repetido.
    - Un informe arreglado muestra la versión que lo arregla, si el servidor la manda.

## Lo que hace el servidor

Va en el servidor, no en el motor. Se anota para que la pantalla lo explique bien:
- quita los datos personales;
- agrupa los informes del mismo problema;
- los clasifica y los traduce;
- puede premiar a quien mandó un informe útil.

## Pruebas

Contra el servidor de prueba de `cloud.md`.

| # | Escenario | Resultado esperado |
|---|---|---|
| IP1 | Abrir el diálogo | Campos en el orden de la regla 2; «Something does not work» elegido; casilla de adjuntar encendida |
| IP2 | «Send» con «In a few words» vacío | No se manda; el aviso de la regla 5; el cursor en el campo |
| IP3 | Mandar sin cuenta, con adjuntos | Pedido sin `Authorization`; `app` `pc`; `log` empieza con el programa, el sistema y la impresora |
| IP4 | Mandar con la casilla apagada | Sin `app_version`, `os` ni `log` |
| IP5 | «See what is attached» | Muestra exactamente el `log` que después se manda |
| IP6 | El servidor contesta `similar` 3 | El aviso suma «reported 3 times before» |
| IP7 | El servidor contesta 429 | El diálogo sigue abierto con el texto; el aviso de demasiados informes |
| IP8 | Registro de más de 190 000 caracteres | Se manda el final; el total no pasa de 190 000 |
| IP9 | Cierre forzado del programa y abrirlo de nuevo | Ofrece informarlo con «Tisma closed» elegido |
| IP10 | «My reports» con dos informes | Los dos, el más nuevo primero, con su estado |
