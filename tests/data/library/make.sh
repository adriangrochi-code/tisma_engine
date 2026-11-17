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
# The seeder's copy of the gear's project (8 octies): one piece of up to 1 MiB,
# its hash list and the piece root, written into the index before it is signed.
sha=$(openssl dgst -sha256 -r seed/engranaje.3mf | cut -c1-64)
size=$(wc -c < seed/engranaje.3mf | tr -d ' ')
rm -rf "seed/$sha"; mkdir -p "seed/$sha"; cp seed/engranaje.3mf "seed/$sha/data"
printf '{"size": %s, "hashes": ["%s"]}\n' "$size" "$sha" > "seed/$sha/pieces.json"
root=$(openssl dgst -sha256 -binary seed/engranaje.3mf | openssl dgst -sha256 -r | cut -c1-64)
sed -e "s/@SHA@/$sha/g" -e "s/@SIZE@/$size/g" -e "s/@ROOT@/$root/g" index.template.json > index.json
openssl pkeyutl -sign -inkey library.pem -rawin -in index.json -out index.json.sig
openssl pkeyutl -sign -inkey library.pem -rawin -in blocklist.json -out blocklist.json.sig
rm -f cert_message.bin cert.bin project.pem library.pem
