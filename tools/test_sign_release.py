#!/usr/bin/env python3
"""Tests of the release list and its signing (tools/updates_list.py,
tools/sign_release.py): a key made here, packages made up, signed, checked
with the public key alone; a changed byte, another key and another public key
refused. Skipped (exit 0 with a note) without openssl 3."""

import base64
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import sign_release  # noqa: E402
import updates_list  # noqa: E402

OPENSSL = shutil.which("openssl")


def make_key(folder, name):
    path = pathlib.Path(folder) / name
    subprocess.run([OPENSSL, "genpkey", "-algorithm", "ed25519", "-out", str(path)], check=True, capture_output=True)
    return path


class SignRelease(unittest.TestCase):
    def setUp(self):
        self.work = tempfile.mkdtemp()
        self.folder = pathlib.Path(self.work) / "release"
        self.folder.mkdir()
        for name, size in (("tisma-engine-1.0.0-windows-x64.zip", 3000), ("tisma-engine-1.0.0-linux-x64.zip", 2000)):
            (self.folder / name).write_bytes(bytes(range(256)) * (size // 256) + b"x" * (size % 256))
        older = {"format": "tisma-updates", "version": 1, "generated": "2026-12-01T00:00:00Z",
                 "releases": [{"version": "0.9.0", "channel": "stable", "date": "2026-12-01", "notes": {"es": "Vieja"},
                               "packages": [{"system": "windows", "family": "current", "min_system": "10",
                                             "url": "https://example.org/old.zip", "size": 1, "sha256": "00",
                                             "signature": "c2lnbmVk"}]}]}
        listing = updates_list.make_list("1.0.0", "v1.0.0", "owner/repo", "Novedades",
                                         sorted(self.folder.glob("*.zip")), older, "2027-01-12", "2027-01-12T10:00:00Z")
        (self.folder / "updates.json").write_bytes(updates_list.dump(listing))
        self.key = make_key(self.work, "clave.pem")

    def tearDown(self):
        shutil.rmtree(self.work)

    def test_list(self):
        listing = json.loads((self.folder / "updates.json").read_text())
        self.assertEqual(listing["format"], "tisma-updates")
        self.assertEqual([r["version"] for r in listing["releases"]], ["1.0.0", "0.9.0"])
        new = listing["releases"][0]
        self.assertEqual(new["channel"], "stable")
        systems = {(p["system"], p["family"]) for p in new["packages"]}
        self.assertEqual(systems, {("windows-x64", "current"), ("linux-x64", "current")})  # rule 13's names
        self.assertEqual(new["notes"], {"es": "Novedades", "en": "Novedades"})
        old_windows = self.folder / "tisma-engine-1.0.0-windows-x86.zip"
        old_windows.write_bytes(b"x")
        self.assertEqual(updates_list.package_entry(old_windows, "")["system"], "windows-x86")
        self.assertEqual(updates_list.package_entry(old_windows, "")["family"], "legacy")
        old_windows.unlink()
        for p in new["packages"]:
            self.assertTrue(p["url"].startswith("https://github.com/owner/repo/releases/download/v1.0.0/"))
            self.assertEqual(p["signature"], "")
        beta = updates_list.make_list("1.1.0-beta.1", "v1.1.0-beta.1", "o/r", "", [self.folder / "tisma-engine-1.0.0-linux-x64.zip"])
        self.assertEqual(beta["releases"][0]["channel"], "beta")

    def test_sign_and_check(self):
        public = sign_release.sign_folder(self.folder, "1.0.0", self.key, OPENSSL)
        self.assertEqual(len(public), 64)
        listing = json.loads((self.folder / "updates.json").read_text())
        for p in listing["releases"][0]["packages"]:
            self.assertEqual(len(base64.b64decode(p["signature"])), 64)
        self.assertEqual(listing["releases"][1]["packages"][0]["signature"], "c2lnbmVk")  # the older one kept
        self.assertEqual(len((self.folder / "updates.json.sig").read_bytes()), 64)
        # The list's signature is over its exact bytes: a changed byte fails.
        with tempfile.TemporaryDirectory() as w:
            pem = pathlib.Path(w) / "p.pem"
            pem.write_text(sign_release.public_pem(bytes.fromhex(public)))
            changed = pathlib.Path(w) / "updates.json"
            changed.write_bytes((self.folder / "updates.json").read_bytes().replace(b"Novedades", b"Novedadez"))
            with self.assertRaises(sign_release.Failure):
                sign_release.verify(OPENSSL, pem, changed, (self.folder / "updates.json.sig").read_bytes(), w)
        # The same key's public key, given: accepted.
        sign_release.sign_folder(self.folder, "1.0.0", self.key, OPENSSL, public)

    def test_refused(self):
        other = make_key(self.work, "otra.pem")
        other_public = sign_release.public_key_raw(OPENSSL, other).hex()
        with self.assertRaisesRegex(sign_release.Failure, "pendrive"):
            sign_release.sign_folder(self.folder, "1.0.0", self.key, OPENSSL, other_public)
        with self.assertRaisesRegex(sign_release.Failure, "no tiene paquetes"):
            sign_release.sign_folder(self.folder, "2.0.0", self.key, OPENSSL)
        package = self.folder / "tisma-engine-1.0.0-linux-x64.zip"
        package.write_bytes(package.read_bytes()[:-1] + b"y")
        with self.assertRaisesRegex(sign_release.Failure, "SHA-256"):
            sign_release.sign_folder(self.folder, "1.0.0", self.key, OPENSSL)


if __name__ == "__main__":
    probe = subprocess.run([OPENSSL or "openssl", "version"], capture_output=True, text=True) if OPENSSL else None
    if not probe or not probe.stdout.startswith("OpenSSL 3"):
        print("test_sign_release: skipped, no openssl 3")
        sys.exit(0)
    unittest.main()
