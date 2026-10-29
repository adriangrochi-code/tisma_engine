#!/usr/bin/env python3
"""Writes a closed UV sphere as STL, to measure the reader until the real
benchmark models are in place (see bench/README.md).

usage: gen_sphere.py <out.stl> --segments N [--ascii]
The sphere has 2*N*(N-1) triangles (N rings, N segments, radius 50 mm).
"""

import argparse
import math
import struct


def sphere(n, radius=50.0):
    rings = n

    def point(i, j):
        # Exact poles and seam, so that shared vertices are bitwise equal and
        # the mesh is closed.
        if i == 0:
            return (0.0, 0.0, radius)
        if i == rings:
            return (0.0, 0.0, -radius)
        j %= n
        theta = math.pi * i / rings
        phi = 2.0 * math.pi * j / n
        return (radius * math.sin(theta) * math.cos(phi),
                radius * math.sin(theta) * math.sin(phi),
                radius * math.cos(theta))

    for i in range(rings):
        for j in range(n):
            a, b = point(i, j), point(i + 1, j)
            c, d = point(i + 1, j + 1), point(i, j + 1)
            if i != 0:
                yield (a, b, d)
            if i != rings - 1:
                yield (b, c, d)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--segments", type=int, default=700)
    ap.add_argument("--ascii", action="store_true")
    args = ap.parse_args()
    tris = list(sphere(args.segments))
    if args.ascii:
        with open(args.out, "w") as f:
            f.write("solid sphere\n")
            for t in tris:
                f.write("  facet normal 0 0 0\n    outer loop\n")
                for p in t:
                    f.write("      vertex %.6e %.6e %.6e\n" % p)
                f.write("    endloop\n  endfacet\n")
            f.write("endsolid sphere\n")
    else:
        with open(args.out, "wb") as f:
            f.write(b"tisma sphere".ljust(80, b"\0"))
            f.write(struct.pack("<I", len(tris)))
            for t in tris:
                f.write(struct.pack("<12fH", 0, 0, 0, *t[0], *t[1], *t[2], 0))
    print(len(tris), "triangles")


if __name__ == "__main__":
    main()
