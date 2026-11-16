# Puerta de lanzamiento: funciones críticas

**Decisión del dueño (16-11-2026):** «si hay un error tan fundamental como que los soportes no funcionen, no se pasa un
lanzamiento con ese estado». Un error raro puede quedar anotado para después; una función crítica rota frena el
lanzamiento.

## Regla

1. Cada función de la tabla tiene una prueba de punta a punta **desde la interfaz** (no solo desde la línea de
   comandos): se arma el caso como lo haría una persona (abrir, elegir, pintar, cambiar ajustes), se lamina o se
   exporta, y se comprueba el **resultado** (el G-code, el archivo, la pantalla), no solo que no falle.
2. Las pruebas corren en la CI de Linux y de Windows en cada envío a `dev`, como un trabajo aparte llamado
   «Puerta de lanzamiento». Si una falla, no se publica ninguna versión (nightly, alpha ni release): el paso de
   publicar depende de ese trabajo.
3. Una prueba de la puerta no se salta, no se marca como «conocida» ni se relaja para que pase. Si una función crítica
   cambia a propósito, se cambia esta ficha primero.
4. El equipo de especificación corre además la puerta desde afuera (caja negra) antes de cada versión para el
   probador, y anota el resultado.

## Funciones críticas

| # | Función | Caso | Se comprueba |
|---|---|---|---|
| G1 | Abrir | STL, OBJ, 3MF propio y 3MF de otro programa | La pieza está en la placa con su tamaño |
| G2 | Laminar | Cubo de 20 mm y Benchy con el perfil por defecto | G-code válido; capas, filamento y tiempo > 0; dimensiones dentro de 0,1 mm |
| G3 | Soportes automáticos | Activar soportes en la interfaz, cada estilo (grilla, árbol, ajustado), con la T, una rampa a 60° y el Benchy | Hay `;TYPE:Support` bajo cada voladizo de más del ángulo; menos de 1 % de soporte flotando |
| G4 | Soportes pintados | Forzar con los automáticos apagados, bloquear con los prendidos, borrar | Soporte solo donde se forzó; nada donde se bloqueó; nada después de borrar |
| G5 | Soportes en la vista previa | Después de G3 | La vista previa muestra el soporte y la leyenda lo cuenta |
| G6 | Soporte que no se apaga solo | Abrir un 3MF, cambiar de perfil e impresora, deshacer | El estado de los soportes no cambia sin aviso |
| G7 | Adhesión | Falda, borde y balsa | Aparecen en la primera capa con su tamaño |
| G8 | Relleno y paredes | Densidad 0, 15 y 100 %; 1 a 4 paredes | El G-code tiene lo pedido |
| G9 | Guardar y abrir proyecto | Pieza con pintura, modificador y ajustes por objeto | Al abrir de nuevo, el G-code sale igual |
| G10 | Exportar y mandar | Exportar G-code; mandar a una impresora simulada de cada tipo | El archivo llega entero y la impresión empieza |
| G11 | Nube | Laminar en la nube contra un maestro de prueba | Se baja el G-code y coincide con el local |
| G12 | Calibración | Cada prueba de Calibración | Se arma, se lamina y el cuadro cierra |
| G13 | Ingeniería | Estructural y aerodinámica sobre el cubo y el Benchy | Hay resultado; ninguna cara sin valor |
| G14 | Perfiles | Elegir 5 impresoras del paquete (Prusa, Bambu, Creality, Klipper, Marlin) | Laminan con su G-code de inicio |
| G15 | Cerrar | Cerrar con un laminado, la nube y un análisis en marcha | El proceso sale en menos de 3 s |
| G16 | Apps del teléfono | Asistente, abrir, laminar, mandar | Igual que G1, G2 y G10 en el emulador |

## Definiciones para G3

- **Voladizo:** caras conectadas, más inclinadas que el ángulo de soporte, a más de 1 mm de la cama, que en alguna
  capa sobresalen de la capa de abajo más de medio cordón (el umbral de `supports_skirt.md`, regla 1). Un labio que
  sobresale menos se imprime sin soporte.
  Los puentes no cuentan: con `support.under_bridges` apagado no llevan soporte (`supports_skirt.md`, regla 1). El
  techo de la cabina del Benchy es un puente.
- **Tiene soporte:** hay extrusión de soporte a menos de 2,5 mm de costado, en los 3 mm de abajo del voladizo.
- **Flotando:** una línea de soporte sin extrusión (pieza o soporte) a menos de 1,5 mm de costado en la capa de abajo.
  Con 1 mm, la capa de interfaz que cruza entre las líneas de la base (separadas unos 2,5 mm) contaría como flotante,
  y no lo es: se apoya como un puente corto (medido el 16-11-2026 con la T y las rampas).
  El porcentaje es la longitud flotante sobre la longitud total de soporte.
