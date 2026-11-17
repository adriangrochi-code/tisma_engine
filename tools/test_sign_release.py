#!/usr/bin/env python3
"""Tests of the release list and its signing (tools/updates_list.py,
tools/sign_release.py): a key made here, packages made up, signed, checked
with the public key alone; a changed byte, another key and another public key
refused. Skipped (exit 0 with a note) without openssl 3."""

import base64
import json
import os
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
        # macOS (rule 13, 2026-11-10): the .zip, from macOS 13.0; the .dmg is no update.
        for arch in ("arm64", "x64"):
            mac = self.folder / f"tisma-engine-1.0.0-macos-{arch}.zip"
            mac.write_bytes(b"x")
            entry = updates_list.package_entry(mac, "")
            self.assertEqual((entry["system"], entry["min_system"]), (f"macos-{arch}", "13.0"))
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


class CreateKey(unittest.TestCase):
    """create-key: the key made on the stick that is plugged in, never replaced,
    copied to a second stick; folders stand for the sticks."""

    def setUp(self):
        self.work = pathlib.Path(tempfile.mkdtemp())
        self.plugged = set()
        self.lock = self.work / "config" / "clave_bloqueada.txt"
        self.old_lock = os.environ.get("TISMA_KEY_LOCK")
        os.environ["TISMA_KEY_LOCK"] = str(self.lock)

    def tearDown(self):
        if self.old_lock is None:
            os.environ.pop("TISMA_KEY_LOCK", None)
        else:
            os.environ["TISMA_KEY_LOCK"] = self.old_lock
        shutil.rmtree(self.work)

    def stick(self, name):
        path = self.work / name
        path.mkdir()
        return str(path)

    def args(self, **kw):
        base = dict(drive=None, wait=5, no_backup=False, backup_wait=5, no_github=True, repo=None, openssl=OPENSSL)
        base.update(kw)
        return type("Args", (), base)

    def plug_after(self, sticks):
        """drives(): each wait takes what is plugged in, then sees the next
        stick of `sticks` plugged in at its first look."""
        queue = list(sticks)
        calls = [0]

        def drives():
            calls[0] += 1
            if calls[0] % 2 == 0 and queue:
                self.plugged.add(queue.pop(0))
            return set(self.plugged)
        return drives

    def test_made_on_the_stick_plugged_in_and_copied(self):
        self.plugged.add(self.stick("other"))  # already there: not used
        first, second = self.stick("first"), self.stick("second")
        drives = self.plug_after([first, second])
        public = sign_release.create_key(self.args(), drives, poll=0)
        key = pathlib.Path(first) / "tisma" / "clave.pem"
        self.assertTrue(key.is_file())
        self.assertFalse((self.work / "other" / "tisma").exists())
        self.assertEqual(sign_release.public_key_raw(OPENSSL, key).hex(), public)
        self.assertEqual((pathlib.Path(first) / "tisma" / "clave_publica.txt").read_text().strip(), public)
        self.assertEqual(len(public), 64)
        copy = pathlib.Path(second) / "tisma" / "clave.pem"
        self.assertEqual(copy.read_bytes(), key.read_bytes())
        self.assertFalse(list(pathlib.Path(first, "tisma").glob("*.nueva")))

    def test_a_key_there_is_never_replaced(self):
        first = self.stick("first")
        key = make_key(self.work, "old.pem")
        (pathlib.Path(first) / "tisma").mkdir()
        shutil.copyfile(key, pathlib.Path(first) / "tisma" / "clave.pem")
        before = key.read_bytes()
        public = sign_release.create_key(self.args(drive=first, no_backup=True), lambda: set(), poll=0)
        self.assertEqual((pathlib.Path(first) / "tisma" / "clave.pem").read_bytes(), before)
        self.assertEqual(public, sign_release.public_key_raw(OPENSSL, key).hex())

    def test_another_key_on_the_backup_is_refused(self):
        first, second = self.stick("first"), self.stick("second")
        (pathlib.Path(second) / "tisma").mkdir()
        other = make_key(pathlib.Path(second) / "tisma", "clave.pem")
        before = other.read_bytes()
        with self.assertRaisesRegex(sign_release.Failure, "otra clave"):
            sign_release.create_key(self.args(drive=first), self.plug_after([second]), poll=0)
        self.assertEqual(other.read_bytes(), before)

    def test_locked_after_the_first_time(self):
        first, empty = self.stick("first"), self.stick("empty")
        public = sign_release.create_key(self.args(drive=first, no_backup=True), lambda: set(), poll=0)
        self.assertEqual(self.lock.read_text().strip(), public)
        # An empty stick gets no new key.
        with self.assertRaisesRegex(sign_release.Failure, "bloqueada"):
            sign_release.create_key(self.args(drive=empty, no_backup=True), lambda: set(), poll=0)
        self.assertFalse((pathlib.Path(empty) / "tisma").exists())
        # A stick with another key is refused; the first one still works.
        other = self.stick("other")
        (pathlib.Path(other) / "tisma").mkdir()
        make_key(pathlib.Path(other) / "tisma", "clave.pem")
        with self.assertRaisesRegex(sign_release.Failure, "otra clave"):
            sign_release.create_key(self.args(drive=other, no_backup=True), lambda: set(), poll=0)
        self.assertEqual(sign_release.create_key(self.args(drive=first, no_backup=True), lambda: set(), poll=0), public)

    def test_on_its_own_with_github(self):
        """The variable set and the drafts looked for, with no question; a key
        already in GitHub locks a new computer too."""
        calls = []

        def gh(args):
            calls.append(args[1:3])
            if args[1:3] == ["variable", "get"]:
                raise sign_release.Failure("not found")
            out = b"[]" if args[1:3] == ["release", "list"] else b""
            return subprocess.CompletedProcess(args, 0, out, b"")
        first = self.stick("first")
        public = sign_release.create_key(self.args(drive=first, no_backup=True, no_github=False), lambda: set(),
                                         poll=0, gh=gh)
        self.assertIn(["variable", "set"], calls)
        self.assertIn(["release", "list"], calls)
        self.lock.unlink()  # another computer: GitHub still holds the key

        def gh_set(args):
            if args[1:3] == ["variable", "get"]:
                return subprocess.CompletedProcess(args, 0, (public + "\n").encode(), b"")
            return gh(args)
        with self.assertRaisesRegex(sign_release.Failure, "bloqueada"):
            sign_release.create_key(self.args(drive=self.stick("new"), no_backup=True, no_github=False),
                                    lambda: set(), poll=0, gh=gh_set)

    def test_no_stick(self):
        with self.assertRaisesRegex(sign_release.Failure, "ningún pendrive"):
            sign_release.create_key(self.args(wait=0.05), lambda: set(), poll=0.01)


if __name__ == "__main__":
    probe = subprocess.run([OPENSSL or "openssl", "version"], capture_output=True, text=True) if OPENSSL else None
    if not probe or not probe.stdout.startswith("OpenSSL 3"):
        print("test_sign_release: skipped, no openssl 3")
        sys.exit(0)
    unittest.main()
