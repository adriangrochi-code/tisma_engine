#!/usr/bin/env python3
"""Signs a release with the project's Ed25519 key and publishes it
(app_updates.md, rules 5 and 13; docs/FIRMAR_VERSIONES.md, in Spanish).

The private key never leaves the owner's USB stick (owner's decision,
2026-11-04): this script runs on the owner's computer, reads the key from the
path given, and the release workflow only leaves the release as a draft with
an unsigned updates.json (tools/updates_list.py).

    python tools/sign_release.py public-key --key E:/tisma/clave.pem
        prints the public key (64 hex digits) the programs are built with

    python tools/sign_release.py sign v1.0.0 --key E:/tisma/clave.pem [--public-key HEX]
        1. downloads the draft release v1.0.0 (gh release download);
        2. checks each of its packages against the size and SHA-256 in
           updates.json, signs it, and writes the signature (base64) in;
        3. signs the exact bytes of updates.json into updates.json.sig (64 bytes);
        4. checks every signature with the public key (the one given, which
           must be the key's own);
        5. uploads updates.json and its .sig to the release, publishes it, and
           puts both in the "updates" release, the list the programs read.
        --dir FOLDER works on a folder with the files instead of downloading;
        --no-upload stops after step 4.

Needs Python 3, openssl 3 (Git Bash on Windows has it) and, to download and
upload, the GitHub CLI (gh) signed in. Python's standard library only; the
signing is openssl's (pkeyutl -sign -rawin: pure Ed25519, RFC 8032, over the
whole file). Own code (PROVENANCE.md).
"""

import argparse
import base64
import hashlib
import json
import pathlib
import subprocess
import sys
import tempfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from updates_list import FORMAT, dump  # noqa: E402

LIST = "updates.json"
LIST_RELEASE = "updates"  # the release the programs read the list from
# The DER of an Ed25519 public key before its 32 bytes (RFC 8410).
ED25519_SPKI_PREFIX = bytes.fromhex("302a300506032b6570032100")


class Failure(Exception):
    pass


def run(args, **kw):
    try:
        return subprocess.run(args, check=True, capture_output=True, **kw)
    except FileNotFoundError:
        raise Failure(f"{args[0]} no está instalado o no está en el PATH")
    except subprocess.CalledProcessError as e:
        message = (e.stderr or b"").decode(errors="replace").strip()
        raise Failure(f"{' '.join(str(a) for a in args[:3])}: {message or 'falló'}")


def public_key_raw(openssl, key):
    """The 32 bytes of the key's public half."""
    der = run([openssl, "pkey", "-in", str(key), "-pubout", "-outform", "DER"]).stdout
    if len(der) != 44 or not der.startswith(ED25519_SPKI_PREFIX):
        raise Failure(f"{key}: no es una clave Ed25519")
    return der[-32:]


def public_pem(raw):
    body = base64.encodebytes(ED25519_SPKI_PREFIX + raw).decode()
    return f"-----BEGIN PUBLIC KEY-----\n{body}-----END PUBLIC KEY-----\n"


def sign(openssl, key, path):
    signature = run([openssl, "pkeyutl", "-sign", "-rawin", "-inkey", str(key), "-in", str(path)]).stdout
    if len(signature) != 64:
        raise Failure(f"{path}: la firma no tiene 64 bytes")
    return signature


def verify(openssl, pem_path, path, signature, work):
    sig_file = pathlib.Path(work) / "check.sig"
    sig_file.write_bytes(signature)
    try:
        run([openssl, "pkeyutl", "-verify", "-rawin", "-pubin", "-inkey", str(pem_path), "-in", str(path),
             "-sigfile", str(sig_file)])
    except Failure:
        raise Failure(f"{path.name}: la firma no coincide con la clave pública")


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for block in iter(lambda: f.read(1 << 20), b""):
            h.update(block)
    return h.hexdigest()


def sign_folder(folder, version, key, openssl="openssl", public_hex=None):
    """Signs the release `version` in `folder` (its packages and updates.json);
    returns the public key, hex."""
    folder = pathlib.Path(folder)
    raw = public_key_raw(openssl, key)
    if public_hex is not None and bytes.fromhex(public_hex) != raw:
        raise Failure("la clave no es la de la clave pública dada: ¿es el pendrive correcto?")
    listing_path = folder / LIST
    try:
        listing = json.loads(listing_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as e:
        raise Failure(f"{listing_path}: {e}")
    if listing.get("format") != FORMAT:
        raise Failure(f"{listing_path}: no es una lista tisma-updates")
    release = next((r for r in listing.get("releases", []) if r.get("version") == version), None)
    if release is None or not release.get("packages"):
        raise Failure(f"{LIST} no tiene paquetes de la versión {version}")
    for p in release["packages"]:
        name = p["url"].rsplit("/", 1)[-1]
        path = folder / name
        if not path.is_file():
            raise Failure(f"{name}: no está en la versión")
        if path.stat().st_size != p["size"] or sha256_of(path) != p["sha256"]:
            raise Failure(f"{name}: su tamaño o su SHA-256 no coincide con {LIST}: ¿se bajó mal?")
        p["signature"] = base64.b64encode(sign(openssl, key, path)).decode()
    listing_path.write_bytes(dump(listing))
    (folder / (LIST + ".sig")).write_bytes(sign(openssl, key, listing_path))
    # Checked with the public key alone, as the programs will.
    with tempfile.TemporaryDirectory() as work:
        pem = pathlib.Path(work) / "public.pem"
        pem.write_text(public_pem(bytes.fromhex(public_hex) if public_hex else raw))
        for p in release["packages"]:
            verify(openssl, pem, folder / p["url"].rsplit("/", 1)[-1], base64.b64decode(p["signature"]), work)
        verify(openssl, pem, listing_path, (folder / (LIST + ".sig")).read_bytes(), work)
    return raw.hex()


def publish(tag, folder, repo):
    """Uploads the signed list to the release, publishes it, and puts the list
    in the release the programs read."""
    where = ["--repo", repo] if repo else []
    files = [str(pathlib.Path(folder) / LIST), str(pathlib.Path(folder) / (LIST + ".sig"))]
    run(["gh", "release", "upload", tag, *files, "--clobber", *where])
    run(["gh", "release", "edit", tag, "--draft=false", *where])
    try:
        run(["gh", "release", "view", LIST_RELEASE, *where])
    except Failure:
        run(["gh", "release", "create", LIST_RELEASE, "--title", "Lista de actualizaciones", "--prerelease",
             "--latest=false", "--notes", "La lista firmada que leen los programas para actualizarse "
             "(docs/FIRMAR_VERSIONES.md). No es una versión.", *where])
    run(["gh", "release", "upload", LIST_RELEASE, *files, "--clobber", *where])


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    pk = sub.add_parser("public-key", help="print the key's public key (64 hex digits)")
    pk.add_argument("--key", required=True)
    pk.add_argument("--openssl", default="openssl")
    sg = sub.add_parser("sign", help="sign a draft release and publish it")
    sg.add_argument("tag", help="the release's tag, as v1.0.0")
    sg.add_argument("--key", required=True, help="the private key (PEM, PKCS#8), on the USB stick")
    sg.add_argument("--public-key", help="the project's public key (64 hex digits), to check the key against it")
    sg.add_argument("--repo", help="owner/name on GitHub (by default, the one of this folder)")
    sg.add_argument("--dir", help="a folder with the files instead of downloading them")
    sg.add_argument("--no-upload", action="store_true")
    sg.add_argument("--openssl", default="openssl")
    a = parser.parse_args()
    try:
        if a.command == "public-key":
            print(public_key_raw(a.openssl, a.key).hex())
            return
        version = a.tag[1:] if a.tag.startswith("v") else a.tag
        with tempfile.TemporaryDirectory() as work:
            folder = a.dir
            if not folder:
                folder = work
                print(f"Bajando {a.tag}…")
                run(["gh", "release", "download", a.tag, "--dir", folder, *(["--repo", a.repo] if a.repo else [])])
            public = sign_folder(folder, version, a.key, a.openssl, a.public_key)
            print(f"Firmado y comprobado con la clave pública {public}.")
            if a.no_upload:
                print(f"Sin subir: los archivos firmados están en {folder}." if a.dir else "Sin subir.")
                return
            publish(a.tag, folder, a.repo)
            print(f"{a.tag} publicada, y la lista de actualizaciones al día.")
    except Failure as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
