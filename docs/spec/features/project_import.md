# Abrir proyectos de otros programas

**Qué hace:** abre un proyecto 3MF hecho en otro programa de laminado y lo deja en Tisma como si se hubiera hecho
acá: las piezas, cómo están armadas, sus ajustes, lo pintado y las placas. Así cualquier pieza preparada en otro
programa se puede seguir trabajando e imprimir sin rehacerla.

**Pedido del dueño** (06-11-2026): «un conversor de 3MF de diferentes slicer al Tisma así podemos integrar cualquier
pieza hecha en el slicer».

**Fase:** antes de la 1.0 (amplía `formats.md`, reglas 10, 11 y 18).

**Fuente:** la especificación pública del 3MF (3MF Consortium: núcleo, «Production Extension», «Materials and
Properties Extension»), la documentación pública para usuarios de cada programa y archivos de muestra guardados con
cada programa usado como caja negra. Ningún código de otro programa.

## Qué programas

1. **Se reconocen** por los archivos que trae el paquete y por el campo `Application` de los metadatos del modelo:

| Programa | Cómo se reconoce |
|---|---|
| Tisma Slicer y PrusaSlicer | `Metadata/Slic3r_PE.config` y `Metadata/Slic3r_PE_model.config` |
| OrcaSlicer, Bambu Studio, Creality Print, Elegoo Slicer, Anycubic Slicer Next | `Metadata/project_settings.config` (JSON), `Metadata/model_settings.config`, mallas en `3D/Objects/*.model` |
| Cura | `Cura/` dentro del paquete (ajustes por perfil) |
| Otro o desconocido | Solo el 3MF del estándar |

2. **Lo que no se reconoce** se abre como 3MF del estándar (mallas, nombres, ubicación, colores del estándar) con el
   aviso «Proyecto de un programa desconocido: se abrieron las piezas, sin sus ajustes».

## Qué se trae

3. **Mallas y ubicación:** cada objeto, sus partes, su transformación y su lugar en la cama, del núcleo del 3MF y de la
   extensión de producción (mallas en archivos aparte). Igual que `formats.md`, reglas 7, 8 y 18.
4. **Tipos de parte:** pieza, modificador, volumen negativo, bloqueador y forzador de soporte, con los tipos de
   `model_tools.md`.
5. **Placas:** cada placa del proyecto pasa a una cama (`model_tools.md`, regla 26), con sus objetos y su orden.
6. **Ajustes del proyecto:** los de impresión, filamento e impresora se traducen a las claves del motor con la tabla
   de traducción del paquete de perfiles (regla 11). Cada ajuste queda en uno de tres grupos, que el diálogo de la
   regla 9 muestra:
   - **traducido:** tiene equivalente y se usa;
   - **parecido:** el equivalente no es igual (otra unidad, otro rango, otra forma de cálculo) y se usa con la
     conversión de la tabla;
   - **sin equivalente:** no se usa; se nombra con su valor.
7. **Ajustes por objeto, por parte y por rango de alturas:** igual que los del proyecto, sobre cada objeto, parte o
   rango (`model_tools.md`, reglas 5 y 6).
8. **Impresora y perfiles:**
   - si el proyecto nombra una impresora, un filamento o un perfil de impresión que está en el paquete de perfiles
     (mismo nombre o el nombre que el paquete guarda como de origen), se elige ese, y encima van los ajustes
     traducidos del proyecto como cambios a mano;
   - si no está, se usa la impresora elegida en Tisma, y los ajustes traducidos van como cambios a mano;
   - el G-code de inicio y de fin del proyecto se guarda en el bloque de la máquina con su dialecto
     (`gcode_macros.md`, regla 10 bis), sin convertir.
9. **Diálogo al abrir:** «Proyecto de <programa>: N piezas, M placas. Ajustes: A traducidos, B parecidos, C sin
   equivalente». Con «Ver detalle» (la lista de los tres grupos, buscable) y «Usar la impresora del proyecto» o
   «Usar la mía».
10. **Pintura:**
    - soporte forzado o bloqueado, costura, piel difusa y colores (multimaterial) pintados sobre la malla se traen
      como pintura de Tisma, triángulo por triángulo, con la subdivisión que guarde el archivo;
    - la forma en que cada programa guarda la pintura en el 3MF se mide con archivos de muestra (prueba PI6 a PI8):
      hasta medirla, la pintura no se trae y el diálogo lo dice («La pintura de este proyecto no se pudo traer»).

## La tabla de traducción

11. **Viene en el paquete de perfiles,** como datos, un archivo por dialecto (`prusaslicer`, `orcaslicer`,
    `crealityprint`, `elegooslicer`, `cura`): por cada clave de origen, la clave del motor, la conversión
    (igual, factor, unidad, lista de valores equivalentes, porcentaje de otra clave) y el grupo de la regla 6. Es la
    misma traducción con que el paquete convierte los perfiles, así que un proyecto y un perfil del mismo programa
    quedan iguales. El motor no lleva tabla propia: con una versión nueva del paquete, traduce más.
12. **Sin paquete instalado:** se abren las piezas, las placas y la pintura; los ajustes no, con el aviso.

## Lo que no hace

13. No escribe proyectos en el formato de otro programa (para eso está el 3MF del estándar de `formats.md`).
14. No abre G-code de otro programa como proyecto (eso es la vista de G-code).
15. No ejecuta nada que traiga el proyecto (scripts de posprocesado): se nombran en «sin equivalente».

## Pruebas

Los archivos de muestra se guardan con cada programa, de una pieza propia (sin modelos de terceros), y se suman a las
pruebas con su programa y versión.

| # | Caso | Resultado esperado |
|---|---|---|
| PI1 | 3MF del estándar con un cubo, sin datos de programa | Se abre el cubo; aviso de programa desconocido |
| PI2 | Proyecto de Tisma Slicer con dos objetos, uno con un modificador | Dos objetos; el modificador como modificador; ajustes traducidos con la tabla `prusaslicer` |
| PI3 | Proyecto de OrcaSlicer con dos placas | Dos camas con sus objetos; impresora del paquete si el nombre está |
| PI4 | Proyecto de Bambu Studio de una A1 con capa de 0,16 mm y relleno 25 % | Impresora A1 del paquete, capa 0,16, relleno 25 %, G-code de la máquina en su dialecto |
| PI5 | Proyecto de Cura con un perfil cambiado | Ajustes traducidos con la tabla `cura`; los sin equivalente, nombrados |
| PI6 | Proyecto de PrusaSlicer con soporte pintado en una cara | La misma cara con soporte forzado en Tisma |
| PI7 | Proyecto de OrcaSlicer con costura pintada | La costura pintada en el mismo lugar |
| PI8 | Proyecto de Bambu Studio con dos colores pintados | Las dos zonas con sus extrusores |
| PI9 | Proyecto con un script de posprocesado | No se ejecuta; aparece en «sin equivalente» |
| PI10 | Un ajuste parecido (velocidad en mm/s contra porcentaje) | Se convierte con la tabla y queda en «parecidos» |
| PI11 | Sin paquete de perfiles | Piezas y placas sí, ajustes no, con aviso |
| PI12 | Abrir, laminar y comparar con el G-code del programa de origen (misma pieza y ajustes) | Mismo número de capas y tiempo dentro del 15 % |
