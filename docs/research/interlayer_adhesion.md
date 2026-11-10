# Interlayer (Z) adhesion of FFF parts: measured data, models and a proposed engine model

Research note for the structural FEA of the engine. Compiled 2026-11-03 from papers, manufacturer data sheets and
published black-box test results only. No slicer source code was consulted.

How to read the numbers:
- **[M]** means measured: the value or ratio comes straight from a published table or text. Ratios marked [M] were
  computed by this note from the two published values.
- **[E]** means estimated: a value fitted or chosen by this note, or a value from an unverified secondary source.
- `S` = tensile strength, `E` = Young's modulus. "XY" = loaded along the beads (flat or on-edge specimen). "Z" =
  loaded across the layers (upright specimen).

---

## 1. Key findings (short)

1. **The Z/XY strength ratio is not a material constant.** For one material it ranges from about 0.2 to 1.0,
   depending on the process. The same PLA gives 0.23 to 0.9 in one study just by changing layer height against
   nozzle size (Kuznetsov 2018). Vendor data sheets, which use tuned settings, solid specimens and usually
   annealing, show the upper end: 0.62 to 0.90 for unfilled amorphous polymers, and 0.45 to 0.75 for fibre-filled
   ones.
2. **Two separate mechanisms set Z strength, and the model should keep them apart.**
   - (a) **Bond (healing) quality.** This is chain interdiffusion across the interface during the short time it
     stays above Tg. It depends on nozzle temperature, the temperature of the layer below (layer time, fan,
     chamber) and molecular weight. Healing theory gives σ/σ∞ ∝ (t/τ_rep)^(1/4).
   - (b) **Geometry.** These are the grooves or necks between stacked beads. The load-bearing area is about 65%
     instead of 95%, and the notches concentrate strain. Allum et al. (2020) showed that for well-printed PLA the
     interface itself carries bulk strength (59.1 vs 63.9 MPa when the actual bonded area is used), and that the
     nominal anisotropy comes mostly from geometry.
3. **Nozzle temperature has an optimum.** Below it, strength falls steeply, roughly like a healing curve. Above it,
   strength falls again: CNC Kitchen measured a drop from 40 to 32 MPa for PLA at 270 °C, and from 32 to 24 MPa
   for PETG at 260 °C.
4. **Layer time works through the temperature of the previous layer.**
   - Little effect for PLA at layer times of about 5 to 16 s (Allum 2020).
   - For PC, 60 s gave 30.1 MPa against 37.7 to 42.7 MPa at 8 to 20 s, and 3.5 s gave 59.7 MPa, which is bulk
     strength (Adisa et al. 2023).
5. **The chamber or envelope temperature is the strongest lever for high-Tg amorphous polymers.**
   - ABS rose from 0.54 to 0.74 of flat strength between 26 °C and 65 °C (CNC Kitchen).
   - Kumrai-Woodruff reports up to +104% for ABS.
   - CF/PEEK rose from 6.96 to 36.28 MPa with a 230 °C chamber.
   - For semicrystalline nylon, a hotter bed raised crystallinity and *reduced* the bond (Vaes 2021).
6. **Z stiffness is reduced much less than Z strength for unfilled polymers.** E_z/E_xy is 0.77 to 0.91, and some
   studies find it equal to bulk. For fibre-filled grades it is about 0.45 to 0.6, because the fibres align along
   the beads.
7. **Z ductility collapses.** Elongation at break in Z is typically 0.15 to 0.6 of XY (vendor sheets), and Allum
   et al. measured 35.6% of XY toughness. The FEA should treat Z failure as brittle.

---

## 2. Per-material tables

### 2.1 Vendor data (same lab and method per vendor) [M]

Bambu Lab data sheets: ISO 527, 100% infill, specimens annealed and dried before testing (55 to 80 °C, 8 to 12 h).
The print temperature listed is the one used for the specimens.

| Material (Bambu) | S_xy MPa | S_z MPa | **S_z/S_xy** | E_xy MPa | E_z MPa | **E_z/E_xy** | Elong. Z/XY | Specimen nozzle °C |
|---|---|---|---|---|---|---|---|---|
| PLA Basic | 39±2 | 35±3 | **0.90** | 2680 | 2160 | **0.81** | 0.61 | 220 |
| PLA Matte | 32±3 | 25±2 | **0.78** | 1960 | 1670 | **0.85** | 0.15 | 220 |
| PLA Tough | 44±2 | 39±2 | **0.89** | 2330 | 2110 | **0.91** | 0.68 | 220 |
| PLA-CF | 38±4 | 26±2 | **0.68** | 2790 | 2160 | **0.77** | 0.28 | 230 |
| PETG Basic | 48±5 | 39±3 | **0.81** | 1460 | 1120 | **0.77** | 0.51 | 255 |
| PETG HF | 34±4 | 23±4 | **0.68** | 1810 | 1540 | **0.85** | 0.59 | 255 |
| PETG-CF | 59±4 | 38±3 | **0.64** | 2460 | 1340 | **0.54** | 0.45 | 255 |
| ABS | 41±1 | 32±2 | **0.78** | 2200 | 1960 | **0.89** | 0.45 | 260 |
| ABS-GF | 36±3 | 29±3 | **0.81** | 3160 | 2250 | **0.71** | 0.37 | 270 |
| ASA | 37±3 | 31±4 | **0.84** | 2450 | 2120 | **0.87** | 0.50 | 260 |
| ASA Aero (foamed) | 32±4 | 21±4 | **0.66** | – | – | – | – | – |
| PC | 55±4 | 34±3 | **0.62** | 2110 | 1450 | **0.69** | 0.55 | 270 |
| PC FR | 60±4 | 21±3 | **0.35** | 1670 | 1450 | **0.87** | 0.63 | 270 |
| PA6-CF | 102±7 | 48±6 | **0.47** | 4430 | 2170 | **0.49** | 0.64 | 280 |
| PAHT-CF (current sheet) | 88±7 | 64±3 | **0.73** | 3860 | 2180 | **0.56** | 0.50 | 290 |
| PAHT-CF (v2 sheet) | 92±7 | 47±5 | **0.51** | 3860 | 2180 | **0.56** | 0.49 | 290 |
| PET-CF | 74±6 | 35±5 | **0.47** | 4730 | 2160 | **0.46** | 0.53 | 270 |
| TPU 95A | 29.6 | 23.2 | **0.78** | 9.2 | 7.8 | **0.85** | >0.7 | 230 |

Polymaker data sheets: ISO 527 / GB/T 1040, 0.4 mm nozzle.

| Material (Polymaker) | S_xy | S_z | **S_z/S_xy** | E_z/E_xy | Notes |
|---|---|---|---|---|---|
| PolyLite PLA V5.4 | 52.3 | 40.5 | **0.77** | – | |
| PolyLite PETG V5.4 | 50.8 | 42.8 | **0.84** | – | |
| PolyLite ABS V5.2 / V5.5 | 33.3 / 33.4 | 25.4 / 29.7 | **0.76 / 0.89** | 0.84 (V5.2) | the newer sheet shows a higher Z value |
| PolyLite ASA V5.1 | 43.8 | 32.0 | **0.73** | 0.83 | |
| PolyLite PC V5.4 | 69.1 | 52.8 | **0.76** | – | annealed at 90 °C for 2 h |
| PC-ABS V5.1 | 39.9 | 22.9 | **0.57** | – | |
| PolyMide CoPA (PA6/66), dry | 66.2 | 43.3±9.1 | **0.65** | 1.15 (sic) | large Z scatter |
| PolyMide CoPA, conditioned 70% RH | 36.4 | 31.4 | **0.86** | 0.67 | |
| PolyMide PA6-CF, dry / conditioned | 105 / 81.7 | 67.7 / 64.4 | **0.64 / 0.79** | 0.58 / 0.83 | |
| PolyMide PA12-CF, dry (two sets) | 71.6 / 73.4 | 43.3 / 42.0 | **0.60 / 0.57** | 0.55 / 0.50 | |
| PolyMide PA6-GF | 84.5 | 61.4 | **0.73** | – | from the search snippet, not re-read |

Industrial machines with heated build chambers (Stratasys and others; XZ = on edge, ZX = upright):

| Material | XZ | ZX | Ratio | Status |
|---|---|---|---|---|
| ULTEM 9085 (Stratasys sheet, ultimate) | 77 | 58 | **0.75** | [M] from search snippet; another sheet gives 68.1 and 39.4 at break, ratio **0.58** |
| ULTEM 9085 filament (Markforged) | 92.0 | 47.6 | **0.52** | [M] from search snippet |
| ABS-M30 (break) | 28.1 | 26.8 | **0.95** | [M] from a distributor page |
| Nylon 12 (Stratasys, via Xometry) | 49.3 | 41.8 | **0.85** | [E]: third-party listing |

PEEK and PAEK (sources disagree; open-chamber printers):

| Material | S_xy | S_z | Ratio | Status |
|---|---|---|---|---|
| PEEK, open printer (via search summary) | 98.9 | 19.6 | **0.20** | [E]: secondary, primary source not read |
| PEEK, "typical" | 80–100 | 13–20 | **0.15–0.25** | [E]: vendor-oriented summary |
| CF/PEEK, chamber at 230 °C vs not heated (Rodzeń et al. 2021, Composites A) | – | 6.96 → 36.28 | ×5.2 | [M]: abstract |
| GF/PEEK: interlayer strength *fell* from 20.61 to 6.03 MPa as the chamber rose to 200 °C | – | – | – | [M]: abstract, one study; contradicts the line above |
| PEKK, open chamber with halogen top-layer heater (Rodzeń et al. 2025) | – | – | UTS 93.9±5.5 MPa | [M]. The interface was kept below the crystallisation threshold. Orientation was not confirmed by this note. |

### 2.2 Ratios from independent tests (not vendor) [M]

| Source | Material | S_z / reference | Conditions |
|---|---|---|---|
| CNC Kitchen, extrusion temperature (blog, about 2020) | PLA | 40/60 = **0.67** at the best temperature | Prusa MK3; reference is a lying specimen |
| same | PETG | 32/55 = **0.58** at the best temperature | |
| CNC Kitchen, heated chamber | ABS (eSUN, 250 °C nozzle) | **0.54** open / **0.59** at about 50 °C / **0.74** at 65 °C | 0.2 mm layers, 2 perimeters, 100% infill |
| same | PC blend (Prusament, 275 °C) | **0.65 / 0.72 / 0.77** | same |
| Allum et al. 2020 | PLA, single-wall | true-area Z/F = 59.1/63.9 = **0.92**; load per mass Z/F = **0.71**; bonded area 64.8% vs 94.5% | 210 °C, 60 °C bed, 5 layer heights × 5 widths |
| Adisa et al. 2023 | PC, single-road box | Z vs compression-moulded 59.7 MPa: **0.50–1.00** | see Table 3.3 |
| Coogan & Kazmer 2017 (RPJ 23(2)) | ABS | bond strength **0.40–0.85** of filament strength | processing conditions varied |
| Prusament PLA TDS | PLA | "interlayer adhesion" 17±3 MPa vs filament 57 MPa = **0.30** | Prusa's own single-wall method; their "vertical xz" bar is on edge, not Z |
| Ultimaker forum (anecdotal) | ABS | 19/38 = **0.50** | ASTM D3039 |
| CSU Sacramento thesis (via search) | PLA / ABS / PETG | XY stronger than Z by 25% / 42% / 128%, so ratio **0.80 / 0.70 / 0.44** | [E]: not read in full |

### 2.3 Summary per material (for the engine)

The "typical" range is what a decent desktop print gives. "Best" is tuned, solid and annealed vendor data.

| Material | S_z/S_xy typical | S_z/S_xy best | E_z/E_xy | Best nozzle range for Z (measured) | Main sources |
|---|---|---|---|---|---|
| PLA | 0.55–0.75 | 0.90 | 0.80–0.91 | 220–240 °C (peak 230–240; drop above about 250) | Bambu, Polymaker, CNC Kitchen, Rivera-López 2024, Allum 2020, Kuznetsov 2018 |
| PETG | 0.5–0.7 | 0.84 | 0.77–0.85 | 240–250 °C (peak 245; 260 already −25%) | Bambu, Polymaker, CNC Kitchen |
| ABS | 0.5–0.6 (open) / 0.7–0.8 (enclosed) | 0.89–0.95 | 0.84–0.89 | 240–260 °C; +20 °C gives +15 to 38% fracture energy | Bambu, Polymaker, Stratasys, CNC Kitchen, Aliheidari 2017/2018 |
| ASA | 0.6–0.75 | 0.84 | 0.83–0.87 | 250–260 °C [E: by analogy with ABS] | Bambu, Polymaker |
| PC | 0.5–0.65 (open) / 0.75 (enclosed) | 0.76 (up to 1.0 at short layer time) | 0.69 (Bambu); about 1.0 (Adisa) | 260–280 °C (no clear trend in 240–280 for Adisa) | Bambu, Polymaker, CNC Kitchen, Adisa 2023, Bartolai 2016 |
| PA (unfilled CoPA / PA12) | 0.6–0.7 dry; higher when conditioned | 0.86 (conditioned) | 0.67 (conditioned) | as hot as allowed: +40 °C gave 3–4× tear energy (Vaes 2021) | Polymaker, Vaes 2021 |
| PA-CF / PA-GF | 0.45–0.65 dry | 0.73–0.79 | 0.49–0.58 dry | 280–300 °C | Bambu, Polymaker |
| PET-CF / PETG-CF | 0.45–0.65 | 0.64 | 0.46–0.54 | 255–270 °C | Bambu |
| PEEK (open or cool chamber) | 0.15–0.25 | – | n/a | 380–420 °C; needs chamber or local heating | secondary sources |
| PEEK / PEKK (heated chamber or top heater) | 0.4–0.8 [E] | about 1 for slow-crystallising PEKK [E] | n/a | | Rodzeń 2021, 2025 |
| PEI (ULTEM 9085), industrial | 0.52–0.75 | 0.75 | n/a | | Stratasys, Markforged |
| TPU 95A | 0.7–0.8 | 0.78 | 0.85 | 220–240 °C | Bambu |

---

## 3. Measured temperature, time and geometry curves

### 3.1 Nozzle temperature

**PLA, CNC Kitchen** (blog, Prusa MK3, dasFilament; standing mini tensile bars; lying reference about 60 MPa) [M]:

| T nozzle °C | 190 | 200 | 210 | 230 | 250 | 270 |
|---|---|---|---|---|---|---|
| S_z MPa | 20* | 37 | 39 | 40 | 37 | 32 |
| relative to peak | 0.50 | 0.93 | 0.98 | 1.00 | 0.93 | 0.80 |

\* The 190 °C batch was under-extruded. The article says the steps were 20 K, but the temperatures listed do not all
fit that; the second batch was mass-matched to ±1%.

**PETG, CNC Kitchen** (lying reference 55 MPa) [M]:

| T °C | 200 | 215 | 230 | 245 | 260 |
|---|---|---|---|---|---|
| S_z MPa | 18 | 22 | 30 | 32 | 24 |
| relative to peak | 0.56 | 0.69 | 0.94 | 1.00 | 0.75 |

CNC Kitchen suspects that the loss at high temperature comes from moisture or bubbles at the interface, not from
degradation of the polymer.

**PLA, Rivera-López et al. 2024**, *Polymers* 16(13):1867, DOI 10.3390/polym16131867. CR-10 S4 printer, 0.2 mm
layers, 0.4 mm lines. The "transversal" specimens fail between rasters, which approximates interface failure but is
not upright Z [M]:

| T °C | 180 | 200 | 220 | 240 | 260 |
|---|---|---|---|---|---|
| S transversal MPa | 18.86 | 29.11 | 32.73 | 35.74 | 35.30 |
| S horizontal MPa | 45.50 | 58.65 | 57.64 | 57.02 | 51.71 |
| ratio | 0.41 | 0.50 | 0.57 | 0.63 | 0.68 |
| E transversal / E horizontal | 0.96 | 0.93 | 0.81 | 0.87 | 0.95 |

**ABS, Aliheidari et al.** (Polymer Testing 2017; Materials & Design 156 (2018) 351–361). Double cantilever beam
interlayer fracture [M]:
- J_c rose from 3391 to 3908 J/m² (+15%) between 220 and 240 °C (thesis figures).
- The journal abstract reports +38% J_c for +20 °C.
- The neck (bonded) area grew by 23%.
- Bed temperature from 85 to 105 °C: +7.6% (3528 to 3799 J/m²).
- The effect levels off at high temperature: full fusion is reached.
- The maximum, 4017 J/m², is close to bulk ABS.
- Since strength scales about as √G, +15 to 38% in G means about +7 to 17% in strength [E].

**Nylon copolymer, Vaes et al. 2021**, *Polymers* 13:2677, DOI 10.3390/polym13162677 [M]:
- Raising the liquefier temperature by 40 °C (220 to 260 °C) gave 3–4× the trouser-tear energy.
- Print speed (5.5 vs 11 mm/s) had a negligible effect.
- A bed at 110 °C instead of 40 °C gave about 1.5× crystallinity and larger spherulites, which they link to a
  weaker interface.

**PC, Adisa et al. 2023**, *Polym. Eng. Sci.*, DOI 10.1002/pen.26495. Single-road boxes, 0.2 mm layers, 0.628 mm
roads, 90 °C bed [M]:

| Condition | T °C | v mm/s | layer time s | S_z MPa | E_z MPa |
|---|---|---|---|---|---|
| 2 | 240 | 30 | 8.09 | 40.1±6.2 | 1872 |
| 1 (base) | 260 | 30 | 8.09 | 37.7±7.9 | 1870 |
| 3 | 280 | 30 | 8.09 | 41.4±8.6 | 2055 |
| 5 | 260 | 90 | 3.49 | **59.7±14.8** (= bulk) | 2284 |
| 6 | 260 | 30 | 20.09 | 42.7±3.5 | 1892 |
| 4 | 260 | 12.5 | 23.54 | 33.2±4.7 | 1837 |
| 7 | 260 | 30 | 60.35 | **30.1±8.1** | 1906 |
| compression moulded | – | – | – | 59.7±2.1 | 1749 |

The values for conditions 1 to 4, 6 and 7 use the true weld thickness measured by microscopy; only condition 5
was also reported against a caliper thickness and assumed full contact (41.0 and 41.8 MPa respectively). Contact
ratio was about 70–81%. The modulus did not depend on layer time.

### 3.2 Layer time and cooling

- **PC (Adisa 2023)** [M]: 8 s and 20 s gave the same strength within scatter; 60 s gave −20 to −30%; 3.5 s at high
  speed gave bulk strength.
- **PLA (Allum 2020)** [M]: layer time about 10–11 s ±50% and print speed had no effect on interlayer strength. The
  authors warn that much longer industrial layer times may behave differently.
- **PLA, layer-building time 30, 90, 150 and 210 s** (ResearchGate study 378002551) [M, qualitative]: adhesion
  falls as layer time grows. No numbers were extracted.
- **PLA, interlayer time and deposition speed** (Int. J. Adv. Manuf. Technol. 2024, DOI 10.1007/s00170-024-14111-8)
  [M, partial]: peak strength and elongation fall with interlayer time, to about 21 MPa at 105 s. Stiffness was
  largely unchanged.
- **Fan**: a COBEM 2021 study on PLA and PETG found the interlayer yield strength about **40% lower with the fan on
  than off** [M, abstract]. An ABS study found that both fan speed (0–100%) and layer time affected Z strength.
  Note that the vendor specimens in §2.1 were printed with the fan at 100% for PLA and still reach 0.8–0.9 after
  annealing.
- **Print speed**: the evidence is contradictory. Abbott et al. 2018 (ABS) found higher speed gave lower strength.
  Coogan & Kazmer and Adisa (PC) found higher speed gave higher strength through shorter layer time.
  Seppala 2017 and Vaes 2021 found a weak effect. Speed matters through the thermal history, so the engine should
  use layer time and temperature, not speed itself.

### 3.3 Chamber, bed and environment

- **ABS (CNC Kitchen)** [M]: chamber at 26 °C gives 21 MPa, about 50 °C gives 23 MPa, 65 °C gives 29 MPa
  (flat reference 39 MPa).
- **PC blend (CNC Kitchen)** [M]: 65%, 72% and 77% of flat at the same three chamber temperatures.
- **ABS, Kumrai-Woodruff & Wang 2020** (ASME, Durham) [M, abstract]: UTS up to +104% against an open enclosure,
  with only a weak positive trend with air temperature.
- **ABS bed temperature (Aliheidari)** [M]: 85 to 105 °C gives +7.6% J_c.
- **Semicrystalline polymers**: a hotter bed or chamber speeds up crystallisation, which can *lower* the bond (Vaes
  2021: nylon; GF/PEEK study), unless it is hot enough to keep the interface amorphous long enough, or above the
  crystallisation range (CF/PEEK at 230 °C, ×5).

### 3.4 Layer height and line width (geometry)

**Kuznetsov et al. 2018**, *Polymers* 10:313, DOI 10.3390/polym10030313. PLA tubes in 3-point bending across the
layers, 25 mm/s [M]. Failure moves from bulk to interlayer at about w/h < 1.6. The paper fits
UFS = UFS_max − 58.19 / r^1.5, with r = nozzle/layer, R² = 0.954. Bulk PLA flexural strength is 83–108 MPa.

| Nozzle mm | Layer mm | UFS MPa |
|---|---|---|
| 0.4 | 0.06 / 0.1 / 0.2 / 0.3 / 0.35 / 0.4 | 67.8 / 59.9 / 50.8 / 38.5 / 25.6 / 17.5 |
| 0.6 | 0.1 / 0.2 / 0.3 / 0.4 / 0.5 / 0.6 | 68.0 / 60.4 / 50.5 / 39.6 / 20.2 / 18.2 |
| 0.8 | 0.15 / 0.2 / 0.4 / 0.6 | 70.7 / 66.8 / 48.1 / 31.3 |

**Allum et al. 2020**, *Additive Manufacturing* 34:101297, DOI 10.1016/j.addma.2020.101297 [M]:
- Z strength based on the true area was 54.6–65.6 MPa over 5 layer heights, and 51.2–67.8 MPa over 5 line
  widths. This is bulk level.
- Load capacity per mass was 28.7% lower for Z.
- The bonded area fraction was 64.8% for Z against 94.5% for F (layer 0.3 mm, width 0.5 mm).
- The capacity fell by 37% as the line width went from 0.45 mm down to the smallest width tested (number not
  extracted).
- Z toughness was 35.6% of F.

---

## 4. Published models (formulas and constants)

### 4.1 Isothermal healing (reptation)

This is the healing theory of Wool & O'Connor 1981 and Kim & Wool 1983, built on de Gennes' reptation. These are
standard results, quoted from the literature summaries above.

- Degree of healing: D_h(t) = σ(t)/σ∞ = (t / t_r)^(1/4) for t < t_r, and 1 after that. Here t_r is the
  reptation or welding time.
- Fracture energy: G_c(t)/G_c∞ = D_h² = (t/t_r)^(1/2).
- The interpenetration depth also grows as t^(1/4) until it reaches about R_g.

### 4.2 Non-isothermal healing

Yang & Pitchumani 2002 (*Macromolecules* 35, DOI 10.1021/ma010858o) and Bastien & Gillespie 1991. The usual form
integrates the reciprocal reptation time over the temperature history:

```
D_h(t) = [ ∫_0^t  dt' / t_r(T(t')) ]^(1/4)
t_r(T) = t_r(T_ref) · a_T(T)
WLF:       log10 a_T = −C1 (T − T_ref) / (C2 + T − T_ref)
Arrhenius: a_T = exp[ (E_a/R) (1/T − 1/T_ref) ]
```

The integration stops at Tg for amorphous polymers. For semicrystalline polymers it stops at the onset of
crystallisation, or at Tg if crystallisation is ignored, as Vaes 2021 did.

**Equivalent isothermal weld time** (Seppala et al. 2017, *Soft Matter* 13:6761, DOI 10.1039/C7SM00950J, on ABS):
t_weld = ∫ dt / a_T(T(t)) over the measured interface history above Tg. At T_ref = 230 °C, the printing conditions
in that study gave t_weld from 0.1 to 100 ms. Fracture energy (mode III trouser tear) correlated with t_weld.
Nozzle temperature had a strong effect and speed a weak one. Vaes 2021 applied the same method to nylon.

**Bartolai, Simpson & Xie** (SFF 2016; *Rapid Prototyping J.* 24(2) 2018, DOI 10.1108/RPJ-02-2017-0026):
- Weld strength: σ_weld = σ_UTS · (t_weld / τ̄_rep)^(1/4), capped at σ_UTS.
- τ̄_rep is the reptation time averaged over the measured IR interface history.
- Part strength by rule of mixtures: σ_part = f_per·σ_UTS + f_weld·σ_weld, with the void area set to zero strength.
- Measured constants for their **PC** [M]:
  - WLF C1 = 4.29, C2 = 87.7 K, T_ref = 200 °C.
  - τ_rep(200 °C) = 0.1 s, from the G'/G'' crossover at 10 rad/s.
  - τ̄_rep = 1.75 s, which is much shorter than t_weld = 32.5 s, so the welds were fully healed.
  - f_per = 0.23 and f_weld = 0.37.
- Result: predicted 39 MPa against measured 35.4 ± 2.93 MPa, with bulk UTS 65 MPa.

**Coogan & Kazmer 2017** (*Rapid Prototyping J.* 23(3):551–561, DOI 10.1108/RPJ-03-2016-0051):
- 1-D transient heat conduction at the interface plus a temperature-dependent diffusion model, integrated to give
  bond strength.
- Uses only rheology and process parameters.
- R² = 0.795 against the strength of hollow ABS boxes.
- Companion paper: bond strength is 40–85% of filament strength, with wetting, intimate contact and diffusion as
  separate factors.
- A 2020 follow-up (*Additive Manufacturing* 35) adds a contact-pressure model (2863 layers, 21 conditions).
  Its supplementary data is on Mendeley (z3yfkxvkz5).

**McIlroy & Olmsted 2017** (*Polymer* 123:376–391; arXiv 1703.09295). Model based on PC [M: their parameters]:
- The interface stays above Tg for only about 1 s: t_g ≈ 1 s at the weld.
- The interpenetration depth χ_W is larger than R_g, so the weld thickness is not the limit.
- The flow in the nozzle disentangles the chains, and the weld ends with an entanglement fraction ν_W < 1. This
  sets the strength ceiling:
  - G_c ∼ 1 − M_e/(q M_w) with q ≈ 0.6.
  - ν_W = 1 − (1 − ν_dep) exp(−∫ dt/τ_d(T)).
- The weld becomes fully entangled only for T_N ≥ 300 °C (PC, Z_eq 22–37).
- PC parameters: Tg 140 °C; WLF C1 = 3, C2 = 160 K at T0 = 260 °C; τ_d = 0.033 s and τ_R = 0.00057 s at
  T_N = 250 °C; M_w 60 kDa; M_e 1.6 kDa; α = 0.14 mm²/s.

**Other calibrations:**
- ABS, Arrhenius degree-of-bonding model from a UBC thesis: E_a = 61.74 kJ/mol, with a 95 °C floor, 3.94% mean
  error [M, thesis, not peer-reviewed].
- Sun, Rizvi, Bellehumeur & Gu 2008 (*Rapid Prototyping J.* 14(2):72–80, DOI 10.1108/13552540810862028), ABS:
  envelope temperature and convection coefficient dominate the cooling and the bond. Sintering or neck growth
  matters only while T is above a critical temperature. No constants were extracted.

### 4.3 Geometry models

- Bartolai (rule of mixtures with measured weld height), and Allum (load-bearing area fraction plus strain
  concentration at the grooves; FEA).
- An obround (stadium) bead of width w and height h has a flat contact of w − h. Measured necks are wider than that,
  because the melt is pressed into the layer below.

---

## 5. Proposed engine model

This is the note's own design. Every constant is [E] unless a measured source is named for it. It has two levels:
- **Level A**: algebraic, per region or per layer. Use it now.
- **Level B**: physics-based, per interface, from a thermal history. Use it later, when the engine has a layer
  thermal model; it is GPU-friendly.

Both return the same multipliers, so Level B can replace Level A factor by factor.

### 5.1 Structure

```
S_z  = S_bulk · min(1, f_heal · f_ent) · φ_geo · f_hot
S_xy = S_bulk · φ_xy                         (φ_xy ≈ 0.93–0.97 for solid regions; fibre grades use vendor S_xy)
E_z  = E_xy · r_E(mat) · (0.85 + 0.15 · min(1, f_heal))
ε_f,z = ε_f,xy · r_ε(mat) · f_heal          (brittle Z failure; r_ε ≈ 0.4–0.6)
```

For the first version, the simpler normalised form is easier to calibrate, because it is anchored to the vendor
ratio:

```
S_z / S_xy = clamp( r_ref(mat) · M_T(T_noz) · M_t(t_layer, fan) · M_env(T_ch) · M_geo(h/w) / M_geo(h_ref/w_ref) ,
                    r_min(mat), r_max(mat) )
```

- `r_ref` is the material's ratio at its reference settings (§5.2), which are the vendor's specimen settings:
  0.2 mm layer, about 0.42 mm line, recommended temperature, solid specimen.
- Every M is 1 at those reference settings.

### 5.2 Constants per material

All constants are [E], derived from the measured data named in the last column.

| Material | r_ref | r_min | r_max | T_low °C | T_opt °C | p | T_hot °C | β 1/°C | r_E | Tg °C | Support |
|---|---|---|---|---|---|---|---|---|---|---|---|
| PLA | 0.80 | 0.20 | 0.95 | 175 | 230 | 0.25 | 245 | 0.008 | 0.85 | 60 | Bambu/Polymaker ratio 0.77–0.90; T curve fits CNC Kitchen and Rivera-López (error ≤ 0.1, see §5.3) |
| PETG | 0.75 | 0.25 | 0.90 | 190 | 245 | 0.35 | 248 | 0.02 | 0.80 | 80 | Bambu 0.68–0.81, Polymaker 0.84; T curve fits CNC Kitchen PETG (error ≤ 0.07) |
| ABS | 0.70 (enclosed) | 0.35 | 0.95 | 215 | 255 | 0.30 | 275 | 0.01 | 0.87 | 105 | Bambu 0.78, Polymaker 0.76–0.89, CNC Kitchen 0.54–0.74; Aliheidari +7–17% strength for +20 °C |
| ASA | 0.72 | 0.35 | 0.90 | 225 | 260 | 0.30 | 280 | 0.01 | 0.85 | 100 | Bambu 0.84, Polymaker 0.73; T values by analogy with ABS |
| PC | 0.65 | 0.30 | 1.00 | 245 | 275 | 0.30 | 300 | 0.01 | 0.75 | 145 | Bambu 0.62, Polymaker 0.76, CNC Kitchen 0.65–0.77, Adisa 0.50–1.0 |
| PA (unfilled) | 0.65 dry | 0.30 | 0.90 | 230 | 270 | 0.5 | 300 | 0.01 | 0.70 | 50–70 | Polymaker CoPA 0.65 dry / 0.86 conditioned; Vaes 3–4× G for +40 °C (p is larger because crystallisation also stops diffusion) |
| PA-CF / PA-GF | 0.55 | 0.30 | 0.80 | 260 | 290 | 0.5 | 310 | 0.01 | 0.55 | 50–70 | Bambu 0.47–0.73, Polymaker 0.57–0.79 |
| PET-CF / PETG-CF | 0.55 | 0.30 | 0.70 | 235 | 265 | 0.35 | 285 | 0.015 | 0.50 | 80 | Bambu 0.47–0.64 |
| PEEK (chamber < Tg) | 0.20 | 0.07 | 0.30 | 360 | 410 | 0.3 | 440 | 0.01 | 0.7 | 143 | secondary sources 0.15–0.25; Rodzeń 2021 (6.96 MPa unheated) |
| PEEK (chamber ≥ 200 °C) | 0.5 | 0.2 | 0.9 | 360 | 410 | 0.3 | 440 | 0.01 | 0.8 | 143 | Rodzeń 2021 ×5; wide uncertainty |
| PEI (9085) | 0.60 | 0.35 | 0.80 | 340 | 370 | 0.3 | 400 | 0.01 | 0.8 | 186 | Stratasys 0.58–0.75, Markforged 0.52 |
| TPU | 0.75 | 0.4 | 0.85 | 205 | 230 | 0.25 | 250 | 0.01 | 0.85 | – | Bambu 0.78 |

### 5.3 The multipliers

**Nozzle temperature.** This is a healing-shaped rise followed by an over-temperature loss:

```
M_T = [ clamp((T − T_low)/(T_opt − T_low), 0, 1) ]^p · max(0, 1 − β·max(0, T − T_hot))
```

The exponent p = 1/4 matches the healing law if the effective weld time grows about linearly with the superheat.

Fit check against measured relative strength [M] → model [E]:
- PLA, Rivera-López: 180 °C 0.53 → 0.55; 200 °C 0.81 → 0.82; 220 °C 0.92 → 0.95; 240 °C 1.00 → 1.00.
- PLA, CNC Kitchen: 250 °C 0.93 → 0.96; 270 °C 0.80 → 0.80.
- PETG, CNC Kitchen: 200 °C 0.56 → 0.55; 215 °C 0.69 → 0.76; 230 °C 0.94 → 0.89; 260 °C 0.75 → 0.76.
- Known misfits:
  - PLA, CNC Kitchen at 200 °C: measured 0.93, model 0.82. The two PLA datasets disagree by about 0.1.
  - PLA, Rivera-López at 260 °C: measured 0.99, model 0.88 (the over-temperature drop does not appear there).

**Layer time and fan** (amorphous polymers):

```
M_t = 1                                   for t_layer ≤ t_crit
M_t = max(m_floor, 1 − γ · ln(t_layer / t_crit))   otherwise
t_crit = 15 s (no fan) … 8 s (fan 100%),  γ = 0.25,  m_floor = 0.6
M_fan = 1 − k_fan · fan_fraction,  k_fan = 0.10 for PLA (Tg low, vendor PLA strong even at 100% fan),
        0.25 for PETG/ABS/ASA/PC [E: from the COBEM "≈40%" and the CNC Kitchen enclosure data]
```

- These are fitted to Adisa (PC): 60 s against 8–20 s gave 0.70–0.80. With γ = 0.25 and t_crit = 15 s, 60 s gives
  0.65.
- Allum (PLA, 5–16 s) gives no effect, which is consistent because those times are below t_crit.
- Short layer times can also *raise* strength (Adisa: 3.5 s reached bulk). That is not modelled at Level A: r_max
  caps it.

**Chamber or envelope** (amorphous polymers, chamber T_ch below Tg):

```
x = clamp((T_ch − 25)/(Tg − 25), 0, 1)
M_env = (1 + δ·x) / (1 + δ·x_ref)
δ = 0.75 (ABS/ASA),  0.55 (PC),  0.3 (PETG),  0 (PLA; enclosures are not recommended above about 35 °C)
```

- ABS from CNC Kitchen: 0.54 to 0.74 for 25 to 65 °C, so δ ≈ 0.74.
- PC blend: 0.65 to 0.77, so δ ≈ 0.55.
- x_ref is the reference chamber of the vendor data, about 45–50 °C for ABS/ASA/PC.
- For semicrystalline polymers (PA, PEEK, PP), set M_env = 1 and use a separate state per material (see the PEEK
  rows). Vaes and the GF/PEEK study show that a warm chamber can hurt unless it stays above the crystallisation
  window.

**Geometry** (layer height h against line width w):

```
M_geo(h/w) = clamp(1 − κ·(h/w)^1.5, 0.2, 1)
κ = 0.78 for single walls and perimeter-only zones; κ = 0.25 for solid, overlapped infill [E]
```

- The single-wall value (κ = 0.78) comes from Kuznetsov's fit (58.19/r^1.5), normalised to UFS_max ≈ 75 MPa
  [E: UFS_max was not given in the paper].
  - At w = 0.4 mm it predicts 0.95 / 0.90 / 0.72 / 0.49 / 0.36 / 0.22 for h = 0.06–0.4 mm.
  - The measured values, normalised the same way, are 0.90 / 0.80 / 0.68 / 0.51 / 0.34 / 0.23.
  - Allum: h/w = 0.6 predicts 0.64, against a measured bonded-area ratio of 0.69.
- The solid-infill value (κ = 0.25) is a guess. Neighbouring lines fill most of the grooves, and vendor solid bars
  at h/w ≈ 0.5 reach 0.8–0.9.

**Entanglement and molecular weight ceiling.** Set f_ent = 1 for Level A; it is already folded into r_max. McIlroy
& Olmsted suggest that ν_W < 1 below about 300 °C for PC, which is one reason r_max < 1 for most materials.

### 5.4 Level B (physics-based)

For each interface between layers k−1 and k:

1. **Interface temperature history.** Use the engine's layer thermal model (CPU reference, GPU kernel later). A
   lumped first version:
   - Previous-layer surface temperature when the new layer arrives:
     `T_prev = T_env + (T_dep − T_env)·exp(−t_layer/τ_L)`, with `τ_L ≈ ρ c w / (2 h_c)`.
     - h_c ≈ 10 W/m²K with natural convection, 50–150 W/m²K with the fan.
     - For PLA with w = 0.45 mm, τ_L is about 5 s with the fan and about 50 s without [E].
   - Contact temperature, for the same material on both sides (equal effusivity):
     `T_c = (T_noz,eff + T_prev)/2`.
   - Interface decay: `T_i(t) = T_env' + (T_c − T_env')·exp(−t/τ_i)`, with τ_i ≈ 0.3–2 s.
     - McIlroy: about 1 s to reach Tg for PC.
     - T_env' is the chamber temperature raised by the bed near the plate.
   - Add the reheating peaks from layers k+1 and k+2, attenuated by about exp(−n·h/δ_th).
2. **Equivalent weld time:**
   `t_w = ∫_{T_i>T_stop} dt / a_T(T_i(t))`, using WLF or Arrhenius constants per material.
   - T_stop = Tg for amorphous polymers, or the crystallisation onset for semicrystalline ones.
3. **Healing:** `f_heal = min(1, (t_w / τ_rep,ref)^(1/4))`. Optionally use a floor for wetting and intimate
   contact: `f_heal = f_0 + (1 − f_0)·D_h`, with f_0 ≈ 0.1–0.2.
4. **Strength:** `S_z = S_bulk · f_heal · f_ent · M_geo · f_hot`. M_geo can come straight from the engine's bead
   cross-section model (contact width / pitch) instead of the κ formula.

**Constants for Level B:**

| Material | Rheology constants | Source and status |
|---|---|---|
| PC | C1 = 4.29, C2 = 87.7 K, T_ref = 200 °C, τ_rep = 0.1 s | Bartolai [M] |
| PC (alternative) | C1 = 3, C2 = 160 K, T0 = 260 °C, τ_d = 0.033 s at 250 °C | McIlroy [M] |
| ABS | E_a ≈ 62 kJ/mol | UBC thesis [M, provisional] |
| ABS (Seppala) | WLF from their rheology | constants not extracted |
| PLA, PETG, ASA, PA, PEEK | universal WLF referenced to Tg: C1g = 17.44, C2g = 51.6 K | [E] |

For the materials with only universal constants, calibrate τ_rep(T_opt) so that Level B reproduces the Level A
temperature curve in §5.3. Then Level B extrapolates in a physically sound way to layer time, fan, chamber and
part size.

### 5.5 Uncertainty to carry into the FEA

| Item | 1σ band [E] | Basis |
|---|---|---|
| r_ref | ±0.10 (absolute) | scatter between vendors for the same polymer (ABS 0.76–0.89; PLA 0.77–0.90) and between sheet revisions (PAHT-CF 0.51 vs 0.73) |
| M_T | ±0.08 | residuals of the fits above |
| M_t, M_fan, M_env | ±0.15 | few studies, small samples |
| M_geo, single wall | ±0.08 | Kuznetsov fit residual |
| M_geo, solid infill | ±0.15 | guessed, not fitted to data |
| Overall, unfilled | ±25% of the S_z prediction | combination of the above |
| Overall, PEEK/PEI and semicrystalline in hot chambers | ±50% | contradictory studies |

**Recommendations for the FEA:**
- Use the lower bound (mean − 1σ), clamped at r_min, for safety factors.
- Give Z a brittle failure criterion (maximum normal stress across the layer, or a cohesive interface), not a
  ductile one.
- Moisture: dry vs conditioned nylon changes the Z/XY ratio from 0.64 to 0.79 for PA6-CF. Expose a "conditioned"
  flag for PA.

### 5.6 What to measure in-house (black-box) to tighten the model

- Upright ISO 527 bars, or the Allum-style single-wall Z coupons, for each material at:
  - 4 nozzle temperatures;
  - 3 layer times (5, 20 and 60 s);
  - fan at 0 and 100%;
  - 2 values of h/w.
- Weigh the coupons and measure the fracture area, so that the geometry factor can be separated from bond quality.
- Running other slicers' output on the same printer, as a black box, is allowed for comparison.

---

## 6. Sources

**Peer-reviewed and theses**

- Allum J., Moetazedian A., Gleadall A., Silberschmidt V.V. (2020). Interlayer bonding has bulk-material strength in
  extrusion additive manufacturing: new understanding of anisotropy. *Additive Manufacturing* 34:101297.
  https://doi.org/10.1016/j.addma.2020.101297. Author version:
  http://fullcontrolgcode.com/wp-content/uploads/2020/07/Author-version-Interlayer-bonding-has-bulk-material-strength-in-extrusion-additive-manufacturing-New-understanding-of-anisotropy.pdf
- Adisa A.O., Colon A.R., Kazmer D.O., Peterson A.M. (2023). Interrelationships between process parameters,
  cross-sectional geometry, fracture behavior, and mechanical properties in material extrusion additive
  manufacturing. *Polym. Eng. Sci.* https://doi.org/10.1002/pen.26495 (https://par.nsf.gov/servlets/purl/10552493)
- Bartolai J., Simpson T.W., Xie R. (2016). Predicting strength of thermoplastic polymer parts produced using
  additive manufacturing. SFF Symposium.
  https://utw10945.utweb.utexas.edu/sites/default/files/2016/077-Bartolai.pdf. Journal version: *RPJ* 24(2) 2018,
  https://doi.org/10.1108/RPJ-02-2017-0026
- McIlroy C., Olmsted P.D. (2017). Disentanglement effects on welding behaviour of polymer melts during the
  fused-filament-fabrication method for additive manufacturing. *Polymer* 123:376–391.
  https://arxiv.org/abs/1703.09295
- Seppala J.E., Han S.H., Hillgartner K.E., Davis C.S., Migler K.B. (2017). Weld formation during material extrusion
  additive manufacturing. *Soft Matter* 13:6761–6769. https://doi.org/10.1039/C7SM00950J (abstract only:
  https://www.nist.gov/publications/weld-formation-during-material-extrusion-additive-manufacturing)
- Vaes D., Coppens M., Goderis B., Zoetelief W., Van Puyvelde P. (2021). The extent of interlayer bond strength
  during FFF of nylon copolymers. *Polymers* 13:2677. https://doi.org/10.3390/polym13162677
- Kuznetsov V.E., Solonin A.N., Urzhumtsev O.D., Schilling R., Tavitov A.G. (2018). Strength of PLA components
  fabricated with FDM as a function of geometrical parameters. *Polymers* 10:313.
  https://doi.org/10.3390/polym10030313 (https://pmc.ncbi.nlm.nih.gov/articles/PMC6414919/)
- Rivera-López F. et al. (2024). Effects of nozzle temperature on mechanical properties of PLA specimens fabricated
  by FDM. *Polymers* 16:1867. https://doi.org/10.3390/polym16131867
- Coogan T.J., Kazmer D.O. (2017). Healing simulation for bond strength prediction of FDM. *RPJ* 23(3):551–561.
  https://doi.org/10.1108/RPJ-03-2016-0051 (abstract). Also Bond and part strength in FDM, *RPJ* 23(2),
  https://doi.org/10.1108/RPJ-03-2016-0050. Data for the 2020 follow-up: https://data.mendeley.com/datasets/z3yfkxvkz5
- Sun Q., Rizvi G.M., Bellehumeur C.T., Gu P. (2008). Effect of processing conditions on the bonding quality of FDM
  polymer filaments. *RPJ* 14(2):72–80. https://doi.org/10.1108/13552540810862028 (abstract)
- Aliheidari N. et al. (2017, 2018). Fracture resistance measurement of FDM 3D printed polymers (*Polymer Testing*),
  and Interlayer adhesion and fracture resistance of polymers printed through melt extrusion AM (*Mater. Des.*
  156:351–361). https://digitalcommons.njit.edu/fac_pubs/8329 (abstracts and thesis figures)
- Abbott A.C., Tandon G.P., Bradford R.L., Koerner H., Baur J.W. (2018). Process-structure-property effects on ABS
  bond strength in FFF. *Additive Manufacturing* 19. https://doi.org/10.1016/j.addma.2017.11.002 (abstract)
- Yang F., Pitchumani R. (2002). Healing of thermoplastic polymers at an interface under nonisothermal conditions.
  *Macromolecules* 35. https://doi.org/10.1021/ma010858o (abstract; the formula in §4.2 is the form commonly
  cited)
- Wool R.P., O'Connor K.M. (1981), *J. Appl. Phys.* 52:5953; Kim Y.H., Wool R.P. (1983), *Macromolecules* 16:1115.
  Classic healing theory; not re-read here.
- Kumrai-Woodruff R., Wang Q. (2020). Temperature control to increase inter-layer bonding strength in FDM. ASME.
  https://dro.dur.ac.uk/32366 (abstract)
- Rodzeń K. et al. (2021). Improvement of the layer-layer adhesion in FFF 3D-printed PEEK/carbon fibre composites.
  *Composites Part A*. https://asep.lib.cas.cz/arl-cav/cs/detail-cav_un_epca-0545310-Improvement-of-the-layerlayer-adhesion-in-FFF-3D-printed-PEEKcarbon-fibre-composites/
  (abstract)
- Rodzeń K., O'Donnell E., Hasson F., Boyd A. (2025). Crystallization and microstructure effects on interlayer
  performance and ductility of PEKK in open-chamber FFF. *Mater. Today Commun.* 49:114253.
  https://doi.org/10.1016/j.mtcomm.2025.114253
- Harris M. et al. (2019). Effect of material and process specific factors on the strength of printed parts in FFF:
  a review. https://pmc.ncbi.nlm.nih.gov/articles/PMC6566369/
- Gao X. et al. (2021). Fused filament fabrication of polymer materials: a review of interlayer bond. *Additive
  Manufacturing* 37:101658. https://doi.org/10.1016/j.addma.2020.101658 (not read)
- COBEM 2021 fan-speed study on PLA and PETG: https://abcm.org.br/proceedings/view/COB2021/0310 (abstract, via
  search)
- UBC thesis, ABS degree of bonding with E_a = 61.74 kJ/mol: https://open.library.ubc.ca/collections/24/items/1.0452040
  (via search)

**Manufacturer data sheets** (downloaded and read with pdftotext)

- Bambu Lab data sheets (PLA Basic, Matte, Tough, PLA-CF, PETG Basic, PETG HF, PETG-CF, ABS, ABS-GF, ASA, PC, PC FR,
  PA6-CF, PAHT-CF, PET-CF, TPU 95A), from cdn.shopify.com, e.g.
  https://cdn.shopify.com/s/files/1/0584/7236/6216/files/Bambu_PA6-CF_Technical_Data_Sheet.pdf
- Polymaker data sheets: PolyLite PLA, PETG, ABS, ASA and PC; PC-ABS; PolyMide CoPA, PA6-CF, PA12-CF and PA6-GF.
  For example https://cdn.shopify.com/s/files/1/0548/7299/7945/files/PolyMide_PA6_CF_TDS_V5.2.pdf and
  https://polymaker.com/wp-content/uploads/lana-downloads/ (the V5.4 values come from search snippets).
- Prusament PLA TDS: https://prusament.com/wp-content/uploads/2022/10/PLA_Prusament_TDS_2021_10_EN.pdf
- Stratasys ULTEM 9085 and ABS-M30 (values via search snippets of distributor and Stratasys pages).

**Black-box tests and blogs** (written results)

- CNC Kitchen, The influence of extrusion temperature on layer adhesion:
  https://cnckitchen.com/blog/the-influence-of-extrusion-temperature-on-layer-adhesion
- CNC Kitchen, Does a heated chamber make your 3D prints stronger?:
  https://www.cnckitchen.com/blog/does-a-heated-chamber-make-your-3d-prints-stronger
- Ultimaker community, ABS inter-layer adhesion: https://community.ultimaker.com/topic/38963-abs-inter-layer-adhesion/

**Not reachable in this session:** the ZHAW 2019 paper on improving filament weld strength
(srv-clst-301-data66.zhaw.ch, blocked by the network allowlist), and the full texts behind paywalls (Seppala 2017,
Coogan & Kazmer, Aliheidari, Sun 2008, Yang & Pitchumani).
