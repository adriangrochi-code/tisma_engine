#!/usr/bin/env python3
"""Merges researched catalog entries into resources/catalog/ (docs/CATALOG.md).

    tools/catalog_merge.py printers  IN.json... [--update] [--into resources/catalog/printers.json]
    tools/catalog_merge.py filaments IN.json... [--update] [--into resources/catalog/filaments.json]

Each entry must have at least one source on its maker's own domain (the table
below); sources on code hosts or slicer sites are dropped, and so is an entry
left without an official source. Keys outside the catalog's format (such as the
research's "evidence" quotes) are left out, and so are values outside the
catalog's ranges or lists. Entries already in the catalog are replaced by the
new ones of the same vendor and name; with --update they keep what they have
and only gain the keys they did not have, and the new official sources (a
different value for a key they have is reported, not taken). Prints what it
kept and what it dropped, and why.
"""

import json
import pathlib
import sys
import urllib.parse

ROOT = pathlib.Path(__file__).resolve().parent.parent

# The makers' own domains: an entry needs a source on one of its vendor's.
OFFICIAL = {
    "anker": ["ankermake.com", "anker.com", "eufymake.com"],
    "ankermake": ["ankermake.com", "anker.com", "eufymake.com"],
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
    "longer": ["longer3d.com", "longer.net"],
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

# How each maker writes its own name.
CANONICAL = {
    "anycubic": "Anycubic", "bambu lab": "Bambu Lab", "colorfabb": "colorFabb", "creality": "Creality",
    "elegoo": "Elegoo", "esun": "eSUN", "extrudr": "Extrudr", "fiberlogy": "Fiberlogy", "fillamentum": "Fillamentum",
    "hatchbox": "Hatchbox", "overture": "Overture", "polymaker": "Polymaker", "prusament": "Prusament",
    "prusa research": "Prusa Research", "sunlu": "SUNLU", "qidi": "QIDI", "qidi tech": "QIDI", "flashforge": "Flashforge",
    "ultimaker": "UltiMaker", "anker": "AnkerMake", "ankermake": "AnkerMake", "ratrig": "RatRig", "rat rig": "RatRig",
    "voron design": "Voron", "voron": "Voron", "two trees": "Two Trees", "twotrees": "Two Trees", "biqu": "BIQU",
    "lulzbot": "LulzBot", "raise3d": "Raise3D", "artillery": "Artillery", "sovol": "Sovol", "snapmaker": "Snapmaker",
    "kingroon": "Kingroon", "peopoly": "Peopoly", "tronxy": "Tronxy", "geeetech": "Geeetech", "longer": "Longer",
}

BANNED = ["github.com", "githubusercontent.com", "gitlab.com", "bitbucket.org", "orcaslicer", "prusaslicer",
          "superslicer", "slic3r", "bambustudio", "bambu-studio", "curaengine", "/cura/", "cura.", "creality-print",
          "crealityprint", "elegooslicer", "anycubicslicer", "flashprint"]

PRINTER_KEYS = ["vendor", "model", "build_volume", "bed_shape", "nozzle_diameters", "filament_diameter", "firmware",
                "extruder", "heated_bed", "max_bed_temp", "max_nozzle_temp", "max_speed", "max_acceleration",
                "auto_leveling", "enclosed", "multi_material", "kinematics", "max_flow", "chamber_heating",
                "hardened_nozzle", "hotend", "probe", "bed_surfaces", "connectivity", "features", "material_slots",
                "tools", "sources"]
FILAMENT_KEYS = ["vendor", "name", "material", "diameter", "density", "nozzle_temp", "nozzle_temp_recommended",
                 "bed_temp", "bed_temp_recommended", "fan", "print_speed", "enclosure", "max_flow", "chamber_temp",
                 "drying_temp", "drying_hours", "abrasive", "soluble", "hardness", "glass_transition", "heat_deflection",
                 "shrinkage", "bed_surfaces", "sources"]

# The ranges parse_catalog accepts (src/catalog/catalog.cpp): a value outside is left out.
NUMBERS = {
    "printers": {"filament_diameter": (1.0, 3.5), "max_bed_temp": (0, 200), "max_nozzle_temp": (150, 600),
                 "max_speed": (10, 2000), "max_acceleration": (100, 100000), "max_flow": (1, 1000),
                 "chamber_heating": (20, 200), "material_slots": (1, 64), "tools": (1, 16)},
    "filaments": {"diameter": (1.0, 3.5), "density": (0.5, 3.0), "nozzle_temp_recommended": (150, 500),
                  "bed_temp_recommended": (0, 200), "max_flow": (1, 1000), "drying_temp": (20, 200),
                  "drying_hours": (0.5, 96), "glass_transition": (-100, 400), "heat_deflection": (0, 400),
                  "shrinkage": (0, 10)},
}
WHOLE = {"material_slots", "tools"}
FLAGS = {"heated_bed", "auto_leveling", "enclosed", "hardened_nozzle", "enclosure", "abrasive", "soluble"}
TEXT_LISTS = {"bed_surfaces", "connectivity", "features"}
CHOICES = {
    "firmware": {"marlin", "klipper", "reprap", "bambu", "other"},
    "extruder": {"direct", "bowden"},
    "bed_shape": {"rectangular", "circular"},
    "kinematics": {"cartesian", "corexy", "corexz", "delta", "idex", "belt", "toolchanger"},
}
CONNECTIVITY = {"wifi", "ethernet", "usb", "sd", "usb drive", "cloud", "app", "lan"}


def bad_value(kind, key, value):
    """Why a value does not fit the catalog's format, or None."""
    if key in FLAGS:
        return None if isinstance(value, bool) else "not true or false"
    if key in NUMBERS[kind]:
        lo, hi = NUMBERS[kind][key]
        if isinstance(value, bool) or not isinstance(value, (int, float)) or not lo <= value <= hi:
            return f"not a number from {lo} to {hi}"
        if key in WHOLE and value != int(value):
            return "not a whole number"
        return None
    if key in TEXT_LISTS:
        if not isinstance(value, list) or not all(isinstance(v, str) and v.strip() for v in value):
            return "not a list of texts"
        if key == "connectivity" and not set(value) <= CONNECTIVITY:
            return f"not from {sorted(CONNECTIVITY)}"
        return None
    if key in CHOICES:
        return None if value in CHOICES[key] else f"not one of {sorted(CHOICES[key])}"
    if key in ("hotend", "probe", "multi_material", "hardness"):
        return None if isinstance(value, str) else "not a text"
    return None


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
    update = "--update" in args
    args = [a for a in args if a != "--update"]
    into = ROOT / "resources" / "catalog" / f"{kind}.json"
    if "--into" in args:
        i = args.index("--into")
        into = pathlib.Path(args[i + 1])
        args = args[:i] + args[i + 2:]
    keys = PRINTER_KEYS if kind == "printers" else FILAMENT_KEYS
    name_key = "model" if kind == "printers" else "name"

    existing = json.loads(into.read_text(encoding="utf-8")) if into.exists() else []
    for e in existing:
        e["vendor"] = CANONICAL.get(e["vendor"].lower().strip(), e["vendor"])
    merged = {(e["vendor"].lower(), e[name_key].lower(), e.get("diameter", 1.75)): e for e in existing}
    kept, dropped, notes = 0, [], []
    for path in args:
        for entry in json.loads(pathlib.Path(path).read_text(encoding="utf-8")):
            entry["vendor"] = CANONICAL.get(str(entry.get("vendor", "")).lower().strip(), entry.get("vendor"))
            label = f'{entry.get("vendor")} {entry.get(name_key)}'
            sources = [s for s in entry.get("sources") or [] if isinstance(s, str) and not banned(s)]
            if not any(official(entry.get("vendor", ""), s) for s in sources):
                dropped.append(f"{label}: no source on the maker's own domain ({', '.join(map(host, sources)) or 'none'})")
                continue
            # Keep only the official sources.
            entry["sources"] = [s for s in sources if official(entry["vendor"], s)]
            clean = {k: entry[k] for k in keys if k in entry and entry[k] is not None and entry[k] != "" and entry[k] != []}
            for k in list(clean):
                why = bad_value(kind, k, clean[k])
                if why:
                    notes.append(f"{label}: {k} = {clean[k]!r} left out, {why}")
                    del clean[k]
            # Ranges need both ends ("up to X" has none below): otherwise left out.
            for k in ("nozzle_temp", "bed_temp", "fan", "print_speed", "chamber_temp"):
                r = clean.get(k)
                if r is not None and not (isinstance(r, list) and len(r) == 2 and all(isinstance(v, (int, float)) for v in r)
                                          and r[0] <= r[1]):
                    del clean[k]
            key = (clean["vendor"].lower(), clean[name_key].lower(), clean.get("diameter", 1.75))
            if update and key in merged:
                old_entry = merged[key]
                added = []
                for k, v in clean.items():
                    if k == "sources":
                        old_entry["sources"] += [s for s in v if s not in old_entry["sources"]]
                    elif k not in old_entry:
                        old_entry[k] = v
                        added.append(k)
                    elif old_entry[k] != v and k not in ("vendor", name_key):
                        notes.append(f"{label}: {k} stays {old_entry[k]!r} (the research says {v!r})")
                # Back in the catalog's key order.
                merged[key] = {k: old_entry[k] for k in keys if k in old_entry}
                if added:
                    kept += 1
                continue
            if kind == "filaments" and "nozzle_temp" not in clean and "nozzle_temp_recommended" not in clean:
                dropped.append(f"{label}: no nozzle temperature")
                continue
            if kind == "printers" and not (isinstance(clean.get("build_volume"), list) and len(clean["build_volume"]) == 3):
                dropped.append(f"{label}: no build volume" + (" (no catalog entry of that name to update)" if update else ""))
                continue
            if kind == "filaments" and "material" not in clean:
                dropped.append(f"{label}: no material" + (" (no catalog entry of that name to update)" if update else ""))
                continue
            merged[key] = clean
            kept += 1
    out = sorted(merged.values(), key=lambda e: (e["vendor"].lower(), e[name_key].lower()))
    into.write_text(json.dumps(out, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")
    print(f"{kind}: {kept} kept from the inputs, {len(out)} in {into.relative_to(ROOT) if into.is_relative_to(ROOT) else into}")
    for d in dropped:
        print(f"  dropped {d}")
    for n in notes:
        print(f"  note: {n}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
