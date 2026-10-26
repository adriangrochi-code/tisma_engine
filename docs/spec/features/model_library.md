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
y los datos de FEA o CFD de la pieza; laminar gratis unas veces por mes una pieza ya hecha y sin límite en los planes
pagos; mandar a imprimir con la comodidad de una app de impresora en el teléfono.

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
| Laminados hechos | G-code por perfil probado, hecho por el servidor (regla 7) | No: se hacen al publicar |
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
6. **Revisión:** el servidor revisa el 3MF como cualquier subida (`cloud.md`; archivos peligrosos) y que el análisis
   corresponda a la pieza.
7. **Laminados hechos:** al publicar, el servidor lamina una vez cada perfil probado con el motor (`cloud.md`,
   regla 18) y guarda el G-code con su resumen (tiempo, filamento y capas). Si el motor cambia de versión, se vuelven a
   laminar cuando alguien los pide (regla 11), y eso no cuenta como laminado remoto de quien lo
   pidió.
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

## Qué cuesta un laminado

11. **Tres casos:**

| Caso | Qué hace el servidor | Cuenta como laminado remoto |
|---|---|---|
| Hay un laminado hecho para esa impresora, boquilla y material, sin cambios | Lo manda tal cual | No |
| Otra impresora, otro material o ajustes cambiados | Lamina con el motor (`cloud.md`, regla 18) | Sí |
| El mismo pedido que otra persona ya laminó (misma versión del modelo, mismos perfiles y ajustes, misma versión del motor) | Manda el que ya hizo | No |

    El tercer caso hace que los modelos populares cuesten poco: el servidor guarda cada laminado de la biblioteca
    mientras se siga pidiendo (por ejemplo, 90 días sin pedidos lo borran).
12. **Planes** (propuesta para que decida el dueño, con los números de abajo):

| Plan | Laminados remotos de la biblioteca | Laminados hechos (sin cómputo) | Análisis nuevos |
|---|---|---|---|
| Gratis, con cuenta | 20 por mes | Sin límite, con un tope de pedidos por minuto | No; los análisis publicados se ven igual |
| Pagos | Sin límite, con un tope de pedidos por minuto y de trabajos a la vez | Sin límite | Con las horas del plan, como hoy |

    - **Por qué 20:** el servidor mide que un Benchy de 225 000 triángulos tarda de 6 a 13 s en un nodo de 4 núcleos
      (medido en el servidor de Tisma Cloud, en su documentación, «Capacidad»). 20 laminados así son de 2 a 4,5 minutos
      de nodo por persona y por mes, menos que la hora del plan de prueba que ya tiene el servidor.
    - **Sin límite tiene un tope:** pocos trabajos a la vez y un tope de pedidos por minuto por cuenta, para que una
      sola cuenta no llene la cola. Es el mismo control que ya tiene el servidor por plan.
    - El número final se fija con el uso medido.
13. **El contador a la vista:** la ficha y la pantalla «Imprimir» dicen cuántos laminados gratis quedan en el mes antes
    de mandar uno. Sin cupo, se ofrece un plan o «Abrir en Tisma» para laminar en la computadora, que siempre es gratis.

## La página, el programa y la app

14. **La misma biblioteca en los tres lugares:**
    - **página web** de Tisma Cloud: buscar, ver la ficha con el visor 3D y el mapa de Ingeniería, bajar, imprimir;
    - **programa:** Más › Tisma Cloud › Biblioteca, con la misma ficha; «Abrir en Tisma» abre el proyecto en la placa,
      con sus perfiles y su análisis;
    - **app** (`mobile_app.md`): la pestaña Biblioteca; «Imprimir» es el uso principal.
15. **Buscar:** por texto, etiqueta, impresora con perfil probado, material y «con análisis». Orden: populares,
    nuevos, más impresos.
16. **Después de imprimir:** la persona puede marcar «La imprimí» con una foto. Eso suma al contador «más impresos» y,
    si usó un perfil probado, a la confianza de ese perfil.

## El contrato con el motor

17. **Lo que el motor ya tiene que poder hacer** (fichas existentes):
    - laminar un 3MF completo sin interfaz (`cloud.md`, regla 18);
    - guardar y abrir el análisis dentro del proyecto (`formats.md`, `structural_analysis.md`);
    - decir qué ajustes cambian la resistencia (regla 3): el motor da la lista y el servidor compara.
18. **Lo nuevo del motor:**
    - el resumen de un análisis en JSON (veredicto, carga, factor de seguridad, deformación, material y los ajustes de
      la regla 2), para mostrarlo sin el motor;
    - el mapa de colores exportado como malla con colores (por ejemplo, 3MF con colores o glTF), para el visor de la
      página y de la app.

## Lo que hace el servidor

Es el programa del dueño (`cloud.md`). Se anota para que el contrato quede claro:
- guarda los modelos, versiones, fotos, perfiles probados, laminados y análisis;
- revisa las subidas;
- lamina al publicar y cuando se pide;
- reutiliza laminados iguales (regla 11);
- cuenta los laminados remotos por mes y aplica los cupos del plan (regla 12);
- manda el G-code a los puentes como hoy.

## Pruebas

| # | Escenario | Resultado esperado |
|---|---|---|
| BI1 | Publicar un proyecto con dos perfiles probados y un análisis | La ficha con dos laminados hechos y el resumen de Ingeniería |
| BI2 | Ver la ficha sin cuenta | Datos, fotos, resumen de Ingeniería y visor; «Imprimir» pide entrar |
| BI3 | Imprimir con una impresora que tiene laminado hecho | Se manda tal cual; el contador de laminados gratis no baja |
| BI4 | Imprimir en otra impresora | Un trabajo de laminado; el contador baja en 1 |
| BI5 | Otra persona pide exactamente lo mismo que BI4 | Se manda el laminado ya hecho; su contador no baja |
| BI6 | Cambiar el relleno de 20 % a 15 % | Aviso de que el análisis no vale; ofrece calcular de nuevo |
| BI7 | Cambiar el color del filamento | El análisis sigue válido |
| BI8 | Cuenta gratis con los 20 laminados usados | Ofrece un plan o «Abrir en Tisma»; no lamina |
| BI9 | Cuenta paga: 50 laminados en una hora | Todos se hacen, con el tope de trabajos a la vez |
| BI10 | «Abrir en Tisma» desde la página | El programa abre el proyecto con sus perfiles y su análisis |
| BI11 | Subir un 3MF con un programa adentro | Rechazado, como cualquier subida peligrosa |
| BI12 | Nueva versión del motor y un laminado hecho viejo | Se lamina de nuevo al pedirlo; no cuenta como laminado remoto de la persona |
