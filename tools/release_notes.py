#!/usr/bin/env python3
"""Release notes for a version tag, from CHANGELOG.md (docs/VERSIONING.md).

    tools/release_notes.py v0.6.0-alpha.1   prints the notes of that version
    tools/release_notes.py --check          checks that VERSION has its section

Fails (exit code 1) when the tag does not match the VERSION file, when
CHANGELOG.md has no section for the version, or when the section is empty.
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
VERSION_PATTERN = re.compile(r"^(\d+)\.(\d+)\.(\d+)(-[0-9A-Za-z.-]+)?$")


def fail(message):
    print(f"release_notes: {message}", file=sys.stderr)
    sys.exit(1)


def section(changelog, version):
    """The text under '## [version] ...' up to the next '## ' heading."""
    lines = changelog.splitlines()
    heading = re.compile(r"^## \[" + re.escape(version) + r"\]")
    for i, line in enumerate(lines):
        if heading.match(line):
            body = []
            for rest in lines[i + 1:]:
                if rest.startswith("## "):
                    break
                body.append(rest)
            return "\n".join(body).strip()
    return None


def main(argv):
    version = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
    if not VERSION_PATTERN.match(version):
        fail(f"VERSION must read MAJOR.MINOR.PATCH[-label], not '{version}'")
    if len(argv) != 2:
        fail("usage: release_notes.py <tag> | --check")
    if argv[1] != "--check" and argv[1] != "v" + version:
        fail(f"the tag {argv[1]} does not match VERSION ({version}): tags read v<VERSION>")
    notes = section((ROOT / "CHANGELOG.md").read_text(encoding="utf-8"), version)
    if notes is None:
        fail(f"CHANGELOG.md has no section '## [{version}]'")
    if not notes:
        fail(f"the section of {version} in CHANGELOG.md is empty")
    if argv[1] != "--check":
        print(notes)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
