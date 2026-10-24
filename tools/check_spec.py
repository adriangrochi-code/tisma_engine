#!/usr/bin/env python3
"""Screens functional specifications before the clean team reads them.

The team that knows Tisma Slicer writes specs (what to build); the engine team
implements them from scratch. A spec must carry no code and no names taken
from PrusaSlicer, Slic3r, Cura or Tisma Slicer (see CLEANROOM.md). This script
reports only file, line and rule, never the text itself, so that running it
does not expose the clean team to what it flags.

usage: check_spec.py <file or directory>...
exit status 1 when a spec breaks a rule.
"""

import pathlib
import re
import sys

# Names of types, modules and files inside the forbidden programs. A spec names
# concepts in its own words instead. Program names themselves (PrusaSlicer,
# Cura...) may appear in prose, for example for black-box comparisons.
DENYLIST = [
    "libslic3r", "Slic3r::", "PrintObject", "PrintRegion", "LayerRegion", "PrintConfig", "DynamicPrintConfig", "FullPrintConfig",
    "PrintObjectConfig", "PrintRegionConfig", "PresetBundle", "PresetCollection", "ModelObject", "ModelVolume",
    "ModelInstance", "TriangleMesh", "TriangleMeshSlicer", "ExPolygon", "ExPolygons", "Polyline", "Polylines",
    "ExtrusionEntity", "ExtrusionPath", "ExtrusionLoop", "ExtrusionMultiPath", "ExtrusionRole", "SurfaceCollection",
    "SurfaceType", "PerimeterGenerator", "GCodeGenerator", "GCodeWriter", "GCodeProcessor", "FillBase", "FillParams",
    "SupportMaterial", "TreeSupport", "SupportSpotsGenerator", "WallToolPaths", "BeadingStrategy",
    "SkeletalTrapezoidation", "SliceDataStorage", "SliceMeshStorage", "Tisma::", "tisma_fea",
]
CODE_LINE = [
    (re.compile(r"#\s*include\s*[<\"]"), "include directive"),
    (re.compile(r"\b\w+::\w+"), "C++ scope operator"),
    (re.compile(r"->\s*\w+\s*\("), "member call through pointer"),
    (re.compile(r"\b(for|while|if|switch)\s*\(.*\)\s*\{?\s*$"), "control statement"),
    # Lists in these documents end their items with ';', so only an assignment
    # ending in ';' counts as code.
    (re.compile(r"^\s*[\w.\[\]>-]+\s*[-+*/|&]?=\s*[^=].*;\s*(//.*)?$"), "assignment statement"),
    (re.compile(r"\b(void|int|float|double|bool|auto|const|static|class|struct|template|def|return)\b.*[({=;]"),
     "declaration or return"),
    (re.compile(r"\w+\.(cpp|hpp|cc|h|py|glsl|comp)\b"), "source file name"),
]
# Data blocks: G-code samples, plain text, JSON, CSV. Their lines are still
# checked for names and code.
DATA_FENCE_LANGS = {"gcode", "text", "txt", "json", "csv"}
FENCE = re.compile(r"^\s*(```|~~~)\s*([\w+#-]*)")


def check_file(path):
    problems = []
    in_fence = False
    fence_is_code = False
    fence_lang = ""
    deny = re.compile(r"(?<![\w])(" + "|".join(re.escape(d) for d in DENYLIST) + r")(?![\w])")
    for n, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
        m = FENCE.match(line)
        if m:
            if not in_fence:
                in_fence = True
                fence_lang = m.group(2).lower()
                fence_is_code = fence_lang not in DATA_FENCE_LANGS
                if fence_is_code:
                    problems.append((n, "code block (only gcode, text, json and csv blocks are allowed)"))
            else:
                in_fence = False
            continue
        if in_fence and fence_is_code:
            continue  # already reported once for the block
        if deny.search(line):
            problems.append((n, "name from a forbidden program"))
        for pattern, rule in CODE_LINE:
            if pattern.search(line):
                problems.append((n, rule))
                break
    return problems


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 2
    files = []
    for arg in sys.argv[1:]:
        p = pathlib.Path(arg)
        files += sorted(p.rglob("*.md")) if p.is_dir() else [p]
    total = 0
    clean = 0
    for f in files:
        problems = check_file(f)
        total += len(problems)
        if not problems:
            clean += 1
        for n, rule in problems:
            print(f"{f}:{n}: {rule}")
    print(f"{clean}/{len(files)} files clean, {total} problems")
    return 1 if total else 0


if __name__ == "__main__":
    sys.exit(main())
