# Making the engine's CFD (LBM) and FEA (voxel) simulations more accurate and faster: literature review

Date: 2026-11-03. Clean-room research note. Sources are papers, books, technical reports and public specifications
only. No slicer or GPL/AGPL source was read. All formulas below are written in the engine's own notation from
what the papers describe, and they must be checked against the papers before implementation. Where a bibliographic
detail (DOI, page range) was **not** confirmed during this search, it is marked "(unverified)".

The engine today, as summarized in the request:

- **CFD:** D3Q19, BGK with Smagorinsky, uniform grid of 16–48 cells across the frontal size, staircase voxels,
  halfway bounce-back walls, velocity inlet, zero-gradient outlet, periodic sides, Re capped at 40 × cells.
  Pressure drag comes from the time-averaged density on the faces, and friction comes from flat-plate correlations.
  CPU with oneTBB, push scheme, SoA FP32. Measured: cube Cd 1.07 at 12 cells (ref 1.05), flat plate 1.00 (ref 1.17).
- **FEA:** hex8 with one element per voxel, orthotropic per voxel. Solver is GMG-preconditioned CG, failure is a
  Tsai-Wu/max-stress style safety factor per voxel, resolution is 2/4/6 voxels per line width.

---

## 0. Executive summary: what to implement first

| # | Area | Change | Impact | Effort | Expected gain |
|---|------|--------|--------|--------|---------------|
| 1 | LBM | **TRT collision with Λ = 3/16** (replaces BGK; Smagorinsky stays on ω⁺) | high | very low | Wall position becomes independent of viscosity. More stable at low ν. Cost about +5 %. |
| 2 | LBM | **Momentum-exchange force** (total force from the links), split into pressure and shear parts | high | low | Removes the "density on faces" approximation. Gives a consistent force, needed for item 3. |
| 3 | LBM | **Interpolated bounce-back (Bouzidi 2001)** with q from the triangle mesh | high | medium | Wall error goes from 1st to 2nd order. Literature shows the coarse-grid drag error drops by a factor of about 2–5. |
| 4 | LBM | **Store populations shifted by wᵢ** (fᵢ − wᵢρ₀), then **FP16 storage** | medium/high | low | Better FP32 round-off now. FP16 halves memory traffic later, so about 1.5–2× faster on bandwidth-bound hardware. |
| 5 | LBM | **In-place streaming (AA or Esoteric Pull)** instead of two grids with push | high (speed) | medium | Half the memory. About 1.3–1.5× faster on CPU because no write-allocate traffic. Required for the GPU. |
| 6 | LBM | **Early stop on Cd convergence + coarse-to-fine initialization** | high (speed) | low | Typically 2–4× fewer time steps on the finest grid. |
| 7 | LBM | **Sponge outlet + blockage correction (Maskell)** | medium | low | Removes 2–10 % domain-dependent bias, depending on blockage ratio. |
| 8 | LBM | **Richardson extrapolation over the 16/24/32/48 ladder** | medium | very low | A better Cd estimate from runs the engine already does, plus an error bar. |
| 9 | LBM | 2-level block refinement around the body (Rohde volumetric scheme) | high | high | Same near-wall resolution for about 1/4–1/8 of the cells. |
| 1 | FEA | **Incompatible-modes / enhanced-strain hex8** (Wilson–Taylor / Simo–Rifai) | high | medium | Removes shear locking at 2 voxels per line width. Bending stiffness errors of tens of percent shrink to a few percent. |
| 2 | FEA | **Stress for failure averaged over a critical distance** (Theory of Critical Distances) instead of the single-voxel peak | high | low | Removes staircase stress spikes in a physically grounded way. |
| 3 | FEA | **Galerkin coarse operators + Chebyshev smoother** in the GMG | high (robustness) | medium | Iteration counts stay flat for thin walls and sparse infill. |
| 4 | FEA | **Mixed precision** (FP32 V-cycle, FP64 CG) and residual tolerance about 1e-5 | medium (speed) | low | About 1.5–2× on CPU and 2–4× on GPU for the preconditioner. |
| 5 | FEA | **Volume-fraction (finite-cell-lite) boundary voxels** | medium | low/medium | Smoother compliance, less staircase stiffening. |
| 6 | FEA | **Homogenized infill blocks** (RVE with periodic boundary conditions, cached per pattern/density/angle) | high (speed) | medium | Infill interior can be meshed 4–8× coarser per axis. |
| 7 | FEA | GPU matrix-free node-gather operator (Wu et al. 2016) | high (speed) | high | 10–50× over a single CPU core, a few × over multicore. |

The sections below give the details, formulas and algorithm steps for each topic.

---

## 1. LBM accuracy

### 1.1 Collision models

**Baseline problem.** BGK at τ → 1/2 (low viscosity) is unstable. It also makes the effective position of a
bounce-back wall depend on τ. With Smagorinsky, τ varies per cell, so the effective wall position varies across the
body. That alone can produce a few percent of drag error on coarse grids.

#### a) TRT (two-relaxation-time). Recommended first.

Ginzburg, Verhaeghe & d'Humières (2008), "Two-relaxation-time lattice Boltzmann scheme: about parametrization,
velocity, pressure and mixed boundary conditions", *Commun. Comput. Phys.* 3(2):427–478.

Algorithm, for each pair of directions (i, ī = opposite of i):

```
f⁺ = (f_i + f_ī)/2          f⁻ = (f_i − f_ī)/2
feq⁺ = (feq_i + feq_ī)/2    feq⁻ = (feq_i − feq_ī)/2
f_i* = f_i − ω⁺ (f⁺ − feq⁺) − ω⁻ (f⁻ − feq⁻)
f_ī* = f_ī − ω⁺ (f⁺ − feq⁺) + ω⁻ (f⁻ − feq⁻)
ν = c_s² (1/ω⁺ − 1/2)                 (c_s² = 1/3)
Λ = (1/ω⁺ − 1/2)(1/ω⁻ − 1/2)   →   ω⁻ = 1 / (Λ/(1/ω⁺ − 1/2) + 1/2)
```

The rest population takes ω⁺ only.

- **Λ = 3/16:** halfway bounce-back puts the wall exactly halfway for Poiseuille flow, independent of ν. This is
  the "magic" value for wall accuracy.
- **Λ = 1/4:** the best-stability value.

With Smagorinsky, compute ω⁺ per cell from ν₀ + ν_t, then compute ω⁻ from Λ per cell. This keeps the wall location
fixed even where ν_t varies.

- **Expected effect:** viscosity-independent wall placement and better stability at low ν. Cost is almost nothing
  (one extra division per cell, or a table).

#### b) Regularized and recursive-regularized (RR) collision

- Latt & Chopard (2006), "Lattice Boltzmann method with regularized pre-collision distribution functions",
  *Math. Comput. Simul.* 72:165–168 (DOI 10.1016/j.matcom.2006.05.017, unverified).
- Malaspinas (2015), "Increasing stability and accuracy of the lattice Boltzmann scheme: recursivity and
  regularization", arXiv:1505.06900.
- Coreixas, Wissocq, Puigt, Boussuge & Sagaut (2017), "Recursive regularization step for high-order lattice
  Boltzmann methods", *Phys. Rev. E* 96:033306, DOI 10.1103/PhysRevE.96.033306 (arXiv:1704.04413).

Steps per cell:

1. Π^neq_αβ = Σ_i c_iα c_iβ (f_i − feq_i). This needs only 6 components.
2. Project the non-equilibrium part onto the 2nd-order Hermite basis:
   f_i^neq,reg = w_i / (2 c_s⁴) · (c_iα c_iβ − c_s² δ_αβ) Π^neq_αβ.
3. f_i* = feq_i + (1 − ω) f_i^neq,reg.

Π^neq also gives the strain rate directly. Smagorinsky can therefore be applied in closed form, with no finite
differences (Hou, Sterling, Chen & Doolen 1996, "A lattice Boltzmann subgrid model for high Reynolds number flow",
*Fields Inst. Commun.* 6:151–166):

```
Q = sqrt(Π^neq_αβ Π^neq_αβ)
τ_eff = ½ [ τ₀ + sqrt( τ₀² + 18 √2 C_s² Q / ρ ) ]       (lattice units, Δx = Δt = 1)
```

The engine probably already does something equivalent; check it against this formula.

The RR variant adds 3rd-order Hermite terms that are built recursively from u and Π. That is the
"recursive regularized" model, and it is noticeably more stable than plain regularization at high Re. The hybrid RR
(HRR) variant of Jacob, Malaspinas & Sagaut (2018, *J. Turbulence*; unverified) mixes in a finite-difference
estimate of the strain to add a little controlled dissipation.

- **Recommendation:** use plain regularization (or TRT) for this Re range. RR is the next step if instabilities
  appear at 48 cells with a small C_s.
- **Caveat:** regularization filters out some non-hydrodynamic modes. That is good for stability, but it can
  slightly smear sharp shear layers on coarse grids.

#### c) MRT on D3Q19

d'Humières, Ginzburg, Krafczyk, Lallemand & Luo (2002), "Multiple-relaxation-time lattice Boltzmann models in three
dimensions", *Phil. Trans. R. Soc. A* 360:437–451, DOI 10.1098/rsta.2001.0955.

- **Method:** transform f to 19 moments with the orthogonal matrix M given in the paper, relax each moment with its
  own rate s_k toward its equilibrium, then transform back with M⁻¹.
- **Relaxation rates:** the shear moments use s = ω (viscosity). The paper suggests values near 1.19 (bulk/energy),
  1.4, 1.2 and 1.98 for the remaining kinetic moments. Copy them from the paper's table.
- **Cost:** about 2× BGK on CPU unless the transform is optimized.
- **Recommendation:** TRT gives most of MRT's wall-accuracy benefit for much less effort. Choose MRT only if bulk
  viscosity tuning is needed, for acoustic damping.

#### d) Cumulant

- Geier, Schönherr, Pasquali & Krafczyk (2015), "The cumulant lattice Boltzmann equation in three dimensions: theory
  and validation", *Comput. Math. Appl.* 70(4):507–547, DOI 10.1016/j.camwa.2015.05.001.
- Geier, Pasquali & Schönherr (2017), "Parametrization of the cumulant lattice Boltzmann method for fourth order
  accurate diffusion, Part I/II", *J. Comput. Phys.* 348:862–888 (Part II covers the sphere at drag crisis).

The method relaxes statistically independent cumulants. Higher cumulants are usually relaxed to equilibrium
(rate 1), or set by the "parametrized" formulas for 4th-order diffusion.

- **Strength:** excellent stability at very low viscosity, good under-resolved behavior, and validated for sphere
  drag.
- **Limitation:** it is designed for **D3Q27**. D3Q19 lacks the corner populations, so a switch of lattice would be
  needed: 27 instead of 19 populations, about 42 % more memory traffic.
- **Recommendation:** worth it only if the engine moves to higher Re or wants LES without a Smagorinsky constant.
  Medium-high effort.

#### e) Entropic and KBC

Karlin, Bösch & Chikatamarla (2014), "Gibbs' principle for the lattice-kinetic theory of fluid dynamics",
*Phys. Rev. E* 90:031302, DOI 10.1103/PhysRevE.90.031302.

1. Split f = k + s + h: k holds the conserved moments, s the shear (stress) part, and h the higher-order part.
2. Post-collision: f* = f − β[2Δs + γΔh], with β = ω/2.
3. γ is chosen per cell to maximize entropy. To first order, γ = 1/β − (2 − 1/β)⟨Δs|Δh⟩/⟨Δh|Δh⟩, where ⟨·|·⟩ is
   the entropic scalar product weighted by 1/feq.

- **Strength:** parameter-free stabilization, with no Smagorinsky needed. Very robust at high Re.
- **Limitation:** like cumulant, best on D3Q27. The per-cell scalar products cost about 30–50 % extra.
- **Recommendation:** an alternative to cumulant. Not a first priority at the engine's Re cap.

**Ranking for collision:** TRT (Λ = 3/16) → regularized (closed-form Smagorinsky) → RR/HRR → cumulant or KBC
(with D3Q27).

### 1.2 Wall treatment

#### a) Interpolated bounce-back (Bouzidi–Firdaouss–Lallemand). Top priority for wall accuracy.

Bouzidi, Firdaouss & Lallemand (2001), "Momentum transfer of a Boltzmann-lattice fluid with boundaries",
*Phys. Fluids* 13(11):3452–3459, DOI 10.1063/1.1399290.

**Definitions.** For a fluid node x_f and a direction i whose link x_f → x_f + c_i crosses the wall, let
q ∈ (0, 1] be the fraction of the link that lies in the fluid: q = |x_wall − x_f| / |c_i|. Compute q by ray–triangle
intersection against the part mesh, which the engine already has. The ray runs from the cell center along c_i, and
only the cut links of boundary cells are needed. Post-collision values are marked *. The opposite direction is ī.

```
if q < 1/2:  f_ī(x_f, t+1) = 2q f_i*(x_f) + (1 − 2q) f_i*(x_f − c_i)
if q ≥ 1/2:  f_ī(x_f, t+1) = f_i*(x_f)/(2q) + (2q − 1)/(2q) · f_ī*(x_f)
moving wall: subtract 2 w_i ρ (c_i·u_w)/c_s² (sign as in the paper)
```

When x_f − c_i is not fluid (a thin gap), fall back to plain bounce-back (q = 1/2) for that link.

- **Accuracy:** the scheme is 2nd order in Δx. Halfway bounce-back on a voxelized body is effectively 1st order,
  because the geometry error is O(Δx).
- **Expected gain:** for sphere and cylinder benchmarks, the literature reports the coarse-grid drag error shrinking
  by roughly 2–5× at 10–20 cells per diameter. For sharp-edged, axis-aligned bodies such as the cube, the gain is
  small, because staircase matches the geometry exactly. The gain shows up on curved and inclined parts, which is
  what users print.
- **Combination with TRT:** pair it with TRT. Linear interpolated bounce-back is viscosity-dependent under BGK, and
  TRT's Λ reduces that (Ginzburg 2008).
- **Data layout:** store a compact list of boundary links, (cell index, direction, q) as uint32 + uint8 + float16.
  That costs about 1–3 % of the cells × 5 links. It is GPU-friendly.

#### b) Multireflection (MR)

Ginzburg & d'Humières (2003), "Multireflection boundary conditions for lattice Boltzmann models",
*Phys. Rev. E* 68:066614, DOI 10.1103/PhysRevE.68.066614.

The method uses up to three nodes along the link, with coefficients chosen so the wall's Taylor expansion is exact
to 2nd order or better. It also adds a post-collision correction term that uses the non-equilibrium part, which
removes the viscosity dependence.

- **Strength:** more accurate than Bouzidi, especially for the wall location of tangential flows.
- **Cost:** needs 2–3 fluid nodes per link. Thin printed walls often do not have them.
- **Recommendation:** second step after Bouzidi + TRT.

A related simpler scheme is Yu, Mei & Shyy (2003), "A unified boundary treatment in lattice Boltzmann method", AIAA
paper 2003-0953 (unverified). It interpolates in a single formula with no q < ½ / q ≥ ½ branch, which is convenient
on GPUs.

#### c) Immersed boundary (IBM)

- Peskin (2002), "The immersed boundary method", *Acta Numerica* 11:479–517.
- For LBM: Feng & Michaelides (2004, *J. Comput. Phys.*); Wu & Shu (2009), implicit velocity correction
  (unverified).

The body is represented by Lagrangian markers. Velocity is interpolated to the markers with a smoothed delta kernel
(2–4 cells wide), a force is computed that enforces no-slip, and that force is spread back to the grid.

- **Strength:** no staircase, smooth geometry, easy for moving bodies.
- **Weakness:** the wall is smeared over about 2 cells, so the effective body is slightly "thicker". At 12–24 cells
  across, that is a several-percent drag bias, and it needs a corrected effective diameter.
- **Recommendation:** not worth it for static parts. Bouzidi is better here.

#### d) Force evaluation: the momentum exchange method (MEM)

- Ladd (1994); Mei, Yu, Shyy & Luo (2002), "Force evaluation in the lattice Boltzmann method involving curved
  geometry", *Phys. Rev. E* 65:041203, DOI 10.1103/PhysRevE.65.041203.
- Galilean-invariant form: Wen, Zhang, Tu, Wang & Fang (2014), "Galilean invariant fluid–solid interfacial dynamics
  in lattice Boltzmann simulations", *J. Comput. Phys.* (arXiv:1303.0625).

```
F_on_body = Σ_boundary links (x_f, i)  [ (c_i − u_w) f_i*(x_f, t) − (c_ī − u_w) f_ī(x_f, t+1) ]
C_D = F_x / (½ ρ₀ U² A_ref)
```

This gives the **total** force (pressure plus viscous) at the simulated Re, consistently with whatever wall rule is
used. With Bouzidi it is 2nd-order accurate. Average it in time.

**Suggested split for this engine**, which caps Re and corrects friction with correlations:

1. Pressure part: integrate p = c_s²(ρ − ρ₀) at the wall-adjacent fluid nodes, extrapolated to the wall along the
   link with q, times the link's projected area.
2. Viscous part at simulated Re: F_visc = F_MEM − F_pressure.
3. Final drag: pressure part plus friction from correlations at the **real** Re.

This keeps the engine's current philosophy, but it makes the pressure part consistent with the boundary rule.

### 1.3 Turbulence model and wall-modelled LES

- **Smagorinsky in laminar or transitional regions:** it is too dissipative near walls and in the shear layers that
  separate from sharp edges. That lengthens the recirculation, raises the base pressure and **lowers** Cd. This
  plausibly explains part of the flat plate's under-prediction (1.00 against 1.17).
- **Options:**
  - Van Driest damping near walls.
  - WALE: Nicoud & Ducros (1999), "Subgrid-scale stress modelling based on the square of the velocity gradient
    tensor", *Flow Turbul. Combust.* 62:183–200. It vanishes in pure shear and near walls.
  - Vreman (2004), *Phys. Fluids* 16:3670–3681. It vanishes in laminar shear.
- **Cost:** both need the velocity gradient, which takes finite differences of u over neighbors. That is one more
  pass, or computing it in the same kernel from neighbor macroscopic values.
- **Recommendation:** try C_s in 0.08–0.12 instead of 0.17, or switch to Vreman. Measure the plate and the cube.
- **Wall-modelled LES:**
  - Guo, Feng, Jacob, Renard & Sagaut (2014), "Wall model for large-eddy simulation based on the lattice Boltzmann
    method", *J. Comput. Phys.* 275:25–40.
  - Haussmann, Claro, Lipeme Kouyi, Rivière, Nirschl & Krause (2019), "Large-eddy simulation coupled with wall
    models for turbulent channel flows at high Reynolds numbers with a lattice Boltzmann method", *Comput. Math.
    Appl.* 78(10):3285–3302, DOI 10.1016/j.camwa.2019.04.033. It uses a Musker profile with van Driest-damped
    Smagorinsky.
  - Pasquali, Geier & Krafczyk (2020), "Near-wall treatment for the simulation of turbulent flow by the cumulant
    lattice Boltzmann method", *Comput. Math. Appl.*, DOI 10.1016/j.camwa.2017.11.022.

  Recipe: take u_t at the first off-wall node at distance y, and solve the log-law, Spalding law or Musker profile
  for u_τ. Then impose the wall shear τ_w = ρu_τ² by reconstructing the boundary populations, or by setting a slip
  velocity on the wall.
- **Recommendation for the engine:** low priority while Re is capped at about 40 × cells, since the first node is at
  y⁺ of order 1–10 at those Re. It becomes relevant only if the cap is raised toward the real Re (10⁴–10⁶ for printed
  parts in air). At that point a wall model can **replace** the flat-plate friction correlation with a local,
  geometry-aware friction.

### 1.4 Grid refinement

**Why:** to put fine cells near the body and in the wake and coarse cells in the far field. With 2 refinement
levels of factor 2, the near-body resolution is that of a 4× finer uniform grid, at roughly 1/4–1/8 of its cost.

**Common rules** (acoustic scaling Δt ∝ Δx, refinement factor n = 2):

- Viscosity equality: τ_f − ½ = n (τ_c − ½).
- The fine level does n substeps per coarse step.
- Interfaces need temporal interpolation of the coarse data for the intermediate fine substep.

#### a) Vertex-based, rescaling of the non-equilibrium part

- Filippova & Hänel (1998), "Grid refinement for lattice-BGK models", *J. Comput. Phys.* 147(1):219–228,
  DOI 10.1006/jcph.1998.6089. It transfers post-collision data.
- Dupuis & Chopard (2003), "Theory and applications of an alternative lattice Boltzmann grid refinement
  algorithm", *Phys. Rev. E* 67:066707, DOI 10.1103/PhysRevE.67.066707. It transfers pre-collision data and is
  stable for all τ.

```
coarse → fine: f_i^f = feq_i + (τ_f / (n τ_c)) f_i^neq,c
fine → coarse: f_i^c = feq_i + (n τ_c / τ_f) f_i^neq,f
```

Spatial interpolation of the coarse values at fine-only nodes must be at least 2nd order (quadratic or cubic) for
2nd-order accuracy. Dupuis & Chopard also report that a hierarchy of grids reaches the steady state up to 1000×
faster. That point is relevant to coarse-to-fine initialization (§2.5).

#### b) Cell-based, volumetric and mass-conservative

- Rohde, Kandhai, Derksen & Van den Akker (2006), "A generic, mass conservative local grid refinement technique for
  lattice-Boltzmann schemes", *Int. J. Numer. Meth. Fluids* 51:439–468.
- Chen et al. (2006), "Grid refinement in lattice Boltzmann methods based on volumetric formulation", *Physica A*
  362(1).
- Large-scale implementation: Schornbaum & Rüde (2016), "Massively parallel algorithms for the lattice Boltzmann
  method on nonuniform grids", *SIAM J. Sci. Comput.* 38(2):C96–C126 (unverified). This is the paper, not the code.

**Rohde steps**, with the coarse cell covering 2³ fine cells:

1. Coarse populations streaming into the fine region are "exploded": each of the 8 fine cells gets the same
   post-collision value, which is homogeneous and conserves mass exactly.
2. Fine populations streaming out are "coalesced" by averaging the 8 fine values.
3. Fine cells collide with τ_f.
4. The non-equilibrium rescaling can be added on top for accuracy.

The scheme fits the engine's voxel data naturally and is simpler to make bug-free than vertex schemes.

#### c) Block structure

Use blocks of fixed size (for example 16³ cells), each with a level. Every block is a dense SoA array, which keeps
the kernels identical to the uniform kernel and is GPU-friendly. Only the interface exchange is special. Refine the
blocks that intersect the body's bounding box dilated by about 1 body length downstream and 0.3 L elsewhere.

**Recommendation:**

1. Implement 2 levels with Rohde's scheme plus non-equilibrium rescaling, on 16³ blocks.
2. Keep the coarse uniform solver as the reference test case.
3. Test: drag on a sphere or cube must match the uniform fine grid within about 1 %.

Effort is high (interfaces, time interpolation, parallel scheduling), but the payoff is the largest available for
coarse-grid accuracy.

### 1.5 Outlet, sponge and blockage

#### a) Convective outlet

A convective outlet reflects less than zero-gradient. Apply it to the populations that enter from the outlet:

```
f_i(x_N, t+1) = ( f_i(x_N, t) + U_c f_i(x_{N−1}, t+1) ) / (1 + U_c)
```

U_c is the mean outflow velocity in lattice units. This is the implicit first-order upwind discretization of
∂f/∂t + U_c ∂f/∂x = 0.

Reference: Lou, Guo & Shi (2013), "Evaluation of outflow boundary conditions for two-phase lattice Boltzmann
equation", *Phys. Rev. E* 87:063301 (unverified), which compares Neumann, convective and extrapolation outlets.

#### b) Sponge (absorbing) layer

Over the last N_s ≈ 8–20 cells before the outlet, and optionally on the lateral boundaries, add a relaxation toward
the free-stream equilibrium after collision:

```
f_i ← f_i − σ(x) [ f_i − feq_i(ρ₀, U∞) ],   σ(x) = σ_max ((x − x_s)/N_s)²,   σ_max ≈ 0.1–0.3
```

Alternatively, ramp up the viscosity in the layer.

References:

- Israeli & Orszag (1981), "Approximation of radiation boundary conditions", *J. Comput. Phys.* 41:115–135
  (the concept).
- Xu & Sagaut (2013), "Analysis of the absorbing layers for the weakly-compressible lattice Boltzmann methods",
  *J. Comput. Phys.* 245:14–42 (unverified). It gives an LBM-specific analysis and tuning.

The layer removes the reflected pressure waves that bias the time-averaged surface density. That bias matters for
the engine's pressure-drag estimate, because pressure is read from density.

#### c) Blockage

Periodic sides are equivalent to an infinite array of bodies. For a body that is symmetric about the lateral
mid-planes, that is the same as free-slip walls, so closed-tunnel blockage corrections apply.

- **Maskell (1963/65)**, "A theory of the blockage effects on bluff bodies and stalled wings in a closed wind
  tunnel", ARC R&M 3400 (public on DTIC, ADA955243). With S the body's frontal area and C the tunnel cross-section:

  ```
  q_c / q_u = 1 + θ C_Du S/C,   θ ≈ 5/2 for 3-D bluff bodies (check the report's θ for plates against prisms)
  C_Dc = C_Du / (1 + θ C_Du S/C)
  ```

- **Barlow, Rae & Pope (1999)**, *Low-Speed Wind Tunnel Testing*, 3rd ed., Wiley, ch. 10 (the Pope & Harper
  lineage). Solid blockage is ε_sb = K τ V / C^{3/2} (body volume V, shape factors K and τ). Wake blockage is
  ε_wb = (S/4C) C_Du. Then q_c = q_u (1 + ε_sb + ε_wb)². Take the exact constants from the book.

- **Practical rule:** keep S/C ≤ 3 % where possible. Because the engine's grid is sized by frontal cells, check the
  lateral domain size. At 5 % blockage the uncorrected Cd of a plate is about 12 % too high. Report both the
  corrected and the uncorrected value.

### 1.6 Getting Cd closer at coarse grids: a combined recipe

1. **Reference values:** compare against references at a matching regime. The plate's 1.17 and the cube's 1.05 are
   high-Re values (Hoerner 1965, *Fluid-Dynamic Drag*). Sharp-edged bodies are nearly Re-independent above roughly
   Re 10³. At 12 cells the Re cap is 480, close to that limit, so expect a few percent difference from Re alone.
2. **Plate thickness:** a thin plate voxelizes to at least 1 cell thick, so at 12 cells across the plate is about
   8 % of its width thick. Real thin plates have sharper separation. Bouzidi with q from the mesh helps here.
   Alternatively, voxelize "thin" features so they occupy exactly one cell of solid with q-based links on both faces.
3. **Sub-grid model:** reduce Smagorinsky over-dissipation (§1.3). For the plate this is likely the main driver of
   the low Cd.
4. **Domain effects:** sponge outlet plus blockage correction (§1.5).
5. **Richardson extrapolation:** use the existing 16/24/32/48 ladder (Roache 1994, "Perspective: a method for
   uniform reporting of grid refinement studies", *J. Fluids Eng.* 116:405–413; Roache 1998, *Verification and
   Validation in Computational Science and Engineering*, Hermosa). With grid sizes h₁ < h₂ < h₃ and a constant ratio
   r:

   ```
   p = ln((Cd₃ − Cd₂)/(Cd₂ − Cd₁)) / ln r
   Cd* = Cd₁ + (Cd₁ − Cd₂)/(r^p − 1)
   GCI = 1.25 |Cd₁ − Cd₂| / (Cd₁ (r^p − 1))
   ```

   Staircase convergence is often non-monotone. If p falls outside [0.5, 3], report the finest value plus the GCI
   instead of extrapolating. Once the Bouzidi wall is in, convergence becomes smoother and the extrapolation more
   reliable.
6. **Averaging window:** average over an integer number of shedding periods. Detect the Strouhal frequency from zero
   crossings of the lateral force, or average over at least 2 flow-throughs after the transient.

---

## 2. LBM speed

### 2.1 Bandwidth model: what the limit is

LBM is memory-bound. The rule of thumb is MLUPS ≈ achieved bandwidth / bytes per cell update. For D3Q19:

| Scheme | Bytes per cell per step (FP32) | Memory per cell |
|---|---|---|
| Two grids, push, normal stores (CPU) | 19×4 read + 19×4 write + 19×4 write-allocate = **228 B** | 152 B |
| Two grids, non-temporal stores | 152 B | 152 B |
| In-place: AA, Esoteric Twist, Esoteric Pull | **152 B** | **76 B** |
| In-place + FP16 storage | **76 B** | **38 B** |

Add 1–4 B per cell for flags if they are stored densely.

Example: a desktop with dual-channel DDR5 sustains about 60–80 GB/s, so the limit is about 400–500 MLUPS for in-place
FP32 against about 300 for two-grid push. A consumer GPU at 500–1000 GB/s gives about 3000–6500 MLUPS (FP32) and about
6000–13000 (FP16 storage). These are roofline estimates, not measurements. Lehmann's papers report implementations
that reach a large fraction of this.

### 2.2 In-place streaming

- **AA pattern:** Bailey, Myre, Walsh, Lilja & Saar (2009), "Accelerating lattice Boltzmann fluid flows using
  graphics processors", *Proc. ICPP 2009* (title unverified).
- **Comparison of patterns:** Wittmann, Zeiser, Hager & Wellein (2013), "Comparison of different propagation steps
  for lattice Boltzmann methods", *Comput. Math. Appl.* 65(6):924–935, DOI 10.1016/j.camwa.2012.05.002 (arXiv
  1111.0922).

The AA pattern alternates two kinds of steps:

```
even step: read f_i(x), collide, write post-collision f_i into slot ī at x (reversed directions, same cell)
odd step:  read f_i from neighbor x − c_i (slot ī there), collide, write into neighbor x + c_i slots (normal order)
```

Each step touches only one cell's set of slots. It is thread-safe with one array, and the code needs two kernel
variants. Boundary handling must know which parity is active.

- **Esoteric Twist:** Geier & Schönherr (2017), "Esoteric Twist: an efficient in-place streaming algorithm for the
  lattice Boltzmann method on massively parallel hardware", *Computation* 5(2):19, DOI 10.3390/computation5020019.
  Each cell reads and writes populations at the cell itself and at its neighbors in the positive directions, and the
  roles alternate with a "twist" on every step. There is a single kernel, and indirect addressing is minimal.
- **Esoteric Pull / Esoteric Push:** Lehmann (2022), "Esoteric Pull and Esoteric Push: two simple in-place streaming
  schemes for the lattice Boltzmann method on GPUs", *Computation* 10(6):92, DOI 10.3390/computation10060092. These
  simplify Esoteric Twist into a pull or push form. There is one kernel, a step-parity flag decides index swaps, and
  the memory footprint is halved. They work with arbitrary boundary handling in the same kernel.
- **Recommendation:** Esoteric Pull for the GPU, since its read pattern is better for coalescing. On the CPU, either
  AA or Esoteric Pull. Use the same code structure on both, so the CPU stays the reference for the GPU tests (a
  project rule).

### 2.3 Floating-point storage

Lehmann, Krause, Amati, Sega, Harting & Gekle (2022), "Accuracy and performance of the lattice Boltzmann method with
64-bit, 32-bit, and customized 16-bit number formats", *Phys. Rev. E* 106:015308,
DOI 10.1103/PhysRevE.106.015308.

- **DDF shifting:** store f_i − w_i (with ρ₀ = 1) instead of f_i, and compute ρ − 1 from the sum. The stored values
  are then small numbers around 0, so FP32 loses fewer significant bits. Do this now, even in FP32. It costs nothing
  and noticeably improves accuracy at low Ma.
- **FP16 storage:** compute in FP32 and store in 16 bits. The paper defines two custom 16-bit formats, a modified
  IEEE half with a re-centered exponent range and a posit-like format, tailored to the small range of shifted
  populations.
  - FP32 against FP64: negligible difference in almost all their tests.
  - 16-bit: sufficient for many cases, with significant speedup.
  - Plain IEEE FP16 needs scaling: multiply the shifted population by a constant before conversion.
- **Vulkan:** 16-bit storage buffer access is provided by `VK_KHR_16bit_storage` (core in Vulkan 1.1, feature
  `storageBuffer16BitAccess`), so FP16 storage is spec-compliant without FP16 arithmetic.
- **Recommendation:** shift now. Add FP16 as an option and validate Cd against FP32. Accept it if |ΔCd| < 0.5 %.

### 2.4 CPU vectorization and threading

- **SIMD over x:** with SoA and x as the fastest index, one AVX2 vector handles 8 consecutive cells. Collision is
  pure arithmetic, so it vectorizes trivially. Streaming in a pull or in-place form becomes unaligned contiguous
  loads.
- **Branches:** avoid per-cell branches on flags. Use masks or blends, or keep solid cells in the stream and
  overwrite them, then handle boundary links in a separate sparse loop over the precomputed link list (§1.2a).
- **Threads:** use TBB `parallel_for` over z-slabs (or y-z pencils) with a grain size of a few pencils. On
  multi-socket machines, initialize with the same partition (first-touch).
- **Stores:** with a two-grid scheme, use non-temporal (streaming) stores to avoid the write-allocate traffic.
  That alone saves about 33 % of the traffic.
- **Fusion:** fuse collision and streaming in one sweep. Accumulate the Cd force within the same sweep for the
  boundary cells only.

### 2.5 Fewer time steps

1. **Higher lattice velocity:** the compressibility error scales as Ma² (Ma = U/c_s). U_lat = 0.1 (Ma ≈ 0.17) gives
   about 1 % error and twice fewer steps than U = 0.05. TRT or regularized collision keeps this stable. Krüger et
   al. (2017), *The Lattice Boltzmann Method: Principles and Practice*, Springer, DOI 10.1007/978-3-319-44649-3,
   ch. 7, discusses choosing parameters via Ma and τ.
2. **Coarse-to-fine initialization:**
   1. Run the N/2 grid to a near-steady state.
   2. Trilinearly interpolate ρ and u onto the N grid.
   3. Initialize f_i = feq_i(ρ, u) + f_i^neq. The non-equilibrium part comes from the Chapman–Enskog estimate
      f_i^neq ≈ −(w_i ρ τ / c_s²) Q_iαβ ∂_α u_β, with Q_iαβ = c_iα c_iβ − c_s² δ_αβ. This avoids the initial
      pressure shock.

   References: Skordos (1993), "Initial and boundary conditions for the lattice Boltzmann method", *Phys. Rev. E*
   48:4823; Mei, Luo, Lallemand & d'Humières (2006), "Consistent initial conditions for lattice Boltzmann
   simulations", *Comput. Fluids* 35:855–862.

   Expected gain: the fine grid skips most of its 1–2 flow-through transient, so about 2–3× fewer fine steps. The
   ladder 16 → 24 → 32 → 48 already exists, so chain it.
3. **Early stopping by Cd convergence:**
   - Keep a running average of Cd over windows of W steps, with W one shedding period or about 0.25 flow-through
     if no shedding is detected.
   - Stop when |C̄d_k − C̄d_{k−1}| / C̄d_k < ε for 2–3 consecutive windows, with ε = 0.3–1 % depending on the
     quality preset.
   - Also stop if the lateral-force signal is periodic with a stable mean.
   - Always enforce a minimum of about 1.5 flow-throughs, so the initial wave has left the domain.
4. **Potential-flow initial guess:** a cheaper alternative to coarse-to-fine for the very first grid. Also rescale
   the time step so that one "flow-through" is defined from the body length, not the domain length. The wake needs
   only about 5–10 body lengths behind the body to settle the pressure drag.

---

## 3. FEA accuracy on voxel meshes

### 3.1 Element formulation: locking

At 2 voxels per line width, a single wall or perimeter is 2 elements thick. A fully integrated trilinear hex8 is
then far too stiff in bending (shear locking, parasitic shear). Volumetric locking (ν → 0.5) is a minor issue for
printed thermoplastics (ν ≈ 0.33–0.40).

#### a) Incompatible modes / enhanced assumed strain. Recommended.

- Wilson–Taylor element: Taylor, Beresford & Wilson (1976), "A non-conforming element for stress analysis",
  *Int. J. Numer. Meth. Eng.* 10:1211–1219 (unverified).
- EAS: Simo & Rifai (1990), "A class of mixed assumed strain methods and the method of incompatible modes",
  *Int. J. Numer. Meth. Eng.* 29:1595–1638 (unverified).

Steps:

1. Add 9 internal displacement modes per element (3 per direction, of the form 1 − ξ², 1 − η², 1 − ζ²).
2. Build K_e = [K_uu K_uα; K_αu K_αα].
3. Condense statically: K̃_e = K_uu − K_uα K_αα⁻¹ K_αu.

All voxels are identical cubes, so K̃_e depends only on the material matrix C. Precompute it per material ID
(§4.1). The cost in the solver is then **zero**, because K̃_e is still a 24×24 matrix.

- **Accuracy:** for rectangular (voxel) elements the formulation reproduces pure bending exactly. A full-integration
  hex8 with 1–2 elements through the thickness is too stiff by tens of percent, and the exact amount depends on the
  aspect ratio of the bent member.
- **Stress recovery:** recover the internal modes element by element after the solve to get stresses.

#### b) B-bar and selective reduced integration

Hughes (1980), "Generalization of selective integration procedures to anisotropic and nonlinear media",
*Int. J. Numer. Meth. Eng.* 15:1413–1418, DOI 10.1002/nme.1620150914.

The method replaces the volumetric part of B with its element average, which fixes volumetric locking. It does
**not** fix shear locking in bending, and it is anisotropy-aware (that is what the paper generalizes).

- **Recommendation:** low priority for these materials.

#### c) One-point integration with hourglass control

Flanagan & Belytschko (1981), "A uniform strain hexahedron and quadrilateral with orthogonal hourglass control",
*Int. J. Numer. Meth. Eng.* 17:679–706.

It is cheap and free of locking, but it needs a tuned hourglass stiffness, and the stresses are only at the
centroid. For linear static analysis, incompatible modes are cleaner.

### 3.2 Staircase stress

A voxel boundary has re-entrant corners at every step. The pointwise stress at surface voxels oscillates and does
not converge as h → 0: the corner singularity is a mesh artifact. A per-voxel max-stress or Tsai-Wu safety factor
therefore picks up mesh noise.

#### a) Evaluate stress where it is best

The element centroid is the optimal (Barlow) point for the trilinear hex8: Barlow (1976), "Optimal stress
locations in finite element models", *Int. J. Numer. Meth. Eng.* 10:243–251. Use centroid stress, not
Gauss-point or nodal extrapolation, for per-voxel failure.

#### b) Superconvergent patch recovery (SPR)

Zienkiewicz & Zhu (1992), "The superconvergent patch recovery and a posteriori error estimates. Part 1: The
recovery technique", *Int. J. Numer. Meth. Eng.* 33(7):1331–1364, DOI 10.1002/nme.1620330702.

1. For each node, take the patch of elements around it: up to 8 voxels.
2. Least-squares fit a linear polynomial σ*(x) = P(x)a to the centroid stresses of the patch, separately for each
   of the 6 components.
3. Evaluate the polynomial at the node.
4. For surface nodes, use interior patches.

The result is a continuous stress field that is smoother and more accurate, and it gives an error estimator
‖σ* − σ_h‖ per element for free. That estimator is useful for deciding where to refine.

#### c) Critical-distance averaging for failure. Recommended for the safety factor.

Taylor (2007), *The Theory of Critical Distances: A New Perspective in Fracture Mechanics*, Elsevier.

The method evaluates the failure criterion on stress averaged over a material length L, not at a point:

- **Point method:** stress at distance L/2 from the hot spot.
- **Volume method:** average over a sphere of radius about 0.54 L in 3-D, or a line of length 2L.
- **L from material data:** L = (1/π)(K_IC / σ_u)².
- **For printed parts:** L of the order of the line width or the layer height is a physically motivated choice,
  because the bead is the material's structural unit.

The averaging is a cheap convolution on the voxel grid (a box or Gaussian kernel of radius about 1 line width over
solid voxels only). It removes the staircase spikes without changing the mesh.

- **Expected effect:** the safety factor becomes mesh-converged instead of decreasing indefinitely with resolution.
- **Calibration:** calibrate the kernel radius once against notched test coupons.

#### d) Geometry smoothing

- Camacho, Hopper, Lin & Myers (1997), "An improved method for finite element mesh generation of geometrically
  complex structures with application to the skullbase", *J. Biomech.* 30(10):1067–1070. Smoothing reduced the
  surface von Mises RMS error from 0.11–0.24 to 0.08–0.15 MPa and cut element-to-element oscillation about 3×.
- Boyd & Müller (2006), "Smooth surface meshing for automated finite element model generation from 3D image data",
  *J. Biomech.*

These move the surface nodes. That breaks "all elements are identical cubes", so per-element K_e and the matrix-free
speed are lost on the boundary layer.

- **Recommendation:** use only for a "high quality" preset, and only for the surface layer, with stored per-element
  K_e for the moved elements.

#### e) Finite cell method (FCM) and CutFEM: keep the grid, integrate exactly

- Parvizian, Düster & Rank (2007), "Finite cell method: h- and p-extension for embedded domain problems in solid
  mechanics", *Comput. Mech.* 41:121–133, DOI 10.1007/s00466-007-0173-y.
- Burman, Claus, Hansbo, Larson & Massing (2015), "CutFEM: discretizing geometry and partial differential
  equations", *Int. J. Numer. Meth. Eng.* 104:472–501, DOI 10.1002/nme.4823 (unverified). It adds a ghost penalty
  for conditioning.

**FCM-lite for the engine:**

1. Keep the voxel grid as the analysis grid, but use a background grid one level coarser than the geometry voxels.
2. Integrate each cut cell over its true material, by octree sub-cells of depth 2–3 or directly from the geometry
   voxels.
3. Give void a stiffness factor α ≈ 10⁻⁶–10⁻⁸ (to avoid singularity), or drop the void nodes.

The simplest form, which costs almost nothing, is **volume-fraction scaling**: K_e,cut = φ_e K_e, with φ_e the
solid fraction of the voxel computed from the mesh. That is FCM with one-point geometry quadrature.

- **Expected effect:** compliance converges smoothly instead of in jumps. Boundary stresses are still noisy and need
  (b) or (c).
- **Effort:** medium for the volume-fraction form, high for true sub-cell quadrature.

### 3.3 Homogenization of printed infill

**Why:** at 2–6 voxels per line width, a sparse infill region is resolved bead by bead. That is accurate but
expensive. Replacing the infill interior with an equivalent homogeneous orthotropic material allows much coarser
voxels there (§4.4).

**Methods:**

- Andreassen & Andreasen (2014), "How to determine composite material properties using numerical homogenization",
  *Comput. Mater. Sci.* 83:488–495, DOI 10.1016/j.commatsci.2013.09.006 (unverified). It is implementation-oriented,
  works on voxel grids, and uses periodic boundary conditions with six unit strain load cases.
- Hollister & Kikuchi (1992), "A comparison of homogenization and standard mechanics analyses for periodic porous
  composites", *Comput. Mech.* 10:73–95.
- FDM-specific: Bean, Lopez-Anido & Vel (2022), "Numerical modeling and experimental investigation of effective
  elastic properties of the 3D printed gyroid infill", *Appl. Sci.* 12(4):2180. It uses an RVE with periodic
  boundary conditions, validates E and ν against compression tests, finds the gyroid nearly isotropic, and fits
  density-dependent formulas. The same group (Bhandari & Lopez-Anido) homogenizes rectilinear infill into
  orthotropic properties.

**Algorithm:**

1. Build an RVE voxel model of one period of the pattern, at the given density and angle, with the engine's per-bead
   orthotropic material.
2. Solve 6 problems with periodic boundary conditions. Each applies a unit macroscopic strain ε⁰⁽ᵏˡ⁾ with
   u = ε⁰x + ũ, where ũ is periodic.
3. Compute C^H_ijkl = (1/|Y|) ∫_Y σ_ij(ε⁰⁽ᵏˡ⁾ + ε(ũ)) dY.
4. Cache C^H by key (pattern, density, layer angle(s), bead material).
5. Store the localization tensors, the micro-strain per unit macro-strain. They let the engine recover the bead
   stresses for failure from the macro strain ("de-homogenization"). This is important: the failure criterion is
   still applied at bead level.

The engine's own GMG-CG solves the RVE problems, so no new solver is needed.

**Bead-level material data and criteria:**

- Rodríguez, Thomas & Renaud (2001, 2003), *Rapid Prototyping J.* 7(3):148–158 and 9(4):219–230. Experiments and
  models for ABS FDM, transversely isotropic.
- Ahn, Montero, Odell, Roundy & Wright (2002), "Anisotropic material properties of fused deposition modeling ABS",
  *Rapid Prototyping J.* 8(4):248–257. It uses Tsai-Wu for raster-angle-dependent strength.
- Popa, Galatanu & Marsavina (2025), *Frattura ed Integrità Strutturale*: Tsai-Hill prediction of FDM tensile
  strength at several raster angles.

### 3.4 Validation (proposed)

There is little literature on "voxel FEA for FDM parts" as such. The validation set below is a proposal, not taken
from a paper.

1. **Analytical:**
   - Cantilever (Euler–Bernoulli plus a Timoshenko correction), for locking.
   - Plate with a hole (Kirsch, K_t = 3), for staircase stress and critical-distance averaging.
   - Patch test, for the element.
   - Run each at 2/4/6 voxels per line width and report the convergence order.
2. **RVE:**
   - Homogenized C^H against direct full-resolution solves of a block of infill.
   - Against Bean et al. (2022) for the gyroid.
3. **Experimental (black-box):** ASTM D638 coupons at 0°/45°/90° raster, a three-point bend, and an open-hole
   tensile coupon. Compare stiffness (expect about 10 % agreement) and failure load (expect about 20–30 %; FDM
   scatter is large).

---

## 4. FEA speed

### 4.1 Matrix-free operator on a uniform grid

All elements are equal cubes, so K_e depends only on the 6×6 material matrix C_e:

```
K_e = Σ_{ab} C_e,ab · K^(ab)
```

The K^(ab) are 21 precomputed 24×24 basis matrices, one per independent entry of symmetric C (3×3 quadrature,
computed once).

There are two ways to store it:

- **Per material ID:** voxels share a small set of (material, orientation) combinations. Quantize the bead angle to
  1°–2°: 180 angles × a few materials × 300 unique entries of the symmetric 24×24 matrix fits easily in cache or GPU
  constant memory.
- **On the fly:** if orientations are continuous, assemble K_e from C_e and the 21 basis matrices on the fly. That
  is more arithmetic but no memory, which suits the GPU.

**Wu, Dick & Westermann (2016)**, "A system for high-resolution topology optimization", *IEEE TVCG* 22(3):1195–1208,
DOI 10.1109/TVCG.2015.2502588:

- Matrix-free GMG-preconditioned CG on the GPU for voxel hex8 elasticity.
- **Node-based gather:** one thread per node computes the 3 rows of y = Kx by gathering from its ≤8 adjacent
  elements and the 27 neighboring nodes. That avoids atomics, it is deterministic, and it coalesces with an x-fastest
  layout.
- Reported: millions of elements per second on a single GPU, with high-resolution designs solved in minutes.
- **Recommendation:** use the same gather structure on the CPU (TBB over z-slabs, SIMD over x). The CPU then stays
  the bit-comparable reference for the GPU kernel.

### 4.2 Multigrid design for printed parts

- **Coarse operators:** rediscretizing coarse levels (averaging the material) misses thin walls and sparse infill.
  A wall 2 voxels thick vanishes after one coarsening, and the convergence collapses. **Galerkin coarse operators**
  (A_c = R A_f P, with trilinear P and R = Pᵀ) capture them. The first coarse level can be built matrix-free per
  coarse element as Σ Pᵀ K_e P over its 8 children, giving 24×24 per coarse element. Deeper levels are assembled.
  - Aage, Andreassen, Lazarov & Sigmund (2017), "Giga-voxel computational morphogenesis for structural design",
    *Nature* 550:84–86, DOI 10.1038/nature23911. They use Galerkin-based GMG for about 10⁹ voxels.
  - Arbenz, van Lenthe, Mennel, Müller & Sala (2007), "A scalable multi-level preconditioner for matrix-free µ-finite
    element analysis of human bone structures", *Int. J. Numer. Meth. Eng.* 73(7):927–947, DOI 10.1002/nme.2101.
    This is voxel hex8 with very irregular, thin, bone-like geometry, which is close to infill, with matrix-free
    fine-level operators and algebraic or aggregation coarse levels.
- **Smoother:** Chebyshev polynomial of degree 2–4 on the Jacobi-preconditioned operator, targeting the upper part
  of the spectrum [λ_max/α, 1.1 λ_max] with α ≈ 10–30. Estimate λ_max with about 10 power or CG iterations.
  - Adams, Brezina, Hu & Tuminaro (2003), "Parallel multigrid smoothing: polynomial versus Gauss–Seidel",
    *J. Comput. Phys.* 188:593–610.
  - The result is fully parallel, deterministic and symmetric, so it is valid inside CG. Use the same smoother
    before and after (a symmetric V-cycle).
  - Damped Jacobi (ω ≈ 0.5–0.6 for 3-D hex8 elasticity) is the simple fallback.
- **Coarsest level:** a sparse direct Cholesky, or many CG iterations, on a grid of about 10³–10⁴ nodes. Remove
  rigid-body modes in floating components (bodies not connected to supports): detect connected components and either
  fix them or warn the user.
- **Void:** do not use "soft" void elements. Drop void voxels, and their nodes, from the active set.
  - Liu, Hu, Zhu, Matusik & Sifakis (2018), "Narrow-band topology optimization on a sparsely populated grid",
    *ACM Trans. Graph.* 37(6), DOI 10.1145/3272127.3275012. Only active voxels are stored, using a sparse paged grid
    (SPGrid), and a multigrid-preconditioned solver handles more than 10⁹ voxels on one workstation. The page-based
    sparse layout fits printed parts, which are mostly air inside the bounding box.

### 4.3 Precision and tolerances

**Mixed precision:** Göddeke, Strzodka & Turek (2007), "Performance and accuracy of hardware-oriented native-,
emulated- and mixed-precision solvers in FEM simulations", *Int. J. Parallel Emergent Distrib. Syst.* 22(4):221–256,
DOI 10.1080/17445760601122076. With mixed precision (low-precision inner solve, high-precision residual and update)
they report 4–5× speedups with the same final accuracy as full double precision.

Recipe for the engine:

1. Keep the CG vectors, dot products and the residual in FP64 (or FP32 with compensated summation for the dots).
2. Run the V-cycle preconditioner fully in FP32.
3. Use flexible CG if the preconditioner is not exactly symmetric in floating point. Otherwise standard PCG is fine.

**Tolerance:**

- Stresses are derivatives of displacements, so they need a tighter solve than compliance. For an engineering
  safety factor, a relative residual ‖r‖/‖f‖ ≤ 10⁻⁵ is a sound default; this is a recommendation, not a published
  number.
- With a good GMG this costs about 10–30 PCG iterations, nearly independent of size.
- An adaptive check is better: stop when the minimum safety factor and its location change by less than 0.5 %
  between successive checks every 5 iterations, but never above 10⁻³ relative residual.

**Warm start:** start each load case from the previous solution, or from the coarser resolution's result,
prolongated (nested iteration / full multigrid). That saves about 30–50 % of the iterations when sweeping
resolutions 2 → 4 → 6.

**Multiple load cases:** solve them simultaneously (block CG, or a batched operator applying K to several vectors
per pass). The operator is bandwidth-bound, so 3–6 right-hand sides cost about 1.5–2× one.

### 4.4 Adaptive and octree voxels

- **Interior coarsening:** in the solid interior and the homogenized infill (§3.3), use voxels 2–8× larger. Fine
  voxels stay only within a few line widths of the surface and in perimeters. Fine–coarse interfaces need
  hanging-node constraints, which eliminate the hanging DOFs by trilinear interpolation of the coarse nodes.
- **Blocks:** a simpler alternative to octrees is the block-structured approach from §1.4 (16³ blocks with levels),
  which keeps the kernels dense.
- **Error indicator:** the SPR estimator (§3.2b) can decide which blocks to refine.
- **Expected gain:** for typical parts with sparse infill, a 5–20× reduction in DOFs.
- **Effort:** high. Do it after homogenization, which delivers most of the gain with the uniform grid kept per
  region.

---

## 5. Ranked implementation lists

### 5.1 LBM accuracy

1. **TRT, Λ = 3/16.** Effort: hours. Formulas in §1.1a. Test: Poiseuille channel, wall position independent of τ.
2. **Momentum-exchange force with the pressure/viscous split.** Effort: about 1 day. §1.2d.
3. **Sub-grid model check:** lower C_s or switch to Vreman. Measure the plate. Effort: about 1 day. §1.3.
4. **Sponge outlet + Maskell correction + Richardson with GCI.** Effort: about 1–2 days. §1.5, §1.6.
5. **Bouzidi interpolated bounce-back with mesh-derived q.** Effort: about 3–5 days, including the link list.
   §1.2a. Test: sphere Cd against Schiller–Naumann at Re 100–300.
6. **Regularized collision with closed-form Smagorinsky.** Effort: about 2 days. §1.1b.
7. **2-level block refinement (Rohde).** Effort: 2–3 weeks. §1.4.
8. Multireflection, then RR/HRR, then cumulant or KBC with D3Q27. Only if needed.

### 5.2 LBM speed

1. **Shifted populations** (f − w). Effort: hours. Also an accuracy gain.
2. **Early stopping + coarse-to-fine initialization + U_lat ≈ 0.1.** Effort: about 2 days. Expect 2–4× in total
   time.
3. **In-place streaming (Esoteric Pull or AA)** with fused collide-stream and a sparse boundary-link list. Effort:
   about 1 week. Expect 1.3–1.5× on CPU and 2× less memory.
4. **SIMD over x**, branch-free collision. Effort: 2–3 days. Expect 1.5–3× on CPU if not yet vectorized.
5. **GPU (Vulkan) with Esoteric Pull and FP16 storage** (`storageBuffer16BitAccess`). Expect 10–30× over a desktop
   CPU, from the bandwidth ratio.
6. Grid refinement (also a speed gain, §1.4).

### 5.3 FEA accuracy

1. **Centroid stress + critical-distance averaging for the safety factor.** Effort: 1–2 days. §3.2a, §3.2c.
2. **Incompatible-modes hex8** (statically condensed, precomputed per material). Effort: 2–4 days. §3.1a. Test:
   cantilever at 2 voxels thick.
3. **SPR nodal recovery + error estimator.** Effort: 2–3 days. §3.2b.
4. **Volume-fraction boundary voxels** (FCM-lite). Effort: 2–3 days. §3.2e.
5. **RVE homogenization with localization tensors.** Effort: 1–2 weeks. §3.3.
6. The validation suite (§3.4), run continuously in CI with recorded numbers.

### 5.4 FEA speed

1. **Mixed precision + residual tolerance 10⁻⁵ + warm start / nested iteration.** Effort: 1–2 days. Expect 1.5–3×.
2. **Galerkin coarse levels + Chebyshev smoother + active-set (no soft void).** Effort: about 1 week. Iteration
   counts become robust for thin walls, which can be a 3–10× gain on hard parts.
3. **Node-gather matrix-free operator, SIMD/TBB on CPU.** Effort: about 1 week.
4. **Batched load cases.** Effort: 2–3 days.
5. **GPU matrix-free GMG-PCG** (Wu et al. 2016 structure) in Vulkan compute, tested against the CPU. Effort: 2–4
   weeks.
6. **Homogenized infill blocks / adaptive coarsening.** Effort: 2–4 weeks. Expect a 5–20× DOF reduction.

---

## 6. Notes and caveats

- **Expected gains:** the figures above are estimates from the cited papers' reported trends, or from the bandwidth
  arithmetic in §2.1. None was measured on this engine. Every item must be benchmarked with the project's benchmark
  set before and after.
- **Unverified citations:** items marked "(unverified)" had their DOI or exact pages not confirmed in this search.
  Check them before recording them in `PROVENANCE.md`.
- **GPL-licensed codes:** several of the cited methods have well-known implementations in GPL-licensed codes. Per
  the clean-room rule, implement only from the papers listed here.

## Sources consulted in this search (web)

- Lehmann 2022, Esoteric Pull/Push: https://www.mdpi.com/2079-3197/10/6/92
- Lehmann et al. 2022, number formats: https://epub.uni-bayreuth.de/6559/1/EN11981.pdf
- Geier & Schönherr 2017, Esoteric Twist: https://mdpi.com/2079-3197/5/2/19
- Wittmann et al. 2013: https://arxiv.org/pdf/1111.0922
- Wu, Dick & Westermann 2016: https://www.cs.cit.tum.de/cg/research/publications/2015/high-resolution-topology-optimization
- Aage et al. 2017: https://www.nature.com/nature/journal/v550/n7674/pdf/nature23911.pdf
- Liu et al. 2018: https://cdfg.csail.mit.edu/publications/narrow-band-topology-optimization-sparsely-populated-grid
- Malaspinas 2015: https://arxiv.org/pdf/1505.06900 ; Coreixas et al. 2017: https://arxiv.org/abs/1704.04413v3
- Geier et al. 2015 cumulant: https://doi.org/10.1016/j.camwa.2015.05.001
- Karlin, Bösch & Chikatamarla 2014: https://link.aps.org/doi/10.1103/PhysRevE.90.031302
- Bouzidi et al. 2001: https://www.citedrive.com/en/discovery/momentum-transfer-of-a-boltzmann-lattice-fluid-with-boundaries/
- Ginzburg & d'Humières 2003: https://hal.inrae.fr/hal-02582796v1
- Ginzburg, Verhaeghe & d'Humières 2008 (TRT): https://global-sci.org/intro/article_detail/auth/7862.html
- Guo et al. 2014 wall model: https://dblp.org/pid/60/769
- Haussmann et al. 2019: https://publikationen.bibliothek.kit.edu/1000095702
- Filippova & Hänel 1998: https://portal.mardi4nfdi.de/wiki/Publication:1285825
- Dupuis & Chopard 2003: https://link.aps.org/doi/10.1103/PhysRevE.67.066707
- Wen et al. 2014: https://arxiv.org/pdf/1303.0625
- Maskell 1963: https://apps.dtic.mil/sti/html/tr/ADA955243/index.html
- Camacho et al. 1997: https://scholars.duke.edu/individual/pub701806
- Parvizian, Düster & Rank 2007: https://portal.mardi4nfdi.de/wiki/Publication:1021051
- Zienkiewicz & Zhu 1992: https://zendy.io/title/10.1002/nme.1620330702
- Arbenz et al. 2007: https://www.research-collection.ethz.ch/handle/20.500.11850/4230?show=full
- Göddeke, Strzodka & Turek 2007: https://hgpu.org/?p=3041
- Bean, Lopez-Anido & Vel 2022: https://www.mdpi.com/2076-3417/12/4/2180
- Popa, Galatanu & Marsavina 2025: https://fracturae.com/index.php/fis/article/view/5416
