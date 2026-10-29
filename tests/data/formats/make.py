# Makes the test files of tests/test_formats.cpp: a 20 x 20 x 10 mm box as
# OBJ and as 3MF packages, and raw DEFLATE streams of data the test makes
# again with the same generator. Own script; run with python3 -I make.py.
import zipfile
import zlib

V = [(0, 0, 0), (20, 0, 0), (20, 20, 0), (0, 20, 0), (0, 0, 10), (20, 0, 10), (20, 20, 10), (0, 20, 10)]
F = [(0, 2, 1), (0, 3, 2), (4, 5, 6), (4, 6, 7), (0, 1, 5), (0, 5, 4),
     (1, 2, 6), (1, 6, 5), (2, 3, 7), (2, 7, 6), (3, 0, 4), (3, 4, 7)]

with open('box.obj', 'w', newline='\n') as f:
    f.write('# a 20 x 20 x 10 mm box\no box\n')
    f.writelines('v %g %g %g\n' % v for v in V)
    f.write('vn 0 0 1\n')
    # Quads for the sides, with the vertex/normal form and negative indices on one.
    f.write('f 1 3 2\nf 1 4 3\nf 5//1 6//1 7//1 8//1\nf 1 2 6 5\nf 2 3 7 6\nf -6 -5 -1 -2\nf 4 1 5 8\n')


def model(unit='millimeter', parts=1, extra=''):
    s = '<?xml version="1.0" encoding="UTF-8"?>\n'
    s += '<model unit="%s" xml:lang="en-US" xmlns="http://schemas.microsoft.com/3dmanufacturing/core/2015/02">\n' % unit
    s += ' <metadata name="Title">Box &amp; parts</metadata>\n <resources>\n'
    for p in range(parts):
        s += '  <object id="%d" type="model">\n   <mesh>\n    <vertices>\n' % (p + 1)
        s += ''.join('     <vertex x="%g" y="%g" z="%g" />\n' % (x / parts + p * 20 / parts, y, z) for x, y, z in V)
        s += '    </vertices>\n    <triangles>\n'
        s += ''.join('     <triangle v1="%d" v2="%d" v3="%d" />\n' % t for t in F)
        s += '    </triangles>\n   </mesh>\n  </object>\n'
    if parts > 1:
        s += '  <object id="10" type="model"><components>'
        s += ''.join('<component objectid="%d"/>' % (p + 1) for p in range(parts)) + '</components></object>\n'
    s += extra
    s += ' </resources>\n <build>\n  <item objectid="%d" transform="1 0 0 0 1 0 0 0 1 90 90 0" />\n </build>\n</model>\n' % (
        10 if parts > 1 else 1)
    return s


RELS = ('<?xml version="1.0" encoding="UTF-8"?>\n<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/'
        'relationships"><Relationship Target="/3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/'
        '3dmanufacturing/2013/01/3dmodel"/></Relationships>\n')
TYPES = ('<?xml version="1.0" encoding="UTF-8"?>\n<Types xmlns="http://schemas.openxmlformats.org/package/2006/'
         'content-types"><Default Extension="rels" ContentType="application/vnd.openxmlformats-package.'
         'relationships+xml"/><Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-'
         '3dmodel+xml"/></Types>\n')


def package(name, text, method=zipfile.ZIP_DEFLATED, more=()):
    with zipfile.ZipFile(name, 'w', method) as z:
        for n, d in (('[Content_Types].xml', TYPES), ('_rels/.rels', RELS), ('3D/3dmodel.model', text)) + tuple(more):
            info = zipfile.ZipInfo(n, date_time=(2026, 10, 26, 0, 0, 0))
            info.compress_type = method
            z.writestr(info, d)


package('box.3mf', model())
package('box_stored.3mf', model(), zipfile.ZIP_STORED)
package('box_inch.3mf', model('inch'))
package('box_parts.3mf', model(parts=2))
# A support object and an item marked not printable are left out.
package('box_support.3mf', model(extra='  <object id="20" type="support"><mesh><vertices><vertex x="0" y="0" z="0"/>'
                                     '<vertex x="1" y="0" z="0"/><vertex x="0" y="1" z="0"/></vertices><triangles>'
                                     '<triangle v1="0" v2="1" v3="2"/></triangles></mesh></object>\n'))
# A slicer project: the second half of the triangles is a modifier volume.
two = model(parts=1).replace('<triangles>\n', '<triangles>\n' + ''.join(
    '     <triangle v1="%d" v2="%d" v3="%d" />\n' % t for t in F), 1)
config = ('<?xml version="1.0" encoding="UTF-8"?>\n<config>\n <object id="1" instances_count="1">\n'
          '  <volume firstid="0" lastid="11">\n   <metadata type="volume" key="volume_type" value="ModelPart"/>\n'
          '  </volume>\n  <volume firstid="12" lastid="23">\n'
          '   <metadata type="volume" key="volume_type" value="ParameterModifier"/>\n  </volume>\n </object>\n</config>\n')
package('box_modifier.3mf', two, more=(('Metadata/Slic3r_PE_model.config', config),))
# Slicer projects with a negative volume (model_tools.md, MT1 and MT3): the box and, after its 12 triangles, a
# block the project lists as a negative volume. MT1: 10 x 10 x 20 mm through the middle, out at the top and the
# bottom. MT3: a closed 10 x 10 x 4 mm hole from Z 3 to 7.
def with_negative(lo, hi):
    vertices = ''.join('     <vertex x="%g" y="%g" z="%g" />\n' % (lo[0] + (hi[0] - lo[0]) * (x / 20),
                                                                   lo[1] + (hi[1] - lo[1]) * (y / 20),
                                                                   lo[2] + (hi[2] - lo[2]) * (z / 10)) for x, y, z in V)
    text = model().replace('    </vertices>\n', vertices + '    </vertices>\n', 1)
    return text.replace('    </triangles>\n', ''.join('     <triangle v1="%d" v2="%d" v3="%d" />\n' %
                                                     (a + 8, b + 8, c + 8) for a, b, c in F) + '    </triangles>\n', 1)


negative = ('<?xml version="1.0" encoding="UTF-8"?>\n<config>\n <object id="1" instances_count="1">\n'
            '  <volume firstid="0" lastid="11">\n   <metadata type="volume" key="volume_type" value="ModelPart"/>\n'
            '  </volume>\n  <volume firstid="12" lastid="23">\n'
            '   <metadata type="volume" key="volume_type" value="NegativeVolume"/>\n  </volume>\n </object>\n</config>\n')
package('box_negative.3mf', with_negative((5, 5, -5), (15, 15, 15)),
        more=(('Metadata/Slic3r_PE_model.config', negative),))
package('box_cavity.3mf', with_negative((5, 5, 3), (15, 15, 7)), more=(('Metadata/Slic3r_PE_model.config', negative),))
# The mesh in another model file of the package, mirrored by its component (Production Extension).
inner = model().replace('<build>\n  <item objectid="1" transform="1 0 0 0 1 0 0 0 1 90 90 0" />\n </build>\n', '<build/>\n')
outer = ('<?xml version="1.0" encoding="UTF-8"?>\n<model unit="millimeter" xmlns="http://schemas.microsoft.com/'
         '3dmanufacturing/core/2015/02" xmlns:p="http://schemas.microsoft.com/3dmanufacturing/production/2015/06">\n'
         ' <resources>\n  <object id="5" type="model"><components><component p:path="/3D/Objects/box.model" '
         'objectid="1" transform="-1 0 0 0 1 0 0 0 1 20 0 0"/></components></object>\n </resources>\n'
         ' <build><item objectid="5" transform="1 0 0 0 1 0 0 0 1 90 90 0"/></build>\n</model>\n')
package('box_production.3mf', outer, more=(('3D/Objects/box.model', inner),))


def lcg(seed):
    x = seed
    while True:
        x = (x * 1103515245 + 12345) % 2147483648
        yield x >> 16


WORDS = ['tisma ', 'layer ', 'nozzle ', 'bed ', 'wall ', 'infill ', 'support ', 'seam ',
         'bridge ', 'skirt ', 'brim ', 'speed ', 'flow ', 'fan ', 'tree ', 'branch ']


def words(n):
    g, out = lcg(1), bytearray()
    while len(out) < n:
        out += WORDS[next(g) % 16].encode()
    return bytes(out[:n])


def noise(n, seed):
    g = lcg(seed)
    return bytes(next(g) & 0xFF for _ in range(n))


def far():
    a = noise(20000, 7)
    return a + words(3000) + a[:12000]  # repeats 23000 bytes back


def raw(data, level=9, strategy=zlib.Z_DEFAULT_STRATEGY):
    c = zlib.compressobj(level, zlib.DEFLATED, -15, 9, strategy)
    return c.compress(data) + c.flush()


for name, data in (('words', raw(words(30000))), ('fixed', raw(words(5000), strategy=zlib.Z_FIXED)),
                   ('stored', raw(noise(3000, 3), level=0)), ('rle', raw(words(8000), strategy=zlib.Z_RLE)),
                   ('far', raw(far())), ('empty', raw(b''))):
    with open(name + '.deflate', 'wb') as f:
        f.write(data)
