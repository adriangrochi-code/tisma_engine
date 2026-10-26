# Thingiverse dentro del programa

**Qué hace:** busca modelos en Thingiverse sin salir del programa y los carga en la placa con un botón.

**Fase:** después de la primera versión.

**Fuentes:**
- la página Thingiverse de Tisma Slicer (Más › Thingiverse), vista en su interfaz;
- Thingiverse, el sitio (`https://www.thingiverse.com/`), su documentación para programadores
  (`https://www.thingiverse.com/developers`) y el contrato de la API (`https://www.thingiverse.com/legal/api`);
- para la API (camino 2), fuentes secundarias, porque el 26-10-2026 las páginas oficiales no se pudieron abrir desde
  la red del equipo de fichas (error 403):
  - Openverse (WordPress), «Provider API – Thingiverse», 2021: pedidos y respuestas reales de cosas y archivos;
  - Women in 3D Printing, «How to use the Thingiverse API», 2017: registro de la aplicación y token;
  - un cliente público en Go (licencia Apache 2.0, 2021): la ruta de búsqueda y la forma de su respuesta;
  - el resumen del contrato de la API que dio el buscador.

  De esas fuentes solo se tomaron los hechos del protocolo (rutas, parámetros, campos), no código. La regla 20 dice
  cómo se confirman antes de programar.

## Camino 1: el sitio dentro del programa

Es lo que hace Tisma Slicer.

1. **Dónde:** Más › Thingiverse («Models to print from Thingiverse»). La página ocupa el lugar de la vista 3D.
2. **La página:** muestra el sitio de Thingiverse en una vista web dentro del programa, con la cuenta de Thingiverse
   de la persona si entra. Arriba, una barra con:
   - **Back:** vuelve a la página anterior; apagado si no hay;
   - **Home:** vuelve a la portada de Thingiverse;
   - a la derecha, **Open in the browser:** abre la página actual en el navegador del sistema;
   - **Download and import this thing:** baja todos los archivos de la cosa que se está viendo (el botón «Download all
     files» del sitio) y los carga en la placa. Solo está encendido en la página de una cosa, es decir, una dirección
     `https://www.thingiverse.com/thing:<número>`.
3. **Descargas del sitio:** los enlaces del sitio que bajan un modelo no abren el diálogo de descarga del sistema:
   - el archivo baja dentro de la sesión del sitio y se guarda en la carpeta de descargas (`preferences.md`,
     «Download path»);
   - después se abre como «Import» (`formats.md`).

   Vale para archivos `.stl`, `.3mf`, `.obj`, `.amf`, `.step`, `.stp` y `.zip` de `thingiverse.com` o de sus
   subdominios. Los enlaces que se abren en otra ventana: si son un modelo, se bajan; si no, se abren en la misma
   página.
4. **Un ZIP:** abre un selector con los modelos que tiene adentro, para elegir cuáles cargar. Es distinto de abrir un
   ZIP desde el disco (`formats.md`, regla 17), que los abre todos: una cosa de Thingiverse suele traer variantes.
5. **Mientras baja:** un aviso «Downloading from Thingiverse...». Dos clics al mismo archivo lo bajan una sola vez.
6. **Si falla:**
   - si el sitio no deja bajar desde la página, el programa intenta bajar el archivo él mismo, solo desde
     `thingiverse.com`;
   - si llega una página en lugar de un archivo (el sitio pide una comprobación o lo movió): «Thingiverse did not send
     a model file. Try the download button of the file on the page.»;
   - otro error: «The file could not be downloaded from Thingiverse: <motivo>».
7. **Seguridad:**
   - solo se bajan archivos de `thingiverse.com` y sus subdominios, por HTTPS;
   - un archivo bajado nunca se ejecuta: solo se abre como modelo;
   - la preferencia «Allow downloads from supported websites» apagada apaga también las descargas de esta página.
8. **De dónde vino:** al cargar, el proyecto guarda la dirección de la cosa y el nombre del archivo (diseño). La lista
   de objetos lo muestra al pasar el ratón, y «Open in the browser» del objeto vuelve a esa página.

## Camino 2: buscar con la API

La búsqueda propia del programa: resultados en una lista del programa, con la licencia a la vista, y descarga directa
a la placa. El camino 1 sigue como respaldo: entrar con la cuenta, comentarios y lo que la API no cubra.

### Cómo llega el programa a la API

9. **Dos transportes, las mismas respuestas:**
   - **por Tisma Cloud** (por defecto): el programa pide `<servidor>/v1/thingiverse/<ruta>` sin token de
     Thingiverse. El servidor agrega el token de la aplicación de Tisma y devuelve la respuesta de Thingiverse sin
     cambiarla (regla 21). Funciona con o sin cuenta de Tisma Cloud;
   - **directo** (avanzado): si la persona pone su propio token en Preferences › Other › «Thingiverse app token», el
     programa pide `https://api.thingiverse.com/<ruta>` con `Authorization: Bearer <token>`.

   El resto del programa no sabe cuál se usa: las rutas, los parámetros y las respuestas son los mismos.
10. **El token es un secreto:**
    - el de la aplicación de Tisma vive solo en el servidor; nunca va en el programa, en el repositorio ni en el
      paquete, porque cualquiera podría sacarlo de ahí;
    - el token propio de la persona se guarda en el almacén de claves del sistema, como el de Tisma Cloud
      (`cloud.md`, regla 3), y nunca en `interface.json`, en registros ni en las preferencias exportadas
      (`preferences.md`, regla 6).
11. **Registrar la aplicación:** lo hace el dueño, en `https://www.thingiverse.com/apps/create`, como «Web App», y
    acepta el contrato de la API. De ahí salen el Client ID, el Client Secret y el App Token, que van solo al servidor.

### Lo que se pide

12. **Rutas** (todas `GET`; «a confirmar» hasta la regla 20):

| Ruta | Para qué | Parámetros | Campos que usa el programa | Fuente |
|---|---|---|---|---|
| `search/` | Buscar | `q` (el texto; a confirmar), `type=things`, `sort`, `page`, `per_page` | `total`; en `hits`: `id`, `name`, `thumbnail`, `public_url`, `creator.name`, `like_count`, `is_nsfw` | Cliente en Go |
| `popular`, `newest` | Portada sin texto | `page`, `per_page` | Los de una cosa en una lista | Women in 3D Printing; Openverse |
| `things/{id}` | Ficha de una cosa | — | `name`, `creator.name`, `public_url`, `license`, `description`, `files_url`, `images_url` | Openverse |
| `things/{id}/images` | Fotos | — | Las direcciones de cada tamaño | Openverse |
| `things/{id}/files` | Archivos | — | `id`, `name`, `size`, `thumbnail`, `download_url` | Openverse |
| `files/{id}/download` | Bajar un archivo | — | El archivo, o una redirección a él | Openverse |

    - `per_page`: 20 en la búsqueda.
    - `sort`: solo `popular` está visto en la API. Si la regla 20 confirma «relevancia» u otros, se suman a la lista
      «Ordenar por».
13. **Pocos pedidos:**
    - un pedido por acción de la persona (buscar, pasar de página, abrir una cosa, bajar);
    - nada en segundo plano, ni recorrer páginas solo, ni bajar por adelantado;
    - como mucho 30 pedidos por minuto desde el programa (diseño). Pasado eso, espera.

### La pantalla

14. **Más › Thingiverse** tiene dos pestañas: **Buscar** (este camino, por defecto) y **Sitio** (camino 1).
15. **Buscar:**
    - un campo de texto, «Ordenar por» y, sin texto, la lista de populares;
    - resultados en tarjetas: miniatura, nombre, autor y me gusta; abajo, «Más resultados»;
    - las cosas con `is_nsfw` verdadero no se muestran (diseño);
    - al pie: «Modelos de Thingiverse. Cada uno es de su autor, con su licencia.».
16. **Una cosa:** al tocar una tarjeta se abre su ficha, al lado de la lista:
    - fotos, nombre, autor y la licencia, siempre a la vista antes de bajar;
    - la lista de archivos, cada uno con miniatura, nombre y tamaño, y su botón «Importar»;
    - «Importar todo» y «Abrir en Thingiverse» (abre `public_url` en la pestaña Sitio).
17. **Bajar e importar:**
    - el archivo baja por el mismo transporte (regla 9) y se guarda en la carpeta de descargas (`preferences.md`,
      «Download path»);
    - se aceptan las extensiones del camino 1 (regla 3) y hasta 500 MB por archivo (diseño);
    - si llega una página HTML en lugar de un archivo, el aviso de la regla 6;
    - después se abre como «Import», y un ZIP abre el selector de la regla 4;
    - el proyecto guarda `public_url`, el autor y la licencia (regla 8).
18. **Errores:**
    - **401 o 403:** «Thingiverse no aceptó el pedido». Con el transporte directo, el aviso suma «Revise el token en
      Preferences»;
    - **429:** «Thingiverse pide esperar», con los segundos de `Retry-After` si vienen. No se reintenta solo;
    - **sin red o sin servidor:** el aviso y la oferta de usar la pestaña Sitio;
    - una respuesta sin los campos de la regla 12: la tarjeta se muestra con lo que haya, y el registro lo anota.

### El contrato de uso de la API

19. Lo que dice el contrato (`https://www.thingiverse.com/legal/api`, según el resumen del buscador; se confirma en la
    regla 20) y cómo lo cumple el programa:

| Pide el contrato | Cómo se cumple |
|---|---|
| El contenido se muestra solo dentro de la aplicación | Las tarjetas y las fichas solo están en el programa y en la app; no se publican en ningún sitio |
| No guardar el contenido, salvo de forma intermedia | Los resultados viven en la memoria mientras la pantalla está abierta; solo se guardan los archivos que la persona baja y, en el proyecto, de dónde vinieron |
| Nada de robots ni scraping | Regla 13: pedidos solo por acciones de la persona |
| No mezclar resultados de modo que no se sepa que son de Thingiverse | Pestaña propia de Thingiverse, con su nombre al pie; los resultados nunca se mezclan con los de otros sitios |
| Límite de pedidos a criterio de Thingiverse | Regla 13 y el límite del servidor (regla 21); un 429 se respeta (regla 18) |

### Antes de programar: confirmar

20. **Confirmación con un token real:** antes de programar este camino, el equipo del motor, con el token propio del
    dueño o uno de prueba:
    - lee la referencia de la API (`https://www.thingiverse.com/developers`) y el contrato completo, y anota en
      `docs/` del motor la fecha y lo que cambió de esta ficha;
    - hace un pedido de cada ruta de la regla 12 y lo guarda como intercambio en `thingiverse/` (pruebas), en JSONL
      como `printer_connections.md` («Integración en sala limpia»), con el token como `<secreto>`;
    - confirma: el nombre del parámetro del texto de búsqueda; los valores de `sort`; si `download_url` baja con el
      token de la aplicación o pide que la persona entre con su cuenta;
    - si algo no coincide, se corrige esta ficha con lo medido. Nada se adivina.

    Si `download_url` pide la cuenta de la persona, la descarga va por el camino 1 hasta que haya una ficha para
    entrar con la cuenta de Thingiverse.

### Lo que hace el servidor

21. **En Tisma Cloud** (programa del dueño, `cloud.md`; se anota para que el contrato quede claro):
    - `GET /v1/thingiverse/<ruta>` acepta solo las rutas de la regla 12, con sus parámetros, y rechaza el resto (404);
    - agrega el token de la aplicación y pasa a Thingiverse; devuelve el código y el cuerpo sin cambios, y en una
      descarga, el archivo;
    - no guarda las respuestas;
    - limita los pedidos por cuenta o por dirección (por ejemplo 30 por minuto) y contesta 429 con `Retry-After`;
    - nunca devuelve el token.

### Créditos

22. Thingiverse no lleva «Basado en» (`credits.md`, regla 2): no se toma nada de él, es un servicio que se usa. La
    pantalla dice que el contenido es de Thingiverse y de sus autores (regla 15).

## En la app

23. La app (`mobile_app.md`, regla 10) abre los modelos que llegan de un enlace de Thingiverse con «Compartir con
    Tisma». La pestaña Buscar (camino 2) también está en la app, con el transporte por Tisma Cloud; la pestaña Sitio
    no.

## Pruebas

Con un servidor de prueba que sirve páginas y archivos con las mismas direcciones (el programa acepta otra dirección
para Thingiverse solo en las pruebas). Para el camino 2, el servidor de prueba contesta con los intercambios grabados
en la regla 20.

| # | Escenario | Resultado esperado |
|---|---|---|
| TV1 | Abrir Más › Thingiverse | La portada; Back apagado; «Download and import this thing» apagado |
| TV2 | Ir a `thing:123` | «Download and import this thing» encendido |
| TV3 | Tocar el enlace de un `.stl` | Se guarda en la carpeta de descargas y se carga en la placa; no aparece el diálogo del sistema |
| TV4 | «Download and import this thing» en una cosa con un `.zip` de 3 modelos | Selector con los 3 modelos |
| TV5 | Enlace de descarga que devuelve una página HTML | El aviso de «did not send a model file»; nada en la placa |
| TV6 | Enlace a un `.stl` de otro sitio | No se baja |
| TV7 | Dos clics seguidos al mismo archivo | Se baja una vez |
| TV8 | «Allow downloads from supported websites» apagado | Los enlaces de modelos no bajan nada |
| TV9 | Proyecto con un modelo bajado de `thing:123`, guardado y abierto | El objeto sigue mostrando la dirección de `thing:123` |
| TV10 | Buscar «benchy» por Tisma Cloud | Un pedido `search/` con `type=things` y `per_page=20`; sin token de Thingiverse en el pedido; tarjetas con miniatura, nombre, autor y me gusta |
| TV11 | La misma búsqueda con el token propio | El pedido va a la API directo con `Authorization: Bearer`; las mismas tarjetas que en TV10 |
| TV12 | Abrir una cosa | Fotos, licencia y archivos con su tamaño, antes de cualquier descarga |
| TV13 | «Importar» un `.stl` | Baja por el mismo transporte, queda en la carpeta de descargas y se carga en la placa; el proyecto guarda la dirección, el autor y la licencia |
| TV14 | Resultado con `is_nsfw` verdadero | No se muestra |
| TV15 | La API contesta 429 con `Retry-After: 60` | El aviso con los 60 s; ningún pedido más hasta que la persona vuelva a tocar |
| TV16 | 31 búsquedas en un minuto | La 31 espera; nunca más de 30 pedidos por minuto |
| TV17 | Abrir la pantalla y no tocar nada | Ningún pedido más que la portada |
| TV18 | Revisar `interface.json`, los registros y las preferencias exportadas | El token propio no aparece |
| TV19 | Servidor: pedir `/v1/thingiverse/users/alguien` | 404: la ruta no está en la regla 12 |
| TV20 | Servidor: cualquier respuesta | Nunca trae el token de la aplicación |
| TV21 | Sin red | El aviso y la oferta de la pestaña Sitio |
