# Procedencia

Cada módulo del motor anota de dónde sale: el artículo y la sección, la especificación, o «diseño propio».

| Módulo | Fuente |
|---|---|
| `src/io/stl.*` — lectura de STL | Especificación pública del formato: 3D Systems, *StereoLithography Interface Specification* (1989), y la variante binaria de uso común (cabecera de 80 bytes, cantidad uint32 little-endian, 50 bytes por faceta). La detección binario/ASCII (tamaño exacto, palabra `solid`, sin bytes NUL) y la tolerancia a mayúsculas, CRLF, varios `solid` y bytes al final son diseño propio. |
| `src/mesh/mesh.*` — malla de triángulos, caja y volumen | Diseño propio. El volumen con signo es el teorema de la divergencia (suma de tetraedros desde el origen), matemática de libro de texto. |
| `src/mesh/indexed_mesh.*` — soldadura exacta de vértices | Diseño propio: tabla hash de direccionamiento abierto sobre el patrón de bits de cada esquina (técnica de libro de texto); vértices numerados por orden de primera aparición. |
| `src/slice/level_set.*` — contornos de nivel de un campo en la malla | Interpolación lineal en las aristas cruzadas («marching triangles»), técnica de libro de texto. La regla `valor >= iso` para los vértices sobre el nivel, el punto calculado en el orden de los índices de la arista (para que los dos triángulos den el mismo punto) y la orientación por el orden de recorrido son diseño propio. |
| `src/slice/contours.*` — unión de segmentos en contornos | Diseño propio: los tramos se encadenan por la identidad de la arista (índices de vértices), sin comparar coordenadas; primero las cadenas abiertas desde su cabeza, después los lazos cerrados. |
| `src/slice/layers.*` — plan de capas y corte por niveles | Diseño propio: alturas calculadas desde el índice de la capa; cada triángulo se reparte en los niveles que cruza su rango de valores (filas comprimidas: contar y después llenar), y cada nivel corta solo sus triángulos. |
| `src/cli/*`, `apps/main.cpp` — línea de comandos | Diseño propio, a partir de la interfaz definida en `PLAN.md`. |
| `bench/gen_sphere.py` — esfera de prueba | Diseño propio (esfera UV). |
| `tools/check_licenses.py` — control de licencias | Diseño propio. |
| `.github/workflows/ci.yml`, `CMakeLists.txt` | Diseño propio; documentación pública de GitHub Actions y CMake. |
