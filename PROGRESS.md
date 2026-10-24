# Avance

Cada bloque de trabajo agrega una entrada con:
- la fecha;
- qué hizo;
- qué midió;
- qué sigue.

## 2026-10-24 — inicio

Se creó el repositorio con la licencia, las reglas de sala limpia y el plan. Todavía no hay código.

**Sigue:** F0 (esqueleto, CI, lectura de STL, banco de pruebas).

## 2026-10-24 — F0: esqueleto, línea de comandos y lectura de STL

**Hecho:**
- Proyecto CMake en C++20: biblioteca `tisma_core` y programa `tisma-engine`.
- Línea de comandos: `tisma-engine slice modelo.stl --config ajustes.json -o salida.gcode` comprueba los
  argumentos, que exista el JSON y lee el STL; después responde «slice: not implemented yet» con código de salida 3.
  `tisma-engine info modelo.stl` imprime formato, triángulos, tamaño, volumen y tiempo de lectura.
  Códigos de salida: 0 bien, 1 línea de comandos mal, 2 archivo de entrada mal, 3 sin implementar.
- Lector de STL binario y ASCII (`src/io/stl.*`), desde la especificación pública. Detecta el formato por el
  contenido (un binario cuya cabecera empieza con `solid` sigue siendo binario). Acepta mayúsculas, CRLF, varios
  `solid`, `endsolid` ausente y bytes sobrantes al final del binario. Rechaza archivos truncados, coordenadas NaN o
  infinitas y facetas con más de 3 vértices; los errores ASCII dicen la línea.
- Malla de triángulos contigua (`src/mesh/mesh.*`): caja envolvente y volumen con signo.
- 24 pruebas Catch2 y 3 pruebas del programa con CTest.
- CI de GitHub Actions: Linux en cada envío (gcc, clang y gcc con ASan+UBSan); Windows 2022 y macOS 14 solo a mano
  (`workflow_dispatch`) y los lunes.
- Catch2 3.7.1 anotada en `THIRD_PARTY.md` (licencia BSL-1.0 leída en el tag).

**Probado (local, Linux, 4 núcleos):** 27/27 pruebas pasan con gcc 13.3 (Release), clang 18 (Release) y gcc con
ASan+UBSan (Debug). Compila sin advertencias con `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`.

**No probado:** Windows ni macOS (la CI de esos sistemas todavía no se lanzó). Los modelos del banco no están en el
repositorio.

**Medido** (`tisma-engine info`, esfera generada de 1 001 112 triángulos, archivo en caché, 5 corridas, Release):

| Archivo | Tamaño | Lectura |
|---|---|---|
| Binario | 50 MB | 58–67 ms |
| ASCII | 220 MB | 713–845 ms |

**Sigue:**
- Lanzar la CI a mano para ver Windows y macOS.
- Banco de pruebas con los modelos reales (pedir al dueño los archivos y confirmar su licencia).
- Después, F1: corte de la malla en capas. Necesitará una malla indexada (vértices compartidos) para unir
  segmentos en contornos, y Clipper2 (anotarla en `THIRD_PARTY.md` antes de usarla).
