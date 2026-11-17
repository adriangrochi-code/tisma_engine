#!/usr/bin/env python3
"""The iPhone's release gate, the checks made from the computer (docs/spec/RELEASE_GATE.md, G2 and G10).

After ReleaseGateTests drove the app on the simulator, the G-code it left (the 20 mm cube and the 3DBenchy) must be
the part's size within 0.1 mm, counting only the part's own lines (walls, infill, bridges, gap fill, ironing; not the
skirt or brim, support, wipe tower or the start G-code's purge line), and each simulated printer must hold the cube's
G-code byte for byte. Own code; the G-code tags are those of docs/spec/GCODE_OUTPUT.md (;TYPE:, ;WIDTH:, ;Z:).

    gate_ios_check.py <app data container> <3DBenchy.stl> <port>...
"""
import glob
import hashlib
import json
import os
import re
import struct
import sys
import urllib.request

PART = ("perimeter", "wall", "infill", "bridge", "gap", "overhang", "ironing", "top", "bottom")


def stl_size(path):
    with open(path, "rb") as f:
        data = f.read()
    n = struct.unpack_from("<I", data, 80)[0]
    lo = [float("inf")] * 3
    hi = [float("-inf")] * 3
    for t in range(n):
        for k in range(3):
            v = struct.unpack_from("<3f", data, 84 + 50 * t + 12 + 12 * k)
            for a in range(3):
                lo[a] = min(lo[a], v[a])
                hi[a] = max(hi[a], v[a])
    return [hi[a] - lo[a] for a in range(3)]


def gcode_size(path):
    """X, Y extent of the part's extrusions widened by half their width, and the top layer's Z."""
    x = y = z = 0.0
    e_abs = True
    last_e = 0.0
    kind = ""
    width = 0.0
    top = 0.0
    lo = [float("inf")] * 2
    hi = [float("-inf")] * 2
    with open(path, errors="replace") as f:
        for raw in f:
            line = raw.strip()
            if line.startswith(";TYPE:"):
                kind = line[6:].strip().lower()
                continue
            if line.startswith(";WIDTH:"):
                width = float(line[7:])
                continue
            code = line.split(";", 1)[0].split()
            if not code:
                continue
            if code[0] == "M82":
                e_abs = True
            elif code[0] == "M83":
                e_abs = False
            elif code[0] == "G92":
                for w in code[1:]:
                    if w[0] == "E":
                        last_e = float(w[1:])
            elif code[0] in ("G0", "G1"):
                nx, ny, nz, e = x, y, z, None
                for w in code[1:]:
                    if w[0] == "X":
                        nx = float(w[1:])
                    elif w[0] == "Y":
                        ny = float(w[1:])
                    elif w[0] == "Z":
                        nz = float(w[1:])
                    elif w[0] == "E":
                        e = float(w[1:])
                extruding = e is not None and ((e - last_e) > 0 if e_abs else e > 0)
                if e is not None and e_abs:
                    last_e = e
                if extruding and (nx, ny) != (x, y) and any(p in kind for p in PART) and "support" not in kind:
                    h = width / 2
                    for px, py in ((x, y), (nx, ny)):
                        lo[0] = min(lo[0], px - h); hi[0] = max(hi[0], px + h)
                        lo[1] = min(lo[1], py - h); hi[1] = max(hi[1], py + h)
                    top = max(top, nz)
                x, y, z = nx, ny, nz
    return [hi[0] - lo[0], hi[1] - lo[1], top]


def main():
    container, benchy, ports = sys.argv[1], sys.argv[2], [int(p) for p in sys.argv[3:]]
    found = {os.path.basename(p): p for p in glob.glob(os.path.join(container, "tmp", "**", "*.gcode"), recursive=True)}
    failures = []
    for name, want in (("cube20.gcode", [20.0, 20.0, 20.0]), ("3DBenchy.gcode", stl_size(benchy))):
        if name not in found:
            failures.append(f"{name}: not found in the app's container ({sorted(found)})")
            continue
        got = gcode_size(found[name])
        for a in range(3):
            if abs(got[a] - want[a]) > 0.1:
                failures.append(f"{name}: {'XYZ'[a]} is {got[a]:.3f} mm, the model {want[a]:.3f} mm")
        print(f"{name}: {got[0]:.2f} x {got[1]:.2f} x {got[2]:.2f} mm (model {want[0]:.2f} x {want[1]:.2f} x {want[2]:.2f})")
    if "cube20.gcode" in found:
        with open(found["cube20.gcode"], "rb") as f:
            sha = hashlib.sha256(f.read()).hexdigest()
        for port in ports:
            with urllib.request.urlopen(f"http://127.0.0.1:{port}/_tisma/received", timeout=10) as r:
                o = json.load(r)
            if not any(sha in (v.get("sha256"), v.get("gcode_sha256")) for v in o.get("files", {}).values()):
                failures.append(f"printer on {port}: the cube's G-code did not arrive whole ({o})")
            else:
                print(f"printer on {port}: got the cube's G-code whole, {o.get('state')}")
    for f in failures:
        print("FAIL " + f)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
