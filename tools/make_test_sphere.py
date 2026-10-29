#!/usr/bin/env python3
"""Writes tests/data/sphere.stl: a binary STL of a sphere of radius 10 mm
resting on z = 0 at x = y = 0, in 48 steps around and 24 from pole to pole
(2 208 triangles), wound outwards. A curved part for the interface's view
tests. Own design; the format is the public STL description.

usage: make_test_sphere.py [out.stl]
"""

import math
import pathlib
import struct
import sys

RADIUS, AROUND, DOWN = 10.0, 48, 24


def point(i, j):
    """The point at step i around and j from the top pole."""
    theta = math.pi * j / DOWN
    phi = 2.0 * math.pi * (i % AROUND) / AROUND
    return (RADIUS * math.sin(theta) * math.cos(phi), RADIUS * math.sin(theta) * math.sin(phi),
            RADIUS + RADIUS * math.cos(theta))


def triangles():
    for j in range(DOWN):
        for i in range(AROUND):
            a, b, c, d = point(i, j), point(i + 1, j), point(i, j + 1), point(i + 1, j + 1)
            if j > 0:
                yield (a, c, b)  # outwards: counter-clockwise seen from outside
            if j < DOWN - 1:
                yield (b, c, d)


def normal(t):
    u = [t[1][k] - t[0][k] for k in range(3)]
    v = [t[2][k] - t[0][k] for k in range(3)]
    n = (u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2], u[0] * v[1] - u[1] * v[0])
    length = math.sqrt(sum(x * x for x in n)) or 1.0
    return tuple(x / length for x in n)


def main():
    out = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else \
        pathlib.Path(__file__).resolve().parent.parent / "tests" / "data" / "sphere.stl"
    tris = list(triangles())
    data = bytearray(b"tisma test sphere".ljust(80, b" "))
    data += struct.pack("<I", len(tris))
    for t in tris:
        data += struct.pack("<3f", *normal(t))
        for p in t:
            data += struct.pack("<3f", *p)
        data += b"\0\0"
    out.write_bytes(bytes(data))
    print(f"wrote {out}: {len(tris)} triangles")


if __name__ == "__main__":
    main()
