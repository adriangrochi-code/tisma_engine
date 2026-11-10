# Intra-layer (bead-to-bead) adhesion in FFF: measured data, models and a proposed engine material model

Research note for the Tisma engine structural FEA. Clean-room: sources are papers, a manufacturer-neutral blog with
published measurements, and public repositories only. No slicer or GPL code was read. All text is summarised in my own
words; numbers are quoted with their source.

**Legend for every number in this note**

| Tag | Meaning |
|-----|---------|
| **[M]** | Measured, taken directly from the cited source (table or text). |
| **[D]** | Derived by me from measured numbers in the cited source (ratio, back-calculation, fit). |
| **[E]** | Estimate or engineering judgement (few or no direct data). Treat as a default with the stated uncertainty. |

Directions used throughout (local bead frame, valid also for non-planar layers):

- **1 (L)**: along the bead (toolpath tangent).
- **2 (T)**: in the layer, across the bead (perpendicular to the toolpath, tangent to the layer surface). This is the
  intra-layer bond direction, the subject of this note.
- **3 (Z)**: across layers (normal to the layer surface, gradient of the layer scalar field).

---

## 1. Key findings (short)

1. Across-bead (T) strength is controlled by two things: the **bonded fraction of the contact** (bond/neck length
   relative to bead height, set mostly by overlap / air gap / extrusion width) and the **bond quality** (degree of
   polymer interdiffusion, set by interface temperature history: nozzle temperature, time to the next neighbour,
   chamber and fan). Fibre strength along the bead is almost unaffected by process settings.
2. The spread is huge and setting-dependent. For ABS the T/L strength ratio ranges from about **0.14 [D]** (zero air
   gap, Ahn 2002) to **0.75 [D]** (negative air gap, Rodriguez 1999). For PLA it ranges from **0.45 [M]** (Casavola
   2016, single layer, default-like settings) to **0.84 [D]** (Song 2017, optimised blocks, 1.5 % porosity).
3. Stiffness is much less anisotropic than strength: E_T/E_L is 0.6–1.0 for unfilled polymers; strength ratios are
   0.15–0.95. Short-carbon-fibre filaments are the exception: E_T/E_L about 0.35–0.4 because fibres align with the
   bead.
4. A **negative air gap (overlap)** is the single most effective lever on across-bead strength (ABS: transverse
   strength went from < 5 MPa to 13.4 MPa when the gap went from 0 to −25.4 µm, Rodriguez 1999 **[M]**; TPU becomes
   nearly isotropic with negative gap, Hohimer 2017 **[M, qualitative]**). Bead width, colour and model temperature had
   little effect on FDM ABS in Ahn 2002's DOE; air gap and raster angle were the only significant factors **[M]**.
5. **Temperature** raises bond quality up to a plateau and then can lower it (degradation / moisture / bubbles). PLA
   interlayer peak near 230 °C, PETG near 245 °C (CNC Kitchen) **[M]**; PC plateau above about 250 °C (Fang 2020)
   **[M]**; ABS fracture toughness of the bead-bead interface rises with nozzle temperature up to the default 270 °C then
   levels off (Rodriguez 1999) **[M]**.
6. **Time to the neighbour bead** matters for intra-layer bonds: long raster lines mean the previous bead is cold when
   the next arrives. Ai & Vogt 2022 found print-path effects only when the time between adjacent roads was about 5 s
   or less **[M]**. Measured intra-layer bond lengths in one ABS part varied by a factor of about 3 between interfaces
   (0.19–0.61 mm on a 0.8 × 0.7 mm bead) (Kapusuzoglu et al.) **[M]**.
7. Neck growth (Frenkel / Pokluda / Bellehumeur; cylindrical version by Gurrala) and reptation healing (Wool;
   non-isothermal integral) are the standard physics models. They are good for trends, but in practice they need
   calibration: Kapusuzoglu et al. needed a discrepancy model, and Rodriguez found the diffusion term over-predicted.

---

## 2. Measured data: along vs across the bead

### 2.1 Single-layer and unidirectional tests (the most direct intra-layer data)

| Material | Source | Specimen / settings | L (0°) UTS, E | T (90°) UTS, E | Ratio σ_T/σ_L, E_T/E_L | Tag |
|---|---|---|---|---|---|---|
| PLA | Casavola et al. 2016 | **Single layer**, 0.35 mm layer, 0.70 mm bead, air gap 0, 200 °C nozzle, 2 contours | 50.23 MPa, 3.12 GPa | 22.49 ± 6.75 MPa, 2.77 GPa | 0.45, 0.89 | [M] / ratio [D] |
| PLA | Casavola et al. 2016 | filament ("wire") reference | 55.26 MPa, 3.38 GPa | — | — | [M] |
| PLA | Casavola et al. 2016 | single layer 45° | 40.68 MPa, 2.86 GPa | — | — | [M] |
| PLA | Casavola et al. 2016 | in-plane shear modulus G12 (ASTM D3518) | G12 = 1.25 GPa | | | [M] |
| PLA | Song et al. 2017 | Machined from unidirectional blocks; 220 °C, 0.2 mm, 60 mm/s; porosity 1.5 % | 54.89 MPa, 3.98 GPa (2.5e-4 1/s) | 46.24 MPa, 4.04 GPa | 0.84, 1.02 | [M] / [D] |
| PLA | Song et al. 2017 | same, 45° | 61.42 MPa | | | [M] |
| PLA | Song et al. 2017 | same, slow rate 1.25e-5 1/s | 34.58 MPa | 38.51 MPa | ≈1.1 | [M] |
| PLA | Song et al. 2017 | lamina constants | E11 3.98, E22 4.04 GPa, ν12 0.34, ν23 0.37, G12 1.50, G23 1.47 GPa | | | [M] |
| PLA | Song et al. 2017 | fracture toughness K_IC | 5.05 MPa√m (crack across beads) | 4.06 MPa√m (crack along beads) | 0.80 | [M] |
| PLA | Song et al. 2017 | Hill coefficients (tension), F/H = 1.47, G = H = 0.5, N = 1.27 | | | | [M] |
| ABS | Casavola et al. 2016 | **Single layer**, same settings, 225 °C | 26.11 MPa, 1.79 GPa | 6.71 MPa, 1.15 GPa | 0.26, 0.64 | [M] / [D] |
| ABS | Casavola et al. 2016 | filament reference; G12 | 32.11 MPa, 1.81 GPa; G12 = 0.81 GPa | | | [M] |
| ABS P400 | Rodriguez, Thomas, Renaud 1999 (SFF) | Unidirectional, aligned, gap 0 | 21.5 MPa | < 5 MPa | < 0.23 | [M] / [D] |
| ABS P400 | Rodriguez et al. 1999 | aligned, gap −25.4 µm | 24.4 MPa, 1.97 GPa | 13.4 MPa, 1.77 GPa | 0.55, 0.90 | [M] / [D] |
| ABS P400 | Rodriguez et al. 1999 | aligned, −25.4 µm, annealed | 22.9 MPa | 18.5 MPa | 0.81 | [M] / [D] |
| ABS P400 | Rodriguez et al. 1999 | skewed, gap +76.2 µm | 17.9 MPa, 1.81 GPa | 13.4 MPa, 1.40 GPa | 0.75, 0.77 | [M] / [D] |
| ABS P400 | Rodriguez et al. 1999 | bulk monofilament | 31.2 MPa, 2.23 GPa | | | [M] |
| ABS P400 | Ziemian et al. 2012 | 0.305 mm road, 0.178 mm slice, gap 0, 320 °C liquefier, 80 °C envelope | 25.72 MPa, 0.99 GPa | 14.56 MPa, 0.74 GPa | 0.57, 0.75 | [M] / [D] |
| ABS P400 | Ziemian et al. 2014 (SFF) | same machine family, 15 layers, gap 0 | 23.08 MPa, 1.82 GPa | 8.37 MPa, 1.10 GPa | 0.36, 0.61 | [M] / [D] |
| ABS P400 | Ziemian et al. 2014 | angles 15/30/45/60/75° | 15.82 / 13.99 / 10.61 / 8.50 / 7.97 MPa | | | [M] |
| ABS P400 | Ziemian et al. 2014 | single fibre / injection moulded | 27.74 / 27.00 MPa | | | [M] |
| ABS P400 | Ahn et al. 2002 | Air gap 0; injection-moulded reference 26 MPa | ≈ 73 % of IM ≈ 19 MPa | ≈ 10 % of IM ≈ 2.6 MPa | ≈ 0.14 | [D] from text |
| ABS P400 | Ahn et al. 2002 | Air gap −0.003 in (−76 µm) | axial "not increased much" | "large increase" | — | [M, qualitative] |
| PETG | Popa, Galatanu, Marsavina (Frattura ed Integrità Strutturale) | 0.2 mm nozzle, 100 % infill, contoured | 55.26 MPa | 52.27 MPa | 0.95 | [M] / [D] |
| PETG | same | uncontoured | 55.21 MPa | 48.89 MPa | 0.89 | [M] / [D] |
| PETG-CF | Science of Sintering 2023 (carbon-reinforced PETG) | | 52.2 MPa, 4.75 GPa | 25.4 MPa, 1.57 GPa | 0.49, 0.33 | [M] / [D] |
| PA-CF (Onyx) | Canegrati et al. 2022 (ECCM20) | Compression moduli: along bead (prism) vs in-plane transverse (hollow prism/cylinder) | 3.13–3.37 GPa; σ_c 108 MPa | 1.16–1.20 GPa | E: 0.35–0.38 | [M] / [D] |
| PA-CF (Onyx) | Canegrati et al. 2022 | Tension 0/90 laminate vs ±45 | 0/90: 23.7 MPa, 0.97 GPa; ±45: 29 MPa, 0.52 GPa | | | [M] |
| TPU | Czekanski & Etiwa 2025 (CSME) | planar tension | ≈ 20 MPa, 230 % strain | ≈ 5 MPa, 50–60 % strain | ≈ 0.25 | [M] / [D] |
| TPU | Hohimer et al. 2017 (SPIE) | negative air gap | | | ≈ 1 (nearly isotropic) | [M, qualitative] |
| TPU | Hohimer et al. 2017 | positive air gap | | | strongly anisotropic | [M, qualitative] |

Notes on interpretation:

- Stresses in most of these papers are on the **apparent** (gross) cross-section, so porosity is included.
- Casavola's single-layer specimens are the cleanest *intra-layer* test: one layer, beads all at the same angle, so
  the 90° failure is purely a bead-to-bead bond failure. Their PLA/ABS contrast (0.45 vs 0.26) reflects that PLA wets
  and bonds better at its usual settings.
- Song 2017 shows that with optimised printing and machining off the surface, PLA can be close to isotropic in plane.
  It is an upper bound, not a default.
- Ziemian 2014 (ABS) shows the angle curve is flat from 60° to 90°: once more than about 60° off-axis, the bond
  controls the failure.
- The +76 µm "skewed" ABS case of Rodriguez shows that a staggered (skewed) bead stacking can keep transverse
  strength even with a positive gap, because beads of the next layer fill the gaps.

### 2.2 Laminates (alternating layers), for validation of a layer-wise model

| Material | Source | Layup | UTS | E | Tag |
|---|---|---|---|---|---|
| ABS | Ziemian 2014 | ±45 / ±15/−75 / ±30/−60 | 15.71 / 14.39 / 14.67 MPa | 1.36 / 1.52 / 1.44 GPa | [M] |
| ABS | Ziemian 2012 | ±45 (default) | 19.36 MPa | 0.77 GPa | [M] |
| ABS | Ahn 2002 | 0/90 and ±45 with −0.003 in gap | 65–72 % of IM ABS (26 MPa) | — | [M] |
| ABS | Li et al. 2001 (SFF) | [0/90], [15/−75], [30/−60], [45/−45], gap −25.4 µm | — | 1773 / 1623 / 1440 / 1370 MPa (laminate model within 3–7 %) | [M] |
| ABS, PLA | Casavola 2016 | [+30/−30/0/−30/+30] | ABS 32.18, PLA 55.37 MPa | ABS 1.86, PLA 3.35 GPa (CLT within 1–5 %) | [M] |
| PLA | Kiendl & Gao 2020 | 0/90 alternation | stiffness and strength nearly isotropic in plane, toughness strongly anisotropic | | [M, qualitative] |
| PC | Cantrell et al. 2017 | flat, various rasters | moduli and strengths vary up to about 20 % with raster | | [M, qualitative] |
| PC | Hossain et al. (SFF 2013) | 0/90 vs ±45 | max 53.75 MPa vs default 46.84 MPa (≈ +18 % from raster change) | | [M] |

Classical laminate theory (CLT) with per-layer orthotropic constants predicts laminate stiffness within a few percent
(Casavola 2016; Li et al. 2001). This supports a per-voxel orthotropic model with the bead direction as axis 1.

### 2.3 Across layers (direction 3), for completeness

| Material | Source | Result | Tag |
|---|---|---|---|
| PLA | CNC Kitchen (extrusion temperature test) | standing samples 37–40 MPa between 200 and 250 °C vs about 60 MPa lying → 0.62–0.67 | [M] / [D] |
| PETG | CNC Kitchen | 18–32 MPa standing vs 55 MPa lying → 0.33–0.60 | [M] / [D] |
| ABS | Coogan & Kazmer 2017 | bond strength 40–85 % of filament strength depending on settings; along-fibre 95 % | [M] |
| PC | Fang et al. 2020 (JHU/NIST) | thin-wall transverse strength scales linearly with bond width; filament as printed 55.2 MPa vs bulk 71.6 MPa | [M] |
| PLA | Perez, Celik, Karkkainen (PMC9828590) | upright perimeter-only specimens 22.8 MPa vs filament 43.6 MPa; FE-fitted interface strength 33.75 MPa | [M] |

---

## 3. Process effects on the bead-to-bead bond (numbers)

### 3.1 Air gap / overlap / extrusion width

| Source | Material | Setting | Effect | Tag |
|---|---|---|---|---|
| Rodriguez 1999 | ABS | gap 0 → −25.4 µm | transverse UTS < 5 → 13.4 MPa (×2.7 or more) | [M] |
| Rodriguez 2000/1999 | ABS | gap and flow rate | void density 4–16 %, bond-length density 39–73 %; lowest voids (6.8 % aligned, 4.5 % skewed) and highest bond density (73 %, 69 %) at −25.4 µm; extrusion and envelope temperature had little influence on **mesostructure** | [M] |
| Rodriguez 1999 | ABS | max positive gap 152 µm | void 16 %, bond density 39 % | [M] |
| Li et al. 2001 | ABS | gap −50 / −25.4 / 0 / +100 µm | area void density ρ1 = 0.034 / 0.043 / 0.080 / 0.199; linear void fraction ρ2 = 0.28 / 0.34 / 0.40 / 1.0 | [M] |
| Ahn 2002 | ABS | 0 → −76 µm | transverse and crisscross strength "large increase"; axial little change; air gap and raster angle were the only significant DOE factors (bead width, model temperature and colour were not) | [M] |
| Perez et al. (PMC9828590) | PLA | FE study: closing inter-bead gaps | strength +13 to +20 %, stiffness +11 to +16 % (3 to 5 perimeters) | [M, simulation] |
| Hohimer 2017 | TPU | negative vs positive gap | negative → nearly isotropic; positive → strongly raster-dependent | [M] |
| Lendvai et al. 2024 (Prog. Addit. Manuf.) | PLA | extrusion multiplier 0.97–1.05 | strength peaks near 1.03, drops above | [M, from abstract] |
| (secondary, not verified) | ABS | extrusion multiplier above nominal | up to +10 % in X and +50 % in Z, near isotropy | [unverified] |

### 3.2 Extrusion temperature

| Source | Material | Result | Tag |
|---|---|---|---|
| Rodriguez 1999 | ABS | interface fracture toughness K_c of bead-bead bond rises with nozzle temperature 255 → 270 °C, then levels off at 285 °C; rises with envelope temperature by 0.014 MPa√m per °C (base 1.88 MPa√m, so about +0.75 %/°C) | [M] / [D] |
| Rodriguez 1999 | ABS | annealing: K_c 1.88 → 2.48 MPa√m, grows with t^(1/4); transverse strength +38 %, longitudinal −6.7 % (but part distortion) | [M] |
| Rodriguez 1999 | ABS | rate effect on K_c about −0.08 MPa√m per decade of displacement rate | [M] |
| CNC Kitchen | PLA (interlayer) | 200 / 210 / 230 / 250 / 270 °C → 37 / 39 / 40 / 37 / 32 MPa | [M] |
| CNC Kitchen | PETG (interlayer) | 200 / 215 / 230 / 245 / 260 °C → 18 / 22 / 30 / 32 / 24 MPa | [M] |
| Fang et al. 2020 | PC | nozzle 230–290 °C: UTS rises then plateaus above about 250 °C; bond width did not change much, so the loss below 250 °C is bond quality, not geometry | [M] |
| Song 2017 | PLA | porosity vs temperature (0.2 mm, 45 mm/s): 200 °C 5.7–7.5 %, 210 °C 4.7 %, 230 °C 5.8 %, 240 °C 7.3 % | [M] |
| Hohimer 2017 | TPU | nozzle temperature did not significantly change UTS | [M] |
| Kapusuzoglu et al. (J. Manuf. Sci. Eng. 2021; arXiv 2608.18431) | ABS | measured bead temperature right after deposition is 20–50 °C below the set nozzle temperature | [M] |

### 3.3 Print speed and time to the neighbour bead

| Source | Material | Result | Tag |
|---|---|---|---|
| Kapusuzoglu et al. | ABS, 0.8 × 0.7 mm beads | a "bonding frontier": above a line in (nozzle T, speed) space parts debond; e.g. 220 °C at 40 mm/s debonded; useful range 210–260 °C, 15–43 mm/s | [M] |
| Kapusuzoglu et al. | ABS, 240 °C, 42 mm/s | measured intra-layer bond lengths 0.19–0.61 mm across 14 interfaces and 6 layers in one part (bond / bead height 0.27–0.87) | [M] / [D] |
| Ai & Vogt 2022 | PC, PP | print-path and size effects appear only when the time between adjacent roads is ≲ 5 s; effects larger for PC than PP | [M] |
| Song 2017 | PLA | porosity at 220 °C, 0.2 mm: 30 mm/s 4.5 %, 45 mm/s (not in table at 220), 60 mm/s 1.5 %; at 230–240 °C, 75–125 mm/s: 5.4–6.7 % | [M] |
| Fang 2020 | PC | 10–30 mm/s: no significant change of strength (similar bond width), waviness at 30 mm/s | [M] |
| Chacón et al. 2017 | PLA | flat and on-edge specimens: layer height and feed rate have only slight effect; upright: strength rises with layer height, falls with feed rate | [M, qualitative] |
| Rodriguez 1999 | ABS | envelope (chamber) temperature has strong effect on bond toughness (see 3.2) | [M] |
| Sun et al. 2008 | ABS | chamber temperature and convection coefficient strongly affect cooling profile, neck size and bond strength; sintering only acts briefly while above the critical sintering temperature | [M, qualitative] |

### 3.4 Wall (perimeter) count and wall-to-infill bond

| Source | Material | Result | Tag |
|---|---|---|---|
| Coca-Gonzalez et al. 2025 (Front. Mech. Eng.) | Onyx (PA + short CF), 100 % infill, 0/90 core | 1 / 2 / 15 perimeters → UTS 36.9 / 38.8 / 50.4 MPa (means of 3), modulus about 1 GPa, no clear modulus trend; failure starts at the wall/core transition | [M] / means [D] |
| Croccolo et al. (IRF 2013) | ABS | contour count and contour width affect tensile strength strongly; analytical model based on contour count reported within about 4 % | [M, from abstract] |
| UT Austin (Hossain et al.) | PC | removing air gaps between rasters **and between contours and rasters** improved tensile properties | [M, qualitative] |
| Ziemian / Ahn | ABS | contours carry load "axially by the fibres" around the outline (Ahn's build rule 1) | [M, qualitative] |
| HackSpace magazine test; CNC Kitchen (secondary) | PLA | more perimeters gives more strength per gram than more infill | [qualitative, not verified] |
| Study in J. Braz. Soc. Mech. Sci. Eng. (authors not checked) | PLA | infill overlap and orientation are dominant factors on UTS | [M, from abstract] |

No source found reports a direct measurement of the wall-to-infill interface strength. It has to be estimated (Section 5).

---

## 4. Models from the literature

### 4.1 Mesostructure / stiffness: Li, Sun, Bellehumeur & Gu (2001, SFF Symposium; also Li 2002 PhD, Calgary)

FDM part = partially bonded filaments plus voids. Two void measures in the plane normal to the bead:

- ρ1: area void fraction.
- ρ2: linear void fraction in the transverse direction, so (1 − ρ2) is the bonded length divided by the overall
  dimension.

Their lamina model:

    E11 = (1 − ρ1) · E
    E22 = α · (1 − ρ2) · E
    G12 = α · (1 − ρ2) · G
    ν12 = ν

where α is an empirical bond factor. With E = 2230 MPa, G = 833 MPa, ν = 0.34 (P400 filament) and ρ from their
micrographs (Section 3.1 table), the laminate model matched [0/90] … [45/−45] laminates within 3–7 %. Back-calculating
from their [0/90] value at −25.4 µm gives α ≈ 0.9 **[D]** (approximate, I assumed E_[0/90] ≈ (E11 + E22)/2).

### 4.2 Strength of a lamina: Rodriguez et al. 1999 and Tsai-Hill

Rodriguez used the Azzi-Tsai (Tsai-Hill) criterion for strength versus off-axis angle θ:

    1/σ_θ² = cos⁴θ/σ_L² + (1/S12² − 1/σ_L²)·cos²θ·sin²θ + sin⁴θ/σ_T²

Back-calculated S12 (in-plane shear strength) from 0°/45°/90° data **[D]**:

| Data | σ_L | σ_T | σ_45 | S12 (fit) | S12/σ_T |
|---|---|---|---|---|---|
| ABS, Ziemian 2014 | 23.08 | 8.37 | 10.61 | 6.9 MPa | 0.82 |
| ABS, Casavola 2016 | 26.11 | 6.71 | 11.97 | 13.2 MPa | 2.0 |
| PLA, Song 2017 (from Hill N = 1.27, yield 54.8 MPa) | 54.8 | 45.2 | — | ≈ 34 MPa | ≈ 0.75 |
| PLA, Casavola 2016 | 50.23 | 22.49 | 40.68 | not stable (σ_T scatter ±6.75 MPa) | — |

So S12 ≈ (0.75–2.0)·σ_T; a conservative default is S12 = σ_T.

### 4.3 Neck growth (wetting / sintering)

- **Frenkel (1945)** viscous sintering of two spheres of radius a: (y/a)² ≈ (3Γ t)/(2 η a), y = neck radius, Γ = surface
  tension, η = viscosity. Valid only for early stage.
- **Pokluda, Bellehumeur & Vlachopoulos (1997, AIChE J. 43:3253)**, closed form for the whole process, written with the
  bond angle θ = asin(y/a):

      dθ/dt = Γ/(a0·η) · [2^(−5/3) cosθ sinθ (2 − cosθ)^(1/3)] / [(1 − cosθ)(1 + cosθ)^(1/3)]

  with a(θ) from volume conservation, a = a0·(4 / ((1 + cosθ)²(2 − cosθ)))^(1/3).
- **Bellehumeur, Li, Sun & Gu (2004, J. Manuf. Processes 6:170)** apply it non-isothermally to FDM ABS with η(T), Γ(T)
  from a bead cooling model.
- **Gurrala & Regalla (2014, Virtual Phys. Prototyping)** version for two cylinders (beads), as quoted in Kapusuzoglu et
  al.:

      dθ/dt = Γ/(3·√π·a0·η) · [((π − θ)cosθ + sinθ)·(π − θ + sinθ cosθ)^(1/2)] / [(π − θ)²·sin²θ]

- ABS parameters used by Kapusuzoglu et al. (from Bellehumeur and Sun): Γ(240 °C) = 0.029 N/m, dΓ/dT = −0.00345
  N/(m·K) as quoted; η = η_r·exp(−β (T − T_r)), η_r = 5100 Pa·s at T_r = 240 °C, β = 0.056 1/K; ρc = 1.196e6 J/(m³K);
  h_conv = 86 W/(m²K), filament-filament contact 200 W/(m²K), k = 0.15 W/(mK); contact fraction of perimeter 0.15 **[M,
  as used in the paper]**.
- Sun et al. 2008: neck growth by sintering happens only during the short time above a critical sintering temperature;
  after that, creep under the weight of later beads dominates the change in mesostructure.

### 4.4 Bond quality (interdiffusion / healing)

- **Wool & O'Connor reptation healing**: interface strength grows as σ/σ∞ = (t/t_r)^(1/4) for t < t_r, then saturates;
  t_r is the reptation (recovery) time, Arrhenius or WLF in T.
- **Non-isothermal degree of healing** (Yang & Pitchumani 2002, used widely for FFF):

      D_h(t) = [ ∫ dt' / t_r(T(t')) ]^(1/4) , integrated while T > T_g (or T_c for semi-crystalline), capped at 1.

- Rodriguez 1999 form: K_c = K_0(T) + (K_∞ − K_0)·[∫ dt/t_∞(T)]^(1/4), t_∞ = C·exp(Q_d/RT); for P400 ABS they found
  Q_d ≈ 390 kJ/mol from annealing data, but this over-predicted bonding at extrusion temperatures (Q_d falls at high T,
  and the polymer degrades) **[M]**.
- Coogan & Kazmer 2017 (RPJ 23:414 and 23:551): bond strength = diffusion × wetting × intimate contact; a healing
  simulation predicted measured ABS bond strengths with R² = 0.795 **[M]**.

### 4.5 Void-limited strength

Tronvoll, Welo & Elverum 2018 (Int J Adv Manuf Technol, 10.1007/s00170-018-2148-x): much of PLA's anisotropy is
explained by the reduced residual cross-section at inter-bead voids, with a weakest-link statistical model **[M,
qualitative]**. Fang 2020 (PC): strength scales with bond width at the narrowest section, modulus loss is about half the
bond-width loss **[M]**.

---

## 5. Proposed engine model (per voxel, orthotropic, bead-frame)

### 5.1 Inputs per voxel (from the slicer data the engine already has)

- Local frame (t1, t2, t3): bead tangent, in-layer normal to the bead, layer normal. For voxels with no bead (void) or
  mixed content, use a volume-fraction blend (5.6).
- Material id → base constants (5.4).
- Geometry: bead width w, layer height h, line spacing s (so overlap/air gap g = s − w, negative = overlap).
- Thermal/time: nozzle temperature T_n, chamber temperature T_ch, fan fraction, and **Δt_n**, the time between laying
  this bead and its in-layer neighbour (from the toolpath timing; the engine knows it).
- Optional: the per-interface neck ratio λ from a thermal + neck growth model if the engine runs it (GPU-friendly).

### 5.2 Two bond descriptors

1. **Contact (geometric) ratio λ ∈ [0, 1]**: bonded length across a bead-bead interface divided by layer height h
   (for direction 2). Default from overlap, fitted to the ABS data of Rodriguez/Li **[E]**:

       λ = clamp( λ0 + k_g · (−g / w), λ_min, 0.95 )

   with λ0 = λ at zero gap (≈ 0.55 ABS, 0.6 PLA/PETG, see table), k_g ≈ 2.5 (ABS: −25.4 µm on a 0.3–0.5 mm bead
   raised bond density from ≈ 0.6 to 0.73), λ_min ≈ 0.05. If g > 0 and the next layer is staggered, use λ from the
   neck model or ≈ 0.4 (Rodriguez skewed case). If the engine runs the neck model (4.3) on the measured/predicted
   interface temperature, λ = y/(h/2) from that model replaces this formula.
2. **Bond quality η ∈ [0, 1]**: fraction of bulk interface strength reached by interdiffusion. Default:

       η = η_ref · f_T · f_t,   capped at 1

   - f_T (nozzle temperature): 1 at the material's reference temperature, −1.5 %/°C below it down to 0.6, flat for
     +0…+20 °C above, then −1 %/°C (degradation), from PLA/PETG/PC data in 3.2 **[E]**.
   - f_t (neighbour delay): f_t = (1 + Δt_n / τ_b)^(−1/4) normalised so f_t = 1 at Δt_n = 1 s, with τ_b ≈ 2 s for
     open-frame printing and τ_b ≈ 8 s in a heated chamber; f_t is constant for Δt_n > 5–10 s (Ai & Vogt 2022) **[E]**.
     The exponent 1/4 follows reptation scaling; the time constants are estimates.
   - Preferred, when the engine has an interface temperature history T(t): η = D_h from 4.4 with t_r(T) calibrated per
     material from one black-box test (90° tensile at two nozzle temperatures).

### 5.3 Constitutive laws

    E1  = (1 − p) · E_f · c1                 c1 ≈ 0.9–1.0 (molecular orientation, defects)
    E2  = (1 − p) · E_f · λ^n_E             n_E ≈ 0.5   (Fang: modulus loss ≈ half the bond-width loss)
    E3  = (1 − p) · E_f · λ_z^n_E           λ_z from layer contact (bond width / bead width)
    G12 = G_f · (1 − p) · λ^n_E
    G13 = G_f · (1 − p) · λ_z^n_E ;  G23 = E2 / (2 (1 + ν23))
    ν12 = ν_f ;  ν23 ≈ ν_f + 0.03 ;  ν13 ≈ ν_f

    σ1  = (1 − p) · σ_f · c_s               c_s ≈ 0.85–0.95 (Coogan & Kazmer 95 %, Ziemian 0.83–0.93)
    σ2  = σ_f · λ · η
    σ3  = σ_f · λ_z · η_z
    S12 = σ2 (default; range 0.75–2 σ2) ;  S13 = S23 = σ3

    Failure: Tsai-Hill (tension) on (σ11, σ22, σ33, τ12, τ13, τ23) in the bead frame; separate compression strengths
    (FDM compression is about 1.6–2 × tension for amorphous polymers: Ahn 2002, Song 2017).

p is the volume porosity (≈ ρ1). Quick check against data **[D]**:

- ABS Rodriguez, −25.4 µm: λ = 0.73, p = 0.068 → E2 = 0.932 · 2230 · 0.854 = 1776 MPa (measured 1765). σ2 = 13.4 MPa
  implies η = 13.4 / (31.2 · 0.73) = 0.59.
- ABS Rodriguez, annealed: η → 0.81 (18.5 / (31.2 · 0.73)).
- PLA Song 2017: p = 0.015, E2/E_f ≈ 1 → λ ≈ 1; σ2/σ_f = 46.2/49.5 ≈ 0.93 → λ·η ≈ 0.93.
- PLA Casavola 2016 (0.35 mm layers, 0.7 mm beads, air gap 0): σ2/σ_f = 0.41, E2/E_f = 0.82 → λ ≈ 0.67, η ≈ 0.6.

### 5.4 Proposed constants per material

Base (filament/bulk, dry, room temperature). Ranges are typical published values; specific grades vary.

| Material | E_f [GPa] | σ_f tensile [MPa] | ν_f | G_f [GPa] | Source / tag |
|---|---|---|---|---|---|
| PLA | 3.4 (3.2–3.6) | 55 (45–65) | 0.33–0.36 | 1.25 | Casavola wire 3.38 / 55.3 [M]; Song moulded 3.68 / 49.5 [M]; Casavola G12 [M] |
| PETG | 2.0 (1.8–2.2) | 50 (45–55) | 0.38 | 0.75 | CNC Kitchen lying 55 [M]; Popa 55 [M]; E [E] |
| ABS | 2.1 (1.8–2.3) | 32 (26–40) | 0.34 | 0.83 | P400 2.23 / 31.2 (Rodriguez), wire 1.81 / 32.1 (Casavola) [M]; G 0.833 (Li) [M] |
| PC | 1.9–2.3 | 60 (55–72) | 0.35 | 0.75 | bulk 1.87 / 71.6, printed filament 1.82 / 55.2 (Fang 2020) [M]; ν 0.35 [M] |
| PA6/PA12 (unfilled, dry) | 1.6 (1.2–2.4) | 45 (35–60) | 0.40 | 0.55 | [E]; drops strongly with moisture |
| PA-CF (short fibre) | along bead 4–8, compression 3.1–3.4 (Onyx) | 60–100 (Dul et al. 96 MPa, 7.9 GPa) | 0.35 | 0.6–1.0 | [M] for quoted values, rest [E] |
| TPU 95A | 0.02–0.06 (initial) | 25–40 | 0.48 | E/3 | [E]; hyperelastic, use only for small strain |

Intra-layer descriptors at **typical default slicer settings** (small overlap, nozzle at the filament maker's
recommended temperature, fan on for PLA/PETG, open frame), and the resulting ratios:

| Material | λ (default) | η (default) | σ2/σ1 default (range in data) | E2/E1 default (range in data) | σ3/σ1 default | Basis |
|---|---|---|---|---|---|---|
| PLA | 0.85 | 0.75 | **0.65** ± 0.2 (0.41–0.84) | **0.92** ± 0.08 (0.82–1.02) | 0.6 ± 0.15 | Casavola, Song [M]; CNC Kitchen Z [M]; defaults [E] |
| PETG | 0.9 | 0.85 | **0.8** ± 0.15 (0.89–0.95, one study) | **0.9** ± 0.1 | 0.5 ± 0.15 | Popa [M]; CNC Kitchen Z [M]; E2 [E] |
| ABS | 0.75 | 0.55 | **0.45** ± 0.2 (0.14–0.75) | **0.75** ± 0.15 (0.61–0.90) | 0.45 ± 0.2 | Rodriguez, Ziemian, Casavola, Ahn [M]; Coogan Z [M] |
| PC | 0.75 | 0.6 | **0.5** ± 0.2 | **0.8** ± 0.15 | 0.5 ± 0.2 | trend data only (Cantrell, Hill & Haghi, Fang) → mostly [E] |
| PA (unfilled) | 0.85 | 0.7 | **0.6** ± 0.2 | **0.85** ± 0.1 | 0.5 ± 0.2 | no direct data found → [E] |
| PA-CF | 0.8 | 0.6 | **0.4** ± 0.15 | **0.37** ± 0.05 | 0.35 ± 0.15 | Canegrati E [M]; PETG-CF strength analogue 0.49 [M]; rest [E] |
| TPU | 0.85 | 0.7 | **0.6** ± 0.3 (0.25 positive gap … ≈1 negative gap) | **0.8** ± 0.2 | 0.6 ± 0.3 | Czekanski & Etiwa, Hohimer [M]; defaults [E] |

For PA-CF the E2/E1 ratio comes from fibre alignment, not only from the bond, so for fibre-filled materials use
E1 = E_f,along and E2 = E_matrix-like value (≈ 1.2 GPa for Onyx) rather than the λ formula.

### 5.5 Corrections for slicer parameters (multiplicative on λ or η, then clamp) **[E]**

| Parameter | Effect in the model | Data basis |
|---|---|---|
| Overlap / negative air gap (−g/w) | λ via 5.2 formula; also lowers p | Rodriguez, Li, Ahn, Hohimer |
| Extrusion multiplier m | treat as w_eff = w·m at fixed spacing (more overlap); above m ≈ 1.03–1.05 quality drops (surface defects): no further gain | Lendvai 2024 |
| Nozzle temperature | f_T on η (5.2) | CNC Kitchen, Fang, Rodriguez |
| Neighbour delay Δt_n (line length / speed + travel) | f_t on η; long lines and low speed lower η, small islands raise it | Ai & Vogt, Kapusuzoglu, Rodriguez |
| Print speed | via Δt_n (shorter delay helps) and via lower deposition temperature at very high flow (cap by the hotend's volumetric limit; −1 % η per 1 mm³/s above 80 % of the limit) | Kapusuzoglu frontier; last coefficient pure estimate |
| Chamber / envelope temperature | η × (1 + 0.0075 · (T_ch − 25 °C)) for amorphous polymers, capped at 1 | Rodriguez K_c slope |
| Fan | lower τ_b (faster cooling): fan 100 % → τ_b × 0.5 | estimate from cooling time constants |
| Layer height (for λ_z) | λ_z falls as h/w rises (Fang PC: bond width decreased with layer height) | Fang 2020 |
| Annealing (post-process) | η × 1.3 (ABS: +38 % transverse); E unchanged; distortion not modelled | Rodriguez 1999 |

### 5.6 Walls, wall-to-infill bond and wall count **[E]**

- Each perimeter bead is a voxel row with axis 1 along the outline. The perimeter-perimeter interface uses the same
  λ, η as infill beads (perimeters are usually printed back-to-back, so Δt_n is short and η is often higher than in
  long infill lines).
- **Wall-to-infill interface**: infill lines touch the innermost wall mostly at their line ends and along any infill
  segment parallel to the wall. Model it as an interface (cohesive layer or reduced-strength voxels) with
  σ_wi = σ2 · min(1, o/(0.5 w)) · 0.8, where o is the infill-wall overlap length. The 0.8 accounts for end-cap contact
  and longer delay. No measured value exists; calibrate with one black-box test (shear-out of a wall shell from a
  solid infill core).
- Wall count effect follows from the rule of mixtures over the cross-section: walls along the load carry σ1, infill
  carries σ2 or σ_θ. The Onyx data (1 → 15 walls, +37 %) are consistent with this, and with failure starting at the
  wall/core transition.

### 5.7 Mixed voxels and GPU layout

- Per voxel store: frame (two unit vectors, t3 from the layer field), material id, porosity p, λ (in-layer), λ_z,
  η (in-layer), η_z. Eight floats plus two vectors fit a 64-byte record (contiguous, structure of arrays for GPU).
- Voxels covering several beads with different directions: average the rotated stiffness tensors (Voigt) for a
  stiffness estimate; for strength keep the weakest direction's criterion (conservative) or the most common bead
  direction.
- Reference implementation on CPU; the per-voxel tensor rotation and Tsai-Hill evaluation are trivially parallel.

### 5.8 Uncertainty and calibration plan

- The largest uncertainties are η (bond quality) for PC, PA, TPU and the wall-to-infill interface. Expect ±30–50 % on
  σ2 before calibration; stiffness is better (±10–15 %).
- Recommended black-box calibration per material (cheap, can be done on a production printer): single-layer or
  unidirectional 0° and 90° coupons (Casavola-style, ASTM D638/D3039), one 45° coupon (for S12), and a standing coupon
  for σ3, at two nozzle temperatures and two line lengths (short vs long Δt_n). That fixes λ0, η_ref, f_T slope and
  τ_b.

---

## 6. Sources

Peer-reviewed / proceedings (numbers used above):

1. Ahn S.-H., Montero M., Odell D., Roundy S., Wright P.K. (2002). Anisotropic material properties of fused deposition
   modeling ABS. Rapid Prototyping Journal 8(4):248–257. doi:10.1108/13552540210441166. Author copy:
   iss.mech.utah.edu (Ahn-Anisotropic_material-2002.pdf).
2. Rodriguez J.F., Thomas J.P., Renaud J.E. (1999). Maximizing the strength of fused-deposition ABS plastic parts. Solid
   Freeform Fabrication Symposium, Austin. utw10945.utweb.utexas.edu/Manuscripts/1999/1999-039-Rodriguez.pdf.
3. Rodriguez J.F., Thomas J.P., Renaud J.E. (2000). Characterization of the mesostructure of fused-deposition
   acrylonitrile-butadiene-styrene materials. RPJ 6(3):175–186. doi:10.1108/13552540010337056.
4. Rodriguez J.F., Thomas J.P., Renaud J.E. (2001). Mechanical behavior of ABS fused deposition materials. Experimental
   investigation. RPJ 7(3):148–158. doi:10.1108/13552540110395547.
5. Rodriguez J.F., Thomas J.P., Renaud J.E. (2003). Mechanical behavior of ABS fused deposition materials modeling. RPJ
   9(4); and Design of fused-deposition ABS components for stiffness and strength. J. Mech. Des. 125(3):545–551,
   doi:10.1115/1.1582499 (abstracts only read).
6. Casavola C., Cazzato A., Moramarco V., Pappalettere C. (2016). Orthotropic mechanical properties of fused deposition
   modelling parts described by classical laminate theory. Materials & Design 90:453–458.
   doi:10.1016/j.matdes.2015.11.009 (pre-print, iris.poliba.it).
7. Song Y., Li Y., Song W., Yee K., Lee K.-Y., Tagarielli V.L. (2017). Measurements of the mechanical response of
   unidirectional 3D-printed PLA. Materials & Design 123:154–164. doi:10.1016/j.matdes.2017.03.051 (accepted
   manuscript, Imperial College Spiral).
8. Ziemian C., Sharma M., Ziemian S. (2012). Anisotropic mechanical properties of ABS parts fabricated by fused
   deposition modelling. In: Mechanical Engineering, InTech. doi:10.5772/34233.
9. Ziemian S., Okwara M., Ziemian C.W. (2014/2015). Tensile and fatigue behavior of layered acrylonitrile butadiene
   styrene. SFF Symposium 2014 (utw10945.utweb.utexas.edu/sites/default/files/2014-044-Ziemian.pdf); journal version
   RPJ 21(3), 2015.
10. Li L., Sun Q., Bellehumeur C., Gu P. (2001). Composite modeling and analysis of FDM prototypes for design and
    fabrication of functionally graded parts. SFF Symposium
    (utw10945.utweb.utexas.edu/Manuscripts/2001/2001-22-Li,Sun.pdf); J. Manuf. Processes 4(2), 2002.
11. Bellehumeur C., Li L., Sun Q., Gu P. (2004). Modeling of bond formation between polymer filaments in the fused
    deposition modeling process. J. Manuf. Processes 6(2):170–178.
12. Pokluda O., Bellehumeur C.T., Vlachopoulos J. (1997). Modification of Frenkel's model for sintering. AIChE J.
    43(12):3253–3256. doi:10.1002/aic.690431213.
13. Sun Q., Rizvi G.M., Bellehumeur C.T., Gu P. (2008). Effect of processing conditions on the bonding quality of FDM
    polymer filaments. RPJ 14(2):72–80. doi:10.1108/13552540810862028 (abstract only).
14. Gurrala P.K., Regalla S.P. (2014). Part strength evaluation of FDM parts using neck growth model. Virtual and
    Physical Prototyping 9(3) (equation as quoted in item 15).
15. Kapusuzoglu B., Sato M., Mahadevan S., Witherell P. Process optimization under uncertainty for improving the bond
    quality of polymer filaments in fused filament fabrication. J. Manuf. Sci. Eng. 143(2):021007 (2021); arXiv
    2608.18431.
16. Coogan T.J., Kazmer D.O. (2017). Bond and part strength in fused deposition modeling. RPJ 23(2):414–422,
    doi:10.1108/RPJ-03-2016-0050; and Healing simulation for bond strength prediction of FDM. RPJ 23(3):551–561
    (abstracts).
17. Fang L., Yan Y., Agarwal O., Seppala J.E., Hemker K.J., Kang S.H. (2020). Processing-structure-property
    relationships of bisphenol-A-polycarbonate samples prepared by fused filament fabrication. Additive Manufacturing
    (NIST contribution; par.nsf.gov/servlets/purl/10181525).
18. Ai J.-R., Vogt B.D. (2022). Size and print path effects on mechanical properties of material extrusion 3D printed
    plastics. Progress in Additive Manufacturing 7:1009–1021. doi:10.1007/s40964-022-00275-w (abstract).
19. Canegrati A., Martulli L.M., Bolzoni G., Kostovic M., Rollo G., Sorrentino A., Carboni M., Bernasconi A. (2022). 3D
    printed short carbon fibres reinforced polyamide: tensile and compressive characterisation and multiscale failure
    analysis. ECCM20, Lausanne. doi:10.5075/epfl-298799_978-2-9701614-0-0.
20. Popa C.F., Galatanu S.V., Marsavina L. Prediction of the tensile strength of FDM specimens based on Tsai-Hill
    criteria. Frattura ed Integrità Strutturale (fracturae.com, issue 73).
21. Coca-Gonzalez M. et al. (2025). Influence of perimeter layers on tension mechanical properties of 3D printed Onyx.
    Frontiers in Mechanical Engineering 11. doi:10.3389/fmech.2025.1528516.
22. Perez D., Celik E., Karkkainen R. Investigation of interlayer interface strength and print morphology effects in
    FDM 3D-printed PLA. (PMC9828590.)
23. Hohimer C.J., Christ J., Aliheidari N., Mo C., Ameli A. (2017). 3D printed thermoplastic polyurethane with isotropic
    material properties. Proc. SPIE 10165, 1016511. doi:10.1117/12.2259810 (abstract).
24. Czekanski A., Etiwa (2025). Effect of raster angles on the anisotropy of FFF TPU. CSME 2025 (espace2.etsmtl.ca
    eprint 32382) (abstract).
25. Cantrell J. et al. (2017). Experimental characterization of the mechanical properties of 3D-printed ABS and
    polycarbonate parts. RPJ 23(4):811–824. doi:10.1108/RPJ-03-2016-0042 (abstract).
26. Hill N., Haghi M. (2014). Deposition direction-dependent failure criteria for fused deposition modeling
    polycarbonate. RPJ 20(3):221–227. doi:10.1108/RPJ-04-2013-0039 (abstract).
27. Kiendl J., Gao C. (2020). Controlling toughness and strength of FDM 3D-printed PLA components through the raster
    layup. Composites Part B 180 (abstract, NTNU Open).
28. Chacón J.M., Caminero M.A., García-Plaza E., Núñez P.J. (2017). Additive manufacturing of PLA structures using fused
    deposition modelling. Materials & Design 124:143–157 (abstract).
29. Tronvoll S.A., Welo T., Elverum C.W. (2018). The effects of voids on structural properties of fused deposition
    modelled parts: a probabilistic approach. Int J Adv Manuf Technol. doi:10.1007/s00170-018-2148-x (abstract).
30. Hossain M.S. et al. (2013). Improving tensile mechanical properties of FDM-manufactured specimens via modifying
    build parameters. SFF Symposium (utw10945.utweb.utexas.edu/Manuscripts/2013/2013-30-Hossain.pdf) (abstract).
31. Lendvai L. et al. (2024). Progress in Additive Manufacturing, doi:10.1007/s40964-024-00646-5 (extrusion multiplier,
    abstract).
32. Science of Sintering (2023), carbon-reinforced PETG raster angle study (doaj c2a3b572c5ff4a0baecd93ceda370238).
33. Wool R.P., O'Connor K.M. (1981). A theory of crack healing in polymers. J. Appl. Phys. 52:5953; Yang F.,
    Pitchumani R. (2002). Healing of thermoplastic polymers at an interface under nonisothermal conditions.
    Macromolecules 35:3213 (standard references, not re-read for this note).

Blogs / user tests (measured, not peer-reviewed):

34. CNC Kitchen (S. Laub). The influence of extrusion temperature on layer adhesion.
    cnckitchen.com/blog/the-influence-of-extrusion-temperature-on-layer-adhesion.
35. HackSpace magazine, Experiments with strong 3D prints (perimeters vs infill; qualitative).

Not found / gaps: direct measurements of wall-to-infill interface strength; unfilled PA and PC 0°/90° unidirectional
tables (only trends in abstracts); PETG unidirectional tests at standard 0.4 mm nozzles; full numeric tables of
Kiendl & Gao 2020, Cantrell 2017, Hill & Haghi 2014, Domingo-Espin 2015 (PC orthotropic constants, paywalled).
