# Printed-part material properties for FEA: data sheets, Z-strength and temperature vs layer adhesion

Research date: 2026-11-03. Clean-room note: built only from manufacturer technical data sheets (TDS), standards
names, papers and community blog posts/videos' written results. No slicer source code was read or searched.
Values are summarised and cited, not copied as text.

## 0. Legend and how to read the numbers

Every value carries a tag:

| Tag | Meaning |
|---|---|
| **C** | Manufacturer-claimed: read directly from the manufacturer's TDS PDF/wiki page (opened and parsed by me). |
| **C?** | Manufacturer-claimed, but only seen via a third-party copy, search-engine summary or reseller page; not verified against the original PDF. |
| **M** | Measured by an independent party (community test, university, paper) on printed specimens. |
| **E** | Estimated by me (ratio rules, resin data, or interpolation). Use only as a fallback. |

Notation: E = Young's (tensile) modulus, S = tensile strength (at yield or break, as the sheet gives it), all in MPa.
"XY" = strength in the layer plane, "Z" = loading across layers (layer adhesion). Ratios Rs = S_z/S_xy and
Re = E_z/E_xy are given where both exist.

### 0.1 Orientation conventions differ between manufacturers (important for comparing)

| Manufacturer | Test standard | Orientations reported | Notes |
|---|---|---|---|
| Bambu Lab | ISO 527 / GB/T 1040 (tensile), ISO 178 / GB/T 9341 (flex), ISO 179 / GB/T 1043 (impact), ISO 75, ISO 306, ISO 1183 | X-Y (flat) and Z (upright) | 100 % infill. **All specimens annealed and dried before testing** (PLA 55 °C 8 h; PETG 65 °C 8 h; ABS/ASA/PC 80 °C 12 h; CF nylons 80 °C 12 h; TPU 70 °C 12 h). Print settings (nozzle, bed, speed) listed per sheet. |
| Polymaker | ISO 527 / GB/T 1040, ISO 178, ISO 179 | X-Y and Z | Shell 2, top/bottom 3, infill 100 %. ABS/ASA/PC printed in a **90 °C environment**, PLA/PETG at ambient. Fiberon/PolyMide CF grades annealed (100 °C 16 h or 80 °C 6 h) and some also reported "wet" (conditioned). |
| Prusa Polymers (Prusament) | ISO 527-1, ISO 178, ISO 179-1; "Interlayer adhesion" by an in-house Prusa Polymers method | "Horizontal" (flat) and "Vertical xz" (**on-edge, still loaded along the filaments, NOT across layers**) | The real Z value is the separate line **"Interlayer Adhesion [MPa]"**. Newer sheets (PLA High Speed, PA11) define a "tensile strength anisotropy coefficient = Z/XY tensile strength" equal to interlayer/XY, so the interlayer value behaves like a Z tensile strength. Specimens: MK3/MK3S, 0.20 mm layers, 2 perimeters, 0 top/bottom, 100 % rectilinear, default fan unless stated. Prusament Z values are systematically lower than other brands' Z values (stricter test or default cooling). |
| BASF Forward AM (Ultrafuse) | ISO 527 (tensile), ISO 178, ISO 180 Izod, ISO 75 | XY, XZ, ZX per ISO/ASTM 52921 | ZX = upright (Z). Catalogue gives XY and ZX for tensile, often XZ for modulus. |
| Kimya (Airtech) | ISO 527-2/1A/50 or 5A/50, ISO 178, ISO 179-1/1eA | Mostly XY only; PETG-S gives XY and ZX | |
| 3DXTech | ISO 527, ISO 178, ISO 75 | "XY Flat" only | 0.4 mm nozzle, 0.25 mm layer, 100 % ±45° infill. |
| Victrex (PEEK AM filaments) | ISO 527 | XY, YZ, ZX | Annealed; chamber noted. |

Comparability rules:
1. Bambu values are after annealing; this raises crystallinity/strength for PLA and nylons. Expect lower as-printed values.
2. Polymaker ABS/ASA/PC values come from a 90 °C chamber; an open printer gives lower Z (see CNC Kitchen chamber test, section 3).
3. Prusament "Vertical xz" must NOT be used as Z. Use "Interlayer Adhesion".
4. Nylon values depend strongly on moisture: "dry" vs "conditioned/wet" can halve strength and cut modulus by 3x.

---

## 1. Consolidated table per material family (for FEA defaults)

Columns: min / typical / max across the sources in section 2 (brand count in brackets). "Best-adhesion nozzle T" is
where layer adhesion peaked in tests (M) or my estimate (E). Tg / service temperature is Tg (DSC) and/or HDT at
0.45 MPa. Density in g/cm3 of filament/printed part as given.

| Family | E_xy (MPa) | E_z (MPa) | S_xy (MPa) | S_z (MPa) | Density | Tg / HDT0.45 (°C) | Nozzle range (°C) | Best-adhesion nozzle T (°C) |
|---|---|---|---|---|---|---|---|---|
| PLA | 1960 / 2500 / 3430 (10) C | 1670 / 2130 / 3065 (6) C | 32 / 45 / 70 (12) C | 6* / 33 / 41 (11) C; 40 M | 1.17–1.31 | Tg 57–63; HDT 55–65 | 190–230 (spec. prints 215–230) | ~230 (M, CNC Kitchen; flat 210–250) |
| PETG / PET | 1460 / 1800 / 2160 (8) C | 1120 / 1600 / 2060 (6) C | 33 / 47 / 52 (10) C | 15 / 30 / 43 (9) C; 32 M | 1.24–1.28 | Tg 66–81; HDT 63–78 | 230–270 | ~245 (M, CNC Kitchen; drops at 260) |
| ABS | 1380 / 2000 / 2250 (4)** C | 1100 / 1850 / 2960 (4) C | 29.5 / 36 / 41 (5) C; 39 M | 15 / 22 / 32 (5) C; 21–29 M | 1.04–1.12 | Tg ~101–108; HDT 87–100 | 240–270 | ~230–250 + warm chamber (M: 231 optimum Foppiano; chamber 65 °C gives +38 %) |
| ASA | 1600 / 2100 / 2450 (4) C | 1400 / 1965 / 2120 (3) C | 34.6 / 43 / 52 (4) C | 11 / 32 / 35 (4) C | 1.05–1.13 | Tg ~98; HDT 93–103 | 230–280 | 260–270 + chamber (E, by analogy with ABS) |
| HIPS | 1588 (1) C | 1603 (1) C | 18.4 (1) C | 13.7 (1) C | ~1.04 E | HDT 91 | 240–260 | ~250 (E) |
| PC (incl. PC blend) | 1670 / 2200 / 3100 (6) C | 1450 / 2000 / 2370 (4) C | 53 / 62 / 72 (7) C | 18 / 41 / 56 (7) C | 1.18–1.22 | Tg 113–145; HDT 112–138 | 250–290 | 270–290, fan off, chamber (E; Prusa: cooling cuts interlayer adhesion) |
| PC/ABS | 1835 / 2190 / 2545 (2) C | 1677 / 1930 / 2188 (2) C | 40 / 45 / 50 (2) C | 17 / 20 / 23 (2) C | 1.1 | Tg 109; HDT 86–112 | 250–270 | ~260 + chamber (E) |
| PA (unfilled, dry) | 395 / 1200 / 2703 (3) C | 334 / 1200 / 2209 (2) C | 33 / 38 / 78 (3) C | 17.6 / 32 / 46 (3) C | 1.02–1.12 | Tg 50–66; HDT 59–135 | 250–290 | 260–280 (E) |
| PA-CF / PA-GF (dry/annealed) | 2470 / 4400 / 8640 (9) C | 1150 / 2200 / 4350 (8) C | 42 / 90 / 109 (9) C | 12 / 49 / 68 (9) C | 1.03–1.17 | Tg 55–74; HDT0.45 131–215 | 260–300 | 280–300 (C: Bambu spec. 280–290, Polymaker 300) |
| PA-CF (wet/conditioned) | 2508 / 4100 / 5666 (2) C | 1056 / 2900 / 4713 (2) C | 54.7 / 68 / 81.7 (2) C | 25.5 / 45 / 64.4 (2) C | | | | |
| PC-CF / PC-GF | 2600 / 2630 / 2664 (2) C | 1231 / 2200 / 3200 (2) C | 36 / 50 / 64 (2) C | 11.2 / 17 / 23 (2) C | 1.22 | HDT 114–134 | 280–295 | ~285, fan 0 (C, Prusament) |
| PET-CF / PETG-CF | 1700 / 4730 / 7773 (6) C | 1340 / 2500 / 3559 (4) C | 47 / 63 / 93 (6) C | 12.5 / 33 / 38 (5) C; Prusament 18 | 1.25–1.34 | Tg 68–79; HDT0.45 74–205 | 240–300 | 255–300 (C, spec. temps) |
| PP (unfilled) | 541 (1) C | 435 (1) C | 15.5 (1) C | 9.0 (1) C | ~0.9 E | HDT 54 | 220–250 E | ~240 (E) |
| PP-CF / PP-GF | 2100 / 2400 / 2628 (3) C | 2242 (1) C | 27 / 40 / 42 (3) C | 13 / 16 / 18 (3) C | 0.91–1.12 | HDT0.45 127–158 | 240–270 | 245–270 (C, spec.) |
| POM | 1870 (1) C? | 1500 E | 50 (1) C? | 20–30 E | 1.41 E | Tm 165–175; HDT ~100 E | 195–220 C? | ~215 (E) |
| PEI (1010/9085) | 2300 / 2500 / 2918 (4) C | 2100 E (edge) | 56 / 90 / 95 (4) C | 14 (1) C (Prusament) | 1.27–1.28 | Tg 186 (9085) / 217 (1010); HDT 194–208 | 375–410 | 400–410 + hot chamber (E) |
| PEEK | 3300 / 3720 / 4500 (4) C | 2700 (1) C | 60 / 80 / 100 (5) C | 20–92 (3) C | 1.28–1.30 | Tg 143–151; HDT 140–152 | 380–450 | 380–420 with chamber ≥150 °C or annealing (C, Victrex) |
| PEKK | 2750 (1) C? | — | 105 (1) C? | 32 (1) C? | 1.26–1.27 | Tg 159–162 | 350–400 | ~390 + chamber (E) |
| PSU / PPSU | 2221 (1) C (PPSU) | 2150 (1) C | 74.5 (1) C | 49 (1) C | 1.27–1.29 | HDT0.45 215 (PPSU); PSU Tg ~185 E | 350–410 | ~400 (PPSU, E) |
| PVDF | 2450 (1) C? | — | 25–51 (2) C? | 15–25 E | 1.78 E | Tm ~170; Tg −35 E | 225–270 | ~255 (E) |
| TPU 95A | 9–34 (secant/low strain) C | 7.4–7.8 C | 27–44 (3) C | 12–23 (2) C; Prusament interlayer 9.5 | 1.20–1.25 | — | 210–240 | ~230 (C, spec. temps) |

Typical = rough median across brands **excluding Prusament "interlayer adhesion" values**, which sit at the low end of every
range (stricter in-house test); min still includes them.

\* 6 MPa is Prusament PLA Blend (outlier); the plain-PLA minimum is Prusament PLA 17 MPa.
\*\* Bambu ABS lists E_xy 3800 MPa next to a bending modulus of 1880 MPa; I treat 3800 as a likely typo and exclude it
from the typical value.

### 1.1 Anisotropy ratios observed (for estimating missing Z values)

| Family | Rs = S_z/S_xy (range, sources) | Re = E_z/E_xy | Suggested FEA default (E) |
|---|---|---|---|
| PLA | 0.52–0.90 (Bambu 0.90, Polymaker 0.77–0.78, BASF 0.61, eSun 0.52, Prusament 0.33–0.60, CNC Kitchen 0.67) | 0.81–0.92 | Rs 0.70, Re 0.88 |
| PETG | 0.38–0.84 (Polymaker 0.78–0.84, Bambu 0.68–0.81, Kimya 0.75, BASF 0.51, Prusament 0.38, CNC Kitchen 0.58) | 0.77–0.95 | Rs 0.65, Re 0.88 |
| ABS | 0.54–0.89 (Polymaker 0.89 in 90 °C env., Bambu 0.78, BASF 0.59–0.61, CNC Kitchen 0.54 open / 0.74 at 65 °C chamber) | 0.80–0.93 | Rs 0.60 open, 0.75 heated chamber; Re 0.85 |
| ASA | 0.26–0.73 (Polymaker 0.73, Bambu 0.67, BASF 0.35, Prusament 0.26, CNC Kitchen hooks 0.29) | 0.77–0.87 | Rs 0.55 open, 0.70 chamber; Re 0.85 |
| PC | 0.33–0.90 (Bambu PC 0.90, PolyLite 0.76, PolyMax 0.78, Bambu PC-FR 0.35, Prusament 0.33; CNC Kitchen 0.65–0.77) | 0.69–0.95 | Rs 0.65, Re 0.85 |
| Nylon CF/GF | 0.26–0.73 (dry) | 0.45–0.58 (fibre-filled) | Rs 0.50, Re 0.50 |
| PET-CF | 0.20–0.64 | 0.46–0.65 | Rs 0.45, Re 0.55 |
| TPU | 0.28–0.82 | 0.76–0.85 | Rs 0.75, Re 0.80 |
| PEEK | 0.69 (Victrex AM200 ZX/XY), 0.3–1.1 (AM450 range) | 0.82 | Rs 0.6 with chamber, 0.35 without; Re 0.8 |
| Fibre-filled in general | Re ≈ 0.4–0.6 because fibres align with the road in XY but not across layers | | |

Unfilled amorphous polymers lose little stiffness across layers (Re 0.8–0.95) but a lot of strength; fibre-filled
grades lose both (Re ~0.5). Z elongation at break is 1/2 to 1/5 of XY (brittle across layers) in almost every sheet.

---

## 2. Per-brand tables (all C unless marked)

### 2.1 Bambu Lab (TDS V1.0–V3.0, ISO 527 / GB/T 1040; specimens annealed and dried; 100 % infill)

| Product (TDS ver.) | E_xy / E_z | S_xy / S_z | Elong. xy / z (%) | Flex mod xy / z | Flex str xy / z | Impact xy / z (kJ/m2) | Density | Tg | HDT 1.8 / 0.45 | Vicat | Nozzle range | Specimen nozzle / bed / speed |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| PLA Basic (V2.0) | 2680 / 2160 | 39 / 35 | 12.2 / 7.5 | 2750 / 2370 | 76 / 68 | 26.6 (notched 7.9) / 13.8 | 1.24 | 60 | 54 / 57 | 57 | 190–230 | 220 / 35 / 200 mm/s |
| PLA Matte (V2.0) | 1960 / 1670 | 32 / 25 | 32.8 / 4.8 | 2360 / 2040 | 53 / 32 | 19.2 / 6.6 | 1.31 | 61 | 52 / 58 | 63 | 190–230 | 220 / 35 / 200 |
| PLA Tough (V1.0) | 2330 / 2110 | 44 / 39 | 8.2 / 5.6 | 2730 / 2490 | 92 / 71 | 31.2 (n. 11.5) / 12.3 | 1.26 | 57 | 53 / 55 | 58 | 190–230 | 220 / 35 / 200 |
| PLA-CF (V2.0) | 2790 / 2160 | 38 / 26 | 12.7 / 3.6 | 3700 / 2260 | 96 / 49 | 23.2 (n. 7.6) / 7.8 | 1.22 | 63 | 54 / 55 | 69 | 210–240 | 230 / 35 / 180 |
| PETG Basic (V2.0) | 1460 / 1120 | 48 / 39 | 11.2 / 5.7 | 1790 / 1580 | 81 / 64 | 52.7 (n. 21.6) / 13.6 | 1.25 | 68 | 65 / 69 | 78 | 240–270 | 255 / 70 / 150 |
| PETG HF (V1.0) | 1810 / 1540 | 34 / 23 | 8.6 / 5.1 | 2050 / 1810 | 64 / 48 | — / 10.6 | 1.28 | 66 | 62 / 69 | 70 | 230–260 | 255 / 70 / 200 (not annealed: "annealing not recommended") |
| PETG-CF (V2.0) | 2460 / 1340 | 59 / 38 | 10.4 / 4.7 | 2890 / 1680 | 83 / 62 | 41.2 (n. 15.7) / 10.7 | 1.25 | 68 | 68 / 74 | 85 | 240–270 | 255 / 70 / 150 |
| PET-CF (V1.0) | 4730 / 2160 | 76 / 38 | 4.1 / 2.2 | 5080 / 2530 | 149 / 51 | 8.6 / 4.5 | 1.29 | 75 | 182 / 205 | 226 | 260–290 | 290 (note says 270) / 100 / 100 |
| ABS (V2.0) | 3800** / 2960 | 41 / 32 | 10.5 / 4.7 | 1880 / 1590 | 68 / 45 | 21.5 / 7.4 | 1.05 | — | 84 / 87 | 94 | 240–270 | 260 / 80 / 200 |
| ASA (V2.0) | 2450 / 2120 | 52 / 35 | 9.2 / 4.6 | 1920 / 1650 | 74 / 42 | 19.6 / 4.9 | 1.05 | — | 92 / 100 | 106 | 240–270 | 260 / 80 / 200 |
| PC (V2.0) | 2110 / 1450 | 62 / 56 | 3.8 / 2.1 | 2310 / 1620 | 108 / 55 | 7.5 / 9.0 | 1.20 | 145 | 117 / 112 | 119 | 260–280 | 270 / 100 / 200 |
| PC FR (V1.0) | 1670 / 1450 | 60 / 21 | 2.4 / 1.5 | 1890 / 1660 | 90 / 47 | 55 (n. 8.4) / 8.0 | 1.18 | 145 | 108 / 113 | 114 | 260–280 | 270 / 100 / 100 |
| PA6-CF (V3.0) | 4430 / 2170 | 102 / 48 | 5.8 / 3.7 | 5460 / 2240 | 151 / 80 | 13.4 / 5.5 | 1.09 | 68 | 164 / 186 | 212 | 260–290 | 280 / 100 / 100 |
| PAHT-CF (V2.0) | 3860 / 2180 | 88 / 64 | 10.4 / 5.2 | 4120 / 1680 | 140 / 68 | 57.5 (n. 22.8) / 13.3 | 1.06 | 70 | 170 / 194 | 220 | 260–290 | 290 / 100 / 100 |
| TPU 95A (V3.0) | 9.2 / 7.8 | 29.6 / 23.2 | >700 / >500 | — | — | 125.2 / 88.7 | 1.20 | — | — | — | 220–240 | 230 / 35 / 80 |
| TPU 95A HF (V1.0) | 9.8 / 7.4 | 27.3 / 22.3 | >650 / >480 | — | — | 123.2 / 86.3 | 1.22 | — | — | — | 220–240 | 230 / 35 / 140 |

Bambu also publishes a one-page "Filament Guide" with XY bending, impact XY/Z, HDT 0.45, water absorption, nozzle
range, fan and annealing per material. Its nozzle ranges differ slightly from some TDS: ABS/ASA 240–280, PC 260–290,
PET-CF / PAHT-CF / PA6-CF 260–300. Annealing: PLA 50–60 °C, ABS/ASA 80–90 °C, PC 85–100 °C, CF nylons and PET-CF
90–130 °C, 6–12 h.

Sources: `https://cdn.shopify.com/s/files/1/0380/5149/files/Bambu_PLA_Basic_Technical_Data_Sheet.pdf`,
`.../Bambu_PLA-CF_Technical_Data_Sheet.pdf`, `.../Bambu_PLA_Matte_Technical_Data_Sheet.pdf`,
`.../Bambu_PLA_Tough_Technical_Data_Sheet.pdf`, `.../Bambu_PETG_Basic_Technical_Data_Sheet.pdf`,
`.../Bambu_PETG-CF_Technical_Data_Sheet.pdf`, `.../Bambu_PAHT-CF_Technical_Data_Sheet.pdf`,
`.../Bambu_PC_FR_Technical_Data_Sheet.pdf` (all under cdn.shopify.com/s/files/1/0380/5149/files/);
ASA: `https://cdn.shopify.com/s/files/1/0645/5876/0155/files/Bambu_ASA_Technical_Data_Sheet.pdf`;
PA6-CF: `https://cdn.shopify.com/s/files/1/0584/7236/6216/files/Bambu_PA6-CF_Technical_Data_Sheet.pdf`;
TPU 95A: `https://cdn.shopify.com/s/files/1/0645/5876/0155/files/Bambu_TPU_95A_Technical_Data_Sheet.pdf`;
ABS / PC / PET-CF / TPU 95A HF: reseller-hosted copies of the Bambu TDS at additive-x.com
(`https://www.additive-x.com/shop/mpattachments/file/viewonline/id/566/product_id/2315` ABS,
`.../id/591/product_id/2340` PC, `.../id/593/product_id/2342` PET-CF, `.../id/823/product_id/2584` TPU 95A HF);
PETG HF: `https://botland.com.pl/img/art/inne/25540_karta_produktu.pdf` (copy of Bambu TDS V1.0);
Filament guide: `https://cdn1.bambulab.com/filament/a5e185aed14e6/filament-guide-en-4.pdf`.

### 2.2 Prusament (Prusa Polymers) — ISO 527-1; "Horizontal" = flat, "Vertical xz" = on edge (in-plane); Z = interlayer adhesion

| Product (ver., date) | E horiz / on-edge (GPa) | S horiz / on-edge | **Interlayer (Z)** | Rs = interlayer/horiz | Elong. at yield (%) | Flex str h / v | Flex mod h / v (GPa) | Charpy unnotched h / v (notched) | Density | HDT 1.8 / 0.45 | Nozzle / bed recommended | Specimen nozzle / bed |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| PLA (1.1, 2022-07-27) | 2.3 / 2.4 | 51 / 59 | **17 ± 3** | 0.33 | 2.9 / 3.2 | 83 / 99 | 3.1 / 3.2 | 13 / 14 | 1.24 | 55 / 55 | 210±10 / 40–60 | 215 / 60 |
| PLA High Speed (1.0, 2026-06-17) | 2.5 / 2.3 | 68 / 65 | **41 ± 3** (anisotropy coeff. 0.6) | 0.60 | 3.9 / 4.1 | 98 / 95 | 3.3 / 2.8 | 17 / 19 | 1.24 | 55 / 57 | 210±20 / 50±10 | 215, 300 mm/s |
| PLA Blend (1.0, 2022-07-27; Polish sheet) | 1.6 / 1.9 | 32 / 42 | **6 ± 1.3** | 0.19 | 3.0 / 3.5 | 47 / 60 | 2.0 / 1.6 | 13 / 15 | 1.24 | 52.5 / 55 | 210±10 / 40–60 | 215 / 60 |
| rPLA Natural Pigments (1.0, 2023-11-21) | 2.3 / 2.4 | 50.6 / 52 | **23 ± 2** | 0.45 | 3.1 / 3.1 | 75 / 77 | 3.0 / 2.8 | 12 / 13 | 1.24 | 59 / 61 | 205±10 / 40–60 | 215 |
| PLA Recycled (1.0, 2021-12-01) | 2.1 / 2.3 | 49 / 55 | **21 ± 2** | 0.43 | 3.1 / 2.9 | 82 / 87 | 3.1 / 3.0 | 14 / 13 | 1.24 | 53 / 55 | 210±10 / 40–60 | — |
| PETG (1.1, 2022-02-16) | 1.5 / 1.6 | 47 / 50 | **18 ± 4** | 0.38 | 5.1 / 5.1 | 66 / 70 | 1.7 / 1.6 | NB / NB (6 / 3) | 1.27 | 68 / 68 | 250±10 / 80±10 | 250 / 80 |
| PETG V0 (1.0, 2023-08-17) | 1.7 / 1.8 | 39 / 42 | **16.4 ± 1.3** | 0.42 | 3.5 / 3.7 | 60 / 64 | 1.0 / 1.0 | 23 / 33 | 1.27 | 68 / 74 | 230±10 / 80±10 | 230 / 80 |
| PETG Carbon Fiber (1.0, 2022-12-20) | 1.7 / 1.8 | 47 / 52 | **18 ± 3** | 0.38 | 4.3 / 4.5 | 68 / 80 | 2.3 / 3.1 | 29 / 43 | 1.27 | 80 / 96 | 265±10 / 90±10 | 265 |
| ASA (1.1, 2024-05-20) | 1.6 / 1.7 | 42 / 45 | **11 ± 1** | 0.26 | 3.4 / 3.8 | 64 / 69 | 2.0 / 1.9 | 25 / 38 (12 / 15) | 1.07 | 86 / 93 | 260±10 / 110±5 | 265 / 110 |
| PC Blend (1.1, 2022-02-16; Czech sheet) | 1.9 / 2.0 | 63 / 63 | **21 ± 2** | 0.33 | 5.8 / 5.8 | 88 / 94 | 2.1 / 2.2 | NB / NB (12 / 12) | 1.22 | 93 / 113 | 275±10 / 110±10 | 275 / 115, fan off, EM 1.034 |
| PC Blend Carbon Fiber (2.0, 2023-04-19) | 2.6 / 3.2 | 64 / 70 | **23 ± 2** | 0.36 | 4.6 / 3.7 | 94 / 130 | 3.5 / 4.2 | 32 / 35 (12 / 17) | 1.22 | 106 / 114 | 285±10 / 110±10 | 285 / 115, fan off, EM 1.07 |
| PC Space Grade (1.0, 2025-09-24) | 3.1 / 3.7 | 72 / 75 | **18 ± 1** | 0.25 | 3.8 / 3.0 | 89 / 104 | 3.7 / 4.9 | 22 / 27 | 1.21 | 128.8 / 137.6 | 290±10 / 120±10 | 290, 100 mm/s |
| PA11 (1.0, 2026-07-22) | 1.2 / 1.2 | 38.3 / 39.5 | **32 ± 1** (anisotropy coeff. 0.84) | 0.84 | 5.6 / 5.6 | 41.4 / 47.2 | 1.1 / 1.0 | — (12.9 / 11.7) | 1.02 | 49.9 / 58.7 | 275±10 / 100±10 | 275 |
| PA11 Carbon Fiber (1.0, 2022-06-29) | 2.5 / 3.3 | 42 / 49 | **20 ± 5** | 0.48 | 3.3 / 2.6 | 63 / 103 | 3.0 / 6.2 | 30 / 51 (12 / 17) | 1.11 | 152 / 192 | 285±5 / 110±10 | 285 / 110 |
| PP Carbon Fiber (1.0, 2024-11-15) | 2.1 / 2.5 | 27.3 / 30.7 | **13 ± 1** | 0.48 | 3.4 / 3.2 | 27.5 / 41.7 | 1.8 / 3.4 | 19 / 17.2 | 0.91 | 115 / 158 | 270±10 / 85±10 | 270, 40 mm/s |
| PP Glass Fiber (1.0, 2024-11-15) | 2.1 / 2.5 | 40.3 / 48.8 | **18 ± 1** | 0.45 | 4.5 / 4.1 | 50.2 / 70.4 | 2.1 / 2.7 | 17.6 / 26.9 | 1.12 | 112.6 / 138.3 | 245±10 / 95±10 | 245, 50 mm/s |
| PEI 1010 (1.0, 2024-09-18) | 2.3 / 2.1 | 95 / 72 | **13.97 ± 1.47** | 0.15 | 6.6 / 4.8 | 98 / 138 | 2.3 / 2.7 | 19.7 / 39 | 1.27 | 194 / 207 | 410±10 / 140–160 | 410, 40 mm/s |
| PVB (1.1, 2022-02-16) | 1.7 / 1.7 | 50 / 49 | **9 ± 1** | 0.18 | 4.6 / 4.4 | 72 / 73 | 2.2 / 2.3 | 55 / 59 | 1.10 | 58 / 63 | 215±10 / 75±5 | — |
| TPU 95A (1.0, 2025-05-29) | — | 41.4 (ISO 37) | **9.5 ± 0.2 (ISO 527)** | 0.23 | 563 % (ISO 37) | — | — | — | 1.25 | 78.6 (1.8) ; Vicat A 101.6 | 230±10 / 65±10 | 220, 36 mm/s |

Notes: the "prusament-pp" product page links the PP Carbon Fiber sheet. Prusa notes on PC Blend CF: "cooling
decreases significantly interlayer adhesion with just small or no effect on printing quality" (fan 0 % recommended).
The 2018 PLA sheet v1.0 had a "Vertical Z-axis" column but its PDF text is unreadable (font encoding), so it is not used.

Sources (opened): `https://prusament.com/materials/` → individual pages; PDFs:
PLA v1.1 via `https://www.matterhackers.com/r/xFvGl8`;
PLA HS `https://storage.googleapis.com/prusa3d-content-prod-14e8-wordpress-prusament-prod/2026/06/269cd3c8-prusament-pla-hs-technical-datasheet.pdf`;
PLA Blend `https://download.kamami.pl/p1201824-prusament-pla-blend-karta-danych-technicznych%20%281%29.pdf`;
rPLA `.../2023/11/0eaa8597-prusament-rpla-natural-pigments-technical-data-sheet.pdf`;
PLA Recycled `https://prusament.com/wp-content/uploads/2021/12/7_PLA-Recycled_Prusament_TDS_2021_V4.pdf`;
PETG `.../2023/10/9f8d2165-tds_prusament-petg_n_en.pdf`;
PETG V0 `https://cdn.shopify.com/s/files/1/1745/6181/files/prusament-petg-v0-technical-data-sheet.pdf`;
PETG CF `https://prusament.com/wp-content/uploads/2023/02/TDS_Prusament-PETG-CF-Black.zip` (EN PDF inside);
ASA `.../2024/05/bc8551ef-tds_prusament-asa_2024_en.pdf`;
PC Blend (CZ) `https://www.materialpro3d.cz/user/related_files/technicky-list-prusament_pcblend.pdf`;
PC Blend CF `https://prusament.com/wp-content/uploads/2023/04/TDS_Prusament-PCCF_2023_EN.pdf`;
PC Space Grade `.../2025/09/e032493a-tds_pc_space_grade_en.pdf`;
PA11 `.../2026/08/2d2145fc-pa11_tds_eng.pdf`; PA11 CF `https://prusament.com/wp-content/uploads/2022/08/TDS_PA11CFB.pdf`;
PP CF `.../2025/01/b4ef2bf6-technical-data-sheet-1.pdf`; PP GF `.../2025/07/512923fd-tds_pp_glass_fiber_en.pdf`;
PEI 1010 `.../2024/09/53ee8f5e-tds_prusament_pei-1010_en.pdf`;
PVB `https://prusament.com/wp-content/uploads/2022/10/PVB_Prusament_TDS_2021_10_EN.pdf`;
TPU 95A `.../2025/05/80316f6d-tds-prusament_tpu_95a_en-1.pdf`
(`...` = `https://storage.googleapis.com/prusa3d-content-prod-14e8-wordpress-prusament-prod`).

### 2.3 Polymaker (TDS V5.4 via wiki.polymaker.com; Fiberon V1.0–V1.1; ISO 527 / GB/T 1040)

| Product | E_xy / E_z | S_xy / S_z | Rs | Elong. xy / z (%) | Flex mod / str (xy) | Notched Charpy xy (z) | Density | Tg | HDT 1.8 / 0.45 | Vicat | Nozzle / bed | Specimen T / bed / env. |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| PolyLite PLA | 3427 / 3065 | 52.3 / 40.5 | 0.77 | 6.3 / 1.8 | 3231 / 86.9 | 3.3 | 1.17 | 61 | 58 / 60 | 63 | 190–230 / 25–60 | 230 / 50 / ambient, fan on |
| PolyMax PLA | 2151 / 1984 | 41.3 / 32.4 | 0.78 | 5.7 / 4.7 | 2393 / 62.0 | 38.9 | 1.17–1.24 | 61 | 52 / 55 | 62 | 190–230 / 25–60 | 230 / 50 / ambient |
| PolyLite PLA-CF | 3281 / 2213 | 31.2 / 15.1 | 0.48 | 13.2 / 0.9 | 3381 / 51.7 | 5.5 | 1.29 | 62 | 50 / 54 | 64 | 190–230 / 25–60 | 230 / 50 / ambient |
| PolyLite PETG | 2117 / 1899 | 50.8 / 42.8 | 0.84 | 8.4 / 3.3 | 1899 / 69.6 | 2.6 | 1.25 | 81 | 75 / 78 | 84 | 230–260 / 70–80 | 240 / 80 / ambient, fan off |
| PolyMax PETG | 1684 / 1603 | 37.9 / 29.4 | 0.78 | 5.2 / 3.1 | 1068 / 58.3 (z 55.1) | 11.6 (2.4) | 1.24 | 79 | 72 / 76 | 82 | 230–260 / 70–80 | 240 / 80 / ambient |
| PolyLite ABS | 2247 / 2081 | 33.4 / 29.7 | 0.89 | 17.9 / 3.1 | 2127 / 56.2 | 18.0 | 1.12 | 101 | 98 / 100 | 104 | 245–265 / 90–100 | 260 / 90 / **90 °C** |
| PolyLite ASA | 2379 / 1965 | 43.8 / 32.0 | 0.73 | 6.7 / 1.65 | 3206 / 73.4 | 10.3 (6.7) | 1.13 | 98 | 100 / 103 | 105 | 230–260 / 75–95 | 260 / 90 / **90 °C** |
| PolyLite PC | 2497 / 2371 | 69.1 / 52.8 | 0.76 | 4.8 / 2.7 | 2640 / 106.1 | 4.1 | 1.19 | 113 | 107 / 111 | 120 | 250–270 / 90–105 | 260 / 100 / **90 °C**, anneal 90 °C 2 h |
| PolyMax PC | 2435 / 2149 | 53.4 / 41.4 | 0.78 | 4.5 / 2.8 | 2050 / 81.3 | 21.3 | 1.19 | 113 | 99 / 114 | 117 | 250–270 / 90–105 | 260 / 100 / 90 °C |
| PC-ABS | 1835 / 1677 | 39.9 / 22.9 | 0.57 | 4.2 / 1.5 | 2081 / 66.3 | 25.8 | 1.10 | 109 | 106 / 112 | 135 | 250–270 / 90–105 | 260 / 100 / 90 °C |
| PolyMide CoPA (dry, annealed 80 °C 6 h) | 2703 / 2209 | 78.0 / 45.8 | 0.59 | 12.7 / 1.3 | 2510 / 109.8 | 6.9 | 1.12 | 66 | 70 / 111 | 180 | 250–270 E | — |
| PolyMide CoPA (conditioned) | 724 / 660 | 34.3 / 13.7 | 0.40 | 13.2 / 1.7 | 636 / — | — | | | | | | |
| PolyMide PA6-CF V5.2 (dry, ann. 80 °C 6 h) | 7453 / 4354 | 105 / 67.7 | 0.64 | 3.0 / 2.5 | 8339 / 169 | 13.3 | 1.17 | 74.2 | 173 / 215 | — | 280–300 / 25–50 | 300 |
| PolyMide PA6-CF V5.2 (70 % RH) | 5666 / 4713 | 81.7 / 64.4 | 0.79 | 4.6 / 1.8 | 6387 / 152 | 32.8 | | | | | | |
| Fiberon PA6-CF20 (dry, ann. 100 °C 16 h) | 8637 / 3760 | 109.3 / 54.0 | 0.49 | 2.1 / 1.9 | 7038 / 161 (z 2975 / 71.3) | 11.0 | 1.17 | 74.2 | 173 / 215 | 219 | 280–300 / 40–50 | — |
| Fiberon PA6-CF20 (wet: 60 °C water 48 h) | 2508 / 1056 | 54.7 / 25.5 | 0.47 | 7.0 / 6.7 | 2286 / 64.9 | 35.6 | | | | | | |
| Fiberon PA612-CF15 (dry) | 5137 / 2786 | 91.9 / 48.3 | 0.53 | 2.6 / 2.1 | 4584 / 138 (z 2265 / 73.5) | 8.1 | 1.03 | — | 114 / 175 | 206 | 250–300 / 40–50 | — |
| Fiberon PA612-CF15 (wet) | 3991 / 2387 | 83.1 / 35.6 | 0.43 | 4.1 / 1.7 | | | | | | | | |
| Fiberon PA12-CF10 (dry) | 3311 / 1807 | 77.4 / 52.2 | 0.67 | 4.2 / 5.0 | 2887 / 112 (z 1453 / 69.8) | 9.9 | 1.06 | 55 | 105 / 131 | 170 | 280–300 / 40–50 | — |
| Fiberon PA12-CF10 (wet) | 3132 / 1622 | 71.7 / 42.1 | 0.59 | 5.3 / 5.3 | | | | | | | | |
| Fiberon PET-CF17 (ann. 120 °C 10 h) | 5481 / 3559 | 65.9 / 27.9 | 0.42 | 2.4 / 0.8 | 4744 / 109 (z 2768 / 43.4) | 5.1 | 1.34 | 79.3 | 105 / 147.5 | 238 | 270–300 / 70–80 | 300 / 70 |
| Fiberon PPS-CF10 (ann. 125 °C 16 h) | 5447 / 2790 | 59.4 / 32.0 | 0.54 | 1.4 / 1.6 | 4647 / 94.3 (z 2619 / 30.0) | 5.3 | 1.29 | 97.7 | 133 / 252.5 | 268 | 310–350 / 80–90 | 330–350 / 90 |
| PolyFlex TPU95 | 33.6 (ISO 37) | — | — | 551 | — | — | 1.20–1.24 | — | — | — | 210–230 / 25–60 | 230 / 50 |

Sources: `https://wiki.polymaker.com/polymaker-products/more-about-our-products/documents/technical-data-sheets`
(Markdown pages per product, each linking its TDS PDF, e.g.
`https://cdn.shopify.com/s/files/1/0548/7299/7945/files/PolyLite_PETG_TDS_EN_V5.4.pdf`);
Fiberon PDFs `https://cdn.shopify.com/s/files/1/0548/7299/7945/files/TDS_FIBERON_PA6-CF20_V1.1_EN.pdf`,
`..._PA612-CF15_V1.1_EN.pdf`, `..._PA12-CF10_V1.1_EN.pdf`,
`https://fiberon.polymaker.com/wp-content/uploads/TDS_FIBERON-PET-CF17_V1.0_EN.pdf`,
`https://fiberon.polymaker.com/wp-content/uploads/TDS_FIBERON-PPS-CF10_V1.1_EN.pdf`;
PolyMide PA6-CF V5.2 `https://cdn.shopify.com/s/files/1/0548/7299/7945/files/PolyMide_PA6_CF_TDS_V5.2.pdf`.
Note: the old `polymaker.com/wp-content/uploads/lana-downloads/...` URLs now return 404.

### 2.4 BASF Forward AM — Ultrafuse (catalogue, ISO 527; XY flat, ZX upright)

| Product | E_xy / E_xz-or-zx | S_xy / S_zx | Rs | Elong. xy / zx (%) | HDT 0.45 (°C) | Nozzle / bed (°C) |
|---|---|---|---|---|---|---|
| PLA | 2308 / 2131 | 34.7 / 21.2 | 0.61 | 4.2 / 1.2 | 65 | 210–230 / 50–70 |
| PLA PRO1 | 3166 / 2930 | 48 / 21.8 | 0.45 | 21.9 / 0.9 | — | 200–220 / 50–70 |
| PET | 1933 / 1665 | 33.4 / 17.2 | 0.51 | 2.7 / 1.1 | 63 | 210–230 / 60–80 |
| rPET | 1640 / 1334 | 38.6 / 14.7 | 0.38 | 4.3 / 1.2 | 71 | 225–245 / 65–85 |
| ABS | 1958 / 1608 | 36.3 / 21.3 | 0.59 | 7.4 / 1.8 | 96 | 240–260 / 90–110 |
| ABS Fusion+ | 1379 / 1106 | 29.5 / 17.9 | 0.61 | 10.9 / 2.1 | 91 | 240–260 / 100–120 |
| ASA | 1828 / 1400 | 34.6 / 12 | 0.35 | 4.5 / 1 | 101 | 260–280 (row after ASA) / 100–120 |
| HiPS | 1588 / 1603 | 18.4 / 13.7 | 0.74 | 1.4 / 1.3 | 91 | 240–260 / 100–120 |
| PP | 541 / 435 | 15.5 / 9.0 | 0.58 | 118.6 / 5.4 | 54 | — |
| PA | 395 / 334 | 33.2 / 17.6 | 0.53 | 143 / 12.8 | 135 | — |
| PC/ABS FR | 2545 / 2188 | 50.1 / 17.3 | 0.35 | 10.7 / 0.8 | 86 | — |
| PPSU | 2221 / 2150 | 74.5 / 49 | 0.66 | 7.3 / 2.9 | 215 | 390–410 (PPSU TDS) |
| PP GF30 | 2628 / 2242 | 41.7 / 15.9 | 0.38 | 4.4 / 0.8 | 127 | 240–260 |
| PET CF15 | 6178 / 2822 | 63.2 / 12.5 | 0.20 | 3.7 / 0.5 | 108 | 250–270 / 65–85 |
| PAHT CF15 | 5052 / 2455 | 62.9 / 19.1 | 0.30 | 2.9 / 0.8 | 128 | 260–280 / 100–120 |
| PA6 GF30 | 2469 / 1156 | 46.4 / 12.2 | 0.26 | 3.2 / 1.9 | 114 | — |
| PC GF30 | 2664 / 1231 | 36.1 / 11.2 | 0.31 | 2.4 / 1.1 | 134 | — |
| TPU 85A / 64D / 95A (stress at break) | — | 34 / 10; 37 / 19; 44.2 / 12.2 | 0.29–0.51 | 600 / 320; 399 / 115; 611 / 192 | — | 200–220; 230–255; 210–230 |

BASF also reports Izod for PLA as XY/XZ/ZX (notched 3.3 / 2.1 / 1.6 kJ/m2), showing the same Z weakness.
The PPSU TDS states layer adhesion gives "balanced flexural strength between z- and x-direction".

Sources: Forward AM AES Catalog
`https://move.forward-am.com/hubfs/AES%20Documentation/General%20Resources/Forward%20AM%20AES%20Catalog.pdf`;
PPSU TDS `https://cdn.shopify.com/s/files/1/1339/4265/files/BASF_Ultrafuse_PPSU_TDS_EN_v2.0.pdf`;
product page `https://forward-am.com/material-portfolio/ultrafuse-filaments-for-fused-filaments-fabrication-fff/standard-filaments/ultrafuse-pet/`.

### 2.5 Kimya (Airtech), data updated 2025-12-22 / 2026-05-21

| Product | E_xy (/ E_zx) | S_xy (/ S_zx) | Elong. (%) | Flex mod | Charpy (kJ/m2) | Density | Tg | Nozzle / bed / chamber |
|---|---|---|---|---|---|---|---|---|
| PETG-S | 2158 / 2058 | 52.2 / 39.3 (Rs 0.75) | 3.4 / 2.3 | 1850 / 1636 | 3.99 / 1.4 | 1.278 | 80 | 230–260 / 75–85 |
| PLA-R | 2818 | 55.3 | 4.3 | 2304 | 3.12 | 1.235 | 61 | 190–210 / 20–60 |
| ABS-S | 1484 | 35.3 | 9.8 | 1433 | 24.7 | 1.035 | 107 | 255–270 / 85–95 |
| ABS Kevlar | 2168 | 34.1 | 6.5 | 1976 | 7.54 | 1.036 | 108 | 210–230 / 85–95 |
| ABS Carbon | 3395 | 36.7 | 1.9 | 2952 | 18 | 1.048 | 108 | 235–255 / 85–95 |
| PETG Carbon | 7773 | 92.9 | 1.9 | 5664 | 4.6 | 1.28 | 78 | 230–260 / 85–95 |
| PC-S | 2116 | 59.9 | 5.5 | 1630 | 5.2 | 1.184 | 145 | 270–280 / 135–145 / 135–145 |
| PEI-1010 | 2918 | 90.9 | 5.6 | 2236 | 3.1 | 1.28 | 217 | 375–390 / — / 215–225 |
| PEKK Carbon | 6815 | 102 | 3.5 | 5396 | 5.9 | 1.21 | 164 | 370–380 / 155–165 / 150–160 |
| PEKK-A | (no mechanicals in sheet) | | | | | 1.261 | 159 (Tm 308) | XY: 395–400; ZX: 380–395 / 150–160 / 150–160 |
| PEKK-SC | 64.1 / 32.4 (XZ vs ZX labels unclear) C? | | | | | | | |

Source: copies hosted by reseller `https://www.makershop.fr/cdn/shop/files/Kimya_<name>_3D_TDS.pdf` (e.g. `Kimya_PETG-S_3D_TDS.pdf`).

### 2.6 3DXTech (TDS Rev 3.0; printed, XY flat, 0.25 mm layer, 100 % ±45°)

| Product | E_xy | S_xy | Elong. | Flex mod / str | Tg | HDT 0.45 | Density | Nozzle / bed |
|---|---|---|---|---|---|---|---|---|
| ThermaX PEEK | 3720 | 100 | 28 % | 2700 / 130 | 143 | 140 | 1.30 | 380–400 / 130–140 |
| CarbonX CF-PEEK | 8100 | 105 (break) | 3 % | 8300 / 136 | 143 | — | 1.39 | — / 140 |
| ThermaX PEI 1010 | 2500 C? | 56 C? | 3 % | 2510 / 110 | 217 | 208 | 1.27 | 380–400 / 130–140 |
| ThermaX PEKK-A | 2750 C? | 105 C? | 5 % | 2680 / 95 | 162 | max use 150 | 1.27 | 350 / 140 |
| FluorX PVDF | 2450 C? | 51 (break) C? | 25 % | — | — | — | — | 245–265 / 90–110 |

Sources: PEEK `https://matterhackers.com/r/nZtN6G`, CF-PEEK `https://matterhackers.com/r/YzLXL6` (opened);
PEI 1010 / PEKK-A via `https://plahunter.pages.dev/data-sheets/3dxtech/...` (third-party mirror, C?);
FluorX `https://cdn.shopify.com/s/files/1/0625/4185/6821/files/FLUORX_PVDF_TDS_v3.pdf` (search summary, C?).
No 3DXTech sheet found with Z values.

### 2.7 Other high-temperature references with Z data

| Product | XY / YZ / ZX tensile (MPa) | Modulus XY / YZ / ZX (GPa) | Elong. % | Conditions | Tag |
|---|---|---|---|---|---|
| Victrex AM 200 FIL (PEEK-based, improved interlayer) | 65 / 70 / 45 (yield) | 3.3 / 2.5 / 2.7 | 15 / 15 / 5 | 3DGence F340, 0.15 mm layer, 380 °C nozzle, 60 °C chamber, annealed 170 °C 2 h; Tg 151 | C |
| Victrex AM 450 FIL (PEEK) | XY 60–80; ZX 20–85 (yield) | XY 3–4; ZX 2–4 | XY 2–95; ZX 1–10 | Chamber ≥150 °C recommended; range reflects machine dependence | C |
| Ensinger TECAFIL PEEK EV | XY 89.3 / ZX 92.3 | — | — | from search summary | C? |
| KEXCELLED PEEK K10 | XY 70–80 | 4.2–4.5 | 4–6 | nozzle 400–450, chamber 50 °C for specimens | C |
| Zortrax Z-PEI 1010 | 56 (orientation n/a) | 2.5 | 3 | Tg 217 | C |
| Tarfuse POM NAT | XY 50 | 1.87 | 11 | Tm 165–170 | C? |
| Solvay Solef PVDF AM | 30 yield / 25 break (ASTM D638) | — | — | nozzle 225–235, bed 100 | C? |

Sources: `https://cdn.shopify.com/s/files/1/2327/6017/files/TDS_AM_200_FIL_1.pdf`,
`https://www.victrex.com/-/media/downloads/datasheets/tds-am-450-fil.pdf`,
`https://www.ensingerplastics.com/en/filaments/tecafil-peek-ev-natural-1-75-mm`,
`https://cdn.shopify.com/s/files/1/0068/9635/3333/files/KEXCELLED-PEEK-K10-TDS.pdf`,
`https://cdn.shopify.com/s/files/1/1339/4265/files/Z-PEI_1010_Technical_Data_Sheet_eng.pdf`,
`https://filament2print.com/en/advanced/2241-tarfuse-pom-nat-filament.html`,
`https://cdn.shopify.com/s/files/1/0672/7306/4714/files/SOLEF_PVDF_AM_FILAMENT_MSC_NT_1_TDS.pdf`.

### 2.8 eSun, Fillamentum, Formfutura (weak or no Z data)

| Brand / product | Data | Tag |
|---|---|---|
| eSun PLA+ (TDS v1.0, 2026-02) | S_xy 70.37, S_z 36.36 (Rs 0.52), elong. 4.14 / 2.92 %, GB/T 1040, eSun printed samples | C? (search summary of eSun page; esun3d.com returned 403) |
| eSun PETG+ | S_xy 48 ± 10, S_z 26 ± 5 | C? |
| eSun ABS+ | S_xy 40.12, S_z 14.94 (retailer listing) | C? |
| Fillamentum Extrafill PLA | TDS gives film-test values (ASTM D882, 60 MPa yield) only, not printed parts. Univ. of Debrecen printed-part database: X 54.2, Y 55.0 MPa; Z 28.1 MPa (different colour) | C / M |
| Formfutura (ApolloX ASA, HDglass PETG, EasyFil) | Only bulk/molded-type values (ApolloX 47.5 MPa ISO 527; HDglass 50 MPa ASTM D638), no orientation | C? |

Sources: `https://www.esun3d.com/pla-pro-product/`, `https://www.esun3d.com/petg-pro-product/`,
`https://www.materialpro3d.cz/user/related_files/technical_data_sheet_pla_extrafill_03012019.pdf`,
`https://3dprintmaterials.unideb.hu/?p=351`, `https://cdn.shopify.com/s/files/1/0367/1723/2266/files/TDS_-_ApolloX.pdf`,
`https://cdn.shopify.com/s/files/1/0069/3049/7593/files/TDS_-_HDglass.pdf`.

---

## 3. Layer adhesion vs nozzle (and chamber) temperature — measured numbers

### 3.1 CNC Kitchen (Stefan Hermann), "The Influence of Extrusion Temperature on Layer Adhesion", 2020-12-19 — M

Prusa MK3, dasFilament PLA and PETG, mini tensile dogbones printed standing (layer adhesion) and lying (reference),
4 samples per temperature. Layer height and fan not stated in text.

| PLA nozzle T (°C) | 190 | 200 | 210 | 230 | 250 | 270 |
|---|---|---|---|---|---|---|
| Standing (Z) strength (MPa) | 20 (under-extruded) | 37 | 39 | **40 (peak)** | 37 | 32 |
| Ratio to lying reference (~60 MPa) | 0.33 | 0.62 | 0.65 | 0.67 | 0.62 | 0.53 |

| PETG nozzle T (°C) | 200 | 215 | 230 | 245 | 260 |
|---|---|---|---|---|---|
| Standing (Z) strength (MPa) | 18 | 22 | 30 | **32 (peak)** | 24 |
| Ratio to lying reference (55 MPa) | 0.33 | 0.40 | 0.55 | 0.58 | 0.44 |

Lying (XY) strength changed little with temperature (chart only). Author's advice: low end of the range for looks,
upper end or slightly above for strength. Source: `https://www.cnckitchen.com/blog/the-influence-of-extrusion-temperature-on-layer-adhesion`.

### 3.2 CNC Kitchen, "Does a heated chamber make your 3D prints stronger?", 2023-10-21 — M

0.4 mm nozzle, 0.2 mm layers, 2 perimeters, 100 % infill. ABS (eSUN) at 250 °C nozzle, PC Blend (Prusament) at 275 °C.

| Material | Chamber | Standing (Z) strength | Ratio to flat |
|---|---|---|---|
| eSUN ABS | open (≤ 26 °C) | 21 MPa | 0.54 |
| eSUN ABS | passive, just under 50 °C | 23 MPa (wide scatter) | 0.59 |
| eSUN ABS | active, 65 °C | 29 MPa | 0.74 |
| eSUN ABS | flat reference | 39 MPa | 1 |
| Prusament PC Blend | open / ~50 °C / 65 °C | (MPa in chart only) | 0.65 / 0.72 / 0.77 |

Source: `https://www.cnckitchen.com/blog/does-a-heated-chamber-make-your-3d-prints-stronger`.

### 3.3 CNC Kitchen, "Comparing PLA, PETG & ASA — feat. Prusament", 2020-02-03 — M (hook test, load in kg)

| Material | Nozzle / fan | Flat hook | Upright hook | Upright / flat |
|---|---|---|---|---|
| Prusament PLA | 215 °C / 100 % | 73 kg | 40 kg | 0.55 |
| Prusament PETG | 250 °C / 30–50 % | 55 kg | 25 kg | 0.46 |
| Prusament ASA | 260 °C / 20 % | 57 kg | 17 kg | 0.29 |
| Prusament ASA in ~30 °C enclosure | 260 °C | — | 20 kg (+20 %) | 0.35 |

Also bending modulus (PLA 3300, ASA 2300, PETG 1900 MPa), notched Izod (5 / 18 / 8.6 kJ/m2), heat sag onset
(PLA 60, PETG 80, ASA 110 °C). Source: `https://www.cnckitchen.com/blog/comparing-pla-petg-amp-asa-feat-prusament`.

### 3.4 Papers and other tests

| Source | Material | Result | Tag |
|---|---|---|---|
| Foppiano, Saluja, Fayazbakhsh 2021, "The effect of variable nozzle temperature and cross-sectional pattern on interlayer tensile strength" (Toronto Metropolitan Univ.) | ABS, upright ASTM D638 / ISO 527-2 coupons, 220–260 °C in 10 °C steps, 110 °C bed, enclosure | Strength and modulus rise to 230 °C then fall; best-fit optimum **231 °C**. Baseline at 230 °C: E_z 1.50 GPa, S_z **23.0 MPa** at high print humidity (~75 % RH); ~19.5 % higher at 31–40 % RH. Per-temperature values only in figures. Specimens get opaque (degraded) from 240 °C. | M |
| Tom's 3D (Thomas Sanladerer), "3D printing with ABS: Hotter nozzle or hotter ambient?", 2017-07-23 | Innofil3D ABS | 240 °C + enclosure (32–35 °C ambient) **almost doubled** the layer strength rating vs 235 °C open air; 240 °C alone also stronger; 20 % fan in enclosure slightly stronger still. Relative only, no units. | M |
| Coogan & Kazmer 2017 (Rapid Prototyping J.) | ABS | Bond strength 40–85 % of filament strength depending on processing conditions. | M (summary) |
| Guessasma et al. 2019, Polymers 11(7):1220 | PETG, ±45° (in-plane) | 32.3 MPa at 240 °C vs 34.0 MPa at 250 °C (filament 54.3) — not Z, but same trend. | M |
| COBEM 2021 (fan speed) | PLA, PETG | Interlayer yield strength about **40 % lower** with cooling fan on vs fan off. | M (summary) |
| Ultimaker community user | PLA, ABS | Informal: PLA full fan 14.3 vs ABS 215 °C no fan 7.1, 245 °C no fan 6.6 (units unclear). Low reliability. | M (anecdotal) |
| Prusa (PC Blend CF TDS) | PC-CF | "Cooling decreases significantly interlayer adhesion with little effect on quality" → fan 0 %. | C |
| Victrex AM 450 FIL | PEEK | ZX yield 20–85 MPa depending on machine/chamber; chamber ≥150 °C recommended. | C |

Sources: `https://www.torontomu.ca/content/dam/frames/publications/Foppiano_Saluja_Fayazbakhsh_2021.pdf`,
`https://toms3d.org/?p=1292`, `https://www.emerald.com/insight/content/doi/10.1108/RPJ-03-2016-0050/full/html`,
`https://pmc.ncbi.nlm.nih.gov/articles/PMC6680790`, `https://abcm.org.br/proceedings/view/COB2021/0310`,
`https://community.ultimaker.com/profile/334937-wisar/content/page/8/`.

### 3.5 Model for the engine (E — derived from the numbers above)

A simple, data-backed factor for Z strength as a function of nozzle temperature T, per material:

`S_z(T) = S_z,ref * f(T)`, with f = 1 at T_opt and a piecewise-linear fall-off:

| Material | T_opt (°C) | f at T_opt − 20 | f at T_opt − 40 | f at T_opt + 20 | f at T_opt + 40 | Basis |
|---|---|---|---|---|---|---|
| PLA | 230 | 0.97 (210) | 0.50 (190, under-extrusion) | 0.92 (250) | 0.80 (270) | CNC Kitchen PLA |
| PETG | 245 | 0.85 (225) | 0.62 (205) | 0.75 (265) | — | CNC Kitchen PETG (interpolated) |
| ABS / ASA | 230–250 (+ chamber) | 0.85 | 0.6 E | 0.9 E | 0.8 E | Foppiano (opt. 231), toms3d, CNC Kitchen |

Chamber factor (ABS-like amorphous): f_ch = 1.00 open (≤ 26 °C), 1.10 at ~50 °C, 1.38 at 65 °C (CNC Kitchen ABS);
PC Blend 1.00 / 1.11 / 1.18. Fan factor: 0.6–0.8 with strong cooling for PETG/ABS/ASA/PC (COBEM, Prusa note);
PLA tolerates 100 % fan (CNC Kitchen PLA at 100 % fan still 0.55–0.67 Rs).

---

## 4. How manufacturers test (summary)

- **Standards**: tensile ISO 527-1/-2 (type 1A, 5A or 1BA specimens; Bambu and Polymaker also cite GB/T 1040),
  ASTM D638 (US brands); flexural ISO 178 / GB/T 9341 / ASTM D790; impact Charpy ISO 179 (Bambu, Prusament,
  Kimya) or Izod ISO 180 (BASF); HDT ISO 75 at 1.8 and 0.45 MPa; Vicat ISO 306; density ISO 1183.
  Prusament interlayer adhesion is an in-house method (TPU sheet labels it ISO 527).
- **Orientation**: XY flat (all), Z upright (Bambu, Polymaker, Kimya ZX, BASF ZX, Victrex ZX). ISO/ASTM 52921
  notation: first letter = longest specimen axis direction. Prusament's "Vertical xz" is on-edge (in-plane).
- **Infill**: 100 % in every sheet that states it (Bambu, Polymaker, Prusament rectilinear 2 perimeters 0 top/bottom,
  3DXTech ±45°, KEXCELLED 45°).
- **Conditioning**: Bambu anneals and dries all specimens; Polymaker nylons/CF annealed, some also wet-conditioned;
  Victrex annealed; Prusament as printed. Values from annealed specimens are upper bounds for as-printed parts.
- **Environment**: Polymaker prints ABS/ASA/PC/PC-ABS at 90 °C environment; Kimya PC-S at 135–145 °C chamber;
  Victrex 60 °C chamber + annealing; Prusament and Bambu at their printers' normal conditions.
- **Statistics**: mean ± standard deviation, typically 5 samples (Victrex 12 repeats).

## 5. Gaps and caveats

- No printed-part Z data found for: POM, PVDF, PSU (only PPSU), PEKK (only an unclear Kimya PEKK-SC figure),
  plain PP from more than one brand, HIPS from more than one brand, PEI Z other than Prusament. Those cells are E.
- 3DXTech, eSun, Formfutura, Fillamentum: mostly XY or non-printed data.
- Temperature sweeps with numbers exist only for PLA and PETG (CNC Kitchen) and ABS (optimum from Foppiano,
  chamber from CNC Kitchen). Every other material's best-adhesion temperature is an estimate from the specimen print
  temperatures in the TDS.
- Some values were seen only through search summaries or third-party mirrors (tag C?). They were not checked
  against the original PDF.
- Bambu ABS E_xy = 3800 MPa looks like a typo (bending modulus 1880, Z modulus 2960).
- Tom's 3D and some CNC Kitchen numbers are from charts or relative ratings; only numbers given in text are listed.

Local copies of the downloaded PDFs and their text extracts are in
`/tmp/claude-0/-home-user-tisma-engine/8eb152f2-9c70-5750-bbb9-dc4ccd484cda/scratchpad/research/pdfs/`.
