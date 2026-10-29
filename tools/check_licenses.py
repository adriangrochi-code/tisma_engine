#!/usr/bin/env python3
"""Keeps the engine's license closable (see CLEANROOM.md).

Fails when:
- a CMake dependency (FetchContent_Declare or ExternalProject_Add) is not in the
  table of THIRD_PARTY.md, or its license there is not in the allowed list;
- a file of ours carries copyleft license text, which would mean copied code.

usage: check_licenses.py [repo_root]
"""

import pathlib
import re
import sys

ALLOWED = {"MIT", "BSD-2-Clause", "BSD-3-Clause", "BSL-1.0", "Apache-2.0", "Zlib", "MPL-2.0"}
OUR_DIRS = ["src", "apps", "tests", "bench", "tools"]
# Text that only appears in copyleft code. Kept split so this file does not match itself.
COPYLEFT = re.compile(
    "|".join([
        "GNU (Affero |Lesser |Library )?" + "General Public License",
        "SPDX-License-Identifier:\\s*(A|L)?" + "GPL",
        "Creative Commons Attribution-" + "ShareAlike",
    ]),
    re.IGNORECASE,
)
SKIP_PARTS = {"_deps", ".git"}


def cmake_files(root):
    for path in root.rglob("CMakeLists.txt"):
        rel = path.relative_to(root)
        if rel.parts[0].startswith("build") or SKIP_PARTS & set(rel.parts):
            continue
        yield path


def declared_dependencies(root):
    pattern = re.compile(r"(?:FetchContent_Declare|ExternalProject_Add)\s*\(\s*([A-Za-z0-9_.+-]+)")
    deps = {}
    for path in cmake_files(root):
        for name in pattern.findall(path.read_text(encoding="utf-8")):
            deps[name.lower()] = path.relative_to(root)
    return deps


def third_party_table(root):
    """Maps the CMake name (lower case) to the license cell."""
    rows = {}
    in_table = False
    for line in (root / "THIRD_PARTY.md").read_text(encoding="utf-8").splitlines():
        if line.startswith("#"):
            in_table = False
            continue
        if not line.startswith("|"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if "CMake" in cells and "Licencia" in cells:
            in_table = True
            name_col, license_col = cells.index("CMake"), cells.index("Licencia")
            continue
        if not in_table or set(line) <= set("|-: "):
            continue
        rows[cells[name_col].lower()] = cells[license_col]
    return rows


def license_ok(cell):
    ids = re.split(r"\s+(?:AND|OR)\s+", cell.strip())
    return all(i.strip("() ") in ALLOWED for i in ids)


def copyleft_hits(root):
    this = pathlib.Path(__file__).resolve()
    for d in OUR_DIRS:
        base = root / d
        if not base.is_dir():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.resolve() == this:
                continue
            try:
                text = path.read_text(encoding="utf-8")
            except (UnicodeDecodeError, OSError):
                continue  # binary test data
            if COPYLEFT.search(text):
                yield path.relative_to(root)


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".").resolve()
    errors = []
    table = third_party_table(root)
    for name, where in sorted(declared_dependencies(root).items()):
        if name not in table:
            errors.append(f"{where}: dependency '{name}' is not in THIRD_PARTY.md")
        elif not license_ok(table[name]):
            errors.append(f"THIRD_PARTY.md: '{name}' has license '{table[name]}', not in {sorted(ALLOWED)}")
    for path in copyleft_hits(root):
        errors.append(f"{path}: contains copyleft license text")
    for e in errors:
        print("error:", e)
    if errors:
        print("The engine's license must stay closable; see CLEANROOM.md.")
        return 1
    print(f"licenses ok: {len(table)} libraries in THIRD_PARTY.md, all dependencies listed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
