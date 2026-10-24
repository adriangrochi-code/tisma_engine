# Avance

Cada bloque de trabajo agrega una entrada con:
- la fecha;
- qué hizo;
- qué midió;
- qué sigue.

## 2026-10-24 — inicio

Se creó el repositorio con la licencia, las reglas de sala limpia y el plan. Todavía no hay código.

**Sigue:** F0 (esqueleto, CI, lectura de STL, banco de pruebas).

## 2026-10-24 — información para el motor

El dueño agregó a `main`:
- `docs/spec/`: especificaciones funcionales (G-code, ajustes, comportamiento, números de referencia) y fuentes públicas;
- `imported/tisma/` y `docs/tisma/`: código y documentos propios traídos de Tisma Slicer (análisis estructural y
  aerodinámico, capas no planares, Z anti-aliasing, relleno entrelazado, impresoras de cinta y más), revisados para que
  no tengan código de PrusaSlicer. Ver `imported/README.md`.

**Sigue:** el bloque siguiente une `main` en `dev` y continúa F0, usando las especificaciones.
