# Tisma engine — notes for Claude

A slicing engine written from scratch for Tisma, the first step of the owner's final goal: rebuild as much of Tisma
Slicer as possible in a clean room. Everything Tisma Slicer does reaches this repository as functional specifications
(`docs/spec/`, feature specs in `docs/spec/features/`), never as code. Read `CLEANROOM.md` and `PLAN.md` before any
work.

## Clean room (mandatory)

- Never read, copy or translate the source of PrusaSlicer, Slic3r, SuperSlicer, OrcaSlicer, Bambu Studio, Cura,
  CuraEngine, Tisma Slicer (`adriangrochi-code/Nuevo-slicer-basado-en-prusa-`) or any other GPL/AGPL code.
  - This includes the history of `imported/` and `docs/tisma/` in this repository (commits `4768a57` and `759eccd`,
    removed on 2026-10-24): never read it, by `git log`, `git show` or any other way.
  - Do not clone them.
  - Do not open them on GitHub.
  - Do not search their code.
- Work from `docs/spec/` (what to build, including the feature specs), papers, public format specifications and
  permissive libraries only. Web search for papers, specifications and user documentation is
  allowed. Every library is recorded in
  `THIRD_PARTY.md` (name, version, license, URL) before it is used.
- Every module records where it comes from in `PROVENANCE.md`: paper and section, specification, or own design.
- Other slicers may only be compared as black boxes: run their program, compare outputs.
- Never rewrite or squash history: it is the record of provenance.

## Hand-over between the two teams (owner's decision)

The main Tisma session knows Tisma Slicer and has read PrusaSlicer and Cura. It tells the engine *what* to build;
the engine team builds it from scratch. See "Paso de información entre los dos equipos" in `CLEANROOM.md`:
- no code crosses, in files, messages or prompts; specs only, in `docs/spec/`, with the engine's own names;
- read a spec only if `tools/check_spec.py` passes on it; never read `imported/tisma/` or `docs/tisma/`;
- send the other team only behavior questions and black-box results.

## The license must stay closable (owner's decision)

The owner must always be able to move the engine to a closed, proprietary license. Never add anything that would
prevent it:
- no copied code from anywhere, not even small snippets (Stack Overflow is CC BY-SA);
- only permissive libraries (MIT, BSD, BSL-1.0, Apache-2.0, Zlib; MPL-2.0 only unmodified), checked at the exact
  version including what they bundle. `tools/check_licenses.py` enforces `THIRD_PARTY.md` in CI;
- no test data, fonts or images whose license forbids it;
- no contributions from third parties until a copyright assignment exists (`CONTRIBUTING.md`).

## Vision

The goal is non-planar and 5-axis slicing with GPU acceleration. The first, planar version is the base. From F1 on,
follow the rules in "Diseño para la visión" in `PLAN.md`:
- layers as level sets of a scalar field;
- 3D toolpaths with a nozzle orientation;
- GPU-ready data, with the CPU path as the reference.

## Working rules (from the project owner)

- Answer the owner in Spanish. Code, comments, commit messages and PR titles are in English.
- Conventional Commits.
- Build and test before reporting. Say plainly what was not tested or measured.
- No model identifiers (model ids or version names) in code, commits or PR text. The only exception is the
  attribution trailer the session requires.
- Performance matters:
  - parallel by layer (oneTBB);
  - GPU (Vulkan compute) from F1 on, in the kernels where it pays off, always tested against the CPU version;
  - Clipper2 for polygon operations;
  - contiguous data;
  - measure each phase with the benchmark set and record the numbers.

## Work blocks

Each scheduled block lasts about one hour. All blocks work on the branch `dev`, so each one continues where the last
one stopped.

1. Fetch `dev`. If it does not exist yet, create it from `main`. Otherwise merge `origin/main` into it (the owner adds
   specifications to `main`).
2. Read `docs/spec/README.md` and the feature specs in `docs/spec/features/` that the item needs, only if
   `tools/check_spec.py` passes on them.
3. Take the next unfinished item of `PLAN.md`. Continue the last one if `PROGRESS.md` says it is unfinished.
4. Implement it with tests, build, run the tests.
5. Update `PROVENANCE.md`, `PLAN.md` and `PROGRESS.md` (date, what was done, measurements, what is next).
6. Commit, push `dev`, and make sure a pull request from `dev` to `main` is open. Do not merge it.
   - The owner merges at the end of each phase.
   - The main Tisma session may review it, but only the behavior and the tests.
7. Stop at a point where the build and the tests pass, even if the item is unfinished. Say so in `PROGRESS.md`.

Every 8 hours, one block is a maintenance block instead (owner's decision, 2026-10-24): optimize, hunt bugs and
errors, and fix what is still unresolved, to polish the engine and keep it stable. What it covers is in
`docs/DEVELOPMENT.md`, "Mantenimiento".

## CI

- Every push runs the build and the tests on Windows.
- Linux builds run inside the work session (owner's decision, 2026-10-24): run `tools/ci_linux.sh` before every push.
  It runs the same steps as the Linux CI jobs, which on GitHub now only run when started by hand.
- macOS comes at the very end of the project, as the last phase of `PLAN.md` (owner's decision, 2026-10-24).
