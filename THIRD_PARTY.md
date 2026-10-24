# Componentes de terceros

Antes de usar una biblioteca se anota acá con:
- su versión exacta;
- su licencia, leída en su archivo de licencia;
- su URL.

Solo licencias permisivas (MIT, BSD, BSL-1.0, Apache-2.0, zlib, MPL-2.0); ver `CLEANROOM.md`.

| Biblioteca | Versión | Licencia | URL | Uso |
|---|---|---|---|---|
| Catch2 | 3.7.1 (commit `fa43b77429ba76c462b1898d6cd2f2d7a9416b14`) | BSL-1.0, leída en `LICENSE.txt` del tag `v3.7.1` | https://github.com/catchorg/Catch2 | Solo pruebas; CMake la descarga con FetchContent. No se distribuye con el programa. |
