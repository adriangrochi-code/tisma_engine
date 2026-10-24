# Tisma engine — notes for Claude

A slicing engine written from scratch for Tisma. Read `CLEANROOM.md` and `PLAN.md` before any work.

## Clean room (mandatory)

- Never read, copy or translate the source of PrusaSlicer, Slic3r, SuperSlicer, OrcaSlicer, Bambu Studio, Cura,
  CuraEngine, Tisma Slicer (`adriangrochi-code/Nuevo-slicer-basado-en-prusa-`) or any other GPL/AGPL code.
  - Do not clone them.
  - Do not open them on GitHub.
  - Do not search their code.
- Work from papers, public format specifications and permissive libraries only. Every library is recorded in
  `THIRD_PARTY.md` (name, version, license, URL) before it is used.
- Every module records where it comes from in `PROVENANCE.md`: paper and section, specification, or own design.
- Other slicers may only be compared as black boxes: run their program, compare outputs.
- Never rewrite or squash history: it is the record of provenance.

## Working rules (from the project owner)

- Answer the owner in Spanish. Code, comments, commit messages and PR titles are in English.
- Conventional Commits.
- Build and test before reporting. Say plainly what was not tested or measured.
- No model identifiers (model ids or version names) in code, commits or PR text. The only exception is the
  attribution trailer the session requires.
- Performance matters:
  - parallel by layer (oneTBB);
  - Clipper2 for polygon operations;
  - contiguous data;
  - measure each phase with the benchmark set and record the numbers.

## Work blocks

Each scheduled block lasts about one hour. All blocks work on the branch `dev`, so each one continues where the last
one stopped.

1. Fetch `dev`. If it does not exist yet, create it from `main`.
2. Take the next unfinished item of `PLAN.md`. Continue the last one if `PROGRESS.md` says it is unfinished.
3. Implement it with tests, build, run the tests.
4. Update `PROVENANCE.md`, `PLAN.md` and `PROGRESS.md` (date, what was done, measurements, what is next).
5. Commit, push `dev`, and make sure a pull request from `dev` to `main` is open. Do not merge it.
   - The owner merges at the end of each phase.
   - The main Tisma session may review it, but only the behavior and the tests.
6. Stop at a point where the build and the tests pass, even if the item is unfinished. Say so in `PROGRESS.md`.

## CI

- Every push runs the build and the tests on Linux and Windows.
- macOS comes at the end of the project, after F6 (owner's decision, 2026-10-24).
