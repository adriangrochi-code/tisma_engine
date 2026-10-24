# Procedencia

Cada módulo del motor anota de dónde sale: el artículo y la sección, la especificación, o «diseño propio».

| Módulo | Fuente |
|---|---|
| `src/io/stl.*` — lectura de STL | Especificación pública del formato: 3D Systems, *StereoLithography Interface Specification* (1989), y la variante binaria de uso común (cabecera de 80 bytes, cantidad uint32 little-endian, 50 bytes por faceta). La detección binario/ASCII (tamaño exacto, palabra `solid`, sin bytes NUL) y la tolerancia a mayúsculas, CRLF, varios `solid` y bytes al final son diseño propio. |
| `src/mesh/mesh.*` — malla de triángulos, caja y volumen | Diseño propio. El volumen con signo es el teorema de la divergencia (suma de tetraedros desde el origen), matemática de libro de texto. |
| `src/cli/*`, `apps/main.cpp` — línea de comandos | Diseño propio, a partir de la interfaz definida en `PLAN.md`. |
| `bench/gen_sphere.py` — esfera de prueba | Diseño propio (esfera UV). |
| `.github/workflows/ci.yml`, `CMakeLists.txt` | Diseño propio; documentación pública de GitHub Actions y CMake. |
