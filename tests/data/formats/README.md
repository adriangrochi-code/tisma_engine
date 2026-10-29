# Test files of the model formats and of DEFLATE

Made by `make.py` (own script; `python3 -I make.py` in this folder), for `tests/test_formats.cpp`:

- `box.obj`: the 20 × 20 × 10 mm box of `docs/spec/features/formats.md` (FM2), with quads, normals and a face
  with negative indices.
- `box*.3mf`: the same box in 3MF packages: deflated and stored, in inches (FM3), in two parts (FM9), with a support
  object, with a slicer project's modifier volume, and with its mesh in another model file of the package, mirrored.
- `box_negative.3mf`, `box_cavity.3mf`: the box with a negative volume (`model_tools.md`, MT1 and MT3): a 10 × 10 × 20
  mm block through the middle, and a closed 10 × 10 × 4 mm hole from Z 3 to 7.
- `*.deflate`: raw DEFLATE streams (dynamic, fixed, stored, run-length, a match 23 000 bytes back, empty) of data the
  test makes again with the same generator, compressed with Python's zlib module.
