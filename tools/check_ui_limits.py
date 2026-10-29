#!/usr/bin/env python3
"""Checks that the settings panel lets every number reach its whole range.

Each number row of the panel (`mm(T::..., "group.key", value, MIN, MAX, ...)`
and `whole(...)` in apps/tisma/app.cpp) is compared with the range column of
its row in docs/CONFIG.md («0 a 100», «0,05 a 40», «más de 0 a 600»). The
panel's limits must cover the documented range: a field that stops below the maximum, or above the
minimum, cannot be set to a value the engine reads (the spec team's UI16 and
UI20). A panel range wider than the documented one is allowed (the reader
clamps it).

Rows whose panel unit differs from the file's (shown in % of a fraction, or in
mm/min) are listed in SCALED with the factor from the file to the panel.

usage: check_ui_limits.py [repo_root]
"""

import pathlib
import re
import sys

# The panel shows these in another unit than the file: file value x factor.
SCALED = {}

NUMBER = r"-?\d+(?:[.,]\d+)?"


def number(text):
    return float(text.replace(",", "."))


def config_ranges(text):
    """{"group.key": (min, max)} from the tables under each «### `group`»."""
    out = {}
    group = None
    for line in text.splitlines():
        heading = re.match(r"^###\s+`([a-z_]+)`", line)
        if heading:
            group = heading.group(1)
            continue
        if not group or not line.startswith("| `"):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        if len(cells) < 4:
            continue
        keys = re.findall(r"`([a-z_0-9]+)`", cells[0])
        # One range for all the keys, or one per key separated by «;».
        parts = [p.strip() for p in cells[3].split(";")]
        if len(parts) != len(keys):
            parts = [cells[3]] * len(keys)
        for key, part in zip(keys, parts):
            # The first «A a B»; «0, o 0,05 a 40» keeps 0 as its minimum, and
            # «más de 0 a 100» leaves the minimum out (above 0, any).
            span = re.search(rf"({NUMBER})\s+a\s+({NUMBER})", part)
            if not span:
                continue
            lo, hi = number(span.group(1)), number(span.group(2))
            if re.match(r"^0\s*,\s*o\s", part):
                lo = 0.0
            if re.search(rf"más de\s+{re.escape(span.group(1))}\s+a", part):
                lo = None
            out.setdefault(f"{group}.{key}", (lo, hi))
    return out


def panel_ranges(text):
    """[(key, min, max, line)] from the panel's number rows."""
    out = []
    pattern = re.compile(
        rf'\b(?:mm|whole)\(T::\w+,\s*"([a-z_]+\.[a-z_0-9]+)",\s*[^,]+,\s*({NUMBER}),\s*({NUMBER})\s*[,)]')
    for n, line in enumerate(text.splitlines(), 1):
        for m in pattern.finditer(line):
            out.append((m.group(1), number(m.group(2)), number(m.group(3)), n))
    return out


def main():
    root = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else ".")
    documented = config_ranges((root / "docs" / "CONFIG.md").read_text(encoding="utf-8"))
    rows = panel_ranges((root / "apps" / "tisma" / "app.cpp").read_text(encoding="utf-8"))
    problems = []
    checked = 0
    for key, lo, hi, line in rows:
        if key not in documented:
            continue
        checked += 1
        want_lo, want_hi = documented[key]
        factor = SCALED.get(key, 1.0)
        want_hi = want_hi * factor
        if want_lo is None:  # above it, exclusive: the panel's minimum only has to be above it too
            low_ok = True
        else:
            want_lo = want_lo * factor
            low_ok = lo <= want_lo + 1e-9
        if hi < want_hi - 1e-9 or not low_ok:
            documented_lo = "more than 0" if want_lo is None else f"{want_lo:g}"
            problems.append(f"apps/tisma/app.cpp:{line}: {key} goes from {lo:g} to {hi:g} in the panel, "
                            f"{documented_lo} to {want_hi:g} in docs/CONFIG.md")
    for p in problems:
        print(p)
    print(f"{checked} panel ranges checked against docs/CONFIG.md, {len(problems)} too narrow")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
