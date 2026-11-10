#!/usr/bin/env python3
"""Writes src/gcode/macro_variables.inc from the variable list of
docs/spec/features/gcode_variables.md (gcode_macros.md, rule 9).

Usage: tools/gcode_variables_table.py <gcode_variables.md> <out.inc>

One row per variable: its name, the variable whose value it takes (its
equivalent, empty when none) and a type letter: n number, i integer, b yes/no,
t text, N I B T their lists, ? not given, v a [word] in a comment that is not
a variable (a capitalised word, or one of COMMENT_WORDS). Variables the macro sets itself are left out. A name in several
dialects is written once.
"""
import re
import sys

TYPES = {
    "número": "n", "entero": "i", "sí/no": "b", "texto": "t", "opción": "t",
    "número o porcentaje": "?", "lista de números": "N", "lista de enteros": "I",
    "lista de sí/no": "B", "lista de textos": "T", "lista de opciones": "T", "": "?",
}
ROW = re.compile(r"\| `([^`]+)` \| ([^|]*)\| ([^|]*)\| ([^|]*)\|")
# Words the list has because a comment wrote them between brackets.
COMMENT_WORDS = {"then", "move", "directories", "filament_check", "nozzle_check", "load_heightmap",
                 "first_layer_temperature_"}
IDENT = re.compile(r"[A-Za-z_][A-Za-z0-9_]*$")


def main() -> int:
    source, target = sys.argv[1], sys.argv[2]
    seen = {}
    dialect = None
    for line in open(source, encoding="utf-8"):
        if line.startswith("## "):
            dialect = line[3:].strip().strip("`")
            continue
        m = ROW.match(line)
        if not m or not dialect:
            continue
        name, kind, equivalent = m.group(1), m.group(2).strip(), m.group(4).strip().strip("`")
        if kind == "propia de la macro" or not IDENT.match(name):
            continue
        if not IDENT.match(equivalent):
            equivalent = ""
        letter = TYPES[kind]
        if not equivalent and (name[0].isupper() or name in COMMENT_WORDS):
            letter = "v"
        if name not in seen:
            seen[name] = (equivalent, letter)
    with open(target, "w", encoding="utf-8", newline="\n") as out:
        out.write("// Written by tools/gcode_variables_table.py from docs/spec/features/gcode_variables.md.\n")
        for name, (equivalent, letter) in sorted(seen.items()):
            out.write(f'{{"{name}", "{equivalent}", \'{letter}\'}},\n')
    print(f"{len(seen)} variables")
    return 0


if __name__ == "__main__":
    sys.exit(main())
