import hashlib, io, os, subprocess, sys, tarfile
out = sys.argv[1]
os.makedirs(out, exist_ok=True)
def xz(data, *args):
    return subprocess.run(["xz", "-c", *args], input=data, capture_output=True, check=True).stdout
def words(n):
    ws = [b"nozzle", b"bed", b"layer", b"filament", b"PLA", b"PETG", b"speed", b"0.4", b"temperature", b"\n", b" "]
    s, x = bytearray(), 12345
    while len(s) < n:
        x = (x * 1103515245 + 12345) & 0x7FFFFFFF
        s += ws[(x >> 16) % len(ws)] + b" "
    return bytes(s[:n])
def rnd(n):
    s, x = bytearray(), 7
    for _ in range(n):
        x = (x * 6364136223846793005 + 1442695040888963407) & 0xFFFFFFFFFFFFFFFF
        s.append(x >> 56)
    return bytes(s)
cases = {}
hello = b"hello, tisma\n"
w = words(200000)
r = rnd(70000)
cases["hello.xz"] = (hello, xz(hello))
cases["words.xz"] = (w, xz(w, "-9e"))
cases["words_lp2.xz"] = (w, xz(w, "--lzma2=preset=6,lc=0,lp=2,pb=0", "--check=sha256"))
cases["words_blocks.xz"] = (w, xz(w, "--block-size=30000", "--check=crc32"))
cases["random.xz"] = (r, xz(r, "--check=none"))
cases["multi.xz"] = (hello + w[:5000], xz(hello) + b"\0" * 8 + xz(w[:5000]))
def tarball(fmt):
    b = io.BytesIO()
    with tarfile.open(fileobj=b, mode="w", format=fmt) as t:
        def add(name, data=None, kind=tarfile.REGTYPE, link=""):
            i = tarfile.TarInfo(name); i.type = kind; i.mtime = 0; i.linkname = link
            if data is not None: i.size = len(data)
            t.addfile(i, io.BytesIO(data) if data is not None else None)
        add("vendors/acme", kind=tarfile.DIRTYPE)
        add("vendors/acme/models/one.json", b'{"a": 1}\n')
        add("vendors/acme/filaments/" + "long_name_" * 12 + ".json", b"long\n")
        add("vendors/acme/link", kind=tarfile.SYMTYPE, link="../x")
        add("community/printers/Acme/One.json", b"")
    return b.getvalue()
for fmt, name in ((tarfile.PAX_FORMAT, "pax.tar.xz"), (tarfile.GNU_FORMAT, "gnu.tar.xz")):
    t = tarball(fmt)
    cases[name] = (t, xz(t))
for name, (plain, packed) in cases.items():
    open(os.path.join(out, name), "wb").write(packed)
    print(f'{{"{name}", {len(plain)}, "{hashlib.sha256(plain).hexdigest()}"}},  // {len(packed)} B')
