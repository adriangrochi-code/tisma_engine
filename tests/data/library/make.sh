#!/bin/sh
# Makes the shared library's test files (p2p_library.md, rules 3 bis to 3
# quinquies) with OpenSSL 3: a project key, a library key certified by it, and
# a signed index and block list. Test keys only. Run from this folder.
set -e
openssl genpkey -algorithm ed25519 -out project.pem
openssl genpkey -algorithm ed25519 -out library.pem
raw() { openssl pkey -in "$1" -pubout -outform DER | tail -c 32; }
hexof() { od -An -tx1 | tr -d ' \n'; }
raw project.pem | hexof > project_key.hex
{ printf 'tisma-library-key/1\n'; raw library.pem; } > cert_message.bin
openssl pkeyutl -sign -inkey project.pem -rawin -in cert_message.bin -out cert.bin
printf '{"public_key": "%s", "certificate": "%s"}\n' "$(raw library.pem | hexof)" "$(hexof < cert.bin)" > key.json
openssl pkeyutl -sign -inkey library.pem -rawin -in index.json -out index.json.sig
openssl pkeyutl -sign -inkey library.pem -rawin -in blocklist.json -out blocklist.json.sig
rm -f cert_message.bin cert.bin project.pem library.pem
