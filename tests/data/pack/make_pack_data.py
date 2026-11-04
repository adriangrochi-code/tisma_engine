# Makes the small invented profile pack the engine's tests use
# (profile_pack.md, «Pruebas»): run from this folder with Python 3 and the xz
# command. Every profile here is invented; nothing comes from another program.
import hashlib, io, json, os, shutil, subprocess, tarfile

def xz(data):
    return subprocess.run(["xz", "-c", "--check=crc64"], input=data, capture_output=True, check=True).stdout

def tar(entries):
    b = io.BytesIO()
    with tarfile.open(fileobj=b, mode="w", format=tarfile.PAX_FORMAT) as t:
        for name, data in entries:
            i = tarfile.TarInfo(name); i.mtime = 0
            if isinstance(data, tuple):  # ("link", target)
                i.type = tarfile.SYMTYPE; i.linkname = data[1]; t.addfile(i)
            else:
                i.size = len(data); t.addfile(i, io.BytesIO(data))
    return xz(b.getvalue())

def j(o):
    return (json.dumps(o, indent=1) + "\n").encode()

def model(name, nozzle_diameter):
    return j({"format": "tisma-pack-model", "version": 2, "technology": "fff", "vendor": "Acme",
              "model": {"id": "A1", "name": "Acme One", "family": "One"}, "default_variant": "0.4",
              "common": {"settings": {"machine": {"max_bed_temp": 110, "nozzle_diameter": 0.4,
                                                  "bed": [[0, 0], [220, 0], [220, 220], [0, 220]]},
                                      "speed": {"travel": 150}}},
              "variants": [{"id": "0.4", "nozzle": 0.4, "name": name + " 0.4", "settings": {},
                            "defaults": {"filament": ["vendors/acme/filaments/PLA.json"],
                                         "print": "vendors/acme/prints/Standard.json"}},
                           {"id": "0.6", "nozzle": 0.6, "name": name + " 0.6",
                            "settings": {"machine": {"nozzle_diameter": nozzle_diameter}}}],
              "license": "AGPL-3.0-or-later", "origin": "Invented"})

def entry(kind, name, settings, compatible, vendor="Acme", technology="fff"):
    return j({"format": "tisma-pack-entry", "version": 2, "technology": technology, "vendor": vendor,
              "profile": {"format": "tisma-profile", "version": 1, "kind": kind, "name": name, "settings": settings},
              "compatible": compatible, "generic": False, "license": "AGPL-3.0-or-later", "origin": "Invented"})

def slab(x0, y0, x1, y1, z0, z1):
    v = [(x, y, z) for z in (z0, z1) for y in (y0, y1) for x in (x0, x1)]
    faces = [(0, 2, 1), (1, 2, 3), (4, 5, 6), (5, 7, 6), (0, 1, 4), (1, 5, 4),
             (2, 6, 3), (3, 6, 7), (0, 4, 2), (2, 4, 6), (1, 3, 5), (3, 7, 5)]
    lines = ["solid bed"]
    for f in faces:
        lines += ["facet normal 0 0 0", "outer loop"] + ["vertex %g %g %g" % v[i] for i in f] + ["endloop", "endfacet"]
    return ("\n".join(lines + ["endsolid bed"]) + "\n").encode()

bed_svg = (b'<svg xmlns="http://www.w3.org/2000/svg" width="220" height="220" viewBox="0 0 220 220">'
           b'<rect x="10" y="10" width="200" height="200" fill="#2a2f3a"/>'
           b'<circle cx="110" cy="110" r="40" fill="#e05a2b"/>'
           b'<path d="M20 200 H200" stroke="#ffffff" stroke-width="4"/></svg>\n')

acme1 = tar([("vendors/acme/beds/A1.stl", slab(0, 0, 220, 220, -3, 0)), ("vendors/acme/beds/A1.svg", bed_svg),
             ("vendors/acme/models/A1.json", model("Acme One", 0.6)),
             ("vendors/acme/filaments/PLA.json", entry("filament", "Acme PLA", {"machine": {"variables": {"temperature": 210}}, "cooling": {"min_speed": 12}}, 0)),
             ("vendors/acme/prints/Standard.json", entry("print", "Acme Standard", {"layers": {"height": 0.16}, "speed": {"walls": 170, "travel": 300}}, None)),
             ("vendors/acme/prints/Fine.json", entry("print", "Acme Fine", {"layers": {"height": 0.1}, "speed": {"walls": 60, "travel": 200}}, None))])
acme2 = tar([("vendors/acme/beds/A1.stl", slab(0, 0, 220, 220, -3, 0)), ("vendors/acme/beds/A1.svg", bed_svg),
             ("vendors/acme/models/A1.json", model("Acme One Plus", 0.6)),
             ("vendors/acme/filaments/PLA.json", entry("filament", "Acme PLA", {"machine": {"variables": {"temperature": 215}}, "cooling": {"min_speed": 12}}, 0)),
             ("vendors/acme/prints/Standard.json", entry("print", "Acme Standard", {"layers": {"height": 0.16}, "speed": {"walls": 170, "travel": 300}}, None)),
             ("vendors/acme/prints/Fine.json", entry("print", "Acme Fine", {"layers": {"height": 0.1}, "speed": {"walls": 60, "travel": 200}}, None))])
bolt = tar([("vendors/bolt/models/B2.json", j({"format": "tisma-pack-model", "version": 2, "technology": "sla",
             "vendor": "Bolt", "model": {"id": "B2", "name": "Bolt Two"}, "default_variant": "default",
             "common": {"settings": {}}, "variants": [{"id": "default", "nozzle": 0, "name": "Bolt Two", "settings": {},
                           "defaults": {"resin": ["vendors/bolt/resins/Grey.json"],
                                        "resin_print": "vendors/bolt/resin_prints/Fine.json"}}]})),
            ("vendors/bolt/resins/Grey.json", entry("resin", "Bolt Grey", {"resin": {"exposure": 3.5}}, 1, "Bolt", "sla")),
            ("vendors/bolt/resin_prints/Fine.json",
             entry("resin_print", "Bolt Fine", {"resin": {"layer_height": 0.03}}, 1, "Bolt", "sla"))])
def value(key, v, unit, kind, url):
    return {"key": key, "value": v, "unit": unit, "kind": kind,
            "source": {"type": "video" if kind != "official" else "manufacturer", "url": url, "title": "Invented",
                       "author": "Nobody", "published": "2026-01-01", "at": "3:20", "accessed": "2026-11-04"}}
community = tar([("community/printers/Acme/Acme One.json", j({"vendor": "Acme", "model": "Acme One", "data": [
    value("machine.max_flow", 15, "mm³/s", "official", "https://example.org/acme-one"),
    value("measured.max_flow", 18.5, "mm³/s", "measured", "https://example.org/video"),
    value("input_shaper.type", "mzv", "", "community", "https://example.org/forum"),
    value("no.such_key", 1, "", "official", "https://example.org/x"),
    value("machine.max_bed_temp", 110, "°C", "rumour", "https://example.org/y")]})),
    ("community/filaments/Acme/Acme PLA.json", j({"vendor": "Acme", "name": "Acme PLA", "data": [
    value("extrusion.density", 1.24, "g/cm³", "official", "https://example.org/acme-pla"),
    value("filament.drying_temperature", 50, "°C", "measured", "https://example.org/dry")]})),
    ("community/resins/Bolt/Bolt Grey.json", j({"vendor": "Bolt", "name": "Bolt Grey", "data": [
    value("resin.exposure", 2.2, "s", "official", "https://example.org/bolt-grey"),
    value("resin.viscosity", 350, "mPa·s", "community", "https://example.org/forum-grey")]}))])
knowledge = tar([("knowledge/glossary.json", j({"format": "tisma-knowledge-glossary", "version": 1, "language": "es",
                                                 "sources": [], "terms": []}))])
compatible = j({"format": "tisma-pack-compatible", "version": 1, "sets": [[0, 1], [2]]})
evil_path = tar([("vendors/acme/models/A1.json", model("Acme One", 0.6)), ("vendors/acme/../../x.json", b"{}\n")])
evil_link = tar([("vendors/bolt/models/B2.json", b"{}\n"), ("vendors/bolt/link", ("link", "/etc/passwd"))])

def file(name, data):
    return {"name": name, "size": len(data), "sha256": hashlib.sha256(data).hexdigest()}

def index(version, acme, bolt_data):
    return j({"format": "tisma-profile-pack", "version": 2, "pack_version": version,
              "generated": "2026-11-04T00:00:00Z", "license": "AGPL-3.0-or-later",
              "printers": ["vendors/acme/models/A1.json#0.4", "vendors/acme/models/A1.json#0.6",
                           "vendors/bolt/models/B2.json#default"],
              "compatible": file("compatible.json", compatible), "community": file("community.tar.xz", community),
              "vendors": [
                  {"id": "acme", "name": "Acme", "license": "AGPL-3.0-or-later", "origin": "Invented",
                   "printers": ["vendors/acme/models/A1.json#0.4", "vendors/acme/models/A1.json#0.6"],
                   "filaments": ["vendors/acme/filaments/PLA.json"], "prints": ["vendors/acme/prints/Standard.json", "vendors/acme/prints/Fine.json"],
                   "archive": file("acme.tar.xz", acme),
                   "models": [{"id": "A1", "name": "Acme One", "technology": "fff", "family": "One",
                               "assets": {"bed_model": "vendors/acme/beds/A1.stl", "bed_texture": "vendors/acme/beds/A1.svg"},
                               "file": "vendors/acme/models/A1.json", "default_variant": "0.4",
                               "variants": [{"id": "0.4", "nozzle": 0.4}, {"id": "0.6", "nozzle": 0.6}]}]},
                  {"id": "bolt", "name": "Bolt", "license": "AGPL-3.0-or-later", "origin": "Invented",
                   "printers": ["vendors/bolt/models/B2.json#default"], "archive": file("bolt.tar.xz", bolt_data),
                   "resins": ["vendors/bolt/resins/Grey.json"], "resin_prints": ["vendors/bolt/resin_prints/Fine.json"],
                   "models": [{"id": "B2", "name": "Bolt Two", "technology": "sla", "file": "vendors/bolt/models/B2.json",
                               "default_variant": "default", "variants": [{"id": "default", "nozzle": 0}]}]}]})

releases = {
    "release1": {"index.json": index("2026.11.01-aaaaaaa", acme1, bolt), "compatible.json": compatible,
                 "acme.tar.xz": acme1, "bolt.tar.xz": bolt, "community.tar.xz": community,
                 "knowledge.tar.xz": knowledge},
    "release2": {"index.json": index("2026.11.04-bbbbbbb", acme2, bolt), "acme.tar.xz": acme2},
    "evil": {"index.json": index("2026.11.04-ccccccc", evil_path, evil_link), "acme.tar.xz": evil_path,
             "bolt.tar.xz": evil_link},
}
for folder, files in releases.items():
    shutil.rmtree(folder, ignore_errors=True)
    os.makedirs(folder)
    for name, data in files.items():
        open(os.path.join(folder, name), "wb").write(data)
