#!/usr/bin/env python3
"""Merges researched catalog entries into resources/catalog/ (docs/CATALOG.md).

    tools/catalog_merge.py printers  IN.json... [--into resources/catalog/printers.json]
    tools/catalog_merge.py filaments IN.json... [--into resources/catalog/filaments.json]

Each entry must have at least one source on its maker's own domain (the table
below); sources on code hosts or slicer sites are dropped, and so is an entry
left without an official source. Keys outside the catalog's format (such as the
research's "evidence" quotes) are left out. Entries already in the catalog are
replaced by the new ones of the same vendor and name. Prints what it kept and
what it dropped, and why.
"""

import json
import pathlib
import sys
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parent.parent

# The makers' own domains: an entry needs a source on one of its vendor's.
OFFICIAL = {
    "anker": ["ankermake.com", "anker.com"],
    "ankermake": ["ankermake.com", "anker.com"],
    "anycubic": ["anycubic.com"],
    "artillery": ["artillery3d.com"],
    "bambu lab": ["bambulab.com", "bblcdn.com"],
    "biqu": ["biqu.equipment", "bigtree-tech.com"],
    "colorfabb": ["colorfabb.com"],
    "creality": ["creality.com"],
    "elegoo": ["elegoo.com"],
    "esun": ["esun3d.com", "esun3dstore.com"],
    "extrudr": ["extrudr.com"],
    "fiberlogy": ["fiberlogy.com"],
    "fillamentum": ["fillamentum.com"],
    "flashforge": ["flashforge.com"],
    "geeetech": ["geeetech.com"],
    "hatchbox": ["hatchbox3d.com"],
    "kingroon": ["kingroon.com"],
    "longer": ["longer3d.com"],
    "lulzbot": ["lulzbot.com", "fargo3dprinting.com"],
    "overture": ["overture3d.com"],
    "peopoly": ["peopoly.net"],
    "polymaker": ["polymaker.com"],
    "prusa research": ["prusa3d.com", "prusament.com"],
    "prusament": ["prusa3d.com", "prusament.com"],
    "qidi": ["qidi3d.com", "qidi.tech", "qidi3dprinter.com"],
    "qidi tech": ["qidi3d.com", "qidi.tech", "qidi3dprinter.com"],
    "raise3d": ["raise3d.com"],
    "ratrig": ["ratrig.com"],
    "rat rig": ["ratrig.com"],
    "snapmaker": ["snapmaker.com"],
    "sovol": ["sovol3d.com"],
    "sunlu": ["sunlu.com"],
    "tronxy": ["tronxy.com"],
    "two trees": ["twotrees3d.com"],
    "twotrees": ["twotrees3d.com"],
    "ultimaker": ["ultimaker.com"],
    "voron": ["vorondesign.com"],
    "voron design": ["vorondesign.com"],
}

BANNED = ["github.com", "githubusercontent.com", "gitlab.com", "bitbucket.org", "orcaslicer", "prusaslicer",
          "superslicer", "slic3r", "bambustudio", "bambu-studio", "curaengine", "/cura/", "cura.", "creality-print",
          "crealityprint", "elegooslicer", "anycubicslicer", "flashprint"]

PRINTER_KEYS = ["vendor", "model", "build_volume", "bed_shape", "nozzle_diameters", "filament_diameter", "firmware",
                "extruder", "heated_bed", "max_bed_temp", "max_nozzle_temp", "max_speed", "max_acceleration",
                "auto_leveling", "enclosed", "multi_material", "sources"]
FILAMENT_KEYS = ["vendor", "name", "material", "diameter", "density", "nozzle_temp", "nozzle_temp_recommended",
                 "bed_temp", "bed_temp_recommended", "fan", "print_speed", "enclosure", "sources"]


def host(url):
    return urllib.parse.urlparse(url).netloc.lower()


def official(vendor, url):
    domains = OFFICIAL.get(vendor.lower().strip())
    if not domains:
        return False
    h = host(url)
    return any(h == d or h.endswith("." + d) for d in domains)


def banned(url):
    u = url.lower()
    return any(b in u for b in BANNED)


def main(argv):
    if len(argv) < 3 or argv[1] not in ("printers", "filaments"):
        print(__doc__)
        return 2
    kind = argv[1]
    args = argv[2:]
    into = ROOT / "resources" / "catalog" / f"{kind}.json"
    if "--into" in args:
        i = args.index("--into")
        into = pathlib.Path(args[i + 1])
        args = args[:i] + args[i + 2:]
    keys = PRINTER_KEYS if kind == "printers" else FILAMENT_KEYS
    name_key = "model" if kind == "printers" else "name"

    existing = json.loads(into.read_text(encoding="utf-8")) if into.exists() else []
    merged = {(e["vendor"].lower(), e[name_key].lower(), e.get("diameter", 1.75)): e for e in existing}
    kept, dropped = 0, []
    for path in args:
        for entry in json.loads(pathlib.Path(path).read_text(encoding="utf-8")):
            label = f'{entry.get("vendor")} {entry.get(name_key)}'
            sources = [s for s in entry.get("sources") or [] if isinstance(s, str) and not banned(s)]
            if not any(official(entry.get("vendor", ""), s) for s in sources):
                dropped.append(f"{label}: no source on the maker's own domain ({', '.join(map(host, sources)) or 'none'})")
                continue
            # Keep only the official sources.
            entry["sources"] = [s for s in sources if official(entry["vendor"], s)]
            clean = {k: entry[k] for k in keys if k in entry and entry[k] is not None and entry[k] != ""}
            merged[(clean["vendor"].lower(), clean[name_key].lower(), clean.get("diameter", 1.75))] = clean
            kept += 1
    out = sorted(merged.values(), key=lambda e: (e["vendor"].lower(), e[name_key].lower()))
    into.write_text(json.dumps(out, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(f"{kind}: {kept} kept from the inputs, {len(out)} in {into.relative_to(ROOT) if into.is_relative_to(ROOT) else into}")
    for d in dropped:
        print(f"  dropped {d}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
