#!/usr/bin/env python3
"""The release list the programs read to update themselves (app_updates.md),
unsigned: the release workflow makes it, the owner signs it
(tools/sign_release.py, docs/FIRMAR_VERSIONES.md).

    tools/updates_list.py --version 1.0.0 --tag v1.0.0 --repo owner/name \\
        --notes notes.md [--previous updates.json] [--date 2027-01-12] \\
        -o updates.json packages/*.zip

The list ("format": "tisma-updates", "version": 1) holds the releases, newest
first. This one goes first, with one entry per package: its system and
family (from the file name, see SYSTEMS: ...-windows-x64.zip, ...-linux-x64.zip,
...-windows-x86.zip for the legacy family, ....apk), its download address in the
release, its size and SHA-256, and an empty "signature" that the owner's
signing fills. The releases of --previous (the list published now) follow,
without one of the same version. A version with a pre-release label goes to
the "beta" channel; the others to "stable".

Own design (PROVENANCE.md), over the format the Android app reads
(apps/android, Updates.kt). Python's standard library only.
"""

import argparse
import datetime
import hashlib
import json
import pathlib
import re
import sys

FORMAT = "tisma-updates"
FORMAT_VERSION = 1

# File name ending -> (system, family, min_system), as app_updates.md rule 13
# names them: system windows-x64, windows-x86, linux-x64 or android; family
# current or legacy (the classic 32-bit Windows one, legacy_windows.md).
SYSTEMS = [
    (re.compile(r"-windows-x64\.(zip|exe|msi)$"), ("windows-x64", "current", "10")),
    (re.compile(r"-windows-x86\.(zip|exe|msi)$"), ("windows-x86", "legacy", "6.0")),
    (re.compile(r"-linux-x64\.(zip|AppImage)$"), ("linux-x64", "current", "")),
    (re.compile(r"-macos-arm64\.zip$"), ("macos-arm64", "current", "13.0")),
    (re.compile(r"-macos-x64\.zip$"), ("macos-x64", "current", "13.0")),
    (re.compile(r"\.apk$"), ("android", "current", "28")),
]
EXPECTED_NAMES = ("…-windows-x64.zip/.exe/.msi, …-windows-x86.zip/.exe/.msi, …-linux-x64.zip/.AppImage, "
                  "…-macos-arm64.zip, …-macos-x64.zip, ….apk")


def fail(message):
    print(f"updates_list: {message}", file=sys.stderr)
    sys.exit(1)


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def package_entry(path, url_base):
    for pattern, (system, family, min_system) in SYSTEMS:
        if pattern.search(path.name):
            return {
                "system": system,
                "family": family,
                "min_system": min_system,
                "url": url_base + path.name,
                "size": path.stat().st_size,
                "sha256": sha256_of(path),
                "signature": "",
            }
    return None


def make_list(version, tag, repo, notes, packages, previous=None, date=None, generated=None, notes_en=None):
    """The list as a dict, this release first."""
    url_base = f"https://github.com/{repo}/releases/download/{tag}/"
    entries = []
    for p in packages:
        e = package_entry(pathlib.Path(p), url_base)
        if e is None:
            fail(f"{p}: no system for this file name (expected names: {EXPECTED_NAMES})")
        entries.append(e)
    if not entries:
        fail("no packages")
    release = {
        "version": version,
        "channel": "beta" if "-" in version else "stable",
        "date": date or datetime.date.today().isoformat(),
        # English for the programs in English (rule 13 falls back to it);
        # the CHANGELOG is Spanish, so its text unless one is given.
        "notes": {"es": notes, "en": notes_en if notes_en is not None else notes},
        "packages": entries,
    }
    older = []
    if previous:
        if previous.get("format") != FORMAT:
            fail("the previous list is not a tisma-updates list")
        older = [r for r in previous.get("releases", []) if r.get("version") != version]
    return {
        "format": FORMAT,
        "version": FORMAT_VERSION,
        "generated": generated or datetime.datetime.now(datetime.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "releases": [release] + older,
    }


def dump(listing):
    """The exact bytes that are signed: UTF-8, keys in a fixed order, one line."""
    return (json.dumps(listing, ensure_ascii=False, separators=(", ", ": ")) + "\n").encode("utf-8")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--version", required=True)
    parser.add_argument("--tag", required=True)
    parser.add_argument("--repo", required=True, help="owner/name on GitHub")
    parser.add_argument("--notes", required=True, help="a file with the release notes (Spanish)")
    parser.add_argument("--notes-en", help="a file with the release notes in English (by default, the Spanish ones)")
    parser.add_argument("--previous", help="the list published now, if there is one")
    parser.add_argument("--date")
    parser.add_argument("-o", "--output", required=True)
    parser.add_argument("packages", nargs="+")
    a = parser.parse_args()
    previous = None
    if a.previous and pathlib.Path(a.previous).exists():
        try:
            previous = json.loads(pathlib.Path(a.previous).read_text(encoding="utf-8"))
        except (OSError, ValueError) as e:
            fail(f"{a.previous}: {e}")
    notes = pathlib.Path(a.notes).read_text(encoding="utf-8").strip()
    notes_en = pathlib.Path(a.notes_en).read_text(encoding="utf-8").strip() if a.notes_en else None
    listing = make_list(a.version, a.tag, a.repo, notes, a.packages, previous, a.date, notes_en=notes_en)
    pathlib.Path(a.output).write_bytes(dump(listing))


if __name__ == "__main__":
    main()
