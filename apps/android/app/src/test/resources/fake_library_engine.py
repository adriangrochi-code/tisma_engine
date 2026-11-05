#!/usr/bin/env python3
# A stand-in for the engine in the app's tests: `slice` writes a small G-code and its summary, and `library`
# answers as the contract agreed with the engine session (local_library.md). Not the engine's format code:
# only enough for the app's screens and calls.
import hashlib
import json
import os
import shutil
import sys
import zipfile


def opt(args, name, default=None):
    return args[args.index(name) + 1] if name in args else default


def out(o, code=0):
    sys.stdout.write(json.dumps(o))
    sys.exit(code)


def sha(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


ENGINE = os.environ.get("FAKE_ENGINE_VERSION", "0.9.0")


def entry_path(lib, eid):
    return os.path.join(lib, eid, "entry.json")


def read(lib, eid):
    with open(entry_path(lib, eid)) as f:
        return json.load(f)


def write(lib, e):
    with open(entry_path(lib, e["id"]), "w") as f:
        json.dump(e, f)


def size(d):
    return sum(os.path.getsize(os.path.join(r, n)) for r, _, ns in os.walk(d) for n in ns)


def library(args):
    cmd = args[0]
    lib = opt(args, "--dir")
    if lib:
        os.makedirs(lib, exist_ok=True)
    if cmd == "list":
        entries = []
        for eid in sorted(os.listdir(lib)):
            if not os.path.isfile(entry_path(lib, eid)):
                continue
            e = read(lib, eid)
            entries.append({"id": eid, "name": e["name"], "tags": e.get("tags", []),
                            "printers": sorted({r["recipe"].get("label", "") for r in e["recipes"]} - {""}),
                            "materials": [], "recipes": len(e["recipes"]), "analyses": len(e["analyses"]),
                            "bytes": size(os.path.join(lib, eid)), "modified": e.get("modified", ""), "thumb": None})
        out({"entries": entries})
    if cmd == "show":
        out(read(lib, opt(args, "--entry")))
    if cmd == "save":
        if "--new" in args:
            n = 1
            while os.path.exists(os.path.join(lib, "e%d" % n)):
                n += 1
            eid = "e%d" % n
            d = os.path.join(lib, eid)
            os.makedirs(d)
            e = {"format": "tisma-library-entry", "version": 1, "id": eid, "name": opt(args, "--name", eid), "tags": [],
                 "notes": "", "recipes": [], "analyses": [], "printed": []}
        else:
            eid = opt(args, "--entry")
            d = os.path.join(lib, eid)
            e = read(lib, eid)
        shutil.copyfile(opt(args, "--model"), os.path.join(d, "project.3mf"))
        with open(opt(args, "--recipe")) as f:
            recipe = json.load(f)
        key = lambda r: tuple(r.get(k) for k in ("printer", "nozzle", "filament", "print"))
        same = [r for r in e["recipes"] if key(r["recipe"]) == key(recipe)]
        rid = same[0]["id"] if same else str(len(e["recipes"]) + 1)
        e["recipes"] = [r for r in e["recipes"] if r["id"] != rid]
        rd = os.path.join(d, "recipes", rid)
        os.makedirs(rd, exist_ok=True)
        shutil.copyfile(opt(args, "--config"), os.path.join(rd, "settings.json"))
        summary = None
        if opt(args, "--summary"):
            with open(opt(args, "--summary")) as f:
                s = json.load(f)
            summary = {"time_s": s.get("print_time_s"), "filament_m": (s.get("filament_mm") or 0) / 1000,
                       "filament_g": s.get("filament_g"), "layers": s.get("layers")}
        e["recipes"].append({"id": rid, "recipe": recipe, "engine": ENGINE, "settings_sha256": sha(opt(args, "--config")),
                             "modified": "2026-11-05T00:00:%02dZ" % len(e["recipes"]), "summary": summary})
        if opt(args, "--analysis"):
            aid = str(len(e["analyses"]) + 1)
            os.makedirs(os.path.join(d, "analyses"), exist_ok=True)
            shutil.copyfile(opt(args, "--analysis"), os.path.join(d, "analyses", aid + ".tea"))
            s = {}
            if opt(args, "--analysis-summary"):
                with open(opt(args, "--analysis-summary")) as f:
                    s = json.load(f)
            e["analyses"].append({"id": aid, "kind": "fea", "file": "analyses/%s.tea" % aid, "summary": s,
                                  "settings_sha256": sha(opt(args, "--config"))})
        write(lib, e)
        out(e)
    if cmd == "check":
        e = read(lib, opt(args, "--entry"))
        r = [r for r in e["recipes"] if r["id"] == opt(args, "--recipe")][0]
        with open(opt(args, "--config")) as f:
            now = json.load(f)
        with open(os.path.join(lib, e["id"], "recipes", r["id"], "settings.json")) as f:
            then = json.load(f)
        # Only the infill changes the strength in this stand-in.
        changed = ["infill.density"] if now.get("infill") != then.get("infill") else []
        out({"same_settings": now == then, "older_engine": r["engine"] != ENGINE, "missing_keys": [],
             "analyses": [{"id": a["id"], "valid": not changed, "changed": changed} for a in e["analyses"]]})
    if cmd == "export":
        d = os.path.join(lib, opt(args, "--entry"))
        with zipfile.ZipFile(opt(args, "-o"), "w") as z:
            for root, _, names in os.walk(d):
                for n in names:
                    p = os.path.join(root, n)
                    z.write(p, os.path.join(os.path.basename(d), os.path.relpath(p, d)))
        out({})
    if cmd == "import":
        with zipfile.ZipFile(args[1]) as z:
            names = z.namelist()
            if any(".." in n.split("/") or n.startswith("/") or n.endswith((".sh", ".exe", ".so")) for n in names):
                out({"code": "rejected", "error": "The file has unsafe paths or programs", "error_es": "El archivo trae rutas o programas no permitidos"}, 2)
            eid = names[0].split("/")[0]
            target = eid
            if os.path.exists(os.path.join(lib, eid)):
                if "--replace" in args:
                    shutil.rmtree(os.path.join(lib, eid))
                elif "--new" in args:
                    n = 1
                    while os.path.exists(os.path.join(lib, "%s_%d" % (eid, n))):
                        n += 1
                    target = "%s_%d" % (eid, n)
                else:
                    out({"exists": eid}, 3)
            tmp = os.path.join(lib, ".import")
            shutil.rmtree(tmp, ignore_errors=True)
            z.extractall(tmp)
            shutil.move(os.path.join(tmp, eid), os.path.join(lib, target))
            shutil.rmtree(tmp, ignore_errors=True)
        e = read(lib, target)
        e["id"] = target
        write(lib, e)
        out(e)
    if cmd == "delete":
        eid = opt(args, "--entry")
        if opt(args, "--recipe"):
            e = read(lib, eid)
            e["recipes"] = [r for r in e["recipes"] if r["id"] != opt(args, "--recipe")]
            write(lib, e)
        elif opt(args, "--analysis"):
            e = read(lib, eid)
            e["analyses"] = [a for a in e["analyses"] if a["id"] != opt(args, "--analysis")]
            write(lib, e)
        else:
            shutil.rmtree(os.path.join(lib, eid))
        out({})
    out({"error": "unknown library command"}, 2)


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(2)
    if a[0] == "library":
        library(a[1:])
    if a[0] == "slice":
        with open(opt(a, "-o"), "w") as f:
            f.write("G28\nG1 X10\n")
        with open(opt(a, "--summary"), "w") as f:
            json.dump({"print_time": "12m", "print_time_s": 720, "filament_mm": 2500, "filament_g": 7.4, "layers": 50}, f)
        print("progress 100")
        sys.exit(0)
    sys.exit(1)  # profile and the rest: not known, the app uses its own settings


main()
