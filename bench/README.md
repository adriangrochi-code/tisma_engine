# Banco de pruebas

Modelos del plan: Benchy, Benchy ×2 con giroide, conejo ×1,5 y tornillo ×4.

**Todavía no están en el repositorio.** Falta que el dueño confirme qué archivos usar y que su licencia permita
guardarlos acá (3DBenchy y el conejo de Stanford tienen licencias propias). Mientras tanto, la lectura se mide con una
esfera generada:

```sh
python3 bench/gen_sphere.py esfera.stl --segments 708          # 1 001 112 triángulos, binario, 50 MB
python3 bench/gen_sphere.py esfera_ascii.stl --segments 708 --ascii   # los mismos, ASCII, 220 MB
build/tisma-engine info esfera.stl     # imprime read_ms
```

Los resultados se anotan en `PROGRESS.md`.
