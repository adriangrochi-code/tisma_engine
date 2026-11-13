#!/usr/bin/env python3
"""Signs a release with the project's Ed25519 key and publishes it
(app_updates.md, rules 5 and 13; docs/FIRMAR_VERSIONES.md, in Spanish).

The private key never leaves the owner's USB stick (owner's decision,
2026-11-04): this script runs on the owner's computer, reads the key from the
path given, and the release workflow only leaves the release as a draft with
an unsigned updates.json (tools/updates_list.py).

    python tools/sign_release.py create-key [--drive E:] [--no-backup] [--no-github]
        the first time, on its own: waits for a USB stick (or uses the drive
        given), makes the project's key on it in tisma/clave.pem, waits a while
        for a second stick for the copy, puts the public key in GitHub (gh
        variable set TISMA_UPDATES_KEY) and signs the drafts waiting. Then it
        is locked: with the key recorded on this computer or in GitHub, it
        never makes another, and only takes a stick with that same key.
        tools/crear_clave_pendrive.bat runs it with a double click on Windows.

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
import os
import pathlib
import shutil
import subprocess
import sys
import tempfile
import time

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


KEY_FOLDER = "tisma"
KEY_NAME = "clave.pem"
PUBLIC_NAME = "clave_publica.txt"
GITHUB_VARIABLE = "TISMA_UPDATES_KEY"


def find_openssl(given="openssl"):
    """openssl from the PATH, or the one Git for Windows brings, so the key can
    be made with a double click, outside Git Bash."""
    if shutil.which(given):
        return given
    if given == "openssl" and os.name == "nt":
        for root in (os.environ.get("ProgramFiles", r"C:\Program Files"), os.environ.get("ProgramW6432", ""),
                     os.path.join(os.environ.get("LOCALAPPDATA", ""), "Programs")):
            for sub in (r"Git\usr\bin\openssl.exe", r"Git\mingw64\bin\openssl.exe"):
                candidate = os.path.join(root, sub)
                if root and os.path.isfile(candidate):
                    return candidate
    return given


def mounted_drives():
    """The drives and mounted volumes there are now, as paths."""
    if os.name == "nt":
        import ctypes
        mask = ctypes.windll.kernel32.GetLogicalDrives()
        # A: and B: are floppies; C: is the system.
        return {f"{chr(65 + i)}:\\" for i in range(3, 26) if mask >> i & 1}
    roots = [pathlib.Path("/Volumes")] if sys.platform == "darwin" else []
    user = os.environ.get("USER", "")
    roots += [pathlib.Path("/media") / user, pathlib.Path("/run/media") / user, pathlib.Path("/media")]
    found = set()
    for root in roots:
        try:
            for d in root.iterdir():
                if d.is_dir() and os.path.ismount(d):
                    found.add(str(d))
        except OSError:
            pass
    return found


def wait_for_new_drive(before, timeout, drives=mounted_drives, poll=1.0):
    """The first drive that appears that was not in `before`, or None after
    `timeout` seconds."""
    end = time.monotonic() + timeout
    while time.monotonic() < end:
        new = sorted(drives() - set(before))
        if new:
            time.sleep(poll)  # let the system finish mounting it
            return new[0]
        time.sleep(poll)
    return None


def key_on(drive):
    return pathlib.Path(drive) / KEY_FOLDER / KEY_NAME


def make_key_on(drive, openssl):
    """Makes the key on `drive` unless one is there; returns (public hex, made)."""
    key = key_on(drive)
    if key.exists():
        return public_key_raw(openssl, key).hex(), False
    key.parent.mkdir(parents=True, exist_ok=True)
    partial = key.with_name(KEY_NAME + ".nueva")
    try:
        run([openssl, "genpkey", "-algorithm", "ed25519", "-out", str(partial)])
        raw = public_key_raw(openssl, partial)
        os.replace(partial, key)
    finally:
        if partial.exists():
            partial.unlink()
    (key.parent / PUBLIC_NAME).write_text(raw.hex() + "\n")
    return raw.hex(), True


def copy_key_to(source, drive, openssl):
    """Copies the key to a second stick; a different key there is not replaced."""
    raw = public_key_raw(openssl, source)
    target = key_on(drive)
    if target.exists():
        if public_key_raw(openssl, target) != raw:
            raise Failure(f"{target}: ya hay otra clave en este pendrive; no se reemplaza")
        return False
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(source, target)
    if public_key_raw(openssl, target) != raw:
        target.unlink()
        raise Failure(f"{target}: la copia no se lee igual; probá con otro pendrive")
    (target.parent / PUBLIC_NAME).write_text(raw.hex() + "\n")
    return True


def lock_path():
    """Where this computer records that the project's key was made: once there,
    create-key never makes another (owner's decision, 2026-11-10)."""
    if os.environ.get("TISMA_KEY_LOCK"):
        return pathlib.Path(os.environ["TISMA_KEY_LOCK"])
    base = os.environ.get("APPDATA") if os.name == "nt" else None
    base = pathlib.Path(base) if base else pathlib.Path.home() / ".config"
    return base / "tisma" / "clave_bloqueada.txt"


def github_key(gh, repo):
    """The public key already in GitHub, or None (not set, or gh not usable)."""
    try:
        out = gh(["gh", "variable", "get", GITHUB_VARIABLE, *(["--repo", repo] if repo else [])]).stdout
    except Failure:
        return None
    value = (out.decode() if isinstance(out, bytes) else out).strip()
    return value or None


def sign_drafts(key, openssl, public, repo, gh):
    """Signs and publishes every draft release that waits for its signature."""
    where = ["--repo", repo] if repo else []
    try:
        listed = gh(["gh", "release", "list", "--json", "tagName,isDraft", "--limit", "50", *where]).stdout
        drafts = [r["tagName"] for r in json.loads(listed) if r.get("isDraft")]
    except (Failure, ValueError):
        print("No se pudieron ver las versiones en GitHub: firmalas después con «sign».")
        return []
    signed = []
    for tag in drafts:
        version = tag[1:] if tag.startswith("v") else tag
        with tempfile.TemporaryDirectory() as folder:
            try:
                gh(["gh", "release", "download", tag, "--dir", folder, *where])
                if not (pathlib.Path(folder) / LIST).is_file():
                    continue
                sign_folder(folder, version, key, openssl, public)
                publish(tag, folder, repo)
                signed.append(tag)
                print(f"{tag} firmada y publicada.")
            except Failure as e:
                print(f"{tag}: no se firmó ({e}).")
    return signed


def create_key(a, drives=mounted_drives, poll=1.0, gh=run):
    """The first time, on its own: the key on the stick plugged in, its copy on
    a second stick, the public key in GitHub, the drafts signed; then locked."""
    openssl = find_openssl(a.openssl)
    lock = lock_path()
    locked = lock.read_text().strip() if lock.is_file() else None
    in_github = None if a.no_github else github_key(gh, a.repo)
    drive = a.drive
    if not drive:
        print("Enchufá el pendrive donde va a vivir la clave… (Ctrl+C para cancelar)")
        drive = wait_for_new_drive(drives(), a.wait, drives, poll)
        if drive is None:
            raise Failure("no apareció ningún pendrive; probá de nuevo o usá --drive E:")
    print(f"Pendrive: {drive}")
    project = locked or in_github
    if key_on(drive).exists():
        public, made = public_key_raw(openssl, key_on(drive)).hex(), False
        if project and public != project:
            raise Failure("este pendrive tiene otra clave, no la del proyecto: ¿es el pendrive correcto?")
        print(f"Este pendrive ya tiene la clave ({key_on(drive)}): no se tocó.")
    elif project:
        raise Failure(f"la clave del proyecto ya se creó (pública {project}) y quedó bloqueada: no se crea otra. "
                      "Enchufá el pendrive que la tiene, o su copia")
    else:
        public, made = make_key_on(drive, openssl)
        print(f"Clave creada en {key_on(drive)}.")
    print(f"Clave pública (no es secreta): {public}")
    lock.parent.mkdir(parents=True, exist_ok=True)
    lock.write_text(public + "\n")
    if made:
        print(f"Bloqueado: esta computadora no va a crear otra clave ({lock}).")
    if not a.no_backup:
        print(f"Para la copia de seguridad, enchufá un segundo pendrive (espero {a.backup_wait:.0f} s; Ctrl+C saltea)…")
        try:
            second = wait_for_new_drive(drives(), a.backup_wait, drives, poll)
        except KeyboardInterrupt:
            second = None
        if second is None:
            print("Sin copia. Hacela pronto con este mismo comando: si se pierde el pendrive, no hay otra clave.")
        elif copy_key_to(key_on(drive), second, openssl):
            print(f"Copia hecha en {key_on(second)}. Guardá ese pendrive en otro lugar.")
        else:
            print(f"{second} ya tenía la misma clave.")
    if a.no_github:
        return public
    if in_github != public:
        try:
            gh(["gh", "variable", "set", GITHUB_VARIABLE, "--body", public, *(["--repo", a.repo] if a.repo else [])])
            print(f"{GITHUB_VARIABLE} puesta en GitHub: los programas que se armen desde ahora llevan la clave.")
        except Failure as e:
            print(f"No se pudo poner {GITHUB_VARIABLE} en GitHub ({e}): ponela a mano (docs/FIRMAR_VERSIONES.md, "
                  "paso 5).")
            return public
    sign_drafts(key_on(drive), openssl, public, a.repo, gh)
    print("Listo. Nunca copies clave.pem a la computadora ni la mandes a nadie.")
    return public


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    ck = sub.add_parser("create-key", help="make the key on a USB stick when it is plugged in, once")
    ck.add_argument("--drive", help="the stick's drive or folder (E: or /media/me/STICK) instead of waiting")
    ck.add_argument("--wait", type=float, default=600, help="seconds to wait for a stick (600)")
    ck.add_argument("--no-backup", action="store_true", help="do not wait for a second stick for the copy")
    ck.add_argument("--backup-wait", type=float, default=120, help="seconds to wait for the second stick (120)")
    ck.add_argument("--no-github", action="store_true", help="do not set the GitHub variable nor sign drafts")
    ck.add_argument("--repo", help="owner/name on GitHub (by default, the one of this folder)")
    ck.add_argument("--openssl", default="openssl")
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
        if a.command == "create-key":
            create_key(a)
            return
        if a.command == "public-key":
            print(public_key_raw(find_openssl(a.openssl), a.key).hex())
            return
        version = a.tag[1:] if a.tag.startswith("v") else a.tag
        with tempfile.TemporaryDirectory() as work:
            folder = a.dir
            if not folder:
                folder = work
                print(f"Bajando {a.tag}…")
                run(["gh", "release", "download", a.tag, "--dir", folder, *(["--repo", a.repo] if a.repo else [])])
            public = sign_folder(folder, version, a.key, find_openssl(a.openssl), a.public_key)
            print(f"Firmado y comprobado con la clave pública {public}.")
            if a.no_upload:
                print(f"Sin subir: los archivos firmados están en {folder}." if a.dir else "Sin subir.")
                return
            publish(a.tag, folder, a.repo)
            print(f"{a.tag} publicada, y la lista de actualizaciones al día.")
    except Failure as e:
        print(f"error: {e}", file=sys.stderr)
        sys.exit(1)
    except KeyboardInterrupt:
        print("\nCancelado: no se cambió nada más.", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
