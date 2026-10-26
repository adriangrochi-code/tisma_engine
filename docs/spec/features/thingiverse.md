# Thingiverse dentro del programa

**Qué hace:** busca modelos en Thingiverse sin salir del programa y los carga en la placa con un botón.

**Fase:** después de la primera versión.

**Fuentes:**
- la página Thingiverse de Tisma Slicer (Más › Thingiverse), vista en su interfaz;
- Thingiverse, el sitio (`https://www.thingiverse.com/`) y su documentación para programadores
  (`https://www.thingiverse.com/developers`).

**Lo que no se pudo verificar:** el 26-10-2026 el equipo de fichas no pudo leer la documentación de la API de
Thingiverse: el sitio la bloquea desde su red. Por eso esta ficha no dice rutas ni parámetros de la API (regla 9). El
camino 1 no la necesita.

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

## Camino 2: buscar con la API (para después)

9. **Solo con la documentación oficial:** una búsqueda propia del programa (lista de resultados con miniatura, nombre,
   autor y licencia, sin la vista web) usa la API pública de Thingiverse. Se programa solo después de que el equipo
   del motor:
   - lea la documentación y las condiciones de uso de la API en `https://www.thingiverse.com/developers`;
   - anote en `docs/` del motor las rutas que usa, con la fecha y el enlace;
   - registre la aplicación del motor en Thingiverse, si la API lo pide.
10. **La clave de la aplicación**, si la API pide una, es un secreto: no va en el repositorio ni en los registros.
11. **La licencia de cada modelo:** los modelos de Thingiverse tienen la licencia que eligió su autor (por ejemplo,
    Creative Commons con o sin uso comercial). La búsqueda la muestra antes de bajar, y el proyecto la guarda con la
    dirección (regla 8).
12. **Créditos:** Thingiverse no lleva «Basado en» (`credits.md`, regla 2): no se toma nada de él, es un servicio que
    se usa. La página dice que el contenido es de Thingiverse y de sus autores.

## En la app

13. La app (`mobile_app.md`, regla 10) abre los modelos que llegan de un enlace de Thingiverse con «Compartir con
    Tisma». La página con el sitio adentro no está en la app.

## Pruebas

Con un servidor de prueba que sirve páginas y archivos con las mismas direcciones (el programa acepta otra dirección
para Thingiverse solo en las pruebas).

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
