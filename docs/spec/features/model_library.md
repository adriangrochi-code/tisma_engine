# Biblioteca de Tisma Cloud: modelos listos para imprimir

**Qué hace:** un sitio de modelos propio de Tisma Cloud, como Thingiverse, donde cada modelo puede traer más que el
archivo:
- **perfiles probados** por su autor, por impresora;
- **laminados hechos**, listos para bajar o mandar a la impresora;
- **su análisis de Ingeniería** (FEA o CFD), con lo que aguanta la pieza a la vista: veredicto, carga, factor de
  seguridad, deformación y el mapa de colores.

Desde el programa, la app o la página se elige el modelo y la impresora, y se manda a imprimir en dos toques. Si no
hay un laminado hecho para esa impresora, el servidor lo lamina.

**Fase:** más adelante, después de `cloud.md` y `mobile_app.md`.

**Pedido del dueño (26-10-2026):** una página de Tisma Cloud como Thingiverse, con descargas ya laminadas con perfiles
y los datos de FEA o CFD de la pieza; laminar gratis una pieza ya hecha y sin límite en los planes pagos; mandar a
imprimir con la comodidad de una app de impresora en el teléfono.

**Relación con otras fichas:**
- `cloud.md`: cuenta, trabajos en la nube, puentes de impresoras. Esta ficha usa lo mismo.
- `structural_analysis.md` y `aerodynamics.md`: los análisis que se muestran.
- `thingiverse.md`: Thingiverse sigue aparte. Sus modelos no se copian a la biblioteca (regla 4).

## Qué tiene un modelo

1. **La ficha de un modelo:**

| Parte | Qué es | Obligatoria |
|---|---|---|
| Modelo | El proyecto 3MF del autor: piezas, posición en la cama, pintura y ajustes por objeto (`formats.md`) | Sí |
| Datos | Nombre, autor, descripción, fotos, etiquetas y licencia | Sí |
| Perfiles probados | Por cada impresora y boquilla en que el autor lo imprimió: los perfiles de impresión y de material, y una foto de la pieza impresa si quiere | No |
| Laminado del autor | El G-code con que el autor lo imprimió, con su perfil (regla 5) | No, pero se pide al publicar |
| Laminados hechos | G-code por perfil probado, hecho por el servidor (regla 7) o preparado de antemano (regla 15) | No: se hacen al publicar o cuando se piden mucho |
| Ingeniería | El análisis guardado en el proyecto: material, apoyos, cargas, resultado (`.tea`) y su resumen | No |

2. **El resumen de Ingeniería**, a la vista en la ficha, sin abrir nada:
   - **FEA:** veredicto (aguanta o no), la carga del análisis con su dirección, el factor de seguridad, la deformación
     máxima, el material, el relleno y las paredes con que se calculó;
   - **CFD:** el arrastre y la velocidad del aire del análisis;
   - el mapa de colores sobre la pieza, en un visor 3D en la página y en la app;
   - y siempre: «Calculado con estos ajustes: <capa, paredes, relleno, patrón, material, orientación>».
3. **El análisis vale solo con esos ajustes:** el veredicto se muestra como válido si el laminado usa los mismos ajustes
   que cambian la resistencia (los de la línea anterior).
   - Si la persona cambia alguno, la ficha dice «El análisis no vale para estos ajustes» y ofrece calcular de nuevo,
     que es un trabajo de análisis normal de `cloud.md` y cuenta como tal en el plan.
   - Cambiar lo que no toca la resistencia (color, velocidad de viaje, falda) deja el análisis válido.
4. **Solo modelos de sus autores:** sube un modelo quien lo hizo o tiene derecho a publicarlo, y elige la licencia. El
   formulario de subida pide aceptar que Tisma Cloud lo guarde, lo lamine y reparta los laminados. Nunca se copian
   modelos de Thingiverse ni de otros sitios.

## Publicar

5. **Desde el programa:** Más › Tisma Cloud › «Publicar en la biblioteca» con un proyecto abierto.
   - El programa manda el 3MF con los perfiles en uso y, si hay, el análisis.
   - La persona marca en qué impresoras lo imprimió («perfil probado»), agrega fotos y elige la licencia.
   - **El laminado del autor:** el programa ofrece subir también el G-code con que lo imprimió, el último exportado
     para ese proyecto. También se puede subir a mano un G-code (texto o binario) de otro laminador. Queda en la ficha
     como «Laminado del autor», con su impresora, su perfil y el programa que lo hizo, y se baja o se manda a la
     impresora sin gastar cómputo.
6. **Revisión:** el servidor revisa el 3MF como cualquier subida (`cloud.md`; archivos peligrosos) y que el análisis
   corresponda a la pieza. El laminado del autor se revisa como el G-code que se manda a imprimir a distancia (sin
   órdenes que cambien la impresora en lugar de imprimir) y que su impresora sea la del perfil que dice.
7. **Laminados hechos:** al publicar, el servidor lamina una vez cada perfil probado con el motor (`cloud.md`,
   regla 18) y guarda el G-code con su resumen (tiempo, filamento y capas). Si el motor cambia de versión, se vuelven a
   laminar cuando alguien los pide (regla 14).
8. **Cambios:** el autor puede subir una versión nueva. Las anteriores quedan, con su número; los laminados y el
   análisis van con su versión.

## Imprimir en dos toques

9. **En la ficha del modelo**, el botón principal es «Imprimir»:
   1. **Impresora:** las de la cuenta (`cloud.md`, puentes) y la del programa. Arriba, las que tienen perfil probado.
   2. **Material:** el cargado en la impresora si ella lo informa; si no, se elige.
   3. **Ajustes:** el perfil probado tal cual, o «Ajustar»: los ajustes rápidos de `settings_panel.md`. Si un cambio
      invalida el análisis, se avisa antes (regla 3).
   4. **Mandar:** el G-code va a la impresora por el puente y empieza. La pantalla pasa al estado de la impresora, con
      la cámara (`cloud.md`, reglas 13 a 15).
10. **Sin impresora conectada:** «Bajar G-code», o «Abrir en Tisma» para abrir el proyecto en el programa.

## Laminado gratis

**Decisión del dueño (26-10-2026):** muchos laminados gratis por mes, con los perfiles que ya están en el servidor. Un
10 % del cómputo del servidor queda reservado solo para eso. El objetivo es que mucha gente pueda mandar a imprimir
desde el teléfono sin pagar.

11. **Qué se lamina gratis:** un modelo de la biblioteca con perfiles que ya están en el servidor:
    - los perfiles probados del autor (regla 1);
    - los perfiles del paquete libre (`profile_pack.md`) para la impresora, la boquilla y el material elegidos;
    - con los cambios de los ajustes rápidos (`settings_panel.md`) encima de esos perfiles.

    **Cada cuenta tiene al menos 2 laminados gratis por día** que gastan cómputo. Lo que no gasta cómputo (regla 14)
    no cuenta y no tiene límite. Todo sale del 10 % reservado (regla 13).
12. **Un perfil propio:** el que la persona armó en su computadora no está en el servidor. Para usarlo:
    - el programa lo sube a la cuenta («Guardar en Tisma Cloud», una vez; después se actualiza solo si cambia);
    - laminar con un perfil propio usa las horas del plan, como cualquier trabajo de `cloud.md` (a decidir por el
      dueño: si también entra en el laminado gratis).
13. **El 10 % reservado:**
    - el servidor guarda para el laminado gratis el 10 % del tiempo de sus nodos, medido en una ventana corrida (por
      ejemplo, la última hora). Los trabajos pagos no usan esa parte;
    - si el laminado gratis llena su 10 %, los pedidos esperan en su propia cola, sin rechazo. La pantalla dice el
      lugar en la cola y el tiempo estimado;
    - (a decidir por el dueño) si los nodos están libres, el laminado gratis puede usar más que el 10 %, y lo devuelve
      apenas llega un trabajo pago;
    - para que una sola cuenta no llene la cola gratis: un laminado gratis a la vez por cuenta y los 2 por día de la
      regla 11. Si en el día el 10 % sobra, el servidor puede dar más a quien los pida (a decidir por el dueño).
14. **Lo que no gasta cómputo** (y por eso no cuenta en el 10 %):

| Caso | Qué hace el servidor |
|---|---|
| Hay un laminado hecho para esa impresora, boquilla, material y ajustes | Lo manda tal cual |
| Otra persona ya pidió exactamente lo mismo (misma versión del modelo, perfiles, ajustes y versión del motor) | Manda el que ya hizo |
| La persona elige el laminado del autor (regla 5) | Lo manda tal cual |
| Hay un laminado preparado de antemano (regla 15) | Lo manda tal cual |

    Los modelos populares casi no gastan: el servidor guarda cada laminado de la biblioteca mientras se siga pidiendo
    (por ejemplo, 90 días sin pedidos lo borran).
15. **Preparar de antemano:** el servidor lamina una sola vez lo que muchos van a pedir, para no gastar cómputo después.
    - **Lo que se pide seguido:** cuando un modelo se pidió 10 veces para la misma impresora, boquilla y material
      (con los perfiles del servidor y sin cambios) en los últimos 30 días, el servidor deja ese laminado guardado.
      Desde ahí, los pedidos siguientes no gastan cómputo;
    - **Los 1000 más bajados, nada más:** para los 1000 modelos más bajados de la biblioteca, el servidor prepara el
      laminado para las impresoras más usadas por las cuentas (las de los puentes, por ejemplo las Bambu Lab más
      comunes), con el perfil del paquete y el material más usado en cada una. El resto de los modelos solo se prepara
      por la regla anterior;
    - **Lo que ya se laminó se guarda:** todo laminado que el servidor hace con perfiles del servidor (gratis o pagado
      con las horas de un plan) queda guardado para los pedidos iguales siguientes. Quien pagó usa sus horas una vez, y
      ese cómputo sirve después para todos. Un laminado con un perfil propio (regla 12) se guarda solo para esa cuenta:
      sus ajustes no se comparten;
    - lo prepara con el cómputo que sobra: solo cuando los nodos no tienen trabajos pagos ni gratis esperando, con la
      prioridad más baja. No gasta el 10 % reservado ni frena a nadie;
    - al cambiar la versión del modelo o del motor, lo preparado viejo se borra y se vuelve a preparar.
16. **Guardar G-code ocupando poco:**
    - **comprimido:** cada G-code se guarda comprimido. Medido el 26-10-2026 (tabla de abajo): con xz (nivel 6) un
      G-code de texto queda en el 18 % al 28 % de su tamaño, y se descomprime en 0,05 s a 0,4 s. El G-code binario
      (`gcode_extras.md`) ya viene comprimido, pero menos: el Benchy pesa 0,90 MB en binario y 0,52 MB en texto con xz;
    - **rápido primero, chico después:** al terminar un laminado se guarda con una compresión rápida (gzip: 0,16 s
      para el Benchy, al 31 %). Con el cómputo que sobra, lo que se va a quedar se vuelve a comprimir con xz (1,5 s,
      al 23 %);
    - **una sola copia:** cada G-code se guarda por su huella (el resumen criptográfico del contenido). Dos pedidos que
      dan el mismo archivo ocupan un lugar;
    - **la receta, no solo el archivo:** junto a cada G-code se guarda con qué se hizo: versión del modelo, perfiles,
      ajustes y versión del motor. El motor da el mismo G-code con la misma receta (medido: dos laminados del Benchy con
      el motor dan el mismo archivo byte a byte), así que un G-code borrado se puede volver a hacer;
    - **un tope de espacio:** la biblioteca tiene un espacio máximo para G-code. Al llenarse, se borra primero lo que
      hace más tiempo que nadie pide. Nunca se borra el laminado del autor (no tiene receta) ni lo preparado de los
      1000 más bajados;
    - **el envío también va comprimido:** al puente o a la app se manda comprimido, y se descomprime al llegar.

| G-code (medido) | Texto | gzip 9 | xz 9e | Binario |
|---|---|---|---|---|
| Benchy, Tisma Slicer | 2,29 MB | 0,71 MB | 0,52 MB | 0,90 MB |
| Benchy al doble con gyroid, Tisma Slicer | 11,92 MB | 3,80 MB | 2,68 MB | — |
| Benchy con soportes, Tisma Slicer | 3,74 MB | 1,08 MB | 0,76 MB | — |
| Benchy, motor (`dev` d3e814a) | 7,57 MB | 2,09 MB | 1,34 MB | — |
| Benchy al doble con gyroid, motor | 22,67 MB | 6,95 MB | 4,13 MB | — |

    - **cuánto espacio hace falta:** si cada uno de los 1000 más bajados se prepara para unas 20 combinaciones de
      impresora, boquilla y material, son unos 20 000 G-code. A 0,5 MB a 1,5 MB cada uno con xz, son de 10 GB a 30 GB.
      Lo laminado por pedido entra en el tope de espacio.
    - el motor hoy escribe un G-code tres veces más grande que Tisma Slicer para la misma pieza (7,57 MB contra
      2,29 MB). Se le pasa al equipo del motor para revisar si sobran comentarios o decimales.
17. **Cuántos laminados entran en el 10 %** (medido el 26-10-2026 en un contenedor Linux de 4 núcleos, mediana de 3;
    los nodos reales son otros equipos, pero la relación se mantiene):

| Pieza | Tisma Slicer (hoy en el servidor) | Motor (`dev` d3e814a) |
|---|---|---|
| Benchy (225 102 triángulos) | 3,3 s | 7,0 s |
| Benchy al doble con gyroid 15 % | 5,8 s | 17,8 s |
| Benchy con soportes | 5,5 s | 6,1 s |
| Conejo al 150 % | 1,9 s | 3,4 s |
| Tornillo M3 al 400 % | 1,0 s | 0,3 s |

    - los ajustes no son iguales en los dos (cada uno usa sus valores por defecto). Sirve como orden de magnitud;
    - la capacidad estimada del servidor es de unas 1 300 horas de cómputo por mes (documentación del servidor de Tisma
      Cloud, «Capacidad estimada», del 20-10-2026). El 10 % son unas 130 horas, es decir, unos 470 000 s;
    - con piezas como el Benchy: unos **140 000 laminados por mes con Tisma Slicer** y unos **65 000 con el motor
      como está hoy**. Con 2 por día por cuenta (unos 60 por mes), alcanza para unas 1 100 a 2 300 cuentas que usen
      todos sus laminados todos los días. Como la mayoría no los usa todos, y lo reutilizado y lo preparado no gasta,
      entran muchas más;
    - si el uso real se acerca al 10 %, primero crece lo preparado de antemano (regla 15), que baja el cómputo, y
      después se revisa el número.

    El número real se publica en la página cuando el motor esté medido en los nodos.
18. **Planes pagos:**
    - laminados sin límite, con un tope de trabajos a la vez para que una cuenta no llene la cola;
    - perfiles propios (regla 12) y análisis nuevos con las horas del plan, como hoy;
    - prioridad sobre la cola gratis, en el 90 % restante.
19. **A la vista:** antes de mandar, la pantalla «Imprimir» dice «Listo, sin laminar» (laminado del autor, hecho o
    preparado), «Laminado gratis: te quedan N hoy» o «Usa tu plan», y, si hay cola, el tiempo estimado.
    Siempre está «Abrir en Tisma» para laminar en la computadora, que es gratis y sin cola.

## La página, el programa y la app

20. **La misma biblioteca en los tres lugares:**
    - **página web** de Tisma Cloud: buscar, ver la ficha con el visor 3D y el mapa de Ingeniería, bajar, imprimir;
    - **programa:** Más › Tisma Cloud › Biblioteca, con la misma ficha; «Abrir en Tisma» abre el proyecto en la placa,
      con sus perfiles y su análisis;
    - **app** (`mobile_app.md`): la pestaña Biblioteca; «Imprimir» es el uso principal.
21. **Buscar:** por texto, etiqueta, impresora con perfil probado, material y «con análisis». Orden: populares,
    nuevos, más impresos.
22. **Después de imprimir:** la persona puede marcar «La imprimí» con una foto. Eso suma al contador «más impresos» y,
    si usó un perfil probado, a la confianza de ese perfil.

## El contrato con el motor

23. **Lo que el motor ya tiene que poder hacer** (fichas existentes):
    - laminar un 3MF completo sin interfaz (`cloud.md`, regla 18);
    - guardar y abrir el análisis dentro del proyecto (`formats.md`, `structural_analysis.md`);
    - decir qué ajustes cambian la resistencia (regla 3): el motor da la lista y el servidor compara.
24. **Lo nuevo del motor:**
    - el resumen de un análisis en JSON (veredicto, carga, factor de seguridad, deformación, material y los ajustes de
      la regla 2), para mostrarlo sin el motor;
    - el mapa de colores exportado como malla con colores (por ejemplo, 3MF con colores o glTF), para el visor de la
      página y de la app.

## Lo que hace el servidor

Es el programa del dueño (`cloud.md`). Se anota para que el contrato quede claro:
- guarda los modelos, versiones, fotos, perfiles probados, laminados y análisis;
- revisa las subidas;
- lamina al publicar y cuando se pide;
- reutiliza laminados iguales y prepara de antemano lo que más se pide (reglas 14 y 15);
- guarda los G-code comprimidos, una copia por contenido, con su receta y un tope de espacio (regla 16);
- reserva el 10 % del cómputo para el laminado gratis, con su cola y sus topes (regla 13);
- guarda los perfiles propios que suben las personas (regla 12);
- manda el G-code a los puentes como hoy.

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| BI1 | Publicar un proyecto con dos perfiles probados y un análisis | La ficha con dos laminados hechos y el resumen de Ingeniería |
| BI2 | Ver la ficha sin cuenta | Datos, fotos, resumen de Ingeniería y visor; «Imprimir» pide entrar |
| BI3 | Imprimir con una impresora que tiene laminado hecho | Se manda tal cual; no gasta cómputo |
| BI4 | Imprimir en otra impresora con un perfil del paquete | Un laminado gratis, en la cola gratis |
| BI5 | Otra persona pide exactamente lo mismo que BI4 | Se manda el laminado ya hecho; no gasta cómputo |
| BI6 | Cambiar el relleno de 20 % a 15 % | Aviso de que el análisis no vale; ofrece calcular de nuevo |
| BI7 | Cambiar el color del filamento | El análisis sigue válido |
| BI8 | La cola gratis usa su 10 % y llega otro pedido gratis | Espera en la cola, con su lugar y el tiempo estimado; no se rechaza |
| BI9 | Cuenta paga: 50 laminados en una hora | Todos se hacen, con el tope de trabajos a la vez y antes que la cola gratis |
| BI13 | Laminar con un perfil propio | Pide guardarlo en Tisma Cloud; el laminado usa las horas del plan |
| BI14 | Trabajos pagos llenan los nodos | El 10 % sigue atendiendo la cola gratis |
| BI15 | Una cuenta gratis manda dos laminados seguidos | El segundo espera a que termine el primero |
| BI16 | Una cuenta gratis pide un tercer laminado que gasta cómputo en el mismo día | Ofrece lo que no gasta (laminado del autor, preparado), un plan o «Abrir en Tisma» |
| BI17 | Publicar con el G-code del autor | La ficha muestra «Laminado del autor» con su impresora y su perfil; mandarlo no gasta cómputo |
| BI18 | Laminado del autor con una orden que actualiza el firmware | Rechazado al publicar |
| BI19 | El décimo pedido igual en 30 días | Queda guardado; el undécimo se manda sin cómputo |
| BI20 | Nodos sin trabajos | El servidor prepara laminados de los modelos más bajados; se detiene apenas llega un trabajo |
| BI10 | «Abrir en Tisma» desde la página | El programa abre el proyecto con sus perfiles y su análisis |
| BI11 | Subir un 3MF con un programa adentro | Rechazado, como cualquier subida peligrosa |
| BI12 | Nueva versión del motor y un laminado hecho viejo | Se lamina de nuevo al pedirlo, con un perfil del servidor: entra en el laminado gratis |
| BI21 | Dos pedidos distintos que dan el mismo G-code | Se guarda una sola copia |
| BI22 | Se llena el tope de espacio | Se borra lo que hace más que no se pide; el laminado del autor y lo preparado de los 1000 quedan |
| BI23 | Pedir un G-code borrado | Se vuelve a hacer con su receta, igual byte a byte |
